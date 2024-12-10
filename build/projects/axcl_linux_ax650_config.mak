MKFILE_PATH  := $(abspath $(lastword $(MAKEFILE_LIST)))
HOME_PATH    ?= $(abspath $(dir $(MKFILE_PATH))../../..)

CROSS	     := aarch64-none-linux-gnu-
CC            = $(CROSS)gcc
CPP           = $(CROSS)g++
LD            = $(CROSS)ld
AR            = $(CROSS)ar -rcs
OBJCPOY       = $(CROSS)objcopy
STRIP         = $(CROSS)strip

VERB          = @
RM            = rm -rf
MKDIR         = mkdir -p
ECHO          = echo
MV            = mv
LN            = ln -sf
CP            = cp -f
TAR           = tar
TOUCH         = touch
ARCH          = arm64
STATIC_FLAG  := -fPIC
DYNAMIC_FLAG := -shared -fPIC

KERNEL_VER   := linux-5.15.73
KERNEL_DIR   := $(HOME_PATH)/kernel/linux/$(KERNEL_VER)

BLACK        = "\e[30;1m"
RED          = "\e[31;1m"
GREEN        = "\e[32;1m"
YELLOW       = "\e[33;1m"
BLUE         = "\e[34;1m"
PURPLE       = "\e[35;1m"
CYAN         = "\e[36;1m"
WHITE        = "\e[37;1m"
DONE         = "\033[0m"