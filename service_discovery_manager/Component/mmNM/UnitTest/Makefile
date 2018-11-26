# 
#  Project Name     : WIG. 
#
#  CopyRight        : 2008 by SAMSUNG Electronics Inc.
#                     All right reserved.
#
#  Project Description :
#  This software is the confidential and proprietary information
#  of Samsung Electronics, Inc.(Confidential Information).  You
#  shall not disclose such Confidential Information and shall use
#  it only in accordance with the terms of the license agreement
#  you entered into with Samsung.
#
#  @author :namgung Eun (nke94@samsung.com)
#          \n Dept : S/W 요소기술 Lab
#          \n Module name : Makefile
# 
#  @date   2008-06-17
# 
#  @todo   Makefile for MP Each Component Unit Test

include ../../../Build/Config.mak
include ../../../Build/Arch.mak

CC				= $(CROSS_COMPILE)gcc
CXX				= $(CROSS_COMPILE)g++
LD				= $(CROSS_COMPILE)ld
AR				= $(CROSS_COMPILE)ar
OBJCOPY			= $(CROSS_COMPILE)objcopy
STRIP			= $(CROSS_COMPILE)strip
CFLAGS		   	= -g -Wall
CFLAGS			+= -DLINUX

BUILD_ROOT		=../../../Build/

EXECS			= nt ns
OBJS			=

INCLUDE       	=  ./ $(COM_OSAL_ADD_PATH) $(COM_PH_ADD_PATH) $(COM_INC_ADD_PATH) $(COM_BASE_ADD_PATH) $(COM_API_ADD_PATH) $(COM_NM_ADD_PATH)
INCLUDES		=$(addprefix $(BUILD_ROOT), $(INCLUDE))

#ifeq ($(SIM), y)
LDPATH        	= $(PROJECT_LIBRARY_PATH)/i386 $(LIB_XML_DIR)/Lib/i386
#else
#LDPATH        	+= $(PROJECT_LIBRARY_PATH)/mips2 $(LIB_XML_DIR)/Lib/mips2
#endif
LDPATHS			=$(addprefix $(BUILD_ROOT), $(LDPATH))

LIBS           	= mmNM pthread stdc++

LDFLAGS        	+= $(addprefix -L, $(LDPATHS)) $(addprefix -l, $(LIBS))
CFLAGS			+= $(addprefix -I, $(INCLUDES))

UT_SRC_DIR			= ./src
UT_OBJ_DIR			= $(UT_SRC_DIR)/obj
UT_BIN_DIR			= $(UT_SRC_DIR)/bin

vpath %.c		$(UT_SRC_DIR)
vpath %.cpp		$(UT_SRC_DIR)

INCLUDES += -I./INC

all : $(EXECS)


nt : ut_NetTunneling.o
	cd ../../../Build; make mmnm
	$(CC) -o $(UT_BIN_DIR)/$@ -Wl,--export-dynamic -Wl,--start-group $(UT_OBJ_DIR)/ut_NetTunneling.o $(LDFLAGS) -Wl,--end-group

ns : ut_TunServer.o
	cd ../../../Build; make mmnm
	$(CC) -o $(UT_BIN_DIR)/$@ -Wl,--export-dynamic -Wl,--start-group $(UT_OBJ_DIR)/ut_TunServer.o $(LDFLAGS) -Wl,--end-group

clean:
	cd ../../../Build; make cmmnm
	$(RM) $(EXECS) $(addsuffix .o, $(EXECS)) *.o

cnt: 
	$(RM) bin/$@ obj/$(addsuffix .o, $@) *.o

cns: 
	$(RM) bin/$@ obj/$(addsuffix .o, $@) *.o
	
.SUFFIXES: .c.cpp.o
.c.o:
	$(RM) $@
	$(CC) -c $(CFLAGS) $< -o $@

.cpp.o:
	@$(RM) $@
	@$(CC) $< -c -o $(UT_OBJ_DIR)/$@ $(CFLAGS) $(INCLUDES)
	
