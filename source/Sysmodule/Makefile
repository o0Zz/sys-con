#---------------------------------------------------------------------------------
# pull in common stratosphere sysmodule configuration
#---------------------------------------------------------------------------------
THIS_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIRECTORY := $(abspath $(dir $(THIS_MAKEFILE)))

include $(CURRENT_DIRECTORY)/../../lib/Atmosphere-libs/config/templates/stratosphere.mk
include $(CURRENT_DIRECTORY)/../../lib/HIDDataInterpreter/build/switch/libhiddatainterpreter.mk

TARGET		:=	sys-con
SOURCES		+=	../ControllerSwitch ../ControllerLib ../ControllerLib/Controllers
INCLUDES	+=	../ControllerSwitch ../ControllerLib

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

CFILES      :=	$(call FIND_SOURCE_FILES,$(SOURCES),c)
CPPFILES    :=	$(call FIND_SOURCE_FILES,$(SOURCES),cpp)
SFILES      :=	$(call FIND_SOURCE_FILES,$(SOURCES),s)

BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(AMS_LIBDIRS),-I$(dir)/include) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(ATMOSPHERE_BUILD_DIR) \
			-I$(HIDDATAINTERPRETER_INCLUDE_DIR)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					$(foreach dir,$(AMS_LIBDIRS),-L$(dir)/$(ATMOSPHERE_LIBRARY_DIR)) \
					-L$(HIDDATAINTERPRETER_LIBRARY_DIR)

export BUILD_EXEFS_SRC := $(TOPDIR)/$(EXEFS_SRC)

ifeq ($(strip $(CONFIG_JSON)),)
	jsons := $(wildcard *.json)
	ifneq (,$(findstring $(TARGET).json,$(jsons)))
		export APP_JSON := $(TOPDIR)/$(TARGET).json
	else
		ifneq (,$(findstring config.json,$(jsons)))
			export APP_JSON := $(TOPDIR)/config.json
		endif
	endif
else
	export APP_JSON := $(TOPDIR)/$(CONFIG_JSON)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nsp $(TARGET).npdm $(TARGET).nso $(TARGET).elf


#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all	:	$(OUTPUT).nsp

ifeq ($(strip $(APP_JSON)),)
$(OUTPUT).nsp	:	$(OUTPUT).nso
else
$(OUTPUT).nsp	:	$(OUTPUT).nso $(OUTPUT).npdm
endif

$(OUTPUT).nso	:	$(OUTPUT).elf

$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
