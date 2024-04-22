/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UTitleFileDownloadCache);

#define TITLE_FILE_DOWNLOAD_CACHE_COMPRESSION_TOKEN (UINT)0x77777777

/**
 * Starts an asynchronous read of the specified file from the local cache
 *
 * @param FileName the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UTitleFileDownloadCache::LoadTitleFile(const FString& Filename)
{
	DWORD Result = E_FAIL;

	FTitleFileCacheEntry* TitleFile = NULL;
	if (Filename.Len())
	{
		// check for existing entry
		TitleFile = GetTitleFile(Filename);
		if (TitleFile != NULL)
		{
			// last operation was a load
			if (TitleFile->FileOp == TitleFile_Load)
			{
				// previously completed a load successfully
				if (TitleFile->AsyncState == OERS_Done)
				{
					Result = ERROR_SUCCESS;
				}
			}
			// last operation was a save so already have the contents of the file in the cache
			else if (TitleFile->FileOp == TitleFile_Save)
			{
				Result = ERROR_SUCCESS;
			}
		}
		if (Result != ERROR_SUCCESS || Result != ERROR_IO_PENDING)
		{
			// Add new entry
			if (TitleFile == NULL)
			{
				TitleFile = &TitleFiles(TitleFiles.AddZeroed());
			}
			// keep track of filename since that is how the entry is found
			TitleFile->Filename = Filename;
			// mark as a load file operation
			TitleFile->FileOp = TitleFile_Load;
			// logical name is loaded from file
			TitleFile->LogicalName = TEXT("");
			// hash is calculated after file is loaded
			TitleFile->Hash = TEXT("");

			// build file path for cache
			FString CacheDir = GetCachePath();
			// full path based on given filename
			FString FilenameFull = CacheDir + Filename;
			// see if file exists and get its size
			INT FileSize = GFileManager->FileSize(*FilenameFull);
			//if (FileSize > 0)
			{
				// create file reader with full path
				FArchive* Ar = GFileManager->CreateFileReader(*FilenameFull);
				if (Ar != NULL)
				{					
					// logical name of the file contents stored in the payload
					*Ar << TitleFile->LogicalName;

					// backup the current position in the file
					INT Pos = Ar->Tell();

					// read in a UINT and see if this file was compressed
					UINT Temp = 0;
					*Ar << Temp;
					if (Temp == TITLE_FILE_DOWNLOAD_CACHE_COMPRESSION_TOKEN)
					{
						INT UncompressedSize = 0;
						INT CompressedSize = 0;
						// compressed size of data
						*Ar << CompressedSize;
						// uncompressed size of data
						*Ar << UncompressedSize;

						void * CompressedBuffer = appMalloc(CompressedSize);
						TitleFile->Data.SetNum(UncompressedSize);
						// rest of the file is the payload
						Ar->Serialize(CompressedBuffer, CompressedSize);
						appUncompressMemory(COMPRESS_ZLIB, TitleFile->Data.GetData(), UncompressedSize, CompressedBuffer, CompressedSize);
						appFree(CompressedBuffer);
					}
					else
					{
						// rewind to the position the file was at before testing for the compression token
						Ar->Seek(Pos);
						// rest of the file is the payload
						*Ar << TitleFile->Data;
					}

					// done reading
					if (Ar->Close())
					{
						Result = ERROR_SUCCESS;
					}
					delete Ar;
					// mark as finished
					TitleFile->AsyncState = Result == ERROR_SUCCESS ? OERS_Done : OERS_Failed;
				}				
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("LoadTitleFile() failed due to empty filename"));
	}
	if (Result == ERROR_SUCCESS)
	{
		// hash payload that was loaded (or previously saved)
		if (TitleFile->Data.Num() > 0 &&
			TitleFile->Hash.Len() == 0)
		{	
			BYTE Hash[20];
			FSHA1::HashBuffer(TitleFile->Data.GetData(),TitleFile->Data.Num(),Hash);
			// concatenate 20 bye SHA1 hash to string
			for (INT Idx=0; Idx < 20; Idx++)
			{
				TitleFile->Hash += FString::Printf(TEXT("%02x"),Hash[Idx]);
			}
		}
	}
	if (Result != ERROR_IO_PENDING)
	{
		TriggerDelegates(TitleFile,TitleFile_Load);
	}
	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
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
UBOOL UTitleFileDownloadCache::SaveTitleFile(const FString& Filename,const FString& LogicalName,const TArray<BYTE>& FileContents)
{
	DWORD Result = E_FAIL;

	FTitleFileCacheEntry* TitleFile = NULL;
	if (Filename.Len())
	{
		// check for existing entry
		TitleFile = GetTitleFile(Filename);
		if (TitleFile != NULL)
		{
			// last operation was a save
			if (TitleFile->FileOp == TitleFile_Save)
			{
				// previously completed a save successfully
				if (TitleFile->AsyncState == OERS_Done)
				{
					Result = ERROR_SUCCESS;
				}
				// save is already in progress so wait for it to finish
				if (TitleFile->AsyncState != OERS_Failed)
				{
					Result = ERROR_IO_PENDING;
				}
			}
		}
		if (Result != ERROR_SUCCESS || Result != ERROR_IO_PENDING)
		{
			// Add new entry
			if (TitleFile == NULL)
			{
				TitleFile = &TitleFiles(TitleFiles.AddZeroed());
			}
			// keep track of filename since that is how the entry is found
			TitleFile->Filename = Filename;
			// mark as a save file operation
			TitleFile->FileOp = TitleFile_Save;
			// keep track of logical name
			TitleFile->LogicalName = LogicalName;
			// copy data
			TitleFile->Data = FileContents;
			// no hash calculated on save
			TitleFile->Hash = TEXT("");
			// delete any previous archive
			delete TitleFile->Ar;

			// build file path to cache
			FString CacheDir = GetCachePath();
			// full path based on given filename
			FString FilenameFull = CacheDir + Filename;
			// create file writer
			TitleFile->Ar = GFileManager->CreateFileWriter(*FilenameFull,FILEWRITE_Async);
			if (TitleFile->Ar != NULL)
			{
				// logical name of the file contents stored in the payload
				*TitleFile->Ar << TitleFile->LogicalName;

#if IPHONE
				// write out magic number to indicate data payload is compressed
				UINT Token = TITLE_FILE_DOWNLOAD_CACHE_COMPRESSION_TOKEN;
				*TitleFile->Ar << Token;

				INT UncompressedSize = TitleFile->Data.Num();
				INT CompressedSize = UncompressedSize;
				// *2 here to ensure the buffer is large enough to compress into, even in the case where the compressed
				// data is larger than the uncompressed data
				void * CompressedBuffer = appMalloc(TitleFile->Data.Num() * 2); 
				appCompressMemory(COMPRESS_ZLIB, CompressedBuffer, CompressedSize, TitleFile->Data.GetData(), TitleFile->Data.Num());
				check(CompressedSize < (UncompressedSize * 2));
				// compressed size of data
				*TitleFile->Ar << CompressedSize;
				// uncompressed size of data
				*TitleFile->Ar << UncompressedSize;
				// rest of the file is the payload
				TitleFile->Ar->Serialize(CompressedBuffer, CompressedSize);
				appFree(CompressedBuffer);
#else
				// rest of the file is the payload
				*TitleFile->Ar << TitleFile->Data;
#endif

				// kick off the async file op
				TitleFile->Ar->Close();
				// mark as in progress
				TitleFile->AsyncState = OERS_InProgress;
				Result = ERROR_IO_PENDING;
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("SaveTitleFile() failed due to empty filename"));
	}
	if (Result != ERROR_IO_PENDING)
	{
		TriggerDelegates(TitleFile,TitleFile_Save);
	}
	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
UBOOL UTitleFileDownloadCache::GetTitleFileContents(const FString& FileName,TArray<BYTE>& FileContents)
{
	// Search for the specified file and return the raw data
	FTitleFileCacheEntry* TitleFile = GetTitleFile(FileName);
	if (TitleFile != NULL)
	{
		FileContents = TitleFile->Data;
		return TRUE;
	}
	return FALSE;
}

/**
 * Determines the async state of the tile file read operation
 *
 * @param FileName the name of the file to check on
 *
 * @return the async state of the file read
 */
BYTE UTitleFileDownloadCache::GetTitleFileState(const FString& FileName)
{
	// Search for the specified file and return current async state
	FTitleFileCacheEntry* TitleFile = GetTitleFile(FileName);
	if (TitleFile != NULL)
	{
		return TitleFile->AsyncState;
	}
	return OERS_NotStarted;
}

/**
 * Determines the hash of the tile file that was read
 *
 * @param FileName the name of the file to check on
 *
 * @return the hash string for the file
 */
FString UTitleFileDownloadCache::GetTitleFileHash(const FString& FileName)
{
	FString Hash;
	// Search for the specified file and return its hash 
	FTitleFileCacheEntry* TitleFile = GetTitleFile(FileName);
	if (TitleFile != NULL)
	{
		Hash = TitleFile->Hash;
	}
	return Hash;
}

/**
 * Determines the hash of the tile file that was read
 *
 * @param FileName the name of the file to check on
 *
 * @return the logical name of the for the given physical filename
 */
FString UTitleFileDownloadCache::GetTitleFileLogicalName(const FString& FileName)
{
	FString LogicalName;
	// Search for the specified file and its logical name
	FTitleFileCacheEntry* TitleFile = GetTitleFile(FileName);
	if (TitleFile != NULL)
	{
		LogicalName = TitleFile->LogicalName;
	}
	return LogicalName;
}

/**
 * Empties the set of cached files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
UBOOL UTitleFileDownloadCache::ClearCachedFiles()
{
	// If there is an async task outstanding, fail to empty
	for (INT Index = 0; Index < TitleFiles.Num(); Index++)
	{
		FTitleFileCacheEntry& TitleFile = TitleFiles(Index);
		if (TitleFile.AsyncState == OERS_InProgress)
		{
			return FALSE;
		}
		delete TitleFile.Ar;
	}
	// No async files being handled, so empty them all
	TitleFiles.Empty();
	return TRUE;
}

/**
 * Empties the cached data for this file if it is not being loaded/saved currently
 *
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be cleared, false if it could not
 */
UBOOL UTitleFileDownloadCache::ClearCachedFile(const FString& FileName)
{
	INT FoundIndex = INDEX_NONE;
	// Search for the file
	for (INT Index = 0; Index < TitleFiles.Num(); Index++)
	{
		FTitleFileCacheEntry& TitleFile = TitleFiles(Index);
		// If there is an async task outstanding on this file, fail to empty
		if (TitleFile.Filename == FileName)
		{
			if (TitleFile.AsyncState == OERS_InProgress)
			{
				return FALSE;
			}
			delete TitleFile.Ar;
			FoundIndex = Index;
			break;
		}
	}
	if (FoundIndex != INDEX_NONE)
	{
		TitleFiles.Remove(FoundIndex);
	}
	return TRUE;
}

/**
 * Deletes the set of title files from disc
 *
 * @param MaxAgeSeconds if > 0 then any files older than max seconds are deleted, if == 0 then all files are deleted
 * @return true if they could be deleted, false if they could not
 */
UBOOL UTitleFileDownloadCache::DeleteTitleFiles(FLOAT MaxAgeSeconds)
{
	UBOOL bResult = FALSE;
	TArray<FString> CacheFiles;
	// build file path
	FString CacheDir = GetCachePath();
	// all files in cache path
	GFileManager->FindFiles(CacheFiles, *(CacheDir + TEXT("*.*")),TRUE,FALSE);
	for (INT Idx=0; Idx < CacheFiles.Num(); Idx++)
	{
		// full path based on given filename
		FString FilenameFull = CacheDir + CacheFiles(Idx);
		UBOOL bShouldDelete = TRUE;
		if (MaxAgeSeconds > 0)
		{
			// determine if file should not be deleted since it is within the max age requirement
			DOUBLE FileAge = GFileManager->GetFileAgeSeconds(*FilenameFull);
			if (FileAge <= MaxAgeSeconds)
			{
				bShouldDelete = FALSE;
			}
		}
		if (bShouldDelete)
		{
			// make sure a file op is not in progress and clear cache entry
			if (ClearCachedFile(CacheFiles(Idx)))
			{
				debugf(NAME_DevOnline,TEXT("Deleting title file: %s"),*FilenameFull);
				// delete from disk
				bResult &= GFileManager->Delete(*FilenameFull,FALSE,TRUE);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Deleting title file failed. Couldn't remove cache entry for: %s"),*CacheFiles(Idx));
				bResult = FALSE; 
 			}
		}
	}
	return bResult;
}

/**
 * Deletes a single file from disc
 *
 * @param FileName the name of the file to delete
 *
 * @return true if it could be deleted, false if it could not
 */
UBOOL UTitleFileDownloadCache::DeleteTitleFile(const FString& FileName)
{
	// make sure a file op is not in progress and clear cache entry
	if (!ClearCachedFile(FileName))
	{
		return FALSE;
	}
	// build file path
	FString CacheDir = GetCachePath();
	// full path based on given filename
	FString FilenameFull = CacheDir + FileName;
	// delete from disk
	return GFileManager->Delete(*FilenameFull,FALSE,TRUE);
}

/**
 * Ticks any outstanding async tasks that need processing
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UTitleFileDownloadCache::Tick(FLOAT DeltaTime)
{
	// Loop through ticking every outstanding async file oop
	for (INT FileIndex = 0; FileIndex < TitleFiles.Num(); FileIndex++)
	{
		FTitleFileCacheEntry& TitleFile = TitleFiles(FileIndex);
		if (TitleFile.Ar != NULL &&
			TitleFile.AsyncState == OERS_InProgress)
		{
			// see if we are done saving
			UBOOL bError = FALSE;
			if (TitleFile.Ar->IsCloseComplete(bError))
			{
				TitleFile.AsyncState = bError ? OERS_Failed : OERS_Done;
				delete TitleFile.Ar;
				TitleFile.Ar = NULL;				
				TriggerDelegates(&TitleFile,(ETitleFileFileOp)TitleFile.FileOp);
			}
		}
	}
}

/**
 * Fires the delegates so the caller knows the file load/save is complete
 *
 * @param TitleFile the information for the file that was loaded/saved
 * @param FileOp read/write opeartion on the file to know which delegates to call
 */
void UTitleFileDownloadCache::TriggerDelegates(const FTitleFileCacheEntry* TitleFile,ETitleFileFileOp FileOp)
{
	if (TitleFile != NULL)
	{
		if (FileOp == TitleFile_Save)
		{
			TitleFileDownloadCache_eventOnSaveTitleFileComplete_Parms Parms(EC_EventParm);
			Parms.bWasSuccessful = TitleFile->AsyncState == OERS_Done ? FIRST_BITFIELD : 0;
			Parms.Filename = TitleFile->Filename;
			TriggerOnlineDelegates(this,SaveCompleteDelegates,&Parms);

		}
		else
		{
			TitleFileDownloadCache_eventOnLoadTitleFileComplete_Parms Parms(EC_EventParm);
			Parms.bWasSuccessful = TitleFile->AsyncState == OERS_Done || TitleFile->FileOp == TitleFile_Save ? FIRST_BITFIELD : 0;
			Parms.Filename = TitleFile->Filename;
			TriggerOnlineDelegates(this,LoadCompleteDelegates,&Parms);
		}
	}
}

/**
 * Searches the list of files for the one that matches the filename
 *
 * @param FileName the file to search for
 *
 * @return the file details
 */
FTitleFileCacheEntry* UTitleFileDownloadCache::GetTitleFile(const FString& FileName)
{
	// Search for the specified file
	for (INT Index = 0; Index < TitleFiles.Num(); Index++)
	{
		FTitleFileCacheEntry* TitleFile = &TitleFiles(Index);
		if (TitleFile &&
			TitleFile->Filename == FileName)
		{
			return TitleFile;
		}
	}
	return NULL;
}

/**
 * @return base path to all cached files
 */
FString UTitleFileDownloadCache::GetCachePath() const
{
	return appCacheDir() + TEXT("TitleCache") PATH_SEPARATOR;
}

#endif //WITH_UE3_NETWORKING
