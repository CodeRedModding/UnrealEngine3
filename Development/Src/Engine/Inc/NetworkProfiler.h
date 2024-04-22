/*=============================================================================
	NetworkProfiler.h: network profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef UNREAL_NETWORK_PROFILER_H
#define UNREAL_NETWORK_PROFILER_H

#if USE_NETWORK_PROFILER 

#define NETWORK_PROFILER(x) x

/*=============================================================================
	FNetworkProfiler
=============================================================================*/

/**
 * Network profiler, using serialized token emission like e.g. script and malloc profiler.
 */
class FNetworkProfiler
{
private:
	/** File writer used to serialize data. Safe to call before GFileManager has been initialized.	*/
	FArchive*								FileWriter;

	/** Critical section to sequence tracking.														*/
	FCriticalSection						CriticalSection;

	/** Mapping from name to index in name array.													*/
	TMap<FString,INT>						NameToNameTableIndexMap;
	/** Array of unique names.																		*/
	TArray<FString>							NameArray;

	/**	Temp file used for sessions in progress.													*/
	FFilename								TempFileName;

	/** Whether noticeable network traffic has occured in this session. Used to discard it.			*/
	UBOOL									bHasNoticeableNetworkTrafficOccured;
	/** Whether tracking is enabled.																*/
	UBOOL									bIsTrackingEnabled;	

	/** URL used for current tracking session.														*/
	FURL									CurrentURL;

	/**
	 * Returns index of passed in name into name array. If not found, adds it.
	 *
	 * @param	Name	Name to find index for
	 * @return	Index of passed in name
	 */
	INT GetNameTableIndex( const FString& Name );

public:
	/**
	 * Constructor, initializing members.
	 */
	FNetworkProfiler();

	/**
	 * Enables/ disables tracking. Emits a session changes if disabled.
	 *
	 * @param	bShouldEnableTracking	Whether tracking should be enabled or not
	 */
	void EnableTracking( UBOOL bShouldEnableTracking );

	/**
	 * Marks the beginning of a frame.
	 */
	void TrackFrameBegin();
	
	/**
	 * Tracks and RPC being sent.
	 * 
	 * @param	Actor		Actor RPC is being called on
	 * @param	Function	Function being called
	 * @param	NumBits		Number of bits serialized into bunch for this RPC
	 */
	void TrackSendRPC( const AActor* Actor, const UFunction* Function, WORD NumBits );
	
	/**
	 * Low level FSocket::Send information.
	 *
	 * @param	Socket					Socket data is being sent to
	 * @param	Data					Data sent
	 * @param	BytesSent				Bytes actually being sent
	 */
	void TrackSocketSend( const FSocket* Socket, const void* Data, WORD BytesSent );

	/**
	 * Low level FSocket::SendTo information.
	 *
 	 * @param	Socket					Socket data is being sent to
	 * @param	Data					Data sent
	 * @param	BytesSent				Bytes actually being sent
	 * @param	Destination				Destination address
	 */
	void TrackSocketSendTo( const FSocket* Socket, const void* Data, WORD BytesSent, const FInternetIpAddr& Destination );
	
	/**
	 * Mid level UChannel::SendBunch information.
	 * 
	 * @param	OutBunch	FOutBunch being sent
	 * @param	NumBits		Num bits to serialize for this bunch (not including merging)
	 */
	void TrackSendBunch( FOutBunch* OutBunch, WORD NumBits );
	
	/**
	 * Track actor being replicated.
	 *
	 * @param	Actor		Actor being replicated
	 */
	void TrackReplicateActor( const AActor* Actor );
	
	/**
	 * Track property being replicated.
	 *
	 * @param	Property	Property being replicated
	 * @param	NumBits		Number of bits used to replicate this property
	 */
	void TrackReplicateProperty( const UProperty* Property, WORD NumBits );

	/**
	 * Track event occuring, like e.g. client join/ leave
	 *
	 * @param	EventName			Name of the event
	 * @param	EventDescription	Additional description/ information for event
	 */
	void TrackEvent( const FString& EventName, const FString& EventDescription );

	/**
	 * Called when the server first starts listening and on round changes or other
	 * similar game events. We write to a dummy file that is renamed when the current
	 * session ends.
	 *
	 * @param	bShouldContinueTracking		Whether to continue tracking
	 * @param	InURL						URL used for new session
	 */
	void TrackSessionChange( UBOOL bShouldContinueTracking, const FURL& InURL );
};

/** Global network profiler instance. */
extern FNetworkProfiler GNetworkProfiler;

#else	// USE_NETWORK_PROFILER

#define NETWORK_PROFILER(x)

#endif

#endif	//#ifndef UNREAL_NETWORK_PROFILER_H
