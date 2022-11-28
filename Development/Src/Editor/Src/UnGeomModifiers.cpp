
#include "EditorPrivate.h"

IMPLEMENT_CLASS(UGeomModifier)
IMPLEMENT_CLASS(UGeomModifier_Edit)
IMPLEMENT_CLASS(UGeomModifier_Extrude)
IMPLEMENT_CLASS(UGeomModifier_Lathe)
IMPLEMENT_CLASS(UGeomModifier_Delete)
IMPLEMENT_CLASS(UGeomModifier_Create)
IMPLEMENT_CLASS(UGeomModifier_Flip)
IMPLEMENT_CLASS(UGeomModifier_Split)
IMPLEMENT_CLASS(UGeomModifier_Triangulate)
IMPLEMENT_CLASS(UGeomModifier_Turn)
IMPLEMENT_CLASS(UGeomModifier_Weld)

/*------------------------------------------------------------------------------
	UGeomModifier
------------------------------------------------------------------------------*/

UBOOL UGeomModifier::InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();

	if( !bInitialized )
	{
		if( !((FGeometryToolSettings*)mode->GetSettings())->bAffectWidgetOnly )
		{
			Initialize();
		}
		bInitialized = 1;
	}

	return 0;
}

void UGeomModifier::GeomError( FString InMsg )
{
	appMsgf( 0, *FString::Printf( TEXT("Modifier (%s) : %s"), *GetModifierDescription(), *InMsg ) );
}

UBOOL UGeomModifier::StartModify()
{
	bInitialized = 0;

	return 1;
}

UBOOL UGeomModifier::EndModify()
{
	GCallback->Send( CALLBACK_RedrawAllViewports );

	return 1;
}

void UGeomModifier::StartTrans()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();

	// Record the current selection list into the selected brushes so that inf

	for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &mode->GeomObjects(o);
		go->CompileSelectionOrder();

		ABrush* Actor = go->GetActualBrush();

		Actor->SavedSelections.Empty();
		FGeomSelection* gs = NULL;

		for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
		{
			FGeomVertex* gv = &go->VertexPool(v);
			if( gv->IsSelected() )
			{
				gs = new( Actor->SavedSelections )FGeomSelection;
				gs->Type = GST_Vertex;
				gs->Index = v;
				gs->SelectionIndex = gv->GetSelectionIndex();
				gs->SelStrength = gv->SelStrength;
			}
		}
		for( INT e = 0 ; e < go->EdgePool.Num() ; ++e )
		{
			FGeomEdge* ge = &go->EdgePool(e);
			if( ge->IsSelected() )
			{
				gs = new( Actor->SavedSelections )FGeomSelection;
				gs->Type = GST_Edge;
				gs->Index = e;
				gs->SelectionIndex = ge->GetSelectionIndex();
				gs->SelStrength = ge->SelStrength;
			}
		}
		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			if( gp->IsSelected() )
			{
				gs = new( Actor->SavedSelections )FGeomSelection;
				gs->Type = GST_Poly;
				gs->Index = p;
				gs->SelectionIndex = gp->GetSelectionIndex();
				gs->SelStrength = gp->SelStrength;
			}
		}
	}

	// Start the transaction

	GEditor->Trans->Begin( *FString::Printf( TEXT("Modifer [%s]" ), *GetModifierDescription() ) );

	// Mark all selected brushes as modified

	for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &mode->GeomObjects(o);
		ABrush* Actor = go->GetActualBrush();

		Actor->Modify();
		Actor->Brush->Modify();
		Actor->Brush->Polys->Element.ModifyAllItems();
	}
}

void UGeomModifier::EndTrans()
{
	if( GEditor->Trans->IsActive() )
	{
		GEditor->Trans->End();
	}
}

/*------------------------------------------------------------------------------
	UGeomModifier_Edit
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Edit::InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if( UGeomModifier::InputDelta( InViewportClient, InViewport, InDrag, InRot, InScale ) )
	{
		return 1;
	}

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	TArray<FGeomVertex*> VertexList;

	/**
	 * All geometry objects can be manipulated by transforming the vertices that make
	 * them up.  So based on the type of thing we're editing, we need to dig for those
	 * vertices a little differently.
	 */

	for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &mode->GeomObjects(o);

		switch( seltype )
		{
			case GST_Object:
				for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
				{
					VertexList.AddUniqueItem( &go->VertexPool(v) );
				}
				break;

			case GST_Poly:
				for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
				{
					FGeomPoly* gp = &go->PolyPool(p);
					if( gp->IsSelected() )
					{
						for( INT e = 0 ; e < gp->EdgeIndices.Num() ; ++e )
						{
							FGeomEdge* ge = &go->EdgePool( gp->EdgeIndices(e) );
							VertexList.AddUniqueItem( &go->VertexPool( ge->VertexIndices[0] ) );
							VertexList.AddUniqueItem( &go->VertexPool( ge->VertexIndices[1] ) );
						}
					}
				}
				break;

			case GST_Edge:
				for( INT e = 0 ; e < go->EdgePool.Num() ; ++e )
				{
					FGeomEdge* ge = &go->EdgePool(e);
					if( ge->IsSelected() )
					{
						VertexList.AddUniqueItem( &go->VertexPool( ge->VertexIndices[0] ) );
						VertexList.AddUniqueItem( &go->VertexPool( ge->VertexIndices[1] ) );
					}
				}
				break;

			case GST_Vertex:
				for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
				{
					FGeomVertex* gv = &go->VertexPool(v);
					if( gv->IsSelected() )
					{
						VertexList.AddUniqueItem( gv );
					}
				}
				break;
		}
	}

	/**
	 * We first generate a list of unique vertices and then transform that list
	 * in one shot.  This prevents vertices from being touched more than once (which
	 * would result in them transforming x times as fast as others).
	 */

	for( INT x = 0 ; x < VertexList.Num() ; ++x )
	{
		mode->ApplyToVertex( VertexList(x), InDrag, InRot, InScale );
	}

	// If we didn't move any vertices, then tell the caller that we didn't handle the input.
	// This allows LDs to drag brushes around in geometry mode as along as no geometry
	// objects are selected.

	if( !VertexList.Num() )
	{
		return 0;
	}
	
	return 1;
}

/*------------------------------------------------------------------------------
	UGeomModifier_Extrude
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Extrude::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Object:
		case GST_Edge:
		case GST_Vertex:
			return 0;
	}

	return 1;
}

void UGeomModifier_Extrude::Initialize()
{
	UGeomModifier::Initialize();

	Apply( GEditor->Constraints.GetGridSize(), 1 );
}

void UGeomModifier_Extrude::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();

	// When applying via the keyboard, we force the local coordinate system.

	ECoordSystem SaveCS = GEditorModeTools.CoordSystem;
	GEditorModeTools.CoordSystem = COORD_Local;

	mode->ModifierWindow->PropertyWindow->FinalizeValues();
	Apply( Length, Segments );

	GEditorModeTools.CoordSystem = SaveCS;

	EndModify();

	EndTrans();
}

void UGeomModifier_Extrude::Apply( int InLength, int InSegments )
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	// Force user input to be valid

	InLength = Max( 1, InLength );
	InSegments = Max( 1, InSegments );

	// Do it

	for( INT s = 0 ; s < InSegments ; ++s )
	{
		for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
		{
			FGeomObject* go = &mode->GeomObjects(o);

			switch( seltype )
			{
				case GST_Poly:
				{
					go->SendToSource();

					TArray<INT> Selections;

					for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
					{
						FGeomPoly* gp = &go->PolyPool(p);

						if( gp->IsSelected() )
						{
							Selections.AddItem( p );
							FPoly OldPoly = *gp->GetActualPoly();

							FVector Normal = mode->GetWidgetNormalFromCurrentAxis( gp );

							// Move the existing poly along the normal by InLength units.

							for( INT v = 0 ; v < gp->GetActualPoly()->NumVertices ; ++v )
							{
								FVector* vtx = &gp->GetActualPoly()->Vertex[v];

								*vtx += Normal * InLength;
							}

							gp->GetActualPoly()->Base += Normal * InLength;

							// Create new polygons to connect the existing one back to the brush

							for( INT v = 0 ; v < OldPoly.NumVertices ; ++v )
							{
								// Create new polygons from the edges of the old one

								FVector v0 = OldPoly.Vertex[v],
									v1 = OldPoly.Vertex[ v+1 == OldPoly.NumVertices ? 0 : v+1 ];

								FPoly* NewPoly = new( gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();

								NewPoly->Init();
								NewPoly->Vertex[0] = v0;
								NewPoly->Vertex[1] = v1;
								NewPoly->Vertex[2] = v1 + (Normal * InLength);
								NewPoly->Vertex[3] = v0 + (Normal * InLength);
								NewPoly->NumVertices = 4;

								NewPoly->Normal = FVector(0,0,0);
								NewPoly->Base = v0;
								NewPoly->PolyFlags = OldPoly.PolyFlags;
								NewPoly->Material = OldPoly.Material;
								NewPoly->Finalize(go->GetActualBrush(),1);
							}
						}
					}

					go->FinalizeSourceData();
					go->GetFromSource();

					for( INT x = 0 ; x < Selections.Num() ; ++x )
					{
						go->PolyPool( Selections(x) ).Select(1);
					}
				}
				break;
			}
		}
	}
}

/*------------------------------------------------------------------------------
	UGeomModifier_Lathe
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Lathe::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Object:
		case GST_Edge:
		case GST_Vertex:
			return 0;
	}

	return 1;
}

void UGeomModifier_Lathe::Initialize()
{
	UGeomModifier::Initialize();

	Apply( TotalSegments, Segments, (EAxis)Axis );
}

void UGeomModifier_Lathe::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();

	// When applying via the keyboard, we force the local coordinate system.

	ECoordSystem SaveCS = GEditorModeTools.CoordSystem;
	GEditorModeTools.CoordSystem = COORD_Local;

	mode->ModifierWindow->PropertyWindow->FinalizeValues();
	Apply( TotalSegments, Segments, (EAxis)Axis );

	GEditorModeTools.CoordSystem = SaveCS;

	EndModify();

	EndTrans();
}

void UGeomModifier_Lathe::Apply( int InTotalSegments, int InSegments, EAxis InAxis )
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	// Force user input to be valid

	InTotalSegments = Max( 3, InTotalSegments );
	InSegments = Clamp( InSegments, 1, InTotalSegments );

	// Do it

	for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &mode->GeomObjects(o);

		switch( seltype )
		{
			case GST_Poly:
			{
				go->SendToSource();

				for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
				{
					FGeomPoly* gp = &go->PolyPool(p);
					TArray<FVector> Vertices;

					if( gp->IsSelected() )
					{

						// Compute the angle step

						FLOAT Angle = 65536.f / (FLOAT)InTotalSegments;

						// If the widget is on the other side of the normal plane for this poly

						//FPlane plane(  )

						//Angle *= -1;

						FPoly OldPoly = *gp->GetActualPoly();
						for( INT s = 0 ; s <= InSegments ; ++s )
						{
							FMatrix matrix = FMatrix::Identity;
							switch( InAxis )
							{
								case AXIS_X:
									matrix = FRotationMatrix( FRotator( Angle*s, 0, 0 ) );
									break;
								case AXIS_Y:
									matrix = FRotationMatrix( FRotator( 0, Angle*s, 0 ) );
									break;
								default:
									matrix = FRotationMatrix( FRotator( 0, 0, Angle*s ) );
									break;
							}

							for( INT v = 0 ; v < OldPoly.NumVertices ; ++v )
							{
								FVector Wk = go->GetActualBrush()->LocalToWorld().TransformFVector( OldPoly.Vertex[v] );

								Wk -= GEditorModeTools.SnappedLocation;
								Wk = matrix.TransformFVector( Wk );
								Wk += GEditorModeTools.SnappedLocation;

								Wk = go->GetActualBrush()->WorldToLocal().TransformFVector( Wk );

								Vertices.AddUniqueItem( Wk );
							}
						}

						// Create the new polygons

						for( INT s = 0 ; s < InSegments ; ++s )
						{
							for( INT v = 0 ; v < OldPoly.NumVertices ; ++v )
							{
								FPoly* NewPoly = new( go->GetActualBrush()->Brush->Polys->Element )FPoly();
								NewPoly->Init();

								INT idx0 = (s * OldPoly.NumVertices) + v;
								INT idx1 = (s * OldPoly.NumVertices) + ((v+1 < OldPoly.NumVertices) ? v+1 : 0);
								INT idx2 = idx1 + OldPoly.NumVertices;
								INT idx3 = idx0 + OldPoly.NumVertices;

								NewPoly->Vertex[0] = Vertices( idx0 );
								NewPoly->Vertex[1] = Vertices( idx1 );
								NewPoly->Vertex[2] = Vertices( idx2 );
								NewPoly->Vertex[3] = Vertices( idx3 );

								NewPoly->NumVertices = 4;

							}
						}

						// Create a cap polygon (if we aren't doing a complete circle)

						if( InSegments < InTotalSegments )
						{
							FPoly* NewPoly = new( go->GetActualBrush()->Brush->Polys->Element )FPoly();
							NewPoly->Init();

							for( INT v = 0 ; v < OldPoly.NumVertices ; ++v )
							{
								NewPoly->Vertex[v] = Vertices( Vertices.Num() - OldPoly.NumVertices + v );
							}

							NewPoly->NumVertices = OldPoly.NumVertices;

							NewPoly->Normal = FVector(0,0,0);
							NewPoly->Base = Vertices(0);
							NewPoly->PolyFlags = 0;

							NewPoly->Finalize(go->GetActualBrush(),1);
						}

						// Mark the original polygon for deletion later

						go->GetActualBrush()->Brush->Polys->Element( gp->ActualPolyIndex ).PolyFlags |= PF_GeomMarked;
					}
				}
			}
			break;
		}

		// Remove any polys that were marked for deletion

		for( INT p = 0 ; p < go->GetActualBrush()->Brush->Polys->Element.Num() ; ++p )
		{
			if( (go->GetActualBrush()->Brush->Polys->Element( p ).PolyFlags&PF_GeomMarked) > 0 )
			{
				go->GetActualBrush()->Brush->Polys->Element.Remove( p );
				p = -1;
			}
		}
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();
}

/*------------------------------------------------------------------------------
	UGeomModifier_Delete
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Delete::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Poly:
		case GST_Vertex:
			return 1;
	}

	return 0;
}

void UGeomModifier_Delete::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
	{
		FGeomObject* go = &mode->GeomObjects(o);

		switch( seltype )
		{
			case GST_Poly:
			{
				for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
				{
					FGeomPoly* gp = &go->PolyPool(p);

					if( gp->IsSelected() )
					{
						gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element( gp->ActualPolyIndex ).PolyFlags |= PF_GeomMarked;
					}
				}

				for( INT p = 0 ; p < go->GetActualBrush()->Brush->Polys->Element.Num() ; ++p )
				{
					if( (go->GetActualBrush()->Brush->Polys->Element( p ).PolyFlags&PF_GeomMarked) > 0 )
					{
						go->GetActualBrush()->Brush->Polys->Element.Remove( p );
						p = -1;
					}
				}
			}
			break;

			case GST_Vertex:
			{
				for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
				{
					FGeomVertex* gv = &go->VertexPool(v);

					if( gv->IsSelected() )
					{
						for( INT x = 0 ; x < gv->GetParentObject()->GetActualBrush()->Brush->Polys->Element.Num() ; ++x )
						{
							FPoly* Poly = &gv->GetParentObject()->GetActualBrush()->Brush->Polys->Element(x);
							Poly->RemoveVertex( *gv );
						}
					}
				}
			}
			break;
		}
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();
	EndTrans();

	EndModify();
}

/*------------------------------------------------------------------------------
	UGeomModifier_Create
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Create::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Vertex:
			return 1;
	}

	return 0;
}

void UGeomModifier_Create::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Vertex:
		{
			for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
			{
				FGeomObject* go = &mode->GeomObjects(o);

				go->CompileSelectionOrder();

				// Create an ordered list of vertices based on the selection order.

				TArray<FGeomVertex*> Verts;
				for( INT x = 0 ; x < go->SelectionOrder.Num() ; ++x )
				{
					FGeomBase* obj = go->SelectionOrder(x);
					if( obj->IsVertex() )
					{
						Verts.AddItem( (FGeomVertex*)obj );
					}
				}

				if( Verts.Num() > 2 )
				{
					// Create new geometry based on the selected vertices

					FPoly* NewPoly = new( go->GetActualBrush()->Brush->Polys->Element )FPoly();

					NewPoly->Init();

					for( INT x = 0 ; x < Verts.Num() ; ++x )
					{
						FGeomVertex* gv = Verts(x);

						NewPoly->Vertex[x] = *gv;
					}

					NewPoly->NumVertices = Verts.Num();

					NewPoly->Normal = FVector(0,0,0);
					NewPoly->Base = *Verts(0);
					NewPoly->PolyFlags = 0;
				}
			}
		}
		break;
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();
	EndTrans();

	EndModify();
}

/*------------------------------------------------------------------------------
	UGeomModifier_Flip
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Flip::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Poly:
			return 1;
	}

	return 0;
}

void UGeomModifier_Flip::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Poly:
		{
			for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
			{
				FGeomObject* go = &mode->GeomObjects(o);

				for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
				{
					FGeomPoly* gp = &go->PolyPool(p);

					if( gp->IsSelected() )
					{
						FPoly* Poly = &go->GetActualBrush()->Brush->Polys->Element( gp->ActualPolyIndex );
						Poly->Reverse();
					}
				}
			}
		}
		break;
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();
	EndTrans();

	EndModify();
}

/*------------------------------------------------------------------------------
	UGeomModifier_Split
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Split::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Vertex:
		case GST_Edge:
			return 1;
	}

	return 0;
}

void UGeomModifier_Split::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Vertex:
		{
			for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
			{
				FGeomObject* go = &mode->GeomObjects(o);

				FGeomVertex* SelectedVerts[2];

				// Make sure only 2 vertices are selected

				INT Count = 0;
				for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
				{
					FGeomVertex* gv = &go->VertexPool(v);

					if( gv->IsSelected() )
					{
						SelectedVerts[Count] = gv;
						Count++;
						if( Count == 2 )
						{
							break;
						}
					}
				}

				if( Count != 2 )
				{
					GeomError( TEXT("Requires 2 selected vertices." ) );
					break;
				}

				// Find the polygon we are splitting.

				FGeomPoly* PolyToSplit = NULL;
				for( INT i = 0 ; i < SelectedVerts[0]->ParentPolyIndices.Num() ; ++i )
				{
					if( SelectedVerts[1]->ParentPolyIndices.FindItemIndex( SelectedVerts[0]->ParentPolyIndices(i) ) != INDEX_NONE )
					{
						PolyToSplit = &go->PolyPool( SelectedVerts[0]->ParentPolyIndices(i) );
						break;
					}
				}

				if( !PolyToSplit )
				{
					GeomError( TEXT("The selected vertices must belong to the same polygon." ) );
					break;
				}

				// Create a plane that cross the selected vertices and split the polygon with it.

				FPoly SavePoly = *PolyToSplit->GetActualPoly();

				FVector PlaneBase = *SelectedVerts[0];
				FVector v0 = *SelectedVerts[0],
					v1 = *SelectedVerts[1],
					v2 = v1 + (PolyToSplit->Normal * 16);
				FVector PlaneNormal = FPlane( v0, v1, v2 );

				FPoly* Front = new( PolyToSplit->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();
				Front->Init();
				FPoly* Back = new( PolyToSplit->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();
				Back->Init();

				INT result = PolyToSplit->GetActualPoly()->SplitWithPlane( PlaneBase, PlaneNormal, Front, Back, 1 );
				check( result == SP_Split );	// Any other result means that the splitting plane was created incorrectly

				Front->Base = SavePoly.Base;
				Front->TextureU = SavePoly.TextureU;
				Front->TextureV = SavePoly.TextureV;
				Front->Material = SavePoly.Material;

				Back->Base = SavePoly.Base;
				Back->TextureU = SavePoly.TextureU;
				Back->TextureV = SavePoly.TextureV;
				Back->Material = SavePoly.Material;

				// Remove the old polygon from the brush

				go->GetActualBrush()->Brush->Polys->Element.Remove( PolyToSplit->ActualPolyIndex );
			}
		}
		break;

		case GST_Edge:
		{
			for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
			{
				FGeomObject* go = &mode->GeomObjects(o);

				TArray<FGeomEdge> Edges;
				go->CompileUniqueEdgeArray( &Edges );

				// Check to see that the selected edges belong to the same polygon

				FGeomPoly* PolyToSplit = NULL;

				for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
				{
					FGeomPoly* gp = &go->PolyPool(p);

					UBOOL bOK = 1;
					for( INT s = 0 ; s < Edges.Num() ; ++s )
					{
						if( Edges(s).ParentPolyIndices.FindItemIndex( p ) == INDEX_NONE )
						{
							bOK = 0;
							break;
						}
					}

					if( bOK )
					{
						PolyToSplit = gp;
						break;
					}
				}

				if( !PolyToSplit )
				{
					GeomError( TEXT("Selected edges must all belong to the same polygon." ) );
					break;
				}

				FPoly SavePoly = *PolyToSplit->GetActualPoly();

				// Add extra vertices to the surrounding polygons.  Any polygon that has
				// any of the selected edges as a part of it needs to have an extra vertex added.

				for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
				{
					FGeomPoly* gp = &go->PolyPool(p);

					for( INT s = 0 ; s < Edges.Num() ; ++s )
					{
						if( Edges(s).ParentPolyIndices.FindItemIndex( p ) != INDEX_NONE )
						{
							FPoly* Poly = gp->GetActualPoly();

							// Give the polygon a new set of vertices which includes the new vertex from the middle of the split edge.

							Poly->NumVertices = 0;

							for( INT i = 0 ; i < gp->EdgeIndices.Num() ; ++i )
							{
								FGeomEdge* ge = &go->EdgePool( gp->EdgeIndices(i) );

								Poly->Vertex[ Poly->NumVertices ] = go->VertexPool( ge->VertexIndices[0] );
								Poly->NumVertices++;

								if( ge->IsSame( &Edges(s) ) )
								{
									Poly->Vertex[ Poly->NumVertices ] = ge->Mid;
									Poly->NumVertices++;
								}
							}

							// If there are too many vertices, split the polygon in half.

							if( Poly->NumVertices >= FPoly::VERTEX_THRESHOLD )
							{
								FPoly* NewPoly = new( PolyToSplit->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();
								Poly->SplitInHalf( NewPoly );
							}

							Poly->Fix();
						}
					}
				}

				// If we are splitting 2 edges, we are splitting the polygon itself.

				if( Edges.Num() == 2 )
				{
					// Split the main poly.  This is the polygon that we determined earlier shares both
					// of the selected edges.  It needs to be removed and 2 new polygons created in its place.

					FVector Mid0 = Edges(0).Mid,
						Mid1 = Edges(1).Mid;

					FVector PlaneBase = Mid0;
					FVector v0 = Mid0,
						v1 = Mid1,
						v2 = v1 + (PolyToSplit->Normal * 16);
					FVector PlaneNormal = FPlane( v0, v1, v2 );

					FPoly* Front = new( PolyToSplit->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();
					Front->Init();
					FPoly* Back = new( PolyToSplit->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();
					Back->Init();

					INT result = PolyToSplit->GetActualPoly()->SplitWithPlane( PlaneBase, PlaneNormal, Front, Back, 1 );
					check( result == SP_Split );	// Any other result means that the splitting plane was created incorrectly

					Front->Base = SavePoly.Base;
					Front->TextureU = SavePoly.TextureU;
					Front->TextureV = SavePoly.TextureV;
					Front->Material = SavePoly.Material;

					Back->Base = SavePoly.Base;
					Back->TextureU = SavePoly.TextureU;
					Back->TextureV = SavePoly.TextureV;
					Back->Material = SavePoly.Material;

					// Remove the old polygon from the brush

					go->GetActualBrush()->Brush->Polys->Element.Remove( PolyToSplit->ActualPolyIndex );
				}
			}
		}
		break;
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();
	EndTrans();

	EndModify();
}

/*------------------------------------------------------------------------------
	UGeomModifier_Triangulate
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Triangulate::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Poly:
			return 1;
	}

	return 0;
}

void UGeomModifier_Triangulate::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Poly:
		{
			// Mark the selected polygons so we can find them in the next loop, and create
			// a local list of FPolys to triangulate later.

			for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
			{
				FGeomObject* go = &mode->GeomObjects(o);

				TArray<FPoly> PolyList;

				for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
				{
					FGeomPoly* gp = &go->PolyPool(p);

					if( gp->IsSelected() )
					{
						gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element( gp->ActualPolyIndex ).PolyFlags |= PF_GeomMarked;
						PolyList.AddItem( gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element( gp->ActualPolyIndex ) );
					}
				}

				// Delete existing polygons

				for( INT p = 0 ; p < go->GetActualBrush()->Brush->Polys->Element.Num() ; ++p )
				{
					if( (go->GetActualBrush()->Brush->Polys->Element( p ).PolyFlags&PF_GeomMarked) > 0 )
					{
						go->GetActualBrush()->Brush->Polys->Element.Remove( p );
						p = -1;
					}
				}

				// Triangulate the old polygons into the brush

				for( INT p = 0 ; p < PolyList.Num() ; ++p )
				{
					PolyList(p).Triangulate( go->GetActualBrush() );
				}
			}
		}
		break;
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();
	EndTrans();	

	EndModify();
}

/*------------------------------------------------------------------------------
	UGeomModifier_Turn
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Turn::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Edge:
			return 1;
	}

	return 0;
}

void UGeomModifier_Turn::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Edge:
		{
			for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
			{
				FGeomObject* go = &mode->GeomObjects(o);

				TArray<FGeomEdge> Edges;
				go->CompileUniqueEdgeArray( &Edges );

				// Make sure that all polygons involved are triangles

				for( INT e = 0 ; e < Edges.Num() ; ++e )
				{
					FGeomEdge* ge = &Edges(e);

					for( INT p = 0 ; p < ge->ParentPolyIndices.Num() ; ++p )
					{
						FGeomPoly* gp = &go->PolyPool( ge->ParentPolyIndices(p) );
						FPoly* Poly = gp->GetActualPoly();

						if( Poly->NumVertices != 3 )
						{
							GeomError( TEXT("The polygons on each side of the edge you want to turn must be triangles." ) );
							EndTrans();
							return;
						}
					}
				}

				// Turn the edges, one by one

				for( INT e = 0 ; e < Edges.Num() ; ++e )
				{
					FGeomEdge* ge = &Edges(e);

					TArray<FVector> Quad;

					// Since we're doing each edge individually, they should each have exactly 2 polygon
					// parents (and each one is a triangle (verified above))

					if( ge->ParentPolyIndices.Num() == 2 )
					{
						FGeomPoly* gp = &go->PolyPool( ge->ParentPolyIndices(0) );
						FPoly* Poly = gp->GetActualPoly();
						FPoly SavePoly0 = *Poly;

						INT idx0 = Poly->GetVertexIndex( go->VertexPool( ge->VertexIndices[0] ) );
						INT idx1 = Poly->GetVertexIndex( go->VertexPool( ge->VertexIndices[1] ) );
						INT idx2 = INDEX_NONE;

						if( idx0 + idx1 == 1 )
						{
							idx2 = 2;
						}
						else if( idx0 + idx1 == 3 )
						{
							idx2 = 0;
						}
						else
						{
							idx2 = 1;
						}

						Quad.AddItem( Poly->Vertex[idx0] );
						Quad.AddItem( Poly->Vertex[idx2] );
						Quad.AddItem( Poly->Vertex[idx1] );

						gp = &go->PolyPool( ge->ParentPolyIndices(1) );
						Poly = gp->GetActualPoly();
						FPoly SavePoly1 = *Poly;

						for( INT v = 0 ; v < Poly->NumVertices ; ++v )
						{
							Quad.AddUniqueItem( Poly->Vertex[v] );
						}

						// Create new polygons

						FPoly* NewPoly;

						NewPoly = new( gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();

						NewPoly->Init();
						NewPoly->Vertex[0] = Quad(2);
						NewPoly->Vertex[1] = Quad(1);
						NewPoly->Vertex[2] = Quad(3);
						NewPoly->NumVertices = 3;

						NewPoly->Base = SavePoly0.Base;
						NewPoly->Material = SavePoly0.Material;
						NewPoly->PolyFlags = SavePoly0.PolyFlags;
						NewPoly->TextureU = SavePoly0.TextureU;
						NewPoly->TextureV = SavePoly0.TextureV;
						NewPoly->Normal = FVector(0,0,0);
						NewPoly->Finalize(go->GetActualBrush(),1);

						NewPoly = new( gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();

						NewPoly->Init();
						NewPoly->Vertex[0] = Quad(3);
						NewPoly->Vertex[1] = Quad(1);
						NewPoly->Vertex[2] = Quad(0);
						NewPoly->NumVertices = 3;

						NewPoly->Base = SavePoly1.Base;
						NewPoly->Material = SavePoly1.Material;
						NewPoly->PolyFlags = SavePoly1.PolyFlags;
						NewPoly->TextureU = SavePoly1.TextureU;
						NewPoly->TextureV = SavePoly1.TextureV;
						NewPoly->Normal = FVector(0,0,0);
						NewPoly->Finalize(go->GetActualBrush(),1);

						// Tag the old polygons

						for( INT p = 0 ; p < ge->ParentPolyIndices.Num() ; ++p )
						{
							FGeomPoly* gp = &go->PolyPool( ge->ParentPolyIndices(p) );

							go->GetActualBrush()->Brush->Polys->Element( gp->ActualPolyIndex ).PolyFlags |= PF_GeomMarked;
						}
					}
				}

				// Delete the old polygons

				for( INT p = 0 ; p < go->GetActualBrush()->Brush->Polys->Element.Num() ; ++p )
				{
					if( (go->GetActualBrush()->Brush->Polys->Element( p ).PolyFlags&PF_GeomMarked) > 0 )
					{
						go->GetActualBrush()->Brush->Polys->Element.Remove( p );
						p = -1;
					}
				}
			}
			
		}
		break;
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();
	EndTrans();

	EndModify();
}

/*------------------------------------------------------------------------------
	UGeomModifier_Weld
------------------------------------------------------------------------------*/

UBOOL UGeomModifier_Weld::Supports( int InSelType )
{
	switch( InSelType )
	{
		case GST_Vertex:
			return 1;
	}

	return 0;
}

void UGeomModifier_Weld::Apply()
{
	StartTrans();

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools.GetCurrentMode();
	EGeometrySelectionType seltype = ((FGeometryToolSettings*)mode->GetSettings())->SelectionType;

	switch( seltype )
	{
		case GST_Vertex:
		{
			for( INT o = 0 ; o < mode->GeomObjects.Num() ; ++o )
			{
				FGeomObject* go = &mode->GeomObjects(o);

				go->CompileSelectionOrder();

				if( go->SelectionOrder.Num() > 1 )
				{
					FGeomVertex* FirstSel = (FGeomVertex*)go->SelectionOrder(0);

					// Move all selected vertices to the location of the first vertex that was selected.

					for( INT v = 1 ; v < go->SelectionOrder.Num() ; ++v )
					{
						FGeomVertex* gv = (FGeomVertex*)go->SelectionOrder(v);

						if( gv->IsSelected() )
						{
							gv->X = FirstSel->X;
							gv->Y = FirstSel->Y;
							gv->Z = FirstSel->Z;
						}
					}

					go->SendToSource();
				}
			}
			
		}
		break;
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();
	EndTrans();

	EndModify();
}

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/
