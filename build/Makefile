CUR_PATH	    := $(shell pwd)
AXCL_HOME_PATH  ?= $(abspath $(dir $(CUR_PATH))..)

include $(CUR_PATH)/config.mak

COMPILE_TIME   := $(shell date +"%Y%m%d%H%M")
TARGET_PATH	   := $(AXCL_HOME_PATH)/out/$(NAME)_$(OS)_${HOST}

# Base dir
THIRD_PATH     := $(AXCL_HOME_PATH)/3rdparty
SAMPLE_PATH    := $(AXCL_HOME_PATH)/sample
DRV_PATH       := $(AXCL_HOME_PATH)/drv

SUBDIRS        += $(THIRD_PATH) \
                  $(SAMPLE_PATH)

############################################################################
#	build
############################################################################
SUBDIRS_ALL     = $(addsuffix .all, $(SUBDIRS))
SUBDIRS_CLEAN   = $(addsuffix .clean, $(SUBDIRS))
SUBDIRS_INSTALL = $(addsuffix .install, $(SUBDIRS))

.EXPORT_ALL_VARIABLES:

.PHONY: $(SUBDIRS) $(SUBDIRS_ALL) $(SUBDIRS_INSTALL) $(SUBDIRS_CLEAN)
.NOTPARALLEL:

default: $(SUBDIRS)

all: $(SUBDIRS_ALL)
	@echo -e $(GREEN)"\nBuild All Modules success!!\n"  $(DONE)

install: $(SUBDIRS_INSTALL)
	@echo -e $(CYAN)"\nInstall success!!\n"  $(DONE)

clean:	$(SUBDIRS_CLEAN)
#	@rm -rf $(TARGET_PATH)
	@echo -e $(GREEN)"\nClean success!!\n"  $(DONE)


$(SUBDIRS):
	@$(ECHO)
	@$(ECHO) -e $(CYAN)"In subdir $@ ..." $(DONE)
	@$(MAKE) -C $(basename $@ )

$(SUBDIRS_ALL):
	@$(ECHO) -e $(CYAN)"Making $(notdir $@)" $(DONE)
	@$(ECHO) -e $(CYAN)"In subdir $(basename $(notdir $@) )..." $(DONE)
	@$(MAKE) -C $(basename $@ ) all

$(SUBDIRS_INSTALL):
	@$(ECHO)
	@$(ECHO) -e $(CYAN)"In subdir $(basename $(notdir $@) )..." $(DONE)
	@$(MAKE) -C $(basename $@ ) install

$(SUBDIRS_CLEAN):
	@$(ECHO) -e $(CYAN)"In subdir $(basename $(notdir $@) )..." $(DONE)
	@$(MAKE) -C $(basename $@ ) clean
