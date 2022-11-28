# Microsoft Developer Studio Project File - Name="UnEdPlug" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=UnEdPlug - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "UnEdPlug.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "UnEdPlug.mak" CFG="UnEdPlug - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "UnEdPlug - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "UnEdPlug - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "UnEdPlug"
# PROP Scc_LocalPath ".."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "UnEdPlug - Win32 Release"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "C:\development\ut2-code\UnrealEd\Inc" /I "..\unExport" /I "..\ase" /I "..\Common" /I "." /I "D:\AW\Maya4.0\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "NT_PLUGIN" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 Foundation.lib Image.lib libMDtAPI.lib OpenMaya.lib OpenMayaAnim.lib OpenMayaFX.lib OpenMayaUI.lib UnrealEdDLL.lib user32.lib /nologo /subsystem:windows /dll /machine:I386 /out:"Release\unEditor.mll" /libpath:".\Lib" /libpath:"D:\AW\Maya4.0\lib" /libpath:"c:\development\ut2-code\system" /export:initializePlugin /export:uninitializePlugin

!ELSEIF  "$(CFG)" == "UnEdPlug - Win32 Debug"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "C:\development\ut2-code\UnrealEd\Inc" /I "..\unExport" /I "..\ase" /I "..\Common" /I "." /I "D:\AW\Maya4.0\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "NT_PLUGIN" /FR /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 Foundation.lib Image.lib libMDtAPI.lib OpenMaya.lib OpenMayaAnim.lib OpenMayaFX.lib OpenMayaUI.lib UnrealEdDLL.lib user32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"Debug\unEditor.mll" /pdbtype:sept /libpath:".\Lib" /libpath:"D:\AW\Maya4.0\lib" /libpath:"c:\development\ut2003\system" /export:initializePlugin /export:uninitializePlugin

!ENDIF 

# Begin Target

# Name "UnEdPlug - Win32 Release"
# Name "UnEdPlug - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CSetInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\CStaticMeshGen.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\precompiled.cpp
# End Source File
# Begin Source File

SOURCE=.\unEditorCmd.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\CSetInfo.h
# End Source File
# Begin Source File

SOURCE=.\CStaticMeshGen.h
# End Source File
# Begin Source File

SOURCE=..\Common\maattrval.h
# End Source File
# Begin Source File

SOURCE=..\Common\precompiled.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "psk"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\UnExport\DynArray.h
# End Source File
# Begin Source File

SOURCE=..\UnExport\FastFile.cpp
# End Source File
# Begin Source File

SOURCE=..\UnExport\FastFile.h
# End Source File
# Begin Source File

SOURCE=..\UnExport\MayaInterface.cpp
# End Source File
# Begin Source File

SOURCE=..\UnExport\MayaInterface.h
# End Source File
# Begin Source File

SOURCE=..\UnExport\SceneIFC.cpp
# End Source File
# Begin Source File

SOURCE=..\UnExport\SceneIFC.h
# End Source File
# Begin Source File

SOURCE=..\UnExport\unExportCmd.cpp
# End Source File
# Begin Source File

SOURCE=..\UnExport\unExportCmd.h
# End Source File
# Begin Source File

SOURCE=..\UnExport\UnSkeletal.cpp
# End Source File
# Begin Source File

SOURCE=..\UnExport\UnSkeletal.h
# End Source File
# Begin Source File

SOURCE=..\UnExport\Vertebrate.cpp
# End Source File
# Begin Source File

SOURCE=..\UnExport\Vertebrate.h
# End Source File
# End Group
# Begin Group "ase"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\ase\exportmesh.cpp
# End Source File
# Begin Source File

SOURCE=..\ase\exportmesh.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\readme.txt
# End Source File
# End Target
# End Project
