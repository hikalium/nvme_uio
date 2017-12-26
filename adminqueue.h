#pragma once

#include "nvme.h"
class DevNvme;

enum class AdminCommandSet : uint16_t {
  kIdentify = 0x06,
  kAbort = 0x08,
};

class DevNvmeAdminQueue {
 public:
  pthread_mutex_t mp;
  DevNvmeAdminQueue() {}
  void Init(DevNvme *nvme);
  int GetSubmissionQueueSize() { return kASQSize; };
  int GetCompletionQueueSize() { return kACQSize; };
  int GetNextSlotOfSubmissionQueue(int y) { return (y + 1) % kASQSize; };
  void SubmitCmdIdentify(const Memory *prp1, uint32_t nsid, uint16_t cntid,
                         uint8_t cns);
  void InterruptHandler();

 private:
  DevNvme *_nvme;
  uint16_t ConstructAdminCommand(int slot, AdminCommandSet op) {
    // returns CID
    // Set CDW0 for op.
    assert(0 <= slot && slot < kASQSize);
    _asq[slot].CDW0.OPC = static_cast<int>(op);
    _asq[slot].CDW0.CID = slot;
    switch (op) {
      case AdminCommandSet::kIdentify:
        _asq[slot].CDW0.FUSE = kFUSE_Normal;
        _asq[slot].CDW0.PSDT = kPSDT_UsePRP;
        break;
      case AdminCommandSet::kAbort:
        break;
      default:
        printf("Tried to construct unknown command %d\n", static_cast<int>(op));
        exit(EXIT_FAILURE);
    }
    return slot;
  };

  static const int kASQSize = 8;
  static const int kACQSize = 8;
  int16_t _next_submission_slot =
      0;  // being incremented by each command construction
  int16_t _next_completion_slot = 0;

  Memory *_mem_for_asq;
  volatile CommandSet *_asq;
  Memory *_mem_for_acq;
  volatile CompletionQueueEntry *_acq;
  int _expectedCompletionQueueEntryPhase = 1;

  pthread_cond_t *_ptCondList;  // same number of elements of _asq
};
