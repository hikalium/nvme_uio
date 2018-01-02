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

#include "regdefs.h"

#include "adminqueue.h"
class DevNvmeAdminQueue;

#include "ioqueue.h"
class DevNvmeIoQueue;

class DevNvme {
 public:
  void Init();
  void Run();
  size_t GetCommandSetSize() { return sizeof(CommandSet); }
  size_t GetCompletionQueueEntrySize() { return sizeof(CompletionQueueEntry); }

  uint32_t GetCtrlReg32(int ofs) { return _ctrl_reg_32_base[ofs]; }
  uint64_t GetCtrlReg64(int ofs) { return _ctrl_reg_64_base[ofs]; }
  void SetCtrlReg32(int ofs, uint32_t data) { _ctrl_reg_32_base[ofs] = data; }
  void SetCtrlReg64(int ofs, uint64_t data) { _ctrl_reg_64_base[ofs] = data; }

  static const int kCtrlReg64OffsetCAP = 0x00 / sizeof(uint64_t);
  static const int kCtrlReg32OffsetINTMS = 0x0C / sizeof(uint32_t);
  static const int kCtrlReg32OffsetINTMC = 0x10 / sizeof(uint32_t);
  static const int kCtrlReg32OffsetCC = 0x14 / sizeof(uint32_t);
  static const int kCtrlReg32OffsetCSTS = 0x1C / sizeof(uint32_t);
  static const int kCtrlReg32OffsetAQA = 0x24 / sizeof(uint32_t);
  static const int kCtrlReg64OffsetASQ = 0x28 / sizeof(uint64_t);
  static const int kCtrlReg64OffsetACQ = 0x30 / sizeof(uint64_t);
  static const int kCtrlReg32OffsetDoorbellBase = 0x1000 / sizeof(uint32_t);

  void SetSQyTDBL(int y, uint64_t tail) {
    assert(_ctrl_reg_32_base != nullptr);
    _ctrl_reg_32_base[GetDoorbellIndex(y, 0)] = tail;
  }
  void SetCQyHDBL(int y, uint64_t head) {
    assert(_ctrl_reg_32_base != nullptr);
    _ctrl_reg_32_base[GetDoorbellIndex(y, 1)] = head;
  }
  void SetInterruptMaskForQueue(int y) {
    _ctrl_reg_32_base[kCtrlReg32OffsetINTMS] = 1 << y;
  }
  void ClearInterruptMaskForQueue(int y) {
    _ctrl_reg_32_base[kCtrlReg32OffsetINTMC] = 1 << y;
  }
  void PrintControllerConfiguration(ControllerConfiguration);
  void PrintControllerStatus(ControllerStatus);
  void PrintControllerCapabilities(ControllerCapabilities);
  void PrintCompletionQueueEntry(volatile CompletionQueueEntry *);
  void PrintAdminQueuesSettings();
  void PrintInterruptMask();

 private:
  pthread_t _irq_handler_thread;
  DevPci _pci;
  DevNvmeAdminQueue *_adminQueue;
  DevNvmeIoQueue *_ioQueue = nullptr;

  static const int kCC_AMS_RoundRobin = 0b000;
  static const int kCC_CSS_NVMeCommandSet = 0b000;
  static const int kCC_SHN_NoNotification = 0b00;
  static const int kCC_SHN_AbruptShutdown = 0b10;

  static const int kCSTS_SHST_Normal = 0b00;
  static const int kCSTS_SHST_Completed = 0b10;

  static const __useconds_t kCtrlTimeout = 500 * 1000;

  // from ControllerCapabilities
  __useconds_t _ctrl_timeout_worst = 0;  // set in Init()
  int _doorbell_stride = 0;              // CAP.DSTRD

  volatile uint8_t *_ctrl_reg_8_base = nullptr;
  uint8_t _ctrl_reg_8_size = 0;
  volatile uint32_t *_ctrl_reg_32_base = nullptr;
  volatile uint64_t *_ctrl_reg_64_base = nullptr;

  void MapControlRegisters();

  int GetDoorbellIndex(int y, int isCompletionQueue) {
    return kCtrlReg32OffsetDoorbellBase +
           ((2 * y + isCompletionQueue) * (4 << _doorbell_stride) /
            sizeof(uint32_t));
  }
  void AttachAllNamespaces();
  static void *IrqHandler(void *);
};
