#include "nvme.h"

int16_t DevNvme::_next_cid;

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
  _asq = _mem_for_asq->GetVirtPtr<volatile CommandSet>();
  printf("ASQ @ %p, physical = %p\n", (void *)_asq,
         (void *)_mem_for_asq->GetPhysPtr());

  _ctrl_reg_64_base[kCtrlReg64OffsetACQ] = _mem_for_acq->GetPhysPtr();
  _acq = _mem_for_acq->GetVirtPtr<volatile CompletionQueueEntry>();
  printf("ACQ @ %p, physical = %p\n", (void *)_acq,
         (void *)_mem_for_acq->GetPhysPtr());

  {
    uint32_t aqa = 0;
    aqa |= (0xfff & kACQSize) << 16;
    aqa |= (0xfff & kASQSize);
    _ctrl_reg_32_base[kCtrlReg32OffsetAQA] = aqa;
  }
}

void DevNvme::PrintControllerConfiguration(
    DevNvme::ControllerConfiguration cc) {
  printf("CC: EN=%d CSS=%d MPS=%d AMS=%d SHN=%d IOSQES=%d IOCQES=%d  \n",
         cc.bits.EN, cc.bits.CSS, cc.bits.MPS, cc.bits.AMS, cc.bits.SHN,
         cc.bits.IOSQES, cc.bits.IOCQES);
}

void DevNvme::PrintControllerStatus(DevNvme::ControllerStatus csts) {
  printf("CSTS: RDY=%d CFS=%d SHST=%d NSSRO=%d PP=%d  \n", csts.bits.RDY,
         csts.bits.CFS, csts.bits.SHST, csts.bits.NSSRO, csts.bits.PP);
}

void DevNvme::PrintControllerCapabilities(DevNvme::ControllerCapabilities cap) {
  printf("CAP: TO=%d NSSRS=%d  \n", cap.bits.TO, cap.bits.NSSRS);
}

void DevNvme::PrintCompletionQueueEntry(
    volatile DevNvme::CompletionQueueEntry *cqe) {
  printf("CQE: SF.P=0x%X SF.SC=0x%X SF.SCT=0x%X\n", cqe->SF.P, cqe->SF.SC,
         cqe->SF.SCT);
}

void DevNvme::PrintAdminQueuesSettings() {
  printf("ASQ @ physical %p (size = %u)\n",
         (void *)_ctrl_reg_64_base[kCtrlReg64OffsetASQ],
         _ctrl_reg_32_base[kCtrlReg32OffsetAQA] & 0xffff);
  printf("ACQ @ physical %p (size = %u)\n",
         (void *)_ctrl_reg_64_base[kCtrlReg64OffsetACQ],
         _ctrl_reg_32_base[kCtrlReg32OffsetAQA] >> 16);
}

void DevNvme::PrintInterruptMask() {
  printf("INTMC: 0x%X\n", _ctrl_reg_32_base[kCtrlReg32OffsetINTMC]);
  printf("INTMS: 0x%X\n", _ctrl_reg_32_base[kCtrlReg32OffsetINTMS]);
}

void DevNvme::Init() {
  _pci.Init();
  _pci.AllowInterrupt();
  uint16_t vid, did;
  _pci.ReadPciReg(DevPci::kVendorIDReg, vid);
  _pci.ReadPciReg(DevPci::kDeviceIDReg, did);
  printf("vid:%08X did:%08X\n", vid, did);
  {
    // Enable Bus Master for transfering data from controller to host.
    uint8_t interface, sub, base;
    _pci.ReadPciReg(DevPci::kRegInterfaceClassCode, interface);
    _pci.ReadPciReg(DevPci::kRegSubClassCode, sub);
    _pci.ReadPciReg(DevPci::kRegBaseClassCode, base);
    uint16_t command;
    _pci.ReadPciReg(DevPci::kCommandReg, command);
    command |= DevPci::kCommandRegBusMasterEnableFlag;
    _pci.WritePciReg(DevPci::kCommandReg, command);
  }
  {
    uint8_t cap;
    _pci.ReadPciReg(DevPci::kCapPtrReg, cap);
    printf("PCI.CAP=%02X\n", cap);
  }
  {
    uint16_t sts;
    _pci.ReadPciReg(DevPci::kStatusReg, sts);
    printf("PCI.STS=%04X\n", sts);
  }
  {
    uint16_t cmd;
    _pci.ReadPciReg(DevPci::kCommandReg, cmd);
    printf("PCI.CMD=%04X\n", cmd);
  }
  {
    uint8_t cmd;
    _pci.ReadPciReg(0x50, cmd);
    printf("MSI.=%04X\n", cmd);
  }
  MapControlRegisters();

  {
    // check CAP
    ControllerCapabilities cap;
    cap.qword = _ctrl_reg_64_base[kCtrlReg64OffsetCAP];
    PrintControllerCapabilities(cap);
    _ctrl_timeout_worst = cap.bits.TO * 500 * 1000;  // in unit 500ms -> us
    _doorbell_stride = cap.bits.DSTRD;
  }
  {
    ControllerConfiguration cc;
    cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
    PrintControllerConfiguration(cc);
  }
  {
    ControllerStatus csts;
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    PrintControllerStatus(csts);
  }

  {
    // reset if needed
    ControllerStatus csts;
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    if (/*csts.bits.RDY*/ true) {
      puts("Performing reset...");
      ControllerConfiguration cc;
      cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
      cc.bits.EN = 0;
      _ctrl_reg_32_base[kCtrlReg32OffsetCC] = cc.dword;
      //
      usleep(kCtrlTimeout);
      csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
      if (csts.bits.RDY) {
        // try again
        usleep(_ctrl_timeout_worst);
        csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
        if (csts.bits.RDY) {
          puts("Failed to reset controller.");
          exit(EXIT_FAILURE);
        }
      }
    } else {
      puts("Reset not required");
    }
  }

  InitAdminQueues();
  PrintAdminQueuesSettings();

  {
    ControllerConfiguration cc;
    cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
    PrintControllerConfiguration(cc);
  }
  {
    ControllerStatus csts;
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
    cc.bits.SHN = kCC_SHN_NoNotification;
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
    usleep(kCtrlTimeout);
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    if (!csts.bits.RDY) {
      // try again
      usleep(_ctrl_timeout_worst);
      csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
      if (!csts.bits.RDY) {
        puts("Failed to enable controller.");
        exit(EXIT_FAILURE);
      }
    }
  }

  // Controller initialize completed.
  puts("Controller initialized.");
  {
    ControllerConfiguration cc;
    cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
    PrintControllerConfiguration(cc);
  }
  {
    ControllerStatus csts;
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    PrintControllerStatus(csts);
  }
  /*
     // だめだコントローラが死ぬつらい
    ConstructAdminCommand(0, AdminCommandSet::kAbort);
    _asq[0].CDW10 = 0;
    _asq[0].CDW10 |= _next_cid << 16;  // always not found
    _asq[0].CDW10 |= 0;                // admin queue
    SetSQyTDBL(0, 1);                  // notify controller
  */

  Run();
  /*
  Memory *prp1 = new Memory(4096);
  {
    ConstructAdminCommand(0, AdminCommandSet::kIdentify);
    _asq[0].PRP1 = prp1->GetPhysPtr();
    _asq[0].NSID = 0xffffffff;
    uint16_t cntid = 0;
    uint8_t cns = 0x01;
    _asq[0].CDW10 = cntid << 16 | cns;

    SetSQyTDBL(0, 1);  // notify controller
  }

  uint16_t sq0hdbl;
  for (;;) {
    sq0hdbl = GetCQyHDBL(0);
    printf("sq0hdbl: %d\n", sq0hdbl);
    PrintCompletionQueueEntry(&_acq[0]);
    PrintInterruptMask();
    _pci.WaitInterrupt();
    // sleep(1);

}
*/
  return;
}

void DevNvme::IssueAdminIdentify(const Memory *prp1, uint32_t nsid,
                                 uint16_t cntid, uint8_t cns) {
  int64_t next_slot = (GetSQyTDBL(0) + 1) % kASQSize;
  ConstructAdminCommand(next_slot, AdminCommandSet::kIdentify);
  _asq[next_slot].PRP1 = prp1->GetPhysPtr();
  _asq[next_slot].NSID = nsid;
  _asq[0].CDW10 = cntid << 16 | cns;

  SetSQyTDBL(0, next_slot);  // notify controller
}

void *DevNvme::Main(void *arg) {
  DevNvme *nvme = reinterpret_cast<DevNvme *>(arg);
  puts("main thread!!");
  Memory *prp1 = new Memory(4096);
  nvme->IssueAdminIdentify(prp1, 0xffffffff, 0, 0x01);
  for (;;) {
  }
  return NULL;
}

