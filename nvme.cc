#include "nvme.h"
#include <stddef.h> /* offsetof */

void DevNvmeAdminQueue::Init(DevNvme *nvme) {
  _nvme = nvme;
  if (pthread_mutex_init(&mp, NULL) < 0) {
    perror("pthread_mutex_init:");
  }
  _mem_for_asq = new Memory(nvme->GetCommandSetSize() * kASQSize);
  _mem_for_acq = new Memory(nvme->GetCompletionQueueEntrySize() * kACQSize);
  bzero(_mem_for_acq->GetVirtPtr<void>(),
        _mem_for_acq->GetSize());  // clear phase tag.

  nvme->SetCtrlReg64(DevNvme::kCtrlReg64OffsetASQ, _mem_for_asq->GetPhysPtr());
  _asq = _mem_for_asq->GetVirtPtr<volatile CommandSet>();
  printf("ASQ @ %p, physical = %p\n", (void *)_asq,
         (void *)_mem_for_asq->GetPhysPtr());

  nvme->SetCtrlReg64(DevNvme::kCtrlReg64OffsetACQ, _mem_for_acq->GetPhysPtr());
  _acq = _mem_for_acq->GetVirtPtr<volatile CompletionQueueEntry>();
  printf("ACQ @ %p, physical = %p\n", (void *)_acq,
         (void *)_mem_for_acq->GetPhysPtr());

  {
    uint32_t aqa = 0;
    aqa |= (0xfff & kACQSize) << 16;
    aqa |= (0xfff & kASQSize);
    nvme->SetCtrlReg32(DevNvme::kCtrlReg32OffsetAQA, aqa);
  }
  _ptCondList = reinterpret_cast<pthread_cond_t *>(
      malloc(sizeof(pthread_cond_t) * _mem_for_asq->GetSize()));
  if (!_ptCondList) {
    perror("_ptCondList");
    exit(EXIT_FAILURE);
  }
  for (unsigned int i = 0; i < _mem_for_asq->GetSize(); i++) {
    if (pthread_cond_init(&_ptCondList[i], NULL)) {
      perror("pthread_cond_init");
      exit(EXIT_FAILURE);
    }
  }
}
void DevNvmeAdminQueue::SubmitCmdIdentify(const Memory *prp1, uint32_t nsid,
                                          uint16_t cntid, uint8_t cns) {
  pthread_mutex_lock(&mp);
  //
  int32_t slot = _next_slot;
  _next_slot = GetNextSlotOfSubmissionQueue(_next_slot);
  ConstructAdminCommand(slot, AdminCommandSet::kIdentify);
  _asq[slot].PRP1 = prp1->GetPhysPtr();
  _asq[slot].NSID = nsid;
  _asq[slot].CDW10 = cntid << 16 | cns;
  printf("Submitted to [%u], next is [%u]\n", slot, _next_slot);
  _nvme->SetSQyTDBL(0, _next_slot);  // notify controller
  //
  pthread_cond_wait(&_ptCondList[slot], &mp);
  pthread_mutex_unlock(&mp);
}

void DevNvmeAdminQueue::InterruptHandler() {
  // TODO: Fix this for multiple queue
  int i = _nvme->GetCQyHDBL(0);
  while (_acq[i].SF.P == _expectedCompletionQueueEntryPhase) {
    printf("Completed: CID=%04X\n", _acq[i].SF.CID);
    _nvme->PrintCompletionQueueEntry(&_acq[i]);
    pthread_cond_signal(&_ptCondList[i]);
    //
    i = (i + 1) % kACQSize;
    if (i == 0)
      _expectedCompletionQueueEntryPhase =
          1 - _expectedCompletionQueueEntryPhase;
  }
  _nvme->SetCQyHDBL(0,
                    i);  // notify controller of interrupt handling completion
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

void DevNvme::Init() {
  assert(sizeof(CommandSet) == 64);
  assert(sizeof(CompletionQueueEntry) == 16);
  assert(sizeof(IdentifyControllerData) == 4096);
  // sizeof(CompletionQueueEntry) may change
  // in the future impl (see section 4.6 in spec)

  _pci.Init();
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
    ControllerStatus csts;
    csts.dword = _ctrl_reg_32_base[kCtrlReg32OffsetCSTS];
    if (csts.bits.CFS){
      puts("Controller is in fatal state. Please reboot.");
      exit(EXIT_FAILURE);
    }
    if (csts.bits.RDY) {
      if(csts.bits.SHST == kCSTS_SHST_Normal){
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
      cc.bits.SHN = kCC_SHN_AbruptShutdown;
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
  Run();
  return;
}

void *DevNvme::Main(void *arg) {
  DevNvme *nvme = reinterpret_cast<DevNvme *>(arg);

  char s[128];
  while (fgets(s, sizeof(s), stdin)) {
    s[strlen(s) - 1] = 0;  // removes new line
    //
    puts("Main: run cmd");
    if (strcmp(s, "list") == 0) {
      Memory prp1(4096);
      nvme->_adminQueue->SubmitCmdIdentify(&prp1, 0xffffffff, 0, 0x01);
      IdentifyControllerData *idata = prp1.GetVirtPtr<IdentifyControllerData>();
      printf("VID: %4X\n", idata->VID);
      printf("SSVID: %4X\n", idata->SSVID);
      printf("SN: %s\n", idata->SN);
      printf("MN: %s\n", idata->MN);
      printf("FR: %s\n", idata->FR);
    } else {
      printf("Unknown comand: %s\n", s);
    }
    nvme->PrintInterruptMask();
    puts("Main: waiting for next input...");
  }
  puts("Main: return");
  return NULL;
}

