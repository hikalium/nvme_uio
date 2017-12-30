#pragma once

#include "nvme.h"
class DevNvme;
#include "queue.h"
class DevNvmeQueue;

enum class AdminCommandSet : uint16_t {
  kIdentify = 0x06,
  kAbort = 0x08,
};

class DevNvmeAdminQueue {
 public:
  void Init(DevNvme *nvme);
  int GetSubmissionQueueSize();
  int GetCompletionQueueSize();
  void SubmitCmdIdentify(const Memory *prp1, uint32_t nsid, uint16_t cntid,
                         uint8_t cns);
  void InterruptHandler();

 private:
  DevNvme *_nvme;
  DevNvmeQueue *_queue;
  uint16_t ConstructAdminCommand(int slot, AdminCommandSet op);
  static const int kACQSize = 8;
  static const int kASQSize = 8;
};
