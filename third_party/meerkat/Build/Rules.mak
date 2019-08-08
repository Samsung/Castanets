
include Arch.mak


override CFLAGS += -Wall -std=c++11
override CFLAGS += $(ARCH_CFLAGS)


CC	= $(CROSS_COMPILE)gcc
CXX	= $(CROSS_COMPILE)g++
LD	= $(CROSS_COMPILE)ld
AR	= $(CROSS_COMPILE)ar
OBJCOPY	= $(CROSS_COMPILE)objcopy
STRIP	= $(CROSS_COMPILE)strip

ifeq ($(DEBUG),y)
	override CFLAGS		+= -g -export-dynamic
else
	override CFLAGS		+= -O2 -DNDEBUG
endif

COMMON_DEF = -DLINUX -D_LINUX
DEFINES = $(COMMON_DEF) $(addprefix -D, $(THIS_DEFINES))
override CFLAGS += $(THIS_CFLAGS) $(DEFINES)
override ARFLAGS = rcvs $(THIS_AR_FLAGS)

#=========================================================================
# Include & Link Options
#=========================================================================
INCLUDES += $(addprefix -I, $(SRC_DIR))
INCLUDES += $(addprefix -I,  $(THIS_INCLUDES) $(ARCH_INC_DIR) $(PROFILE_DIR))

LDPATHS = $(addprefix -L, $(LIB_DIR) $(ARCH_LIB_DIR) $(THIS_LDPATHS))
LIBS = $(addprefix -l, $(ARCH_LIBS) $(THIS_LIBS))
override LDFLAGS += $(THIS_LDFLAGS)
override LDFLAGS += $(LIBS) $(LDPATHS)


vpath	%.c			$(SRC_DIR)
vpath	%.cpp		$(SRC_DIR)
vpath	%.o			$(OBJ_DIR)

ifdef EXEC_NAME
	EXEC_OBJS = $(addsuffix .o, $(EXEC_NAME))
	SOURCEFILES = $(addsuffix .cpp, $(EXEC_NAME))
endif
ifdef CPP_NAME
	OBJ_OBJS = $(addsuffix .o, $(CPP_NAME))
	SOURCEFILES += $(addsuffix .cpp, $(CPP_NAME))
endif
ifdef C_NAME
	OBJ_OBJS += $(addsuffix .o, $(C_NAME))
	SOURCEFILES += $(addsuffix .c, $(C_NAME))
endif
ifdef LIB_NAME
	LIB_FILE = $(LIB_DIR)/lib$(LIB_NAME).a
endif
ifdef LIB_NAME_SO
	LIB_FILE_SO = $(LIB_DIR_SHARED)/lib$(LIB_NAME_SO).so
endif
ifdef INI_NAME
	INI_FILE = $(addsuffix .ini, $(INI_NAME))
endif
ifndef TARGET_DIR
	TARGET_DIR = $(BIN_DIR)
endif

#=========================================================================
# Rules
#=========================================================================
.PHONY: all dep imageclean distclean

all : imageclean $(OBJ_OBJS) $(LIB_FILE)  $(LIB_FILE_SO) $(EXEC_OBJS)
#	@echo "$(CC) -o $(TARGET_DIR)/$(EXEC_NAME) $(addprefix $(OBJ_DIR)/, $(EXEC_NAME).o $(OBJ_OBJS)) $(LDFLAGS)"
#	$(STRIP) -s -R .comment -R .reginfo -R .mdebug -R .note $(TARGET_DIR)/$$i

ifdef EXEC_NAME
#	$(CXX) -o $(TARGET_DIR)/$(EXEC_NAME) -Wl,--start-group $(addprefix $(OBJ_DIR)/, $(OBJ_OBJS)) $(LDFLAGS) -Wl,--end-group -Wl,-rpath-link=$(NDK_ROOT)/platforms/android-9/arch-arm/usr/lib -L$(NDK_ROOT)/platforms/android-9/arch-arm/usr/lib -nostdlib $(NDK_ROOT)/platforms/android-9/arch-arm/usr/lib/crtbegin_dynamic.o -lc
	$(CC) -o $(TARGET_DIR)/$(EXEC_NAME) -Wl,--start-group $(addprefix $(OBJ_DIR)/, $(OBJ_OBJS)) $(LDFLAGS) -Wl,--end-group
endif

	@for i in $(SUB_DIRS) ;\
	do \
		$(MAKE) -C $$i; \
	done
	@echo "BUILDING [$(PFM)]: $(TARGET_NOW)"
	@echo "#################################################################"

ifdef INI_NAME
	cp -f $(addprefix $(COM_ROOT_DIR)/, $(INI_FILE)) $(TARGET_DIR)
endif

dep:
	@if [ ! -f $(OBJ_DIR)/.depend-$(TARGET_NOW) ]; then \
		touch $(OBJ_DIR)/.depend-$(TARGET_NOW); \
		gccmakedep -f $(OBJ_DIR)/.depend-$(TARGET_NOW) -- $(INCLUDES) $(DEFINES)-- $(addprefix $(SRC_DIR)/, $(SOURCEFILES)); \
	fi
	@for i in $(SUB_DIRS) ;\
	do \
		$(MAKE) $@ -C $$i; \
	done

imageclean:
ifdef LIB_NAME
	$(RM) $(LIB_FILE)
endif
ifdef LIB_NAME_SO
	$(RM) $(LIB_FILE_SO)
endif
ifdef EXEC_NAME
	$(RM) $(addprefix $(TARGET_DIR)/, $(EXEC_NAME))
endif
ifdef INI_NAME
	$(RM) $(addprefix $(TARGET_DIR)/, $(INI_FILE))
endif

#=========================================================================
# always re-make lib file and move to bin dir
#=========================================================================
$(LIB_FILE):
	@echo ""
	@echo ">>> $@";
	@$(RM) $@
	@$(AR) $(ARFLAGS) $@ $(addprefix $(OBJ_DIR)/, $(OBJ_OBJS) $(ARFILES)) > /dev/null 2>&1
	@echo ""

$(LIB_FILE_SO):
	@echo ""
	@echo ">>> $@";
	@$(RM) $@
	@$(CC) -shared $(addprefix $(OBJ_DIR)/, $(OBJ_OBJS)) -o $@
	@echo ""

.SUFFIXES: .c.cpp.o
.c.o:
	@if [ ! -d $(OBJ_DIR) ]; then	\
		mkdir -p $(OBJ_DIR);	\
	fi
	@if [ ! -d $(LIB_DIR) ]; then	\
		mkdir -p $(LIB_DIR);	\
	fi
	@if [ ! -d $(BIN_DIR) ]; then	\
		mkdir -p $(BIN_DIR);	\
	fi
	@echo "<<< $<"
	@$(RM) $@
	@$(CC) $< -c -o $(OBJ_DIR)/$@ $(CFLAGS) $(INCLUDES)

.cpp.o:
	@if [ ! -d $(OBJ_DIR) ]; then	\
		mkdir -p $(OBJ_DIR);	\
	fi
	@if [ ! -d $(LIB_DIR) ]; then	\
		mkdir -p $(LIB_DIR);	\
	fi
	@if [ ! -d $(BIN_DIR) ]; then	\
		mkdir -p $(BIN_DIR);	\
	fi
	@echo "<<< $<"
	@$(RM) $@
	@$(CC) $< -c -o $(OBJ_DIR)/$@ $(CFLAGS) $(INCLUDES)

clean:
	$(RM) $(LIB_FILE)  $(LIB_FILE_SO)
	$(RM) $(addprefix $(TARGET_DIR)/, $(EXEC_NAME)) $(addprefix $(OBJ_DIR)/, $(EXEC_OBJS) $(OBJ_OBJS)) $(addprefix $(TARGET_DIR)/, $(INI_FILE))
	@for i in $(SUB_DIRS) ;\
	do \
		$(MAKE) $@ -C $$i; \
	done
	@echo ""
	@echo "CLEANING [$(PFM)]: $(TARGET_NOW)"
	@echo "#################################################################"
	@echo ""

ifneq "$(wildcard $(OBJ_DIR)/.depend-$(TARGET_NOW))" ""
	include $(OBJ_DIR)/.depend-$(TARGET_NOW)
endif
