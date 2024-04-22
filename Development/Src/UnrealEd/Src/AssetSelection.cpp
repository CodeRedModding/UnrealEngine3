/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#include "UnrealEd.h"
#include "AssetSelection.h"
#include "ScopedTransaction.h"

#include "EngineFogVolumeClasses.h"
#include "EngineFluidClasses.h"
#include "LevelUtils.h"
#include "EditorLevelUtils.h"

extern void OnPlaceStaticMeshActor( AActor* MeshActor, UBOOL bUseSurfaceOrientation );

/**
 * Creates an actor using the specified factory.  When this is called, it is expected that the
 * factory already has a reference to the asset that the actor is being created for (usually
 * by calling AutoFillFields)
 *
 * Does nothing if ActorClass is NULL.
 */
static AActor* PrivateAddActor( UActorFactory* Factory, UBOOL bUseSurfaceOrientation=FALSE )
{
	AActor* Actor = NULL;
	if ( Factory )
	{
		GEditor->Constraints.Snap( GEditor->ClickLocation, FVector(0, 0, 0) );

		AActor* NewActorTemplate = Factory->GetDefaultActor();
		FVector Collision = NewActorTemplate->GetCylinderExtent();

		// if no collision extent found, try alternate collision for editor placement
		if ( Collision.IsZero() && NewActorTemplate->bCollideWhenPlacing )
		{
			// can't use AActor::GetComponentsBoundingBox() since default actor's components aren't attached
			if ( NewActorTemplate->CollisionComponent )
			{
				FBox Box(0.f);

				// Only use collidable components to find collision bounding box.
				if( NewActorTemplate->CollisionComponent->CollideActors )
				{
					FBoxSphereBounds DefaultBounds = NewActorTemplate->CollisionComponent->Bounds;
					NewActorTemplate->CollisionComponent->UpdateBounds();
					Box += NewActorTemplate->CollisionComponent->Bounds.GetBox();
					NewActorTemplate->CollisionComponent->Bounds = DefaultBounds;
				}
				FVector BoxExtent = Box.GetExtent();
				FLOAT CollisionRadius = appSqrt( (BoxExtent.X * BoxExtent.X) + (BoxExtent.Y * BoxExtent.Y) );
				Collision = FVector(CollisionRadius, CollisionRadius, BoxExtent.Z);
			}
		}
		FVector Location = GEditor->ClickLocation + GEditor->ClickPlane * (FBoxPushOut(GEditor->ClickPlane, Collision) + 0.1);

		GEditor->Constraints.Snap( Location, FVector(0, 0, 0) );

		// Orient the new actor with the surface normal?
		const FRotator SurfaceOrientation( GEditor->ClickPlane.Rotation() );
		const FRotator* const Rotation = bUseSurfaceOrientation ? &SurfaceOrientation : NULL;


		// Check to see if the user wants to place the actor into a streaming grid network
		ULevel* DesiredLevel = EditorLevelUtils::GetLevelForPlacingNewActor( Location );


		// Don't spawn the actor if the current level is locked.
		if ( FLevelUtils::IsLevelLocked(DesiredLevel) )
		{
			appMsgf(AMT_OK, TEXT("AddActor: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
			return NULL;
		}


		// Make sure the desired level is current
		ULevel* OldCurrentLevel = GWorld->CurrentLevel;
		GWorld->CurrentLevel = DesiredLevel;


		{
			const FScopedTransaction Transaction( TEXT("Create Actor") );
			// Create the actor.
			Actor = Factory->CreateActor( &Location, Rotation, NULL ); 
			if(Actor)
			{
				// Apply any static mesh tool settings if we placed a static mesh. 
				OnPlaceStaticMeshActor( Actor, bUseSurfaceOrientation );

				GEditor->SelectNone( FALSE, TRUE );
				GEditor->SelectActor( Actor, TRUE, NULL, TRUE );
				Actor->InvalidateLightingCache();
				Actor->PostEditMove( TRUE );
			}
		}

		// Restore current level
		GWorld->CurrentLevel = OldCurrentLevel;

		GEditor->RedrawLevelEditingViewports();

		if ( Actor )
		{
			Actor->MarkPackageDirty();
			GCallbackEvent->Send( CALLBACK_LevelDirtied );
			GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
		}

		// clear any factory references to the selected objects
		Factory->ClearFields();
	}

	return Actor;
}


/* ==========================================================================================================
	UActorFactorySelection
========================================================================================================== */
IMPLEMENT_CLASS(UActorFactorySelection);

/**
 * Adds an object to the private selection set.
 * Does not affect object flags related to selection.  Does not send selection related notifications.
 *
 * @param	ObjectToSelect		the object to add to the selection set
 */
void UActorFactorySelection::AddToSelection( UObject* ObjectToSelect )
{
	if ( ObjectToSelect != NULL && (ObjectToSelect->GetOutermost()->PackageFlags&PKG_PlayInEditor) == 0 )
	{
		SelectedObjects.AddUniqueItem(ObjectToSelect);
	}
}

/**
 * Removes an object from this private selection set.
 * Does not affect object flags related to selection.  Does not send selection related notifications.
 *
 * @param	ObjectToDeselect		the object to remove from the selection set
 */
void UActorFactorySelection::RemoveFromSelection( UObject* ObjectToDeselect )
{
	// Remove from selected list.
	SelectedObjects.RemoveItem( ObjectToDeselect );
}

/**
 * Replaces the currently selection set with a new list of objects.
 * Does not affect object flags related to selection.  Does not send selection related notifications.
 */
void UActorFactorySelection::SetSelection( const TArray<UObject*>& InSelectionSet )
{
	ClearSelection();
	for ( INT SelectIdx = 0; SelectIdx < InSelectionSet.Num(); SelectIdx++ )
	{
		UObject* Obj = InSelectionSet(SelectIdx);
		AddToSelection(Obj);
	}
}

/**
 * Clears this private selection set, removing all objects from the list.
 * Does not affect object flags related to selection.  Does not send selection related notifications.
 */
void UActorFactorySelection::ClearSelection()
{
	SelectedObjects.Empty();
}



/* ==========================================================================================================
	FActorFactoryAssetProxy
========================================================================================================== */
UActorFactorySelection* FActorFactoryAssetProxy::ActorFactorySelection = NULL;


/**
 * Accessor for retrieving the ActorFactoryAssetProxy's selection object.  Creates a new one if necessary.
 */
UActorFactorySelection* FActorFactoryAssetProxy::GetActorFactorySelector()
{
	if ( ActorFactorySelection == NULL )
	{
		// Create a new Selection
		ActorFactorySelection = new( UObject::GetTransientPackage(), TEXT("ActorFactorySelectionHelper") ) UActorFactorySelection;
		// Make sure it cant be garbage collected.
		ActorFactorySelection->AddToRoot();
	}

	return ActorFactorySelection;
}

/**
 * Builds a list of strings for populating the actor factory context menu items.  This menu is shown when
 * the user right-clicks in a level viewport.
 *
 * @param	SelectedAssets			the list of loaded assets which are currently selected
 * @param	OutQuickMenuItems		receives the list of strings to use for populating the actor factory context menu.
 * @param	OutAdvancedMenuItems	receives the list of strings to use for populating the actor factory advanced context menu [All Templates].
 * @param	OutSelectedMenuItems	receives the list of strings to use for populating the actor factory selected asset menu.
 * @param	bRequireAsset			if TRUE, only factories that can create actors with the currently selected assets will be added
 * @param	bCheckPlaceable			if TRUE, only bPlaceable factories will be added
 * @param	bCheckInQuickMenu		if TRUE, only assets with bShowInEditorQuickMenu set to TRUE will be added to the quick menu items
 */
void FActorFactoryAssetProxy::GenerateActorFactoryMenuItems( const TArray<FSelectedAssetInfo>& SelectedAssets, TArray<FString>& OutQuickMenuItems, TArray<FString>& OutAdvancedMenuItems, TArray<FString>* OutSelectedMenuItems, UBOOL bRequireAsset, UBOOL bCheckPlaceable, UBOOL bCheckInQuickMenu )
{
	FString UnusedErrorMessage;

	UActorFactorySelection* FactorySelectionHelper = GetActorFactorySelector();
	FactorySelectionHelper->ClearSelection();

	for ( INT SelectIdx = 0; SelectIdx < SelectedAssets.Num(); SelectIdx++ )
	{
		const FSelectedAssetInfo& AssetInfo = SelectedAssets(SelectIdx);
		FactorySelectionHelper->AddToSelection(AssetInfo.Object);
	}

	if( OutSelectedMenuItems )
	{
		OutSelectedMenuItems->Empty(GEditor->ActorFactories.Num());
	}
	OutQuickMenuItems.Empty(GEditor->ActorFactories.Num());
	OutAdvancedMenuItems.Empty(GEditor->ActorFactories.Num());

	FString UnusedErrorMsg;
	for ( INT FactoryIdx = 0; FactoryIdx < GEditor->ActorFactories.Num(); FactoryIdx++ )
	{
		UActorFactory* Factory = GEditor->ActorFactories(FactoryIdx);

		const UBOOL bPlacable = ( !bCheckPlaceable || ( bCheckPlaceable && Factory->bPlaceable ) );
		const UBOOL bShowInQuickMenu = ( !bCheckInQuickMenu || ( bCheckInQuickMenu && Factory->bShowInEditorQuickMenu ) );

		// The basic menu only shows factories that can be run without any intervention
		Factory->AutoFillFields( FactorySelectionHelper );

		const FString FactoryMenuString = Factory->GetMenuName();
		if( OutSelectedMenuItems && Factory->CanCreateActor( UnusedErrorMsg, TRUE ) && bPlacable )
		{
			OutSelectedMenuItems->AddItem( FactoryMenuString );
			// Need to add an empty string to the quick menu items so that the menu item ID's match up to the correct factory.
			OutQuickMenuItems.AddItem(TEXT(""));
		}
		else if( Factory->CanCreateActor( UnusedErrorMsg, bRequireAsset ) && bPlacable && bShowInQuickMenu )
		{
			if( OutSelectedMenuItems )
			{
				// Need to add an empty string to the selected items if they exist so that the menu item ID's match up to the correct factory.
				OutSelectedMenuItems->AddItem(TEXT(""));
			}
			OutQuickMenuItems.AddItem(FactoryMenuString);
		}
		else
		{
			if( OutSelectedMenuItems )
			{
				// Need to add an empty string to the selected items if they exist so that the menu item ID's match up to the correct factory.
				OutSelectedMenuItems->AddItem(TEXT(""));
			}
			OutQuickMenuItems.AddItem(TEXT(""));
		}

		// The advanced menu shows all of them.
		OutAdvancedMenuItems.AddItem(FactoryMenuString);

		// clear any factory references to the selected objects
		Factory->ClearFields();
	}

	FactorySelectionHelper->ClearSelection();
}

/**
 * Find the appropriate actor factory for an asset by type.
 *
 * @param	AssetData			contains information about an asset that to get a factory for
 * @param	bRequireValidObject	indicates whether a valid asset object is required.  specify FALSE to allow the asset
 *								class's CDO to be used in place of the asset if no asset is part of the drag-n-drop
 *
 * @return	the factory that is responsible for creating actors for the specified asset type.
 */
UActorFactory* FActorFactoryAssetProxy::GetFactoryForAsset( const FSelectedAssetInfo& AssetData, UBOOL bRequireValidObject/*=FALSE*/ )
{
	//@todo?
	// 	TArray<UObject*> PreviouslySelectedO
	// 	DropTargetSelection->GetSelectedObjects

	UObject* AssetObj = AssetData.Object;
	if ( AssetObj == NULL && !bRequireValidObject )
	{
		AssetObj = AssetData.ObjectClass->GetDefaultObject();
	}
	
	return FActorFactoryAssetProxy::GetFactoryForAssetObject( AssetObj );
}

/**
 * Find the appropriate actor factory for an asset.
 *
 * @param	AssetObj	The asset that to find the appropriate actor factory for
 *
 * @return	The factory that is responsible for creating actors for the specified asset
 */
UActorFactory* FActorFactoryAssetProxy::GetFactoryForAssetObject( UObject* AssetObj )
{
	UActorFactory* Result = NULL;

	// Attempt to find a factory that is capable of creating the asset
	GetActorFactorySelector()->AddToSelection( AssetObj );
	{
		const TArray< UActorFactory *>& ActorFactories = GEditor->ActorFactories;
		FString Unused;
		for ( INT FactoryIdx = 0; Result == NULL && FactoryIdx < ActorFactories.Num(); ++FactoryIdx )
		{
			UActorFactory* ActorFactory = ActorFactories(FactoryIdx);
			if ( ActorFactory->bPlaceable )
			{
				ActorFactory->AutoFillFields( GetActorFactorySelector() );
				// Check if the actor can be created using this factory, making sure to check for an asset to be assigned from the selector
				if ( ActorFactory->CanCreateActor( Unused, TRUE ) )
				{
					Result = ActorFactory;
				}

				ActorFactory->ClearFields();
			}
		}
	}
	GetActorFactorySelector()->RemoveFromSelection( AssetObj );

	return Result;
}

/**
 * Places an actor instance using the factory appropriate for the type of asset
 *
 * @param	AssetObj						the asset that is contained in the d&d operation
 * @param	bUseSurfaceOrientation			specify TRUE to indicate that the factory should align the actor
 *											instance with the target surface.
 * @param	FactoryToUse					optional actor factory to use to create the actor; if not specified,
 *											the highest priority factory that is valid will be used
 *
 * @return	the actor that was created by the factory, or NULL if there aren't any factories for this asset (or
 *			the actor couldn't be created for some other reason)
 */
AActor* FActorFactoryAssetProxy::AddActorForAsset( UObject* AssetObj, UBOOL bUseSurfaceOrientation, UActorFactory* FactoryToUse /*= NULL*/ )
{
	AActor* Result = NULL;

	FString ErrorString;
	if ( AssetObj != NULL && GetActorFactorySelector() != NULL )
	{
		GetActorFactorySelector()->AddToSelection(AssetObj);
		
		// If a specific factory has been provided, verify its validity and then use it to create the actor
		if ( FactoryToUse )
		{
			FactoryToUse->AutoFillFields( GetActorFactorySelector() );
			if ( FactoryToUse->bPlaceable && FactoryToUse->CanCreateActor( ErrorString, TRUE ) )
			{
				Result = PrivateAddActor( FactoryToUse, bUseSurfaceOrientation );
			}
			FactoryToUse->ClearFields();
		}
		// If no specific factory has been provided, find the highest priority one that is valid for the asset and use
		// it to create the actor
		else
		{
			const TArray<UActorFactory*>& ActorFactories = GEditor->ActorFactories;
			for ( INT FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); FactoryIdx++ )
			{
				UActorFactory* ActorFactory = ActorFactories(FactoryIdx);
				if ( ActorFactory->bPlaceable )
				{
					ActorFactory->AutoFillFields(GetActorFactorySelector());
					// Check if the actor can be created using this factory, making sure to check for an asset to be assigned from the selector
					if ( ActorFactory->CanCreateActor(ErrorString, TRUE) )
					{
						Result = PrivateAddActor(ActorFactory, bUseSurfaceOrientation);
						if ( Result != NULL )
						{
							break;
						}
					}

					ActorFactory->ClearFields();
				}
			}
		}
		GetActorFactorySelector()->RemoveFromSelection(AssetObj);
	}


	return Result;
}

/**
 * Places an actor instance using the factory appropriate for the type of asset using the current object selection as the asset
 *
 * @param	ActorClass						The type of actor to create
 * @param	bUseSurfaceOrientation			specify TRUE to indicate that the factory should align the actor
 *											instance with the target surface.
 * @param	FactoryToUse					optional actor factory to use to create the actor; if not specified,
 *											the highest priority factory that is valid will be used
 *
 * @return	the actor that was created by the factory, or NULL if there aren't any factories for this asset (or
 *			the actor couldn't be created for some other reason)
 */
AActor* FActorFactoryAssetProxy::AddActorFromSelection( UClass* ActorClass, UBOOL bUseSurfaceOrientation, UActorFactory* ActorFactory )
{
	check( ActorClass != NULL );

	if( !ActorFactory )
	{
		// Look for an actor factory capable of creating actors of the actors type.
		ActorFactory = GEditor->FindActorFactory( ActorClass );
	}

	AActor* Result = NULL;
	FString ErrorString;
	
	if ( ActorFactory )
	{
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
		ActorFactory->AutoFillFields( GEditor->GetSelectedObjects() );
		if( ActorFactory->CanCreateActor( ErrorString, FALSE ) )
		{
			if ( ActorFactory->bPlaceable  )
			{
				// Attempt to add the actor
				Result = PrivateAddActor( ActorFactory, bUseSurfaceOrientation );
			}
		}
		else
		{
			// If the actor couldnt be added, display a message to the user saying why.
			appMsgf( AMT_OK, *LocalizeUnrealEd( *ErrorString ) );
		}
		ActorFactory->ClearFields();
	}

	return Result;
}

/**
 * Determines if the provided actor is capable of having a material applied to it.
 *
 * @param	TargetActor	Actor to check for the validity of material application
 *
 * @return	TRUE if the actor is valid for material application; FALSE otherwise
 */
UBOOL FActorFactoryAssetProxy::IsActorValidForMaterialApplication( AActor* TargetActor )
{
	UBOOL bIsValid = FALSE;

	// Check if the actor has a mesh, fog volume density, or fluid surface component. If so, it can likely have
	// a material applied to it. Otherwise, it cannot.
	if ( TargetActor )
	{
		for ( TArray<UActorComponent*>::TConstIterator ComponentIter( TargetActor->Components ); ComponentIter; ++ComponentIter )
		{
			const UComponent* CurActorComponent = *ComponentIter;
			if	( CurActorComponent && 
				( CurActorComponent->IsA( UMeshComponent::StaticClass() ) ||
				  CurActorComponent->IsA( UFogVolumeDensityComponent::StaticClass() ) ||
				  CurActorComponent->IsA( UFluidSurfaceComponent::StaticClass() ) ) )
			{
				bIsValid = TRUE;
				break;
			}
		}
	}

	return bIsValid;
}
/**
 * Attempts to apply the material to the specified actor.
 *
 * @param	TargetActor		the actor to apply the material to
 * @param	MaterialToApply	the material to apply to the actor
 *
 * @return	TRUE if the material was successfully applied to the actor
 */
UBOOL FActorFactoryAssetProxy::ApplyMaterialToActor( AActor* TargetActor, UMaterialInterface* MaterialToApply )
{
	UBOOL bResult = FALSE;

	if ( TargetActor != NULL && MaterialToApply != NULL )
	{
		UFogVolumeDensityComponent* FoundFogComponent = NULL;
		UFluidSurfaceComponent* FoundFluidComponent = NULL;

		// Some actors (such as a fractured static mesh actor) could potentially have multiple mesh components, so
		// we need to store all of the potentially valid ones (or else perform special cases with IsA checks on the
		// target actor)
		TArray<UMeshComponent*> FoundMeshComponents;	

		// Find which mesh the user clicked on first.
		for ( INT ComponentIdx=0; ComponentIdx < TargetActor->Components.Num(); ComponentIdx++ )
		{
			UComponent* ActorComp = TargetActor->Components(ComponentIdx);

			UFogVolumeDensityComponent* FogComponent = Cast<UFogVolumeDensityComponent>(ActorComp);
			if ( FogComponent != NULL )
			{
				FoundFogComponent = FogComponent;
				break;
			}
			UFluidSurfaceComponent* FluidComponent = Cast<UFluidSurfaceComponent>(ActorComp);
			if ( FluidComponent != NULL )
			{
				FoundFluidComponent = FluidComponent;
				break;
			}
			UMeshComponent* MeshComponent = Cast<UMeshComponent>(ActorComp);
			if ( MeshComponent != NULL )
			{
				// Intentionally do not break the loop here, as there could be potentially multiple mesh components
				FoundMeshComponents.AddUniqueItem( MeshComponent );
				continue;
			}
		}

		if ( FoundFogComponent != NULL )
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT( "DropTarget_UndoSetActorMaterial" ) ) );
			FoundFogComponent->Modify();
			FoundFogComponent->FogMaterial = MaterialToApply;
			TargetActor->ForceUpdateComponents(FALSE,FALSE);
			bResult = TRUE;
		}
		else if ( FoundFluidComponent != NULL )
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT( "DropTarget_UndoSetActorMaterial" ) ) );
			FoundFluidComponent->Modify();
			FoundFluidComponent->FluidMaterial = MaterialToApply;
			TargetActor->ForceUpdateComponents(FALSE,FALSE);
			bResult = TRUE;
		}
		else if ( FoundMeshComponents.Num() > 0 )
		{
			// Check each component that was found
			for ( TArray<UMeshComponent*>::TConstIterator MeshCompIter( FoundMeshComponents ); MeshCompIter; ++MeshCompIter )
			{
				UMeshComponent* FoundMeshComponent = *MeshCompIter;

				// OK, we need to figure out how many material slots this mesh component/static mesh has.
				// Start with the actor's material count, then drill into the static/skeletal mesh to make sure 
				// we have the right total.
				INT MaterialCount = FoundMeshComponent->Materials.Num();

				// Check static mesh material count
				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(FoundMeshComponent);
				if( StaticMeshComp != NULL )
				{
					UStaticMesh* StaticMesh = StaticMeshComp->StaticMesh;
					if( StaticMesh != NULL )
					{
						const INT ForcedLODIndex = 0;
						for( INT ElementIndex = 0; ElementIndex < StaticMesh->LODModels( ForcedLODIndex ).Elements.Num(); ++ElementIndex )
						{
							const FStaticMeshElement& Element = StaticMesh->LODModels( ForcedLODIndex ).Elements( ElementIndex );
							if( MaterialCount < Element.MaterialIndex + 1 )
							{
								MaterialCount = Element.MaterialIndex + 1;
							}
						}
					}
				}

				// Check skeletal mesh material count
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(FoundMeshComponent);
				if( SkelMeshComp != NULL )
				{
					USkeletalMesh* SkeletalMesh = SkelMeshComp->SkeletalMesh;
					if( SkeletalMesh != NULL )
					{
						if( MaterialCount < SkeletalMesh->Materials.Num() )
						{
							MaterialCount = SkeletalMesh->Materials.Num();
						}
					}
				}


				// Any materials to overwrite?
				if( MaterialCount > 0 )
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT( "DropTarget_UndoSetActorMaterial" ) ) );
					FoundMeshComponent->Modify();
					for( INT CurMaterialIndex = 0; CurMaterialIndex < MaterialCount; ++CurMaterialIndex )
					{
						FoundMeshComponent->SetMaterial( CurMaterialIndex, MaterialToApply );
					}
					TargetActor->ForceUpdateComponents( FALSE, FALSE );
					bResult = TRUE;
				}
			}
		}
	}


	return bResult;
}


// EOF




