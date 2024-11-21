COMPILER_HOSTNAME_STR := \"$(shell hostname)\"
COMPILER_USERNAME_STR := \"$(shell whoami | sed 's/\\/\\\\/g')\"

CPP         := $(CROSS)g++
CPPFLAGS	+= -DCOMPILER_HOSTNAME=$(COMPILER_HOSTNAME_STR)
CFLAGS		+= -DCOMPILER_HOSTNAME=$(COMPILER_HOSTNAME_STR)
CFLAGS		+= -DCOMPILER_USERNAME=$(COMPILER_USERNAME_STR)
CFLAGS		+= -Werror -Wunused-function

# CPPFLAGS  += -D$(PROJECT) -D$(CHIP_NAME)
# CFLAGS    += -D$(PROJECT) -D$(CHIP_NAME)

ifeq ($(ARCH),arm)
CFLAGS		+= -Wno-psabi
endif

ifeq ($(SUPPORT_HW_EL),TRUE)
CFLAGS		+= -DCONFIG_SUPPORT_HW_EL
endif

ifeq ($(asan),yes)
CPPFLAGS	+= -fsanitize=leak -fsanitize=address  -fno-omit-frame-pointer
CFLAGS		+= -fsanitize=leak -fsanitize=address  -fno-omit-frame-pointer
LDFLAGS     += -fsanitize=leak -fsanitize=address  -fno-omit-frame-pointer
endif

ifneq ($(SONAME),)
LDFLAGS		+= -Wl,-soname,$(SONAME)
endif

ifeq ($(LIB_DIR),)
LIB_DIR		:= lib
endif

ifneq ($(INSTALL_LIB),)
ifneq ($(wildcard $(INSTALL_LIB)),)
MV_TARGET := $(INSTALL_LIB)
endif
endif

define BUILD_STATIC_LIB
	$(if $(wildcard $@),@$(RM) $@)
	$(if $(wildcard ar.mac),@$(RM) ar.mac)
	$(if $(filter $(LIB_DIR)%.a, $@),
		@$(ECHO) CREATE $@ > ar.mac
		@$(ECHO) SAVE >> ar.mac
		@$(ECHO) END >> ar.mac
		@$(CROSS)ar -M < ar.mac)
	$(if $(filter %.o, $^),@$(CROSS)ar -q $@ $(filter %.o, $^))
	$(if $(filter %.a, $(COMBINE_LIBS)),
		@$(ECHO) OPEN $@ > ar.mac
		$(foreach LIB, $(filter %.a, $(COMBINE_LIBS)),
			@echo ADDLIB $(LIB) >> ar.mac)
		@$(ECHO) SAVE >> ar.mac
		@$(ECHO) END >> ar.mac
		@$(CROSS)ar -M < ar.mac)
	$(if $(wildcard ar.mac),@$(RM) ar.mac)
endef

define file_exists
	$(shell if [ -e $(1) ]; then echo 1; else echo 0; fi;)
endef

#.NOTPARALLEL: clean install
all: install

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
-include $(CPPDEPS)
endif

$(TARGET): $(OBJS) $(CPPOBJS)

	@$(RM) $@;
	$(VERB) $(LINK) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(CPPOBJS) $(CLIB)
ifneq ($(LINKNAME),)
	$(VERB) $(LN) $@ $(LINKNAME)
endif
ifneq ($(RENAME),)
	$(VERB) $(MV) $@ $(RENAME)
	$(VERB) $(LN) $(RENAME) $@
endif

$(STRIPPED_TARGET): $(OBJS) $(CPPOBJS)
	@$(RM) $@;
	$(VERB) $(LINK) $(CFLAGS) $(LDFLAGS) -o $(STRIPPED_TARGET) $(OBJS) $(CPPOBJS) $(CLIB)
ifeq ($(SPLIT_DEBUG_INFO),yes)
	$(OBJCPOY) --only-keep-debug $(STRIPPED_TARGET) $(DEBUG_TARGET)
	$(STRIP) -g $(STRIPPED_TARGET)
	$(OBJCPOY) --add-gnu-debuglink=$(DEBUG_TARGET) $(STRIPPED_TARGET)
else
	$(VERB) $(CP) $@ $(DEBUG_TARGET)
	$(STRIP) $@
endif
ifneq ($(LINKNAME),)
	$(VERB) $(LN) $@ $(LINKNAME)
endif
ifneq ($(RENAME),)
	$(VERB) $(MV) $@ $(RENAME)
	$(VERB) $(LN) $(RENAME) $@
endif

$(STATIC_TARGET): $(OBJS) $(CPPOBJS)
	@-$(RM) $@
ifneq ($(COMBINE_LIBS),)
	$(call BUILD_STATIC_LIB)
else
	$(VERB) $(AR) $(STATIC_TARGET) $(OBJS) $(CPPOBJS)
endif

$(STRIPPED_STATIC_TARGET): $(OBJS) $(CPPOBJS)
	@$(RM) $@;
ifneq ($(COMBINE_LIBS),)
	$(call BUILD_STATIC_LIB)
else
	$(VERB) $(AR) $(STRIPPED_STATIC_TARGET) $(OBJS) $(CPPOBJS)
endif
	$(STRIP) --strip-unneeded $(STRIPPED_STATIC_TARGET)

$(OUTPUT)/%.o %.o : %.c
	@$(MKDIR) $(dir $@)
	$(VERB) $(ECHO) "[CC]  " $<
	$(VERB) $(CC) -MMD $(CFLAGS) $(CINCLUDE) -c -o $@ $<

$(OUTPUT)/%.o %.o : %.cpp
	@$(MKDIR) $(dir $@)
	$(VERB) $(ECHO) "[CPP]  " $<
	$(VERB) $(CPP) -MMD $(CPPFLAGS) $(CFLAGS) $(CINCLUDE) -c -o $@ $<

$(OUTPUT)/%.o %.o : %.cc
	@$(MKDIR) $(dir $@)
	$(VERB) $(ECHO) "[CPP]  " $<
	$(VERB) $(CPP) -MMD $(CPPFLAGS) $(CFLAGS) $(CINCLUDE) -c -o $@ $<

$(OUTPUT)/%.a %.a : %.a
	@$(MKDIR) $(dir $@)
	$(VERB) $(ECHO) "[LINK STATIC]  " $<
	@-$(CP) $< $(dir $@)

$(OUTPUT)/%.o %.o : %.o
	@$(MKDIR) $(dir $@)
	$(VERB) $(ECHO) "[LINK OBJ]  " $<

clean: local_clean
	$(VERB) $(RM) $(TARGET) $(STRIPPED_TARGET) $(DEBUG_TARGET) $(STATIC_TARGET) $(STRIPPED_STATIC_TARGET) $(DEPS) $(CPPDEPS) $(LINKNAME)
	$(VERB) $(RM) $(OUTPUT) $(TARGET_OUT_PATH) -R


local_clean:
ifneq ($(LOCAL_TARGETS),)
	$(VERB) $(RM) $(LOCAL_TARGETS)
endif

#  -----------------------------------------------------------------------------
#  Desc: Install TARGET
#  -----------------------------------------------------------------------------
install: $(TARGET) $(STRIPPED_TARGET) $(STATIC_TARGET) $(STRIPPED_STATIC_TARGET)
ifneq ($(INSTALL_TARGET), )
ifneq ($(INSTALL_DIR), $(dir $(INSTALL_TARGET)))

ifneq ($(INSTALL_DIR), $(wildcard $(INSTALL_DIR)))
	$(VERB) $(MKDIR) $(INSTALL_DIR)
endif
	@-$(CP) -P -p -r $(INSTALL_TARGET) $(INSTALL_DIR)
	@$(ECHO) -e "\e[36;1m" "INSTALL   $(INSTALL_TARGET) to $(INSTALL_DIR)" "\033[0m"
ifneq ($(MV_TARGET),)
ifneq ($(TARGET_OUT_PATH),)
	@$(MV)  $(MV_TARGET) $(TARGET_OUT_PATH)
endif
endif
endif
endif

ifneq ($(INSTALL_LIB), )
ifneq ($(MOD_TARGET_PATH)/$(LIB_DIR), $(dir $(INSTALL_LIB)))

ifneq ($(MOD_TARGET_PATH)/$(LIB_DIR), $(wildcard $(MOD_TARGET_PATH)/$(LIB_DIR)))
	$(VERB) $(MKDIR) $(MOD_TARGET_PATH)/$(LIB_DIR)
endif
	@-$(CP) -P -p $(INSTALL_LIB) $(MOD_TARGET_PATH)/$(LIB_DIR)/
	@$(ECHO) -e "\e[36;1m" "INSTALL   $(INSTALL_LIB) to $(MOD_TARGET_PATH)/$(LIB_DIR)" "\033[0m"
ifneq ($(MV_TARGET),)
ifneq ($(TARGET_OUT_PATH),)
	@$(MV)  $(MV_TARGET) $(TARGET_OUT_PATH)
endif
endif
endif
endif

ifneq ($(INSTALL_BIN), )
ifneq ($(MOD_TARGET_PATH)/bin, $(dir $(INSTALL_BIN)))

ifneq ($(MOD_TARGET_PATH)/bin, $(wildcard $(MOD_TARGET_PATH)/bin))
	$(VERB) $(MKDIR) $(MOD_TARGET_PATH)/bin
endif
	@-$(CP) -P -p -r $(INSTALL_BIN) $(MOD_TARGET_PATH)/bin/
	@$(ECHO) -e "\e[36;1m" "INSTALL   $(INSTALL_BIN) to $(MOD_TARGET_PATH)/bin" "\033[0m"
ifneq ($(MV_TARGET),)
ifneq ($(TARGET_OUT_PATH),)
	@$(MV)  $(MV_TARGET) $(TARGET_OUT_PATH)
endif
endif
endif
endif

ifneq ($(INSTALL_INC), )
ifneq ($(MOD_TARGET_PATH)/include, $(dir $(INSTALL_INC)))

ifneq ($(MOD_TARGET_PATH)/include, $(wildcard $(MOD_TARGET_PATH)/include))
	$(VERB) $(MKDIR) $(MOD_TARGET_PATH)/include
endif
	@-$(CP) -P -p $(INSTALL_INC) $(MOD_TARGET_PATH)/include/
	@$(ECHO) -e "\e[36;1m" "INSTALL   $(INSTALL_INC) to $(MOD_TARGET_PATH)/include" "\033[0m"
endif
endif

ifneq ($(INSTALL_DATA), )
ifneq ($(MOD_TARGET_PATH)/data, $(dir $(INSTALL_DATA)))

ifneq ($(MOD_TARGET_PATH)/data, $(wildcard $(MOD_TARGET_PATH)/data))
	$(VERB) $(MKDIR) $(MOD_TARGET_PATH)/data
endif
	@-$(CP) -r -P -p $(INSTALL_DATA) $(MOD_TARGET_PATH)/data/
	@$(ECHO) -e "\e[36;1m" "INSTALL   $(INSTALL_DATA) to $(MOD_TARGET_PATH)/data" "\033[0m"
endif
endif

ifneq ($(INSTALL_SKEL_MODELS), )
ifneq ($(MOD_TARGET_PATH)/etc/skelModels, $(dir $(INSTALL_SKEL_MODELS)))

ifneq ($(MOD_TARGET_PATH)/etc/skelModels, $(wildcard $(MOD_TARGET_PATH)/etc/skelModels))
	$(VERB) $(MKDIR) -p $(MOD_TARGET_PATH)/etc/skelModels
endif
	@-$(CP) -r -P -p $(INSTALL_SKEL_MODELS) $(MOD_TARGET_PATH)/etc/skelModels/
	@$(ECHO) -e "\e[36;1m" "INSTALL   $(INSTALL_SKEL_MODELS) to $(MOD_TARGET_PATH)/etc/skelModels" "\033[0m"
endif
endif

ifneq ($(INSTALL_JSON), )
ifneq ($(MOD_TARGET_PATH)/json, $(dir $(INSTALL_JSON)))

ifneq ($(MOD_TARGET_PATH)/json, $(wildcard $(MOD_TARGET_PATH)/json))
	$(VERB) $(MKDIR) $(MOD_TARGET_PATH)/json
endif
	@-$(CP) -r -P -p $(INSTALL_JSON) $(MOD_TARGET_PATH)/json/
	@$(ECHO) -e "\e[36;1m" "INSTALL   $(INSTALL_JSON) to $(MOD_TARGET_PATH)/json" "\033[0m"
endif
endif
