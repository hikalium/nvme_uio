#include "nvme.h"
#include "hub.h"
#include "keyboard.h"

void DevNvme::Init() {
  _pci.Init();
  uint16_t vid, did;
  _pci.ReadPciReg(DevPci::kVendorIDReg, vid);
  _pci.ReadPciReg(DevPci::kDeviceIDReg, did);
  printf("vid:%08X did:%08X\n", vid, did);

  /*
    uint8_t interface, sub, base;
    _pci.ReadPciReg(DevPci::kRegInterfaceClassCode, interface);
    _pci.ReadPciReg(DevPci::kRegSubClassCode, sub);
    _pci.ReadPciReg(DevPci::kRegBaseClassCode, base);
    uint16_t command;
    _pci.ReadPciReg(DevPci::kCommandReg, command);
    command |= DevPci::kCommandRegBusMasterEnableFlag;
    _pci.WritePciReg(DevPci::kCommandReg, command);
    */
  // mmap BAR0

  volatile uint8_t *controlRegisters8BaseAddr = nullptr;
  volatile uint32_t *controlRegisters32BaseAddr = nullptr;
  uint8_t controlRegisters8Size = 0;
  {
    uint32_t addr_bkup;
    _pci.ReadPciReg(DevPci::kBaseAddressReg0, addr_bkup);
    _pci.WritePciReg(DevPci::kBaseAddressReg0, 0xFFFFFFFF);
    uint32_t size;
    _pci.ReadPciReg(DevPci::kBaseAddressReg0, size);
    size = ~size + 1;
    _pci.WritePciReg(DevPci::kBaseAddressReg0, addr_bkup);

    int fd = open("/sys/class/uio/uio0/device/resource0", O_RDWR);
    controlRegisters8BaseAddr = reinterpret_cast<volatile uint8_t *>(
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    controlRegisters8Size = size;
    close(fd);
  }
  printf("BAR0: %p %d\n", controlRegisters8BaseAddr, controlRegisters8Size);
  controlRegisters32BaseAddr =
      reinterpret_cast<volatile uint32_t *>(controlRegisters8BaseAddr);

  printf("CSTS: 0x%08X\n",
         controlRegisters32BaseAddr[kControlRegister32OffsetCSTS]);

  return;
}

