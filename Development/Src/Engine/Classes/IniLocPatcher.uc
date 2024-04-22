/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class reads a set of files from Live/NP servers and uses it to
 * update the game.
 */
class IniLocPatcher extends Object
	native
	config(Engine);

/** Holds the list of files to download and their download state */
struct native IniLocFileEntry
{
	/** The file to read from the online service */
	var string Filename;
	/** Unique name of file only used for downloading */
	var string DLName;
	/** Hash to verify integrity of file against cached version */
	var string HashCode;
	/** Whether the file should be treated as unicode or not */
	var bool bIsUnicode;
	/** The state of that read */
	var EOnlineEnumerationReadState ReadState;
};

/** The list of files to request from the online service */
var config array<IniLocFileEntry> Files;

/** if TRUE Then file list is downloaded from EMS otherwise it comes from config */
var config bool bRequestEmsFileList;
/** Max age in seconds to keep cached EMS files */
var config int MaxCachedFileAge;

/** Interface for downloading files */
var transient OnlineTitleFileInterface TitleFileInterface;
/** Interface for caching files to/from disk */
var transient OnlineTitleFileCacheInterface TitleFileCacheInterface;

/** list of delegates that get called whenever a file is downloaded or loaded from cache */
var array<delegate<OnReadTitleFileComplete> > ReadTitleFileCompleteDelegates;
/**
 * Delegate fired when a file read from the network platform's title specific storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnReadTitleFileComplete(bool bWasSuccessful,string FileName);

/**
 * Delegate fired when all title files have completed processing
 */
delegate OnAllTitleFilesCompleted();

/**
 * Initializes the patcher, sets delegates, vars, etc.
 */
function Init()
{
	local OnlineSubsystem OnlineSub;
	local int Index;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		TitleFileInterface = OnlineSub.TitleFileInterface;
		if (TitleFileInterface != None)
		{
			// Set the callback for notifications of files completing download
			TitleFileInterface.AddReadTitleFileCompleteDelegate(OnDownloadFileComplete);
		}
		else
		{
			// Mark all as failed to be read since there is no way to read them
			for (Index = 0; Index < Files.Length; Index++)
			{
				Files[Index].ReadState = OERS_Failed;
			}
		}
		// Hook up the caching interface
		TitleFileCacheInterface = OnlineSub.TitleFileCacheInterface;
		if (TitleFileCacheInterface != None)
		{
			// Set the callback for notifications of files completing a load
			TitleFileCacheInterface.AddLoadTitleFileCompleteDelegate(OnFileCacheLoadComplete);
			TitleFileCacheInterface.AddSaveTitleFileCompleteDelegate(OnFileCacheSaveComplete);
		}
	}
}

/**
 * Reads the set of files from the online service
 */
function DownloadFiles()
{
	local int FileIdx;

	if (bRequestEmsFileList)
	{
		// Delete stale files
		if (MaxCachedFileAge > 0 &&
			TitleFileCacheInterface != None)
		{
			TitleFileCacheInterface.DeleteTitleFiles(MaxCachedFileAge);
		}
		// Get a list of files that should be downloaded
		TitleFileInterface.AddRequestTitleFileListCompleteDelegate(OnRequestTitleFileListComplete);
		TitleFileInterface.RequestTitleFileList();
	}
	else
	{
		// config DLNames and Filenames are expected to match unless requested
		for (FileIdx = 0; FileIdx < Files.Length; FileIdx++)
		{
			Files[FileIdx].DLName = Files[FileIdx].Filename;
		}
		StartLoadingFiles();
	}
}

/**
 * Delegate fired when the request for a list of files completes
 *
 * @param bWasSuccessful whether the request completed successfully
 * @param ResultStr contains the list of files and associated meta data
 */
function OnRequestTitleFileListComplete(bool bWasSuccessful, string ResultStr)
{
	local JsonObject Root;
	local int JsonObjectIdx;
	local IniLocFileEntry RequestFileEntry;
	
	TitleFileInterface.ClearRequestTitleFileListCompleteDelegate(OnRequestTitleFileListComplete);
	if (bWasSuccessful)
	{
		Root = class'JsonObject'.static.DecodeJson(ResultStr);
		if (Root != None)
		{
			Files.Length = 0;
			// fill in the list of RequestFiles
			for (JsonObjectIdx=0; JsonObjectIdx < Root.ObjectArray.Length; JsonObjectIdx++)
			{
				RequestFileEntry.Filename = Root.ObjectArray[JsonObjectIdx].GetStringValue("file_name");
				RequestFileEntry.DLName = Root.ObjectArray[JsonObjectIdx].GetStringValue("dl_name");
				RequestFileEntry.HashCode = Root.ObjectArray[JsonObjectIdx].GetStringValue("hash_code");
				// Any file that is not INT or INI should be Unicode
				RequestFileEntry.bIsUnicode = InStr(RequestFileEntry.FileName,".ini",,true) == INDEX_NONE && InStr(RequestFileEntry.FileName,".int",,true) == INDEX_NONE;
				Files.AddItem(RequestFileEntry);
			}
			// Determine which files can be loaded from the cache that don't require downloading
			StartLoadingFiles();
		}
		else
		{
			`log(`location@"Download of file list failed. Bad json."
				@"ResultStr="$ResultStr);
		}

	}
	else
	{
		`log(`location@"Download of file list failed.");
	}
}

/**
 * Kick off the cache/download requests
 */
function StartLoadingFiles()
{
	local int Index;

	// If there is online interface, then try to download the files
	if (bRequestEmsFileList)
	{
		if (TitleFileCacheInterface != None)
		{
			// Iterate through files trying to download them
            for (Index = 0; Index < Files.Length; Index++)
            {
                // Kick off the read of that file if not already started or failed
                if (Files[Index].ReadState == OERS_NotStarted)
                {
                    Files[Index].ReadState = OERS_InProgress;
                    if (!TitleFileCacheInterface.LoadTitleFile(Files[Index].DLName))
                    {
						if (Files[Index].ReadState != OERS_Done && Files[Index].ReadState != OERS_Failed)
						{
							Files[Index].ReadState = OERS_InProgress;
						}
						else
						{
							Files[Index].ReadState = OERS_Failed;
						}
                    }
                }
            }
		}
	}
	else
	{
		if (TitleFileInterface != None)
		{
			// Iterate through files trying to download them
			for (Index = 0; Index < Files.Length; Index++)
			{
				// Kick off the read of that file if not already started or failed
				if (Files[Index].ReadState == OERS_NotStarted)
				{
					// If this is a loc file name, make sure we are getting the right language
					Files[Index].Filename = UpdateLocFileName(Files[Index].Filename);
					if (TitleFileInterface.ReadTitleFile(Files[Index].DLName))
					{
						Files[Index].ReadState = OERS_InProgress;
					}
					else
					{
						Files[Index].ReadState = OERS_Failed;
					}
				}
			}
		}
	}
}

/**
 * Notifies us when the download of a file is complete
 *
 * @param bWasSuccessful true if the download completed ok, false otherwise
 * @param FileName the file that was downloaded (or failed to)
 */
function OnDownloadFileComplete(bool bWasSuccessful,string FileName)
{
	local bool bSuccessLoad;
	local int Index;
	local array<byte> FileData;

	// Iterate through files to verify that this is one that we requested
	for (Index = 0; Index < Files.Length; Index++)
	{
		if (Files[Index].DLName == FileName)
		{
			if (bWasSuccessful)
			{
				// Read the contents so that they can be processed
				if (TitleFileInterface.GetTitleFileContents(FileName,FileData) &&
					FileData.Length > 0)
				{
					bSuccessLoad = true;
					Files[Index].ReadState = OERS_Done;
					// Cache it so that the file doesn't need to be downloaded next time
					if (bRequestEmsFileList &&
						TitleFileCacheInterface != None)
					{
						TitleFileCacheInterface.SaveTitleFile(Files[Index].DLName,Files[Index].Filename,FileData);
					}
					// Clear memory copy of file from title file downloader 
					TitleFileInterface.ClearDownloadedFile(FileName);
					// patch loc and class
					ProcessIniLocFile(Files[Index].Filename,Files[Index].bIsUnicode,FileData);
				}
				else
				{
					Files[Index].ReadState = OERS_Failed;
				}
			}
			else
			{
				`Log("Failed to download the file from system interface."
					@"DLName="$Files[Index].DLName
					@"Filename="$Files[Index].Filename);
				Files[Index].ReadState = OERS_Failed;
			}
			break;
		}		
	}
	// Notify that download complete
	TriggerDownloadCompleteDelegates(bSuccessLoad,FileName);
}

/**
 * Delegate fired when a file read from the local cache is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
function OnFileCacheLoadComplete(bool bWasSuccessful,string FileName)
{
	local int Index;
	local array<byte> FileData;
	local bool bRequiresDownload;

	bRequiresDownload = true;
	// Iterate through files to verify that this is one that we requested
	for (Index = 0; Index < Files.Length; Index++)
	{
		if (Files[Index].DLName == FileName)
		{
			if (bWasSuccessful)
			{
				// Compare requested hash vs the one from the cache
				if (TitleFileCacheInterface.GetTitleFileHash(FileName) == Files[Index].HashCode)
				{
					// Read the contents so that they can be processed
					if (TitleFileCacheInterface.GetTitleFileContents(FileName,FileData) &&
						FileData.Length > 0)
					{
						Files[Index].ReadState = OERS_Done;
						bRequiresDownload = false;
						// We are now pushing .bin files through this patcher system as well.
						// In that case we don't want to process or remove the file from the cache yet.
						// The game can register another handler that will deal with them.
						if (InStr(Files[Index].Filename, ".bin",false,true) == INDEX_NONE)
						{
							// patch loc and class
							ProcessIniLocFile(Files[Index].Filename,Files[Index].bIsUnicode,FileData);
							// Clear memory copy of file from title file downloader 
							TitleFileCacheInterface.ClearCachedFile(FileName);
						}
					}
				}
				else
				{
					`Log("Hash for file cache entry not valid."
						@"DLName="$Files[Index].DLName
						@"Filename="$Files[Index].Filename);
				}
			}
			break;
		}
	}
	// File could not be loaded from cache or hash was invalid. so, download it
	if (bRequiresDownload)
	{
		// File was invalid so delete it
		TitleFileCacheInterface.DeleteTitleFile(FileName);
		// Start the download
		if (TitleFileInterface != None &&
			TitleFileInterface.ReadTitleFile(FileName))
		{
			Files[Index].ReadState = OERS_InProgress;
		}
		else
		{
			Files[Index].ReadState = OERS_Failed;
		}
	}
	else
	{
		// Notify that load completed
		TriggerDownloadCompleteDelegates(true,FileName);
	}
}

/**
 * Delegate fired when a file save to the local cache is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
function OnFileCacheSaveComplete(bool bWasSuccessful,string FileName)
{
	local string LogicalName;

	LogicalName = TitleFileCacheInterface.GetTitleFileLogicalName(FileName);
	// We are now pushing .bin files through this patcher system as well.
	// In that case we don't want to process or remove the file from the cache yet.
	// The game can register another handler that will deal with them.
	if (InStr(LogicalName, ".bin",false,true) == INDEX_NONE)
	{
		// clear the memory for the entry that was saved
		TitleFileCacheInterface.ClearCachedFile(FileName);
	}
}

/**
 * Triggers list of delegates whenever a file has been loaded
 */
function TriggerDownloadCompleteDelegates(bool bSuccess,string FileName)
{
	local int Index;
	local delegate<OnReadTitleFileComplete> OnReadTitleFileComplete;

	// Call delegates for file completion
	for (Index=0; Index < ReadTitleFileCompleteDelegates.Length; Index++)
	{
		if (ReadTitleFileCompleteDelegates[Index] != None)
		{
			OnReadTitleFileComplete = ReadTitleFileCompleteDelegates[Index];
			OnReadTitleFileComplete(bSuccess,FileName);
		}
	}
	// See if all files are done
	CheckForAllFilesComplete();
}

/**
 * Triggers the final delegate if all files have been processed
 */
function CheckForAllFilesComplete()
{
	local int Index;
	local bool bAllFilesComplete;

	bAllFilesComplete = true;
	for (Index = 0; Index < Files.Length; Index++)
	{
		if (Files[Index].ReadState == OERS_NotStarted ||
			Files[Index].ReadState == OERS_InProgress)
		{
			bAllFilesComplete = false;
		}
	}
	if (bAllFilesComplete)
	{
		OnAllTitleFilesCompleted();
	}
}

/**
 * Takes the data, merges with the INI/Loc system, and then reloads the config for the
 * affected objects
 *
 * @param FileName the name of the file being merged
 * @param bIsUnicode whether the file should be treated as unicode or not
 * @param FileData the file data to merge with the config cache
 */
native function ProcessIniLocFile(string FileName,bool bIsUnicode,const out array<byte> FileData);

/**
 * Adds a loc/ini file to download
 *
 * @param FileName the file to download
 */
function AddFileToDownload(string FileName)
{
	local int FileIndex;

	FileIndex = Files.Find('FileName',FileName);
	// Don't add more than once
	if (FileIndex == INDEX_NONE)
	{
		// Add a new entry which will default to not started
		FileIndex = Files.Length;
		Files.Length = FileIndex + 1;
		Files[FileIndex].FileName = FileName;
		Files[FileIndex].DLName = FileName;
		// Any file that is not INT or INI should be Unicode
		Files[FileIndex].bIsUnicode = InStr(FileName,".ini",,true) == INDEX_NONE && InStr(FileName,".int",,true) == INDEX_NONE;
	}
	else
	{
		Files[FileIndex].ReadState = OERS_NotStarted;
	}
	// Kick off the download
	DownloadFiles();
}

/**
 * Adds the specified delegate to the registered downloader. Since the file read can come from
 * different objects, this method hides that detail, but still lets callers get notifications
 *
 * @param ReadTitleFileCompleteDelegate the delegate to set
 */
function AddReadFileDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate)
{
	// Add the delegate if not None and not found
	if (ReadTitleFileCompleteDelegate != None &&
		ReadTitleFileCompleteDelegates.Find(ReadTitleFileCompleteDelegate) == INDEX_NONE)
	{
		ReadTitleFileCompleteDelegates.AddItem(ReadTitleFileCompleteDelegate);
	}
}

/**
 * Clears the specified delegate from any registered downloaders
 *
 * @param ReadTitleFileCompleteDelegate the delegate to remove from the downloader
 */
function ClearReadFileDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = ReadTitleFileCompleteDelegates.Find(ReadTitleFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ReadTitleFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Tells any subclasses to clear their cached file data
 */
function ClearCachedFiles()
{
	local int Index;

	// Iterate through files trying to download them
	for (Index = 0; Index < Files.Length; Index++)
	{
		// Reset their status
		Files[Index].ReadState = OERS_NotStarted;
	}
	if (TitleFileInterface != None)
	{
		// memory copy of downloaded files
		TitleFileInterface.ClearDownloadedFiles();
	}
	if (TitleFileCacheInterface != None)
	{
		// memory copy of cached files
		TitleFileCacheInterface.ClearCachedFiles();
	}
}

/**
 * Gets the proper language extension for the loc file
 *
 * @param FileName the file name being modified
 *
 * @return the modified file name for this language setting
 */
native function string UpdateLocFileName(string FileName);
