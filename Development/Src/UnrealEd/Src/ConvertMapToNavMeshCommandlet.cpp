/**
* This commandlet will take an existing map that has navigationpoints and create a pylon configuration that 
* spans all the area previously covered by the navpoints
* 
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
#include "UnrealEd.h"
#include "EngineAIClasses.h"

IMPLEMENT_CLASS(UConvertMapToNavMesh);

struct FConvToNavMeshReplaceActorInfo
{
	AActor* WithActor;
	UBOOL	bRefByKismet;
	UBOOL	bRefByRoute;

	FConvToNavMeshReplaceActorInfo()
	{
	}
	FConvToNavMeshReplaceActorInfo( AActor* With, UBOOL bKismet, UBOOL bRoute )
	{
		WithActor = With;
		bRefByKismet = bKismet;
		bRefByRoute = bRoute;
	}
};


typedef TMap<ANavigationPoint*, struct FNavPointGroup*> NavToNavGroupMapType;

// group of navigation points to be replaced by a single pylon
struct FNavPointGroup
{
	FNavPointGroup() : SeedNav(NULL),Center(0.f),Radius(0.f), SeedLocation(0.0f),Outer(NULL)
	{

	}

	void AddNav(ANavigationPoint* Nav, NavToNavGroupMapType& Map);
	void GetRadiusAndCenterAfterAddingNav(ANavigationPoint* Nav, FVector& out_Center, FLOAT& out_Radius);
	void SetSeed(ANavigationPoint* Nav, NavToNavGroupMapType& Map);

	ANavigationPoint* SeedNav;
	TArray<ANavigationPoint*> NavPts;

	ULevel* Outer;
	FVector SeedLocation;
	FVector Center;
	FLOAT   Radius;
};

void FNavPointGroup::SetSeed(ANavigationPoint* Nav, NavToNavGroupMapType& Map)
{
	AddNav(Nav,Map);
	SeedNav = Nav;
	SeedLocation = Nav->Location;
	Outer = Nav->GetLevel();
}

// fits a sphere around this group and the incoming point
void FNavPointGroup::GetRadiusAndCenterAfterAddingNav(ANavigationPoint* Nav,
													  FVector& out_Center,
													  FLOAT& out_Radius)
{
	out_Radius = Radius;
	out_Center = Center;
	// see if this incoming nav is further away from any of our current navs than our current diameter
	for(INT Idx=0;Idx<NavPts.Num();Idx++)
	{
		ANavigationPoint* CurPt = NavPts(Idx);
		FVector Diff = (CurPt->Location - Nav->Location);
		FLOAT CurDist = Diff.Size();
		if ( CurDist > out_Radius * 2.0f )
		{
			out_Radius = CurDist*0.5f+30.0f;
			out_Center = Nav->Location + (Diff * 0.5f);
		}
	}

	// second pass, bump up radius if anyone is outside the sphere
	for(INT Idx=0;Idx<NavPts.Num();Idx++)
	{
		ANavigationPoint* CurPt = NavPts(Idx);
		FVector Diff = (CurPt->Location - Nav->Location);
		FLOAT CurDist = Diff.Size();
		if ( CurDist > out_Radius * 2.0f )
		{
			out_Radius = CurDist*0.5f+30.0f;
		}
	}
}

void FNavPointGroup::AddNav(ANavigationPoint* Nav, NavToNavGroupMapType& Map)
{
	GetRadiusAndCenterAfterAddingNav(Nav,Center,Radius);
	NavPts.AddItem(Nav);
	Map.Set(Nav,this);
}

FLOAT MaxPylonRadius = 7168.0f;
#define MAX_RADIUS MaxPylonRadius
#define MIN_RADIUS 512.0f
// will traverse the navigation network starting with the seed of this group and add all navigation points to the group
// that it can
void BuildNavGroup(FNavPointGroup& Group, NavToNavGroupMapType& Map)
{
	// add seed
	TDoubleLinkedList<ANavigationPoint*> WorkingSet;
	WorkingSet.AddHead(Group.SeedNav);

	// expand out from seed until our radius is > MaxPylonRadius, or we're out of stuff to expand
	while(WorkingSet.Num() > 0)
	{
		// pop head
		ANavigationPoint* CurNode = WorkingSet.GetHead()->GetValue();
		WorkingSet.RemoveNode(WorkingSet.GetHead());

		// expand from this node
		for(INT NeighborIdx=0;NeighborIdx<CurNode->PathList.Num();NeighborIdx++)
		{
			UReachSpec* Spec = CurNode->PathList(NeighborIdx);
			if(Spec == NULL || *Spec->End == NULL)
			{
				continue;
			}
			ANavigationPoint* CurrentNeighbor = Spec->End.Nav();

			// if this node has already been gobbled, skip
			if(Map.HasKey( CurrentNeighbor ))
			{
				continue;
			}

			// if this node is in a different level than the seed, skip
			if(CurrentNeighbor->GetOutermost() != Group.SeedNav->GetOutermost())
			{
				continue;
			}

			// if this node would make radius too big, skip
			FLOAT NewRadius(0.f);
			FVector NewCtr(0.f);
			Group.GetRadiusAndCenterAfterAddingNav(CurrentNeighbor,NewCtr,NewRadius);
			if(NewRadius > MAX_RADIUS)
			{
				continue;
			}

			// otherwise! add it to the group, and the working set
			Group.AddNav(CurrentNeighbor,Map);
			WorkingSet.AddTail(CurrentNeighbor);
		}

	}
}
IMPLEMENT_COMPARE_CONSTREF( FLOAT, GearEditorCommandletsSmallFirst, { return (B - A) < 0 ? 1 : -1; } )

// builds groups without pathing information, just uses proximity
void BuildNavGroupFallback(FNavPointGroup& Group, NavToNavGroupMapType& Map, TDoubleLinkedList<ANavigationPoint*>& Pts)
{
	TMap< ANavigationPoint*, FLOAT > PointDistMap;
	for(TDoubleLinkedList<ANavigationPoint*>::TIterator Itt(Pts.GetHead());Itt;++Itt)
	{
		ANavigationPoint* CurPoint = *Itt;

		if(CurPoint != NULL &&
			CurPoint != Group.SeedNav &&
			CurPoint->GetOutermost() == Group.SeedNav->GetOutermost() && 
			!(Map.HasKey( CurPoint )))
		{
			PointDistMap.Set(CurPoint,(CurPoint->Location - Group.SeedLocation).SizeSquared());
		}
	}

	PointDistMap.ValueSort<COMPARE_CONSTREF_CLASS(FLOAT,GearEditorCommandletsSmallFirst)>();

	TArray<ANavigationPoint*> SortedPts;
	PointDistMap.GenerateKeyArray(SortedPts);

	for(INT Idx=0;Idx<SortedPts.Num();++Idx)
	{
		ANavigationPoint* CurPoint = SortedPts(Idx);

		// if this node would make radius too big, skip
		FLOAT NewRadius(0.f);
		FVector NewCtr(0.f);
		Group.GetRadiusAndCenterAfterAddingNav(CurPoint,NewCtr,NewRadius);
		if(NewRadius < MAX_RADIUS)
		{
			Group.AddNav(CurPoint,Map);
		}
	}

}

void DestroyGroup( FNavPointGroup* Group, NavToNavGroupMapType& Map)
{
	Map.Remove(Group->SeedNav);

	for(INT Idx=0;Idx<Group->NavPts.Num();++Idx)
	{
		Map.Remove(Group->NavPts(Idx));
	}
	delete Group;
}

#define BOUNDS_BUFFER 50.f
INT UConvertMapToNavMesh::Main( const FString& CmdLine )
{
	warnf(TEXT("UConvertMapToNavMesh Commandlet Main..."));

	// Parse command line args
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine( *CmdLine, Tokens, Switches );

	if( Tokens.Num() == 0 )
	{
		warnf(NAME_Error, TEXT("Missing map name argument!"));
		return 0;
	}
	if( Tokens.Num() > 1 )
	{
		warnf(NAME_Error, TEXT("Can only rebuild a single map at a time!"));
		return 0;
	}

	check(Tokens.Num()==1);

	// See whether filename was found in cache
	FFilename Filename;	
	if( !GPackageFileCache->FindPackageFile( *Tokens(0), NULL, Filename ) )
	{
		warnf(NAME_Error, TEXT("Failed to find file name %s"), *Tokens(0) );
		return 0;
	}

	// Load the map file.
	UPackage* Package = UObject::LoadPackage( NULL, *Filename, 0 );
	if( !Package )
	{
		warnf(NAME_Error, TEXT("Error loading '%s'!"), *Filename);
		return 0;
	}

	// Find the world object inside the map file.
	UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
	if( !World )
	{
		warnf(NAME_Error, TEXT("Cannot find world object in '%s'! Is it a map file?"), *Filename);
		return 0;
	}
	// Set loaded world as global world object and add to root set so code doesn't garbage collect it.
	GWorld = World;
	GWorld->AddToRoot();
	GWorld->Init();

	// Mark all sublevels as being visible in the Editor so they get rebuilt.
	for( INT LevelIndex=0; LevelIndex<GWorld->GetWorldInfo()->StreamingLevels.Num(); LevelIndex++ )
	{
		ULevelStreaming* LevelStreamingObject = GWorld->GetWorldInfo()->StreamingLevels(LevelIndex);
		if( LevelStreamingObject )
		{
			LevelStreamingObject->bShouldBeVisibleInEditor = TRUE;
		}
	}
	// Associate sublevels, loading and making them visible.
	GWorld->UpdateLevelStreaming();
	GWorld->FlushLevelStreaming();

	// Check for any PrefabInstances which are out of date.
	GEditor->UpdatePrefabs();

	// Update components for all levels.
	GWorld->UpdateComponents(FALSE);

	// Rebuild BSP.
	GEditor->Exec( TEXT("MAP REBUILD ALLVISIBLE") );
	GEditor->ClearComponents();
	GWorld->CleanupWorld();
	GEditor->UpdateComponents();


	//** End grunt work to load levels **//



	///////////////////////////////  ** PYLON LOCATION SELECTION ** --  Build list of pylons to insert
	//	loop through all navigation points, and create gobble groups to represent new pylon positions
	NavToNavGroupMapType NavGroupMap;
	TArray<FNavPointGroup*> NavPointGroups;
	TArray<APylon*>			Pylons;
	TDoubleLinkedList<ANavigationPoint*> FallbackNodes;
	for( INT LevelIndex=0; LevelIndex<World->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = World->Levels(LevelIndex);
		GWorld->CurrentLevel = Level;
		for( INT ActorIdx = 0; ActorIdx < Level->Actors.Num(); ActorIdx++ )
		{

			ANavigationPoint* Node = Cast<ANavigationPoint>(Level->Actors(ActorIdx));
			if(Node != NULL && !NavGroupMap.HasKey(Node))
			{
				APylon* Pylon = Cast<APylon>(Node);
				if(Pylon != NULL)
				{
					Pylons.AddItem(Pylon);
				}
				else
				{
					FNavPointGroup* NewGroup = new FNavPointGroup();
					NewGroup->SetSeed(Node,NavGroupMap);
					BuildNavGroup(*NewGroup,NavGroupMap);

					if(NewGroup->Radius <= MIN_RADIUS)
					{
						FallbackNodes.AddTail(Node);
						DestroyGroup(NewGroup,NavGroupMap);
						warnf(NAME_Log, TEXT("Could not build navpoint group for node %s,%s, adding to fallback list"),*Node->GetOutermost()->GetName(),*Node->GetName());
					}
					else
					{
						NavPointGroups.AddItem(NewGroup);
						warnf(NAME_Log, TEXT("Group created for Seed %s.%s at %s with %i nodes, and %.2f radius"),*Node->GetOutermost()->GetName(),*Node->GetName(),*Node->Location.ToString(),NewGroup->NavPts.Num(),NewGroup->Radius);
					}

				}
			}
		}
	}

	// *** fallback step to catch nodes which are not in any groups yet

	// first remove anything that's already in a pylon's grasp
	for(TDoubleLinkedList<ANavigationPoint*>::TIterator Itt(FallbackNodes.GetHead());Itt;)
	{
		ANavigationPoint* CurNode = *Itt;
		checkSlowish(CurNode);

		UBOOL bInPylon = FALSE;
		// see if this node is already within a pylon's expansion bounds
		for(INT PylonIdx=0;PylonIdx<Pylons.Num();++PylonIdx)
		{
			APylon* CurPylon = Pylons(PylonIdx);
			if(CurPylon != NULL && CurPylon->IsPtWithinExpansionBounds(CurNode->Location,BOUNDS_BUFFER))
			{
				bInPylon=TRUE;
				break;
			}
		}

		if(bInPylon)
		{
			// remove it from the list 
			TDoubleLinkedList<ANavigationPoint*>::TDoubleLinkedListNode* ToDelete = Itt.GetNode();
			++Itt;
			FallbackNodes.RemoveNode(ToDelete);
		}
		else
		{
			++Itt;
		}
	}

	// now build groups out of the rest

	for(TDoubleLinkedList<ANavigationPoint*>::TIterator Itt(FallbackNodes.GetHead());Itt;++Itt)
	{
		ANavigationPoint* CurNode = *Itt;
		checkSlowish(CurNode);
		if(NavGroupMap.HasKey(CurNode))
		{
			continue;
		}

		FNavPointGroup* NewGroup = new FNavPointGroup();
		NewGroup->SetSeed(CurNode,NavGroupMap);
		BuildNavGroupFallback(*NewGroup,NavGroupMap,FallbackNodes);
		if(NewGroup->Radius < MIN_RADIUS)
		{
			DestroyGroup(NewGroup,NavGroupMap);
		}
		else
		{
			NavPointGroups.AddItem(NewGroup);
		}
	}



	////////////////////////////// ** ROUTE FIXUP **  Gather a list of routes
	TArray<ARoute*> RouteList;
	for( INT LevelIndex=0; LevelIndex<World->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = World->Levels(LevelIndex);
		for( INT ActorIdx = 0; ActorIdx < Level->Actors.Num(); ActorIdx++ )
		{
			ARoute* Route = Cast<ARoute>(Level->Actors(ActorIdx));
			if( Route != NULL )
			{
				RouteList.AddItem( Route );
			}
		}
	}

	// Loop through all ACoverLinks and convert to ACoverHandle
	TMap<AActor*,FConvToNavMeshReplaceActorInfo> ReplacePairs;
	for( INT LevelIndex=0; LevelIndex<World->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = World->Levels(LevelIndex);
		GWorld->CurrentLevel = Level;
		for( INT ActorIdx = 0; ActorIdx < Level->Actors.Num(); ActorIdx++ )
		{
			/*	ACoverLink* Link = Cast<ACoverLink>(Level->Actors(ActorIdx));
			if( Link != NULL )
			{
			// Create new cover handle and copy all necessary info from old link
			ACoverHandle* Handle = Cast<ACoverHandle>(World->SpawnActor(ACoverHandle::StaticClass(), NAME_None, Link->Location, Link->Rotation, NULL, TRUE, FALSE, NULL, NULL, TRUE ));
			check(Handle);
			Handle->CopyFromCoverLink( Link );

			// Find out if link is referenced by any routes
			UBOOL bRefByRoute = FALSE;
			for( INT RouteIdx = 0; RouteIdx < RouteList.Num(); RouteIdx++ )
			{
			ARoute* Route = RouteList(RouteIdx);
			if( Route->HasRefToActor(Link) )
			{
			bRefByRoute = TRUE;
			break;
			}
			}
			UBOOL bRefByKismet = Link->IsReferencedByKismet();

			// If link is referenced by kismet or route, save pairing
			if( bRefByRoute || bRefByKismet )
			{
			ReplacePairs.Set( Link, FConvToNavMeshReplaceActorInfo(Handle, bRefByKismet, bRefByRoute) );
			}
			// Otherwise, destroy the old link
			else
			{
			World->DestroyActor( Link, FALSE, FALSE );
			}
			}
			else*/
			{
				APathNode* Node = Cast<APathNode>(Level->Actors(ActorIdx));
				if( Node != NULL )
				{
					// Find out if link is referenced by any routes
					UBOOL bRefByRoute = FALSE;
					for( INT RouteIdx = 0; RouteIdx < RouteList.Num(); RouteIdx++ )
					{
						ARoute* Route = RouteList(RouteIdx);
						if( Route->HasRefToActor(Node) )
						{
							bRefByRoute = TRUE;
							break;
						}
					}
					UBOOL bRefByKismet = Node->IsReferencedByKismet();

					// If node is referenced by kismet or route, save pairing
					if( bRefByRoute || bRefByKismet )
					{
						// Create a PathTargetPoint to take the place of the pathnode 
						APathTargetPoint* TP = Cast<APathTargetPoint>(World->SpawnActor(APathTargetPoint::StaticClass(), NAME_None, Node->Location, Node->Rotation, NULL, TRUE, FALSE, NULL, NULL, TRUE ));
						check(TP);

						ReplacePairs.Set( Node, FConvToNavMeshReplaceActorInfo(TP, bRefByKismet, bRefByRoute) );
					}
					// Otherwise, destroy the old node
					else
					{
						World->DestroyActor( Node, FALSE, FALSE );
					}
				}
				else
				{
					ANote* Note = Cast<ANote>(Level->Actors(ActorIdx));
					// replace note with pathtargetactor
					if(Note != NULL)
					{

						// Find out if link is referenced by any routes
						UBOOL bRefByRoute = FALSE;
						for( INT RouteIdx = 0; RouteIdx < RouteList.Num(); RouteIdx++ )
						{
							ARoute* Route = RouteList(RouteIdx);
							if( Route->HasRefToActor(Note) )
							{
								bRefByRoute = TRUE;
								break;
							}
						}


						UBOOL bRefByKismet = Note->IsReferencedByKismet();
						if( bRefByRoute || bRefByKismet )
						{
							// replace notes ref'd in kismet with pathtargetpoints
							APathTargetPoint* TP = Cast<APathTargetPoint>(World->SpawnActor(APathTargetPoint::StaticClass(), NAME_None, Note->Location, Note->Rotation, NULL, TRUE, FALSE, NULL, NULL, TRUE ));

							ReplacePairs.Set( Note ,FConvToNavMeshReplaceActorInfo(TP, bRefByKismet, bRefByRoute) );
						}
					}
					else
					{
						// Destroy any markers not placed by LDs
						ANavigationPoint* Nav = Cast<ANavigationPoint>(Level->Actors(ActorIdx));
						if( Nav != NULL && !(Nav->GetClass()->ClassFlags & CLASS_Placeable) )
						{
							World->DestroyActor( Nav );
						}
					}

				}
			}
		}
	}

	// Go thru replacement pairs and fixup kismet, routes, etc
	for( TMap<AActor*, FConvToNavMeshReplaceActorInfo>::TIterator It(ReplacePairs); It; ++It)
	{
		AActor* RepActor = It.Key();
		FConvToNavMeshReplaceActorInfo& Info = It.Value();

		if( Info.bRefByKismet )
		{
			USequence* Sequence = RepActor->GetLevel()->GetGameSequence();
			USequenceObject* pRef = NULL;
			while( Sequence && Sequence->ReferencesObject( RepActor, &pRef ) )
			{
				USeqVar_Object* Var = Cast<USeqVar_Object>(pRef);
				if( Var != NULL )
				{
					Var->ObjValue = Info.WithActor;
				}
			}
		}

		if( Info.bRefByRoute )
		{
			for( INT RouteIdx = 0; RouteIdx < RouteList.Num(); RouteIdx++ )
			{
				ARoute* Route = RouteList(RouteIdx);
				INT Idx = -1;
				while( Route != NULL && Route->HasRefToActor( RepActor, &Idx ) )
				{
					Route->RouteList(Idx) = Info.WithActor;
				}
			}
		}
	}

	// Destroy all replaced actors
	for( TMap<AActor*, FConvToNavMeshReplaceActorInfo>::TIterator It(ReplacePairs); It; ++It)
	{
		AActor* RepActor = It.Key();
		World->DestroyActor( RepActor, FALSE, FALSE );
	}


	// --- create pylons from generated positions	
	for( INT Idx=0;Idx<NavPointGroups.Num();Idx++ )
	{
		FNavPointGroup* Group = NavPointGroups(Idx);
		GWorld->CurrentLevel = Group->Outer;
		APylon* Pylon = Cast<APylon>(GWorld->SpawnActor(APylon::StaticClass(), NAME_None, Group->SeedLocation));
		if( Pylon != NULL )
		{
			Pylon->ExpansionRadius = Group->Radius;
			Pylon->bUseExpansionSphereOverride = TRUE;
			Pylon->ExpansionSphereCenter = Group->Center;
		}
	}

	// List all navigation points that still exist...
	warnf(NAME_Log, TEXT("List Remaining NavPoints..."));
	for( INT LevelIndex=0; LevelIndex<World->Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = World->Levels(LevelIndex);
		for( INT ActorIdx = 0; ActorIdx < Level->Actors.Num(); ActorIdx++ )
		{
			ANavigationPoint* Nav = Cast<ANavigationPoint>(Level->Actors(ActorIdx));
			if( Nav != NULL )
			{
				warnf(NAME_Log, TEXT("\t%s"), *Nav->GetFullName() );
			}
		}
	}

	DOUBLE PathingStartTime = appSeconds();
	FPathBuilder::Exec( TEXT("PREDEFINEPATHS") );
	FPathBuilder::Exec( TEXT("GENERATENAVMESH SHOWMAPCHECK=0") );
	FPathBuilder::Exec( TEXT("BUILDCOVER FROMDEFINEPATHS=1") );
	FPathBuilder::Exec( TEXT("POSTDEFINEPATHS") );
	FPathBuilder::Exec( TEXT("FINISHPATHBUILD") );
	warnf(TEXT("Building paths took    %5.2f minutes."), (appSeconds() - PathingStartTime) / 60 );


	UObject::CollectGarbage(RF_Native);

	// Iterate over all levels in the world and save them.
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel*			Level			= GWorld->Levels(LevelIndex);
		check(Level);					
		UWorld*			World			= CastChecked<UWorld>(Level->GetOuter());
		ULinkerLoad*	Linker			= World->GetLinker();
		check(Linker);
		FString			LevelFilename	= Linker->Filename;

		// Save sublevel.
		UObject::SavePackage( World->GetOutermost(), World, 0, *LevelFilename, GWarn );
	}

	// Remove from root again.
	GWorld->RemoveFromRoot();

	warnf(NAME_Log, TEXT("Completed conversion on %s"), *Filename);
	return 0;
}
