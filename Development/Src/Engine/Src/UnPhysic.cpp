/*=============================================================================
	UnPhysic.cpp: Actor physics implementation

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "EnginePhysicsClasses.h"

#if 0
	#define DEBUGPHYSONLY(x)		{ ##x }
#else
	#define DEBUGPHYSONLY(x)
#endif

/** Internal. */
FVector CalcAngularVelocity(FRotator const& OldRot, FRotator const& NewRot, FLOAT DeltaTime)
{
	FVector RetAngVel(0.f);

	if (OldRot != NewRot)
	{
		FLOAT const InvDeltaTime = 1.f / DeltaTime;
		FQuat const DeltaQRot = (NewRot - OldRot).Quaternion();
		
		FVector Axis; 
		FLOAT Angle;
		DeltaQRot.ToAxisAndAngle(Axis, Angle);

		RetAngVel = Axis * Angle * InvDeltaTime;
		check(!RetAngVel.ContainsNaN());
	}

	return RetAngVel;
}


void AActor::execSetPhysics( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE(NewPhysics);
	P_FINISH;

	// Script code call stack of what is setting physics modes
	DEBUGPHYSONLY
	(
		if( GetAPawn() )
		{
			debugf(TEXT("*** Setting Physics: %d on %s from script.\n\r %s"), NewPhysics, *GetName(), *Stack.GetStackTrace() );
		}
	)

	setPhysics(NewPhysics);
}

void AActor::AutonomousPhysics(FLOAT DeltaSeconds)
{
	// @TODO: Evaluate more carefully to find which parts of the simulated move do and do not need to be done for PHYS_RigidBody (TTP 71329)
	if(Physics == PHYS_RigidBody && Role == ROLE_Authority)
	{
		return;
	}

	// round acceleration to be consistent with replicated acceleration
	Acceleration.X = 0.1f * int(10 * Acceleration.X);
	Acceleration.Y = 0.1f * int(10 * Acceleration.Y);
	Acceleration.Z = 0.1f * int(10 * Acceleration.Z);

	// Perform physics.
	if( Physics!=PHYS_None )
		performPhysics( DeltaSeconds );
}

//======================================================================================
// Gravity Accessors

/**
 * Returns the Z component of the current world gravity and initializes it to the default
 * gravity if called for the first time.
 *
 * @return Z component of current world gravity.
 */
FLOAT AWorldInfo::GetGravityZ()
{
	if ( WorldGravityZ == 0.f )
	{
		// initialize
		if ( GlobalGravityZ != 0.f )
			WorldGravityZ = GlobalGravityZ;
		else
			WorldGravityZ = DefaultGravityZ;
	}
	return WorldGravityZ;
}

/** returns terminal velocity (max speed while falling) for this actor.  Unless overridden, it returns the TerminalVelocity of the PhysicsVolume in which this actor is located.
*/
FLOAT AActor::GetTerminalVelocity()
{
	return PhysicsVolume ? PhysicsVolume->TerminalVelocity : ((APhysicsVolume *)(APhysicsVolume::StaticClass()->GetDefaultActor()))->TerminalVelocity;
}

/**
 * Returns the Z component of the current world gravity and initializes it to the default
 * gravity if called for the first time.
 *
 * @return Z component of current world gravity.
 */
FLOAT AWorldInfo::GetRBGravityZ()
{
	return GetGravityZ() * RBPhysicsGravityScaling;
}

FLOAT APhysicsVolume::GetGravityZ()
{
	return GWorld->GetGravityZ();
}

FVector APhysicsVolume::GetZoneVelocityForActor(AActor *TheActor)
{
	return ZoneVelocity;
}

FLOAT AActor::GetGravityZ()
{
	if ( Physics == PHYS_RigidBody )
	{
		if ( PhysicsVolume )
		{
			return WorldInfo->RBPhysicsGravityScaling * PhysicsVolume->GetVolumeRBGravityZ();
		}
		return WorldInfo->RBPhysicsGravityScaling * GWorld->GetGravityZ();
	}
	else
	{
		if ( PhysicsVolume )
		{
			return PhysicsVolume->GetGravityZ();
		}
		return GWorld->GetGravityZ();
	}
}

//======================================================================================
// smooth movement (no real physics)

void AActor::execMoveSmooth( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Delta);
	P_FINISH;

	bJustTeleported = 0;
	int didHit = moveSmooth(Delta);

	*(DWORD*)Result = didHit;
}

UBOOL AActor::moveSmooth(FVector const& Delta)
{
	// MT-> HACKERY!  we have collision with actors disable din navmeshwalking for speed, so do the same on clients
	if( Physics == PHYS_NavMeshWalking )
	{
		Location += Delta;
		return FALSE;
	}
	else
	{
		FCheckResult Hit(1.f);
		UBOOL didHit = GWorld->MoveActor( this, Delta, Rotation, 0, Hit );
		if (Hit.Time < 1.f)
		{
			FVector GravDir = FVector(0,0,-1);
			FVector DesiredDir = Delta.SafeNormal();

			FLOAT UpDown = GravDir | DesiredDir;
			if ( (Abs(Hit.Normal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) )
			{
				stepUp(GravDir, DesiredDir, Delta * (1.f - Hit.Time), Hit);
			}
			else
			{
				FVector Adjusted = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
				if( (Delta | Adjusted) >= 0 )
				{
					FVector OldHitNormal = Hit.Normal;
					DesiredDir = Delta.SafeNormal();
					GWorld->MoveActor(this, Adjusted, Rotation, 0, Hit);
					if (Hit.Time < 1.f)
					{
						SmoothHitWall(Hit.Normal, Hit.Actor);
						TwoWallAdjust(DesiredDir, Adjusted, Hit.Normal, OldHitNormal, Hit.Time);
						GWorld->MoveActor(this, Adjusted, Rotation, 0, Hit);
					}
				}
			}
		}
		return didHit;
	}
}

void AActor::SmoothHitWall(FVector const& HitNormal, AActor *HitActor)
{
	eventHitWall(HitNormal, HitActor, NULL);
}

void APawn::SmoothHitWall(FVector const& HitNormal, AActor *HitActor)
{
	FVector ModifiedHitNormal = HitNormal;
	if ( Controller )
	{
		if ( Physics == PHYS_Walking )
		{
			ModifiedHitNormal.Z = 0;
		}
		if ( Controller->eventNotifyHitWall(ModifiedHitNormal, HitActor) )
			return;
	}
	eventHitWall(ModifiedHitNormal, HitActor, NULL);
}

//======================================================================================

/** Attempts to find a base for this actor; does not modify the actor's base.  HeightBelow is the number of units below center to trace (FindBase uses 8) */
void AActor::SearchForBaseBelow(FLOAT HeightBelow, class AActor*& HitActor, FVector& HitNormal)
{
	FCheckResult Hit(1.f);

	const FVector ColLocation = CollisionComponent ? Location + CollisionComponent->Translation : Location;

	GWorld->SingleLineCheck(Hit, this, ColLocation + FVector(0,0,-HeightBelow), ColLocation, TRACE_AllBlocking, GetCylinderExtent());

	HitActor = Hit.Actor;
	HitNormal = Hit.Normal;
}

void AActor::FindBase()
{
	AActor* NewBase;
	FVector BaseNormal;
	SearchForBaseBelow(8.0f, /*out*/ NewBase, /*out*/ BaseNormal);

	if (NewBase != Base)
	{
		SetBase(NewBase, BaseNormal);
	}
}

/** Walks up the Base chain from this Actor and returns the Actor at the top (the eventual Base). this->Base is NULL, returns this. */
AActor* AActor::GetBaseMost()
{
	AActor* Top;
	for( Top=this; Top && Top->Base; Top=Top->Base );
	return Top;
}

FVector AActor::GetAggregateBaseVelocity( AActor* TestBase )
{
	FVector Result(0);

	if( TestBase == NULL )
	{
		TestBase = Base;
	}

	while( TestBase != NULL )
	{
		if( !TestBase->bStatic )
		{
			Result += TestBase->Velocity;
		}
		TestBase = TestBase->Base;
	}

	return Result;
}



static UBOOL IsRigidBodyPhysics(BYTE Physics)
{
	if( Physics == PHYS_RigidBody )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void AActor::setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV)
{
	if( Physics == NewPhysics )
	{
		return;
	}

	// log C++ call stack of physics mode changes on Pawns
	DEBUGPHYSONLY
	(
		if( Cast<APawn>(this) != NULL )
		{
			debugf(TEXT("%3.2f %s SetPhysics: %d, OldPhysics: %d, NewFloor: %s"), GWorld->GetTimeSeconds(), *GetName(), NewPhysics, Physics, *NewFloor->GetName());
			
			// Dump call stack to log
			appDumpCallStackToLog();
		}
	)
	const BYTE OldPhysics = Physics;

	Physics = NewPhysics;

	if( (Physics == PHYS_Walking) || (Physics == PHYS_None) || (Physics == PHYS_Rotating) || (Physics == PHYS_Spider) )
	{	
		if( NewFloor == NULL || NewFloor->IgnoreBlockingBy(this) )
		{
			FindBase();
		}
		else if( Base != NewFloor )
		{
			SetBase(NewFloor, NewFloorV);
		}
	}
	// if the new physics doesn't support a Base, remove it
	else if (Base != NULL && Physics != PHYS_Interpolating)
	{
		SetBase(NULL);
	}
	
	if( (Physics == PHYS_None) || (Physics == PHYS_Rotating) )
	{
		Velocity		= FVector(0.f);
		Acceleration	= FVector(0.f);
	}
	
	if( PhysicsVolume )
	{
		PhysicsVolume->eventPhysicsChangedFor(this);
	}

	// Handle changing to and from rigid-body physics mode.
	if( Physics == PHYS_RigidBody )
	{
		if( CollisionComponent )
		{
			CollisionComponent->SetComponentRBFixed(FALSE);

			// Should we always wake when we switch to PHYS_RigidBody? Seems like a sensible thing to do anyway...
			CollisionComponent->WakeRigidBody(); 
		}
	}
	else if( OldPhysics == PHYS_RigidBody )
	{
		if( CollisionComponent )
		{
			CollisionComponent->SetComponentRBFixed(TRUE);
		}
	}
}

void AActor::performPhysics(FLOAT DeltaSeconds)
{
	const FVector OldVelocity = Velocity;

	// make sure we have a valid physicsvolume (level streaming might kill it)
	if (PhysicsVolume == NULL)
	{
		SetZone(FALSE, FALSE);
	}

	// change position
	switch (Physics)
	{
		case PHYS_None: return;
		case PHYS_Walking: physWalking(DeltaSeconds, 0); break;
		case PHYS_Projectile: physProjectile(DeltaSeconds, 0); break;
		case PHYS_Falling: physFalling(DeltaSeconds, 0); break;
		case PHYS_Rotating: break;
		case PHYS_RigidBody: physRigidBody(DeltaSeconds); break;
		case PHYS_SoftBody: physSoftBody(DeltaSeconds); break;
		case PHYS_Interpolating: physInterpolating(DeltaSeconds); break;
		case PHYS_Custom: physCustom(DeltaSeconds, 0); break;
		default:
			debugf(NAME_Warning, TEXT("%s has unsupported physics mode %d"), *GetName(), INT(Physics));
			setPhysics(PHYS_None);
			break;
	}
	
	if( bDeleteMe )
	{
		return;
	}

	// rotate
	if (!RotationRate.IsZero() && Physics != PHYS_Interpolating && Physics != PHYS_RigidBody)
	{
		physicsRotation(DeltaSeconds,OldVelocity);
	}

	// allow touched actors to impact physics
	if( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		AActor *OldTouch = PendingTouch;
		PendingTouch = PendingTouch->PendingTouch;
		OldTouch->PendingTouch = NULL;
	}
}

void APawn::performPhysics(FLOAT DeltaSeconds)
{
	// Skip regular perform physics when doing Root Motion, we want to do it right after the animation has been extracted, to prevent having a frame of lag.
	// GIsGame check to match check in UnSkeletalComponent.cpp. Editor can allow root motion updates with deltatime <= 0.
	if( Mesh != NULL && Mesh->RootMotionMode == RMM_Accel && GIsGame )
	{
		if( Mesh->bProcessingRootMotion == bForceRegularVelocity )
		{
			return;
		}
	}

	CheckStillInWorld();

	if( bDeleteMe )
	{
		return;
	}

	// make sure we have a valid physicsvolume (level streaming might kill it)
	if (PhysicsVolume == NULL)
	{
		SetZone(FALSE, FALSE);
	}

	FVector OldVelocity = Velocity;

	if( Physics != PHYS_Walking )
	{
		// only crouch while walking
		if( Physics != PHYS_Falling && bIsCrouched )
		{
			UnCrouch();
		}
	}
	else if( bWantsToCrouch && bCanCrouch ) 
	{
		// players crouch by setting bWantsToCrouch to true
		if( !bIsCrouched )
		{
			Crouch();
		}
		else if( bTryToUncrouch )
		{
			UncrouchTime -= DeltaSeconds;
			if( UncrouchTime <= 0.f )
			{
				bWantsToCrouch = FALSE;
				bTryToUncrouch = FALSE;
			}
		}
	}

	// change position
	startNewPhysics(DeltaSeconds,0);

	// Pawn has moved, post process this movement.
	PostProcessPhysics(DeltaSeconds, OldVelocity);

	bSimulateGravity = ((Physics == PHYS_Falling) || (Physics == PHYS_Walking));

	// uncrouch if no longer desiring crouch
	// or if not in walking physics
	if( bIsCrouched && ((Physics != PHYS_Walking && Physics != PHYS_Falling) || !bWantsToCrouch))
	{
		UnCrouch();
	}

	if( Controller || bRunPhysicsWithNoController )
	{
		if(Controller)
		{
			Controller->MoveTimer -= DeltaSeconds;
		}

		if (Physics != PHYS_RigidBody && Physics != PHYS_Interpolating)
		{
			physicsRotation(DeltaSeconds, OldVelocity);
		}
	}

	AvgPhysicsTime = 0.8f * AvgPhysicsTime + 0.2f * DeltaSeconds;

	if( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		if( PendingTouch )
		{
			AActor *OldTouch = PendingTouch;
			PendingTouch = PendingTouch->PendingTouch;
			OldTouch->PendingTouch = NULL;
		}
	}
}

void APawn::setPhysics(BYTE NewPhysics, AActor* NewFloor, FVector NewFloorV)
{
	// make sure we validate our new floor on initial entry of the walking physics
	if (Physics != PHYS_Walking && NewPhysics == PHYS_Walking)
	{
		bForceFloorCheck = TRUE;
	}

	Super::setPhysics(NewPhysics, NewFloor, NewFloorV);
}

void APawn::startNewPhysics(FLOAT deltaTime, INT Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_PawnPhysics);

	if ( (deltaTime < 0.0003f) || (Iterations > 7) )
		return;
	switch (Physics)
	{
		case PHYS_None: return;
		case PHYS_Walking: physWalking(deltaTime, Iterations); break;
		case PHYS_Falling: physFalling(deltaTime, Iterations); break;
		case PHYS_Flying: physFlying(deltaTime, Iterations); break;
		case PHYS_Swimming: physSwimming(deltaTime, Iterations); break;
		case PHYS_Spider: physSpider(deltaTime, Iterations); break;
		case PHYS_Ladder: physLadder(deltaTime, Iterations); break;
		case PHYS_RigidBody: physRigidBody(deltaTime); break;
		case PHYS_SoftBody: physSoftBody(deltaTime); break;
		case PHYS_NavMeshWalking: physNavMeshWalking(deltaTime); break;
		case PHYS_Interpolating: physInterpolating(deltaTime); break;
		case PHYS_Custom: physCustom(deltaTime, Iterations); break;
		default:
			debugf(NAME_Warning, TEXT("%s has unsupported physics mode %d"), *GetName(), INT(Physics));
			setPhysics(PHYS_None);
			break;
	}
}

/** Called in PerformPhysics(), after StartNewPhysics() is done moving the Actor, and before the PendingTouch() event is dispatched. */
void APawn::PostProcessPhysics( FLOAT DeltaSeconds, const FVector& OldVelocity ) {}

int AActor::fixedTurn(int current, int desired, int deltaRate)
{
	if (deltaRate == 0)
		return (current & 65535);

	int result = current & 65535;
	current = result;
	desired = desired & 65535;

	if (current > desired)
	{
		if (current - desired < 32768)
			result -= Min((current - desired), Abs(deltaRate));
		else
			result += Min((desired + 65536 - current), Abs(deltaRate));
	}
	else
	{
		if (desired - current < 32768)
			result += Min((desired - current), Abs(deltaRate));
		else
			result -= Min((current + 65536 - desired), Abs(deltaRate));
	}
	return (result & 65535);
}

/* FindSlopeRotation()
return a rotation that will leave actor pointed in desired direction, and placed snugly against its floor
*/
FRotator AActor::FindSlopeRotation(const FVector& FloorNormal, const FRotator& NewRotation)
{
	if ( (FloorNormal.Z < 0.99f) && !FloorNormal.IsNearlyZero() )
	{
		FRotator OutRotation = NewRotation;
		FRotator TempRot = NewRotation;
		// allow yawing, but pitch and roll fixed based on wall
		TempRot.Pitch = 0;
		
		FVector YawDir = TempRot.Vector();
		FVector PitchDir = YawDir - FloorNormal * (YawDir | FloorNormal);
		TempRot = PitchDir.Rotation();
		OutRotation.Pitch = TempRot.Pitch;

		FVector RollDir = PitchDir ^ FloorNormal;
		TempRot = RollDir.Rotation();
		OutRotation.Roll = TempRot.Pitch;
		
		return OutRotation;
	}
	else
	{
		return FRotator(0, NewRotation.Yaw, 0);
	}
}

void APawn::physicsRotation(FLOAT deltaTime, FVector OldVelocity)
{
	// If SetDesiredRotation is used, and it reached to the destination
	CheckDesiredRotation();

	if (Controller == NULL || deltaTime <= 0.0f)
	{
		return;
	}

	// always call SetRotationRate() as it may change our DesiredRotation
	FRotator deltaRot = Controller->SetRotationRate(deltaTime);
	if( !bCrawler && (Rotation == DesiredRotation) && !IsHumanControlled() )
		return;

	// Accumulate a desired new rotation.
	FRotator NewRotation = Rotation;	

	if( (Physics == PHYS_Ladder) && OnLadder )
	{
		// must face ladder
		NewRotation = OnLadder->WallDir;
	}
	else
	{
		//YAW
		if( DesiredRotation.Yaw != NewRotation.Yaw )
		{
			NewRotation.Yaw = fixedTurn(NewRotation.Yaw, DesiredRotation.Yaw, deltaRot.Yaw);
		}

		// PITCH
		if( !bRollToDesired && ((Physics == PHYS_Walking) || (Physics == PHYS_Falling) || (Physics == PHYS_NavMeshWalking)) )
		{
			DesiredRotation.Pitch = 0;
		}
		if( DesiredRotation.Pitch != NewRotation.Pitch )
		{
			NewRotation.Pitch = fixedTurn(NewRotation.Pitch, DesiredRotation.Pitch, deltaRot.Pitch);
		}
	}

	// ROLL
	if( bRollToDesired || bCrawler )
	{
		if( DesiredRotation.Roll != NewRotation.Roll )
		{
			NewRotation.Roll = fixedTurn(NewRotation.Roll, DesiredRotation.Roll, deltaRot.Roll);
		}
	}
	else
	{
		NewRotation.Roll = 0;
	}

	AngularVelocity = CalcAngularVelocity( Rotation, NewRotation, deltaTime );

	// Set the new rotation.
	// fixedTurn() returns denormalized results so we must convert Rotation to prevent negative values in Rotation from causing unnecessary MoveActor() calls
	if( NewRotation != Rotation.GetDenormalized() )
	{
		FCheckResult Hit(1.f);
		GWorld->MoveActor( this, FVector(0,0,0), NewRotation, 0, Hit );
	}
}

void AActor::physicsRotation(FLOAT deltaTime, FVector OldVelocity)
{
	if (deltaTime > 0.0f)
	{
		// Accumulate a desired new rotation.
		FRotator OldRotation = Rotation;
		FRotator NewRotation = Rotation;
		FRotator deltaRotation = RotationRate * deltaTime;

		NewRotation = NewRotation + deltaRotation;

		// Set the new rotation.
		// fixedTurn() returns denormalized results so we must convert Rotation to prevent negative values in Rotation from causing unnecessary MoveActor() calls
		if (NewRotation != Rotation.GetDenormalized())
		{
			FCheckResult Hit(1.f);
			GWorld->MoveActor( this, FVector(0,0,0), NewRotation, 0, Hit );
		}

		AngularVelocity = CalcAngularVelocity(OldRotation, NewRotation, deltaTime);
	}
}

/*====================================================================================
physWalking()
*/
// if AI controlled, check for fall by doing trace forward
// try to find reasonable walk along ledge

FVector APawn::CheckForLedges(FVector AccelDir, FVector Delta, FVector GravDir, int &bCheckedFall, int &bMustJump )
{
	FCheckResult Hit(1.f);
	FVector ColLocation = CollisionComponent ? Location + CollisionComponent->Translation : Location;
	if (Base == NULL)
	{
		// if already off the ledge, then must jump
		if (GWorld->SingleLineCheck(Hit, this, ColLocation - FVector(0.f,0.f,LedgeCheckThreshold), ColLocation, TRACE_AllBlocking|TRACE_StopAtAnyHit, GetCylinderExtent()))
		{
			bMustJump = TRUE;
			return Delta;
		}
	}

	// check if clear in front
	FVector ForwardCheck = (AccelDir.IsNearlyZero() ? Delta.SafeNormal() : AccelDir) * CylinderComponent->CollisionRadius;
	FVector Destn = ColLocation + Delta + ForwardCheck;
	if ( !(ColLocation - Destn).IsNearlyZero() 
		&& !GWorld->SingleLineCheck(Hit, this, Destn, ColLocation, TRACE_AllBlocking|TRACE_StopAtAnyHit) )
	{
		// going to hit something, so don't bother
		return Delta;
	}

	// Work out, based on that slope, how many units down you go for each unit forwards (this is just pythag), triangle formed by surf normal and world Z
	FLOAT DropRatio = appSqrt(1.f - (WalkableFloorZ*WalkableFloorZ))/WalkableFloorZ;

	// clear in front - see if there is footing at walk destination
	FLOAT DesiredDist = Delta.Size();
	// check down enough to catch either step or slope
	FLOAT TestDown = LedgeCheckThreshold + CylinderComponent->CollisionHeight + ::Max( MaxStepHeight, DropRatio * (CylinderComponent->CollisionRadius + DesiredDist));
	const FLOAT MaxStepPct = (CylinderComponent->CollisionHeight + MaxStepHeight) / TestDown;

	// try a point trace
	FVector TraceExtent(0.5f);
	GWorld->SingleLineCheck( Hit, this, Destn + TestDown * GravDir, Destn , TRACE_AllBlocking, TraceExtent );
	// if point trace hit nothing, or hit a steep slope, or below a normal step down, do a trace with extent
	if ( !bAvoidLedges )
	{
		Destn = ColLocation + Delta;
	}

	if (Hit.Time == 1.f ||
		Hit.Normal.Z < WalkableFloorZ ||
		(Hit.Time * TestDown > CylinderComponent->CollisionHeight + LedgeCheckThreshold + Min<FLOAT>(MaxStepHeight,appSqrt(1.f - Hit.Normal.Z * Hit.Normal.Z) * (CylinderComponent->CollisionRadius + DesiredDist)/Hit.Normal.Z)))
	{
		if (!GWorld->SingleLineCheck(Hit, this, Destn, ColLocation, TRACE_AllBlocking|TRACE_StopAtAnyHit, GetCylinderExtent()))
		{
			return Delta;
		}
		if (!bAllowLedgeOverhang)
		{
			// do a zero-extent test at the edge of the cylinder
			FVector CheckLocation = Destn + Delta.SafeNormal() * GetCylinderExtent().X;
			GWorld->SingleLineCheck(Hit, this, CheckLocation + GravDir * (MaxStepHeight + LedgeCheckThreshold), CheckLocation , TRACE_AllBlocking|TRACE_StopAtAnyHit);
			if (Hit.Time == 1.f)
			{
				if ( Controller && Controller->StopAtLedge() )
				{
					return FVector(0.f);
				}
				// no available direction, so try to jump
				if ( !bCheckedFall && Controller && Controller->IsProbing(NAME_MayFall) )
				{
					bCheckedFall = 1;
					bCanJump = TRUE;
					Controller->eventMayFall((Hit.Time < 1.f), Hit.Normal);
					bMustJump = bCanJump;
				}
				return bMustJump ? Delta : FVector(0.f);
			}
		}
		GWorld->SingleLineCheck(Hit, this, Destn + GravDir * (MaxStepHeight + LedgeCheckThreshold), Destn , TRACE_AllBlocking|TRACE_StopAtAnyHit, GetCylinderExtent());
	}
	else
	{
		// If trace drops more than step height
		if( Hit.Time > MaxStepPct )
		{
			// Mark partially over ledge and store the movement direction
			bPartiallyOverLedge = TRUE;
			PartialLedgeMoveDir = Delta.SafeNormal();
		}
	}

	if ( Hit.Time == 1.f ||	Hit.Normal.Z < WalkableFloorZ )
	{
		// We have a ledge!
		if ( Controller && Controller->StopAtLedge() )
		{
			return FVector(0.f,0.f,0.f);
		}

		// check which direction ledge goes
		FVector DesiredDir = Destn - ColLocation;
		DesiredDir = DesiredDir.SafeNormal();
		FVector SideDir(DesiredDir.Y, -1.f * DesiredDir.X, 0.f);
		
		// try left
		FVector LeftSide = Destn + DesiredDist * SideDir;
		GWorld->SingleLineCheck(Hit, this, LeftSide, Destn, TRACE_AllBlocking|TRACE_StopAtAnyHit, GetCylinderExtent());
		if ( (Hit.Time == 1.f) || (Hit.Normal.Z >= WalkableFloorZ) )
		{
			if ( Hit.Time == 1.f )
			{
				GWorld->SingleLineCheck(Hit, this, LeftSide + GravDir * (MaxStepHeight + LedgeCheckThreshold), LeftSide , TRACE_AllBlocking|TRACE_StopAtAnyHit, GetCylinderExtent());
			}
			if ( (Hit.Time < 1.f) && (Hit.Normal.Z >= WalkableFloorZ) )
			{
				// go left
				FVector NewDir = (LeftSide - ColLocation).SafeNormal();
				return NewDir * DesiredDist;
			}
		}

		// try right
		FVector RightSide = Destn - DesiredDist * SideDir;
		GWorld->SingleLineCheck(Hit, this, RightSide, Destn, TRACE_AllBlocking|TRACE_StopAtAnyHit, GetCylinderExtent());
		if ( (Hit.Time == 1.f) || (Hit.Normal.Z >= WalkableFloorZ) )
		{
			if ( Hit.Time == 1.f )
			{
				GWorld->SingleLineCheck(Hit, this, RightSide + GravDir * (MaxStepHeight + LedgeCheckThreshold), RightSide , TRACE_AllBlocking|TRACE_StopAtAnyHit, GetCylinderExtent());
			}
			if ( (Hit.Time < 1.f) && (Hit.Normal.Z >= WalkableFloorZ) )
			{
				// go right
				FVector NewDir = (RightSide - ColLocation).SafeNormal();
				return NewDir * DesiredDist;
			}
		}

		// no available direction, so try to jump
		if ( !bCheckedFall && Controller && Controller->IsProbing(NAME_MayFall) )
		{
			bCheckedFall = 1;
			bCanJump = TRUE;
			Controller->eventMayFall((Hit.Time < 1.f), Hit.Normal);
			bMustJump = bCanJump;
		}
	}
	return Delta;
}

void APawn::physWalking(FLOAT deltaTime, INT Iterations)
{
	if( !Controller && !bRunPhysicsWithNoController )
	{
		Acceleration	= FVector(0.f);
		Velocity		= FVector(0.f);
		return;
	}

#if !FINAL_RELEASE
	if(CollisionComponent != NULL && CollisionComponent->IsA(UCylinderComponent::StaticClass()) && !CollisionComponent->Translation.IsNearlyZero())
	{
		warnf(TEXT("WARNING! %s has a cylinder colllision component (%s) which has a non-zero translation (%s)!! This is not supported and will probably result in broken movement for this pawn."),*GetName(),*CollisionComponent->GetName(),*CollisionComponent->Translation.ToString());
	}
#endif
	
	//bound acceleration
	Velocity.Z = 0.f;
	Acceleration.Z = 0.f;
	FVector AccelDir = Acceleration.IsZero() ? Acceleration : Acceleration.SafeNormal();
	CalcVelocity(AccelDir, deltaTime, GroundSpeed, PhysicsVolume->GroundFriction, 0, 1, 0);
	
	// Add effect of velocity zone
	// Rather than constant velocity, hacked to make sure that velocity being clamped when walking doesn't
	// cause the zone velocity to have too much of an effect at fast frame rates
	// @todo actually clamp this ^^
	FVector DesiredMove = Velocity;
	if ( PhysicsVolume->bVelocityAffectsWalking )
	{
		DesiredMove += PhysicsVolume->GetZoneVelocityForActor(this) * 25.f * deltaTime;
	}
	DesiredMove.Z = 0.f;

	//Perform the move
	const FVector GravDir = FVector(0.f,0.f,-1.f);
	const FVector Down = GravDir * (MaxStepHeight + MAXSTEPHEIGHTFUDGE);
	FCheckResult Hit(1.f);
	const FVector OldLocation = Location;
	const FVector OldFloor = Floor;
	AActor* OldBase = Base;
	FVector OldBaseLocation = (Base != NULL) ? Base->Location : FVector(0.f, 0.f, 0.f);
	bJustTeleported = 0;
	UBOOL bWasPartiallyOverLedge = bPartiallyOverLedge;
	bPartiallyOverLedge = FALSE;
	INT bCheckedFall = 0;
	INT bMustJump = 0;
	UBOOL bRejectMove = FALSE;
	FLOAT remainingTime = deltaTime;

	while ( (remainingTime > 0.f) && (Iterations < 8) && (Controller || bRunPhysicsWithNoController) )
	{
		Iterations++;
		// subdivide moves to be no longer than 0.05 seconds
		const FLOAT timeTick = (remainingTime > 0.05f) ? Min(0.05f, remainingTime * 0.5f) : remainingTime;
		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector subLoc = Location;
		FLOAT NewHitTime = 0.f;
		FVector NewFloor(0.f,0.f,0.f);
		AActor *NewBase = NULL;
		const UBOOL bZeroDelta = Delta.IsNearlyZero();

		if ( bZeroDelta )
		{
			remainingTime = 0.f;
		}
		else
		{
			// if AI controlled or walking player, avoid falls
			const UBOOL bCheckLedges = Controller ? Controller->WantsLedgeCheck() : FALSE;
			if( bCheckLedges )
			{
				FVector subMove = Delta;
				Delta = CheckForLedges(AccelDir, Delta, GravDir, bCheckedFall, bMustJump);
				if( Controller->MoveTimer == -1.f || Delta.IsZero() )
				{
					remainingTime = 0.f;
				}
			}

			// try to move forward
			if ( (Floor.Z < 0.98f) && ((Floor | Delta) < 0.f) )
			{
				Hit.Time = 0.f;
				Hit.Normal = Floor;
			}
			else
			{
				GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
			}

			if ( Hit.Time < 1.f )
			{
				// Handle pawn bumping into mesh that can become dynamic
				if ( Hit.Actor && Hit.Actor->bWorldGeometry && (Base != Hit.Actor) )
				{
					UStaticMeshComponent *HitStaticMesh = Cast<UStaticMeshComponent>(Hit.Component);
					if ( HitStaticMesh && HitStaticMesh->CanBecomeDynamic() )
					{
						AKActorFromStatic* NewKActor = Cast<AKActorFromStatic>(AKActorFromStatic::StaticClass()->GetDefaultActor())->MakeDynamic(HitStaticMesh);
						if ( NewKActor )
						{
							FVector HitDir =  Hit.Location - Location;
							HitDir.Z = ::Max(HitDir.Z, 0.f);
							NewKActor->eventApplyImpulse(HitDir, GroundSpeed,  Hit.Location); 
							Hit.Actor = NewKActor;
						}
					}
				}

				if ( !Hit.Actor || Hit.Actor->bCanStepUpOn || Base == Hit.Actor )
				{
					// hit a barrier, try to step up
					const FLOAT DesiredDist = Delta.Size();
					const FVector DesiredDir = Delta/DesiredDist;
					stepUp(GravDir, DesiredDir, Delta * (1.f - Hit.Time), Hit);
					if ( Physics == PHYS_Falling ) // pawn decided to jump up
					{
						const FLOAT ActualDist = (Location - subLoc).Size2D();
						remainingTime += timeTick * (1 - Min(1.f,ActualDist/DesiredDist));
						eventFalling();
						if ( Physics == PHYS_Flying )
						{
							Velocity = FVector(0.f,0.f,AirSpeed);
							Acceleration = FVector(0.f,0.f,AccelRate);
						}
						startNewPhysics(remainingTime,Iterations);
						return;
					}

					// see if I already found a floor
					if ( Hit.Time < 1.f )
					{
						NewHitTime = Hit.Time;
						NewFloor = Hit.Normal;
						NewBase = Hit.Actor;
					}
				}
				else if ( Hit.Actor && !Hit.Actor->bCanStepUpOn )
				{
					const FLOAT DesiredDist = Delta.Size();
					const FVector DesiredDir = Delta/DesiredDist;

					// notify script that pawn ran into a wall
					processHitWall(Hit);
					if ( Physics == PHYS_Falling )
					{
						// pawn decided to jump up
						const FLOAT ActualDist = (Location - subLoc).Size2D();
						remainingTime += timeTick * (1 - Min(1.f,ActualDist/DesiredDist));
						eventFalling();
						if ( Physics == PHYS_Flying )
						{
							Velocity = FVector(0.f,0.f,AirSpeed);
							Acceleration = FVector(0.f,0.f,AccelRate);
						}
						startNewPhysics(remainingTime,Iterations);
						return;
					}

					// adjust along wall
					Hit.Normal.Z = 0.f;	// treat barrier as vertical;
					Hit.Normal = Hit.Normal.SafeNormal();
					const FVector OldHitNormal = Hit.Normal;
					FVector NewDelta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
					if( (NewDelta | Delta) >= 0.f && !NewDelta.IsNearlyZero() )
					{
						GWorld->MoveActor(this, NewDelta, Rotation, 0, Hit);
						if (Hit.Time < 1.f)
						{
							processHitWall(Hit);
							if ( Physics == PHYS_Falling )
								return;
							TwoWallAdjust(DesiredDir, NewDelta, Hit.Normal, OldHitNormal, Hit.Time);
							GWorld->MoveActor(this, NewDelta, Rotation, 0, Hit);
						}
					}
				}
			}
			if ( Physics == PHYS_Swimming ) //just entered water
			{
				startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		//drop to floor
		FLOAT FloorDist;

		if (Base != NULL && !Base->bCollideActors && Base != WorldInfo)
		{
			bForceFloorCheck = TRUE;
		}

		if ( NewBase )
		{
			bForceFloorCheck = FALSE;
			const FVector ColLocation = CollisionComponent ? CollisionComponent->Bounds.Origin : Location;
			// note: MoveActor multiplies extent by FVector(1.001f,1.001f,1.0f), we do smaller checks here so that if we get popped onto a surface due to floating point error, we will not stick there
			DWORD	TraceFlags	= TRACE_AllBlocking;
			if ( bMoveIgnoresDestruction )
			{
				TraceFlags |= TRACE_MoveIgnoresDestruction;
			}
			GWorld->SingleLineCheck( Hit, this, ColLocation + Down, ColLocation, TraceFlags, GetCylinderExtent() );
			FloorDist = Hit.Time * (MaxStepHeight + MAXSTEPHEIGHTFUDGE);
		}
		else if ( bZeroDelta && Base && Base->bWorldGeometry && (RelativeLocation == Location - Base->Location) && !bForceFloorCheck )
		{
			Hit.Actor = Base;
			Hit.Normal = Floor;
			Hit.Time = 0.1f;
			FloorDist = MAXFLOORDIST;
		}
		else
		{
			bForceFloorCheck = FALSE;
			const FVector ColLocation = CollisionComponent ? CollisionComponent->Bounds.Origin : Location;
			// note: MoveActor multiplies extent by FVector(1.001f,1.001f,1.0f), we do smaller checks here so that if we get popped onto a surface due to floating point error, we will not stick there
			DWORD	TraceFlags	= TRACE_AllBlocking;
			if ( bMoveIgnoresDestruction )
			{
				TraceFlags |= TRACE_MoveIgnoresDestruction;
			}
			GWorld->SingleLineCheck( Hit, this, ColLocation + Down, ColLocation, TraceFlags, GetCylinderExtent() );
			FloorDist = Hit.Time * (MaxStepHeight + MAXSTEPHEIGHTFUDGE);
			//debugf(TEXT("- new hit actor: %s, current base: %s"),Hit.Actor!=NULL?*Hit.Actor->GetName():TEXT("NULL"),Base!=NULL?*Base->GetName():TEXT("NULL"));
		}

		Floor = Hit.Normal;

		if ( (Hit.Normal.Z < WalkableFloorZ) && !Delta.IsNearlyZero() && ((Delta | Hit.Normal) < 0) )
		{
			// slide down slope
			FVector Slide = Hit.Normal * (FVector(0.f,0.f,MaxStepHeight) | Hit.Normal) - FVector(0.f,0.f,MaxStepHeight);

			// slight fudge to angle away from the slope, to help prevent getting caught due to
			// precision errors.  helps climbing vertical walls.
			static const FLOAT NormalFudge=0.1f;
			Slide += Hit.Normal * NormalFudge;

			GWorld->MoveActor(this, Slide, Rotation, 0, Hit);
			if ( (Hit.Actor != Base) && (Physics == PHYS_Walking) )
			{
				SetBase(Hit.Actor, Hit.Normal);
			}
			Floor = Hit.Normal;

			if ( NewBase && (Floor.Z < WalkableFloorZ) && Hit.Time < 1.f )
			{
				// If we're here, stepup put us an unwalkable surface and we could't resolve it with a slide.
				// Reject the movement outright.  This tends to happen when pressing into skewed corners.
				bRejectMove = TRUE;
			}
		}
		else if( Hit.Time< 1.f && !Hit.bStartPenetrating && (Hit.Actor!=Base || FloorDist>MAXFLOORDIST) )
		{
			if ( ShouldCatchAir(OldFloor, Floor) )
			{
				StartFalling(Iterations, remainingTime, timeTick, Delta, subLoc);
				return;
			}
			else
			{
				// move down to correct position
				const FVector RealNorm = Hit.Normal;
				AActor* RealHitActor = Hit.Actor;

				GWorld->MoveActor(this, FVector(0.0f,0.0f,0.5f*(MINFLOORDIST+MAXFLOORDIST) - FloorDist), Rotation, 0, Hit);
				if ( Hit.Time == 1.f )
				{
					Hit.Time = 0.f;
					Hit.Normal = RealNorm;
					Hit.Actor = RealHitActor;
				}
				if ( (Hit.Actor != Base) && (Physics == PHYS_Walking) && IsBlockedBy(Hit.Actor, Hit.Component) )
				{
					SetBase(Hit.Actor, Hit.Normal);
				}
			}
		}
		else if (FloorDist < MINFLOORDIST && !Hit.bStartPenetrating)
		{
			// move up to correct position (average of MAXFLOORDIST and MINFLOORDIST above floor)
			const FVector RealNorm = Hit.Normal;
			GWorld->MoveActor(this, FVector(0.f,0.f,0.5f*(MINFLOORDIST+MAXFLOORDIST) - FloorDist), Rotation, 0, Hit);
			Hit.Time = 0.f;
			Hit.Normal = RealNorm;
		}

		// check if just entered water
		if ( Physics == PHYS_Swimming )
		{
			startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
			return;
		}
		
		if( !bMustJump && Hit.Time<1.f && Hit.Normal.Z>=WalkableFloorZ )
		{
			// standing on walkable surface...

			if( (Hit.Normal.Z < 0.99f) && ((Hit.Normal.Z * PhysicsVolume->GroundFriction) < 3.3f) ) 
			{
				// slide down slope, depending on friction and gravity
				const FVector Slide(0.f, 0.f, (deltaTime * GetGravityZ()/(2.f * ::Max(0.5f, PhysicsVolume->GroundFriction))) * deltaTime);
				Delta = Slide - Hit.Normal * (Slide | Hit.Normal);
				if( (Delta | Slide) >= 0.f )
				{
					GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
				}
				if ( Physics == PHYS_Swimming ) //just entered water
				{
					startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
					return;
				}
			}				
		}
		else
		{
			if (!bMustJump && !bCheckedFall && Controller && Controller->IsProbing(NAME_MayFall))
			{
				// give this pawn a chance to abort its fall
				bCanJump = TRUE;
				bCheckedFall = TRUE;
				Controller->eventMayFall((Hit.Time<1.f), Hit.Normal);
			}

			if (!bMustJump && !bCanJump && (OldBase == NULL || (!OldBase->bCollideActors && OldBase != WorldInfo)))
			{
				//debugf(TEXT("- forcing must jump"));
				bMustJump = TRUE;
			}

			// If we haven't checked for a ledge already
			// Make sure we can walk off this one
			if( bRejectMove ||
				(!bJustTeleported && 
				!bMustJump && 
				( !bCanJump || 
				  (!bCanWalkOffLedges && (bIsWalking || bIsCrouched) && !bZeroDelta ) ) ) )
			{
				// this pawn shouldn't fall, so undo its move
				Velocity = FVector(0.f,0.f,0.f);
				Acceleration = FVector(0.f,0.f,0.f);
				GWorld->FarMoveActor(this, OldLocation, FALSE, FALSE);
				bJustTeleported = FALSE;
				// if our previous base couldn't have moved or changed in any physics-affecting way, restore it
				if (OldBase != NULL && (OldBase->IsStatic() || OldBase->bWorldGeometry || !OldBase->bMovable || (OldBase->IsEncroacher() && OldBase->Location == OldBaseLocation)))
				{
					SetBase(OldBase,OldFloor);
				}
				if ( Controller )
				{
					Controller->FailMove();
				}
				return;
			}
			else
			{
				StartFalling(Iterations, remainingTime, timeTick, Delta, subLoc);
				return;
			}
		}
	}

	// Allow touch events and such to change physics state and velocity
	if( Physics == PHYS_Walking ) 
	{
		// Make velocity reflect actual move
		if( !bJustTeleported && !bWasPartiallyOverLedge )
		{
			Velocity = (Location - OldLocation) / deltaTime;
		}
		Velocity.Z = 0.f;
	}

	if( Controller )
	{
		Controller->PostPhysWalking( deltaTime );
	}
}

/** Whether pawn should go into falling mode when starting down a steep decline.
  *  @Returns FALSE to provide traditional default behavior
  */
UBOOL APawn::ShouldCatchAir(const FVector& OldFloor, const FVector& Floor)
{
	return FALSE;
}

/** Transition from walking to falling */
void APawn::StartFalling(INT Iterations, FLOAT remainingTime, FLOAT timeTick, const FVector& Delta, const FVector& subLoc)
{
	// go into falling physics even though we found a floor
	const FLOAT DesiredDist = Delta.Size();
	const FLOAT ActualDist = (Location - subLoc).Size2D();
	if (DesiredDist == 0.f)
	{
		remainingTime = 0.f;
	}
	else
	{
		remainingTime += timeTick * (1.f - Min(1.f,ActualDist/DesiredDist));
	}
	Velocity.Z = 0.f;
	eventFalling();
	if (Physics == PHYS_Walking)
	{
		if ( !GIsEditor )
		{
			setPhysics(PHYS_Falling); //default behavior if script didn't change physics
		}
		else
		{
			// This is to catch cases where the first frame of PIE is executed, and the
			// level is not yet visible. In those cases, the player will fall out of the
			// world... So, don't set PHYS_Falling straight away.
			if ( GWorld->HasBegunPlay() && (GWorld->GetTimeSeconds() >= 1.f) )
			{
				setPhysics(PHYS_Falling); //default behavior if script didn't change physics
			}
			else
			{
				// Make sure that the floor check code continues processing during this delay.
				bForceFloorCheck = TRUE;
			}
		}
	}
	startNewPhysics(remainingTime,Iterations);
}

void APawn::ApplyVelocityBraking(FLOAT DeltaTime, FLOAT Friction)
{
	const	FVector OldVel = Velocity;
			FVector SumVel = FVector(0);

	// subdivide braking to get reasonably consistent results at lower frame rates
	// (important for packet loss situations w/ networking)
			FLOAT RemainingTime = DeltaTime;
	const	FLOAT TimeStep		= 0.03f;

	while( RemainingTime > 0.f )
	{
		const FLOAT dt = ((RemainingTime > TimeStep) ? TimeStep : RemainingTime);
		RemainingTime -= dt;

		// don't drift to a stop, brake
		Velocity = Velocity - (2.f * Velocity) * dt * Friction; 
		if( (Velocity | OldVel) > 0.f )
		{
			SumVel += dt * Velocity/DeltaTime;
		}
	}

	Velocity = SumVel;

	// brake to a stop, not backwards
	if( ((OldVel | Velocity) < 0.f)	|| (Velocity.SizeSquared() < SLOWVELOCITYSQUARED) )
	{
		Velocity = FVector(0.f);
	}
}

/** 
 * CalcVelocity()
 * Calculates new velocity and acceleration for pawn for this tick
 * bounds acceleration and velocity, adds effects of friction and momentum
 * @param	AccelDir	Acceleration direction. (Normalized vector).
 * @param	DeltaTime	time elapsed since last frame.
 * @param	MaxSpeed	Maximum speed Pawn can go at. (f.e. Pawn.GroundSpeed for walking physics).
 * @param	Friction	friction
 * @param	bFluid		bFluid
 * @param	bBrake		if should brake to a stop when acceleration is zero.
 * @param	bBuoyant	Apply buoyancy for swimming physics
 */
void APawn::CalcVelocity(FVector &AccelDir, FLOAT DeltaTime, FLOAT MaxSpeed, FLOAT Friction, INT bFluid, INT bBrake, INT bBuoyant)
{
	// bForceRMVelocity is set when replaying moves that used root motion derived velocity on a client in a network game
	// the velocity can't be derived again, since the animation state isn't saved.
	if ( bForceRMVelocity )
	{
		Velocity = RMVelocity;
		return;
	}

	/** 
	 * In order to be able to do Root Motion Mode velocity, Animations must have been processed once.
	 * So we check for PreviousRMM to be up to date with the current root motion mode.
	 * We need to do this to get a valid motion.
	 */
	const UBOOL bDoRootMotionVelocity	= (Mesh && Mesh->RootMotionMode == RMM_Velocity && Mesh->PreviousRMM != RMM_Ignore && !bForceRegularVelocity);
	const UBOOL bDoRootMotionAccel		= (Mesh && Mesh->RootMotionMode == RMM_Accel && Mesh->PreviousRMM != RMM_Ignore && !bForceRegularVelocity);
	const UBOOL	bDoRootMotion			= (bDoRootMotionVelocity ||  bDoRootMotionAccel);

	/** 
	 * Root Motion Magnitude.
	 */
	const FVector	RootMotionTranslation	= bDoRootMotion ? Mesh->RootMotionDelta.GetTranslation() : FVector::ZeroVector;
	const FLOAT		RootMotionMag			= bDoRootMotion ? (RootMotionTranslation.Size() / DeltaTime) : 0.f;

	// if we're using root motion, then clear accumulated root motion
	// See USkeletalMeshComponent::UpdateSkelPose() for details.
	if( bDoRootMotion )
	{
		Mesh->RootMotionDelta.SetTranslation(FVector::ZeroVector);
	}

	// Scale MaxSpeed and AccelRate.
	const FLOAT SpeedModifier = MaxSpeedModifier();

	/** 
	 * When using root motion mode velocity, use that to limit the max acceleration allowed.
	 * This helps the Pawn catch up quicker with RootMotion Velocity when it is very irregular.
	 * Otherwise when it drops really low, it takes a while for the velocity to catch with a much higher value.
	 * We don't want to force the velocity to match root motion, since we want to keep code control on movement.
	 */
	const FLOAT MaxAccel = bDoRootMotionAccel ? (RootMotionMag / DeltaTime) : (bDoRootMotionVelocity ? Max(GetMaxAccel(SpeedModifier), (RootMotionMag / DeltaTime)) : GetMaxAccel(SpeedModifier));

	// Adjust MaxSpeed by Root Motion or SpeedModifier.
	MaxSpeed = bDoRootMotion ? RootMotionMag : MaxSpeed * SpeedModifier;

	// Debug RootMotionVelocity
	DEBUGPHYSONLY
	(
		if( bDoRootMotionVelocity )
		{
			debugf(TEXT("%3.3f [%s] bDoRootMotionVelocity MaxSpeed: %3.2f"), GWorld->GetTimeSeconds(), *GetName(), MaxSpeed);
		}
	)

	// Drive velocity by destination
	// This doesn't work with Root Motion Accel
	if( !bDoRootMotionAccel && Controller && Controller->bPreciseDestination )
	{
		// check to see if we've reached the destination
		FVector Dest = Controller->GetDestinationPosition();
		if( ReachedDestination(Location, Dest, NULL) )
		{
			Controller->bPreciseDestination = FALSE;
			Controller->eventReachedPreciseDestination();

			// clear velocity/accel, otherwise it's possible to continue drifting
			Velocity		= FVector(0.f);
			Acceleration	= FVector(0.f);
		}
		else
		if( bForceMaxAccel )
		{
			const FVector Dir = (Dest - Location).SafeNormal();
			Acceleration = Dir * MaxAccel;
			Velocity	 = Dir * MaxSpeed;
		}
		else
		{
			// otherwise calculate velocity towards the destination
			Velocity = (Dest - Location) / DeltaTime;
		}
		RMVelocity = Velocity;
	}
	else
	{
		// check to see if acceleration is being driven by animation
		if( bDoRootMotionAccel )
		{
			Velocity		= RootMotionTranslation / DeltaTime;
			AccelDir		= Velocity.SafeNormal();
			Acceleration	= Velocity / DeltaTime;
			RMVelocity		= Velocity;

			DEBUGPHYSONLY(debugf(TEXT("%3.3f [%s] bDoRootMotionAccel Move: %3.3f, Vect: %s"), GWorld->GetTimeSeconds(), *GetName(), Velocity.Size() * DeltaTime, *(Velocity*DeltaTime).ToString());)
			return;
		}

		// Force acceleration at full speed.
		// In condideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if( bForceMaxAccel )
		{
			if( !Acceleration.IsNearlyZero() )
			{
				Acceleration	= AccelDir * MaxAccel;
				DEBUGPHYSONLY( debugf(TEXT("%3.3f [%s] bForceMaxAccel picked from Acceleration. Acceleration: %s"), GWorld->GetTimeSeconds(), *GetName(), *Acceleration.ToString()); )
			}
			else if( !Velocity.IsNearlyZero() )
			{
				Acceleration	= Velocity.SafeNormal() * MaxAccel;
				AccelDir		= Acceleration.SafeNormal();
				DEBUGPHYSONLY( debugf(TEXT("%3.3f [%s] bForceMaxAccel picked from Velocity. Acceleration: %s"), GWorld->GetTimeSeconds(), *GetName(), *Acceleration.ToString()); )
			}
			else
			{
				Acceleration	= Rotation.Vector() * MaxAccel;
				AccelDir		= Acceleration.SafeNormal();
				DEBUGPHYSONLY( debugf(TEXT("%3.3f [%s] bForceMaxAccel picked from Rotation. Acceleration: %s"), GWorld->GetTimeSeconds(), *GetName(), *Acceleration.ToString()); )
			}
		}
		// If doing root motion velocity, make sure Acceleration is high enough to match root motion velocity
		// Only adjust acceleration if it is non zero (this means player wants to stop to a break).
		else if( bDoRootMotionVelocity && !Acceleration.IsNearlyZero() )
		{
			Acceleration = AccelDir * MaxAccel;
		}

		DEBUGPHYSONLY(debugf(TEXT("%3.3f [%s] bForceMaxAccel: %d, Accel Mag: %3.2f, Accel Vect: %s"), GWorld->GetTimeSeconds(), *GetName(), bForceMaxAccel, Acceleration.Size(), *Acceleration.ToString());)

		if( bBrake && Acceleration.IsZero() )
		{
			ApplyVelocityBraking(DeltaTime,Friction);
		}
		else
		{
			if( Acceleration.SizeSquared() > MaxAccel * MaxAccel )
			{
				Acceleration = Acceleration.SafeNormal() * MaxAccel;
			}
			const FLOAT VelSize = Velocity.Size();
			Velocity = Velocity - (Velocity - AccelDir * VelSize) * DeltaTime * Friction;
		}

		Velocity = Velocity * (1 - bFluid * Friction * DeltaTime) + Acceleration * DeltaTime;

		if( bBuoyant )
		{
			Velocity.Z += GetGravityZ() * DeltaTime * (1.f - Buoyancy);
		}
	}

	// Make sure velocity does not exceed MaxSpeed
	if( Velocity.SizeSquared() > MaxSpeed * MaxSpeed )
	{
		Velocity = Velocity.SafeNormal() * MaxSpeed;
	}

	RMVelocity	= Velocity;

	DEBUGPHYSONLY(debugf(TEXT("%3.3f [%s] CalcVelocity MaxSpeed: %3.2f, Vel Mag: %3.2f, Vel Vect: %s"), GWorld->GetTimeSeconds(), *GetName(), MaxSpeed, Velocity.Size(), *Velocity.ToString());)
}


/**
 * Scales the maximum velocity the Pawn can move.
 */
FLOAT APawn::MaxSpeedModifier()
{
	FLOAT Result = 1.f;

	if( !IsHumanControlled() )
	{
		Result *= DesiredSpeed;
	}

	if( bIsCrouched )
	{
		Result *= CrouchedPct;
	}
	else if( bIsWalking )
	{
		Result *= WalkingPct;
	}

	// Apply the MovementSpeedModifier
	Result *= MovementSpeedModifier;

	return Result;
}

FLOAT APawn::GetMaxAccel( FLOAT SpeedModifier )
{
	return (AccelRate * SpeedModifier);
}


void APawn::stepUp(const FVector& GravDir, const FVector& DesiredDir, const FVector& Delta, FCheckResult &Hit)
{
	FVector Down = GravDir * (MaxStepHeight + MAXSTEPHEIGHTFUDGE);
	UBOOL bStepDown = TRUE;

	// If walking up a slope that is walkable (step up - used instead of trying to slide up)
    FLOAT StepSideZ = -1.f * (Hit.Normal | GravDir);
	if( (StepSideZ < MAXSTEPSIDEZ) || (Hit.Normal.Z >= WalkableFloorZ) )
	{
		// step up - treat as vertical wall
		GWorld->MoveActor(this, -1.f * Down, Rotation, 0, Hit);
		GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
	}
	else if ( Physics != PHYS_Walking )
	{
		 // slide up slope
		FLOAT Dist = Delta.Size();
		GWorld->MoveActor(this, Delta + FVector(0,0,Dist*Hit.Normal.Z), Rotation, 0, Hit);
		bStepDown = FALSE;
	}

	if (Hit.Time < 1.f)
	{
		// Handle pawn bumping into mesh that can become dynamic
		if ( Hit.Actor && Hit.Actor->bWorldGeometry && (Base != Hit.Actor) )
		{
			UStaticMeshComponent *HitStaticMesh = Cast<UStaticMeshComponent>(Hit.Component);
			if ( HitStaticMesh && HitStaticMesh->CanBecomeDynamic() )
			{
				AKActorFromStatic* NewKActor = Cast<AKActorFromStatic>(AKActorFromStatic::StaticClass()->GetDefaultActor())->MakeDynamic(HitStaticMesh);
				if ( NewKActor )
				{
					FVector HitDir =  Hit.Location - Location;
					HitDir.Z = ::Max(HitDir.Z, 0.f);
					NewKActor->eventApplyImpulse(HitDir, GroundSpeed,  Hit.Location); 
					Hit.Actor = NewKActor;
				}
			}
		}

		// step up again if went far enough to consider a valid step, and step side is ~vertical, and can step onto the hit actor
		if ( ( -1.f * (Hit.Normal | GravDir) < MAXSTEPSIDEZ) && (Hit.Time * Delta.SizeSquared() > MINSTEPSIZESQUARED) 
			  && (!Hit.Actor || Hit.Actor->bCanStepUpOn) )
		{
			if ( bStepDown )
			{
				FCheckResult StepDownHit(1.f);
				GWorld->MoveActor(this, Down, Rotation, 0, StepDownHit);
			}
			stepUp(GravDir, DesiredDir, Delta * (1 - Hit.Time), Hit);
			return;
		}

		// notify script that pawn ran into a wall
		processHitWall(Hit);
		if ( Physics == PHYS_Falling )
			return;

		//adjust and try again
		Hit.Normal.Z = 0.f;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector OldHitNormal = Hit.Normal;
		FVector NewDelta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
		if( (NewDelta | Delta) >= 0.f )
		{
			GWorld->MoveActor(this, NewDelta, Rotation, 0, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, NewDelta, Hit.Normal, OldHitNormal, Hit.Time);
				GWorld->MoveActor(this, NewDelta, Rotation, 0, Hit);
			}
		}
	}
	if ( bStepDown )
	{
		GWorld->MoveActor(this, Down, Rotation, 0, Hit);
	}
}

/* AActor::stepUp() used by MoveSmooth() to move smoothly up steps

*/
void AActor::stepUp(const FVector& GravDir, const FVector& DesiredDir, const FVector& Delta, FCheckResult &Hit)
{
	FVector Down = GravDir * UCONST_ACTORMAXSTEPHEIGHT;

	if (Abs(Hit.Normal.Z) < MAXSTEPSIDEZ)
	{
		// step up - treat as vertical wall
		GWorld->MoveActor(this, -1 * Down, Rotation, 0, Hit);
		GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
	}
	else
	{
		 // slide up slope
		FLOAT Dist = Delta.Size();
		GWorld->MoveActor(this, Delta + FVector(0,0,Dist*Hit.Normal.Z), Rotation, 0, Hit);
	}

	if (Hit.Time < 1.f)
	{
		if ( (Abs(Hit.Normal.Z) < MAXSTEPSIDEZ) && (Hit.Time * Delta.SizeSquared() > MINSTEPSIZESQUARED) )
		{
			// try another step
			GWorld->MoveActor(this, Down, Rotation, 0, Hit);
			stepUp(GravDir, DesiredDir, Delta * (1.f - Hit.Time), Hit);
			return;
		}

		// notify script that actor ran into a wall
		processHitWall(Hit);
		if ( Physics == PHYS_Falling )
			return;

		//adjust and try again
		Hit.Normal.Z = 0;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector OldHitNormal = Hit.Normal;
		FVector NewDelta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
		if( (NewDelta | Delta) >= 0.f )
		{
			GWorld->MoveActor(this, NewDelta, Rotation, 0, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, NewDelta, Hit.Normal, OldHitNormal, Hit.Time);
				GWorld->MoveActor(this, NewDelta, Rotation, 0, Hit);
			}
		}
	}
	GWorld->MoveActor(this, Down, Rotation, 0, Hit);
}

/*
CanCrouchWalk()
Used by AI to determine if could continue moving forward by crouching
*/
UBOOL APawn::CanCrouchWalk( const FVector& StartLocation, const FVector& EndLocation, AActor* HitActor )
{
	const FVector CrouchedOffset = FVector(0.0f,0.0f,CrouchHeight-CylinderComponent->CollisionHeight);

    if( !bCanCrouch )
        return FALSE;

	DWORD TraceFlags = TRACE_World;
	if ( HitActor && !HitActor->bWorldGeometry )
		TraceFlags = TRACE_AllBlocking;

	// quick zero extent trace from start location
	FCheckResult Hit(1.0f);
	GWorld->SingleLineCheck(
		Hit,
		this,
		EndLocation + CrouchedOffset,
		StartLocation + CrouchedOffset,
		TraceFlags | TRACE_StopAtAnyHit );

	if( !Hit.Actor )
	{
		// try slower extent trace
		GWorld->SingleLineCheck(
			Hit,
			this,
			EndLocation + CrouchedOffset,
			StartLocation + CrouchedOffset,
			TraceFlags,
			FVector(CrouchRadius,CrouchRadius,CrouchHeight) );

			if ( Hit.Time == 1.0f )
			{
				bWantsToCrouch = TRUE;
				bTryToUncrouch = TRUE;
				UncrouchTime = 0.5f;
				return TRUE;
			}
	}
	return FALSE;
}

/** 
 * @param Hit Describes the collision.
 * @param TimeSlice Time period for the simulation that produced this hit.  Useful for
 *		  putting Hit.Time in context.  Can be zero in certain situations where it's not appropriate, 
 *		  be sure to handle that.
 */
void AActor::processHitWall(FCheckResult const& Hit, FLOAT TimeSlice)
{
	AActor* const HitActor = Hit.Actor;
	if (HitActor != NULL)
	{
		APortalTeleporter* Portal = HitActor->GetAPortalTeleporter();
		if (Portal != NULL && Portal->TransformActor(this))
		{
			return;
		}
		if (!bBlockActors && HitActor->GetAPawn() && !HitActor->IsEncroacher())
		{
			return;
		}
	}
	eventHitWall(Hit.Normal, HitActor, Hit.Component);
}

void AProjectile::processHitWall(FCheckResult const& Hit, FLOAT TimeSlice)
{
	AActor* const HitActor = Hit.Actor;
	if ( HitActor )
	{
		if ( bSwitchToZeroCollision && HitActor->CollisionComponent && !HitActor->CollisionComponent->BlockZeroExtent )
		{
			// ignore hitting walls that don't collide with zero collision projectiles if haven't shrunk yet
			return;
		}
		APortalTeleporter* Portal = HitActor->GetAPortalTeleporter();
		if (Portal != NULL && Portal->TransformActor(this))
		{
			return;
		}
	}
	eventHitWall(Hit.Normal, HitActor, Hit.Component);
}

void APawn::processHitWall(FCheckResult const& WallHit, FLOAT TimeSlice)
{
	AActor* HitActor = WallHit.Actor;
	if ( !HitActor )
	{
		return;
	}

	// Handle pawn bumping into mesh that can become dynamic
	if ( HitActor->bWorldGeometry )
	{
		UStaticMeshComponent *HitStaticMesh = Cast<UStaticMeshComponent>(WallHit.Component);
		if ( HitStaticMesh && HitStaticMesh->CanBecomeDynamic() )
		{
			AKActorFromStatic* NewKActor = Cast<AKActorFromStatic>(AKActorFromStatic::StaticClass()->GetDefaultActor())->MakeDynamic(HitStaticMesh);
			if ( NewKActor )
			{
				FVector HitDir =  WallHit.Location - Location;
				HitDir.Z = ::Max(HitDir.Z, 0.f);
				NewKActor->eventApplyImpulse(HitDir, GroundSpeed,  WallHit.Location); 
				HitActor = NewKActor;
			}
		}
	}

	APortalTeleporter* Portal = HitActor->GetAPortalTeleporter();
	if (Portal != NULL && Portal->TransformActor(this))
	{
		return;
	}

	FVector HitNormal = WallHit.Normal;

	// if we have both a controller and that controller has a pawn
	FVector DesiredDir = (Controller && Controller->Pawn) ? Controller->DesiredDirection() : Velocity;
	APawn* HitPawn = HitActor->GetAPawn();
	if (HitPawn != NULL)
	{
		if ( !Controller || (Physics == PHYS_Falling) || !HitActor->GetAVehicle() )
			return;
		if ( Controller->eventNotifyHitWall(HitNormal, HitActor) )
			return;
		FVector Cross = DesiredDir ^ FVector(0.f,0.f,1.f);
		FLOAT ColRadius = HitPawn->CylinderComponent 
							? HitPawn->CylinderComponent->CollisionRadius
							: 100.f;
		FVector Dir = 1.2f * ColRadius * Cross.SafeNormal();
		if ( (Cross | DesiredDir) < 0.f )
			Dir *= -1.f;
		if ( appFrand() < 0.3f )
			Dir *= -2.f;
		FVector AdjustLoc = Location + Dir;
		FCheckResult Hit(1.0f);
		if (!GWorld->SingleLineCheck(Hit, this, AdjustLoc, Location, TRACE_World))
		{
			FLOAT MyColRadius = CylinderComponent 
								? CylinderComponent->CollisionRadius
								: 100.f;
			AdjustLoc = Hit.Location - (Dir.SafeNormal() * MyColRadius);
		}
		Controller->SetAdjustLocation( AdjustLoc, TRUE );
		return;		
	}

	if ( !bDirectHitWall && Controller )
	{
		FVector Dir = DesiredDir.SafeNormal();
		if ( Physics == PHYS_Walking )
		{
			HitNormal.Z = 0;
			Dir.Z = 0;
			HitNormal = HitNormal.SafeNormal();
			Dir = Dir.SafeNormal();
		}
		if ( Controller->MinHitWall < (Dir | HitNormal) )
		{
			if ( Controller->bNotifyFallingHitWall && (Physics == PHYS_Falling) )
			{
				FVector OldVel = Velocity;
				Controller->eventNotifyFallingHitWall(HitNormal, HitActor);
				if ( Velocity != OldVel )
					bJustTeleported = true;
			}
			return;
		}
		// give controller the opportunity to handle the hitwall event instead of the controlled pawn
		if ( Controller->eventNotifyHitWall(HitNormal, HitActor) )
			return;
		if ( Physics != PHYS_Falling )
		{
			if ( (Physics == PHYS_Walking) && !IsHumanControlled() && bCanCrouch && !bIsCrouched )
			{
				// if not currently crouched, try moving crouched, maybe with step up
				if ( CanCrouchWalk( Location, Location + CylinderComponent->CollisionRadius*Dir, HitActor) )
				{
					return;
				}
				FCheckResult Hit(1.f);
				GWorld->MoveActor(this,FVector(0,0,-1.f * MaxStepHeight), Rotation, 0, Hit);
				if ( CanCrouchWalk( Location, Location + CylinderComponent->CollisionRadius*Dir, HitActor) )
				{
					return;
				}
			}
			if ( Controller && HitActor->bWorldGeometry && GIsGame)
			{
				Controller->AdjustFromWall(HitNormal, HitActor);
			}
		}
		else if ( Controller && Controller->bNotifyFallingHitWall )
		{
			FVector OldVel = Velocity;
			Controller->eventNotifyFallingHitWall(HitNormal, HitActor);
			if ( Velocity != OldVel )
				bJustTeleported = true;
		}
	}
	eventHitWall(HitNormal, HitActor, WallHit.Component);
}

UBOOL AActor::ShrinkCollision(AActor *HitActor, UPrimitiveComponent* HitComponent, const FVector &StartLocation)
{
	return false;
}

UBOOL AProjectile::ShrinkCollision(AActor *HitActor, UPrimitiveComponent* HitComponent, const FVector &StartLocation)
{
	if ( bSwitchToZeroCollision	)
	{
		bSwitchToZeroCollision = FALSE;
		if ( CylinderComponent
			&& ((CylinderComponent->CollisionHeight != 0.f) || (CylinderComponent->CollisionRadius != 0.f)) )
		{
			FCheckResult Hit(1.f);
			if (GWorld->SinglePointCheck(Hit, StartLocation, FVector(0,0,0), TRACE_AllBlocking | TRACE_StopAtAnyHit  | TRACE_ComplexCollision))
			{
				 // if no hit
				CylinderComponent->SetCylinderSize(0.f,0.f);
				ZeroCollider = HitActor;
				ZeroColliderComponent = HitComponent;
				bCollideComplex = TRUE;
				return TRUE;			
			}
		}
	}
	return FALSE;
}

void AProjectile::GrowCollision()
{
	if ( ZeroCollider )
	{
		FCheckResult Hit(1.f);
		AProjectile* DefaultProj = GetClass()->GetDefaultObject<AProjectile>();
		UCylinderComponent* DefaultCyl = DefaultProj->CylinderComponent;
		if ( DefaultCyl != NULL &&
			// Collide against the component that was hit.  If NULL, assume BSP.
			ZeroColliderComponent
				? ZeroColliderComponent->PointCheck(Hit, Location, FVector(DefaultCyl->CollisionRadius,DefaultCyl->CollisionRadius,DefaultCyl->CollisionHeight), DefaultProj->bCollideComplex ? TRACE_ComplexCollision : 0 )
				: GWorld->BSPPointCheck(Hit, NULL, Location, FVector(DefaultCyl->CollisionRadius,DefaultCyl->CollisionRadius,DefaultCyl->CollisionHeight))
			)
		{
			ZeroCollider = NULL;
			ZeroColliderComponent = NULL;
			bSwitchToZeroCollision = TRUE;
			bCollideComplex = DefaultProj->bCollideComplex;
			SetCollisionSize(DefaultCyl->CollisionRadius, DefaultCyl->CollisionHeight);
		}
	}
}

void AActor::processLanded(FVector const& HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations)
{
	CheckStillInWorld();

	if ( bDeleteMe )
	{
		return;
	}

	FVector ZoneVel = PhysicsVolume->GetZoneVelocityForActor(this);
	if ( PhysicsVolume->bBounceVelocity && !ZoneVel.IsZero() )
	{
		Velocity = ZoneVel + FVector(0.f, 0.f, 2.f * UCONST_ACTORMAXSTEPHEIGHT);
		return;
	}

	eventLanded(HitNormal, HitActor);
	if ( bDeleteMe )
	{
		return;
	}
	if (Physics == PHYS_Falling)
	{
		setPhysics(PHYS_None, HitActor, HitNormal);
		Velocity = FVector(0,0,0);
	}
	if ( bOrientOnSlope && (Physics == PHYS_None) )
	{
		// rotate properly onto slope
		FCheckResult Hit(1.f);
		FRotator NewRotation = FindSlopeRotation(HitNormal,Rotation);
		GWorld->MoveActor(this, FVector(0,0,0), NewRotation, 0, Hit);
	}
}

void APawn::processLanded(FVector const& HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations)
{
	// We want to make sure that this is a valid landing spot, and not a very small protrusion (like a BSP cut)
	// To do this, we trace down with a slightly smaller box.  If it doesn't hit a floor, we bounce this pawn off of this protrusion
	FCheckResult Hit(1.f);
	FVector ColLocation = CollisionComponent ? Location + CollisionComponent->Translation : Location;
	GWorld->SingleLineCheck(Hit, this, ColLocation - FVector(0,0,0.2f * CylinderComponent->CollisionHeight + 2.f*LedgeCheckThreshold),
		ColLocation, TRACE_AllBlocking|TRACE_StopAtAnyHit, 0.9f * GetCylinderExtent());

	if( Hit.Time == 1.f ) //Not a valid landing
	{
		FVector Adjusted = Location;
		if( GWorld->FindSpot(1.1f * GetCylinderExtent(), Adjusted, bCollideComplex, this) && (Adjusted != Location) )
		{
			GWorld->FarMoveActor(this, Adjusted, 0, 0);
			Velocity.X += 0.2f * GroundSpeed * (appFrand() - 0.5f);
			Velocity.Y += 0.2f * GroundSpeed * (appFrand() - 0.5f);
			//@HACK: increment/handle failure counter to make sure Pawns don't get stuck here
			FailedLandingCount++;
			if (FailedLandingCount > 300)
			{
				eventTakeDamage(1000, Controller, Location, FVector(0.f, 0.f, 0.f), UDamageType::StaticClass());
			}
			else if (FailedLandingCount >= 150 && FailedLandingCount % 50 == 0)
			{
				Velocity.Z = Max<FLOAT>(JumpZ, 1.f);
			}
			return;
		}
	}
	
	//@HACK: reset failure counter
	FailedLandingCount = 0;

	Floor = HitNormal;
	if( !Controller || !Controller->eventNotifyLanded(HitNormal, HitActor) )
	{
		eventLanded(HitNormal, HitActor);
	}

	if( Physics == PHYS_Falling )
	{
		SetPostLandedPhysics(HitActor, HitNormal);
	}
	
	if( Physics == PHYS_Walking )
	{
		Acceleration = Acceleration.SafeNormal();
	}
	
	startNewPhysics(remainingTime, Iterations);

	if( Controller && Controller->bNotifyPostLanded )
	{
		Controller->eventNotifyPostLanded();
	}
}

/** Set new physics after landing */
void APawn::SetPostLandedPhysics(AActor *HitActor, FVector HitNormal)
{
	if( Health > 0 )
	{
		setPhysics(WalkingPhysics, HitActor, HitNormal);
	}
	else
	{
		setPhysics(PHYS_None, HitActor, HitNormal);
	}
}

FVector APawn::NewFallVelocity(FVector OldVelocity, FVector OldAcceleration, FLOAT timeTick)
{
	FLOAT NetBuoyancy = 0.f;
	FLOAT NetFluidFriction = 0.f;
	GetNetBuoyancy(NetBuoyancy, NetFluidFriction);

	FVector NewVelocity = OldVelocity * (1 - NetFluidFriction * timeTick)
			+ OldAcceleration * (1.f - NetBuoyancy) * timeTick;

	return NewVelocity;
}

void APawn::physFalling(FLOAT deltaTime, INT Iterations)
{
	//bound acceleration, falling object has minimal ability to impact acceleration
	FLOAT BoundSpeed = 0; //Bound final 2d portion of velocity to this if non-zero

	if( Controller )
	{
		Controller->PreAirSteering(deltaTime);
	}

	// Is the Pawn using root motion?
	const UBOOL bDoRootMotionVelocity	= (Mesh && Mesh->RootMotionMode == RMM_Velocity && Mesh->PreviousRMM != RMM_Ignore && !bForceRegularVelocity);
	const UBOOL bDoRootMotionAccel		= (Mesh && Mesh->RootMotionMode == RMM_Accel && Mesh->PreviousRMM != RMM_Ignore && !bForceRegularVelocity);
	const UBOOL	bDoRootMotion			= (bDoRootMotionVelocity ||  bDoRootMotionAccel);

	// If doing root motion, get root motion velocity.
	if( bDoRootMotion )
	{
		// bForceRMVelocity is set when replaying moves that used root motion derived velocity on a client in a network game
		// the velocity can't be derived again, since the animation state isn't saved.
		if( bForceRMVelocity )
		{
			Velocity = RMVelocity;
		}
		else
		{
			const FVector CurrentVelocity		= Velocity;
			const FVector RootMotionTranslation	= bDoRootMotion ? Mesh->RootMotionDelta.GetTranslation() : FVector::ZeroVector;

			// check to see if acceleration is being driven by animation
			if( bDoRootMotionAccel )
			{
				// When falling, only have root motion take over X and Y, not Z.
				Velocity		= RootMotionTranslation / deltaTime;
				Velocity.Z		= CurrentVelocity.Z;

				// Do not allow any sort of AirControl when doing root motion accel.
				Acceleration	= FVector(0.f);
				RMVelocity		= Velocity;

				DEBUGPHYSONLY(debugf(TEXT("%3.3f [%s] bDoRootMotionAccel Move: %3.3f, Vect: %s"), GWorld->GetTimeSeconds(), *GetName(), Velocity.Size() * deltaTime, *(Velocity*deltaTime).ToString());)
			}

			// We've used root motion, clear the accumulator.
			Mesh->RootMotionDelta.SetTranslation(FVector::ZeroVector);
		}
	}

	FVector RealAcceleration = Acceleration;
	FCheckResult Hit(1.f);

	// test for slope to avoid using air control to climb walls
	FLOAT TickAirControl = AirControl;
	Acceleration.Z = 0.f;
	if( !bDoRootMotion && TickAirControl > 0.05f )
	{
		FVector TestWalk = ( TickAirControl * AccelRate * Acceleration.SafeNormal() + Velocity ) * deltaTime;
		TestWalk.Z = 0.f;
		if(!TestWalk.IsZero())
		{
			FVector ColLocation = CollisionComponent ? Location + CollisionComponent->Translation : Location;
			GWorld->SingleLineCheck( Hit, this, ColLocation + TestWalk, ColLocation, TRACE_World|TRACE_StopAtAnyHit, FVector( CylinderComponent->CollisionRadius, CylinderComponent->CollisionRadius, CylinderComponent->CollisionHeight ) );
			if( Hit.Actor )
			{
				TickAirControl = 0.f;
			}
		}
	}

	if( !bDoRootMotion && bLimitFallAccel )
	{
		// Boost maxAccel to increase player's control when falling
		FLOAT	maxAccel		= AccelRate * TickAirControl;
		FVector Velocity2D		= Velocity;
				Velocity2D.Z	= 0.f;
		FLOAT	speed2d			= Velocity2D.Size2D();

		if( (speed2d < 10.f) && (TickAirControl > 0.f) ) //allow initial burst
		{
			maxAccel = maxAccel + (10.f - speed2d)/deltaTime;
		}
		else if( speed2d >= GroundSpeed )
		{
			if( TickAirControl <= 0.05f )
			{
				maxAccel = 1.f;
			}
			else
			{
				BoundSpeed = speed2d;
			}
		}

		if( Acceleration.SizeSquared() > maxAccel * maxAccel )
		{
			Acceleration = Acceleration.SafeNormal();
			Acceleration *= maxAccel;
		}
	}

	if( Controller )
	{
		Controller->PostAirSteering(deltaTime);
	}

	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.1f;
	FVector OldLocation = Location;

	while( (remainingTime > 0.f) && (Iterations < 8) )
	{
		Iterations++;

		if( remainingTime > 0.05f )
		{
			timeTick = Min(0.05f, remainingTime * 0.5f);
		}
		else 
		{
			timeTick = remainingTime;
		}

		remainingTime -= timeTick;
		OldLocation = Location;
		bJustTeleported = 0;

		FVector OldVelocity = Velocity;
		Velocity = NewFallVelocity(OldVelocity, Acceleration + FVector(0.f,0.f,GetGravityZ()),timeTick);
		if( Controller && Controller->bNotifyApex && (Velocity.Z <= 0.f) )
		{
			// Just passed jump apex since now going down, so notify controller that this happened
			// Set bJustTeleported to TRUE so we don't stomp on any velocity changes made by game script
			bJustTeleported = TRUE;
			Controller->bNotifyApex = FALSE;
			Controller->NotifyJumpApex();
		}

		if( !bDoRootMotion && BoundSpeed != 0 )
		{
			// using air control, so make sure not exceeding acceptable speed
			FVector Vel2D = Velocity;
			Vel2D.Z = 0;
			if( Vel2D.SizeSquared() > BoundSpeed * BoundSpeed )
			{
				Vel2D = Vel2D.SafeNormal();
				Vel2D = Vel2D * BoundSpeed;
				Vel2D.Z = Velocity.Z;
				Velocity = Vel2D;
			}
		}
		FVector Adjusted = (Velocity + PhysicsVolume->GetZoneVelocityForActor(this)) * timeTick;

		GWorld->MoveActor(this, Adjusted, Rotation, MOVE_TraceHitMaterial, Hit);
		if( bDeleteMe )
		{
			return;
		}
		else if ( Physics == PHYS_Swimming ) //just entered water
		{
			remainingTime = remainingTime + timeTick * (1.f - Hit.Time);
			startSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if ( Hit.Time < 1.f )
		{
			if ( Hit.Actor && Hit.Actor->GetAPawn() && (Adjusted.Z > 0.f) && (Hit.Time == 0.f) && (Hit.Normal.Z == 1.f) )
			{
				// used to address pawns getting stuck on each other due to precision issues when Z component of location is far from the origin
				eventStuckOnPawn(Hit.Actor->GetAPawn());
			}
			if (Hit.Normal.Z >= WalkableFloorZ)
			{
				remainingTime += timeTick * (1.f - Hit.Time);
				if (!bJustTeleported && (Hit.Time > 0.1f) && (Hit.Time * timeTick > 0.003f) )
					Velocity = (Location - OldLocation)/(timeTick * Hit.Time);
				processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
				return;
			}
			else
			{
				if( !RealAcceleration.IsZero() || !Controller || !Controller->AirControlFromWall(timeTick, RealAcceleration) )
				{
					processHitWall(Hit, timeTick);
					
					// If we've changed physics mode, abort.
					if( bDeleteMe || Physics != PHYS_Falling )
					{
						return;
					}
				}
				FVector OldHitNormal = Hit.Normal;
				FVector Delta = CalculateSlopeSlide(Adjusted, Hit);

				if( (Delta | Adjusted) >= 0.f )
				{
					GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
					if (Hit.Time < 1.f) //hit second wall
					{
						if ( Hit.Normal.Z >= WalkableFloorZ )
						{
							remainingTime = 0.f;
							processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
							return;
						}

						processHitWall(Hit, timeTick);
						
						// If we've changed physics mode, abort.
						if( bDeleteMe || Physics != PHYS_Falling )
						{
							return;
						}

						FVector DesiredDir = Adjusted.SafeNormal();
						TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);

						// bDitch=TRUE means that pawn is straddling two slopes, neither of which he can stand on
						UBOOL bDitch = ( (OldHitNormal.Z > 0.f) && (Hit.Normal.Z > 0.f) && (Delta.Z == 0.f) && ((Hit.Normal | OldHitNormal) < 0.f) );
						GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
						if ( bDitch || (Hit.Normal.Z >= WalkableFloorZ) )
						{
							remainingTime = 0.f;
							processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
							return;
						}
					}
				}

				// Calculate average velocity based on actual movement after considering collisions
				FLOAT OldVelZ = OldVelocity.Z;
				OldVelocity = (Location - OldLocation)/timeTick;

				// Use average velocity for XY movement (no acceleration except for air control in those axes), but want actual velocity in Z axis
				OldVelocity.Z = OldVelZ;
			}
		}

		if (!bDoRootMotion && !bJustTeleported && Physics != PHYS_None)
		{
			// refine the velocity by figuring out the average actual velocity over the tick, and then the final velocity.
			// This particularly corrects for situations where level geometry affected the fall.
			Velocity = (Location - OldLocation)/timeTick; //actual average velocity
			if( (Velocity.Z < OldVelocity.Z) || (OldVelocity.Z >= 0.f) )
			{
				Velocity = 2.f * Velocity - OldVelocity; //end velocity has 2* accel of avg
			}

			if( Velocity.SizeSquared() > Square(GetTerminalVelocity()) )
			{
				Velocity = Velocity.SafeNormal();
				Velocity *= GetTerminalVelocity();
			}
		}
	}

	if( Controller )
	{
		Controller->PostPhysFalling(deltaTime);
	}

	Acceleration = RealAcceleration;
}

void AActor::physFalling(FLOAT deltaTime, INT Iterations)
{
	CheckStillInWorld();

	//bound acceleration, falling object has minimal ability to impact acceleration
	FVector RealAcceleration = Acceleration;
	FCheckResult Hit(1.f);
	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.1f;
	int numBounces = 0;
	GrowCollision();
	FVector OldLocation = Location;

	while ( (remainingTime > 0.f) && (Iterations < 8) )
	{
		Iterations++;
		if (remainingTime > 0.05f)
			timeTick = Min(0.05f, remainingTime * 0.5f);
		else timeTick = remainingTime;

		remainingTime -= timeTick;
		OldLocation = Location;
		bJustTeleported = 0;

		FVector OldVelocity = Velocity;
		FLOAT NetBuoyancy = 0.f;
		FLOAT NetFluidFriction = 0.f;
		GetNetBuoyancy(NetBuoyancy, NetFluidFriction);
		Velocity = OldVelocity * (1.f - NetFluidFriction * timeTick) + Acceleration;
		Velocity.Z += GetGravityZ() * (1.f - NetBuoyancy) * timeTick;

		FVector Adjusted = (Velocity + PhysicsVolume->GetZoneVelocityForActor(this)) * timeTick;

		FVector TmpVelocity = Velocity;

		GWorld->MoveActor(this, Adjusted, Rotation, 0, Hit);

		if (Velocity != TmpVelocity)
		{
			bJustTeleported = TRUE;
		}

		if ( bDeleteMe )
			return;

		// if actor with box collision hit something ,but wants to shrink to zero extent to pass through, then move again
		if ( (Hit.Time < 1.f) && ShrinkCollision(Hit.Actor, Hit.Component, OldLocation) )
		{
			Adjusted = (Velocity + PhysicsVolume->GetZoneVelocityForActor(this)) * timeTick * (1.f - Hit.Time);
			GWorld->MoveActor(this, Adjusted, Rotation, 0, Hit);
			if ( bDeleteMe )
				return;
		}
		if ( Hit.Time < 1.f )
		{
			if (bBounce)
			{
				processHitWall(Hit, timeTick);
				if (bDeleteMe || Physics != PHYS_Falling)
					return;
				else if ( numBounces < 2 )
					remainingTime += timeTick * (1.f - Hit.Time);
				numBounces++;
				bJustTeleported = TRUE; // don't average velocity, since bounced
			}
			else
			{
				if (Hit.Normal.Z >= UCONST_MINFLOORZ)
				{
					// hit the ground
					remainingTime += timeTick * (1.f - Hit.Time);
					if (!bJustTeleported && (Hit.Time > 0.1f) && (Hit.Time * timeTick > 0.003f) )
					{
						Velocity = (Location - OldLocation)/(timeTick * Hit.Time);
					}
					processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
					return;
				}
				else
				{
					// hit a wall
					processHitWall(Hit, timeTick);
					if (bDeleteMe || Physics != PHYS_Falling)
						return;
					FVector OldHitNormal = Hit.Normal;
					FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
					if( (Delta | Adjusted) >= 0 )
					{
						if ( Delta.Z > 0.f ) // friction slows sliding up slopes
							Delta *= 0.5f;
						GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
						if ( bDeleteMe )
							return;
						if (Hit.Time < 1.f) //hit second wall
						{
							if ( Hit.Normal.Z >= UCONST_MINFLOORZ )
							{
								remainingTime = 0.f;
								processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
								return;
							}
							else
								processHitWall(Hit, timeTick);
		
							if ( bDeleteMe )
								return;
							FVector DesiredDir = Adjusted.SafeNormal();
							TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
							if ( bDeleteMe )
								return;
							UBOOL bDitch = ( (OldHitNormal.Z > 0) && (Hit.Normal.Z > 0) && (Delta.Z == 0) && ((Hit.Normal | OldHitNormal) < 0) );
							GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
							if ( bDeleteMe )
								return;
							if ( bDitch || (Hit.Normal.Z >= UCONST_MINFLOORZ) )
							{
								remainingTime = 0.f;
								processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
								return;
							}
						}
					}

					// Calculate average velocity based on actual movement after considering collisions
					FLOAT OldVelZ = OldVelocity.Z;
					OldVelocity = (Location - OldLocation)/timeTick;

					// Use average velocity for XY movement (no acceleration except for air control in those axes), but want actual velocity in Z axis
					OldVelocity.Z = OldVelZ;
				}
			}
		}

		if ( !bJustTeleported )
		{
			// refine the velocity by figuring out the average actual velocity over the tick, and then the final velocity.
			// This particularly corrects for situations where level geometry affected the fall.
			Velocity = (Location - OldLocation)/timeTick; //actual average velocity
			if ( (Velocity.Z < OldVelocity.Z) || (OldVelocity.Z >= 0) )
				Velocity = 2.f * Velocity - OldVelocity; //end velocity has 2* accel of avg
			if (Velocity.SizeSquared() > Square(GetTerminalVelocity()))
			{
				Velocity = Velocity.SafeNormal();
				Velocity *= GetTerminalVelocity();
			}
		}
	}

	Acceleration = RealAcceleration;
}


FVector APawn::CalculateSlopeSlide(const FVector& Adjusted, const FCheckResult& Hit)
{
	FVector Result = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);

	// prevent boosting up slopes
	if ( Result.Z > 0.f )
	{
    	Result.Z = ::Min(Result.Z, Adjusted.Z * (1.f - Hit.Time));
	}
	return Result;
}

void APawn::startSwimming(FVector OldLocation, FVector OldVelocity, FLOAT timeTick, FLOAT remainingTime, INT Iterations)
{
	if ( !bJustTeleported )
	{
		if ( timeTick > 0.f )
			Velocity = (Location - OldLocation)/timeTick; //actual average velocity
		Velocity = 2.f * Velocity - OldVelocity; //end velocity has 2* accel of avg
		if (Velocity.SizeSquared() > Square(GetTerminalVelocity()))
		{
			Velocity = Velocity.SafeNormal();
			Velocity *= GetTerminalVelocity();
		}
	}
	FVector End = findWaterLine(Location, OldLocation);
	FLOAT waterTime = 0.f;
	if (End != Location)
	{	
		waterTime = timeTick * (End - Location).Size()/(Location - OldLocation).Size();
		remainingTime += waterTime;
		FCheckResult Hit(1.f);
		GWorld->MoveActor(this, End - Location, Rotation, 0, Hit);
	}
	if ((Velocity.Z > 2.f*SWIMBOBSPEED) && (Velocity.Z < 0.f)) //allow for falling out of water
		Velocity.Z = SWIMBOBSPEED - Velocity.Size2D() * 0.7f; //smooth bobbing
	if ( (remainingTime > 0.01f) && (Iterations < 8) )
		physSwimming(remainingTime, Iterations);

}

void APawn::physFlying(FLOAT deltaTime, INT Iterations)
{
	FVector AccelDir;

	if( Acceleration.IsZero() )
	{
		AccelDir = Acceleration;
	}
	else
	{
		AccelDir = Acceleration.SafeNormal();
	}

	CalcVelocity(AccelDir, deltaTime, AirSpeed, 0.5f * PhysicsVolume->FluidFriction, 1, 0, 0);

	Iterations++;
	bJustTeleported = 0;

	FVector OldLocation = Location;
	const FVector Adjusted = (Velocity + PhysicsVolume->GetZoneVelocityForActor(this)) * deltaTime;
	
	DEBUGPHYSONLY(debugf(TEXT("%3.3f [%s] physFlying OriginalMove: %s, Adjusted: %s"), GWorld->GetTimeSeconds(), *GetName(), *(Velocity*deltaTime).ToString(), *Adjusted.ToString());)
	
	FCheckResult Hit(1.f);
	GWorld->MoveActor(this, Adjusted, Rotation, 0, Hit);

	if( Hit.Time < 1.f )
	{
		DEBUGPHYSONLY(debugf(TEXT("-- hit %s"),Hit.Actor != NULL ? *Hit.Actor->GetName() : TEXT("NULL"));)
		Floor = Hit.Normal;
		FVector GravDir = FVector(0,0,-1);
		FVector DesiredDir = Adjusted.SafeNormal();
		FVector VelDir = Velocity.SafeNormal();
		FLOAT UpDown = GravDir | VelDir;
		
		if( (Abs(Hit.Normal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) )
		{
			FLOAT stepZ = Location.Z;
			stepUp(GravDir, DesiredDir, Adjusted * (1.f - Hit.Time), Hit);
			OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
		}
		else
		{
			processHitWall(Hit, deltaTime);
			//adjust and try again
			FVector OldHitNormal = Hit.Normal;
			FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
				
				if( Hit.Time < 1.f ) //hit second wall
				{
					processHitWall(Hit, deltaTime*(1.f-Hit.Time));
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
				}
			}
		}
	}
	else
	{
		Floor = FVector(0.f,0.f,1.f);
	}

	if( !bJustTeleported )
	{
		Velocity = (Location - OldLocation) / deltaTime;
		DEBUGPHYSONLY( debugf(TEXT("%3.3f [%s] physFlying Adjusted velocity to: %s, DeltaMove: %s"), GWorld->GetTimeSeconds(), *GetName(), *Velocity.ToString(), *(Velocity*deltaTime).ToString()); )
	}
}


/* Swimming uses gravity - but scaled by (1.f - buoyancy)
This is used only by pawns
*/
FLOAT APawn::Swim(FVector Delta, FCheckResult &Hit)
{
	FVector Start = Location;
	FLOAT airTime = 0.f;
	GWorld->MoveActor(this, Delta, Rotation, 0, Hit);

	if ( !PhysicsVolume->bWaterVolume ) //then left water
	{
		FVector End = findWaterLine(Start, Location);
		if (End != Location)
		{
			airTime = (End - Location).Size()/Delta.Size();
			if ( ((Location - Start) | (End - Location)) > 0.f )
				airTime = 0.f;
			GWorld->MoveActor(this, End - Location, Rotation, 0, Hit);
		}
	}
	return airTime;
}

//get as close to waterline as possible, staying on same side as currently
FVector APawn::findWaterLine(FVector InWater, FVector OutofWater)
{
	FCheckResult Hit(1.f);
	FMemMark Mark(GMainThreadMemStack);
	FCheckResult* FirstHit = GWorld->MultiLineCheck
	(
		GMainThreadMemStack,
		InWater,
		OutofWater,
		FVector(0,0,0),
		TRACE_Volumes | TRACE_World,
		this
	);

	// Skip owned actors and return the one nearest actor.
	for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
	{
		if( !IsOwnedBy( Check->Actor ) )
		{
			if( Check->Actor->bWorldGeometry )
			{
				Mark.Pop();
				return OutofWater;		// never hit a water volume
			}
			else
			{
				APhysicsVolume *W = Cast<APhysicsVolume>(Check->Actor);
				if ( W && W->bWaterVolume )
				{
					FVector Dir = InWater - OutofWater;
					Dir = Dir.SafeNormal();
					FVector Result = Check->Location;
					if ( W == PhysicsVolume )
						Result += 0.1f * Dir;
					else
						Result -= 0.1f * Dir;
					Mark.Pop();
					return Result;
				}
			}
		}
	}
	Mark.Pop();
	return OutofWater;
}

void AActor::GetNetBuoyancy(FLOAT &NetBuoyancy, FLOAT &NetFluidFriction)
{
}

/*
GetNetBuoyancy()
determine how deep in water actor is standing:
0 = not in water,
1 = fully in water
*/
void APawn::GetNetBuoyancy(FLOAT &NetBuoyancy, FLOAT &NetFluidFriction)
{
	APhysicsVolume *WaterVolume = NULL;
	FLOAT depth = 0.f;

	if ( PhysicsVolume->bWaterVolume )
	{
		FLOAT CollisionHeight, CollisionRadius;
		GetBoundingCylinder(CollisionRadius, CollisionHeight);

		WaterVolume = PhysicsVolume;
		if ( (CollisionHeight == 0.f) || (Buoyancy == 0.f) )
			depth = 1.f;
		else
		{
			FCheckResult Hit(1.f);
		    if ( PhysicsVolume->CollisionComponent )
			    PhysicsVolume->CollisionComponent->LineCheck(Hit,
									    Location - FVector(0.f,0.f,CollisionHeight),
									    Location + FVector(0.f,0.f,CollisionHeight),
									    FVector(0.f,0.f,0.f),
									    0);
		    if ( Hit.Time == 1.f )
			    depth = 1.f;
		    else
			    depth = (1.f - Hit.Time);
		}
	}
	if ( WaterVolume )
	{
		NetBuoyancy = Buoyancy * depth;
		NetFluidFriction = WaterVolume->FluidFriction * depth;
	}
}

/*
Encompasses()
returns true if point is within the volume
*/
INT AVolume::Encompasses(FVector point,FVector Extent/*=FVector(0.f)*/)
{
	if (CollisionComponent == NULL)
	{
		return FALSE;
	}
	else
	{
		FCheckResult Hit(1.f);
		if (Brush != NULL)
		{
			//	debugf(TEXT("%s brush pointcheck %d at %f %f %f"),*GetName(),!Brush->PointCheck(Hit,this,	point, FVector(0.f,0.f,0.f), 0), point.X, point.Y,point.Z);
			return !Brush->PointCheck(Hit,this,	NULL, point, Extent);
		}
		else
		{
			return !CollisionComponent->PointCheck(Hit, point, Extent, 0);
		}
	}
}

void APawn::physSwimming(FLOAT deltaTime, INT Iterations)
{
	FLOAT NetBuoyancy = 0.f;
	FLOAT NetFluidFriction  = 0.f;
	GetNetBuoyancy(NetBuoyancy, NetFluidFriction);
	if ( (Velocity.Z > FASTWALKSPEED) && (Buoyancy != 0.f) )
	{
		//damp positive Z out of water
		Velocity.Z = Velocity.Z * NetBuoyancy/Buoyancy;
	}

	Iterations++;
	FVector OldLocation = Location;
	bJustTeleported = 0;
	FVector AccelDir;
	if ( Acceleration.IsZero() )
		AccelDir = Acceleration;
	else
		AccelDir = Acceleration.SafeNormal();
	CalcVelocity(AccelDir, deltaTime, WaterSpeed, 0.5f * PhysicsVolume->FluidFriction, 1, 0, 1);
	FLOAT velZ = Velocity.Z;
	FVector Adjusted = (Velocity + PhysicsVolume->GetZoneVelocityForActor(this) * 25 * deltaTime) * deltaTime;
	FCheckResult Hit(1.f);
	FLOAT remainingTime = deltaTime * Swim(Adjusted, Hit);

	//may have left water - if so, script might have set new physics mode
	if ( Physics != PHYS_Swimming )
	{
		startNewPhysics(remainingTime, Iterations);
		return;
	}

	if (Hit.Time < 1.f)
	{
		Floor = Hit.Normal;
		FLOAT stepZ = Location.Z;
		FVector RealVelocity = Velocity;
		Velocity.Z = 1.f;	// HACK: since will be moving up, in case pawn leaves the water
		stepUp(-1.f*Hit.Normal, Adjusted.SafeNormal(), Adjusted * (1.f - Hit.Time), Hit);
		//may have left water - if so, script might have set new physics mode
		if ( Physics != PHYS_Swimming )
		{
			startNewPhysics(remainingTime, Iterations);
			return;
		}
		Velocity = RealVelocity;
		OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
	}
	else
		Floor = FVector(0.f,0.f,1.f);

	if (!bJustTeleported && (remainingTime < deltaTime))
	{
		UBOOL bWaterJump = !PhysicsVolume->bWaterVolume;
		if (bWaterJump)
			velZ = Velocity.Z;
		Velocity = (Location - OldLocation) / (deltaTime - remainingTime);
		if (bWaterJump)
			Velocity.Z = velZ;
	}

	if ( !PhysicsVolume->bWaterVolume )
	{
		if (Physics == PHYS_Swimming)
			setPhysics(PHYS_Falling); //in case script didn't change it (w/ zone change)
		if ((Velocity.Z < 160.f) && (Velocity.Z > 0)) //allow for falling out of water
			Velocity.Z = 40.f + Velocity.Size2D() * 0.4; //smooth bobbing
	}

	//may have left water - if so, script might have set new physics mode
	if ( Physics != PHYS_Swimming )
		startNewPhysics(remainingTime, Iterations);
}

/** PhysWalking for Actors is a fast mini version of pawn walking physics
 * it doesn't emulate friction, a max speed, or anything
 * all it does is force the object to stick to the ground and climb up/down small steps
 */
void AActor::physWalking(FLOAT deltaTime, INT Iterations)
{
	// no z-axis movement while walking
	Acceleration.Z = 0;
	Velocity.Z = 0;

	// do the move
	Velocity += Acceleration * deltaTime;
	AngularVelocity = FVector(0.f);
	FVector Delta = Velocity * deltaTime;
	moveSmooth(Delta);

	// check floor
	FVector GravDir = (GetGravityZ() > 0.f) ? FVector(0.f, 0.f, 1.f) : FVector(0.f, 0.f, -1.f);
	FVector Down = GravDir * (UCONST_ACTORMAXSTEPHEIGHT + MAXSTEPHEIGHTFUDGE);
	FCheckResult Hit(1.f);
	FVector ColLocation = (CollisionComponent != NULL) ? Location + CollisionComponent->Translation : Location;
	GWorld->SingleLineCheck(Hit, this, ColLocation + Down, ColLocation, TRACE_AllBlocking, GetCylinderExtent());
	FLOAT FloorDist = Hit.Time * (UCONST_ACTORMAXSTEPHEIGHT + MAXSTEPHEIGHTFUDGE);
	if (Hit.Normal.Z < UCONST_MINFLOORZ && !Delta.IsNearlyZero() && (Delta | Hit.Normal) < 0)
	{
		// slide down slope
		FVector Slide = FVector(0.f, 0.f, UCONST_ACTORMAXSTEPHEIGHT) - Hit.Normal * (FVector(0.f, 0.f, UCONST_ACTORMAXSTEPHEIGHT) | Hit.Normal);
		GWorld->MoveActor(this, -1 * Slide, Rotation, 0, Hit); 
		if (Hit.Actor != Base && Physics == PHYS_Walking)
		{
			SetBase(Hit.Actor, Hit.Normal);
		}
	}
	else if (Hit.Time < 1.f && (Hit.Actor != Base || FloorDist > MAXFLOORDIST))
	{
		// move down to correct position 
		GWorld->MoveActor(this, Down, Rotation, 0, Hit);
		if (Hit.Actor != Base && Physics == PHYS_Walking)
		{
			SetBase(Hit.Actor, Hit.Normal);
		}
	}
	else if (FloorDist < MINFLOORDIST)
	{
		// move up to correct position (average of MAXFLOORDIST and MINFLOORDIST above floor)
		GWorld->MoveActor(this, FVector(0.f, 0.f, 0.5f * (MINFLOORDIST + MAXFLOORDIST) - FloorDist), Rotation, 0, Hit);
	}
	else if (Hit.Time >= 1.f || Hit.Normal.Z < UCONST_MINFLOORZ)
	{
		// fall
		eventFalling();
		if (Physics == PHYS_Walking)
		{
			setPhysics(PHYS_Falling); // default if script didn't change physics
		}
	}
}

/* PhysProjectile is tailored for projectiles
*/
void AActor::physProjectile( FLOAT DeltaTime, INT Iterations )
{
	CheckStillInWorld();

	FLOAT	RemainingTime	= DeltaTime;
	INT		NumBounces		= 0;
	bJustTeleported			= 0;
	DWORD	MoveFlags		= 0;

	FCheckResult Hit(1.f);
	if( bCollideActors )
	{
		GrowCollision();
	}

	// Apply acceleration
	if( !Acceleration.IsZero() )
	{
		//debugf(TEXT("%s has acceleration!"),*GetName());
		Velocity = Velocity	+ Acceleration * DeltaTime;
		BoundProjectileVelocity();
	}

	while( RemainingTime > 0.f && !bDeleteMe && Physics != PHYS_None )
	{
		Iterations++;

		FVector StartLocation	= Location;
		FLOAT	TimeTick		= RemainingTime;
		FVector Adjusted		= Velocity * TimeTick;
		RemainingTime			= 0.f;
		Hit.Time				= 1.f;
		
		if( bProjectileMoveSingleBlocking )
		{
			MoveFlags |= MOVE_SingleBlocking;
		}
		GWorld->MoveActor( this, Adjusted, Rotation, MoveFlags, Hit );	

		if( bDeleteMe )
		{
			return;
		}

		if( Hit.Time<1.f && !bJustTeleported )
		{
			if( ShrinkCollision(Hit.Actor, Hit.Component, StartLocation) )
			{
				RemainingTime = TimeTick * (1.f - Hit.Time);
			}
			else
			{
				processHitWall(Hit, TimeTick);
				if( bDeleteMe )
				{
					return;
				}
				if( bBounce )
				{
					if( NumBounces < 2 )
					{
						RemainingTime = TimeTick * (1.f - Hit.Time);
					}
					NumBounces++;
					if( Physics == PHYS_Falling )
					{
						physFalling(RemainingTime, Iterations);
						return;
					}
				}
			}
		}
	}
}

void AProjectile::physProjectile(FLOAT DeltaTime, INT Iterations)
{
	Super::physProjectile(DeltaTime, Iterations);

	if (bRotationFollowsVelocity)
	{
		FRotator const NewRot = Velocity.Rotation();
		AngularVelocity = CalcAngularVelocity(Rotation, NewRot, DeltaTime);
		Rotation = NewRot;
	}
}

void AActor::BoundProjectileVelocity()
{
	if ( !Acceleration.IsZero() && (Velocity.SizeSquared() > Acceleration.SizeSquared()) )
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= Acceleration.Size();
	}
}
void AProjectile::BoundProjectileVelocity()
{
	if ( MaxSpeed > 0.f && Velocity.SizeSquared() > MaxSpeed * MaxSpeed )
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= MaxSpeed;

		// bRotationFollowsVelocity implies seeking behavior for accelerating projectiles - don't clear acceleration if seeking
		if ( !bRotationFollowsVelocity )
		{
			// clear acceleration, since have reached max speed
			Acceleration.X = 0.f;
			Acceleration.Y = 0.f;
			Acceleration.Z = 0.f;
		}
	}
}

/*
Move only in ClimbDir or -1 * ClimbDir, but also push into LookDir.
If leave ladder volume, then step pawn up onto ledge.
If hit ground, then change to walking
*/
void APawn::physLadder(FLOAT deltaTime, INT Iterations)
{
	Iterations++;
	FLOAT remainingTime = deltaTime;
	ALadderVolume *OldLadder = OnLadder;
	Velocity = FVector(0.f,0.f,0.f);

	if ( OnLadder && Controller && !Acceleration.IsZero() )
	{
		FCheckResult Hit(1.f);
		UBOOL bClimbUp = ((Acceleration | (OnLadder->ClimbDir + OnLadder->LookDir)) > 0.f);
		// First, push into ladder
		if ( !OnLadder->bNoPhysicalLadder && bClimbUp )
		{
			Velocity = OnLadder->LookDir * GroundSpeed;
			GWorld->MoveActor(this, OnLadder->LookDir * remainingTime * GroundSpeed, Rotation, 0, Hit);
			remainingTime = remainingTime * (1.f - Hit.Time);
			if ( !OnLadder )
			{
				if ( PhysicsVolume->bWaterVolume )
					setPhysics(PHYS_Swimming);
				else
					setPhysics(WalkingPhysics);
				startNewPhysics(remainingTime, Iterations);
				return;
			}
			if ( remainingTime == 0.f )
				return;
		}
		FVector AccelDir = Acceleration.SafeNormal();
		Velocity = FVector(0.f,0.f,0.f);

		// set up or down movement velocity
		if ( !OnLadder->bAllowLadderStrafing || (Abs(AccelDir | OnLadder->ClimbDir) > 0.1f) )
		Velocity = OnLadder->ClimbDir * LadderSpeed;
		if ( !bClimbUp )
			Velocity *= -1.f;

		// support moving sideways on ladder
		if ( OnLadder->bAllowLadderStrafing )
		{
			FVector LeftDir = OnLadder->LookDir ^ OnLadder->ClimbDir;
			LeftDir = LeftDir.SafeNormal();
			Velocity += (LeftDir | AccelDir) * LeftDir * LadderSpeed;
		}

		FVector MoveDir = Velocity * remainingTime;

		// move along ladder
		GWorld->MoveActor(this, MoveDir, Rotation, 0, Hit);
		remainingTime = remainingTime * (1.f - Hit.Time);

		if ( !OnLadder )
		{
			//Moved out of ladder, try to step onto ledge
			if ( MoveDir.Z * GetGravityZ() > 0.f )
			{
				setPhysics(PHYS_Falling);
				return;
			}
			FVector Out = MoveDir.SafeNormal();
			Out *= 1.1f * CylinderComponent->CollisionHeight;
			GWorld->MoveActor(this, Out, Rotation, 0, Hit);
			GWorld->MoveActor(this, 0.5f * OldLadder->LookDir * CylinderComponent->CollisionRadius, Rotation, 0, Hit);
			GWorld->MoveActor(this, -1.f * (Out + MoveDir), Rotation, 0, Hit);
			GWorld->MoveActor(this, (-0.5f * CylinderComponent->CollisionRadius + LADDEROUTPUSH) * OldLadder->LookDir , Rotation, 0, Hit);
			Velocity = FVector(0,0,0);
			if ( PhysicsVolume->bWaterVolume )
				setPhysics(PHYS_Swimming);
			else
				setPhysics(WalkingPhysics);
			startNewPhysics(remainingTime, Iterations);
			return;
		}	
		else if ( (Hit.Time < 1.f) && Hit.Actor->bWorldGeometry )
		{
			// hit ground
			FVector OldLocation = Location;
			MoveDir = OnLadder->LookDir * GroundSpeed * remainingTime;
			if ( !bClimbUp )
				MoveDir *= -1.f;

			// try to move along ground
			GWorld->MoveActor(this, MoveDir, Rotation, 0, Hit);
			if ( Hit.Time < 1.f )
			{
				FVector GravDir = FVector(0,0,-1);
				FVector DesiredDir = MoveDir.SafeNormal();
				stepUp(GravDir, DesiredDir, MoveDir, Hit);
				if ( OnLadder && (Physics != PHYS_Ladder) )
					setPhysics(PHYS_Ladder);
			}
			Velocity = (Location - OldLocation)/remainingTime;
		}
		else if ( !OnLadder->bNoPhysicalLadder && !bClimbUp )
		{
			FVector ClimbDir = OnLadder->ClimbDir;
			FVector PushDir = OnLadder->LookDir;
			GWorld->MoveActor(this, -1.f * ClimbDir * MaxStepHeight, Rotation, 0, Hit);
			FLOAT Dist = Hit.Time * MaxStepHeight;
			if ( Hit.Time == 1.f )
				GWorld->MoveActor(this, PushDir * deltaTime * GroundSpeed, Rotation, 0, Hit);
			GWorld->MoveActor(this, ClimbDir * Dist, Rotation, 0, Hit);
			if ( !OnLadder )
			{
				if ( PhysicsVolume->bWaterVolume )
					setPhysics(PHYS_Swimming);
				else
					setPhysics(WalkingPhysics);
			}
		}
	}
	
	if ( !Controller )
		setPhysics(PHYS_Falling);

}

/*
physSpider()

*/
#ifdef __GNUG__
int APawn::checkFloor(FVector Dir, FCheckResult &Hit)
#else
inline int APawn::checkFloor(FVector Dir, FCheckResult &Hit)
#endif
{
	GWorld->SingleLineCheck(Hit, 0, Location - MaxStepHeight * Dir, Location, TRACE_World, GetCylinderExtent());
	if (Hit.Time < 1.f)
	{
		SetBase(Hit.Actor, Hit.Normal);
		return 1;
	}
	return 0;
}

/* findNewFloor()
Helper function used by PHYS_Spider for determining what wall or floor to crawl on
*/
int APawn::findNewFloor(FVector OldLocation, FLOAT deltaTime, FLOAT remainingTime, int Iterations)
{
	//look for floor
	FCheckResult Hit(1.f);
	//debugf("Find new floor for %s", GetFullName());
	if ( checkFloor(FVector(0,0,1), Hit) )
		return 1;
	if ( checkFloor(FVector(0,1,0), Hit) )
		return 1;
	if ( checkFloor(FVector(0,-1,0), Hit) )
		return 1;
	if ( checkFloor(FVector(1,0,0), Hit) )
		return 1;
	if ( checkFloor(FVector(-1,0,0), Hit) )
		return 1;
	if ( checkFloor(FVector(0,0,-1), Hit) )
		return 1;

	// Fall
	eventFalling();
	if (Physics == PHYS_Spider)
		setPhysics(PHYS_Falling); //default if script didn't change physics
	if (Physics == PHYS_Falling)
	{
		FLOAT velZ = Velocity.Z;
		if (!bJustTeleported && (deltaTime > remainingTime))
			Velocity = (Location - OldLocation)/(deltaTime - remainingTime);
		Velocity.Z = velZ;
		if (remainingTime > 0.005f)
			physFalling(remainingTime, Iterations);
	}

	return 0;

}

void APawn::SpiderstepUp(const FVector& DesiredDir, const FVector& Delta, FCheckResult &Hit)
{
	FVector Down = -1.f * Floor * MaxStepHeight;

	if ( (Floor | Hit.Normal) < 0.1 )
	{
		// step up - treat as vertical wall
		GWorld->MoveActor(this, -1 * Down, Rotation, 0, Hit);
		GWorld->MoveActor(this, Delta, Rotation, 0, Hit);
	}
	else // walk up slope
	{
		Floor = Hit.Normal;
		Down = -1.f * Floor * MaxStepHeight;
		const FLOAT Dist = Delta.Size();
		const FVector newMoveDir = (Delta - Delta.ProjectOnTo(Floor)).SafeNormal() * Dist;
		GWorld->MoveActor(this, newMoveDir, Rotation, 0, Hit);
	}

	if (Hit.Time < 1.f)
	{
		if ( ((Floor | Hit.Normal) < 0.1) && (Hit.Time * Delta.SizeSquared() > MINSTEPSIZESQUARED) )
		{
			// try another step
			GWorld->MoveActor(this, Down, Rotation, 0, Hit);
			SpiderstepUp(DesiredDir, Delta*(1 - Hit.Time), Hit);
			return;
		}

		// Found a new floor
		FVector OldFloor = Floor;
		Floor = Hit.Normal;
		Down = -1.f * Floor * MaxStepHeight;

		//adjust and try again
		Hit.Normal.Z = 0;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector NewDelta;
		FVector OldHitNormal = Hit.Normal;

		FVector CrossY = Floor ^ OldFloor;
		CrossY.Normalize();
		FVector VecX = CrossY ^ OldFloor;
		VecX.Normalize();
		FLOAT X = VecX | Delta;
		FLOAT Y = CrossY | Delta;
		FLOAT Z = OldFloor | Delta;
		VecX = CrossY ^ Floor;
		NewDelta = X * VecX + Y * CrossY + Z * Floor;

		if( (NewDelta | Delta) >= 0 )
		{
			GWorld->MoveActor(this, NewDelta, Rotation, 0, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, NewDelta, Hit.Normal, OldHitNormal, Hit.Time);
				GWorld->MoveActor(this, NewDelta, Rotation, 0, Hit);
			}
		}
	}
	GWorld->MoveActor(this, Down, Rotation, 0, Hit);
}

void APawn::physSpider(FLOAT deltaTime, INT Iterations)
{
	if( !Controller )
	{
		return;
	}
	if( Floor.IsNearlyZero() && 
		!findNewFloor( Location, deltaTime, deltaTime, Iterations ) )
	{
		return;
	}


	FVector AccelDir = Acceleration.SafeNormal();
	CalcVelocity( AccelDir, deltaTime, GroundSpeed, PhysicsVolume->GroundFriction, 0, 1, 0 );
	FLOAT Mag = Velocity.Size();
	Velocity = (Velocity - Floor * (Floor | Velocity)).SafeNormal() * Mag;
	

	FVector DesiredMove = Velocity;
	Iterations++;

	//-------------------------------------------------------------------------------------------
	//Perform the move
	FCheckResult Hit(1.f);
	FVector OldLocation = Location;
	bJustTeleported = 0;

	FLOAT remainingTime = deltaTime;
	FLOAT timeTick;
	FLOAT MaxStepHeightSq = MaxStepHeight * MaxStepHeight;
	while ( (remainingTime > 0.f) && (Iterations < 8) )
	{
		Iterations++;

		// Subdivide moves to be no longer than 0.05 seconds for players, or no longer than the collision radius for non-players
		if( remainingTime > 0.05f && 
				(IsHumanControlled() ||
					(DesiredMove.SizeSquared() * remainingTime * remainingTime > 
					 Min(CylinderComponent->CollisionRadius * CylinderComponent->CollisionRadius, MaxStepHeightSq))) )
		{
				timeTick = Min(0.05f, remainingTime * 0.5f);
		}
		else 
		{
			timeTick = remainingTime;
		}

		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector subLoc = Location;
		UBOOL bZeroMove = Delta.IsNearlyZero();

		if( bZeroMove )
		{
			// If not moving, quick check if still on valid floor
			remainingTime = 0;
			FVector Foot = Location - CylinderComponent->CollisionHeight * Floor;
			GWorld->SingleLineCheck( Hit, this, Foot - 20 * Floor, Foot, TRACE_World );
			FLOAT FloorDist = Hit.Time * 20;
			bZeroMove = ((Base == Hit.Actor) && (FloorDist <= MAXFLOORDIST + CYLINDERREPULSION) && (FloorDist >= MINFLOORDIST + CYLINDERREPULSION));
		}
		else
		{
			// try to move forward
			GWorld->MoveActor( this, Delta, Rotation, 0, Hit );


			// If we bumped into something besides a pawn
			if( Hit.Time < 1.f && (!Hit.Actor || !Hit.Actor->GetAPawn()) )
			{
				// hit a barrier, try to step up
				FVector DesiredDir = Delta.SafeNormal();
				SpiderstepUp(DesiredDir, Delta * (1.f - Hit.Time), Hit);
			}

			if ( Physics == PHYS_Swimming ) //just entered water
			{
				startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		if( !bZeroMove )
		{
			//drop to floor
			FVector ColLocation = CollisionComponent ? Location + CollisionComponent->Translation : Location;
			GWorld->SingleLineCheck( Hit, this, ColLocation - Floor * (MaxStepHeight + MAXSTEPHEIGHTFUDGE), ColLocation, TRACE_AllBlocking, GetCylinderExtent() );
			if ( Hit.Time == 1.f )
			{
				GWorld->MoveActor(this, -8.f * Floor, Rotation, 0, Hit);
				// find new floor or fall
				if( !findNewFloor(Location, deltaTime, deltaTime, Iterations) )
				{
					return;
				}
			}
			else
			{
				Floor = Hit.Normal;
				GWorld->MoveActor(this, -1.f * Floor * (MaxStepHeight + MAXSTEPHEIGHTFUDGE), Rotation, 0, Hit);

				if ( Hit.Actor != Base )
				{
					SetBase(Hit.Actor, Hit.Normal);
				}
			}
		}
	}

	// make velocity reflect actual move
	if( !bJustTeleported )
	{
		Velocity = (Location - OldLocation) / deltaTime;
	}

	if( Controller )
	{
		Controller->PostPhysSpider( deltaTime );
	}
}

/** 
 * Use a movement track to update the current position and rotation.
 * Returns TRUE if actor actual moved, FALSE otherwise.
 */
UBOOL AActor::MoveWithInterpMoveTrack(UInterpTrackMove* MoveTrack, UInterpTrackInstMove* MoveInst, FLOAT CurInterpTime, FLOAT DeltaTime)
{
	FVector		NewPos = Location;
	FRotator	NewRot = Rotation;

	// Is the track enabled?
	UBOOL bIsTrackEnabled = !MoveTrack->IsDisabled();
	UInterpGroupInst* GrInst = Cast<UInterpGroupInst>( MoveInst->GetOuter() );
	if( GrInst != NULL )
	{
		USeqAct_Interp* Seq = Cast<USeqAct_Interp>( GrInst->GetOuter() );
		if( Seq != NULL )
		{
			if( MoveTrack->ActiveCondition == ETAC_GoreEnabled && !Seq->bShouldShowGore ||
				MoveTrack->ActiveCondition == ETAC_GoreDisabled && Seq->bShouldShowGore )
			{
				bIsTrackEnabled = FALSE;
			}
		}
	}


	if( !bIsTrackEnabled )
	{
		CurInterpTime = 0.0f;	
	}

	if (MoveTrack->GetLocationAtTime(MoveInst, CurInterpTime, NewPos, NewRot) == FALSE)
	{
		return FALSE;
	}

	FVector OldLocation = Location;
	FRotator OldRotation = Rotation;

	// Allow subclasses to adjust position
	AdjustInterpTrackMove(NewPos, NewRot, DeltaTime, MoveTrack->RotMode == IMR_Ignore);

	FCheckResult Hit(1.f);
	GWorld->MoveActor(this, NewPos - Location, NewRot, 0, Hit);

//	debugf(TEXT("[%s(%f)] OldLocation %s, NewPos %s, NewLocation %s"), *GetName(), CurInterpTime, *OldLocation.ToString(), *NewPos.ToString(), *Location.ToString());
	// wait until movement is done, now do rotation
	if ( MoveTrack->RotMode == IMR_Ignore )
	{
		FVector OldVelocity = Velocity;
		physicsRotation(DeltaTime, OldVelocity);
	}

	// set velocity for our desired move (so any events that get triggered, such as encroachment, know where we were trying to go)
	if(DeltaTime > KINDA_SMALL_NUMBER)
	{
		Velocity = (Location - OldLocation) / DeltaTime;
		AngularVelocity = CalcAngularVelocity(OldRotation, NewRot, DeltaTime);
	}
	else
	{
		Velocity = FVector(0.f);
		AngularVelocity = FVector(0.f);
	}

	// Did we actually move this frame?
	UBOOL bMovingNow = ((OldRotation != Rotation) || (OldLocation != Location));

	// If based on something - update the RelativeLocation and RelativeRotation so that its still correct with the new position.
	AActor* BaseActor = GetBase();
	if (BaseActor)
	{
		FMatrix BaseTM;
		// Look at bone we are attached to if attached to a skeletal mesh component
		if(BaseSkelComponent)
		{
			INT BoneIndex = BaseSkelComponent->MatchRefBone(BaseBoneName);
			if(BoneIndex != INDEX_NONE)
			{
				BaseTM = BaseSkelComponent->GetBoneMatrix(BoneIndex);
			}
			else
			{
				BaseTM = FRotationTranslationMatrix(BaseActor->Rotation, BaseActor->Location);
			}
		}
		// Not skeletal case - just use actor transform.
		else
		{
			BaseTM = FRotationTranslationMatrix(BaseActor->Rotation, BaseActor->Location);
		}

		FMatrix InvBaseTM = BaseTM.InverseSafe();
		FRotationTranslationMatrix ActorTM(Rotation,Location);

		FMatrix RelTM = ActorTM * InvBaseTM;
		RelativeLocation = RelTM.GetOrigin();
		RelativeRotation = RelTM.Rotator();
	}

	return bMovingNow;
}

/** 
 * Adjust Position/Rotation for Interp Track Move .
 * When Pawn is moving via InterpTrack, adjust Collision Height.
 * And set DesiredRotation to be Rot so that it doesn't keep re-correcting it.
 * If you override this, make sure to call the super or fix up the z position manually.
 */
void APawn::AdjustInterpTrackMove(FVector& Pos, FRotator& Rot, FLOAT DeltaTime, UBOOL bIgnoreRotation)
{
	// if cylinder component exists, adjust up to collision height
	if (CylinderComponent)
	{
		Pos.Z += CylinderComponent->CollisionHeight;

		// Only set rotation if the calling function wanted to
		if (!bIgnoreRotation)
		{
			SetDesiredRotation(Rot, FALSE, FALSE, 0.f);
		}
	}
}

void AActor::physInterpolating(FLOAT DeltaTime)
{
	UInterpTrackMove*		MoveTrack;
	UInterpTrackInstMove*	MoveInst;
	USeqAct_Interp*			Seq;

	UBOOL bMovingNow = FALSE;

	//debugf(TEXT("AActor::physInterpolating %f - %s"), GWorld->GetTimeSeconds(), *GetFName());

	// If we have a movement track currently working on this Actor, update position to co-incide with it.
	if( FindInterpMoveTrack(&MoveTrack, &MoveInst, &Seq) )
	{
		bMovingNow = MoveWithInterpMoveTrack(MoveTrack, MoveInst, Seq->Position, DeltaTime);
	}
	else
	{
		Velocity = FVector(0.f);
	}

	// If we have just stopped moving - update all components so their PreviousLocalToWorld is the same as their LocalToWorld,
	// and so motion blur realises they have stopped moving.
	if(bIsMoving && !bMovingNow)
	{
		ForceUpdateComponents(FALSE);

		// We need to do the same thing for any actors attached to this one, to stop them from motion-blurring at the end of the movement as well
		for(INT AttachIdx=0; AttachIdx<Attached.Num(); AttachIdx++)
		{
			AActor* AttachedActor = Attached(AttachIdx);
			if(AttachedActor && (AttachedActor->Physics == PHYS_Interpolating || AttachedActor->Physics == PHYS_None))
			{
				AttachedActor->ForceUpdateComponents(FALSE);
			}
		}
	}
	bIsMoving = bMovingNow;
}

