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

volatile CompletionQueueEntry *DevNvmeAdminQueue::SubmitCmdIdentify(
    const Memory *prp1, uint32_t nsid, uint16_t cntid, uint8_t cns) {
  _queue->Lock();
  int32_t slot = _queue->GetNextSubmissionSlot();
  //
  volatile CommandSet *cmd = _queue->GetCommandSet(slot);
  cmd->CDW0.OPC = kCmdIdentify;
  cmd->CDW0.CID = slot;
  cmd->CDW0.FUSE = kFUSE_Normal;
  cmd->CDW0.PSDT = kPSDT_UsePRP;
  //
  cmd->PRP1 = prp1->GetPhysPtr();
  cmd->NSID = nsid;
  cmd->CDW10 = cntid << 16 | cns;
  //
  _queue->SubmitCommand();
  volatile CompletionQueueEntry *cqe = _queue->WaitUntilCompletion(slot);
  _queue->Unlock();
  return cqe;
}

volatile CompletionQueueEntry *
DevNvmeAdminQueue::SubmitCmdCreateIoCompletionQueue(const Memory *prp1,
                                                    uint16_t size,
                                                    uint16_t qid) {
  _queue->Lock();
  int32_t slot = _queue->GetNextSubmissionSlot();
  //
  volatile CommandSet *cmd = _queue->GetCommandSet(slot);
  cmd->CDW0.OPC = kCmdCreateIoCompletionQueue;
  cmd->CDW0.CID = slot;
  cmd->CDW0.FUSE = kFUSE_Normal;
  cmd->CDW0.PSDT = kPSDT_UsePRP;
  //
  cmd->PRP1 = prp1->GetPhysPtr();
  cmd->CDW10 = size << 16 | qid;
  cmd->CDW11 = 3;
  //
  _queue->SubmitCommand();
  volatile CompletionQueueEntry *cqe = _queue->WaitUntilCompletion(slot);
  _queue->Unlock();
  return cqe;
}

volatile CompletionQueueEntry *
DevNvmeAdminQueue::SubmitCmdCreateIoSubmissionQueue(const Memory *prp1,
                                                    uint16_t size, uint16_t qid,
                                                    uint16_t cqid) {
  _queue->Lock();
  int32_t slot = _queue->GetNextSubmissionSlot();
  //
  volatile CommandSet *cmd = _queue->GetCommandSet(slot);
  cmd->CDW0.OPC = kCmdCreateIoSubmissionQueue;
  cmd->CDW0.CID = slot;
  cmd->CDW0.FUSE = kFUSE_Normal;
  cmd->CDW0.PSDT = kPSDT_UsePRP;
  //
  cmd->PRP1 = prp1->GetPhysPtr();
  cmd->CDW10 = size << 16 | qid;
  cmd->CDW11 = cqid << 16 | 1;
  //
  _queue->SubmitCommand();
  volatile CompletionQueueEntry *cqe = _queue->WaitUntilCompletion(slot);
  _queue->Unlock();
  return cqe;
}

void DevNvmeAdminQueue::InterruptHandler() { _queue->InterruptHandler(); }
