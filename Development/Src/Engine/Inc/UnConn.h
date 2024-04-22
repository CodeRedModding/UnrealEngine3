/*=============================================================================
	UnConn.h: Unreal network connection base class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Determines if the communication with a remote platform requires byte swapping
 *
 * @param OtherPlatform the remote platform
 *
 * @return TRUE if the other platform is a different endianess, FALSE otherwise
 */
inline UBOOL appNetworkNeedsByteSwapping(UE3::EPlatformType OtherPlatform)
{
#if XBOX || PS3
	return OtherPlatform != UE3::PLATFORM_Xbox360 && OtherPlatform != UE3::PLATFORM_PS3;
#else
	return OtherPlatform == UE3::PLATFORM_Xbox360 || OtherPlatform == UE3::PLATFORM_PS3;
#endif
}

class UNetDriver;

/*-----------------------------------------------------------------------------
	UNetConnection.
-----------------------------------------------------------------------------*/

//
// Whether to support net lag and packet loss testing.
//
#define DO_ENABLE_NET_TEST (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)

//
// State of a connection.
//
enum EConnectionState
{
	USOCK_Invalid   = 0, // Connection is invalid, possibly uninitialized.
	USOCK_Closed    = 1, // Connection permanently closed.
	USOCK_Pending	= 2, // Connection is awaiting connection.
	USOCK_Open      = 3, // Connection is open.
};

//
// The serverside status of the connection handshake
//
enum EHandshakeStatus
{
	HS_NotStarted,		// Handshake process has not started
	HS_SentChallenge,	// Challenge has been sent to client (on client, with Steam sockets, challenge response has been sent)
	HS_Complete		// Handshake process is complete
};

#if DO_ENABLE_NET_TEST
//
// A lagged packet
//
struct DelayedPacket
{
	TArray<BYTE> Data;
	DOUBLE SendTime;
};
#endif

struct FDownloadInfo
{
	UClass* Class;
	FString ClassName;
	FString Params;
	UBOOL Compression;
};


//
// A package to guid cache, so that we can easily find cooked package guids
//
class UGuidCache : public UObject
{
	DECLARE_CLASS_INTRINSIC(UGuidCache,UObject,0,Engine);

	/**
	 * Create an instance of this class given a filename. First try to load from disk and if not found
	 * will construct object and store the filename for later use during saving.
	 *
	 * @param	Filename	Filename to use for serialization
	 * @return	instance of the cache associated with the filename
	 */
	static UGuidCache* CreateInstance(const TCHAR* Filename);
	
	/**
	 * Saves the data to disk.
	 *
	 * @param bShouldByteSwapData If TRUE, this will byteswap on save
	 */
	void SaveToDisk(UBOOL bShouldByteSwapData);

	/**
	 * Serialize function.
	 *
	 * @param	Ar	Archive to serialize with.
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Stores the guid for a package
	 *
	 * @param	PackageName				Name of the package to cache the guid of
	 * @param	Guid					Guid on disk of the package
	 */
	void SetPackageGuid(FName PackageName, const FGuid& Guid); 

	/**
	 * Retrieves the guid for the given package
	 *
	 * @param	PackageName				Name of the package to lookup the guid of
	 * @param	Guid					[out] Guid of the package
	 *
	 * @return TRUE if the Guid was found
	 */
	UBOOL GetPackageGuid(FName PackageName, FGuid& Guid);

	/**
	 * Merge another guid cache object into this one
	 *
	 * @param Other Other guid cache to merge in
	 */
	void Merge(UGuidCache* Other);

	/** Map from package name to guid */
	TMap<FName, FGuid> PackageGuidMap;

	/** Filename to use for serialization. */
	FString	Filename;
};


//
// A network connection.
//
#ifndef MAX_NET_CHANNELS
// NOTE: this should not get larger than 50,000, or else bit packed offsets in the GC don't have enough space to represent member offsets into
//		 UNetConnection and will overflow. 
#define MAX_NET_CHANNELS 2048
#endif 

class UNetConnection : public UPlayer
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(UNetConnection,UPlayer,CLASS_Transient|CLASS_Config,Engine)

	// Constants.
	enum{ MAX_PROTOCOL_VERSION = 1     };	// Maximum protocol version supported.
	enum{ MIN_PROTOCOL_VERSION = 1     };	// Minimum protocol version supported.
	enum{ MAX_CHANNELS         = MAX_NET_CHANNELS  };	// Maximum channels.

	// Connection information.
	UNetDriver*			Driver;					// Owning driver.
	EConnectionState	State;					// State this connection is in.
	FURL				URL;					// URL of the other side.
	UPackageMap*		PackageMap;				// Package map between local and remote.
	/** Whether this channel needs to byte swap all data or not */
	UBOOL				bNeedsByteSwapping;
	/** whether the server has welcomed this client (i.e. called UWorld::WelcomePlayer() for it) */
	UBOOL				bWelcomed;
	/** Net id of remote player on this connection. Only valid on client connections. */
	FUniqueNetId		PlayerId;

	// Negotiated parameters.
	INT				ProtocolVersion;		// Protocol version we're communicating with (<=PROTOCOL_VERSION).
	INT				MaxPacket;				// Maximum packet size.
	INT				PacketOverhead;			// Bytes overhead per packet sent.
	UBOOL			InternalAck;			// Internally ack all packets, for 100% reliable connections.
	FString			Challenge;				// Server-generated challenge.
	FString			ClientResponse;			// Client-generated response.
	INT				ResponseId;				// Id assigned by the server for linking responses to connections upon authentication
	INT				NegotiatedVer;			// Negotiated version for new channels.
	FStringNoInit	RequestURL;				// URL requested by client

	// CD key authentication
    FString			CDKeyHash;				// Hash of client's CD key
	FString			CDKeyResponse;			// Client's response to CD key challenge

	// Internal.
	DOUBLE			LastReceiveTime;		// Last time a packet was received, for timeout checking.
	DOUBLE			LastSendTime;			// Last time a packet was sent, for keepalives.
	DOUBLE			LastTickTime;			// Last time of polling.
	INT				QueuedBytes;			// Bytes assumed to be queued up.
	INT				TickCount;				// Count of ticks.
	/** The last time an ack was received */
	FLOAT			LastRecvAckTime;
	/** Time when connection request was first initiated */
	FLOAT			ConnectTime;

	// Merge info.
#if WITH_UE3_NETWORKING
	FBitWriterMark  LastStart;				// Most recently sent bunch start.
	FBitWriterMark  LastEnd;				// Most recently sent bunch end.
#endif	//#if WITH_UE3_NETWORKING
	UBOOL			AllowMerge;				// Whether to allow merging.
	UBOOL			TimeSensitive;			// Whether contents are time-sensitive.
#if WITH_UE3_NETWORKING
	FOutBunch*		LastOutBunch;			// Most recent outgoing bunch.
	FOutBunch		LastOut;
#endif	//#if WITH_UE3_NETWORKING

	// Stat display.
	/** Time of last stat update */
	DOUBLE			StatUpdateTime;
	/** Interval between gathering stats */
	FLOAT			StatPeriod;
	FLOAT			BestLag,   AvgLag;		// Lag.

	// Stat accumulators.
	FLOAT			LagAcc, BestLagAcc;		// Previous msec lag.
	INT				LagCount;				// Counter for lag measurement.
	DOUBLE			LastTime, FrameTime;	// Monitors frame time.
	DOUBLE			CumulativeTime, AverageFrameTime;
	INT				CountedFrames;
	/** bytes sent/received on this connection */
	INT InBytes, OutBytes;
	/** packets lost on this connection */
	INT InPacketsLost, OutPacketsLost;

	// Packet.
#if WITH_UE3_NETWORKING
	FBitWriter		Out;					// Outgoing packet.
#endif	//#if WITH_UE3_NETWORKING
	DOUBLE			OutLagTime[256];		// For lag measuring.
	INT				OutLagPacketId[256];	// For lag measuring.
	INT				InPacketId;				// Full incoming packet index.
	INT				OutPacketId;			// Most recently sent packet.
	INT 			OutAckPacketId;			// Most recently acked outgoing packet.

	// Channel table.
	UChannel*  Channels     [ MAX_CHANNELS ];
	INT        OutReliable  [ MAX_CHANNELS ];
	INT        InReliable   [ MAX_CHANNELS ];
	INT		PendingOutRec[ MAX_CHANNELS ];	// Outgoing reliable unacked data from previous (now destroyed) channel in this slot.  This contains the first chsequence not acked
	TArray<INT> QueuedAcks, ResendAcks;
	TArray<UChannel*> OpenChannels;
	TArray<AActor*> SentTemporaries;
	TMap<AActor*,UActorChannel*> ActorChannels;

	// File Download
	UDownload*				Download;
	TArray<FDownloadInfo>	DownloadInfo;

	// This is set to TRUE when server sees NMT_Hello, so we know this client has an auth-enabled online subsystem, only relevant if AccessControl.bAuthenticateClients is set
	UBOOL					bSupportsAuth;
	// whether or not the login process has been paused; Welcome/Login will not be called, until the login process is resumed
	UBOOL					bLoginPaused;
	// Timestamp for when the login was paused (times out after 30 seconds, if not resumed)
	FLOAT					PauseTimestamp;
	// Whether or not the connection is ready to have Welcome/Login called (used when bLoginPaused is set)
	UBOOL					bWelcomeReady;

	/** list of packages the client has received info from the server on via "USES" but can't verify until async loading has completed
	 * @see UWorld::NotifyReceivedText() and UWorld::VerifyPackageInfo()
	 */
	TArray<FPackageInfo> PendingPackageInfos;
	/** on the server, the world the client has told us it has loaded
	 * used to make sure the client has traveled correctly, prevent replicating actors before level transitions are done, etc
	 */
	FName ClientWorldPackageName;
	/** on the server, the package names of streaming levels that the client has told us it has made visible
	 * the server will only replicate references to Actors in visible levels so that it's impossible to send references to
	 * Actors the client has not initialized
	 */
	TArray<FName> ClientVisibleLevelNames;
	/** GUIDs of packages we are waiting for the client to acknowledge before removing from the PackageMap */
	TArray<FGuid> PendingRemovePackageGUIDs;

	/** child connections for secondary viewports */
	TArray<class UChildConnection*> Children;

	AActor*  Viewer;
	AActor **OwnedConsiderList;
	INT OwnedConsiderListSize;

	/** The serverside status of the control channel handshake */
	EHandshakeStatus HandshakeStatus;

	/** The stored handshake challenge */
	DWORD HandshakeChallenge;

#if WITH_STEAMWORKS_SOCKETS
	/** whether or not to sign all packets with a UID, shared between the client/server, rejecting packets not matching that UID */
	UBOOL bUseSessionUID;

	/**
	 * The UID of the current connection session; appended to all packets, so Steam socket data (which is connectionless) doesn't collide
	 * NOTE: This has the side-benefit, of blocking almost all spoofed-packet exploits, and is useful outside of Steam
	 *		(this is 3 bytes instead of 1, to make guessing/spoofing the UID impractical)
	 */
	BYTE SessionUID[3];
#endif


#if DO_ENABLE_NET_TEST
	// For development.
	/** Packet settings for testing lag, net errors, etc */
	FPacketSimulationSettings PacketSimulationSettings;
	TArray<DelayedPacket> Delayed;

	/** Copies the settings from the net driver to our local copy */
	void UpdatePacketSimulationSettings(void);
#endif
	
	/** @hack: set to net connection currently inside CleanUp(), for HasClientLoadedCurrentWorld() to be able to find it during PlayerController
	 * destruction, since we clear its Player before destroying it. (and that's not easily reversed)
	 */
	static UNetConnection* GNetConnectionBeingCleanedUp;

	// Constructors and destructors.
	UNetConnection();

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void FinishDestroy();

	// UPlayer interface.
	void ReadInput( FLOAT DeltaSeconds );

	// FExec interface.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );

	// UNetConnection interface.
	virtual UChildConnection* GetUChildConnection()
	{
		return NULL;
	}
	virtual FString LowLevelGetRemoteAddress(UBOOL bAppendPort=FALSE) PURE_VIRTUAL(UNetConnection::LowLevelGetRemoteAddress,return TEXT(""););
	virtual FString LowLevelDescribe() PURE_VIRTUAL(UNetConnection::LowLevelDescribe,return TEXT(""););
	virtual void LowLevelSend( void* Data, INT Count ) PURE_VIRTUAL(UNetConnection::LowLevelSend,); //!! "Looks like an FArchive"
	virtual void InitOut();
	virtual void AssertValid();
	virtual void SendAck( INT PacketId, UBOOL FirstTime=1 );
	/** flushes any pending data, bundling it into a packet and sending it via LowLevelSend()
	 * also handles network simulation settings (simulated lag, packet loss, etc) unless bIgnoreSimulation is TRUE
	 */
	virtual void FlushNet(UBOOL bIgnoreSimulation = FALSE);
	virtual void Tick();
	virtual INT IsNetReady( UBOOL Saturate );
	virtual void HandleClientPlayer( APlayerController* PC );
	virtual void SetActorDirty(AActor* DirtyActor);
	/** @return Returns the address of the connection as an integer */
	virtual INT GetAddrAsInt(void)
	{
		return 0;
	}
	/** @return Returns the port of the connection as an integer */
	virtual INT GetAddrPort(void)
	{
		return 0;
	}
	/** closes the connection (including sending a close notify across the network) */
	void Close();
	/** closes the control channel, cleans up structures, and prepares for deletion */
	virtual void CleanUp();

	/**
	 * Initializes a connection with the passed in settings
	 *
	 * @param InDriver the net driver associated with this connection
	 * @param InSocket the socket associated with this connection
	 * @param InRemoteAddr the remote address for this connection
	 * @param InState the connection state to start with for this connection
	 * @param InOpenedLocally whether the connection was a client/server
	 * @param InURL the URL to init with
	 * @param InMaxPacket the max packet size that will be used for sending
	 * @param InPacketOverhead the packet overhead for this connection type
	 */
	virtual void InitConnection(UNetDriver* InDriver,class FSocket* InSocket,const class FInternetIpAddr& InRemoteAddr,EConnectionState InState,UBOOL InOpenedLocally,const FURL& InURL,INT InMaxPacket = 0,INT InPacketOverhead = 0);

	/**
	 * Intializes a "addressless" connection with the passed in settings
	 *
	 * @param InDriver the net driver associated with this connection
	 * @param InState the connection state to start with for this connection
	 * @param InURL the URL to init with
	 * @param InConnectionSpeed Optional connection speed override
	 */
	virtual void InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, INT InConnectionSpeed=0);

	// Functions.
	void PurgeAcks();
	void SendPackageMap();
	void PreSend( INT SizeBits );
	void PostSend();

	/** parses the passed in control channel bunch and fills the given package info struct with that data
	 * @param Bunch network data bunch received on control channel
	 * @param Info (out) FPackageInfo that receives the parsed data
	 */
	void ParsePackageInfo(FInBunch& Bunch, FPackageInfo& Info);
	/** sends text describing the given package info to the client via the control channel
	 * @note: parameter lacks const due to FArchive/FOutBunch interface; it won't be modified
	 */
	void SendPackageInfo(FPackageInfo& Info);
	/** adds a package to this connection's PackageMap and synchronizes it with the client
	 * @param Package the package to add
	 */
	void AddNetPackage(UPackage* Package);
	/** removes a package from this connection's PackageMap and synchronizes it with the client
	 * @param Package the package to remove
	 */
	void RemoveNetPackage(UPackage* Package);
	/** returns whether the client has initialized the level required for the given object
	 * @return TRUE if the client has initialized the level the object is in or the object is not in a level, FALSE otherwise
	 */
	virtual UBOOL ClientHasInitializedLevelFor(UObject* TestObject);

	/**
	 * Allows the connection to process the raw data that was received
	 *
	 * @param Data the data to process
	 * @param Count the size of the data buffer to process
	 */
	virtual void ReceivedRawPacket(void* Data,INT Count);

	INT SendRawBunch( FOutBunch& Bunch, UBOOL InAllowMerge );
	UNetDriver* GetDriver() {return Driver;}
	class UControlChannel* GetControlChannel();
	UChannel* CreateChannel( enum EChannelType Type, UBOOL bOpenedLocally, INT ChannelIndex=INDEX_NONE );
#if WITH_UE3_NETWORKING
	void ReceivedPacket( FBitReader& Reader );
#endif	//#if WITH_UE3_NETWORKING
	void ReceivedNak( INT NakPacketId );
	void ReceiveFile( INT PackageIndex );
	void SlowAssertValid()
	{
#if DO_GUARD_SLOW
		AssertValid();
#endif
	}

	/**
	 * Called to determine if a voice packet should be replicated to this
	 * connection or any of its child connections
	 *
	 * @param Sender - the sender of the voice packet
	 *
	 * @return true if it should be sent on this connection, false otherwise
	 */
	UBOOL ShouldReplicateVoicePacketFrom(const FUniqueNetId& Sender);

	/**
	 * Determine if a voice packet should be replicated from the local sender to a peer connection as identified by a net id.
	 *
	 * @param DestPlayer - destination net id of the peer connection to send voice packet to
	 *
	 * @return true if it should be sent on this connection, false otherwise
	 */
	UBOOL ShouldReplicateVoicePacketToPeer(const FUniqueNetId& DestPlayer);

	/**
	 * @return Finds the voice channel for this connection or NULL if none
	 */
	class UVoiceChannel* GetVoiceChannel(void);
protected:
	/** handles cleaning up the associated PlayerController when killing the connection */
	void CleanUpActor();
};

/** represents a secondary splitscreen connection that reroutes calls to the parent connection */
class UChildConnection : public UNetConnection
{
public:
	DECLARE_CLASS_INTRINSIC(UChildConnection,UNetConnection,CLASS_Transient|CLASS_Config|0,Engine)

	UNetConnection* Parent;

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UNetConnection interface.
	virtual UChildConnection* GetUChildConnection()
	{
		return this;
	}
	virtual FString LowLevelGetRemoteAddress(UBOOL bAppendPort=FALSE)
	{
		return Parent->LowLevelGetRemoteAddress(bAppendPort);
	}
	virtual FString LowLevelDescribe()
	{
		return Parent->LowLevelDescribe();
	}
	virtual void LowLevelSend( void* Data, INT Count )
	{
	}
	virtual void InitOut()
	{
		Parent->InitOut();
	}
	virtual void AssertValid()
	{
		Parent->AssertValid();
	}
	virtual void SendAck( INT PacketId, UBOOL FirstTime=1 )
	{
	}
	virtual void FlushNet(UBOOL bIgnoreSimulation = FALSE)
	{
		Parent->FlushNet(bIgnoreSimulation);
	}
	virtual INT IsNetReady(UBOOL Saturate)
	{
		return Parent->IsNetReady(Saturate);
	}
	void SetActorDirty(AActor* DirtyActor)
	{
		Parent->SetActorDirty(DirtyActor);
	}
	virtual void Tick()
	{
		State = Parent->State;
	}
	virtual void HandleClientPlayer(APlayerController* PC);
	virtual void CleanUp();
};

