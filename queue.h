#pragma once

#include "nvme.h"
class DevNvme;

class DevNvmeQueue {
 public:
  int GetSubmissionQueueSize() { return _sq_size; }
  int GetCompletionQueueSize() { return _cq_size; }
  const size_t GetSubmissionQueuePhysPtr() { return _mem_for_sq->GetPhysPtr(); }
  const size_t GetCompletionQueuePhysPtr() { return _mem_for_cq->GetPhysPtr(); }
  const Memory *GetSubmissionQueueMemory() { return _mem_for_sq; }
  const Memory *GetCompletionQueueMemory() { return _mem_for_cq; }
  void Init(DevNvme *nvme, int id, int sq_size, int cq_size);
  void InterruptHandler();
  void Lock() { pthread_mutex_lock(&_mp); }
  void SubmitCommand();
  volatile CompletionQueueEntry *WaitUntilCompletion(int slot);
  void Unlock() { pthread_mutex_unlock(&_mp); }
  int GetNextSubmissionSlot() { return _next_submission_slot; }
  volatile CommandSet *GetCommandSet(int slot) {
    assert(0 <= slot && slot < _sq_size);
    return &_sq[slot];
  }

 private:
  pthread_mutex_t _mp;
  DevNvme *_nvme;
  int _id;

  int _sq_size = 0;
  int _cq_size = 0;

  int16_t _next_submission_slot = 0;
  int16_t _next_completion_slot = 0;

  Memory *_mem_for_sq = nullptr;
  Memory *_mem_for_cq = nullptr;

  volatile CommandSet *_sq;
  volatile CompletionQueueEntry *_cq;

  int _expected_completion_phase = 1;
  pthread_cond_t *_pt_cond_list;  // same number of elements of submission queue
};
