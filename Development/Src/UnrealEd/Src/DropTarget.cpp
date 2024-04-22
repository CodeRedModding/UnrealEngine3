/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEd.h"
#include "DropTarget.h"
#include "AssetSelection.h"
#include "ScopedTransaction.h"
#include "EngineProcBuildingClasses.h"
#include "EnginePrefabClasses.h"

#if WITH_MANAGED_CODE
#include "ContentBrowserShared.h"
#endif

/**
 * Internal helper function that attempts to apply the supplied object as a material to the supplied actor.
 *
 * @param	ObjToUse				Object to attempt to apply as a material
 * @param	ActorToApplyTo			Actor to whom the material should be applied
 * @param	bAllowDecalMaterials	If TRUE, allow decal materials to be applied to the actor as well
 *
 * @return	TRUE if the provided object was successfully applied to the provided actor as a material
 */
static UBOOL AttemptApplyObjAsMaterialToActor( UObject* ObjToUse, AActor* ActorToApplyTo, UBOOL bAllowDecalMaterials )
{
	UBOOL bResult = FALSE;

	// Ensure the provided object is some form of material
	UMaterialInterface* DroppedObjAsMaterial = Cast<UMaterialInterface>( ObjToUse );

	// Check if the provided object is a decal material if they shouldn't be allowed
	if ( ActorToApplyTo && DroppedObjAsMaterial && ( bAllowDecalMaterials || ( !( Cast<UDecalMaterial>( ObjToUse ) ) ) ) )
	{
		// Apply the material to the actor
		FScopedTransaction Transaction( *LocalizeUnrealEd("DragDrop_Transaction_ApplyMaterialToActor") );
		bResult = FActorFactoryAssetProxy::ApplyMaterialToActor( ActorToApplyTo, DroppedObjAsMaterial );
	}

	return bResult;
}

/**
 * Internal helper function that attempts to apply the supplied object as a proc. building ruleset to the
 * supplied actor.
 *
 * @param	ObjToUse		Object to attempt to apply as a proc. building ruleset
 * @param	ActorToApplyTo	Actor to whom the ruleset should be applied
 *
 * @return	TRUE if the provided object was successfully applied to the provided actor as a proc. building ruleset
 */
static UBOOL AttemptApplyObjAsProcBuildingRulesetToActor( UObject* ObjToUse, AActor* ActorToApplyTo )
{
	UBOOL bResult = FALSE;

	// Ensure that the provided object is a proc building ruleset and that the provided actor is
	// a proc building actor
	UProcBuildingRuleset* DroppedObjAsRuleset = Cast<UProcBuildingRuleset>( ObjToUse );
	AProcBuilding* ActorAsProcBuilding = Cast<AProcBuilding>( ActorToApplyTo );
	if ( DroppedObjAsRuleset && ActorAsProcBuilding )
	{
		// Apply the ruleset to the proc building actor
		FScopedTransaction Transaction( *LocalizeUnrealEd("DragDrop_Transaction_ApplyRulesetToProcBuildingActor") );
		ActorAsProcBuilding->Modify();
		ActorAsProcBuilding->Ruleset = DroppedObjAsRuleset;
		ActorAsProcBuilding->PreEditChange( NULL );
		ActorAsProcBuilding->PostEditChange();
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Internal helper function that attempts to apply the supplied object as a material to the BSP surface specified by the
 * provided model and index.
 *
 * @param	ObjToUse				Object to attempt to apply as a material
 * @param	ModelHitProxy			Hit proxy containing the relevant model
 * @param	SurfaceIndex			The index in the model's surface array of the relevant
 * @param	bAllowDecalMaterials	If TRUE, allow decal materials to apply to the surface as well
 *
 * @return	TRUE if the supplied object was successfully applied to the specified BSP surface
 */
static UBOOL AttemptApplyObjAsMaterialToSurface( UObject* ObjToUse, HModel* ModelHitProxy, INT SurfaceIndex, UBOOL bAllowDecalMaterials )
{
	UBOOL bResult = FALSE;

	// Ensure the dropped object is a material
	UMaterialInterface* DroppedObjAsMaterial = Cast<UMaterialInterface>( ObjToUse );

	// Check if the provided material is a decal material if they shouldn't be allowed
	if ( DroppedObjAsMaterial && ModelHitProxy && SurfaceIndex != INDEX_NONE && ( bAllowDecalMaterials || ( !Cast<UDecalMaterial>( DroppedObjAsMaterial ) ) ) )
	{
		// Apply the material to the specified surface
		FScopedTransaction Transaction( *LocalizeUnrealEd("DragDrop_Transaction_ApplyMaterialToSurface") );

		// Modify the component so that PostEditUndo can reattach the model after undo
		ModelHitProxy->GetModelComponent()->Modify();

		UModel* DroppedUponModel = ModelHitProxy->GetModel();
		check( DroppedUponModel );
		check( DroppedUponModel->Surfs.IsValidIndex( SurfaceIndex ) );

		DroppedUponModel->ModifySurf( SurfaceIndex, TRUE  );
		DroppedUponModel->Surfs( SurfaceIndex ).Material = DroppedObjAsMaterial;
		GEditor->polyUpdateMaster( DroppedUponModel, SurfaceIndex, FALSE );
		bResult = TRUE;
	}
	return bResult;
}

/**
 * Internal helper function that attempts to use the provided object/asset to create an actor to place.
 *
 * @param	ObjToUse		Asset to attempt to use for an actor to place
 * @param	HitProxy		Hit proxy at the location where the drop is occurring
 * @param	CursorLocation	Location of the cursor while dropping
 *
 * @return	TRUE if the object was successfully used to place an actor; FALSE otherwise
 */
static UBOOL AttemptDropObjAsActor( UObject* ObjToUse, const HHitProxy* HitProxy, const FViewportCursorLocation& CursorLocation )
{
	UBOOL bActorSuccessfullyPlaced = FALSE;

	// Determine whether surface orientation should be used or not; Currently only necessary for decal materials
	const UBOOL bUseSurfaceOrientation = Cast<UDecalMaterial>( ObjToUse ) != NULL;
	
	AActor* PlacedActor = NULL;
	if( ObjToUse->IsA( AActor::StaticClass() ) && !ObjToUse->HasAnyFlags(RF_ArchetypeObject) )
	{
		// If the object being dropped is an actor, the drag&drop originated from the actor browser.
		UClass* ActorClass = ObjToUse->GetClass();
		FVector Collision = ActorClass->GetDefaultActor()->GetCylinderExtent();

		// Find an actor factory appropriate for this actor class
		UActorFactory* ActorFactory = GEditor->FindActorFactory( ActorClass );
		if( ActorFactory )
		{
			// If an actor factory was found use that to add the actor.
			PlacedActor = FActorFactoryAssetProxy::AddActorFromSelection( ActorClass, bUseSurfaceOrientation, ActorFactory );
			bActorSuccessfullyPlaced = ( PlacedActor != NULL );
		}
		else
		{
			// If no actor factory was found, add the actor directly.
			// Orient the new actor with the surface normal?
			const FRotator SurfaceOrientation( GEditor->ClickPlane.Rotation() );
			const FRotator* const Rotation = bUseSurfaceOrientation ? &SurfaceOrientation : NULL;

			PlacedActor = GEditor->AddActor( ActorClass, GEditor->ClickLocation + GEditor->ClickPlane * ( FBoxPushOut( GEditor->ClickPlane, Collision ) + 0.1 ) );
			if( PlacedActor && Rotation )
			{
				PlacedActor->Rotation = *Rotation;
			}
			bActorSuccessfullyPlaced = ( PlacedActor != NULL );
		}
	}
	else
	{
		// Attempt to add an actor for the provided asset
		PlacedActor = FActorFactoryAssetProxy::AddActorForAsset( ObjToUse, bUseSurfaceOrientation );
		bActorSuccessfullyPlaced = ( PlacedActor != NULL );
	}

	// If the actor was successfully placed, but it was done on the background of a perspective viewport, forcibly move the actor in front
	// of the camera
	if ( bActorSuccessfullyPlaced && !HitProxy && CursorLocation.GetViewportType() == LVT_Perspective )
	{
		GEditor->MoveActorInFrontOfCamera( *PlacedActor, CursorLocation.GetOrigin(), CursorLocation.GetDirection() );
	}

	return bActorSuccessfullyPlaced;
}

/**
 * Internal helper function that attempts to place the provided object as a prefab instance.
 *
 * @param	ObjToUse		Object to attempt to place as a prefab instance
 * @param	HitProxy		Hit proxy at the location where the drop is occurring
 * @param	CursorLocation	Location of the cursor while dropping
 *
 * @return	TRUE if the provided object was successfully placed as a prefab instance, FALSE otherwise
 */
static UBOOL AttemptDropObjAsPrefabInstance( UObject* ObjToUse, const HHitProxy* HitProxy, const FViewportCursorLocation& CursorLocation )
{
	UBOOL bPrefabSuccessfullyPlaced = FALSE;

	// Ensure the provided object is a prefab
	UPrefab* DroppedObjAsPrefab = Cast<UPrefab>( ObjToUse );
	if ( DroppedObjAsPrefab )
	{
		// Ensure the prefab is actually placeable by confirming that it contains placeable object archetypes
		UBOOL bPrefabIsPlaceable = FALSE;
		for ( TArray<UObject*>::TConstIterator PrefabArchIter( DroppedObjAsPrefab->PrefabArchetypes ); PrefabArchIter; ++PrefabArchIter )
		{
			const UObject* CurPrefabArch = *PrefabArchIter;
			if ( CurPrefabArch && CurPrefabArch->GetClass()->HasAnyClassFlags( CLASS_Placeable ) )
			{
				bPrefabIsPlaceable = TRUE;
				break;
			}
		}

		if ( bPrefabIsPlaceable )
		{		
			const FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("Prefab_UndoTag_AddPrefab") ) );
			
			// Spawn a prefab instance
			APrefabInstance* PrefabInstance = CastChecked<APrefabInstance>( GWorld->SpawnActor( APrefabInstance::StaticClass(), NAME_None, GEditor->ClickLocation, FRotator(0,0,0) ) );
			bPrefabSuccessfullyPlaced = ( PrefabInstance != NULL );
			
			if ( bPrefabSuccessfullyPlaced )
			{
				// If the prefab instance was successfully placed, but it was done on the background of a perspective viewport, forcibly move
				// the actor in front of the camera
				if ( !HitProxy && CursorLocation.GetViewportType() == LVT_Perspective )
				{
					GEditor->MoveActorInFrontOfCamera( *PrefabInstance, CursorLocation.GetOrigin(), CursorLocation.GetDirection() );
				}

				// Instance the prefab from the provided object
				PrefabInstance->InstancePrefab( DroppedObjAsPrefab );
			}
		}
		// Warn the user the prefab doesn't contain anything placeable
		else
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("DragDrop_Error_PrefabNotPlaceable") );
		}
	}
	return bPrefabSuccessfullyPlaced;
}

/**
 * Internal helper function to correctly set GEditor click location, plane, etc. for a drop to the background
 *
 * @param	CursorLocation	Location of the mouse cursor
 */
static void ConfigureClickInfoOnBGDrop( const FViewportCursorLocation& CursorLocation )
{
	// If an asset was dropped in an orthographic view,
	// adjust the location so the asset will not be positioned at the camera origin (which is at the edge of the world for ortho cams)
	const FLOAT DistanceMultiplier = ( CursorLocation.GetViewportType() == LVT_Perspective ) ? 1.0f : HALF_WORLD_MAX;

	GEditor->ClickLocation = CursorLocation.GetOrigin() + CursorLocation.GetDirection() * DistanceMultiplier;
	GEditor->ClickPlane = FPlane( 0, 0, 0, 0 );
}

/**
 * Internal helper function to correctly set GEditor click location, plane, etc. for a drop onto an actor
 *
 * @param	DroppedOnActor	Reference to the actor that was dropped upon
 * @param	CursorLocation	Location of the mouse cursor
 */
static void ConfigureClickInfoOnActorDrop( const AActor& DroppedOnActor, const FViewportCursorLocation& CursorLocation )
{
	// Find the point on the actor component which was clicked on.
	FCheckResult Hit( 1 );
	if ( !GWorld->SingleLineCheck( Hit, NULL, CursorLocation.GetOrigin() + CursorLocation.GetDirection() * HALF_WORLD_MAX, CursorLocation.GetOrigin(), TRACE_Actors | TRACE_ComplexCollision ) )
	{
		GEditor->ClickLocation = Hit.Location;
		GEditor->ClickPlane = FPlane( Hit.Location,Hit.Normal );
	}
	else
	{
		GEditor->ClickLocation = DroppedOnActor.Location;
		GEditor->ClickPlane = FPlane( DroppedOnActor.Location, FVector( 0.0f, 0.0f, 1.0f ) );
	}
}

/**
 * Internal helper function to correctly set GEditor click location, plane, etc. for a drop onto a BSP surface
 *
 * @param	DroppedOnSurf	Reference to the BSP surface that was dropped upon
 * @param	CursorLocation	Location of the mouse cursor
 */
static void ConfigureClickInfoOnSurfDrop( const FBspSurf& DroppedOnSurf, const FViewportCursorLocation& CursorLocation )
{
	GEditor->ClickLocation	= FLinePlaneIntersection( CursorLocation.GetOrigin(), CursorLocation.GetOrigin() + CursorLocation.GetDirection(), DroppedOnSurf.Plane );
	GEditor->ClickPlane		= DroppedOnSurf.Plane;
}

/* ==========================================================================================================
	FEditorTextDropTarget
========================================================================================================== */
/** Ctor */
WxEditorTextDropTarget::WxEditorTextDropTarget( FEditorLevelViewportClient* InViewportClient )
: FEditorDropTarget(InViewportClient)
{
}

/* ==========================================================================================================
	WxObjectPathNameDropTarget
========================================================================================================== */
/**
 * Constructor
 *
 * @param	InViewportClient	the VC for the viewport that will receive the drop
 */
WxObjectPathNameDropTarget::WxObjectPathNameDropTarget( FEditorLevelViewportClient* InViewportClient )
: WxEditorTextDropTarget(InViewportClient)
{
}

/**
 * Parses the string passed in by the drop-source data object to generate a list of FSelectedAssetInfo
 */
void WxObjectPathNameDropTarget::ParseDroppedAssetString()
{
	const TCHAR AssetDelimiter[] = { AssetMarshalDefs::AssetDelimiter, TEXT('\0') };

	wxTextDataObject* dobj = (wxTextDataObject*)GetDataObject();
	if ( dobj != NULL )
	{
		FString SourceData = dobj->GetText().c_str();

		TArray<FString> DroppedAssetStrings;
		SourceData.ParseIntoArray(&DroppedAssetStrings, AssetDelimiter, TRUE);

		DroppedAssets.Empty(DroppedAssetStrings.Num());
		for ( INT StringIdx = 0; StringIdx < DroppedAssetStrings.Num(); StringIdx++ )
		{
			// Account for the possibility that a dropped string can come from an invalid source
			// If it cant be parsed we should not add it to the dropped assets list because it
			// would contain invalid data.
			if( FSelectedAssetInfo::CanBeParsedFrom( DroppedAssetStrings( StringIdx ) ) )
			{
				new(DroppedAssets) FSelectedAssetInfo(DroppedAssetStrings(StringIdx));
			}
		}
	}
}

/**
 * Clears the list of FSelectedAssetInfo
 */
void WxObjectPathNameDropTarget::ClearDroppedAssetData()
{
	DroppedAssets.Empty();
}

/** @hack: hook to fill-in the drop-target's data member with the data from the drop-source */
bool WxObjectPathNameDropTarget::IsAcceptedData(IDataObject *pIDataSource) const
{
	if ( WxEditorTextDropTarget::IsAcceptedData(pIDataSource) )
	{
		// hook to set our datasource prior to calling GetData() from OnEnter;
		// some evil stuff here - don't try this at home!
		const_cast<WxObjectPathNameDropTarget*>(this)->SetDataSource(pIDataSource);
		return true;
	}

	return false;
}

// called when the mouse enters the window (only once until OnLeave())
wxDragResult WxObjectPathNameDropTarget::OnEnter(wxCoord x, wxCoord y, wxDragResult def)
{
	if ( GetData() )
	{
		ParseDroppedAssetString();
	}

	return OnDragOver(x, y, def);
}

/**
* Called when the mouse moves in the window - shouldn't take long to execute or otherwise mouse movement would be too slow
*
* @param	x	x position of cursor
* @param	y	y position of cursor
* @param	def	result to return if dropping is allowed at this coordinate
*
* @return	value indicating whether this coordinate is a valid drop location
*/
wxDragResult WxObjectPathNameDropTarget::OnDragOver( wxCoord MouseX, wxCoord MouseY, wxDragResult def )
{
	FIntPoint ViewportOrigin, ViewportSize;
	ViewportClient->GetViewportDimensions(ViewportOrigin, ViewportSize);
	MouseX -= ViewportOrigin.X;
	MouseY -= ViewportOrigin.Y;

	// Initialize the result to wxDragNone in case we dont have any droppable assets
	wxDragResult Result = wxDragNone;
	if ( DroppedAssets.Num() == 1 )
	{
		FSelectedAssetInfo& DropData = DroppedAssets(0);

#if WITH_MANAGED_CODE
		if ( FContentBrowser::IsInitialized() && FContentBrowser::IsAssetValidForPlacing(DropData.ObjectPathName) )
#endif
		{
			UObject* AssetObj = DropData.Object != NULL ? DropData.Object : DropData.ObjectClass->GetDefaultObject();
			
			const UBOOL bAssetIsPrefab = AssetObj->IsA( UPrefab::StaticClass() );

			HHitProxy* HitProxy = ViewportClient->Viewport->GetHitProxy(MouseX, MouseY);
			if ( HitProxy == NULL )
			{
				// if no hit-proxy, or the hit-proxy is BSP, we must have a valid factory type
				if ( FActorFactoryAssetProxy::GetFactoryForAsset(DropData) != NULL || bAssetIsPrefab )
				{
					Result = def;
				}
				else if( AssetObj->IsA( AActor::StaticClass() ) )
				{
					// Drag and drop originated from the actor browser
					Result = def;
				}
			}
			else
			{
				// otherwise, if the hit-proxy is an actor, the dropped object could potentially be a material 
				// (which means the hit proxy must be an actor which can accept a material) or it could potentially
				// be an object to create an actor with (and so a valid factory type must exist for the dropped object)
				if ( HitProxy->IsA(HActor::StaticGetType()) )
				{
					// +++
					// actor
					// might be a material
					AActor* Actor = static_cast<HActor*>(HitProxy)->Actor;
					if ( Actor != NULL && ( FActorFactoryAssetProxy::GetFactoryForAsset(DropData) != NULL || Cast<UMaterialInterface>(AssetObj) != NULL  || bAssetIsPrefab ) )
					{
						//make sure this isn't a speed tree actor
						if (!Actor->IsA(ASpeedTreeActor::StaticClass()))
						{
							Result = def;
						}
					}
					//if a proc building
					if ( Actor != NULL && ( Cast<UProcBuildingRuleset>(AssetObj) != NULL ) )
					{
						Result = def;
					}
					else if( Actor != NULL && AssetObj->IsA( AActor::StaticClass() ) )
					{
						// Drag and drop originated from the actor browser
						Result = def;
					}
				}
				else if (HitProxy->IsA(HModel::StaticGetType()))
				{
					// +++
					// BSP surface (only when not in geometry mode)
					// if no hit-proxy, or the hit-proxy is BSP, we must have a valid factory type
					if ( FActorFactoryAssetProxy::GetFactoryForAsset(DropData) != NULL || Cast<UMaterialInterface>(AssetObj) != NULL || bAssetIsPrefab )
					{
						//@todo ronp - do we need to check whether we have a valid UModel
						Result = def;
					}
					else if( AssetObj->IsA( AActor::StaticClass() ) )
					{
						// Drag and drop originated from the actor browser
						Result = def;
					}
				}
				else if( HitProxy->IsA( HWidgetAxis::StaticGetType() ) )
				{
					if ( FActorFactoryAssetProxy::GetFactoryForAsset(DropData) != NULL || bAssetIsPrefab )
					{
						// +++
						// axis translation/rotation widget - find out what's underneath the axis widget
						Result = def;
					}
					else if( AssetObj->IsA( AActor::StaticClass() ) )
					{
						// Drag and drop originated from the actor browser
						Result = def;
					}
				}
			}
		}
	}
	return Result;
}

// called when mouse leaves the window: might be used to remove the feedback which was given in OnEnter()
void WxObjectPathNameDropTarget::OnLeave()
{
	ClearDroppedAssetData();
}

/**
 * Called when the user releases the mouse button after dragging an asset from the content browser.  Places a new instance
 * of the appropriate asset/class at the location of the mouse.
 *
 * @param	MouseX	x position of the mouse, in canvas coordinates (i.e. 0,0 => SizeX,SizeY)
 * @param	MouseY	y position of the mouse, in canvas coordinates (i.e. 0,0 => SizeX,SizeY)
 * @param	Text	a string containing a pipe-delimited list of UObject pathnames
 *
 * @return	TRUE if the viewport accepted the drop-operation.
 */
bool WxObjectPathNameDropTarget::OnDropText( wxCoord MouseX, wxCoord MouseY, const wxString& Text )
{
	// determine which objects are already loaded
	TArray<UObject*> DroppedObjects;
	for ( INT NameIdx = 0; NameIdx < DroppedAssets.Num(); NameIdx++ )
	{
		FSelectedAssetInfo& AssetItem = DroppedAssets(NameIdx);
		if ( AssetItem.Object == NULL )
		{
			AssetItem.Object = UObject::StaticFindObject(AssetItem.ObjectClass, ANY_PACKAGE, *AssetItem.ObjectPathName);
			if ( AssetItem.Object == NULL )
			{
#if WITH_MANAGED_CODE
				if ( FContentBrowser::IsInitialized() && FContentBrowser::IsAssetValidForLoading(AssetItem.ObjectPathName) )
#endif
				{
					// if Obj is NULL, assume it means this is because we haven't loaded this object yet, so do that now
					AssetItem.Object = UObject::StaticLoadObject(AssetItem.ObjectClass, NULL, *AssetItem.ObjectPathName, NULL, LOAD_NoWarn|LOAD_Quiet, NULL, FALSE);
					if ( AssetItem.Object != NULL )
					{
						// issue a request to update the AssetVisual for this item since it is now loaded
						FCallbackEventParameters Parms(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI | CBR_UpdatePackageList, AssetItem.Object );
						GCallbackEvent->Send(Parms);
					}
				}
			}
		}

#if WITH_MANAGED_CODE
		if (AssetItem.Object != NULL && FContentBrowser::IsInitialized() && FContentBrowser::IsAssetValidForPlacing(AssetItem.ObjectPathName) )
#else
		if ( AssetItem.Object != NULL )
#endif
		{
			DroppedObjects.AddUniqueItem(AssetItem.Object);
		}
		else
		{
			// Drag and drop originated from the actor browser
			if( AssetItem.ObjectClass->IsChildOf( AActor::StaticClass() ) )
			{
				// Add the default actor object as a template for how to spawn the actor
				DroppedObjects.AddUniqueItem( AssetItem.ObjectClass->GetDefaultObject() );
			}
		}
	}

	ClearDroppedAssetData();
	FIntPoint ViewportOrigin, ViewportSize;
	ViewportClient->GetViewportDimensions(ViewportOrigin, ViewportSize);
	MouseX -= ViewportOrigin.X;
	MouseY -= ViewportOrigin.Y;

	return ProcessDropIntoViewport(MouseX, MouseY, DroppedObjects) == TRUE;
}

/**
 * Called when the user drags an asset from the content browser into a level viewport.
 *
 * @param	DropX			X location (relative to the viewport's origin) where the user dropped the objects
 * @param	DropY			Y location (relative to the viewport's origin) where the user dropped the objects
 * @param	DroppedObjects	list of asset object that were dragged from the content browser
 *
 * @return	TRUE if the viewport accepted the drop operation
 */
UBOOL WxObjectPathNameDropTarget::ProcessDropIntoViewport( INT DropX, INT DropY, const TArray<UObject*>& DroppedObjects )
{
	UBOOL bResult = FALSE;

	if ( DroppedObjects.Num() > 0 )
	{
		FSceneViewFamilyContext ViewFamily(
			ViewportClient->Viewport, ViewportClient->GetScene(),
			ViewportClient->ShowFlags,
			GWorld->GetTimeSeconds(),
			GWorld->GetDeltaSeconds(),
			GWorld->GetRealTimeSeconds(),
			ViewportClient->IsRealtime()
			);
		FSceneView* View = ViewportClient->CalcSceneView( &ViewFamily );
		FViewportCursorLocation DropData(View, ViewportClient, DropX, DropY);

		FVector4 ScreenSpacePos = View->PixelToScreen(DropX, DropY, 0.f);
		GEditor->ClickLocation = View->InvViewProjectionMatrix.TransformFVector4(ScreenSpacePos);
		GEditor->ClickPlane = FPlane(EC_EventParm);

		// Invalidate the hit proxy map so it will be rendered out again when GetHitProxy is called
		ViewportClient->Viewport->InvalidateHitProxy();
		HHitProxy* HitProxy = ViewportClient->Viewport->GetHitProxy(DropX, DropY);

		// If the user is pressing CTRL while performing the drop, a special "context menu" drop will be performed.
		// All dropping logic will be routed through the context menu. However, we don't want to create a context menu
		// (even if CTRL is down) if the hit proxy is a widget, because this function will be called again in that case, with
		// widgets disabled (and we'd end up with 2 context menus). Once this function is called a second time, the context
		// menu will be correctly made.
		const UBOOL bCtrlPressed = GetAsyncKeyState( VK_CONTROL ) & 0x8000;
		const UBOOL bIsContextMenuDrop = bCtrlPressed && ( !HitProxy || !( HitProxy->IsA( HWidgetAxis::StaticGetType() ) ) );

		if ( bIsContextMenuDrop )
		{
			// Create the drag drop menu and display it
			WxDragDropContextMenu DragDropMenu( DroppedObjects, HitProxy, *View, DropData );
			FTrackPopupMenu Tpm( GApp->EditorFrame, &DragDropMenu );
			Tpm.Show();
			bResult = DragDropMenu.WasDropHandled();
		}
		else
		{
			if ( HitProxy == NULL )
			{
				bResult = OnDropToBackground(View, DropData, DroppedObjects);
			}
			else if (HitProxy->IsA(HActor::StaticGetType()))
			{
				// +++
				// actor
				HActor* TargetProxy = static_cast<HActor*>( HitProxy );
				AActor* TargetActor = TargetProxy->Actor;

				// if the target actor is selected, we should drop onto all selected items
				// otherwise, we should drop only onto this object
				UBOOL bDropOntoSelected = TargetActor->IsSelected();

				if( !bDropOntoSelected )
				{
					bResult = OnDropToActor( View, DropData, DroppedObjects, TargetProxy );
				}
				else
				{
					for ( USelection::TObjectIterator It = GEditor->GetSelectedActors()->ObjectItor() ; It ; ++It )
					{
						AActor* Actor = static_cast<AActor*>( *It );
						if( Actor )
						{
							TargetProxy->Actor = Actor;
							OnDropToActor( View, DropData, DroppedObjects, TargetProxy );
							bResult = TRUE;
						}
					}
				}
			}
			else if (HitProxy->IsA(HModel::StaticGetType()))
			{
				// +++
				// BSP surface (only when not in geometry mode)
				bResult = OnDropToBSPSurface(View, DropData, DroppedObjects, static_cast<HModel*>(HitProxy));
			}
			else if( HitProxy->IsA( HWidgetAxis::StaticGetType() ) )
			{
				// +++
				// axis translation/rotation widget - find out what's underneath the axis widget
				bResult = OnDropToWidget(View, DropData, DroppedObjects, static_cast<HWidgetAxis*>(HitProxy));
			}
			else if (HitProxy->IsA(HBSPBrushVert::StaticGetType()))
			{
				// +++
				// BSP brush vert 
				// ignore
			}
			else if (HitProxy->IsA(HStaticMeshVert::StaticGetType()))
			{
				// +++
				// static mesh vertex (only enabled when "Large Vertices" show flag is enabled)
				// ignore
			}
			else if (HitProxy->IsA(HGeomPolyProxy::StaticGetType()))
			{
				// +++
				// BSP poly surface (geometry mode only)
				// ignore
			}
			else if (HitProxy->IsA(HGeomEdgeProxy::StaticGetType()))
			{
				// +++
				// BSP poly edge (geometry mode only)
				// ignore
			}
			else if (HitProxy->IsA(HGeomVertexProxy::StaticGetType()))
			{
				// +++
				// BSP poly vertex (geometry mode only)
				// ignore
			}
		}
	
		if ( bResult )
		{
			// Give the dropped-in viewport focus
			SetFocus( static_cast<HWND>( ViewportClient->Viewport->GetWindow() ) );

			// Set the current level editing viewport client to the dropped-in viewport client
			if ( GCurrentLevelEditingViewportClient != ViewportClient )
			{
				// Invalidate the old vp client to remove its special selection box
				if ( GCurrentLevelEditingViewportClient )
				{
					GCurrentLevelEditingViewportClient->Invalidate();
				}
				GCurrentLevelEditingViewportClient = ViewportClient;
			}
			ViewportClient->Invalidate();
		}
	}

	return bResult;
}

/**
 * Called when an asset is dropped onto the blank area of a viewport.
 *
 * @param	View				The SceneView for the dropped-in viewport
 * @param	ViewportMousePos	Mouse cursor location
 * @param	DroppedObjects		Array of objects dropped into the viewport
 *
 * @return	TRUE if the drop operation was successfully handled; FALSE otherwise
 */
UBOOL WxObjectPathNameDropTarget::OnDropToBackground( FSceneView* View, FViewportCursorLocation& DropData, const TArray<UObject*>& DroppedObjects )
{
	UBOOL bResult = FALSE;

	if ( DroppedObjects.Num() > 0 )
	{
		checkf( DroppedObjects.Num() == 1, TEXT("Only the dropping of one asset is supported!") );
		
		UObject* AssetObj = DroppedObjects(0);
		check( AssetObj );

		// Configure editor click information
		ConfigureClickInfoOnBGDrop( DropData );

		// Attempt to create an actor from the dropped object
		bResult = AttemptDropObjAsActor( AssetObj, NULL, DropData );

		// If nothing has successfully handled the drop, attempt to drop the item as a prefab instance
		if ( !bResult )
		{
			bResult = AttemptDropObjAsPrefabInstance( AssetObj, NULL, DropData );
		}
	}

	return bResult;
}

/**
 * Called when an asset is dropped upon an existing actor.
 *
 * @param	View				The SceneView for the dropped-in viewport
 * @param	ViewportMousePos	Mouse cursor location
 * @param	DroppedObjects		Array of objects dropped into the viewport
 * @param	TargetProxy			Hit proxy representing the dropped upon actor
 *
 * @return	TRUE if the drop operation was successfully handled; FALSE otherwise
 */
UBOOL WxObjectPathNameDropTarget::OnDropToActor( FSceneView* View, FViewportCursorLocation& DropData, const TArray<UObject*>& DroppedObjects, HActor* TargetProxy )
{
	UBOOL bResult = FALSE;
	AActor* DroppedUponActor = TargetProxy->Actor;

	if ( DroppedUponActor != NULL && DroppedObjects.Num() > 0 )
	{
		checkf( DroppedObjects.Num() == 1, TEXT("Only the dropping of one asset is supported!") );

		UObject* AssetObj = DroppedObjects(0);
		check( AssetObj );

		// Attempt to apply the dropped asset as a material to the actor (don't allow decal materials)
		if ( !bResult )
		{
			bResult = AttemptApplyObjAsMaterialToActor( AssetObj, DroppedUponActor, FALSE );
		}

		// Attempt to apply the dropped asset as a proc. building ruleset to the actor
		if ( !bResult )
		{
			bResult = AttemptApplyObjAsProcBuildingRulesetToActor( AssetObj, DroppedUponActor );
		}

		if ( !bResult )
		{
			ConfigureClickInfoOnActorDrop( *DroppedUponActor, DropData );

			// Attempt to create an actor from the dropped object
			bResult = AttemptDropObjAsActor( AssetObj, TargetProxy, DropData );

			// Attempt to create a prefab instance from the dropped object
			if ( !bResult )
			{
				bResult = AttemptDropObjAsPrefabInstance( AssetObj, TargetProxy, DropData );
			}
		}
	}

	return bResult;
}

/**
 * Called when an asset is dropped upon a BSP surface.
 *
 * @param	View				The SceneView for the dropped-in viewport
 * @param	ViewportMousePos	Mouse cursor location
 * @param	DroppedObjects		Array of objects dropped into the viewport
 * @param	TargetProxy			Hit proxy representing the dropped upon model
 *
 * @return	TRUE if the drop operation was successfully handled; FALSE otherwise
 */
UBOOL WxObjectPathNameDropTarget::OnDropToBSPSurface( FSceneView* View, FViewportCursorLocation& DropData, const TArray<UObject*>& DroppedObjects, HModel* TargetProxy )
{
	UBOOL bResult = FALSE;

	UINT SurfaceIndex = INDEX_NONE;
	const INT DropX = DropData.GetCursorPos().X;
	const INT DropY = DropData.GetCursorPos().Y;

	// Determine which surface the drop operation occured on
	if ( TargetProxy && TargetProxy->ResolveSurface( View, DropX, DropY, SurfaceIndex ) && TargetProxy->GetModel() && DroppedObjects.Num() > 0 )
	{
		checkf( DroppedObjects.Num() == 1, TEXT("Only the dropping of one asset is supported!") );

		UObject* AssetObj = DroppedObjects( 0 );
		check( AssetObj );

		// Attempt to apply the dropped asset as a material to the BSP surface
		if ( !bResult )
		{
			bResult = AttemptApplyObjAsMaterialToSurface( AssetObj, TargetProxy, SurfaceIndex, FALSE );
		}

		// Place the dropped asset as an actor at the drop location
		if ( !bResult )
		{
			ConfigureClickInfoOnSurfDrop( TargetProxy->GetModel()->Surfs( SurfaceIndex ), DropData );
			
			// Attempt to create an actor from the dropped object
			bResult = AttemptDropObjAsActor( AssetObj, TargetProxy, DropData );

			// Attempt to create a prefab instance from the dropped object
			if ( !bResult )
			{
				bResult = AttemptDropObjAsPrefabInstance( AssetObj, TargetProxy, DropData );
			}
		}
	}
	return bResult;
}

/**
 * Called when an asset is dropped upon a manipulation widget.
 *
 * @param	View				The SceneView for the dropped-in viewport
 * @param	ViewportMousePos	Mouse cursor location
 * @param	DroppedObjects		Array of objects dropped into the viewport
 * @param	TargetProxy			Hit proxy representing the dropped upon manipulation widget
 *
 * @return	TRUE if the drop operation was successfully handled; FALSE otherwise
 */
UBOOL WxObjectPathNameDropTarget::OnDropToWidget( FSceneView* View, FViewportCursorLocation& DropData, const TArray<UObject*>& DroppedObjects, HWidgetAxis* TargetProxy )
{
	UBOOL bResult = FALSE;

	// axis translation/rotation widget - find out what's underneath the axis widget

	// OK, we need to be a bit evil right here.  Basically we want to hijack the ShowFlags
	// for the scene so we can re-render the hit proxies without any axis widgets.  We'll
	// store the original ShowFlags and modify them appropriately
	const EShowFlags OldShowFlags = ViewportClient->ShowFlags;
	const EShowFlags OldSCFShowFlags = View->Family->ShowFlags;
	
	ViewportClient->ShowFlags &= ~SHOW_ModeWidgets;
	FSceneViewFamily* SceneViewFamily = const_cast< FSceneViewFamily* >( View->Family );
	SceneViewFamily->ShowFlags &= ~SHOW_ModeWidgets;

	// Invalidate the hit proxy map so it will be rendered out again when GetHitProxy is called
	ViewportClient->Viewport->InvalidateHitProxy();

	// This will actually re-render the viewport's hit proxies!
	FIntPoint DropPos = DropData.GetCursorPos();
	
	HHitProxy* HitProxy = ViewportClient->Viewport->GetHitProxy(DropPos.X, DropPos.Y);
		
	// We should never encounter a widget axis.  If we do, then something's wrong
	// with our ShowFlags (or the widget drawing code)
	check( !HitProxy || ( HitProxy && !HitProxy->IsA( HWidgetAxis::StaticGetType() ) ) );

	// Try this again, but without the widgets this time!
	const FIntPoint& CursorPos = DropData.GetCursorPos();
	bResult = ProcessDropIntoViewport(CursorPos.X, CursorPos.Y, DroppedObjects);

	// Undo the evil
	ViewportClient->ShowFlags = OldShowFlags;
	SceneViewFamily->ShowFlags = OldSCFShowFlags;

	return bResult;
}


/*-----------------------------------------------------------------------------
	WxDragDropContextMenu.
-----------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE( WxDragDropContextMenu, wxMenu )
	EVT_MENU_RANGE( IDMENU_DragDropActorFactory_Start, IDMENU_DragDropActorFactory_End, WxDragDropContextMenu::OnDropCreateActor )
	EVT_MENU_RANGE( IDMENU_DragDropReplaceWithActorFactory_Start, IDMENU_DragDropReplaceWithActorFactory_End, WxDragDropContextMenu::OnDropReplaceActor )
	EVT_MENU( IDMENU_DragDropApplyToDroppedUpon, WxDragDropContextMenu::OnDropApplyObjectToDroppedUpon )
	EVT_MENU( IDMENU_DragDropDropAsPrefab, WxDragDropContextMenu::OnDropCreatePrefabInstance )
END_EVENT_TABLE()

/**
 * Construct a WxDragDropContextMenu
 *
 * @param	DroppedObjects	Array of objects that have been dropped (currently only the first object is used)
 * @param	HitProxy		HitProxy at the location of the user drop, if any
 * @param	SceneView		SceneView of the viewport where the user performed the drop
 * @param	CursorLocation	Location of the cursor where the user performed the drop
 */ 
WxDragDropContextMenu::WxDragDropContextMenu( TArray<UObject*> DroppedObjects, HHitProxy* HitProxy, const FSceneView& SceneView, const FViewportCursorLocation& CursorLocation )
:	DroppedInSceneView( SceneView ),
	DroppedCursorLocation( CursorLocation ),
	DroppedUponProxy( HitProxy ),
	DroppedUponSurfaceIndex( INDEX_NONE ),
	bHandledDrop( FALSE )
{
	check( DroppedObjects.Num() > 0 );
	for ( TArray<UObject*>::TConstIterator DroppedIter( DroppedObjects ); DroppedIter; ++DroppedIter )
	{
		check( *DroppedIter );
		DroppedAssets.AddItem( FSelectedAssetInfo( *DroppedIter ) );
	}

	// Create all valid menu options
	AppendApplyToOptions();
	AppendFactoryOptions();
	AppendMiscOptions();
}

/**
 * Destroy a WxDragDropContextMenu
 */
WxDragDropContextMenu::~WxDragDropContextMenu()
{
	DroppedUponProxy = NULL;
	DroppedAssets.Empty();
}

/**
 * Checks whether the context menu "handled" the drag-and-drop operation
 *
 * @return	TRUE if the menu handled the operation (via actor creation, etc.); FALSE if the user closed the menu or it
 *			did not handle the operation for some reason
 */
UBOOL WxDragDropContextMenu::WasDropHandled() const
{
	return bHandledDrop;
}

/** Helper function designed to append any valid add actor/replace with actor actor factory options */
void WxDragDropContextMenu::AppendFactoryOptions()
{
	// Check to see if the builder brush is selected because it can't 
	// be replaced. It would crash the editor if we deleted it!
	UBOOL bHasBuilderBrushSelected = FALSE;

	for( FSelectionIterator SelectionIter = GEditor->GetSelectedActorIterator(); SelectionIter; ++SelectionIter )
	{
		if( (*SelectionIter)->IsA(AActor::StaticClass()) && CastChecked<AActor>(*SelectionIter)->IsABuilderBrush() )
		{
			bHasBuilderBrushSelected = TRUE;
			break;
		}
	}


	// Create the sub-menu for replacing selected actors with an actor from any valid factory, but only if there are selected actors in the editor
	// and the drop operation is upon an existing actor
	wxMenu* ReplaceActorFactoryMenu = ( GEditor->GetSelectedActorCount() > 0 && DroppedUponProxy && !bHasBuilderBrushSelected && DroppedUponProxy->IsA( HActor::StaticGetType() ) ) ? new wxMenu() : NULL;

	// Generate the list of valid factories for the sub-menus (only interested in factories which produce placable actors from assets)
	TArray<FString> QuickMenuItems, AdvMenuItems;
	FActorFactoryAssetProxy::GenerateActorFactoryMenuItems( DroppedAssets, QuickMenuItems, AdvMenuItems, NULL, TRUE, TRUE, FALSE);

	// Append each valid factory to the respective menus
	for ( TArray<FString>::TConstIterator QuickMenuIter( QuickMenuItems ); QuickMenuIter; ++QuickMenuIter )
	{
		const FString& CurMenuString = *QuickMenuIter;
		if ( CurMenuString.Len() > 0 )
		{
			Append( IDMENU_DragDropActorFactory_Start + QuickMenuIter.GetIndex(), *CurMenuString, TEXT("") );
			if ( ReplaceActorFactoryMenu )
			{
				ReplaceActorFactoryMenu->Append( IDMENU_DragDropReplaceWithActorFactory_Start + QuickMenuIter.GetIndex(), *CurMenuString, TEXT("") );
			}
		}
	}

	// If there was at least one valid factory, at least one selected actor, and the drop occurred upon an actor, append the replace sub-menu
	if ( ReplaceActorFactoryMenu && ReplaceActorFactoryMenu->GetMenuItemCount() > 0 )
	{
		Append( wxID_ANY, *LocalizeUnrealEd("DragDropContextMenu_DropReplaceSelected"), ReplaceActorFactoryMenu );
	}
}

/** Helper function designed to append any valid "Apply actor as ____ to ____" options */
void WxDragDropContextMenu::AppendApplyToOptions()
{
	// Determine which "apply to" actions are valid based upon the dropped upon hit proxy
	if ( DroppedUponProxy )
	{
		UObject* FirstDroppedObject = DroppedAssets( 0 ).Object;
		check( FirstDroppedObject );

		// Handle various "apply to actor" possibilities
		if ( DroppedUponProxy->IsA( HActor::StaticGetType() ) )
		{
			AActor* DroppedUponActor = static_cast<HActor*>( DroppedUponProxy )->Actor;
			if ( DroppedUponActor )
			{
				// If the dropped asset is a material and the actor can have a material applied to it, present the "apply material to actor" option
				if ( Cast<UMaterialInterface>( FirstDroppedObject ) && FActorFactoryAssetProxy::IsActorValidForMaterialApplication( DroppedUponActor ) )
				{
					Append( IDMENU_DragDropApplyToDroppedUpon, *LocalizeUnrealEd("DragDropContextMenu_DropApplyMaterialToActor") );
				}

				// If the dropped asset is a proc. building ruleset and the actor is a proc. building, present the "apply ruleset to proc building" option
				if ( Cast<UProcBuildingRuleset>( FirstDroppedObject ) && Cast<AProcBuilding>( DroppedUponActor ) )
				{
					Append( IDMENU_DragDropApplyToDroppedUpon, *LocalizeUnrealEd("DragDropContextMenu_DropApplyRulesetToProcBuildingActor") );
				}
			}
		}

		// Handle various "apply to surface" possibilities
		else if ( DroppedUponProxy->IsA( HModel::StaticGetType() ) )
		{
			HModel* ModelHitProxy = static_cast<HModel*>( DroppedUponProxy );
			if ( ModelHitProxy->ResolveSurface( &DroppedInSceneView, DroppedCursorLocation.GetCursorPos().X, DroppedCursorLocation.GetCursorPos().Y, DroppedUponSurfaceIndex ) )
			{
				// If the dropped asset is a material, present the "apply material to surface" option
				if ( Cast<UMaterialInterface>( FirstDroppedObject ) && DroppedUponSurfaceIndex != INDEX_NONE )
				{
					Append( IDMENU_DragDropApplyToDroppedUpon, *LocalizeUnrealEd("DragDropContextMenu_DropApplyMaterialToSurface") );
				}
			}
		}
	}
}

/** Helper function designed to append any miscellaneous options */
void WxDragDropContextMenu::AppendMiscOptions()
{
	check( DroppedAssets.Num() > 0 );
	UObject* FirstDroppedObject = DroppedAssets( 0 ).Object;
	
	// If the drop object is a prefab, provide the option to drop it as a prefab instance
	if ( FirstDroppedObject->IsA( UPrefab::StaticClass() ) )
	{
		Append( IDMENU_DragDropDropAsPrefab, *LocalizeUnrealEd("DragDropContextMenu_DropAsPrefabInstance") );
	}
}

/** Helper function to correctly set GEditor click locations, etc. based upon the type of drop operation */
void WxDragDropContextMenu::ConfigureClickInformation()
{
	// Background drop
	if ( !DroppedUponProxy )
	{
		ConfigureClickInfoOnBGDrop( DroppedCursorLocation );
	}
	// Actor drop
	else if ( DroppedUponProxy->IsA( HActor::StaticGetType() ) )
	{
		AActor* ActorOfHitProxy = static_cast<HActor*>( DroppedUponProxy )->Actor;
		check( ActorOfHitProxy );

		ConfigureClickInfoOnActorDrop( *ActorOfHitProxy, DroppedCursorLocation );
	}
	// Surface drop
	else if ( DroppedUponProxy->IsA( HModel::StaticGetType() ) )
	{
		check( DroppedUponSurfaceIndex != INDEX_NONE );
		UModel* ModelOfHitProxy = static_cast<HModel*>( DroppedUponProxy )->GetModel();
		check ( ModelOfHitProxy );

		ConfigureClickInfoOnSurfDrop( ModelOfHitProxy->Surfs( DroppedUponSurfaceIndex ), DroppedCursorLocation );
	}
}

/**
 * Called in response to the user selecting a menu option to add an actor of the dropped object from one of
 * the valid actor factories
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects a menu option
 */
void WxDragDropContextMenu::OnDropCreateActor( wxCommandEvent& In )
{
	// Determine the actor factory index in the GEditor array based upon the sent-in ID
	INT ActorFactoryIndex = In.GetId() - IDMENU_DragDropActorFactory_Start;
	check( ActorFactoryIndex >= 0 && ActorFactoryIndex < GEditor->ActorFactories.Num() );
	check( DroppedAssets.Num() > 0 );

	UActorFactory* ActorFactory = GEditor->ActorFactories( ActorFactoryIndex );
	UObject* AssetObject = DroppedAssets( 0 ).Object;
	check( ActorFactory );
	check( AssetObject );

	// Set up the GEditor click location, etc. correctly depending on the type of drop operation
	ConfigureClickInformation();

	// Determine if surface orientation should be used (currently only needed for decals) and create an actor from the specified factory
	UBOOL bShouldUseSurfaceOrientation = Cast<UDecalMaterial>( AssetObject ) != NULL;
	AActor* NewActor = FActorFactoryAssetProxy::AddActorForAsset( AssetObject, bShouldUseSurfaceOrientation, ActorFactory );
	
	// Background, perspective viewport drops need special handling to ensure the actor is in front of the camera
	if( NewActor && DroppedCursorLocation.GetViewportType() == LVT_Perspective && !DroppedUponProxy )
	{
		// Move the actor in front of the camera if we are in perspective view.
		GEditor->MoveActorInFrontOfCamera( *NewActor, DroppedCursorLocation.GetOrigin(), DroppedCursorLocation.GetDirection() );
	}

	bHandledDrop = ( NewActor != NULL );
}

/**
 * Called in response to the user selecting a menu option to replace all selected actors with an actor of the
 * dropped object from one of the valid actor factories
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects a menu option
 */
void WxDragDropContextMenu::OnDropReplaceActor( wxCommandEvent& In )
{
	// Determine the actor factory index in the GEditor array based upon the sent-in ID
	INT ActorFactoryIndex = In.GetId() - IDMENU_DragDropReplaceWithActorFactory_Start;
	check( ActorFactoryIndex >= 0 && ActorFactoryIndex < GEditor->ActorFactories.Num() );
	check( DroppedAssets.Num() > 0 );

	UActorFactory* ActorFactory = GEditor->ActorFactories( ActorFactoryIndex );
	check( ActorFactory );

	// Replace all selected actors with actors created from the specified factory
	GEditor->ReplaceSelectedActors( ActorFactory, NULL );
	bHandledDrop = TRUE;
}

/**
 * Called in response to the user selecting a menu option to apply the dropped object(s) to the dropped upon
 * object (such as a material to a BSP surface, etc.)
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects a menu option
 */
void WxDragDropContextMenu::OnDropApplyObjectToDroppedUpon( wxCommandEvent& In )
{
	check( DroppedUponProxy && DroppedAssets.Num() > 0 );

	UObject* FirstDroppedObject = DroppedAssets( 0 ).Object;
	check( FirstDroppedObject );

	// Handle various "apply to actor" possibilities
	if ( DroppedUponProxy->IsA( HActor::StaticGetType() ) )
	{
		AActor* DroppedUponActor = static_cast<HActor*>( DroppedUponProxy )->Actor;
		check( DroppedUponActor );
	
		// Attempt to apply the dropped asset as a material to the actor
		if ( !bHandledDrop )
		{
			bHandledDrop = AttemptApplyObjAsMaterialToActor( FirstDroppedObject, DroppedUponActor, TRUE );
		}

		// Attempt to apply the dropped asset as a proc building ruleset to the actor
		if ( !bHandledDrop )
		{
			bHandledDrop = AttemptApplyObjAsProcBuildingRulesetToActor( FirstDroppedObject, DroppedUponActor );
		}
	}

	// Handle various "apply to surface" possibilities
	else if ( DroppedUponProxy->IsA( HModel::StaticGetType() ) )
	{
		// Attempt to apply the dropped asset as a material to the surface
		check( DroppedUponSurfaceIndex != INDEX_NONE );
		bHandledDrop = AttemptApplyObjAsMaterialToSurface( FirstDroppedObject, static_cast<HModel*>( DroppedUponProxy ), DroppedUponSurfaceIndex, TRUE );
	}
}

/**
 * Called in response to the user selecting a menu option to drop the dropped object as a prefab instance.
 *
 * @param	In	Event automatically generated by wxWidgets when the user selects a menu option
 */
void WxDragDropContextMenu::OnDropCreatePrefabInstance( wxCommandEvent& In )
{
	check( DroppedAssets.Num() > 0 );

	UObject* AssetObject = DroppedAssets( 0 ).Object;
	check( AssetObject );

	// Attempt to drop the object as a prefab instance
	bHandledDrop = AttemptDropObjAsPrefabInstance( AssetObject, DroppedUponProxy, DroppedCursorLocation );
}

// EOF




