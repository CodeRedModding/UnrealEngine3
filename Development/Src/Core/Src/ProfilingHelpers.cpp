/**
 * Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
 * code everywhere.  And we can have consistent naming for all our files.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// Core includes.
#include "CorePrivate.h"
#include "ProfilingHelpers.h"
#include "PerfMem.h"


// find where these are really defined
#if PS3
const static INT MaxFilenameLen = 42;
#elif XBOX
const static INT MaxFilenameLen = 42;
#else
const static INT MaxFilenameLen = 100;
#endif


/**
 * This makes it so UnrealConsole will open up the memory profiler for us
 *
 * @param NotifyType has the <namespace>:<type> (e.g. UE_PROFILER!UE3STATS:)
 * @param FullFileName the File name to copy from the console
 **/
void SendDataToPCViaUnrealConsole( const FString& NotifyType, const FString& FullFileName )
{
	//warnf( TEXT("SendDataToPCViaUnrealConsole %s%s"), *NotifyType, *FullFileName );

	const FString NotifyString = NotifyType + FullFileName;
	FTCHARToANSI_Convert ToAnsi;
	ANSICHAR* AnsiCmd = ToAnsi.Convert( *NotifyString, 0, 0 );
	check(AnsiCmd);

	// send it across via UnrealConsole
	appSendNotificationString( AnsiCmd );  // why not just use: TCHAR_TO_ANSI(*NotifyString)? ??
}



/** 
 * This will generate the profiling file name that will work with limited filename sizes on consoles.
 * We want a uniform naming convention so we will all just call this function.
 *
 * @param ProfilingType this is the type of profiling file this is
 * 
 **/
FString CreateProfileFilename( const FString& InFileExtension, UBOOL bIncludeDateForDirectoryName )
{
	FString Retval;

	// set up all of the parts we will use
	
	// Create unique filename based on time.
	INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
	appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
	const FString SystemTime = FString::Printf(TEXT("%02i.%02i-%02i.%02i.%02i"), Month, Day, Hour, Min, Sec );
	const FString FileTime = FString::Printf(TEXT("%02i-%02i.%02i.%02i"), Day, Hour, Min, Sec );

	extern const FString GetMapNameStatic();
	const FString MapNameStr = GetMapNameStatic();
	const FString PlatformStr = appGetPlatformString();

	/** This is meant to hold the name of the "sessions" that is occurring **/
	static UBOOL bSetProfilingSessionFolderName = FALSE;
	static FString ProfilingSessionFolderName = TEXT(""); 

	// here we want to have just the same profiling session name so all of the files will go into that folder over the course of the run so you don't just have a ton of folders
	FString FolderName;
	if( bSetProfilingSessionFolderName == FALSE )
	{
		// now create the string
		FolderName = FString::Printf( TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *SystemTime );
		FolderName = FolderName.Right(MaxFilenameLen);

		ProfilingSessionFolderName = FolderName;
		bSetProfilingSessionFolderName = TRUE;
	}
	else
	{
		FolderName = ProfilingSessionFolderName;
	}

	// now create the string
	// NOTE: due to the changelist this is implicitly using the same directory
	FString FolderNameOfProfileNoDate = FString::Printf( TEXT("%s-%s-%i"), *MapNameStr, *PlatformStr, GetChangeListNumberForPerfTesting() );
	FolderNameOfProfileNoDate = FolderNameOfProfileNoDate.Right(MaxFilenameLen);


	FString NameOfProfile = FString::Printf( TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *FileTime );
	NameOfProfile = NameOfProfile.Right(MaxFilenameLen);

	FString FileNameWithExtension = FString::Printf( TEXT("%s%s"), *NameOfProfile, *InFileExtension );
	FileNameWithExtension = FileNameWithExtension.Right(MaxFilenameLen);

	FString Filename;
	if( bIncludeDateForDirectoryName == TRUE )
	{
		Filename = FolderName + PATH_SEPARATOR + FileNameWithExtension;
	}
	else
	{
		Filename = FolderNameOfProfileNoDate + PATH_SEPARATOR + FileNameWithExtension;
	}


	Retval = Filename;

	return Retval;
}



FString CreateProfileDirectoryAndFilename( const FString& InSubDirectoryName, const FString& InFileExtension )
{
	// Create unique filename based on time.
	INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
	appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
	const FString SystemTime = FString::Printf(TEXT("%02i.%02i-%02i.%02i"), Month, Day, Hour, Min );

	extern const FString GetMapNameStatic();
	const FString MapNameStr = GetMapNameStatic();
	const FString PlatformStr = FString(
#if PS3
		TEXT("PS3")
#elif XBOX
		TEXT("Xe")
#else
		TEXT("PC")
#endif // PS3
		);


	// create Profiling dir and sub dir
	const FString PathName = (appProfilingDir() + InSubDirectoryName + PATH_SEPARATOR );
	GFileManager->MakeDirectory( *PathName );
	//warnf( TEXT( "CreateProfileDirectoryAndFilename: %s"), *PathName );

		// create the directory name of this profile
	FString NameOfProfile = FString::Printf( TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *SystemTime );
		NameOfProfile = NameOfProfile.Right(MaxFilenameLen);
		GFileManager->MakeDirectory( *(PathName+NameOfProfile) );
		//warnf( TEXT( "CreateProfileDirectoryAndFilename: %s"), *(PathName+NameOfProfile) );


	// create the actual file name
	FString FileNameWithExtension = FString::Printf( TEXT("%s%s"), *NameOfProfile, *InFileExtension );
	FileNameWithExtension = FileNameWithExtension.Left(MaxFilenameLen);
	//warnf( TEXT( "CreateProfileDirectoryAndFilename: %s"), *FileNameWithExtension );


	FString Filename = PathName + NameOfProfile + PATH_SEPARATOR + FileNameWithExtension;
	//warnf( TEXT( "CreateProfileDirectoryAndFilename: %s"), *Filename );

	return Filename;
}
