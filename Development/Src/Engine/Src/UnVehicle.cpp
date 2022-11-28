/*=============================================================================
	UnVehicle.cpp: Vehicle physics implementation

	Copyright 2000-2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 7/00
=============================================================================*/

#include "EnginePrivate.h"

int AVehicle::pointReachable(FVector aPoint, int bKnowVisible)
{
	return true;
}

int AVehicle::actorReachable(AActor *Other, UBOOL bKnowVisible, UBOOL bNoAnchorCheck)
{
	return true;
}

UBOOL AVehicle::moveToward(const FVector &Dest, AActor *GoalActor )
{
	if ( !Controller )
		return false;

	FVector Direction = Dest - Location;
	FLOAT ZDiff = Direction.Z;

	Direction.Z = 0.f;
	FLOAT Distance = Direction.Size();
	FCheckResult Hit(1.f);

	// FIXME - own version of reached destination, plus always steer prepared for next turn
	if ( ReachedDestination(Dest - Location, GoalActor) )
	{
		// FIXME - not if en route
        Throttle = 0;

		// if Pawn just reached a navigation point, set a new anchor
		ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
		if ( Nav )
			SetAnchor(Nav);
		return true;
	}
	else if ( (Distance < CylinderComponent->CollisionRadius)
			&& (!GoalActor || ((ZDiff > CylinderComponent->CollisionHeight + 2.f * MaxStepHeight)
				&& !GetLevel()->SingleLineCheck(Hit, this, Dest, Location, TRACE_World))) )
	{
		// failed - below target
		return true;
	}
	else if ( Distance > 0.f )
	{
		Direction = Direction/Distance;
		FVector Facing = Rotation.Vector();
		FLOAT Dot = Facing | Direction;
		Throttle = 1.f;
		if  ( Dot > 0.95f )
			Steering = 0.f;
		else if ( Dot < 0.f )
		{
			// fixme traces/pick dir
			Steering = -1.f;
			Throttle = -1.f;
		}
		else
		{
			FVector Cross = Facing ^ FVector(0.f,0.f,1.f);
			if ( (Cross | Direction) > 0.f )
				Steering = 1.f;
			else
				Steering = -1.f;
		}
	}

	if ( Controller->MoveTarget && Controller->MoveTarget->IsA(APawn::StaticClass()) )
	{
		APawn* P = (APawn*)Controller->MoveTarget;

		if (Distance < CylinderComponent->CollisionRadius + P->CylinderComponent->CollisionRadius + 0.8f * MeleeRange)
			return true;
		return false;
	}

/*	FLOAT speed = Velocity.Size(); 

	if ( Distance < 1.4f * AvgPhysicsTime * speed )
	{
		if ( !bReducedSpeed ) //haven't reduced speed yet
		{
			DesiredSpeed = 0.51f * DesiredSpeed;
			bReducedSpeed = 1;
		}
		if ( speed > 0 )
			DesiredSpeed = Min(DesiredSpeed, 200.f/speed);
	}
*/
	return false;
}

/* rotateToward()
rotate Actor toward a point.  Returns 1 if target rotation achieved.
(Set DesiredRotation, let physics do actual move)
*/
void AVehicle::rotateToward(AActor *Focus, FVector FocalPoint)
{
	// fixme implement as three point turn
}
