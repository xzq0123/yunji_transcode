MKFILE_PATH             := $(abspath $(lastword $(MAKEFILE_LIST)))
SRC_PATH                := $(abspath $(dir $(MKFILE_PATH)))
HOME_PATH               := $(abspath $(dir $(MKFILE_PATH))../..)

include $(HOME_PATH)/build/config.mak

AXCL_PCIE_PATH          := $(AXCL_HOME_PATH)/drv
OBJ_OUT_PATH            := $(AXCL_PRJ_OUT_PATH)/objs
SRC_RELATIVE_PATH       := $(subst $(AXCL_HOME_PATH)/,,$(SRC_PATH))
KBUILD_DIR              := $(OBJ_OUT_PATH)/$(SRC_RELATIVE_PATH)
KBUILD_MAKEFILE         := $(KBUILD_DIR)/Makefile

KBUILD_EXTRA_SYMBOLS    := $(OBJ_OUT_PATH)/drv/host_dev/Module.symvers
export KBUILD_EXTRA_SYMBOLS

MODULE_NAME             := ax_pcie_msg

EXTRA_CFLAGS            += -I$(AXCL_PCIE_PATH)/include \
                           -I$(KERNEL_DIR)/fs/proc

ccflags-y               += -DAXCL_MODULE_NAME=\"$(MODULE_NAME)\"

SRCS                    := ax_pcie_msg_usrdev.c version.c

$(MODULE_NAME)-objs     := $(SRCS:%.c=%.o)

obj-m                   := $(MODULE_NAME).o

clean-objs              := $(SRCS:%.c=%.o)
clean-objs              += $(join $(dir $(SRCS)), $(patsubst %.c, .%.o.cmd, $(notdir $(SRCS))))

include $(AXCL_BUILD_PATH)/krules.mak
