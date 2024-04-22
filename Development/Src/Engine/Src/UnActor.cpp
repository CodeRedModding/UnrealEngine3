/*=============================================================================
	UnActor.cpp: AActor implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "EngineAIClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineMeshClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineFoliageClasses.h"
#include "UnStatChart.h"
#include "BSPOps.h"
#include "EngineAnimClasses.h"

/**
 * PERF_ISSUE_FINDER
 *
 * Point checks should not take an excessively long time as we do lots of them
 *
 * Turn this on to have the engine log when a specific actor is taking longer than 
 * PERF_SHOW_SLOW_OVERLAPS_TAKING_LONG_TIME_AMOUNT to do its IsOverlapping check.
 *
 **/
//#define PERF_SHOW_SLOW_OVERLAPS 1
const static FLOAT PERF_SHOW_SLOW_OVERLAPS_TAKING_LONG_TIME_AMOUNT = 0.05f; // modify this value to look at larger or smaller sets of "bad" actors

#define PERF_SHOW_NONPAWN_VOLTRIG_OVERLAPS 0


/*-----------------------------------------------------------------------------
	AActor object implementations.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AActor);
IMPLEMENT_CLASS(ALight);
IMPLEMENT_CLASS(ADirectionalLight);
IMPLEMENT_CLASS(ADominantDirectionalLight);
IMPLEMENT_CLASS(ADominantDirectionalLightMovable);
IMPLEMENT_CLASS(ADirectionalLightToggleable);
IMPLEMENT_CLASS(APointLight);
IMPLEMENT_CLASS(APointLightMovable);
IMPLEMENT_CLASS(APointLightToggleable);
IMPLEMENT_CLASS(ADominantPointLight);
IMPLEMENT_CLASS(ASpotLight);
IMPLEMENT_CLASS(ASpotLightMovable);
IMPLEMENT_CLASS(ASpotLightToggleable);
IMPLEMENT_CLASS(AGeneratedMeshAreaLight);
IMPLEMENT_CLASS(ADominantSpotLight);
IMPLEMENT_CLASS(AWeapon);
IMPLEMENT_CLASS(ANote);
IMPLEMENT_CLASS(AWorldInfo);
IMPLEMENT_CLASS(AGameInfo);
IMPLEMENT_CLASS(AZoneInfo);
IMPLEMENT_CLASS(UReachSpec);
IMPLEMENT_CLASS(UForcedReachSpec);
IMPLEMENT_CLASS(UAdvancedReachSpec);
IMPLEMENT_CLASS(ULadderReachSpec);
IMPLEMENT_CLASS(UProscribedReachSpec);
IMPLEMENT_CLASS(USlotToSlotReachSpec);
IMPLEMENT_CLASS(UMantleReachSpec);
IMPLEMENT_CLASS(USwatTurnReachSpec);
IMPLEMENT_CLASS(UCoverSlipReachSpec);
IMPLEMENT_CLASS(UWallTransReachSpec);
IMPLEMENT_CLASS(UFloorToCeilingReachSpec);
IMPLEMENT_CLASS(UCeilingReachSpec);
IMPLEMENT_CLASS(APathNode);
IMPLEMENT_CLASS(ANavigationPoint);
IMPLEMENT_CLASS(APylon);
IMPLEMENT_CLASS(ADynamicPylon);
IMPLEMENT_CLASS(AAISwitchablePylon);
IMPLEMENT_CLASS(AScout);
IMPLEMENT_CLASS(AProjectile);
IMPLEMENT_CLASS(ATeleporter);
IMPLEMENT_CLASS(APlayerStart);
IMPLEMENT_CLASS(AKeypoint);
IMPLEMENT_CLASS(ATargetPoint);
IMPLEMENT_CLASS(AInventory);
IMPLEMENT_CLASS(AInventoryManager);
IMPLEMENT_CLASS(APickupFactory);
IMPLEMENT_CLASS(ATrigger);
IMPLEMENT_CLASS(ATrigger_PawnsOnly);
IMPLEMENT_CLASS(AHUD);
IMPLEMENT_CLASS(USavedMove);
IMPLEMENT_CLASS(AInfo);
IMPLEMENT_CLASS(AReplicationInfo);
IMPLEMENT_CLASS(APlayerReplicationInfo);
IMPLEMENT_CLASS(AGameReplicationInfo);
IMPLEMENT_CLASS(ADroppedPickup);
IMPLEMENT_CLASS(AController);
IMPLEMENT_CLASS(AAIController);
IMPLEMENT_CLASS(APlayerController);
IMPLEMENT_CLASS(AMutator);
IMPLEMENT_CLASS(AVehicle);
IMPLEMENT_CLASS(ALadder);
IMPLEMENT_CLASS(UDamageType);
IMPLEMENT_CLASS(UKillZDamageType);
IMPLEMENT_CLASS(ABrush);
IMPLEMENT_CLASS(ABrushShape);
IMPLEMENT_CLASS(AVolume);
IMPLEMENT_CLASS(APhysicsVolume);
IMPLEMENT_CLASS(AGravityVolume);
IMPLEMENT_CLASS(ADefaultPhysicsVolume);
IMPLEMENT_CLASS(ALadderVolume);
IMPLEMENT_CLASS(APotentialClimbWatcher);
IMPLEMENT_CLASS(ABlockingVolume);
IMPLEMENT_CLASS(AAutoLadder);
IMPLEMENT_CLASS(ATeamInfo);
IMPLEMENT_CLASS(UEdCoordSystem);
IMPLEMENT_CLASS(ALiftCenter);
IMPLEMENT_CLASS(ALiftExit);
IMPLEMENT_CLASS(UBookMark);
IMPLEMENT_CLASS(UBookMark2D);
IMPLEMENT_CLASS(UKismetBookMark);
IMPLEMENT_CLASS(UClipPadEntry);
IMPLEMENT_CLASS(ATriggerVolume);
IMPLEMENT_CLASS(AVolumePathNode);
IMPLEMENT_CLASS(ADoorMarker);
IMPLEMENT_CLASS(ADynamicBlockingVolume);
IMPLEMENT_CLASS(APortalMarker);
IMPLEMENT_CLASS(UTeleportReachSpec);
IMPLEMENT_CLASS(ALevelStreamingVolume);
IMPLEMENT_CLASS(UMapInfo);
IMPLEMENT_CLASS(ACullDistanceVolume);
IMPLEMENT_CLASS(AEmitterPool);
IMPLEMENT_CLASS(ASkyLight);
IMPLEMENT_CLASS(ASkyLightToggleable);
IMPLEMENT_CLASS(AAutoTestManager);
IMPLEMENT_CLASS(UEditorLinkSelectionInterface);
IMPLEMENT_CLASS(UInterface_Speaker);
IMPLEMENT_CLASS(UAICommandBase);
IMPLEMENT_CLASS(UCloudSaveSystem);

/** Slow version of deref that will use GUID if Actor is NULL */
AActor* FActorReference::operator~()
{
	return (Actor ? Actor : (Guid.IsValid() ? GWorld->FindActorByGuid(Guid) : NULL));
}

FArchive& operator<<( FArchive& Ar, FActorReference& T )
{
	Ar << T.Actor;
	Ar << T.Guid;	
	return Ar;
}

FArchive& operator<<( FArchive& Ar, FCoverReference& T )
{
	Ar << ((FActorReference&)T);
	Ar << T.SlotIdx;

	if( Ar.Ver() < VER_REMOVED_DIR_COVERREF )
	{
		// dummy serialize direction
		INT HelloIAmAUselessInt=0;
		Ar << HelloIAmAUselessInt;
	}
	
	return Ar;
}

FBasedPosition::FBasedPosition()
{
	Base = NULL;
	Position = FVector(0,0,0);
}
FBasedPosition::FBasedPosition(EEventParm)
{
	appMemzero(this, sizeof(FBasedPosition));
}
FBasedPosition::FBasedPosition( class AActor *InBase, FVector& InPosition )
{
	Set( InBase, InPosition );
}

FArchive& operator<<( FArchive& Ar, FBasedPosition& T )
{
	Ar << T.Base;
	Ar << T.Position;	
	return Ar;
}

// Retrieve world location of this position
FVector FBasedPosition::operator*()
{
	if( Base != NULL )
	{
		// If base hasn't changed location/rotation use cached transformed position
		if( CachedBaseLocation != Base->Location ||
			CachedBaseRotation != Base->Rotation )
		{
			CachedBaseLocation	= Base->Location;
			CachedBaseRotation	= Base->Rotation;
			CachedTransPosition = Base->Location + FRotationMatrix(Base->Rotation).TransformFVector(Position);
		}

		return CachedTransPosition;
	}
	return Position;
}

void FBasedPosition::Set( class AActor* InBase, FVector& InPosition )
{
	if( InPosition.IsNearlyZero() )
	{
		Base = NULL;
		Position = FVector(0,0,0);
		return;
	}

	Base = (InBase && !InBase->IsStatic()) ? InBase : NULL;
	if( Base != NULL )
	{
		Position = FRotationMatrix(Base->Rotation).InverseTransformFVectorNoScale( InPosition - Base->Location );

		CachedBaseLocation	= Base->Location;
		CachedBaseRotation	= Base->Rotation;
		CachedTransPosition = InPosition;
	}
	else
	{
		Position = InPosition;
	}
}

void FBasedPosition::Clear()
{
	Base = NULL;
	Position = FVector(0);
}

void AActor::Vect2BP( FBasedPosition& BP, FVector Pos, AActor* ForcedBase ) const
{
	if( ForcedBase == NULL )
	{
		APawn* P = const_cast<AActor*>(this)->GetAPawn();
		if( P == NULL )
		{
			AController* C = const_cast<AActor*>(this)->GetAController();
			if( C != NULL && C->Pawn != NULL )
			{
				P = C->Pawn;
			}
		}

		if( P != NULL )
		{
			ForcedBase = P->Base;
		}
	}
	BP.Set( ForcedBase, Pos );
}
FVector AActor::BP2Vect( FBasedPosition BP ) const
{
	return *BP;
}

void AActor::SetBasedPosition( FBasedPosition& BP, FVector Pos, AActor* ForcedBase ) const
{
	Vect2BP(BP,Pos,ForcedBase);
}

FVector AActor::GetBasedPosition( FBasedPosition BP ) const
{
	return BP2Vect(BP);
}



/** 
 *	Do anything needed to clear out cross level references; Called from ULevel::PreSave
 */
void AActor::ClearCrossLevelReferences()
{
	if( Base != NULL && GetOutermost() != Base->GetOutermost() )
	{
		SetBase( NULL );
	}
}

void AActor::NetDirty(UProperty* property)
{
	if ( property && (property->PropertyFlags & CPF_Net) )
	{
		// test and make sure actor not getting dirtied too often!
		bNetDirty = true;
	}
}

INT* AActor::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	if ( bSkipActorPropertyReplication && !bNetInitial )
	{
		return Ptr;
	}

	if ( Role == ROLE_Authority )
	{
		DOREP(Actor,bHardAttach);
  		if ( bReplicateMovement )
		{
			UBOOL bRigidActor = (Physics == PHYS_RigidBody && ((AActor*)Recent)->Physics == PHYS_RigidBody && (!bNetInitial || bStatic || bNoDelete));
			if ( RemoteRole != ROLE_AutonomousProxy )
			{
				if ( !bRigidActor )
				{
					UBOOL bAlreadyLoc = false;

					// If the actor was based and is no longer, send the location!
					if ( !Base && ((AActor*)Recent)->Base )
					{
						static UProperty* spLocation = FindObjectChecked<UProperty>(AActor::StaticClass(),TEXT("Location"));
						*Ptr++ = spLocation->RepIndex;
						bAlreadyLoc = true;
					}

					DOREP(Actor,Base);
					if( Base && !Base->bWorldGeometry && Map->CanSerializeObject(Base) )
					{
						if ((((AActor*)Recent)->RemoteRole == ROLE_AutonomousProxy || ((AActor*)Recent)->Base != Base) && (!bNetInitial || (!bStatic && !bNoDelete)))
						{
							// We've changed bases, relative location and rotation may be out of sync with what the client has, but still match the last replicated values.
							static UProperty* spRelativeLocation = FindObjectChecked<UProperty>(AActor::StaticClass(),TEXT("RelativeLocation"));
							*Ptr++ = spRelativeLocation->RepIndex;
							static UProperty* spRelativeRotation = FindObjectChecked<UProperty>(AActor::StaticClass(),TEXT("RelativeRotation"));
							*Ptr++ = spRelativeRotation->RepIndex;
						}
						else if (bUpdateSimulatedPosition)
						{
							DOREP(Actor,RelativeLocation);
							DOREP(Actor,RelativeRotation);
						}
					}
					else 
					{
						if (bUpdateSimulatedPosition)
						{
							if (!bAlreadyLoc)
							{
								// if velocity changed to zero, make sure location gets replicated again one last time
								if ( Velocity.IsZero() && NEQ(Velocity, ((AActor*)Recent)->Velocity, Map, Channel) )
								{
									static UProperty* spLocationb = FindObjectChecked<UProperty>(AActor::StaticClass(),TEXT("Location"));
									*Ptr++ = spLocationb->RepIndex;
								}
								else
								{
									DOREP(Actor,Location);
								}
							}

							if (!bNetInitial)
							{
								DOREP(Actor,Physics);
								// if Physics has changed but Base hasn't, force it to be sent anyway
								// this fixes client ambiguity related to SetPhysics() potentially modifying Base
								if (Physics != ((AActor*)Recent)->Physics && Base == ((AActor*)Recent)->Base)
								{
									static UProperty* spBase = FindObjectChecked<UProperty>(AActor::StaticClass(),TEXT("Base"));
									*Ptr++ = spBase->RepIndex;
								}
									DOREP(Actor,Rotation);
								}
							else if (!bNetInitialRotation)
							{
								DOREP(Actor,Rotation);
							}
						}
						else if (bNetInitial && !bNetInitialRotation && !bStatic && !bNoDelete)
						{
							DOREP(Actor,Rotation);
						}
					}
				}
				else if (bReplicateRigidBodyLocation)
				{
					DOREP(Actor,Location);
				}

				if ( RemoteRole == ROLE_SimulatedProxy )
				{
					if ( !bRigidActor && (bNetInitial || bUpdateSimulatedPosition) )
					{
						DOREP(Actor,Velocity);
					}

					if ( bNetInitial )
					{
						DOREP(Actor,Physics);
						// if Physics has changed but Base hasn't, force it to be sent anyway
						// this fixes client ambiguity related to SetPhysics() potentially modifying Base
						if (Physics != ((AActor*)Recent)->Physics && Base == ((AActor*)Recent)->Base)
						{
							static UProperty* spBase = FindObjectChecked<UProperty>(AActor::StaticClass(),TEXT("Base"));
							*Ptr++ = spBase->RepIndex;
						}
					}
				}
			}
			else if ( bNetInitial && !bNetInitialRotation )
			{
				DOREP(Actor,Rotation);
			}
		}
		if ( bNetDirty )
		{
			DOREP(Actor,DrawScale);
			//DOREP(Actor,DrawScale3D); // Doesn't work in networking, because of vector rounding
			DOREP(Actor,bCollideActors);
			DOREP(Actor,bCollideWorld);
			DOREP(Actor,ReplicatedCollisionType);
			DOREP(Actor,bHidden);
			if( bCollideActors || bCollideWorld )
			{
				DOREP(Actor,bProjTarget);
				DOREP(Actor,bBlockActors);
			}
			if ( !bSkipActorPropertyReplication )
			{
				// skip these if bSkipActorPropertyReplication, because if they aren't relevant to the client, bNetInitial never gets cleared
				// which obviates bSkipActorPropertyReplication
 				if ( bNetOwner )
				{
					DOREP(Actor,Owner);
				}
				else if (((AActor*)Recent)->Owner != NULL && ((AActor*)Recent)->Owner != Owner)
				{
					// we don't do DOREP() here because NEQ() will mark Actor to stay dirty if Owner can't be serialized, which we don't want
					// but we do need it to replicate a NULL in that case
					static UProperty* spOwnerb = FindObjectChecked<UProperty>(AActor::StaticClass(), TEXT("Owner"));
					*Ptr++ = spOwnerb->RepIndex;
				}

				if ( bReplicateInstigator && (!bNetTemporary || (Instigator && Map->CanSerializeObject(Instigator))) )
				{
					DOREP(Actor,Instigator);
				}
			}
		}
		DOREP(Actor,Role);
		DOREP(Actor,RemoteRole);
		DOREP(Actor,bNetOwner);
		DOREP(Actor,bTearOff);
	}

	return Ptr;
}

INT* APawn::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	// listing these properties here guarantees they will be received no later than receiving bTearOff (which is in the Super call)
	if (bTearOff && bNetDirty)
	{
		DOREP(Pawn,HitDamageType);
		DOREP(Pawn,TakeHitLocation);
		DOREP(Pawn,TearOffMomentum);
	}

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if ( Role==ROLE_Authority )
	{
		if( bNetDirty )
		{
			DOREP(Pawn,HealthMax);
		}

		if (!bNetOwner || bDemoRecording)
		{
			DOREP(Pawn,RemoteViewPitch);
		}

		if( bNetInitial && !bNetOwner )
		{
			DOREP(Pawn,bRootMotionFromInterpCurve);
		
			if( bRootMotionFromInterpCurve )
			{
				DOREP(Pawn,RootMotionInterpRate);
				DOREP(Pawn,RootMotionInterpCurrentTime);
				DOREP(Pawn,RootMotionInterpCurveLastValue);
			}
		}

		if ( bNetDirty )
		{
			DOREP(Pawn,PlayerReplicationInfo); 
			if (bNetOwner || bReplicateHealthToAll)
			{
				DOREP(Pawn,Health);
				DOREP(Pawn,HealthMax);
			}
			else
			{
				UBOOL bShouldReplicateHealth = FALSE;
				for (INT i = 0; i < WorldInfo->ReplicationViewers.Num(); i++)
				{
					if (WorldInfo->ReplicationViewers(i).InViewer->ViewTarget == this || WorldInfo->ReplicationViewers(i).Viewer == this)
					{
						bShouldReplicateHealth = TRUE;
						break;
					}
				}
				if (bShouldReplicateHealth)
				{
					DOREP(Pawn,Health);
				}
			}

			if (bNetOwner && (Controller == NULL || Map->CanSerializeObject(Controller)) && (!bDemoRecording || Controller == NULL || !Controller->bForceDemoRelevant))
			{
				DOREP(Pawn,Controller);
			}
			else if (((APawn*)Recent)->Controller != NULL && ((APawn*)Recent)->Controller != Controller)
			{
				// we don't do DOREP() here because NEQ() will mark Pawn to stay dirty if Controller can't be serialized, which we don't want
				// but we do need it to replicate a NULL in that case
				static UProperty* spControllerb = FindObjectChecked<UProperty>(APawn::StaticClass(), TEXT("Controller"));
				*Ptr++ = spControllerb->RepIndex;
			}

			if( bNetOwner )
			{
				DOREP(Pawn,GroundSpeed);
				DOREP(Pawn,WaterSpeed);
				DOREP(Pawn,AirSpeed);
				DOREP(Pawn,AccelRate);
				DOREP(Pawn,JumpZ);
				DOREP(Pawn,AirControl);
				DOREP(Pawn,InvManager);
				if (bNetInitial)
				{
					DOREP(Pawn,bCanSwatTurn);
				}
			}

			if (!bNetOwner || bDemoRecording)
			{
				DOREP(Pawn,bIsCrouched);
				DOREP(Pawn,FlashCount);
				DOREP(Pawn,FiringMode);
			}
			else
			{
				// update Recent so that if the player loses ownership of this Pawn (e.g. gets out of vehicle)
				// it won't replicate FlashCount unless it changes again after the possession change
				// this way the client won't get an erroneous extra firing event that it already performed as owner
				((APawn*)Recent)->FlashCount = FlashCount;
			}

			DOREP(Pawn,FlashLocation);
			DOREP(Pawn,bSimulateGravity);
			DOREP(Pawn,bIsWalking);

			if (DrivenVehicle == NULL || Map->CanSerializeObject(DrivenVehicle))
			{
				DOREP(Pawn,DrivenVehicle);
			}

			if (!bTearOff)
			{
				DOREP(Pawn,HitDamageType);
				DOREP(Pawn,TakeHitLocation);
			}

			DOREP(Pawn, bUsedByMatinee);
			DOREP(Pawn, bFastAttachedMove);
		}
	}

	return Ptr;
}

INT* AVehicle::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if ( Role == ROLE_Authority )
	{
		if ( bNetDirty )
		{
			DOREP(Vehicle,bDriving);
			if ( bNetOwner || !Driver || !Driver->bHidden )
			{
				DOREP(Vehicle,Driver);
			}
		}
	}

	return Ptr;
}

INT* ASVehicle::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if (Physics == PHYS_RigidBody && (Controller != NULL || NEQ(VState.RBState, ((ASVehicle*)Recent)->VState.RBState, Map, Channel)))
	{
		DOREP(SVehicle,VState);
	}

	if(bNetDirty && (Role == ROLE_Authority))
	{
		DOREP(SVehicle,MaxSpeed);
	}

	return Ptr;
}

INT* AController::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if( bNetDirty && (Role==ROLE_Authority) )
	{
		DOREP(Controller,PlayerReplicationInfo);
		DOREP(Controller,Pawn);
	}

	return Ptr;
}

INT* APlayerController::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if( bNetOwner && (Role==ROLE_Authority) )
	{
		if ( (ViewTarget != Pawn) && ViewTarget && ViewTarget->GetAPawn() )
		{
			DOREP(PlayerController,TargetViewRotation);
			DOREP(PlayerController,TargetEyeHeight);
		}
	}

	return Ptr;
}

void APhysicsVolume::Spawned()
{
	Super::Spawned();

	Register();
}

void APhysicsVolume::PostLoad()
{
	Super::PostLoad();

	Register();
}

void APhysicsVolume::BeginDestroy()
{
	Super::BeginDestroy();

	Unregister();
}

void APhysicsVolume::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Register();
}

void APhysicsVolume::Register()
{
	if(WorldInfo)
	{
		if(WorldInfo->FirstPhysicsVolume == NULL)
		{
			WorldInfo->FirstPhysicsVolume = this;

			NextPhysicsVolume = NULL;
		}
		else
		{
			APhysicsVolume* CurrentVolume(WorldInfo->FirstPhysicsVolume);

			while(CurrentVolume->NextPhysicsVolume && CurrentVolume != this)
			{
				CurrentVolume = CurrentVolume->NextPhysicsVolume;
			}

			if(CurrentVolume != this)
			{
				CurrentVolume->NextPhysicsVolume = this;

				NextPhysicsVolume = NULL;
			}
		}
	}
}

void APhysicsVolume::Unregister()
{
	if(WorldInfo)
	{
		APhysicsVolume* CurrentVolume(WorldInfo->FirstPhysicsVolume);

		if(CurrentVolume == this)
		{
			WorldInfo->FirstPhysicsVolume = NextPhysicsVolume;
		}
		else
		{
			while(CurrentVolume && CurrentVolume->NextPhysicsVolume && CurrentVolume->NextPhysicsVolume != this)
			{
				CurrentVolume = CurrentVolume->NextPhysicsVolume;
			}

			if(CurrentVolume && CurrentVolume->NextPhysicsVolume == this)
			{
				CurrentVolume->NextPhysicsVolume = CurrentVolume->NextPhysicsVolume->NextPhysicsVolume;
			}
		}
	}

	NextPhysicsVolume = NULL;
}

INT* APhysicsVolume::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if( (Role==ROLE_Authority) && bSkipActorPropertyReplication && !bNetInitial )
	{
		DOREP(Actor,Location);
		DOREP(Actor,Rotation);
		DOREP(Actor,Base);
		if( Base && !Base->bWorldGeometry )
		{
			DOREP(Actor,RelativeLocation);
			DOREP(Actor,RelativeRotation);
		}
	}

	return Ptr;
}

static inline UBOOL NEQ(FMusicTrackStruct& A,FMusicTrackStruct& B,UPackageMap* Map,UActorChannel* Channel) {return (A.TheSoundCue != B.TheSoundCue) || (A.MP3Filename != B.MP3Filename);}


INT* AWorldInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	// only replicate needed actor properties
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if (bNetDirty)
	{
		DOREP(WorldInfo,Pauser);
		DOREP(WorldInfo,TimeDilation);
		DOREP(WorldInfo,WorldGravityZ);
		DOREP(WorldInfo,bHighPriorityLoading);
		// bias towards doing nothing instead of sending NULL if the music can't be serialized, so the client keeps using the old track
		if (ReplicatedMusicTrack.TheSoundCue == NULL || Map->SupportsObject(ReplicatedMusicTrack.TheSoundCue))
		{
			DOREP(WorldInfo,ReplicatedMusicTrack);
		}
		else
		{
			Channel->bActorMustStayDirty = TRUE;
		}
	}

	return Ptr;
}

INT* AReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{

	if ( !bSkipActorPropertyReplication )
		Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	return Ptr;
}

INT* APlayerReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if (bNetDirty)
	{
		DOREP(PlayerReplicationInfo,Score);
		DOREP(PlayerReplicationInfo,Deaths);

		if (!bNetOwner)
		{
			DOREP(PlayerReplicationInfo,Ping);
		}
		DOREP(PlayerReplicationInfo,PlayerName);
		DOREP(PlayerReplicationInfo,Team);
		DOREP(PlayerReplicationInfo,bAdmin);
		DOREP(PlayerReplicationInfo,bIsSpectator);
		DOREP(PlayerReplicationInfo,bOnlySpectator);
		DOREP(PlayerReplicationInfo,bWaitingPlayer);
		DOREP(PlayerReplicationInfo,bReadyToPlay);
		DOREP(PlayerReplicationInfo,bOutOfLives);
		DOREP(PlayerReplicationInfo,bFromPreviousLevel);
		DOREP(PlayerReplicationInfo,StartTime);
		// NOTE: This needs to be replicated to the owning client so don't move it from here
		DOREP(PlayerReplicationInfo,UniqueId);

		if (bNetInitial)
		{
			DOREP(PlayerReplicationInfo,PlayerID);
			DOREP(PlayerReplicationInfo,bBot);
			DOREP(PlayerReplicationInfo,bIsInactive);
		}
	}

	return Ptr;
}

INT* AGameReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if( (Role==ROLE_Authority) && bNetDirty )
	{
		DOREP(GameReplicationInfo,bStopCountDown);
		DOREP(GameReplicationInfo,bMatchHasBegun);
		DOREP(GameReplicationInfo,bMatchIsOver);
		DOREP(GameReplicationInfo,Winner);
		if ( bNetInitial )
		{
			DOREP(GameReplicationInfo,GameClass);
			DOREP(GameReplicationInfo,RemainingTime);
			DOREP(GameReplicationInfo,ElapsedTime);
			DOREP(GameReplicationInfo,GoalScore);
			DOREP(GameReplicationInfo,TimeLimit);
			DOREP(GameReplicationInfo,ServerName);

			// prevent RemainingMinute from getting replicated until it changes as we already send the more up to date RemainingTime in this case
			((AGameReplicationInfo*)Recent)->RemainingMinute = RemainingMinute;
		}
		else
		{
			DOREP(GameReplicationInfo,RemainingMinute);
		}
	}

	return Ptr;
}

INT* ATeamInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	
	if( Role==ROLE_Authority )
	{
		if ( bNetDirty )
		{
			DOREP(TeamInfo,Score);
		}
		if ( bNetInitial )
		{
			DOREP(TeamInfo,TeamName);
			DOREP(TeamInfo,TeamIndex);
		}
	}

	return Ptr;
}

INT* APickupFactory::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	if (bNetInitial)
	{
		DOREP(PickupFactory,InventoryType);
	}
	DOREP(PickupFactory,bPickupHidden);
	if ( bOnlyReplicateHidden )
	{
		DOREP(Actor,bHidden);
		if ( bNetInitial )
		{
			DOREP(Actor,Rotation);
		}
	}
	else
	{
		Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	}
	
	return Ptr;
}

INT* AInventory::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( bNetOwner && (Role==ROLE_Authority) && bNetDirty )
	{
		DOREP(Inventory,InvManager);
		DOREP(Inventory,Inventory);
	}

	return Ptr;
}

INT* AMatineeActor::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	if (!bSkipActorPropertyReplication)
	{
		Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);
	}

	if (bNetDirty)
	{
		if (bNetInitial)
		{
			DOREP(MatineeActor,InterpAction);
		}
		DOREP(MatineeActor,bIsPlaying);
		DOREP(MatineeActor,bReversePlayback);
		DOREP(MatineeActor,bPaused);
		DOREP(MatineeActor,PlayRate);
		DOREP(MatineeActor,Position);
		DOREPARRAY(MatineeActor, AIGroupNames);
		DOREPARRAY(MatineeActor, AIGroupPawns);

		// If we couldn't replicate a property, make sure we try again right away
		if (Channel->bActorMustStayDirty)
		{
			bForceNetUpdate = TRUE;
		}
	}	

	return Ptr;
}

INT* AKActor::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);

	if (bNeedsRBStateReplication)
	{
		DOREP(KActor,RBState);
	}
	if (bNetInitial)
	{
		DOREP(KActor,bWakeOnLevelStart);
		DOREP(KActor,ReplicatedDrawScale3D);
	}

	return Ptr;
}

INT* AKAsset::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);

	if ( bNetDirty )
	{
		DOREP(KAsset,ReplicatedMesh);
		DOREP(KAsset,ReplicatedPhysAsset);
	}
	return Ptr;
}
 
INT* AVolume::GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel)
{
	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);

	Ptr = Super::GetOptimizedRepList(Recent, Retire, Ptr, Map, Channel);

	// LDs can change collision of dynamic volumes at runtime via Kismet, so we need to make sure we replicate that even if bSkipActorPropertyReplication is true
	if (bSkipActorPropertyReplication && !bNetInitial)
	{
		DOREP(Actor,bCollideActors);
	}

	return Ptr;
}

/*-----------------------------------------------------------------------------
	AActor networking implementation.
-----------------------------------------------------------------------------*/

//
// Static variables for networking.
//
static FVector				SavedLocation;
static FVector				SavedRelativeLocation;
static FRotator				SavedRotation;
static FRotator				SavedRelativeRotation;
static AActor*				SavedBase;
static DWORD				SavedCollision;
static FLOAT				SavedRadius;
static FLOAT				SavedHeight;
static FVector				SavedSimInterpolate;
static FLOAT				SavedDrawScale;
static AVehicle*			SavedDrivenVehicle;
static UBOOL				SavedHardAttach;
static UBOOL				SavedbIsCrouched;
static USeqAct_Interp*		SavedInterpAction;
static UBOOL				SavedbIsPlaying;
static FLOAT				SavedPosition;
static BYTE					SavedPhysics;
static UBOOL				SavedbHidden;
static AActor*				SavedOwner;
static BYTE					SavedReplicatedCollisionType;

/** GetNetPriority()
@param Viewer		PlayerController owned by the client for whom net priority is being determined
@param InChannel	Channel on which this actor is being replicated.
@param Time			Time since actor was last replicated
@returns			Priority of this actor for replication
*/
FLOAT AActor::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth)
{
	if ( Instigator && (Instigator == Viewer->Pawn) )
		Time *= 4.f; 
	else if ( !bHidden )
	{
		FVector Dir = Location - ViewPos;
		FLOAT DistSq = Dir.SizeSquared();
		
		// adjust priority based on distance and whether actor is in front of viewer
		if ( (ViewDir | Dir) < 0.f )
		{
			if ( DistSq > NEARSIGHTTHRESHOLDSQUARED )
				Time *= 0.2f;
			else if ( DistSq > CLOSEPROXIMITYSQUARED )
				Time *= 0.4f;
		}
		else if ( DistSq > MEDSIGHTTHRESHOLDSQUARED )
			Time *= 0.4f;
		else if ( Base && (Base == Viewer->Pawn) )
		{
			// if based on viewer, increase priority
			Time *= 3.f;
		}
	}

	return NetPriority * Time;
}

//
// Always called immediately before properties are received from the remote.
//
void AActor::PreNetReceive()
{
	SavedLocation   = Location;
	SavedRotation   = Rotation;
	SavedRelativeLocation = RelativeLocation;
	SavedRelativeRotation = RelativeRotation;
	SavedBase       = Base;
	SavedHardAttach = bHardAttach;
	SavedCollision  = bCollideActors;
	SavedDrawScale	= DrawScale;
	SavedPhysics = Physics;
	SavedbHidden = bHidden;
	SavedOwner = Owner;
	SavedReplicatedCollisionType = ReplicatedCollisionType;
}

void APawn::PreNetReceive()
{
	SavedbIsCrouched = bIsCrouched;
	SavedDrivenVehicle = DrivenVehicle;
	AActor::PreNetReceive();
}

//
// Always called immediately after properties are received from the remote.
//
void AActor::PostNetReceive()
{
	Exchange ( Location,        SavedLocation  );
	Exchange ( Rotation,        SavedRotation  );
	Exchange ( RelativeLocation,        SavedRelativeLocation  );
	Exchange ( RelativeRotation,        SavedRelativeRotation  );
	Exchange ( Base,            SavedBase      );
	ExchangeB( bCollideActors,  SavedCollision );
	Exchange ( DrawScale, SavedDrawScale       );
	ExchangeB( bHardAttach, SavedHardAttach );
	ExchangeB( bHidden, SavedbHidden );
	Exchange ( Owner, SavedOwner );

	if (bHidden != SavedbHidden)
	{
		SetHidden(SavedbHidden);
	}
	if (Owner != SavedOwner)
	{
		SetOwner(SavedOwner);
	}

	// process ReplicatedCollisionType first and restore flags on the Actor so that the replicated values take precedence
	if (ReplicatedCollisionType != SavedReplicatedCollisionType)
	{
		UBOOL bOldBlockActors = bBlockActors;
		UBOOL bOldCollideActors = bCollideActors;
		CollisionType = ReplicatedCollisionType;
		SetCollisionFromCollisionType();
		bBlockActors = bOldBlockActors;
		// see if we need to update touching actors
		if (bCollideActors == SavedCollision && bCollideActors != bOldCollideActors)
		{
			if (bCollideActors)
			{
				FindTouchingActors();
			}
			else
			{
				for (INT i = 0; i < Touching.Num(); )
				{
					if (Touching(i) != NULL)
					{
						Touching(i)->EndTouch(this, 0);
					}
					else
					{
						i++;
					}
				}
			}
		}
	}
	if( bCollideActors!=SavedCollision )
	{
		SetCollision( SavedCollision, bBlockActors, bIgnoreEncroachers );
	}
	PostNetReceiveLocation();
	if( Rotation!=SavedRotation )
	{
		FCheckResult Hit;
		GWorld->MoveActor( this, FVector(0,0,0), SavedRotation, MOVE_NoFail, Hit );
	}
	if ( DrawScale!=SavedDrawScale )
		SetDrawScale(SavedDrawScale);

	if (Physics != SavedPhysics)
	{
		Exchange(Physics, SavedPhysics);
		setPhysics(SavedPhysics);
	}

	PostNetReceiveBase(SavedBase);
}

void AActor::PostNetReceiveBase(AActor* NewBase)
{	
	UBOOL bBaseChanged = ( Base!=NewBase );
	if( bBaseChanged )
	{
		bHardAttach = SavedHardAttach;

		// Base changed.
		SetBase( NewBase );
	}
	else
	{
		// If the base didn't change, but the 'hard attach' flag did, re-base this actor.
		if(SavedHardAttach != bHardAttach)
		{
			bHardAttach = SavedHardAttach;

			SetBase( NULL );
			SetBase( NewBase );
		}
	}

	if ( Base && !Base->bWorldGeometry )
	{
		if ( bBaseChanged || (RelativeLocation != SavedRelativeLocation) )
		{
			GWorld->FarMoveActor( this, Base->Location + SavedRelativeLocation, 0, 1, 1 );
		}
		// Base could be lost by above FarMoveActor() if it touches something and triggers script, for example
		if (Base != NULL && (bBaseChanged || RelativeRotation != SavedRelativeRotation))
		{
			FCheckResult Hit;
			FRotator NewRotation = (FRotationMatrix(SavedRelativeRotation) * FRotationMatrix(Base->Rotation)).Rotator();
			GWorld->MoveActor( this, FVector(0,0,0), NewRotation, MOVE_NoFail, Hit);
			// MoveActor() won't automatically set RelativeRotation if we are based on a bone or a mover
			if (BaseBoneName != NAME_None || Physics == PHYS_Interpolating)
			{
				RelativeRotation = SavedRelativeRotation;
			}
		}
	}
	else
	{
		// Relative* are currently meaningless, but save them in case we simply haven't received Base yet
		RelativeLocation = SavedRelativeLocation;
		RelativeRotation = SavedRelativeRotation;
	}
	bJustTeleported = 0;
}

void AActor::PostNetReceiveLocation()
{
	if (Location != SavedLocation)
	{
		UBOOL bUpdateRBPosition = (Physics == PHYS_RigidBody && (Location - SavedLocation).SizeSquared() > UCONST_REP_RBLOCATION_ERROR_TOLERANCE_SQ);
		GWorld->FarMoveActor(this, SavedLocation, 0, 1, 1);
		if (bUpdateRBPosition)
		{
			// move rigid body components in physics system
			for (INT i = 0; i < Components.Num(); i++)
			{
				UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Components(i));
				if (Primitive != NULL)
				{
					USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Primitive);
					if(!SkelComp || (SkelComp->bSyncActorLocationToRootRigidBody && SkelComp->PhysicsAssetInstance != NULL))
					{
						Primitive->SetRBPosition(Primitive->LocalToWorld.GetOrigin());
					}
				}
			}
		}
	}
}

/** If TRUE, bypass simulated client physics, and run owned/server physics instead. Do not perform simulation/correction. */
UBOOL APawn::ShouldBypassSimulatedClientPhysics()
{
	return FALSE;
}

void APawn::PostNetReceiveLocation()
{
	if ( DrivenVehicle != SavedDrivenVehicle )
	{
		if ( DrivenVehicle )
		{
			if (SavedDrivenVehicle != NULL)
			{
				AVehicle* NewDrivenVehicle = DrivenVehicle;
				eventStopDriving(SavedDrivenVehicle);
				DrivenVehicle = NewDrivenVehicle;
			}

			eventStartDriving(DrivenVehicle);
			SavedBase = Base;
			SavedPhysics = Physics;
			SavedHardAttach = bHardAttach;
			SavedRotation = Rotation;
			SavedRelativeLocation = RelativeLocation;
			SavedRelativeRotation = RelativeRotation;
			return;
		}
		else
		{
			FVector NewLoc = SavedLocation;
			SavedLocation = Location;
			eventStopDriving(SavedDrivenVehicle);
			if ( Location == SavedLocation )
				SavedLocation = NewLoc;
		}
	}

	if ( Physics == PHYS_RigidBody )
	{
		AActor::PostNetReceiveLocation();
		return;
	}

	// Fire event when Pawn starts or stops crouching, to update collision and mesh offset.
	if( bIsCrouched != SavedbIsCrouched )
	{
		if( bIsCrouched )
		{
			if (Role == ROLE_SimulatedProxy)
			{
				// restore collision size if it was reduced by below code so it doesn't affect crouch height change
				APawn* DefaultPawn = (APawn*)(GetClass()->GetDefaultActor());
				if (DefaultPawn->CylinderComponent->CollisionRadius - CylinderComponent->CollisionRadius - 1.f < KINDA_SMALL_NUMBER)
				{
					SetCollisionSize(CylinderComponent->CollisionRadius + 1.f, CylinderComponent->CollisionHeight + 1.f);
				}
			}

			Crouch(1);
		}
		else
		{
			UnCrouch(1);
		}
	}

	// always consider Location as changed if we were spawned this tick as in that case our replicated Location was set as part of spawning, before PreNetReceive()
	if (Location == SavedLocation && CreationTime != WorldInfo->TimeSeconds)
	{
		return;
	}

	// If doing Root Motion, and animation drives physics (all but RMM_Velocity), then we want to skip position correction.
	// Instead we'll run physics as normal after animation has been ticked.
	if( ShouldBypassSimulatedClientPhysics() )
	{
		// Override replicated physics changes in that case. We're running our own version of perform physics.
		if( Physics != SavedPhysics )
		{
			Exchange(Physics, SavedPhysics);
			SavedPhysics = Physics;
		}

		return;
	}

	if( Role == ROLE_SimulatedProxy )
	{
		FCheckResult Hit(1.f);
		if ( GWorld->EncroachingWorldGeometry(Hit,SavedLocation+CollisionComponent->Translation,GetCylinderExtent(), FALSE, this) )
		{
			APawn* DefaultPawn = (APawn*)(GetClass()->GetDefaultActor());

			if ( CylinderComponent->CollisionRadius == DefaultPawn->CylinderComponent->CollisionRadius )
			{
				// slightly reduce cylinder size to compensate for replicated vector rounding errors
				SetCollisionSize(CylinderComponent->CollisionRadius - 1.f, CylinderComponent->CollisionHeight - 1.f);
			}
			bSimGravityDisabled = TRUE;
		}
		else if ( bIsCrouched || Velocity.IsZero() )
			bSimGravityDisabled = TRUE;
		else
		{
			SavedLocation.Z += 2.f;
			bSimGravityDisabled = FALSE;
		}
		FVector OldLocation = Location;
		GWorld->FarMoveActor( this, SavedLocation, 0, 1, 1 );

		SmoothCorrection(OldLocation);
	}
	else
		Super::PostNetReceiveLocation();
}

/**
  * Smooth out movement of other players on net clients
  * Default pawn implementation actually moves actor.
  * Recommended to implement by doing mesh translation (see UDKPawn for example implementation)
  * Mesh translation implementation depends on what other features (e.g. root motion and foot placement) are 
  * already performing mesh translation.
  */
void APawn::SmoothCorrection(const FVector& OldLocation)
{
		if ( !bSimGravityDisabled )
			{
			// smooth out movement of other players to account for frame rate induced jitter
			// look at whether location is a reasonable approximation already
			// if so only partially correct
			FVector Dir = OldLocation - Location;
			FLOAT StartError = Dir.Size();
			if ( StartError > 4.f )
			{
				moveSmooth(::Min(0.75f * StartError,CylinderComponent->CollisionRadius) * Dir.SafeNormal());
			}
		}
}

FLOAT AMatineeActor::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, FLOAT Time, UBOOL bLowBandwidth)
{
	FLOAT Result = Super::GetNetPriority(ViewPos, ViewDir, Viewer, InChannel, Time, bLowBandwidth);
	// attempt to replicate MatineeActors approximately in the order that they were spawned to reduce ordering issues
	// when LDs make multipler matinees affect the same target(s)
	// not great, but without a full depedancy setup this is the best we can do
	if (InChannel == NULL)
	{
		Result += 1.0f - CreationTime / WorldInfo->TimeSeconds;
	}
	return Result;
}

void AMatineeActor::PreNetReceive()
{
	Super::PreNetReceive();

	SavedInterpAction = InterpAction;

 	if (InterpAction != NULL)
 	{
 		SavedbIsPlaying = bIsPlaying;
 		SavedPosition = InterpAction->Position;
 	}
}

/** @hack: saves and restores fade state for a PC when it goes out of scope
 * used for fade track hack below */
struct FSavedFadeState
{
public:
	FSavedFadeState(ACamera* InCamera)
		: Camera(InCamera), bEnableFading(InCamera->bEnableFading), FadeAmount(InCamera->FadeAmount), FadeTimeRemaining(InCamera->FadeTimeRemaining)
	{}
	~FSavedFadeState()
	{
		Camera->bEnableFading = bEnableFading;
		Camera->FadeAmount = FadeAmount;
		Camera->FadeTimeRemaining = FadeTimeRemaining;
	}
private:
	ACamera* Camera;
	UBOOL bEnableFading;
	FLOAT FadeAmount;
	FLOAT FadeTimeRemaining;
};

void AMatineeActor::PostNetReceive()
{
	Super::PostNetReceive();

	if (InterpAction != NULL)
	{
		// if we just received the matinee action, set 'saved' values to default so we make sure to apply previously received values
		if (SavedInterpAction == NULL)
		{
			AMatineeActor* Default = GetClass()->GetDefaultObject<AMatineeActor>();
			SavedbIsPlaying = Default->bIsPlaying;
			SavedPosition = Default->Position;
		}
		// apply PlayRate
		InterpAction->PlayRate = PlayRate;

		// apply bReversePlayback
		if (InterpAction->bReversePlayback != bReversePlayback)
		{
			InterpAction->bReversePlayback = bReversePlayback;
			if (SavedbIsPlaying && bIsPlaying)
			{
				// notify actors that something has changed
				for (INT i = 0; i < InterpAction->LatentActors.Num(); i++)
				{
					if (InterpAction->LatentActors(i) != NULL)
					{
						InterpAction->LatentActors(i)->eventInterpolationChanged(InterpAction);
					}
				}
			}
		}
		// start up interpolation, if necessary
		if (!SavedbIsPlaying && (bIsPlaying || Position != SavedPosition))
		{	
			// will initialize, set this flag to be FALSE, so that it can initialize it
			AllAIGroupsInitialized = FALSE;		
			// clear all flags
			appMemzero(AIGroupInitStage, sizeof(INT)*UCONST_MAX_AIGROUP_NUMBER);

			InterpAction->InitInterp();

			// re-initialize AIGroup Actors for clients
			if (ClientInitializeAIGroupActors())
			{
				AllAIGroupsInitialized = TRUE;
			}

			// if we're playing forward, call Play() to process any special properties on InterpAction that may affect the meaning of 'Position' (bNoResetOnRewind, etc)
			if (!bReversePlayback)
			{
				InterpAction->Play();
			}

			// find affected actors and add InterpAction to their LatentActions list so they can find it in physInterpolating()
			// @warning: this code requires the linked actors to be static object references (i.e., some other Kismet action can't be assigning them)
			// this might not work for AI pawns
			TArray<UObject**> ObjectVars;
			InterpAction->GetObjectVars(ObjectVars);
			for (INT i = 0; i < ObjectVars.Num(); i++)
			{
				AActor* Actor = Cast<AActor>(*(ObjectVars(i)));
				UInterpGroupInst * GrInst = InterpAction->FindGroupInst(Actor);
				if (Actor != NULL && !Actor->bDeleteMe && GrInst != NULL) //@see: USeqAct_Interp::Activated()
				{
					Actor->LatentActions.AddItem(InterpAction);
					InterpAction->LatentActors.AddItem(Actor);
					// fire an event if we're really playing (and not just starting it up to do a position update)
					if (bIsPlaying)
					{
						// if actor has already been ticked, reupdate physics with updated position from track.
						// Fixes Matinee viewing through a camera at old position for 1 frame.
						Actor->performPhysics(1.f);
						Actor->eventInterpolationStarted(InterpAction, GrInst);
					}
				}
			}

			// go through all pawn in the list and initialize
			for ( INT I=0; I<UCONST_MAX_AIGROUP_NUMBER; ++I )
			{
				// at least initialized
				if ( AIGroupNames[I]!=NAME_None && AIGroupInitStage[I] == 1)
				{
					AActor * Actor = AIGroupPawns[I];
					check (Actor);

					// only handle AIGroup Inst, others are done
					UInterpGroupInst * GrInst = InterpAction->FindGroupInst(Actor);

					// only allow if AIGroup
					if (GrInst->IsA(UInterpGroupInstAI::StaticClass()) && 
						Actor != NULL && !Actor->bDeleteMe && GrInst != NULL) //@see: USeqAct_Interp::Activated()
					{
						Actor->LatentActions.AddItem(InterpAction);
						InterpAction->LatentActors.AddItem(Actor);
						// fire an event if we're really playing (and not just starting it up to do a position update)
						if (bIsPlaying)
						{
							// if actor has already been ticked, reupdate physics with updated position from track.
							// Fixes Matinee viewing through a camera at old position for 1 frame.
							Actor->performPhysics(1.f);
							Actor->eventInterpolationStarted(InterpAction, GrInst);
							AIGroupInitStage[I] = 2;
						}
					}
				}
			}
		}

		// if it's playing, but AI group isn't initialized yet
		// then try initialize
		if ( bIsPlaying && AllAIGroupsInitialized == FALSE )
		{
			if (ClientInitializeAIGroupActors())
			{
				AllAIGroupsInitialized = TRUE;
			}

			// still things to do
			if (AllAIGroupsInitialized == FALSE)
			{
 				//debugf(TEXT("Client Initialize AIGroup"));
 				// if we're playing forward, call Play() to process any special properties on InterpAction that may affect the meaning of 'Position' (bNoResetOnRewind, etc)
  				if (!bReversePlayback)
  				{
  					// I need AIgroup play only
  					InterpAction->Play(TRUE);
  				}

				// go through all pawn in the list and initialize
				for ( INT I=0; I<UCONST_MAX_AIGROUP_NUMBER; ++I )
				{
					// at least initialized
					if ( AIGroupNames[I]!=NAME_None && AIGroupInitStage[I] == 1)
					{
						AActor * Actor = AIGroupPawns[I];
						check (Actor);

						// only handle AIGroup Inst, others are done
						UInterpGroupInst * GrInst = InterpAction->FindGroupInst(Actor);

						// only allow if AIGroup
						if (GrInst->IsA(UInterpGroupInstAI::StaticClass()) && 
							Actor != NULL && !Actor->bDeleteMe && GrInst != NULL) //@see: USeqAct_Interp::Activated()
						{
//							debugf(TEXT("MatineeActor: Client (%s) is being initialized"), *Actor->GetName());
							Actor->LatentActions.AddItem(InterpAction);
							InterpAction->LatentActors.AddItem(Actor);
							// fire an event if we're really playing (and not just starting it up to do a position update)
							if (bIsPlaying)
							{
								// if actor has already been ticked, reupdate physics with updated position from track.
								// Fixes Matinee viewing through a camera at old position for 1 frame.
								Actor->performPhysics(1.f);
								Actor->eventInterpolationStarted(InterpAction, GrInst);
								AIGroupInitStage[I] = 2;
							}
						}
					}
				}
			}
		}
		// if we received a different current position
		if (Position != SavedPosition)
		{
			//@hack: negate fade tracks if we're updating a stopped matinee
			// the right fix is probably to pass bJump=TRUE to UpdateInterp() when (!bIsPlaying && !SavedbIsPlaying),
			// but that may have lots of other side effects I don't have time to test right now
			TArray<FSavedFadeState> SavedFadeStates;
			if (!bIsPlaying && !SavedbIsPlaying && InterpAction->InterpData != NULL)
			{
				for (FLocalPlayerIterator It(GEngine); It; ++It)
				{
					if (It->Actor != NULL && It->Actor->PlayerCamera != NULL)
					{
						new(SavedFadeStates) FSavedFadeState(It->Actor->PlayerCamera);
					}
				}
			}

			if (bIsPlaying && SavedPosition != -1 && Abs(Position - SavedPosition) < ClientSidePositionErrorTolerance)
			{
				// The error value between us and the server is too small to change gameplay, but will cause visual pops
				Position = SavedPosition;
			}
			else
			{
				// set to position replicated from server
				InterpAction->UpdateInterp(Position, false, false);
				// update interpolating actors for the new position
				for (INT i = 0; i < InterpAction->LatentActors.Num(); i++)
				{
					AActor *InterpActor = InterpAction->LatentActors(i);
					if (InterpActor != NULL && !InterpActor->IsPendingKill() && InterpActor->Physics == PHYS_Interpolating)
					{
						InterpAction->LatentActors(i)->physInterpolating(InterpActor->WorldInfo->DeltaSeconds);
					}
				}
			}
		}
		// terminate interpolation, if necessary
		if ((SavedbIsPlaying || Position != SavedPosition) && !bIsPlaying)
		{
			InterpAction->TermInterp();
			AllAIGroupsInitialized = FALSE;
			// clear all flags
			appMemzero(AIGroupInitStage, sizeof(INT)*UCONST_MAX_AIGROUP_NUMBER);
			// find affected actors and remove InterpAction from their LatentActions list
			while (InterpAction->LatentActors.Num() > 0)
			{
				AActor* Actor = InterpAction->LatentActors.Pop();
				if (Actor != NULL)
				{
					Actor->LatentActions.RemoveItem(InterpAction);
					// fire an event if we were really playing (and not just starting it up to do a position update)
					if (SavedbIsPlaying)
					{
						Actor->eventInterpolationFinished(InterpAction);
					}
				}
			}
		}
		InterpAction->bIsPlaying = bIsPlaying;
		InterpAction->bPaused = bPaused;
	}
}

/*-----------------------------------------------------------------------------
	APlayerController implementation.
-----------------------------------------------------------------------------*/

//
// Set the player.
//
void APlayerController::SetPlayer( UPlayer* InPlayer )
{
	check(InPlayer!=NULL);

	// Detach old player.
	if( InPlayer->Actor )
		InPlayer->Actor->Player = NULL;

	// Set the viewport.
	Player = InPlayer;
	InPlayer->Actor = this;

	// cap outgoing rate to max set by server
	UNetDriver* Driver = GWorld->GetNetDriver();
	if( (ClientCap>=2600) && Driver && Driver->ServerConnection )
	{
		Player->CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp( ClientCap, 1800, Driver->MaxClientRate );
	}

	// initialize the input system only if local player
	if ( Cast<ULocalPlayer>(InPlayer) )
	{
		eventInitInputSystem();
	}

	eventSpawnPlayerCamera();

	// notify script that we've been assigned a valid player
	eventReceivedPlayer();
}

/*-----------------------------------------------------------------------------
	AActor.
-----------------------------------------------------------------------------*/

void AActor::BeginDestroy()
{
	ClearComponents();
	Super::BeginDestroy();
}

UBOOL AActor::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && DetachFence.GetNumPendingFences() == 0;
}

void AActor::PostLoad()
{
	Super::PostLoad();

	// check for empty Attached entries
	for ( INT i=0; i<Attached.Num(); i++ )
	{
		if ( (Attached(i) == NULL) || (Attached(i)->Base != this) || Attached(i)->bDeleteMe )
		{
			Attached.Remove(i--);
		}
	}

	if (GIsGame && !IsTemplate())
	{
		// remove empty Components entries in game (from editor-only components and such)
		// don't remove from templates as this could break the order needed for proper loading with archetypes
		Components.RemoveItem(NULL);
	}

	// add ourselves to our Owner's Children array
	if (Owner != NULL)
	{
		checkSlow(!Owner->Children.ContainsItem(this));
		Owner->Children.AddItem(this);
	}

	SetDefaultCollisionType();

	if ( GIsEditor )
	{
		// Propagate the hidden at editor startup flag to the transient hidden flags
		bHiddenEdTemporary = bHiddenEd;
		bHiddenEdScene = bHiddenEd;

		// Check/warning when loading actors in editor. Should never load bDeleteMe Actors!
		if ( bDeleteMe )
		{
			debugf( TEXT("Loaded Actor (%s) with bDeleteMe == true"), *GetName() );
		}
	}

	if (GetLinker() && (GetLinker()->Ver() < VER_RENAMED_GROUPS_TO_LAYERS))
	{
		Layer = Group_DEPRECATED;
		bHiddenEdLayer = bHiddenEdGroup_DEPRECATED;
	}
}

void AActor::ProcessEvent( UFunction* Function, void* Parms, void* Result )
{
	if( ((GWorld && GWorld->HasBegunPlay()) || HasAnyFlags(RF_ClassDefaultObject)) && !GIsGarbageCollecting )
	{
		Super::ProcessEvent( Function, Parms, Result );
	}
}

/** information for handling Actors having their Base changed between PreEditChange() and PostEditChange() */
struct FBaseInfo
{
	AActor* Actor;
	AActor* Base;
	USkeletalMeshComponent* BaseSkelComponent;
	FName BaseBoneName;

	FBaseInfo(AActor* InActor)
	: Actor(InActor), Base(InActor->Base), BaseSkelComponent(InActor->BaseSkelComponent), BaseBoneName(InActor->BaseBoneName)
	{}
};
static TArray<FBaseInfo> BaseInfos;

/** hackish way to prevent LDs getting spammed with world geometry bBlockActors warnings when changing many actors at once */
static UBOOL bDisplayedWorldGeometryWarning = FALSE;

void AActor::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	ClearComponents();

	new(BaseInfos) FBaseInfo(this);

	bDisplayedWorldGeometryWarning = FALSE;
}

void AActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for ( INT i=0; i<Attached.Num(); i++ )
	{
		if ( Attached(i) == NULL )
		{
			Attached.Remove(i);
			i--;
		}
	}

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	// we only want CollisionType to modify the flags if the LD explicitly modified it
	if (PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("CollisionType")))
	{
		SetCollisionFromCollisionType();
		GCallbackEvent->Send( CALLBACK_RefreshPropertyWindows );
	}

	if (PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("Layer")))
	{
		// tell the world that the set of layers may have changed (and that this actor caused the change)
		GCallbackEvent->Send(CALLBACK_LayersHaveChanged, this);
	}


	if (CollisionComponent != NULL && (PropertyThatChanged == NULL || PropertyThatChanged->GetFName() == FName(TEXT("BlockRigidBody"))))
	{
		CollisionComponent->BlockRigidBody = BlockRigidBody;
		GCallbackEvent->Send( CALLBACK_RefreshPropertyWindows );
	}

	ForceUpdateComponents(FALSE,FALSE);

	// Now, because we need to correctly remove this Actor from its old Base's Attachments array, we have to pretend that nothing has changed yet
	// and call SetBase(NULL). We backed up the old base information in PreEditChange...
	for (INT i = 0; i < BaseInfos.Num(); i++)
	{
		// If this is the actor you are looking for
		if( BaseInfos(i).Actor == this )
		{
			// If this actor can't move AND new base can move - invalid base
			if( Base != NULL && (bStatic || !bMovable) && (!Base->bStatic && Base->bMovable) )
			{
				debugf(TEXT("PostEditChange 'Base' failed! Cannot base static actor %s on moveable actor %s - restoring previous base %s"), *GetName(), *Base->GetName(), BaseInfos(i).Base ? *BaseInfos(i).Base->GetName() : TEXT("NULL") );

				// Restore the old base
				SetBase( BaseInfos(i).Base );
				EditorUpdateBase();
			}
			else
			// Otherwise, if something has changed
			if( Base != BaseInfos(i).Base || BaseSkelComponent != BaseInfos(i).BaseSkelComponent || BaseBoneName != BaseInfos(i).BaseBoneName )
			{
				// Back up 'new data'
				FBaseInfo NewBaseInfo(this);

				// Restore 'old' base settings, and call SetBase(NULL) to cleanly detach and broadly attach to new Actor
				Base = BaseInfos(i).Base;
				BaseSkelComponent = BaseInfos(i).BaseSkelComponent;
				BaseBoneName = BaseInfos(i).BaseBoneName;
				SetBase(NewBaseInfo.Base);

				// Put in 'new' settings, and call EditorUpdateBase (which will find a SkeletalMeshComponent if necessary)
				// and update the Base fully.
				BaseSkelComponent = NewBaseInfo.BaseSkelComponent;
				BaseBoneName = NewBaseInfo.BaseBoneName;
				EditorUpdateBase();
			}
			BaseInfos.Remove(i);
			break;
		}
	}

	GWorld->bDoDelayedUpdateCullDistanceVolumes = TRUE;

	GCallbackEvent->Send( CALLBACK_UpdateUI );
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

// There might be a constraint to this actor, so we iterate over all actors to check. If there is, update ref frames.
void AActor::PostEditMove(UBOOL bFinished)
{
	if ( bFinished )
	{
		// propagate our movement
		GObjectPropagator->OnActorMove(this);

		if ( GIsEditor && !GIsPlayInEditorWorld )
		{
#if WITH_EDITOR
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(GetLevel());
#endif
			for ( INT Idx = 0 ; Idx < Components.Num() ; ++Idx )
			{
				// Look for a component on this actor that accepts decals.
				// Issue a decal update request if one is found.
				const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>( Components(Idx) );
				const ULightComponent* LightComponent = Cast<ULightComponent>( Components(Idx) );
				if( (PrimitiveComponent && 
					(PrimitiveComponent->bAcceptsDynamicDecals || PrimitiveComponent->bAcceptsStaticDecals)) ||
					LightComponent )
				{
					GEngine->IssueDecalUpdateRequest();
#if WITH_EDITOR
					if( !IFA )
#endif
					{
						break;
					}
				}

#if WITH_EDITOR
				if( IFA && Components(Idx) )
				{
					IFA->MoveInstancesForMovedComponent( Components(Idx) );
				}
#endif
			}
		}
		GWorld->bDoDelayedUpdateCullDistanceVolumes = TRUE;

		// The engine is going to update constraints
		GEngine->bAreConstraintsDirty = TRUE;

		GCallbackEvent->Send( CALLBACK_RefreshPropertyWindows );

		// Let other systems know that an actor was moved
		GCallbackEvent->Send( CALLBACK_OnActorMoved, this );

		GCallbackEvent->Send( CALLBACK_UpdateUI );
	}
	
	// Mark components as dirty so their rendering gets updated.
	MarkComponentsAsDirty();
}

#if WITH_EDITOR
/**
 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
 * The default implementation is simply to translate the actor's location.
 */
void AActor::EditorApplyTranslation(const FVector& DeltaTranslation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	Location += DeltaTranslation;
}

/**
 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
 * The default implementation is simply to modify the actor's rotation.
 */
void AActor::EditorApplyRotation(const FRotator& DeltaRotation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	FRotator ActorRotWind, ActorRotRem;
	this->Rotation.GetWindingAndRemainder(ActorRotWind, ActorRotRem);

	const FQuat ActorQ = ActorRotRem.Quaternion();
	const FQuat DeltaQ = DeltaRotation.Quaternion();
	const FQuat ResultQ = DeltaQ * ActorQ;

	const FRotator NewActorRotRem = FRotator( ResultQ );
	FRotator DeltaRot = NewActorRotRem - ActorRotRem;
	DeltaRot.MakeShortestRoute();

	this->Rotation += DeltaRot;
}

/**
 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
 * The default implementation is simply to modify the actor's draw scale.
 */
void AActor::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	DrawScale3D += ScaleMatrix.TransformFVector( DrawScale3D );

	if( PivotLocation )
	{
		Location -= *PivotLocation;
		Location += ScaleMatrix.TransformFVector( Location );
		Location += *PivotLocation;
	}

	GCallbackEvent->Send( CALLBACK_UpdateUI );
}
/**
* Called by MirrorActors to perform a mirroring operation on the actor
*/
void AActor::EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation)
{
	const FRotationMatrix TempRot( Rotation );
	const FVector New0( TempRot.GetAxis(0) * MirrorScale );
	const FVector New1( TempRot.GetAxis(1) * MirrorScale );
	const FVector New2( TempRot.GetAxis(2) * MirrorScale );
	// Revert the handedness of the rotation, but make up for it in the scaling.
	// Arbitrarily choose the X axis to remain fixed.
	const FMatrix NewRot( -New0, New1, New2, FVector(0,0,0) );

	DrawScale3D.X = -DrawScale3D.X;
	Rotation = NewRot.Rotator();
	Location -= PivotLocation - PrePivot;
	Location *= MirrorScale;
	Location += PivotLocation - PrePivot;
}
#endif

void AActor::SetDefaultCollisionType()
{
	// default to 'custom' (programmer set nonstandard settings)
	CollisionType = COLLIDE_CustomDefault;

	if (bCollideActors && CollisionComponent != NULL && CollisionComponent->CollideActors)
	{
		if (!bBlockActors || CollisionComponent->BlockActors)
		{
			if (CollisionComponent->BlockZeroExtent)
			{
				if (CollisionComponent->BlockNonZeroExtent)
				{
					CollisionType = (bBlockActors && CollisionComponent->BlockActors) ? COLLIDE_BlockAll : COLLIDE_TouchAll;
				}
				else
				{
					CollisionType = (bBlockActors && CollisionComponent->BlockActors) ? COLLIDE_BlockWeapons : COLLIDE_TouchWeapons;

					// See if we are COLLIDE_BlockWeaponsKickable
					if( CollisionType == COLLIDE_BlockWeapons && 
						CollisionComponent->BlockRigidBody && 
						CollisionComponent->RBChannel == RBCC_EffectPhysics )
					{
						CollisionType = COLLIDE_BlockWeaponsKickable;
					}
				}
			}
			else if (CollisionComponent->BlockNonZeroExtent)
			{
				CollisionType = (bBlockActors && CollisionComponent->BlockActors) ? COLLIDE_BlockAllButWeapons : COLLIDE_TouchAllButWeapons;
			}
		}
		// else (bBlockActors && !CollisionComponent->BlockActors), we're using some custom collision (e.g. only secondary collision component blocks)
	}
	else if (!bCollideActors && (!CollisionComponent || !CollisionComponent->BlockRigidBody))
	{
		CollisionType = COLLIDE_NoCollision;
	}

	// match mirrored BlockRigidBody flag
	if (CollisionComponent != NULL)
	{
		BlockRigidBody = CollisionComponent->BlockRigidBody;
	}

	// also make sure archetype CollisionType is set so that it only shows up bold in the property window if it has actually been changed
	AActor* TemplateActor = GetArchetype<AActor>();
	if (TemplateActor != NULL)
	{
		TemplateActor->SetDefaultCollisionType();
	}
}

/** Sets new collision type and updates the collision properties based on the type */
void AActor::SetCollisionType(BYTE NewCollisionType)
{
	UBOOL bOldCollideActors = bCollideActors;
	CollisionType = (ECollisionType)NewCollisionType;
	ReplicatedCollisionType = CollisionType;
	bNetDirty = TRUE;
	SetCollisionFromCollisionType();
	// see if we need to update touching actors
	if (bCollideActors != bOldCollideActors && GIsGame)
	{
		if (bCollideActors)
		{
			FindTouchingActors();
		}
		else
		{
			for (INT i = 0; i < Touching.Num(); )
			{
				if (Touching(i) != NULL)
				{
					Touching(i)->EndTouch(this, 0);
				}
				else
				{
					i++;
				}
			}
		}
	}
}

/** sets collision flags based on the current CollisionType */
void AActor::SetCollisionFromCollisionType()
{
	if (CollisionComponent != NULL)
	{
		// this is called from PostEditChange(), actor factories, etc that are calling Clear/UpdateComponents() themselves so we only reattach stuff that is currently attached
		TArray<UActorComponent*> PreviouslyAttachedComponents;
		for (INT i = 0; i < Components.Num(); i++)
		{
			if (Components(i) != NULL && Components(i)->IsAttached())
			{
				PreviouslyAttachedComponents.AddItem(Components(i));
				Components(i)->ConditionalDetach(TRUE);
			}
		}

		switch (CollisionType)
		{
			case COLLIDE_CustomDefault:
			{
					// restore to default programmer-defined settings
					AActor* DefaultActor = GetClass()->GetDefaultActor();
					bCollideActors = DefaultActor->bCollideActors;
					bBlockActors = DefaultActor->bBlockActors;
					if (DefaultActor->CollisionComponent != NULL)
					{
						CollisionComponent->CollideActors = DefaultActor->CollisionComponent->CollideActors;
						CollisionComponent->BlockActors = DefaultActor->CollisionComponent->BlockActors;
						CollisionComponent->BlockNonZeroExtent = DefaultActor->CollisionComponent->BlockNonZeroExtent;
						CollisionComponent->BlockZeroExtent = DefaultActor->CollisionComponent->BlockZeroExtent;
						CollisionComponent->SetBlockRigidBody(DefaultActor->CollisionComponent->BlockRigidBody);
					}
					else
					{
						debugf(NAME_Warning, TEXT("SetCollisionFromCollisionType(): class %s has no default CollisionComponent so %s's CollisionComponent cannot be reset to defaults."), *GetClass()->GetName(), *GetName());
					}
					break;
			}
			case COLLIDE_NoCollision:
				bCollideActors = FALSE;
				bBlockActors = FALSE;
				CollisionComponent->CollideActors = FALSE;
				CollisionComponent->SetBlockRigidBody(FALSE);
				break;
			case COLLIDE_BlockAll:
			case COLLIDE_BlockWeapons:
			case COLLIDE_BlockAllButWeapons:
			case COLLIDE_BlockWeaponsKickable:
				bCollideActors = TRUE;
				bBlockActors = TRUE;
				CollisionComponent->CollideActors = TRUE;
				CollisionComponent->BlockActors = TRUE;
				CollisionComponent->BlockNonZeroExtent = (CollisionType == COLLIDE_BlockAll || CollisionType == COLLIDE_BlockAllButWeapons);
				CollisionComponent->SetBlockRigidBody(CollisionComponent->BlockNonZeroExtent || (CollisionType == COLLIDE_BlockWeaponsKickable));
				CollisionComponent->BlockZeroExtent = (CollisionType == COLLIDE_BlockAll || CollisionType == COLLIDE_BlockWeapons || CollisionType == COLLIDE_BlockWeaponsKickable);
				if(CollisionType == COLLIDE_BlockWeaponsKickable)
				{
					CollisionComponent->SetRBChannel(RBCC_EffectPhysics);
				}
				break;
			case COLLIDE_TouchAll:
			case COLLIDE_TouchWeapons:
			case COLLIDE_TouchAllButWeapons:
				// bWorldGeometry actors must block if they collide at all, so force the flags to respect that even if the LD tries to change them
				if (bWorldGeometry)
				{
					if (!bDisplayedWorldGeometryWarning)
					{
						appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_WorldGeometryMustBlock"), *GetName()));
						bDisplayedWorldGeometryWarning = TRUE;
					}
					SetDefaultCollisionType();
				}
				else
				{
					bCollideActors = TRUE;
					bBlockActors = FALSE;
					CollisionComponent->CollideActors = TRUE;
					CollisionComponent->BlockActors = FALSE;
					CollisionComponent->SetBlockRigidBody(FALSE);
					CollisionComponent->BlockNonZeroExtent = (CollisionType == COLLIDE_TouchAll || CollisionType == COLLIDE_TouchAllButWeapons);
					CollisionComponent->BlockZeroExtent = (CollisionType == COLLIDE_TouchAll || CollisionType == COLLIDE_TouchWeapons);
				}
				break;
			default:
				debugf(NAME_Error, TEXT("%s set CollisionType to unknown value %i"), *GetFullName(), INT(CollisionType));
				bCollideActors = FALSE;
				break;
		}

		// mirror BlockRigidBody flag
		BlockRigidBody = CollisionComponent->BlockRigidBody;

		// reattach components that were previously attached
		const FMatrix& ActorToWorld = LocalToWorld();
		for (INT i = 0; i < PreviouslyAttachedComponents.Num(); i++)
		{
			if (!PreviouslyAttachedComponents(i)->IsAttached()) // might have been attached by a previous component in the list
			{
				PreviouslyAttachedComponents(i)->ConditionalAttach(GWorld->Scene, this, ActorToWorld);
			}
		}
	}
}

void AActor::Spawned()
{
	SetDefaultCollisionType();
}

//
// Get the collision cylinder extent to use when moving this actor through the level (ie. just looking at CollisionComponent)
//
FVector AActor::GetCylinderExtent() const
{
	UCylinderComponent* CylComp = Cast<UCylinderComponent>(CollisionComponent);
	
	if(CylComp)
	{
		return FVector(CylComp->CollisionRadius, CylComp->CollisionRadius, CylComp->CollisionHeight);
	}
	else
	{
		// use bounding cylinder
		FLOAT CollisionRadius, CollisionHeight;
		((AActor *)this)->GetBoundingCylinder(CollisionRadius, CollisionHeight);
		return FVector(CollisionRadius,CollisionRadius,CollisionHeight);
	}
}

//
// Get height/radius of big cylinder around this actors colliding components.
//
void AActor::GetBoundingCylinder(FLOAT& CollisionRadius, FLOAT& CollisionHeight) const
{
	FBox Box = GetComponentsBoundingBox();
	FVector BoxExtent = Box.GetExtent();

	CollisionHeight = BoxExtent.Z;
	CollisionRadius = appSqrt( (BoxExtent.X * BoxExtent.X) + (BoxExtent.Y * BoxExtent.Y) );
}

//
// Set the actor's collision properties.
//
void AActor::SetCollision(UBOOL bNewCollideActors, UBOOL bNewBlockActors, UBOOL bNewIgnoreEncroachers )
{
	// Make sure we're calling this function to change something.
	if( ( bCollideActors == bNewCollideActors )
		&& ( bBlockActors == bNewBlockActors )
		&& ( bIgnoreEncroachers == bNewIgnoreEncroachers )
		)
	{
		return;
	}

#if !FINAL_RELEASE
	// Check to see if this move is illegal during this tick group
	if( GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork )
	{
		debugf(NAME_Error,TEXT("Can't change collision on actor (%s) during async work!"),*GetName());
	}
#endif

	const UBOOL bOldCollideActors = bCollideActors;

	// Untouch everything if we're turning collision off.
	if( bCollideActors && !bNewCollideActors )
	{
		for( INT i=0; i<Touching.Num(); )
		{
			if( Touching(i) )
			{
				Touching(i)->EndTouch(this, 0);
			}
			else
			{
				i++;
			}
		}
	}

	// If the collide actors flag is changing, then all collidable components
	// need to be detached and then reattached
	UBOOL bClearAndUpdate = bCollideActors != bNewCollideActors;
	if (bClearAndUpdate)
	{
		// clear only primitive components so we don't needlessly reattach components that never collide
		for (INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Components(ComponentIndex));
			if (Primitive != NULL && Primitive->CollideActors)
			{
				Primitive->ConditionalDetach(TRUE);
			}
		}
	}
	// Set properties.
	bCollideActors = bNewCollideActors;
	bBlockActors   = bNewBlockActors;
	bIgnoreEncroachers = bNewIgnoreEncroachers;
	// Collision flags changed and collidable components need to be re-added
	if (bClearAndUpdate)
	{
		ConditionalUpdateComponents();
	}

	// Touch.
	if( bNewCollideActors && !bOldCollideActors )
	{
		FindTouchingActors();
	}
	// notify script
	eventCollisionChanged();
	// dirty this actor for replication
	bNetDirty = TRUE;
}

/**
* Used by the cooker to pre cache the convex data for static meshes within a given actor.  
* This data is stored with the level.
* @param Level - The level the cache is in
* @param TriByteCount - running total of memory usage for per-tri collision cache
* @param TriMeshCount - running count of per-tri collision cache
* @param HullByteCount - running total of memory usage for hull cache
* @param HullCount - running count of hull cache
*/
void AActor::BuildPhysStaticMeshCache(ULevel* Level,
									  INT& TriByteCount, INT& TriMeshCount, INT& HullByteCount, INT& HullCount)
{
	// Iterate over all components of that actor
	for(INT j=0; j<AllComponents.Num(); j++)
	{
		// If its a static mesh component, with a static mesh
		UActorComponent* Comp = AllComponents(j);
		UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Comp);
		if(SMComp && SMComp->StaticMesh)
		{
			// Overall scale factor for this mesh.
			FVector TotalScale3D = SMComp->Scale * DrawScale * SMComp->Scale3D * DrawScale3D;

			SMComp->CookPhysConvexDataForScale(Level, TotalScale3D, TriByteCount, TriMeshCount, HullByteCount, HullCount);
		}
		else
		{
			UApexStaticDestructibleComponent* ApexSDComp = Cast<UApexStaticDestructibleComponent>(Comp);
			if(ApexSDComp)
			{
				// Overall scale factor for this mesh.
				FVector TotalScale3D = DrawScale * DrawScale3D;

				ApexSDComp->CookPhysConvexDataForScale(Level, TotalScale3D, TriByteCount, TriMeshCount, HullByteCount, HullCount);
			}
		}
	}
}

/** UnTouchActors()
UnTouch actors which are no longer overlapping this actor
*/
void AActor::UnTouchActors()
{
	for( INT i=0; i<Touching.Num(); )
	{
		if( Touching(i) && !IsOverlapping(Touching(i)) )
		{
			EndTouch( Touching(i), 0 );
		}
		else
		{
			i++;
		}
	}
}

/** FindTouchingActors()
Touch all actors which are overlapping
and untouch actors we are no longer overlapping
*/
void AActor::FindTouchingActors()
{
	FMemMark Mark(GMainThreadMemStack);

	
	TLookupMap<AActor*> NewTouching;

	
	FLOAT ColRadius, ColHeight;
	GetBoundingCylinder(ColRadius, ColHeight);
	UBOOL bIsZeroExtent = (ColRadius == 0.f) && (ColHeight == 0.f);
	FCheckResult* FirstHit = GWorld->Hash ? GWorld->Hash->ActorEncroachmentCheck( GMainThreadMemStack, this, Location, Rotation, TRACE_AllColliding ) : NULL;	
	for( FCheckResult* Test = FirstHit; Test; Test=Test->GetNext() )
		if(	Test->Actor!=this && !Test->Actor->IsBasedOn(this) && Test->Actor != GWorld->GetWorldInfo() )
		{
			if( !IsBlockedBy(Test->Actor,Test->Component)
				&& (!Test->Component || (bIsZeroExtent ? Test->Component->BlockZeroExtent : Test->Component->BlockNonZeroExtent)) )
			{
				// Make sure Test->Location is not Zero, if that's the case, use Location
				FVector	HitLocation = Test->Location.IsZero() ? Location : Test->Location;

				// Make sure we have a valid Normal
				FVector NormalDir = Test->Normal.IsZero() ? (Location - HitLocation) : Test->Normal;
				if( !NormalDir.IsZero() )
				{
					NormalDir.Normalize();
				}
				else
				{
					NormalDir = FVector(0,0,1.f);
				}

				NewTouching.AddItem(Test->Actor);
				BeginTouch( Test->Actor, Test->Component, HitLocation, NormalDir, Test->SourceComponent );
			}
		}						

	for(INT Idx=0;Idx<Touching.Num();)
	{
		if(Touching(Idx) && !NewTouching.Find(Touching(Idx)))		
		{
			EndTouch(Touching(Idx),0);
		}
		else
		{
			Idx++;
		}
	}

	Mark.Pop();
}

//
// Set the collision cylinder size (if there is one).
//
void AActor::SetCollisionSize( FLOAT NewRadius, FLOAT NewHeight )
{
	UCylinderComponent* CylComp = Cast<UCylinderComponent>(CollisionComponent);

	if(CylComp)
		CylComp->SetCylinderSize(NewRadius, NewHeight);

	FindTouchingActors();
	// notify script
	eventCollisionChanged();
	// dirty this actor for replication
	bNetDirty = true;	
}

void AActor::SetDrawScale(FLOAT NewScale)
{
	if (DrawScale != NewScale)
	{
		DrawScale = NewScale;
		// mark components for an update
		MarkComponentsAsDirty();

		bNetDirty = true;	// for network replication
	}
}

void AActor::SetDrawScale3D( FVector NewScale3D )
{
	if (DrawScale3D != NewScale3D)
	{
		DrawScale3D = NewScale3D;
		// mark components for an update
		MarkComponentsAsDirty();

		// not replicated!
	}
}

/* Update relative rotation - called by ULevel::MoveActor()
 don't update RelativeRotation if attached to a bone -
 if attached to a bone, only update RelativeRotation directly
*/
void AActor::UpdateRelativeRotation()
{
	if ( !Base || Base->bWorldGeometry || (BaseBoneName != NAME_None) )
		return;

	// update RelativeRotation which is the rotation relative to the base's rotation
	RelativeRotation = (FRotationMatrix(Rotation) * FRotationMatrix(Base->Rotation).Transpose()).Rotator();
}

// Adds up the bounding box from each primitive component to give an aggregate bounding box for this actor.
FBox AActor::GetComponentsBoundingBox(UBOOL bNonColliding) const
{
	FBox Box(0);

	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)this->Components.Num();ComponentIndex++)
	{
		UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(this->Components(ComponentIndex));

		// Only use collidable components to find collision bounding box.
		if( primComp && primComp->IsAttached() && (bNonColliding || primComp->CollideActors) )
		{
			Box += primComp->Bounds.GetBox();
		}
	}

	return Box;
}

void AActor::GetComponentsBoundingBox(FBox& ActorBox) const
{
	ActorBox = GetComponentsBoundingBox();
}


/**
 * This will check to see if the Actor is still in the world.  It will check things like
 * the KillZ, SoftKillZ, outside world bounds, etc. and handle the situation.
 **/
void AActor::CheckStillInWorld()
{
	// check the variations of KillZ
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo( TRUE );

	if( Location.Z < ((WorldInfo->bSoftKillZ && Physics == PHYS_Falling) ? (WorldInfo->KillZ - WorldInfo->SoftKill) : WorldInfo->KillZ) )
	{
		eventFellOutOfWorld(WorldInfo->KillZDamageType);
	}
	// Check if box has poked outside the world
	else if( ( CollisionComponent != NULL ) && ( CollisionComponent->IsAttached() == TRUE ) )
	{
		const FBox&	Box = CollisionComponent->Bounds.GetBox();
		if(	Box.Min.X < -HALF_WORLD_MAX || Box.Max.X > HALF_WORLD_MAX ||
			Box.Min.Y < -HALF_WORLD_MAX || Box.Max.Y > HALF_WORLD_MAX ||
			Box.Min.Z < -HALF_WORLD_MAX || Box.Max.Z > HALF_WORLD_MAX )
		{
			debugf(NAME_Warning, TEXT("%s is outside the world bounds!"), *GetName());
			eventOutsideWorldBounds();
			// not safe to use physics or collision at this point
			SetCollision(FALSE, FALSE, bIgnoreEncroachers);
			setPhysics(PHYS_None);
		}
	}
}

FVector AActor::OverlapAdjust;

UBOOL CylCylIntersect(UCylinderComponent* CylComp1, UCylinderComponent* CylComp2, const FVector& OverlapAdjust, FCheckResult* Hit)
{
	// use OverlapAdjust because actor may have been temporarily moved
	FVector CylOrigin1 = CylComp1->GetOrigin() + OverlapAdjust;
	FVector CylOrigin2 = CylComp2->GetOrigin();

	if ( (Square(CylOrigin1.Z - CylOrigin2.Z) < Square(CylComp1->CollisionHeight + CylComp2->CollisionHeight))
		&&	(Square(CylOrigin1.X - CylOrigin2.X) + Square(CylOrigin1.Y - CylOrigin2.Y)
		< Square(CylComp1->CollisionRadius + CylComp2->CollisionRadius)) )
	{
		if( Hit )
		{
			Hit->Component = CylComp2;
		}
		return TRUE;
	}

	return FALSE;
}

extern UBOOL GShouldLogOutAFrameOfIsOverlapping;

#if PERF_SHOW_NONPAWN_VOLTRIG_OVERLAPS

struct FTrigVolOverlapInfo
{
	FString TrigVolPathName;
	FLOAT TotalOverlapTime;
	INT NumOverlaps;
};

TArray<FTrigVolOverlapInfo> OverlapInfos;

DOUBLE LastOutputTime = 0.f;

void AddOverlapInfo(AActor* VolTrig, FLOAT Time)
{
	FString PathName = VolTrig->GetPathName();
	for(INT i=0; i<OverlapInfos.Num(); i++)
	{
		if(OverlapInfos(i).TrigVolPathName == PathName)
		{
			OverlapInfos(i).TotalOverlapTime += Time;
			OverlapInfos(i).NumOverlaps++;
			return;
		}
	}
	
	INT NewInfoIndex = OverlapInfos.AddZeroed();
	OverlapInfos(NewInfoIndex).TrigVolPathName = PathName;
	OverlapInfos(NewInfoIndex).TotalOverlapTime = Time;
	OverlapInfos(NewInfoIndex).NumOverlaps = 1;
}

IMPLEMENT_COMPARE_CONSTREF(FTrigVolOverlapInfo, UnActor, { return A.TotalOverlapTime < B.TotalOverlapTime ? 1 : -1; } );

void DumpOverlapInfos()
{
	Sort<USE_COMPARE_CONSTREF(FTrigVolOverlapInfo, UnActor)>(&OverlapInfos(0), OverlapInfos.Num());

	for(INT i=0; i<OverlapInfos.Num(); i++)
	{
		FTrigVolOverlapInfo& Info = OverlapInfos(i);
		debugf(TEXT("* %s %f %d %f"), *Info.TrigVolPathName, Info.TotalOverlapTime, Info.NumOverlaps, Info.TotalOverlapTime/Info.NumOverlaps);
	}
}

#endif

//
// Return whether this actor overlaps another.
// Called normally from MoveActor, to see if we should 'untouch' things.
// Normally - the only things that can overlap an actor are volumes.
// However, we also use this test during ActorEncroachmentCheck, so we support
// Encroachers (ie. movers) overlapping actors.
//
UBOOL AActor::IsOverlapping( AActor* Other, FCheckResult* Hit, UPrimitiveComponent* OtherPrimitiveComponent, UPrimitiveComponent* MyPrimitiveComponent )
{
	checkSlow(Other!=NULL);


#if !FINAL_RELEASE
	if(GShouldLogOutAFrameOfIsOverlapping)
	{
		debugf(TEXT("ISOVERLAPPING: %s/%s"), *GetName(), *Other->GetName());
	}
#endif

	if ( (this->IsBrush() && Other->IsBrush()) || (Other == GWorld->GetWorldInfo()) )
	{
		// We cannot detect whether these actors are overlapping so we say they aren't.
		return 0;
	}

	// Things dont overlap themselves
	if(this == Other)
	{
		return 0;
	}

	// Things that do encroaching (movers, rigid body actors etc.) can't encroach each other!
	if(this->IsEncroacher() && Other->IsEncroacher())
	{
		return 0;
	}

	// Things that are joined together dont overlap.
	if( this->IsBasedOn(Other) || Other->IsBasedOn(this) )
	{
		return 0;
	}

	// Can't overlap actors with collision turned off.
	if( !this->bCollideActors || !Other->bCollideActors )
	{
		return 0;
	}

	// Now have 2 actors we want to overlap, so we have to pick which one to treat as a box and test against the PrimitiveComponents of the other.
	AActor* PrimitiveActor = NULL;
	AActor* BoxActor = NULL;

	// If we were not supplied an FCheckResult, use a temp one.
	FCheckResult TestHit;
	if(Hit==NULL)
	{
		Hit = &TestHit;
	}

	// For volumes, test the bounding box against the volume primitive.
	// in the volume case, we cannot test per-poly
	UBOOL bForceSimpleCollision = FALSE;
	if(this->GetAVolume())
	{
		PrimitiveActor = this;
		BoxActor = Other;
		bForceSimpleCollision = TRUE;
	}
	else if(Other->GetAVolume())
	{
		PrimitiveActor = Other;
		BoxActor = this;
		bForceSimpleCollision = TRUE;
	}
	// For fluid surfaces, test the bounding box against the fluid.
	else if(this->IsAFluidSurface())
	{
		PrimitiveActor = this;
		BoxActor = Other;
	}
	else if(Other->IsAFluidSurface())
	{
		PrimitiveActor = Other;
		BoxActor = this;
	}
	// For Encroachers, we test the complex primitive of the mover against the bounding box of the other thing.
	else if(this->IsEncroacher())
	{
		PrimitiveActor = this;
		BoxActor = Other;	
	}
	else if(Other->IsEncroacher())
	{
		PrimitiveActor = Other;	
		BoxActor = this;
	}
	// if we've been provided with both components to test, early out and just check those two components
	else if( OtherPrimitiveComponent != NULL && OtherPrimitiveComponent->CollideActors &&
			MyPrimitiveComponent != NULL && MyPrimitiveComponent->CollideActors)
	{
		DWORD MyTraceFlags=(bCollideComplex)? TRACE_ComplexCollision : 0;
		if(MyPrimitiveComponent != CollisionComponent)
		{
			MyPrimitiveComponent->OverrideTraceFlagsForNonCollisionComponentChecks(MyTraceFlags);
		}
		
		DWORD OtherTraceFlags=(OtherPrimitiveComponent->GetOwner()->bCollideComplex)? TRACE_ComplexCollision : 0;
		if(OtherPrimitiveComponent != Other->CollisionComponent)
		{
			OtherPrimitiveComponent->OverrideTraceFlagsForNonCollisionComponentChecks(OtherTraceFlags);
		}
		

		return MyPrimitiveComponent->IsOverlapping(OtherPrimitiveComponent,Hit,OverlapAdjust,MyTraceFlags,OtherTraceFlags);
	}
	// If none of these cases, test all colliding components against one another to check for overlap
	else
	{
		// first try collision components against each other
		if(CollisionComponent && Other->CollisionComponent)
		{
			UCylinderComponent* CylComp1 = Cast<UCylinderComponent>(CollisionComponent);
			UCylinderComponent* CylComp2 = Cast<UCylinderComponent>(Other->CollisionComponent);
			if(CylComp1 && CylComp2 && CylCylIntersect(CylComp1,CylComp2,OverlapAdjust,Hit))
			{
				return TRUE;
			}
		}


		// for each of my components that needs collision testing
		for(UINT MyComponentIndex = 0;MyComponentIndex < (UINT)Components.Num();MyComponentIndex++)
		{
			UPrimitiveComponent*	MyPrimComp = Cast<UPrimitiveComponent>(Components(MyComponentIndex));

			if( MyPrimComp != NULL && MyPrimComp->IsAttached() && MyPrimComp->CollideActors && MyPrimComp->BlockNonZeroExtent) 
			{
				// cache off version that is a cylinder comp, so if they're both cylinder comps we can do a true cyl-cyl collision
				UCylinderComponent* CylComp1 = Cast<UCylinderComponent>(MyPrimComp);

				// for each component on other who needs collision testing
				for(UINT OtherComponentIndex = 0;OtherComponentIndex < (UINT)Other->Components.Num();OtherComponentIndex++)
				{
					UPrimitiveComponent*	OtherPrimComponent = Cast<UPrimitiveComponent>(Other->Components(OtherComponentIndex));
					
					if( OtherPrimComponent != NULL && OtherPrimComponent->IsAttached() && OtherPrimComponent->CollideActors && OtherPrimComponent->BlockNonZeroExtent )
					{

						// cache off version that is a cylinder comp, so if they're both cylinder comps we can do a true cyl-cyl collision
						UCylinderComponent* CylComp2 = Cast<UCylinderComponent>(OtherPrimComponent);

						if(CylComp1 && CylComp2)
						{
							// skip testing collision cylinders which are the the Actor's CollisionComponents against each other, because we already have!
							if(CollisionComponent == CylComp1 && Other->CollisionComponent == CylComp2)
							{
								continue;
							}
							if(CylCylIntersect(CylComp1,CylComp2,OverlapAdjust,Hit)==TRUE)
							{
								return TRUE;
							}
						}
						else
						{
							DWORD MyTraceFlags=(bCollideComplex)? TRACE_ComplexCollision : 0;
							if(MyPrimComp != CollisionComponent)
							{
								MyPrimComp->OverrideTraceFlagsForNonCollisionComponentChecks(MyTraceFlags);
							}
							
							DWORD OtherTraceFlags=(Other->bCollideComplex)? TRACE_ComplexCollision : 0;
							if(OtherPrimComponent != Other->CollisionComponent)
							{
								OtherPrimComponent->OverrideTraceFlagsForNonCollisionComponentChecks(OtherTraceFlags);
							}
							
							
							if(MyPrimComp->IsOverlapping(OtherPrimComponent,Hit,OverlapAdjust,MyTraceFlags,OtherTraceFlags))
							{
								return TRUE;
							}
						}
					}
				}
			}
		}
		
		return FALSE;		
	}

	check(BoxActor);
	check(PrimitiveActor);
	check(BoxActor != PrimitiveActor);

	if(!BoxActor->CollisionComponent)
		return 0;
		
	// Check bounding box of collision component against each colliding PrimitiveComponent.
	FBox BoxActorBox = BoxActor->CollisionComponent->Bounds.GetBox();
	if(BoxActorBox.IsValid)
	{
		// adjust box position since overlap check is for different location than actor's current location
		if ( BoxActor == this )
		{
			BoxActorBox.Min += OverlapAdjust;
			BoxActorBox.Max += OverlapAdjust;
		}
		else
		{
			BoxActorBox.Min -= OverlapAdjust;
			BoxActorBox.Max -= OverlapAdjust;
		}
	}

	//GWorld->LineBatcher->DrawWireBox( BoxActorBox, FColor(255,255,0) );

	// If we failed to get a valid bounding box from the Box actor, we can't get an overlap.
	if(!BoxActorBox.IsValid)
	{
		return 0;
	}

	FVector BoxCenter, BoxExtent;
	BoxActorBox.GetCenterAndExtents(BoxCenter, BoxExtent);

	// DEBUGGING: Time how long the point checks take, and print if its more than PERF_SHOW_SLOW_OVERLAPS_TAKING_LONG_TIME_AMOUNT.
#if defined(PERF_SHOW_SLOW_OVERLAPS) || LOOKING_FOR_PERF_ISSUES || PERF_SHOW_NONPAWN_VOLTRIG_OVERLAPS
	DWORD Time=0;
	CLOCK_CYCLES(Time);
#endif

	UPrimitiveComponent* HitComponent = NULL;

	// Only check against passed in component if passed in.
	if (OtherPrimitiveComponent != NULL && PrimitiveActor == Other)
	{
		if( OtherPrimitiveComponent->CollideActors && OtherPrimitiveComponent->BlockNonZeroExtent )
		{
			if( OtherPrimitiveComponent->PointCheck(*Hit, BoxCenter, BoxExtent, (BoxActor->bCollideComplex && !bForceSimpleCollision) ? TRACE_ComplexCollision : 0) == 0 )
			{
				HitComponent = OtherPrimitiveComponent;
			}
		}
	}
	// Check box against each colliding primitive component.
	else
	{
		for(UINT ComponentIndex = 0; ComponentIndex < (UINT)PrimitiveActor->Components.Num(); ComponentIndex++)
		{
			UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(PrimitiveActor->Components(ComponentIndex));
			if(PrimComp && PrimComp->CollideActors && PrimComp->BlockNonZeroExtent)
			{
				if( PrimComp->PointCheck(*Hit, BoxCenter, BoxExtent, (BoxActor->bCollideComplex && !bForceSimpleCollision) ? TRACE_ComplexCollision : 0) == 0 )
				{
					HitComponent = PrimComp;
					break;
				}
			}
		}
	}

	Hit->Component = HitComponent;

#if defined(PERF_SHOW_SLOW_OVERLAPS) || LOOKING_FOR_PERF_ISSUES || PERF_SHOW_NONPAWN_VOLTRIG_OVERLAPS
	UNCLOCK_CYCLES(Time);
	FLOAT MSec = Time * GSecondsPerCycle * 1000.f;
#endif

#if PERF_SHOW_NONPAWN_VOLTRIG_OVERLAPS
	UBOOL bHavePawn = this->IsA(APawn::StaticClass()) || Other->IsA(APawn::StaticClass());
	UBOOL bHaveTrigger = this->IsA(ATrigger::StaticClass()) || Other->IsA(ATrigger::StaticClass());
	UBOOL bHaveVolume = this->IsA(AVolume::StaticClass()) || Other->IsA(AVolume::StaticClass());
	if(!bHavePawn && (bHaveTrigger || bHaveVolume))
	{
		AddOverlapInfo(Other, MSec);
		//debugf(TEXT("NON-PAWN vs TRIGGER/VOLUME OVERLAP: %s - %s (%f ms)"), *GetPathName(), *Other->GetPathName(), MSec);
	}

	if(appSeconds() - LastOutputTime > 10.f)
	{
		DumpOverlapInfos();
		LastOutputTime = appSeconds();
	}
#endif

#if defined(PERF_SHOW_SLOW_OVERLAPS) || LOOKING_FOR_PERF_ISSUES
	if( MSec > PERF_SHOW_SLOW_OVERLAPS_TAKING_LONG_TIME_AMOUNT )
	{
		debugf( NAME_PerfWarning, TEXT("IsOverLapping: Testing: P:%s - B:%s Time: %f"), *(PrimitiveActor->GetPathName()), *(BoxActor->GetPathName()), MSec );
	}
#endif

	return HitComponent != NULL;

}

/**
 * Sets the ticking group for this actor.
 *
 * @param NewTickGroup the new value to assign
 */
void AActor::SetTickGroup(BYTE NewTickGroup)
{
	check((NewTickGroup == TG_EffectsUpdateWork) ? this->IsA(AEmitter::StaticClass()) : 1);
	TickGroup = NewTickGroup;
}

/** turns on or off this Actor's desire to be ticked (bTickIsDisabled)
 * because this is implemented as a separate tickable list, calls to this function
 * to disable ticking will not take effect until the end of the current list to avoid shuffling
 * elements around while they are being iterated over
 */
void AActor::SetTickIsDisabled(UBOOL bInDisabled)
{
	if (bInDisabled != bTickIsDisabled && !bDeleteMe && !bStatic)
	{
		ULevel* Level = GetLevel();
		if (bInDisabled)
		{
			Level->PendingUntickableActors.AddItem(this);
		}
		// if it was in the list to remove, it's already in the TickableActors list, so don't add it again
		else if (Level->PendingUntickableActors.RemoveItem(this) <= 0)
		{
			Level->TickableActors.AddItem(this);
			// if we had already been ticked this frame yet still got here, this function would have had to have been already called with TRUE this frame
			// and thus we would have been in the PendingUntickableActors list, preventing us from reaching this code
			// therefore, we can assume we haven't been ticked, so make sure the flag indicates this
			bTicked = !GWorld->Ticked;
		}
		bTickIsDisabled = bInDisabled;
	}
}


/*-----------------------------------------------------------------------------
	Actor touch minions.
-----------------------------------------------------------------------------*/

static UBOOL TouchTo( AActor* Actor, AActor* Other, UPrimitiveComponent* OtherComp, const FVector &HitLocation, const FVector &HitNormal)
{
	check(Actor);
	check(Other);
	check(Actor!=Other);

	// if already touching, then don't bother with further checks
	if (Actor->Touching.ContainsItem(Other))
	{
		return 1;
	}

	// check for touch sequence events
	if (GIsGame)
	{
		for (INT Idx = 0; Idx < Actor->GeneratedEvents.Num(); Idx++)
		{
			USeqEvent_Touch *TouchEvent = Cast<USeqEvent_Touch>(Actor->GeneratedEvents(Idx));
			if (TouchEvent != NULL)
			{
				TouchEvent->CheckTouchActivate(Actor,Other);
			}
		}
	}

	// Make Actor touch TouchActor.
	Actor->Touching.AddItem(Other);
	Actor->eventTouch( Other, OtherComp, HitLocation, HitNormal );

	// See if first actor did something that caused an UnTouch.
	INT i = 0;
	return ( Actor->Touching.FindItem(Other,i) );

}

//
// Note that TouchActor has begun touching Actor.
//
// This routine is reflexive.
//
// Handles the case of the first-notified actor changing its touch status.
//
void AActor::BeginTouch( AActor* Other, UPrimitiveComponent* OtherComp, const FVector &HitLocation, const FVector &HitNormal, UPrimitiveComponent* MyComp)
{
	// Perform reflective touch.
	if ( TouchTo( this, Other, OtherComp, HitLocation, HitNormal ) )
		TouchTo( Other, this, (MyComp) ? MyComp : this->CollisionComponent, HitLocation, HitNormal );

}

//
// Note that TouchActor is no longer touching Actor.
//
// If NoNotifyActor is specified, Actor is not notified but
// TouchActor is (this happens during actor destruction).
//
void AActor::EndTouch( AActor* Other, UBOOL bNoNotifySelf )
{
	check(Other!=this);

	// Notify Actor.
	INT i=0;
	if ( !bNoNotifySelf && Touching.FindItem(Other,i) )
	{
		eventUnTouch( Other );
	}
	Touching.RemoveItem(Other);

	// check for untouch sequence events on both actors
	if (GIsGame)
	{
		USeqEvent_Touch *TouchEvent = NULL;
		for (INT Idx = 0; Idx < GeneratedEvents.Num(); Idx++)
		{
			TouchEvent = Cast<USeqEvent_Touch>(GeneratedEvents(Idx));
			if (TouchEvent != NULL)
			{
				TouchEvent->CheckUnTouchActivate(this,Other);
			}
		}
		for (INT Idx = 0; Idx < Other->GeneratedEvents.Num(); Idx++)
		{
			TouchEvent = Cast<USeqEvent_Touch>(Other->GeneratedEvents(Idx));
			if (TouchEvent != NULL)
			{
				TouchEvent->CheckUnTouchActivate(Other,this);
			}
		}
	}

	if ( Other->Touching.FindItem(this,i) )
	{
		Other->eventUnTouch( this );
		Other->Touching.RemoveItem(this);
	}
}

/*-----------------------------------------------------------------------------
	Relations.
-----------------------------------------------------------------------------*/

#if WITH_EDITOR
/**
 * @return		TRUE if the actor is in the named layer, FALSE otherwise.
 */
UBOOL AActor::IsInLayer(const TCHAR* LayerName) const
{
	// Take the actor's layer string and break it up into an array.
	TArray<FString> LayerList;
	GetLayers( LayerList );

	// Iterate over the array of layer names searching for the input layer.
	for( INT LayerIndex = 0 ; LayerIndex < LayerList.Num() ; ++LayerIndex )
	{
		if( LayerList( LayerIndex ) == LayerName )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Parses the actor's layer string into a list of layer names (strings).
 * @param		OutLayers		[out] Receives the list of layer names.
 */
void AActor::GetLayers(TArray<FString>& OutLayers) const
{
	OutLayers.Empty();
	Layer.ToString().ParseIntoArray( &OutLayers, TEXT(","), FALSE );
}
#endif

/** marks all PrimitiveComponents for which their Owner is relevant for visibility as dirty because the Owner of some Actor in the chain has changed
 * @param TheActor the actor to mark components dirty for
 * @param bProcessChildren whether to recursively iterate over children
 */
static void MarkOwnerRelevantComponentsDirty(AActor* TheActor)
{
	for (INT i = 0; i < TheActor->AllComponents.Num(); i++)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(TheActor->AllComponents(i));
		if (Primitive != NULL && (TheActor->bOnlyOwnerSee || Primitive->bOnlyOwnerSee || Primitive->bOwnerNoSee))
		{
			Primitive->BeginDeferredReattach();
		}
	}

	// recurse over children of this Actor
	for (INT i = 0; i < TheActor->Children.Num(); i++)
	{
		AActor* Child = TheActor->Children(i);
		if (Child != NULL && !Child->ActorIsPendingKill())
		{
			MarkOwnerRelevantComponentsDirty(Child);
		}
	}
}

//
// Change the actor's owner.
//
void AActor::SetOwner( AActor *NewOwner )
{
	if (Owner != NewOwner && !ActorIsPendingKill())
	{
		if (NewOwner != NULL && NewOwner->IsOwnedBy(this))
		{
			debugf(NAME_Error, TEXT("SetOwner(): Failed to set '%s' owner of '%s' because it would cause an Owner loop"), *NewOwner->GetName(), *GetName());
			return;
		}

		// Sets this actor's parent to the specified actor.
		AActor* OldOwner = Owner;
		if( Owner != NULL )
		{
			Owner->eventLostChild( this );
			if (OldOwner != Owner)
			{
				// LostChild() resulted in another SetOwner()
				return;
			}
			// remove from old owner's Children array
			verifySlow(Owner->Children.RemoveItem(this) == 1);
		}

		Owner = NewOwner;

		if( Owner != NULL )
		{
			// add to new owner's Children array
			checkSlow(!Owner->Children.ContainsItem(this));
			Owner->Children.AddItem(this);

			Owner->eventGainedChild( this );
			if (Owner != NewOwner)
			{
				// GainedChild() resulted in another SetOwner()
				return;
			}
		}

		// mark all components for which Owner is relevant for visibility to be updated
		MarkOwnerRelevantComponentsDirty(this);

		bNetDirty = TRUE;
	}
}

/** changes the value of bOnlyOwnerSee
 * @param bNewOnlyOwnerSee the new value to assign to bOnlyOwnerSee
 */
void AActor::SetOnlyOwnerSee(UBOOL bNewOnlyOwnerSee)
{
	bOnlyOwnerSee = bNewOnlyOwnerSee;
	MarkComponentsAsDirty(FALSE);
}

/**
 *	Change the actor's base.
 *	If you are a attaching to a SkeletalMeshComponent that is using another SkeletalMeshComponent for its bone transforms (via the ParentAnimComponent pointer)
 *	you should base the Actor on that component instead.
 *	AttachName is checks for a socket w/ that name first otherwise uses it as a direct bone name
 */
void AActor::SetBase( AActor* NewBase, FVector NewFloor, INT bNotifyActor, USkeletalMeshComponent* SkelComp, FName AttachName )
{
	/** 
	 * If we are based on a StaticMeshCollectionActor, then use the world info instead, as that can be replicated.
	 * Otherwise non owning clients on consoles find themselves with NULL bases. :(
	 * If this is causing some issues, let laurent.delayen@epicgames.com know!
	 */
	if( NewBase && NewBase->IsA(AStaticMeshCollectionActor::StaticClass()) )
	{
		NewBase = GWorld->GetWorldInfo();
	}

	// Verify no recursion.
	for( AActor* Loop=NewBase; Loop!=NULL; Loop=Loop->Base )
	{
		if( Loop == this )
		{
			debugf(TEXT(" SetBase failed! Recursion detected. Actor %s already based on %s."), *GetName(), *NewBase->GetName());
			return;
		}
	}

	// Don't allow static actors to be based on movable or deleted actors
	if( NewBase != NULL && (bStatic || !bMovable) && ((!NewBase->bStatic && NewBase->bMovable) || NewBase->bDeleteMe) )
	{
		debugf(TEXT("SetBase failed! Cannot base static actor %s on moveable actor %s"), *GetName(), *NewBase->GetName());
		return;
	}
	// verify SkeletalMeshComponent is attached to the base passed in
	if (SkelComp != NULL && SkelComp->GetOwner() != NewBase)
	{
		debugf(NAME_Warning, TEXT("Failed to set base of %s because SkelComp (%s) is not owned by NewBase (%s)!"), *GetName(), *SkelComp->GetName(), *NewBase->GetName());
		return;
	}

	// If anything is different from current base, update the based information.
	if( (NewBase != Base) || (SkelComp != BaseSkelComponent) || (AttachName != BaseBoneName) )
	{
		//debugf(TEXT("%3.2f SetBase %s -> %s, SkelComp: %s, AttachName: %s"), GWorld->GetTimeSeconds(), *GetName(), NewBase ? *NewBase->GetName() : TEXT("NULL"), *SkelComp->GetName(), *AttachName.ToString());

		// Notify old base, unless it's the level or terrain (but not movers).
		if( Base && !Base->bWorldGeometry )
		{
			INT RemovalCount = Base->Attached.RemoveItem(this);
			//@fixme - disabled check for editor since it was being triggered during import, is this safe?
			checkf(!GIsGame || RemovalCount <= 1, TEXT("%s was in Attached array of %s multiple times!"), *GetFullName(), *Base->GetFullName()); // Verify that an actor wasn't attached multiple times. @todo: might want to also check > 0?
			Base->eventDetach( this );
		}

		// Set base.
		Base = NewBase;
		BaseSkelComponent = NULL;
		BaseBoneName = NAME_None;

		if ( Base && !Base->bWorldGeometry )
		{
			if ( !bHardAttach || (Role == ROLE_Authority) )
			{
				RelativeLocation = Location - Base->Location;
				UpdateRelativeRotation();
			}

			// If skeletal case, check bone is valid before we try and attach.
			INT BoneIndex = INDEX_NONE;

			// Check to see if it is a socket first
			USkeletalMeshSocket* Socket = (SkelComp && SkelComp->SkeletalMesh) ? SkelComp->SkeletalMesh->FindSocket( AttachName ) : NULL;
			if( Socket )
			{
				// Use socket bone name
				AttachName = Socket->BoneName;
			}

			if( SkelComp && (AttachName != NAME_None) )
			{
				// Check we are not trying to base on a bone of a SkeletalMeshComponent that is 
				// using another SkeletalMeshComponent for its bone transforms.
				if(SkelComp->ParentAnimComponent)
				{
					debugf( 
						TEXT("SkeletalMeshComponent %s in Actor %s has a ParentAnimComponent - should attach Actor %s to that instead."), 
						*SkelComp->GetPathName(),
						*Base->GetPathName(),
						*GetPathName()
						);
				}
				else
				{
					BoneIndex = SkelComp->MatchRefBone(AttachName);
					if(BoneIndex == INDEX_NONE)
					{
						debugf( TEXT("AActor::SetBase : Bone (%s) not found on %s for %s!"), *AttachName.ToString() , *Base->GetName(), *GetName());					
					}
				}
			}

			// Bone exists and component is successfully initialized, so remember offset from it.
			if(BoneIndex != INDEX_NONE && SkelComp->IsAttached() && SkelComp->GetOwner() != NULL)
			{				
				if( Socket )
				{
					RelativeLocation = Socket->RelativeLocation;
					RelativeRotation = Socket->RelativeRotation;
				}
				else
				{
					// Get transform of bone we wish to attach to.
					FMatrix BaseTM = SkelComp->GetBoneMatrix(BoneIndex);
					BaseTM.RemoveScaling();
					FMatrix BaseInvTM = BaseTM.InverseSafe();

					FRotationTranslationMatrix ChildTM(Rotation,Location);

					// Find relative transform of actor from its base bone, and store it.
					FMatrix HardRelMatrix =  ChildTM * BaseInvTM;
					RelativeLocation = HardRelMatrix.GetOrigin();
					RelativeRotation = HardRelMatrix.Rotator();
				}

				BaseSkelComponent = SkelComp;
				BaseBoneName = AttachName;
			}
			// Calculate the transform of this actor relative to its base. When it's a simulated proxy, the relative location and rotation are replicated over.
			else if(bHardAttach && Role != ROLE_SimulatedProxy)
			{
				FMatrix BaseInvTM = FTranslationMatrix(-Base->Location) * FInverseRotationMatrix(Base->Rotation);
				FRotationTranslationMatrix ChildTM(Rotation, Location);

				FMatrix HardRelMatrix =  ChildTM * BaseInvTM;
				RelativeLocation = HardRelMatrix.GetOrigin();
				RelativeRotation = HardRelMatrix.Rotator();
			}
		}

		// Notify new base, unless it's the level.
		if( Base && !Base->bWorldGeometry )
		{
			Base->Attached.AddItem(this);
			Base->eventAttach( this );
		}

		// Notify this actor of his new floor.
		if ( bNotifyActor )
		{
			eventBaseChange();
		}
	}
}

//
// Determine if BlockingActor should block actors of the given class.
// This routine needs to be reflexive or else it will create funky
// results, i.e. A->IsBlockedBy(B) <-> B->IsBlockedBy(A).
//
UBOOL AActor::IsBlockedBy( const AActor* Other, const UPrimitiveComponent* Primitive ) const
{
	checkSlow(this!=NULL);
	checkSlow(Other!=NULL);

	if(Primitive && !Primitive->BlockActors)
		return FALSE;

	if( Other->bWorldGeometry )
		return bCollideWorld && Other->bBlockActors;
	else if ( Other->IgnoreBlockingBy((AActor *)this) || IgnoreBlockingBy((AActor*)Other) )
		return FALSE;
	else if( Other->IsBrush() || Other->IsEncroacher() )
		return bCollideWorld && Other->bBlockActors;
	else if ( IsBrush() || IsEncroacher() )
		return Other->bCollideWorld && bBlockActors;
	else
		return Other->bBlockActors && bBlockActors;

}

UBOOL AActor::IgnoreBlockingBy( const AActor *Other ) const
{
	return ( bIgnoreEncroachers && Other->IsEncroacher() );
}

UBOOL ABlockingVolume::IgnoreBlockingBy( const AActor *Other ) const
{
	return ( Other->GetAProjectile() != NULL );
}

UBOOL APawn::IgnoreBlockingBy( const AActor *Other ) const
{
	return ((Physics == PHYS_RigidBody && Other->bIgnoreRigidBodyPawns) || (bIgnoreEncroachers && Other->IsEncroacher()));
}

UBOOL APlayerController::IgnoreBlockingBy( const AActor *Other ) const
{
	// allow playercontrollers (acting as cameras) to go through movers
	if ( (Other->Physics == PHYS_RigidBody) && !Other->IsA(AVehicle::StaticClass()) )
		return TRUE;
	return ( bIgnoreEncroachers && Other->IsEncroacher() );
}

UBOOL AProjectile::IgnoreBlockingBy( const AActor *Other ) const
{
	if ( bIgnoreEncroachers && Other->IsEncroacher() )
		return TRUE;
	return ( !bBlockedByInstigator && Other == Instigator );
}

/** 
  *  Static to dynamic meshes don't block pawns or projectiles
  */
UBOOL AKActorFromStatic::IgnoreBlockingBy( const AActor *Other ) const
{
	return (Other->GetAPawn() != NULL);
}

#if WITH_EDITOR
void APawn::EditorApplyRotation(const FRotator& DeltaRotation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);

	// Forward new rotation on to the pawn's controller.
	if( Controller )
	{
		Controller->Rotation = Rotation;
	}
}
#endif

void APawn::SetBase( AActor* NewBase, FVector NewFloor, INT bNotifyActor, USkeletalMeshComponent* SkelComp, FName AttachName )
{
	Floor = NewFloor;
	Super::SetBase(NewBase,NewFloor,bNotifyActor,SkelComp,AttachName);
}

/** Add a data point to a line on the global debug chart. */
void AActor::ChartData(const FString& DataName, FLOAT DataValue)
{
	if(GStatChart)
	{
		// Make graph line name by concatenating actor name with data name.
		FString LineName = FString::Printf(TEXT("%s_%s"), *GetName(), *DataName);

		GStatChart->AddDataPoint(LineName, DataValue);
	}
}

//
void AActor::SetHidden(UBOOL bNewHidden)
{
	if (bHidden != bNewHidden)
	{
		bHidden = bNewHidden;
		bNetDirty = TRUE;
		MarkComponentsAsDirty(FALSE);
	}
}

UBOOL AActor::IsPlayerOwned()
{
    AActor* TopActor = GetTopOwner();
	AController* C = TopActor ? TopActor->GetAController() : NULL;
	return C? C->IsPlayerOwner() : FALSE;
}

/**
 * Returns TRUE if this actor is contained by TestLevel.
 * @todo seamless: update once Actor->Outer != Level
 */
UBOOL AActor::IsInLevel(const ULevel *TestLevel) const
{
	return (GetOuter() == TestLevel);
}

/** Return the ULevel that this Actor is part of. */
ULevel* AActor::GetLevel() const
{
	return CastChecked<ULevel>( GetOuter() );
}

UBOOL AActor::IsInPersistentLevel(UBOOL bIncludeLevelStreamingPersistent) const
{
	ULevel* MyLevel = GetLevel();
	return ( (MyLevel == GWorld->PersistentLevel) || ( bIncludeLevelStreamingPersistent && WorldInfo->StreamingLevels.Num() > 0 &&
														Cast<ULevelStreamingPersistent>(WorldInfo->StreamingLevels(0)) != NULL &&
														WorldInfo->StreamingLevels(0)->LoadedLevel == MyLevel ) );
}

/** Support dynamic trail effect */
UParticleSystem* AActor::GetAnimTrailParticleSystem(const UAnimNotify_Trails* AnimNotifyData) const
{
	return AnimNotifyData->PSTemplate;
}

/**
 * Determine whether this actor is referenced by its level's GameSequence.
 *
 * @param	pReferencer		if specified, will be set to the SequenceObject that is referencing this actor.
 *
 * @return TRUE if this actor is referenced by kismet.
 */
UBOOL AActor::IsReferencedByKismet( USequenceObject** pReferencer/*=NULL*/ ) const
{
	USequence* Sequence = GIsGame ? GWorld->GetGameSequence() : GetLevel()->GetGameSequence();
	return ( Sequence && Sequence->ReferencesObject(this, pReferencer) );
}

/** whether this Actor can be modified by Kismet actions
 * primarily used by error checking to warn LDs when their Kismet may not apply changes correctly (especially on clients)
 * @param AskingOp - Kismet operation to which this Actor is linked
 * @param Reason (out) - If this function returns false, contains the reason why the Kismet action is not allowed to execute on this Actor
 * @return whether the AskingOp can correctly modify this Actor
 */
UBOOL AActor::SupportsKismetModification(USequenceOp* AskingOp, FString& Reason) const
{
	// the primary thing that goes wrong is the effect works on server but can't replicate to the client
	// due to bStatic things not being able to change RemoteRole
	if (bStatic && RemoteRole == ROLE_None && !bForceAllowKismetModification)
	{
		Reason = TEXT("Actor is static");
		return FALSE;
	}
	// don't allow torn off stuff to be modified as we can't replicate changes on it anymore
	else if (bTearOff)
	{
		Reason = TEXT("Actor is dead");
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

/** @return whether this Actor has exactly one attached colliding component (directly or indirectly)
 *  and that component is its CollisionComponent
 */
UBOOL AActor::HasSingleCollidingComponent()
{
	// early out if there is no CollisionComponent
	if (CollisionComponent == NULL)
	{
		return FALSE;
	}
	else
	{
		for (INT i = 0; i < AllComponents.Num(); i++)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(AllComponents(i));
			if (Primitive != NULL && Primitive->CollideActors && Primitive != CollisionComponent)
			{
				return FALSE;
			}
		}

		return TRUE;
	}
}


/** Called each from while the Matinee action is running, to set the animation weights for the actor. */
void AActor::SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos )
{
	// do nothing
}



/*-----------------------------------------------------------------------------
	Special editor support.
-----------------------------------------------------------------------------*/

AActor* AActor::GetHitActor()
{
	return this;
}

// Determines if this actor is hidden in the editor viewports or not.

UBOOL AActor::IsHiddenEd() const
{
	// If any of the standard hide flags are set, return TRUE
	if( bHiddenEdLayer || bHiddenEdCustom || !bEditable || ( GIsEditor && ( bHiddenEdTemporary || bHiddenEdLevel || bHiddenEdScene ) ) )
	{
		return TRUE;
	}
	// Otherwise, it's visible
	return FALSE;
}

// Get the name of the map from the last used URL
FString AActor::GetURLMap()
{
	if (!GIsEditor)
	{
		return CastChecked<UGameEngine>(GEngine)->LastURL.Map;
	}
	else
	{
		//@fixme - figure out map name from editor
		return FString(TEXT(""));
	}
}

/*---------------------------------------------------------------------------------------
	Brush class implementation.
---------------------------------------------------------------------------------------*/

/**
 * Serialize function
 *
 * @param Ar Archive to serialize with
 */
void ABrush::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void ABrush::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(Brush)
	{
		Brush->BuildBound();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ABrush::CopyPosRotScaleFrom( ABrush* Other )
{
	check(BrushComponent);
	check(Other);
	check(Other->BrushComponent);

	Location    = Other->Location;
	Rotation    = Other->Rotation;
	PrePivot	= Other->PrePivot;

	if(Brush)
	{
		Brush->BuildBound();
	}

	ClearComponents();
	ConditionalUpdateComponents();
}

/**
 * Return whether this actor is a builder brush or not.
 *
 * @return TRUE if this actor is a builder brush, FALSE otherwise
 */
UBOOL ABrush::IsABuilderBrush() const
{
	if( GIsGame )
	{
		return FALSE;
	}
	else
	{
		return GetLevel()->GetBrush() == this;
	}
}

/**
 * Return whether this actor is the current builder brush or not
 *
 * @return TRUE if this actor is the current builder brush, FALSE otherwise
 */
UBOOL ABrush::IsCurrentBuilderBrush() const
{
	if(GIsGame || GIsUCC)
	{
		return FALSE;
	}
	else
	{
		return GWorld->GetBrush() == this;
	}
}

void ABrush::InitPosRotScale()
{
	check(BrushComponent);

	Location  = FVector(0,0,0);
	Rotation  = FRotator(0,0,0);
	PrePivot  = FVector(0,0,0);

}
void ABrush::PostLoad()
{
	Super::PostLoad();
#if !CONSOLE
	if ( GIsEditor && !Rotation.IsZero() )
	{
		// Only path up the rotation if this is a level-placed actor (ie not a prefab).
		if ( GetOuter() && GetOuter()->IsA(ULevel::StaticClass()) )
		{
			if ( IsVolumeBrush() )
			{
				const FRotator OldRotation = Rotation;
				Rotation = FRotator(0,0,0);
				FBSPOps::RotateBrushVerts( this, OldRotation, FALSE );
			}
			else
			{
				Rotation = FRotator(0,0,0);
			}

			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_BrushRotationFixed" ), *GetName() ) ), TEXT( "BrushRotationFixed" ) );
		}
	}

	// Assign the default material to brush polys with NULL material references.
	if ( Brush && Brush->Polys )
	{
		if ( IsStaticBrush() )
		{
			for( INT PolyIndex = 0 ; PolyIndex < Brush->Polys->Element.Num() ; ++PolyIndex )
			{
				FPoly& CurrentPoly = Brush->Polys->Element(PolyIndex);
				if ( !CurrentPoly.Material )
				{
					CurrentPoly.Material = GEngine->DefaultMaterial;
				}
			}
		}

		// if the polys of the brush have the wrong outer, fix it up to be the UModel (my Brush member)
		// UModelFactory::FactoryCreateText was passing in the ABrush as the Outer instead of the UModel
		if (Brush->Polys->GetOuter() == this)
		{
			Brush->Polys->Rename(*Brush->Polys->GetName(), Brush, REN_ForceNoResetLoaders);
		}
	}
#endif
}

FColor ABrush::GetWireColor() const
{
	FColor Color = GEngine->C_BrushWire;

	if( IsStaticBrush() )
	{
		Color = bColored ?						BrushColor :
				CsgOper == CSG_Subtract ?		GEngine->C_SubtractWire :
				CsgOper != CSG_Add ?			GEngine->C_BrushWire :
				(PolyFlags & PF_Portal) ?		GEngine->C_SemiSolidWire :
				(PolyFlags & PF_NotSolid) ?		GEngine->C_NonSolidWire :
				(PolyFlags & PF_Semisolid) ?	GEngine->C_ScaleBoxHi :
												GEngine->C_AddWire;
	}
	else if( IsVolumeBrush() )
	{
		Color = bColored ? BrushColor : GEngine->C_Volume;
	}
	else if( IsBrushShape() )
	{
		Color = bColored ? BrushColor : GEngine->C_BrushShape;
	}

	return Color;
}

/**
* Note that the object has been modified.  If we are currently recording into the 
* transaction buffer (undo/redo), save a copy of this object into the buffer and 
* marks the package as needing to be saved.
*
* @param	bAlwaysMarkDirty	if TRUE, marks the package dirty even if we aren't
*								currently recording an active undo/redo transaction
*/
void ABrush::Modify(UBOOL bAlwaysMarkDirty)
{
	Super::Modify(bAlwaysMarkDirty);

	if(Brush)
	{
		Brush->Modify(bAlwaysMarkDirty);
	}
}

/*---------------------------------------------------------------------------------------
	Tracing check implementation.
	ShouldTrace() returns true if actor should be checked for collision under the conditions
	specified by traceflags
---------------------------------------------------------------------------------------*/

UBOOL AActor::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( bWorldGeometry && (TraceFlags & TRACE_LevelGeometry) )
	{
		return TRUE;
	}
	else if( !bWorldGeometry && (TraceFlags & TRACE_Others) )
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			return (bProjTarget || (bBlockActors && Primitive->BlockActors));
		}
		else
		{
			return (!(TraceFlags & TRACE_Blocking) || (SourceActor && SourceActor->IsBlockedBy(this,Primitive)));
		}
	}
	return FALSE;
}

UBOOL AInterpActor::ShouldTrace(UPrimitiveComponent *Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( TraceFlags & TRACE_Movers ) 
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			return (bProjTarget || (bBlockActors && Primitive->BlockActors));
		}
		else
		{
			return (!(TraceFlags & TRACE_Blocking) || (SourceActor && SourceActor->IsBlockedBy(this,Primitive)));
		}
	}
	return FALSE;
}

UBOOL ATrigger_PawnsOnly::ShouldTrace(UPrimitiveComponent *Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if (SourceActor == NULL || SourceActor->GetAPawn() == NULL) 
	{
		return FALSE;
	}

	return Super::ShouldTrace(Primitive, SourceActor, TraceFlags);
}

/**
 * This will look over the set of all attached of components that are SetBased on this Actor
 * and then ShadowParent them to our MyPrimComp and use MyLightEnv.
 **/
void AActor::SetShadowParentOnAllAttachedComponents(UPrimitiveComponent* MyPrimComp, ULightEnvironmentComponent* MyLightEnv)
{
	if (!Base && MyPrimComp)
	{
		// Stack to handle nested attachments without recursion
		TArray<AActor*, TInlineAllocator<5> > ProcessStack;
		for (INT AttachedIndex = 0; AttachedIndex < Attached.Num(); AttachedIndex++)
		{
			AActor* AttachedActor = Attached(AttachedIndex);
			// hack to not affect pawns on the derrick
			const UBOOL bIsNonVehiclePawn = AttachedActor && AttachedActor->GetAPawn() && !AttachedActor->GetAVehicle();
			if (AttachedActor && !bIsNonVehiclePawn)
			{
				ProcessStack.AddItem(AttachedActor);
			}
		}

		while (ProcessStack.Num() > 0)
		{
			AActor* AttachedActor = ProcessStack.Pop();
			checkSlow(AttachedActor);
			// Push attached actors onto the stack of actors to process so that we handle nested attachments
			for( INT AttachedIndex = 0; AttachedIndex < AttachedActor->Attached.Num(); AttachedIndex++ )
			{
				AActor* CurrentAttachedActor = AttachedActor->Attached(AttachedIndex);
				// hack to not affect pawns on the derrick
				const UBOOL bIsNonVehiclePawn = CurrentAttachedActor && CurrentAttachedActor->GetAPawn() && !CurrentAttachedActor->GetAVehicle();

				if (CurrentAttachedActor && !bIsNonVehiclePawn)
				{
					// Cycles in the attachment chain are not allowed so we don't have to handle it
					ProcessStack.AddItem(CurrentAttachedActor);
				}
			}
			for( INT ComponentIndex = 0; ComponentIndex < AttachedActor->Components.Num(); ++ComponentIndex )
			{
				//array of all attached components for this actor
				TArray <UMeshComponent*> AttachedMeshComponents;

				//actor directly inside the actor
				UMeshComponent* RootMeshComp = Cast<UMeshComponent>(AttachedActor->Components(ComponentIndex));
				if (RootMeshComp)
				{
					//add the component that is attached to the actor to seed the recursion
					AttachedMeshComponents.AddItem(RootMeshComp);

					//append on all components attached to THAT component (nesting)
					for( INT AttachmentCheckIndex = 0; AttachmentCheckIndex < AttachedMeshComponents.Num(); ++AttachmentCheckIndex )
					{
						//if this was a valid mesh component
						UMeshComponent* NestedCheckMeshComponent = AttachedMeshComponents(AttachmentCheckIndex);
						check(NestedCheckMeshComponent);
						USkeletalMeshComponent* SkelMeshCheckComponent = Cast<USkeletalMeshComponent>(NestedCheckMeshComponent);
						if (SkelMeshCheckComponent)
						{
							for( INT SkelMeshChildIndex = 0; SkelMeshChildIndex < SkelMeshCheckComponent->Attachments.Num(); ++SkelMeshChildIndex )
							{
								//if the attached component is a mesh component, then we can add it to the array (which will be processed subsequently)
								UMeshComponent* ChildMeshComp = Cast<UMeshComponent>(SkelMeshCheckComponent->Attachments(SkelMeshChildIndex).Component);
								if (ChildMeshComp)
								{
									AttachedMeshComponents.AddItem(ChildMeshComp);
								}
							}
						}
					}
				}

				for( INT ChildMeshComponentIndex = 0; ChildMeshComponentIndex < AttachedMeshComponents.Num(); ++ChildMeshComponentIndex )
				{
					UMeshComponent* MeshComp = AttachedMeshComponents(ChildMeshComponentIndex);
					//
					if (MeshComp != NULL 
						&& MeshComp->LightingChannels == MyPrimComp->LightingChannels
						&& MeshComp->bSelfShadowOnly == MyPrimComp->bSelfShadowOnly)
					{
						UBOOL bOverrodeShadowOrDLE = FALSE;
						if (MyPrimComp->CastShadow 
							&& MyPrimComp->bCastDynamicShadow
							// Only shadow parent if both have the same visibility states
							&& (!MeshComp->GetOwner() || MeshComp->GetOwner()->bHidden == bHidden)
							&& MyPrimComp->HiddenGame == MeshComp->HiddenGame)
						{
							bOverrodeShadowOrDLE = TRUE;
							MeshComp->SetShadowParent( MyPrimComp );
						}

						// Don't override DLE if the attached actor's component uses precomputed shadowing
						if (MyLightEnv && MyLightEnv->IsEnabled() && !MeshComp->bUsePrecomputedShadows)
						{
							ULightEnvironmentComponent* AttacheeMeshLightEnvironment = MeshComp->LightEnvironment;
							// Disable the Attachee's LightEnvironment
							// Make sure we don't disable our own LightEnvironment since it may be shared by the Attachee
							if (AttacheeMeshLightEnvironment != NULL && AttacheeMeshLightEnvironment != MyLightEnv)
							{
								AttacheeMeshLightEnvironment->SetEnabled(FALSE);
							}

							bOverrodeShadowOrDLE = TRUE;
							MeshComp->SetLightEnvironment( MyLightEnv ); // set to parent's LE
						}

						if (bOverrodeShadowOrDLE)
						{
							MeshComp->SetLightingChannels( MyPrimComp->LightingChannels );
						}
					}
				}
			}
		}
	}
}

#if WITH_EDITOR
void AInterpActor::CheckForErrors()
{
	Super::CheckForErrors();
	if ( Base && Base->Physics == PHYS_RigidBody )
	{
		if ( StaticMeshComponent && StaticMeshComponent->BlockRigidBody ) // GWarn'd by ADynamicSMActor::CheckForErrors()
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_BlockRigidBodyWithRigidBody" ), *GetName() ) ), TEXT( "BlockRigidBodyWithRigidBody" ) );
		}
	}
	

	// Warn the user that the actor is attached to a node which doesn't exist in all LOD of the skeletal mesh
	if( StaticMeshComponent && BaseSkelComponent )
	{
		if( StaticMeshComponent->StaticMesh->LODModels.Num() < BaseSkelComponent->SkeletalMesh->LODModels.Num() )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_AttachedNodeDoesntExistInAllLODSkeletalMesh" ), *GetName() ) ), TEXT( "AttachedNodeDoesntExistInAllLODSkeletalMesh" ) );
		}
	}

	if (StaticMeshComponent)
	{
		const UBOOL bPreShadowAllowed = StaticMeshComponent->LightEnvironment && StaticMeshComponent->LightEnvironment->IsEnabled() && !CastChecked<UDynamicLightEnvironmentComponent>(StaticMeshComponent->LightEnvironment)->bUseBooleanEnvironmentShadowing;
		if(StaticMeshComponent->CastShadow 
			&& StaticMeshComponent->bCastDynamicShadow 
			&& StaticMeshComponent->IsAttached() 
			&& StaticMeshComponent->Bounds.SphereRadius > 2000.0f
			&& bPreShadowAllowed)
		{
			// Large shadow casting objects that create preshadows will cause a massive performance hit, since preshadows are meant for small shadow casters.
			// Setting bUseBooleanEnvironmentShadowing=TRUE will prevent the preshadow from being created.
			GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ActorLargeShadowCaster" ), *GetName() ) ), TEXT( "ActorLargeShadowCaster" ) );
		}
	}

	if( bHidden == TRUE )
	{
		if( StaticMeshComponent != NULL )
		{
			if( ( LightEnvironment != NULL ) && ( LightEnvironment->IsEnabled() == TRUE ) )
			{
				const FString NameOfSM = StaticMeshComponent->StaticMesh->GetFullName();

				// Load the 'PerfWarnings_InterpActor_Hidden_DefaultMesh_With_DLE' list.
				const TCHAR* PerfWarningsSection = TEXT("UnrealEd.PerfWarnings_InterpActor_Hidden_DefaultMesh_With_DLE");
				FConfigSection* PerfWarningsSectionList = GConfig->GetSectionPrivate(PerfWarningsSection, FALSE, TRUE, GEditorIni);
				if( PerfWarningsSectionList != NULL )
				{
					// Add each value to the CubList map.
					for (FConfigSectionMap::TIterator It(*PerfWarningsSectionList); It; ++It)
					{
						const FString ValueString = It.Value();

						//warnf( TEXT("%s %s"), *NameOfSM, *ValueString );
						if( NameOfSM.InStr( ValueString ) != INDEX_NONE )
						{
							GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_HiddenInterpActorUsingDefaultCube" ), *GetName(), *ValueString ) ), TEXT( "HiddenInterpActorUsingDefaultCube" ) );
						}
					}
				}
			}
		}
	}

}
#endif

UBOOL AVolume::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( bPawnsOnly && (SourceActor == NULL || SourceActor->GetAPawn() == NULL) )
	{
		return FALSE;
	}
	else if( bWorldGeometry && (TraceFlags & TRACE_LevelGeometry) )
	{
		return TRUE;
	}
	else if( !bWorldGeometry && (TraceFlags & TRACE_Volumes) )
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			return (bProjTarget || (bBlockActors && Primitive->BlockActors));
		}
		else
		{
			return (!(TraceFlags & TRACE_Blocking) || (SourceActor && SourceActor->IsBlockedBy(this,Primitive)));
		}
	}
	return FALSE;
}

/**
 * Force TRACE_LevelGeometry to still work with us even though bWorldGeometry is cleared
 * bWorldGeometry is cleared so that actors can base properly on moving volumes
 * 
 * @param Primitive - the primitive to trace against
 * 
 * @param SourceActor - the actor doing the trace
 * 
 * @param TraceFlags - misc flags describing the trace
 */
UBOOL ADynamicBlockingVolume::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( bPawnsOnly && (SourceActor == NULL || SourceActor->GetAPawn() == NULL) )
	{
		return FALSE;
	}

	if (TraceFlags & TRACE_LevelGeometry)
	{
		return TRUE;
	}

	return Super::ShouldTrace(Primitive, SourceActor, TraceFlags);
}

void AVolume::PostEditImport()
{
	Super::PostEditImport();

#if !CONSOLE
	// The default physics volume doesn't have an associated UModel, so we need to handle that case gracefully.
	if(Brush)
	{
		FBSPOps::csgPrepMovingBrush( this );
	}
#endif // !CONSOLE
}

/**
 * Serialize function.
 *
 * @param	Ar	Archive to serialize with.
 */
void ALevelStreamingVolume::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
}

/**
* Performs operations after the object is loaded. 
*/
void ALevelStreamingVolume::PostLoad()
{
	Super::PostLoad();

	const ULinkerLoad* Linker = GetLinker();

	// Fixup older versions of objects so we don't have bad data
	if(Linker)
	{
		const INT Version = Linker->Ver();

		// This version changes the default streaming volume usage from SVB_Loading to SVB_LoadingAndVisibility.
		if( Version < VER_STREAMINGVOLUME_USAGE_DEFAULT )
		{
			StreamingUsage = Usage_DEPRECATED;
		}
	}
}

/**
 * Called after change has occured - used to force update of contained primitives.
 */
void ACullDistanceVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	GWorld->UpdateCullDistanceVolumes();
}

/**
 * bFinished is FALSE while the actor is being continually moved, and becomes TRUE on the last call.
 * This can be used to defer computationally intensive calculations to the final PostEditMove call of
 * eg a drag operation.
 */
void ACullDistanceVolume::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove(bFinished);
	if( bFinished )
	{
		GWorld->UpdateCullDistanceVolumes();
	}
}

/**
 * Returns whether the passed in primitive can be affected by cull distance volumes.
 *
 * @param	PrimitiveComponent	Component to test
 * @return	TRUE if tested component can be affected, FALSE otherwise
 */
UBOOL ACullDistanceVolume::CanBeAffectedByVolumes( UPrimitiveComponent* PrimitiveComponent )
{
	AActor* Owner = PrimitiveComponent ? PrimitiveComponent->GetOwner() : NULL;

	// Require an owner so we can use its location
	if(	Owner
	// Disregard dynamic actors
	&& ( Owner->IsStatic() || Owner->bNoDelete )
	// Disregard prefabs.
	&& !PrimitiveComponent->IsTemplate()
	// Skip primitives that have bHiddenGame set as we don't want to cull out brush rendering or other helper objects.
	&&	PrimitiveComponent->HiddenGame == FALSE
	// Only operate on primitives attached to current world.				
	&&	PrimitiveComponent->GetScene() == GWorld->Scene 
	// Require cull distance volume support to be enabled.
	&&	PrimitiveComponent->bAllowCullDistanceVolume )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}	
}

/**
 * Get the set of primitives and new max draw distances defined by this volume.
 */
void ACullDistanceVolume::GetPrimitiveMaxDrawDistances(TMap<UPrimitiveComponent*,FLOAT>& OutCullDistances)
{
	// Nothing to do if there is no brush component or no cull distances are set
	if( BrushComponent && CullDistances.Num() > 0 && bEnabled )
	{
		// Test center of mesh bounds to see whether it is encompassed by volume
		// and propagate cull distance if it is.
		for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
		{
			UPrimitiveComponent*	PrimitiveComponent	= *It;
			AActor*					Owner				= PrimitiveComponent->GetOwner();

			// Check whether primitive can be affected by cull distance volumes.
			if( Owner && ACullDistanceVolume::CanBeAffectedByVolumes( PrimitiveComponent ) )
			{
				// Check whether primitive supports cull distance volumes and its center point is being encompassed by this volume.
				if( Encompasses( Owner->Location ) )
				{		
					// Find best match in CullDistances array.
					FLOAT PrimitiveSize			= PrimitiveComponent->Bounds.SphereRadius * 2;
					FLOAT CurrentError			= FLT_MAX;
					FLOAT CurrentCullDistance	= 0;
					for( INT CullDistanceIndex=0; CullDistanceIndex<CullDistances.Num(); CullDistanceIndex++ )
					{
						const FCullDistanceSizePair& CullDistancePair = CullDistances(CullDistanceIndex);
						if( Abs( PrimitiveSize - CullDistancePair.Size ) < CurrentError )
						{
							CurrentError		= Abs( PrimitiveSize - CullDistancePair.Size );
							CurrentCullDistance = CullDistancePair.CullDistance;
						}
					}

					FLOAT* CurrentDistPtr = OutCullDistances.Find(PrimitiveComponent);
					check(CurrentDistPtr);

					// LD or other volume specified cull distance, use minimum of current and one used for this volume.
					if( *CurrentDistPtr > 0 )
					{
						OutCullDistances.Set(PrimitiveComponent, Min( *CurrentDistPtr, CurrentCullDistance ));
					}
					// LD didn't specify cull distance, use current setting directly.
					else
					{
						OutCullDistances.Set(PrimitiveComponent, CurrentCullDistance);
					}
				}
			}
		}
	}
}

UBOOL APhysicsVolume::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( bPawnsOnly && (SourceActor == NULL || SourceActor->GetAPawn() == NULL) )
	{
		return FALSE;
	}

	return ((TraceFlags & TRACE_PhysicsVolumes) || Super::ShouldTrace(Primitive,SourceActor,TraceFlags));
}

#if WITH_EDITOR
void ATrigger::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if ( bCtrlDown )
	{
		// CTRL+Scaling modifies trigger collision height.  This is for convenience, so that height
		// can be changed without having to use the non-uniform scaling widget (which is
		// inaccessable with spacebar widget cycling).
		CylinderComponent->CollisionHeight += ModifiedScale.X;
		CylinderComponent->CollisionHeight = Max( 0.0f, CylinderComponent->CollisionHeight );
	}
	else
	{
		CylinderComponent->CollisionRadius += ModifiedScale.X;
		CylinderComponent->CollisionRadius = Max( 0.0f, CylinderComponent->CollisionRadius );

		// If non-uniformly scaling, Z scale affects height and Y can affect radius too.
		if ( !ModifiedScale.AllComponentsEqual() )
		{
			CylinderComponent->CollisionHeight += -ModifiedScale.Z;
			CylinderComponent->CollisionHeight = Max( 0.0f, CylinderComponent->CollisionHeight );

			CylinderComponent->CollisionRadius += ModifiedScale.Y;
			CylinderComponent->CollisionRadius = Max( 0.0f, CylinderComponent->CollisionRadius );
		}
	}
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void ApplyScaleToFloat(FLOAT& Dst, const FVector& DeltaScale, FLOAT Magnitude)
{
	const FLOAT Multiplier = ( DeltaScale.X > 0.0f || DeltaScale.Y > 0.0f || DeltaScale.Z > 0.0f ) ? Magnitude : -Magnitude;
	Dst += Multiplier * DeltaScale.Size();
	Dst = Max( 0.0f, Dst );
}

void APointLight::PostLoad()
{
	Super::PostLoad();
	if (IsStatic() || !bMovable)
	{
		// bForceDynamicLight used to be exposed to the UI, which allowed LDs to take a static light type,
		// And make it render with dynamic lighting even though it was still a static light type
		LightComponent->bForceDynamicLight = FALSE;
	}

	if (IsStatic())
	{
		// UseDirectLightMap used to be exposed to the UI, but for level placed lights we want it to always come from the light actor
		LightComponent->UseDirectLightMap = TRUE;
		LightComponent->Function = NULL;
	}
}

void ASpotLight::PostLoad()
{
	Super::PostLoad();
	if (IsStatic() || !bMovable)
	{
		// bForceDynamicLight used to be exposed to the UI, which allowed LDs to take a static light type,
		// And make it render with dynamic lighting even though it was still a static light type
		LightComponent->bForceDynamicLight = FALSE;
	}

	if (IsStatic())
	{
		// UseDirectLightMap used to be exposed to the UI, but for level placed lights we want it to always come from the light actor
		LightComponent->UseDirectLightMap = TRUE;
		LightComponent->Function = NULL;
	}
}

void ADirectionalLight::PostLoad()
{
	Super::PostLoad();
	if (IsStatic() || !bMovable)
	{
		// bForceDynamicLight used to be exposed to the UI, which allowed LDs to take a static light type,
		// And make it render with dynamic lighting even though it was still a static light type
		LightComponent->bForceDynamicLight = FALSE;
	}

	if (IsStatic())
	{
		// UseDirectLightMap used to be exposed to the UI, but for level placed lights we want it to always come from the light actor
		LightComponent->UseDirectLightMap = TRUE;
		LightComponent->Function = NULL;
	}
}

#if WITH_EDITOR
void AWindPointSource::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	check( Component );

	const FVector ModifiedScale = DeltaScale * 500.0f;

	ApplyScaleToFloat( Component->Radius, ModifiedScale, 1.0f );
	PostEditChange();
}

void APointLight::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>( LightComponent );
	check( PointLightComponent );

	const FVector ModifiedScale = DeltaScale * 500.0f;

	ApplyScaleToFloat( PointLightComponent->Radius, ModifiedScale, 1.0f );
	PostEditChange();
}

void ASpotLight::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>( LightComponent );
	check( SpotLightComponent );
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if ( bCtrlDown )
	{
		ApplyScaleToFloat( SpotLightComponent->OuterConeAngle, ModifiedScale, 0.01f );
		SpotLightComponent->OuterConeAngle = Min( 89.0f, SpotLightComponent->OuterConeAngle );
		SpotLightComponent->InnerConeAngle = Min( SpotLightComponent->OuterConeAngle, SpotLightComponent->InnerConeAngle );
	}
	else if ( bAltDown )
	{
		ApplyScaleToFloat( SpotLightComponent->InnerConeAngle, ModifiedScale, 0.01f );
		SpotLightComponent->InnerConeAngle = Min( 89.0f, SpotLightComponent->InnerConeAngle );
		SpotLightComponent->OuterConeAngle = Max( SpotLightComponent->OuterConeAngle, SpotLightComponent->InnerConeAngle );
	}
	else
	{
		ApplyScaleToFloat( SpotLightComponent->Radius, ModifiedScale, 1.0f );
	}

	PostEditChange();
}
#endif

/**
 * Changes the bHardAttach value. First checks to see if this actor is based
 * on another actor. If so, the actor is "re-based" so that the hard attach
 * will take effect.
 *
 * @param bNewHardAttach the new hard attach setting
 */
void AActor::SetHardAttach(UBOOL bNewHardAttach)
{
	if (bNewHardAttach != bHardAttach)
	{
		AActor* OldBase = Base;
		// If this actor is already based, it needs to rebase itself so that
		// the matrix is properly updated
		if (OldBase != NULL)
		{
			// Cache the other settings
			USkeletalMeshComponent* OldBaseSkelComponent = BaseSkelComponent;
			FName OldBaseBoneName = BaseBoneName;
			// Clear the old base
			SetBase(NULL,FVector(0.f,0.f,1.f),0,NULL,NAME_None);
			bHardAttach = bNewHardAttach;
			// "Re-base" to the old base
			SetBase(OldBase,FVector(0.f,0.f,1.f),0,OldBaseSkelComponent,OldBaseBoneName);
		}
		else
		{
			bHardAttach = bNewHardAttach;
		}
	}
}

/**
 * Changes the bHardAttach value. First checks to see if this actor is based
 * on another actor. If so, the actor is "re-based" so that the hard attach
 * will take effect.
 *
 * @param bNewHardAttach the new hard attach setting
 */
void APawn::SetHardAttach(UBOOL bNewHardAttach)
{
	if (bNewHardAttach != bHardAttach)
	{
		AActor* OldBase = Base;
		// If this actor is already based, it needs to rebase itself so that
		// the matrix is properly updated
		if (OldBase != NULL)
		{
			// Cache the other settings
			USkeletalMeshComponent* OldBaseSkelComponent = BaseSkelComponent;
			FName OldBaseBoneName = BaseBoneName;
			// Pawns also use the floor setting
			FVector OldFloor = Floor;
			// Clear the old base
			SetBase(NULL,FVector(0.f,0.f,1.f),0,NULL,NAME_None);
			bHardAttach = bNewHardAttach;
			// "Re-base" to the old base
			SetBase(OldBase,OldFloor,0,OldBaseSkelComponent,OldBaseBoneName);
		}
		else
		{
			bHardAttach = bNewHardAttach;
		}
	}
}

/** @return the optimal location to fire weapons at this actor */
FVector AActor::GetTargetLocation(AActor* RequestedBy, UBOOL bRequestAlternateLoc) const
{
	return Location;
}

/**
 * Check if this actor is the owner when doing relevancy checks for actors marked bOnlyRelevantToOwner
 * 
 * @param ReplicatedActor - the actor we're doing a relevancy test on
 * 
 * @param ActorOwner - the owner of ReplicatedActor
 * 
 * @param ConnectionActor - the controller of the connection that we're doing relevancy checks for
 * 
 * @return TRUE if this actor should be considered the owner
 */
UBOOL AActor::IsRelevancyOwnerFor(AActor* ReplicatedActor, AActor* ActorOwner, AActor* ConnectionActor)
{
	return (ActorOwner == this);
}

void AActor::SetNetUpdateTime(FLOAT NewUpdateTime)
{
	NetUpdateTime = NewUpdateTime;
}

/** adds/removes a property from a list of properties that will always be replicated when this Actor is bNetInitial, even if the code thinks
 * the client has the same value the server already does
 * This is a workaround to the problem where an LD places an Actor in the level, changes a replicated variable away from the defaults,
 * then at runtime the variable is changed back to the default but it doesn't replicate because initial replication is based on class defaults
 * Only has an effect when called on bStatic or bNoDelete Actors
 * Only properties already in the owning class's replication block may be specified
 * @param PropToReplicate the property to add or remove to the list
 * @param bAdd true to add the property, false to remove the property
 */
void AActor::SetForcedInitialReplicatedProperty(UProperty* PropToReplicate, UBOOL bAdd)
{
	if (bStatic || bNoDelete)
	{
		if (PropToReplicate == NULL)
		{
			debugf(NAME_Warning, TEXT("None passed into SetForcedInitialReplicatedProperty()"));
		}
		else if (!GetClass()->IsChildOf(PropToReplicate->GetOwnerClass()))
		{
			debugf(NAME_Warning, TEXT("Property '%s' passed to SetForcedInitialReplicatedProperty() that is not a member of Actor '%s'"), *PropToReplicate->GetPathName(), *GetName());
		}
		else if (!(PropToReplicate->PropertyFlags & CPF_Net))
		{
			debugf(NAME_Warning, TEXT("Property '%s' passed to SetForcedInitialReplicatedProperty() is not in the replication block for its class"), *PropToReplicate->GetPathName());
		}
		else if (WorldInfo->NetMode != NM_Client && WorldInfo->NetMode != NM_Standalone && GWorld->GetNetDriver() != NULL)
		{
			TArray<UProperty*>* PropArray = GWorld->GetNetDriver()->ForcedInitialReplicationMap.Find(this);
			if (bAdd)
			{
				if (PropArray == NULL)
				{
					// add an entry for this Actor
					TArray<UProperty*> NewArray;
					NewArray.AddItem(PropToReplicate);
					GWorld->GetNetDriver()->ForcedInitialReplicationMap.Set(this, NewArray);
				}
				else
				{
					// add to list for this Actor
					PropArray->AddUniqueItem(PropToReplicate);
				}
			}
			else if (PropArray != NULL)
			{
				PropArray->RemoveItem(PropToReplicate);
				if (PropArray->Num() == 0)
				{
					GWorld->GetNetDriver()->ForcedInitialReplicationMap.Remove(this);
				}
			}
		}
	}
}

/*
 * Tries to position a box to avoid overlapping world geometry.
 * If no overlap, the box is placed at SpotLocation, otherwise the position is adjusted
 * @Parameter BoxExtent is the collision extent (X and Y=CollisionRadius, Z=CollisionHeight)
 * @Parameter SpotLocation is the position where the box should be placed.  Contains the adjusted location if it is adjusted.
 * @Return true if successful in finding a valid non-world geometry overlapping location
 */
UBOOL AActor::FindSpot(FVector BoxExtent, FVector& SpotLocation)
{
	return GWorld->FindSpot(BoxExtent, SpotLocation, bCollideComplex, this);
}

/**
Hook to allow actors to render HUD overlays for themselves.
Assumes that appropriate font has already been set
*/
void AActor::NativePostRenderFor(APlayerController *PC, UCanvas *Canvas, FVector CameraPosition, FVector CameraDir)
{
	if ( bPostRenderIfNotVisible || ((WorldInfo->TimeSeconds - LastRenderTime < 0.1f) && (CameraDir | (Location - CameraPosition)) > 0.f) )
	{
		eventPostRenderFor(PC, Canvas, CameraPosition, CameraDir);
	}
}

BYTE AActor::GetTeamNum()
{
	return eventScriptGetTeamNum();
}

/** This will return the direction in LocalSpace that that actor is moving.  This is useful for firing off effects based on which way the actor is moving. **/
BYTE AActor::MovingWhichWay( FLOAT& Amount )
{
	EMoveDir Retval = MD_Stationary;

	const FVector& LocalDir = LocalToWorld().TransformNormal( Velocity );

	// if X > Y then we are moving forward backward
	// 
	if( Abs(LocalDir.X) >= Abs(LocalDir.Y) )
	{
		if( LocalDir.X > 0 )
		{
			Retval = MD_Forward;
		}
		else
		{
			Retval = MD_Backward;
		}

		Amount = LocalDir.X;
	}
	else
	{
		if( LocalDir.Y > 0 )
		{
			Retval = MD_Left;
		}
		else
		{
			Retval = MD_Right;
		}

		Amount = LocalDir.Y;
	}

	// now check for up / down
	if( ( Abs(LocalDir.Z) > Abs(LocalDir.X) )
		&& ( Abs(LocalDir.Z) > Abs(LocalDir.Y) )
		)
	{
		if( LocalDir.Z > 0 )
		{
			Retval = MD_Up;
		}
		else
		{
			Retval = MD_Down;
		}

		Amount = LocalDir.Z;
	}

	//warnf( TEXT( "X:%f Y:%f Z:%f  D:%d  A:%f"), LocalDir.X, LocalDir.Y, LocalDir.Z, static_cast<BYTE>(Retval), Amount );

	return Retval;
}

void AActor::SetHUDLocation(FVector NewHUDLocation)
{}

FVector AWeapon::GetPhysicalFireStartLoc(FVector AimDir)
{
	FLOAT MuzzleDist;
	FVector MuzzleLoc, PulledInMuzzleLoc, ExtraPullIn, CylinderLocation;

	MuzzleLoc = eventGetMuzzleLoc();

	if ( (Instigator == NULL) || (AimDir.IsNearlyZero()) )
		return MuzzleLoc;

	if (Instigator->CylinderComponent)
	{
		// is muzzle outside pawn's collision cylinder?
		CylinderLocation = Instigator->Location - Instigator->CylinderComponent->Translation;
		MuzzleDist = (MuzzleLoc - CylinderLocation).Size2D();
		if (MuzzleDist > Instigator->CylinderComponent->CollisionRadius)
		{
			// pull MuzzleLoc back toward cylinder
			PulledInMuzzleLoc = MuzzleLoc - MuzzleDist * AimDir;
			MuzzleDist = (PulledInMuzzleLoc - CylinderLocation).Size2D();
			if ( MuzzleDist < Instigator->CylinderComponent->CollisionRadius )
			{
				MuzzleLoc = PulledInMuzzleLoc;
			}
			else
			{
				ExtraPullIn = CylinderLocation - PulledInMuzzleLoc;
				ExtraPullIn.Z = 0;
				ExtraPullIn = (2.0 + MuzzleDist - Instigator->CylinderComponent->CollisionRadius) * (ExtraPullIn).SafeNormal();
				MuzzleLoc = PulledInMuzzleLoc + ExtraPullIn;
			}
		}
	}
	return MuzzleLoc;
}

/**
 *	Calls PrestreamTextures() for all the actor's meshcomponents.
 *	@param Seconds			Number of seconds to force all mip-levels to be resident
 *	@param bEnableStreaming	Whether to start (TRUE) or stop (FALSE) streaming
 *	@param CinematicTextureGroups	Bitfield indicating which texture groups that use extra high-resolution mips
 */
void AActor::PrestreamTextures( FLOAT Seconds, UBOOL bEnableStreaming, INT CinematicTextureGroups )
{
	// This only handles non-location-based streaming. Location-based streaming is handled by SeqAct_StreamInTextures::UpdateOp.
	FLOAT Duration = 0.0;
	if ( bEnableStreaming )
	{
		// A Seconds==0.0f, it means infinite (e.g. 30 days)
		Duration = appIsNearlyZero(Seconds) ? (60.0f*60.0f*24.0f*30.0f) : Seconds;
	}

	// Iterate over all components of that actor
	for (INT ComponentIndex=0; ComponentIndex < AllComponents.Num(); ComponentIndex++)
	{
		// If its a static mesh component, with a static mesh
		UActorComponent* Component = AllComponents(ComponentIndex);
		UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component);
		if ( MeshComponent )
		{
			MeshComponent->PrestreamTextures( Duration, FALSE, CinematicTextureGroups );
		}
	}
}

FVector AActor::GetAvoidanceVector(const TArray<class AActor*>& Obstacles, FVector GoalLocation, FLOAT CollisionRadius, FLOAT MaxSpeed, INT NumSamples, FLOAT VelocityStepRate, FLOAT MaxTimeTilOverlap)
{
	FVector GoalVec = (GoalLocation - Location);
	GoalVec.Z = 0;
	//debugf(TEXT("GetAvoidanceVector for %s, number of obstacles: %d, samples: %d, step rate: %.3f, time til: %.3f"),*GetName(),Obstacles.Num(),NumSamples,VelocityStepRate,MaxTimeTilOverlap);
	if ((GoalVec).Size2D() < CollisionRadius * 0.5)	//@todo - handle z
	{
		//debugf(TEXT("> touching goal, skipping checks"));
		return FVector(0.f);
	}
	// preferred velocity = goal - location
	FRotator PreferredDir = GoalVec.Rotation();
	// for each velocity candidate
	FRotator BestDir(0,0,0);
	FLOAT BestPenalty = 99999.f;
	FRotator Dir(PreferredDir);
	for (INT Idx = 0; Idx < NumSamples; Idx++)
	{
		// calc new vector to test
		Dir.Yaw = PreferredDir.Yaw + appTrunc(Idx/(FLOAT)(NumSamples)*65535.f);
		FLOAT DirPenalty = 1.f + (Dir.Vector() | -GoalVec.SafeNormal());
		FLOAT BasePenalty = Max<FLOAT>(DirPenalty,0.1);
		// rate this candidate based on collisions with other obstacles
		for (INT ObstacleIdx = 0; ObstacleIdx < Obstacles.Num(); ObstacleIdx++)
		{
			AActor *Obstacle = Obstacles(ObstacleIdx);
			if (Obstacle == NULL || Obstacle == this)
			{
				continue;
			}
			FLOAT TimeTilOverlap = MaxTimeTilOverlap;
			if (WillOverlap(Location,Dir.Vector()*MaxSpeed,Obstacle->Location,Obstacle->Velocity,VelocityStepRate,CollisionRadius,TimeTilOverlap))
			{
				//debugf(TEXT(">> will overlap %s at %.3f w/ candidate %d (%.3f vs %.3f"),*Obstacle->GetName(),TimeTilOverlap,Idx,MaxSpeed,Obstacle->Velocity.Size());
				if (TimeTilOverlap < MaxTimeTilOverlap)
				{
					//@todo - need to tweak this a bit, also incorporate weightings to allow certain actors to override others
					DirPenalty += BasePenalty * (MaxTimeTilOverlap/TimeTilOverlap);
				}
				else
				{
					// add a slight penalty to avoid picking over a completely collision free option
					DirPenalty += BasePenalty * 0.01f;
				}
				if (DirPenalty > BestPenalty)
				{
					// this direction is a fail, skip further tests
					//debugf(TEXT(">>> already worse than best, early out"));
					break;
				}
			}
			//@todo - (optionally) test for static collisions using line checks
		}
		//debugf(TEXT("> candidate %d penalty: %.3f [current best: %.3f]"),Idx,DirPenalty,BestPenalty);
		if (DirPenalty <= 0.01f)
		{
			// first wins, don't bother further testing
			BestDir = Dir;
			break;
		}
		else if (DirPenalty < BestPenalty)
		{
			BestPenalty = DirPenalty;
			BestDir = Dir;
		}
	}
	//@todo - (optionally) use best dir if the obstacle isn't also attempting avoidance (i.e. player avoidance)
	return (BestDir.Vector() + Velocity.SafeNormal())/2.f;
}

UBOOL AActor::WillOverlap(FVector PosA,FVector VelA,FVector PosB,FVector VelB,FLOAT StepSize,FLOAT Radius,FLOAT& Time)
{
	const FLOAT MaxTime = Time;
	Time = 0.f;
	FLOAT Dist = 0.f;
	FLOAT LastDist = 99999.f;
	const FLOAT CheckSize = 2 * Radius;
	while (Time < MaxTime)
	{
		Dist = (PosA - PosB).Size();
		if (Dist <= CheckSize)
		{
			return TRUE;
		}
		// parting ways, early out
		else if (Abs(LastDist - Dist) < 0.01)
		{
			return FALSE;
		}
		// step to next position
		else
		{
			PosA += VelA * StepSize;
			PosB += VelB * StepSize;
			Time += StepSize;
			LastDist = Dist;
		}
	};
	return FALSE;
}

/** Returns the Environment Volume at TestLocation which has the highest LocationPriority number */
AEnvironmentVolume* AWorldInfo::FindEnvironmentVolume(FVector TestLocation)
{
	for(INT i=0; i<EnvironmentVolumes.Num(); i++)
	{
		AEnvironmentVolume* TestVolume = EnvironmentVolumes(i);
		if( TestVolume->Encompasses(TestLocation) )
		{
			return TestVolume;
		}
	}

	return NULL;
}

void ATargetPoint::IncrementSpawnRef()
{
#if WITH_EDITORONLY_DATA
	if (SpawnRefCount == 0 && SpriteComp != NULL)
	{
		SpriteComp->SetSprite(SpawnSpriteTexture);
	}
#endif // WITH_EDITORONLY_DATA
	SpawnRefCount++;
}

void ATargetPoint::DecrementSpawnRef()
{
	SpawnRefCount--;
	if (SpawnRefCount < 0)
	{
		SpawnRefCount = 0;
	}
	if (SpawnRefCount == 0 && SpriteComp != NULL)
	{
		SpriteComp->SetSprite(((ATargetPoint*)(ATargetPoint::StaticClass()->GetDefaultObject()))->SpriteComp->Sprite);
	}
}

UBOOL AActor::ShouldBeHiddenBySHOW_NavigationNodes()
{
	return IsA(ANavigationPoint::StaticClass());
}

/**
 * Called when this actor is in a level which is being removed from the world (e.g. my level is getting UWorld::RemoveFromWorld called on it)
 */
void AActor::OnRemoveFromWorld()
{
	// if this is actor implements the path object interface, notify the interface it's being unloaded
	IInterface_NavMeshPathObstacle* POInt = InterfaceCast<IInterface_NavMeshPathObstacle>(this);
	if(POInt != NULL)
	{
		POInt->CleanupOnRemoval();
	}
	// Shut down physics for all the Actors in the level.
	TermRBPhys(NULL);
	bScriptInitialized = FALSE;
}

/**
 *  Retrieve various actor metrics depending on the provided type.  All of
 *  these will total the values of the given type for every component that
 *  makes up the actor.
 *
 *  @param MetricsType The type of metric to calculate.
 *
 *  METRICS_VERTS    - Get the number of vertices.
 *  METRICS_TRIS     - Get the number of triangles.
 *  METRICS_SECTIONS - Get the number of sections.
 *
 *  @return INT The total of the given type for this actor.
 */
INT AActor::GetActorMetrics(BYTE MetricsType)
{
	INT TotalCount(0);

	// move rigid body components in physics system
	for (INT i = 0; i < AllComponents.Num(); ++i)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(AllComponents(i));
		if (Primitive != NULL)
		{
			TotalCount += Primitive->GetActorMetrics(EActorMetricsType(MetricsType));
		}
	}

	return TotalCount;
}

/**
 * Searches through this Actor's Components and returns the
 * first SpriteComponent found. If none exist, returns NULL.
 *
 * @return	SpriteComponent	First found SpriteComponent for this actor. NULL if none are found.
 */
USpriteComponent* AActor::GetActorSpriteComponent() const
{
	// Search through actor components and return the first found SpriteComponent
	for( INT CurComponentIndex = 0; CurComponentIndex < Components.Num(); ++CurComponentIndex )
	{
		USpriteComponent* SpriteComponent = Cast<USpriteComponent>( Components( CurComponentIndex ) );
		if( SpriteComponent )
		{
			return SpriteComponent;
		}
	}
	return NULL;
}


void UCloudSaveSystem::SerializeObject(class UObject* ObjectToSerialize, FMemoryWriter& MemoryWriter, int VersionNumber)
{
	MemoryWriter << VersionNumber;

	FObjectAndNameAsStringProxyArchive Ar(MemoryWriter);

	ObjectToSerialize->Serialize(Ar);
}

UObject* UCloudSaveSystem::DeserializeObject(class UClass* ObjectClass, FMemoryReader MemoryReader, BYTE VersionSupport, int VersionNumber)
{
	int StoredVersionNumber;
	MemoryReader << StoredVersionNumber;

	switch(VersionSupport)
	{
		case SaveDataVersionSupportLessThenEqual:
			if (StoredVersionNumber > VersionNumber)
			{
				return NULL;
			}
			break;
		case SaveDataVersionSupportEqual:
			if (StoredVersionNumber != VersionNumber)
			{
				return NULL;
			}
			break;
		default:
			break;
	}

	UObject* Obj = StaticConstructObject(ObjectClass);
	// use a wrapper archive that converts FNames and UObject*'s to strings that can be read back in
	FObjectAndNameAsStringProxyArchive Ar(MemoryReader);

	// serialize the object
	Obj->Serialize(Ar);

	// return the deserialized object
	return Obj;
}

void UCloudSaveSystem::SerializeObject(class UObject* ObjectToSerialize, TArray<BYTE>& Data, int VersionNumber)
{
	FMemoryWriter MemoryWriter(Data);
	SerializeObject(ObjectToSerialize, MemoryWriter, VersionNumber);
}

UObject* UCloudSaveSystem::DeserializeObject(class UClass* ObjectClass, TArray<BYTE>& Data, BYTE VersionSupport,  int VersionNumber)
{
	FMemoryReader MemoryReader(Data, TRUE);

	return DeserializeObject(ObjectClass, MemoryReader, VersionSupport, VersionNumber);
}

