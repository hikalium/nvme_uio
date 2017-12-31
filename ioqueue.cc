#include "ioqueue.h"

void DevNvmeIoQueue::Init(DevNvme *nvme, DevNvmeAdminQueue *aq, uint16_t qid,
                          uint16_t sq_size, uint16_t cq_size) {
  _nvme = nvme;

  _queue = new DevNvmeQueue();
  _queue->Init(_nvme, qid, sq_size, cq_size);

  volatile CompletionQueueEntry *cqe;

  cqe = aq->SubmitCmdCreateIoCompletionQueue(_queue->GetCompletionQueueMemory(),
                                             cq_size, qid);
  if (cqe->SF.SCT != 0 || cqe->SF.SC != 0) {
    printf("Command failed. SCT=%d, SC=%d\n\n", cqe->SF.SCT, cqe->SF.SC);
    exit(EXIT_FAILURE);
  } else {
    puts("OK");
  }
  cqe = aq->SubmitCmdCreateIoSubmissionQueue(_queue->GetSubmissionQueueMemory(),
                                             sq_size, qid, qid);
  if (cqe->SF.SCT != 0 || cqe->SF.SC != 0) {
    printf("Command failed. SCT=%d, SC=%d\n\n", cqe->SF.SCT, cqe->SF.SC);
    exit(EXIT_FAILURE);
  } else {
    puts("OK");
  }
  printf("IO Queue %d created\n", qid);
}

void DevNvmeIoQueue::InterruptHandler() { _queue->InterruptHandler(); }
/*
volatile CompletionQueueEntry *DevNvmeAdminQueue::SubmitCmdIdentify(
    const Memory *prp1, uint32_t nsid, uint16_t cntid, uint8_t cns) {
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
  volatile CompletionQueueEntry *cqe = _queue->WaitUntilCompletion(slot);
  _queue->Unlock();
  return cqe;
}


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
*/
