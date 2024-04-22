/*=============================================================================
	UnPenLev.h: Unreal pending level definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//@todo seamless: UPendingLevel and UNetPendingLevel should be removed

/*-----------------------------------------------------------------------------
	UPendingLevel.
-----------------------------------------------------------------------------*/

//
// Class controlling a pending game level.
//
class UPendingLevel : public ULevelBase, public FNetworkNotify
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(UPendingLevel,ULevelBase,CLASS_Transient,Engine)

public:

	// Variables.
	class UNetDriver*		NetDriver;
	class UDemoRecDriver*	DemoRecDriver;
	class UNetDriver*		PeerNetDriver;
	UBOOL					bSuccessfullyConnected;
	UBOOL					bSentJoinRequest;
	INT						FilesNeeded;
	FString					ConnectionError;
	TArray<UGuidCache*>		CachedGuidCaches;

	// Constructors.
	UPendingLevel() 
		: NetDriver(NULL)
		, DemoRecDriver(NULL)
		, PeerNetDriver(NULL)
		, bSuccessfullyConnected(FALSE)
		, bSentJoinRequest(FALSE)
		, FilesNeeded(0)
	{}
	UPendingLevel( const FURL& InURL );

	/**
	 * Static constructor, called once during static initialization of global variables for native 
	 * classes. Used to e.g. register object references for native- only classes required for realtime
	 * garbage collection or to associate UProperties.
	 */
	void StaticConstructor()
	{
		UClass* TheClass = GetClass();
		TheClass->EmitObjectReference( STRUCT_OFFSET( UPendingLevel, NetDriver ) );
		TheClass->EmitObjectReference( STRUCT_OFFSET( UPendingLevel, DemoRecDriver ) );
		TheClass->EmitObjectReference( STRUCT_OFFSET( UPendingLevel, PeerNetDriver ) );
	}

	// UObject interface.
	void Serialize( FArchive& Ar );
	void FinishDestroy()
	{
		NetDriver = NULL;
		DemoRecDriver = NULL;
		PeerNetDriver = NULL;
		
		Super::FinishDestroy();
	}

	// FNetworkNotify interface.
	void NotifyProgress( EProgressMessageType MessageType, const FString& Title, const FString& Message ) {}

	// UPendingLevel interface.
	virtual void Tick( FLOAT DeltaTime ) PURE_VIRTUAL(UPendingLevel::Tick,);
	virtual UNetDriver* GetDriver() PURE_VIRTUAL(UPendingLevel::GetDriver,return NULL;);
	virtual void SendJoin() PURE_VIRTUAL(UPendingLevel::SendJoin,);
	virtual UBOOL TrySkipFile() PURE_VIRTUAL(UPendingLevel::TrySkipFile,return FALSE;);
};

/*-----------------------------------------------------------------------------
	UNetPendingLevel.
-----------------------------------------------------------------------------*/

class UNetPendingLevel : public UPendingLevel
{
	DECLARE_CLASS_INTRINSIC(UNetPendingLevel,UPendingLevel,CLASS_Transient,Engine)

	UNetPendingLevel() : UPendingLevel()
	{}
	// Constructors.
	UNetPendingLevel( const FURL& InURL );

	// FNetworkNotify interface.
	EAcceptConnection NotifyAcceptingConnection();
	void NotifyAcceptedConnection( class UNetConnection* Connection );
	UBOOL NotifyAcceptingChannel( class UChannel* Channel );
	UWorld* NotifyGetWorld();
	virtual void NotifyControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch);
	void NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped );
	UBOOL NotifySendingFile( UNetConnection* Connection, FGuid Guid );
	void NotifyProgress( EProgressMessageType MessageType, const FString& Title, const FString& Message );
	/**
	 * Determine if peer connections are currently being accepted
	 *
	 * @return EAcceptConnection type based on if ready to accept a new peer connection
	 */
	virtual EAcceptConnection NotifyAcceptingPeerConnection();
	/**
	 * Notify that a new peer connection was created from the listening socket
	 *
	 * @param Connection net connection that was just created
	 */
	virtual void NotifyAcceptedPeerConnection( class UNetConnection* Connection );
	/**
	 * Handler for control channel messages sent on a peer connection
	 *
	 * @param Connection net connection that received the message bunch
	 * @param MessageType type of the message bunch
	 * @param Bunch bunch containing the data for the message type read from the connection
	 */
	virtual void NotifyPeerControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch);

	// UPendingLevel interface.
	void Tick( FLOAT DeltaTime );
	UNetDriver* GetDriver() { return NetDriver; }
	void SendJoin();
	UBOOL TrySkipFile();

	// UNetPendingLevel interface
	void ReceiveNextFile( UNetConnection* Connection );

	/**
	 * Create the peer net driver and a socket to listen for new client peer connections.
	 */
	void InitPeerListen();
};

