COMPILER_HOSTNAME_STR := \"$(shell hostname)\"
COMPILER_USERNAME_STR := \"$(shell whoami | sed 's/\\/\\\\/g')\"
#  -----------------------------------------------------------------------------
#  Desc: TARGET
#  -----------------------------------------------------------------------------
MKFILE_PATH     := $(abspath $(lastword $(MAKEFILE_LIST)))
HOME_PATH       ?= $(abspath $(dir $(MKFILE_PATH))../../..)
PROJECT         ?= AX650_emmc
CHIP_NAME       ?= AX650
KBUILD_OUTDIR   ?= $(HOME_PATH)/build/out/$(PROJECT)/objs/kernel/linux/$(KERNEL_VER)
DEBUG_OUT_PATH  := $(MOD_TARGET_PATH)/debug_ko

EXT_FLAG        ?= O=$(KBUILD_OUTDIR)  HOME_PATH=$(HOME_PATH)
ccflags-y       += -DAXCL_BUILD_VERSION=\"$(SDK_VERSION)\"
ccflags-y       += -DCOMPILER_USERNAME=$(COMPILER_USERNAME_STR)
EXTRA_CFLAGS    += -Wno-error=date-time -Wno-date-time
EXTRA_CFLAGS    += -D$(CHIP_NAME)

.PHONY: modules install clean
.NOTPARALLEL: clean install

all: modules

modules: $(KBUILD_MAKEFILE)
	@$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) KCFLAGS="$(KCFLAGS)" -C $(KERNEL_DIR) M=$(KBUILD_DIR) src=$(SRC_PATH) $@ $(EXT_FLAG)

$(KBUILD_MAKEFILE): $(KBUILD_DIR)
	@$(TOUCH) $@

$(KBUILD_DIR):
	@$(MKDIR) -p $@

install: modules
ifeq ($(debug),yes)
	@$(CP) $(KBUILD_DIR)/$(MODULE_NAME).ko $(KBUILD_DIR)/$(MODULE_NAME).debug
ifneq ($(DEBUG_OUT_PATH), $(wildcard $(DEBUG_OUT_PATH)))
	$(VERB) $(MKDIR) $(DEBUG_OUT_PATH)
endif
	@$(CP) $(KBUILD_DIR)/$(MODULE_NAME).debug $(DEBUG_OUT_PATH)/$(MODULE_NAME).ko -rf
endif
	$(CROSS)strip --strip-debug $(KBUILD_DIR)/$(MODULE_NAME).ko
ifneq ($(MOD_TARGET_PATH)/ko, $(wildcard $(MOD_TARGET_PATH)/ko))
	$(VERB) $(MKDIR) $(MOD_TARGET_PATH)/ko
endif
	@cp $(KBUILD_DIR)/$(MODULE_NAME).ko $(MOD_TARGET_PATH)/ko/ -rf
	@echo -e "\e[36;1m" "INSTALL  $(KBUILD_DIR)/$(MODULE_NAME).ko to $(MOD_TARGET_PATH)/ko" "\033[0m"

clean:
	@rm -rf $(clean-objs) *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
	@rm -rf $(MOD_TARGET_PATH)/ko/$(MODULE_NAME).ko
	@rm -rf $(KBUILD_DIR)
ifeq ($(debug),yes)
	@rm -rf $(DEBUG_OUT_PATH)
endif