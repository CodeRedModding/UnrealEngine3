/*=============================================================================
	UnVehicle.cpp: Vehicle AI implementation

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnPath.h"
#include "EngineAIClasses.h"

void AVehicle::setMoveTimer(FVector MoveDir)
{
	if ( !Controller )
		return;

	Super::setMoveTimer(MoveDir);
	Controller->MoveTimer += 2.f;
	if ( (MoveDir | Rotation.Vector()) < 0.f )
		Controller->MoveTimer += TurnTime;
}

UBOOL AVehicle::IsStuck()
{
	if ( WorldInfo->TimeSeconds - StuckTime < 1.f )
	{
		return TRUE;
	}
	if (Velocity.SizeSquared() > SLOWVELOCITYSQUARED || WorldInfo->TimeSeconds - ThrottleTime < 1.f)
	{
		if (Steering == 0.f || Throttle != 0.f || WorldInfo->TimeSeconds - OnlySteeringStartTime < 10.f)
		{
			StuckCount = 0;
			return FALSE;
		}
		else if (WorldInfo->TimeSeconds - OnlySteeringStartTime < 10.f * FLOAT(StuckCount))
		{
			return FALSE;
		}
	}

	StuckCount++;
	StuckTime = WorldInfo->TimeSeconds;
	return TRUE;
}

UBOOL AVehicle::IsGlider()
{
	return (Physics == PHYS_RigidBody) ? (!bCanStrafe && bFollowLookDir) : Super::IsGlider();
}

UBOOL AVehicle::moveToward(const FVector &Dest, AActor *GoalActor )
{
	if ( !Controller )
		return false;

	UBOOL bFlyingDown = false;
	FVector AdjustedDest = Dest;

	FLOAT GoalRadius = 0.f;
	FLOAT GoalHeight = 0.f;
	if ( GoalActor )
		GoalActor->GetBoundingCylinder(GoalRadius, GoalHeight);

	if ( GoalActor && Controller->CurrentPath )
	{
		if ( Cast<AVolumePathNode>(GoalActor) )
		{
			if ( !Cast<AVolumePathNode>(Controller->CurrentPath->Start) )
			{
				// if not inside flying pathnode, just move straight up toward it if it's above me
				FVector Dir = GoalActor->Location - Location;
				if ( Dir.Z > GoalHeight )
				{
					Dir.Z = 0.f;
					if ( Dir.SizeSquared() < GoalRadius * GoalRadius )
					{
						AdjustedDest = Location;
						AdjustedDest.Z = GoalActor->Location.Z;
					}
				}
			}
		}
		else if ( Cast<AVolumePathNode>(Controller->CurrentPath->Start) )
		{
			// if inside flying pathnode, just move straight across toward it if it's below me
			FVector Dir = Controller->CurrentPath->Start->Location - Dest;
			if ( Dir.Z > Controller->CurrentPath->Start->CylinderComponent->CollisionHeight )
			{
				Dir.Z = 0.f;
				if ( (Dir.SizeSquared() < Controller->CurrentPath->Start->CylinderComponent->CollisionRadius * Controller->CurrentPath->Start->CylinderComponent->CollisionRadius)
					&& (Dir.SizeSquared() > ::Max(40000.f ,GoalRadius * GoalRadius)) )
				{
					AdjustedDest = Dest;
					if ( Location.Z < Controller->CurrentPath->Start->Location.Z )
						AdjustedDest.Z = Location.Z - 0.7f*CylinderComponent->CollisionHeight;
					FVector TestLocation = Location;
					TestLocation.Z = Dest.Z;
					if ( ReachedDestination(TestLocation, Dest, GoalActor) )
						AdjustedDest = Dest;
				}
			}
			bFlyingDown = true;
		}
		// check if on next path already - FIXME - also should work when no currentpath (moving to initial path on network)
		else if (Controller->NextRoutePath != NULL && Controller->NextRoutePath->Start != NULL && Controller->NextRoutePath->End.Nav() != NULL)
		{
			FVector NextPathDir = Controller->NextRoutePath->End->Location - Controller->NextRoutePath->Start->Location;
			// see if location is between start and end
			if ( (((Location - Controller->NextRoutePath->Start->Location) | NextPathDir) > 0.f)
				&& (((Location - Controller->NextRoutePath->End->Location) | NextPathDir) < 0.f) )
			{
				// check distance to line
				NextPathDir = NextPathDir.SafeNormal();
				FVector Start = Controller->NextRoutePath->Start->Location;
				FVector ClosestPoint = Start + (NextPathDir | (Location - Start)) * NextPathDir;
				FVector LineDir = Location - ClosestPoint;
				if ( LineDir.SizeSquared() < Controller->NextRoutePath->CollisionRadius*Controller->NextRoutePath->CollisionRadius )
				{
					if ( Controller->Focus == Controller->MoveTarget )
					{
						Controller->Focus = Controller->NextRoutePath->End;
					}
					if (Controller->RouteCache.Num() > 0 && Controller->RouteCache(0) == GoalActor)
					{
						Controller->RouteCache_RemoveIndex( 0 );
					}
					Controller->MoveTarget = Controller->NextRoutePath->End;
					GoalActor = Controller->MoveTarget;
					AdjustedDest = GoalActor->Location;
					Controller->CurrentPath = Controller->NextRoutePath;
					Controller->NextRoutePath = Controller->GetNextRoutePath((ANavigationPoint*)GoalActor);
					setMoveTimer(AdjustedDest - Location); 
				}
				else if ( (LineDir.Z > 0.f) && (LineDir.Z < VEHICLEADJUSTDIST) && (WorldInfo->TimeSeconds - AIMoveCheckTime > 0.2f) )
				{
					LineDir.Z = 0.f;
					if ( LineDir.SizeSquared() < Controller->NextRoutePath->CollisionRadius*Controller->NextRoutePath->CollisionRadius )
					{
						FCheckResult Hit(1.f);
						GWorld->SingleLineCheck(Hit, this, ClosestPoint, Location, TRACE_World, GetCylinderExtent());
						if ( !Hit.Actor )
						{
							if ( Controller->Focus == Controller->MoveTarget )
							{
								Controller->Focus = Controller->NextRoutePath->End;
							}
							if (Controller->RouteCache.Num() > 0 && Controller->RouteCache(0) == GoalActor)
							{
								Controller->RouteCache_RemoveIndex( 0 );
							}
							Controller->MoveTarget = Controller->NextRoutePath->End;
							GoalActor = Controller->MoveTarget;
							AdjustedDest = GoalActor->Location;
							Controller->CurrentPath = Controller->NextRoutePath;
							Controller->NextRoutePath = Controller->GetNextRoutePath((ANavigationPoint*)GoalActor);
							setMoveTimer(AdjustedDest - Location); 
						}
					}
				}
			}
		}
	}

	if ( (Throttle == 0.f) || (Velocity.SizeSquared() > 10000.f) )
		ThrottleTime = WorldInfo->TimeSeconds;

	VehicleMovingTime = WorldInfo->TimeSeconds;

	FVector Direction = AdjustedDest - Location;
	FLOAT ZDiff = Direction.Z;

	Direction.Z = 0.f;
	FLOAT Distance = Direction.Size();
	FCheckResult Hit(1.f);

	if ( ReachedDestination(Location, Dest, GoalActor) )
	{
		if (Controller->RouteGoal == NULL || GoalActor == Controller->RouteGoal)
		{
			Throttle = 0.f;
		}

		// if Pawn just reached a navigation point, set a new anchor
		ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
		if (Nav != NULL)
		{
			SetAnchor(Nav);
		}
		else if (bScriptedRise && GoalActor != NULL)
		{
			APawn *P = GoalActor->GetAPawn();
			if (P != NULL && P == Controller->Enemy)
			{
				Rise = -1;
			}
		}
		return true;
	}
	else if ( !bCanFly 
			&& (Distance < CylinderComponent->CollisionRadius)
			&& (!GoalActor || ((ZDiff > CylinderComponent->CollisionHeight + 2.f * MaxStepHeight)
			&& !GWorld->SingleLineCheck(Hit, this, Dest, Location, TRACE_World))) )
	{
		// failed - below target
		return true;
	}
	else
	{
		if ( bCanFly )
		{
			if ( AdjustFlight(ZDiff, bFlyingDown,Distance,GoalActor) )
						return true;
		}
		else if ( bDuckObstacles )
		{
			if ( WorldInfo->TimeSeconds - AIMoveCheckTime > 0.2f )
			{
				AIMoveCheckTime = WorldInfo->TimeSeconds;

				GWorld->SingleLineCheck(Hit, this, AdjustedDest, Location, TRACE_World);
				if ( Hit.Actor && (Hit.Actor != GoalActor) )
				{
					Rise = -1.f;
					Throttle *= 0.1f;
				}
				else if (WorldInfo->TimeSeconds - StuckTime > 5.f)
				{
					Rise = 0.f;
				}
			}
			else if (WorldInfo->TimeSeconds - StuckTime > 5.f)
			{
				 Rise = 0.f;
			}
		}
		else if ( !bScriptedRise )
			Rise = 0.f;	
		else if ( WorldInfo->TimeSeconds - AIMoveCheckTime > 0.2f )
		{
			AIMoveCheckTime = WorldInfo->TimeSeconds;

			// look for obstacles periodically, and rise over them
			if ( Rise == 0.f )
			{
				FVector DirNormal = FVector(Direction.X, Direction.Y, ZDiff).SafeNormal();
				GWorld->SingleLineCheck(Hit, this, Location + Min<FLOAT>(Distance, 500.f) * DirNormal, Location, TRACE_World|TRACE_Pawns);
				if ( Hit.Actor )
				{
					if ( Hit.Actor->bWorldGeometry && (Distance > 300.f) )
					{
						Rise = 1.f;
					}
					else if ( Hit.Actor->GetAPawn() )
					{
						// rise over vehicles and teammates, but not enemy foot soldiers
						if ( Cast<AVehicle>(Hit.Actor) )
						{
							Rise = 1.f;
						}
						else if ( PlayerReplicationInfo && PlayerReplicationInfo->Team && Hit.Actor->GetAPawn()->PlayerReplicationInfo
								&& (Hit.Actor->GetAPawn()->PlayerReplicationInfo->Team == PlayerReplicationInfo->Team) )
						{
							Rise = 1.f;
						}
					}
				}
			}
		}
		SteerVehicle(Direction);
	}

	if ( Controller->MoveTarget && Controller->MoveTarget->GetAPawn() )
	{
		if (Distance < CylinderComponent->CollisionRadius + Controller->MoveTarget->GetAPawn()->CylinderComponent->CollisionRadius + 0.8f * MeleeRange)
			return true;
		return false;
	}

	if ( IsStuck() )
	{
		Controller->FailMove();
		StuckTime = 0.f;
	}

	AdjustThrottle( Distance );

	return false;
}

void AVehicle::AdjustThrottle( FLOAT Distance )
{
	FLOAT Speed = Velocity.Size(); 
	if (Speed > 0.f)
	{
		// if we need to make a big turn to get to our next path, slow down more when reaching the end of this one
		if ( !bCanStrafe && Throttle > 0.f && Distance < Speed && Controller->CurrentPath != NULL && Controller->NextRoutePath != NULL &&
			Controller->NextRoutePath->Start != NULL && *Controller->NextRoutePath->End != NULL )
		{
			Throttle *= Distance / Speed;
			Throttle *= (Controller->NextRoutePath->End->Location - Controller->NextRoutePath->Start->Location).SafeNormal() | Controller->CurrentPathDir;
		}
		else
		{
			Throttle *= ::Min(1.f, 2.f * Distance / Speed);
		}
	}

}

/* AdjustFlight()
ZDiff is Destination.Z - Location.Z
Distance is 2D distance
bFlyingDown=true means going from AVolumePathNode to NavigationPoint
*/
UBOOL AVehicle::AdjustFlight(FLOAT ZDiff, UBOOL bFlyingDown, FLOAT Distance, AActor* GoalActor)
{
	if ( ZDiff > -0.7f * CylinderComponent->CollisionHeight )
	{
		Rise = 1.f;
		if ( bFlyingDown && (Distance < 2.f*VEHICLEADJUSTDIST) )
		{
			ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
			if ( Nav && Nav->bMustBeReachable )
			{
				return true;
			}
		}					
	}
	else 
	{
		FLOAT MaxRiseForce = GetMaxRiseForce();
		if ( ZDiff < -1.f * CylinderComponent->CollisionHeight )
		{
			Rise = ::Clamp((ZDiff - Velocity.Z)/MaxRiseForce,-1.f,1.f);
			if ( bFlyingDown )
			{
				if ( Distance > ::Max(VEHICLEADJUSTDIST,Abs(ZDiff)) )
					Rise = ::Clamp((Distance - ::Max(VEHICLEADJUSTDIST,Abs(ZDiff)))/MaxRiseForce,-1.f,1.f);
				else if ( JumpOutCheck(GoalActor, Distance, ZDiff) )
					return true;
			}
		}
		else
		{
			if ( bFlyingDown && (ZDiff <  0.f) )
			{
				Rise = ::Clamp((ZDiff - Velocity.Z)/MaxRiseForce,-1.f,1.f);
			}
			else
			{
				Rise = ::Clamp(-1.f*Velocity.Z/MaxRiseForce,-1.f,1.f);
			}
		}
	}
	return false;
}

FLOAT AVehicle::GetMaxRiseForce()
{
	return 100.f;
}

/** 
JumpOutCheck()
AI may decide to jump out of vehicle if close enought to objective
Returns true if AI jumped out.
*/
UBOOL AVehicle::JumpOutCheck(AActor *GoalActor, FLOAT Distance, FLOAT ZDiff)
{
	return FALSE;
}				


void AVehicle::SteerVehicle(FVector Direction)
{
	Direction.Z = 0.f;
	if ( bFollowLookDir && (VehicleMovingTime == WorldInfo->TimeSeconds) )
	{
		// make vehicle correct if velocity is off course
		FVector DirCross = Direction ^ FVector(0.f,0.f,1.f);
		DirCross = DirCross.SafeNormal();
		FVector VelDir = (Velocity | DirCross) * DirCross;
		if ( (VelDir.SizeSquared() > 160000.f) && (VelDir.SizeSquared() < Direction.SizeSquared()) )
		{
			FLOAT Distance = Direction.Size();
			Direction = Direction - Distance * VelDir.SafeNormal(); 
		}
		else if ( VelDir.SizeSquared() > 10000.f )
			Direction = Direction - VelDir;	
	}
	FLOAT Distance = Direction.Size();
	if ( Distance != 0.f ) 
		Direction = Direction/Distance;
	FRotator FaceRot = Rotation;
	FaceRot.Pitch = 0;
	FVector Facing = FaceRot.Vector();
	FLOAT Dot = Facing | Direction;

	Throttle = 1.f;

	if ( bTurnInPlace )
	{
		FRotator ViewRot = Rotation;
		ViewRot.Pitch = 0;
		FLOAT ViewDot = Direction | ViewRot.Vector();
		if ( ViewDot < 0.9f )
		{
			if ( (ViewDot < -0.9f) && (Distance > 0.5f * CylinderComponent->CollisionRadius) && !bAvoidReversing )
				Throttle = -1.f;
			else
				Throttle = 0.f;
		}
	}

	if  (Dot > 0.995f)
	{
		if ( VehicleMovingTime < WorldInfo->TimeSeconds )
			Throttle = 0.f;
		Steering = 0.f;
		DesiredRotation.Yaw = Rotation.Yaw;
	}
	else if ( bFollowLookDir )
	{
		Throttle = Dot;
		if (bCanStrafe)
		{
			FVector Cross = Facing ^ FVector(0.f,0.f,1.f);
			Cross = Cross.SafeNormal();
			Steering = Cross | Direction;
		}
		else if (Dot < 0.9f && Distance < CylinderComponent->CollisionRadius * 2.0f)
		{
			// wait for vehicle to turn before continuing forward
			Throttle = bTurnInPlace ? 0.f : -1.f;
		}
	}
	else if ( !bTurnInPlace && (Dot < -0.7f) && (Distance < 500.f) && (Distance > 1.5f) )
	{
		Throttle = -1.f;
		FVector Cross = Facing ^ FVector(0.f,0.f,1.f);
		if ( (Cross | Direction) < 0.f )
			Steering = 1.f;
		else
			Steering = -1.f;
	}
	else
	{
		FVector Cross = Facing ^ FVector(0.f,0.f,1.f);
		Cross = Cross.SafeNormal();
		Steering = Clamp<FLOAT>((Cross | Direction) * 2.0f, -1.0f, 1.0f);

		if ( !bTurnInPlace && (Dot < ((OldThrottle == -1.f) ? 0.3f : 0.f)) )
		{
			Throttle = -1.f;
		}
		// steering direction is reversed when going backwards
		if (Throttle < 0.f)
		{
			Steering *= -1.f;
		}
		if ( IsStuck() )
		{
			if ( bScriptedRise )
				Rise = 1.f;
			else
			{
				// check if stuck
				Steering *= -1.f;
				Throttle *= -1.f;
			}
		}
		if ( bHasHandbrake )
		{
			Direction.Z = 0.f;
			Direction = Direction.SafeNormal();
			const FLOAT HandbrakeDot = Facing | Direction;
			if ( (HandbrakeDot < 0.9f) && (HandbrakeDot > 0.f) && (Velocity.SizeSquared() > 240000.f) )
			{
				FVector Velocity2D = Velocity;
				Velocity2D.Z = 0.f;
				FVector VelDir = Velocity2D.SafeNormal();
				// check if sliding
				if ( (VelDir | Facing) < 0.96f )
				{
					if ( (VelDir | Facing) > 0.f )
					{
						// if facing forward, steer into it, or throttle down
						if ( (VelDir | Facing) < 0.9f )
						{
							Steering = 0.f;
						}
						else
							Throttle *= 0.1f;
					}
					// if sliding, no handbrake
					Rise = 0.f;
				}
				else
				{
					Rise = 1.f;
				}
				if ( HandbrakeDot < 0.7f )
					Throttle = 0.f;
			}
			else
				Rise = 0.f;
		}
	}
	if (Steering != 0.f && Throttle == 0.f)
	{
		if (OldSteering == 0.f || OldThrottle != 0.f)
		{
			OnlySteeringStartTime = WorldInfo->TimeSeconds;
		}
		if (StuckCount == 1)
		{
			if (!IsStuck())
			{
				Steering *= -1.f;
			}
		}
		else if (StuckCount > 1)
		{
			Throttle = (StuckCount < 5) ? -1.f : 1.f; // try to get unstuck by using throttle
		}
	}
	OldSteering = Steering;
	OldThrottle = Throttle;
}

/* rotateToward()
rotate Actor toward a point.  Returns 1 if target rotation achieved.
(Set DesiredRotation, let physics do actual move)
*/
void AVehicle::rotateToward(FVector FocalPoint)
{
	// if we have no controller, or we don't need to rotate the Vehicle itself to track our Focus, do nothing
	if ( !Controller || bSeparateTurretFocus)
		return;

	if ( (Throttle == 0.f) || (Velocity.SizeSquared() > 10000.f) )
		ThrottleTime = WorldInfo->TimeSeconds;

	// Do not allow this if DesiredRotation is set by somebody else outside
	if ( IsDesiredRotationInUse() )
		return;
		
	FVector Direction = FocalPoint - Location - FVector(0,0,BaseEyeHeight);

	if ( bFollowLookDir )
	{
		Controller->Rotation = Direction.Rotation();
		DesiredRotation = Controller->Rotation;
		if ( VehicleMovingTime < WorldInfo->TimeSeconds )
			Throttle = bTurnInPlace ? 0.f : 1.f;
		return;
	}
	SteerVehicle(Direction.SafeNormal());
	if ( bTurnInPlace && (VehicleMovingTime < WorldInfo->TimeSeconds) )
	{
		Throttle = 0.f;
		if ( bFollowLookDir )
			Steering = 0.f;
		if ( bCanFly )
		{
		    if ( Velocity.Z < -1.f * VEHICLEADJUSTDIST )
			    Rise = 1.f;
		    else if ( Velocity.Z > 2.f*VEHICLEADJUSTDIST )
			    Rise = -1.f;
		    else
			    Rise = 0.f;
		}
	}
}

void AVehicle::performPhysics(FLOAT DeltaSeconds)
{
	if ( !bIgnoreStallZ && (Location.Z > WorldInfo->StallZ) )
	{
		// force vehicle down
		if ( Velocity.Z < -2.f * (Location.Z - WorldInfo->StallZ) )
		{
			Rise = ::Min(Rise, 0.f);
		}
		else
			Rise = -1.f;
	}

	Super::performPhysics(DeltaSeconds);
}

/** CheckDetour()
By default, vehicles don't check for detours (since they don't turn as easily)
*/
ANavigationPoint* AVehicle::CheckDetour(ANavigationPoint* BestDest, ANavigationPoint* Start, UBOOL bWeightDetours)
{
	return BestDest;
}

UBOOL AActor::BlockedByVehicle()
{
	if ( !bCollideActors )
		return FALSE;

	for ( INT i=0; i<Touching.Num(); i++ )
		if ( Touching(i) && Touching(i)->GetAVehicle() )
			return TRUE;

	return FALSE;
}

void AVehicle::MarkEndPoints(ANavigationPoint* EndAnchor, AActor* Goal, const FVector& GoalLocation)
{
	Super::MarkEndPoints(EndAnchor, Goal, GoalLocation);

	// for vehicles, also mark nearby paths
    UBOOL bEndPath = (Cast<ANavigationPoint>(Goal) != NULL);
	UBOOL bSkip = false;

	// check if already close
	for ( INT i=0; i<EndAnchor->PathList.Num(); i++ )
	{
		if ( EndAnchor->PathList(i)->End == Anchor )
		{
			bSkip = true;
			break;
		}
	}

	if ( !bSkip )
	{
		// mark nearby nodes also
		FCheckResult Hit(1.f);
		for ( INT i=0; i<EndAnchor->PathList.Num(); i++ )
		{
			if (EndAnchor->PathList(i)->End.Nav() != NULL)
			{
				UReachSpec *UpStream = EndAnchor->PathList(i)->End.Nav()->GetReachSpecTo(EndAnchor);
				if ( UpStream && !UpStream->IsProscribed() && ((UpStream->reachFlags & R_FLY) == 0) )
				{
					Hit.Actor = NULL;
					if ( !bEndPath )
						GWorld->SingleLineCheck( Hit, this,  EndAnchor->PathList(i)->End->Location, GoalLocation, TRACE_World|TRACE_StopAtAnyHit );
					if ( !Hit.Actor )
					{
						EndAnchor->PathList(i)->End.Nav()->bEndPoint = 1;
					}
				}
			}
		}
	}
}

/** SecondRouteAttempt()
Allows a second try at finding a route.  Not used by base pawn implementation.  Attempts to find a route for the dismounted driver
*/
FLOAT AVehicle::SecondRouteAttempt(ANavigationPoint* Anchor, ANavigationPoint* EndAnchor, NodeEvaluator NodeEval, FLOAT BestWeight, AActor *goal, const FVector& GoalLocation, FLOAT StartDist, FLOAT EndDist, INT MaxPathLength, INT SoftMaxNodes)
{
	if (bRetryPathfindingWithDriver && Driver != NULL)
	{
		// temporarily switch controller over to driver for path finding
		AController *OldController = Driver->Controller; // fixme - should always be NULL
		Controller->Pawn = Driver;
		Driver->Controller = Controller;
		Driver->Anchor = Anchor;

		// reset navigation network
		for ( ANavigationPoint *Nav=GWorld->GetFirstNavigationPoint(); Nav; Nav=Nav->nextNavigationPoint )
		{
			// reset only BestPathTo() temporaries instead of calling ClearForPathFinding() so we don't clobber transient endpoints, etc
			Nav->visitedWeight = UCONST_INFINITE_PATH_COST;
			Nav->nextOrdered = NULL;
			Nav->prevOrdered = NULL;
			Nav->previousPath = NULL;
			Nav->bAlreadyVisited = FALSE;
		}
		if (EndAnchor != NULL)
		{
			Controller->MarkEndPoints(EndAnchor, goal, GoalLocation);
		}
		Anchor->visitedWeight = appRound(StartDist);

		// have driver search for path
		ANavigationPoint* BestDest = Driver->BestPathTo(NodeEval, Anchor, &BestWeight, FALSE, MaxPathLength, SoftMaxNodes);

		// return controller to self
		Driver->Controller = OldController;
		Controller->Pawn = this;

		if ( BestDest )
		{
			// found a route for the driver - set up the route cache
			Controller->SetRouteCache(BestDest,StartDist,EndDist);
			ANavigationPoint *N = (Controller->RouteCache.Num() > 0) ? Controller->RouteCache(0) : NULL;
			if ( Anchor == N )
			{
				N = (Controller->RouteCache.Num() > 1) ? Controller->RouteCache(1) : NULL;
			}
			Controller->RouteCache_Empty(); // will get filled again by another SetRouteCache() call below
			if ( N )
			{
				UReachSpec *spec = Anchor->GetReachSpecTo(N);
				if ( spec != NULL && spec->supports(appTrunc(CylinderComponent->CollisionRadius), appTrunc(CylinderComponent->CollisionHeight), calcMoveFlags(), appTrunc(GetAIMaxFallSpeed())) &&
					spec->CostFor(this) < UCONST_BLOCKEDPATHCOST )
				{
					// if can drive along this part of the route, stay in vehicle
					Controller->SetRouteCache(BestDest,StartDist,EndDist);
					return BestWeight;
				}
			}
			// tell bot to get out, continue on foot
			Controller->SetRouteCache(BestDest,StartDist,EndDist);
			if ( eventContinueOnFoot() )
			{
				return BestWeight;
			}
		}
	}
	return 0.f;
}

UBOOL AVehicle::HasRelevantDriver()
{
	return Driver != NULL;
}

/**
 * @returns location to target when aiming at this vehicle
 */
FVector AVehicle::GetTargetLocation(AActor *RequestedBy, UBOOL bRequestAlternateLoc) const
{
	if ( Mesh )
	{
		return Mesh->LocalToWorld.GetOrigin() + TargetLocationAdjustment;
	}
	return CylinderComponent->LocalToWorld.GetOrigin() + TargetLocationAdjustment;
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
UBOOL AVehicle::IsRelevancyOwnerFor(AActor* ReplicatedActor, AActor* ActorOwner, AActor* ConnectionActor)
{
	return ( (ActorOwner == this) || ((Driver == ActorOwner) && (Driver != NULL)) );
}
