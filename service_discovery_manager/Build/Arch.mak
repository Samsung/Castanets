#
#  Project Name     : Service Discovery Manager
#
#  CopyRight        : 2017 by SAMSUNG Electronics Inc.
#                     All right reserved.
#
#  Project Description :
#  This software is the confidential and proprietary information
#  of Samsung Electronics, Inc.(Confidential Information).  You
#  shall not disclose such Confidential Information and shall use
#  it only in accordance with the terms of the license agreement
#  you entered into with Samsung.
#
#  @author : N.K.EUN (nke94@samsung.com)
#          \n Dept : Tizen Platform Lab
#          \n Module name : Makefile
#
#  @date
#
#  @todo   Target architecture configuration



CROSS_COMPILE   =

ifeq ($(TARGET_TYPE),X64)
	CROSS_COMPILE   =
	ARCH_LIBS	=
	ARCH_CFLAGS	=
endif

ifeq ($(TARGET_TYPE),TIZEN_STANDARD_ARMV7L)
	CROSS_COMPILE   =
	ARCH_LIBS	=
	ARCH_CFLAGS	= -fPIE
endif

ifeq ($(TARGET_TYPE),TIZEN_TV_PRODUCT)
	CROSS_COMPILE   =
	ARCH_LIBS	=
	ARCH_CFLAGS	= -fPIE
endif

ifeq ($(TARGET_TYPE), NOTE4)
	CROSS_COMPILE  	= arm-linux-androideabi-
	ARCH_CFLAGS	    = -Wl,-rpath-link=$(NDK_ROOT)/platforms/android-9/arch-arm/usr/lib
	ARCH_CFLAGS    += -I$(NDK_ROOT)/platforms/android-9/arch-arm/usr/include
	ARCH_CFLAGS    += -nostdlib
	ARCH_CFLAGS    += -L$(NDK_ROOT)/platforms/android-9/arch-arm/usr/lib
endif


#=========================================================================
# EXTERNAL CONFIGUARTION
# ${LIB_DIR}
# ${OBJ_DIR}
# ${BIN_DIR}
# ${PFM}
# ${PROFILE_DIR}
# ${DEBUG}
#=========================================================================


OBJ_DIR = OBJ/$(TARGET_TYPE)
BIN_DIR = BIN/$(TARGET_TYPE)
LIB_DIR = LIB/$(TARGET_TYPE)
LIB_DIR_SHARED = SO/$(TARGET_TYPE)

export LIB_DIR LIB_DIR_SHARED OBJ_DIR BIN_DIR

ifndef DEBUG
DEBUG          = y
export DEBUG
endif
