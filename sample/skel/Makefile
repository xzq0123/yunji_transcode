CUR_PATH                  := $(shell pwd)
SRC_PATH                  := $(CUR_PATH)
HOME_PATH                 := $(abspath $(CUR_PATH)/../../..)

include $(HOME_PATH)/axcl/build/config.mak

OUT_PATH                  := $(AXCL_OUT_PATH)
OBJ_OUT_PATH              := $(AXCL_PRJ_OUT_PATH)/objs
SRC_RELATIVE_PATH         := $(subst $(AXCL_HOME_PATH)/,,$(SRC_PATH))
TARGET_OUT_PATH           := $(OBJ_OUT_PATH)/$(SRC_RELATIVE_PATH)

# output
MOD_NAME                  := axcl_sample_skel
OUTPUT                    := $(TARGET_OUT_PATH)/.obj

# source
SRCS                      :=
SRCCPPS                   := $(wildcard $(SRC_PATH)/*.c)

CINCLUDE                  := -I$(SRC_PATH) \
                             -I$(AXCL_INC_PATH)

OBJS                      := $(SRCS:%.c=$(OUTPUT)/%.o)
CPPOBJS                   := $(SRCCPPS:%.c=$(OUTPUT)/%.o)

DEPS                      := $(OBJS:%.o=%.d)
CPPDEPS                   := $(CPPOBJS:%.o=%.d)

# exec
TARGET                    := $(TARGET_OUT_PATH)/$(MOD_NAME)

# build flags
CFLAGS                    += -DAXCL_BUILD_VERSION=\"$(SDK_VERSION)\"
CFLAGS                    += -DAXCL_SAMPLE_NAME=\"$(TARGET)\"

CFLAGS                    += -Wall -Werror -O2 -ggdb3

# dependency
CLIB                      := -lstdc++ -pthread -lm
CLIB                      += -Wl,-rpath-link=$(AXCL_LIB_PATH)
CLIB                      += -L$(AXCL_LIB_PATH) -laxcl_sys -laxcl_ivps -laxcl_npu -laxcl_skel -laxcl_rt

# install
INSTALL_TARGET            := $(TARGET)
INSTALL_DIR               := $(MOD_TARGET_PATH)/bin/
# INSTALL_DATA              := $(CUR_PATH)/bin/data/*

# link
LINK = $(CC)

include $(AXCL_BUILD_PATH)/rules.mak
