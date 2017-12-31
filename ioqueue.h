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
  /*
  volatile CompletionQueueEntry *SubmitCmdIdentify(const Memory *prp1,
                                                   uint32_t nsid,
                                                   uint16_t cntid, uint8_t cns);
*/
 private:
  DevNvme *_nvme;
  DevNvmeQueue *_queue;
};
