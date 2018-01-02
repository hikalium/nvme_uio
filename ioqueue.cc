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

volatile CompletionQueueEntry *DevNvmeIoQueue::SubmitCmdFlush(uint32_t nsid) {
  _queue->Lock();
  int32_t slot = _queue->GetNextSubmissionSlot();
  //
  volatile CommandSet *cmd = _queue->GetCommandSet(slot);
  bzero((void *)cmd, sizeof(CommandSet));
  cmd->CDW0.OPC = kCmdFlush;
  cmd->CDW0.CID = slot;
  //
  cmd->NSID = nsid;
  //
  _queue->SubmitCommand();
  volatile CompletionQueueEntry *cqe = _queue->WaitUntilCompletion(slot);
  _queue->Unlock();
  if (cqe->SF.SCT != 0 || cqe->SF.SC != 0) {
    printf("Flush: Command failed. SCT=%d, SC=%d\n\n", cqe->SF.SCT, cqe->SF.SC);
  } else {
    puts("Flush:OK");
  }
  return cqe;
}

volatile CompletionQueueEntry *DevNvmeIoQueue::Read(uint32_t nsid, uint64_t lba,
                                                    uint16_t number_of_blocks) {
  Memory prplist(4096), prp(4096);
  volatile CompletionQueueEntry *cqe;
  cqe = SubmitCmdRead(&prp, nsid, lba, number_of_blocks, false, false, 0);
  uint8_t *buf = prp.GetVirtPtr<uint8_t>();
  for (int i = 0; i < 64; i++) {
    printf("%02X%c", buf[i], i % 16 == 15 ? '\n' : ' ');
  }
  return cqe;
}

volatile CompletionQueueEntry *DevNvmeIoQueue::SubmitCmdRead(
    Memory *prp1, uint32_t nsid, uint64_t lba, uint16_t number_of_blocks,
    bool do_limited_retry, bool force_unit_access, uint8_t protection_info) {
  _queue->Lock();
  int32_t slot = _queue->GetNextSubmissionSlot();
  //
  volatile CommandSet *cmd = _queue->GetCommandSet(slot);
  bzero((void *)cmd, sizeof(CommandSet));
  cmd->CDW0.OPC = kCmdRead;
  cmd->CDW0.CID = slot;
  //
  cmd->PRP1 = prp1->GetPhysPtr();
  cmd->NSID = nsid;
  cmd->CDW10 = lba & 0xffffffff;
  cmd->CDW11 = (lba >> 32);
  cmd->CDW12 |= do_limited_retry << 31;
  cmd->CDW12 |= force_unit_access << 30;
  cmd->CDW12 |= protection_info << 26;
  cmd->CDW12 |= number_of_blocks;
  //
  _queue->SubmitCommand();
  volatile CompletionQueueEntry *cqe = _queue->WaitUntilCompletion(slot);
  _queue->Unlock();
  if (cqe->SF.SCT != 0 || cqe->SF.SC != 0) {
    printf("Read: Command failed. SCT=%d, SC=%d\n\n", cqe->SF.SCT, cqe->SF.SC);
  } else {
    puts("Read:OK");
  }
  return cqe;
}
/*

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
