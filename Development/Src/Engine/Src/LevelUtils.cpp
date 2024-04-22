/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "LevelUtils.h"

/**
 * Assembles the set of all referenced worlds.
 *
 * @param	OutWorlds			[out] The set of referenced worlds.
 * @param	bIncludeGWorld		If TRUE, include GWorld in the output list.
 * @param	bOnlyEditorVisible	If TRUE, only sub-levels that should be visible in-editor are included
 */
void FLevelUtils::GetWorlds(TArray<UWorld*>& OutWorlds, UBOOL bIncludeGWorld, UBOOL bOnlyEditorVisible)
{
	OutWorlds.Empty();
	if ( bIncludeGWorld )
	{
		OutWorlds.AddUniqueItem( GWorld );
	}

	// Iterate over the worldinfo's level array to find referenced levels ("worlds"). We don't 
	// iterate over the GWorld->Levels array as that only contains currently associated levels.
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

	for ( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
		if ( StreamingLevel )
		{
			// If we asked for only sub-levels that are editor-visible, then limit our results appropriately
			UBOOL bShouldAlwaysBeLoaded = FALSE; // Cast< ULevelStreamingAlwaysLoaded >( StreamingLevel ) != NULL;
			if( !bOnlyEditorVisible || bShouldAlwaysBeLoaded || StreamingLevel->bShouldBeVisibleInEditor )
			{
				// This should always be the case for valid level names as the Editor preloads all packages.
				if ( StreamingLevel->LoadedLevel )
				{
					// Newer levels have their packages' world as the outer.
					UWorld* World = Cast<UWorld>( StreamingLevel->LoadedLevel->GetOuter() );
					if ( World )
					{
						OutWorlds.AddUniqueItem( World );
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	FindStreamingLevel methods.
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns the streaming level corresponding to the specified ULevel, or NULL if none exists.
 *
 * @param		Level		The level to query.
 * @return					The level's streaming level, or NULL if none exists.
 */
ULevelStreaming* FLevelUtils::FindStreamingLevel(ULevel* Level)
{
	ULevelStreaming* MatchingLevel = NULL;

	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* CurStreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
		if( CurStreamingLevel && CurStreamingLevel->LoadedLevel == Level )
		{
			MatchingLevel = CurStreamingLevel;
			break;
		}
	}

	return MatchingLevel;
}

/**
 * Returns the streaming level by package name, or NULL if none exists.
 *
 * @param		PackageName		Name of the package containing the ULevel to query
 * @return						The level's streaming level, or NULL if none exists.
 */
ULevelStreaming* FLevelUtils::FindStreamingLevel(const TCHAR* InPackageName)
{
	const FName PackageName( InPackageName );

	ULevelStreaming* MatchingLevel = NULL;

	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	for( INT LevelIndex = 0 ; LevelIndex< WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* CurStreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
		if( CurStreamingLevel && CurStreamingLevel->PackageName == PackageName )
		{
			MatchingLevel = CurStreamingLevel;
			break;
		}
	}

	return MatchingLevel;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	Level bounding box methods.
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Queries for the bounding box visibility a level's UStreamLevel 
 *
 * @param	Level		The level to query.
 * @return				TRUE if the level's bounding box is visible, FALSE otherwise.
 */
UBOOL FLevelUtils::IsLevelBoundingBoxVisible(ULevel* Level)
{
	if ( Level == GWorld->PersistentLevel )
	{
		return TRUE;
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	checkMsg( StreamingLevel, "Couldn't find streaming level" );

	const UBOOL bBoundingBoxVisible = StreamingLevel->bBoundingBoxVisible;
	return bBoundingBoxVisible;
}

/**
 * Toggles whether or not a level's bounding box is rendered in the editor in place
 * of the level itself.
 *
 * @param	Level		The level to query.
 */
void FLevelUtils::ToggleLevelBoundingBox(ULevel* Level)
{
	if ( !Level || Level == GWorld->PersistentLevel )
	{
		return;
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	checkMsg( StreamingLevel, "Couldn't find streaming level" );

	StreamingLevel->bBoundingBoxVisible = !StreamingLevel->bBoundingBoxVisible;

	GWorld->UpdateLevelStreaming();
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	RemoveLevelFromWorld
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Removes a level from the world.  Returns true if the level was removed successfully.
 *
 * @param	Level		The level to remove from the world.
 * @return				TRUE if the level was removed successfully, FALSE otherwise.
 */
UBOOL FLevelUtils::RemoveLevelFromWorld(ULevel* Level)
{
	if ( !Level || Level == GWorld->PersistentLevel )
	{
		return FALSE;
	}

	if ( IsLevelLocked(Level) )
	{
		appMsgf(AMT_OK, TEXT("RemoveLevelFromWorld: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		return FALSE;
	}

	INT StreamingLevelIndex = INDEX_NONE;

	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
		if( StreamingLevel && StreamingLevel->LoadedLevel == Level )
		{
			StreamingLevelIndex = LevelIndex;
			break;
		}
	}

	const UBOOL bSuccess = StreamingLevelIndex != INDEX_NONE;
	if ( bSuccess )
	{
		WorldInfo->StreamingLevels.Remove( StreamingLevelIndex );
		WorldInfo->PostEditChange();
		
		GWorld->EditorDestroyLevel(Level);
	}

	return bSuccess;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	Level locking/unlocking.
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns TRUE if the specified level is locked for edit, FALSE otherwise.
 *
 * @param	Level		The level to query.
 * @return				TRUE if the level is locked, FALSE otherwise.
 */
UBOOL FLevelUtils::IsLevelLocked(ULevel* Level)
{
	// Don't permit spawning in read only levels if they are locked
	if ( GIsEditor && !GIsEditorLoadingMap )
	{
		if ( GEngine && GEngine->bLockReadOnlyLevels )
		{
			const UPackage* pPackage = Cast<UPackage>( Level->GetOutermost() );
			if ( pPackage )
			{
				FString PackageFileName;
				if ( GPackageFileCache && GPackageFileCache->FindPackageFile( *pPackage->GetName(), NULL, PackageFileName ) )
				{
					const UBOOL bIsReadOnly = GFileManager ? GFileManager->IsReadOnly( *PackageFileName ) : FALSE;
					if( bIsReadOnly )
					{
						warnf( TEXT( "Read-only packages are locked, unlock them from the Tools menu to permit changes to level: %s" ), *pPackage->GetName() );
						return TRUE;
					}
				}
			}
		}
	}

	// PIE levels, the persistent level, and transient move levels are usually never locked.
	if ( Level->IsInPIEPackage() || Level == GWorld->PersistentLevel || Level->GetName() == TEXT("TransLevelMoveBuffer") )
	{
		return FALSE;
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	if( StreamingLevel != NULL )
	{
		return StreamingLevel->bLocked;
	}

	warnf( TEXT( "Couldn't find streaming level: %s" ), *Level->GetName() );
	return FALSE;
}
UBOOL FLevelUtils::IsLevelLocked(AActor* Actor)
{
	return Actor != NULL && !Actor->IsTemplate() && Actor->GetLevel() != NULL && IsLevelLocked(Actor->GetLevel());
}

/**
 * Sets a level's edit lock.
 *
 * @param	Level		The level to modify.
 */
void FLevelUtils::ToggleLevelLock(ULevel* Level)
{
	if ( !Level || Level == GWorld->PersistentLevel )
	{
		return;
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	checkMsg( StreamingLevel, "Couldn't find streaming level" );

	StreamingLevel->bLocked = !StreamingLevel->bLocked;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	Level loading/unloading.
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns TRUE if the level is currently loaded in the editor, FALSE otherwise.
 *
 * @param	Level		The level to query.
 * @return				TRUE if the level is loaded, FALSE otherwise.
 */
UBOOL FLevelUtils::IsLevelLoaded(ULevel* Level)
{
	if ( Level == GWorld->PersistentLevel )
	{
		// The persistent level is always loaded.
		return TRUE;
	}

	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	checkMsg( StreamingLevel, "Couldn't find streaming level" );

	// @todo: Dave, please come talk to me before implementing anything like this.
	return TRUE;
}

/**
 * Flags an unloaded level for loading.
 *
 * @param	Level		The level to modify.
 */
void FLevelUtils::MarkLevelForLoading(ULevel* Level)
{
	// If the level is valid and not the persistent level (which is always loaded) . . .
	if ( Level && Level != GWorld->PersistentLevel )
	{
		// Mark the level's stream for load.
		ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
		checkMsg( StreamingLevel, "Couldn't find streaming level" );
		// @todo: Dave, please come talk to me before implementing anything like this.
	}
}

/**
 * Flags a loaded level for unloading.
 *
 * @param	Level		The level to modify.
 */
void FLevelUtils::MarkLevelForUnloading(ULevel* Level)
{
	// If the level is valid and not the persistent level (which is always loaded) . . .
	if ( Level && Level != GWorld->PersistentLevel )
	{
		ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
		checkMsg( StreamingLevel, "Couldn't find streaming level" );
		// @todo: Dave, please come talk to me before implementing anything like this.
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//	Level visibility.
//
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns TRUE if the specified level is visible in the editor, FALSE otherwise.
 *
 * @param	StreamingLevel		The level to query.
 */
UBOOL FLevelUtils::IsLevelVisible(ULevelStreaming* StreamingLevel)
{
	const UBOOL bVisible = StreamingLevel->bShouldBeVisibleInEditor;
	return bVisible;
}

/**
 * Returns TRUE if the specified level is visible in the editor, FALSE otherwise.
 *
 * @param	Level		The level to query.
 */
UBOOL FLevelUtils::IsLevelVisible(ULevel* Level)
{
	// P-level is specially handled
	if ( Level == GWorld->PersistentLevel )
	{
		return !( GWorld->PersistentLevel->GetWorldInfo()->bHiddenEdLevel );
	}

	// Handle streaming level
	ULevelStreaming* StreamingLevel = FindStreamingLevel( Level );
	if( StreamingLevel )
	{
		return IsLevelVisible( StreamingLevel );
	}
	else
	{
		warnf( TEXT( "Couldn't find streaming level: %s" ), *Level->GetName() );
	}

	return TRUE;
}

/**
* @return		TRUE if the actor should be considered for layer visibility, FALSE otherwise.
*/
static inline UBOOL IsValidForLayerVisibility(const AActor* Actor)
{
	return ( Actor && Actor != GWorld->GetBrush() && Actor->GetClass()->GetDefaultActor()->bHiddenEd == FALSE );
}

/**
 * Sets a level's visibility in the editor.
 *
 * @param	StreamingLevel			The level to modify.
 * @param	bShouldBeVisible		The level's new visibility state.
 * @param	bForceLayersVisible		If TRUE and the level is visible, force the level's layers to be visible.
 */
void FLevelUtils::SetLevelVisibility(ULevelStreaming* StreamingLevel, ULevel* Level, UBOOL bShouldBeVisible, UBOOL bForceLayersVisible)
{
#if WITH_EDITORONLY_DATA
	// Handle the case of a streaming level
	if ( StreamingLevel )
	{
		// Set the visibility state for this streaming level.  Note that this will dirty the persistent level.
		StreamingLevel->Modify();
		StreamingLevel->bShouldBeVisibleInEditor = bShouldBeVisible;

		GWorld->UpdateLevelStreaming();

		// Force the level's layers to be visible, if desired
		if ( Level )
		{
			GCallbackEvent->Send( CALLBACK_RedrawAllViewports );

			// Iterate over the level's actors, making a list of their layers and unhiding the layers.
			TArray<FString> VisibleLayers;
			GWorld->GetWorldInfo()->VisibleLayers.ParseIntoArray( &VisibleLayers, TEXT(","), 0 );

			TArray<FString> NewLayers;
			TTransArray<AActor*>& Actors = Level->Actors;
			UBOOL bVisibleLayersNeedUpdate = FALSE;
			for ( INT ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
			{
				AActor* Actor = Actors( ActorIndex );
				if ( bShouldBeVisible && bForceLayersVisible && IsValidForLayerVisibility( Actor ) )
				{
					// Make the actor layer visible, if it's not already.
					if ( Actor->bHiddenEdLayer )
					{
						// While this action "dirties" the actor, intentionally do not call Modify() in order to prevent
						// the level from dirtying over a visibility change
						Actor->bHiddenEdLayer = FALSE;
					}

					// Add the actor's layers to the list of visible layers.
					const FString LayerName = *Actor->Layer.ToString();
					LayerName.ParseIntoArray( &NewLayers, TEXT(","), FALSE );

					for( INT x = 0 ; x < NewLayers.Num() ; ++x )
					{
						const FString& NewLayer = NewLayers( x );
						if ( !VisibleLayers.ContainsItem( NewLayer ) )
						{
							VisibleLayers.AddItem( NewLayer );
							bVisibleLayersNeedUpdate = TRUE;
						}
					}
				}

				// Set the visibility of each actor in the streaming level
				if ( Actor && !Actor->IsABuilderBrush() )
				{
					Actor->bHiddenEdLevel = !bShouldBeVisible;
					Actor->ForceUpdateComponents( FALSE, FALSE );
				}
			}

			if ( bShouldBeVisible && bForceLayersVisible )
			{
				// Only update the visible layers if required
				if ( bVisibleLayersNeedUpdate )
				{
					// Copy the visible layers list back over to worldinfo.
					GWorld->GetWorldInfo()->VisibleLayers = TEXT("");
					for( INT x = 0 ; x < VisibleLayers.Num() ; ++x )
					{
						if( GWorld->GetWorldInfo()->VisibleLayers.Len() > 0 )
						{
							GWorld->GetWorldInfo()->VisibleLayers += TEXT(",");
						}
						GWorld->GetWorldInfo()->VisibleLayers += VisibleLayers(x);
					}
				}
			}
		}
	}

	// Handle the case of the p-level
	// The p-level can't be unloaded, so its actors/BSP should just be temporarily hidden/unhidden
	// Also, intentionally do not force layers visible for the p-level
	else if ( Level && Level == GWorld->PersistentLevel )
	{
		// Set the visibility of each actor in the p-level
		for ( TArray<AActor*>::TIterator PLevelActorIter( Level->Actors ); PLevelActorIter; ++PLevelActorIter )
		{
			AActor* CurActor = *PLevelActorIter;
			if ( CurActor && !CurActor->IsABuilderBrush() )
			{
				CurActor->bHiddenEdLevel = !bShouldBeVisible;
				CurActor->ForceUpdateComponents( FALSE, FALSE );
			}
		}

		// Set the visibility of each BSP surface in the p-level
		UModel* CurLevelModel = Level->Model;
		if ( CurLevelModel )
		{
			for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel->Surfs ); SurfaceIterator; ++SurfaceIterator )
			{
				FBspSurf& CurSurf = *SurfaceIterator;
				CurSurf.bHiddenEdLevel = !bShouldBeVisible;
			}
		}
		GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
	}

	GCallbackEvent->Send( CALLBACK_RefreshEditor_LayerBrowser );
#endif // WITH_EDITORONLY_DATA
}
