/*=============================================================================
	EditorLevelUtils.cpp: Editor-specific level management routines
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EditorLevelUtils.h"

#include "BusyCursor.h"
#include "DlgGenericComboEntry.h"
#include "EngineProcBuildingClasses.h"
#include "FileHelpers.h"
#include "LevelBrowser.h"
#include "LevelUtils.h"


namespace EditorLevelUtils
{
	/** Global: True if we're currently updating levels for actors */
	static UBOOL GIsUpdatingLevelsForActors = FALSE;


	/**
	 * Returns true if we're current in the middle of updating levels for actors.  This is used to
	 * prevent re-entrancy problems where 'actor moved' callbacks and fire off as we're moving
	 * actors between levels and such.
	 *
	 * @return	True if we're currently updating levels
	 */
	UBOOL IsCurrentlyUpdatingLevelsForActors()
	{
		return GIsUpdatingLevelsForActors;	
	}


	/**
	 * Tries to find the best possible level for a level grid volume actor based on spatial overlap
	 *
	 * @param	InActor		The actor to find the best level for
	 *
	 * @return	The level streaming object that represents the best level we could find for this actor
	 */
	ULevelStreaming* FindBestLevelForActorInLevelGridVolume( AActor* InActor )
	{
		// Is this actor in a level that's associated with a grid volume?
		ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( InActor->GetLevel() );
		check( LevelStreaming != NULL );
		check( LevelStreaming->EditorGridVolume->IsActorMemberOfGrid( InActor ) );


		// Find the best grid cell to place the actor in
		ULevelStreaming* BestLevelStreaming = NULL;
		{
			// Grab this actor's world space AABB
			FBox ActorAABB;
			InActor->GetComponentsBoundingBox( ActorAABB );	// Out

			// If the actor returns a null or zero-volume bounds then use the actor's origin by default
			if( ActorAABB.GetVolume() < KINDA_SMALL_NUMBER ||
				( ActorAABB.Min == FVector::ZeroVector && ActorAABB.Max == FVector::ZeroVector ) )
			{
				ActorAABB.Min = ActorAABB.Max = InActor->Location;
			}


			// Is this actor based on another actor?
			if( InActor->Base != NULL )
			{
				ULevel* BaseActorLevel = InActor->Base->GetLevel();
				if( BaseActorLevel != NULL )
				{
					// Always place actor in it's base actor's level if we can
					BestLevelStreaming = FLevelUtils::FindStreamingLevel( BaseActorLevel );
				}
			}


			if( BestLevelStreaming == NULL )
			{
				const UBOOL bMustOverlap = FALSE;
				FLevelGridCellCoordinate BestGridCell;
				if( ensure( LevelStreaming->EditorGridVolume->FindBestGridCellForBox( ActorAABB, bMustOverlap, BestGridCell ) ) )
				{
					// Now find the actual level that goes with this grid cell
					BestLevelStreaming = LevelStreaming->EditorGridVolume->FindLevelForGridCell( BestGridCell );
					if( BestLevelStreaming == NULL )
					{
						// No level has been created for this grid cell yet.  We'll create one now.
						FGridAndCellCoordinatePair GridAndCellPair;
						GridAndCellPair.GridVolume = LevelStreaming->EditorGridVolume;
						GridAndCellPair.CellCoordinate = BestGridCell;
						ULevel* CreatedLevel = CreateLevelForGridCell( GridAndCellPair );
						if( CreatedLevel != NULL )
						{
							BestLevelStreaming = LevelStreaming->EditorGridVolume->FindLevelForGridCell( BestGridCell );
						}
						else
						{
							// Was unable to create a new level
						}
					}
				}
			}
		}


		if( ensure( BestLevelStreaming != NULL && BestLevelStreaming->LoadedLevel != NULL ) )
		{
			return BestLevelStreaming;
		}

		return NULL;
	}


	
	/**
	 * Moves the specified list of actors to the specified level
	 *
	 * @param	ActorsToMove		List of actors to move
	 * @param	DestLevelStreaming	The level streaming object associated with the destination level
	 * @param	OutNumMovedActors	The number of actors that were successfully moved to the new level
	 */
	void MovesActorsToLevel( TLookupMap< AActor* >& ActorsToMove, ULevelStreaming* DestLevelStreaming, INT& OutNumMovedActors )
	{
		OutNumMovedActors = 0;


		// Backup the current contents of the clipboard string as we'll be using cut/paste features to move actors
		// between levels and this will trample over the clipboard data.
		const FString OriginalClipboardContent = appClipboardPaste();

		check( DestLevelStreaming != NULL );
		ULevel* DestLevel = DestLevelStreaming->LoadedLevel;
		check( DestLevel != NULL );


		// Save the current level so we can restore it later
		ULevel* OldCurrentLevel = GWorld->CurrentLevel;

		
		// Deselect all actors
		GEditor->Exec(TEXT("ACTOR SELECT NONE"));


		// Set the current level to the destination level.  Any 'pasted' actors will land in this level.
		GWorld->CurrentLevel = DestLevel;
		const FString& NewLevelName = DestLevelStreaming->PackageName.ToString();


		for( TLookupMap< AActor* >::TIterator CurActorIt( ActorsToMove ); CurActorIt; ++CurActorIt )
		{
			AActor* CurActor = CurActorIt.Key();
			check( CurActor != NULL );

			if( !FLevelUtils::IsLevelLocked( DestLevel ) && !FLevelUtils::IsLevelLocked( CurActor ) )
			{
				ULevelStreaming* ActorPrevLevel = FLevelUtils::FindStreamingLevel( CurActor->GetLevel() );
				const FString& PrevLevelName = ActorPrevLevel != NULL ? ActorPrevLevel->PackageName.ToString() : CurActor->GetLevel()->GetName();
				warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Moving %s from %s to %s" ), *CurActor->GetName(), *PrevLevelName, *NewLevelName );

				// Select this actor
				GEditor->SelectActor( CurActor, TRUE, NULL, FALSE, TRUE );
			}
			else
			{
				// Either the source or destination level was locked!
				// @todo: Display warning
			}
		}


		if( GEditor->GetSelectedActorCount() > 0 )
		{
			// @todo: Perf: Not sure if this is needed here.
			GEditor->NoteSelectionChange();

			// Move the actors!
			const UBOOL bUseCurrentLevelGridVolume = FALSE;
			GEditor->MoveSelectedActorsToCurrentLevel( bUseCurrentLevelGridVolume );

			// The moved (pasted) actors will now be selected
			OutNumMovedActors += GEditor->GetSelectedActorCount();
		}


		// Restore the old current level
		GWorld->CurrentLevel = OldCurrentLevel;


		// Restore the original clipboard contents
		appClipboardCopy( *OriginalClipboardContent );
	}



	/**
	 * Finds the level grid volume that an actor belongs to and returns it
	 *
	 * @param	InActor		The actor to find the level grid volume for
	 *
	 * @return	The level grid volume this actor belong to, or NULL if the actor doesn't belong to a level grid volume
	 */
	ALevelGridVolume* GetLevelGridVolumeForActor( AActor* InActor )
	{
		check( InActor != NULL );

		// Is this actor in a level that's associated with a grid volume?
		ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( InActor->GetLevel() );
		if( LevelStreaming != NULL && LevelStreaming->EditorGridVolume != NULL )
		{
			// Actor is in a level grid volume, so return that
			return LevelStreaming->EditorGridVolume;
		}

		// Actor does not belong to a level grid volume
		return NULL;
	}



	/**
	 * For each actor in the specified list, checks to see if the actor needs to be moved to
	 * a different grid volume level, and if so queues the actor to be moved
	 *
	 * @param	InActorsToProcess	List of actors to update level membership for.  Note that this list may be modified by the function.
	 */
	void UpdateLevelsForActorsInLevelGridVolumes( TArray< AActor* >& InActorsToProcess )
	{
		// Check for re-entrancy!
		check( !IsCurrentlyUpdatingLevelsForActors() );
		GIsUpdatingLevelsForActors = TRUE;


		warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Updating levels for %i actors" ), InActorsToProcess.Num() );

		// Build a list of actors to process, with parent actors appearing before their children
		// Also, make sure that all based actors are also in this list.  If a parent actor is moved
		// to a new level, its children may need to be relocated as well!
		// @todo perf: The following operation is effectively O(N^2)
		TLookupMap< AActor* > OrderedActorsToProcess;
		INT NumActorsInGridVolumes = 0;
		{
			// NOTE: We'll grow this list as we're iterating over it to catch child actors
			const INT OriginalActorCount = InActorsToProcess.Num();
			for( INT CurActorIndex = 0; CurActorIndex < InActorsToProcess.Num(); ++CurActorIndex )
			{
				AActor* CurActor = InActorsToProcess( CurActorIndex );

				// Make sure this actor isn't a streaming volume itself!
				// Also, ignore other actor types (such as ProcBuilding low LOD actors, which are destroyed
				// any re-created automatically by the editor.)
				if( !CurActor->IsA( ALevelStreamingVolume::StaticClass() ) &&
					!CurActor->IsA( ALevelGridVolume::StaticClass() ) &&
					!CurActor->IsA( AProcBuilding_SimpleLODActor::StaticClass() ) &&
					!CurActor->IsA( ADefaultPhysicsVolume::StaticClass() ) &&
					!CurActor->IsA( AWorldInfo::StaticClass() ) &&
					!CurActor->IsABuilderBrush() )
				{
					// Is this actor in a level that's associated with a grid volume?
					if( GetLevelGridVolumeForActor( CurActor ) != NULL )
					{
						++NumActorsInGridVolumes;

						// Keep track of where our children lie within the list, so that we can make sure
						// to insert the parent before its children
						UINT MinChildListIndex = (UINT)INDEX_NONE;

						// Find this actor's children
						TArray<AActor*> ChildActors;
						for( FActorIterator AllActorsIt; AllActorsIt; ++AllActorsIt )
						{
							AActor* TestActor = *AllActorsIt;

							// Make sure the actor we're testing isn't our current actor, and that it's actually
							// parented to another actor
							if( TestActor != CurActor && !TestActor->bDeleteMe && TestActor->Base != NULL )
							{
								// Is this my direct child?
								if( TestActor->Base == CurActor )
								{
									// Add this actor to our list of actors to process, if we don't already have it
									// @todo perf: Use a TLookupMap for this list as well to speed up this test?
									if( !InActorsToProcess.ContainsItem( TestActor ) )
									{
										InActorsToProcess.AddItem( TestActor );
									}

									// Update min child list index
									INT* ExistingIndex = OrderedActorsToProcess.Find( CurActor );
									if( ExistingIndex != NULL )
									{
										// @todo: Not sure if this code is ever hit in practice!
										const UINT ChildIndexInList = (UINT)*ExistingIndex;
										if( ChildIndexInList < MinChildListIndex )
										{
											MinChildListIndex = ChildIndexInList;
										}
									}
								}
							}
						}

						// Now add the current actor to the list, making sure that it appears before children.
						// Also note that it's possible that we've already been added (as a child of another actor.)
						// In that case, we would have already been added to the list after our parents.
						if( !OrderedActorsToProcess.HasKey( CurActor ) )
						{
							if( MinChildListIndex != (UINT)INDEX_NONE )
							{
								OrderedActorsToProcess.InsertItem( CurActor, MinChildListIndex );
							}
							else
							{
								OrderedActorsToProcess.AddItem( CurActor );
							}
						}
					}
				}
			}

			const INT NumBasedActorsAdded = InActorsToProcess.Num() - OriginalActorCount;
			if( NumBasedActorsAdded > 0 )
			{
				warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Found an additional %i actor(s) that were parented to actors in this list" ), NumBasedActorsAdded );
			}
		}


		warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Processing %i actors (includes based actors)" ), OrderedActorsToProcess.Num() );

		typedef TMap< ULevelStreaming*, TLookupMap<AActor*> > FLevelToActorsMap;
		FLevelToActorsMap LevelToActorsMap;

		// Iterate over actors that need to be processed.  Note that this list is ordered such that parents
		// always appear before their children
		INT TotalActorCount = 0;
		INT MovedActorCount = 0;
		for( TLookupMap< AActor* >::TIterator CurActorIt( OrderedActorsToProcess ); CurActorIt; ++CurActorIt )
		{
			AActor* CurActor = CurActorIt.Key();

			ULevelStreaming* BestLevelStreaming = NULL;


			// Is this actor based on another actor?
			if( CurActor->Base != NULL )
			{
				// Check to see if the base actor is in our already-processed list of actors.  Remember, we
				// always process parents before children so if the parent is due to be moved, it will definitely
				// be in this list.  This handles the case where the base actor is due to be moved to a new level
				// but hasn't been moved yet.  We want attached actors to end up in the same level as their parent!
				{
					for( FLevelToActorsMap::TIterator LevelIt( LevelToActorsMap ); LevelIt; ++LevelIt )
					{
						ULevelStreaming* CurLevelStreaming = LevelIt.Key();
						check( CurLevelStreaming != NULL );

						// Note: This array is ordered such that parent actors appear before their children
						TLookupMap< AActor* >& ActorsArray = LevelIt.Value();
						if( ActorsArray.HasKey( CurActor->Base ) )
						{
							// Found the parent actor!  We'll make sure the child is placed into the same level
							BestLevelStreaming = CurLevelStreaming;
							break;
						}
					}
				}
			}


			if( BestLevelStreaming == NULL )
			{
				// Find the best grid cell to place the actor in
				BestLevelStreaming = FindBestLevelForActorInLevelGridVolume( CurActor );
			}


			if( BestLevelStreaming != NULL && BestLevelStreaming->LoadedLevel != NULL )
			{
				// Does the actor need to be moved to a new level?
				ULevel* BestLevel = BestLevelStreaming->LoadedLevel;
				if( CurActor->GetLevel() != BestLevel )
				{
					if( !LevelToActorsMap.HasKey( BestLevelStreaming ) )
					{
						LevelToActorsMap.Set( BestLevelStreaming, TLookupMap< AActor* >() );
					}

					// Note that its important to maintain the order of the actors in the per-level
					// actor array, as parents must appear in the list before children
					TLookupMap< AActor* >& ActorsArray = *LevelToActorsMap.Find( BestLevelStreaming );
					if( !ActorsArray.HasKey( CurActor ) )
					{
						ActorsArray.AddItem( CurActor );
					}
				}
			}

			++TotalActorCount;
		}


		// For each destination level, move actors to that level
		for( FLevelToActorsMap::TIterator LevelIt( LevelToActorsMap ); LevelIt; ++LevelIt )
		{
			ULevelStreaming* DestLevelStreaming = LevelIt.Key();
			check( DestLevelStreaming != NULL );

			// Note: This array is ordered such that parent actors appear before their children
			TLookupMap< AActor* >& ActorsArray = LevelIt.Value();

			// Move actors to the level!
			INT NumActorsMovedToLevel = 0;
			MovesActorsToLevel( ActorsArray, DestLevelStreaming, NumActorsMovedToLevel );

			MovedActorCount += NumActorsMovedToLevel;
		}

		warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Processed %i actors in grid volumes (of %i total actors in world)" ), NumActorsInGridVolumes, TotalActorCount );
		if( MovedActorCount > 0 )
		{
			warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Moved %i actors to a different level" ), MovedActorCount );
		}
		else
		{
			warnf( NAME_DevLevelTools, TEXT( "AutoLevel: No actors were moved" ), MovedActorCount );
		}
		warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Finished updating levels for %i actors" ), InActorsToProcess.Num() );


		if( MovedActorCount > 0 )
		{
			// Queue a level browser update because the actor counts may have changed
			GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
		}


		GIsUpdatingLevelsForActors = FALSE;
	}


	/**
	 * Scans all actors in memory and checks to see if each actor needs to be moved to a different
	 * grid volume level, and if so queues the actor to be moved
	 */
	void UpdateLevelsForAllActors()
	{
		// Never move actors around while Matinee is open as their locations may be temporary
		if( !GEditorModeTools().IsModeActive( EM_InterpEdit ) )
		{
			warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Updating levels for all actors" ) );


			// Build up a list of actors to process
			// @todo gridvolume: Multiple GCs will happen in this stack frame which may invalidate these ptrs!
			TArray< AActor* > ActorsToProcess;
			for( FActorIterator CurActorIt; CurActorIt; ++CurActorIt )
			{
				AActor* CurActor = *CurActorIt;
				if( CurActor != NULL )
				{
					ActorsToProcess.AddItem( CurActor );
				}
			}

			// No need to gather based actors since we're already passing in every actor!
			UpdateLevelsForActorsInLevelGridVolumes( ActorsToProcess );

			warnf( NAME_DevLevelTools, TEXT( "AutoLevel: Finished updating levels for all actors" ) );
		}
	}


	static TArray<FString> GStreamingMethodStrings;
	static TArray<UClass*> GStreamingMethodClassList;

	/**
	 * Initializes the list of possible level streaming methods. 
	 * Does nothing if the lists are already initialized.
	 */
	void InitializeStreamingMethods()
	{
		check( GStreamingMethodStrings.Num() == GStreamingMethodClassList.Num() );
		if ( GStreamingMethodClassList.Num() == 0 )
		{
			// Assemble a list of possible level streaming methods.
			for ( TObjectIterator<UClass> It ; It ; ++It )
			{
				if ( It->IsChildOf( ULevelStreaming::StaticClass() ) &&
					(It->ClassFlags & CLASS_EditInlineNew) &&
					!(It->ClassFlags & CLASS_Hidden) &&
					!(It->ClassFlags & CLASS_Abstract) &&
					!(It->ClassFlags & CLASS_Deprecated) &&
					!(It->ClassFlags & CLASS_Transient) )
				{
					const FString ClassName( It->GetName() );
					// Strip the leading "LevelStreaming" text from the class name.
					// @todo DB: This assumes the names of all ULevelStreaming-derived types begin with the string "LevelStreaming".
					GStreamingMethodStrings.AddItem( ClassName.Mid( 14 ) );
					GStreamingMethodClassList.AddItem( *It );
				}
			}
		}
	}


	/**
	 * Adds the named level package to the world.  Does nothing if the level already exists in the world.
	 *
	 * @param	LevelPackageBaseFilename	The base filename of the level package to add.
	 * @param	OverrideLevelStreamingClass	Unless NULL, forces a specific level streaming class type instead of prompting the user to select one
	 *
	 * @return								The new level, or NULL if the level couldn't added.
	 */
	ULevel* AddLevelToWorld(const TCHAR* LevelPackageBaseFilename, UClass* OverrideLevelStreamingClass)
	{
		ULevel* NewLevel = NULL;
		UBOOL bIsPersistentLevel = (GWorld->PersistentLevel->GetOutermost()->GetName() == FString(LevelPackageBaseFilename));

		if ( bIsPersistentLevel || FLevelUtils::FindStreamingLevel( LevelPackageBaseFilename ) )
		{
			// Do nothing if the level already exists in the world.
			appMsgf( AMT_OK, *LocalizeUnrealEd("LevelAlreadyExistsInWorld") );
		}
		else
		{
			UClass* SelectedClass = OverrideLevelStreamingClass;
			if( SelectedClass == NULL )
			{
				// Ensure the set of available level streaming methods is initialized.
				InitializeStreamingMethods();

				// Display a dialog prompting the user to choose streaming method for this level.
				const FString TitleText( FString::Printf( TEXT("%s - %s"), *LocalizeUnrealEd("SelectStreamingMethod"), LevelPackageBaseFilename ) );

				WxDlgGenericComboEntry dlg( TRUE );
				dlg.SetTitleAndCaption( *TitleText, *LocalizeUnrealEd("StreamingMethod:"), FALSE );
				dlg.PopulateComboBox( GStreamingMethodStrings );
				dlg.SetSelection( TEXT("Kismet") );

				if ( dlg.ShowModal() == wxID_OK )
				{
					// Use the selection index to look up the selected level streaming type.
					SelectedClass = GStreamingMethodClassList( dlg.GetComboBox().GetSelection() );
				}
				else
				{
					// User pressed cancel, so bail
					return NULL;
				}
			}

			const FScopedBusyCursor BusyCursor;

			ULevelStreaming* StreamingLevel = static_cast<ULevelStreaming*>( UObject::StaticConstructObject( SelectedClass, GWorld, NAME_None, 0, NULL) );

			// Associate a package name.
			StreamingLevel->PackageName = LevelPackageBaseFilename;

			// Seed the level's draw color.
			StreamingLevel->DrawColor = FColor::MakeRandomColor();

			// Add the new level to worldinfo.
			GWorld->GetWorldInfo()->StreamingLevels.AddItem( StreamingLevel );
			GWorld->GetWorldInfo()->PostEditChange();
			GWorld->MarkPackageDirty();

			NewLevel = StreamingLevel->LoadedLevel;
			LevelBrowser::SetLevelVisibility( NewLevel, TRUE );

			// Create a kismet sequence for this level if one does not already exist.
			if ( NewLevel )
			{
				USequence* ExistingSequence = NewLevel->GetGameSequence();
				if ( !ExistingSequence )
				{
					// The newly added level contains no sequence -- create a new one.
					USequence* NewSequence = ConstructObject<USequence>( USequence::StaticClass(), NewLevel, TEXT("Main_Sequence"), RF_Transactional );
					GWorld->SetGameSequence( NewSequence, NewLevel );
					NewSequence->MarkPackageDirty();

					// Fire CALLBACK_RefreshEditor_Kismet when falling out of scope.
					FScopedRefreshEditor_Kismet RefreshEditor_KismetCallback;
					RefreshEditor_KismetCallback.Request();
				}
			} 
		}

		return NewLevel;
	}

	/**
	 * Removes the specified level from the world.  Refreshes.
	 *
	 * @return	TRUE	If a level was removed.
	 */
	UBOOL RemoveLevelFromWorld(ULevel* InLevel)
	{
		// Disallow for cooked packages.
		if( GWorld && GWorld->GetOutermost()->PackageFlags & PKG_Cooked )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
			return FALSE;
		}

		if( !FLevelUtils::IsLevelLocked(InLevel) )
		{
			// If the level isn't locked (which means it can be removed) unselect all actors before removing the level
			// This avoids crashing in areas that rely on getting a selected actors level. The level will be invalid after its removed.
			for( INT ActorIdx = 0; ActorIdx < InLevel->Actors.Num(); ++ActorIdx )
			{
				if( InLevel->Actors( ActorIdx ) != NULL )
				{
					GEditor->SelectActor( InLevel->Actors( ActorIdx ), FALSE, NULL, FALSE );
				}
			}
			// Update the property windows in case one was referencing an actor in this level
			GUnrealEd->UpdatePropertyWindows();
		}

		const UBOOL bRemovingCurrentLevel	= InLevel && InLevel == GWorld->CurrentLevel;
		const UBOOL bRemoveSuccessful		= FLevelUtils::RemoveLevelFromWorld( InLevel );
		if ( bRemoveSuccessful )
		{
			// Collect garbage to clear out the destroyed level
			UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

			// If the current level was removed, make the persistent level current.
			WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
			if( bRemovingCurrentLevel )
			{
				if( ensure( LevelBrowser != NULL ) )
				{
					LevelBrowser->MakeLevelCurrent( GWorld->PersistentLevel );
				}
			}

			// Do a full level browser redraw.
			if( LevelBrowser != NULL )
			{
				LevelBrowser->RequestUpdate( TRUE );
			}

			// Redraw the main editor viewports.
			GCallbackEvent->Send( CALLBACK_RedrawAllViewports );

			// refresh editor windows
			GCallbackEvent->Send( CALLBACK_RefreshEditor_AllBrowsers );

			// refresh Kismet windows
			GCallbackEvent->Send( CALLBACK_RefreshEditor_Kismet );
		}
		return bRemoveSuccessful;
	}

	/**
	 * Creates a new streaming level.
	 *
	 * @param	bMoveSelectedActorsIntoNewLevel		If TRUE, move any selected actors into the new level.
	 * @param	DefaultFilename						Optional file name for level.  If empty, the user will be prompted during the save process.
	 * @param	OverrideLevelStreamingClass	Unless NULL, forces a specific level streaming class type instead of prompting the user to select one
	 * 
	 * @return	Returns the newly created level, or NULL on failure
	 */
	ULevel* CreateNewLevel(UBOOL bMoveSelectedActorsIntoNewLevel, const FString& DefaultFilename, UClass* OverrideLevelStreamingClass )
	{
		// Disallow for cooked packages.
		if( GWorld && GWorld->GetOutermost()->PackageFlags & PKG_Cooked )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
			return NULL;
		}

		WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>( TEXT("LevelBrowser") );
		LevelBrowser->DisableUpdate();

		// Editor modes cannot be active when any level saving occurs.
		GEditorModeTools().ActivateMode( EM_Default );

		// Cache the current GWorld and clear it, then allocate a new one, and finally restore it.
		UWorld* OldGWorld = GWorld;
		GWorld = NULL;
		UWorld::CreateNew();
		UWorld* NewGWorld = GWorld;
		check(NewGWorld);

		// Save the new world to disk.
		const UBOOL bNewWorldSaved = FEditorFileUtils::SaveLevel( NewGWorld->PersistentLevel, DefaultFilename );
		FString NewPackageName;
		if ( bNewWorldSaved )
		{
			NewPackageName = GWorld->GetOutermost()->GetName();
		}

		// Restore the old GWorld and GC the new one.
		GWorld = OldGWorld;

		NewGWorld->RemoveFromRoot();
		UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		LevelBrowser->EnableUpdate();

		// If the new world was saved successfully, import it as a streaming level.
		if ( bNewWorldSaved )
		{
			ULevel* NewLevel = AddLevelToWorld( *NewPackageName, OverrideLevelStreamingClass );

			if ( bMoveSelectedActorsIntoNewLevel )
			{
				LevelBrowser->MakeLevelCurrent( NewLevel );

				const UBOOL bUseCurrentLevelGridVolume = FALSE;
				GEditor->MoveSelectedActorsToCurrentLevel( bUseCurrentLevelGridVolume );
			}

			return NewLevel;
		}
		// If the new world wasn't saved successfully, force an update of the level browser as it could
		// have become invalidated during the process
		else
		{
			LevelBrowser->Update();
		}
	
		return NULL;
	}



	/**
	 * Creates a new level for the specified grid cell
	 *
	 * @param	GridCell	The grid volume and cell to create a level for
	 *
	 * @return	The newly-created level, or NULL on failure
	 */
	ULevel* CreateLevelForGridCell( FGridAndCellCoordinatePair& GridCell )
	{
		// Disallow for cooked packages.
		if( GWorld && GWorld->GetOutermost()->PackageFlags & PKG_Cooked )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
			return NULL;
		}


		// Check to see if the level already exists for this cell
		ULevelStreaming* AlreadyExistingLevel = GridCell.GridVolume->FindLevelForGridCell( GridCell.CellCoordinate );
		if( !ensure( AlreadyExistingLevel == NULL ) )
		{
			// Level already exists.  Shouldn't be calling this function.
			return AlreadyExistingLevel->LoadedLevel;
		}


		const FString& LevelName = GridCell.GridVolume->GetLevelGridVolumeName();


		// If the grid only has a single vertical slice, then don't bother appending the Z coordinate
		// to the level file name
		FString LevelCoordinatesString;
		if( GridCell.GridVolume->Subdivisions[ 2 ] < 2 )
		{
			LevelCoordinatesString = FString::Printf(
				TEXT( "%02ix%02iy"),
				GridCell.CellCoordinate.X,
 				GridCell.CellCoordinate.Y );
		}
		else
		{
			LevelCoordinatesString = FString::Printf(
				TEXT( "%02ix%02iy%02iz"),
				GridCell.CellCoordinate.X,
 				GridCell.CellCoordinate.Y,
				GridCell.CellCoordinate.Z );
		}

		// Set a file name for the new level based on the grid volume name and cell coordinates
		const FString LevelFilename = FString::Printf(
			TEXT( "%s_%s.%s" ),
			*LevelName,
			*LevelCoordinatesString,
			*FURL::DefaultMapExt );


		// Grab the path to the persistent level
		FString PersistentLevelPath;
		if( ensure( GPackageFileCache->FindPackageFile( *GWorld->GetOutermost()->GetName(), NULL, PersistentLevelPath ) ) )
		{
			const FString LevelPath = FFilename( PersistentLevelPath ).GetPath() * LevelFilename;

			// Create the level
			const UBOOL bMoveActorsToNewLevel = FALSE;
			ULevel* NewLevel = CreateNewLevel( bMoveActorsToNewLevel, LevelPath, ULevelStreamingKismet::StaticClass() );
			if( NewLevel != NULL )
			{
				ULevelStreaming* NewLevelStreaming = FLevelUtils::FindStreamingLevel( NewLevel ); 
				check( NewLevelStreaming != NULL );


				// Point the level streaming object to our grid volume
				NewLevelStreaming->Modify();
				NewLevelStreaming->EditorGridVolume = GridCell.GridVolume;
				NewLevelStreaming->GridPosition[ 0 ] = GridCell.CellCoordinate.X;
				NewLevelStreaming->GridPosition[ 1 ] = GridCell.CellCoordinate.Y;
				NewLevelStreaming->GridPosition[ 2 ] = GridCell.CellCoordinate.Z;

				// Update the level grid volume so the visual cue in the 3D viewport will reflect
				// that we have an actual level for this grid cell now
				{
					GridCell.GridVolume->PreEditChange( NULL );

					const UBOOL bAlwaysMarkDirty = TRUE;
					GridCell.GridVolume->Modify( bAlwaysMarkDirty );

					GridCell.GridVolume->PostEditChange();
				}

				// Success!
				return NewLevel;
			}
			else
			{
				// Level creation failed
			}
		}
		else
		{
			// Couldn't get path for persistent level.   Not saved yet?
		}

		
		// Failed to create new level
		return NULL;	
	}



	/**
	 * Given a location for the new actor, determines which level the actor would best be placed in.
	 * This uses the world's 'current' level and grid volume when selecting a level.
	 *
	 * @param	InActorLocation				Location of the actor that we're interested in finding a level for
	 * @param	bUseCurrentLevelGridVolume	True if we should try to place the actor into a level associated with the world's 'current' level grid volume, if one is set
	 *
	 * @return	The best level to place the new actor into
	 */
	ULevel* GetLevelForPlacingNewActor( const FVector& InActorLocation, const UBOOL bUseCurrentLevelGridVolume )
	{
		ULevel* DesiredLevel = GWorld->CurrentLevel;
		if( GWorld->CurrentLevelGridVolume != NULL && bUseCurrentLevelGridVolume )
		{
			// Find the best grid cell to place the actor in
			FBox LocationBox( InActorLocation, InActorLocation );
			const UBOOL bMustOverlap = FALSE;
			FLevelGridCellCoordinate BestGridCell;
			if( GWorld->CurrentLevelGridVolume->FindBestGridCellForBox( LocationBox, bMustOverlap, BestGridCell ) )
			{
				// Now find the actual level that goes with this grid cell
				ULevelStreaming* BestLevel = GWorld->CurrentLevelGridVolume->FindLevelForGridCell( BestGridCell );
				if( BestLevel == NULL )
				{
					// No level has been created for this grid cell yet.  We'll create one now.
					FGridAndCellCoordinatePair GridAndCellPair;
					GridAndCellPair.GridVolume = GWorld->CurrentLevelGridVolume;
					GridAndCellPair.CellCoordinate = BestGridCell;
					ULevel* CreatedLevel = CreateLevelForGridCell( GridAndCellPair );
					if( CreatedLevel != NULL )
					{
						BestLevel = GWorld->CurrentLevelGridVolume->FindLevelForGridCell( BestGridCell );
					}
					else
					{
						// Failed to create the level
					}
				}

				// We now have the desired level!
				if( BestLevel != NULL && BestLevel->LoadedLevel != NULL )
				{
					DesiredLevel = BestLevel->LoadedLevel;
				}
			}
		}

		return DesiredLevel;
	}
}

