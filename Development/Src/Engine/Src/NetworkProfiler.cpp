/*=============================================================================
	NetworkProfiler.cpp: server network profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if USE_NETWORK_PROFILER

/**
 * Whether to track the raw network data or not.
 */
#define NETWORK_PROFILER_TRACK_RAW_NETWORK_DATA		0

#include "UnNet.h"
#if PS3
	#include <common/include/sdk_version.h>
	#include <sys/ansi.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <netex/errno.h>
	#include <netex/net.h>
	#include <sys/time.h>
	#include <sys/select.h>
	#include <cell/sysmodule.h>
	#include <np.h>
#endif
#if PLATFORM_MACOSX
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif
#include "UnSocket.h"
#include "NetworkProfiler.h"
#include "ProfilingHelpers.h"

/** Global network profiler instance. */
FNetworkProfiler GNetworkProfiler;

/** Magic value, determining that file is a network profiler file.				*/
#define NETWORK_PROFILER_MAGIC						0x1DBF348A
/** Version of memory profiler. Incremented on serialization changes.			*/
#define NETWORK_PROFILER_VERSION					3

enum ENetworkProfilingPayloadType
{
	NPTYPE_FrameMarker			= 0,	// Frame marker, signaling beginning of frame.	
	NPTYPE_SocketSendTo,				// FSocket::SendTo
	NPTYPE_SendBunch,					// UChannel::SendBunch
	NPTYPE_SendRPC,						// Sending RPC
	NPTYPE_ReplicateActor,				// Replicated object	
	NPTYPE_ReplicateProperty,			// Property being replicated.
	NPTYPE_EndOfStreamMarker,			// End of stream marker		
	NPTYPE_Event,						// Event
	NPTYPE_RawSocketData,				// Raw socket data being sent
};


/*=============================================================================
	Network profiler header.
=============================================================================*/

struct FNetworkProfilerHeader
{
	/** Magic to ensure we're opening the right file.	*/
	DWORD	Magic;
	/** Version number to detect version mismatches.	*/
	DWORD	Version;

	/** Offset in file for name table.					*/
	DWORD	NameTableOffset;
	/** Number of name table entries.					*/
	DWORD	NameTableEntries;

	/** Tag, set via -networkprofiler=TAG				*/
	FString Tag;
	/** Game name, e.g. Example							*/
	FString GameName;
	/** URL used to open/ browse to the map.			*/
	FString URL;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Header		Header to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FNetworkProfilerHeader Header )
	{
		check( Ar.IsSaving() );
		Ar	<< Header.Magic
			<< Header.Version
			<< Header.NameTableOffset
			<< Header.NameTableEntries;
		SerializeStringAsANSICharArray( Header.Tag, Ar, 255 );
		SerializeStringAsANSICharArray( Header.GameName, Ar, 255 );
		SerializeStringAsANSICharArray( Header.URL, Ar, 255 );
		return Ar;
	}
};


/*=============================================================================
	FMallocProfiler implementation.
=============================================================================*/

/**
 * Constructor, initializing member variables.
 */
FNetworkProfiler::FNetworkProfiler()
:	FileWriter(NULL)
,	bHasNoticeableNetworkTrafficOccured(FALSE)
,	bIsTrackingEnabled(FALSE)
{
}

/**
 * Returns index of passed in name into name array. If not found, adds it.
 *
 * @param	Name	Name to find index for
 * @return	Index of passed in name
 */
INT FNetworkProfiler::GetNameTableIndex( const FString& Name )
{
	// Index of name in name table.
	INT Index = INDEX_NONE;

	// Use index if found.
	INT* IndexPtr = NameToNameTableIndexMap.Find( Name );
	if( IndexPtr )
	{
		Index = *IndexPtr;
	}
	// Encountered new name, add to array and set index mapping.
	else
	{
		Index = NameArray.Num();
		new(NameArray)FString(Name);
		NameToNameTableIndexMap.Set(*Name,Index);
	}

	check(Index!=INDEX_NONE);
	return Index;
}

/**
 * Enables/ disables tracking. Emits a session changes if disabled.
 *
 * @param	bShouldEnableTracking	Whether tracking should be enabled or not
 */
void FNetworkProfiler::EnableTracking( UBOOL bShouldEnableTracking )
{
	if( bShouldEnableTracking )
	{
		debugf(TEXT("Network Profiler: ENABLED"));
	}

	// Flush existing session in progress if we're disabling tracking and it was enabled.
	if( bIsTrackingEnabled && !bShouldEnableTracking )
	{
		TrackSessionChange(FALSE,FURL());
	}
	// Important to not change bIsTrackingEnabled till after we flushed as it's used during flushing.
	bIsTrackingEnabled = bShouldEnableTracking;
}

/**
 * Marks the beginning of a frame.
 */
void FNetworkProfiler::TrackFrameBegin()
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		BYTE Type = NPTYPE_FrameMarker;
		(*FileWriter) << Type;
		FLOAT RelativeTime=  (FLOAT)(appSeconds() - GStartTime);
		(*FileWriter) << RelativeTime;
	}
}

/**
 * Tracks and RPC being sent.
 * 
 * @param	Actor		Actor RPC is being called on
 * @param	Function	Function being called
 * @param	NumBits		Number of bits serialized into bunch for this RPC
 */
void FNetworkProfiler::TrackSendRPC( const AActor* Actor, const UFunction* Function, WORD NumBits )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		BYTE Type = NPTYPE_SendRPC;
		(*FileWriter) << Type;
		INT ActorNameTableIndex = GetNameTableIndex( Actor->GetName() );
		(*FileWriter) << ActorNameTableIndex;
		INT FunctionNameTableIndex = GetNameTableIndex( Function->GetName() );
		(*FileWriter) << FunctionNameTableIndex;
		(*FileWriter) << NumBits;
	}
}

/**
 * Low level FSocket::Send information.
 *
 * @param	Socket					Socket data is being sent to
 * @param	Data					Data sent
 * @param	BytesSent				Bytes actually being sent
 */
void FNetworkProfiler::TrackSocketSend( const FSocket* Socket, const void* Data, WORD BytesSent )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		FInternetIpAddr DummyAddr;
		TrackSocketSendTo( Socket, Data, BytesSent, DummyAddr );
	}
}

/**
 * Low level FSocket::SendTo information.
 *
 * @param	Socket					Socket data is being sent to
 * @param	Data					Data sent
 * @param	BytesSent				Bytes actually being sent
 * @param	Destination				Destination address
 */
void FNetworkProfiler::TrackSocketSendTo( const FSocket* Socket, const void* Data, WORD BytesSent, const FInternetIpAddr& Destination )
{
	if( bIsTrackingEnabled )
	{
		// Low level socket code is called from multiple threads.
		SCOPE_LOCK_REF(CriticalSection);
		BYTE Type = NPTYPE_SocketSendTo;
		(*FileWriter) << Type;
		DWORD CurrentThreadID = appGetCurrentThreadId();
		(*FileWriter) << CurrentThreadID;
		INT NameTableIndex = GetNameTableIndex( Socket->GetDescription() );
		(*FileWriter) << NameTableIndex;
		(*FileWriter) << BytesSent;
		DWORD NetworkByteOrderIP;
		Destination.GetIp(NetworkByteOrderIP);
		(*FileWriter) << NetworkByteOrderIP;
#if NETWORK_PROFILER_TRACK_RAW_NETWORK_DATA
		Type = NPTYPE_RawSocketData;
		(*FileWriter) << Type;
		(*FileWriter) << BytesSent;
		check( FileWriter->IsSaving() );
		FileWriter->Serialize( const_cast<void*>(Data), BytesSent );
#endif
	}
}

/**
 * Mid level UChannel::SendBunch information.
 * 
 * @param	Channel		UChannel data is being sent to/ on
 * @param	OutBunch	FOutBunch being sent
 * @param	NumBits		Num bits to serialize for this bunch (not including merging)
 */
void FNetworkProfiler::TrackSendBunch( FOutBunch* OutBunch, WORD NumBits )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		BYTE Type = NPTYPE_SendBunch;
		(*FileWriter) << Type;
		WORD ChannelIndex = OutBunch->ChIndex;
		(*FileWriter) << ChannelIndex;
		BYTE ChannelType = OutBunch->ChType;
		(*FileWriter) << ChannelType;
		(*FileWriter) << NumBits;
	}
}

/**
 * Track actor being replicated.
 *
 * @param	Actor		Actor being replicated
 */
void FNetworkProfiler::TrackReplicateActor( const AActor* Actor )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		BYTE Type = NPTYPE_ReplicateActor;
		(*FileWriter) << Type;
		BYTE NetFlags = Actor->bNetDirty | (Actor->bNetInitial << 1) | (Actor->bNetOwner << 2);
		(*FileWriter) << NetFlags;
		INT NameTableIndex = GetNameTableIndex( Actor->GetName() );
		(*FileWriter) << NameTableIndex;
		// Use actor replication as indication whether session is worth keeping or not.
		bHasNoticeableNetworkTrafficOccured = TRUE;
	}
}

/**
 * Track property being replicated.
 *
 * @param	Property	Property being replicated
 * @param	NumBits		Number of bits used to replicate this property
 */
void FNetworkProfiler::TrackReplicateProperty( const UProperty* Property, WORD NumBits )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		BYTE Type = NPTYPE_ReplicateProperty;
		(*FileWriter) << Type;
		INT NameTableIndex = GetNameTableIndex( Property->GetName() );
		(*FileWriter) << NameTableIndex;
		(*FileWriter) << NumBits;
	}
}

/**
 * Track event occuring, like e.g. client join/ leave
 *
 * @param	EventName			Name of the event
 * @param	EventDescription	Additional description/ information for event
 */
void FNetworkProfiler::TrackEvent( const FString& EventName, const FString& EventDescription )
{
	if( bIsTrackingEnabled )
	{
		SCOPE_LOCK_REF(CriticalSection);
		BYTE Type = NPTYPE_Event;
		(*FileWriter) << Type;
		INT EventNameNameTableIndex = GetNameTableIndex( EventName );
		(*FileWriter) << EventNameNameTableIndex;
		INT EventDescriptionNameTableIndex = GetNameTableIndex( EventDescription );
		(*FileWriter) << EventDescriptionNameTableIndex ;
	}
}

/**
 * Called when the server first starts listening and on round changes or other
 * similar game events. We write to a dummy file that is renamed when the current
 * session ends.
 *
 * @param	bShouldContinueTracking		Whether to continue tracking
 * @param	InURL						URL used for new session
 */
void FNetworkProfiler::TrackSessionChange( UBOOL bShouldContinueTracking, const FURL& InURL )
{
#if ALLOW_DEBUG_FILES
	if( bIsTrackingEnabled )
	{
		// Session change might occur while other thread uses low level networking.
		SCOPE_LOCK_REF(CriticalSection);

		// End existing tracking session.
		if( FileWriter )
		{	
			if( bHasNoticeableNetworkTrafficOccured )
			{
				debugf(TEXT("Netork Profiler: Writing out session file for '%s'"), *CurrentURL.String());

				// Write end of stream marker.
				BYTE Type = NPTYPE_EndOfStreamMarker;
				(*FileWriter) << Type;

				// Real header, written at start of the file but overwritten out right before we close the file.
				FNetworkProfilerHeader Header;
				Header.Magic	= NETWORK_PROFILER_MAGIC;
				Header.Version	= NETWORK_PROFILER_VERSION;
				Header.Tag		= TEXT("");
				Parse(appCmdLine(), TEXT("NETWORKPROFILER="), Header.Tag);
				Header.GameName = appGetGameName();
				Header.URL		= CurrentURL.String();

				// Write out name table and update header with offset and count.
				Header.NameTableOffset	= FileWriter->Tell();
				Header.NameTableEntries	= NameArray.Num();
				for( INT NameIndex=0; NameIndex<NameArray.Num(); NameIndex++ )
				{
					SerializeStringAsANSICharArray( NameArray(NameIndex), *FileWriter );
				}

				// Seek to the beginning of the file and write out proper header.
				FileWriter->Seek( 0 );
				(*FileWriter) << Header;

				// Close file writer so we can rename the file to its final destination.
				FileWriter->Close();
			
				// Rename/ move file.
				const FString FinalFileName = appProfilingDir() + GGameName + TEXT("-") + appSystemTimeString() + TEXT(".nprof");
				UBOOL bWasMovedSuccessfully = GFileManager->Move( *FinalFileName, *TempFileName );

				// Send data to UnrealConsole to upload to DB.
				if( bWasMovedSuccessfully )
				{
					SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!NETWORK:"), *FinalFileName );
				}
			}
			else
			{
				FileWriter->Close();
			}

			// Clean up.
			delete FileWriter;
			FileWriter = NULL;
			bHasNoticeableNetworkTrafficOccured = FALSE;
		}

		if( bShouldContinueTracking )
		{
			// Start a new tracking session.
			check( GFileManager && GFileManager->IsInitialized() );
			check( FileWriter == NULL );

			// Use a dummy name for sessions in progress that is renamed at end.
			TempFileName = appProfilingDir() + TEXT("NetworkProfiling.tmp");

			// Create folder and file writer.
			GFileManager->MakeDirectory( *TempFileName.GetPath() );
			FileWriter = GFileManager->CreateFileWriter( *TempFileName, FILEWRITE_EvenIfReadOnly | FILEWRITE_Async );
			checkf( FileWriter );

			// Serialize dummy header, overwritten when session ends.
			FNetworkProfilerHeader DummyHeader;
			appMemzero( &DummyHeader, sizeof(DummyHeader) );
			(*FileWriter) << DummyHeader;
		}

		CurrentURL = InURL;
	}
#endif	//#if ALLOW_DEBUG_FILES
}

#endif	//#if USE_NETWORK_PROFILER

