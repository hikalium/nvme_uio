#pragma once

#include "nvme.h"
class DevNvme;
#include "queue.h"
class DevNvmeQueue;

class DevNvmeAdminQueue {
 public:
  void Init(DevNvme *nvme);
  void InterruptHandler();

  volatile CompletionQueueEntry *AttachNamespace(uint16_t nsid, uint16_t qid);

  volatile CompletionQueueEntry *SubmitCmdIdentify(const Memory *prp1,
                                                   uint32_t nsid,
                                                   uint16_t cntid, uint8_t cns);
  volatile CompletionQueueEntry *SubmitCmdCreateIoSubmissionQueue(
      const Memory *prp1, uint16_t size, uint16_t qid, uint16_t cqid);
  volatile CompletionQueueEntry *SubmitCmdCreateIoCompletionQueue(
      const Memory *prp1, uint16_t size, uint16_t qid);
  volatile CompletionQueueEntry *SubmitCmdNamespaceAttachment(
      const Memory *prp1, int sel, uint16_t nsid);

 private:
  static const int kACQSize = 8;
  static const int kASQSize = 8;

  static const int kCmdCreateIoSubmissionQueue = 0x01;
  static const int kCmdCreateIoCompletionQueue = 0x05;
  static const int kCmdIdentify = 0x06;
  static const int kCmdNamespaceAttachment = 0x15;

  DevNvme *_nvme;
  DevNvmeQueue *_queue;
};
