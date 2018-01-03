#pragma once

#include "nvme.h"

class DevNvmeNamespace {
 public:
  void Init(DevNvme *nvme, DevNvmeAdminQueue *aq, uint32_t nsid);
  uint32_t GetId() { return _nsid; }
  uint64_t GetBlockSize() { return _block_size; }
  void PrintInfo() {
    printf("NSID: %08X\n", _nsid);
    printf("  block size: %ld bytes\n", _block_size);
    printf("        size: %ld blocks (%ld bytes)\n", _size_in_blocks,
           _block_size * _size_in_blocks);
  }

 private:
  uint32_t _nsid = 0;
  DevNvme *_nvme = nullptr;

  uint64_t _block_size = 0;
  uint64_t _size_in_blocks = 0;
};
