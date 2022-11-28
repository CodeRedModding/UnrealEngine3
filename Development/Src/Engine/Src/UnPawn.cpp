/*=============================================================================
	UnPawn.cpp: APawn AI implementation

  This contains both C++ methods (movement and reachability), as well as some
  AI related natives

	Copyright 1997-2004 Epic MegaGames, Inc. This software is a trade secret.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "FConfigCacheIni.h"
#include "UnPath.h"

/*-----------------------------------------------------------------------------
	APawn object implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(APawn);

APawn* APawn::GetPlayerPawn() const
{
	if ( !Controller || !Controller->IsAPlayerController() )
		return NULL;

	return ((APawn *)this);
}

UBOOL APawn::IsPlayer()
{
	return ( Controller && Controller->bIsPlayer );
}

UBOOL APawn::IsHumanControlled()
{
	return ( Controller && Controller->GetAPlayerController() ); 
}

UBOOL APawn::IsLocallyControlled()
{
	return ( Controller && Controller->LocalPlayerController() );
}

UBOOL APawn::ReachedDesiredRotation()
{
	//only base success on Yaw 
	if ( Abs(DesiredRotation.Yaw - (Rotation.Yaw & 65535)) < 2000 )
		return true;

	if ( Abs(DesiredRotation.Yaw - (Rotation.Yaw & 65535)) > 63535 )
		return true;

	return false;
}

inline UBOOL APawn::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	if( this->bOnlyAffectPawns && SourceActor && !SourceActor->GetAPawn() )
		return false;

// FIXMESTEVE - hack for turrets, etc.
//	if ( bStationary )
//		return (TraceFlags & 0x0010 ); // TRACE_Others

	// inlined for speed on PS2
	return (TraceFlags & 0x0001 /*TRACE_Pawns*/);
}

UBOOL APawn::DelayScriptReplication(FLOAT LastFullUpdateTime) 
{ 
	return ( LastFullUpdateTime != Level->TimeSeconds );
}

void APawn::SetAnchor(ANavigationPoint *NewAnchor)
{
	Anchor = NewAnchor;
	if ( Anchor )
	{
		LastValidAnchorTime = Level->TimeSeconds;
		LastAnchor = Anchor;
	}
}

/** @see Pawn::SetRemoteViewPitch */
void APawn::execSetRemoteViewPitch( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(NewRemoteViewPitch);
	P_FINISH;

	RemoteViewPitch = (NewRemoteViewPitch & 65535) >> 8;
}

/* PlayerCanSeeMe()
	returns true if actor is visible to some player
*/
void AActor::execPlayerCanSeeMe( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	int seen = 0;
	if ( (Level->NetMode == NM_Standalone) || (Level->NetMode == NM_Client) )
	{
		// just check local player visibility
		seen = (GetLevel()->TimeSeconds - LastRenderTime < 1);
	}
	else
	{
		for ( AController *next=Level->ControllerList; next!=NULL; next=next->NextController )
			if ( TestCanSeeMe(next->GetAPlayerController()) )
			{
				seen = 1;
				break;
			}
	}
	*(DWORD*)Result = seen;
}

int AActor::TestCanSeeMe( APlayerController *Viewer )
{
	if ( !Viewer )
		return 0;
	if ( Viewer->GetViewTarget() == this )
		return 1;

	float distSq = (Location - Viewer->ViewTarget->Location).SizeSquared();

	// JAG_COLRADIUS_HACK
	FVector BoxExtent = GetComponentsBoundingBox(1).GetExtent();

	return ( (distSq < 100000.f * ( Max(BoxExtent.X, BoxExtent.Y) + 3.6 ))
		&& ( Viewer->PlayerCamera != NULL
			|| (Square(Viewer->Rotation.Vector() | (Location - Viewer->ViewTarget->Location)) >= 0.25f * distSq) )
		&& Viewer->LineOfSightTo(this) );

}

/*-----------------------------------------------------------------------------
	Pawn related functions.
-----------------------------------------------------------------------------*/

void APawn::execForceCrouch( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	Crouch(false);
}
void APawn::execReachedDestination( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(GoalActor);
	P_FINISH;

	if ( GoalActor )
		*(DWORD*)Result = ReachedDestination(GoalActor->Location - Location, GoalActor);
	else
		*(DWORD*)Result = 0;
}

/*MakeNoise
- check to see if other creatures can hear this noise
*/
void AActor::execMakeNoise( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(Loudness);
	P_FINISH;	
	if ( (Level->NetMode != NM_Client) && Instigator )
	{
		if ( (Owner == Instigator) || (Instigator == this) )
			Loudness *= Instigator->SoundDampening;
		if ( Instigator->Visibility < 2 )
			Loudness *= 0.3f;
		CheckNoiseHearing(Loudness);
	}
}

//=================================================================================
void APawn::setMoveTimer(FVector MoveDir)
{
	if ( !Controller )
		return;

	if ( DesiredSpeed == 0.f )
		Controller->MoveTimer = 0.5f;
	else
	{
		FLOAT Extra = 2.f;
		if ( bIsCrouched )
			Extra = ::Max(Extra, 1.f/CrouchedPct);
		else if ( bIsWalking )
			Extra = ::Max(Extra, 1.f/WalkingPct);
		FLOAT MoveSize = MoveDir.Size();
		Controller->MoveTimer = 0.5f + Extra * MoveSize/(DesiredSpeed * 0.6f * GetMaxSpeed());
	}
}

FLOAT APawn::GetMaxSpeed()
{
	if (Physics == PHYS_Flying)
		return AirSpeed;
	else if (Physics == PHYS_Swimming)
		return WaterSpeed;
	return GroundSpeed;
}

/* StartNewSerpentine()
pawn is using serpentine motion while moving to avoid being hit (while staying within the reachspec
its using.  At this point change direction (either reverse, or go straight for a while
*/
void APawn::StartNewSerpentine(FVector Dir, FVector Start)
{
	FVector NewDir(Dir.Y, -1.f * Dir.X, Dir.Z);
	if ( (NewDir | (Location - Start)) > 0.f )
		NewDir *= -1.f;
	SerpentineDir = NewDir;

	if ( !Controller->bAdvancedTactics )
	{
		SerpentineTime = 9999.f;
		SerpentineDist = appFrand();
		if ( appFrand() < 0.4f )
			SerpentineDir *= -1.f;
		SerpentineDist *= ::Max(0.f,Controller->CurrentPath->CollisionRadius - CylinderComponent->CollisionRadius);
		return;
	}
	
	if ( appFrand() < 0.2f )
	{
		SerpentineTime = 0.1f + 0.4f * appFrand();
		return;
	}
	SerpentineTime = 0.f;


	FLOAT ForcedStrafe = ::Min(1.f, 4.f * CylinderComponent->CollisionRadius/Controller->CurrentPath->CollisionRadius);
	SerpentineDist = (ForcedStrafe + (1.f - ForcedStrafe) * appFrand());
	SerpentineDist *= (Controller->CurrentPath->CollisionRadius - CylinderComponent->CollisionRadius);
}

/* ClearSerpentine()
completely clear all serpentine related attributes
*/
void APawn::ClearSerpentine()
{
	SerpentineTime = 999.f;
	SerpentineDist = 0.f;
}

/* moveToward()
move Actor toward a point.  Returns 1 if Actor reached point
(Set Acceleration, let physics do actual move)
*/
UBOOL APawn::moveToward(const FVector &Dest, AActor *GoalActor )
{
	if ( !Controller )
		return false;

	if ( Controller->bAdjusting )
		GoalActor = NULL;
	FVector Direction = Dest - Location;
	FLOAT ZDiff = Direction.Z;

	if (Physics == PHYS_Walking)
		Direction.Z = 0.f;
	else if (Physics == PHYS_Falling)
	{
		// use air control if low grav
		if ( (Velocity.Z < 0.f) && (PhysicsVolume->Gravity.Z > 0.9f * ((APhysicsVolume *)PhysicsVolume->GetClass()->GetDefaultObject())->Gravity.Z) )
		{
			if ( ZDiff > 0.f )
		{
				if ( ZDiff > 2.f * MaxJumpHeight )
				{
					Controller->MoveTimer = -1.f;
					Controller->eventNotifyMissedJump();
				}
			}
			else
			{
				if ( (Velocity.X == 0.f) && (Velocity.Y == 0.f) )
					Acceleration = FVector(0.f,0.f,0.f);
				else
				{
					FLOAT Dist2D = Direction.Size2D();
			Direction.Z = 0.f;
					Acceleration = Direction;
					Acceleration = Acceleration.SafeNormal();
			Acceleration *= AccelRate;
					if ( (Dist2D < 0.5f * Abs(Direction.Z)) && ((Velocity | Direction) > 0.5f*Dist2D*Dist2D) )
						Acceleration *= -1.f;

					// JAG_COLRADIUS_HACK
					//if ( Dist2D < (GoalActor ? ::Min(GoalActor->CollisionRadius, 1.5f*CylinderComponent->CollisionRadius) : 1.5f*CylinderComponent->CollisionRadius) )
					if ( Dist2D < 1.5f*CylinderComponent->CollisionRadius )
					{
						Velocity.X = 0.f;
						Velocity.Y = 0.f;
						Acceleration = FVector(0.f,0.f,0.f);
					}
					else if ( (Velocity | Direction) < 0.f )
					{
						FLOAT M = ::Max(0.f, 0.2f - AvgPhysicsTime);
						Velocity.X *= M;
						Velocity.Y *= M;
					}
				}
			}
		}
		return false; // don't end move until have landed
	}
	else if ( (Physics == PHYS_Ladder) && OnLadder )
	{
		if ( ReachedDestination(Dest - Location, GoalActor) )
		{
			Acceleration = FVector(0.f,0.f,0.f);

			// if Pawn just reached a navigation point, set a new anchor
			ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
			if ( Nav )
				SetAnchor(Nav);
			return true;
		}
		Acceleration = Direction.SafeNormal();
		if ( GoalActor && (OnLadder != GoalActor->PhysicsVolume)
			&& ((Acceleration | (OnLadder->ClimbDir + OnLadder->LookDir)) > 0.f)
			&& (GoalActor->Location.Z < Location.Z) )
			setPhysics(PHYS_Falling);
		Acceleration *= LadderSpeed;
		return false;
	}
	if ( Controller->MoveTarget && Controller->MoveTarget->IsA(APickupFactory::StaticClass()) 
		 && (Abs(Location.Z - Controller->MoveTarget->Location.Z) < CylinderComponent->CollisionHeight)
		 && (Square(Location.X - Controller->MoveTarget->Location.X) + Square(Location.Y - Controller->MoveTarget->Location.Y) < Square(CylinderComponent->CollisionRadius)) )
		 Controller->MoveTarget->eventTouch(this, Location, (Controller->MoveTarget->Location - Location) );
	
	FLOAT Distance = Direction.Size();
	UBOOL bGlider = ( !bCanStrafe && ((Physics == PHYS_Flying) || (Physics == PHYS_Swimming)) );
	FCheckResult Hit(1.f);

	if ( ReachedDestination(Dest - Location, GoalActor) )
	{
		if ( !bGlider )
			Acceleration = FVector(0.f,0.f,0.f);

		// if Pawn just reached a navigation point, set a new anchor
		ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
		if ( Nav )
			SetAnchor(Nav);
		return true;
	}
	else 
	// if walking, and within radius, and
	// goal is null or
	// the vertical distance is greater than collision + step height and trace hit something to our destination
	if (Physics == PHYS_Walking 
		&& Distance < (CylinderComponent->CollisionRadius * MoveThresholdScale) &&
		(GoalActor == NULL ||
		 (ZDiff > CylinderComponent->CollisionHeight + 2.f * MaxStepHeight && 
		  !GetLevel()->SingleLineCheck(Hit, this, Dest, Location, TRACE_World))))
	{
		return true;
	}
	else if ( bGlider )
		Direction = Rotation.Vector();
	else if ( Distance > 0.f )
	{
		Direction = Direction/Distance;
		if ( Controller->CurrentPath )
		{
			if ( SerpentineTime > 0.f )
			{
				SerpentineTime -= AvgPhysicsTime;
				if ( SerpentineTime <= 0.f )
					StartNewSerpentine(Controller->CurrentPathDir,Controller->CurrentPath->Start->Location);
				else if ( SerpentineDist > 0.f )
				{
					if ( Distance < 2.f * SerpentineDist )
						ClearSerpentine();
					else
					{
						FVector Start = Controller->CurrentPath->Start->Location;
						FVector LineDir = Location - (Start + (Controller->CurrentPathDir | (Location - Start)) * Controller->CurrentPathDir);
						if ( (LineDir.SizeSquared() >= SerpentineDist * SerpentineDist) && ((LineDir | SerpentineDir) > 0.f) )
							Direction = (Dest - Location + SerpentineDir*SerpentineDist).SafeNormal();
						else
							Direction = (Direction + 0.2f * SerpentineDir).SafeNormal();
					}
				}
			}
			if ( SerpentineTime <= 0.f )
			{
				if ( Distance < 2.f * SerpentineDist )
					ClearSerpentine();
				else
				{
					FVector Start = Controller->CurrentPath->Start->Location;
					FVector LineDir = Location - (Start + (Controller->CurrentPathDir | (Location - Start)) * Controller->CurrentPathDir);
					if ( (LineDir.SizeSquared() >= SerpentineDist * SerpentineDist) && ((LineDir | SerpentineDir) > 0.f) )
						StartNewSerpentine(Controller->CurrentPathDir,Start);
					else
						Direction = (Direction + SerpentineDir).SafeNormal();
				}
			}
		}
	}

	Acceleration = Direction * AccelRate;

	if ( !Controller->bAdjusting && Controller->MoveTarget && Controller->MoveTarget->GetAPawn() )
	{
		if (Distance < CylinderComponent->CollisionRadius + Controller->MoveTarget->GetAPawn()->CylinderComponent->CollisionRadius + 0.8f * MeleeRange)
			return true;
		return false;
	}

	FLOAT speed = Velocity.Size();

	if ( !bGlider && (speed > 100.f) )
	{
		FVector VelDir = Velocity/speed;
		Acceleration -= 0.2f * (1 - (Direction | VelDir)) * speed * (VelDir - Direction);
	}
	if ( Distance < 1.4f * AvgPhysicsTime * speed )
	{
		if ( !bReducedSpeed ) //haven't reduced speed yet
		{
			DesiredSpeed = 0.51f * DesiredSpeed;
			bReducedSpeed = 1;
		}
		if ( speed > 0 )
			DesiredSpeed = Min(DesiredSpeed, 200.f/speed);
		if ( bGlider )
			return true;
	}
	return false;
}

/* rotateToward()
rotate Actor toward a point.  Returns 1 if target rotation achieved.
(Set DesiredRotation, let physics do actual move)
*/
void APawn::rotateToward(AActor *Focus, FVector FocalPoint)
{
	if ( bRollToDesired || (Physics == PHYS_Spider) )
		return;
	UBOOL bGlider = !bCanStrafe && ((Physics == PHYS_Flying) || (Physics == PHYS_Swimming) || bFlyingRigidBody);
	if ( bGlider )
		Acceleration = Rotation.Vector() * AccelRate;

	if ( Focus )
	{
		ANavigationPoint *NavFocus = Cast<ANavigationPoint>(Focus);
		if ( NavFocus && Controller && Controller->CurrentPath && (Controller->MoveTarget == NavFocus) && !Velocity.IsZero() )
		{
			// gliding pawns must focus on where they are going
			if ( bGlider )
			{
				if ( Controller->bAdjusting )
					FocalPoint = Controller->AdjustLoc;
				else
					FocalPoint = Focus->Location;
			}
		else
				FocalPoint = Focus->Location - Controller->CurrentPath->Start->Location + Location;
}
		else if ( !Controller || (Controller->FocusLead == 0.f) )
			FocalPoint = Focus->Location;
		else
		{
			FLOAT Dist = (Location - Focus->Location).Size();
			FocalPoint = Focus->Location + Controller->FocusLead * Focus->Velocity * Dist;
		}
	}
	FVector Direction = FocalPoint - Location;

	// Rotate toward destination
	DesiredRotation = Direction.Rotation();
	DesiredRotation.Yaw = DesiredRotation.Yaw & 65535;
	if ( (Physics == PHYS_Walking) && (!Controller || !Controller->MoveTarget || !Controller->MoveTarget->GetAPawn()) )
		DesiredRotation.Pitch = 0;

}

/* HurtByVolume() - virtual	`
Returns true if pawn can be hurt by the zone Z.  By default, pawns are not hurt by pain zones
whose damagetype == their ReducedDamageType
*/
UBOOL APawn::HurtByVolume(AActor *A)
{
	if ( A->PhysicsVolume )
	{
		if ( A->PhysicsVolume->bPainCausing && (A->PhysicsVolume->DamageType != ReducedDamageType) 
			&& (A->PhysicsVolume->DamagePerSec > 0) )
			return true;
	}
	else
	{
	for ( INT i=0; i<A->Touching.Num(); i++ )
	{
		APhysicsVolume *V = Cast<APhysicsVolume>(A->Touching(i));
		if ( V && V->bPainCausing && (V->DamageType != ReducedDamageType)
			&& (V->DamagePerSec > 0) )
			return true;
	}
	}

	return false;
}

int APawn::actorReachable(AActor *Other, UBOOL bKnowVisible, UBOOL bNoAnchorCheck)
{
	if ( !Other || Other->bDeleteMe )
		return 0;

	if ( (Other->Physics == PHYS_Flying) && !bCanFly )
		return 0;

	// If goal is on the navigation network, check if it will give me reachability
	ANavigationPoint *Nav = Cast<ANavigationPoint>(Other);
	if (Nav != NULL)
	{
		FVector Dir = Other->Location - Location;
		if (!bNoAnchorCheck)
		{
			if (ReachedDestination(Dir,Nav))
			{
				Anchor = Nav;
				return 1;
			}
			else
			if ( ValidAnchor() )
			{
				UReachSpec* Path = Anchor->GetReachSpecTo(Nav);
				return ( Path && Path->supports(CylinderComponent->CollisionRadius,CylinderComponent->CollisionHeight,calcMoveFlags(),MaxFallSpeed) );			
			}
		}
		// prevent long reach tests during gameplay as they are expensive
		if (Level->bBegunPlay &&
			Dir.Size2D() > 0.5f * MAXPATHDIST)
		{
			return 0;
		}
	}	

	FVector Dir = Other->Location - Location;
	FLOAT distsq = Dir.SizeSquared();

	if ( Level->bBegunPlay )
	{
		// Use the navigation network if more than MAXPATHDIST units to goal
		if( distsq > MAXPATHDISTSQ )
			return 0;
		if ( HurtByVolume(Other) )
			return 0;
		if ( Other->PhysicsVolume && Other->PhysicsVolume->bWaterVolume )
		{
			if ( !bCanSwim )
				return 0;
		}
		else if ( !bCanFly && !bCanWalk )
			return 0;
	}

	//check other visible
	if ( !bKnowVisible )
	{
		FCheckResult Hit(1.f);
		FVector	ViewPoint = Location;
		ViewPoint.Z += BaseEyeHeight; //look from eyes
		GetLevel()->SingleLineCheck(Hit, this, Other->Location, ViewPoint, TRACE_World|TRACE_StopAtFirstHit);
		if( Hit.Time!=1.f && Hit.Actor!=Other )
			return 0;
	}

	if ( Other->GetAPawn() )
	{
		APawn* OtherPawn = (APawn*)Other;

		FLOAT Threshold = CylinderComponent->CollisionRadius + ::Min(1.5f * CylinderComponent->CollisionRadius, MeleeRange) + OtherPawn->CylinderComponent->CollisionRadius;
		FLOAT Thresholdsq = Threshold * Threshold;
		if (distsq <= Thresholdsq)
			return 1;
	}
	FVector realLoc = Location;
	FVector aPoint = Other->Location; //adjust destination
	if ( Other->Physics == PHYS_Falling )
	{
		// check if ground below it
		FCheckResult Hit(1.f);
		GetLevel()->SingleLineCheck(Hit, this, Other->Location - FVector(0.f,0.f,400.f), Other->Location, TRACE_World);
		if ( Hit.Time == 1.f )
			return false;
		aPoint = Hit.Location + FVector(0.f,0.f,CylinderComponent->CollisionRadius + MaxStepHeight);
		if ( GetLevel()->FarMoveActor(this, aPoint, 1) )
		{
			aPoint = Location;
			GetLevel()->FarMoveActor(this, realLoc,1,1);
			FVector	ViewPoint = Location;
			ViewPoint.Z += BaseEyeHeight; //look from eyes
			GetLevel()->SingleLineCheck(Hit, this, aPoint, ViewPoint, TRACE_World);
			if( Hit.Time!=1.f && Hit.Actor!=Other )
				return 0;
		}
		else
			return 0;
	}
	else
	{
		// JAG_COLRADIUS_HACK
		FLOAT OtherRadius, OtherHeight;
		Other->GetBoundingCylinder(OtherRadius, OtherHeight);

		if ( ((CylinderComponent->CollisionRadius > OtherRadius) || (CylinderComponent->CollisionHeight > OtherHeight))
			&& GetLevel()->FarMoveActor(this, aPoint, 1) )
		{
			aPoint = Location;
			GetLevel()->FarMoveActor(this, realLoc,1,1);
		}
	}
	return Reachable(aPoint, Other);
}

int APawn::pointReachable(FVector aPoint, int bKnowVisible)
{
	if (Level->bBegunPlay)
	{
		FVector Dir2D = aPoint - Location;
		Dir2D.Z = 0.f;
		if (Dir2D.SizeSquared() > MAXPATHDISTSQ)
			return 0;
	}

	//check aPoint visible
	if ( !bKnowVisible )
	{
		FVector	ViewPoint = Location;
		ViewPoint.Z += BaseEyeHeight; //look from eyes
		FCheckResult Hit(1.f);
		GetLevel()->SingleLineCheck( Hit, this, aPoint, ViewPoint, TRACE_World|TRACE_StopAtFirstHit );
		if ( Hit.Actor )
			return 0;
	}

	FVector realLoc = Location;
	if ( GetLevel()->FarMoveActor(this, aPoint, 1) )
	{
		aPoint = Location; //adjust destination
		GetLevel()->FarMoveActor(this, realLoc,1,1);
	}
	return Reachable(aPoint, NULL);

}

/**
 * Pawn crouches.
 * Checks if new cylinder size fits (no encroachment), and trigger Pawn->eventStartCrouch() in script if succesful.
 *
 * @param	bTest				true when testing for path reachability
 * @param	bClientSimulation	true when called when bIsCrouched is replicated to non owned clients, to update collision cylinder and offset.
 */
void APawn::Crouch(INT bTest, INT bClientSimulation)
{
	// Do not perform if collision is already at desired size.
	if( (CylinderComponent->CollisionHeight == CrouchHeight) && (CylinderComponent->CollisionRadius == CrouchRadius) )
	{
		return;
	}

	// Change collision size to crouching dimensions
	FLOAT OldHeight = CylinderComponent->CollisionHeight;
	FLOAT OldRadius = CylinderComponent->CollisionRadius;
	SetCollisionSize(CrouchRadius, CrouchHeight);

	UBOOL bEncroached	= false;
	FLOAT HeightAdjust	= OldHeight - CrouchHeight;

	if( !bTest && 
		!bClientSimulation && 
		((CrouchRadius > OldRadius) || (CrouchHeight > OldHeight)) )
	{
		FMemMark Mark(GMem);
		FCheckResult* FirstHit = GetLevel()->Hash->ActorEncroachmentCheck
			(	GMem,
				this, 
				Location - FVector(0,0,HeightAdjust), 
				Rotation, 
				TRACE_Pawns | TRACE_Movers | TRACE_Others 
			);

		for( FCheckResult* Test = FirstHit; Test!=NULL; Test=Test->GetNext() )
		{
			if ( (Test->Actor != this) && IsBlockedBy(Test->Actor,Test->Component) )
			{
				bEncroached = true;
				break;
			}
		}
		Mark.Pop();
	}

	// If encroached, cancel
	if( bEncroached )
	{
		SetCollisionSize(OldRadius, OldHeight);
		return;
	}

	if( !bTest || bClientSimulation )
	{
		if( !bClientSimulation )
		{
			bNetDirty	= true;	// bIsCrouched replication controlled by bNetDirty
			bIsCrouched = true;
		}
		eventStartCrouch( HeightAdjust );
	}
}

/**
 * Pawn uncrouches.
 * Checks if new cylinder size fits (no encroachment), and trigger Pawn->eventEndCrouch() in script if succesful.
 *
 * @param	bTest				true when testing for path reachability
 * @param	bClientSimulation	true when called when bIsCrouched is replicated to non owned clients, to update collision cylinder and offset.
 */
void APawn::UnCrouch(INT bTest, INT bClientSimulation)
{
	// see if space to uncrouch
	FCheckResult Hit(1.f);
	FLOAT	HeightAdjust	= ((APawn *)(GetClass()->GetDefaultObject()))->CylinderComponent->CollisionHeight - CylinderComponent->CollisionHeight;
	FVector	NewLoc			= Location + FVector(0.f,0.f,HeightAdjust);

	UBOOL bEncroached = false;
	SetCollisionSize(((APawn*)(GetClass()->GetDefaultObject()))->CylinderComponent->CollisionRadius, ((APawn *)(GetClass()->GetDefaultObject()))->CylinderComponent->CollisionHeight);

	if( !bTest && !bClientSimulation )
	{
		FMemMark Mark(GMem);
		FCheckResult* FirstHit = GetLevel()->Hash->ActorEncroachmentCheck
			(	GMem, 
				this, 
				NewLoc, 
				Rotation, 
				TRACE_Pawns | TRACE_Movers | TRACE_Others 
			);

		for( FCheckResult* Test = FirstHit; Test!=NULL; Test=Test->GetNext() )
		{
			if ( (Test->Actor != this) && IsBlockedBy(Test->Actor,Test->Component) )
			{
				bEncroached = true;
				break;
			}
		}
		Mark.Pop();
	}

	// if encroached or cannot move to adjusted location, then abort.
	if( bEncroached || !GetLevel()->FarMoveActor(this, NewLoc, bTest, false, true) )
	{
		SetCollisionSize(CrouchRadius, CrouchHeight);
		return;
	}

	// space enough to uncrouch, so stand up
	if( !bTest || bClientSimulation )
	{
		if( !bClientSimulation )
		{
			bNetDirty	= true;			// bIsCrouched replication controlled by bNetDirty
			bIsCrouched = false;
		}
		eventEndCrouch( HeightAdjust );
	}
}

int APawn::Reachable(FVector aPoint, AActor* GoalActor)
{
	UBOOL bShouldUncrouch = false;
	INT Result = 0;
	if ( bCanCrouch && !bIsCrouched )
	{
		// crouch for reach test
		bShouldUncrouch = true;
		Crouch(1);
	}
	if ( PhysicsVolume->bWaterVolume )
		Result = swimReachable(aPoint, 0, GoalActor);
	else if ( PhysicsVolume->IsA(ALadderVolume::StaticClass()) )
		Result = ladderReachable(aPoint, 0, GoalActor);
	else if ( (Physics == PHYS_Walking) || (Physics == PHYS_Swimming) || (Physics == PHYS_Ladder) || (Physics == PHYS_Falling) )
		Result = walkReachable(aPoint, 0, GoalActor);
	else if (Physics == PHYS_Flying)
		Result = flyReachable(aPoint, 0, GoalActor);
	else
	{
/* FIXMESTEVE
		FCheckResult Hit(1.f);

		FVector Slice = CollisionRadius * FVector(1.f,1.f,0.f);
		Slice.Z = 1.f;
		FVector Dest = aPoint + CollisionRadius * (Location - Dest).SafeNormal();
		GetLevel()->SingleLineCheck( Hit, this, Dest, Location, TRACE_World|TRACE_StopAtFirstHit, Slice );
		Result = !Hit.Actor;
*/
	}

	if ( bShouldUncrouch )
	{
		//return to full height
		UnCrouch(1);
	}
	return Result;
}

/*
	ReachedDestination()
	Return true if sufficiently close to destination.
*/
UBOOL APawn::ReachedDestination(FVector Dir, AActor* GoalActor)
{
	// if crouched, Z threshold should be based on standing
	FLOAT FullHeight = ((APawn *)(GetClass()->GetDefaultObject()))->CylinderComponent->CollisionHeight;
	FLOAT UpThreshold = FullHeight + FullHeight - CylinderComponent->CollisionHeight;
	FLOAT DownThreshold = CylinderComponent->CollisionHeight;
	FLOAT Threshold = CylinderComponent->CollisionRadius;

	if ( GoalActor )
	{
		// Navigation points must have their center reached for efficient navigation
		if ( GoalActor->IsA(ANavigationPoint::StaticClass()) )
		{
			ANavigationPoint* GoalNavPoint = (ANavigationPoint*)GoalActor;

			if ( GoalActor->IsA(ALadder::StaticClass()) )
			{
				// look at difference along ladder direction
				return ( OnLadder && (Abs(Dir | OnLadder->ClimbDir) < CylinderComponent->CollisionHeight) );
			}
			if ( GoalActor->bCollideActors && GoalActor->IsA(ATeleporter::StaticClass()) && Level->bBegunPlay )
			{
				// must touch teleporter
				for ( INT i=0; i<GoalActor->Touching.Num(); i++ )
					if ( GoalActor->Touching(i) == this )
						return true;
				return false;
			}

			UpThreshold = ::Max<FLOAT>(UpThreshold,2.f + GoalNavPoint->CylinderComponent->CollisionHeight - CylinderComponent->CollisionHeight + MaxStepHeight);
			DownThreshold = ::Max<FLOAT>(DownThreshold, 2.f + CylinderComponent->CollisionHeight + MaxStepHeight - GoalNavPoint->CylinderComponent->CollisionHeight);
		}
		else if ( GoalActor->IsA(APawn::StaticClass()) )
		{
			APawn* GoalPawn = (APawn*)GoalActor;

			UpThreshold = ::Max(UpThreshold,GoalPawn->CylinderComponent->CollisionHeight);
			if ( GoalActor->Physics == PHYS_Falling )
				UpThreshold += 2.f * MaxJumpHeight;
			DownThreshold = ::Max(DownThreshold,GoalPawn->CylinderComponent->CollisionHeight);
			Threshold = CylinderComponent->CollisionRadius + ::Min(1.5f * CylinderComponent->CollisionRadius, MeleeRange) + GoalPawn->CylinderComponent->CollisionRadius;
		}
		else
		{
			// JAG_COLRADIUS_HACK
			FLOAT ColHeight, ColRadius;
			GoalActor->GetBoundingCylinder(ColRadius, ColHeight);

			UpThreshold += ColHeight;
			DownThreshold += ColHeight;
			if ( GoalActor->bBlockActors || !Level->bBegunPlay )
				Threshold = CylinderComponent->CollisionRadius + ColRadius;
		}
		Threshold += DestinationOffset;
	}
	// apply any extra scale to the move threshold
	Threshold *= MoveThresholdScale;

	FLOAT Zdiff = Dir.Z;
	Dir.Z = 0.f;
	if ( Dir.SizeSquared() > Threshold * Threshold )
		return false;

	if ( (Zdiff > 0.f) ? (Abs(Zdiff) > UpThreshold) : (Abs(Zdiff) > DownThreshold) )
	{
		if ( (Zdiff > 0.f) ? (Abs(Zdiff) > 2.f * UpThreshold) : (Abs(Zdiff) > 2.f * DownThreshold) )
			return false;

		// Check if above or below because of slope
		FCheckResult Hit(1.f);
		GetLevel()->SingleLineCheck( Hit, this, Location - FVector(0.f,0.f,6.f), Location, TRACE_World, FVector(CylinderComponent->CollisionRadius,CylinderComponent->CollisionRadius,CylinderComponent->CollisionHeight));
		if ( (Hit.Normal.Z < 0.95f) && (Hit.Normal.Z >= UCONST_MINFLOORZ) )
		{
			// check if above because of slope
			if ( (Zdiff < 0)
				&& (Zdiff * -1.f < FullHeight + CylinderComponent->CollisionRadius * appSqrt(1.f/(Hit.Normal.Z * Hit.Normal.Z) - 1.f)) )
			{
				return true;
			}
			else
			{
				// might be below because on slope

				// JAG_COLRADIUS_HACK
				ANavigationPoint* DefaultNavPoint = (ANavigationPoint*)ANavigationPoint::StaticClass()->GetDefaultActor();

				FLOAT adjRad = 0.f;
				if ( GoalActor != NULL )
				{
					FLOAT GoalHeight;
					GoalActor->GetBoundingCylinder(adjRad, GoalHeight);
				}
				else
				{
					adjRad = DefaultNavPoint->CylinderComponent->CollisionRadius;
				}
				if ( (CylinderComponent->CollisionRadius < adjRad)
					&& (Zdiff < FullHeight + (adjRad + 15.f - CylinderComponent->CollisionRadius) * appSqrt(1.f/(Hit.Normal.Z * Hit.Normal.Z) - 1.f)) )
				{
					return true;
				}
			}
		}
		return false;
	}
	return true;
}

/* ladderReachable()
Pawn is on ladder. Return false if no GoalActor, else true if GoalActor is on the same ladder
*/
int APawn::ladderReachable(FVector Dest, int reachFlags, AActor* GoalActor)
{
	if ( !OnLadder || !GoalActor || ((GoalActor->Physics != PHYS_Ladder) && !GoalActor->IsA(ALadder::StaticClass())) )
		return walkReachable(Dest, reachFlags, GoalActor);
	
	ALadder *L = Cast<ALadder>(GoalActor);
	ALadderVolume *GoalLadder = NULL;
	if ( L )
		GoalLadder = L->MyLadder;
	else
	{
		APawn *GoalPawn = GoalActor->GetAPawn(); 
		if ( GoalPawn && GoalPawn->OnLadder )
			GoalLadder = GoalPawn->OnLadder;
		else
			return walkReachable(Dest, reachFlags, GoalActor);
	}

	if ( GoalLadder == OnLadder )
		return (reachFlags | R_LADDER);
	return walkReachable(Dest, reachFlags, GoalActor);
}

int APawn::flyReachable(FVector Dest, int reachFlags, AActor* GoalActor)
{
	reachFlags = reachFlags | R_FLY;
	int success = 0;
	FVector OriginalPos = Location;
	FVector realVel = Velocity;
	ETestMoveResult stillmoving = TESTMOVE_Moved;
	FVector Direction = Dest - Location;
	FLOAT Movesize = ::Max(200.f, CylinderComponent->CollisionRadius);
	FLOAT MoveSizeSquared = Movesize * Movesize;
	INT ticks = 100; 
	if ( !Level->bBegunPlay )
		ticks = 10000;

	while (stillmoving != TESTMOVE_Stopped )
	{
		Direction = Dest - Location;
		if ( !ReachedDestination(Direction, GoalActor) )
		{
			if ( Direction.SizeSquared() < MoveSizeSquared )
				stillmoving = flyMove(Direction, GoalActor, 8.f);
			else
			{
				Direction = Direction.SafeNormal();
				stillmoving = flyMove(Direction * Movesize, GoalActor, MINMOVETHRESHOLD);
			}
			if ( stillmoving == TESTMOVE_HitGoal ) //bumped into goal
			{
				stillmoving = TESTMOVE_Stopped;
				success = 1;
			}
			if ( stillmoving && PhysicsVolume->bWaterVolume )
			{
				stillmoving = TESTMOVE_Stopped;
				if ( bCanSwim && !HurtByVolume(this) )
				{
					reachFlags = swimReachable(Dest, reachFlags, GoalActor);
					success = reachFlags;
				}
			}
		}
		else
		{
			stillmoving = TESTMOVE_Stopped;
			success = 1;
		}
		ticks--;
		if (ticks < 0)
			stillmoving = TESTMOVE_Stopped;
	}

	GetLevel()->FarMoveActor(this, OriginalPos, 1, 1); //move actor back to starting point
	Velocity = realVel;	
	if (success)
		return reachFlags;
	else
		return 0;
}

int APawn::swimReachable(FVector Dest, int reachFlags, AActor* GoalActor)
{
	//debugf("Swim reach to %f %f %f from %f %f %f",Dest.X,Dest.Y,Dest.Z,Location.X,Location.Y, Location.Z);
	reachFlags = reachFlags | R_SWIM;
	int success = 0;
	FVector OriginalPos = Location;
	FVector realVel = Velocity;
	ETestMoveResult stillmoving = TESTMOVE_Moved;
	FVector Direction = Dest - Location;
	FLOAT Movesize = ::Max(200.f, CylinderComponent->CollisionRadius);
	FLOAT MoveSizeSquared = Movesize * Movesize;
	INT ticks = 100; 
	if ( !Level->bBegunPlay )
		ticks = 10000;

	while ( stillmoving != TESTMOVE_Stopped )
	{
		Direction = Dest - Location;
		if ( !ReachedDestination(Direction,GoalActor) )
		{
			if ( Direction.SizeSquared() < MoveSizeSquared )
				stillmoving = swimMove(Direction, GoalActor, 8.f);
			else
			{
				Direction = Direction.SafeNormal();
				stillmoving = swimMove(Direction * Movesize, GoalActor, MINMOVETHRESHOLD);
			}
			if ( stillmoving == TESTMOVE_HitGoal ) //bumped into goal
			{
				stillmoving = TESTMOVE_Stopped;
				success = 1;
			}
			if ( !PhysicsVolume->bWaterVolume )
			{
				stillmoving = TESTMOVE_Stopped;
				if (bCanFly)
				{
					reachFlags = flyReachable(Dest, reachFlags, GoalActor);
					success = reachFlags;
				}
				else if ( bCanWalk && (Dest.Z < Location.Z + CylinderComponent->CollisionHeight + MaxStepHeight) )
				{
					FCheckResult Hit(1.f);
					GetLevel()->MoveActor(this, FVector(0,0,::Max(CylinderComponent->CollisionHeight + MaxStepHeight,Dest.Z - Location.Z)), Rotation, Hit, 1, 1);
					if (Hit.Time == 1.f)
					{
						success = flyReachable(Dest, reachFlags, GoalActor);
						reachFlags = R_WALK | (success & !R_FLY);
					}
				}
			}
			else if ( HurtByVolume(this) )
			{
				stillmoving = TESTMOVE_Stopped;
				success = 0;
			}
		}
		else
		{
			stillmoving = TESTMOVE_Stopped;
			success = 1;
		}
		ticks--;
		if (ticks < 0)
			stillmoving = TESTMOVE_Stopped;
	}

	GetLevel()->FarMoveActor(this, OriginalPos, 1, 1); //move actor back to starting point
	Velocity = realVel;	
	if (success)
		return reachFlags;
	else
		return 0;
}

/*walkReachable() -
walkReachable returns 0 if Actor cannot reach dest, and 1 if it can reach dest by moving in
 straight line
 actor must remain on ground at all times
 Note that Actor is not actually moved
*/
int APawn::walkReachable(FVector Dest, int reachFlags, AActor* GoalActor)
{
	reachFlags = reachFlags | R_WALK;
	INT success = 0;
	FVector OriginalPos = Location;
	FVector realVel = Velocity;
	ETestMoveResult stillmoving = TESTMOVE_Moved;
	FLOAT Movesize = 16.f;
	FVector Direction;
	INT ticks = 100; 
	if (Level->bBegunPlay)
	{
		if ( bJumpCapable )
			Movesize = ::Max(128.f, CylinderComponent->CollisionRadius);
		else
			Movesize = ::Max(12.f, CylinderComponent->CollisionRadius);
	}
	else
		ticks = 10000;
	
	FLOAT MoveSizeSquared = Movesize * Movesize;
	FCheckResult Hit(1.f);

	while ( stillmoving == TESTMOVE_Moved )
	{
		Direction = Dest - Location;
		if ( ReachedDestination(Direction, GoalActor) )
		{
			stillmoving = TESTMOVE_Stopped;
			success = 1;
		}
		else
		{
			Direction.Z = 0; //this is a 2D move
			FLOAT DistanceSquared = Direction.SizeSquared(); //2D size
			if ( DistanceSquared < MoveSizeSquared)
				stillmoving = walkMove(Direction, Hit, GoalActor, 8.f);
			else
			{
				Direction = Direction.SafeNormal();
				Direction *= Movesize;
				stillmoving = walkMove(Direction, Hit, GoalActor, MINMOVETHRESHOLD);
			}
			if (stillmoving != TESTMOVE_Moved)
			{
				if ( stillmoving == TESTMOVE_HitGoal ) //bumped into goal
				{
					stillmoving = TESTMOVE_Stopped;
					success = 1;
				}
				else if ( Region.ZoneNumber == 0 )
				{
					debugf(TEXT("Walked out of world!!!"));
					stillmoving = TESTMOVE_Stopped;
					success = 0;
				}
				else if (bCanFly)
				{
					stillmoving = TESTMOVE_Stopped;
					reachFlags = flyReachable(Dest, reachFlags, GoalActor);
					success = reachFlags;
				}
				else if ( bJumpCapable )
				{
					reachFlags = reachFlags | R_JUMP;	
					if (stillmoving == TESTMOVE_Fell)
					{
						FVector Landing = Dest;
						if ( GoalActor )
						{
							// JAG_COLRADIUS_HACK
							FLOAT GoalRadius, GoalHeight;
							GoalActor->GetBoundingCylinder(GoalRadius, GoalHeight);

							Landing.Z = Landing.Z - GoalHeight + CylinderComponent->CollisionHeight;
						}
						stillmoving = FindBestJump(Landing);
					}
					else if (stillmoving == TESTMOVE_Stopped)
					{
						stillmoving = FindJumpUp(Direction);
						if ( stillmoving == TESTMOVE_HitGoal ) //bumped into goal
						{
							stillmoving = TESTMOVE_Stopped;
							success = 1;
						}
					}
				}
				else if ( (stillmoving == TESTMOVE_Fell) && (Movesize > MaxStepHeight) ) //try smaller
				{
					stillmoving = TESTMOVE_Moved;
					Movesize = MaxStepHeight;
				}
			}
			else if ( !Level->bBegunPlay )// FIXME - make sure fully on path
			{
				FCheckResult Hit(1.f);
				GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0,0,-1 * (0.5f * CylinderComponent->CollisionHeight + MaxStepHeight + 4.0)) , Location, TRACE_World|TRACE_StopAtFirstHit, 0.5f * GetCylinderExtent());
				if ( Hit.Time == 1.f )
					reachFlags = reachFlags | R_JUMP;	
			}
			
			if ( HurtByVolume(this) )
			{
				stillmoving = TESTMOVE_Stopped;
				success = 0;
			}
			else if ( PhysicsVolume->bWaterVolume )
			{
				//debugf("swim from walk");
				stillmoving = TESTMOVE_Stopped;
				if (bCanSwim )
				{
					reachFlags = swimReachable(Dest, reachFlags, GoalActor);
					success = reachFlags;
				}
			}
			else if ( bCanClimbLadders && PhysicsVolume->IsA(ALadderVolume::StaticClass())
					&& GoalActor && (GoalActor->PhysicsVolume == PhysicsVolume) )
			{
				stillmoving = TESTMOVE_Stopped;
				ALadderVolume *RealLadder = OnLadder;
				OnLadder = Cast<ALadderVolume>(PhysicsVolume);

				// JAG_COLRADIUS_HACK
				FLOAT GoalRadius, GoalHeight;
				GoalActor->GetBoundingCylinder(GoalRadius, GoalHeight);

				success = ( Abs((Dest - Location) | OnLadder->ClimbDir) < ::Max(CylinderComponent->CollisionHeight,GoalHeight) );
				OnLadder = RealLadder;
			}

			ticks--;
			if (ticks < 0)
			{
				//debugf(TEXT("OUT OF TICKS"));
				stillmoving = TESTMOVE_Stopped;
			}
		}
	}

	GetLevel()->FarMoveActor(this, OriginalPos, 1, 1); //move actor back to starting point
	Velocity = realVel;
	if (success)
		return reachFlags;
	else
		return 0;
}

int APawn::jumpReachable(FVector Dest, int reachFlags, AActor* GoalActor)
{
	// debugf(TEXT("Jump reach to %f %f %f from %f %f %f"),Dest.X,Dest.Y,Dest.Z,Location.X,Location.Y, Location.Z);
	FVector OriginalPos = Location;
	reachFlags = reachFlags | R_JUMP;
	if ( jumpLanding(Velocity, 1) == TESTMOVE_Stopped )
		return 0;
	int success = walkReachable(Dest, reachFlags, GoalActor);
	GetLevel()->FarMoveActor(this, OriginalPos, 1, 1); //move actor back to starting point
	return success;
}

/* jumpLanding()
determine landing position of current fall, given testVel as initial velocity.
Assumes near-zero acceleration by pawn during jump (make sure creatures do this in script
- e.g. no air control)
*/
ETestMoveResult APawn::jumpLanding(FVector testVel, int movePawn)
{
	FVector OriginalPos = Location;
	int landed = 0;
	int ticks = 0;
	FLOAT tickTime = 0.1f;
	//debugf("Jump vel %f %f %f", testVel.X, testVel.Y, testVel.Z);
	while ( !landed )
	{
		FLOAT NetFluidFriction = PhysicsVolume->bWaterVolume ? PhysicsVolume->FluidFriction : 0.f;
		testVel = testVel * (1 - NetFluidFriction * tickTime) + PhysicsVolume->Gravity * tickTime; 
		FVector Adjusted = (testVel + PhysicsVolume->ZoneVelocity) * tickTime;
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit, 1, 1);
		if( PhysicsVolume->bWaterVolume )
			landed = 1;
		else if ( testVel.Z < -1.f * MaxFallSpeed )
		{
			landed = 1;
			GetLevel()->FarMoveActor(this, OriginalPos, 1, 1); //move actor back to starting point
		}
		else if(Hit.Time < 1.f)
		{
			if( Hit.Normal.Z >= UCONST_MINFLOORZ )
				landed = 1;
			else
			{
				FVector OldHitNormal = Hit.Normal;
				FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
				if( (Delta | Adjusted) >= 0 )
				{
					GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1, 1);
					if (Hit.Time < 1.f) //hit second wall
					{
						if (Hit.Normal.Z >= UCONST_MINFLOORZ)
							landed = 1;	
						FVector DesiredDir = Adjusted.SafeNormal();
						TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
						GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1, 1);
						if (Hit.Normal.Z >= UCONST_MINFLOORZ)
							landed = 1;
					}
				}
			}
		}
		ticks++;
		if ( (Region.ZoneNumber == 0) || (ticks > 35) )
		{
			GetLevel()->FarMoveActor(this, OriginalPos, 1, 1); //move actor back to starting point
			landed = 1;
		}
	}

	AScout *S = Cast<AScout>(this);
	if ( S )
		S->MaxLandingVelocity = ::Max(S->MaxLandingVelocity, -1.f * testVel.Z);
	FVector Landing = Location;
	if (!movePawn)
		GetLevel()->FarMoveActor(this, OriginalPos, 1, 1); //move actor back to starting point

	if ( Landing != OriginalPos )
		return TESTMOVE_Moved;
	else
		return TESTMOVE_Stopped;
}

ETestMoveResult APawn::FindJumpUp(FVector Direction)
{
	FCheckResult Hit(1.f);
	FVector StartLocation = Location;
	GetLevel()->MoveActor(this, FVector(0,0,MaxJumpHeight - MaxStepHeight), Rotation, Hit, 1, 1);
	ETestMoveResult success = walkMove(Direction, Hit, NULL, MINMOVETHRESHOLD);

	StartLocation.Z = Location.Z;
	if ( success != TESTMOVE_Stopped )
	{
		GetLevel()->MoveActor(this, FVector(0,0,-1.f * MaxJumpHeight), Rotation, Hit, 1, 1);
		// verify walkmove didn't just step down
		StartLocation.Z = Location.Z;
		if ((StartLocation - Location).SizeSquared() < MINMOVETHRESHOLD * MINMOVETHRESHOLD)
			return TESTMOVE_Stopped;
	}
	else
		GetLevel()->FarMoveActor(this, StartLocation, 1, 1);

	return success;

}

FVector AActor::SuggestFallVelocity(FVector Dest, FVector Start, FLOAT XYSpeed, FLOAT BaseZ, FLOAT JumpZ, FLOAT MaxXYSpeed)
{
	FVector SuggestedVelocity = Dest - Start;
	FLOAT DistZ = SuggestedVelocity.Z;
	SuggestedVelocity.Z = 0.f;
	FLOAT XYDist = SuggestedVelocity.Size();
	if ( (XYDist == 0.f) || (XYSpeed == 0.f) )
		return FVector(0.f,0.f,::Max(BaseZ,JumpZ));
	SuggestedVelocity = SuggestedVelocity/XYDist;

	// check for negative gravity
	if ( PhysicsVolume->Gravity.Z >= 0 )
		return (SuggestedVelocity * XYSpeed);

	FLOAT GravityZ = PhysicsVolume->Gravity.Z;

	//determine how long I might be in the air
	FLOAT ReachTime = XYDist/XYSpeed; 	

	// reduce time in low grav if above dest
	if ( (DistZ < 0.f) && (PhysicsVolume->Gravity.Z > ((APhysicsVolume *)(PhysicsVolume->GetClass()->GetDefaultActor()))->Gravity.Z) )
	{
		// reduce by time to fall DistZ
		ReachTime = ReachTime - appSqrt(DistZ/GravityZ);
	}

	// calculate starting Z velocity so end at dest Z position
	SuggestedVelocity.Z = DistZ/ReachTime - GravityZ * ReachTime;

	if ( (DistZ > 0.f) && (PhysicsVolume->Gravity.Z <= ((APhysicsVolume *)(PhysicsVolume->GetClass()->GetDefaultActor()))->Gravity.Z) )
		SuggestedVelocity.Z += 50.f;

	if ( SuggestedVelocity.Z < BaseZ )
	{
		// reduce XYSpeed
		// solve quadratic for ReachTime
		ReachTime = (-1.f * BaseZ + appSqrt(BaseZ * BaseZ + 4.f * GravityZ * DistZ))/(2.f * GravityZ);
		ReachTime = ::Max(ReachTime, 0.05f);
		XYSpeed = Min(MaxXYSpeed,XYDist/ReachTime);
		SuggestedVelocity.Z = BaseZ;
	}
	else if ( SuggestedVelocity.Z > BaseZ + JumpZ )
	{
		XYSpeed *= (BaseZ + JumpZ)/SuggestedVelocity.Z;
		SuggestedVelocity.Z = BaseZ + JumpZ;
	}

	SuggestedVelocity.X *= XYSpeed;
	SuggestedVelocity.Y *= XYSpeed;

	return SuggestedVelocity;
}

/* SuggestJumpVelocity()
returns a recommended Jump velocity vector, given a desired speed in the XY direction,
a minimum Z direction velocity, and a maximum Z direction jump velocity (JumpZ) to reach
the Dest point
*/
FVector APawn::SuggestJumpVelocity(FVector Dest, FLOAT XYSpeed, FLOAT BaseZ)
{
	if ( !Level->bBegunPlay )
		return SuggestFallVelocity(Dest, Location, XYSpeed, BaseZ, JumpZ, GroundSpeed);

	FLOAT GravityZ = PhysicsVolume->Gravity.Z;

	FVector SuggestedVelocity = Dest - Location;
	FLOAT DistZ = SuggestedVelocity.Z;
	SuggestedVelocity.Z = 0.f;
	FLOAT XYDist = SuggestedVelocity.Size();
	if ( XYDist == 0.f )
		return FVector(0.f,0.f,::Max(BaseZ,JumpZ));
	SuggestedVelocity = SuggestedVelocity/XYDist;

	// check for negative gravity
	if ( PhysicsVolume->Gravity.Z >= 0 )
	{
		SuggestedVelocity *= GroundSpeed;
		return SuggestedVelocity;
	}

	//FIXME - remove XYSpeed parameter
	// start with Z of either BaseZ or BaseZ+JumpZ
	FLOAT MinReachTime = XYDist/GroundSpeed;
	FLOAT SuggestedZ = DistZ/MinReachTime - GravityZ * MinReachTime;
	if ( SuggestedZ > BaseZ )
	{
		if ( SuggestedZ > BaseZ + 0.5f * JumpZ )
			SuggestedZ = BaseZ + JumpZ;
		else
			SuggestedZ = BaseZ + 0.5f * JumpZ;
	}
	else
		SuggestedZ = BaseZ;

	FLOAT X = SuggestedZ * SuggestedZ + 4.f * DistZ * GravityZ;
	if ( X >= 0.f )
	{
		FLOAT ReachTime = (-1.f * SuggestedZ - appSqrt(X))/(2.f * GravityZ);
		XYSpeed = ::Min(GroundSpeed, XYDist/ReachTime);
	}
	SuggestedVelocity *= XYSpeed;
	SuggestedVelocity.Z = SuggestedZ;
	return SuggestedVelocity;

}

/* Find best jump from current position toward destination.  Assumes that there is no immediate
barrier.  Sets Landing to the expected Landing, and moves actor if moveActor is set
*/
ETestMoveResult APawn::FindBestJump(FVector Dest)
{
	FVector realLocation = Location;

	//debugf("Jump best to %f %f %f from %f %f %f",Dest.X,Dest.Y,Dest.Z,Location.X,Location.Y, Location.Z);
	FVector vel = SuggestJumpVelocity(Dest, GroundSpeed, 0.f);

	// Now imagine jump
	ETestMoveResult success = jumpLanding(vel, 1);
	if ( success != TESTMOVE_Stopped )
	{
		if ( HurtByVolume(this) )
			success = TESTMOVE_Stopped;
		else if (!bCanSwim && PhysicsVolume->bWaterVolume )
			success = TESTMOVE_Stopped;
		else
		{
			FVector olddist = Dest - realLocation;
			FVector dist = Dest - Location;
			FLOAT netchange = olddist.Size2D();
			netchange -= dist.Size2D();
			if ( netchange > 0.f )
				success = TESTMOVE_Moved;
			else
				success = TESTMOVE_Stopped;
		}
	}
	//debugf("New Loc %f %f %f success %d",Location.X, Location.Y, Location.Z, success);
	// FIXME? - if failed, imagine with no jumpZ (step out first)
	return success;
}

ETestMoveResult APawn::HitGoal(AActor *GoalActor)
{
	if ( GoalActor->IsA(ANavigationPoint::StaticClass()) )
		return TESTMOVE_Stopped;

	return TESTMOVE_HitGoal;
}

/* walkMove()
- returns 1 if move happened, zero if it didn't because of barrier, and -1
if it didn't because of ledge
Move direction must not be adjusted.
*/
ETestMoveResult APawn::walkMove(FVector Delta, FCheckResult& Hit, AActor* GoalActor, FLOAT threshold)
{
	FVector StartLocation = Location;
	Delta.Z = 0.f;
	//-------------------------------------------------------------------------------------------
	//Perform the move
	FVector GravDir = FVector(0.f,0.f,-1.f);
	if ( PhysicsVolume->Gravity.Z > 0.f )
		GravDir.Z = 1.f;
	FVector Down = GravDir * MaxStepHeight;

	GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1, 1);
	if ( GoalActor && (Hit.Actor == GoalActor) )
		return HitGoal(GoalActor);
	FVector StopLocation = Location;
	if(Hit.Time < 1.f) //try to step up
	{
		Delta = Delta * (1.f - Hit.Time);
		GetLevel()->MoveActor(this, -1.f * Down, Rotation, Hit, 1, 1);
		GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1, 1);
		if ( GoalActor && (Hit.Actor == GoalActor) )
			return HitGoal(GoalActor);
		GetLevel()->MoveActor(this, Down, Rotation, Hit, 1, 1);
		if ( (Hit.Time < 1.f) && (Hit.Normal.Z < UCONST_MINFLOORZ) )
		{
			//Want only good floors, else undo move
			GetLevel()->FarMoveActor(this, StopLocation, 1, 1);
			return TESTMOVE_Stopped;
		}
	}

	//drop to floor
	FVector Loc = Location;
	Down = GravDir * (MaxStepHeight + 2.f);
	GetLevel()->MoveActor(this, Down, Rotation, Hit, 1, 1);
	if ( (Hit.Time == 1.f) || (Hit.Normal.Z < UCONST_MINFLOORZ) ) //then falling
	{
		GetLevel()->FarMoveActor(this, Loc, 1, 1);
		return TESTMOVE_Fell;
	}
	if ( GoalActor && (Hit.Actor == GoalActor) )
		return HitGoal(GoalActor);

	//check if move successful
	if ((Location - StartLocation).SizeSquared() < threshold * threshold)
		return TESTMOVE_Stopped;

	return TESTMOVE_Moved;
}

ETestMoveResult APawn::flyMove(FVector Delta, AActor* GoalActor, FLOAT threshold)
{
	FVector StartLocation = Location;
	FVector Down = FVector(0,0,-1) * MaxStepHeight;
	FVector Up = -1 * Down;
	FCheckResult Hit(1.f);

	GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1, 1);
	if ( GoalActor && (Hit.Actor == GoalActor) )
		return HitGoal(GoalActor);
	if (Hit.Time < 1.f) //try to step up
	{
		Delta = Delta * (1.f - Hit.Time);
		GetLevel()->MoveActor(this, Up, Rotation, Hit, 1, 1);
		GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1, 1);
		if ( GoalActor && (Hit.Actor == GoalActor) )
			return HitGoal(GoalActor);
	}

	if ((Location - StartLocation).SizeSquared() < threshold * threshold)
		return TESTMOVE_Stopped;

	return TESTMOVE_Moved;
}

ETestMoveResult APawn::swimMove(FVector Delta, AActor* GoalActor, FLOAT threshold)
{
	FVector StartLocation = Location;
	FVector Down = FVector(0,0,-1) * MaxStepHeight;
	FVector Up = -1 * Down;
	FCheckResult Hit(1.f);

	GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1, 1);
	if ( GoalActor && (Hit.Actor == GoalActor) )
		return HitGoal(GoalActor);
	if ( !PhysicsVolume->bWaterVolume )
	{
		FVector End = findWaterLine(StartLocation, Location);
		if (End != Location)
			GetLevel()->MoveActor(this, End - Location, Rotation, Hit, 1, 1);
		return TESTMOVE_Stopped;
	}
	else if (Hit.Time < 1.f) //try to step up
	{
		Delta = Delta * (1.f - Hit.Time);
		GetLevel()->MoveActor(this, Up, Rotation, Hit, 1, 1);
		GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1, 1);
		if ( GoalActor && (Hit.Actor == GoalActor) )
			return HitGoal(GoalActor); //bumped into goal
	}

	if ((Location - StartLocation).SizeSquared() < threshold * threshold)
		return TESTMOVE_Stopped;

	return TESTMOVE_Moved;
}

/* TryJumpUp()
Check if could jump up over obstruction
*/
UBOOL APawn::TryJumpUp(FVector Dir, DWORD TraceFlags)
{
	FVector Out = 14.f * Dir;
	FCheckResult Hit(1.f);
	FVector Up = FVector(0.f,0.f,MaxJumpHeight);
	GetLevel()->SingleLineCheck(Hit, this, Location + Up, Location, TRACE_World, GetCylinderExtent());
	FLOAT FirstHit = Hit.Time;
	if ( FirstHit > 0.5f )
	{
		GetLevel()->SingleLineCheck(Hit, this, Location + Up * Hit.Time + Out, Location + Up * Hit.Time, TraceFlags, GetCylinderExtent());
		return (Hit.Time == 1.f);
	}
	return false;
}

/* PickWallAdjust()
Check if could jump up over obstruction (only if there is a knee height obstruction)
If so, start jump, and return current destination
Else, try to step around - return a destination 90 degrees right or left depending on traces
out and floor checks
*/
UBOOL APawn::PickWallAdjust(FVector WallHitNormal, AActor* HitActor)
{
	if ( (Physics == PHYS_Falling) || !Controller )
		return false;

	if ( (Physics == PHYS_Flying) || (Physics == PHYS_Swimming) )
		return Pick3DWallAdjust(WallHitNormal, HitActor);

	DWORD TraceFlags = TRACE_World | TRACE_StopAtFirstHit;
	if ( HitActor && !HitActor->bWorldGeometry )
		TraceFlags = TRACE_AllBlocking | TRACE_StopAtFirstHit;

	// first pick likely dir with traces, then check with testmove
	FCheckResult Hit(1.f);
	FVector ViewPoint = Location + FVector(0.f,0.f,BaseEyeHeight);
	FVector Dir = Controller->Destination - Location;
	FLOAT zdiff = Dir.Z;
	Dir.Z = 0.f;
	FLOAT AdjustDist = 1.5f * CylinderComponent->CollisionRadius + 16.f;
	AActor *MoveTarget = ( Controller->MoveTarget ? Controller->MoveTarget->AssociatedLevelGeometry() : NULL );

	if ( (zdiff < CylinderComponent->CollisionHeight) && ((Dir | Dir) - CylinderComponent->CollisionRadius * CylinderComponent->CollisionRadius < 0) )
		return false;
	FLOAT Dist = Dir.Size();
	if ( Dist == 0.f )
		return false;
	Dir = Dir/Dist;
	GetLevel()->SingleLineCheck( Hit, this, Controller->Destination, ViewPoint, TraceFlags );
	if ( Hit.Actor && (Hit.Actor != MoveTarget) )
		AdjustDist += CylinderComponent->CollisionRadius;

	//look left and right
	FVector Left = FVector(Dir.Y, -1 * Dir.X, 0);
	INT bCheckRight = 0;
	FVector CheckLeft = Left * 1.4f * CylinderComponent->CollisionRadius;
	GetLevel()->SingleLineCheck(Hit, this, Controller->Destination, ViewPoint + CheckLeft, TraceFlags); 
	if ( Hit.Actor && (Hit.Actor != MoveTarget) ) //try right
	{
		bCheckRight = 1;
		Left *= -1;
		CheckLeft *= -1;
		GetLevel()->SingleLineCheck(Hit, this, Controller->Destination, ViewPoint + CheckLeft, TraceFlags); 
	}
	if ( Hit.Actor && (Hit.Actor != MoveTarget) ) //neither side has visibility
	{
		return false;
	}

	if ( (Physics == PHYS_Walking) && bCanJump && TryJumpUp(Dir, TraceFlags) )
	{
		Dir = Controller->Destination - Location;
		Dir.Z = 0.f;
		Dir = Dir.SafeNormal();
		Velocity = GroundSpeed * Dir;
		Acceleration = AccelRate * Dir;
		Velocity.Z = JumpZ;
		bNoJumpAdjust = true; // don't let script adjust this jump again
		Controller->bJumpOverWall = true;
		Controller->bNotifyApex = true;
		setPhysics(PHYS_Falling);
		return 1;
	}

	//try step left or right
	FVector Out = 14.f * Dir;
	Left *= AdjustDist;
	GetLevel()->SingleLineCheck(Hit, this, Location + Left, Location, TraceFlags, GetCylinderExtent());
	if (Hit.Time == 1.f)
	{
		GetLevel()->SingleLineCheck(Hit, this, Location + Left + Out, Location + Left, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			Controller->SetAdjustLocation(Location + Left);
			return true;
		}
	}
	
	if ( !bCheckRight ) // if didn't already try right, now try it
	{
		CheckLeft *= -1;
		GetLevel()->SingleLineCheck(Hit, this, Controller->Destination, ViewPoint + CheckLeft, TraceFlags); 
		if ( Hit.Time < 1.f )
			return false;
		Left *= -1;
		GetLevel()->SingleLineCheck(Hit, this, Location + Left, Location, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			GetLevel()->SingleLineCheck(Hit, this, Location + Left + Out, Location + Left, TraceFlags, GetCylinderExtent());
			if (Hit.Time == 1.f)
			{
				Controller->SetAdjustLocation(Location + Left);
				return true;
			}
		}
	}
	return false;
}

/* Pick3DWallAdjust()
pick wall adjust when swimming or flying
*/
UBOOL APawn::Pick3DWallAdjust(FVector WallHitNormal, AActor* HitActor)
{
	DWORD TraceFlags = TRACE_World | TRACE_StopAtFirstHit;
	if ( HitActor && !HitActor->bWorldGeometry )
		TraceFlags = TRACE_AllBlocking | TRACE_StopAtFirstHit;

	// first pick likely dir with traces, then check with testmove
	FCheckResult Hit(1.f);
	FVector ViewPoint = Location + FVector(0.f,0.f,BaseEyeHeight);
	FVector Dir = Controller->Destination - Location;
	FLOAT zdiff = Dir.Z;
	Dir.Z = 0.f;
	FLOAT AdjustDist = 1.5f * CylinderComponent->CollisionRadius + 16.f;
	AActor *MoveTarget = ( Controller->MoveTarget ? Controller->MoveTarget->AssociatedLevelGeometry() : NULL );

	INT bCheckUp = 0;
	if ( zdiff < CylinderComponent->CollisionHeight )
	{
		if ( (Dir | Dir) - CylinderComponent->CollisionRadius * CylinderComponent->CollisionRadius < 0 )
			return false;
		if ( Dir.SizeSquared() < 4 * CylinderComponent->CollisionHeight * CylinderComponent->CollisionHeight )
		{
			FVector Up = FVector(0,0,2.f*CylinderComponent->CollisionHeight);
			bCheckUp = 1;
			if ( Location.Z > Controller->Destination.Z )
			{
				bCheckUp = -1;
				Up *= -1;
			}
			GetLevel()->SingleLineCheck(Hit, this, Location + Up, Location, TraceFlags, GetCylinderExtent());
			if (Hit.Time == 1.f)
			{
				FVector ShortDir = Dir.SafeNormal();
				ShortDir *= CylinderComponent->CollisionRadius;
				GetLevel()->SingleLineCheck(Hit, this, Location + Up + ShortDir, Location + Up, TraceFlags, GetCylinderExtent());
				if (Hit.Time == 1.f)
				{
					Controller->SetAdjustLocation(Location + Up);
					return true;
				}
			}
		}
	}

	FLOAT Dist = Dir.Size();
	if ( Dist == 0.f )
		return false;
	Dir = Dir/Dist;
	GetLevel()->SingleLineCheck( Hit, this, Controller->Destination, ViewPoint, TraceFlags );
	if ( (Hit.Actor != MoveTarget) && (zdiff > 0) )
	{
		FVector Up = FVector(0,0, 2.f*CylinderComponent->CollisionHeight);
		GetLevel()->SingleLineCheck(Hit, this, Location + 2 * Up, Location, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			Controller->SetAdjustLocation(Location + Up);
			return true;
		}
	}

	//look left and right
	FVector Left = FVector(Dir.Y, -1 * Dir.X, 0);
	INT bCheckRight = 0;
	FVector CheckLeft = Left * 1.4f * CylinderComponent->CollisionRadius;
	GetLevel()->SingleLineCheck(Hit, this, Controller->Destination, ViewPoint + CheckLeft, TraceFlags); 
	if ( Hit.Actor != MoveTarget ) //try right
	{
		bCheckRight = 1;
		Left *= -1;
		CheckLeft *= -1;
		GetLevel()->SingleLineCheck(Hit, this, Controller->Destination, ViewPoint + CheckLeft, TraceFlags); 
	}

	if ( Hit.Actor != MoveTarget ) //neither side has visibility
		return false;

	FVector Out = 14.f * Dir;

	//try step left or right
	Left *= AdjustDist;
	GetLevel()->SingleLineCheck(Hit, this, Location + Left, Location, TraceFlags, GetCylinderExtent());
	if (Hit.Time == 1.f)
	{
		GetLevel()->SingleLineCheck(Hit, this, Location + Left + Out, Location + Left, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			Controller->SetAdjustLocation(Location + Left);
			return true;
		}
	}
	
	if ( !bCheckRight ) // if didn't already try right, now try it
	{
		CheckLeft *= -1;
		GetLevel()->SingleLineCheck(Hit, this, Controller->Destination, ViewPoint + CheckLeft, TraceFlags); 
		if ( Hit.Time < 1.f )
			return false;
		Left *= -1;
		GetLevel()->SingleLineCheck(Hit, this, Location + Left, Location, TraceFlags, GetCylinderExtent());
		if (Hit.Time == 1.f)
		{
			GetLevel()->SingleLineCheck(Hit, this, Location + Left + Out, Location + Left, TraceFlags, GetCylinderExtent());
			if (Hit.Time == 1.f)
			{
				Controller->SetAdjustLocation(Location + Left);
				return true;
			}
		}
	}

	//try adjust up or down if swimming or flying
	FVector Up = FVector(0,0,2.5f*CylinderComponent->CollisionHeight);

	if ( bCheckUp != 1 )
	{
		GetLevel()->SingleLineCheck(Hit, this, Location + Up, Location, TraceFlags, GetCylinderExtent());
		if ( Hit.Time > 0.7f )
		{
			GetLevel()->SingleLineCheck(Hit, this, Location + Up + Out, Location + Up, TraceFlags, GetCylinderExtent());
			if ( (Hit.Time == 1.f) || (Hit.Normal.Z > 0.7f) )
			{
				Controller->SetAdjustLocation(Location + Up);
				return true;
			}
		}
	}

	if ( bCheckUp != -1 )
	{
		Up *= -1; //try adjusting down
		GetLevel()->SingleLineCheck(Hit, this, Location + Up, Location, TraceFlags, GetCylinderExtent());
		if ( Hit.Time > 0.7f )
		{
			GetLevel()->SingleLineCheck(Hit, this, Location + Up + Out, Location + Up, TraceFlags, GetCylinderExtent());
			if (Hit.Time == 1.f)
			{
				Controller->SetAdjustLocation(Location + Up);
				return true;
			}
		}
	}

	return false;
}

int APawn::calcMoveFlags()
{
	return ( R_FORCED + bCanWalk * R_WALK + bCanFly * R_FLY + bCanSwim * R_SWIM + bJumpCapable * R_JUMP
			+ Controller->bCanOpenDoors * R_DOOR + Controller->bCanDoSpecial * R_SPECIAL
			+ Controller->bIsPlayer * R_PLAYERONLY + bCanClimbLadders * R_LADDER);
}

/*-----------------------------------------------------------------------------
	Networking functions.
-----------------------------------------------------------------------------*/

FLOAT APawn::GetNetPriority( FVector& ViewPos, FVector& ViewDir, AActor* Recent, FLOAT Time, FLOAT Lag )
{
	if
	(	Controller
	&&	Controller->bIsPlayer
	&&	Recent
	&&	Physics==PHYS_Walking
	&&	!Recent->bNetOwner
	&&  Recent->bHidden==bHidden )
	{
		FLOAT LocationError = ((Recent->Location+(Time+0.5f*Lag)*Recent->Velocity) - (Location+0.5f*Lag*Velocity)).SizeSquared();
		FVector Dir = Location - ViewPos;
		if ( (ViewDir | Dir) < 0.f )
			LocationError *= 0.2f;

		// Note: Lags and surges in position occur for other players because of
		// ServerMove/ClientAdjustPosition temporal wobble.
		Time = Time*0.5f + 2.f*LocationError / GroundSpeed;
	}
	return NetPriority * Time;
}

UBOOL APawn::SharingVehicleWith(APawn *P)
{
	return ( P && ((Base == P) || (P->Base == this)) );
}
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

