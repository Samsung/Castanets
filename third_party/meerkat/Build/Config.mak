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
#  @todo   Project configration

#=========================================================================
# Directory Information
#=========================================================================


#-------------------------------------------------------------------------
#  Each Component Root Directory
#-------------------------------------------------------------------------

COM_ROOT_DIR		= ../Component
COM_BASE_DIR		= ../Component/mmBASE
COM_SOCK_DIR		= ../Component/mmSOCK
COM_OSAL_DIR		= ../Component/mmOSAL
COM_INC_DIR		    = ../Component/mmINC
COM_NM_DIR		    = ../Component/mmNM
COM_DS_DIR		    = ../Component/mmDiscovery

export COM_ROOT_DIR COM_OSAL_DIR COM_INC_DIR COM_BASE_DIR COM_SOCK_DIR COM_NM_DIR COM_DS_DIR


#-------------------------------------------------------------------------
#  Include Path
#-------------------------------------------------------------------------

COM_BASE_ADD_PATH		= $(COM_BASE_DIR) $(COM_BASE_DIR)/BaseAPI $(COM_BASE_DIR)/SubSystem
COM_OSAL_ADD_PATH		= $(COM_OSAL_DIR)
COM_SOCK_ADD_PATH		= $(COM_SOCK_DIR) 
COM_INC_ADD_PATH		= $(COM_INC_DIR)
COM_NM_ADD_PATH			= $(COM_NM_DIR) $(COM_NM_DIR)/tunneling $(COM_NM_DIR)/server $(COM_NM_DIR)/Interface $(COM_NM_DIR)/Util
COM_DS_ADD_PATH         = $(COM_DS_DIR)

export COM_OSAL_ADD_PATH COM_INC_ADD_PATH COM_BASE_ADD_PATH COM_SOCK_ADD_PATH COM_NM_ADD_PATH COM_DS_ADD_PATH

#-------------------------------------------------------------------------
#  Library Path
#-------------------------------------------------------------------------
PROJECT_LIBRARY_PATH = LIB

export PROJECT_LIBRARY_PATH

#-------------------------------------------------------------------------
#  Build helpers
#-------------------------------------------------------------------------
ifeq ($(OS_TYPE),LINUX)
CONFIG_HAVE_PKGCONFIG=1
export CONFIG_HAVE_PKGCONFIG
else
ifeq ($(OS_TYPE),TIZEN)
CONFIG_HAVE_PKGCONFIG=1
export CONFIG_HAVE_PKGCONFIG
endif
endif
