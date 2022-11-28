/*=============================================================================
	UnPhysic.cpp: Actor physics implementation

	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

#include "EnginePrivate.h"

#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "EnginePhysicsClasses.h"

void AActor::execSetPhysics( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE(NewPhysics);
	P_FINISH;

	setPhysics(NewPhysics);
}

void AActor::execAutonomousPhysics( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(DeltaSeconds);
	P_FINISH;

	// round acceleration to be consistent with replicated acceleration
	Acceleration.X = 0.1f * int(10 * Acceleration.X);
	Acceleration.Y = 0.1f * int(10 * Acceleration.Y);
	Acceleration.Z = 0.1f * int(10 * Acceleration.Z);

	// Perform physics.
	if( Physics!=PHYS_None )
		performPhysics( DeltaSeconds );
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

UBOOL AActor::moveSmooth(FVector Delta)
{
	FCheckResult Hit(1.f);
	UBOOL didHit = GetLevel()->MoveActor( this, Delta, Rotation, Hit );
	if (Hit.Time < 1.f)
	{
		FVector GravDir = FVector(0,0,-1);
		if (PhysicsVolume->Gravity.Z > 0)
			GravDir.Z = 1;
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
				FVector DesiredDir = Delta.SafeNormal();
				GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
				if (Hit.Time < 1.f)
				{
					SmoothHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Adjusted, Hit.Normal, OldHitNormal, Hit.Time);
					GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
				}
			}
		}
	}
	return didHit;
}

void AActor::SmoothHitWall(FVector HitNormal, AActor *HitActor)
{
	eventHitWall(HitNormal, HitActor);
}

void APawn::SmoothHitWall(FVector HitNormal, AActor *HitActor)
{
	if ( Controller )
	{
		FVector Dir = (Controller->Destination - Location).SafeNormal();
		if ( Physics == PHYS_Walking )
		{
			HitNormal.Z = 0;
			Dir.Z = 0;
		}
		if ( Controller->eventNotifyHitWall(HitNormal, HitActor) )
			return;
	}
	eventHitWall(HitNormal, HitActor);
}

//======================================================================================

void AActor::FindBase()
{
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck( Hit, this, Location + FVector(0,0,-8), Location, TRACE_AllBlocking, GetCylinderExtent() );
	if (Base != Hit.Actor)
		SetBase(Hit.Actor,Hit.Normal);
}

static UBOOL IsRigidBodyPhysics(BYTE Physics)
{
	if(Physics == PHYS_Articulated || Physics == PHYS_RigidBody)
		return true;
	else
		return false;
}

void AActor::setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV)
{
	if (Physics == NewPhysics)
		return;

	BYTE OldPhysics = Physics;

	Physics = NewPhysics;

	if ( (Physics == PHYS_Walking) || (Physics == PHYS_None) || (Physics == PHYS_Rotating) || (Physics == PHYS_Spider) )
	{	
		if ( NewFloor == NULL )
			FindBase();
		else if ( Base != NewFloor )
			SetBase(NewFloor,NewFloorV);

		if ( Physics == PHYS_Spider )
		{
			APawn *P = Cast<APawn>(this);
			if ( P )
				P->bUseCompressedPosition = false; // pawn must replicate Rotation.Roll if PHYS_Spider
		}
	}
	else if (Base != NULL)
		SetBase(NULL);
	if ( (Physics == PHYS_None) || (Physics == PHYS_Rotating) )
	{
		Velocity = FVector(0,0,0);
		Acceleration = FVector(0,0,0);
	}
	
	if ( PhysicsVolume )
		PhysicsVolume->eventPhysicsChangedFor(this);

	// Handle changing to and from rigid-body physics mode.
	if( Physics == PHYS_RigidBody )
	{
		if(CollisionComponent && CollisionComponent->BodyInstance)
		{
			CollisionComponent->BodyInstance->SetFixed(false);
		}
	}
	else if(OldPhysics == PHYS_RigidBody)
	{
		if(CollisionComponent && CollisionComponent->BodyInstance)
		{
			CollisionComponent->BodyInstance->SetFixed(true);
		}
	}
}

void AActor::performPhysics(FLOAT DeltaSeconds)
{
	FVector OldVelocity = Velocity;

	// change position
	switch (Physics)
	{
		case PHYS_Projectile: physProjectile(DeltaSeconds, 0); break;
		case PHYS_Falling: physFalling(DeltaSeconds, 0); break;
		case PHYS_Rotating: break;
		case PHYS_RigidBody: physRigidBody(DeltaSeconds); break;
		case PHYS_Articulated: physArticulated(DeltaSeconds); break;
		case PHYS_Interpolating: physInterpolating(DeltaSeconds); break;
	}

	// rotate
	if ( !RotationRate.IsZero() )
		physicsRotation(DeltaSeconds);

	// allow touched actors to impact physics
	if ( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		AActor *OldTouch = PendingTouch;
		PendingTouch = PendingTouch->PendingTouch;
		OldTouch->PendingTouch = NULL;
	}
}

void APawn::performPhysics(FLOAT DeltaSeconds)
{
	if ( (Location.Z < ((Region.Zone->bSoftKillZ && (Physics != PHYS_Walking)) ? Region.Zone->KillZ - Region.Zone->SoftKill : Region.Zone->KillZ)) && (Controller || (Region.Zone->KillZDamageType != UDmgType_Suicided::StaticClass())) )
	{
		eventFellOutOfWorld(Region.Zone->KillZDamageType);
		if ( bDeleteMe )
			return;
	}
	if ( bCollideWorld && (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		// not in valid spot
		debugf( TEXT("%s fell out of the world!"), GetName());
		eventFellOutOfWorld(NULL);
		return;
	}

	FVector OldVelocity = Velocity;
	OldZ = Location.Z;	// used for eyeheight smoothing

	if ( Physics != PHYS_Walking )
	{
		// only crouch while walking
		if ( Physics != PHYS_Falling && bIsCrouched )
		{
			UnCrouch();
		}
	}
	else if ( bWantsToCrouch && bCanCrouch ) 
	{
		// players crouch by setting bWantsToCrouch to true
		if ( !bIsCrouched )
		{
			Crouch();
		}
		else if ( bTryToUncrouch )
		{
			UncrouchTime -= DeltaSeconds;
			if ( UncrouchTime <= 0.f )
			{
				bWantsToCrouch = false;
				bTryToUncrouch = false;
			}
		}
	}

	// change position
	startNewPhysics(DeltaSeconds,0);
	bSimulateGravity = ( (Physics == PHYS_Falling) || (Physics == PHYS_Walking) );

	// uncrouch if no longer desiring crouch
	// or if not in walking physics
	if ( bIsCrouched && ((Physics != PHYS_Walking && Physics != PHYS_Falling) || !bWantsToCrouch ) )
	{
		UnCrouch();
	}

	if( Controller )
	{
		Controller->MoveTimer -= DeltaSeconds;
		if ( Physics != PHYS_RigidBody && Physics != PHYS_Articulated )
		{
			// rotate
			if ( bCrawler || (Rotation != DesiredRotation) || (RotationRate.Roll > 0) || IsHumanControlled() )
				physicsRotation(DeltaSeconds, OldVelocity);
		}
	}

	AvgPhysicsTime = 0.8f * AvgPhysicsTime + 0.2f * DeltaSeconds;

	if ( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		if ( PendingTouch )
		{
			AActor *OldTouch = PendingTouch;
			PendingTouch = PendingTouch->PendingTouch;
			OldTouch->PendingTouch = NULL;
		}
	}
}

void APawn::startNewPhysics(FLOAT deltaTime, INT Iterations)
{
	if ( (deltaTime < 0.0003f) || (Iterations > 7) )
		return;
	switch (Physics)
	{
		case PHYS_Walking: physWalking(deltaTime, Iterations); break;
		case PHYS_Falling: physFalling(deltaTime, Iterations); break;
		case PHYS_Flying: physFlying(deltaTime, Iterations); break;
		case PHYS_Swimming: physSwimming(deltaTime, Iterations); break;
		case PHYS_Spider: physSpider(deltaTime, Iterations); break;
		case PHYS_Ladder: physLadder(deltaTime, Iterations); break;
		case PHYS_RigidBody: physRigidBody(deltaTime); break;
	}
}

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
FRotator AActor::FindSlopeRotation(FVector FloorNormal, FRotator NewRotation)
{
	if ( (FloorNormal.Z < 0.95) && !FloorNormal.IsNearlyZero() )
	{
		FRotator TempRot = NewRotation;
		// allow yawing, but pitch and roll fixed based on wall
		TempRot.Pitch = 0;
		FVector YawDir = TempRot.Vector();
		FVector PitchDir = YawDir - FloorNormal * (YawDir | FloorNormal);
		TempRot = PitchDir.Rotation();
		NewRotation.Pitch = TempRot.Pitch;
		FVector RollDir = PitchDir ^ FloorNormal;
		TempRot = RollDir.Rotation();
		NewRotation.Roll = TempRot.Pitch;
	}
	else
		return FRotator(0,NewRotation.Yaw, 0);

	return NewRotation;
}

FRotator APawn::FindSlopeRotation(FVector FloorNormal, FRotator NewRotation)
{
	if (Physics == PHYS_Spider && Controller )
	{
		FMatrix M = FMatrix::Identity;
		M.SetAxis( 0, Controller->ViewY ^ Floor );
		M.SetAxis( 1, Controller->ViewY );
		M.SetAxis( 2, Floor );
		return M.Rotator();
	}
	else
		return Super::FindSlopeRotation(FloorNormal,NewRotation);
}

void APawn::physicsRotation(FLOAT deltaTime, FVector OldVelocity)
{
	// Accumulate a desired new rotation.
	FRotator NewRotation = Rotation;	

	INT bPhysOnFloor = (Physics == PHYS_Walking);
	INT deltaYaw = 0;
	if ( !IsHumanControlled() )
	{
		if (Controller->bForceDesiredRotation)
		{
			DesiredRotation = Controller->DesiredRotation;
		}
		if (Controller &&
			Controller->Enemy &&
			Controller->Focus == Controller->Enemy &&
			!Controller->bForceDesiredRotation)
		{
			if ( Controller->bEnemyAcquired && (Controller->Enemy->Visibility > 1) )
				deltaYaw = appRound(::Max(RotationRate.Yaw, Controller->RotationRate.Yaw) * deltaTime);
			else
				deltaYaw = appRound(Controller->AcquisitionYawRate * deltaTime);
			INT YawDiff = Abs((Rotation.Yaw & 65535) - DesiredRotation.Yaw);
			if ( (YawDiff < 2048) || (YawDiff > 63287) )
				Controller->bEnemyAcquired = true;
		}
		else
		{
			INT YawDiff = Abs((Rotation.Yaw & 65535) - DesiredRotation.Yaw);
			if ( YawDiff > 32768 )
				YawDiff -= 32768;
			deltaYaw = Clamp(2*YawDiff, RotationRate.Yaw, 2*RotationRate.Yaw);
			deltaYaw = appRound(deltaYaw * deltaTime);
		}
	}

	if ( (Physics == PHYS_Ladder) && OnLadder )
	{
		// must face ladder
		NewRotation = OnLadder->WallDir;
	}
	else
	{
		//YAW
		if ( DesiredRotation.Yaw != NewRotation.Yaw )
			NewRotation.Yaw = fixedTurn(NewRotation.Yaw, DesiredRotation.Yaw, deltaYaw);

		// PITCH
		if ( !bRollToDesired && (bPhysOnFloor || (Physics == PHYS_Falling)) )
			DesiredRotation.Pitch = 0;
		if ( (!bCrawler || !bPhysOnFloor) && (DesiredRotation.Pitch != NewRotation.Pitch) )
			NewRotation.Pitch = fixedTurn(NewRotation.Pitch, DesiredRotation.Pitch, deltaYaw);
	}

	//ROLL
	if ( bRollToDesired )
	{
		if ( DesiredRotation.Roll != NewRotation.Roll )
			NewRotation.Roll = fixedTurn(NewRotation.Roll, DesiredRotation.Roll, deltaYaw);
	}
	else if ( bCrawler  )
	{
		if ( !bPhysOnFloor )
		{
			// Straighten out
			INT deltaYaw = (INT) (RotationRate.Yaw * deltaTime);
			NewRotation.Pitch = fixedTurn(NewRotation.Pitch, 0, deltaYaw);
			NewRotation.Roll = fixedTurn(NewRotation.Roll, 0, deltaYaw);
		}
		else
			NewRotation = FindSlopeRotation(Floor,NewRotation);
	}
	else if ( RotationRate.Roll > 0 )
	{
		NewRotation.Roll = NewRotation.Roll & 65535;
		if ( NewRotation.Roll < 32768 )
		{
			if ( NewRotation.Roll > RotationRate.Roll )
				NewRotation.Roll = RotationRate.Roll;
		}
		else if ( NewRotation.Roll < 65536 - RotationRate.Roll )
			NewRotation.Roll = 65536 - RotationRate.Roll;
		//pawns roll based on physics
		if ( (Physics == PHYS_Walking) && (Velocity.SizeSquared() < 40000.f) )
		{
			FLOAT SmoothRoll = Min(1.f, 8.f * deltaTime);
			if (NewRotation.Roll < 32768)
				NewRotation.Roll = (INT) (NewRotation.Roll * (1 - SmoothRoll));
			else
				NewRotation.Roll = (INT) (NewRotation.Roll + (65536 - NewRotation.Roll) * SmoothRoll);
		}
		else
		{
			FVector RealAcceleration = (Velocity - OldVelocity)/deltaTime;
			if (RealAcceleration.SizeSquared() > 10000.f)
			{
				FLOAT MaxRoll = 28000.f;
				if ( Physics == PHYS_Walking )
					MaxRoll = 4096.f;
				NewRotation.Roll = 0;

				RealAcceleration = FRotationMatrix(NewRotation).Transpose().TransformNormal(RealAcceleration); //y component will affect roll

				if (RealAcceleration.Y > 0)
					NewRotation.Roll = Min(RotationRate.Roll, (int)(RealAcceleration.Y * MaxRoll/AccelRate));
				else
					NewRotation.Roll = ::Max(65536 - RotationRate.Roll, (int)(65536.f + RealAcceleration.Y * MaxRoll/AccelRate));

				//smoothly change rotation
				Rotation.Roll = Rotation.Roll & 65535;
				if (NewRotation.Roll > 32768)
				{
					if (Rotation.Roll < 32768)
						Rotation.Roll += 65536;
				}
				else if (Rotation.Roll > 32768)
					Rotation.Roll -= 65536;
	
				FLOAT SmoothRoll = Min(1.f, 5.f * deltaTime);
				NewRotation.Roll = (INT) (NewRotation.Roll * SmoothRoll + Rotation.Roll * (1 - SmoothRoll));
			}
			else
			{
				FLOAT SmoothRoll = Min(1.f, 8.f * deltaTime);
				if (NewRotation.Roll < 32768)
					NewRotation.Roll = (INT) (NewRotation.Roll * (1 - SmoothRoll));
				else
					NewRotation.Roll = (INT) (NewRotation.Roll + (65536 - NewRotation.Roll) * SmoothRoll);
			}
		}
	}
	else
		NewRotation.Roll = 0;

	// Set the new rotation.
	if( NewRotation != Rotation )
	{
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
	}
}

void AActor::physicsRotation(FLOAT deltaTime)
{
	// Accumulate a desired new rotation.
	FRotator NewRotation = Rotation;	
	FRotator deltaRotation = RotationRate * deltaTime;

	if ( Physics == PHYS_Rotating )
	{
		NewRotation = NewRotation + deltaRotation;
	}
	else
	{
		//YAW
		if ( (deltaRotation.Yaw != 0) && (DesiredRotation.Yaw != NewRotation.Yaw) )
			NewRotation.Yaw = fixedTurn(NewRotation.Yaw, DesiredRotation.Yaw, deltaRotation.Yaw);
		//PITCH
		if ( (deltaRotation.Pitch != 0) && (DesiredRotation.Pitch != NewRotation.Pitch) )
			NewRotation.Pitch = fixedTurn(NewRotation.Pitch, DesiredRotation.Pitch, deltaRotation.Pitch);
		//ROLL
		if ( (deltaRotation.Roll != 0) && (DesiredRotation.Roll != NewRotation.Roll) )
			NewRotation.Roll = fixedTurn(NewRotation.Roll, DesiredRotation.Roll, deltaRotation.Roll);	
	}

	// Set the new rotation.
	if( NewRotation != Rotation )
	{
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
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
	if (Base == NULL)
	{
		//@fixme - steve this check seems useless, do you remember the purpose of it?
		if (GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0.f,0.f,4.f), Location, TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent()))
		{
			bMustJump = true;
			return Delta;
		}
	}

	// check if clear in front
	FVector ForwardCheck = AccelDir * CylinderComponent->CollisionRadius;
	FVector Destn = Location + Delta + ForwardCheck;
	if (!GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_AllBlocking|TRACE_StopAtFirstHit))
	{
		// going to hit something, so don't bother
		return Delta;
	}

	// clear in front - see if there is footing at walk destination
	FLOAT DesiredDist = Delta.Size();
	// check down enough to catch either step or slope
	FLOAT TestDown = 4.f + CylinderComponent->CollisionHeight + ::Max( MaxStepHeight, CylinderComponent->CollisionRadius + DesiredDist);
	// try a point trace
	GetLevel()->SingleLineCheck(Hit, this, Destn + TestDown * GravDir, Destn , TRACE_AllBlocking);
	// if point trace hit nothing, or hit a steep slope, or below a normal step down, do a trace with extent
	if ( !bAvoidLedges )
	{
		Destn = Location + Delta;
	}
	if (Hit.Time == 1.f ||
		Hit.Normal.Z < UCONST_MINFLOORZ ||
		((Hit.Time * TestDown > CylinderComponent->CollisionHeight + 4.f + Min<FLOAT>(MaxStepHeight,appSqrt(1.f - Hit.Normal.Z * Hit.Normal.Z) * (CylinderComponent->CollisionRadius + DesiredDist)/Hit.Normal.Z))))
	{
		if (!GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent()))
		{
			return Delta;
		}
		GetLevel()->SingleLineCheck(Hit, this, Destn + GravDir * (MaxStepHeight + 4.f), Destn , TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
	}
	if ( (Hit.Time == 1.f) || (Hit.Normal.Z < UCONST_MINFLOORZ) )
	{
		// We have a ledge!
		if ( Controller && Controller->StopAtLedge() )
		{
			return FVector(0.f,0.f,0.f);
		}

		// check which direction ledge goes
		FVector DesiredDir = Destn - Location;
		DesiredDir = DesiredDir.SafeNormal();
		FVector SideDir(DesiredDir.Y, -1.f * DesiredDir.X, 0.f);
		
		// try left
		FVector LeftSide = Destn + DesiredDist * SideDir;
		GetLevel()->SingleLineCheck(Hit, this, LeftSide, Destn, TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			GetLevel()->SingleLineCheck(Hit, this, LeftSide + GravDir * (MaxStepHeight + 4.f), LeftSide , TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
			if ( (Hit.Time < 1.f) && (Hit.Normal.Z >= UCONST_MINFLOORZ) )
			{
				// go left
				FVector NewDir = (LeftSide - Location).SafeNormal();
				return NewDir * DesiredDist;
			}
		}

		// try right
		FVector RightSide = Destn - DesiredDist * SideDir;
		GetLevel()->SingleLineCheck(Hit, this, RightSide, Destn, TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
		if ( Hit.Time == 1.f )
		{
			GetLevel()->SingleLineCheck(Hit, this, RightSide + GravDir * (MaxStepHeight + 4.f), RightSide , TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
			if ( (Hit.Time < 1.f) && (Hit.Normal.Z >= UCONST_MINFLOORZ) )
			{
				// go right
				FVector NewDir = (RightSide - Location).SafeNormal();
				return NewDir * DesiredDist;
			}
		}

		// no available direction, so try to jump
		if ( !bCheckedFall && Controller && Controller->IsProbing(NAME_MayFall) )
		{
			bCheckedFall = 1;
			Controller->eventMayFall();
			bMustJump = bCanJump;
		}
	}
	return Delta;
}

void APawn::physWalking(FLOAT deltaTime, INT Iterations)
{
	if ( !Controller )
	{
		Acceleration = FVector(0.f,0.f,0.f);
		Velocity = FVector(0.f,0.f,0.f);
		return;
	}
	
	//bound acceleration
	Velocity.Z = 0.f;
	Acceleration.Z = 0.f;
	FVector AccelDir = Acceleration.IsZero() ? Acceleration : Acceleration.SafeNormal();
	calcVelocity(AccelDir, deltaTime, GroundSpeed, PhysicsVolume->GroundFriction, 0, 1, 0);
	
	FVector DesiredMove = Velocity;
	// Add effect of velocity zone
	// Rather than constant velocity, hacked to make sure that velocity being clamped when walking doesn't
	// cause the zone velocity to have too much of an effect at fast frame rates
	if ( (PhysicsVolume->ZoneVelocity.SizeSquared() > 0.f)
		&& (IsHumanControlled() || (PhysicsVolume->ZoneVelocity.SizeSquared() > 10000.f)) )
		DesiredMove = DesiredMove + PhysicsVolume->ZoneVelocity * 25 * deltaTime;

	DesiredMove.Z = 0.f;

	//Perform the move
	FVector GravDir = (PhysicsVolume->Gravity.Z > 0.f) ? FVector(0.f,0.f,1.f) : FVector(0.f,0.f,-1.f);
	FVector Down = GravDir * (MaxStepHeight + 2.f);
	FCheckResult Hit(1.f);
	FVector OldLocation = Location;
	FVector OldFloor = Floor;
	AActor *OldBase = Base;
	bJustTeleported = 0;
	INT bCheckedFall = 0;
	INT bMustJump = 0;
	FLOAT remainingTime = deltaTime;

	while ( (remainingTime > 0.f) && (Iterations < 8) && Controller )
	{
		Iterations++;
		// subdivide moves to be no longer than 0.05 seconds
		FLOAT timeTick = (remainingTime > 0.05f) ? Min(0.05f, remainingTime * 0.5f) : remainingTime;
		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector subLoc = Location;
		FLOAT NewHitTime = 0.f;
		FVector NewFloor(0.f,0.f,0.f);
		AActor *NewBase = NULL;

		if ( Delta.IsNearlyZero() )
			remainingTime = 0.f;
		else
		{
			// if AI controlled or walking player, avoid falls
			if ( Controller && Controller->WantsLedgeCheck() )
			{
				FVector subMove = Delta;
				Delta = CheckForLedges(AccelDir, Delta, GravDir, bCheckedFall, bMustJump);
				if ( Controller->MoveTimer == -1.f )
					remainingTime = 0.f;
			}

			// try to move forward
			if ( (Floor.Z < 0.98f) && ((Floor | Delta) < 0.f) )
			{
				Hit.Time = 0.f;
			}
			else
			{
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			}

			if (Hit.Time < 1.f)
			{
				// hit a barrier, try to step up
				FLOAT DesiredDist = Delta.Size();
				FVector DesiredDir = Delta/DesiredDist;
				stepUp(GravDir, DesiredDir, Delta * (1.f - Hit.Time), Hit);
				if ( Physics == PHYS_Falling ) // pawn decided to jump up
				{
					FLOAT ActualDist = (Location - subLoc).Size2D();
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
			if ( Physics == PHYS_Swimming ) //just entered water
			{
				startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		//drop to floor
		FLOAT FloorDist;

		if ( NewBase )
		{
			Hit.Actor = NewBase;
			Hit.Normal = NewFloor;
			Hit.Time = 0.1f;
			FloorDist = MAXFLOORDIST;
		}
		else
		{
			GetLevel()->SingleLineCheck( Hit, this, Location + Down, Location, TRACE_AllBlocking, GetCylinderExtent() );
			FloorDist = Hit.Time * (MaxStepHeight + 2.f);
		}

		Floor = Hit.Normal;

		if ( (Hit.Normal.Z < UCONST_MINFLOORZ) && !Delta.IsNearlyZero() && ((Delta | Hit.Normal) < 0) )
		{
			// slide down slope
			FVector Slide = FVector(0.f,0.f,MaxStepHeight) - Hit.Normal * (FVector(0.f,0.f,MaxStepHeight) | Hit.Normal);
			GetLevel()->MoveActor(this, -1 * Slide, Rotation, Hit);
			if ( (Hit.Actor != Base) && (Physics == PHYS_Walking) )
				SetBase(Hit.Actor, Hit.Normal);
		}
		else if( Hit.Time< 1.f && (Hit.Actor!=Base || FloorDist>MAXFLOORDIST) )
		{
			// move down to correct position
			GetLevel()->MoveActor(this, Down, Rotation, Hit);
			if ( (Hit.Actor != Base) && (Physics == PHYS_Walking) )
				SetBase(Hit.Actor, Hit.Normal);
		}
		else if ( FloorDist < MINFLOORDIST )
		{
			// move up to correct position (average of MAXFLOORDIST and MINFLOORDIST above floor)
			FVector realNorm = Hit.Normal;
			GetLevel()->MoveActor(this, FVector(0.f,0.f,0.5f*(MINFLOORDIST+MAXFLOORDIST) - FloorDist), Rotation, Hit);
			Hit.Time = 0.f;
			Hit.Normal = realNorm;
		}

		// check if just entered water
		if ( Physics == PHYS_Swimming )
		{
			startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
			return;
		}
		
		if( !bMustJump && Hit.Time<1.f && Hit.Normal.Z>=UCONST_MINFLOORZ )
		{
			if( (Hit.Normal.Z < 0.99f) && ((Hit.Normal.Z * PhysicsVolume->GroundFriction) < 3.3f) ) 
			{
				// slide down slope, depending on friction and gravity
				//FIXMESTEVE - test
				FVector Slide = (deltaTime * PhysicsVolume->Gravity/(2.f * ::Max(0.5f, PhysicsVolume->GroundFriction))) * deltaTime;
				Delta = Slide - Hit.Normal * (Slide | Hit.Normal);
				if( (Delta | Slide) >= 0.f )
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				if ( Physics == PHYS_Swimming ) //just entered water
				{
					startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
					return;
				}
			}				
		}
		else
		{
			if ( !bMustJump && bCanJump && !bCheckedFall && Controller && Controller->IsProbing(NAME_MayFall) )
			{
				// give this pawn a chance to abort its fall
				bCheckedFall = 1;
				Controller->eventMayFall();
			}
			if ( !bJustTeleported && !bMustJump && (!bCanJump || (!bCanWalkOffLedges && (bIsWalking || bIsCrouched))) )
			{
				// this pawn shouldn't fall, so undo its move
				Velocity = FVector(0.f,0.f,0.f);
				Acceleration = FVector(0.f,0.f,0.f);
				GetLevel()->FarMoveActor(this,OldLocation,false,false);
				if ( OldBase && (OldBase->bStatic || OldBase->bWorldGeometry || !OldBase->bMovable) )
				{
					SetBase(OldBase,OldFloor);
				}
				if ( Controller )
					Controller->MoveTimer = -1.f;
				return;
			}
			else
			{
				// falling
				FLOAT DesiredDist = Delta.Size();
				FLOAT ActualDist = (Location - subLoc).Size2D();
				if (DesiredDist == 0.f)
					remainingTime = 0.f;
				else
					remainingTime += timeTick * (1.f - Min(1.f,ActualDist/DesiredDist));
				Velocity.Z = 0.f;
				eventFalling();
				if (Physics == PHYS_Walking)
					setPhysics(PHYS_Falling); //default if script didn't change physics
				startNewPhysics(remainingTime,Iterations);
				return;
			}
		}
	}

	// make velocity reflect actual move
	if ( !bJustTeleported && !bNoVelocityUpdate )
		Velocity = (Location - OldLocation) / deltaTime;
	bNoVelocityUpdate = 0;
	Velocity.Z = 0.f;
}

/* calcVelocity()
Calculates new velocity and acceleration for pawn for this tick
bounds acceleration and velocity, adds effects of friction and momentum
// bBrake only for walking?
*/
void APawn::calcVelocity(FVector AccelDir, FLOAT deltaTime, FLOAT maxSpeed, FLOAT friction, INT bFluid, INT bBrake, INT bBuoyant)
{
	if (Controller != NULL &&
		Controller->bPreciseDestination)
	{
		// check to see if we've reached the destination
		// LAURENT -- this test fails easily... need something better here!
		if( ReachedDestination(Controller->Destination - Location, NULL) )
		{
			Controller->bPreciseDestination = 0;
		}
		else
		{
			// otherwise calculate velocity towards the destination
			Velocity = (Controller->Destination - Location) * 1.f/deltaTime;
		}
	}
	else
	{
		FVector OldVelocity = Velocity;

		if ( bBrake && Acceleration.IsZero() )
		{
			FVector OldVel = Velocity;
			FVector SumVel = FVector(0,0,0);

			FLOAT RemainingTime = deltaTime;
			// subdivide braking to get reasonably consistent results at lower frame rates
			// (important for packet loss situations w/ networking)
			while( RemainingTime > 0.03f )
			{
				Velocity = Velocity - (2 * Velocity) * 0.03f * friction; //don't drift to a stop, brake
				if ( (Velocity | OldVel) > 0.f )
					SumVel += 0.03f * Velocity/deltaTime;
				RemainingTime -= 0.03f;
			}
			Velocity = Velocity - (2 * Velocity) * RemainingTime * friction; //don't drift to a stop, brake
			if ( (Velocity | OldVel) > 0.f )
				SumVel += RemainingTime * Velocity/deltaTime;
			Velocity = SumVel;
			if ( ((OldVel | Velocity) < 0.f)
				|| (Velocity.SizeSquared() < 100) )//brake to a stop, not backwards
				Velocity = FVector(0,0,0);
		}
		else
		{
			FLOAT VelSize = Velocity.Size();
			if (Acceleration.SizeSquared() > AccelRate * AccelRate)
				Acceleration = AccelDir * AccelRate;
			Velocity = Velocity - (Velocity - AccelDir * VelSize) * deltaTime * friction;
		}

		Velocity = Velocity * (1 - bFluid * friction * deltaTime) + Acceleration * deltaTime;


		if ( bBuoyant )
		{
			Velocity = Velocity + PhysicsVolume->Gravity * deltaTime * (1.f - Buoyancy);
		}

		if ( IsProbing(NAME_ModifyVelocity) )
		{
			eventModifyVelocity(deltaTime,OldVelocity);
		}
	}

	maxSpeed *= MaxSpeedModifier();
	if (Velocity.SizeSquared() > maxSpeed * maxSpeed)
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= maxSpeed;
	}
}

FLOAT APawn::MaxSpeedModifier()
{
	FLOAT Result = 1.f;

	if ( !IsHumanControlled() )
		Result *= DesiredSpeed;

	if ( bIsCrouched )
		Result *= CrouchedPct;
	else if ( bIsWalking )
		Result *= WalkingPct;

	return Result;
}

void APawn::stepUp(const FVector& GravDir, const FVector& DesiredDir, const FVector& Delta, FCheckResult &Hit)
{
	FVector Down = GravDir * (MaxStepHeight + 2.f);
	UBOOL bStepDown = true;

	if ( (Abs(Hit.Normal.Z) < MAXSTEPSIDEZ) || (Hit.Normal.Z >= UCONST_MINFLOORZ) )
	{
		// step up - treat as vertical wall
		GetLevel()->MoveActor(this, -1 * Down, Rotation, Hit);
		GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	}
	else if ( Physics != PHYS_Walking )
	{
		 // slide up slope
		FLOAT Dist = Delta.Size();
		GetLevel()->MoveActor(this, Delta + FVector(0,0,Dist*Hit.Normal.Z), Rotation, Hit);
		bStepDown = false;
	}

	if (Hit.Time < 1.f)
	{
		if ( (Abs(Hit.Normal.Z) < MAXSTEPSIDEZ) && (Hit.Time * Delta.SizeSquared() > 144.f) )
		{
			// try another step
			if ( bStepDown )
			GetLevel()->MoveActor(this, Down, Rotation, Hit);
			stepUp(GravDir, DesiredDir, Delta * (1 - Hit.Time), Hit);
			return;
		}

		// notify script that pawn ran into a wall
		processHitWall(Hit.Normal, Hit.Actor);
		if ( Physics == PHYS_Falling )
			return;

		//adjust and try again
		Hit.Normal.Z = 0;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector NewDelta = Delta;
		FVector OldHitNormal = Hit.Normal;
		NewDelta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
		if( (NewDelta | Delta) >= 0 )
		{
			GetLevel()->MoveActor(this, NewDelta, Rotation, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit.Normal, Hit.Actor);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, NewDelta, Hit.Normal, OldHitNormal, Hit.Time);
				GetLevel()->MoveActor(this, NewDelta, Rotation, Hit);
			}
		}
	}
	if ( bStepDown )
	GetLevel()->MoveActor(this, Down, Rotation, Hit);

}

/* AActor::stepUp() used by MoveSmooth() to move smoothly up steps

*/
void AActor::stepUp(const FVector& GravDir, const FVector& DesiredDir, const FVector& Delta, FCheckResult &Hit)
{
	const FLOAT ActorMaxStepHeight = 35.f;
	FVector Down = GravDir * ActorMaxStepHeight;

	if ( Abs(Hit.Normal.Z) < MAXSTEPSIDEZ )
	{
		// step up - treat as vertical wall
		GetLevel()->MoveActor(this, -1 * Down, Rotation, Hit);
		GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	}
	else
	{
		 // slide up slope
		FLOAT Dist = Delta.Size();
		GetLevel()->MoveActor(this, Delta + FVector(0,0,Dist*Hit.Normal.Z), Rotation, Hit);
	}

	if (Hit.Time < 1.f)
	{
		if ( (Abs(Hit.Normal.Z) < MAXSTEPSIDEZ) && (Hit.Time * Delta.SizeSquared() > 144.f) )
		{
			// try another step
			GetLevel()->MoveActor(this, Down, Rotation, Hit);
			stepUp(GravDir, DesiredDir, Delta * (1 - Hit.Time), Hit);
			return;
		}

		// notify script that actor ran into a wall
		processHitWall(Hit.Normal, Hit.Actor);
		if ( Physics == PHYS_Falling )
			return;

		//adjust and try again
		Hit.Normal.Z = 0;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector NewDelta = Delta;
		FVector OldHitNormal = Hit.Normal;
		NewDelta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
		if( (NewDelta | Delta) >= 0 )
		{
			GetLevel()->MoveActor(this, NewDelta, Rotation, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit.Normal, Hit.Actor);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, NewDelta, Hit.Normal, OldHitNormal, Hit.Time);
				GetLevel()->MoveActor(this, NewDelta, Rotation, Hit);
			}
		}
	}
	GetLevel()->MoveActor(this, Down, Rotation, Hit);
}

void AActor::processHitWall(FVector HitNormal, AActor *HitActor)
{
	if ( !bBlockActors && HitActor->IsA(APawn::StaticClass()) && !HitActor->IsEncroacher() )
		return;
	eventHitWall(HitNormal, HitActor);
}

/*
CanCrouchWalk()
Used by AI to determine if could continue moving forward by crouching
*/
UBOOL APawn::CanCrouchWalk( const FVector& StartLocation, const FVector& EndLocation, AActor* HitActor )
{
	const FVector CrouchedOffset = FVector(0.0f,0.0f,CrouchHeight-CylinderComponent->CollisionHeight);

    if( !bCanCrouch )
        return false;

	DWORD TraceFlags = TRACE_World;
	if ( HitActor && !HitActor->bWorldGeometry )
		TraceFlags = TRACE_AllBlocking;

	// quick zero extent trace from start location
	FCheckResult Hit(1.0f);
	GetLevel()->SingleLineCheck(
		Hit,
		this,
		EndLocation + CrouchedOffset,
		StartLocation + CrouchedOffset,
		TraceFlags | TRACE_StopAtFirstHit );

	if( !Hit.Actor )
	{
		// try slower extent trace
		GetLevel()->SingleLineCheck(
			Hit,
			this,
			EndLocation + CrouchedOffset,
			StartLocation + CrouchedOffset,
			TraceFlags,
			FVector(CrouchRadius,CrouchRadius,CrouchHeight) );

			if ( Hit.Time == 1.0f )
			{
				bWantsToCrouch = true;
				bTryToUncrouch = true;
				UncrouchTime = 0.5f;
				return true;
			}
	}
	return false;
}

void APawn::processHitWall(FVector HitNormal, AActor *HitActor)
{
	if ( !HitActor )
		return;
		
	if ( HitActor->IsA(APawn::StaticClass()) )
	{
		if ( !Controller || (Physics == PHYS_Falling) || !HitActor->IsA(AVehicle::StaticClass()) )
			return;
		if ( Controller->eventNotifyHitWall(HitNormal, HitActor) )
		return;
		FVector Cross = (Controller->Destination - Location) ^ FVector(0.f,0.f,1.f);
		FVector Dir = 1.2f * Cast<APawn>(HitActor)->CylinderComponent->CollisionRadius * Cross.SafeNormal();
		if ( (Cross | (Controller->Destination - Location)) < 0.f )
			Dir *= -1.f;
		if ( appFrand() < 0.3f )
			Dir *= -2.f;
		Controller->SetAdjustLocation(Location+Dir);
		return;		
	}
	if ( !bDirectHitWall && Controller )
	{
		if ( Acceleration.IsZero() )
			return;
		FVector Dir = (Controller->Destination - Location).SafeNormal();
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
			UBOOL bTryCrouch = (Physics == PHYS_Walking) && !IsHumanControlled() && bCanCrouch && !bIsCrouched;
			// try moving crouched stepped up
			if ( bTryCrouch && CanCrouchWalk( Location, Location + CylinderComponent->CollisionRadius*Dir, HitActor) )
				return;
			FCheckResult Hit(1.f);
			GetLevel()->MoveActor(this,FVector(0,0,-1.f * MaxStepHeight), Rotation, Hit);
			if ( bTryCrouch && CanCrouchWalk( Location, Location + CylinderComponent->CollisionRadius*Dir, HitActor) )
				return;
			if ( Controller && HitActor->bWorldGeometry )
			Controller->AdjustFromWall(HitNormal, HitActor);
		}
		else if ( Controller->bNotifyFallingHitWall )
		{
			FVector OldVel = Velocity;
			Controller->eventNotifyFallingHitWall(HitNormal, HitActor);
			if ( Velocity != OldVel )
				bJustTeleported = true;
		}
	}
	eventHitWall(HitNormal, HitActor);
}

UBOOL AActor::ShrinkCollision(AActor *HitActor, const FVector &StartLocation)
{
	return false;
}

UBOOL AProjectile::ShrinkCollision(AActor *HitActor, const FVector &StartLocation)
{
	// JAG_COLRADIUS_HACK
	UCylinderComponent* CylComp = Cast<UCylinderComponent>(CollisionComponent);

	if ( bSwitchToZeroCollision	
		&& CylComp
		&& ((CylComp->CollisionHeight != 0.f) || (CylComp->CollisionRadius != 0.f)) )
	{
		/* FIXMESTEVE
		if ( HitActor->CollisionComponent && HitActor->CollisionComponent->BlockZeroExtent )
		{
			UStaticMeshComponent* HitComp = Cast<UStaticMeshComponent>(HitActor->CollisionComponent);
			if ( !HitComp || !HitComp->UseSimpleBoxCollision )
				return false;
			AStaticMeshActor *S = Cast<AStaticMeshActor>(HitActor);
			if ( S && !S->bExactProjectileCollision )
				return false;
		}
		*/
		FCheckResult Hit(1.f);
		FVector Dir = (StartLocation - Location).SafeNormal();

		GetLevel()->SingleLineCheck(Hit, this, StartLocation + 100.f*Dir, Location, TRACE_AllBlocking|TRACE_StopAtFirstHit);
		if ( Hit.Time == 1.f )
			GetLevel()->SingleLineCheck(Hit, this, Location, StartLocation + 100.f*Dir, TRACE_AllBlocking|TRACE_StopAtFirstHit);
		else
			GetLevel()->SingleLineCheck(Hit, this, Location, Hit.Location, TRACE_AllBlocking|TRACE_StopAtFirstHit);

		if ( Hit.Time == 1.f )
		{
			bSwitchToZeroCollision = false;
		CylComp->SetSize(0.f,0.f);
			ZeroCollider = HitActor;
			return true;
		}
	}
	return false;
}

void AActor::processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations)
{
	if ( Location.Z < Region.Zone->KillZ )
		eventFellOutOfWorld(Region.Zone->KillZDamageType);
	if ( bDeleteMe )
		return;
	if ( PhysicsVolume->bBounceVelocity && (PhysicsVolume->ZoneVelocity != FVector(0,0,0)) )
	{
		Velocity = PhysicsVolume->ZoneVelocity + FVector(0,0,80);
		return;
	}

	eventLanded(HitNormal);
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
		GetLevel()->MoveActor(this, FVector(0,0,0), NewRotation, Hit);
	}
}

void APawn::processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations)
{
	//Check that it is a valid landing (not a BSP cut)
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck(Hit, this, Location -  FVector(0,0,0.2f * CylinderComponent->CollisionHeight + 8),
		Location, TRACE_Actors|TRACE_World|TRACE_StopAtFirstHit, 0.9f * GetCylinderExtent());

	if ( Hit.Time == 1.f ) //Not a valid landing
	{
		FVector Adjusted = Location;
		if ( GetLevel()->FindSpot(1.1f * GetCylinderExtent(), Adjusted) && (Adjusted != Location) )
		{
			GetLevel()->FarMoveActor(this, Adjusted, 0, 0);
			Velocity.X += appFrand() * 60 - 30;
			Velocity.Y += appFrand() * 60 - 30;
			return;
		}
	}
	Floor = HitNormal;
	if ( !Controller || !Controller->eventNotifyLanded(HitNormal) )
		eventLanded(HitNormal);
	if ( Physics == PHYS_Falling )
	{
		if ( Health > 0 )
			setPhysics(PHYS_Walking, HitActor, HitNormal);
		else
			setPhysics(PHYS_None, HitActor, HitNormal);
	}
	if ( Physics == PHYS_Walking )
		Acceleration = Acceleration.SafeNormal();
	startNewPhysics(remainingTime, Iterations);
	if ( Controller && Controller->bNotifyPostLanded )
		Controller->eventNotifyPostLanded();

	if ( !Controller && (Physics == PHYS_None) )
	{
		// rotate properly onto slope
		FCheckResult Hit(1.f);
		FRotator NewRotation = FindSlopeRotation(HitNormal,Rotation);
		GetLevel()->MoveActor(this, FVector(0,0,0), NewRotation, Hit);
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
	FVector RealAcceleration = Acceleration;
	FCheckResult Hit(1.f);

	// test for slope to avoid using air control to climb walls
	FLOAT TickAirControl = AirControl;
	Acceleration.Z = 0.f;
	if( TickAirControl > 0.05f )
	{
		FVector TestWalk = ( TickAirControl * AccelRate * Acceleration.SafeNormal() + Velocity ) * deltaTime;
		TestWalk.Z = 0;
		GetLevel()->SingleLineCheck( Hit, this, Location + TestWalk, Location, TRACE_World|TRACE_StopAtFirstHit, FVector( CylinderComponent->CollisionRadius, CylinderComponent->CollisionRadius, CylinderComponent->CollisionHeight ) );
		if( Hit.Actor )
			TickAirControl = 0.f;
	}

	// boost maxAccel to increase player's control when falling
	FLOAT maxAccel = AccelRate * TickAirControl;
	FVector Velocity2D = Velocity;
	Velocity2D.Z = 0;
	FLOAT speed2d = Velocity2D.Size2D();
	if ( (speed2d < 10.f) && (TickAirControl > 0.f) ) //allow initial burst
		maxAccel = maxAccel + (10 - speed2d)/deltaTime;
	else if ( speed2d >= GroundSpeed )
	{
		if ( TickAirControl <= 0.05f )
			maxAccel = 1.f;
		else
			BoundSpeed = speed2d;
	}

	if ( Acceleration.SizeSquared() > maxAccel * maxAccel )
	{
		Acceleration = Acceleration.SafeNormal();
		Acceleration *= maxAccel;
	}

	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.1f;
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
		Velocity = NewFallVelocity(OldVelocity,Acceleration + PhysicsVolume->Gravity,timeTick);
		if ( Controller && Controller->bNotifyApex && (Velocity.Z <= 0.f) )
		{
			bJustTeleported = true;
			Controller->bNotifyApex = false;
			Controller->eventNotifyJumpApex();
			Controller->bJumpOverWall = false;
		}

		if ( BoundSpeed != 0 )
		{
			// using air control, so make sure not exceeding acceptable speed
			FVector Vel2D = Velocity;
			Vel2D.Z = 0;
			if ( Vel2D.SizeSquared() > BoundSpeed * BoundSpeed )
			{
				Vel2D = Vel2D.SafeNormal();
				Vel2D = Vel2D * BoundSpeed;
				Vel2D.Z = Velocity.Z;
				Velocity = Vel2D;
			}
		}
		if ( IsProbing(NAME_ModifyVelocity) )
			eventModifyVelocity(timeTick,OldVelocity);
		FVector Adjusted = (Velocity + PhysicsVolume->ZoneVelocity) * timeTick;

		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		if ( bDeleteMe )
			return;
		else if ( Physics == PHYS_Swimming ) //just entered water
		{
			remainingTime = remainingTime + timeTick * (1.f - Hit.Time);
			startSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if ( Hit.Time < 1.f )
		{
			if (Hit.Normal.Z >= UCONST_MINFLOORZ)
			{
				remainingTime += timeTick * (1.f - Hit.Time);
				if (!bJustTeleported && (Hit.Time > 0.1f) && (Hit.Time * timeTick > 0.003f) )
					Velocity = (Location - OldLocation)/(timeTick * Hit.Time);
				processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
				return;
			}
			else
			{
				// FIXMESTEVE - should call Controller AirControl() function
				if ( RealAcceleration.IsZero() && Controller && !Controller->GetAPlayerController() && !Controller->bNotifyFallingHitWall )
				{
					// try aircontrol push
					Acceleration = Velocity;
					Acceleration.Z = 0.f;
					Acceleration = Acceleration.SafeNormal();
					Acceleration *= AccelRate;
					RealAcceleration = Acceleration;
				}
				else
					processHitWall(Hit.Normal, Hit.Actor);
				FVector OldHitNormal = Hit.Normal;
				FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);

				if( (Delta | Adjusted) >= 0.f )
				{
					//if ( Delta.Z > 0 ) // friction slows sliding up slopes
					//	Delta *= 0.5f;	// FIXME should this be gone forever?
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
					if (Hit.Time < 1.f) //hit second wall
					{
						if ( Hit.Normal.Z >= UCONST_MINFLOORZ )
						{
							remainingTime = 0.f;
							processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
							return;
						}
						processHitWall(Hit.Normal, Hit.Actor);
						FVector DesiredDir = Adjusted.SafeNormal();
						TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
						int bDitch = ( (OldHitNormal.Z > 0.f) && (Hit.Normal.Z > 0.f) && (Delta.Z == 0.f) && ((Hit.Normal | OldHitNormal) < 0.f) );
						GetLevel()->MoveActor(this, Delta, Rotation, Hit);
						if ( bDitch || (Hit.Normal.Z >= UCONST_MINFLOORZ) )
						{
							remainingTime = 0.f;
							processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
							return;
						}
					}
				}
				FLOAT OldVelZ = OldVelocity.Z;
				OldVelocity = (Location - OldLocation)/timeTick;
				OldVelocity.Z = OldVelZ;
			}
		}

		if ( !bJustTeleported )
		{
			// refine the velocity by figuring out the average actual velocity over the tick, and then the final velocity.
			// This particularly corrects for situations where level geometry affected the fall.
			Velocity = (Location - OldLocation)/timeTick - PhysicsVolume->ZoneVelocity; //actual average velocity
			if ( (Velocity.Z < OldVelocity.Z) || (OldVelocity.Z >= 0.f) )
				Velocity = 2 * Velocity - OldVelocity; //end velocity has 2* accel of avg
			if (Velocity.SizeSquared() > PhysicsVolume->TerminalVelocity * PhysicsVolume->TerminalVelocity)
			{
				Velocity = Velocity.SafeNormal();
				Velocity *= PhysicsVolume->TerminalVelocity;
			}
		}
	}

	Acceleration = RealAcceleration;
}

void AActor::physFalling(FLOAT deltaTime, INT Iterations)
{
	if ( Location.Z < (Region.Zone->bSoftKillZ ? Region.Zone->KillZ - Region.Zone->SoftKill : Region.Zone->KillZ) )
	{
		eventFellOutOfWorld(Region.Zone->KillZDamageType);
		return;
	}

	if ( (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		// not in valid spot
		if( Role == ROLE_Authority && IsA(ADroppedPickup::StaticClass()) )
			debugf( TEXT("%s fell out of the world at %f %f %f!"), *GetFullName(), Location.X, Location.Y, Location.Z );
		eventFellOutOfWorld(NULL);
		return;
	}

	//bound acceleration, falling object has minimal ability to impact acceleration
	FVector RealAcceleration = Acceleration;
	FCheckResult Hit(1.f);
	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.1f;
	int numBounces = 0;
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
		Velocity = OldVelocity * (1 - NetFluidFriction * timeTick)
				+ Acceleration + PhysicsVolume->Gravity * (1.f - NetBuoyancy) * timeTick;

		FVector Adjusted = (Velocity + PhysicsVolume->ZoneVelocity) * timeTick;

		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		if ( bDeleteMe )
			return;
		if ( (Hit.Time < 1.f) && ShrinkCollision(Hit.Actor, OldLocation) )
		{
			timeTick = timeTick * (1.f - Hit.Time);
			Adjusted = (Velocity + PhysicsVolume->ZoneVelocity) * timeTick;
			GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		}
		if ( Hit.Time < 1.f )
		{
			if (bBounce)
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				if ( bDeleteMe || (Physics == PHYS_None) )
					return;
				else if ( numBounces < 2 )
					remainingTime += timeTick * (1.f - Hit.Time);
				numBounces++;
			}
			else
			{
				if (Hit.Normal.Z >= UCONST_MINFLOORZ)
				{
					remainingTime += timeTick * (1.f - Hit.Time);
					if (!bJustTeleported && (Hit.Time > 0.1f) && (Hit.Time * timeTick > 0.003f) )
						Velocity = (Location - OldLocation)/(timeTick * Hit.Time);
					processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
					return;
				}
				else
				{
					processHitWall(Hit.Normal, Hit.Actor);
					if ( bDeleteMe )
						return;
					FVector OldHitNormal = Hit.Normal;
					FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
					if( (Delta | Adjusted) >= 0 )
					{
						if ( Delta.Z > 0 ) // friction slows sliding up slopes
							Delta *= 0.5;
						GetLevel()->MoveActor(this, Delta, Rotation, Hit);
						if (Hit.Time < 1.f) //hit second wall
						{
							if ( Hit.Normal.Z >= UCONST_MINFLOORZ )
							{
								remainingTime = 0.f;
								processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
								return;
							}
							else
								processHitWall(Hit.Normal, Hit.Actor);
		
							if ( bDeleteMe )
								return;
							FVector DesiredDir = Adjusted.SafeNormal();
							TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
							int bDitch = ( (OldHitNormal.Z > 0) && (Hit.Normal.Z > 0) && (Delta.Z == 0) && ((Hit.Normal | OldHitNormal) < 0) );
							GetLevel()->MoveActor(this, Delta, Rotation, Hit);
							if ( bDitch || (Hit.Normal.Z >= UCONST_MINFLOORZ) )
							{
								remainingTime = 0.f;
								processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
								return;
							}
						}
					}
					FLOAT OldVelZ = OldVelocity.Z;
					OldVelocity = (Location - OldLocation)/timeTick;
					OldVelocity.Z = OldVelZ;
				}
			}
		}

		if (!bBounce && !bJustTeleported)
		{
			// refine the velocity by figuring out the average actual velocity over the tick, and then the final velocity.
			// This particularly corrects for situations where level geometry affected the fall.
			Velocity = (Location - OldLocation)/timeTick - PhysicsVolume->ZoneVelocity; //actual average velocity
			if ( (Velocity.Z < OldVelocity.Z) || (OldVelocity.Z >= 0) )
				Velocity = 2 * Velocity - OldVelocity; //end velocity has 2* accel of avg
			if (Velocity.SizeSquared() > PhysicsVolume->TerminalVelocity * PhysicsVolume->TerminalVelocity)
			{
				Velocity = Velocity.SafeNormal();
				Velocity *= PhysicsVolume->TerminalVelocity;
			}
		}
	}

	Acceleration = RealAcceleration;
}

void APawn::startSwimming(FVector OldLocation, FVector OldVelocity, FLOAT timeTick, FLOAT remainingTime, INT Iterations)
{
	if (!bBounce && !bJustTeleported)
	{
		if ( timeTick > 0.f )
			Velocity = (Location - OldLocation)/timeTick; //actual average velocity
		Velocity = 2 * Velocity - OldVelocity; //end velocity has 2* accel of avg
		if (Velocity.SizeSquared() > PhysicsVolume->TerminalVelocity * PhysicsVolume->TerminalVelocity)
		{
			Velocity = Velocity.SafeNormal();
			Velocity *= PhysicsVolume->TerminalVelocity;
		}
	}
	FVector End = findWaterLine(Location, OldLocation);
	FLOAT waterTime = 0.f;
	if (End != Location)
	{	
		waterTime = timeTick * (End - Location).Size()/(Location - OldLocation).Size();
		remainingTime += waterTime;
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor(this, End - Location, Rotation, Hit);
	}
	if ((Velocity.Z > -160.f) && (Velocity.Z < 0)) //allow for falling out of water
		Velocity.Z = -80.f - Velocity.Size2D() * 0.7; //smooth bobbing
	if ( (remainingTime > 0.01f) && (Iterations < 8) )
		physSwimming(remainingTime, Iterations);

}

void APawn::physFlying(FLOAT deltaTime, INT Iterations)
{
	FVector AccelDir;

	if ( bCollideWorld && (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		// not in valid spot
		if ( !Controller || !Controller->bIsPlayer )
		{
			debugf( TEXT("%s flew out of the world!"), GetName());
			GetLevel()->DestroyActor( this );
		}
		return;
	}

	if ( Acceleration.IsZero() )
		AccelDir = Acceleration;
	else
		AccelDir = Acceleration.SafeNormal();

	calcVelocity(AccelDir, deltaTime, AirSpeed, 0.5f * PhysicsVolume->FluidFriction, 1, 0, 0);

	Iterations++;
	FVector OldLocation = Location;
	bJustTeleported = 0;
	FVector ZoneVel;
	if ( IsHumanControlled() || (PhysicsVolume->ZoneVelocity.SizeSquared() > 90000) )
		ZoneVel = PhysicsVolume->ZoneVelocity;
	else
		ZoneVel = FVector(0,0,0);
	FVector Adjusted = (Velocity + ZoneVel) * deltaTime;
	FCheckResult Hit(1.f);
	GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
	if (Hit.Time < 1.f)
	{
		Floor = Hit.Normal;
		FVector GravDir = FVector(0,0,-1);
		if (PhysicsVolume->Gravity.Z > 0)
			GravDir.Z = 1;
		FVector DesiredDir = Adjusted.SafeNormal();
		FVector VelDir = Velocity.SafeNormal();
		FLOAT UpDown = GravDir | VelDir;
		if ( (Abs(Hit.Normal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) )
		{
			FLOAT stepZ = Location.Z;
			stepUp(GravDir, DesiredDir, Adjusted * (1.f - Hit.Time), Hit);
			OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
		}
		else
		{
			processHitWall(Hit.Normal, Hit.Actor);
			//adjust and try again
			FVector OldHitNormal = Hit.Normal;
			FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				if (Hit.Time < 1.f) //hit second wall
				{
					processHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				}
			}
		}
	}
	else
		Floor = FVector(0.f,0.f,1.f);

	if ( !bJustTeleported && !bNoVelocityUpdate )
		Velocity = (Location - OldLocation) / deltaTime;
	bNoVelocityUpdate = 0;
}

/* Swimming uses gravity - but scaled by (1.f - buoyancy)
This is used only by pawns
*/
FLOAT APawn::Swim(FVector Delta, FCheckResult &Hit)
{
	FVector Start = Location;
	FLOAT airTime = 0.f;
	GetLevel()->MoveActor(this, Delta, Rotation, Hit);

	if ( !PhysicsVolume->bWaterVolume ) //then left water
	{
		FVector End = findWaterLine(Start, Location);
		if (End != Location)
		{
			airTime = (End - Location).Size()/Delta.Size();
			if ( ((Location - Start) | (End - Location)) > 0.f )
				airTime = 0.f;
			GetLevel()->MoveActor(this, End - Location, Rotation, Hit);
		}
	}
	return airTime;
}

//get as close to waterline as possible, staying on same side as currently
FVector APawn::findWaterLine(FVector InWater, FVector OutofWater)
{
	FCheckResult Hit(1.f);
	FMemMark Mark(GMem);
	FCheckResult* FirstHit = GetLevel()->MultiLineCheck
	(
		GMem,
		InWater,
		OutofWater,
		FVector(0,0,0),
		Level,
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

/*
GetNetBuoyancy()
determine how deep in water actor is standing:
0 = not in water,
1 = fully in water
*/
void AActor::GetNetBuoyancy(FLOAT &NetBuoyancy, FLOAT &NetFluidFriction)
{
	APhysicsVolume *WaterVolume = NULL;
	FLOAT depth = 0.f;

	if ( PhysicsVolume->bWaterVolume )
	{
		// JAG_COLRADIUS_HACK
		FLOAT CollisionHeight, CollisionRadius;
		GetBoundingCylinder(CollisionRadius, CollisionHeight);

		WaterVolume = PhysicsVolume;
		if ( (CollisionHeight == 0.f) || (Buoyancy == 0.f) )
			depth = 1.f;
		else
		{
			FCheckResult Hit(1.f);
		    if ( PhysicsVolume->Brush )
			    PhysicsVolume->Brush->LineCheck(Hit,PhysicsVolume,
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
INT AVolume::Encompasses(FVector point)
{
	check(BrushComponent);

	if ( !Brush )
		return 0;
	FCheckResult Hit(1.f);
//	debugf(TEXT("%s brush pointcheck %d at %f %f %f"),GetName(),!Brush->PointCheck(Hit,this,	point, FVector(0.f,0.f,0.f), 0), point.X, point.Y,point.Z);
	return !Brush->PointCheck(Hit,this,	point, FVector(0.f,0.f,0.f));
}

void APawn::physSwimming(FLOAT deltaTime, INT Iterations)
{
	FLOAT NetBuoyancy = 0.f;
	FLOAT NetFluidFriction  = 0.f;
	GetNetBuoyancy(NetBuoyancy, NetFluidFriction);
	if ( (Velocity.Z > 100.f) && (Buoyancy != 0.f) )
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
	calcVelocity(AccelDir, deltaTime, WaterSpeed, 0.5f * PhysicsVolume->FluidFriction, 1, 0, 1);
	FLOAT velZ = Velocity.Z;
	FVector ZoneVel;
	if ( IsHumanControlled() || (PhysicsVolume->ZoneVelocity.SizeSquared() > 90000) )
	{
		// Add effect of velocity zone
		// Rather than constant velocity, hacked to make sure that velocity being clamped when swimming doesn't
		// cause the zone velocity to have too much of an effect at fast frame rates

		ZoneVel = PhysicsVolume->ZoneVelocity * 25 * deltaTime;
	}
	else
		ZoneVel = FVector(0,0,0);
	FVector Adjusted = (Velocity + ZoneVel) * deltaTime;
	FCheckResult Hit(1.f);
	FLOAT remainingTime = deltaTime * Swim(Adjusted, Hit);

	if (Hit.Time < 1.f)
	{
		Floor = Hit.Normal;
		FVector GravDir = FVector(0,0,-1);
		if (PhysicsVolume->Gravity.Z > 0)
			GravDir.Z = 1;
		FVector DesiredDir = Adjusted.SafeNormal();
		FVector VelDir = Velocity.SafeNormal();
		FLOAT UpDown = GravDir | VelDir;
		if ( (Abs(Hit.Normal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) )
		{
			FLOAT stepZ = Location.Z;
			stepUp(GravDir, DesiredDir, Adjusted * (1.f - Hit.Time), Hit);
			OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
		}
		else
		{
			processHitWall(Hit.Normal, Hit.Actor);
			//adjust and try again
			FVector OldHitNormal = Hit.Normal;
			FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				remainingTime = remainingTime * (1.f - Hit.Time) * Swim(Delta, Hit);
				if(Hit.Time < 1.f) //hit second wall
				{
					processHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					remainingTime = remainingTime * (1.f - Hit.Time) * Swim(Delta, Hit);
				}
			}
		}
	}
	else
		Floor = FVector(0.f,0.f,1.f);

	if (!bJustTeleported && (remainingTime < deltaTime))
	{
		int bWaterJump = !PhysicsVolume->bWaterVolume;
		if (bWaterJump)
			velZ = Velocity.Z;
		if ( !bNoVelocityUpdate )
			Velocity = (Location - OldLocation) / (deltaTime - remainingTime);
		bNoVelocityUpdate = 0;
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

/* PhysProjectile is tailored for projectiles
*/
void AActor::physProjectile(FLOAT deltaTime, INT Iterations)
{
	if ( Location.Z < (Region.Zone->bSoftKillZ ? Region.Zone->KillZ - Region.Zone->SoftKill : Region.Zone->KillZ) )
	{
		eventFellOutOfWorld(Region.Zone->KillZDamageType);
		return;
	}

	FLOAT remainingTime = deltaTime;
	INT numBounces = 0;

	if ( (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		GetLevel()->DestroyActor( this );
		return;
	}

	bJustTeleported = 0;
	FCheckResult Hit(1.f);
	if ( bCollideActors )
	{
		AProjectile *Proj = Cast<AProjectile>(this);
		if ( Proj && Proj->ZeroCollider )
		{
			UBOOL bStillTouching = false;
			for ( INT i=0; i<Touching.Num(); i++ )
				if ( Touching(i) == Proj->ZeroCollider )
				{
					bStillTouching = true;
					break;
				}
			if ( !bStillTouching )
			{
				Proj->ZeroCollider = NULL;
				UCylinderComponent* CylComp = Cast<UCylinderComponent>(CollisionComponent);
				// FIXMESTEVE-reimplement setcollisionsize()
				//if ( CylComp )
				//	CylComp->SetSize(CylComp->GetDefaultObject()->CollisionRadius,CylComp->GetDefaultObject()->CollisionHeight); 
			}
		}
	}

	while ( remainingTime > 0.f )
	{
		Iterations++;
		if ( !Acceleration.IsZero() )
		{
			//debugf(TEXT("%s has acceleration!"),GetName());
			Velocity = Velocity	+ Acceleration * remainingTime;
			BoundProjectileVelocity();
		}

		FLOAT timeTick = remainingTime;
		remainingTime = 0.f;
		FVector Adjusted = Velocity * deltaTime;

		Hit.Time = 1.f;
		FVector StartLocation = Location;
		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);	

		if( Hit.Time<1.f && !bDeleteMe && !bJustTeleported )
		{
			if ( ShrinkCollision(Hit.Actor, StartLocation) )
				remainingTime = timeTick * (1.f - Hit.Time);
			else
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				if (bBounce)
				{
					if (numBounces < 2)
						remainingTime = timeTick * (1.f - Hit.Time);
					numBounces++;
					if (Physics == PHYS_Falling)
						physFalling(remainingTime, Iterations);
				}
			}
		}
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
	if ( Velocity.SizeSquared() > MaxSpeed * MaxSpeed )
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= MaxSpeed;
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
			GetLevel()->MoveActor(this, OnLadder->LookDir * remainingTime * GroundSpeed, Rotation, Hit);
			remainingTime = remainingTime * (1.f - Hit.Time);
			if ( !OnLadder )
			{
				if ( PhysicsVolume->bWaterVolume )
					setPhysics(PHYS_Swimming);
				else
					setPhysics(PHYS_Walking);
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
		GetLevel()->MoveActor(this, MoveDir, Rotation, Hit);
		remainingTime = remainingTime * (1.f - Hit.Time);

		if ( !OnLadder )
		{
			//Moved out of ladder, try to step onto ledge
			if ( (MoveDir | PhysicsVolume->Gravity) > 0.f )
			{
				setPhysics(PHYS_Falling);
				return;
			}
			FVector Out = MoveDir.SafeNormal();
			Out *= 1.1f * CylinderComponent->CollisionHeight;
			GetLevel()->MoveActor(this, Out, Rotation, Hit);
			GetLevel()->MoveActor(this, 0.5f * OldLadder->LookDir * CylinderComponent->CollisionRadius, Rotation, Hit);
			GetLevel()->MoveActor(this, -1.f * (Out + MoveDir), Rotation, Hit);
			GetLevel()->MoveActor(this, (-0.5f * CylinderComponent->CollisionRadius + 3.f) * OldLadder->LookDir , Rotation, Hit);
			Velocity = FVector(0,0,0);
			if ( PhysicsVolume->bWaterVolume )
				setPhysics(PHYS_Swimming);
			else
				setPhysics(PHYS_Walking);
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
			GetLevel()->MoveActor(this, MoveDir, Rotation, Hit);
			if ( Hit.Time < 1.f )
			{
				FVector GravDir = FVector(0,0,-1);
				if (PhysicsVolume->Gravity.Z > 0)
					GravDir.Z = 1;
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
			GetLevel()->MoveActor(this, -1.f * ClimbDir * MaxStepHeight, Rotation, Hit);
			FLOAT Dist = Hit.Time * MaxStepHeight;
			if ( Hit.Time == 1.f )
				GetLevel()->MoveActor(this, PushDir * deltaTime * GroundSpeed, Rotation, Hit);
			GetLevel()->MoveActor(this, ClimbDir * Dist, Rotation, Hit);
			if ( !OnLadder )
			{
				if ( PhysicsVolume->bWaterVolume )
					setPhysics(PHYS_Swimming);
				else
					setPhysics(PHYS_Walking);
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
	GetLevel()->SingleLineCheck(Hit, 0, Location - MaxStepHeight * Dir, Location, TRACE_World, GetCylinderExtent());
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
		GetLevel()->MoveActor(this, -1 * Down, Rotation, Hit);
		GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	}
	else // walk up slope
	{
		Floor = Hit.Normal;
		Down = -1.f * Floor * MaxStepHeight;
		FLOAT Dist = Delta.Size();
		GetLevel()->MoveActor(this, Delta + FVector(0,0,Dist*Hit.Normal.Z), Rotation, Hit);
	}

	if (Hit.Time < 1.f)
	{
		if ( ((Floor | Hit.Normal) < 0.1) && (Hit.Time * Delta.SizeSquared() > 144.f) )
		{
			// try another step
			GetLevel()->MoveActor(this, Down, Rotation, Hit);
			SpiderstepUp(DesiredDir, Delta * (1 - Hit.Time), Hit);
			return;
		}

		// Found a new floor
		FVector OldFloor = Floor;
		Floor = Hit.Normal;
		Down = -1.f * Floor * MaxStepHeight;

		//adjust and try again
		Hit.Normal.Z = 0;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector NewDelta = Delta;
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
			GetLevel()->MoveActor(this, NewDelta, Rotation, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit.Normal, Hit.Actor);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, NewDelta, Hit.Normal, OldHitNormal, Hit.Time);
				GetLevel()->MoveActor(this, NewDelta, Rotation, Hit);
			}
		}
	}
	GetLevel()->MoveActor(this, Down, Rotation, Hit);
}

void APawn::physSpider(FLOAT deltaTime, INT Iterations)
{
	if ( !Controller )
		return;
	if ( Floor.IsNearlyZero() && !findNewFloor(Location, deltaTime, deltaTime, Iterations) )
		return;

	// calculate velocity
	FVector AccelDir;
	if ( Acceleration.IsZero() )
	{
		AccelDir = Acceleration;
		FVector OldVel = Velocity;
		Velocity = Velocity - (2 * Velocity) * deltaTime * PhysicsVolume->GroundFriction; //don't drift to a stop, brake
		if ((OldVel | Velocity) < 0.f) //brake to a stop, not backwards
			Velocity = Acceleration;
	}
	else
	{
		AccelDir = Acceleration.SafeNormal();
		FLOAT VelSize = Velocity.Size();
		if (Acceleration.SizeSquared() > AccelRate * AccelRate)
			Acceleration = AccelDir * AccelRate;
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * deltaTime * PhysicsVolume->GroundFriction;
	}

	Velocity = Velocity + Acceleration * deltaTime;
	// only move along plane of floor
	Velocity = Velocity - Floor * (Floor | Velocity);

	FLOAT maxSpeed = GroundSpeed * DesiredSpeed;
	Iterations++;

	if (Velocity.SizeSquared() > maxSpeed * maxSpeed)
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= maxSpeed;
	}
	FVector DesiredMove = Velocity;

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
		// subdivide moves to be no longer than 0.05 seconds for players, or no longer than the collision radius for non-players
		if ( (remainingTime > 0.05f) && (IsHumanControlled() ||
			(DesiredMove.SizeSquared() * remainingTime * remainingTime > Min(CylinderComponent->CollisionRadius * CylinderComponent->CollisionRadius, MaxStepHeightSq))) )
				timeTick = Min(0.05f, remainingTime * 0.5f);
		else timeTick = remainingTime;
		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector subLoc = Location;
		int bZeroMove = Delta.IsNearlyZero();

		if ( bZeroMove )
		{
			remainingTime = 0;
			// if not moving, quick check if still on valid floor
			FVector Foot = Location - CylinderComponent->CollisionHeight * Floor;
			GetLevel()->SingleLineCheck( Hit, this, Foot - 20 * Floor, Foot, TRACE_World );
			FLOAT FloorDist = Hit.Time * 20;
			bZeroMove = ((Base == Hit.Actor) && (FloorDist <= MAXFLOORDIST + CYLINDERREPULSION) && (FloorDist >= MINFLOORDIST + CYLINDERREPULSION));
		}
		else
		{
			// try to move forward
			GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			if (Hit.Time < 1.f)
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

		if ( !bZeroMove )
		{
			//drop to floor
			GetLevel()->SingleLineCheck( Hit, this, Location - Floor * (MaxStepHeight + 2.f), Location, TRACE_AllBlocking, GetCylinderExtent() );
			if ( Hit.Time == 1.f )
			{
				GetLevel()->MoveActor(this, -8.f * Floor, Rotation, Hit);
				// find new floor or fall
				if ( !findNewFloor(Location, deltaTime, deltaTime, Iterations) )
					return;
			}
			else
			{
				Floor = Hit.Normal;
				GetLevel()->MoveActor(this, -1.f * Floor * (MaxStepHeight + 2.f), Rotation, Hit);
				if ( Hit.Actor != Base )
					SetBase(Hit.Actor, Hit.Normal);
			}
		}
	}

	// make velocity reflect actual move
	if (!bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;
}

// When we get an update for the physics, we try to do it smoothly if it is less than .._THRESHOLD.
// We directly fix .._AMOUNT * error. The rest is fixed by altering the velocity to correct the actor over 1.0/.._RECIPFIXTIME seconds.
// So if .._AMOUNT is 1, we will always just move the actor directly to its correct position (as it the error was over .._THRESHOLD
// If .._AMOUNT is 0, we will correct just by changing the velocity.

/** threshold, beyond that actor will be teleported instead of corrected through interpolation */
#define INTERP_LINEAR_THRESHOLD_SQR (1000.0f)
/** 1/INTERP_LINEAR_RECIPFIXTIME seconds to reach auto corrected position */
#define INTERP_LINEAR_RECIPFIXTIME (1.0f)
/** below that threshold, position will be fixed by INTERP_LINEAR_AMOUNT * Error. Otherwise, we only rely on velocity adjustment */
#define INTERP_LINEAR_VELOCITY_THRESHOLD_SQR (0.2f)
/** Amount of error fixed */
#define INTERP_LINEAR_AMOUNT (0.2f)

#define	INTERP_LINEAR_MINVEL 

/** Interpolation, client position correction */
void AActor::physInterpolatingCorrectPosition( const FVector &NewLocation )
{
	FVector	Delta	= NewLocation - Location;
	FVector	VelocityAdjust(0.f);

	// If delta from Old to New Location is small enough, then proceed with interpolation
	if( Delta.SizeSquared() > KINDA_SMALL_NUMBER && 
		Delta.SizeSquared() < INTERP_LINEAR_THRESHOLD_SQR )
	{
		// If velocity is small enough, then adjust it won't be enough, so we slowly teleport the actor closer to its destination (step by step)
		if( Velocity.SizeSquared() < INTERP_LINEAR_VELOCITY_THRESHOLD_SQR )
		{
			FVector InterpLoc = INTERP_LINEAR_AMOUNT * NewLocation + (1.0f - INTERP_LINEAR_AMOUNT) * Location;
			GetLevel()->FarMoveActor( this, InterpLoc, 0, 1, 1 );
		}

		VelocityAdjust = Delta * INTERP_LINEAR_RECIPFIXTIME;
	}
	else
	{
		// if delta too big, just teleport there.
		GetLevel()->FarMoveActor( this, NewLocation, 0, 1, 1 );
	}

	/*
	// debug server actual position
	GetLevel()->PersistentLineBatcher->BatchedLines.Empty();
	GetLevel()->PersistentLineBatcher->DrawLine(NewLocation, NewLocation+FVector(0,0,500), FColor(255, 0, 0));
	GetLevel()->PersistentLineBatcher->DrawLine(NewLocation, NewLocation+FVector(0,500,0), FColor(255, 0, 0));
	GetLevel()->PersistentLineBatcher->DrawLine(NewLocation, NewLocation+FVector(500,0,0), FColor(255, 0, 0));
	*/

	// Adjust velocity
	// FIXME LAURENT -- Make sure NewLocation and Velocity are replicated together, otherwise the adjusted Velocity can be overwritten later...
	Velocity += VelocityAdjust;

	// TODO: rotation and angular velocity replication.
	// but shouldn't it be better to replicate the matinee timeline position and play rate instead?
}

void AActor::physInterpolating(FLOAT deltaTime)
{
	// Simulate client side
	if( Level->NetMode == NM_Client )
	{
		FVector	DeltaMove = Velocity * deltaTime;

		// Move Actor
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor(this, DeltaMove, Rotation, Hit);

		UpdateRBPhysKinematicData();
		/*
		// debug client actual position
		GetLevel()->LineBatcher->DrawLine(Location, Location+FVector(0,0,500), FColor(0, 255, 0));
		GetLevel()->LineBatcher->DrawLine(Location, Location+FVector(0,500,0), FColor(0, 255, 0));
		GetLevel()->LineBatcher->DrawLine(Location, Location+FVector(500,0,0), FColor(0, 255, 0));
		*/
		return;
	}

	UInterpTrackMove*		MoveTrack;
	UInterpTrackInstMove*	MoveInst;
	USeqAct_Interp*			Seq;

	Velocity = FVector(0.f);

	// If we have a movement track currently working on this Actor, update position to co-incide with it.
	if( FindInterpMoveTrack(&MoveTrack, &MoveInst, &Seq) )
	{
		// We found a movement track - so use it to update the current position.
		FVector NewPos = Location;
		FRotator NewRot = Rotation;

		MoveTrack->GetLocationAtTime(MoveInst, Seq->Position, NewPos, NewRot);

		FVector OldLocation = Location;

		FCheckResult Hit(1.f);
		GetLevel()->MoveActor(this, NewPos - Location, NewRot, Hit);

		Velocity = (Location - OldLocation)/deltaTime;
		UpdateRBPhysKinematicData();

		// If based on something - update the RelativeLocation and RelativeRotation so that its still correct with the new position.
		AActor* BaseActor = GetBase();
		if(BaseActor)
		{
			FMatrix BaseTM(BaseActor->Location, BaseActor->Rotation);
			BaseTM.RemoveScaling();

			FMatrix InvBaseTM = BaseTM.Inverse();

			FMatrix ActorTM(Location, Rotation);

			FMatrix RelTM = ActorTM * InvBaseTM;
			RelativeLocation = RelTM.GetOrigin();
			RelativeRotation = RelTM.Rotator();
		}
	}
}