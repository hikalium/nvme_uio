#include "nvme.h"
#include <stddef.h> /* offsetof */

void DevNvme::Init() {
  assert(sizeof(CommandSet) == 64);
  assert(sizeof(CompletionQueueEntry) == 16);
  // sizeof(CompletionQueueEntry) may change
  // in the future impl (see section 4.6 in spec)
  assert(sizeof(IdentifyControllerData) == 4096);
  assert(offsetof(IdentifyNamespaceData, LBAF) == 128);

  _pci.Init();
  assert(_pci.HasClassCodes(0x01, 0x08, 0x02));
  {
    uint16_t vid, did;
    _pci.ReadPciReg(DevPci::kVendorIDReg, vid);
    _pci.ReadPciReg(DevPci::kDeviceIDReg, did);
    printf("vid:%08X did:%08X\n", vid, did);
  }
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
    ControllerStatus csts;
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    if (csts.bits.CFS) {
      puts("Controller is in fatal state. Please reboot this machine.");
      exit(EXIT_FAILURE);
    }
    if (csts.bits.RDY) {
      if (csts.bits.SHST == kCSTS_SHST_Normal) {
        // shutdown if needed
        puts("Performing shutdown...");
        ControllerConfiguration cc;
        cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
        cc.bits.SHN = kCC_SHN_AbruptShutdown;
        _ctrl_reg_32_base[kCtrlReg32OffsetCC] = cc.dword;
        //
        usleep(kCtrlTimeout);
      }
      csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
      if (csts.bits.SHST != kCSTS_SHST_Completed) {
        // wait more
        usleep(_ctrl_timeout_worst);
        if (csts.bits.SHST != kCSTS_SHST_Completed) {
          puts("Failed to shutdown controller.");
          exit(EXIT_FAILURE);
        }
      }

      // reset controller
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
        if (csts.bits.RDY) {
          puts("Failed to reset controller.");
          exit(EXIT_FAILURE);
        }
      }
    } else {
      puts("Reset not required");
    }
  }

  _adminQueue = new DevNvmeAdminQueue();
  _adminQueue->Init(this);

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
  Run();
  return;
}

void DevNvme::Run() {
  if (pthread_create(&_irq_handler_thread, NULL, IrqHandler, this) != 0) {
    perror("pthread_create:");
    exit(1);
  }
  AttachAllNamespaces();
}

void *DevNvme::IrqHandler(void *arg) {
  DevNvme *nvme = reinterpret_cast<DevNvme *>(arg);

  while (true) {
    nvme->_pci.WaitInterrupt();
    // admin queue
    nvme->_adminQueue->InterruptHandler();
  }
}

void DevNvme::AttachAllNamespaces() {
  puts("Attach all name");
  char s[128];
  unsigned int nsid;
  {
    Memory prp1(4096);
    _adminQueue->SubmitCmdIdentify(&prp1, 0xffffffff, 0, 0x01);
    IdentifyControllerData *idata = prp1.GetVirtPtr<IdentifyControllerData>();
    printf("VID: %4X\n", idata->VID);
    printf("SSVID: %4X\n", idata->SSVID);
    printf("SN: %.20s\n", idata->SN);
    printf("MN: %.40s\n", idata->MN);
    printf("FR: %.8s\n", idata->FR);
  }
  {
    // TODO: support 1024< entries.
    Memory prp1(4096);
    _adminQueue->SubmitCmdIdentify(&prp1, 0x00000000, 0, 0x02);
    uint32_t *id_list = prp1.GetVirtPtr<uint32_t>();
    int i;
    for (i = 0; i < 1024; i++) {
      if (id_list[i] == 0) break;
      printf("%08X\n", id_list[i]);
    }
    printf("%d namespaces found.\n", i);
  }

  _ioQueue = new DevNvmeIoQueue();
  _ioQueue->Init(this, _adminQueue, 1, 8, 8);

  while (fgets(s, sizeof(s), stdin)) {
    s[strlen(s) - 1] = 0;  // removes new line

    if (strcmp(s, "list") == 0) {
    } else if (sscanf(s, "nsinfo %x", &nsid) == 1) {
      printf("Get info of NSID: %08X\n", nsid);
      Memory prp1(4096);
      _adminQueue->SubmitCmdIdentify(&prp1, nsid, 0, 0x00);
      IdentifyNamespaceData *nsdata = prp1.GetVirtPtr<IdentifyNamespaceData>();

      int LBAFindex = nsdata->FLBAS & 0xF;
      printf("Current LBAFormat: %d\n", LBAFindex);
      int LBASize = 1 << nsdata->LBAF[LBAFindex].LBADS;
      printf("       block size: %d bytes\n", LBASize);
      printf("NSZE: %ld blocks (%ld bytes)\n", nsdata->NSZE,
             nsdata->NSZE * LBASize);
      printf("NCAP: %ld blocks (%ld bytes)\n", nsdata->NCAP,
             nsdata->NCAP * LBASize);
      printf("NUSE: %ld blocks (%ld bytes)\n", nsdata->NUSE,
             nsdata->NUSE * LBASize);
    } else {
      printf("Unknown comand: %s\n", s);
    }
  }
}

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

void DevNvme::PrintControllerConfiguration(ControllerConfiguration cc) {
  printf("CC: EN=%d CSS=%d MPS=%d AMS=%d SHN=%d IOSQES=%d IOCQES=%d  \n",
         cc.bits.EN, cc.bits.CSS, cc.bits.MPS, cc.bits.AMS, cc.bits.SHN,
         cc.bits.IOSQES, cc.bits.IOCQES);
}

void DevNvme::PrintControllerStatus(ControllerStatus csts) {
  printf("CSTS: RDY=%d CFS=%d SHST=%d NSSRO=%d PP=%d  \n", csts.bits.RDY,
         csts.bits.CFS, csts.bits.SHST, csts.bits.NSSRO, csts.bits.PP);
}

void DevNvme::PrintControllerCapabilities(ControllerCapabilities cap) {
  printf("CAP: TO=%d NSSRS=%d  \n", cap.bits.TO, cap.bits.NSSRS);
}

void DevNvme::PrintCompletionQueueEntry(volatile CompletionQueueEntry *cqe) {
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
