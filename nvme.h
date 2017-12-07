#pragma once

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#include "generic.h"
#include "mem.h"
#include "pci.h"

class DevNvme {
 public:
  void Init();
  void Run() {
    pthread_t tid;
    if (pthread_create(&tid, NULL, Main, this) != 0) {
      perror("pthread_create:");
      exit(1);
    }
    while (true) {
      puts("Waiting for interrupt...");
      _pci.WaitInterrupt();
	  pthread_mutex_lock(&_mp);
	  {
      	puts("Interrupted!");
		InterruptHandler();
	  }
	  pthread_mutex_unlock(&_mp);
    }
  }
  static void *Main(void *);
  void IssueAdminIdentify(const Memory *prp1, uint32_t nsid, uint16_t cntid,
                          uint8_t cns);

 private:
  pthread_mutex_t _mp;
  DevPci _pci;

  static const int kCtrlReg64OffsetCAP = 0x00 / sizeof(uint64_t);
  static const int kCtrlReg32OffsetINTMS = 0x0C / sizeof(uint32_t);
  static const int kCtrlReg32OffsetINTMC = 0x10 / sizeof(uint32_t);
  static const int kCtrlReg32OffsetCC = 0x14 / sizeof(uint32_t);
  static const int kCtrlReg32OffsetCSTS = 0x1C / sizeof(uint32_t);
  static const int kCtrlReg32OffsetAQA = 0x24 / sizeof(uint32_t);
  static const int kCtrlReg64OffsetASQ = 0x28 / sizeof(uint64_t);
  static const int kCtrlReg64OffsetACQ = 0x30 / sizeof(uint64_t);
  static const int kCtrlReg32OffsetDoorbellBase = 0x1000 / sizeof(uint32_t);

  static const int kASQSize = 8;
  static const int kACQSize = 8;

  static const int kCC_AMS_RoundRobin = 0b000;
  static const int kCC_CSS_NVMeCommandSet = 0b000;
  static const int kCC_SHN_NoNotification = 0b00;

  static const __useconds_t kCtrlTimeout = 500 * 1000;

  enum class AdminCommandSet : uint16_t {
    kIdentify = 0x06,
    kAbort = 0x08,
  };

  static const int kFUSE_Normal = 0b00;
  static const int kPSDT_UsePRP = 0b00;
  struct CommandSet {
    struct {
      unsigned OPC : 8;
      unsigned FUSE : 2;
      unsigned Reserved0 : 4;
      unsigned PSDT : 2;
      unsigned CID : 16;
    } CDW0;
    uint32_t NSID;
    uint64_t Reserved0;
    uint64_t MPTR;
    uint64_t PRP1;
    uint64_t PRP2;
    uint32_t CDW10;
    uint32_t CDW11;
    uint32_t CDW12;
    uint32_t CDW13;
    uint32_t CDW14;
    uint32_t CDW15;
  };
  struct CompletionQueueEntry {
    uint32_t DW0;
    uint32_t DW1;
    uint16_t SQHD;
    uint16_t SQID;
    struct {
      unsigned CID : 16;
      unsigned P : 1;
      unsigned SC : 8;
      unsigned SCT : 3;
      unsigned Reserved0 : 2;
      unsigned M : 1;
      unsigned DNR : 1;
    } SF;
  } __attribute__((packed));

  union ControllerCapabilities {
    uint64_t qword;
    struct {
      unsigned MQES : 16;
      unsigned CQR : 1;
      unsigned AMS : 2;
      unsigned Reserved0 : 5;
      unsigned TO : 8;  // in 500ms unit
      unsigned DSTRD : 4;
      unsigned NSSRS : 1;
      unsigned CSS : 8;
      unsigned BPS : 1;
      unsigned Reserved1 : 2;
      unsigned MPSMIN : 4;
      unsigned MPSMAX : 4;
      unsigned Reserved2 : 8;
    } bits;
  };

  union ControllerConfiguration {
    uint32_t dword;
    struct {
      unsigned EN : 1;
      unsigned Reserved0 : 3;
      unsigned CSS : 3;
      unsigned MPS : 4;
      unsigned AMS : 3;
      unsigned SHN : 2;
      unsigned IOSQES : 4;
      unsigned IOCQES : 4;
      unsigned Reserved1 : 8;
    } bits;
  };

  union ControllerStatus {
    uint32_t dword;
    struct {
      unsigned RDY : 1;
      unsigned CFS : 1;
      unsigned SHST : 2;
      unsigned NSSRO : 1;
      unsigned PP : 1;
      unsigned Reserved : 26;
    } bits;
  };

  // from ControllerCapabilities
  __useconds_t _ctrl_timeout_worst = 0;  // set in Init()
  int _doorbell_stride = 0;              // CAP.DSTRD

  volatile uint8_t *_ctrl_reg_8_base = nullptr;
  uint8_t _ctrl_reg_8_size = 0;
  volatile uint32_t *_ctrl_reg_32_base = nullptr;
  volatile uint64_t *_ctrl_reg_64_base = nullptr;

  Memory *_mem_for_asq;
  volatile CommandSet *_asq;
  Memory *_mem_for_acq;
  volatile CompletionQueueEntry *_acq;
  int _expectedCompletionQueueEntryPhase = 1;

  static int16_t _next_cid;  // being incremented by each command construction

  void MapControlRegisters();
  void InitAdminQueues();

  int GetDoorbellIndex(int y, int isCompletionQueue){
  	return kCtrlReg32OffsetDoorbellBase + ((2 * y + isCompletionQueue) * (4 << _doorbell_stride) / sizeof(uint32_t));
  }

  uint32_t GetSQyTDBL(int y) {
    assert(_ctrl_reg_32_base != nullptr);
    return _ctrl_reg_32_base[GetDoorbellIndex(y, 0)];
  }
  void SetSQyTDBL(int y, uint64_t tail) {
    assert(_ctrl_reg_32_base != nullptr);
    _ctrl_reg_32_base[GetDoorbellIndex(y, 0)] = tail;
  }
  uint32_t GetCQyHDBL(int y) {
    assert(_ctrl_reg_32_base != nullptr);
    return _ctrl_reg_32_base[GetDoorbellIndex(y, 1)];
  }
  void SetCQyHDBL(int y, uint64_t head) {
    assert(_ctrl_reg_32_base != nullptr);
    _ctrl_reg_32_base[GetDoorbellIndex(y, 1)] = head;
  }
  void SetInterruptMaskForQueue(int y){
  	uint32_t mask = _ctrl_reg_32_base[kCtrlReg32OffsetINTMS];
	mask |= 1 << y;
	_ctrl_reg_32_base[kCtrlReg32OffsetINTMS] = 0xffffffff;
  }
  uint16_t ConstructAdminCommand(int slot, AdminCommandSet op) {
    // returns CID
    // Set CDW0 for op.
    uint16_t cid = _next_cid++;
    assert(0 <= slot && slot < kASQSize);
    switch (op) {
      case AdminCommandSet::kIdentify:
        _asq[slot].CDW0.OPC = static_cast<int>(op);
        _asq[slot].CDW0.FUSE = kFUSE_Normal;
        _asq[slot].CDW0.PSDT = kPSDT_UsePRP;
        _asq[slot].CDW0.CID = cid;
        break;
      case AdminCommandSet::kAbort:
        _asq[slot].CDW0.OPC = static_cast<int>(op);
        _asq[slot].CDW0.CID = cid;
        break;
      default:
        printf("Tried to construct unknown command %d\n", static_cast<int>(op));
        exit(EXIT_FAILURE);
    }
    return cid;
  };
  void PrintControllerConfiguration(ControllerConfiguration);
  void PrintControllerStatus(ControllerStatus);
  void PrintControllerCapabilities(ControllerCapabilities);
  void PrintCompletionQueueEntry(volatile CompletionQueueEntry *);
  void PrintAdminQueuesSettings();
  void PrintInterruptMask();
  void InterruptHandler();
};
