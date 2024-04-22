/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "ScopedTransaction.h"
#include "DlgMapCheck.h"
#include "DlgLightingResults.h"
#include "EnginePrefabClasses.h"
#include "EngineProcBuildingClasses.h"
#include "DlgActorSearch.h"
#include "LevelUtils.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Used for actor rotation gizmo.
INT GARGAxis = -1;
UBOOL GbARG = 0;

// Click flags.
enum EViewportClick
{
	CF_MOVE_ACTOR	= 1,	// Set if the actors have been moved since first click
	CF_MOVE_TEXTURE = 2,	// Set if textures have been adjusted since first click
	CF_MOVE_ALL     = (CF_MOVE_ACTOR | CF_MOVE_TEXTURE),
};

/*-----------------------------------------------------------------------------
   Change transacting.
-----------------------------------------------------------------------------*/

//
// If this is the first time called since first click, note all selected actors.
//
void UUnrealEdEngine::NoteActorMovement()
{
	if( !GUndo && !(GEditor->ClickFlags & CF_MOVE_ACTOR) )
	{
		GEditor->ClickFlags |= CF_MOVE_ACTOR;

		const FScopedTransaction Transaction( *LocalizeUnrealEd("ActorMovement") );
		GEditorModeTools().Snapping=0;
		
		AActor* SelectedActor = NULL;
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			SelectedActor = Actor;
			break;
		}

		if( SelectedActor == NULL )
		{
			USelection* SelectedActors = GetSelectedActors();
			SelectedActors->Modify();
			SelectActor( GWorld->GetBrush(), TRUE, NULL, TRUE );
		}

		// Look for an actor that requires snapping.
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( Actor->bEdShouldSnap )
			{
				GEditorModeTools().Snapping = 1;
				break;
			}
		}

		// Modify selected actors.
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			Actor->Modify();
			if ( Actor->IsABrush() )
			{
				ABrush* Brush = (ABrush*)Actor;
				if( Brush->Brush )
				{
					Brush->Brush->Polys->Element.ModifyAllItems();
				}
			}
		}
	}
}

/** Finish snapping all actors. */
void UUnrealEdEngine::FinishAllSnaps()
{
	if(!GIsUCC)
	{
		if( ClickFlags & CF_MOVE_ACTOR )
		{
			ClickFlags &= ~CF_MOVE_ACTOR;

			for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				Actor->Modify();
				Actor->InvalidateLightingCache();
				Actor->PostEditMove( TRUE );
			}
		}
	}
}

/**
 * Cleans up after major events like e.g. map changes.
 *
 * @param	ClearSelection	Whether to clear selection
 * @param	Redraw			Whether to redraw viewports
 * @param	TransReset		Human readable reason for resetting the transaction system
 */
void UUnrealEdEngine::Cleanse( UBOOL ClearSelection, UBOOL Redraw, const TCHAR* TransReset )
{
	if( GIsRunning && !Bootstrapping )
	{
		// Release any object references held by editor dialogs.
		WxDlgActorSearch* ActorSearch = GApp->GetDlgActorSearch();
		check(ActorSearch);
		ActorSearch->Clear();

		WxDlgMapCheck* MapCheck = GApp->GetDlgMapCheck();
		check(MapCheck);
		MapCheck->ClearMessageList();

		WxDlgLightingResults* LightingResults = GApp->GetDlgLightingResults();
		check(LightingResults);
		LightingResults->ClearMessageList();
		
		WxDlgStaticMeshLightingInfo* StaticMeshLighting = GApp->GetDlgStaticMeshLightingInfo();
		check(StaticMeshLighting);
		StaticMeshLighting->ClearMessageList();
	}

	Super::Cleanse( ClearSelection, Redraw, TransReset );
}

//
// Get the editor's pivot location
//
FVector UUnrealEdEngine::GetPivotLocation()
{
	return GEditorModeTools().PivotLocation;
}

/**
 * Sets the editor's pivot location, and optionally the pre-pivots of actors.
 *
 * @param	NewPivot				The new pivot location
 * @param	bSnapPivotToGrid		If TRUE, snap the new pivot location to the grid.
 * @param	bIgnoreAxis				If TRUE, leave the existing pivot unaffected for components of NewPivot that are 0.
 * @param	bAssignPivot			If TRUE, assign the given pivot to any valid actors that retain it (defaults to FALSE)
 */
void UUnrealEdEngine::SetPivot( FVector NewPivot, UBOOL bSnapPivotToGrid, UBOOL bIgnoreAxis, UBOOL bAssignPivot/*=FALSE*/ )
{
	FEditorModeTools& EditorModeTools = GEditorModeTools();

	if( !bIgnoreAxis )
	{
		// Don't stomp on orthonormal axis.
		if( NewPivot.X==0 ) NewPivot.X=EditorModeTools.PivotLocation.X;
		if( NewPivot.Y==0 ) NewPivot.Y=EditorModeTools.PivotLocation.Y;
		if( NewPivot.Z==0 ) NewPivot.Z=EditorModeTools.PivotLocation.Z;
	}

	// Set the pivot.
	EditorModeTools.PivotLocation   = NewPivot;
	EditorModeTools.GridBase        = FVector(0,0,0);
	EditorModeTools.SnappedLocation = NewPivot;

	if( bSnapPivotToGrid )
	{
		FRotator DummyRotator(0,0,0);
		Constraints.Snap( EditorModeTools.SnappedLocation, EditorModeTools.GridBase, DummyRotator );
		EditorModeTools.PivotLocation = EditorModeTools.SnappedLocation;
	}

	// Check all actors.
	INT Count=0, SnapCount=0;

	//default to using the x axis for the translate rotate widget
	EditorModeTools.TranslateRotateXAxisAngle = 0.0f;
	FVector TranslateRotateWidgetWorldXAxis;

	AActor* LastSelectedActor = NULL;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if (Count==0)
		{
			TranslateRotateWidgetWorldXAxis = Actor->LocalToWorld().TransformNormal(FVector(1.0f, 0.0f, 0.0f));
			//get the xy plane project of this vector
			TranslateRotateWidgetWorldXAxis.Z = 0.0f;
			if (!TranslateRotateWidgetWorldXAxis.Normalize())
			{
				TranslateRotateWidgetWorldXAxis = FVector(1.0f, 0.0f, 0.0f);
			}
		}

		LastSelectedActor = Actor;
		++Count;
		SnapCount += Actor->bEdShouldSnap;
	}
	
	if( bAssignPivot && LastSelectedActor && GEditor->bGroupingActive ) 
	{
		// set group pivot for the root-most group
		AGroupActor* ActorGroupRoot = AGroupActor::GetRootForActor(LastSelectedActor, TRUE, TRUE);
		if(ActorGroupRoot)
		{
			ActorGroupRoot->SetLocation( EditorModeTools.PivotLocation );
		}
	}

	//if there are multiple actors selected, just use the x-axis for the "translate/rotate" widget
	if (Count == 1)
	{
		EditorModeTools.TranslateRotateXAxisAngle = TranslateRotateWidgetWorldXAxis.Rotation().Yaw;
	}

	// Update showing.
	EditorModeTools.PivotShown = SnapCount>0 || Count>1;
}

//
// Reset the editor's pivot location.
//
void UUnrealEdEngine::ResetPivot()
{
	GEditorModeTools().PivotShown = 0;
	GEditorModeTools().Snapping   = 0;
}

/*-----------------------------------------------------------------------------
	Selection.
-----------------------------------------------------------------------------*/

/**
 * Fast track function to set render thread flags marking selection rather than reconnecting all components
 * @param InActor - the actor to toggle view flags for
 */
void UUnrealEdEngine::SetActorSelectionFlags (AActor* InActor)
{
	const UBOOL bActorSelected = InActor->IsSelected();

	//Original way of updating selection
	//InActor->ForceUpdateComponents(FALSE,FALSE);

	//for every component in the actor
	for(INT ComponentIndex = 0;ComponentIndex < InActor->Components.Num();ComponentIndex++)
	{
		UActorComponent* Component = InActor->Components(ComponentIndex);
		if (Component && Component->IsAttached())
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
			if (PrimitiveComponent)
			{
				PrimitiveComponent->PushSelectionToProxy(bActorSelected);
			}
			ULightComponent* LightComponent = Cast<ULightComponent>(Component);
			if (LightComponent)
			{
				LightComponent->UpdateSelection(bActorSelected);
			}
		}
	}

	//special case for terrain actors
	ATerrain* TerrainActor = Cast <ATerrain>(InActor);
	if (TerrainActor)
	{
		for(INT ComponentIndex = 0;ComponentIndex < TerrainActor->TerrainComponents.Num();ComponentIndex++)
		{
			UTerrainComponent* TerrainComponent = TerrainActor->TerrainComponents(ComponentIndex);
			if (TerrainComponent)
			{
				TerrainComponent->PushSelectionToProxy(bActorSelected);
			}
		}
	}
}


//
// Selection change.
//
void UUnrealEdEngine::NoteSelectionChange()
{
	// Notify the editor.
	GCallbackEvent->Send( CALLBACK_SelChange );

	// Pick a new common pivot, or not.
	INT Count=0;
	AActor* SingleActor=NULL;

	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		SingleActor = Actor;
		check(Actor->IsTemplate() || !FLevelUtils::IsLevelLocked(Actor->GetLevel()));
		Count++;
	}
	
	if( Count==0 ) 
	{
		ResetPivot();
	}
	else
	{
		// Set pivot point to the actor's location
		FVector PivotPoint = SingleActor->Location;

		// If grouping is active, see if this actor is part of a locked group and use that pivot instead
		if(GEditor->bGroupingActive)
		{
			AGroupActor* ActorGroupRoot = AGroupActor::GetRootForActor(SingleActor, TRUE, TRUE);
			if(ActorGroupRoot)
			{
				PivotPoint = ActorGroupRoot->Location;
			}
		}
		SetPivot( PivotPoint, FALSE, TRUE );
	}

	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		ActiveModes(ModeIndex)->ActorSelectionChangeNotify();
	}

	UpdatePropertyWindows();

	RedrawLevelEditingViewports();
}

/*
 * @param	InGroupActor		Group to Select/Deselect
 * @param	bForceSelection	Flag to select the group even if it's not currently locked. Defaults to false
 * @param	bInSelected	Flag to select or deselect the group. Defaults to true
 */
void UUnrealEdEngine::SelectGroup(AGroupActor* InGroupActor, UBOOL bForceSelection/*=FALSE*/, UBOOL bInSelected/*=TRUE*/)
{
	// Select all actors within the group (if locked or forced)
	if( bForceSelection || InGroupActor->IsLocked() )
	{	
		TArray<AActor*> GroupActors;
		InGroupActor->GetGroupActors(GroupActors);
		for( INT ActorIndex=0; ActorIndex < GroupActors.Num(); ++ActorIndex )
		{                  
			SelectActor(GroupActors(ActorIndex), bInSelected, NULL, TRUE );
		}
		bForceSelection = TRUE;

		// Recursively select any subgroups
		TArray<AGroupActor*> SubGroups;
		InGroupActor->GetSubGroups(SubGroups);
		for( INT GroupIndex=0; GroupIndex < SubGroups.Num(); ++GroupIndex )
		{
			SelectGroup(SubGroups(GroupIndex), bForceSelection, bInSelected);
		}
	}
}

/**
 * Selects/deselects and actor.
 */
void UUnrealEdEngine::SelectActor(AActor* Actor, UBOOL bInSelected, FViewportClient* InViewportClient, UBOOL bNotify, UBOOL bSelectEvenIfHidden)
{
	// Recursion guard, set when selecting parts of prefabs.
	static UBOOL bIteratingOverPrefabActors = FALSE;
	static UBOOL bIteratingOverGroups = FALSE;

	// If selections are globally locked, leave.
	if( GEdSelectionLock || Actor && !Actor->bEditable )
	{
		return;
	}

	// Only abort from hidden actors if we are selecting. You can deselect hidden actors without a problem.
	if ( bInSelected )
	{
		// If the actor is NULL, can't select it
		if ( Actor == NULL )
		{
			return;
		}

		// If the actor is NULL or hidden, leave.
		if ( !bSelectEvenIfHidden && ( Actor->IsHiddenEd() || !FLevelUtils::IsLevelVisible( Actor->GetLevel() ) ) )
		{
			return;
		}

		if ( !Actor->IsTemplate() && FLevelUtils::IsLevelLocked(Actor->GetLevel()) )
		{
			warnf(TEXT("SelectActor: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
			return;
		}
	}

	// If grouping operations are not currently allowed, don't select groups.
	AGroupActor* SelectedGroupActor = Cast<AGroupActor>(Actor);
	if( SelectedGroupActor && !GEditor->bGroupingActive )
	{
		return;
	}

	UBOOL bSelectionHandled = FALSE;

	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );
	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		bSelectionHandled |= ActiveModes(ModeIndex)->Select( Actor, bInSelected );
	}

	// Select the actor and update its internals.
	if( !bSelectionHandled )
	{
		// There's no need to recursively look for parts of prefabs, because actors can belong to a single prefab only.
		if ( !bIteratingOverPrefabActors )
		{
			// Select parts of prefabs if "prefab lock" is enabled, or if this actor is a prefab.
			if ( bPrefabsLocked || Cast<APrefabInstance>(Actor) )
			{
				AProcBuilding* Building = Cast<AProcBuilding>(Actor);
				if(Building)
				{
					bIteratingOverPrefabActors = TRUE;

					TArray<AProcBuilding*> GroupBuildings;
					Building->GetAllGroupedProcBuildings(GroupBuildings);
					
					for(INT ActorIdx=0; ActorIdx<GroupBuildings.Num(); ActorIdx++ )
					{
						AProcBuilding* GroupB = GroupBuildings(ActorIdx);
						SelectActor( GroupB, bInSelected, InViewportClient, FALSE, TRUE );
					}
					
					bIteratingOverPrefabActors = FALSE;
					
					if (bNotify)
					{
						NoteSelectionChange();
					}
				}
			
				APrefabInstance* PrefabInstance = Actor->FindOwningPrefabInstance();
				if ( PrefabInstance )
				{
					bIteratingOverPrefabActors = TRUE;

					// Select the prefab instance itself, if we're not already selecting it.
					if ( Actor != PrefabInstance )
					{
						SelectActor( PrefabInstance, bInSelected, InViewportClient, FALSE, TRUE );
					}

					// Select the prefab parts.
					TArray<AActor*> ActorsInPrefabInstance;
					PrefabInstance->GetActorsInPrefabInstance( ActorsInPrefabInstance );
					for( INT ActorIndex = 0 ; ActorIndex < ActorsInPrefabInstance.Num() ; ++ActorIndex )
					{
						AActor* PrefabActor = ActorsInPrefabInstance(ActorIndex);
						SelectActor( PrefabActor, bInSelected, InViewportClient, FALSE, TRUE );
					}

					bIteratingOverPrefabActors = FALSE;
				}
			}
		}

		if(GEditor->bGroupingActive && !bIteratingOverGroups)
		{
			// if this actor is a group, do a group select
			bIteratingOverGroups = TRUE;
			if( SelectedGroupActor )
			{
				SelectGroup(SelectedGroupActor, TRUE, bInSelected);
			}
			else
			{
				// Select this actor's entire group, starting from the top locked group.
				// If none is found, just select the actor.
				AGroupActor* ActorLockedRootGroup = AGroupActor::GetRootForActor(Actor, TRUE);
				if( ActorLockedRootGroup )
				{
					SelectGroup(ActorLockedRootGroup, FALSE, bInSelected);
				}
			}
			bIteratingOverGroups = FALSE;
		}

		// Don't do any work if the actor's selection state is already the selected state.
		const UBOOL bActorSelected = Actor->IsSelected();
		if ( (bActorSelected && !bInSelected) || (!bActorSelected && bInSelected) )
		{
			GetSelectedActors()->Select( Actor, bInSelected );
			
			//A fast path to mark selection rather than reconnecting ALL components for ALL actors that have changed state
			SetActorSelectionFlags (Actor);

			if( bNotify )
			{
				NoteSelectionChange();
			}
		}
		else
		{
			if( bNotify )
			{
				//reset the property windows.  In case something has changed since previous selection
				UpdatePropertyWindows();
			}
		}
	}

	// If we are locking selections to the camera, move the viewport camera to the selected actors location.
	if( bInSelected && GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->bLockSelectedToCamera )
	{
		GCurrentLevelEditingViewportClient->ViewLocation = Actor->Location;
		GCurrentLevelEditingViewportClient->ViewRotation = Actor->Rotation;
	}
}

/**
 * Selects or deselects a BSP surface in the persistent level's UModel.  Does nothing if GEdSelectionLock is TRUE.
 *
 * @param	InModel					The model of the surface to select.
 * @param	iSurf					The index of the surface in the persistent level's UModel to select/deselect.
 * @param	bSelected				If TRUE, select the surface; if FALSE, deselect the surface.
 * @param	bNoteSelectionChange	If TRUE, call NoteSelectionChange().
 */
void UUnrealEdEngine::SelectBSPSurf(UModel* InModel, INT iSurf, UBOOL bSelected, UBOOL bNoteSelectionChange)
{
	if( GEdSelectionLock )
	{
		return;
	}

	FBspSurf& Surf = InModel->Surfs( iSurf );
	InModel->ModifySurf( iSurf, FALSE );

	if( bSelected )
	{
		Surf.PolyFlags |= PF_Selected;
	}
	else
	{
		Surf.PolyFlags &= ~PF_Selected;
	}

	if( bNoteSelectionChange )
	{
		NoteSelectionChange();
	}
}

/**
 * Deselects all BSP surfaces in the specified level.
 *
 * @param	Level		The level for which to deselect all levels.
 * @return				The number of surfaces that were deselected
 */
static DWORD DeselectAllSurfacesForLevel(ULevel* Level)
{
	DWORD NumSurfacesDeselected = 0;
	if ( Level )
	{
		UModel* Model = Level->Model;
		for( INT SurfaceIndex = 0 ; SurfaceIndex < Model->Surfs.Num() ; ++SurfaceIndex )
		{
			FBspSurf& Surf = Model->Surfs(SurfaceIndex);
			if( Surf.PolyFlags & PF_Selected )
			{
				Model->ModifySurf( SurfaceIndex, FALSE );
				Surf.PolyFlags &= ~PF_Selected;
				++NumSurfacesDeselected;
			}
		}
	}
	return NumSurfacesDeselected;
}

/**
 * Deselect all actors.  Does nothing if GEdSelectionLock is TRUE.
 *
 * @param	bNoteSelectionChange		If TRUE, call NoteSelectionChange().
 * @param	bDeselectBSPSurfs			If TRUE, also deselect all BSP surfaces.
 */
void UUnrealEdEngine::SelectNone(UBOOL bNoteSelectionChange, UBOOL bDeselectBSPSurfs, UBOOL WarnAboutManyActors)
{
	if( GEdSelectionLock )
	{
		return;
	}

	UBOOL bShowProgress = FALSE;

	// If there are a lot of actors to process, pop up a warning "are you sure?" box
	if( WarnAboutManyActors )
	{
		INT NumSelectedActors = GEditor->GetSelectedActorCount();
		if( NumSelectedActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
		{
			bShowProgress = TRUE;

			FString ConfirmText = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "Warning_ManyActorsForDeselect" ), NumSelectedActors ) );
			WxSuppressableWarningDialog ManyActorsWarning( ConfirmText, LocalizeUnrealEd( "Warning_ManyActors" ), "Warning_ManyActors", TRUE );
			if( ManyActorsWarning.ShowModal() == wxID_CANCEL )
			{
				return;
			}
		}
	}

	if( bShowProgress )
	{
		GWarn->BeginSlowTask( TEXT( "Deselecting Actors" ), TRUE );
	}

	USelection* SelectedActors = GetSelectedActors();

	// Make a list of selected actors . . .
	TArray<AActor*> ActorsToDeselect;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		ActorsToDeselect.AddItem( Actor );
	}

	SelectedActors->Modify();

	// . . . and deselect them.
	for ( INT ActorIndex = 0 ; ActorIndex < ActorsToDeselect.Num() ; ++ActorIndex )
	{
		AActor* Actor = ActorsToDeselect( ActorIndex );
		SelectActor( Actor, FALSE, NULL, FALSE );
	}

	DWORD NumDeselectSurfaces = 0;
	if( bDeselectBSPSurfs )
	{
		// Unselect all surfaces in all levels.
		NumDeselectSurfaces += DeselectAllSurfacesForLevel( GWorld->PersistentLevel );
		AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
		for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			if( StreamingLevel )
			{
				NumDeselectSurfaces += DeselectAllSurfacesForLevel( StreamingLevel->LoadedLevel );
			}
		}
	}

	//prevents clicking on background multiple times spamming selection changes
	if (ActorsToDeselect.Num() || NumDeselectSurfaces)
	{
		GetSelectedActors()->DeselectAll();

		if( bNoteSelectionChange )
		{
			NoteSelectionChange();
		}
	}

	if( bShowProgress )
	{
		GWarn->EndSlowTask();
	}
}
