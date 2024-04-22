/*=============================================================================
	UnConn.h: Unreal connection base class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "NetworkProfiler.h"

/*-----------------------------------------------------------------------------
	UGuidCache implementation.
-----------------------------------------------------------------------------*/

/**
 * Create an instance of this class given a filename. First try to load from disk and if not found
 * will construct object and store the filename for later use during saving.
 *
 * @param	Filename	Filename to use for serialization
 * @return	instance of the cache associated with the filename
 */
UGuidCache* UGuidCache::CreateInstance(const TCHAR* Filename)
{
	UGuidCache* Instance = NULL;

	// Find it on disk first.
	if (!Instance)
	{
		// Try to load the package.
		UPackage* Package = LoadPackage(NULL, Filename, LOAD_NoWarn | LOAD_Quiet);

		// Find in memory if package loaded successfully.
		if (Package)
		{
			Instance = FindObject<UGuidCache>(Package, TEXT("GuidCache"));
		}
	}

	// If not create an instance.
	if( !Instance )
	{
		check(GIsCooking);
		UPackage* Package = UObject::CreatePackage( NULL, NULL );
		Instance = ConstructObject<UGuidCache>(UGuidCache::StaticClass(), Package, TEXT("GuidCache"));
		check( Instance );
	}

	// make sure it's not in the package map
	Instance->GetOutermost()->PackageFlags |= PKG_ServerSideOnly;
	Instance->GetOutermost()->PackageFlags &= ~PKG_AllowDownload;

	// Keep the filename around for serialization and add to root to prevent garbage collection.
	Instance->Filename = Filename;
	Instance->AddToRoot();

	return Instance;
}

/**
 * Saves the data to disk.
 *
 * @param bShouldByteSwapData If TRUE, this will byteswap on save
 */
void UGuidCache::SaveToDisk(UBOOL bShouldByteSwapData)
{
	// Set the cooked flag on the GuildCache package so that it gets treated like the other packages which are going to end up on the DVD
	// For example, ECC alignment in UObject::SavePackage
	GetOutermost()->PackageFlags |= PKG_Cooked;
	// Save package to disk using filename that was passed to CreateInstance.
	UObject::SavePackage(GetOutermost(), this, 0, *Filename, GError, NULL, bShouldByteSwapData);
}

/**
 * Serialize function.
 *
 * @param	Ar	Archive to serialize with.
 */
void UGuidCache::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// just write out the map
	Ar << PackageGuidMap;

	if (ParseParam(appCmdLine(), TEXT("dumpguidcache")))
	{
		debugf(TEXT("Guid Cache:"));
 		for (TMap<FName, FGuid>::TIterator It(PackageGuidMap); It; ++It)
 		{
 			debugf(TEXT("%s = %s"), *It.Key().ToString(), *It.Value().String());
		}
	}
}

/**
 * Stores the guid for a package
 *
 * @param	PackageName				Name of the package to cache the guid of
 * @param	Guid					Guid on disk of the package
 */
void UGuidCache::SetPackageGuid(FName PackageName, const FGuid& Guid)
{
	// just set the package's guid. Easy!
	PackageGuidMap.Set(PackageName, Guid);
}

/**
 * Retrieves the guid for the given package
 *
 * @param	PackageName				Name of the package to lookup the guid of
 * @param	Guid					[out] Guid of the package
 *
 * @return TRUE if the Guid was found
 */
UBOOL UGuidCache::GetPackageGuid(FName PackageName, FGuid& Guid)
{
	// look it up
	FGuid* FoundGuid = PackageGuidMap.Find(PackageName);

	// if we found it, then copy it to the out param
	if (FoundGuid)
	{
		Guid = *FoundGuid;
	}

	// return true if we found it
	return FoundGuid != NULL;
}

/**
 * Merge another guid cache object into this one
 *
 * @param Other Other guid cache to merge in
 */
void UGuidCache::Merge(UGuidCache* Other)
{
	for (TMap<FName, FGuid>::TIterator It(Other->PackageGuidMap); It; ++It)
	{
		// look for existing
		FGuid* Existing = PackageGuidMap.Find(It.Key());

		// look to see if they don't match, and emit a warning
		if (Existing && *Existing != It.Value() && !It.Key().ToString().StartsWith(TEXT("LocalShaderCache")))
		{
			debugf(TEXT("Mismatched Guid while merging guid caches for package %s, overwriting with Other"), *It.Key().ToString());
		}
		// update our map with the other guid cache info
		PackageGuidMap.Set(It.Key(), It.Value());
	}
}


IMPLEMENT_CLASS(UGuidCache);

/*-----------------------------------------------------------------------------
	UNetConnection implementation.
-----------------------------------------------------------------------------*/

UNetConnection::UNetConnection()
:	Driver				( NULL )
,	State				( USOCK_Invalid )
,	PackageMap			( NULL )

,	ProtocolVersion		( MIN_PROTOCOL_VERSION )
,	MaxPacket			( 0 )
,	PacketOverhead		( 0 )
,	InternalAck			( FALSE )
,	ResponseId			( 0 )
,	NegotiatedVer		( GEngineNegotiationVersion )

,	QueuedBytes			( 0 )
,	TickCount			( 0 )
,	ConnectTime			( 0.0 )

,	AllowMerge			( FALSE )
,	TimeSensitive		( FALSE )
#if WITH_UE3_NETWORKING
,	LastOutBunch		( NULL )
#endif	//#if WITH_UE3_NETWORKING

,	StatPeriod			( 1.f  )
,	BestLag				( 9999 )
,	AvgLag				( 9999 )

,	LagAcc				( 9999 )
,	BestLagAcc			( 9999 )
,	LagCount			( 0 )
,	LastTime			( 0 )
,	FrameTime			( 0 )
,	CumulativeTime		( 0 )
,	AverageFrameTime	( 0 )
,	CountedFrames		( 0 )

#if WITH_UE3_NETWORKING
,	Out					( 0 )
#endif	//#if WITH_UE3_NETWORKING
,	InPacketId			( -1 )
,	OutPacketId			( 0 ) // must be initialized as OutAckPacketId + 1 so loss of first packet can be detected
,	OutAckPacketId		( -1 )
{
}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UNetConnection::StaticConstructor()
{
	// need access to these in script
	UArrayProperty*	ChildrenProperty = new(GetClass(),TEXT("Children"),RF_Public) UArrayProperty(CPP_PROPERTY(Children),TEXT(""),CPF_Transient|CPF_DuplicateTransient);
	ChildrenProperty->Inner = new(ChildrenProperty,TEXT("Children"),RF_Public) UObjectProperty(EC_CppProperty,0,TEXT(""),CPF_Transient|CPF_Const,NULL);
	Cast<UObjectProperty>(ChildrenProperty->Inner)->PropertyClass = UChildConnection::StaticClass();

	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UNetConnection, PackageMap ) );
	TheClass->EmitFixedArrayBegin( STRUCT_OFFSET( UNetConnection, Channels ), sizeof(UChannel*), MAX_CHANNELS );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UNetConnection, Channels ) );
	TheClass->EmitFixedArrayEnd();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UNetConnection, Download ) );
	//@note: below line needed because Link(Ar, TRUE) is not called for intrinsic classes, so properties don't get GC tokens
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( UNetConnection, Children ) );
}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UChildConnection::StaticConstructor()
{
	// need access to these in script
	new(GetClass(),TEXT("Parent"),RF_Public) UObjectProperty(CPP_PROPERTY(Parent),TEXT(""),CPF_Transient|CPF_Const|CPF_DuplicateTransient, UNetConnection::StaticClass());

	//@note: below line needed because Link(Ar, TRUE) is not called for intrinsic classes, so properties don't get GC tokens
	GetClass()->EmitObjectReference(STRUCT_OFFSET(UChildConnection, Parent));
}

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
void UNetConnection::InitConnection(UNetDriver* InDriver,class FSocket* InSocket,const class FInternetIpAddr& InRemoteAddr,EConnectionState InState,UBOOL InOpenedLocally,const FURL& InURL,INT InMaxPacket,INT InPacketOverhead)
{
	// Use the passed in values
	MaxPacket = InMaxPacket;
	PacketOverhead = InPacketOverhead;
	check(MaxPacket && PacketOverhead);

#if DO_ENABLE_NET_TEST
	// Copy the command line settings from the net driver
	UpdatePacketSimulationSettings();
#endif

	// Other parameters.
	CurrentNetSpeed = URL.HasOption(TEXT("LAN")) ? GetDefault<UPlayer>()->ConfiguredLanSpeed : GetDefault<UPlayer>()->ConfiguredInternetSpeed;

	if ( CurrentNetSpeed == 0 )
	{
		CurrentNetSpeed = 2600;
	}
	else
	{
		CurrentNetSpeed = ::Max<INT>(CurrentNetSpeed, 1800);
	}

	// Create package map.
	if( GUseSeekFreePackageMap )
	{
		PackageMap = new(this)UPackageMapSeekFree(this);
	}
	else
	{
		PackageMap = new(this)UPackageMapLevel(this);
	}
	// Create the voice channel
	CreateChannel(CHTYPE_Voice,TRUE,VOICE_CHANNEL_INDEX);

	if (InDriver->bIsPeer)
	{
		// Notify game vp client of a new peer connection attempt
		GEngine->SetProgress(PMT_Information, TEXT(""),LocalizeProgress(TEXT("PeerConnecting"), TEXT("Engine")));
	}
}

/**
 * Intializes a "addressless" connection with the passed in settings
 *
 * @param InDriver the net driver associated with this connection
 * @param InState the connection state to start with for this connection
 * @param InURL the URL to init with
 * @param InConnectionSpeed Optional connection speed override
 */
void UNetConnection::InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, INT InConnectionSpeed)
{
	Driver = InDriver;
	// We won't be sending any packets, so use a default size
	MaxPacket = 512;
	PacketOverhead = 0;
	State = InState;

#if DO_ENABLE_NET_TEST
	// Copy the command line settings from the net driver
	UpdatePacketSimulationSettings();
#endif

	// Get the 
	if (InConnectionSpeed)
	{
		CurrentNetSpeed = InConnectionSpeed;
	}
	else
	{

		CurrentNetSpeed =  URL.HasOption(TEXT("LAN")) ? GetDefault<UPlayer>()->ConfiguredLanSpeed : GetDefault<UPlayer>()->ConfiguredInternetSpeed;
		if ( CurrentNetSpeed == 0 )
		{
			CurrentNetSpeed = 2600;
		}
		else
		{
			CurrentNetSpeed = ::Max<INT>(CurrentNetSpeed, 1800);
		}
	}

	// Create package map.
	if( GUseSeekFreePackageMap )
	{
		PackageMap = new(this)UPackageMapSeekFree(this);
	}
	else
	{
		PackageMap = new(this)UPackageMapLevel(this);
	}
}

void UNetConnection::Serialize( FArchive& Ar )
{
	UObject::Serialize( Ar );
	Ar << PackageMap;
	for( INT i=0; i<MAX_CHANNELS; i++ )
	{
		Ar << Channels[i];
	}
	Ar << Download;

	if (Ar.IsCountingMemory())
	{
		Children.CountBytes(Ar);
		PendingRemovePackageGUIDs.CountBytes(Ar);
		ClientVisibleLevelNames.CountBytes(Ar);
		PendingPackageInfos.CountBytes(Ar);
		QueuedAcks.CountBytes(Ar);
		ResendAcks.CountBytes(Ar);
		OpenChannels.CountBytes(Ar);
		SentTemporaries.CountBytes(Ar);
		ActorChannels.CountBytes(Ar);
	}
}

/** closes the connection (including sending a close notify across the network) */
void UNetConnection::Close()
{
	if (Driver != NULL)
	{
		// @todo joeg/matto -- Wrap this whole function in this if?
		if (State != USOCK_Closed)
		{
			NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("CLOSE"), *(GetName() + TEXT(" ") + LowLevelGetRemoteAddress())));

			debugf(NAME_NetComeGo, TEXT("Close %s %s %s %s"), *Driver->GetDescription(), *GetName(), *LowLevelGetRemoteAddress(TRUE),
				appTimestamp());

			extern void appAuthConnectionClose(UNetConnection* Connection);

#if WITH_STEAMWORKS_SOCKETS
			if (GWorld == NULL || GWorld->RedirectNetDriver != Driver)
#endif
			{
				appAuthConnectionClose(this);

				// Notify all local players, that the server connection is being closed
				if (GEngine != NULL && Driver->ServerConnection == this)
				{
					for (FLocalPlayerIterator It(GEngine); It; ++It)
					{
						if (It && !It->IsPendingKill() && !It->HasAnyFlags(RF_Unreachable))
						{
							It->eventNotifyServerConnectionClose();
						}
					}
				}
			}
		}

		if (Channels[0] != NULL)
		{
			Channels[0]->Close();
		}

		State = USOCK_Closed;
		FlushNet();
	}
}

UNetConnection* UNetConnection::GNetConnectionBeingCleanedUp = NULL;

/** handles cleaning up the associated PlayerController when killing the connection */
void UNetConnection::CleanUpActor()
{
	if (Actor != NULL)
	{
		check(GNetConnectionBeingCleanedUp == NULL);
		GNetConnectionBeingCleanedUp = this;
		//@note: if we ever implement support for splitscreen players leaving a match without the primary player leaving, we'll need to insert
		// a call to eventClearOnlineDelegates() here so that PlayerController.ClearOnlineDelegates can use the correct ControllerId (which lives
		// in ULocalPlayer)
		Actor->Player = NULL;
		if (GWorld != NULL)
		{
			GWorld->DestroyActor(Actor, 1);
		}
		Actor = NULL;
		GNetConnectionBeingCleanedUp = NULL;
	}
}

/** closes the control channel, cleans up structures, and prepares for deletion */
void UNetConnection::CleanUp()
{
	// Remove UChildConnection(s)
	for (INT i = 0; i < Children.Num(); i++)
	{
		Children(i)->CleanUp();
	}
	Children.Empty();

	Close();

	if (Driver != NULL)
	{
		// Remove from driver.
		if (Driver->ServerConnection)
		{
			check(Driver->ServerConnection == this);
			Driver->ServerConnection = NULL;
		}
		else
		{
			check(Driver->ServerConnection == NULL);
			verify(Driver->ClientConnections.RemoveItem(this) == 1);
		}
	}

	// Kill all channels.
	for (INT i = OpenChannels.Num() - 1; i >= 0; i--)
	{
		UChannel* OpenChannel = OpenChannels(i);
		if (OpenChannel != NULL)
		{
			OpenChannel->ConditionalCleanUp();
		}
	}

	PackageMap = NULL;

	// Kill download object.
	if (Download != NULL)
	{
		Download->CleanUp();
	}

	if (GIsRunning)
	{
		// Cleanup the connection controller
		if (Driver != NULL && Driver->bIsPeer)
		{
			Actor = NULL;
			if (GWorld != NULL && GWorld->GetWorldInfo() != NULL)
			{
				// Notify game vp client of the peer connection that is closing
				GEngine->SetProgress(PMT_Information, TEXT(""),LocalizeProgress(TEXT("PeerDisconnecting"), TEXT("Engine")));
			}
		}
		else
		{
			// destroy the PC that was waiting for a swap, if it exists
			if (GWorld != NULL)
			{
				GWorld->DestroySwappedPC(this);
			}
			// Destroy the connection's actor.
			if (Actor != NULL)
			{
				CleanUpActor();
			}
			else if (GWorld != NULL && GWorld->GetWorldInfo() != NULL)
			{
				AGameInfo* GameInfo = GWorld->GetWorldInfo()->Game;
				if (GameInfo != NULL)
				{
					GameInfo->eventNotifyPendingConnectionLost();
				}
			}
		}		
	}

	Driver = NULL;
}

void UChildConnection::CleanUp()
{
	if (GIsRunning)
	{
		// destroy the PC that was waiting for a swap, if it exists
		if (GWorld != NULL)
		{
			GWorld->DestroySwappedPC(this);
		}
		// Destroy the connection's actor.
		if (Actor != NULL)
		{
			CleanUpActor();
		}
	}
	PackageMap = NULL;
	Driver = NULL;
}

void UNetConnection::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		CleanUp();
	}

	Super::FinishDestroy();
}

UBOOL UNetConnection::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if ( Super::Exec(Cmd,Ar) )
	{
		return TRUE;
	}
	else if ( GEngine->Exec( Cmd, Ar ) )
	{
		return TRUE;
	}
	return FALSE;
}
void UNetConnection::AssertValid()
{
	// Make sure this connection is in a reasonable state.
	check(ProtocolVersion>=MIN_PROTOCOL_VERSION);
	check(ProtocolVersion<=MAX_PROTOCOL_VERSION);
	check(State==USOCK_Closed || State==USOCK_Pending || State==USOCK_Open);

}
void UNetConnection::SendPackageMap()
{
	// Send package map to the remote.
	for( TArray<FPackageInfo>::TIterator It(PackageMap->List); It; ++It )
	{
		SendPackageInfo(*It);
	}

	// the positioning here is important as the below may cause AddNetPackage() to be called
	bWelcomed = TRUE;

	for( INT i=0; i<Driver->DownloadManagers.Num(); i++ )
	{
		UClass* DownloadClass = StaticLoadClass( UDownload::StaticClass(), NULL, *Driver->DownloadManagers(i), NULL, LOAD_NoWarn, NULL );
		if( DownloadClass )
		{
			UObject* DefaultObject = DownloadClass->GetDefaultObject();
			check(DefaultObject);

			FString Params = *CastChecked<UDownload>(DefaultObject)->DownloadParams;
			UBOOL Compression = CastChecked<UDownload>(DefaultObject)->UseCompression;
			if( *(*Params) )
			{
				FString ClassPath(DownloadClass->GetPathName());
				FNetControlMessage<NMT_DLMgr>::Send(this, ClassPath, Params, Compression);
			}
		}
	}
}

/** parses the passed in string and fills the given package info struct with that data
 * @param Text pointer to the string
 * @param Info (out) FPackageInfo that receives the parsed data
 */
void UNetConnection::ParsePackageInfo(FInBunch& Bunch, FPackageInfo& Info)
{
	FString PackageName, FileName, ForcedExportName;
	FNetControlMessage<NMT_Uses>::Receive(Bunch, Info.Guid, PackageName, FileName, Info.Extension, Info.PackageFlags, Info.RemoteGeneration, ForcedExportName, Info.LoadingPhase);
	Info.PackageName = FName(*PackageName);
	Info.FileName = FName(*FileName);
	Info.ForcedExportBasePackageName = FName(*ForcedExportName);
}

/** sends text describing the given package info to the client via Logf() */
void UNetConnection::SendPackageInfo(FPackageInfo& Info)
{
	FString PackageName(Info.PackageName.ToString());
	FString FileName(Info.FileName.ToString());
	FString ForcedExportName((Info.Parent != NULL && Info.Parent->GetForcedExportBasePackageName() != NAME_None) ? *Info.Parent->GetForcedExportBasePackageName().ToString() : TEXT(""));
	FNetControlMessage<NMT_Uses>::Send(this, Info.Guid, PackageName, FileName, Info.Extension, Info.PackageFlags, Info.LocalGeneration, ForcedExportName, Info.LoadingPhase);
}

/** adds a package to this connection's PackageMap and synchronizes it with the client
 * @param Package the package to add
 */
void UNetConnection::AddNetPackage(UPackage* Package)
{
	if (Driver != NULL && Driver->ServerConnection != NULL)
	{
		debugf(NAME_Error, TEXT("UNetConnection::AddNetPackage() called on client"));
		appErrorfDebug(TEXT("UNetConnection::AddNetPackage() called on client"));
	}
	// if we haven't been welcomed, do nothing as that process will cause the current package map to be sent
	else if (PackageMap != NULL && bWelcomed && !GUseSeekFreePackageMap)
	{
		// we're no longer removing this package, so make sure it's not in the list
		PendingRemovePackageGUIDs.RemoveItem(Package->GetGuid());

		INT Index = PackageMap->AddPackage(Package);
		checkSlow(PackageMap->List.IsValidIndex(Index));
		PackageMap->List(Index).LoadingPhase = GSeamlessTravelHandler.HasSwitchedToDefaultMap() ? 1 : 0;
		SendPackageInfo(PackageMap->List(Index));
	}
}

/** removes a package from this connection's PackageMap and synchronizes it with the client
 * @param Package the package to remove
 */
void UNetConnection::RemoveNetPackage(UPackage* Package)
{
	if (Driver != NULL && Driver->ServerConnection != NULL)
	{
		debugf(NAME_Error, TEXT("UNetConnection::RemoveNetPackage() called on client"));
		appErrorfDebug(TEXT("UNetConnection::RemoveNetPackage() called on client"));
	}
	else if (PackageMap != NULL && !GUseSeekFreePackageMap)
	{
		// we DO NOT delete the entry in client PackageMaps so that indices are not reshuffled during play as that can potentially result in mismatches
		if (!PackageMap->RemovePackageOnlyIfSynced(Package))
		{
			// if the client hasn't acknowledged this package yet, we need to wait for it to do so
			// otherwise we get mismatches if the ack is already in transit at the time we send this
			// as we'll receive the ack but without the package reference be unable to figure out how many indices we need to reserve
			//@todo: package acks need a timeout/failsafe so we're not waiting forever and leaving UPackage objects around unnecessarily
			PendingRemovePackageGUIDs.AddItem(Package->GetGuid());
		}
		FGuid SendGuid = Package->GetGuid();
		FNetControlMessage<NMT_Unload>::Send(this, SendGuid);
	}
}

/** returns whether the client has initialized the level required for the given object
 * @return TRUE if the client has initialized the level the object is in or the object is not in a level, FALSE otherwise
 */
UBOOL UNetConnection::ClientHasInitializedLevelFor(UObject* TestObject)
{
	checkSlow(GWorld->IsServer());

	// get the level for the object
	ULevel* Level = NULL;
	for (UObject* Obj = TestObject; Obj != NULL; Obj = Obj->GetOuter())
	{
		Level = Cast<ULevel>(Obj);
		if (Level != NULL)
		{
			break;
		}
	}

	return ( Level == NULL || (Level == GWorld->PersistentLevel && GWorld->GetOutermost()->GetFName() == ClientWorldPackageName) ||
			ClientVisibleLevelNames.ContainsItem(Level->GetOutermost()->GetFName()) );
}

/**
 * Resets the FBitWriter to its default state
 */
void UNetConnection::InitOut()
{
#if WITH_UE3_NETWORKING
	// Initialize the one outgoing buffer.
	if (MaxPacket * 8 == Out.GetMaxBits())
	{
		// Reset all of our values to their initial state without a malloc/free
		Out.Reset();
	}
	else
	{
		// First time initialization needs to allocate the buffer
		Out = FBitWriter(MaxPacket * 8);
	}
#endif	//#if WITH_UE3_NETWORKING
}

void UNetConnection::ReceivedRawPacket( void* InData, INT Count )
{
#if WITH_UE3_NETWORKING
	BYTE* Data = (BYTE*)InData;

	// Handle an incoming raw packet from the driver.
	debugfSuppressed( NAME_DevNetTrafficDetail, TEXT("%6.3f: Received %i"), appSeconds() - GStartTime, Count );
	INT PacketBytes = Count + PacketOverhead;
	InBytes += PacketBytes;
	Driver->InBytes += PacketBytes;
	Driver->InPackets++;
	if( Count>0 )
	{
		BYTE LastByte = Data[Count-1];
		if( LastByte )
		{
			INT BitSize = Count*8-1;
			while( !(LastByte & 0x80) )
			{
				LastByte *= 2;
				BitSize--;
			}
			FBitReader Reader( Data, BitSize );
			ReceivedPacket( Reader );
		}
		else 
		{
			debugfSuppressed( NAME_DevNetTraffic, TEXT("Packet missing trailing 1") );
			appErrorfDebug( TEXT("Packet missing trailing 1") );
		}
	}
	else 
	{
		debugfSuppressed( NAME_DevNetTraffic, TEXT("Received zero-size packet") );
		appErrorfDebug( TEXT("Received zero-size packet") );
	}
#endif	//#if WITH_UE3_NETWORKING
}

/** flushes any pending data, bundling it into a packet and sending it via LowLevelSend()
 * also handles network simulation settings (simulated lag, packet loss, etc) unless bIgnoreSimulation is TRUE
 */
void UNetConnection::FlushNet(UBOOL bIgnoreSimulation)
{
#if WITH_UE3_NETWORKING
	// Update info.
	check(!Out.IsError());
	LastEnd = FBitWriterMark();
	TimeSensitive = 0;

	// If there is any pending data to send, send it.
	if( Out.GetNumBits() || Driver->Time-LastSendTime>Driver->KeepAliveTime )
	{
		// If sending keepalive packet, still generate header.
		if( Out.GetNumBits()==0 )
		{
			PreSend( 0 );
		}

		// Make sure packet size is byte-aligned.
		Out.WriteBit( 1 );
		check(!Out.IsError());
		while( Out.GetNumBits() & 7 )
		{
			Out.WriteBit( 0 );
		}
		check(!Out.IsError());

		// Send now.
#if DO_ENABLE_NET_TEST
		// if the connection is closing/being destroyed/etc we need to send immediately regardless of settings
		// because we won't be around to send it delayed
		if (State == USOCK_Closed || GIsGarbageCollecting || bIgnoreSimulation)
		{
			// Checked in FlushNet() so each child class doesn't have to implement this
			if (Driver->IsNetResourceValid())
			{
				LowLevelSend(Out.GetData(), Out.GetNumBytes());
			}
		}
		else if( PacketSimulationSettings.PktOrder )
		{
			DelayedPacket& B = *(new(Delayed)DelayedPacket);
			B.Data.Add( Out.GetNumBytes() );
			appMemcpy( &B.Data(0), Out.GetData(), Out.GetNumBytes() );

			for( INT i=Delayed.Num()-1; i>=0; i-- )
			{
				if( appFrand()>0.50 )
				{
					if( !PacketSimulationSettings.PktLoss || appFrand()*100.f > PacketSimulationSettings.PktLoss )
					{
						// Checked in FlushNet() so each child class doesn't have to implement this
						if (Driver->IsNetResourceValid())
						{
							LowLevelSend( (char*)&Delayed(i).Data(0), Delayed(i).Data.Num() );
						}
					}
					Delayed.Remove( i );
				}
			}
		}
		else if( PacketSimulationSettings.PktLag )
		{
			if( !PacketSimulationSettings.PktLoss || appFrand()*100.f > PacketSimulationSettings.PktLoss )
			{
				DelayedPacket& B = *(new(Delayed)DelayedPacket);
				B.Data.Add( Out.GetNumBytes() );
				appMemcpy( &B.Data(0), Out.GetData(), Out.GetNumBytes() );
				B.SendTime = appSeconds() + (DOUBLE(PacketSimulationSettings.PktLag)  + 2.0f * (appFrand() - 0.5f) * DOUBLE(PacketSimulationSettings.PktLagVariance))/ 1000.f;
			}
		}
		else if( !PacketSimulationSettings.PktLoss || appFrand()*100.f >= PacketSimulationSettings.PktLoss )
		{
#endif
			// Checked in FlushNet() so each child class doesn't have to implement this
			if (Driver->IsNetResourceValid())
			{
				LowLevelSend( Out.GetData(), Out.GetNumBytes() );
			}
#if DO_ENABLE_NET_TEST
			if( PacketSimulationSettings.PktDup && appFrand()*100.f < PacketSimulationSettings.PktDup )
			{
				// Checked in FlushNet() so each child class doesn't have to implement this
				if (Driver->IsNetResourceValid())
				{
					LowLevelSend( (char*)Out.GetData(), Out.GetNumBytes() );
				}
			}
		}
#endif

		// Update stuff.
		INT Index = OutPacketId & (ARRAY_COUNT(OutLagPacketId)-1);
		OutLagPacketId [Index] = OutPacketId;
		OutLagTime     [Index] = Driver->Time;
		OutPacketId++;
		Driver->OutPackets++;
		LastSendTime = Driver->Time;
		INT PacketBytes = Out.GetNumBytes() + PacketOverhead;
		QueuedBytes += PacketBytes;
		OutBytes += PacketBytes;
		Driver->OutBytes += PacketBytes;
		InitOut();
	}

	// Move acks around.
	for( INT i=0; i<QueuedAcks.Num(); i++ )
	{
		ResendAcks.AddItem(QueuedAcks(i));
	}
	QueuedAcks.Empty(32);
#endif	//#if WITH_UE3_NETWORKING
}

INT UNetConnection::IsNetReady( UBOOL Saturate )
{
#if WITH_UE3_NETWORKING
	// Return whether we can send more data without saturation the connection.
	if( Saturate )
		QueuedBytes = -Out.GetNumBytes();
	return QueuedBytes+Out.GetNumBytes() <= 0;
#else	//#if WITH_UE3_NETWORKING
	return 0;
#endif	//#if WITH_UE3_NETWORKING
}

void UNetConnection::ReadInput( FLOAT DeltaSeconds )
{}
IMPLEMENT_CLASS(UNetConnection);
IMPLEMENT_CLASS(UChildConnection);

/*-----------------------------------------------------------------------------
	Packet reception.
-----------------------------------------------------------------------------*/

#if WITH_UE3_NETWORKING
//
// Packet was negatively acknowledged.
//
void UNetConnection::ReceivedNak( INT NakPacketId )
{
	// Make note of the nak.
	for( INT i=OpenChannels.Num()-1; i>=0; i-- )
	{
		UChannel* Channel = OpenChannels(i);
		Channel->ReceivedNak( NakPacketId );
		if( Channel->OpenPacketId==NakPacketId )
			Channel->ReceivedAcks(); //warning: May destroy Channel.
	}
}
#endif	//#if WITH_UE3_NETWORKING

#if WITH_UE3_NETWORKING
//
// Handle a packet we just received.
//
void UNetConnection::ReceivedPacket( FBitReader& Reader )
{
	AssertValid();

	// Handle PacketId.
	if( Reader.IsError() )
	{
		debugfSuppressed( NAME_DevNetTraffic, TEXT("Packet too small") );
		appErrorfDebug( TEXT("Packet too small") );
		return;
	}

	// Update receive time to avoid timeout.
	LastReceiveTime = Driver->Time;

#if WITH_STEAMWORKS_SOCKETS
	// Check packet UID
	if (bUseSessionUID)
	{
		BYTE InSessionUID[3];
		Reader.Serialize(&InSessionUID[0], 3);

		if (Reader.IsError())
		{
			appErrorfDebug(TEXT("Overflow while reading packet SessionUID"));
			return;
		}

		if (HandshakeStatus != HS_NotStarted)
		{
			if (InSessionUID[0] != SessionUID[0] || InSessionUID[1] != SessionUID[1] || InSessionUID[2] != SessionUID[2])
			{
				debugfSuppressed(NAME_DevNetTraffic, TEXT("Received packet with incorrect UID; expected '%i %i %i' got '%i %i %i'"),
					SessionUID[0], SessionUID[1], SessionUID[2], InSessionUID[0], InSessionUID[1], InSessionUID[2]);

				return;
			}
		}
		// If the handshake has not started, reject packets with a non-null SessionUID, as they are from a previous session
		else if (InSessionUID[0] != 0 || InSessionUID[1] != 0 || InSessionUID[2] != 0)
		{
			debugfSuppressed(NAME_DevNetTraffic,
				TEXT("Received packet with non-null UID (%i %i %i), before handshake start (from previous session?)"),
				InSessionUID[0], InSessionUID[1], InSessionUID[2]);

			return;
		}
	}
#endif

	// Check packet ordering.
	INT PacketId = MakeRelative(Reader.ReadInt(MAX_PACKETID),InPacketId,MAX_PACKETID);
	if( PacketId > InPacketId )
	{
		INT PacketsLost = PacketId - InPacketId - 1;
		InPacketsLost += PacketsLost;
		Driver->InPacketsLost += PacketsLost;
		InPacketId = PacketId;
	}
	else
	{
		Driver->InOutOfOrderPackets++;
	}
	//debugfSuppressed( NAME_DevNetTraffic, TEXT("RcvdPacket: %6.3f %i"), appSeconds() - GStartTime, PacketId );


	// Disassemble and dispatch all bunches in the packet.
	while( !Reader.AtEnd() && State!=USOCK_Closed )
	{
		// Parse the bunch.
		INT StartPos = Reader.GetPosBits();
		UBOOL IsAck = Reader.ReadBit();
		if( Reader.IsError() )
		{
			debugfSuppressed( NAME_DevNetTraffic, TEXT("Bunch missing ack flag") );
			appErrorfDebug( TEXT("Bunch missing ack flag") );
			// Acknowledge the packet.
			SendAck( PacketId );
			return;
		}

		// Process the bunch.
		if( IsAck )
		{
			LastRecvAckTime = Driver->Time;

			// This is an acknowledgement.
			INT AckPacketId = MakeRelative(Reader.ReadInt(MAX_PACKETID),OutAckPacketId,MAX_PACKETID);
			if( Reader.IsError() )
			{
				debugfSuppressed( NAME_DevNetTraffic, TEXT("Bunch missing ack. PacketID=%i"), AckPacketId );
				appErrorfDebug( TEXT("Bunch missing ack") );
				// Acknowledge the packet.
				SendAck( PacketId );
				return;
			}

			// Resend any old reliable packets that the receiver hasn't acknowledged.
			if( AckPacketId>OutAckPacketId )
			{
				// If an ack packet is lost but another comes in, OutAckPacketId will be set higher than one of the packets that needs an ack
				// So the original code here will never call ReceivedNak for it, causing the reliable queue to get blocked permanently which is very bad
				// The code toward the end of this block will catch missed naks

				for (INT NakPacketId = OutAckPacketId + 1; NakPacketId<AckPacketId; NakPacketId++, OutPacketsLost++, Driver->OutPacketsLost++)
				{
					debugfSuppressed( NAME_DevNetTraffic, TEXT("   Received virtual nak %i (%.1f)"), NakPacketId, (Reader.GetPosBits()-StartPos)/8.f );
					ReceivedNak( NakPacketId );
				}
				OutAckPacketId = AckPacketId;
			}
			else if( AckPacketId<OutAckPacketId )
			{
				//warning: Double-ack logic makes this unmeasurable.
				//OutOrdAcc++;
			}
			// Update lag.
			INT Index = AckPacketId & (ARRAY_COUNT(OutLagPacketId)-1);
			if( OutLagPacketId[Index]==AckPacketId )
			{
				FLOAT NewLag = Driver->Time - OutLagTime[Index] - (FrameTime/2.f);
				LagAcc += NewLag;
					LagCount++;
			}

			// Forward the ack to the channel.
			debugfSuppressed( NAME_DevNetTrafficDetail, TEXT("   Received ack %i (%.1f)"), AckPacketId, (Reader.GetPosBits()-StartPos)/8.f );
			for( INT i=OpenChannels.Num()-1; i>=0; i-- )
			{
				UChannel* Channel = OpenChannels(i);
				for( FOutBunch* Out=Channel->OutRec; Out; Out=Out->Next )
				{
					if( Out->PacketId==AckPacketId )
					{
						Out->ReceivedAck = 1;
						if( Out->bOpen )
							Channel->OpenAcked = 1;
					}
				}
				if( Channel->OpenPacketId==AckPacketId ) // Necessary for unreliable "bNetTemporary" channels.
					Channel->OpenAcked = 1;
				Channel->ReceivedAcks(); //warning: May destroy Channel.
			}
		}
		else
		{
			// Parse the incoming data.
			FInBunch Bunch( this );
			INT IncomingStartPos = Reader.GetPosBits();
			BYTE bControl      = Reader.ReadBit();
			Bunch.PacketId     = PacketId;
			Bunch.bOpen        = bControl ? Reader.ReadBit() : 0;
			Bunch.bClose       = bControl ? Reader.ReadBit() : 0;
			Bunch.bReliable    = Reader.ReadBit();
			Bunch.ChIndex      = Reader.ReadInt( MAX_CHANNELS );
			Bunch.ChSequence   = Bunch.bReliable ? MakeRelative(Reader.ReadInt(MAX_CHSEQUENCE),InReliable[Bunch.ChIndex],MAX_CHSEQUENCE) : 0;
			Bunch.ChType       = (Bunch.bReliable||Bunch.bOpen) ? Reader.ReadInt(CHTYPE_MAX) : CHTYPE_None;
			INT BunchDataBits  = Reader.ReadInt( UNetConnection::MaxPacket*8 );
			INT HeaderPos      = Reader.GetPosBits();

#if SUPPORT_SUPPRESSED_LOGGING
			// TEMP
			if( Bunch.bReliable )
			{
				debugfSuppressed( NAME_DevNetTraffic, TEXT("   Received reliable packet, Channel %i Sequence %i Packet %i"), Bunch.ChIndex, Bunch.ChSequence, Bunch.PacketId );
			}
#endif

			if( Reader.IsError() )
			{
				debugfSuppressed( NAME_DevNetTraffic, TEXT("Bunch header overflowed") );
				appErrorfDebug( TEXT("Bunch header overflowed") );
				// Acknowledge the packet.
				SendAck( PacketId );
				return;
			}
			Bunch.SetData( Reader, BunchDataBits );
			if( Reader.IsError() )
			{
				// Bunch claims it's larger than the enclosing packet.
				debugfSuppressed( NAME_DevNetTraffic, TEXT("Bunch data overflowed (%i %i+%i/%i)"), IncomingStartPos, HeaderPos, BunchDataBits, Reader.GetNumBits() );
				appErrorfDebug( TEXT("Bunch data overflowed (%i %i+%i/%i)"), IncomingStartPos, HeaderPos, BunchDataBits, Reader.GetNumBits() );
				// Acknowledge the packet.
				SendAck( PacketId );
				return;
			}
			if( Bunch.bReliable )
			{
				debugfSuppressed( NAME_DevNetTraffic, TEXT("   Reliable Bunch, Channel %i Sequence %i: Packet %i: Size %.1f+%.1f"), Bunch.ChIndex, Bunch.ChSequence, Bunch.PacketId, (HeaderPos-IncomingStartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			}
			else
			{
				debugfSuppressed( NAME_DevNetTraffic, TEXT("   Unreliable Bunch, Channel %i: Size %.1f+%.1f"), Bunch.ChIndex, (HeaderPos-IncomingStartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			}
			if (Channels[Bunch.ChIndex] == NULL && (Bunch.ChIndex != 0 || Bunch.ChType != CHTYPE_Control))
			{
				// Can't handle other channels until control channel exists.
				if (Channels[0] == NULL)
				{
					debugf(NAME_DevNetTraffic, TEXT("Received bunch before connected"));
					Close();
					SendAck( PacketId );
					return;
				}
				// on the server, if we receive bunch data for a channel that doesn't exist while we're still logging in,
				// it's either a broken client or a new instance of a previous connection,
				// so reject it
				// NOTE: Changed from 'Actor == NULL' to '!bWelcomed', because Actor is not non-NULL until GameInfo::Login;
				//	this was causing pre-login file downloads to break. bWelcomed is set at PreLogin.
				else if (!bWelcomed && Driver->ClientConnections.ContainsItem(this))
				{
					debugf(NAME_DevNetTraffic, TEXT("Received non-control bunch during login process"));
					Close();
					SendAck(PacketId);
					return;
				}
			}
			// ignore control channel close if it hasn't been opened yet
			if (Bunch.ChIndex == 0 && Channels[0] == NULL && Bunch.bClose && Bunch.ChType == CHTYPE_Control)
			{
				debugf(NAME_DevNetTraffic, TEXT("      Received control channel close before open"));
				Close();
				SendAck(PacketId);
				return;
			}

			// Receiving data.
			UChannel* Channel = Channels[Bunch.ChIndex];

			// Ignore if reliable packet has already been processed.
			if( Bunch.bReliable && Bunch.ChSequence<=InReliable[Bunch.ChIndex] )
			{
				debugfSuppressed( NAME_DevNetTraffic, TEXT("      Received outdated bunch (Current Sequence %i)"), InReliable[Bunch.ChIndex] );
				continue;
			}

			// If unreliable but not one-shot open+close "bNetTemporary" packet, discard it.
			if( !Bunch.bReliable && (!Bunch.bOpen || !Bunch.bClose) && (!Channel || Channel->OpenPacketId==INDEX_NONE) )
			{
				debugfSuppressed( NAME_DevNetTraffic, TEXT("      Received unreliable bunch before open (Current Sequence %i)"), InReliable[Bunch.ChIndex] );
				continue;
			}

			// Create channel if necessary.
			if( !Channel )
			{
				// Validate channel type.
				if( !UChannel::IsKnownChannelType(Bunch.ChType) )
				{
					// Unknown type.
					debugfSuppressed( NAME_DevNetTraffic, TEXT("Connection unknown channel type (%i)"), Bunch.ChType );
					appErrorfDebug( TEXT("Connection unknown channel type (%i)"), Bunch.ChType );
					// Acknowledge the packet.
					SendAck( PacketId );
					return;
				}

				// Reliable (either open or later), so create new channel.
				debugfSuppressed( NAME_DevNetTraffic, TEXT("      Bunch Create %i: ChType %i"), Bunch.ChIndex, Bunch.ChType );
				Channel = CreateChannel( (EChannelType)Bunch.ChType, FALSE, Bunch.ChIndex );

				// Notify the server of the new channel.
				if( !Driver->Notify->NotifyAcceptingChannel( Channel ) )
				{
					// Channel refused, so close it, flush it, and delete it.
					FOutBunch CloseBunch( Channel, 1 );
					check(!CloseBunch.IsError());
					check(CloseBunch.bClose);
					CloseBunch.bReliable = 1;
					Channel->SendBunch( &CloseBunch, 0 );
					FlushNet();
					Channel->ConditionalCleanUp();
					if( Bunch.ChIndex==0 )
					{
						debugfSuppressed( NAME_DevNetTraffic, TEXT("Channel 0 create failed") );
						State = USOCK_Closed;
					}
					continue;
				}
			}

			if( Bunch.bOpen )
			{
				Channel->OpenAcked = 1;
				Channel->OpenPacketId = PacketId;
			}

			// Dispatch the raw, unsequenced bunch to the channel.
			Channel->ReceivedRawBunch( Bunch ); //warning: May destroy channel.
			Driver->InBunches++;

			// Disconnect if we received a corrupted packet from the client (eg server crash attempt).
			if( !Driver->ServerConnection && Bunch.IsCriticalError() )
			{
				debugfSuppressed( NAME_DevNetTraffic, TEXT("Received corrupted packet data from client %s.  Disconnecting."), *LowLevelGetRemoteAddress() );
				State = USOCK_Closed;
			}
		}
	}

	// Acknowledge the packet.
	SendAck( PacketId );

	// Brute force approach to finding and sending missing sending Naks
	// I'm not sure how this happens, but sometimes a packet can get skipped by the Nak code above
	// So look for any un-acked packets that are less than OutAckPacketId and send a nak
	// @todo ib2merge: How much overhead does this add? Is this going to slow down all networking? Why has it not been needed before?
	{
		// Find all the un-acked packets and remember their Ids
		TArray<INT> NakPacketIds;
		NakPacketIds.Reserve(32);
		for( INT i=OpenChannels.Num()-1; i>=0; i-- )
		{
			UChannel* Channel = OpenChannels(i);
			for( FOutBunch* Out=Channel->OutRec; Out; Out=Out->Next )
			{
				if( !Out->ReceivedAck && Out->PacketId < OutAckPacketId )
				{
					check(Out->bReliable);
					NakPacketIds.AddItem(Out->PacketId);
					debugfSuppressed( NAME_DevNetTraffic, TEXT("      MISSED NAK Channel %i Sequence %i Packet %i"), Out->ChIndex, Out->ChSequence, Out->PacketId );
				}
			}
		}

		// Resend each nak
		OutPacketsLost += NakPacketIds.Num();
		Driver->OutPacketsLost += NakPacketIds.Num();
		for (INT i=0; i < NakPacketIds.Num(); i++)
		{
			debugfSuppressed( NAME_DevNetTraffic, TEXT("   Received missed virtual nak %i"), NakPacketIds(i) );
			ReceivedNak( NakPacketIds(i) );
		}
	}
}
#endif	//#if WITH_UE3_NETWORKING

/*-----------------------------------------------------------------------------
	All raw sending functions.
-----------------------------------------------------------------------------*/

//
// Called before sending anything.
//
void UNetConnection::PreSend( INT SizeBits )
{
#if WITH_UE3_NETWORKING
	// Flush if not enough space.
	if( Out.GetNumBits() + SizeBits + MAX_PACKET_TRAILER_BITS > MaxPacket*8 )
		FlushNet();

	// If start of packet, send packet id.
	if( Out.GetNumBits()==0 )
	{
#if WITH_STEAMWORKS_SOCKETS
		if (bUseSessionUID)
		{
			Out.Serialize(&SessionUID[0], 3);
		}
#endif
		Out.WriteIntWrapped(OutPacketId, MAX_PACKETID);

#if WITH_STEAMWORKS_SOCKETS
		if (bUseSessionUID)
		{
			check(Out.GetNumBits()<=(MAX_PACKET_HEADER_BITS+SESSION_UID_BITS));
		}
		else
#endif
		{
			check(Out.GetNumBits()<=MAX_PACKET_HEADER_BITS);
		}
	}

	// Make sure there's enough space now.
	if( Out.GetNumBits() + SizeBits + MAX_PACKET_TRAILER_BITS > MaxPacket*8 )
		appErrorf( TEXT("PreSend overflowed: %i+%i>%i"), Out.GetNumBits(), SizeBits, MaxPacket*8 );
#endif	//#if WITH_UE3_NETWORKING
}

//
// Called after sending anything.
//
void UNetConnection::PostSend()
{
#if WITH_UE3_NETWORKING
	// If absolutely filled now, flush so that MaxSend() doesn't return zero unnecessarily.
	check(Out.GetNumBits()<=MaxPacket*8);
	if( Out.GetNumBits()==MaxPacket*8 )
		FlushNet();
#endif	//#if WITH_UE3_NETWORKING
}

//
// Resend any pending acks.
//
void UNetConnection::PurgeAcks()
{
	for( INT i=0; i<ResendAcks.Num(); i++ )
		SendAck( ResendAcks(i), 0 );
	ResendAcks.Empty(32);
}

//
// Send an acknowledgement.
//
void UNetConnection::SendAck( INT AckPacketId, UBOOL FirstTime )
{
#if WITH_UE3_NETWORKING
	if( !InternalAck )
	{
		if( FirstTime )
		{
			PurgeAcks();
			QueuedAcks.AddItem(AckPacketId);
		}
		PreSend( appCeilLogTwo(MAX_PACKETID)+1 );
		Out.WriteBit( 1 );
		Out.WriteIntWrapped(AckPacketId, MAX_PACKETID);
		AllowMerge = FALSE;
		PostSend();

//		debugfSuppressed( NAME_DevNetTraffic, TEXT("   Send ack %i 0x%016I64X"), AckPacketId, PlayerId.Uid );
	}
#endif	//#if WITH_UE3_NETWORKING
}

#if WITH_UE3_NETWORKING
//
// Send a raw bunch.
//
INT UNetConnection::SendRawBunch( FOutBunch& Bunch, UBOOL InAllowMerge )
{
	check(!Bunch.ReceivedAck);
	check(!Bunch.IsError());
	Driver->OutBunches++;
	TimeSensitive = 1;

	// Build header.
	FBitWriter Header( MAX_BUNCH_HEADER_BITS );
	Header.WriteBit( 0 );
	Header.WriteBit( Bunch.bOpen || Bunch.bClose );
	if( Bunch.bOpen || Bunch.bClose )
	{
		Header.WriteBit( Bunch.bOpen );
		Header.WriteBit( Bunch.bClose );
	}
	Header.WriteBit( Bunch.bReliable );
	Header.WriteIntWrapped(Bunch.ChIndex, MAX_CHANNELS);
	if (Bunch.bReliable)
	{
		Header.WriteIntWrapped(Bunch.ChSequence, MAX_CHSEQUENCE);
	}
	if (Bunch.bReliable || Bunch.bOpen)
	{
		Header.WriteIntWrapped(Bunch.ChType, CHTYPE_MAX);
	}
	Header.WriteIntWrapped(Bunch.GetNumBits(), UNetConnection::MaxPacket * 8);
	check(!Header.IsError());

	// If this data doesn't fit in the current packet, flush it.
	PreSend( Header.GetNumBits() + Bunch.GetNumBits() );

	// Remember start position.
	AllowMerge      = InAllowMerge;
	Bunch.PacketId  = OutPacketId;
	Bunch.Time      = Driver->Time;

	// Remember start position, and write data.
	LastStart = FBitWriterMark( Out );
	Out.SerializeBits( Header.GetData(), Header.GetNumBits() );
	Out.SerializeBits( Bunch .GetData(), Bunch .GetNumBits() );

	// Finished.
	PostSend();

	return Bunch.PacketId;
}
#endif	//#if WITH_UE3_NETWORKING

/*-----------------------------------------------------------------------------
	Channel creation.
-----------------------------------------------------------------------------*/

//
// Create a channel.
//
UChannel* UNetConnection::CreateChannel( EChannelType ChType, UBOOL bOpenedLocally, INT ChIndex )
{
	check(UChannel::IsKnownChannelType(ChType));
	AssertValid();

	// If no channel index was specified, find the first available.
	if( ChIndex==INDEX_NONE )
	{
		INT FirstChannel = 1;
		// Control channel is hardcoded to live at location 0
		if ( ChType == CHTYPE_Control )
		{
			FirstChannel = 0;
		}
		// If this is a voice channel, use its predefined channel index
		if (ChType == CHTYPE_Voice)
		{
			FirstChannel = VOICE_CHANNEL_INDEX;
		}
		// Search the channel array for an available location
		for( ChIndex=FirstChannel; ChIndex<MAX_CHANNELS; ChIndex++ )
		{
			if( !Channels[ChIndex] )
			{
				break;
			}
		}
		// Fail to create if the channel array is full
		if( ChIndex==MAX_CHANNELS )
		{
			warnf(NAME_Warning,TEXT("WARNING! Failed to create network channel for actor.  Out of channels!! (%i max)."),MAX_NET_CHANNELS);
			return NULL;
		}
	}

	// Make sure channel is valid.
	check(ChIndex<MAX_CHANNELS);
	check(Channels[ChIndex]==NULL);

	// Create channel.
	UChannel* Channel = ConstructObject<UChannel>( UChannel::ChannelClasses[ChType] );
	Channel->Init( this, ChIndex, bOpenedLocally );
	Channels[ChIndex] = Channel;
	OpenChannels.AddItem(Channel);
	debugfSuppressed(NAME_DevNetTraffic, TEXT("Created channel %i of type %i"), ChIndex, (INT)ChType);

	return Channel;
}

/*-----------------------------------------------------------------------------
	Connection polling.
-----------------------------------------------------------------------------*/

//
// Poll the connection.
// If it is timed out, close it.
//
void UNetConnection::Tick()
{
#if WITH_UE3_NETWORKING
	AssertValid();

	// Lag simulation.
#if DO_ENABLE_NET_TEST
	if( PacketSimulationSettings.PktLag )
	{
		for( INT i=0; i < Delayed.Num(); i++ )
		{
			if( appSeconds() > Delayed(i).SendTime )
			{
				LowLevelSend( (char*)&Delayed(i).Data(0), Delayed(i).Data.Num() );
				Delayed.Remove( i );
				i--;
			}
		}
	}
#endif

	// Get frame time.
	DOUBLE CurrentTime = appSeconds();
	FrameTime = CurrentTime - LastTime;
	LastTime = CurrentTime;
	CumulativeTime += FrameTime;
	CountedFrames++;
	if(CumulativeTime > 1.f)
	{
		AverageFrameTime = CumulativeTime / CountedFrames;
		CumulativeTime = 0;
		CountedFrames = 0;
	}

	// Pretend everything was acked, for 100% reliable connections or demo recording.
	if( InternalAck )
	{
		LastReceiveTime = Driver->Time;
		for( INT i=OpenChannels.Num()-1; i>=0; i-- )
		{
			UChannel* It = OpenChannels(i);
			for( FOutBunch* Out=It->OutRec; Out; Out=Out->Next )
				Out->ReceivedAck = 1;
			It->OpenAcked = 1;
			It->ReceivedAcks();
		}
	}

	// Update stats.
	if( Driver->Time - StatUpdateTime > StatPeriod )
	{
		// Update stats.
		FLOAT RealTime	= Driver->Time - StatUpdateTime;
		if( LagCount )
			AvgLag = LagAcc/LagCount;
		BestLag = AvgLag;

		if (Actor != NULL)
		{
			INT MaxPktLoss = ::Max(InPacketsLost, OutPacketsLost);
			if (Actor->myHUD != NULL)
			{
				FLOAT PktLoss = FLOAT(MaxPktLoss) * 0.01f;
				FLOAT ModifiedLag = BestLag + 1.2f * PktLoss;
				Actor->myHUD->bShowBadConnectionAlert = !InternalAck && ((ModifiedLag>0.8 || CurrentNetSpeed * (1 - PktLoss)<2000) && ActorChannels.FindRef(Actor) || Driver->InPackets < 2); 
			}
			if (Actor->PlayerReplicationInfo != NULL)
			{
				// Track Ping and PktLoss
				INT Ping = INT(Actor->PlayerReplicationInfo->Ping) * 4;

				Actor->PlayerReplicationInfo->StatPingTotals += Ping;
				Actor->PlayerReplicationInfo->StatPKLTotal += MaxPktLoss;
				Actor->PlayerReplicationInfo->StatConnectionCounts++;

				if (Actor->PlayerReplicationInfo->StatPingMin == 0 || Ping < Actor->PlayerReplicationInfo->StatPingMin)
				{
					Actor->PlayerReplicationInfo->StatPingMin = Ping;
				}

				if (Ping > Actor->PlayerReplicationInfo->StatPingMax)
				{
					Actor->PlayerReplicationInfo->StatPingMax = Ping;
				}
				
				INT PktLossPerSec = appTrunc(MaxPktLoss / RealTime);
				if (Actor->PlayerReplicationInfo->StatPKLMin == 0 || PktLossPerSec < Actor->PlayerReplicationInfo->StatPKLMin)
				{
					Actor->PlayerReplicationInfo->StatPKLMin = PktLossPerSec;
				}

				if (PktLossPerSec > Actor->PlayerReplicationInfo->StatPKLMax)
				{
					Actor->PlayerReplicationInfo->StatPKLMax = PktLossPerSec;
				}

				// Add code for In/OUT BPS
				INT InBPS = appTrunc(InBytes / RealTime);
				INT OutBPS = appTrunc(OutBytes / RealTime);

				Actor->PlayerReplicationInfo->StatAvgInBPS += InBPS;
				if (Actor->PlayerReplicationInfo->StatMaxInBPS < InBPS )
				{
					Actor->PlayerReplicationInfo->StatMaxInBPS = InBPS;
				}

				Actor->PlayerReplicationInfo->StatAvgOutBPS += OutBPS;
				if (Actor->PlayerReplicationInfo->StatMaxOutBPS < OutBPS )
				{
					Actor->PlayerReplicationInfo->StatMaxOutBPS = OutBPS;
				}
			}
		}

		// Init counters.
		LagAcc = 0;
		StatUpdateTime = Driver->Time;
		BestLagAcc = 9999;
		LagCount = 0;
		InPacketsLost = 0;
		OutPacketsLost = 0;
		InBytes = 0;
		OutBytes = 0;
	}

	// Compute time passed since last update.
	FLOAT DeltaTime     = Driver->Time - LastTickTime;
	LastTickTime        = Driver->Time;

	// Handle timeouts.
	FLOAT Timeout = Driver->InitialConnectTimeout;
	if ( (State!=USOCK_Pending) && Actor && (Actor->bPendingDestroy || Actor->bShortConnectTimeOut) )
		Timeout = Actor->bPendingDestroy ? 2.f : Driver->ConnectionTimeout;
	if( Driver->Time - LastReceiveTime > Timeout )
	{
		// Timeout.
		if( State != USOCK_Closed )
		{
			debugf( NAME_DevNet, TEXT("%s Connection timed out after %f seconds (%f)"), *Driver->GetName(), Timeout, Driver->Time - LastReceiveTime );
		}
		if (Driver->bIsPeer)
		{
			// Notify peer connection failure
			GEngine->SetProgress(PMT_PeerConnectionFailure,
				LocalizeError(TEXT("ConnectionFailed_Title"),TEXT("Engine")),
				LocalizeError(TEXT("ConnectionTimeout"),TEXT("Engine")));
				
		}
		else if (Actor != NULL)
		{
			// Let the player controller know why the connection was dropped
			Actor->eventClientSetProgressMessage(PMT_ConnectionFailure,
				LocalizeError(TEXT("ConnectionTimeout"),TEXT("Engine")),
				LocalizeError(TEXT("ConnectionFailed_Title"),TEXT("Engine")));
		}
		Close();
	}
	else
	{
		// Tick the channels.
		for( INT i=OpenChannels.Num()-1; i>=0; i-- )
			OpenChannels(i)->Tick();

		// If channel 0 has closed, mark the conection as closed.
		if( Channels[0]==NULL && (OutReliable[0]!=0 || InReliable[0]!=0) )
			State = USOCK_Closed;
	}

	// Handle timeout from paused login
	if (bLoginPaused && (CurrentTime - PauseTimestamp) >= 30.0)
	{
		debugf(TEXT("Warning, paused login timed out after 30 seconds ('%s')"), *LowLevelGetRemoteAddress(TRUE));
		Close();
	}

	// Flush.
	PurgeAcks();
	if( TimeSensitive || Driver->Time-LastSendTime>Driver->KeepAliveTime )
		FlushNet();

	if( Download )
		Download->Tick();

	// Update queued byte count.
	// this should be at the end so that the cap is applied *after* sending (and adjusting QueuedBytes for) any remaining data for this tick
	FLOAT DeltaBytes = CurrentNetSpeed * DeltaTime;
	QueuedBytes -= appTrunc(DeltaBytes);
	FLOAT AllowedLag = 2.f * DeltaBytes;
	if (QueuedBytes < -AllowedLag)
	{
		QueuedBytes = appTrunc(-AllowedLag);
	}
#endif	//#if WITH_UE3_NETWORKING
}

/*---------------------------------------------------------------------------------------
	Client Player Connection.
---------------------------------------------------------------------------------------*/

void UNetConnection::HandleClientPlayer( APlayerController *PC )
{
	// Hook up the Viewport to the new player actor.
	ULocalPlayer*	LocalPlayer = NULL;
	for(FLocalPlayerIterator It(Cast<UGameEngine>(GEngine));It;++It)
	{
		LocalPlayer = *It;
		break;
	}

	// Detach old player.
	check(LocalPlayer);
	if( LocalPlayer->Actor )
	{
		LocalPlayer->Actor->eventClearOnlineDelegates();
		if (LocalPlayer->Actor->Role == ROLE_Authority)
		{
			// local placeholder PC while waiting for connection to be established
			GWorld->DestroyActor(LocalPlayer->Actor);
		}
		else
		{
			// tell the server the swap is complete
			// we cannot use a replicated function here because the server has already transferred ownership and will reject it
			// so use a control channel message
			INT Index = INDEX_NONE;
			FNetControlMessage<NMT_PCSwap>::Send(this, Index);
		}
		LocalPlayer->Actor->Player = NULL;
		LocalPlayer->Actor = NULL;
	}

	LocalPlayer->CurrentNetSpeed = CurrentNetSpeed;

	// Init the new playerpawn.
	PC->Role = ROLE_AutonomousProxy;
	PC->SetPlayer(LocalPlayer);
	debugf(NAME_DevNet, TEXT("%s setplayer %s"),*PC->GetName(),*LocalPlayer->GetName());
	State = USOCK_Open;
	Actor = PC;

	// if we have already loaded some sublevels, tell the server about them
	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	if (WorldInfo != NULL)
	{
		for (INT i = 0; i < WorldInfo->StreamingLevels.Num(); ++i)
		{
			ULevelStreaming* LevelStreaming = WorldInfo->StreamingLevels(i);
			if (LevelStreaming != NULL && LevelStreaming->LoadedLevel != NULL && LevelStreaming->bIsVisible)
			{
				PC->eventServerUpdateLevelVisibility(LevelStreaming->LoadedLevel->GetOutermost()->GetFName(), TRUE);
			}
		}
	}

	// if we have splitscreen viewports, ask the server to join them as well
	UBOOL bSkippedFirst = FALSE;
	for (FLocalPlayerIterator It(Cast<UGameEngine>(GEngine)); It; ++It)
	{
		if (*It != LocalPlayer)
		{
			// send server command for new child connection
			It->SendSplitJoin();
		}
	}
}

void UChildConnection::HandleClientPlayer(APlayerController* PC)
{
	// find the first player that doesn't already have a connection
	ULocalPlayer* NewPlayer = NULL;
	BYTE CurrentIndex = 0;
	for (FLocalPlayerIterator It(Cast<UGameEngine>(GEngine)); It; ++It, CurrentIndex++)
	{
		if (CurrentIndex == PC->NetPlayerIndex)
		{
			NewPlayer = *It;
			break;
		}
	}

	if (NewPlayer == NULL)
	{
#if !FINAL_RELEASE
		debugf(NAME_Error, TEXT("Failed to find LocalPlayer for received PlayerController '%s' with index %d. PlayerControllers:"), *PC->GetName(), INT(PC->NetPlayerIndex));
		for (FDynamicActorIterator It; It; ++It)
		{
			if (It->GetAPlayerController() && It->Role < ROLE_Authority)
			{
				debugf(TEXT(" - %s"), *It->GetFullName());
			}
		}
		appErrorf(TEXT("Failed to find LocalPlayer for received PlayerController"));
#else
		return; // avoid crash
#endif
	}

	// Detach old player.
	check(NewPlayer);
	if (NewPlayer->Actor != NULL)
	{
		NewPlayer->Actor->eventClearOnlineDelegates();
		if (NewPlayer->Actor->Role == ROLE_Authority)
		{
			// local placeholder PC while waiting for connection to be established
			GWorld->DestroyActor(NewPlayer->Actor);
		}
		else
		{
			// tell the server the swap is complete
			// we cannot use a replicated function here because the server has already transferred ownership and will reject it
			// so use a control channel message
			INT Index = Parent->Children.FindItemIndex(this);
			FNetControlMessage<NMT_PCSwap>::Send(Parent, Index);
		}
		NewPlayer->Actor->Player = NULL;
		NewPlayer->Actor = NULL;
	}

	NewPlayer->CurrentNetSpeed = CurrentNetSpeed;

	// Init the new playerpawn.
	PC->Role = ROLE_AutonomousProxy;
	PC->SetPlayer(NewPlayer);
	debugf(NAME_DevNet, TEXT("%s setplayer %s"), *PC->GetName(), *NewPlayer->GetName());
	Actor = PC;
}

/*---------------------------------------------------------------------------------------
	File transfer.
---------------------------------------------------------------------------------------*/
//
// Initiate downloading a file to the cache directory.
// The transfer will eventually succeed or fail, and the
// NotifyReceivedFile will be called with the results.
//
void UNetConnection::ReceiveFile( INT PackageIndex )
{
	check(PackageMap->List.IsValidIndex(PackageIndex));
	if( DownloadInfo.Num() == 0 )
	{
		DownloadInfo.AddZeroed();
		DownloadInfo(0).Class = UChannelDownload::StaticClass();
		DownloadInfo(0).ClassName = TEXT("Engine.UChannelDownload");
		DownloadInfo(0).Params = TEXT("");
		DownloadInfo(0).Compression = 0;
	}
	Download = ConstructObject<UDownload>( DownloadInfo(0).Class );	
	Download->ReceiveFile( this, PackageIndex, *DownloadInfo(0).Params, DownloadInfo(0).Compression );
}

#if DO_ENABLE_NET_TEST
/**
 * Copies the simulation settings from the net driver to this object
 */
void UNetConnection::UpdatePacketSimulationSettings(void)
{
	PacketSimulationSettings.PktLoss = Driver->PacketSimulationSettings.PktLoss;
	PacketSimulationSettings.PktOrder = Driver->PacketSimulationSettings.PktOrder;
	PacketSimulationSettings.PktDup = Driver->PacketSimulationSettings.PktDup;
	PacketSimulationSettings.PktLag = Driver->PacketSimulationSettings.PktLag;
	PacketSimulationSettings.PktLagVariance = Driver->PacketSimulationSettings.PktLagVariance;
}
#endif

/**
 * Called to determine if a voice packet should be replicated to this
 * connection or any of its child connections
 *
 * @param Sender - the sender of the voice packet
 *
 * @return true if it should be sent on this connection, false otherwise
 */
UBOOL UNetConnection::ShouldReplicateVoicePacketFrom(const FUniqueNetId& Sender)
{
	if (Actor &&
		// Has the handshaking of the mute list completed?
		Actor->bHasVoiceHandshakeCompleted)
	{	
		// Check with the owning player controller first.
		if (Sender.HasValue() &&
			// Determine if the server should ignore replication of voice packets that are already handled by a peer connection
			(!Driver->AllowPeerVoice || !Actor->HasPeerConnection(Sender)) &&
			// Determine if the sender was muted for the local player 
			Actor->IsPlayerMuted(Sender) == FALSE)
		{
			// The parent wants to allow, but see if any child connections want to mute
			for (INT Index = 0; Index < Children.Num(); Index++)
			{
				if (Children(Index)->ShouldReplicateVoicePacketFrom(Sender) == FALSE)
				{
					// A child wants to mute, so skip
					return FALSE;
				}
			}
			// No child wanted to block it so accept
			return TRUE;
		}
	}
	// Not able to handle voice yet
	return FALSE;
}

/**
 * Determine if a voice packet should be replicated from the local sender to a peer connection as identified by a net id.
 *
 * @param DestPlayer - destination net id of the peer connection to send voice packet to
 *
 * @return true if it should be sent on this connection, false otherwise
 */
UBOOL UNetConnection::ShouldReplicateVoicePacketToPeer(const FUniqueNetId& DestPlayer)
{
	if (Actor &&
		// Has the handshaking of the mute list completed?
		Actor->bHasVoiceHandshakeCompleted &&
		// Only sending for peer connections
		Driver->AllowPeerVoice && Driver->bIsPeer)
	{
		// Check with the owning player controller first
		if (DestPlayer.HasValue() &&
			// Only replicate via peer connection if a valid peer exists
			Actor->HasPeerConnection(DestPlayer) &&
			// Determine if the dest player is muted for the local player 
			Actor->IsPlayerMuted(DestPlayer) == FALSE)
		{
			// The parent wants to allow, but see if any child connections want to mute
			for (INT Index = 0; Index < Children.Num(); Index++)
			{
				if (Children(Index)->ShouldReplicateVoicePacketToPeer(DestPlayer) == FALSE)
				{
					// A child wants to mute, so skip
					return FALSE;
				}
			}
			// No child wanted to block it so accept
			return TRUE;
		}
	}
	// Not able to handle voice yet
	return FALSE;
}

/**
 * @return Finds the voice channel for this connection or NULL if none
 */
UVoiceChannel* UNetConnection::GetVoiceChannel(void)
{
	return Channels[VOICE_CHANNEL_INDEX] != NULL && Channels[VOICE_CHANNEL_INDEX]->ChType == CHTYPE_Voice ?
		(UVoiceChannel*)Channels[VOICE_CHANNEL_INDEX] : NULL;
}

