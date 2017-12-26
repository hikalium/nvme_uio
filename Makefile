OBJS= main.o nvme.o adminqueue.o
DEPS= $(filter %.d, $(subst .o,.d, $(OBJS)))

CXXFLAGS += -g -std=c++14 -MMD -MP -Wall -Wpedantic -pthread

.PHONY: load_uio run

default: a.out

-include $(DEPS)

TARGET_PCI_BUS_ID=$(shell lspci | grep Non-Volatile | cut -f 1 -d ' ')
TARGET_PCI_VID_DID=$(shell lspci -n -s $(TARGET_PCI_BUS_ID) | cut -f 3 -d ' ')
TARGET_PCI_VID=$(shell echo $(TARGET_PCI_VID_DID) | cut -f 1 -d ':')
TARGET_PCI_DID=$(shell echo $(TARGET_PCI_VID_DID) | cut -f 2 -d ':')
TARGET_KERNEL_DRIVER=$(shell lspci -k -s $(TARGET_PCI_BUS_ID) | grep "Kernel driver in use:" | cut -f 2 -d ":" | tr -d " ")

check:
	@lspci -vv -n -k -s $(TARGET_PCI_BUS_ID)
	@echo "   bus: $(TARGET_PCI_BUS_ID)"
	@echo "vendor: $(TARGET_PCI_VID)"
	@echo "device: $(TARGET_PCI_DID)"
	@echo "driver: $(TARGET_KERNEL_DRIVER
	sudo modprobe uio_pci_generic
	sudo sh -c "echo '$(TARGET_PCI_VID) $(TARGET_PCI_DID)' > /sys/bus/pci/drivers/uio_pci_generic/new_id"
	sudo sh -c "echo -n 0000:$(TARGET_PCI_BUS_ID) > /sys/bus/pci/drivers/$(TARGET_KERNEL_DRIVER)/unbind"
	sudo sh -c "echo -n 0000:$(TARGET_PCI_BUS_ID) > /sys/bus/pci/drivers/uio_pci_generic/bind"

restore:
	sudo modprobe nvme
	sudo sh -c "echo '$(TARGET_PCI_VID) $(TARGET_PCI_DID)' > /sys/bus/pci/drivers/nvme/new_id"
	sudo sh -c "echo -n 0000:$(TARGET_PCI_BUS_ID) > /sys/bus/pci/drivers/uio_pci_generic/unbind"
	sudo sh -c "echo -n 0000:$(TARGET_PCI_BUS_ID) > /sys/bus/pci/drivers/nvme/bind"

install_uio_module:
	wget https://raw.githubusercontent.com/PFLab-OS/Raph_Kernel_devenv_box/master/uio.sh
	@echo "Please waiti for a while..."
	sh uio.sh
	rm -r build-linux uio.sh

run: a.out
	sudo sh -c "echo 120 > /proc/sys/vm/nr_hugepages"
	sudo ./a.out

a.out: $(OBJS)
	g++ $(CXXFLAGS) $^

clean:
	-rm a.out $(DEPS) $(OBJS)

watch_irq:
	watch -n1 "cat /proc/interrupts"
