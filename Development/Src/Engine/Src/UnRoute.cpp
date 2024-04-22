/*=============================================================================
	UnRoute.cpp: Unreal AI routing code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
 
#include "EnginePrivate.h"
#include "UnPath.h"
#include "EngineAIClasses.h"

IMPLEMENT_CLASS(ADynamicAnchor);

// path constraints
IMPLEMENT_CLASS(UPathConstraint);
IMPLEMENT_CLASS(UPath_TowardGoal);
IMPLEMENT_CLASS(UPath_TowardPoint);
IMPLEMENT_CLASS(UPath_AlongLine);
IMPLEMENT_CLASS(UPath_WithinTraversalDist);
IMPLEMENT_CLASS(UPath_WithinDistanceEnvelope);
IMPLEMENT_CLASS(UPath_MinDistBetweenSpecsOfType);
IMPLEMENT_CLASS(UPath_AvoidInEscapableNodes);
IMPLEMENT_CLASS(UPathGoalEvaluator);
IMPLEMENT_CLASS(UGoal_AtActor);
IMPLEMENT_CLASS(UGoal_Null);

/************************************************************************
   A* functions follow
 ************************************************************************/
ANavigationPoint* PopBestNode(ANavigationPoint*& OpenList)
{
	ANavigationPoint* Best = OpenList;
	OpenList = Best->nextOrdered;
	if(OpenList != NULL)
	{
		OpenList->prevOrdered = NULL;
	}
	
	
	// indicate this node is no longer on the open list
	Best->prevOrdered = NULL;
	Best->nextOrdered = NULL;
	return Best;
}

UBOOL InsertSorted(ANavigationPoint* NodeForInsertion, ANavigationPoint*& OpenList)
{
	// if list is empty insert at the beginning
	if(OpenList == NULL)
	{
		OpenList = NodeForInsertion;
		NodeForInsertion->nextOrdered = NULL;
		NodeForInsertion->prevOrdered = NULL;
		return TRUE;
	}

	ANavigationPoint* CurrentNode = OpenList;
#if !PS3 && !FINAL_RELEASE
	INT LoopCounter = 0;
#endif
	for(;CurrentNode != NULL;CurrentNode = CurrentNode->nextOrdered)
	{
#if !PS3 && !FINAL_RELEASE
		checkFatalPathFailure(LoopCounter++ <= MAX_LOOP_ITTS, TEXT("Infinite loop detected in A*::InsertSorted!  Try rebuilding paths."),FALSE);
#endif
		if(NodeForInsertion->bestPathWeight <= CurrentNode->bestPathWeight)
		{
			checkSlow(NodeForInsertion != CurrentNode);
			NodeForInsertion->nextOrdered = CurrentNode;
			NodeForInsertion->prevOrdered = CurrentNode->prevOrdered;
			if(CurrentNode->prevOrdered != NULL)
			{
				CurrentNode->prevOrdered->nextOrdered = NodeForInsertion;
			}
			else
			{
				OpenList = NodeForInsertion;
			}
			CurrentNode->prevOrdered = NodeForInsertion;
			return TRUE;
		}
		
		if(CurrentNode->nextOrdered == NULL)
			break;
	}

	// if we got here, append to the end
	CurrentNode->nextOrdered = NodeForInsertion;
	NodeForInsertion->prevOrdered = CurrentNode;
	return TRUE;

}

UBOOL AddToOpen(ANavigationPoint*& OpenList, ANavigationPoint* NodeToAdd, ANavigationPoint* GoalNode, INT EdgeCost, UReachSpec* EdgeSpec, APawn* Pawn)
{
	const FVector DirToGoal = (GoalNode->Location - NodeToAdd->Location).SafeNormal2D();
	ANavigationPoint* Predecessor = EdgeSpec->Start;
	NodeToAdd->visitedWeight = EdgeCost + Predecessor->visitedWeight;
	NodeToAdd->previousPath = Predecessor;
	NodeToAdd->bestPathWeight = EdgeSpec->AdjustedCostFor( Pawn, DirToGoal, GoalNode, NodeToAdd->visitedWeight );
	if(	NodeToAdd->bestPathWeight <= 0 )
	{
		debugf( TEXT("Path Warning!!! Got neg/zero adjusted cost for %s"),*EdgeSpec->GetName());
		DEBUGPATHLOG(FString::Printf(TEXT("Path Warning!!! Got neg/zero adjusted cost for %s"),*EdgeSpec->GetName()));
		NodeToAdd->bAlreadyVisited = TRUE;
		return TRUE;
	}

	return InsertSorted(NodeToAdd,OpenList);
}

UBOOL AddNodeToOpen( ANavigationPoint*& OpenList, ANavigationPoint* NodeToAdd, INT EdgeCost, INT HeuristicCost, UReachSpec* EdgeSpec, APawn* Pawn )
{
	ANavigationPoint* Predecessor = EdgeSpec->Start;
	NodeToAdd->visitedWeight = EdgeCost + Predecessor->visitedWeight;
	if(Predecessor->previousPath == NodeToAdd)
	{
		PRINTDEBUGPATHLOG(TRUE);
	}
	checkSlow(Predecessor->previousPath != NodeToAdd);
	
	NodeToAdd->previousPath	 = Predecessor;

	//debug
	DEBUGPATHLOG(FString::Printf(TEXT("Set %s prev path = %s"), *NodeToAdd->GetName(), *Predecessor->GetName()));
	DEBUGREGISTERCOST( NodeToAdd, TEXT("Previous"), Predecessor->visitedWeight );

	NodeToAdd->bestPathWeight = NodeToAdd->visitedWeight + HeuristicCost;
	return InsertSorted( NodeToAdd, OpenList );
}

// walks the A* previousPath list and adds the path to the routecache (reversing it so the order is start->goal)
static void SaveResultingPath(ANavigationPoint* Start, ANavigationPoint* Goal, APawn* Pawn)
{
	// fill in the route cache with the new path
	Pawn->Controller->RouteGoal = Goal;
	ANavigationPoint *CurrentNav = Goal;
#if !PS3 && !FINAL_RELEASE
	INT LoopCounter = 0;
#endif
	while (CurrentNav != Start && CurrentNav != NULL)
	{
#if !PS3 && !FINAL_RELEASE
		checkFatalPathFailure(LoopCounter++ <= MAX_LOOP_ITTS,TEXT("Infinite Loop Detected in A*::SaveResultingPath.  Try rebuilding paths"),);
#endif
		Pawn->Controller->RouteCache_InsertItem( CurrentNav, 0 );
		checkSlow(CurrentNav->previousPath == NULL || CurrentNav->previousPath != CurrentNav->previousPath->previousPath);
		CurrentNav = CurrentNav->previousPath;
	}
	// add the start if necessary
	if( !Pawn->ReachedDestination( Start ) )
	{
		Pawn->Controller->RouteCache_InsertItem( Start, 0 );
	}
}

void RemoveNodeFromOpen(ANavigationPoint* NodeToRemove, ANavigationPoint*& OpenList)
{
	if(NodeToRemove->prevOrdered != NULL)
	{
		NodeToRemove->prevOrdered->nextOrdered = NodeToRemove->nextOrdered;
		checkSlow(NodeToRemove->nextOrdered != NodeToRemove);
	}
	else // it's the top of the stack, so pop it off
	{
		OpenList = NodeToRemove->nextOrdered;
	}
	if(NodeToRemove->nextOrdered != NULL)
	{
		NodeToRemove->nextOrdered->prevOrdered = NodeToRemove->prevOrdered;
		NodeToRemove->nextOrdered = NULL;
	}						

	NodeToRemove->prevOrdered = NULL;
}

	
#define DO_PATH_PROFILING 0

#if DO_PATHCONSTRAINT_PROFILING
TArray<FConstraintProfileDatum> ConstraintProfileData;
INT	  CallCount =0;
FLOAT CallMax = -1.f;
FLOAT CallAvg=-1.f;
FLOAT CallTotal=0.f;
FConstraintProfileDatum* ProfileDatum = NULL;
#endif

static UBOOL ApplyConstraints(APawn* Pawn, UReachSpec* EdgeSpec, INT& EdgeCost, INT& HeuristicCost)
{
#if DO_PATHCONSTRAINT_PROFILING
	INT Idx=0;
	SCOPETIMER(CallCount,CallAvg,CallMax,CallTotal,OVERALL)
#endif
	UPathConstraint* Constraint = Pawn->PathConstraintList;

#ifdef _DEBUG
	INT OriginalEdgeCost = EdgeCost;
#endif
	while( Constraint != NULL )
	{
#if DO_PATHCONSTRAINT_PROFILING
		FName ClassName = Constraint->GetClass()->GetFName();
		if(ConstraintProfileData.Num() <= Idx)
		{
			ConstraintProfileData.AddItem(FConstraintProfileDatum(ClassName));
		}
		ProfileDatum = &ConstraintProfileData(Idx++);
		SCOPETIMER(ProfileDatum->CallCount,ProfileDatum->AvgTime,ProfileDatum->MaxTime,ProfileDatum->TotalTime,PERCONSTRAINT)
#endif
		if( !Constraint->EvaluatePath( EdgeSpec, Pawn, EdgeCost, HeuristicCost ) )
		{
			// debug
			DEBUGPATHLOG(FString::Printf(TEXT("---== Spec %s (%s->%s) forbidden by PathConstraint %s"),*EdgeSpec->GetName(), *EdgeSpec->Start->GetName(), *EdgeSpec->End->GetName(), *Constraint->GetName()));
			return FALSE;
		}
		Constraint = Constraint->NextConstraint;
	}
#ifdef _DEBUG
	checkSlow(EdgeCost >= OriginalEdgeCost);
#endif

	return TRUE;
}

static UBOOL GenerateConstrainedPath( APawn *Pawn, ANavigationPoint *Start )
 {
	if( Start == NULL || Pawn == NULL )
	{
		return FALSE;
	}

#if DO_PATH_PROFILING
	DWORD TotalCycles = 0;
	CLOCK_CYCLES(TotalCycles);
#endif
	
	if( Pawn->PathGoalList == NULL ||
		Pawn->PathGoalList->InitialAbortCheck( Start, Pawn ) )
	{
		DEBUGPATHLOG(FString::Printf(TEXT("[%f] GenerateConstrainedPath... Initial abort! %s goal %s"),Pawn->WorldInfo->TimeSeconds, *Pawn->GetName(),*Pawn->PathGoalList->GetName()));

		return FALSE;
	}

	#if DO_PATHCONSTRAINT_PROFILING
		ConstraintProfileData.Empty();
		CallCount=0;
		CallMax=-1.0f;
		CallAvg=-1.f;
		CallTotal=0.f;
	#endif

	//debug
	DEBUGPATHONLY(Pawn->FlushPersistentDebugLines());
	DEBUGPATHONLY(UWorld::VerifyNavList(*FString::Printf(TEXT("NEW!!! MyTestPathTo %s %s"), *Pawn->GetName(), *Start->GetFullName())););

	// add start node to the open list
	ANavigationPoint* OpenList = Start;
	UPathGoalEvaluator* GoalCheck = Pawn->PathGoalList;
	Start->visitedWeight = 0;
	Start->bestPathWeight = 0;

	// grab initial flags needed for spec testing
	INT iRadius	= Pawn->bCanCrouch ? appTrunc(Pawn->CrouchRadius) : appTrunc(Pawn->CylinderComponent->CollisionRadius);
	INT iHeight = Pawn->bCanCrouch ? appTrunc(Pawn->CrouchHeight) : appTrunc(Pawn->CylinderComponent->CollisionHeight);
	INT iMaxFallSpeed = appTrunc(Pawn->GetAIMaxFallSpeed());
	INT moveFlags = Pawn->calcMoveFlags();

	// maximum number of nodes to be popped from the open list before we bail out
	INT MaxPathVisits = 0;
	for (UPathGoalEvaluator* CurrentGoal = GoalCheck; CurrentGoal != NULL; CurrentGoal = CurrentGoal->NextEvaluator)
	{
		MaxPathVisits = Max<INT>(MaxPathVisits, CurrentGoal->MaxPathVisits);
	}
	// set a default value if none was specified in the goal evaluator
	if (MaxPathVisits == 0)
	{
		MaxPathVisits = UPathGoalEvaluator::StaticClass()->GetDefaultObject<UPathGoalEvaluator>()->MaxPathVisits;
	}
	INT NumVisits = 0;

#if DO_PATH_PROFILING
	FLOAT EvaluateGoalTime = 0;
	INT LoopItts = 0;
#endif
	// ++ Begin A* Loop
	

	while( OpenList != NULL )
	{
		//debug
		DEBUGSTOREPATHSTEP( OpenList, Pawn );

		// pop best node from Open list
		ANavigationPoint* CurrentNode = PopBestNode(OpenList);

		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("+++ evaluating %s w/ %d paths"),*CurrentNode->GetFullName(),CurrentNode->PathList.Num()));

		// if the node we just pulled from the open list is the goal, we're done!
		ANavigationPoint* PossibleGoal = CurrentNode;

#if DO_PATH_PROFILING
		DWORD ThisEvaluateGoalTime = 0;
		CLOCK_CYCLES(ThisEvaluateGoalTime);
#endif
		if (GoalCheck->EvaluateGoal(PossibleGoal, Pawn))
		{
			//debug
			DEBUGPATHLOG(FString::Printf(TEXT("-! found path to goal from %s"),*CurrentNode->GetName()));

			GoalCheck->GeneratedGoal = PossibleGoal;
#if DO_PATH_PROFILING
			UNCLOCK_CYCLES(ThisEvaluateGoalTime);
			EvaluateGoalTime += ThisEvaluateGoalTime;
#endif
			break;
		}

#if DO_PATH_PROFILING
		UNCLOCK_CYCLES(ThisEvaluateGoalTime);
		EvaluateGoalTime += ThisEvaluateGoalTime;
#endif

		// cap Open list pops at MaxPathVisits (after we check for goal, because if we just popped the goal off we don't want to do this :) )
		if(	++NumVisits > MaxPathVisits )
		{
			debugf(NAME_DevPath, TEXT("Path Warning!!! %s Exceeded maximum path visits while trying to path from %s, Returning best guess."), *Pawn->GetName(), *Start->GetName());
			DEBUGPATHLOG(FString::Printf(TEXT("Path Warning!!! %s Exceeded maximum path visits while trying to path from %s, Returning best guess."), *Pawn->GetName(), *Start->GetName()));
			GoalCheck->NotifyExceededMaxPathVisits(CurrentNode);
			break;
		}

		// for each edge leaving CurrentNode
		for( INT PathIdx = 0; PathIdx < CurrentNode->PathList.Num(); PathIdx++ )
		{
#if DO_PATH_PROFILING
			LoopItts++;
#endif
			// if it is a valid connection that the pawn can walk
			UReachSpec *Spec = CurrentNode->PathList(PathIdx);
			if( Spec == NULL || Spec->bDisabled || *Spec->End == NULL || Spec->End->ActorIsPendingKill() || !Spec->supports(iRadius,iHeight,moveFlags,iMaxFallSpeed) )
			{
				continue;
			}

			ANavigationPoint *CurrentNeighbor = Spec->End.Nav();

			//debug
			DEBUGPATHLOG(FString::Printf(TEXT("--- check path %s (%d/%d)"), *CurrentNeighbor->GetName(), PathIdx, CurrentNeighbor->bAlreadyVisited));

			// Cache the cost of this node
			INT InitialCost = Spec->CostFor(Pawn);
			DEBUGREGISTERCOST( CurrentNeighbor, TEXT("Initial"), InitialCost );
			if( Pawn->bModifyReachSpecCost )
			{
				InitialCost += Pawn->ModifyCostForReachSpec( Spec, InitialCost );
			}

			if( InitialCost <= 0 )
			{
				//debug
				DEBUGPATHLOG(FString::Printf(TEXT("---== Node cost == 0 which is BAD BAD 'mmkay? %s %s to %s %d"), *Spec->GetName(), *Spec->Start->GetName(), *CurrentNeighbor->GetName(), InitialCost ));

				// Skip broken paths
				continue;
			}

			if( InitialCost >= UCONST_BLOCKEDPATHCOST )
			{
				//debug
				DEBUGPATHLOG(FString::Printf(TEXT("---== node blocked")));
				DEBUGPATHONLY(Pawn->DrawDebugLine(CurrentNode->Location,CurrentNeighbor->Location,255,0,0,TRUE););

				// don't bother with blocked paths
				continue;
			}
			
			// make sure this edge is valid for the current pawn
			if( !CurrentNeighbor->ANavigationPoint::IsUsableAnchorFor(Pawn) )
			{
				//debug
				DEBUGPATHLOG(FString::Printf(TEXT("---== node is invalid anchor, no way to move through it ")));
				DEBUGPATHONLY(Pawn->DrawDebugLine(CurrentNode->Location,CurrentNeighbor->Location,255,0,0,TRUE););

				continue;
			}

			// apply our path constraints to this edge
			INT HeuristicCost = 0;
			if( !ApplyConstraints(Pawn,Spec,InitialCost,HeuristicCost) )
			{
				DEBUGPATHLOG(FString::Printf(TEXT("---== rejected by constraints ")));
				continue;
			}

			DEBUGPATHLOG(FString::Printf(TEXT("---= final costs: %i (%i) "), InitialCost, HeuristicCost));

			UBOOL bIsOnClosed	= CurrentNeighbor->bAlreadyVisited;
			UBOOL bIsOnOpen		= CurrentNeighbor->prevOrdered != NULL || CurrentNeighbor->nextOrdered != NULL || OpenList == CurrentNeighbor;

			if( bIsOnClosed || bIsOnOpen )
			{
				// as long as the costs already in the list is as good or better, throw this sucker out
				if( CurrentNeighbor->visitedWeight <= InitialCost + CurrentNode->visitedWeight )
				{
					continue; 
				}
				else // otherwise the incoming value is better, so pull it out of the lists
				{
					//debug
					DEBUGPATHLOG(FString::Printf(TEXT("--> found shortcut: prevweight %d newweight %d"), CurrentNeighbor->visitedWeight, (InitialCost + CurrentNode->visitedWeight) ));

					if( bIsOnClosed )
					{
						CurrentNeighbor->bAlreadyVisited = FALSE;
					}
					if( bIsOnOpen )
					{
						RemoveNodeFromOpen(CurrentNeighbor,OpenList);	
					}
				}
			}

			// add the new node to the open list
			if( !AddNodeToOpen( OpenList, CurrentNeighbor, InitialCost, HeuristicCost, Spec, Pawn ) )
			{
				break;
			}

			//debug
			DEBUGPRINTSINGLEPATH( CurrentNeighbor );
		}

		// mark the node we just explored from as closed (e.g. add to closed list)
		CurrentNode->bAlreadyVisited = TRUE;	
	}
	// -- End A* loop

#if DO_PATH_PROFILING
	DWORD DetermineFinalGoalTime = 0;
	CLOCK_CYCLES(DetermineFinalGoalTime);
#endif
	UBOOL bPathComplete = GoalCheck->DetermineFinalGoal( GoalCheck->GeneratedGoal );
#if DO_PATH_PROFILING
	UNCLOCK_CYCLES(DetermineFinalGoalTime);
#endif
	if( bPathComplete )
	{
		SaveResultingPath( Start, GoalCheck->GeneratedGoal, Pawn );
	}
	else
	{
		DEBUGPATHLOG(FString::Printf(TEXT("PATH ERROR!!!! No path found")));
	}

//	PRINTDEBUGPATHLOG(TRUE);

#if DO_PATH_PROFILING
	// do this up here so it doesn't get polluted with printf overhead
	UNCLOCK_CYCLES(TotalCycles);
#endif 

#if DO_PATHCONSTRAINT_PROFILING
	
	debugf(TEXT("-----------------------PATH CONSTRAINT STATS for %s---------------------"),(Pawn->Controller) ? *Pawn->Controller->GetName() : TEXT("NONE"));
	//add up total
	FLOAT RealTotal = 0.f;
	for(INT Idx=0;Idx<ConstraintProfileData.Num();++Idx)
	{
		RealTotal += ConstraintProfileData(Idx).TotalTime;
	}
	for(INT Idx=0;Idx<ConstraintProfileData.Num();++Idx)
	{
		FName ConstraintName = ConstraintProfileData(Idx).ConstraintName;
		debugf(TEXT("Time: %3.3fms (Per call: %3.3fms avg(%3.3fmsmax)) CallCount:%i PctOverall:%.2f%% -- %s"),
			ConstraintProfileData(Idx).TotalTime* GSecondsPerCycle *1000.f,
			ConstraintProfileData(Idx).AvgTime* GSecondsPerCycle *1000.f,
			ConstraintProfileData(Idx).MaxTime* GSecondsPerCycle *1000.f,
			ConstraintProfileData(Idx).CallCount,
			ConstraintProfileData(Idx).TotalTime/RealTotal*100.f,
			*ConstraintName.ToString());			
	}
	DWORD SamplingError = (CallTotal - RealTotal);
	debugf(TEXT("OVERALL TIME:%3.3fms (%3.3fms sampling time) (per call %3.3fms avg, %3.3fms max), CallCount: %i"),RealTotal*GSecondsPerCycle*1000.f,SamplingError*GSecondsPerCycle*1000.f,CallAvg*GSecondsPerCycle *1000.f,CallMax*GSecondsPerCycle *1000.f,CallCount);
	debugf(TEXT("-------------------------------------------------------------------------------------------------"));
#if DO_PATH_PROFILING
	TotalCycles -= SamplingError;
#endif
#endif

#if DO_PATH_PROFILING
	debugf(TEXT("GenerateConstrainedPath >>> Pathing from %s to %s took %3.3fms (%i PathVisits, %i loop iterations) EvaluateGoal: %3.3fms DetermineFinalGoal: %3.3fms GoalEvaluator: %s"),
					(Start) ? *Start->GetName() : TEXT("NULL"),
					(GoalCheck->GeneratedGoal) ? *GoalCheck->GeneratedGoal->GetName() : TEXT("NULL"),
					TotalCycles * GSecondsPerCycle * 1000.f,
					NumVisits,
					LoopItts,
					EvaluateGoalTime * GSecondsPerCycle * 1000.f,
					DetermineFinalGoalTime * GSecondsPerCycle * 1000.f,
					*GoalCheck->GetName()
					);
#endif

	return bPathComplete;
}

static UBOOL AStarBestPathTo(APawn *Pawn, ANavigationPoint *Start, ANavigationPoint *Goal, UBOOL bReturnPartial)
{
//	Pawn->FlushPersistentDebugLines();

	if(Goal == NULL)
	{
		return FALSE;
	}

	if(Start == Goal || Start->bEndPoint)
	{
		Pawn->Controller->RouteCache_InsertItem(Start,0);
		return TRUE;
	}

	// add start node to the open list
	ANavigationPoint* OpenList = Start;
	Start->visitedWeight = 0;
	Start->bestPathWeight = 0;

	// grab initial flags needed for spec testing
	INT iRadius	= Pawn->bCanCrouch ? appTrunc(Pawn->CrouchRadius) : appTrunc(Pawn->CylinderComponent->CollisionRadius);
	INT iHeight = Pawn->bCanCrouch ? appTrunc(Pawn->CrouchHeight) : appTrunc(Pawn->CylinderComponent->CollisionHeight);
	INT iMaxFallSpeed = appTrunc(Pawn->GetAIMaxFallSpeed());
	INT moveFlags = Pawn->calcMoveFlags();
	
	UBOOL bPathComplete = FALSE;
	

	// maximum number of nodes to be popped from the open list before we bail out
	const INT MaxPathVisits = 1024;
	INT NumVisits = 0;
	
	// ++ Begin A* Loop
	while(OpenList != NULL)
	{
		// pop best node from Open list
		ANavigationPoint* CurrentNode = PopBestNode(OpenList);

		// if the node we just pulled from the open list is the goal, we're done!
		if(CurrentNode == Goal)
		{
			bPathComplete = TRUE;
			break;
		}
		else if (CurrentNode->bEndPoint)
		{
			bPathComplete = TRUE;
			Goal = CurrentNode;
			break;
		}

		// cap Open list pops at MaxPathVisits (after we check for goal, because if we just popped the goal off we don't want to do this :) )
		if(	++NumVisits > MaxPathVisits )
		{
			debugf( TEXT("Path Warning!!! %s Exceeded maximum path visits while trying to path from %s to %s, Returning best guess."), *Pawn->GetName(), *Start->GetName(), *Goal->GetName() );
			DEBUGPATHLOG(FString::Printf(TEXT("Path Warning!!! %s Exceeded maximum path visits while trying to path from %s to %s, Returning best guess."), *Pawn->GetName(), *Start->GetName(), *Goal->GetName()));
			Goal->previousPath = CurrentNode;
			bPathComplete = TRUE;
			break;
		}

		// for each edge leaving CurrentNode
		for (INT PathIdx = 0; PathIdx < CurrentNode->PathList.Num(); PathIdx++)
		{
			// if it is a valid connection that the pawn can walk
			UReachSpec *Spec = CurrentNode->PathList(PathIdx);
			if (Spec == NULL || Spec->bDisabled || *Spec->End == NULL || Spec->End->ActorIsPendingKill() || !Spec->supports(iRadius,iHeight,moveFlags,iMaxFallSpeed))
			{
				continue;
			}

			ANavigationPoint *CurrentNeighbor = Spec->End.Nav();
			
			// Cache the cost of this node
			INT InitialCost = Spec->CostFor(Pawn);
			if( Pawn->bModifyReachSpecCost )
			{
				InitialCost += Pawn->ModifyCostForReachSpec( Spec, InitialCost );
			}
			
			// Make sure cost is valid
			if( InitialCost <= 0 )
			{
				debugf( TEXT("Path Warning!!! Cost from %s to %s is zero/neg %i -- %s"), *CurrentNode->GetFullName(), *CurrentNeighbor->GetFullName(), InitialCost, *Spec->GetName() );
				DEBUGPATHLOG(FString::Printf(TEXT("Path Warning!!! Cost from %s to %s is zero/neg %i -- %s"), *CurrentNode->GetFullName(), *CurrentNeighbor->GetFullName(), InitialCost, *Spec->GetName() ));
				InitialCost = 1;
			}

			if( InitialCost >= UCONST_BLOCKEDPATHCOST )
			{
				// don't bother with blocked paths
				continue;
			}

			// make sure this edge is valid for the current pawn
			if( !CurrentNeighbor->ANavigationPoint::IsUsableAnchorFor(Pawn) )
			{
				continue;
			}
			

			UBOOL bIsOnClosed = CurrentNeighbor->bAlreadyVisited;
			UBOOL bIsOnOpen = CurrentNeighbor->prevOrdered != NULL || CurrentNeighbor->nextOrdered != NULL || OpenList == CurrentNeighbor;

			if(bIsOnClosed == TRUE || bIsOnOpen == TRUE)
			{
				// as long as the costs already in the list is as good or better, throw this sucker out
				if(CurrentNeighbor->visitedWeight <= InitialCost + CurrentNode->visitedWeight)
				{
					continue; 
				}
				else // otherwise the incoming value is better, so pull it out of the lists
				{
					if(bIsOnClosed == TRUE)
					{
						CurrentNeighbor->bAlreadyVisited = FALSE;
					}
					if(bIsOnOpen == TRUE)
					{
						RemoveNodeFromOpen(CurrentNeighbor,OpenList);	
					}
				}
			}

			// add the new node to the open list
			if( AddToOpen(OpenList,CurrentNeighbor,Goal,InitialCost,Spec,Pawn) == FALSE )
			{
				break;
			}
		}

		// mark the node we just explored from as closed (e.g. add to closed list)
		CurrentNode->bAlreadyVisited = TRUE;
	}
	// -- End A* loop

	if( bPathComplete == TRUE )
	{
		SaveResultingPath(Start,Goal,Pawn);
	}
	else
	{
		debugf(NAME_Warning,TEXT("PATH ERROR!!!! No path found to %s from %s for %s"),*Goal->GetName(),*Start->GetName(),*Pawn->GetName());
	}

	return bPathComplete;
}

ANavigationPoint* FSortedPathList::FindStartAnchor(APawn *Searcher)
{
	// see which nodes are visible and reachable
	FCheckResult Hit(1.f);
	for ( INT i=0; i<numPoints; i++ )
	{
		GWorld->SingleLineCheck( Hit, Searcher, Path[i]->Location, Searcher->Location, TRACE_World|TRACE_StopAtAnyHit );
		if ( Hit.Actor )
			GWorld->SingleLineCheck( Hit, Searcher, Path[i]->Location + FVector(0.f,0.f, Path[i]->CylinderComponent->CollisionHeight), Searcher->Location + FVector(0.f,0.f, Searcher->CylinderComponent->CollisionHeight), TRACE_World|TRACE_StopAtAnyHit );
		if ( !Hit.Actor && Searcher->actorReachable(Path[i], 1, 0) )
			return Path[i];
	}
	return NULL;
}

ANavigationPoint* FSortedPathList::FindEndAnchor(APawn *Searcher, AActor *GoalActor, FVector EndLocation, UBOOL bAnyVisible, UBOOL bOnlyCheckVisible )
{
	if ( bOnlyCheckVisible && !bAnyVisible )
		return NULL;

	ANavigationPoint* NearestVisible = NULL;
	FVector RealLoc = Searcher->Location;

	// now see from which nodes EndLocation is visible and reachable
	FCheckResult Hit(1.f);
	for ( INT i=0; i<numPoints; i++ )
	{
		GWorld->SingleLineCheck( Hit, Searcher, EndLocation, Path[i]->Location, TRACE_World|TRACE_StopAtAnyHit );
		if ( Hit.Actor )
		{
			if ( GoalActor )
			{
				FLOAT GoalRadius, GoalHeight;
				GoalActor->GetBoundingCylinder(GoalRadius, GoalHeight);

				GWorld->SingleLineCheck( Hit, Searcher, EndLocation + FVector(0.f,0.f,GoalHeight), Path[i]->Location  + FVector(0.f,0.f, Path[i]->CylinderComponent->CollisionHeight), TRACE_World|TRACE_StopAtAnyHit );
			}
			else
				GWorld->SingleLineCheck( Hit, Searcher, EndLocation, Path[i]->Location  + FVector(0.f,0.f, Path[i]->CylinderComponent->CollisionHeight), TRACE_World|TRACE_StopAtAnyHit );
		}
			if ( !Hit.Actor )
		{
			if ( bOnlyCheckVisible )
				return Path[i];
		FVector AdjustedDest = Path[i]->Location;
		AdjustedDest.Z = AdjustedDest.Z + Searcher->CylinderComponent->CollisionHeight - Path[i]->CylinderComponent->CollisionHeight;
			if ( GWorld->FarMoveActor(Searcher,AdjustedDest,1,1) )
		{
			if ( GoalActor ? Searcher->actorReachable(GoalActor,1,0) : Searcher->pointReachable(EndLocation, 1) )
			{
				GWorld->FarMoveActor(Searcher, RealLoc, 1, 1);
				return Path[i];
			}
			else if ( bAnyVisible && !NearestVisible )
				NearestVisible = Path[i];
		}
	}
	}

	if ( Searcher->Location != RealLoc )
	{
		GWorld->FarMoveActor(Searcher, RealLoc, 1, 1);
	}

	return NearestVisible;
}

UBOOL APawn::ValidAnchor()
{
	// if bforcekeepanchor is on and the anchor is NULL, something bad happened..
	checkSlow(!bForceKeepAnchor || Anchor != NULL);

	if(bForceKeepAnchor && Anchor == NULL)
	{
#if !FINAL_RELEASE && !PS3
		warnf(TEXT("WARNING! bForceKeepAnchor is TRUE, but Anchor is NULL on %s.. turning off bForceKeepAnchor to avoid crashing, This is probably going to break behavior ;)"),*GetName());
#endif
		bForceKeepAnchor = FALSE;
	}

	

	if( bForceKeepAnchor || 
		(Anchor && !Anchor->bBlocked 
		&& (bCanCrouch ? (Anchor->MaxPathSize.Radius >= CrouchRadius) && (Anchor->MaxPathSize.Height >= CrouchHeight)
						: (Anchor->MaxPathSize.Radius >= CylinderComponent->CollisionRadius) && (Anchor->MaxPathSize.Height >= CylinderComponent->CollisionHeight))
		&& ReachedDestination(Location, Anchor->GetDestination(Controller), Anchor)) )
	{
		LastValidAnchorTime = GWorld->GetTimeSeconds();
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
//		debugf(TEXT("Found endpoint %s"),*CurrentNode->GetName());
		return 2.f;
	}
	else
		return 0.f;
}

/** utility for FindAnchor() that returns whether Start has a ReachSpec to End that is acceptable for considering Start as an anchor */
static FORCEINLINE UBOOL HasReachSpecForAnchoring(ANavigationPoint* Start, ANavigationPoint* End)
{
	UReachSpec* Spec = Start->GetReachSpecTo(End);
	// only allow octree-accessible specs
	return (Spec != NULL && Spec->bAddToNavigationOctree);
}

/** utility for FindAnchor() that will verify that the nav point is within maxstepheigt to TestLocation */
static FORCEINLINE UBOOL NavPointWithinStepHeight( APawn* Pawn, const FVector& TestLocation, ANavigationPoint* NavCandidate)
{

	FVector NavGroundPos = NavCandidate->Location;
	if(NavCandidate->CylinderComponent != NULL)
	{
		NavGroundPos.Z -= NavCandidate->CylinderComponent->CollisionHeight;
	}

	FVector TestGroundPos = TestLocation;
	if(Pawn->CylinderComponent != NULL)
	{
		TestGroundPos.Z -= Pawn->CylinderComponent->CollisionHeight;
	}

	return Abs<FLOAT>(NavGroundPos.Z-TestGroundPos.Z) < Pawn->MaxStepHeight;
}

/** finds the closest NavigationPoint within MAXPATHDIST that is usable by this pawn and directly reachable to/from TestLocation
* @param TestActor the Actor to find an anchor for
* @param TestLocation the location to find an anchor for
* @param bStartPoint true if we're finding the start point for a path search, false if we're finding the end point
* @param bOnlyCheckVisible if true, only check visibility - skip reachability test
* @param Dist (out) if an anchor is found, set to the distance TestLocation is from it. Set to 0.f if the anchor overlaps TestLocation
* @return a suitable anchor on the navigation network for reaching TestLocation, or NULL if no such point exists
*/
ANavigationPoint* APawn::FindAnchor(AActor* TestActor, const FVector& TestLocation, UBOOL bStartPoint, UBOOL bOnlyCheckVisible, FLOAT& Dist)
{
	INT Radius = appTrunc(CylinderComponent->CollisionRadius);
	INT Height = appTrunc(CylinderComponent->CollisionHeight);
	INT iMaxFallSpeed = appTrunc(GetAIMaxFallSpeed());
	INT MoveFlags = calcMoveFlags();

	// first try fast point check
	// return the first usable NavigationPoint found, otherwise use closest ReachSpec endpoint
	TArray<FNavigationOctreeObject*> Objects;
	GWorld->NavigationOctree->PointCheck(TestLocation, FVector(CylinderComponent->CollisionRadius, CylinderComponent->CollisionRadius, CylinderComponent->CollisionHeight), Objects);
	ANavigationPoint* BestReachSpecPoint = NULL;
	FLOAT BestReachSpecPointDistSquared = BIG_NUMBER;
	for (INT i = 0; i < Objects.Num(); i++)
	{
		ANavigationPoint* Nav = Objects(i)->GetOwner<ANavigationPoint>();
		if (Nav != NULL 
			&& Nav->IsUsableAnchorFor(this) 
			&& (bStartPoint ? !Nav->bDestinationOnly : (!Nav->bSourceOnly && !Nav->bMakeSourceOnly))
			&& NavPointWithinStepHeight(this,TestLocation,Nav))
		{
			Dist = 0.f;
			return Nav;
		}
		else
		{
			UReachSpec* Spec = Objects(i)->GetOwner<UReachSpec>();
			if( Spec != NULL && 
				Spec->Start != NULL && 
				*Spec->End != NULL && 
				!Spec->End.Nav()->bSpecialMove &&	// Don't do for special move paths, it breaks special movement code
				!Spec->End.Nav()->bDestinationOnly && 
				!Spec->bDisabled &&
				Spec->supports(Radius, Height, MoveFlags, iMaxFallSpeed))
			{
				FLOAT StartDistSquared = (Spec->Start->Location - TestLocation).SizeSquared();
				FLOAT EndDistSquared = (Spec->End->Location - TestLocation).SizeSquared();
				// choose closer endpoint
				if (StartDistSquared < EndDistSquared && Spec->Start->IsUsableAnchorFor(this) && HasReachSpecForAnchoring(Spec->End.Nav(), Spec->Start)) // only consider Start if there's a reverse ReachSpec
				{
					// if it's within stepheight, don't do a full test.. otherwise check reachability
					UBOOL bReachable = NavPointWithinStepHeight(this,TestLocation,Spec->Start);
					FVector RealLoc = Location;

					if (!bReachable)
					{
						bReachable = GWorld->FarMoveActor(this, TestLocation, 1, 1) && Reachable(Spec->Start->Location, 0);
						GWorld->FarMoveActor(this,RealLoc,TRUE,TRUE);
					}

					if (bReachable && StartDistSquared < BestReachSpecPointDistSquared)
					{
						if (bStartPoint && Spec->Start->PathList.Num() == 0)
						{
							continue;
						}
						// do fast sanity trace to spec center to make sure it isn't poking through wall
						//@FIXME: path building should be fixed so this can be assumed
						const FVector PathDir = (Spec->End->Location - Spec->Start->Location).SafeNormal();
						const FVector ClosestPoint = (Spec->Start->Location + (PathDir | (TestLocation - Spec->Start->Location)) * PathDir);
						FCheckResult Hit(1.0f);
						if (GWorld->SingleLineCheck(Hit, TestActor, ClosestPoint, TestLocation, TRACE_World | TRACE_StopAtAnyHit))
						{
							BestReachSpecPoint = Spec->Start;
							BestReachSpecPointDistSquared = StartDistSquared;
						}
					}
				}
				else if (EndDistSquared < BestReachSpecPointDistSquared && Spec->End.Nav()->IsUsableAnchorFor(this))
				{
					// if it's within stepheight, don't do a full test.. otherwise check reachability
					UBOOL bReachable = NavPointWithinStepHeight(this,TestLocation,Spec->End.Nav());
					FVector RealLoc = Location;

					if (!bReachable)
					{
						bReachable = GWorld->FarMoveActor(this, TestLocation, 1, 1) && Reachable(Spec->End->Location, 0);
						GWorld->FarMoveActor(this,RealLoc,TRUE,TRUE);
					}

					if (bReachable)
					{
						if (bStartPoint && Spec->End.Nav()->PathList.Num() == 0)
						{
							continue;
						}
						// do fast sanity trace to spec center to make sure it isn't poking through wall
						//@FIXME: path building should be fixed so this can be assumed
						const FVector PathDir = (Spec->End->Location - Spec->Start->Location).SafeNormal();
						const FVector ClosestPoint = (Spec->Start->Location + (PathDir | (TestLocation - Spec->Start->Location)) * PathDir);
						FCheckResult Hit(1.0f);
						if (GWorld->SingleLineCheck(Hit, TestActor, ClosestPoint, TestLocation, TRACE_World | TRACE_StopAtAnyHit))
						{
							BestReachSpecPoint = Spec->End.Nav();
							BestReachSpecPointDistSquared = EndDistSquared;
						}
					}
				}
			}
		}
	}
	// not directly touching NavigationPoint, use closest ReachSpec endpoint, if any
	if (BestReachSpecPoint != NULL)
	{
		Dist = appSqrt(BestReachSpecPointDistSquared);
		return BestReachSpecPoint;
	}

	// point check failed, try MAXPATHDIST radius check
	// we'll need to trace and check reachability, so create a distance sorted list of suitable points and then check them until we find one
	Objects.Empty(Objects.Num());
	FSortedPathList TestPoints;
	GWorld->NavigationOctree->RadiusCheck(TestLocation, MAXPATHDIST, Objects);
	for (INT i = 0; i < Objects.Num(); i++)
	{
		ANavigationPoint* Nav = Objects(i)->GetOwner<ANavigationPoint>();
		if (Nav != NULL && Nav->IsUsableAnchorFor(this) && (bStartPoint ? !Nav->bDestinationOnly : (!Nav->bSourceOnly && !Nav->bMakeSourceOnly)))
		{
			TestPoints.AddPath(Nav, appTrunc((TestLocation - Nav->Location).SizeSquared()));
		}
	}

	// find the closest usable anchor among those found
	if (TestPoints.numPoints == 0)
	{
		return NULL;
	}
	else
	{
		ANavigationPoint* Result;
		if (bStartPoint)
		{
			Result = TestPoints.FindStartAnchor(this);
		}
		else
		{
			Result = TestPoints.FindEndAnchor(this, TestActor, TestLocation, (TestActor != NULL && Controller->AcceptNearbyPath(TestActor)), bOnlyCheckVisible);
		}
		if (Result == NULL)
		{
			FVector RealLoc = Location;
			if (GWorld->FarMoveActor(this, TestLocation, 1, 1))
			{
				// try finding reachable ReachSpec to move towards
				for (INT i = 0; i < Objects.Num() && Result == NULL; i++)
				{
					UReachSpec* Spec = Objects(i)->GetOwner<UReachSpec>();
					if( Spec != NULL && 
						Spec->Start != NULL && 
						*Spec->End != NULL &&
						!Spec->End.Nav()->bSpecialMove &&	// Don't do for special move paths, it breaks special movement code
						!Spec->bDisabled &&
						Spec->supports( Radius, Height, MoveFlags, iMaxFallSpeed ) )
					{
						// make sure we're between the path's endpoints
						const FVector PathDir = (Spec->End->Location - Spec->Start->Location).SafeNormal();
						if (((Spec->Start->Location - Location).SafeNormal() | PathDir) < 0.f && ((Spec->End->Location - Location).SafeNormal() | PathDir) > 0.f)
						{
							// try point on path that's closest to us
							const FVector ClosestPoint = (Spec->Start->Location + (PathDir | (Location - Spec->Start->Location)) * PathDir);
							// see if it's reachable
							if (pointReachable(ClosestPoint, 0))
							{
								// If looking for a start anchor...
								// create a dynamic anchor that we can move from to one of the reachspec ends
								if( bStartPoint )
								{
									// we need StaticLoadClass() here to verify that the native class's defaults have been loaded, since it probably isn't referenced anywhere
									StaticLoadClass(ADynamicAnchor::StaticClass(), NULL, TEXT("Engine.DynamicAnchor"), NULL, LOAD_None, NULL);
									ADynamicAnchor* DynamicAnchor = (ADynamicAnchor*)GWorld->SpawnActor(ADynamicAnchor::StaticClass(), NAME_None, ClosestPoint);
									if (DynamicAnchor != NULL)
									{
										DynamicAnchor->Initialize(Controller, Spec->Start, Spec->End.Nav(), Spec);
										Result = DynamicAnchor;
									}
								}
								// Otherwise, we can't use a dynamic anchor because they only have paths going away from them
								// and there is no way to pathfind to them
								else
								{
									// Select the end point that is closest to the pawn
									FLOAT StartDist = (Spec->Start->Location - Location).SizeSquared();
									FLOAT EndDist	= (Spec->End->Location - Location).SizeSquared();

									Result = (StartDist < EndDist) ? Spec->Start : Spec->End.Nav();
								}
							}
							else
							{
								// if that fails, try endpoints if they are more than MAXPATHDIST away (since they wouldn't have been checked yet)
								FCheckResult Hit(1.0f);
								if ( (Spec->Start->Location - Location).SizeSquared() > MAXPATHDISTSQ
									&& GWorld->SingleLineCheck(Hit, this, Spec->Start->Location, Location, TRACE_World | TRACE_StopAtAnyHit)
									&& Reachable(Spec->Start->Location, Spec->Start) )
								{
									Result = Spec->Start;
								}
								else if ( (Spec->End->Location - Location).SizeSquared() > MAXPATHDISTSQ
									&& GWorld->SingleLineCheck(Hit, this, Spec->End->Location, Location, TRACE_World | TRACE_StopAtAnyHit)
									&& Reachable(Spec->End->Location, Spec->End) )
								{
									// don't allow start anchors if they don't have any paths away
									if (bStartPoint &&
										Spec->End.Nav()->PathList.Num() == 0)
									{
										continue;
									}
									Result = Spec->End.Nav();
								}
							}
						}
					}
				}
			}
			GWorld->FarMoveActor(this, RealLoc, 1, 1);
		}
		if (Result != NULL)
		{
			Dist = (Result->Location - TestLocation).Size();
		}
		// FIXME: if no reachspecs in list, try larger radius?
		return Result;
	}
}

//@fixme - temporary guarantee that only one pathfind occurs at a time
static UBOOL bIsPathFinding = FALSE;
struct FPathFindingGuard
{
	FPathFindingGuard()
	{
		bIsPathFinding = TRUE;
	}

	~FPathFindingGuard()
	{
		bIsPathFinding = FALSE;
	}
};

FLOAT APawn::findPathToward(AActor *goal, FVector GoalLocation, NodeEvaluator NodeEval, FLOAT BestWeight, UBOOL bWeightDetours, INT MaxPathLength, UBOOL bReturnPartial, INT SoftMaxNodes)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_FindPathToward);
	TRACK_DETAILED_PATHFINDING_STATS(this);

	//debug
	DEBUGEMPTYPATHLOG;
	DEBUGEMPTYPATHSTEP;
	DEBUGPATHLOG(FString::Printf(TEXT("%s FindPathToward: %s"),*GetName(), goal != NULL ? *goal->GetPathName() : *GoalLocation.ToString()))

	//@fixme - remove this once the problem has been identified
	// make sure only one path find occurs at once
	if (bIsPathFinding)
	{
		debugf(NAME_Warning,TEXT("findPathToward() called during an existing path find! %s"),*GetName());
		return 0.f;
	}
	FPathFindingGuard PathFindingGuard;

	NextPathRadius = 0.f;

	// If world has no navigation points OR
	// We already tried (and failed) to find an anchor this tick OR 
	// we have no controller
	if( !GWorld->GetFirstNavigationPoint() || (FindAnchorFailedTime == GWorld->GetTimeSeconds()) || !Controller )
	{
		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("- initial abort, %2.1f"),FindAnchorFailedTime))

		// Early fail
		return 0.f;
	}

	UBOOL	bSpecifiedEnd		= (NodeEval == NULL);
	FVector RealLocation		= Location;

	ANavigationPoint *EndAnchor = goal ? goal->SpecifyEndAnchor(this) : NULL;
	FLOAT StartDist	= 0.f;
	UBOOL	bOnlyCheckVisible = (Physics == PHYS_RigidBody) || (goal && !EndAnchor && goal->AnchorNeedNotBeReachable());

	// Grab goal location from actor
	if( goal != NULL )
	{
		GoalLocation = goal->GetDestination( Controller );
	}
	FLOAT EndDist = EndAnchor ? (EndAnchor->Location - GoalLocation).Size() : 0.f;

	// check if my anchor is still valid
	if ( !ValidAnchor() )
	{
		SetAnchor( NULL );
	}
	
	if ( !Anchor || (!EndAnchor && bSpecifiedEnd) )
	{
		if (Anchor == NULL)
		{
			//debug
			DEBUGPATHLOG(FString::Printf(TEXT("- looking for new anchor")))

			SetAnchor( FindAnchor(this, Location, true, false, StartDist) );
			if (Anchor == NULL)
			{
				FindAnchorFailedTime = WorldInfo->TimeSeconds;
				return 0.f;
			}

			//debug
			DEBUGPATHLOG(FString::Printf(TEXT("-+ found new anchor %s"),*Anchor->GetPathName()))

			LastValidAnchorTime = GWorld->GetTimeSeconds();
			LastAnchor = Anchor;
		}
		else
		{
			//debug
			DEBUGPATHLOG(FString::Printf(TEXT("- using existing anchor %s"),*Anchor->GetPathName()))
		}
		if (EndAnchor == NULL && bSpecifiedEnd)
		{
			EndAnchor = FindAnchor(goal, GoalLocation, false, bOnlyCheckVisible, EndDist);
			if ( goal )
			{
				goal->NotifyAnchorFindingResult(EndAnchor, this);
			}
			if ( !EndAnchor )
			{
				return 0.f;
			}
		}
		
		if ( EndAnchor == Anchor )
		{
			// no way to get closer on the navigation network
			Controller->RouteCache_Empty();

			INT PassedAnchor = 0;
			if ( ReachedDestination(Location, Anchor->Location, goal) )
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

			if ( PassedAnchor )
			{
				ANavigationPoint* N = Cast<ANavigationPoint>(goal);
				if (N != NULL)
				{
					Controller->RouteCache_AddItem(N);
				}
			}
			else
			{
				Controller->RouteCache_AddItem(Anchor);
			}
			return (GoalLocation - Location).Size();
		}
	}
	else
	{
		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("- using existing anchors %s and %s"),*Anchor->GetPathName(),*EndAnchor->GetPathName()))
	}

	InitForPathfinding( goal, EndAnchor );
	for (ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
	{
		if (Nav->nextNavigationPoint)
		{
			CONSOLE_PREFETCH(&Nav->nextNavigationPoint->NavOctreeObject);
			CONSOLE_PREFETCH(&Nav->nextNavigationPoint->Cost);
			CONSOLE_PREFETCH(&Nav->nextNavigationPoint->AnchoredPawn);
		}

		if (bCanFly && !Nav->bFlyingPreferred)
		{
			Nav->TransientCost += 4000;
		}

		Nav->ClearForPathFinding();
	}

	if ( EndAnchor )
	{
		Controller->MarkEndPoints(EndAnchor, goal, GoalLocation);
	}

	GWorld->FarMoveActor(this, RealLocation, 1, 1);
	Anchor->visitedWeight = appRound(StartDist);
	if ( bSpecifiedEnd )
	{
		NodeEval = &FindEndPoint;
	}

	// verify the anchors are on the same network
	if( Anchor != NULL && 
		EndAnchor != NULL && 
		Anchor->NetworkID != -1 &&
		EndAnchor->NetworkID != -1 &&
		Anchor->NetworkID != EndAnchor->NetworkID )
	{
		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("Attempt to find path from %s to %s failed because they are on unconnected networks (%i/%i)"),*Anchor->GetFullName(),*EndAnchor->GetFullName(),Anchor->NetworkID,EndAnchor->NetworkID))
		return 0.f;
	}

	//debug
	DEBUGPATHLOG(FString::Printf(TEXT("%s - searching for path from %s to %s"), *GetName(), *Anchor->GetPathName(),*EndAnchor->GetPathName()))

	Controller->eventSetupSpecialPathAbilities();

	if (MaxPathLength == 0)
	{
		MaxPathLength = UCONST_BLOCKEDPATHCOST;
	}

	// allow Controller a chance to override with a cached path
	if (bSpecifiedEnd && Controller->OverridePathTo(EndAnchor, goal, GoalLocation, bWeightDetours, BestWeight))
	{
		return BestWeight;
	}

	Controller->RouteCache_Empty();

	FLOAT Result = 0.f;
	switch( PathSearchType )
	{
		case PST_NewBestPathTo:
			Result = AStarBestPathTo(this,Anchor,EndAnchor,bReturnPartial);
			break;
		case PST_Default:   // FALL THRU
		case PST_Breadth:	// FALL THRU
		default:
			ANavigationPoint* BestDest = BestPathTo(NodeEval, Anchor, &BestWeight, bWeightDetours, MaxPathLength, SoftMaxNodes);
			if ( BestDest )
			{
				//debug
				DEBUGPATHLOG(FString::Printf(TEXT("- found path!")))

				Controller->SetRouteCache( BestDest, StartDist, EndDist );
				Result = BestWeight;
			}
			else
			{
				//debug
				DEBUGPATHLOG(FString::Printf(TEXT("NO PATH!")))

				Result = SecondRouteAttempt(Anchor, EndAnchor, NodeEval, BestWeight, goal, GoalLocation, StartDist, EndDist, MaxPathLength, SoftMaxNodes);
			}
	}

	return Result;
}

UBOOL APawn::GeneratePath()
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_FindPathToward);
	TRACK_DETAILED_PATHFINDING_STATS(this);
#if DO_PATH_PROFILING
	DWORD Time =0;
	CLOCK_CYCLES(Time);
#endif

	//debug
	DEBUGEMPTYPATHLOG;
	DEBUGEMPTYPATHSTEP;
	DEBUGPATHLOG(FString::Printf(TEXT("AGeneratePath: %s"), *GetName()));

	if( bIsPathFinding )
	{
		debugf(NAME_Warning,TEXT("[%f] GeneratePath() called during an existing path find! %s"),WorldInfo->TimeSeconds, *GetName());
	return FALSE;
}
	FPathFindingGuard PathFindingGuard;

	NextPathRadius = 0.f;

	// If world has no navigation points OR
	// We already tried (and failed) to find an anchor this tick OR 
	// we have no controller
	if( !GWorld->GetFirstNavigationPoint() || (FindAnchorFailedTime == GWorld->GetTimeSeconds()) || !Controller )
	{
		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("- initial abort, %2.1f"),FindAnchorFailedTime));

		debugf(NAME_Warning,TEXT("[%f] APawn::GeneratePath... Initial abort! %s %2.1f"),WorldInfo->TimeSeconds, *GetName(),FindAnchorFailedTime);

		// Early fail
		return FALSE;
	}

	FVector RealLocation = Location;
	FLOAT	StartDist = 0.f;
	FLOAT	EndDist = 0.f;

	// check if my anchor is still valid
	if( !ValidAnchor() )
	{
		SetAnchor( NULL );
	}

	if( Anchor == NULL )
	{
		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("- looking for new anchor")));

		SetAnchor( FindAnchor(this, Location, TRUE, FALSE, StartDist) );
		if( Anchor == NULL )
		{
			debugf(NAME_Warning,TEXT("[%f] APawn::GeneratePath... Failed to find anchor! %s"),WorldInfo->TimeSeconds,*GetName());

			FindAnchorFailedTime = WorldInfo->TimeSeconds;
			return FALSE;
		}

		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("-+ found new anchor %s"),*Anchor->GetPathName()));

		LastValidAnchorTime = GWorld->GetTimeSeconds();
		LastAnchor = Anchor;
	}
	else
	{
		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("- using existing anchor %s"),*Anchor->GetPathName()));
	}

	InitForPathfinding( NULL, NULL );
	for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint )
	{
		if (Nav->nextNavigationPoint)
		{
			CONSOLE_PREFETCH(&Nav->nextNavigationPoint->NavOctreeObject);
			CONSOLE_PREFETCH(&Nav->nextNavigationPoint->Cost);
			CONSOLE_PREFETCH(&Nav->nextNavigationPoint->AnchoredPawn);
		}

		Nav->ClearForPathFinding();
	}

	GWorld->FarMoveActor( this, RealLocation, 1, 1 );
	Anchor->visitedWeight = appRound(StartDist);

	//debug
	DEBUGPATHLOG(FString::Printf(TEXT("%s - searching for path from %s"), *GetName(), *Anchor->GetPathName()));

	Controller->eventSetupSpecialPathAbilities();
	
	Controller->RouteCache_Empty();

#if DO_PATH_PROFILING
	UNCLOCK_CYCLES(Time);
	debugf(TEXT(">>>> Path initialization time (FindAnchor): %3.3fms for %s"),Time*GSecondsPerCycle*1000.f,(Controller) ? *Controller->GetName() : TEXT("NULL"));
	Time =0;
	CLOCK_CYCLES(Time);
#endif
	UBOOL Result = GenerateConstrainedPath( this, Anchor );
#if DO_PATH_PROFILING
	UNCLOCK_CYCLES(Time);
	debugf(TEXT("WTFWTWFWWDFLKJSDF  GenerateConstrainedPath took %3.3fms"),Time*GSecondsPerCycle*1000.f);
#endif
		
	return Result;
}

void APawn::ClearConstraints()
{
	UPathConstraint* NextConstraint;
	while (PathConstraintList != NULL)
	{
		NextConstraint = PathConstraintList->NextConstraint;
		PathConstraintList->eventRecycle();
		PathConstraintList = NextConstraint;
	}

	UPathGoalEvaluator* NextGoalEval;
	while (PathGoalList != NULL)
	{
		NextGoalEval = PathGoalList->NextEvaluator;
		PathGoalList->eventRecycle();
		PathGoalList = NextGoalEval;
	}
}
void APawn::AddPathConstraint( UPathConstraint* Constraint )
{
	if( PathConstraintList == NULL )
	{
		PathConstraintList = Constraint;
	}
	else
	{
		UPathConstraint* Cur = PathConstraintList;
		while( Cur->NextConstraint != NULL )
		{
			Cur = Cur->NextConstraint;
		}
		Cur->NextConstraint = Constraint;
	}	
}
void APawn::AddGoalEvaluator( UPathGoalEvaluator* Evaluator )
{
	// ensure this is clear in case this evaluator was cached
	Evaluator->NextEvaluator = NULL;

	if( PathGoalList == NULL )
	{
		PathGoalList = Evaluator;
	}
	else
	{
		UPathGoalEvaluator* Cur = PathGoalList;
		while( Cur->NextEvaluator != NULL )
		{
			Cur = Cur->NextEvaluator;
			checkSlowish(Evaluator != Cur);
		}
		checkSlowish(Evaluator != Cur);
		if(Cur != Evaluator)
		{
			Cur->NextEvaluator = Evaluator;
		}		
	}
}

void APawn::IncrementPathStep( INT Cnt, UCanvas* C )
{
	DEBUGDRAWSTEPPATH( Cnt, 0, C );
}
void APawn::IncrementPathChild( INT Cnt, UCanvas* C )
{
	DEBUGDRAWSTEPPATH( 0, Cnt, C );
}
void APawn::DrawPathStep( UCanvas* C )
{
	DEBUGDRAWSTEPPATH( 0, 0, C );
}
void APawn::ClearPathStep()
{
	DEBUGEMPTYPATHSTEP;
}

ANavigationPoint* ANavigationPoint::SpecifyEndAnchor(APawn* RouteFinder)
{
	return this;
}

UBOOL AActor::AnchorNeedNotBeReachable()
{
	return (Physics == PHYS_Falling) || (Physics == PHYS_Projectile);
}

/** SecondRouteAttempt()
Allows a second try at finding a route.  Not used by base pawn implementation.  See AVehicle implementation, where it attempts to find a route for the dismounted driver
*/
FLOAT APawn::SecondRouteAttempt(ANavigationPoint* Anchor, ANavigationPoint* EndAnchor, NodeEvaluator NodeEval, FLOAT BestWeight, AActor *goal, const FVector& GoalLocation, FLOAT StartDist, FLOAT EndDist, INT MaxPathLength, INT SoftMaxNodes)
{
#if !FINAL_RELEASE
	// Something is most likely wrong if we performed a full navigation search without a valid result
	if (bDisplayPathErrors && (MaxPathLength == 0 || MaxPathLength == UCONST_BLOCKEDPATHCOST) && goal != NULL && goal->GetAPawn() == NULL && Cast<APickupFactory>(goal) == NULL)
	{
		debugf(TEXT("%s (Controller: %s) failed to find path towards %s using start anchor %s end anchor %s"), *GetName(), *Controller->GetName(), *goal->GetPathName(), *Anchor->GetPathName(), *EndAnchor->GetPathName());
		GWorld->GetWorldInfo()->bMapHasPathingErrors = TRUE;
	}
#endif

	return 0.f;
}

void AController::MarkEndPoints(ANavigationPoint* EndAnchor, AActor* Goal, const FVector& GoalLocation)
{
	if (Pawn != NULL)
	{
		Pawn->MarkEndPoints(EndAnchor, Goal, GoalLocation);
	}
}

void APawn::MarkEndPoints(ANavigationPoint* EndAnchor, AActor* Goal, const FVector& GoalLocation)
{	
	if (EndAnchor != NULL)
	{
		EndAnchor->bEndPoint = 1;
	}
}

/* AddPath()
add a path to a sorted path list - sorted by distance
*/
void FSortedPathList::AddPath(ANavigationPoint *node, INT dist)
{
	INT n=0;
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
/** BestPathTo()
* Search for best (or satisfactory) destination in NavigationPoint network, as defined by NodeEval function.  Nodes are visited in the order of least cost.
* A sorted list of nodes is maintained - the first node on the list is visited, and all reachable nodes attached to it (which haven't already been visited
* at a lower cost) are inserted into the list. Returns best next node when NodeEval function returns 1.
* @param NodeEval: function pointer to function used to evaluate nodes
* @param start:  NavigationPoint which is the starting point for the traversal of the navigation network.  
* @param Weight:  starting value defines minimum acceptable evaluated value for destination node.
* @param MaxPathLength (optional, defaults to zero): maximum path cost - if nonzero, after this is exceeded we return the best node we've got (if anything)
* @param SoftMaxNodes (optional, defaults to 200): after this many nodes have been evaluated, this function will return as soon as it finds something that NodeEval gives a rating > 0.0
* @returns recommended next node.
*/
ANavigationPoint* APawn::BestPathTo(NodeEvaluator NodeEval, ANavigationPoint *start, FLOAT *Weight, UBOOL bWeightDetours, INT MaxPathLength, INT SoftMaxNodes)
{
	SCOPE_CYCLE_COUNTER(STAT_PathFinding_BestPathTo);
	TRACK_DETAILED_PATHFINDING_STATS(this);

	//debug
	DEBUGPATHONLY(FlushPersistentDebugLines();)
	DEBUGPATHONLY(UWorld::VerifyNavList(*FString::Printf(TEXT("BESTPATHTO %s %s"), *GetName(), *start->GetFullName() ));)

	ANavigationPoint* currentnode = start;
	ANavigationPoint* LastAdd = currentnode;
	ANavigationPoint* BestDest = NULL;

	INT iRadius = appTrunc(CylinderComponent->CollisionRadius);
	INT iHeight = appTrunc(CylinderComponent->CollisionHeight);
	INT iMaxFallSpeed = appTrunc(GetAIMaxFallSpeed());
	INT moveFlags = calcMoveFlags();
	
	if ( bCanCrouch )
	{
		iHeight = appTrunc(CrouchHeight);
		iRadius = appTrunc(CrouchRadius);
	}

	UBOOL bCheckLength = (MaxPathLength > 0 && MaxPathLength < UCONST_BLOCKEDPATHCOST);

	INT n = 0;
	// While still evaluating a node
	while (currentnode != NULL && (!bCheckLength || currentnode->visitedWeight <= MaxPathLength))
	{
		// Mark as visited
		currentnode->bAlreadyVisited = true;

		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("-> Distance to %s is %d"), *currentnode->GetName(), currentnode->visitedWeight))

		// Evaluate the node
		FLOAT thisWeight = (*NodeEval)(currentnode, this, *Weight);
		// If the weight is better than our last best weight
		if ( thisWeight > *Weight )
		{
			// Keep current node as our dest
			*Weight = thisWeight;
			BestDest = currentnode;
		}
		// If we found a "perfect" node
		if ( *Weight >= 1.f )
		{
			// Return detour check
			return CheckDetour(BestDest, start, bWeightDetours);
		}

		// Otherwise, if we have exceeded the max number of searches
		if (n++ > SoftMaxNodes)
		{
			// If we have found something worth anything
			if (*Weight > 0.f)
			{
				// Return detour check
				return CheckDetour(BestDest, start, bWeightDetours);
			}
		}

		// Search through each path away from this node
		INT nextweight = 0;
		for ( INT i=0; i<currentnode->PathList.Num(); i++ )
		{
			UReachSpec *spec = currentnode->PathList(i);
			if (spec != NULL && *spec->End != NULL)
			{
				//debug
				DEBUGPATHLOG(FString::Printf(TEXT("-> check path from %s to %s with %d, %d, supports? %s, visited? %s"),*spec->Start->GetName(), *spec->End != NULL ? *spec->End->GetName() : TEXT("NULL"), spec->CollisionRadius, spec->CollisionHeight, spec->supports(iRadius, iHeight, moveFlags, iMaxFallSpeed) ? TEXT("True") : TEXT("False"), spec->End.Nav()->bAlreadyVisited ? TEXT("TRUE") : TEXT("FALSE")))
			}

			// If path hasn't already been visited and it supports the pawn
			if ( spec && !spec->bDisabled && *spec->End != NULL && !spec->End.Nav()->bAlreadyVisited && spec->supports(iRadius, iHeight, moveFlags, iMaxFallSpeed) )
			{
				//debug
				DEBUGPATHONLY(DrawDebugLine(currentnode->Location,spec->End->Location,0,64,0,TRUE);)

				// Get the cost for this path
				nextweight = spec->CostFor(this);
				if( bModifyReachSpecCost )
				{
					nextweight += ModifyCostForReachSpec( spec, nextweight );
				}

				// If path is not blocked
				if ( nextweight < UCONST_BLOCKEDPATHCOST )
				{
					ANavigationPoint* endnode = spec->End.Nav();

					// Don't allow zero or negative weight - could create a loop
					if ( nextweight <= 0 )
					{
						//debug
						debugf(TEXT("WARNING!!! - negative weight %d from %s to %s (%s)"), nextweight, *currentnode->GetName(), *endnode->GetName(), *spec->GetName() );

						nextweight = 1;
					}

					// Get total path weight for the next node
					INT newVisit = nextweight + currentnode->visitedWeight; 

					//debug
					DEBUGPATHLOG(FString::Printf(TEXT("--> Path from %s to %s costs %d total %d"), *currentnode->GetName(), *endnode->GetName(), nextweight, newVisit))

					if ( endnode->visitedWeight > newVisit )
					{
						//debug
						DEBUGPATHLOG(FString::Printf(TEXT("--> found better path to %s through %s"), *endnode->GetName(), *currentnode->GetName()))
//						DEBUGPRINTPATHLIST(endnode);

						//debug
						DEBUGPATHLOG(FString::Printf(TEXT("BPT set %s prev path = %s"), *endnode->GetName(), *currentnode->GetName()))

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
						ANavigationPoint* nextnode = LastAdd;
#if PATH_LOOP_TESTING
						TArray<ANavigationPoint*> NavList;
#endif
#if !PS3 && !FINAL_RELEASE
						INT LoopCounter = 0;
#endif
						if ( nextnode->visitedWeight <= newVisit )
						{
							while (nextnode->nextOrdered && (nextnode->nextOrdered->visitedWeight < newVisit))
							{
#if !PS3 && !FINAL_RELEASE
								checkFatalPathFailure(LoopCounter++ <= MAX_LOOP_ITTS, TEXT("Infinite loop caught in BestPathTo!  Try rebuilding paths and see if this goes away."),NULL);
#endif
								nextnode = nextnode->nextOrdered;
#if PATH_LOOP_TESTING
								if (NavList.ContainsItem(nextnode))
								{
									//debug
									debugf(NAME_Warning,TEXT("PATH ERROR!!!! %s BPT [NEXTORDERED LOOP] Encountered a loop trying to insert %s into nextordered"),
										*GetName(), *endnode->GetFullName() );

									PRINTDEBUGPATHLOG(TRUE);
									break;
								}
								else
								{
									NavList.AddItem(nextnode);
								}
#endif
							}
						}
						else
						{
							while ( nextnode->prevOrdered && (nextnode->visitedWeight > newVisit) )
							{
#if !PS3 && !FINAL_RELEASE
								checkFatalPathFailure(LoopCounter++ <= MAX_LOOP_ITTS,TEXT("Infinite loop caught in BestPathTo!  Try rebuilding paths and see if this goes away."),NULL);
#endif

								nextnode = nextnode->prevOrdered;
#if PATH_LOOP_TESTING
								if (NavList.ContainsItem(nextnode))
								{
									//debug
									debugf(NAME_Warning,TEXT("PATH ERROR!!!! %s BPT [PREVORDERED LOOP] Encountered a loop trying to insert %s into prevordered"),
										*GetName(), *endnode->GetFullName() );

									PRINTDEBUGPATHLOG(TRUE);
									break;
								}
								else
								{
									NavList.AddItem(nextnode);
								}
#endif
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
				else
				{
					//debug
					DEBUGPATHONLY(DrawDebugLine(currentnode->Location,spec->End->Location,64,0,0,TRUE);)
				}
			}
		}
		currentnode = currentnode->nextOrdered;
	}

	/* UNCOMMENT TO HAVE FAILED ROUTE FINDING ATTEMPTS DISPLAYED
	if ( !BestDest && (NodeEval == &FindEndPoint) )
	{
		currentnode = start;
		GWorld->PersistentLineBatcher->BatchedLines.Empty();
		if ( currentnode )
		{
			for ( INT i=0; i<currentnode->PathList.Num(); i++ )
			{
				if ( currentnode->PathList(i)->End && currentnode->PathList(i)->End.Nav()->bAlreadyVisited )
					GWorld->PersistentLineBatcher->DrawLine(currentnode->PathList(i)->End->Location, currentnode->Location, FColor(0, 255, 0));
			}
		}
		while ( currentnode )
		{
			for ( INT i=0; i<currentnode->PathList.Num(); i++ )
			{
				if ( currentnode->PathList(i)->End )
				{
					if ( currentnode->PathList(i)->End.Nav()->bAlreadyVisited )
						GWorld->PersistentLineBatcher->DrawLine(currentnode->PathList(i)->End->Location, currentnode->Location, FColor(0, 0, 255));
					else
						GWorld->PersistentLineBatcher->DrawLine(currentnode->PathList(i)->End->Location, currentnode->Location, FColor(255, 0, 0));
				}
			}
			currentnode = currentnode->nextOrdered;
		}
		WorldInfo->bPlayersOnly = 1;
		return NULL;
	}
*/
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
		if (*spec->End != NULL && spec->End.Nav()->visitedWeight < 2.f * MAXPATHDIST && !spec->End.Nav()->BlockedByVehicle())
		{
			UReachSpec *Return = spec->End.Nav()->GetReachSpecTo(Anchor);
			if (Return != NULL && !Return->IsForced() && !Return->IsProscribed() && !Return->IsA(UAdvancedReachSpec::StaticClass()))
			{
				spec->End.Nav()->LastDetourWeight = spec->End.Nav()->eventDetourWeight(this,spec->End.Nav()->visitedWeight);
				if ( spec->End.Nav()->LastDetourWeight > DetourWeight )
				{
					DetourDest = spec->End.Nav();
				}
			}
		}
	}
	if ( !DetourDest )
	{
		return BestDest;
	}

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
	if( !EndPath )
	{
		return;
	}

	// if our goal is a pickup factory that's been replaced, redirect to the replacement
	APickupFactory* PickupGoal = EndPath->GetAPickupFactory();
	if (PickupGoal != NULL)
	{
		while (PickupGoal->ReplacementFactory != NULL)
		{
			PickupGoal = PickupGoal->ReplacementFactory;
		}
		RouteGoal = PickupGoal;
	}

	RouteDist = EndPath->visitedWeight + EndDist;

	// reverse order of linked node list
	EndPath->nextOrdered = NULL;
#if PATH_LOOP_TESTING
	TArray<ANavigationPoint*> NavList;
#endif
	while ( EndPath->previousPath)
	{
		//debug
		DEBUGPATHONLY(DrawDebugLine(EndPath->Location,EndPath->previousPath->Location,0,255,0,TRUE);)

		EndPath->previousPath->nextOrdered = EndPath;
		EndPath = EndPath->previousPath;

#if PATH_LOOP_TESTING
		if (NavList.ContainsItem(EndPath))
		{
			//debug
			debugf(NAME_Warning,TEXT("PATH ERROR!!!! SetRouteCache [PREVPATH LOOP] %s Encountered loop [%s to %s] "),
					*Pawn->GetName(),*Pawn->Anchor->GetName(),*EndPath->GetName());
			PRINTDEBUGPATHLOG(TRUE);
		}
		else
		{
			NavList.AddItem(EndPath);
		}
#endif
	}

	// if the pawn is on the start node, then the first node in the path should be the next one
	if ( Pawn && (StartDist > 0.f) )
	{
		// if pawn not on the start node, check if second node on path is a better destination
		if ( EndPath->nextOrdered )
		{
			TArray<FNavigationOctreeObject*> Objects;
			GWorld->NavigationOctree->PointCheck(Pawn->Location, Pawn->GetCylinderExtent(), Objects);
			UBOOL bAlreadyOnPath = false;
			// if already on a reachspec to a further node on the path, then keep going
			for (INT i = 0; i < Objects.Num() && !bAlreadyOnPath; i++)
			{
				UReachSpec* Spec = Objects(i)->GetOwner<UReachSpec>();
				if (Spec != NULL)
				{
					for (ANavigationPoint* NextPath = EndPath->nextOrdered; NextPath != NULL && !bAlreadyOnPath; NextPath = NextPath->nextOrdered)
					{
						if (Spec->End == NextPath || (Spec->Start == NextPath && Spec->End.Nav() != NULL && Spec->End.Nav()->GetReachSpecTo(Spec->Start) != NULL))
						{
							bAlreadyOnPath = true;
							EndPath = NextPath;
						}
					}
				}
			}
			if (!bAlreadyOnPath)
			{
				// check if pawn is closer to second node than first node is
				FLOAT TwoDist = (Pawn->Location - EndPath->nextOrdered->Location).Size();
				FLOAT PathDist = (EndPath->Location - EndPath->nextOrdered->Location).Size();
				FLOAT MaxDist = 0.75f * MAXPATHDIST;
				if ( EndPath->nextOrdered->IsA(AVolumePathNode::StaticClass()) )
				{
					MaxDist = ::Max(MaxDist,EndPath->nextOrdered->CylinderComponent->CollisionRadius);
				}
				if (TwoDist < MaxDist && TwoDist < PathDist)
				{
					// make sure second node is reachable
					FCheckResult Hit(1.f);
					GWorld->SingleLineCheck( Hit, this, EndPath->nextOrdered->Location, Pawn->Location, TRACE_World|TRACE_StopAtAnyHit );
					if ( !Hit.Actor	&& Pawn->actorReachable(EndPath->nextOrdered, 1, 1) )
					{
						//debugf(TEXT("Skipping Anchor"));
						EndPath = EndPath->nextOrdered;
					}
				}
			}
		}
	}
	else if( EndPath->nextOrdered )
	{
		EndPath = EndPath->nextOrdered;
	}

	// place all of the path into the controller route cache
#if PATH_LOOP_TESTING
	ANavigationPoint* EP = EndPath;
#endif
	while (EndPath != NULL)
	{
#if PATH_LOOP_TESTING
		if (RouteCache.ContainsItem(EndPath))
		{
			debugf(NAME_Warning,TEXT("PATH ERROR!!!! SetRouteCache [NEXTORDERED LOOP] %s Encountered loop [%s to %s] "),
				*Pawn->GetName(),*Pawn->Anchor->GetName(),*EP->GetName());
			PRINTDEBUGPATHLOG(TRUE);
			break;
		}
#endif
		RouteCache_AddItem( EndPath );
		EndPath = EndPath->nextOrdered;
	}

	if( Pawn && RouteCache.Num() > 1 )
	{
		ANavigationPoint *FirstPath = RouteCache(0);
		UReachSpec* NextSpec = NULL;
		if( FirstPath )
		{
			ANavigationPoint *SecondPath = RouteCache(1);
			if( SecondPath )
			{
				NextSpec = FirstPath->GetReachSpecTo(SecondPath);
			}
		}
		if ( NextSpec )
		{
			Pawn->NextPathRadius = NextSpec->CollisionRadius;
		}
		else
		{
			Pawn->NextPathRadius = 0.f;
		}
	}
}

/** initializes us with the given user and creates ReachSpecs to connect ourselves to the given endpoints,
 * using the given ReachSpec as a template
 * @param InUser the Controller that will be using us for navigation
 * @param Point1 the first NavigationPoint to connect to
 * @param Point2 the second NavigationPoint to connect to
 * @param SpecTemplate the ReachSpec to use as a template for the ReachSpecs we create
 */
void ADynamicAnchor::Initialize(AController* InUser, ANavigationPoint* Point1, ANavigationPoint* Point2, UReachSpec* SpecTemplate)
{
	checkSlow(InUser != NULL && InUser->Pawn != NULL && Point1 != NULL && Point2 != NULL && SpecTemplate != NULL);

	CurrentUser = InUser;
	INT NewRadius = appTrunc(InUser->Pawn->CylinderComponent->CollisionRadius);
	INT NewHeight = appTrunc(InUser->Pawn->CylinderComponent->CollisionHeight);
	
	// construct reachspecs between ourselves and the two endpoints of the given reachspec
	// copy its path properties (collision size, reachflags, etc)

	InitHelper( this, Point1, NewHeight, NewRadius, SpecTemplate );
	InitHelper( this, Point2, NewHeight, NewRadius, SpecTemplate );

	MaxPathSize.Height = InUser->Pawn->CylinderComponent->CollisionHeight;
	MaxPathSize.Radius = InUser->Pawn->CylinderComponent->CollisionRadius;

	// set our collision size in between that of the endpoints to make sure the AI doesn't get stuck beneath us in the case of one or both of the endpoints have a large size
	SetCollisionSize( Max<FLOAT>(Point1->CylinderComponent->CollisionRadius, Point2->CylinderComponent->CollisionRadius),
						Max<FLOAT>(Point1->CylinderComponent->CollisionHeight, Point2->CylinderComponent->CollisionHeight) );
	// add ourselves to the navigation octree so any other Pawns that need an Anchor here can find us instead of creating another
	AddToNavigationOctree();

	// Get base so we can move with pawn if atop an interp actor
	{
		// not using find base, because don't want to fail if LD has navigationpoint slightly interpenetrating floor
		FCheckResult Hit(1.f);
		FVector CollisionSlice = InUser->Pawn->GetCylinderExtent();
		CollisionSlice.Z = 1.f;
		// and use this node's smaller collision radius if possible
		if( CylinderComponent->CollisionRadius < CollisionSlice.X )
		{
			CollisionSlice.X = CollisionSlice.Y = CylinderComponent->CollisionRadius;
		}
		// check for placement
		GWorld->SingleLineCheck( Hit, InUser->Pawn, Location - FVector(0,0, 4.f * CylinderComponent->CollisionHeight), Location, TRACE_AllBlocking, CollisionSlice );
		SetBase(Hit.Actor, Hit.Normal);
	}
}

void ADynamicAnchor::InitHelper( ANavigationPoint* Start, ANavigationPoint* End, INT NewHeight, INT NewRadius, UReachSpec* SpecTemplate )
{
	//@FIXME: need to be able to copy the other reachspec but passing it as a template causes it to be set as the ObjectArchetype, which screws up GCing streaming levels
	//UReachSpec* NewSpec = ConstructObject<UReachSpec>(SpecTemplate->GetClass(), GetOuter(), NAME_None, 0, SpecTemplate, INVALID_OBJECT);
	UReachSpec* NewSpec = ConstructObject<UReachSpec>(SpecTemplate->GetClass(), GetOuter(), NAME_None, 0 );
	NewSpec->reachFlags			= SpecTemplate->reachFlags;
	NewSpec->MaxLandingVelocity = SpecTemplate->MaxLandingVelocity;
	NewSpec->PathColorIndex		= SpecTemplate->PathColorIndex;
	NewSpec->bCanCutCorners		= SpecTemplate->bCanCutCorners;

	NewSpec->Start	  = Start;
	NewSpec->End	  = End;
	NewSpec->End.Guid = *End->GetGuid();
	NewSpec->Distance = appTrunc((NewSpec->End->Location - NewSpec->Start->Location).Size());
	NewSpec->bAddToNavigationOctree = FALSE; // unnecessary, since encompassed by ConnectTo
	NewSpec->bCanCutCorners			= FALSE;
	NewSpec->bCheckForObstructions	= FALSE;
	NewSpec->CollisionRadius		= NewRadius;
	NewSpec->CollisionHeight		= NewHeight;

	NewSpec->Start->PathList.AddItem(NewSpec);
}

void ADynamicAnchor::PostScriptDestroyed()
{
	// For each reach spec we have
	for( INT PathIdx = 0; PathIdx < PathList.Num(); PathIdx++ )
	{
		ANavigationPoint* Dest = PathList(PathIdx)->End.Nav();
		if( Dest != NULL )
		{
			// Find paths from the dest back to this dynamic anchor and remove it
			for( INT DestIdx = Dest->PathList.Num()-1; DestIdx >= 0; DestIdx--  )
			{
				if( *Dest->PathList(DestIdx)->End == this )
				{
					Dest->PathList.Remove( DestIdx );
					break;
				}
			}
		}
	}

	Super::PostScriptDestroyed();
}

void ADynamicAnchor::TickSpecial(FLOAT DeltaSeconds)
{
	if ( CurrentUser == NULL || CurrentUser->bDeleteMe || CurrentUser->Pawn == NULL || ( CurrentUser->Pawn->Anchor != this && CurrentUser->MoveTarget != this &&
																						PathList.FindItemIndex(CurrentUser->CurrentPath) == INDEX_NONE ) )
	{
		// try to find another user
		CurrentUser = NULL;
		for (AController* C = GWorld->GetFirstController(); C != NULL && CurrentUser == NULL; C = C->NextController)
		{
			if (C->Pawn != NULL && (C->Pawn->Anchor == this || C->MoveTarget == this && PathList.FindItemIndex(C->CurrentPath) == INDEX_NONE))
			{
				CurrentUser = C;
			}
		}
		// destroy if not in use
		if (CurrentUser == NULL)
		{
			GWorld->DestroyActor(this);
		}
	}
}
/************************************************************************/
/* Pathnode path constraints...                                         */
/************************************************************************/
UBOOL UPathConstraint::EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost )
{
	return TRUE;
}

UBOOL UPath_TowardGoal::EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost )
{
	INT AddedCost = appTrunc((Spec->End->Location - GoalActor->Location).Size());
	out_HeuristicCost += AddedCost;

	//debug
	DEBUGREGISTERCOST( *Spec->End, *GetClass()->GetName(), AddedCost );

	return TRUE;
}

UBOOL UPath_TowardPoint::EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost )
{
	INT AddedCost = appTrunc((Spec->End->Location - GoalPoint).Size());
	out_HeuristicCost += AddedCost;

	//debug
	DEBUGREGISTERCOST( *Spec->End, *GetClass()->GetName(), AddedCost );

	return TRUE;
}

UBOOL UPath_AlongLine::EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost )
{
	// scale cost to make paths away from the goal direction more expensive
	const FLOAT DotToGoal = Clamp<FLOAT>(1.f - (Spec->GetDirection() | Direction),0.1f,2.f);
	// Additional cost based on the distance to goal, and based on the distance travelled
	INT AddedCost = appTrunc(Spec->Distance * DotToGoal);
	out_HeuristicCost += AddedCost;

	//debug
	DEBUGREGISTERCOST( *Spec->End, *GetClass()->GetName(), AddedCost );

	return TRUE;
}


UBOOL UPath_WithinTraversalDist::EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost )
{
	if(out_PathCost + Spec->Start->visitedWeight  > MaxTraversalDist)
	{
		if(bSoft)
		{
			out_PathCost += appTrunc(SoftStartPenalty + (out_PathCost - MaxTraversalDist));
			return TRUE;
		}
		else
		{
			return FALSE;
		}		
	}
	return TRUE;
}

UBOOL UPath_WithinDistanceEnvelope::EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost )
{
	if(Pawn == NULL || Spec->End.Nav() == NULL || Spec->Start == NULL)
	{
		return TRUE;
	}
	FLOAT DistToEnvTestPt = (EnvelopeTestPoint - Spec->End->Location).Size();


	// figure out distance from envelope threshold
	FLOAT HalfEnv = ((MaxDistance - MinDistance) * 0.5f);
	FLOAT EnvelopeCenter = MinDistance + HalfEnv;
	FLOAT DistOutsideEnvelope = Max<FLOAT>(0.f, Abs<FLOAT>(DistToEnvTestPt-EnvelopeCenter) - HalfEnv);

	if(DistOutsideEnvelope > 0)
	{
		if(!bSoft)
		{
			FLOAT DistSq = (Spec->Start->Location - EnvelopeTestPoint).SizeSquared();
			UBOOL bStartInside = (DistSq < MaxDistance && DistSq > MinDistance);
			DistSq = (Spec->End->Location - EnvelopeTestPoint).SizeSquared();
			UBOOL bEndInside = (DistSq < MaxDistance && DistSq > MinDistance);
			// if bOnlyThrowOutNodesThatLeaveEnvelope is off, make sure both end points are outside envelope before throwing the node out
			if(!bOnlyThrowOutNodesThatLeaveEnvelope || (bStartInside && !bEndInside) )
			{
				return FALSE;
			}
		}
		else
		{
			out_PathCost += appTrunc(SoftStartPenalty + DistOutsideEnvelope);
		}
	}

	return TRUE;
}

/**
* traverse back along the previous nodes for this path and add up the distance between specs of the type we care about
*/
UBOOL UPath_MinDistBetweenSpecsOfType::IsNodeWithinMinDistOfSpecInPath(ANavigationPoint* Node)
{
	INT Distance = 0;
	ANavigationPoint* CurrentNode = Node;
	while(CurrentNode->previousPath != NULL)
	{
		UReachSpec* Path = CurrentNode->previousPath->GetReachSpecTo(CurrentNode);

		checkSlowish(Path);
		Distance += Path->Distance;

		// if we exceed our mindist stop checking because we don't care any more
		if(Distance > MinDistBetweenSpecTypes)
		{
			return FALSE;
		}

		// if we just hit another spec of the class we're looking for, then that is the distance we need to check against
		// so return true if we're under the minimum
		if(Path->GetClass() == ReachSpecClass)
		{
			return (Distance<appTrunc(MinDistBetweenSpecTypes));
		}

		CurrentNode = CurrentNode->previousPath;
	}

	// if we got here it means we didn't ever find a spec of the specified class
	return FALSE;
}

UBOOL UPath_MinDistBetweenSpecsOfType::EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost )
{
	if(Spec->GetClass() == ReachSpecClass)
	{
		// if it's too close to the init location, or it's too close in its path to another spec of the type we're looking for
		// penalize it heavily (don't block it in case there is no other path)
		if((!InitLocation.IsNearlyZero() && (InitLocation - Spec->Start->Location).SizeSquared() < MinDistBetweenSpecTypes * MinDistBetweenSpecTypes) ||
			IsNodeWithinMinDistOfSpecInPath(Spec->Start))
		{
			out_PathCost += 10000;
		}
	}

	return TRUE;
}

void UPath_AvoidInEscapableNodes::CachePawnReacFlags(APawn* Pawn)
{
	Radius	= Pawn->bCanCrouch ? appTrunc(Pawn->CrouchRadius) : appTrunc(Pawn->CylinderComponent->CollisionRadius);
	Height = Pawn->bCanCrouch ? appTrunc(Pawn->CrouchHeight) : appTrunc(Pawn->CylinderComponent->CollisionHeight);
	MaxFallSpeed = appTrunc(Pawn->GetAIMaxFallSpeed());
	MoveFlags = Pawn->calcMoveFlags();
}

UBOOL UPath_AvoidInEscapableNodes::EvaluatePath( UReachSpec* Spec, APawn* Pawn, INT& out_PathCost, INT& out_HeuristicCost )
{
	// ensure that there is at least one path leaving the end point which the pawn can use
	ANavigationPoint* End = Spec->End.Nav();

	if(End == NULL)
	{
		return FALSE;
	}

	UReachSpec* CurSpec = NULL;
	for(INT Idx=0;Idx<End->PathList.Num();Idx++)
	{
		CurSpec = End->PathList(Idx);
		if(CurSpec == NULL || Spec == CurSpec || CurSpec->CostFor(Pawn) >= UCONST_BLOCKEDPATHCOST)
		{
			continue;
		}

		if(CurSpec->supports(Radius,Height,MoveFlags,MaxFallSpeed))
		{
			return TRUE;
		}
	}

	return FALSE;
}

UBOOL UPathGoalEvaluator::InitialAbortCheck( ANavigationPoint* Start, APawn* Pawn )
{
	if( NextEvaluator != NULL )
	{
		return NextEvaluator->InitialAbortCheck( Start, Pawn );
	}
	return FALSE;
}

UBOOL UPathGoalEvaluator::EvaluateGoal(ANavigationPoint*& PossibleGoal, APawn* Pawn)
{
	if (NextEvaluator != NULL)
	{
		return NextEvaluator->EvaluateGoal(PossibleGoal, Pawn);
	}
	return FALSE;
}

UBOOL UPathGoalEvaluator::DetermineFinalGoal( ANavigationPoint*& out_GoalNav )
{
	if( NextEvaluator != NULL )
	{
		return NextEvaluator->DetermineFinalGoal( out_GoalNav );
	}
	return GeneratedGoal != NULL;
}

void UPathGoalEvaluator::NotifyExceededMaxPathVisits( ANavigationPoint* BestGuess )
{
	GeneratedGoal = BestGuess;
}


UBOOL UGoal_AtActor::InitialAbortCheck( ANavigationPoint* Start, APawn* Pawn )
{
	//debug
	DEBUGPATHLOG(FString::Printf(TEXT("UGoal_AtActor::InitalAbortCheck... Attempt to find path from %s to %s"),*Start->GetFullName(),*GoalActor->GetFullName()));

	if( Start == GoalActor )
	{
		debugf(TEXT("Attempt to find path from %s to %s failed because start is the goal"),*Start->GetFullName(),*GoalActor->GetFullName());

		return TRUE;
	}

	// verify the anchors are on the same network
	ANavigationPoint* GoalNav = Cast<ANavigationPoint>(GoalActor);
	if( Start != NULL && Start->IsOnDifferentNetwork( GoalNav ) )
	{
		//debug
		DEBUGPATHLOG(FString::Printf(TEXT("Attempt to find path from %s to %s failed because they are on unconnected networks (%i/%i)"),*Start->GetFullName(),*GoalNav->GetFullName(),Start->NetworkID,GoalNav->NetworkID));

		return TRUE;
	}

	return Super::InitialAbortCheck( Start, Pawn );
}

UBOOL UGoal_AtActor::EvaluateGoal(ANavigationPoint*& PossibleGoal, APawn* Pawn)
{
	if( PossibleGoal == GoalActor )
	{
		return TRUE;
	}

	// If close enough in height
	if( Abs(PossibleGoal->Location.Z - GoalActor->Location.Z) < 32.f )
	{
		// Check to see if we are close enough in distance
		const FLOAT DistSq2D = (GoalActor->Location - PossibleGoal->Location).SizeSquared2D();
		if( DistSq2D <= GoalDist*GoalDist )
		{
			return TRUE;
		}
	}

	// If keeping track of partial paths and node is not the start
	if( bKeepPartial && PossibleGoal->bestPathWeight > 0 )
	{
		// If we don't have a partial path list already OR
		// The possible nav has more "goodness" than our current parital goal
		if( GeneratedGoal == NULL || 
			(PossibleGoal->bestPathWeight - PossibleGoal->visitedWeight) < 
			(GeneratedGoal->bestPathWeight - GeneratedGoal->visitedWeight) )
		{
			// Keep nav as our partial goal
			GeneratedGoal = PossibleGoal;
		}		
	}

	return FALSE;
}

void UGoal_AtActor::NotifyExceededMaxPathVisits( ANavigationPoint* BestGuess )
{
	// only save off goal if we want to allow partial paths
	if( bKeepPartial )
	{
		GeneratedGoal = BestGuess;
	}
}

UBOOL UGoal_Null::EvaluateGoal(ANavigationPoint*& PossibleGoal, APawn* Pawn)
{
	if( PossibleGoal->bestPathWeight > 0 )
	{
		// If we don't have a partial path list already OR
		// The possible nav has more "goodness" than our current parital goal
		if( GeneratedGoal == NULL || 
			(PossibleGoal->bestPathWeight - PossibleGoal->visitedWeight) < 
			(GeneratedGoal->bestPathWeight - GeneratedGoal->visitedWeight) )
		{
			// Keep nav as our partial goal
			GeneratedGoal = PossibleGoal;
		}		
	}

	return FALSE;
}
