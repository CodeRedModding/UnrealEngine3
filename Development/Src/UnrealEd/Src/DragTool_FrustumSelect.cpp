/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DragTool_FrustumSelect.h"

///////////////////////////////////////////////////////////////////////////////
//
// FDragTool_FrustumSelect
//
///////////////////////////////////////////////////////////////////////////////

/** 
 * Updates the drag tool's end location with the specified delta.  The end location is
 * snapped to the editor constraints if bUseSnapping is TRUE.
 *
 * @param	InDelta		A delta of mouse movement.
 */
void FDragTool_FrustumSelect::AddDelta( const FVector& InDelta )
{
	EndWk += InDelta;

	End = EndWk;

	const UBOOL bUseHoverFeedback = GEditor != NULL && GEditor->GetUserSettings().bEnableViewportHoverFeedback;

	if( bUseHoverFeedback )
	{
		// If we are using over feedback calculate a new frustum from the current box being dragged
		FConvexVolume Frustum;
		CalculateFrustum( Frustum, TRUE );

		// Does the actor have to be fully contained inside the box
		const UBOOL bStrictDragSelection = GEditor->GetUserSettings().bStrictBoxSelection;

		// Check every visible actor to see if it intersects the frustum created by the box
		// If it does, the actor will be selected and should be given a hover cue
		for( INT ActorIndex = 0; ActorIndex < ActorsToCheck.Num(); ++ActorIndex )
		{
			AActor& Actor = *ActorsToCheck(ActorIndex);

			if( IntersectsFrustum( Actor, Frustum, bStrictDragSelection ) )
			{
				// Apply a hover effect to any actor that will be selected
				AddHoverEffect( Actor );
			}
			else
			{
				// Remove any hover effect on this actor as it no longer will be selected by the current box
				RemoveHoverEffect( Actor );
			}
		}

		// Check each model to see if it will be selected
		for( INT ModelIndex = 0; ModelIndex < ModelsToCheck.Num(); ++ModelIndex )
		{
			UModel& Model = *ModelsToCheck(ModelIndex);
			for (INT NodeIndex = 0; NodeIndex < Model.Nodes.Num(); NodeIndex++)
			{
				if( IntersectsFrustum( Model, NodeIndex, Frustum, bStrictDragSelection ) )
				{
					// Apply a hover effect to any bsp surface that will be selected
					AddHoverEffect( Model, Model.Nodes(NodeIndex).iSurf );
				}
				else
				{
					// Remove any hover effect on this bsp surface as it no longer will be selected by the current box
					RemoveHoverEffect( Model, Model.Nodes(NodeIndex).iSurf );
				}
			}
		}
	}
}

/**
 * Starts a mouse drag behavior.  The start location is snapped to the editor constraints if bUseSnapping is TRUE.
 *
 * @param	InViewportClient	The viewport client in which the drag event occurred.
 * @param	InStart				Where the mouse was when the drag started.
 */
void FDragTool_FrustumSelect::StartDrag(FEditorLevelViewportClient* InViewportClient, const FVector& InStart)
{
	FDragTool::StartDrag( InViewportClient, InStart );

	const UBOOL bUseHoverFeedback = GEditor != NULL && GEditor->GetUserSettings().bEnableViewportHoverFeedback;

	// Remove any active hover objects
	FEditorLevelViewportClient::ClearHoverFromObjects();

	ViewportClient = InViewportClient;
	FIntPoint MousePos = ViewportClient->CurrentMousePos;
	Start = FVector(MousePos.X, MousePos.Y, 0);
	End = EndWk = Start;

	// If showing hover feedback for actors which will be selected by the box,
	// create a list of actors to check against each update.  Cull out any that aren't in view
	// now so we have a smaller list to check against.
	if( bUseHoverFeedback )
	{
		// Does an actor have to be fully contained in the box to be selected
		const UBOOL bStrictDragSelection = GEditor->GetUserSettings().bStrictBoxSelection;

		// Get the view frustum
		FConvexVolume Frustum;
		CalculateFrustum( Frustum, FALSE );

		ActorsToCheck.Reset();
		// Add all actors which intersect the view frustum
		for( FActorIterator It; It; ++It )
		{
			AActor* Actor = *It;
			if( IntersectsFrustum( *Actor, Frustum, bStrictDragSelection ) )
			{
				ActorsToCheck.AddItem( Actor );
			}
		}
	}

	// Create a list of bsp models to check for intersection with the box
	ModelsToCheck.Reset();
	// Do not select BSP if its not visible
	if( (ViewportClient->ShowFlags & SHOW_BSP) != 0 && (ViewportClient->ShowFlags & SHOW_ViewMode_BrushWireframe) == 0 )
	{
		// Add the persistent level always
		ModelsToCheck.AddItem( GWorld->PersistentLevel->Model );
		// Add all streaming level models
		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
		for( INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
			// Only add streaming level models if the level is visible
			if( StreamingLevel->bShouldBeVisibleInEditor )
			{	
				ModelsToCheck.AddItem( StreamingLevel->LoadedLevel->Model );
			}
		}
	}

}

/**
* Ends a mouse drag behavior (the user has let go of the mouse button).
*/
void FDragTool_FrustumSelect::EndDrag()
{
	FEditorModeTools& EdModeTools = GEditorModeTools();
	const UBOOL bGeometryMode = EdModeTools.IsModeActive( EM_Geometry );

	// Generate a frustum out of the dragged box
	FConvexVolume Frustum;
	CalculateFrustum( Frustum, TRUE );

	// If the user is selecting, but isn't hold down SHIFT, remove all current selections.
	if( bLeftMouseButtonDown && !bShiftDown )
	{
		GEditorModeTools().SelectNone();
	}

	// Does an actor have to be fully contained in the box to be selected
	const UBOOL bStrictDragSelection = GEditor->GetUserSettings().bStrictBoxSelection;
	UBOOL bSelectionChanged = FALSE;

	// Let the editor mode try to handle the selection.
	const UBOOL bEditorModeHandledSelection = GEditorModeTools().FrustumSelect( Frustum, bLeftMouseButtonDown );

	if( !bEditorModeHandledSelection )
	{
		// If the user is selecting, but isn't hold down SHIFT, remove all current selections.
		if( bLeftMouseButtonDown && !bShiftDown )
		{
			GEditor->SelectNone( TRUE, TRUE );
		}
	
		// If we have prebuilt list of actors use that as the list of actors which are visible
		// This list is only created if we are showing hover cues
		if( ActorsToCheck.Num() )
		{
			for( INT ActorIndex = 0; ActorIndex < ActorsToCheck.Num(); ++ActorIndex )
			{
				// Check to see if the actor intersects the frustum
				AActor* Actor = ActorsToCheck(ActorIndex);
				UBOOL bIntersect = IntersectsFrustum( *Actor, Frustum, bStrictDragSelection );
				if(bIntersect)
				{
					// Select the actor if it intersected
					GEditor->SelectActor( Actor, bLeftMouseButtonDown, ViewportClient, FALSE);
					bSelectionChanged = TRUE;
				}
			}
		}
		else
		{
			// There is no prebuilt list.  Iterate over every actor and see if it intersects
			for( FActorIterator It; It; ++It )
			{
				AActor* Actor = *It;
				UBOOL bIntersect = IntersectsFrustum( *Actor, Frustum, bStrictDragSelection );
				if(bIntersect)
				{
					// Select the actor if it intersected
					GEditor->SelectActor( Actor, bLeftMouseButtonDown, ViewportClient, FALSE);
					bSelectionChanged = TRUE;
				}
			}
		}

		// Check every model to see if its BSP surfaces should be selected
		for( INT ModelIndex = 0; ModelIndex < ModelsToCheck.Num(); ++ModelIndex )
		{
			UModel& Model = *ModelsToCheck(ModelIndex);
			// Check every node in the model
			for (INT NodeIndex = 0; NodeIndex < Model.Nodes.Num(); NodeIndex++)
			{
				if( IntersectsFrustum( Model, NodeIndex, Frustum, bStrictDragSelection ) )
				{
					// If the node intersected the frustum select the corresponding surface
					GEditor->SelectBSPSurf( &Model, Model.Nodes(NodeIndex).iSurf, TRUE, FALSE );
					bSelectionChanged = TRUE;
				}
			}
		}

		if(bSelectionChanged)
		{
			// If any selections were made.  Notify that now.
			GEditor->NoteSelectionChange();
		}
	}

	// Clear any hovered objects that might have been created while dragging
	FEditorLevelViewportClient::ClearHoverFromObjects();

	FDragTool::EndDrag();
}

void FDragTool_FrustumSelect::Render(const FSceneView* View, FCanvas* Canvas )
{
	DrawBox2D(Canvas, FVector2D(Start.X, Start.Y), FVector2D(End.X, End.Y), FColor(255,0,0) );
}

/** 
 * Returns TRUE if the mesh on the component has vertices which intersect the frustum
 *
 * @param InComponent			The static mesh or skeletal mesh component to check
 * @param InFrustum				The frustum to check against.
 * @param bUseStrictSelection	TRUE if all the vertices must be entirely within the frustum
 */
UBOOL FDragTool_FrustumSelect::IntersectsVertices( UPrimitiveComponent& InComponent, const FConvexVolume& InFrustum, UBOOL bUseStrictSelection ) const
{
	UBOOL bAlreadyProcessed = FALSE;
	UBOOL bResult = FALSE;
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(&InComponent);
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(&InComponent);

	if( StaticMeshComponent && StaticMeshComponent->StaticMesh && (ViewportClient->ShowFlags & SHOW_StaticMeshes) )
	{
		check( StaticMeshComponent->StaticMesh->LODModels.Num() > 0 );
		const FStaticMeshRenderData& LODModel = StaticMeshComponent->StaticMesh->LODModels(0);
		for( UINT VertexIndex = 0; VertexIndex < LODModel.NumVertices; ++VertexIndex )
		{
			const FVector& LocalPosition = LODModel.PositionVertexBuffer.VertexPosition( VertexIndex );
			const FVector& WorldPosition =StaticMeshComponent->LocalToWorld.TransformFVector( LocalPosition );
			UBOOL bLocationIntersected = InFrustum.IntersectBox( WorldPosition, FVector(0,0,0) );
			if( bLocationIntersected && !bUseStrictSelection )
			{
				bResult = TRUE;
				bAlreadyProcessed = TRUE;
				break;
			}
			else if( !bLocationIntersected && bUseStrictSelection )
			{
				bResult = FALSE;
				bAlreadyProcessed = TRUE;
				break;
			}
		}

		if( !bAlreadyProcessed && bUseStrictSelection )
		{
			bResult = TRUE;
			bAlreadyProcessed = TRUE;
		}
	}
	else if( SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh && (ViewportClient->ShowFlags & SHOW_SkeletalMeshes) )
	{
		check( SkeletalMeshComponent->SkeletalMesh->LODModels.Num() > 0 );

		const FStaticLODModel& LODModel = SkeletalMeshComponent->SkeletalMesh->LODModels(0);
		for( INT ChunkIndex = 0 ; ChunkIndex < LODModel.Chunks.Num() && !bAlreadyProcessed; ++ChunkIndex )
		{
			const FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIndex);
			for( INT VertexIndex = 0; VertexIndex < Chunk.RigidVertices.Num(); ++VertexIndex )
			{
				const FVector Location = SkeletalMeshComponent->LocalToWorld.TransformFVector( Chunk.RigidVertices(VertexIndex).Position );
				const UBOOL bLocationIntersected = InFrustum.IntersectBox( Location, FVector(0,0,0) );

				// If the selection box doesn't have to encompass the entire component and a skeletal mesh vertex has intersected with
				// the selection box, this component is being touched by the selection box
				if( bLocationIntersected && !bUseStrictSelection )
				{
					bResult = TRUE;
					bAlreadyProcessed = TRUE;
					break;
				}

				// If the selection box has to encompass the entire component and a skeletal mesh vertex didn't intersect with the selection
				// box, this component does not qualify
				else if ( !bLocationIntersected && bUseStrictSelection )
				{
					bResult = FALSE;
					bAlreadyProcessed = TRUE;
					break;
				}
			}

			for( INT VertexIndex = 0 ; VertexIndex < Chunk.SoftVertices.Num() && !bAlreadyProcessed ; ++VertexIndex )
			{
				const FVector Location = SkeletalMeshComponent->LocalToWorld.TransformFVector( Chunk.SoftVertices(VertexIndex).Position );
				const UBOOL bLocationIntersected = InFrustum.IntersectBox( Location, FVector(0,0,0) );

				// If the selection box doesn't have to encompass the entire component and a skeletal mesh vertex has intersected with
				// the selection box, this component is being touched by the selection box
				if( bLocationIntersected && !bUseStrictSelection )
				{
					bResult = TRUE;
					bAlreadyProcessed = TRUE;
					break;
				}

				// If the selection box has to encompass the entire component and a skeletal mesh vertex didn't intersect with the selection
				// box, this component does not qualify
				else if ( !bLocationIntersected && bUseStrictSelection  )
				{
					bResult = FALSE;
					bAlreadyProcessed = TRUE;
					break;
				}
			}

			// If the selection box has to encompass all of the component and none of the component's verts failed the intersection test, this component
			// is consider touching
			if ( !bAlreadyProcessed && bUseStrictSelection )
			{
				bResult = TRUE;
				bAlreadyProcessed = TRUE;
			}
		}
	}

	return bResult;
}

UBOOL FDragTool_FrustumSelect::IntersectsFrustum( AActor& InActor, const FConvexVolume& InFrustum, UBOOL bUseStrictSelection ) const
{	
	UBOOL bActorHitByBox = FALSE;
	UBOOL bActorFullyContained = FALSE;

	// Check for special cases (like certain show flags that might hide an actor)
	UBOOL bActorIsHiddenByShowFlags = FALSE;

	if( InActor.IsA(ANavigationPoint::StaticClass()) && !(ViewportClient->ShowFlags&SHOW_NavigationNodes) )
	{
		bActorIsHiddenByShowFlags = TRUE;
	}
	// Check to see that volume actors are visible in the viewport
	else if( InActor.IsAVolume() && (!(ViewportClient->ShowFlags&SHOW_Volumes) || !ViewportClient->IsVolumeVisibleInViewport(InActor) ) )
	{
		bActorIsHiddenByShowFlags = TRUE;
	}

	// Never drag-select hidden actors or builder brushes. 
	if( !bActorIsHiddenByShowFlags && !InActor.IsHiddenEd() && !InActor.IsABuilderBrush() )
	{
		// Determine if the user would prefer to use strict selection or not
		const UBOOL bStrictDragSelection = GEditor->GetUserSettings().bStrictBoxSelection;

		// Skeletal meshes.
		APawn* Pawn = Cast<APawn>( &InActor );
		if( Pawn && Pawn->Mesh )
		{
			if( InFrustum.IntersectBox(Pawn->Location, Pawn->GetCylinderExtent(), bActorFullyContained) 
				&& (!bStrictDragSelection || bActorFullyContained) )
			{
				bActorHitByBox = TRUE;
			}
		}
		else
		{
			// Iterate over all actor components, selecting out primitive components
			for( INT ComponentIndex = 0 ; ComponentIndex < InActor.Components.Num() ; ++ComponentIndex )
			{
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>( InActor.Components(ComponentIndex) );
				if ( PrimitiveComponent && !PrimitiveComponent->HiddenEditor )
				{
					FVector Origin = PrimitiveComponent->Bounds.Origin;
					FVector Extent;
					if(PrimitiveComponent->IsA(UDrawLightRadiusComponent::StaticClass())
						|| PrimitiveComponent->IsA(UDrawLightConeComponent::StaticClass()))
					{
						// Do not select by DrawLight Radii or Cones
						continue;
					}
					if(PrimitiveComponent->IsA(USpriteComponent::StaticClass()))
					{
						// Use the size of the sprite itself rather than its box extent
						USpriteComponent* SpriteComponent = static_cast<USpriteComponent*>(PrimitiveComponent);
						Extent = FVector(InActor.DrawScale * SpriteComponent->Scale * SpriteComponent->Sprite->SizeX/2.0f, InActor.DrawScale * SpriteComponent->Scale * SpriteComponent->Sprite->SizeY/2.0f, 0.0f);
					}
					else
					{
						Extent = PrimitiveComponent->Bounds.BoxExtent;
					}


					UBrushComponent* BrushComponent = Cast<UBrushComponent>( PrimitiveComponent );
					if( BrushComponent && BrushComponent->Brush && BrushComponent->Brush->Polys )
					{
						UBOOL bAlreadyProcessed = FALSE;

						// Need to check each vert of the brush separately
						for( INT PolyIndex = 0 ; PolyIndex < BrushComponent->Brush->Polys->Element.Num() && !bAlreadyProcessed ; ++PolyIndex )
						{
							const FPoly& Poly = BrushComponent->Brush->Polys->Element( PolyIndex );

							for( INT VertexIndex = 0 ; VertexIndex < Poly.Vertices.Num() ; ++VertexIndex )
							{
								const FVector Location = BrushComponent->LocalToWorld.TransformFVector( Poly.Vertices(VertexIndex) );
								const UBOOL bIntersect = InFrustum.IntersectBox( Location, FVector(0,0,0) );

								if( bIntersect && !bStrictDragSelection )
								{
									// If we intersected a vertex and we dont require the box to encompass the entire component
									// then the actor should be selected and we can stop checking
									bAlreadyProcessed = TRUE;
									bActorHitByBox = TRUE;
									break;
								}
								else if( !bIntersect && bStrictDragSelection )
								{
									// If we didnt intersect a vertex but we require the box to encompass the entire component
									// then this test failed and we can stop checking
									bAlreadyProcessed = TRUE;
									bActorHitByBox = FALSE;
									break;
								}
							}
						}

						if( bStrictDragSelection && !bAlreadyProcessed )
						{
							// If we are here then every vert was intersected so we should select the actor
							bActorHitByBox = TRUE;
							bAlreadyProcessed = TRUE;
						}

						if( bAlreadyProcessed )
						{
							// if this component has been processed, dont bother checking other components
							break;
						}
					}
					else
					{
						if ( InFrustum.IntersectBox( Origin, Extent, bActorFullyContained) 
							&& (!bStrictDragSelection || bActorFullyContained))
						{
							if( PrimitiveComponent->IsA( UStaticMeshComponent::StaticClass() ) || PrimitiveComponent->IsA( USkeletalMeshComponent::StaticClass() ) )
							{
								// Check each vertex on the component's mesh for intersection
								bActorHitByBox = IntersectsVertices( *PrimitiveComponent, InFrustum, bUseStrictSelection );
							}
							else
							{
								// Use the bounding box check for all other components
								bActorHitByBox = TRUE;
							}
							
							break;
						}
					}
				}
			}
		}
	}

	return bActorHitByBox;
}

/** 
 * Returns TRUE if the provided BSP node intersects with the provided frustum 
 *
 * @param InModel				The model containing BSP nodes to check
 * @param NodeIndex				The index to a BSP node in the model.  This node is used for the bounds check.
 * @param InFrustum				The frustum to check against.
 * @param bUseStrictSelection	TRUE if the node must be entirely within the frustum
 */
UBOOL FDragTool_FrustumSelect::IntersectsFrustum( const UModel& InModel, INT NodeIndex, const FConvexVolume& InFrustum, UBOOL bUseStrictSelection ) const
{
	FBox NodeBB;
	// Get a bounding box of the node being checked
	InModel.GetNodeBoundingBox( InModel.Nodes(NodeIndex), NodeBB );

	UBOOL bFullyContained = FALSE;

	// Does the box intersect the frustum
	UBOOL bIntersects = InFrustum.IntersectBox( NodeBB.GetCenter(), NodeBB.GetExtent(), bFullyContained );

	return bIntersects && (!bUseStrictSelection || bUseStrictSelection && bFullyContained );
}

/** 
 * Calculates a frustum to check actors against 
 * 
 * @param OutFrustum		The created frustum
 * @param bUseBoxFrustum	If TRUE a frustum out of the current dragged box will be created.  FALSE will use the view frustum.
 */
void FDragTool_FrustumSelect::CalculateFrustum( FConvexVolume& OutFrustum, UBOOL bUseBoxFrustum )
{
	// Create a Scene View for our calculations
	FSceneViewFamilyContext ViewFamily(
		ViewportClient->Viewport, ViewportClient->GetScene(),
		ViewportClient->ShowFlags,
		GWorld->GetTimeSeconds(),
		GWorld->GetDeltaSeconds(),
		GWorld->GetRealTimeSeconds(),
		NULL,
		ViewportClient->IsRealtime()
		);
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);

	if( bUseBoxFrustum )
	{
		FVector CamPoint = ViewportClient->ViewLocation;
		FVector BoxPoint1, BoxPoint2, BoxPoint3, BoxPoint4;
		FVector TempDir;
		// Deproject the four corners of the selection box
		FVector2D Point1(Min(Start.X, End.X), Min(Start.Y, End.Y)); // Upper Left Corner
		FVector2D Point2(Max(Start.X, End.X), Min(Start.Y, End.Y)); // Upper Right Corner
		FVector2D Point3(Max(Start.X, End.X), Max(Start.Y, End.Y)); // Lower Right Corner
		FVector2D Point4(Min(Start.X, End.X), Max(Start.Y, End.Y)); // Lower Left Corner
		View->DeprojectFVector2D(Point1, BoxPoint1, TempDir);
		View->DeprojectFVector2D(Point2, BoxPoint2, TempDir);
		View->DeprojectFVector2D(Point3, BoxPoint3, TempDir);
		View->DeprojectFVector2D(Point4, BoxPoint4, TempDir);
		// Get six planes to create a frustum
		FPlane NearPlane;
		View->ViewProjectionMatrix.GetFrustumNearPlane(NearPlane);
		FPlane FarPlane;
		View->ViewProjectionMatrix.GetFrustumFarPlane(FarPlane);
		// Use the camera position and the selection box to create the bounding planes
		FPlane TopPlane(BoxPoint1, BoxPoint2, CamPoint); // Top Plane
		FPlane RightPlane(BoxPoint2, BoxPoint3, CamPoint); // Right Plane
		FPlane BottomPlane(BoxPoint3, BoxPoint4, CamPoint); // Bottom Plane
		FPlane LeftPlane(BoxPoint4, BoxPoint1, CamPoint); // Left Plane

		OutFrustum.Planes.Empty();
		OutFrustum.Planes.AddItem(NearPlane);
		OutFrustum.Planes.AddItem(FarPlane);
		OutFrustum.Planes.AddItem(TopPlane);
		OutFrustum.Planes.AddItem(RightPlane);
		OutFrustum.Planes.AddItem(BottomPlane);
		OutFrustum.Planes.AddItem(LeftPlane);
		OutFrustum.Init();
	}
	else
	{
		OutFrustum = View->ViewFrustum;
		OutFrustum.Init();
	}
}

/** Adds a hover effect to the passed in actor */
void FDragTool_FrustumSelect::AddHoverEffect( AActor& InActor )
{
	FEditorLevelViewportClient::FViewportHoverTarget HoverTarget( &InActor );
	FEditorLevelViewportClient::AddHoverEffect( HoverTarget );
	FEditorLevelViewportClient::HoveredObjects.Add( HoverTarget );
}

/** Removes a hover effect from the passed in actor */
void FDragTool_FrustumSelect::RemoveHoverEffect( AActor& InActor  )
{
	FEditorLevelViewportClient::FViewportHoverTarget HoverTarget( &InActor );
	FSetElementId Id = FEditorLevelViewportClient::HoveredObjects.FindId( HoverTarget );
	if( Id.IsValidId() )
	{
		FEditorLevelViewportClient::RemoveHoverEffect( HoverTarget );
		FEditorLevelViewportClient::HoveredObjects.Remove( Id );
	}
}

/** Adds a hover effect to the passed in bsp surface */
void FDragTool_FrustumSelect::AddHoverEffect( UModel& InModel, INT SurfIndex )
{
	FEditorLevelViewportClient::FViewportHoverTarget HoverTarget( &InModel, SurfIndex );
	FEditorLevelViewportClient::AddHoverEffect( HoverTarget );
	FEditorLevelViewportClient::HoveredObjects.Add( HoverTarget );
}

/** Removes a hover effect from the passed in bsp surface */
void FDragTool_FrustumSelect::RemoveHoverEffect( UModel& InModel, INT SurfIndex )
{
	FEditorLevelViewportClient::FViewportHoverTarget HoverTarget( &InModel, SurfIndex );
	FSetElementId Id = FEditorLevelViewportClient::HoveredObjects.FindId( HoverTarget );
	if( Id.IsValidId() )
	{
		FEditorLevelViewportClient::RemoveHoverEffect( HoverTarget );
		FEditorLevelViewportClient::HoveredObjects.Remove( Id );
	}
}