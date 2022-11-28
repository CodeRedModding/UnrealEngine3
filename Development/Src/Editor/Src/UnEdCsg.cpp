/*=============================================================================
	UnEdCsg.cpp: High-level CSG tracking functions for editor
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"
#include <math.h>

//
// Globals:
//!!wastes 128K, fixed size
//
BYTE GFlags1 [MAXWORD+1]; // For fast polygon selection
BYTE GFlags2 [MAXWORD+1];

extern FRebuildTools GRebuildTools;

/*-----------------------------------------------------------------------------
	Level brush tracking.
-----------------------------------------------------------------------------*/

//
// Prepare a moving brush.
//
void UEditorEngine::csgPrepMovingBrush( ABrush* Actor )
{
	check(Actor);
	check(Actor->BrushComponent);
	check(Actor->Brush);
	check(Actor->Brush->RootOutside);
	debugf( NAME_Log, TEXT("Preparing brush %s"), Actor->GetName() );

	// Allocate tables.
	Actor->ClearFlags( RF_NotForClient | RF_NotForServer );
	Actor->Brush->ClearFlags( RF_NotForClient | RF_NotForServer );
	Actor->Brush->EmptyModel( 1, 0 );

	// Build bounding box.
	Actor->Brush->BuildBound();

	// Build BSP for the brush.
	bspBuild( Actor->Brush, BSP_Good, 15, 70, 1, 0 );
	bspRefresh( Actor->Brush, 1 );
	bspBuildBounds( Actor->Brush );

}

//
// Duplicate the specified brush and make it into a CSG-able level brush.
// Returns new brush, or NULL if the original was empty.
//
void UEditorEngine::csgCopyBrush
(
	ABrush*		Dest,
	ABrush*		Src,
	DWORD		PolyFlags, 
	DWORD		ResFlags,
	UBOOL		NeedsPrep
)
{
	check(Src);
	check(Src->BrushComponent);
	check(Src->Brush);

	// Handle empty brush.
	if( !Src->Brush->Polys->Element.Num() )
	{
		Dest->Brush = NULL;
		Dest->BrushComponent->Brush = NULL;
		return;
	}

	// Duplicate the brush and its polys.
	Dest->PolyFlags		= PolyFlags;
	Dest->Brush			= new( Src->Brush->GetOuter(), NAME_None, ResFlags )UModel( NULL, Src->Brush->RootOutside );
	Dest->Brush->Polys	= new( Src->Brush->GetOuter(), NAME_None, ResFlags )UPolys;
	check(Dest->Brush->Polys->Element.GetOwner()==Dest->Brush->Polys);
	Dest->Brush->Polys->Element = Src->Brush->Polys->Element;
	check(Dest->Brush->Polys->Element.GetOwner()==Dest->Brush->Polys);
	Dest->BrushComponent->Brush = Dest->Brush;

	// Update poly textures.
	for( INT i=0; i<Dest->Brush->Polys->Element.Num(); i++ )
		Dest->Brush->Polys->Element(i).iBrushPoly = INDEX_NONE;

	// Copy positioning, and build bounding box.
	Dest->CopyPosRotScaleFrom( Src );

	// If it's a moving brush, prep it.
	if( NeedsPrep )
		csgPrepMovingBrush( Dest );

}

//
// Add a brush to the list of CSG brushes in the level, using a CSG operation, and return 
// a newly-created copy of it.
//
ABrush* UEditorEngine::csgAddOperation
(
	ABrush*		Actor,
	ULevel*		Level,
	DWORD		PolyFlags,
	ECsgOper	CsgOper
)
{
	check(Actor);
	check(Actor->BrushComponent);
	check(Actor->Brush);
	check(Actor->Brush->Polys);

	// Can't do this if brush has no polys.
	if( !Actor->Brush->Polys->Element.Num() )
		return NULL;

	// Spawn a new actor for the brush.
	ABrush* Result  = Level->SpawnBrush();
	Result->SetFlags( RF_NotForClient | RF_NotForServer );

	// set bDeleteMe to true so that when the initial undo record is made,
	// the actor will be treated as destroyed, in that undo an add will
	// actually work
	Result->bDeleteMe = 1;
	Result->Modify();
	Result->bDeleteMe = 0;

	// Duplicate the brush.
	csgCopyBrush
	(
		Result,
		Actor,
		PolyFlags,
		RF_NotForClient | RF_NotForServer | RF_Transactional,
		0
	);
	check(Result->Brush);

	// Set add-info.
	Result->CsgOper = CsgOper;

	Result->ClearComponents();
	Result->UpdateComponents();

	return Result;
}

const TCHAR* UEditorEngine::csgGetName( ECsgOper CSG )
{
	return *(FindObjectChecked<UEnum>( ANY_PACKAGE, TEXT("ECsgOper") ) )->Names(CSG);
}

/*-----------------------------------------------------------------------------
	CSG Rebuilding.
-----------------------------------------------------------------------------*/

//
// Rebuild the level's Bsp from the level's CSG brushes
//
// Note: Needs to be expanded to defragment Bsp polygons as needed (by rebuilding
// the Bsp), so that it doesn't slow down to a crawl on complex levels.
//
#if 0
void UEditorEngine::csgRebuild( ULevel* Level )
{
	INT NodeCount,PolyCount,LastPolyCount;
	TCHAR TempStr[80];

	BeginSlowTask( "Rebuilding geometry", 1 );
	FastRebuild = 1;

	FinishAllSnaps(Level);

	// Empty the model out.
	Level->Lock( LOCK_Trans );
	Level->Model->EmptyModel( 1, 1 );
	Level->Unlock( LOCK_Trans );

	// Count brushes.
	INT BrushTotal=0, BrushCount=0;
	for( FStaticBrushIterator TempIt(Level); TempIt; ++TempIt )
		if( *TempIt != Level->Brush() )
			BrushTotal++;

	LastPolyCount = 0;
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		if( *It == Level->Brush()
			continue;

		BrushCount++;
		appSprintf(TempStr,"Applying brush %i of %i",BrushCount,BrushTotal);
		StatusUpdate( TempStr, BrushCount, BrushTotal );

		// See if the Bsp has become badly fragmented and, if so, rebuild.
		PolyCount = Level->Model->Surfs->Num();
		NodeCount = Level->Model->Nodes->Num();
		if( PolyCount>2000 && PolyCount>=3*LastPolyCount )
		{
			appStrcat( TempStr, ": Refreshing Bsp..." );
			StatusUpdate( TempStr, BrushCount, BrushTotal );

			debugf 				( NAME_Log, "Map: Rebuilding Bsp" );
			bspBuildFPolys		( Level->Model, 1, 0 );
			bspMergeCoplanars	( Level->Model, 0, 0 );
			bspBuild			( Level->Model, BSP_Lame, 25, 0, 0 );
			debugf				( NAME_Log, "Map: Reduced nodes by %i%%, polys by %i%%", (100*(NodeCount-Level->Model->Nodes->Num()))/NodeCount,(100*(PolyCount-Level->Model->Surfs->Num()))/PolyCount );

			LastPolyCount = Level->Model->Surfs->Num();
		}

		// Perform this CSG operation.
		bspBrushCSG( *It, Level->Model, It->PolyFlags, (ECsgOper)It->CsgOper, 0 );
	}

	// Build bounding volumes.
	Level->Lock( LOCK_Trans );
	bspBuildBounds( Level->Model );
	Level->Unlock( LOCK_Trans );

	// Done.
	FastRebuild = 0;
	EndSlowTask();
}
#endif

//
// Repartition the bsp.
//
void UEditorEngine::bspRepartition( ULevel* Level, INT iNode, INT Simple )
{
	bspBuildFPolys( Level->Model, 1, iNode );
	bspMergeCoplanars( Level->Model, 0, 0 );
	bspBuild( Level->Model, BSP_Good, 12, 70, Simple, iNode );
	bspRefresh( Level->Model, 1 );

}

//
// Build list of leaves.
//
static void EnlistLeaves( UModel* Model, TArray<INT>& iFronts, TArray<INT>& iBacks, INT iNode )
{
	FBspNode& Node=Model->Nodes(iNode);

	if( Node.iFront==INDEX_NONE ) iFronts.AddItem(iNode);
	else EnlistLeaves( Model, iFronts, iBacks, Node.iFront );

	if( Node.iBack==INDEX_NONE ) iBacks.AddItem(iNode);
	else EnlistLeaves( Model, iFronts, iBacks, Node.iBack );

}

//
// Rebuild the level's Bsp from the level's CSG brushes.
//
void UEditorEngine::csgRebuild( ULevel* Level )
{
	GWarn->BeginSlowTask( TEXT("Rebuilding geometry"), 1 );
	FastRebuild = 1;

	UBOOL bVisibleOnly = GRebuildTools.GetCurrent()->Options & REBUILD_OnlyVisible;

	FinishAllSnaps(Level);

	// Empty the model out.
	Level->Model->EmptyModel( 1, 1 );

	// Count brushes.
	INT BrushTotal=0, BrushCount=0;
	for( FStaticBrushIterator TempIt(Level); TempIt; ++TempIt )
		if( !bVisibleOnly || ( bVisibleOnly && !TempIt->IsHiddenEd() ) )
			if( *TempIt != Level->Brush() )
				BrushTotal++;

	// Compose all structural brushes and portals.
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		if( !bVisibleOnly || ( bVisibleOnly && !It->IsHiddenEd() ) )
			if( *It!=Level->Brush() )
			{
				if
				(  !(It->PolyFlags&PF_Semisolid)
				||	(It->CsgOper!=CSG_Add)
				||	(It->PolyFlags&PF_Portal) )
				{
					// Treat portals as solids for cutting.
					if( It->PolyFlags & PF_Portal )
						It->PolyFlags = (It->PolyFlags & ~PF_Semisolid) | PF_NotSolid;
					BrushCount++;
					GWarn->StatusUpdatef( BrushCount, BrushTotal, TEXT("Applying structural brush %i of %i"), BrushCount, BrushTotal );
					bspBrushCSG( *It, Level->Model, It->PolyFlags, (ECsgOper)It->CsgOper, 0 );
				}
			}
	}

	// Repartition the structural BSP.
	bspRepartition( Level, 0, 0 );
	TestVisibility( Level, Level->Model, 0, 0 );

	// Remember leaves.
	TArray<INT> iFronts, iBacks;
	if( Level->Model->Nodes.Num() )
		EnlistLeaves( Level->Model, iFronts, iBacks, 0 );

	// Compose all detail brushes.
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		if( !bVisibleOnly || ( bVisibleOnly && !It->IsHiddenEd() ) )
			if
			(	*It!=Level->Brush()
			&&	(It->PolyFlags&PF_Semisolid)
			&& !(It->PolyFlags&PF_Portal)
			&&	It->CsgOper==CSG_Add )
			{
				BrushCount++;
				GWarn->StatusUpdatef( BrushCount, BrushTotal, TEXT("Applying detail brush %i of %i"), BrushCount, BrushTotal );
				bspBrushCSG( *It, Level->Model, It->PolyFlags, (ECsgOper)It->CsgOper, 0 );
			}
	}

	// Optimize the sub-bsp's.
	INT iNode;
	for( TArray<INT>::TIterator ItF(iFronts); ItF; ++ItF )
		if( (iNode=Level->Model->Nodes(*ItF).iFront)!=INDEX_NONE )
			bspRepartition( Level, iNode, 2 );
	for( TArray<INT>::TIterator ItB(iBacks); ItB; ++ItB )
		if( (iNode=Level->Model->Nodes(*ItB).iBack)!=INDEX_NONE )
			bspRepartition( Level, iNode, 2 );

	// Build bounding volumes.
	bspOptGeom( Level->Model );
	bspBuildBounds( Level->Model );

	// Rebuild dynamic brush BSP's.
	for( int i=0; i<Level->Actors.Num(); i++ )
	{
		ABrush* B=Cast<ABrush>(Level->Actors(i));
		if(B && B->Brush && !B->IsStaticBrush())
			csgPrepMovingBrush(B);
	}

	// Done.
	FastRebuild = 0;
	GWarn->EndSlowTask();
}

/*---------------------------------------------------------------------------------------
	Flag setting and searching
---------------------------------------------------------------------------------------*/

//
// Sets and clears all Bsp node flags.  Affects all nodes, even ones that don't
// really exist.
//
void UEditorEngine::polySetAndClearPolyFlags(UModel *Model, DWORD SetBits, DWORD ClearBits,INT SelectedOnly, INT UpdateMaster)
{
	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf& Poly = Model->Surfs(i);
		if( !SelectedOnly || (Poly.PolyFlags & PF_Selected) )
		{
			DWORD NewFlags = (Poly.PolyFlags & ~ClearBits) | SetBits;
			if( NewFlags != Poly.PolyFlags )
			{
				Model->ModifySurf( i, UpdateMaster );
				Poly.PolyFlags = NewFlags;
				if( UpdateMaster )
					polyUpdateMaster( Model, i, 0 );
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Polygon searching
-----------------------------------------------------------------------------*/

//
// Find the Brush EdPoly corresponding to a given Bsp surface.
//
INT UEditorEngine::polyFindMaster(UModel *Model, INT iSurf, FPoly &Poly)
{
	FBspSurf &Surf = Model->Surfs(iSurf);
	if( !Surf.Actor )
	{
		return 0;
	}
	else
	{
		Poly = Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly);
		return 1;
	}
}

//
// Update a the master brush EdPoly corresponding to a newly-changed
// poly to reflect its new properties.
//
// Doesn't do any transaction tracking.
//
void UEditorEngine::polyUpdateMaster
(
	UModel*	Model,
	INT  	iSurf,
	INT		UpdateTexCoords
)
{
	FBspSurf &Surf = Model->Surfs(iSurf);
	if( !Surf.Actor )
		return;

	for( INT iEdPoly = Surf.iBrushPoly; iEdPoly < Surf.Actor->Brush->Polys->Element.Num(); iEdPoly++ )
	{
		FPoly& MasterEdPoly = Surf.Actor->Brush->Polys->Element(iEdPoly);
		if( iEdPoly==Surf.iBrushPoly || MasterEdPoly.iLink==Surf.iBrushPoly )
		{
			Surf.Actor->Brush->Polys->Element.ModifyItem( iEdPoly );

			MasterEdPoly.Material  = Surf.Material;
			MasterEdPoly.PolyFlags = Surf.PolyFlags & ~(PF_NoEdit);

			if( UpdateTexCoords )
			{
				MasterEdPoly.Base = (Model->Points(Surf.pBase) - Surf.Actor->Location) + Surf.Actor->PrePivot;
				MasterEdPoly.TextureU = Model->Vectors(Surf.vTextureU);
				MasterEdPoly.TextureV = Model->Vectors(Surf.vTextureV);
			}
		}
	}

	GEditor->Level->InvalidateModelSurface(iSurf);
}

// Populates a list with all polys that are linked to the specified poly.  The
// resulting list includes the original poly.
void UEditorEngine::polyGetLinkedPolys
(
	ABrush* InBrush,
	FPoly* InPoly,
	TArray<FPoly>* InPolyList
)
{
	InPolyList->Empty();

	if( InPoly->iLink == INDEX_NONE )
	{
		// If this poly has no links, just stick the one poly in the final list.
		new(*InPolyList)FPoly( *InPoly );
	}
	else
	{
		// Find all polys that match the source polys link value.
		for( INT poly = 0 ; poly < InBrush->Brush->Polys->Element.Num() ; poly++ )
			if( InBrush->Brush->Polys->Element(poly).iLink == InPoly->iLink )
				new(*InPolyList)FPoly( InBrush->Brush->Polys->Element(poly) );
	}

}

// Takes a list of polygons and creates a new list of polys which have no overlapping edges.  It splits
// edges as necessary to achieve this.
void UEditorEngine::polySplitOverlappingEdges( TArray<FPoly>* InPolyList, TArray<FPoly>* InResult )
{
	InResult->Empty();

	for( INT poly = 0 ; poly < InPolyList->Num() ; poly++ )
	{
		FPoly* SrcPoly = &(*InPolyList)(poly);
		FPoly NewPoly = *SrcPoly;

		for( INT edge = 0 ; edge < SrcPoly->NumVertices ; edge++ )
		{
			FEdge SrcEdge = FEdge( SrcPoly->Vertex[edge], SrcPoly->Vertex[ edge+1 < SrcPoly->NumVertices ? edge+1 : 0 ] );
			FPlane SrcEdgePlane( SrcEdge.Vertex[0], SrcEdge.Vertex[1], SrcEdge.Vertex[0] + (SrcPoly->Normal * 16) );

			for( INT poly2 = 0 ; poly2 < InPolyList->Num() ; poly2++ )
			{
				FPoly* CmpPoly = &(*InPolyList)(poly2);

				// We can't compare to ourselves.
				if( CmpPoly == SrcPoly )
					continue;

				for( INT edge2 = 0 ; edge2 < CmpPoly->NumVertices ; edge2++ )
				{
					FEdge CmpEdge = FEdge( CmpPoly->Vertex[edge2], CmpPoly->Vertex[ edge2+1 < CmpPoly->NumVertices ? edge2+1 : 0 ] );

					// If both vertices on this edge lie on the same plane as the original edge, create
					// a sphere around the original 2 vertices.  If either of this edges vertices are inside of
					// that sphere, we need to split the original edge by adding a vertex to it's poly.
					if( ::fabs( FPointPlaneDist( CmpEdge.Vertex[0], SrcEdge.Vertex[0], SrcEdgePlane ) ) < THRESH_POINT_ON_PLANE
							&& ::fabs( FPointPlaneDist( CmpEdge.Vertex[1], SrcEdge.Vertex[0], SrcEdgePlane ) ) < THRESH_POINT_ON_PLANE )
					{
						//
						// Check THIS edge against the SOURCE edge
						//

						FVector Dir = SrcEdge.Vertex[1] - SrcEdge.Vertex[0];
						Dir.Normalize();
						FLOAT Dist = FDist( SrcEdge.Vertex[1], SrcEdge.Vertex[0] );
						FVector Origin = SrcEdge.Vertex[0] + (Dir * (Dist / 2.0f));
						FLOAT Radius = Dist / 2.0f;

						for( INT vtx = 0 ; vtx < 2 ; vtx++ )
							if( FDist( Origin, CmpEdge.Vertex[vtx] ) && FDist( Origin, CmpEdge.Vertex[vtx] ) < Radius )
								NewPoly.InsertVertex( edge2+1, CmpEdge.Vertex[vtx] );
					}
				}
			}
		}

		new(*InResult)FPoly( NewPoly );
	}

}

// Takes a list of polygons and returns a list of the outside edges (edges which are not shared
// by other polys in the list).
void UEditorEngine::polyGetOuterEdgeList
(
	TArray<FPoly>* InPolyList,
	TArray<FEdge>* InEdgeList
)
{
	TArray<FPoly> NewPolyList;
	polySplitOverlappingEdges( InPolyList, &NewPolyList );

	TArray<FEdge> TempEdges;

	// Create a master list of edges.
	for( INT poly = 0 ; poly < NewPolyList.Num() ; poly++ )
	{
		FPoly* Poly = &NewPolyList(poly);
		for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
			new( TempEdges )FEdge( Poly->Vertex[vtx], Poly->Vertex[ vtx+1 < Poly->NumVertices ? vtx+1 : 0] );
	}

	// Add all the unique edges into the final edge list.
	TArray<FEdge> FinalEdges;

	for( INT tedge = 0 ; tedge < TempEdges.Num() ; tedge++ )
	{
		FEdge* TestEdge = &TempEdges(tedge);

		INT EdgeCount = 0;
		for( INT edge = 0 ; edge < TempEdges.Num() ; edge++ )
		{
			if( TempEdges(edge) == *TestEdge )
				EdgeCount++;
		}

		if( EdgeCount == 1 )
			new( FinalEdges )FEdge( *TestEdge );
	}

	// Reorder all the edges so that they line up, end to end.
	InEdgeList->Empty();
	if( !FinalEdges.Num() ) return;

	new( *InEdgeList )FEdge( FinalEdges(0) );
	FVector Comp = FinalEdges(0).Vertex[1];
	FinalEdges.Remove(0);

	FEdge DebuG;
	for( INT x = 0 ; x < FinalEdges.Num() ; x++ )
	{
		DebuG = FinalEdges(x);

		// If the edge is backwards, flip it
		if( FinalEdges(x).Vertex[1] == Comp )
			Exchange( FinalEdges(x).Vertex[0], FinalEdges(x).Vertex[1] );

		if( FinalEdges(x).Vertex[0] == Comp )
		{
			new( *InEdgeList )FEdge( FinalEdges(x) );
			Comp = FinalEdges(x).Vertex[1];
			FinalEdges.Remove(x);
			x = -1;
		}
	}

}

/*-----------------------------------------------------------------------------
   All transactional polygon selection functions
-----------------------------------------------------------------------------*/

void GetListOfUniqueBrushes( TArray<ABrush*>* InBrushes )
{
	InBrushes->Empty();

	// Generate a list of unique brushes.
	for( INT i = 0 ; i < GEditor->Level->Model->Surfs.Num() ; i++ )
	{
		FBspSurf* Surf = &GEditor->Level->Model->Surfs(i);
		if( Surf->PolyFlags & PF_Selected )
		{
			ABrush* ParentBrush = Cast<ABrush>(Surf->Actor);

			// See if we've already got this brush ...
			INT brush;
			for( brush = 0 ; brush < InBrushes->Num() ; brush++ )
				if( ParentBrush == (*InBrushes)(brush) )
					break;

			// ... if not, add it to the list.
			if( brush == InBrushes->Num() )
				(*InBrushes)( InBrushes->Add() ) = ParentBrush;
		}
	}
}

void UEditorEngine::polySelectAll(UModel *Model)
{
	polySetAndClearPolyFlags(Model,PF_Selected,0,0,0);
};

void UEditorEngine::polySelectMatchingGroups( UModel* Model )
{
	return;	// Temp fix until this can be rewritten (crashes a lot)

	appMemzero( GFlags1, sizeof(GFlags1) );
	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		if( Surf->PolyFlags&PF_Selected )
		{
			FPoly Poly; polyFindMaster(Model,i,Poly);
			GFlags1[Poly.Actor->Group.GetIndex()]=1;
		}
	}
	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		FPoly Poly;
		polyFindMaster(Model,i,Poly);
		if
		(	(GFlags1[Poly.Actor->Group.GetIndex()]) 
			&&	(!(Surf->PolyFlags & PF_Selected)) )
		{
			Model->ModifySurf( i, 0 );
			GEditor->SelectBSPSurf( Level, i, 1, 0 );
		}
	}
	NoteSelectionChange( Level );
}

void UEditorEngine::polySelectMatchingItems(UModel *Model)
{
	appMemzero(GFlags1,sizeof(GFlags1));
	appMemzero(GFlags2,sizeof(GFlags2));

	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		if( Surf->Actor )
		{
			if( Surf->PolyFlags & PF_Selected )
				GFlags2[Surf->Actor->Brush->GetIndex()]=1;
		}
		if( Surf->PolyFlags&PF_Selected )
		{
			FPoly Poly; polyFindMaster(Model,i,Poly);
			GFlags1[Poly.ItemName.GetIndex()]=1;
		}
	}
	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		if( Surf->Actor )
		{
			FPoly Poly; polyFindMaster(Model,i,Poly);
			if ((GFlags1[Poly.ItemName.GetIndex()]) &&
				( GFlags2[Surf->Actor->Brush->GetIndex()]) &&
				(!(Surf->PolyFlags & PF_Selected)))
			{
				Model->ModifySurf( i, 0 );
				GEditor->SelectBSPSurf( Level, i, 1, 0 );
			}
		}
	}
	NoteSelectionChange( Level );
}

enum EAdjacentsType
{
	ADJACENT_ALL,		// All adjacent polys
	ADJACENT_COPLANARS,	// Adjacent coplanars only
	ADJACENT_WALLS,		// Adjacent walls
	ADJACENT_FLOORS,	// Adjacent floors or ceilings
	ADJACENT_SLANTS,	// Adjacent slants
};

//
// Select all adjacent polygons (only coplanars if Coplanars==1) and
// return number of polygons newly selected.
//
INT TagAdjacentsType(UModel *Model, EAdjacentsType AdjacentType)
{
	FVert	*VertPool;
	FVector		*Base,*Normal;
	BYTE		b;
	INT		    i;
	int			Selected,Found;

	Selected = 0;
	appMemzero( GFlags1, sizeof(GFlags1) );

	// Find all points corresponding to selected vertices:
	for (i=0; i<Model->Nodes.Num(); i++)
	{
		FBspNode &Node = Model->Nodes(i);
		FBspSurf &Poly = Model->Surfs(Node.iSurf);
		if (Poly.PolyFlags & PF_Selected)
		{
			VertPool = &Model->Verts(Node.iVertPool);

			for (b=0; b<Node.NumVertices; b++)
				GFlags1[(VertPool++)->pVertex] = 1;
		}
	}

	// Select all unselected nodes for which two or more vertices are selected:
	for( i = 0 ; i < Model->Nodes.Num() ; i++)
	{
		FBspNode &Node = Model->Nodes(i);
		FBspSurf &Poly = Model->Surfs(Node.iSurf);
		if (!(Poly.PolyFlags & PF_Selected))
		{
			Found    = 0;
			VertPool = &Model->Verts(Node.iVertPool);
			//
			Base   = &Model->Points (Poly.pBase);
			Normal = &Model->Vectors(Poly.vNormal);
			//
			for (b=0; b<Node.NumVertices; b++) Found += GFlags1[(VertPool++)->pVertex];
			//
			if (AdjacentType == ADJACENT_COPLANARS)
			{
				if (!GFlags2[Node.iSurf]) Found=0;
			}
			else if (AdjacentType == ADJACENT_FLOORS)
			{
				if (Abs(Normal->Z) <= 0.85) Found = 0;
			}
			else if (AdjacentType == ADJACENT_WALLS)
			{
				if (Abs(Normal->Z) >= 0.10) Found = 0;
			}
			else if (AdjacentType == ADJACENT_SLANTS)
			{
				if (Abs(Normal->Z) > 0.85) Found = 0;
				if (Abs(Normal->Z) < 0.10) Found = 0;
			}

			if (Found > 0)
			{
				Model->ModifySurf( Node.iSurf, 0 );
				GEditor->SelectBSPSurf( GEditor->Level, Node.iSurf, 1, 0 );
				Selected++;
			}
		}
	}
	GEditor->NoteSelectionChange( GEditor->Level );
	return Selected;
}

void TagCoplanars(UModel *Model)
{
	appMemzero(GFlags2,sizeof(GFlags2));

	for(INT SelectedNodeIndex = 0;SelectedNodeIndex < Model->Nodes.Num();SelectedNodeIndex++)
	{
		FBspNode&	SelectedNode = Model->Nodes(SelectedNodeIndex);
		FBspSurf&	SelectedSurf = Model->Surfs(SelectedNode.iSurf);

		if(SelectedSurf.PolyFlags & PF_Selected)
		{
			FVector	SelectedBase = Model->Points(Model->Verts(SelectedNode.iVertPool).pVertex),
					SelectedNormal = Model->Vectors(SelectedSurf.vNormal);

			for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
			{
				FBspNode&	Node = Model->Nodes(NodeIndex);
				FBspSurf&	Surf = Model->Surfs(Node.iSurf);
				FVector		Base = Model->Points(Model->Verts(Node.iVertPool).pVertex),
							Normal = Model->Vectors(Surf.vNormal);

				if(FCoplanar(SelectedBase,SelectedNormal,Base,Normal) && !(Surf.PolyFlags & PF_Selected))
				{
					GFlags2[Node.iSurf] = 1;
				}
			}
		}
	}

}

void UEditorEngine::polySelectAdjacents(UModel *Model)
	{
	do {} while (TagAdjacentsType (Model,ADJACENT_ALL) > 0);
	};

void UEditorEngine::polySelectCoplanars(UModel *Model)
	{
	TagCoplanars(Model);
	do {} while (TagAdjacentsType(Model,ADJACENT_COPLANARS) > 0);
	};

void UEditorEngine::polySelectMatchingBrush(UModel *Model)
{
	TArray<ABrush*> Brushes;
	GetListOfUniqueBrushes( &Brushes );

	// Select all the faces.
	for( INT i = 0 ; i < Model->Surfs.Num() ; i++ )
	{
		FBspSurf* Surf = &Model->Surfs(i);

		// Select all the polys on each brush in the unique list.
		for( INT brush = 0 ; brush < Brushes.Num() ; brush++ )
			if( Cast<ABrush>(Surf->Actor) == Brushes(brush) )
				for( INT poly = 0 ; poly < Brushes(brush)->Brush->Polys->Element.Num() ; poly++ )
					if( Surf->iBrushPoly == poly )
					{
						Model->ModifySurf( i, 0 );
						GEditor->SelectBSPSurf( Level, i, 1, 0 );
					}
	}
	NoteSelectionChange( Level );

};

void UEditorEngine::polySelectMatchingTexture(UModel *Model)
{
	// Get list of unique materials that are on selected faces

	TArray<UMaterialInstance*> Materials;

	for( INT i = 0 ; i < Model->Surfs.Num() ; i++ )
	{
		FBspSurf* Surf = &Model->Surfs(i);

		if( Surf->PolyFlags&PF_Selected )
			Materials.AddUniqueItem( Surf->Material );
	}

	// Select any BSP faces with a material that is in the unique materials list

	for( INT i = 0 ; i < Model->Surfs.Num() ; i++ )
	{
		FBspSurf* Surf = &Model->Surfs(i);

		if( Materials.FindItemIndex( Surf->Material ) != INDEX_NONE )
		{
			Model->ModifySurf( i, 0 );
			GEditor->SelectBSPSurf( Level, i, 1, 0 );
		}
	}

	NoteSelectionChange( Level );

}

void UEditorEngine::polySelectAdjacentWalls(UModel *Model)
{
	do 
	{
	} 
	while (TagAdjacentsType  (Model,ADJACENT_WALLS) > 0);
};

void UEditorEngine::polySelectAdjacentFloors(UModel *Model)
{
	do
	{
	}
	while (TagAdjacentsType (Model,ADJACENT_FLOORS) > 0);
};

void UEditorEngine::polySelectAdjacentSlants(UModel *Model)
{
	do 
	{
	} 
	while (TagAdjacentsType  (Model,ADJACENT_SLANTS) > 0);
};

void UEditorEngine::polySelectReverse(UModel *Model)
{
	for (INT i=0; i<Model->Surfs.Num(); i++)
	{
		FBspSurf *Poly = &Model->Surfs(i);
		Model->ModifySurf( i, 0 );
		Poly->PolyFlags ^= PF_Selected;
		//
		Poly++;
	};
};

void UEditorEngine::polyMemorizeSet(UModel *Model)
{
	for (INT i=0; i<Model->Surfs.Num(); i++)
	{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->PolyFlags & PF_Selected) 
		{
			if (!(Poly->PolyFlags & PF_Memorized))
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags |= (PF_Memorized);
			};
		}
		else
		{
			if (Poly->PolyFlags & PF_Memorized)
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Memorized);
			};
		};
		Poly++;
	};
};

void UEditorEngine::polyRememberSet(UModel *Model)
{
	for (INT i=0; i<Model->Surfs.Num(); i++)
	{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->PolyFlags & PF_Memorized) 
		{
			if (!(Poly->PolyFlags & PF_Selected))
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags |= (PF_Selected);
			};
		}
		else
		{
			if (Poly->PolyFlags & PF_Selected)
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Selected);
			};
		};
		Poly++;
	};
};

void UEditorEngine::polyXorSet(UModel *Model)
{
	int			Flag1,Flag2;
	//
	for (INT i=0; i<Model->Surfs.Num(); i++)
	{
		FBspSurf *Poly = &Model->Surfs(i);
		Flag1 = (Poly->PolyFlags & PF_Selected ) != 0;
		Flag2 = (Poly->PolyFlags & PF_Memorized) != 0;
		//
		if (Flag1 ^ Flag2)
		{
			if (!(Poly->PolyFlags & PF_Selected))
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags |= PF_Selected;
			};
		}
		else
		{
			if (Poly->PolyFlags & PF_Selected)
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Selected);
			};
		};
		Poly++;
	};
};

void UEditorEngine::polyUnionSet(UModel *Model)
{
	for (INT i=0; i<Model->Surfs.Num(); i++)
	{
		FBspSurf *Poly = &Model->Surfs(i);
		if (!(Poly->PolyFlags & PF_Memorized))
		{
			if (Poly->PolyFlags & PF_Selected)
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Selected);
			};
		};
		Poly++;
	};
};

void UEditorEngine::polyIntersectSet(UModel *Model)
{
	for (INT i=0; i<Model->Surfs.Num(); i++)
	{
		FBspSurf *Poly = &Model->Surfs(i);
		if ((Poly->PolyFlags & PF_Memorized) && !(Poly->PolyFlags & PF_Selected))
		{
			Poly->PolyFlags |= PF_Selected;
		};
		Poly++;
	};
};

void UEditorEngine::polySelectZone( UModel* Model )
{
	// identify the list of currently selected zones
	TArray<INT> iZoneList;
	for( INT i = 0; i < Model->Nodes.Num(); i++ )
	{
		FBspNode* Node = &Model->Nodes(i);
		FBspSurf* Poly = &Model->Surfs( Node->iSurf );
		if( Poly->PolyFlags & PF_Selected )
		{
			if( Node->iZone[1] != 0 )
				iZoneList.AddUniqueItem( Node->iZone[1] ); //front zone
			if( Node->iZone[0] != 0 )
				iZoneList.AddUniqueItem( Node->iZone[0] ); //back zone
		}
	}

	// select all polys that are match one of the zones identified above
	for( INT i = 0; i < Model->Nodes.Num(); i++ )
	{
		FBspNode* Node = &Model->Nodes(i);
		for( INT j = 0; j < iZoneList.Num(); j++ ) 
		{
			if( Node->iZone[1] == iZoneList(j) || Node->iZone[0] == iZoneList(j) )
			{
				FBspSurf* Poly = &Model->Surfs( Node->iSurf );
				Poly->PolyFlags |= PF_Selected;
			}
		}
	}

}

/*---------------------------------------------------------------------------------------
   Brush selection functions
---------------------------------------------------------------------------------------*/

//
// Generic selection routines
//

typedef INT (*BRUSH_SEL_FUNC)( ABrush* Brush, INT Tag );

void MapSelect( ULevel* Level, BRUSH_SEL_FUNC Func, INT Tag )
{
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		ABrush* Actor = *It;
		if( Func( Actor, Tag ) )
		{
			GSelectionTools.Select( Actor );
		}
		else
		{
			GSelectionTools.Select( Actor, 0 );
		}
	}

	GEditor->NoteSelectionChange( Level );
}

//
// Select none
//
static INT BrushSelectNoneFunc( ABrush* Actor, INT Tag )
{
	return 0;
}

//
// Select by CSG operation
//
INT BrushSelectOperationFunc( ABrush* Actor, INT Tag )
{
	return ((ECsgOper)Actor->CsgOper == Tag) && !(Actor->PolyFlags & (PF_NotSolid | PF_Semisolid));
}
void UEditorEngine::mapSelectOperation(ULevel *Level,ECsgOper CsgOper)
{
	MapSelect( Level, BrushSelectOperationFunc, CsgOper );
}

INT BrushSelectFlagsFunc( ABrush* Actor, INT Tag )
{
	return Actor->PolyFlags & Tag;
}
void UEditorEngine::mapSelectFlags(ULevel *Level,DWORD Flags)
	{
	MapSelect( Level, BrushSelectFlagsFunc, (int)Flags );
	};

/*---------------------------------------------------------------------------------------
   Other map brush functions
---------------------------------------------------------------------------------------*/

//
// Put the first selected brush into the current Brush.
//
void UEditorEngine::mapBrushGet( ULevel* Level )
{
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		ABrush* Actor = Cast<ABrush>(Level->Actors(i));
		if( Actor && Actor!=Level->Brush() && GSelectionTools.IsSelected( Actor ) )
		{
			Level->Brush()->Modify();
			Level->Brush()->Brush->Polys->Element = Actor->Brush->Polys->Element;
			Level->Brush()->CopyPosRotScaleFrom( Actor );
			break;
		}
	}
}

//
// Replace all selected brushes with the current Brush.
//
void UEditorEngine::mapBrushPut( ULevel* Level )
{
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		ABrush* Actor = Cast<ABrush>(Level->Actors(i));
		if( Actor && Actor!=Level->Brush() && GSelectionTools.IsSelected( Actor ) )
		{
			Actor->Modify();
			Actor->Brush->Polys->Element = Level->Brush()->Brush->Polys->Element;
			Actor->CopyPosRotScaleFrom( Level->Brush() );
		}
	}
}

//
// Generic private routine for send to front / send to back
//
void SendTo( ULevel* Level, INT bSendToFirst )
{
	FMemMark Mark(GMem);

	// Partition.
	TArray<AActor*> Lists[2];
	for( INT i=2; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Lists[GSelectionTools.IsSelected( Level->Actors(i) ) ^ bSendToFirst ^ 1].AddItem( Level->Actors(i) );

	// Refill.
	check(Level->Actors.Num()>=2);
	Level->Actors.Remove(2,Level->Actors.Num()-2);
	for( INT i=0; i<2; i++ )
		for( INT j=0; j<Lists[i].Num(); j++ )
			Level->Actors.AddItem( Lists[i](j) );

	Mark.Pop();
}

//
// Send all selected brushes in a level to the front of the hierarchy
//
void UEditorEngine::mapSendToFirst( ULevel* Level )
{
	SendTo( Level, 0 );
}

//
// Send all selected brushes in a level to the back of the hierarchy
//
void UEditorEngine::mapSendToLast( ULevel* Level )
{
	SendTo( Level, 1 );
}

//
// Swaps the first 2 selected actors in the actor list
//
void UEditorEngine::mapSendToSwap( ULevel* Level )
{
	INT Count = 0;
	AActor** Actors[2];
	for( INT i=2; i<Level->Actors.Num() && Count < 2; i++ )
		if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
		{
			Actors[Count] = &(Level->Actors(i));
			Count++;
		}

	Exchange( *Actors[0], *Actors[1] );

}

void UEditorEngine::mapSetBrush
(
	ULevel*				Level,
	EMapSetBrushFlags	PropertiesMask,
	_WORD				BrushColor,
	FName				GroupName,
	DWORD				SetPolyFlags,
	DWORD				ClearPolyFlags,
	DWORD				CSGOper,
	INT					DrawType
)
{
	for( FStaticBrushIterator It(Level); It; ++It )
	{
		if( *It!=Level->Brush() && GSelectionTools.IsSelected( *It ) )
		{
			if( PropertiesMask & MSB_PolyFlags )
			{
				It->Modify();
				It->PolyFlags = (It->PolyFlags & ~ClearPolyFlags) | SetPolyFlags;
			}
			if( PropertiesMask & MSB_CSGOper )
			{
				It->Modify();
				It->CsgOper = CSGOper;
			}
		}
	}
}

/*---------------------------------------------------------------------------------------
   Poly texturing operations
---------------------------------------------------------------------------------------*/

//
// Pan textures on selected polys.  Doesn't do transaction tracking.
//
void UEditorEngine::polyTexPan(UModel *Model,INT PanU,INT PanV,INT Absolute)
{
	for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
	{
		FBspSurf&	Surf = Model->Surfs(SurfaceIndex);

		if(Surf.PolyFlags & PF_Selected)
		{
			if(Absolute)
				Model->Points(Surf.pBase) = FVector(0,0,0);

			FVector	TextureU = Model->Vectors(Surf.vTextureU),
					TextureV = Model->Vectors(Surf.vTextureV);

			Model->Points(Surf.pBase) += PanU * (TextureU / TextureU.SizeSquared());
			Model->Points(Surf.pBase) += PanV * (TextureV / TextureV.SizeSquared());

			polyUpdateMaster(Model,SurfaceIndex,1);
		}
	}

}

//
// Scale textures on selected polys. Doesn't do transaction tracking.
//
void UEditorEngine::polyTexScale( UModel* Model, FLOAT UU, FLOAT UV, FLOAT VU, FLOAT VV, INT Absolute )
{
	for( INT i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Poly = &Model->Surfs(i);
		if (Poly->PolyFlags & PF_Selected)
		{
			FVector OriginalU = Model->Vectors(Poly->vTextureU);
			FVector OriginalV = Model->Vectors(Poly->vTextureV);

			if( Absolute )
			{
				OriginalU *= 1.0/OriginalU.Size();
				OriginalV *= 1.0/OriginalV.Size();
			}

			// Calc new vectors.
			Model->Vectors(Poly->vTextureU) = OriginalU * UU + OriginalV * UV;
			Model->Vectors(Poly->vTextureV) = OriginalU * VU + OriginalV * VV;

			// Update generating brush poly.
			polyUpdateMaster( Model, i, 1 );
		}
		Poly++;
	}

}

/*---------------------------------------------------------------------------------------
   Map geometry link topic handler
---------------------------------------------------------------------------------------*/

AUTOREGISTER_TOPIC(TEXT("Map"),MapTopicHandler);
void MapTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	INT NumBrushes  = 0;
	INT NumAdd	    = 0;
	INT NumSubtract	= 0;
	INT NumSpecial  = 0;
	INT NumPolys    = 0;

	for( FStaticBrushIterator It(Level); It; ++It )
	{
		NumBrushes++;
		UModel* Brush        = It->Brush;
		UPolys* BrushEdPolys = Brush->Polys;

		if      (It->CsgOper == CSG_Add)		NumAdd++;
		else if (It->CsgOper == CSG_Subtract)	NumSubtract++;
		else									NumSpecial++;

		NumPolys += BrushEdPolys->Element.Num();
	}

	if     ( appStricmp(Item,TEXT("Brushes"       ))==0 ) Ar.Logf(TEXT("%i"),NumBrushes-1);
	else if( appStricmp(Item,TEXT("Add"           ))==0 ) Ar.Logf(TEXT("%i"),NumAdd);
	else if( appStricmp(Item,TEXT("Subtract"      ))==0 ) Ar.Logf(TEXT("%i"),NumSubtract);
	else if( appStricmp(Item,TEXT("Special"       ))==0 ) Ar.Logf(TEXT("%i"),NumSpecial);
	else if( appStricmp(Item,TEXT("AvgPolys"      ))==0 ) Ar.Logf(TEXT("%i"),NumPolys/Max(1,NumBrushes-1));
	else if( appStricmp(Item,TEXT("TotalPolys"    ))==0 ) Ar.Logf(TEXT("%i"),NumPolys);
	else if( appStricmp(Item,TEXT("Points"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->Points.Num());
	else if( appStricmp(Item,TEXT("Vectors"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->Vectors.Num());
	else if( appStricmp(Item,TEXT("Sides"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->NumSharedSides);
	else if( appStricmp(Item,TEXT("Zones"		  ))==0 ) Ar.Logf(TEXT("%i"),Level->Model->NumZones-1);
	else if( appStricmp(Item,TEXT("DuplicateBrush"))==0 )
	{
		// Duplicate brush.
		for( INT i=0; i<Level->Actors.Num(); i++ )
			if
			(	Level->Actors(i)
			&&	Cast<ABrush>(Level->Actors(i))
			&&	GSelectionTools.IsSelected( Level->Actors(i) ) )
			{
				ABrush* Actor    = (ABrush*)Level->Actors(i);
				Actor->Location  = Level->Brush()->Location;
				//Actor->Rotation  = Level->Brush()->Rotation;
				Actor->PrePivot  = Level->Brush()->PrePivot;
				GEditor->csgCopyBrush( Actor, Level->Brush(), 0, 0, 1 );
				break;
			}
		debugf( NAME_Log, TEXT("Duplicated brush") );
	}
}
void MapTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{}

/*---------------------------------------------------------------------------------------
   Polys link topic handler
---------------------------------------------------------------------------------------*/

AUTOREGISTER_TOPIC(TEXT("Polys"),PolysTopicHandler);
void PolysTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	DWORD		OnFlags,OffFlags;

	INT n=0, StaticLights=0, Meshels=0, MeshU=0, MeshV=0;
	FString MaterialName = TEXT("");
	OffFlags = (DWORD)~0;
	OnFlags  = (DWORD)~0;
	for( INT i=0; i<Level->Model->Surfs.Num(); i++ )
	{
		FBspSurf *Poly = &Level->Model->Surfs(i);
		if( Poly->PolyFlags&PF_Selected )
		{
			if( Poly->Material )
			{
				FString Name = Poly->Material->GetFullName();
				Name = Name.Right( Name.Len() - Name.InStr(TEXT(" "), 0) );
				if( (MaterialName == TEXT("") || MaterialName == Name) && MaterialName != TEXT("Multiple Materials") )
					MaterialName = Name;
				else
					MaterialName = TEXT("Multiple Materials");
			}

			OnFlags  &=  Poly->PolyFlags;
			OffFlags &= ~Poly->PolyFlags;
			n++;
		}
	}
	if      (!appStricmp(Item,TEXT("NumSelected")))				Ar.Logf(TEXT("%i"),n);
	else if (!appStricmp(Item,TEXT("StaticLights")))			Ar.Logf(TEXT("%i"),StaticLights);
	else if (!appStricmp(Item,TEXT("Meshels")))					Ar.Logf(TEXT("%i"),Meshels);
	else if (!appStricmp(Item,TEXT("SelectedSetFlags")))		Ar.Logf(TEXT("%u"),OnFlags  & ~PF_NoEdit);
	else if (!appStricmp(Item,TEXT("SelectedClearFlags")))		Ar.Logf(TEXT("%u"),OffFlags & ~PF_NoEdit);
	else if (!appStricmp(Item,TEXT("MeshSize")) && n==1)		Ar.Logf(TEXT("%ix%i"),MeshU,MeshV);
	else if (!appStricmp(Item,TEXT("MaterialName")))			Ar.Logf(TEXT("%s"),*MaterialName);

}
void PolysTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
