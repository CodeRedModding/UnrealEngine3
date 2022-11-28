/*=============================================================================
	UnReach.cpp: Reachspec creation and management

	These methods are members of the UReachSpec class, 

	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/
#include "EnginePrivate.h"
#include "UnPath.h"

void UReachSpec::Init()
{
	// Init everything here.
	Start = End = NULL;
	Distance = CollisionRadius = CollisionHeight = 0;
	reachFlags = 0;
	bPruned = 0;
	bForced = 0;
	MaxLandingVelocity = 0;
};


/* Path Color
returns the color to use when displaying this path in the editor
 */
FPlane UReachSpec::PathColor()
{
	if ( reachFlags & R_FORCED ) // yellow for forced paths
		return FPlane(1.f, 1.f, 0.f, 0.f);

	if ( reachFlags & R_PROSCRIBED ) // red is reserved for proscribed paths
		return FPlane(1.f, 0.f, 0.f, 0.f);

	if ( (reachFlags & R_FLY) && !(reachFlags & R_WALK) )
		return FPlane(1.f,0.5f,0.f,0.f);	// orange = flying

	if ( reachFlags & R_SPECIAL )
		return FPlane(1.f,0.f,1.f, 0.f);	// purple path = special (lift or teleporter)

	if ( reachFlags & R_LADDER )
		return FPlane(1.f,0.5f, 1.f,0.f);	// light purple = ladder

	/*
	if ( (CollisionRadius >= MAXCOMMONRADIUS) && (CollisionHeight >= MINCOMMONHEIGHT)
			&& !(reachFlags & R_FLY) )
		return FPlane(1.f,1.f,1.f,0.f);  // white path = very wide

	if ( (CollisionRadius >= COMMONRADIUS) && (CollisionHeight >= MINCOMMONHEIGHT)
			&& !(reachFlags & R_FLY) )
		return FPlane(0.f,1.f,0.f,0.f);  // green path = wide
	*/

	return FPlane(0.f,0.f,1.f,0.f); // blue path = narrow
}

/* operator <=
Used for comparing reachspecs reach requirements
less than means that this has easier reach requirements (equal or easier in all categories,
does not compare distance, start, and end
*/
int UReachSpec::operator<= (const UReachSpec &Spec)
{
	int result =  
		(CollisionRadius >= Spec.CollisionRadius) &&
		(CollisionHeight >= Spec.CollisionHeight) &&
		((reachFlags | Spec.reachFlags) == Spec.reachFlags) &&
		(MaxLandingVelocity <= Spec.MaxLandingVelocity);

	return result; 
}

/* defineFor()
initialize the reachspec for a  traversal from start actor to end actor.
Note - this must be a direct traversal (no routing).
Returns 1 if the definition was successful (there is such a reachspec), and zero
if no definition was possible
*/

int UReachSpec::defineFor(ANavigationPoint *begin, ANavigationPoint *dest, APawn *ScoutPawn)
{
	Start = begin;
	End = dest;
	AScout *Scout = Cast<AScout>(ScoutPawn);
	Scout->InitForPathing();
	Start->PrePath();
	End->PrePath();
	debugf(TEXT("Define path from %s to %s"),begin->GetName(), dest->GetName());
	INT result = findBestReachable((AScout *)Scout);
	Start->PostPath();
	End->PostPath();
	return result;
}

/* findBestReachable()

  This is the function that determines what radii and heights of pawns are checked for setting up reachspecs

  Modify this function if you want a different set of parameters checked

  Note that reachable regions are also determined by this routine, so allow a wide max radius to be tested

  What is checked currently (in order):

	Crouched human (HUMANRADIUS, CROUCHEDHUMANHEIGHT)
	Standing human (HUMANRADIUS, HUMANHEIGHT)
	Small non-human
	Common non-human (COMMONRADIUS, COMMONHEIGHT)
	Large non-human (MAXCOMMONRADIUS, MAXCOMMONHEIGHT)

	Then, with the largest height that was successful, determine the reachable region

  TODO: it might be a good idea to look at what pawns are referenced by the level, and check for all their radii/height combos.
  This won't work for a UT type game where pawns are dynamically loaded

*/

INT UReachSpec::findBestReachable(AScout *Scout)
{
	// start with the smallest collision size
	FLOAT bestRadius = Scout->PathSizes(0).Radius;
	FLOAT bestHeight = Scout->PathSizes(0).Height;
	Scout->SetCollisionSize(bestRadius, bestHeight);
	INT success = 0;
	// attempt to place the scout at the start node
	if (PlaceScout(Scout))
	{
		FCheckResult Hit(1.f);
		// initialize scout for movement
		Scout->MaxLandingVelocity = 0.f;
		FVector ViewPoint = Start->Location;
		ViewPoint.Z += Start->CylinderComponent->CollisionHeight;
		// check visibility to end node, first from eye level,
		// and then directly if that failed
		if (Scout->GetLevel()->SingleLineCheck(Hit, Scout, End->Location, ViewPoint, TRACE_World | TRACE_StopAtFirstHit) ||
			Scout->GetLevel()->SingleLineCheck(Hit, Scout, End->Location, Scout->Location, TRACE_World | TRACE_StopAtFirstHit))
		{
			// if we successfully walked with smallest collision,
			if ((success = Scout->actorReachable(End,1,1)) != 0)
			{
				// save the movement flags and landing velocity
				MaxLandingVelocity = (int)(Scout->MaxLandingVelocity);
				reachFlags = success;
				// and find the largest supported size
				INT svdSuccess = success;
				for (INT idx = 1; idx < Scout->PathSizes.Num() && success != 0; idx++)
				{
					 Scout->SetCollisionSize(Scout->PathSizes(idx).Radius,Scout->PathSizes(idx).Height);
					 if ((success = PlaceScout(Scout)) != 0)
					 {
						 if ((success = Scout->actorReachable(End,1,1)) != 0)
						 {
							 // save the size if still reachable
							 bestRadius = Scout->PathSizes(idx).Radius;
							 bestHeight = Scout->PathSizes(idx).Height;
							 svdSuccess = success;
						 }
						 //TODO: handle testing/flagging crouch specs
					 }
				}
				success = svdSuccess;
			}
		}
	}
	if (success != 0)
	{
		// init reach spec based on results
		CollisionRadius = (INT)bestRadius;
		CollisionHeight = (INT)bestHeight;
		Distance = (int)(End->Location - Start->Location).Size(); 
	}
	return success;
}

UBOOL UReachSpec::PlaceScout(AScout * Scout)
{
	// try placing above and moving down
	FCheckResult Hit(1.f);
	UBOOL bSuccess = false;

	if ( Start->Base )
	{
		FVector Up(0.f,0.f, Scout->CylinderComponent->CollisionHeight - Start->CylinderComponent->CollisionHeight + ::Max(0.f, Scout->CylinderComponent->CollisionRadius - Start->CylinderComponent->CollisionRadius)); 
		if ( Scout->GetLevel()->FarMoveActor(Scout, Start->Location + Up) )
		{
			bSuccess = true;
			Scout->GetLevel()->MoveActor(Scout, -1.f * Up, Scout->Rotation, Hit);
		}
	}
	if ( !bSuccess && !Scout->GetLevel()->FarMoveActor(Scout, Start->Location) )
		return false;

	// if scout is walking, make sure it is on the ground
	if ( (Scout->Physics == PHYS_Walking) && !Scout->PhysicsVolume->bWaterVolume )
		Scout->GetLevel()->MoveActor(Scout, FVector(0,0,-1*Start->CylinderComponent->CollisionHeight), Scout->Rotation, Hit);
	return true;
}

