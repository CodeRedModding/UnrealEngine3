/*=============================================================================
	UnPath.cpp: Unreal pathnode placement
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"
#include "UnTerrain.h"
#include "EngineAIClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "DebugRenderSceneProxy.h"

IMPLEMENT_CLASS(APathBlockingVolume);
IMPLEMENT_CLASS(ARoute);

const FOctreeNodeBounds FNavigationOctree::RootNodeBounds(FVector(0,0,0),HALF_WORLD_MAX);
/** maximum objects we can have in one node before we split it */
#define MAX_OBJECTS_PER_NODE 10


void ANavigationPoint::TogglePathRendering(UBOOL bShouldDrawPaths)
{
	UBOOL bHasComponent = FALSE;
	for (INT Idx = 0; Idx < Components.Num(); Idx++)
	{
		UPathRenderingComponent *PathRenderer = Cast<UPathRenderingComponent>(Components(Idx));
		if (PathRenderer != NULL)
		{
			bHasComponent = TRUE;
			PathRenderer->SetHiddenGame(!bShouldDrawPaths);
			break;
		}
	}
	if (!bHasComponent)
	{
		UPathRenderingComponent *PathRenderer = ConstructObject<UPathRenderingComponent>(UPathRenderingComponent::StaticClass(),this);
		PathRenderer->SetHiddenGame(!bShouldDrawPaths);
		AttachComponent(PathRenderer);
	}
}

/**
 * Removes paths from all navigation points in the world, and removes all
 * path markers from actors in the world.
 */
#if WITH_EDITOR
void AScout::UndefinePaths()
{
	debugfSuppressed(NAME_DevPath,TEXT("Remove old reachspecs"));
	GWarn->BeginSlowTask( TEXT("Undefining Paths"), FALSE);
	//clear NavigationPoints
	const INT ProgressDenominator = FActorIteratorBase::GetProgressDenominator();
	const INT UpdateGranularity = ProgressDenominator / 5;
	INT LastUpdate =0;
	GWarn->StatusUpdatef( 0, ProgressDenominator, *LocalizeUnrealEd(TEXT("Undefining")) );
	for( FActorIterator It; It; ++It )
	{
		if(It.GetProgressNumerator() >= LastUpdate + UpdateGranularity)
		{
			GWarn->UpdateProgress( It.GetProgressNumerator(), ProgressDenominator );
			LastUpdate=It.GetProgressNumerator();
		}

		AActor*	Actor = *It;
		ANavigationPoint *Nav = Cast<ANavigationPoint>(Actor);
		if (Nav != NULL)
		{
			APylon* Pylon = Cast<APylon>(Actor);
			if (!(Nav->GetClass()->ClassFlags & CLASS_Placeable))
			{
				/* delete any nodes which aren't placeable, because they were automatically generated,
				  and will be automatically generated again. */
				GWorld->DestroyActor(Nav);
			}
			else
			{
				// reset the nav network id for this nav
				Nav->NetworkID = -1;
				// and clear any previous paths
				Nav->ClearPaths();
			}
		}
		else
		{
			Actor->ClearMarker();
		}
	}
	if ( GWorld->GetWorldInfo()->bPathsRebuilt )
	{
		debugf(TEXT("undefinepaths Clear paths rebuilt"));
	}

	GWorld->NavigationOctree->RemoveAllObjects();
	GWorld->GetWorldInfo()->bPathsRebuilt = FALSE;
	GWarn->EndSlowTask();
}

void AScout::ReviewPaths()
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ReviewingPathsE")), FALSE );

	if ( !GWorld->GetFirstNavigationPoint() )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, NULL, *FString( LocalizeUnrealEd( "MapCheck_Message_NoNavigationPoints" ) ), TEXT( "NoNavigationPoints" ) );
	}
	else
	{
		INT NumDone = 0;
		INT NumPaths = 0;
		INT NumStarts = 0;
		for ( ANavigationPoint* N=GWorld->GetFirstNavigationPoint(); N!=NULL; N=N->nextNavigationPoint )
		{
			if ( Cast<APlayerStart>(N) )
			{
				NumStarts++;
			}
			NumPaths++;
		}
		
		if ( NumStarts < MinNumPlayerStarts )
		{
			GWarn->MapCheck_Add( MCTYPE_ERROR, GWorld->GetFirstNavigationPoint(), *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NeedMorePlayerstarts" ), NumStarts, MinNumPlayerStarts ) ), TEXT( "NeedMorePlayerstarts" ) );
		}

		SetPathCollision(TRUE);
		GWarn->StatusUpdatef( 0, NumPaths, *LocalizeUnrealEd(TEXT("ReviewingPaths")) );
		for ( ANavigationPoint* N=GWorld->GetFirstNavigationPoint(); N!=NULL; N=N->nextNavigationPoint )
		{
			N->ReviewPath(this);
			GWarn->UpdateProgress( NumDone++, NumPaths );
		}
		SetPathCollision(FALSE);
	}

	GWarn->EndSlowTask();
}

void AActor::SetCollisionForPathBuilding(UBOOL bNowPathBuilding)
{
	if (bNowPathBuilding)
	{
		// turn off collision - for non-static actors with bPathColliding false and blocking volumes with no player collision
		if (!bDeleteMe)
		{
			if ( bBlockActors && bCollideActors &&
				(!bStatic && !bPathColliding))
			{
				bPathTemp = TRUE;
				SetCollision(FALSE, bBlockActors, bIgnoreEncroachers);
			}
			else
			{
				bPathTemp = FALSE;
			}
		}
	}
	else if( bPathTemp )
	{
		bPathTemp = FALSE;
		SetCollision(TRUE, bBlockActors, bIgnoreEncroachers);
	}
}

void AStaticMeshActor::SetCollisionForPathBuilding(UBOOL bNowPathBuilding)
{
	if( StaticMeshComponent && StaticMeshComponent->StaticMesh && StaticMeshComponent->StaticMesh->bCanBecomeDynamic )
	{
		if ( bNowPathBuilding )
		{
			bPathTemp = TRUE;
			SetCollision(FALSE, bBlockActors, bIgnoreEncroachers);
		}
		else if(bPathTemp)
		{
			bPathTemp = FALSE;
			SetCollision(TRUE, bBlockActors, bIgnoreEncroachers);
		}
	}
	else
	{
		Super::SetCollisionForPathBuilding(bNowPathBuilding);
	}
}
void APathBlockingVolume::SetCollisionForPathBuilding(UBOOL bNowPathBuilding)
{
	if ( bNowPathBuilding )
	{
		bPathTemp = TRUE;
		// turn ON collision when path building
		SetCollision(TRUE, bBlockActors, bIgnoreEncroachers);
	}
	else if ( bPathTemp )
	{
		bPathTemp = FALSE;
		// turn OFF collision when not path building
		SetCollision(FALSE, bBlockActors, bIgnoreEncroachers);		
	}
}

void AScout::SetPathCollision(UBOOL bEnabled)
{
	for( FActorIterator It; It; ++It )
	{
		if( !FPathBuilder::IsBuildingPaths() || FPathBuilder::IsPtWithinPathBuildBounds(It->Location) )
		{
			It->SetCollisionForPathBuilding(bEnabled);
		}
	}
}

INT AScout::PrunePathsForNav(ANavigationPoint* Nav)
{
	// default is just a pass through
	return Nav->PrunePaths();
}

void AScout::PrunePaths(INT NumPaths)
{
	// first, check for unnecessary PathNodes
	INT NumDone = 0;
	GWarn->StatusUpdatef( NumDone, NumPaths, *LocalizeUnrealEd(TEXT("CheckingForUnnecessaryPathNodes")) );
	for (ANavigationPoint* Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
	{
		GWarn->UpdateProgress( NumDone, NumPaths );

		if (Nav->GetClass() == APathNode::StaticClass() && Nav->PathList.Num() > 0)
		{
			// if the PathNode is connected only to other nodes that have paths that encompass it, then it is unnecessary
			UBOOL bNecessary = TRUE;
			for (INT i = 0; i < Nav->PathList.Num(); i++)
			{
				// nodes with forced paths are always necessary
				if (Nav->PathList(i)->IsA(UForcedReachSpec::StaticClass()))
				{
					bNecessary = TRUE;
					break;
				}
				// if Nav is not designed for flying Pawns, ignore end nodes that are
				if ( *Nav->PathList(i)->End != NULL && (Nav->bFlyingPreferred || !Nav->PathList(i)->End.Nav()->bFlyingPreferred) &&
						(Nav->PhysicsVolume == NULL || Nav->PathList(i)->End->PhysicsVolume == NULL || Nav->PhysicsVolume->bWaterVolume == Nav->PathList(i)->End->PhysicsVolume->bWaterVolume) )
				{
					bNecessary = FALSE;
					UBOOL bOverlapsNav = FALSE;
					for (INT j = 0; j < Nav->PathList(i)->End.Nav()->PathList.Num(); j++)
					{
						UReachSpec* Spec = Nav->PathList(i)->End.Nav()->PathList(j);
						if (Spec != NULL && *Spec->End != NULL)
						{
							if (Spec->bAddToNavigationOctree && Spec->End != Nav && Spec->IsOnPath(Nav->Location, Nav->CylinderComponent->CollisionRadius))
							{
								bOverlapsNav = TRUE;
								break;
							}
						}
					}

					if (!bOverlapsNav)
					{
						bNecessary = TRUE;
						break;
					}
				}
			}
			if (!bNecessary)
			{
				// second pass: check that there are no special one-way connections that require this node
				for (ANavigationPoint* SecondNav = GWorld->GetFirstNavigationPoint(); SecondNav != NULL; SecondNav = SecondNav->nextNavigationPoint)
				{
					if (Nav->bFlyingPreferred || !SecondNav->bFlyingPreferred)
					{
						UReachSpec* Spec = SecondNav->GetReachSpecTo(Nav);
						if (Spec != NULL && !Spec->bAddToNavigationOctree && Nav->GetReachSpecTo(SecondNav) == NULL)
						{
							bNecessary = TRUE;
							break;
						}
					}
				}
				if (!bNecessary)
				{
					// consider as necessary if it's part of any scripted paths
					for (FActorIterator It; It; ++It)
					{
						ARoute* Route = Cast<ARoute>(*It);
						if (Route != NULL)
						{
							for (INT i = 0; i < Route->RouteList.Num(); i++)
							{
								if (Route->RouteList(i).Nav() == Nav)
								{
									bNecessary = TRUE;
									break;
								}
							}
						}
					}
					if (!bNecessary)
					{
						GWarn->MapCheck_Add( MCTYPE_WARNING, Nav, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_UselessPathNode" ), *Nav->GetName() ) ), TEXT( "UselessPathNode" ) );
					}
				}
			}
		}
		NumDone++;
	}

	// prune excess reachspecs
	debugfSuppressed(NAME_DevPath,TEXT("Prune reachspecs"));
	INT NumPruned = 0;
	NumDone = 0;
	GWarn->StatusUpdatef( NumDone, NumPaths, *LocalizeUnrealEd(TEXT("PruningReachspecs")) );
	for (ANavigationPoint* Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
	{
		GWarn->UpdateProgress( NumDone, NumPaths );
		NumPruned += PrunePathsForNav(Nav);
		NumDone++;
	}

	// second prune pass
	NumDone =0;
	for (ANavigationPoint* Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint)
	{
		GWarn->UpdateProgress( NumDone, NumPaths );
		NumPruned += SecondPassPrunePathsForNav(Nav);
		NumDone++;
	}
	debugfSuppressed(NAME_DevPath,TEXT("Pruned %d reachspecs"), NumPruned);
}

void AScout::UpdateInterpActors(UBOOL &bProblemsMoving, TArray<USeqAct_Interp*> &InterpActs)
{
#if WITH_EDITORONLY_DATA
	bProblemsMoving = FALSE;
	// Keep track of which actors we are updating - so we can check for multiple actions updating the same actor.
	TArray<AActor*> MovedActors;
	for (TObjectIterator<USeqAct_Interp> InterpIt; InterpIt; ++InterpIt)
	{
		// For each SeqAct_Interp we flagged as having to be updated for pathing...
		USeqAct_Interp* Interp = *InterpIt;
		if(Interp->bInterpForPathBuilding)
		{
			InterpActs.AddItem(Interp);
			// Initialise it..
			Interp->InitInterp();
			for(INT j=0; j<Interp->GroupInst.Num(); j++)
			{
				// Check if any of the actors its working on have already been affected..
				AActor* InterpActor = Interp->GroupInst(j)->GroupActor;
				if(InterpActor)
				{
					if( MovedActors.ContainsItem(InterpActor) )
					{
						// This Actor is being manipulated by another SeqAct_Interp - so bail out.
						appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_MultipleInterpsReferToSameActor"), *InterpActor->GetName()));
						appMsgf(AMT_OK,TEXT("The full name of the interp actor which can't be interp'd is: %s.%s"),*InterpActor->GetOutermost()->GetName(),*InterpActor->GetName());
						bProblemsMoving = true;
					}
					else
					{
						// Things are ok - just add it to the list.
						MovedActors.AddItem(InterpActor);
					}
				}
			}

			if(!bProblemsMoving)
			{
				// If there were no problem initialising, first backup the current state of the actors..
				for(INT i=0; i<Interp->GroupInst.Num(); i++)
				{
					Interp->GroupInst(i)->SaveGroupActorState();
					AActor* GroupActor = Interp->GroupInst(i)->GetGroupActor();
					if( GroupActor )
					{
						const UBOOL bOnlyCaptureChildren = FALSE;
						Interp->SaveActorTransforms( GroupActor, bOnlyCaptureChildren );
					}
				}

				// ..then interpolate them to the 'PathBuildTime'.
				Interp->UpdateInterp( Interp->InterpData->PathBuildTime, true );
			}
			else
			{
				// If there were problems, term this interp now (so we won't try and restore its state later on).
				Interp->TermInterp();
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void AScout::RestoreInterpActors(TArray<USeqAct_Interp*> &InterpActs)
{
	// Reset any interpolated actors.
	const FString LocalizeBuildPathsResettingActors( LocalizeUnrealEd(TEXT("BuildPathsResettingActors")) );
	for(INT i=0; i<InterpActs.Num(); i++)
	{
		GWarn->StatusUpdatef( i, InterpActs.Num(), *LocalizeBuildPathsResettingActors );

		USeqAct_Interp* Interp = CastChecked<USeqAct_Interp>( InterpActs(i) );

		if(Interp->bInterpForPathBuilding)
		{
			for(INT j=0; j<Interp->GroupInst.Num(); j++)
			{
				Interp->GroupInst(j)->RestoreGroupActorState();
			}

			Interp->RestoreActorVisibilities();
			Interp->RestoreActorTransforms();
			Interp->TermInterp();
			Interp->Position = 0.f;
		}
	}
}

void AScout::BuildNavLists()
{
	// build the per-level nav lists
	UBOOL bBuildCancelled = GEngine->GetMapBuildCancelled();
	if (!bBuildCancelled)
	{
		TArray<FGuid> NavGuids;
		// reset the world's nav list
		GWorld->ResetNavList();
		// for each level,
		GWarn->StatusUpdatef( 0, GWorld->Levels.Num(), *LocalizeUnrealEd(TEXT("BuildPathsNavigationPointsOnBases")) );
		for ( INT LevelIdx = 0; LevelIdx < GWorld->Levels.Num(); LevelIdx++ )
		{
			ULevel *Level = GWorld->Levels(LevelIdx);
			if (Level != NULL)
			{
				Level->ResetNavList();
				UBOOL bHasPathNodes = FALSE;
				// look for any path nodes in this level
				for (INT ActorIdx = 0; ActorIdx < Level->Actors.Num() && !bBuildCancelled; ActorIdx++)
				{
					ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(ActorIdx));
					if (Nav != NULL && !Nav->IsPendingKill())
					{
						// note that the level does have pathnodes
						bHasPathNodes = TRUE;
						// check for invalid cylinders
						if (Nav->CylinderComponent == NULL)
						{
							GWarn->MapCheck_Add( MCTYPE_WARNING, Nav, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_CylinderComponentNull" ), *Nav->GetName() ) ), TEXT( "CylinderComponentNull" ) );
						}
						else
						{
							// add to the level path list
							Level->AddToNavList(Nav);
							// base the nav points as well
							Nav->FindBase();
							// init the nav for pathfinding
							Nav->InitForPathFinding();
							Nav->bHasCrossLevelPaths = FALSE;
							// and generate a guid for it as well
							if (!Nav->GetGuid()->IsValid() || NavGuids.ContainsItem(*Nav->GetGuid()))
							{
								Nav->NavGuid = appCreateGuid();
							}
							else
							{
								// save the existing guid to check for duplicates
								NavGuids.AddItem(*Nav->GetGuid());
							}
						}
					}

					// Init guids for all Path Object Interface actors
					IInterface_NavMeshPathObject* PO = InterfaceCast<IInterface_NavMeshPathObject>(Level->Actors(ActorIdx));
					if( PO != NULL )
					{
						PO->InitGuid( NavGuids );
					}

					bBuildCancelled = GEngine->GetMapBuildCancelled();
				}
				GWarn->UpdateProgress( LevelIdx, GWorld->Levels.Num() );
				// if there are any nodes, 
				if (bHasPathNodes)
				{
					// mark the level as dirty so that it will be saved
					Level->MarkPackageDirty();
					// and add to the world nav list
					GWorld->AddLevelNavList(Level);
				}
			}
		}
	}
}

/**
 * Clears all the paths and rebuilds them.
 *
 * @param	bReviewPaths	If TRUE, review paths if any were created.
 * @param	bShowMapCheck	If TRUE, conditionally show the Map Check dialog.
 * @param	bUndefinePaths	IF TRUE, paths will be undefined first
 */
void AScout::DefinePaths(UBOOL bReviewPaths, UBOOL bShowMapCheck, UBOOL bUndefinePaths)
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("DefiningPaths")), FALSE );

	// Rest map has pathing errors warning.
	GWorld->GetWorldInfo()->bMapHasPathingErrors = FALSE;

	// Build Terrain Collision Data
	for (TObjectIterator<UTerrainComponent> TerrainIt; TerrainIt; ++TerrainIt)
	{
		TerrainIt->BuildCollisionData();
	}

	// remove old paths
	if (bUndefinePaths)
	{
		UndefinePaths();
	}

	// Position interpolated actors in desired locations for path-building.
	TArray<USeqAct_Interp*> InterpActs;
	UBOOL bProblemsMoving = FALSE;
	UpdateInterpActors(bProblemsMoving,InterpActs);
	UBOOL bBuildCancelled = GEngine->GetMapBuildCancelled();
	if (!bProblemsMoving && !bBuildCancelled)
	{
		// Setup all actor collision for path building
		SetPathCollision(TRUE);
		// number of paths, used for progress bar
		INT NumPaths = 0; 
		// Add NavigationPoint markers to any actors which want to be marked
		INT ProgressDenominator = FActorIteratorBase::GetProgressDenominator();
		SetCollision( FALSE, FALSE, bIgnoreEncroachers );
		GWarn->StatusUpdatef( 0, ProgressDenominator, *LocalizeUnrealEd(TEXT("BuildPathsDefining")) );
		for( FActorIterator It; It && !bBuildCancelled; ++It )
		{
			GWarn->UpdateProgress(It.GetProgressNumerator(), ProgressDenominator);
			AActor *Actor = *It;
			NumPaths += Actor->AddMyMarker(this);

			bBuildCancelled = GEngine->GetMapBuildCancelled();
		}
		// build the navigation lists
		BuildNavLists();
		// setup the scout
		SetCollision(TRUE, TRUE, bIgnoreEncroachers);

		// Adjust cover
		if( !bBuildCancelled )
		{
			//@note - needs to happen after addmymarker in case newly created cover was added
			Exec( TEXT("ADJUSTCOVER FROMDEFINEPATHS=1") );
		}

		// calculate and add reachspecs to pathnodes
		debugfSuppressed(NAME_DevPath,TEXT("Add reachspecs"));
		INT NumDone = 0;
		GWarn->StatusUpdatef( 0, NumPaths, *LocalizeUnrealEd(TEXT("BuildPathsAddingReachspecs")) );
		for( ANavigationPoint *Nav=GWorld->GetFirstNavigationPoint(); Nav && !bBuildCancelled; Nav=Nav->nextNavigationPoint )
		{
			Nav->addReachSpecs(this,FALSE);
			GWarn->UpdateProgress(NumDone++, NumPaths);
			debugfSuppressed( NAME_DevPath, TEXT("Added reachspecs to %s"),*Nav->GetName() );
			bBuildCancelled = GEngine->GetMapBuildCancelled();
		}

		// Called to eliminate paths so computing long paths 
		// doesn't bother iterating over specs that will be deleted anyway
		if( !bBuildCancelled )
		{
			PrunePaths(NumPaths);
		}

		Exec( *FString::Printf(TEXT("ADDLONGREACHSPECS NUMPATHS=%d"), NumPaths) );

		// allow scout to add any game specific special reachspecs
		if (!bBuildCancelled)
		{
			debugf(TEXT("%s add special paths"),*GetName());
			AddSpecialPaths(NumPaths, FALSE);
		}
		// turn off collision and reset temporarily changed actors
		SetPathCollision( FALSE );

		// Add forced specs if needed
		GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("AddingForcedSpecs")), FALSE );
		{
			INT NavigationPointCount = 0;
			for( ANavigationPoint* Nav = GWorld->GetFirstNavigationPoint(); Nav && !bBuildCancelled; Nav = Nav->nextNavigationPoint )
			{
				Nav->AddForcedSpecs( this );

				GWarn->StatusUpdatef(NavigationPointCount, NumPaths, LocalizeSecure(LocalizeUnrealEd(TEXT("BuildPathsAddingForcedSpecsFE")), NavigationPointCount));

				bBuildCancelled = GEngine->GetMapBuildCancelled();
				NavigationPointCount++;
			}

			for( ANavigationPoint* Nav = GWorld->GetFirstNavigationPoint(); Nav && !bBuildCancelled; Nav = Nav->nextNavigationPoint )
			{
				// Call prune paths so MaxPathSize can be updated
				PrunePathsForNav(Nav);
				bBuildCancelled = GEngine->GetMapBuildCancelled();
			}
		}
		GWarn->EndSlowTask();

		if( !bBuildCancelled )
		{
			INT NavigationPointCount = 0;

			// sort PathLists
			// clear pathschanged flags and remove bases if in other level
			// handle specs crossing levels
			for( ANavigationPoint *Nav=GWorld->GetFirstNavigationPoint(); Nav && !bBuildCancelled; Nav=Nav->nextNavigationPoint )
			{
				Nav->SortPathList();
				Nav->bPathsChanged = FALSE;
				GWarn->StatusUpdatef( NavigationPointCount, NumPaths, LocalizeSecure(LocalizeUnrealEd(TEXT("BuildPathsCheckingCrossLevelReachspecsFE")), NavigationPointCount) );
				bBuildCancelled = GEngine->GetMapBuildCancelled();
				NavigationPointCount++;
			}

			if (!bBuildCancelled)
			{
				GWorld->GetWorldInfo()->bPathsRebuilt = TRUE;
				debugf(TEXT("SET paths rebuilt"));
			}
		}
	}
	// reset the interp actors moved for path building
	RestoreInterpActors(InterpActs);

	GWarn->EndSlowTask();

	// if the build was cancelled then invalidate paths
	if( bBuildCancelled )
	{
		UndefinePaths();
	}
	else
	{
		debugfSuppressed(NAME_DevPath,TEXT("All done"));
		if ( bReviewPaths && GWorld->GetFirstNavigationPoint() )
		{
			ReviewPaths();
		}
		if ( bShowMapCheck )
		{
			GWarn->MapCheck_ShowConditionally();
		}
	}
}

/** 
 * inserts an appropriate slot at the given index
 * extra code here to copy over manually assigned properties from adjacent slots to this one
 */
void InsertHomogeneousSlotToLink(const FVector& SlotLoc, ACoverLink* Link, INT InsertIdx, AScout* Scout)
{
	INT PrevIdx = InsertIdx-1;
	INT NextIdx = InsertIdx+1;
	

	
	INT NewIdx = Link->AddCoverSlot(SlotLoc, Link->GetSlotRotation(PrevIdx),InsertIdx,FALSE,Scout);					
	check(NewIdx == InsertIdx);
	check( PrevIdx >= 0 && NextIdx < Link->Slots.Num() );

	FCoverSlot& PrevSlot = Link->Slots(PrevIdx);
	FCoverSlot& NextSlot = Link->Slots(NextIdx);
	FCoverSlot& NewSlot = Link->Slots(NewIdx);

	BYTE ForcedCoverType = CT_None;
	if( PrevSlot.ForceCoverType != CT_None )
	{
		ForcedCoverType = PrevSlot.ForceCoverType;
	}

	if( NextSlot.ForceCoverType != ForcedCoverType )
	{
		if( NextSlot.ForceCoverType == CT_MidLevel || ForcedCoverType == CT_None )
		{
			ForcedCoverType = NextSlot.ForceCoverType;
		}
	}
	NewSlot.bForceCanPopUp = PrevSlot.bForceCanPopUp || PrevSlot.bForceCanPopUp;
	NewSlot.bEnabled = PrevSlot.bEnabled && PrevSlot.bEnabled;
	NewSlot.bAllowPopup = PrevSlot.bAllowPopup && PrevSlot.bAllowPopup;
	NewSlot.bAllowMantle = PrevSlot.bAllowMantle && PrevSlot.bAllowMantle;
	NewSlot.bAllowCoverSlip = PrevSlot.bAllowCoverSlip && PrevSlot.bAllowCoverSlip;
	NewSlot.bAllowClimbUp = PrevSlot.bAllowClimbUp && PrevSlot.bAllowClimbUp;
	NewSlot.bAllowSwatTurn = PrevSlot.bAllowSwatTurn && PrevSlot.bAllowSwatTurn;
	NewSlot.bForceNoGroundAdjust = PrevSlot.bForceNoGroundAdjust || PrevSlot.bForceNoGroundAdjust;
	NewSlot.bPlayerOnly = PrevSlot.bPlayerOnly || PrevSlot.bPlayerOnly;

	if( PrevSlot.LocationDescription != CoverDesc_None ) 
	{
		NewSlot.LocationDescription = PrevSlot.LocationDescription;
	}
	else 
	if( NextSlot.LocationDescription != CoverDesc_None )
	{
		NewSlot.LocationDescription = NextSlot.LocationDescription;
	}
}

/**
 * enforces a max distance between cover slots to ensure proper density
 * @param Link - link we're fixing up density for
 * @param Scout - scout ref
 * @return - number of slots we've added
 */
INT FixupSlotDensity(ACoverLink* Link, AScout* Scout)
{
	static UBOOL bSkipAll = FALSE;
	// if this has been disabled by the LD just bail now
	if( bSkipAll || ! Link->bDoAutoSlotDensityFixup )
	{
		return 0;
	}

	INT AddedSlotsThisLink = 0;

	// see if the link has any slots that failed to align to a surface
	UBOOL bLinkSlotFailedToFindSurface = FALSE;
	for ( INT SlotIndex = 0; SlotIndex < Link->Slots.Num(); ++SlotIndex )
	{
		if ( Link->Slots(SlotIndex).bFailedToFindSurface )
		{
			bLinkSlotFailedToFindSurface = TRUE;
			break;
		}
	}
	if( Link->Slots.Num() > 50 || bLinkSlotFailedToFindSurface )
	{
		warnf(NAME_Warning, TEXT("%s.%s has too many slots, or did not align properly.. interval enforcement is being skipped!"), *Link->GetOutermost()->GetName(), *Link->GetName());
	}
	else
	{

		// verify that there are slots at the proper interval 
		FVector PrevLoc = Link->GetSlotLocation(0);
		for (INT SlotIdx = 1; SlotIdx < Link->Slots.Num(); ++SlotIdx)
		{
			FVector Curr_Loc = Link->GetSlotLocation(SlotIdx);

			FVector Delta = (Curr_Loc-PrevLoc);
			FLOAT Dist = Delta.Size();

			// big vertical gaps probably indicate a slot is floating in space
			if( Abs<FLOAT>(Delta.Z) > 5.0f * Scout->NavMeshGen_EntityHalfHeight )
			{
				continue;
			}

			// too much of a gap, add a new slot!
			if ( Dist > Link->AutoCoverSlotInterval )
			{
				FVector Dir = Delta/Dist;

				// split space up into an even # of segments 				
				INT NumSegmentsToAdd = appFloor(Dist / Link->AutoCoverSlotInterval);
				FLOAT Offset = Dist / (NumSegmentsToAdd+1);
				FVector NewLoc(0.f);
				for( INT SegAddNum=1;SegAddNum <= NumSegmentsToAdd; ++SegAddNum )
				{
					NewLoc = PrevLoc + Dir * Offset * SegAddNum;
					InsertHomogeneousSlotToLink(NewLoc,Link,SlotIdx+SegAddNum-1,Scout);
					if( ++AddedSlotsThisLink > 50 )
					{
						warnf(NAME_Warning,TEXT("%s.%s - Slot spacing enforcement has added way too many slots, something has gone wrong!"), *Link->GetOutermost()->GetName(), *Link->GetName());
						break;
					}
				}
				SlotIdx = SlotIdx+NumSegmentsToAdd-1;
				PrevLoc = NewLoc;
			}
			else
			{
				PrevLoc = Curr_Loc;
			}

			if( AddedSlotsThisLink > 50 )
			{
				break;
			}
		}

		// after this has been run once on the slot, disable it so there is no chance of it affecting things later on
		Link->bDoAutoSlotDensityFixup=FALSE;
	}

	return AddedSlotsThisLink;
}
/** 
*	Called before addReachSpecs so that slot locations can be updated for proper placement 
*	of the slot markers, since slot location will be nudged around
*/
void AScout::AdjustCover( UBOOL bFromDefinePaths )
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("AdjustCover")), FALSE);

	INT NumCover = 0;
	INT NumDone  = 0;
	TArray<ACoverLink*> Links;
	UBOOL bBuildCancelled = GEngine->GetMapBuildCancelled();

	INT TotalSlotsBeforeAdd = 0;
	AWorldInfo *Info = GWorld->GetWorldInfo();
	for( ACoverLink *Link = Info->CoverList; Link != NULL && !bBuildCancelled; Link = Link->NextCoverLink )
	{
		if( !FPathBuilder::IsBuildingPaths() || FPathBuilder::IsPtWithinPathBuildBounds(Link->Location) )
		{
			// perform initial order independent setup
			Link->SortSlots();
			NumCover++;
			TotalSlotsBeforeAdd+=Link->Slots.Num();
		}
	}

	// Initial alignment/orientation pass
	
	INT AddedSlots = 0;
	
	const FString LocalizeBuildCoverAdjustingCoverSlots( LocalizeUnrealEd(TEXT("BuildCoverAdjustingCoverSlots")) );
	for (ACoverLink *Link = Info->CoverList; Link != NULL && !bBuildCancelled; Link = Link->NextCoverLink)
	{
			
		if( !FPathBuilder::IsBuildingPaths() || FPathBuilder::IsPtWithinPathBuildBounds(Link->Location) )
		{

			// fixup slot density
			AddedSlots += FixupSlotDensity(Link,this);

			//GWarn->UpdateProgress(NumDone++, NumCover);
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				Link->AutoAdjustSlot( SlotIdx, FALSE );
			}
		}
		bBuildCancelled = GEngine->GetMapBuildCancelled();
	}

	debugf(TEXT("Added %i slots due to spacing! (had %i before adds) (%i added bytes)"), AddedSlots, TotalSlotsBeforeAdd, sizeof(FCoverSlot) * AddedSlots);

	GWarn->EndSlowTask();
}

/** 
*	Called, after reach specs have been added, to generate fire links and special move flags
*/
void AScout::BuildCover( UBOOL bFromDefinePaths )
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("BuildingCover")), FALSE);

	DOUBLE BeginTime = appSeconds();
	INT NumCover = 0;
	INT NumDone  = 0;
	TArray<ACoverLink*> Links;
	UBOOL bBuildCancelled = GEngine->GetMapBuildCancelled();

	// disable collision for the scout so we don't accidentally interfere with linechecks
	SetCollision(FALSE,FALSE,bIgnoreEncroachers);

	for( INT LevelIdx = 0; LevelIdx < GWorld->Levels.Num(); LevelIdx++ )
	{
		ULevel* Level = GWorld->Levels(LevelIdx);
		Level->PurgeCrossLevelCoverArrays();
	}

	if( !bFromDefinePaths )
	{
		// setup the nav lists before building since they are most likely outdated
		BuildNavLists();
		SetPathCollision(TRUE);
	}

	AWorldInfo *Info = GWorld->GetWorldInfo();
	for (ACoverLink *Link = Info->CoverList; Link != NULL && !bBuildCancelled; Link = Link->NextCoverLink)
	{
		Link->ClearExposedFireLinks();
		NumCover++;
	}

	INT LastUpdate = 0;
	INT UpdateGranularity = NumCover / 5;

	if( !bFromDefinePaths )
	{
		// Adjust all slots if it wasn't already done in DefinePaths
		AdjustCover( FALSE );


		// Second alignment/orientation pass
		NumDone = 0;
		GWarn->StatusUpdatef( NumDone++, NumCover, *LocalizeUnrealEd(TEXT("BuildCoverAdjustingCoverSlots")) );
		for (ACoverLink *Link = Info->CoverList; Link != NULL && !bBuildCancelled; Link = Link->NextCoverLink)
		{
			if( !FPathBuilder::IsBuildingPaths() || FPathBuilder::IsPtWithinPathBuildBounds(Link->Location) )
			{

				if(NumDone >= (LastUpdate + UpdateGranularity))
				{
					GWarn->UpdateProgress( NumDone++, NumCover );
					LastUpdate=NumDone;
				}

				for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
				{
					Link->AutoAdjustSlot( SlotIdx, TRUE );
				}
			}
			bBuildCancelled = GEngine->GetMapBuildCancelled();
		}

		const FString LocalizeBuildCoverAdjustingCoverSlots( LocalizeUnrealEd(TEXT("BuildCoverAdjustingCoverSlots")) );
		LastUpdate =0;
		for( ACoverLink *Link = Info->CoverList; Link != NULL && !bBuildCancelled; Link = Link->NextCoverLink )
		{
			if(NumDone >= (LastUpdate + UpdateGranularity))
			{
				GWarn->StatusUpdatef( NumDone++, NumCover, *LocalizeBuildCoverAdjustingCoverSlots );
				LastUpdate=NumDone;
			}

			if( !FPathBuilder::IsBuildingPaths() || FPathBuilder::IsPtWithinPathBuildBounds(Link->Location) )
			{

				for( INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++ )
				{
					Link->BuildSlotInfo( SlotIdx );
				}
			}
			bBuildCancelled = GEngine->GetMapBuildCancelled();
		}
	}

	// Pass for fire links
	NumDone = 0;
	LastUpdate=0;
	for( ACoverLink *Link = Info->CoverList; Link != NULL && !bBuildCancelled; Link = Link->NextCoverLink, NumDone++ )
	{
		if(NumDone >= (LastUpdate + UpdateGranularity))
		{
			GWarn->StatusUpdatef( NumDone, NumCover, LocalizeSecure(LocalizeUnrealEd(TEXT("BuildCoverBuildingFireLinksF")), NumDone, NumCover) );
			LastUpdate = NumDone;
		}

		if( !FPathBuilder::IsBuildingPaths() || FPathBuilder::IsPtWithinPathBuildBounds(Link->Location) )
		{
			Link->BuildFireLinks( this );
		}

		bBuildCancelled = GEngine->GetMapBuildCancelled();
	}

	// Pass for other links (ie exposed links)
	NumDone = 0;
	LastUpdate = 0;
	for( ACoverLink *Link = Info->CoverList; Link != NULL && !bBuildCancelled; Link = Link->NextCoverLink, NumDone++ )
	{
		if(NumDone >= (LastUpdate + UpdateGranularity))
		{
			GWarn->StatusUpdatef( NumDone, NumCover, LocalizeSecure(LocalizeUnrealEd(TEXT("BuildCoverBuildingOtherLinksF")), NumDone, NumCover) );
			LastUpdate = NumDone;
		}

		if( !FPathBuilder::IsBuildingPaths() || FPathBuilder::IsPtWithinPathBuildBounds(Link->Location) )
		{
			Link->BuildOtherLinks( this );
		}

		bBuildCancelled = GEngine->GetMapBuildCancelled();
	}

	if( !bFromDefinePaths )
	{
		SetPathCollision(FALSE);
	}

	GWorld->DumpCoverStats();

	debugf(TEXT("Time: %.2f"),appSeconds()-BeginTime);

	GWarn->EndSlowTask();
}

void AScout::FinishPathBuild()
{
	UBOOL bBuildCancelled = GEngine->GetMapBuildCancelled();
	if( !bBuildCancelled )
	{
		AWorldInfo *Info = GWorld->GetWorldInfo();
		// check for links across levels
		for (ACoverLink *Link = Info->CoverList; Link != NULL; Link = Link->NextCoverLink)
		{
			for (INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++)
			{
				for (INT Idx = 0; Idx < Link->Slots(SlotIdx).FireLinks.Num(); Idx++)
				{
					FFireLink &FireLink = Link->Slots(SlotIdx).FireLinks(Idx);
					// Get info destination of the firelink
					FCoverInfo DestInfo;
					if( !Link->GetFireLinkTargetCoverInfo( SlotIdx, Idx, DestInfo ) )
					{
						continue;
					}

					FireLink.SetCoverRefIdx( Link->FindCoverReference( DestInfo.Link, DestInfo.SlotIdx ) );
				}
			}
		}

		// remove bases in other levels
		for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav && !bBuildCancelled; Nav=Nav->nextNavigationPoint )
		{
			if (Nav->Base != NULL && (Nav->Base->GetOutermost() != Nav->GetOutermost()))
			{
				Nav->SetBase(NULL);
				Nav->bPathsChanged = FALSE;
				Nav->PostEditChange();
				// check for cross level paths,
				Nav->bHasCrossLevelPaths = FALSE;
				for (INT PathIdx = 0; PathIdx < Nav->PathList.Num(); PathIdx++)
				{
					UReachSpec *Spec = Nav->PathList(PathIdx);
					if (Spec->Start != NULL && *Spec->End != NULL &&
						Spec->Start == Nav && !Spec->bPruned &&
						Spec->Start->GetOutermost() != Spec->End->GetOutermost())
					{
						ANavigationPoint *Start = Spec->Start;
						ANavigationPoint *End = Spec->End.Nav();
						debugf(TEXT("Path crosses levels %s vs %s"),*Start->GetFullName(),*End->GetFullName());
						Spec->End.Guid = *End->GetGuid();
						Nav->bHasCrossLevelPaths = TRUE;
					}
				}
			}
		}
	}
}


//------------------------------------------------------------------------------------------------
// certain actors add paths to mark their position
INT ANavigationPoint::AddMyMarker(AActor *S)
{
	return 1;
}

INT APathNode::AddMyMarker(AActor *S)
{
	GWorld->GetWorldInfo()->bHasPathNodes = true;
	return 1;
}

FVector ALadderVolume::FindCenter()
{
	FVector Center(0.f,0.f,0.f);
	check(BrushComponent);
	check(Brush);
	for(INT PolygonIndex = 0;PolygonIndex < Brush->Polys->Element.Num();PolygonIndex++)
	{
		FPoly&	Poly = Brush->Polys->Element(PolygonIndex);
		FVector NewPart(0.f,0.f,0.f);
		for(INT VertexIndex = 0;VertexIndex < Poly.Vertices.Num();VertexIndex++)
			NewPart += Poly.Vertices(VertexIndex);
		NewPart /= Poly.Vertices.Num();
		Center += NewPart;
	}
	Center /= Brush->Polys->Element.Num();
	return Center;
}

INT ALadderVolume::AddMyMarker(AActor *S)
{
	check(BrushComponent);

	if ( !bAutoPath || !Brush )
		return 0;

	FVector Center = FindCenter();
	Center = LocalToWorld().TransformFVector(Center);

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0 ;

	UClass *pathClass = AAutoLadder::StaticClass();
	AAutoLadder* DefaultLadder = (AAutoLadder*)( pathClass->GetDefaultActor() );

	// find ladder bottom
	FCheckResult Hit(1.f);
	GWorld->SingleLineCheck(Hit, this, Center - 10000.f * ClimbDir, Center, TRACE_World);
	if ( Hit.Time == 1.f )
		return 0;
	FVector Position = Hit.Location + DefaultLadder->CylinderComponent->CollisionHeight * ClimbDir;

	// place Ladder at bottom of volume
	GWorld->SpawnActor(pathClass, NAME_None, Position, FRotator(0,0,0), NULL, FALSE, FALSE, this);

	// place Ladder at top of volume + 0.5 * CollisionHeight of Ladder
	Position = FindTop(Center + 500.f * ClimbDir);
	GWorld->SpawnActor(pathClass, NAME_None, Position - 5.f * ClimbDir, FRotator(0,0,0), NULL, FALSE, FALSE, this);
	return 2;
}

// find the edge of the brush in the ClimbDir direction
FVector ALadderVolume::FindTop(FVector V)
{
	if ( Encompasses(V) )
		return FindTop(V + 500.f * ClimbDir);

	// trace back to brush edge from this outside point
	FCheckResult Hit(1.f);
	check(BrushComponent);
	BrushComponent->LineCheck( Hit, V - 10000.f * ClimbDir, V, FVector(0.f,0.f,0.f), 0 );
	return Hit.Location;

}
#endif

/**
 * Called when a property value from a member struct or array has been changed in the editor.
 */
void ALadderVolume::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// Check to see if a member of the WallDir property was modified
	FEditPropertyChain::TDoubleLinkedListNode* MemberNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
	UProperty* OutermostProperty = MemberNode->GetValue();
	if ( OutermostProperty != NULL )
	{
		FName OuterPropertyName = OutermostProperty->GetFName();
		if ( OuterPropertyName == TEXT("WallDir") )
		{
			WallDirArrow->Rotation = WallDir;
		}
	}

	// Now call the parent implementation which will update the volume brush
	FPropertyChangedEvent PropertyEvent(PropertyChangedEvent.PropertyChain.GetActiveNode()->GetValue());
	Super::PostEditChangeProperty( PropertyEvent );
}

//------------------------------------------------------------------------------------------------
//Private methods

#if WITH_EDITOR
UBOOL FPathBuilder::NavMeshWorld()
{
	for( FActorIterator It; It; ++It )
	{
		APylon* Pylon = Cast<APylon>(*It);
		if( Pylon != NULL )
		{
			return TRUE;
		}
	}
	return FALSE;
}

#define PATHBUILDOP(x) \
	if ( MapCancelled == NULL || !(*MapCancelled)() ) \
{ \
	FPathBuilder::Exec( TEXT(x) ); \
} \

void FPathBuilder::BuildPaths(UBOOL bBuildReachSpecs, UBOOL bOnlyBuildSelected, MapCancelledFunc MapCancelled)
{
	bBuildingPaths = TRUE;
	PathBuildBounds.Init();
	PathBuildBounds.IsValid = FALSE;
	PATHBUILDOP("PREDEFINEPATHS");

	UBOOL bNavMesh = NavMeshWorld();
	if ( bNavMesh )
	{
		if( bOnlyBuildSelected )
		{		
			PATHBUILDOP("GENERATENAVMESH SHOWMAPCHECK=0 ONLYBUILDSELECTED=1");
		}
		else
		{
			PATHBUILDOP("GENERATENAVMESH SHOWMAPCHECK=0 ONLYBUILDSELECTED=0");
		}

		if(bBuildReachSpecs)
		{
			PATHBUILDOP("DEFINEPATHS REVIEWPATHS=1 SHOWMAPCHECK=0 UNDEFINEPATHS=0");
		}
	}
	else
	{
		PATHBUILDOP("DEFINEPATHS REVIEWPATHS=1 SHOWMAPCHECK=0 UNDEFINEPATHS=1");
	}

	if( !bOnlyBuildSelected || PathBuildBounds.IsValid )
	{
		PATHBUILDOP("BUILDCOVER FROMDEFINEPATHS=0");
	}
	
	PATHBUILDOP("POSTDEFINEPATHS");
	if(bBuildReachSpecs||!bNavMesh)
	{
		PATHBUILDOP("BUILDNETWORKIDS");
	}
	PATHBUILDOP("FINISHPATHBUILD");
	bBuildingPaths = FALSE;
}

void FPathBuilder::AddBoxToPathBuildBounds( const FBox& Box )
{
	if( !PathBuildBounds.IsValid )
	{
		PathBuildBounds.Init();
	}
	PathBuildBounds+=Box;
}
UBOOL FPathBuilder::IsPtWithinPathBuildBounds(const FVector& Point )
{
	if( PathBuildBounds.IsValid )
	{
		return PathBuildBounds.IsInside(Point);
	}
	return TRUE;	
}
#endif

AScout* FPathBuilder::Scout = NULL;
UBOOL FPathBuilder::bBuildingPaths = FALSE;
DWORD FPathBuilder::LoadedPathVersionNum = 0;
FBox FPathBuilder::PathBuildBounds = FBox();

void FPathBuilder::DestroyScout()
{
	for (FActorIterator It; It; ++It)
	{
		AScout* TheScout = Cast<AScout>(*It);
		if (TheScout != NULL)
		{
			if (TheScout->Controller != NULL)
			{
				GWorld->DestroyActor(TheScout->Controller);
			}
			GWorld->DestroyActor(TheScout);
		}
	}

	FPathBuilder::Scout = NULL;
}

/*getScout()
Find the scout actor in the level. If none exists, add one.
*/
AScout* FPathBuilder::GetScout()
{
	AScout* NewScout = FPathBuilder::Scout;
	if (NewScout == NULL || NewScout->IsPendingKill())
	{
		NewScout = NULL;
		FString ScoutClassName = GEngine->ScoutClassName;
		UClass *ScoutClass = FindObject<UClass>(ANY_PACKAGE, *ScoutClassName);
		if (ScoutClass == NULL)
		{
			appErrorf(TEXT("Failed to find scout class for path building!"));
		}
		// search for an existing scout
		for( FActorIterator It; It && NewScout == NULL; ++It )
		{
			if (It->IsA(ScoutClass))
			{
				NewScout = Cast<AScout>(*It);
			}
		}
		// if one wasn't found create a new one
		if (NewScout == NULL)
		{
			NewScout = (AScout*)GWorld->SpawnActor(ScoutClass);
			check(NewScout != NULL);
			// set the scout as transient so it can't get accidentally saved in the map
			NewScout->SetFlags(RF_Transient);
			// give it a default controller
			
			// MT->Make sure scout always spawns in the persistent level! so it doesn't get orphaned in sublevels
			ULevel* OldLevel = GWorld->CurrentLevel;
			GWorld->CurrentLevel = GWorld->PersistentLevel;
			NewScout->Controller = (AController*)GWorld->SpawnActor(FindObjectChecked<UClass>(ANY_PACKAGE, TEXT("AIController")));
			GWorld->CurrentLevel = OldLevel;

			check(NewScout->Controller != NULL);
			NewScout->Controller->SetFlags(RF_Transient);
		}
		if (NewScout != NULL)
		{
			// initialize scout collision properties
			NewScout->SetCollision( TRUE, TRUE, NewScout->bIgnoreEncroachers );
			NewScout->bCollideWorld = 1;
			NewScout->SetZone( 1,1 );
			NewScout->PhysicsVolume = GWorld->GetWorldInfo()->GetDefaultPhysicsVolume();
			NewScout->SetVolumes();
			NewScout->bHiddenEdTemporary = TRUE;
			NewScout->SetPrototype();

		}
	}
	return NewScout;
}

#if WITH_EDITOR
void FPathBuilder::Exec( const TCHAR* Str )
{
	Scout = GetScout();
	if( Scout )
	{
		Scout->Exec( Str );
	}
	DestroyScout();
}

void AScout::Exec( const TCHAR* Str )
{
	if( ParseCommand( &Str, TEXT("PREDEFINEPATHS") ) )
	{
		// Give kismet objects a chance to influence path building
		for( TObjectIterator<USequenceObject> SeqObj; SeqObj; ++SeqObj )
		{
			SeqObj->PrePathBuild( this );
		}
	}
	else if( ParseCommand( &Str, TEXT("POSTDEFINEPATHS") ) )
	{
		GWarn->BeginSlowTask( TEXT("PostPathBuild..."), FALSE );
		SetPathCollision(TRUE);

		INT Count = 0;
		for( TObjectIterator<USequenceObject> SeqObj; SeqObj; ++SeqObj)
		{
			Count++;
		}
		INT Idx=0;
		for( TObjectIterator<USequenceObject> SeqObj; SeqObj; ++SeqObj,++Idx)
		{
			SeqObj->PostPathBuild( this );
		}

		SetPathCollision(FALSE);

		GWarn->EndSlowTask();
	}
	else if( ParseCommand( &Str, TEXT("DEFINEPATHS") ) )
	{
		UBOOL bReviewPaths	= FALSE;
		UBOOL bShowMapCheck = FALSE;
		UBOOL bUndefinePaths = FALSE;

		ParseUBOOL( Str, TEXT("REVIEWPATHS="),  bReviewPaths  );
		ParseUBOOL( Str, TEXT("SHOWMAPCHECK="), bShowMapCheck );
		ParseUBOOL( Str, TEXT("UNDEFINEPATHS="), bUndefinePaths );

		DefinePaths( bReviewPaths, bShowMapCheck,bUndefinePaths );
	}
	else if( ParseCommand( &Str, TEXT("BUILDCOVER") ) )
	{
		UBOOL bFromDefinePaths = FALSE;

		ParseUBOOL( Str, TEXT("FROMDEFINEPATHS="), bFromDefinePaths );

		BuildCover( bFromDefinePaths );
		FPathBuilder::Exec( TEXT("BUILDCOMBATZONES FROMDEFINEPATHS=0") );
	}
	else if( ParseCommand( &Str, TEXT("SETPATHCOLLISION") ) )
	{
		UBOOL bEnabled = FALSE;

		ParseUBOOL( Str, TEXT("ENABLED="), bEnabled );

		SetPathCollision( bEnabled );
	}
	else if( ParseCommand( &Str, TEXT("ADDLONGREACHSPECS") ) )
	{
		INT NumPaths = 0;
		Parse( Str, TEXT("NUMPATHS="), NumPaths );
		AddLongReachSpecs( NumPaths );
	}	
	else if( ParseCommand( &Str, TEXT("BUILDNETWORKIDS") ) )
	{
		ANavigationPoint::BuildNetworkIDs();
	}
	else if( ParseCommand( &Str, TEXT("FINISHPATHBUILD") ) )
	{
		FinishPathBuild();
	}

	// NAV MESH
	if( ParseCommand( &Str, TEXT("GENERATENAVMESH") ) )
	{
		UBOOL bShowMapCheck = FALSE;
		UBOOL bOnlyBuildSelected = FALSE;
		ParseUBOOL( Str, TEXT("SHOWMAPCHECK="), bShowMapCheck );
		ParseUBOOL( Str, TEXT("ONLYBUILDSELECTED="), bOnlyBuildSelected );
		GenerateNavMesh( bShowMapCheck, bOnlyBuildSelected );
	}
}

/** Do a second pass to add long range reachspecs */
void AScout::AddLongReachSpecs( INT NumPaths )
{
	debugfSuppressed(NAME_DevPath, TEXT("Add long range reachspecs"));
	INT NumDone = 0;
	UBOOL bBuildCancelled = GEngine->GetMapBuildCancelled();
	const FString LocalizeBuildPathsAddingLongRangeReachspecs( LocalizeUnrealEd(TEXT("BuildPathsAddingLongRangeReachspecs")) );
	for( ANavigationPoint *Nav=GWorld->GetFirstNavigationPoint(); Nav && !bBuildCancelled; Nav=Nav->nextNavigationPoint )
	{
		GWarn->StatusUpdatef( NumDone, NumPaths, *LocalizeBuildPathsAddingLongRangeReachspecs );
		bBuildCancelled = GEngine->GetMapBuildCancelled();
		Nav->AddLongPaths(this, FALSE);
		NumDone++;
	}

	// Called again to potentially prune new paths
	if( !bBuildCancelled )
	{
		PrunePaths(NumPaths);
	}
}

FVector AScout::GetMaxSize()
{
	FLOAT MaxRadius = 0.f;
	FLOAT MaxHeight = 0.f;
	for (INT i = 0; i < PathSizes.Num(); i++)
	{
		MaxRadius = Max<FLOAT>(MaxRadius, PathSizes(i).Radius);
		MaxHeight = Max<FLOAT>(MaxHeight, PathSizes(i).Height);
	}
	return FVector(MaxRadius, MaxHeight, 0.f);
}
#endif


void AScout::PostBeginPlay()
{
	Super::PostBeginPlay();
	SetPrototype();
}

void AScout::SetPrototype()
{
}

AScout* AScout::GetGameSpecificDefaultScoutObject()
{
	static AScout* CachedScoutDefaultObject=NULL;


	if(CachedScoutDefaultObject == NULL && !(GIsGarbageCollecting))
	{
		UClass* ScoutClass = UObject::StaticLoadClass(AScout::StaticClass(), NULL, *GEngine->ScoutClassName, NULL, LOAD_None, NULL);

		if(ScoutClass != NULL)
		{
			CachedScoutDefaultObject = ScoutClass->GetDefaultObject<AScout>();
		}
	}

	return CachedScoutDefaultObject;

}
//=============================================================================
// UReachSpec

static void DrawLineArrow(FPrimitiveDrawInterface* PDI,const FVector &start,const FVector &end,const FColor &color,FLOAT mag)
{
	// draw a pretty arrow
	FVector dir = end - start;
	FLOAT dirMag = dir.Size();
	dir /= dirMag;
	FVector YAxis, ZAxis;
	dir.FindBestAxisVectors(YAxis,ZAxis);
	FMatrix arrowTM(dir,YAxis,ZAxis,start);
	DrawDirectionalArrow(PDI,arrowTM,color,dirMag,mag,SDPG_World);
}


/**
* Adds geometery for a UReachSpec object.
* @param ReachSpec		ReachSpec to add geometry for.
*/
void UReachSpec::AddToDebugRenderProxy(FDebugRenderSceneProxy* DRSP)
{
	if ( Start && *End && !End->IsPendingKill() )
	{
		FPlane PathColorValue = PathColor();
		FVector Dir = (End->Location - Start->Location);
		FLOAT Size = Dir.Size();
		Dir /= Size;
		UBOOL bOneWay = (End.Nav()->GetReachSpecTo(Start) == NULL);
		
		FLOAT StartDist = Size * 0.5f;
		FLOAT EndDist = (GIsGame) ? (Max<FLOAT>(Size-10.f,5.f)) : (Max<FLOAT>(Size-20.f,5.f));
		if(bOneWay && AScout::GetGameSpecificDefaultScoutObject()->bHightlightOneWayReachSpecs)
		{
			// if there's no edge going back, draw a red dashed line from middle back to non connected point
			new(DRSP->DashedLines) FDebugRenderSceneProxy::FDashedLine(Start->Location + Dir * (Size - EndDist) ,Start->Location + (Dir * StartDist),FLinearColor(255,0,0),6.f);
		}

		FLinearColor Color = FLinearColor(PathColorValue.X,PathColorValue.Y,PathColorValue.Z,PathColorValue.W);
		//new(DRSP->ArrowLines) FDebugRenderSceneProxy::FArrowLine(Start->Location + (Dir * StartDist), Start->Location + Dir * EndDist,Color);
		new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Start->Location, End->Location,Color);
		if((reachFlags & R_JUMP) == R_JUMP)
		{
			new(DRSP->Stars) FDebugRenderSceneProxy::FWireStar((Start->Location + End->Location)*0.5f,Color,25.f);
		}
	}
	else
	{
		// check for connections across levels
		if ( GIsEditor && Start && End.Guid.IsValid() )
		{
			for (FActorIterator It; It; ++It)
			{
				ANavigationPoint *Nav = Cast<ANavigationPoint>(*It);
				if (Nav != NULL && *Nav->GetGuid() == End.Guid)
				{

					FVector Dir = (Nav->Location - Start->Location);
					FLOAT Size = Dir.Size();
					Dir /= Size;
					//new(DRSP->ArrowLines) FDebugRenderSceneProxy::FArrowLine(Start->Location + (Dir * Min<FLOAT>(10.f,Size * 0.2f)), Nav->Location + Dir * (Max<FLOAT>(Size-10.f,5.f)), FColor(128,128,128,255));

					if( GIsEditor &&
						Start->IsSelected() && 
						Nav->IsSelected() )
					{
						const INT Idx = DRSP->Cylinders.Add();
						FDebugRenderSceneProxy::FWireCylinder &Cylinder = DRSP->Cylinders(Idx);

						Cylinder.Base = Start->Location + FVector(0,0,(CollisionHeight - Start->CylinderComponent->CollisionHeight));
						Cylinder.Color = FColor(255,0,0);
						Cylinder.Radius = CollisionRadius;
						Cylinder.HalfHeight = CollisionHeight;
					}

					// no need to continue searching
					break;
				}
			}
		}
	}
}

void FNavigationOctreeObject::SetOwner(UObject* InOwner)
{
	Owner = InOwner;
	OwnerType = 0;
	if (Cast<ANavigationPoint>(Owner))
	{
		OwnerType |= NAV_NavigationPoint;
	}
	else if (Cast<UReachSpec>(Owner))
	{
		OwnerType |= NAV_ReachSpec;
	}
}

void FNavigationOctreeObject::SetBox(const FBox& InBoundingBox)
{
	UBOOL bIsInOctree = (OctreeNode != NULL);
	if (bIsInOctree)
	{
		GWorld->NavigationOctree->RemoveObject(this);
	}
	BoundingBox = InBoundingBox;
	BoxCenter = BoundingBox.GetCenter();
	if (bIsInOctree)
	{
		GWorld->NavigationOctree->AddObject(this);
	}
}

UBOOL FNavigationOctreeObject::OverlapCheck(const FBox& TestBox)
{
	if (Owner != NULL && OwnerType & NAV_ReachSpec)
	{
		return ((UReachSpec*)Owner)->NavigationOverlapCheck(TestBox);
	}
	else
	{
		// the default is to only use the AABB check
		return false;
	}
}

/** destructor, removes us from the octree if we're still there */
FNavigationOctreeObject::~FNavigationOctreeObject()
{
	if (OctreeNode != NULL && GWorld != NULL && GWorld->NavigationOctree != NULL)
	{
		GWorld->NavigationOctree->RemoveObject(this);
	}
}

/** filters an object with the given bounding box through this node, either adding it or passing it to its children
 * if the object is added to this node, the node may also be split if it exceeds the maximum number of objects allowed for a node without children
 * assumes the bounding box fits in this node and always succeeds
 * @param Object the object to filter
 * @param NodeBounds the bounding box for this node
 */
void FNavigationOctreeNode::FilterObject(FNavigationOctreeObject* Object, const FOctreeNodeBounds& NodeBounds)
{
	INT ChildIndex = -1;
	if (Children != NULL)
	{
		ChildIndex = FindChild(NodeBounds, Object->BoundingBox);
	}
	if (ChildIndex != -1)
	{
		Children[ChildIndex].FilterObject(Object, FOctreeNodeBounds(NodeBounds, ChildIndex));
	}
	else
	{
		if (Children == NULL && Objects.Num() >= MAX_OBJECTS_PER_NODE)
		{
			Children = new FNavigationOctreeNode[8];

			// remove and reinsert primitives at this node, to see if they belong at a lower level
			TArray<FNavigationOctreeObject*> ReinsertObjects(Objects);
			Objects.Empty();
			ReinsertObjects.AddItem(Object);
			
			for (INT i = 0; i < ReinsertObjects.Num(); i++)
			{
				FilterObject(ReinsertObjects(i), NodeBounds);
			}
		}
		else
		{
			// store the object here
			Objects.AddItem(Object);
			Object->OctreeNode = this;
		}
	}
}

/** returns all objects in this node and all children whose bounding box intersects with the given sphere
 * @param Point the center of the sphere
 * @param RadiusSquared squared radius of the sphere
 * @param Extent bounding box for the sphere
 * @param OutObjects (out) all objects found in the radius
 * @param NodeBounds the bounding box for this node
 */
void FNavigationOctreeNode::RadiusCheck(const FVector& Point, FLOAT RadiusSquared, const FBox& Extent, TArray<FNavigationOctreeObject*>& OutObjects, const FOctreeNodeBounds& NodeBounds)
{
	// iterate through all the objects and add all the ones whose origin is within the radius
	for (INT i = 0; i < Objects.Num(); i++)
	{
		if (SphereAABBIntersectionTest(Point, RadiusSquared, Objects(i)->BoundingBox))
		{
			OutObjects.AddItem(Objects(i));
		}
	}

	// check children
	if (Children != NULL)
	{
		INT ChildIdx[8];
		INT NumChildren = FindChildren(NodeBounds, Extent, ChildIdx);
		for (INT i = 0; i < NumChildren; i++)
		{
			Children[ChildIdx[i]].RadiusCheck(Point, RadiusSquared, Extent, OutObjects, FOctreeNodeBounds(NodeBounds, ChildIdx[i]));
		}
	}
}

/** checks the given box against the objects in this node and returns the first object found that intersects with it
 * recurses down to children that intersect the box
 * @param Box the box to check
 * @param NodeBounds the bounding box for this node
 * @return the first object found that intersects, or NULL if none do
 */
void FNavigationOctreeNode::OverlapCheck(const FBox& Box, TArray<FNavigationOctreeObject*>& OutObjects, const FOctreeNodeBounds& NodeBounds)
{
	const INT ObjectsNum = Objects.Num();
	// iterate through all the objects; if we find one that intersects the given box, return it
	for (INT i = 0; i < ObjectsNum; i++)
	{
		if (i+2 < ObjectsNum)
		{
			CONSOLE_PREFETCH(&Objects(i+2)->BoundingBox);		
		}
		if (Objects(i)->BoundingBox.Intersect(Box) && !Objects(i)->OverlapCheck(Box))
		{
			OutObjects.AddItem(Objects(i));
		}
	}

	// check children
	if (Children != NULL)
	{
		INT ChildIdx[8];
		INT NumChildren = FindChildren(NodeBounds, Box, ChildIdx);
		for (INT i = 0; i < NumChildren; i++)
		{
			Children[ChildIdx[i]].OverlapCheck(Box, OutObjects, FOctreeNodeBounds(NodeBounds, ChildIdx[i]));
		}
	}
}

/** adds an object with the given bounding box
 * @param Object the object to add
 * @note this method assumes the object is not already in the octree
 */
void FNavigationOctree::AddObject(FNavigationOctreeObject* Object)
{
	//debugf(TEXT("Adding object %s to octree"),*Object->GetOwner<UObject>()->GetPathName());
	const FBox& BoundingBox = Object->BoundingBox;
	if (	BoundingBox.Max.X < -HALF_WORLD_MAX || BoundingBox.Min.X > HALF_WORLD_MAX ||
			BoundingBox.Max.Y < -HALF_WORLD_MAX || BoundingBox.Min.Y > HALF_WORLD_MAX ||
			BoundingBox.Max.Z < -HALF_WORLD_MAX || BoundingBox.Min.Z > HALF_WORLD_MAX )
	{
		debugf(NAME_Warning, TEXT("%s is outside the world"), *Object->GetOwner<UObject>()->GetName());
	}
	else
	{
		// verify that the object wasn't already in the octree
		checkSlow(Object->OctreeNode == NULL);

		RootNode->FilterObject(Object, RootNodeBounds);
	}
}

/** removes the given object from the octree
 * @param Object the object to remove
 * @return true if the object was in the octree and was removed, false if it wasn't in the octree
 */
UBOOL FNavigationOctree::RemoveObject(FNavigationOctreeObject* Object)
{
	if (Object->OctreeNode == NULL)
	{
		return false;
	}
	else
	{
		//debugf(TEXT("Removing object %s from octree"),*Object->GetOwner<UObject>()->GetPathName());
		UBOOL bResult = Object->OctreeNode->RemoveObject(Object);
		if (!bResult)
		{
			debugf(NAME_Warning, TEXT("Attempt to remove %s from navigation octree but it isn't there"), *Object->GetOwner<UObject>()->GetName());
		}		
		Object->OctreeNode = NULL;

		return bResult;
	}
}

/** 
 * Removes all objects from octree.
 */
void FNavigationOctree::RemoveAllObjects()
{
	delete RootNode;
	RootNode = new FNavigationOctreeNode;
}

/** counts the number of nodes and objects there are in the octree
 * @param NumNodes (out) incremented by the number of nodes (this one plus all children)
 * @param NumObjects (out) incremented by the total number of objects in this node and its child nodes
 */
void FNavigationOctreeNode::CollectStats(INT& NumNodes, INT& NumObjects)
{
	NumNodes++;
	NumObjects += Objects.Num();
	if (Children != NULL)
	{
		for (INT i = 0; i < 8; i++)
		{
			Children[i].CollectStats(NumNodes, NumObjects);
		}
	}
}

/** 
* Find the given object in the octree node
* @param bRecurseChildren - recurse on this node's child nodes
* @return TRUE if object exists in the octree node 
*/
UBOOL FNavigationOctreeNode::FindObject( UObject* Owner, UBOOL bRecurseChildren )
{
	if( Owner )
	{
		for( INT ObjIdx=0; ObjIdx < Objects.Num(); ObjIdx++ )
		{
			FNavigationOctreeObject* CurObj = Objects(ObjIdx);
			if( CurObj->GetOwner<UObject>() == Owner )
			{
				return TRUE;
			}
		}
		if( Children != NULL &&
			bRecurseChildren )
		{
			for( INT i=0; i<8; i++ )
			{
				if( Children[i].FindObject(Owner,TRUE) )
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/** console command handler for implementing debugging commands */
UBOOL FNavigationOctree::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (ParseCommand(&Cmd, TEXT("NAVOCTREE")))
	{
		if (ParseCommand(&Cmd, TEXT("STATS")))
		{
			INT NumNodes = 0, NumObjects = 0;
			RootNode->CollectStats(NumNodes, NumObjects);
			Ar.Logf(TEXT("Number of objects: %i"), NumObjects);
			Ar.Logf(TEXT("Number of nodes: %i"), NumNodes);
			Ar.Logf(TEXT("Memory used by octree structures: %i bytes"), sizeof(FNavigationOctree) + NumNodes * sizeof(FNavigationOctreeNode) + NumObjects * sizeof(FNavigationOctreeObject*));
			Ar.Logf(TEXT("Memory used by objects in the octree: %i bytes"), NumObjects * sizeof(FNavigationOctreeObject));
		}
		else if( ParseCommand(&Cmd,TEXT("FIND")) )
		{
			UObject* Object;
			if(	ParseObject(Cmd,TEXT("NAME="), UObject::StaticClass(), Object, ANY_PACKAGE) )
			{
				debugf(TEXT("NAVOCTREE: FIND: %s = %s"), *Object->GetFullName(), RootNode->FindObject(Object,TRUE) ? TEXT("TRUE") : TEXT("FALSE") );
			}
			else
			{
				debugf(TEXT("NAVOCTREE: FIND: invalid object"));
			}
		}

		return 1;
	}	
	else
	{
		return 0;
	}
}

INT ARoute::ResolveRouteIndex( INT Idx, BYTE RouteDirection, BYTE& out_bComplete, BYTE& out_bReverse)
{
	if( RouteDirection == ERD_Forward )
	{
		// If reached the end of the route list
		if( Idx >= RouteList.Num() )
		{
			if( RouteType == ERT_Linear )
			{
				// Notify completed route
				out_bComplete = 1;
				Idx = -1;
			}
			else
			if( RouteType == ERT_Loop )
			{
				// Reverse direction
				out_bReverse = 1;
				Idx	= RouteList.Num() - 2;
			}
			else
			if( RouteType == ERT_Circle )
			{
				// Circle back to start of the list
				Idx = 0;
			}
		}
	}
	else /* if( RouteDirection == ERD_Reverse ) */
	{
		// If reached the start of the route list
		if( Idx < 0 )
		{
			if( RouteType == ERT_Linear )
			{
				// Notify completed route
				out_bComplete = 1;
				Idx = -1;
			}
			else
			if( RouteType == ERT_Loop )
			{
				// Reverse direction
				out_bReverse = 1;
				Idx = 1;
			}
			else
			if( RouteType == ERT_Circle )
			{
				// Circle back to end of the list
				Idx = RouteList.Num() - 1;
			}
		}
	}

	return Idx;
}

/**
 *	Find the closest navigation point in the route
 *	(that is also within tether distance)
 */
INT ARoute::MoveOntoRoutePath( APawn* P, BYTE RouteDirection, FLOAT DistFudgeFactor )
{
	FLOAT BestDistSq	= 0.f;
	FLOAT DistSq		= 0.f;
	FLOAT DistToClosest = 0.f;
	FLOAT DistToPawn	= 0.f;

	BYTE bReverse		= 0;
	BYTE bComplete		= 0;

	// Find closest route index
	INT BestIdx = -1;
	INT Idx = 0;
	for( Idx = 0; Idx < RouteList.Num(); Idx++ )
	{
		ANavigationPoint* Nav = RouteList(Idx).Nav();
		if( Nav != NULL )
		{
			DistSq = (P->Location-Nav->Location).SizeSquared();
			if( BestIdx < 0 ||
				DistSq < BestDistSq )
			{
				BestIdx		= Idx;
				BestDistSq	= DistSq;
			}
		}
	}
	Idx = BestIdx;

	//debug
	//`AILog( "- closest Route Idx"@Idx@((Idx>=0)?RouteList[Idx].Nav:None) );

	if( RouteDirection == ERD_Forward )
	{
		BestIdx = ResolveRouteIndex( Idx+1+RouteIndexOffset, RouteDirection, bComplete, bReverse );
	}
	else
	{
		BestIdx = ResolveRouteIndex( Idx-1+RouteIndexOffset, RouteDirection, bComplete, bReverse );
	}


	// If route is ending
	if( BestIdx < 0 )
	{
		// Move to last node in route
		if( RouteDirection == ERD_Forward )
		{
			BestIdx = RouteList.Num() - 1;
		}
		else
		{
			BestIdx = 0;
		}
	}
	else
	{
		//debug
		//`AILog( "- resolve index"@BestIdx@RouteList[BestIdx].Nav@Idx );

		if( Idx >= 0 && RouteList(BestIdx).Actor != NULL && RouteList(Idx).Actor != NULL)
		{
			
			DistToClosest = (RouteList(BestIdx)->Location-RouteList(Idx)->Location).Size();
			DistToPawn    = (RouteList(BestIdx)->Location-P->Location).Size() * DistFudgeFactor;

			// If Pawn is between our closest index and the next index
			if( DistToClosest <= DistToPawn )
			{
				// Move toward next one
				BestIdx = Idx;
			}
		}
	}

	//debug
	//`AILog( "- move onto target"@BestIdx@((BestIdx>=0)?RouteList[BestIdx].Nav:None) );

	return BestIdx;
}


