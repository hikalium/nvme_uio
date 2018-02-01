OBJS= main.o nvme.o queue.o adminqueue.o ioqueue.o namespace.o
TARGET_PCI_BUS_ID=				# Leave blank unless you want to specify device manually.
TARGET_KEYWORD=Non-Volatile		# This will be overrided by TARGET_PCI_BUS_ID if specified.
TARGET_DEFAULT_DRIVER=nvme
ARGS=help

-include pcie_uio/common.mk

