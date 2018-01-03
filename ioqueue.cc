#include "ioqueue.h"

void DevNvmeIoQueue::Init(DevNvme *nvme, DevNvmeAdminQueue *aq, uint16_t qid,
                          uint16_t sq_size, uint16_t cq_size) {
  _nvme = nvme;

  _queue = new DevNvmeQueue();
  _queue->Init(_nvme, qid, sq_size, cq_size);

  volatile CompletionQueueEntry *cqe;

  cqe = aq->SubmitCmdCreateIoCompletionQueue(_queue->GetCompletionQueueMemory(),
                                             cq_size, qid);
  if (cqe->PrintIfError("CreateIoCompletionQueue")) {
    exit(EXIT_FAILURE);
  }
  cqe = aq->SubmitCmdCreateIoSubmissionQueue(_queue->GetSubmissionQueueMemory(),
                                             sq_size, qid, qid);
  if (cqe->PrintIfError("CreateIoSubmissionQueue")) {
    exit(EXIT_FAILURE);
  }

  printf("IO Queue %d created\n", qid);
}

void DevNvmeIoQueue::InterruptHandler() { _queue->InterruptHandler(); }

int DevNvmeIoQueue::Flush(DevNvmeNamespace *ns) {
  SubmitCmdFlush(ns->GetId());
  return 0;
}

volatile CompletionQueueEntry *DevNvmeIoQueue::ReadBlock(void *dst,
                                                         DevNvmeNamespace *ns,
                                                         uint64_t lba) {
  assert(ns->GetBlockSize() <= 4096);
  Memory prp(4096);
  volatile CompletionQueueEntry *cqe;
  cqe = SubmitCmdRead(&prp, ns->GetId(), lba, 1, false, false, 0);
  uint8_t *buf = prp.GetVirtPtr<uint8_t>();
  if (!cqe->isError()) {
    memcpy(dst, buf, ns->GetBlockSize());
  }
  return cqe;
}

volatile CompletionQueueEntry *DevNvmeIoQueue::WriteBlock(void *src,
                                                          DevNvmeNamespace *ns,
                                                          uint64_t lba) {
  assert(ns->GetBlockSize() <= 4096);
  Memory prp(4096);
  uint8_t *buf = prp.GetVirtPtr<uint8_t>();
  memcpy(buf, src, ns->GetBlockSize());
  return SubmitCmdWrite(&prp, ns->GetId(), lba, 1, false, false, 0);
}

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

  cqe->PrintIfError("SubmitCmdFlush");
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

  cqe->PrintIfError("SubmitCmdRead");
  return cqe;
}

volatile CompletionQueueEntry *DevNvmeIoQueue::SubmitCmdWrite(
    Memory *prp1, uint32_t nsid, uint64_t lba, uint16_t number_of_blocks,
    bool do_limited_retry, bool force_unit_access, uint8_t protection_info) {
  _queue->Lock();
  int32_t slot = _queue->GetNextSubmissionSlot();
  //
  volatile CommandSet *cmd = _queue->GetCommandSet(slot);
  bzero((void *)cmd, sizeof(CommandSet));
  cmd->CDW0.OPC = kCmdWrite;
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

  cqe->PrintIfError("SubmitCmdWrite");
  return cqe;
}
