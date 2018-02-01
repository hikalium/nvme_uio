#include <sys/resource.h>
#include <sys/time.h>
#include "nvme.h"

int main(int argc, const char **argv) {
  auto nvme = new DevNvme;
  nvme->Init();

  uint32_t nsidx;
  uint64_t lba;
  char *buf = static_cast<char *>(malloc(nvme->_namespaces[0]->GetBlockSize()));
  if (argc < 2) {
    printf("Usage: %s <cmd> <args> ...\n\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  if (strcmp(argv[1], "help") == 0) {
    puts("> list");
    puts("> readblock <Namespace index> <LBA>");
    puts("> writeblock <Namespace index> <LBA>");
  } else if (strcmp(argv[1], "list") == 0) {
    for (int i = 0; i < 1024; i++) {
      if (!nvme->_namespaces[i]) break;
      printf("_namespaces[%d]:\n", i);
      nvme->_namespaces[i]->PrintInfo();
    }
  } else if (sscanf(argv[1], "readblock %x %lx", &nsidx, &lba) == 2) {
    assert(nsidx < 1024);
    if (nvme->_namespaces[nsidx]) {
      if (!nvme->_ioQueue->ReadBlock(buf, nvme->_namespaces[0], lba)
               ->isError()) {
        for (uint64_t i = 0; i < nvme->_namespaces[0]->GetBlockSize(); i++) {
          printf("%02X%c", buf[i], i % 16 == 15 ? '\n' : ' ');
        }
      }
    } else {
      puts("namespace index out of bound (check result of list cmd");
    }
  } else if (sscanf(argv[1], "writeblock %x %lx %s", &nsidx, &lba, buf) == 3) {
    assert(nsidx < 1024);
    if (nvme->_namespaces[nsidx]) {
      if (!nvme->_ioQueue->WriteBlock(buf, nvme->_namespaces[0], lba)
               ->isError()) {
        puts("OK");
      }
    } else {
      puts("namespace index out of bound (check result of list cmd");
    }
  } else if (argc == 5 && strcmp(argv[1], "dd") == 0) {
    struct rusage usage;
    struct timeval ut1, ut2;
    struct timeval st1, st2;
    struct timeval rt1, rt2;

    getrusage(RUSAGE_SELF, &usage);
    ut1 = usage.ru_utime;
    st1 = usage.ru_stime;
    gettimeofday(&rt1, NULL);
    //
    nsidx = atoi(argv[2]);
    lba = atoi(argv[3]);
    FILE *fp = fopen(argv[4], "rb");
    if (!fp) {
      puts("fopen failed.");
      return 1;
    }
    while (fread(buf, 1, nvme->_namespaces[0]->GetBlockSize(), fp)) {
      if (!nvme->_ioQueue->WriteBlock(buf, nvme->_namespaces[0], lba)
               ->isError()) {
        putchar('#');
      } else {
        puts("Failed");
        break;
      }
      lba++;
    }
    puts("\ndone.\n");
    fclose(fp);
    //
    getrusage(RUSAGE_SELF, &usage);
    ut2 = usage.ru_utime;
    st2 = usage.ru_stime;
    gettimeofday(&rt2, NULL);
    //
    timersub(&ut2, &ut1, &ut1);
    timersub(&st2, &st1, &st1);
    timersub(&rt2, &rt1, &rt1);
    printf("real \t%lf\n", rt1.tv_sec + rt1.tv_usec * 1.0E-6);
    printf("user \t%lf\n", ut1.tv_sec + ut1.tv_usec * 1.0E-6);
    printf("sys  \t%lf\n", st1.tv_sec + st1.tv_usec * 1.0E-6);
  } else {
    printf("Unknown comand: %s\n", argv[0]);
  }
  return 0;
}
