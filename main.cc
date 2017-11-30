#include "nvme.h"

int main(int argc, const char **argv) {
  auto dev = new DevNvme;
  dev->Init();
  //dev->Run();
  return 0;
}
