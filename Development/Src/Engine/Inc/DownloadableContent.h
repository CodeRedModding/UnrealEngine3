/*=============================================================================
	DownloadableContent.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef DOWNLOADABLECONTENT_INC__
#define DOWNLOADABLECONTENT_INC__

/** Maintains the list of changes that DLC applied to the config cache for undo purposes */
struct FDLCConfigCacheChanges
{
	/** The name of the config file that was changed */
	FString ConfigFileName;
	/** The list of sections as they existed before the change */
	TMap<FString,FConfigSection> SectionsToReplace;
	/** The list of sections that were added to the file and should be removed */
	TArray<FString> SectionsToRemove;
};

#if PS3
// global objects
extern class FDownloadableContent* GDownloadableContent;
#endif

#endif
