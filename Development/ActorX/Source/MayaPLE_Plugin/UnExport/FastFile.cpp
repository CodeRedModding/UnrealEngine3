/*

	FastFile.cpp - file io by epic

    Copyright (c) 1999,2000 Epic MegaGames, Inc. All Rights Reserved.
*/

#include "precompiled.h"
#include "FastFile.h"

int appGetVarArgs( char* Dest, INT Count, const char*& Fmt )
{
	va_list ArgPtr;
	va_start( ArgPtr, Fmt );
	INT Result = _vsnprintf( Dest, Count, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}


// Windows stuff

void PopupBox(char* PrintboxString, ... )
{
	char TempStr[4096];
	appGetVarArgs(TempStr,4096,PrintboxString);
	MessageBox(GetActiveWindow(),TempStr, " Note: ", MB_OK);
}



LogFile DLog;
LogFile AuxLog;
