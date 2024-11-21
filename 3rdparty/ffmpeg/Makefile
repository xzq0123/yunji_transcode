CUR_PATH           := $(shell pwd)
AXCL_HOME_PATH     := $(CUR_PATH)/../..

include $(AXCL_HOME_PATH)/build/config.mak

SRC_PATH           := $(CUR_PATH)/$(ARCH)
INSTALL_PATH       := $(AXCL_LIB_PATH)/ffmpeg

all:
	@$(ECHO) -e $(GREEN)"\nBuild ffmpeg success!!\n"  $(DONE)

install:
	@$(MKDIR) $(INSTALL_PATH)
	@$(ECHO) -e $(GREEN)"install: $(SRC_PATH)"  $(DONE)
	@$(CP) -d $(SRC_PATH)/lib/libavcodec.so* $(INSTALL_PATH)
	@$(CP) -d $(SRC_PATH)/lib/libavfilter.so* $(INSTALL_PATH)
	@$(CP) -d $(SRC_PATH)/lib/libavformat.so* $(INSTALL_PATH)
	@$(CP) -d $(SRC_PATH)/lib/libavutil.so* $(INSTALL_PATH)
	@$(CP) -d $(SRC_PATH)/lib/libswresample.so* $(INSTALL_PATH)
	@$(CP) -d $(SRC_PATH)/lib/libswscale.so* $(INSTALL_PATH)
	@$(ECHO) -e $(GREEN)"\nInstall ffmpeg success!!\n"  $(DONE)

clean:
	@$(RM) $(INSTALL_PATH)
	@$(ECHO) -e $(GREEN)"\nClean ffmpeg success!!\n"  $(DONE)