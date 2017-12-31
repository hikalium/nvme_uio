#include "adminqueue.h"

void DevNvmeAdminQueue::Init(DevNvme *nvme) {
  _nvme = nvme;

  _queue = new DevNvmeQueue();
  _queue->Init(_nvme, 0, kASQSize, kACQSize);

  nvme->SetCtrlReg64(DevNvme::kCtrlReg64OffsetASQ,
                     _queue->GetSubmissionQueuePhysPtr());
  nvme->SetCtrlReg64(DevNvme::kCtrlReg64OffsetACQ,
                     _queue->GetCompletionQueuePhysPtr());

  {
    uint32_t aqa = 0;
    aqa |= (0xfff & kACQSize) << 16;
    aqa |= (0xfff & kASQSize);
    nvme->SetCtrlReg32(DevNvme::kCtrlReg32OffsetAQA, aqa);
  }
}

void DevNvmeAdminQueue::SubmitCmdIdentify(const Memory *prp1, uint32_t nsid,
                                          uint16_t cntid, uint8_t cns) {
  _queue->Lock();
  int32_t slot = _queue->GetNextSubmissionSlot();
  //
  volatile CommandSet *cmd = _queue->GetCommandSet(slot);
  ConstructAdminCommand(slot, AdminCommandSet::kIdentify);
  cmd->PRP1 = prp1->GetPhysPtr();
  cmd->NSID = nsid;
  cmd->CDW10 = cntid << 16 | cns;
  //
  _queue->SubmitCommand();
  _queue->WaitUntilCompletion(slot);
  _queue->Unlock();
}

int DevNvmeAdminQueue::GetSubmissionQueueSize() {
  return _queue->GetSubmissionQueueSize();
}

int DevNvmeAdminQueue::GetCompletionQueueSize() {
  return _queue->GetCompletionQueueSize();
}

void DevNvmeAdminQueue::InterruptHandler() { _queue->InterruptHandler(); }

uint16_t DevNvmeAdminQueue::ConstructAdminCommand(int slot,
                                                  AdminCommandSet op) {
  // returns CID
  // Set CDW0 for op.
  volatile CommandSet *cmd = _queue->GetCommandSet(slot);
  cmd->CDW0.OPC = static_cast<int>(op);
  cmd->CDW0.CID = slot;
  switch (op) {
    case AdminCommandSet::kIdentify:
      cmd->CDW0.FUSE = kFUSE_Normal;
      cmd->CDW0.PSDT = kPSDT_UsePRP;
      break;
    case AdminCommandSet::kAbort:
      break;
    default:
      printf("Tried to construct unknown command %d\n", static_cast<int>(op));
      exit(EXIT_FAILURE);
  }
  return slot;
}
