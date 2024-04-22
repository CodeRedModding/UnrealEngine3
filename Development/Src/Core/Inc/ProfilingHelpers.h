/**
 * Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
 * code everywhere.  And we can have consistent naming for all our files.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _PROFILING_HELPERS_H_
#define _PROFILING_HELPERS_H_

enum EStreamingStatus
{
	LEVEL_Unloaded,
	LEVEL_UnloadedButStillAround,
	LEVEL_Loading,
	LEVEL_Loaded,
	LEVEL_MakingVisible,
	LEVEL_Visible,
	LEVEL_Preloading,
};

/**
 * This makes it so UnrealConsole will open up the memory profiler for us
 *
 * @param NotifyType has the <namespace>:<type> (e.g. UE_PROFILER!UE3STATS:)
 * @param FullFileName the File name to copy from the console
 **/
void SendDataToPCViaUnrealConsole( const FString& NotifyType, const FString& FullFileName );


/** 
 * This will generate the profiling file name that will work with limited filename sizes on consoles.
 * We want a uniform naming convention so we will all just call this function.
 *
 * @param ProfilingType this is the type of profiling file this is
 * 
 **/
FString CreateProfileFilename( const FString& InFileExtension, UBOOL bIncludeDateForDirectoryName );

/** 
 * This will create the directories and the file name all in one function
 **/
FString CreateProfileDirectoryAndFilename( const FString& InSubDirectoryName, const FString& InFileExtension );






#endif // _PROFILING_HELPERS_H_
