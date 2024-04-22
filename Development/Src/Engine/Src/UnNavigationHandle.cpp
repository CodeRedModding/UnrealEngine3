/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

///////////////////////////////////////////////////////////////////
/////////////////// NAVIGATION HANDLE /////////////////////////////
///////////////////////////////////////////////////////////////////
#include "EnginePrivate.h"
#include "UnPath.h"
#include "EngineAIClasses.h"
#include "GenericOctree.h"


IMPLEMENT_CLASS(UNavigationHandle);
IMPLEMENT_CLASS(UInterface_NavigationHandle)
IMPLEMENT_CLASS(UInterface_NavMeshPathObject)
IMPLEMENT_CLASS(UInterface_NavMeshPathSwitch)
IMPLEMENT_CLASS(UInterface_NavMeshPathObstacle)
IMPLEMENT_CLASS(ANavMeshObstacle)
IMPLEMENT_CLASS(ATestSplittingVolume)
IMPLEMENT_CLASS(AEnvironmentVolume)
IMPLEMENT_CLASS(UAutoNavMeshPathObstacleUnregister)

#define DEBUG_DRAW_NAVMESH_PATH 0
#define PERF_DO_PATH_PROFILING_MESH (0 || LOOKING_FOR_PERF_ISSUES)

// threshold (in ms) above which a warning will be printed
#define PATH_PROFILING_MESH_THRESHOLD 5.0f

/** NavMesh stats */
DECLARE_STATS_GROUP(TEXT("NavMesh"), STATGROUP_NavMesh);

// Nav mesh stat enums. This could be in UnPath.h if we need stats in 
// UnNavigationMesh.cpp, etc (but then more rebuilds needed when changing)
enum ENavMeshStats
{
	STAT_GeneratePathTime = STAT_NavMeshFirstStat,
	STAT_EvaluateGoalTime,
	STAT_ComputeEdgeTime,
	STAT_EdgeIterations,
	STAT_ApplyConstraintsTime,
	STAT_DetermineGoalTime,
	STAT_SavePathTime,
	STAT_ObstaclePointCheckTime,
};
DECLARE_CYCLE_STAT(TEXT("  Save Path"), STAT_SavePathTime, STATGROUP_NavMesh);
DECLARE_CYCLE_STAT(TEXT("  Determine Goal"), STAT_DetermineGoalTime, STATGROUP_NavMesh);
DECLARE_DWORD_COUNTER_STAT(TEXT("    Edge Iterations"), STAT_EdgeIterations, STATGROUP_NavMesh);
// apply constraints can be called from an inner loop, so we only enable it if STATS_SLOW is defined
DECLARE_CYCLE_STAT_SLOW(TEXT("    Apply Constraints"), STAT_ApplyConstraintsTime, STATGROUP_NavMesh);
DECLARE_CYCLE_STAT(TEXT("  Compute Edges"), STAT_ComputeEdgeTime, STATGROUP_NavMesh);
DECLARE_CYCLE_STAT(TEXT("  Evaluate Goal"), STAT_EvaluateGoalTime, STATGROUP_NavMesh);
DECLARE_CYCLE_STAT(TEXT("Generate Path"), STAT_GeneratePathTime, STATGROUP_NavMesh);
DECLARE_CYCLE_STAT(TEXT("Obstacle Point Check"), STAT_ObstaclePointCheckTime, STATGROUP_NavMesh);


#define DEFAULT_MIN_WALKABLE_Z AScout::GetGameSpecificDefaultScoutObject()->WalkableFloorZ

// DEBUG INFO MACROS
#if FINAL_RELEASE || CONSOLE
#define VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(...)
#define VERBOSE_LOG_EDGE_TRAVERSAL_SUCCESS_MSG(...)
#define VERBOSE_LOG_EDGE_TRAVERSAL_SUCCESS(...)
#define VERBOSE_LOG_EDGE_CONSTRAINTCOST(...)
#define VERBOSE_LOG_START_PATH_SEARCH(...)
#define VERBOSE_LOG_PATH_STEP(...)
#define VERBOSE_LOG_PATH_MESSAGE(...)
#define VERBOSE_LOG_PATH_FINISH(...)
#define VERBOSE_LOG_PATH_POLY_GOALEVAL_START(...)
#define VERBOSE_LOG_PATH_POLY_GOALEVAL_STATUS(...)
#define VERBOSE_LOG_PATH_SUPPORTSMOVETOEDGEFAIL(...)
#else 
UINT NavPathStepIndex=0;
UINT DebugStringIndex=0;
UINT PolyIdx=0;
#define VERBOSE_LOG_START_PATH_SEARCH(NAVHANDLE) \
	if (NAVHANDLE->bUltraVerbosePathDebugging)\
	{\
		AWorldInfo* WI = GWorld->GetWorldInfo();\
\
		NavPathStepIndex=0;\
		DebugStringIndex=0;\
		PolyIdx=0;\
		WI->FlushDebugStrings();\
		WI->FlushPersistentDebugLines();\
		WI->DrawDebugCoordinateSystem(NAVHANDLE->CachedPathParams.SearchStart,FRotator::ZeroRotator,25.f,TRUE);\
		UObject* InterfaceObj = NAVHANDLE->CachedPathParams.Interface->GetUObjectInterfaceInterface_NavigationHandle();\
		debugf(TEXT("+++++++++ STARTING PATH SEARCH for handle %s on %s"),*NAVHANDLE->GetName(), *InterfaceObj->GetName());\
		debugf(TEXT("SEARCH PARAMS:"));\
		debugf(TEXT("SearchStart: %s"),*NAVHANDLE->CachedPathParams.SearchStart.ToString());\
		debugf(TEXT("SearchExtent: %s"),*NAVHANDLE->CachedPathParams.SearchExtent.ToString());\
		debugf(TEXT("CanMantle: %u"), NAVHANDLE->CachedPathParams.bCanMantle);\
		debugf(TEXT("MaxDropHeight: %.2f"), NAVHANDLE->CachedPathParams.MaxDropHeight);\
		debugf(TEXT("MinWalkableZ: %.2f"), NAVHANDLE->CachedPathParams.MinWalkableZ);\
		debugf(TEXT("---"));\
		debugf(TEXT("PathGoalEvaluators:"));\
		UNavMeshPathGoalEvaluator* GoalEval = NAVHANDLE->PathGoalList; \
		while(GoalEval != NULL)\
		{\
			debugf(TEXT("%s"),*GoalEval->GetName());\
			GoalEval = GoalEval->NextEvaluator;\
		}\
		debugf(TEXT("---"));\
		debugf(TEXT("PathConstraints:"));\
		UNavMeshPathConstraint* Constraint = NAVHANDLE->PathConstraintList;\
		while(Constraint != NULL)\
		{\
			debugf(TEXT("%s"),*Constraint->GetName());\
			Constraint = Constraint->NextConstraint;\
		}\
		debugf(TEXT("---"));\
	\
	}\
	else if (NAVHANDLE->bVisualPathDebugging)\
	{\
		AWorldInfo* WI = GWorld->GetWorldInfo();\
		WI->FlushDebugStrings();\
		WI->FlushPersistentDebugLines();\
		WI->DrawDebugCoordinateSystem(NAVHANDLE->CachedPathParams.SearchStart,FRotator::ZeroRotator,25.f,TRUE);\
	}

#define VERBOSE_LOG_PATH_STEP(NAVHANDLE,OPENLIST)\
	if (NAVHANDLE->bUltraVerbosePathDebugging)\
	{\
		NavPathStepIndex++;\
		PathCardinalType CurrentNode = OPENLIST;\
		INT Count=0;\
		for(;CurrentNode != NULL&&Count<10000;CurrentNode = CurrentNode->NextOpenOrdered){Count++;}\
		debugf(TEXT("+++Finished path step %u!, Openlist now has %i nodes in it."),NavPathStepIndex,Count);\
	}

#define VERBOSE_LOG_PATH_FINISH(NAVHANDLE, OPENLIST, REASON)\
	if (NAVHANDLE->bUltraVerbosePathDebugging)\
	{\
		PathCardinalType CurrentNode = OPENLIST;\
		INT Count=0;\
		for(;CurrentNode != NULL&&Count<10000;CurrentNode = CurrentNode->NextOpenOrdered){Count++;}\
		debugf(TEXT("+++++++++ STOPPING PATH SEARCH -- Nodes on openlist: %i Reason: %s"),Count,REASON);\
		debugf(TEXT("---- PathCache ----"));\
		for(INT Idx=0;Idx<NAVHANDLE->PathCache.Num();++Idx)\
		{\
			debugf(TEXT("[%i] - %s"),Idx,*NAVHANDLE->PathCache(Idx)->GetDebugText());\
		}\
		debugf(TEXT("---- END PathCache ----"));\
		GWorld->GetWorldInfo()->eventDebugFreezeGame();\
	}

#define VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(EDGE, MESSAGE, NAVHANDLE) \
	if(NAVHANDLE->bUltraVerbosePathDebugging)\
	{\
		if(EDGE==NULL)\
		{\
			debugf(TEXT("PATH_DEBUG_MESSAGE[-]: %s Edge: %s"),MESSAGE,TEXT("NULL"));\
		}\
		else\
		{\
		\
			AWorldInfo* WI = GWorld->GetWorldInfo();\
			FVector Start = (EDGE->GetPoly0())? EDGE->GetPoly0()->GetPolyCenter()+VRand() : EDGE->GetEdgeCenter();\
			FVector End = (EDGE->GetPoly1())? EDGE->GetPoly1()->GetPolyCenter()+VRand() : EDGE->GetEdgeCenter();\
			/*WI->DrawDebugLine( Start, End, 255,0,0,TRUE );*/\
			/*//WI->DrawDebugLine(EDGE->GetVertLocation(0),EDGE->GetVertLocation(1),100,0,0,TRUE);*/\
			WI->DrawDebugString((Start+End)*0.5f + VRand() + FVector(0.f,0.f,EDGE->EffectiveEdgeLength/3.f) ,FString::Printf(TEXT("C:%u"),++DebugStringIndex),NULL,FColor(255,0,0,255));\
			debugf(TEXT("PATH_DEBUG_MESSAGE[C:%u]: %s Edge: %s"),DebugStringIndex,MESSAGE,*EDGE->GetDebugText());\
		}\
	}

#define VERBOSE_LOG_EDGE_TRAVERSAL_SUCCESS(EDGE, MESSAGE, NAVHANDLE)\
	if(NAVHANDLE->bUltraVerbosePathDebugging || NAVHANDLE->bVisualPathDebugging)\
	{\
		AWorldInfo* WI = GWorld->GetWorldInfo();\
		FVector Start = EDGE->GetPoly0()->GetPolyCenter()+VRand();\
		FVector End = EDGE->GetPoly1()->GetPolyCenter()+VRand();\
		WI->DrawDebugLine(EDGE->GetVertLocation(0),EDGE->GetVertLocation(1),(15*NavPathStepIndex)%100,100,0,TRUE);\
		WI->DrawDebugLine( Start, End, (15*NavPathStepIndex)%255,255,0,TRUE );\
		WI->DrawDebugString( (Start+End) * 0.5f + VRand(), MESSAGE, NULL, FColor(0,255,0,255) );\
	}

#define VERBOSE_LOG_EDGE_CONSTRAINTCOST(EDGE, NAVHANDLE, MESSAGE) \
	if( NAVHANDLE->bUltraVerbosePathDebugging ) \
	{\
		debugf(TEXT("=== %s "), MESSAGE );\
	}

#define VERBOSE_LOG_PATH_POLY_GOALEVAL_START(NAVHANDLE, EDGE)\
	if(NAVHANDLE->bUltraVerbosePathDebugging || NAVHANDLE->bVisualPathDebugging)\
	{\
		AWorldInfo* WI = GWorld->GetWorldInfo();\
		WI->DrawDebugString(EDGE->GetPathDestinationPoly()->GetPolyCenter() + VRand(),FString::Printf(TEXT("P:%u"),++PolyIdx),NULL,FColor(200,200,200,255));\
		WI->DrawDebugLine(EDGE->PreviousPosition,EDGE->GetPathDestinationPoly()->GetPolyCenter(),200,200,200,TRUE);\
	}

#define VERBOSE_LOG_PATH_POLY_GOALEVAL_STATUS(NAVHANDLE, POLY, GOALEVAL, INTEXT) \
	if(NAVHANDLE->bUltraVerbosePathDebugging)\
	{\
		debugf(TEXT("Poly (P:%u) (polyctr:%s) was just given status [%s] by %s"),PolyIdx,*POLY->GetPolyCenter().ToString(),INTEXT,*GOALEVAL->GetName());\
	}

#define VERBOSE_LOG_PATH_MESSAGE(NAVHANDLE, MESSAGE)\
	if(NAVHANDLE->bUltraVerbosePathDebugging)\
	{\
		debugf(TEXT("VERBOSE PATH MESSAGE: %s"),MESSAGE);\
	}

#define VERBOSE_LOG_PATH_SUPPORTSMOVETOEDGEFAIL(NAVHANDLEINTERFACE, NEXTEDGE, PREVEDGE, NEXTEDGEPOS) \
	AController* C = Cast<AController>(NAVHANDLEINTERFACE->GetUObjectInterfaceInterface_NavigationHandle());\
	UNavigationHandle* Handle = (C) ? C->NavigationHandle : NULL;\
	if(Handle != NULL && (Handle->bUltraVerbosePathDebugging || Handle->bVisualPathDebugging))\
	{\
		debugf(TEXT("SUPPORTED MOVE TO EDGE FAIL[C:%u]: NextEdge:%s PrevEdge:%s"),DebugStringIndex+1,*NEXTEDGE->GetDebugText(), *PREVEDGE->GetDebugText());\
		AWorldInfo* WI = GWorld->GetWorldInfo();\
		WI->DrawDebugLine(PREVEDGE->PreviousPosition,NEXTEDGEPOS,255,128,0,TRUE);\
		WI->DrawDebugLine(NEXTEDGEPOS+FVector(0.f,0.f,20.f),NEXTEDGEPOS,255,128,0,TRUE);\
	}



#endif

#if DO_PATHCONSTRAINT_PROFILING 

TArray<FConstraintProfileDatum> HandleConstraintProfileData;
INT	  HandleCallCount =0;
FLOAT HandleCallMax = -1.f;
FLOAT HandleCallAvg=-1.f;
FLOAT HandleCallTotal=0.f;
FConstraintProfileDatum* HandleProfileDatum = NULL;
#endif


/** The octree semantics for the pylon octree. */
struct FPylonOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	static FBoxCenterAndExtent GetBoundingBox(APylon* Pylon)
	{
		return FBoxCenterAndExtent(Pylon->GetBounds(WORLD_SPACE));
	}

	static void SetElementId(APylon* Pylon, FOctreeElementId Id)
	{
		Pylon->OctreeId = Id;
	}
};

///////////////////////////////////////////////////////////////////////
// FNavMeshWorld Implementation

FNavMeshWorld* FNavMeshWorld::GetNavMeshWorld()
{
	if(GWorld == NULL)
	{
		return NULL;
	}

	if(GWorld->NavMeshWorld == NULL)
	{
		GWorld->NavMeshWorld = new FNavMeshWorld();
	}

	return GWorld->NavMeshWorld;
}

void FNavMeshWorld::DestroyNavMeshWorld()
{
	if(GWorld && GWorld->NavMeshWorld != NULL)
	{
		if(GWorld->NavMeshWorld->PylonOctree != NULL)
		{
			delete GWorld->NavMeshWorld->PylonOctree;
			GWorld->NavMeshWorld->PylonOctree=NULL;
		}

		delete GWorld->NavMeshWorld;
		GWorld->NavMeshWorld=NULL;
	}
}

FPylonOctreeType* FNavMeshWorld::GetPylonOctree(UBOOL bDontCreateNew)
{
	FNavMeshWorld* World = GetNavMeshWorld();

	if(World != NULL)
	{
		if(World->PylonOctree == NULL && !bDontCreateNew)
		{	
			World->PylonOctree = new FPylonOctreeType(FVector(0,0,0), HALF_WORLD_MAX);
		}
		return World->PylonOctree;
	}

	return NULL;
}

void FNavMeshWorld::DrawPylonOctreeBounds( const FPylonOctreeType* inOctree )
{
	for(FPylonOctreeType::TConstIterator<> OctreeIt(*(inOctree)); OctreeIt.HasPendingNodes(); OctreeIt.Advance())
	{
		const FPylonOctreeType::FNode& Node = OctreeIt.GetCurrentNode();

		for( FPylonOctreeType::ElementConstIt NodePrimitiveIt(Node.GetElementIt()); NodePrimitiveIt; ++NodePrimitiveIt )
		{
			APylon* Py = ((APylon*)(*NodePrimitiveIt));

			FBox PyBounds = Py->GetBounds(TRUE);
			Py->DrawDebugBox( PyBounds.GetCenter(), PyBounds.GetExtent(), 0, 255, 0, TRUE );
		}
	}
}

void FNavMeshWorld::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel)
{
	FNavMeshWorld* World = GetNavMeshWorld();

	if(World != NULL)
	{
		for(ObstacleToPolyMapType::TIterator It(World->ActiveObstacles);It;++It)
		{		
			FPolyReference& PolyRef = It.Value();
			if( (bIsRemovingLevel && PolyRef.OwningPylon.Actor != NULL) || 
				(!bIsRemovingLevel && PolyRef.OwningPylon.Actor == NULL))
			{
				ActorRefs.AddItem(&PolyRef.OwningPylon);
			}
		}
	}
}

void FNavMeshWorld::ClearRefsToLevel(ULevel* Level)
{
	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();

	if(World != NULL)
	{
		UNavigationHandle* CurHandle = NULL;
		for(INT HandleIdx=0;HandleIdx<World->ActiveHandles.Num();++HandleIdx)
		{		
			CurHandle = World->ActiveHandles(HandleIdx);
			if(CurHandle != NULL)
			{
				CurHandle->ClearCrossLevelRefs(Level);
			}
		}
	}	
}

/**
 * clears all references from handles to meshes (e.g. during seamless travel)
 */
void FNavMeshWorld::ClearAllNavMeshRefs()
{
	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();

	if(World != NULL)
	{
		UNavigationHandle* CurHandle = NULL;
		for(INT HandleIdx=0;HandleIdx<World->ActiveHandles.Num();++HandleIdx)
		{		
			CurHandle = World->ActiveHandles(HandleIdx);
			if(CurHandle != NULL)
			{
				CurHandle->ClearAllMeshRefs();
			}
		}
	}
}

/**
 * called when a pylon is loaded into the level
 * will run around to adjacent meshes and add dynamic edges to submeshes that we otherwise would not link to because
 * they were built while this pylon was unloaded and therefore couldn't link to us at that time
 * @param PylonToFixup - the pylon we're fixing up dynamic linkage for
 */
void CreateEdgesToAdjacentPylonSubmeshes(APylon* PylonToFixup)
{
	if( PylonToFixup == NULL || PylonToFixup->NavMeshPtr == NULL )
	{
		return;
	}
	// loop through all cross pylon edges and check for submeshes on the other side not accompanied by submeshes on our side
	FNavMeshCrossPylonEdge* CPEdge = NULL;
	TLookupMap<FNavMeshPolyBase*> PolysToFixup;
	for(INT CPIdx=0;CPIdx<PylonToFixup->NavMeshPtr->CrossPylonEdges.Num();++CPIdx)
	{
		CPEdge = PylonToFixup->NavMeshPtr->CrossPylonEdges(CPIdx);
		
		if( CPEdge->IsValid(TRUE) )
		{
			FNavMeshPolyBase* Poly0 = CPEdge->GetPoly0();
			FNavMeshPolyBase* Poly1 = CPEdge->GetPoly1();

			if( Poly0 != NULL && Poly1 != NULL )
			{
				UBOOL bPoly0HasSubMesh = ( Poly0->NumObstaclesAffectingThisPoly > 0 );
				UBOOL bPoly1HasSubMesh = ( Poly1->NumObstaclesAffectingThisPoly > 0 );

				// if the destination poly has a submesh, but the source does not.. need to add links to it
				// (it is assumed that if source poly has a submesh proper linkages were made by submesh code in this pylon )
				if( bPoly1HasSubMesh && !bPoly0HasSubMesh )
				{
					PolysToFixup.AddItem(Poly0);				
				}
				else if ( bPoly0HasSubMesh && !bPoly1HasSubMesh)
				{
					PolysToFixup.AddItem(Poly1);				
				}
			}
		}
	}


	FNavMeshPolyBase* Poly=NULL;
	for(INT Idx=0;Idx<PolysToFixup.Num();++Idx)
	{
		Poly = PolysToFixup(Idx);
		Poly->NavMesh->BuildSubMeshEdgesForJustClearedTLPoly(Poly->Item);
	}
}

/**
 * called after a level is loaded.  Gives pylons a chance to do work
 * once cross level refs have been fixed up
 * @param LeveJustFixedUp - the level that just got cross level refs fixed up
 */
void FNavMeshWorld::PostCrossLevelRefsFixup(ULevel* LevelJustFixedUp)
{
	// find all pylons which have cross level refs, and let them do things 
	for (INT Idx = 0; Idx < LevelJustFixedUp->CrossLevelActors.Num(); Idx++)
	{
		APylon* Pylon = Cast<APylon>(LevelJustFixedUp->CrossLevelActors(Idx));
		if( Pylon != NULL )
		{
			CreateEdgesToAdjacentPylonSubmeshes(Pylon);
		}
	}
}

void FNavMeshWorld::RegisterActiveHandle(UNavigationHandle* NewActiveHandle)
{
	FNavMeshWorld* World = GetNavMeshWorld();

	if(World != NULL)
	{
		World->ActiveHandles.AddItem(NewActiveHandle);
	}
}

void FNavMeshWorld::UnregisterActiveHandle(UNavigationHandle* HandleToRemove)
{
	FNavMeshWorld* World = GetNavMeshWorld();

	if(World != NULL)
	{
		World->ActiveHandles.RemoveItem(HandleToRemove);
	}
}

/** 
 * if we have no edge deletion holds at the moment, trigger the edge to be deleted.. otherwise
 * queue it for deletion
 */
void FNavMeshWorld::DestroyEdge(FNavMeshEdgeBase* Edge, UBOOL bJustNotify)
{
	Edge->bPendingDelete = TRUE;

	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();
	if( World!=NULL )
	{
		World->EdgesPendingDeletion.Set(Edge,bJustNotify);

		if(World->EdgeDeletionHoldStackDepth < 1)
		{
			FlushEdgeDeletionQueue();
		}
	}
}

/**
 * puts a hold on deletions and deletion notifications, they will instead be queued and flushed once the hold is removed
 */
void FNavMeshWorld::HoldEdgeDeletes()
{
	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();
	if(World!=NULL)
	{
		World->EdgeDeletionHoldStackDepth++;
	}
}

/**
 * removes a hold from edge deletes, if depth reaches 0 will then flush the queue
 */
void FNavMeshWorld::RemoveEdgeDeleteHold()
{
	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();
	if(World!=NULL)
	{
		if(World->EdgeDeletionHoldStackDepth>0)
		{
			World->EdgeDeletionHoldStackDepth--;
		}

		if(World->EdgeDeletionHoldStackDepth==0)
		{
			FlushEdgeDeletionQueue();
		}
	}
}

/**
 * will run through the queue of edges pending delete and notify handles they're being deleted, and then
 * perform the deletion
 */
void FNavMeshWorld::FlushEdgeDeletionQueue()
{
	FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();

	if(World != NULL && World->EdgesPendingDeletion.Num() > 0)
	{
		// build a map of edges to handles that ref them
		TMap< FNavMeshEdgeBase*, TArray< UNavigationHandle* > > EdgeToHandleMap;
		UNavigationHandle* CurrentHandle = NULL;
		FNavMeshEdgeBase* CurrentEdge = NULL;
		
		// FOR EACH active handle
		for(INT HandleIdx=0;HandleIdx<World->ActiveHandles.Num();++HandleIdx)
		{		
			CurrentHandle = World->ActiveHandles(HandleIdx);

			// FOR EACH edge in this handle
			for(INT HandleEdgeIdx=0;HandleEdgeIdx < CurrentHandle->PathCache.Num(); ++HandleEdgeIdx)
			{
				// save mapping from edge to handle referencing it
				CurrentEdge = CurrentHandle->PathCache(HandleEdgeIdx);
				TArray<UNavigationHandle*>* HandlesForEdge = EdgeToHandleMap.Find(CurrentEdge);
				
				// edge not in the map yet, add a new entry
				if(HandlesForEdge == NULL)
				{
					TArray<UNavigationHandle*> TmpHandlArray;
					TmpHandlArray.AddItem(CurrentHandle);
					EdgeToHandleMap.Set(CurrentEdge,TmpHandlArray);
				}
				else
				{
					HandlesForEdge->AddItem(CurrentHandle);
				}

			}

			// AND for CurrentEdge
			if( CurrentHandle->CurrentEdge != NULL )
			{
				// save mapping from edge to handle referencing it
				CurrentEdge = CurrentHandle->CurrentEdge;
				TArray<UNavigationHandle*>* HandlesForEdge = EdgeToHandleMap.Find(CurrentEdge);

				// edge not in the map yet, add a new entry
				if(HandlesForEdge == NULL)
				{
					TArray<UNavigationHandle*> TmpHandlArray;
					TmpHandlArray.AddItem(CurrentHandle);
					EdgeToHandleMap.Set(CurrentEdge,TmpHandlArray);
				}
				else
				{
					HandlesForEdge->AddItem(CurrentHandle);
				}
			}


		}


		// build local lists, and flush the queue so if we get into this function again somehow from PostEdgeCleanup() it doesn't
		// stomp the list while we're iterating over it
		TArray<FNavMeshEdgeBase*> Edges;
		World->EdgesPendingDeletion.GenerateKeyArray(Edges);
		TArray<UBOOL> DoDeleteArray;
		World->EdgesPendingDeletion.GenerateValueArray(DoDeleteArray);
		World->EdgesPendingDeletion.Empty(10);


		for(INT EdgeDeleteIdx=0;EdgeDeleteIdx<Edges.Num();++EdgeDeleteIdx)
		{
			CurrentEdge = Edges(EdgeDeleteIdx);

			TArray<UNavigationHandle*>* HandlesForEdge = EdgeToHandleMap.Find(CurrentEdge);
			if(HandlesForEdge != NULL)
			{
				// for each handle reffing this edge, let it know it's about to be deleted
				for(INT DeletingEdgeIdx=0;DeletingEdgeIdx<HandlesForEdge->Num();++DeletingEdgeIdx)
				{
					CurrentHandle = (*HandlesForEdge)(DeletingEdgeIdx);
					CurrentHandle->PostEdgeCleanup(CurrentEdge);
				}
			}

			// if this edge was marked for full delete and not just notify-delete, do so!
			UBOOL bJustNotifyAndDontDelete = DoDeleteArray(EdgeDeleteIdx);;
			if(!bJustNotifyAndDontDelete)
			{
				delete CurrentEdge;
			}
		}		
	}
}

/**
 * verifies the passed edge is valid and not deleted 
 * @param CurEdge - the edge to verify
 */
void VerifyEdge(FNavMeshEdgeBase* CurEdge)
{
	FNavMeshPathObjectEdge* POEdge = NULL;
	if(CurEdge != NULL && CurEdge->GetEdgeType()==NAVEDGE_PathObject)
	{
		POEdge = static_cast<FNavMeshPathObjectEdge*>(CurEdge);
		// call some functions on the POEdge to verify stuff 
		AActor* ActorRef = *POEdge->PathObject;
		if(ActorRef!=NULL)
		{
			IInterface_NavMeshPathObject* POInt = InterfaceCast<IInterface_NavMeshPathObject>(ActorRef);
			check(POInt!=NULL&&"Path object actor doesn't implement the PO Interface?!");
			if(POInt != NULL)
			{
				if(!POInt->Verify())
				{
					debugf(TEXT("Edge failed verification! %p"));
					debugf(TEXT("Edge's PO owner:(%p) %s"),ActorRef,*ActorRef->GetName());
					check(FALSE && "Edge failed verification");
				}
			}
		}
	}

}

/**
 * loops through all edges in the passed mesh and calls VerifyEdge on it
 *@param Mesh - the mesh to verify
 */
void VerifyEdgesInMesh(UNavigationMeshBase* Mesh)
{
	FNavMeshEdgeBase* CurEdge = NULL;
	for(INT Idx=0;Idx<Mesh->GetNumEdges();++Idx)
	{
		CurEdge = Mesh->GetEdgeAtIdx(Idx);
		VerifyEdge(CurEdge);
	}

	// dynamic edges
	for(DynamicEdgeList::TIterator Itt(Mesh->DynamicEdges);Itt;++Itt)
	{
		CurEdge = Itt.Value();
		VerifyEdge(CurEdge);
	}


	for(INT PolyIdx=0;PolyIdx<Mesh->Polys.Num();++PolyIdx)
	{
		FNavMeshPolyBase& CurPoly = Mesh->Polys(PolyIdx);

		UNavigationMeshBase* SubMesh = CurPoly.GetSubMesh();
		if( SubMesh != NULL )
		{
			VerifyEdgesInMesh(SubMesh);
		}
	}
}

/**
 * prints info about all path object edges
 *@param Mesh - the mesh to print path object edges from
 */
void PrintPOEdgesForMesh(UNavigationMeshBase* Mesh)
{
	FNavMeshEdgeBase* CurEdge = NULL;
	for(INT Idx=0;Idx<Mesh->GetNumEdges();++Idx)
	{
		CurEdge = Mesh->GetEdgeAtIdx(Idx);
		if( CurEdge->EdgeType == NAVEDGE_PathObject )
		{
			debugf(TEXT("EDGE >%s<"),*CurEdge->GetDebugText());
		}
	}

	// dynamic edges
	for(DynamicEdgeList::TIterator Itt(Mesh->DynamicEdges);Itt;++Itt)
	{
		CurEdge = Itt.Value();
		if( CurEdge->EdgeType == NAVEDGE_PathObject )
		{
			debugf(TEXT("EDGE >%s<"),*CurEdge->GetDebugText());
		}
	}


	for(INT PolyIdx=0;PolyIdx<Mesh->Polys.Num();++PolyIdx)
	{
		FNavMeshPolyBase& CurPoly = Mesh->Polys(PolyIdx);

		UNavigationMeshBase* SubMesh = CurPoly.GetSubMesh();
		if( SubMesh != NULL )
		{
			PrintPOEdgesForMesh(SubMesh);
		}
	}
}

/**
 * PrintAllPathObjectEdges
 * - iterates through all edges and prints out info if they are pathobject edges
 */
void FNavMeshWorld::PrintAllPathObjectEdges()
{
	// loop through all edges, and if it's a path object edge 
	for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
	{
		UNavigationMeshBase* Mesh = ListPylon->GetNavMesh();
		if(Mesh != NULL)	
		{
			PrintPOEdgesForMesh(Mesh);
		}
	}
}

/**
 * VerifyPathObjects
 * - iterates through all edges looking for path object edges to verify all POs that are referenced by the mesh
 */
void FNavMeshWorld::VerifyPathObjects()
{
	// loop through all edges, and if it's a path object edge 
	for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
	{
		UNavigationMeshBase* Mesh = ListPylon->GetNavMesh();
		if(Mesh != NULL)	
		{
			VerifyEdgesInMesh(Mesh);
		}
	}
}

void FNavMeshWorld::VerifyPathObstacles()
{
	FNavMeshWorld* World = GetNavMeshWorld();
	check(World!=NULL);

	TArray<IInterface_NavMeshPathObstacle*> Obstacles;
	World->ActiveObstacles.GenerateKeyArray(Obstacles);
	
	IInterface_NavMeshPathObstacle* CurObstacle = NULL;
	for(INT Idx=0;Idx<Obstacles.Num();++Idx)
	{
		CurObstacle = Obstacles(Idx);
		if(!CurObstacle->VerifyObstacle())
		{
			UObject* Owner = CurObstacle->GetUObjectInterfaceInterface_NavMeshPathObstacle();
			debugf(TEXT("Obstacle at %p"),CurObstacle);
			debugf(TEXT("%s failed verification!"),*Owner->GetName());
			check(FALSE && "Obstacle failed verification");
		}
	}
}

void FNavMeshWorld::PrintObstacleInfo()
{
	FNavMeshWorld* World = GetNavMeshWorld();
	check(World!=NULL);

	TArray<IInterface_NavMeshPathObstacle*> Obstacles;
	World->ActiveObstacles.GenerateKeyArray(Obstacles);

	IInterface_NavMeshPathObstacle* CurObstacle = NULL;
	debugf(TEXT("---- Current active(registered) obstacles: ----"));
	for(INT Idx=0;Idx<Obstacles.Num();++Idx)
	{
		CurObstacle = Obstacles(Idx);
		
		UObject* Owner = CurObstacle->GetUObjectInterfaceInterface_NavMeshPathObstacle();
		debugf(TEXT("REGISTERED Obstacle (%s) at %p "),*Owner->GetName(),CurObstacle);
	}
	debugf(TEXT("Total obstacles: %i"),Obstacles.Num());
	debugf(TEXT("--------------------------------------"));
	debugf(TEXT("---- Current submeshes:                   ----"));
	INT SubmeshTotal = 0;
	for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
	{
		UNavigationMeshBase* Mesh = ListPylon->GetNavMesh();
		if(Mesh != NULL)	
		{
			TArray<FPolyObstacleInfo> ObstInfoList;
			Mesh->PolyObstacleInfoMap.GenerateValueArray(ObstInfoList);

			if( ObstInfoList.Num() > 0 )
			{
				debugf(TEXT("	Submeshes for (%s.%s): "),*ListPylon->GetName(),*Mesh->GetName());
				for(INT Idx=0;Idx<ObstInfoList.Num();++Idx)
				{
					const FPolyObstacleInfo& Info = ObstInfoList(Idx);
					if (Info.SubMesh != NULL)
					{
						++SubmeshTotal;
						debugf(TEXT("		Poly(%i) submesh at %p (%i polys, %i obstacles)"),Info.Poly->Item,Info.SubMesh,Info.SubMesh->Polys.Num(),Info.LinkedObstacles.Num());
					}				
				}
			}

		}
	}
	debugf(TEXT("Total submeshes: %i"),SubmeshTotal);

}

/**
 * Verifies cover references in the passed mesh
 *@param Mesh - the mesh to verify cover refs in
 */
void VerifyCoverReferencesForMesh(UNavigationMeshBase* Mesh)
{

	for(INT PolyIdx=0;PolyIdx<Mesh->Polys.Num();++PolyIdx)
	{
		FNavMeshPolyBase& CurPoly = Mesh->Polys(PolyIdx);

		for(INT CovRefIdx=0;CovRefIdx<CurPoly.PolyCover.Num();++CovRefIdx)
		{
			FCoverReference& CovRef = CurPoly.PolyCover(CovRefIdx);

			ACoverLink* Link = Cast<ACoverLink>(*CovRef);
			check(Link == NULL || !Link->IsPendingKill() );
			check(Link->GetName().Len());
		}

// 		UNavigationMeshBase* SubMesh = CurPoly.GetSubMesh();
// 		if( SubMesh != NULL )
// 		{
// 			PrintPOEdgesForMesh(SubMesh);
// 		}
	}
}

void FNavMeshWorld::VerifyCoverReferences()
{
	// loop through all edges, and if it's a path object edge 
	for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
	{
		UNavigationMeshBase* Mesh = ListPylon->GetNavMesh();
		if(Mesh != NULL)	
		{
			VerifyCoverReferencesForMesh(Mesh);
		}
	}
}

/**
 * for DEBUG only, will draw all edges that don't support the passed entity params 
 * @param PathParams - path params for the entity you want to verify
 */
void FNavMeshWorld::DrawNonSupportingEdges( const FNavMeshPathParams& PathParams )
{	
	GWorld->GetWorldInfo()->FlushPersistentDebugLines();
	for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
	{
		UNavigationMeshBase* Mesh = ListPylon->GetNavMesh();
		if(Mesh != NULL)	
		{
			FNavMeshEdgeBase* CurEdge = NULL;
			for(INT EdgeIdx=0;EdgeIdx<Mesh->GetNumEdges();++EdgeIdx)
			{
				CurEdge = Mesh->GetEdgeAtIdx(EdgeIdx);
				if( CurEdge && ! CurEdge->Supports(PathParams, NULL, NULL) )
				{
					CurEdge->DrawEdge(GWorld->PersistentLineBatcher,FColor(255,0,0));
				}
			}
		}
	}
}



// END FNavMeshWorld Implementation
///////////////////////////////////////////////////////////////////////

/**
 * this is called on postload of pylons to give dynamicpylons a chance to link into them
 * if the dynamic pylon is already loaded when this one is streamed in
 * (e.g. DynamicPylon already loaded and stationary when an adjacent mesh comes in, we still want a link there!)
 * @param PylonToLink - pylon to link up to adjacent dynamic pylons
 */
void LinkToDynamicAdjacentPylons(APylon* PylonToLink)
{
	// dynamic pylons will handle themselves
	if( PylonToLink->IsA(ADynamicPylon::StaticClass()) )
	{
		return;
	}

	TArray<APylon*> Pylons;
	FBox PylonBounds = PylonToLink->GetBounds(WORLD_SPACE).ExpandBy(10.0f);

	UNavigationHandle::GetIntersectingPylons(PylonBounds.GetCenter(), PylonBounds.GetExtent(),Pylons);
	// for each intersecting pylon, add links from MyPlon to it, and from it to MyPylon
	for(INT IsectPylonIdx=0;IsectPylonIdx<Pylons.Num();IsectPylonIdx++)
	{
		ADynamicPylon* CurPylon = Cast<ADynamicPylon>(Pylons(IsectPylonIdx));
		
		if( CurPylon != NULL && CurPylon != PylonToLink && !CurPylon->bMoving)
		{
			CurPylon->RebuildDynamicEdges();
		}
	}

}

void APylon::UpdateMeshForPreExistingNavMeshObstacles()
{
	if( GIsGame && !GIsCooking && !GIsUCC )
	{
		if ( CompatibleWithDynamicObstacles() )
		{

			// if we just got added to the octree we need to see if there are any registered obstacles that we need to worry about
			ObstacleToPolyMapType& ActiveObstacles = FNavMeshWorld::GetNavMeshWorld()->ActiveObstacles;
			IInterface_NavMeshPathObstacle* Obstacle = NULL;


			// build a list of intersecting obstacles, then add them in a second step because it's not safe to iterate over the list while we're adding to it
			TArray<IInterface_NavMeshPathObstacle*> Obstacles;
			ActiveObstacles.GenerateKeyArray(Obstacles);

			TArray<APylon*> Pylons;
			Pylons.AddItem(this);

			for(INT Idx=0;Idx<Obstacles.Num();++Idx)
			{
				Obstacle = Obstacles(Idx);
				FBox PolyBounds(0);
				for(INT ShapeIdx=0;ShapeIdx<Obstacle->GetNumBoundingShapes();++ShapeIdx)
				{
					TArray<FVector> BoundingShape;

					if(!Obstacle->GetBoundingShape(BoundingShape,ShapeIdx))
					{
						continue;
					}

					for(INT Idx=0;Idx<BoundingShape.Num();++Idx)
					{
						PolyBounds += BoundingShape(Idx);
						PolyBounds += BoundingShape(Idx)+FVector(0.f,0.f,10.f);
					}

					if(!GetBounds(WORLD_SPACE).Intersect(PolyBounds))
					{		
						continue;
					}

					FVector Ctr(0.f), Extent(0.f);
					PolyBounds.GetCenterAndExtents(Ctr,Extent);

					TArray<FNavMeshPolyBase*> Polys;

					GetPolysAffectedByObstacleShape(Obstacle, BoundingShape,Ctr,Extent,Polys);

					if( Polys.Num() > 0 )
					{
						Obstacle->RegisterObstacleWithPolys(BoundingShape,Polys);
					}
				}
				Obstacle->UpdateAllDynamicObstaclesInPylonList(Pylons);

			}

			LinkToDynamicAdjacentPylons(this);
		}

	}
}

void APylon::AddToNavigationOctree()
{
	Super::AddToNavigationOctree();
 
	AddToPylonOctree();

	UpdateMeshForPreExistingNavMeshObstacles();
}

//////////////////////////////////////////////////////////////////////////
// Interface_navigationhandle
/**
 * returns the offset from the edge move point this entity should move toward (e.g. how high off the ground we should move to)
 * @param Edge - the edge we're moving to
 * @return - the offset to use
 */
FVector APylon::GetEdgeZAdjust(struct FNavMeshEdgeBase* Edge)
{
	return FVector(0.f);
}

/**
 * this function is responsible for setting all the relevant parmeters used for pathfinding
 * @param out_ParamCache - the output struct to populate params in
 * @NOTE: ALL Params FNavMeshPathParams should be populated
 * 
 */
void APylon::SetupPathfindingParams( struct FNavMeshPathParams& out_ParamCache )
{
	VERIFY_NAVMESH_PARAMS(9);
	out_ParamCache.bAbleToSearch = TRUE;
	out_ParamCache.SearchExtent = DebugPathExtent;
	out_ParamCache.SearchLaneMultiplier = 0.f;
	out_ParamCache.bCanMantle = FALSE;
	out_ParamCache.bNeedsMantleValidityTest = FALSE;
	out_ParamCache.MaxDropHeight = 0.f;
	out_ParamCache.MinWalkableZ=0.7f;
	out_ParamCache.MaxHoverDistance = 0.f;
	out_ParamCache.SearchStart = DebugPathStartLocation;
}

/**
 * Called from FindPath() at the beginning of a path search to give this entity a chance to initialize transient data
 */
void APylon::InitForPathfinding()
{
}
// END Interface_navigationhandle
//////////////////////////////////////////////////////////////////////////

/**
 * called whenever this pylon is turned on or off.. will do necessary work 
 * in area to make sure the state of the mesh is up to date
 */
void APylon::OnPylonStatusChange()
{
	if( NavMeshPtr != NULL )
	{

		// list of handles that need to be notified they are using an edge in this pylon which is being disabled
		TArray< UNavigationHandle* > HandlesNeedingNotification;
		HandlesNeedingNotification.Reset();

		
		// if we're turning this pylon off we need to build a map of edges to handles using them so we can notify 
		// that the edge is now invalid
		if( bDisabled )
		{
			FNavMeshWorld* World = FNavMeshWorld::GetNavMeshWorld();
			

			// build a map of edges to handles that ref them
			UNavigationHandle* CurrentHandle = NULL;
			FNavMeshEdgeBase* CurrentEdge = NULL;

			// FOR EACH active handle
			for(INT HandleIdx=0;HandleIdx<World->ActiveHandles.Num();++HandleIdx)
			{		
				CurrentHandle = World->ActiveHandles(HandleIdx);

				// FOR EACH edge in this handle
				for(INT HandleEdgeIdx=0;HandleEdgeIdx < CurrentHandle->PathCache.Num(); ++HandleEdgeIdx)
				{
					// if the edge being used is from this pylon, save it off so we can notify that handle
					CurrentEdge = CurrentHandle->PathCache(HandleEdgeIdx);
					
					if( CurrentEdge->NavMesh->GetPylon() == this )
					{
						HandlesNeedingNotification.AddItem(CurrentHandle);
						break;
					}
				}
			}
		}

		static TArray<FNavMeshPolyBase*> PolysNeedingRebuild;
		PolysNeedingRebuild.Reset();




		// iterate through all cross pylon edges and if they are actually cross pylon
		// gather polys nearby and see if we need to rebuild submeshes in them
		FNavMeshCrossPylonEdge* CurEdge = NULL;
		for(INT Idx=0;Idx<NavMeshPtr->CrossPylonEdges.Num();++Idx)
		{
			CurEdge = NavMeshPtr->CrossPylonEdges(Idx);

			FNavMeshPolyBase* Poly0 = CurEdge->Poly0Ref.GetPoly(TRUE);
			FNavMeshPolyBase* Poly1 = CurEdge->Poly1Ref.GetPoly(TRUE);

			FNavMeshPolyBase* OtherPoly = NULL;
			if( Poly0 != NULL && Poly0->NavMesh->GetPylon() != this )
			{
				OtherPoly = Poly0;
			}
			else if ( Poly1 != NULL && Poly1->NavMesh->GetPylon() != this )
			{
				OtherPoly = Poly1;
			}
			else
			{
				continue;
			}

			FBox Box(1);
			Box += CurEdge->GetVertLocation(0);
			Box += CurEdge->GetVertLocation(1);
			Box.ExpandBy(CurEdge->EffectiveEdgeLength);

			FVector Ctr(0.f),Extent(0.f);		
			Box.GetCenterAndExtents(Ctr,Extent);
			static TArray<FNavMeshPolyBase*> Query_Polys;
			Query_Polys.Reset();
			OtherPoly->NavMesh->GetIntersectingPolys(Ctr,Extent,Query_Polys,WORLD_SPACE,TRUE,TRUE);

			for(INT QueryIdx=0;QueryIdx<Query_Polys.Num();++QueryIdx)
			{
				FNavMeshPolyBase* CurPoly = Query_Polys(QueryIdx);
				if( CurPoly->NumObstaclesAffectingThisPoly > 0 )
				{
					PolysNeedingRebuild.AddItem(CurPoly);
				}
			}
		}

		IInterface_NavMeshPathObstacle::TriggerRebuildForPassedTLPolys(PolysNeedingRebuild);


		// send notifications after all rebuilds are complete
		for(INT Idx=0;Idx<HandlesNeedingNotification.Num();++Idx)
		{
			UNavigationHandle* Handle = HandlesNeedingNotification(Idx);

			if( Handle != NULL )
			{
				Handle->PostEdgeCleanup(NULL);
			}
		}
	}
}

void APylon::BeginDestroy()
{
	RemoveFromPylonOctree();
	Super::BeginDestroy();
}

void APylon::RemoveFromNavigationOctree()
{
	Super::RemoveFromNavigationOctree();
	RemoveFromPylonOctree();
}

void APylon::RemoveFromPylonOctree()
{	
	if(OctreeId.IsValidId() && OctreeIWasAddedTo != NULL)
	{
		FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree(TRUE);

		//if the octree has changed since we were added, null the reference 
		if(Octree != NULL && Octree == OctreeIWasAddedTo)
		{
			Octree->RemoveElement(OctreeId);
		}
		OctreeIWasAddedTo=NULL;

	}
	OctreeId=FOctreeElementId();
}

void APylon::AddToPylonOctree()
{	
	FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

	if(OctreeIWasAddedTo != Octree)
	{
		OctreeIWasAddedTo=NULL;
	}

	if(Octree != NULL)
	{
		if(OctreeId.IsValidId() && OctreeIWasAddedTo == Octree)
		{
			Octree->RemoveElement(OctreeId);
			OctreeId=FOctreeElementId();
		}


		Octree->AddElement(this);
		OctreeIWasAddedTo=Octree;
	}
}

void APylon::LinkSelection(USelection* SelectedActors)
{
	// loop through all the selected actors, and if it's a volume toggle it being part of our expansion list
	for(INT SelectedIdx=0;SelectedIdx<SelectedActors->Num();++SelectedIdx)
	{
		AVolume* CurVolume = Cast<AVolume>((*SelectedActors)(SelectedIdx));
		if(CurVolume != NULL)
		{
			INT Idx=0;
			if(ExpansionVolumes.FindItem(CurVolume,Idx))
			{
				// it's already in the list.. remove it
				ExpansionVolumes.Remove(Idx,1);
			}
			else
			{
				// not in the list yet, add it!
				ExpansionVolumes.AddItem(CurVolume);
			}
		}

		APylon* CurPylon = Cast<APylon>( (*SelectedActors)(SelectedIdx) );
		if ( CurPylon != NULL && CurPylon != this )
		{
			INT Idx=0;
			if(ImposterPylons.FindItem(CurPylon,Idx))
			{
				// it's already in the list.. remove it
				ImposterPylons.Remove(Idx,1);
			}
			else
			{
				// not in the list yet, add it!
				ImposterPylons.AddItem(CurPylon);
			}
		}
	}
}

void UNavigationHandle::ClearConstraints()
{
	UNavMeshPathConstraint* NextConstraint;
	while (PathConstraintList != NULL)
	{
		NextConstraint = PathConstraintList->NextConstraint;
		PathConstraintList->eventRecycle();
		PathConstraintList = NextConstraint;
	}

	UNavMeshPathGoalEvaluator* NextGoalEval;
	while (PathGoalList != NULL)
	{
		NextGoalEval = PathGoalList->NextEvaluator;
		PathGoalList->eventRecycle();
		PathGoalList = NextGoalEval;
	}

	if(GWorld != NULL)
	{
		AWorldInfo* WI = GWorld->GetWorldInfo();
		if( WI != NULL )
		{
			// release any constraints we allocated from the pools
			WI->ReleaseCachedConstraintsAndEvaluators();
		}
	}
}

void UNavigationHandle::AddPathConstraint( UNavMeshPathConstraint* Constraint )
{
	if( PathConstraintList == NULL )
	{
		PathConstraintList = Constraint;
	}
	else
	{
		UNavMeshPathConstraint* Cur = PathConstraintList;
		while( Cur->NextConstraint != NULL )
		{
			Cur = Cur->NextConstraint;
		}
		Cur->NextConstraint = Constraint;
	}	
}

void UNavigationHandle::AddGoalEvaluator( UNavMeshPathGoalEvaluator* Evaluator )
{
	// ensure this is clear in case this evaluator was cached
	Evaluator->NextEvaluator = NULL;

	if( PathGoalList == NULL )
	{
		PathGoalList = Evaluator;
	}
	else
	{
		UNavMeshPathGoalEvaluator* Cur = PathGoalList;
		while( Cur->NextEvaluator != NULL )
		{
			Cur = Cur->NextEvaluator;
			//checkSlowish(Evaluator != Cur);
		}
		//checkSlowish(Evaluator != Cur);
		if(Cur != Evaluator)
		{
			Cur->NextEvaluator = Evaluator;
		}		
	}
}

UBOOL UNavigationHandle::EvaluateGoal( PathCardinalType PossibleGoal, PathCardinalType& out_GeneratedGoal )
{
	SCOPE_CYCLE_COUNTER(STAT_EvaluateGoalTime);
	UNavMeshPathGoalEvaluator* CurrentGoal = PathGoalList;
	
	VERBOSE_LOG_PATH_POLY_GOALEVAL_START(this,PossibleGoal)

	// OR, one returns TRUE = TRUE
	// OR, all return FALSE = FALSE
	// OR, all return TRUE = TRUE
	// AND, one returns TRUE = FALSE
	// AND, all return fALSE = FALSE
	// AND, all return TRUE = TRUE	
	UBOOL bReturnVal = !bUseORforEvaluateGoal; 
	while( CurrentGoal != NULL)
	{
		// if we already know what the return value is going to be (e.g. one of the evaluators threw the node out) we don't
		// need to call EvaluateGoal on subsequent evaluators UNLESS! they have bAlwaysCallEvaluateGoal set.. in which case
		// they still need to get called
		UBOOL bOutcomeAlreadyDetermined = bReturnVal == bUseORforEvaluateGoal;

		UBOOL bEvaluateGoalPassed = FALSE;
		if( (!bOutcomeAlreadyDetermined || CurrentGoal->bAlwaysCallEvaluateGoal))
		{
			bEvaluateGoalPassed = CurrentGoal->EvaluateGoal( PossibleGoal, CachedPathParams, out_GeneratedGoal );

			VERBOSE_LOG_PATH_POLY_GOALEVAL_STATUS(this,PossibleGoal->GetPathDestinationPoly(),CurrentGoal,*FString::Printf(TEXT("EvaluateGoal returned %u"),bEvaluateGoalPassed))

			// DEBUG STATS - increment the number of nodes we've processed
#if !FINAL_RELEASE
			if(bDebugConstraintsAndGoalEvals)
			{
				CurrentGoal->NumNodesProcessed++;
			}
#endif

			if(bEvaluateGoalPassed == bUseORforEvaluateGoal)
			{
				bReturnVal = bUseORforEvaluateGoal;
			}
			// DEBUG STATS - if evaluategoal returned false, and we're keeping stats record it
#if !FINAL_RELEASE
			if(bDebugConstraintsAndGoalEvals && !bEvaluateGoalPassed)
			{
				CurrentGoal->NumNodesThrownOut++;
			}
#endif
		}
		
		CurrentGoal = CurrentGoal->NextEvaluator;
	}

	// if this was not accepted as a good candidate, clear out any sets of out_GeneratedGoal that may have taken place
	if(bReturnVal == FALSE)
	{
		out_GeneratedGoal = NULL;
	}

	// mark the fact that we've seen this poly this path session 
	FNavMeshPolyBase* PossibleGoalPoly = PossibleGoal->GetPathDestinationPoly(); 
	if (PossibleGoalPoly != NULL )
	{
		PossibleGoalPoly->SavedPathSessionID = PossibleGoal->SavedSessionID; 
	}
	return bReturnVal;
}

/**
 * this will evaluate all the edges in this edge's group, and return the edge which is longest out of the group that supports the passed entity params
 * (so that for edges in the same group that are on top of each other, we only evalute the longest (and therefore best) one, and skip the others since they're redundant)
 * @param CurrentNeighborEdge - edge whose group we are evaluating
 * @param CachedPathParams - params for the entity pathing
 * @param CurrentPoly - the poly we're expanding from
 * @param PredecessorEdge - the predecessor edge we traversed to get to this poly
 * @param SessionID - the sessionID of this path search
 * @return the longest edge in the group
 */
FNavMeshEdgeBase* GetLongestSupportedEdgeInGroup(FNavMeshEdgeBase* CurrentNeighborEdge,const FNavMeshPathParams& CachedPathParams,FNavMeshPolyBase* CurrentPoly,FNavMeshEdgeBase* PredecessorEdge, DWORD SessionID)
{
	static TArray<FNavMeshEdgeBase*> EdgesInGroup;
	EdgesInGroup.Reset();

	CurrentNeighborEdge->GetAllEdgesInGroup(CurrentPoly,EdgesInGroup);

	if( EdgesInGroup.Num() == 1 )
	{
		return EdgesInGroup(0);
	}

	FLOAT BestLength = 0.f;
	FNavMeshEdgeBase* BestEdge = NULL;
	for(INT EdgeIdx=0;EdgeIdx<EdgesInGroup.Num();++EdgeIdx)
	{
		FNavMeshEdgeBase* CurEdge = EdgesInGroup(EdgeIdx);

		CurEdge->ConditionalClear(SessionID);

		// if we've already evaluated this edge's group rank and it's not the longest, skip it
		if( CurEdge->bNotLongestEdgeInGroup )
		{
			continue;
		}

		// tur this on for all edges in group, will be turned off for the one we pick that is longest
		CurEdge->bNotLongestEdgeInGroup=TRUE;

		const FLOAT Rad = Max<FLOAT>(CachedPathParams.SearchExtent.X,CachedPathParams.SearchExtent.Y);

		// if the edge is wide enough itself then we know we're good to go 
		if ( CurEdge->EffectiveEdgeLength+KINDA_SMALL_NUMBER <= Rad )
		{
			continue;
		}

		FLOAT CurEdgeLen = CurEdge->GetEdgeLength();
		if( CurEdgeLen > BestLength )
		{
			BestEdge = CurEdge;
			BestLength = CurEdgeLen;
		}
	}

	if ( BestEdge != NULL )
	{
		BestEdge->bNotLongestEdgeInGroup=FALSE;
	}

	return BestEdge;
}
/** 
 *  Adds successor edges from the given poly to the A* search
 *  @param CurPoly - poly to add successor edges for
 *  @param PathParams - path params being used to search currently
 *  @param PredecessorEdge - edge we got to this poly from
 *  @param PathSessionID - SessionID for this pathfind
 *  @param OpenList - first edge on the open list
 */
void UNavigationHandle::AddSuccessorEdgesForPoly(
							FNavMeshPolyBase* CurrentPoly,
							const FNavMeshPathParams& PathParams,
							FNavMeshEdgeBase* PredecessorEdge,
							INT PathSessionID,
							PathOpenList& OpenList,
							INT OverrideVisitedCost/*=-1*/,
							INT OverrideHeuristicCost/*=-1*/)
{
	SCOPE_CYCLE_COUNTER(STAT_ComputeEdgeTime);
	STAT(INT EdgeIterations = 0);

	// build a list of the edges we're going to evaluate (skip edges in the same group, except for the longest supported one)
	static TArray<FNavMeshEdgeBase*> EdgesToEval;
	static TArray<FLOAT> CachedEdgeLengths;
	static TArray<FNavMeshPolyBase*> CachedNeighborPolys;
	EdgesToEval.Reset();
	CachedEdgeLengths.Reset();
	CachedNeighborPolys.Reset();

	INT NumEdges = CurrentPoly->GetNumEdges();
	
	FNavMeshEdgeBase* CurrentNeighborEdge = NULL; 
	FNavMeshEdgeBase* InnerEdgeToBeEvald = NULL;
	FNavMeshPolyBase* CurrentNeighborPoly = NULL;

	for( INT EdgeIdx = 0; EdgeIdx < NumEdges; ++EdgeIdx )
	{
		CurrentNeighborEdge = CurrentPoly->GetEdgeFromIdx(EdgeIdx);
		if( CurrentNeighborEdge == NULL)
		{
			continue;
		}

		checkSlowish(CurrentNeighborEdge->GetPoly0() == NULL || CurrentNeighborEdge->GetPoly0() != CurrentNeighborEdge->GetPoly1());
		CurrentNeighborPoly = CurrentNeighborEdge->GetOtherPoly(CurrentPoly);

		// ensure that the mesh for this edge is up to date
		CurrentPoly->UpdateDynamicObstaclesForEdge(PathSessionID,CurrentNeighborPoly);
		
		if (CurrentNeighborEdge->EdgeType == NAVEDGE_BackRefDummy || CurrentNeighborEdge->IsInSameGroupAs(PredecessorEdge) )
		{
			// completely ignore backrefs so they don't clutter up the debug space
			continue;
		}

		FLOAT CurrentNeighborEdgeLengthSq = CurrentNeighborEdge->GetEdgeLengthSq();

		// this can happen legitimately (for example if the edge is cross-level and the other level isn't loaded)
		checkSlow(CurrentNeighborPoly != CurrentPoly);
		if(CurrentNeighborPoly == NULL || CurrentNeighborPoly == CurrentPoly)
		{
			VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(CurrentNeighborEdge,*FString::Printf(TEXT("Invalid Neighbor Poly %d (CurPoly %d)"),CurrentNeighborPoly, CurrentPoly),this)
			continue;
		}

		// make sure new poly is clean
		CurrentNeighborEdge->ConditionalClear(PathSessionID);

		// look for edges already saved, that are in the same group
		INT InnerEdgeInSameGroupIdx=-1;
		
		for (INT EvalEdgeIdx=0;EvalEdgeIdx<EdgesToEval.Num();++EvalEdgeIdx)
		{
			InnerEdgeToBeEvald = EdgesToEval(EvalEdgeIdx);

			// if they're in the same group, save the index and bail
			if( InnerEdgeToBeEvald->IsInSameGroupAs(CurrentNeighborEdge) )
			{
				InnerEdgeInSameGroupIdx=EvalEdgeIdx;
				break;
			}
		}

		// if we found an edge in the same group, only the longest shall survive!
		if( InnerEdgeInSameGroupIdx != -1 && CurrentNeighborEdgeLengthSq > CachedEdgeLengths(InnerEdgeInSameGroupIdx) )
		{
			
			if( !CurrentNeighborEdge->Supports(CachedPathParams, CurrentPoly, PredecessorEdge) )
			{
				VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(CurrentNeighborEdge,(CurrentNeighborEdge)?TEXT("Edge does not support this entity (supports returned FALSE)") : TEXT("Edge was NULL (invalid)"),this)
			}
			else
			{
				EdgesToEval(InnerEdgeInSameGroupIdx) = CurrentNeighborEdge;
				CachedEdgeLengths(InnerEdgeInSameGroupIdx) = CurrentNeighborEdgeLengthSq;
				CachedNeighborPolys(InnerEdgeInSameGroupIdx) = CurrentNeighborPoly;
			}
		}
		// if we found a group already, and it wasn't the longest, don't bother doing anything else
		else if( InnerEdgeInSameGroupIdx == -1 )
		{
			if( !CurrentNeighborEdge->Supports(CachedPathParams, CurrentPoly, PredecessorEdge) )
			{
				VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(CurrentNeighborEdge,(CurrentNeighborEdge)?TEXT("Edge does not support this entity (supports returned FALSE)") : TEXT("Edge was NULL (invalid)"),this)
			}
			else
			{
				// add a new entry! we haven't seen this group before
				EdgesToEval.AddItem(CurrentNeighborEdge);
				CachedEdgeLengths.AddItem(CurrentNeighborEdgeLengthSq);
				CachedNeighborPolys.AddItem(CurrentNeighborPoly);
			}
		}
		else
		{
			// if it got here, it means CurrentNeighborEdge was in the same group as an edge already added, and was shorter
			continue;
		}
	}


	for( INT EdgeIdx = 0; EdgeIdx < EdgesToEval.Num(); ++EdgeIdx )
	{
		STAT(EdgeIterations++);

		CurrentNeighborEdge = EdgesToEval(EdgeIdx);
		CurrentNeighborPoly = CachedNeighborPolys(EdgeIdx);

		FVector PreviousPathPoint = (CurrentPoly==AnchorPoly || PredecessorEdge == NULL ) ? PathParams.SearchStart : PredecessorEdge->PreviousPosition;
		FVector UsedEdgePoint = PreviousPathPoint;// the point the edge used to compute the cost (will become previous point for successors)
		INT InitialCost = OverrideVisitedCost;
		
		if( InitialCost == -1 )
		{
			InitialCost = CurrentNeighborEdge->CostFor(CachedPathParams,PreviousPathPoint,UsedEdgePoint,CurrentPoly);

			if( InitialCost <= 0 )
			{
				VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(CurrentNeighborEdge,*FString::Printf(TEXT("IntialCost was < 0 (%i), skipping edge"),InitialCost),this)

				// Skip broken paths, or paths we've already traversed
				continue;
			}

			if( InitialCost >= UCONST_BLOCKEDPATHCOST )
			{
				VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(CurrentNeighborEdge,TEXT("IntialCost was > BLOCKEDPATHCOST"),this)
				// don't bother with blocked paths
				continue;
			}
		}

		// apply our path constraints to this edge
		INT HeuristicCost = 0;

		// only apply constraints if we don't have overidden cost values and we need to figure 
		if( OverrideVisitedCost == -1 || OverrideHeuristicCost == -1)
		{
			
			if( !ApplyConstraints( CurrentNeighborEdge, PredecessorEdge, CurrentPoly, CurrentNeighborPoly, InitialCost, HeuristicCost, UsedEdgePoint ) )
			{
				continue; 
			}
		}

		if( OverrideHeuristicCost != -1 )
		{
			HeuristicCost = OverrideHeuristicCost;
		}

		UBOOL bIsOnClosed = CurrentNeighborEdge->bAlreadyVisited;
		UBOOL bIsOnOpen	  = CurrentNeighborEdge->PrevOpenOrdered != NULL || 
			CurrentNeighborEdge->NextOpenOrdered != NULL || 
			OpenList == CurrentNeighborEdge;

		if( bIsOnClosed || bIsOnOpen )
		{
			// as long as the costs already in the list is as good or better, throw this sucker out
			INT PredecessorCost = (PredecessorEdge!=NULL) ? PredecessorEdge->VisitedPathWeight : 0;
			if( CurrentNeighborEdge->VisitedPathWeight <= InitialCost + PredecessorCost)
			{
				//VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(Edge,*FString::Printf(TEXT("Already in list and not as good %d vs %d (%d/%d)"), CurrentNeighbor->visitedWeight, (InitialCost + CurrentPoly->visitedWeight), InitialCost, CurrentPoly->visitedWeight),this)

				continue; 
			}
			// otherwise the incoming value is better, so pull it out of the lists
			else
			{
				if( bIsOnClosed )
				{
					CurrentNeighborEdge->bAlreadyVisited = FALSE;
				}
				if( bIsOnOpen )
				{
					RemoveNodeFromOpen( CurrentNeighborEdge, OpenList );
				}
			}
		}

		// add the new node to the open list
		if( !AddNodeToOpen( OpenList, CurrentNeighborEdge, InitialCost, HeuristicCost, PredecessorEdge, UsedEdgePoint, CurrentNeighborPoly ) )
		{
			//VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(Edge,TEXT("FAILED TO ADD TO OPEN LIST"),this)

			break;
		}

		// add to closed
		CurrentNeighborEdge->bAlreadyVisited=TRUE;

		VERBOSE_LOG_EDGE_TRAVERSAL_SUCCESS(CurrentNeighborEdge, FString::Printf(TEXT("VW: %d BPW: %d"), CurrentNeighborEdge->VisitedPathWeight, CurrentNeighborEdge->EstimatedOverallPathWeight ), this );
	}

	// update stats with how many times we iterated over edges
	INC_DWORD_STAT_BY(STAT_EdgeIterations, EdgeIterations);
}

/**
 * returns TRUE if the passed segment (start,end) leaves the poly supplied
 * @param Poly - poly to check against
 * @param start - start point of segment
 * @param end - end point of segment
 * @return TRUE if the segment leaves the poly
 */
UBOOL DoesTrajectoryLeavePoly(FNavMeshPolyBase* Poly, const FVector& Start, const FVector& End)
{
	FVector EntryPt(0.f), ExitPt(0.f);
	UBOOL Result = Poly->IntersectsPoly2D(Start,End,EntryPt,ExitPt,WORLD_SPACE);

	// what we really want to know is if the segment ever leaves the poly, if it does there will either be no intersection,
	// or the isect points will be off the edge points of the segment
	if( !Result || (		
		(!EntryPt.Equals(Start,1.f) && !EntryPt.Equals(End,1.f)) &&
		(!ExitPt.Equals(Start,1.f) && !ExitPt.Equals(End,1.f))
		)
		)
	{
		return TRUE;
	}


	return FALSE;
}

/**
 * determines if a move from this edge to the provided edge is valid, given the supplied pathparams
 * @param PathParams - pathing parameters we're doing our determination for
 * @param NextEdge - the edge we need to move to from this one
 * @param PolyToMoveThru - the polygon we'd be moving through edge to edge
 * @return TRUE if the move is possible
 */
#define EDGE_ON_LINE_THRESH 1.f
#define MIN_OVERLAP_DIST 5.0f // should be roughly min_edge_dist
UBOOL FNavMeshEdgeBase::SupportsMoveToEdge(const FNavMeshPathParams& PathParams, FNavMeshEdgeBase* NextEdge, FNavMeshPolyBase* PolyToMoveThru)
{
	const FVector MyV0 = GetVertLocation(0);
	const FVector MyV1 = GetVertLocation(1);
	const FVector NextV0 = NextEdge->GetVertLocation(0);
	const FVector NextV1 = NextEdge->GetVertLocation(1);

	// EARLY OUT project next edge onto this one, and see if there is overlap
	const FVector ThisDelta = MyV1 - MyV0;
	const FLOAT ThisLen = ThisDelta.Size();
	const FVector ThisDir = ThisDelta/ThisLen;

	const FLOAT DistAlongDirTo_NextV0 = Clamp<FLOAT>((NextV0 - MyV0) | ThisDir,0.f,ThisLen);
	const FLOAT DistAlongDirTo_NextV1 = Clamp<FLOAT>((NextV1 - MyV0) | ThisDir,0.f,ThisLen);

	// find overlap
	const FLOAT Overlap = Abs<FLOAT>(DistAlongDirTo_NextV0-DistAlongDirTo_NextV1);


	if( Overlap > MIN_OVERLAP_DIST )
	{
		return TRUE;
	}

	const FLOAT EntRad = PathParams.SearchExtent.X;

	FVector ClosestPtOnThis_ToNextEdge(0.f), ClosestPtOnNextEdge_ToThis(0.f);

	// find two closest points on the edges to use for comparison
	PointDistToSegment(PreviousPosition,NextV0,NextV1,ClosestPtOnNextEdge_ToThis);
 	FLOAT DistBetweenPts = PointDistToSegment(ClosestPtOnNextEdge_ToThis, MyV0,MyV1,ClosestPtOnThis_ToNextEdge);
	
	// early out if they're right on top of each other
	if ( DistBetweenPts < EntRad )
	{
		return TRUE;
	}

	const FVector PolyCtr = PolyToMoveThru->GetPolyCenter();

	// find dirs perpendicular to the edges which point into the poly
	FVector This_Perp = GetEdgePerpDir();
	if( ( (PolyCtr - ClosestPtOnThis_ToNextEdge) | This_Perp ) < 0.f )
	{
		This_Perp *= -1.0f;
	}
	
	FVector Next_Perp = NextEdge->GetEdgePerpDir();
	if( ( (PolyCtr - ClosestPtOnNextEdge_ToThis) | Next_Perp ) < 0.f )
	{
		Next_Perp *= -1.0f;
	}

	const FVector ThisTestPt = ClosestPtOnThis_ToNextEdge + This_Perp*EntRad;
	const FVector NextTestPt = ClosestPtOnNextEdge_ToThis + Next_Perp*EntRad;

	if (DoesTrajectoryLeavePoly(PolyToMoveThru,ThisTestPt,NextTestPt))
	{
		VERBOSE_LOG_PATH_SUPPORTSMOVETOEDGEFAIL(PathParams.Interface,NextEdge,this,ClosestPtOnNextEdge_ToThis);
		return FALSE;
	}


	return TRUE;
}




ANavigationPoint* EE_PopBestNode(ANavigationPoint*& OpenList)
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

UBOOL EE_InsertSorted(ANavigationPoint* NodeForInsertion, ANavigationPoint*& OpenList)
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
		if( LoopCounter++ > 4096 )
		{
			return FALSE;
		}
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

void EE_RemoveNodeFromOpen(ANavigationPoint* NodeToRemove, ANavigationPoint*& OpenList)
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


UBOOL EE_AddToOpen(ANavigationPoint*& OpenList, ANavigationPoint* NodeToAdd, ANavigationPoint* GoalNode, INT EdgeCost, UReachSpec* EdgeSpec)
{
	const FVector DirToGoal = (GoalNode->Location - NodeToAdd->Location).SafeNormal2D();
	ANavigationPoint* Predecessor = EdgeSpec->Start;
	NodeToAdd->visitedWeight = EdgeCost + Predecessor->visitedWeight;
	NodeToAdd->previousPath = Predecessor;
	NodeToAdd->bestPathWeight = EdgeCost + appTrunc((NodeToAdd->Location-GoalNode->Location).Size());
	if(	NodeToAdd->bestPathWeight <= 0 )
	{
		debugf( TEXT("Path Warning!!! Got neg/zero adjusted cost for %s"),*EdgeSpec->GetName());
		DEBUGPATHLOG(FString::Printf(TEXT("Path Warning!!! Got neg/zero adjusted cost for %s"),*EdgeSpec->GetName()));
		NodeToAdd->bAlreadyVisited = TRUE;
		return TRUE;
	}

	return EE_InsertSorted(NodeToAdd,OpenList);
}

UBOOL UNavigationHandle::DoesPylonAHaveAPathToPylonB(APylon* A,APylon* B)
{
	return (BuildFromPylonAToPylonB(A,B) != NULL);
}

void APylon::ClearForPathFinding()
{
	Super::ClearForPathFinding();
	bPylonInHighLevelPath=FALSE;
}

/**
 * returns whether or not pylon A has any path to Pylon B (useful for high level early outs)
 */
APylon* UNavigationHandle::BuildFromPylonAToPylonB(APylon* A, APylon* B)
{
	if ( A == B )
	{
		return A;
	}

	if( A==NULL || B==NULL || A->bDisabled || B->bDisabled )
	{
		return NULL;
	}

	for( APylon* ListPylon = GWorld->GetWorldInfo()->PylonList; ListPylon != NULL; ListPylon = ListPylon->NextPylon )
	{
		ListPylon->ClearForPathFinding();
	}

	// put start pylon on the open list 
	ANavigationPoint* OpenList = A;
	
	// ++ Begin A* Loop
	while(OpenList != NULL)
	{
		// pop best node from Open list
		ANavigationPoint* CurrentNode = EE_PopBestNode(OpenList);

		// if the node we just pulled from the open list is the goal, we're done!
		if(CurrentNode == B)
		{
			return B;
		}

		INT NumVisits=0;
		INT MaxPathVisits=4096;

		// cap Open list pops at MaxPathVisits (after we check for goal, because if we just popped the goal off we don't want to do this :) )
		if(	++NumVisits > MaxPathVisits )
		{
			return NULL;
		}

		// for each edge leaving CurrentNode
		for (INT PathIdx = 0; PathIdx < CurrentNode->PathList.Num(); PathIdx++)
		{
			// if it is a valid connection that the pawn can walk
			UReachSpec *Spec = CurrentNode->PathList(PathIdx);
			if (Spec == NULL || Spec->bDisabled || *Spec->End == NULL || Spec->End->ActorIsPendingKill())
			{
				continue;
			}

			APylon *CurrentNeighbor = Cast<APylon>(Spec->End.Nav());

			if( CurrentNeighbor == NULL || CurrentNeighbor->bDisabled )
			{
				continue;
			}

			// Cache the cost of this node
			INT InitialCost = Spec->Distance;

			// Make sure cost is valid
			if( InitialCost <= 0 )
			{
				//debugf( TEXT("Path Warning!!! Cost from %s to %s is zero/neg %i -- %s"), *CurrentNode->GetFullName(), *CurrentNeighbor->GetFullName(), InitialCost, *Spec->GetName() );
				DEBUGPATHLOG(FString::Printf(TEXT("Path Warning!!! Cost from %s to %s is zero/neg %i -- %s"), *CurrentNode->GetFullName(), *CurrentNeighbor->GetFullName(), InitialCost, *Spec->GetName() ));
				InitialCost = 1;
			}

			if( InitialCost >= UCONST_BLOCKEDPATHCOST )
			{
				// don't bother with blocked paths
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
						EE_RemoveNodeFromOpen(CurrentNeighbor,OpenList);	
					}
				}
			}

			// add the new node to the open list
			if( EE_AddToOpen(OpenList,CurrentNeighbor,B,InitialCost,Spec) == FALSE )
			{
				break;
			}
		}

		// mark the node we just explored from as closed (e.g. add to closed list)
		CurrentNode->bAlreadyVisited = TRUE;
	}
	// -- End A* loop

	// ran out of nodes on the open list, no connection found!
	return NULL;
}

/**
 * internal function which does the heavy lifting for A* searches (typically called from FindPath()
 * @param out_DestActor - output param goal evals can set if a particular actor was the result of the search
 * @param out_DestItem - output param goal evans can set if they need an index into something (e.g. cover slot)
 * @return - TRUE If search found a goal
 */
UBOOL UNavigationHandle::GeneratePath( AActor** out_DestActor, INT* out_DestItem )
{

#if DO_PATHCONSTRAINT_PROFILING
	HandleConstraintProfileData.Empty();
	HandleCallCount=0;
	HandleCallMax=-1.0f;
	HandleCallAvg=-1.f;
	HandleCallTotal=0.f;
#endif

	SCOPE_CYCLE_COUNTER(STAT_GeneratePathTime);

	UObject* InterfaceObj = CachedPathParams.Interface->GetUObjectInterfaceInterface_NavigationHandle();
	check(InterfaceObj);
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();


	// check if we should early out
	// InitializeSearch responsible for setting NavigationHandle::LastPathError if it fails
	if( PathGoalList == NULL ||
		!PathGoalList->InitializeSearch( this, CachedPathParams ) )
	{
		VERBOSE_LOG_PATH_MESSAGE(this,TEXT("SEARCH FAILED! IntializeSearch return FALSE!"))

		return FALSE;
	}


	// path session ID keeps track of whether we need to clear nodes or not
	static DWORD PathSessionID = 0;
	PathSessionID++;

	FNavMeshEdgeBase* OpenList = NULL;

	// SeedWorkingSet responsible for setting NavigationHandle::LastPathError if it fails
	if( !PathGoalList->SeedWorkingSet( OpenList, AnchorPoly, PathSessionID, this, CachedPathParams ) )
	{
		VERBOSE_LOG_PATH_MESSAGE(this,*FString::Printf(TEXT("FAILED TO SEED WORKING SET! first pathgoaleval is %s"),*PathGoalList->GetName()))

		return FALSE;
	}

	UNavMeshPathGoalEvaluator* GoalCheck = PathGoalList;
	// maximum number of nodes to be popped from the open list before we bail out
	INT MaxPathVisits = 0;
	for (UNavMeshPathGoalEvaluator* CurrentGoal = GoalCheck; CurrentGoal != NULL; CurrentGoal = CurrentGoal->NextEvaluator)
	{
		MaxPathVisits = Max<INT>(MaxPathVisits, CurrentGoal->MaxPathVisits);
	}
	// set a default value if none was specified in the goal evaluator
	if (MaxPathVisits == 0)
	{
		MaxPathVisits = UNavMeshPathGoalEvaluator::StaticClass()->GetDefaultObject<UNavMeshPathGoalEvaluator>()->MaxPathVisits;
	}
	INT NumVisits = 0;

	// ++ Begin A* Loop

	FNavMeshEdgeBase* GeneratedGoal = NULL;
	while( OpenList != NULL )
	{
		FNavMeshEdgeBase* PredecessorEdge = PopBestNode(OpenList);
		FNavMeshPolyBase* CurrentPoly = (PredecessorEdge->DestinationPolyID) ? PredecessorEdge->GetPoly1() : PredecessorEdge->GetPoly0(); 
		
		// check to see if the edge we just pulled off the open list is the one we were looking for!
		if(EvaluateGoal(PredecessorEdge,GeneratedGoal))
		{
			VERBOSE_LOG_PATH_MESSAGE(this, TEXT("EvaluateGoal return TRUE.. we found a valid goal!"))			
			// we found a valid goal
			break;
		}

		// cap Open list pops at MaxPathVisits (after we check for goal, because if we just popped the goal off we don't want to do this :) )
		if(	++NumVisits > MaxPathVisits )
		{
			debugf(NAME_DevPath, TEXT("Path Warning!!! %s Exceeded maximum path visits while trying to path from %s, Returning best guess."), *InterfaceObj->GetName(), *CachedPathParams.SearchStart.ToString());
			NAVHANDLE_DEBUG_LOG(*FString::Printf(TEXT("WARNING! Path is terminating because max path visits was hit (MaxPathVisits:%i).  Was trying to path from %s.. returning best guess."),MaxPathVisits,*CachedPathParams.SearchStart.ToString()));
			VERBOSE_LOG_PATH_MESSAGE(this,TEXT("We have exceeded our maximum path visits.. so we're bailing!"))
			GoalCheck->NotifyExceededMaxPathVisits( PredecessorEdge, GeneratedGoal );
			break;
		}

		// add successor edges if they're supported, etc
		AddSuccessorEdgesForPoly(CurrentPoly,CachedPathParams,PredecessorEdge,PathSessionID,OpenList);

		VERBOSE_LOG_PATH_STEP(this,OpenList)
	}
	// -- End A* loop


	UBOOL bPathComplete=FALSE;

	{
		SCOPE_CYCLE_COUNTER(STAT_DetermineGoalTime);
		bPathComplete = GoalCheck->DetermineFinalGoal( GeneratedGoal, out_DestActor, out_DestItem );
	}

	if( bPathComplete )
	{
		SCOPE_CYCLE_COUNTER(STAT_SavePathTime);		
		if( GeneratedGoal != NULL )
		{
			GoalCheck->SaveResultingPath( AnchorPoly, GeneratedGoal->GetPathDestinationPoly(), this, GeneratedGoal );
		}
		VERBOSE_LOG_PATH_FINISH(this,OpenList,TEXT("Path finished, and DetermineFinalGoal returned TRUE.. search was a success!"));
	}
	else
	{
		if( GeneratedGoal )
		{
			SCOPE_CYCLE_COUNTER(STAT_SavePathTime);
			GoalCheck->SaveResultingPath( AnchorPoly, GeneratedGoal->GetPathDestinationPoly(), this, GeneratedGoal );
		}
		VERBOSE_LOG_PATH_FINISH(this,OpenList, TEXT("Path finished, and DetermineFinalGoal returned FALSE.. search was a FAILURE!"))

		SetPathError(PATHERROR_NOPATHFOUND);
	}

#if DO_PATHCONSTRAINT_PROFILING

	debugf(TEXT("-----------------------PATH CONSTRAINT STATS for %s---------------------"),*InterfaceObj->GetName());
	
	//add up total
	FLOAT RealTotal = 0.f;
	for(INT Idx=0;Idx<HandleConstraintProfileData.Num();++Idx)
	{
		RealTotal += HandleConstraintProfileData(Idx).TotalTime;
	}

	for(INT Idx=0;Idx<HandleConstraintProfileData.Num();++Idx)
	{
		const FName ConstraintName = HandleConstraintProfileData(Idx).ConstraintName;
		debugf(TEXT("Time: %3.3fms (Per call: %3.3fms avg(%3.3fmsmax)) CallCount:%i PctOverall:%.2f%% -- %s"),
			HandleConstraintProfileData(Idx).TotalTime * GSecondsPerCycle * 1000.f,
			HandleConstraintProfileData(Idx).AvgTime * GSecondsPerCycle * 1000.f,
			HandleConstraintProfileData(Idx).MaxTime * GSecondsPerCycle * 1000.f,
			HandleConstraintProfileData(Idx).CallCount,
			(RealTotal > 0.f) ? HandleConstraintProfileData(Idx).TotalTime/RealTotal*100.f : 0.f,
			*ConstraintName.ToString());			
	}
	const DWORD SamplingError = (HandleCallTotal - RealTotal);
	debugf(TEXT("OVERALL TIME:  %3.3fms (%3.3fms sampling time) (per call %3.3fms avg, %3.3fms max), CallCount: %i")
		,RealTotal * GSecondsPerCycle * 1000.f
		,(RealTotal - SamplingError)*GSecondsPerCycle * 1000.f
		,HandleCallAvg * GSecondsPerCycle * 1000.f
		,HandleCallMax * GSecondsPerCycle * 1000.f
		,HandleCallCount
		);
	debugf(TEXT("-------------------------------------------------------------------------------------------------"));
#endif

// **** DEBUGGING CONSTRAINT STATISTICS ****
#if !FINAL_RELEASE 
	if(bDebugConstraintsAndGoalEvals)
	{
		
		FLOAT TotalAddedPathCost = 0.f;
		FLOAT TotalAddedHeuristicCost = 0.f;
		INT TotalNodesThrownOut = 0;
		INT TotalNodesProcessed = 0;
		
		// gather totals (for path constraints) 
		UNavMeshPathConstraint* CurConstraint = PathConstraintList;
		while(CurConstraint != NULL)
		{
			TotalAddedPathCost += CurConstraint->AddedDirectCost;
			TotalAddedHeuristicCost += CurConstraint->AddedHeuristicCost;
			TotalNodesThrownOut += CurConstraint->NumThrownOutNodes;
			CurConstraint = CurConstraint->NextConstraint;
		}

		debugf(TEXT("------- PATH CONSTRAINT STATS --------"));
		CurConstraint = PathConstraintList;
		while(CurConstraint != NULL)
		{
			debugf(TEXT("Processed: %i ThrownOut: %i (%.2f%% thrown out) AddedPathCost: %.2f (%.2f%% total) AddedHeuristic: %.2f (%.2f%% total) - (%s)"),
					CurConstraint->NumNodesProcessed,
					CurConstraint->NumThrownOutNodes,
					(CurConstraint->NumNodesProcessed>0)?(FLOAT)CurConstraint->NumThrownOutNodes/(FLOAT)CurConstraint->NumNodesProcessed*100.f:0.f,
					CurConstraint->AddedDirectCost, 
					(TotalAddedPathCost>0.f) ? CurConstraint->AddedDirectCost/TotalAddedPathCost*100.f : 0.f,
					CurConstraint->AddedHeuristicCost,
					(TotalAddedHeuristicCost>0.f) ? CurConstraint->AddedHeuristicCost/TotalAddedHeuristicCost*100.f:0.f,
					*CurConstraint->GetName());
			CurConstraint = CurConstraint->NextConstraint;
		}
		debugf(TEXT("--------------------------------------"));
		debugf(TEXT("TotalThrownOut: %i TotalAddedDirectCost: %.2f TotalAddedHeuristicCost: %.2f")
			,TotalNodesThrownOut
			,TotalAddedPathCost
			,TotalAddedHeuristicCost);

		// gather total nodes thrown out (for goal evals) 
		TotalNodesThrownOut=0;
		UNavMeshPathGoalEvaluator* CurGoalEval = PathGoalList;
		while(CurGoalEval != NULL)
		{
			TotalNodesThrownOut += CurGoalEval->NumNodesThrownOut;
			CurGoalEval = CurGoalEval->NextEvaluator;
		}

		debugf(TEXT("------- GOAL EVALUATOR STATS --------"));
		CurGoalEval = PathGoalList;
		while(CurGoalEval != NULL)
		{
			debugf(TEXT("Threw Out %i (out of %i processed (%.2f%%)) (Responsible for %.2f%% of all nodes thrown out) - %s "),
				CurGoalEval->NumNodesThrownOut,
				CurGoalEval->NumNodesProcessed,
				(CurGoalEval->NumNodesProcessed>0) ? (FLOAT)CurGoalEval->NumNodesThrownOut/(FLOAT)CurGoalEval->NumNodesProcessed*100.f : 0.f,
				(TotalNodesThrownOut>0) ? (FLOAT)CurGoalEval->NumNodesThrownOut/(FLOAT)TotalNodesThrownOut*100.f : 0.f,
				*CurGoalEval->GetName());

			UNavMeshGoal_GenericFilterContainer* Container = Cast<UNavMeshGoal_GenericFilterContainer>(CurGoalEval);
			if( Container != NULL )
			{
				debugf(TEXT("\t------- Container EVALUATOR STATS --------"));

				for( INT FilterIdx = 0; FilterIdx < Container->GoalFilters.Num(); ++FilterIdx )
				{
					UNavMeshGoal_Filter* Filter = Container->GoalFilters( FilterIdx );
					if( Filter != NULL )
					{
						debugf(TEXT("\tThrew Out %i (out of %i processed (%.2f%%)) (Responsible for %.2f%% of all nodes thrown out) - %s "),
							Filter->NumNodesThrownOut,
							Filter->NumNodesProcessed,
							(Filter->NumNodesProcessed>0) ? (FLOAT)Filter->NumNodesThrownOut/(FLOAT)Filter->NumNodesProcessed*100.f : 0.f,
							(TotalNodesThrownOut>0) ? (FLOAT)Filter->NumNodesThrownOut/(FLOAT)TotalNodesThrownOut*100.f : 0.f,
							*Filter->GetName());
					}
				}
				
			}

			CurGoalEval = CurGoalEval->NextEvaluator;

		}
		debugf(TEXT("---------------------------------------\n"));
	}
#endif

	return bPathComplete;
}

/************************************************************************
A* functions follow
************************************************************************/
/**
 * internal function which does the heavy lifting for A* searches (typically called from FindPath()
 * @param out_DestActor - output param goal evals can set if a particular actor was the result of the search
 * @param out_DestItem - output param goal evans can set if they need an index into something (e.g. cover slot)
 * @return - TRUE If search found a goal
 */
PathCardinalType UNavigationHandle::PopBestNode( PathOpenList& OpenList )
{
	PathCardinalType Best = OpenList;
	OpenList = Best->NextOpenOrdered;
	if( OpenList != NULL )
	{
		OpenList->PrevOpenOrdered = NULL;
	}
	// indicate this node is no longer on the open list
	Best->PrevOpenOrdered = NULL;
	Best->NextOpenOrdered = NULL;
	return Best;
}

UBOOL UNavigationHandle::InsertSorted( PathCardinalType NodeForInsertion, PathOpenList& OpenList )
{
	// if list is empty insert at the beginning
	if(OpenList == NULL)
	{
		OpenList = NodeForInsertion;
		NodeForInsertion->NextOpenOrdered = NULL;
		NodeForInsertion->PrevOpenOrdered = NULL;
		return TRUE;
	}

	PathCardinalType CurrentNode = OpenList;
#if !PS3 && !FINAL_RELEASE
	INT LoopCounter = 0;
#endif
	
	for(INT OpenListSize=0;CurrentNode != NULL;CurrentNode = CurrentNode->NextOpenOrdered,++OpenListSize)
	{
		if( PathGoalList != NULL && PathGoalList->bDoPartialAStar && OpenListSize >= PathGoalList->MaxOpenListSize )
		{
			return FALSE;
		}
#if !PS3 && !FINAL_RELEASE
//		checkFatalPathFailure(LoopCounter++ <= MAX_LOOP_ITTS, TEXT("Infinite loop detected in A*::InsertSorted!  Try rebuilding paths."),FALSE);
#endif
		if(NodeForInsertion->EstimatedOverallPathWeight <= CurrentNode->EstimatedOverallPathWeight)
		{
			checkSlow(NodeForInsertion != CurrentNode);
			NodeForInsertion->NextOpenOrdered = CurrentNode;
			NodeForInsertion->PrevOpenOrdered = CurrentNode->PrevOpenOrdered;
			if(CurrentNode->PrevOpenOrdered != NULL)
			{
				CurrentNode->PrevOpenOrdered->NextOpenOrdered = NodeForInsertion;
			}
			else
			{
				OpenList = NodeForInsertion;
			}
			CurrentNode->PrevOpenOrdered = NodeForInsertion;
			return TRUE;
		}

		if(CurrentNode->NextOpenOrdered == NULL)
			break;
	}

	// if we got here, append to the end
	CurrentNode->NextOpenOrdered = NodeForInsertion;
	NodeForInsertion->PrevOpenOrdered = CurrentNode;
	return TRUE;

}

UBOOL UNavigationHandle::AddNodeToOpen( PathOpenList& OpenList,
									   PathCardinalType NodeToAdd,
									   INT EdgeCost, INT HeuristicCost,
									   PathCardinalType Predecessor,
									   const FVector& PrevPos,
									   FNavMeshPolyBase* DestinationPolyForEdge)
{
	if(Predecessor != NULL)
	{
		NodeToAdd->VisitedPathWeight = EdgeCost + Predecessor->VisitedPathWeight;

		if( Predecessor->PreviousPathEdge == NodeToAdd )
		{
			PRINTDEBUGPATHLOG(TRUE);
		}
		checkSlow(Predecessor->PreviousPathEdge != NodeToAdd);
		checkSlow(NodeToAdd != Predecessor);

		DEBUGREGISTERCOST( NodeToAdd, TEXT("Previous"), Predecessor->VisitedPathWeight );
	}
	else
	{
		NodeToAdd->VisitedPathWeight = EdgeCost;
	}
	NodeToAdd->PreviousPathEdge	 = Predecessor;
	NodeToAdd->PreviousPosition = PrevPos;
	
	NodeToAdd->EstimatedOverallPathWeight= NodeToAdd->VisitedPathWeight + HeuristicCost;
	NodeToAdd->DestinationPolyID = (NodeToAdd->GetPoly1() == DestinationPolyForEdge) ? 1 : 0;
	return InsertSorted( NodeToAdd, OpenList );
}

void UNavigationHandle::RemoveNodeFromOpen( PathCardinalType NodeToRemove, PathOpenList& OpenList )
{
	if(NodeToRemove->PrevOpenOrdered != NULL)
	{
		NodeToRemove->PrevOpenOrdered->NextOpenOrdered = NodeToRemove->NextOpenOrdered;
		checkSlow(NodeToRemove->NextOpenOrdered != NodeToRemove);
	}
	else // it's the top of the stack, so pop it off
	{
		OpenList = NodeToRemove->NextOpenOrdered;
	}
	if(NodeToRemove->NextOpenOrdered != NULL)
	{
		NodeToRemove->NextOpenOrdered->PrevOpenOrdered = NodeToRemove->PrevOpenOrdered;
		NodeToRemove->NextOpenOrdered = NULL;
	}						

	NodeToRemove->PrevOpenOrdered = NULL;
}


/** 
 * will loop through all constraints in the constraint list and let them have their say on the current cost of the edge being considered
 * @param Edge - the edge being considered
 * @param PredecessorEdge - the edge we are traversing from to reach 'Edge'
 * @param SrcPoly - the poly we're coming from 
 * @param DestPoly - the poly we're going to!
 * @param EdgeCost - output param with current edge cost, and should be updated cost after this function is called
 * @param HeuristicCost - output param with current heuristiccost, and should be updated heuristic cost (h) after this function is called
 * @param EdgePoint - the point on the edge being used for cost calculations ( a good place to do heuristic weighting )
 * @return TRUE if this edge is fit at all, FALSE if it should be skipped
 */
UBOOL UNavigationHandle::ApplyConstraints( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, INT& EdgeCost, INT& HeuristicCost, const FVector& EdgePoint )
{
	SCOPE_CYCLE_COUNTER_SLOW(STAT_ApplyConstraintsTime);

#if DO_PATHCONSTRAINT_PROFILING
	INT Idx=0;
	SCOPETIMER(HandleCallCount,HandleCallAvg,HandleCallMax,HandleCallTotal,OVERALL)
#endif
	UNavMeshPathConstraint* Constraint = PathConstraintList;

#ifdef _DEBUG
	INT OriginalEdgeCost = EdgeCost;
#endif

#if !FINAL_RELEASE
	INT OldEdgeCost = 0;
	INT OldHeuristicCost = 0;
#endif
	while( Constraint != NULL )
	{
#if DO_PATHCONSTRAINT_PROFILING
		FName ClassName = Constraint->GetClass()->GetFName();
		if(HandleConstraintProfileData.Num() <= Idx)
		{
			HandleConstraintProfileData.AddItem(FConstraintProfileDatum(ClassName));
		}
		HandleProfileDatum = &HandleConstraintProfileData(Idx++);
		SCOPETIMER(HandleProfileDatum->CallCount,HandleProfileDatum->AvgTime,HandleProfileDatum->MaxTime,HandleProfileDatum->TotalTime,PERCONSTRAINT)
#endif

#if !FINAL_RELEASE
		if( bDebugConstraintsAndGoalEvals )
		{
			OldEdgeCost = EdgeCost;
			OldHeuristicCost = HeuristicCost;				
		}
#endif 
		if( !Constraint->EvaluatePath( Edge, PredecessorEdge, SrcPoly, DestPoly, CachedPathParams, EdgeCost, HeuristicCost, EdgePoint ) )
		{
			VERBOSE_LOG_EDGE_TRAVERSAL_FAIL(Edge,*FString::Printf(TEXT("Path constraint %s EvaluatePath rejected this edge!"),*Constraint->GetName()),this)

#if !FINAL_RELEASE
			if(bDebugConstraintsAndGoalEvals)
			{
				Constraint->NumNodesProcessed++;
				Constraint->NumThrownOutNodes++;
			}
#endif
			return FALSE;
		}
#if 0 && !FINAL_RELEASE
		VERBOSE_LOG_EDGE_CONSTRAINTCOST(Edge, this, *FString::Printf(TEXT("%s applied EdgeCost %d (from %d to %d) Heuristic Cost %d (from %d to %d)"), *Constraint->GetName(), EdgeCost - OldEdgeCost, OldEdgeCost, EdgeCost, HeuristicCost - OldHeuristicCost, OldHeuristicCost, HeuristicCost ))
#endif

#if !FINAL_RELEASE
		if(bDebugConstraintsAndGoalEvals)
		{
			Constraint->NumNodesProcessed++;
			Constraint->AddedDirectCost = EdgeCost - OldEdgeCost;
			Constraint->AddedHeuristicCost = HeuristicCost - OldHeuristicCost;
		}
#endif

		Constraint = Constraint->NextConstraint;
	}
#ifdef _DEBUG
	checkSlow(EdgeCost >= OriginalEdgeCost);
#endif

	return TRUE;
}
/**
 * called when a path is searched for and not found
 * sets the lastpatherror and saves off the time of the failure
 * @param ErrorType - the type of path error that just occurred 
 */
void UNavigationHandle::SetPathError(EPathFindingError ErrorType)
{
	LastPathError=ErrorType;
	if( GWorld != NULL )
	{
		LastPathFailTime = GWorld->GetTimeSeconds();
	}
}

void UNavigationHandle::DrawPathCache(FVector DrawOffset, UBOOL bPersistent, FColor DrawColor)
{
	if( !FindPylon() )
		return;
	UNavigationMeshBase* NavMesh = AnchorPylon->GetNavMesh();
	if( NavMesh == NULL )
		return;

	if (DrawColor.DWColor() == 0)
	{
		DrawColor = FColor(0,0,255);
	}

	ULineBatchComponent* LineBatcher = (bPersistent) ? GWorld->PersistentLineBatcher : GWorld->LineBatcher;
	if(CurrentEdge) CurrentEdge->DrawEdge( LineBatcher, FColor(0,255,0),DrawOffset+FVector(0,0,16));
	for( INT Idx = 0; Idx < PathCache.Num(); Idx++ )
	{
		FNavMeshEdgeBase* Edge = PathCache(Idx);
		FNavMeshPolyBase* Poly0 = Edge->GetPoly0();
		FNavMeshPolyBase* Poly1 = Edge->GetPoly1();
		if(Poly0) Poly0->DrawPoly( LineBatcher, DrawColor, DrawOffset );
		if(Poly1) Poly1->DrawPoly( LineBatcher, DrawColor, DrawOffset );
		if(Edge) Edge->DrawEdge( LineBatcher, Edge->GetEdgeColor(), DrawOffset + FVector(0,0,15) );
	}
}

void UNavigationHandle::PrintPathCacheDebugText()
{
	for( INT Idx = 0; Idx < PathCache.Num(); Idx++ )
	{
		FNavMeshEdgeBase* Edge = PathCache(Idx);
		NAVHANDLE_DEBUG_LOG(*Edge->GetDebugText());
	}
	NAVHANDLE_DEBUG_LOG(*FString::Printf(TEXT("--- %i edges total ---"),PathCache.Num()));
}


FString UNavigationHandle::GetCurrentEdgeDebugText()
{
	if( CurrentEdge == NULL )
	{
		return FString(TEXT("No Current Edge"));
	}

	return CurrentEdge->GetDebugText();
}

void UNavigationHandle::ClearCurrentEdge()
{
	CurrentEdge = NULL;
}


BYTE UNavigationHandle::GetCurrentEdgeType()
{
	if ( CurrentEdge != NULL )
	{
		return CurrentEdge->GetEdgeType();
	}
	return 0;
}


// useful for 'flattening' vectors according to the up vector passed (e.g. making vectors 2D about arbitrary up normals)
void FlattenVectAlongPassedAxis(FVector& Vect, const FVector& Up)
{
	Vect -= (Vect | Up) * Up;
}

FLOAT GetFlattenedDistanceBetweenVects(const FVector& A, const FVector& B, const FVector& Up)
{
	FVector Delta = (A-B);
	FlattenVectAlongPassedAxis(Delta,Up);
	return Delta.Size();
}

FLOAT GetFlattenedDistanceBetweenVectsSq(const FVector& A, const FVector& B, const FVector& Up)
{
	FVector Delta = (A-B);
	FlattenVectAlongPassedAxis(Delta,Up);
	return Delta.SizeSquared();
}

/**
 * This function will generate a move point which will get the bot into the next polygon
 * by compensating for early-arrival if needed
 * @param PathIdx - the index of the pathedge position we're trying to resolve (usually 0)
 * @param out_movePosition - the position we determined to be best to move to (should be set with the current desired location to move to)
 * @param NextMovePoitn    - the next move point in the path
 * @param HandleExtent     - the extent of the handle we're resolving for
 * @param CurHandlePos     - the current world position of the handle we're resolving for
 * @param ArrivalDistance  - how close to a point we have ot be before moveto() will return
 */
#define CLOSE_THRESHOLD ArrivalDistance*0.22f
#define CLOSE_THRESHOLD_SQ CLOSE_THRESHOLD * CLOSE_THRESHOLD
void UNavigationHandle::CompensateForEarlyArrivals(INT PathIdx,
												   FVector& out_MovePosition, 
												   const FVector& NextMovePoint, 
												   const FVector& InCurHandlePos, 
												   FLOAT ArrivalDistance)
{
	FVector CurHandlePos = InCurHandlePos;

	// nudge toward the next move point a little bit so that we actually get into the new poly
	FLOAT BumpThresh = 1.5f*ArrivalDistance;
	FNavMeshEdgeBase* CurEdge = PathCache(PathIdx);
	
	FVector EdgePerp = CurEdge->GetEdgePerpDir();
	const FVector VertLoc0 = CurEdge->GetVertLocation(0);
	const FVector VertLoc1 = CurEdge->GetVertLocation(1);
	
	FVector Norm(0.f,0.f,1.f);
	FVector Ground_movePos = out_MovePosition;
	FNavMeshPolyBase* OtherPoly = NULL;
	if( AnchorPoly != NULL )
	{
		Norm=AnchorPoly->GetPolyNormal();
		AnchorPoly->AdjustPositionToDesiredHeightAbovePoly(CurHandlePos,0.f);
		AnchorPoly->AdjustPositionToDesiredHeightAbovePoly(Ground_movePos,0.f);
		OtherPoly = CurEdge->GetOtherPoly(AnchorPoly);

	}

	// remove 'up' component from the delt to get 2D values relative to this edge's polys
	FLOAT Dist2dSq = GetFlattenedDistanceBetweenVectsSq(CurHandlePos,Ground_movePos,Norm);
	// check to see if we're right on top of the position on the edge we're trying to move to.. if so handle that specially because compensating may just 
	// cause point circling
	if (Dist2dSq < Square(BumpThresh))
	{
		// remove 'up' component of the delta
		FLOAT Dist = GetFlattenedDistanceBetweenVects(Ground_movePos,CurHandlePos,Norm);


		// assume the bot is going to stop when within BumpThresh of the goal, so we compensate by BumpThresh to get us within Dist
		FLOAT BumpAmt = BumpThresh+Dist+CLOSE_THRESHOLD;

		FVector NewPos(0.f);
		FVector OhterNorm = ( OtherPoly != NULL ) ? OtherPoly->GetPolyNormal() : FVector(0.f,0.f,1.f);
		FLOAT DistToNextSq = GetFlattenedDistanceBetweenVectsSq(NextMovePoint,CurHandlePos,OhterNorm);

		// determine the direction we're going to offset in
		FVector Dir(0.f);

		// whether we were able to determine a valid direction
		UBOOL bGotValidDirection=FALSE;

		// if we're right on top of the edge, or it's clear to the next move point just move on toward the next edge
		if(DistToNextSq > CLOSE_THRESHOLD_SQ && PointReachable(NextMovePoint))
		{
			Dir = (NextMovePoint-CurHandlePos);
			bGotValidDirection=TRUE;			
		}
		// if we couldn't move to the next edge position, and we're far enough away from the current edge position that the direction will be valid
		else if(Dist > CLOSE_THRESHOLD) 
		{
			// extend the move point by bump amount in the direction we're moving
			Dir = (Ground_movePos - CurHandlePos);
			bGotValidDirection=TRUE;
		}

		if(bGotValidDirection)
		{
			// we got a valid direction of movement, so offset the destination in that direction!
			Dir = Dir.SafeNormal();
			NewPos = out_MovePosition + Dir * BumpAmt;
		}

		FVector EntryPt(0.f),ExitPt(0.f),Closest(0.f);

		
		// ensure AI is in front of the edge before we do this next bit
		FVector EdgeDelta = (VertLoc1-VertLoc0);
		FLOAT Vert1Dist = EdgeDelta.Size();
		
		UBOOL bInFrontOfEdge = FALSE;
		if( Vert1Dist > KINDA_SMALL_NUMBER )
		{
			EdgeDelta/=Vert1Dist;
			
			FLOAT ProjectedDisp = (VertLoc1 - CurHandlePos) | EdgeDelta;
			if( ProjectedDisp > 0.f && ProjectedDisp < Vert1Dist )
			{
				bInFrontOfEdge=TRUE;
			}
		}

		
		// if the new direction was not valid at all, or it goes outside the path, move toward perp of the current edge
		if(	OtherPoly !=NULL &&
			(!bGotValidDirection || !OtherPoly->IntersectsPoly2D(Ground_movePos,NewPos,EntryPt,ExitPt,WORLD_SPACE)))
		{

			FNavMeshPolyBase* PolyToMoveToward = (bInFrontOfEdge) ? OtherPoly : AnchorPoly;

			if( PolyToMoveToward != NULL )
			{
				//GWorld->GetWorldInfo()->DrawDebugLine(out_MovePosition,(OtherPoly!=NULL)?OtherPoly->GetPolyCenter():*FinalDestination,255,0,255,TRUE);

				// if the direction we were moving took us out of the path, move instead perpindicular to the direction of the edge we're moving through
				FVector Delta = ((PolyToMoveToward!=NULL)?PolyToMoveToward->GetPolyCenter():*FinalDestination) - VertLoc0;
				FVector DirToNextMoveTarget = Delta.SafeNormal();
				if(((-EdgePerp) | DirToNextMoveTarget) > (EdgePerp | DirToNextMoveTarget))
				{
					EdgePerp *= -1.0f;
				}

				NewPos = out_MovePosition + EdgePerp * BumpAmt;
				checkSlow(!NewPos.ContainsNaN());
			}
		}
	
		checkSlow(!out_MovePosition.ContainsNaN());
		out_MovePosition=NewPos;
	}
	else // then we were not really close to the next move point, so move to a compensated (bumped) destination as usual
	{
		// if moving to the next point is going to pass through the current point, just flat out skip ahead and avoid unnecessary stops
		FVector ClosestPoint;
		if (PointDistToSegment(out_MovePosition, InCurHandlePos, NextMovePoint, ClosestPoint) < CachedPathParams.SearchExtent.X)
		{
			out_MovePosition = NextMovePoint;
		}
		else
		{
			FLOAT BumpAmt = BumpThresh+2.5f;
			FVector NewPos = out_MovePosition + (Ground_movePos - CurHandlePos).SafeNormal() * BumpAmt;
			if (OtherPoly != NULL && CurEdge != NULL && !OtherPoly->ContainsPoint(NewPos) && AnchorPoly != NULL && !AnchorPoly->ContainsPoint(NewPos))
			{
				NewPos = out_MovePosition + (NextMovePoint - CurHandlePos).SafeNormal() * BumpAmt;
				if (OtherPoly != NULL && CurEdge != NULL && !OtherPoly->ContainsPoint(NewPos) && AnchorPoly != NULL && !AnchorPoly->ContainsPoint(NewPos))
				{
					NewPos = out_MovePosition + (CurEdge->GetEdgeCenter() - CurHandlePos).SafeNormal() * BumpAmt;
					/*
					if (OtherPoly != NULL && CurEdge != NULL && !OtherPoly->ContainsPoint(NewPos) && AnchorPoly != NULL && !AnchorPoly->ContainsPoint(NewPos))
					{
						out_MovePosition = CurEdge->GetEdgeCenter() + (Ground_movePos - CurHandlePos).SafeNormal() * BumpAmt;

						FLOAT DistToNewPoint2dSq = GetFlattenedDistanceBetweenVectsSq(CurHandlePos,out_MovePosition,Norm);
						if( DistToNewPoint2dSq < BumpThresh*BumpThresh )
						{
							out_MovePosition = Ground_movePos;
						}
					}*/
				}
			}
			out_MovePosition = (GetFlattenedDistanceBetweenVectsSq(CurHandlePos, NewPos, Norm) < Square(BumpThresh)) ? NextMovePoint : NewPos;
			checkSlow(!out_MovePosition.ContainsNaN());
		}
	}


	// finally, see if the move destination we have chosen is very close to the edge even after bump offsets.. if so move it away from the edge a bit so we don't thrash along the line of the edge
	if( BumpThresh > 0.f )
	{
		FVector Closest(0.f);
		PointDistToLine(out_MovePosition,VertLoc0-VertLoc1,VertLoc0,Closest);
		FVector DeltaFromEdge = (out_MovePosition-Closest);
		FlattenVectAlongPassedAxis(DeltaFromEdge,CurEdge->GetEdgeNormal());
		FLOAT NewMovePointDistToRay = DeltaFromEdge.Size();
		if( NewMovePointDistToRay < 2.0f*CLOSE_THRESHOLD )
		{ // we are trying to move very close to parallel to the edge, push the destination point away from the edge so we don't oscillate here forever
			// move it out so it's far enough away from the edge
			out_MovePosition += DeltaFromEdge.SafeNormal() * ((2.1f*CLOSE_THRESHOLD)-NewMovePointDistToRay);
		}		
	}
}


/**
 * will operate on the pathcache to generate a set of points to run through.. this is based on the edges in the path
 * and a 'stringpull' method applied to the edges to find the best route through the edges
 * @param Interface - the interface of the entity we're computing for
 * @param PathIdx   - the index of the edge we are computing
 * @param out_EdgePos - the output of the computation
 * @param ArrivalDist - the radius around points which the entity will consider itself arrived (used to offset points to make sure the bot moves far enough forward)
 * @param bConstrainedToCurrentEdge - skip compensating if the edge requires special movement, rely solely upon GetEdgeDestination()
 * @param out_EdgePoints - optional pointer to an array to fill with all the computed edge movement points
 */
void UNavigationHandle::ComputeOptimalEdgePosition(INT PathIdx,
												   FVector& out_EdgePos,
												   FLOAT ArrivalDistance,
												   UBOOL bConstrainedToCurrentEdge,
												   TArray<FVector>* out_EdgePoints)
{	
	TArray<FVector> MovePoints;

	FVector CurHandlePos = CachedPathParams.SearchStart;
	FVector HandleExtent = CachedPathParams.SearchExtent;

	FLOAT EffectiveRadius = HandleExtent.X;

	MovePoints.AddZeroed(PathCache.Num()+1);
	FVector LastPt = *FinalDestination;

	// set inflection point as start point,
	// walk forward finding closest point on next edge to inflection point until inflection point is on end of edge segment
	// (save off points at each step)
	// when inflection is on end of segment move inflection to current point, keep walking forward
	FVector CurrentInflectionPoint = CurHandlePos;
	INT CurrentInflectionIdx = -1;
	FVector LastPoint = CurrentInflectionPoint;
	FVector ZAdjust(0.f);
	FVector InnerEdge0(0.f);
	FVector InnerEdge1(0.f);
	
	for(INT Idx=0;Idx<=PathCache.Num();++Idx)
	{
		FNavMeshEdgeBase* CurEdge = NULL;
		if( Idx < PathCache.Num() )
		{
			CurEdge = PathCache(Idx);
			MovePoints(Idx) = CurEdge->GetEdgeDestination(CachedPathParams, EffectiveRadius, CurrentInflectionPoint, CurrentInflectionPoint, this, TRUE);
		}
		else
		{
			MovePoints(Idx) = *FinalDestination;
		}

		const FVector MovePt = MovePoints(Idx);

		for(INT InflectionCheckIdx=CurrentInflectionIdx+1;InflectionCheckIdx <= Min<INT>(Idx,PathCache.Num()-1);++InflectionCheckIdx)
		{
			FNavMeshEdgeBase* InnerEdge = PathCache(InflectionCheckIdx);
			FVector OtherClosestPoint(0.f),ClosestPt(0.f);

			//ZAdjust = CachedPathParams.Interface->GetEdgeZAdjust(InnerEdge);
			InnerEdge0 = InnerEdge->GetVertLocation(0)+ZAdjust;
			InnerEdge1 = InnerEdge->GetVertLocation(1)+ZAdjust;


			const FVector DeltaToMovePt = MovePt-CurrentInflectionPoint;
			// if we're right on top of the next point don't treat it as an inflection point
			if (DeltaToMovePt.SizeSquared2D() < EffectiveRadius * EffectiveRadius)
			{
				continue;
			}

			// if trajectory is close to parallel to edge, use closest point on edge to next point (since all points are equally close, pick on in the direction we're going)
			const FLOAT DotP = Abs<FLOAT>(DeltaToMovePt.SafeNormal() | (InnerEdge0-InnerEdge1).SafeNormal());
			static FLOAT DotPThresh = 0.98f;
			if ( DotP > DotPThresh )
			{
				PointDistToSegment(MovePt,InnerEdge0,InnerEdge1,ClosestPt);
			}
			else
			{
				SegmentDistToSegmentSafe(InnerEdge0,InnerEdge1,MovePt,CurrentInflectionPoint,ClosestPt,OtherClosestPoint);
			}
			if( (ClosestPt - MovePt).SizeSquared() > EffectiveRadius*EffectiveRadius && ( ClosestPt.Equals(InnerEdge0,0.01f) || ClosestPt.Equals(InnerEdge1,0.01f) ) )
			{
				
#if DEBUG_DRAW_NAVMESH_PATH
				GWorld->GetWorldInfo()->DrawDebugLine(MovePoints(InflectionCheckIdx),MovePoints(InflectionCheckIdx)+FVector(0.f,0.f,100.f),255,255,128,TRUE);
				GWorld->GetWorldInfo()->DrawDebugLine(MovePoints(InflectionCheckIdx)+FVector(0.f,0.f,10.f),CurrentInflectionPoint+FVector(0.f,0.f,10.f),255,255,128,TRUE);
#endif
				FVector NewInflectionPt = InnerEdge->GetEdgeDestination(CachedPathParams,EffectiveRadius,ClosestPt,ClosestPt,this,TRUE);
				
				MovePoints(InflectionCheckIdx)=NewInflectionPt; 
				// go from last inflection point up to this point and set positions along the line
				for(INT InnerIdx=CurrentInflectionIdx+1;InnerIdx<InflectionCheckIdx;++InnerIdx)
				{
					FNavMeshEdgeBase* CurrentInnerEdge = PathCache(InnerIdx);

					//ZAdjust = CachedPathParams.Interface->GetEdgeZAdjust(CurrentInnerEdge);
					InnerEdge0 = CurrentInnerEdge->GetVertLocation(0)+ZAdjust;
					InnerEdge1 = CurrentInnerEdge->GetVertLocation(1)+ZAdjust;

					FVector NotUsed(0.f);
					SegmentDistToSegmentSafe(InnerEdge0,InnerEdge1,NewInflectionPt,CurrentInflectionPoint,ClosestPt,NotUsed);
					MovePoints(InnerIdx) = ClosestPt;
				}

				CurrentInflectionPoint = NewInflectionPt;
				CurrentInflectionIdx = InflectionCheckIdx;
				Idx=CurrentInflectionIdx;

				break;
			}			
		}

#if !DEBUG_DRAW_NAVMESH_PATH
		// if we're not drawing the complete path we only need to find the first inflection point after our current position and bail
		// this saves extra calls to getedgedestination
		if( CurrentInflectionIdx > PathIdx )
		{
			break;
		}
#endif
	}

	const FVector FinalDestPt = MovePoints.Last();
	MovePoints.RemoveSwap(MovePoints.Num()-1);

	// need to pull in points between the last inflection and the final dest now
	for(INT Idx=CurrentInflectionIdx+1;Idx < PathCache.Num();++Idx)
	{
		FNavMeshEdgeBase* CurrentInnerEdge = PathCache(Idx);

		//ZAdjust = CachedPathParams.Interface->GetEdgeZAdjust(CurrentInnerEdge);
		InnerEdge0 = CurrentInnerEdge->GetVertLocation(0)+ZAdjust;
		InnerEdge1 = CurrentInnerEdge->GetVertLocation(1)+ZAdjust;

		FVector NotUsed(0.f), ClosestPt(0.f);
		SegmentDistToSegmentSafe(InnerEdge0,InnerEdge1,CurrentInflectionPoint,FinalDestPt,ClosestPt,NotUsed);
		MovePoints(Idx) = ClosestPt;

	}

	
	// adjust for corners to first point
	
	if( PathIdx == 0 && PathCache.Num() > 0 )
	{
		FNavMeshEdgeBase* CurrentInnerEdge = PathCache(0);
		FVector NewEdgeDest = CurrentInnerEdge->GetEdgeDestination(CachedPathParams,EffectiveRadius,MovePoints(0),CurHandlePos,this);
		MovePoints(0) = NewEdgeDest;
	}
		

#if DEBUG_DRAW_NAVMESH_PATH

	for(INT Idx=0;Idx<PathCache.Num();++Idx)
	{
		if(Idx > 0)
		{
			GWorld->GetWorldInfo()->DrawDebugLine(MovePoints(Idx-1),MovePoints(Idx),255,0,255,TRUE);
		}
		else
		{
			GWorld->GetWorldInfo()->DrawDebugLine(MovePoints(Idx),CurHandlePos,255,0,255,TRUE);
		}
	}
#endif

		

#if DEBUG_DRAW_NAVMESH_PATH
	GWorld->GetWorldInfo()->DrawDebugLine(MovePoints(MovePoints.Num()-1),*FinalDestination,255,0,0,TRUE);
#endif

	if(out_EdgePoints != NULL)
	{
		*out_EdgePoints = MovePoints;
	}

	out_EdgePos = MovePoints(PathIdx);
	checkSlow(!out_EdgePos.ContainsNaN());

	// skip compensating if the edge requires special movement, rely solely upon GetEdgeDestination() since CompensateForEarlyArrivals may attempt to skip past the current edge
	if (!bConstrainedToCurrentEdge)
	{
		CompensateForEarlyArrivals(PathIdx,out_EdgePos,(PathIdx+1<MovePoints.Num())?MovePoints(PathIdx+1):*FinalDestination,CurHandlePos,ArrivalDistance);
	}
	checkSlow(!out_EdgePos.ContainsNaN());

}

void Vect2BP( FBasedPosition& BP, FVector Pos, AActor* ForcedBase, UObject* OwningObject )
{
	if(OwningObject->IsA(AActor::StaticClass()))
	{
		((AActor*)OwningObject)->Vect2BP(BP,Pos,ForcedBase);
	}
	else
	{
		BP.Set(ForcedBase,Pos);
	}
}

UBOOL UNavigationHandle::SetFinalDestination( FVector FinalDest )
{
	if(bSkipRouteCacheUpdates)
	{
		return FALSE;
	}

	checkSlow(!FinalDest.ContainsNaN());

 	UObject* InterfaceObject = GetOuter(); // MT-TODO->take this as a parameter
	Vect2BP(FinalDestination,FinalDest,NULL,InterfaceObject);

	// since we're now moving to a new final dest clear out transient vars related to pathing
	AnchorPoly=NULL;
	SubGoal_DestPoly=NULL;

	return TRUE;
}

#define DRAW_FINAL_DEST_DEBUG 0
UBOOL UNavigationHandle::ComputeValidFinalDestination(FVector& out_ComputedPosition)
{

	if(!PopulatePathfindingParamCache())
	{
		return FALSE;
	}

	FVector SearchExtent = CachedPathParams.SearchExtent;
	FVector SearchStart =  CachedPathParams.SearchStart;
	FVector Position = out_ComputedPosition;

	static TArray<APylon*> PylonsToCheck;
	PylonsToCheck.Reset();
	UNavigationHandle::GetAllPylonsFromPos(Position,SearchExtent,PylonsToCheck,FALSE);

	FCheckResult Hit(1.0f);
	FNavMeshPolyBase* HitPoly = NULL;
	if(UNavigationHandle::StaticObstaclePointCheck(Hit,Position,SearchExtent,&HitPoly,&PylonsToCheck))
	{
		out_ComputedPosition = Position;
		return TRUE;
	}

	UBOOL bColliding = TRUE;
	INT Tries=0;
	static INT MAX_TRIES = 5;
	const FVector Buffer = FVector(1.5f);
	while(bColliding && Tries++ < MAX_TRIES && HitPoly != NULL)
	{
#if DRAW_FINAL_DEST_DEBUG
		GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(Hit.Location,Hit.Normal.Rotation(),25.f,TRUE);
#endif
		
		static TArray<FNavMeshPolyBase*> ObstaclePolys;
		ObstaclePolys.Reset();

		FVector PolyNorm = HitPoly->GetPolyNormal();

		// if the position is behind the hit poly, try and find a poly that we're colliding with that we're not behind to adjust
		// from
		if( HitPoly->GetPolyPlane().PlaneDot(Position) < 0.f )
		{
			UNavigationHandle::GetAllObstaclePolysFromPos(Position,SearchExtent,ObstaclePolys,&PylonsToCheck);

			for(INT Idx=0;Idx<ObstaclePolys.Num();++Idx)
			{
				FNavMeshPolyBase* ObstaclePoly = ObstaclePolys(Idx);
				
				if( ObstaclePoly != HitPoly && ObstaclePoly->GetPolyPlane().PlaneDot(Position) > 0.f )
				{
					PolyNorm = ObstaclePoly->GetPolyNormal();
					HitPoly = ObstaclePoly;
					break;
				}
			}
		}


		// we hit something, try jumping to the hitlocation from the point check ( should be the pulled back location )
		FLOAT OffsetDist = FBoxPushOut(PolyNorm,SearchExtent+Buffer);

		OffsetDist = Max<FLOAT>(OffsetDist - FPointPlaneDist(out_ComputedPosition,HitPoly->GetPolyCenter(),PolyNorm),0.f);

		Position += PolyNorm*OffsetDist;
		checkSlow(!Position.ContainsNaN());

		bColliding = !UNavigationHandle::StaticObstaclePointCheck(Hit,Position,SearchExtent,&HitPoly);

#if DRAW_FINAL_DEST_DEBUG
		if(bColliding)
		{
			HitPoly->DrawPoly(GWorld->PersistentLineBatcher,FColor(255,0,0));
			GWorld->GetWorldInfo()->DrawDebugLine(Hit.Location,HitPoly->GetPolyCenter(),255,0,0,TRUE);
		}
#endif
	}

	if( !bColliding )
	{
		out_ComputedPosition = Position;
	}

	if(bColliding)
	{
#if DRAW_FINAL_DEST_DEBUG
		GWorld->GetWorldInfo()->DrawDebugLine(Position,Position+FVector(0,0,300),255,255,0,TRUE);
		GWorld->GetWorldInfo()->DrawDebugBox(Position,SearchExtent,255,255,0,TRUE);
#endif

		SetPathError(PATHERROR_COMPUTEVALIDFINALDEST_FAIL);
	}
	return !bColliding;
	
}

#define LOG_GETNEXTMOVELOC_FAIL(TXT) { SetPathError(PATHERROR_GETNEXTMOVELOCATION_FAIL);NAVHANDLE_DEBUG_LOG(*FString::Printf(TEXT("GetNextMoveLocation FAIL! - %s [%s]"),TEXT(TXT), *GetOuter()->GetName())) }
#define LOG_GETNEXTMOVELOC_FAIL_MAN(EXPR) { SetPathError(PATHERROR_GETNEXTMOVELOCATION_FAIL);NAVHANDLE_DEBUG_LOG(*FString::Printf(TEXT("GetNextMoveLocation FAIL! - %s [%s]"),(EXPR), *GetOuter()->GetName())) }
UBOOL UNavigationHandle::GetNextMoveLocation( FVector& out_Dest, FLOAT ArrivalDistance )
{
#define CAPTURE_GNM_NAV_MESH_PERF 0
#if CAPTURE_GNM_NAV_MESH_PERF
	static UBOOL bDoIt = FALSE;
	if( bDoIt == TRUE )
	{
		GCurrentTraceName = NAME_Game;
	}
	else
	{
		GCurrentTraceName = NAME_None;
	}

	appStartCPUTrace( NAME_Game, FALSE, TRUE, 40, NULL );
#endif

	out_Dest = FVector(0.f);

	if(!PopulatePathfindingParamCache())
	{
		LOG_GETNEXTMOVELOC_FAIL("Could not populate path params");
#if CAPTURE_GNM_NAV_MESH_PERF
		appStopCPUTrace( NAME_Game );
#endif
		return FALSE;
	}
#if DEBUG_DRAW_NAVMESH_PATH
	GWorld->GetWorldInfo()->FlushPersistentDebugLines();
#endif


	FVector Extent = CachedPathParams.SearchExtent;
	FVector SearchStart = CachedPathParams.SearchStart;

	checkSlow(!SearchStart.ContainsNaN());


	// ** figure out how far along the path we are, remove parts of the path we've completed
	//     (loop through pathcache until we find an edge which the searcher is in neither poly for)
	
	/** bool that indicates we've been in at least one poly so far */
	UBOOL bInAtLeastOnePoly = FALSE; 
	
	/** index of the first edge encountered who returned false from AllowMoveToNextEdge (-1 if none found)*/
	INT AllowMoveToNextEdgeIdx = -1;

	INT PathIdx = 0;
	FNavMeshEdgeBase* LastEdge = NULL;
	for (;PathIdx<PathCache.Num();++PathIdx)
	{
		FNavMeshEdgeBase* CurEdge = PathCache(PathIdx);

		FNavMeshPolyBase* Poly0 = CurEdge->GetPoly0();
		FNavMeshPolyBase* Poly1 = CurEdge->GetPoly1();

		// if either poly ref is NULL, something about the path has changed since we built it (e.g. a submesh was built).. so bail
		if(Poly0 == NULL || Poly1 == NULL)
		{
			LOG_GETNEXTMOVELOC_FAIL("Poly0 or Poly1 is NULL");
#if CAPTURE_GNM_NAV_MESH_PERF
			appStopCPUTrace( NAME_Game );
#endif
			return FALSE;
		}

		FBox Box = FBox::BuildAABB(SearchStart,Extent);
		UBOOL bInPoly0 = (Poly0 != NULL && Poly0->ContainsBox(Box,WORLD_SPACE,CachedPathParams.MaxHoverDistance));
		UBOOL bInPoly1 = (Poly1 != NULL && Poly1->ContainsBox(Box,WORLD_SPACE,CachedPathParams.MaxHoverDistance));
	
		// does this edge want us to stick on it until it's done with something?
		if (!CurEdge->AllowMoveToNextEdge(CachedPathParams,bInPoly0,bInPoly1))
		{
			AllowMoveToNextEdgeIdx = PathIdx;
		}

		UBOOL bReturnStatus = FALSE;
		if( CurEdge->OverrideGetNextMoveLocation(this,out_Dest,ArrivalDistance,bReturnStatus) )
		{
			// we have been overidden by the edge! 
			return bReturnStatus;
		}
		
		UBOOL bThisEdgeLinksSamePolysAsLastEdge = FALSE;

		if( LastEdge != NULL && PathIdx > 1)
		{
			FNavMeshPolyBase* Last_Poly0 = LastEdge->GetPoly0();
			FNavMeshPolyBase* Last_Poly1 = LastEdge->GetPoly1();

			// if two edges link the same polys in a row, we need to stop on the first one so that we don't clip off a section of the path that we need to follow to make it around an obstacle
			bThisEdgeLinksSamePolysAsLastEdge = (Last_Poly0 == Poly0 && Last_Poly1 == Poly1) || (Last_Poly1 == Poly0 && Last_Poly0 == Poly1) ;
		}
		LastEdge = CurEdge;


		// if the searcher is not in either of this edge's polys then we know we've gone too far
		if (bThisEdgeLinksSamePolysAsLastEdge || (!bInPoly0 && !bInPoly1))
		{
			// only stop if we were in a previous poly's edges, but not this one
			if(bInAtLeastOnePoly)
			{
				break;
			}
		}
		else
		{
			// we're in one of the polys, so turn this on
			bInAtLeastOnePoly=TRUE;			

			AnchorPoly = bInPoly0 ? Poly0 : Poly1;
			SubGoal_DestPoly = bInPoly0 ? Poly1 : Poly0;

			// if this is the last edge, and we're on the same side as our final dest then force this edge to get clipped
			if (PathIdx >= PathCache.Num()-1 && AllowMoveToNextEdgeIdx == -1) 
			{
				FVector FinalLoc = *FinalDestination;

				UBOOL bGoalInPoly0 = Poly0->ContainsPoint(FinalLoc,WORLD_SPACE);
				UBOOL bGoalInPoly1 = Poly1->ContainsPoint(FinalLoc,WORLD_SPACE);

				// if the goal is in both polys (e.g. on a corner), use the one that is closest to the vector
				if(bGoalInPoly0 && bGoalInPoly1)
				{
					FLOAT SignedDistToOne = ( (FinalLoc - Poly0->GetClosestPointOnPoly(FinalLoc)) | Poly0->GetPolyNormal() );
					FLOAT SignedDistToTwo = ( (FinalLoc - Poly1->GetClosestPointOnPoly(FinalLoc)) | Poly1->GetPolyNormal() );
					
					bGoalInPoly0 = (SignedDistToOne >= 0.f && SignedDistToOne <= SignedDistToTwo);
					bGoalInPoly1 = (SignedDistToTwo >= 0.f && SignedDistToTwo <= SignedDistToOne);
				}

				if( ( bInPoly0 && bGoalInPoly0 )  ||  ( bInPoly1 && bGoalInPoly1 ) )
				{
					// if we're in the same poly as our goal, turn off inpoly so we get into handlenotonpath
					UBOOL bInBothPolys = (bInPoly0 && bInPoly1);
					if( !bInBothPolys )
					{
						bInAtLeastOnePoly=FALSE;
						PathCache_RemoveIndex(0,1);
					}
				}
				else if(!bGoalInPoly1 && !bGoalInPoly0) 
				// if this is the last edge and the goal is in neither poly 
				// we probably never had a complete path to goal, override final dest to a location we can get to
				{
					SetFinalDestination(SubGoal_DestPoly->GetPolyCenter()+FVector(0.f,0.f,CachedPathParams.SearchExtent.Z));
					checkSlow(!(*FinalDestination).ContainsNaN());

				}
			}
			
		}
	}

	// decrement the counter since the last loop iteration is the one which was too far forward in the path
	PathIdx-=1;

	// if we weren't in any of the polys along the path then something odd happened.. special handling follows
	if(!bInAtLeastOnePoly)
	{
		// if we are not anywhere in the path, assume we should be moving the the edge in the front position
		if( PathCache.Num() > 0  )
		{
			CurrentEdge = PathCache(0);
		}
		UBOOL bReturn = HandleNotOnPath(ArrivalDistance,out_Dest);

		if(!bReturn)
		{
			LOG_GETNEXTMOVELOC_FAIL("Was not on path, and HandleNotOnPath returned FALSE!");
		}

#if CAPTURE_GNM_NAV_MESH_PERF
		appStopCPUTrace( NAME_Game );
#endif
		return bReturn;
	}

	// clip off everything before PathIdx in the pathcache
	UBOOL bConstrainedToCurrentEdge = (AllowMoveToNextEdgeIdx!=-1 && PathIdx>AllowMoveToNextEdgeIdx);
	
	if ( bConstrainedToCurrentEdge )
	{
		PathCache_RemoveIndex(0,AllowMoveToNextEdgeIdx);
	}
	else
	if(PathIdx > 0)
	{
		PathCache_RemoveIndex(0,PathIdx);	
	}

	FNavMeshEdgeBase* Edge = (PathCache.Num() > 0) ? PathCache(0) : NULL;

	if (Edge != NULL) 
	{
		CurrentEdge = Edge;
		
		// Finds closest vert and move away from it along the edge by pawns radius
		ComputeOptimalEdgePosition(0,out_Dest,ArrivalDistance,bConstrainedToCurrentEdge);	
		checkSlow(!out_Dest.ContainsNaN());

#if DEBUG_DRAW_NAVMESH_PATH
		FNavMeshPolyBase* Poly0 = Edge->GetPoly0();
		FNavMeshPolyBase* Poly1 = Edge->GetPoly1();

		Poly0->DrawPoly(GWorld->PersistentLineBatcher, Poly0==AnchorPoly?FColor(255,0,0):FColor(0,255,0), FVector(0,0,25));
		Poly1->DrawPoly(GWorld->PersistentLineBatcher, Poly1==AnchorPoly?FColor(0,255,0):FColor(255,0,0), FVector(0,0,25));

		if(SubGoal_DestPoly!=NULL)
		{
			SubGoal_DestPoly->DrawPoly(GWorld->PersistentLineBatcher, FColor(255,255,0), FVector(0,0,26));
		}

		GWorld->GetWorldInfo()->DrawDebugBox( out_Dest, FVector(5.f), 0, 0, 255, TRUE );
#endif
#if CAPTURE_GNM_NAV_MESH_PERF
		appStopCPUTrace( NAME_Game );
#endif
		return TRUE;
	}
	else
	if( CurrentEdge != NULL )
	{
		// May have reached the final edge and still not be in the dest poly
		// so move directly to final dest 

		CurrentEdge = NULL;
#if DEBUG_DRAW_NAVMESH_PATH
			GWorld->GetWorldInfo()->DrawDebugBox( *FinalDestination, FVector(5,5,5), 255, 255, 255, TRUE );
#endif

		out_Dest = *FinalDestination;
		checkSlow(!out_Dest.ContainsNaN());
#if CAPTURE_GNM_NAV_MESH_PERF
		appStopCPUTrace( NAME_Game );
#endif
		return TRUE;
	}

#if DEBUG_DRAW_NAVMESH_PATH
	GWorld->GetWorldInfo()->DrawDebugBox( *FinalDestination, FVector(5,5,5), 255, 164, 177, TRUE );
	GWorld->GetWorldInfo()->DrawDebugBox( SearchStart, FVector(5,5,5), 255, 64, 77, TRUE );
	AnchorPoly->DrawPoly( GWorld->PersistentLineBatcher, FColor(255,164,177), FVector(0,0,15) );	
#endif

	//GWorld->GetWorldInfo()->DrawDebugLine(SearchStart,SearchStart+FVector(0.f,0.f,100.f),255,0,255,TRUE);
	LOG_GETNEXTMOVELOC_FAIL("Could not determine current edge?");
#if CAPTURE_GNM_NAV_MESH_PERF
	appStopCPUTrace( NAME_Game );
#endif
	return FALSE;
}

UBOOL CanReachFinalDest(const UNavigationHandle::FFitNessFuncParams& Params)
{
	// check reachability
	FVector FinalDestPt = *Params.AskingHandle->FinalDestination;

	INT Its=0;
	FCheckResult Hit(1.f);

	FVector CheckDir = (Params.Point - FinalDestPt).SafeNormal();

	while(Its++ < 5 && !UNavigationHandle::StaticObstacleLineCheck(Params.AskingHandle, Hit, FinalDestPt, Params.Point,Params.Extent,TRUE,NULL,Params.PylonsWeCareAbout) )
	{
		// filter backface hits
		if ( (CheckDir | Hit.Normal)  < KINDA_SMALL_NUMBER )
		{
			FinalDestPt = Hit.Location + CheckDir * FBoxPushOut(Hit.Normal,Params.Extent)*1.1f;
		}
		else
		{
			return FALSE;
		}
	}

	return TRUE;
}

UBOOL UNavigationHandle::HandleNotOnPath( FLOAT ArrivalDistance, FVector& out_Dest )
{
	FVector Extent = CachedPathParams.SearchExtent;
	FVector SearchStart = CachedPathParams.SearchStart;
	if(CurrentEdge != NULL && PathCache.Num() > 0)
	{
		// even if we're not in any of the polys along our path, if we're close to the current edge, keep on truckin
		FVector Closest(0.f);
		FLOAT DistFromEdge = PointDistToSegment(SearchStart,CurrentEdge->GetVertLocation(0),CurrentEdge->GetVertLocation(1),Closest );
		DistFromEdge = GetFlattenedDistanceBetweenVectsSq(SearchStart,Closest,CurrentEdge->GetEdgeNormal());
		checkSlow(!Closest.ContainsNaN());

		// if we're really close to the current edge, move toward it like normal
		if( DistFromEdge < Extent.X*Extent.X )
		{
			ComputeOptimalEdgePosition(0,out_Dest,ArrivalDistance);	

			LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath ComputeOptimalEdgePosition");
			return TRUE;
		}
		// if we're sort of close, try and get back into the path first
		else if( DistFromEdge < Square<FLOAT>(Extent.X*1.5f) )
		{
			// figure out which poly for this edge we should try and get into
			FVector BestLocPoly0(0.f),BestLocPoly1(0.f);
			UBOOL bFoundPoly0Pos = (CurrentEdge->GetPoly0() && CurrentEdge->GetPoly0()->GetBestLocationForCyl(SearchStart,Extent.X,Extent.Z,BestLocPoly0));
			UBOOL bFoundPoly1Pos = (CurrentEdge->GetPoly1() && CurrentEdge->GetPoly1()->GetBestLocationForCyl(SearchStart,Extent.X,Extent.Z,BestLocPoly1));

			checkSlow(!BestLocPoly0.ContainsNaN());
			checkSlow(!BestLocPoly1.ContainsNaN());

			if(!bFoundPoly0Pos && !bFoundPoly1Pos)
			{
				LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath (!bFoundPoly0Pos && !bFoundPoly1Pos)");
				return FALSE;
			}
			else if(bFoundPoly0Pos&&bFoundPoly1Pos)
			{
				FVector Up = CurrentEdge->GetEdgeNormal();

				checkSlow(!Up.ContainsNaN());

				FLOAT Dist0 = GetFlattenedDistanceBetweenVects(SearchStart,BestLocPoly0,Up);
				FLOAT Dist1 = GetFlattenedDistanceBetweenVects(SearchStart,BestLocPoly1,Up);

				FVector NewPt = (Dist0<Dist1) ? BestLocPoly0 : BestLocPoly1;
				FVector Delta = NewPt-SearchStart;
				FVector Dir = Delta.SafeNormal();
				Delta += Dir * ArrivalDistance;
				out_Dest = SearchStart+Delta+CachedPathParams.Interface->GetEdgeZAdjust(CurrentEdge);
				checkSlow(!out_Dest.ContainsNaN());

				LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath (bFoundPoly0Pos&&bFoundPoly1Pos)");
				//GWorld->GetWorldInfo()->DrawDebugLine(SearchStart,out_Dest,0,128,255,TRUE);
				return TRUE;
			}
		}
		else
		{
			// NO LONGER IN THE POLY WE EXPECTED (AND NOT CLOSE), TRY TO RECOVER
			APylon*			  CurPylon = NULL;
			FNavMeshPolyBase* CurPoly  = NULL;
			// GET NEW ANCHOR POLY
#if 0 
			if( GetPylonAndPolyFromPos( CachedPathParams.SearchStart, CachedPathParams.MinWalkableZ, CurPylon, CurPoly ) )
			{
				FNavMeshPolyBase* Poly0 = CurrentEdge->GetPoly0();
				FNavMeshPolyBase* Poly1 = CurrentEdge->GetPoly1();

// 				CurPoly->DrawPoly( GWorld->PersistentLineBatcher, FColor( 255, 255, 255 ), FVector(0,0,20) );
// 				Poly0->DrawPoly( GWorld->PersistentLineBatcher, FColor( 255, 0, 0 ), FVector(0,0,20) );
// 				Poly1->DrawPoly( GWorld->PersistentLineBatcher, FColor( 128, 0, 0 ), FVector(0,0,20) );
// 				AnchorPoly->DrawPoly( GWorld->PersistentLineBatcher, FColor( 0, 255, 0 ), FVector(0,0,30) );

				FNavMeshPolyBase* TargPoly = (AnchorPoly == Poly0) ?  Poly1 : Poly0;
				FPathStore OldPathCache = PathCache;
				FBasedPosition OldFinalDest = FinalDestination;

				// GENERATE PATH FROM CURPOLY TO TARGPOLY
				AAIController* AI = Cast<AAIController>(GetOuter());
				if( AI != NULL )
				{
					UBOOL bHasPath = AI->eventGeneratePathToLocation( TargPoly->GetPolyCenter() + FVector(0,0,CachedPathParams.SearchExtent.Z), 0, FALSE );
					// IF PATH FOUND, STITCH TOGETHER OLD CACHE AND NEW CACHE
					if( bHasPath && PathCache.Num() > 0 )
					{
						INT EdgeCnt = 0;
						UBOOL bFoundTargetPoly = FALSE;
						FNavMeshEdgeBase* ClipEdge = NULL;
						for( INT Idx = 0; Idx < OldPathCache.Num(); Idx++ )
						{
							FNavMeshEdgeBase* Edge = OldPathCache(Idx);
							FNavMeshPolyBase* P0 = Edge->GetPoly0();
							FNavMeshPolyBase* P1 = Edge->GetPoly1();

							EdgeCnt++;
							if( P0 == TargPoly || P1 == TargPoly )
							{
								if( OldPathCache.Num() > Idx+1 )
								{
									ClipEdge = OldPathCache(Idx+1);
								}
								break;
							}
						}
						
						if(ClipEdge != NULL && ClipEdge->GetPoly0() != TargPoly && ClipEdge->GetPoly1() != TargPoly)
						{
							// hi mom
						}
						else
						{
							// CUT ALL EDGES IN OLD CACHE BEFORE MATCH
							PathCache_RemoveIndex( 0, EdgeCnt, &OldPathCache );
						}

						
						// APPEND OLD CACHE TO PATH CACHE
						FPathStore NewPathCache = PathCache;
						PathCache = OldPathCache;
						for( INT NewPathIdx = NewPathCache.Num() - 1; NewPathIdx >= 0; NewPathIdx-- )
						{
							FNavMeshEdgeBase* TestEdge = NewPathCache(NewPathIdx);
//							TestEdge->DrawEdge( GWorld->PersistentLineBatcher, FColor(255,255,255), FVector(0,0,50) );

							PathCache_InsertEdge( NewPathCache(NewPathIdx), 0 );
						}

						// UPDATE FINAL DESTINATION AND CURRENT EDGE
						FinalDestination = OldFinalDest;
						CurrentEdge = PathCache(0);

						// SET DESTINATION FOR CURRENT EDGE
						ComputeOptimalEdgePosition(0,out_Dest,ArrivalDistance,TRUE);	
						checkSlow(!out_Dest.ContainsNaN());

						// BE HAPPY! :)
						return TRUE;							
					}
				}
			}
#endif

			LOG_GETNEXTMOVELOC_FAIL_MAN(*FString::Printf(TEXT("%s GetNextMoveLocation fail! - HandleNotOnPath First Else return FALSE case  DistFromEdge: %f  Extent.X: %f  Extent.X*1.5: %f"), *GetOuter()->GetName(), DistFromEdge, Extent.X, Extent.X*1.5f ))
			return FALSE;
		}
	}
	else if(PathCache.Num() == 0) 
		// if we have no path cache, and currentEdge is null, check
		// to see if we're in the same poly as destination, but can't get straight to it because it or we
		// are too close to the wall
	{
		FVector NewWalkLoc = *FinalDestination;
		const FVector FinalDest = NewWalkLoc;
		checkSlow(!NewWalkLoc.ContainsNaN());

		if(PointReachable(NewWalkLoc))
		{
			// the final destination is reachable.. go straight to it
			out_Dest = NewWalkLoc;
		}
		else
		{
			AnchorPoly = GetAnchorPoly();
			APylon* Py=NULL;

			UBOOL bHasComputedValidTrajectory=FALSE;
			if(AnchorPoly == NULL)
			{
				LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath (AnchorPoly == NULL)");
				return FALSE;
			}
			else
			{
				FNavMeshPolyBase* GoalPoly=NULL;
				if(!GetPylonAndPolyFromPos(NewWalkLoc,CachedPathParams.MinWalkableZ,Py,GoalPoly) || GoalPoly != AnchorPoly)
				{
					// if getanchorpoly returned something different than goal poly, but we're considered inside goal poly
					// we are actually probably right on the boundary between goalpoly and anchorpoly, so just make anchor poly be GoalPoly

					if( GoalPoly != NULL && GoalPoly->ContainsPoint(CachedPathParams.SearchStart,WORLD_SPACE) )
					{
						AnchorPoly = GoalPoly;
					}
					else
					{
						LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath GetPylonAndPolyFromPos");
						return FALSE;
					}
				}

				// walk away from the edge of our current poly
				if(!AnchorPoly->GetBestLocationForCyl(SearchStart,Extent.X,Extent.Z,NewWalkLoc,TRUE)||
					!PointReachable(FinalDest,NewWalkLoc))
				{
					FVector Adjusted_Start = SearchStart;

					if( !ObstaclePointCheck(SearchStart,Extent))
					{
						if( !ComputeValidFinalDestination(Adjusted_Start))
						{
							LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath SearchStart is embedded, couldn't resolve non embedded pos");
							return FALSE;
						}
					}


					// try to find somewhere we nearby we can get to our final dest from
					static TArray<FVector> Points;
					Points.Reset();
					GetValidPositionsForBoxEx(Adjusted_Start,Extent.X*6,Extent,TRUE,Points,1,0.f,FVector(0.f),&CanReachFinalDest);
					if( Points.Num() > 0 )
					{
						bHasComputedValidTrajectory=TRUE;
						NewWalkLoc = Points(0);
					}
					else
					{
						LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath GetBestLocationForCyl");
						return FALSE;
					}
				}


				checkSlow(!NewWalkLoc.ContainsNaN());

			}

			
			if ( !bHasComputedValidTrajectory )
			{
				FVector TempLoc = *FinalDestination;
				FVector Adjusted_finalDest(0.f);
				checkSlow(!TempLoc.ContainsNaN());
				
				if(!AnchorPoly->GetBestLocationForCyl(TempLoc,Extent.X,Extent.Z,Adjusted_finalDest,TRUE))
				{
					LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath Second (finaldest) GetBestLocationForCyl");
					return FALSE;
				}

				if( PointReachable(Adjusted_finalDest) )
				{
					NewWalkLoc=Adjusted_finalDest;
				}

			}

			FVector Delta = NewWalkLoc-SearchStart;
			FVector DeltaDir = Delta.SafeNormal();

			Delta += DeltaDir * ArrivalDistance;

			out_Dest = SearchStart+Delta;
		}

		//GWorld->GetWorldInfo()->DrawDebugLine(SearchStart,out_Dest,255,255,0,TRUE);
		LOG_GETNEXTMOVELOC_FAIL("HandleNotOnPath returning TRUE");
		CurrentEdge=NULL;
		return TRUE;
	}

	// check to see if we are "in" a poly we're not very close to the surface of (e.g. we're floating above it)
	AnchorPoly = GetAnchorPoly();
	if(AnchorPoly != NULL && !AnchorPoly->ContainsBox(FBox::BuildAABB(SearchStart,Extent),WORLD_SPACE,CachedPathParams.MaxHoverDistance))
	{
		out_Dest = AnchorPoly->GetClosestPointOnPoly(SearchStart)+FVector(FBoxPushOut(AnchorPoly->GetPolyNormal(),Extent));
		return TRUE;
	}

	return FALSE;
}


UBOOL IsValidAnchorPos(const UNavigationHandle::FFitNessFuncParams& Params)
{
	// check reachability
	FVector StartCheckPos = Params.StartPt;

	INT Its=0;
	FCheckResult Hit(1.f);

	FVector CheckDir = (Params.Point - StartCheckPos).SafeNormal();

	while(Its++ < 5 && !UNavigationHandle::StaticObstacleLineCheck(Params.AskingHandle, Hit, StartCheckPos, Params.Point,Params.Extent,TRUE,NULL,Params.PylonsWeCareAbout) )
	{
		// filter backface hits
		if ( (CheckDir | Hit.Normal)  < KINDA_SMALL_NUMBER )
		{
			StartCheckPos = Hit.Location + CheckDir * FBoxPushOut(Hit.Normal,Params.Extent)*1.1f;
		}
		else
		{
			return FALSE;
		}
	}
	
	if ( Params.PolyContainingPoint != NULL )
	{
		return Params.PolyContainingPoint->IsEscapableBy(Params.AskingHandle->CachedPathParams);
	}
	return FALSE;
}
/** 
 * will get a point nearby which is in a poly that has edges outbound that support this AI
 * @param out_NewAnchorLoc -  
 * @param OverrideStartLoc - optional param to override the starting location for this query (if none is given this AI's searchstart will be used)
 * @return true on success
 */
UBOOL UNavigationHandle::GetValidatedAnchorPosition(FVector& out_NewAnchorLoc, FVector OverrideStartLoc/*=FVector()*/)
{
	if( !PopulatePathfindingParamCache()) 
	{
		return FALSE;
	}


	FVector StartLoc = CachedPathParams.SearchStart;
	if( !OverrideStartLoc.IsZero() )
	{
		StartLoc = OverrideStartLoc;
	}

	return StaticGetValidatedAnchorPosition(out_NewAnchorLoc,StartLoc,CachedPathParams.SearchExtent);

}

/** 
 * will get a point nearby which is in a poly that has edges outbound that support this AI
 * @param out_NewAnchorLoc -  
 * @param OverrideStartLoc - optional param to override the starting location for this query (if none is given this AI's searchstart will be used)
 * @return true on success
 */
UBOOL UNavigationHandle::StaticGetValidatedAnchorPosition(FVector& out_NewAnchorLoc, FVector StartBaseLoc, FVector Extent )
{
	static TArray<FVector> Points;
	Points.Reset();

	GetValidPositionsForBoxEx(StartBaseLoc,Extent.X*6.0f,Extent,FALSE,Points,1,0.f,Extent,&IsValidAnchorPos);

	if (Points.Num() > 0)
	{
		out_NewAnchorLoc = Points(0);
		return TRUE;
	}

	return FALSE;
}

UBOOL UNavigationHandle::HandleWallAdjust( FVector HitNormal, AActor* HitActor )
{
	return FALSE;
}

UBOOL UNavigationHandle::HandleFinishedAdjustMove()
{
	AController* C = Cast<AController>(GetOuter());
	if( C != NULL )
	{
		// End move action for controller after wall adjust
		// Give NavHandle a chance to stitch in a new path since you are unlikely to be in the poly associated with the current edge
		C->GetStateFrame()->LatentAction = 0;
		return TRUE;
	}
	return FALSE;
}

UBOOL UNavigationHandle::SuggestMovePreparation( FVector& out_MovePt, AController* C )
{
	UBOOL bResult = FALSE;
	if( CurrentEdge != NULL && C != NULL)
	{		
		bResult = CurrentEdge->PrepareMoveThru( C, out_MovePt );
		if( bResult )
		{
			PathCache_RemoveIndex( 0, 1 );
		}
	}
	
	return bResult;
}

/**
* This function is called by APawn::ReachedDestination and is used to coordinate when to stop moving to a point during
* path following (e.g. once we are inside the next poly, stop moving rather than trying to hit an exact point)
* @param Destination - destination we're trying to move to
* @param out_bReached - whether or not we have reached this destination
* @param HandleOuterActor - the actor which implements the navhandle interface for this test
* @return - returns TRUE If this function was able to determien if we've arrived (FALSE means keep checking elsewhere)
*/
UBOOL UNavigationHandle::ReachedDestination(const FVector& Destination, AActor* HandleOuterActor, FLOAT ArrivalThreshold, UBOOL& out_bReached)
{
	// if we're moving to our final destination 
	// do a reach threshold test
	if(Destination.Equals(*FinalDestination,0.1f) || SubGoal_DestPoly == NULL || AnchorPoly == NULL)
	{
		// indicate the caller is responsible for this determination (e.g. we failed to determine it here)
		return FALSE;
	}


	if(!PopulatePathfindingParamCache())
	{
		return FALSE;
	}
	FVector SearchStart = CachedPathParams.SearchStart;
	FVector Extent = CachedPathParams.SearchExtent;
	
	if( (SearchStart-Destination).SizeSquared2D() < ArrivalThreshold*ArrivalThreshold)
	{
		if(Abs<FLOAT>(SearchStart.Z - Destination.Z) < Extent.Z*2.1f)
		{
			out_bReached = TRUE;
		}
		else
		{
			// if all that's keeping us from arriving is height, check to see if there is something in the way, otherwise pass it
			out_bReached = PointReachable(Destination);
		}

	}
	else
	{
		UBOOL bInDestPoly = SubGoal_DestPoly->ContainsBox(FBox::BuildAABB(SearchStart,Extent),WORLD_SPACE,CachedPathParams.MaxHoverDistance);
		
		FCheckResult Hit(1.f);
		// if we're inside the destination poly, AND we're not colliding with the obstalce mesh, AND we're close to the surface of the poly, we're done
		out_bReached =  bInDestPoly && StaticObstaclePointCheck(Hit,SearchStart,Extent);

		if( out_bReached )
		{
			if ( CurrentEdge != NULL )
			{
				FNavMeshPolyBase* Poly0 = CurrentEdge->GetPoly0();
				FNavMeshPolyBase* Poly1 = CurrentEdge->GetPoly1();

				UBOOL bInPoly0 = Poly0 == SubGoal_DestPoly || Poly0 == AnchorPoly || Poly0->ContainsBox(FBox::BuildAABB(SearchStart,Extent),WORLD_SPACE,CachedPathParams.MaxHoverDistance);
				UBOOL bInPoly1 = Poly1 == SubGoal_DestPoly || Poly1 == AnchorPoly || Poly1->ContainsBox(FBox::BuildAABB(SearchStart,Extent),WORLD_SPACE,CachedPathParams.MaxHoverDistance);
				// if we're in both polys they probably overlap, don't return tRUE until we are only in one of them		
				if( bInPoly0 && bInPoly1 )
				{
					out_bReached=FALSE;
				}
			}
		}
	}
	return TRUE;
}

void UNavigationHandle::ClearCrossLevelRefs(ULevel* Level)
{
	if(AnchorPylon != NULL && AnchorPylon->IsInLevel(Level))
	{
		AnchorPylon = NULL;
	}

	if(AnchorPoly != NULL && AnchorPoly->NavMesh->GetPylon()->IsInLevel(Level))
	{
		AnchorPoly = NULL;
	}

	if(CurrentEdge != NULL && CurrentEdge->NavMesh->GetPylon()->IsInLevel(Level))
	{
		CurrentEdge=NULL;
	}

	if(SubGoal_DestPoly != NULL && SubGoal_DestPoly->NavMesh->GetPylon()->IsInLevel(Level))
	{
		SubGoal_DestPoly=NULL;
	}

	for(INT PathIdx=0;PathIdx<PathCache.Num();PathIdx++)
	{
		FNavMeshEdgeBase* CurEdge = PathCache(PathIdx);
		if(CurEdge->NavMesh->GetPylon()->IsInLevel(Level))
		{
			PathCache_Empty();
			break;
		}
	}

}

void UNavigationHandle::ClearAllMeshRefs()
{
	AnchorPylon = NULL;
	AnchorPoly = NULL;
	CurrentEdge = NULL;
	PathCache_Empty();
}

UBOOL UNavigationHandle::FindPylon()
{
	if(!PopulatePathfindingParamCache())
	{
		return FALSE;
	}
	AnchorPylon = GetPylonFromPos(CachedPathParams.SearchStart);
	return (AnchorPylon!=NULL);
}

/**
 * returns TRUE if the poly described by the passed list of vectors intersects a loaded portion of the mesh
 * @param Poly - vertexlist representing the poly we want to query against the mesh
 * @param out_Pylon - pylon we collided with
 * @param out_Poly - poly we collided with
 * @arapm ExclusionPolys - optional list of polys we want to exclude from the search
 * @param bIgnoreImportedMeshes - when TRUE meshes which unwalkable surfaces will be ignored for overlap testing
 * @param IgnorePylons - optional pylons to ignore collisions from
 * @return TRUE if poly intersects
 */
UBOOL UNavigationHandle::PolyIntersectsMesh( TArray<FVector>& InPoly,
												APylon*& out_Pylon,
												struct FNavMeshPolyBase*& out_Poly,
												TArray<FNavMeshPolyBase*>* ExclusionPolys/*=NULL*/,
												UBOOL bIgnoreImportedMeshes/*=FALSE*/,
												TArray<APylon*>* IgnorePylons/*=NULL*/,
												DWORD TraceFlags/*=0*/)
{
	FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

	if( Octree == NULL )
	{
		return FALSE;
	}

	FBox BoxBounds;
	BoxBounds.Init();

	for( INT Idx = 0; Idx < InPoly.Num(); Idx++ )
	{
		BoxBounds += InPoly(Idx);
		BoxBounds += InPoly(Idx) + AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_EntityHalfHeight * FVector(0.f,0.f,1.f); 
		BoxBounds += InPoly(Idx) + -15.f * FVector(0.f,0.f,1.0f); 

	}
	FBoxCenterAndExtent QueryBox(BoxBounds);

	FNavMeshPolyBase* Poly = NULL;
	// Iterate over the octree nodes containing the query point.
	for(FPylonOctreeType::TConstElementBoxIterator<> OctreeIt(*(Octree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
	{
		APylon* CurPylon = OctreeIt.GetCurrentElement();
		check(CurPylon);

		if (CurPylon->IsValid() && (IgnorePylons == NULL || !IgnorePylons->ContainsItem(CurPylon)))
		{
			FLOAT MinWalkZ = -1.f;
			if ( bIgnoreImportedMeshes && CurPylon->bImportedMesh )
			{
				MinWalkZ = AScout::GetGameSpecificDefaultScoutObject()->WalkableFloorZ;
			}

			if(CurPylon->NavMeshPtr->IntersectsPoly(InPoly,Poly,ExclusionPolys,WORLD_SPACE,MinWalkZ,TraceFlags))
			{
				out_Poly = Poly;
				out_Pylon = CurPylon;
				return TRUE;
			}
		}
	}

	out_Poly=NULL;
	out_Pylon=NULL;
	return FALSE;
}

UBOOL UNavigationHandle::BoxIntersectsMesh( const FVector& Center, const FVector& Extent, APylon*& out_Pylon, struct FNavMeshPolyBase*& out_Poly, DWORD TraceFlags/*=0*/)
{
	FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

	if( Octree == NULL )
	{
		return FALSE;
	}

	FBoxCenterAndExtent QueryBox(Center, Extent);

	FNavMeshPolyBase* Poly = NULL;
	// Iterate over the octree nodes containing the query point.
	for(FPylonOctreeType::TConstElementBoxIterator<> OctreeIt(*(Octree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
	{
		APylon* CurPylon = OctreeIt.GetCurrentElement();
		check(CurPylon);

		if(CurPylon->IsValid())
		{
			if(CurPylon->NavMeshPtr->IntersectsPolyBounds(Center,Extent,Poly,WORLD_SPACE,TraceFlags))
			{
				out_Poly = Poly;
				out_Pylon = CurPylon;
				return TRUE;
			}
		}
	}

	out_Poly=NULL;
	out_Pylon=NULL;
	return TRUE;
}

/**
 * Given a line segment, walks along the segment returning all polys that it crosses as entry and exit points of that segment
 * (will find spans from any navmesh in the world)
 * @param Start - Start point of span to check
 * @param End - end point of span 
 * @Param out_Spans - out array of spans found and the polys they link to
 */
void UNavigationHandle::GetPolySegmentSpanList( FVector& Start, FVector& End, TArray<FPolySegmentSpan>& out_Spans)
{

	FBox QueryBox(0);
	QueryBox += Start;
	QueryBox += End;


	TArray<APylon*> Pylons;

	GetAllPylonsFromPos(QueryBox.GetCenter(),QueryBox.GetExtent(),Pylons,FALSE);

	APylon* CurPylon = NULL;
	for(INT PylonIdx=0;PylonIdx<Pylons.Num();++PylonIdx)
	{
		CurPylon = Pylons(PylonIdx);
		if(CurPylon->GetNavMesh() != NULL)
		{
			CurPylon->GetNavMesh()->GetPolySegmentSpanList(Start,End,out_Spans,WORLD_SPACE);
		}
	}
}

UBOOL UNavigationHandle::GetPylonAndPolyFromActorPos(AActor* Actor, APylon*& out_Pylon, FNavMeshPolyBase*& out_Poly)
{
	if( Actor == NULL )
	{
		return FALSE;
	}

	return GetPylonAndPolyFromPos(Actor->Location,DEFAULT_MIN_WALKABLE_Z,out_Pylon,out_Poly);
}

UBOOL UNavigationHandle::IsAnchorInescapable()
{
	if(!PopulatePathfindingParamCache())
	{
		return FALSE;
	}


	AnchorPoly = GetAnchorPoly();

	if(AnchorPoly == NULL)
	{
		return TRUE;
	}

	return !AnchorPoly->IsEscapableBy(CachedPathParams);
}

/**
 * @param PathParams - path params representing the AI we're asking about
 * @returns TRUE if this poly is escapable by the entity represented by the passed pathparams
 */
UBOOL FNavMeshPolyBase::IsEscapableBy( const FNavMeshPathParams& PathParams )
{
	FNavMeshEdgeBase* Edge = NULL;

	INT NumEdges = GetNumEdges();
	for(INT Idx=0;Idx<NumEdges;++Idx)
	{
		Edge = GetEdgeFromIdx(Idx);

		if( Edge != NULL )
		{
			// clear transient path vars so supports doesn't think we're mid path search and try to do bad things
			Edge->ConditionalClear(0);

			if(Edge->Supports(PathParams,this,NULL))
			{
				// we found an outbound edge that supports us, we can leave
				return TRUE;
			}
		}
	}

	return FALSE;
}



UBOOL UNavigationHandle::GetAllPylonsFromPos(const FVector& Pos, const FVector& Extent, TArray<APylon*>& out_Pylons,UBOOL bWalkable)
{
	FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

	if( Octree == NULL )
	{
		return FALSE;
	}

	FBoxCenterAndExtent QueryBox(Pos,Extent);

	// Iterate over the octree nodes containing the query point.
	for(FPylonOctreeType::TConstElementBoxIterator<> OctreeIt(*(Octree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
	{
		APylon* CurPylon = OctreeIt.GetCurrentElement();
		check(CurPylon);


		if(CurPylon->IsValid())
		{
			if( bWalkable == TRUE )
			{
				// this will check to see that the Pylon being tested has a "Walkable" path back to the passed in Pos
				FNavMeshPolyBase* Poly = CurPylon->NavMeshPtr->GetPolyFromPoint(Pos,-1.f,WORLD_SPACE);
				if(Poly != NULL)
				{
					out_Pylons.AddItem(CurPylon);
				}
			}
			else
			{
				out_Pylons.AddItem(CurPylon);
			}
		}
	}

	return out_Pylons.Num() > 0;
}

/**
 * GetPylonANdPolyFromPos
 * - will search for the pylon and polygon that contain this point
 * @param Pos - position to get pylon and poly from
 * @param out_Pylon - output var for pylon this position is within
 * @param out_Poly - output var for poly this position is within
 * @param PylonsToConsider - only check these pylons (useful for perf)
 * @return - TRUE if the pylon and poly were found succesfully
 */
UBOOL UNavigationHandle::GetPylonAndPolyFromPos(const FVector& Pos, FLOAT MinWalkableFloorZ, APylon*& out_Pylon, struct FNavMeshPolyBase*& out_Poly, TArray<APylon*>* PylonsToConsider/*=NULL*/)
{
#if PERF_DO_PATH_PROFILING_MESH
	static INT	 GPAPFPCallCount =0;
	static FLOAT GPAPFPCallMax = -1.f;
	static FLOAT GPAPFPCallAvg=-1.f;
	static FLOAT GPAPFPCallTotal=0.f;
	SCOPETIMER(GPAPFPCallCount,GPAPFPCallAvg,GPAPFPCallMax,GPAPFPCallTotal,GetPylonAndPolyFromPos)
#endif


	static TArray<APylon*> Pylons;
	Pylons.Reset();

	if( PylonsToConsider == NULL )
	{
		PylonsToConsider = &Pylons;
		
		FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

		if( Octree == NULL )
		{
			return FALSE;
		}

		FBoxCenterAndExtent QueryBox(Pos, FVector(5.f,5.f,Max<FLOAT>(1024.f,AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MaxPolyHeight)));

		// Iterate over the octree nodes containing the query point.
		for(FPylonOctreeType::TConstElementBoxIterator<> OctreeIt(*(Octree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
		{
			APylon* CurPylon = OctreeIt.GetCurrentElement();
			check(CurPylon);

			
			if(CurPylon->IsValid())
			{
				PylonsToConsider->AddItem(CurPylon);
			}
		}
	}


	for(INT PylonIdx=0;PylonIdx<PylonsToConsider->Num();++PylonIdx)
	{
		APylon* CurPylon = (*PylonsToConsider)(PylonIdx);

		FNavMeshPolyBase* Poly = CurPylon->NavMeshPtr->GetPolyFromPoint(Pos,MinWalkableFloorZ,WORLD_SPACE);
		if(Poly != NULL)
		{
			out_Poly = Poly;
			out_Pylon = CurPylon;
			return TRUE;
		}
	}


	out_Poly = NULL;
	out_Pylon = NULL;
	return FALSE;
}

/**
* GetAnchorPoly
* - will find a suitable anchor (start) poly for this handle
* @return - the suitable poly (if any)
*/
FNavMeshPolyBase* UNavigationHandle::GetAnchorPoly()
{
	APylon* Py = NULL;
	FNavMeshPolyBase* Poly = NULL;


	GetPylonAndPolyFromPos(CachedPathParams.SearchStart,CachedPathParams.MinWalkableZ,Py,Poly);

	return Poly;
}


/**
* GetPylonAndPolyFromBox
* - will search for the pylon and polygon that contain this box
* @param Box - the box to use to find a poly for
* @param out_Pylon - output var for pylon this position is within
* @param out_Poly - output var for poly this position is within
* @return - TRUE if the pylon and poly were found succesfully
*/
UBOOL UNavigationHandle::GetPylonAndPolyFromBox(const FBox& Box, FLOAT MinWalkableZ, APylon*& out_Pylon, struct FNavMeshPolyBase*& out_Poly)
{
#if PERF_DO_PATH_PROFILING_MESH
	static INT	 GPAPFPCallCount =0;
	static FLOAT GPAPFPCallMax = -1.f;
	static FLOAT GPAPFPCallAvg=-1.f;
	static FLOAT GPAPFPCallTotal=0.f;
	SCOPETIMER(GPAPFPCallCount,GPAPFPCallAvg,GPAPFPCallMax,GPAPFPCallTotal,GetPylonAndPolyFromBox)
#endif

	FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

	if( Octree == NULL )
	{
		return FALSE;
	}

	FBoxCenterAndExtent QueryBox(Box.ExpandBy(DEFAULT_BOX_PADDING));

	// Iterate over the octree nodes containing the query point.
	for(FPylonOctreeType::TConstElementBoxIterator<> OctreeIt(*(Octree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
	{
		APylon* CurPylon = OctreeIt.GetCurrentElement();
		check(CurPylon);


		if(CurPylon->IsValid())
		{
			FNavMeshPolyBase* Poly = CurPylon->NavMeshPtr->GetPolyFromBox(Box,MinWalkableZ);
			if(Poly != NULL)
			{
				out_Poly = Poly;
				out_Pylon = CurPylon;
				return TRUE;
			}
		}
	}

	out_Poly = NULL;
	out_Pylon = NULL;
	return FALSE;
}

UBOOL UNavigationHandle::GetAllPolysFromPos( const FVector& Pos, const FVector& Extent, TArray<struct FNavMeshPolyBase*>& out_PolyList, UBOOL bIgnoreDynamic, UBOOL bReturnBothDynamicAndStatic/*=FALSE*/, TArray<APylon*>* PylonsToConsider/*=NULL*/, DWORD TraceFlags/*=0*/ )
{
	static TArray<APylon*> Pylons;
	Pylons.Reset();

	if( PylonsToConsider == NULL )
	{
		PylonsToConsider=&Pylons;
		FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

		if( Octree == NULL )
		{
			return FALSE;
		}

		FBoxCenterAndExtent QueryBox(Pos,Extent);

		// Iterate over the octree nodes containing the query point.
		for( FPylonOctreeType::TConstElementBoxIterator<> OctreeIt(*(Octree),QueryBox); OctreeIt.HasPendingElements();OctreeIt.Advance())
		{
			APylon* CurPylon = OctreeIt.GetCurrentElement();
			check(CurPylon);

			if(CurPylon->IsValid())
			{
				PylonsToConsider->AddItem(CurPylon);
			}
		}
	}


	for(INT PylonIdx=0;PylonIdx<PylonsToConsider->Num();++PylonIdx)
	{
		APylon* CurPylon = (*PylonsToConsider)(PylonIdx);
		CurPylon->GetIntersectingPolys( Pos, Extent, out_PolyList, bIgnoreDynamic, bReturnBothDynamicAndStatic,TraceFlags);
	}


	return (out_PolyList.Num()>0);
}

/**
 * GetAllObstaclePolysFromPos
 * will return all obstacle polys in any mesh which are within the passed extent
 * @param Pos - Center of extent to check
 * @param Extent - extent of box to check
 * @param out_PolyList - output array of polys to check
 * @param bSkipDynamicObstacleMesh - OPTIONAL param, when true only static obstacle mesh polys will be returned
 * @return TRUE if polys were found
 */											 
void UNavigationHandle::GetAllObstaclePolysFromPos( const FVector& Pos,
											const FVector& Extent,
											TArray<struct FNavMeshPolyBase*>& out_PolyList,
											const TArray<APylon*>* PylonsToCheck/*=NULL*/,
											UBOOL bSkipDynamicObstacleMesh/*=FALSE*/,
											DWORD TraceFlags/*=0*/)
{
	// if we've been given an explicit list of pylons to check, do that and nothing else
	static TArray<APylon*> Pylons;
	Pylons.Reset();
	if( PylonsToCheck == NULL )
	{
		GetAllOverlappingPylonsFromBox(Pos,Extent,Pylons);
		PylonsToCheck = &Pylons;
	}


	for(INT PyIdx=0;PyIdx< PylonsToCheck->Num();++PyIdx)
	{
		APylon* Pylon = (*PylonsToCheck)(PyIdx);
		// check start pylon's mesh
		if (Pylon != NULL)
		{
			if (Pylon->ObstacleMesh != NULL)
			{
				Pylon->ObstacleMesh->GetIntersectingPolys(Pos,Extent,out_PolyList,WORLD_SPACE,FALSE,FALSE,FALSE,TraceFlags);
			}

			if( !bSkipDynamicObstacleMesh )
			{
				if( Pylon->DynamicObstacleMesh != NULL )
				{
					Pylon->DynamicObstacleMesh->GetIntersectingPolys(Pos,Extent,out_PolyList,WORLD_SPACE,FALSE,FALSE,FALSE,TraceFlags);
				}
			}
		}
	}
}

void UNavigationHandle::GetAllPolyCentersWithinBounds( FVector Pos, FVector Extent, TArray<FVector>& out_PolyCtrs)
{
	TArray<FNavMeshPolyBase*> Polys;
	if(!GetAllPolysFromPos(Pos,Extent,Polys,FALSE))
	{
		return;
	}

	for(INT PolyIdx=0;PolyIdx<Polys.Num();PolyIdx++)
	{
		out_PolyCtrs.AddItem(Polys(PolyIdx)->GetPolyCenter());
	}
}

UBOOL UNavigationHandle::GetAllCoverSlotsInRadius( FVector FromLoc, FLOAT Radius, TArray<FCoverInfo>& out_CoverList )
{
	FVector Extent(Radius,Radius,80.f);
	TArray<FNavMeshPolyBase*> PolyList;
	
	if( !UNavigationHandle::GetAllPolysFromPos( FromLoc, Extent, PolyList, FALSE, TRUE ) )
	{
		return FALSE;
	}

	for( INT PolyIdx = 0; PolyIdx < PolyList.Num(); PolyIdx++ )
	{
		FNavMeshPolyBase* Poly = PolyList(PolyIdx);
		if( Poly == NULL )
		{
			continue;
		}

		for( INT CovIdx = 0; CovIdx < Poly->PolyCover.Num(); CovIdx++ )
		{
			ACoverLink* Link = Cast<ACoverLink>(*Poly->PolyCover(CovIdx));
			INT SlotIdx = Poly->PolyCover(CovIdx).SlotIdx;
			// validate the owning link information
			if( !Link || SlotIdx < 0 || SlotIdx > Link->Slots.Num() - 1 )
			{
				continue;
			}

			FCoverInfo Info;
			Info.Link	 = Link;
			Info.SlotIdx = SlotIdx;
			out_CoverList.AddItem( Info );
		}
	}

	return (out_CoverList.Num() > 0);
}

struct FPosInfo
{
	UBOOL bHeightTested;
	FLOAT Height;
};

void SaveHeightToNeighbors(FLOAT Height, INT X, INT Y, TArray< TArray< FPosInfo > >& Heights)
{
	// up, upright,right,rightdwn,dwn,leftdwn,left,leftup
	INT XOffsets[] = {1,1,0,-1,-1,-1, 0, 1};
	INT YOffsets[] = {0,1,1, 1, 0,-1,-1,-1};
	
	INT tmpX=X;
	INT tmpY=Y;
	for(INT Idx=0;Idx<8;++Idx)
	{
		tmpX = X+XOffsets[Idx];
		tmpY = Y+YOffsets[Idx];
		
		// if we're not out of bounds, and the height for this neighbor hasn't alread been set, gogogo
		if(tmpX >=0 && tmpX < Heights.Num() 
			&& tmpY >=0 && tmpY < Heights.Num() 
			&& Heights(tmpX)(tmpY).bHeightTested == FALSE)
		{
			Heights(tmpX)(tmpY).Height = Height;
		}
	}
}

UBOOL SavePossibleOutPos(UNavigationHandle* Handle,
						const FVector& NewPos,
						const FVector& BasePos,
						const FVector& Extent,
						UBOOL bDirectWalkToStart,
						FVector ValidBoxAroundStartPos,
						FLOAT Radius,
						FLOAT MinRadius,
						TArray<FVector>& out_PosList,
						TArray< TArray< FPosInfo > >& Heights,
						INT X,
						INT Y,
						INT MaxPositions,
						const TArray<APylon*>& PylonsWeCareAbout,
						UNavigationHandle::ValidBoxPositionFunc FitNessFunc)
{
	FLOAT DistSq = (NewPos - BasePos).SizeSquared();
	
	// if the matrix is of even dimensions we will leave the bounds of the square grid for a couple legs of the 
	// walk, so just dont' save those as they're invalid and keep going so we can return and pick up 
	// the remaining parts of the grid
	UBOOL bXInBounds = (X > -1 && X < Heights.Num());
	UBOOL bYInBounds = (Y > -1 && Y < Heights.Num());

	if(bXInBounds && bYInBounds && DistSq < Radius*Radius)
	{
		FVector UpOffset = FVector(0.f,0.f,Extent.Z);

		// find a starting height, and then do a mesh linecheck to get an accurate height from the mesh 
		FVector FloorSnappedPos = NewPos;

		if(Heights(X)(Y).bHeightTested)
		{
			FloorSnappedPos.Z = Heights(X)(Y).Height;		
		}

		FCheckResult Hit(1.f);

		//if( Handle->LineCheck(FloorSnappedPos+UpOffset,FloorSnappedPos-UpOffset*3.0f,Extent,&FloorSnappedPos,NULL))
		FNavMeshPolyBase* HitPoly = NULL;
		if( UNavigationHandle::StaticLineCheck(Hit,FloorSnappedPos+UpOffset,FloorSnappedPos-UpOffset*3.0f,Extent,&HitPoly,&PylonsWeCareAbout) )
		{
			// height check didn't hit anything!
			//GWorld->GetWorldInfo()->DrawDebugLine(FloorSnappedPos,FloorSnappedPos+UpOffset,255,0,0,TRUE);

			return FALSE;
		}
		else
		{
			FloorSnappedPos = Hit.Location;
		}
		// mark our currnent position as an actual one
		Heights(X)(Y).bHeightTested=TRUE;

		// save the height we just found to our neighboring positions
		SaveHeightToNeighbors(FloorSnappedPos.Z,X,Y,Heights);

		if( DistSq >= MinRadius*MinRadius )
		{
			
			
			
			// if we need to be directly reachable, do an obstacle line check to see if it's clear
			UBOOL bValidTrajectoryToPosFromStart = TRUE;
			if(bDirectWalkToStart)
			{
				//if(!Handle->ObstacleLineCheck(FloorSnappedPos,BasePos,Extent,&HitLoc))
					if(!UNavigationHandle::StaticObstacleLineCheck(NULL,Hit,FloorSnappedPos,BasePos,Extent,TRUE,NULL,&PylonsWeCareAbout))
				{
					// we hit something, see if the hit loc is within the acceptable box
					FBox ValidBox(0);
					
					ValidBox += BasePos-ValidBoxAroundStartPos;
					ValidBox += BasePos+ValidBoxAroundStartPos;
					if(!ValidBox.IsInside(Hit.Location))
					{
// 						GWorld->GetWorldInfo()->DrawDebugLine(FloorSnappedPos,BasePos,255,0,0,TRUE);
// 						GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(Hit.Location,FRotator::ZeroRotator,15.f,TRUE);
						bValidTrajectoryToPosFromStart=FALSE;
					}
				}
			}

			//if( Handle->ObstaclePointCheck(FloorSnappedPos,Extent) && bValidTrajectoryToPosFromStart)
			if( bValidTrajectoryToPosFromStart && UNavigationHandle::StaticObstaclePointCheck(Hit,FloorSnappedPos,Extent,NULL,&PylonsWeCareAbout))
			{
				//GWorld->GetWorldInfo()->DrawDebugBox(FloorSnappedPos,Extent,0,255,0,TRUE);

				// if we have no further fitness function, or the fitness function OK's the point, save it off
				if( FitNessFunc==NULL)
				{
					out_PosList.AddItem(FloorSnappedPos);
				}
				else
				{
					UNavigationHandle::FFitNessFuncParams Params(Handle,BasePos,Extent,FloorSnappedPos,HitPoly,&PylonsWeCareAbout);
					 if( (*FitNessFunc)(Params) )
					 {
						 out_PosList.AddItem(FloorSnappedPos);
					 }
				}

				if(MaxPositions > 0 && out_PosList.Num() >= MaxPositions)
				{
					return TRUE;
				}
			}
// 			else
// 			{
// 				GWorld->GetWorldInfo()->DrawDebugBox(FloorSnappedPos,Extent,255,0,0,TRUE);
// 			}
		}
	}
	
	return FALSE;
}

UBOOL WalkInDir(UNavigationHandle* Handle,
			   const FVector& Dir,
			   INT Dim,
			   FVector& CurPos,
			   const FVector& BasePos, 
			   const FVector& Extent,
			   UBOOL bDirectWalkToStart,
			   FVector ValidBoxAroundStartPos,
			   FLOAT Radius,
			   FLOAT MinRadius,
			   TArray<FVector>& out_PosList,
			   TArray< TArray< FPosInfo > >& Heights,
			   INT& X,
			   INT& Y,
			   INT MaxPositions,
			   const TArray<APylon*>& PylonsWeCareAbout,
			   UNavigationHandle::ValidBoxPositionFunc FitNessFunc)
{
	for(INT Idx=0;Idx<2*Dim;++Idx)
	{
		CurPos += Dir;
		X += appTrunc(Clamp<FLOAT>(Dir.X,-1.f,1.f));
		Y += appTrunc(Clamp<FLOAT>(Dir.Y,-1.f,1.f));

		if( SavePossibleOutPos(Handle,CurPos,BasePos,Extent,bDirectWalkToStart,ValidBoxAroundStartPos,Radius,MinRadius,out_PosList,Heights,X,Y,MaxPositions,PylonsWeCareAbout,FitNessFunc) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

void UNavigationHandle::GetAllOverlappingPylonsFromBox(const FVector& Ctr, const FVector& Extent, TArray<APylon*>& out_OverlappingPylons)
{
	FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

	if( Octree == NULL )
	{
		return;
	}

	FBoxCenterAndExtent QueryBox(Ctr, Extent);

	// Iterate over the octree nodes containing the query point.
	for(FPylonOctreeType::TConstElementBoxIterator<> OctreeIt(*(Octree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
	{
		out_OverlappingPylons.AddUniqueItem(OctreeIt.GetCurrentElement());
	}
	
}

void UNavigationHandle::GetValidPositionsForBox( FVector Pos,
												FLOAT radius,
												FVector Extent, 
												UBOOL bMustBeReachableFromStartPos, 
												TArray<FVector>& out_ValidPositions,
												INT MaxValidPositions,
												FLOAT MinRadius,
												FVector ValidBoxAroundStartPos)
{
	// passthru to real function
	GetValidPositionsForBoxEx(Pos,radius,Extent,bMustBeReachableFromStartPos,out_ValidPositions,MaxValidPositions,MinRadius,ValidBoxAroundStartPos);
}

/**
 * will return a list of valid spots on the mesh which fit the passed extent and are within radius to Pos
 * @param Pos - Center of bounds to check for polys
 * @param Radius - radius from Pos to find valid positions within
 * @param Extent - Extent of entity we're finding a spot for
 * @param bMustBeReachableFromStartPos - if TRUE, only positions which are directly reachable from the starting position will be returned
 * @param ValidPositions - out var of valid positions for the passed entity size
 * @param MaxPositions - the maximum positions needed (e.g. the search for valid positions will stop after this many have been found)
 * @param MinRadius    - minimum distance from center position to potential spots (default 0)
 * @param ValidBoxAroundStartPos - when bMustBeReachableFromStartPos is TRUE, all hits that are within this AABB of the start pos will be considered valid
 * @param ValidBoxPositionFunc - function pointer which can be supplied to filter out potential points from the result list
 */
void UNavigationHandle::GetValidPositionsForBoxEx(FVector Pos,
												FLOAT radius,
												FVector Extent,
												UBOOL bMustBeReachableFromStartPos,
												TArray<FVector>& out_ValidPositions,
												INT MaxValidPositions/*=-1*/,
												FLOAT MinRadius/*=0*/,
												FVector ValidBoxAroundStartPos/*=FVector(0.000000,0.000000,0.000000)*/,
												ValidBoxPositionFunc FitnessFunction/*=NULL*/)
{
	SCOPE_QUICK_TIMER_NAVHANDLECPP(GetValidPositionsForBox)	


	static TArray<APylon*> PylonsWeCareAbout;
	PylonsWeCareAbout.Reset();
	GetAllOverlappingPylonsFromBox(Pos,FVector(radius),PylonsWeCareAbout);
	if(PylonsWeCareAbout.Num() == 0)
	{
		return;
	}

	FLOAT Width = 2.0f * Max<FLOAT>(Extent.X,Extent.Y);

	
	if(radius < KINDA_SMALL_NUMBER || Width < KINDA_SMALL_NUMBER)
	{
		return;
	}

	INT HalfMaxDim = appCeil(radius/Width);

	if(HalfMaxDim<1)
	{
		return;
	}

	FVector CurPos = Pos;
	FVector Right = FVector(0.f,Width,0.f);
	FVector Down = FVector(-Width,0.f,0.f);
	FVector Left = FVector(0.f,-Width,0.f);
	FVector Up = FVector(Width,0.f,0.f);

	TArray< TArray< FPosInfo > > Heights;
	Heights.AddZeroed(2*HalfMaxDim);
	for(INT Idx=0;Idx<Heights.Num();++Idx)
	{
		Heights(Idx).AddZeroed(2*HalfMaxDim);
	}

	INT X,Y;
	X = Y = HalfMaxDim-1;

	Heights(X)(Y).Height = Pos.Z;
	Heights(X)(Y).bHeightTested=TRUE;

	if( SavePossibleOutPos(this,Pos,Pos,Extent,bMustBeReachableFromStartPos,ValidBoxAroundStartPos,radius,MinRadius,out_ValidPositions,Heights,X,Y,MaxValidPositions,PylonsWeCareAbout,FitnessFunction) )
	{
		return;
	}
	
	if(HalfMaxDim <=0)
	{
		return;
	}


	for(INT CurDim=1;CurDim<=HalfMaxDim;++CurDim)
	{
		 CurPos += FVector(Width,Width,0.f);
		 X++;
		 Y++;
		// down
		if( WalkInDir(this,Down,CurDim,CurPos,Pos,Extent,bMustBeReachableFromStartPos,ValidBoxAroundStartPos,radius,MinRadius,out_ValidPositions,Heights,X,Y,MaxValidPositions,PylonsWeCareAbout,FitnessFunction) )
		{
			return;
		}

		// to the left
		if( WalkInDir(this,Left,CurDim,CurPos,Pos,Extent,bMustBeReachableFromStartPos,ValidBoxAroundStartPos,radius,MinRadius,out_ValidPositions,Heights,X,Y,MaxValidPositions,PylonsWeCareAbout,FitnessFunction))
		{
			return;
		}
		// up
		if( WalkInDir(this,Up,CurDim,CurPos,Pos,Extent,bMustBeReachableFromStartPos,ValidBoxAroundStartPos,radius,MinRadius,out_ValidPositions,Heights,X,Y,MaxValidPositions,PylonsWeCareAbout,FitnessFunction))
		{
			return;
		}
		// to the right
		if( WalkInDir(this,Right,CurDim,CurPos,Pos,Extent,bMustBeReachableFromStartPos,ValidBoxAroundStartPos,radius,MinRadius,out_ValidPositions,Heights,X,Y,MaxValidPositions,PylonsWeCareAbout,FitnessFunction) )
		{
			return;
		}
	}

}


APylon* UNavigationHandle::GetPylonFromPos( FVector Position )
{
	return StaticGetPylonFromPos(Position);
}

APylon* UNavigationHandle::StaticGetPylonFromPos( FVector Position )
{
	FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();

	if( Octree == NULL )
	{
		return NULL;
	}

	APylon* Pylon=NULL;
	FNavMeshPolyBase* Poly = NULL;
	if(GetPylonAndPolyFromPos(Position,-1.f,Pylon,Poly))
	{
		return Pylon;
	}

	return NULL;
}

void UNavigationHandle::GetIntersectingPylons(const FVector& Loc, const FVector& Extent, TArray<APylon*>& out_Pylons, AActor* SrcActor )
{
	FPylonOctreeType* Octree = FNavMeshWorld::GetPylonOctree();
	if(Octree == NULL)
	{
		return;
	}

//	FNavMeshWorld::DrawPylonOctreeBounds( Octree );

	FBoxCenterAndExtent QueryBox(Loc, Extent);
	// Iterate over the octree nodes containing the query point.
	for(FPylonOctreeType::TConstElementBoxIterator<> OctreeIt(*(Octree),QueryBox); OctreeIt.HasPendingElements(); OctreeIt.Advance())
	{
		APylon* CurPylon = OctreeIt.GetCurrentElement();
		checkSlowish(CurPylon != NULL);

		if(CurPylon->IsValid())
		{
			out_Pylons.AddItem(CurPylon);
		}
	}
}

UBOOL UNavigationHandle::PopulatePathfindingParamCache()
{
	IInterface_NavigationHandle* Interface = InterfaceCast<IInterface_NavigationHandle>(GetOuter());
	if(Interface == NULL)
	{
		return FALSE;
	}

	// populate path finding parameters
	Interface->SetupPathfindingParams(CachedPathParams);
	CachedPathParams.Interface = Interface;
	return TRUE;
}

#define CAPTURE_FINDPATH_PERF 0
UBOOL UNavigationHandle::FindPath( AActor** out_DestActor, INT* out_DestItem )
{
#if CAPTURE_FINDPATH_PERF
	static UBOOL bDoIt = FALSE;
	if( bDoIt == TRUE )
	{
		GCurrentTraceName = NAME_Game;
	}
	else
	{
		GCurrentTraceName = NAME_None;
	}

	appStartCPUTrace( NAME_Game, FALSE, TRUE, 40, NULL );
#endif

	class ScopedClearTransientInfo
	{
	public:
		ScopedClearTransientInfo(UNavigationHandle* Hand) : MyHandle(Hand) {}
		~ScopedClearTransientInfo()
		{
			MyHandle->ClearConstraints();
			FNavMeshPolyBase::ClearTransientCosts();
		}
		UNavigationHandle* MyHandle;
	};
	ScopedClearTransientInfo Clear(this);

#if PERF_DO_PATH_PROFILING_MESH
	DWORD Time =0;
	CLOCK_CYCLES(Time);
#endif

	IInterface_NavigationHandle* Interface = InterfaceCast<IInterface_NavigationHandle>(GetOuter());
	if( Interface == NULL )
	{
		return FALSE;
	}
	Interface->InitForPathfinding();

	if(!PopulatePathfindingParamCache())
	{
		return FALSE;
	}

	VERBOSE_LOG_START_PATH_SEARCH(this)

	if( !CachedPathParams.bAbleToSearch )
	{
		VERBOSE_LOG_PATH_MESSAGE(this,TEXT("FINDPATH RETURNING FALSE! bAbleToSearch is FALSE"))
		return FALSE;
	}


	PathCache_Empty();

#if PERF_DO_PATH_PROFILING_MESH
	UNCLOCK_CYCLES(Time);
	const FLOAT PreGenPathTime = Time*GSecondsPerCycle*1000.f;
	Time=0;
	CLOCK_CYCLES(Time);
#endif

	const UBOOL bResult = GeneratePath( out_DestActor, out_DestItem );
	if( !bResult )
	{
		AnchorPylon = NULL;
	}

#if PERF_DO_PATH_PROFILING_MESH
	UNCLOCK_CYCLES(Time);
	const FLOAT TotalTime = PreGenPathTime+(Time*GSecondsPerCycle*1000.f);
	if(TotalTime >= PATH_PROFILING_MESH_THRESHOLD)
	{
		debugf(TEXT(">>>>>>>>> WARNING! FINDPATH SPIKE!"));
		debugf(TEXT("   GoalEvaluators: "));
		UNavMeshPathGoalEvaluator* GoalEval = PathGoalList; 
		while(GoalEval!=NULL)
		{
			debugf(TEXT("      -%s"),*GoalEval->GetName());
			GoalEval = GoalEval->NextEvaluator;
		}
		debugf(TEXT("   Constraints: "));
		UNavMeshPathConstraint* Const = PathConstraintList;
		while(Const!=NULL)
		{
			debugf(TEXT("      -%s"),*Const->GetName());
			Const = Const->NextConstraint;
		}
		debugf(TEXT(">> FindPath(PRE GENERATEPATH) took %3.3fms"),PreGenPathTime);
		debugf(TEXT(">> FindPath(OVERALL) took %3.3fms (%i nodes in path) for %s (SearchStart: %s"),TotalTime,PathCache.Num(),*GetOuter()->GetName(),*CachedPathParams.SearchStart.ToString());
	}
#endif


#if DEBUG_DRAW_NAVMESH_PATH
	GWorld->GetWorldInfo()->FlushPersistentDebugLines();

	FNavMeshPolyBase* OldPoly = AnchorPoly;
	for( INT RouteIdx = 0; RouteIdx < PathCache.Num(); RouteIdx++ )
	{
		FNavMeshPolyBase* Poly = PathCache(RouteIdx)->GetOtherPoly(OldPoly);
		OldPoly = NULL;
		if(Poly != NULL)
		{
			Poly->DrawPoly( GWorld->PersistentLineBatcher, FColor(0,0,255) );
		}
	}
#endif

#if CAPTURE_FINDPATH_PERF	
	appStopCPUTrace( NAME_Game );
#endif
	return bResult;	
}

UBOOL UNavigationHandle::ObstacleLineCheck( FVector Start, FVector End, FVector Extent, FVector* out_HitLocation, FVector* out_HitNormal  )
{
	FCheckResult Hit(1.f);
	const UBOOL bResult = StaticObstacleLineCheck(GetOuter(),Hit,Start,End,Extent);
	//if(bResult == FALSE)
	//{
	//	GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(Hit.Location,FRotator(0,0,0),50.f);
	//}

	if(out_HitLocation != NULL)
	{
		*out_HitLocation = Hit.Location;
	}

	if(out_HitNormal != NULL)
	{
		*out_HitNormal = Hit.Normal;
	}
	return bResult;
}

UBOOL UNavigationHandle::LineCheck( FVector Start, FVector End, FVector Extent, FVector* out_HitLocation, FVector* out_HitNormal )
{
	FCheckResult Hit(1.f);
	UBOOL bResult = StaticLineCheck(Hit,Start,End,Extent);
	if(out_HitLocation != NULL)
	{
		*out_HitLocation = Hit.Location;
	}

	if(out_HitNormal != NULL)
	{
		*out_HitNormal = Hit.Normal;
	}
	return bResult;
}

UBOOL UNavigationHandle::ObstaclePointCheck( FVector Pt,FVector Extent )
{
	FCheckResult Hit(0.f);
	UBOOL bResult = StaticObstaclePointCheck(Hit,Pt,Extent);
	return bResult;
}

UBOOL UNavigationHandle::PointCheck(FVector Pt,FVector Extent)
{
	FCheckResult Hit(0.f);
	UBOOL bResult = StaticPointCheck(Hit,Pt,Extent);
	return bResult;
}

/**
 * static function that will do a point check agains the obstacle mesh
 * @param Hit - hitresult struct for point check
 * @param Pt - centroid of extent box to point check
 * @param Extent - extent of box to point check
 * @param out_HitPoly - optional output param stuffed with the poly we collided with (if any)
 * @param PylonsToCheck - OPTIONAL, if present only these pylons' meshes will be linecheck'd
 * @param bSkipPointInMeshCheck - OPTIONAL, if TRUE, ONLY a pointcheck against the obstacle mesh will be done, no verification that the point is on the mesh somewhere will be done (be careful with this one!)
 * @param TraceFlags - flags for trace dilineation
 * @return TRUE of nothing hit
 */
UBOOL UNavigationHandle::StaticObstaclePointCheck(FCheckResult& Hit,
												  FVector Pt,
												  FVector Extent,
												  FNavMeshPolyBase** out_HitPoly/*=NULL*/,
												  const TArray<APylon*>* PylonsToCheck/*=NULL*/,
												  UBOOL bSkipPointInMeshCheck/*=FALSE*/,
												  DWORD TraceFlags/*=0*/
												  )
{
	SCOPE_CYCLE_COUNTER(STAT_ObstaclePointCheckTime);

	// ensure the point is in the mesh somewhere
	UBOOL bInAtLeastOnePoly = bSkipPointInMeshCheck;

	// if we've been given an explicit list of pylons to check, do that and nothing else
	if(PylonsToCheck != NULL)
	{
		UNavigationMeshBase *CurMesh = NULL, *CurObsMesh = NULL;
		for(INT PylonIdx=0;PylonIdx<PylonsToCheck->Num();++PylonIdx)
		{
			CurMesh = (*PylonsToCheck)(PylonIdx)->GetNavMesh();
			bInAtLeastOnePoly = (bInAtLeastOnePoly || (CurMesh != NULL && CurMesh->GetPolyFromPoint(Pt,-1.0f,WORLD_SPACE)));
			CurObsMesh = (*PylonsToCheck)(PylonIdx)->ObstacleMesh;
			if( CurMesh != NULL && !CurObsMesh->PointCheck(CurMesh,Hit,Pt,Extent,0,out_HitPoly) )
			{
				return FALSE;
			}
		}

		return bInAtLeastOnePoly;
	}

	APylon* Pylon = StaticGetPylonFromPos(Pt);
	if(Pylon == NULL)
	{
		// if we're not in the mesh, this spot is invalid
		Hit.Location = Pt;
		Hit.Actor = NULL;

#if DEBUG_DRAW_NAVMESH_PATH
		GWorld->GetWorldInfo()->DrawDebugBox(Pt,Extent,255,0,0,TRUE);
		GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(Pt,Hit.Normal.Rotation(),15.f,TRUE);
#endif
		return FALSE;
	}

	UBOOL bHit = !Pylon->ObstacleMesh->PointCheck( Pylon->NavMeshPtr,Hit,Pt,Extent,TraceFlags,out_HitPoly);

#if DEBUG_DRAW_NAVMESH_PATH
	if(bHit)
	{
		GWorld->GetWorldInfo()->DrawDebugBox(Pt,Extent,255,0,0,TRUE);
		GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(Pt,Hit.Normal.Rotation(),15.f,TRUE);
	}
#endif
	return !bHit;
}

/**
 * static function that will do a point check agains the walkable
 * @param Hit - hitresult struct for point check
 * @param Pt - centroid of extent box to point check
 * @param Extent - extent of box to point check
 * @param out_HitPoly - OPTIONAL, poly the pointcheck hit
 * @param PylonsToCheck - OPTIONAL, if present only these pylons' meshes will be linecheck'd
 * @return TRUE of nothing hit
 */
UBOOL UNavigationHandle::StaticPointCheck(FCheckResult& Hit,
										  FVector Pt,
										  FVector Extent,
										  FNavMeshPolyBase** out_HitPoly/*=NULL*/,
										  const TArray<APylon*>* PylonsToCheck/*=NULL*/,
										  DWORD TraceFlags/*=0*/)
{
	SCOPE_CYCLE_COUNTER(STAT_ObstaclePointCheckTime);

	TArray<APylon*> Pylons;
	if( PylonsToCheck == NULL )
	{
		GetAllOverlappingPylonsFromBox(Pt,Extent,Pylons);
		PylonsToCheck = &Pylons;
	}

	
	if(PylonsToCheck->Num() == 0)
	{
		if(out_HitPoly!=NULL)
		{
			*out_HitPoly=NULL;
		}
		// if we're not in the mesh, this spot is invalid
		return FALSE;
	}

	for( INT PylonIdx=0; PylonIdx < PylonsToCheck->Num(); ++PylonIdx )
	{
		APylon* Pylon = (*PylonsToCheck)(PylonIdx);
		if (!Pylon->NavMeshPtr->PointCheck( Pylon->NavMeshPtr,Hit,Pt,Extent,TraceFlags,out_HitPoly))
		{
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * this will take the given position and attempt to move it the passed height above the poly that point is in (along a cardinal axis)
 * @param Point - point to adjust
 * @param Height - height above mesh you would like
 * @return Adjusted point
 */
FVector UNavigationHandle::MoveToDesiredHeightAboveMesh(FVector Point, FLOAT Height)
{
	// find poly
	APylon* Py = NULL;
	FNavMeshPolyBase* Poly = NULL;

	AScout* ScoutDefaultObject = AScout::GetGameSpecificDefaultScoutObject();
	if( !ScoutDefaultObject || ! GetPylonAndPolyFromPos(Point, ScoutDefaultObject->WalkableFloorZ,Py,Poly) )
	{
		return Point;
	}

	Poly->AdjustPositionToDesiredHeightAbovePoly(Point,Height);

	return Point;
}


/**
 * will find a point halfway between top of the poly and poly surface
 * @param Poly - poly to adjust for
 * @param Pt - point to adjust
 * @return Adjusted Point
 */
FVector GetHeightAdjustedPosForPoly(FNavMeshPolyBase* Poly, const FVector& Pt)
{
	FVector NewPoint = Pt;
	Poly->AdjustPositionToDesiredHeightAbovePoly(NewPoint,Poly->PolyHeight*0.5f);
	return NewPoint;
}

/**
 *  static function that will do an obstacle line check against any pylon's meshes colliding with the line passed
 *  @param InOuter - The Outer which owns this NavHandle.  Can be used for debugging
 *  @param Hit - Hitresult struct for line check
 *  @param Start - start of line check
 *  @param End - end of line check
 *  @param Extent - extent of box to sweep
 *  @param bIgnoreNormalMesh - OPTIONAL - default:FALSE - when TRUE no checks against the walkable mesh will be performed
 *  @param out_HitPoly - optional output param stuffed with the poly we collided with (if any)
 *  @param PylonsToCheck - OPTIONAL, if present only these pylons' meshes will be linecheck'd	 
 *  @param TraceFlags - bitfield to control tracing options
 *  @return TRUE if nothing hit
 */
UBOOL UNavigationHandle::StaticObstacleLineCheck( const UObject* const InOuter,
												 FCheckResult& Hit,
												 FVector Start,
												 FVector End,
												 FVector Extent,
												 UBOOL bIgnoreNormalMesh/*=FALSE*/,
												 FNavMeshPolyBase** out_HitPoly/*=NULL*/, 
												 const TArray<APylon*>* PylonsToCheck/*=NULL*/,
												 DWORD TraceFlags/*=0*/)
{

	// if we've been given an explicit list of pylons to check, do that and nothing else
	TArray<APylon*> Pylons;
	if( PylonsToCheck == NULL )
	{
		FBox TraceBox(1);
		TraceBox += Start;
		TraceBox += End;
		GetAllOverlappingPylonsFromBox(TraceBox.GetCenter(),TraceBox.GetExtent(),Pylons);
		PylonsToCheck = &Pylons;
	}


	Hit.Time = 1.0f;
	UBOOL bSingleResult = (TraceFlags & TRACE_SingleResult) != 0;
	for(INT PyIdx=0;PyIdx< PylonsToCheck->Num();++PyIdx)
	{
		APylon* StartPylon = (*PylonsToCheck)(PyIdx);
		FCheckResult TempHit(1.f);
		// check start pylon's mesh
		if (StartPylon != NULL &&
			StartPylon->ObstacleMesh != NULL &&
			!StartPylon->ObstacleMesh->LineCheck(StartPylon->NavMeshPtr,TempHit,End,Start,Extent,TraceFlags,out_HitPoly) ||
				(
				!bIgnoreNormalMesh &&
				(StartPylon->NavMeshPtr != NULL && !StartPylon->NavMeshPtr->LineCheck(StartPylon->NavMeshPtr,TempHit,End,Start,Extent,TraceFlags,out_HitPoly))
				)
			)
		{
			// if we don't care about which hit is closest, bail as soon as we find one
			if( !bSingleResult )
			{
				Hit = TempHit;
				return FALSE;
			}
			else
			{
				if( TempHit.Time < Hit.Time )
				{
					Hit = TempHit;
				}
			}
		}
	}

	return !(Hit.Time < 1.0f);
}

// works very similarly to the StaticObstacleLineCheck
UBOOL UNavigationHandle::StaticLineCheck( FCheckResult& Hit, FVector Start, FVector End, FVector Extent, FNavMeshPolyBase** out_HitPoly/*=NULL*/, const TArray<APylon*>* PylonsToCheck/*=NULL*/, DWORD TraceFlags/*=0*/ )
{
	// if we've been given an explicit list of pylons to check, do that and nothing else
	TArray<APylon*> Pylons;
	if( PylonsToCheck == NULL )
	{
		FBox TraceBox(1);
		TraceBox += Start;
		TraceBox += End;
		GetAllOverlappingPylonsFromBox(TraceBox.GetCenter(),TraceBox.GetExtent(),Pylons);
		PylonsToCheck = &Pylons;
	}


	Hit.Time = 1.0f;
	UBOOL bSingleResult = (TraceFlags & TRACE_SingleResult) != 0;
	for(INT PyIdx=0;PyIdx< PylonsToCheck->Num();++PyIdx)
	{
		APylon* StartPylon = (*PylonsToCheck)(PyIdx);
		FCheckResult TempHit(1.f);
		// check start pylon's mesh
		if (StartPylon != NULL &&
			StartPylon->NavMeshPtr != NULL &&
			!StartPylon->NavMeshPtr->LineCheck(StartPylon->NavMeshPtr,TempHit,End,Start,Extent,TraceFlags,out_HitPoly))
		{
			// if we don't care about which hit is closest, bail as soon as we find one
			if( !bSingleResult )
			{
				Hit = TempHit;
				return FALSE;
			}
			else
			{
				if( TempHit.Time < Hit.Time )
				{
					Hit = TempHit;
				}
			}
		}
	}

	return !(Hit.Time < 1.0f);
}

UBOOL UNavigationHandle::ActorReachable( AActor* A )
{
	if( A == NULL )
		return FALSE;

	return PointReachable( A->Location );
}

/**
 * called from PointReachable, and is recursive to split the cast into several that conform to the mesh
 * @param Hit - hitresult struct for point check
 * @param Pt - centroid of extent box to point check
 * @param Extent - extent of box to point check
 * @param out_HitPoly - optional output param stuffed with the poly we collided with (if any)
 * @return TRUE of nothing hit
 */
const UINT MaxDepth = 20;
UBOOL UNavigationHandle::PointReachableLineCheck( const UObject* const InOuter,
												FCheckResult& Hit,
												FVector Start,
												FVector End,
												FVector Extent,
												UBOOL bIgnoreNormalMesh/*=FALSE*/,
												FNavMeshPolyBase** out_HitPoly/*=NULL*/,
												UBOOL bComparePolyNormalZs/*=FALSE*/,
												DWORD TraceFlags/*=0*/,
												UINT StackDepth/*=0*/)
{

	if ( StackDepth > MaxDepth )
	{
		return FALSE;
	}

	FVector Cast = End-Start;
	FLOAT CastDist = Cast.Size();
	FVector Dir = Cast/CastDist;

	FVector TruncatedEnd = End;

	APylon* StartPylon = NULL;
	APylon* EndPylon = NULL;

	FNavMeshPolyBase *StartPoly = NULL, *EndPoly=NULL;
	GetPylonAndPolyFromPos(Start, -1.f, StartPylon, StartPoly);
	GetPylonAndPolyFromPos(End, -1.f, EndPylon, EndPoly);

	// whether or not we need to continue the raycast 
	UBOOL bNeedsRecurse = FALSE;
	// if the cast is longer than our max dist, clip the cast to our set granularity, and do the first part of the raycast, and then recurse for the rest
	if (CastDist > UCONST_LINECHECK_GRANULARITY && (StartPoly != EndPoly || StartPoly == NULL))
	{		
		TruncatedEnd = Start + Dir * (UCONST_LINECHECK_GRANULARITY-1.0f);

		bNeedsRecurse=TRUE;

		// *** Grab Start pylon offset

		// grab the pylon and poly the start position is over
		if (StartPoly != NULL)
		{
			// start from center of poly height bounds
			Start = GetHeightAdjustedPosForPoly(StartPoly,Start);
		}
		else
		{
			if(out_HitPoly!=NULL)
			{
				*out_HitPoly=NULL;
			}
			//			GWorld->GetWorldInfo()->DrawDebugSphere(Start,5.f,10,0,255,0,TRUE);
			//			GWorld->GetWorldInfo()->DrawDebugSphere(TruncatedEnd,5.f,10,255,0,0,TRUE);
			return FALSE;
		}		

		// *** Find the poly for end position, and conform end pos to computed height above the mesh
		FNavMeshPolyBase* EndPoly = NULL;

		// grab the pylon and poly the end position is over
		if( GetPylonAndPolyFromPos(TruncatedEnd,-1.f,EndPylon,EndPoly) )
		{
			// snap Z to start offset above mesh
			TruncatedEnd = GetHeightAdjustedPosForPoly(EndPoly,TruncatedEnd);
		}
	}
	else
	{
		TruncatedEnd = End;

		if (StartPoly != NULL)
		{
			// bump up start position to mid Z height of poly 
			Start = GetHeightAdjustedPosForPoly(StartPoly, Start);
		}
		if (EndPoly != NULL)
		{
			// bump up start position to mid Z height of poly 
			TruncatedEnd = GetHeightAdjustedPosForPoly(EndPoly, TruncatedEnd);
		}
	}

	if(StartPylon == NULL || StartPylon->ObstacleMesh == NULL || EndPylon == NULL || EndPylon->ObstacleMesh == NULL)
	{
#if DEBUG_DRAW_NAVMESH_PATH
		GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(TruncatedEnd,FRotator(0,0,0),50.f,TRUE);

		if(EndPylon == NULL)
		{
			GWorld->GetWorldInfo()->DrawDebugBox(TruncatedEnd,FVector(5.f),255,0,0,TRUE);
			GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(TruncatedEnd,FRotator(0,0,0),50.f,TRUE);
		}
		else if(StartPylon == NULL)
		{
			GWorld->GetWorldInfo()->DrawDebugBox(Start,FVector(5.f),255,0,0,TRUE);
			GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(Start,FRotator(0,0,0),50.f,TRUE);
		}
#endif

		// if we started in the mesh, and left it try and find the point where we crossed to stuff Hit information
		if(StartPylon != NULL && EndPylon == NULL)
		{
			if(StartPylon->ObstacleMesh->LineCheck(StartPylon->NavMeshPtr,Hit,TruncatedEnd,Start,Extent,0,out_HitPoly))
			{
				// try and check against the starting pylon's normal mesh to see if we're going through the ground
				if(StartPylon->NavMeshPtr->LineCheck(StartPylon->NavMeshPtr,Hit,TruncatedEnd,Start,Extent,0,out_HitPoly))
				{
					// 					StartPylon->DrawDebugLine(Start,End,255,0,0,TRUE);
					// 					StartPylon->DrawDebugLine(Start+FVector(0.f,0.f,1.f),TruncatedEnd+FVector(0.f,0.f,1.f),0,255,0,TRUE);
					// 					if(Extent.IsNearlyZero())
					// 					{
					// 						StartPylon->DrawDebugCoordinateSystem(End,FRotator(0,0,0),15.f,TRUE);
					// 					}
					// 					else
					// 					{
					// 						StartPylon->DrawDebugBox(End,FVector(Extent),255,125,0,TRUE);
					// 					}
					// 					debugf(TEXT("UNavigationHandle::StaticObstacleLineCheck startpt in the mesh, end point not in mesh but linecheck hit nothing?!?! Outer: %s"), *InOuter->GetFullName() );
				}
			}
		}
		return FALSE;
	}

	const AScout* const Scout = AScout::GetGameSpecificDefaultScoutObject();
	const FLOAT DefaultMinZ = Scout->WalkableFloorZ;
	const FLOAT StepHeight = Scout->NavMeshGen_MaxStepHeight;

	// check start pylon's mesh
	if (!StartPylon->ObstacleMesh->LineCheck(StartPylon->NavMeshPtr, Hit, TruncatedEnd, Start, Extent, 0, out_HitPoly))
	{
#if DEBUG_DRAW_NAVMESH_PATH
		StartPylon->DrawDebugLine(Start,TruncatedEnd,255,0,0,TRUE);
		StartPylon->DrawDebugCoordinateSystem(TruncatedEnd,FRotator(0,0,0),50.f,TRUE);
		StartPylon->DrawDebugBox(Hit.Location,Extent,255,0,0,TRUE);
#endif
		return FALSE;
	}
	
	// if both locations are in the same poly, reachability is implied
	if (StartPoly != EndPoly)
	{
		// also make sure we don't collide with the walkable mesh either (make sure we're not punching through the ground)
		if (!bIgnoreNormalMesh)
		{
			FVector CurrentLoc = Start;
			UBOOL bResult;
			UBOOL bHaveSteppedUp = FALSE;
			do
			{
				Hit = FCheckResult(1.0f);
				bResult = StartPylon->NavMeshPtr->LineCheck(StartPylon->NavMeshPtr, Hit, TruncatedEnd, CurrentLoc, Extent, 0, out_HitPoly);
				if (!bResult)
				{
					CurrentLoc = Hit.Location;
					// try step up
					if ((Hit.Time > KINDA_SMALL_NUMBER || !bHaveSteppedUp) && (CurrentLoc - TruncatedEnd).SizeSquared2D() > 100.0f)
					{
						CurrentLoc.Z += StepHeight;
						bHaveSteppedUp = TRUE;
					}
					//@todo: probably also need to handle the reverse case of irregular (but walkable) drop
					else
					{
						// didn't get anywhere or can't step up further so fail
						return FALSE;
					}
				}
			} while (!bResult);
		}

		// if start and end are in different pylons, check end pylon as well 
		if(StartPylon != EndPylon)
		{
			if (!EndPylon->ObstacleMesh->LineCheck(EndPylon->NavMeshPtr, Hit, TruncatedEnd, Start, Extent, 0, out_HitPoly))
			{
#if DEBUG_DRAW_NAVMESH_PATH
				EndPylon->DrawDebugLine(Start,TruncatedEnd,255,0,0,TRUE);
				EndPylon->DrawDebugCoordinateSystem(TruncatedEnd,FRotator(0,0,0),50.f,TRUE);
				EndPylon->DrawDebugBox(Hit.Location,Extent,255,0,0,TRUE);
#endif
				return FALSE;
			}
			// also make sure we don't collide with the walkable mesh either (make sure we're not punching through the ground)
			if (!bIgnoreNormalMesh)
			{
				FVector CurrentLoc = Start;
				UBOOL bResult;
				UBOOL bHaveSteppedUp = FALSE;
				do
				{
					Hit = FCheckResult(1.0f);
					bResult = EndPylon->NavMeshPtr->LineCheck(EndPylon->NavMeshPtr, Hit, TruncatedEnd, CurrentLoc, Extent, 0, out_HitPoly);
					if (!bResult)
					{
						CurrentLoc = Hit.Location;
						// try step up
						if ((Hit.Time > KINDA_SMALL_NUMBER || !bHaveSteppedUp) && (CurrentLoc - TruncatedEnd).SizeSquared2D() > 100.0f)
						{
							CurrentLoc.Z += StepHeight;
							bHaveSteppedUp = TRUE;
						}
						//@todo: probably also need to handle the reverse case of irregular (but walkable) drop
						else
						{
							// didn't get anywhere or can't step up further so fail
							return FALSE;
						}
					}
				} while (!bResult);
			}
		}
	}

#if DEBUG_DRAW_NAVMESH_PATH
	EndPylon->DrawDebugLine(Start,TruncatedEnd,0,255,0,TRUE);
	EndPylon->DrawDebugCoordinateSystem(TruncatedEnd,FRotator(0,0,0),50.f,TRUE);
#endif

	if(bNeedsRecurse)
	{
		// if we need to do more checking, start where we clipped the ray last time, and continue on
		return PointReachableLineCheck(InOuter,Hit,TruncatedEnd,End,Extent,bIgnoreNormalMesh,out_HitPoly,bComparePolyNormalZs,TraceFlags,++StackDepth);
	}
	else
	{
		// one last check to see if the normal of the starting/ending polys are drastically different
		if(bComparePolyNormalZs && StartPoly != NULL && EndPoly != NULL)
		{
			const FVector StartNorm = StartPoly->GetPolyNormal();
			const FVector EndNorm = EndPoly->GetPolyNormal();
			if( StartNorm.Z < DefaultMinZ ||  EndNorm.Z < DefaultMinZ )
			{
				return FALSE;
			}
		}
		return TRUE;
	}
}

/**
 * returns TRUE if Point/Actor is directly reachable
 * @param Point - point we want to test to
 * @param OverrideStartPoint (optional) - optional override for starting position of AI (default uses bot location)
 * @return TRUE if the point is reachable
 */
UBOOL UNavigationHandle::PointReachable( FVector Point, FVector OverrideStartPoint/*=FVector(EC_EventParm)*/, UBOOL bAllowHitsInEndCollisionBox/*=TRUE*/ )
{
	if(!PopulatePathfindingParamCache())
	{
		return FALSE;
	}

	FVector StartPt = CachedPathParams.SearchStart;
	if( !OverrideStartPoint.IsNearlyZero() )
	{
		StartPt = OverrideStartPoint;
	}
	
	FCheckResult Hit(1.f);
	if( PointReachableLineCheck(GetOuter(),Hit,StartPt,Point,CachedPathParams.SearchExtent,FALSE,NULL,TRUE) )
	{
		return TRUE;
	}
	else
	{
		// bring hit location back down to entity height for this check (most hits will be halfway to maxheight of poly)
		FNavMeshPolyBase* Poly = NULL;
		APylon* Pylon = NULL;
		if( !GetPylonAndPolyFromPos(Hit.Location,CachedPathParams.MinWalkableZ,Pylon,Poly) )
		{
			// couldn't find walkable poly at hitloc, it's all bad
			return FALSE;
		}

		if( bAllowHitsInEndCollisionBox )
		{
			FVector CheckLoc = Hit.Location;
			Poly->AdjustPositionToDesiredHeightAbovePoly(CheckLoc,Max<FLOAT>(CachedPathParams.SearchExtent.X,CachedPathParams.SearchExtent.Z)-1.0f);
			// if we hit something, but the collision point is within our collision extent call it good
			FBox Box(0);
			Box = FBox::BuildAABB(CheckLoc,CachedPathParams.SearchExtent);
			if(Box.IsInside(Point))
			{
				return TRUE;
			}
		}

		//GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(Hit.Location,FRotator(0,0,0),50.f);
		//GWorld->GetWorldInfo()->DrawDebugBox(Hit.Location,Interface->GetSearchExtent(),255,0,0,TRUE);
}

	return FALSE;
}

void UNavigationHandle::BeginDestroy()
{
	Super::BeginDestroy();
	for(INT Idx=0;Idx<PathCache.Num();++Idx)
	{
		FNavMeshEdgeBase* Edge = PathCache(Idx);
		if(Edge != NULL && Edge->NavMesh != NULL)
		{
			Edge->NavMesh->UnMarkEdgeAsActive(Edge,this);
		}
	}

	if(!IsTemplate())
	{
		FNavMeshWorld::UnregisterActiveHandle(this);
	}
}

void UNavigationHandle::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);
	for(INT Idx=0;Idx<PathCache.Num();++Idx)
	{
		FNavMeshEdgeBase* Edge = PathCache(Idx);
		AddReferencedObject(ObjectArray,Edge->NavMesh);
	}
}

void UNavigationHandle::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if(Ar.IsObjectReferenceCollector())
	{
		for(INT Idx=0;Idx<PathCache.Num();++Idx)
		{
			FNavMeshEdgeBase* Edge = PathCache(Idx);
			Ar << Edge->NavMesh;
		}
	}
}

/**
 * PostEdgeCleanup
 * this function is called after an edge has been cleaned up, but before it has been deleted. Whatever triggerd
 * the edge deletion is finished, so it's safe to call other code that might affect the mesh
 * @param Edge - the edge that is being cleaned up
 */
void UNavigationHandle::PostEdgeCleanup(FNavMeshEdgeBase* Edge)
{
	INT PathCacheCount = PathCache.Num();
	
	if( PathCacheCount > 0 )
	{
		PathCache_Empty();

		IInterface_NavigationHandle* Interface = InterfaceCast<IInterface_NavigationHandle>(GetOuter());	
		if(Interface != NULL)
		{
			UObject* InterfaceImplementor = Interface->GetUObjectInterfaceInterface_NavigationHandle();
			if(InterfaceImplementor != NULL && !InterfaceImplementor->HasAnyFlags(RF_Unreachable))
			{
				Interface->eventNotifyPathChanged();
			}
		}	
	}

	CurrentEdge = NULL;
	SubGoal_DestPoly = NULL;
}

void UNavigationHandle::CopyPathStoreToPathCache(const struct FPathStore& InStore)
{
	PathCache = InStore;
}

UBOOL UNavigationHandle::PathCache_Empty()
{
	return PathCache_Empty( NULL );
}

/** Allows operations on nodes in the route before emptying the cache */
UBOOL UNavigationHandle::PathCache_Empty( FPathStore* PCache )
{
	if( bSkipRouteCacheUpdates )
		return FALSE;

	if( PCache == NULL )
	{
		PCache = &PathCache;
	}

	CurrentEdge = NULL;
	SubGoal_DestPoly = NULL;
	for( INT Idx = 0; Idx < PCache->Num(); ++Idx )
	{
		FNavMeshEdgeBase* Edge = (*PCache)(Idx);
		if( Edge != NULL )
		{
			checkSlowish(Edge->NavMesh != NULL);

			Edge->NavMesh->UnMarkEdgeAsActive( Edge, this );
		}
	}

	PCache->EdgeList.Empty();

	return TRUE;
}
UBOOL UNavigationHandle::PathCache_AddEdge( FNavMeshEdgeBase* Edge, FPathStore* PCache )
{
	if( bSkipRouteCacheUpdates )
		return FALSE;

	if( PCache == NULL )
	{
		PCache = &PathCache;
	}

	if( Edge != NULL )
	{
		PCache->EdgeList.AddItem( Edge );

		checkSlowish(Edge->NavMesh != NULL);

		// notify the mesh we're using this edge 
		Edge->NavMesh->MarkEdgeAsActive(Edge,this);
	}

	return TRUE;
}

UBOOL UNavigationHandle::PathCache_InsertEdge( FNavMeshEdgeBase* Edge, INT Idx, FPathStore* PCache )
{
	if( bSkipRouteCacheUpdates )
		return FALSE;

	if( PCache == NULL )
	{
		PCache = &PathCache;
	}

	if( Edge != NULL )
	{
		PCache->EdgeList.InsertItem( Edge, Idx );

		checkSlowish(Edge->NavMesh != NULL);

		// notify the mesh we're using this edge 
		Edge->NavMesh->MarkEdgeAsActive(Edge,this);

	}

	return TRUE;
}

UBOOL UNavigationHandle::PathCache_RemoveEdge( FNavMeshEdgeBase* Edge, FPathStore* PCache )
{
	if( bSkipRouteCacheUpdates )
		return FALSE;

	if( PCache == NULL )
	{
		PCache = &PathCache;
	}

	if( Edge != NULL )
	{
		PCache->EdgeList.RemoveItem( Edge );

		checkSlowish(Edge->NavMesh != NULL);

		// notify the mesh we're using this edge 
		Edge->NavMesh->UnMarkEdgeAsActive(Edge,this);

	}

	return TRUE;
}

UBOOL UNavigationHandle::PathCache_RemoveIndex( INT Idx, INT Count )
{
	return PathCache_RemoveIndex( Idx, Count, NULL );
}

UBOOL UNavigationHandle::PathCache_RemoveIndex( INT Idx, INT Count, FPathStore* PCache )
{
	if( bSkipRouteCacheUpdates )
		return FALSE;

	if( PCache == NULL )
	{
		PCache = &PathCache;
	}

	if( Idx >= 0 && Idx < PCache->Num() )
	{
		for(INT FinCount=0;FinCount<Count;++FinCount)
		{
			FNavMeshEdgeBase* Edge = (*PCache)(Idx+FinCount);
			if(Edge != NULL)
			{
				checkSlowish(Edge->NavMesh != NULL);

				Edge->NavMesh->UnMarkEdgeAsActive(Edge,this);
			}
		}
		PCache->EdgeList.Remove( Idx, Count );
	}

	return TRUE;
}

FVector UNavigationHandle::PathCache_GetGoalPoint()
{
	return PathCache_GetGoalPoint( NULL );
}

FVector UNavigationHandle::PathCache_GetGoalPoint( FPathStore* PCache )
{
	if( PCache == NULL )
	{
		PCache = &PathCache;
	}

	if( PCache->Num() != 0 && PCache->Top()->GetPoly1() )
	{
		
		if( PopulatePathfindingParamCache() )
		{
			FNavMeshEdgeBase* LastEdge = PCache->Top();
			// determine which poly linked to the last edge is actually the last poly
			FNavMeshPolyBase* LastPoly = LastEdge->GetPoly1();
			if(PCache->Num() > 1)
			{
				// if the next to last edge shares this poly, then the real end poly is the other one on the last edge
				if(PCache->Last(1)->GetPoly0() == LastPoly || PCache->Last(1)->GetPoly1() == LastPoly)
				{
					LastPoly = PCache->Top()->GetOtherPoly(LastPoly);
				}
				// otherwise we have the right poly already
			}
			else // if the path is one edge long, find the poly the entity is not in
			{
				if(LastPoly->ContainsBox(FBox::BuildAABB(CachedPathParams.SearchStart,CachedPathParams.SearchExtent),WORLD_SPACE,CachedPathParams.MaxHoverDistance))
				{
					LastPoly = LastEdge->GetOtherPoly(LastPoly);
				}
				// otherwise he must be in the other poly, and we have the right one
			}

			return LastPoly->GetPolyCenter() + CachedPathParams.Interface->GetEdgeZAdjust(CurrentEdge);
		}		
	}
	return FVector(0, 0, 0);
}

FVector UNavigationHandle::GetBestUnfinishedPathPoint() const
{
	if( BestUnfinishedPathPoint != NULL )
	{
		return BestUnfinishedPathPoint->GetPolyCenter();
	}

	return FVector(0, 0, 0);
}

/**
 * will clip off edges from the pathcache which are greater than the specified distance from the start of the path
 * @param MaxDist - the maximum distance for the path
 */
void UNavigationHandle::LimitPathCacheDistance(FLOAT MaxDist)
{
	FNavMeshEdgeBase* Edge = NULL;
	FVector EdgePos(0.f);

	if(!PopulatePathfindingParamCache())
	{
		return;
	}
	FVector PrevPt = CachedPathParams.SearchStart;

	FLOAT TotalDist = 0.f;
	for(INT Idx=0;Idx<PathCache.Num();++Idx)
	{
		ComputeOptimalEdgePosition(Idx,EdgePos,0.f);
		TotalDist += (EdgePos - PrevPt).Size();
		PrevPt = EdgePos;

		// if we've gone too far clip off this and the rest of the path
		if(TotalDist > MaxDist)
		{
			PathCache_RemoveIndex(Idx,PathCache.Num() - Idx);
			break;
		}
	}
}

/**
 * this will calculate the optimal edge positions along the pathcache, and add up the distances 
 * to generate an accurate distance that will be travelled along the current path
 * Note: includes distance to final destination
 * @return - the path distance calculated
 */
FLOAT UNavigationHandle::CalculatePathDistance(FVector FinalDest)
{
	TArray<FVector> Points;
	if(!PopulatePathfindingParamCache())
	{
		return 0.f;
	}

	if(FinalDest==FVector(EC_EventParm))
	{
		FinalDest = *FinalDestination;
	}
	FVector LastPos = CachedPathParams.SearchStart;
	FLOAT Dist = 0.f;

	if(PathCache.Num() > 0)
	{
		FNavMeshEdgeBase* Edge = PathCache(0);
		FVector Pos(0.f);
		ComputeOptimalEdgePosition(PathCache.Num()-1,Pos,0.f,FALSE,&Points);
		Dist += (LastPos - Points(0)).Size();
		for(INT Idx=0;Idx<Points.Num()-1;++Idx)
		{
			Dist += (Points(Idx) - Points(Idx+1)).Size();
			LastPos = Points(Idx+1);
		}
	}

	if(!FinalDest.IsZero())
	{
		Dist += (LastPos - FinalDest).Size();
	}

	return Dist;
}

/**
 * this will calculate the optimal edge positions along the pathcache, and add up the distances 
 * to generate an accurate distance that will be travelled along the current path
 * Note: includes distance to final destination
 * @return - the path distance calculated
 */
void UNavigationHandle::CopyMovePointsFromPathCache(FVector FinalDest, TArray<FVector>& out_MovePoints)
{
	
	if(!PopulatePathfindingParamCache())
	{
		return;
	}

	if(FinalDest==FVector(EC_EventParm))
	{
		FinalDest = *FinalDestination;
	}
	FVector LastPos = CachedPathParams.SearchStart;
	FLOAT Dist = 0.f;

	if(PathCache.Num() > 0)
	{
		FVector Pos(0.f);
		ComputeOptimalEdgePosition(PathCache.Num()-1,Pos,0.f,FALSE,&out_MovePoints);
	}

	if(!FinalDest.IsZero())
	{
		out_MovePoints.AddItem(FinalDest);
	}

}


FVector UNavigationHandle::GetFirstMoveLocation()
{
	if(!PopulatePathfindingParamCache())
	{
		return FVector(0.f);
	}

	if(PathCache.Num() < 1)
	{
		return *FinalDestination;
	}

	FVector RetPos(0.f);
	ComputeOptimalEdgePosition(0,RetPos,0.f);

	return RetPos;
}

void UNavigationHandle::UpdateBreadCrumbs(FVector Location)
{
	const FVector RecentCrumb = Breadcrumbs[BreadCrumbMostRecentIdx];
	if( RecentCrumb.IsZero() )
	{
		Breadcrumbs[BreadCrumbMostRecentIdx] = Location;
	}
	else
	if( (RecentCrumb-Location).SizeSquared() > BreadCrumbDistanceInterval*BreadCrumbDistanceInterval )
	{
		BreadCrumbMostRecentIdx = (BreadCrumbMostRecentIdx+1) % UCONST_NumBreadCrumbs;
		Breadcrumbs[BreadCrumbMostRecentIdx] = Location;

	}
}

UBOOL UNavigationHandle::GetNextBreadCrumb(FVector& out_BreadCrumbLoc)
{
	const FVector RecentCrumb = Breadcrumbs[BreadCrumbMostRecentIdx];
	if( RecentCrumb.IsZero() )
	{
		return FALSE;
	}


	out_BreadCrumbLoc=RecentCrumb;
	if( --BreadCrumbMostRecentIdx < 0 )
	{
		BreadCrumbMostRecentIdx = UCONST_NumBreadCrumbs-1;
	}
	Breadcrumbs[BreadCrumbMostRecentIdx]=FVector(0.f);


	return TRUE;
}

UBOOL ANavMeshObstacle::GetBoundingShape(TArray<FVector>& out_PolyShape,INT ShapeIdx)
{
	if( !eventGetObstacleBoudingShape(out_PolyShape) )
	{
		return FALSE;
	}

	if( !UNavigationMeshBase::IsConvex(out_PolyShape) )
	{
		warnf(NAME_Warning,TEXT("Registration of obstacle for %s ignored, becuase shape is not convex!"),*GetName());
		out_PolyShape.Empty();
		return FALSE;
	}

	return TRUE;
}

void ANavMeshObstacle::RegisterObstacle()
{
	RegisterObstacleWithNavMesh();
}

void ANavMeshObstacle::UnRegisterObstacle()
{
	UnregisterObstacleWithNavMesh();
}

/**
 * Removes the Environment volume from the world info's list of Environment volumes.
 */
void AEnvironmentVolume::ClearComponents( void )
{
	// Route clear to super first.
	Super::ClearComponents();

	// GWorld will be NULL during exit purge.
	if( GWorld )
	{
		GWorld->GetWorldInfo()->EnvironmentVolumes.RemoveItem( this );
	}
}

/**
 * Adds the Environment volume to world info's list of Environment volumes.
 */
void AEnvironmentVolume::UpdateComponentsInternal( UBOOL bCollisionUpdate )
{
	// Route update to super first.
	Super::UpdateComponentsInternal( bCollisionUpdate );

	GWorld->GetWorldInfo()->EnvironmentVolumes.AddItem( this );
}

/** Should this volume register and split the NavMesh to affect AI pathing? */
void AEnvironmentVolume::SetSplitNavMesh(UBOOL bNewValue)
{
	if( bSplitNavMesh != bNewValue )
	{
		bSplitNavMesh = bNewValue;

		if( bSplitNavMesh )
		{
			RegisterObstacleWithNavMesh();
		}
		else
		{
			UnregisterObstacleWithNavMesh();
		}
	}
}

/** 
*  Called from edges linked to this PO
*  @param Interface     - the navhandle interface of the entity pathing 
*  @param PreviousPoint - the previous point in the path search (e.g. the point along the predecessor edge)
*  @param out_PathEdgePoint - the point we used along this edge to determine the cost 
*  @param Edge - the edge linked to this PO which needs to compute cost	 
*  @return     - the cost for traversing this edge
*/
INT AEnvironmentVolume::CostFor( const FNavMeshPathParams& PathParams, const FVector& PreviousPoint, FVector& out_PathEdgePoint, FNavMeshPathObjectEdge* Edge, FNavMeshPolyBase* SourcePoly )
{
	if( bSplitNavMesh )
	{
		AAIController* AIC = Cast<AAIController>(PathParams.Interface->GetUObjectInterfaceInterface_NavigationHandle());
		if( AIC != NULL && ShouldAIAvoidMe(AIC) )
		{
			// avoid this edge if at all possible!
			return UCONST_BLOCKEDPATHCOST;
		}
	}
	
	return Edge->FNavMeshEdgeBase::CostFor(PathParams, PreviousPoint, out_PathEdgePoint, SourcePoly);
}

/** 
 * Implement rules based on AIControllers to avoid this location. 
 * @return TRUE if this location should be avoided, FALSE otherwise.
 */
UBOOL AEnvironmentVolume::ShouldAIAvoidMe(AAIController* AIC)
{
	return FALSE;
}


/**
* called to allow this PO to draw custom stuff for edges linked to it
* @param DRSP          - the sceneproxy we're drawing for
* @param DrawOffset    - offset from the actual location we should be drawing 
* @param Edge          - the edge we're drawing
* @return - whether this PO is doing custom drawing for the passed edge (FALSE indicates the default edge drawing functionality should be used)
*/
UBOOL AEnvironmentVolume::DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset, FNavMeshPathObjectEdge* Edge )
{
	// indicate we drew the edge, so base functionality should not
	return FALSE;		
}

/**
* this function should populate out_polyshape with a list of verts which describe this object's 
* convex bounding shape
* @param out_PolyShape - output array which holds the vertex buffer for this obstacle's bounding polyshape
* @return TRUE if this object should block things right now (FALSE means this obstacle shouldn't affect the mesh)
*/
UBOOL AEnvironmentVolume::GetBoundingShape(TArray<FVector>& out_PolyShape,INT ShapeIdx)
{
	// calculate an oriented bounding shape from the AABB of the mesh in local space, then transform to WS
	FBoxSphereBounds Bounds = CollisionComponent->Bounds;

	// top right
	FVector Corner = Bounds.Origin + FVector(Bounds.BoxExtent.X,Bounds.BoxExtent.Y,Bounds.BoxExtent.Z*0.5f);
	out_PolyShape.AddItem(Corner);

	// bottom right
	Corner = Bounds.Origin + FVector(-Bounds.BoxExtent.X,Bounds.BoxExtent.Y,Bounds.BoxExtent.Z*0.5f);
	out_PolyShape.AddItem(Corner);

	// bottom left
	Corner = Bounds.Origin + FVector(-Bounds.BoxExtent.X,-Bounds.BoxExtent.Y,Bounds.BoxExtent.Z*0.5f);
	out_PolyShape.AddItem(Corner);

	// top left
	Corner = Bounds.Origin + FVector(Bounds.BoxExtent.X,-Bounds.BoxExtent.Y,Bounds.BoxExtent.Z*0.5f);
	out_PolyShape.AddItem(Corner);

// 	DrawDebugLine(out_PolyShape(0), out_PolyShape(1), 255, 0, 0, TRUE);
// 	DrawDebugLine(out_PolyShape(1), out_PolyShape(2), 255, 0, 0, TRUE);
// 	DrawDebugLine(out_PolyShape(2), out_PolyShape(3), 255, 0, 0, TRUE);
// 	DrawDebugLine(out_PolyShape(3), out_PolyShape(0), 255, 0, 0, TRUE);

	return TRUE;
}

/**
 * This function is called when an edge is going to be added connecting a polygon internal to this obstacle to another polygon which is not
 * Default behavior just a normal edge, override to add special costs or behavior (e.g. link a pathobject to the obstacle)
 * @param Status - current status of edges (e.g. what still needs adding)	 
 * @param inV1 - vertex location of first vert in the edge
 * @param inV2 - vertex location of second vert in the edge
 * @param ConnectedPolys - the polys this edge links
 * @param bEdgesNeedToBeDynamic - whether or not added edges need to be dynamic (e.g. we're adding edges between meshes)
 * @param PolyAssocatedWithThisPO - the index into the connected polys array parmaeter which tells us which poly from that array is associated with this path object
 * @(optional) param SupportedEdgeWidth - width of unit that this edge supports, defaults to -1.0f meaning the length of the edge itself will be used
 * @(optional) param EdgeGroupID - ID of the edgegroup this edge is a part of (defaults to no group)
 * @return returns an enum describing what just happened (what actions did we take) - used to determien what accompanying actions need to be taken 
 *         by other obstacles and calling code
 */
EEdgeHandlingStatus AEnvironmentVolume::AddObstacleEdge( EEdgeHandlingStatus Status, const FVector& inV1, const FVector& inV2, TArray<FNavMeshPolyBase*>& ConnectedPolys, UBOOL bEdgesNeedToBeDynamic, INT PolyAssocatedWithThisPO, FLOAT SupportedEdgeWidth/*=1.0f*/, BYTE EdgeGroupID/*=255*/)
{
	return AddObstacleEdgeForObstacle(Status,inV1,inV2,ConnectedPolys,bEdgesNeedToBeDynamic,PolyAssocatedWithThisPO,this,SupportedEdgeWidth,EdgeGroupID);
}

UBOOL ATestSplittingVolume::GetMeshSplittingPoly( TArray<FVector>& out_PolyShape, FLOAT& PolyHeight )
{
	if( BrushComponent->Brush && BrushComponent->Brush->Polys )
	{
		// find top and bottom poly
		FPoly* TopPoly = NULL;
		FPoly* BottomPoly = NULL;
		for( INT PolyIndex = 0 ; PolyIndex < BrushComponent->Brush->Polys->Element.Num() ; ++PolyIndex )
		{
			FPoly& Poly = BrushComponent->Brush->Polys->Element( PolyIndex );

			const FVector MidPt = Poly.GetMidPoint();
			if(BottomPoly == NULL || MidPt.Z < BottomPoly->GetMidPoint().Z)
			{
				BottomPoly = &Poly;
			}

			if(TopPoly == NULL || MidPt.Z > TopPoly->GetMidPoint().Z)
			{
				TopPoly = &Poly;
			}
		}

		if(TopPoly == NULL || BottomPoly == NULL)
		{
			return FALSE;
		}

		for( INT VertexIndex = 0 ; VertexIndex < BottomPoly->Vertices.Num() ; ++VertexIndex )
		{
			const FVector VertLocation = BrushComponent->LocalToWorld.TransformFVector( BottomPoly->Vertices(VertexIndex) );
			out_PolyShape.AddItem(VertLocation);
		}
		PolyHeight = Abs<FLOAT>(TopPoly->GetMidPoint().Z - BottomPoly->GetMidPoint().Z);

		// DEBUG DEBUG DEBUG
		for(INT Idx=0;Idx<4;++Idx)
		{
			INT Next = (Idx+1) % 4;
			GWorld->GetWorldInfo()->DrawDebugLine(out_PolyShape(Idx),out_PolyShape(Next),255,0,0,TRUE);
		}

		return TRUE;

	}

	return FALSE;
}

FNavMeshPolyBase* GetAdjacentPolyContainingPoint(FNavMeshPolyBase* BasePoly, const FVector& Pt, const FVector& Extent,  const FVector& DesiredMoveDir, TArray<FNavMeshPolyBase*>& AdjacentPolys)
{
static FLOAT Ep = 0.1f;
#define ADJACENT_EPSILON Ep
	
	FNavMeshPolyBase* AdjacentPoly = NULL;
	
	FNavMeshPolyBase* BestAdjacentPoly = NULL;
	FLOAT BestDot = 1.f;

	for(INT AdjacentIdx=0;AdjacentIdx<AdjacentPolys.Num();++AdjacentIdx)
	{
		AdjacentPoly = AdjacentPolys(AdjacentIdx);
		
		//GWorld->GetWorldInfo()->DrawDebugLine(BasePoly->GetPolyCenter(),AdjacentPoly->GetPolyCenter(),255,255,0);
		if(AdjacentPoly->NumObstaclesAffectingThisPoly == 0 && AdjacentPoly->ContainsPoint(Pt,WORLD_SPACE,ADJACENT_EPSILON))
		{
			FLOAT ThisDot = Abs<FLOAT>(AdjacentPoly->GetPolyNormal()|DesiredMoveDir);
			// want to find the dot closest to 0 (closest to perp with our desired move dir
			if( ThisDot < BestDot || ( AdjacentPoly == BestAdjacentPoly ) )
			{
				BestAdjacentPoly=AdjacentPoly;
				BestDot = ThisDot;
			}
		}
	}



	return BestAdjacentPoly;	
}


FLOAT GetTForPointToSegment(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint)
{
	const FVector Segment = EndPoint - StartPoint;
	const FVector VectToPoint = Point - StartPoint;

	const FLOAT Dot1 = VectToPoint | Segment;
	const FLOAT Dot2 = Segment | Segment;

	return (Dot1 / Dot2);
}


#define MAX_STACK 10
UBOOL NavMeshWalkingObstacleCheck( APawn* Pawn, UNavigationHandle* Handle, FCheckResult& Hit,const FVector& Start,const FVector& End, const FVector& Extent, const FVector& Floor, TArray<FNavMeshPolyBase*>& PolysToCheckAgainst, TArray<FCheckResult>& out_ActorHits, INT StackCount=0)
{
	FNavMeshPolyBase* HitPoly=NULL;
	FVector HitLoc(0.f);
	FLOAT HitTime;
	UBOOL bTrajectoryClear = !UNavigationMeshBase::LineCheckAgainstSpecificPolys(Start,End,Extent,PolysToCheckAgainst,HitLoc, &HitTime, &HitPoly);

	// see if we should filter this hit because it's on an obstacle mesh poly which is 
	// on an edge we are traversing (e.g. drop down edges)
	if( !bTrajectoryClear )
	{
		const FVector PolyNorm = HitPoly->GetPolyNormal();

		Hit.Location = HitLoc;
		Hit.Normal = PolyNorm;
		Hit.Time = HitTime;

		if(Handle->CurrentEdge != NULL && HitPoly != NULL)
		{
			const FVector VertLoc0 = Handle->CurrentEdge->GetVertLocation(0);
			const FVector VertLoc1 = Handle->CurrentEdge->GetVertLocation(1);
			
			const FVector HitPolyLoc = HitPoly->GetPolyCenter();
		
			FVector IsectPt(0.f);
			const FLOAT Close_Thresh = (Extent.X);


			if( StackCount < MAX_STACK && (FPointPlaneDist(VertLoc0,HitPolyLoc,PolyNorm) < Close_Thresh || FPointPlaneDist(VertLoc1,HitPolyLoc,PolyNorm) < Close_Thresh) )
			{ 
				//project hitpoly verts onto currentedge 
				const FVector EdgeDelta = VertLoc1-VertLoc0;
				FLOAT EdgeLen = EdgeDelta.Size();
				const FVector EdgeDir = EdgeDelta/EdgeLen;
				FLOAT EdgeMin=BIG_NUMBER, EdgeMax=-BIG_NUMBER;

				for(INT PolyVertIdx=0;PolyVertIdx<HitPoly->PolyVerts.Num();++PolyVertIdx)
				{
					FLOAT CurProj = (HitPoly->GetVertLocation(PolyVertIdx) - VertLoc0) | EdgeDir;

					if( CurProj > EdgeMin )
					{
						EdgeMin = CurProj;
					}
					if( CurProj < EdgeMax )
					{
						EdgeMax = CurProj;
					}
				}

				if(!Handle->CurrentEdge->IsOneWayEdge() || EdgeMax >= EdgeLen && EdgeMin <= 0.f )
				{
					FVector ProjPt = FPointPlaneProject(Hit.Location,HitPolyLoc,PolyNorm);

					FLOAT ProjToEdge_T = GetTForPointToSegment(ProjPt,VertLoc0,VertLoc1);
					
					// compute a tolerance based on the T value of the radius of this entity.. the idea being that if we're within entityradius of the current edge we're good
					FLOAT Edge_T_Tolerance = Extent.X / Handle->CurrentEdge->GetEdgeLength();

					if(ProjToEdge_T > -Edge_T_Tolerance && ProjToEdge_T < 1.f+Edge_T_Tolerance)
					{
						FVector NewStart = Hit.Location + (End-Start).SafeNormal()*FBoxPushOut(Hit.Normal,Extent*1.05f);

						if( ((End-NewStart) | (End-Start)) > 0.f )
						{
							bTrajectoryClear=NavMeshWalkingObstacleCheck(Pawn,Handle,Hit,NewStart,End,Extent,Floor,PolysToCheckAgainst,out_ActorHits,StackCount+1);
						}
						else
						{
							bTrajectoryClear=TRUE;
						}
					}
				}
			}
		}

	}


	// we cleared the navmesh, now check against actors
	if ( bTrajectoryClear && StackCount==0 && 
		( (Pawn->bCollideActors || Pawn->bCollideWorld) &&
		Pawn->CollisionComponent ))
	{
		//DWORD TraceFlags = TRACE_Pawns | TRACE_Others | TRACE_Volumes;
		//DWORD TraceFlags = TRACE_Pawns | TRACE_Others;
		DWORD TraceFlags = TRACE_Pawns;

		FCheckResult* FirstHit = NULL;

		FirstHit = GWorld->MultiLineCheck
			(
			GMainThreadMemStack,
			End,
			Start,
			Pawn->GetCylinderExtent(),
			TraceFlags,
			Pawn
			);

		// Handle first blocking actor.
		if( Pawn->bCollideWorld || Pawn->bBlockActors )
		{
			Hit = FCheckResult(1.f);
			for( FCheckResult* Test=FirstHit; Test; Test=Test->GetNext() )
			{
				if( (!Pawn->IsBasedOn(Test->Actor)) && !Test->Actor->IsBasedOn(Pawn) )
				{
					Hit = *Test;
					out_ActorHits.AddItem(Hit);

					if( Pawn->IsBlockedBy(Test->Actor,Test->Component) )
					{
						bTrajectoryClear=FALSE;
						// break after we hit something that stops us
						break;
					}
				}
			}
		}
	}

	
	return bTrajectoryClear;
}


UBOOL NavMeshWalkingDropToFloor(FVector& out_NewLoc, FNavMeshPolyBase* Poly, FLOAT CollisionHeight , FLOAT DeltaTime)
{

#define MAX_FLOOR_DROP_SPEED 2.0f * CollisionHeight * DeltaTime

	FLOAT OldZ=out_NewLoc.Z;
	Poly->AdjustPositionToDesiredHeightAbovePoly(out_NewLoc,CollisionHeight);
	
	FLOAT Delta = out_NewLoc.Z - OldZ;
	
	out_NewLoc.Z = OldZ + Clamp<FLOAT>(Delta, -MAX_FLOOR_DROP_SPEED, MAX_FLOOR_DROP_SPEED);
	
	return FALSE;
}

UBOOL GetPylonsToCheck(UNavigationHandle* Handle, const FVector& Mid, const FVector& CacheExtent, TArray<APylon*>& PylonsToConsider)
{

	PylonsToConsider.Reset();

	// avoid the octree if we can
	const FBox ThisBox = FBox::BuildAABB(Mid,CacheExtent);
	APylon* RESTRICT AnchorPylon = Handle->AnchorPylon;
	if( AnchorPylon != NULL && AnchorPylon->IsValid() )
	{
		FBox PylonBounds = AnchorPylon->GetBounds(WORLD_SPACE);
		if( PylonBounds.Intersect(ThisBox) )
		{
			PylonsToConsider.AddItem(Handle->AnchorPylon);
			

			for (INT PathIdx = 0; PathIdx < AnchorPylon->PathList.Num(); PathIdx++)
			{
				// if it is a valid connection that the pawn can walk
				UReachSpec *Spec = AnchorPylon->PathList(PathIdx);
				if (Spec == NULL || Spec->bDisabled || *Spec->End == NULL || Spec->End->ActorIsPendingKill())
				{
					continue;
				}

				APylon *CurrentNeighbor = Cast<APylon>(Spec->End.Nav());

				if( CurrentNeighbor == NULL || CurrentNeighbor->bDisabled )
				{
					continue;
				}

				PylonBounds = CurrentNeighbor->GetBounds(WORLD_SPACE);
				if( PylonBounds.Intersect(ThisBox) )
				{
					PylonsToConsider.AddItem(CurrentNeighbor);
				}
			}

			return TRUE;
		}
	}

	
	// if we haven't found a valid early out pylon, call getallpylonsfrompos
	if( PylonsToConsider.Num() == 0 )
	{
		UNavigationHandle::GetAllPylonsFromPos(Mid,CacheExtent,PylonsToConsider,FALSE);		
	}

	return FALSE;
}

void APawn::physNavMeshWalking(FLOAT deltaTime)
{
	// can't cling to navmesh with no navhandle!
	if( Controller == NULL || Controller->NavigationHandle == NULL )
	{
		Acceleration	= FVector(0.f);
		Velocity		= FVector(0.f);
		return;
	}

	UNavigationHandle* Handle = Controller->NavigationHandle;
	FVector Extent = GetCylinderExtent();

	// ** update our current polygon
	FNavMeshPolyBase* CurrentPoly = Handle->AnchorPoly;
	APylon* Py=NULL;

	// ** figure out what our desired move is
	FVector AccelDir = Acceleration.SafeNormal();
	CalcVelocity( AccelDir, deltaTime, GroundSpeed, PhysicsVolume->GroundFriction, FALSE, TRUE, FALSE );

	// cache some stuff!
	static TArray<FNavMeshPolyBase*> ObstaclePolysToConsider;
	ObstaclePolysToConsider.Reset();
	static TArray<FNavMeshPolyBase*> PolysToConsider;
	PolysToConsider.Reset();
	static TArray<APylon*> PylonsToConsider;
	// is reset inside 'GetPylonsToCheck' conditionally

	//DrawDebugLine(Location,Location+Velocity.SafeNormal() * 200.f,255,0,0);
	FLOAT Mag = Velocity.Size();
	//FVector OldVel = Velocity;
	Velocity = (Velocity - Floor * (Floor | Velocity)).SafeNormal() * Mag;
	FVector DesiredMove = Velocity;
	//debugf(TEXT("ACCEL: %s VELOCITY PRE: %s Post: %s "), *Velocity.ToString(), *OldVel.ToString(), *Velocity.ToString());

	DesiredMove *= deltaTime;
	FLOAT DesiredMag = DesiredMove.Size();
	FVector OldLocation = Location;
	FVector ProjectedDesired = OldLocation+DesiredMove;	
	FVector Delta = ProjectedDesired-Location;
	FVector Mid = Location + (Delta * 0.5f);
	FVector CacheExtent = FVector(Abs<FLOAT>(Delta.GetMax()));

	// cache off pylons so we only have to do this once
	UBOOL bUsingCached = GetPylonsToCheck(Handle, Mid, Extent+CacheExtent,PylonsToConsider);
	// now grab polys in the area and obstacle mesh polys in the area
	UNavigationHandle::GetAllObstaclePolysFromPos(Mid, Extent+CacheExtent, ObstaclePolysToConsider,&PylonsToConsider,FALSE,NAVMESHTRACE_IgnoreLinklessEdges);
	CacheExtent.Z += Extent.Z;
	UNavigationHandle::GetAllPolysFromPos(Mid, CacheExtent, PolysToConsider,FALSE,FALSE,&PylonsToConsider);

	FLOAT DistMoved=0.f;

	if(CurrentPoly == NULL || CurrentPoly->NumObstaclesAffectingThisPoly>0 || !CurrentPoly->ContainsPoint(Location,WORLD_SPACE))
	{
		CurrentPoly = GetAdjacentPolyContainingPoint(CurrentPoly,Location,Extent,FVector(0.f),PolysToConsider);
	}

	if (CurrentPoly==NULL )
	{
		if( bUsingCached )
		{
			UNavigationHandle::GetAllPylonsFromPos(Mid,CacheExtent,PylonsToConsider,FALSE);		
			CacheExtent.Z -= Extent.Z;
			UNavigationHandle::GetAllObstaclePolysFromPos(Mid, Extent+CacheExtent, ObstaclePolysToConsider,&PylonsToConsider,FALSE,NAVMESHTRACE_IgnoreLinklessEdges);
		}
		if( PylonsToConsider.Num() > 0 )
		{
			// try a bigger search
			PolysToConsider.Reset();
			UNavigationHandle::GetAllPolysFromPos(Mid, Extent, PolysToConsider,FALSE,FALSE,&PylonsToConsider);
			CurrentPoly = GetAdjacentPolyContainingPoint(CurrentPoly,Location,Extent,FVector(0.f),PolysToConsider);
			if( CurrentPoly == NULL && PolysToConsider.Num() > 0 )
			{
				CurrentPoly = PolysToConsider(0);
			}
		}
		if( CurrentPoly == NULL )
		{
			// if curpoly is still null, bail!
			//warnf(NAME_Warning,TEXT("WARNING! PHYS_NavMeshWalking could not resolve current poly for %s at (%s), reverting to PHYS_Walking!!"),*GetName(),*Location.ToString());
			setPhysics(PHYS_Walking);
			FVector TestLoc = Location;
			if( GWorld->FindSpot(Extent, TestLoc, bCollideComplex) )
			{
				GWorld->FarMoveActor(this, TestLoc, 0, 0);
			}
			return;
		}
	}

	Handle->AnchorPoly = CurrentPoly;
	Floor = CurrentPoly->GetPolyNormal();
	INT Iterations=0;
	FVector Working_location = Location;

	// PERFORM MOVE
#define MAX_STEP 5.0f

	// list of Actor Touches/Bumps we need to handle after the move is complete
	static TArray<FCheckResult> ActorHits;
	ActorHits.Reset();

	while((DistMoved+0.01f) < DesiredMag && (Iterations++ < 7))
	{
		ProjectedDesired = OldLocation+DesiredMove;
		const FVector HeightOffset = CylinderComponent->CollisionHeight * Floor;
		const FVector Working_Delta = (ProjectedDesired - Location);
		FLOAT NewMag = Working_Delta.Size();
		const FVector Working_Delta_Dir = (NewMag>KINDA_SMALL_NUMBER) ? Working_Delta/NewMag : FVector(0.f);
		NewMag = (ProjectedDesired - Working_location).Size();
		NewMag = Min<FLOAT>(MAX_STEP,NewMag);
		ProjectedDesired = Working_location + Working_Delta_Dir*NewMag;
		const FNavMeshPolyBase* RESTRICT OldPoly = CurrentPoly;
		//const FVector OldPoly_Normal = OldPoly->GetPolyNormal();

		if(NewMag > KINDA_SMALL_NUMBER)
		{			
			CurrentPoly = GetAdjacentPolyContainingPoint(CurrentPoly,ProjectedDesired,GetCylinderExtent(),AccelDir,PolysToConsider);
			check( CurrentPoly == NULL || CurrentPoly->NumObstaclesAffectingThisPoly==0 );
		}

		if( CurrentPoly == NULL )
		{			
			if( PolysToConsider.Num() > 0 )
			{
				// if we couldn't find a poly we are actually in, just pick one since it's nearby and we need something for height adjust
				CurrentPoly = PolysToConsider(0);
			}
		}

		if( CurrentPoly != NULL )
		{
			Handle->AnchorPoly=CurrentPoly;
			Handle->AnchorPylon=CurrentPoly->NavMesh->GetPylon();
			NavMeshWalkingDropToFloor(ProjectedDesired,CurrentPoly,Extent.Z,deltaTime);
		}

#define DEBUG_THIS 0			
		if(OldPoly != CurrentPoly)
		{
			Handle->AnchorPoly = CurrentPoly;
			if(CurrentPoly != NULL)
			{
				Floor = CurrentPoly->GetPolyNormal();
			}
		}

		// TEST destination
		FCheckResult Hit(1.f);
		if(!NavMeshWalkingObstacleCheck(this,Handle,Hit,Working_location,ProjectedDesired,Extent,Floor,ObstaclePolysToConsider,ActorHits))
		{
			// WALL ADJUST
			FVector Delta = ProjectedDesired - Working_location;
			FVector NewDelta = Delta;
			const FVector OldHitNormal = Hit.Normal;

#if DEBUG_THIS// DEBUG Draw
			FlushPersistentDebugLines();
			DrawDebugBox(Hit.Location,Extent,255,0,0,TRUE);
			DrawDebugCoordinateSystem(Hit.Location,Hit.Normal.Rotation(),150.f,TRUE);
			DrawDebugLine(Hit.Location,Hit.Location+Delta.SafeNormal()*50.f,255,255,0,TRUE);

			for(INT PolyIdx=0;PolyIdx<ObstaclePolysToConsider.Num();++PolyIdx)
			{
				FNavMeshPolyBase* CurPoly = ObstaclePolysToConsider(PolyIdx);
				CurPoly->DrawPoly(GWorld->PersistentLineBatcher,FColor::MakeRandomColor());
			}
#endif

			NewDelta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
			if( (NewDelta | Delta) >= 0.f )
			{
				
				if (!NavMeshWalkingObstacleCheck(this,Handle,Hit,Working_location,Working_location+NewDelta,Extent,Floor,ObstaclePolysToConsider,ActorHits))
				{
					//DrawDebugBox(Hit.Location,Extent,255,255,0,TRUE);
					TwoWallAdjust(Delta.SafeNormal(), NewDelta, Hit.Normal, OldHitNormal, Hit.Time);					
					ProjectedDesired = Working_location + NewDelta;
					if ( !NavMeshWalkingObstacleCheck(this,Handle,Hit,Working_location,Working_location+NewDelta,Extent,Floor,ObstaclePolysToConsider,ActorHits))
					{
						ProjectedDesired = Hit.Location;
					}
				}
				else
				{
					ProjectedDesired = Working_location + NewDelta;
				}
			}
			else 
			{
				ProjectedDesired = Hit.Location;
			}
		}
#if DEBUG_THIS
		else
		{
			FlushPersistentDebugLines();
			DrawDebugLine(Hit.Location,Hit.Location+Delta.SafeNormal()*50.f,0,255,0,TRUE);

			for(INT PolyIdx=0;PolyIdx<ObstaclePolysToConsider.Num();++PolyIdx)
			{
				FNavMeshPolyBase* CurPoly = ObstaclePolysToConsider(PolyIdx);
				CurPoly->DrawPoly(GWorld->PersistentLineBatcher,FColor::MakeRandomColor());
			}
		}
#endif

		checkSlow(!ProjectedDesired.ContainsNaN());
		FLOAT ThisDelta = (Working_location - ProjectedDesired).Size();
		DistMoved += ThisDelta;
		Working_location = ProjectedDesired;
		//CurrentPoly->DrawPoly(GWorld->LineBatcher,FColor(255,255,0));
	}

	// if delta was 0, still drop to floor
	if( appIsNearlyZero(DesiredMag) && CurrentPoly != NULL )
	{
		Handle->AnchorPoly=CurrentPoly;
		Handle->AnchorPylon=CurrentPoly->NavMesh->GetPylon();
		NavMeshWalkingDropToFloor(Working_location,CurrentPoly,Extent.Z,deltaTime);
	}

	// Do not call farmoveactor (or setlocation, which calls farmoveactor) because it would trigger redundant collision checks we've already done
	// (for touching actors, etc)
	Location = Working_location;
	ForceUpdateComponents( GWorld->InTick && !GWorld->bPostTickComponentUpdate );
	SetZone(FALSE,FALSE);

	// make sure velocity reflects the actual move
	Velocity = (Location - OldLocation) / deltaTime;

	// process touch/bump events **AFTER** everything else so we don't modify the mesh while we're walking it
	for(INT ActorIdx=0;ActorIdx<ActorHits.Num();++ActorIdx)
	{
		FCheckResult& Hit = ActorHits(ActorIdx);
		if( Hit.Actor && IsBlockedBy(Hit.Actor,Hit.Component) )		
		{
			// Notification that Actor has bumped against the level.
			if( Hit.Actor->bWorldGeometry )
			{
				NotifyBumpLevel(Hit.Location, Hit.Normal);
			}
			// Notify first bumped actor unless it's the level or the actor's base.
			else if( !IsBasedOn(Hit.Actor) )
			{
				// Notify both actors of the bump.
				Hit.Actor->NotifyBump(this, CollisionComponent, Hit.Normal);
				NotifyBump(Hit.Actor, Hit.Component, Hit.Normal);
			}
		}
		if ( (!IsBasedOn(Hit.Actor)) &&
			(!IsBlockedBy(Hit.Actor,Hit.Component)) && this != Hit.Actor)
		{
			BeginTouch(Hit.Actor, Hit.Component, Hit.Location, Hit.Normal, Hit.SourceComponent);
		}
	}

	// UnTouch notifications!
	UnTouchActors();
}

/**
 * Called when this actor is in a level which is being removed from the world (e.g. my level is getting UWorld::RemoveFromWorld called on it)
 */
void APylon::OnRemoveFromWorld()
{
	Super::OnRemoveFromWorld();

	if( NavMeshPtr != NULL )
	{
		NavMeshPtr->OnRemoveFromWorld();
	}
}




