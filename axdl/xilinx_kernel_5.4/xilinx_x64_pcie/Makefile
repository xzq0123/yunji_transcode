ARCH=arm64
CROSS_COMPILE :=aarch64-linux-gnu-
CC :=$(CROSS_COMPILE)gcc
SUBDIRS := sample/boot sample/dma smaple/msg tools/pcieTF 
KERNELDIR=/home/gant/peta_prj/linuxPsBase/petalinux/build/tmp/work-shared/zynqmp-generic/kernel-build-artifacts
INC := ./include
#VPATH +=$(PCIE_KERNEL_PATH)/fs/proc/
#ccflags-y  += $(patsubst %,-I%,$(subst :, ,$(VPATH))) -I$(INC)
ccflags-y += -I$(INC)
KCFLAGS	+=-DIS_THIRD_PARTY_PLATFORM

.PHONY : clean

# compile host ko
    obj-m := ax_pcie_host_dev.o
    obj-m += ax_pcie_boot.o
    obj-m += ax_pcie_msg.o
    obj-m += ax_pcie_mmb.o
    obj-m += ax_pcie_net2.o
    ax_pcie_host_dev-objs := host_dev/ax_pcie_dev_host.o common/ax_pcie_opt.o common/ax_pcie_proc.o common/ax_pcie_msg_transfer.o
    ax_pcie_boot-objs     := boot/ax_pcie_boot_usrdev.o
    ax_pcie_msg-objs   := msg/ax_pcie_msg_usrdev.o
    ax_pcie_mmb-objs    := mmb/ax_mmb.o
    ax_pcie_net2-objs    := net/rc-net/ax_pcie_net.o

all:
	@mkdir -p ./out
	@mkdir -p ./out/ko
	@mkdir -p ./out/sample
	@mkdir -p ./out/tools
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNELDIR) M=$(shell pwd) KCFLAGS=$(KCFLAGS) modules
	for dir in $(SUBDIRS);\
	do $(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) -C $$dir;\
	done
	@rm -f *.mod *.mod.o *.o *mod *mod.c

install:
	@mv ./*.ko out/ko
	@mv sample/boot/sample_pcie_boot out/sample
	@mv sample/dma/sample_pcie_dma_host out/sample
	@mv sample/msg/sample_pcie_msg_host out/sample
	@mv tools/pcieTF/pcie_transfer_host out/tools

clean:
	make -C $(KERNELDIR) M=$(shell pwd) clean
	for dir in $(SUBDIRS);\
	do $(MAKE) -C $$dir clean;\
	done
	rm -rf ./out
