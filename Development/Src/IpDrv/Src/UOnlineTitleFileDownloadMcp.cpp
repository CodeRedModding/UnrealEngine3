/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#include "HTTPDownload.h"

IMPLEMENT_CLASS(UOnlineTitleFileDownloadBase);
IMPLEMENT_CLASS(UOnlineTitleFileDownloadMcp);

/**
 * Ticks any http requests that are in flight
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineTitleFileDownloadMcp::Tick(FLOAT DeltaTime)
{
	if (DownloadCount)
	{
		// Loop through ticking every outstanding HTTP request
		for (INT FileIndex = 0; FileIndex < TitleFiles.Num(); FileIndex++)
		{
			FTitleFileMcp& TitleFile = TitleFiles(FileIndex);
			if (TitleFile.HttpDownloader != NULL)
			{
				// Tick the task and check for timeout
				TitleFile.HttpDownloader->Tick(DeltaTime);
				// See if we are done
				if (TitleFile.HttpDownloader->GetHttpState() == HTTP_Closed)
				{
					TitleFile.HttpDownloader->GetBinaryData(TitleFile.Data);
					TitleFile.AsyncState = OERS_Done;
					delete TitleFile.HttpDownloader;
					TitleFile.HttpDownloader = NULL;
					DownloadCount--;
				}
				// Or are in error
				else if (TitleFile.HttpDownloader->GetHttpState() == HTTP_Error)
				{
					// Failed zero everything
					TitleFile.AsyncState = OERS_Failed;
					TitleFile.Data.Empty();
					delete TitleFile.HttpDownloader;
					TitleFile.HttpDownloader = NULL;
					DownloadCount--;
				}
				// Trigger the delegate for this one if done
				if (TitleFile.AsyncState != OERS_InProgress)
				{
					TriggerDelegates(&TitleFile);
				}
			}
		}
	}
}

/**
 * Starts an asynchronous read of the specified file from the network platform's
 * title specific file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineTitleFileDownloadMcp::ReadTitleFile(const FString& FileToRead)
{
	DWORD Result = E_FAIL;
	if (FileToRead.Len())
	{
		// See if we have added the file to be processed
		FTitleFileMcp* TitleFile = GetTitleFile(FileToRead);
		if (TitleFile)
		{
			// Determine if it's done, in error, or in progress
			// and handle the delegate based off of that
			if (TitleFile->AsyncState == OERS_Done)
			{
				Result = ERROR_SUCCESS;
			}
			// If it hasn't started or is in progress, then mark as pending
			else if (TitleFile->AsyncState != OERS_Failed)
			{
				Result = ERROR_IO_PENDING;
			}
		}
		else
		{
			// Add this file to the list to process
			INT AddIndex = TitleFiles.AddZeroed();
			TitleFile = &TitleFiles(AddIndex);
			TitleFile->Filename = FileToRead;
			// Attempt to kick off the download
			FResolveInfo* ResolveInfo = NULL;
			// Build an url from the string
			FURL Url(NULL,*GetUrlForFile(FileToRead),TRAVEL_Absolute);
			// See if we need to resolve this string
			UBOOL bIsValidIp = FInternetIpAddr::IsValidIp(*Url.Host);
			if (bIsValidIp == FALSE)
			{
				// Allocate a platform specific resolver and pass that in
				ResolveInfo = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*Url.Host));
			}
			const FString GetParams = BuildURLParameters(FileToRead);
			// Create the new downloader object
			TitleFile->HttpDownloader = new FHttpDownloadBinary(TimeOut,GetParams,ResolveInfo);
			// This is the one to start downloading
			TitleFile->HttpDownloader->DownloadUrl(Url);
			TitleFile->AsyncState = OERS_InProgress;
			DownloadCount++;
			Result = ERROR_IO_PENDING;
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("ReadTitleFile() failed due to empty filename"));
	}
	if (Result != ERROR_IO_PENDING)
	{
		TriggerDelegates(GetTitleFile(FileToRead));
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
UBOOL UOnlineTitleFileDownloadMcp::GetTitleFileContents(const FString& FileName,TArray<BYTE>& FileContents)
{
	// Search for the specified file and return the raw data
	FTitleFileMcp* TitleFile = GetTitleFile(FileName);
	if (TitleFile)
	{
		FileContents = TitleFile->Data;
		return TRUE;
	}
	return FALSE;
}

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
UBOOL UOnlineTitleFileDownloadMcp::ClearDownloadedFiles(void)
{
	for (INT Index = 0; Index < TitleFiles.Num(); Index++)
	{
		FTitleFileMcp& TitleFile = TitleFiles(Index);
		// If there is an async task outstanding, fail to empty
		if (TitleFile.AsyncState == OERS_InProgress)
		{
			return FALSE;
		}
		delete TitleFile.HttpDownloader;
		TitleFile.HttpDownloader = NULL;
	}
	// No async files being handled, so empty them all
	TitleFiles.Empty();
	return TRUE;
}

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
UBOOL UOnlineTitleFileDownloadMcp::ClearDownloadedFile(const FString& FileName)
{
	INT FoundIndex = INDEX_NONE;
	// Search for the file
	for (INT Index = 0; Index < TitleFiles.Num(); Index++)
	{
		FTitleFileMcp& TitleFile = TitleFiles(Index);
		// If there is an async task outstanding on this file, fail to empty
		if (TitleFile.Filename == FileName)
		{
			if (TitleFile.AsyncState == OERS_InProgress)
			{
				return FALSE;
			}
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
 * Fires the delegates so the caller knows the file download is complete
 *
 * @param TitleFile the information for the file that was downloaded
 */
void UOnlineTitleFileDownloadMcp::TriggerDelegates(const FTitleFile* TitleFile)
{
	if (TitleFile)
	{
		OnlineTitleFileDownloadBase_eventOnReadTitleFileComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = TitleFile->AsyncState == OERS_Done ? FIRST_BITFIELD : 0;
		Parms.Filename = TitleFile->Filename;
		TriggerOnlineDelegates(this,ReadTitleFileCompleteDelegates,&Parms);
	}
}

/**
 * Searches the filename to URL mapping table for the specified filename
 *
 * @param FileName the file to search the table for
 *
 * @param the URL to use to request the file or BaseURL if no special mapping is present
 */
FString UOnlineTitleFileDownloadBase::GetUrlForFile(const FString& FileName)
{
	FName File(*FileName);
	// Search through the array for the file name
	for (INT Index = 0; Index < FilesToUrls.Num(); Index++)
	{
		const FFileNameToURLMapping& UrlMapping = FilesToUrls(Index);
		if (UrlMapping.Filename == File)
		{
			return UrlMapping.UrlMapping.ToString();
		}
	}
	return BaseUrl + RequestFileURL;
}

#endif
