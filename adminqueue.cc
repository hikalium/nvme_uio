#include "adminqueue.h"

void DevNvmeAdminQueue::Init(DevNvme *nvme) {
  _nvme = nvme;
  if (pthread_mutex_init(&mp, NULL) < 0) {
    perror("pthread_mutex_init:");
  }
  _mem_for_asq = new Memory(nvme->GetCommandSetSize() * kASQSize);
  _mem_for_acq = new Memory(nvme->GetCompletionQueueEntrySize() * kACQSize);
  bzero(_mem_for_acq->GetVirtPtr<void>(),
        _mem_for_acq->GetSize());  // clear phase tag.

  nvme->SetCtrlReg64(DevNvme::kCtrlReg64OffsetASQ, _mem_for_asq->GetPhysPtr());
  _asq = _mem_for_asq->GetVirtPtr<volatile CommandSet>();
  printf("ASQ @ %p, physical = %p\n", (void *)_asq,
         (void *)_mem_for_asq->GetPhysPtr());

  nvme->SetCtrlReg64(DevNvme::kCtrlReg64OffsetACQ, _mem_for_acq->GetPhysPtr());
  _acq = _mem_for_acq->GetVirtPtr<volatile CompletionQueueEntry>();
  printf("ACQ @ %p, physical = %p\n", (void *)_acq,
         (void *)_mem_for_acq->GetPhysPtr());

  {
    uint32_t aqa = 0;
    aqa |= (0xfff & kACQSize) << 16;
    aqa |= (0xfff & kASQSize);
    nvme->SetCtrlReg32(DevNvme::kCtrlReg32OffsetAQA, aqa);
  }
  _ptCondList = reinterpret_cast<pthread_cond_t *>(
      malloc(sizeof(pthread_cond_t) * _mem_for_asq->GetSize()));
  if (!_ptCondList) {
    perror("_ptCondList");
    exit(EXIT_FAILURE);
  }
  for (unsigned int i = 0; i < _mem_for_asq->GetSize(); i++) {
    if (pthread_cond_init(&_ptCondList[i], NULL)) {
      perror("pthread_cond_init");
      exit(EXIT_FAILURE);
    }
  }
}
void DevNvmeAdminQueue::SubmitCmdIdentify(const Memory *prp1, uint32_t nsid,
                                          uint16_t cntid, uint8_t cns) {
  pthread_mutex_lock(&mp);
  //
  int32_t slot = _next_submission_slot;
  _next_submission_slot = GetNextSlotOfSubmissionQueue(_next_submission_slot);
  ConstructAdminCommand(slot, AdminCommandSet::kIdentify);
  _asq[slot].PRP1 = prp1->GetPhysPtr();
  _asq[slot].NSID = nsid;
  _asq[slot].CDW10 = cntid << 16 | cns;
  printf("Submitted to [%u], next is [%u]\n", slot, _next_submission_slot);
  _nvme->SetSQyTDBL(0, _next_submission_slot);  // notify controller
  //
  pthread_cond_wait(&_ptCondList[slot], &mp);
  pthread_mutex_unlock(&mp);
}

void DevNvmeAdminQueue::InterruptHandler() {
  while (_acq[_next_completion_slot].SF.P ==
         _expectedCompletionQueueEntryPhase) {
    printf("Completed: CID=%04X\n", _acq[_next_completion_slot].SF.CID);
    _nvme->PrintCompletionQueueEntry(&_acq[_next_completion_slot]);
    pthread_cond_signal(&_ptCondList[_next_completion_slot]);
    //
    _next_completion_slot = (_next_completion_slot + 1) % kACQSize;
    if (_next_completion_slot == 0)
      _expectedCompletionQueueEntryPhase =
          1 - _expectedCompletionQueueEntryPhase;
  }
  _nvme->SetCQyHDBL(0,
                    _next_completion_slot);  // notify controller of interrupt
                                             // handling completion
}
