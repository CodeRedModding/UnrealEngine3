/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "ScopedTransaction.h"
#include "Factories.h"
#include "EngineSequenceClasses.h"
#include "EnginePrefabClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineMeshClasses.h"
#include "EngineAnimClasses.h"
#include "EngineFoliageClasses.h"
#include "EngineProcBuildingClasses.h"
#include "LevelUtils.h"
#include "UnTerrain.h"
#include "UnLinkedObjEditor.h"
#include "Kismet.h"
#include "BusyCursor.h"
#include "DlgGenericComboEntry.h"
#include "SpeedTree.h"
#include "BSPOps.h"
#include "AttachmentEditor.h"
#include "EditorLevelUtils.h"

PRAGMA_DISABLE_OPTIMIZATION /* Not performance-critical */

static INT RecomputePoly( ABrush* InOwner, FPoly* Poly )
{
	// force recalculation of normal, and texture U and V coordinates in FPoly::Finalize()
	Poly->Normal = FVector(0,0,0);

	// catch normalization exceptions to warn about non-planar polys
	try
	{
		return Poly->Finalize( InOwner, 0 );
	}
	catch(...)
	{
		debugf( TEXT("WARNING: FPoly::Finalize() failed (You broke the poly!)") );
	}

	return 0;
}

/*-----------------------------------------------------------------------------
   Actor adding/deleting functions.
-----------------------------------------------------------------------------*/

class FSelectedActorExportObjectInnerContext : public FExportObjectInnerContext
{
public:
	FSelectedActorExportObjectInnerContext::FSelectedActorExportObjectInnerContext()
		//call the empty version of the base class
		: FExportObjectInnerContext(FALSE)
	{
		// For each object . . .
		for ( TObjectIterator<UObject> It ; It ; ++It )
		{
			UObject* InnerObj = *It;
			UObject* OuterObj = InnerObj->GetOuter();

			//assume this is not part of a selected actor
			UBOOL bIsChildOfSelectedActor = FALSE;

			UObject* TestParent = OuterObj;
			while (TestParent)
			{
				AActor* TestParentAsActor = Cast<AActor>(TestParent);
				if ( TestParentAsActor && TestParentAsActor->IsSelected())
				{
					bIsChildOfSelectedActor = TRUE;
					break;
				}
				TestParent = TestParent->GetOuter();
			}

			if (bIsChildOfSelectedActor)
			{
				InnerList* Inners = ObjectToInnerMap.Find( OuterObj );
				if ( Inners )
				{
					// Add object to existing inner list.
					Inners->AddItem( InnerObj );
				}
				else
				{
					// Create a new inner list for the outer object.
					InnerList& InnersForOuterObject = ObjectToInnerMap.Set( OuterObj, InnerList() );
					InnersForOuterObject.AddItem( InnerObj );
				}
			}
		}
	}
};


/**
 * Copy selected actors to the clipboard.  Does not copy PrefabInstance actors or parts of Prefabs.
 *
 * @param	bReselectPrefabActors	If TRUE, reselect any actors that were deselected prior to export as belonging to prefabs.
 * @param	bClipPadCanBeUsed		If TRUE, the clip pad is available for use if the user is holding down SHIFT.
 * @param	DestinationData			If != NULL, additionally copy data to string
 */
void UUnrealEdEngine::edactCopySelected(UBOOL bReselectPrefabActors, UBOOL bClipPadCanBeUsed, FString* DestinationData)
{
	// Before copying, deselect:
	//		- Actors belonging to prefabs unless all actors in the prefab are selected.
	//		- Builder brushes.
	TArray<AActor*> ActorsToDeselect;

	UBOOL bSomeSelectedActorsNotInCurrentLevel = FALSE;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Deselect any selected builder brushes.
		const UBOOL bActorIsBuilderBrush = (Actor->IsABrush() && Actor->IsABuilderBrush());
		if( bActorIsBuilderBrush || Actor->IsInPrefabInstance() )
		{
			ActorsToDeselect.AddItem(Actor);
		}

		// If any selected actors are not in the current level, warn the user that some actors will not be copied.
		if ( !bSomeSelectedActorsNotInCurrentLevel && Actor->GetLevel() != GWorld->CurrentLevel )
		{
			bSomeSelectedActorsNotInCurrentLevel = TRUE;
			appMsgf( AMT_OK, *LocalizeUnrealEd("CopySelectedActorsInNonCurrentLevel") );
		}
	}

	const FScopedBusyCursor BusyCursor;
	for( INT ActorIndex = 0 ; ActorIndex < ActorsToDeselect.Num() ; ++ActorIndex )
	{
		AActor* Actor = ActorsToDeselect(ActorIndex);
		GetSelectedActors()->Deselect( Actor );
	}

	// Export the actors.
	FStringOutputDevice Ar;
	const FSelectedActorExportObjectInnerContext Context;
	UExporter::ExportToOutputDevice( &Context, GWorld, NULL, Ar, TEXT("copy"), 0, PPF_DeepCompareInstances | PPF_ExportsNotFullyQualified);
	appClipboardCopy( *Ar );
	if( DestinationData )
	{
		*DestinationData = *Ar;
	}

	// If the clip pad is being used...
	if( bClipPadCanBeUsed && (GetAsyncKeyState(VK_SHIFT) & 0x8000) )
	{	
		PasteClipboardIntoClipPad();
	}

	// Reselect actors that were deselected for being or belonging to prefabs.
	if ( bReselectPrefabActors )
	{
		for( INT ActorIndex = 0 ; ActorIndex < ActorsToDeselect.Num() ; ++ActorIndex )
		{
			AActor* Actor = ActorsToDeselect(ActorIndex);
			GetSelectedActors()->Select( Actor );
		}
	}
}

/**
 * Paste selected actors from the clipboard.
 *
 * @param	bDuplicate			Is this a duplicate operation (as opposed to a real paste)?
 * @param	bOffsetLocations	Should the actor locations be offset after they are created?
 * @param	bClipPadCanBeUsed	If TRUE, the clip pad is available for use if the user is holding down SHIFT.
 * @param	SourceData			If != NULL, use instead of clipboard data
 */
void UUnrealEdEngine::edactPasteSelected(UBOOL bDuplicate, UBOOL bOffsetLocations, UBOOL bClipPadCanBeUsed, FString* SourceData)
{
	// If the clip pad is the source of the pasted text...

	if( bClipPadCanBeUsed && (GetAsyncKeyState(VK_SHIFT) & 0x8000) )
	{	
		AWorldInfo* Info = GWorld->GetWorldInfo();
		FString ClipboardText;
		if( SourceData )
		{
			ClipboardText = *SourceData;
		}
		else
		{
			ClipboardText = appClipboardPaste();
		}

		WxDlgGenericComboEntry* dlg = new WxDlgGenericComboEntry( FALSE, TRUE );

		// Gather up the existing clip pad entry names

		TArray<FString> ClipPadNames;

		for( int x = 0 ; x < Info->ClipPadEntries.Num() ; ++x )
		{
			ClipPadNames.AddItem( Info->ClipPadEntries(x)->Title );
		}

		// Ask where the user which clip pad entry they want to paste from

		if( dlg->ShowModal( TEXT("ClipPadSelect"), TEXT("PasteFrom:"), ClipPadNames, 0, TRUE ) == wxID_OK )
		{
			FString ClipPadName = dlg->GetComboBoxString();

			// Find the requested entry and copy it to the clipboard

			for( int x = 0 ; x < Info->ClipPadEntries.Num() ; ++x )
			{
				if( Info->ClipPadEntries(x)->Title == ClipPadName )
				{
					appClipboardCopy( *(Info->ClipPadEntries(x)->Text) );
				}
			}
		}
		else
		{
			return;
		}
	}

	const FScopedBusyCursor BusyCursor;

	// Save off visible layers.
	TArray<FString> VisibleLayerArray;
	GWorld->GetWorldInfo()->VisibleLayers.ParseIntoArray( &VisibleLayerArray, TEXT(","), 0 );

	// Transact the current selection set.
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	// Get pasted text.
	FString PasteString;
	if( SourceData )
	{
		PasteString = *SourceData;
	}
	else
	{
		PasteString = appClipboardPaste();
	}
	const TCHAR* Paste = *PasteString;

	// Import the actors.
	ULevelFactory* Factory = new ULevelFactory;
	Factory->FactoryCreateText( ULevel::StaticClass(), GWorld->CurrentLevel, GWorld->CurrentLevel->GetFName(), RF_Transactional, NULL, bDuplicate ? TEXT("move") : TEXT("paste"), Paste, Paste+appStrlen(Paste), GWarn );

	// Fire CALLBACK_LevelDirtied and CALLBACK_RefreshEditor_LevelBrowser when falling out of scope.
	FScopedLevelDirtied					LevelDirtyCallback;
	FScopedRefreshEditor_LevelBrowser	LevelBrowserRefresh;

	ELevelViewportType ViewportType = LVT_None;
	if (GCurrentLevelEditingViewportClient)
	{
		ViewportType = GCurrentLevelEditingViewportClient->ViewportType;
	}

	// Update the actors' locations and update the global list of visible layers.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Offset the actor's location.
		Actor->Location += Actor->CreateLocationOffset( bDuplicate, bOffsetLocations, ViewportType, GEditor->Constraints.GetGridSize() );

		// Add any layers this actor belongs to into the visible array.
		TArray<FString> ActorLayers;
		Actor->Layer.ToString().ParseIntoArray( &ActorLayers, TEXT(","), 0 );

		for( INT LayerIndex = 0 ; LayerIndex < ActorLayers.Num() ; ++LayerIndex )
		{
			const FString& LayerName = ActorLayers(LayerIndex);
			VisibleLayerArray.AddUniqueItem( LayerName );
		}

		// Call PostEditMove to update components, notify decals, etc.
		Actor->PostEditMove( TRUE );
		Actor->PostDuplicate();

		// Request saves/refreshes.
		Actor->MarkPackageDirty();
		LevelDirtyCallback.Request();
		LevelBrowserRefresh.Request();
	}

	// Create the new list of visible layers and set it
	FString NewVisibleLayers;
	for( INT LayerIndex = 0 ; LayerIndex < VisibleLayerArray.Num() ; ++LayerIndex )
	{
		if( NewVisibleLayers.Len() )
		{
			NewVisibleLayers += TEXT(",");
		}
		NewVisibleLayers += VisibleLayerArray(LayerIndex);
	}
	GWorld->GetWorldInfo()->VisibleLayers = NewVisibleLayers;
	GCallbackEvent->Send( CALLBACK_RefreshEditor_LayerBrowser );

	// Note the selection change.  This will also redraw level viewports and update the pivot.
	NoteSelectionChange();
}

namespace DuplicateSelectedActors {
/**
 * A collection of actors to duplicate and prefabs to instance that all belong to the same level.
 */
class FDuplicateJob
{
public:
	/** A list of actors to duplicate. */
	TArray<AActor*>	Actors;

	/** A list of prefabs to instance. */
	TArray<APrefabInstance*> PrefabInstances;

	/** The source level that all actors in the Actors array come from. */
	ULevel*			SrcLevel;

	/**
	 * Duplicate the job's actors to the specified destination level.  The new actors
	 * are appended to the specified output lists of actors.
	 *
	 * @param	OutNewActors			[out] Newly created actors are appended to this list.
	 * @param	OutNewPrefabInstances	[out] Newly created prefab instances are appended to this list.
	 * @param	DestLevel				The level to duplicate the actors in this job to.
	 * @param	bOffsetLocations		Passed to edactPasteSelected; TRUE if new actor locations should be offset.
	 */
	void DuplicateActorsToLevel(TArray<AActor*>& OutNewActors, TArray<APrefabInstance*>& OutNewPrefabInstances, ULevel* DestLevel, UBOOL bOffsetLocations)
	{
		if ( FLevelUtils::IsLevelLocked(SrcLevel) || FLevelUtils::IsLevelLocked(DestLevel) )
		{
			warnf(TEXT("DuplicateActorsToLevel: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
			return;
		}

		// Set the selection set to be precisely the actors belonging to this job.
		GWorld->CurrentLevel = SrcLevel;
		GEditor->SelectNone( FALSE, TRUE );
		for ( INT ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
		{
			AActor* Actor = Actors( ActorIndex );
			GEditor->SelectActor( Actor, TRUE, NULL, FALSE );
		}

		FString ScratchData;

		// Copy actors from src level.
		GEditor->edactCopySelected( TRUE, FALSE, &ScratchData );

		// Paste to the dest level.
		GWorld->CurrentLevel = DestLevel;
		GEditor->edactPasteSelected( TRUE, bOffsetLocations, FALSE, &ScratchData );

		// The selection set will be the newly created actors; copy them over to the output array.
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );
			OutNewActors.AddItem( Actor );
		}

		ELevelViewportType ViewportType = LVT_None;
		if (GCurrentLevelEditingViewportClient)
		{
			ViewportType = GCurrentLevelEditingViewportClient->ViewportType;
		}

		// Create new prefabs in the destination level.
		for ( INT PrefabIndex = 0 ; PrefabIndex < PrefabInstances.Num() ; ++PrefabIndex )
		{
			APrefabInstance* SrcPrefabInstance = PrefabInstances(PrefabIndex);
			UPrefab* TemplatePrefab = SrcPrefabInstance->TemplatePrefab;
			const FVector NewLocation( SrcPrefabInstance->Location + SrcPrefabInstance->CreateLocationOffset( TRUE, bOffsetLocations, ViewportType, GEditor->Constraints.GetGridSize() ) );

			APrefabInstance* NewPrefabInstance = TemplatePrefab
				? GEditor->Prefab_InstancePrefab( TemplatePrefab, NewLocation, FRotator(0,0,0) )
				: NULL;

			if ( NewPrefabInstance )
			{
				OutNewPrefabInstances.AddItem( NewPrefabInstance );
			}
			else
			{
				debugf( TEXT("Failed to instance prefab %s into level %s"), *SrcPrefabInstance->GetPathName(), *GWorld->CurrentLevel->GetName() );
			}
		}
	}
};
}

/** 
 * Duplicates selected actors.  Handles the case where you are trying to duplicate PrefabInstance actors.
 *
 * @param	bOffsetLocations		Should the actor locations be offset after they are created?
 */
void UUnrealEdEngine::edactDuplicateSelected( UBOOL bOffsetLocations )
{
	using namespace DuplicateSelectedActors;

	const FScopedBusyCursor BusyCursor;
	GetSelectedActors()->Modify();

	// Get the list of selected prefabs.
	TArray<APrefabInstance*> SelectedPrefabInstances;
	DeselectActorsBelongingToPrefabs( SelectedPrefabInstances, FALSE );

	// Create per-level job lists.
	typedef TMap<ULevel*, FDuplicateJob*>	DuplicateJobMap;
	DuplicateJobMap							DuplicateJobs;

	// Build set of selected actors before duplication
	TArray<AActor*> PreDuplicateSelection;

	// Add selected actors to the per-level job lists.
	UBOOL bHaveActorLocation = FALSE;
	FVector AnyActorLocation = FVector::ZeroVector;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( !bHaveActorLocation )
		{
			bHaveActorLocation = TRUE;
			AnyActorLocation = Actor->Location;
		}

		PreDuplicateSelection.AddItem(Actor);

		ULevel* OldLevel = Actor->GetLevel();
		FDuplicateJob** Job = DuplicateJobs.Find( OldLevel );
		if ( Job )
		{
			(*Job)->Actors.AddItem( Actor );
		}
		else
		{
			// Allocate a new job for the level.
			FDuplicateJob* NewJob = new FDuplicateJob;
			NewJob->SrcLevel = OldLevel;
			NewJob->Actors.AddItem( Actor );
			DuplicateJobs.Set( OldLevel, NewJob );
		}
	}

	// Add prefab instances to the per-level job lists.
	for ( INT Index = 0 ; Index < SelectedPrefabInstances.Num() ; ++Index )
	{
		APrefabInstance* PrefabInstance = SelectedPrefabInstances(Index);
		ULevel* PrefabLevel = PrefabInstance->GetLevel();
		FDuplicateJob** Job = DuplicateJobs.Find( PrefabLevel );
		if ( Job )
		{
			(*Job)->PrefabInstances.AddItem( PrefabInstance );
		}
		else
		{
			// Allocate a new job for the level.
			FDuplicateJob* NewJob = new FDuplicateJob;
			NewJob->SrcLevel = PrefabLevel;
			NewJob->PrefabInstances.AddItem( PrefabInstance );
			DuplicateJobs.Set( PrefabLevel, NewJob );
		}
	}

	// Copy off the current level
	ULevel* OldCurrentLevel = GWorld->CurrentLevel;

	// Check to see if the user wants to place the actor into a streaming grid network
	ULevel* DesiredLevel = EditorLevelUtils::GetLevelForPlacingNewActor( AnyActorLocation );
	GWorld->CurrentLevel = DesiredLevel;


	// For each level, select the actors in that level and copy-paste into the destination level.
	TArray<AActor*>	NewActors;
	TArray<APrefabInstance*> NewPrefabInstances;
	for ( DuplicateJobMap::TIterator It( DuplicateJobs ) ; It ; ++It )
	{
		FDuplicateJob* Job = It.Value();
		check( Job );
		Job->DuplicateActorsToLevel( NewActors, NewPrefabInstances, GWorld->CurrentLevel, bOffsetLocations );
	}

	// Restore the current level
	GWorld->CurrentLevel = OldCurrentLevel;

	// Select any newly created actors and prefabs.
	SelectNone( FALSE, TRUE );
	for ( INT ActorIndex = 0 ; ActorIndex < NewActors.Num() ; ++ActorIndex )
	{
		AActor* Actor = NewActors( ActorIndex );
		SelectActor( Actor, TRUE, NULL, FALSE );
	}
	for ( INT PrefabIndex = 0 ; PrefabIndex < NewPrefabInstances.Num() ; ++PrefabIndex )
	{
		APrefabInstance* PrefabInstance = NewPrefabInstances( PrefabIndex );
		SelectActor( PrefabInstance, TRUE, NULL, FALSE );
	}
	NoteSelectionChange();

	// Finally, cleanup.
	for ( DuplicateJobMap::TIterator It( DuplicateJobs ) ; It ; ++It )
	{
		FDuplicateJob* Job = It.Value();
		delete Job;
	}
	
	// Build set of selected actors after duplication
	TArray<AActor*> PostDuplicateSelection;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		PostDuplicateSelection.AddItem(Actor);
	}
	
	TArray<FEdMode*> ActiveModes;
	GEditorModeTools().GetActiveModes( ActiveModes );

	for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
	{
		// Tell the tools about the duplication
		ActiveModes(ModeIndex)->ActorsDuplicatedNotify(PreDuplicateSelection, PostDuplicateSelection, bOffsetLocations);
	}
}


/**
 * Deletes all selected actors.  bIgnoreKismetReferenced is ignored when bVerifyDeletionCanHappen is TRUE.
 *
 * @param		bVerifyDeletionCanHappen	[opt] If TRUE (default), verify that deletion can be performed.
 * @param		bIgnoreKismetReferenced		[opt] If TRUE, don't delete actors referenced by Kismet.
 * @return									TRUE unless the delete operation was aborted.
 */
UBOOL UUnrealEdEngine::edactDeleteSelected(UBOOL bVerifyDeletionCanHappen, UBOOL bIgnoreKismetReferenced)
{
	if ( bVerifyDeletionCanHappen )
	{
		// Provide the option to abort the delete if e.g. Kismet-referenced actors are selected.
		bIgnoreKismetReferenced = FALSE;
		if ( ShouldAbortActorDeletion(bIgnoreKismetReferenced) )
		{
			return FALSE;
		}
	}

	const DOUBLE StartSeconds = appSeconds();

	GetSelectedActors()->Modify();

	// Fire CALLBACK_LevelDirtied and CALLBACK_RefreshEditor_LevelBrowser when falling out of scope.
	FScopedLevelDirtied					LevelDirtyCallback;
	FScopedRefreshEditor_LevelBrowser	LevelBrowserRefresh;

	// Iterate over all levels and create a list of world infos.
	TArray<AWorldInfo*> WorldInfos;
	for ( INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
	{
		ULevel* Level = GWorld->Levels( LevelIndex );
		WorldInfos.AddItem( Level->GetWorldInfo() );
	}

	// Iterate over selected actors and assemble a list of actors to delete.
	TArray<AActor*> ActorsToDelete;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor		= static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Only delete transactional actors that aren't a level's builder brush or worldinfo.
		UBOOL bDeletable	= FALSE;
		if ( Actor->HasAllFlags( RF_Transactional ) )
		{
			const UBOOL bRejectBecauseOfKismetReference = bIgnoreKismetReferenced && Actor->IsReferencedByKismet();
			if ( !bRejectBecauseOfKismetReference )
			{
				const UBOOL bIsDefaultBrush = Actor->IsBrush() && Actor->IsABuilderBrush();
				if ( !bIsDefaultBrush )
				{
					const UBOOL bIsWorldInfo =
						Actor->IsA( AWorldInfo::StaticClass() ) && WorldInfos.ContainsItem( static_cast<AWorldInfo*>(Actor) );
					if ( !bIsWorldInfo )
					{
						bDeletable = TRUE;
					}
				}
			}
		}

		if ( bDeletable )
		{
			ActorsToDelete.AddItem( Actor );
		}
		else
		{
			debugf( LocalizeSecure(LocalizeUnrealEd("CannotDeleteSpecialActor"), *Actor->GetFullName()) );
		}
	}

	// Maintain a list of levels that have already been Modify()'d so that each level
	// is modify'd only once.
	TArray<ULevel*> LevelsAlreadyModified;

	// A list of levels that will need their Bsp updated after the deletion is complete
	TArray<ULevel*> LevelsToRebuild;

	UBOOL	bTerrainWasDeleted = FALSE;
	UBOOL	bBrushWasDeleted = FALSE;
	INT		DeleteCount = 0;

	USelection* SelectedActors = GetSelectedActors();

	for ( INT ActorIndex = 0 ; ActorIndex < ActorsToDelete.Num() ; ++ActorIndex )
	{
		AActor* Actor = ActorsToDelete( ActorIndex );

		// Track whether or not a terrain actor was deleted.
		// Avoid the IsA call if we already know a terrain was deleted.
		if ( !bTerrainWasDeleted && Actor->IsA(ATerrain::StaticClass()) )
		{
			bTerrainWasDeleted = TRUE;
		}
		else if (Actor->IsBrush() && !Actor->IsABuilderBrush()) // Track whether or not a brush actor was deleted.
		{
			bBrushWasDeleted = TRUE;
			ULevel* BrushLevel = Actor->GetLevel();
			if (BrushLevel)
			{
				LevelsToRebuild.AddUniqueItem(BrushLevel);
			}
		}

		// If the actor about to be deleted is a PrefabInstance, need to do a couple of additional steps.
		// check to see whether the user would like to delete the PrefabInstance's members
		// remove the prefab's sequence from the level's sequence
		APrefabInstance* PrefabInstance = Cast<APrefabInstance>(Actor);
		if ( PrefabInstance != NULL )
		{
			TArray<AActor*> PrefabMembers;
			PrefabInstance->GetActorsInPrefabInstance(PrefabMembers);

			UBOOL bAllPrefabMembersSelected = TRUE;	// remains TRUE if all members of the prefab are selected
			for ( INT MemberIndex = 0 ; MemberIndex < PrefabMembers.Num() ; ++MemberIndex )
			{
				AActor* PrefabActor = PrefabMembers(MemberIndex);
				if ( !PrefabActor->IsSelected() )
				{
					// setting to FALSE ends the loop
					bAllPrefabMembersSelected = FALSE;
					break;
				}
			}

			// if not all of the PrefabInstance's members were selected, ask the user whether they should also be deleted
			if ( !bAllPrefabMembersSelected )
			{
				const UBOOL bShouldDeleteEntirePrefab = appMsgf(AMT_YesNo, LocalizeSecure(LocalizeUnrealEd(TEXT("Prefab_DeletePrefabMembersPrompt")), *PrefabInstance->GetPathName()));
				if ( bShouldDeleteEntirePrefab )
				{
					for ( INT MemberIndex = 0; MemberIndex < PrefabMembers.Num(); MemberIndex++ )
					{
						AActor* PrefabActor = PrefabMembers(MemberIndex);

						// add the actor to the selection set (probably not necessary, but do it just in case something assumes that all prefab actors would be selected)
						SelectedActors->Select(PrefabActor, TRUE);

						// now add this actor to the list of actors being deleted if it isn't already there
						ActorsToDelete.AddUniqueItem(PrefabActor);
					}
				}
			}

			// remove the prefab's sequence from the level's sequence
			PrefabInstance->DestroyKismetSequence();
		}

		// If the actor about to be deleted is in a group, be sure to remove it from the group
		AGroupActor* ActorParentGroup = AGroupActor::GetParentForActor(Actor);
		if(ActorParentGroup)
		{
#if ENABLE_SIMPLYGON_MESH_PROXIES
			// If this is a proxy being deleted, ask if the user want to deletes the hidden meshes, rather than revert them
			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>( Actor );
			if ( SMActor && SMActor->IsProxy() && !appMsgf( AMT_YesNo, LocalizeSecure( LocalizeUnrealEd( "Group_ContainsProxyDelete" ), *SMActor->GetName() ) ) )
			{
				// Get all the actors in the group
				TArray<AActor*> Actors;
				ActorParentGroup->GetGroupActors( Actors, FALSE );
				for ( INT ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
				{
					// Just delete any meshes that are hidden (as these aren't included in the group selection)
					AStaticMeshActor* SMActorToDelete = Cast<AStaticMeshActor>( Actors( ActorIndex ) );
					if ( SMActorToDelete && SMActorToDelete != SMActor && SMActorToDelete->IsHiddenByProxy() )
					{
						SMActorToDelete->SetHiddenByProxy( FALSE );
						ActorParentGroup->Remove( *SMActorToDelete, FALSE );
						GWorld->EditorDestroyActor( SMActorToDelete, TRUE );
					}
				}
			}
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
			ActorParentGroup->Remove(*Actor, TRUE, FALSE, TRUE);	// Maintain group
		}

		// Let the object propagator report the deletion
		GObjectPropagator->OnActorDelete(Actor);

		// Mark the actor's level as dirty.
		Actor->MarkPackageDirty();
		LevelDirtyCallback.Request();
		LevelBrowserRefresh.Request();

		// Deselect the Actor.
		SelectedActors->Deselect(Actor);

		// Modify the level.  Each level is modified only once.
		// @todo DB: Shouldn't this be calling UWorld::ModifyLevel?
		ULevel* Level = Actor->GetLevel();
		if ( LevelsAlreadyModified.FindItemIndex( Level ) == INDEX_NONE )
		{
			LevelsAlreadyModified.AddItem( Level );
			Level->Modify();
		}

		// See if there is any foliage that also needs to be removed
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
		if( IFA )
		{
			for(INT ComponentIndex = 0;ComponentIndex < Actor->AllComponents.Num();ComponentIndex++)
			{
				IFA->DeleteInstancesForComponent( Actor->AllComponents(ComponentIndex) );
			}
		}

		// Destroy actor and clear references.
		GWorld->EditorDestroyActor( Actor, FALSE );

#if USE_MASSIVE_LOD
		//if using massive LOD, save this actor off for the engine Tick to do a reattach.  Only components, whose replacement primtive is directly attached
		//to this actor should be re-attached like this
		if (GWorld->bEditorHasMassiveLOD)
		{
			ActorsForGlobalReattach.AddItem(Actor);
		}
#endif

		DeleteCount++;
	}

	// Propagate RF_IsPendingKill till count doesn't change.
	INT CurrentObjectsPendingKillCount	= 0;
	INT LastObjectsPendingKillCount		= -1;
	while( CurrentObjectsPendingKillCount != LastObjectsPendingKillCount )
	{
		LastObjectsPendingKillCount		= CurrentObjectsPendingKillCount;
		CurrentObjectsPendingKillCount	= 0;

		for( TObjectIterator<UObject> It; It; ++It )
		{	
			UObject* Object = *It;
			if( Object->HasAnyFlags( RF_PendingKill ) )
			{
				// Object has RF_PendingKill set.
				CurrentObjectsPendingKillCount++;
			}
			else if( Object->IsPendingKill() )
			{
				// Make sure that setting pending kill is undoable.
				if( !Object->HasAnyFlags( RF_PendingKill ) )
				{
					Object->Modify();

					// Object didn't have RF_PendingKill set but IsPendingKill returned TRUE so we manually set the object flag.
					Object->MarkPendingKill();
				}
				CurrentObjectsPendingKillCount++;
			}
		}
	}

	// Remove all references to destroyed actors once at the end, instead of once for each Actor destroyed..
	UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	NoteSelectionChange();

	if (bTerrainWasDeleted)
	{
		GCallbackEvent->Send(CALLBACK_RefreshEditor_TerrainBrowser);
	}

	// If any brush actors were deleted, update the Bsp in the appropriate levels
	if (bBrushWasDeleted)
	{
		FlushRenderingCommands();

		for (INT LevelIdx = 0; LevelIdx < LevelsToRebuild.Num(); ++LevelIdx)
		{
			GEditor->RebuildLevel(*(LevelsToRebuild(LevelIdx)));
		}

		GWorld->UpdateComponents(TRUE);
		RedrawLevelEditingViewports();
		GCallbackEvent->Send(CALLBACK_LevelDirtied);
		GCallbackEvent->Send(CALLBACK_RefreshEditor_LevelBrowser);
	}

	debugf( TEXT("Deleted %d Actors (%3.3f secs)"), DeleteCount, appSeconds() - StartSeconds );

	return TRUE;
}

class WxHandleKismetReferencedActors : public wxDialog
{
public:
	WxHandleKismetReferencedActors()
		:	wxDialog( GApp->EditorFrame, -1, *LocalizeUnrealEd("ActorsReferencedByKismet"), wxDefaultPosition, wxDefaultSize)//, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER )
	{
		wxBoxSizer* UberSizer = new wxBoxSizer(wxVERTICAL);
		{
			// Text string
			wxSizer *TextSizer = new wxBoxSizer(wxHORIZONTAL);
			{
				wxStaticText* QuestionText = new wxStaticText( this, -1, *LocalizeUnrealEd("PromptSomeActorsAreReferencedByKismet") );
				TextSizer->Add(QuestionText, 0, /*wxALIGN_LEFT|*/wxALL|wxADJUST_MINSIZE, 5);
			}
			UberSizer->Add(TextSizer, 0, wxALL, 5);

			// OK/Cancel Buttons
			wxSizer *ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
			{
				wxButton* ButtonMoveAll = new wxButton(this, ID_Continue, *LocalizeUnrealEd("Continue"));
				ButtonMoveAll->SetDefault();
				ButtonSizer->Add(ButtonMoveAll, 0, wxEXPAND | wxALL, 5);

				wxButton* ButtonMoveNonKismet = new wxButton(this, ID_IgnoreReferenced, *LocalizeUnrealEd("IgnoreKismetReferencedActors"));
				ButtonSizer->Add(ButtonMoveNonKismet, 0, wxEXPAND | wxALL, 5);

				wxButton* ButtonCancel = new wxButton(this, wxID_CANCEL, *LocalizeUnrealEd("Cancel"));
				ButtonSizer->Add(ButtonCancel, 0, wxEXPAND | wxALL, 5);
			}
			UberSizer->Add(ButtonSizer, 0, wxALL, 5);
		}

		SetSizer( UberSizer );
		SetAutoLayout( true );
		GetSizer()->Fit( this );
	}
private:
	DECLARE_EVENT_TABLE();
	void OnContinue(wxCommandEvent& In)
	{
		EndModal( ID_Continue );
	}
	void OnIgnoreReferenced(wxCommandEvent& In)
	{
		EndModal( ID_IgnoreReferenced );
	}
};
BEGIN_EVENT_TABLE( WxHandleKismetReferencedActors, wxDialog )
	EVT_BUTTON(ID_Continue, WxHandleKismetReferencedActors::OnContinue)
	EVT_BUTTON(ID_IgnoreReferenced, WxHandleKismetReferencedActors::OnIgnoreReferenced)
END_EVENT_TABLE()

/**
 * Checks the state of the selected actors and notifies the user of any potentially unknown destructive actions which may occur as
 * the result of deleting the selected actors.  In some cases, displays a prompt to the user to allow the user to choose whether to
 * abort the deletion.
 *
 * @param	bOutIgnoreKismetReferenced		[out] Set only if it's okay to delete actors; specifies if the user wants Kismet-refernced actors not deleted.
 * @return									FALSE to allow the selected actors to be deleted, TRUE if the selected actors should not be deleted.
 */
UBOOL UUnrealEdEngine::ShouldAbortActorDeletion(UBOOL& bOutIgnoreKismetReferenced) const
{
	UBOOL bResult = FALSE;

	// Can't delete actors if Matinee is open.
	if ( !GEditorModeTools().EnsureNotInMode( EM_InterpEdit, TRUE ) )
	{
		bResult = TRUE;
	}

	if ( !bResult )
	{
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			ULevel* ActorLevel = Actor->GetLevel();
			if ( FLevelUtils::IsLevelLocked(ActorLevel) )
			{
				warnf(TEXT("Cannot perform action on actor %s because the actor's level is locked"), *Actor->GetName());
				bResult = TRUE;
				break;
			}

			if ( Actor->IsReferencedByKismet() )
			{
				WxHandleKismetReferencedActors Dlg;
				const INT DlgResult = Dlg.ShowModal();
				if ( DlgResult == wxID_CANCEL )
				{
					bResult = TRUE;
				}
				else
				{
					// Some actors are referenced by Kismet, but the user wants to proceed.
					// Record whether or not they wanted to ignore referenced actors.
					bOutIgnoreKismetReferenced = (DlgResult == ID_IgnoreReferenced);
				}
				break;
			}
		}
	}

	if( !bResult )
	{
		UBOOL bHasRouteList = FALSE;
		TArray<ARoute*> RouteList;

		UBOOL bYesToAllSelected = FALSE;

		for( FSelectionIterator It( GetSelectedActorIterator() ); It && !bResult && !bYesToAllSelected; ++It )
		{
			ANavigationPoint* DelNav = Cast<ANavigationPoint>(*It);
			if( DelNav )
			{
				if( !bHasRouteList )
				{
					bHasRouteList = TRUE;
					for( FActorIterator It; It; ++It )
					{
						ARoute* Route = Cast<ARoute>(*It);
						if( Route )
						{
							RouteList.AddItem( Route );
						}
					}
				}

				for( INT RouteIdx = 0; RouteIdx < RouteList.Num() && !bYesToAllSelected; RouteIdx++ )
				{
					ARoute* Route = RouteList(RouteIdx);
					for( INT NavIdx = 0; NavIdx < Route->RouteList.Num(); NavIdx++ )
					{
						ANavigationPoint* Nav = Cast<ANavigationPoint>(~Route->RouteList(NavIdx));
						if( Nav == DelNav )
						{
							const INT DeleteNavChoice = appMsgf( AMT_YesNoYesAllNoAll, *LocalizeUnrealEd("Prompt_30") );
							bYesToAllSelected = DeleteNavChoice == ART_YesAll;
							bResult = ( DeleteNavChoice == ART_No || DeleteNavChoice == ART_NoAll );
							break;
						}
					}
				}
			}
		}
	}

	return bResult;
}

/**
 * Replace all selected brushes with the default brush.
 */
void UUnrealEdEngine::edactReplaceSelectedBrush()
{
	// Make a list of brush actors to replace.
	ABrush* DefaultBrush = GWorld->GetBrush();

	TArray<ABrush*> BrushesToReplace;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		if ( Actor->IsBrush() && Actor->HasAnyFlags(RF_Transactional) && Actor != DefaultBrush )
		{
			BrushesToReplace.AddItem( static_cast<ABrush*>(Actor) );
		}
	}

	// Fire CALLBACK_LevelDirtied and CALLBACK_RefreshEditor_LevelBrowser when falling out of scope.
	FScopedLevelDirtied					LevelDirtyCallback;
	FScopedRefreshEditor_LevelBrowser	LevelBrowserRefresh;

	GetSelectedActors()->Modify();

	// Replace brushes.
	for ( INT BrushIndex = 0 ; BrushIndex < BrushesToReplace.Num() ; ++BrushIndex )
	{
		ABrush* SrcBrush = BrushesToReplace(BrushIndex);
		ABrush* NewBrush = FBSPOps::csgAddOperation( DefaultBrush, SrcBrush->PolyFlags, (ECsgOper)SrcBrush->CsgOper );
		if( NewBrush )
		{
			SrcBrush->MarkPackageDirty();
			NewBrush->MarkPackageDirty();

			LevelDirtyCallback.Request();
			LevelBrowserRefresh.Request();

			NewBrush->Modify();
			NewBrush->Layer = SrcBrush->Layer;
			NewBrush->CopyPosRotScaleFrom( SrcBrush );
			NewBrush->PostEditMove( TRUE );
			SelectActor( SrcBrush, FALSE, NULL, FALSE );
			SelectActor( NewBrush, TRUE, NULL, FALSE );
			GWorld->EditorDestroyActor( SrcBrush, TRUE );
		}
	}

	NoteSelectionChange();
}

static void CopyActorProperties( AActor* Dest, const AActor *Src )
{
	// Events
	Dest->Tag	= Src->Tag;

	// Object
	Dest->Layer	= Src->Layer;
}

/**
 * Replaces the specified actor with a new actor of the specified class.  The new actor
 * will be selected if the current actor was selected.
 * 
 * @param	CurrentActor			The actor to replace.
 * @param	NewActorClass			The class for the new actor.
 * @param	Archetype				The template to use for the new actor.
 * @param	bNoteSelectionChange	If TRUE, call NoteSelectionChange if the new actor was created successfully.
 * @return							The new actor.
 */
AActor* UUnrealEdEngine::ReplaceActor( AActor* CurrentActor, UClass* NewActorClass, UObject* Archetype, UBOOL bNoteSelectionChange )
{
	AActor* NewActor = GWorld->SpawnActor
	(
		NewActorClass,
		NAME_None,
		CurrentActor->Location,
		CurrentActor->Rotation,
		Cast<AActor>(Archetype),
		1
	);

	if( NewActor )
	{
		NewActor->Modify();

		const UBOOL bCurrentActorSelected = GetSelectedActors()->IsSelected( CurrentActor );
		if ( bCurrentActorSelected )
		{
			// The source actor was selected, so deselect the old actor and select the new one.
			GetSelectedActors()->Modify();
			SelectActor( NewActor, bCurrentActorSelected, NULL, FALSE );
			SelectActor( CurrentActor, FALSE, NULL, FALSE );
		}

		CopyActorProperties( NewActor, CurrentActor );
		GWorld->EditorDestroyActor( CurrentActor, TRUE );

		// Note selection change if necessary and requested.
		if ( bCurrentActorSelected && bNoteSelectionChange )
		{
			NoteSelectionChange();
		}
	}

	return NewActor;
}

/**
 * Replace all selected non-brush actors with the specified class.
 */
void UUnrealEdEngine::edactReplaceSelectedNonBrushWithClass(UClass* Class)
{
	// Make a list of actors to replace.
	TArray<AActor*> ActorsToReplace;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		if ( !Actor->IsBrush() && Actor->HasAnyFlags(RF_Transactional) )
		{
			ActorsToReplace.AddItem( Actor );
		}
	}

	// Fire CALLBACK_LevelDirtied and CALLBACK_RefreshEditor_LevelBrowser when falling out of scope.
	FScopedLevelDirtied					LevelDirtyCallback;
	FScopedRefreshEditor_LevelBrowser	LevelBrowserRefresh;

	// Replace actors.
	for ( INT i = 0 ; i < ActorsToReplace.Num() ; ++i )
	{
		AActor* SrcActor = ActorsToReplace(i);
		AActor* NewActor = ReplaceActor( SrcActor, Class, NULL, FALSE );
		if ( NewActor )
		{
			NewActor->MarkPackageDirty();
			LevelDirtyCallback.Request();
			LevelBrowserRefresh.Request();
		}
	}

	NoteSelectionChange();
}

/**
 * Replace all actors of the specified source class with actors of the destination class.
 *
 * @param	SrcClass	The class of actors to replace.
 * @param	DstClass	The class to replace with.
 */
void UUnrealEdEngine::edactReplaceClassWithClass(UClass* SrcClass, UClass* DstClass)
{
	// Make a list of actors to replace.
	TArray<AActor*> ActorsToReplace;
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if ( Actor->IsA( SrcClass ) && Actor->HasAnyFlags(RF_Transactional) )
		{
			ActorsToReplace.AddItem( Actor );
		}
	}

	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied					LevelDirtyCallback;
	FScopedRefreshEditor_LevelBrowser	LevelBrowserRefresh;

	// Replace actors.
	for ( INT i = 0 ; i < ActorsToReplace.Num() ; ++i )
	{
		AActor* SrcActor = ActorsToReplace(i);
		AActor* NewActor = ReplaceActor( SrcActor, DstClass, NULL, FALSE );
		if ( NewActor )
		{
			NewActor->MarkPackageDirty();
			LevelDirtyCallback.Request();
			LevelBrowserRefresh.Request();
		}
	}

	NoteSelectionChange();
}

/*-----------------------------------------------------------------------------
   Actor hiding functions.
-----------------------------------------------------------------------------*/

/**
 * Hide selected actors and BSP models by marking their bHiddenEdTemporary flags TRUE. Will not
 * modify/dirty actors/BSP.
 */
void UUnrealEdEngine::edactHideSelected()
{
	// Assemble a list of actors to hide.
	TArray<AActor*> ActorsToHide;
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );
		
		// Don't consider already hidden actors or the builder brush
		if ( !Actor->IsABuilderBrush() && !Actor->IsHiddenEd() )
		{
			ActorsToHide.AddItem( Actor );
		}
	}
	
	// Hide the actors that were selected and deselect them in the process
	if ( ActorsToHide.Num() > 0 )
	{
		USelection* SelectedActors = GetSelectedActors();
		SelectedActors->Modify();

		for( INT ActorIndex = 0 ; ActorIndex < ActorsToHide.Num() ; ++ActorIndex )
		{
			AActor* Actor = ActorsToHide( ActorIndex );
			
			// Save the actor to the transaction buffer to support undo/redo, but do
			// not call Modify, as we do not want to dirty the actor's package and
			// we're only editing temporary, transient values
			Actor->SaveToTransactionBuffer( FALSE );
			Actor->bHiddenEdTemporary = TRUE;
			Actor->ForceUpdateComponents( FALSE, FALSE );
			SelectedActors->Deselect( Actor );
		}

		NoteSelectionChange();
	}

	// Iterate through all of the BSP models and hide any that were selected (deselecting them in the process)
	if ( GWorld )
	{
		for ( TArray<ULevel*>::TIterator LevelIterator( GWorld->Levels ); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				if ( ( CurSurface.PolyFlags & PF_Selected ) && !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), FALSE );
					
					// Deselect the surface and mark it as hidden to the editor
					CurSurface.PolyFlags &= ~PF_Selected;
					CurSurface.bHiddenEdTemporary = TRUE;
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}

/**
 * Hide unselected actors and BSP models by marking their bHiddenEdTemporary flags TRUE. Will not
 * modify/dirty actors/BSP.
 */
void UUnrealEdEngine::edactHideUnselected()
{
	// Iterate through all of the actors and hide the ones which are not selected and are not already hidden
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if( !Actor->IsABuilderBrush() && !Actor->IsSelected() && !Actor->IsHiddenEd() )
		{
			// Save the actor to the transaction buffer to support undo/redo, but do
			// not call Modify, as we do not want to dirty the actor's package and
			// we're only editing temporary, transient values
			Actor->SaveToTransactionBuffer( FALSE );
			Actor->bHiddenEdTemporary = TRUE;
			Actor->ForceUpdateComponents( FALSE, FALSE );
		}
	}

	// Iterate through all of the BSP models and hide the ones which are not selected and are not already hidden
	if ( GWorld )
	{
		for ( TArray<ULevel*>::TIterator LevelIterator( GWorld->Levels ); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;

				// Only modify surfaces that aren't selected and aren't already hidden
				if ( !( CurSurface.PolyFlags & PF_Selected ) && !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), FALSE );
					CurSurface.bHiddenEdTemporary = TRUE;
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}

/**
 * Attempt to unhide all actors and BSP models by setting their bHiddenEdTemporary flags to FALSE if they
 * are TRUE. Note: Will not unhide actors/BSP hidden by higher priority visibility settings, such as bHiddenEdLayer,
 * but also will not modify/dirty actors/BSP.
 */
void UUnrealEdEngine::edactUnHideAll()
{
	// Iterate through all of the actors and unhide them
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if( !Actor->IsABuilderBrush() && Actor->bHiddenEdTemporary )
		{
			// Save the actor to the transaction buffer to support undo/redo, but do
			// not call Modify, as we do not want to dirty the actor's package and
			// we're only editing temporary, transient values
			Actor->SaveToTransactionBuffer( FALSE );
			Actor->bHiddenEdTemporary = FALSE;
			Actor->ForceUpdateComponents( FALSE, FALSE );
		}
	}

	// Iterate through all of the BSP models and unhide them if they are already hidden
	if ( GWorld )
	{
		for ( TArray<ULevel*>::TIterator LevelIterator( GWorld->Levels ); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				if ( CurSurface.bHiddenEdTemporary )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), FALSE );
					CurSurface.bHiddenEdTemporary = FALSE;
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}

/**
 * Mark all selected actors and BSP models to be hidden upon editor startup, by setting their bHiddenEd flag to
 * TRUE, if it is not already. This directly modifies/dirties the relevant actors/BSP.
 */
void UUnrealEdEngine::edactHideSelectedStartup()
{
	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;

	// Iterate through all of the selected actors
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Set the actor to hide at editor startup, if it's not already set that way
		if ( !Actor->IsABuilderBrush() && !Actor->IsHiddenEd() && !Actor->IsHiddenEdAtStartup() )
		{
			Actor->Modify();
			Actor->bHiddenEd = TRUE;
			LevelDirtyCallback.Request();
		}
	}

	if ( GWorld )
	{
		// Iterate through all of the selected BSP surfaces
		for ( TArray<ULevel*>::TIterator LevelIterator( GWorld->Levels ); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				
				// Set the BSP surface to hide at editor startup, if it's not already set that way
				if ( ( CurSurface.PolyFlags & PF_Selected ) && !CurSurface.IsHiddenEdAtStartup() && !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.Modify();
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), FALSE );
					CurSurface.PolyFlags |= PF_HiddenEd;
					LevelDirtyCallback.Request();
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}

/**
 * Mark all actors and BSP models to be shown upon editor startup, by setting their bHiddenEd flag to FALSE, if it is
 * not already. This directly modifies/dirties the relevant actors/BSP.
 */
void UUnrealEdEngine::edactUnHideAllStartup()
{
	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;

	// Iterate over all actors
	for ( FActorIterator It ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// If the actor is set to be hidden at editor startup, change it so that it will be shown at startup
		if ( !Actor->IsABuilderBrush() && Actor->IsHiddenEdAtStartup() )
		{
			Actor->Modify();
			Actor->bHiddenEd = FALSE;
			LevelDirtyCallback.Request();
		}
	}

	if ( GWorld )
	{
		// Iterate over all BSP surfaces
		for ( TArray<ULevel*>::TIterator LevelIterator( GWorld->Levels ); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;

				// If the BSP surface is set to be hidden at editor startup, change it so that it will be shown at startup
				if ( CurSurface.IsHiddenEdAtStartup() )
				{
					CurLevelModel.Modify();
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), FALSE );
					CurSurface.PolyFlags &= ~PF_HiddenEd;
					LevelDirtyCallback.Request();
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}

/**
 * Mark all selected actors and BSP models to be shown upon editor startup, by setting their bHiddenEd flag to FALSE, if it
 * not already. This directly modifies/dirties the relevant actors/BSP.
 */
void UUnrealEdEngine::edactUnHideSelectedStartup()
{
	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;

	// Iterate over all selected actors
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Mark the selected actor as showing at editor startup if it was currently set to be hidden
		if ( !Actor->IsABuilderBrush() && Actor->IsHiddenEdAtStartup() )
		{
			Actor->Modify();
			Actor->bHiddenEd = FALSE;
			LevelDirtyCallback.Request();
		}
	}

	if ( GWorld )
	{
		// Iterate over all selected BSP surfaces
		for ( TArray<ULevel*>::TIterator LevelIterator( GWorld->Levels ); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;

				// Mark the selected BSP surface as showing at editor startup if it was currently set to be hidden
				if ( ( CurSurface.PolyFlags & PF_Selected ) && CurSurface.IsHiddenEdAtStartup() )
				{
					CurLevelModel.Modify();
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), FALSE );
					CurSurface.PolyFlags &= ~PF_HiddenEd;
					LevelDirtyCallback.Request();
				}
			}
		}
	}
	RedrawLevelEditingViewports();
}

/** Returns the configuration of attachment that would result from calling AttachSelectedActors at this point in time */
AActor* UUnrealEdEngine::GetDesiredAttachmentState(TArray<AActor*>& OutNewChildren)
{
	// Get the selection set (first one will be the new base)
	AActor* NewBase = NULL;
	OutNewChildren.Empty();
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* SelectedActor = Cast<AActor>(*It);
		if(SelectedActor)
		{
			OutNewChildren.AddUniqueItem(SelectedActor);
		}
	}

	// Last element of the array becomes new base
	if(OutNewChildren.Num() > 0)
	{
		NewBase = OutNewChildren.Pop();
	}

	return NewBase;
}

/** Uses the current selection state to attach actors together (using SetBase). Last selected Actor becomes the base. */
void UUnrealEdEngine::AttachSelectedActors()
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("AttachActors")) );

	// Get what we want attachment to be
	TArray<AActor*> NewChildren;
	AActor* NewBase = GetDesiredAttachmentState(NewChildren);
	if(NewBase && (NewChildren.Num() > 0))
	{
		NewBase->Modify();

		// Do the actual base change
		for(INT ChildIdx=0; ChildIdx<NewChildren.Num(); ChildIdx++)
		{
			AActor* Child = NewChildren(ChildIdx);
			check(Child);
			Child->Modify();

			// Clear old base first, just to ensure loop-prevention doesn't trip
			Child->SetBase(NULL);

			// Make sure the new base doesn't point to the child we want
			if(NewBase->IsBasedOn(Child))
			{
				NewBase->SetBase(NULL);
			}

			Child->SetHardAttach(TRUE); // 'soft' attach is only really used for players standing on movers, and that will not be set up in the editor
			Child->SetBase(NewBase);
		}

		RedrawLevelEditingViewports();
	}
}

/** Adds all selected actors to the attachment editor */
void UUnrealEdEngine::AddSelectedToAttachmentEditor()
{
	// Find the attachment editor browser
	WxAttachmentEditor* AttachEd = GUnrealEd->GetBrowser<WxAttachmentEditor>( TEXT("Attachments") );
	if( AttachEd )
	{
		// Add selected actors to the browser
		AttachEd->AddSelectedToEditor();
	}
}

/*-----------------------------------------------------------------------------
   Actor selection functions.
-----------------------------------------------------------------------------*/

/**
 * Select all actors and BSP models, except those which are hidden.
 */
void UUnrealEdEngine::edactSelectAll( UBOOL UseLayerSelect/*=FALSE*/ )
{
	// If there are a lot of actors to process, pop up a warning "are you sure?" box
	INT NumActors = FActorIteratorBase::GetActorCount();
	UBOOL bShowProgress = FALSE;
	if( NumActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
	{
		bShowProgress = TRUE;
		FString ConfirmText;
		if( UseLayerSelect )
		{
			ConfirmText = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "Warning_ManyActorsForGroupSelect" ), NumActors ) );
		}
		else
		{
			ConfirmText = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "Warning_ManyActorsForSelect" ), NumActors ) );
		}
		WxSuppressableWarningDialog ManyActorsWarning( ConfirmText, LocalizeUnrealEd( "Warning_ManyActors" ), "Warning_ManyActors", TRUE );
		if( ManyActorsWarning.ShowModal() == wxID_CANCEL )
		{
			return;
		}
	}

	if( bShowProgress )
	{
		GWarn->BeginSlowTask( TEXT( "Selecting All Actors" ), TRUE);
	}

	TArray<FName> LayerArray;

	// Add all selected actors' layer name to the LayerArray.
	USelection* SelectedActors = GetSelectedActors();

	if( UseLayerSelect )
	{
		for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if( !Actor->IsHiddenEd() && Actor->Layer!=NAME_None )
			{
				LayerArray.AddUniqueItem( Actor->Layer );
			}
		}
	}

	SelectedActors->Modify();

	if( ( !UseLayerSelect ) || ( LayerArray.Num() == 0 ) ) 
	{
		for( FActorIterator It; It; ++It )
		{
			AActor* Actor = *It;
			if( !Actor->IsSelected() && !Actor->IsHiddenEd() )
			{
				SelectActor( Actor, 1, NULL, 0 );
			}
		}
	} 
	// otherwise, select all actors that match one of the layers,
	else 
	{
		// use appStrfind() to allow selection based on hierarchically organized layer names
		for( FActorIterator It; It; ++It )
		{
			AActor* Actor = *It;
			if( !Actor->IsSelected() && !Actor->IsHiddenEd() )
			{
				for( INT j=0; j<LayerArray.Num(); j++ ) 
				{
					if( appStrfind( *Actor->Layer.ToString(), *LayerArray(j).ToString() ) != NULL ) 
					{
						SelectActor( Actor, 1, NULL, 0 );
						break;
					}
				}
			}
		}
	}

	// Iterate through all of the BSP models and select them if they are not hidden
	if( !UseLayerSelect )
	{
		if ( GWorld )
		{
			for ( TArray<ULevel*>::TIterator LevelIterator( GWorld->Levels ); LevelIterator; ++LevelIterator )
			{
				UModel& CurLevelModel = *( ( *LevelIterator )->Model );
				for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
				{
					FBspSurf& CurSurface = *SurfaceIterator;
					if ( !CurSurface.IsHiddenEd() )
					{
						CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), FALSE );
						CurSurface.PolyFlags |= PF_Selected;
					}
				}
			}
		}
	}

	NoteSelectionChange();

	if( bShowProgress )
	{
		GWarn->EndSlowTask( );
	}
}

/**
 * Invert the selection of all actors and BSP models.
 */
void UUnrealEdEngine::edactSelectInvert()
{
	// If there are a lot of actors to process, pop up a warning "are you sure?" box
	INT NumActors = FActorIteratorBase::GetActorCount();
	UBOOL bShowProgress = FALSE;
	if( NumActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
	{
		bShowProgress = TRUE;
		FString ConfirmText = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "Warning_ManyActorsForInvertSelect" ), NumActors ) );
		WxSuppressableWarningDialog ManyActorsWarning( ConfirmText, LocalizeUnrealEd( "Warning_ManyActors" ), "Warning_ManyActors", TRUE );
		if( ManyActorsWarning.ShowModal() == wxID_CANCEL )
		{
			return;
		}
	}

	if( bShowProgress )
	{
		GWarn->BeginSlowTask( TEXT( "Inverting Selected Actors" ), TRUE);
	}

	GetSelectedActors()->Modify();

	// Iterate through all of the actors and select them if they are not currently selected (and not hidden)
	// or deselect them if they are currently selected

	// Turn off Grouping during this process to avoid double toggling of selected actors via group selection
	const UBOOL bGroupingActiveSaved = bGroupingActive;
	bGroupingActive = FALSE;
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if( !Actor->IsABuilderBrush() && !Actor->IsHiddenEd() )
		{
			SelectActor( Actor, !Actor->IsSelected(), NULL, FALSE );
		}
	}
	// Restore bGroupingActive to its original value
	bGroupingActive = bGroupingActiveSaved;

	// Iterate through all of the BSP models and select them if they are not currently selected (and not hidden)
	// or deselect them if they are currently selected
	if ( GWorld )
	{
		for ( TArray<ULevel*>::TIterator LevelIterator( GWorld->Levels ); LevelIterator; ++LevelIterator )
		{
			UModel& CurLevelModel = *( ( *LevelIterator )->Model );
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel.Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurface = *SurfaceIterator;
				if ( !CurSurface.IsHiddenEd() )
				{
					CurLevelModel.ModifySurf( SurfaceIterator.GetIndex(), FALSE );
					CurSurface.PolyFlags ^= PF_Selected;
				}
			}
		}
	}

	NoteSelectionChange();

	if( bShowProgress )
	{
		GWarn->EndSlowTask( );
	}
}

/**
 * Select any actors based on currently selected actors 
 */
void UUnrealEdEngine::edactSelectBased()
{
	USelection* SelectedActors = GetSelectedActors();

	if( SelectedActors != NULL )
	{
		SelectedActors->Modify();

		TArray<AActor*> BasedActors;
		for( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			for( INT AttachIdx = 0; AttachIdx < Actor->Attached.Num(); AttachIdx++ )
			{
				AActor* Child = Actor->Attached(AttachIdx);
				if( Child == NULL )
					continue;

				BasedActors.AddUniqueItem( Child );
			}
		}

		for( INT BasedIdx = 0; BasedIdx < BasedActors.Num(); BasedIdx++ )
		{
			SelectActor( BasedActors( BasedIdx ), TRUE, NULL, TRUE );
		}

		NoteSelectionChange();
	}
}

/**
 * Select all actors in a particular class.
 */
void UUnrealEdEngine::edactSelectOfClass( UClass* Class )
{
	GetSelectedActors()->Modify();

	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if( Actor->GetClass()==Class && !Actor->IsSelected() && !Actor->IsHiddenEd() )
		{
			// Selection by class not permitted for actors belonging to prefabs.
			// Selection by class not permitted for builder brushes.
			if ( !Actor->IsInPrefabInstance() && !Actor->IsABuilderBrush() )
			{
				SelectActor( Actor, 1, NULL, 0 );
			}
		}
	}
	NoteSelectionChange();
}

/**
 * Select all actors of a particular class and archetype.
 *
 * @param	InClass		Class of actor to select
 * @param	InArchetype	Archetype of actor to select
 */
void UUnrealEdEngine::edactSelectOfClassAndArchetype( const UClass* InClass, const UObject* InArchetype )
{
	GetSelectedActors()->Modify();

	// Select all actors with of the provided class and archetype, assuming they aren't already selected, 
	// aren't hidden in the editor, aren't a member of a prefab, and aren't builder brushes
	for( FActorIterator ActorIter; ActorIter; ++ActorIter )
	{
		AActor* CurActor = *ActorIter;
		if ( CurActor->GetClass() == InClass && CurActor->GetArchetype() == InArchetype && !CurActor->IsSelected() 
			&& !CurActor->IsHiddenEd() && !CurActor->IsInPrefabInstance() && !CurActor->IsABuilderBrush() )
		{
			SelectActor( CurActor, TRUE, NULL, FALSE );
		}
	}

	NoteSelectionChange();
}

/**
 * Select all actors in a particular class and its subclasses.
 */
void UUnrealEdEngine::edactSelectSubclassOf( UClass* Class )
{
	GetSelectedActors()->Modify();

	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if( !Actor->IsSelected() && !Actor->IsHiddenEd() && Actor->GetClass()->IsChildOf(Class) )
		{
			// Selection by class not permitted for actors belonging to prefabs.
			// Selection by class not permitted for builder brushes.
			if ( !Actor->IsInPrefabInstance() && !Actor->IsABuilderBrush() )
			{
				SelectActor( Actor, 1, NULL, 0 );
			}
		}
	}
	NoteSelectionChange();
}

/**
 * Select all actors in a level that are marked for deletion.
 */
void UUnrealEdEngine::edactSelectDeleted()
{
	GetSelectedActors()->Modify();

	UBOOL bSelectionChanged = FALSE;
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if( !Actor->IsSelected() && !Actor->IsHiddenEd() )
		{
			if( Actor->bDeleteMe )
			{
				bSelectionChanged = TRUE;
				SelectActor( Actor, 1, NULL, 0 );
			}
		}
	}
	if ( bSelectionChanged )
	{
		NoteSelectionChange();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Select matching static meshes.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

/**
 * Information about an actor and its static mesh.
 */
class FStaticMeshActor
{
public:
	/** Non-NULL if the actor is a static mesh. */
	AStaticMeshActor* StaticMeshActor;
	/** Non-NULL if the actor is an InterpActor actor. */
	AInterpActor* InterpActor;
	/** Non-NULL if the actor is a KActor actor. */
	AKActor* KActor;
	/** Non-NULL if the actor is a FracturedStaticMeshActor actor. */
	AFracturedStaticMeshActor* FracMeshActor;
	/** Non-NULL if the actor has a static mesh. */
	UStaticMesh* StaticMesh;

	FStaticMeshActor()
		: StaticMeshActor(NULL)
		, InterpActor(NULL)
		, KActor(NULL)
		, FracMeshActor(NULL)
		, StaticMesh(NULL)
	{}

	UBOOL IsStaticMeshActor() const
	{
		return StaticMeshActor != NULL;
	}

	UBOOL IsInterpActor() const
	{
		return InterpActor != NULL;
	}

	UBOOL IsKActor() const
	{
		return KActor != NULL;
	}

	UBOOL IsFracMeshActor() const
	{
		return FracMeshActor != NULL;
	}

	UBOOL HasStaticMesh() const
	{
		return StaticMesh != NULL;
	}

	/**
	 * Extracts the static mesh information from the specified actor.
	 */
	static UBOOL GetStaticMeshInfoFromActor(AActor* Actor, FStaticMeshActor& OutStaticMeshActor)
	{
		OutStaticMeshActor.StaticMeshActor = Cast<AStaticMeshActor>( Actor );
		OutStaticMeshActor.InterpActor = Cast<AInterpActor>( Actor );
		OutStaticMeshActor.KActor = Cast<AKActor>( Actor );
		OutStaticMeshActor.FracMeshActor = Cast<AFracturedStaticMeshActor>( Actor );

		if( OutStaticMeshActor.IsStaticMeshActor() )
		{
			if ( OutStaticMeshActor.StaticMeshActor->StaticMeshComponent )
			{
				OutStaticMeshActor.StaticMesh = OutStaticMeshActor.StaticMeshActor->StaticMeshComponent->StaticMesh;
			}
		}
		else if ( OutStaticMeshActor.IsInterpActor() )
		{
			if ( OutStaticMeshActor.InterpActor->StaticMeshComponent )
			{
				OutStaticMeshActor.StaticMesh = OutStaticMeshActor.InterpActor->StaticMeshComponent->StaticMesh;
			}
		}
		else if ( OutStaticMeshActor.IsKActor() )
		{
			if ( OutStaticMeshActor.KActor->StaticMeshComponent )
			{
				OutStaticMeshActor.StaticMesh = OutStaticMeshActor.KActor->StaticMeshComponent->StaticMesh;
			}
		}
		else if ( OutStaticMeshActor.IsFracMeshActor() )
		{
			if ( OutStaticMeshActor.FracMeshActor->FracturedStaticMeshComponent )
			{
				OutStaticMeshActor.StaticMesh = OutStaticMeshActor.FracMeshActor->FracturedStaticMeshComponent->StaticMesh;
			}
		}
		return OutStaticMeshActor.HasStaticMesh();
	}
};

} // namespace

/**
 * Select all actors that have the same static mesh assigned to them as the selected ones.
 *
 * @param bAllClasses		If TRUE, also select non-AStaticMeshActor actors whose meshes match.
 */
void UUnrealEdEngine::edactSelectMatchingStaticMesh(UBOOL bAllClasses)
{
	TArray<FStaticMeshActor> StaticMeshActors;
	TArray<FStaticMeshActor> InterpActors;
	TArray<FStaticMeshActor> KActors;
	TArray<FStaticMeshActor> FracMeshActors;

	// Make a list of selected actors with static meshes.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		FStaticMeshActor ActorInfo;
		if ( FStaticMeshActor::GetStaticMeshInfoFromActor( Actor, ActorInfo ) )
		{
			if ( ActorInfo.IsStaticMeshActor() )
			{
				StaticMeshActors.AddItem( ActorInfo );
			}
			else if ( ActorInfo.IsInterpActor() )
			{
				InterpActors.AddItem( ActorInfo );
			}
			else if ( ActorInfo.IsKActor() )
			{
				KActors.AddItem( ActorInfo );
			}
			else if ( ActorInfo.IsFracMeshActor() )
			{
				FracMeshActors.AddItem( ActorInfo );
			}
		}
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	// Loop through all non-hidden actors in visible levels, selecting those that have one of the
	// static meshes in the list.
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if ( !Actor->IsHiddenEd() )
		{
			FStaticMeshActor ActorInfo;
			if ( FStaticMeshActor::GetStaticMeshInfoFromActor( Actor, ActorInfo ) )
			{
				UBOOL bSelectActor = FALSE;
				if ( !bSelectActor && (bAllClasses || ActorInfo.IsStaticMeshActor()) )
				{
					for ( INT i = 0 ; i < StaticMeshActors.Num() ; ++i )
					{
						if ( StaticMeshActors(i).StaticMesh == ActorInfo.StaticMesh )
						{
							bSelectActor = TRUE;
							break;
						}
					}
				}
				if ( !bSelectActor && (bAllClasses || ActorInfo.IsInterpActor()) )
				{
					for ( INT i = 0 ; i < InterpActors.Num() ; ++i )
					{
						if ( InterpActors(i).StaticMesh == ActorInfo.StaticMesh )
						{
							bSelectActor = TRUE;
							break;
						}
					}
				}
				if ( !bSelectActor && (bAllClasses || ActorInfo.IsKActor()) )
				{
					for ( INT i = 0 ; i < KActors.Num() ; ++i )
					{
						if ( KActors(i).StaticMesh == ActorInfo.StaticMesh )
						{
							bSelectActor = TRUE;
							break;
						}
					}
				}
				if ( !bSelectActor && (bAllClasses || ActorInfo.IsFracMeshActor()) )
				{
					for ( INT i = 0 ; i < FracMeshActors.Num() ; ++i )
					{
						if ( FracMeshActors(i).StaticMesh == ActorInfo.StaticMesh )
						{
							bSelectActor = TRUE;
							break;
						}
					}
				}

				if ( bSelectActor )
				{
					SelectActor( Actor, TRUE, NULL, FALSE );
				}
			}
		}
	}

	NoteSelectionChange();
}

/**
* Select all actors that have the same skeletal mesh assigned to them as the selected ones.
*
* @param bAllClasses		If TRUE, also select all actors whose meshes match.
*/
void UUnrealEdEngine::edactSelectMatchingSkeletalMesh(UBOOL bAllClasses)
{
	TArray<USkeletalMesh*> SelectedMeshes;
	UBOOL bSelectSkelMeshActors = FALSE;
	UBOOL bSelectKAssets = FALSE;
	UBOOL bSelectPawns = FALSE;

	// Make a list of skeletal meshes of selected actors, and note what classes we have selected.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// Look for SkelMeshActor
		ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Actor);
		if(SkelMeshActor && SkelMeshActor->SkeletalMeshComponent)
		{
			bSelectSkelMeshActors = TRUE;
			SelectedMeshes.AddUniqueItem(SkelMeshActor->SkeletalMeshComponent->SkeletalMesh);
		}

		// Look for Kasset
		AKAsset* KAsset = Cast<AKAsset>(Actor);
		if(KAsset && KAsset->SkeletalMeshComponent)
		{
			bSelectKAssets = TRUE;
			SelectedMeshes.AddUniqueItem(KAsset->SkeletalMeshComponent->SkeletalMesh);
		}

		// Look for Pawn
		APawn* Pawn = Cast<APawn>(Actor);
		if(Pawn && Pawn->Mesh)
		{
			bSelectPawns = TRUE;
			SelectedMeshes.AddUniqueItem(Pawn->Mesh->SkeletalMesh);
		}
	}

	// If desired, select all class types
	if(bAllClasses)
	{
		bSelectSkelMeshActors = TRUE;
		bSelectKAssets = TRUE;
		bSelectPawns = TRUE;
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	// Loop through all non-hidden actors in visible levels, selecting those that have one of the skeletal meshes in the list.
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if ( !Actor->IsHiddenEd() )
		{
			UBOOL bSelectActor = FALSE;

			if(bSelectSkelMeshActors)
			{
				ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Actor);
				if( SkelMeshActor && 
					SkelMeshActor->SkeletalMeshComponent && 
					SelectedMeshes.ContainsItem(SkelMeshActor->SkeletalMeshComponent->SkeletalMesh) )
				{
					bSelectActor = TRUE;
				}
			}

			if(bSelectKAssets)
			{
				AKAsset* KAsset = Cast<AKAsset>(Actor);
				if( KAsset && 
					KAsset->SkeletalMeshComponent &&
					SelectedMeshes.ContainsItem(KAsset->SkeletalMeshComponent->SkeletalMesh) )
				{
					bSelectActor = TRUE;
				}
			}

			if(bSelectPawns)
			{
				APawn* Pawn = Cast<APawn>(Actor);
				if( Pawn && 
					Pawn->Mesh &&
					SelectedMeshes.ContainsItem(Pawn->Mesh->SkeletalMesh) )
				{
					bSelectActor = TRUE;
				}
			}

			if ( bSelectActor )
			{
				SelectActor( Actor, TRUE, NULL, FALSE );
			}
		}
	}

	NoteSelectionChange();
}

/**
 * Select all material actors that have the same material assigned to them as the selected ones.
 */
void UUnrealEdEngine::edactSelectMatchingMaterial()
{
	// Set for fast lookup of used materials.
	TSet<UMaterialInterface*> MaterialsInSelection;

	// For each selected actor, find all the materials used by this actor.
	for ( FSelectionIterator ActorItr( GetSelectedActorIterator() ) ; ActorItr ; ++ActorItr )
	{
		AActor* CurrentActor = Cast<AActor>( *ActorItr );

		if( CurrentActor )
		{
			// Find the materials by iterating over every primitive component.
			for (INT ComponentIdx = 0; ComponentIdx < CurrentActor->Components.Num(); ComponentIdx++)
			{
				UPrimitiveComponent* CurrentComponent = Cast<UPrimitiveComponent>( CurrentActor->Components(ComponentIdx) );
				if ( CurrentComponent )
				{
					TArray<UMaterialInterface*> UsedMaterials;
					CurrentComponent->GetUsedMaterials( UsedMaterials );
					MaterialsInSelection.Add( UsedMaterials );
				}
			}
		}
	}

	// Now go over every actor and see if any of the actors are using any of the materials that 
	// we found above.
	for( FActorIterator ActorIt; ActorIt; ++ActorIt )
	{
		AActor* Actor = *ActorIt;

		// Do not bother checking hidden actors
		if( !Actor->IsHiddenEd() )
		{
			const INT NumComponents = Actor->Components.Num();
			for (INT ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex )
			{
				UPrimitiveComponent* CurrentComponent = Cast<UPrimitiveComponent>( Actor->Components(ComponentIndex) );
				if ( CurrentComponent )
				{
					TArray<UMaterialInterface*> UsedMaterials;
					CurrentComponent->GetUsedMaterials( UsedMaterials );
					const INT NumMaterials = UsedMaterials.Num();
					// Iterate over every material we found so far and see if its in the list of materials used by selected actors.
					for( INT MatIndex = 0; MatIndex < NumMaterials; ++MatIndex )
					{
						UMaterialInterface* Material = UsedMaterials( MatIndex );
						// Is this material used by currently selected actors?
						if( MaterialsInSelection.Find( Material ) )
						{
							SelectActor( Actor, TRUE, NULL, FALSE );
							// We dont need to continue searching as this actor has already been selected
							MatIndex = NumMaterials;
							ComponentIndex = NumComponents;
						}
					}
				}
			}
		}
	}

	NoteSelectionChange();
}

/**
 * Select all emitter actors that have the same particle system template assigned to them as the selected ones.
 */
void UUnrealEdEngine::edactSelectMatchingEmitter()
{
	TArray<UParticleSystem*> SelectedParticleSystemTemplates;

	// Check all of the currently selected actors to find the relevant particle system templates to use to match
	for ( FSelectionIterator SelectedIterator( GetSelectedActorIterator() ) ; SelectedIterator ; ++SelectedIterator )
	{
		AActor* Actor = static_cast<AActor*>( *SelectedIterator );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		AEmitter* Emitter = Cast<AEmitter>( Actor );
		
		if ( Emitter && Emitter->ParticleSystemComponent && Emitter->ParticleSystemComponent->Template )
		{
			SelectedParticleSystemTemplates.AddUniqueItem( Emitter->ParticleSystemComponent->Template );
		}
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	// Iterate over all of the non-hidden actors, selecting those who have a particle system template that matches one from the previously-found list
	for( FActorIterator ActorIterator; ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		if ( !Actor->IsHiddenEd() )
		{
			AEmitter* ActorAsEmitter = Cast<AEmitter>( Actor );
			if ( ActorAsEmitter && ActorAsEmitter->ParticleSystemComponent && SelectedParticleSystemTemplates.ContainsItem( ActorAsEmitter->ParticleSystemComponent->Template ) )
			{
				SelectActor( Actor, TRUE, NULL, FALSE );
			}
		}
	}

	NoteSelectionChange();
}

/**
* Select all proc buildings that use the same ruleset as this one.
*/
void UUnrealEdEngine::edactSelectMatchingProcBuildingsByRuleset()
{
	TArray<UProcBuildingRuleset*> SelectedProcBuildingRulesets;

	// Check all of the currently selected actors to find the relevant rulesets to use to match
	for ( FSelectionIterator SelectedIterator( GetSelectedActorIterator() ) ; SelectedIterator ; ++SelectedIterator )
	{
		AActor* Actor = static_cast<AActor*>( *SelectedIterator );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		AProcBuilding* ProcBuilding = Cast<AProcBuilding>( Actor );

		if ( ProcBuilding && ProcBuilding->Ruleset )
		{
			SelectedProcBuildingRulesets.AddUniqueItem( ProcBuilding->Ruleset );
		}
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	// Iterate over all of the non-hidden actors, selecting those who have a ruleset from the previous list
	for( FActorIterator ActorIterator; ActorIterator; ++ActorIterator )
	{
		AActor* Actor = *ActorIterator;
		if ( !Actor->IsHiddenEd() )
		{
			AProcBuilding* ActorAsProcBuilding = Cast<AProcBuilding>( Actor );
			if ( ActorAsProcBuilding && ActorAsProcBuilding->Ruleset && SelectedProcBuildingRulesets.ContainsItem( ActorAsProcBuilding->Ruleset ) )
			{
				SelectActor( Actor, TRUE, NULL, FALSE );
			}
		}
	}

	NoteSelectionChange();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Selects actors in the current level based on whether or not they're referenced by Kismet.
 *
 * @param	bReferenced		If TRUE, select actors referenced by Kismet; if FALSE, select unreferenced actors.
 * @param	bCurrent			If TRUE, select actors in current level; if FALSE, select actors from all levels.
 */
void UUnrealEdEngine::edactSelectKismetReferencedActors(UBOOL bReferenced, UBOOL bCurrent)
{
	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	SelectNone( FALSE, TRUE );
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		if ( !bCurrent || Actor->GetLevel() == GWorld->CurrentLevel )
		{
			const UBOOL bReferencedByKismet = Actor->IsReferencedByKismet();
			if ( bReferencedByKismet == bReferenced )
			{
				SelectActor( Actor, TRUE, NULL, FALSE );
			}
		}
	}

	NoteSelectionChange();
}

/**
 * Select the relevant lights for all selected actors
 */
void UUnrealEdEngine::edactSelectRelevantLights(UBOOL bDominantOnly)
{
	TArray<ALight*> RelevantLightList;
	// Make a list of selected actors with static meshes.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if (Actor->GetLevel() == GWorld->CurrentLevel)
		{
			// Gather static lighting info from each of the actor's components.
			for(INT ComponentIndex = 0;ComponentIndex < Actor->AllComponents.Num();ComponentIndex++)
			{
				UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->AllComponents(ComponentIndex));
				if (Primitive)
				{
					// Primitives with light environments have to be handled specially, 
					// Since lights that are composited into the DLE won't show up as influences
					if (Primitive->LightEnvironment && Primitive->LightEnvironment->IsEnabled())
					{
						Primitive->LightEnvironment->AddRelevantLights(RelevantLightList, bDominantOnly);
					}
					else
					{
						TArray<const ULightComponent*> RelevantLightComponents;
						GWorld->Scene->GetRelevantLights(Primitive, &RelevantLightComponents);

						for (INT LightComponentIndex = 0; LightComponentIndex < RelevantLightComponents.Num(); LightComponentIndex++)
						{
							const ULightComponent* LightComponent = RelevantLightComponents(LightComponentIndex);
							ALight* LightOwner = Cast<ALight>(LightComponent->GetOwner());
							if (LightOwner && (!bDominantOnly || IsDominantLightType(LightComponent->GetLightType())))
							{
								RelevantLightList.AddUniqueItem(LightOwner);
							}
						}
					}
				}
			}
		}
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	SelectNone( FALSE, TRUE );

	debugf(TEXT("Found %d relevant lights!"), RelevantLightList.Num());
	for (INT LightIdx = 0; LightIdx < RelevantLightList.Num(); LightIdx++)
	{
		ALight* Light = RelevantLightList(LightIdx);
		if (Light)
		{
			SelectActor(Light, TRUE, NULL, FALSE);
			debugf(TEXT("\t%s"), *(Light->GetPathName()));
		}
	}

	NoteSelectionChange();
}

void UUnrealEdEngine::SelectMatchingSpeedTrees()
{
	TArray<USpeedTree*> SelectedSpeedTrees;

	// Make a list of selected SpeedTrees.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		ASpeedTreeActor* SpeedTreeActor = Cast<ASpeedTreeActor>(*It);
		if(SpeedTreeActor)
		{
			if(SpeedTreeActor->SpeedTreeComponent->SpeedTree)
			{
				SelectedSpeedTrees.AddUniqueItem(SpeedTreeActor->SpeedTreeComponent->SpeedTree);
			}
		}
	}

	USelection* SelectedActors = GetSelectedActors();
	SelectedActors->Modify();

	// Loop through all non-hidden actors in visible levels, selecting those that have one of the
	// SpeedTrees in the list.
	for( FActorIterator It; It; ++It )
	{
		ASpeedTreeActor* SpeedTreeActor = Cast<ASpeedTreeActor>(*It);
		if(SpeedTreeActor && !SpeedTreeActor->IsHiddenEd())
		{
			if(SelectedSpeedTrees.ContainsItem(SpeedTreeActor->SpeedTreeComponent->SpeedTree))
			{
				SelectActor(SpeedTreeActor,TRUE,NULL,FALSE);
			}
		}
	}

	NoteSelectionChange();
}

/**
* Align the origin with the current grid.
*/
void UUnrealEdEngine::edactAlignOrigin()
{
	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;

	// Apply transformations to all selected brushes.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( Actor->IsABrush() )
		{
			ABrush* Brush = Cast<ABrush>( Actor );
			LevelDirtyCallback.Request();

			Brush->PreEditChange(NULL);
			Brush->Modify();

			//Snap the location of the brush to the grid
			Brush->Location.X = appRound( Brush->Location.X  / Constraints.GetGridSize() ) * Constraints.GetGridSize();
			Brush->Location.Y = appRound( Brush->Location.Y  / Constraints.GetGridSize() ) * Constraints.GetGridSize();
			Brush->Location.Z = appRound( Brush->Location.Z  / Constraints.GetGridSize() ) * Constraints.GetGridSize();

			//Update EditorMode locations to match the new brush location
			GEditorModeTools().PivotLocation = Brush->Location;
			GEditorModeTools().SnappedLocation = Brush->Location;
			GEditorModeTools().GridBase = Brush->Location;


			Brush->Brush->BuildBound();
			Brush->PostEditChange();
		}
	}
}

/**
 * Align all vertices with the current grid.
 */
void UUnrealEdEngine::edactAlignVertices()
{
	// Fires CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied LevelDirtyCallback;
	
	//Before aligning verts, align the origin with the grid
	edactAlignOrigin();

	// Apply transformations to all selected brushes.
	for ( FSelectionIterator It( GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( Actor->IsABrush() )
		{
			ABrush* Brush = Cast<ABrush>( Actor );
			LevelDirtyCallback.Request();

			Brush->PreEditChange(NULL);
			Brush->Modify();

			// Snap each vertex in the brush to an integer grid.
			UPolys* Polys = Brush->Brush->Polys;
			for( INT PolyIdx=0; PolyIdx<Polys->Element.Num(); PolyIdx++ )
			{
				FPoly* Poly = &Polys->Element(PolyIdx);
				for( INT VertIdx=0; VertIdx<Poly->Vertices.Num(); VertIdx++ )
				{
					// Snap each vertex to the nearest grid.
					Poly->Vertices(VertIdx).X = appRound( ( Poly->Vertices(VertIdx).X + Brush->Location.X )  / Constraints.GetGridSize() ) * Constraints.GetGridSize() - Brush->Location.X;
					Poly->Vertices(VertIdx).Y = appRound( ( Poly->Vertices(VertIdx).Y + Brush->Location.Y )  / Constraints.GetGridSize() ) * Constraints.GetGridSize() - Brush->Location.Y;
					Poly->Vertices(VertIdx).Z = appRound( ( Poly->Vertices(VertIdx).Z + Brush->Location.Z )  / Constraints.GetGridSize() ) * Constraints.GetGridSize() - Brush->Location.Z;
				}

				// If the snapping resulted in an off plane polygon, triangulate it to compensate.
				if( !Poly->IsCoplanar() || !Poly->IsConvex() )
				{

					FPoly BadPoly = *Poly;
					// Remove the bad poly
					Polys->Element.Remove( PolyIdx );

					// Triangulate the bad poly
					TArray<FPoly> Triangles;
					if ( BadPoly.Triangulate( Brush, Triangles ) > 0 )
					{
						// Add all new triangles to the brush
						for( INT TriIdx = 0 ; TriIdx < Triangles.Num() ; ++TriIdx )
						{
							Polys->Element.AddItem( Triangles(TriIdx) );
						}
					}
					
					PolyIdx = -1;
				}
				else
				{
					if( RecomputePoly( Brush, &Polys->Element(PolyIdx) ) == -2 )
					{
						PolyIdx = -1;
					}

					// Determine if we are in geometry edit mode.
					if ( GEditorModeTools().IsModeActive(EM_Geometry) )
					{
						// If we are in geometry mode, go through the list of geometry objects
						// and find our current brush and update its source data as it might have changed 
						// in RecomputePoly
						FEdModeGeometry* GeomMode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
						FEdModeGeometry::TGeomObjectIterator It = GeomMode->GeomObjectItor();
						for( It; It; ++It )
						{
							FGeomObject* Object = *It;
							if( Object->GetActualBrush() == Brush )
							{
								// We found our current brush, update the geometry object's data
								Object->GetFromSource();
								break;
							}
						}
					}
				}
			}

			Brush->Brush->BuildBound();

			Brush->PostEditChange();
		}
	}
}

PRAGMA_ENABLE_OPTIMIZATION

