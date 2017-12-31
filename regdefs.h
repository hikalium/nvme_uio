#pragma once

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
