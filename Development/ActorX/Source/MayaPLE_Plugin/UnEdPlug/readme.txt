
Copyright (C) 2002 Secret Level and Epic Games, Inc.


_________________________________________________________________

Copyright (C) 2002 Secret Level 
Author: Michael Arnold

readme.txt : unEditor.mll

Plugin that launches unRealEd from inside Maya

To get this to work, check through the following settings to be consistent
with your own set up.

Preprocessor: Addition Include Settings
.,C:\AW\Maya4.0\include,W:\UnrealEd\Inc,..\unExport,..\ase,..\Common

Additional Library Path
C:\AW\Maya4.0\lib,W:\UnrealEd\Lib

Output file name
C:\AW\Maya4.0\bin\plug-ins\unEditor.mll

Also, include W:\System in your environment path, so that unEditor.mll can locate
unrealEdDLL.dll

CAUTION: Currently, if either Maya is closed, or the plugin unloaded, The two
programmes silently crash. Be aware of saving work prior to this moment!


_________________________________________________________________

Example files:

MyCylinder.ma
- animation export demo

From Maya
> file -f -open "MyCylinder.ma"
> loadPlugin unEditor        // launches UnrealEd
> unEditor anim myCylinder   // exports contents of set "myCylinder"

Things to notice
Animation "myCylinder" appears in package "TestPackage".
To see control of this - see attributes "package","animargs" and "scale",and
connections on set "myCylinder". Also, see attribute "URMaterialName" on
shader Phong1SG

MyTorus.ma
- mesh export demo
From Maya
> file -f -open "MyTorus.ma"
> loadPlugin unEditor      // launches UnreadEd
> unEditor mesh myTorus    // exports contents of set "myTorus"

Things to notice
Static Mesh "myTorus" appears in "TestPackage".
To see control of this, see attribute "package" and connections in "myTorus".
Also, see attribute "URMaterialName" on shader Phong1SG.
 
ISSUES: Attribute "group" for static meshes does not appear to function right
now (apologies). Also "unEditor help" does not give info on "unEditor mesh".

_________________________________________________________________
   
Copyright (C) 2002 Secret Level 
Author: Michael Arnold

readme.txt : unEditor.mll

Plugin that launches unRealEd from inside Maya

To get this to work, check through the following settings to be consistent
with your own set up.

Preprocessor: Addition Include Settings
.,C:\AW\Maya4.0\include,W:\UnrealEd\Inc,..\unExport,..\ase,..\Common

Additional Library Path
C:\AW\Maya4.0\lib,W:\UnrealEd\Lib

Output file name
C:\AW\Maya4.0\bin\plug-ins\unEditor.mll

Also, include W:\System in your environment path, so that unEditor.mll can locate
unrealEdDLL.dll

CAUTION: Currently, if either Maya is closed, or the plugin unloaded, The two
programmes silently crash. Be aware of saving work prior to this moment!

____________________________________________________________________

Copyright (C) 2002 Secret Level 
Author: Michael Arnold

readme.txt : UnrealEdDLL.dll

For launching UnrealEd from other applications

Note, cannot be called UnrealEd.dll as this causes conflicts with package
loading in UnrealEd. There may be a better work around, but that is what
it is at this moment.

The most major changes are in WinMain, and and in MainLoop (now CMainLoop)
CMainLoop should probably be re-engineered.

____________________________________________________________________

