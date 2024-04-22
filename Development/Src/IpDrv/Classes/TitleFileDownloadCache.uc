/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Local caching functionality for downloaded title files
 */
class TitleFileDownloadCache extends MCPBase
	native
	config(Engine)
	implements(OnlineTitleFileCacheInterface)
	dependson(OnlineSubsystem);

/** File operations being performed */
enum ETitleFileFileOp
{
	TitleFile_None,
	TitleFile_Save,
	TitleFile_Load
};

/** Entry for a file that has been loaded/saved or is in the process of doing so */
struct native TitleFileCacheEntry extends OnlineSubsystem.TitleFile
{
	/** Logical name to assign to the physical filename */
	var string LogicalName;
	/** CRC hash of the file that was read */
	var string Hash;
	/** Last file operation for this cached entry */
	var ETitleFileFileOp FileOp;
	/** Archive for loading/saving file. Only valid during async operation */
	var private native const pointer Ar{class FArchive};
};
/** List of files that have been processed */
var array<TitleFileCacheEntry> TitleFiles;

/** The list of delegates to notify when a file is loaded */
var private array<delegate<OnLoadTitleFileComplete> > LoadCompleteDelegates;
/** The list of delegates to notify when a file is saved */
var private array<delegate<OnSaveTitleFileComplete> > SaveCompleteDelegates;

/**
 * Starts an asynchronous read of the specified file from the local cache
 *
 * @param FileName the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
native function bool LoadTitleFile(string FileName);

/**
 * Delegate fired when a file read from the local cache is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnLoadTitleFileComplete(bool bWasSuccessful,string FileName);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param LoadCompleteDelegate the delegate to add
 */
function AddLoadTitleFileCompleteDelegate(delegate<OnLoadTitleFileComplete> LoadCompleteDelegate)
{
	if (LoadCompleteDelegates.Find(LoadCompleteDelegate) == INDEX_NONE)
	{
		LoadCompleteDelegates[LoadCompleteDelegates.Length] = LoadCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param LoadCompleteDelegate the delegate to remove
 */
function ClearLoadTitleFileCompleteDelegate(delegate<OnLoadTitleFileComplete> LoadCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = LoadCompleteDelegates.Find(LoadCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		LoadCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Starts an asynchronous write of the specified file to disk
 *
 * @param FileName the name of the file to save
 * @param LogicalName the name to associate with the physical filename
 * @param FileContents the buffer to write data from
 *
 * @return true if the calls starts successfully, false otherwise
 */
native function bool SaveTitleFile(string FileName,string LogicalName,array<byte> FileContents);

/**
 * Delegate fired when a file save to the local cache is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnSaveTitleFileComplete(bool bWasSuccessful,string FileName);

/**
 * Adds the delegate to the list to be notified when a requested file has been saved
 *
 * @param SaveCompleteDelegate the delegate to add
 */
function AddSaveTitleFileCompleteDelegate(delegate<OnSaveTitleFileComplete> SaveCompleteDelegate)
{
	if (SaveCompleteDelegates.Find(SaveCompleteDelegate) == INDEX_NONE)
	{
		SaveCompleteDelegates[SaveCompleteDelegates.Length] = SaveCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param SaveCompleteDelegate the delegate to remove
 */
function ClearSaveTitleFileCompleteDelegate(delegate<OnSaveTitleFileComplete> SaveCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = SaveCompleteDelegates.Find(SaveCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		SaveCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
native function bool GetTitleFileContents(string FileName,out array<byte> FileContents);

/**
 * Determines the async state of the tile file read operation
 *
 * @param FileName the name of the file to check on
 *
 * @return the async state of the file read
 */
native function EOnlineEnumerationReadState GetTitleFileState(string FileName);

/**
 * Determines the hash of the tile file that was read
 *
 * @param FileName the name of the file to check on
 *
 * @return the hash string for the file
 */
native function string GetTitleFileHash(string FileName);

/**
 * Determines the hash of the tile file that was read
 *
 * @param FileName the name of the file to check on
 *
 * @return the logical name of the for the given physical filename
 */
native function string GetTitleFileLogicalName(string FileName);

/**
 * Empties the set of cached files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
native function bool ClearCachedFiles();

/**
 * Empties the cached data for this file if it is not being loaded/saved currently
 *
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be cleared, false if it could not
 */
native function bool ClearCachedFile(string FileName);

/**
 * Deletes the set of title files from disc
 *
 * @param MaxAgeSeconds if > 0 then any files older than max seconds are deleted, if == 0 then all files are deleted
 * @return true if they could be deleted, false if they could not
 */
native function bool DeleteTitleFiles(float MaxAgeSeconds);

/**
 * Deletes a single file from disc
 *
 * @param FileName the name of the file to delete
 *
 * @return true if it could be deleted, false if it could not
 */
native function bool DeleteTitleFile(string FileName);


cpptext
{
	/**
	 * Ticks any outstanding async tasks that need processing
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Fires the delegates so the caller knows the file load/save is complete
	 *
	 * @param TitleFile the information for the file that was loaded/saved
	 * @param FileOp read/write opeartion on the file to know which delegates to call
	 */
	void TriggerDelegates(const FTitleFileCacheEntry* TitleFile,ETitleFileFileOp FileOp);

	/**
	 * Searches the list of files for the one that matches the filename
	 *
	 * @param FileName the file to search for
	 *
	 * @return the file details
	 */
	FTitleFileCacheEntry* GetTitleFile(const FString& FileName);

	/**
	 * @return base path to all cached files
	 */
	FString GetCachePath() const;
};
