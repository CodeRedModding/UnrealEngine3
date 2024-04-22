/*=============================================================================
	UnNetDrv.h: Unreal network driver base class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _UNNETDRV_H_
#define _UNNETDRV_H_

/*-----------------------------------------------------------------------------
	UPackageMapLevel.
-----------------------------------------------------------------------------*/

class UPackageMapLevel : public UPackageMap
{
	DECLARE_CLASS_INTRINSIC(UPackageMapLevel,UPackageMap,CLASS_Transient,Engine);

	UNetConnection* Connection;

	UBOOL CanSerializeObject( UObject* Obj );
	UBOOL SerializeObject( FArchive& Ar, UClass* Class, UObject*& Obj );

	UPackageMapLevel()
	{}
	UPackageMapLevel( UNetConnection* InConnection )
	: Connection( InConnection )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();
};

/**
 * Temporary hack to allow seekfree loading to work in the absence of linkers or a package map
 * replacement by serializing objects and names as strings. This is NOT meant for shipping due to
 * the obvious performance and bandwidth impact of the approach.
 */
class UPackageMapSeekFree : public UPackageMapLevel
{
	DECLARE_CLASS_INTRINSIC(UPackageMapSeekFree,UPackageMapLevel,CLASS_Transient,Engine);
	
	virtual UBOOL SerializeObject( FArchive& Ar, UClass* Class, UObject*& Obj );
	virtual UBOOL SerializeName( FArchive& Ar, FName& Name );

	virtual UBOOL SupportsPackage( UObject* InOuter );
	virtual UBOOL SupportsObject( UObject* Obj );

	// Dummies stubbed out.
	virtual void Compute() {}
	virtual void Copy( UPackageMap* Other ) {}
	virtual void AddNetPackages() {}
	virtual INT AddPackage(UPackage* Package) { return INDEX_NONE; }
	
	// Dummies that should never be called.
	virtual INT ObjectToIndex( UObject* Object )
	{
		appErrorf(TEXT("This should never have been called with UPackageMapSeekFree!"));
		return INDEX_NONE;
	}
	virtual UObject* IndexToObject( INT InIndex, UBOOL Load )
	{
		appErrorf(TEXT("This should never have been called with UPackageMapSeekFree!"));
		return NULL;
	}

	UPackageMapSeekFree()
	{}

	UPackageMapSeekFree( UNetConnection* InConnection )
	: UPackageMapLevel( InConnection )
	{}
};

#include "VoiceDataCommon.h"

/*-----------------------------------------------------------------------------
	UNetDriver.
-----------------------------------------------------------------------------*/

//
// Base class of a network driver attached to an active or pending level.
//
class UNetDriver : public USubsystem, public FNetObjectNotify
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(UNetDriver,USubsystem,CLASS_Transient|CLASS_Config|0,Engine)

	// Variables.
	TArray<UNetConnection*>		ClientConnections;
	UNetConnection*				ServerConnection;
	FNetworkNotify*				Notify;
	UPackageMap*				MasterMap;
	FLOAT						Time;
	FLOAT						ConnectionTimeout;
	FLOAT						InitialConnectTimeout;
	FLOAT						KeepAliveTime;
	FLOAT						RelevantTimeout;
	FLOAT						SpawnPrioritySeconds;
	FLOAT						ServerTravelPause;
	INT							MaxClientRate;
	INT							MaxInternetClientRate;
	INT							NetServerMaxTickRate;
	UBOOL						bClampListenServerTickRate;
	UBOOL						AllowDownloads;
	/** If TRUE then peer to peer connections are allowed */
	UBOOL						AllowPeerConnections;
	/** If TRUE then voice traffic is routed through peer connections when available */
	UBOOL						AllowPeerVoice;
	/** If TRUE then client connections are to other client peers */ 
	UBOOL						bIsPeer;
	UBOOL						ProfileStats;
	UProperty*					RoleProperty;
	UProperty*					RemoteRoleProperty;
	INT							SendCycles, RecvCycles;
	INT							MaxDownloadSize;
	TArray<FString>				DownloadManagers;
	/** Stats for network perf */

	DWORD						InBytesPerSecond;
	DWORD						OutBytesPerSecond;

	DWORD						InBytes;
	DWORD						OutBytes;
	DWORD						InPackets;
	DWORD						OutPackets;
	DWORD						InBunches;
	DWORD						OutBunches;
	DWORD						InPacketsLost;
	DWORD						OutPacketsLost;
	DWORD						InOutOfOrderPackets;
	DWORD						OutOutOfOrderPackets;
	/** Tracks the total number of voice packets sent */
	DWORD						VoicePacketsSent;
	/** Tracks the total number of voice bytes sent */
	DWORD						VoiceBytesSent;
	/** Tracks the total number of voice packets received */
	DWORD						VoicePacketsRecv;
	/** Tracks the total number of voice bytes received */
	DWORD						VoiceBytesRecv;
	/** Tracks the voice data percentage of in bound bytes */
	DWORD						VoiceInPercent;
	/** Tracks the voice data percentage of out bound bytes */
	DWORD						VoiceOutPercent;
	/** map of Actors to properties they need to be forced to replicate when the Actor is bNetInitial */
	TMap< AActor*, TArray<UProperty*> >	ForcedInitialReplicationMap;
	/** Time of last stat update */
	DOUBLE						StatUpdateTime;
	/** Interval between gathering stats */
	FLOAT						StatPeriod;
	/** Used to specify the class to use for connections */
	FStringNoInit				NetConnectionClassName;
	/** The loaded UClass of the net connection type to use */
	UClass*						NetConnectionClass;

	/** Used to determine if checking for standby cheats should occur */
	UBOOL						bIsStandbyCheckingEnabled;
	/** Used to determine whether we've already caught a cheat or not */
	UBOOL						bHasStandbyCheatTriggered;
	/** The amount of time without packets before triggering the cheat code */
	FLOAT						StandbyRxCheatTime;
	FLOAT						StandbyTxCheatTime;
	/** The point we think the host is cheating or shouldn't be hosting due to crappy network */
	INT							BadPingThreshold;
	/** The number of clients missing data before triggering the standby code */
	FLOAT						PercentMissingForRxStandby;
	FLOAT						PercentMissingForTxStandby;
	/** The number of clients with bad ping before triggering the standby code */
	FLOAT						PercentForBadPing;
	/** The amount of time to wait before checking a connection for standby issues */
	FLOAT						JoinInProgressStandbyWaitTime;

#if WITH_STEAMWORKS_SOCKETS
	/** This net driver is solely used to redirect incoming connections to a new address (e.g. from IP address to Steam address) */
	UBOOL						bRedirectDriver;
#endif

	/**
	 * Updates the various values in determining the standby cheat and
	 * causes the dialog to be shown/hidden as needed
	 */
	void UpdateStandbyCheatStatus(void);

#if DO_ENABLE_NET_TEST
	FPacketSimulationSettings	PacketSimulationSettings;
#endif

	// Constructors.
	UNetDriver();
	void StaticConstructor();
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	// UObject interface.
	void FinishDestroy();
	void Serialize( FArchive& Ar );

	/**
	 * Called by the game engine, when the game is exiting, to allow for special game-exit cleanup
	 */
	virtual void PreExit();

	/**
	 * Initializes the net connection class to use for new connections
	 */
	virtual UBOOL InitConnectionClass(void);

	// UNetDriver interface.
	virtual void LowLevelDestroy() PURE_VIRTUAL(UNetDriver::LowLevelDestroy,);
	virtual FString LowLevelGetNetworkNumber(UBOOL bAppendPort=FALSE) PURE_VIRTUAL(UNetDriver::LowLevelGetNetworkNumber,return TEXT(""););
	virtual void AssertValid();
	virtual UBOOL InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error );
	virtual UBOOL InitListen( FNetworkNotify* InNotify, FURL& ListenURL, FString& Error );
	/**
	 * Initialize a new peer connection on the net driver
	 *
	 * @param InNotify notification object to associate with the net driver
	 * @param ConnectURL remote ip:port of client peer to connect
	 * @param RemotePlayerId remote net id of client peer player
	 * @param LocalPlayerId net id of primary local player
	 * @param Error resulting error string from connection attempt
	 */
	virtual UBOOL InitPeer( FNetworkNotify* InNotify, const FURL& ConnectURL, FUniqueNetId RemotePlayerId, FUniqueNetId LocalPlayerId, FString& Error );
	/**
	 * Update peer connections. Associate each connection with the net owner.
	 *
	 * @param NetOwner player controller (can be NULL) for the current client
	 */
	virtual void UpdatePeerConnections(APlayerController* NetOwner);
	virtual void TickFlush();
	virtual void TickDispatch( FLOAT DeltaTime );
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	virtual void NotifyActorDestroyed( AActor* Actor );
	/** creates a child connection and adds it to the given parent connection */
	virtual class UChildConnection* CreateChild(UNetConnection* Parent);

	/** notification when a package is added to the NetPackages list */
	virtual void NotifyNetPackageAdded(UPackage* Package);
	/** notification when a package is removed from the NetPackages list */
	virtual void NotifyNetPackageRemoved(UPackage* Package);
	/** notification when an object is removed from a net package's NetObjects list */
	virtual void NotifyNetObjectRemoved(UObject* Object);

	/**
	 * Process any local talker packets that need to be sent to clients
	 */
	virtual void ProcessLocalServerPackets(void);

	/**
	 * Process any local talker packets that need to be sent to the server
	 */
	virtual void ProcessLocalClientPackets(void);

	/**
	 * Determines which other connections should receive the voice packet and
	 * queues the packet for those connections
	 *
	 * @param VoicePacket the packet to be queued
	 * @param CameFromConn the connection this packet came from (obviously skip it)
	 */
	virtual void ReplicateVoicePacket(FVoicePacket* VoicePacket,UNetConnection* CameFromConn);

	/**
	 * Determine if local voice pakcets should be replicated to server.
	 * If there are peer connections that can handle the data then no need to replicate to host.
	 *
	 * @return TRUE if voice data should be replicated on the server connection
	 */
	virtual UBOOL ShouldSendVoicePacketsToServer();

	/**
	 * Mark all local voice packets as being processed.
	 * This occurs after all net drivers have had a chance to read the local voice data.
	 */
	static void ClearLocalVoicePackets(void);

	/**
	 * @return String that uniquely describes the net driver instance
	 */
	FString GetDescription() 
	{ 
		return FString(GetName()) + FString(bIsPeer ? TEXT("(PEER)") : TEXT("")); 
	}

	/** @return TRUE if the net resource is valid or FALSE if it should not be used */
	virtual UBOOL IsNetResourceValid(void)
	{
		check(0 && "This function must be overloaded");
		return FALSE;
	}
};

#endif
