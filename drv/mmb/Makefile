MKFILE_PATH             := $(abspath $(lastword $(MAKEFILE_LIST)))
SRC_PATH                := $(abspath $(dir $(MKFILE_PATH)))
HOME_PATH               := $(abspath $(dir $(MKFILE_PATH))../..)

include $(HOME_PATH)/build/config.mak

AXCL_PCIE_PATH          := $(AXCL_HOME_PATH)/drv
OBJ_OUT_PATH            := $(AXCL_PRJ_OUT_PATH)/objs
SRC_RELATIVE_PATH       := $(subst $(AXCL_HOME_PATH)/,,$(SRC_PATH))
KBUILD_DIR              := $(OBJ_OUT_PATH)/$(SRC_RELATIVE_PATH)
KBUILD_MAKEFILE         := $(KBUILD_DIR)/Makefile

MODULE_NAME             := ax_pcie_mmb

EXTRA_CFLAGS            += -I$(AXCL_PCIE_PATH)/include \
                           -I$(KERNEL_DIR)/fs/proc

ccflags-y               += -DAXCL_MODULE_NAME=\"$(MODULE_NAME)\"

ifeq ($(HOST),x86)
SRCS                    := ax_mmb.c version.c
else
SRCS                    := ax_mmb.c ax_mmb_cache.o version.c
endif

$(MODULE_NAME)-objs     := $(SRCS:%.c=%.o)

obj-m                   := $(MODULE_NAME).o

clean-objs              := $(SRCS:%.c=%.o)
clean-objs              += $(join $(dir $(SRCS)), $(patsubst %.c, .%.o.cmd, $(notdir $(SRCS))))

include $(AXCL_BUILD_PATH)/krules.mak
