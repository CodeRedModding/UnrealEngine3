
#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	FModeTool.
-----------------------------------------------------------------------------*/

FModeTool::FModeTool()
{
	ID = MT_None;
	bUseWidget = 1;
	Settings = NULL;
}

FModeTool::~FModeTool()
{
	delete Settings;
}

FToolSettings* FModeTool::GetSettings()
{
	return Settings;
}

/*-----------------------------------------------------------------------------
	FModeTool_GeometryModify.
-----------------------------------------------------------------------------*/

FModeTool_GeometryModify::FModeTool_GeometryModify()
{
	ID = MT_GeometryModify;

	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Edit::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Extrude::StaticClass() ) );

	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Create::StaticClass() ) );
	//Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Lathe::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Delete::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Flip::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Split::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Triangulate::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Turn::StaticClass() ) );
	Modifiers.AddItem( ConstructObject<UGeomModifier>( UGeomModifier_Weld::StaticClass() ) );

	CurrentModifier = NULL;
}

FModeTool_GeometryModify::~FModeTool_GeometryModify()
{
}

void FModeTool_GeometryModify::SetCurrentModifier( UGeomModifier* InModifier )
{
	CurrentModifier = InModifier;
}

void FModeTool_GeometryModify::SelectNone()
{
	FEdModeGeometry* mode = ((FEdModeGeometry*)GEditorModeTools.GetCurrentMode());
	mode->SelectNone();
}

void FModeTool_GeometryModify::BoxSelect( FBox& InBox, UBOOL InSelect )
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	for( int o = 0 ; o < mode->GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &mode->GeomObjects(o);

		switch( seltype )
		{
			case GST_Poly:
			{
				for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
				{
					FGeomPoly* gp = &go->PolyPool(p);

					if( FPointBoxIntersection( go->GetActualBrush()->LocalToWorld().TransformFVector( gp->Mid ), InBox ) )
					{
						gp->Select( InSelect );
					}
				}
			}
			break;

			case GST_Edge:
			{
				for( INT e = 0 ; e < go->EdgePool.Num() ; ++e )
				{
					FGeomEdge* ge = &go->EdgePool(e);

					if( FPointBoxIntersection( go->GetActualBrush()->LocalToWorld().TransformFVector( ge->Mid ), InBox ) )
					{
						ge->Select( InSelect );
					}
				}
			}
			break;

			case GST_Vertex:
			{
				for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
				{
					FGeomVertex* gv = &go->VertexPool(v);

					if( FPointBoxIntersection( go->GetActualBrush()->LocalToWorld().TransformFVector( gv->Mid ), InBox ) )
					{
						gv->Select( InSelect );
					}
				}
			}
			break;
		}
	}
}

UBOOL FModeTool_GeometryModify::InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if( InViewportClient->Widget->CurrentAxis == AXIS_None )
	{
		return 0;
	}

	if( !CurrentModifier )
	{
		return 0;
	}

	// Geometry mode passes the input on to the current modifier.

	return CurrentModifier->InputDelta( InViewportClient, InViewport, InDrag, InRot, InScale );
}

UBOOL FModeTool_GeometryModify::StartModify()
{
	FEdModeGeometry* mode = ((FEdModeGeometry*)GEditorModeTools.GetCurrentMode());

	// Let the modifier do any set up work that it needs to.

	CurrentModifier->StartTrans();
	return CurrentModifier->StartModify();
}

UBOOL FModeTool_GeometryModify::EndModify()
{
	FEdModeGeometry* mode = ((FEdModeGeometry*)GEditorModeTools.GetCurrentMode());

	// Update the source data to match the current geometry data.

	mode->SendToSource();

	// Make sure the source data has remained viable.

	if( mode->FinalizeSourceData() )
	{
		// If the source data was modified, reconstruct the geometry data to reflect that.

		mode->GetFromSource();
	}

	CurrentModifier->EndTrans();

	// Let the modifier finish up

	CurrentModifier->EndModify();

	// Update internals.

	for( INT x = 0 ; x < mode->GeomObjects.Num() ; ++x )
	{
		FGeomObject* go = &mode->GeomObjects(x);
		go->ComputeData();
		GEditor->bspUnlinkPolys( go->GetActualBrush()->Brush );
	}

	return 1;
}

void FModeTool_GeometryModify::StartTrans()
{
	CurrentModifier->StartTrans();
}

void FModeTool_GeometryModify::EndTrans()
{
	CurrentModifier->EndTrans();
}

/*-----------------------------------------------------------------------------
	FModeTool_Texture.
-----------------------------------------------------------------------------*/

FModeTool_Texture::FModeTool_Texture()
{
	ID = MT_Texture;
	bUseWidget = 1;
}

FModeTool_Texture::~FModeTool_Texture()
{
}

UBOOL FModeTool_Texture::InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if( InViewportClient->Widget->CurrentAxis == AXIS_None )
	{
		return 0;
	}

	if( !InDrag.IsZero() )
	{
		// Ensure each polygon has a unique base point index.

		for(INT SurfaceIndex = 0;SurfaceIndex < GEditor->GetLevel()->Model->Surfs.Num();SurfaceIndex++)
		{
			FBspSurf&	Surf = GEditor->GetLevel()->Model->Surfs(SurfaceIndex);

			if(Surf.PolyFlags & PF_Selected)
			{
				FVector	Base = GEditor->GetLevel()->Model->Points(Surf.pBase);

				Surf.pBase = GEditor->GetLevel()->Model->Points.AddItem(Base);
			}
		}

		GEditor->polyTexPan( GEditor->GetLevel()->Model, InDrag.X, InDrag.Y, 0 );
	}

	if( !InRot.IsZero() )
	{
		FRotationMatrix rotmatrix( InRot );

		// Ensure each polygon has unique texture vector indices.

		for(INT SurfaceIndex = 0;SurfaceIndex < GEditor->GetLevel()->Model->Surfs.Num();SurfaceIndex++)
		{
			FBspSurf&	Surf = GEditor->GetLevel()->Model->Surfs(SurfaceIndex);

			if(Surf.PolyFlags & PF_Selected)
			{
				FVector	TextureU = GEditor->GetLevel()->Model->Vectors(Surf.vTextureU),
						TextureV = GEditor->GetLevel()->Model->Vectors(Surf.vTextureV);
				
				TextureU = rotmatrix.TransformFVector( TextureU );
				TextureV = rotmatrix.TransformFVector( TextureV );

				Surf.vTextureU = GEditor->GetLevel()->Model->Vectors.AddItem(TextureU);
				Surf.vTextureV = GEditor->GetLevel()->Model->Vectors.AddItem(TextureV);

				GEditor->polyUpdateMaster( GEditor->GetLevel()->Model, SurfaceIndex, 1 );
			}
		}
	}

	if( !InScale.IsZero() )
	{
		// Ensure each polygon has unique texture vector indices.

		for(INT SurfaceIndex = 0;SurfaceIndex < GEditor->GetLevel()->Model->Surfs.Num();SurfaceIndex++)
		{
			FBspSurf&	Surf = GEditor->GetLevel()->Model->Surfs(SurfaceIndex);

			if(Surf.PolyFlags & PF_Selected)
			{
				FVector	TextureU = GEditor->GetLevel()->Model->Vectors(Surf.vTextureU),
						TextureV = GEditor->GetLevel()->Model->Vectors(Surf.vTextureV);

				Surf.vTextureU = GEditor->GetLevel()->Model->Vectors.AddItem(TextureU);
				Surf.vTextureV = GEditor->GetLevel()->Model->Vectors.AddItem(TextureV);
			}
		}

		FLOAT ScaleU = InScale.X / GEditor->Constraints.GetGridSize();
		FLOAT ScaleV = InScale.Y / GEditor->Constraints.GetGridSize();

		ScaleU = 1.f - (ScaleU / 100.f);
		ScaleV = 1.f - (ScaleV / 100.f);

		GEditor->polyTexScale( GEditor->GetLevel()->Model, ScaleU, 0.f, 0.f, ScaleV, 0 );
	}

	return 1;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
