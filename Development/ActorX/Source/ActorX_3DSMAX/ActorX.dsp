# Microsoft Developer Studio Project File - Name="ActorX" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=ActorX - Win32 Hybrid
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ActorX.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ActorX.mak" CFG="ActorX - Win32 Hybrid"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ActorX - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ActorX - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ActorX - Win32 Hybrid" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ActorX - Win32 ReleaseMax42" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ActorX - Win32 ReleaseMax40" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ActorX - Win32 ReleaseMax50" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ActorX - Win32 ReleaseMax51" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/ActorX", SUCBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ActorX - Win32 Release"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax5\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax5\plugins\ActorX.dlu" /libpath:"D:\3dsmax5\maxsdk\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ActorX - Win32 Debug"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MDd /W3 /Gm /GX /Zi /Od /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax5\maxsdk\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /YX"ActorX.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /debug /machine:I386 /out:"D:\3dsmax5\plugins\ActorX.dlu" /pdbtype:sept /libpath:"D:\3dsmax5\maxsdk\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ActorX - Win32 Hybrid"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "ActorX___Win32_Hybrid"
# PROP BASE Intermediate_Dir "ActorX___Win32_Hybrid"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "ActorX___Win32_Hybrid"
# PROP Intermediate_Dir "ActorX___Win32_Hybrid"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MD /W3 /Gm /GX /Zi /Od /I "f:\3dsmax\maxsdk\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_MBCS" /D "_USRDLL" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MD /W3 /Gm /GX /Zi /Od /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax5\maxsdk\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /debug /machine:I386 /out:"f:\3dsmax\plugins\ActorX.dlu" /pdbtype:sept /libpath:"f:\3dsmax\maxsdk\lib"
# ADD LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /debug /machine:I386 /out:"D:\3dsmax5\plugins\ActorX.dlu" /pdbtype:sept /libpath:"D:\3dsmax5\maxsdk\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ActorX - Win32 ReleaseMax42"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ActorX___Win32_ReleaseMax42"
# PROP BASE Intermediate_Dir "ActorX___Win32_ReleaseMax42"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ActorX___Win32_ReleaseMax42"
# PROP Intermediate_Dir "ActorX___Win32_ReleaseMax42"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax5\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax4\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax5\plugins\ActorX.dlu" /libpath:"D:\3dsmax5\maxsdk\lib"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax4\plugins\ActorX.dlu" /libpath:"D:\3dsmax4\maxsdk\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ActorX - Win32 ReleaseMax40"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ActorX___Win32_ReleaseMax40"
# PROP BASE Intermediate_Dir "ActorX___Win32_ReleaseMax40"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ActorX___Win32_ReleaseMax40"
# PROP Intermediate_Dir "ActorX___Win32_ReleaseMax40"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax4\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax4.0\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax4\plugins\ActorX.dlu" /libpath:"D:\3dsmax4\maxsdk\lib"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax4.0\plugins\ActorX.dlu" /libpath:"D:\3dsmax4.0\maxsdk\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ActorX - Win32 ReleaseMax50"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ActorX___Win32_ReleaseMax50"
# PROP BASE Intermediate_Dir "ActorX___Win32_ReleaseMax50"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ActorX___Win32_ReleaseMax50"
# PROP Intermediate_Dir "ActorX___Win32_ReleaseMax50"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax5\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /FR /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax5.0\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax5\plugins\ActorX.dlu" /libpath:"D:\3dsmax5\maxsdk\lib"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax5.0\plugins\ActorX.dlu" /libpath:"D:\3dsmax5.0\maxsdk\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "ActorX - Win32 ReleaseMax51"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ActorX___Win32_ReleaseMax510"
# PROP BASE Intermediate_Dir "ActorX___Win32_ReleaseMax510"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ActorX___Win32_ReleaseMax510"
# PROP Intermediate_Dir "ActorX___Win32_ReleaseMax510"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax5\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /FR /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /O2 /I "C:\Program Files\Microsoft SDK\Include" /I "D:\3dsmax5\maxsdk\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MAX" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax5\plugins\ActorX.dlu" /libpath:"D:\3dsmax5\maxsdk\lib"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 COMCTL32.LIB KERNEL32.LIB USER32.LIB GDI32.LIB WINSPOOL.LIB COMDLG32.LIB ADVAPI32.LIB SHELL32.LIB OLE32.LIB OLEAUT32.LIB UUID.LIB bmm.lib core.lib geom.lib gfx.lib mesh.lib util.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib GEOM.lib GFX.lib CORE.lib MAXUTIL.lib MESH.lib /nologo /base:"0X2D290000" /subsystem:windows /dll /machine:I386 /out:"D:\3dsmax5\plugins\ActorX.dlu" /libpath:"D:\3dsmax5\maxsdk\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "ActorX - Win32 Release"
# Name "ActorX - Win32 Debug"
# Name "ActorX - Win32 Hybrid"
# Name "ActorX - Win32 ReleaseMax42"
# Name "ActorX - Win32 ReleaseMax40"
# Name "ActorX - Win32 ReleaseMax50"
# Name "ActorX - Win32 ReleaseMax51"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ActorX.cpp
# End Source File
# Begin Source File

SOURCE=.\Dialogs.cpp
# End Source File
# Begin Source File

SOURCE=.\MaxInterface.cpp
# End Source File
# Begin Source File

SOURCE=.\MayaInterface.cpp
# End Source File
# Begin Source File

SOURCE=.\SceneIFC.cpp
# End Source File
# Begin Source File

SOURCE=.\UnSkeletal.cpp
# End Source File
# Begin Source File

SOURCE=.\Win32IO.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ActorX.h
# End Source File
# Begin Source File

SOURCE=.\DynArray.h
# End Source File
# Begin Source File

SOURCE=.\MaxInterface.h
# End Source File
# Begin Source File

SOURCE=.\Phyexp.h
# End Source File
# Begin Source File

SOURCE=.\PHYEXP3.H
# End Source File
# Begin Source File

SOURCE=.\SceneIFC.h
# End Source File
# Begin Source File

SOURCE=.\UnSkeletal.h
# End Source File
# Begin Source File

SOURCE=.\Vertebrate.h
# End Source File
# Begin Source File

SOURCE=.\Win32IO.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\actorX.bmp
# End Source File
# Begin Source File

SOURCE=.\Res\ActorX.def
# End Source File
# Begin Source File

SOURCE=.\Res\ActorX.rc
# End Source File
# Begin Source File

SOURCE=.\Res\actorXnew.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\Res\cursor1.cur
# End Source File
# Begin Source File

SOURCE=.\Res\cursor3.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor4.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor5.cur
# End Source File
# Begin Source File

SOURCE=.\Res\cursor6.cur
# End Source File
# Begin Source File

SOURCE=.\Res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\Res\resource.h
# End Source File
# Begin Source File

SOURCE=.\Res\Unreal140.bmp
# End Source File
# End Group
# Begin Group "Misc"

# PROP Default_Filter ""
# End Group
# End Target
# End Project
