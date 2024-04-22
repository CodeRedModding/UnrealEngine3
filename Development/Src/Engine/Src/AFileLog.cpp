/*=============================================================================
	AFileLog.cpp: Unreal Tournament 2003 mod author logging
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Stat Log Implementation.
-----------------------------------------------------------------------------*/
 
IMPLEMENT_CLASS(AFileWriter);
IMPLEMENT_CLASS(AFileLog);

UBOOL AFileWriter::OpenFile(const FString& InFilename,BYTE InFileType,const FString& InExtension,UBOOL bUnique, UBOOL bIncludeTimeStamp)
{
#if !(NO_LOGGING || FINAL_RELEASE)
	if (ArchivePtr == NULL)
	{
		// Create the default Path based on the InFileType

		FString DefaultPath;
		FString DefaultExt;
		switch (InFileType)
		{
			case	FWFT_Stats:		

					DefaultPath = appGameDir() + TEXT("Stats") PATH_SEPARATOR;
					DefaultExt = TEXT(".stats");
					break;

			case	FWFT_HTML:		

					DefaultPath = appGameDir() + TEXT("Web") PATH_SEPARATOR TEXT("DynamicHTML") PATH_SEPARATOR;
					DefaultExt = TEXT(".html");

					break;

			case	FWFT_User:		

					DefaultPath = appGameDir() + TEXT("User") PATH_SEPARATOR; 
					DefaultExt = TEXT(".txt");
					break;

			case	FWFT_Debug:		

					DefaultPath = appGameDir() + TEXT("Debug") PATH_SEPARATOR;	
					DefaultExt = TEXT(".debug");
					break;

			default:

					DefaultPath = appGameDir() + TEXT("Logs") PATH_SEPARATOR;
					DefaultExt = TEXT(".log");
					break;
		}

		// Make sure the path exists

		GFileManager->MakeDirectory(*DefaultPath,1);

		// Autoappend the default extesion if it doesn't exist

		if ( InExtension.Len() > 0 )
			DefaultExt = InExtension;

		// Attempt to generate a filename.  Keep going added _xxx to the filename until a unique

		if ( bIncludeTimeStamp )
		{
			INT Year, Month, DayOfWeek, Day, Hour, Min, Sec,MSec;
			appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
		
			Filename = FString::Printf(TEXT("%s%s-%02i%02i%02i.%02i%02i%02i%s"),*DefaultPath,*InFilename,Year,Month, Day, Hour, Min, Sec,*DefaultExt);
		}
		else
		{
			Filename = FString::Printf(TEXT("%s%s%s"),*DefaultPath,*InFilename,*DefaultExt);
		}

		if (bUnique)
		{
			INT UniqueTag=0;
			while ( GFileManager->FileSize(*Filename) >= 0 )
			{
				Filename = FString::Printf(TEXT("%s%s_%i%s"),*DefaultPath,*InFilename,++UniqueTag,*DefaultExt);
			}
		}

		debugf(TEXT("Creating file: %s"),*Filename);
		INT Flags = FILEWRITE_EvenIfReadOnly;
		// Check for async writes and change flags accordingly
		if (bWantsAsyncWrites)
		{
			bFlushEachWrite = FALSE;
			Flags |= FILEWRITE_Async;
		}

		// set the file so you can tail -f it
		Flags |= FILEWRITE_AllowRead;
		
		// and create the actual archive
		ArchivePtr = GFileManager->CreateFileWriter(*Filename, Flags, GWarn);

		return (ArchivePtr != NULL);
	}
	else
	{
		return true;
	}
#else
	return FALSE;
#endif
}

void AFileWriter::CloseFile()
{
#if !(NO_LOGGING || FINAL_RELEASE)
	// if the archive exists
	if (ArchivePtr != NULL) 
	{
		// delete it
		delete ArchivePtr;
	}
	ArchivePtr = NULL;
#endif
}

void AFileWriter::Logf(const FString& logString)
{
#if !(NO_LOGGING || FINAL_RELEASE)
	if (ArchivePtr != NULL)
	{
		// convert to ansi
		ANSICHAR ansiStr[1024];
		INT idx;
		for (idx = 0; idx < logString.Len() && idx < 1024 - 3; idx++)
		{
			ansiStr[idx] = ToAnsi((*logString)[idx]);
		}

		// null terminate
		ansiStr[idx++] = '\r';
		ansiStr[idx++] = '\n';
		ansiStr[idx] = '\0';

		// and serialize to the archive
		ArchivePtr->Serialize(ansiStr, idx);
		if (bFlushEachWrite)
		{
			ArchivePtr->Flush();
		}
	}
#endif
}

void AFileWriter::BeginDestroy()
{
	// make sure the file has been closed
	CloseFile();

	Super::BeginDestroy();
}
