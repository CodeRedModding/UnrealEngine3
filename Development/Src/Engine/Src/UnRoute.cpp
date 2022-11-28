/*=============================================================================
	UnRoute.cpp: Unreal AI routing code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* ...
=============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"
#include "EngineAIClasses.h"

ANavigationPoint* FSortedPathList::findStartAnchor(APawn *Searcher)
{
	// see which nodes are visible and reachable
	FCheckResult Hit(1.f);
	for ( INT i=0; i<numPoints; i++ )
	{
		Searcher->GetLevel()->SingleLineCheck( Hit, Searcher, Path[i]->Location, Searcher->Location, TRACE_World|TRACE_StopAtFirstHit );
		if ( Hit.Actor )
			Searcher->GetLevel()->SingleLineCheck( Hit, Searcher, Path[i]->Location + FVector(0.f,0.f, Path[i]->CylinderComponent->CollisionHeight), Searcher->Location + FVector(0.f,0.f, Searcher->CylinderComponent->CollisionHeight), TRACE_World|TRACE_StopAtFirstHit );
		if ( !Hit.Actor && Searcher->actorReachable(Path[i], 1, 0) )
			return Path[i];
	}
	return NULL;
}

ANavigationPoint* FSortedPathList::findEndAnchor(APawn *Searcher, AActor *GoalActor, FVector EndLocation, UBOOL bAnyVisible, UBOOL bOnlyCheckVisible )
{
	if ( bOnlyCheckVisible && !bAnyVisible )
		return NULL;

	ANavigationPoint* NearestVisible = NULL;
	ULevel* MyLevel = Searcher->GetLevel();
	FVector RealLoc = Searcher->Location;

	// now see from which nodes EndLocation is visible and reachable
	FCheckResult Hit(1.f);
	for ( INT i=0; i<numPoints; i++ )
	{
		MyLevel->SingleLineCheck( Hit, Searcher, EndLocation, Path[i]->Location, TRACE_World|TRACE_StopAtFirstHit );
		if ( Hit.Actor )
		{
			if ( GoalActor )
			{
				// JAG_COLRADIUS_HACK
				FLOAT GoalRadius, GoalHeight;
				GoalActor->GetBoundingCylinder(GoalRadius, GoalHeight);

				MyLevel->SingleLineCheck( Hit, Searcher, EndLocation + FVector(0.f,0.f,GoalHeight), Path[i]->Location  + FVector(0.f,0.f, Path[i]->CylinderComponent->CollisionHeight), TRACE_World|TRACE_StopAtFirstHit );
			}
			else
				MyLevel->SingleLineCheck( Hit, Searcher, EndLocation, Path[i]->Location  + FVector(0.f,0.f, Path[i]->CylinderComponent->CollisionHeight), TRACE_World|TRACE_StopAtFirstHit );
		}
			if ( !Hit.Actor )
		{
			if ( bOnlyCheckVisible )
				return Path[i];
		FVector AdjustedDest = Path[i]->Location;
		AdjustedDest.Z = AdjustedDest.Z + Searcher->CylinderComponent->CollisionHeight - Path[i]->CylinderComponent->CollisionHeight;
			if ( MyLevel->FarMoveActor(Searcher,AdjustedDest,1,1) )
		{
			if ( GoalActor ? Searcher->actorReachable(GoalActor,1,0) : Searcher->pointReachable(EndLocation, 1) )
			{
				MyLevel->FarMoveActor(Searcher, RealLoc, 1, 1);
				return Path[i];
			}
			else if ( bAnyVisible && !NearestVisible )
				NearestVisible = Path[i];
		}
	}
	}

	if ( Searcher->Location != RealLoc )
		MyLevel->FarMoveActor(Searcher, RealLoc, 1, 1);

	return NearestVisible;
}

UBOOL APawn::ValidAnchor()
{
	if ( Anchor && !Anchor->bBlocked 
		&& (bCanCrouch ? (Anchor->MaxPathSize.X >= CrouchRadius) && (Anchor->MaxPathSize.Z >= CrouchHeight)
						: (Anchor->MaxPathSize.X >= CylinderComponent->CollisionRadius) && (Anchor->MaxPathSize.Z >= CylinderComponent->CollisionHeight))
		&& ReachedDestination(Anchor->Location-Location,Anchor) )
	{
		LastValidAnchorTime = Level->TimeSeconds;
		LastAnchor = Anchor;
		return true;
	}
	return false;
}

typedef FLOAT ( *NodeEvaluator ) (ANavigationPoint*, APawn*, FLOAT);

static FLOAT FindEndPoint( ANavigationPoint* CurrentNode, APawn* seeker, FLOAT bestWeight )
{
	if ( CurrentNode->bEndPoint )
	{
//		debugf(TEXT("Found endpoint %s"),CurrentNode->GetName());
		return 2.f;
	}
	else
		return 0.f;
}

//#define DEBUG_PATHFIND

FLOAT APawn::findPathToward(AActor *goal, FVector GoalLocation, NodeEvaluator NodeEval, FLOAT BestWeight, UBOOL bWeightDetours)
{
#ifdef DEBUG_PATHFIND
	debugf(TEXT("%s FindPathToward: %s"),GetName(), goal != NULL ? goal->GetName() : TEXT("Point"));
#endif
	NextPathRadius = 0.f;
	if ( !Level->NavigationPointList || (FindAnchorFailedTime == Level->TimeSeconds) || !Controller )
	{
#ifdef DEBUG_PATHFIND
		debugf(TEXT("- initial abort, %2.1f"),FindAnchorFailedTime);
#endif
		return 0.f;
	}

	Controller->RouteCache.Empty();

	UBOOL bSpecifiedEnd = (NodeEval == NULL);
	UBOOL bOnlyCheckVisible = (Physics == PHYS_RigidBody) || (Physics == PHYS_Articulated);
	FVector RealLocation = Location;
	ANavigationPoint *EndAnchor = Cast<ANavigationPoint>(goal);
	FLOAT EndDist=0, StartDist=0;
	if (goal != NULL)
	{
		GoalLocation = goal->Location;
	}

	// find EndAnchor (destination path on navigation network)
	if ( goal && !EndAnchor )
	{
		APawn* PawnGoal = goal->GetAPawn();
		if ( PawnGoal )
		{
			if ( PawnGoal->ValidAnchor() )
			{
				EndAnchor = PawnGoal->Anchor;
				EndDist = (EndAnchor->Location - GoalLocation).Size();
			}
			else
			{
				AAIController *AI = Cast<AAIController>(PawnGoal->Controller);
				if ( AI && (AI->GetStateFrame()->LatentAction == UCONST_LATENT_MOVETOWARD) )
					EndAnchor = Cast<ANavigationPoint>(AI->MoveTarget);
			}
			if ( !EndAnchor && PawnGoal->LastAnchor && (Anchor != PawnGoal->LastAnchor) && (Level->TimeSeconds - PawnGoal->LastValidAnchorTime < 0.25f)
				&& PawnGoal->Controller && PawnGoal->Controller->LineOfSightTo(PawnGoal->LastAnchor) )
			{
				EndAnchor = PawnGoal->LastAnchor;
				EndDist = (EndAnchor->Location - GoalLocation).Size();
			}
			
			if ( !EndAnchor && (PawnGoal->Physics == PHYS_Falling) )
			{
				if ( PawnGoal->LastAnchor && (Anchor != PawnGoal->LastAnchor) && (Level->TimeSeconds - PawnGoal->LastValidAnchorTime < 1.f)
					 && PawnGoal->Controller && PawnGoal->Controller->LineOfSightTo(PawnGoal->LastAnchor) )
				{
					EndAnchor = PawnGoal->LastAnchor;
					EndDist = (EndAnchor->Location - GoalLocation).Size();
				}
				else
				{
					bOnlyCheckVisible = true;
				}
			}
			if ( !EndAnchor )
			{
				APlayerController *PC = Cast<APlayerController>(PawnGoal->Controller);
				if ( PC && (PawnGoal->Location == PC->FailedPathStart) )
				{
					bOnlyCheckVisible = true;
				}
			}
		}
		else
		{
			ACarriedObject *Flag = Cast<ACarriedObject>(goal);
			if ( Flag )
			{
				if ( Flag->LastAnchor && (Level->TimeSeconds - Flag->LastValidAnchorTime < 0.25f) )
				{
					EndAnchor = Flag->LastAnchor;
					EndDist = (EndAnchor->Location - GoalLocation).Size();
				}
				else if ( (Flag->Physics == PHYS_Falling) || (Flag->Physics == PHYS_Projectile) )
					bOnlyCheckVisible = true;
			}
			else if ( goal->Physics == PHYS_Falling )
				bOnlyCheckVisible = true;
		}
	}

	// check if my anchor is still valid
	if ( !ValidAnchor() )
		Anchor = NULL;
	
	if ( !Anchor || (!EndAnchor && bSpecifiedEnd) )
	{
		//find anchors from among nearby paths
		FCheckResult Hit(1.f);
		FSortedPathList StartPoints, DestPoints;
		int dist;
		for ( ANavigationPoint *Nav=Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		{
			Nav->ClearForPathFinding();
			if ( !Nav->bBlocked )
			{
				if ( !Anchor )
				{
					dist = appRound((Location - Nav->Location).SizeSquared());
					if ( dist < MAXPATHDISTSQ )
						StartPoints.addPath(Nav, dist);
				}
				if ( !EndAnchor && bSpecifiedEnd )
				{
					dist = appRound((GoalLocation - Nav->Location).SizeSquared());
					if ( dist < MAXPATHDISTSQ )
						DestPoints.addPath(Nav, dist);
				}
			}
		}
#ifdef DEBUG_PATHFIND
		debugf(TEXT("- Startpoints = %d, DestPoints = %d"), StartPoints.numPoints, DestPoints.numPoints);
#endif
		if ( !Anchor )
		{
			if ( StartPoints.numPoints > 0 )
				Anchor = StartPoints.findStartAnchor(this);
			if ( !Anchor )
			{
				FindAnchorFailedTime = Level->TimeSeconds;
				return 0.f;
			}
			LastValidAnchorTime = Level->TimeSeconds;
			LastAnchor = Anchor;
			StartDist = (Anchor->Location - Location).Size();
			if ( Abs(Anchor->Location.Z - Location.Z) < ::Max(CylinderComponent->CollisionHeight,Anchor->CylinderComponent->CollisionHeight) )
			{
				FLOAT StartDist2D = (Anchor->Location - Location).Size2D();
				if ( StartDist2D <= CylinderComponent->CollisionRadius + Anchor->CylinderComponent->CollisionRadius )
				StartDist = 0.f;
		}
		}
		if ( !EndAnchor && bSpecifiedEnd )
		{
			if ( DestPoints.numPoints > 0 )
				EndAnchor = DestPoints.findEndAnchor(this, goal, GoalLocation, (goal && Controller->AcceptNearbyPath(goal)), bOnlyCheckVisible );
			if ( !EndAnchor )
				return 0.f;
			if ( goal )
			{
				APawn* PawnGoal = goal->GetAPawn();
				if ( PawnGoal )
				{
					PawnGoal->LastValidAnchorTime = Level->TimeSeconds;
					PawnGoal->LastAnchor = EndAnchor;
				}
				else
				{
					ACarriedObject *Flag = Cast<ACarriedObject>(goal);
					if ( Flag )
					{
						Flag->LastValidAnchorTime = Level->TimeSeconds;
						Flag->LastAnchor = EndAnchor;
					}
				}
			}
			EndDist = (EndAnchor->Location - GoalLocation).Size();
		}
		if ( EndAnchor == Anchor )
		{
			// no way to get closer on the navigation network
			INT PassedAnchor = 0;

			if ( ReachedDestination(Anchor->Location - Location, goal) )
			{
				PassedAnchor = 1;
				if ( !goal )
				{
					return 0.f;
				}
			}
			else
			{
				// if on route (through or past anchor), keep going
				FVector GoalAnchor = GoalLocation - Anchor->Location;
				GoalAnchor = GoalAnchor.SafeNormal();
				FVector ThisAnchor = Location - Anchor->Location;
				ThisAnchor = ThisAnchor.SafeNormal();
				if ( (ThisAnchor | GoalAnchor) > 0.9 )
					PassedAnchor = 1;
			}

			if (PassedAnchor != NULL)
			{
				Controller->RouteCache.AddItem(goal);
			}
			else
			{
				Controller->RouteCache.AddItem(Anchor);
			}
			return (GoalLocation - Location).Size();
		}
	}
	else
	{
		for ( ANavigationPoint *Nav=Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
			Nav->ClearForPathFinding();
	}
	//debugf(TEXT("Found anchors"));

	if ( EndAnchor )
		EndAnchor->bEndPoint = 1;

	GetLevel()->FarMoveActor(this, RealLocation, 1, 1);
	Anchor->visitedWeight = appRound(StartDist);
	if ( bSpecifiedEnd )
	{
		NodeEval = &FindEndPoint;
	}
#ifdef DEBUG_PATHFIND
	debugf(TEXT("- searching for path from %s to %s"),Anchor->GetName(),EndAnchor->GetName());
#endif
	ANavigationPoint* BestDest = breadthPathTo(NodeEval,Anchor,calcMoveFlags(),&BestWeight, bWeightDetours);
	if ( BestDest )
	{
#ifdef DEBUG_PATHFIND
		debugf(TEXT("- found path!"));
#endif
		Controller->SetRouteCache(BestDest,StartDist,EndDist);
		return BestWeight;
	}
#ifdef DEBUG_PATHFIND
	else
	{
		debugf(TEXT("- no path!"));
	}
#endif
	return 0.f;
}

/* addPath()
add a path to a sorted path list - sorted by distance
*/

void FSortedPathList::addPath(ANavigationPoint *node, INT dist)
{
	int n=0;
	if ( numPoints > 8 )
	{
		if ( dist > Dist[numPoints/2] )
		{
			n = numPoints/2;
			if ( (numPoints > 16) && (dist > Dist[n + numPoints/4]) )
				n += numPoints/4;
		}
		else if ( (numPoints > 16) && (dist > Dist[numPoints/4]) )
			n = numPoints/4;
	}

	while ((n < numPoints) && (dist > Dist[n]))
		n++;

	if (n < MAXSORTED)
	{
		if (n == numPoints)
		{
			Path[n] = node;
			Dist[n] = dist;
			numPoints++;
		}
		else
		{
		ANavigationPoint *nextPath = Path[n];
			INT nextDist = Dist[n];
		Path[n] = node;
		Dist[n] = dist;
		if (numPoints < MAXSORTED)
			numPoints++;
		n++;
		while (n < numPoints)
		{
			ANavigationPoint *afterPath = Path[n];
				INT afterDist = Dist[n];
			Path[n] = nextPath;
				Dist[n] = nextDist;
			nextPath = afterPath;
			nextDist = afterDist;
			n++;
		}
	}
}
}

//-------------------------------------------------------------------------------------------------
/* breadthPathTo()
Breadth First Search through navigation network
starting from path bot is on.

Return when NodeEval function returns 1
*/
ANavigationPoint* APawn::breadthPathTo(NodeEvaluator NodeEval, ANavigationPoint *start, int moveFlags, FLOAT *Weight, UBOOL bWeightDetours)
{
	ANavigationPoint* currentnode = start;
	ANavigationPoint* nextnode = NULL;
	ANavigationPoint* LastAdd = currentnode;
	ANavigationPoint* BestDest = NULL;

	INT iRadius = appFloor(CylinderComponent->CollisionRadius);
	INT iHeight = appFloor(CylinderComponent->CollisionHeight);
	INT iMaxFallSpeed = appFloor(MaxFallSpeed);
	FLOAT CrouchMultiplier = CROUCHCOSTMULTIPLIER * 1.f/CrouchedPct;
	APawn* DefaultPawn = (APawn*)(GetClass()->GetDefaultActor());
	INT DefaultCollisionHeight = DefaultPawn->CylinderComponent->CollisionHeight;
	
	if ( bCanCrouch )
	{
		iHeight = appFloor(CrouchHeight);
		iRadius = appFloor(CrouchRadius);
	}
	INT n = 0;
	if ( Controller )
		Controller->eventSetupSpecialPathAbilities();

	while ( currentnode )
	{
		currentnode->bAlreadyVisited = true;
#ifdef DEBUG_PATHFIND
		debugf(TEXT("-> Distance to %s is %d"), currentnode->GetName(), currentnode->visitedWeight);
#endif
		FLOAT thisWeight = (*NodeEval)(currentnode, this, *Weight);
		if ( thisWeight > *Weight )
		{
			*Weight = thisWeight;
			BestDest = currentnode;
		}
		if ( *Weight >= 1.f )
		{
			return CheckDetour(BestDest, start, bWeightDetours);
		}
		if ( n++ > 200 )
		{
			if ( *Weight > 0 )
			{
				return CheckDetour(BestDest, start, bWeightDetours);
			}
			else
			{
				n = 150;
			}
		}
		INT nextweight = 0;

		for ( INT i=0; i<currentnode->PathList.Num(); i++ )
		{
			ANavigationPoint* endnode = NULL;
			UReachSpec *spec = currentnode->PathList(i);
#ifdef DEBUG_PATHFIND
			debugf(TEXT("-> check path from %s to %s with %d, %d, supports? %s, visited? %s"),spec->Start->GetName(), spec->End->GetName(), spec->CollisionRadius, spec->CollisionHeight, spec->supports(iRadius, iHeight, moveFlags, iMaxFallSpeed) ? TEXT("True") : TEXT("False"), spec->End->bAlreadyVisited ? TEXT("TRUE") : TEXT("FALSE"));
#endif
			if ( spec && spec->End && !spec->End->bAlreadyVisited && spec->supports(iRadius, iHeight, moveFlags, iMaxFallSpeed) )
			{
				endnode = spec->End;
				if ( !endnode->bBlocked && (!endnode->bMayCausePain || !HurtByVolume(endnode)) )
				{
					if ( spec->bForced && endnode->bSpecialForced )
					{
						nextweight = spec->Distance + endnode->eventSpecialCost(this,spec);
					}
					else if( spec->CollisionHeight >= DefaultPawn->CylinderComponent->CollisionHeight )
					{
						nextweight = spec->Distance + endnode->cost;
					}
					else
					{
						nextweight = CrouchMultiplier * spec->Distance + endnode->cost;
					}
					
					if ( nextweight <= 0 )
					{
						debugf(TEXT("WARNING - negative weight %d from %s to %s"), nextweight, currentnode->GetName(), endnode->GetName());
						nextweight = 1;
					}
					INT newVisit = nextweight + currentnode->visitedWeight; 
#ifdef DEBUG_PATHFIND
					debugf(TEXT("--> Path from %s to %s costs %d total %d"), currentnode->GetName(), endnode->GetName(), nextweight, newVisit);
#endif
					if ( endnode->visitedWeight > newVisit )
					{
						// found a better path to endnode
						endnode->previousPath = currentnode;
						if ( endnode->prevOrdered ) //remove from old position
						{
							endnode->prevOrdered->nextOrdered = endnode->nextOrdered;
							if (endnode->nextOrdered)
							{
								endnode->nextOrdered->prevOrdered = endnode->prevOrdered;
							}
							if ( (LastAdd == endnode) || (LastAdd->visitedWeight > endnode->visitedWeight) )
							{
								LastAdd = endnode->prevOrdered;
							}
							endnode->prevOrdered = NULL;
							endnode->nextOrdered = NULL;
						}
						endnode->visitedWeight = newVisit;

						// LastAdd is a good starting point for searching the list and inserting this node
						nextnode = LastAdd;
						if ( nextnode->visitedWeight <= newVisit )
						{
							while (nextnode->nextOrdered && (nextnode->nextOrdered->visitedWeight < newVisit))
							{
								nextnode = nextnode->nextOrdered;
							}
						}
						else
						{
							while ( nextnode->prevOrdered && (nextnode->visitedWeight > newVisit) )
							{
								nextnode = nextnode->prevOrdered;
							}
						}

						if (nextnode->nextOrdered != endnode)
						{
							if (nextnode->nextOrdered)
								nextnode->nextOrdered->prevOrdered = endnode;
							endnode->nextOrdered = nextnode->nextOrdered;
							nextnode->nextOrdered = endnode;
							endnode->prevOrdered = nextnode;
						}
						LastAdd = endnode;
					}
				}
			}
		}
		currentnode = currentnode->nextOrdered;
	}
	return CheckDetour(BestDest, start, bWeightDetours);
}

ANavigationPoint* APawn::CheckDetour(ANavigationPoint* BestDest, ANavigationPoint* Start, UBOOL bWeightDetours)
{
	if ( !bWeightDetours || !Start || !BestDest || (Start == BestDest) || !Anchor )
	{
		return BestDest;
	}

	ANavigationPoint* DetourDest = NULL;
	FLOAT DetourWeight = 0.f;

	// FIXME - mark list to ignore (if already in route)
	for ( INT i=0; i<Anchor->PathList.Num(); i++ )
	{
		UReachSpec *spec = Anchor->PathList(i);
		if ( spec->End->visitedWeight < 2.f * MAXPATHDIST )
		{
			UReachSpec *Return = spec->End->GetReachSpecTo(Anchor);
			if ( Return && !Return->bForced )
			{
				spec->End->LastDetourWeight = spec->End->eventDetourWeight(this,spec->End->visitedWeight);
				if ( spec->End->LastDetourWeight > DetourWeight )
					DetourDest = spec->End;
			}
		}
	}
	if ( !DetourDest )
	return BestDest;

	ANavigationPoint *FirstPath = BestDest;
	// check that detourdest doesn't occur in route
	for ( ANavigationPoint *Path=BestDest; Path!=NULL; Path=Path->previousPath )
	{
		FirstPath = Path;
		if ( Path == DetourDest )
			return BestDest;
	}

	// check that AI really wants to detour
	if ( !Controller )
		return BestDest;
	Controller->RouteGoal = BestDest;
	Controller->RouteDist = BestDest->visitedWeight;
	if ( !Controller->eventAllowDetourTo(DetourDest) )
		return BestDest;

	// add detourdest to start of route
	if ( FirstPath != Anchor )
	{
		FirstPath->previousPath = Anchor;
		Anchor->previousPath = DetourDest;
		DetourDest->previousPath = NULL;
	}
	else
	{
		for ( ANavigationPoint *Path=BestDest; Path!=NULL; Path=Path->previousPath )
			if ( Path->previousPath == Anchor )
			{
				Path->previousPath = DetourDest;
				DetourDest->previousPath = Anchor;
				break;
			}
	}

	return BestDest;
}

/* SetRouteCache() puts the first 16 navigationpoints in the best route found in the
Controller's RouteCache[].
*/
void AController::SetRouteCache(ANavigationPoint *EndPath, FLOAT StartDist, FLOAT EndDist)
{
	RouteGoal = EndPath;
	if ( !EndPath )
		return;
	RouteDist = EndPath->visitedWeight + EndDist;

	// reverse order of linked node list
	EndPath->nextOrdered = NULL;
	while ( EndPath->previousPath )
	{
		EndPath->previousPath->nextOrdered = EndPath;
		EndPath = EndPath->previousPath;
	}
	// if the pawn is on the start node, then the first node in the path should be the next one

	if ( Pawn && (StartDist > 0.f) )
	{
		// if pawn not on the start node, check if second node on path is a better destination
		if ( EndPath->nextOrdered )
		{
			FLOAT TwoDist = (Pawn->Location - EndPath->nextOrdered->Location).Size();
			FLOAT PathDist = (EndPath->Location - EndPath->nextOrdered->Location).Size();
			if ( (TwoDist < 0.75f * MAXPATHDIST) && (TwoDist < PathDist)
				&& ((Level->NetMode != NM_Standalone) || (Level->TimeSeconds - Pawn->LastRenderTime < 5.f) || (StartDist > 250.f)) )
			{
				FCheckResult Hit(1.f);
				GetLevel()->SingleLineCheck( Hit, this, EndPath->nextOrdered->Location, Pawn->Location, TRACE_World|TRACE_StopAtFirstHit );
				if ( !Hit.Actor	&& Pawn->actorReachable(EndPath->nextOrdered, 1, 1) )
					EndPath = EndPath->nextOrdered;
			}
		}

	}
	else if ( EndPath->nextOrdered )
		EndPath = EndPath->nextOrdered;

	// place all of the path into the controller route cache
	while (EndPath != NULL)
	{
		RouteCache.AddItem(EndPath);
		EndPath = EndPath->nextOrdered;
	}
	if ( Pawn && RouteCache.Num() > 1 )
	{
		ANavigationPoint *FirstPath = Cast<ANavigationPoint>(RouteCache(0));
		UReachSpec* NextSpec = NULL;
		if ( FirstPath )
		{
			ANavigationPoint *SecondPath = Cast<ANavigationPoint>(RouteCache(1));
			if ( SecondPath )
				NextSpec = FirstPath->GetReachSpecTo(SecondPath);
		}
		if ( NextSpec )
			Pawn->NextPathRadius = NextSpec->CollisionRadius;
		else
			Pawn->NextPathRadius = 0.f;
	}
}



