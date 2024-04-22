/*=============================================================================
	McpMessages.cpp: Mcp Message Support Functions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnIpDrv.h"
#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UMcpServiceBase);
IMPLEMENT_CLASS(UMcpMessageBase);
IMPLEMENT_CLASS(UMcpMessageManager);


/** Number of bytes to place at the beginning of the compressed buffer. In this case we use the header to hold the uncompressed buffer size for use in future uncompression */
const INT COMPRESSED_HEADER_SIZE = 4;

/**
 * Sets up the compression task; adds it to the queue; and kicks it off
 */
UBOOL UMcpMessageManager::StartAsyncCompression(BYTE MessageCompressionType, const TArray<BYTE>& MessageContent,class UHttpRequestInterface* Request)
{
	DWORD CompressionFlags;
	switch(MessageCompressionType)
	{
		case MMCT_LZO:
			CompressionFlags = (COMPRESS_LZO | COMPRESS_BiasSpeed);
			break;
		case MMCT_ZLIB:
			CompressionFlags = (COMPRESS_ZLIB | COMPRESS_BiasSpeed);
			break;
		default:
			check("Unsupported Compression Type");
			return FALSE;
			break;
	}

	const INT UncompressedBufferSize = MessageContent.Num();
	if(UncompressedBufferSize > 0)
	{
		INT CompressedBufferSize = UncompressedBufferSize;

		FMcpCompressMessageRequest* CompressRequest = new(CompressMessageRequests) FMcpCompressMessageRequest(EC_EventParm);

		CompressRequest->SourceBuffer = MessageContent;
		CompressRequest->Request = Request;
		CompressRequest->OutCompressedSize = CompressedBufferSize+COMPRESSED_HEADER_SIZE;
		CompressRequest->DestBuffer.Empty(CompressedBufferSize+COMPRESSED_HEADER_SIZE);
		CompressRequest->DestBuffer.Add(CompressedBufferSize+COMPRESSED_HEADER_SIZE);
	
		CompressRequest->DestBuffer(0) = (UncompressedBufferSize & 0xFF000000) >> 24;
		CompressRequest->DestBuffer(1) = (UncompressedBufferSize & 0x00FF0000) >> 16;
		CompressRequest->DestBuffer(2) = (UncompressedBufferSize & 0x0000FF00) >> 8;
		CompressRequest->DestBuffer(3) = UncompressedBufferSize & 0xFF;
	
		CompressRequest->CompressionWorker = new FAsyncTask<FCompressAsyncWorker>(
			(ECompressionFlags)CompressionFlags,
			CompressRequest->SourceBuffer.GetTypedData(),
			UncompressedBufferSize,
			&CompressRequest->DestBuffer(COMPRESSED_HEADER_SIZE),
			&CompressRequest->OutCompressedSize);
		CompressRequest->CompressionWorker->StartBackgroundTask();
		return TRUE;
	}
	return FALSE;
}

/**
 * Sets up the uncompression task; adds it to the queue; and kicks it off
 * 
 * @param MessageId
 * @param MessageCompressionType
 * @param CompressedBuffer
 */
UBOOL UMcpMessageManager::StartAsyncUncompression(const FString& MessageId,BYTE MessageCompressionType,const TArray<BYTE>& CompressedBuffer)
{
	DWORD CompressionFlags;
	switch(MessageCompressionType)
	{
	case MMCT_LZO:
		CompressionFlags = (COMPRESS_LZO | COMPRESS_BiasSpeed);
		break;
	case MMCT_ZLIB:
		CompressionFlags = (COMPRESS_ZLIB | COMPRESS_BiasSpeed);
		break;
	default:
		CompressionFlags = (COMPRESS_None | COMPRESS_BiasSpeed);
		check("Unsupported Compression Type");
		break;
	}
	const INT CompressedBufferSize = CompressedBuffer.Num()-COMPRESSED_HEADER_SIZE;
	
	if(CompressedBufferSize  > 0 )
	{
		// Pull the size from the buffer
		INT UncompressedBufferSize = CompressedBuffer(0) << 24 |
			CompressedBuffer(1) << 16 |
			CompressedBuffer(2) << 8 |
			CompressedBuffer(3);

		FMcpUncompressMessageRequest* UncompressRequest = new(UncompressMessageRequests) FMcpUncompressMessageRequest(EC_EventParm);

		UncompressRequest->SourceBuffer = CompressedBuffer;
		UncompressRequest->OutUncompressedSize = UncompressedBufferSize;
		UncompressRequest->DestBuffer.Empty(UncompressedBufferSize);
		UncompressRequest->DestBuffer.Add(UncompressedBufferSize);
		UncompressRequest->MessageId = MessageId;

		UncompressRequest->UncompressionWorker = new FAsyncTask<FUncompressAsyncWorker>(
			(ECompressionFlags)CompressionFlags,
			&UncompressRequest->SourceBuffer(COMPRESSED_HEADER_SIZE),
			CompressedBufferSize,
			&UncompressRequest->DestBuffer(0),
			UncompressRequest->OutUncompressedSize);
		
		UncompressRequest->UncompressionWorker->StartBackgroundTask();
		return TRUE;
	}
	return FALSE;
}

/**
 * Ticks any compression requests that are in flight
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UMcpMessageManager::Tick(FLOAT DeltaTime)
{
	// Tick any objects in the list and remove if they are done
	for (INT Index = 0; Index < CompressMessageRequests.Num(); Index++)
	{
		FMcpCompressMessageRequest& CompressMessageRequest = CompressMessageRequests(Index);
		// See if we are ticking an async compression task
		if (CompressMessageRequest.CompressionWorker != NULL)
		{
			// If it's done, kick off the upload
			if (CompressMessageRequest.CompressionWorker->IsDone())
			{
				if(CompressMessageRequest.OutCompressedSize < CompressMessageRequest.DestBuffer.Num())
				{
					CompressMessageRequest.DestBuffer.Remove(
						CompressMessageRequest.OutCompressedSize+COMPRESSED_HEADER_SIZE, 
						CompressMessageRequest.DestBuffer.Num() - (CompressMessageRequest.OutCompressedSize+COMPRESSED_HEADER_SIZE) 
						);
				}

				CompressMessageRequest.Request->SetContent(CompressMessageRequest.DestBuffer);
				CompressMessageRequest.Request->ProcessRequest();

				// Empty the array and clean up the task
				CompressMessageRequest.DestBuffer.Empty();
				CompressMessageRequest.SourceBuffer.Empty();
				delete CompressMessageRequest.CompressionWorker;
				CompressMessageRequest.CompressionWorker = NULL;
				CompressMessageRequests.Remove(Index--);
			}
		}
		else
		{
			//If the compression worker is Null then the request shouldn't be in the queue so remove it.
			CompressMessageRequests.Remove(Index--);
		}
	}

	// Tick any objects in the list and remove if they are done
	for (INT Index = 0; Index < UncompressMessageRequests.Num(); Index++)
	{
		FMcpUncompressMessageRequest& UncompressMessageRequest = UncompressMessageRequests(Index);
		// See if we are ticking an async uncompression task
		if (UncompressMessageRequest.UncompressionWorker != NULL)
		{
			// If it's done, remove from queue and call the Finished event
			if (UncompressMessageRequest.UncompressionWorker->IsDone())
			{
				eventFinishedAsyncUncompression(TRUE, UncompressMessageRequest.DestBuffer, UncompressMessageRequest.MessageId );

				// Empty the array and clean up the task
				UncompressMessageRequest.DestBuffer.Empty();
				UncompressMessageRequest.SourceBuffer.Empty();
				delete UncompressMessageRequest.UncompressionWorker;
				UncompressMessageRequest.UncompressionWorker = NULL;
				UncompressMessageRequests.Remove(Index--);
			}
		}
		// We aren't compressing anymore, we're uploading
		else
		{
			// See if we are done and remove the item if so
			UncompressMessageRequests.Remove(Index--);
		}
	}
}
#endif // WITH_UE3_NETWORKING