#=============================================================================
# Unreal Editor makefile for Linux.
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

SRC_DIRS	= $(EDITOR_SRC)
INC_DIRS	= $(EDITOR_INC)
BUILD_DIR	= $(EDITOR_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) $(EXT_CXXFLAGS) -I$(EDITOR_INC) -I$(ENGINE_INC) \
			-I$(CORE_INC) -DGPackage=GPackageEditor
LDFLAGS		= $(STD_LDFLAGS) $(EXT_LDFLAGS)$(EDITOR_SONAME)
LIBS		= $(STD_LIBS) $(CORE) $(ENGINE)

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =  \
	Editor.o \
	UAnalyzeBuild.o \
	UAnalyzeContentCommandlet.o \
	UBatchExportCommandlet.o \
	UBrushBuilder.o \
	UConformCommandlet.o \
	UConvertMaterialCommandlet.o \
	UCutdownContentCommandlet.o \
	UDXTConvertCommandlet.o \
	UDumpIntCommandlet.o \
	UGroupRepairCommandlet.o \
	UMakeCommandlet.o \
	UMapConvertCommandlet.o \
	UMasterCommandlet.o \
	UPS2ConvertCommandlet.o \
	UPkgCommandlet.o \
	URebuildCommandlet.o \
	USurfaceTypeCommandlet.o \
	UTextureInfoCommandlet.o \
	UTextureLODCommandlet.o \
	UTextureStripCommandlet.o \
	UnBsp.o \
	UnEdCnst.o \
	UnEdCsg.o \
	UnEdExp.o \
	UnEdFact.o \
	UnEdSrv.o \
	UnEdSrvExecImporters.o \
	UnEdTran.o \
	UnEditor.o \
	UnLightwaveImporter.o \
	UnMeshEd.o \
	UnMeshLP.o \
	UnParams.o \
	UnPrefab.o \
	UnScrCom.o \
	UnShadow.o \
	UnStaticMesh.o \
	UnTopics.o \
	UnVisi.o \
	xEdInt.o \
	xObjExporters.o \
	xTextureConversions.o

OUT = $(EDITOR)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
