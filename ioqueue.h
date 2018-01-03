#pragma once

#include "nvme.h"
class DevNvme;
#include "queue.h"
class DevNvmeQueue;
#include "namespace.h"
class DevNvmeNamespace;

class DevNvmeIoQueue {
 public:
  void Init(DevNvme *nvme, DevNvmeAdminQueue *aq, uint16_t qid,
            uint16_t sq_size, uint16_t cq_size);
  void InterruptHandler();

  int Flush(DevNvmeNamespace *ns);
  volatile CompletionQueueEntry *ReadBlock(void *dst, DevNvmeNamespace *ns,
                                           uint64_t lba);
  volatile CompletionQueueEntry *WriteBlock(void *src, DevNvmeNamespace *ns,
                                            uint64_t lba);

  volatile CompletionQueueEntry *SubmitCmdFlush(uint32_t nsid);
  volatile CompletionQueueEntry *SubmitCmdRead(
      Memory *prp1, uint32_t nsid, uint64_t lba, uint16_t number_of_blocks,
      bool do_limited_retry, bool force_unit_access, uint8_t protection_info);
  volatile CompletionQueueEntry *SubmitCmdWrite(
      Memory *prp1, uint32_t nsid, uint64_t lba, uint16_t number_of_blocks,
      bool do_limited_retry, bool force_unit_access, uint8_t protection_info);

 private:
  static const int kCmdFlush = 0x00;
  static const int kCmdWrite = 0x01;
  static const int kCmdRead = 0x02;
  DevNvme *_nvme;
  DevNvmeQueue *_queue;
};
