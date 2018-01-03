#include "namespace.h"

void DevNvmeNamespace::Init(DevNvme *nvme, DevNvmeAdminQueue *aq,
                            uint32_t nsid) {
  _nvme = nvme;
  _nsid = nsid;

  Memory prp1(4096);
  if (aq->SubmitCmdIdentify(&prp1, nsid, 0, 0x00)->isError()) {
    exit(EXIT_FAILURE);
  }
  IdentifyNamespaceData *nsdata = prp1.GetVirtPtr<IdentifyNamespaceData>();

  int LBAFindex = nsdata->FLBAS & 0xF;
  _block_size = 1 << nsdata->LBAF[LBAFindex].LBADS;
  // FIXME: _block_size may be 1 << 255 maximum so uint64_t is not sufficient.
  _size_in_blocks = nsdata->NSZE;
}
