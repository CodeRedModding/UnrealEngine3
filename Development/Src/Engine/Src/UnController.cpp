/*=============================================================================
	UnController.cpp: AI implementation

  This contains both C++ methods (movement and reachability), as well as some
  AI related natives

   Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "UnNet.h"
#include "UnPath.h"
#include "EngineAIClasses.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "EnginePlatformInterfaceClasses.h"

IMPLEMENT_CLASS(UCheatManager);

/** GetNetPriority()
@param Viewer		PlayerController owned by the client for whom net priority is being determined
@param InChannel	Channel on which this actor is being replicated.
@param Time			Time since actor was last replicated
@return				Priority of this actor for replication
*/
FLOAT APlayerController::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth)
{
	if ( Viewer == this )
		Time *= 4.f;
	return NetPriority * Time;
}

/**
  * Tries to determine if a player is cheating by running his local client's clock faster than the server clock
  * Since we don't synchronize clocks, this can only be approximate
  * Also, have to be careful not to flag false positives caused by packets getting backed up on the network. 
  * Lots of servermove packets arriving at once looks a lot like a speed hack
  * Returns TRUE if no issue was detected
  */
UBOOL APlayerController::CheckSpeedHack(FLOAT NewDeltaTime)
{
	UBOOL Result = TRUE;

	FLOAT DefaultMaxTimeMargin = ((AGameInfo *)(AGameInfo::StaticClass()->GetDefaultActor()))->MaxTimeMargin;
	if ( (ServerTimeStamp > 0.f) && (DefaultMaxTimeMargin > 0.f) )
    {
 		if ( GWorld->GetTimeSeconds() - ServerTimeStamp > 0.3f )
		{
			// haven't received an update for a long time
			TimeMargin = 0.f;
			MaxTimeMargin = DefaultMaxTimeMargin;
		}
		else if ( (TimeMargin > MaxTimeMargin) && (MaxTimeMargin < 0.2f) )
        {
			if ( MaxTimeMargin == 0.f )
			{
				// initialize max time margin
				MaxTimeMargin = DefaultMaxTimeMargin;
			}
			else
			{
				// player is too far ahead - make them stand still for a while
				if ( Pawn )
				{
					Pawn->Velocity = FVector(0.f,0.f,0.f);
				}
				TimeMargin -= 0.7f * (GWorld->GetTimeSeconds() - ServerTimeStamp);
				if ( TimeMargin < MaxTimeMargin )
				{
					MaxTimeMargin = DefaultMaxTimeMargin;
				}
				else
				{
					MaxTimeMargin = 0.1f;
				}
				Result = FALSE;
			}
        }
		else
		{
			FLOAT DefaultMinTimeMargin = ((AGameInfo *)(AGameInfo::StaticClass()->GetDefaultActor()))->MinTimeMargin;
			if ( TimeMargin < DefaultMinTimeMargin )
			{	
				// limit how far behind we let client clock get.  Used to prevent speedhacks where client slows their clock down for a while then speeds it up.
				TimeMargin = 0.7f * DefaultMinTimeMargin;
			}
			else if ( TimeMargin < -0.3f )
			{
				// if client clock is already behind server clock, don't provide any slack so it will catch back up.
				TimeMargin = TimeMargin + NewDeltaTime - (GWorld->GetTimeSeconds() - ServerTimeStamp) - 0.002f;
			}
			else
			{
				// Increase time margin, with a bit of slack (allowing clocks to actually run at different speeds)
				TimeMargin = TimeMargin + NewDeltaTime - ((AGameInfo *)(AGameInfo::StaticClass()->GetDefaultActor()))->TimeMarginSlack * (GWorld->GetTimeSeconds() - ServerTimeStamp);
				if ( TimeMargin > 0.f )
				{
					TimeMargin -= 0.002f;
				}
			}
            
  			// if still same tick on server, don't trip detection
			if ( GWorld->GetTimeSeconds() != ServerTimeStamp )
			{
				if ( TimeMargin > MaxTimeMargin )
				{
					MaxTimeMargin = 0.1f;
					// Result = FALSE; // commented out to give him one tick of grace, in case it gets reset
				}
				else 
				{
					MaxTimeMargin = DefaultMaxTimeMargin;
				}
			}
		}
    }

	return Result;
}

/**
 * Called when a level with path nodes is being removed.  Clear any path refs so that the level can be garbage collected.
 */
void AController::ClearCrossLevelPaths(ULevel *Level)
{
	if (Pawn != NULL)
	{
		if (Pawn->Anchor != NULL && Pawn->Anchor->IsInLevel(Level))
		{
			Pawn->SetAnchor(NULL);
		}
		if (Pawn->LastAnchor != NULL && Pawn->LastAnchor->IsInLevel(Level))
		{
			Pawn->LastAnchor = NULL;
		}
	}
	for (INT Idx = 0; Idx < RouteCache.Num(); Idx++)
	{
		ANavigationPoint *Nav = RouteCache(Idx);
		if (Nav != NULL && Nav->IsInLevel(Level))
		{
			RouteCache_Empty();
			GetStateFrame()->LatentAction = 0;
			break;
		}
	}
	if (MoveTarget != NULL && MoveTarget->IsInLevel(Level))
	{
		MoveTarget = NULL;
		GetStateFrame()->LatentAction = 0;
	}
	if (CurrentPath != NULL && CurrentPath->Start != NULL && CurrentPath->Start->IsInLevel(Level))
	{
		CurrentPath = NULL;
	}
	if (NextRoutePath != NULL && NextRoutePath->Start != NULL && NextRoutePath->Start->IsInLevel(Level))
	{
		NextRoutePath = NULL;
	}
}

void AController::Spawned()
{
	Super::Spawned();
	PlayerNum = GWorld->PlayerNum++;
}


/** Called when a Controller is garbage collected. Fixes up WorldInfo's ControllerList appropriately */
void AController::BeginDestroy()
{
	if( Role == ROLE_Authority && NextController != NULL && !NextController->HasAnyFlags(RF_BeginDestroyed) && !NextController->HasAnyFlags(RF_Unreachable) &&
		GWorld != NULL && GWorld->GetWorldInfo() != NULL )
	{
		AController* LastController = GWorld->GetWorldInfo()->ControllerList;
		if( LastController == NULL || LastController == this )
		{
			GWorld->GetWorldInfo()->ControllerList = NextController;
		}
		else
		{
			while( LastController != NULL && LastController != NextController )
			{
				if( LastController->NextController == this || LastController->NextController == NULL )
				{
					LastController->NextController = NextController;
					break;
				}
				LastController = LastController->NextController;
			}
		}
	}
	NextController = NULL;
	Super::BeginDestroy();
}

/** returns whether this controller is a local controller.
* @RETURN true always for non-playercontroller
 */
UBOOL AController::IsLocalController()
{
	return TRUE;
}

/** returns whether this controller is a local controller.
  * @RETURN true if is local playercontroller
 */
UBOOL APlayerController::IsLocalController()
{
	return (WorldInfo->NetMode == NM_Standalone) || IsLocalPlayerController();
}

UBOOL AController::IsLocalPlayerController()
{
	return FALSE;
}

UBOOL APlayerController::IsLocalPlayerController()
{
	return ( Player && Player->IsA(ULocalPlayer::StaticClass()) );
}

UBOOL AController::WantsLedgeCheck()
{
	if ( !Pawn || !Pawn->CylinderComponent )
		return FALSE;
	if ( Pawn->bCanJump )
	{
		// check if moving toward a navigation point, and not messed with
		if ( MoveTarget && (GetStateFrame()->LatentAction == AI_PollMoveToward) )
		{
			// check if still on path
			if ( CurrentPath && (CurrentPath->End == MoveTarget) )
			{
				FVector LineDir = Pawn->Location - (CurrentPath->Start->Location + (CurrentPathDir | (Pawn->Location - CurrentPath->Start->Location)) * CurrentPathDir);
				if (LineDir.SizeSquared() < 0.5f * Pawn->CylinderComponent->CollisionRadius * Pawn->CylinderComponent->CollisionRadius)
				{
					//debugf(TEXT("%s skip ledge check because on path"), *Pawn->GetName());
					return FALSE;
				}
			}
			// check if could reach by jumping
			if ( MoveTarget->Physics != PHYS_Falling )
			{
				FVector JumpVelocity(0.f,0.f,0.f);
				if ( Pawn->SuggestJumpVelocity(JumpVelocity, MoveTarget->Location, Pawn->Location) )
				{
					return FALSE;
				}
			}
		}
	}
	//debugf(TEXT("%s do ledge check"), *Pawn->GetName());
	return ( !Pawn->bCanFly );
}

UBOOL APlayerController::WantsLedgeCheck()
{
	return ( Pawn && (Pawn->bIsCrouched || Pawn->bIsWalking) );
}

UBOOL APlayerController::StopAtLedge()
{
	return FALSE;
}

UBOOL AController::StopAtLedge()
{
	if( !Pawn || 
		!Pawn->bCanJump || 
		 Pawn->bStopAtLedges )
	{
		FailMove();
		return TRUE;
	}

	return FALSE;
}

/** Called to force controller to abort current move
*/
void AController::FailMove()
{
	MoveTimer = -1.f;
}

//-------------------------------------------------------------------------------------------------
/*
Node Evaluation functions, used with APawn::BestPathTo()
*/

// declare type for node evaluation functions
typedef FLOAT ( *NodeEvaluator ) (ANavigationPoint*, APawn*, FLOAT);


FLOAT FindRandomPath( ANavigationPoint* CurrentNode, APawn* seeker, FLOAT bestWeight )
{
	if ( CurrentNode->bEndPoint )
		return (1000.f + appFrand());
	return appFrand();
}
//----------------------------------------------------------------------------------

void APlayerController::ClientUpdateLevelStreamingStatus(FName PackageName, UBOOL bNewShouldBeLoaded, UBOOL bNewShouldBeVisible, UBOOL bNewShouldBlockOnLoad )
{
	// if we're about to commit a map change, we assume that the streaming update is based on the to be loaded map and so defer it until that is complete
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine != NULL && GameEngine->bShouldCommitPendingMapChange)
	{
		new(GameEngine->PendingLevelStreamingStatusUpdates) FLevelStreamingStatus(PackageName, bNewShouldBeLoaded, bNewShouldBeVisible);
	}
	else
	{
		// search for the level object by name
		ULevelStreaming* LevelStreamingObject = NULL;
		if (PackageName != NAME_None)
		{
			AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
			for (INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++)
			{
				ULevelStreaming* CurrentLevelStreamingObject = WorldInfo->StreamingLevels(LevelIndex);
				if (CurrentLevelStreamingObject != NULL && CurrentLevelStreamingObject->PackageName == PackageName)
				{
					LevelStreamingObject = CurrentLevelStreamingObject;
					if (LevelStreamingObject != NULL)
					{
						// If we're unloading any levels, we need to request a one frame delay of garbage collection to make sure it happens after the level is actually unloaded
						if (LevelStreamingObject->bShouldBeLoaded && !bNewShouldBeLoaded)
						{
							GWorld->DelayGarbageCollection();
						}

						LevelStreamingObject->bShouldBeLoaded		= bNewShouldBeLoaded;
						LevelStreamingObject->bShouldBeVisible		= bNewShouldBeVisible;
						LevelStreamingObject->bShouldBlockOnLoad	= bNewShouldBlockOnLoad;
					}
					else
					{
						debugf( NAME_DevStreaming, TEXT("Unable to handle streaming object %s"),*LevelStreamingObject->GetName() );
					}

					// break out of object iterator if we found a match
					break;
				}
			}
		}

		if (LevelStreamingObject == NULL)
		{
			debugf( NAME_DevStreaming, TEXT("Unable to find streaming object %s"), *PackageName.ToString() );
		}
	}
}

void APlayerController::ClientFlushLevelStreaming()
{
	// if we're already doing a map change, requesting another blocking load is just wasting time
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine == NULL || !GameEngine->bShouldCommitPendingMapChange)
	{
		// request level streaming be flushed next frame
		GWorld->UpdateLevelStreaming(NULL);
		WorldInfo->bRequestedBlockOnAsyncLoading = TRUE;
		// request GC as soon as possible to remove any unloaded levels from memory
		WorldInfo->ForceGarbageCollection();
	}
}

/** called when the client adds/removes a streamed level
 * the server will only replicate references to Actors in visible levels so that it's impossible to send references to
 * Actors the client has not initialized
 * @param PackageName the name of the package for the level whose status changed
 */
void APlayerController::ServerUpdateLevelVisibility(FName PackageName, UBOOL bIsVisible)
{
	UNetConnection* Connection = Cast<UNetConnection>(Player);
	if (Connection != NULL)
	{
		// add or remove the level package name from the list, as requested
		if (bIsVisible)
		{
			// verify that we were passed a valid level name
			FString Filename;
			UPackage* TempPkg = FindPackage(NULL, *PackageName.ToString());
			ULinkerLoad* Linker = ULinkerLoad::FindExistingLinkerForPackage(TempPkg);

			//If we have a linker we know it has been loaded off disk successfully
			//Alternately if we have a file it is fine too
			if (Linker || GPackageFileCache->FindPackageFile(*PackageName.ToString(), NULL, Filename, NULL))
			{
				Connection->ClientVisibleLevelNames.AddUniqueItem(PackageName);
			}
			else
			{
				debugf(NAME_Warning, TEXT("ServerUpdateLevelVisibility() ignored non-existant package '%s'"), *PackageName.ToString());
			}
		}
		else
		{
			Connection->ClientVisibleLevelNames.RemoveItem(PackageName);
		}
	}
}

void APlayerController::ClientAddTextureStreamingLoc(FVector InLoc, FLOAT Duration, UBOOL bOverrideLocation )
{
	if (GStreamingManager != NULL)
	{
		GStreamingManager->AddViewSlaveLocation(InLoc, 1.0f, bOverrideLocation, Duration);
	}
}

/** Whether or not to allow mature language **/
void APlayerController::SetAllowMatureLanguage( UBOOL bAllowMatureLanguge )
{
	GEngine->bAllowMatureLanguage = bAllowMatureLanguge;
}


/** Sets the Audio Group to this the value passed in **/
void APlayerController::SetAudioGroupVolume( FName ClassName, FLOAT Volume )
{
	if( ( GEngine->Client != NULL )
		&& ( GEngine->Client->GetAudioDevice() != NULL )
		)
	{
		GEngine->Client->GetAudioDevice()->SetClassVolume( ClassName, Volume );
	}
}


void APlayerController::SetNetSpeed(INT NewSpeed)
{
	UNetDriver* Driver = GWorld->GetNetDriver();
	if (Player != NULL && Driver != NULL)
	{
		Player->CurrentNetSpeed = Clamp(NewSpeed, 1800, Driver->MaxClientRate);
		if (Driver->ServerConnection != NULL)
		{
			Driver->ServerConnection->CurrentNetSpeed = Player->CurrentNetSpeed;
		}
	}
}

void APlayerController::UpdateURL(const FString& NewOption, const FString& NewValue, UBOOL bSaveDefault)
{
	UGameEngine* GameEngine = Cast<UGameEngine>( GEngine );
	if( GameEngine )
	{
		// Remove any characters in the URL option value that may cause problems for us.  The most common problem
		// case is a player's name (which might be pulled from a online service that supports all sorts of funky
		// characters in the string.)  In these cases, the URL value won't exactly match the original string value.
		FString FilteredValue = NewValue;
		FURL::FilterURLString( FilteredValue );

		GameEngine->LastURL.AddOption( *(NewOption + TEXT("=") + FilteredValue) );
		if( bSaveDefault )
		{
			GameEngine->LastURL.SaveURLConfig( TEXT("DefaultPlayer"), *NewOption, GGameIni );
		}
	}
}

FString APlayerController::GetDefaultURL(const FString& o)
{
	FString Option = o;
	FURL URL;
	URL.LoadURLConfig( TEXT("DefaultPlayer"), GGameIni );

	return FString( URL.GetOption(*(Option + FString(TEXT("="))), TEXT("")) );
}

void ADroppedPickup::AddToNavigation()
{
	if ( !Inventory )
		return;

	if ( PickupCache )
	{
		if ( PickupCache->InventoryCache == this )
			PickupCache->InventoryCache = NULL;
		PickupCache = NULL;
	}

	// find searcher
	APawn *Searcher = NULL;
	for ( AController *C=GWorld->GetFirstController(); C!=NULL; C=C->NextController )
	{
		if ( C->bIsPlayer && C->Pawn )
		{
			Searcher = C->Pawn;
			break;
		}
	}

	if ( !Searcher )
	{
		return;
	}

	// find nearest path
	FSortedPathList EndPoints;
	TArray<FNavigationOctreeObject*> Objects;
	GWorld->NavigationOctree->RadiusCheck(Location, MAXPATHDIST, Objects);
	for (INT i = 0; i < Objects.Num(); i++)
	{
		ANavigationPoint* Nav = Objects(i)->GetOwner<ANavigationPoint>();
		if ( Nav != NULL && (Location.Z - Nav->Location.Z < Searcher->MaxStepHeight + Searcher->MaxJumpHeight) &&
			(Nav->InventoryCache == NULL || Nav->InventoryCache->bDeleteMe || Nav->InventoryCache->Inventory == NULL || Nav->InventoryCache->Inventory->MaxDesireability <= Inventory->MaxDesireability) )
		{
			EndPoints.AddPath(Nav, appTrunc((Location - Nav->Location).SizeSquared()));
		}
	}

	if ( EndPoints.numPoints > 0 )
	{
		PickupCache = EndPoints.FindEndAnchor(Searcher,this,Location,false,false);
	}

	if ( PickupCache )
	{
		PickupCache->InventoryCache = this;
		PickupCache->InventoryDist = (Location - PickupCache->Location).Size();
	}
}

void ADroppedPickup::RemoveFromNavigation()
{
	if( !PickupCache )
	{
		return;
	}

	if( PickupCache->InventoryCache == this )
	{
		PickupCache->InventoryCache = NULL;
	}
}


FString APlayerController::ConsoleCommand(const FString& Cmd,UBOOL bWriteToLog)
{
	if (Player != NULL)
	{
		UConsole* ViewportConsole = (GEngine->GameViewport != NULL) ? GEngine->GameViewport->ViewportConsole : NULL;
		FConsoleOutputDevice StrOut(ViewportConsole);

		const INT CmdLen = Cmd.Len();
		TCHAR* CommandBuffer = (TCHAR*)appMalloc((CmdLen+1)*sizeof(TCHAR));
		TCHAR* Line = (TCHAR*)appMalloc((CmdLen+1)*sizeof(TCHAR));

		const TCHAR* Command = CommandBuffer;
		// copy the command into a modifiable buffer
		appStrcpy(CommandBuffer, (CmdLen+1), *Cmd.Left(CmdLen)); 
		
		// iterate over the line, breaking up on |'s
		while (ParseLine(&Command, Line, CmdLen+1))	// The ParseLine function expects the full array size, including the NULL character.
		{
			if (Player)
			{
				if(!Player->Exec(Line, StrOut))
				{
					StrOut.Logf(TEXT("Command not recognized: %s"), Line);
				}
			}
		}

		// Free temp arrays
		appFree(CommandBuffer);
		CommandBuffer=NULL;

		appFree(Line);
		Line=NULL;

		if (!bWriteToLog)
		{
			return *StrOut;
		}
	}

	return TEXT("");
}

/* CanSee()
returns true if LineOfSightto object and it is within creature's
peripheral vision
*/

UBOOL AController::CanSee(class APawn* Other)
{
	return SeePawn(Other, false);
}

/** 
 * Similar to CanSee but uses points to check instead of actor locations 
 */
UBOOL AController::CanSeeByPoints( FVector ViewLocation, FVector TestLocation, FRotator ViewRotation )
{
	if( BeyondFogDistance( ViewLocation, TestLocation ) || Pawn == NULL )
	{
		return FALSE;
	}

	const FLOAT maxdist = Pawn->SightRadius;

	// fixed max sight distance
	if( (TestLocation - ViewLocation).SizeSquared() > maxdist * maxdist )
	{
		return FALSE;
	}

	// check field of view
	const FVector& SightDir		= (TestLocation - ViewLocation).SafeNormal();
	const FVector& LookDir		= ViewRotation.Vector();
	if( (SightDir | LookDir) < Pawn->PeripheralVision )
	{
		return FALSE;
	}

	FCheckResult Hit;
	const UBOOL bClearLine = GWorld->SingleLineCheck( Hit, Pawn, TestLocation, ViewLocation, TRACE_World|TRACE_StopAtAnyHit|TRACE_ComplexCollision );

	return bClearLine;
}

AVehicle* APawn::GetVehicleBase()
{
	return Base ? Base->GetAVehicle() : NULL;
}

/* PickTarget()
Find the best pawn target for this controller to aim at.  Used for autoaiming.
*/
APawn* AController::PickTarget(UClass* TargetClass, FLOAT& bestAim, FLOAT& bestDist, FVector FireDir, FVector projStart, FLOAT MaxRange)
{
	if (Role < ROLE_Authority)
	{
		debugf(NAME_Warning, TEXT("Can't call PickTarget() on client"));
		return NULL;
	}

	if( !TargetClass )
	{
		TargetClass = APawn::StaticClass();
	}

	if( bestAim >= 1.f )
	{
		return NULL;
	}

	APawn* BestTarget = NULL;
	const FLOAT VerticalAim = bestAim * 3.f - 2.f;
	FCheckResult Hit(1.f);
    const FLOAT MaxRangeSquared = MaxRange*MaxRange;

	for ( AController *next=GWorld->GetFirstController(); next!=NULL; next=next->NextController )
	{
		APawn* NewTarget = next->Pawn;
		if ( NewTarget && (NewTarget != Pawn) )
		{
			if ( !NewTarget->bProjTarget )
			{
				// perhaps target vehicle this pawn is based on instead
				NewTarget = NewTarget->GetVehicleBase();
				if( !NewTarget || NewTarget->Controller )
				{
					continue;
				}
			}
			// look for best controlled pawn target
			if ( NewTarget->GetClass()->IsChildOf(TargetClass) && NewTarget->IsValidEnemyTargetFor(PlayerReplicationInfo, TRUE) )
			{
				const FVector AimDir = NewTarget->Location - projStart;
				FLOAT newAim = FireDir | AimDir;
				if ( newAim > 0 )
				{
					FLOAT FireDist = AimDir.SizeSquared();
					// only find targets which are < MaxRange units away
					if ( FireDist < MaxRangeSquared )
					{
						FireDist = appSqrt(FireDist);
						newAim = newAim/FireDist;
						if ( newAim > bestAim )
						{
							// target is more in line than current best - see if target is visible
							GWorld->SingleLineCheck( Hit, this, NewTarget->Location + FVector(0,0,NewTarget->EyeHeight), projStart, TRACE_World|TRACE_StopAtAnyHit );
							if( Hit.Actor )
							{
								GWorld->SingleLineCheck( Hit, this, NewTarget->Location, projStart, TRACE_World|TRACE_StopAtAnyHit );
							}

							if ( !Hit.Actor )
							{
								BestTarget = NewTarget;
								bestAim = newAim;
								bestDist = FireDist;
							}
						}
						else if ( !BestTarget )
						{
							// no target yet, so be more liberal about up/down error (more vertical autoaim help)
							FVector FireDir2D = FireDir;
							FireDir2D.Z = 0;
							FireDir2D.Normalize();
							FLOAT newAim2D = FireDir2D | AimDir;
							newAim2D = newAim2D/FireDist;
							if ( (newAim2D > bestAim) && (newAim > VerticalAim) )
							{
								GWorld->SingleLineCheck( Hit, this, NewTarget->Location, projStart, TRACE_World|TRACE_StopAtAnyHit );
								if( Hit.Actor )
								{
									GWorld->SingleLineCheck( Hit, this, NewTarget->Location + FVector(0,0,NewTarget->EyeHeight), projStart, TRACE_World|TRACE_StopAtAnyHit );
								}

								if ( !Hit.Actor )
								{
									BestTarget = NewTarget;
									bestDist = FireDist;
								}
							}
						}
					}
				}
			}
		}
	}

	return BestTarget;
}

/** returns if we are a valid enemy for PRI
 * checks things like whether we're alive, teammates, etc
 * works on clients and servers
 */
UBOOL APawn::IsValidEnemyTargetFor(const APlayerReplicationInfo* OtherPRI, UBOOL bNoPRIIsEnemy)
{
	// only am valid target if not dead, and not driving a vehicle
	if ( bDeleteMe || (Health <=0) || DrivenVehicle )
	{
		return FALSE;
	}
	if ( !PlayerReplicationInfo )
	{
		 return bNoPRIIsEnemy;
	}
	
	// and not on same team, or neither is on a team (which implies not a team game)
	return !OtherPRI || !PlayerReplicationInfo->Team || (PlayerReplicationInfo->Team != OtherPRI->Team);
}

/* execWaitForLanding()
wait until physics is not PHYS_Falling
*/
void AController::execWaitForLanding( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_OPTX(waitDuration,4.f);
	P_FINISH;

	LatentFloat = waitDuration;
	if ( Pawn && (Pawn->Physics == PHYS_Falling) )
		GetStateFrame()->LatentAction = AI_PollWaitForLanding;
}

void AController::execPollWaitForLanding( FFrame& Stack, RESULT_DECL )
{
	if( Pawn && (Pawn->Physics != PHYS_Falling) )
	{
		GetStateFrame()->LatentAction = 0;
	}
	else
	{
		FLOAT DeltaSeconds = *(FLOAT*)Result;
		LatentFloat -= DeltaSeconds;
		if (LatentFloat <= 0.f)
		{
			eventLongFall();
		}
	}
}
IMPLEMENT_FUNCTION( AController, AI_PollWaitForLanding, execPollWaitForLanding);

UBOOL AController::PickWallAdjust(FVector HitNormal)
{
	if ( !Pawn )
		return false;
	return Pawn->PickWallAdjust(HitNormal, NULL);
}

/* FindStairRotation()
returns an integer to use as a pitch to orient player view along current ground (flat, up, or down)
*/
INT APlayerController::FindStairRotation(FLOAT deltaTime)
{
	// only recommend pitch if controller has a pawn, and frame rate isn't ridiculously low

	if ( !Pawn || (deltaTime > 0.33) )
	{
		return Rotation.Pitch;
	}

	if (Rotation.Pitch > 32768)
		Rotation.Pitch = (Rotation.Pitch & 65535) - 65536;
	
	FCheckResult Hit(1.f);
	FRotator LookRot = Rotation;
	LookRot.Pitch = 0;
	FVector Dir = LookRot.Vector();
	FVector EyeSpot = Pawn->Location + FVector(0,0,Pawn->BaseEyeHeight);
	FLOAT height = Pawn->CylinderComponent->CollisionHeight + Pawn->BaseEyeHeight;
	FVector CollisionSlice(Pawn->CylinderComponent->CollisionRadius,Pawn->CylinderComponent->CollisionRadius,1.f);

	GWorld->SingleLineCheck(Hit, this, EyeSpot + 2 * height * Dir, EyeSpot, TRACE_World, CollisionSlice);
	FLOAT Dist = 2 * height * Hit.Time;
	INT stairRot = 0;
	if (Dist > 0.8 * height)
	{
		FVector Spot = EyeSpot + 0.5 * Dist * Dir;
		FLOAT Down = 3 * height;
		GWorld->SingleLineCheck(Hit, this, Spot - FVector(0,0,Down), Spot, TRACE_World, CollisionSlice);
		if (Hit.Time < 1.f)
		{
			FLOAT firstDown = Down * Hit.Time;
			if (firstDown < 0.7f * height - 6.f) // then up or level
			{
				Spot = EyeSpot + Dist * Dir;
				GWorld->SingleLineCheck(Hit, this, Spot - FVector(0,0,Down), Spot, TRACE_World, CollisionSlice);
				stairRot = ::Max(0, Rotation.Pitch);
				if ( Down * Hit.Time < firstDown - 10 )
					stairRot = 3600;
			}
			else if  (firstDown > 0.7f * height + 6.f) // then down or level
			{
				GWorld->SingleLineCheck(Hit, this, Pawn->Location + 0.9*Dist*Dir, Pawn->Location, TRACE_World|TRACE_StopAtAnyHit);
				if (Hit.Time == 1.f)
				{
					Spot = EyeSpot + Dist * Dir;
					GWorld->SingleLineCheck(Hit, this, Spot - FVector(0,0,Down), Spot, TRACE_World, CollisionSlice);
					if (Down * Hit.Time > firstDown + 10)
						stairRot = -4000;
				}
			}
		}
	}
	INT Diff = Abs(Rotation.Pitch - stairRot);
	if( (Diff > 0) && (GWorld->GetTimeSeconds() - GroundPitchTime > 0.25) )
	{
		FLOAT RotRate = 4;
		if( Diff < 1000 )
			RotRate = 4000/Diff;

		RotRate = ::Min(1.f, RotRate * deltaTime);
		stairRot = appRound(FLOAT(Rotation.Pitch) * (1 - RotRate) + FLOAT(stairRot) * RotRate);
	}
	else
	{
		if ( (Diff < 10) && (Abs(stairRot) < 10) )
			GroundPitchTime = GWorld->GetTimeSeconds();
		stairRot = Rotation.Pitch;
	}
	return stairRot;
}

/** Clears out 'left-over' audio components. */
void APlayerController::CleanUpAudioComponents()
{
	for (INT CompIndex = Components.Num() - 1; CompIndex >= 0; CompIndex--)
	{
		UComponent* Component = Components(CompIndex);
		UAudioComponent* AComp = Cast<UAudioComponent>(Component);
		if (AComp)
		{
			if (AComp->SoundCue == NULL)
			{
				AComp->Detach();
				Components.Remove(CompIndex);
			}
		}
		else if (Component == NULL)
		{
			// Clear out null entries...
			Components.Remove(CompIndex);
		}
	}
}

void AActor::execSuggestTossVelocity( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR_REF(TossVelocity);
	P_GET_VECTOR(Destination);
	P_GET_VECTOR(Start);
	P_GET_FLOAT(TossSpeed);
	P_GET_FLOAT_OPTX(BaseTossZ, 0.f);
	P_GET_FLOAT_OPTX(DesiredZPct, 0.05f);
	P_GET_VECTOR_OPTX(CollisionSize, FVector(0.f,0.f,0.f));
	P_GET_FLOAT_OPTX(TerminalVelocity, 0.f);
	P_GET_FLOAT_OPTX(OverrideGravityZ, 0.f);
	P_GET_UBOOL_OPTX(bOnlyTraceUp, FALSE);
	P_FINISH;

	*(DWORD*)Result = SuggestTossVelocity(&TossVelocity, Destination, Start, TossSpeed, BaseTossZ, DesiredZPct, CollisionSize, TerminalVelocity, OverrideGravityZ, bOnlyTraceUp);
}

UBOOL AController::ActorReachable(class AActor* actor)
{
	if ( !actor || !Pawn )
	{
		debugfSuppressed(NAME_DevPath,TEXT("Warning: No pawn or goal for ActorReachable by %s in %s"),*GetName(), *GetStateFrame()->Describe() );
		return 0;
	}

	// check if cached failed reach
	if ( (LastFailedReach == actor) && (FailedReachTime == GWorld->GetTimeSeconds())
		&& (FailedReachLocation == Pawn->Location) )
	{
		return 0;
	}
	else
	{
		INT Reach = Pawn->actorReachable(actor);
		if ( !Reach )
		{
			LastFailedReach = actor;
			FailedReachTime = GWorld->GetTimeSeconds();
			FailedReachLocation = Pawn->Location;
		}
		return Reach;
	}
}

UBOOL AController::PointReachable(FVector point)
{
	if ( !Pawn )
	{
		debugfSuppressed(NAME_DevPath,TEXT("Warning: No pawn for pointReachable by %s in %s"),*GetName(), *GetStateFrame()->Describe() );
		return 0;
	}

	return Pawn->pointReachable(point);
}

/* FindPathTo() and FindPathToward()
returns the best pathnode toward a point or actor - even if it is directly reachable
If there is no path, returns None
By default clears paths.  If script wants to preset some path weighting, etc., then
it can explicitly clear paths using execClearPaths before presetting the values and
calling FindPathTo with clearpath = 0
*/
AActor* AController::FindPath(const FVector& Point, AActor* Goal, UBOOL bWeightDetours, INT MaxPathLength, UBOOL bReturnPartial)
{
	if ( !Pawn )
	{
		debugfSuppressed(NAME_DevPath,TEXT("Warning: No pawn for FindPath by %s in %s"),*GetName(), *GetStateFrame()->Describe() );
		return NULL;
	}

	LastRouteFind = WorldInfo->TimeSeconds;
	AActor * bestPath = NULL;
	bPreparingMove = FALSE;

	if( Pawn->PathSearchType == PST_Constraint )
	{
		if( Pawn->GeneratePath() )
		{
			bestPath = SetPath();
		}
		Pawn->ClearConstraints();
	}
	else
	if (Pawn->findPathToward(Goal, Point, NULL, 0.f, bWeightDetours, MaxPathLength, bReturnPartial) > 0.f)
	{
		bestPath = SetPath();
	}

	return bestPath;
}

AActor* AController::FindPathTo( FVector Point, INT MaxPathLength, UBOOL bReturnPartial )
{
	return FindPath( Point, NULL, FALSE, MaxPathLength, bReturnPartial );
}

AActor* AController::FindPathToward( class AActor* goal, UBOOL bWeightDetours, INT MaxPathLength, UBOOL bReturnPartial )
{
	if ( !goal )
	{
		debugfSuppressed(NAME_DevPath,TEXT("Warning: No goal for FindPathToward by %s in %s"),*GetName(), *GetStateFrame()->Describe() );
		return NULL;
	}
	return FindPath(FVector(0,0,0), goal, bWeightDetours, MaxPathLength, bReturnPartial );
}

AActor* AController::FindPathToIntercept(class APawn* goal, class AActor* OtherRouteGoal, UBOOL bWeightDetours, INT MaxPathLength, UBOOL bReturnPartial )
{
    APawn *goalPawn = goal ? goal->GetAPawn() : NULL;
	if ( !goalPawn || !Pawn )
	{
		debugfSuppressed(NAME_DevPath,TEXT("Warning: No goal for FindPathToIntercept by %s in %s"),*GetName(), *GetStateFrame()->Describe() );
		return NULL;
	}
	// debugf(TEXT("%s Find path to intercept %s going to %s"),*Pawn->GetName(),*goal->GetName(),*OtherRouteGoal->GetName());
	if ( !Pawn->ValidAnchor() || !goalPawn->Controller || !OtherRouteGoal )
	{
		AActor *ResultPath = FindPath(FVector(0,0,0), goalPawn, bWeightDetours, MaxPathLength, bReturnPartial );
		return ResultPath;
	}
	UBOOL bFindDirectPath = true;
	UBOOL bHumanPathed = false;
	if ( goalPawn->IsHumanControlled() )
	{
		APlayerController *GoalPC = Cast<APlayerController>(goalPawn->Controller);
		if ( GoalPC && (goalPawn->Location != GoalPC->FailedPathStart) )
		{	
			bHumanPathed = (goalPawn->Controller->FindPath(FVector(0.f,0.f,0.f), OtherRouteGoal, FALSE, MaxPathLength, bReturnPartial )!= NULL);
			if ( !bHumanPathed )
				GoalPC->FailedPathStart = goalPawn->Location;
		}
	}

	if ( ((goalPawn->Controller->GetStateFrame()->LatentAction == AI_PollMoveToward) || (GWorld->GetTimeSeconds() - goalPawn->Controller->LastRouteFind < 0.75f))
		|| bHumanPathed )
	{
		// if already on path, movetoward goalPawn
		for (INT i=0; i<goalPawn->Controller->RouteCache.Num(); i++ )
		{
			if ( !goalPawn->Controller->RouteCache(i) )
				break;
			else
			{	
				bFindDirectPath = FALSE;
				if ( goalPawn->Controller->RouteCache(i) == Pawn->Anchor )
				{
//						debugf(TEXT("Already on path"));
					bFindDirectPath = TRUE;
					break;
				}
			}
		}
	}
	AActor *ResultActor = NULL;

	if ( bFindDirectPath )
	{
		ResultActor = FindPath(FVector(0.f,0.f,0.f), goalPawn, bWeightDetours, MaxPathLength, bReturnPartial );
	}
	else
	{
		ANavigationPoint* Nav = Cast<ANavigationPoint>(goalPawn->Controller->MoveTarget);
		if ( Nav )
		{
			Nav->bTransientEndPoint = TRUE;
		}

		for (INT i=0; i<goalPawn->Controller->RouteCache.Num(); i++ )
		{
			Nav = goalPawn->Controller->RouteCache(i);
			if ( Nav )
			{
				Nav->bTransientEndPoint = TRUE;
//					debugf(TEXT("Mark %s"),*Nav->GetName());
			}
			else
				break;
		}
		ResultActor = FindPath( FVector(0.f,0.f,0.f), goalPawn, bWeightDetours, MaxPathLength, bReturnPartial );
	}
	return ResultActor;
}

AActor* AController::FindPathTowardNearest(class UClass* GoalClass, UBOOL bWeightDetours, INT MaxPathLength, UBOOL bReturnPartial )
{
	if ( !GoalClass || !Pawn )
	{
		debugfSuppressed(NAME_DevPath,TEXT("Warning: No goal for FindPathTowardNearest by %s in %s"),*GetName(), *GetStateFrame()->Describe() );
		return NULL;
	}
	ANavigationPoint* Found = NULL;

	// mark appropriate Navigation points
	for ( ANavigationPoint* Nav=GWorld->GetFirstNavigationPoint(); Nav; Nav=Nav->nextNavigationPoint )
		if ( Nav->GetClass() == GoalClass )
		{
			Nav->bTransientEndPoint = true;
			Found = Nav;
		}
	if ( Found )
		return FindPath(FVector(0,0,0), Found, bWeightDetours, MaxPathLength, bReturnPartial );
	else
		return NULL;
}

/* FindRandomDest()
returns a random pathnode which is reachable from the creature's location.  Note that the path to
this destination is in the RouteCache.
*/
ANavigationPoint* AController::FindRandomDest()
{
	if ( !Pawn )
		return NULL;

	ANavigationPoint * bestPath = NULL;
	bPreparingMove = false;
	if( Pawn->findPathToward( NULL, FVector(0,0,0), &FindRandomPath, 0.f, FALSE, UCONST_BLOCKEDPATHCOST, FALSE ) > 0 )
	{
		bestPath = Cast<ANavigationPoint>(RouteGoal);
	}

	return bestPath;
}

void AController::SetFocalPoint( FVector FP, UBOOL bOffsetFromBase )
{
	FocalPosition.Set( (bOffsetFromBase && Pawn != NULL) ? Pawn->Base : NULL, FP );
}

FVector AController::GetFocalPoint()
{
	return *FocalPosition;
}

void AController::SetDestinationPosition( FVector Dest, UBOOL bOffsetFromBase )
{
	DestinationPosition.Set( (bOffsetFromBase && Pawn != NULL) ? Pawn->Base : NULL, Dest );
}

FVector AController::GetDestinationPosition()
{
	return *DestinationPosition;
}


void AController::SetAdjustLocation(FVector NewLoc, UBOOL bAdjust, UBOOL bOffsetFromBase)
{
}

void AAIController::SetAdjustLocation(FVector NewLoc, UBOOL bAdjust, UBOOL bOffsetFromBase)
{
	bAdjusting = bAdjust;
	AdjustPosition.Set( (bOffsetFromBase && Pawn != NULL) ? Pawn->Base : NULL, NewLoc );
}

FVector AController::GetAdjustLocation()
{
	return *AdjustPosition;
}


void AController::execLineOfSightTo( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(Other);
	P_GET_VECTOR_OPTX(chkLocation,FVector(0.f,0.f,0.f));
	P_GET_UBOOL_OPTX(bTryAlternateTargetLoc, FALSE);
	P_FINISH;

	if (chkLocation.IsZero())
	{
		*(DWORD*)Result = LineOfSightTo(Other, 0, NULL, bTryAlternateTargetLoc);
	}
	else
	{
		*(DWORD*)Result = LineOfSightTo(Other, 0, &chkLocation, bTryAlternateTargetLoc);
	}
}

/* execMoveTo()
start moving to a point -does not use routing
Destination is set to a point
*/
void AController::execMoveTo(FFrame& Stack, RESULT_DECL)
{
	P_GET_VECTOR(dest);
	P_GET_ACTOR_OPTX(viewfocus, NULL);
	P_GET_FLOAT_OPTX(DesiredOffset,0.f);
	P_GET_UBOOL_OPTX(bShouldWalk, (Pawn != NULL) ? Pawn->bIsWalking : 0);
	P_FINISH;

	MoveTo(dest,viewfocus,DesiredOffset,bShouldWalk);
}

/**
 * execMoveToDirectNonPathPos
 * - should be used when moving directly to a goal location (e.g. not following a path)
 */
void AController::execMoveToDirectNonPathPos(FFrame& Stack, RESULT_DECL)
{
	P_GET_VECTOR(dest);
	P_GET_ACTOR_OPTX(viewfocus, NULL);
	P_GET_FLOAT_OPTX(DesiredOffset,0.f);
	P_GET_UBOOL_OPTX(bShouldWalk, (Pawn != NULL) ? Pawn->bIsWalking : 0);
	P_FINISH;

	if(NavigationHandle != NULL)
	{
		NavigationHandle->SetFinalDestination(dest);
	}

	MoveTo(dest,viewfocus,DesiredOffset,bShouldWalk);
}

/** 
  * Controller sets pawns acceleration to move toward the specified destination dest.
  * Note that setting bPreciseDestination (Controller property) will result in more exactly reaching the destination position, by fine tuning the pawn velocity in APawn::CalcVelocity().
  */
void AController::MoveTo(const FVector& dest, AActor* viewfocus, FLOAT DesiredOffset, UBOOL bShouldWalk)
{
	if ( !Pawn )
		return;

	if ( bShouldWalk != Pawn->bIsWalking )
		Pawn->eventSetWalking(bShouldWalk);
	FVector MoveDir = dest - Pawn->Location;

	MoveTarget = NULL;
	Pawn->bReducedSpeed = FALSE;
	Pawn->DesiredSpeed = Pawn->MaxDesiredSpeed;
	Pawn->DestinationOffset = DesiredOffset;
	Pawn->NextPathRadius = 0.f;
	Focus = viewfocus;
	Pawn->setMoveTimer(MoveDir); 
	GetStateFrame()->LatentAction = AI_PollMoveTo;

	UBOOL bBasedDest = FALSE;
	FCheckResult Hit;
	if( !GWorld->SingleLineCheck( Hit, Pawn, dest + FVector(0,0,-100), dest, TRACE_World ) )
	{
		if( Hit.Actor == Pawn->Base )
		{
			bBasedDest = TRUE;
		}
	}

	SetDestinationPosition( dest, bBasedDest );
	if( !Focus )
	{
		SetFocalPoint( GetDestinationPosition(), bBasedDest );
	}
	CurrentPath = NULL;
	NextRoutePath = NULL;
	Pawn->ClearSerpentine();
	SetAdjustLocation( GetDestinationPosition(), FALSE );
	bAdvancedTactics = FALSE;
	Pawn->moveToward(GetDestinationPosition(), NULL);
}

void AController::execPollMoveTo( FFrame& Stack, RESULT_DECL )
{
	if( !Pawn || ((MoveTimer < 0.f) && (Pawn->Physics != PHYS_Falling)) )
	{
		PollMoveComplete();
		return;
	}
	if ( bAdjusting )
	{
		bAdjusting = !Pawn->moveToward(GetAdjustLocation(), NULL);
		if( !bAdjusting )
		{
			if( NavigationHandle != NULL && NavigationHandle->HandleFinishedAdjustMove() )
			{			
				return;
			}
		}
	}
	if (!bAdjusting)
	{
		PrePollMove();
		if( Pawn == NULL || Pawn->moveToward(GetDestinationPosition(), NULL))
		{
			PollMoveComplete();
		}
		else
		{
			PostPollMove();
		}
	}
}
IMPLEMENT_FUNCTION( AController, AI_PollMoveTo, execPollMoveTo);

void AController::PollMoveComplete()
{
	if( Pawn != NULL )
	{
		Pawn->Acceleration = FVector::ZeroVector;
	}
	GetStateFrame()->LatentAction = 0;
}

void AController::EndClimbLadder()
{
	if ( (GetStateFrame()->LatentAction == AI_PollMoveToward)
		&& Pawn && MoveTarget && MoveTarget->IsA(ALadder::StaticClass()) )
	{
		if ( Pawn->IsOverlapping(MoveTarget) )
			Pawn->SetAnchor(Cast<ANavigationPoint>(MoveTarget));
		GetStateFrame()->LatentAction = 0;
	}
}

/* execInLatentExecution()
returns true if controller currently performing latent execution with
passed in LatentAction value
*/
UBOOL AController::InLatentExecution(INT LatentActionNumber)
{
	return ( GetStateFrame()->LatentAction == LatentActionNumber );
}

void AController::StopLatentExecution()
{
	GetStateFrame()->LatentAction = 0;
	LatentFloat = -1.f;
}

/** activates path lanes for this Controller's current movement and adjusts its destination accordingly
 * @param DesiredLaneOffset the offset from the center of the Controller's CurrentPath that is desired
 * 				the Controller sets its LaneOffset as close as it can get to it without
 *				allowing any part of the Pawn's cylinder outside of the CurrentPath
 */
void AController::SetPathLane(FLOAT DesiredLaneOffset)
{
	// only enable if we're currently moving along the navigation network
	if (GetStateFrame()->LatentAction == AI_PollMoveToward && CurrentPath != NULL)
	{
		bUsingPathLanes = TRUE;
		// clamp the desired offset to what we can actually fit in
		FLOAT MaxOffset = FLOAT(CurrentPath->CollisionRadius) - Pawn->CylinderComponent->CollisionRadius;
		LaneOffset = Clamp<FLOAT>(DesiredLaneOffset, -MaxOffset, MaxOffset);
		// adjust to get in the lane
		FLOAT AdjustDist = LaneOffset + Pawn->CylinderComponent->CollisionRadius;
		if (LaneOffset > 0.f && !bAdjusting && Square(AdjustDist) < (Pawn->Location - CurrentPath->End->Location).SizeSquared2D())
		{
			FVector ClosestPointOnPath = (CurrentPath->Start->Location + (CurrentPathDir | (Pawn->Location - CurrentPath->Start->Location)) * CurrentPathDir);
			SetAdjustLocation( ClosestPointOnPath + (CurrentPathDir * AdjustDist) - ((CurrentPathDir ^ FVector(0.f, 0.f, 1.f)) * LaneOffset), TRUE );
		}
	}
}

/* MoveToward()
start moving toward a goal actor -does not use routing
MoveTarget is set to goal
*/
void AController::execMoveToward(FFrame& Stack, RESULT_DECL)
{
	P_GET_ACTOR(goal);
	P_GET_ACTOR_OPTX(viewfocus, NULL);
	P_GET_FLOAT_OPTX(DesiredOffset, 0.f);
	P_GET_UBOOL_OPTX(bStrafe, FALSE);
	P_GET_UBOOL_OPTX(bShouldWalk, (Pawn != NULL) ? Pawn->bIsWalking : 0);
	P_FINISH;

	MoveToward(goal,viewfocus,DesiredOffset,bStrafe,bShouldWalk);
}

void AController::MoveToward(AActor* goal, AActor* viewfocus, FLOAT DesiredOffset, UBOOL bStrafe, UBOOL bShouldWalk)
{
	if ( !goal || !Pawn )
	{
		//Stack.Log("MoveToward with no goal");
		return;
	}

	if( bShouldWalk != Pawn->bIsWalking )
	{
		Pawn->eventSetWalking( bShouldWalk );
	}

	Pawn->bReducedSpeed = false;
	Pawn->DesiredSpeed = Pawn->MaxDesiredSpeed;

	MoveTarget = goal;
	Focus = viewfocus ? viewfocus : goal;

	if ( (MoveTimer < -1.f) && (MoveTarget == FailedMoveTarget) )
	{
		MoveFailureCount++;
	}
	else
	{
		MoveFailureCount = 0;
		FailedMoveTarget = NULL;
	}

	UBOOL bAllowAdvancedTactics = (MoveTimer > -1.f);

	if( goal->GetAPawn() )
	{
		MoveTimer = MaxMoveTowardPawnTargetTime; //max before re-assess movetoward
	}
	else
	{
		FVector Move = goal->GetDestination( this ) - Pawn->Location;
		Pawn->setMoveTimer(Move);
	}

	SetDestinationPosition( MoveTarget->GetDestination( this ), (Pawn->Base == MoveTarget->Base) );
	GetStateFrame()->LatentAction = AI_PollMoveToward;

	SetAdjustLocation( GetDestinationPosition(), FALSE );
	Pawn->ClearSerpentine();
	bAdvancedTactics = bStrafe && bAllowAdvancedTactics && ( bForceStrafe || (GWorld->GetNetMode() != NM_Standalone) || (GWorld->GetTimeSeconds() - Pawn->LastRenderTime < 5.f) || bSoaking );
	bUsingPathLanes = FALSE;
	LaneOffset = 0.f;

	// if necessary, allow the pawn to prepare for this move
	// give pawn the opportunity if its a navigation network move,
	// based on the reachspec
	ANavigationPoint *NavGoal = Cast<ANavigationPoint>(MoveTarget);

	FLOAT NewDestinationOffset = 0.f;
	CurrentPath = NULL;
	NextRoutePath = NULL;
	if ( NavGoal )
	{
		// if the reachspec isn't currently supported by the pawn
		// then give the pawn an opportunity to perform some latent preparation
		// (Controller will set its bPreparingMove=true if it needs latent preparation)
		if (Pawn->Anchor != NULL)
		{
			UReachSpec* NewCurrentPath = Pawn->Anchor->GetReachSpecTo(NavGoal);
			if (NewCurrentPath != NULL && (Pawn->ValidAnchor() || NewCurrentPath->IsOnPath(Pawn->Location, Pawn->GetCylinderExtent().X)))
			{
				CurrentPath = NewCurrentPath;
			}
		}
		if (CurrentPath != NULL)
		{
			CurrentPath = PrepareForMove( NavGoal, CurrentPath );
			if ( CurrentPath )
			{
				NextRoutePath = GetNextRoutePath( NavGoal );
				CurrentPathDir = CurrentPath->End->Location - CurrentPath->Start->Location;
				CurrentPathDir = CurrentPathDir.SafeNormal();
				if( NavGoal->bSpecialMove )
				{
					NavGoal->eventSuggestMovePreparation( Pawn );
				}

				// If move target was cancelled... exit
				if( !MoveTarget )
				{
					return;
				}
				else
				{
					// Otherwise, reset destination in case move prep has changed MoveTarget
					SetDestinationPosition( MoveTarget->GetDestination( this ) );
					SetAdjustLocation( GetDestinationPosition(), FALSE );
				}

				FVector Dest = GetDestinationPosition();
				if (CurrentPath != NULL)
				{
					// handle leaving AVolumePathNodes (since don't go all the way to the center)
					if ( (Pawn->Physics == PHYS_Flying) || (Pawn->Physics == PHYS_Swimming) )
					{
						AVolumePathNode *StartFPN = Cast<AVolumePathNode>(CurrentPath->Start);
						// need to alter direction to get out of current AVolumePathNode region safely, if still in it
						if ( StartFPN && ((Abs(StartFPN->Location.Z - Dest.Z) > StartFPN->CylinderComponent->CollisionHeight)
									|| ((StartFPN->Location - Dest).Size2D() > StartFPN->CylinderComponent->CollisionRadius)) )
						{
							FCheckResult Hit(1.f);
							FVector Start = StartFPN->Location;
							if ( !Cast<AVolumePathNode>(CurrentPath->End) && ((StartFPN->Location - Dest).Size2D() < StartFPN->CylinderComponent->CollisionRadius) 
								&& (Dest.Z < StartFPN->Location.Z) )
							{
								Start = GetDestinationPosition();
								Start.Z = StartFPN->Location.Z - StartFPN->CylinderComponent->CollisionHeight + 10.f;
							}
							if ( !StartFPN->CylinderComponent->LineCheck(Hit,Dest, Start,FVector(0.f,0.f,0.f),0) )
							{
								SetAdjustLocation( Hit.Location, TRUE );
							}
						}
					}

					// if other AI pawns are using this path, use path lanes
					if(ShouldUsePathLanes())
					{
						UReachSpec* OppositePath = NavGoal->GetReachSpecTo(Pawn->Anchor);

						// construct a list of Controllers using the same path or the opposite path
						TArray<AController*> PathMates;
						PathMates.Reserve(4);
						for (AController* C = WorldInfo->ControllerList; C != NULL; C = C->NextController)
						{
							if (C == this)
							{
								PathMates.AddItem(C);
							}
							else if (C->Pawn != NULL)
							{
								// if the other Controller is moving along the same path or will be soon
								if ( C->GetStateFrame()->LatentAction == AI_PollMoveToward &&
									( (C->CurrentPath != NULL && (C->CurrentPath == CurrentPath || C->CurrentPath == OppositePath)) ||
									(C->NextRoutePath != NULL && (C->NextRoutePath == CurrentPath || C->NextRoutePath == OppositePath)) ) )
								{
									PathMates.AddItem(C);
								}
								// or if it's sitting on top of our endpoint
								else if (C->Pawn->Velocity.IsZero() && C->Pawn->Acceleration.IsZero() && C->Pawn->Anchor == NavGoal && C->Pawn->ValidAnchor())
								{
									PathMates.AddItem(C);
								}
							}
						}

						if (PathMates.Num() > 1)
						{
							// allocate lanes for each Pawn on the path, starting from our far right
							FLOAT CurrentLaneOffset = FLOAT(CurrentPath->CollisionRadius);
							// set our lane first
							//SetPathLane(CurrentLaneOffset);
							// take the offset we actually used and find out where the next pawn's lane should start
							//CurrentLaneOffset = LaneOffset - Pawn->CylinderComponent->CollisionRadius;
							// now do the same for the other Pawns
							for (INT i = 0; i < PathMates.Num(); i++)
							{
								// if this is the last one, always force it all the way to the left
								if (i == PathMates.Num() - 1)
								{
									CurrentLaneOffset = -FLOAT(CurrentPath->CollisionRadius);
								}
								// if this one is going in the opposite direction, we have to take the negative of everything
								if (PathMates(i)->CurrentPath == OppositePath || PathMates(i)->NextRoutePath == OppositePath)
								{
									PathMates(i)->SetPathLane(-CurrentLaneOffset + PathMates(i)->Pawn->CylinderComponent->CollisionRadius);
									CurrentLaneOffset = -PathMates(i)->LaneOffset - PathMates(i)->Pawn->CylinderComponent->CollisionRadius;
								}
								else
								{
									PathMates(i)->SetPathLane(CurrentLaneOffset - PathMates(i)->Pawn->CylinderComponent->CollisionRadius);
									CurrentLaneOffset = PathMates(i)->LaneOffset - PathMates(i)->Pawn->CylinderComponent->CollisionRadius;
								}
							}
						}
					}// if ShouldUsePathLanes

				}
			}
		}
		else if( NavGoal->bSpecialMove )
		{
			NavGoal->eventSuggestMovePreparation(Pawn);		
		}

		if ( !NavGoal->bNeverUseStrafing && !NavGoal->bForceNoStrafing )
		{
			if( CurrentPath )
			{
				Pawn->InitSerpentine();
			}
			if( NavGoal != RouteGoal )
			{
				NewDestinationOffset = (0.7f + 0.3f * appFrand()) * 
											::Max(0.f, Pawn->NextPathRadius - Pawn->CylinderComponent->CollisionRadius);
			}
		}
	}

	Pawn->DestinationOffset = (DesiredOffset == 0.f) ? NewDestinationOffset : DesiredOffset;
	Pawn->NextPathRadius = 0.f;

	if( !bPreparingMove )
	{
		Pawn->moveToward(GetDestinationPosition(), MoveTarget);
	}
}

UReachSpec* AController::PrepareForMove( ANavigationPoint *NavGoal, UReachSpec* Path )
{
	Path->PrepareForMove( this );
	return Path;
}

UReachSpec* AController::GetNextRoutePath( ANavigationPoint *NavGoal )
{
	UReachSpec* Spec = NULL;
	if(  RouteGoal && 
		(NavGoal == CurrentPath->End) && 
		(NavGoal != RouteGoal) )
	{
		for( INT i = 0; i < RouteCache.Num() - 1; i++ )
		{
			if( !RouteCache(i) )
			{
				break;
			}
			else if( RouteCache(i) == CurrentPath->End )
			{
				ANavigationPoint *NextNav = RouteCache(i+1);
				if ( NextNav )
				{
					Spec = CurrentPath->End.Nav()->GetReachSpecTo(NextNav);
				}
				break;
			}
		}
	}

	return Spec;
}

FVector AActor::GetDestination(AController* C)
{
	FVector Dest = Location;
	if (C != NULL && C->Pawn != NULL && C->Pawn->bModifyNavPointDest)
	{
		Dest += C->Pawn->AdjustDestination(this, Dest);
	}
	return Dest;
}
#if 0
#define RESET_REASON(REASON) \
	debugf(TEXT("(GT:%.2f) %s resetting latent action for poll movetoward because %s "),GWorld->GetTimeSeconds(),*GetName(),TEXT(REASON));
#else
#define RESET_REASON(REASON)
#endif

void AController::execPollMoveToward( FFrame& Stack, RESULT_DECL )
{
	if( !MoveTarget || !Pawn || ((MoveTimer < 0.f) && (Pawn->Physics != PHYS_Falling)) )
	{	
		RESET_REASON("MoveTimer?");
		PollMoveComplete();
		return;
	}
	// check that pawn is ready to go
	if ( bPreparingMove )
		return;
	// check if adjusting around an obstacle
	if ( bAdjusting )
	{
		bAdjusting = !Pawn->moveToward(GetAdjustLocation(), MoveTarget);
		if( !bAdjusting )
		{
			if( NavigationHandle != NULL && NavigationHandle->HandleFinishedAdjustMove() )
			{			
				return;
			}
		}
	}
	if ( !MoveTarget || !Pawn )
	{
		RESET_REASON("!MoveTarget || !Pawn");
		PollMoveComplete();
		return;
	}
	if ( bAdjusting && Cast<AVolumePathNode>(MoveTarget) )
	{
		if ( Pawn->ReachedDestination(Pawn->Location,MoveTarget->Location, MoveTarget) )
		{
			RESET_REASON("Reached Destination");
			PollMoveComplete();
			return;
		}
		else if ( (Pawn->Velocity | (GetAdjustLocation() - Pawn->Location)) < 0.f )
		{
			bAdjusting = FALSE;
		}
	}

	if ( !bAdjusting )
	{
		// set destination to current movetarget location
		FVector StartingDest = MoveTarget->GetDestination(this);
		FVector Dest = StartingDest;
		FLOAT MoveTargetRadius, MoveTargetHeight;
		MoveTarget->GetBoundingCylinder(MoveTargetRadius, MoveTargetHeight);

		if( Pawn->Physics==PHYS_Flying )
		{
			if ( MoveTarget->GetAPawn() )
			{
				if ( MoveTarget->GetAPawn()->bStationary )
				{
					Dest.Z += 2.f * ::Max(MoveTargetHeight, 2.5f * Pawn->CylinderComponent->CollisionHeight);
				}
				else
				{
					Dest.Z += 0.7f * MoveTargetHeight;
				}
			}
			else if ( MoveTarget->IsA(ANavigationPoint::StaticClass()) && !MoveTarget->IsA(AVolumePathNode::StaticClass()) )
			{
				if ( MoveTarget->IsA(ALiftExit::StaticClass()) && CurrentPath && CurrentPath->Start->IsA(ALiftCenter::StaticClass()) )
				{
					Dest = Pawn->Location;
					Dest.Z = MoveTarget->Location.Z;
				}
				else if ( Pawn->Location.Z < Dest.Z )
				{
					if ( Pawn->Location.Z > Dest.Z - MoveTargetHeight )
					{
						Dest.Z += MoveTargetHeight;
					}
					else
					{
						Dest.Z += 500.f;
					}
				}
			}
		}
		else if( Pawn->Physics == PHYS_Spider )
		{
			Dest = Dest - MoveTargetRadius * Pawn->Floor;
		}
		SetDestinationPosition( Dest );

		FLOAT oldDesiredSpeed = Pawn->DesiredSpeed;

		PrePollMove();

		// move to movetarget
		if (Pawn->moveToward(GetDestinationPosition(), MoveTarget))
		{
			RESET_REASON("Pawn->moveToward returned TRUE");
			PollMoveComplete();
		}
		else if ( MoveTarget && Pawn && (Pawn->Physics == PHYS_Walking) )
		{
			FVector Diff = Pawn->Location - GetDestinationPosition();
			FLOAT DiffZ = Diff.Z;
			Diff.Z = 0.f;
			// reduce timer if seem to be stuck above or below
			if ( Diff.SizeSquared() < Pawn->CylinderComponent->CollisionRadius * Pawn->CylinderComponent->CollisionRadius )
			{
				MoveTimer -= Pawn->AvgPhysicsTime;
				if ( DiffZ > Pawn->CylinderComponent->CollisionRadius + 2 * Pawn->MaxStepHeight )
				{
					// check if visible below
					FCheckResult Hit(1.f);
					GWorld->SingleLineCheck(Hit, Pawn, GetDestinationPosition(), Pawn->Location, TRACE_World|TRACE_StopAtAnyHit);
					if ( (Hit.Time < 1.f) && (Hit.Actor != MoveTarget) )
					{
						RESET_REASON("I seem to be stuck above destination position");
						PollMoveComplete();
					}
				}
			}
		}
		if ( !MoveTarget || !Pawn )
		{
			RESET_REASON("!MoveTarget || !Pawn (2)");
			PollMoveComplete();
			return;
		}
		if ( GetStateFrame()->LatentAction != 0 )
		{
			PostPollMove();
		}

		SetDestinationPosition( StartingDest );

		if( MoveTarget->GetAPawn() )
		{
			Pawn->DesiredSpeed = oldDesiredSpeed; //don't slow down when moving toward a pawn
			if ( !Pawn->bCanSwim && MoveTarget->PhysicsVolume->bWaterVolume )
			{
				FailMove();
			}
		}
	}
}
IMPLEMENT_FUNCTION( AController, AI_PollMoveToward, execPollMoveToward);

/* execTurnToward()
turn toward Focus
*/
void AController::FinishRotation()
{
	GetStateFrame()->LatentAction = AI_PollFinishRotation;
}

void AController::execPollFinishRotation( FFrame& Stack, RESULT_DECL )
{

	if( !Pawn || Pawn->ReachedDesiredRotation() )
	{
		GetStateFrame()->LatentAction = 0;

	}

}
IMPLEMENT_FUNCTION( AController, AI_PollFinishRotation, execPollFinishRotation);

UBOOL AController::BeyondFogDistance(FVector ViewPoint, FVector OtherPoint)
{
	return FALSE;
}

/*
SeePawn()

returns true if Other was seen by this controller's pawn.  Chance of seeing other pawn decreases with increasing
distance or angle in peripheral vision
*/
DWORD AController::SeePawn(APawn *Other, UBOOL bMaySkipChecks)
{
	if ( !Other || !Pawn || Other->IsInvisible() )
		return 0;

	if (Other != Enemy)
		bLOSflag = !bLOSflag;
	else
		return LineOfSightTo(Other);

	if ( BeyondFogDistance(Pawn->Location, Other->Location) )
		return 0;

	FLOAT maxdist = Pawn->SightRadius;

	// fixed max sight distance
	if ( (Other->Location - Pawn->Location).SizeSquared() > maxdist * maxdist )
		return 0;

	FLOAT dist = (Other->Location - Pawn->Location).Size();

	// may skip if more than 1/5 of maxdist away (longer time to acquire)
	if ( bMaySkipChecks && (appFrand() * dist > 0.1f * maxdist) )
			return 0;

	// check field of view
	FVector SightDir = (Other->Location - Pawn->Location).SafeNormal();
	FVector LookDir = Rotation.Vector();
	FLOAT Stimulus = (SightDir | LookDir);
	if ( Stimulus < Pawn->PeripheralVision )
		return 0;

	// need to make this only have effect at edge of vision
	//if ( bMaySkipChecks && (appFrand() * (1.f - Pawn->PeripheralVision) < 1.f - Stimulus) )
	//	return 0;
	if ( bMaySkipChecks && bSlowerZAcquire && (appFrand() * dist > 0.1f * maxdist) )
	{
		// lower FOV vertically
		SightDir.Z *= 2.f;
		SightDir.Normalize();
		if ( (SightDir | LookDir) < Pawn->PeripheralVision )
			return 0;

		// notice other pawns at very different heights more slowly
		FLOAT heightMod = Abs(Other->Location.Z - Pawn->Location.Z);
		if ( appFrand() * dist < heightMod )
			return 0;
	}

	Stimulus = 1;
	return LineOfSightTo(Other, bMaySkipChecks);

}

AActor* AController::GetViewTarget()
{
	if ( Pawn )
		return Pawn;
	return this;
}

void APlayerController::UpdateViewTarget(AActor* NewViewTarget)
{
	if ( (NewViewTarget == ViewTarget) || !NewViewTarget )
		return;

	AActor* OldViewTarget = ViewTarget;
	ViewTarget = NewViewTarget;

	ViewTarget->eventBecomeViewTarget(this);
	if ( OldViewTarget )
		OldViewTarget->eventEndViewTarget(this);

	if (!bPendingDelete && !IsLocalPlayerController() && WorldInfo->NetMode != NM_Client)
	{
		// inform remote client via rpc
		eventClientSetViewTarget(ViewTarget);
	}
}
	
AActor* APlayerController::GetViewTarget()
{
	if( PlayerCamera )
	{
		return PlayerCamera->GetViewTarget();
	}

	if ( RealViewTarget && !RealViewTarget->bDeleteMe )
	{
		if ( !ViewTarget || ViewTarget->bDeleteMe || !ViewTarget->GetAPawn() || (ViewTarget->GetAPawn()->PlayerReplicationInfo != RealViewTarget) )
		{
			// not viewing pawn associated with RealViewTarget, so look for one
			// Assuming on server, so PRI Owner is valid
			if ( !RealViewTarget->Owner )
			{
				RealViewTarget = NULL;
			}
			else
			{
				AController* PRIOwner = RealViewTarget->Owner->GetAController();
				if ( PRIOwner )
				{
					if ( PRIOwner->GetAPlayerController() && PRIOwner->GetAPlayerController()->ViewTarget && !PRIOwner->GetAPlayerController()->ViewTarget->bDeleteMe )
					{
						UpdateViewTarget(PRIOwner->GetAPlayerController()->ViewTarget);
					}
					else if ( PRIOwner->Pawn )
					{
						UpdateViewTarget(PRIOwner->Pawn);
					}
				}
				else
				{
					RealViewTarget = NULL;
				}
			}
		}
	}

	if ( !ViewTarget || ViewTarget->bDeleteMe )
	{
		if ( Pawn && !Pawn->bDeleteMe && !Pawn->bPendingDelete )
			UpdateViewTarget(Pawn);
		else
			UpdateViewTarget(this);
	}
	return ViewTarget;
}

/** Set the player controller to treat NewTarget as its new view target. */
void APlayerController::SetViewTarget(class AActor* NewViewTarget, struct FViewTargetTransitionParams TransitionParams)
{
	// if we're being controlled by a director track, update it with the new viewtarget 
	// so it returns to the proper viewtarget when it finishes.
	UInterpTrackInstDirector* const Director = GetControllingDirector();
	if (Director)
	{
		Director->OldViewTarget = NewViewTarget;
	}

	// Player camera overrides player controller implementation
	if( PlayerCamera )
	{
		PlayerCamera->SetViewTarget( NewViewTarget, TransitionParams );
		return;
	}

	if ( !NewViewTarget )
		NewViewTarget = this;

	// Update RealViewTarget (used to follow same player through pawn transitions, etc., when spectating)
	if (NewViewTarget == this || NewViewTarget == Pawn) 
	{	
		RealViewTarget = NULL;
	}
	else if ( NewViewTarget->GetAController() )
	{
		RealViewTarget = NewViewTarget->GetAController()->PlayerReplicationInfo;
	}
	else if ( NewViewTarget->GetAPawn() )
	{
		RealViewTarget = NewViewTarget->GetAPawn()->PlayerReplicationInfo;
	}
	else if ( Cast<APlayerReplicationInfo>(NewViewTarget) )
	{
		RealViewTarget = Cast<APlayerReplicationInfo>(NewViewTarget);
	}
	else
	{
		RealViewTarget = NULL;
	}
	
	UpdateViewTarget(NewViewTarget);

	if (GWorld->GetNetMode() != NM_Client)
		GetViewTarget();

	if ( ViewTarget == this  ) 
		RealViewTarget = NULL;
}



/**
 * Sets the Matinee director track instance that's currently possessing this player controller
 *
 * @param   NewControllingDirector    The director track instance that's now controlling this player controller (or NULL for none)
 */
void APlayerController::SetControllingDirector( UInterpTrackInstDirector* NewControllingDirector )
{
	ControllingDirTrackInst = NewControllingDirector;
}



/**
 * Returns the Matinee director track that's currently possessing this player controller, or NULL for none
 */
UInterpTrackInstDirector* APlayerController::GetControllingDirector()
{
	return ControllingDirTrackInst;
}


#define DEBUG_LOS 0
/*
 * LineOfSightTo()
 * returns TRUE if controller's pawn can see Other actor.
 * Checks line to center of other actor, and possibly to head or box edges depending on distance
 */
DWORD AController::LineOfSightTo(const AActor* Other, INT bUseLOSFlag, const FVector* chkLocation, UBOOL bTryAlternateTargetLoc)
{
	if( !Other )
	{
		return 0;
	}

#if DEBUG_LOS
	debugf(TEXT("AController::LineOfSightTo. Controller: %s, Target: %s"), *GetName(), *Other->GetName());
#endif

	FVector ViewPoint;
	// check for a viewpoint override
	if( chkLocation != NULL )
	{
		ViewPoint = *chkLocation;
	}
	else
	{
		AActor*	ViewTarg = GetViewTarget();
		ViewPoint = ViewTarg->Location;
		if( ViewTarg == Pawn )
		{
			ViewPoint.Z += Pawn->BaseEyeHeight; //look from eyes
		}
	}

	if( BeyondFogDistance(ViewPoint, Other->Location) )
	{
#if DEBUG_LOS
		debugf(TEXT("AController::LineOfSightTo. BeyondFogDistance"));
#endif
		return 0;
	}

	FLOAT OtherRadius, OtherHeight;
	Other->GetBoundingCylinder(OtherRadius, OtherHeight);

	FCheckResult Hit(1.f);
	if( Other == Enemy )
	{
#if DEBUG_LOS
		debugf(TEXT("AController::LineOfSightTo. Enemy Branch"));
#endif
		GWorld->SingleLineCheck( Hit, this, Other->Location, ViewPoint, TRACE_World|TRACE_StopAtAnyHit|TRACE_ComplexCollision );

		if( Hit.Actor && (Hit.Actor != Other) )
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. HitActor: %s"), *Hit.Actor->GetName());
#endif
			GWorld->SingleLineCheck( Hit, this, Enemy->Location + FVector(0,0,Enemy->BaseEyeHeight), ViewPoint, TRACE_World|TRACE_StopAtAnyHit|TRACE_ComplexCollision );
		}

		if( !Hit.Actor || (Hit.Actor == Other) )
		{
			// update enemy info 
			// NOTE that controllers update this info even if the enemy is behind them
			// unless they check for this in UpdateEnemyInfo()
			UpdateEnemyInfo(Enemy);
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. LOS successful #1"));
#endif
			return 1;
		}

		// only check sides if width of other is significant compared to distance
		if( OtherRadius * OtherRadius/(Other->Location - ViewPoint).SizeSquared() < 0.0001f )
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. Don't check sides."));
#endif
			return 0;
		}
	}
	else
	{
#if DEBUG_LOS
		debugf(TEXT("AController::LineOfSightTo. No Enemy Branch"));
#endif

		FVector TargetLocation = Other->GetTargetLocation(Pawn, bTryAlternateTargetLoc);
		GWorld->SingleLineCheck( Hit, this, TargetLocation, ViewPoint, TRACE_World|TRACE_StopAtAnyHit|TRACE_ComplexCollision );
		if( !Hit.Actor || (Hit.Actor == Other) )
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. No Actor Hit - success."));
#endif
			return 1;
		}
		else
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. Hit: %s."), *Hit.Actor->GetName());
#endif
		}

		// if other isn't using a cylinder for collision and isn't a Pawn (which already requires an accurate cylinder for AI)
		// then don't go any further as it likely will not be tracing to the correct location
		UBOOL bTargetIsPawn = (Other->GetAPawn() != NULL);
		if (!bTargetIsPawn && Cast<UCylinderComponent>(Other->CollisionComponent) == NULL)
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. No Pawn - fail."));
#endif
			return 0;
		}
		FLOAT distSq = (Other->Location - ViewPoint).SizeSquared();
		if ( distSq > FARSIGHTTHRESHOLDSQUARED )
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. Too far - fail."));
#endif
			return 0;
		}
		if ( (!bIsPlayer || !bTargetIsPawn) && (distSq > NEARSIGHTTHRESHOLDSQUARED) ) 
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. Not player/pawn target - fail."));
#endif
			return 0;
		}
		
		//try viewpoint to head
		if ( !bUseLOSFlag || !bLOSflag )
		{
			GWorld->SingleLineCheck( Hit, this, Other->Location + FVector(0,0,OtherHeight), ViewPoint, TRACE_World|TRACE_StopAtAnyHit|TRACE_ComplexCollision );
			if ( !Hit.Actor || (Hit.Actor == Other) )
			{
#if DEBUG_LOS
				debugf(TEXT("AController::LineOfSightTo. Head check - success."));
#endif
				return 1;
			}
		}

		// bLOSFlag used by SeePawn to reduce visibility checks
		if ( bUseLOSFlag && !bLOSflag )
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. bUseLOSFlag - fail."));
#endif
			return 0;
		}
		// only check sides if width of other is significant compared to distance
		if ( OtherRadius * OtherRadius/distSq < 0.00015f )
		{
#if DEBUG_LOS
			debugf(TEXT("AController::LineOfSightTo. Width/distance - fail."));
#endif
			return 0;
		}
	}

	if( !bSkipExtraLOSChecks )
	{
		//try checking sides - look at dist to four side points, and cull furthest and closest
		FVector Points[4];
		Points[0] = Other->Location - FVector(OtherRadius, -1 * OtherRadius, 0);
		Points[1] = Other->Location + FVector(OtherRadius, OtherRadius, 0);
		Points[2] = Other->Location - FVector(OtherRadius, OtherRadius, 0);
		Points[3] = Other->Location + FVector(OtherRadius, -1 * OtherRadius, 0);
		INT imin = 0;
		INT imax = 0;
		FLOAT currentmin = (Points[0] - ViewPoint).SizeSquared();
		FLOAT currentmax = currentmin;
		for ( INT i=1; i<4; i++ )
		{
			FLOAT nextsize = (Points[i] - ViewPoint).SizeSquared();
			if (nextsize > currentmax)
			{
				currentmax = nextsize;
				imax = i;
			}
			else if (nextsize < currentmin)
			{
				currentmin = nextsize;
				imin = i;
			}
		}

		for ( INT i=0; i<4; i++ )
		{
			if	( (i != imin) && (i != imax) )
			{
				GWorld->SingleLineCheck( Hit, this, Points[i], ViewPoint, TRACE_World|TRACE_StopAtAnyHit|TRACE_ComplexCollision );
				if ( !Hit.Actor || (Hit.Actor == Other) )
				{
#if DEBUG_LOS
					debugf(TEXT("AController::LineOfSightTo. Extra check - success."));
#endif
					return 1;
				}
			}
		}
	}

#if DEBUG_LOS
	debugf(TEXT("AController::LineOfSightTo. Default - fail."));
#endif

	return 0;
}

/* CanHear()

Returns 1 if controller can hear this noise
Several types of hearing are supported

Noises must be perceptible (based on distance, loudness, and the alerntess of the controller

  Options for hearing are: (assuming the noise is perceptible

  bLOSHearing = Hear any perceptible noise which is not blocked by geometry
  bMuffledHearing = Hear occluded noises if closer
*/
UBOOL AController::CanHear(const FVector& NoiseLoc, FLOAT Loudness, AActor *Other)
{
	if ( !Other->Instigator || !Other->Instigator->Controller || !Pawn )
		return FALSE; //ignore sounds from uncontrolled (dead) pawns, or if don't have a pawn to control

	FLOAT DistSq = (Pawn->Location - NoiseLoc).SizeSquared();
	FLOAT Perceived = Loudness * Pawn->HearingThreshold * Pawn->HearingThreshold;

	// take pawn alertness into account (it ranges from -1 to 1 normally)
	Perceived *= ::Max(0.f,(Pawn->Alertness + 1.f));

	// check if sound is too quiet to hear
	if ( Perceived < DistSq )
		return FALSE;

	// if not checking for occlusion, then we're done
	if ( !Pawn->bLOSHearing )
		return TRUE;

	// if can hear muffled sounds, see if they are within half range
	if ( Pawn->bMuffledHearing && (Perceived > 4.f * DistSq) )
	{
		return TRUE;
	}

	// check if sound is occluded
	FVector ViewLoc = Pawn->Location + FVector(0.f,0.f,Pawn->BaseEyeHeight);
	FCheckResult Hit(1.f);
	GWorld->SingleLineCheck(Hit, this, NoiseLoc, ViewLoc, TRACE_Level);
	return ( Hit.Time == 1.f );
}

void AController::CheckEnemyVisible()
{
	if ( Enemy )
	{
		check(Enemy->IsValid());
		if ( !LineOfSightTo(Enemy) )
			eventEnemyNotVisible();
	}
}

/* Player shows self to pawns that are ready
*/
void AController::ShowSelf()
{
	if ( !Pawn )
		return;
	for ( AController *C=GWorld->GetFirstController(); C!=NULL; C=C->NextController )
	{
		if( C!=this && C->ShouldCheckVisibilityOf(this) && C->SeePawn(Pawn) )
		{
			if ( bIsPlayer )
				C->eventSeePlayer(Pawn);
			else
				C->eventSeeMonster(Pawn);
		}
	}
}

/** ShouldCheckVisibilityOf()
returns true if should check whether pawn controlled by controller C is visible
*/
UBOOL AController::ShouldCheckVisibilityOf(AController *C)
{
	// only check visibility if this or C is a player, and sightcounter has counted down, and is probing event.
	if ( (bIsPlayer || C->bIsPlayer) && (SightCounter < 0.f) && (C->bIsPlayer ? IsProbing(NAME_SeePlayer) : IsProbing(NAME_SeeMonster)) )
	{
		// don't check visibility if on same team if bSeeFriendly==false
		return ( bSeeFriendly || (WorldInfo->Game && !WorldInfo->Game->bTeamGame) || !PlayerReplicationInfo || !PlayerReplicationInfo->Team 
			|| !C->PlayerReplicationInfo || !C->PlayerReplicationInfo->Team
			|| (PlayerReplicationInfo->Team != C->PlayerReplicationInfo->Team) );
	}
	return FALSE;
}

/*
SetPath()
Based on the results of the navigation network (which are stored in RouteCache[],
return the desired path.  Check if there are any intermediate goals (such as hitting a
switch) which must be completed before continuing toward the main goal
*/
AActor* AController::SetPath(INT bInitialPath)
{
	static AActor* ChosenPaths[4];

	if ( RouteCache.Num() == 0 )
		return NULL;

	AActor * bestPath = RouteCache(0);

	if ( !Pawn->ValidAnchor() )
		return bestPath;	// make sure on network before trying to find complex routes

	if ( bInitialPath )
	{
		for ( INT i=0; i<4; i++ )
			ChosenPaths[i] = NULL;
		// if this is setting the path toward the main (final) goal
		// make sure still same goal as before
		if ( RouteGoal == GoalList[0] )
		{
			// check for existing intermediate goals
			if ( GoalList[1] )
			{
				INT i = 1;
				while (i < 4 && GoalList[i] != NULL)
				{
					i++;
				}
				AActor* RealGoal = GoalList[i-1];
				if ( Pawn->actorReachable(RealGoal) )
				{
					// I can reach the intermediate goal, so
					GoalList[i-1] = NULL;
					bPreparingMove = false;
					return RealGoal;
				}
				// find path to new goal
				UBOOL bOldPrep = bPreparingMove;
				bPreparingMove = false;
				if ( Pawn->findPathToward( RealGoal,RealGoal->Location,NULL, 0.f,FALSE,UCONST_BLOCKEDPATHCOST, FALSE ) > 0.f )
				{
					bestPath = SetPath(0);
				}
				else
				{
					bPreparingMove = bOldPrep;
				}
			}
		}
		else
		{
			GoalList[0] = RouteGoal;
			for ( INT i=1; i<4; i++ )
				GoalList[i] = NULL;
		}
	}
	else
	{
		// add new goal to goal list
		for ( INT i=0; i<4; i++ )
		{
			if ( GoalList[i] == RouteGoal )
				break;
			if ( !GoalList[i] )
			{
				GoalList[i] = RouteGoal;
				break;
			}
		}
	}
	for ( INT i=0; i<4; i++ )
	{
		if ( ChosenPaths[i] == NULL )
		{
			ChosenPaths[i] = bestPath;
			break;
		}
		else if ( ChosenPaths[i] == bestPath )
			return bestPath;
	}
	if ( bestPath && bestPath->IsProbing(NAME_SpecialHandling) )
		bestPath = HandleSpecial(bestPath);
	return bestPath;
}

/** Allows operations on nodes in the route before emptying the cache */
void AController::RouteCache_Empty()
{
	RouteCache.Empty();
}
void AController::RouteCache_AddItem( ANavigationPoint* Nav )
{
	if( Nav )
	{
		RouteCache.AddItem( Nav );
	}
}
void AController::RouteCache_InsertItem( ANavigationPoint* Nav, INT Idx )
{
	if( Nav )
	{
		RouteCache.InsertItem( Nav, Idx );
	}
}
void AController::RouteCache_RemoveItem( ANavigationPoint* Nav )
{
	if( Nav )
	{
		RouteCache.RemoveItem( Nav );
	}
}
void AController::RouteCache_RemoveIndex( INT Index, INT Count )
{
	if( Index >= 0 && Index < RouteCache.Num() )
	{
		RouteCache.Remove( Index, Count );
	}
}

AActor* AController::HandleSpecial(AActor *bestPath)
{
	if ( !bCanDoSpecial || GoalList[3] )
		return bestPath;	//limit AI intermediate goal depth to 4

	AActor * newGoal = bestPath->eventSpecialHandling(Pawn);

	if ( newGoal && (newGoal != bestPath) )
	{
		UBOOL bOldPrep = bPreparingMove;
		bPreparingMove = false;
		// if can reach intermediate goal directly, return it
		if ( Pawn->actorReachable(newGoal) )
		{
			return newGoal;
		}

		// find path to new goal
		if( Pawn->findPathToward( newGoal, newGoal->Location, NULL, 0.f, FALSE, UCONST_BLOCKEDPATHCOST, FALSE ) > 0.f )
		{
			bestPath = SetPath(0);
		}
		else
		{
			debugfSuppressed(NAME_DevPath, TEXT("Failed to find path to special goal %s on the way to %s"), *newGoal->GetName(), *bestPath->GetName());
			bPreparingMove = bOldPrep;
		}
	}
	return bestPath;

}

/* AcceptNearbyPath() returns true if the controller will accept a path which gets close to
and withing sight of the destination if no reachable path can be found.
*/
INT AController::AcceptNearbyPath(AActor *goal)
{
	return 0;
}

INT AAIController::AcceptNearbyPath(AActor *goal)
{
	if ( Cast<AVehicle>(Pawn) )
		return true;
	return (goal && (goal->GetAPawn() || (goal->Physics == PHYS_Falling)) );
}

/* ForceReached()
* Controller can override Pawn NavigationPoint reach test
* return true if want to force successful reach by pawn of Nav
*/
UBOOL AController::ForceReached(ANavigationPoint *Nav, const FVector& TestPosition)
{
	return FALSE;
}

/** JumpOverWall()
Make pawn jump over an obstruction
*/
void AController::JumpOverWall(FVector WallNormal)
{
	FVector Dir = DesiredDirection();
	Dir.Z = 0.f;
	Dir = Dir.SafeNormal();

	// check distance to destination vs how we'll slide along wall
	if ( WallNormal.Z != 0.f )
	{
		WallNormal.Z = 0.f;
		WallNormal = WallNormal.SafeNormal();
	}
	WallNormal *= -1.f;
	// if the opposite of the wall normal is approximately in the direction we want to go anyway, then jump directly over it
	FLOAT DotP = Dir | WallNormal;
	if ( (DotP > 0.8f) || (DesiredDirection().Size2D() < 6.f*Pawn->CylinderComponent->CollisionRadius) )
	{
		Dir = WallNormal;
	}
	else if (CurrentPath != NULL && CurrentPath->Start != NULL)
	{
		// if the opposite of the wall normal is in the same direction as the closest point of our current path line, then jump directly over it
		FVector ClosestPoint;
		if ( PointDistToLine(Pawn->Location, CurrentPathDir, CurrentPath->Start->Location, ClosestPoint) > Pawn->CylinderComponent->CollisionRadius &&
			((ClosestPoint - Pawn->Location).SafeNormal() | WallNormal) > 0.5f )
		{
			Dir = WallNormal;
		}
	}

	Pawn->Velocity = Pawn->GroundSpeed * Dir;
	Pawn->Acceleration = Pawn->AccelRate * WallNormal;
	Pawn->Velocity.Z = Pawn->JumpZ;
	Pawn->setPhysics(PHYS_Falling);
}

void AController::NotifyJumpApex()
{
	eventNotifyJumpApex();
}

/* AdjustFromWall()
Gives controller a chance to adjust around an obstacle and keep moving
*/

void AController::AdjustFromWall(FVector HitNormal, AActor* HitActor)
{
}

void AAIController::AdjustFromWall(FVector HitNormal, AActor* HitActor)
{
	if ( bAdjustFromWalls
		&& ((GetStateFrame()->LatentAction == AI_PollMoveTo)
			|| (GetStateFrame()->LatentAction == AI_PollMoveToward)) )
	{
		if (Pawn != NULL && MoveTarget != NULL)
		{
            AInterpActor* HitMover = Cast<AInterpActor>(HitActor);
			if (HitMover != NULL && MoveTarget->HasAssociatedLevelGeometry(HitMover))
			{
				ANavigationPoint *Nav = Cast<ANavigationPoint>(MoveTarget);
				if (Nav != NULL && Nav->bSpecialMove)
				{
					Nav->eventSuggestMovePreparation(Pawn);
				}
				return;
			}
		}

 		if( NavigationHandle != NULL && NavigationHandle->HandleWallAdjust(HitNormal, HitActor) )
 		{
 			FailMove();
 			FailedMoveTarget = MoveTarget;
 			return;
 		}

		if ( bAdjusting )
		{
			FailMove();
			FailedMoveTarget = MoveTarget;
		}
		else
		{
			Pawn->SerpentineDir *= -1.f;
			if ( !Pawn->PickWallAdjust(HitNormal, HitActor) )
			{
				FailMove();
				FailedMoveTarget = MoveTarget;
			}
		}
	}
}

void APlayerController::PostScriptDestroyed()
{
	PlayerInput = NULL;
	CheatManager = NULL;
	NavigationHandle = NULL;
	Super::PostScriptDestroyed();
}

void AController::PostBeginPlay()
{
	Super::PostBeginPlay();
	if ( !bDeleteMe )
	{
		GWorld->AddController( this );
	}
}

void AController::PostScriptDestroyed()
{
	Super::PostScriptDestroyed();
	GWorld->RemoveController( this );
}

void AController::UpdatePawnRotation()
{
	if ( Focus )
	{
		ANavigationPoint *NavFocus = Cast<ANavigationPoint>(Focus);
		if ( NavFocus && CurrentPath && CurrentPath->Start && (MoveTarget == NavFocus) && !Pawn->Velocity.IsZero() )
		{
			// gliding pawns must focus on where they are going
			if ( Pawn->IsGlider() )
			{
				SetFocalPoint( bAdjusting ? GetAdjustLocation() : Focus->Location, (bAdjusting || Focus->Base == Pawn->Base) );
			}
			else
			{
				SetFocalPoint( Focus->Location - CurrentPath->Start->Location + Pawn->Location, (Focus->Base == Pawn->Base) );
			}
		}
		else
		{
			SetFocalPoint( Focus->Location, (Focus->Base == Pawn->Base) );
		}
	}

	// rotate pawn toward focus
	FVector FocalPoint = GetFocalPoint();
	if( !FocalPoint.IsZero() )
	{
		Pawn->rotateToward(FocalPoint);

		// face same direction as pawn
		Rotation = Pawn->Rotation;
	}
}

/** SetRotationRate()
returns how fast pawn should rotate toward desired rotation
*/
FRotator AController::SetRotationRate(FLOAT deltaTime)
{
	if (Pawn == NULL || Pawn->IsHumanControlled())
	{
		return FRotator(0,0,0);
	}
	else
	{
		return FRotator(appRound(Pawn->RotationRate.Pitch * deltaTime), appRound(Pawn->RotationRate.Yaw * deltaTime), appRound(Pawn->RotationRate.Roll * deltaTime));
	}
}

/** 
 * Creates a soundcue and PCM data for a the text in SpokenText
 */
USoundCue* APlayerController::CreateTTSSoundCue( const FString& SpokenText, class APlayerReplicationInfo* PRI )
{
	//@see USeqAct_Speak::InitSpeakSoundCue
	USoundCue* SoundCueTTS = NULL;
#if WITH_TTS
	if( GEngine && GEngine->Client )
	{
		UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();
		if( AudioDevice && AudioDevice->TextToSpeech )
		{
			// create the soundcue
			ETTSSpeaker Speaker = PRI ? ( ETTSSpeaker )PRI->TTSSpeaker : TTSSPEAKER_Paul;

			SoundCueTTS = AudioDevice->CreateTTSSoundCue( SpokenText, Speaker );
		}
	}
#endif
	// returning NULL will orphan the constructed soundcue data, to be cleaned up by GC
	return SoundCueTTS;
}

void APlayerReplicationInfo::UpdatePing(FLOAT TimeStamp)
{
	// calculate round trip time
	FLOAT NewPing = ::Min(1.5f, WorldInfo->TimeSeconds - TimeStamp);

	if ( ExactPing < 0.004f )
	{
		// initialize ping
		ExactPing = ::Min(0.3f,NewPing);
	}
	else
	{
		// reduce impact of sudden transient ping spikes
		if ( NewPing > 2.f * ExactPing )
			NewPing = ::Min(NewPing, 3.f*ExactPing);

		// calculate approx. moving average
		ExactPing = ::Min(0.99f, 0.99f * ExactPing + 0.01f * NewPing); 
	}

	// since Ping is a byte, scale the value stored to maximize resolution (since ping is clamped to 1 sec)
	Ping = ::Min(appFloor(250.f * ExactPing), 255);
}

/** DesiredDirection()
returns the direction in which the controller wants its pawn to move.
*/
FVector AController::DesiredDirection()
{
	return Pawn->Velocity;
}

/** DesiredDirection()
returns the direction in which the controller wants its pawn to move.
AAIController implementation assumes destination is set properly.
*/
FVector AAIController::DesiredDirection()
{
	return GetDestinationPosition() - Pawn->Location;
}

/** Called when the AIController is destroyed via script */
void AAIController::PostScriptDestroyed()
{
	Super::PostScriptDestroyed();
	AI_PROFILER_CONTROLLER_DESTROYED( this, NULL, NAME_None );
}

/**
 * Native function to determine if voice data should be received from this player.
 * Only called on the server to determine whether voice packet replication
 * should happen for the given sender.
 *
 * NOTE: This function is final because it can be called n^2 number of times
 * in a given frame, where n is the number of players. Change/overload this
 * function with caution as this can affect your network performance.
 *
 * @param Sender the player to check for mute status
 *
 * @return TRUE if this player is muted, FALSE otherwise
 */
UBOOL APlayerController::IsPlayerMuted(const FUniqueNetId& Sender)
{
	// Search the list for a matching Uid
	for (INT Index = 0; Index < VoicePacketFilter.Num(); Index++)
	{
		// Compare them as QWORDs for speed
		if ((QWORD&)VoicePacketFilter(Index) == (QWORD&)Sender)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/** 
 * Returns the player controller associated with this net id
 *
 * @param PlayerNetId the id to search for
 *
 * @return the player controller if found, otherwise NULL
 */
APlayerController* APlayerController::GetPlayerControllerFromNetId(FUniqueNetId PlayerNetId)
{
	// Get the world info so we can iterate the list
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	check(WorldInfo);
	// Iterate through the controller list looking for the net id
	for (AController* Controller = WorldInfo->ControllerList;
		Controller != NULL;
		Controller = Controller->NextController)
{
		// Determine if this is a player with replication
		APlayerController* PlayerController = Controller->GetAPlayerController();
		if (PlayerController != NULL && PlayerController->PlayerReplicationInfo != NULL)
		{
			// If the ids match, then this is the right player. Compare as QWORD for speed
			if ((QWORD&)PlayerController->PlayerReplicationInfo->UniqueId == (QWORD&)PlayerNetId)
			{
				return PlayerController;
			}
		}
	}
	return NULL;
}

/** called to notify the server when the client has loaded a new world via seamless travelling
 * @param WorldPackageName the name of the world package that was loaded
 */
void APlayerController::ServerNotifyLoadedWorld(FName WorldPackageName)
{
	if (GWorld->IsServer())
	{
		// update our info on what world the client is in
		UNetConnection* Connection = Cast<UNetConnection>(Player);
		if (Connection != NULL)
		{
			Connection->ClientWorldPackageName = WorldPackageName;
		}

		// if both the server and this client have completed the transition, handle it
		if (!GSeamlessTravelHandler.IsInTransition() && WorldPackageName == GWorld->GetOutermost()->GetFName() && GWorld->GetWorldInfo()->Game != NULL)
		{
			AController* TravelPlayer = this;
			GWorld->GetWorldInfo()->Game->eventHandleSeamlessTravelPlayer(TravelPlayer);
		}
	}
}

/** returns whether the client has completely loaded the server's current world (valid on server only) */
UBOOL APlayerController::HasClientLoadedCurrentWorld()
{
	UNetConnection* Connection = Cast<UNetConnection>(Player);
	if (Connection == NULL && UNetConnection::GNetConnectionBeingCleanedUp != NULL && UNetConnection::GNetConnectionBeingCleanedUp->Actor == this)
	{
		Connection = UNetConnection::GNetConnectionBeingCleanedUp;
	}
	if (Connection != NULL)
	{
		if (Connection->GetUChildConnection())
		{
			Connection = ((UChildConnection*)Connection)->Parent;
		}
		return (Connection->ClientWorldPackageName == GWorld->GetOutermost()->GetFName());
	}
	else
	{
		// if we have no client connection, we're local, so we always have the current world
		return TRUE;
	}
}

void APlayerController::ForceSingleNetUpdateFor(AActor* Target)
{
	if (Target == NULL)
	{
		warnf(TEXT("PlayerController::ForceSingleNetUpdateFor(): No Target specified"));
	}
	else if (WorldInfo->NetMode == NM_Client)
	{
		warnf(TEXT("PlayerController::ForceSingleNetUpdateFor(): Only valid on server"));
	}
	else
	{
		UNetConnection* Conn = Cast<UNetConnection>(Player);
		if (Conn != NULL)
		{
			if (Conn->GetUChildConnection() != NULL)
			{
				Conn = ((UChildConnection*)Conn)->Parent;
				checkSlow(Conn != NULL);
			}
			UActorChannel* Channel = Conn->ActorChannels.FindRef(Target);
			if (Channel != NULL)
			{
				Target->bPendingNetUpdate = TRUE; // will cause some other clients to do lesser checks too, but that's unavoidable with the current functionality
				Channel->ActorDirty = TRUE;
			}
		}
	}
}

/** 
 * worker function for APlayerController::SmoothTargetViewRotation()
 */
INT BlendRot(FLOAT DeltaTime, INT BlendC, INT NewC)
{
	if ( Abs(BlendC - NewC) > 32767 )
	{
		if ( BlendC > NewC )
			NewC += 65536;
		else
			BlendC += 65536;
	}
	if ( Abs(BlendC - NewC) > 4096 )
		BlendC = NewC;
	else
		BlendC = appTrunc(BlendC + (NewC - BlendC) * ::Min(1.f,24.f * DeltaTime));

	return (BlendC & 65535);
}

/**
 * Called client-side to smoothly interpolate received TargetViewRotation (result is in BlendedTargetViewRotation)
 * @parameter TargetPawn   is the pawn which is the current ViewTarget
 * @parameter DeltaSeconds is the time interval since the last smoothing update
 */
void APlayerController::SmoothTargetViewRotation(APawn* TargetPawn, FLOAT DeltaSeconds)
{
	if ( TargetPawn->bSimulateGravity )
		TargetViewRotation.Roll = 0;
	BlendedTargetViewRotation.Pitch = BlendRot(DeltaSeconds, BlendedTargetViewRotation.Pitch, TargetViewRotation.Pitch & 65535);
	BlendedTargetViewRotation.Yaw = BlendRot(DeltaSeconds, BlendedTargetViewRotation.Yaw, TargetViewRotation.Yaw & 65535);
	BlendedTargetViewRotation.Roll = BlendRot(DeltaSeconds, BlendedTargetViewRotation.Roll, TargetViewRotation.Roll & 65535);
}

/** 
 * This will turn the subtitles on or off depending on the value of bValue 
 *
 * @param bValue  to show or not to show
 **/
void APlayerController::SetShowSubtitles( UBOOL bValue )
{
	ULocalPlayer* LP = NULL;

	// Only let player 0 control the subtitle setting.
	LP = Cast<ULocalPlayer>(Player);

	if(LP && UUIInteraction::GetPlayerIndex(LP)==0)
	{
		// if we are not forcing subtitles to be off then use the value passed in
		if( GEngine->bSubtitlesForcedOff == FALSE )
		{
			GEngine->bSubtitlesEnabled = bValue;
		}
		// if we are forcing them off then disable subtitles in case they were some how magically turned on
		else
		{
			GEngine->bSubtitlesEnabled = FALSE;
		}


		//debugf(TEXT("Changing subtitle setting, new value: %i"), GEngine->bSubtitlesEnabled);
	}
}

/** 
* This will turn return whether the subtitles are on or off
*
**/
UBOOL APlayerController::IsShowingSubtitles()
{
	return GEngine->bSubtitlesEnabled;
}

/**
 * Determine if this player has a peer connection for the given net id
 *
 * @param PeerNetId net id of remote client peer
 * @return TRUE if the player has the peer connection
 */
UBOOL APlayerController::HasPeerConnection(const struct FUniqueNetId& PeerNetId) const
{
	if (PeerNetId.HasValue())
	{
		for (INT PeerIdx=0; PeerIdx < ConnectedPeers.Num(); PeerIdx++)
		{
			const FConnectedPeerInfo& PeerInfo = ConnectedPeers(PeerIdx);
			if (PeerInfo.PlayerID == PeerNetId)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/**
 * Delay and then travel as the new host to the given URL
 *
 * @param TravelCountdownTimer Seconds to delay before initiating the travel
 * @param URL browse path for the map/game to load as host
 */
void APlayerController::PeerTravelAsHost(FLOAT TravelCountdownTimer,const FString& URL)
{
	debugf(NAME_DevNet,TEXT("PeerTravelAsHost: Initiating travel as new host. TravelCountdownTimer=%.2f URL=%s"),TravelCountdownTimer,*URL);

	if (WorldInfo != NULL)
	{
		WorldInfo->UpdateHostMigrationState(HostMigration_HostReadyToTravel);
		WorldInfo->PeerHostMigration.HostMigrationTravelURL = URL;
		WorldInfo->PeerHostMigration.HostMigrationTravelCountdown = Clamp<FLOAT>(TravelCountdownTimer,0.0f,WorldInfo->HostMigrationTimeout*0.5f);
	}
}

/**
 * Notify client peer to travel to the new host. RPC sent through peer net driver.
 *
 * @param ToPeerNetId peer player to find connection for
 */
void APlayerController::TellPeerToTravel(struct FUniqueNetId ToPeerNetId)
{
	if (GWorld != NULL && 
		GWorld->PeerNetDriver != NULL)
	{
		if (ToPeerNetId.HasValue())
		{
			// find the connection for the peer
			for (INT PeerIdx=0; PeerIdx < GWorld->PeerNetDriver->ClientConnections.Num(); PeerIdx++)
			{
				UNetConnection* Connection = GWorld->PeerNetDriver->ClientConnections(PeerIdx);
				if (Connection != NULL &&
					Connection->PlayerId == ToPeerNetId)
				{
					debugf(NAME_DevNet,TEXT("(APlayerController.TellPeerToTravel): Sending NMT_PeerNewHostTravel. Notifying peer=0x%016I64X to travel to new host."),
						ToPeerNetId.Uid);

					FClientPeerTravelInfo ClientPeerTravelInfo;
					FNetControlMessage<NMT_PeerNewHostTravel>::Send(Connection,ClientPeerTravelInfo);
					Connection->FlushNet(TRUE);
				}
			}
		}
		else
		{
			debugf(NAME_DevNet,TEXT("(APlayerController.TellPeerToTravel): invalid peer netid."));
		}
	}
	else
	{
		debugf(NAME_DevNet,TEXT("(APlayerController.TellPeerToTravel): no peer net driver found."));
	}
}

/**
 * Notify client peer to travel to the new host via its migrated session. RPC is sent through peer net driver.
 *
 * @param ToPeerNetId peer player to find connection for
 * @param SessionName Name of session that was migrated to travel to
 * @param SearchClass Search class being used by the current game session
 * @param PlatformSpecificInfo Byte array with secure session info
 * @param PlatformSpecificInfoSize Size in bytes of PlatformSpecificInfo
 */
void APlayerController::TellPeerToTravelToSession(struct FUniqueNetId ToPeerNetId,FName SessionName,class UClass* SearchClass,BYTE* PlatformSpecificInfo,INT PlatformSpecificInfoSize)
{
	if (GWorld != NULL && 
		GWorld->PeerNetDriver != NULL)
	{
		if (ToPeerNetId.HasValue())
		{
			// find the connection for the peer and send the travel message
			for (INT PeerIdx=0; PeerIdx < GWorld->PeerNetDriver->ClientConnections.Num(); PeerIdx++)
			{
				UNetConnection* Connection = GWorld->PeerNetDriver->ClientConnections(PeerIdx);

				if (Connection != NULL &&
					Connection->PlayerId == ToPeerNetId)
				{
					debugf(NAME_DevNet,TEXT("(APlayerController.TellPeerToTravelToSession): Sending NMT_PeerNewHostTravelSession. Notifying peer=0x%016I64X to travel to new host with migrated session."),
						ToPeerNetId.Uid);

					FClientPeerTravelSessionInfo ClientPeerTravelSessionInfo;
					// copy platform info to byte array
					ClientPeerTravelSessionInfo.PlatformSpecificInfo.Empty(PlatformSpecificInfoSize);
					ClientPeerTravelSessionInfo.PlatformSpecificInfo.AddZeroed(PlatformSpecificInfoSize);
					appMemcpy(ClientPeerTravelSessionInfo.PlatformSpecificInfo.GetData(),PlatformSpecificInfo,PlatformSpecificInfoSize);
					// copy session name and class string
					ClientPeerTravelSessionInfo.SessionName = SessionName.ToString();					
					ClientPeerTravelSessionInfo.SearchClassPath = SearchClass->GetPathName();

					FNetControlMessage<NMT_PeerNewHostTravelSession>::Send(Connection,ClientPeerTravelSessionInfo);
					Connection->FlushNet(TRUE);
				}
			}
		}
		else
		{
			debugf(NAME_DevNet,TEXT("(APlayerController.TellPeerToTravelToSession): invalid peer netid."));
		}
	}
	else
	{
		debugf(NAME_DevNet,TEXT("(APlayerController.TellPeerToTravelToSession): no peer net driver found."));
	}
}

/** allows the game code an opportunity to modify post processing settings
 * @param PPSettings - the post processing settings to apply
 */
void APlayerController::ModifyPostProcessSettings(FPostProcessSettings& PPSettings) const
{
	if (PlayerCamera != NULL)
	{
		PlayerCamera->ModifyPostProcessSettings(PPSettings);
	}
}


/**
 * @return whether or not this Controller has Tilt Turned on
 **/
UBOOL APlayerController::IsControllerTiltActive() const
{
	UBOOL Retval = FALSE;
	ULocalPlayer* LP = Cast<ULocalPlayer>(Player);

	if( LP && LP->ViewportClient && LP->ViewportClient->Viewport )
	{
		Retval = LP->ViewportClient->Viewport->IsControllerTiltActive( LP->ControllerId );
	}

	return Retval;
}

/**
 * sets whether or not the Tilt functionality is turned on
 **/
void APlayerController::SetControllerTiltActive( UBOOL bActive )
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(Player);

	if( LP && LP->ViewportClient && LP->ViewportClient->Viewport )
	{
		LP->ViewportClient->Viewport->SetControllerTiltActive( LP->ControllerId, bActive );
	}
}


/**
 * sets whether or not to ONLY use the tilt input controls
 **/
void APlayerController::SetOnlyUseControllerTiltInput( UBOOL bActive )
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(Player);

	if( LP && LP->ViewportClient && LP->ViewportClient->Viewport )
	{
		LP->ViewportClient->Viewport->SetOnlyUseControllerTiltInput( LP->ControllerId, bActive );
	}
}


/**
 * sets whether or not to use the tilt forward and back input controls
 **/
void APlayerController::SetUseTiltForwardAndBack( UBOOL bActive )
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(Player);

	if( LP && LP->ViewportClient && LP->ViewportClient->Viewport )
	{
		LP->ViewportClient->Viewport->SetUseTiltForwardAndBack( LP->ControllerId, bActive );
	}
}

/**
 * @return whether or not this Controller has a keyboard available to be used
 **/
UBOOL APlayerController::IsKeyboardAvailable() const
{
	return TRUE;
}

/**
 * @return whether or not this Controller has a mouse available to be used
 **/
UBOOL APlayerController::IsMouseAvailable() const
{
	return TRUE;
}

/* epic ===============================================
* ::GetTeamNum
*
 * Queries the PRI and returns our current team index.
* =====================================================
*/
BYTE AController::GetTeamNum()
{
	if (PlayerReplicationInfo != NULL)
	{
		return (PlayerReplicationInfo->Team != NULL) ? PlayerReplicationInfo->Team->TeamIndex : 255;
	}
	else
	{
		return eventScriptGetTeamNum();
	}
}

BYTE ATeamInfo::GetTeamNum()
{
	return TeamIndex;
}

BYTE APlayerReplicationInfo::GetTeamNum()
{
	return Team ? Team->TeamIndex : 255;
}

/**
* Helper to return the default object of the GameInfo class corresponding to this GRI
*/
AGameInfo *AGameReplicationInfo::GetDefaultGameInfo()
{
	if ( GameClass )
	{
		AGameInfo *DefaultGameActor = GameClass->GetDefaultObject<AGameInfo>();
		return DefaultGameActor;
	}
	return NULL;
}

/**
 * Checks to see if two actors are on the same team.
 *
 * @return	true if they are, false if they aren't
 */
UBOOL AGameReplicationInfo::OnSameTeam(AActor *A, AActor *B)
{
	if ( !A || !B )
	{
		return FALSE;
	}

	if ( GameClass )
	{
		AGameInfo *DefaultGameActor = GameClass->GetDefaultObject<AGameInfo>();
		if ( DefaultGameActor && !DefaultGameActor->bTeamGame )
		{
			return FALSE;
		}
	}

	BYTE ATeamIndex = A->GetTeamNum();
	if ( ATeamIndex == 255 )
	{
		return FALSE;
	}

	BYTE BTeamIndex = B->GetTeamNum();
	if ( BTeamIndex == 255 )
	{
		return FALSE;
	}

	return ( ATeamIndex == BTeamIndex );
}

/**
  * PlayerControllers ignore hear noise by default
  */
void APlayerController::HearNoise(AActor* NoiseMaker, FLOAT Loudness, FName NoiseType) {}

/**
  */
void AController::HearNoise(AActor* NoiseMaker, FLOAT Loudness, FName NoiseType)
{
	if(	IsProbing(NAME_HearNoise) 
		&& CanHear(NoiseMaker->Location, Loudness, NoiseMaker) )
	{
		eventHearNoise(Loudness, NoiseMaker, NoiseType);
	}
}

/** IMPLEMENT Interface_NavigationHandle */
FVector AController::GetEdgeZAdjust(struct FNavMeshEdgeBase* Edge)
{
	if(Pawn != NULL)
	{
		return FVector(0.f,0.f,Pawn->GetCylinderExtent().Z);
	}

	return FVector(0.f);
}

FLOAT AController::GetMaxDropHeight()
{
	if(Pawn != NULL)
	{
		return Pawn->LedgeCheckThreshold;
	}
	
	return 0.f;
}

void AController::SetupPathfindingParams( FNavMeshPathParams& out_ParamCache )
{
	VERIFY_NAVMESH_PARAMS(9);
	if(Pawn != NULL)
	{
		out_ParamCache.bAbleToSearch = TRUE;
		out_ParamCache.SearchExtent = Pawn->GetCylinderExtent()+NavMeshPath_SearchExtent_Modifier;
		out_ParamCache.SearchLaneMultiplier = 0.f;
		out_ParamCache.SearchStart = Pawn->Location;
		out_ParamCache.bCanMantle = Pawn->bCanMantle;
		out_ParamCache.bNeedsMantleValidityTest = FALSE;
		out_ParamCache.MaxDropHeight = GetMaxDropHeight();
		out_ParamCache.MinWalkableZ = Pawn->WalkableFloorZ;
		if( Pawn->WalkableFloorZ >= 0.6f || Pawn->bCanFly )
		{
			out_ParamCache.MaxHoverDistance = -1.f;
		}
		else
		{
			out_ParamCache.MaxHoverDistance = DEFAULT_BOX_PADDING;
		}
	}
	else
	{
		out_ParamCache.bAbleToSearch = FALSE;
		out_ParamCache.SearchExtent = FVector(0.f);
		out_ParamCache.SearchLaneMultiplier = 0.f;
		out_ParamCache.SearchStart = Location;
		out_ParamCache.bCanMantle = FALSE;
		out_ParamCache.bNeedsMantleValidityTest = FALSE;
		out_ParamCache.MaxDropHeight = 0.f;
		out_ParamCache.MinWalkableZ=0.7f;
		out_ParamCache.MaxHoverDistance = 0.f;
	}

	if( bOverrideSearchStart )
	{
		out_ParamCache.SearchStart = OverrideSearchStart;
	}
}

UBOOL AController::CanCoverSlip(ACoverLink* Link, INT SlotIdx)
{
	return Pawn && Pawn->bCanCoverSlip;
}



// AnimControl Matinee Track support

/** Used to provide information on the slots that this Actor provides for animation to Matinee. */
void AController::GetAnimControlSlotDesc(TArray<struct FAnimSlotDesc>& OutSlotDescs)
{
	if (Pawn!=NULL)
	{
		Pawn->GetAnimControlSlotDesc(OutSlotDescs);
	}
}

/**
*	Called by Matinee when we open it to start controlling animation on this Actor.
*	Is also called again when the GroupAnimSets array changes in Matinee, so must support multiple calls.
*/
void AController::PreviewBeginAnimControl(UInterpGroup* InInterpGroup)
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewBeginAnimControl(InInterpGroup);
	}
}

/** Called each frame by Matinee to update the desired sequence by name and position within it. */
void AController::PreviewSetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bLooping, UBOOL bFireNotifies, UBOOL bEnableRootMotion, FLOAT DeltaTime)
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewSetAnimPosition(SlotName, ChannelIndex, InAnimSeqName, InPosition, bLooping, bFireNotifies, bEnableRootMotion, DeltaTime);
	}
}

/** Called each frame by Matinee to update the desired animation channel weights for this Actor. */
void AController::PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos)
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewSetAnimWeights(SlotInfos);
	}
}

/** Called by Matinee when we close it after we have been controlling animation on this Actor. */
void AController::PreviewFinishAnimControl(UInterpGroup* InInterpGroup)
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewFinishAnimControl(InInterpGroup);
	}
}

/** Function used to control FaceFX animation in the editor (Matinee). */
void AController::PreviewUpdateFaceFX(UBOOL bForceAnim, const FString& GroupName, const FString& SeqName, FLOAT InPosition)
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewUpdateFaceFX(bForceAnim, GroupName, SeqName, InPosition);
	}
}

/** Used by Matinee playback to start a FaceFX animation playing. */
void AController::PreviewActorPlayFaceFX(const FString& GroupName, const FString& SeqName, USoundCue* InSoundCue)
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewActorPlayFaceFX(GroupName, SeqName, InSoundCue);
	}
}

/** Used by Matinee to stop current FaceFX animation playing. */
void AController::PreviewActorStopFaceFX()
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewActorStopFaceFX();
	}
}

/** Used in Matinee to get the AudioComponent we should play facial animation audio on. */
UAudioComponent* AController::PreviewGetFaceFXAudioComponent()
{
	if (Pawn!=NULL)
	{
		return Pawn->PreviewGetFaceFXAudioComponent();
	}

	return NULL;
}

/** Get the UFaceFXAsset that is currently being used by this Actor when playing facial animations. */
class UFaceFXAsset* AController::PreviewGetActorFaceFXAsset()
{
	if (Pawn!=NULL)
	{
		return Pawn->PreviewGetActorFaceFXAsset();
	}

	return NULL;
}

/** Called each frame by Matinee to update the weight of a particular MorphNodeWeight. */
void AController::PreviewSetMorphWeight(FName MorphNodeName, FLOAT MorphWeight)
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewSetMorphWeight(MorphNodeName, MorphWeight);
	}
}

/** Called each frame by Matinee to update the controlstrength on a SkelControl. */
void AController::SetSkelControlStrength(FName SkelControlName, FLOAT ControlStrength)
{
	if (Pawn!=NULL)
	{
		Pawn->SetSkelControlStrength(SkelControlName, ControlStrength);
	}
}


/** Called each frame by Matinee to update the scaling on a SkelControl. */
void AController::PreviewSetSkelControlScale(FName SkelControlName, FLOAT Scale)
{
	if (Pawn!=NULL)
	{
		Pawn->PreviewSetSkelControlScale(SkelControlName, Scale);
	}
}
/** Called each from while the Matinee action is running, to set the animation weights for the actor. */
void AController::SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos )
{
	if (Pawn!=NULL)
	{
		Pawn->SetAnimWeights(SlotInfos);
	}
}


void UCheatManager::LogPlaySoundCalls( UBOOL bShoudLog )
{
	//warnf( TEXT( "LogPlaySoundCalls called with %d"), bShoudLog );
	GShouldLogAllPlaySoundCalls = bShoudLog;
}


void UCheatManager::LogParticleActivateSystemCalls( UBOOL bShoudLog )
{
	//warnf( TEXT( "LogGShouldLogAllParticleActivateSystemCalls called with %d"), bShoudLog );
	GShouldLogAllParticleActivateSystemCalls = bShoudLog;
}

void UCheatManager::VerifyNavMeshObjects()
{
	FNavMeshWorld::VerifyPathObjects();
	FNavMeshWorld::VerifyPathObstacles();
}

void UCheatManager::PrintNavMeshObstacles()
{
	FNavMeshWorld::PrintObstacleInfo();
}

void UCheatManager::PrintAllPathObjectEdges()
{
	FNavMeshWorld::PrintAllPathObjectEdges();
}

void UCheatManager::VerifyNavMeshCoverRefs()
{
	FNavMeshWorld::VerifyCoverReferences();
}

void UCheatManager::DrawUnsupportingEdges(const FString& PawnClassName)
{
	UClass* PawnClass = Cast<UClass>(StaticLoadObject( UClass::StaticClass(), NULL, *PawnClassName, NULL, LOAD_NoWarn , NULL ));

	if ( PawnClass != NULL )
	{
		APawn* DefaultPawn = PawnClass->GetDefaultObject<APawn>();
		if( DefaultPawn != NULL )
		{

			// fill out path params
			FNavMeshPathParams Parms;
			Parms.bAbleToSearch = TRUE;
			Parms.SearchExtent = DefaultPawn->GetCylinderExtent();
			Parms.Interface = NULL;
			Parms.bCanMantle = DefaultPawn->bCanMantle;
			Parms.bNeedsMantleValidityTest = FALSE;
			Parms.MinWalkableZ = DefaultPawn->WalkableFloorZ;
			Parms.MaxHoverDistance = (DefaultPawn->bCanFly) ? -1.f : DEFAULT_BOX_PADDING;

			FNavMeshWorld::DrawNonSupportingEdges(Parms);
		}
	}
	
}

void UCheatManager::DumpCoverStats()
{
	GWorld->DumpCoverStats();
}

void UCheatManager::GetAnalyticsUserId()
{
	debugf(TEXT("Analytics UserId is:%s"),*UPlatformInterfaceBase::GetAnalyticEventsInterfaceSingleton()->UserId);
}



