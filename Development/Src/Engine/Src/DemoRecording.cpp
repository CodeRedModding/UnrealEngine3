/*=============================================================================
	DemoRecDrv.cpp: Unreal demo recording network driver.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter.
=============================================================================*/

#include "EnginePrivate.h"
#include "DemoRecording.h"
#define PACKETSIZE 512

/*-----------------------------------------------------------------------------
	UDemoRecConnection.
-----------------------------------------------------------------------------*/

UDemoRecConnection::UDemoRecConnection()
{
	MaxPacket   = PACKETSIZE;
	InternalAck = 1;
}

/**
 * Intializes an "addressless" connection with the passed in settings
 *
 * @param InDriver the net driver associated with this connection
 * @param InState the connection state to start with for this connection
 * @param InURL the URL to init with
 * @param InConnectionSpeed Optional connection speed override
 */
void UDemoRecConnection::InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, INT InConnectionSpeed)
{
	// default implementation
	Super::InitConnection(InDriver, InState, InURL, InConnectionSpeed);

	InitOut();

	// the driver must be a DemoRecording driver (GetDriver makes assumptions to avoid Cast'ing each time)
	check(InDriver->IsA(UDemoRecDriver::StaticClass()));
}

FString UDemoRecConnection::LowLevelGetRemoteAddress(UBOOL bAppendPort)
{
	return TEXT("");
}

void UDemoRecConnection::LowLevelSend( void* Data, INT Count )
{
	if (!GetDriver()->ServerConnection && GetDriver()->FileAr)
	{
		*GetDriver()->FileAr << GetDriver()->LastDeltaTime << GetDriver()->FrameNum << Count;
		GetDriver()->FileAr->Serialize( Data, Count );
		//@todo demorec: if GetDriver()->GetFileAr()->IsError(), print error, cancel demo recording
	}
}

FString UDemoRecConnection::LowLevelDescribe()
{
	return TEXT("Demo recording driver connection");
}

INT UDemoRecConnection::IsNetReady( UBOOL Saturate )
{
	return 1;
}

void UDemoRecConnection::FlushNet(UBOOL bIgnoreSimulation)
{
	// in playback, there is no data to send except
	// channel closing if an error occurs.
	if (GetDriver()->ServerConnection != NULL)
	{
		InitOut();
	}
	else
	{
		Super::FlushNet(bIgnoreSimulation);
	}
}

void UDemoRecConnection::HandleClientPlayer( APlayerController* PC )
{
	Super::HandleClientPlayer(PC);
	PC->bDemoOwner = TRUE;
}

UBOOL UDemoRecConnection::ClientHasInitializedLevelFor(UObject* TestObject)
{
	// there is no way to know when demo playback will load levels,
	// so we assume it always is anytime after initial replication and rely on the client to load it in time to make the reference work
	return (GetDriver()->FrameNum > 2 || Super::ClientHasInitializedLevelFor(TestObject));
}

IMPLEMENT_CLASS(UDemoRecConnection);

/*-----------------------------------------------------------------------------
	UDemoRecDriver.
-----------------------------------------------------------------------------*/

UDemoRecDriver::UDemoRecDriver()
{}
UBOOL UDemoRecDriver::InitBase( UBOOL Connect, FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
	DemoFilename	= ConnectURL.Map;
	Time			= 0;
	FrameNum	    = 0;
	bHasDemoEnded	= FALSE;

	return TRUE;
}

UBOOL UDemoRecDriver::InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
#if !PS3
	// handle default initialization
	if (!Super::InitConnect(InNotify, ConnectURL, Error))
	{
		return FALSE;
	}
	if (!InitBase(1, InNotify, ConnectURL, Error))
	{
		return FALSE;
	}

	// Playback, local machine is a client, and the demo stream acts "as if" it's the server.
	ServerConnection = ConstructObject<UNetConnection>(UDemoRecConnection::StaticClass());
	ServerConnection->InitConnection(this, USOCK_Pending, ConnectURL, 1000000);

	// open the pre-recorded demo file
	FileAr = GFileManager->CreateFileReader(*DemoFilename);
	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for reading"), *DemoFilename );//@todo demorec: localize
		return 0;
	}

	// use the same byte format regardless of platform so that the demos are cross platform
	//@note: swap on non console platforms as the console archives have byte swapping compiled out by default
#if !XBOX && !PS3
	FileAr->SetByteSwapping(TRUE);
#endif

	INT EngineVersion = 0;
	INT ChangeList = 0;
	(*FileAr) << EngineVersion;
	(*FileAr) << ChangeList;
#if SHIPPING_PC_GAME
	debugf(TEXT("Starting demo playback with demo %s recorded with version %i"), *DemoFilename, EngineVersion);
#else
	debugf(TEXT("Starting demo playback with demo %s recorded with version %i, changelist %i"), *DemoFilename, EngineVersion, ChangeList);
#endif
	(*FileAr) << PlaybackTotalFrames;

	LoopURL								= ConnectURL;
	bNoFrameCap							= ConnectURL.HasOption(TEXT("timedemo"));
	bAllowInterpolation					= !ConnectURL.HasOption(TEXT("disallowinterp"));
	bShouldExitAfterPlaybackFinished	= ConnectURL.HasOption(TEXT("exitafterplayback"));
	PlayCount							= appAtoi( ConnectURL.GetOption(TEXT("playcount="), TEXT("1")) );
	if( PlayCount == 0 )
	{
		PlayCount = INT_MAX;
	}
	bShouldSkipPackageChecking			= ConnectURL.HasOption(TEXT("skipchecks"));
	
	PlaybackStartTime					= appSeconds();
	LastFrameTime						= appSeconds();
#endif

	return TRUE;
}

/** sets the RemoteGeneration and LocalGeneration as appopriate for the given package info
 * so that the demo driver can correctly record object references into the demo
 */
void UDemoRecDriver::SetDemoPackageGeneration(FPackageInfo& Info)
{
	// content is always recorded as generation 1 so playing back the demo in a different language works
	// but we allow code to be the current version so that any new features/etc get recorded into the demo
	// (however, this means patches break old demos)
	if (Info.PackageFlags & PKG_ContainsScript)
	{
		Info.RemoteGeneration = Info.LocalGeneration;
	}
	else
	{
		Info.LocalGeneration = 1;
		Info.RemoteGeneration = 1;
	}
}

UBOOL UDemoRecDriver::InitListen( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
#if !PS3
	if( !Super::InitListen( InNotify, ConnectURL, Error ) )
	{
		return 0;
	}
	if( !InitBase( 0, InNotify, ConnectURL, Error ) )
	{
		return 0;
	}

	class AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	if ( !WorldInfo )
	{
		Error = TEXT("No WorldInfo!!");
		return FALSE;
	}

	// Recording, local machine is server, demo stream acts "as if" it's a client.
	UDemoRecConnection* Connection = ConstructObject<UDemoRecConnection>(UDemoRecConnection::StaticClass());
	Connection->InitConnection(this, USOCK_Open, ConnectURL, 1000000);
	Connection->InitOut();

	FileAr = GFileManager->CreateFileWriter( *DemoFilename );
	ClientConnections.AddItem( Connection );

	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for writing"), *DemoFilename );//@todo demorec: localize
		return 0;
	}

	// use the same byte format regardless of platform so that the demos are cross platform
	//@note: swap on non console platforms as the console archives have byte swapping compiled out by default
#if !XBOX && !PS3
	FileAr->SetByteSwapping(TRUE);
#endif

	// write engine version info
	INT EngineVersion = GEngineVersion;
	INT ChangeList = GBuiltFromChangeList;
	(*FileAr) << EngineVersion;
	(*FileAr) << ChangeList;

	// write placeholder for total frames - will be updated when the demo is stopped
	PlaybackTotalFrames = -1;
	(*FileAr) << PlaybackTotalFrames;

	// Setup
	UGameEngine* GameEngine = CastChecked<UGameEngine>(GEngine);

	// Build package map.
	MasterMap->AddNetPackages();
	// fixup the RemoteGeneration to be LocalGeneration
	for (INT InfoIndex = 0; InfoIndex < MasterMap->List.Num(); InfoIndex++)
	{
		SetDemoPackageGeneration(MasterMap->List(InfoIndex));
	}
	MasterMap->Compute();

	UPackage::NetObjectNotifies.AddItem(this);

	// Create the control channel.
	Connection->CreateChannel( CHTYPE_Control, 1, 0 );

	// Send initial message.
	BYTE PlatformType = BYTE(appGetPlatformType());
	FNetControlMessage<NMT_HandshakeStart>::Send(Connection, PlatformType);
	Connection->FlushNet();

	// Welcome the player to the level.
	GWorld->WelcomePlayer(Connection);

	// Spawn the demo recording spectator.
	SpawnDemoRecSpectator(Connection);
#endif
	return 1;
}

void UDemoRecDriver::NotifyNetPackageAdded(UPackage* Package)
{
	// overridden to force the Local/RemoteGeneration for demo playback
	if (!GIsRequestingExit && ServerConnection == NULL && !GUseSeekFreePackageMap)
	{
		// updating the master map is probably unnecessary after the connection has been created, but it doesn't hurt to keep it in sync
		SetDemoPackageGeneration(MasterMap->List(MasterMap->AddPackage(Package)));

		if (ClientConnections.Num() > 0 && ClientConnections(0) != NULL && ClientConnections(0)->bWelcomed)
		{
			INT Index = ClientConnections(0)->PackageMap->AddPackage(Package);
			SetDemoPackageGeneration(ClientConnections(0)->PackageMap->List(Index));
			ClientConnections(0)->SendPackageInfo(ClientConnections(0)->PackageMap->List(Index));
		}
	}
}

void UDemoRecDriver::StaticConstructor()
{
	new(GetClass(),TEXT("DemoSpectatorClass"), RF_Public)UStrProperty(CPP_PROPERTY(DemoSpectatorClass), TEXT("Client"), CPF_Config);
	new(GetClass(),TEXT("MaxRewindPoints"), RF_Public)UIntProperty(CPP_PROPERTY(MaxRewindPoints), TEXT("Rewind"), CPF_Config);
	new(GetClass(),TEXT("RewindPointInterval"), RF_Public)UFloatProperty(CPP_PROPERTY(RewindPointInterval), TEXT("Rewind"), CPF_Config);
	new(GetClass(),TEXT("NumRecentRewindPoints"), RF_Public)UIntProperty(CPP_PROPERTY(NumRecentRewindPoints), TEXT("Rewind"), CPF_Config);
}

void UDemoRecDriver::LowLevelDestroy()
{
	debugf( TEXT("Closing down demo driver.") );

	// Shut down file.
	if( FileAr )
	{	
		delete FileAr;
		FileAr = NULL;
	}
}

UBOOL UDemoRecDriver::UpdateDemoTime( FLOAT* DeltaTime, FLOAT TimeDilation )
{
	UBOOL Result = 0;
	bNoRender = FALSE;

	// Playback.
	if( ServerConnection )
	{
		// skip play back if in player only mode
		if (GWorld->GetWorldInfo()->bPlayersOnly)
		{
			return 0;
		}

		// This will be triggered several times during initial handshake.
		if( FrameNum == 0 )
		{
			PlaybackStartTime = appSeconds();
		}

		if (ShouldInterpolate())
		{
			if (ServerConnection->State == USOCK_Open)
			{
				if (!FileAr->AtEnd() && !FileAr->IsError())
				{
					// peek at next delta time.
					FLOAT NewDeltaTime;
					INT NewFrameNum;

					*FileAr << NewDeltaTime << NewFrameNum;
					FileAr->Seek(FileAr->Tell() - sizeof(NewDeltaTime) - sizeof(NewFrameNum));

					// only increment frame if enough time has passed
					DemoRecMultiFrameDeltaTime += *DeltaTime * TimeDilation;
					while (DemoRecMultiFrameDeltaTime >= NewDeltaTime)
					{
						FrameNum++;
						DemoRecMultiFrameDeltaTime -= NewDeltaTime;
					}
				}
			}
			else
			{
				// increment the current frame every client frame until we're fully initialized
				FrameNum++;
			}
		}
		else
		{
			// Ensure LastFrameTime is inside a valid range, so we don't lock up if things get very out of sync.
			LastFrameTime = Clamp<DOUBLE>( LastFrameTime, appSeconds() - 1.0, appSeconds() );

			FrameNum++;
			if( ServerConnection->State==USOCK_Open ) 
			{
				if( !FileAr->AtEnd() && !FileAr->IsError() )
				{
					// peek at next delta time.
					FLOAT NewDeltaTime;
					INT NewFrameNum;

					*FileAr << NewDeltaTime << NewFrameNum;
					FileAr->Seek(FileAr->Tell() - sizeof(NewDeltaTime) - sizeof(NewFrameNum));

					// If the real delta time is too small, sleep for the appropriate amount.
					if( !bNoFrameCap )
					{
						if ( (appSeconds() > LastFrameTime+(DOUBLE)NewDeltaTime/TimeDilation) )
						{
							bNoRender = TRUE;
						}
						else
						{
							while(appSeconds() < LastFrameTime+(DOUBLE)NewDeltaTime/TimeDilation)
							{
								appSleep(0);
							}
						}
					}
					// Lie to the game about the amount of time which has passed.
					*DeltaTime = NewDeltaTime;
				}
			}
	 		LastFrameTime = appSeconds();
		}
	}
	// Recording.
	else
	{
		BYTE NetMode = GWorld->GetWorldInfo()->NetMode;

		// Accumulate the current DeltaTime for the real frames this demo frame will represent.
		DemoRecMultiFrameDeltaTime += *DeltaTime;

		// Cap client demo recording rate (but not framerate).
		if( NetMode==NM_DedicatedServer || ( (appSeconds()-LastClientRecordTime) >= (DOUBLE)(1.f/NetServerMaxTickRate) ) )
		{
			// record another frame.
			FrameNum++;
			LastClientRecordTime		= appSeconds();
			LastDeltaTime				= DemoRecMultiFrameDeltaTime;
			DemoRecMultiFrameDeltaTime	= 0.f;
			Result						= 1;

			// Save the new delta-time and frame number, with no data, in case there is nothing to replicate.
			INT Count = 0;
			*FileAr << LastDeltaTime << FrameNum << Count;
		}
	}

	return Result;
}

void UDemoRecDriver::DemoPlaybackEnded()
{
	ServerConnection->State = USOCK_Closed;
	bHasDemoEnded = TRUE;
	PlayCount--;

	FLOAT Seconds = appSeconds()-PlaybackStartTime;
	if( bNoFrameCap )
	{
		FString Result = FString::Printf(TEXT("Demo %s ended: %d frames in %lf seconds (%.3f fps)"), *DemoFilename, FrameNum, Seconds, FrameNum/Seconds );
		debugf(TEXT("%s"),*Result);
		if (ServerConnection->Actor != NULL)
		{
			ServerConnection->Actor->eventClientMessage( *Result, NAME_None );//@todo demorec: localize
		}
	}
	else
	{
		if (ServerConnection->Actor != NULL)
		{
			ServerConnection->Actor->eventClientMessage( *FString::Printf(TEXT("Demo %s ended: %d frames in %f seconds"), *DemoFilename, FrameNum, Seconds ), NAME_None );//@todo demorec: localize
		}
	}

	// Exit after playback of last loop iteration has finished.
	if( bShouldExitAfterPlaybackFinished && PlayCount == 0 )
	{
		GIsRequestingExit = TRUE;
	}

	if( PlayCount > 0 )
	{
		// Play while new loop count.
		LoopURL.AddOption( *FString::Printf(TEXT("playcount=%i"),PlayCount) );
		
		// Start over again.
		GWorld->Exec( *(FString(TEXT("DEMOPLAY "))+(*LoopURL.String())), *GLog );
	}
}

void UDemoRecDriver::TickDispatch( FLOAT DeltaTime )
{
	Super::TickDispatch( DeltaTime );

	if( ServerConnection && (ServerConnection->State==USOCK_Pending || ServerConnection->State==USOCK_Open) )
	{	
		BYTE Data[PACKETSIZE + 8];
		// Read data from the demo file
		DWORD PacketBytes;
		INT PlayedThisTick = 0;
		for( ; ; )
		{
			// At end of file?
			if( FileAr->AtEnd() || FileAr->IsError() )
			{
				DemoPlaybackEnded();
				return;
			}
	
			INT ServerFrameNum;
			FLOAT ServerDeltaTime;

			*FileAr << ServerDeltaTime;
			*FileAr << ServerFrameNum;
			if( ServerFrameNum > FrameNum )
			{
				FileAr->Seek(FileAr->Tell() - sizeof(ServerFrameNum) - sizeof(ServerDeltaTime));
				break;
			}
			*FileAr << PacketBytes;

			if( PacketBytes )
			{
				// Read data from file.
				FileAr->Serialize( Data, PacketBytes );
				if( FileAr->IsError() )
				{
					debugf( NAME_DevNet, TEXT("Failed to read demo file packet") );
					DemoPlaybackEnded();
					return;
				}

				// Update stats.
				PlayedThisTick++;

				// Process incoming packet.
				ServerConnection->ReceivedRawPacket( Data, PacketBytes );
			}

			if (ServerConnection == NULL || ServerConnection->State == USOCK_Closed)
			{
				// something we received resulted in the demo being stopped
				DemoPlaybackEnded();
				return;
			}

			// Only play one packet per tick on demo playback, until we're 
			// fully connected.  This is like the handshake for net play.
			if(ServerConnection->State == USOCK_Pending)
			{
				FrameNum = ServerFrameNum;
				break;
			}
		}
	}
}

struct FDemoRewindPointWriter : public FArchiveSaveCompressedProxy
{
public:
	FDemoRewindPointWriter(UDemoRecDriver* DemoDriver, TArray<BYTE>& InBytes)
	: FArchiveSaveCompressedProxy(InBytes, (ECompressionFlags)(COMPRESS_LZO|COMPRESS_BiasSpeed))
	{
		ArShouldSkipBulkData = TRUE;

		checkSlow(DemoDriver != NULL);
		checkSlow(DemoDriver->ServerConnection != NULL);
		checkSlow(GWorld != NULL);

		// remember where we were in the demo file
		INT DemoPos = DemoDriver->FileAr->Tell();
		*this << DemoPos;
		*this << DemoDriver->FrameNum;
		// remember networking packet indices
		*this << DemoDriver->ServerConnection->InPacketId;
		for (INT i = 0; i < ARRAY_COUNT(DemoDriver->ServerConnection->InReliable); i++)
		{
			*this << DemoDriver->ServerConnection->InReliable[i];
		}
		//@todo: serialize packagemap
		//@todo: serialize streamed levels
		// serialize number of actors
		INT NumActors = FActorIteratorBase::GetActorCount();
		*this << NumActors;
		// serialize all actors
		// manually iterate every Actor entry, even NULL entries, so that we guarantee the above count is correct
		for (INT i = 0; i < GWorld->Levels.Num(); i++)
		{
			ULevel* Level = GWorld->Levels(i);
			for (INT j = 0; j < Level->Actors.Num(); j++)
			{
				AActor* Actor = Level->Actors(j);
				*this << Actor;
				NumActors--;
			}
		}
		checkf(NumActors == 0, TEXT("Actor count vs iterator mismatch (Expected: %i, Got: %i)"), FActorIteratorBase::GetActorCount(), FActorIteratorBase::GetActorCount() - NumActors);

		// serialize channel list
		INT NumChannels = DemoDriver->ServerConnection->OpenChannels.Num();
		*this << NumChannels;
		for (INT i = 0; i < NumChannels; i++)
		{
			UChannel* Channel = DemoDriver->ServerConnection->OpenChannels(i);
			// serialize info required to create the appropriate channel
			BYTE ChType = Channel->ChType;
			*this << Channel->ChIndex << ChType << Channel->OpenedLocally;
			if (Channel->ChType == CHTYPE_Actor)
			{
				// serialize actor channel specific info
				checkSlow(Channel->IsA(UActorChannel::StaticClass()));
				UActorChannel* ActorChan = (UActorChannel*)Channel;
				*this << ActorChan->Actor;
				if (ActorChan->Actor != NULL)
				{
					for (INT j = 0; j < ActorChan->Retirement.Num(); j++)
					{
						*this << ActorChan->Retirement(j).InPacketId << ActorChan->Retirement(j).OutPacketId << ActorChan->Retirement(j).Reliable;
					}
				}
			}
			// serialize general info
			DWORD Broken = Channel->Broken;
			DWORD bTornOff = Channel->bTornOff;
			*this << Broken << bTornOff << Channel->OpenPacketId;
		}
	}

	virtual FArchive& operator<<(FName& N)
	{
		NAME_INDEX Index = N.GetIndex();
		INT Number = N.GetNumber();
		*this << Index << Number;
		return *this;
	}

	FArchive& operator<<(UObject*& Obj)
	{
		// if it's pending kill, write "None"
		if (Obj == NULL || Obj->IsPendingKill())
		{
			BYTE bIsActor = FALSE;
			*this << bIsActor;
			
			BYTE ObjChainCount = 0;
			*this << ObjChainCount;
		}
		else if (Cast<AActor>(Obj) != NULL && !Obj->IsTemplate() && !Obj->HasAnyFlags(RF_Transient))
		{
			// write that this is an Actor, which means we can potentially create it
			BYTE bIsActor = TRUE;
			*this << bIsActor;
			
			// everything in a living Actor's path name is guaranteed except the outermost and its own name, so record just those two as FNames
			FName PackageName = Obj->GetOutermost()->GetFName();
			FName ObjName = Obj->GetFName();
			*this << PackageName << ObjName;

			if (!SerializedObjects.HasKey(Obj))
			{
				//debugf(TEXT(" - serializing %s"), *Obj->GetPathName());
				SerializedObjects.AddItem(Obj);
				// serialize class path so it can be recreated if necessary
				FString ClassName(Obj->GetClass()->GetPathName());
				*this << ClassName;
				// don't write properties for static, not net relevant actors - they shouldn't change
				AActor* Actor = (AActor*)(Obj);
				BYTE bWriteProperties = (!Actor->IsStatic() || Actor->Role > ROLE_None);
				*this << bWriteProperties;
				if (bWriteProperties)
				{
					Actor->Serialize(*this);
				}
			}
		}
		// otherwise just write the full name and hope it works
		else
		{
			BYTE bIsActor = FALSE;
			*this << bIsActor;

			// write as a series of FNames for better performance
			BYTE ObjChainCount = 0;
			FName ObjNames[8];
			for (UObject* CurrentObj = Obj; CurrentObj != NULL; CurrentObj = CurrentObj->GetOuter())
			{
				ObjNames[ObjChainCount] = CurrentObj->GetFName();
				if (++ObjChainCount >= ARRAY_COUNT(ObjNames))
				{
					appErrorf(TEXT("Object %s exceeds %i outers in chain, unable to save in rewind point"), *Obj->GetPathName(), ARRAY_COUNT(ObjNames));
				}
			}
			*this << ObjChainCount;
			// write in reverse order so we can look up the outers as we're reading them
			for (INT i = ObjChainCount - 1; i >= 0; i--)
			{
				*this << ObjNames[i];
			}
		}

		return *this;
	}
private:
	/** List of objects that have already been serialized */
	TLookupMap<UObject*> SerializedObjects;
};

struct FDemoRewindPointReader : public FArchiveLoadCompressedProxy
{
public:
	FDemoRewindPointReader(UDemoRecDriver* DemoDriver, TArray<BYTE>& InBytes)
	: FArchiveLoadCompressedProxy(InBytes, COMPRESS_LZO)
	{
		ArShouldSkipBulkData = TRUE;

		DWORD Time = 0;
		CLOCK_CYCLES(Time);
		checkSlow(DemoDriver != NULL);
		checkSlow(DemoDriver->ServerConnection != NULL);
		checkSlow(GWorld != NULL);

		// save off some playback properties for which we want to ignore the recorded values
		AWorldInfo* Info = GWorld->GetWorldInfo();
		FLOAT DemoPlayTimeDilation = Info->DemoPlayTimeDilation;
		APlayerReplicationInfo* Pauser = Info->Pauser;
		UBOOL bPlayersOnly = Info->bPlayersOnly;

		// return to the old position in the demo file
		INT DemoPos = INDEX_NONE;
		*this << DemoPos;
		DemoDriver->FileAr->Seek(DemoPos);
		*this << DemoDriver->FrameNum;
		// restore networking packet indices
		*this << DemoDriver->ServerConnection->InPacketId;
		for (INT i = 0; i < ARRAY_COUNT(DemoDriver->ServerConnection->InReliable); i++)
		{
			*this << DemoDriver->ServerConnection->InReliable[i];
		}

		//@todo: update packagemap
		//@todo: update streamed levels

		// we need to GC so that we recreate required Actors that are currently pending kill
		UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, FALSE);

		// deserialize all the actors
		INT NumActors = 0;
		*this << NumActors;
		for (; NumActors > 0; NumActors--)
		{
			AActor* Actor = NULL;
			*this << Actor;
		}

		// delete any dynamic actors that weren't serialized as they weren't in the playback at this point in time
		//@FIXME: this breaks if some of these Actors attached components to other, still living Actors
		//			correct behavior is to serialize a list of Actors we're going to operate on and delete the rest at the beginning
		//			but I'm worried that's going to be too slow and take up too much memory
		//			let's wait and see how well the hack below gets the job done 
		for (FDynamicActorIterator It; It; ++It)
		{
			if (!It->bNoDelete && !It->HasAnyFlags(RF_Transient) && !SerializedObjects.HasKey(*It))
			{
				//@note: not using DestroyActor() here so that various notifications aren't called that could disrupt the restored state
				//		(basically treat it as if the Actor was simply GC'ed, as in level streaming)
				It->TermRBPhys(NULL);
				GWorld->RemoveActor(*It, TRUE);
				It->bDeleteMe = TRUE;
				It->MarkPendingKill();
				It->MarkComponentsAsPendingKill(FALSE);
				It->ClearComponents();
			}
		}
		//@HACK: workaround for above FIXME, iterate over the components that were inside destroyed actors and detach them
		for (TObjectIterator<UObject> It(TRUE); It; ++It)
		{
			if (It->GetOuter() != NULL && It->GetOuter()->HasAnyFlags(RF_PendingKill) && It->IsA(UActorComponent::StaticClass()))
			{
				((UActorComponent*)*It)->DetachFromAny();
			}
		}

		TArray<UChannel*> RemainingChannels = DemoDriver->ServerConnection->OpenChannels;
		// deserialize channel list
		INT NumChannels = 0;
		*this << NumChannels;
		for (INT i = 0; i < NumChannels; i++)
		{
			INT ChIndex = 0;
			BYTE ChType = CHTYPE_None;
			INT OpenedLocally = 0;
			// base info
			*this << ChIndex << ChType << OpenedLocally;
			UChannel* OldChannel = DemoDriver->ServerConnection->Channels[ChIndex];
			RemainingChannels.RemoveItem(OldChannel);
			UChannel* NewChannel = NULL;
			if (ChType == CHTYPE_Actor)
			{
				// serialize actor channel specific info
				AActor* Actor = NULL;
				*this << Actor;
				// see if we need to recreate this channel
				if (OldChannel == NULL || OldChannel->ChType != ChType || ((UActorChannel*)OldChannel)->Actor != Actor)
				{
					if (OldChannel != NULL)
					{
						if (OldChannel->ChType == ChType)
						{
							((UActorChannel*)OldChannel)->Actor = NULL; // so it doesn't destroy the old Actor (might still be relevant)
						}
						OldChannel->ConditionalCleanUp(); //@warning: OldChannel no longer valid for access
					}
					NewChannel = DemoDriver->ServerConnection->CreateChannel(EChannelType(ChType), OpenedLocally, ChIndex);
					check(NewChannel != NULL); //@FIXME: handle gracefully (kill demo playback)
					checkSlow(NewChannel->IsA(UActorChannel::StaticClass()));
					if (Actor != NULL)
					{
						((UActorChannel*)NewChannel)->SetChannelActor(Actor);
					}
				}
				else
				{
					NewChannel = OldChannel;
				}

				if (Actor != NULL)
				{
					checkSlow(NewChannel->IsA(UActorChannel::StaticClass()));
					UActorChannel* ActorChan = (UActorChannel*)NewChannel;
					for (INT j = 0; j < ActorChan->Retirement.Num(); j++)
					{
						*this << ActorChan->Retirement(j).InPacketId << ActorChan->Retirement(j).OutPacketId << ActorChan->Retirement(j).Reliable;
					}
				}
			}
			else if (OldChannel == NULL || OldChannel->ChType != ChType)
			{
				if (OldChannel != NULL)
				{
					OldChannel->ConditionalCleanUp(); //@warning: OldChannel no longer valid for access
				}
				NewChannel = DemoDriver->ServerConnection->CreateChannel(EChannelType(ChType), OpenedLocally, ChIndex);
				check(NewChannel != NULL); //@FIXME: handle gracefully (kill demo playback)
			}
			else
			{
				NewChannel = OldChannel;
			}

			// serialize general info
			DWORD Broken = 0;
			DWORD bTornOff = 0;
			*this << Broken << bTornOff << NewChannel->OpenPacketId;
			NewChannel->Broken = Broken;
			NewChannel->bTornOff = bTornOff;
		}
		// destroy channels that weren't mentioned in the data
		for (INT i = 0; i < RemainingChannels.Num(); i++)
		{
			RemainingChannels(i)->ConditionalCleanUp();
		}

		// restore properties we don't want to use the recorded values for
		Info->DemoPlayTimeDilation = DemoPlayTimeDilation;
		Info->Pauser = Pauser;
		Info->bPlayersOnly = bPlayersOnly;

		// notify relevant Actors that we have rewound the demo
		for (TLookupMap<UObject*>::TIterator It(SerializedObjects); It; ++It)
		{
			AActor* Actor = (AActor*)It.Key();
			if (!Actor->IsStatic() || Actor->Role > ROLE_None)
			{
				Actor->eventPostDemoRewind();
			}
		}

		UNCLOCK_CYCLES(Time);
		debugf(TEXT("Rewinding demo took %f"), FLOAT(Time * GSecondsPerCycle * 1000.0f));
	}

	virtual FArchive& operator<<(FName& N)
	{
		NAME_INDEX Index = INDEX_NONE;
		INT Number = INDEX_NONE;
		*this << Index << Number;
		N = FName(EName(Index), Number);
		return *this;
	}

	FArchive& operator<<(UObject*& Obj)
	{
		BYTE bIsActor = 0;
		*this << bIsActor;
		if (!bIsActor)
		{
			BYTE ObjChainCount = 0;
			*this << ObjChainCount;
			if (ObjChainCount == 0)
			{
				Obj = NULL;
			}
			else
			{
				UObject* CurrentObj = NULL;
				UBOOL bBroken = FALSE;
				while (ObjChainCount > 0)
				{
					FName NextName;
					*this << NextName;
					if (!bBroken)
					{
						CurrentObj = UObject::StaticFindObjectFast(UObject::StaticClass(), CurrentObj, NextName, FALSE);
						bBroken = (CurrentObj == NULL);
					}
					ObjChainCount--;
				}
				// if the object wasn't found, leave the original
				// this is so that inline components don't get clobbered when the recorded Actor needs to be respawned (since all the component names will be different)
				if (CurrentObj != NULL)
				{
					Obj = CurrentObj;
				}
			}
		}
		else
		{
			FName PackageName, ObjName;
			*this << PackageName << ObjName;
			// avoid re-creating Actors unnecessarily to improve performance
			// find each object in the chain one at a time so we don't have to use FString construction and can use StaticFindObjectFast()
			//@warning: assumption about Actor outer chain and naming scheme
			Obj = NULL;
			UObject* Outermost = UObject::StaticFindObjectFast(UPackage::StaticClass(), NULL, PackageName);
			if (Outermost != NULL)
			{
				UObject* World = UObject::StaticFindObjectFast(UWorld::StaticClass(), Outermost, NAME_TheWorld);
				if (World != NULL)
				{
					UObject* Level = UObject::StaticFindObjectFast(ULevel::StaticClass(), World, NAME_PersistentLevel);
					if (Level != NULL)
					{
						Obj = UObject::StaticFindObjectFast(AActor::StaticClass(), Level, ObjName);
					}
				}
			}
			UBOOL bSerializedClassName = FALSE;
			if (Obj == NULL)
			{
				// create it
				FString ClassName;
				*this << ClassName;
				bSerializedClassName = TRUE;
				//@warning: assumption that all such Actors were in the persistent level
				Obj = GWorld->SpawnActor(FindObject<UClass>(NULL, *ClassName), ObjName, FVector(0,0,0), FRotator(0,0,0), NULL, TRUE, TRUE, NULL, NULL, TRUE);
				check(Obj != NULL);
			}
			if (!SerializedObjects.HasKey(Obj))
			{
				//debugf(TEXT(" - serializing %s"), *Obj->GetPathName());
				SerializedObjects.AddItem(Obj);
				if (!bSerializedClassName)
				{
					// read the class name and throw it away as it isn't needed
					FString ClassName;
					*this << ClassName;
					bSerializedClassName = TRUE;
				}
				// see if we need to deserialize the properties for this object
				BYTE bWriteProperties = 0;
				*this << bWriteProperties;
				if (bWriteProperties)
				{
					AActor* Actor = (AActor*)Obj;
					Actor->ClearComponents();
					// remove from current Owner's Children array
					if (Actor->Owner != NULL)
					{
						verifySlow(Actor->Owner->Children.RemoveItem(Actor) == 1);
					}
					// consider components array transient
					//@todo: maybe should also add any components that were successfully serialized?
					TArray<UActorComponent*> OldComponents = Actor->Components;
					Actor->Serialize(*this);
					Actor->Components = OldComponents;
					// handle updating Owner's Children as that array is transient
					if (Actor->Owner != NULL)
					{
						checkSlow(!Actor->Owner->Children.ContainsItem(Actor));
						Actor->Owner->Children.AddItem(Actor);
					}
					Actor->ForceUpdateComponents(FALSE, FALSE);
				}
			}
		}

		return *this;
	}

private:
	/** List of objects that have already been serialized */
	TLookupMap<UObject*> SerializedObjects;
};

void UDemoRecDriver::TickFlush()
{
	Super::TickFlush();

	// see if we should save a rewind point now
	if ( MaxRewindPoints > 0 && RewindPointInterval > 0.0f && ServerConnection != NULL && ServerConnection->State == USOCK_Open &&
		GWorld != NULL && GWorld->GetTimeSeconds() - LastRewindPointTime > RewindPointInterval )
	{
		LastRewindPointTime = GWorld->GetTimeSeconds();
		// don't actually record a rewind point now if we already have one for this demo frame or we recorded a frame further along
		if (RewindPoints.Num() == 0 || RewindPoints.Last().FrameNum < FrameNum)
		{
			if (RewindPoints.Num() >= MaxRewindPoints)
			{
				// we need to remove a rewind point
				// the approach here is to remove every other rewind point,
				// looping back to the beginning when we reach the set of protected "recent" rewind points at the end
				if (RewindPoints.Num() == 1 || MaxRewindPoints <= NumRecentRewindPoints)
				{
					// configuration doesn't allow us to do anything but remove the oldest rewind point
					RewindPoints.Remove(0);
				}
				else
				{
					LastRewindPointCulled += 1; // this is effectively 2 away from the last removed entry since the elements would have shifted
					if (LastRewindPointCulled >= MaxRewindPoints - NumRecentRewindPoints)
					{
						LastRewindPointCulled = 1;
					}
					RewindPoints.Remove(LastRewindPointCulled);
				}
			}
			// create and serialize the rewind point
			FDemoRewindPoint* NewRewindPoint = new(RewindPoints) FDemoRewindPoint(FrameNum);
			FDemoRewindPointWriter Ar(this, NewRewindPoint->Data);
			debugf(NAME_DevNet, TEXT("Demo playback wrote rewind point (%i bytes)"), NewRewindPoint->Data.Num());
		}
	}
}

FString UDemoRecDriver::LowLevelGetNetworkNumber(UBOOL bAppendPort/*=FALSE*/)
{
	return TEXT("");
}

UBOOL UDemoRecDriver::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( bHasDemoEnded )
	{
		return 0;
	}
	if( ParseCommand(&Cmd,TEXT("DEMOREC")) || ParseCommand(&Cmd,TEXT("DEMOPLAY")) )
	{
		if( ServerConnection )
		{
			Ar.Logf( TEXT("Demo playback currently active: %s"), *DemoFilename );//@todo demorec: localize
		}
		else
		{
			Ar.Logf( TEXT("Demo recording currently active: %s"), *DemoFilename );//@todo demorec: localize
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("DEMOSTOP")) )
	{
		PlayCount = 0;
		Ar.Logf( TEXT("Demo %s stopped at frame %d"), *DemoFilename, FrameNum );//@todo demorec: localize
		if( !ServerConnection )
		{
			// write the total number of frames in the placeholder at the beginning of the file
			if (FileAr != NULL && GWorld != NULL)
			{
				PlaybackTotalFrames = FrameNum;
				INT OldPos = FileAr->Tell();
				FileAr->Seek(sizeof(INT) + sizeof(INT)); //@see: UDemoRecDriver::InitListen()
				(*FileAr) << PlaybackTotalFrames;
				FileAr->Seek(OldPos);
			}
			// let GC cleanup the object
			if (ClientConnections.Num() > 0 && ClientConnections(0) != NULL)
			{
				ClientConnections(0)->Close();
				ClientConnections(0)->CleanUp(); // make sure DemoRecSpectator gets destroyed immediately
			}

			GWorld->DemoRecDriver=NULL;
		}
		else
		{
			// flush out any pending network traffic
			ServerConnection->FlushNet();
			ServerConnection->State = USOCK_Closed;
			GEngine->SetClientTravel(TEXT("?closed"), TRAVEL_Absolute);
		}

		delete FileAr;
		FileAr = NULL;
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("DEMOREWIND")))
	{
		if (RewindPoints.Num() == 0 || FileAr == NULL || ServerConnection == NULL || ServerConnection->State != USOCK_Open || GWorld == NULL)
		{
			Ar.Logf(TEXT("Demo rewind not available"));
		}
		else
		{
			// figure out the rewind point we're currently on
			INT CurrentIndex = RewindPoints.Num();
			for (INT i = 0; i < RewindPoints.Num(); i++)
			{
				if (RewindPoints(i).FrameNum >= FrameNum)
				{
					CurrentIndex = i;
					break;
				}
			}
			// figure out the rewind point we want to go to
			INT JumpAmount = appAtoi(Cmd);
			if (JumpAmount == 0)
			{
				JumpAmount = 1;
			}
			INT RewindIndex = Clamp<INT>(CurrentIndex - JumpAmount, 0, RewindPoints.Num() - 1);
			// apply it
			FDemoRewindPointReader Ar(this, RewindPoints(RewindIndex).Data);
		}

		return TRUE;
	}
	else 
	{
		return Super::Exec(Cmd, Ar);
	}
}

void UDemoRecDriver::SpawnDemoRecSpectator( UNetConnection* Connection )
{
	UClass* C = StaticLoadClass( AActor::StaticClass(), NULL, *DemoSpectatorClass, NULL, LOAD_None, NULL );
	APlayerController* Controller = CastChecked<APlayerController>(GWorld->SpawnActor( C ));

	for (FActorIterator It; It; ++It)
	{
		if (It->IsA(APlayerStart::StaticClass()))
		{
			Controller->Location = It->Location;
			Controller->Rotation = It->Rotation;
			break;
		}
	}

	Controller->SetPlayer(Connection);
}
IMPLEMENT_CLASS(UDemoRecDriver);

void AWorldInfo::GetDemoFrameInfo(INT* CurrentFrame, INT* TotalFrames)
{
	if (GWorld->DemoRecDriver != NULL && GWorld->DemoRecDriver->ServerConnection != NULL)
	{
		if (CurrentFrame != NULL)
		{
			*CurrentFrame = GWorld->DemoRecDriver->FrameNum;
		}
		if (TotalFrames != NULL)
		{
			*TotalFrames = 	GWorld->DemoRecDriver->PlaybackTotalFrames;
		}
	}
	else
	{
		if (CurrentFrame != NULL)
		{
			*CurrentFrame = -1;
		}
		if (TotalFrames != NULL)
		{
			*TotalFrames = -1;
		}
	}
}

UBOOL AWorldInfo::GetDemoRewindPoints(TArray<INT>& OutRewindPoints)
{
	if (GWorld->DemoRecDriver != NULL && GWorld->DemoRecDriver->ServerConnection != NULL && GWorld->DemoRecDriver->RewindPoints.Num() > 0)
	{
		OutRewindPoints.Reset();
		for (INT i = 0; i < GWorld->DemoRecDriver->RewindPoints.Num(); i++)
		{
			OutRewindPoints.AddItem(GWorld->DemoRecDriver->RewindPoints(i).FrameNum);
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*-----------------------------------------------------------------------------
	UDemoPlayPendingLevel implementation.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
UDemoPlayPendingLevel::UDemoPlayPendingLevel(const FURL& InURL)
:	UPendingLevel( InURL )
{
	NetDriver = NULL;

	// Try to create demo playback driver.
	UClass* DemoDriverClass = StaticLoadClass( UDemoRecDriver::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.DemoRecordingDevice"), NULL, LOAD_None, NULL );
	DemoRecDriver = ConstructObject<UDemoRecDriver>( DemoDriverClass );
	if( DemoRecDriver->InitConnect( this, URL, ConnectionError ) )
	{
	}
	else
	{
		//@todo ronp connection
		// make sure this failure is propagated to the game
		DemoRecDriver = NULL;
	}
}

//
// FNetworkNotify interface.
//
void UDemoPlayPendingLevel::NotifyControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch)
{
#if !SHIPPING_PC_GAME
	debugf(NAME_DevNet, TEXT("DemoPlayPendingLevel received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
	switch (MessageType)
	{
		case NMT_Uses:
		{
			// Dependency information.
			FPackageInfo& Info = *new(Connection->PackageMap->List)FPackageInfo(NULL);
			Connection->ParsePackageInfo(Bunch, Info);

#if !SHIPPING_PC_GAME
			debugf(NAME_DevNet, TEXT(" ---> GUID: %s, Generation: %i"), *Info.Guid.String(), Info.RemoteGeneration);
#endif
			// in the seekfree loading case, we load the requested map first and then attempt to load requested packages that haven't been loaded yet
			// as packages referenced by the map might be forced exports and not actually have a file associated with them
			//@see UGameEngine::LoadMap()
			//@todo: figure out some early-out code to detect when missing downloadable content, etc so we don't have to load the level first
			if( !GUseSeekFreeLoading )
			{
				// verify that we have this package, or it is downloadable
				FString Filename;
				if (GPackageFileCache->FindPackageFile(*Info.PackageName.ToString(), DemoRecDriver->bShouldSkipPackageChecking ? NULL : &Info.Guid, Filename))
				{
					Info.Parent = CreatePackage(NULL, *Info.PackageName.ToString());
					// check that the GUID matches (meaning it is the same package or it has been conformed)
					BeginLoad();
					ULinkerLoad* Linker = GetPackageLinker(Info.Parent, NULL, LOAD_NoWarn | LOAD_NoVerify | LOAD_Quiet, NULL, DemoRecDriver->bShouldSkipPackageChecking ? NULL : &Info.Guid);
					EndLoad();
					if (Linker == NULL || (!DemoRecDriver->bShouldSkipPackageChecking && Linker->Summary.Guid != Info.Guid))
					{
						// incompatible files
						debugf(NAME_DevNet, TEXT("Package '%s' mismatched - Server GUID: %s Client GUID: %s"), *Info.Parent->GetName(), *Info.Guid.String(), (Linker != NULL) ? *Linker->Summary.Guid.String() : TEXT("None"));
						ConnectionError = FString::Printf(TEXT("Package '%s' version mismatch"), *Info.Parent->GetName());
						DemoRecDriver->ServerConnection->Close();
						return;
					}
					else
					{
						Info.LocalGeneration = Linker->Summary.Generations.Num();
						if (Info.LocalGeneration < Info.RemoteGeneration)
						{
							// the indices will be mismatched in this case as there's no real server to adjust them for our older package version
							debugf(NAME_DevNet, TEXT("Package '%s' mismatched for demo playback - local version: %i, demo version: %i"), *Info.Parent->GetName(), Info.LocalGeneration, Info.RemoteGeneration);
							ConnectionError = FString::Printf(TEXT("Package '%s' version mismatch"), *Info.Parent->GetName());
							DemoRecDriver->ServerConnection->Close();
							return;
						}
					}
				}
				else
				{
					// we need to download this package
					FilesNeeded++;
					Info.PackageFlags |= PKG_Need;
					/*@todo:
					if (DemoRecDriver->ClientRedirectURLs.Num()==0 || !DemoRecDriver->AllowDownloads || !(Info.PackageFlags & PKG_AllowDownload))
					*/
					if (TRUE)
					{
						ConnectionError = FString::Printf(TEXT("Demo requires missing/mismatched package '%s'"), *Info.PackageName.ToString());
						DemoRecDriver->ServerConnection->Close();
						return;
					}
				}
			}
			break;
		}
		case NMT_Welcome:
		{
			// Parse welcome message.
			FString GameName;
			FNetControlMessage<NMT_Welcome>::Receive(Bunch, URL.Map, GameName);

			/*@todo:
			if (FilesNeeded > 0)
			{
				// Send first download request.
				ReceiveNextFile( Connection, 0 );
			}*/

			DemoRecDriver->Time = 0;
			bSuccessfullyConnected = TRUE;
			break;
		}
	}
}

//
// UPendingLevel interface.
//
void UDemoPlayPendingLevel::Tick( FLOAT DeltaTime )
{
	check(DemoRecDriver);
	check(DemoRecDriver->ServerConnection);

	if( DemoRecDriver->ServerConnection && DemoRecDriver->ServerConnection->Download )
	{
		DemoRecDriver->ServerConnection->Download->Tick();
	}

	if( !FilesNeeded )
	{
		// Update demo recording driver.
		DemoRecDriver->UpdateDemoTime( &DeltaTime, 1.f );
		DemoRecDriver->TickDispatch( DeltaTime );
		DemoRecDriver->TickFlush();
	}
}

UNetDriver* UDemoPlayPendingLevel::GetDriver()
{
	return DemoRecDriver;
}

IMPLEMENT_CLASS(UDemoPlayPendingLevel);

