#include "nvme.h"

void DevNvme::MapControlRegisters() {
  uint32_t addr_bkup;
  _pci.ReadPciReg(DevPci::kBaseAddressReg0, addr_bkup);
  _pci.WritePciReg(DevPci::kBaseAddressReg0, 0xFFFFFFFF);
  uint32_t size;
  _pci.ReadPciReg(DevPci::kBaseAddressReg0, size);
  size = ~size + 1;
  _pci.WritePciReg(DevPci::kBaseAddressReg0, addr_bkup);

  int fd = open("/sys/class/uio/uio0/device/resource0", O_RDWR);
  if (!fd) {
    perror("MapControlRegisters");
    exit(EXIT_FAILURE);
  }
  _ctrl_reg_8_base = reinterpret_cast<volatile uint8_t *>(
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  _ctrl_reg_8_size = size;
  close(fd);
  //
  _ctrl_reg_32_base = reinterpret_cast<volatile uint32_t *>(_ctrl_reg_8_base);
  _ctrl_reg_64_base = reinterpret_cast<volatile uint64_t *>(_ctrl_reg_8_base);
}

void DevNvme::InitAdminQueues() {
  assert(sizeof(CommandSet) == 64);
  assert(sizeof(CompletionQueueEntry) == 16);
  // sizeof(CompletionQueueEntry) may change
  // in the future impl (see section 4.6 in spec)

  _mem_for_asq = new Memory(sizeof(CommandSet) * kASQSize);
  _mem_for_acq = new Memory(sizeof(CompletionQueueEntry) * kACQSize);
  bzero(_mem_for_acq->GetVirtPtr<void>(),
        _mem_for_acq->GetSize());  // clear phase tag.

  _ctrl_reg_64_base[kCtrlReg64OffsetASQ] = _mem_for_asq->GetPhysPtr();
  _ctrl_reg_64_base[kCtrlReg64OffsetACQ] = _mem_for_acq->GetPhysPtr();

  {
    uint32_t aqa = 0;
    aqa |= (0xfff & _mem_for_acq->GetSize()) << 16;
    aqa |= (0xfff & _mem_for_asq->GetSize());
    _ctrl_reg_32_base[kCtrlReg32OffsetAQA] = aqa;
  }
}

void PrintControllerConfiguration(DevNvme::ControllerConfiguration cc) {
  printf("CC: EN=%d CSS=%d MPS=%d AMS=%d IOSQES=%d IOCQES=%d  \n", cc.bits.EN,
         cc.bits.CSS, cc.bits.MPS, cc.bits.AMS, cc.bits.IOSQES, cc.bits.IOCQES);
}

void PrintControllerStatus(DevNvme::ControllerStatus csts) {
  printf("CSTS: RDY=%d CFS=%d SHST=%d NSSRO=%d PP=%d  \n", csts.bits.RDY,
         csts.bits.CFS, csts.bits.SHST, csts.bits.NSSRO, csts.bits.PP);
}

void PrintControllerCapabilities(DevNvme::ControllerCapabilities cap) {
  printf("CAP: TO=%d NSSRS=%d  \n", cap.bits.TO, cap.bits.NSSRS);
}

void DevNvme::Init() {
  _pci.Init();
  uint16_t vid, did;
  _pci.ReadPciReg(DevPci::kVendorIDReg, vid);
  _pci.ReadPciReg(DevPci::kDeviceIDReg, did);
  printf("vid:%08X did:%08X\n", vid, did);

  MapControlRegisters();

  {
    // check CAP
    ControllerCapabilities cap;
    cap.qword = _ctrl_reg_64_base[kCtrlReg64OffsetCAP];
    PrintControllerCapabilities(cap);
    _ctrl_timeout_worst = cap.bits.TO * 500 * 1000;  // in unit 500ms -> us
  }

  {
    // reset if needed
    ControllerStatus csts;
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    PrintControllerStatus(csts);
    if (csts.bits.RDY) {
      puts("Performing reset...");
      ControllerConfiguration cc;
      cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
      cc.bits.EN = 0;
      _ctrl_reg_32_base[kCtrlReg32OffsetCC] = cc.dword;
      //
      usleep(_ctrl_timeout);
      csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
      if (csts.bits.RDY) {
        // try again
        usleep(_ctrl_timeout);
        csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
        if (csts.bits.RDY) {
          puts("Failed to reset controller.");
          exit(EXIT_FAILURE);
        }
      }
    }
  }

  InitAdminQueues();

  {
    // print current status
    ControllerConfiguration cc;
    ControllerStatus csts;
    cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
    PrintControllerConfiguration(cc);
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    PrintControllerStatus(csts);
  }

  {
    // set ControllerConfiguration
    ControllerConfiguration cc;
    cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
    // PrintControllerConfiguration(cc);
    cc.bits.AMS = kCC_AMS_RoundRobin;
    cc.bits.MPS = 0;  // for 4KB. TODO: changed to be decided dynamically.
    cc.bits.CSS = kCC_CSS_NVMeCommandSet;
    _ctrl_reg_32_base[kCtrlReg32OffsetCC] = cc.dword;
  }

  {
    // enable controller
    ControllerConfiguration cc;
    ControllerStatus csts;

    puts("Enabling controller...");
    cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
    cc.bits.EN = 1;
    _ctrl_reg_32_base[kCtrlReg32OffsetCC] = cc.dword;
    //
    usleep(_ctrl_timeout);
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    if (!csts.bits.RDY) {
      // try again
      usleep(_ctrl_timeout);
      csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
      if (!csts.bits.RDY) {
        puts("Failed to enable controller.");
        exit(EXIT_FAILURE);
      }
    }
  }

  return;
}

