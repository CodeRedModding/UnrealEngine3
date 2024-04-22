/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

//#include "UnrealEd.h"
//#include "DlgGeometryTools.h"
//#include "Properties.h"
#include "UnrealEd.h"
#include "ScopedTransaction.h"
#include "BSPOps.h"
#include "GeomTools.h"

IMPLEMENT_CLASS(UGeomModifier)
IMPLEMENT_CLASS(UGeomModifier_Edit)
IMPLEMENT_CLASS(UGeomModifier_Extrude)
IMPLEMENT_CLASS(UGeomModifier_Clip)
IMPLEMENT_CLASS(UGeomModifier_Lathe)
IMPLEMENT_CLASS(UGeomModifier_Pen)
IMPLEMENT_CLASS(UGeomModifier_Delete)
IMPLEMENT_CLASS(UGeomModifier_Create)
IMPLEMENT_CLASS(UGeomModifier_Flip)
IMPLEMENT_CLASS(UGeomModifier_Split)
IMPLEMENT_CLASS(UGeomModifier_Triangulate)
IMPLEMENT_CLASS(UGeomModifier_Optimize)
IMPLEMENT_CLASS(UGeomModifier_Turn)
IMPLEMENT_CLASS(UGeomModifier_Weld)

/*------------------------------------------------------------------------------
	UGeomModifier
------------------------------------------------------------------------------*/

/**
 * @return		The modifier's description string.
 */
const FString& UGeomModifier::GetModifierDescription() const
{ 
	return Description;
}

/**
 * Gives the individual modifiers a chance to do something the first time they are activated.
 */
void UGeomModifier::Initialize()
{
}

/**
 * @return		TRUE if the key was handled by this editor mode tool.
 */
UBOOL UGeomModifier::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	return FALSE;
}

/**
 * @return		TRUE if the delta was handled by this editor mode tool.
 */
UBOOL UGeomModifier::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if( GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		if( !bInitialized )
		{
			const FEdModeGeometry* CurMode = static_cast<const FEdModeGeometry*>( GEditorModeTools().GetActiveMode(EM_Geometry) );
			const FGeometryToolSettings* Settings = static_cast<const FGeometryToolSettings*>( CurMode->GetSettings() );
			Initialize();
			bInitialized = TRUE;
		}
	}

	return FALSE;
}

/**
 * Applies the modifier.  Does nothing if the editor is not in geometry mode.
 *
 * @return		TRUE if something happened.
 */
UBOOL UGeomModifier::Apply()
{
	UBOOL bResult = FALSE;
	if( GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		StartTrans();
		bResult = OnApply();
		EndTrans();
		EndModify();
	}
	return bResult;
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier::OnApply()
{
	return FALSE;
}

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier::Supports()
{
	return TRUE;
}

/**
 * Interface for displaying error messages.
 *
 * @param	InErrorMsg		The error message to display.
 */
void UGeomModifier::GeomError(const FString& InErrorMsg)
{
	appMsgf( AMT_OK, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Error_Modifier"), *GetModifierDescription(), *InErrorMsg) ) );
}

/**
 * Starts the modification of geometry data.
 */
UBOOL UGeomModifier::StartModify()
{
	bInitialized = FALSE;
	return FALSE;
}

/**
 * Ends the modification of geometry data.
 */
UBOOL UGeomModifier::EndModify()
{
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
	return TRUE;
}

void UGeomModifier::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
}

void UGeomModifier::DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
}

/**
 * Stores the current state of the brush so that upon faulty operations, the
 * brush may be restored to its previous state
 */
 void UGeomModifier::CacheBrushState()
 {
	ABrush* BuilderBrush = GWorld->GetBrush();
	if( !CachedPolys )
	{
		//Create the list of polys
		CachedPolys = ConstructObject<UPolys>(UPolys::StaticClass(), this);
	}
	CachedPolys->Element.Empty();

	//Create duplicates of all of the polys in the brush
	for( INT polyIndex = 0 ; polyIndex < BuilderBrush->Brush->Polys->Element.Num() ; ++polyIndex )
	{
		FPoly currentPoly = BuilderBrush->Brush->Polys->Element(polyIndex);
		FPoly newPoly;
		newPoly.Init();
		newPoly.Base = currentPoly.Base;

		//Add all of the verts to the new poly
		for( INT vertIndex = 0; vertIndex < currentPoly.Vertices.Num(); ++vertIndex )
		{
			FVector newVertex = currentPoly.Vertices(vertIndex);
			newPoly.Vertices.AddItem( newVertex );	
		}
		CachedPolys->Element.AddItem(newPoly);
	}
 }
 
/**
 * Restores the brush to its cached state
 */
 void UGeomModifier::RestoreBrushState()
 {
	ABrush* BuilderBrush = GWorld->GetBrush();
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	//Remove all of the current polys
	BuilderBrush->Brush->Polys->Element.Empty();

	//Add all of the cached polys
	for( INT polyIndex = 0 ; polyIndex < CachedPolys->Element.Num() ; polyIndex++ )
	{
		BuilderBrush->Brush->Polys->Element.Push(CachedPolys->Element(polyIndex));
	}

	BuilderBrush->Brush->BuildBound();

	BuilderBrush->ClearComponents();
	BuilderBrush->ConditionalUpdateComponents();

	mode->FinalizeSourceData();
	mode->GetFromSource();

	GEditor->SelectNone( TRUE, TRUE );

	GEditor->RedrawLevelEditingViewports(TRUE);

	//Tell the user what just happened
	appDebugMessagef(TEXT("Invalid brush state could fail to triangulate.  Reverting to previous state."));
}

/**
 * @return		TRUE if two edges in the shapes overlap not at a vertex
 */
UBOOL UGeomModifier::DoEdgesOverlap()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	//Loop through all of the geometry objects
	for( FEdModeGeometry::TGeomObjectIterator itor( mode->GeomObjectItor() ) ; itor ; ++itor )
	{
		FGeomObject* geomObject = *itor;

		//Loop through all of the edges
		for( INT edgeIndex1 = 0 ; edgeIndex1 < geomObject->EdgePool.Num() ; ++edgeIndex1 )
		{
			FGeomEdge* edge1 = &geomObject->EdgePool(edgeIndex1);

			for( INT edgeIndex2 = 0 ; edgeIndex2 < geomObject->EdgePool.Num() ; ++edgeIndex2 )
			{
				FGeomEdge* edge2 = &geomObject->EdgePool(edgeIndex2);
				//Don't compare an edge with itself
				if( !(edge1->IsSameEdge(*edge2)) )
				{
					FVector closestPoint1, closestPoint2;
					FVector edge1Vert1 = geomObject->VertexPool( edge1->VertexIndices[0] );
					FVector edge2Vert1 = geomObject->VertexPool( edge2->VertexIndices[0] );
					FVector edge1Vert2 = geomObject->VertexPool( edge1->VertexIndices[1] );
					FVector edge2Vert2 = geomObject->VertexPool( edge2->VertexIndices[1] );

					//Find the distance between the two segments
					SegmentDistToSegment( edge1Vert1, edge1Vert2, edge2Vert1, edge2Vert2, closestPoint1, closestPoint2 );

					if ( (closestPoint1.Equals(closestPoint2)) )
					{
						//Identical closest points indicates that lines cross
						UBOOL bSharedVertex =  ((edge1Vert1.Equals(edge2Vert1)) || (edge1Vert1.Equals(edge2Vert2))
											|| (edge1Vert2.Equals(edge2Vert1)) || (edge1Vert2.Equals(edge2Vert2)));

						// Edges along the same line are exempt
						if (  !bSharedVertex )
						{
							UBOOL bIntersectionIsVert = ((edge1Vert1.Equals(closestPoint2, THRESH_POINTS_ARE_SAME)) || (edge1Vert2.Equals(closestPoint2, THRESH_POINTS_ARE_SAME))
												||  (edge2Vert1.Equals(closestPoint2, THRESH_POINTS_ARE_SAME )) || (edge2Vert2.Equals(closestPoint2, THRESH_POINTS_ARE_SAME)) );

							// Edges intersecting at a vertex are exempt
							if ( !bIntersectionIsVert )
							{
								// Edges cross.  The shape drawn with this brush will likely be undesireable
								return TRUE;
							}
						}
					}
				}
			}
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Transaction tracking.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
	/**
	 * @return		The shared transaction object used by 
	 */
	static FScopedTransaction*& StaticTransaction()
	{
		static FScopedTransaction* STransaction = NULL;
		return STransaction;
	}

	/**
	 * Ends the outstanding transaction, if one exists.
	 */
	static void EndTransaction()
	{
		delete StaticTransaction();
		StaticTransaction() = NULL;
	}

	/**
	 * Begins a new transaction, if no outstanding transaction exists.
	 */
	static void BeginTransaction(const TCHAR* SessionName)
	{
		if ( !StaticTransaction() )
		{
			StaticTransaction() = new FScopedTransaction( SessionName );
		}
	}
} // namespace

/**
 * Handles the starting of transactions against the selected ABrushes.
 */
void UGeomModifier::StartTrans()
{
	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return;
	}

	FEdModeGeometry* CurMode = static_cast<FEdModeGeometry*>( GEditorModeTools().GetActiveMode(EM_Geometry) );

	// Record the current selection list into the selected brushes.
	for( FEdModeGeometry::TGeomObjectIterator Itor( CurMode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;
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
				gs->Type = GS_Vertex;
				gs->Index = v;
				gs->SelectionIndex = gv->GetSelectionIndex();
			}
		}
		for( INT e = 0 ; e < go->EdgePool.Num() ; ++e )
		{
			FGeomEdge* ge = &go->EdgePool(e);
			if( ge->IsSelected() )
			{
				gs = new( Actor->SavedSelections )FGeomSelection;
				gs->Type = GS_Edge;
				gs->Index = e;
				gs->SelectionIndex = ge->GetSelectionIndex();
			}
		}
		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			if( gp->IsSelected() )
			{
				gs = new( Actor->SavedSelections )FGeomSelection;
				gs->Type = GS_Poly;
				gs->Index = p;
				gs->SelectionIndex = gp->GetSelectionIndex();
			}
		}
	}

	// Start the transaction.
	BeginTransaction( *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("Modifier_F")), *GetModifierDescription()) ) );

	// Mark all selected brushes as modified.
	for( FEdModeGeometry::TGeomObjectIterator Itor( CurMode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;
		ABrush* Actor = go->GetActualBrush();

		Actor->Modify();
	}
}

/**
 * Handles the stopping of transactions against the selected ABrushes.
 */
void UGeomModifier::EndTrans()
{
	EndTransaction();
}

/*------------------------------------------------------------------------------
	UGeomModifier_Edit
------------------------------------------------------------------------------*/

/**
 * @return		TRUE if the delta was handled by this editor mode tool.
 */
UBOOL UGeomModifier_Edit::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if( UGeomModifier::InputDelta( InViewportClient, InViewport, InDrag, InRot, InScale ) )
	{
		return TRUE;
	}

	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return FALSE;
	}

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();

	TArray<FGeomVertex*> UniqueVertexList;

	/**
	* All geometry objects can be manipulated by transforming the vertices that make
	* them up.  So based on the type of thing we're editing, we need to dig for those
	* vertices a little differently.
	*/

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);
			if( gp->IsSelected() )
			{
				for( INT e = 0 ; e < gp->EdgeIndices.Num() ; ++e )
				{
					FGeomEdge* ge = &go->EdgePool( gp->EdgeIndices(e) );
					UniqueVertexList.AddUniqueItem( &go->VertexPool( ge->VertexIndices[0] ) );
					UniqueVertexList.AddUniqueItem( &go->VertexPool( ge->VertexIndices[1] ) );
				}
			}
		}

		for( INT e = 0 ; e < go->EdgePool.Num() ; ++e )
		{
			FGeomEdge* ge = &go->EdgePool(e);
			if( ge->IsSelected() )
			{
				UniqueVertexList.AddUniqueItem( &go->VertexPool( ge->VertexIndices[0] ) );
				UniqueVertexList.AddUniqueItem( &go->VertexPool( ge->VertexIndices[1] ) );
			}
		}

		for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
		{
			FGeomVertex* gv = &go->VertexPool(v);
			if( gv->IsSelected() )
			{
				UniqueVertexList.AddUniqueItem( gv );
			}
		}
	}

	// If we didn't move any vertices, then tell the caller that we didn't handle the input.
	// This allows LDs to drag brushes around in geometry mode as long as no geometry
	// objects are selected.
	if( !UniqueVertexList.Num() )
	{
		return FALSE;
	}

	const UBOOL bShiftPressed = InViewportClient->Input->IsShiftPressed();

	// If we're trying to rotate vertices, only allow that if Shift is held down.  This just makes it easier
	// to rotate brushes around while working in geometry mode, since you rarely ever want to rotate vertices
	FRotator FinalRot = InRot;
	if( !bShiftPressed )
	{
		FinalRot = FRotator( 0.0f, 0.0f, 0.0f );
	}

	if( InDrag.IsZero() && FinalRot.IsZero() && InScale.IsZero() )
	{
		// No change, but handled
		return TRUE;
	}

	// Let tool know that some modification has actually taken place
	tool->bGeomModified = TRUE;

	/**
	* Scaling needs to know the bounding box for the selected verts, so generate that before looping.
	*/
	FBox VertBBox(0);

	for( INT x = 0 ; x < UniqueVertexList.Num() ; ++x )
	{
		VertBBox += *UniqueVertexList(x);
	}

	FVector BBoxExtent = VertBBox.GetExtent();

	FGeomVertex* vertex0 = UniqueVertexList(0);
	const ABrush* Brush = vertex0->GetParentObject()->GetActualBrush();
	FVector vertOffset = FVector(0, 0, 0);

	//Calculate translation now so that it isn't done every iteration of the proceeding loop
	
	if( !InDrag.IsZero() )
	{
		// Volumes store their rotation locally, whereas normal brush rotations are always in worldspace, so we need to transform
		// the drag vector into a volume's local space before applying it.
		if( Brush->IsVolumeBrush() )
		{
			const FVector Wk = Brush->WorldToLocal().TransformNormal(InDrag);
			vertOffset = Wk;
		}
		else
		{
			vertOffset = InDrag;
		}
	}

	/**
	* We first generate a list of unique vertices and then transform that list
	* in one shot.  This prevents vertices from being touched more than once (which
	* would result in them transforming x times as fast as others).
	*/
	for( INT x = 0 ; x < UniqueVertexList.Num() ; ++x )
	{
		FGeomVertex* vtx = UniqueVertexList(x);

		//Translate

			*vtx += vertOffset;

		// Rotate

		if( !FinalRot.IsZero() )
		{
			FRotationMatrix matrix( FinalRot );

			FVector Wk( vtx->X, vtx->Y, vtx->Z );
			Wk = vtx->GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( Wk );
			Wk -= GEditorModeTools().PivotLocation;
			Wk = matrix.TransformFVector( Wk );
			Wk += GEditorModeTools().PivotLocation;
			*vtx = vtx->GetParentObject()->GetActualBrush()->WorldToLocal().TransformFVector( Wk );
		}

		// Scale

		if( !InScale.IsZero() )
		{
			FLOAT XFactor = (InScale.X > 0.f) ? 1 : -1;
			FLOAT YFactor = (InScale.Y > 0.f) ? 1 : -1;
			FLOAT ZFactor = (InScale.Z > 0.f) ? 1 : -1;
			FLOAT Strength;

			FVector Wk( vtx->X, vtx->Y, vtx->Z );
			Wk = vtx->GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( Wk );

			// Move vert to the origin

			Wk -= GEditorModeTools().PivotLocation;

			// Move it along each axis based on it's distance from the origin

			if( Wk.X != 0 )
			{
				Strength = (BBoxExtent.X / Wk.X) * XFactor;
				Wk.X += GEditor->Constraints.GetGridSize() * Strength;
			}

			if( Wk.Y != 0 )
			{
				Strength = (BBoxExtent.Y / Wk.Y) * YFactor;
				Wk.Y += GEditor->Constraints.GetGridSize() * Strength;
			}

			if( Wk.Z != 0 )
			{
				Strength = (BBoxExtent.Z / Wk.Z) * ZFactor;
				Wk.Z += GEditor->Constraints.GetGridSize() * Strength;
			}

			// Move it back into world space

			Wk += GEditorModeTools().PivotLocation;

			*vtx = vtx->GetParentObject()->GetActualBrush()->WorldToLocal().TransformFVector( Wk );
		}
	}
	
	if( DoEdgesOverlap() )
	{
		//Two edges overlap, which causes triangulation problems, so move the vertices back to their previous location
		for( INT x = 0 ; x < UniqueVertexList.Num() ; ++x )
		{
			FGeomVertex* vtx = UniqueVertexList(x);
			*vtx -= vertOffset;
		}
		
		GEditorModeTools().PivotLocation -= vertOffset;
		GEditorModeTools().SnappedLocation -= vertOffset;
	}

	const UBOOL bIsCtrlPressed	= InViewportClient->Input->IsCtrlPressed();
	const UBOOL bIsAltPressed	= InViewportClient->Input->IsAltPressed();

	if(!InDrag.IsZero() && bShiftPressed && bIsCtrlPressed && !bIsAltPressed)
	{
		FVector CameraDelta(InDrag);

		// Only apply camera speed modifiers to the drag if we aren't zooming in an ortho viewport.
		if( !InViewportClient->IsOrtho() || !(InViewport->KeyState(KEY_LeftMouseButton) && InViewport->KeyState(KEY_RightMouseButton)) )
		{
			CameraDelta *= GCurrentLevelEditingViewportClient->CameraSpeed / 4.f;
		}

		if( LVT_OrthoXY == InViewportClient->ViewportType )
		{
			CameraDelta.X = -InDrag.Y;
			CameraDelta.Y = InDrag.X;
		}

		InViewportClient->MoveViewportCamera( CameraDelta, InRot );
	}

	return TRUE;
}

/*------------------------------------------------------------------------------
	UGeomModifier_Extrude
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Extrude::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return mode->HavePolygonsSelected();
}

/**
* Gives the modifier a chance to initialize it's internal state when activated.
*/

void UGeomModifier_Extrude::WasActivated()
{
	// Extrude requires a local coordinate system to work properly so automatically enable
	// that here while saving the current coordinate system for restoration later.
	SaveCoordSystem = GEditorModeTools().CoordSystem;
	GEditorModeTools().CoordSystem = COORD_Local;

	GEditor->RedrawLevelEditingViewports(TRUE);
}

void UGeomModifier_Extrude::WasDeactivated()
{
	// When the user leaves this modifier, restore their old coordinate system.
	GEditorModeTools().CoordSystem = (ECoordSystem)SaveCoordSystem;

	GEditor->RedrawLevelEditingViewports(TRUE);
}

/**
 * Gives the individual modifiers a chance to do something the first time they are activated.
 */
void UGeomModifier_Extrude::Initialize()
{
	if( GEditorModeTools().CoordSystem != COORD_Local )
	{
		return;
	}
	StartTrans();
	Apply( GEditor->Constraints.GetGridSize(), 1 );
	EndTrans();
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Extrude::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	// When applying via the keyboard, we force the local coordinate system.
	const ECoordSystem SaveCS = GEditorModeTools().CoordSystem;
	GEditorModeTools().CoordSystem = COORD_Local;

	//GApp->DlgGeometryTools->PropertyWindow->FinalizeValues();
	Apply( Length, Segments );

	// Restore the coordinate system.
	GEditorModeTools().CoordSystem = SaveCS;

	return TRUE;
}

IMPLEMENT_COMPARE_CONSTREF( FPoly, UnGeomModifiers_Extrude,\
	{\
		return (B.Normal - A.Normal).Size();\
	}\
);

void ExtrudePolygonGroup( ABrush* InBrush, FVector InGroupNormal, INT InStartOffset, INT InLength, TArray<FPoly>& InPolygonGroup )
{
	TArray< TArray<FVector> > Windings;

	FPoly::GetOutsideWindings( InBrush, InPolygonGroup, Windings, FALSE );

	for( INT w = 0 ; w < Windings.Num() ; ++w )
	{
		TArray<FVector>* WindingVerts = &Windings(w);

		FVector Offset = InGroupNormal * InLength;
		FVector StartOffset = InGroupNormal * InStartOffset;

		for( INT v = 0 ; v < WindingVerts->Num() ; ++v )
		{
			FVector vtx0 = StartOffset + (*WindingVerts)( v );
			FVector vtx1 = StartOffset + (*WindingVerts)( v ) + Offset;
			FVector vtx2 = StartOffset + (*WindingVerts)( (v + 1) % WindingVerts->Num() ) + Offset;
			FVector vtx3 = StartOffset + (*WindingVerts)( (v + 1) % WindingVerts->Num() );

			FPoly NewPoly;
			NewPoly.Init();
			NewPoly.Base = InBrush->Location;

			NewPoly.Vertices.AddItem( vtx1 );
			NewPoly.Vertices.AddItem( vtx0 );
			NewPoly.Vertices.AddItem( vtx3 );
			NewPoly.Vertices.AddItem( vtx2 );

			if( NewPoly.Finalize( InBrush, 1 ) == 0 )
			{
				InBrush->Brush->Polys->Element.AddItem( NewPoly );
			}
		}
	}
}

void UGeomModifier_Extrude::Apply(INT InLength, INT InSegments)
{
	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return;
	}

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	// Force user input to be valid

	InLength = Max( 1, InLength );
	InSegments = Max( 1, InSegments );

	//

	TArray<INT> SavedSelectionIndices;

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;
		ABrush* Brush = go->GetActualBrush();

		go->SendToSource();

		TArray<FPoly> Polygons;

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);

			FVector Normal = mode->GetWidgetNormalFromCurrentAxis( gp );

			if( gp->IsSelected() )
			{
				SavedSelectionIndices.AddItem( p );

				FPoly* Poly = gp->GetActualPoly();

				Polygons.AddItem( *Poly );

				// Move the existing poly along the normal by InLength units.

				for( INT v = 0 ; v < Poly->Vertices.Num() ; ++v )
				{
					FVector* vtx = &Poly->Vertices(v);

					*vtx += Normal * (InLength * InSegments);
				}

				Poly->Base += Normal * (InLength * InSegments);
			}
		}

		if( Polygons.Num() )
		{
			Sort<USE_COMPARE_CONSTREF(FPoly, UnGeomModifiers_Extrude)>( &Polygons(0), Polygons.Num() );
			FVector NormalCompare;
			TArray<FPoly> PolygonGroup;

			for( INT p = 0 ; p < Polygons.Num() ; ++p )
			{
				FPoly* Poly = &Polygons(p);

				if( p == 0 )
				{
					NormalCompare = Poly->Normal;
				}

				if( NormalCompare.Equals( Poly->Normal ) )
				{
					PolygonGroup.AddItem( *Poly );
				}
				else
				{
					if( PolygonGroup.Num() )
					{
						for( INT s = 0 ; s < InSegments ; ++s )
						{
							ExtrudePolygonGroup( Brush, NormalCompare, InLength * s, InLength, PolygonGroup );
						}
					}

					NormalCompare = Poly->Normal;
					PolygonGroup.Empty();
					PolygonGroup.AddItem( *Poly );
				}
			}

			if( PolygonGroup.Num() )
			{
				for( INT s = 0 ; s < InSegments ; ++s )
				{
					ExtrudePolygonGroup( Brush, NormalCompare, InLength * s, InLength, PolygonGroup );
				}
			}
		}

		go->FinalizeSourceData();
		go->GetFromSource();

		for( INT x = 0 ; x < SavedSelectionIndices.Num() ; ++x )
		{
			FGeomPoly* Poly = &go->PolyPool( SavedSelectionIndices(x) );
			Poly->Select(1);
		}
	}
}

/*------------------------------------------------------------------------------
	UGeomModifier_Lathe
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Lathe::Supports()
{
	// Lathe mode requires ABrushShape actors to be selected.

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor->IsBrushShape() )
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
* Gives the individual modifiers a chance to do something the first time they are activated.
*/
void UGeomModifier_Lathe::Initialize()
{
}

/**
* Implements the modifier application.
*/
UBOOL UGeomModifier_Lathe::OnApply()
{
	//FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetCurrentMode();

	//GApp->DlgGeometryTools->PropertyWindow->FinalizeValues();
	Apply( TotalSegments, Segments, (EAxis)Axis );

	return TRUE;
}

void UGeomModifier_Lathe::Apply( INT InTotalSegments, INT InSegments, EAxis InAxis )
{
	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return;
	}

	// Determine the axis from the active ortho viewport

	if ( !GLastKeyLevelEditingViewportClient || !GLastKeyLevelEditingViewportClient->IsOrtho() )
	{
		return;
	}

	//Save the brush state in case a bogus shape is generated
	CacheBrushState();

	switch( GLastKeyLevelEditingViewportClient->ViewportType )
	{
		case LVT_OrthoXZ:
			Axis = AXIS_X;
			break;

		case LVT_OrthoXY:
			Axis = AXIS_Y;
			break;

		case LVT_OrthoYZ:
			Axis = AXIS_Z;
			break;
	}

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	InTotalSegments = Max( 3, InTotalSegments );
	InSegments = Max( 1, InSegments );

	if( InSegments > InTotalSegments )
	{
		InTotalSegments = InSegments;
	}

	// We will be replacing the builder brush, so get it prepped.

	ABrush* BuilderBrush = GWorld->GetBrush();

	BuilderBrush->Location = mode->GetWidgetLocation();
	BuilderBrush->PrePivot = FVector(0,0,0);
	BuilderBrush->SetFlags( RF_Transactional );
	BuilderBrush->Brush->Polys->Element.Empty();

	// Ensure the builder brush is unhidden.
	BuilderBrush->bHidden = FALSE;
	BuilderBrush->bHiddenEdLayer = FALSE;
	BuilderBrush->bHiddenEdScene = FALSE;
	BuilderBrush->bHiddenEdTemporary = FALSE;

	// Some convenience flags
	UBOOL bNeedCaps = (InSegments < InTotalSegments);

	// Lathe every selected ABrushShape actor into the builder brush

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		TArray<FEdge> EdgePool;

		const AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor->IsBrushShape() )
		{
			const ABrushShape* BrushShape = ConstCast<ABrushShape>( Actor );

			if( BrushShape->Brush->Polys->Element.Num() < 1 )
			{
				continue;
			}

			TArray< TArray<FVector> > Windings;
			FPoly::GetOutsideWindings( (ABrush*)Actor, BrushShape->Brush->Polys->Element, Windings );

			FVector delta = mode->GetWidgetLocation() - BrushShape->Location;

			//
			// Let's lathe...
			//

			// Build up an array of vertices that represents the entire lathe.

			FLOAT AngleStep = 65536.f / (FLOAT)InTotalSegments;
			FLOAT Angle = 0;

			for( INT w = 0 ; w < Windings.Num() ; ++w )
			{
				TArray<FVector>* WindingVerts = &Windings(w);

				TArray<FVector> ShapeVertices;

				for( INT s = 0 ; s < (InSegments + 1 + (AlignToSide?1:0)) ; ++s )
				{
					FRotator rot = FRotator( 0, Angle, 0 );
					if( Axis == AXIS_X )
					{
						rot = FRotator( Angle, 0, 0 );
					}
					else if( Axis == AXIS_Z )
					{
						rot = FRotator( 0, 0, Angle );
					}

					FRotationMatrix RotationMatrix( rot );

					for( INT e = 0 ; e < WindingVerts->Num() ; ++e )
					{
						FVector vtx = (*WindingVerts)(e) - delta - BrushShape->PrePivot;

						vtx = RotationMatrix.TransformFVector( vtx );

						ShapeVertices.AddItem( vtx );
					}

					if( AlignToSide && (s == 0 || s == InSegments) )
					{
						Angle += AngleStep / 2.0f;
					}
					else
					{
						Angle += AngleStep;
					}
				}

				INT NumVertsInShape = WindingVerts->Num();

				for( INT s = 0 ; s < InSegments + (AlignToSide?1:0) ; ++s )
				{
					INT BaseIdx = s * WindingVerts->Num();

					for( INT v = 0 ; v < NumVertsInShape ; ++v )
					{
						FVector vtx0 = ShapeVertices( BaseIdx + v );
						FVector vtx1 = ShapeVertices( BaseIdx + NumVertsInShape + v );
						FVector vtx2 = ShapeVertices( BaseIdx + NumVertsInShape + ((v + 1) % NumVertsInShape) );
						FVector vtx3 = ShapeVertices( BaseIdx + ((v + 1) % NumVertsInShape) );

						FPoly NewPoly;
						NewPoly.Init();
						NewPoly.Base = BuilderBrush->Location;

						NewPoly.Vertices.AddItem( vtx0 );
						NewPoly.Vertices.AddItem( vtx1 );
						NewPoly.Vertices.AddItem( vtx2 );
						NewPoly.Vertices.AddItem( vtx3 );

						if( NewPoly.Finalize( BuilderBrush, 1 ) == 0 )
						{
							BuilderBrush->Brush->Polys->Element.AddItem( NewPoly );
						}
						
					}
				}
			}

			// Create start/end capping polygons if they are necessary

			if( bNeedCaps )
			{
				for( INT w = 0 ; w < Windings.Num() ; ++w )
				{
					TArray<FVector>* WindingVerts = &Windings(w);

					//
					// Create the start cap
					//

					FPoly Poly;
					Poly.Init();
					Poly.Base = BrushShape->Location;

					// Add the verts from the shape

					for( INT v = 0 ; v < WindingVerts->Num() ; ++v )
					{
						Poly.Vertices.AddItem( (*WindingVerts)(v) - delta - BrushShape->PrePivot );
					}

					Poly.Finalize( BuilderBrush, 1 );

					// Break the shape down into convex shapes.

					TArray<FPoly> Polygons;
					Poly.Triangulate( BuilderBrush, Polygons );
					FPoly::OptimizeIntoConvexPolys( BuilderBrush, Polygons );

					// Add the resulting convex polygons into the brush

					for( INT p = 0 ; p < Polygons.Num() ; ++p )
					{
						FPoly Poly = Polygons(p);

						if( Poly.Finalize( BuilderBrush, 1 ) == 0 )
						{
							BuilderBrush->Brush->Polys->Element.AddItem( Poly );
						}
					}

					//
					// Create the end cap
					//

					Poly.Init();
					Poly.Base = BrushShape->Location;

					// Add the verts from the shape

					FRotator rot = FRotator( 0, AngleStep * InSegments, 0 );
					if( Axis == AXIS_X )
					{
						rot = FRotator( AngleStep * InSegments, 0, 0 );
					}
					else if( Axis == AXIS_Z )
					{
						rot = FRotator( 0, 0, AngleStep * InSegments );
					}

					FRotationMatrix RotationMatrix( rot );

					for( INT v = 0 ; v < WindingVerts->Num() ; ++v )
					{
						Poly.Vertices.AddItem( RotationMatrix.TransformFVector( (*WindingVerts)(v) - delta - BrushShape->PrePivot ) );
					}

					Poly.Finalize( BuilderBrush, 1 );

					// Break the shape down into convex shapes.

					Polygons.Empty();
					Poly.Triangulate( BuilderBrush, Polygons );
					FPoly::OptimizeIntoConvexPolys( BuilderBrush, Polygons );

					// Add the resulting convex polygons into the brush

					for( INT p = 0 ; p < Polygons.Num() ; ++p )
					{
						FPoly Poly = Polygons(p);
						Poly.Reverse();

						if( Poly.Finalize( BuilderBrush, 1 ) == 0 )
						{
							BuilderBrush->Brush->Polys->Element.AddItem( Poly );
						}
					}
				}
			}
		}
	}

	// Finalize the builder brush

	BuilderBrush->Brush->BuildBound();

	BuilderBrush->ClearComponents();
	BuilderBrush->ConditionalUpdateComponents();

	mode->FinalizeSourceData();
	mode->GetFromSource();

	GEditor->SelectNone( TRUE, TRUE );
	GEditor->SelectActor( BuilderBrush, TRUE, GLastKeyLevelEditingViewportClient, TRUE );

	if( DoEdgesOverlap() )
	{//Overlapping edges yielded an invalid brush state
		RestoreBrushState();
	}
	else
	{
		GEditor->RedrawLevelEditingViewports(TRUE);
	}
}

/*------------------------------------------------------------------------------
	UGeomModifier_Pen
------------------------------------------------------------------------------*/

/**
* Gives the modifier a chance to initialize it's internal state when activated.
*/

void UGeomModifier_Pen::WasActivated()
{
	ShapeVertices.Empty();
}

/**
* Implements the modifier application.
*/
UBOOL UGeomModifier_Pen::OnApply()
{
	Apply();

	return TRUE;
}

void UGeomModifier_Pen::Apply()
{
	if( ShapeVertices.Num() > 2 )
	{
		FVector BaseLocation = ShapeVertices(0);

		ABrush* ResultingBrush = GWorld->GetBrush();
		ABrush* BuilderBrush = GWorld->GetBrush();

		if( bCreateBrushShape )
		{
			// Create a shape brush instead of modifying the builder brush

			ResultingBrush = (ABrush*)GWorld->SpawnActor(ABrushShape::StaticClass(),NAME_None,BaseLocation);

			ResultingBrush->PreEditChange(NULL);

			FBSPOps::csgCopyBrush
				(
				ResultingBrush,
				GWorld->GetBrush(),
				0,
				RF_Transactional,
				1,
				TRUE
				);
		
			ResultingBrush->PostEditChange();

			// Make sure the graphics engine isn't busy rendering this geometry before we
			// we go and modify it
			FlushRenderingCommands();
		}

		// Move all the vertices that the user placed to the same "height" as the builder brush, based on
		// viewport orientation.  This is preferable to always creating the new builder brush at height zero.

		for( INT v = 0 ; v < ShapeVertices.Num() ; ++v )
		{
			FVector* vtx = &ShapeVertices(v);

			switch( GLastKeyLevelEditingViewportClient->ViewportType )
			{
				case LVT_OrthoXY:
					vtx->Z = BuilderBrush->Location.Z;
					break;

				case LVT_OrthoXZ:
					vtx->Y = BuilderBrush->Location.Y;
					break;

				case LVT_OrthoYZ:
					vtx->X = BuilderBrush->Location.X;
					break;
			}
		}

		ResultingBrush->Location = BaseLocation;
		ResultingBrush->PrePivot = FVector(0,0,0);
		ResultingBrush->SetFlags( RF_Transactional );
		ResultingBrush->Brush->Polys->Element.Empty();

		// Ensure the builder brush is unhidden.
		ResultingBrush->bHidden = FALSE;
		ResultingBrush->bHiddenEdLayer = FALSE;
		ResultingBrush->bHiddenEdScene = FALSE;
		ResultingBrush->bHiddenEdTemporary = FALSE;

		FPoly Poly;
		Poly.Init();
		Poly.Base = BaseLocation;

		for( INT v = 0 ; v < ShapeVertices.Num() ; ++v )
		{
			new(Poly.Vertices) FVector( ShapeVertices(v) - BaseLocation );
		}

		if( Poly.Finalize( ResultingBrush, 1 ) == 0 )
		{
			// Break the shape down into triangles.

			TArray<FPoly> Triangles;
			Poly.Triangulate( ResultingBrush, Triangles );

			TArray<FPoly> Polygons = Triangles;

			// Optionally, optimize the resulting triangles into convex polygons.

			if( bCreateConvexPolygons )
			{
				FPoly::OptimizeIntoConvexPolys( ResultingBrush, Polygons );
			}

			// Now that we have a set of convex polygons, add them all to the brush.  These will form the top face.

			for( INT  p = 0  ; p < Polygons.Num() ; ++p )
			{
				if( Polygons(p).Finalize( ResultingBrush, 0 ) == 0 )
				{
					new(ResultingBrush->Brush->Polys->Element)FPoly(Polygons(p));
				}
			}

			// If the user isn't creating an ABrushShape, then carry on adding the sides and bottom face

			if( !bCreateBrushShape )
			{
				// If the user wants a full brush created, add the rest of the polys

				if( bAutoExtrude && ExtrudeDepth > 0 )
				{
					FVector delta;

					// Create another set of polygons that will represent the bottom face

					for( INT p = 0 ; p < Polygons.Num() ; ++p )
					{
						FPoly poly = Polygons(p);

						poly.Reverse();

						if( poly.Finalize( ResultingBrush, 0 ) == 0 )
						{
							delta = poly.Normal * ExtrudeDepth;

							for( INT v = 0 ; v < poly.Vertices.Num() ; ++v )
							{
								FVector* vtx = &poly.Vertices(v);
								*vtx += delta;
							}

							new(ResultingBrush->Brush->Polys->Element)FPoly( poly );
						}
					}

					// Create the polygons that make up the sides of the brush

					if( Polygons.Num() > 0 )
					{
						for( INT v = 0 ; v < ShapeVertices.Num() ; ++v )
						{
							FVector vtx0 = ShapeVertices(v);
							FVector vtx1 = ShapeVertices((v+1)%ShapeVertices.Num());
							FVector vtx2 = vtx1 + delta;
							FVector vtx3 = vtx0 + delta;

							FPoly SidePoly;
							SidePoly.Init();

							SidePoly.Vertices.AddItem( vtx1 - BaseLocation );
							SidePoly.Vertices.AddItem( vtx0 - BaseLocation );
							SidePoly.Vertices.AddItem( vtx3 - BaseLocation );
							SidePoly.Vertices.AddItem( vtx2 - BaseLocation );

							if( SidePoly.Finalize( ResultingBrush, 1 ) == 0 )
							{
								new(ResultingBrush->Brush->Polys->Element)FPoly( SidePoly );
							}
						}
					}
				}
			}
		}

		// Finish up

		ResultingBrush->Brush->BuildBound();

		ResultingBrush->ClearComponents();
		ResultingBrush->ConditionalUpdateComponents();

		ShapeVertices.Empty();

		FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

		mode->FinalizeSourceData();
		mode->GetFromSource();

		GEditor->SelectNone( TRUE, TRUE );
		GEditor->SelectActor( ResultingBrush, TRUE, GLastKeyLevelEditingViewportClient, TRUE );

		// Switch back to edit mode
		//FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();
		//tool->SetCurrentModifier( tool->GetModifier(0) );

		GEditor->RedrawLevelEditingViewports(TRUE);
	}
}

/**
* @return		TRUE if the key was handled by this editor mode tool.
*/
UBOOL UGeomModifier_Pen::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	UBOOL bResult = FALSE;
#if WITH_EDITORONLY_DATA
	if( ViewportClient->IsOrtho() && Event == IE_Pressed )
	{
		const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
		const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
		const UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

		// CTRL+RightClick (or SPACE bar) adds a vertex to the world

		if( (bCtrlDown && !bShiftDown && !bAltDown && Key == KEY_RightMouseButton) || Key == KEY_SpaceBar )
		{
			// if we're trying to edit vertices in a different viewport to the one we started in then popup a warning
			if( ShapeVertices.Num() && ViewportClient != UsingViewportClient )
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd( "GeomModifierPen_Warning_AddingVertexInWrongViewport" ) );
				return TRUE;
			}
			if( ShapeVertices.Num() && MouseWorldSpacePos.Equals( ShapeVertices(0) ) )
			{
				Apply();
			}
			else
			{
				UsingViewportClient = ViewportClient;
				ShapeVertices.AddItem( MouseWorldSpacePos );
			}

			bResult = TRUE;
		}
		else if( Key == KEY_Escape || Key == KEY_BackSpace )
		{
			if( ShapeVertices.Num() )
			{
				ShapeVertices.Remove( ShapeVertices.Num() - 1 );
			}

			bResult = TRUE;
		}
		else if( Key == KEY_Enter )
		{
			Apply();

			bResult = TRUE;
		}
	}

	if( bResult )
	{
		GEditor->RedrawLevelEditingViewports( TRUE );
	}
#endif // WITH_EDITORONLY_DATA

	return bResult;
}

void UGeomModifier_Pen::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();
	if( tool->GetCurrentModifier() != this )
	{
		return;
	}

	// Only draw in ortho viewports

	if( !((FEditorLevelViewportClient*)(Viewport->GetClient()))->IsOrtho() )
	{
		return;
	}

	FLinearColor Color = bCreateBrushShape ? GEngine->C_BrushShape : GEngine->C_BrushWire;

	// If we have more than 2 vertices placed, connect them with lines

	if( ShapeVertices.Num() > 1 )
	{
		for( INT v = 0 ; v < ShapeVertices.Num() - 1 ; ++v )
		{
			PDI->DrawLine( ShapeVertices(v), ShapeVertices( v+1 ), Color, SDPG_UnrealEdForeground );
		}
	}

	// Draw vertices for each point the user has put down

	for( INT v = 0 ; v < ShapeVertices.Num() ; ++v )
	{
		PDI->DrawPoint( ShapeVertices(v), Color, 6.f, SDPG_UnrealEdForeground );
	}

	if( ShapeVertices.Num() )
	{
		// Draw a dashed line from the last placed vertex to the current mouse position
		DrawDashedLine( PDI, ShapeVertices( ShapeVertices.Num() - 1 ), MouseWorldSpacePos, FLinearColor(1,0.5f,0), GEditor->Constraints.GetGridSize(), SDPG_UnrealEdForeground );
	}

	if( ShapeVertices.Num() > 2 )
	{
		// Draw a darkened dashed line to show what the completed shape will look like
		DrawDashedLine( PDI, ShapeVertices( ShapeVertices.Num() - 1 ), ShapeVertices( 0 ), FLinearColor(.5,0,0), GEditor->Constraints.GetGridSize(), SDPG_UnrealEdForeground );
	}

	// Draw a box where the next vertex will be placed

	INT BoxSz = Max( GEditor->Constraints.GetGridSize() / 2, 1.f );
	DrawWireBox(PDI, FBox::BuildAABB( MouseWorldSpacePos, FVector(BoxSz,BoxSz,BoxSz) ), FColor(255, 255, 255), SDPG_UnrealEdForeground);
}

void UGeomModifier_Pen::DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
}

void UGeomModifier_Pen::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	if( GCurrentLevelEditingViewportClient == ViewportClient )
	{
		// If the grid is enabled, figure out where the nearest grid location is to the mouse cursor
		FVector NewMouseWorldSpacePos = GEditor->Constraints.GridEnabled ? ViewportClient->MouseWorldSpacePos.GridSnap( GEditor->Constraints.GetGridSize() ) : ViewportClient->MouseWorldSpacePos;

		// If the mouse position has moved, update the viewport
		if( NewMouseWorldSpacePos != MouseWorldSpacePos )
		{
			MouseWorldSpacePos = NewMouseWorldSpacePos;
			GEditor->RedrawLevelEditingViewports( TRUE );
		}
	}
}

/*------------------------------------------------------------------------------
	UGeomModifier_Clip
------------------------------------------------------------------------------*/

namespace GeometryClipping {

/**
* Creates a giant brush aligned with this plane.
*
* @param	OutGiantBrush		[out] The new brush.
* @param	InPlane				Plane with which to align the brush.
*
* NOTE: it is up to the caller to set up the new brush upon return in regards to it's CSG operation and flags.
*/
static void BuildGiantAlignedBrush( ABrush& OutGiantBrush, const FPlane& InPlane )
{
	OutGiantBrush.Location = FVector(0,0,0);
	OutGiantBrush.PrePivot = FVector(0,0,0);

	verify( OutGiantBrush.Brush );
	verify( OutGiantBrush.Brush->Polys );

	OutGiantBrush.Brush->Polys->Element.Empty();

	// Create a list of vertices that can be used for the new brush
	FVector vtxs[8];

	FPlane FlippedPlane = InPlane.Flip();
	FPoly TempPoly = FPoly::BuildInfiniteFPoly( FlippedPlane );
	TempPoly.Finalize(&OutGiantBrush,0);
	vtxs[0] = TempPoly.Vertices(0);
	vtxs[1] = TempPoly.Vertices(1);
	vtxs[2] = TempPoly.Vertices(2);
	vtxs[3] = TempPoly.Vertices(3);

	FlippedPlane = FlippedPlane.Flip();
	FPoly TempPoly2 = FPoly::BuildInfiniteFPoly( FlippedPlane );
	vtxs[4] = TempPoly2.Vertices(0) + (TempPoly2.Normal * -(WORLD_MAX));	vtxs[5] = TempPoly2.Vertices(1) + (TempPoly2.Normal * -(WORLD_MAX));
	vtxs[6] = TempPoly2.Vertices(2) + (TempPoly2.Normal * -(WORLD_MAX));	vtxs[7] = TempPoly2.Vertices(3) + (TempPoly2.Normal * -(WORLD_MAX));

	// Create the polys for the new brush.
	FPoly newPoly;

	// TOP
	newPoly.Init();
	newPoly.Base = vtxs[0];
	newPoly.Vertices.AddItem( vtxs[0] );
	newPoly.Vertices.AddItem( vtxs[1] );
	newPoly.Vertices.AddItem( vtxs[2] );
	newPoly.Vertices.AddItem( vtxs[3] );
	newPoly.Finalize(&OutGiantBrush,0);
	new(OutGiantBrush.Brush->Polys->Element)FPoly(newPoly);

	// BOTTOM
	newPoly.Init();
	newPoly.Base = vtxs[4];
	newPoly.Vertices.AddItem( vtxs[4] );
	newPoly.Vertices.AddItem( vtxs[5] );
	newPoly.Vertices.AddItem( vtxs[6] );
	newPoly.Vertices.AddItem( vtxs[7] );
	newPoly.Finalize(&OutGiantBrush,0);
	new(OutGiantBrush.Brush->Polys->Element)FPoly(newPoly);

	// SIDES
	// 1
	newPoly.Init();
	newPoly.Base = vtxs[1];
	newPoly.Vertices.AddItem( vtxs[1] );
	newPoly.Vertices.AddItem( vtxs[0] );
	newPoly.Vertices.AddItem( vtxs[7] );
	newPoly.Vertices.AddItem( vtxs[6] );
	newPoly.Finalize(&OutGiantBrush,0);
	new(OutGiantBrush.Brush->Polys->Element)FPoly(newPoly);

	// 2
	newPoly.Init();
	newPoly.Base = vtxs[2];
	newPoly.Vertices.AddItem( vtxs[2] );
	newPoly.Vertices.AddItem( vtxs[1] );
	newPoly.Vertices.AddItem( vtxs[6] );
	newPoly.Vertices.AddItem( vtxs[5] );
	newPoly.Finalize(&OutGiantBrush,0);
	new(OutGiantBrush.Brush->Polys->Element)FPoly(newPoly);

	// 3
	newPoly.Init();
	newPoly.Base = vtxs[3];
	newPoly.Vertices.AddItem( vtxs[3] );
	newPoly.Vertices.AddItem( vtxs[2] );
	newPoly.Vertices.AddItem( vtxs[5] );
	newPoly.Vertices.AddItem( vtxs[4] );
	newPoly.Finalize(&OutGiantBrush,0);
	new(OutGiantBrush.Brush->Polys->Element)FPoly(newPoly);

	// 4
	newPoly.Init();
	newPoly.Base = vtxs[0];
	newPoly.Vertices.AddItem( vtxs[0] );
	newPoly.Vertices.AddItem( vtxs[3] );
	newPoly.Vertices.AddItem( vtxs[4] );
	newPoly.Vertices.AddItem( vtxs[7] );
	newPoly.Finalize(&OutGiantBrush,0);
	new(OutGiantBrush.Brush->Polys->Element)FPoly(newPoly);

	// Finish creating the new brush.
	OutGiantBrush.Brush->BuildBound();
}

/**
 * Clips the specified brush against the specified plane.
 *
 * @param		InPlane		The plane to clip against.
 * @param		InBrush		The brush to clip.
 * @return					The newly created brush representing the portion of the brush in the plane's positive halfspace.
 */
static ABrush* ClipBrushAgainstPlane(const FPlane& InPlane, ABrush* InBrush)
{
	ULevel* OldCurrentLevel = GWorld->CurrentLevel;

	// Create a giant brush in the level of the source brush to use in the intersection process.
	GWorld->CurrentLevel = InBrush->GetLevel();
	ABrush* ClippedBrush = NULL;

	// When clipping non-builder brushes, create a duplicate of the brush 
	// to clip. This duplicate will replace the existing brush. 
	if( !InBrush->IsABuilderBrush() )
	{
		// When duplicating an actor in a prefab instance, the engine will duplicate the 
		// entire prefab instance instead of just the actor. Thus, we must detach the original 
		// brush from prefab instance so we can duplicate and delete the original brush.
		// Detach the actor BEFORE attempting to select it. Otherwise, the whole instance is selected.
		if( InBrush->IsInPrefabInstance() )
		{
			// To remove an actor from a prefab instance, we must reset the archetype back to 
			// the class default object and remove the reference in the ArchetypeToInstance 
			// map on the prefab instance. Since the brush will be deleted, we don't have to 
			// worry about removing the reference. 
			InBrush->SetArchetype( InBrush->GetClass()->GetArchetype() );
		}

		// Select only the original brush to prevent other actors from being duplicated. 
		GEditor->SelectNone( FALSE, TRUE );
		GEditor->SelectActor( InBrush, TRUE, FALSE, FALSE );

		// Duplicate the original brush. This will serve as our clipped brush. 
		GEditor->edactDuplicateSelected( FALSE );

		// Clipped brush should be the only selected 
		// actor if the duplication didn't fail. 
		ClippedBrush = GEditor->GetSelectedActors()->GetTop<ABrush>();
	}
	// To clip the builder brush, instead of replacing it, spawn a 
	// temporary brush to clip. Then, copy that to the builder brush.
	else
	{
		// NOTE: This brush is discarded later on after copying the values to the builder brush. 
		ClippedBrush = CastChecked<ABrush>(GWorld->SpawnActor( InBrush->GetClass(),NAME_None,FVector(0,0,0), FRotator(0,0,0), InBrush ));
	}

	// It's possible that the duplication failed. 
	if( !ClippedBrush )
	{
		return NULL;
	}

	// The brushes should have the same class otherwise 
	// perhaps there were additional brushes were selected. 
	check( ClippedBrush->GetClass() == InBrush->GetClass() );

	ClippedBrush->Brush = new( InBrush->GetOuter(), NAME_None, RF_NotForClient|RF_NotForServer )UModel( NULL );
	ClippedBrush->BrushComponent->Brush = ClippedBrush->Brush;

	GeometryClipping::BuildGiantAlignedBrush( *ClippedBrush, InPlane );

	ClippedBrush->CsgOper = InBrush->CsgOper;
	ClippedBrush->SetFlags( InBrush->GetFlags() );
	ClippedBrush->PolyFlags = InBrush->PolyFlags;

	// Create a BSP for the brush that is being clipped.
	FBSPOps::bspBuild( InBrush->Brush, FBSPOps::BSP_Optimal, 15, 70, 1, 0 );
	FBSPOps::bspRefresh( InBrush->Brush, TRUE );
	FBSPOps::bspBuildBounds( InBrush->Brush );

	// Intersect the giant brush with the source brush's BSP.  This will give us the finished, clipping brush
	// contained inside of the giant brush.

	GEditor->bspBrushCSG( ClippedBrush, InBrush->Brush, 0, CSG_Intersect, FALSE, FALSE, TRUE );
	FBSPOps::bspUnlinkPolys( ClippedBrush->Brush );

	// Remove all polygons on the giant brush that don't match the normal of the clipping plane

	for( INT p = 0 ; p < ClippedBrush->Brush->Polys->Element.Num() ; ++p )
	{
		FPoly* P = &ClippedBrush->Brush->Polys->Element(p);

		if( P->Finalize( ClippedBrush, 1 ) == 0 )
		{
			if( !FPlane( P->Vertices(0), P->Normal ).Equals( InPlane, 0.01f ) )
			{
				ClippedBrush->Brush->Polys->Element.Remove( p );
				p = -1;
			}
		}
	}

	// The BSP "CSG_Intersect" code sometimes creates some nasty polygon fragments so clean those up here before going further.

	FPoly::OptimizeIntoConvexPolys( ClippedBrush, ClippedBrush->Brush->Polys->Element );

	// Clip each polygon in the original brush against the clipping plane.  For every polygon that is behind the plane or split by it, keep the back portion.

	FVector PlaneBase = FVector( InPlane.X, InPlane.Y, InPlane.Z ) * InPlane.W;

	for( INT p = 0 ; p < InBrush->Brush->Polys->Element.Num() ; ++p )
	{
		FPoly Poly = InBrush->Brush->Polys->Element(p);

		FPoly front, back;

		INT res = Poly.SplitWithPlane( PlaneBase, InPlane.SafeNormal(), &front, &back, TRUE );

		switch( res )
		{
			case SP_Back:
				ClippedBrush->Brush->Polys->Element.AddItem( Poly );
				break;

			case SP_Split:
				ClippedBrush->Brush->Polys->Element.AddItem( back );
				break;
		}
	}

	// At this point we have a clipped brush with optimized capping polygons so we can finish up by fixing it's ordering in the actor array and other misc things.

	ClippedBrush->CopyPosRotScaleFrom( InBrush );
	ClippedBrush->PolyFlags = InBrush->PolyFlags;

	// Clean the brush up.
	for( INT poly = 0 ; poly < ClippedBrush->Brush->Polys->Element.Num() ; poly++ )
	{
		FPoly* Poly = &(ClippedBrush->Brush->Polys->Element(poly));
		Poly->iLink = poly;
		Poly->Normal = FVector(0,0,0);
		Poly->Finalize(ClippedBrush,0);
	}

	// One final pass to clean the polyflags of all temporary settings.
	for( INT poly = 0 ; poly < ClippedBrush->Brush->Polys->Element.Num() ; poly++ )
	{
		FPoly* Poly = &(ClippedBrush->Brush->Polys->Element(poly));
		Poly->PolyFlags &= ~PF_EdCut;
		Poly->PolyFlags &= ~PF_EdProcessed;
	}

	// Move the new brush to where the new brush was to preserve brush ordering.
	ABrush* BuilderBrush = GWorld->GetBrush();
	if( InBrush == BuilderBrush )
	{
		// Special-case behaviour for the builder brush.

		// Copy the temporary brush back over onto the builder brush (keeping object flags)
		BuilderBrush->Modify();
		FBSPOps::csgCopyBrush( BuilderBrush, ClippedBrush, BuilderBrush->GetFlags(), 0, 0, TRUE );
		GWorld->EditorDestroyActor( ClippedBrush, FALSE );
		// Note that we're purposefully returning non-NULL here to report that the clip was successful,
		// even though the ClippedBrush has been destroyed!
	}
	else
	{
		// Remove the old brush.
		const INT ClippedBrushIndex = GWorld->CurrentLevel->Actors.Num() - 1;
		check( GWorld->CurrentLevel->Actors(ClippedBrushIndex) == ClippedBrush );
		GWorld->CurrentLevel->Actors.Remove(ClippedBrushIndex);

		// Add the new brush right after the old brush.
		const INT OldBrushIndex = GWorld->CurrentLevel->Actors.FindItemIndex( InBrush );
		check( OldBrushIndex != INDEX_NONE );
		GWorld->CurrentLevel->Actors.InsertItem( ClippedBrush, OldBrushIndex+1 );
	}

	// Restore the current level.
	GWorld->CurrentLevel = OldCurrentLevel;

	return ClippedBrush;
}

} // namespace GeometryClipping

/**
* Gives the modifier a chance to initialize it's internal state when activated.
*/

void UGeomModifier_Clip::WasActivated()
{
	ClipMarkers.Empty();
}

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Clip::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return mode->GetSelectionState() ? FALSE : TRUE;
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Clip::OnApply()
{
	ApplyClip( bSplit, bFlipNormal );
	return TRUE;
}

void UGeomModifier_Clip::ApplyClip( UBOOL InSplit, UBOOL InFlipNormal )
{
	if ( !GLastKeyLevelEditingViewportClient )
	{
		return;
	}

	// Assemble the set of selected brushes.
	TArray<ABrush*> Brushes;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor->IsBrush() )
		{
			Brushes.AddItem( static_cast<ABrush*>( Actor ) );
		}
	}

	// Do nothing if no brushes are selected.
	if ( Brushes.Num() == 0 )
	{
		return;
	}

	// Make sure enough clip markers have been placed.
	if( ClipMarkers.Num() != 2 )
	{
		GeomError( *LocalizeUnrealEd(TEXT("Error_NotEnoughClipMarkers")) );
		return;
	}

	// Focus has to be in an orthographic viewport so the editor can determine where the third point on the plane is
	if( !GLastKeyLevelEditingViewportClient->IsOrtho() )
	{
		GeomError( *LocalizeUnrealEd(TEXT("Error_BrushClipViewportNotOrthographic")) );
		return;
	}


	// Create a clipping plane based on ClipMarkers present in the level.
	const FVector vtx1 = ClipMarkers(0);
	const FVector vtx2 = ClipMarkers(1);
	FVector vtx3;

	// Compute the third vertex based on the viewport orientation.

	vtx3 = vtx1;

	switch( GLastKeyLevelEditingViewportClient->ViewportType )
	{
		case LVT_OrthoXY:
			vtx3.Z -= 64;
			break;

		case LVT_OrthoXZ:
			vtx3.Y -= 64;
			break;

		case LVT_OrthoYZ:
			vtx3.X -= 64;
			break;
	}

	// Perform the clip.
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd(TEXT("BrushClip")) );

		GEditor->SelectNone( FALSE, TRUE );

		// Clip the brush list.
		TArray<ABrush*> NewBrushes;
		TArray<ABrush*> OldBrushes;

		for ( INT BrushIndex = 0 ; BrushIndex < Brushes.Num() ; ++BrushIndex )
		{
			ABrush* SrcBrush = Brushes( BrushIndex );

			// Compute a clipping plane in the local frame of the brush.
			const FMatrix ToBrushLocal( SrcBrush->WorldToLocal() );
			const FVector LocalVtx1( ToBrushLocal.TransformFVector( vtx1 ) );
			const FVector LocalVtx2( ToBrushLocal.TransformFVector( vtx2 ) );
			const FVector LocalVtx3( ToBrushLocal.TransformFVector( vtx3 ) );

			FVector PlaneNormal( (LocalVtx2 - LocalVtx1) ^ (LocalVtx3 - LocalVtx1) );
			if( PlaneNormal.SizeSquared() < THRESH_ZERO_NORM_SQUARED )
			{
				GeomError( *LocalizeUnrealEd(TEXT("Error_ClipUnableToComputeNormal")) );
				continue;
			}
			PlaneNormal.Normalize();

			FPlane ClippingPlane( LocalVtx1, PlaneNormal );
			if ( InFlipNormal )
			{
				ClippingPlane = ClippingPlane.Flip();
			}
			
			// Is the brush a builder brush?
			const UBOOL bIsBuilderBrush = SrcBrush->IsABuilderBrush();

			// Perform the clip.
			UBOOL bCreatedBrush = FALSE;
			ABrush* NewBrush = GeometryClipping::ClipBrushAgainstPlane( ClippingPlane, SrcBrush );
			if ( NewBrush )
			{
				// Select the src brush for builders, or the returned brush for non-builders.
				if ( !bIsBuilderBrush )
				{
					NewBrushes.AddItem( NewBrush );
				}
				else
				{
					NewBrushes.AddItem( SrcBrush );
				}
				bCreatedBrush = TRUE;
			}

			// If we're doing a split instead of just a plain clip . . .
			if( InSplit )
			{
				// Don't perform a second clip if the builder brush was already split.
				if ( !bIsBuilderBrush || !bCreatedBrush )
				{
					// Clip the brush against the flipped clipping plane.
					ABrush* NewBrush2 = GeometryClipping::ClipBrushAgainstPlane( ClippingPlane.Flip(), SrcBrush );
					if ( NewBrush2 )
					{
						// We don't add the brush to the list of new brushes, so that only new brushes
						// in the non-cleaved halfspace of the clipping plane will be selected.
						bCreatedBrush = TRUE;
					}
				}
			}

			// Destroy source brushes that aren't builders.
			if ( !bIsBuilderBrush )
			{
				OldBrushes.AddItem( SrcBrush );
			}
		}

		// Clear selection to prevent the second clipped brush from being selected. 
		// When both are selected, it's hard to tell that the brush is clipped.
		GEditor->SelectNone( FALSE, TRUE );

		// Delete old brushes.
		for ( INT BrushIndex = 0 ; BrushIndex < OldBrushes.Num() ; ++BrushIndex )
		{
			ABrush* OldBrush = OldBrushes( BrushIndex );
			GWorld->EditorDestroyActor( OldBrush, TRUE );
		}

		// Select new brushes.
		for ( INT BrushIndex = 0 ; BrushIndex < NewBrushes.Num() ; ++BrushIndex )
		{
			ABrush* NewBrush = NewBrushes( BrushIndex );
			GEditor->SelectActor( NewBrush, TRUE, NULL, FALSE );
		}

		// Notify editor of new selection state.
		GEditor->NoteSelectionChange();
	}

	FEdModeGeometry* Mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	Mode->FinalizeSourceData();
	Mode->GetFromSource();
}

/**
 * @return		TRUE if the key was handled by this editor mode tool.
 */
UBOOL UGeomModifier_Clip::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	UBOOL bResult = FALSE;

	if( ViewportClient->IsOrtho() && Event == IE_Pressed )
	{
		const UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
		const UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
		const UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

		if( (bCtrlDown && !bShiftDown && !bAltDown && Key == KEY_RightMouseButton) || Key == KEY_SpaceBar )
		{
			// if the user has 2 markers placed and the click location is on top of the second point, perform the cllck.  This is a shortcut the LDs wanted.

			if( ClipMarkers.Num() == 2 )
			{
				const FVector* Pos = &ClipMarkers(1);

				if( Pos->Equals( SnappedMouseWorldSpacePos ) )
				{
					OnApply();
					return TRUE;
				}
			}

			// If there are already 2 clip markers in the world, clear them out.

			if( ClipMarkers.Num() > 1 )
			{
				ClipMarkers.Empty();
			}

			ClipMarkers.AddItem( SnappedMouseWorldSpacePos );
			bResult = TRUE;
		}
		else if( Key == KEY_Escape || Key == KEY_BackSpace )
		{
			if( ClipMarkers.Num() )
			{
				ClipMarkers.Remove( ClipMarkers.Num() - 1 );
			}

			bResult = TRUE;
		}
		else if( Key == KEY_Enter )
		{
			// If the user has 1 marker placed when they press ENTER, go ahead and place a second one at the current mouse location.
			// This allows LDs to place one point, move to a good spot and press ENTER for a quick clip.

			if( ClipMarkers.Num() == 1 )
			{
				ClipMarkers.AddItem( SnappedMouseWorldSpacePos );
			}

			ApplyClip( bAltDown, bShiftDown );

			bResult = TRUE;
		}
	}

	if( bResult )
	{
		GEditor->RedrawLevelEditingViewports( TRUE );
	}

	return bResult;
}

void UGeomModifier_Clip::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	FModeTool_GeometryModify* tool = (FModeTool_GeometryModify*)mode->GetCurrentTool();
	if( tool->GetCurrentModifier() != this )
	{
		return;
	}

	// Only draw in ortho viewports

	if( !((FEditorLevelViewportClient*)(Viewport->GetClient()))->IsOrtho() )
	{
		return;
	}

	// Draw a yellow box on each clip marker

	for( INT x = 0 ; x < ClipMarkers.Num() ; ++x )
	{
		FVector* vtx = &ClipMarkers(x);

		PDI->DrawPoint( *vtx, FLinearColor(1,0,0), 6.f, SDPG_UnrealEdForeground );
	}

	// If 2 markers are placed, draw a line connecting them and a line showing the clip normal.
	// If 1 marker is placed, draw a dashed line and normal to show where the clip plane will appear if the user commits.

	if( ClipMarkers.Num() )
	{
		FVector LineStart = ClipMarkers(0);
		FVector LineEnd = (ClipMarkers.Num() == 2) ? ClipMarkers(1) : SnappedMouseWorldSpacePos;

		if( ClipMarkers.Num() == 1 )
		{
			DrawDashedLine( PDI, LineStart, LineEnd, FLinearColor(1,.5,0), GEditor->Constraints.GetGridSize(), SDPG_UnrealEdForeground );
		}
		else
		{
			PDI->DrawLine( LineStart, LineEnd, FLinearColor(1,0,0), SDPG_UnrealEdForeground );
		}

		FVector vtx1, vtx2, vtx3;
		FPoly NormalPoly;

		vtx1 = LineStart;
		vtx2 = LineEnd;

		vtx3 = vtx1;

		const FEditorLevelViewportClient* ViewportClient = static_cast<FEditorLevelViewportClient*>( Viewport->GetClient() );
		switch( ViewportClient->ViewportType )
		{
			case LVT_OrthoXY:
				vtx3.Z -= 64;
				break;
			case LVT_OrthoXZ:
				vtx3.Y -= 64;
				break;
			case LVT_OrthoYZ:
				vtx3.X -= 64;
				break;
		}

		NormalPoly.Vertices.AddItem( vtx1 );
		NormalPoly.Vertices.AddItem( vtx2 );
		NormalPoly.Vertices.AddItem( vtx3 );

		if( !NormalPoly.CalcNormal(1) )
		{
			FVector Start = ( vtx1 + vtx2 ) / 2.f;

			FLOAT NormalLength = (vtx2 - vtx1).Size() / 2.f;

			if( ClipMarkers.Num() == 1 )
			{
				DrawDashedLine( PDI, Start, Start + NormalPoly.Normal * NormalLength, FLinearColor(1,.5,0), GEditor->Constraints.GetGridSize(), SDPG_UnrealEdForeground );
			}
			else
			{
				PDI->DrawLine( Start, Start + NormalPoly.Normal * NormalLength, FLinearColor(1,0,0), SDPG_UnrealEdForeground );
			}
		}
	}

	// Draw a box at the cursor location

	INT BoxSz = Max( GEditor->Constraints.GetGridSize() / 2, 1.f );
	DrawWireBox(PDI, FBox::BuildAABB( SnappedMouseWorldSpacePos, FVector(BoxSz,BoxSz,BoxSz) ), FColor(255, 255, 255), SDPG_UnrealEdForeground);
}

void UGeomModifier_Clip::DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
}

void UGeomModifier_Clip::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	if( GCurrentLevelEditingViewportClient == ViewportClient )
	{
		// Figure out where the nearest grid location is to the mouse cursor
		FVector NewSnappedMouseWorldSpacePos = ViewportClient->MouseWorldSpacePos.GridSnap( GEditor->Constraints.GetGridSize() );

		// If the snapped mouse position has moved, update the viewport
		if( NewSnappedMouseWorldSpacePos != SnappedMouseWorldSpacePos )
		{
			SnappedMouseWorldSpacePos = NewSnappedMouseWorldSpacePos;
			GEditor->RedrawLevelEditingViewports( TRUE );
		}
	}
}

/*------------------------------------------------------------------------------
	UGeomModifier_Delete
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Delete::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return (mode->HavePolygonsSelected() || mode->HaveVerticesSelected());
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Delete::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	UBOOL bHandled = FALSE;

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;

		// Polys

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);

			if( gp->IsSelected() )
			{
				gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element( gp->ActualPolyIndex ).PolyFlags |= PF_GeomMarked;
				bHandled = 1;
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

		// Verts

		for( INT v = 0 ; v < go->VertexPool.Num() ; ++v )
		{
			FGeomVertex* gv = &go->VertexPool(v);

			if( gv->IsSelected() )
			{
				for( INT x = 0 ; x < gv->GetParentObject()->GetActualBrush()->Brush->Polys->Element.Num() ; ++x )
				{
					FPoly* Poly = &gv->GetParentObject()->GetActualBrush()->Brush->Polys->Element(x);
					Poly->RemoveVertex( *gv );
					bHandled = 1;
				}
			}
		}
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();

	return bHandled;
}

/*------------------------------------------------------------------------------
	UGeomModifier_Create
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Create::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return mode->HaveVerticesSelected();
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Create::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;

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

				new(NewPoly->Vertices) FVector(*gv);
			}

			NewPoly->Normal = FVector(0,0,0);
			NewPoly->Base = *Verts(0);
			NewPoly->PolyFlags = PF_DefaultFlags;
		}
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();

	return TRUE;
}

/*------------------------------------------------------------------------------
	UGeomModifier_Flip
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Flip::Supports()
{
	// Supports polygons selected and objects selected

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return (!mode->HaveEdgesSelected() && !mode->HaveVerticesSelected());
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Flip::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	UBOOL bHavePolygonsSelected = mode->HavePolygonsSelected();

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);

			if( gp->IsSelected() || !bHavePolygonsSelected )
			{
				FPoly* Poly = &go->GetActualBrush()->Brush->Polys->Element( gp->ActualPolyIndex );
				Poly->Reverse();
			}
		}
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();

	return TRUE;
}

/*------------------------------------------------------------------------------
	UGeomModifier_Split
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Split::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	// This modifier assumes that a single geometry object is selected

	if( mode->CountObjectsSelected() != 1 )
	{
		return FALSE;
	}

	INT NumPolygonsSelected = mode->CountSelectedPolygons();
	INT NumEdgesSelected = mode->CountSelectedEdges();
	INT NumVerticesSelected = mode->CountSelectedVertices();

	if( (NumPolygonsSelected == 1 && NumEdgesSelected == 1 && NumVerticesSelected == 0)			// Splitting a face at an edge mid point (scalpel)
			|| (NumPolygonsSelected == 0 && NumEdgesSelected > 0 && NumVerticesSelected == 0)	// Splitting a brush at an edge mid point (ring cut)
			|| (NumPolygonsSelected == 1 && NumEdgesSelected == 0 && NumVerticesSelected == 2)	// Splitting a polygon across 2 vertices
			|| (NumPolygonsSelected == 0 && NumEdgesSelected == 0 && NumVerticesSelected == 2)	// Splitting a brush across 2 vertices
			)
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Split::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	// Get a pointer to the selected geom object

	FGeomObject* GeomObject = NULL;
	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		GeomObject = *Itor;
		break;
	}

	if( GeomObject == NULL )
	{
		return FALSE;
	}

	// Count up how many of each subobject are selected so we can determine what the user is trying to split

	INT NumPolygonsSelected = mode->CountSelectedPolygons();
	INT NumEdgesSelected = mode->CountSelectedEdges();
	INT NumVerticesSelected = mode->CountSelectedVertices();

	if( NumPolygonsSelected == 1 && NumEdgesSelected == 1 && NumVerticesSelected == 0 )
		{
		//
		// Splitting a face at an edge mid point (scalpel)
		//

		// Get the selected edge
		TArray<FGeomEdge*> Edges;
		mode->GetSelectedEdges( Edges );
		check( Edges.Num() == 1 );

		FGeomEdge* SelectedEdge = Edges(0);

		// Figure out the verts that are part of that edge

		FGeomVertex* Vertex0 = &GeomObject->VertexPool( SelectedEdge->VertexIndices[0] );
		FGeomVertex* Vertex1 = &GeomObject->VertexPool( SelectedEdge->VertexIndices[1] );

		const FVector Vtx0 = *Vertex0->GetActualVertex( Vertex0->ActualVertexIndices(0) );
		const FVector Vtx1 = *Vertex1->GetActualVertex( Vertex1->ActualVertexIndices(0) );

		// Get the selected polygon
		TArray<FGeomPoly*> Polygons;
		mode->GetSelectedPolygons( Polygons );
		check( Polygons.Num() == 1 );

		FGeomPoly* Polygon = Polygons(0);
		FPoly* SelectedPoly = Polygon->GetActualPoly();

		// Get the selected brush
		ABrush* Brush = GeomObject->GetActualBrush();

		//
		// Sanity checking
		//
			{
			// 1. Make sure that the selected edge is part of the selected polygon

			if( !SelectedPoly->Vertices.ContainsItem( Vtx0 ) || !SelectedPoly->Vertices.ContainsItem( Vtx1 ) )
				{
				GeomError( *LocalizeUnrealEd(TEXT("Error_SelectedEdgeMustBelongToSelectedPoly")) );
				return FALSE;
			}
		}

		// Generate a base and a normal for the cutting plane

		const FVector PlaneNormal( (Vtx1 - Vtx0).SafeNormal() );
		const FVector PlaneBase = 0.5f*(Vtx1 + Vtx0);

		// Clip the selected polygon against the cutting plane

		FPoly Front, Back;
		Front.Init();
		Back.Init();

		INT Res = SelectedPoly->SplitWithPlane( PlaneBase, PlaneNormal, &Front, &Back, 1 );

		if( Res == SP_Split )
		{
			TArray<FPoly> NewPolygons;

			NewPolygons.AddItem( Front );
			NewPolygons.AddItem( Back );

			// At this point, see if any other polygons in the brush need to have a vertex added to an edge

			FPlane CuttingPlane( PlaneBase, PlaneNormal );

			for( INT p = 0 ; p < Brush->Brush->Polys->Element.Num() ; ++p )
			{
				FPoly* P = &Brush->Brush->Polys->Element(p);

				if( P != SelectedPoly )
				{
					for( INT v = 0 ; v < P->Vertices.Num() ; ++v )
					{
						FVector* v0 = &P->Vertices(v);
						FVector* v1 = &P->Vertices( (v + 1) % P->Vertices.Num() );

						// Make sure the line formed by the edge actually crosses the plane before checking for the intersection point.

						if( IsNegativeFloat( CuttingPlane.PlaneDot( *v0 ) ) != IsNegativeFloat( CuttingPlane.PlaneDot( *v1 ) ) )
		{
							FVector Intersection = FLinePlaneIntersection( *v0, *v1, CuttingPlane );

							// Make sure that the intersection point lies on the same plane as the selected polygon as we only need to add it there and not
							// to any other edge that might intersect the cutting plane.

							if( SelectedPoly->OnPlane( Intersection ) )
			{
								P->Vertices.InsertItem( Intersection, (v+1) % P->Vertices.Num() );
				break;
			}
		}
					}

					NewPolygons.AddItem( *P );
				}

			}

			// Replace the old polygon list with the new one

			Brush->Brush->Polys->Element = NewPolygons;
		}
	}
	else if( NumPolygonsSelected == 0 && NumEdgesSelected > 0 && NumVerticesSelected == 0 )
		{
		//
		// Splitting a brush at an edge mid point (ring cut)
		//

		// Get the selected edge
		TArray<FGeomEdge*> Edges;
		mode->GetSelectedEdges( Edges );
		check( Edges.Num() > 0 );

		FGeomEdge* Edge = Edges(0);

		// Generate a base and a normal for the cutting plane

		FGeomVertex* Vertex0 = &GeomObject->VertexPool( Edge->VertexIndices[0] );
		FGeomVertex* Vertex1 = &GeomObject->VertexPool( Edge->VertexIndices[1] );

		const FVector v0 = *Vertex0->GetActualVertex( Vertex0->ActualVertexIndices(0) );
		const FVector v1 = *Vertex1->GetActualVertex( Vertex1->ActualVertexIndices(0) );
		const FVector PlaneNormal( (v1 - v0).SafeNormal() );
		const FVector PlaneBase = 0.5f*(v1 + v0);

		ABrush* Brush = GeomObject->GetActualBrush();

		// The polygons for the new brush are stored in here and the polys inside of the original brush are replaced at the end of the loop

		TArray<FPoly> NewPolygons;

		// Clip each polygon against the cutting plane

		for( INT p = 0 ; p < Brush->Brush->Polys->Element.Num() ; ++p )
		{
			FPoly* Poly = &Brush->Brush->Polys->Element(p);

			FPoly Front, Back;
			Front.Init();
			Back.Init();

			INT Res = Poly->SplitWithPlane( PlaneBase, PlaneNormal, &Front, &Back, 1 );
			switch( Res )
			{
				case SP_Split:
					NewPolygons.AddItem( Front );
					NewPolygons.AddItem( Back );
					break;

				default:
					NewPolygons.AddItem( *Poly );
					break;
			}
	}

		// Replace the old polygon list with the new one

		Brush->Brush->Polys->Element = NewPolygons;
	}
	else if( NumPolygonsSelected == 1 && NumEdgesSelected == 0 && NumVerticesSelected == 2 )
	{
		//
		// Splitting a polygon across 2 vertices
		//

		// Get the selected verts
		TArray<FGeomVertex*> Verts;
		mode->GetSelectedVertices( Verts );
		check( Verts.Num() == 2 );

		FGeomVertex* Vertex0 = Verts(0);
		FGeomVertex* Vertex1 = Verts(1);

		const FVector v0 = *Vertex0->GetActualVertex( Vertex0->ActualVertexIndices(0) );
		const FVector v1 = *Vertex1->GetActualVertex( Vertex1->ActualVertexIndices(0) );

		// Get the selected polygon
		TArray<FGeomPoly*> Polys;
		mode->GetSelectedPolygons( Polys );
		check( Polys.Num() == 1 );

		FGeomPoly* SelectedPoly = Polys(0);
		FPoly* Poly = SelectedPoly->GetActualPoly();

		//
		// Sanity checking
		//
		{
			// 1. Make sure that the selected vertices are part of the selected polygon

			if( !SelectedPoly->GetActualPoly()->Vertices.ContainsItem( v0 ) || !SelectedPoly->GetActualPoly()->Vertices.ContainsItem( v1 ) )
			{
				GeomError( *LocalizeUnrealEd(TEXT("Error_SelectedVerticesMustBelongToSelectedPoly")) );
				return FALSE;
				}
			}

		// Generate a base and a normal for the cutting plane

		FVector v2 = v0 + (SelectedPoly->GetNormal() * 64.0f);

		const FPlane PlaneNormal( v0, v1, v2 );
		const FVector PlaneBase = 0.5f*(v1 + v0);

		ABrush* Brush = GeomObject->GetActualBrush();

		// The polygons for the new brush are stored in here and the polys inside of the original brush are replaced at the end of the loop

		TArray<FPoly> NewPolygons;

		// Clip the selected polygon against the cutting plane.

		for( INT p = 0 ; p < Brush->Brush->Polys->Element.Num() ; ++p )
					{
			FPoly* P = &Brush->Brush->Polys->Element(p);

			if( P == Poly )
			{
				FPoly Front, Back;
				Front.Init();
				Back.Init();

				INT Res = P->SplitWithPlane( PlaneBase, PlaneNormal, &Front, &Back, 1 );
				switch( Res )
						{
				case SP_Split:
					NewPolygons.AddItem( Front );
					NewPolygons.AddItem( Back );
					break;

				default:
					NewPolygons.AddItem( *P );
					break;
						}
					}
			else
			{
				NewPolygons.AddItem( *P );
				}
			}

		// Replace the old polygon list with the new one

		Brush->Brush->Polys->Element = NewPolygons;
		}
	else if( NumPolygonsSelected == 0 && NumEdgesSelected == 0 && NumVerticesSelected == 2 )
	{
		//
		// Splitting a brush across 2 vertices
		//

		// Get the selected verts
		TArray<FGeomVertex*> Verts;
		mode->GetSelectedVertices( Verts );
		check( Verts.Num() == 2 );

		// Generate a base and a normal for the cutting plane

		FGeomVertex* Vertex0 = Verts(0);
		FGeomVertex* Vertex1 = Verts(1);

		const FVector v0 = *Vertex0->GetActualVertex( Vertex0->ActualVertexIndices(0) );
		const FVector v1 = *Vertex1->GetActualVertex( Vertex1->ActualVertexIndices(0) );

		FVector v2 = ((Vertex0->GetNormal() + Vertex1->GetNormal()) / 2.0f) * 64.f;

		const FPlane PlaneNormal( v0, v1, v2 );
		const FVector PlaneBase = 0.5f*(v1 + v0);

		ABrush* Brush = GeomObject->GetActualBrush();

		// The polygons for the new brush are stored in here and the polys inside of the original brush are replaced at the end of the loop

		TArray<FPoly> NewPolygons;

		// Clip each polygon against the cutting plane

		for( INT p = 0 ; p < Brush->Brush->Polys->Element.Num() ; ++p )
		{
			FPoly* Poly = &Brush->Brush->Polys->Element(p);

			FPoly Front, Back;
			Front.Init();
			Back.Init();

			INT Res = Poly->SplitWithPlane( PlaneBase, PlaneNormal, &Front, &Back, 1 );
			switch( Res )
			{
			case SP_Split:
				NewPolygons.AddItem( Front );
				NewPolygons.AddItem( Back );
				break;

			default:
				NewPolygons.AddItem( *Poly );
				break;
			}
		}

		// Replace the old polygon list with the new one

		Brush->Brush->Polys->Element = NewPolygons;
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();

	return TRUE;
}

/*------------------------------------------------------------------------------
	UGeomModifier_Triangulate
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Triangulate::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return (!mode->HaveEdgesSelected() && !mode->HaveVerticesSelected());
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Triangulate::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	UBOOL bHavePolygonsSelected = mode->HavePolygonsSelected();

	// Mark the selected polygons so we can find them in the next loop, and create
	// a local list of FPolys to triangulate later.

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;

		TArray<FPoly> PolyList;

		for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
		{
			FGeomPoly* gp = &go->PolyPool(p);

			if( gp->IsSelected() || !bHavePolygonsSelected )
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
			TArray<FPoly> Triangles;
			PolyList(p).Triangulate( go->GetActualBrush(), Triangles );

			for( INT t = 0 ; t < Triangles.Num() ; ++t )
			{
				go->GetActualBrush()->Brush->Polys->Element.AddItem( Triangles(t) );
			}
		}
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();

	return TRUE;
}

/*------------------------------------------------------------------------------
UGeomModifier_Optimize
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Optimize::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return !mode->HaveVerticesSelected() && !mode->HaveEdgesSelected();
}

/**
* Implements the modifier application.
*/
UBOOL UGeomModifier_Optimize::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	TArray<FPoly> Polygons;

	if( mode->HavePolygonsSelected() )
	{
		for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
		{
			FGeomObject* go = *Itor;
			ABrush* ActualBrush = go->GetActualBrush();

			// Gather a list of polygons that are 
			for( INT p = 0 ; p < go->PolyPool.Num() ; ++p )
			{
				FGeomPoly* gp = &go->PolyPool(p);

				if( gp->IsSelected() )
				{
					ActualBrush->Brush->Polys->Element( gp->ActualPolyIndex ).PolyFlags |= PF_GeomMarked;
					Polygons.AddItem( ActualBrush->Brush->Polys->Element( gp->ActualPolyIndex ) );
				}
			}

			// Delete existing polygons

			for( INT p = 0 ; p < go->GetActualBrush()->Brush->Polys->Element.Num() ; ++p )
			{
				if( (ActualBrush->Brush->Polys->Element( p ).PolyFlags&PF_GeomMarked) > 0 )
				{
					ActualBrush->Brush->Polys->Element.Remove( p );
					p = -1;
				}
			}

			// Optimize the polygons in the list

			FPoly::OptimizeIntoConvexPolys( ActualBrush, Polygons );

			// Copy the new polygons into the brush

			for( INT p = 0 ; p < Polygons.Num() ; ++p )
			{
				FPoly Poly = Polygons(p);

				Poly.PolyFlags &= ~PF_GeomMarked;

				ActualBrush->Brush->Polys->Element.AddItem( Poly );
			}
		}
	}
	else
	{
		for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
		{
			FGeomObject* go = *Itor;
			ABrush* ActualBrush = go->GetActualBrush();

			// Optimize the polygons

			FPoly::OptimizeIntoConvexPolys( ActualBrush, ActualBrush->Brush->Polys->Element );
		}
	}

	mode->FinalizeSourceData();
	mode->GetFromSource();

	return TRUE;
}

/*------------------------------------------------------------------------------
	UGeomModifier_Turn
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Turn::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return mode->HaveEdgesSelected();
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Turn::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	// Edges

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;

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

				if( Poly->Vertices.Num() != 3 )
				{
					GeomError( *LocalizeUnrealEd(TEXT("Error_PolygonsOnEdgeToTurnMustBeTriangles")) );
					EndTrans();
					return 0;
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

				Quad.AddItem( Poly->Vertices(idx0) );
				Quad.AddItem( Poly->Vertices(idx2) );
				Quad.AddItem( Poly->Vertices(idx1) );

				gp = &go->PolyPool( ge->ParentPolyIndices(1) );
				Poly = gp->GetActualPoly();
				FPoly SavePoly1 = *Poly;

				for( INT v = 0 ; v < Poly->Vertices.Num() ; ++v )
				{
					Quad.AddUniqueItem( Poly->Vertices(v) );
				}

				// Create new polygons

				FPoly* NewPoly;

				NewPoly = new( gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();

				NewPoly->Init();
				new(NewPoly->Vertices) FVector(Quad(2));
				new(NewPoly->Vertices) FVector(Quad(1));
				new(NewPoly->Vertices) FVector(Quad(3));

				NewPoly->Base = SavePoly0.Base;
				NewPoly->Material = SavePoly0.Material;
				NewPoly->PolyFlags = SavePoly0.PolyFlags;
				NewPoly->TextureU = SavePoly0.TextureU;
				NewPoly->TextureV = SavePoly0.TextureV;
				NewPoly->Normal = FVector(0,0,0);
				NewPoly->Finalize(go->GetActualBrush(),1);

				NewPoly = new( gp->GetParentObject()->GetActualBrush()->Brush->Polys->Element )FPoly();

				NewPoly->Init();
				new(NewPoly->Vertices) FVector(Quad(3));
				new(NewPoly->Vertices) FVector(Quad(1));
				new(NewPoly->Vertices) FVector(Quad(0));

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

	mode->FinalizeSourceData();
	mode->GetFromSource();

	return TRUE;
}

/*------------------------------------------------------------------------------
	UGeomModifier_Weld
------------------------------------------------------------------------------*/

/**
* @return		TRUE if this modifier will work on the currently selected sub objects.
*/
UBOOL UGeomModifier_Weld::Supports()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return mode->HaveVerticesSelected();
}

/**
 * Implements the modifier application.
 */
UBOOL UGeomModifier_Weld::OnApply()
{
	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	// Verts

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		FGeomObject* go = *Itor;

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

	mode->FinalizeSourceData();
	mode->GetFromSource();

	return TRUE;
}
