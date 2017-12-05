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
  struct CommandSet {
    uint32_t CDW0;
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
    uint16_t SF;  // includes Phase Tag
    uint16_t CID;
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
      unsigned Reserved : 28;
    } bits;
  };

  void Init();
  /*
  void Run() {
    pthread_t tid;
    if (pthread_create(&tid, NULL, AttachAll, this) != 0) {
      perror("pthread_create:");
      exit(1);
    }
    while(true) {
      _pci.WaitInterrupt();
      pthread_mutex_lock(&_mp);
      _interrupter.Handle();
      pthread_mutex_unlock(&_mp);
    }
  }
  */
 private:
  DevPci _pci;

  static const int kCtrlReg64OffsetCAP = 0x00 / sizeof(uint64_t);
  static const int kCtrlReg32OffsetCC = 0x14 / sizeof(uint32_t);
  static const int kCtrlReg32OffsetCSTS = 0x1C / sizeof(uint32_t);
  static const int kCtrlReg32OffsetAQA = 0x24 / sizeof(uint32_t);
  static const int kCtrlReg64OffsetASQ = 0x28 / sizeof(uint64_t);
  static const int kCtrlReg64OffsetACQ = 0x30 / sizeof(uint64_t);

  static const int kASQSize = 8;
  static const int kACQSize = 8;

  static const int kCC_AMS_RoundRobin = 0b000;
  static const int kCC_CSS_NVMeCommandSet = 0b000;

  volatile uint8_t *_ctrl_reg_8_base = nullptr;
  uint8_t _ctrl_reg_8_size = 0;
  volatile uint32_t *_ctrl_reg_32_base = nullptr;
  volatile uint64_t *_ctrl_reg_64_base = nullptr;

  __useconds_t _ctrl_timeout_worst = 0;  // set in Init()
  static const __useconds_t _ctrl_timeout = 500 * 1000;

  Memory *_mem_for_asq;
  Memory *_mem_for_acq;

  void MapControlRegisters();
  void InitAdminQueues();
};
