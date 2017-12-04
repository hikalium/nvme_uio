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
  static const int kControlRegister32OffsetCSTS = 0x1C / sizeof(uint32_t);
  DevPci _pci;
};
