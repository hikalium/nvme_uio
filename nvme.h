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

// Figure 109: Identify – Identify Controller Data Structure
struct IdentifyControllerData {
  uint16_t VID;
  uint16_t SSVID;
  char SN[20];
  char MN[40];
  // +64
  char FR[8];
  uint8_t RAB;
  uint8_t IEEE[3];
  uint8_t CMIC;
  uint8_t MDTS;
  uint16_t CNTLID;
  uint32_t VER;
  uint32_t RTD3R;
  uint32_t RTD3E;
  uint32_t OAES;
  uint32_t CTRATT;
  uint8_t Reserved0[12];
  uint8_t FGUID[16];
  // +128
  uint8_t Reserved1[112];
  uint8_t NVMEMI[16];  // Refer to the NVMe Management Interface Spec.
  // +256
  uint16_t OACS;
  uint8_t ACL;
  uint8_t AERL;
  uint8_t FRMW;
  uint8_t LPA;
  uint8_t ELPE;
  uint8_t NPSS;
  uint8_t AVSCC;
  uint8_t APSTA;
  uint16_t WCTEMP;
  uint16_t CCTEMP;
  uint16_t MTFA;
  uint32_t HMPRE;
  uint32_t HMMIN;
  uint8_t TNVMCAP[16];
  uint8_t UNVMCAP[16];
  uint32_t RPMBS;
  uint16_t EDSTT;
  uint8_t DSTO;
  uint8_t FWUG;
  uint16_t KAS;
  uint16_t HCTMA;
  uint16_t MNTMT;
  uint16_t MXTMT;
  uint32_t SANICAP;
  uint8_t Reserved2[180];
  // +512
  uint8_t SQES;
  uint8_t CQES;
  uint16_t MAXCMD;
  uint32_t NN;
  uint16_t ONCS;
  uint16_t FUSES;
  uint8_t FNA;
  uint8_t VWC;
  uint16_t AWUN;
  uint16_t AWUPF;
  uint8_t NVSCC;
  uint8_t Reserved3;
  uint16_t ACWU;
  uint16_t Reserved4;
  uint32_t SGLS;
  uint8_t Reserved5[228];
  char SUBNQN[256];
  // +1024
  uint8_t Reserved6[768];
  uint8_t NVMOF[256];  // Refer to the NVMe over Fabrics spec.
  // +2048
  uint8_t PSD[32][32];
  // +3072
  uint8_t VENDSPEC[1024];
};

struct IdentifyLBAFormatData {
  uint16_t MS;
  uint8_t LBADS;
  unsigned RP : 2;
  unsigned Reserved0 : 6;
};

struct IdentifyNamespaceData {
  uint64_t NSZE;
  uint64_t NCAP;
  uint64_t NUSE;
  uint8_t NSFEAT;
  uint8_t NLBAF;
  uint8_t FLBAS;
  uint8_t MC;
  uint8_t DPC;
  uint8_t DPS;
  uint8_t NMIC;
  uint8_t RESCAP;
  uint8_t FPI;
  uint8_t DLFEAT;
  uint16_t NAWUN;
  uint16_t NAWUPF;
  uint16_t NACWU;
  uint16_t NABSN;
  uint16_t NABO;
  uint16_t NABSPF;
  uint16_t NOIOB;
  uint8_t NVMCAP[16];
  uint8_t Reserved0[40];
  uint8_t NGUID[16];
  uint64_t EUI64;
  struct IdentifyLBAFormatData LBAF[16];
};

class DevNvme;

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
      _pci.WaitInterrupt();
      //
      SetInterruptMaskForQueue(0);
      // puts("Interrupted!");
      pthread_mutex_lock(&_adminQueue->mp);
      { _adminQueue->InterruptHandler(); }
      pthread_mutex_unlock(&_adminQueue->mp);
      ClearInterruptMaskForQueue(0);
    }
  }
  static void *Main(void *);
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
  DevPci _pci;
  DevNvmeAdminQueue *_adminQueue;

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
};
