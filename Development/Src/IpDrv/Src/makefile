#=============================================================================
# Unreal IpDrv makefile for Linux.
#
# Revision history:
# 	* Created by Brandon Reinhart 
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(IPDRV_SRC)
INC_DIRS    = $(IPDRV_INC)
BUILD_DIR	= $(IPDRV_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) $(EXT_CXXFLAGS) -I$(IPDRV_INC) -I$(ENGINE_INC) \
			-I$(CORE_INC) -DGPackage=GPackageIpDrv 
LDFLAGS		= $(STD_LDFLAGS) $(EXT_LDFLAGS)$(IPDRV_SONAME)
LIBS		= $(STD_LIBS) $(CORE) $(ENGINE) -D_REENTRANT -lpthread

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =			\
	HTTPDownload.o	\
	IpDrv.o		\
	InternetLink.o	\
	TcpLink.o	\
	UdpLink.o	\
	UnSocket.o	\
	TcpNetDriver.o	\
	MasterServerClient.o	\
	MasterServerUplink.o	\
	UCompressCommandlet.o

OUT = $(IPDRV)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
