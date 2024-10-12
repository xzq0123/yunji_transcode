SHELL = /bin/bash
ARCH=arm64
# Specify the cross-compiler prefix
CROSS_COMPILE := aarch64-linux-gnu-

# Set the compiler and other tools
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
LD := $(CROSS_COMPILE)ld

topdir := $(shell cd $(src)/.. && pwd)

DX_PCIE_MODULE:=dx_dma
KBUILD_CFLAGS   +=  -g -Wall

# Include Path
ccflags-y       += -I$(PWD)/include 

# Debug
#EXTRA_CFLAGS += -DCONFIG_DEBUG_FS
#EXTRA_CFLAGS += -DDX_DEBUG_ENABLE
#EXTRA_CFLAGS += -DDMA_PERF_MEASURE
#EXTRA_CFLAGS += -DDUMP_DESC_TABLE
#EXTRA_CFLAGS += -DDMA_DESC_SRAM

# Driver Version
V_FILE:=inlcude/version.h
V_MAJOR=$(shell grep "\#define DRV_MOD_MAJOR" include/version.h | sed "s/\#define DRV_MOD_MAJOR//g" | tr -d '\t' | tr -d ' ')
V_MINOR=$(shell grep "\#define DRV_MOD_MINOR" include/version.h | sed "s/\#define DRV_MOD_MINOR//g" | tr -d '\t' | tr -d ' ')
V_PATCHLEVEL=$(shell grep "\#define DRV_MOD_PATCHLEVEL" include/version.h | sed "s/\#define DRV_MOD_PATCHLEVEL//g" | tr -d '\t' | tr -d ' ')
DRV_VERSION=$(V_MAJOR).$(V_MINOR).$(V_PATCHLEVEL)
#KERNELRELEASE=/home/gant/peta_prj/linuxPsBase/petalinux/build/tmp/work/zynqmp_generic-xilinx-linux/linux-xlnx/5.4+git999-r0/linux-xlnx-5.4+git999/
ifneq ($(KERNELRELEASE),)
	$(DX_PCIE_MODULE)-objs :=	\
		dw-edma-pcie.o \
		dw-edma-core.o \
		dw-edma-v0-core.o \
		dw-edma-v0-debugfs.o \
		dw-edma-thread.o \
		dx_util.o \
		dx_cdev.o \
		dx_sgdma_cdev.o \
		dx_cdev_ctrl.o \
		dx_cdev_events.o \
		virt-dma.o	
	obj-m += $(DX_PCIE_MODULE).o
else
	KERNELDIR=/home/gant/peta_prj/linuxPsBase/petalinux/build/tmp/work/zynqmp_generic-xilinx-linux/linux-xlnx/5.4+git999-r0/linux-xlnx-5.4+git999/
	PWD:=$(shell pwd)
	
default: all

help:
	@echo "******************************************************************************"
	@echo "*              DeepX PCIe Driver [Version:$(DRV_VERSION)]                    *"
	@echo "* usage: make [target]                                                       *"
	@echo "*                                                                            *"
	@echo "* target:                                                                    *" 
	@echo "*   all          Generate the ko file                                        *"
	@echo "*                                                                            *"
	@echo "*   clean        Delete the generated files and build directory              *"
	@echo "*                                                                            *"
	@echo "*   install      Install the driver to setup auto boot.                      *"
	@echo "*                                                                            *"
	@echo "*   uninstall    Uninstall the driver                                        *"
	@echo "*                                                                            *"
	@echo "*   help:        Display this help                                           *"
	@echo "******************************************************************************"

all :
	@echo 4444444444444444
	@echo "*-------------------------------------------------------------*"
	@echo "* Compiling DeepX PCIe driver module [Version:$(DRV_VERSION)] *"
	@echo "*-------------------------------------------------------------*"
	$(MAKE) ARCH=$(ARCH) -C $(KERNELDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	@echo "*-----------------------------------*"
	@echo "* Cleaning DeepX PCIe driver module *"
	@echo "*-----------------------------------*"
	$(MAKE) ARCH=$(ARCH) -C $(KERNELDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) clean
	@/bin/rm -f *.ko modules.order *.mod.c *.o *.o.ur-safe .*.o.cmd

install: all
ifneq ($(shell id -u),0)
	@echo "make install should run as root"
	exit 1
endif
	@echo "*--------------------------------------------------------------*"
	@echo "* Installing DeepX PCIe driver module [Version:$(DRV_VERSION)] *"
	@echo "*--------------------------------------------------------------*"
	$(MAKE) ARCH=$(ARCH) -C $(KERNELDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) INSTALL_MOD_DIR=kernel/drivers/misc modules_install
	depmod -a

uninstall:
ifneq ($(shell id -u),0)
	@echo "make install should run as root"
	exit 1
endif
	@echo "*---------------------------------------*"
	@echo "* Uninstalling DeepX PCIe driver module *"
	@echo "*---------------------------------------*"
	sudo rm -f /lib/modules/$(shell uname -r)/kernel/drivers/misc/$(DX_PCIE_MODULE).ko
	depmod -a

endif

.PHONY: help clean
