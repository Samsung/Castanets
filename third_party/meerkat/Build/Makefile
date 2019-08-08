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
#  @todo   MM Make


include .env
include Config.mak

#=========================================================================
# Building Block
#=========================================================================
BUILD_COMPONENT	= mmnm mmserver mmclient ntserver ntclient

CLEAN_COMPONENT	= $(addprefix c, $(BUILD_COMPONENT))

DEP_COMPONENT	= $(addprefix dep, $(BUILD_COMPONENT))
DEP_PLATFORM	= $(addprefix dep, $(BUILD_PLATFORM))

#=========================================================================
# BUILD LIBRARY
#=========================================================================

#-------------------------------COMPONNET---------------------------------

all: $(BUILD_COMPONENT)
	@echo "-------------------------------------------"
	@echo "|          Build [COMPONENT]              |"
	@echo "-------------------------------------------"

config:
	@echo "-------------------------------------------"
	@echo "|        Build MODULE [CONFIG]            |"
	@echo "-------------------------------------------"
	cd ./; ./.config

mmnm:
	@echo "-------------------------------------------"
	@echo "|  Build MODULE [ MM NM]			         |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ -f Makefile.NM

mmserver:
	@echo "-------------------------------------------"
	@echo "|  Build MODULE [ MM SERVER]              |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ -f Makefile.SERVER

mmclient:
	@echo "-------------------------------------------"
	@echo "|  Build MODULE [ MM CLIENT]              |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ -f Makefile.CLIENT

ntserver:
	@echo "-------------------------------------------"
	@echo "|  Build MODULE [ NT SERVER]              |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ -f Makefile.NTSERVER

ntclient:
	@echo "-------------------------------------------"
	@echo "|  Build MODULE [ NT CLIENT]              |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ -f Makefile.NTCLIENT


#=========================================================================
# CLEAN LIBRARY
#=========================================================================

#-------------------------------COMPONNET---------------------------------

clean: $(CLEAN_COMPONENT)
	@echo "-------------------------------------------"
	@echo "|          Clean ALL Componen             |"
	@echo "-------------------------------------------"

cmmnm:
	@echo "-------------------------------------------"
	@echo "|  Clean MODULE [ MM NM LIBRARY]			 |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ clean -f Makefile.NM

cmmserver:
	@echo "-------------------------------------------"
	@echo "|  Clean MODULE [ MM SERVER]              |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ clean -f Makefile.SERVER

cmmclient:
	@echo "-------------------------------------------"
	@echo "|  Clean MODULE [ MM CLIENT]              |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ clean -f Makefile.CLIENT

cntserver:
	@echo "-------------------------------------------"
	@echo "|  Clean MODULE [ NT SERVER]              |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ clean -f Makefile.NTSERVER

cntclient:
	@echo "-------------------------------------------"
	@echo "|  Clean MODULE [ NT CLIENT]              |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ clean -f Makefile.NTCLIENT


#=========================================================================
# DEP LIBRARY
#=========================================================================

dep: $(DEP_COMPONENT)

dmmnm:
	@echo "-------------------------------------------"
	@echo "|          Dep MODULE [MM NM]             |"
	@echo "-------------------------------------------"
	$(MAKE) -e TARGET_NOW=$@ dep -f Makefile.NM



#=========================================================================
# DIST clean
#=========================================================================

distclean: clean
	$(RM) $(OBJ_DIR)/.depend*


#=========================================================================
# HELP
#=========================================================================
help:
	@echo "=========================================================================================	"
	@echo "HELP"
	@echo "=========================================================================================	"
	@echo "----------COMPONENT----------------------------------------------------------------------	"
	@echo " make <c>mmnm    : Component Common Library  	 	<libMMNM.a>		                "
	@echo " make <c>mmserver : Component Server Runner	   	<server_runner>		                "
	@echo " make <c>mmclient : Component Client Runner	   	<client_runner>		                "
	@echo " make <c>ntserver : Component STUN Server	  	<NTServer>		                "
	@echo " make <c>ntclient : Component STUN Client	   	<NTClient>		                "
