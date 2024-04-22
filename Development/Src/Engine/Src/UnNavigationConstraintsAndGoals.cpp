/*=============================================================================
	UnNavigationMesh.cpp:

  UNavigationMesh and subclass functions

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "UnPath.h"
#include "EngineAIClasses.h"
#include "DebugRenderSceneProxy.h"
#include "GenericOctree.h"


// navmesh constraints
IMPLEMENT_CLASS(UNavMeshPathConstraint);
IMPLEMENT_CLASS(UNavMeshPath_Toward);
IMPLEMENT_CLASS(UNavMeshPath_AlongLine);
IMPLEMENT_CLASS(UNavMeshPath_WithinTraversalDist);
IMPLEMENT_CLASS(UNavMeshPath_WithinDistanceEnvelope);
IMPLEMENT_CLASS(UNavMeshPath_MinDistBetweenSpecsOfType);
IMPLEMENT_CLASS(UNavMeshPath_EnforceTwoWayEdges);
IMPLEMENT_CLASS(UNavMeshPath_SameCoverLink);
IMPLEMENT_CLASS(UNavMeshPathGoalEvaluator);
IMPLEMENT_CLASS(UNavMeshGoal_At);
IMPLEMENT_CLASS(UNavMeshGoal_Null);
IMPLEMENT_CLASS(UNavMeshGoal_Random);
IMPLEMENT_CLASS(UNavMeshGoal_ClosestActorInList);
IMPLEMENT_CLASS(UNavMeshGoal_PolyEncompassesAI);
IMPLEMENT_CLASS(UNavMeshGoal_GenericFilterContainer);
IMPLEMENT_CLASS(UNavMeshGoal_Filter);
IMPLEMENT_CLASS(UNavMeshGoal_WithinDistanceEnvelope);


IMPLEMENT_CLASS(UNavMeshGoalFilter_PolyEncompassesAI);



/************************************************************************/
/* NavMesh path constraints...                                          */
/************************************************************************/
/**
 * EvaluatePath - this function is called for every A* successor edge.  This gives constraints a chance
 * to both modify the heuristic cost (h), the computed actual cost (g), as well as deny use of this edge altogether
 * @param Edge - the successor candidate edge
 * @param PredecessorEdge - Edge we traversed from to reach `Edge` 
 * @param SrcPoly - the poly we are expanding from (e.g. the poly from which we want to traverse the Edge)
 * @param DestPoly - The poly we're trying to traverse to (from SrcPoly)
 * @param PathParams - the cached pathing parameters of the pathing entity 
 * @param out_PathCost - the computed path cost of this edge (the current value is supplied as input, and can be modified in this function) 
 *                       (this constitutes G of the pathfinding heuristic function F=G+H)
 * @param out_HeuristicCost - the heuristic cost to be applied to this edge (the current heuristic is supplied as input, and can be modified in this function)
 *                          (this constitutes H of the pathfindign heuristic function F=G+H)
 * @param EdgePoint - the point on the edge that was used for cost calculations, useful starting point if computing distance to something
 * @return - TRUE if this edge is a valid successor candidate and should be added to the open list, FALSE if it should be thrown out
 */
UBOOL UNavMeshPathConstraint::EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint )
{
	checkSlowish(FALSE && "Base version of EvaluatePath called on PathConstraint!  (un-implimented EvaluatePath?)");
	return TRUE;
}

/**
 * EvaluatePath - this function is called for every A* successor edge.  This gives constraints a chance
 * to both modify the heuristic cost (h), the computed actual cost (g), as well as deny use of this edge altogether
 * @param Edge - the successor candidate edge
 * @param PredecessorEdge - Edge we traversed from to reach `Edge` 
 * @param SrcPoly - the poly we are expanding from (e.g. the poly from which we want to traverse the Edge)
 * @param DestPoly - The poly we're trying to traverse to (from SrcPoly)
 * @param PathParams - the cached pathing parameters of the pathing entity 
 * @param out_PathCost - the computed path cost of this edge (the current value is supplied as input, and can be modified in this function) 
 *                       (this constitutes G of the pathfinding heuristic function F=G+H)
 * @param out_HeuristicCost - the heuristic cost to be applied to this edge (the current heuristic is supplied as input, and can be modified in this function)
 *                          (this constitutes H of the pathfindign heuristic function F=G+H)
 * @param EdgePoint - the point on the edge that was used for cost calculations, useful starting point if computing distance to something
 * @return - TRUE if this edge is a valid successor candidate and should be added to the open list, FALSE if it should be thrown out
 */
UBOOL UNavMeshPath_Toward::EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint )
{	
	INT AddedCost = 0;
	if( GoalActor != NULL )
	{
		AddedCost = appTrunc((GoalActor->Location-EdgePoint).Size());
	}
	else
	{
		AddedCost = appTrunc((EdgePoint-GoalPoint).Size());
	}	
	out_HeuristicCost += AddedCost;

	if( bBiasAgainstHighLevelPath )
	{
		APylon* OwningPylon = Edge->NavMesh->GetPylon();
		if( OwningPylon == NULL || !OwningPylon->bPylonInHighLevelPath )
		{
			out_HeuristicCost += OutOfHighLevelPathBias;
		}
	}

	return TRUE;
}

/**
 * EvaluatePath - this function is called for every A* successor edge.  This gives constraints a chance
 * to both modify the heuristic cost (h), the computed actual cost (g), as well as deny use of this edge altogether
 * @param Edge - the successor candidate edge
 * @param SrcPoly - the poly we are expanding from (e.g. the poly from which we want to traverse the Edge)
 * @param DestPoly - The poly we're trying to traverse to (from SrcPoly)
 * @param PathParams - the cached pathing parameters of the pathing entity 
 * @param out_PathCost - the computed path cost of this edge (the current value is supplied as input, and can be modified in this function) 
 *                       (this constitutes G of the pathfinding heuristic function F=G+H)
 * @param out_HeuristicCost - the heuristic cost to be applied to this edge (the current heuristic is supplied as input, and can be modified in this function)
 *                          (this constitutes H of the pathfindign heuristic function F=G+H)
 * @param EdgePoint - the point on the edge that was used for cost calculations, useful starting point if computing distance to something
 * @return - TRUE if this edge is a valid successor candidate and should be added to the open list, FALSE if it should be thrown out
 */
UBOOL UNavMeshPath_AlongLine::EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint )
{
	// scale cost to make paths away from the goal direction more expensive
	FVector SrcToDest = DestPoly->GetPolyCenter()-SrcPoly->GetPolyCenter();
	const FLOAT	SrcToDestDist = SrcToDest.Size();
	SrcToDest /= SrcToDestDist;
	const FLOAT DotToGoal = Clamp<FLOAT>(1.f - (SrcToDest | Direction),0.1f,2.f);
	// Additional cost based on the distance to goal, and based on the distance travelled
	INT AddedCost = appTrunc(SrcToDestDist * DotToGoal);
	out_HeuristicCost += AddedCost;

	return TRUE;
}

/**
 * EvaluatePath - this function is called for every A* successor edge.  This gives constraints a chance
 * to both modify the heuristic cost (h), the computed actual cost (g), as well as deny use of this edge altogether
 * @param Edge - the successor candidate edge
 * @param PredecessorEdge - Edge we traversed from to reach `Edge` 
 * @param SrcPoly - the poly we are expanding from (e.g. the poly from which we want to traverse the Edge)
 * @param DestPoly - The poly we're trying to traverse to (from SrcPoly)
 * @param PathParams - the cached pathing parameters of the pathing entity 
 * @param out_PathCost - the computed path cost of this edge (the current value is supplied as input, and can be modified in this function) 
 *                       (this constitutes G of the pathfinding heuristic function F=G+H)
 * @param out_HeuristicCost - the heuristic cost to be applied to this edge (the current heuristic is supplied as input, and can be modified in this function)
 *                          (this constitutes H of the pathfindign heuristic function F=G+H)
 * @param EdgePoint - the point on the edge that was used for cost calculations, useful starting point if computing distance to something
 * @return - TRUE if this edge is a valid successor candidate and should be added to the open list, FALSE if it should be thrown out
 */
UBOOL UNavMeshPath_WithinTraversalDist::EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint )
{
	INT PredecessorCost = 0;
	if ( PredecessorEdge != NULL )
	{
		PredecessorCost = PredecessorEdge->VisitedPathWeight;
	}

	if( out_PathCost + PredecessorCost > MaxTraversalDist)
	{
//		warnf( TEXT( "UNavMeshPath_WithinTraversalDist %s %s %d, %d, %f"), *SrcPoly->GetPolyCenter().ToString(), *GetName(), out_PathCost,SrcPoly->visitedWeight,MaxTraversalDist );
//		GWorld->GetWorldInfo()->DrawDebugLine( SrcPoly->GetPolyCenter()+FVector(0,0,128), SrcPoly->GetPolyCenter(), 255, 0, 0, TRUE );

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


/** 
 * sets up internal vars for path searching, and will early out if something fails
 * @param Handle - handle we're initializing for
 * @param PathParams - pathfinding parameter packet
 * @return - whether or not we should early out form this search
 */
UBOOL UNavMeshPathGoalEvaluator::InitializeSearch( UNavigationHandle* Handle, const FNavMeshPathParams& PathParams )
{
	if( NextEvaluator != NULL )
	{
		return NextEvaluator->InitializeSearch( Handle, PathParams);
	}

	if( !Handle->GetPylonAndPolyFromPos(PathParams.SearchStart,PathParams.MinWalkableZ,Handle->AnchorPylon,Handle->AnchorPoly) )
	{
		Handle->SetPathError(PATHERROR_STARTPOLYNOTFOUND);
		return FALSE;
	}

	return TRUE;
}

/**
 * Called each time a node is popped off the working set to determine
 * whether or not we should finish the search (e.g. did we find the node we're looking for)
 * @param PossibleGoal - the chosen (cheapest) successor from the open list
 * @param PathParams   - the cached pathfinding params for the pathing entity
 * @param out_GenGoal  - the poly we should consider the 'goal'.  (Normally PossibleGOal when this returns true, but doesn't have to be)
 * @return - TRUE indicates we have found the node we're looking for and we should stop the search
 */
UBOOL UNavMeshPathGoalEvaluator::EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal )
{
	checkSlowish(FALSE && "A goal evaluator has not implemented EvaluateGoal! this should not be the case.");
	return FALSE;
}
/**
 * after the search is over this is called to allow the goal evaluator to determine the final result from the search.
 * this is useful if your search is gathering lots of nodes and you need to pick the most fit after your search is complete
 * @param out_GenGoal - the edge that got us to our final goal
 * @param out_DestACtor - custom user usable actor output pointer
 * @param out_DestItem  - custom user usable integter output 
 * @return - if no final goal could be determined this should return false inidcating failure
 */
UBOOL UNavMeshPathGoalEvaluator::DetermineFinalGoal( PathCardinalType& out_GenGoal, AActor** out_DestActor, INT* out_DestItem )
{
	if( NextEvaluator != NULL )
	{
		return NextEvaluator->DetermineFinalGoal( out_GenGoal, out_DestActor, out_DestItem );
	}
	if( out_DestActor != NULL )
	{
		*out_DestActor = NULL;
	}
	if( out_DestItem != NULL )
	{
		*out_DestItem = -1;
	}

	return out_GenGoal != NULL;
}

/**
 * called when we have hit our upper bound for path iterations, allows 
 * evaluators to handle this case specifically to their needs
 * @param BestGuess - last visited node from the open list, which is our "best guess"
 * @param out_GenGoal - current generated goal
 */
void UNavMeshPathGoalEvaluator::NotifyExceededMaxPathVisits( PathCardinalType BestGuess, PathCardinalType& out_GenGoal )
{
	out_GenGoal = BestGuess;
}

/**
 * walks the previousPath chain back and saves out edges into the handle's pathcache for that handle to follow
 * @param StartingPoly - the Polygon we are walking backwards toward
 * @param GoalPoly     - the polygon to begin walking backwards from
 * @param Handle	   - the handle to save the path out to 
 * @param GoalEdge     - the edge that lead us to the goal poly
 */
void UNavMeshPathGoalEvaluator::SaveResultingPath( FNavMeshPolyBase* StartingPoly, FNavMeshPolyBase* GoalPoly, UNavigationHandle* Handle, PathCardinalType GoalEdge )
{
	FNavMeshEdgeBase* CurrentEdge = GoalEdge;

#if !PS3 && !FINAL_RELEASE
	INT LoopCounter = 0;
#endif

	FNavMeshEdgeBase* PrevEdge = (GoalEdge == NULL) ? NULL : GoalEdge->PreviousPathEdge;

	while(CurrentEdge != NULL)
	{
#if !PS3 && !FINAL_RELEASE
		checkFatalPathFailure(LoopCounter++ <= MAX_LOOP_ITTS,TEXT("Infinite Loop Detected in A*::SaveResultingPath.  Try rebuilding paths"),);
#endif
		Handle->PathCache_InsertEdge( CurrentEdge, 0 );
		checkSlow(CurrentEdge->PreviousPathEdge == NULL || CurrentEdge->PreviousPathEdge != CurrentEdge->PreviousPathEdge->PreviousPathEdge);

		CurrentEdge = CurrentEdge->PreviousPathEdge;
	}

	for(INT PathIdx=0;PathIdx<Handle->PathCache.Num();++PathIdx)
	{
		FNavMeshEdgeBase* CurEdge = Handle->PathCache(PathIdx);
		if( CurEdge->PathConstructedWithThisEdge(Handle,PathIdx) )
		{
			// start over if the pathcache as been modified
			PathIdx=-1;
		}		
	}

	DoPathObjectPathMods(Handle);
}

/** 
 *  Allows any pathobjects in the path to modify the final path after it has been generated
 *  @param Handle - the navigation handle we're pathfinding for
 *  @return - TRUE if a path object modified the path
 */
UBOOL UNavMeshPathGoalEvaluator::DoPathObjectPathMods( UNavigationHandle* Handle )
{
	// For debugging stale path objects
	//FNavMeshWorld::VerifyPathObjects();
	//FNavMeshWorld::VerifyPathObstacles();

	UBOOL bMod = FALSE;
	UBOOL bCompletedLoop = FALSE;
	while(!bCompletedLoop)
	{
		bCompletedLoop=TRUE;
		for(INT Idx=0;Idx<Handle->PathCache.Num();++Idx)
		{
			FNavMeshEdgeBase* Edge = Handle->PathCache(Idx);
			if(Edge->GetEdgeType()== NAVEDGE_PathObject)
			{
				FNavMeshPathObjectEdge* POEdge = static_cast<FNavMeshPathObjectEdge*>(Edge);
				IInterface_NavMeshPathObject* POInt = InterfaceCast<IInterface_NavMeshPathObject>(*POEdge->PathObject);
				if( POInt->ModifyFinalPath(Handle,Idx) )
				{
					bMod = TRUE;
					// if this PO modified the list we need to start over in case the order in the cache changed
					bCompletedLoop=FALSE;
					break;
				}
			}
		}		
	}

	return bMod;
}


/**
 * adds initial nodes to the working set.  For basic searches this is just the start node.
 * @param OpenList		- Pointer to the start of the open list
 * @param AnchorPoly    - the anchor poly (poly the entity that's searching is in)
 * @param PathSessionID - unique ID fo this particular path search (used for cheap clearing of path info)
 * @param PathParams    - the cached pathfinding parameters for this path search
 * @return - whether or not we were successful in seeding the search
 */
UBOOL UNavMeshPathGoalEvaluator::SeedWorkingSet( PathOpenList& OpenList,
												FNavMeshPolyBase* AnchorPoly,
												DWORD PathSessionID,
												UNavigationHandle* Handle,
												const FNavMeshPathParams& PathParams)
{
	if(AnchorPoly == NULL)
	{
		DEBUGPATHLOG(TEXT("AnchorPoly is NULL"))

		Handle->SetPathError(PATHERROR_ANCHORPYLONNOTFOUND);
		return FALSE;
	}

	// default behavior is just add the starting poly to the open list 
	Handle->AddSuccessorEdgesForPoly(AnchorPoly,PathParams,NULL,PathSessionID,OpenList);

	return TRUE;
}

void UNavMeshGoal_At::RecycleNative()
{
	GoalPoly=NULL;
	PartialGoal=NULL;
	bGoalInSamePolyAsAnchor=FALSE;
}

UBOOL UNavMeshGoal_At::InitializeSearch( UNavigationHandle* Handle, const FNavMeshPathParams& PathParams)
{
	//debug
	//DEBUGPATHLOG(FString::Printf(TEXT("UNavMeshGoal_At::InitalAbortCheck... Attempt to find path from %s to %s"),*Start.ToString(),*Goal.ToString()));

	UObject* InterfaceObj = PathParams.Interface->GetUObjectInterfaceInterface_NavigationHandle();
	APylon* GoalPylon = NULL;
#if PERF_DO_PATH_PROFILING_MESH
	DWORD Time=0;
	CLOCK_CYCLES(Time);
#endif
	if( !Handle->GetPylonAndPolyFromPos(Goal,PathParams.MinWalkableZ,GoalPylon,GoalPoly) )
	{
#if PERF_DO_PATH_PROFILING_MESH
		UNCLOCK_CYCLES(Time);
		debugf(TEXT(">> GetPylonAndPolyFromPos(Goall Not On Given Nav Mesh) for %s took %3.3fms"),*GetName(),Time*GSecondsPerCycle*1000.f);
#endif
		DEBUGPATHLOG(FString::Printf(TEXT("%s Attempt to find path from %s to %s failed because goal not on the given nav mesh"),*InterfaceObj->GetName(),*Start.ToString(),*Goal.ToString()));

		Handle->SetPathError(PATHERROR_GOALPOLYNOTFOUND);
		return FALSE;
	}
#if PERF_DO_PATH_PROFILING_MESH
	UNCLOCK_CYCLES(Time);
	FLOAT PylonPolyTime = Time*GSecondsPerCycle*1000.f;
	if(PylonPolyTime > PATH_PROFILING_MESH_THRESHOLD)
	{
		debugf(TEXT(">> GetPylonAndPolyFromPos for %s took %3.3fms"),*GetName(),PylonPolyTime);
	}
#endif

	Handle->SetFinalDestination(Goal);

	UBOOL bSuperRet = Super::InitializeSearch( Handle, PathParams );

	if( bSuperRet && Handle->AnchorPoly == NULL  )
	{
		DEBUGPATHLOG(FString::Printf(TEXT("%s Attempt to find path from %s to %s failed because AnchorPoly is NULL"),*InterfaceObj->GetName(),*Start.ToString(),*Goal.ToString()));

		Handle->SetPathError(PATHERROR_ANCHORPYLONNOTFOUND);
		return FALSE;
	}

	if( Handle->AnchorPoly != NULL && GoalPoly != NULL )
	{
		APylon* StartPylon = Handle->AnchorPoly->NavMesh->GetPylon();
		APylon* GoalPylon = GoalPoly->NavMesh->GetPylon();
		APylon* CurPath = Handle->BuildFromPylonAToPylonB(StartPylon,GoalPylon);
		if( CurPath == NULL  )
		{
			if(Handle->bUltraVerbosePathDebugging)
			{
				debugf(TEXT("VERBOSE PATH MESSAGE: TOP LEVEL EARLY OUT! No high level path to target (tried to path from %s to %s)"), *StartPylon->GetName(), *GoalPylon->GetName());
			}
			DEBUGPATHLOG(FString::Printf(TEXT("%s Attempt to find path from %s to %s failed because of high level early out! (NO PATH)"),*InterfaceObj->GetName(),*Start.ToString(),*Goal.ToString()));


			Handle->SetPathError(PATHERROR_NOPATHFOUND);
			return FALSE;
		}

		// if we found a high level path, mark each pylon as in the path, so it can be used in the heuristic
		const FVector Offset = FVector(0.f,0.f,100.f);
		INT LoopCounter=0;
		while( CurPath != NULL )
		{
			if(++LoopCounter > 500 )
			{
				debugf(TEXT("WARNING! infinite loop in top level path for %s"),*InterfaceObj->GetName());
				return FALSE;
			}
			
			if( (Handle->bUltraVerbosePathDebugging || Handle->bVisualPathDebugging) && CurPath->previousPath != NULL )
			{
				GWorld->GetWorldInfo()->DrawDebugLine(CurPath->Location+Offset,CurPath->previousPath->Location + Offset,255,200,0,TRUE);
				GWorld->GetWorldInfo()->DrawDebugStar(CurPath->Location+Offset,50,255,200,0,TRUE);
			}
			CurPath->bPylonInHighLevelPath=TRUE;
			CurPath = Cast<APylon>(CurPath->previousPath);
		}

	}


	return bSuperRet;
}

/**
 * Called each time a node is popped off the working set to determine
 * whether or not we should finish the search (e.g. did we find the node we're looking for)
 * @param PossibleGoal - the chosen (cheapest) successor from the open list
 * @param PossibleGoalPoly - the poly associated with the possible goal
 * @param PathParams   - the cached pathfinding params for the pathing entity
 * @param out_GenGoal  - the poly we should consider the 'goal'.  (Normally PossibleGOal when this returns true, but doesn't have to be)
 * @return - TRUE indicates we have found the node we're looking for and we should stop the search
 */
UBOOL UNavMeshGoal_At::EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal )
{
	FNavMeshPolyBase* PossibleGoalPoly = PossibleGoal->GetPathDestinationPoly();
	check(PossibleGoalPoly != NULL);
	if( PossibleGoalPoly == GoalPoly)
	{
		out_GenGoal = PossibleGoal;
		return TRUE;
	}

	// If keeping track of partial paths and node is not the start
	if( bKeepPartial )
	{
		if (bWeightPartialByDist)
		{
			FLOAT TestDistSq = (PossibleGoalPoly->GetPolyCenter() - Goal).SizeSquared();
			if (TestDistSq < PartialDistSq)
			{
				PartialDistSq = TestDistSq;
				PartialGoal = PossibleGoal;
			}
		}
		else
		{
			// If we don't have a partial path list already OR
			// The possible nav has more "goodness" than our current parital goal
			if( PartialGoal == NULL || 
				(PossibleGoal->EstimatedOverallPathWeight - PossibleGoal->VisitedPathWeight) < 
				(PartialGoal->EstimatedOverallPathWeight - PartialGoal->VisitedPathWeight) )
			{
				// Keep nav as our partial goal
				PartialGoal = PossibleGoal;
			}
		}
	}

	return FALSE;
}

UBOOL UNavMeshGoal_At::SeedWorkingSet( PathOpenList& OpenList,
							FNavMeshPolyBase* AnchorPoly,
							DWORD PathSessionID,
							UNavigationHandle* Handle,
							const FNavMeshPathParams& PathParams)
{
	// if goal is in anchorpoly, just bail withotu seeding so we don't search a lot needlessly
	if( AnchorPoly == GoalPoly )
	{
		bGoalInSamePolyAsAnchor=TRUE;
		return TRUE;
	}

	return Super::SeedWorkingSet(OpenList,AnchorPoly,PathSessionID,Handle,PathParams);
}

UBOOL UNavMeshGoal_At::DetermineFinalGoal( PathCardinalType& out_GenGoal, class AActor** out_DestActor, INT* out_DestItem )
{
	if(Super::DetermineFinalGoal(out_GenGoal,out_DestActor,out_DestItem))
	{
		return TRUE;
	}

	if( bGoalInSamePolyAsAnchor )
	{
		return TRUE;
	}

	// if we got here means we didn't find a real path.. 
	if(bKeepPartial && PartialGoal != NULL)
	{
		out_GenGoal = PartialGoal;
		return TRUE;
	}

	return FALSE;
}

void UNavMeshGoal_At::NotifyExceededMaxPathVisits( PathCardinalType BestGuess, PathCardinalType& out_GenGoal )
{
	// only save off goal if we want to allow partial paths
#define MAX_PARTIAL_DIST_SQ 512.f*512.0f
	if( bKeepPartial && (BestGuess->GetEdgeCenter()-GoalPoly->GetPolyCenter()).SizeSquared() < MAX_PARTIAL_DIST_SQ )
	{
		out_GenGoal = BestGuess;
	}
}

/**
 * Called each time a node is popped off the working set to determine
 * whether or not we should finish the search (e.g. did we find the node we're looking for)
 * @param PossibleGoal - the chosen (cheapest) successor from the open list
 * @param PossibleGoalPoly - the poly associated with the possible goal we're evaluating
 * @param PathParams   - the cached pathfinding params for the pathing entity
 * @param out_GenGoal  - the poly we should consider the 'goal'.  (Normally PossibleGOal when this returns true, but doesn't have to be)
 * @return - TRUE indicates we have found the node we're looking for and we should stop the search
 */
UBOOL UNavMeshGoal_Null::EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal )
{
	if( PossibleGoal->EstimatedOverallPathWeight > 0 )
	{
		// If we don't have a partial path list already OR
		// The possible nav has more "goodness" than our current parital goal
		if( PartialGoal == NULL || 
			PossibleGoal->EstimatedOverallPathWeight < PartialGoal->EstimatedOverallPathWeight)
		{
			// Keep nav as our partial goal
			PartialGoal = PossibleGoal;
		}		
 	}

	return FALSE;
}

UBOOL UNavMeshGoal_Null::DetermineFinalGoal( PathCardinalType& out_GenGoal, class AActor** out_DestActor, INT* out_DestItem )
{
	if(Super::DetermineFinalGoal(out_GenGoal,out_DestActor,out_DestItem))
	{
		return TRUE;
	}

	out_GenGoal = PartialGoal;
	return PartialGoal != NULL;
}

void UNavMeshGoal_Null::RecycleNative()
{
	PartialGoal = NULL;
}

UBOOL UNavMeshGoal_Random::EvaluateGoal(PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal)
{
	if (PossibleGoal->VisitedPathWeight > MinDist)
	{
		FLOAT NewRating = appFrand();
		if (NewRating > BestRating)
		{
			PartialGoal = PossibleGoal;
			BestRating = NewRating;
		}
	}

	return FALSE;
}

UBOOL UNavMeshGoal_Random::DetermineFinalGoal(PathCardinalType& out_GenGoal, class AActor** out_DestActor, INT* out_DestItem)
{
	if (Super::DetermineFinalGoal(out_GenGoal, out_DestActor, out_DestItem))
	{
		return TRUE;
	}
	else
	{
		out_GenGoal = PartialGoal;
		return PartialGoal != NULL;
	}
}

void UNavMeshGoal_Random::RecycleNative()
{
	PartialGoal = NULL;
}

UBOOL UNavMeshGoal_WithinDistanceEnvelope::EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal )
{
	if( PossibleGoal->VisitedPathWeight == 0 )
	{ 
		return FALSE;
	}

	FNavMeshPolyBase* PossibleGoalPoly = PossibleGoal->GetPathDestinationPoly();
	check( PossibleGoalPoly != NULL );

	FLOAT DistToEnvTestPt = (EnvelopeTestPoint - PossibleGoalPoly->GetPolyCenter()).Size();

	// figure out distance from envelope threshold
	FLOAT HalfEnv = ((MaxDistance - MinDistance) * 0.5f);
	FLOAT EnvelopeCenter = MinDistance + HalfEnv;
	FLOAT DistOutsideEnvelope = Max<FLOAT>(0.f, Abs<FLOAT>(DistToEnvTestPt-EnvelopeCenter) - HalfEnv);

	const FLOAT DistToSearchStart = (PathParams.SearchStart - PossibleGoalPoly->GetPolyCenter()).Size();
	if( DistOutsideEnvelope < KINDA_SMALL_NUMBER && (MinTraversalDist <= KINDA_SMALL_NUMBER || DistToSearchStart > MinTraversalDist) )
	{
// 		GWorld->GetWorldInfo()->DrawDebugLine( PossibleGoal->GetPolyCenter()+FVector(0,0,1024), PossibleGoal->GetPolyCenter(), 255, 255, 0, TRUE );
// 		GWorld->GetWorldInfo()->DrawDebugCylinder( EnvelopeTestPoint + FVector(0,0,25), EnvelopeTestPoint + FVector(0,0,25), MinDistance, 16, 255, 0, 0, TRUE );
// 		GWorld->GetWorldInfo()->DrawDebugCylinder( EnvelopeTestPoint + FVector(0,0,25), EnvelopeTestPoint + FVector(0,0,25), MaxDistance, 16, 0, 255, 0, TRUE);

		out_GenGoal = PossibleGoal;
		return TRUE;
	}
	return FALSE;
}


UBOOL UNavMeshGoal_ClosestActorInList::SeedWorkingSet( PathOpenList& OpenList,
													  FNavMeshPolyBase* AnchorPoly,
													  DWORD PathSessionID,
													  UNavigationHandle* Handle,
													  const FNavMeshPathParams& PathParams )
{
	if(AnchorPoly == NULL)
	{
		DEBUGPATHLOG(TEXT("AnchorPoly is NULL"))

		Handle->SetPathError(PATHERROR_ANCHORPYLONNOTFOUND);
		return FALSE;
	}

	FVector Start = PathParams.SearchStart;
	// loop through all of our goal actors and add them to the working set after scoring them
	for(INT GoalActorIdx=0;GoalActorIdx<GoalList.Num();++GoalActorIdx)
	{
		AActor* GoalActor = GoalList(GoalActorIdx).Goal;
		if(GoalActor!=NULL)
		{
			APylon* Py=NULL;
			FNavMeshPolyBase* Poly=NULL;
			if(Handle->GetPylonAndPolyFromActorPos(GoalActor,Py,Poly))
			{
				// if they're not connected at all, don't bother
				if( !Handle->DoesPylonAHaveAPathToPylonB(AnchorPoly->NavMesh->GetPylon(),Py) )
				{
					continue;
				}

				UBOOL bPolyAlreadyInOpenList = PolyToGoalActorMap.HasKey(Poly);

				PolyToGoalActorMap.Add(Poly,GoalActor);

				if(!bPolyAlreadyInOpenList)
				{
					
					INT HeuristicCost = appTrunc((Start - Poly->GetPolyCenter(WORLD_SPACE)).Size());
					Handle->AddSuccessorEdgesForPoly(Poly,PathParams,NULL,PathSessionID,OpenList,GoalList(GoalActorIdx).ExtraCost,HeuristicCost);
				}
			}
		}
	}

	return TRUE;
}

UBOOL UNavMeshGoal_ClosestActorInList::InitializeSearch( UNavigationHandle* Handle, const FNavMeshPathParams& PathParams )
{
	if( !Super::InitializeSearch( Handle, PathParams ) )
	{
		return FALSE;
	}

	CachedAnchorPoly = Handle->AnchorPoly;
	if( CachedAnchorPoly == NULL )
	{
		Handle->SetPathError(PATHERROR_ANCHORPYLONNOTFOUND);
		return FALSE;
	}

	return TRUE;
}

/**
 * Called each time a node is popped off the working set to determine
 * whether or not we should finish the search (e.g. did we find the node we're looking for)
 * @param PossibleGoal - the chosen (cheapest) successor from the open list
 * @param PossibleGoalPoly - the poly associated with the possible goal
 * @param PathParams   - the cached pathfinding params for the pathing entity
 * @param out_GenGoal  - the poly we should consider the 'goal'.  (Normally PossibleGOal when this returns true, but doesn't have to be)
 * @return - TRUE indicates we have found the node we're looking for and we should stop the search
 */
UBOOL UNavMeshGoal_ClosestActorInList::EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal )
{
	FNavMeshPolyBase* PossibleGoalPoly = PossibleGoal->GetPathDestinationPoly();
	check(PossibleGoalPoly!=NULL);
	if(PossibleGoalPoly == CachedAnchorPoly)	
	{
		out_GenGoal=PossibleGoal;
		return TRUE;
	}

	return FALSE;	
}

UBOOL UNavMeshGoal_ClosestActorInList::DetermineFinalGoal( PathCardinalType& out_GenGoal, class AActor** out_DestActor, INT* out_DestItem )
{
	
	// if we didn't end up with the anchor poly then something went wrong, indicate we failed
	
	if(out_GenGoal == NULL || out_GenGoal->GetPathDestinationPoly() != CachedAnchorPoly)
	{
		return FALSE;
	}

#if !PS3 && !FINAL_RELEASE
	INT LoopCounter=0;
#endif
	// Success! loop back through the predecessor lists and figure out which goal actor we just found a path to
	FNavMeshEdgeBase* CurEdge = out_GenGoal;
	FNavMeshEdgeBase* OldCurEdge = NULL;
	while(CurEdge->PreviousPathEdge!=NULL)
	{
#if !PS3 && !FINAL_RELEASE
		checkFatalPathFailure(LoopCounter++ <= MAX_LOOP_ITTS,TEXT("Infinite Loop Detected in UNavMeshGoal_ClosestActorInList::DetermineFinalGoal.  Contact your friendly neighborhood programmer"),FALSE);
#endif
		OldCurEdge = CurEdge;
		CurEdge = CurEdge->PreviousPathEdge;
	}
	
	// need source poly of edge
	FNavMeshPolyBase* CurPoly = CurEdge->GetOtherPoly(CurEdge->GetPathDestinationPoly());
	// out of all the actors in this poly, find the one closest to the previous path position 
	TArray<AActor*> Actors;
	PolyToGoalActorMap.MultiFind(CurPoly,Actors);
	
	// if there are no actors associated with the goal we stopped on, we were unable to find a path to an actor
	// (Possibly due to some other goal evaluators in the list CBing the whole deal)
	if(Actors.Num() > 0)
	{
		// if we don't have a previous poly in the path, just use the anchor poly
		FVector ComparePos = (OldCurEdge != NULL) ? OldCurEdge->PreviousPosition : CachedAnchorPoly->GetPolyCenter(WORLD_SPACE);
		AActor* ClosestActor = Actors(0);

		// find best candidate to compare position
		FLOAT ClosestDistSq = BIG_NUMBER;
		for( INT Idx=0;Idx<Actors.Num();++Idx)
		{
			FLOAT ThisDistSq = (Actors(Idx)->Location - ComparePos).SizeSquared();
			if(ThisDistSq < ClosestDistSq)
			{
				ClosestActor = Actors(Idx);
				ClosestDistSq=ThisDistSq;
			}
		}

		if(ClosestActor != NULL)
		{
			out_GenGoal=CurEdge;
			if(out_DestActor != NULL)
			{
				*out_DestActor=ClosestActor;
			}
			return TRUE;
		}
	}

	return FALSE;
}

// just like super, except we add to the end of the list instead of inserting since we already have a forward facing path
void UNavMeshGoal_ClosestActorInList::SaveResultingPath( FNavMeshPolyBase* StartingPoly,
														FNavMeshPolyBase* GoalPoly,
														UNavigationHandle* Handle,
														FNavMeshEdgeBase* GoalEdge)
{
	FNavMeshEdgeBase* CurrentEdge = GoalEdge;

#if !PS3 && !FINAL_RELEASE
	INT LoopCounter = 0;
#endif


	while( CurrentEdge != NULL) 
	{
#if !PS3 && !FINAL_RELEASE
		checkFatalPathFailure(LoopCounter++ <= MAX_LOOP_ITTS,TEXT("Infinite Loop Detected in UNavMeshGoal_ClosestActorInList::::SaveResultingPath.  OH NOES!"),);
#endif
		Handle->PathCache_AddEdge( CurrentEdge );
		checkSlow(CurrentEdge->PreviousPathEdge == NULL || CurrentEdge->PreviousPathEdge != CurrentEdge->PreviousPathEdge->PreviousPathEdge);

		CurrentEdge = CurrentEdge->PreviousPathEdge;
	}
}

void UNavMeshGoal_ClosestActorInList::RecycleInternal()
{
	PolyToGoalActorMap.Empty();
	CachedAnchorPoly=NULL;
}
// -- end UNavMeshGoal_ClosestActorInList

UBOOL UNavMeshPath_EnforceTwoWayEdges::EvaluatePath( FNavMeshEdgeBase* Edge,
												  FNavMeshEdgeBase* PredecessorEdge, 
												  FNavMeshPolyBase* SrcPoly,
												  FNavMeshPolyBase* DestPoly,
												  const FNavMeshPathParams& PathParams,
												  INT& out_PathCost,
												  INT& out_HeuristicCost,
												  const FVector& EdgePoint)
{
	// if this isn't a one way edge we know we're safe
	if(!Edge->IsOneWayEdge())
	{
		return TRUE;
	}


	// if this is a one way edge, check to see if there is a path backwards
	return (DestPoly->GetEdgeTo(SrcPoly) != NULL);
}

UBOOL UNavMeshPath_WithinDistanceEnvelope::EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint )
{
	if( SrcPoly == NULL || DestPoly == NULL)
	{
		return TRUE;
	}
	FLOAT DistToEnvTestPt = (EnvelopeTestPoint - DestPoly->GetPolyCenter()).Size();


	// figure out distance from envelope threshold
	FLOAT HalfEnv = ((MaxDistance - MinDistance) * 0.5f);
	FLOAT EnvelopeCenter = MinDistance + HalfEnv;
	FLOAT DistOutsideEnvelope = Max<FLOAT>(0.f, Abs<FLOAT>(DistToEnvTestPt-EnvelopeCenter) - HalfEnv);

	if(DistOutsideEnvelope > 0)
	{
		if(!bSoft)
		{
			FLOAT MaxDistSq = MaxDistance*MaxDistance;
			FLOAT MinDistSq = MinDistance*MinDistance;
			FLOAT DistSq = (SrcPoly->GetPolyCenter() - EnvelopeTestPoint).SizeSquared();
			UBOOL bStartInside = (DistSq < MaxDistSq && DistSq > MinDistSq);
			
			UBOOL bEndInside = DistOutsideEnvelope<=KINDA_SMALL_NUMBER;

			// if bOnlyThrowOutNodesThatLeaveEnvelope is off, make sure both end points are outside envelope before throwing the node out
			if(!bOnlyThrowOutNodesThatLeaveEnvelope || (bStartInside && !bEndInside) )
			{
// 				warnf( TEXT( "UNavMeshPath_WithinDistanceEnvelope: Failed!") );
// 				GWorld->GetWorldInfo()->DrawDebugLine( SrcPoly->GetPolyCenter()+FVector(0,0,1024), SrcPoly->GetPolyCenter(), 255, 255, 0, TRUE );
// 				GWorld->GetWorldInfo()->DrawDebugCylinder( EnvelopeTestPoint, EnvelopeTestPoint, MinDistance, 16, 255, 0, 0, TRUE );
// 				GWorld->GetWorldInfo()->DrawDebugCylinder( EnvelopeTestPoint, EnvelopeTestPoint, MaxDistance, 16, 0, 255, 0, TRUE);
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
* traverse back along the previous nodes for this path and add up the distance between edges of the type we care about
*/                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
UBOOL UNavMeshPath_MinDistBetweenSpecsOfType::IsWithinMinDistOfEdgeInPath(FNavMeshEdgeBase* Edge,FNavMeshEdgeBase* PredecessorEdge)
{
	if( PredecessorEdge != NULL )
	{

		FNavMeshEdgeBase* CurrentEdge = PredecessorEdge;

		FVector ClosestOnCurEdge(0.f);
		Edge->PointDistToEdge(PredecessorEdge->PreviousPosition,WORLD_SPACE,&ClosestOnCurEdge);

		INT Distance = appTrunc((ClosestOnCurEdge - CurrentEdge->PreviousPosition).Size());
 	
		while(CurrentEdge->PreviousPathEdge != NULL)
		{
			FNavMeshEdgeBase* PrevEdge = CurrentEdge->PreviousPathEdge;

			checkSlowish(PrevEdge);
			Distance += appTrunc((CurrentEdge->PreviousPosition - PrevEdge->PreviousPosition).Size());

			// if we exceed our mindist stop checking because we don't care any more
			if(Distance > appTrunc(MinDistBetweenEdgeTypes))
			{
				return FALSE;
			}

			// if we just hit another spec of the class we're looking for, then that is the distance we need to check against
			// so return true if we're under the minimum
			if(PrevEdge->GetEdgeType() == EdgeType)
			{
				return (Distance < appTrunc(MinDistBetweenEdgeTypes));
			}

			CurrentEdge = PrevEdge;
		}
	}

	// if we got here it means we didn't ever find a spec of the specified class
	return FALSE;
}


UBOOL UNavMeshPath_MinDistBetweenSpecsOfType::EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge,  FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint )
{
	if( Edge->GetEdgeType() == EdgeType )
	{
		// if it's too close to the init location, or it's too close in its path to another spec of the type we're looking for
		// penalize it heavily (don't block it in case there is no other path)
		if((!InitLocation.IsNearlyZero() && (InitLocation - Edge->GetEdgeCenter()).SizeSquared() < MinDistBetweenEdgeTypes * MinDistBetweenEdgeTypes) ||
			IsWithinMinDistOfEdgeInPath(Edge,PredecessorEdge))
		{
			out_PathCost += Penalty;
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//NavMeshGoal_PolyEncompassesAI
/**
 * Called each time a node is popped off the working set to determine
 * whether or not we should finish the search (e.g. did we find the node we're looking for)
 * @param PossibleGoal - the chosen (cheapest) successor from the open list
 * @param PathParams   - the cached pathfinding params for the pathing entity
 * @param out_GenGoal  - the poly we should consider the 'goal'.  (Normally PossibleGOal when this returns true, but doesn't have to be)
 * @return - TRUE indicates we have found the node we're looking for and we should stop the search
 */
UBOOL UNavMeshGoal_PolyEncompassesAI::EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal )
{

	FVector ExtentToCheck = PathParams.SearchExtent;

	if( OverrideExtentToCheck.Size() > 0.0f )
	{
		ExtentToCheck = OverrideExtentToCheck;
	}


	FCheckResult Hit(1.f);
	if(UNavigationHandle::StaticObstaclePointCheck(Hit,PossibleGoal->GetPathDestinationPoly()->GetPolyCenter(WORLD_SPACE),ExtentToCheck))
	{
		out_GenGoal = PossibleGoal;
		return TRUE;
	}
	return FALSE;
}
//////////////////////////////////////////////////////////////////////////
// >>>> navmesh goal filter classes 
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//UNavMeshGoalFilter_PolyEncompassesAI
UBOOL UNavMeshGoalFilter_PolyEncompassesAI::IsValidFinalGoal( PathCardinalType PossibleGoal,
															 const FNavMeshPathParams& PathParams)
{
	FVector ExtentToCheck = PathParams.SearchExtent;

	if( OverrideExtentToCheck.Size() > 0.0f )
	{
		ExtentToCheck = OverrideExtentToCheck;
	}


	TArray<FNavMeshPolyBase*> Polys;
	UNavigationHandle::GetAllObstaclePolysFromPos(PossibleGoal->GetPathDestinationPoly()->GetPolyCenter(WORLD_SPACE),ExtentToCheck,Polys);
// 	FCheckResult Hit(1.f);
// 	if(UNavigationHandle::StaticObstaclePointCheck(Hit,PossibleGoal->GetPathDestinationPoly()->GetPolyCenter(WORLD_SPACE),ExtentToCheck))
// 	{
// 		return TRUE;
// 	}

	// so if any obstacle polys in the extent box then we can't spawn here
	if( Polys.Num() > 0 )
	{
		return FALSE;
	}
	return TRUE;
}

void UNavMeshGoal_GenericFilterContainer::NotifyExceededMaxPathVisits( PathCardinalType BestGuess, PathCardinalType& out_GenGoal )
{
	// do nothing for the filter case of this
}

//////////////////////////////////////////////////////////////////////////
//UNavMeshGoalFilter_MinPathDistance
IMPLEMENT_CLASS(UNavMeshGoalFilter_MinPathDistance)
UBOOL UNavMeshGoalFilter_MinPathDistance::IsValidFinalGoal( PathCardinalType PossibleGoal,
															 const FNavMeshPathParams& PathParams)
{
#if !FINAL_RELEASE
	if( bShowDebug == TRUE )
	{
		warnf( TEXT("UNavMeshGoalFilter_MinPathDistance:  bestPathWeight: %d  MinDistancePathShouldBe: %d "), PossibleGoal->EstimatedOverallPathWeight, MinDistancePathShouldBe );
	}
#endif // !FINAL_RELEASE


	if( PossibleGoal->EstimatedOverallPathWeight > MinDistancePathShouldBe )
	{
#if !FINAL_RELEASE
		if( bShowDebug == TRUE )
		{
			GWorld->GetWorldInfo()->DrawDebugLine( PossibleGoal->GetPathDestinationPoly()->GetPolyCenter()+FVector(0,0,2048), PossibleGoal->GetPathDestinationPoly()->GetPolyCenter(), 255, 0, 0, TRUE );
		}
#endif // !FINAL_RELEASE
		return TRUE;
	}

	return FALSE;

}

//////////////////////////////////////////////////////////////////////////
// UNavMeshGoalFilter_NotNearOtherAI
IMPLEMENT_CLASS(UNavMeshGoalFilter_NotNearOtherAI)
UBOOL UNavMeshGoalFilter_NotNearOtherAI::IsValidFinalGoal( PathCardinalType PossibleGoal,														   const FNavMeshPathParams& PathParams)
{
	UBOOL Retval = TRUE;

	// Query Octree to find nearby things.
	FMemMark Mark(GMainThreadMemStack);

	const FVector PolyCtr = PossibleGoal->GetPathDestinationPoly()->GetPolyCenter();
	FCheckResult* FirstOverlap = GWorld->Hash->ActorOverlapCheck( GMainThreadMemStack, NULL, PolyCtr, DistanceToCheck );

	for( FCheckResult* Result = FirstOverlap; Result; Result=Result->GetNext() )
	{
		APawn* NP = Cast<APawn>(Result->Actor);
		if( ( NP != NULL ) && ( NP->IsHumanControlled() == FALSE ) )
		{

#if !FINAL_RELEASE
			if( bShowDebug == TRUE )
			{
				warnf( TEXT("Found Another AI HERE!  %s"), *NP->GetName() );
				GWorld->GetWorldInfo()->DrawDebugLine( PolyCtr+FVector(0,0,2048), PolyCtr, 255, 0, 0, TRUE );
			}
#endif // !FINAL_RELEASE

			Retval = FALSE;
			break;
		}
	}

	// clean up the mark
	Mark.Pop();

	return Retval;

}

//////////////////////////////////////////////////////////////////////////
// NavMeshGoalFilter_OutOfViewFrom
IMPLEMENT_CLASS(UNavMeshGoalFilter_OutOfViewFrom)
UBOOL UNavMeshGoalFilter_OutOfViewFrom::IsValidFinalGoal( PathCardinalType PossibleGoal,														   const FNavMeshPathParams& PathParams)
{
	// do line check
	FCheckResult Hit(1.f);

	const FVector PolyCtr = PossibleGoal->GetPathDestinationPoly()->GetPolyCenter();

	// simulate eye sight here
	const FVector StartLocation = OutOfViewLocation + FVector(0,0,100);
	const FVector EndLocation = PolyCtr + FVector(0,0,176);

	// we need a bounding box check here or you get guys appearing around tiny corners
	GWorld->SingleLineCheck( Hit, NULL, EndLocation, StartLocation, TRACE_World|TRACE_StopAtAnyHit );

	if( Hit.Actor )
	{
#if !FINAL_RELEASE
		// green VALID NO SEE
		if( bShowDebug == TRUE )
		{
			warnf( TEXT( "UNavMeshGoal_OutOfViewFrom::EvaluateGoal NO SEE  %s  %s"), *Hit.Actor->GetFullName(), *PolyCtr.ToString() );
			GWorld->GetWorldInfo()->DrawDebugLine( EndLocation, StartLocation, 0, 255, 0, TRUE );
		}
#endif // !FINAL_RELEASE

		return TRUE;
	}
	else
	{
#if !FINAL_RELEASE
		// red INVALID CAN SEE
		if( bShowDebug == TRUE )
		{
			warnf( TEXT( "UNavMeshGoal_OutOfViewFrom::EvaluateGoal CAN SEE  %s"), *PolyCtr.ToString()  );
			GWorld->GetWorldInfo()->DrawDebugLine( EndLocation, StartLocation, 255, 0, 0, TRUE );
		}
#endif // !FINAL_RELEASE
	}

	return FALSE;

}

//////////////////////////////////////////////////////////////////////////
// NavMeshGoalFilter_OutOfViewFrom
IMPLEMENT_CLASS(UNavMeshGoalFilter_OutSideOfDotProductWedge)
UBOOL UNavMeshGoalFilter_OutSideOfDotProductWedge::IsValidFinalGoal( PathCardinalType PossibleGoal,														   const FNavMeshPathParams& PathParams)
{
	const FVector VectToLocation = (PossibleGoal->GetPathDestinationPoly()->GetPolyCenter() - Location).SafeNormal();
	const FVector& LookDirection = Rotation;

	const FLOAT DotProd = VectToLocation | LookDirection;

	const UBOOL bOutsideOfDotProductWedge= DotProd <= Epsilon;

	return bOutsideOfDotProductWedge;

}

//////////////////////////////////////////////////////////////////////////
//NavMeshGoal_GenericFilterContainer
/**
 * Called each time a node is popped off the working set to determine
 * whether or not we should finish the search (e.g. did we find the node we're looking for)
 * @param PossibleGoal - the chosen (cheapest) successor from the open list
 * @param PathParams   - the cached pathfinding params for the pathing entity
 * @param out_GenGoal  - the poly we should consider the 'goal'.  (Normally PossibleGOal when this returns true, but doesn't have to be)
 * @return - TRUE indicates we have found the node we're looking for and we should stop the search
 */
UBOOL UNavMeshGoal_GenericFilterContainer::EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal )
{
	UNavMeshGoal_Filter* Current_Filter = NULL;
	for(INT Idx=0;Idx<GoalFilters.Num();++Idx)
	{
		Current_Filter = GoalFilters(Idx);

#if !FINAL_RELEASE
		if( MyNavigationHandle->bDebugConstraintsAndGoalEvals )
		{
			Current_Filter->NumNodesProcessed++;
		}
#endif //!FINAL_RELEASE

		if( ! Current_Filter->IsValidFinalGoal( PossibleGoal,PathParams ) )
		{
#if !FINAL_RELEASE
			if( MyNavigationHandle->bDebugConstraintsAndGoalEvals )
			{
				Current_Filter->NumNodesThrownOut++;
			}
#endif //!FINAL_RELEASE

			return FALSE;
		}
	}

	// since we passed all of our filters this is a valid out_GenGoal/SuccessfulGoal
	//warnf( TEXT("We found a valid goal! %s"), *PossibleGoal->GetPolyCenter().ToString() );
	//GWorld->GetWorldInfo()->DrawDebugLine( PossibleGoal->GetPolyCenter()+FVector(0,0,2048), PossibleGoal->GetPolyCenter(), 0, 255, 0, TRUE );

	SuccessfulGoal = PossibleGoal;
	out_GenGoal = PossibleGoal;
	return TRUE;
}

/**
 * this will ask each filter in this guy's list if the passed poly is a viable seed to get added at start time
 * @param PossibleSeed - the seed to check viability for
 * @param PathParams - params for entity searching
 */
UBOOL UNavMeshGoal_GenericFilterContainer::IsValidSeed( FNavMeshPolyBase* PossibleSeed, const FNavMeshPathParams& PathParams )
{
	UNavMeshGoal_Filter* Current_Filter = NULL;
	for(INT Idx=0;Idx<GoalFilters.Num();++Idx)
	{
		Current_Filter = GoalFilters(Idx);
		if( ! Current_Filter->IsValidSeed( PossibleSeed,PathParams ) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

/** 
 * sets up internal vars for path searching, and will early out if something fails
 * @param Handle - handle we're initializing for
 * @param PathParams - pathfinding parameter packet
 * @return - whether or not we should early out form this search
 */
UBOOL UNavMeshGoal_GenericFilterContainer::InitializeSearch( UNavigationHandle* Handle, const FNavMeshPathParams& PathParams )
{
	SuccessfulGoal = NULL;
// verify there is only one filtercontainer on the list
#if !FINAL_RELEASE
	UNavMeshPathGoalEvaluator* GoalEval = Handle->PathGoalList;
	while(GoalEval != NULL)
	{
		check(GoalEval == this || !GoalEval->IsA(UNavMeshGoal_GenericFilterContainer::StaticClass()));
		GoalEval = GoalEval->NextEvaluator;
	}
#endif
	
	return Super::InitializeSearch(Handle,PathParams);
}

FVector UNavMeshGoal_GenericFilterContainer::GetGoalPoint()
{
	if( SuccessfulGoal != NULL)
	{
		return SuccessfulGoal->GetPathDestinationPoly()->GetPolyCenter();
	}

	return FVector(0.f);
}



UBOOL UNavMeshGoal_GenericFilterContainer::SeedWorkingSet( PathOpenList& OpenList,
													  FNavMeshPolyBase* AnchorPoly,
													  DWORD PathSessionID,
													  UNavigationHandle* Handle,
													  const FNavMeshPathParams& PathParams )
{
	TLookupMap<FNavMeshPolyBase*> SeedPolys;

	FVector Start = PathParams.SearchStart;
	// loop through all of our goal actors and add them to the working set after scoring them
	for(INT SeedLocIdx=0;SeedLocIdx<SeedLocations.Num();++SeedLocIdx)
	{
		FVector SeedLoc = SeedLocations(SeedLocIdx);
		APylon* Py=NULL;
		FNavMeshPolyBase* Poly=NULL;
		if( Handle->GetPylonAndPolyFromPos(SeedLoc,PathParams.MinWalkableZ,Py,Poly) )
		{
			
			if( !SeedPolys.HasKey(Poly) )
			{
				SeedPolys.AddItem(Poly);
			
				Handle->AddSuccessorEdgesForPoly(Poly,PathParams,NULL,PathSessionID,OpenList,0,0);
			}
		}
	}

	return SeedPolys.Num()>0;
}


