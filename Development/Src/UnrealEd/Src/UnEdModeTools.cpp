/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "SurfaceIterators.h"
#include "BSPOps.h"

/*-----------------------------------------------------------------------------
	FModeTool.
-----------------------------------------------------------------------------*/

FModeTool::FModeTool():
	ID( MT_None ),
	bUseWidget( 1 ),
	Settings( NULL )
{}

FModeTool::~FModeTool()
{
	delete Settings;
}

void FModeTool::DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
}

void FModeTool::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
}

/*-----------------------------------------------------------------------------
	FModeTool_GeometryModify.
-----------------------------------------------------------------------------*/

FModeTool_GeometryModify::FModeTool_GeometryModify()
{
	ID = MT_GeometryModify;

	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Edit::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Extrude::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Clip::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Pen::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Lathe::StaticClass() ) );

	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Create::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Delete::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Flip::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Split::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Triangulate::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Optimize::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Turn::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Weld::StaticClass() ) );

	CurrentModifier = NULL;
	
	bGeomModified = FALSE;
}

void FModeTool_GeometryModify::SelectNone()
{
	FEdModeGeometry* mode = ((FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry ));
	mode->SelectNone();
}

/** @return		TRUE if something was selected/deselected, FALSE otherwise. */
UBOOL FModeTool_GeometryModify::BoxSelect( FBox& InBox, UBOOL InSelect )
{
	UBOOL bResult = FALSE;
	if( GEditorModeTools().IsModeActive( EM_Geometry ) )
	{
		FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );

		for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
		{
			FGeomObject* go = *Itor;

			// Only verts for box selection

			for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
			{
				FGeomVertex& gv = go->VertexPool(v);
				if( FPointBoxIntersection( go->GetActualBrush()->LocalToWorld().TransformFVector( gv.GetMid() ), InBox ) )
				{
					gv.Select( InSelect );
					bResult = TRUE;
				}
			}
		}
	}
	return bResult;
}

/** @return		TRUE if something was selected/deselected, FALSE otherwise. */
UBOOL FModeTool_GeometryModify::FrustumSelect( const FConvexVolume& InFrustum, UBOOL InSelect /* = TRUE */ )
{
	UBOOL bResult = FALSE;
	if( GEditorModeTools().IsModeActive( EM_Geometry ) )
	{
		FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );

		for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
		{
			FGeomObject* go = *Itor;

			// Check each vertex to see if its inside the frustum
			for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
			{
				FGeomVertex& gv = go->VertexPool(v);
				if( InFrustum.IntersectBox( go->GetActualBrush()->LocalToWorld().TransformFVector( gv.GetMid() ), FVector(0,0,0) ) )
				{
					gv.Select( InSelect );
					bResult = TRUE;
				}
			}
		}
	}
	return bResult;
}

void FModeTool_GeometryModify::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	if( CurrentModifier )
	{
		CurrentModifier->Tick( ViewportClient, DeltaTime );
	}
}

/**
 * @return		TRUE if the delta was handled by this editor mode tool.
 */
UBOOL FModeTool_GeometryModify::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	UBOOL bResult = FALSE;
	if( InViewportClient->Widget->GetCurrentAxis() != AXIS_None )
	{
		// Geometry mode passes the input on to the current modifier.
		if( CurrentModifier )
		{
			bResult = CurrentModifier->InputDelta( InViewportClient, InViewport, InDrag, InRot, InScale );
		}
	}
	return bResult;
}

UBOOL FModeTool_GeometryModify::StartModify()
{
	// Let the modifier do any set up work that it needs to.
	if( CurrentModifier != NULL )
	{
		//Store the current state of the brush so that we can return to upon faulty operation
		CurrentModifier->CacheBrushState();

		return CurrentModifier->StartModify();
	}

	// Clear modified flag, so we can track if something actually changes before EndModify
	bGeomModified = FALSE;

	// No modifier to start
	return FALSE;
}

UBOOL FModeTool_GeometryModify::EndModify()
{
	// Let the modifier finish up.
	if( CurrentModifier != NULL )
	{
		FEdModeGeometry* mode = ((FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry));

		// Update the source data to match the current geometry data.
		mode->SendToSource();

		// Make sure the source data has remained viable.
		if( mode->FinalizeSourceData() )
		{
			// If the source data was modified, reconstruct the geometry data to reflect that.
			mode->GetFromSource();
		}

		CurrentModifier->EndModify();

		// Update internals.
		for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
		{
			FGeomObject* go = *Itor;
			go->ComputeData();
			FBSPOps::bspUnlinkPolys( go->GetActualBrush()->Brush );			
			
			// If geometry was actually changed, call PostEditBrush
			if(bGeomModified)
			{
				ABrush* Brush = go->GetActualBrush();
				if(Brush)
				{
					Brush->PostEditBrush();
					GEditor->RebuildAlteredBSP(); // Brush has been altered, update the Bsp
				}
				bGeomModified = FALSE;
			}
			
		}		
	}

	return 1;
}

void FModeTool_GeometryModify::StartTrans()
{
	if( CurrentModifier != NULL )
	{
		CurrentModifier->StartTrans();
	}
}

void FModeTool_GeometryModify::EndTrans()
{
	if( CurrentModifier != NULL )
	{
		CurrentModifier->EndTrans();
	}
}

/**
 * @return		TRUE if the key was handled by this editor mode tool.
 */
UBOOL FModeTool_GeometryModify::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	check( GEditorModeTools().IsModeActive( EM_Geometry ) );
	// Give the current modifier a chance to handle this first
	if( CurrentModifier && CurrentModifier->InputKey( ViewportClient, Viewport, Key, Event ) )
	{
		return TRUE;
	}

	if (Key == KEY_Escape)
	{
		// Hitting ESC will deselect any subobjects first.  If no subobjects are selected, then it will
		// deselect the brushes themselves.

		FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode( EM_Geometry );
		UBOOL bHadSubObjectSelections = (mode->GetSelectionState() > 0) ? TRUE : FALSE;

		for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
		{
			FGeomObject* go = *Itor;

			for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
			{
				FGeomPoly& gp = go->PolyPool(p);
				if( gp.IsSelected() )
				{
					gp.Select( FALSE );
					bHadSubObjectSelections = TRUE;
				}
			}

			for( INT e = 0 ; e < go->EdgePool.Num() ; ++e )
			{
				FGeomEdge& ge = go->EdgePool(e);
				if( ge.IsSelected() )
				{
					ge.Select( FALSE );
					bHadSubObjectSelections = TRUE;
				}
			}

			for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
			{
				FGeomVertex& gv = go->VertexPool(v);
				if( gv.IsSelected() )
				{
					gv.Select( FALSE );
					bHadSubObjectSelections = TRUE;
				}
			}
		}

		if( bHadSubObjectSelections )
		{
			GEditor->RedrawAllViewports();
			return TRUE;
		}
	}
	else
	{
		return FModeTool::InputKey( ViewportClient, Viewport, Key, Event );
	}

	return FALSE;
}

void FModeTool_GeometryModify::DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
	// Give the current modifier a chance to draw a HUD

	if( CurrentModifier )
	{
		CurrentModifier->DrawHUD( ViewportClient, Viewport, View, Canvas );
	}
}

void FModeTool_GeometryModify::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	// Give the current modifier a chance to render

	if( CurrentModifier )
	{
		CurrentModifier->Render( View, Viewport, PDI );
	}
}

/*-----------------------------------------------------------------------------
	FModeTool_StaticMesh.
-----------------------------------------------------------------------------*/

FModeTool_StaticMesh::FModeTool_StaticMesh()
{
	ID = MT_StaticMesh;
	bUseWidget = 1;

	StaticMeshModeOptions = ConstructObject<UStaticMeshMode_Options>( UStaticMeshMode_Options::StaticClass() );
}

void FModeTool_StaticMesh::Serialize(FArchive& Ar)
{
	Ar << StaticMeshModeOptions;
}

/**
 * Applies the stored static mesh options to the given static mesh actor.
 *
 * @param	MeshActor	The static mesh actor to apply settings to, this now applies to SpeedTrees also.
 */
void FModeTool_StaticMesh::ApplySettings( AActor* MeshActor )
{
	if( MeshActor )
	{
		// Add in any pre-rotation the user has specified first.

		MeshActor->Rotation += StaticMeshModeOptions->PreRotation;

		// Custom settings

		MeshActor->SetCollisionType( StaticMeshModeOptions->CollisionType );

		//
		// Rotation
		//

		FRotator Rotation = FRotator(
			RandRange( StaticMeshModeOptions->RotationMin.Pitch, StaticMeshModeOptions->RotationMax.Pitch ),
			RandRange( StaticMeshModeOptions->RotationMin.Yaw, StaticMeshModeOptions->RotationMax.Yaw ),
			RandRange( StaticMeshModeOptions->RotationMin.Roll, StaticMeshModeOptions->RotationMax.Roll )
			);

		// Multiplying the current actor rotation by the extra rotation value from the user gives us a usable rotator
		FQuat ActorRotation = FQuat( MeshActor->Rotation );
		FQuat ExtraRotationFromUser = FQuat( Rotation );

		MeshActor->Rotation = FRotator( ActorRotation * ExtraRotationFromUser );

		//
		// DrawScale3D
		//

		FVector DrawScale3D = FVector(
			RandRange( StaticMeshModeOptions->Scale3DMin.X, StaticMeshModeOptions->Scale3DMax.X ),
			RandRange( StaticMeshModeOptions->Scale3DMin.Y, StaticMeshModeOptions->Scale3DMax.Y ),
			RandRange( StaticMeshModeOptions->Scale3DMin.Z, StaticMeshModeOptions->Scale3DMax.Z )
			);

		// Sanity checking as a scale of 0 is pretty useless.
		if( DrawScale3D.X == 0.0f ) { DrawScale3D.X = 1.0f; }
		if( DrawScale3D.Y == 0.0f ) { DrawScale3D.Y = 1.0f; }
		if( DrawScale3D.Z == 0.0f ) { DrawScale3D.Z = 1.0f; }

		// Snaps the drawscale values to be a single decimal place so they don't look ridiculous
		DrawScale3D.X = appTrunc( DrawScale3D.X * 10.f ) / 10.0f;
		DrawScale3D.Y = appTrunc( DrawScale3D.Y * 10.f ) / 10.0f;
		DrawScale3D.Z = appTrunc( DrawScale3D.Z * 10.f ) / 10.0f;

		MeshActor->DrawScale3D = DrawScale3D;

		//
		// DrawScale
		//

		FLOAT DrawScale = RandRange( StaticMeshModeOptions->ScaleMin, StaticMeshModeOptions->ScaleMax );

		// Sanity checking as a scale of 0 is pretty useless.
		if( DrawScale == 0.0f ) { DrawScale = 1.0f; }

		// Snaps the drawscale value to be a single decimal place so they don't look ridiculous
		DrawScale = appTrunc( DrawScale * 10.f ) / 10.0f;

		MeshActor->DrawScale = DrawScale;
	}
}

/*-----------------------------------------------------------------------------
	FModeTool_Texture.
-----------------------------------------------------------------------------*/

FModeTool_Texture::FModeTool_Texture()
{
	ID = MT_Texture;
	bUseWidget = 1;
	PreviousInputDrag = FVector(0.0f);
}

/**
 * @return		TRUE if the delta was handled by this editor mode tool.
 */
UBOOL FModeTool_Texture::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if( InViewportClient->Widget->GetCurrentAxis() == AXIS_None )
	{
		return FALSE;
	}

	// calculate delta drag for this tick for the call to GEditor->polyTexPan below which is using relative (delta) mode
	FVector deltaDrag = InDrag;
	if (TRUE == InViewportClient->IsPerspective())
	{
		// perspective viewports pass the absolute drag so subtract the last tick's drag value to get the delta
		deltaDrag -= PreviousInputDrag;
	}
	PreviousInputDrag = InDrag;

	if( !deltaDrag.IsZero() )
	{
		// Ensure each polygon has a unique base point index.
		FOR_EACH_UMODEL;
			for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
			{
				FBspSurf& Surf = Model->Surfs(SurfaceIndex);

				if(Surf.PolyFlags & PF_Selected)
				{
					const FVector Base = Model->Points(Surf.pBase);
					Surf.pBase = Model->Points.AddItem(Base);
				}
			}
			FMatrix Mat = GEditorModeTools().GetCustomDrawingCoordinateSystem();
			FVector UVW = Mat.InverseTransformNormal( deltaDrag );  // InverseTransformNormal because Mat is the transform from the surface/widget's coords to world coords
			GEditor->polyTexPan( Model, UVW.X, UVW.Y, 0 );  // 0 is relative mode because UVW is made from deltaDrag - the user input since the last tick
		END_FOR_EACH_UMODEL;
	}

	if( !InRot.IsZero() )
	{
		const FRotationMatrix RotationMatrix( InRot );

		// Ensure each polygon has unique texture vector indices.
		for ( TSelectedSurfaceIterator<> It ; It ; ++It )
		{
			FBspSurf* Surf = *It;
			UModel* Model = It.GetModel();

			FVector	TextureU = Model->Vectors(Surf->vTextureU);
			FVector TextureV = Model->Vectors(Surf->vTextureV);

			TextureU = RotationMatrix.TransformFVector( TextureU );
			TextureV = RotationMatrix.TransformFVector( TextureV );

			Surf->vTextureU = Model->Vectors.AddItem(TextureU);
			Surf->vTextureV = Model->Vectors.AddItem(TextureV);

			GEditor->polyUpdateMaster( Model, It.GetSurfaceIndex(), 1 );
		}
	}

	if( !InScale.IsZero() )
	{
		FLOAT ScaleU = InScale.X / GEditor->Constraints.GetGridSize();
		FLOAT ScaleV = InScale.Y / GEditor->Constraints.GetGridSize();

		ScaleU = 1.f - (ScaleU / 100.f);
		ScaleV = 1.f - (ScaleV / 100.f);

		// Ensure each polygon has unique texture vector indices.
		FOR_EACH_UMODEL;
			for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
			{
				FBspSurf& Surf = Model->Surfs(SurfaceIndex);
				if(Surf.PolyFlags & PF_Selected)
				{
					const FVector TextureU = Model->Vectors(Surf.vTextureU);
					const FVector TextureV = Model->Vectors(Surf.vTextureV);

					Surf.vTextureU = Model->Vectors.AddItem(TextureU);
					Surf.vTextureV = Model->Vectors.AddItem(TextureV);
				}
			}
			GEditor->polyTexScale( Model, ScaleU, 0.f, 0.f, ScaleV, FALSE );
		END_FOR_EACH_UMODEL;

	}

	return TRUE;
}
