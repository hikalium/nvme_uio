OBJS= main.o nvme.o queue.o adminqueue.o ioqueue.o namespace.o
TARGET_KEYWORD=Non-Volatile
TARGET_DEFAULT_DRIVER=nvme
ARGS=help

-include pcie_uio/common.mk

