SUBDIRS := benchmark

ifeq ($(HOST),ax650)
SUBDIRS += classification yolov5s
endif

################################################################################
# prepare param
################################################################################
CUR_PATH			:= $(shell pwd)
HOME_PATH			:= $(abspath $(CUR_PATH)/../../..)

include $(HOME_PATH)/build/config.mak

################################################################################
# set task
################################################################################
SUBDIRS_CLEAN		= $(addsuffix .clean, $(SUBDIRS))
SUBDIRS_INSTALL		= $(addsuffix .install, $(SUBDIRS))

.PHONY: $(SUBDIRS) $(SUBDIRS_INSTALL) $(SUBDIRS_CLEAN)
.NOTPARALLEL: clean install

all: $(SUBDIRS)
	@$(ECHO) -e $(GREEN)"\nBuild All $(CURDIR) Modules success!!\n"  $(DONE)

install: $(SUBDIRS_INSTALL)
	@$(ECHO) -e $(GREEN)"\nInstall $(CURDIR) success!!\n"  $(DONE)

clean: $(SUBDIRS_CLEAN)
	@$(ECHO) -e $(GREEN)"\nClean $(CURDIR) success!!\n"  $(DONE)

$(SUBDIRS):
	@$(ECHO)
	@$(ECHO) -e $(CYAN)"In subdir $@ ..." $(DONE)
	@$(MAKE) -C $(basename $@ )

$(SUBDIRS_INSTALL):
	@$(ECHO)
	@$(ECHO) -e $(CYAN)"In subdir $(basename $@ )..." $(DONE)
	@$(MAKE) -C $(basename $@ ) install

$(SUBDIRS_CLEAN):
	@$(ECHO) -e $(CYAN)"In subdir $(basename $@ )..." $(DONE)
	@$(MAKE) -C $(basename $@ ) clean


# Export val
export MOD_TARGET_PATH
