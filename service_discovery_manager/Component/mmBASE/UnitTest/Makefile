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

BUILD_ROOT		=../../../Build/

include $(BUILD_ROOT).env
include $(BUILD_ROOT)Config.mak
include $(BUILD_ROOT)Arch.mak

CC				= $(CROSS_COMPILE)gcc
CXX				= $(CROSS_COMPILE)g++
LD				= $(CROSS_COMPILE)ld
AR				= $(CROSS_COMPILE)ar
OBJCOPY		= $(CROSS_COMPILE)objcopy
STRIP			= $(CROSS_COMPILE)strip
CFLAGS		= -g -Wall
CFLAGS		+= -DLINUX


EXECS           = msg thread task sock signal
OBJS            =

INCLUDE         = ./ $(COM_BASE_ADD_PATH) $(COM_OSAL_ADD_PATH) $(COM_INC_ADD_PATH) 
INCLUDES		= $(addprefix $(BUILD_ROOT), $(INCLUDE))


LDPATH          = $(PROJECT_LIBRARY_PATH)/X64
LDPATHS         = $(addprefix $(BUILD_ROOT), $(LDPATH))

LIBS            = mmNM pthread stdc++

LDFLAGS         += $(addprefix -L, $(LDPATHS)) $(addprefix -l, $(LIBS))
CFLAGS			+= $(addprefix -I, $(INCLUDES))

UT_SRC_DIR      = ./src
UT_OUT_DIR		= ./out
UT_OBJ_DIR		= $(UT_OUT_DIR)/obj
UT_BIN_DIR	    = $(UT_OUT_DIR)/bin

vpath %.c		$(UT_SRC_DIR)
vpath %.cpp		$(UT_SRC_DIR)

INCLUDES += -I./INC

TARGET_OBJS = $(addprefix $(UT_OBJ_DIR)/, $(EXECS))
TARGET_BINS = $(addprefix $(UT_BIN_DIR)/, $(EXECS))

all : $(EXECS)
	
msg : ut_Message.o
	$(CC) -o $(UT_BIN_DIR)/$@ -Wl,--start-group $(UT_OBJ_DIR)/msg.o $(LDFLAGS) -Wl,--end-group

thread : ut_Thread.o
	$(CC) -o $(UT_BIN_DIR)/$@ -Wl,--start-group $(UT_OBJ_DIR)/ut_Thread.o $(LDFLAGS) -Wl,--end-group

task : ut_Task.o
	$(CC) -o $(UT_BIN_DIR)/$@ -Wl,--start-group $(UT_OBJ_DIR)/ut_Task.o $(LDFLAGS) -Wl,--end-group
			
sock : ut_Socket.o
	$(CC) -o $(UT_BIN_DIR)/$@ -Wl,--start-group $(UT_OBJ_DIR)/ut_Socket.o $(LDFLAGS) -Wl,--end-group

dispatcher : ut_Dispatcher.o
	$(CC) -o $(UT_BIN_DIR)/$@ -Wl,--start-group $(UT_OBJ_DIR)/ut_Dispatcher.o $(LDFLAGS) -Wl,--end-group

signal : ut_Signal.o
	$(CC) -o $(UT_BIN_DIR)/$@ -Wl,--start-group $(UT_OBJ_DIR)/ut_Signal.o $(LDFLAGS) -Wl,--end-group


clean:
	$(RM) $(addsuffix .o, $(TARGET_OBJS)) $(TARGET_BINS) $(UT_OBJ_DIR)/*.o
	
.SUFFIXES: .c.cpp.o
.c.o:
	$(RM) $@
	$(CC) -c $(CFLAGS) $< -o $@

.cpp.o:
	@$(RM) $@
	@$(CC) $< -c -o $(UT_OBJ_DIR)/$@ $(CFLAGS)
	
