#include "queue.h"

void DevNvmeQueue::SubmitCommand() {
  _next_submission_slot = (_next_submission_slot + 1) % _sq_size;
  _nvme->SetSQyTDBL(_id, _next_submission_slot);
}

volatile CompletionQueueEntry *DevNvmeQueue::WaitUntilCompletion(int slot) {
  pthread_cond_wait(&_pt_cond_list[slot], &_mp);
  // TODO: Fix index
  return &_cq[slot];
}

void DevNvmeQueue::Init(DevNvme *nvme, int id, int sq_size, int cq_size) {
  _nvme = nvme;
  _id = id;
  _sq_size = sq_size;
  _cq_size = cq_size;

  if (pthread_mutex_init(&_mp, NULL) < 0) {
    perror("pthread_mutex_init:");
    exit(EXIT_FAILURE);
  }

  _mem_for_sq = new Memory(nvme->GetCommandSetSize() * _sq_size);
  _mem_for_cq = new Memory(nvme->GetCompletionQueueEntrySize() * _cq_size);
  bzero(_mem_for_cq->GetVirtPtr<void>(),
        _mem_for_cq->GetSize());  // clear phase tag.

  _sq = _mem_for_sq->GetVirtPtr<volatile CommandSet>();
  _cq = _mem_for_cq->GetVirtPtr<volatile CompletionQueueEntry>();

  _pt_cond_list = reinterpret_cast<pthread_cond_t *>(
      malloc(sizeof(pthread_cond_t) * _sq_size));
  if (!_pt_cond_list) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < _sq_size; i++) {
    if (pthread_cond_init(&_pt_cond_list[i], NULL)) {
      perror("pthread_cond_init");
      exit(EXIT_FAILURE);
    }
  }
  printf("Queue %d initialized. sq(size=%d) cq(size=%d)\n", _id, _sq_size,
         _cq_size);
}

void DevNvmeQueue::InterruptHandler() {
  _nvme->SetInterruptMaskForQueue(_id);
  Lock();
  if (_cq[_next_completion_slot].SF.P == _expected_completion_phase) {
    while (_cq[_next_completion_slot].SF.P == _expected_completion_phase) {
      // TODO: FIX _pt_cond_list index
      int completed_command_id = _cq[_next_completion_slot].SF.CID;
      pthread_cond_signal(&_pt_cond_list[completed_command_id]);
      //
      _next_completion_slot = (_next_completion_slot + 1) % _cq_size;
      if (_next_completion_slot == 0)
        _expected_completion_phase = 1 - _expected_completion_phase;
    }
    _nvme->SetCQyHDBL(_id, _next_completion_slot);
  }
  Unlock();
  _nvme->ClearInterruptMaskForQueue(_id);
}
