@echo off
REM %1 is the game name
REM %2 is the platform name
REM %3 is the configuration name
REM %4 is the output path.

Targets\Clean.exe %1 %2 %3 %4
