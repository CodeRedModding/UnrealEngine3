/*=============================================================================
	UnLevel.cpp: Level-related functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "EngineSequenceClasses.h"
#include "UnTerrain.h"
#include "UnStatChart.h"

void ULineBatchComponent::DrawLine(const FVector& Start,const FVector& End,FColor Color)
{
	new(BatchedLines) FLine(Start,End,Color,DefaultLifeTime);
}

void ULineBatchComponent::DrawLine(const FVector& Start,const FVector& End,FColor Color,FLOAT LifeTime)
{
	new(BatchedLines) FLine(Start,End,Color,LifeTime);
}

void ULineBatchComponent::Tick(FLOAT DeltaTime)
{
	// Update the life time of batched lines, removing the lines which have expired.
	for(INT LineIndex = 0;LineIndex < BatchedLines.Num();LineIndex++)
	{
		FLine& Line = BatchedLines(LineIndex);
		if(Line.RemainingLifeTime > 0.0f)
		{
			Line.RemainingLifeTime -= DeltaTime;
			if(Line.RemainingLifeTime <= 0.0f)
			{
				// The line has expired, remove it.
				BatchedLines.Remove(LineIndex--);
			}
		}
	}
}

void ULineBatchComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	for(UINT LineIndex = 0;LineIndex < (UINT)BatchedLines.Num();LineIndex++)
	{
		FLine&	Line = BatchedLines(LineIndex);
		PRI->DrawLine(Line.Start,Line.End,Line.Color);
	}
}

/*-----------------------------------------------------------------------------
	ULevelBase implementation.
-----------------------------------------------------------------------------*/

ULevelBase::ULevelBase( UEngine* InEngine, const FURL& InURL )
:	URL( InURL )
,	Engine( InEngine )
,	Actors( this )
{}
void ULevelBase::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	if( Ar.IsTrans() )
	{
		Ar << Actors;
	}
	else
	{
		//oldver Old-format actor list.
		INT DbNum=Actors.Num(), DbMax=DbNum;
		Actors.CountBytes( Ar );
		Ar << DbNum << DbMax;
		if( Ar.IsLoading() )
		{
			Actors.Empty( DbNum );
			Actors.Add( DbNum );
		}
		for( INT i=0; i<Actors.Num(); i++ )
			Ar << Actors(i);
	}
	
	// Level variables.
	Ar << URL;
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << NetDriver;
	}

}
void ULevelBase::Destroy()
{
	if( NetDriver )
	{
		delete NetDriver;
		NetDriver = NULL;
	}
	Super::Destroy();
}
void ULevelBase::NotifyProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
	Engine->SetProgress( Str1, Str2, Seconds );
}
IMPLEMENT_CLASS(ULevelBase);

/*-----------------------------------------------------------------------------
	Level creation & emptying.
-----------------------------------------------------------------------------*/

//
//	Create a new level and allocate all objects needed for it.
//	Call with Editor=1 to allocate editor structures for it, also.
//
ULevel::ULevel( UEngine* InEngine, UBOOL InRootOutside )
:	ULevelBase( InEngine )
{
	// Initialize the collision octree.
	Hash = GNewCollisionHash(this);

	// Allocate subobjects.
	SetFlags( RF_Transactional );
	Model = new( GetOuter() )UModel( NULL, InRootOutside );
	Model->SetFlags( RF_Transactional );

	// Spawn the level info.
	SpawnActor( ALevelInfo::StaticClass() );
	check(GetLevelInfo());

	// Spawn the default brush.
	ABrush* Temp = SpawnBrush();
	check(Temp==Actors(1));
	check(Temp->BrushComponent);
	Temp->Brush = new( GetOuter(), TEXT("Brush") )UModel( Temp, 1 );
	Temp->BrushComponent->Brush = Temp->Brush;
	Temp->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );
	Temp->Brush->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );

	// spawn default physics volume
	GetLevelInfo()->GetDefaultPhysicsVolume();

	UpdateComponents();
}
void ULevel::ShrinkLevel()
{
	Model->ShrinkModel();
}

/*-----------------------------------------------------------------------------
	Level locking and unlocking.
-----------------------------------------------------------------------------*/

//
// Modify this level.
//
void ULevel::Modify( UBOOL DoTransArrays )
{
	UObject::Modify();
	Model->Modify();
}
void ULevel::PostLoad()
{
	Super::PostLoad();
	for( TObjectIterator<AActor> It; It; ++It )
		if( It->GetOuter()==GetOuter() )
			It->XLevel = this;

	// Clean up existing LineBatchComponents.
	for(INT ActorIndex = 0;ActorIndex < Actors.Num();ActorIndex++)
	{
		AActor* Actor = Actors(ActorIndex);
		if(Actor)
		{
			for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
			{
				UActorComponent* Component = Actor->Components(ComponentIndex);
				if(Component && Component->IsA(ULineBatchComponent::StaticClass()))
				{
					check(!Component->Initialized);
					Actor->Components.Remove(ComponentIndex--);
				}
			}
		}
	}

	// Initialize the collision octree.
	Hash = GNewCollisionHash(this);
}

//
//	ULevel::ClearComponents
//

void ULevel::ClearComponents()
{
	// Remove the model components from the scene.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex) && ModelComponents(ComponentIndex)->Initialized)
		{
			ModelComponents(ComponentIndex)->Destroyed();
		}
	}

	// Remove the line batcher from the scene.
	if(LineBatcher)
	{
		if(LineBatcher->Initialized)
		{
			LineBatcher->Destroyed();
		}
	}
	if(PersistentLineBatcher)
	{
		if(PersistentLineBatcher->Initialized)
		{
			PersistentLineBatcher->Destroyed();
		}
	}

	// Remove the actors' components from the scene.
	for(UINT ActorIndex = 0;ActorIndex < (UINT)Actors.Num();ActorIndex++)
		if(Actors(ActorIndex))
			Actors(ActorIndex)->ClearComponents();
}

//
//	ULevel::UpdateComponents
//

void ULevel::UpdateComponents()
{
	// Create/update the level's BSP model components.

	if(!ModelComponents.Num())
	{
		if(Model->NumZones)
		{
			// Create a model component for the BSP nodes of each zone.
			for(INT ZoneIndex = 1;ZoneIndex < Max(Model->NumZones,1);ZoneIndex++)
			{
				UModelComponent*	ModelComponent = ConstructObject<UModelComponent>(UModelComponent::StaticClass(),this);
				ModelComponent->Level = this;
				ModelComponent->Model = Model;
				ModelComponent->ZoneIndex = ZoneIndex;
				ModelComponents.AddItem(ModelComponent);
			}
		}
		else
		{
			// If the level isn't zoned, create a single model component for all BSP nodes.
			// The BSP nodes may still have invalid zone indices, so use the special meaning of ZoneIndex = INDEX_NONE
			// to ignore the BSP node indices.
			UModelComponent*	ModelComponent = ConstructObject<UModelComponent>(UModelComponent::StaticClass(),this);
			ModelComponent->Level = this;
			ModelComponent->Model = Model;
			ModelComponent->ZoneIndex = INDEX_NONE;
			ModelComponents.AddItem(ModelComponent);
		}
	}
	else
	{
		for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents(ComponentIndex) && ModelComponents(ComponentIndex)->Initialized)
			{
				ModelComponents(ComponentIndex)->Destroyed();
			}
		}
	}

	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->Scene = this;
			ModelComponents(ComponentIndex)->SetParentToWorld(FMatrix::Identity);
			ModelComponents(ComponentIndex)->Created();
		}
	}

	// Create/update the level's line batcher.

	if(!LineBatcher)
	{
		LineBatcher = ConstructObject<ULineBatchComponent>(ULineBatchComponent::StaticClass());
	}
	else if(LineBatcher && LineBatcher->Initialized)
	{
		LineBatcher->Destroyed();
	}

	LineBatcher->Scene = this;
	LineBatcher->SetParentToWorld(FMatrix::Identity);
	LineBatcher->Created();

	if(!PersistentLineBatcher)
	{
		PersistentLineBatcher = ConstructObject<ULineBatchComponent>(ULineBatchComponent::StaticClass());
	}
	else if(PersistentLineBatcher && PersistentLineBatcher->Initialized)
	{
		PersistentLineBatcher->Destroyed();
	}

	PersistentLineBatcher->Scene = this;
	PersistentLineBatcher->SetParentToWorld(FMatrix::Identity);
	PersistentLineBatcher->Created();

	// Update the actor components.

	for(UINT ActorIndex = 0;ActorIndex < (UINT)Actors.Num();ActorIndex++)
	{
		if(Actors(ActorIndex))
		{
			Actors(ActorIndex)->ClearComponents();
			Actors(ActorIndex)->UpdateComponents();
		}
	}
}

void ULevel::InvalidateModelGeometry()
{
	// Delete the existing model components.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex)->Initialized)
			ModelComponents(ComponentIndex)->Destroyed();
		delete ModelComponents(ComponentIndex);
	}
	ModelComponents.Empty();

	// Create new model components.
	UpdateComponents();
}

void ULevel::InvalidateModelSurface(INT SurfIndex)
{
	// Find the zones which the surface has nodes in.
	TArray<INT>	Zones;
	for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
	{
		FBspNode&	Node = Model->Nodes(NodeIndex);
		if(Node.iSurf == SurfIndex)
			Zones.AddUniqueItem(Node.iZone[1]);
	}

	// Update the model components which contain BSP nodes in the dirty zones.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		if(ModelComponents(ComponentIndex)->ZoneIndex == INDEX_NONE || Zones.FindItemIndex(ModelComponents(ComponentIndex)->ZoneIndex) != INDEX_NONE)
			ModelComponents(ComponentIndex)->InvalidateSurfaces();
}

/*-----------------------------------------------------------------------------
	Level object implementation.
-----------------------------------------------------------------------------*/

void ULevel::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	FLOAT ApproxTime = (FLOAT)TimeSeconds;
	Ar << Model;
	Ar << ApproxTime;
	Ar << FirstDeleted;
	Ar << TravelInfo;

	if( Model && !Ar.IsTrans() )
		Ar.Preload( Model );

	if(!Ar.IsTrans())
		Ar << ModelComponents;

	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << LineBatcher << PersistentLineBatcher << DynamicPrimitives;
		FScene::Serialize(Ar);
	}

	Ar << GameSequence;

	Ar << EditorViews[0];
	Ar << EditorViews[1];
	Ar << EditorViews[2];
	Ar << EditorViews[3];

	if( Ar.Ver() >= 181 )
	{
		Ar << StaticStreamableTextureInfos;
	
		if( Ar.IsLoading() )
		{
			// Remove NULL references, can e.g. be caused by missing references.
			for( INT i=0; i<StaticStreamableTextureInfos.Num(); i++ )
			{
				if( StaticStreamableTextureInfos(i).Texture == NULL )
				{
					StaticStreamableTextureInfos.Remove( i-- );
				}
			}
		}
	}
}

/**
 * Presave function, gets called once before the level gets serialized (multiple times) for saving.
 * Used to rebuild streaming data on save.
 */
void ULevel::PreSave()
{
	UPackage* Package = CastChecked<UPackage>(GetOuter());
			
	// Don't rebuild streaming data if we're saving out a cooked package as the raw data required has already been stripped.
	if( !(Package->PackageFlags & PKG_Cooked) )
	{
		BuildStreamingData();
	}
}

void ULevel::Destroy()
{
	CleanupLevel();

	// Free allocated stuff.
	if( Hash )
	{
		delete Hash;
		Hash = NULL; /* Required because actors may try to unhash themselves. */
	}

	TermLevelRBPhys();

	Super::Destroy();
}
IMPLEMENT_CLASS(ULevel);

/*-----------------------------------------------------------------------------
	ULevel command-line.
-----------------------------------------------------------------------------*/

UBOOL ULevel::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( NetDriver && NetDriver->Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("SHOWEXTENTLINECHECK") ) )
	{
		bShowExtentLineChecks = !bShowExtentLineChecks;
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("SHOWLINECHECK") ) )
	{
		bShowLineChecks = !bShowLineChecks;
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("SHOWPOINTCHECK") ) )
	{
		bShowPointChecks = !bShowPointChecks;
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("FLUSHPERSISTENTDEBUGLINES") ) )
	{
		PersistentLineBatcher->BatchedLines.Empty();
		return 1;
	}
	else if( Hash->Exec(Cmd,Ar) )
		return 1;
	else if( ExecRBCommands( Cmd, &Ar, this ) )
		return 1;

	else return 0;
}

/*-----------------------------------------------------------------------------
	Utility functions.
-----------------------------------------------------------------------------*/

// Moves an actor to the floor.  Optionally will align with the trace normal.
UBOOL ULevel::ToFloor( AActor* InActor, UBOOL InAlign, AActor* InIgnoreActor )
{
	check( InActor );

	FVector BestLoc = FVector(0,0,-HALF_WORLD_MAX);
	FRotator SaveRot = FRotator(0,0,0);

	FVector Direction = FVector(0,0,-1);

	FVector	StartLocation = InActor->Location,
			LocationOffset = FVector(0,0,0),
			Extent = FVector(0,0,0);

	if(InActor->CollisionComponent && InActor->CollisionComponent->IsValidComponent())
	{
		check(InActor->CollisionComponent->Initialized);
		LocationOffset = InActor->CollisionComponent->Bounds.Origin - InActor->Location;
		StartLocation = InActor->CollisionComponent->Bounds.Origin;
		Extent = InActor->CollisionComponent->Bounds.BoxExtent;
	}

	FCheckResult Hit(1.f);
	if( !SingleLineCheck( Hit, InActor, StartLocation + Direction*WORLD_MAX, StartLocation, TRACE_World, Extent ) )
	{
		FVector	NewLocation = Hit.Location - LocationOffset;

		if( Hit.Actor == GetLevelInfo() )
		{
			FCheckResult PointCheckHit(1.f);
			if( !Hit.Actor->GetLevel()->Model->PointCheck( PointCheckHit, NULL, NewLocation, Extent ) )
				return 0;
		}

		FarMoveActor( InActor, NewLocation );
		if( InAlign )
		{
			// TODO: This doesn't take into account that rotating the actor changes LocationOffset.
			InActor->Rotation = Hit.Normal.Rotation();
			InActor->Rotation.Pitch -= 16384;
		}

		return 1;
	}

	return 0;

}

/*-----------------------------------------------------------------------------
	ULevel networking related functions.
-----------------------------------------------------------------------------*/

//
// Start listening for connections.
//
UBOOL ULevel::Listen( FString& Error )
{
	if( NetDriver )
	{
		Error = LocalizeError(TEXT("NetAlready"),TEXT("Engine"));
		return 0;
	}
	if( !GetLinker() )
	{
		Error = LocalizeError(TEXT("NetListen"),TEXT("Engine"));
		return 0;
	}

	// Create net driver.
	UClass* NetDriverClass = StaticLoadClass( UNetDriver::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.NetworkDevice"), NULL, LOAD_NoFail, NULL );
	NetDriver = (UNetDriver*)StaticConstructObject( NetDriverClass );
	if( !NetDriver->InitListen( this, URL, Error ) )
	{
		debugf( TEXT("Failed to listen: %s"), *Error );
		delete NetDriver;
		NetDriver=NULL;
		return 0;
	}
	static UBOOL LanPlay = ParseParam(appCmdLine(),TEXT("lanplay"));
	if ( !LanPlay && (NetDriver->MaxInternetClientRate < NetDriver->MaxClientRate) && (NetDriver->MaxInternetClientRate > 2500) )
		NetDriver->MaxClientRate = NetDriver->MaxInternetClientRate;

	if ( GetLevelInfo()->Game && (GetLevelInfo()->Game->MaxPlayers > 16) )
		NetDriver->MaxClientRate = ::Min(NetDriver->MaxClientRate, 10000);

	// Load everything required for network server support.
	UGameEngine* GameEngine = CastChecked<UGameEngine>( Engine );
	GameEngine->BuildServerMasterMap( NetDriver, this );

	// Spawn network server support.
	for( INT i=0; i<GameEngine->ServerActors.Num(); i++ )
	{
		TCHAR Str[240];
		const TCHAR* Ptr = *GameEngine->ServerActors(i);
		if( ParseToken( Ptr, Str, ARRAY_COUNT(Str), 1 ) )
		{
			debugf(NAME_DevNet, TEXT("Spawning: %s"), Str );
			UClass* HelperClass = StaticLoadClass( AActor::StaticClass(), NULL, Str, NULL, LOAD_NoFail, NULL );
/*FIXMESTEVE
#if DEMOVERSION
			UPackage* Package = CastChecked<UPackage>(HelperClass->GetOuter());
			if ( Package->GetFName() != NAME_UWeb && Package->GetFName() != NAME_IpDrv )
				appThrowf(TEXT("%s is not allowed in the demo"), HelperClass->GetFullName());
#endif
*/
			AActor* Actor = SpawnActor( HelperClass );
			while( Actor && ParseToken(Ptr,Str,ARRAY_COUNT(Str),1) )
			{
				TCHAR* Value = appStrchr(Str,'=');
				if( Value )
				{
					*Value++ = 0;
					for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Actor->GetClass()); It; ++It )
						if
						(	appStricmp(It->GetName(),Str)==0
						&&	(It->PropertyFlags & CPF_Config) )
							It->ImportText( Value, (BYTE*)Actor + It->Offset, 0, Actor );
				}
			}
		}
	}

	// Set LevelInfo properties.
	GetLevelInfo()->NetMode = Engine->Client ? NM_ListenServer : NM_DedicatedServer;
	GetLevelInfo()->NextSwitchCountdown = NetDriver->ServerTravelPause;

	return 1;
}

//
// Return whether this level is a server.
//
UBOOL ULevel::IsServer()
{
	return !NetDriver || !NetDriver->ServerConnection;
}

/*-----------------------------------------------------------------------------
	ULevel network notifys.
-----------------------------------------------------------------------------*/

//
// The network driver is about to accept a new connection attempt by a
// connectee, and we can accept it or refuse it.
//
EAcceptConnection ULevel::NotifyAcceptingConnection()
{
	check(NetDriver);
	if( NetDriver->ServerConnection )
	{
		// We are a client and we don't welcome incoming connections.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Client refused") );
		return ACCEPTC_Reject;
	}
	else if( GetLevelInfo()->NextURL!=TEXT("") )
	{
		// Server is switching levels.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Server %s refused"), GetName() );
		return ACCEPTC_Ignore;
	}
	else
	{
		// Server is up and running.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Server %s accept"), GetName() );
		return ACCEPTC_Accept;
	}
}

//
// This server has accepted a connection.
//
void ULevel::NotifyAcceptedConnection( UNetConnection* Connection )
{
	check(NetDriver!=NULL);
	check(NetDriver->ServerConnection==NULL);
	debugf( NAME_NetComeGo, TEXT("Open %s %s %s"), GetName(), appTimestamp(), *Connection->LowLevelGetRemoteAddress() );
}

//
// The network interface is notifying this level of a new channel-open
// attempt by a connectee, and we can accept or refuse it.
//
UBOOL ULevel::NotifyAcceptingChannel( UChannel* Channel )
{
	check(Channel);
	check(Channel->Connection);
	check(Channel->Connection->Driver);
	UNetDriver* Driver = Channel->Connection->Driver;

	if( Driver->ServerConnection )
	{
		// We are a client and the server has just opened up a new channel.
		//debugf( "NotifyAcceptingChannel %i/%i client %s", Channel->ChIndex, Channel->ChType, GetName() );
		if( Channel->ChType==CHTYPE_Actor )
		{
			// Actor channel.
			//debugf( "Client accepting actor channel" );
			return 1;
		}
		else
		{
			// Unwanted channel type.
			debugf( NAME_DevNet, TEXT("Client refusing unwanted channel of type %i"), (BYTE)Channel->ChType );
			return 0;
		}
	}
	else
	{
		// We are the server.
		if( Channel->ChIndex==0 && Channel->ChType==CHTYPE_Control )
		{
			// The client has opened initial channel.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel Control %i server %s: Accepted"), Channel->ChIndex, *GetFullName() );
			return 1;
		}
		else if( Channel->ChType==CHTYPE_File )
		{
			// The client is going to request a file.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel File %i server %s: Accepted"), Channel->ChIndex, *GetFullName() );
			return 1;
		}
		else
		{
			// Client can't open any other kinds of channels.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel %i %i server %s: Refused"), (BYTE)Channel->ChType, Channel->ChIndex, *GetFullName() );
			return 0;
		}
	}
}

//
// Welcome a new player joining this server.
//
#if UNICODE
void ULevel::WelcomePlayer( UNetConnection* Connection, TCHAR* Optional )
#else
void ULevel::WelcomePlayer( UNetConnection* Connection, char* Optional )
#endif
{
	Connection->PackageMap->Copy( Connection->Driver->MasterMap );
	Connection->SendPackageMap();
	if( Optional[0] )
		Connection->Logf( TEXT("WELCOME LEVEL=%s LONE=%i %s"), GetOuter()->GetName(), GetLevelInfo()->bLonePlayer, Optional );
	else
		Connection->Logf( TEXT("WELCOME LEVEL=%s LONE=%i"), GetOuter()->GetName(), GetLevelInfo()->bLonePlayer );
	Connection->FlushNet();

}

//
// Received text on the control channel.
//
void ULevel::NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text )
{
	if( ParseCommand(&Text,TEXT("USERFLAG")) )
	{
		Connection->UserFlags = appAtoi(Text);
	}
	else if( NetDriver->ServerConnection )
	{
		// We are the client.
		debugf( NAME_DevNet, TEXT("Level client received: %s"), Text );
		if( ParseCommand(&Text,TEXT("FAILURE")) )
		{
			// Return to entry.
			Engine->SetClientTravel( TEXT("?failed"), 0, TRAVEL_Absolute );
		}
		else if ( ParseCommand(&Text,TEXT("BRAWL")) )
		{
			debugf(TEXT("The client has failed one of the MD5 tests"));
			Engine->SetClientTravel( TEXT("?failed"), 0, TRAVEL_Absolute );
		}
	}
	else
	{
		// We are the server.
		debugf( NAME_DevNet, TEXT("Level server received: %s"), Text );
		if( ParseCommand(&Text,TEXT("HELLO")) )
		{
			// Versions.
			INT RemoteMinVer=219, RemoteVer=219;
			Parse( Text, TEXT("MINVER="), RemoteMinVer );
			Parse( Text, TEXT("VER="),    RemoteVer    );
			if( RemoteVer<GEngineMinNetVersion || RemoteMinVer>GEngineVersion )
			{
				Connection->Logf( TEXT("UPGRADE MINVER=%i VER=%i"), GEngineMinNetVersion, GEngineVersion );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}
			Connection->NegotiatedVer = Min(RemoteVer,GEngineVersion);

			// Get byte limit.
			INT Stats = GetLevelInfo()->Game->bEnableStatLogging;
			Connection->Challenge = appCycles();
			Connection->Logf( TEXT("CHALLENGE VER=%i CHALLENGE=%i STATS=%i"), Connection->NegotiatedVer, Connection->Challenge, Stats );
			Connection->FlushNet();
		}
		else if( ParseCommand(&Text,TEXT("NETSPEED")) )
		{
			INT Rate = appAtoi(Text);
			Connection->CurrentNetSpeed = Clamp( Rate, 1800, NetDriver->MaxClientRate );
			debugf( TEXT("Client netspeed is %i"), Connection->CurrentNetSpeed );
		}
		else if( ParseCommand(&Text,TEXT("HAVE")) )
		{
			// Client specifying his generation.
			FGuid Guid(0,0,0,0);
			Parse( Text, TEXT("GUID=" ), Guid );
			for( TArray<FPackageInfo>::TIterator It(Connection->PackageMap->List); It; ++It )
				if( It->Guid==Guid )
					Parse( Text, TEXT("GEN=" ), It->RemoteGeneration );
		}
		else if( ParseCommand( &Text, TEXT("SKIP") ) )
		{
			FGuid Guid(0,0,0,0);
			Parse( Text, TEXT("GUID=" ), Guid );
			if( Connection->PackageMap )
			{
				for( INT i=0;i<Connection->PackageMap->List.Num();i++ )
					if( Connection->PackageMap->List(i).Guid == Guid )
					{
						debugf( NAME_DevNet, TEXT("User skipped download of '%s'"), *Connection->PackageMap->List(i).URL );
						Connection->PackageMap->List.Remove( i );
						break;
					}
			}
		}
		else if( ParseCommand(&Text,TEXT("LOGIN")) )
		{
			// Admit or deny the player here.
			INT Response=0;
			if
			(	!Parse(Text,TEXT("RESPONSE="),Response)
			||	Engine->ChallengeResponse(Connection->Challenge)!=Response )	
			{
				// Challenge failed, abort right now

				debugf( NAME_DevNet, TEXT("Client %s failed CHALLENGE."), *Connection->LowLevelGetRemoteAddress() );
				Connection->Logf( TEXT("FAILCODE CHALLENGE") );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}
			TCHAR Str[1024]=TEXT("");
			FString Error, FailCode;
			Parse( Text, TEXT("URL="), Str, ARRAY_COUNT(Str) );
			Connection->RequestURL = Str;
			debugf( NAME_DevNet, TEXT("Login request: %s"), *Connection->RequestURL );
			const TCHAR* Tmp;
			for( Tmp=Str; *Tmp && *Tmp!='?'; Tmp++ );
			GetLevelInfo()->Game->eventPreLogin( Tmp, Connection->LowLevelGetRemoteAddress(), Error, FailCode );
			if( Error!=TEXT("") )
			{
				debugf( NAME_DevNet, TEXT("PreLogin failure: %s (%s)"), *Error, *FailCode );
				Connection->Logf( TEXT("FAILURE %s"), *Error );
				if( (*FailCode)[0] )
					Connection->Logf( TEXT("FAILCODE %s"), *FailCode );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}
			WelcomePlayer( Connection );
		}
		else if( ParseCommand(&Text,TEXT("JOIN")) && !Connection->Actor )
		{
			// Finish computing the package map.
			Connection->PackageMap->Compute();

			// Spawn the player-actor for this network player.
			FString Error;
			debugf( NAME_DevNet, TEXT("Join request: %s"), *Connection->RequestURL );
			Connection->Actor = SpawnPlayActor( Connection, ROLE_AutonomousProxy, FURL(NULL,*Connection->RequestURL,TRAVEL_Absolute), Error );
			if( !Connection->Actor )
			{
				// Failed to connect.
				debugf( NAME_DevNet, TEXT("Join failure: %s"), *Error );
				Connection->Logf( TEXT("FAILURE %s"), *Error );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
			}
			else
			{
				// Successfully in game.
				debugf( NAME_DevNet, TEXT("Join succeeded: %s"), *Connection->Actor->PlayerReplicationInfo->PlayerName );
			}
		}
	}
}

//
// Called when a file receive is about to begin.
//
void ULevel::NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped )
{
	appErrorf( TEXT("Level received unexpected file") );
}

//
// Called when other side requests a file.
//
UBOOL ULevel::NotifySendingFile( UNetConnection* Connection, FGuid Guid )
{
	if( NetDriver->ServerConnection )
	{
		// We are the client.
		debugf( NAME_DevNet, TEXT("Server requested file: Refused") );
		return 0;
	}
	else
	{
		// We are the server.
		debugf( NAME_DevNet, TEXT("Client requested file: Allowed") );
		return 1;
	}
}

/*-----------------------------------------------------------------------------
	Clock.
-----------------------------------------------------------------------------*/

void ULevel::UpdateTime(ALevelInfo* Info)
{
	appSystemTime( Info->Year, Info->Month, Info->DayOfWeek, Info->Day, Info->Hour, Info->Minute, Info->Second, Info->Millisecond );
}

/*-----------------------------------------------------------------------------
	Level initialization.
-----------------------------------------------------------------------------*/

void ULevel::BeginPlay(FURL InURL,UPackageMap* PackageMap)
{
	ALevelInfo* Info = GetLevelInfo();

	// Set level info.
	if( !InURL.GetOption(TEXT("load"),NULL) )
		URL = InURL;
	Info->EngineVersion = FString::Printf( TEXT("%i"), GEngineVersion );
	Info->MinNetVersion = FString::Printf( TEXT("%i"), GEngineMinNetVersion );
	Info->ComputerName = appComputerName();
	Engine = GEngine;

	// Initialize the level's actor components.
	UpdateComponents();

	// Update the LevelInfo's time.
	UpdateTime(Info);

	// Init the game info.
	TCHAR Options[1024]=TEXT("");
	TCHAR GameClassName[256]=TEXT("");
	FString	Error=TEXT("");
	for( INT i=0; i<InURL.Op.Num(); i++ )
	{
		appStrcat( Options, TEXT("?") );
		appStrcat( Options, *InURL.Op(i) );
		Parse( *InURL.Op(i), TEXT("GAME="), GameClassName, ARRAY_COUNT(GameClassName) );
	}
	if( IsServer() && !Info->Game )
	{
		// Get the GameInfo class.
		UClass* GameClass=NULL;
		if ( GameClassName[0] )
			GameClass = StaticLoadClass( AGameInfo::StaticClass(), NULL, GameClassName, NULL, LOAD_NoFail, PackageMap );
		if( !GameClass && Info->DefaultGameType.Len() > 0 )
			GameClass = StaticLoadClass( AGameInfo::StaticClass(), NULL, *(Info->DefaultGameType), NULL, LOAD_NoFail, PackageMap );
		if( !GameClass && appStricmp(GetOuter()->GetName(),TEXT("Entry"))==0 ) 
			GameClass = AGameInfo::StaticClass();
		if ( !GameClass )
			GameClass = StaticLoadClass( AGameInfo::StaticClass(), NULL, GEngine->Client ? TEXT("game-ini:Engine.GameInfo.DefaultGame") : TEXT("game-ini:Engine.GameInfo.DefaultServerGame"), NULL, LOAD_NoFail, PackageMap );
        if ( !GameClass ) 
			GameClass = AGameInfo::StaticClass();

		// Spawn the GameInfo.
		debugf( NAME_Log, TEXT("Game class is '%s'"), GameClass->GetName() );
		Info->Game = (AGameInfo*)SpawnActor( GameClass );
		check(Info->Game!=NULL);
	}
#if VIEWPORT_ACTOR_DISABLED
	// Init detail.
	Info->DetailMode = DM_SuperHigh;
	if(Client && Client->Viewports.Num() && Client->Viewports(0)->RenDev)
	{
		if(Client->Viewports(0)->RenDev->SuperHighDetailActors)
			Info->DetailMode = DM_SuperHigh;
		else if(Client->Viewports(0)->RenDev->HighDetailActors)
			Info->DetailMode = DM_High;
		else
			Info->DetailMode = DM_Low;
	}
#endif

	// Clear any existing stat charts.
	if(GStatChart)
	{
		GStatChart->Reset();
	}

	// Init level gameplay info.
	iFirstDynamicActor = 0;
	if( !Info->bBegunPlay )
	{
		// Check that paths are valid
		if ( !GetLevelInfo()->bPathsRebuilt )
			debugf(TEXT("*** WARNING - PATHS MAY NOT BE VALID ***"));
		// Lock the level.
		debugf( NAME_Log, TEXT("Bringing %s up for play (%i)..."), *GetFullName(), appRound(GEngine->GetMaxTickRate()) );
		TimeSeconds = 0;
		GetLevelInfo()->TimeSeconds = 0;
		GetLevelInfo()->GetDefaultPhysicsVolume()->bNoDelete = true;

		// Kill off actors that aren't interesting to the client.
		if( !IsServer() )
		{
			for( INT i=0; i<Actors.Num(); i++ )
			{
				AActor* Actor = Actors(i);
				if( Actor )
				{
					if ( Actor->bStatic || Actor->bNoDelete )
					{
						if ( !Actor->bClientAuthoritative )
							Exchange( Actor->Role, Actor->RemoteRole );
					}
					else
						DestroyActor( Actor );
				}
			}
		}

		// Init touching actors & clear LastRenderTime
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) )
			{
				Actors(i)->LastRenderTime = 0.f;
				Actors(i)->Touching.Empty();
				Actors(i)->PhysicsVolume = GetLevelInfo()->GetDefaultPhysicsVolume();
			}


		// Init scripting.
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) )
				Actors(i)->InitExecution();

		// Enable actor script calls.
		Info->bBegunPlay = 1;
		Info->bStartup = 1;

		if(!Info->bRBPhysNoInit)
		{
			InitLevelRBPhys();

			for( INT i=0; i<Actors.Num(); i++ )
				if( Actors(i) )
					Actors(i)->InitRBPhys();
		}

		// Init the game.
		if( Info->Game )
			Info->Game->eventInitGame( Options, Error );

		// Send PreBeginPlay.
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && !Actors(i)->bScriptInitialized )
				Actors(i)->eventPreBeginPlay();

		// Set BeginPlay.
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && !Actors(i)->bScriptInitialized )
				Actors(i)->eventBeginPlay();

		// intialize any scripting sequences (not on client)
		if( GameSequence != NULL &&
			GetLevelInfo()->NetMode != NM_Client )
		{
			GameSequence->BeginPlay();
		}

		// Set zones && gather volumes.
		TArray<AVolume*> LevelVolumes;
		for( INT i=0; i<Actors.Num(); i++ )
		{
			if( Actors(i) && !Actors(i)->bScriptInitialized )
				Actors(i)->SetZone( 1, 1 );

			AVolume* Volume = Cast<AVolume>(Actors(i));
			if( Volume )
				LevelVolumes.AddItem(Volume);
		}

		// Set appropriate volumes for each actor.
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && !Actors(i)->bScriptInitialized )
				Actors(i)->SetVolumes( LevelVolumes );

		// Post begin play.
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && !Actors(i)->bScriptInitialized )
			{
				Actors(i)->eventPostBeginPlay();

				if(Actors(i))
					Actors(i)->PostBeginPlay();
			}

		// Post net begin play.
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && !Actors(i)->bScriptInitialized )
				Actors(i)->eventPostNetBeginPlay();

		// Begin scripting.
		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && !Actors(i)->bScriptInitialized )
				Actors(i)->eventSetInitialState();

		// Find bases
		for( INT i=0; i<Actors.Num(); i++ )
		{
			if( Actors(i) ) 
			{
				if ( Actors(i)->AttachTag != NAME_None )
				{
					//find actor to attach self onto
					for( INT j=0; j<Actors.Num(); j++ )
					{
						if( Actors(j) 
							&& ((Actors(j)->Tag == Actors(i)->AttachTag) || (Actors(j)->GetFName() == Actors(i)->AttachTag))  )
						{
							// Actor(i) is the actor we want to attach. Actor(j) is the new base.

							// If a bone name is specified, attempt to attach to CollisionComponent. 
							// Would be nice to specify _which_ component, not always CollisionComponent, but tricky...
							if(Actors(i)->BaseBoneName != NAME_None)
							{
								// First see if we have a Base SkeletalMeshComponent specified
								USkeletalMeshComponent* BaseSkelComp = Actors(i)->BaseSkelComponent;

								// If not, see if the CollisionComponent in the Base is a SkeletalMesh
								if(!BaseSkelComp)
								{									
									BaseSkelComp = Cast<USkeletalMeshComponent>( Actors(j)->CollisionComponent );
								}

								// If BaseSkelComp is still NULL at this point, SetBase will fail gracefully and just attach it relative to the Actor ref frame as usual.
								Actors(i)->SetBase( Actors(j), FVector(0,0,1), 0, BaseSkelComp, Actors(i)->BaseBoneName );				
							}
							else // Normal case - just attaching to actor.
							{
								Actors(i)->SetBase( Actors(j), FVector(0,0,1), 0 );
							}

							break;
						}
					}
				}
				else if( Actors(i)->bCollideWorld && Actors(i)->bShouldBaseAtStartup
				 &&	((Actors(i)->Physics == PHYS_None) || (Actors(i)->Physics == PHYS_Rotating)) )
				{
					 Actors(i)->FindBase();
				}
			}
		}

		Info->bStartup = 0;
	}
	else TimeSeconds = GetLevelInfo()->TimeSeconds;

	// Rearrange actors: static first, then others.
	TArray<AActor*> NewActors;
	NewActors.AddItem(Actors(0));
	NewActors.AddItem(Actors(1));
	for( INT i=2; i<Actors.Num(); i++ )
		if( Actors(i) && Actors(i)->bStatic && !Actors(i)->bAlwaysRelevant )
			NewActors.AddItem( Actors(i) );
	iFirstNetRelevantActor=NewActors.Num();
	for( INT i=2; i<Actors.Num(); i++ )
		if( Actors(i) && Actors(i)->bStatic && Actors(i)->bAlwaysRelevant )
			NewActors.AddItem( Actors(i) );
	iFirstDynamicActor=NewActors.Num();
	for( INT i=2; i<Actors.Num(); i++ )
		if( Actors(i) && !Actors(i)->bStatic )
			NewActors.AddItem( Actors(i) );
	Actors = NewActors;
}

/**
 * Cleans up components, streaming data and assorted other intermediate data.	
 */
void ULevel::CleanupLevel()
{
	ClearComponents();

	DynamicPrimitives.Empty();
	StaticStreamableTextureInfos.Empty();
}

/**
 * Static helper function to add a material's textures to a level's StaticStreamableTextureInfos array.
 * 
 * @param Level				Level whose StaticStreamableTextureInfos array is being used
 * @param Material			Material to query for streamable textures
 * @param BoundingSphere	BoundingSphere to add
 * @param TexelFactor		TexelFactor to add
 */
static void AddStreamingMaterial( ULevel* Level, UMaterial* Material, const FSphere& BoundingSphere, FLOAT TexelFactor )
{
	if( Material )
	{
		for( INT TextureIndex = 0; TextureIndex < Material->StreamingTextures.Num(); TextureIndex++ )
		{
			UTexture* Texture = Material->StreamingTextures(TextureIndex)->GetUTexture();				
			if( Texture )
			{
				INT TextureInfoIndex = 0;
				for( ; TextureInfoIndex<Level->StaticStreamableTextureInfos.Num(); TextureInfoIndex++ )
				{
					if( Level->StaticStreamableTextureInfos(TextureInfoIndex).Texture == Texture )
					{
						break;
					}
				}

				if( TextureInfoIndex == Level->StaticStreamableTextureInfos.Num() )
				{
					Level->StaticStreamableTextureInfos.AddZeroed( 1 ); // AddZeroed instead of Add as FStreamableTextureInfo contains a TArray.
					Level->StaticStreamableTextureInfos(TextureInfoIndex).Texture = Texture;
				}
		
				FStreamableTextureInfo&		TextureInfo	= Level->StaticStreamableTextureInfos(TextureInfoIndex);
				FStreamableTextureInstance	TextureInstance;

				TextureInstance.BoundingSphere	= BoundingSphere;
				TextureInstance.TexelFactor		= TexelFactor;

				TextureInfo.TextureInstances.AddItem( TextureInstance );
			}
		}
	}
}

/**
 * Rebuilds static streaming data.	
 */
void ULevel::BuildStreamingData()
{
	DOUBLE StartTime = appSeconds();

	// Start from scratch.
	StaticStreamableTextureInfos.Empty();

	if( GIsUCC )
	{
		// Only components that are attached to a scene have valid Bounds/ LocalToWorld matrices.
		UpdateComponents();
	}

	// Static meshes...
	for( TObjectIterator<UStaticMeshComponent> It; It; ++It )
	{
		UStaticMeshComponent* StaticMeshComponent = *It;

		UBOOL IsStatic  = StaticMeshComponent->Owner && StaticMeshComponent->Owner->bStatic;
		UBOOL IsInLevel = StaticMeshComponent->IsIn( GetOuter() );

		if( IsStatic && IsInLevel && StaticMeshComponent->StaticMesh )
		{
			StaticMeshComponent->UpdateBounds();

			for( INT MaterialIndex = 0; MaterialIndex < StaticMeshComponent->StaticMesh->Materials.Num(); MaterialIndex++ )
			{
				UMaterialInstance*	MaterialInstance	= StaticMeshComponent->GetMaterial(MaterialIndex);
				UMaterial*			Material			= MaterialInstance ? MaterialInstance->GetMaterial() : NULL;
				FLOAT				TexelFactor			= StaticMeshComponent->StaticMesh->GetStreamingTextureFactor(0) * StaticMeshComponent->Owner->DrawScale * StaticMeshComponent->Owner->DrawScale3D.GetAbsMax();
				
 				AddStreamingMaterial( this, Material, FSphere( StaticMeshComponent->Bounds.Origin, StaticMeshComponent->Bounds.SphereRadius ), TexelFactor );
			}
		}
	}

	// BSP...
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		ABrush* BrushActor	= Cast<ABrush>(Actors(ActorIndex));
		UModel* Brush		= BrushActor && !BrushActor->bDeleteMe ? BrushActor->Brush : NULL;

		if( Brush && Brush->Polys )
		{
			TArray<FVector> Points;
			for( INT PolyIndex=0; PolyIndex<Brush->Polys->Element.Num(); PolyIndex++ )
			{			
				for( INT VertexIndex=0; VertexIndex<Brush->Polys->Element(PolyIndex).NumVertices; VertexIndex++ )
				{
					Points.AddItem(Brush->Polys->Element(PolyIndex).Vertex[VertexIndex]);
				}
			}

			//@todo streaming: should be optimized to use per surface specific texel factor.

			FBoxSphereBounds	Bounds = FBoxSphereBounds( &Points(0), Points.Num() ).TransformBy(BrushActor->LocalToWorld());
			FSphere				BoundingSphere( Bounds.Origin, Bounds.SphereRadius );
			FLOAT				TexelFactor	= Brush->GetStreamingTextureFactor();
			TArray<UMaterial*>	Materials;

			for( INT PolyIndex=0; PolyIndex<Brush->Polys->Element.Num(); PolyIndex++ )
			{
				FPoly&		Poly	 = Brush->Polys->Element(PolyIndex);
				UMaterial*	Material = Poly.Material ? Poly.Material->GetMaterial() : NULL;

				Materials.AddUniqueItem( Material );
			}

			for( INT MaterialIndex=0; MaterialIndex<Materials.Num(); MaterialIndex++ )
			{
				AddStreamingMaterial( this, Materials(MaterialIndex), BoundingSphere, TexelFactor );
			}
		}
	}

	// Terrain...
	for( TObjectIterator<UTerrainComponent> It; It; ++It )
	{
		UTerrainComponent*	TerrainComponent	= *It;
		UBOOL				IsInLevel			= TerrainComponent->IsIn( GetOuter() );

		if( IsInLevel )
		{
			TerrainComponent->UpdateBounds();

			FSphere				BoundingSphere		= FSphere( TerrainComponent->Bounds.Origin, TerrainComponent->Bounds.SphereRadius );
			ATerrain*			Terrain				= TerrainComponent->GetTerrain();
		
			for( INT MaterialIndex=0; MaterialIndex<Terrain->WeightedMaterials.Num(); MaterialIndex++ )
			{
				UMaterial*	Material	= NULL;
				FLOAT		TexelFactor	= 0;
					
				for( INT BatchIndex=0; BatchIndex<TerrainComponent->BatchMaterials.Num(); BatchIndex++ )
				{
					if( TerrainComponent->BatchMaterials(BatchIndex).Get(MaterialIndex) )
					{
						UTerrainMaterial* TerrainMaterial = Terrain->WeightedMaterials(MaterialIndex).Material;
						if( TerrainMaterial && TerrainMaterial->Material )
						{
							Material	= TerrainMaterial->Material->GetMaterial();
							TexelFactor = TerrainMaterial->MappingScale;
							break;
						}
					}
				}

				if( Material )
				{
					AddStreamingMaterial( this, Material, BoundingSphere, TexelFactor );
				}
			}
		}
	}

	if( GIsUCC )
	{
		// Clear components initialized at the top of this function so garbage collection doesn't complain.
		ClearComponents();
	}

	debugf(TEXT("ULevel::BuildStreamingData took %.3f seconds."), appSeconds() - StartTime);
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

