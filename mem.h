#pragma once
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "generic.h"

using phys_addr = size_t;
using virt_addr = size_t;

class Memory {
 public:
  Memory() = delete;
  Memory(size_t size) {
    _req_size = size;

    if (_req_size > 0) {
      assert(_req_size <= 2 * 1024 * 1024);
      _act_size = 2 * 1024 * 1024;
      _virt =
          mmap(NULL, _act_size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_HUGETLB | MAP_ANONYMOUS | MAP_POPULATE, 0, 0);
      if (_virt == MAP_FAILED) {
        perror("page alloc:");
        panic("");
      }
      _phys = vtop(reinterpret_cast<size_t>(_virt));
    } else {
      _virt = 0;
      _phys = 0;
    }
  }
  ~Memory() {
    if (_act_size > 0) {
      munmap(_virt, 2 * 1024 * 1024);
    }
  }
  template <class T>
  T *GetVirtPtr() {
    return reinterpret_cast<T *>(_virt);
  }
  phys_addr GetPhysPtr() const { return _phys; }
  size_t GetSize() { return _req_size; }

 private:
  static size_t vtop(size_t vaddr) {
    FILE *pagemap;
    size_t paddr = 0;
    ssize_t offset = (vaddr / sysconf(_SC_PAGESIZE)) * sizeof(uint64_t);
    uint64_t e;

    // https://www.kernel.org/doc/Documentation/vm/pagemap.txt
    if ((pagemap = fopen("/proc/self/pagemap", "r"))) {
      if (lseek(fileno(pagemap), offset, SEEK_SET) == offset) {
        if (fread(&e, sizeof(uint64_t), 1, pagemap)) {
          if (e & (1ULL << 63)) {            // page present ?
            paddr = e & ((1ULL << 55) - 1);  // pfn mask
            paddr = paddr * sysconf(_SC_PAGESIZE);
            // add offset within page
            paddr = paddr | (vaddr & (sysconf(_SC_PAGESIZE) - 1));
          }
        }
      }
      fclose(pagemap);
    }

    return paddr;
  }

  void *_virt;
  phys_addr _phys;
  size_t _req_size = 0;
  size_t _act_size = 0;
};
