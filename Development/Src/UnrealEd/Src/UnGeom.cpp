/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "..\..\UnrealEd\Inc\Utils.h"
#include "..\..\UnrealEd\Inc\ResourceIDs.h"
#include "..\..\UnrealEd\Inc\PropertyWindow.h"

///////////////////////////////////////////////////////////////////////////////
//
// FGeomBase
//
///////////////////////////////////////////////////////////////////////////////

FGeomBase::FGeomBase()
{
	SelectionIndex = INDEX_NONE;
	Normal = FVector(0,0,0);
	ParentObjectIndex = INDEX_NONE;
}

void FGeomBase::Select( UBOOL InSelect )
{
	FEditorModeTools& Tools = GEditorModeTools();
	check( Tools.IsModeActive(EM_Geometry) );

	if( InSelect )
	{
		SelectionIndex = GetParentObject()->GetNewSelectionIndex();
	}
	else
	{
		SelectionIndex = INDEX_NONE;
	}

	// If something is selected, move the pivot and snap locations to the widget location.
	if( IsSelected() )
	{
		Tools.PivotLocation = Tools.SnappedLocation = GetWidgetLocation();
	}

	GetParentObject()->DirtySelectionOrder();
}

FGeomObject* FGeomBase::GetParentObject()
{
	check( GEditorModeTools().IsModeActive(EM_Geometry) );
	check( ParentObjectIndex > INDEX_NONE );

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return mode->GetGeomObject( ParentObjectIndex );
}

const FGeomObject* FGeomBase::GetParentObject() const
{
	check( GEditorModeTools().IsModeActive(EM_Geometry) );
	check( ParentObjectIndex > INDEX_NONE );

	const FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);
	return mode->GetGeomObject( ParentObjectIndex );
}

///////////////////////////////////////////////////////////////////////////////
//
// FGeomVertex
//
///////////////////////////////////////////////////////////////////////////////

FGeomVertex::FGeomVertex()
{
	X = Y = Z = 0;
	ParentObjectIndex = INDEX_NONE;
}

FVector FGeomVertex::GetWidgetLocation()
{
	return GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( *this );
}

FVector FGeomVertex::GetMidPoint() const
{
	return *this;
}

FVector* FGeomVertex::GetActualVertex( FPolyVertexIndex& InPVI )
{
	return &(GetParentObject()->GetActualBrush()->Brush->Polys->Element( InPVI.PolyIndex ).Vertices( InPVI.VertexIndex ));
}

///////////////////////////////////////////////////////////////////////////////
//
// FGeomEdge
//
///////////////////////////////////////////////////////////////////////////////

FGeomEdge::FGeomEdge()
{
	ParentObjectIndex = INDEX_NONE;
	VertexIndices[0] = INDEX_NONE;
	VertexIndices[1] = INDEX_NONE;
}

// Returns true if InEdge matches this edge, independant of winding.
UBOOL FGeomEdge::IsSameEdge( const FGeomEdge& InEdge ) const
{
	return (( VertexIndices[0] == InEdge.VertexIndices[0] && VertexIndices[1] == InEdge.VertexIndices[1] ) ||
			( VertexIndices[0] == InEdge.VertexIndices[1] && VertexIndices[1] == InEdge.VertexIndices[0] ));
}

FVector FGeomEdge::GetWidgetLocation()
{
	FVector dir = (GetParentObject()->VertexPool( VertexIndices[1] ) - GetParentObject()->VertexPool( VertexIndices[0] ));
	const FLOAT dist = dir.Size() / 2;
	dir.Normalize();
	const FVector loc = GetParentObject()->VertexPool( VertexIndices[0] ) + (dir * dist);
	return GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( loc );
}

FVector FGeomEdge::GetMidPoint() const
{
	const FGeomVertex* wk0 = &(GetParentObject()->VertexPool( VertexIndices[0] ));
	const FGeomVertex* wk1 = &(GetParentObject()->VertexPool( VertexIndices[1] ));

	const FVector v0( wk0->X, wk0->Y, wk0->Z );
	const FVector v1( wk1->X, wk1->Y, wk1->Z );

	return (v0 + v1) / 2;
}

///////////////////////////////////////////////////////////////////////////////
//
// FGeomPoly
//
///////////////////////////////////////////////////////////////////////////////

FGeomPoly::FGeomPoly()
{
	ParentObjectIndex = INDEX_NONE;
}

FVector FGeomPoly::GetWidgetLocation()
{
	const FPoly* Poly = GetActualPoly();
	FVector Wk(0.f,0.f,0.f);
	if ( Poly->Vertices.Num() > 0 )
	{
		for( INT VertIndex = 0 ; VertIndex < Poly->Vertices.Num() ; ++VertIndex )
		{
			Wk += Poly->Vertices(VertIndex);
		}
		Wk = Wk / Poly->Vertices.Num();
	}

	return GetParentObject()->GetActualBrush()->LocalToWorld().TransformFVector( Wk );
}

FVector FGeomPoly::GetMidPoint() const
{
	FVector Wk(0,0,0);
	INT Count = 0;

	for( INT e = 0 ; e < EdgeIndices.Num() ; ++e )
	{
		const FGeomEdge* ge = &GetParentObject()->EdgePool( EdgeIndices(e) );
		Wk += GetParentObject()->VertexPool( ge->VertexIndices[0] );
		Count++;
		Wk += GetParentObject()->VertexPool( ge->VertexIndices[1] );
		Count++;
	}

	check( Count );
	return Wk / Count;
}

FPoly* FGeomPoly::GetActualPoly()
{
	FGeomObject* Parent = GetParentObject();
	check( Parent );
	ABrush* Brush = Parent->GetActualBrush();
	check( Brush );

	return &( Brush->Brush->Polys->Element(ActualPolyIndex) );
}

///////////////////////////////////////////////////////////////////////////////
//
// FGeomObject
//
///////////////////////////////////////////////////////////////////////////////

FGeomObject::FGeomObject():
	LastSelectionIndex( INDEX_NONE )
{
	DirtySelectionOrder();
	ActualBrush = NULL;
}

FVector FGeomObject::GetWidgetLocation()
{
	return GetActualBrush()->Location;
}

INT FGeomObject::AddVertexToPool( INT InObjectIndex, INT InParentPolyIndex, INT InPolyIndex, INT InVertexIndex )
{
	FGeomVertex* gv = NULL;
	FVector CmpVtx = GetActualBrush()->Brush->Polys->Element( InPolyIndex ).Vertices(InVertexIndex);

	// See if the vertex is already in the pool
	for( INT x = 0 ; x < VertexPool.Num() ; ++x )
	{
		if( FPointsAreNear( VertexPool(x), CmpVtx, 0.5f ) )
		{
			gv = &VertexPool(x);
			gv->ActualVertexIndices.AddUniqueItem( FPolyVertexIndex( InPolyIndex, InVertexIndex ) );
			gv->ParentPolyIndices.AddUniqueItem( InParentPolyIndex );
			return x;
		}
	}

	// If not, add it...
	if( gv == NULL )
	{
		new( VertexPool )FGeomVertex();
		gv = &VertexPool( VertexPool.Num()-1 );
		*gv = CmpVtx;
	}

	gv->ActualVertexIndices.AddUniqueItem( FPolyVertexIndex( InPolyIndex, InVertexIndex ) );
	gv->SetParentObjectIndex( InObjectIndex );
	gv->ParentPolyIndices.AddUniqueItem( InParentPolyIndex );

	return VertexPool.Num()-1;
}

INT FGeomObject::AddEdgeToPool( FGeomPoly* InPoly, INT InParentPolyIndex, INT InVectorIdxA, INT InVectorIdxB )
{
	const INT idx0 = AddVertexToPool( InPoly->GetParentObjectIndex(), InParentPolyIndex, InPoly->ActualPolyIndex, InVectorIdxA );
	const INT idx1 = AddVertexToPool( InPoly->GetParentObjectIndex(), InParentPolyIndex, InPoly->ActualPolyIndex, InVectorIdxB );

	// See if the edge is already in the pool.  If so, leave.
	FGeomEdge* ge = NULL;
	for( INT e = 0 ; e < EdgePool.Num() ; ++e )
	{
		if( EdgePool(e).VertexIndices[0] == idx0 && EdgePool(e).VertexIndices[1] == idx1 )
		{
			EdgePool(e).ParentPolyIndices.AddItem( InPoly->GetParentObject()->PolyPool.FindItemIndex( *InPoly ) );
			return e;
		}
	}

	// Add a new edge to the pool and set it up.
	new( EdgePool )FGeomEdge();
	ge = &EdgePool( EdgePool.Num()-1 );

	ge->VertexIndices[0] = idx0;
	ge->VertexIndices[1] = idx1;

	ge->ParentPolyIndices.AddItem( InPoly->GetParentObject()->PolyPool.FindItemIndex( *InPoly ) );
	ge->SetParentObjectIndex( InPoly->GetParentObjectIndex() );

	return EdgePool.Num()-1;
}

/**
 * Removes all geometry data and reconstructs it from the source brushes.
 */

void FGeomObject::GetFromSource()
{
	PolyPool.Empty();
	EdgePool.Empty();
	VertexPool.Empty();

	for( INT p = 0 ; p < GetActualBrush()->Brush->Polys->Element.Num() ; ++p )
	{
		FPoly* poly = &(GetActualBrush()->Brush->Polys->Element(p));

		new( PolyPool )FGeomPoly();
		FGeomPoly* gp = &PolyPool( PolyPool.Num()-1 );
		gp->SetParentObjectIndex( GetObjectIndex() );
		gp->ActualPolyIndex = p;

		for( int v = 1 ; v <= poly->Vertices.Num() ; ++v )
		{
			int idx = (v == poly->Vertices.Num()) ? 0 : v,
				previdx = v-1;

			INT eidx = AddEdgeToPool( gp, PolyPool.Num()-1, previdx, idx );
			gp->EdgeIndices.AddItem( eidx );
		}
	}

	ComputeData();
}

INT FGeomObject::GetObjectIndex()
{
	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return INDEX_NONE;
	}

	FEdModeGeometry* mode = (FEdModeGeometry*)GEditorModeTools().GetActiveMode(EM_Geometry);

	for( FEdModeGeometry::TGeomObjectIterator Itor( mode->GeomObjectItor() ) ; Itor ; ++Itor )
	{
		if( *Itor == this )
		{
			return Itor.GetIndex();
		}
	}

	check(0);	// Should never happen
	return INDEX_NONE;
}

/**
 * Sends the vertex data that we have back to the source vertices.
 */

void FGeomObject::SendToSource()
{
	for( INT v = 0 ; v < VertexPool.Num() ; ++v )
	{
		FGeomVertex* gv = &VertexPool(v);

		for( INT x = 0 ; x < gv->ActualVertexIndices.Num() ; ++x )
		{
			FVector* vtx = gv->GetActualVertex( gv->ActualVertexIndices(x) );
			vtx->X = gv->X;
			vtx->Y = gv->Y;
			vtx->Z = gv->Z;
		}
	}
}

/**
 * Finalizes the source geometry by checking for invalid polygons,
 * updating components, etc. - anything that needs to be done
 * before the engine will accept the resulting brushes/polygons
 * as valid.
 */

UBOOL FGeomObject::FinalizeSourceData()
{
	if( !GEditorModeTools().IsModeActive(EM_Geometry) )
	{
		return 0;
	}

	ABrush* brush = GetActualBrush();
	UBOOL Ret = FALSE;
	DOUBLE StartTime = appSeconds();
	const DOUBLE TimeLimit = 10.0;

	// Remove invalid polygons from the brush

	for( INT x = 0 ; x < brush->Brush->Polys->Element.Num() ; ++x )
	{
		FPoly* Poly = &brush->Brush->Polys->Element(x);

		if( Poly->Vertices.Num() < 3 )
		{
			brush->Brush->Polys->Element.Remove( x );
			x = -1;
		}
	}

	for( INT p = 0 ; p < brush->Brush->Polys->Element.Num() ; ++p )
	{
		FPoly* Poly = &(brush->Brush->Polys->Element(p));
		Poly->iLink = p;
		INT SaveNumVertices = Poly->Vertices.Num();

		if( !Poly->IsCoplanar() || !Poly->IsConvex() )
		{
			// If the polygon is no longer coplanar and/or convex, break it up into separate triangles.

			FPoly WkPoly = *Poly;
			brush->Brush->Polys->Element.Remove( p );

			TArray<FPoly> Polygons;
			if( WkPoly.Triangulate( brush, Polygons ) > 0 )
			{
				FPoly::OptimizeIntoConvexPolys( brush, Polygons );

				for( INT t = 0 ; t < Polygons.Num() ; ++t )
				{
					brush->Brush->Polys->Element.AddItem( Polygons(t) );
				}
			}
			p = -1;
			Ret = TRUE;

			// if this is a 'problem' poly and the time limit has expired then there is a potential
			// infinite loop so remove this poly
			if(TimeLimit < appSeconds() - StartTime)
			{
				brush->Brush->Polys->Element.Remove( p );
			}
		}
		else
		{
			INT FixResult = Poly->Fix();
			if( FixResult != SaveNumVertices )
			{
				// If the polygon collapses after running "Fix" against it, it needs to be
				// removed from the brushes polygon list.

				if( FixResult == 0 )
				{
					brush->Brush->Polys->Element.Remove( p );
				}

				p = -1;
				Ret = TRUE;

				// if this is a 'problem' poly and the time limit has expired then there is a potential
				// infinite loop so remove this poly
				if(TimeLimit < appSeconds() - StartTime)
				{
					brush->Brush->Polys->Element.Remove( p );
				}
			}
			else
			{
				// If we get here, the polygon is valid and needs to be kept.  Finalize its internals.

				Poly->Finalize(brush,1);
			}
		}
	}

	if (TimeLimit < appSeconds() - StartTime)
	{
		warnf(NAME_Error, TEXT("FGeomObject::FinalizeSourceData() failed because it took too long"));
	}

	brush->ClearComponents();
	brush->ConditionalUpdateComponents();

	return Ret;
}

/**
 * Recomputes data specific to the geometry data (i.e. normals, mid points, etc)
 */

void FGeomObject::ComputeData()
{
	INT Count;
	FVector Wk;

	// Polygons

	FGeomPoly* poly;
	for( INT p = 0 ; p < PolyPool.Num() ; ++p )
	{
		poly = &PolyPool(p);

		poly->SetNormal( poly->GetActualPoly()->Normal );
		poly->SetMid( poly->GetMidPoint() );

	}

	// Vertices (= average normal of all the polygons that touch it)

	FGeomVertex* gv;
	for( INT v = 0 ; v < VertexPool.Num() ; ++v )
	{
		gv = &VertexPool(v);
		Count = 0;
		Wk = FVector(0,0,0);

		for( INT e = 0 ; e < EdgePool.Num() ; ++e )
		{
			FGeomEdge* ge = &EdgePool(e);

			FGeomVertex *v0 = &VertexPool( ge->VertexIndices[0] ),
				*v1 = &VertexPool( ge->VertexIndices[1] );

			if( gv == v0 || gv == v1 )
			{
				for( INT p = 0 ; p < ge->ParentPolyIndices.Num() ; ++ p )
				{
					FGeomPoly* poly = &PolyPool( ge->ParentPolyIndices(p) );

					Wk += poly->GetNormal();
					Count++;
				}
			}
		}

		gv->SetNormal( Wk / Count );
		gv->SetMid( gv->GetMidPoint() );
	}

	// Edges (= average normal of all the polygons that touch it)

	FGeomEdge* ge;
	for( INT e = 0 ; e < EdgePool.Num() ; ++e )
	{
		ge = &EdgePool(e);
		Count = 0;
		Wk = FVector(0,0,0);

		for( INT e2 = 0 ; e2 < EdgePool.Num() ; ++e2 )
		{
			FGeomEdge* ge2 = &EdgePool(e2);

			if( ge->IsSameEdge( *ge2 ) )
			{
				for( INT p = 0 ; p < ge2->ParentPolyIndices.Num(); ++p )
				{
					FGeomPoly* gp = &PolyPool( ge2->ParentPolyIndices(p) );

					Wk += gp->GetActualPoly()->Normal;
					Count++;

				}
			}
		}

		ge->SetNormal( Wk / Count );
		ge->SetMid( ge->GetMidPoint() );
	}
}

void FGeomObject::ClearData()
{
	EdgePool.Empty();
	VertexPool.Empty();
}

FVector FGeomObject::GetMidPoint() const
{
	return GetActualBrush()->Location;
}

INT FGeomObject::GetNewSelectionIndex()
{
	LastSelectionIndex++;
	return LastSelectionIndex;
}

void FGeomObject::SelectNone()
{
	Select( 0 );

	for( int i = 0 ; i < EdgePool.Num() ; ++i )
	{
		EdgePool(i).Select( 0 );
	}
	for( int i = 0 ; i < PolyPool.Num() ; ++i )
	{
		PolyPool(i).Select( 0 );
	}
	for( int i = 0 ; i < VertexPool.Num() ; ++i )
	{
		VertexPool(i).Select( 0 );
	}

	LastSelectionIndex = INDEX_NONE;
}

/**
 * Compiles the selection order array by putting every geometry object
 * with a valid selection index into the array, and then sorting it.
 */

static int CDECL SelectionIndexCompare(const void *InA, const void *InB)
{
	const INT A = (*(FGeomBase**)InA)->GetSelectionIndex();
	const INT B = (*(FGeomBase**)InB)->GetSelectionIndex();
	return A - B;
}

void FGeomObject::CompileSelectionOrder()
{
	// Only compile the array if it's dirty.

	if( !bSelectionOrderDirty )
	{
		return;
	}

	SelectionOrder.Empty();

	for( int i = 0 ; i < EdgePool.Num() ; ++i )
	{
		FGeomEdge* ge = &EdgePool(i);
		if( ge->GetSelectionIndex() > INDEX_NONE )
		{
			SelectionOrder.AddItem( ge );
		}
	}
	for( int i = 0 ; i < PolyPool.Num() ; ++i )
	{
		FGeomPoly* gp = &PolyPool(i);
		if( gp->GetSelectionIndex() > INDEX_NONE )
		{
			SelectionOrder.AddItem( gp );
		}
	}
	for( int i = 0 ; i < VertexPool.Num() ; ++i )
	{
		FGeomVertex* gv = &VertexPool(i);
		if( gv->GetSelectionIndex() > INDEX_NONE )
		{
			SelectionOrder.AddItem( gv );
		}
	}

	if( SelectionOrder.Num() )
	{
		appQsort( &SelectionOrder(0), SelectionOrder.Num(), sizeof(FGeomBase*), (QSORT_COMPARE)SelectionIndexCompare );
	}

	bSelectionOrderDirty = 0;
}

/**
 * Compiles a list of unique edges.  This runs through the edge pool
 * and only adds edges into the pool that aren't already there (the
 * difference being that this routine counts edges that share the same
 * vertices, but are wound backwards to each other, as being equal).
 *
 * @param	InEdges	The edge array to fill up
 */

void FGeomObject::CompileUniqueEdgeArray( TArray<FGeomEdge>* InEdges )
{
	InEdges->Empty();

	// Fill the array with any selected edges

	for( INT e = 0 ; e < EdgePool.Num() ; ++e )
	{
		FGeomEdge* ge = &EdgePool(e);

		if( ge->IsSelected() )
		{
			InEdges->AddItem( *ge );
		}
	}

	// Gather up any other edges that share the same position

	for( INT e = 0 ; e < EdgePool.Num() ; ++e )
	{
		FGeomEdge* ge = &EdgePool(e);

		// See if this edge is in the array already.  If so, add its parent
		// polygon indices into the transient edge arrays list.

		for( int x = 0 ; x < InEdges->Num() ; ++x )
		{
			FGeomEdge* geWk = &((*InEdges)(x));

			if( geWk->IsSameEdge( *ge ) )
			{
				// The parent polygon indices of both edges must be combined so that the resulting
				// item in the edge array will point to the complete list of polygons that share that edge.

				for( INT p = 0 ; p < ge->ParentPolyIndices.Num() ; ++p )
				{
					geWk->ParentPolyIndices.AddUniqueItem( ge->ParentPolyIndices(p) );
				}

				break;
			}
		}
	}
}

void FGeomObject::Serialize(FArchive& Ar)
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << ActualBrush;
	}
}
