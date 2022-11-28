/*=============================================================================
	UnLevTic.cpp: Level timer tick function
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "UnPath.h"

#include "EngineSequenceClasses.h"

/*-----------------------------------------------------------------------------
	FTickableObject implementation.
-----------------------------------------------------------------------------*/

/** Static array of tickable objects */
TArray<FTickableObject*> FTickableObject::TickableObjects;

/*-----------------------------------------------------------------------------
	Helper classes.
-----------------------------------------------------------------------------*/
	
//
// Priority sortable list.
//
struct FActorPriority
{
	INT			    Priority;	// Update priority, higher = more important.
	AActor*			Actor;		// Actor.
	UActorChannel*	Channel;	// Actor channel.
	FActorPriority()
	{}
	FActorPriority( FVector& ViewPos, FVector& ViewDir, UNetConnection* InConnection, UActorChannel* InChannel, AActor* InActor, APlayerController *Viewer, UBOOL bLowBandwidth )
	{
		Actor       = InActor;
		Channel     = InChannel;
		
		FLOAT Time  = Channel ? (InConnection->Driver->Time - Channel->LastUpdateTime) : InConnection->Driver->SpawnPrioritySeconds;
		if ( (Actor == Viewer) || (Actor == Viewer->Pawn) || (Actor->Instigator && (Actor->Instigator == InActor)) )
			Time *= 4.f; 
		else if ( !Actor->bHidden )
		{
			FVector Dir = Actor->Location - ViewPos;
			FLOAT DistSq = Dir.SizeSquared();
			if ( bLowBandwidth )
			{
				UBOOL bIsBehindView = false;
				// prioritize projectiles and pawns in actual view more 
				if ( (ViewDir | Dir) < 0.f )
				{
					bIsBehindView = true;
					if ( DistSq > 4000000.f )
						Time *= 0.2f;
					else if ( DistSq > 250000.f )
						Time *= 0.5f;
				}
				if ( Actor->GetAPawn() )
				{
					if ( !bIsBehindView )
					{
						Dir = Dir.SafeNormal();
						if ( (ViewDir | Dir) > 0.7f )
							Time *= 2.5f;
					}
					else
					{
						APawn * P = Actor->GetAPawn();
						if ( P->DrivenVehicle && (P->DrivenVehicle->Controller == Viewer) )
							Time *= 2.5f;
					}
					if ( DistSq /* * Viewer->FOVBias * Viewer->FOVBias */ > 10000000.f ) //FIXMESTEVE - consider FOV
						Time *= 0.5f;
				}
				else if ( Actor->GetAProjectile() )
				{
					if ( !bIsBehindView )
					{
						Dir = Dir.SafeNormal();
						if ( (ViewDir | Dir) > 0.7f )
							Time *= 2.5f;
					}
					if ( DistSq > 10000000.f )
						Time *= 0.2f;
				}
				else if ( DistSq > 25000000.f )
					Time *= 0.3f;
				else if ( Actor->Base && (Actor->Base == Viewer->Pawn) )
					Time *= 3.f;
			}
			else if ( (ViewDir | Dir) < 0.f )
			{
				if ( DistSq > 4000000.f )
					Time *= 0.3f;
				else if ( DistSq > 250000.f )
					Time *= 0.5f;
			}
		}
		Priority = appRound(65536.0f * Actor->NetPriority * Time);
	}
};

IMPLEMENT_COMPARE_POINTER( FActorPriority, UnLevTic, { return B->Priority - A->Priority; } )

/*-----------------------------------------------------------------------------
	Tick a single actor.
-----------------------------------------------------------------------------*/

UBOOL AActor::CheckOwnerUpdated()
{
	if( Owner && (INT)Owner->bTicked!=GetLevel()->Ticked && !Owner->bStatic && !Owner->bDeleteMe )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}
	return 1;
}

UBOOL APawn::CheckOwnerUpdated()
{
	if( Owner && (INT)Owner->bTicked!=GetLevel()->Ticked && !Owner->bStatic && !Owner->bDeleteMe )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}
	// Handle controller-first updating.
	if( Controller && !Controller->bDeleteMe && (INT)Controller->bTicked!=GetLevel()->Ticked )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}
	return 1;
}

void AActor::TickAuthoritative( FLOAT DeltaSeconds )
{
	// Tick the nonplayer.
	//clockSlow(GStats.DWORDStats(GEngineStats.STATS_Game_ScriptTickCycles));
	eventTick(DeltaSeconds);
	//unclockSlow(GStats.DWORDStats(GEngineStats.STATS_Game_ScriptTickCycles));

	// Update the actor's script state code.
	ProcessState( DeltaSeconds );

	UpdateTimers(DeltaSeconds );

	// Update LifeSpan.
	if( LifeSpan!=0.f )
	{
		LifeSpan -= DeltaSeconds;
		if( LifeSpan <= 0.0001f )
		{
			// Actor's LifeSpan expired.
			GetLevel()->DestroyActor( this );
			return;
		}
	}

	// Perform physics.
	if ( !bDeleteMe && (Physics!=PHYS_None) && (Role!=ROLE_AutonomousProxy) )
		performPhysics( DeltaSeconds );
}

void AActor::TickSimulated( FLOAT DeltaSeconds )
{
	TickAuthoritative(DeltaSeconds);
}

void APawn::TickSimulated( FLOAT DeltaSeconds )
{
	// Simulated Physics for pawns
	// simulate gravity

	if ( bHardAttach )
	{
		Acceleration = FVector(0.f,0.f,0.f);
		if ( (Physics == PHYS_RigidBody) || (Physics == PHYS_Articulated) )
			setPhysics(PHYS_None);
		else
			Physics = PHYS_None;
	}
	else if ( (Physics != PHYS_Articulated) && (Physics != PHYS_Articulated) )
	{
	Acceleration = Velocity.SafeNormal();
	if ( PhysicsVolume->bWaterVolume )
		Physics = PHYS_Swimming;
	else if ( bCanClimbLadders && PhysicsVolume->IsA(ALadderVolume::StaticClass()) )
		Physics = PHYS_Ladder;
	else if ( bSimulateGravity )
		Physics = PHYS_Walking;	// set physics mode guess for use by animation
	else
		Physics = PHYS_Flying;

	//simulated pawns just predict location, no script execution
	moveSmooth(Velocity * DeltaSeconds);

	// if simulated gravity, check if falling
	if ( bSimulateGravity && !bSimGravityDisabled )
	{
		FCheckResult Hit(1.f);
		if ( Velocity.Z == 0.f )
			GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0.f,0.f,1.5f * CylinderComponent->CollisionHeight), Location, TRACE_AllBlocking, FVector(CylinderComponent->CollisionRadius,CylinderComponent->CollisionRadius,4.f));
		else if ( Velocity.Z < 0.f )
			GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0.f,0.f,8.f), Location, TRACE_AllBlocking, GetCylinderExtent());

		if ( (Hit.Time == 1.f) || (Hit.Normal.Z < UCONST_MINFLOORZ) )
		{
			if ( Velocity.Z == 0.f )
				Velocity.Z = 0.15f * PhysicsVolume->Gravity.Z;
			Velocity += PhysicsVolume->Gravity * DeltaSeconds;
			Physics = PHYS_Falling;
		}
		else
			Velocity.Z = 0.f;
	}
	}
	// Tick the nonplayer.
	eventTick(DeltaSeconds);

}

void AActor::TickSpecial( FLOAT DeltaSeconds )
{
}

void APawn::TickSpecial( FLOAT DeltaSeconds )
{
	// assume dead if bTearOff
	if ( bTearOff && !bPlayedDeath )
	{
		eventPlayDying(HitDamageType,TakeHitLocation);
	}

	// update eyeheight if someone is viewing through this pawn's eyes
	if ( bUpdateEyeheight || IsHumanControlled() )
		eventUpdateEyeHeight(DeltaSeconds);

	if( (Role==ROLE_Authority) && (BreathTime > 0.f) )
	{
		BreathTime -= DeltaSeconds;
		if (BreathTime < 0.001f)
		{
			BreathTime = 0.0f;
			eventBreathTimer();
		}
	}
}

UBOOL AActor::PlayerControlled()
{
	return 0;
}

UBOOL APawn::PlayerControlled()
{
	return ( IsLocallyControlled() );
}

UBOOL AActor::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	// Ignore actors in stasis
	if
	(	bStasis
	&&	((Physics==PHYS_None) || (Physics == PHYS_Rotating))
	&&	(GetLevel()->TimeSeconds - LastRenderTime > 5)
	&&	(Level->NetMode == NM_Standalone) )
	{
		bTicked = GetLevel()->Ticked;
		return 1;
	}

	// Handle owner-first updating.
	if ( !CheckOwnerUpdated() )
		return 0;
	bTicked = GetLevel()->Ticked;

	// Non-player update.
	if( (TickType!=LEVELTICK_ViewportsOnly) || PlayerControlled() )
	{
		// This actor is tickable.
		if( RemoteRole == ROLE_AutonomousProxy )
		{
			APlayerController *PC = GetTopPlayerController();//Cast<APlayerController>(GetTopOwner());
			if ( (PC && PC->LocalPlayerController()) || Physics == PHYS_RigidBody )
				TickAuthoritative(DeltaSeconds);
			else
			{
				eventTick(DeltaSeconds);

				// Update the actor's script state code.
				ProcessState( DeltaSeconds );
				// Server handles timers for autonomous proxy.
				UpdateTimers( DeltaSeconds );
			}
		}
		else if ( Role>ROLE_SimulatedProxy )
			TickAuthoritative(DeltaSeconds);
		else if ( Role == ROLE_SimulatedProxy )
			TickSimulated(DeltaSeconds);
	else if ( !bDeleteMe && ((Physics == PHYS_Falling) || (Physics == PHYS_Rotating) || (Physics == PHYS_Projectile)) ) // dumbproxies simulate falling if client side physics set
			performPhysics( DeltaSeconds );

		if ( bDeleteMe )
			return 1;

		TickSpecial(DeltaSeconds);	// perform any tick functions unique to an actor subclass
	}

	// Update components. We do this after the position has been updated so stuff like animation can update using the new position.
	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Components.Num();ComponentIndex++)
		if(Components(ComponentIndex) && Components(ComponentIndex)->Initialized)
			Components(ComponentIndex)->Tick(DeltaSeconds);

	return 1;
}


/* Controller Tick
Controllers are never animated, and do not look for an owner to be ticked before them
Non-player controllers don't support being an autonomous proxy
*/
UBOOL AController::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	// Ignore controllers with no pawn, or pawn is in stasis
	if
	(	bStasis
	|| (TickType==LEVELTICK_ViewportsOnly)
	|| (Pawn && Pawn->bStasis
	&&	((Pawn->Physics==PHYS_None) || (Pawn->Physics == PHYS_Rotating))
	&&	(GetLevel()->TimeSeconds - Pawn->LastRenderTime > 5)
	&&	(Level->NetMode == NM_Standalone)) )
	{
		bTicked = GetLevel()->Ticked;
		return 1;
	}

	bTicked = GetLevel()->Ticked;

	if( Role>=ROLE_SimulatedProxy )
		TickAuthoritative(DeltaSeconds);
	
	// Update eyeheight and send visibility updates
	// with PVS, monsters look for other monsters, rather than sending msgs

	if( Role==ROLE_Authority && TickType==LEVELTICK_All )
	{
		if( SightCounter < 0.0f )
		{
			if( IsProbing(NAME_EnemyNotVisible) )
			{
				CheckEnemyVisible();
				SightCounter = 0.05f + 0.1f * appFrand();
			}
			else
				SightCounter += 0.15f + 0.1f * appFrand();
		}

		SightCounter = SightCounter - DeltaSeconds;
		// for best performance, players show themselves to players and non-players (e.g. monsters),
		// and monsters show themselves to players
		// but monsters don't show themselves to each other
		// also

		if( Pawn && !Pawn->bHidden && !Pawn->bAmbientCreature )
			ShowSelf();
	}

	if ( Pawn )
	{
		// rotate pawn toward focus
		Pawn->rotateToward(Focus, FocalPoint);

		// face same direction as pawn
		Rotation = Pawn->Rotation;
	}
	return 1;
}

/*
PlayerControllers
Controllers are never animated, and do not look for an owner to be ticked before them
*/
UBOOL APlayerController::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	bTicked = GetLevel()->Ticked;

	GetViewTarget();
	if( (RemoteRole == ROLE_AutonomousProxy) && !LocalPlayerController() )
	{
		// kick idlers
		if ( PlayerReplicationInfo )
		{
			if ( (Pawn && (!Level->bKickLiveIdlers || (Pawn->Physics != PHYS_Walking)) ) || !bShortConnectTimeOut || (PlayerReplicationInfo->bOnlySpectator && (ViewTarget != this)) || PlayerReplicationInfo->bOutOfLives 
			|| Level->Pauser || (Level->Game && (Level->Game->bWaitingToStartMatch || Level->Game->bGameEnded || (Level->Game->NumPlayers < 2))) || PlayerReplicationInfo->bAdmin )
			{
				LastActiveTime = Level->TimeSeconds;
			}
			else if ( (Level->Game->MaxIdleTime > 0) && (Level->TimeSeconds - LastActiveTime > Level->Game->MaxIdleTime - 10) )
			{
				if ( Level->TimeSeconds - LastActiveTime > Level->Game->MaxIdleTime )
				{
					Level->Game->eventKickIdler(this);
					LastActiveTime = Level->TimeSeconds - Level->Game->MaxIdleTime + 3.f;
				}
				else
					eventKickWarning();
			}
		}

		// force physics update for clients that aren't sending movement updates in a timely manner 
		// this prevents cheats associated with artificially induced ping spikes
		if ( Pawn && !Pawn->bDeleteMe 
			&& (Pawn->Physics!=PHYS_None) && (Pawn->Physics != PHYS_RigidBody) && (Pawn->Physics != PHYS_Articulated)
			&& (Level->TimeSeconds - ServerTimeStamp > ::Max(DeltaSeconds+0.06f,0.25f)) 
			&& (ServerTimeStamp != 0.f) )
		{
			// force position update
			if ( !Pawn->Velocity.IsZero() )
			{
				Pawn->performPhysics( Level->TimeSeconds - ServerTimeStamp );
			}
			ServerTimeStamp = Level->TimeSeconds;
			TimeMargin = 0.f;
			MaxTimeMargin = Level->MaxTimeMargin;
		}

		// update viewtarget replicated info
		if( ViewTarget != Pawn )
		{
            APawn* TargetPawn = ViewTarget ? ViewTarget->GetAPawn() : NULL; 
			if ( TargetPawn )
			{
				if ( TargetPawn->Controller && TargetPawn->Controller->GetAPlayerController() )
					TargetViewRotation = TargetPawn->Controller->Rotation;
				else
					TargetViewRotation = TargetPawn->Rotation;
				TargetEyeHeight = TargetPawn->BaseEyeHeight;
			}
		}

		// Update the actor's script state code.
		ProcessState( DeltaSeconds );
		// Server handles timers for autonomous proxy.
		UpdateTimers( DeltaSeconds );

		// send ClientAdjustment if necessary
		if ( PendingAdjustment.TimeStamp > 0.f )
			eventSendClientAdjustment();
	}
	else if( Role>=ROLE_SimulatedProxy )
	{
		// Process PlayerTick with input.
		if ( !PlayerInput )
			eventInitInputSystem();
	
		for(UINT InteractionIndex = 0;InteractionIndex < (UINT)Interactions.Num();InteractionIndex++)
			if(Interactions(InteractionIndex))
				Interactions(InteractionIndex)->Tick(DeltaSeconds);

		if(PlayerInput)
			eventPlayerTick(DeltaSeconds);

		for(UINT InteractionIndex = 0;InteractionIndex < (UINT)Interactions.Num();InteractionIndex++)
			if(Interactions(InteractionIndex))
				Interactions(InteractionIndex)->Tick(-1.0f);

		// Update the actor's script state code.
		ProcessState( DeltaSeconds );

		UpdateTimers( DeltaSeconds );

		if ( bDeleteMe )
			return 1;

		// Perform physics.
		if( Physics!=PHYS_None && Role!=ROLE_AutonomousProxy )
			performPhysics( DeltaSeconds );
	}

	// Update eyeheight and send visibility updates
	// with PVS, monsters look for other monsters, rather than sending msgs
	if( Role==ROLE_Authority && TickType==LEVELTICK_All )
	{
		if( SightCounter < 0.0f )
			SightCounter += 0.2f;

		SightCounter = SightCounter - DeltaSeconds;
		if( Pawn && !Pawn->bHidden )
			ShowSelf();
	}

	return 1;
}

void AActor::UpdateTimers(FLOAT DeltaSeconds)
{
	// split into two loops to avoid infinite loop where
	// the timer is called, causes settimer to be called
	// again with a rate less than our current delta
	// and causing an invalid index to be accessed
	for (INT idx = 0; idx < Timers.Num(); idx++)
	{
		// just increment the counters
		Timers(idx).Count += DeltaSeconds;
	}
	for (INT idx = 0; idx < Timers.Num(); idx++)
	{
		if (Timers(idx).Rate < Timers(idx).Count)
		{
			// calculate how many times the timer may of elapsed
			// (for large delta times on looping timers)
			INT callCount = Timers(idx).bLoop == 1 ? Timers(idx).Count/Timers(idx).Rate : 1;
			// lookup the function to call
			UFunction *Func = FindFunction(Timers(idx).FuncName);
			// if we didn't find the function, or it's not looping
			if( Func == NULL ||
				!Timers(idx).bLoop )
			{
				if( Func == NULL )
				{
					debugf(NAME_Warning,
						TEXT("Failed to find function %s for timer"),
						*Timers(idx).FuncName);
				}

				// remove defunct timer
				Timers.Remove(idx--,1);
			}
			else
			{
				// otherwise reset for loop
				Timers(idx).Count -= callCount * Timers(idx).Rate;
			}
			// now call the function
			if (Func != NULL)
			{
				// allocate null func params
				void *funcParms = appAlloca(Func->ParmsSize);
				appMemzero( funcParms, Func->ParmsSize );
				
				while (callCount > 0)
				{
					// and call the function
					ProcessEvent(Func,funcParms);
					callCount--;
				}
			}
		}
	}

}

/*-----------------------------------------------------------------------------
	Network client tick.
-----------------------------------------------------------------------------*/

void ULevel::TickNetClient( FLOAT DeltaSeconds )
{
	if( NetDriver->ServerConnection->State==USOCK_Open )
	{
		// Don't replicate any properties from client to server.
	}
	else if( NetDriver->ServerConnection->State==USOCK_Closed )
	{
		//@todo merge: connection lost message if disconnect (could be map change)
		Engine->SetClientTravel( TEXT("?closed"), 0, TRAVEL_Absolute );
	}
}

/*-----------------------------------------------------------------------------
	Network server ticking individual client.
-----------------------------------------------------------------------------*/
UBOOL AActor::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	if( bAlwaysRelevant || IsOwnedBy(Viewer) || IsOwnedBy(RealViewer) || this==Viewer || Viewer==Instigator )
		return 1;
#if ACTOR_COMPONENT_DISABLED
	else if( AmbientSound
			&& ((Location-Viewer->Location).SizeSquared() < Square(GAudioMaxRadiusMultiplier*SoundRadius)) )
		return 1;
	else if ( (Owner && (Base == Owner) && !bOnlyOwnerSee)
			|| (Base && (AttachmentBone != NAME_None) && Cast<USkeletalMesh>(Base->Mesh)) )
		return Base->IsNetRelevantFor( RealViewer, Viewer, SrcLocation );
#endif
	else if( (bHidden || bOnlyOwnerSee) && !bBlockActors )
		return 0;
	else
	{
/* FIXMESTEVE
		// check distance fog
		FLOAT DistSq = (Location - SrcLocation).SizeSquared();
		if ( Region.Zone->bDistanceFog && (DistSq > Region.Zone->DistanceFogEnd * Region.Zone->DistanceFogEnd) )
			return 0;
*/
		if ( GetLevel()->Model->FastLineCheck(Location,SrcLocation) )
		{
			/* FIXMESTEVE
			if ( RealViewer->bWasSaturated && (CullDistance > 0.f) && (DistSq > 0.8f * CullDistance) )
			{
			// check against terrain
			AZoneInfo* Z = Viewer->Region.Zone;
			if( Z && Z->bTerrainZone )
			{
				FVector End = Location;
				End.Z += 2.f * CollisionHeight;
				FCheckResult Hit(1.f);
				for( INT t=0;t<Z->Terrains.Num();t++ )
				{
						if( Z->Terrains(t)->LineCheck( Hit, End, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0 )
					{
						FPointRegion HitRegion = GetLevel()->Model->PointRegion( GetLevel()->GetLevelInfo(), Hit.Location );
						if( HitRegion.Zone == Z )
							{
							return 0;
					}
				}
			} 
				} 
			}*/
			/*
			// check against antiportal volumes
			FCheckResult Hit(1.f);
			for ( INT i=0; i<GetLevel()->AntiPortals.Num(); i++ )
				if ( GetLevel()->AntiPortals(i) && GetLevel()->AntiPortals(i)->GetPrimitive()->LineCheck( Hit, GetLevel()->AntiPortals(i), Location, SrcLocation, FVector(0.f,0.f,0.f), 0, TRACE_StopAtFirstHit )==0 )
					return 0;
			*/
			return 1;
		}
		return 0;
	}
}

UBOOL APlayerController::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	return ( this==RealViewer );
}

UBOOL AWeapon::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	if ( Super::IsNetRelevantFor(RealViewer, Viewer, SrcLocation) )
		return 1;

	// FIXME LAURENT - right now we force only carried weapon
	return ( Instigator && Instigator->bReplicateWeapon && Instigator->IsNetRelevantFor(RealViewer, Viewer, SrcLocation) && Instigator->Weapon == this );
}

UBOOL APawn::CacheNetRelevancy(UBOOL bIsRelevant, APlayerController* RealViewer, AActor* Viewer)
{
	bCachedRelevant = bIsRelevant;
	NetRelevancyTime = Level->TimeSeconds;
	LastRealViewer = RealViewer;
	LastViewer = Viewer;
	return bIsRelevant;
}

UBOOL APawn::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	if ( (NetRelevancyTime == Level->TimeSeconds) && (RealViewer == LastRealViewer) && (Viewer == LastViewer) )
		return bCachedRelevant;
	if( bAlwaysRelevant || IsOwnedBy(Viewer) || IsOwnedBy(RealViewer) || this==Viewer || Viewer==Instigator )
		return CacheNetRelevancy(true,RealViewer,Viewer);
#if ACTOR_COMPONENT_DISABLED
	if( AmbientSound
			&& ((Location-Viewer->Location).SizeSquared() < Square(GAudioMaxRadiusMultiplier*SoundRadius)) )
		return CacheNetRelevancy(true,RealViewer,Viewer);
#endif
	else if( (bHidden || bOnlyOwnerSee) && !bBlockActors ) 
		return CacheNetRelevancy(false,RealViewer,Viewer);
	else
	{
		// check against BSP - check head and center
		//debugf(TEXT("Check relevance of %s"),*(PlayerReplicationInfo->PlayerName));
		if ( !GetLevel()->Model->FastLineCheck(Location + FVector(0.f,0.f,BaseEyeHeight),SrcLocation) 
			 && !GetLevel()->Model->FastLineCheck(Location,SrcLocation)  )
			return CacheNetRelevancy(false,RealViewer,Viewer);
		/*
		// if large collision volume, check edges
		if ( GetLevel()->Model->FastLineCheck(Location + FVector(0.f,CollisionRadius,0.f),SrcLocation) )
			return CacheNetRelevancy(true,RealViewer,Viewer);
		if ( GetLevel()->Model->FastLineCheck(Location - FVector(0.f,CollisionRadius,0.f),SrcLocation) )
			return CacheNetRelevancy(true,RealViewer,Viewer);
		if ( GetLevel()->Model->FastLineCheck(Location + FVector(CollisionRadius,0.f,0.f),SrcLocation) )
			return CacheNetRelevancy(true,RealViewer,Viewer);
		if ( GetLevel()->Model->FastLineCheck(Location - FVector(CollisionRadius,0.f,0.f),SrcLocation) )
			return CacheNetRelevancy(true,RealViewer,Viewer);
		return 0;
		*/
		/* FIXMESTEVE
		if ( RealViewer->bWasSaturated )
		{
		// check against terrain
		// FIXME - Jack, it would be nice to make this faster (a linecheck that just returns whether there was a hit)
		AZoneInfo* Z = Viewer->Region.Zone;
		if( Z && Z->bTerrainZone )
		{
			FVector End = Location;
				End.Z += 1.5f * CollisionHeight;
			FCheckResult Hit(1.f);
			for( INT t=0;t<Z->Terrains.Num();t++ )
			{
					if( (Z->Terrains(t)->LineCheck( Hit, End, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0)
						&& (Z->Terrains(t)->LineCheck( Hit, Location, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0) )
				{
					FPointRegion HitRegion = GetLevel()->Model->PointRegion( GetLevel()->GetLevelInfo(), Hit.Location );
					if( HitRegion.Zone == Z )
						{
					return CacheNetRelevancy(false,RealViewer,Viewer);
				}
			}
		}
		}
		*/
		/*
		// check against antiportal volumes
		FCheckResult Hit(1.f);
		for ( INT i=0; i<GetLevel()->AntiPortals.Num(); i++ )
			if ( GetLevel()->AntiPortals(i)
				&& GetLevel()->AntiPortals(i)->GetPrimitive()->LineCheck( Hit, GetLevel()->AntiPortals(i), Location + FVector(0.f,0.f,CollisionHeight), SrcLocation, FVector(0.f,0.f,0.f), 0, TRACE_StopAtFirstHit )==0 )
				return CacheNetRelevancy(false,RealViewer,Viewer);
		*/
		return CacheNetRelevancy(true,RealViewer,Viewer);
	}
}

UBOOL AVehicle::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	if ( bAlwaysRelevant )
		return true;
	if ( (NetRelevancyTime == Level->TimeSeconds) && (RealViewer == LastRealViewer) && (Viewer == LastViewer) )
		return bCachedRelevant;
	if( IsOwnedBy(Viewer) || IsOwnedBy(RealViewer) || this==Viewer || Viewer==Instigator
		|| IsBasedOn(Viewer) || (Viewer && Viewer->IsBasedOn(this)) )
		return CacheNetRelevancy(true,RealViewer,Viewer);
//FIXMESTEVE	if( AmbientSound 
//			&& ((Location-Viewer->Location).SizeSquared() < Square(GAudioMaxRadiusMultiplier*SoundRadius)) )
//		return CacheNetRelevancy(true,RealViewer,Viewer);
//	else if ( (Owner && (Base == Owner) && !bOnlyOwnerSee)
//			|| (Base && (AttachmentBone != NAME_None) && Cast<USkeletalMesh>(Base->Mesh)) )
//		return Base->IsNetRelevantFor( RealViewer, Viewer, SrcLocation );
//	else if( (bHidden || bOnlyOwnerSee) && !bBlockActors && !AmbientSound )
//		return CacheNetRelevancy(false,RealViewer,Viewer);
	else
	{
		FLOAT DistSq = (Location - SrcLocation).SizeSquared();
/* FIXMESTEVE
		// check distance fog
		if ( Region.Zone->bDistanceFog && (DistSq > (500.f + CollisionRadius + Region.Zone->DistanceFogEnd) * (500.f + CollisionRadius + Region.Zone->DistanceFogEnd)) )
			return CacheNetRelevancy(false,RealViewer,Viewer);

		if ( RealViewer->bWasSaturated && (CollisionRadius < 200.f) )
		{
			// check against terrain
			AZoneInfo* Z = Viewer->Region.Zone;
			if( Z && Z->bTerrainZone ) 
{
				FVector End = Location;
				End.Z += 1.5f * CollisionHeight;
				FCheckResult Hit(1.f);
				for( INT t=0;t<Z->Terrains.Num();t++ )
				{
					if( (Z->Terrains(t)->LineCheck( Hit, End, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0)
						&& (Z->Terrains(t)->LineCheck( Hit, Location, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0) )
					{
						// check extreme edges
						FVector Right = (End - SrcLocation) ^ FVector(0.f,0.f,1.f);
						Right = Right.SafeNormal() * 1.25f * CollisionRadius;
						if ( (Z->Terrains(t)->LineCheck( Hit, End + Right, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0)
							&& (Z->Terrains(t)->LineCheck( Hit, End - Right, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0) )
						{
							FPointRegion HitRegion = GetLevel()->Model->PointRegion( GetLevel()->GetLevelInfo(), Hit.Location );
							if( HitRegion.Zone == Z )
							{
								return CacheNetRelevancy(false,RealViewer,Viewer);
							}
						}
					}
				} 
			}
		}
*/
		return CacheNetRelevancy(true,RealViewer,Viewer);
	}
}

INT ULevel::ServerTickClients( FLOAT DeltaSeconds )
{
	if ( NetDriver->ClientConnections.Num() == 0 )
		return 0;
	INT Updated=0;

	FMemMark Mark(GMem);
	// initialize connections
	for( INT i=NetDriver->ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = NetDriver->ClientConnections(i);
		check(Connection);
		check(Connection->State==USOCK_Pending || Connection->State==USOCK_Open || Connection->State==USOCK_Closed);

		// Handle not ready channels.
		if( Connection->Actor && Connection->IsNetReady(0) && Connection->State==USOCK_Open
				&& (Connection->Driver->Time - Connection->LastReceiveTime < 1.5f) )
		{
			Connection->Viewer = Connection->Actor->GetViewTarget();
			Connection->OwnedConsiderList = new(GMem,Actors.Num()-iFirstNetRelevantActor+2)AActor*;
			Connection->OwnedConsiderListSize = 0;
		}
		else
		{
			Connection->Viewer = NULL;
		}
	}
			
	// make list of actors to consider
	AActor **ConsiderList = new(GMem,Actors.Num()-iFirstNetRelevantActor+2)AActor*;
	INT ConsiderListSize = 0;

	// Add LevelInfo to considerlist
	if( Actors(0) && (Actors(0)->RemoteRole!=ROLE_None) )
	{
		ConsiderList[0] = Actors(0);
		ConsiderListSize++;
	}
	FLOAT ServerTickTime = Engine->GetMaxTickRate();
	if ( ServerTickTime == 0.f )
		ServerTickTime = DeltaSeconds;
	else
		ServerTickTime = 1.f/ServerTickTime;
//	debugf(TEXT("START LIST"));
	for( INT i=iFirstNetRelevantActor; i<Actors.Num(); i++ )
	{
		AActor* Actor = Actors(i);
/*
		if ( Actor && (Actor->RemoteRole != ROLE_None) )
		{
			debugf(TEXT(" Consider Replicating %s"),Actor->GetName());
		}
*/
		if( Actor 
			&& (Actor->RemoteRole!=ROLE_None) 
			&& (TimeSeconds > Actor->NetUpdateTime) ) 
		{
			Actor->NetUpdateTime = TimeSeconds + appFrand() * ServerTickTime + 1.f/Actor->NetUpdateFrequency; // FIXME - cache 1/netupdatefreq
			if ( Actor->bAlwaysRelevant || !Actor->bOnlyRelevantToOwner ) 
			{
				ConsiderList[ConsiderListSize] = Actor;
				ConsiderListSize++;
			}
			else
			{
				AActor* ActorOwner = Actor->Owner;
				if ( !ActorOwner && (Actor->GetAPlayerController() || Actor->GetAPawn()) ) 
					ActorOwner = Actor;
				if ( ActorOwner )
				{
					for ( INT j=0; j<NetDriver->ClientConnections.Num(); j++ )
					{
						UNetConnection* Connection = NetDriver->ClientConnections(j);
						if ( Connection->Viewer && ((ActorOwner == Connection->Viewer) || (ActorOwner == Connection->Actor)) )
						{
							Connection->OwnedConsiderList[Connection->OwnedConsiderListSize] = Actor;
							Connection->OwnedConsiderListSize++;
						}
					}
				}
			}
		}
	}

	for( INT i=NetDriver->ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = NetDriver->ClientConnections(i);

		if( Connection->Viewer )
		{
			AActor*      Viewer    = Connection->Viewer;
			APlayerController* InViewer  = Connection->Actor;
			InViewer->Level->ReplicationViewer = InViewer;
			InViewer->Level->ReplicationViewTarget = Viewer;

		// Get list of visible/relevant actors.
		FLOAT PruneActors = 0.f;
		clock(PruneActors);
		FMemMark Mark(GMem);
		NetTag++;
		Connection->TickCount++;

		// Set up to skip all sent temporary actors.
		for( INT i=0; i<Connection->SentTemporaries.Num(); i++ )
			Connection->SentTemporaries(i)->NetTag = NetTag;

			// Get viewer coordinates.
			FVector      Location  = Viewer->Location;
			FRotator     Rotation  = InViewer->Rotation;
		  InViewer->eventGetPlayerViewPoint( Location, Rotation );  //FIXMESTEVE - should pass Viewer also

		// Compute ahead-vectors for prediction.
		FVector Ahead = FVector(0,0,0);
		if( Connection->TickCount & 1 )
		{
			FLOAT PredictSeconds = (Connection->TickCount&2) ? 0.4f : 0.9f;
			Ahead = PredictSeconds * Viewer->Velocity;
			if( Viewer->Base )
				Ahead += PredictSeconds * Viewer->Base->Velocity;
			FCheckResult Hit(1.0f);
			Hit.Location = Location + Ahead;
			Viewer->GetLevel()->Model->LineCheck(Hit,NULL,Hit.Location,Location,FVector(0,0,0),TRACE_Visible);
			Location = Hit.Location;
		}

		// Make list of all actors to consider.
		INT              ConsiderCount  = 0;
		FActorPriority*  PriorityList   = new(GMem,Actors.Num()-iFirstNetRelevantActor+2)FActorPriority;
		FActorPriority** PriorityActors = new(GMem,Actors.Num()-iFirstNetRelevantActor+2)FActorPriority*;
		FVector          ViewPos        = Viewer->Location;
		FVector          ViewDir        = InViewer->Rotation.Vector();
			UBOOL bLowNetBandwidth = ( Connection->CurrentNetSpeed/FLOAT(Viewer->Level->Game->NumPlayers + Viewer->Level->Game->NumBots)
									< (Viewer->Level->Game->bAllowVehicles ? 500.f : 300.f) );
			InViewer->bWasSaturated = InViewer->bWasSaturated && bLowNetBandwidth;

			for( INT i=0; i<ConsiderListSize; i++ )
				{
				AActor* Actor = ConsiderList[i];
				if( Actor->NetTag!=NetTag )
					{
					//debugf(TEXT("Consider %s alwaysrelevant %d frequency %f "),Actor->GetName(), Actor->bAlwaysRelevant, Actor->NetUpdateFrequency);
				UActorChannel* Channel = Connection->ActorChannels.FindRef(Actor);
				if ( Actor->bOnlyDirtyReplication
					&& Channel
						&& !Channel->ActorDirty
							&& Channel->Recent.Num()
						&& Channel->Dirty.Num() == 0 )
						{
							Channel->RelevantTime = NetDriver->Time;
						}
						else
						{
							Actor->NetTag                 = NetTag;
						PriorityList  [ConsiderCount] = FActorPriority( ViewPos, ViewDir, Connection, Channel, Actor, InViewer, bLowNetBandwidth );
							PriorityActors[ConsiderCount] = PriorityList + ConsiderCount;
							ConsiderCount++;
						}
					}
				}
			for( INT i=0; i<Connection->OwnedConsiderListSize; i++ )
			{
				AActor* Actor = Connection->OwnedConsiderList[i];
				//debugf(TEXT("Consider owned %s alwaysrelevant %d frequency %f  "),Actor->GetName(), Actor->bAlwaysRelevant,Actor->NetUpdateFrequency);
				if( Actor->NetTag!=NetTag )
					{
						UActorChannel* Channel = Connection->ActorChannels.FindRef(Actor);
						if ( Actor->bOnlyDirtyReplication
							&& Channel
						&& !Channel->ActorDirty
							&& Channel->Recent.Num()
						&& Channel->Dirty.Num() == 0 )
						{
							Channel->RelevantTime = NetDriver->Time;
						}
						else
						{
							Actor->NetTag                 = NetTag;
						PriorityList  [ConsiderCount] = FActorPriority( ViewPos, ViewDir, Connection, Channel, Actor, InViewer, bLowNetBandwidth );
							PriorityActors[ConsiderCount] = PriorityList + ConsiderCount;
							ConsiderCount++;
						}
					}
		}
			Connection->OwnedConsiderList = NULL;
			Connection->OwnedConsiderListSize = 0;
		Connection->LastRepTime = Connection->Driver->Time;

		FLOAT RelevantTime = 0.f;
		// Sort by priority.
		Sort<USE_COMPARE_POINTER(FActorPriority,UnLevTic)>( PriorityActors, ConsiderCount );
		// Update all relevant actors in sorted order.
			INT j;
			UBOOL bNewSaturated = false;
			//debugf(TEXT("START"));
			for( j=0; j<ConsiderCount; j++ )
		{
			UActorChannel* Channel     = PriorityActors[j]->Channel;
				//debugf(TEXT(" Maybe Replicate %s"),PriorityActors[j]->Actor->GetName());
			if ( !Channel || Channel->Actor ) //make sure didn't just close this channel
			{
				AActor*        Actor       = PriorityActors[j]->Actor;
			UBOOL          CanSee      = 0;
				// only check visibility on already visible actors every 1.0 + 0.5R seconds
			// bTearOff actors should never be checked
				if ( !Actor->bTearOff && (!Channel || NetDriver->Time-Channel->RelevantTime>1.f) )
				CanSee = Actor->IsNetRelevantFor( InViewer, Viewer, Location );
			if( CanSee || (Channel && NetDriver->Time-Channel->RelevantTime<NetDriver->RelevantTimeout) )
			{
				// Find or create the channel for this actor.
				if( !Channel && Connection->PackageMap->ObjectToIndex(Actor->GetClass())!=INDEX_NONE )
				{
					// Create a new channel for this actor.
					Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
					if( Channel )
						Channel->SetChannelActor( Actor );
				}
				if( Channel )
				{
					if ( !Connection->IsNetReady(0) ) // here also???
							{
								bNewSaturated = true;
						break;
							}
					if( CanSee )
							Channel->RelevantTime = NetDriver->Time + 0.5f * appFrand();
					if( Channel->IsNetReady(0) )
					{
							//debugf(TEXT("Replicate %s"),Actor->GetName());
						Channel->ReplicateActor();
						Updated++;
					}
							else
								Actor->NetUpdateTime = TimeSeconds - 1.f;
					if ( !Connection->IsNetReady(0) )
							{
								bNewSaturated = true;
						break;
				}
						}
			}
			else if( Channel )
			{
				Channel->Close();
			}
			}
		}
			InViewer->bWasSaturated = bNewSaturated;

			for ( INT k=j; k<ConsiderCount; k++ )
			{
				AActor* Actor = PriorityActors[k]->Actor;
				if( Actor->IsNetRelevantFor( InViewer, Viewer, Location ) )
					Actor->NetUpdateTime = TimeSeconds - 1.f;
			}
		Mark.Pop();
		unclock(RelevantTime);
			//debugf(TEXT("Potential %04i ConsiderList %03i ConsiderCount %03i Prune=%01.4f Relevance=%01.4f"),Actors.Num()-iFirstNetRelevantActor, 
			//			ConsiderListSize, ConsiderCount, PruneActors * GSecondsPerCycle * 1000.f,RelevantTime * GSecondsPerCycle * 1000.f);
	}
	}
	Mark.Pop();
	return Updated;
}

/*-----------------------------------------------------------------------------
	Network server tick.
-----------------------------------------------------------------------------*/

void UNetConnection::SetActorDirty(AActor* DirtyActor )
{
	if( Actor && State==USOCK_Open )
	{
		UActorChannel* Channel = ActorChannels.FindRef(DirtyActor);
		if ( Channel )
			Channel->ActorDirty = true;
	}
}

void ULevel::TickNetServer( FLOAT DeltaSeconds )
{
	// Update all clients.
	INT Updated=0;

	// first, set which channels have dirty actors (need replication)
	AActor* Actor = Actors(0);
	if( Actor && Actor->bNetDirty )
	{
		for( INT j=NetDriver->ClientConnections.Num()-1; j>=0; j-- )
			NetDriver->ClientConnections(j)->SetActorDirty(Actor);
		Actor->bNetDirty = 0;
	}
	for( INT i=iFirstNetRelevantActor; i<Actors.Num(); i++ )
	{
		AActor* Actor = Actors(i);
		if( Actor && Actor->bNetDirty )
		{
			if ( Actor->RemoteRole != ROLE_None )
			{
				for( INT j=NetDriver->ClientConnections.Num()-1; j>=0; j-- )
					NetDriver->ClientConnections(j)->SetActorDirty(Actor);
			}
			Actor->bNetDirty = 0;
		}
	}
	Updated = ServerTickClients( DeltaSeconds );

	// Log message.
	if( (INT)(TimeSeconds-DeltaSeconds)!=(INT)(TimeSeconds) )
		debugf( NAME_Title, *LocalizeProgress(TEXT("RunningNet"),TEXT("Engine")), *GetLevelInfo()->Title, *URL.Map, NetDriver->ClientConnections.Num() );

	// Stats.
	if( Updated )
	{
		for( INT i=0; i<NetDriver->ClientConnections.Num(); i++ )
		{
			UNetConnection* Connection = NetDriver->ClientConnections(i);
			if( Connection->Actor && Connection->State==USOCK_Open )
			{
				if( Connection->UserFlags&1 )
				{
					// Send stats.
/* FIXMESTEVE
					INT NumActors=0;
					for( INT i=0; i<Actors.Num(); i++ )
						NumActors += Actors(i)!=NULL;

					FString Stats = FString::Printf
					(
						TEXT("r=%i cli=%i act=%03.1f (%i) net=%03.1f pv/c=%i rep/c=%i rpc/c=%i"),
						appRound(Engine->GetMaxTickRate()),
						NetDriver->ClientConnections.Num(),
						GSecondsPerCycle*1000*GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles),
						NumActors,
						GSecondsPerCycle*1000*GStats.DWORDStats(GEngineStats.STATS_Game_NetTickCycles),
						GStats.DWORDStats(GEngineStats.STATS_Net_NumPV)   / NetDriver->ClientConnections.Num(),
						GStats.DWORDStats(GEngineStats.STATS_Net_NumReps) / NetDriver->ClientConnections.Num(),
						GStats.DWORDStats(GEngineStats.STATS_Net_NumRPC)  / NetDriver->ClientConnections.Num()
					);
					Connection->Actor->eventClientMessage( *Stats, NAME_None );
*/
				}
				if( Connection->UserFlags&2 )
				{
					FString Stats = FString::Printf
					(
						TEXT("snd=%02.1f recv=%02.1f"),
						GSecondsPerCycle*1000*Connection->Driver->SendCycles,
						GSecondsPerCycle*1000*Connection->Driver->RecvCycles
					);
					Connection->Actor->eventClientMessage( *Stats, NAME_None );
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Main level timer tick handler.
-----------------------------------------------------------------------------*/

UBOOL ULevel::IsPaused()
{
	return ( GetLevelInfo()->Pauser && (TimeSeconds >= GetLevelInfo()->PauseDelay) );
}
//
// Update the level after a variable amount of time, DeltaSeconds, has passed.
// All child actors are ticked after their owners have been ticked.
//
void ULevel::Tick( ELevelTick TickType, FLOAT DeltaSeconds )
{
	ALevelInfo* Info = GetLevelInfo();

	FMemMark Mark(GMem);
	FMemMark EngineMark(GEngineMem);
	GInitRunaway();
	InTick=1;

	// Update the net code and fetch all incoming packets.
	if( NetDriver )
	{
		NetDriver->TickDispatch( DeltaSeconds );
		if( NetDriver->ServerConnection )
			TickNetClient( DeltaSeconds );
	}
	// Update collision.
	if( Hash )
		Hash->Tick();

	// Update batched lines.
	PersistentLineBatcher->Tick(DeltaSeconds);

	// Update time.
	if( !GIsBenchmarking )
		DeltaSeconds *= Info->TimeDilation;
	// Clamp time between 2000 fps and 2.5 fps.
	DeltaSeconds = Clamp(DeltaSeconds,0.0005f,0.40f);

	if ( !IsPaused() )
		TimeSeconds += DeltaSeconds;
	Info->TimeSeconds = TimeSeconds;
	UpdateTime(Info);
	if( Info->bPlayersOnly )
		TickType = LEVELTICK_ViewportsOnly;

	// If caller wants time update only, or we are paused, skip the rest.
	if
	(	(TickType!=LEVELTICK_TimeOnly)
	&&	!IsPaused()
	&&	(!NetDriver || !NetDriver->ServerConnection || NetDriver->ServerConnection->State==USOCK_Open) )
	{
		// Tick all actors, owners before owned.
		NewlySpawned = NULL;
		INT Updated  = 1;

		TickLevelRBPhys(DeltaSeconds);

		for( INT iActor=iFirstDynamicActor; iActor<Actors.Num(); iActor++ )
		{
			if( Actors( iActor ) && !Actors(iActor)->bDeleteMe )
			{
				Updated += Actors( iActor )->Tick(DeltaSeconds,TickType);
			}
		}

		while( NewlySpawned && Updated )
		{
			FActorLink* Link = NewlySpawned;
			NewlySpawned     = NULL;
			Updated          = 0;
			for( Link; Link; Link=Link->Next )
			{
				if( Link->Actor->bTicked!=(DWORD)Ticked && !Link->Actor->bDeleteMe )
				{
					Updated += Link->Actor->Tick( DeltaSeconds, TickType );
		}
			}
		}

		// Tick all objects inheriting from FTickableObjects.
		for( INT i=0; i<FTickableObject::TickableObjects.Num(); i++ )
			FTickableObject::TickableObjects(i)->Tick( DeltaSeconds );

		// update the base sequence (not on client)
		if( GameSequence != NULL &&
			GetLevelInfo()->NetMode != NM_Client )
		{
			GameSequence->UpdateOp( DeltaSeconds );
		}
	}
	else if( IsPaused() )
	{
		// Absorb input if paused.
		NewlySpawned = NULL;
		INT Updated  = 1;
		for( INT iActor=iFirstDynamicActor; iActor<Actors.Num(); iActor++ )
		{
			APlayerController* PC=Cast<APlayerController>(Actors(iActor));
			if( PC )
			{
				PC->PlayerInput->eventPlayerInput( DeltaSeconds );
				//FIXMESTEVE PC->LastPlayerCalcView = 0.f;
				for( TFieldIterator<UFloatProperty> It(PC->GetClass()); It; ++It )
					if( It->PropertyFlags & CPF_Input )
						*(FLOAT*)((BYTE*)PC + It->Offset) = 0.f;
				PC->bTicked = (DWORD)Ticked;
			}
			else if( Actors(iActor) )
			{
				if ( Actors(iActor)->bAlwaysTick && !Actors(iActor)->bDeleteMe )
					Actors(iActor)->Tick(DeltaSeconds,TickType);
				else
					Actors(iActor)->bTicked = (DWORD)Ticked;
			}
		}
		while( NewlySpawned && Updated )
		{
			FActorLink* Link = NewlySpawned;
			NewlySpawned     = NULL;
			Updated          = 0;
			for( Link; Link; Link=Link->Next )
				if( Link->Actor->bTicked!=(DWORD)Ticked && !Link->Actor->bDeleteMe )
				{
					if ( Link->Actor->bAlwaysTick )
						Updated += Link->Actor->Tick( DeltaSeconds, TickType );
					else
						Link->Actor->bTicked = (DWORD)Ticked;
				}
		}
	}

	// Update net server and flush networking.
	if( NetDriver )
	{
		if( !NetDriver->ServerConnection )
			TickNetServer( DeltaSeconds );
		NetDriver->TickFlush();
	}

	// Finish up.
	Ticked = !Ticked;
	InTick = 0;
	Mark.Pop();
	EngineMark.Pop();

	CleanupDestroyed( 0 );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

