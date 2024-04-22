/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineDecalClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EnginePrefabClasses.h"
#include "EngineSplineClasses.h"
#include "EngineAIClasses.h"
#include "EngineAnimClasses.h"
#include "CameraController.h"
#include "MouseDeltaTracker.h"
#include "ScopedTransaction.h"
#include "HModel.h"
#include "SpeedTree.h"
#include "BSPOps.h"
#include "LevelUtils.h"
#include "LayerUtils.h"
#include "SplineEdit.h"
#include "InterpEditor.h"
#include "ResourceIDs.h"
#include "..\Src\StaticLightingPrivate.h"
#include "EditorLevelUtils.h"
#include "Engine.h"
#include "SocketSnapping.h"

static const FLOAT CAMERA_MAYAPAN_SPEED		= 0.6f;

/** Static: List of objects we're hovering over */
TSet< FEditorLevelViewportClient::FViewportHoverTarget > FEditorLevelViewportClient::HoveredObjects;


/**
 * A hit proxy class for sockets in the main editor viewports.
 */
struct HLevelSocketProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( HLevelSocketProxy, HHitProxy );

	AActor* Actor;
	USkeletalMeshComponent* SkelMeshComponent;
	INT SocketIndex;

	HLevelSocketProxy(AActor* InActor, USkeletalMeshComponent* InSkelMeshComponent, INT InSocketIndex)
		:	HHitProxy(HPP_UI)
		,	Actor( InActor )
		,	SkelMeshComponent( InSkelMeshComponent )
		,	SocketIndex( InSocketIndex )
	{}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Actor;
		Ar << SkelMeshComponent;
	}
};

/**
 * Cached off joystick input state
 */
class FCachedJoystickState
{
public:
	UINT JoystickType;
	TMap <FName, FLOAT> AxisDeltaValues;
	TMap <FName, EInputEvent> KeyEventValues;
};

/**
 * Corrects the orientation if the mesh if placed on a mesh 
 * and applies static mesh tool settings if applicable. 
 *
 * @param	Actor					The static mesh or speed tree actor that was placed.
 * @param	bUseSurfaceOrientation	When TRUE, changes the mesh orientation to reflect the surface.
 */
void OnPlaceStaticMeshActor( AActor* Actor, UBOOL bUseSurfaceOrientation )
{
	check( Actor );

	if( ( Actor->IsA(AStaticMeshActor::StaticClass()) || Actor->IsA( ASpeedTreeActor::StaticClass() ) ) && 
		( bUseSurfaceOrientation || GEditorModeTools().IsModeActive(EM_StaticMesh) ) )
	{
		if( bUseSurfaceOrientation )
		{
			Actor->Rotation = GEditor->ClickPlane.SafeNormal().Rotation();

			// This is necessary because static meshes are authored along the vertical axis rather than
			// the X axis, as the surface orientation code expects.  To compensate for this, we add 90
			// degrees to the static mesh's Pitch.

			Actor->Rotation.Pitch -= 16384;
		}

		// Only do the extra static mesh placement stuff if the user is in static mesh mode
		if( GEditorModeTools().IsModeActive(EM_StaticMesh) )
		{
			FEdMode* EditMode = GEditorModeTools().GetActiveMode(EM_StaticMesh);
			FModeTool_StaticMesh* StaticMeshTool = (FModeTool_StaticMesh*)((FEdModeStaticMesh*)EditMode)->GetCurrentTool();
			StaticMeshTool->ApplySettings(Actor);
		}

		Actor->ForceUpdateComponents();
	}
}

// Loop through all viewports and disable any realtime viewports viewing the edited level before running the game.
// This won't disable things like preview viewports in Cascade etc.
void UEditorEngine::DisableRealtimeViewports()
{
	for( INT x = 0 ; x < ViewportClients.Num() ; ++x)
	{
		FEditorLevelViewportClient* VC = ViewportClients(x);
		if( VC )
		{
			VC->SetRealtime( FALSE, TRUE );
		}
	}

	RedrawAllViewports();

	GCallbackEvent->Send( CALLBACK_UpdateUI );
}

/**
 * Restores any realtime viewports that have been disabled by DisableRealtimeViewports. This won't
 * disable viewporst that were realtime when DisableRealtimeViewports has been called and got
 * latter toggled to be realtime.
 */
void UEditorEngine::RestoreRealtimeViewports()
{
	for( INT x = 0 ; x < ViewportClients.Num() ; ++x)
	{
		FEditorLevelViewportClient* VC = ViewportClients(x);
		if( VC )
		{
			VC->RestoreRealtime();
		}
	}

	RedrawAllViewports();

	GCallbackEvent->Send( CALLBACK_UpdateUI );
}


/**
 * Adds an actor to the world at the specified location.
 *
 * @param	Class		A valid non-abstract, non-transient, placeable class.
 * @param	Location	The world-space location to spawn the actor.
 * @param	bSilent		If TRUE, suppress logging (optional, defaults to FALSE).
 * @result				A pointer to the newly added actor, or NULL if add failed.
 */
AActor* UEditorEngine::AddActor(UClass* Class, const FVector& Location, UBOOL bSilent)
{
	check( Class );

	if( !bSilent )
	{
		debugf( NAME_Log,
				TEXT("Attempting to add actor of class '%s' to level at %0.2f,%0.2f,%0.2f"),
				*Class->GetName(), Location.X, Location.Y, Location.Z );
	}

	///////////////////////////////
	// Validate class flags.

	if( Class->ClassFlags & CLASS_Abstract )
	{
		warnf( NAME_Error, TEXT("Class %s is abstract.  You can't add actors of this class to the world."), *Class->GetName() );
		return NULL;
	}
	if( !(Class->ClassFlags & CLASS_Placeable) )
	{
		warnf( NAME_Error, TEXT("Class %s isn't placeable.  You can't add actors of this class to the world."), *Class->GetName() );
		return NULL;
	}
	if( Class->ClassFlags & CLASS_Transient )
	{
		warnf( NAME_Error, TEXT("Class %s is transient.  You can't add actors of this class in UnrealEd."), *Class->GetName() );
		return NULL;
	}


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


	// Transactionally add the actor.
	AActor* Actor = NULL;
	{
		const FScopedTransaction Transaction( TEXT("Add Actor") );

		SelectNone( FALSE, TRUE );

		AActor* Default = Class->GetDefaultActor();
		Actor = GWorld->SpawnActor( Class, NAME_None, Location, Default->Rotation, NULL, TRUE );
		if( Actor )
		{
			SelectActor( Actor, 1, NULL, 0 );
			ABrush* Brush = Cast<ABrush>(Actor);
			if( Brush && Brush->BrushComponent )
			{
				FBSPOps::csgCopyBrush( Brush, (ABrush*)Class->GetDefaultActor(), 0, 0, 1, TRUE );
			}
			Actor->InvalidateLightingCache();
			Actor->PostEditMove( TRUE );
		}
		else
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_Couldn'tSpawnActor") );
		}
	}

	///////////////////////////////
	// If this actor is part of any layers (set in its default properties), add them into the visible layers list.

	if( Actor )
	{
		TArray<FString> NewLayers;
		Actor->Layer.ToString().ParseIntoArray( &NewLayers, TEXT(","), 0 );

		TArray<FString> VisibleLayers;
		GWorld->GetWorldInfo()->VisibleLayers.ParseIntoArray( &VisibleLayers, TEXT(","), 0 );

		for( INT x = 0 ; x < NewLayers.Num() ; ++x )
		{
			VisibleLayers.AddUniqueItem( NewLayers(x) );
		}

		GWorld->GetWorldInfo()->VisibleLayers = TEXT("");
		for( INT x = 0 ; x < VisibleLayers.Num() ; ++x )
		{
			if( GWorld->GetWorldInfo()->VisibleLayers.Len() > 0 )
			{
				GWorld->GetWorldInfo()->VisibleLayers += TEXT(",");
			}
			GWorld->GetWorldInfo()->VisibleLayers += VisibleLayers(x);
		}

		GCallbackEvent->Send( CALLBACK_LayerChange );
	}

	///////////////////////////////
	// Clean up.

	// Restore current level
	GWorld->CurrentLevel = OldCurrentLevel;

	if ( Actor )
	{
		Actor->MarkPackageDirty();
		GCallbackEvent->Send( CALLBACK_LevelDirtied );
		GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
	}

	NoteSelectionChange();

	return Actor;
}

/**
 * Looks for an appropriate actor factory for the specified UClass.
 *
 * @param	InClass		The class to find the factory for.
 * @return				A pointer to the factory to use.  NULL if no factories support this class.
 */
UActorFactory* UEditorEngine::FindActorFactory( const UClass* InClass )
{
	for( INT i = 0 ; i < ActorFactories.Num() ; ++i )
	{
		UActorFactory* Factory = ActorFactories(i);

		// force NewActorClass update
		Factory->GetDefaultActor();
		if( Factory->NewActorClass == InClass )
		{
			return Factory;
		}
	}

	return NULL;
}

/** 
 * Gets the world space cursor info from the current mouse position
 * 
 * @param InViewportClient	The viewport client to check for mouse position and to set up the scene view.
 * @return					An FViewportCursorLocation containing information about the mouse position in world space.
 */
static FViewportCursorLocation GetCursorWorldLocationFromMousePos( FEditorLevelViewportClient& InViewportClient )
{
	// Create the scene view context
	FSceneViewFamilyContext ViewFamily(
		InViewportClient.Viewport, InViewportClient.GetScene(),
		InViewportClient.ShowFlags,
		GWorld->GetTimeSeconds(),
		GWorld->GetDeltaSeconds(),
		GWorld->GetRealTimeSeconds(),
		NULL,
		InViewportClient.IsRealtime()
		);

	// Calculate the scene vew
	FSceneView* View = InViewportClient.CalcSceneView( &ViewFamily );

	// Construct an FViewportCursorLocation which calculates world space postion from the scene view and mouse pos.
	return FViewportCursorLocation( View, 
									&InViewportClient, 
									InViewportClient.CurrentMousePos.X, 
									InViewportClient.CurrentMousePos.Y
								   );
}

/**
* Uses the supplied factory to create an actor at the clicked location ands add to level.
*
* @param	Factory					The factor to create the actor from.  Must be non-NULL.
* @param	bIgnoreCanCreate		[opt] If TRUE, don't call UActorFactory::CanCreateActor.  Default is FALSE.
* @param	bUseSurfaceOrientation	[opt] If TRUE, align new actor's orientation to the underlying surface normal.  Default is FALSE.
 * @param	bUseCurrentSelection	[opt] If TRUE, fills in the factory properties using the currently selected object(s)
* @return							A pointer to the new actor, or NULL on fail.
*/
AActor* UEditorEngine::UseActorFactory(UActorFactory* Factory, UBOOL bIgnoreCanCreate, UBOOL bUseSurfaceOrientation, UBOOL bUseCurrentSelection )
{
	check( Factory );

	// Were we asked to populate the factory using the currently selected editor objects?
	if( bUseCurrentSelection )
	{
		// ensure that all selected assets are loaded
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
		Factory->AutoFillFields( GetSelectedObjects() );
	}

	UBOOL bIsAllowedToCreateActor = TRUE;
	if ( !bIgnoreCanCreate )
	{
		FString ActorErrorMsg;
		if( !Factory->CanCreateActor( ActorErrorMsg ) )
		{
			bIsAllowedToCreateActor = FALSE;
			appMsgf( AMT_OK, *LocalizeUnrealEd( *ActorErrorMsg ) );
		}
	}

	AActor* Actor = NULL;
	if ( bIsAllowedToCreateActor )
	{
		AActor* NewActorTemplate = Factory->GetDefaultActor();
		FVector Collision = NewActorTemplate->GetCylinderExtent();

		if( NewActorTemplate->bEdShouldSnap )
		{
			Constraints.Snap( ClickLocation, FVector(0, 0, 0) );
		}

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

		// Get cursor origin and direction in world space.
		FViewportCursorLocation CursorLocation = GetCursorWorldLocationFromMousePos( *GCurrentLevelEditingViewportClient );

		FLOAT DistanceMultiplier = 1.0f;
		if( CursorLocation.GetViewportType() != LVT_Perspective )
		{
			// If an asset was dropped in an orthographic view,
			// adjust the location so the asset will not be positioned at the camera origin (which is at the edge of the world for ortho cams)
			DistanceMultiplier = HALF_WORLD_MAX;
		}
		
		const FIntPoint CursorPos = CursorLocation.GetCursorPos();
		HHitProxy* HitProxy = GCurrentLevelEditingViewportClient->Viewport->GetHitProxy( CursorPos.X, CursorPos.Y );
		if( HitProxy == NULL )
		{
			// If the hit proxy is null, we clicked on the background so we need to calculate our own click location since the old one will be off at the worlds edge somewhere.
			ClickLocation = CursorLocation.GetOrigin() + CursorLocation.GetDirection() * DistanceMultiplier;
		}

		// Calculate the new actors location from the mouse click location and collision information.
		FVector Location = ClickLocation + ClickPlane * (FBoxPushOut(ClickPlane, Collision) + 0.1);

		if( NewActorTemplate->bEdShouldSnap )
		{
			Constraints.Snap( Location, FVector(0, 0, 0) );
		}

		// Orient the new actor with the surface normal?
		const FRotator SurfaceOrientation( ClickPlane.Rotation() );
		const FRotator* const Rotation = bUseSurfaceOrientation ? &SurfaceOrientation : NULL;

		// Check to see if the user wants to place the actor into a streaming grid network
		ULevel* DesiredLevel = EditorLevelUtils::GetLevelForPlacingNewActor( Location );

		// Don't spawn the actor if the current level is locked.
		if( !FLevelUtils::IsLevelLocked( DesiredLevel ) )
		{
			// Check to see if the level it's being added to is hidden and ask the user if they want to proceed
			const UBOOL bLevelVisible = FLevelUtils::IsLevelVisible( DesiredLevel );
			if ( bLevelVisible || appMsgf( AMT_OKCancel, TEXT("Current level [%s] is hidden, actor will also be hidden until level is visible"), *DesiredLevel->GetOutermost()->GetName() ) )
			{
				// Make sure the desired level is current
				ULevel* OldCurrentLevel = GWorld->CurrentLevel;
				GWorld->CurrentLevel = DesiredLevel;

				const FScopedTransaction Transaction( TEXT("Create Actor") );

				// Create the actor.
				Actor = Factory->CreateActor( &Location, Rotation, NULL ); 
				if(Actor)
				{
					// Apply any static mesh tool settings if we placed a static mesh. 
					OnPlaceStaticMeshActor( Actor, bUseSurfaceOrientation );

					if( CursorLocation.GetViewportType() == LVT_Perspective && HitProxy == NULL  )
					{
						// Move the actor in front of the camera if the cursor location is in the perspective viewport and we dont have a valid hit proxy (which would place the actor somewhere else)
						MoveActorInFrontOfCamera( *Actor, CursorLocation.GetOrigin(), CursorLocation.GetDirection() );
					}

					SelectNone( FALSE, TRUE );
					SelectActor( Actor, TRUE, NULL, TRUE );
					Actor->InvalidateLightingCache();
					Actor->PostEditMove( TRUE );

					// Make sure the actors visibility reflects that of the level it's in
					if ( !bLevelVisible )
					{
						Actor->bHiddenEdLayer = TRUE;
						Actor->ForceUpdateComponents( FALSE, FALSE );
					}
				}

				// Restore current level
				GWorld->CurrentLevel = OldCurrentLevel;

				RedrawLevelEditingViewports();

				if ( Actor )
				{
					Actor->MarkPackageDirty();
					GCallbackEvent->Send( CALLBACK_LevelDirtied );
					GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
				}
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			appMsgf(AMT_OK, TEXT("AddActor: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
			return NULL;
		}
	}

	if ( Factory != NULL )
	{
		// clear any factory references to the selected objects
		Factory->ClearFields();
	}
	return Actor;
}

/**
 * Replaces the selected Actors with the same number of a different kind of Actor
 * if a Factory is specified, it is used to spawn the requested Actors, otherwise NewActorClass is used (one or the other must be specified)
 * note that only Location, Rotation, Drawscale, Drawscale3D, Tag, and Group are copied from the old Actors
 * 
 * @param Factory - the Factory to use to create Actors
 */
void UEditorEngine::ReplaceSelectedActors(UActorFactory* Factory, UClass* NewActorClass)
{
	// Provide the option to abort the delete if e.g. Kismet-referenced actors are selected.
	UBOOL bIgnoreKismetReferenced = FALSE;
	if (ShouldAbortActorDeletion(bIgnoreKismetReferenced))
	{
		return;
	}
	else if (Factory != NULL)
	{
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

		Factory->AutoFillFields(GetSelectedObjects());

		FString ActorErrorMsg;
		if (!Factory->CanCreateActor(ActorErrorMsg))
		{
			// clear any factory references to the selected objects
			Factory->ClearFields();
			appMsgf(AMT_OK, *LocalizeUnrealEd(*ActorErrorMsg));
			return;
		}
	}
	else if (NewActorClass == NULL)
	{
		debugf(NAME_Error, TEXT("UEditorEngine::ReplaceSelectedActors() called with NULL parameters!"));
		return;
	}

	{
		const FScopedTransaction Transaction(TEXT("Replace Actor(s)"));

		ULevel* OldCurrentLevel = GWorld->CurrentLevel;

		// construct a list of Actors to replace in a separate pass so we can modify the selection set as we perform the replacement
		TArray<AActor*> ActorsToReplace;
		for (FSelectionIterator It = GetSelectedActorIterator(); It; ++It)
		{
			if (!It->IsInPrefabInstance() || It->IsA(APrefabInstance::StaticClass()))
			{
				ActorsToReplace.AddItem((AActor*)*It);
			}
		}

		while (ActorsToReplace.Num() > 0)
		{
			AActor* OldActor = ActorsToReplace.Pop();
			GWorld->CurrentLevel = OldActor->GetLevel();
			AActor* NewActor = NULL;
			// create the actor
			if (Factory != NULL)
			{
				NewActor = Factory->CreateActor(&OldActor->Location, &OldActor->Rotation, NULL);
			}
			else
			{
				NewActor = GWorld->SpawnActor(NewActorClass, NAME_None, OldActor->Location, OldActor->Rotation);
			}
			if (NewActor != NULL)
			{
				if(!GEditorModeTools().GetReplaceRespectsScale())
				{
					NewActor->DrawScale = 1.0f;
					NewActor->DrawScale3D = FVector(1.0f, 1.0f, 1.0f);
				}
				else
				{
					NewActor->DrawScale = OldActor->DrawScale;
					NewActor->DrawScale3D = OldActor->DrawScale3D;
				}
				NewActor->Layer = OldActor->Layer;
				NewActor->Tag = OldActor->Tag;

				NewActor->EditorReplacedActor(OldActor);
				
				SelectActor(OldActor, FALSE, NULL, TRUE);
				SelectActor(NewActor, TRUE, NULL, TRUE);
				NewActor->InvalidateLightingCache();
				NewActor->PostEditMove(TRUE);
				NewActor->MarkPackageDirty();
				if (OldActor->IsA(APrefabInstance::StaticClass()))
				{
					((APrefabInstance*)OldActor)->DestroyPrefab(GetSelectedActors());
				}
				GWorld->EditorDestroyActor(OldActor, TRUE);
			}
		}

		GWorld->CurrentLevel = OldCurrentLevel;
	}

	RedrawLevelEditingViewports();

	GCallbackEvent->Send( CALLBACK_LevelDirtied );
	GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );

	if ( Factory != NULL )
	{
		// clear any factory references to the selected objects
		Factory->ClearFields();
	}
}

/**
 * Changes the state of preview mesh mode to on or off. 
 *
 * @param	bState	Enables the preview mesh mode if TRUE; Disables the preview mesh mode if FALSE. 
 */
void UEditorEngine::SetPreviewMeshMode( UBOOL bState )
{
	// Only change the state if it's different than the current.
	if( bShowPreviewMesh != bState )
	{
		// Set the preview mesh mode state. 
		bShowPreviewMesh = !bShowPreviewMesh;

		UBOOL bHavePreviewMesh = (PreviewMeshComp != NULL);

		// It's possible that the preview mesh hasn't been loaded yet,
		// such as on first-use for the preview mesh mode or there 
		// could be valid mesh names provided in the INI. 
		if( !bHavePreviewMesh )
		{
			bHavePreviewMesh = LoadPreviewMesh( GUnrealEd->PreviewMeshIndex );
		}

		// If we have a	preview mesh, change it's visibility based on the preview state. 
		if( bHavePreviewMesh )
		{
			// The hidden state is opposite of the mesh 
			// preview mode (On = Unhidden; Off = Hidden).
			PreviewMeshComp->SetHiddenEditor( !bShowPreviewMesh );
			RedrawLevelEditingViewports();
		}
		else
		{
			// Without a preview mesh, we can't really use the preview mesh mode. 
			// So, disable it even if the caller wants to enable it. 
			bShowPreviewMesh = FALSE;
		}
	}
}

/**
 * Updates the position of the preview mesh in the level. 
 */
void UEditorEngine::UpdatePreviewMesh()
{
	if( bShowPreviewMesh )
	{
		// The component should exist by now. Is the bPlayerHeight state 
		// manually changed instead of calling SetPreviewMeshMode()?
		check(PreviewMeshComp);

		// Use the cursor world location as the starting location for the line check. 
		FViewportCursorLocation CursorLocation = GetCursorWorldLocationFromMousePos( *GCurrentLevelEditingViewportClient );
		FVector LineCheckStart = CursorLocation.GetOrigin();
		FVector LineCheckEnd = CursorLocation.GetOrigin() + CursorLocation.GetDirection() * HALF_WORLD_MAX;

		// Perform a line check from the camera eye to the surface to place the preview mesh. 
		FCheckResult Hit(1.0f);
		if( !GWorld->SingleLineCheck(Hit, NULL, LineCheckEnd, LineCheckStart, TRACE_Terrain | TRACE_Level | TRACE_LevelGeometry) )
		{
			// Dirty the transform so UpdateComponent will actually update the transforms. 
			PreviewMeshComp->DirtyTransform();
			PreviewMeshComp->UpdateComponent(GWorld->Scene, NULL, FTranslationMatrix(Hit.Location) );
		}

		// Redraw the viewports because the mesh won't 
		// be shown or hidden until that happens. 
		RedrawLevelEditingViewports();
	}
}

/**
 * Changes the preview mesh to the next one. 
 */
void UEditorEngine::CyclePreviewMesh()
{
	const INT StartingPreviewMeshIndex = GUnrealEd->PreviewMeshIndex;
	INT CurrentPreviewMeshIndex = StartingPreviewMeshIndex;
	UBOOL bPreviewMeshFound = FALSE;

	do
	{
		// Cycle to the next preview mesh. 
		CurrentPreviewMeshIndex++;

		// If we reached the max index, start at index zero.
		if( CurrentPreviewMeshIndex == PreviewMeshNames.Num() )
		{
			CurrentPreviewMeshIndex = 0;
		}

		// Load the mesh (if not already) onto the mesh actor. 
		bPreviewMeshFound = LoadPreviewMesh(CurrentPreviewMeshIndex);

		if( bPreviewMeshFound )
		{
			// Save off the index so we can reference it later when toggling the preview mesh mode. 
			GUnrealEd->PreviewMeshIndex = CurrentPreviewMeshIndex;
			bPreviewMeshFound = TRUE;
		}

	// Keep doing this until we found another valid mesh, or we cycled through all possible preview meshes. 
	} while( !bPreviewMeshFound && (StartingPreviewMeshIndex != CurrentPreviewMeshIndex) );
}

/**
 * Attempts to load a preview mesh from the array preview meshes at the given index.
 *
 * @param	Index	The index of the name in the PreviewMeshNames array from the editor user settings.
 * @return	TRUE if a mesh was loaded; FALSE, otherwise. 
 */
UBOOL UEditorEngine::LoadPreviewMesh( INT Index )
{
	UBOOL bMeshLoaded = FALSE;

	// If there are no mesh names loaded, the preview mesh 
	// names may not have been loaded yet. Try to load them. 
	if( PreviewMeshNames.Num() == 0 )
	{
		GConfig->GetSingleLineArray(TEXT("EditorPreviewMesh"), TEXT("PreviewMeshNames"), PreviewMeshNames, GEditorUserSettingsIni);
	}

	if( PreviewMeshNames.IsValidIndex(Index) )
	{
		const FString MeshName = PreviewMeshNames(Index);

		// If we don't have a preview mesh component in the world yet, create one. 
		if( !PreviewMeshComp )
		{
			PreviewMeshComp = ConstructObject<UStaticMeshComponent>( UStaticMeshComponent::StaticClass(), UObject::GetTransientPackage() );
			check(PreviewMeshComp);

			// Attach the component to the scene even if the preview mesh doesn't load.
			PreviewMeshComp->ConditionalAttach( GWorld->Scene, NULL, FMatrix::Identity );
		}

		// Load the new mesh, if not already loaded. 
		UStaticMesh* PreviewMesh = LoadObject<UStaticMesh>( NULL, *MeshName, NULL, LOAD_None, NULL );

		// Swap out the meshes if we loaded or found the given static mesh. 
		if( PreviewMesh )
		{
			bMeshLoaded = TRUE;
			PreviewMeshComp->StaticMesh = PreviewMesh;
		}
		else
		{
			warnf( TEXT("Couldn't load the PreviewMeshNames for the player at index, %d, with the name, %s."), Index, *MeshName );
		}
	}
	else
	{
		debugf( TEXT("Invalid array index, %d, provided for PreviewMeshNames in UEditorEngine::LoadPreviewMesh"), Index );
	}

	return bMeshLoaded;
}

/* Gets the common components of a specific type between two actors so that they may be copied.
 * 
 * @param InOldActor		The actor to copy component properties from
 * @param InNewActor		The actor to copy to
 */
static void CopyLightComponentProperties( const AActor& InOldActor, AActor& InNewActor )
{
	// Since this is only being used for lights, make sure only the light component can be copied.
	const UClass* CopyableComponentClass =  ULightComponent::StaticClass();
	
	// Get the light component from the default actor of source actors class.
	// This is so we can avoid copying properties that have not changed. 
	// using ULightComponent::StaticClass()->GetDefaultObject() will not work since each light actor sets default component properties differently.
	ALight* OldActorDefaultObject = Cast<ALight>(InOldActor.GetClass()->GetDefaultActor());
	check(OldActorDefaultObject);
	UActorComponent* DefaultLightComponent = OldActorDefaultObject->LightComponent;
	check(DefaultLightComponent);
	
	// The component we are copying from class
	UClass* CompToCopyClass = NULL;
	UActorComponent* LightComponentToCopy = NULL;

	// Go through the old actor's components and look for a light component to copy.  
	for( INT CompToCopyIdx = 0; CompToCopyIdx <  InOldActor.AllComponents.Num(); ++CompToCopyIdx )
	{
		UActorComponent* Component = InOldActor.AllComponents(CompToCopyIdx);

		if( Component->IsA( CopyableComponentClass ) ) 
		{
			// A light component has been found. 
			CompToCopyClass = Component->GetClass();
			LightComponentToCopy = Component;
			break;
		}
	}

	// The light component from the new actor
	UActorComponent* NewActorLightComponent = NULL;
	// The class of the new actors light component
	const UClass* CommonLightComponentClass = NULL;

	// Dont do anything if there is no valid light component to copy from
	if( LightComponentToCopy )
	{
		// Find a light component to overwrite in the new actor
		for( INT NewCompIdx = 0; NewCompIdx < InNewActor.AllComponents.Num(); ++NewCompIdx )
		{
			UActorComponent* Component =  InNewActor.AllComponents( NewCompIdx );
			// Find a common component class between the new and old actor.   
			// This needs to be done so we can copy as many properties as possible. 
			// For example: if we are converting from a point light to a spot light, the point light component will be the common superclass.
			// That way we can copy properties like light radius, which would have been impossible if we just took the base LightComponent as the common class.
			const UClass* CommonSuperclass = Component->FindNearestCommonBaseClass( CompToCopyClass );

			if( CommonSuperclass->IsChildOf( CopyableComponentClass ) )
			{
				NewActorLightComponent = Component;
				CommonLightComponentClass = CommonSuperclass;
			}
		}
	}

	// Don't do anything if there is no valid light component to copy to
	if( NewActorLightComponent )
	{
		UBOOL bCopiedAnyProperty = FALSE;
		
		// Find and copy the lightmass settings directly as they need to be examined and copied individually and not by the entire light mass settings struct
		const FString LightmassPropertyName = TEXT("LightmassSettings");

		UProperty* PropertyToCopy = NULL;
		for( UProperty* Property = CompToCopyClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
		{
			if( Property->GetName() == LightmassPropertyName )
			{
				// Get the offset in the old actor where lightmass properties are stored.
				PropertyToCopy = Property;
				break;
			}
		}
		
		if( PropertyToCopy != NULL )
		{
			// Find the location of the lightmass settings in the new actor (if any)
			for( UProperty* NewProperty = NewActorLightComponent->GetClass()->PropertyLink; NewProperty != NULL; NewProperty = NewProperty->PropertyLinkNext )
			{
				if( NewProperty->GetName() == LightmassPropertyName )
				{
					UStructProperty* OldLightmassProperty = Cast<UStructProperty>(PropertyToCopy);
					UStructProperty* NewLightmassProperty = Cast<UStructProperty>(NewProperty);
					
					// The lightmass settings are a struct property so the cast should never fail.
					check(OldLightmassProperty);
					check(NewLightmassProperty);

					// Iterate through each property field in the lightmass settings struct that we are copying from...
					for( TFieldIterator<UProperty> OldIt(OldLightmassProperty->Struct); OldIt; ++OldIt)
					{
						UProperty* OldLightmassField = *OldIt;

						// And search for the same field in the lightmass settings struct we are copying to.
						// We should only copy to fields that exist in both structs.
						// Even though their offsets match the structs may be different depending on what type of light we are converting to
						UBOOL bPropertyFieldFound = FALSE;
						for( TFieldIterator<UProperty> NewIt(NewLightmassProperty->Struct); NewIt; ++NewIt)
						{
							UProperty* NewLightmassField = *NewIt;
							if( OldLightmassField->GetName() == NewLightmassField->GetName() )
							{
								// The field is in both structs.  Ok to copy
								// The offset of each value is the offset of the lightmass settings plus the offset of the actual field in the lightmass struct
								UBOOL bIsIdentical = OldLightmassField->Identical( (BYTE*)LightComponentToCopy+PropertyToCopy->Offset+OldLightmassField->Offset, (BYTE*)DefaultLightComponent+PropertyToCopy->Offset+OldLightmassField->Offset);
								if( !bIsIdentical )
								{
									// Copy if the value has changed
									OldLightmassField->CopySingleValue( (BYTE*)NewActorLightComponent+NewProperty->Offset+NewLightmassField->Offset, (BYTE*)LightComponentToCopy+PropertyToCopy->Offset+OldLightmassField->Offset);
									bCopiedAnyProperty = TRUE;
								}
								break;
							}
						}
					}
					// No need to continue once we have found the lightmass settings
					break;
				}
			}
		}
	
		

		// Now Copy the light component properties.
		for( UProperty* Property = CommonLightComponentClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
		{
			UBOOL bIsNative = (Property->PropertyFlags & CPF_Native);
			UBOOL bIsTransient = (Property->PropertyFlags & CPF_Transient);
			// Properties are identical if they have not changed from the light component on the default source actor
			UBOOL bIsIdentical = Property->Identical( (BYTE*)LightComponentToCopy + Property->Offset, (BYTE*)DefaultLightComponent + Property->Offset );
			UBOOL bIsComponent = (Property->PropertyFlags & CPF_Component);
			
			if ( !bIsNative && !bIsTransient && !bIsIdentical && !bIsComponent && Property->GetName() != LightmassPropertyName )
			{
				bCopiedAnyProperty = TRUE;
				// Copy only if not native, not transient, not identical, not a component (at this time don't copy components within components)
				// Also dont copy lightmass settings, those were examined and taken above
				Property->CopyCompleteValue( (BYTE*)NewActorLightComponent + Property->Offset, (BYTE*)LightComponentToCopy + Property->Offset );
			}
		}	

		if (bCopiedAnyProperty)
		{
			NewActorLightComponent->PostEditChange();
		}
	}
}

/* Checks to make sure light conversation is possible 
 * At this time the only check is to make sure two dominant directional lights cant be created.
 */ 
static UBOOL CanConvertLightToClass( const AActor* ActorToConvert, const UClass* ReplaceWithClass )
{
	UBOOL bCanConvert = TRUE;
	// If trying to replace with a dominant directional light, make sure there aren't any other dominant lights already (or that the actor
	// being converted is the lone dominant directional light)
	if( ReplaceWithClass->IsChildOf( ADominantDirectionalLight::StaticClass() ) && !ActorToConvert->IsA( ADominantDirectionalLight::StaticClass() ) )
	{
		UBOOL bHasDominantDirLight = FALSE;
		// Look for other dominant directional lights.  There can only be one
		for( TObjectIterator<ADominantDirectionalLight> ObjectIt; ObjectIt; ++ObjectIt )
		{
			ADominantDirectionalLight* CurrentLight = *ObjectIt;
			if ( !CurrentLight->IsPendingKill()
				&& CurrentLight->LightComponent 
				&& CurrentLight->LightComponent->bEnabled )
			{
				GWarn->Logf( *LocalizeUnrealEd("Error_CouldNotCreateActor_AlreadyADominantDirectionalLight") );			
				bCanConvert = FALSE;
				break;
			}
		}
	}

	return bCanConvert;
}

/** 
 * Converts passed in light actors into new actors of another type.
 * Note: This replaces the old actor with the new actor.
 * Most properties of the old actor that can be copied are copied to the new actor during this process.
 * Properties that can be copied are ones found in a common superclass between the actor to convert and the new class. 
 * Common light component properties between the two classes are also copied
 *
 * @param	ActorsToConvert	A list of actors to convert
 * @param	ConvertToClass	The light class we are going to convert to. 
 */
void UEditorEngine::ConvertLightActors( const TArray< AActor* >& ActorsToConvert, UClass* ConvertToClass )
{
	// Store the current level as we are about to change it to spawn actors to their own levels
	ULevel* PrevCurrentLevel = GWorld->CurrentLevel;

	GWarn->BeginSlowTask( *LocalizeUnrealEd("ConvertingLights"), TRUE );

	INT NumLightsConverted = 0;
	INT NumLightsToConvert = ActorsToConvert.Num();

	// Convert each light 
	for( INT ActorIdx = 0; ActorIdx < ActorsToConvert.Num(); ++ActorIdx )
	{
		AActor* ActorToConvert = ActorsToConvert( ActorIdx );

		// The class of the actor we are about to replace
		UClass* ClassToReplace = ActorToConvert->GetClass();

		// Set the current level to the level where the convertible actor resides
		GWorld->CurrentLevel = ActorToConvert->GetLevel();
		checkSlow( GWorld->CurrentLevel != NULL );

		if( !CanConvertLightToClass( ActorToConvert, ConvertToClass ) )
		{
			// This light cant be converted
			continue;
		}

		// Find a common superclass between the actors so we know what properties to copy
		const UClass* CommonSuperclass = ActorToConvert->FindNearestCommonBaseClass( ConvertToClass );
		check ( CommonSuperclass );
		
		// spawn the new actor
		AActor* NewActor = NULL;	
		
		// Take the old actors location always, not rotation.  If rotation was changed on the source actor, it will be copied below.
		NewActor = GWorld->SpawnActor( ConvertToClass, NAME_None, ActorToConvert->Location, ConvertToClass->GetDefaultActor()->Rotation );
		// The new actor must exist
		check(NewActor);

		// Copy non component properties from the old actor to the new actor
		for( UProperty* Property = CommonSuperclass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
		{
			UBOOL bIsNative = (Property->PropertyFlags & CPF_Native);
			UBOOL bIsTransient = (Property->PropertyFlags & CPF_Transient);
			UBOOL bIsComponentProp = (Property->PropertyFlags & CPF_Component);
			UBOOL bIsIdentical = Property->Identical( (BYTE*)ActorToConvert + Property->Offset, (BYTE*)ClassToReplace->GetDefaultActor() + Property->Offset );

			if ( !bIsNative && !bIsTransient && !bIsIdentical && !bIsComponentProp && Property->GetName() != TEXT("Tag") )
			{
				// Copy only if not native, not transient, not identical, not a component.
				// Copying components directly here is a bad idea because the next garbage collection will delete the component since we are deleting its outer.  

				// Also do not copy the old actors tag.  That will always come up as not identical since the default actor's Tag is "None" and SpawnActor uses the actor's class name
				// The tag will be examined for changes later.
				Property->CopyCompleteValue((BYTE*)NewActor + Property->Offset, (BYTE*)ActorToConvert + Property->Offset);
			}
		}

		if( ActorToConvert->Tag != ActorToConvert->GetClass()->GetFName() )
		{
			// The actors tag changed from default (which is the class name)
			NewActor->Tag = ActorToConvert->Tag;
		}

		// Copy common light component properties
		CopyLightComponentProperties( *ActorToConvert, *NewActor );

		// Select the new actor
		GEditor->SelectActor( ActorToConvert, FALSE, NULL, TRUE );
		GEditor->SelectActor( NewActor, TRUE, NULL, TRUE );

		NewActor->InvalidateLightingCache();
		NewActor->PostEditChange();
		NewActor->PostEditMove( TRUE );
		NewActor->Modify();

		// We have converted another light.
		++NumLightsConverted;

		GWarn->Logf( TEXT("Converted: %s to %s"), *ActorToConvert->GetName(), *NewActor->GetName() );

		GWarn->StatusUpdatef( NumLightsConverted, NumLightsToConvert,  TEXT("Converted: %s to %s"), *ActorToConvert->GetName(), *NewActor->GetName() );

		// Destroy the old actor.
		GWorld->EditorDestroyActor( ActorToConvert, TRUE );
	}

	GEditor->RedrawLevelEditingViewports();

	GCallbackEvent->Send( CALLBACK_LevelDirtied );
	GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );

	// Reset the level back to what it was before
	GWorld->CurrentLevel = PrevCurrentLevel;

	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	GWarn->EndSlowTask();
}

/**
 * Internal helper function to copy component properties from one actor to another. Only copies properties
 * from components if the source actor, source actor class default object, and destination actor all contain
 * a component of the same name (specified by parameter) and all three of those components share a common base
 * class, at which point properties from the common base are copied. Component template names are used instead of
 * component classes because an actor could potentially have multiple components of the same class.
 *
 * @param	SourceActor		Actor to copy component properties from
 * @param	DestActor		Actor to copy component properties to
 * @param	ComponentNames	Set of component template names to attempt to copy
 */
void CopyActorComponentProperties( const AActor* SourceActor, AActor* DestActor, const TSet<FString>& ComponentNames )
{
	// Don't attempt to copy anything if the user didn't specify component names to copy
	if ( ComponentNames.Num() > 0 )
	{
		check( SourceActor && DestActor );
		const AActor* SrcActorDefaultActor = SourceActor->GetClass()->GetDefaultActor();
		check( SrcActorDefaultActor );

		// Construct a mapping from the default actor of its relevant component names to its actual components. Here relevant component
		// names are those that match a name provided as a parameter.
		TMap<FString, const UActorComponent*> NameToDefaultComponentMap; 
		for ( TArray<UActorComponent*>::TConstIterator CompIter( SrcActorDefaultActor->Components ); CompIter; ++CompIter )
		{
			const UActorComponent* CurComp = *CompIter;
			check( CurComp );

			const FString CurCompName = CurComp->TemplateName.ToString();
			if ( ComponentNames.Contains( CurCompName ) )
			{
				NameToDefaultComponentMap.Set( CurCompName, CurComp );
			}
		}

		// Construct a mapping from the source actor of its relevant component names to its actual components. Here relevant component names
		// are those that match a name provided as a parameter.
		TMap<FString, const UActorComponent*> NameToSourceComponentMap;
		for ( TArray<UActorComponent*>::TConstIterator CompIter( SourceActor->Components ); CompIter; ++CompIter )
		{
			const UActorComponent* CurComp = *CompIter;
			check( CurComp );

			const FString CurCompName = CurComp->TemplateName.ToString();
			if ( ComponentNames.Contains( CurCompName ) )
			{
				NameToSourceComponentMap.Set( CurCompName, CurComp );
			}
		}

		UBOOL bCopiedAnyProperty = FALSE;

		// Iterate through all of the destination actor's components to find the ones which should have properties copied into them.
		for ( TArray<UActorComponent*>::TConstIterator DestCompIter( DestActor->Components ); DestCompIter; ++DestCompIter )
		{
			const UActorComponent* CurComp = *DestCompIter;
			check( CurComp );

			const FString CurCompName = CurComp->TemplateName.ToString();
			
			// Check if the component is one that the user wanted to copy properties into
			if ( ComponentNames.Contains( CurCompName ) )
			{
				const UActorComponent** DefaultComponent = NameToDefaultComponentMap.Find( CurCompName );
				const UActorComponent** SourceComponent = NameToSourceComponentMap.Find( CurCompName );

				// Make sure that both the default actor and the source actor had a component of the same name
				if ( DefaultComponent && SourceComponent )
				{
					const UClass* CommonBaseClass = NULL;
					const UClass* DefaultCompClass = (*DefaultComponent)->GetClass();
					const UClass* SourceCompClass = (*SourceComponent)->GetClass();
					
					// Handle the unlikely case of the default component and the source actor component not being the exact same class by finding
					// the common base class across all three components (default, source, and destination)
					if ( DefaultCompClass != SourceCompClass )
					{
						const UClass* CommonBaseClassWithDefault = CurComp->FindNearestCommonBaseClass( DefaultCompClass );
						const UClass* CommonBaseClassWithSource = CurComp->FindNearestCommonBaseClass( SourceCompClass );
						if ( CommonBaseClassWithDefault && CommonBaseClassWithSource )
						{
							// If both components yielded the same common base, then that's the common base of all three
							if ( CommonBaseClassWithDefault == CommonBaseClassWithSource )
							{
								CommonBaseClass = CommonBaseClassWithDefault;
							}
							// If not, find a common base across all three components
							else
							{
								CommonBaseClass = const_cast<UClass*>(CommonBaseClassWithDefault)->GetDefaultObject()->FindNearestCommonBaseClass( CommonBaseClassWithSource );
							}
						}
					}
					else
					{
						CommonBaseClass = CurComp->FindNearestCommonBaseClass( DefaultCompClass );
					}

					// If all three components have a base class in common, copy the properties from that base class from the source actor component
					// to the destination
					if ( CommonBaseClass )
					{
						// Iterate through the properties, only copying those which are non-native, non-transient, non-component, and not identical
						// to the values in the default component
						for ( UProperty* Property = CommonBaseClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
						{
							const UBOOL bIsNative = ( Property->PropertyFlags & CPF_Native );
							const UBOOL bIsTransient = ( Property->PropertyFlags & CPF_Transient );
							const UBOOL bIsIdentical = Property->Identical( (BYTE*)(*SourceComponent) + Property->Offset, (BYTE*)(*DefaultComponent) + Property->Offset );
							const UBOOL bIsComponent = ( Property->PropertyFlags & CPF_Component );

							if ( !bIsNative && !bIsTransient && !bIsIdentical && !bIsComponent )
							{
								bCopiedAnyProperty = TRUE;
								Property->CopyCompleteValue( (BYTE*)CurComp + Property->Offset, (BYTE*)(*SourceComponent) + Property->Offset );
							}
						}
					}
				}
			}
		}

		// If any properties were copied at all, alert the actor to the changes
		if ( bCopiedAnyProperty )
		{
			DestActor->PostEditChange();
		}
	}
}

/**
 * Converts passed in actors into new actors of the specified type.
 * Note: This replaces the old actors with brand new actors while attempting to preserve as many properties as possible.
 * Properties of the actors components are also attempted to be copied for any component names supplied in the third parameter.
 * If a component name is specified, it is only copied if a component of the specified name exists in the source and destination actors,
 * as well as in the class default object of the class of the source actor, and that all three of those components share a common base class.
 * This approach is used instead of simply accepting component classes to copy because some actors could potentially have multiple of the same
 * component type.
 *
 * @param	ActorsToConvert			Array of actors which should be converted to the new class type
 * @param	ConvertToClass			Class to convert the provided actors to
 * @param	ComponentsToConsider	Names of components to consider for property copying as well
 * @param	bIgnoreKismetRefActors	If TRUE, actors which are referenced by Kismet will not be converted
 */
void UEditorEngine::ConvertActors( const TArray<AActor*>& ActorsToConvert, UClass* ConvertToClass, const TSet<FString>& ComponentsToConsider, UBOOL bIgnoreKismetRefActors )
{
	// Store the current level as we are about to change it to spawn actors to their own levels
	ULevel* PrevCurrentLevel = GWorld->CurrentLevel;

	GWarn->BeginSlowTask( *LocalizeUnrealEd("ConvertingSkeletalMeshes"), TRUE );

	// This is kind of unpleasant, but we need to ensure that every actor in the world has its base properly updated. Unfortunately not all
	// editor code with attachment results in actors being added to their base's attached array, so the update is necessary in order
	// to restore base information later.
	for( FActorIterator ActorIter; ActorIter; ++ActorIter )
	{
		AActor* Actor = *ActorIter;
		if( Actor && !Actor->bDeleteMe )
		{
			Actor->EditorUpdateBase();
		}
	}

	INT NumActorsConverted = 0;
	INT NumActorsToConvert = ActorsToConvert.Num();

	for( INT ActorIdx = 0; ActorIdx < ActorsToConvert.Num(); ++ActorIdx )
	{
		AActor* ActorToConvert = ActorsToConvert( ActorIdx );

		// If the user wants to ignore actors that are referenced by kismet, skip any that are
		if ( bIgnoreKismetRefActors && ActorToConvert->IsReferencedByKismet() )
		{
			++NumActorsConverted;
			GWarn->Logf( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("ConvertSkeletalMeshKismetRefSkipped") ), *ActorToConvert->GetName() ) ) );
			continue;
		}

		// The class of the actor we are about to replace
		UClass* ClassToReplace = ActorToConvert->GetClass();

		// Set the current level to the level where the convertible actor resides
		GWorld->CurrentLevel = ActorToConvert->GetLevel();
		checkSlow( GWorld->CurrentLevel != NULL );

		// Find a common base class between the actors so we know what properties to copy
		const UClass* CommonBaseClass = ActorToConvert->FindNearestCommonBaseClass( ConvertToClass );
		check ( CommonBaseClass );

		// spawn the new actor
		AActor* NewActor = NULL;	

		// Take the old actors location always, not rotation.  If rotation was changed on the source actor, it will be copied below.
		NewActor = GWorld->SpawnActor( ConvertToClass, NAME_None, ActorToConvert->Location, ConvertToClass->GetDefaultActor()->Rotation, NULL, TRUE );
		check( NewActor );

		// Copy non component properties from the old actor to the new actor
		for( UProperty* Property = CommonBaseClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
		{
			const UBOOL bIsNative = (Property->PropertyFlags & CPF_Native);
			const UBOOL bIsTransient = (Property->PropertyFlags & CPF_Transient);
			const UBOOL bIsComponentProp = (Property->PropertyFlags & CPF_Component);
			const UBOOL bIsIdentical = Property->Identical( (BYTE*)ActorToConvert + Property->Offset, (BYTE*)ClassToReplace->GetDefaultActor() + Property->Offset );

			if ( !bIsNative && !bIsTransient && !bIsIdentical && !bIsComponentProp && Property->GetName() != TEXT("Tag") )
			{
				// Copy only if not native, not transient, not identical, and not a component.
				// Copying components directly here is a bad idea because the next garbage collection will delete the component since we are deleting its outer.  

				// Also do not copy the old actors tag.  That will always come up as not identical since the default actor's Tag is "None" and SpawnActor uses the actor's class name
				// The tag will be examined for changes later.
				Property->CopyCompleteValue( (BYTE*)NewActor + Property->Offset, (BYTE*)ActorToConvert + Property->Offset );
			}
		}

		if( ActorToConvert->Tag != ActorToConvert->GetClass()->GetFName() )
		{
			// The actors tag changed from default (which is the class name)
			NewActor->Tag = ActorToConvert->Tag;
		}

		// Copy properties from actor components
		CopyActorComponentProperties( ActorToConvert, NewActor, ComponentsToConsider );

		// Attempt to preserve base/attachments made prior to the conversion. Have to iterate in reverse here
		// as the later call to SetBase will invalidate an iterator by removing from the Attached array.
		for ( INT AttachedIdx = ActorToConvert->Attached.Num() - 1; AttachedIdx >= 0; --AttachedIdx )
		{
			AActor* BasedActor = ActorToConvert->Attached(AttachedIdx);
			const USkeletalMeshComponent* CurBaseComponent = BasedActor->BaseSkelComponent;
			USkeletalMeshComponent* NewBaseComponent = NULL;
	
			// If the current based actor specifies a component to attach to, we need to find the corresponding
			// component within the new actor. Iterate through each component searching for a match.
			if ( CurBaseComponent )
			{
				for ( TArray<UActorComponent*>::TIterator ComponentIter( NewActor->Components ); ComponentIter; ++ComponentIter )
				{
					USkeletalMeshComponent* CurSkelComponent = Cast<USkeletalMeshComponent>( *ComponentIter );
					if ( CurSkelComponent && CurBaseComponent->TemplateName == CurSkelComponent->TemplateName )
					{
						NewBaseComponent = CurSkelComponent;
						break;
					}
				}
			}

			// Re-base the attached actor to the new actor
			BasedActor->Modify();
			BasedActor->SetBase( NewActor, FVector(0,0,1), TRUE, NewBaseComponent, BasedActor->BaseBoneName );
		}

		// Select the new actor
		GEditor->SelectActor( ActorToConvert, FALSE, NULL, TRUE );
		GEditor->SelectActor( NewActor, TRUE, NULL, TRUE );


		NewActor->InvalidateLightingCache();
		NewActor->PostEditChange();
		NewActor->PostEditMove( TRUE );
		NewActor->Modify();

		++NumActorsConverted;

		GWarn->Logf( TEXT("Converted: %s to %s"), *ActorToConvert->GetName(), *NewActor->GetName() );

		GWarn->StatusUpdatef( NumActorsConverted, NumActorsToConvert,  TEXT("Converted: %s to %s"), *ActorToConvert->GetName(), *NewActor->GetName() );

		// If the user is not ignoring kismet referenced actors, then the old references in Kismet need to be forcefully
		// updated to point to the converted actor
		if ( !bIgnoreKismetRefActors )
		{
			USequence* RootSeq = GWorld->GetGameSequence( ActorToConvert->GetLevel() );
			if ( RootSeq )
			{
				TArray<USequenceObject*> SeqObjects;
				RootSeq->FindSeqObjectsByObjectName( ActorToConvert->GetFName(), SeqObjects );

				for ( TArray<USequenceObject*>::TIterator SeqObjIter( SeqObjects ); SeqObjIter; ++SeqObjIter )
				{
					USequenceObject* CurSeqObj = *SeqObjIter;
					USequenceEvent* SeqEvent = Cast<USequenceEvent>( CurSeqObj );
					if( SeqEvent )
					{
						check( SeqEvent->Originator == ActorToConvert );
						SeqEvent->Originator = NewActor;
					}

					USeqVar_Object* SeqVar = Cast<USeqVar_Object>( CurSeqObj );
					if( SeqVar )
					{
						check( SeqVar->ObjValue == ActorToConvert );
						SeqVar->ObjValue = NewActor;
					}
				}

			}
		}

		// Destroy the old actor.
		GWorld->EditorDestroyActor( ActorToConvert, TRUE );
	}

	// Display the actor properties window
	GEditor->Exec( TEXT("FixupSkeletalMeshConversions") );

	GEditor->RedrawLevelEditingViewports();

	GCallbackEvent->Send( CALLBACK_LevelDirtied );
	GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );

	// Reset the level back to what it was before
	GWorld->CurrentLevel = PrevCurrentLevel;

	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	GWarn->EndSlowTask();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	FViewportCursorLocation
//	Contains information about a mouse cursor position within a viewport, transformed into the correct
//	coordinate system for the viewport.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
FViewportCursorLocation::FViewportCursorLocation( const FSceneView* View, FEditorLevelViewportClient* ViewportClient, INT X, INT Y )
:	Origin(EC_EventParm), Direction(EC_EventParm), CursorPos(X, Y)
{

	FVector4 ScreenPos = View->PixelToScreen(X, Y, 0);

	const FMatrix InvViewMatrix = View->ViewMatrix.Inverse();
	const FMatrix InvProjMatrix = View->InvProjectionMatrix;

	const FLOAT ScreenX = ScreenPos.X;
	const FLOAT ScreenY = ScreenPos.Y;

	ViewportType = ViewportClient->ViewportType;

	if(ViewportType == LVT_Perspective)
	{
		Origin = View->ViewOrigin;
		Direction = InvViewMatrix.TransformNormal(FVector(InvProjMatrix.TransformFVector4(FVector4(ScreenX * GNearClippingPlane,ScreenY * GNearClippingPlane,0.0f,GNearClippingPlane)))).SafeNormal();
	}
	else
	{
		Origin = InvViewMatrix.TransformFVector4(InvProjMatrix.TransformFVector4(FVector4(ScreenX,ScreenY,0.0f,1.0f)));
		Direction = InvViewMatrix.TransformNormal(FVector(0,0,1)).SafeNormal();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	FViewportClick::FViewportClick - Calculates useful information about a click for the below ClickXXX functions to use.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
FViewportClick::FViewportClick(const FSceneView* View,FEditorLevelViewportClient* ViewportClient,FName InKey,EInputEvent InEvent,INT X,INT Y)
:	FViewportCursorLocation(View, ViewportClient, X, Y)
,	Key(InKey), Event(InEvent)
{
	ControlDown = ViewportClient->Viewport->KeyState(KEY_LeftControl) || ViewportClient->Viewport->KeyState(KEY_RightControl);
	ShiftDown = ViewportClient->Viewport->KeyState(KEY_LeftShift) || ViewportClient->Viewport->KeyState(KEY_RightShift);
	AltDown = ViewportClient->Viewport->KeyState(KEY_LeftAlt) || ViewportClient->Viewport->KeyState(KEY_RightAlt);
}

namespace
{
	/**
	 * This function displays the socket snapping dialog allowing the user to snap the selected actor(s) to the one passed in.
	 * When shown, sockets are sorted with those closest to the user's click point on screen first.
	 * @param SkelMeshComponent	The skeletal mesh component of the actor on which actors are to be snapped. (the Target)
	 * @param LineOrigin		World-space point at the start of the user's mouse click ray.
	 * @param LineDirection		World-space direction of the user's mouse click ray.
	 */
	static void ShowSocketSnappingDialog(USkeletalMeshComponent* SkelMeshComponent, const FVector& LineOrigin, const FVector& LineDirection)
	{
		WxSocketSnappingDialog* SocketSnappingDialog = new WxSocketSnappingDialog(SkelMeshComponent, LineOrigin, LineDirection, GApp->EditorFrame, wxID_ANY, wxDefaultPosition, wxSize(350, 400));
		SocketSnappingDialog->ShowModal();
		GEditor->RedrawLevelEditingViewports();
	}

	/**
	 * This function picks a color from under the mouse in the viewport and adds a light with that color.
	 * This is to make it easy for LDs to add lights that fake radiosity.
	 * @param Viewport	Viewport to pick color from.
	 * @param Click		A class that has information about where and how the user clicked on the viewport.
	 */
	static void PickColorAndAddLight(FViewport* Viewport, const FViewportClick &Click)
	{
		// Read pixels from viewport.
		TArray<FColor> OutputBuffer;

		// We need to redraw the viewport before reading pixels otherwise we may be reading back from an old buffer.
		Viewport->Draw();
		Viewport->ReadPixels(OutputBuffer);

		// Sample the color we want.
		const INT ClickX = Click.GetClickPos().X;
		const INT ClickY = Click.GetClickPos().Y;
		const INT PixelIdx = ClickX + ClickY * (INT)Viewport->GetSizeX();

		if(PixelIdx < OutputBuffer.Num())
		{
			const FColor PixelColor = OutputBuffer(PixelIdx);
			
			UActorFactory* ActorFactory = GEditor->FindActorFactory( APointLight::StaticClass() );
			
			AActor* NewActor = GEditor->UseActorFactory( ActorFactory, TRUE );

			APointLight* Light = CastChecked<APointLight>(NewActor);
			UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>( Light->LightComponent );

			// Set properties to defaults that will make it easy for LDs to use this light for faking radiosity.
			PointLightComponent->LightColor = PixelColor;
			PointLightComponent->Brightness = 0.2f;
			PointLightComponent->Radius = 512.0f;

			if ( PointLightComponent->PreviewLightRadius )
			{
				PointLightComponent->PreviewLightRadius->SphereRadius = PointLightComponent->Radius;
				PointLightComponent->PreviewLightRadius->Translation = PointLightComponent->Translation;
			}
		}
	}

	/**
 	 * Creates an actor of the specified type, trying first to find an actor factory,
	 * falling back to "ACTOR ADD" exec and SpawnActor if no factory is found.
	 * Does nothing if ActorClass is NULL.
	 */
	static AActor* PrivateAddActor(UClass* ActorClass, UBOOL bUseSurfaceOrientation=FALSE)
	{
		if ( ActorClass )
		{
			// use an actor factory if possible
			UActorFactory* ActorFactory = GEditor->FindActorFactory( ActorClass );
			if( ActorFactory )
			{
				return GEditor->UseActorFactory( ActorFactory, FALSE, bUseSurfaceOrientation );
			}
			// otherwise use AddActor so that we can return the newly created actor
			else
			{
				// Determine if the actor is being added onto the backdrop. If so, and it is being added from a perspective viewport,
				// it will be moved in front of the camera.
				const FViewportCursorLocation& CursorLocation = GetCursorWorldLocationFromMousePos( *GCurrentLevelEditingViewportClient );
				const FIntPoint& CursorPos = CursorLocation.GetCursorPos();
				const UBOOL bOnBackdrop = ( GCurrentLevelEditingViewportClient->Viewport->GetHitProxy( CursorPos.X, CursorPos.Y ) == NULL );
		
				FVector Collision = ActorClass->GetDefaultActor()->GetCylinderExtent();
				AActor* CreatedActor = GEditor->AddActor( ActorClass,GEditor->ClickLocation + GEditor->ClickPlane * ( FBoxPushOut( GEditor->ClickPlane, Collision ) + 0.1 ) );
				
				// If the actor was added to the backdrop in a perspective viewport, move it in front of the camera 
				if ( CreatedActor && GCurrentLevelEditingViewportClient->ViewportType == LVT_Perspective && bOnBackdrop )
				{
					GEditor->MoveActorInFrontOfCamera( *CreatedActor, CursorLocation.GetOrigin(), CursorLocation.GetDirection() );
				}
				return CreatedActor;
			}
		}
		return NULL;
	}

	/**
	 * Spawns an actor using PrivateAddActor if holding down a key configured in UUnrealEdKeyBindings while clicking
	 *
	 * @param ViewportClient the viewport client handling a click event
	 * @param Click the data for the click being processed
	 */
	static UBOOL SpawnQuickActor (FEditorLevelViewportClient* ViewportClient, const FViewportClick& Click)
	{
		if ( Click.GetKey() == KEY_LeftMouseButton )
		{
			// See if a key associated with a quick actor is held down, then spawn the actor if it is.
			// This happens before the default keys to allow ini file changes to override existing behavior.
			const UUnrealEdOptions* Options = GUnrealEd->GetUnrealEdOptions();

			if (Options)
			{
				const UUnrealEdKeyBindings* KeyBindings = Options->EditorKeyBindings;

				if (KeyBindings)
				{
					for (INT i = 0; i < KeyBindings->QuickActorKeyBindings.Num(); ++i)
					{
						// Matching the key and modifiers...
						const FName KeyName = KeyBindings->QuickActorKeyBindings(i).Key;
						const FName ActorClassName = KeyBindings->QuickActorKeyBindings(i).ActorClassName;
						const UBOOL bIsKeyDown = KeyName != NAME_None && ViewportClient->Viewport->KeyState(KeyName);
						const UBOOL bAreModifiersDown = KeyBindings->QuickActorKeyBindings(i).bCtrlDown == Click.IsControlDown() &&
							KeyBindings->QuickActorKeyBindings(i).bAltDown == Click.IsAltDown() &&
							KeyBindings->QuickActorKeyBindings(i).bShiftDown == Click.IsShiftDown();
						const UBOOL bIsClassNameValid = ActorClassName != NAME_None;

						if ( bIsKeyDown && bAreModifiersDown && bIsClassNameValid )
						{
							// Determine the class and spawn the actor
							UClass* ActorClass = UObject::StaticLoadClass(AActor::StaticClass(), NULL, *ActorClassName.ToString(), NULL, LOAD_None, NULL);

							if ( ActorClass != NULL )
							{
								PrivateAddActor( ActorClass, FALSE );
								return TRUE;
							}
						}
					}
				}
			}
		}

		return FALSE;
	}

	static UBOOL ClickActor(FEditorLevelViewportClient* ViewportClient,AActor* Actor,const FViewportClick& Click,UBOOL bAllowSelectionChange)
	{
		// Find the point on the actor component which was clicked on.
		// Do an accurate trace to avoid legacy pull back by an arbitrary amount.
		// TRACE_Accurate is needed for texel selection to work.
		FCheckResult Hit(1);
		if (!GWorld->SingleLineCheck(Hit, NULL, Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX, Click.GetOrigin(), TRACE_Actors | TRACE_ComplexCollision | TRACE_Accurate))
		{	
			GEditor->ClickLocation = Hit.Location;
			GEditor->ClickPlane = FPlane(Hit.Location,Hit.Normal);
		}

		// Pivot snapping
		if( Click.GetKey() == KEY_MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, TRUE, FALSE, TRUE );

			return TRUE;
		}
		// Handle selection.
		else if( Click.GetKey() == KEY_RightMouseButton && !Click.IsControlDown() && !ViewportClient->Viewport->KeyState(KEY_LeftMouseButton) )
		{
			UBOOL bNeedViewportRefresh = FALSE;
			{
				const FScopedTransaction Transaction( TEXT("Clicking on Actors (context menu)") );

				GEditor->GetSelectedActors()->Modify();

				if( bAllowSelectionChange )
				{
					// If the actor the user clicked on was already selected, then we won't bother clearing the selection
					if( !Actor->IsSelected() )
					{
						GEditor->SelectNone( FALSE, TRUE );
						bNeedViewportRefresh = TRUE;
					}

					// Select the actor the user clicked on
					GEditor->SelectActor( Actor, TRUE, NULL, TRUE );
				}
			}

			if( bNeedViewportRefresh )
			{
				// Redraw the viewport so the user can see which object was right clicked on
				ViewportClient->Viewport->Draw();
				FlushRenderingCommands();
			}

			// Display the context menu for the selected actor(s)
			GEditor->ShowUnrealEdContextMenu();

			return TRUE;
		}
		else if( Click.GetEvent() == IE_DoubleClick && Click.GetKey() == KEY_LeftMouseButton && !Click.IsControlDown() )
		{
			{
				const FScopedTransaction Transaction( TEXT("Clicking on Actors (double-click)") );

				GEditor->GetSelectedActors()->Modify();

				if( bAllowSelectionChange )
				{
					// Clear the selection
					GEditor->SelectNone( FALSE, TRUE );

					// Select the actor the user clicked on
					GEditor->SelectActor( Actor, TRUE, NULL, TRUE );
				}
			}

			// Display the actor properties window
			GEditor->Exec( TEXT("EDCALLBACK SELECTEDPROPS") );

			return TRUE;
		}
		else if( Click.GetKey() != KEY_RightMouseButton )
		{
			if ( SpawnQuickActor(ViewportClient, Click) )
			{
				// Quick actor spawned, ignore other key cases
			}
			else if ( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_T) )
			{
				if( Click.IsControlDown() )
				{
					ViewportClient->TeleportViewportCamera( GEditor->ClickLocation, Actor );
				}
				#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
				else
				{
					SetDebugLightmapSample(&Actor->Components, NULL, 0, GEditor->ClickLocation);
				}
				#endif
			}
			else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_L) )
			{
				// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
				if(Click.IsControlDown())
				{
					PickColorAndAddLight(ViewportClient->Viewport, Click);
				}
				else
				{
					// Create a point light.
					PrivateAddActor( APointLight::StaticClass(), FALSE );
				}

				return TRUE;
			}
			else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_D) )
			{
				// Create a decal.
				PrivateAddActor( ADecalActor::StaticClass(), TRUE );

				return TRUE;
			}
			else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_S) )
			{
				// Create a static mesh.
				PrivateAddActor( AStaticMeshActor::StaticClass(), Click.IsAltDown() );

				return TRUE;
			}
			else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_A) )
			{
				// Create an actor of the selected class.
				UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
				if( SelectedClass )
				{
					PrivateAddActor( SelectedClass, FALSE );
				}

				return TRUE;
			}
			else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_Period) )
			{
				if(Click.IsControlDown())
				{
					// create a pylon
					UClass* PylonClass = GEditor->GetClassFromPairMap( FString(TEXT("Pylon")) );
					PrivateAddActor( PylonClass, FALSE );
				}
				else
				{
					// Create a PathTargetPoint.
					PrivateAddActor( APathTargetPoint::StaticClass(), FALSE );
				}
				return TRUE;
			}
			else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_Comma) )
			{
				// Create a coverlink.
				ACoverLink *Link = Cast<ACoverLink>(PrivateAddActor( ACoverLink::StaticClass(), FALSE ));
				if (Click.IsControlDown() && Link != NULL)
				{
					// have the link automatically setup
					Link->EditorAutoSetup(Click.GetDirection());
				}

				return TRUE;
			}
			else
			{
				if( bAllowSelectionChange )
				{
					const FScopedTransaction Transaction( TEXT("Clicking on Actors") );
					GEditor->GetSelectedActors()->Modify();

					if( Click.IsControlDown() )
					{
						GEditor->SelectActor( Actor, !Actor->IsSelected(), NULL, TRUE );
					}
					else
					{
						// check to see how many actors need deselecting first - and warn as appropriate
						INT NumSelectedActors = GEditor->GetSelectedActorCount();
						if( NumSelectedActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
						{
							FString ConfirmText = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "Warning_ManyActorsToSelectOne" ), NumSelectedActors ) );
							WxSuppressableWarningDialog ManyActorsWarning( ConfirmText, LocalizeUnrealEd( "Warning_ManyActors" ), "Warning_ManyActors", TRUE );
							if( ManyActorsWarning.ShowModal() == wxID_CANCEL )
							{
								return FALSE;
							}
						}

						GEditor->SelectNone( FALSE, TRUE, FALSE );
						GEditor->SelectActor( Actor, TRUE, NULL, TRUE );
					}
				}

				return FALSE;
			}
		}

		return FALSE;
	}

	static void ClickBrushVertex(FEditorLevelViewportClient* ViewportClient,ABrush* InBrush,FVector* InVertex,const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == KEY_MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, TRUE, FALSE, TRUE );
		}
		else if( Click.GetKey() == KEY_RightMouseButton )
		{
			const FScopedTransaction Transaction( TEXT("Clicking on Brush Vertex") );
			GEditor->SetPivot( InBrush->LocalToWorld().TransformFVector(*InVertex), FALSE, FALSE );

			const FVector World = InBrush->LocalToWorld().TransformFVector(*InVertex);
			FVector Snapped = World;
			GEditor->Constraints.Snap( Snapped, FVector(GEditor->Constraints.GetGridSize()) );
			const FVector Delta = Snapped - World;
			GEditor->SetPivot( Snapped, FALSE, FALSE );

			if( GEditorModeTools().IsModeActive( EM_Default ) || GEditorModeTools().IsModeActive( EM_CoverEdit) )
			{		
				// All selected actors need to move by the delta.
				for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
				{
					AActor* Actor = static_cast<AActor*>( *It );
					checkSlow( Actor->IsA(AActor::StaticClass()) );

					Actor->Modify();
					Actor->Location += Delta;
					Actor->ForceUpdateComponents();
				}
			}

			ViewportClient->Invalidate( TRUE, TRUE );

			// Update Bsp
			GEditor->RebuildAlteredBSP();
		}
	}

	static void ClickStaticMeshVertex(FEditorLevelViewportClient* ViewportClient,AActor* InActor,FVector& InVertex,const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == KEY_MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, TRUE, FALSE, TRUE );
		}
		else if( Click.GetKey() == KEY_RightMouseButton )
		{
			const FScopedTransaction Transaction( TEXT("Clicking on Static Mesh Vertex") );

			FVector Snapped = InVertex;
			GEditor->Constraints.Snap( Snapped, FVector(GEditor->Constraints.GetGridSize()) );
			const FVector Delta = Snapped - InVertex;
			GEditor->SetPivot( Snapped, FALSE, TRUE );

			// All selected actors need to move by the delta.
			for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				Actor->Modify();
				Actor->Location += Delta;
				Actor->ForceUpdateComponents();
			}

			ViewportClient->Invalidate( TRUE, TRUE );
		}
	}

	static UBOOL ClickGeomPoly(FEditorLevelViewportClient* ViewportClient, HGeomPolyProxy* InHitProxy, const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == KEY_MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, TRUE, FALSE, TRUE );

			return TRUE;
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GeomObject->GetActualBrush(), FALSE, NULL, TRUE );
		}
		else if( Click.GetKey() == KEY_LeftMouseButton )
		{
			// This should only happen in geometry mode
			FEdMode* Mode = GEditorModeTools().GetActiveMode( EM_Geometry );
			if( Mode )
			{
				Mode->GetCurrentTool()->StartTrans();

				if( !Click.IsControlDown() )
				{
					Mode->GetCurrentTool()->SelectNone();
				}

				FGeomPoly& gp = InHitProxy->GeomObject->PolyPool( InHitProxy->PolyIndex );
				gp.Select( Click.IsControlDown() ? !gp.IsSelected() : TRUE );

				Mode->GetCurrentTool()->EndTrans();
				ViewportClient->Invalidate( TRUE, FALSE );
			}
		}

		return FALSE;
	}

	/**
	 * Utility method used by ClickGeomEdge and ClickGeomVertex.  Returns TRUE if the projections of
	 * the vectors onto the specified viewport plane are equal within the given tolerance.
	 */
	static UBOOL OrthoEqual(ELevelViewportType ViewportType, const FVector& Vec0, const FVector& Vec1, FLOAT Tolerance=0.1f)
	{
		UBOOL bResult = FALSE;
		switch( ViewportType )
		{
			case LVT_OrthoXY:	bResult = Abs(Vec0.X - Vec1.X) < Tolerance && Abs(Vec0.Y - Vec1.Y) < Tolerance;	break;
			case LVT_OrthoXZ:	bResult = Abs(Vec0.X - Vec1.X) < Tolerance && Abs(Vec0.Z - Vec1.Z) < Tolerance;	break;
			case LVT_OrthoYZ:	bResult = Abs(Vec0.Y - Vec1.Y) < Tolerance && Abs(Vec0.Z - Vec1.Z) < Tolerance;	break;
			default:			check( 0 );		break;
		}
		return bResult;
	}

	static UBOOL ClickGeomEdge(FEditorLevelViewportClient* ViewportClient, HGeomEdgeProxy* InHitProxy, const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == KEY_MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, TRUE, FALSE, TRUE );

			return TRUE;
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GeomObject->GetActualBrush(), FALSE, NULL, TRUE );
		}
		else if( Click.GetKey() == KEY_LeftMouseButton )
		{
			FEdMode* Mode = GEditorModeTools().GetActiveMode( EM_Geometry );
			
			if( Mode )
			{ 
				Mode->GetCurrentTool()->StartTrans();

				const UBOOL bControlDown = Click.IsControlDown();
				if( !bControlDown )
				{
					Mode->GetCurrentTool()->SelectNone();
				}

				FGeomEdge& HitEdge = InHitProxy->GeomObject->EdgePool( InHitProxy->EdgeIndex );
				HitEdge.Select( bControlDown ? !HitEdge.IsSelected() : TRUE );

				if( ViewportClient->IsOrtho() )
				{
					// Select all edges in the brush that match the projected mid point of the original edge.
					for( INT EdgeIndex = 0 ; EdgeIndex < InHitProxy->GeomObject->EdgePool.Num() ; ++EdgeIndex )
					{
						if ( EdgeIndex != InHitProxy->EdgeIndex )
						{
							FGeomEdge& GeomEdge = InHitProxy->GeomObject->EdgePool( EdgeIndex );
							if ( OrthoEqual( ViewportClient->ViewportType, GeomEdge.GetMid(), HitEdge.GetMid() ) )
							{
								GeomEdge.Select( bControlDown ? !GeomEdge.IsSelected() : TRUE );
							}
						}
					}
				}

				Mode->GetCurrentTool()->EndTrans();
				ViewportClient->Invalidate( TRUE, TRUE );
				return TRUE;
			}

			return FALSE;

			
		}

		return FALSE;
	}

	static UBOOL ClickGeomVertex(FEditorLevelViewportClient* ViewportClient,HGeomVertexProxy* InHitProxy,const FViewportClick& Click)
	{
		if( !GEditorModeTools().IsModeActive( EM_Geometry ) )
		{
			return FALSE;
		}

		FEdModeGeometry* mode = static_cast<FEdModeGeometry*>( GEditorModeTools().GetActiveMode(EM_Geometry) );

		// Note: The expected behavior is that right clicking on a vertex will snap the vertex that was
		// right-clicked on to the nearest grid point, then move all SELECTED verts by the appropriate
		// delta.  So, we need to handle the right mouse button click BEFORE we change the selection set below.

		if( Click.GetKey() == KEY_RightMouseButton )
		{
			FModeTool_GeometryModify* Tool = static_cast<FModeTool_GeometryModify*>( mode->GetCurrentTool() );
			Tool->GetCurrentModifier()->StartTrans();

			// Compute out far to move to get back on the grid.
			const FVector WorldLoc = InHitProxy->GeomObject->GetActualBrush()->LocalToWorld().TransformFVector( InHitProxy->GeomObject->VertexPool( InHitProxy->VertexIndex ) );

			FVector SnappedLoc = WorldLoc;
			GEditor->Constraints.Snap( SnappedLoc, FVector(GEditor->Constraints.GetGridSize()) );

			const FVector Delta = SnappedLoc - WorldLoc;
			GEditor->SetPivot( SnappedLoc, FALSE, FALSE );

			for( INT VertexIndex = 0 ; VertexIndex < InHitProxy->GeomObject->VertexPool.Num() ; ++VertexIndex )
			{
				FGeomVertex& GeomVertex = InHitProxy->GeomObject->VertexPool(VertexIndex);
				if( GeomVertex.IsSelected() )
				{
					GeomVertex += Delta;
				}
			}

			Tool->GetCurrentModifier()->EndTrans();
			InHitProxy->GeomObject->SendToSource();
			ViewportClient->Invalidate( TRUE, TRUE );

			// HACK: The Bsp update has to occur after SendToSource() updates the vert pool, putting it outside
			// of the mode tool's transaction, therefore, the Bsp update requires a transaction of its own
			{
				FScopedTransaction Transaction(TEXT("GEOM MODE VERTEX SNAP"));

				// Update Bsp
				GEditor->RebuildAlteredBSP();
			}
		}

		if( Click.GetKey() == KEY_MiddleMouseButton && Click.IsAltDown() )
		{
			// Pivot snapping

			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, TRUE, FALSE, TRUE );

			return TRUE;
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GeomObject->GetActualBrush(), FALSE, NULL, TRUE );
		}
		else if( Click.GetKey() == KEY_LeftMouseButton )
		{
			mode->GetCurrentTool()->StartTrans();

			// Disable Ctrl+clicking for selection if selecting with RMB.
			const UBOOL bControlDown = Click.IsControlDown();
			if( !bControlDown )
			{
				mode->GetCurrentTool()->SelectNone();
			}

			FGeomVertex& HitVertex = InHitProxy->GeomObject->VertexPool( InHitProxy->VertexIndex );
			UBOOL bSelect = bControlDown ? !HitVertex.IsSelected() : TRUE;

			HitVertex.Select( bSelect );

			if( ViewportClient->IsOrtho() )
			{
				// Select all vertices that project to the same location.
				for( INT VertexIndex = 0 ; VertexIndex < InHitProxy->GeomObject->VertexPool.Num() ; ++VertexIndex )
				{
					if ( VertexIndex != InHitProxy->VertexIndex )
					{
						FGeomVertex& GeomVertex = InHitProxy->GeomObject->VertexPool(VertexIndex);
						if ( OrthoEqual( ViewportClient->ViewportType, GeomVertex, HitVertex ) )
						{
							GeomVertex.Select( bSelect );
						}
					}
				}
			}

			mode->GetCurrentTool()->EndTrans();

			ViewportClient->Invalidate( TRUE, TRUE );

			return TRUE;
		}

		return FALSE;
	}


	static FBspSurf GSaveSurf;

	static void ClickSurface(FEditorLevelViewportClient* ViewportClient,UModel* Model,INT iSurf,const FViewportClick& Click)
	{
		// Gizmos can cause BSP surfs to become selected without this check
		if(Click.GetKey() == KEY_RightMouseButton && Click.IsControlDown())
		{
			return;
		}

		// Remember hit location for actor-adding.
		FBspSurf& Surf			= Model->Surfs(iSurf);
		const FPlane Plane		= Surf.Plane;
		GEditor->ClickLocation	= FLinePlaneIntersection( Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection(), Plane );
		GEditor->ClickPlane		= Plane;

		// Pivot snapping
		if( Click.GetKey() == KEY_MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, TRUE, FALSE, TRUE );
		}
		else if ( SpawnQuickActor(ViewportClient, Click) )
		{
			// Quick actor spawned, ignore other key cases
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && Click.IsShiftDown() && Click.IsControlDown() )
		{
			
			if( !GEditorModeTools().GetClickBSPSelectsBrush() )
			{
				// Add to the actor selection set the brush actor that belongs to this BSP surface.
				// Check Surf.Actor, as it can be NULL after deleting brushes and before rebuilding BSP.
				if( Surf.Actor )
				{
					const FScopedTransaction Transaction( TEXT("Select Brush from Surface") );

					// If the builder brush is selected, first deselect it.
					USelection* SelectedActors = GEditor->GetSelectedActors();
					for ( USelection::TObjectIterator It( SelectedActors->ObjectItor() ) ; It ; ++It )
					{
						ABrush* Brush = Cast<ABrush>( *It );
						if ( Brush && Brush->IsCurrentBuilderBrush() )
						{
							GEditor->SelectActor( Brush, FALSE, NULL, FALSE );
							break;
						}
					}

					GEditor->SelectActor( Surf.Actor, TRUE, NULL, TRUE );
				}
			}
			else
			{
				// Select or deselect surfaces.
				{
					const FScopedTransaction Transaction( TEXT("Select Surfaces") );
					Model->ModifySurf( iSurf, FALSE );
					Surf.PolyFlags ^= PF_Selected;
				}
				GEditor->NoteSelectionChange();
			}
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && Click.IsShiftDown() )
		{
			GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );

			// Apply texture to all selected.
			const FScopedTransaction Transaction( TEXT("Apply Material to Selected Surfaces") );

			UMaterialInterface* SelectedMaterialInstance = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
			for( INT i=0; i<Model->Surfs.Num(); i++ )
			{
				if( Model->Surfs(i).PolyFlags & PF_Selected )
				{
					Model->ModifySurf( i, 1 );
					Model->Surfs(i).Material = SelectedMaterialInstance;
					GEditor->polyUpdateMaster( Model, i, 0 );
				}
			}
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_A) )
		{
			// Create an actor of the selected class.
			UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
			if( SelectedClass )
			{
				PrivateAddActor( SelectedClass, FALSE );
			}
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_L) )
		{
			// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
			if(Click.IsControlDown())
			{
				PickColorAndAddLight(ViewportClient->Viewport, Click);
			}
			else
			{
				// Create a point light.
				PrivateAddActor( APointLight::StaticClass(), FALSE );
			}
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_T) )
		{
			if( Click.IsControlDown() )
			{
				ViewportClient->TeleportViewportCamera(GEditor->ClickLocation, Surf.Actor);
			}
			#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
			else
			{
				SetDebugLightmapSample(NULL, Model, iSurf, GEditor->ClickLocation);
			}
			#endif
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_D) )
		{
			// Create a decal.
			PrivateAddActor( ADecalActor::StaticClass(), TRUE );
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_S) )
		{
			// Create a static mesh.
			PrivateAddActor( AStaticMeshActor::StaticClass(), Click.IsAltDown() );
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_Period) )
		{
			if(Click.IsControlDown())
			{
				// create a pylon
				UClass* PylonClass = GEditor->GetClassFromPairMap( FString(TEXT("Pylon")) );
				PrivateAddActor( PylonClass, FALSE );
			}
			else
			{
				// Create a PathTargetPoint.
				PrivateAddActor( APathTargetPoint::StaticClass(), FALSE );
			}
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_Semicolon) )
		{
			PrivateAddActor( ATargetPoint::StaticClass(), FALSE );
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_Comma) )
		{
			// Create a coverlink.
			ACoverLink *Link = Cast<ACoverLink>(PrivateAddActor( ACoverLink::StaticClass(), FALSE ));
			if (Click.IsControlDown() && Link != NULL)
			{
				// have the link automatically setup
				FVector HitL(GEditor->ClickLocation), HitN(Plane);
				Link->EditorAutoSetup(Click.GetDirection(),&HitL,&HitN);
			}
		}
		else if( Click.IsAltDown() && Click.GetKey() == KEY_RightMouseButton )
		{
			// Grab the texture.
			GEditor->GetSelectedObjects()->SelectNone(UMaterialInterface::StaticClass());
			//Added so that in the below if (Alt-left mouse down) the CALLBACK_LoadSelectedAssetsIfNeeded doesn't change selection back to that of the Content Browser
			GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_EmptySelection ) );
			if(Surf.Material)
			{
				GEditor->GetSelectedObjects()->Select(Surf.Material);
			}
			GSaveSurf = Surf;
		}
		else if( Click.IsAltDown() && Click.GetKey() == KEY_LeftMouseButton)
		{
			GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

			// Apply texture to the one polygon clicked on.
			const FScopedTransaction Transaction( TEXT("Apply Material to Surface") );
			Model->ModifySurf( iSurf, TRUE );
			Surf.Material = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
			if( Click.IsControlDown() )
			{
				Surf.vTextureU	= GSaveSurf.vTextureU;
				Surf.vTextureV	= GSaveSurf.vTextureV;
				if( Surf.vNormal == GSaveSurf.vNormal )
				{
					GLog->Logf( TEXT("WARNING: the texture coordinates were not parallel to the surface.") );
				}
				Surf.PolyFlags	= GSaveSurf.PolyFlags;
				GEditor->polyUpdateMaster( Model, iSurf, 1 );
			}
			else
			{
				GEditor->polyUpdateMaster( Model, iSurf, 0 );
			}
		}
		else if( Click.GetKey() == KEY_RightMouseButton && !Click.IsControlDown() )
		{
			// Select surface and display context menu
			check( Model );

			UBOOL bNeedViewportRefresh = FALSE;
			{
				const FScopedTransaction Transaction( TEXT("Select Surfaces") );

				// We only need to unselect surfaces if the surface the user clicked on was not already selected
				if( !( Surf.PolyFlags & PF_Selected ) )
				{
					GEditor->SelectNone( FALSE, TRUE );
					bNeedViewportRefresh = TRUE;
				}

				// Select the surface the user clicked on
				Model->ModifySurf( iSurf, FALSE );
				Surf.PolyFlags |= PF_Selected;

				GEditor->NoteSelectionChange();
			}

			if( bNeedViewportRefresh )
			{
				// Redraw the viewport so the user can see which object was right clicked on
				ViewportClient->Viewport->Draw();
				FlushRenderingCommands();
			}

			// Display the context menu for the selected surface(s)
			GEditor->ShowUnrealEdContextSurfaceMenu();	
		}
		else if( Click.GetEvent() == IE_DoubleClick && Click.GetKey() == KEY_LeftMouseButton && !Click.IsControlDown() )
		{
			{
				const FScopedTransaction Transaction( TEXT("Select Surface") );

				// Clear the selection
				GEditor->SelectNone( FALSE, TRUE );
				
				// Select the surface
				const DWORD SelectMask = Surf.PolyFlags & PF_Selected;
				Model->ModifySurf( iSurf, FALSE );
				Surf.PolyFlags = ( Surf.PolyFlags & ~PF_Selected ) | ( SelectMask ^ PF_Selected );				
			}
			GEditor->NoteSelectionChange();

			// Display the surface properties window
			GEditor->Exec( TEXT("EDCALLBACK SURFPROPS") );
		}
		else
		{	
			UBOOL bDeselectAlreadyHandled = FALSE;
			if( GEditorModeTools().GetClickBSPSelectsBrush() )
			{
				// Add to the actor selection set the brush actor that belongs to this BSP surface.
				// Check Surf.Actor, as it can be NULL after deleting brushes and before rebuilding BSP.
				if( Surf.Actor )
				{
					const FScopedTransaction Transaction( TEXT("Select Brush from Surface") );
					if( !Click.IsControlDown() )
					{
						GEditor->SelectNone( FALSE, TRUE );
						bDeselectAlreadyHandled = TRUE;
					}
					// If the builder brush is selected, first deselect it.
					USelection* SelectedActors = GEditor->GetSelectedActors();
					for ( USelection::TObjectIterator It( SelectedActors->ObjectItor() ) ; It ; ++It )
					{
						ABrush* Brush = Cast<ABrush>( *It );
						if ( Brush && Brush->IsCurrentBuilderBrush() )
						{
							GEditor->SelectActor( Brush, FALSE, NULL, FALSE );
							break;
						}
					}

					GEditor->SelectActor( Surf.Actor, TRUE, NULL, TRUE );
				}
			}
			// Select or deselect surfaces.
			{
				const FScopedTransaction Transaction( TEXT("Select Surfaces") );
				if( !Click.IsControlDown() && !bDeselectAlreadyHandled)
				{
					GEditor->SelectNone( FALSE, TRUE );
				}
				Model->ModifySurf( iSurf, FALSE );
				Surf.PolyFlags ^= PF_Selected;
			}
			GEditor->NoteSelectionChange();
		}
	}

	static void ClickBackdrop(FEditorLevelViewportClient* ViewportClient,const FViewportClick& Click)
	{
		GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
		GEditor->ClickPlane    = FPlane(0,0,0,0);

		// Pivot snapping
		if( Click.GetKey() == KEY_MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, TRUE, FALSE, TRUE );
		}
		else if ( SpawnQuickActor(ViewportClient, Click) )
		{
			// Quick actor spawned, ignore other key cases
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_A) )
		{
			// Create an actor of the selected class.
			UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
			if( SelectedClass )
			{
				PrivateAddActor( SelectedClass, FALSE );
			}
		}
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_T) )
		{
			SetDebugLightmapSample(NULL, NULL, 0, GEditor->ClickLocation);
		}
#endif
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_L) )
		{
			// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
			if(Click.IsControlDown())
			{
				PickColorAndAddLight(ViewportClient->Viewport, Click);
			}
			else
			{
				// Create a point light.
				PrivateAddActor( APointLight::StaticClass(), FALSE );
			}
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_D) )
		{
			// Create a decal.
			PrivateAddActor( ADecalActor::StaticClass(), FALSE );
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_S) )
		{
			// Create a static mesh.
			PrivateAddActor( AStaticMeshActor::StaticClass(), FALSE );
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_Period) )
		{
			// Create a pathnode.
			if(Click.IsControlDown())
			{
				// create a pylon
				UClass* PylonClass = GEditor->GetClassFromPairMap( FString(TEXT("Pylon")) );
				PrivateAddActor( PylonClass, FALSE );
			}
			else
			{
				// Create a PathTargetPoint.
				PrivateAddActor( APathTargetPoint::StaticClass(), FALSE );
			}
		}
		else if( Click.GetKey() == KEY_LeftMouseButton && ViewportClient->Viewport->KeyState(KEY_Comma) )
		{
			// Create a coverlink.
			ACoverLink *Link = Cast<ACoverLink>(PrivateAddActor( ACoverLink::StaticClass(), FALSE ));
			if (Click.IsControlDown() && Link != NULL)
			{
				// have the link automatically setup
				Link->EditorAutoSetup(Click.GetDirection());
			}
		}
		else if( Click.GetKey() == KEY_RightMouseButton && !Click.IsControlDown() && !ViewportClient->Viewport->KeyState(KEY_LeftMouseButton) )
		{
			// NOTE: We intentionally do not deselect selected actors here even though the user right
			//		clicked on an empty background.  This is because LDs often use wireframe modes to
			//		interact with brushes and such, and it's easier to summon the context menu for
			//		these actors when right clicking *anywhere* will not deselect things.

			// Redraw the viewport so the user can see which object was right clicked on
			ViewportClient->Viewport->Draw();
			FlushRenderingCommands();

			GEditor->ShowUnrealEdContextMenu();
		}
		else if( Click.GetKey() == KEY_LeftMouseButton )
		{
			if( !Click.IsControlDown() )
			{
				const FScopedTransaction Transaction( TEXT("Clicking Background") );
				GEditor->SelectNone( TRUE, TRUE );
			}
		}
	}
}

//
//	FEditorLevelViewportClient::FEditorLevelViewportClient
//
/**
 * Constructor
 *
 * @param	ViewportInputClass	the input class to use for creating this viewport's input object
 */

FEditorLevelViewportClient::FEditorLevelViewportClient( UClass* ViewportInputClass/*=NULL*/ ):
	Viewport(NULL),	
	ViewLocation( 0.0f, 0.0f, 0.0f ),
	LastViewLocation( 0.0f, 0.0f, 0.0f ),
	ViewRotation( 0.0f, 0.0f, 0.0f ),
	ViewFOV(GEditor->FOVAngle),
	ViewportType(LVT_Perspective),

	OrthoZoom(DEFAULT_ORTHOZOOM),
	
	ViewState(AllocateViewState()),

	ShowFlags(SHOW_ViewMode_Lit|(SHOW_DefaultEditor&~SHOW_ViewMode_Mask)),
	CachedShowFlags(0),
	bForcedUnlitShowFlags(FALSE),
	LastShowFlags(SHOW_DefaultEditor),
	bInGameViewMode(FALSE),
	bMoveUnlit(FALSE),
	bLevelStreamingVolumePrevis(FALSE),
	bPostProcessVolumePrevis(TRUE),
	bLockSelectedToCamera(FALSE),
	bViewportLocked(FALSE),
	bVariableFarPlane(FALSE),

	bAllowMayaCam(FALSE),
	bUsingMayaCam(FALSE),
	MayaRotation(0,0,0),
	MayaLookAt(0,0,0),
	MayaZoom(0,0,0),
	MayaActor(NULL),

	bDrawAxes(TRUE),
	bDrawVertices(FALSE),
	bDuplicateActorsOnNextDrag( FALSE ),
	bDuplicateActorsInProgress( FALSE ),
	bIsTracking( FALSE ),
	bIsTrackingBrushModification( FALSE ),
	bDraggingByHandle( FALSE ),
	bDisableInput( FALSE ),
    
	bConstrainAspectRatio(FALSE),
	AspectRatio(1.777777f),

	NearPlane(GNearClippingPlane),

	bAllowAmbientOcclusion(FALSE),
	OverridePostProcessSettingsAlpha(0.f),
	OverrideProcessSettings(0),

	FramesSinceLastDraw(0),
	bNeedsLinkedRedraw(FALSE),

	bEditorFrameClient(FALSE),
	
	RenderingOverrides(FRenderingPerformanceOverrides(E_ForceInit)),
	bEnableFading(FALSE),
	FadeAmount(0.f),
	FadeColor( FColor(0,0,0) ),

	bEnableColorScaling(FALSE),
	ColorScale( FVector(1,1,1) ),

	bOverrideDiffuseAndSpecular(FALSE),
	bShowReflectionsOnly(FALSE),

	bAudioRealtimeOverride(FALSE),
	bWantAudioRealtime(FALSE),

	bSetListenerPosition(FALSE),

	NumPendingRedraws(0),

	CameraSpeed(MOVEMENTSPEED_SLOW),

	bIsRealtime(FALSE),
	bStoredRealtime(FALSE),
	bShowFPS(FALSE),
	bShowStats(FALSE),
	bUseSquintMode(FALSE),

	bIsFloatingViewport(FALSE),
	bAllowMatineePreview(FALSE),
	bAllowAlignViewport(TRUE),

	CameraController( NULL ),
	CameraUserImpulseData( NULL ),
	FlightCameraSpeedScale( 1.0f ),
	RealTimeFlightCameraSpeedScaleChanged( -10000.0 ),
	RecordingInterpEd(NULL),
	bAllowPlayInViewport(TRUE),

	MouseOverSocketOwner(NULL),
	MouseOverSocketIndex(INDEX_NONE),

	bWasControlledByOtherViewport(FALSE),
	TimeForForceRedraw( 0.0 )
{
	if ( ViewportInputClass == NULL )
	{
		ViewportInputClass = UEditorViewportInput::StaticClass();
	}
	else
	{
		checkSlow(ViewportInputClass->IsChildOf(UEditorViewportInput::StaticClass()));

		// ensure that this class is fully loaded
		LoadClass<UEditorViewportInput>(ViewportInputClass->GetOuter(), *ViewportInputClass->GetName(), NULL, LOAD_None, NULL);
	}

	
	// Register for editor cleanse events so we can release references to hovered actors
	GCallbackEvent->Register( CALLBACK_CleanseEditor, this );


	CameraController = new FEditorCameraController();
	CameraUserImpulseData = new FCameraControllerUserImpulseData();

	Input = ConstructObject<UEditorViewportInput>(ViewportInputClass);
	Input->Editor = GEditor;
	Widget = new FWidget;
	MouseDeltaTracker = new FMouseDeltaTracker;
	
	// add this client to list of views, and remember the index
	ViewIndex = GEditor->ViewportClients.AddItem(this);

	// make sure all actors know about this view for per-view layer vis
	FLayerUtils::UpdatePerViewVisibility(ViewIndex);

	// by default, this viewport should not draw the base attachment volume
	bDrawBaseInfo = FALSE;

	//used to know if the mouse has moved since the mouse was clicked
	bHasMouseMovedSinceClick = FALSE;

	// Get the number of volume classes so we can initialize our bit array
	TArray<UClass*> VolumeClasses;
	GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );
	VolumeActorVisibility.Init(TRUE, VolumeClasses.Num() );

	// Initialize all sprite categories to visible
	SpriteCategoryVisibility.Init( TRUE, GUnrealEd->SortedSpriteCategories.Num() );
}

//
//	FEditorLevelViewportClient::~FEditorLevelViewportClient
//

FEditorLevelViewportClient::~FEditorLevelViewportClient()
{
	ViewState->Destroy();
	ViewState = NULL;

	delete Widget;
	delete MouseDeltaTracker;

	delete CameraController;
	CameraController = NULL;

	delete CameraUserImpulseData;
	CameraUserImpulseData = NULL;

	// Unregister for all global callbacks to this object
	GCallbackEvent->UnregisterAll( this );

	if(Viewport)
	{
		appErrorf(*LocalizeUnrealEd("Error_ViewportNotNULL"));
	}

	// make sure all actors have this view removed from their visibility bits
	FLayerUtils::RemoveViewFromActorViewVisibility(ViewIndex);

	GEditor->ViewportClients.RemoveItem(this);
	//make to clean up the global "current" client when we delete the active one.
	if (GCurrentLevelEditingViewportClient == this)
	{
		GCurrentLevelEditingViewportClient = NULL;
	}

	// fix up the other viewport indices
	for (INT ViewportIndex = ViewIndex; ViewportIndex < GEditor->ViewportClients.Num(); ViewportIndex++)
	{
		GEditor->ViewportClients(ViewportIndex)->ViewIndex = ViewportIndex;
	}
}


/** FCallbackEventDevice: Called when a registered global event is fired */
void FEditorLevelViewportClient::Send( ECallbackEventType InType )
{
	if( InType == CALLBACK_CleanseEditor )
	{
		// Clear out our lists of hovered actors and models
		ClearHoverFromObjects();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FEditorLevelViewportClient::SetViewLocationForOrbiting(const FVector& Loc)
{
	FMatrix EpicMat = FTranslationMatrix(-ViewLocation);
	EpicMat = EpicMat * FInverseRotationMatrix(ViewRotation);
	FMatrix CamRotMat = EpicMat.Inverse();
	FVector CamDir = FVector(CamRotMat.M[0][0],CamRotMat.M[0][1],CamRotMat.M[0][2]);
	ViewLocation = Loc - 256 * CamDir;
}

void FEditorLevelViewportClient::SwitchStandardToMaya()
{
	FMatrix EpicMat = FTranslationMatrix(-ViewLocation);
	EpicMat = EpicMat * FInverseRotationMatrix(ViewRotation);

	FMatrix MayaMat = EpicMat * 
		FRotationMatrix( FRotator(0,16384,0) ) * FTranslationMatrix( FVector(0,-256,0) );

	MayaZoom = FVector(0,0,0);

	FMatrix CamRotMat = EpicMat.Inverse();
	FVector CamDir = FVector(CamRotMat.M[0][0],CamRotMat.M[0][1],CamRotMat.M[0][2]);
	MayaLookAt = ViewLocation + 256 * CamDir;

	FMatrix RotMat = FTranslationMatrix( -MayaLookAt ) * MayaMat;
	FMatrix RotMatInv = RotMat.Inverse();
	FRotator RollVec = RotMatInv.Rotator();
	FMatrix YawMat = RotMatInv * FInverseRotationMatrix( FRotator(0, 0, -RollVec.Roll));
	FMatrix YawMatInv = YawMat.Inverse();
	FRotator YawVec = YawMat.Rotator();
	FRotator rotYawInv = YawMatInv.Rotator();
	MayaRotation = FRotator(-RollVec.Roll,-YawVec.Yaw,0);

	bUsingMayaCam = TRUE;
}

void FEditorLevelViewportClient::SwitchMayaToStandard()
{
	FMatrix MayaMat =
		FTranslationMatrix( -MayaLookAt ) *
		FRotationMatrix( FRotator(0,MayaRotation.Yaw,0) ) * 
		FRotationMatrix( FRotator(0, 0, MayaRotation.Pitch)) *
		FTranslationMatrix( MayaZoom ) *
		FTranslationMatrix( FVector(0,256,0) ) *
		FInverseRotationMatrix( FRotator(0,16384,0) );
	MayaMat =  MayaMat.Inverse();

	ViewRotation = MayaMat.Rotator();
	ViewLocation = MayaMat.GetOrigin();

	bUsingMayaCam = FALSE;
}

void FEditorLevelViewportClient::SwitchMayaCam()
{
	if ( bAllowMayaCam )
	{
		if ( GEditor->bUseMayaCameraControls && !bUsingMayaCam )
		{
			SwitchStandardToMaya();
		}
		else if ( !GEditor->bUseMayaCameraControls && bUsingMayaCam )
		{
			SwitchMayaToStandard();
		}
	}
}

/**
 * Reset the camera position and rotation.  Used when creating a new level.
 */
void FEditorLevelViewportClient::ResetCamera()
{
	if(ViewportType == LVT_Perspective)
	{
		ViewLocation = EditorViewportDefs::DefaultPerspectiveViewLocation;
		ViewRotation = EditorViewportDefs::DefaultPerspectiveViewRotation;
	}
	else
	{
		ViewLocation.Set( 0.0f, 0.0f, 0.0f );
		ViewRotation.Pitch = ViewRotation.Roll = ViewRotation.Yaw = 0.0f;
	}
	ViewFOV = GEditor->FOVAngle;

	OrthoZoom = DEFAULT_ORTHOZOOM;

	// If interp mode is active, tell it about the camera movement.
	FEdMode* Mode = GEditorModeTools().GetActiveMode(EM_InterpEdit);
	if( Mode )
	{
		((FEdModeInterpEdit*)Mode)->CamMoveNotify(this);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Configures the specified FSceneView object with the view and projection matrices for this viewport. 

FSceneView* FEditorLevelViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily)
{
	UPostProcessChain* LevelViewPostProcess = NULL;

	FMatrix ViewMatrix = FTranslationMatrix(-ViewLocation);	

	if(ViewportType == LVT_OrthoXY)
	{
		ViewMatrix = ViewMatrix * FMatrix(
			FPlane(0,	1,	0,					0),
			FPlane(1,	0,	0,					0),
			FPlane(0,	0,	-1,					0),
			FPlane(0,	0,	-ViewLocation.Z,	1));
	}
	else if(ViewportType == LVT_OrthoXZ)
	{
		ViewMatrix = ViewMatrix * FMatrix(
			FPlane(1,	0,	0,					0),
			FPlane(0,	0,	-1,					0),
			FPlane(0,	1,	0,					0),
			FPlane(0,	0,	-ViewLocation.Y,	1));
	}
	else if(ViewportType == LVT_OrthoYZ)
	{
		ViewMatrix = ViewMatrix * FMatrix(
			FPlane(0,	0,	1,					0),
			FPlane(1,	0,	0,					0),
			FPlane(0,	1,	0,					0),
			FPlane(0,	0,	ViewLocation.X,		1));
	}

	FMatrix ProjectionMatrix;

	INT X = 0;
	INT Y = 0;
	INT ClipX = 0;
	INT ClipY = 0;
	UINT SizeX = Viewport->GetSizeX();
	UINT SizeY = Viewport->GetSizeY();

	// set all other matching viewports to my location, if the LOD locking is enabled,
	// unless another viewport already set me this frame (otherwise they fight)
	if (GEditor->bEnableLODLocking && !bWasControlledByOtherViewport)
	{
		for (INT ViewportIndex = 0; ViewportIndex < GEditor->ViewportClients.Num(); ViewportIndex++)
		{
			FEditorLevelViewportClient* ViewportClient = GEditor->ViewportClients(ViewportIndex);

			//only change camera for a viewport that is looking at the same scene
			if (GetScene() != ViewportClient->GetScene())
			{
				continue;
			}

			// go over all other level viewports
			if (ViewportClient && ViewportClient->Viewport && ViewportClient != this)
			{
				// force camera of same-typed viewports
				if (ViewportClient->ViewportType == ViewportType)
				{
					ViewportClient->ViewLocation = ViewLocation;
					ViewportClient->ViewRotation = ViewRotation;
					ViewportClient->OrthoZoom = OrthoZoom;

					// don't let this other viewport update itself in its own CalcSceneView
					ViewportClient->bWasControlledByOtherViewport = TRUE;
				}
				// when we are LOD locking, ortho views get their camera position from this view, so make sure it redraws
				else if (ViewportType == LVT_Perspective && ViewportClient->ViewportType != LVT_Perspective && ViewportClient->ViewportType != LVT_None)
				{
					// don't let this other viewport update itself in its own CalcSceneView
					ViewportClient->bWasControlledByOtherViewport = TRUE;
				}
			}

			// if the above code determined that this viewport has changed, delay the update unless
			// an update is already in the pipe
			if (ViewportClient->bWasControlledByOtherViewport && ViewportClient->TimeForForceRedraw == 0.0)
			{
				ViewportClient->TimeForForceRedraw = appSeconds() + 0.9 + appFrand() * 0.2;
			}
		}
	}

	bWasControlledByOtherViewport = FALSE;

	// no matter how we are drawn (forced or otherwise), reset our time here
	TimeForForceRedraw = 0.0;


	// for ortho views to steal perspective view origin
	FVector4 OverrideLODViewOrigin(0, 0, 0, 0);

	if (ViewportType == LVT_Perspective)
	{
		// only use post process chain for the perspective view
		LevelViewPostProcess = GEngine->GetWorldPostProcessChain();

		if (GEditor->bIsPushingView)
		{
			GEngine->Exec(*FString::Printf(TEXT("REMOTE PUSHVIEW %f %f %f %d %d %d"), ViewLocation.X, ViewLocation.Y, ViewLocation.Z, ViewRotation.Pitch, ViewRotation.Yaw, ViewRotation.Roll), *GNull);
		}

		SwitchMayaCam();

		if ( bAllowMayaCam && GEditor->bUseMayaCameraControls )
		{
			ViewMatrix =
				FTranslationMatrix( -MayaLookAt ) *
				FRotationMatrix( FRotator(0,MayaRotation.Yaw,0) ) * 
				FRotationMatrix( FRotator(0, 0, MayaRotation.Pitch)) *
				FTranslationMatrix( MayaZoom ) *
				FTranslationMatrix( FVector(0,256,0) ) *
				FInverseRotationMatrix( FRotator(0,16384,0) );

			FMatrix MayaMat =  ViewMatrix.Inverse();
			ViewRotation = MayaMat.Rotator();
			ViewLocation = MayaMat.GetOrigin();
		}
		else
		{
			ViewMatrix = ViewMatrix * FInverseRotationMatrix(ViewRotation);
		}

		ViewMatrix = ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));

		FLOAT MinZ = NearPlane;
		FLOAT MaxZ;
		if ( bVariableFarPlane && GEditor->FarClippingPlane > GNearClippingPlane )
		{
			MaxZ = GEditor->FarClippingPlane;
			// Disable ambient occlusion and motion blur as they don't work with a non-infinite far plane
			ViewFamily->ShowFlags &= ~(SHOW_SSAO | SHOW_MotionBlur);
		}
		else
		{
			//use maximum Z allowed.  See FPerspectiveMatrix constructor
			MaxZ = MinZ;
		}
		FLOAT MatrixFOV = ViewFOV * (FLOAT)PI / 360.0f;

		if( bConstrainAspectRatio )
		{
			ProjectionMatrix = FPerspectiveMatrix(
				MatrixFOV,
				MatrixFOV,
				1.0f,
				AspectRatio,
				MinZ,
				MaxZ
				);

			Viewport->CalculateViewExtents( AspectRatio, X, Y, SizeX, SizeY );
		}
		else
		{
			FLOAT XAxisMultiplier;
			FLOAT YAxisMultiplier;
			BYTE AspectRatioAxisConstraint = GEditor->GetUserSettings().AspectRatioAxisConstraint;

			if (((SizeX > SizeY) && (AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV)) || (AspectRatioAxisConstraint == AspectRatio_MaintainXFOV))
			{
				//if the viewport is wider than it is tall
				XAxisMultiplier = 1.0f;
				YAxisMultiplier = SizeX / (FLOAT)SizeY;
			}
			else
			{
				//if the viewport is taller than it is wide
				XAxisMultiplier = SizeY / (FLOAT)SizeX;
				YAxisMultiplier = 1.0f;
			}

			ProjectionMatrix = FPerspectiveMatrix (
				MatrixFOV,
				MatrixFOV,
				XAxisMultiplier,
				YAxisMultiplier,
				MinZ,
				MaxZ
				);
		}
	}
	else
	{
		//The divisor for the matrix needs to match the translation code.
		const FLOAT	Zoom = OrthoZoom / CAMERA_ZOOM_DIV;
		ProjectionMatrix = FOrthoMatrix(
			Zoom * SizeX / 2.0f,
			Zoom * SizeY / 2.0f,
			0.5f / HALF_WORLD_MAX,
			HALF_WORLD_MAX
			);

#if USE_MASSIVE_LOD 
		// ortho views need a perspective view's origin to calculate distance to camera
		if (GEditor->bEnableLODLocking)
		{
			for (INT ViewportIndex = 0; ViewportIndex < GEditor->ViewportClients.Num(); ViewportIndex++)
			{
				FEditorLevelViewportClient* ViewportClient = GEditor->ViewportClients(ViewportIndex);
				if (ViewportClient->ViewportType == LVT_Perspective)
				{
					OverrideLODViewOrigin = ViewportClient->ViewLocation;
					// we need to set the W to > 0 for the check in SceneRendering.cpp to think this is a valid origin
					OverrideLODViewOrigin.W = 1.0f;
				}
			}
		}
#endif
	}

	if(GetScene() == GWorld->Scene)
	{
		// Find the post-process settings for the view.
		GWorld->GetWorldInfo()->GetPostProcessSettings(
			ViewLocation,
			bPostProcessVolumePrevis,
			PostProcessSettings
			);
	}
	else
	{
		// If this isn't a level viewport, use the default post process settings.
		PostProcessSettings = GetDefault<AWorldInfo>()->DefaultPostProcessSettings;
	}

	// See if we want to specify the PP settings, or use those from the level.
	if(OverridePostProcessSettingsAlpha > 0.f)
	{
		OverrideProcessSettings.OverrideSettingsFor(PostProcessSettings, OverridePostProcessSettingsAlpha);
	}

	// If in squint mode, override post process settings and enable DOF.
	if ( bUseSquintMode )
	{
		PostProcessSettings.bEnableDOF = TRUE;
		PostProcessSettings.DOF_BlurKernelSize = GWorld->GetWorldInfo()->SquintModeKernelSize;
		PostProcessSettings.DOF_FalloffExponent = 1.0f;
		PostProcessSettings.DOF_FocusDistance = 0.0f;
		PostProcessSettings.DOF_FocusInnerRadius = 0.0f;
		PostProcessSettings.DOF_FocusType = FOCUS_Distance;
		PostProcessSettings.DOF_InterpolationDuration = 0.0f;
		PostProcessSettings.DOF_MaxFarBlurAmount = 1.0f;
		PostProcessSettings.DOF_MaxNearBlurAmount = 1.0f;
	}

	// Set up the rendering overrides.
	FRenderingPerformanceOverrides LocalRenderingOverrides = RenderingOverrides;	

	// If temporal AA is disabled in the WorldInfo, propagate that to the rendering overrides.
	if(!GWorld->GetWorldInfo()->GetAllowTemporalAA())
	{
		LocalRenderingOverrides.bAllowTemporalAA = FALSE;
	}

	const FTemporalAAParameters TemporalAAParameters = CalcTemporalAAParameters(
		(ViewFamily->ShowFlags & SHOW_TemporalAA) && LocalRenderingOverrides.bAllowTemporalAA,
		SizeX,
		SizeY,
		(ViewLocation - LastViewLocation).Size());

	FSceneView* View = new FSceneView(
		ViewFamily,
		ViewState,
		-1,
		NULL,
		NULL,
		NULL,
		LevelViewPostProcess,
		&PostProcessSettings,
		this,
		X,
		Y,
		ClipX,
		ClipY,
		SizeX,
		SizeY,
		ViewMatrix,
		ProjectionMatrix,
		GetBackgroundColor(),
		FLinearColor(0,0,0,0),
		FLinearColor::White,
		TSet<UPrimitiveComponent*>(),
		LocalRenderingOverrides,
		1.0f,
		FALSE,
		TemporalAAParameters,
		(QWORD)1 << ViewIndex, // send the bit for this view - each actor will check it's visibility bits against this
		OverrideLODViewOrigin, // any override origin for ortho views
		FALSE,
		SpriteCategoryVisibility // send the viewport's sprite category visibility
		);
	View->bForceLowestMassiveLOD = GWorld->GetWorldInfo()->IsInsideMassiveLODVolume(ViewLocation);
	ViewFamily->Views.AddItem(View);
	return View;
}

//
//	FEditorLevelViewportClient::ProcessClick
//

void FEditorLevelViewportClient::ProcessClick(FSceneView* View,HHitProxy* HitProxy,FName Key,EInputEvent Event,UINT HitX,UINT HitY)
{
	const UBOOL bAltDown = Viewport->KeyState( KEY_LeftAlt ) || Viewport->KeyState( KEY_RightAlt );

	const FViewportClick Click(View,this,Key,Event,HitX,HitY);
	if (!GEditorModeTools().HandleClick(HitProxy,Click))
	{
		if (HitProxy == NULL)
		{
			ClickBackdrop(this,Click);
		}
		else if (HitProxy->IsA(HActor::StaticGetType()))
		{
			AActor* Actor = ((HActor*)HitProxy)->Actor;
			if (Actor)
			{
				// clicked a skeletal mesh actor?
				ASkeletalMeshActor* SkelActor = Cast<ASkeletalMeshActor>(Actor);
				// ...with sockets?
				INT SocketCount = 0;
				if (SkelActor)
				{
					check(SkelActor->SkeletalMeshComponent);
					check(SkelActor->SkeletalMeshComponent->SkeletalMesh);
					SocketCount = SkelActor->SkeletalMeshComponent->SkeletalMesh->Sockets.Num();
				}
				// clicking an actor that is already selected?
				UBOOL bClickingSelected = FALSE;
				UBOOL bSelectedCannotSnap = FALSE;
				for ( FSelectionIterator SelIt(GEditor->GetSelectedActorIterator()) ; SelIt ; ++SelIt)
				{
					AActor* SelActor = static_cast<AActor*>(*SelIt);
					if (SelActor->IsStatic() || !SelActor->bMovable)
					{
						bSelectedCannotSnap = TRUE;
						break;
					}
					if (SelActor == Actor)
					{
						bClickingSelected = TRUE;
						break;
					}
				}

				if (GEditor->bEnableSocketSnapping &&
					0 < GEditor->GetSelectedActorCount() &&
					0 < SocketCount &&
					!bClickingSelected &&
					!bSelectedCannotSnap)
				{
					// Only treat this click as a socket snapping action if the mode is active, there are selected actor(s), that the clicked actor has sockets
					// and that the clicked actor is not already selected.
					FVector WorldOrigin, WorldDirection;
					View->DeprojectFVector2D(FVector2D(HitX, HitY), WorldOrigin, WorldDirection);
					ShowSocketSnappingDialog(SkelActor->SkeletalMeshComponent, WorldOrigin, WorldDirection);
				}
				else
				{
					ClickActor(this,((HActor*)HitProxy)->Actor,Click,TRUE);
				}
			}
		}
		else if (HitProxy->IsA(HBSPBrushVert::StaticGetType()))
		{
			ClickBrushVertex(this,((HBSPBrushVert*)HitProxy)->Brush,((HBSPBrushVert*)HitProxy)->Vertex,Click);
		}
		else if (HitProxy->IsA(HStaticMeshVert::StaticGetType()))
		{
			ClickStaticMeshVertex(this,((HStaticMeshVert*)HitProxy)->Actor,((HStaticMeshVert*)HitProxy)->Vertex,Click);
		}
		else if (HitProxy->IsA(HGeomPolyProxy::StaticGetType()))
		{
			FCheckResult CheckResult;
			UBOOL bHit = !GWorld->SingleLineCheck(CheckResult,
				((HGeomPolyProxy*)HitProxy)->GeomObject->ActualBrush,
				Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX,
				Click.GetOrigin(),
				TRACE_World,
				FVector(1.f));

			if( bHit )
			{	
				GEditor->ClickLocation = CheckResult.Location;
				GEditor->ClickPlane = FPlane(CheckResult.Location,CheckResult.Normal);
			}

			if( !ClickActor(this,((HGeomPolyProxy*)HitProxy)->GeomObject->ActualBrush,Click,FALSE) )
			{
				ClickGeomPoly(this,(HGeomPolyProxy*)HitProxy,Click);
			}

			Invalidate( TRUE, TRUE );
		}
		else if (HitProxy->IsA(HGeomEdgeProxy::StaticGetType()))
		{
			if( !ClickGeomEdge(this,(HGeomEdgeProxy*)HitProxy,Click) )
			{
				ClickActor(this,((HGeomEdgeProxy*)HitProxy)->GeomObject->ActualBrush,Click,TRUE);
			}
		}
		else if (HitProxy->IsA(HGeomVertexProxy::StaticGetType()))
		{
			ClickGeomVertex(this,(HGeomVertexProxy*)HitProxy,Click);
		}
		else if (HitProxy->IsA(HModel::StaticGetType()))
		{
			HModel* ModelHit = (HModel*)HitProxy;

			// Compute the viewport's current view family.
			FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
			FSceneView* View = CalcSceneView( &ViewFamily );

			UINT SurfaceIndex = INDEX_NONE;
			if(ModelHit->ResolveSurface(View,HitX,HitY,SurfaceIndex))
			{
				ClickSurface(this,ModelHit->GetModel(),SurfaceIndex,Click);
			}
		}
		else if (HitProxy->IsA(HLevelSocketProxy::StaticGetType()) && GEditor->bEnableSocketSnapping && 0 < GEditor->GetSelectedActorCount())
		{
			// User clicked a socket
			const INT HalfX = Viewport->GetSizeX()/2;
			const INT HalfY = Viewport->GetSizeY()/2;

			// Get the position of the clicked socket
			HLevelSocketProxy* SocketProxy = static_cast<HLevelSocketProxy*>( HitProxy );
			check( SocketProxy->SkelMeshComponent );
			USkeletalMeshSocket* Socket = SocketProxy->SkelMeshComponent->SkeletalMesh->Sockets(SocketProxy->SocketIndex);
			FMatrix SocketTM;
			Socket->GetSocketMatrix( SocketTM, SocketProxy->SkelMeshComponent );
			FVector SocketPoint = SocketTM.GetOrigin();

			// Turn the socket location into a screen space point
			FPlane SocketProj = View->Project(SocketPoint);
			if (SocketProj.W > 0.0f)
			{
				const INT XPos = HalfX + ( HalfX * SocketProj.X );
				const INT YPos = HalfY + ( HalfY * (SocketProj.Y * -1) );

				// Turn the screen space location back into a world space ray (required by the dialog below)
				FVector WorldOrigin, WorldDirection;
				View->DeprojectFVector2D(FVector2D(XPos, YPos), WorldOrigin, WorldDirection);

				ShowSocketSnappingDialog(SocketProxy->SkelMeshComponent, WorldOrigin, WorldDirection);
			}
		}
		else if( HitProxy->IsA( HWidgetAxis::StaticGetType() ) )
		{

			if(Click.GetKey() == KEY_RightMouseButton)
			{
				// If this is a right click, always handle as though we're clicking the backdrop
				ClickBackdrop(this, Click);
			}
			else
			{
				// The user clicked on an axis translation/rotation hit proxy.  However, we want
				// to find out what's underneath the axis widget.  To do this, we'll need to render
				// the viewport's hit proxies again, this time *without* the axis widgets!

				// OK, we need to be a bit evil right here.  Basically we want to hijack the ShowFlags
				// for the scene so we can re-render the hit proxies without any axis widgets.  We'll
				// store the original ShowFlags and modify them appropriately
				const EShowFlags OldShowFlags = ShowFlags;
				const EShowFlags OldSCFShowFlags = View->Family->ShowFlags;
				ShowFlags &= ~SHOW_ModeWidgets;
				FSceneViewFamily* SceneViewFamily = const_cast< FSceneViewFamily* >( View->Family );
				SceneViewFamily->ShowFlags &= ~SHOW_ModeWidgets;
				UBOOL bWasWidgetDragging = Widget->IsDragging();
				Widget->SetDragging(FALSE);

				// Invalidate the hit proxy map so it will be rendered out again when GetHitProxy
				// is called
				Viewport->InvalidateHitProxy();

				// This will actually re-render the viewport's hit proxies!
				HHitProxy* HitProxyWithoutAxisWidgets = Viewport->GetHitProxy( HitX, HitY );
				if( HitProxyWithoutAxisWidgets != NULL )
				{
					// We should never encounter a widget axis.  If we do, then something's wrong
					// with our ShowFlags (or the widget drawing code)
					check( !HitProxyWithoutAxisWidgets->IsA( HWidgetAxis::StaticGetType() ) );

					// Try this again, but without the widget this time!
					ProcessClick( View, HitProxyWithoutAxisWidgets, Key, Event, HitX, HitY );
				}

				// Undo the evil
				ShowFlags = OldShowFlags;
				SceneViewFamily->ShowFlags = OldSCFShowFlags;
				Widget->SetDragging(bWasWidgetDragging);

				// Invalidate the hit proxy map again so that it'll be refreshed with the original
				// scene contents if we need it again later.
				Viewport->InvalidateHitProxy();
			}
		}
		else if( HitProxy->IsA(HSplineProxy::StaticGetType()) )
		{
			HSplineProxy* SplineProxy = (HSplineProxy*)HitProxy;
			
			// If holding ALT, break connection
			if(bAltDown)
			{
				SplineBreakGivenProxy(SplineProxy);
				GEditor->RedrawLevelEditingViewports();			
			}
			// If not, do normal clicking
			else
			{
				if(SplineProxy->SplineComp)
				{
					ClickActor(this, SplineProxy->SplineComp->GetOwner(), Click, TRUE);				
				}
			}
		}
	}
}

// Frustum parameters for the perspective view.
static FLOAT GPerspFrustumAngle=90.f;
static FLOAT GPerspFrustumAspectRatio=1.77777f;
static FLOAT GPerspFrustumStartDist=GNearClippingPlane;
static FLOAT GPerspFrustumEndDist=HALF_WORLD_MAX;
static FMatrix GPerspViewMatrix;

//
//	FEditorLevelViewportClient::Tick
//

void FEditorLevelViewportClient::Tick(FLOAT DeltaTime)
{
	// Update show flags
	UpdateLightingShowFlags();

	// Update any real-time camera movement
	UpdateCameraMovement( DeltaTime );

	UpdateMouseDelta();

	GEditorModeTools().Tick(this,DeltaTime);

	// Update the preview mesh for the preview mesh mode. 
	GEditor->UpdatePreviewMesh();

	// Copy perspective views to the global if this viewport is a view parent or has streaming volume previs enabled
	if ( ViewState->IsViewParent() || bLevelStreamingVolumePrevis )
	{
		GPerspFrustumAngle=ViewFOV;
		GPerspFrustumAspectRatio=AspectRatio;
		GPerspFrustumStartDist=NearPlane;

		GPerspFrustumEndDist=( bVariableFarPlane && GEditor->FarClippingPlane > GNearClippingPlane )
			? GEditor->FarClippingPlane
			: HALF_WORLD_MAX;

		FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds(),IsRealtime());
		FSceneView* View = CalcSceneView(&ViewFamily);
		GPerspViewMatrix = View->ViewMatrix;
	}

	// refresh ourselves if told to from another view
	if (TimeForForceRedraw != 0.0 && appSeconds() > TimeForForceRedraw)
	{
		Invalidate();
	}
}

namespace ViewportDeadZoneConstants
{
	enum
	{
		NO_DEAD_ZONE,
		STANDARD_DEAD_ZONE
	};
};

FLOAT GetFilteredDelta(const FLOAT DefaultDelta, const UINT DeadZoneType, const FLOAT StandardDeadZoneSize)
{
	if (DeadZoneType == ViewportDeadZoneConstants::NO_DEAD_ZONE)
	{
		return DefaultDelta;
	}
	else
	{
		//can't be one or normalizing won't work
		check(IsWithin<FLOAT>(StandardDeadZoneSize, 0.0f, 1.0f));
		//standard dead zone
		FLOAT ClampedAbsValue = Clamp(Abs(DefaultDelta), StandardDeadZoneSize, 1.0f);
		FLOAT NormalizedClampedAbsValue = (ClampedAbsValue - StandardDeadZoneSize)/(1.0f-StandardDeadZoneSize);
		FLOAT ClampedSignedValue = (DefaultDelta >= 0.0f) ? NormalizedClampedAbsValue : -NormalizedClampedAbsValue;
		return ClampedSignedValue;
	}
}

/**Applies Joystick axis control to camera movement*/
void FEditorLevelViewportClient::UpdateCameraMovementFromJoystick(const UBOOL bRelativeMovement, FCameraControllerConfig& InConfig)
{
	for(TMap<INT,FCachedJoystickState*>::TConstIterator JoystickIt(JoystickStateMap);JoystickIt;++JoystickIt)
	{
		FCachedJoystickState* JoystickState = JoystickIt.Value();
		check(JoystickState);
		for(TMap<FName,FLOAT>::TConstIterator AxisIt(JoystickState->AxisDeltaValues);AxisIt;++AxisIt)
		{
			FName Key = AxisIt.Key();
			FLOAT UnfilteredDelta = AxisIt.Value();
			const FLOAT StandardDeadZone = CameraController->GetConfig().ImpulseDeadZoneAmount;

			if (bRelativeMovement)
			{
				//XBOX Controller
				if (Key == KEY_XboxTypeS_LeftX)
				{
					CameraUserImpulseData->MoveRightLeftImpulse += GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.TranslationMultiplier;
				}
				else if (Key == KEY_XboxTypeS_LeftY)
				{
					CameraUserImpulseData->MoveForwardBackwardImpulse += GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.TranslationMultiplier;
				}
				else if (Key == KEY_XboxTypeS_RightX)
				{
					FLOAT DeltaYawImpulse = GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.RotationMultiplier * (InConfig.bInvertX ? -1.0f : 1.0f);
					CameraUserImpulseData->RotateYawImpulse += DeltaYawImpulse;
					InConfig.bForceRotationalPhysics |= (DeltaYawImpulse != 0.0f);
				}
				else if (Key == KEY_XboxTypeS_RightY)
				{
					FLOAT DeltaPitchImpulse = GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.RotationMultiplier * (InConfig.bInvertY ? -1.0f : 1.0f);
					CameraUserImpulseData->RotatePitchImpulse -= DeltaPitchImpulse;
					InConfig.bForceRotationalPhysics |= (DeltaPitchImpulse != 0.0f);
				}
				else if (Key == KEY_XboxTypeS_LeftTriggerAxis)
				{
					CameraUserImpulseData->MoveUpDownImpulse -= GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.TranslationMultiplier;
				}
				else if (Key == KEY_XboxTypeS_RightTriggerAxis)
				{
					CameraUserImpulseData->MoveUpDownImpulse += GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.TranslationMultiplier;
				}
				//GAME CASTER
				else if (Key == KEY_GameCaster_LeftThumbX)
				{
					CameraUserImpulseData->MoveRightLeftImpulse += GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.TranslationMultiplier;
				}
				else if (Key == KEY_GameCaster_LeftThumbY)
				{
					CameraUserImpulseData->MoveForwardBackwardImpulse -= GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.TranslationMultiplier;
				}
				else if (Key == KEY_GameCaster_YawAxis)
				{
					CameraUserImpulseData->RotateYawImpulse += GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::NO_DEAD_ZONE, StandardDeadZone) * InConfig.RotationMultiplier;
				}
				else if (Key == KEY_GameCaster_RightThumb)
				{
					CameraUserImpulseData->MoveUpDownImpulse -= GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.TranslationMultiplier;
				}
				else if (Key == KEY_GameCaster_Zoom)
				{
					CameraUserImpulseData->ZoomOutInImpulse += GetFilteredDelta(UnfilteredDelta, ViewportDeadZoneConstants::STANDARD_DEAD_ZONE, StandardDeadZone) * InConfig.ZoomMultiplier;
				}
			}
			else
			{
				if (Key == KEY_GameCaster_PitchAxis)
				{
					CameraUserImpulseData->RotatePitchImpulse = 0.0f;
					//up and down only 90 degrees
					UnfilteredDelta=GetFilteredDelta(UnfilteredDelta*.5f, ViewportDeadZoneConstants::NO_DEAD_ZONE, StandardDeadZone);

					//Filter Pitch
					if (RecordingInterpEd)
					{
						INT MaxNumSamples = RecordingInterpEd->GetNumRecordPitchSmoothingSamples();
						if (PitchAngleHistory.Num() >= MaxNumSamples)
						{
							//remove the end of the array
							PitchAngleHistory.Remove(0, PitchAngleHistory.Num() - MaxNumSamples + 1);
						}
						PitchAngleHistory.AddItem(UnfilteredDelta);
						//now average the results
						FLOAT Total = 0.0f;
						for (INT HistoryIndex = 0; HistoryIndex < PitchAngleHistory.Num(); ++HistoryIndex)
						{
							Total += PitchAngleHistory(HistoryIndex);
						}
						UnfilteredDelta = Total/PitchAngleHistory.Num() + InConfig.PitchTrim;
					}

					ViewRotation.Pitch = appTrunc(-UnfilteredDelta*32768.f);
				}
				else if (Key == KEY_GameCaster_RollAxis)
				{
					//stop movement and set directly
					CameraUserImpulseData->RotateRollImpulse = 0.0f;
					//compensate for range
					UnfilteredDelta=GetFilteredDelta(UnfilteredDelta*.5f, ViewportDeadZoneConstants::NO_DEAD_ZONE, StandardDeadZone);

					//Filter Roll
					if (RecordingInterpEd)
					{
						INT MaxNumSamples = RecordingInterpEd->GetNumRecordRollSmoothingSamples();
						if (RollAngleHistory.Num() >= MaxNumSamples)
						{
							//remove the end of the array
							RollAngleHistory.Remove(0, RollAngleHistory.Num() - MaxNumSamples + 1);
						}
						RollAngleHistory.AddItem(UnfilteredDelta);
						//now average the results
						FLOAT Total = 0.0f;
						for (INT HistoryIndex = 0; HistoryIndex < RollAngleHistory.Num(); ++HistoryIndex)
						{
							Total += RollAngleHistory(HistoryIndex);
						}
						UnfilteredDelta = Total/RollAngleHistory.Num();
					}

					ViewRotation.Roll = appTrunc(UnfilteredDelta*32768.f);
				}
			}
		}
		if (bRelativeMovement)
		{
			for(TMap<FName,EInputEvent>::TConstIterator KeyIt(JoystickState->KeyEventValues);KeyIt;++KeyIt)
			{
				FName Key = KeyIt.Key();
				EInputEvent KeyState = KeyIt.Value();

				const UBOOL bPressed = (KeyState==IE_Pressed);
				const UBOOL bRepeat = (KeyState == IE_Repeat);

				if ((Key == KEY_XboxTypeS_LeftShoulder) && (bPressed || bRepeat))
				{
					CameraUserImpulseData->ZoomOutInImpulse +=  InConfig.ZoomMultiplier;
				}
				else if ((Key == KEY_XboxTypeS_RightShoulder) && (bPressed || bRepeat))
				{
					CameraUserImpulseData->ZoomOutInImpulse -= InConfig.ZoomMultiplier;
				}
				else if (RecordingInterpEd)
				{
					UBOOL bRepeatAllowed = RecordingInterpEd->IsRecordMenuChangeAllowedRepeat();
					if ((Key == KEY_XboxTypeS_DPad_Up) && bPressed)
					{
						const UBOOL bNextMenuItem = FALSE;
						RecordingInterpEd->ChangeRecordingMenu(bNextMenuItem);
						bRepeatAllowed = FALSE;
					}
					else if ((Key == KEY_XboxTypeS_DPad_Down) && bPressed)
					{
						const UBOOL bNextMenuItem = TRUE;
						RecordingInterpEd->ChangeRecordingMenu(bNextMenuItem);
						bRepeatAllowed = FALSE;
					}
					else if ((Key == KEY_XboxTypeS_DPad_Right) && (bPressed || (bRepeat && bRepeatAllowed)))
					{
						const UBOOL bIncrease= TRUE;
						RecordingInterpEd->ChangeRecordingMenuValue(this, bIncrease);
					}
					else if ((Key == KEY_XboxTypeS_DPad_Left) && (bPressed || (bRepeat && bRepeatAllowed)))
					{
						const UBOOL bIncrease= FALSE;
						RecordingInterpEd->ChangeRecordingMenuValue(this, bIncrease);
					}
					else if ((Key == KEY_XboxTypeS_RightThumbstick) && (bPressed))
					{
						const UBOOL bIncrease= TRUE;
						RecordingInterpEd->ResetRecordingMenuValue(this);
					}
					else if ((Key == KEY_XboxTypeS_LeftThumbstick) && (bPressed))
					{
						RecordingInterpEd->ToggleRecordMenuDisplay();
					}
					else if ((Key == KEY_XboxTypeS_A) && (bPressed))
					{
						RecordingInterpEd->ToggleRecordInterpValues();
					}
					else if ((Key == KEY_XboxTypeS_B) && (bPressed))
					{
						if (!RecordingInterpEd->Interp->bIsPlaying)
						{
							UBOOL bLoop = TRUE;
							UBOOL bForward = TRUE;
							RecordingInterpEd->StartPlaying(bLoop, bForward);
						}
						else
						{
							RecordingInterpEd->StopPlaying();
						}
					}

					if (!bRepeatAllowed)
					{
						//only respond to this event ONCE
						JoystickState->KeyEventValues.Remove(Key);
					}
				}
				if (bPressed)
				{
					//instantly set to repeat to stock rapid flickering until the time out
					JoystickState->KeyEventValues.Set(Key, IE_Repeat);
				}
			}
		}
	}
}
/**
 * Updates real-time camera movement.  Should be called every viewport tick!
 *
 * @param	DeltaTime	Time interval in seconds since last update
 */
void FEditorLevelViewportClient::UpdateCameraMovement( FLOAT DeltaTime )
{
	// We only want to move perspective cameras around like this
	if( Viewport != NULL && ViewportType == LVT_Perspective )
	{
		// Certain keys are only available while the flight camera input mode is active
		const UBOOL bUsingFlightInput = IsFlightCameraInputModeActive();

		// Do we want to use the regular arrow keys for flight input?
		// Because the arrow keys are used for nudging actors, we'll only do this while the ALT key is up
		const UBOOL bAltDown = Viewport->KeyState( KEY_LeftAlt ) || Viewport->KeyState( KEY_RightAlt );
		const UBOOL bRemapArrowKeys = !bAltDown;

		// Do we want to remap the various WASD keys for flight input?
		const UBOOL bRemapWASDKeys =
			(!Viewport->KeyState( KEY_LeftControl ) && !Viewport->KeyState( KEY_RightControl ) &&
			 !Viewport->KeyState( KEY_LeftShift ) && !Viewport->KeyState( KEY_RightShift ) &&
			 !bAltDown) &&
			(GEditor->GetUserSettings().FlightCameraControlType == WASD_Always ||
			 (bUsingFlightInput &&
			  (GEditor->GetUserSettings().FlightCameraControlType == WASD_RMBOnly &&
			   Viewport->KeyState(KEY_RightMouseButton))));

		//reset impulses if we're using WASD keys
		CameraUserImpulseData->MoveForwardBackwardImpulse = 0.0f;
		CameraUserImpulseData->MoveRightLeftImpulse = 0.0f;
		CameraUserImpulseData->MoveUpDownImpulse = 0.0f;
		CameraUserImpulseData->ZoomOutInImpulse = 0.0f;
		CameraUserImpulseData->RotateYawImpulse = 0.0f;
		CameraUserImpulseData->RotatePitchImpulse = 0.0f;
		CameraUserImpulseData->RotateRollImpulse = 0.0f;

		// Forward/back
		if( ( bRemapWASDKeys && Viewport->KeyState( KEY_W ) ) ||
			( bRemapArrowKeys && Viewport->KeyState( KEY_Up ) ) ||
			Viewport->KeyState( KEY_NumPadEight ) )
		{
			CameraUserImpulseData->MoveForwardBackwardImpulse += 1.0f;
		}
		if( ( bRemapWASDKeys && Viewport->KeyState( KEY_S ) ) ||
			( bRemapArrowKeys && Viewport->KeyState( KEY_Down ) ) ||
			Viewport->KeyState( KEY_NumPadTwo ) )
		{
			CameraUserImpulseData->MoveForwardBackwardImpulse -= 1.0f;
		}

		// Right/left
		if( ( bRemapWASDKeys && Viewport->KeyState( KEY_D ) ) ||
			( bRemapArrowKeys && Viewport->KeyState( KEY_Right ) ) ||
			Viewport->KeyState( KEY_NumPadSix ) )
		{
			CameraUserImpulseData->MoveRightLeftImpulse += 1.0f;
		}
		if( ( bRemapWASDKeys && Viewport->KeyState( KEY_A ) ) ||
			( bRemapArrowKeys && Viewport->KeyState( KEY_Left ) ) ||
			Viewport->KeyState( KEY_NumPadFour ) )
		{
			CameraUserImpulseData->MoveRightLeftImpulse -= 1.0f;
		}

		// Up/down
		if( ( bRemapWASDKeys && Viewport->KeyState( KEY_E ) ) ||
			Viewport->KeyState( KEY_PageUp ) || Viewport->KeyState( KEY_NumPadNine ) )
		{
			CameraUserImpulseData->MoveUpDownImpulse += 1.0f;
		}
		if( ( bRemapWASDKeys && Viewport->KeyState( KEY_Q ) ) ||
			Viewport->KeyState( KEY_PageDown ) || Viewport->KeyState( KEY_NumPadSeven ) )
		{
			CameraUserImpulseData->MoveUpDownImpulse -= 1.0f;
		}

		// Zoom FOV out/in
		if( ( bRemapWASDKeys && Viewport->KeyState( KEY_Z ) ) ||
			Viewport->KeyState( KEY_NumPadOne ) )
		{
			CameraUserImpulseData->ZoomOutInImpulse += 1.0f;
		}
		if( ( bRemapWASDKeys && Viewport->KeyState( KEY_C ) ) ||
			Viewport->KeyState( KEY_NumPadThree ) )
		{
			CameraUserImpulseData->ZoomOutInImpulse -= 1.0f;
		}

		if (!CameraController->IsRotating())
		{
			CameraController->GetConfig().bForceRotationalPhysics = FALSE;
		}

		UBOOL bIgnoreJoystickControls = FALSE;
		//if we're playing back (without recording), stop input from being processed
		if (RecordingInterpEd && RecordingInterpEd->Interp)
		{
			if (RecordingInterpEd->Interp->bIsPlaying && !RecordingInterpEd->IsRecordingInterpValues())
			{
				bIgnoreJoystickControls = TRUE;
			}

			CameraController->GetConfig().bPlanarCamera = (RecordingInterpEd->GetCameraMovementScheme() == MatineeConstants::CAMERA_SCHEME_PLANAR_CAM);
		}

		//Now update for cached joystick info (relative movement first)
		UpdateCameraMovementFromJoystick(TRUE, CameraController->GetConfig());

		//if we're not playing any cinematics right now
		if (!bIgnoreJoystickControls)
		{
			//Now update for cached joystick info (absolute movement second)
			UpdateCameraMovementFromJoystick(FALSE, CameraController->GetConfig());
		}

		FVector ViewEuler = ViewRotation.Euler();
		FVector NewViewLocation = ViewLocation;
		FRotator NewViewRotation = ViewRotation;
		FVector NewViewEuler = ViewEuler;
		FLOAT NewViewFOV = ViewFOV;

		// We'll combine the regular camera speed scale (controlled by viewport toolbar setting) with
		// the flight camera speed scale (controlled by mouse wheel).
		const FLOAT FinalCameraSpeedScale = FlightCameraSpeedScale * ( CameraSpeed / MOVEMENTSPEED_SLOW );

		// Only allow FOV recoil if flight camera mode is currently inactive.
		const UBOOL bAllowRecoilIfNoImpulse = (!bUsingFlightInput) && (!IsMatineeRecordingWindow());

		// Update the camera's position, rotation and FOV
		CameraController->UpdateSimulation(
			*CameraUserImpulseData,
			DeltaTime,
			bAllowRecoilIfNoImpulse,
			FinalCameraSpeedScale,
			NewViewLocation,
			NewViewEuler,
			NewViewFOV );


		// We'll zero out rotation velocity modifier after updating the simulation since these actions
		// are always momentary -- that is, when the user mouse looks some number of pixels,
		// we increment the impulse value right there
		{
			CameraUserImpulseData->RotateYawVelocityModifier = 0.0f;
			CameraUserImpulseData->RotatePitchVelocityModifier = 0.0f;
			CameraUserImpulseData->RotateRollVelocityModifier = 0.0f;
		}


		if( !ViewEuler.Equals( NewViewEuler, SMALL_NUMBER ) )
		{
			NewViewRotation = FRotator::MakeFromEuler( NewViewEuler );
		}

		if( !NewViewLocation.Equals( ViewLocation, SMALL_NUMBER ) ||
			NewViewRotation != ViewRotation ||
			!appIsNearlyEqual( NewViewFOV, ViewFOV, FLOAT(SMALL_NUMBER) ) )
		{
			// Something has changed!
			Invalidate();

			// Update the FOV
			ViewFOV = NewViewFOV;
			
			// Actually move/rotate the camera
			MoveViewportPerspectiveCamera(
				NewViewLocation - ViewLocation,
				NewViewRotation - ViewRotation,
				TRUE );		// Unlit movement?
		}
	}
}



void FEditorLevelViewportClient::UpdateMouseDelta()
{
	// Do nothing if a drag tool is being used.
	if( MouseDeltaTracker->UsingDragTool() )
	{
		return;
	}

	FVector DragDelta;

	// If any actor in the selection requires snapping, they all need to be snapped.
	UBOOL bNeedMovementSnapping = FALSE;

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( Actor->bEdShouldSnap )
		{
			bNeedMovementSnapping = TRUE;
			break;
		}
	}

	if( Widget->GetCurrentAxis() != AXIS_None && bNeedMovementSnapping )
	{
		DragDelta = MouseDeltaTracker->GetDeltaSnapped();
	}
	else
	{
		DragDelta = MouseDeltaTracker->GetDelta();
	}

	GEditor->MouseMovement += FVector( Abs(DragDelta.X), Abs(DragDelta.Y), Abs(DragDelta.Z) );

	if( Viewport )
	{
		// Update the status bar text using the absolute change.
		UpdateMousePositionTextUsingDelta( bNeedMovementSnapping );

		if( !DragDelta.IsNearlyZero() )
		{
			const UBOOL AltDown = Input->IsAltPressed();
			const UBOOL ShiftDown = Input->IsShiftPressed();
			const UBOOL ControlDown = Input->IsCtrlPressed();
			const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton);
			const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

			if ( !Input->IsPressed(KEY_L) )
			{
				MayaActor = NULL;
			}

			for( INT x = 0 ; x < 3 ; ++x )
			{
				FVector WkDelta( 0.f, 0.f, 0.f );
				WkDelta[x] = DragDelta[x];
				if( !WkDelta.IsZero() )
				{
					// Convert the movement delta into drag/rotation deltas
					FVector Drag;
					FRotator Rot;
					FVector Scale;
					EAxis CurrentAxis = Widget->GetCurrentAxis();
					if ( IsOrtho() && LeftMouseButtonDown && RightMouseButtonDown )
					{
						bWidgetAxisControlledByDrag = FALSE;
						Widget->SetCurrentAxis( AXIS_None );
						MouseDeltaTracker->ConvertMovementDeltaToDragRot( this, WkDelta, Drag, Rot, Scale );
						Widget->SetCurrentAxis( CurrentAxis );
						CurrentAxis = AXIS_None;
					}
					else
					{
						//if Absolute Translation, and not just moving the camera around
						if (IsUsingAbsoluteTranslation())
						{
							// Compute a view.
							FSceneViewFamilyContext ViewFamily(Viewport, GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds(), IsRealtime());
							FSceneView* View = CalcSceneView( &ViewFamily );

							MouseDeltaTracker->AbsoluteTranslationConvertMouseToDragRot(View, this, Drag, Rot, Scale);
							//only need to go through this loop once
							x = 3;
						} 
						else
						{
							MouseDeltaTracker->ConvertMovementDeltaToDragRot( this, WkDelta, Drag, Rot, Scale );
						}
					}

					// Give the current editor mode a chance to use the input first.  If it does, don't apply it to anything else.
					if( GEditorModeTools().InputDelta( this, Viewport, Drag, Rot, Scale ) )
					{
						if( GEditorModeTools().AllowWidgetMove() )
						{
							GEditorModeTools().PivotLocation += Drag;
							GEditorModeTools().SnappedLocation += Drag;
						}

						// Update visuals of the rotate widget 
						ApplyDeltaToRotateWidget( Rot );
					}
					else
					{
						if( CurrentAxis != AXIS_None )
						{
							// Is the user not dragging by the widget handles? If so, Ctrl key toggles between dragging
							// the viewport or dragging the actor.
							if( !ControlDown && !AltDown && CurrentAxis == AXIS_None )
							{
								// Only apply camera speed modifiers to the drag if we aren't zooming in an ortho viewport.
								if( !IsOrtho() || !(LeftMouseButtonDown && RightMouseButtonDown) )
								{
									Drag *= GCurrentLevelEditingViewportClient->CameraSpeed / 4.f;
								}
								FVector CameraDelta( Drag );
								if (ViewportType==LVT_OrthoXY)
								{
									CameraDelta.X = -Drag.Y;
									CameraDelta.Y = Drag.X;
								}
								MoveViewportCamera( CameraDelta, Rot );
							}
							else
							{
								// If duplicate dragging . . .
								if ( AltDown && (LeftMouseButtonDown || RightMouseButtonDown) )
								{
									// The widget has been offset, so check if we should duplicate actors.
									if ( bDuplicateActorsOnNextDrag )
									{
										// Only duplicate if we're translating or rotating.
										if ( !Drag.IsNearlyZero() || !Rot.IsZero() )
										{
											// Actors haven't been dragged since ALT+LMB went down.
											bDuplicateActorsOnNextDrag = FALSE;

											GEditor->edactDuplicateSelected( FALSE );
										}
									}
								}

								// Apply deltas to selected actors or viewport cameras
								ApplyDeltaToActors( Drag, Rot, Scale );
								GEditorModeTools().PivotLocation += Drag;
								GEditorModeTools().SnappedLocation += Drag;

								if( ShiftDown )
								{
									FVector CameraDelta( Drag );
									if (ViewportType==LVT_OrthoXY)
									{
										CameraDelta.X = -Drag.Y;
										CameraDelta.Y = Drag.X;
									}
									MoveViewportCamera( CameraDelta, FRotator(0,0,0) );
								}

								TArray<FEdMode*> ActiveModes; 
								GEditorModeTools().GetActiveModes( ActiveModes );

								for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
								{
									ActiveModes(ModeIndex)->UpdateInternalData();
								}
							} 
						}
						else
						{
							// Only apply camera speed modifiers to the drag if we aren't zooming in an ortho viewport.

							AActor* SelectedActor = GEditor->GetSelectedActors()->GetTop<AActor>();
							
							const UBOOL bKeyDownU = Viewport->KeyState(KEY_U);
							const UBOOL bKeyPressedL = Viewport->KeyState(KEY_L);

							if ( !IsOrtho() && GEditor->bUseMayaCameraControls && 
								(bKeyDownU || (bKeyPressedL && SelectedActor)) )
							{
								if (!bUsingMayaCam)
								{
									// Enable Maya cam if U or L keys are down.
									bAllowMayaCam = TRUE;
									SwitchStandardToMaya();
									if (bKeyPressedL && SelectedActor)
									{
										MayaZoom.Y = (ViewLocation - SelectedActor->Location).Size();
										MayaLookAt = SelectedActor->Location;
										MayaActor = SelectedActor;
									}
								}
								else
								{
									// Switch focus if selected actor changes
									if (bKeyPressedL && SelectedActor && 
										(SelectedActor != MayaActor))
									{
										MayaLookAt = SelectedActor->Location;
										MayaActor = SelectedActor;
									}
								}

								FVector TempDrag;
								FRotator TempRot;
								InputAxisMayaCam( Viewport, DragDelta, TempDrag, TempRot );
							}
							else
							{
								// Disable Maya cam
								if ( bUsingMayaCam )
								{
									bAllowMayaCam = FALSE;
									SwitchMayaToStandard();
								}

								if( !IsOrtho() || !(LeftMouseButtonDown && RightMouseButtonDown) )
								{
									Drag *= GCurrentLevelEditingViewportClient->CameraSpeed / 4.f;
								}
								MoveViewportCamera( Drag, Rot );
							}
						}
					}

					// Clean up
					MouseDeltaTracker->ReduceBy( WkDelta );
				}
			}

			Invalidate( FALSE, TRUE );
		}
	}
}

/**
 * forces a cursor update and marks the window as a move has occurred
 */
void FEditorLevelViewportClient::MarkMouseMovedSinceClick()
{
	if (!bHasMouseMovedSinceClick )
	{
		bHasMouseMovedSinceClick = TRUE;
		//if we care about the cursor
		if (Viewport->IsCursorVisible() && Viewport->HasMouseCapture())
		{
			//force a refresh
			Viewport->UpdateMouseCursor(TRUE);
		}
	}
}


/**
* Calculates absolute transform for mouse position status text using absolute mouse delta.
*
* @param bUseSnappedDelta Whether or not to use absolute snapped delta for transform calculations.
*/
void FEditorLevelViewportClient::UpdateMousePositionTextUsingDelta( UBOOL bUseSnappedDelta )
{
	// Only update using the info from the active viewport.
	if(GCurrentLevelEditingViewportClient != this || !bIsTracking)
	{
		return;
	}

	// We need to calculate the absolute change in Drag, Rotation, and Scale since we started,
	// so we use GEditor->MouseMovement as our delta.
	FVector Delta;

	if( bUseSnappedDelta )
	{
		Delta = MouseDeltaTracker->GetAbsoluteDeltaSnapped();
	}
	else
	{
		Delta = MouseDeltaTracker->GetAbsoluteDelta();
	}

	// Delta.Z isn't necessarily set by the MouseDeltaTracker
	Delta.Z = 0.0f;

	//Make sure our delta is a tangible amount to avoid divide by zero problems.
	if( Delta.IsNearlyZero() )
	{
		if( CanUpdateStatusBarText() )
		{
			GEditor->UpdateMousePositionText(UEditorEngine::MP_NoChange, FVector(0,0,0));
		}

		return;
	}

	// We need to separate the deltas for each of the axis and compute the sum of the change vectors
	// because the ConvertMovement function only uses the 'max' axis as its source for converting delta to movement.
	FVector Drag;
	FRotator Rot;
	FVector Scale;

	//if Absolute Translation, and not just moving the camera around
	if (IsUsingAbsoluteTranslation())
	{
		// Compute a view.
		FSceneViewFamilyContext ViewFamily(Viewport, GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds(), IsRealtime());
		FSceneView* View = CalcSceneView( &ViewFamily );

		MouseDeltaTracker->AbsoluteTranslationConvertMouseToDragRot(View, this, Drag, Rot, Scale);
	}
	else
	{
		FVector TempDrag;
		FRotator TempRot;
		FVector TempScale;

		MouseDeltaTracker->ConvertMovementDeltaToDragRot( this, FVector(Delta.X,0,0), TempDrag, TempRot, TempScale );
		MouseDeltaTracker->ConvertMovementDeltaToDragRot( this, FVector(0,Delta.Y,0), Drag, Rot, Scale );

		Drag += TempDrag;
		Rot += TempRot;
		Scale += TempScale;
	}

	// Now we see which movement mode we are working on and use that to determine which operation the
	// user is doing.
	FVector ReadableRotation( Rot.Pitch, Rot.Yaw, Rot.Roll );
	const UBOOL bTranslating = !Drag.IsNearlyZero();
	const UBOOL bRotating = !ReadableRotation.IsNearlyZero();
	const UBOOL bScaling = !Scale.IsNearlyZero();

	
	if( CanUpdateStatusBarText() )
	{
		if( bTranslating )
		{
			GEditor->UpdateMousePositionText(UEditorEngine::MP_Translate, Drag);
		}
		else if( bRotating )
		{
			GEditor->UpdateMousePositionText(UEditorEngine::MP_Rotate, ReadableRotation);
		}
		else if( bScaling )
		{
			GEditor->UpdateMousePositionText(UEditorEngine::MP_Scale, Scale);
		}
		else
		{
			GEditor->UpdateMousePositionText(UEditorEngine::MP_NoChange, FVector(0,0,0));
		}
	}
}

/** Determines whether this viewport is currently allowed to use Absolute Movement */
UBOOL FEditorLevelViewportClient::IsUsingAbsoluteTranslation (void)
{
	UBOOL bIsHotKeyAxisLocked = Input->IsCtrlPressed();
	UBOOL bCameraLockedToWidget = Input->IsShiftPressed();
	UBOOL bAbsoluteMovementEnabled = GEditorModeTools().IsUsingAbsoluteTranslation();
	UBOOL bCurrentWidgetSupportsAbsoluteMovement = FWidget::AllowsAbsoluteTranslationMovement();
	UBOOL bWidgetActivelyTrackingAbsoluteMovement = Widget && (Widget->GetCurrentAxis() != AXIS_None);

	const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL MiddleMouseButtonDown = Viewport->KeyState(KEY_MiddleMouseButton);
	const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	const UBOOL bAnyMouseButtonsDown = (LeftMouseButtonDown || MiddleMouseButtonDown || RightMouseButtonDown);

	// Allow absolute transform when combined widget is selected even in ortho viewport otherwise rotation won't work
	const UBOOL bNotOrthoOrCombinedWidget = !IsOrtho() || GEditorModeTools().GetWidgetMode() == FWidget::WM_TranslateRotateZ;

	return (!bCameraLockedToWidget && !bIsHotKeyAxisLocked && bAbsoluteMovementEnabled && bCurrentWidgetSupportsAbsoluteMovement && bWidgetActivelyTrackingAbsoluteMovement && bNotOrthoOrCombinedWidget && bAnyMouseButtonsDown);
}


/**
* Converts the character to a suitable FName and has the app check if a global hotkey was pressed.
* @param	Viewport - The viewport which the keyboard input is from.
* @param	KeyName - The key that was pressed
*/
void FEditorLevelViewportClient::CheckIfGlobalHotkey(FViewport* Viewport, FName KeyName)
{
	const UBOOL bCtrlPressed	= ( Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl) );
	const UBOOL bAltPressed		= ( Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt) );
	const UBOOL bShiftPressed	= ( Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift) );

	GApp->CheckIfGlobalHotkey( KeyName, bCtrlPressed, bShiftPressed, bAltPressed );
}

//
//	FEditorLevelViewportClient::InputKey
//

UBOOL FEditorLevelViewportClient::InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT/*AmountDepressed*/,UBOOL/*Gamepad*/)
{
	if (bDisableInput)
	{
		return TRUE;
	}

	const INT	HitX = Viewport->GetMouseX();
	const INT	HitY = Viewport->GetMouseY();

	FCachedJoystickState* JoystickState = JoystickStateMap.FindRef(ControllerId);
	if (JoystickState)
	{
		JoystickState->KeyEventValues.Set(Key, Event);
	}

	// Remember which keys and buttons are pressed down.
	const UBOOL bCtrlButtonEvent = (Key == KEY_LeftControl || Key == KEY_RightControl);
	const UBOOL bShiftButtonEvent = (Key == KEY_LeftShift || Key == KEY_RightShift);
	const UBOOL bAltButtonEvent = (Key == KEY_LeftAlt || Key == KEY_RightAlt);
	const UBOOL TabDown	= Viewport->KeyState(KEY_Tab); 
	const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL MiddleMouseButtonDown = Viewport->KeyState(KEY_MiddleMouseButton);
	const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);
	const UBOOL bMouseButtonDown = (LeftMouseButtonDown || MiddleMouseButtonDown || RightMouseButtonDown );
	const UBOOL bMouseButtonEvent = (Key == KEY_LeftMouseButton || Key == KEY_MiddleMouseButton || Key == KEY_RightMouseButton);

	// Check to see if the current event is a modifier key and that key was already in the
	// same state.
	const UBOOL bIsRedundantModifierEvent =
		( bAltButtonEvent && ( ( Event != IE_Released ) == Input->IsAltPressed() ) ) ||
		( bCtrlButtonEvent && ( ( Event != IE_Released ) == Input->IsCtrlPressed() ) ) ||
		( bShiftButtonEvent && ( ( Event != IE_Released ) == Input->IsShiftPressed() ) );

	// Store a reference to the last viewport that received a keypress.
	GLastKeyLevelEditingViewportClient = this;

	if( GCurrentLevelEditingViewportClient != this )
	{
		if (GCurrentLevelEditingViewportClient)
		{
			//redraw without yellow selection box
			GCurrentLevelEditingViewportClient->Invalidate();
		}
		//cause this viewport to redraw WITH yellow selection box
		Invalidate();
		GCurrentLevelEditingViewportClient = this;
	}

	// Compute a view.
	FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds(),IsRealtime());
	FSceneView* View = CalcSceneView( &ViewFamily );

	// Compute the click location.

	GEditor->ClickLocation = FVector((View->ViewMatrix * View->ProjectionMatrix).Inverse().TransformFVector4(FVector4((HitX - Viewport->GetSizeX() / 2.0f) / (Viewport->GetSizeX() / 2.0f),(HitY - Viewport->GetSizeY() / 2.0f) / -(Viewport->GetSizeY() / 2.0f),0.5f,1.0f)));

	// Let the current mode have a look at the input before reacting to it.
	if( GEditorModeTools().InputKey(this, Viewport, Key, Event) )
	{
		return TRUE;
	}

	// Tell the viewports input handler about the keypress.
	if( Input->InputKey(ControllerId, Key, Event) )
	{
		return TRUE;
	}

	// Handle input for the player height preview mode. 
	if( Key == KEY_Backslash )
	{
		// Holding down the backslash buttons turns on the mode. 
		if( Event == IE_Pressed )
		{
			GEditor->SetPreviewMeshMode(TRUE);

			// If shift down, cycle between the preview meshes
			if( Input->IsShiftPressed() )
			{
				GEditor->CyclePreviewMesh();
			}
		}
		// Releasing backslash turns off the mode. 
		else if( Event == IE_Released )
		{
			GEditor->SetPreviewMeshMode(FALSE);
		}
	}

	// Tell game stats viewer about key
	GEditor->GameStatsInputKey(this, Key, Event);

	// Tell Sentinel stats viewer about key
	GEditor->SentinelInputKey(this, Key, Event);

	// Tell the other editor viewports about the keypress so they will be aware if the user tries
	// to do something like box select inside of them without first clicking them for focus.
	for( INT v = 0 ; v < GEditor->ViewportClients.Num() ; ++v )
	{
		FEditorLevelViewportClient* vc = GEditor->ViewportClients(v);

		if( vc != this )
		{
			vc->Input->InputKey( ControllerId, Key, Event );
		}
	}

	// Remember which modifier keys are held down
	// NOTE:	This cannot occur prior to Input->InputKey being called in this function or
	//			these will be set incorrectly for the first time the keys are actually pressed
	const UBOOL AltDown = !( bAltButtonEvent && Event == IE_Released ) && Input->IsAltPressed();
	const UBOOL	ShiftDown = !( bShiftButtonEvent && Event == IE_Released ) && Input->IsShiftPressed();
	const UBOOL ControlDown = !( bCtrlButtonEvent && Event == IE_Released ) && Input->IsCtrlPressed();

	if ( !AltDown && ControlDown && !LeftMouseButtonDown && !MiddleMouseButtonDown && RightMouseButtonDown && IsOrtho() )
	{
		GEditorModeTools().SetWidgetModeOverride( FWidget::WM_Rotate );
	}
	else
	{
		GEditorModeTools().SetWidgetModeOverride( FWidget::WM_None );
	}

	// We'll make sure the mouse is confined and the cursor is hidden while the mouse button
	// is pressed down, unless we've just double-clicked
	if ( bMouseButtonEvent && Event != IE_DoubleClick )
	{
		//Update cursor and lock to window if invisible
		UBOOL bShowCursor = UpdateCursorVisibility ();
		Viewport->LockMouseToWindow( !bShowCursor );
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	if (!bMouseButtonDown)
	{
		bHasMouseMovedSinceClick = FALSE;
	}

	// Start tracking if any mouse button is down and it was a tracking event (MouseButton/Ctrl/Shift/Alt):
	if ( bMouseButtonDown && (Event == IE_Pressed || Event == IE_Released) && (bMouseButtonEvent || bCtrlButtonEvent || bAltButtonEvent || bShiftButtonEvent) )		
	{
		//First mouse down, note where they clicked
		LastMouseX = Viewport->GetMouseX();
		LastMouseY = Viewport->GetMouseY();

		// Only start (or restart) tracking mode if the current event wasn't a modifier key that
		// was already pressed or released.
		if( !bIsRedundantModifierEvent )
		{
			bDraggingByHandle = (Widget->GetCurrentAxis() != AXIS_None);

			// Stop current tracking
			if ( bIsTracking )
			{
				MouseDeltaTracker->EndTracking( this );
				bIsTracking = FALSE;
			}

			// Re-initialize new tracking only if a new button was pressed, otherwise we continue the previous one.
			if ( Event == IE_Pressed )
			{
				// Tracking initialization:
				GEditor->MouseMovement = FVector(0,0,0);
			
				if ( AltDown )
				{
					if(Event == IE_Pressed && (Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton) && !bDuplicateActorsInProgress)
					{
						// Set the flag so that the actors actors will be duplicated as soon as the widget is displaced.
						bDuplicateActorsOnNextDrag = TRUE;
						bDuplicateActorsInProgress = TRUE;
					}
				}
				else
				{
					bDuplicateActorsOnNextDrag = FALSE;
				}
			}

			// Start new tracking. Potentially reset the widget so that StartTracking can pick a new axis.
			if ( !bDraggingByHandle || (ControlDown && !TabDown) ) //|| (bDraggingByHandle && !ControlDown))
			{
				bWidgetAxisControlledByDrag = FALSE;
				Widget->SetCurrentAxis( AXIS_None );
			}
			MouseDeltaTracker->StartTracking( this, HitX, HitY );
			bIsTracking = TRUE;

			//only reset the initial point when the mouse is actually clicked
			if (bMouseButtonEvent && Widget)
			{
				Widget->ResetInitialTranslationOffset();
			}

			// See if any brushes are about to be transformed via their Widget
			if (bDraggingByHandle)
			{
				for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It && !bIsTrackingBrushModification; ++It)
				{
					AActor* Actor = static_cast<AActor*>(*It);
					checkSlow( Actor->IsA(AActor::StaticClass()) );

					// First, check for selected brush actors
					if (Actor->IsABrush() && !Actor->IsABuilderBrush())
					{
						bIsTrackingBrushModification = TRUE;
					}
					else // Next, check for selected groups actors that contain brushes
					{
						AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
						if (GroupActor)
						{
							TArray<AActor*> GroupMembers;
							GroupActor->GetAllChildren(GroupMembers, TRUE);
							for (INT GroupMemberIdx = 0; GroupMemberIdx < GroupMembers.Num(); ++GroupMemberIdx)
							{
								if (GroupMembers(GroupMemberIdx)->IsABrush() && !GroupMembers(GroupMemberIdx)->IsABuilderBrush())
								{
									bIsTrackingBrushModification = TRUE;
								}
							}
						}
					}
				}
			}

			//Dont update the cursor visibility if we dont have focus or mouse capture 
			if( Viewport->HasFocus() ||  Viewport->HasMouseCapture())
			{
				//Need to call this one more time as the axis variable for the widget has just been updated
				UBOOL bShowCursor = UpdateCursorVisibility();
				Viewport->LockMouseToWindow( !bShowCursor );
			}
		}
		return TRUE;
	}

	// Stop tracking if no mouse button is down
	if ( bIsTracking && !bMouseButtonDown && bMouseButtonEvent )
	{
		if ( CachedShowFlags )
		{
			ShowFlags = CachedShowFlags;
			CachedShowFlags = 0;
		}
		
		// Ignore actor manipulation if we're using a tool
		if ( !MouseDeltaTracker->UsingDragTool() )
		{
			// If the mouse haven't moved too far, treat the button release as a click.
			if( GEditor->MouseMovement.Size() < MOUSE_CLICK_DRAG_DELTA && !MouseDeltaTracker->UsingDragTool() && !MouseDeltaTracker->WasExternalMovement() )
			{
				HHitProxy* HitProxy = Viewport->GetHitProxy(HitX,HitY);

				// If this was a mouse click that clicked on something (thereby selecting it) and the builder brush
				// had been auto-selected deselect it so that only the clicked object becomes selected.
				if(HitProxy != NULL && MouseDeltaTracker->bSelectedBuilderBrush)
				{
					GEditor->SelectNone(FALSE, FALSE);
				}

				ProcessClick(View,HitProxy,Key,Event,HitX,HitY);
			}
			else
			{
				// Only disable the duplicate on next drag flag if we actually dragged the mouse.
				bDuplicateActorsOnNextDrag = FALSE;
			}

			bWidgetAxisControlledByDrag = FALSE;
			Widget->SetCurrentAxis( AXIS_None );
			Invalidate( TRUE, TRUE );
		}

		// Finish tracking a brush tranform and update the Bsp
		if (bIsTrackingBrushModification)
		{
			bIsTrackingBrushModification = FALSE;
			GEditor->RebuildAlteredBSP();
		}

		MouseDeltaTracker->EndTracking( this );
		
		MouseDeltaTracker->bSelectedBuilderBrush = FALSE;
		bIsTracking = FALSE;
	}

	// Clear Duplicate Actors mode when ALT and all mouse buttons are released
	if ( !AltDown && !bMouseButtonDown )
	{
		bDuplicateActorsInProgress = FALSE;
	}

	if ( Event == IE_DoubleClick )
	{
		// Stop current tracking
		if ( bIsTracking )
		{
			MouseDeltaTracker->EndTracking( this );
			bIsTracking = FALSE;
		}

		MouseDeltaTracker->StartTracking( this, HitX, HitY );
		bIsTracking = TRUE;
		GEditor->MouseMovement = FVector(0,0,0);
		HHitProxy*	HitProxy = Viewport->GetHitProxy(HitX,HitY);
		ProcessClick(View,HitProxy,Key,Event,HitX,HitY);
		MouseDeltaTracker->EndTracking( this );
		bIsTracking = FALSE;

		// This needs to be set to false to allow the axes to update
		bWidgetAxisControlledByDrag = FALSE;

		return TRUE;
	}

	if( ( Key == KEY_MouseScrollUp || Key == KEY_MouseScrollDown ) && Event == IE_Pressed )
	{
		if ( IsOrtho() )
		{
			// Scrolling the mousewheel up/down zooms the orthogonal viewport in/out.
			INT Delta = 25;
			if( Key == KEY_MouseScrollUp )
			{
				Delta *= -1;
			}

			//Extract current state
			INT ViewportWidth = Viewport->GetSizeX();
			INT ViewportHeight = Viewport->GetSizeY();

			FVector OldOffsetFromCenter;
			if (GEditorModeTools().GetCenterZoomAroundCursor())
			{
				//Y is actually backwards, but since we're move the camera opposite the cursor to center, we negate both
				//therefore the x is negated
				//X Is backwards, negate it
				//default to viewport mouse position
				INT CenterX = Viewport->GetMouseX();
				INT CenterY = Viewport->GetMouseY();
				if (ShouldUseMoveCanvasMovement())
				{
					//use virtual mouse while dragging (normal mouse is clamped when invisible)
					CenterX = LastMouseX;
					CenterY = LastMouseY;
				}
				INT DeltaFromCenterX = -(CenterX - (ViewportWidth>>1));
				INT DeltaFromCenterY =  (CenterY - (ViewportHeight>>1));
				switch( ViewportType )
				{
					case LVT_OrthoXY:
						//note, x and y are reversed for the top viewport.
						OldOffsetFromCenter.Set(DeltaFromCenterY, DeltaFromCenterX, 0.0f);
						break;
					case LVT_OrthoXZ:
						OldOffsetFromCenter.Set(DeltaFromCenterX, 0.0f, DeltaFromCenterY);
						break;
					case LVT_OrthoYZ:
						OldOffsetFromCenter.Set(0.0f, DeltaFromCenterX, DeltaFromCenterY);
						break;
				}
			}
			
			//save off old zoom
			FLOAT OldOrthoZoom = OrthoZoom;

			//update zoom based on input
			OrthoZoom += (OrthoZoom / CAMERA_ZOOM_DAMPEN) * Delta;
			OrthoZoom = Clamp<FLOAT>( OrthoZoom, MIN_ORTHOZOOM, MAX_ORTHOZOOM );

			if (GEditorModeTools().GetCenterZoomAroundCursor())
			{
				//This is the equivalent to moving the viewport to center about the cursor, zooming, and moving it back a proportional amount towards the cursor
				FVector FinalDelta = ((OrthoZoom - OldOrthoZoom)/CAMERA_ZOOM_DIV)*OldOffsetFromCenter;
				//now move the view location proportionally
				ViewLocation += FinalDelta;
				ViewLocation.BoundToCube(HALF_WORLD_MAX1);
			}

			// Update linked ortho viewport movement based on updated zoom and view location, 
			UpdateLinkedOrthoViewports( TRUE );

			Invalidate( TRUE, TRUE );
			//mark "externally moved" so context menu doesn't come up
			MouseDeltaTracker->SetExternalMovement();
		}
		else
		{
			// If flight camera input is active, then the mouse wheel will control the speed of camera
			// movement
			if( IsFlightCameraInputModeActive() )
			{
				const FLOAT MinCameraSpeedScale = 0.1f;
				const FLOAT MaxCameraSpeedScale = 10.0f;

				// Adjust and clamp the camera speed scale
				if( Key == KEY_MouseScrollUp )
				{
					if( FlightCameraSpeedScale >= 2.0f )
					{
						FlightCameraSpeedScale += 0.5f;
					}
					else if( FlightCameraSpeedScale >= 1.0f )
					{
						FlightCameraSpeedScale += 0.2f;
					}
					else
					{
						FlightCameraSpeedScale += 0.1f;
					}
				}
				else
				{
					if( FlightCameraSpeedScale > 2.49f )
					{
						FlightCameraSpeedScale -= 0.5f;
					}
					else if( FlightCameraSpeedScale >= 1.19f )
					{
						FlightCameraSpeedScale -= 0.2f;
					}
					else
					{
						FlightCameraSpeedScale -= 0.1f;
					}
				}

				FlightCameraSpeedScale = Clamp( FlightCameraSpeedScale, MinCameraSpeedScale, MaxCameraSpeedScale );

				if( appIsNearlyEqual( FlightCameraSpeedScale, 1.0f, 0.01f ) )
				{
					// Snap to 1.0 if we're really close to that
					FlightCameraSpeedScale = 1.0f;
				}

				// Keep track of when the camera speed was changed list so that the status bar is
				// updated appropriately
				RealTimeFlightCameraSpeedScaleChanged = appSeconds();

				GEditor->UpdateMousePositionText(
					UEditorEngine::MP_CameraSpeed,
					FVector( FlightCameraSpeedScale, 0.0f, 0.0f ) );	// X = Camera speed value
			}
			else
			{
				// Scrolling the mousewheel up/down moves the perspective viewport forwards/backwards.
				FVector Drag(0,0,0);

				Drag.X = GMath.CosTab( ViewRotation.Yaw ) * GMath.CosTab( ViewRotation.Pitch );
				Drag.Y = GMath.SinTab( ViewRotation.Yaw ) * GMath.CosTab( ViewRotation.Pitch );
				Drag.Z = GMath.SinTab( ViewRotation.Pitch );

				if( Key == KEY_MouseScrollDown )
				{
					Drag = -Drag;
				}

				Drag *= GCurrentLevelEditingViewportClient->CameraSpeed * 8.f;

				MoveViewportCamera( Drag, FRotator(0,0,0), FALSE );
				Invalidate( TRUE, TRUE );
			}
		}
	}
	else if( ControlDown && MiddleMouseButtonDown )
	{
		FVector vec;

		if( IsOrtho() )
		{
			vec = GEditor->ClickLocation;
		}
		else
		{
			vec = ViewLocation;
		}

		INT idx = -1;

		switch( ViewportType )
		{
			case LVT_OrthoXY:
				idx = 2;
				break;
			case LVT_OrthoXZ:
				idx = 1;
				break;
			case LVT_OrthoYZ:
				idx = 0;
				break;
		}

		for( INT v = 0 ; v < GEditor->ViewportClients.Num() ; ++v )
		{
			FEditorLevelViewportClient* vc = GEditor->ViewportClients(v);

			if( vc != this )
			{
				if( IsOrtho() )
				{
					if( idx != 0 )		vc->ViewLocation.X = vec.X;
					if( idx != 1 )		vc->ViewLocation.Y = vec.Y;
					if( idx != 2 )		vc->ViewLocation.Z = vec.Z;
				}
				else
				{
					vc->ViewLocation = vec;
				}
				vc->Invalidate( FALSE, TRUE );
			}
		}
	}
	else if( (IsOrtho() || AltDown) && (Key == KEY_Left || Key == KEY_Right || Key == KEY_Up || Key == KEY_Down) )
	{
		if( Event == IE_Pressed || Event == IE_Repeat )
		{
			// If this is a pressed event, start tracking.
			if ( !bIsTracking && Event == IE_Pressed )
			{
				//@{
				//@note ronp - without the check for !bIsTracking line or two up, the following code would cause a new transaction to be created
				// for each "nudge" that occurred while the key was held down.  Disabling this code prevents the transaction
				// from being constantly recreated while as long as the key is held, so that the entire move is considered an atomic action (and
				// doing undo reverts the entire movement, as opposed to just the last nudge that occurred while the key was held down)

				// Stop current tracking
// 				if ( bIsTracking )
// 				{
// 					MouseDeltaTracker->EndTracking( this );
// 					bIsTracking = FALSE;
// 				}
				//@}

				MouseDeltaTracker->StartTracking( this, HitX, HitY, TRUE );
				bIsTracking = TRUE;
			}

			// Keyboard nudging of the widget
			if( Key == KEY_Left || Key == KEY_Right )
			{
				bWidgetAxisControlledByDrag = FALSE;
				Widget->SetCurrentAxis( GetHorizAxis() );
				MouseDeltaTracker->AddDelta( this, KEY_MouseX, GEditor->Constraints.GetGridSize() * (Key == KEY_Left?-1:1), 1 );
				Widget->SetCurrentAxis( GetHorizAxis() );
			} 
			else if( Key == KEY_Up || Key == KEY_Down )
			{
				bWidgetAxisControlledByDrag = FALSE;
				Widget->SetCurrentAxis( GetVertAxis() );
				MouseDeltaTracker->AddDelta( this, KEY_MouseY, GEditor->Constraints.GetGridSize() * (Key == KEY_Up?1:-1), 1 );
				Widget->SetCurrentAxis( GetVertAxis() );
			}

			UpdateMouseDelta();
		}
		else if( bIsTracking && Event == IE_Released )
		{
			bWidgetAxisControlledByDrag = FALSE;
			MouseDeltaTracker->EndTracking( this );
			bIsTracking = FALSE;
			Widget->SetCurrentAxis( AXIS_None );
		}
		
		GEditor->RedrawLevelEditingViewports();
	}
	else if( (!Viewport->KeyState( KEY_LeftControl ) && !Viewport->KeyState( KEY_RightControl ) &&
			  !Viewport->KeyState( KEY_LeftShift ) && !Viewport->KeyState( KEY_RightShift ) &&
			  !Viewport->KeyState( KEY_LeftAlt ) && !Viewport->KeyState( KEY_RightAlt )) &&
			 (GEditor->GetUserSettings().FlightCameraControlType == WASD_Always ||
			  (IsFlightCameraInputModeActive() &&
			   ( GEditor->GetUserSettings().FlightCameraControlType == WASD_RMBOnly &&
				Viewport->KeyState(KEY_RightMouseButton)))) &&
			 ( Key == KEY_W || Key == KEY_S || Key == KEY_A || Key == KEY_D ||
			   Key == KEY_E || Key == KEY_Q || Key == KEY_Z || Key == KEY_C ) )
	{
		// Flight camera control is active, so simply absorb the key.  The camera will update based
		// on currently pressed keys (Viewport->KeyState) in the Tick function.

		// (Don't remove this condition!)
		//mark "externally moved" so context menu doesn't come up
		MouseDeltaTracker->SetExternalMovement();
	}
	else if( Event == IE_Pressed || Event == IE_Repeat )
	{
		UBOOL bRedrawAllViewports = FALSE;

		// BOOKMARKS

		TCHAR ch = 0;
		if( Key == KEY_Zero )		ch = '0';
		else if( Key == KEY_One )	ch = '1';
		else if( Key == KEY_Two )	ch = '2';
		else if( Key == KEY_Three )	ch = '3';
		else if( Key == KEY_Four )	ch = '4';
		else if( Key == KEY_Five )	ch = '5';
		else if( Key == KEY_Six )	ch = '6';
		else if( Key == KEY_Seven )	ch = '7';
		else if( Key == KEY_Eight )	ch = '8';
		else if( Key == KEY_Nine )	ch = '9';

		if( (ch >= '0' && ch <= '9') && !AltDown && !ShiftDown )
		{
			// Bookmarks.
			const INT BookmarkIndex = ch - '0';

			// CTRL+# will set a bookmark and record level visibility
			// # will jump to a bookmark without changing level visibility from its current state
			// double-tapping # will jump to a bookmark and restore level visibility
			if( ControlDown )
			{
				GEditorModeTools().SetBookmark( BookmarkIndex, this );
			}
			else
			{
				static FLOAT LastPressTime = 0.0f;
				static INT LastBookmarkIndex = -1;
				UBOOL DoubleTap = FALSE;
				FLOAT Time = appSeconds();
				static const FLOAT DoubleTapInterval = 0.6f;

				// check for double-tapping
				if( BookmarkIndex == LastBookmarkIndex )
				{
					FLOAT TimeElapsed = Time - LastPressTime;
					if( TimeElapsed < DoubleTapInterval )
					{
						DoubleTap = TRUE;
					}
				}

				GEditorModeTools().JumpToBookmark( BookmarkIndex, DoubleTap );

				LastPressTime = Time;
				LastBookmarkIndex = BookmarkIndex;
			}

			bRedrawAllViewports = TRUE;
		}

		// Change grid size

		if( Key == KEY_LeftBracket )
		{
			if(ShiftDown)
			{
				GUnrealEd->Constraints.RotGridSizeDecrement();
			}
			else
			{
				GEditor->Constraints.SetGridSz( GEditor->Constraints.CurrentGridSz - 1 );
			}
			bRedrawAllViewports = TRUE;
		}
		if( Key == KEY_RightBracket )
		{
			if(ShiftDown)
			{
				GUnrealEd->Constraints.RotGridSizeIncrement();
			}
			else
			{
				GEditor->Constraints.SetGridSz( GEditor->Constraints.CurrentGridSz + 1 );
			}
			bRedrawAllViewports = TRUE;
		}

		// Change editor modes

		if( ShiftDown )
		{
			if( Key == KEY_One )
			{
				GEditor->Exec(TEXT("MODE CAMERAMOVE"));
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Two )
			{
				GEditor->Exec(TEXT("MODE GEOMETRY"));
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Three )
			{
				GEditor->Exec(TEXT("MODE TERRAINEDIT"));
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Four )
			{
				GEditor->Exec(TEXT("MODE TEXTURE"));
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Five )
			{
				GEditor->Exec(TEXT("MODE COVEREDIT"));
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Six )
			{
				GEditor->Exec(TEXT("MODE MESHPAINT"));
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Seven )
			{
				GEditor->Exec(TEXT("MODE STATICMESH"));
				bRedrawAllViewports = TRUE;
			}
		}

		// Change viewport mode

		if( AltDown )
		{
			if( Key == KEY_One )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_BrushWireframe;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Two )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_Wireframe;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Three )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_Unlit;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Four )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_Lit;
				bOverrideDiffuseAndSpecular = FALSE;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Five )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_Lit;
				bOverrideDiffuseAndSpecular = TRUE;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Six )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_LightingOnly;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Seven )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_LightComplexity;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Eight )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_ShaderComplexity;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Nine )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_TextureDensity;
				bRedrawAllViewports = TRUE;
			}
			else if( Key == KEY_Zero )
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_LightMapDensity;
				bRedrawAllViewports = TRUE;
			}
			else if (Key == KEY_Underscore)
			{
				ShowFlags &= ~SHOW_ViewMode_Mask;
				ShowFlags |= SHOW_ViewMode_LitLightmapDensity;
				bRedrawAllViewports = TRUE;
			}
			// Switch to perspective viewport type
			else if ( Key == KEY_F )
			{
				SetViewportType( LVT_Perspective );
			}
			// Switch to "top" viewport type
			else if ( Key == KEY_G )
			{
				SetViewportType( LVT_OrthoXY );
			}
			// Switch to "front" viewport type
			else if ( Key == KEY_H )
			{
				SetViewportType( LVT_OrthoYZ );
			}
			// Switch to "side" viewport type
			else if ( Key == KEY_J )
			{
				SetViewportType( LVT_OrthoXZ );
			}

			// Toggle maximize for the current viewport (Alt + X)
			else if ( Key == KEY_X )
			{
				if ( GApp->EditorFrame->ViewportConfigData )
				{
					GApp->EditorFrame->ViewportConfigData->ToggleMaximize( this->Viewport );
					Invalidate();
				}
			}

			if( bRedrawAllViewports )
			{
				GCallbackEvent->Send( CALLBACK_UpdateUI );
			}
		}

		// Try to see if this input can be handled by showflags key bindings, if not,
		// handle other special input cases.
		const UBOOL bInputHandled = CheckForShowFlagInput(Key, ControlDown, AltDown, ShiftDown);

		if(bInputHandled == TRUE)
		{
			bRedrawAllViewports = TRUE;
		}
		else
		{
			if ( Key == KEY_L && ControlDown && !AltDown && !ShiftDown )
			{
				// CTRL+L selects all actors belonging to the layers of the currently selected actors.

				// Iterate over selected actors and make a list of all layers the selected actors belong to.
				TArray<FString> SelectedLayers;
				for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
				{
					AActor* Actor = static_cast<AActor*>( *It );
					checkSlow( Actor->IsA(AActor::StaticClass()) );

					// Parse the actor's layers into an array.
					TArray<FString> NewLayers;
					Actor->Layer.ToString().ParseIntoArray( &NewLayers, TEXT(","), 0 );

					// Add them to the list of selected layers.
					for( INT NewLayerIndex = 0 ; NewLayerIndex < NewLayers.Num() ; ++NewLayerIndex )
					{
						SelectedLayers.AddUniqueItem( NewLayers(NewLayerIndex) );
					}
				}

				if ( SelectedLayers.Num() > 0 )
				{
					const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("SelectActorsByLayer")) );
					GEditor->SelectNone( FALSE, FALSE );
					for ( FActorIterator It ; It ; ++It )
					{
						AActor* Actor = *It;

						// Take the actor's layer string and break it up into an array.
						const FString LayerNames = Actor->Layer.ToString();

						TArray<FString> LayerList;
						LayerNames.ParseIntoArray( &LayerList, TEXT(","), FALSE );

						// Iterate over the array of layer names searching for the input layer.
						UBOOL bShouldSelect = FALSE;
						for( INT LayerIndex = 0 ; LayerIndex < LayerList.Num() && !bShouldSelect ; ++LayerIndex )
						{
							const FString& ActorLayer = LayerList(LayerIndex);
							for ( INT SelectedLayerIndex = 0 ; SelectedLayerIndex < SelectedLayers.Num() ; ++SelectedLayerIndex )
							{
								const FString& SelectedLayer = SelectedLayers(SelectedLayerIndex);
								if ( SelectedLayer == ActorLayer )
								{
									bShouldSelect = TRUE;
									break;
								}
							}
						}

						if ( bShouldSelect )
						{
							GEditor->SelectActor( Actor, TRUE, NULL, FALSE );
						}
					}
					bRedrawAllViewports = TRUE;
					GEditor->NoteSelectionChange();
				}
				
			}
			else if( ControlDown && Key == KEY_R )
			{
				SetRealtime( !IsRealtime() );
				GCallbackEvent->Send( CALLBACK_UpdateUI );
				bRedrawAllViewports = TRUE;
			}
			else if( ControlDown && ShiftDown && Key == KEY_H )
			{
				// Toggle FPS display for real-time viewports
				SetShowFPS( !ShouldShowFPS() );

				// Also make sure that real-time mode is turned on
				if( ShouldShowFPS() )
				{
					SetRealtime( ShouldShowFPS() );
					bRedrawAllViewports = TRUE;
				}
			}
			else if( ControlDown && AltDown && Key == KEY_U )
			{
				// Toggle BSP auto-updating
				GEditorModeTools().SetBSPAutoUpdate(!GEditorModeTools().GetBSPAutoUpdate());
			}
			else if( Key == KEY_H )
			{
				if( ShiftDown )
				{
					GUnrealEd->Exec(TEXT("ACTOR HIDE UNSELECTED"));
				}
				else if( ControlDown )
				{
					GUnrealEd->Exec(TEXT("ACTOR UNHIDE ALL"));
				}
				else if( !AltDown ) // alt+h toggles front viewport
				{
					GUnrealEd->Exec(TEXT("ACTOR HIDE SELECTED"));
				}
			}
			else if( ShiftDown && Key == KEY_L )
			{
				// Toggle stats display for real-time viewports
				SetShowStats( !ShouldShowStats() );

				// Also make sure that real-time mode is turned on
				if( ShouldShowStats() )
				{
					SetRealtime( ShouldShowStats() );
					bRedrawAllViewports = TRUE;
				}
			}
			else if( Key == KEY_V )
			{
				if( ShiftDown )
				{
					EShowFlags SaveShowFlags = ShowFlags;

					// Turn off all collision flags
					ShowFlags &= ~SHOW_Collision_Any;

					if( (SaveShowFlags&SHOW_Collision_Any) == 0 )
					{
						ShowFlags |= SHOW_CollisionZeroExtent;
					}
					else if( SaveShowFlags&SHOW_CollisionZeroExtent )
					{
						ShowFlags |= SHOW_CollisionNonZeroExtent;
					}
					else if( SaveShowFlags&SHOW_CollisionNonZeroExtent )
					{
						ShowFlags |= SHOW_CollisionRigidBody;
					}

					bRedrawAllViewports = TRUE;
				}
			}
			else if( Key == KEY_F8 )
			{
				// Play in editor/ play in viewport
				WxEditorFrame* Frame = static_cast<WxEditorFrame*>(GetEditorFrame());

				FVector* StartLocation = NULL;
				FRotator* StartRotation = NULL;
				FVector PerspectiveStartLocation;
				FRotator PerspectiveStartRotation;

				INT MyViewportIndex = -1;
				if( AltDown )
				{ // Play in viewport
					// Figure out which viewport index we are
					for( INT CurViewportIndex = 0; CurViewportIndex < Frame->ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
					{
						FVCD_Viewport& CurViewport = Frame->ViewportConfigData->AccessViewport( CurViewportIndex );
						if( CurViewport.bEnabled && CurViewport.ViewportWindow == GCurrentLevelEditingViewportClient)
						{
							// If this is a perspective viewport, then we'll Play From Here
							if( CurViewport.ViewportWindow->ViewportType == LVT_Perspective )
							{
								// Start PIE from the camera's location and orientation!
								PerspectiveStartLocation = CurViewport.ViewportWindow->ViewLocation;
								PerspectiveStartRotation = CurViewport.ViewportWindow->ViewRotation;
								StartLocation = &PerspectiveStartLocation;
								StartRotation = &PerspectiveStartRotation;
							}
							MyViewportIndex = CurViewportIndex;
							break;
						}
					}
				}

				// Else, if supported, cue up a regular PIE session
				if( IsPlayInViewportAllowed() )
				{
					// Queue a PIE session
					GUnrealEd->PlayMap( StartLocation, StartRotation, -1, MyViewportIndex );
				}
			}
		}

		if( bRedrawAllViewports )
		{
			GEditor->RedrawLevelEditingViewports();
		}
	}

	return TRUE;
}

/**
 * Checks to see if the current input event modified any show flags.
 * @param Key				Key that was pressed.
 * @param bControlDown		Flag for whether or not the control key is held down.
 * @param bAltDown			Flag for whether or not the alt key is held down.
 * @param bShiftDown		Flag for whether or not the shift key is held down.
 * @return					Flag for whether or not we handled the input.
 */
UBOOL FEditorLevelViewportClient::CheckForShowFlagInput(FName Key, UBOOL bControlDown, UBOOL bAltDown, UBOOL bShiftDown)
{
	// We handle input only in the cases where alt&shift are held down, or no modifiers are held down.
	// If the user holds down ALT-SHIFT, and presses a button, then instead of toggling that showflag,
	// we toggle everything BUT that showflag.

	UBOOL bSpecialMode = FALSE;

	if( bControlDown == TRUE || bShiftDown == TRUE )
	{
		return FALSE;
	}	
	
	// Toggle mode is the way that the flags will be set.  
	enum EToggleMode 
	{
		TM_Toggle,			// Just toggles the flag.
		TM_ToggleView,		// Toggles the view flag.  This is necessary to toggle flags that are part of SHOW_ViewMode_Mask.
		TM_SetOffOnly,		// Turns the flag off only.
		TM_Set				// Sets only the flags specified ON, all others off.
	};

	// Show flags mapping table.  This table maps key inputs to various show flag toggles.
	struct FShowFlagKeyPair
	{
		EShowFlags Flags;		// The flag to toggle
		FName Key;				// The key which does the toggling
		UBOOL bAllowAlt;		// If alt+key also toggles the flag
		EToggleMode ToggleMode;	// How the flag should be toggled
	};

	// Remember the current viewmode
	EShowFlags CurrentViewFlags = ShowFlags & SHOW_ViewMode_Mask;
	const EShowFlags DefaultGameNoView = SHOW_DefaultGame & ~SHOW_ViewMode_Mask;

	// Operate in a viewmode independent fashion
	ShowFlags &= ~SHOW_ViewMode_Mask;
	EShowFlags ToggleFlags = ShowFlags;
	EShowFlags ToggleViewFlags = CurrentViewFlags;

	const FShowFlagKeyPair KeyMappings [] = 
	{
		{SHOW_Volumes,			KEY_O, 	FALSE,	TM_Toggle},
		{SHOW_BSP,				KEY_Q,	TRUE,	TM_Toggle},
		{SHOW_StaticMeshes,		KEY_W,	TRUE,	TM_Toggle},
		{SHOW_Paths,			KEY_P, 	FALSE,	TM_Toggle},
		{SHOW_NavigationNodes,	KEY_N, 	FALSE,	TM_Toggle},
		{SHOW_Terrain,			KEY_T, 	FALSE,	TM_Toggle},
		{SHOW_KismetRefs,		KEY_K, 	FALSE,	TM_Toggle},
		{SHOW_Collision,		KEY_C, 	TRUE,	TM_Toggle},
		{SHOW_Decals,			KEY_E, 	TRUE,	TM_Toggle},
		{SHOW_Fog,				KEY_F, 	FALSE,	TM_Toggle},
		{SHOW_LightRadius,		KEY_R, 	FALSE,	TM_Toggle},
		{SHOW_LevelColoration,  KEY_I,	FALSE,	TM_Toggle},
		// Toggling 'G' mode does not touch view flags.
		{DefaultGameNoView,		KEY_G, 	FALSE,	TM_Set },
		{0} // Must be the last element in the array.
	};

	// Loop through the KeyMappings array and see if the key passed in matches.
	UBOOL bFoundKey = FALSE;
	const FShowFlagKeyPair* Pair = KeyMappings;

	while(Pair->Flags != 0)
	{
		// Allow the key binding to pass if the corresponding key was pressed and alt is not down or we allow alt+key to toggle the flag
		if( Pair->Key == Key && ( !bAltDown || ( Pair->bAllowAlt && bAltDown ) ) )
		{
			switch(Pair->ToggleMode)
			{
			case TM_Toggle:
				ToggleFlags ^= Pair->Flags;
				break;
			case TM_ToggleView:
				ToggleViewFlags ^= Pair->Flags;
				break;
			case TM_Set:
				bSpecialMode = TRUE;
				ToggleFlags = Pair->Flags;
			break;
			case TM_SetOffOnly:
				if((ToggleFlags & Pair->Flags) == 0)
				{
					ToggleFlags |= Pair->Flags;
				}
				else
				{
					ToggleFlags &= ~(Pair->Flags);
				}
				
				break;
			default:
				break;
			}

			bFoundKey = TRUE;
			break;
		}

		Pair++;
	}

	// The builder brush case needs to be handled a bit differently because it sets its value to all viewports.
	if( Key == KEY_B && !bAltDown )
	{
		// Use the builder brush show status in this viewport to determine what the new state should be for all viewports.
		const UBOOL bNewBuilderBrushState = !(ShowFlags & SHOW_BuilderBrush ? TRUE : FALSE);
		for( INT ViewportIndex = 0 ; ViewportIndex < GEditor->ViewportClients.Num() ; ++ViewportIndex )
		{
			FEditorLevelViewportClient* ViewportClient = GEditor->ViewportClients( ViewportIndex );
			const UBOOL bCurBuilderBrushState = ViewportClient->ShowFlags & SHOW_BuilderBrush ? TRUE : FALSE;
			if ( bCurBuilderBrushState != bNewBuilderBrushState )
			{
				ViewportClient->ShowFlags ^= SHOW_BuilderBrush;
			}
		}
		
		bFoundKey = TRUE;
	}
	else if(bFoundKey == TRUE)
	{
		// If this is a special mode and is the name special mode as the last special mode that was set, it means that the user is toggling the special mode
		// so reset the showflags to whatever they were before the user entered the special mode.
		if(bSpecialMode && bInGameViewMode)
		{
			ShowFlags = LastShowFlags;
			bInGameViewMode = FALSE;
		}
		else
		{
			// If the user is changing the current showflags, then check to see if this is a special mode,
			// if it is and we are not already in a existing special mode, then store the current show flag state before changing it.
			if(ToggleFlags != ShowFlags
				|| (ToggleViewFlags != CurrentViewFlags))
			{
				if(bSpecialMode == TRUE)
				{
					if(!bInGameViewMode)
					{
						LastShowFlags = ShowFlags;
						bInGameViewMode = TRUE;
					}
				}

				ShowFlags = ToggleFlags;
			}
			else
			{
				ShowFlags = LastShowFlags;
			}
		}
	}

	// Reset the current viewmode
	ShowFlags |= ToggleViewFlags;
	return bFoundKey;
}



/**
 * Forcibly disables lighting show flags if there are no lights in the scene, or restores lighting show
 * flags if lights are added to the scene.
 */
void FEditorLevelViewportClient::UpdateLightingShowFlags()
{
	UBOOL bViewportNeedsRefresh = FALSE;

	// We'll only use default lighting for viewports that are viewing the main world
	if( GetScene() != NULL && GetScene()->GetWorld() != NULL && GetScene()->GetWorld() == GWorld )
	{
		// Make sure that we're not currently in a special (or temporary) editor view mode.  We won't mess
		// with those show flags.
		if( !bInGameViewMode && CachedShowFlags == 0 )
		{
			// Check to see if there are any lights in the scene
			UBOOL bAnyLights = FALSE;
			{
				UWorld* World = GetScene()->GetWorld();
				if( World->StaticLightList.Num() > 0 ||
					World->DynamicLightList.Num() > 0 ||
					World->DominantDirectionalLight != NULL ||
					World->DominantSpotLights.Num() > 0 ||
					World->DominantPointLights.Num() > 0 )
				{
					bAnyLights = TRUE;
				}
			}

			if( bAnyLights )
			{
				// Did we previously force unlit mode on because there were no lights in the scene?
				if( bForcedUnlitShowFlags )
				{
					// Is unlit mode currently enabled?  We'll make sure that all of the regular unlit view
					// mode show flags are set (not just SHOW_Lighting), so we don't disrupt other view modes
					if( SHOW_ViewMode_Unlit == ( ShowFlags & SHOW_ViewMode_Unlit ))
					{
						// We have lights in the scene now so go ahead and turn lighting back on
						const EShowFlags ShowFlagsWithLighting = ( ShowFlags | SHOW_Lighting );
						ShowFlags = ShowFlagsWithLighting;

						// Make sure the viewport gets refreshed
						bViewportNeedsRefresh = TRUE;
					}
			
					// No longer forcing lighting to be off
					bForcedUnlitShowFlags = FALSE;
				}
			}
			else
			{
				// Make sure we haven't already forced lighting off.  If the user goes ahead and toggles
				// lighting back on manually we want to allow that.
				if( !bForcedUnlitShowFlags )
				{
					// Is lighting currently enabled?
					if( ShowFlags & SHOW_Lighting )
					{
						// No lights in the scene, so make sure that lighting is turned off so the level
						// designer can see what they're interacting with!
						const EShowFlags ShowFlagsWithoutLighting = ((ShowFlags | SHOW_ViewMode_Unlit) & ~SHOW_Lighting);
						ShowFlags = ShowFlagsWithoutLighting;

						// Make sure the viewport gets refreshed
						bViewportNeedsRefresh = TRUE;

						// Take note that we forced unlit mode
						bForcedUnlitShowFlags = TRUE;
					}
				}
			}
		}
	}

	// Invalidate the viewport if needed
	if( bViewportNeedsRefresh )
	{
		const UBOOL bInvalidateChildViews = FALSE;
		const UBOOL bInvalidateHitProxies = FALSE;
		Invalidate( bInvalidateChildViews, bInvalidateHitProxies );
	}
}





/**
 * Returns the horizontal axis for this viewport.
 */

EAxis FEditorLevelViewportClient::GetHorizAxis() const
{
	switch( ViewportType )
	{
		case LVT_OrthoXY:
			return AXIS_Y;
		case LVT_OrthoXZ:
			return AXIS_X;
		case LVT_OrthoYZ:
			return AXIS_Y;
	}

	return AXIS_X;
}

/**
 * Returns the vertical axis for this viewport.
 */

EAxis FEditorLevelViewportClient::GetVertAxis() const
{
	switch( ViewportType )
	{
		case LVT_OrthoXY:
			return AXIS_X;
		case LVT_OrthoXZ:
			return AXIS_Z;
		case LVT_OrthoYZ:
			return AXIS_Z;
	}

	return AXIS_Y;
}

//
//	FEditorLevelViewportClient::InputAxis
//

UBOOL FEditorLevelViewportClient::InputAxis(FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad)
{
	if (bDisableInput)
	{
		return TRUE;
	}

	if( GCurrentLevelEditingViewportClient != this )
	{
		//GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
		GCurrentLevelEditingViewportClient = this;
	}

	/**Save off axis commands for future camera work*/
	FCachedJoystickState* JoystickState = JoystickStateMap.FindRef(ControllerId);
	if (JoystickState)
	{
		JoystickState->AxisDeltaValues.Set(Key, Delta);
	}

	// Let the current mode have a look at the input before reacting to it.
	if ( GEditorModeTools().InputAxis(this, Viewport, ControllerId, Key, Delta, DeltaTime) )
	{
		return TRUE;
	}

	// Let the engine try to handle the input first.
	if( Input->InputAxis(ControllerId,Key,Delta,DeltaTime,bGamepad) )
	{
		return TRUE;
	}

	if( bIsTracking	)
	{
		// Accumulate and snap the mouse movement since the last mouse button click.
		MouseDeltaTracker->AddDelta( this, Key, Delta, 0 );
	}

	// If we are using a drag tool, paint the viewport so we can see it update.
	if( MouseDeltaTracker->UsingDragTool() )
	{
		Invalidate( FALSE, FALSE );
	}
	
	return TRUE;
}

/**
* Returns TRUE if this viewport is a realtime viewport.
* @returns TRUE if realtime, FALSE otherwise.
*/
UBOOL FEditorLevelViewportClient::GetIsRealtime() const
{
	return bIsRealtime;
}

/**
 * Sets whether or not a controller is actively plugged in
 * @param InControllID - Unique ID of the joystick
 * @param bInConnected - TRUE, if the joystick is valid for input
 */
void FEditorLevelViewportClient::OnJoystickPlugged(const UINT InControllerID, const UINT InType, const UINT bInConnected)
{
	FCachedJoystickState* CurrentState = JoystickStateMap.FindRef(InControllerID);
	//joystick is now disabled, delete if needed
	if (!bInConnected)
	{
		JoystickStateMap.RemoveKey(InControllerID);
		delete CurrentState;
	}
	else
	{
		if (CurrentState == NULL)
		{
			/** Create new joystick state for cached input*/
			CurrentState = new FCachedJoystickState();
			CurrentState->JoystickType = InType;
			JoystickStateMap.Set(InControllerID, CurrentState);
		}
	}
}


void FEditorLevelViewportClient::InputAxisMayaCam(FViewport* Viewport, const FVector& DragDelta, FVector& Drag, FRotator& Rot)
{
	FRotator tempRot = ViewRotation;
	ViewRotation = FRotator(0,16384,0);	
	ConvertMovementToDragRotMayaCam(DragDelta, Drag, Rot);
	ViewRotation = tempRot;
	Drag.X = DragDelta.X;

	UBOOL	LeftMouseButton = Viewport->KeyState(KEY_LeftMouseButton),
		MiddleMouseButton = Viewport->KeyState(KEY_MiddleMouseButton),
		RightMouseButton = Viewport->KeyState(KEY_RightMouseButton);

	if ( LeftMouseButton )
	{
		MayaRotation += FRotator( Rot.Pitch, -Rot.Yaw, Rot.Roll );
	}
	else if ( MiddleMouseButton )
	{
		FVector DeltaLocation = FVector(Drag.X, 0, -Drag.Z) * CAMERA_MAYAPAN_SPEED;

		FMatrix rotMat =
			FTranslationMatrix( -MayaLookAt ) *
			FRotationMatrix( FRotator(0,MayaRotation.Yaw,0) ) * 
			FRotationMatrix( FRotator(0, 0, MayaRotation.Pitch));

		MayaLookAt = MayaLookAt + rotMat.Inverse().TransformNormal(DeltaLocation);
	}
	else if ( RightMouseButton )
	{
		MayaZoom.Y += -Drag.Y;
	}
}

/**
 * Implements screenshot capture for editor viewports.  Should be called by derived class' InputKey.
 */
void FEditorLevelViewportClient::InputTakeScreenshot(FViewport* Viewport, FName Key, EInputEvent Event)
{
	const UBOOL F9Down = Viewport->KeyState(KEY_F9);
	if ( F9Down )
	{
		if ( Key == KEY_LeftMouseButton )
		{
			if( Event == IE_Pressed )
			{
				// We need to invalidate the viewport in order to generate the correct pixel buffer for picking.
				Invalidate( FALSE, TRUE );
			}
			else if( Event == IE_Released )
			{
				// Redraw the viewport so we don't end up with clobbered data from other viewports using the same frame buffer.
				Viewport->Draw();
				// Read the contents of the viewport into an array.
				TArray<FColor> Bitmap;
				if( Viewport->ReadPixels(Bitmap) )
				{
					check(Bitmap.Num() == Viewport->GetSizeX() * Viewport->GetSizeY());

					// Create screenshot folder if not already present.
					GFileManager->MakeDirectory( *appScreenShotDir(), TRUE );

					// Save the contents of the array to a bitmap file.
					appCreateBitmap(*(appScreenShotDir() * TEXT("ScreenShot")),Viewport->GetSizeX(),Viewport->GetSizeY(),&Bitmap(0),GFileManager);
				}
			}
		}
	}
}


/**
 * Invalidates this viewport and optionally child views.
 *
 * @param	bInvalidateChildViews		[opt] If TRUE (the default), invalidate views that see this viewport as their parent.
 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
 */
void FEditorLevelViewportClient::Invalidate(UBOOL bInvalidateChildViews, UBOOL bInvalidateHitProxies)
{
	if ( Viewport )
	{
		if ( bInvalidateHitProxies )
		{
			// Invalidate hit proxies and display pixels.
			Viewport->Invalidate();
		}
		else
		{
			// Invalidate only display pixels.
			Viewport->InvalidateDisplay();
		}

		// If this viewport is a view parent . . .
		if ( bInvalidateChildViews &&
			ViewState->IsViewParent() )
		{
			GEditor->InvalidateChildViewports( ViewState, bInvalidateHitProxies );	
		}

		// If this viewport is a perspective, ensure it is in the observer list
		if(ViewportType == LVT_Perspective)
		{
			AddObserver();
		}
		else
		{
			RemoveObserver();
		}
		GCallbackEvent->Send( CALLBACK_ViewportClientInvalidated );
	}
}

/**
 * Determines if InComponent is inside of InSelBBox.  This check differs depending on the type of component.
 * If InComponent is NULL, FALSE is returned.
 *
 * @param	InActor							Used only when testing sprite components.
 * @param	InComponent						The component to query.  If NULL, FALSE is returned.
 * @param	InSelBox						The selection box.
 * @param	bConsiderOnlyBSP				If TRUE, consider only BSP.
 * @param	bMustEncompassEntireComponent	If TRUE, the entire component must be encompassed by the selection box in order to return TRUE.
 */
UBOOL FEditorLevelViewportClient::ComponentIsTouchingSelectionBox(AActor* InActor, UPrimitiveComponent* InComponent, const FBox& InSelBBox, UBOOL bConsiderOnlyBSP, UBOOL bMustEncompassEntireComponent)
{
	UBOOL bResult = FALSE;
	UBOOL bAlreadyProcessed = FALSE;

	if( ( ShowFlags & ( SHOW_Volumes|SHOW_BSP ) ) && InComponent->IsA( UBrushComponent::StaticClass() ) )
	{
		if ( InActor->IsAVolume() )
		{
			// Don't select if the brush is a volume and the volume show flag is unset.
			if ( !(ShowFlags & SHOW_Volumes) )
			{
				bResult = FALSE;
				bAlreadyProcessed = TRUE;
			}
		}
		else
		{
			// Don't select if the brush is regular bsp and the bsp show flag is unset.
			if ( !(ShowFlags & SHOW_BSP) )
			{
				bResult = FALSE;
				bAlreadyProcessed = TRUE;
			}
		}

		const UBrushComponent* BrushComponent = static_cast<UBrushComponent*>( InComponent );
		
		// Check the brush component
		if( BrushComponent->Brush && BrushComponent->Brush->Polys && !bAlreadyProcessed )
		{
			for( INT PolyIndex = 0 ; PolyIndex < BrushComponent->Brush->Polys->Element.Num() && !bAlreadyProcessed ; ++PolyIndex )
			{
				const FPoly& Poly = BrushComponent->Brush->Polys->Element( PolyIndex );

				for( INT VertexIndex = 0 ; VertexIndex < Poly.Vertices.Num() ; ++VertexIndex )
				{
					const FVector Location = InComponent->LocalToWorld.TransformFVector( Poly.Vertices(VertexIndex) );
					const UBOOL bLocationIntersected = FPointBoxIntersection( Location, InSelBBox );
					
					// If the selection box doesn't have to encompass the entire component and a poly vertex has intersected with
					// the selection box, this component is being touched by the selection box
					if ( !bMustEncompassEntireComponent && bLocationIntersected )
					{
						bResult = TRUE;
						bAlreadyProcessed = TRUE;
						break;
					}

					// If the selection box has to encompass the entire component and a poly vertex didn't intersect with the selection
					// box, this component does not qualify
					else if ( bMustEncompassEntireComponent && !bLocationIntersected )
					{
						bResult = FALSE;
						bAlreadyProcessed = TRUE;
						break;
					}
				}
			}

			// If the selection box has to encompass all of the component and none of the component's verts failed the intersection test, this component
			// is consider touching
			if ( bMustEncompassEntireComponent && !bAlreadyProcessed )
			{
				bResult = TRUE;
				bAlreadyProcessed = TRUE;
			}
		}
	}
	else if( !bConsiderOnlyBSP && ( ShowFlags & SHOW_StaticMeshes ) && InComponent->IsA( UStaticMeshComponent::StaticClass() ) )
	{
		const UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>( InComponent );

		if( StaticMeshComponent->StaticMesh )
		{
			check( StaticMeshComponent->StaticMesh->LODModels.Num() > 0 );
			const FStaticMeshRenderData& LODModel = StaticMeshComponent->StaticMesh->LODModels(0);
			for( UINT VertexIndex = 0 ; VertexIndex < LODModel.NumVertices ; ++VertexIndex )
			{
				const FVector& SrcPosition = LODModel.PositionVertexBuffer.VertexPosition( VertexIndex );
				const FVector Location = StaticMeshComponent->LocalToWorld.TransformFVector( SrcPosition );
				const UBOOL bLocationIntersected = FPointBoxIntersection( Location, InSelBBox );

				// If the selection box doesn't have to encompass the entire component and a static mesh vertex has intersected with
				// the selection box, this component is being touched by the selection box
				if( !bMustEncompassEntireComponent && bLocationIntersected )
				{
					bResult = TRUE;
					bAlreadyProcessed = TRUE;
					break;
				}

				// If the selection box has to encompass the entire component and a static mesh vertex didn't intersect with the selection
				// box, this component does not qualify
				else if ( bMustEncompassEntireComponent && !bLocationIntersected )
				{
					bResult = FALSE;
					bAlreadyProcessed = TRUE;
					break;
				}
			}

			// If the selection box has to encompass all of the component and none of the component's verts failed the intersection test, this component
			// is consider touching
			if ( bMustEncompassEntireComponent && !bAlreadyProcessed )
			{
				bResult = TRUE;
				bAlreadyProcessed = TRUE;
			}
		}
	}
	else if( !bConsiderOnlyBSP && (ShowFlags & SHOW_Sprites) && InComponent->IsA(USpriteComponent::StaticClass()))
	{
		const USpriteComponent*	SpriteComponent = static_cast<USpriteComponent*>( InComponent );
		
		// Construct a box representing the sprite
		FBox SpriteBox(
			InActor->Location - InActor->DrawScale * SpriteComponent->Scale * Max(SpriteComponent->Sprite->SizeX,SpriteComponent->Sprite->SizeY) * FVector(1,1,1),
			InActor->Location + InActor->DrawScale * SpriteComponent->Scale * Max(SpriteComponent->Sprite->SizeX,SpriteComponent->Sprite->SizeY) * FVector(1,1,1) );

		// If the selection box doesn't have to encompass the entire component and it intersects with the box constructed for the sprite, then it is valid.
		// Additionally, if the selection box does have to encompass the entire component and both the min and max vectors of the sprite box are inside the selection box,
		// then it is valid.
		if (	( !bMustEncompassEntireComponent && InSelBBox.Intersect( SpriteBox ) ) 
			||	( bMustEncompassEntireComponent && InSelBBox.IsInside( SpriteBox.Min ) && InSelBBox.IsInside( SpriteBox.Max ) ) )
		{
			// Disallow selection based on sprites of nav nodes that aren't being drawn because of SHOW_NavigationNodes.
			const UBOOL bIsHiddenNavNode = ( !( ShowFlags&SHOW_NavigationNodes ) && InActor->IsA( ANavigationPoint::StaticClass() ) );
			if ( !bIsHiddenNavNode )
			{
				bResult = TRUE;
				bAlreadyProcessed = TRUE;
			}
		}
	}
	else if( !bConsiderOnlyBSP && (ShowFlags & SHOW_SkeletalMeshes) && InComponent->IsA( USkeletalMeshComponent::StaticClass() ) )
	{
		const USkeletalMeshComponent* SkeletalMeshComponent = static_cast<USkeletalMeshComponent*>( InComponent );
		if( SkeletalMeshComponent->SkeletalMesh )
		{
			check(SkeletalMeshComponent->SkeletalMesh->LODModels.Num() > 0);

			// Transform hard and soft verts into world space. Note that this assumes skeletal mesh is in reference pose...
			const FStaticLODModel& LODModel = SkeletalMeshComponent->SkeletalMesh->LODModels(0);
			for( INT ChunkIndex = 0 ; ChunkIndex < LODModel.Chunks.Num() && !bAlreadyProcessed; ++ChunkIndex )
			{
				const FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIndex);
				for( INT i = 0; i < Chunk.RigidVertices.Num(); ++i )
				{
					const FVector Location = SkeletalMeshComponent->LocalToWorld.TransformFVector( Chunk.RigidVertices(i).Position );
					const UBOOL bLocationIntersected = FPointBoxIntersection( Location, InSelBBox );
					
					// If the selection box doesn't have to encompass the entire component and a skeletal mesh vertex has intersected with
					// the selection box, this component is being touched by the selection box
					if( !bMustEncompassEntireComponent && bLocationIntersected )
					{
						bResult = TRUE;
						bAlreadyProcessed = TRUE;
						break;
					}

					// If the selection box has to encompass the entire component and a skeletal mesh vertex didn't intersect with the selection
					// box, this component does not qualify
					else if ( bMustEncompassEntireComponent && !bLocationIntersected )
					{
						bResult = FALSE;
						bAlreadyProcessed = TRUE;
						break;
					}
				}

				for( INT i = 0 ; i < Chunk.SoftVertices.Num() ; ++i )
				{
					const FVector Location = SkeletalMeshComponent->LocalToWorld.TransformFVector( Chunk.SoftVertices(i).Position );
					const UBOOL bLocationIntersected = FPointBoxIntersection( Location, InSelBBox );
					
					// If the selection box doesn't have to encompass the entire component and a skeletal mesh vertex has intersected with
					// the selection box, this component is being touched by the selection box
					if( !bMustEncompassEntireComponent && bLocationIntersected )
					{
						bResult = TRUE;
						bAlreadyProcessed = TRUE;
						break;
					}

					// If the selection box has to encompass the entire component and a skeletal mesh vertex didn't intersect with the selection
					// box, this component does not qualify
					else if ( bMustEncompassEntireComponent && !bLocationIntersected )
					{
						bResult = FALSE;
						bAlreadyProcessed = TRUE;
						break;
					}
				}
			}

			// If the selection box has to encompass all of the component and none of the component's verts failed the intersection test, this component
			// is consider touching
			if ( bMustEncompassEntireComponent && !bAlreadyProcessed )
			{
				bResult = TRUE;
				bAlreadyProcessed = TRUE;
			}
		}
	}
	else if ( !bConsiderOnlyBSP )
	{
		UBOOL bSelectByBoundingBox = FALSE;

		// Determine whether the component may be selected by bounding box.
		if( (ShowFlags & SHOW_SpeedTrees) && InComponent->IsA(USpeedTreeComponent::StaticClass()) )
		{
			bSelectByBoundingBox = TRUE;
		}

		if( bSelectByBoundingBox )
		{
			const FBox& ComponentBounds = InComponent->Bounds.GetBox();
			
			// Check the component bounds versus the selection box
			// If the selection box must encompass the entire component, then both the min and max vector of the bounds must be inside in the selection
			// box to be valid. If the selection box only has to touch the component, then it is sufficient to check if it intersects with the bounds.
			if (   ( !bMustEncompassEntireComponent && InSelBBox.Intersect( ComponentBounds ) ) 
				|| ( bMustEncompassEntireComponent && InSelBBox.IsInside( ComponentBounds.Min ) && InSelBBox.IsInside( ComponentBounds.Max ) ) )
			{
				bResult = TRUE;
				bAlreadyProcessed = TRUE;
			}
		}
	}

	return bResult;
}

static UINT GetVolumeActorVisiblityId( const AActor& InActor )
{
	UClass* Class = InActor.GetClass();

	static TMap<UClass*, UINT > ActorToIdMap;
	if( ActorToIdMap.Num() == 0 )
	{
		// Build a mapping of volume classes to ID's.  Do this only once
		TArray< UClass *> VolumeClasses;
		GApp->EditorFrame->GetSortedVolumeClasses( &VolumeClasses );
		for( INT VolumeIdx = 0; VolumeIdx < VolumeClasses.Num(); ++VolumeIdx )
		{
			// An actors flag is just the index of the actor in the stored volume array shifted left to represent a unique bit.
			ActorToIdMap.Set( VolumeClasses(VolumeIdx), VolumeIdx );
		}
	}

	UINT* ActorID =  ActorToIdMap.Find( Class );

	// return 0 if the actor flag was not found, otherwise return the actual flag.  
	return ActorID ? *ActorID : 0;
}


/** 
 * Returns TRUE if the passed in volume is visible in the viewport (due to volume actor visibility flags)
 *
 * @param Volume	The volume to check
 */
UBOOL FEditorLevelViewportClient::IsVolumeVisibleInViewport( const AActor& VolumeActor ) const
{
	// We pass in the actor class for compatibility but we should make sure 
	// the function is only given volume actors
	check( VolumeActor.IsAVolume() );

	UINT VolumeId = GetVolumeActorVisiblityId( VolumeActor );
	return VolumeActorVisibility( VolumeId );
}

// Determines which axis InKey and InDelta most refer to and returns
// a corresponding FVector.  This vector represents the mouse movement
// translated into the viewports/widgets axis space.
//
// @param InNudge		If 1, this delta is coming from a keyboard nudge and not the mouse

FVector FEditorLevelViewportClient::TranslateDelta( FName InKey, FLOAT InDelta, UBOOL InNudge )
{
	UBOOL AltDown = Input->IsAltPressed(),
		ShiftDown = Input->IsShiftPressed(),
		ControlDown = Input->IsCtrlPressed(),
		LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton),
		RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	FVector vec(0.0f, 0.0f, 0.0f);

	FLOAT X = InKey == KEY_MouseX ? InDelta : 0.f;
	FLOAT Y = InKey == KEY_MouseY ? InDelta : 0.f;

	switch( ViewportType )
	{
		case LVT_OrthoXY:
		case LVT_OrthoXZ:
		case LVT_OrthoYZ:
			{
				if (ShouldUseMoveCanvasMovement())
				{
					const INT SizeX = Viewport->GetSizeX();
					const INT SizeY = Viewport->GetSizeY();
					UBOOL bInsideViewport = IsWithin<INT>(LastMouseX, 1, SizeX-1) && IsWithin<INT>(LastMouseY, 1, SizeY-1);

					LastMouseX += X;
					LastMouseY -= Y;

					if ((X != 0.0f) || (Y!=0.0f))
					{
						MarkMouseMovedSinceClick();
					}

					//only invert x,y if we're moving the camera
					if (Widget->GetCurrentAxis() == AXIS_None)
					{
						X = -X;
						Y = -Y;
					}

					UpdateCursorVisibility();
					UpdateMousePosition();
				}

				FWidget::EWidgetMode WidgetMode = GEditorModeTools().GetWidgetMode();
				UBOOL bIgnoreOrthoScaling = ((WidgetMode == FWidget::WM_Scale) || (WidgetMode == FWidget::WM_ScaleNonUniform)) && (Widget->GetCurrentAxis() != AXIS_None);

				if( InNudge || bIgnoreOrthoScaling )
				{
					vec = FVector( X, Y, 0.f );
				}
				else
				{
					vec = FVector( X * (OrthoZoom/CAMERA_ZOOM_DIV), Y * (OrthoZoom/CAMERA_ZOOM_DIV), 0.f );

					if( Widget->GetCurrentAxis() == AXIS_None )
					{
						switch( ViewportType )
						{
							case LVT_OrthoXY:
								vec.X *= -1;
								break;
							case LVT_OrthoXZ:
								vec = FVector( X * (OrthoZoom/CAMERA_ZOOM_DIV), 0.f, Y * (OrthoZoom/CAMERA_ZOOM_DIV) );
								break;
							case LVT_OrthoYZ:
								vec = FVector( 0.f, X * (OrthoZoom/CAMERA_ZOOM_DIV), Y * (OrthoZoom/CAMERA_ZOOM_DIV) );
								break;
						}
					}
				}
			}
			break;

		case LVT_Perspective:
			vec = FVector( X, Y, 0.f );
			break;

		default:
			check(0);		// Unknown viewport type
			break;
	}

	if( IsOrtho() && (LeftMouseButtonDown && RightMouseButtonDown) && Y != 0.f )
	{
		vec = FVector(0,0,Y);
	}

	return vec;
}

// Converts a generic movement delta into drag/rotation deltas based on the viewport and keys held down

void FEditorLevelViewportClient::ConvertMovementToDragRot(const FVector& InDelta,
														  FVector& InDragDelta,
														  FRotator& InRotDelta)
{
	const UBOOL AltDown = Input->IsAltPressed();
	const UBOOL ShiftDown = Input->IsShiftPressed();
	const UBOOL ControlDown = Input->IsCtrlPressed();
	const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	InDragDelta = FVector(0,0,0);
	InRotDelta = FRotator(0,0,0);

	switch( ViewportType )
	{
		case LVT_OrthoXY:
		case LVT_OrthoXZ:
		case LVT_OrthoYZ:
		{
			if( LeftMouseButtonDown && RightMouseButtonDown )
			{
				// Both mouse buttons change the ortho viewport zoom.
				InDragDelta = FVector(0,0,InDelta.Z);
			}
			else if( RightMouseButtonDown )
			{
				// @todo: set RMB to move opposite to the direction of drag, in other words "grab and pull".
				InDragDelta = InDelta;
			}
			else if( LeftMouseButtonDown )
			{
				// LMB moves in the direction of the drag.
				InDragDelta = InDelta;
			}
		}
		break;

		case LVT_Perspective:
		{
			if( LeftMouseButtonDown && !RightMouseButtonDown )
			{
				// Move forward and yaw

				InDragDelta.X = InDelta.Y * GMath.CosTab( ViewRotation.Yaw );
				InDragDelta.Y = InDelta.Y * GMath.SinTab( ViewRotation.Yaw );

				InRotDelta.Yaw = InDelta.X * CAMERA_ROTATION_SPEED;
			}
			else if( RightMouseButtonDown && LeftMouseButtonDown )
			{
				// Pan left/right/up/down

				InDragDelta.X = InDelta.X * -GMath.SinTab( ViewRotation.Yaw );
				InDragDelta.Y = InDelta.X *  GMath.CosTab( ViewRotation.Yaw );
				InDragDelta.Z = InDelta.Y;
			}
			else if( RightMouseButtonDown && !LeftMouseButtonDown )
			{
				// Change viewing angle

				InRotDelta.Yaw = InDelta.X * CAMERA_ROTATION_SPEED;
				InRotDelta.Pitch = InDelta.Y * CAMERA_ROTATION_SPEED;
			}
		}
		break;

		default:
			check(0);	// unknown viewport type
			break;
	}
}

void FEditorLevelViewportClient::ConvertMovementToDragRotMayaCam(const FVector& InDelta,
																 FVector& InDragDelta,
																 FRotator& InRotDelta)
{
	const UBOOL AltDown = Input->IsAltPressed();
	const UBOOL ShiftDown = Input->IsShiftPressed();
	const UBOOL ControlDown = Input->IsCtrlPressed();
	const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);
	const UBOOL MiddleMouseButtonDown = Viewport->KeyState(KEY_MiddleMouseButton);

	InDragDelta = FVector(0,0,0);
	InRotDelta = FRotator(0,0,0);

	switch( ViewportType )
	{
	case LVT_OrthoXY:
	case LVT_OrthoXZ:
	case LVT_OrthoYZ:
		{
			if( LeftMouseButtonDown && RightMouseButtonDown )
			{
				// Change ortho zoom.
				InDragDelta = FVector(0,0,InDelta.Z);
			}
			else if( RightMouseButtonDown )
			{
				// Move camera.
				InDragDelta = InDelta;
			}
			else if( LeftMouseButtonDown )
			{
				// Move actors.
				InDragDelta = InDelta;
			}
		}
		break;

	case LVT_Perspective:
		{
			if( LeftMouseButtonDown )
			{
				// Change the viewing angle
				InRotDelta.Yaw = InDelta.X * CAMERA_ROTATION_SPEED;
				InRotDelta.Pitch = InDelta.Y * CAMERA_ROTATION_SPEED;
			}
			else if( MiddleMouseButtonDown )
			{
				// Pan left/right/up/down
				InDragDelta.X = InDelta.X * -GMath.SinTab( ViewRotation.Yaw );
				InDragDelta.Y = InDelta.X *  GMath.CosTab( ViewRotation.Yaw );
				InDragDelta.Z = InDelta.Y;
			}
			else if( RightMouseButtonDown )
			{
				// Zoom in and out.
				InDragDelta.X = InDelta.Y * GMath.CosTab( ViewRotation.Yaw );
				InDragDelta.Y = InDelta.Y * GMath.SinTab( ViewRotation.Yaw );
			}
		}
		break;

	default:
		check(0);	// unknown viewport type
		break;
	}
}

/** Returns true if perspective flight camera input mode is currently active in this viewport */
UBOOL FEditorLevelViewportClient::IsFlightCameraInputModeActive() const
{
	if( Viewport != NULL && ViewportType == LVT_Perspective )
	{
		if( CameraController != NULL )
		{
			const UBOOL bControlDown = Viewport->KeyState( KEY_LeftControl ) || Viewport->KeyState( KEY_RightControl );
			const UBOOL bShiftDown = Viewport->KeyState( KEY_LeftShift ) || Viewport->KeyState( KEY_RightShift );
			const UBOOL bAltDown = Viewport->KeyState( KEY_LeftAlt ) || Viewport->KeyState( KEY_RightAlt );
			const UBOOL bLeftMouseButtonDown = Viewport->KeyState( KEY_LeftMouseButton );
			const UBOOL bRightMouseButtonDown = Viewport->KeyState( KEY_RightMouseButton );

			const UBOOL bIsMouseLooking =
				Widget->GetCurrentAxis() == AXIS_None &&
				bRightMouseButtonDown &&
				!bLeftMouseButtonDown &&
				!bControlDown && !bShiftDown && !bAltDown;

			return bIsMouseLooking;
		}
	}

	return FALSE;
}



/** Moves a perspective camera */
void FEditorLevelViewportClient::MoveViewportPerspectiveCamera( const FVector& InDrag, const FRotator& InRot, UBOOL bUnlitMovement )
{
	check( ViewportType == LVT_Perspective );

	// Update camera Rotation
	ViewRotation += FRotator( InRot.Pitch, InRot.Yaw, InRot.Roll );
	
	// If rotation is pitching down by using a big positive number, change it so its using a smaller negative number
	if((ViewRotation.Pitch > 49151) && (ViewRotation.Pitch <= 65535))
	{
		ViewRotation.Pitch = ViewRotation.Pitch - 65535;
	}

	// Make sure its withing  +/- 90 degrees.
	ViewRotation.Pitch = Clamp( ViewRotation.Pitch, -16384, 16384 );

	// Update camera Location
	ViewLocation.AddBounded( InDrag, HALF_WORLD_MAX1 );

	// Tell the editing mode that the camera moved, in case its interested.
	FEdMode* Mode = GEditorModeTools().GetActiveMode(EM_InterpEdit);
	if( Mode )
	{
		((FEdModeInterpEdit*)Mode)->CamMoveNotify(this);
	}

	// If turned on, move any selected actors to the cameras location/rotation
	if( bLockSelectedToCamera )
	{
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if ( !Actor->bLockLocation )
			{
				Actor->SetLocation( GCurrentLevelEditingViewportClient->ViewLocation );
			}
			if( Actor->IsBrush() )
			{
				FBSPOps::RotateBrushVerts( (ABrush*)Actor, GCurrentLevelEditingViewportClient->ViewRotation, TRUE );
			}
			else
			{
				Actor->SetRotation( GCurrentLevelEditingViewportClient->ViewRotation );
			}
		}
	}

	if ( bUnlitMovement && bMoveUnlit && !CachedShowFlags )
	{
		CachedShowFlags = ShowFlags;
		ShowFlags &= ~SHOW_Lighting;
	}
}

void FEditorLevelViewportClient::MoveViewportCamera(const FVector& InDrag,
													const FRotator& InRot,
													UBOOL bUnlitMovement)
{
	const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton);
	const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

	switch( ViewportType )
	{
		case LVT_OrthoXY:
		case LVT_OrthoXZ:
		case LVT_OrthoYZ:
		{
			if( LeftMouseButtonDown && RightMouseButtonDown )
			{
				OrthoZoom += (OrthoZoom / CAMERA_ZOOM_DAMPEN) * InDrag.Z;
				OrthoZoom = Clamp<FLOAT>( OrthoZoom, MIN_ORTHOZOOM, MAX_ORTHOZOOM );	
			}
			else
			{
				if ( ViewportType == LVT_OrthoXY )
					ViewLocation.AddBounded( FVector(InDrag.Y, -InDrag.X, InDrag.Z), THREE_FOURTHS_WORLD_MAX );
				else
					ViewLocation.AddBounded( InDrag, THREE_FOURTHS_WORLD_MAX );
			}

			if ( bUnlitMovement && bMoveUnlit && !CachedShowFlags )
			{
				CachedShowFlags = ShowFlags;
				ShowFlags &= ~SHOW_Lighting;
			}

			// Update any linked orthographic viewports.
			UpdateLinkedOrthoViewports();
		}
		break;

		case LVT_Perspective:
		{
			// If the flight camera is active, we'll update the rotation impulse data for that instead
			// of rotating the camera ourselves here
			if( IsFlightCameraInputModeActive() && CameraController->GetConfig().bUsePhysicsBasedRotation )
			{
				// NOTE: We damp the rotation for impulse input since the camera controller will
				//	apply its own rotation speed
				const FLOAT VelModRotSpeed = 900.0f;
				const FVector RotEuler = InRot.Euler();
				CameraUserImpulseData->RotateRollVelocityModifier += VelModRotSpeed * RotEuler.X / CAMERA_ROTATION_SPEED;
				CameraUserImpulseData->RotatePitchVelocityModifier += VelModRotSpeed * RotEuler.Y / CAMERA_ROTATION_SPEED;
				CameraUserImpulseData->RotateYawVelocityModifier += VelModRotSpeed * RotEuler.Z / CAMERA_ROTATION_SPEED;
			}
			else
			{
				MoveViewportPerspectiveCamera( InDrag, InRot, bUnlitMovement );
			}
		}
		break;
	}
}

void FEditorLevelViewportClient::ApplyDeltaToActors(const FVector& InDrag,
													const FRotator& InRot,
													const FVector& InScale)
{
	if( GEditorModeTools().GetMouseLock() || (InDrag.IsZero() && InRot.IsZero() && InScale.IsZero()) )
	{
		return;
	}

	// If we are scaling, we need to change the scaling factor a bit to properly align to grid.
	FVector ModifiedScale = InScale;
	USelection* SelectedActors = GEditor->GetSelectedActors();
	const UBOOL bScalingActors = !InScale.IsNearlyZero();

	if( bScalingActors )
	{
		/* @todo: May reenable this form of calculating scaling factors later on.
		// Calculate a bounding box for the actors.
		FBox ActorsBoundingBox( 0 );

		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			const FBox ActorsBox = Actor->GetComponentsBoundingBox( TRUE );
			ActorsBoundingBox += ActorsBox;
		}

		const FVector BoxExtent = ActorsBoundingBox.GetExtent();

		for (INT Idx=0; Idx < 3; Idx++)
		{
			ModifiedScale[Idx] = InScale[Idx] / BoxExtent[Idx];
		}
		*/

		// If we are uniformly scaling, make sure to change scale x, y, z by the same amount.
		INT ScalePercent;

		// If we are snapping to grid, use the selected scaling value to determine how fast to scale.
		// otherwise, just default to 10%, since that seems to be a good default.
		if(GEditor->Constraints.SnapScaleEnabled)
		{
			ScalePercent = GEditor->Constraints.ScaleGridSize;
		}
		else
		{
			ScalePercent = 10;
		}

		if(GEditorModeTools().GetWidgetMode() == FWidget::WM_Scale)
		{
			FLOAT ScaleFactor = (GEditor->Constraints.ScaleGridSize / 100.0f) * InScale[0] / GEditor->Constraints.GetGridSize();
			
			ModifiedScale = FVector(ScaleFactor, ScaleFactor, ScaleFactor);
		}
		else
		{
			ModifiedScale = InScale * ((GEditor->Constraints.ScaleGridSize / 100.0f) / GEditor->Constraints.GetGridSize());
		}
	}

	// Transact the actors.
	GEditor->NoteActorMovement();

	TArray<AGroupActor*> ActorGroups;

	// Apply the deltas to any selected actors.
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if ( !Actor->bLockLocation )
		{
			// find topmost selected group
			AGroupActor* ParentGroup = AGroupActor::GetRootForActor(Actor, TRUE, TRUE);
			if(ParentGroup && GEditor->bGroupingActive )
			{
				ActorGroups.AddUniqueItem(ParentGroup);
			}
			else
			{
				ApplyDeltaToActor( Actor, InDrag, InRot, ModifiedScale );
			}
		}
	}
	AGroupActor::RemoveSubGroupsFromArray(ActorGroups);
	for(INT ActorGroupsIndex=0; ActorGroupsIndex<ActorGroups.Num(); ++ActorGroupsIndex)
	{
		ActorGroups(ActorGroupsIndex)->GroupApplyDelta(this, InDrag, InRot, ModifiedScale);
	}

	// Update visuals of the rotate widget 
	ApplyDeltaToRotateWidget( InRot );
}

//
//	FEditorLevelViewportClient::ApplyDeltaToActor
//

void FEditorLevelViewportClient::ApplyDeltaToActor( AActor* InActor, const FVector& InDeltaDrag, const FRotator& InDeltaRot, const FVector& InDeltaScale )
{
	const UBOOL bAltDown = Input->IsAltPressed();
	const UBOOL	bShiftDown = Input->IsShiftPressed();
	const UBOOL bControlDown = Input->IsCtrlPressed();

	GEditor->ApplyDeltaToActor(
		InActor,
		TRUE,
		&InDeltaDrag,
		&InDeltaRot,
		&InDeltaScale,
		bAltDown,
		bShiftDown,
		bControlDown );
}

/** Updates the rotate widget with the passed in delta rotation. */
void FEditorLevelViewportClient::ApplyDeltaToRotateWidget( const FRotator& InRot )
{
	//apply rotation to translate rotate widget
	if (!InRot.IsZero())
	{
		FRotator TranslateRotateWidgetRotation(0, GEditorModeTools().TranslateRotateXAxisAngle, 0);
		TranslateRotateWidgetRotation += InRot;
		GEditorModeTools().TranslateRotateXAxisAngle = TranslateRotateWidgetRotation.Yaw;
		GEditorModeTools().TotalDeltaRotation += GEditorModeTools().CurrentDeltaRotation;
		if ((GEditorModeTools().TotalDeltaRotation <= -MAXWORD) || (GEditorModeTools().TotalDeltaRotation >= MAXWORD))
		{
			GEditorModeTools().TotalDeltaRotation &= MAXWORD;
		}
	}

}
void FEditorLevelViewportClient::GetViewportDimensions( FIntPoint& out_Origin, FIntPoint& out_Size )
{
	out_Origin = FIntPoint(0,0);
	if ( Viewport != NULL )
	{
		out_Size.X = Viewport->GetSizeX();
		out_Size.Y = Viewport->GetSizeY();
	}
	else
	{
		out_Size = FIntPoint(0,0);
	}
}

//
//	FEditorLevelViewportClient::MouseMove
//

void FEditorLevelViewportClient::MouseMove(FViewport* Viewport,INT x, INT y)
{
	// Update the current mouse position;
	CurrentMousePos.X = x;
	CurrentMousePos.Y = y;

	// Let the current editor mode know about the mouse movement.
	if( GEditorModeTools().MouseMove(this, Viewport, x, y) )
	{
		return;
	}

	// Inform GameStatsViewer about the mouse movement
	GEditor->GameStatsMouseMove(this, x, y);

	// Inform Sentinel about the mouse movement
	GEditor->SentinelMouseMove(this, x, y);

	//Only update the position if we are not tracking mouse movement because it will be updated elsewhere if we are.
	if( !bIsTracking )
	{
		if( Viewport->GetSizeX() > 0 && Viewport->GetSizeY() > 0 )
		{
			// Since there is no editor tool that cares about the mouse worldspace position, generate our own status string using the worldspace position
			// of the mouse.  If we are mousing over the perspective viewport, blank out the string because we cannot accurate display the mouse position in that viewport.
			FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds(),IsRealtime());
			FSceneView* View = CalcSceneView(&ViewFamily);

			if( CanUpdateStatusBarText() )
			{
				if( ViewportType == LVT_Perspective )
				{
					MouseWorldSpacePos = FVector(0.f);
					GEditor->UpdateMousePositionText( UEditorEngine::MP_None, MouseWorldSpacePos );
				}
				else
				{
					MouseWorldSpacePos = View->ScreenToWorld(View->PixelToScreen(x,y,0.5f));
					GEditor->UpdateMousePositionText(
						UEditorEngine::MP_WorldspacePosition,
						MouseWorldSpacePos
						);
				}
			}
		}
	}
}

/**
 * Sets the cursor to be visible or not.  Meant to be called as the mouse moves around in "move canvas" mode (not just on button clicks)
 */
UBOOL FEditorLevelViewportClient::UpdateCursorVisibility (void)
{
	UBOOL bShowCursor = ShouldCursorBeVisible();
	Viewport->ShowCursor( bShowCursor );
	return bShowCursor;
}

/**
 * Given that we're in "move canvas" mode, set the snap back visible mouse position to clamp to the viewport
 */
void FEditorLevelViewportClient::UpdateMousePosition(void)
{
	const INT SizeX = Viewport->GetSizeX();
	const INT SizeY = Viewport->GetSizeY();

	INT ClampedMouseX = Clamp<INT>(LastMouseX, 0, SizeX);
	INT ClampedMouseY = Clamp<INT>(LastMouseY, 0, SizeY);

	Viewport->SetMouse(ClampedMouseX, ClampedMouseY);
}


/** Determines if the cursor should presently be visible
 * @return - TRUE if the cursor should remain visible
 */
UBOOL FEditorLevelViewportClient::ShouldCursorBeVisible (void)
{
	const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton) ? TRUE : FALSE;
	const UBOOL MiddleMouseButtonDown = Viewport->KeyState(KEY_MiddleMouseButton) ? TRUE : FALSE;
	const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton) ? TRUE : FALSE;
	const UBOOL bMouseButtonDown = (LeftMouseButtonDown || MiddleMouseButtonDown || RightMouseButtonDown );

	UBOOL AltDown = Input->IsAltPressed();
	UBOOL ShiftDown = Input->IsShiftPressed();
	UBOOL ControlDown = Input->IsCtrlPressed();

	//if we're using the new move canvas mode, we're in an ortho viewport, and the mouse is down
	if (GEditorModeTools().GetPanMovesCanvas() && (ViewportType != LVT_Perspective) && bMouseButtonDown)
	{
		const INT SizeX = Viewport->GetSizeX();
		const INT SizeY = Viewport->GetSizeY();

		UBOOL bInViewport = IsWithin<INT>(LastMouseX, 1, SizeX-1) && IsWithin<INT>(LastMouseY, 1, SizeY-1);

		//MOVING CAMERA
		if ((AltDown == FALSE) && (ShiftDown == FALSE) && (ControlDown == FALSE) && (Widget->GetCurrentAxis() == AXIS_None) && (LeftMouseButtonDown ^ RightMouseButtonDown))
		{
			return bInViewport;
		}
		//Translating an object, but NOT moving the camera AND the object (shift)
		if ((AltDown == FALSE) && (ShiftDown == FALSE) && (GEditorModeTools().GetWidgetMode() == FWidget::WM_Translate) && (Widget->GetCurrentAxis() != AXIS_None) && (LeftMouseButtonDown ^ RightMouseButtonDown))
		{
			return bInViewport;
		}

		//ALL other cases hide the mouse
		return !bMouseButtonDown;
	}
	else
	{
		//current system - do not show cursor when mouse is down
		//if Absolute Translation and not just moving the camera around
		if (IsUsingAbsoluteTranslation())
		{
			return TRUE;
		}
		//current system - do not show cursor when mouse is down
		return !bMouseButtonDown;
	}
}

/** Determines if the new MoveCanvas movement should be used 
 * @return - TRUE if we should use the new drag canvas movement.  Returns false for combined object-camera movement and marquee selection
 */
UBOOL FEditorLevelViewportClient::ShouldUseMoveCanvasMovement (void)
{
	const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton) ? TRUE : FALSE;
	const UBOOL MiddleMouseButtonDown = Viewport->KeyState(KEY_MiddleMouseButton) ? TRUE : FALSE;
	const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton) ? TRUE : FALSE;
	const UBOOL bMouseButtonDown = (LeftMouseButtonDown || MiddleMouseButtonDown || RightMouseButtonDown );

	UBOOL AltDown = Input->IsAltPressed();
	UBOOL ShiftDown = Input->IsShiftPressed();
	UBOOL ControlDown = Input->IsCtrlPressed();

	//if we're using the new move canvas mode, we're in an ortho viewport, and the mouse is down
	if (GEditorModeTools().GetPanMovesCanvas() && (ViewportType != LVT_Perspective) && bMouseButtonDown)
	{
		//MOVING CAMERA
		if ((AltDown == FALSE) && (ShiftDown == FALSE) && (ControlDown == FALSE) && (Widget->GetCurrentAxis() == AXIS_None) && (LeftMouseButtonDown ^ RightMouseButtonDown))
		{
			return TRUE;
		}
		//OBJECT MOVEMENT CODE
		if ((AltDown == FALSE) && (ShiftDown == FALSE) && (GEditorModeTools().GetWidgetMode() == FWidget::WM_Translate) && (Widget->GetCurrentAxis() != AXIS_None) && (LeftMouseButtonDown ^ RightMouseButtonDown))
		{
			return TRUE;
		}


		//ALL other cases hide the mouse
		return FALSE;
	}
	else
	{
		//current system - do not show cursor when mouse is down
		return FALSE;
	}
}

/** True if the window is maximized or floating */
UBOOL FEditorLevelViewportClient::IsVisible() const
{
	UBOOL bIsVisible = FALSE;
	
	if( !IsFloatingViewport() )
	{
		// Find any viewport that is maximized.  
		if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
		{
			FViewportConfig_Data *ViewportConfig = GApp->EditorFrame->ViewportConfigData;
			if( ViewportConfig->IsViewportMaximized() )
			{
				// If this viewport is maximized then its visible
				if( ViewportConfig->AccessViewport( ViewportConfig->MaximizedViewport ).ViewportWindow == this )
				{
					bIsVisible = TRUE;
				}
			}
			else
			{
				// If there is no maximized viewport, the fact we are calling IsVisible means this viewport is visible.
				bIsVisible = TRUE;
			}
		}
	}
	else
	{
		// Floating viewports are always visible
		bIsVisible = TRUE;
	}
	
	return bIsVisible;
}

/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewport	Viewport that captured the mouse input
 * @param	InMouseX	New mouse cursor X coordinate
 * @param	InMouseY	New mouse cursor Y coordinate
 */
void FEditorLevelViewportClient::CapturedMouseMove( FViewport* InViewport, INT InMouseX, INT InMouseY )
{
	// Let the current editor mode know about the mouse movement.
	if( GEditorModeTools().CapturedMouseMove( this, InViewport, InMouseX, InMouseY ) )
	{
		return;
	}
}


//
//	FEditorLevelVIewportClient::GetCursor
//

EMouseCursor FEditorLevelViewportClient::GetCursor(FViewport* Viewport,INT X,INT Y)
{
	USkeletalMeshComponent* OldMouseOverSocketOwner = MouseOverSocketOwner;
	INT OldMouseOverSocketIndex = MouseOverSocketIndex;

	EMouseCursor MouseCursor = MC_Arrow;

	// We'll keep track of changes to hovered objects as the cursor moves
	const UBOOL bUseHoverFeedback = IsEditorFrameClient() && GEditor != NULL && GEditor->GetUserSettings().bEnableViewportHoverFeedback;
	TSet< FViewportHoverTarget > NewHoveredObjects;


	UBOOL bVisibleCursor = ShouldCursorBeVisible();
	UBOOL bMoveCanvasMovement = ShouldUseMoveCanvasMovement();
	//only camera movement gets the hand icon
	if (bVisibleCursor && bMoveCanvasMovement && (Widget->GetCurrentAxis() == AXIS_None) && bHasMouseMovedSinceClick)
	{
		//We're grabbing the canvas so the icon should look "grippy"
		MouseCursor = MC_GrabHand;
	}
	else if (bVisibleCursor && bMoveCanvasMovement && (GEditorModeTools().GetWidgetMode() == FWidget::WM_Translate) && bHasMouseMovedSinceClick)
	{
		MouseCursor = MC_SizeAll;
	}
	//wyisyg mode
	else if (IsUsingAbsoluteTranslation() && bHasMouseMovedSinceClick)
	{
		MouseCursor = MC_SizeAll;
	}
	// Don't select widget axes by mouse over while they're being controlled by a mouse drag.
	else if(!bWidgetAxisControlledByDrag && Viewport) 
	{
		const UBOOL LeftMouseButtonDown = Viewport->KeyState(KEY_LeftMouseButton);
		const UBOOL RightMouseButtonDown = Viewport->KeyState(KEY_RightMouseButton);

		// Don't select a widget axes if both keys are held down
		if (!(LeftMouseButtonDown && RightMouseButtonDown))
		{
			const EAxis SaveAxis = Widget->GetCurrentAxis();
			HHitProxy* HitProxy = Viewport->GetHitProxy(X,Y);
			EAxis NewAxis = AXIS_None;

			MouseOverSocketOwner = NULL;

			// Change the mouse cursor if the user is hovering over something they can interact with.
			if( HitProxy )
			{
				if (GEditor->bEnableSocketNames && HitProxy->IsA(HLevelSocketProxy::StaticGetType()))
				{
					// mouse is over a socket hit proxy
					// store which one we're over in order to hide the text on others to declutter the view
					MouseOverSocketOwner = ((HLevelSocketProxy*)HitProxy)->SkelMeshComponent;
					MouseOverSocketIndex = ((HLevelSocketProxy*)HitProxy)->SocketIndex;
				}

				MouseCursor = HitProxy->GetMouseCursor();
				AActor* ActorUnderCursor = NULL;

				if( HitProxy->IsA(HWidgetAxis::StaticGetType() ) )
				{
					NewAxis = ((HWidgetAxis*)HitProxy)->Axis;
				}

				// If the cursor is visible over level viewports, then we'll check for new objects to be hovered over
				if( bUseHoverFeedback && bVisibleCursor )
				{
					// Set mouse hover cue for objects under the cursor
					if( HitProxy->IsA( HActor::StaticGetType() ) )
					{
						// Hovered over an actor
						HActor* ActorHitProxy = static_cast< HActor* >( HitProxy );
						AActor* ActorUnderCursor = ActorHitProxy->Actor;

						if( ActorUnderCursor != NULL  )
						{
							// Check to see if the actor under the cursor is part of a group.  If so, we will how a hover cue the whole group
							AGroupActor* GroupActor = AGroupActor::GetRootForActor( ActorUnderCursor, TRUE, FALSE );
					
							if( GroupActor && GEditor->bGroupingActive)
							{
								// Get all the actors in the group and add them to the list of objects to show a hover cue for.
								TArray<AActor*> ActorsInGroup;
								GroupActor->GetGroupActors( ActorsInGroup, TRUE );
								for( INT ActorIndex = 0; ActorIndex < ActorsInGroup.Num(); ++ActorIndex )
								{
									NewHoveredObjects.Add( FViewportHoverTarget( ActorsInGroup(ActorIndex) ) );
								}
							}
							else
							{
								NewHoveredObjects.Add( FViewportHoverTarget( ActorUnderCursor ) );
							}
						}
					}
					else if( HitProxy->IsA( HModel::StaticGetType() ) )
					{
						// Hovered over a model (BSP surface)
						HModel* ModelHitProxy = static_cast< HModel* >( HitProxy );
						UModel* ModelUnderCursor = ModelHitProxy->GetModel();
						if( ModelUnderCursor != NULL )
						{
							FSceneViewFamilyContext ViewFamily(
								Viewport, GetScene(),
								ShowFlags,
								GWorld->GetTimeSeconds(),
								GWorld->GetDeltaSeconds(),
								GWorld->GetRealTimeSeconds(),
								IsRealtime()
								);
							FSceneView* SceneView = CalcSceneView( &ViewFamily );

							UINT SurfaceIndex = INDEX_NONE;
							if( ModelHitProxy->ResolveSurface( SceneView, X, Y, SurfaceIndex ) )
							{
								FBspSurf& Surf = ModelUnderCursor->Surfs( SurfaceIndex );
								Surf.PolyFlags |= PF_Hovered;

								NewHoveredObjects.Add( FViewportHoverTarget( ModelUnderCursor, SurfaceIndex ) );
							}
						}
					}
				}
			}
		

			// If the current axis on the widget changed, repaint the viewport.
			if( NewAxis != SaveAxis )
			{
				Widget->SetCurrentAxis( NewAxis );
				GEditorModeTools().SetCurrentWidgetAxis( NewAxis );
				Invalidate( FALSE, FALSE );
			}
		}
	}

	// Check to see if there are any hovered objects that need to be updated
	{
		UBOOL bAnyHoverChanges = FALSE;
		for( TSet<FViewportHoverTarget>::TIterator It( HoveredObjects ); It; ++It )
		{
			FViewportHoverTarget& OldHoverTarget = *It;
			if( !NewHoveredObjects.Contains( OldHoverTarget ) )
			{
				// Remove hover effect from object that no longer needs it
				RemoveHoverEffect( OldHoverTarget );
				HoveredObjects.RemoveKey( OldHoverTarget );

				bAnyHoverChanges = TRUE;
			}
		}

		for( TSet<FViewportHoverTarget>::TIterator It( NewHoveredObjects ); It; ++It )
		{
			FViewportHoverTarget& NewHoverTarget = *It;
			if( !HoveredObjects.Contains( NewHoverTarget ) )
			{
				// Add hover effect to this object
				AddHoverEffect( NewHoverTarget );
				HoveredObjects.Add( NewHoverTarget );

				bAnyHoverChanges = TRUE;
			}
		}


		// Redraw the viewport if we need to
		if( bAnyHoverChanges )
		{
			// NOTE: We're only redrawing the viewport that the mouse is over.  We *could* redraw all viewports
			//		 so the hover effect could be seen in all potential views, but it will be slower.
			RedrawRequested( Viewport );
		}
	}

	if (OldMouseOverSocketOwner != MouseOverSocketOwner || OldMouseOverSocketIndex != MouseOverSocketIndex)
	{
		Invalidate( FALSE, FALSE );
	}

	return MouseCursor;
}

void FEditorLevelViewportClient::RedrawRequested(FViewport* InViewport)
{
	if (ShowFlags & SHOW_Wireframe)
	{
		// wire frame won't benefit from occlusion queries.  Only invalidate once.
		NumPendingRedraws = 1;
	}
	else
	{
		// Whenever a redraw is needed, redraw at least twice to account for occlusion queries being a frame behind.
		NumPendingRedraws = 2;
	}
}

/** 
 * Renders a view frustum specified by the provided frustum parameters
 *
 * @param	PDI					PrimitiveDrawInterface to use to draw the view frustum
 * @param	FrustumColor		Color to draw the view frustum in
 * @param	FrustumAngle		Angle of the frustum
 * @param	FrustumAspectRatio	Aspect ratio of the frustum
 * @param	FrustumStartDist	Start distance of the frustum
 * @param	FrustumEndDist		End distance of the frustum
 * @param	InViewMatrix		View matrix to use to draw the frustum
 */
static void RenderViewFrustum( FPrimitiveDrawInterface* PDI,
									const FLinearColor& FrustumColor,
									FLOAT FrustumAngle,
									FLOAT FrustumAspectRatio,
									FLOAT FrustumStartDist,
									FLOAT FrustumEndDist,
									const FMatrix& InViewMatrix)
{
	FVector Direction(0,0,1);
	FVector LeftVector(1,0,0);
	FVector UpVector(0,1,0);

	FVector Verts[8];

	// FOVAngle controls the horizontal angle.
	FLOAT HozHalfAngle = (FrustumAngle) * ((FLOAT)PI/360.f);
	FLOAT HozLength = FrustumStartDist * appTan(HozHalfAngle);
	FLOAT VertLength = HozLength/FrustumAspectRatio;

	// near plane verts
	Verts[0] = (Direction * FrustumStartDist) + (UpVector * VertLength) + (LeftVector * HozLength);
	Verts[1] = (Direction * FrustumStartDist) + (UpVector * VertLength) - (LeftVector * HozLength);
	Verts[2] = (Direction * FrustumStartDist) - (UpVector * VertLength) - (LeftVector * HozLength);
	Verts[3] = (Direction * FrustumStartDist) - (UpVector * VertLength) + (LeftVector * HozLength);

	HozLength = FrustumEndDist * appTan(HozHalfAngle);
	VertLength = HozLength/FrustumAspectRatio;

	// far plane verts
	Verts[4] = (Direction * FrustumEndDist) + (UpVector * VertLength) + (LeftVector * HozLength);
	Verts[5] = (Direction * FrustumEndDist) + (UpVector * VertLength) - (LeftVector * HozLength);
	Verts[6] = (Direction * FrustumEndDist) - (UpVector * VertLength) - (LeftVector * HozLength);
	Verts[7] = (Direction * FrustumEndDist) - (UpVector * VertLength) + (LeftVector * HozLength);

	for( INT x = 0 ; x < 8 ; ++x )
	{
		Verts[x] = InViewMatrix.Inverse().TransformFVector( Verts[x] );
	}

	const BYTE PrimitiveDPG = SDPG_Foreground;
	PDI->DrawLine( Verts[0], Verts[1], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[1], Verts[2], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[2], Verts[3], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[3], Verts[0], FrustumColor, PrimitiveDPG );

	PDI->DrawLine( Verts[4], Verts[5], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[5], Verts[6], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[6], Verts[7], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[7], Verts[4], FrustumColor, PrimitiveDPG );

	PDI->DrawLine( Verts[0], Verts[4], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[1], Verts[5], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[2], Verts[6], FrustumColor, PrimitiveDPG );
	PDI->DrawLine( Verts[3], Verts[7], FrustumColor, PrimitiveDPG );
}

void FEditorLevelViewportClient::DrawTools(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Draw the current editor mode.
	GEditorModeTools().DrawComponents(View, PDI);
	GEditorModeTools().Render( View, Viewport, PDI );
}

void FEditorLevelViewportClient::Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	FMemMark Mark(GMainThreadMemStack);

	if (GIsTiledScreenshot)
	{
		return;
	}

	if (ShowFlags & SHOW_StreamingBounds)
	{
		DrawTextureStreamingBounds(View, PDI);
	}

	// Draw the current editor mode.
	DrawTools( View, PDI );

	// Determine if a view frustum should be rendered in the viewport.
	// The frustum should definitely be rendered if the viewport has a view parent.
	UBOOL bRenderViewFrustum = ViewState->HasViewParent();

	// If the viewport doesn't have a view parent, a frustum still should be drawn anyway if the viewport is ortho and level streaming
	// volume previs is enabled in some viewport
	if ( !bRenderViewFrustum && ( ViewportType == LVT_OrthoXY || ViewportType == LVT_OrthoXZ || ViewportType == LVT_OrthoYZ ) )
	{
		for ( INT ViewportClientIndex = 0; ViewportClientIndex < GEditor->ViewportClients.Num(); ++ViewportClientIndex )
		{
			const FEditorLevelViewportClient* CurViewportClient = GEditor->ViewportClients( ViewportClientIndex );
			if ( CurViewportClient && CurViewportClient->bLevelStreamingVolumePrevis )
			{
				bRenderViewFrustum = TRUE;
				break;
			}
		}
	}
	
	// Draw the view frustum of the view parent or level streaming volume previs viewport, if necessary
	if ( bRenderViewFrustum )
	{
		RenderViewFrustum( PDI, FLinearColor(1.0,0.0,1.0,1.0),
			GPerspFrustumAngle,
			GPerspFrustumAspectRatio,
			GPerspFrustumStartDist,
			GPerspFrustumEndDist,
			GPerspViewMatrix);
	}

	// Draw the drag tool.
	MouseDeltaTracker->Render3DDragTool( View, PDI );

	// Draw the widget.
	Widget->Render( View, PDI );

	if (ViewportType == LVT_Perspective)
	{
		DrawStaticLightingDebugInfo( View, PDI );
		extern void DrawLightEnvironmentDebugInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI);
		DrawLightEnvironmentDebugInfo( View, PDI );
	}

	if ( GEditor->bEnableSocketSnapping || GEditor->bEnableSocketNames )
	{
		for( FActorIterator It; It; ++It )
		{
			for ( INT ComponentIndex = 0 ; ComponentIndex < It->Components.Num() ; ++ComponentIndex )
			{
				USkeletalMeshComponent* SkelMeshComponent = Cast<USkeletalMeshComponent>(It->Components(ComponentIndex));
				if ( SkelMeshComponent && SkelMeshComponent->SkeletalMesh )
				{
					UBOOL bGameViewMode = ((View->Family->ShowFlags & SHOW_Game) != 0) && (GEditor->bDrawSocketsInGMode == FALSE);
					AActor* Owner = SkelMeshComponent->GetOwner();
					if ((Owner != NULL) && 
						(bGameViewMode || Owner->bHiddenEd || Owner->bHiddenEdTemporary))
					{
						// Don't display sockets on hidden actors...
						continue;
					}

					for ( INT SocketIndex = 0 ; SocketIndex < SkelMeshComponent->SkeletalMesh->Sockets.Num() ; ++SocketIndex )
					{
						USkeletalMeshSocket* Socket = SkelMeshComponent->SkeletalMesh->Sockets(SocketIndex);
						FMatrix SocketTM;
						if( Socket->GetSocketMatrix(SocketTM, SkelMeshComponent) )
						{
							FColor SocketColour = GEditor->bEnableSocketSnapping ? FColor(255,128,128) : FColor(128,128,255);

							if (MouseOverSocketOwner == SkelMeshComponent && MouseOverSocketIndex == SocketIndex)
							{
								SocketColour = FColor(255,255,200);
							}			

							PDI->SetHitProxy( new HLevelSocketProxy( *It, SkelMeshComponent, SocketIndex ) );
							DrawWireDiamond(PDI, SocketTM, 2.f, SocketColour, SDPG_Foreground );
							PDI->SetHitProxy( NULL );
						}
					}
				}
			}
		}
	}

	if (GEditor->bDrawParticleHelpers == TRUE)
	{
		if ((View->Family->ShowFlags & SHOW_Game) == 0)
		{
			extern void DrawParticleSystemHelpers(const FSceneView* View,FPrimitiveDrawInterface* PDI);
			DrawParticleSystemHelpers(View, PDI);
		}
	}

	// Let the game stats visualizer draw things if desired
	if(ViewportType == LVT_Perspective || ViewportType == LVT_OrthoXY)
	{
		GEditor->GameStatsRender3D(View, PDI, ViewportType);
	}

	// Let Sentinel draw things if desired
	if((ViewportType == LVT_Perspective) && (View->Family->ShowFlags & SHOW_SentinelStats))
	{
		GEditor->SentinelStatRender3D(View, PDI);
	}

	// This viewport was just rendered, reset this value.
	FramesSinceLastDraw = 0;

	Mark.Pop();
}

/**
 * Updates the audio listener for this viewport 
 *
 * @param View	The scene view to use when calculate the listener position
 */
void FEditorLevelViewportClient::UpdateAudioListener( const FSceneView& View )
{
	UAudioDevice* AudioDevice = GEditor->Client->GetAudioDevice();

	// AudioDevice may not exist. For example if we are in -nosound mode
	if( AudioDevice )
	{
		FMatrix CameraToWorld = View.ViewMatrix.Inverse();

		FVector ProjUp = CameraToWorld.TransformNormal( FVector( 0, 1000, 0 ) );
		FVector ProjRight = CameraToWorld.TransformNormal( FVector( 1000, 0, 0 ) );
		FVector ProjFront = ProjRight ^ ProjUp;

		ProjUp.Z = Abs( ProjUp.Z ); // Don't allow flipping "up".

		ProjUp.Normalize();
		ProjRight.Normalize();
		ProjFront.Normalize();

		AudioDevice->SetListener( 0, 1, ViewLocation, ProjUp, ProjRight, ProjFront, FALSE );

		// Update reverb settings based on the view of the first player we encounter.
		FReverbSettings ReverbSettings;
		FInteriorSettings InteriorSettings;
		INT ReverbVolumeIndex = GWorld->GetWorldInfo()->GetAudioSettings( ViewLocation, &ReverbSettings, &InteriorSettings );
		AudioDevice->SetAudioSettings( ReverbVolumeIndex, ReverbSettings, InteriorSettings );
	}
}

void FEditorLevelViewportClient::Draw(FViewport* Viewport,FCanvas* Canvas)
{
	// Determine whether we should use world time or real time based on the scene.
	FLOAT TimeSeconds;
	FLOAT RealTimeSeconds;
	FLOAT DeltaTimeSeconds;

	if (( GetScene() != GWorld->Scene) || (IsRealtime() == TRUE))
	{
		// Use time relative to start time to avoid issues with FLOAT vs DOUBLE
		TimeSeconds = GCurrentTime - GStartTime;
		RealTimeSeconds = GCurrentTime - GStartTime;
		DeltaTimeSeconds = GDeltaTime;
	}
	else
	{
		TimeSeconds = GWorld->GetTimeSeconds();
		RealTimeSeconds = GWorld->GetRealTimeSeconds();
		DeltaTimeSeconds = GWorld->GetDeltaSeconds();
	}

	// Setup a FSceneViewFamily/FSceneView for the viewport.
	FSceneViewFamilyContext ViewFamily(
		Canvas->GetRenderTarget(),
		GetScene(),
		ShowFlags,
		TimeSeconds,
		DeltaTimeSeconds,
		RealTimeSeconds,
		IsRealtime(),
		bAllowAmbientOcclusion);

	ViewFamily.bDrawBaseInfo = bDrawBaseInfo;

	FSceneView* View = CalcSceneView( &ViewFamily );

	// Tiled rendering for high-res screenshots.
	if ( GIsTiledScreenshot )
	{
		ViewFamily.ShowFlags = SHOW_ViewMode_Lit | (SHOW_DefaultGame & ~(SHOW_ViewMode_Mask|SHOW_Fog));
		Viewport->CalculateTiledScreenshotSettings(View);
	}

	if(GEditorModeTools().GetActiveMode(EM_InterpEdit) == 0 || !AllowMatineePreview())
	{
		// disable camera motion blur and ambient occlusion history smoothing for the editor, unless the viewport is matinee controlled
		ViewFamily.ShowFlags &= ~SHOW_CameraInterpolation;
	}

	// Update the listener.
	UAudioDevice* AudioDevice = GEditor && GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;
	if( AudioDevice && ( ViewportType == LVT_Perspective ) && IsRealtime() && bSetListenerPosition )
	{
		UpdateAudioListener( *View );
	}

	if((ViewportType == LVT_Perspective) && bConstrainAspectRatio)
	{
		// Clear the background to black if the aspect ratio is constrained, as the scene view won't write to all pixels.
		Clear(Canvas,FLinearColor::Black);
	}

	// Don't use fading or color scaling while we're in light complexity mode, since it may change the colors!
	if( ( ShowFlags & SHOW_LightComplexity ) == 0 )
	{
		if(bEnableFading)
		{
			View->OverlayColor = FadeColor;
			View->OverlayColor.A = Clamp(FadeAmount, 0.f, 1.f);
		}

		if(bEnableColorScaling)
		{
			View->ColorScale = FLinearColor(ColorScale.X,ColorScale.Y,ColorScale.Z);
		}
	}

	if(View->Family->ShowFlags & SHOW_Wireframe)
	{
		// Wireframe color is emissive-only, and mesh-modifying materials do not use material substitution, hence...
		View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
		View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
	}
	else if (bOverrideDiffuseAndSpecular)
	{
		View->DiffuseOverrideParameter = FVector4(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
		View->SpecularOverrideParameter = FVector4(.1f, .1f, .1f, 0.0f);
	}
	else if (bShowReflectionsOnly)
	{
		View->DiffuseOverrideParameter = FVector4(0, 0, 0, 0.0f);
		View->SpecularOverrideParameter = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
	}

	BeginRenderingViewFamily(Canvas,&ViewFamily);

	// Remove temporary debug lines.
	// Possibly a hack. Lines may get added without the scene being rendered etc.
	if (GWorld->LineBatcher != NULL && GWorld->LineBatcher->BatchedLines.Num())
	{
		GWorld->LineBatcher->BatchedLines.Empty();
		GWorld->LineBatcher->BeginDeferredReattach();
	}

	// Draw socket names if desired.
	if ( GEditor->bEnableSocketNames )
	{
		const INT HalfX = Viewport->GetSizeX()/2;
		const INT HalfY = Viewport->GetSizeY()/2;
		for( FActorIterator It; It; ++It )
		{
			for ( INT ComponentIndex = 0 ; ComponentIndex < It->Components.Num() ; ++ComponentIndex )
			{
				USkeletalMeshComponent* SkelMeshComponent = Cast<USkeletalMeshComponent>(It->Components(ComponentIndex));
				if ( SkelMeshComponent && SkelMeshComponent->SkeletalMesh )
				{
					UBOOL bGameViewMode = ((View->Family->ShowFlags & SHOW_Game) != 0) && (GEditor->bDrawSocketsInGMode == FALSE);
					AActor* Owner = SkelMeshComponent->GetOwner();
					if ((Owner != NULL) && 
						(bGameViewMode || Owner->bHiddenEd || Owner->bHiddenEdTemporary))
					{
						// Don't display socket names on hidden actors...
						continue;
					}

					if (MouseOverSocketOwner && MouseOverSocketOwner != SkelMeshComponent)
					{
						// Don't display socket names on sockets that aren't the one under the mouse
						continue;
					}

					for ( INT SocketIndex = 0 ; SocketIndex < SkelMeshComponent->SkeletalMesh->Sockets.Num() ; ++SocketIndex )
					{
						if (MouseOverSocketOwner && MouseOverSocketIndex != SocketIndex)
						{
							// Don't display socket names on sockets that aren't the one under the mouse
							continue;
						}

						USkeletalMeshSocket* Socket = SkelMeshComponent->SkeletalMesh->Sockets(SocketIndex);
						FMatrix SocketTM;
						if( Socket->GetSocketMatrix(SocketTM, SkelMeshComponent) )
						{
							const FVector SocketPos	= SocketTM.GetOrigin();
							const FPlane proj		= View->Project( SocketPos );
							if(proj.W > 0.f)
							{
								const INT XPos = HalfX + ( HalfX * proj.X );
								const INT YPos = HalfY + ( HalfY * (proj.Y * -1) );
								FColor SocketColour = GEditor->bEnableSocketSnapping ? FColor(255,128,128) : FColor(128,128,255);
								DrawShadowedString(Canvas, XPos, YPos, *Socket->SocketName.ToString(), GEngine->SmallFont, SocketColour);
							}
						}
					}
				}
			}
		}
	}

	GEditorModeTools().DrawHUD(this,Viewport,View,Canvas);

	// Draw the widget.
	if (Widget)
	{
		Widget->DrawHUD( Canvas );
	}
	
	// Axes indicators
	if( bDrawAxes && ViewportType == LVT_Perspective &&
		( ViewFamily.ShowFlags & SHOW_Game ) != SHOW_Game )
	{
		DrawAxes(Viewport, Canvas);
	}


	// Information string
	DrawShadowedString(Canvas, 4,4, *GEditorModeTools().InfoString, GEngine->SmallFont, FColor(255,255,255) );


	// Check to see this if this viewport is currently maximized
	UBOOL bIsMaximized = FALSE;
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		if( GApp->EditorFrame->ViewportConfigData->IsViewportMaximized() )
		{
			const INT MaximizedViewportIndex = GApp->EditorFrame->ViewportConfigData->MaximizedViewport;
			if( MaximizedViewportIndex >= 0 && MaximizedViewportIndex < 4 )
			{
				if( GApp->EditorFrame->ViewportConfigData->Viewports[ MaximizedViewportIndex ].ViewportWindow == this )
				{
					bIsMaximized = TRUE;
				}
			}
		}
	}

	// Only highlight the viewport border when we're not already maximized (unless there are any
	// floating viewport windows.)
	if( !bIsMaximized || GApp->EditorFrame->ViewportConfigData->FloatingViewports.Num() > 0 )
	{
		// If we are the current viewport, draw a yellow-highlighted border around the outer edge.
		if( GCurrentLevelEditingViewportClient == this )
		{
			FColor BorderColor = FColor(255,255,0);

			for( INT c = 0 ; c < 3 ; ++c )
			{
				const INT X = 1+c;
				const INT Y = 1+c;
				const INT W = Viewport->GetSizeX()-c;
				const INT H = Viewport->GetSizeY()-c;

				DrawLine2D(Canvas, FVector2D(X,Y), FVector2D(W,Y), BorderColor );
				DrawLine2D(Canvas, FVector2D(W,Y), FVector2D(W,H), BorderColor );
				DrawLine2D(Canvas, FVector2D(W,H), FVector2D(X,H), BorderColor );
				DrawLine2D(Canvas, FVector2D(X,H), FVector2D(X,Y-1), BorderColor );
			}
		}
	}

	// Kismet references
	if((View->Family->ShowFlags & SHOW_KismetRefs) && GWorld->GetGameSequence())
	{
		for ( INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
		{
			ULevel *Level = GWorld->Levels( LevelIndex );
			DrawKismetRefs( Level, Viewport, View, Canvas );
		}
	}

	// Tag drawing
	if(View->Family->ShowFlags & SHOW_ActorTags)
	{
		DrawActorTags(Viewport, View, Canvas);
	}

	DrawStaticLightingDebugInfo(View, Canvas);
	
	// Let the game stats visualizer draw things if desired
	if(ViewportType == LVT_OrthoXY || ViewportType == LVT_Perspective)	
	{
		GEditor->GameStatsRender(this, View, Canvas, ViewportType);
	}

	if((ViewportType != LVT_Perspective) && (View->Family->ShowFlags & SHOW_SentinelStats))
	{
		GEditor->SentinelStatRender(this, View, Canvas);
	}

	// Frame rate display
	INT NextYPos = 8;
	if( IsRealtime() && ShouldShowFPS() )
	{
		const INT XPos = Max( 10, (INT)Viewport->GetSizeX() - 90 );
		NextYPos = DrawFPSCounter( Viewport, Canvas, XPos, NextYPos );
	}

	// Stats display
	if( IsRealtime() && ShouldShowStats() )
	{
		const INT XPos = 4;
		TArray< FDebugDisplayProperty > EmptyPropertyArray;
		DrawStatsHUD( Viewport, Canvas, NULL, EmptyPropertyArray, ViewLocation, ViewRotation );
	}

	if(!IsRealtime())
	{
		// Wait for the rendering thread to finish drawing the view before returning.
		// This reduces the apparent latency of dragging the viewport around.
		FlushRenderingCommands();
	}

	LastViewLocation = ViewLocation;
}




/**
 * Draws a screen space bounding box around the specified actor
 *
 * @param	InCanvas		Canvas to draw on
 * @param	InView			View to render
 * @param	InViewport		Viewport we're rendering into
 * @param	InActor			Actor to draw a bounding box for
 * @param	InColor			Color of bounding box
 * @param	bInDrawBracket	True to draw a bracket, otherwise a box will be rendered
 * @param	bInLabelText	Optional label text to draw
 */
void FEditorLevelViewportClient::DrawActorScreenSpaceBoundingBox( FCanvas* InCanvas, const FSceneView* InView, FViewport* InViewport, AActor* InActor, const FLinearColor& InColor, const UBOOL bInDrawBracket, const FString& InLabelText )
{
	check( InActor != NULL );

	
	// First check to see if we're dealing with a sprite, otherwise just use the normal bounding box
	USpriteComponent* Sprite = InActor->GetActorSpriteComponent();

	FBox ActorBox;
	if( Sprite != NULL )
	{
		ActorBox = Sprite->Bounds.GetBox();
	}
	else
	{
		const UBOOL bNonColliding = TRUE;
		ActorBox = InActor->GetComponentsBoundingBox( bNonColliding );
	}


	// If we didn't get a valid bounding box, just make a little one around the actor location
	if( !ActorBox.IsValid || ActorBox.GetExtent().GetMin() < KINDA_SMALL_NUMBER )
	{
		ActorBox = FBox( InActor->Location - FVector( -20 ), InActor->Location + FVector( 20 ) );
	}


	FVector BoxCenter, BoxExtents;
	ActorBox.GetCenterAndExtents( BoxCenter, BoxExtents );


	// Project center of bounding box onto screen.
	const FVector4 ProjBoxCenter = InView->WorldToScreen(BoxCenter);

	// Do nothing if behind camera
	if( ProjBoxCenter.W > 0.f )
	{
		// Project verts of world-space bounding box onto screen and take their bounding box
		const FVector Verts[8] = {	FVector( 1, 1, 1),
									FVector( 1, 1,-1),
									FVector( 1,-1, 1),
									FVector( 1,-1,-1),
									FVector(-1, 1, 1),
									FVector(-1, 1,-1),
									FVector(-1,-1, 1),
									FVector(-1,-1,-1) };

		const INT HalfX = 0.5f * InViewport->GetSizeX();
		const INT HalfY = 0.5f * InViewport->GetSizeY();

		FVector2D ScreenBoxMin(1000000000, 1000000000);
		FVector2D ScreenBoxMax(-1000000000, -1000000000);

		for(INT j=0; j<8; j++)
		{
			// Project vert into screen space.
			const FVector WorldVert = BoxCenter + (Verts[j]*BoxExtents);
			FVector2D PixelVert;
			if(InView->ScreenToPixel(InView->WorldToScreen(WorldVert),PixelVert))
			{
				// Update screen-space bounding box with with transformed vert.
				ScreenBoxMin.X = ::Min<INT>(ScreenBoxMin.X, PixelVert.X);
				ScreenBoxMin.Y = ::Min<INT>(ScreenBoxMin.Y, PixelVert.Y);

				ScreenBoxMax.X = ::Max<INT>(ScreenBoxMax.X, PixelVert.X);
				ScreenBoxMax.Y = ::Max<INT>(ScreenBoxMax.Y, PixelVert.Y);
			}
		}

		if( bInDrawBracket )
		{
			// Draw a bracket when considering the non-current level.
			const FLOAT DeltaX = ScreenBoxMax.X - ScreenBoxMin.X;
			const FLOAT DeltaY = ScreenBoxMax.X - ScreenBoxMin.X;
			const FIntPoint Offset( DeltaX * 0.2f, DeltaY * 0.2f );

			DrawLine2D(InCanvas, FVector2D(ScreenBoxMin.X, ScreenBoxMin.Y), FVector2D(ScreenBoxMin.X + Offset.X, ScreenBoxMin.Y), InColor );
			DrawLine2D(InCanvas, FVector2D(ScreenBoxMin.X, ScreenBoxMax.Y), FVector2D(ScreenBoxMin.X + Offset.X, ScreenBoxMax.Y), InColor );

			DrawLine2D(InCanvas, FVector2D(ScreenBoxMax.X, ScreenBoxMin.Y), FVector2D(ScreenBoxMax.X - Offset.X, ScreenBoxMin.Y), InColor );
			DrawLine2D(InCanvas, FVector2D(ScreenBoxMax.X, ScreenBoxMax.Y), FVector2D(ScreenBoxMax.X - Offset.X, ScreenBoxMax.Y), InColor );

			DrawLine2D(InCanvas, FVector2D(ScreenBoxMin.X, ScreenBoxMin.Y), FVector2D(ScreenBoxMin.X, ScreenBoxMin.Y + Offset.Y), InColor );
			DrawLine2D(InCanvas, FVector2D(ScreenBoxMax.X, ScreenBoxMin.Y), FVector2D(ScreenBoxMax.X, ScreenBoxMin.Y + Offset.Y), InColor );

			DrawLine2D(InCanvas, FVector2D(ScreenBoxMin.X, ScreenBoxMax.Y), FVector2D(ScreenBoxMin.X, ScreenBoxMax.Y - Offset.Y), InColor );
			DrawLine2D(InCanvas, FVector2D(ScreenBoxMax.X, ScreenBoxMax.Y), FVector2D(ScreenBoxMax.X, ScreenBoxMax.Y - Offset.Y), InColor );
		}
		else
		{
			// Draw a box when considering the current level.
			DrawLine2D(InCanvas, FVector2D(ScreenBoxMin.X, ScreenBoxMin.Y), FVector2D(ScreenBoxMin.X, ScreenBoxMax.Y), InColor );
			DrawLine2D(InCanvas, FVector2D(ScreenBoxMin.X, ScreenBoxMax.Y), FVector2D(ScreenBoxMax.X, ScreenBoxMax.Y), InColor );
			DrawLine2D(InCanvas, FVector2D(ScreenBoxMax.X, ScreenBoxMax.Y), FVector2D(ScreenBoxMax.X, ScreenBoxMin.Y), InColor );
			DrawLine2D(InCanvas, FVector2D(ScreenBoxMax.X, ScreenBoxMin.Y), FVector2D(ScreenBoxMin.X, ScreenBoxMin.Y), InColor );
		}

		if (InLabelText.Len() > 0)
		{
			DrawStringCenteredZ(InCanvas,ScreenBoxMin.X + ((ScreenBoxMax.X - ScreenBoxMin.X) * 0.5f),ScreenBoxMin.Y,1.0f,*InLabelText,GEngine->GetMediumFont(),InColor);
		}
	}
}

/** Draw screen-space box around each Actor in the level referenced by the Kismet sequence. */
void FEditorLevelViewportClient::DrawKismetRefs(ULevel* Level, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas )
{
	USequence* Seq = GWorld->GetGameSequence( Level );
	if ( !Seq )
	{
		return;
	}

	// Get all SeqVar_Objects and SequenceEvents from the Sequence (and all subsequences).
	TArray<USequenceObject*> SeqObjVars;
	Seq->FindSeqObjectsByClass( USequenceObject::StaticClass(), SeqObjVars );
	
	const FColor KismetRefColor(175, 255, 162);
	const UBOOL bIsCurrentLevel = ( Level == GWorld->CurrentLevel );

	TArray<AActor*>	DrawnActors;

	for(INT i=0; i<SeqObjVars.Num(); i++)
	{
		if( !SeqObjVars(i) )
		{
			continue;
		}

		// Give object a chance to draw in viewport also
		SeqObjVars(i)->DrawKismetRefs( Viewport, View, Canvas );

		AActor* RefActor = NULL;
		USeqVar_Object* VarObj = Cast<USeqVar_Object>( SeqObjVars(i) );
		if(VarObj)
		{
			RefActor = Cast<AActor>(VarObj->ObjValue);
		}

		USequenceEvent* EventObj = Cast<USequenceEvent>( SeqObjVars(i) );
		if(EventObj)
		{
			RefActor = EventObj->Originator;
		}

		// If this is a refence to an un-Hidden Actor that we haven't drawn yet, draw it now.
		if(RefActor && !RefActor->IsHiddenEd() && !DrawnActors.ContainsItem(RefActor))
		{

			const UBOOL bDrawBracket = !bIsCurrentLevel;
			const FString KismetDesc = SeqObjVars(i)->ObjComment;
			DrawActorScreenSpaceBoundingBox( Canvas, View, Viewport, RefActor, KismetRefColor, bDrawBracket, KismetDesc );


			// Not not to draw this actor again.
			DrawnActors.AddItem(RefActor);
		}
	}
}

void FEditorLevelViewportClient::DrawActorTags(FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		// Actors default to having a tag set to their class name. Ignore those - we only want tags we have explicitly changed.
		if( Actor->Tag != NAME_None && Actor->Tag != Actor->GetClass()->GetFName() )
		{
			FVector2D PixelLocation;
			if(View->ScreenToPixel(View->WorldToScreen(Actor->Location),PixelLocation))
			{
				const FColor TagColor(250,160,50);
				DrawShadowedString(Canvas,PixelLocation.X,PixelLocation.Y, *Actor->Tag.ToString(), GEngine->SmallFont, TagColor);
			}
		}
	}
}

void FEditorLevelViewportClient::DrawAxes(FViewport* Viewport, FCanvas* Canvas, const FRotator* InRotation)
{
	FRotationMatrix ViewTM( this->ViewRotation );

	if( InRotation )
	{
		ViewTM = FRotationMatrix( *InRotation );
	}

	const INT SizeX = Viewport->GetSizeX();
	const INT SizeY = Viewport->GetSizeY();

	const FIntPoint AxisOrigin( 30, SizeY - 30 );
	const FLOAT AxisSize = 25.f;

	INT XL, YL;
	StringSize(GEngine->SmallFont, XL, YL, TEXT("Z"));

	FVector AxisVec = AxisSize * ViewTM.InverseTransformNormal( FVector(1,0,0) );
	FIntPoint AxisEnd = AxisOrigin + FIntPoint( AxisVec.Y, -AxisVec.Z );
	DrawLine2D(Canvas, AxisOrigin, AxisEnd, FColor(255,0,0) );
	DrawString(Canvas, AxisEnd.X + 2, AxisEnd.Y - 0.5*YL, TEXT("X"), GEngine->SmallFont, FColor(255,0,0) );

	AxisVec = AxisSize * ViewTM.InverseTransformNormal( FVector(0,1,0) );
	AxisEnd = AxisOrigin + FIntPoint( AxisVec.Y, -AxisVec.Z );
	DrawLine2D(Canvas, AxisOrigin, AxisEnd, FColor(0,255,0) );
	DrawString(Canvas, AxisEnd.X + 2, AxisEnd.Y - 0.5*YL, TEXT("Y"), GEngine->SmallFont, FColor(0,255,0) );

	AxisVec = AxisSize * ViewTM.InverseTransformNormal( FVector(0,0,1) );
	AxisEnd = AxisOrigin + FIntPoint( AxisVec.Y, -AxisVec.Z );
	DrawLine2D(Canvas, AxisOrigin, AxisEnd, FColor(0,0,255) );
	DrawString(Canvas, AxisEnd.X + 2, AxisEnd.Y - 0.5*YL, TEXT("Z"), GEngine->SmallFont, FColor(0,0,255) );
}

/**
 *	Draw the texture streaming bounds.
 */
void FEditorLevelViewportClient::DrawTextureStreamingBounds(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Iterate each level
	for (TObjectIterator<ULevel> It; It; ++It)
	{
		ULevel* Level = *It;
		// Grab the streaming bounds entries for the level
		UTexture2D* TargetTexture = NULL;
		TArray<FStreamableTextureInstance>* STIA = Level->GetStreamableTextureInstances(TargetTexture);
		if (STIA)
		{
			for (INT Index = 0; Index < STIA->Num(); Index++)
			{
				FStreamableTextureInstance& STI = (*STIA)(Index);
#if defined(_STREAMING_BOUNDS_DRAW_BOX_)
				FVector InMin = STI.BoundingSphere.Center;
				FVector InMax = STI.BoundingSphere.Center;
				FLOAT Max = STI.BoundingSphere.W;
				InMin -= FVector(Max);
				InMax += FVector(Max);
				FBox Box = FBox(InMin, InMax);
				DrawWireBox(PDI, Box, FColor(255,255,0), SDPG_World);
#else	//#if defined(_STREAMING_BOUNDS_DRAW_BOX_)
				// Draw bounding spheres
				FVector Origin = STI.BoundingSphere.Center;
				FLOAT Radius = STI.BoundingSphere.W;
				DrawCircle(PDI, Origin, FVector(1,0,0), FVector(0,1,0), FColor(255,255,0), Radius, 32, SDPG_World);
				DrawCircle(PDI, Origin, FVector(1,0,0), FVector(0,0,1), FColor(255,255,0), Radius, 32, SDPG_World);
				DrawCircle(PDI, Origin, FVector(0,1,0), FVector(0,0,1), FColor(255,255,0), Radius, 32, SDPG_World);
#endif	//#if defined(_STREAMING_BOUNDS_DRAW_BOX_)
			}
		}
	}
}

/** Serialization. */
FArchive& operator<<(FArchive& Ar,FEditorLevelViewportClient& Viewport)
{
	Ar << Viewport.Input;
	for( TSet<FEditorLevelViewportClient::FViewportHoverTarget>::TIterator It( FEditorLevelViewportClient::HoveredObjects ); It; ++It )
	{
		FEditorLevelViewportClient::FViewportHoverTarget& CurHoverTarget = *It;
		Ar << CurHoverTarget.HoveredActor;
		Ar << CurHoverTarget.HoveredModel;
	}

	return Ar;
}

/**
 * Copies layout and camera settings from the specified viewport
 *
 * @param InViewport The viewport to copy settings from
 */
void FEditorLevelViewportClient::CopyLayoutFromViewport( const FEditorLevelViewportClient& InViewport )
{
	ViewLocation = InViewport.ViewLocation;
	ViewRotation = InViewport.ViewRotation;
	ViewFOV = InViewport.ViewFOV;
	ViewportType = InViewport.ViewportType;
	OrthoZoom = InViewport.OrthoZoom;
	ShowFlags = InViewport.ShowFlags;
	bMoveUnlit = InViewport.bMoveUnlit;
	bLevelStreamingVolumePrevis = InViewport.bLevelStreamingVolumePrevis;
	bPostProcessVolumePrevis = InViewport.bPostProcessVolumePrevis;
	bViewportLocked = InViewport.bViewportLocked;
	bLockSelectedToCamera = InViewport.bLockSelectedToCamera;
	CameraSpeed = InViewport.CameraSpeed;
	bUseSquintMode = InViewport.bUseSquintMode;
	bAllowMatineePreview = InViewport.bAllowMatineePreview;
}

/**
 * Set the viewport type of the client
 *
 * @param InViewportType	The viewport type to set the client to
 */
void FEditorLevelViewportClient::SetViewportType( ELevelViewportType InViewportType )
{
	ViewportType = InViewportType;
	Invalidate();
}

/** Updates any orthographic viewport movement to use the same location as this viewport */
void FEditorLevelViewportClient::UpdateLinkedOrthoViewports( UBOOL bInvalidate )
{
	// Only update if linked ortho movement is on, this viewport is orthographic, and is the current viewport being used.
	if( GEditor->GetUserSettings().bUseLinkedOrthographicViewports && IsOrtho() && GCurrentLevelEditingViewportClient == this )
	{
		INT MaxFrames = -1;
		INT NextViewportIndexToDraw = INDEX_NONE;

		// Search through all viewports for orthographic ones
		for( INT ViewportIndex = 0; ViewportIndex < GEditor->ViewportClients.Num(); ++ViewportIndex )
		{
			FEditorLevelViewportClient* Client = GEditor->ViewportClients(ViewportIndex);
			// Only update other orthographic viewports
			if( Client != this && Client->IsOrtho() )
			{
				INT Frames = Client->FramesSinceLastDraw;
				Client->bNeedsLinkedRedraw = FALSE;
				Client->OrthoZoom = OrthoZoom;
				Client->ViewLocation = ViewLocation;
				if( Client->IsVisible() )
				{
					// Find the viewport which has the most number of frames since it was last rendered.  We will render that next.
					if( Frames > MaxFrames )
					{
						MaxFrames = Frames;
						NextViewportIndexToDraw = ViewportIndex;
					}

				if( bInvalidate )
				{
					Client->Invalidate();
				}
			}
		}
		}

		if( bInvalidate )
		{
			Invalidate();
		}

		if( NextViewportIndexToDraw != INDEX_NONE )
		{
			// Force this viewport to redraw.
			GEditor->ViewportClients(NextViewportIndexToDraw)->bNeedsLinkedRedraw = TRUE;
		}
	}
}
/**
 * Allows custom disabling of camera recoil
 */
void FEditorLevelViewportClient::SetMatineeRecordingWindow (WxInterpEd* InInterpEd)
{
	RecordingInterpEd = InInterpEd;
	FEditorCameraController* CameraController = GetCameraController();
	if (CameraController)
	{
		FCameraControllerConfig Config = CameraController->GetConfig();
		RecordingInterpEd->LoadRecordingSettings(OUT Config);
		CameraController->SetConfig(Config);
	}
}


//
//	FEditorLevelViewportClient::GetScene
//

FSceneInterface* FEditorLevelViewportClient::GetScene()
{
	return GWorld->Scene;
}

//
//	FEditorLevelViewportClient::GetBackgroundColor
//

FLinearColor FEditorLevelViewportClient::GetBackgroundColor()
{
	return (ViewportType == LVT_Perspective) ? GEditor->C_WireBackground : GEditor->C_OrthoBackground;
}


/**
 * Static: Adds a hover effect to the specified object
 *
 * @param	InHoverTarget	The hoverable object to add the effect to
 */
void FEditorLevelViewportClient::AddHoverEffect( FEditorLevelViewportClient::FViewportHoverTarget& InHoverTarget )
{
	AActor* ActorUnderCursor = InHoverTarget.HoveredActor;
	UModel* ModelUnderCursor = InHoverTarget.HoveredModel;

	if( ActorUnderCursor != NULL )
	{
		for(INT ComponentIndex = 0;ComponentIndex < ActorUnderCursor->Components.Num();ComponentIndex++)
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(ActorUnderCursor->Components(ComponentIndex));
			if (PrimitiveComponent && PrimitiveComponent->IsAttached())
			{
				PrimitiveComponent->PushHoveredToProxy( TRUE );
			}
		}
	}
	else if( ModelUnderCursor != NULL )
	{
		check( InHoverTarget.ModelSurfaceIndex != INDEX_NONE );
		check( InHoverTarget.ModelSurfaceIndex < (UINT)ModelUnderCursor->Surfs.Num() );
		FBspSurf& Surf = ModelUnderCursor->Surfs( InHoverTarget.ModelSurfaceIndex );
		Surf.PolyFlags |= PF_Hovered;
	}
}


/**
 * Static: Removes a hover effect to the specified object
 *
 * @param	InHoverTarget	The hoverable object to remove the effect from
 */
void FEditorLevelViewportClient::RemoveHoverEffect( FEditorLevelViewportClient::FViewportHoverTarget& InHoverTarget )
{
	AActor* CurHoveredActor = InHoverTarget.HoveredActor;
	if( CurHoveredActor != NULL )
	{
		for(INT ComponentIndex = 0;ComponentIndex < CurHoveredActor->Components.Num();ComponentIndex++)
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(CurHoveredActor->Components(ComponentIndex));
			if (PrimitiveComponent && PrimitiveComponent->IsAttached())
			{
				PrimitiveComponent->PushHoveredToProxy( FALSE );
			}
		}
	}

	UModel* CurHoveredModel = InHoverTarget.HoveredModel;
	if( CurHoveredModel != NULL )
	{
		if( InHoverTarget.ModelSurfaceIndex != INDEX_NONE &&
			(UINT)CurHoveredModel->Surfs.Num() >= InHoverTarget.ModelSurfaceIndex )
		{
			FBspSurf& Surf = CurHoveredModel->Surfs( InHoverTarget.ModelSurfaceIndex );
			Surf.PolyFlags &= ~PF_Hovered;
		}
	}
}


/**
 * Static: Clears viewport hover effects from any objects that currently have that
 */
void FEditorLevelViewportClient::ClearHoverFromObjects()
{
	// Clear hover feedback for any actor's that were previously drawing a hover cue
	if( HoveredObjects.Num() > 0 )
	{
		for( TSet<FViewportHoverTarget>::TIterator It( HoveredObjects ); It; ++It )
		{
			FViewportHoverTarget& CurHoverTarget = *It;
			RemoveHoverEffect( CurHoverTarget );
		}

		HoveredObjects.Empty();
	}
}

/**
 * Returns whether the provided unlocalized sprite category is visible in the viewport or not
 *
 * @param	UnlocalizedCategory	UnlocalizedCategory	The unlocalized sprite category name
 *
 * @return	TRUE if the specified category is visible in the viewport; FALSE if it is not
 */
UBOOL FEditorLevelViewportClient::GetSpriteCategoryVisibility( const FName& UnlocalizedCategory ) const
{
	const INT CategoryIndex = GEngine->GetSpriteCategoryIndex( UnlocalizedCategory );
	check( CategoryIndex != INDEX_NONE && CategoryIndex < SpriteCategoryVisibility.Num() );
	
	return SpriteCategoryVisibility( CategoryIndex );
}

/**
 * Returns whether the sprite category specified by the provided index is visible in the viewport or not
 *
 * @param	Index	Index of the sprite category to check
 *
 * @return	TRUE if the category specified by the index is visible in the viewport; FALSE if it is not
 */
UBOOL FEditorLevelViewportClient::GetSpriteCategoryVisibility( INT Index ) const
{
	check( Index >= 0 && Index < SpriteCategoryVisibility.Num() );
	return SpriteCategoryVisibility( Index );
}

/**
 * Sets the visibility of the provided unlocalized category to the provided value
 *
 * @param	UnlocalizedCategory	The unlocalized sprite category name to set the visibility of
 * @param	bVisible			TRUE if the category should be made visible, FALSE if it should be hidden
 */
void FEditorLevelViewportClient::SetSpriteCategoryVisibility( const FName& UnlocalizedCategory, UBOOL bVisible )
{
	const INT CategoryIndex = GEngine->GetSpriteCategoryIndex( UnlocalizedCategory );
	check( CategoryIndex != INDEX_NONE && CategoryIndex < SpriteCategoryVisibility.Num() );

	SpriteCategoryVisibility( CategoryIndex ) = bVisible;
}

/**
 * Sets the visibility of the category specified by the provided index to the provided value
 *
 * @param	Index		Index of the sprite category to set the visibility of
 * @param	bVisible	TRUE if the category should be made visible, FALSE if it should be hidden
 */
void FEditorLevelViewportClient::SetSpriteCategoryVisibility( INT Index, UBOOL bVisible )
{
	check( Index >= 0 && Index < SpriteCategoryVisibility.Num() );
	SpriteCategoryVisibility( Index ) = bVisible;
}

/**
 * Sets the visibility of all sprite categories to the provided value
 *
 * @param	bVisible	TRUE if all the categories should be made visible, FALSE if they should be hidden
 */
void FEditorLevelViewportClient::SetAllSpriteCategoryVisibility( UBOOL bVisible )
{
	SpriteCategoryVisibility.Init( bVisible, SpriteCategoryVisibility.Num() );
}

/**
* Moves the viewport camera to the given point, offset based on the given point and current facing direction of the viewport camera
*
* @param	NewTargetLocation	Location to move the viewport camera
* @param	TargetActor			Target actor to view (used for better calculating of camera offset)
*/
void FEditorLevelViewportClient::TeleportViewportCamera(FVector NewTargetLocation, const AActor* TargetActor/* = NULL*/)
{
	if(ViewportType == LVT_Perspective)
	{
		// Get normalized direction from the camera towards the indended target
		FVector Direction = ViewLocation - NewTargetLocation;
		Direction.Normalize();

		// Initialize offset with a default value to be used if the target actor does not exist or does not have a bounding cylinder.
		FLOAT CameraOffset = 1000.0f;

		// Base offset approximately off size of the target actor, if there is one
		if(TargetActor)
		{
			FLOAT CylRadius, CylHeight;
			TargetActor->GetBoundingCylinder(CylRadius, CylHeight);
			if(CylRadius > 0.0f)
			{
				// Don't go beyond our default offset so we don't move the camera far away from large Actors
				CameraOffset = Min(CameraOffset, CylRadius * 2.0f);
			}
		}
		
		ViewLocation = NewTargetLocation + Direction * CameraOffset, TRUE;
	}
};

//
//	UEditorViewportInput::Exec
//

UBOOL UEditorViewportInput::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(CurrentEvent == IE_Pressed || (CurrentEvent == IE_Released && ParseCommand(&Cmd,TEXT("OnRelease"))))
	{
		if(Editor->Exec(Cmd,Ar))
			return 1;
	}

	return Super::Exec(Cmd,Ar);

}

IMPLEMENT_CLASS(UEditorViewportInput);
