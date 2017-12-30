#include "queue.h"

void DevNvmeQueue::SubmitCommand() {
    _nvme->SetSQyTDBL(0, _next_submission_slot);
    _next_submission_slot = (_next_submission_slot + 1) % _sq_size;
}

void DevNvmeQueue::Init(DevNvme *nvme, int id, int sq_size, int cq_size) {
    _nvme = nvme;
    _id = id;
    _sq_size = sq_size;
    _cq_size = cq_size;

    if (pthread_mutex_init(&_mp, NULL) < 0) {
        perror("pthread_mutex_init:");
    }

    _mem_for_sq = new Memory(nvme->GetCommandSetSize() * _sq_size);
    _mem_for_cq = new Memory(nvme->GetCompletionQueueEntrySize() * _cq_size);

    _pt_cond_list = reinterpret_cast<pthread_cond_t *>(
        malloc(sizeof(pthread_cond_t) * _sq_size));
    if (!_pt_cond_list) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (unsigned int i = 0; i < _sq_size; i++) {
        if (pthread_cond_init(&_pt_cond_list[i], NULL)) {
            perror("pthread_cond_init");
            exit(EXIT_FAILURE);
        }
    }
}

void DevNvmeQueue::InterruptHandler() {
    Lock();
    while (_cq[_next_completion_slot].SF.P == _expected_completion_phase) {
        pthread_cond_signal(&_pt_cond_list[_next_completion_slot]);
        _next_completion_slot = (_next_completion_slot + 1) % _cq_size;
        if (_next_completion_slot == 0)
            _expected_completion_phase = 1 - _expected_completion_phase;
    }
    _nvme->SetCQyHDBL(0, _next_completion_slot);
    Unlock();
}
