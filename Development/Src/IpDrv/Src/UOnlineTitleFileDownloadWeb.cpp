/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UOnlineTitleFileDownloadWeb);

/** Number of bytes to place at the beginning of the compressed buffer. In this case we use the header to hold the uncompressed buffer size for use in future uncompression */
const INT FILE_COMPRESSED_HEADER_SIZE = 4;

/**
 * Ticks any http requests that are in flight
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineTitleFileDownloadWeb::Tick(FLOAT DeltaTime)
{
	//@todo sz - handle timeouts
}

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
UBOOL UOnlineTitleFileDownloadWeb::GetTitleFileContents(const FString& FileName,TArray<BYTE>& FileContents)
{
	// Search for the specified file and return the raw data
	FTitleFileWeb* TitleFile = GetTitleFile(FileName);
	UBOOL bWasSuccessful = FALSE;
	if (TitleFile)
	{
		if (TitleFile->Data.Num() > 0)
		{
			if(TitleFile->FileCompressionType == MFCT_ZLIB )
			{
				if(UncompressTitleFileContents(TitleFile->FileCompressionType, TitleFile->Data, FileContents))
				{
					bWasSuccessful = TRUE;
				}	
			}
			else
			{
				FileContents = TitleFile->Data;
				bWasSuccessful = TRUE;
			}
		}
		else
		{
			FMemoryWriter Ar(FileContents);
			Ar << TitleFile->StringData;
		}
	} 
	return bWasSuccessful;
}

/**
 * Uncompresses file data into from the  specified buffer for the specified file
 *
 * @param FileCompressionType
 * @param CompressedBuffer
 * @param UncompressedBuffer
 *
 * @return true if the data was copied, false otherwise
 */
UBOOL UOnlineTitleFileDownloadWeb::UncompressTitleFileContents(BYTE FileCompressionType,const TArray<BYTE>& CompressedBuffer, TArray<BYTE>& UncompressedBuffer)
{
	DWORD CompressionFlags;
	switch(FileCompressionType)
	{
	case MFCT_ZLIB:
		CompressionFlags = (COMPRESS_ZLIB | COMPRESS_BiasSpeed);
		break;
	default:
		CompressionFlags = (COMPRESS_None | COMPRESS_BiasSpeed);
		check("Unsupported Compression Type");
		break;
	}

	UBOOL bWasSuccessful = FALSE;

	const INT CompressedBufferSize = CompressedBuffer.Num()-FILE_COMPRESSED_HEADER_SIZE;
	const INT CompressedBufferSize1 = CompressedBuffer.Num();
	const INT CompressedBufferSize2= FILE_COMPRESSED_HEADER_SIZE;

	if(CompressedBufferSize  > 0 )
	{
		// Pull the size from the buffer
		INT UncompressedBufferSize = CompressedBuffer(0) << 24 |
			CompressedBuffer(1) << 16 |
			CompressedBuffer(2) << 8 |
			CompressedBuffer(3);

		UncompressedBuffer.AddZeroed(UncompressedBufferSize);

		if(appUncompressMemory(
			(ECompressionFlags)CompressionFlags,
			&UncompressedBuffer(0),
			UncompressedBufferSize,
			(void *) &CompressedBuffer(FILE_COMPRESSED_HEADER_SIZE),
			CompressedBufferSize))
		{
			bWasSuccessful = TRUE;
		}
	}
	
	return bWasSuccessful;
}

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
UBOOL UOnlineTitleFileDownloadWeb::ClearDownloadedFiles(void)
{
	for (INT Index = 0; Index < TitleFiles.Num(); Index++)
	{
		FTitleFileWeb& TitleFile = TitleFiles(Index);
		// If there is an async task outstanding, fail to empty
		if (TitleFile.AsyncState == OERS_InProgress)
		{
			return FALSE;
		}
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
UBOOL UOnlineTitleFileDownloadWeb::ClearDownloadedFile(const FString& FileName)
{
	INT FoundIndex = INDEX_NONE;
	// Search for the file
	for (INT Index = 0; Index < TitleFiles.Num(); Index++)
	{
		FTitleFileWeb& TitleFile = TitleFiles(Index);
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
 * Runs the delegates registered on this interface for each file download request
 *
 * @param bSuccess true if the request was successful
 * @param FileRead name of the file that was read
 */
void UOnlineTitleFileDownloadWeb::TriggerDelegates(UBOOL bSuccess,const FString& FileRead)
{
	OnlineTitleFileDownloadBase_eventOnReadTitleFileComplete_Parms Parms(EC_EventParm);
	Parms.bWasSuccessful = bSuccess ? FIRST_BITFIELD : 0;
	Parms.Filename = FileRead;
	TriggerOnlineDelegates(this,ReadTitleFileCompleteDelegates,&Parms);
}

/**
 * Searches the list of files for the one that matches the filename
 *
 * @param FileName the file to search for
 *
 * @return the file details
 */
FTitleFileWeb* UOnlineTitleFileDownloadWeb::GetTitleFile(const FString& FileName)
{
	// Search for the specified file
	for (INT Index = 0; Index < TitleFiles.Num(); Index++)
	{
		FTitleFileWeb* TitleFile = &TitleFiles(Index);
		if (TitleFile &&
			TitleFile->Filename == FileName)
		{
			return TitleFile;
		}
	}
	return NULL;
}

#endif
