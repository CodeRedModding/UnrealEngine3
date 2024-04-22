/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "LayerUtils.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper functions.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @return		TRUE if the actor can be considered by the layer browser, FALSE otherwise.
 */
static inline UBOOL IsValid(const AActor* Actor)
{
	const UBOOL bIsBuilderBrush	= Actor->IsABuilderBrush();
	const UBOOL bIsHidden		= ( Actor->GetClass()->GetDefaultActor()->bHiddenEd == TRUE );
	const UBOOL bIsValid		= !bIsHidden && !bIsBuilderBrush;

	return bIsValid;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on an individual actor.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Adds the actor to the named layer.
 *
 * @return		TRUE if the actor was added.  FALSE is returned if the
 *              actor already belongs to the layer.
 */
UBOOL FLayerUtils::AddActorToLayer(AActor* Actor, const FString& LayerName, UBOOL bModifyActor)
{
	if(	IsValid( Actor ) )
	{
		// Don't add the actor to a layer it already belongs to.
		if( !Actor->IsInLayer( *LayerName ) )
		{
			if ( bModifyActor )
			{
				Actor->Modify();
			}

			const FString CurLayer = Actor->Layer.ToString();
			Actor->Layer = FName( *FString::Printf(TEXT("%s%s%s"), *CurLayer, (CurLayer.Len() ? TEXT(","):TEXT("")), *LayerName ) );

			// Remove the actor from "None.
			const FString NoneLayer( TEXT("None") );
			if ( Actor->IsInLayer( *NoneLayer ) )
			{
				RemoveActorFromLayer( Actor, NoneLayer, FALSE );
			}

			// update per-view visibility info
			UpdateActorAllViewsVisibility(Actor);

			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Removes an actor from the specified layer.
 *
 * @return		TRUE if the actor was removed from the layer.  FALSE is returned if the
 *              actor already belonged to the layer.
 */
UBOOL FLayerUtils::RemoveActorFromLayer(AActor* Actor, const FString& LayerToRemove, UBOOL bModifyActor)
{
	if(	IsValid( Actor ) )
	{
		TArray<FString> LayerArray;
		FString LayerName = Actor->Layer.ToString();
		LayerName.ParseIntoArray( &LayerArray, TEXT(","), FALSE );

		const UBOOL bRemovedFromLayer = LayerArray.RemoveItem( LayerToRemove ) > 0;
		if ( bRemovedFromLayer )
		{
			// Reconstruct a new layer name list for the actor.
			LayerName = TEXT("");
			for( INT LayerIndex = 0 ; LayerIndex < LayerArray.Num() ; ++LayerIndex )
			{
				if( LayerName.Len() )
				{
					LayerName += TEXT(",");
				}
				LayerName += LayerArray(LayerIndex);
			}

			if ( bModifyActor )
			{
				Actor->Modify();
			}
			Actor->Layer = FName( *LayerName );

			// update per-view visibility info
			UpdateActorAllViewsVisibility(Actor);

			return TRUE;
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on selected actors.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Adds selected actors to the named layers.
 *
 * @param	LayerNames	A valid list of layer names.
 * @return				TRUE if at least one actor was added.  FALSE is returned if all selected
 *                      actors already belong to the named layers.
 */
UBOOL FLayerUtils::AddSelectedActorsToLayers(const TArray<FString>& LayerNames)
{
	UBOOL bReturnVal = FALSE;

	const FString NoneString( TEXT("None") );
	const UBOOL bAddingToNoneLayer = LayerNames.ContainsItem( NoneString );

	TArray<FString> LayerArray;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( IsValid( Actor ) )
		{
			// Break the actors layer names down and only add the ones back in that don't occur in LayerNames.
			FString LayerName = Actor->Layer.ToString();
			LayerName.ParseIntoArray( &LayerArray, TEXT(","), FALSE );

			const INT OldLayerNum = LayerArray.Num();
			const UBOOL bWasInNoneLayer = LayerArray.ContainsItem( NoneString );

			// Add layers to the list.
			for( INT x = 0 ; x < LayerNames.Num() ; ++x )
			{
				LayerArray.AddUniqueItem( LayerNames(x) );
			}

			// Were any new layers added to this actor?
			if ( OldLayerNum != LayerArray.Num() )
			{
				// Reconstruct a new layer name list for the actor.
				LayerName = TEXT("");

				// Skip over the "None" layer if the actor used to belong to that layer and we're not adding it.
				const UBOOL bSkipOverNoneLayer = bWasInNoneLayer && !bAddingToNoneLayer;

				for( INT LayerIndex = 0 ; LayerIndex < LayerArray.Num() ; ++LayerIndex )
				{
					// Skip over the "None" layer if the actor used to belong to that layer
					if ( bSkipOverNoneLayer && LayerArray(LayerIndex) == NoneString )
					{
						continue;
					}

					if( LayerName.Len() )
					{
						LayerName += TEXT(",");
					}
					LayerName += LayerArray(LayerIndex);
				}
				Actor->Modify();
				Actor->Layer = FName( *LayerName );

				// update per-view visibility info
				UpdateActorAllViewsVisibility(Actor);

				bReturnVal = TRUE;
			}
		}
	}

	return bReturnVal;
}

/**
 * Adds selected actors to the named layer.
 *
 * @param	LayerName	A layer name.
 * @return				TRUE if at least one actor was added.  FALSE is returned if all selected
 *                      actors already belong to the named layer.
 */
UBOOL FLayerUtils::AddSelectedActorsToLayer(const FString& LayerName)
{
	UBOOL bReturnVal = FALSE;

	const FString NoneLayer( TEXT("None") );
	const UBOOL bAddingToNoneLayer = (LayerName == NoneLayer);

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( IsValid( Actor ) )
		{
			// Don't add the actor to a layer it already belongs to.
			if( !Actor->IsInLayer( *LayerName ) )
			{
				// Add the layer to the actors layer list.
				Actor->Modify();

				const FString CurLayer = Actor->Layer.ToString();
				Actor->Layer = FName( *FString::Printf(TEXT("%s%s%s"), *CurLayer, (CurLayer.Len() ? TEXT(","):TEXT("")), *LayerName ) );

				// If we're not adding to the "None" layer and the actor has single membership in the "None" layer, remove the actor from "None.
				if ( !bAddingToNoneLayer && Actor->IsInLayer( *NoneLayer ) )
				{
					RemoveActorFromLayer( Actor, NoneLayer, FALSE );
				}

				// update per-view visibility info
				UpdateActorAllViewsVisibility(Actor);

				bReturnVal = TRUE;
			}
		}
	}
	return bReturnVal;
}

/**
 * Removes selected actors from the named layers.
 *
 * @param	LayerNames	A valid list of layer names.
 * @return				TRUE if at least one actor was removed.
 */
UBOOL FLayerUtils::RemoveSelectedActorsFromLayers(const TArray<FString>& LayerNames)
{
	UBOOL bReturnVal = FALSE;
	TArray<FString> LayerArray;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( IsValid( Actor ) )
		{
			// Break the actors layer names down and only add the ones back in that don't occur in LayerNames.
			FString LayerName = Actor->Layer.ToString();
			LayerName.ParseIntoArray( &LayerArray, TEXT(","), FALSE );

			UBOOL bRemovedFromLayer = FALSE;
			for( INT x = 0 ; x < LayerArray.Num() ; ++x )
			{
				if ( LayerNames.ContainsItem( LayerArray(x) ) )
				{
					LayerArray.Remove( x );
					--x;
					bRemovedFromLayer = TRUE;
				}
			}

			if ( bRemovedFromLayer )
			{
				// Reconstruct a new layer name list for the actor.
				LayerName = TEXT("");
				for( INT LayerIndex = 0 ; LayerIndex < LayerArray.Num() ; ++LayerIndex )
				{
					if( LayerName.Len() )
					{
						LayerName += TEXT(",");
					}
					LayerName += LayerArray(LayerIndex);
				}
				Actor->Modify();
				Actor->Layer = FName( *LayerName );

				// update per-view visibility info
				UpdateActorAllViewsVisibility(Actor);

				bReturnVal = TRUE;
			}
		}
	}

	return bReturnVal;
}

/**
 * Selects/de-selects actors belonging to the named layers.
 *
 * @param	LayerNames	A list of layer names.
 * @param	bSelect		If TRUE actors are selected; if FALSE, actors are deselected.
 * @param	Editor		The editor in which to select/de-select the actors.
 * @return				TRUE if at least one actor was selected/deselected.
 */
UBOOL FLayerUtils::SelectActorsInLayers(const TArray<FString>& LayerNames, UBOOL bSelect, UEditorEngine* Editor)
{
	UBOOL bSelectedActor = FALSE;

	if ( LayerNames.Num() > 0 )
	{
		// Iterate over all actors, looking for actors in the specified layers.
		for( FActorIterator It ; It ; ++It )
		{
			AActor* Actor = *It;
			if( IsValid( Actor ) )
			{
				for ( INT LayerIndex = 0 ; LayerIndex < LayerNames.Num() ; ++LayerIndex )
				{
					const FString& CurLayer = LayerNames(LayerIndex);
					if ( Actor->IsInLayer( *CurLayer ) )
					{
						// The actor was found to be in a specified layer.
						// Set selection state and move on to the next actor.
						Editor->SelectActor( Actor, bSelect, NULL, FALSE, TRUE );
						bSelectedActor = TRUE;
						break;
					}
				}
			}
		}
	}

	return bSelectedActor;
}








/**
 * Updates the per-view visibility for all actors for the given view
 *
 * @param ViewIndex Index of the view (into GEditor->ViewportClients)
 * @param LayerThatChanged [optional] If one layer was changed (toggled in view pop-up, etc), then we only need to modify actors that use that layer
 */
void FLayerUtils::UpdatePerViewVisibility(INT ViewIndex, FName LayerThatChanged)
{
	// get the viewport client
	FEditorLevelViewportClient* Viewport = GEditor->ViewportClients(ViewIndex);

	// cache the FString representation of the fname
	FString LayerString = LayerThatChanged.ToString();

	// Iterate over all actors, looking for actors in the specified layers.
	for( FActorIterator It ; It ; ++It )
	{
		AActor* Actor = *It;
		if( IsValid( Actor ) )
		{
			// if the view has nothing hidden, just just quickly mark the actor as visible in this view 
			if (Viewport->ViewHiddenLayers.Num() == 0)
			{
				// if the actor had this view hidden, then unhide it
				if (Actor->HiddenEditorViews & ((QWORD)1 << ViewIndex))
				{
					// make sure this actor doesn't have the view set
					Actor->HiddenEditorViews &= ~((QWORD)1 << ViewIndex);
					Actor->ConditionalForceUpdateComponents(FALSE, FALSE);
				}
			}
			// else if we were given a name that was changed, only update actors with that name in their layers,
			// otherwise update all actors
			else if (LayerThatChanged == NAME_Skip || Actor->Layer.ToString().InStr(*LayerString) != INDEX_NONE)
			{
				UpdateActorViewVisibility(ViewIndex, Actor);
			}
		}
	}

	// make sure we redraw the viewport
	Viewport->Invalidate();
}


/**
 * Updates per-view visibility for the given actor in the given view
 *
 * @param ViewIndex Index of the view (into GEditor->ViewportClients)
 * @param Actor Actor to update
 * @param bReattachIfDirty If TRUE, the actor will reattach itself to give the rendering thread updated information
 */
void FLayerUtils::UpdateActorViewVisibility(INT ViewIndex, AActor* Actor, UBOOL bReattachIfDirty)
{
	// get the viewport client
	FEditorLevelViewportClient* Viewport = GEditor->ViewportClients(ViewIndex);

	// get the layers the actor is in
	TArray<FString> LayerList;
	Actor->GetLayers( LayerList );

	INT NumHiddenLayers = 0;
	// look for which of the actor layers are hidden
	for (INT HiddenLayerIndex = 0; HiddenLayerIndex < LayerList.Num(); HiddenLayerIndex++)
	{
		FName HiddenLayer = FName(*LayerList(HiddenLayerIndex));
		// if its in the view hidden list, this layer is hidden for this actor
		if (Viewport->ViewHiddenLayers.FindItemIndex(HiddenLayer) != -1)
		{
			NumHiddenLayers++;
			// right now, if one is hidden, the actor is hidden
			break;
		}
	}

	QWORD OriginalHiddenViews = Actor->HiddenEditorViews;

	// right now, if one is hidden, the actor is hidden
	if (NumHiddenLayers)
	{
		Actor->HiddenEditorViews |= ((QWORD)1 << ViewIndex);
	}
	else
	{
		Actor->HiddenEditorViews &= ~((QWORD)1 << ViewIndex);
	}

	// reattach if we changed the visibility bits, as the rnedering thread needs them
	if (bReattachIfDirty && OriginalHiddenViews != Actor->HiddenEditorViews)
	{
		Actor->ConditionalForceUpdateComponents(FALSE, FALSE);

		// make sure we redraw the viewport
		Viewport->Invalidate();
	}
}

/**
 * Updates per-view visibility for the given actor for all views
 *
 * @param Actor Actor to update
 */
void FLayerUtils::UpdateActorAllViewsVisibility(AActor* Actor)
{
	QWORD OriginalHiddenViews = Actor->HiddenEditorViews;

	for (INT ViewIndex = 0; ViewIndex < GEditor->ViewportClients.Num(); ViewIndex++)
	{
		// don't have this reattach, as we can do it once for all views
		UpdateActorViewVisibility(ViewIndex, Actor, FALSE);
	}

	// reattach if we changed the visibility bits, as the rnedering thread needs them
	if (OriginalHiddenViews != Actor->HiddenEditorViews)
	{
		Actor->ConditionalForceUpdateComponents(FALSE, FALSE);

		// redraw all viewports if the actor
		for (INT ViewIndex = 0; ViewIndex < GEditor->ViewportClients.Num(); ViewIndex++)
		{
			// make sure we redraw all viewports
			GEditor->ViewportClients(ViewIndex)->Invalidate();
		}
	}
}


/**
 * Removes the corresponding visibility bit from all actors (slides the later bits down 1)
 *
 * @param ViewIndex Index of the view (into GEditor->ViewportClients)
 */
void FLayerUtils::RemoveViewFromActorViewVisibility(INT ViewIndex)
{
	// get the bit for the view index
	QWORD ViewBit = ((QWORD)1 << ViewIndex);
	// get all bits under that that we want to keep
	QWORD KeepBits = ViewBit - 1;

	// Iterate over all actors, looking for actors in the specified layers.
	for( FActorIterator It ; It ; ++It )
	{
		AActor* Actor = *It;
		if( IsValid( Actor ) )
		{
			// remember original bits
			QWORD OriginalHiddenViews = Actor->HiddenEditorViews;

			QWORD Was = Actor->HiddenEditorViews;

			// slide all bits higher than ViewIndex down one since the view is being removed from GEditor
			QWORD LowBits = Actor->HiddenEditorViews & KeepBits;

			// now slide the top bits down by ViewIndex + 1 (chopping off ViewBit)
			QWORD HighBits = Actor->HiddenEditorViews >> (ViewIndex + 1);
			// then slide back up by ViewIndex, which will now have erased ViewBit, as well as leaving 0 in the low bits
			HighBits = HighBits << ViewIndex;

			// put it all back together
			Actor->HiddenEditorViews = LowBits | HighBits;

			// reattach if we changed the visibility bits, as the rnedering thread needs them
			if (OriginalHiddenViews != Actor->HiddenEditorViews)
			{
				// Find all attached primitive components and update the scene proxy with the actors updated visibility map
				for( INT ComponentIdx = 0; ComponentIdx < Actor->Components.Num(); ++ComponentIdx )
				{
					UActorComponent* Component = Actor->Components(ComponentIdx);
					if (Component && Component->IsAttached())
					{
						UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
						if (PrimitiveComponent)
						{
							// Push visibility to the render thread
							PrimitiveComponent->PushEditorVisibilityToProxy( Actor->HiddenEditorViews );
						}
					}
				}
			}
		}
	}
}

/**
 * Gets all known layers from the world
 *
 * @param AllLayers Output array to store all known layers
 */
void FLayerUtils::GetAllLayers(TArray<FName>& AllLayers)
{
	// clear anything in there
	AllLayers.Empty();

	// get a list of all unique layers currently in use by actors
	for (FActorIterator It; It; ++It)
	{
		AActor* Actor = *It;
		if(	IsValid(Actor) )
		{
			TArray<FString> LayerArray;
			Actor->GetLayers( LayerArray );

			// add unique layers to the list of all layers
			for( INT LayerIndex = 0 ; LayerIndex < LayerArray.Num() ; ++LayerIndex )
			{
				AllLayers.AddUniqueItem(FName(*LayerArray(LayerIndex)));
			}
		}
	}
}

/**
 * Gets all known layers from the world
 *
 * @param AllLayers Output array to store all known layers
 */
void FLayerUtils::GetAllLayers(TArray<FString>& AllLayers)
{
	// clear anything in there
	AllLayers.Empty();

	// get a list of all unique layers currently in use by actors
	for (FActorIterator It; It; ++It)
	{
		AActor* Actor = *It;
		if(	IsValid(Actor) )
		{
			TArray<FString> LayerArray;
			Actor->GetLayers( LayerArray );

			// add unique layers to the list of all layers
			for( INT LayerIndex = 0 ; LayerIndex < LayerArray.Num() ; ++LayerIndex )
			{
				AllLayers.AddUniqueItem(LayerArray(LayerIndex));
			}
		}
	}
}
