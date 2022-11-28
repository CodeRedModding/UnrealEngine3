/*=============================================================================
	AFileLog.cpp: Unreal Tournament 2003 mod author logging
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Joe Wilcox
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Stat Log Implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AFileLog);

void AFileLog::OpenLog(FString &fileName,FString &extension)
{
	if (LogAr == NULL)
	{
		// Strip all pathing characters from the name
		for (INT i = 0; i < fileName.Len(); i++)
		{
			if ( (*fileName)[i]=='\\' || (*fileName)[i]=='.')
			{
				((TCHAR*)(*fileName))[i] = '_';
			}
		}
		// append the extension
		fileName += extension;
		// save the new log file name
		fileName = FString::Printf(TEXT("%s%s"),*appGameLogDir(),*fileName);
		LogFileName = fileName;
		debugf(TEXT("Opening user log %s"),*LogFileName);
		// and create the actual archive
		LogAr = GFileManager->CreateFileWriter(*LogFileName, FILEWRITE_EvenIfReadOnly);
	}

}

void AFileLog::execOpenLog( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(fileName);
	P_GET_STR_OPTX(extension,TEXT(".txt"));
	P_FINISH;
	OpenLog(fileName,extension);
}

void AFileLog::execCloseLog( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	// if the archive exists
	if (LogAr != NULL) 
	{
		// delete it
		delete (FArchive*)LogAr;
	}
	LogAr = NULL;
}

void AFileLog::Logf(FString &logString)
{
	if (LogAr != NULL)
	{
		// append eol characters
		logString += TEXT("\r\n");
		check(logString.Len() < 1024 && "Can only log strings <1024 length");
		// and convert to ansi
		ANSICHAR ansiStr[1024];
		INT idx;
		for (idx = 0; idx < logString.Len(); idx++)
		{
			ansiStr[idx] = ToAnsi((*logString)[idx]);
		}
		// null terminate
		ansiStr[idx] = '\0';
		// and serialize to the archive
		((FArchive*)LogAr)->Serialize(ansiStr, idx);
		((FArchive*)LogAr)->Flush();
	}
}

void AFileLog::execLogf( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(logString);
	P_FINISH;
	Logf(logString);
}
