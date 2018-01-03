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
  PrintControllerConfiguration();
  PrintControllerStatus();
  {
    ControllerStatus csts;
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    if (csts.bits.CFS) {
      puts("**** Controller is in fatal state. ****");
    }
    if (csts.bits.RDY || csts.bits.CFS) {
      if (csts.bits.SHST == kCSTS_SHST_Normal || csts.bits.CFS) {
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
    if (nvme->_ioQueue) {
      nvme->_ioQueue->InterruptHandler();
    }
  }
}

void DevNvme::AttachAllNamespaces() {
  char s[128];
  {
    Memory prp1(4096);
    _adminQueue->SubmitCmdIdentify(&prp1, 0xffffffff, 0, 0x01);
    IdentifyControllerData *idata = prp1.GetVirtPtr<IdentifyControllerData>();
    printf("  VID: %4X\n", idata->VID);
    printf("SSVID: %4X\n", idata->SSVID);
    printf("   SN: %.20s\n", idata->SN);
    printf("   MN: %.40s\n", idata->MN);
    printf("   FR: %.8s\n", idata->FR);
  }
  _ioQueue = new DevNvmeIoQueue();
  _ioQueue->Init(this, _adminQueue, 1, 8, 8);
  {
    // TODO: support 1024< entries.
    Memory prp1(4096);
    _adminQueue->SubmitCmdIdentify(&prp1, 0x00000000, 0, 0x02);
    uint32_t *id_list = prp1.GetVirtPtr<uint32_t>();
    int i;
    for (i = 0; i < 1024; i++) {
      if (id_list[i] == 0) break;
      _namespaces[i] = new DevNvmeNamespace();
      _namespaces[i]->Init(this, _adminQueue, id_list[i]);
      _adminQueue->AttachNamespace(id_list[i], 1);
    }
    printf("%d namespaces found.\n", i);
  }
  uint32_t nsidx;
  uint64_t lba;
  while (fgets(s, sizeof(s), stdin)) {
    s[strlen(s) - 1] = 0;  // removes new line

    if (strcmp(s, "help") == 0) {
      puts("> list");
      puts("> readblock <Namespace index> <LBA>");
      puts("> writeblock <Namespace index> <LBA>");
    } else if (strcmp(s, "list") == 0) {
      for (int i = 0; i < 1024; i++) {
        if (!_namespaces[i]) break;
        _namespaces[i]->PrintInfo();
      }
    } else if (sscanf(s, "readblock %x %lx", &nsidx, &lba) == 2) {
      assert(nsidx < 1024);
      if (_namespaces[nsidx]) {
        uint8_t *buf =
            static_cast<uint8_t *>(malloc(_namespaces[0]->GetBlockSize()));
        if (!_ioQueue->ReadBlock(buf, _namespaces[0], lba)->isError()) {
          for (uint64_t i = 0; i < _namespaces[0]->GetBlockSize(); i++) {
            printf("%02X%c", buf[i], i % 16 == 15 ? '\n' : ' ');
          }
        }
        free(buf);
      } else {
        puts("namespace index out of bound (check result of list cmd");
      }

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

void DevNvme::PrintControllerConfiguration() {
  ControllerConfiguration cc;
  cc.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCC];
  printf("CC: EN=%d CSS=%d MPS=%d AMS=%d SHN=%d IOSQES=%d IOCQES=%d  \n",
         cc.bits.EN, cc.bits.CSS, cc.bits.MPS, cc.bits.AMS, cc.bits.SHN,
         cc.bits.IOSQES, cc.bits.IOCQES);
}

void DevNvme::PrintControllerStatus() {
  ControllerStatus csts;
  csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
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
