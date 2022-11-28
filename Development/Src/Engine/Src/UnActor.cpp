/*=============================================================================
	UnActor.cpp: AActor implementation
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "EngineAIClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "UnStatChart.h"

/*-----------------------------------------------------------------------------
	AActor object implementations.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AActor);
IMPLEMENT_CLASS(ALight);
IMPLEMENT_CLASS(AClipMarker);
IMPLEMENT_CLASS(APolyMarker);
IMPLEMENT_CLASS(AWeapon);
IMPLEMENT_CLASS(ANote);
IMPLEMENT_CLASS(ALevelInfo);
IMPLEMENT_CLASS(AGameInfo);
IMPLEMENT_CLASS(AZoneInfo);
IMPLEMENT_CLASS(UReachSpec);
IMPLEMENT_CLASS(APathNode);
IMPLEMENT_CLASS(ANavigationPoint);
IMPLEMENT_CLASS(AScout);
IMPLEMENT_CLASS(AProjectile);
IMPLEMENT_CLASS(ATeleporter);
IMPLEMENT_CLASS(APlayerStart);
IMPLEMENT_CLASS(AKeypoint);
IMPLEMENT_CLASS(AInventory);
IMPLEMENT_CLASS(AInventoryManager);
IMPLEMENT_CLASS(APickupFactory);
IMPLEMENT_CLASS(ATrigger);
IMPLEMENT_CLASS(AHUD);
IMPLEMENT_CLASS(ASavedMove);
IMPLEMENT_CLASS(AInfo);
IMPLEMENT_CLASS(AReplicationInfo);
IMPLEMENT_CLASS(APlayerReplicationInfo);
IMPLEMENT_CLASS(AInternetInfo);
IMPLEMENT_CLASS(AGameReplicationInfo);
IMPLEMENT_CLASS(ULevelSummary);
IMPLEMENT_CLASS(ADroppedPickup);
IMPLEMENT_CLASS(AController);
IMPLEMENT_CLASS(AAIController);
IMPLEMENT_CLASS(APlayerController);
IMPLEMENT_CLASS(AMutator);
IMPLEMENT_CLASS(AVehicle);
IMPLEMENT_CLASS(ALadder);
IMPLEMENT_CLASS(UDamageType);
IMPLEMENT_CLASS(UKillZDamageType);
IMPLEMENT_CLASS(UDmgType_Suicided);
IMPLEMENT_CLASS(ABrush);
IMPLEMENT_CLASS(AVolume);
IMPLEMENT_CLASS(APhysicsVolume);
IMPLEMENT_CLASS(ADefaultPhysicsVolume);
IMPLEMENT_CLASS(ALadderVolume);
IMPLEMENT_CLASS(APotentialClimbWatcher);
IMPLEMENT_CLASS(ABlockingVolume);
IMPLEMENT_CLASS(AAutoLadder);
IMPLEMENT_CLASS(ATeamInfo);
IMPLEMENT_CLASS(AJumpPad);
IMPLEMENT_CLASS(ACarriedObject);
IMPLEMENT_CLASS(AObjective);
IMPLEMENT_CLASS(UEdCoordSystem);
IMPLEMENT_CLASS(UEdLayer);
IMPLEMENT_CLASS(ARoute);
IMPLEMENT_CLASS(UBookMark);

/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/
static inline UBOOL NEQ(BYTE A,BYTE B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(INT A,INT B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(BITFIELD A,BITFIELD B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(FLOAT& A,FLOAT& B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FVector& A,FVector& B,UPackageMap* Map,UActorChannel* Channel) {return ((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] || ((INT*)&A)[2]!=((INT*)&B)[2];}
static inline UBOOL NEQ(FRotator& A,FRotator& B,UPackageMap* Map,UActorChannel* Channel) {return A.Pitch!=B.Pitch || A.Yaw!=B.Yaw || A.Roll!=B.Roll;}
static inline UBOOL NEQ(UObject* A,UObject* B,UPackageMap* Map,UActorChannel* Channel) {if( Map->CanSerializeObject(A) )return A!=B; Channel->bActorMustStayDirty = true; 
//debugf(TEXT("%s Must stay dirty because of %s"),Channel->Actor->GetName(),A->GetName());
return (B!=NULL);}
static inline UBOOL NEQ(FName& A,FName B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FColor& A,FColor& B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FPlane& A,FPlane& B,UPackageMap* Map,UActorChannel* Channel) {return
((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] ||
((INT*)&A)[2]!=((INT*)&B)[2] || ((INT*)&A)[3]!=((INT*)&B)[3];}
static inline UBOOL NEQ(const FString& A,const FString& B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}


static inline UBOOL NEQ(const FCompressedPosition& A, const FCompressedPosition& B,UPackageMap* Map,UActorChannel* Channel)
{
		return 1; // only try to replicate in compressed form if already know location has changed
}

#define DOREP(c,v) \
	if( NEQ(v,((A##c*)Recent)->v,Map,Channel) ) \
	{ \
		static UProperty* sp##v = FindObjectChecked<UProperty>(A##c::StaticClass(),TEXT(#v)); \
		*Ptr++ = sp##v->RepIndex; \
	}

#define DOREPARRAY(c,v) \
	{static UProperty* sp##v = FindObjectChecked<UProperty>(A##c::StaticClass(),TEXT(#v)); \
		for( INT i=0; i<ARRAY_COUNT(v); i++ ) \
			if( NEQ(v[i],((A##c*)Recent)->v[i],Map,Channel) ) \
				*Ptr++ = sp##v->RepIndex+i;}

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
	if ( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( bSkipActorPropertyReplication && !bNetInitial )
		{
			return Ptr;
		}

		if ( Role == ROLE_Authority )
		{
			if ( bReplicateMovement )
			{
				UBOOL bRigidActor = ( !bNetInitial && ((Physics == PHYS_RigidBody) || (Physics == PHYS_Articulated)) );
				if ( RemoteRole != ROLE_AutonomousProxy )
				{
					if ( !bRigidActor )
					{
						UBOOL bAlreadyLoc = false;

						// If the actor was based and is no longer, send the location!
						if ( !bCompressedPosition && !Base && ((AActor*)Recent)->Base )
						{
							static UProperty* spLocation = FindObjectChecked<UProperty>(AActor::StaticClass(),TEXT("Location"));
							*Ptr++ = spLocation->RepIndex;
							bAlreadyLoc = true;
						}

						DOREP(Actor,Base);
						if( Base && !Base->bWorldGeometry )
						{
							DOREP(Actor,RelativeLocation);
							DOREP(Actor,RelativeRotation);
								DOREP(Actor,BaseBoneName);
							if ( !bCompressedPosition && !((AActor*)Recent)->Base )
								DOREP(Actor,Location);
						}
						else if( !bCompressedPosition && (bNetInitial || bUpdateSimulatedPosition) )
						{
							if ( !bNetInitial && !bAlreadyLoc )
							{
								DOREP(Actor,Location);
							}
							if ( !bNetInitial )
								DOREP(Actor,Physics);
							if( !bNetInitial || !bNetInitialRotation )
								DOREP(Actor,Rotation);
						}

						if( Physics==PHYS_Rotating )
						{
							DOREP(Actor,RotationRate);
							DOREP(Actor,DesiredRotation);
						}
					}

					if ( RemoteRole == ROLE_SimulatedProxy )
					{
						if ( !bRigidActor && !bCompressedPosition && (bNetInitial || bUpdateSimulatedPosition) )
						{
							DOREP(Actor,Velocity);
						}

						if ( bNetInitial )
						{
							DOREP(Actor,Physics);
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
				DOREP(Actor,PrePivot);
				DOREP(Actor,bCollideActors);
				DOREP(Actor,bCollideWorld);
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
					if ( bReplicateInstigator && (!bNetTemporary || (Instigator && Map->CanSerializeObject(Instigator))) )
					{
						DOREP(Actor,Instigator);
					}
				}
			}
			DOREP(Actor,bHidden);
			DOREP(Actor,Role);
			DOREP(Actor,RemoteRole);
			DOREP(Actor,bNetOwner);
			DOREP(Actor,bTearOff);
			DOREP(Actor,bHardAttach);
 		}
	}
	return Ptr;
}

INT* APawn::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	bCompressedPosition = false;

	if ( !bNetOwner )
	{
/* FIXMESTEVE
		if (  bUseCompressedPosition
&& (Base ? Base->bWorldGeometry : !((AActor*)Recent)->Base) )
		{
			if ( Velocity != ((AActor*)Recent)->Velocity )
			{
				LastLocTime = Level->TimeSeconds;
				bCompressedPosition = true;
				PawnPosition.Location = Location;
				PawnPosition.Rotation = Rotation;
				PawnPosition.Velocity = Velocity;
				DOREP(Pawn,PawnPosition);
				((APawn*)Recent)->Location = Location;
				((APawn*)Recent)->Rotation = Rotation;
				((APawn*)Recent)->Velocity = Velocity;
			}
			else if ( (Level->TimeSeconds - LastLocTime > 1.f) && (Physics == PHYS_Walking) && (((APawn*)Recent)->Location == Location) )
			{
				// force location update every half second
				LastLocTime = Level->TimeSeconds;
				bCompressedPosition = true;
				PawnPosition.Location = Location;
				PawnPosition.Rotation = Rotation;
				if ( PawnPosition.Velocity == FVector(0.f,0.f,-1.f) )
					PawnPosition.Velocity = FVector(0.f,0.f,-2.f);
				else
					PawnPosition.Velocity = FVector(0.f,0.f,-1.f);
				DOREP(Pawn,PawnPosition);
				((APawn*)Recent)->Location = Location;
				((APawn*)Recent)->Rotation = Rotation;
				((APawn*)Recent)->Velocity = PawnPosition.Velocity;
			}
		}
*/
		if ( !bTearOff && !bNetOwner && (Level->TimeSeconds - Channel->LastFullUpdateTime < 0.09f) )
		{
			DOREP(Pawn,PlayerReplicationInfo);
			return Ptr;
		}
	}
	Channel->LastFullUpdateTime = Level->TimeSeconds;

	if ( bTearOff
		&& (StaticClass()->ClassFlags & CLASS_NativeReplication)
		&& (Role==ROLE_Authority) && bNetDirty )
	{
		DOREP(Pawn,TearOffMomentum);
	}

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( Role==ROLE_Authority )
		{
			if ( !bNetOwner )
			{
				DOREP(Pawn,RemoteViewPitch);
			}

			if ( bNetDirty )
			{
				if( bNetOwner || bReplicateWeapon )
				{
					DOREP(Pawn,Weapon);
				}

				DOREP(Pawn,PlayerReplicationInfo);
				DOREP(Pawn,Health);

				if ( (bNetOwner || ((APawn*)Recent)->Controller) && (!Controller || Map->CanSerializeObject(Controller)) )
				{
					DOREP(Pawn,Controller);
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
				}

				if ( !bNetOwner )
				{
					DOREP(Pawn,bIsCrouched);
				}

				DOREP(Pawn,HitDamageType);
				DOREP(Pawn,TakeHitLocation);
				DOREP(Pawn,bSimulateGravity);
				DOREP(Pawn,bIsWalking);
				DOREP(Pawn,bIsTyping);
				DOREP(Pawn,DrivenVehicle);
				DOREP(Pawn,HeadScale);

				if ( !bTearOff )
				{
					DOREP(Pawn,TearOffMomentum);
				}
			}
		}
	}
	return Ptr;
}

INT* AController::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( bNetDirty && (Role==ROLE_Authority) )
		{
			DOREP(Controller,PlayerReplicationInfo);
			DOREP(Controller,Pawn);
		}
	}
	return Ptr;
}

INT* APlayerController::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( bNetOwner && (Role==ROLE_Authority) )
		{
			if ( (GetViewTarget() != Pawn) && ViewTarget->GetAPawn() )
			{
				DOREP(PlayerController,TargetViewRotation);
				DOREP(PlayerController,TargetEyeHeight);
				DOREP(PlayerController,TargetWeaponViewOffset);
			}
		}
	}
	return Ptr;
}

INT* APhysicsVolume::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
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
		if ( (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(PhysicsVolume,Gravity);
		}
	}
	return Ptr;
}

INT* ALevelInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	// only replicate needed actor properties
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(LevelInfo,Pauser);
			DOREP(LevelInfo,TimeDilation);
			DOREP(LevelInfo,DefaultGravity);
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
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(PlayerReplicationInfo,Score);
			DOREP(PlayerReplicationInfo,Deaths);
			DOREP(PlayerReplicationInfo,bHasFlag);

			if ( !bNetOwner )
			{
			DOREP(PlayerReplicationInfo,Ping);
				DOREP(PlayerReplicationInfo,PacketLoss);
			}
 			DOREP(PlayerReplicationInfo,PlayerLocationHint);
			DOREP(PlayerReplicationInfo,PlayerName);
			DOREP(PlayerReplicationInfo,Team);
			DOREP(PlayerReplicationInfo,TeamID);
			DOREP(PlayerReplicationInfo,bAdmin);
			DOREP(PlayerReplicationInfo,bIsFemale);
			DOREP(PlayerReplicationInfo,bIsSpectator);
			DOREP(PlayerReplicationInfo,bOnlySpectator);
			DOREP(PlayerReplicationInfo,bWaitingPlayer);
			DOREP(PlayerReplicationInfo,bReadyToPlay);
			DOREP(PlayerReplicationInfo,bOutOfLives);
			DOREP(PlayerReplicationInfo,StartTime);
			if ( bNetInitial )
			{
				DOREP(PlayerReplicationInfo,PlayerID);
				DOREP(PlayerReplicationInfo,bBot);
				DOREP(PlayerReplicationInfo,StartTime);
			}
		}
	}
	return Ptr;
}
INT* AGameReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(GameReplicationInfo,bStopCountDown);
			DOREP(GameReplicationInfo,bMatchHasBegun);
			DOREP(GameReplicationInfo,Winner);
			DOREP(GameReplicationInfo,MatchID);
			DOREPARRAY(GameReplicationInfo,CarriedObjectState);
			DOREPARRAY(GameReplicationInfo,Teams);
			if ( bNetInitial )
			{
				DOREP(GameReplicationInfo,GameName);
				DOREP(GameReplicationInfo,GameClass);
				DOREP(GameReplicationInfo,bTeamGame);
				DOREP(GameReplicationInfo,RemainingTime);
				DOREP(GameReplicationInfo,ElapsedTime);
				DOREP(GameReplicationInfo,ServerName);
				DOREP(GameReplicationInfo,ShortName);
				DOREP(GameReplicationInfo,AdminName);
				DOREP(GameReplicationInfo,AdminEmail);
				DOREP(GameReplicationInfo,ServerRegion);
				DOREP(GameReplicationInfo,MessageOfTheDay);
				DOREP(GameReplicationInfo,GoalScore);
				DOREP(GameReplicationInfo,TimeLimit);
				DOREP(GameReplicationInfo,MaxLives);
			}
			else
				DOREP(GameReplicationInfo,RemainingMinute);
		}
	}
	return Ptr;
}
INT* ATeamInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
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
	}
	return Ptr;
}

INT* APickupFactory::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		DOREP(PickupFactory,bPickupHidden);
		if ( bOnlyReplicateHidden )
		{
			DOREP(Actor,bHidden);
			if ( bNetInitial )
				DOREP(Actor,Rotation);
		}
		else
			Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	}
	return Ptr;

}

INT* AInventory::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( bNetOwner && (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(Inventory,Inventory);
		}
	}
	return Ptr;
}

INT* AWeapon::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);

	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( Role == ROLE_Authority )
		{
			if ( bNetDirty )
			{
				if ( !bNetOwner )
				{
					DOREP(Weapon,ShotCount);
				}
				DOREP(Weapon,CurrentFireMode);
			}
		}
	}

	return Ptr;
}

INT* ACarriedObject::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( Role == ROLE_Authority )
		{
			DOREP(CarriedObject,bHome);
			DOREP(CarriedObject,bHeld);
			DOREP(CarriedObject,HolderPRI);
		}
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
static FVector				SavedDrawScale3D;
static FLOAT				SavedHeadScale;
static FCompressedPosition	SavedPawnPosition;
static FLOAT				SavedGravity;
static AVehicle*			SavedDrivenVehicle;
static UBOOL				SavedHardAttach;
static AWeapon*				SavedWeapon;
static UBOOL				SavedbIsCrouched;

// Weapon
static INT		SavedShotCount;

//
// Net priority.
//
FLOAT AActor::GetNetPriority( FVector& ViewPos, FVector& ViewDir, AActor* Sent, FLOAT Time, FLOAT Lag )
{
	if ( bAlwaysRelevant )
		return NetPriority * Time * ::Min(NetUpdateFrequency * 0.1f, 1.f);
	else
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
	SavedCollision  = bCollideActors;
	SavedDrawScale	= DrawScale;
	SavedDrawScale3D= DrawScale3D;
}

void APawn::PreNetReceive()
{
	SavedPawnPosition	= PawnPosition;
	SavedHeadScale		= HeadScale;
	SavedWeapon			= Weapon;
	AActor::PreNetReceive();
}

void ALevelInfo::PreNetReceive()
{
	SavedGravity = DefaultGravity;
	AActor::PreNetReceive();
}

void AWeapon::PreNetReceive()
{
	SavedShotCount = ShotCount;

}

void AWeapon::PostNetReceive()
{
	if ( ShotCount != SavedShotCount )
	{
		eventPlayFireEffects( CurrentFireMode );
	}
}


void ALevelInfo::PostNetReceive()
{
	if ( DefaultGravity != SavedGravity )
	{
		GetDefaultPhysicsVolume()->Gravity.Z = DefaultGravity;
	}

	AActor::PostNetReceive();
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
	Exchange ( DrawScale3D, SavedDrawScale3D   );
	ExchangeB( bHardAttach, SavedHardAttach );

	if( bCollideActors!=SavedCollision )
		SetCollision( SavedCollision, bBlockActors );
	if( Location!=SavedLocation )
		PostNetReceiveLocation();
	if( Rotation!=SavedRotation )
	{
		FCheckResult Hit;
		GetLevel()->MoveActor( this, FVector(0,0,0), SavedRotation, Hit, 0, 0, 0, 1 );
	}
	if ( DrawScale!=SavedDrawScale )
		SetDrawScale(SavedDrawScale);
	if ( DrawScale3D!=SavedDrawScale3D )
		SetDrawScale3D(SavedDrawScale3D);
	UBOOL bBaseChanged = ( Base!=SavedBase );
	if( bBaseChanged )
	{
		bHardAttach = SavedHardAttach;

		// Base changed.
		/* why sending a bump notification??
		if( SavedBase )
		{
			eventBump( SavedBase );
			SavedBase->eventBump( this );
		}
		*/
		UBOOL bRelativeLocationChanged = (SavedRelativeLocation != RelativeLocation);
		UBOOL bRelativeRotationChanged = (SavedRelativeRotation != RelativeRotation);
		SetBase( SavedBase );
		if ( !bRelativeLocationChanged )
			SavedRelativeLocation = RelativeLocation;
		if ( !bRelativeRotationChanged )
			SavedRelativeRotation = RelativeRotation;
	}
	else
	{
		// If the base didn't change, but the 'hard attach' flag did, re-base this actor.
		if(SavedHardAttach != bHardAttach)
		{
			bHardAttach = SavedHardAttach;

			SetBase( NULL );
			SetBase( Base );
		}
	}

	if ( Base && !Base->bWorldGeometry )
	{
		if ( bBaseChanged || (RelativeLocation != SavedRelativeLocation) )
		{
			GetLevel()->FarMoveActor( this, Base->Location + SavedRelativeLocation, 0, 1, 1 );
			RelativeLocation = SavedRelativeLocation;
		}
		if ( bBaseChanged || (RelativeRotation != SavedRelativeRotation) )
		{
			FCheckResult Hit;
			FRotator NewRotation = (FRotationMatrix(SavedRelativeRotation) * FRotationMatrix(Base->Rotation)).Rotator();
			GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit, 0, 0, 0, 1 );
		}
	}
	bJustTeleported = 0;
}

void AActor::PostNetReceiveLocation()
{
	if( Physics == PHYS_Interpolating )
	{
		physInterpolatingCorrectPosition( SavedLocation );
	}
	else
	{
		GetLevel()->FarMoveActor( this, SavedLocation, 0, 1, 1 );
	}
}

void APawn::PostNetReceive()
{
	if ( (SavedPawnPosition.Location != PawnPosition.Location)
		|| (SavedPawnPosition.Rotation != PawnPosition.Rotation)
		|| (SavedPawnPosition.Velocity != PawnPosition.Velocity) )
	{
		Location = PawnPosition.Location;
		Rotation = PawnPosition.Rotation;
		Velocity = PawnPosition.Velocity;
	}
	
	if ( DrivenVehicle != SavedDrivenVehicle )
	{
		if ( DrivenVehicle )
		{
			Exchange ( Location,        SavedLocation  );
			GetLevel()->FarMoveActor( this, SavedLocation, 0, 1, 1 );
			Exchange ( Base, SavedBase );
			eventStartDriving(DrivenVehicle);
			SavedBase = Base;
			SavedLocation = Location;
			SavedRotation = Rotation;
			SavedRelativeLocation = RelativeLocation;
			SavedRelativeRotation = RelativeRotation;
		}
		else
		{
			FVector NewLoc = Location;
//FIXMESTEVE			Location = ColLocation;
//FIXMESTEVE			SavedLocation = ColLocation;
			eventStopDriving(SavedDrivenVehicle);
//FIXMESTEVE			if ( Location == ColLocation )
//FIXMESTEVE				Location = NewLoc;
		}
	}

	// fire event when weapon changes to play weapon switching effects
	if ( Weapon != SavedWeapon )
	{
		eventPlayWeaponSwitch( SavedWeapon, Weapon );
		SavedWeapon = Weapon;
	}

	// Fire event when Pawn starts or stops crouching, to update collision and mesh offset.
	if( bIsCrouched != SavedbIsCrouched )
	{
		if( bIsCrouched )
		{
			Crouch(0,1);
		}
		else
		{
			UnCrouch(0,1);
		}
		SavedbIsCrouched = bIsCrouched; 
	}

	AActor::PostNetReceive();

	if ( SavedHeadScale != HeadScale )
		eventSetHeadScale(HeadScale);
}

void APawn::PostNetReceiveLocation()
{
	if ( (Physics == PHYS_RigidBody) || (Physics == PHYS_Articulated) )
	{
		AActor::PostNetReceiveLocation();
	}
	else if( Role == ROLE_SimulatedProxy )
	{
		FCheckResult Hit(1.f);
		if ( GetLevel()->EncroachingWorldGeometry(Hit,SavedLocation,GetCylinderExtent(),Level) )
		{
			APawn* DefaultPawn = (APawn*)(GetClass()->GetDefaultActor());

			if ( CylinderComponent->CollisionRadius == DefaultPawn->CylinderComponent->CollisionRadius )
				SetCollisionSize(CylinderComponent->CollisionRadius - 1.f, CylinderComponent->CollisionHeight - 1.f);
			bSimGravityDisabled = true;
		}
		else if ( Velocity.IsZero() )
			bSimGravityDisabled = true;
		else
		{
			SavedLocation.Z += 2.f;
			bSimGravityDisabled = false;
		}
		FVector OldLocation = Location;
		GetLevel()->FarMoveActor( this, SavedLocation, 0, 1, 1 );
		if ( !bSimGravityDisabled )
	{
		// smooth out movement of other players to account for frame rate induced jitter
		// look at whether location is a reasonable approximation already
		// if so only partially correct
			FVector Dir = OldLocation - Location;
			FLOAT StartError = Dir.Size();
			if ( StartError > 4.f )
		{
				StartError = ::Min(0.5f * StartError,CylinderComponent->CollisionRadius);
				Dir.Normalize();
				moveSmooth(StartError * Dir);
			}
		}

	}
	else
		AActor::PostNetReceiveLocation();
}

/*-----------------------------------------------------------------------------
	APlayerPawn implementation.
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
	UNetDriver* Driver = GetLevel()->NetDriver;
	if( (ClientCap>=2600) && Driver && Driver->ServerConnection )
		Player->CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp( ClientCap, 1800, Driver->MaxClientRate );

	// initialize the input system only if local player
	if ( Cast<ULocalPlayer>(InPlayer) )
	{
		eventInitInputSystem();
		Console->InitExecution();
	}

}

/*-----------------------------------------------------------------------------
	AActor.
-----------------------------------------------------------------------------*/

void AActor::Destroy()
{
	TermRBPhys();

	UObject::Destroy();
}

void AActor::PostLoad()
{
	Super::PostLoad();

	if( GetClass()->ClassFlags & CLASS_Localized )
		LoadLocalized();

	// check for empty Attached entries
	for ( INT i=0; i<Attached.Num(); i++ )
		if ( (Attached(i) == NULL) || (Attached(i)->Base != this) || Attached(i)->bDeleteMe )
		{
			Attached.Remove(i);
			i--;
		}

	// check for empty Touching entries
	for ( INT i=0; i<Touching.Num(); i++ )
		if ( (Touching(i) == NULL) || Touching(i)->bDeleteMe )
		{
			Touching.Remove(i);
			i--;
		}

	// Set our components' owners so InitRBPhys works.
	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Components.Num();ComponentIndex++)
	{
		if(Components(ComponentIndex))
		{
			Components(ComponentIndex)->Owner = this;
		}
	}

	// Check/warning when loading actors in editor. Should never load bDeleteMe Actors!
	if(GIsEditor && bDeleteMe)
	{
		debugf( TEXT("Loaded Actor (%s) with bDeleteMe == true"), GetName() );
	}
}

void AActor::ProcessEvent( UFunction* Function, void* Parms, void* Result )
{
	if( Level->bBegunPlay && !GIsGarbageCollecting )
		Super::ProcessEvent( Function, Parms, Result );
}

void AActor::PreEditChange()
{
	ClearComponents();
}

void AActor::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	for ( INT i=0; i<Attached.Num(); i++ )
		if ( Attached(i) == NULL )
		{
			Attached.Remove(i);
			i--;
		}
	UpdateComponents();
}

// Called before we have done undo things.
void AActor::PreEditUndo()
{
	ClearComponents();

}

// Called after we have done undo things.
void AActor::PostEditUndo()
{
	UpdateComponents();

}

// There might be a constraint to this actor, so we iterate over all actors to check. If there is, update ref frames.
void AActor::PostEditMove()
{
	ULevel* level = GetLevel();
	for( INT iActor=level->iFirstDynamicActor; iActor<level->Actors.Num(); iActor++ )
	{
		if( level->Actors(iActor) )
		{
			ARB_ConstraintActor* ConActor = Cast<ARB_ConstraintActor>( level->Actors(iActor) );
			if(ConActor)
			{
				if(ConActor->ConstraintActor1 == this || ConActor->ConstraintActor2 == this)
				{
					ConActor->PostEditMove();
				}
			}
		}
	}

}


//
// Get the collision cylinder extent to use when moving this actor through the level (ie. just looking at CollisionComponent)
//
FVector AActor::GetCylinderExtent() const
{
	UCylinderComponent* CylComp = Cast<UCylinderComponent>(CollisionComponent);
	
	if(CylComp)
		return FVector(CylComp->CollisionRadius, CylComp->CollisionRadius, CylComp->CollisionHeight);
	else
		return FVector(0,0,0);

}

//
// Get height/radius of big cylinder around this actors colliding components.
//
void AActor::GetBoundingCylinder(FLOAT& CollisionRadius, FLOAT& CollisionHeight)
{
	FBox Box = GetComponentsBoundingBox();
	FVector BoxExtent = Box.GetExtent();

	CollisionHeight = BoxExtent.Z;
	CollisionRadius = appSqrt( (BoxExtent.X * BoxExtent.X) + (BoxExtent.Y * BoxExtent.Y) );

}

//
// Set the actor's collision properties.
//
void AActor::SetCollision
(
	UBOOL NewCollideActors,
	UBOOL NewBlockActors
)
{
	UBOOL OldCollideActors = bCollideActors;

	// Untouch everything if we're turning collision off.
	if( bCollideActors && !NewCollideActors )
	{
		for( int i=0; i<Touching.Num(); )
		{
			if( Touching(i) )
				Touching(i)->EndTouch( this, 0 );
			else
				i++;
		}
	}

	// Set properties.
	ClearComponents();
	bCollideActors = NewCollideActors;
	bBlockActors   = NewBlockActors;
	UpdateComponents();

	// Touch.
	if( NewCollideActors && !OldCollideActors )
	{
		FMemMark Mark(GMem);
		FCheckResult* FirstHit = GetLevel()->Hash ? GetLevel()->Hash->ActorEncroachmentCheck( GMem, this, Location, Rotation, TRACE_AllColliding ) : NULL;	
		for( FCheckResult* Test = FirstHit; Test; Test=Test->GetNext() )
			if(	Test->Actor!=this &&
				!Test->Actor->IsBasedOn(this) &&
				Test->Actor != Level )
			{
				if( !IsBlockedBy(Test->Actor,Test->Component) )
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

					BeginTouch( Test->Actor, HitLocation, NormalDir );
				}
			}						
		Mark.Pop();
	}

	bNetDirty = true; //for network replication

}

//
// Set the collision cylinder size (if there is one).
//
void AActor::SetCollisionSize( FLOAT NewRadius, FLOAT NewHeight )
{
	// JAG_COLRADIUS_HACK
	UCylinderComponent* CylComp = Cast<UCylinderComponent>(CollisionComponent);

	if(CylComp)
		CylComp->SetSize(NewHeight, NewRadius);

	/* FIXMESTEVE - commented out for UT2003, until I have time to get it in w/ no issues
	// check that touching array is correct.
	if ( !bTest )
		GetLevel()->CheckEncroachment(this,Location,Rotation,true);
	*/
	bNetDirty = true;	// for network replication
}

void AActor::SetDrawScale( FLOAT NewScale )
{
	DrawScale = NewScale;
	UpdateComponents();

	bNetDirty = true;	// for network replication
}

void AActor::SetDrawScale3D( FVector NewScale3D )
{
	DrawScale3D = NewScale3D;
	UpdateComponents();

	bNetDirty = true;	// for network replication
}

void AActor::SetPrePivot( FVector NewPrePivot )
{
	PrePivot = NewPrePivot;
	UpdateComponents();

	bNetDirty = true;
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
FBox AActor::GetComponentsBoundingBox(UBOOL bNonColliding)
{
	FBox Box(0);

	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)this->Components.Num();ComponentIndex++)
	{
		UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(this->Components(ComponentIndex));

		// Only use collidable components to find collision bounding box.
		if( primComp && (bNonColliding || primComp->CollideActors) )
			Box += primComp->Bounds.GetBox();
	}

	return Box;

}

//
// Return whether this actor overlaps another.
// Called normally from MoveActor, to see if we should 'untouch' things.
// Normally - the only things that can overlap an actor are volumes.
// However, we also use this test during ActorEncroachmentCheck, so we support
// Encroachers (ie. movers) overlapping actors.
//
UBOOL AActor::IsOverlapping( AActor* Other, FCheckResult* Hit )
{
	checkSlow(Other!=NULL);

	if ( (this->IsBrush() && Other->IsBrush()) || (Other == Level) )
	{
		// We cannot detect whether these actors are overlapping so we say they aren't.
		return 0;
	}

	// Things dont overlap themselves
	if(this == Other)
		return 0;

	// Things that do encroaching (movers, karma actors etc.) can't encroach each other!
	if(this->IsEncroacher() && Other->IsEncroacher())
		return 0;

	// Things that are joined together dont overlap.
	if( this->IsBasedOn(Other) || Other->IsBasedOn(this) )
		return 0;

	// Can't overlap actors with collision turned off.
	if( !this->bCollideActors || !Other->bCollideActors )
		return 0;

	// If one only affects pawns, and the other isn't a pawn, dont check.
	if(this->bOnlyAffectPawns && !Other->GetAPawn() )
		return 0;

	if(Other->bOnlyAffectPawns && !this->GetAPawn() )
		return 0;

	// Now have 2 actors we want to overlap, so we have to pick which one to treat as a box and test against the PrimitiveComponents of the other.
	AActor* PrimitiveActor = NULL;
	AActor* BoxActor = NULL;

	// For volumes, test the bounding box against the volume primitive.
	if(this->IsA(AVolume::StaticClass()))
	{
		PrimitiveActor = this;
		BoxActor = Other;
	}
	else if(Other->IsA(AVolume::StaticClass()))
	{
		PrimitiveActor = Other;
		BoxActor = this;
	}
	// For arituclated things, we have to test the overall bounding box against the other thing.
	// JTODO: Once we have point-check against PhysicsAssets we can remove this!
	else if(this->Physics == PHYS_Articulated)
	{
		PrimitiveActor = Other;
		BoxActor = this;
	}
	else if(Other->Physics == PHYS_Articulated)
	{
		PrimitiveActor = this;
		BoxActor = Other;
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
	// If none of these cases, try a cylinder/cylinder overlap.
	else
	{
		// JAG_COLRADIUS_HACK
		// Fallback - see if collision component cylinders are overlapping.
		UCylinderComponent* CylComp1 = Cast<UCylinderComponent>(this->CollisionComponent);
		UCylinderComponent* CylComp2 = Cast<UCylinderComponent>(Other->CollisionComponent);

		if(CylComp1 && CylComp2)
		{
		return
				( (Square(Location.Z - Other->Location.Z) < Square(CylComp1->CollisionHeight + CylComp2->CollisionHeight))
			&&	(Square(Location.X - Other->Location.X) + Square(Location.Y - Other->Location.Y)
				< Square(CylComp1->CollisionRadius + CylComp2->CollisionRadius)) );
		}
		else
		{
			return 0;
		}
	}
	
	check(BoxActor);
	check(PrimitiveActor);
	check(BoxActor != PrimitiveActor);

	if(!BoxActor->CollisionComponent)
		return 0;
		
	// Check bounding box of collision component against each colliding PrimitiveComponent.
	FBox BoxActorBox = BoxActor->CollisionComponent->Bounds.GetBox();
	//GetLevel()->LineBatcher->DrawWireBox( BoxActorBox, FColor(255,255,0) );

	// If we failed to get a valid bounding box from the Box actor, we can't get an overlap.
	if(!BoxActorBox.IsValid)
		return 0;

	FVector BoxCenter, BoxExtent;
	BoxActorBox.GetCenterAndExtents(BoxCenter, BoxExtent);

	// If we were not supplied an FCheckResult, use a temp one.
	FCheckResult TestHit;
	if(Hit==NULL)
		Hit = &TestHit;

	
#if 0 // DEBUGGING: Time how long the point checks take, and print if its more than a millisecond.
	DWORD Time=0;
	clock(Time);
#endif

	// Check box against each colliding primitive component.
	UBOOL HaveHit = 0;
	for(UINT ComponentIndex = 0; ComponentIndex < (UINT)PrimitiveActor->Components.Num(); ComponentIndex++)
	{
		UPrimitiveComponent* primComp = Cast<UPrimitiveComponent>(PrimitiveActor->Components(ComponentIndex));

		if(primComp && primComp->CollideActors && primComp->BlockNonZeroExtent)
		{
			HaveHit = ( primComp->PointCheck( *Hit, BoxCenter, BoxExtent ) == 0 );
			if(HaveHit)
				break;
			}
		}

#if 0
	unclock(Time);
	FLOAT mSec = Time * GSecondsPerCycle * 1000.f;
	if(mSec > 1.f)
		debugf(TEXT("IOL: Testing: P:%s - B:%s Time: %f"), PrimitiveActor->GetName(), BoxActor->GetName(), mSec );
#endif

	return HaveHit;

}


/*-----------------------------------------------------------------------------
	Actor touch minions.
-----------------------------------------------------------------------------*/

static UBOOL TouchTo( AActor* Actor, AActor* Other, const FVector &HitLocation, const FVector &HitNormal)
{
	check(Actor);
	check(Other);
	check(Actor!=Other);

	for ( INT j=0; j<Actor->Touching.Num(); j++ )
	  if ( Actor->Touching(j) == Other )
		return 1;

	// check for touch sequence events
	USeqEvent_Touch *touchEvent = NULL;
	while ((touchEvent = Cast<USeqEvent_Touch>(Actor->GetEventOfClass(USeqEvent_Touch::StaticClass(),touchEvent))) != NULL)
	{
		touchEvent->CheckActivate(Actor,Other);
	}

	// Make Actor touch TouchActor.
	Actor->Touching.AddItem(Other);
	Actor->eventTouch( Other, HitLocation, HitNormal );

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
void AActor::BeginTouch( AActor* Other, const FVector &HitLocation, const FVector &HitNormal)
{
	// Perform reflective touch.
	if ( TouchTo( this, Other, HitLocation, HitNormal ) )
		TouchTo( Other, this, HitLocation, HitNormal );

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
	USeqEvent_UnTouch *untouchEvent = NULL;
	while ((untouchEvent = Cast<USeqEvent_UnTouch>(GetEventOfClass(USeqEvent_UnTouch::StaticClass(),untouchEvent))) != NULL)
	{
		untouchEvent->CheckActivate(this,Other);
	}
	untouchEvent = NULL;
	while ((untouchEvent = Cast<USeqEvent_UnTouch>(Other->GetEventOfClass(USeqEvent_UnTouch::StaticClass(),untouchEvent))) != NULL)
	{
		untouchEvent->CheckActivate(Other,this);
	}

	if ( Other->Touching.FindItem(this,i) )
	{
		Other->eventUnTouch( this );
		Other->Touching.RemoveItem(this);
	}
}

/*-----------------------------------------------------------------------------
	AActor member functions.
-----------------------------------------------------------------------------*/

//
// Destroy the actor.
//
void AActor::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
}

/*-----------------------------------------------------------------------------
	Relations.
-----------------------------------------------------------------------------*/

//
// Change the actor's owner.
//
void AActor::SetOwner( AActor *NewOwner )
{
	// Sets this actor's parent to the specified actor.
	if( Owner != NULL )
		Owner->eventLostChild( this );

	Owner = NewOwner;

	if( Owner != NULL )
		Owner->eventGainedChild( this );
	bNetDirty = true;
}

//
// Change the actor's base.
//
void AActor::SetBase( AActor* NewBase, FVector NewFloor, int bNotifyActor, USkeletalMeshComponent* SkelComp, FName BoneName )
{
	//debugf(TEXT("SetBase %s -> %s"),GetName(),NewBase ? NewBase->GetName() : TEXT("NULL"));

	// SkelComp should always be owned by the Actor we are trying to base on.
	check(!SkelComp || SkelComp->Owner == NewBase);

	// Verify no recursion.
	for( AActor* Loop=NewBase; Loop!=NULL; Loop=Loop->Base )
		if ( Loop == this )
			return;

	if( NewBase != Base )
	{
		// Notify old base, unless it's the level or terrain (but not movers).
		if( Base && !Base->bWorldGeometry )
		{
			Base->Attached.RemoveItem(this);
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
			if(SkelComp)
			{
				BoneIndex = SkelComp->MatchRefBone(BoneName);
				if(BoneIndex == INDEX_NONE)
				{
					debugf( TEXT("AActor::SetBase : Bone (%s) not found!"), *BoneName );					
				}
			}

			// Bone exists, so remember offset from it.
			if(BoneIndex != INDEX_NONE)
			{
				FMatrix BaseTM = SkelComp->GetBoneMatrix(BoneIndex);
				BaseTM.RemoveScaling();
				FMatrix BaseInvTM = BaseTM.Inverse();

				FMatrix ChildTM(Location, Rotation);

				FMatrix HardRelMatrix =  ChildTM * BaseInvTM;
				RelativeLocation = HardRelMatrix.GetOrigin();
				RelativeRotation = HardRelMatrix.Rotator();

				BaseSkelComponent = SkelComp;
				BaseBoneName = BoneName;
			}
			// Calculate the transform of this actor relative to its base.
			else if(bHardAttach)
			{
				FMatrix BaseInvTM = FTranslationMatrix(-Base->Location) * FInverseRotationMatrix(Base->Rotation);
				FMatrix ChildTM(Location, Rotation);

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
			eventBaseChange();
	}

}

//
// Determine if BlockingActor should block actors of the given class.
// This routine needs to be reflexive or else it will create funky
// results, i.e. A->IsBlockedBy(B) <-> B->IsBlockedBy(A).
//
inline UBOOL AActor::IsBlockedBy( const AActor* Other, const UPrimitiveComponent* Primitive ) const
{
	checkSlow(this!=NULL);
	checkSlow(Other!=NULL);

	if(Primitive && !Primitive->BlockActors)
		return 0;

	if( Other->bWorldGeometry )
		return bCollideWorld && Other->bBlockActors;
	else if ( Other->IgnoreBlockingBy((AActor *)this) )
		return false;
	else if ( IgnoreBlockingBy((AActor*)Other) )
		return false;
	else if( Other->IsBrush() || Other->IsEncroacher() )
		return bCollideWorld && Other->bBlockActors;
	else if ( IsBrush() || IsEncroacher() )
		return Other->bCollideWorld && bBlockActors;
	else
		return Other->bBlockActors && bBlockActors;

}

UBOOL AActor::IgnoreBlockingBy( const AActor *Other ) const
{
	if ( bIgnoreEncroachers && Other->IsEncroacher() )
		return true;
	return false;
}

UBOOL AVehicle::IgnoreBlockingBy( const AActor *Other ) const
{
	if ( Other->bIgnoreVehicles )
		return true;
	if ( bIgnoreEncroachers && Other->IsEncroacher() )
		return true;
	return false;
}

void APawn::SetBase( AActor* NewBase, FVector NewFloor, int bNotifyActor )
{
	Floor = NewFloor;
	Super::SetBase(NewBase,NewFloor,bNotifyActor);
}

/** Add a data point to a line on the global debug chart. */
void AActor::ChartData(const FString& DataName, FLOAT DataValue)
{
	if(GStatChart)
	{
		// Make graph line name by concatenating actor name with data name.
		FString LineName = FString::Printf(TEXT("%s_%s"), GetName(), *DataName);

		GStatChart->AddDataPoint(LineName, DataValue);
	}
}


/*-----------------------------------------------------------------------------
	Special editor support.
-----------------------------------------------------------------------------*/

AActor* AActor::GetHitActor()
{
	return this;
}

// Determines if this actor is hidden in the editor viewports or not.

UBOOL AActor::IsHiddenEd()
{
	// If any of the standard hide flags are set, return 1

	if( bHiddenEd || bHiddenEdGroup )
		return 1;

	// If this actor is part of a layer and that layer is hidden, return 1

	if( Layer && Layer->bVisible == 0 )
		return 1;

	// Otherwise, it's visible

	return 0;
}

/*---------------------------------------------------------------------------------------
	Brush class implementation.
---------------------------------------------------------------------------------------*/

void ABrush::InitPosRotScale()
{
	check(BrushComponent);
	check(Brush);
	
	Location  = FVector(0,0,0);
	Rotation  = FRotator(0,0,0);
	PrePivot  = FVector(0,0,0);

}
void ABrush::PostLoad()
{
	Super::PostLoad();
}

FColor ABrush::GetWireColor()
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

	return Color;
}

/*---------------------------------------------------------------------------------------
	Tracing check implementation.
	ShouldTrace() returns true if actor should be checked for collision under the conditions
	specified by traceflags
---------------------------------------------------------------------------------------*/

UBOOL AActor::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	// Check bOnlyAffectPawns flag. APawn has its own ShouldTrace, so this actor can't be one.
	if( SourceActor && SourceActor->bOnlyAffectPawns )
		return false;

	if( this->bOnlyAffectPawns && SourceActor && !SourceActor->GetAPawn() )
		return false;

	if ( bWorldGeometry )
		return (TraceFlags & TRACE_LevelGeometry);
	else if( TraceFlags & TRACE_Others )
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			if( bProjTarget || bBlockActors )
				return true;
		}
		else if ( TraceFlags & TRACE_Blocking )
		{
			if ( SourceActor && SourceActor->IsBlockedBy(this,Primitive) )
				return true;
		}
		else
			return true;
	}
		
	return false;
}

UBOOL AVolume::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( SourceActor && SourceActor->bOnlyAffectPawns )
		return false;

	if( this->bOnlyAffectPawns && SourceActor && !SourceActor->GetAPawn() )
		return false;

	if ( bWorldGeometry && (TraceFlags & TRACE_LevelGeometry) )
		return true;
	if ( TraceFlags & TRACE_Volumes )
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			if( bProjTarget || bBlockActors )
				return true;
			if ( TraceFlags & TRACE_Water )
			{
				APhysicsVolume *PV = Cast<APhysicsVolume>(this);
				if ( PV && PV->bWaterVolume )
					return true;
			}
		}
		else if ( TraceFlags & TRACE_Blocking )
		{
			if ( SourceActor && SourceActor->IsBlockedBy(this,Primitive) )
				return true;
		}
		else
			return true;
	}
	return false;
}

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

