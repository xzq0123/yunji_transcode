NAME 				:= axcl
OS                  := linux

MKFILE_PATH         := $(abspath $(lastword $(MAKEFILE_LIST)))
AXCL_HOME_PATH      := $(abspath $(dir $(MKFILE_PATH))..)
AXCL_BUILD_PATH     := $(AXCL_HOME_PATH)/build

ifeq ($(host),x86)
HOST                := x86
include $(AXCL_BUILD_PATH)/projects/$(NAME)_$(OS)_$(HOST)_config.mak
else ifeq ($(host),ax650)
HOST                := ax650
include $(AXCL_BUILD_PATH)/projects/$(NAME)_$(OS)_$(HOST)_config.mak
else ifeq ($(host),)
HOST                := ax650
include $(AXCL_BUILD_PATH)/projects/$(NAME)_$(OS)_$(HOST)_config.mak
else
$(error $(host) is unknown host)
endif

AXCL_PRJ_OUT_PATH   := $(AXCL_BUILD_PATH)/out/$(NAME)_$(OS)_$(HOST)
AXCL_OUT_PATH       := $(AXCL_HOME_PATH)/out/$(NAME)_$(OS)_$(HOST)
AXCL_LIB_PATH       := $(AXCL_HOME_PATH)/out/$(NAME)_$(OS)_$(HOST)/lib
AXCL_BIN_PATH       := $(AXCL_HOME_PATH)/out/$(NAME)_$(OS)_$(HOST)/bin
AXCL_ETC_PATH       := $(AXCL_HOME_PATH)/out/$(NAME)_$(OS)_$(HOST)/etc
AXCL_INC_PATH       := $(AXCL_HOME_PATH)/out/$(NAME)_$(OS)_$(HOST)/include
AXCL_KO_PATH        := $(AXCL_HOME_PATH)/out/$(NAME)_$(OS)_$(HOST)/ko
AXCL_JSON_PATH      := $(AXCL_HOME_PATH)/out/$(NAME)_$(OS)_$(HOST)/json
MOD_TARGET_PATH     := $(AXCL_OUT_PATH)

include $(AXCL_BUILD_PATH)/version.mak

