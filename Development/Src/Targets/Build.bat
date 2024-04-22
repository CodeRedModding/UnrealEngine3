@ECHO off
REM %1 is the game name
REM %2 is the platform name
REM %3 is the configuration name

IF EXIST ..\Intermediate\UnrealBuildTool\Release\UnrealBuildTool.exe (
         ..\Intermediate\UnrealBuildTool\Release\UnrealBuildTool.exe %* -DEPLOY
) ELSE (
	ECHO UnrealBuildTool.exe not found in ..\Intermediate\UnrealBuildTool\Release\UnrealBuildTool.exe 
	EXIT /B 999
)
