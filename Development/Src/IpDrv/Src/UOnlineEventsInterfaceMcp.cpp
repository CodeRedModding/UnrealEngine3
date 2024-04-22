/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#include "HTTPDownload.h"

IMPLEMENT_CLASS(UOnlineEventsInterfaceMcp);

#define GAMEPLAY_EVENTS_XML_VER 3
const INT GAMEPLAY_EVENTS_BINARY_VER = 1003;

/** @note these must match the COM component used by MCP */
#define COMPRESSED_VER 1
#define UNCOMPRESSED_HEADER_SIZE (1 + 3)
#define COMPRESSED_HEADER_SIZE (1 + 3 + 4)
#define BINARY_STATS_FLAG 0x02
#define BYTE_SWAP_BINARY_UPLOADS !DEDICATED_SERVER

/**
 * Ticks any http requests that are in flight
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineEventsInterfaceMcp::Tick(FLOAT DeltaTime)
{
	// Tick any objects in the list and remove if they are done
	for (INT Index = 0; Index < MCPEventPostObjects.Num(); Index++)
	{
		FMCPEventPoster* MCPEventPoster = MCPEventPostObjects(Index);
		// See if we are ticking an async compression task
		if (MCPEventPoster->CompressionWorker != NULL)
		{
			// If it's done, kick off the upload
			if (MCPEventPoster->CompressionWorker->IsDone())
			{
				// Copy the payload into the object for sending
				MCPEventPoster->HttpPoster->CopyPayload(MCPEventPoster->CompressedBuffer.GetTypedData(),
					MCPEventPoster->OutCompressedSize + COMPRESSED_HEADER_SIZE);
				// Empty the array and clean up the task
				MCPEventPoster->CompressedBuffer.Empty();
				MCPEventPoster->SourceBuffer.Empty();
				delete MCPEventPoster->CompressionWorker;
				MCPEventPoster->CompressionWorker = NULL;
				// Start the download task
				MCPEventPoster->HttpPoster->DownloadUrl(MCPEventPoster->Url);
			}
		}
		// We aren't compressing anymore, we're uploading
		else
		{
			MCPEventPoster->HttpPoster->Tick(DeltaTime);
			// See if we are done and remove the item if so
			if (MCPEventPoster->HttpPoster->GetHttpState() == HTTP_Closed ||
				MCPEventPoster->HttpPoster->GetHttpState() == HTTP_Error)
			{
#if _DEBUG
				FString Response;
				MCPEventPoster->HttpPoster->GetString(Response);
				debugf(NAME_DevOnline,TEXT("Event upload response:\r\n%s"),*Response);
#endif
				delete MCPEventPoster->HttpPoster;
				delete MCPEventPoster;
				MCPEventPostObjects.Remove(Index);
				Index--;
			}
		}
	}
}

/**
 * Sends the profile data to the server for statistics aggregation
 *
 * @param Id the unique id of the player
 * @param Nickname the nickname of the player
 * @param ProfileSettings the profile object that is being sent
 *
 * @return true if the async task was started successfully, false otherwise
 */
UBOOL UOnlineEventsInterfaceMcp::UploadPlayerData(FUniqueNetId Id,const FString& Nickname,UOnlineProfileSettings* ProfileSettings,UOnlinePlayerStorage* PlayerStorage)
{
#if WITH_UE3_NETWORKING
	FString XmlPayload = FString::Printf(TEXT("<Player TitleId=\"%d\" UniqueId=\"%s\" Name=\"%s\" PlatformId=\"%d\" EngineVersion=\"%d\">\r\n"),
		appGetTitleId(),
		*FormatAsString(Id),
		*EscapeString(Nickname),
		(DWORD)appGetPlatformType(),
		GEngineVersion);
	// Append any hardware data last
	XmlPayload += BuildHardwareXmlData();
	// Append the profile data
	if (ProfileSettings)
	{
		// Build the XML string allowing per platform specialization
		XmlPayload += FString::Printf(TEXT("<Profile Version=\"%d\">\r\n"),ProfileSettings->VersionNumber);
		// Now add the profile data
		ToXml(XmlPayload,ProfileSettings,1);
		// Close the tag
		XmlPayload += TEXT("</Profile>\r\n");
	}
	// Append the player storage data
	if (PlayerStorage)
	{
		// Build the XML string allowing per platform specialization
		XmlPayload += FString::Printf(TEXT("<PlayerStorage Version=\"%d\">\r\n"),PlayerStorage->VersionNumber);
		// Now add the profile data
		ToXml(XmlPayload,PlayerStorage,1);
		// Close the tag
		XmlPayload += TEXT("</PlayerStorage>\r\n");
	}
	// Close the player block
	XmlPayload += TEXT("</Player>\r\n");
	// Now POST the data and have a response parsed
	return UploadPayload(EUT_ProfileData,XmlPayload,Id);
#endif
	return FALSE;
}

/**
 * Method for POST-ing text data
 *
 * @param UploadType the type of upload that is happening
 * @param Payload the data to send
 * @param NetId unique id of the player sending the data
 *
 * @return TRUE if the send started successfully, FALSE otherwise
 */
UBOOL UOnlineEventsInterfaceMcp::UploadPayload(BYTE UploadType,const FString& Payload,const FUniqueNetId NetId)
{
	TArray<BYTE> UncompressedBuffer;
	INT UncompressedBufferSize = Payload.Len();
	// Copy the data into the uncompressed buffer in ANSI char form
	UncompressedBuffer.Empty(UncompressedBufferSize);
	UncompressedBuffer.Add(UncompressedBufferSize);
	appMemcpy(UncompressedBuffer.GetTypedData(),(const BYTE*)TCHAR_TO_ANSI(*Payload),UncompressedBufferSize);
	return UploadFinalPayload(true,UploadType,UncompressedBuffer,NetId);
}

/**
 * Method for POST-ing binary data.
 *
 * @param UploadType the type of upload that is happening
 * @param Payload the data to send
 * @param NetId unique id of the player sending the data
 *
 * @return TRUE if the send started successfully, FALSE otherwise
 */
UBOOL UOnlineEventsInterfaceMcp::UploadBinaryPayload(BYTE UploadType,const TArray<BYTE>& UncompressedBuffer,const FUniqueNetId NetId)
{
	return UploadFinalPayload(false,UploadType,UncompressedBuffer,NetId);
}

/**
 * Common method for POST-ing a payload to an URL (determined by upload type)
 *
 * @param UploadType the type of upload that is happening
 * @param Payload the data to send
 * @param NetId unique id of the player sending the data
 *
 * @return TRUE if the send started successfully, FALSE otherwise
 */
UBOOL UOnlineEventsInterfaceMcp::UploadFinalPayload(UBOOL bWasText,BYTE UploadType,const TArray<BYTE>& UncompressedBuffer,const FUniqueNetId NetId)
{
	DWORD Result = E_FAIL;
	// Find the upload configuration
	FEventUploadConfig* UploadConfig = FindUploadConfig(UploadType);
	// Validate the entry was configured properly
	if (UploadConfig &&
		UploadConfig->UploadUrl.Len())
	{
		// Build an url from the string
		FURL Url(NULL,*UploadConfig->UploadUrl,TRAVEL_Absolute);
		FResolveInfo* ResolveInfo = NULL;
		// See if we need to resolve this string
		UBOOL bIsValidIp = FInternetIpAddr::IsValidIp(*Url.Host);
		if (bIsValidIp == FALSE)
		{
			// Allocate a platform specific resolver and pass that in
			ResolveInfo = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*Url.Host));
		}
		// Build the extra parameters to add to the URL
		const FString Parameters = BuildGenericURLParameters(NetId);
		// Create the book keeping struct
		FMCPEventPoster* MCPEventPoster = new FMCPEventPoster();
		// Create a new posting object
		MCPEventPoster->HttpPoster = new FHttpDownloadString(FALSE,
			UploadConfig->TimeOut,
			Parameters,
			ResolveInfo,
			HRT_Post);
#if !FINAL_RELEASE
		FString Ignored;
		const UBOOL bDumpHttpEnabled = Parse(appCmdLine(),TEXT("DUMPHTTP"),Ignored);
#else
		const UBOOL bDumpHttpEnabled = FALSE;
#endif
		// Determine whether to send as text or compressed
		if (UploadConfig->bUseCompression && !bDumpHttpEnabled)
		{
			// Copy the for deferred processing
			MCPEventPoster->Url = Url;
			// Copy the source array so that the background thread can operate on it
			MCPEventPoster->SourceBuffer = UncompressedBuffer;
			const INT UncompressedBufferSize = UncompressedBuffer.Num();
			INT CompressedBufferSize = UncompressedBufferSize + COMPRESSED_HEADER_SIZE;
			// Now size the buffer leaving space for the header
			MCPEventPoster->CompressedBuffer.Empty(CompressedBufferSize);
			MCPEventPoster->CompressedBuffer.Add(CompressedBufferSize);
			// Finally, write the header information into the buffer
			MCPEventPoster->CompressedBuffer(0) = 0x4D;	// M
			MCPEventPoster->CompressedBuffer(1) = 0x43;	// C
			MCPEventPoster->CompressedBuffer(2) = 0x50; // P
			MCPEventPoster->CompressedBuffer(3) = COMPRESSED_VER;
			if (!bWasText)
			{
				MCPEventPoster->CompressedBuffer(3) |= BINARY_STATS_FLAG;	// Turn on the Binary Data flag
			}
			MCPEventPoster->CompressedBuffer(4) = (UncompressedBufferSize & 0xFF000000) >> 24;
			MCPEventPoster->CompressedBuffer(5) = (UncompressedBufferSize & 0x00FF0000) >> 16;
			MCPEventPoster->CompressedBuffer(6) = (UncompressedBufferSize & 0x0000FF00) >> 8;
			MCPEventPoster->CompressedBuffer(7) = UncompressedBufferSize & 0xFF;
			MCPEventPoster->OutCompressedSize = UncompressedBufferSize;
			// Allocate the async compression task
			MCPEventPoster->CompressionWorker = new FAsyncTask<FCompressAsyncWorker>(ECompressionFlags(COMPRESS_ZLIB | COMPRESS_BiasSpeed), MCPEventPoster->SourceBuffer.GetTypedData(),
				UncompressedBufferSize,
				&MCPEventPoster->CompressedBuffer(COMPRESSED_HEADER_SIZE),
				&MCPEventPoster->OutCompressedSize);
			// Now start it on another thread
			MCPEventPoster->CompressionWorker->StartBackgroundTask();
		}
		else
		{
			// Just pass the payload straight through
			// no longer requiring MCP header and saving a full extra buffers worth of memory
			const INT UncompressedBufferSize = UncompressedBuffer.Num();
			MCPEventPoster->HttpPoster->CopyPayload(UncompressedBuffer.GetTypedData(),UncompressedBufferSize);
			// Start the download task
			MCPEventPoster->HttpPoster->DownloadUrl(Url);
		}
		// Add it so it can be ticked
		MCPEventPostObjects.AddItem(MCPEventPoster);
		Result = ERROR_IO_PENDING;
	}
	else
	{
		debugf(NAME_Error,TEXT("No URL configured for upload type (%d)"),(DWORD)UploadType);
	}
	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}


/**
 * Sends gameplay event data to MCP
 *
 * @param UniqueId the player that is sending the stats
 * @param Payload the stats data to upload
 *
 * @return true if the async send started ok, false otherwise
 */
UBOOL UOnlineEventsInterfaceMcp::UploadGameplayEventsData(FUniqueNetId UniqueId,const TArray<BYTE>& Payload)
{
	return UploadBinaryPayload(EUT_GenericStats,Payload,UniqueId);
}

/**
 * Filters out escape characters that can't be sent to MCP via XML and
 * replaces them with the XML allowed sequences
 *
 * @param Source the source string to modify
 *
 * @return a new string with the data escaped
 */
FString UOnlineEventsInterfaceMcp::EscapeString(const FString& Source)
{
	FString Return = Source.Replace(TEXT("\""),TEXT("&quot;"));
	Return = Return.Replace(TEXT("<"),TEXT("&lt;"));
	Return = Return.Replace(TEXT(">"),TEXT("&gt;"));
	Return = Return.Replace(TEXT("&"),TEXT("&amp;"));
	return Return.Replace(TEXT("'"),TEXT("&apos;"));
}

/**
 * Sends the network backend the playlist population for this host
 *
 * @param PlaylistId the playlist we are updating the population for
 * @param NumPlayers the number of players on this host in this playlist
 *
 * @return true if the async send started ok, false otherwise
 */
UBOOL UOnlineEventsInterfaceMcp::UpdatePlaylistPopulation(INT PlaylistId,INT NumPlayers)
{
	DWORD Result = E_FAIL;
	// Find the upload configuration
	FEventUploadConfig* UploadConfig = FindUploadConfig(EUT_PlaylistPopulation);
	// Validate the entry was configured properly
	if (UploadConfig &&
		UploadConfig->UploadUrl.Len())
	{
		// Build an url from the string
		FURL Url(NULL,*UploadConfig->UploadUrl,TRAVEL_Absolute);
		FResolveInfo* ResolveInfo = NULL;
		// See if we need to resolve this string
		UBOOL bIsValidIp = FInternetIpAddr::IsValidIp(*Url.Host);
		if (bIsValidIp == FALSE)
		{
			// Allocate a platform specific resolver and pass that in
			ResolveInfo = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*Url.Host));
		}
		// Build the extra parameters to add to the URL
		const FString Parameters = BuildPlaylistPopulationURLParameters(PlaylistId,NumPlayers);
		// Create the book keeping struct
		FMCPEventPoster* MCPEventPoster = new FMCPEventPoster();
		// Create a new posting object
		MCPEventPoster->HttpPoster = new FHttpDownloadString(FALSE,
			UploadConfig->TimeOut,
			Parameters,
			ResolveInfo,
			HRT_Post);
		// Post the data to MCP
		MCPEventPoster->HttpPoster->DownloadUrl(Url);
		// Add it so it can be ticked
		MCPEventPostObjects.AddItem(MCPEventPoster);
		Result = ERROR_IO_PENDING;
	}
	else
	{
		debugf(NAME_Error,TEXT("No URL configured for upload type (%d)"),(DWORD)EUT_PlaylistPopulation);
	}
	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

/**
 * Builds the URL of additional parameters used when posting playlist population data
 *
 * @param PlaylistId the playlist id being reported
 * @param NumPlayers the number of players on the host
 *
 * @return the URL to use with all of the per platform extras
 */
FString UOnlineEventsInterfaceMcp::BuildPlaylistPopulationURLParameters(INT PlaylistId,INT NumPlayers)
{
	return FString::Printf(TEXT("PlaylistId=%d&NumPlayers=%d&TitleID=%d&PlatformID=%d"),
		PlaylistId,
		NumPlayers,
		appGetTitleId(),
		(DWORD)appGetPlatformType());
}

/**
 * Builds the URL of additional parameters used when posting data
 *
 * @param NetId the unique id of the player sending their data
 *
 * @return the URL to use with all of the per platform extras
 */
FString UOnlineEventsInterfaceMcp::BuildGenericURLParameters(const FUniqueNetId NetId)
{
	return FString::Printf(TEXT("UniqueId=%s&TitleID=%d&PlatformID=%d"),
		*FormatAsString(NetId),
		appGetTitleId(),
		(DWORD)appGetPlatformType());
}

/**
 * Sends matchmaking stats data to MCP
 *
 * @param UniqueId the unique id for the player
 * @param MMStats object that contains aggregated matchmaking stats data
 *
 * @return true if the async send started ok, false otherwise
 */
UBOOL UOnlineEventsInterfaceMcp::UploadMatchmakingStats(struct FUniqueNetId UniqueId,class UOnlineMatchmakingStats* MMStats)
{
	UBOOL bResult = FALSE;
	if (MMStats != NULL)
	{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		const UBOOL bIndent = TRUE;
#else
		const UBOOL bIndent = FALSE;
#endif
		// Platform specific ids
		FString XmlPlatformStr(
			FString::Printf(TEXT("TitleId=\"%d\" PlatformId=\"%d\" %s"),
			appGetTitleId(),
			(DWORD)appGetPlatformType(),
			*BuildPlatformXmlData())
			);
		// Build XML string from matchmaking stats
		FString XmlPayload;
		MMStats->ToXML(
			XmlPayload,
			UniqueId,
			XmlPlatformStr,
			bIndent);
		// Now POST the data and have a response parsed
		bResult = UploadPayload(EUT_MatchmakingData,XmlPayload,UniqueId);		
	}
	return bResult;
}

#endif