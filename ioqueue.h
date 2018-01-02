#pragma once

#include "nvme.h"
class DevNvme;
#include "queue.h"
class DevNvmeQueue;

class DevNvmeIoQueue {
 public:
  void Init(DevNvme *nvme, DevNvmeAdminQueue *aq, uint16_t qid,
            uint16_t sq_size, uint16_t cq_size);
  void InterruptHandler();
  volatile CompletionQueueEntry *SubmitCmdFlush(uint32_t nsid);
  volatile CompletionQueueEntry *Read(uint32_t nsid, uint64_t lba,
                                      uint16_t number_of_blocks);
  volatile CompletionQueueEntry *SubmitCmdRead(
      Memory *prp1, uint32_t nsid, uint64_t lba, uint16_t number_of_blocks,
      bool do_limited_retry, bool force_unit_access, uint8_t protection_info);

 private:
  static const int kCmdFlush = 0x00;
  static const int kCmdWrite = 0x01;
  static const int kCmdRead = 0x02;
  DevNvme *_nvme;
  DevNvmeQueue *_queue;
};
