#=============================================================================
# Unreal Core makefile for Linux.
#
# Revision history:
# 	* Created by Mike Danylchuk
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(CORE_SRC)
INC_DIRS	= $(CORE_INC)
BUILD_DIR	= $(CORE_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) $(EXT_CXXFLAGS) -I$(PSX2_INC) -I$(CORE_INC) \
			  -I$(OGG_INC) -I$(VORBIS_INC) -DGPackage=Core
LDFLAGS		= $(STD_LDFLAGS) $(EXT_LDFLAGS)$(CORE_SONAME)
LIBS		= $(STD_LIBS)
AS		= $(STD_AS)
ASFLAGS		= $(STD_ASFLAGS)
LCFILE		= $(PSX2_LIB)/app.cmd

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

CORE_OBJS =		\
	Core.o		\
	UExporter.o	\
	UFactory.o	\
	UnAnsi.o	\
	UnBits.o	\
	UnCache.o	\
	UnClass.o	\
	UnCoreNative.o	\
	UnCoreNet.o	\
	UnCorSc.o	\
	UnGUID.o	\
	UnLinker.o	\
	UnMath.o	\
	UnMem.o		\
	UnMisc.o	\
	UnName.o	\
	UnObj.o		\
	UnProp.o

ifeq ($(TARGETTYPE),linux)
OBJS = $(CORE_OBJS) UnUnix.o
endif

ifeq ($(TARGETTYPE),freebsd)
OBJS = $(CORE_OBJS) UnUnix.o
endif

ifeq ($(TARGETTYPE),macosx)
OBJS = $(CORE_OBJS) UnUnix.o
endif

ifeq ($(TARGETTYPE),psx2)
OBJS = $(CORE_OBJS) UnPSX2.o
endif

OUT = $(CORE)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
