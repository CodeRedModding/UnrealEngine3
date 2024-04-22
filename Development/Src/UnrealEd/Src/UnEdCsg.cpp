/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "SurfaceIterators.h"
#include "EngineProcBuildingClasses.h"
#include "BSPOps.h"

// Globals:
static TArray<BYTE*> GFlags1;
static TArray<BYTE*> GFlags2;

/*-----------------------------------------------------------------------------
	Helper classes.
-----------------------------------------------------------------------------*/

/**
 * Iterator used to iterate over all static brush actors in the current level.
 */
class FStaticBrushIterator
{
public:
	/**
	 * Default constructor, initializing all member variables and iterating to first.
	 */
	FStaticBrushIterator()
	:	ActorIndex( -1 ),
		ReachedEnd( FALSE )
	{
		// Iterate to first.
		++(*this);
	}

	/**
	 * Iterates to next suitable actor.
	 */
	void operator++()
	{
		UBOOL FoundSuitableActor = FALSE;
		while( !ReachedEnd && !FoundSuitableActor )
		{
			if( ++ActorIndex >= GWorld->CurrentLevel->Actors.Num() )
			{
				ReachedEnd = TRUE;
			}
			else
			{
				//@todo locked levels - should we skip brushes contained by locked levels?
				AActor* Actor = GWorld->CurrentLevel->Actors(ActorIndex);
				FoundSuitableActor = Actor && Actor->IsStaticBrush();
			}
		}
	}

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	AActor* operator*()
	{
		check(ActorIndex<=GWorld->CurrentLevel->Actors.Num());
		check(!ReachedEnd);
		AActor* Actor = GWorld->CurrentLevel->Actors(ActorIndex);
		return Actor;
	}

	/**
	 * Returns the current suitable actor pointed at by the Iterator
	 *
	 * @return	Current suitable actor
	 */
	AActor* operator->()
	{
		check(ActorIndex<=GWorld->CurrentLevel->Actors.Num());
		check(!ReachedEnd);
		AActor* Actor = GWorld->CurrentLevel->Actors(ActorIndex);
		return Actor;
	}

	/**
	 * Returns whether the iterator has reached the end and no longer points
	 * to a suitable actor.
	 *
	 * @return TRUE if iterator points to a suitable actor, FALSE if it has reached the end
	 */
	operator UBOOL()
	{
		return !ReachedEnd;
	}

protected:
	/** Current index into actors array							*/
	INT		ActorIndex;
	/** Whether we already reached the end						*/
	UBOOL	ReachedEnd;
};

/*-----------------------------------------------------------------------------
	CSG Rebuilding.
-----------------------------------------------------------------------------*/

//
// Repartition the bsp.
//
void UEditorEngine::bspRepartition( INT iNode, UBOOL Simple )
{
	bspBuildFPolys( GWorld->GetModel(), 1, iNode );
	bspMergeCoplanars( GWorld->GetModel(), 0, 0 );
	FBSPOps::bspBuild( GWorld->GetModel(), FBSPOps::BSP_Good, 12, 70, Simple, iNode );
	FBSPOps::bspRefresh( GWorld->GetModel(), 1 );

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
void UEditorEngine::csgRebuild()
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("RebuildingGeometry")), FALSE );
	FBSPOps::GFastRebuild = 1;

	FinishAllSnaps();

	// Empty the model out.
	GWorld->GetModel()->EmptyModel( 1, 1 );

	// Count brushes.
	INT BrushTotal=0, BrushCount=0;
	for( FStaticBrushIterator It; It; ++It )
	{
		ABrush* Brush = CastChecked<ABrush>(*It);
		if( !Brush->IsABuilderBrush() )
		{
			BrushTotal++;
		}
	}

	// Check for the giant cube brush that is created for subtractive levels.
	// If it's found, apply the RemoveSurfaceMaterial to its polygons so they aren't lit or drawn.
	for(FStaticBrushIterator GiantCubeBrushIt;GiantCubeBrushIt;++GiantCubeBrushIt)
	{
		ABrush* GiantCubeBrush = CastChecked<ABrush>(*GiantCubeBrushIt);
		if(GiantCubeBrush->Brush && GiantCubeBrush->Brush->Bounds.SphereRadius > HALF_WORLD_MAX)
		{
			check(GiantCubeBrush->Brush->Polys);
			for(INT PolyIndex = 0;PolyIndex < GiantCubeBrush->Brush->Polys->Element.Num();PolyIndex++)
			{
				FPoly& Polygon = GiantCubeBrush->Brush->Polys->Element(PolyIndex);
				const FLOAT PolygonArea = Polygon.Area();
				if(PolygonArea > WORLD_MAX * WORLD_MAX * 0.99f && PolygonArea < WORLD_MAX * WORLD_MAX * 1.01f)
				{
					Polygon.Material = GEngine->RemoveSurfaceMaterial;
				}
			}
		}
	}

	// Compose all structural brushes and portals.
	for( FStaticBrushIterator It; It; ++It )
	{	
		ABrush* Brush = CastChecked<ABrush>(*It);
		if( !Brush->IsABuilderBrush() )
		{
			if
			(  !(Brush->PolyFlags&PF_Semisolid)
			||	(Brush->CsgOper!=CSG_Add)
			||	(Brush->PolyFlags&PF_Portal) )
			{
				// Treat portals as solids for cutting.
				if( Brush->PolyFlags & PF_Portal )
				{
					Brush->PolyFlags = (Brush->PolyFlags & ~PF_Semisolid) | PF_NotSolid;
				}
				BrushCount++;
				GWarn->StatusUpdatef( BrushCount, BrushTotal, LocalizeSecure(LocalizeUnrealEd(TEXT("ApplyingStructuralBrushF")), BrushCount, BrushTotal) );
				bspBrushCSG( Brush, GWorld->GetModel(), Brush->PolyFlags, (ECsgOper)Brush->CsgOper, FALSE, TRUE, FALSE );
			}
		}
	}

	// Repartition the structural BSP.
	{
		GWarn->StatusUpdatef( 0, 4, *LocalizeUnrealEd(TEXT("RebuildBSPBuildingPolygons")) );
		bspBuildFPolys( GWorld->GetModel(), 1, 0 );

		GWarn->StatusUpdatef( 1, 4, *LocalizeUnrealEd(TEXT("RebuildBSPMergingPlanars")) );
		bspMergeCoplanars( GWorld->GetModel(), 0, 0 );

		GWarn->StatusUpdatef( 2, 4, *LocalizeUnrealEd(TEXT("RebuildBSPPartitioning")) );
		FBSPOps::bspBuild( GWorld->GetModel(), FBSPOps::BSP_Optimal, 15, 70, 0, 0 );

		GWarn->StatusUpdatef( 3, 4, *LocalizeUnrealEd(TEXT("RebuildBSPBuildingVisibilityZones")) );
		TestVisibility( GWorld->GetModel(), 0, 0 );

		GWarn->UpdateProgress( 4, 4 );
	}

	// Remember leaves.
	TArray<INT> iFronts, iBacks;
	if( GWorld->GetModel()->Nodes.Num() )
	{
		EnlistLeaves( GWorld->GetModel(), iFronts, iBacks, 0 );
	}

	// Compose all detail brushes.
	for( FStaticBrushIterator It; It; ++It )
	{
		ABrush* Brush = CastChecked<ABrush>(*It);
		if
		(	!Brush->IsABuilderBrush()
		&&	(Brush->PolyFlags&PF_Semisolid)
		&& !(Brush->PolyFlags&PF_Portal)
		&&	Brush->CsgOper==CSG_Add )
		{
			BrushCount++;
			GWarn->StatusUpdatef( BrushCount, BrushTotal, LocalizeSecure(LocalizeUnrealEd(TEXT("ApplyingDetailBrushF")), BrushCount, BrushTotal) );
			bspBrushCSG( Brush, GWorld->GetModel(), Brush->PolyFlags, (ECsgOper)Brush->CsgOper, FALSE, TRUE, FALSE );
		}
	}

	// Optimize the sub-bsp's.
	GWarn->StatusUpdatef( 0, 4,  *LocalizeUnrealEd(TEXT("RebuildCSGOptimizingSubBSPs")) );
	INT iNode;
	for( TArray<INT>::TIterator ItF(iFronts); ItF; ++ItF )
	{
		if( (iNode=GWorld->GetModel()->Nodes(*ItF).iFront)!=INDEX_NONE )
		{
			bspRepartition( iNode, 2 );
		}
	}
	for( TArray<INT>::TIterator ItB(iBacks); ItB; ++ItB )
	{
		if( (iNode=GWorld->GetModel()->Nodes(*ItB).iBack)!=INDEX_NONE )
		{
			bspRepartition( iNode, 2 );
		}
	}

	GWarn->StatusUpdatef( 1, 4, *LocalizeUnrealEd(TEXT("RebuildBSPOptimizingGeometry")) );
	bspOptGeom( GWorld->GetModel() );

	// Build bounding volumes.
	GWarn->StatusUpdatef( 2, 4,  *LocalizeUnrealEd(TEXT("RebuildCSGBuildingBoundingVolumes")) );
	FBSPOps::bspBuildBounds( GWorld->GetModel() );

	// Rebuild dynamic brush BSP's.
	GWarn->StatusUpdatef( 3, 4,  *LocalizeUnrealEd(TEXT("RebuildCSGRebuildingDynamicBrushBSPs")) );

	TArray<ABrush*> DynamicBrushes;
	DynamicBrushes.Empty();
	for( FActorIterator It; It; ++It )
	{
		ABrush* B=Cast<ABrush>(*It);
		if ( B && B->Brush && !B->IsStaticBrush() )
		{
			DynamicBrushes.AddItem(B);
		}
	}

	GWarn->PushStatus();
		GWarn->StatusUpdatef( 0, DynamicBrushes.Num(),  *LocalizeUnrealEd(TEXT("RebuildCSGRebuildingDynamicBrushBSPs")) );
		for ( INT BrushIndex = 0; BrushIndex < DynamicBrushes.Num(); BrushIndex++ )
		{
			GWarn->UpdateProgress(BrushIndex, DynamicBrushes.Num());

			ABrush* B = DynamicBrushes(BrushIndex);
			FBSPOps::csgPrepMovingBrush(B);
			
			if ( GEngine->GetMapBuildCancelled() )
			{
				break;
			}
		}
	GWarn->PopStatus();

	GWarn->UpdateProgress( 4, 4 );

	// Empty EdPolys.
	GWorld->GetModel()->Polys->Element.Empty();

	// Done.
	FBSPOps::GFastRebuild = 0;
	GWorld->CurrentLevel->MarkPackageDirty();
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
	if( !Surf.Actor || Surf.Actor->Brush->Polys->Element.Num() == 0 )
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

	Model->InvalidSurfaces = TRUE;
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

		for( INT edge = 0 ; edge < SrcPoly->Vertices.Num() ; edge++ )
		{
			FEdge SrcEdge = FEdge( SrcPoly->Vertices(edge), SrcPoly->Vertices( edge+1 < SrcPoly->Vertices.Num() ? edge+1 : 0 ) );
			FPlane SrcEdgePlane( SrcEdge.Vertex[0], SrcEdge.Vertex[1], SrcEdge.Vertex[0] + (SrcPoly->Normal * 16) );

			for( INT poly2 = 0 ; poly2 < InPolyList->Num() ; poly2++ )
			{
				FPoly* CmpPoly = &(*InPolyList)(poly2);

				// We can't compare to ourselves.
				if( CmpPoly == SrcPoly )
					continue;

				for( INT edge2 = 0 ; edge2 < CmpPoly->Vertices.Num() ; edge2++ )
				{
					FEdge CmpEdge = FEdge( CmpPoly->Vertices(edge2), CmpPoly->Vertices( edge2+1 < CmpPoly->Vertices.Num() ? edge2+1 : 0 ) );

					// If both vertices on this edge lie on the same plane as the original edge, create
					// a sphere around the original 2 vertices.  If either of this edges vertices are inside of
					// that sphere, we need to split the original edge by adding a vertex to it's poly.
					if( Abs( FPointPlaneDist( CmpEdge.Vertex[0], SrcEdge.Vertex[0], SrcEdgePlane ) ) < THRESH_POINT_ON_PLANE
							&& Abs( FPointPlaneDist( CmpEdge.Vertex[1], SrcEdge.Vertex[0], SrcEdgePlane ) ) < THRESH_POINT_ON_PLANE )
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
		for( INT vtx = 0 ; vtx < Poly->Vertices.Num() ; vtx++ )
			new( TempEdges )FEdge( Poly->Vertices(vtx), Poly->Vertices( vtx+1 < Poly->Vertices.Num() ? vtx+1 : 0) );
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

/**
 * Generates a list of brushes corresponding to the set of selected surfaces for the specified model.
 */
static void GetListOfUniqueBrushes( TArray<ABrush*>* InBrushes, UModel *Model )
{
	InBrushes->Empty();

	// Generate a list of unique brushes.
	for( INT i = 0 ; i < Model->Surfs.Num() ; i++ )
	{
		FBspSurf* Surf = &Model->Surfs(i);
		if( Surf->PolyFlags & PF_Selected )
		{
			ABrush* ParentBrush = Cast<ABrush>(Surf->Actor);
			if ( ParentBrush )
			{
				// See if we've already got this brush ...
				INT brush;
				for( brush = 0 ; brush < InBrushes->Num() ; brush++ )
				{
					if( ParentBrush == (*InBrushes)(brush) )
					{
						break;
					}
				}

				// ... if not, add it to the list.
				if( brush == InBrushes->Num() )
				{
					(*InBrushes)( InBrushes->Add() ) = ParentBrush;
				}
			}
		}
	}
}

void UEditorEngine::polySelectAll(UModel *Model)
{
	polySetAndClearPolyFlags(Model,PF_Selected,0,0,0);
}

void UEditorEngine::polySelectMatchingGroups( UModel* Model )
{
	// @hack: polySelectMatchingGroups: do nothing for now as temp fix until this can be rewritten (crashes a lot)
#if 0
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
			GEditor->SelectBSPSurf( Model, i, TRUE, FALSE );
		}
	}
	NoteSelectionChange();
#endif
}

void UEditorEngine::polySelectMatchingItems(UModel *Model)
{
#if 0
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
				GEditor->SelectBSPSurf( Model, i, TRUE, FALSE );
			}
		}
	}
	NoteSelectionChange();
#endif
}

enum EAdjacentsType
{
	ADJACENT_ALL,		// All adjacent polys
	ADJACENT_COPLANARS,	// Adjacent coplanars only
	ADJACENT_WALLS,		// Adjacent walls
	ADJACENT_FLOORS,	// Adjacent floors or ceilings
	ADJACENT_SLANTS,	// Adjacent slants
};

/**
 * Selects all adjacent polygons (only coplanars if Coplanars==1)
 * @return		Number of polygons newly selected.
 */
static INT TagAdjacentsType(EAdjacentsType AdjacentType)
{
	// Allocate GFlags1
	check( GFlags1.Num() == 0 );
	FOR_EACH_UMODEL;
		BYTE* Ptr = new BYTE[MAXWORD+1];
		appMemzero( Ptr, MAXWORD+1 );
		GFlags1.AddItem( Ptr );
	END_FOR_EACH_UMODEL;

	FVert		*VertPool;
	FVector		*Base,*Normal;
	BYTE		b;
	INT		    i;
	int			Selected,Found;

	Selected = 0;

	// Find all points corresponding to selected vertices:
	INT ModelIndex1 = 0;
	FOR_EACH_UMODEL;
		BYTE* Flags1 = GFlags1(ModelIndex1++);
		for (i=0; i<Model->Nodes.Num(); i++)
		{
			FBspNode &Node = Model->Nodes(i);
			FBspSurf &Poly = Model->Surfs(Node.iSurf);
			if (Poly.PolyFlags & PF_Selected)
			{
				VertPool = &Model->Verts(Node.iVertPool);
				for (b=0; b<Node.NumVertices; b++)
				{
					Flags1[(VertPool++)->pVertex] = 1;
				}
			}
		}
	END_FOR_EACH_UMODEL;

	// Select all unselected nodes for which two or more vertices are selected:
	ModelIndex1 = 0;
	INT ModelIndex2 = -1;
	FOR_EACH_UMODEL;
		BYTE* Flags1 = GFlags1(ModelIndex1);
		ModelIndex1++;
		ModelIndex2++;
		for( i = 0 ; i < Model->Nodes.Num() ; i++ )
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
				for (b=0; b<Node.NumVertices; b++) Found += Flags1[(VertPool++)->pVertex];
				//
				if (AdjacentType == ADJACENT_COPLANARS)
				{
					if (!GFlags2(ModelIndex2)[Node.iSurf]) Found=0;
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
					GEditor->SelectBSPSurf( Model, Node.iSurf, TRUE, FALSE );
					Selected++;
				}
			}
		}
	END_FOR_EACH_UMODEL;

	// Free GFlags1.
	for ( INT i = 0 ; i < GFlags1.Num() ; ++i )
	{
		delete[] GFlags1(i);
	}
	GFlags1.Empty();

	GEditor->NoteSelectionChange();
	return Selected;
}

void UEditorEngine::polySelectAdjacents(UModel *Model)
{
	do {} while (TagAdjacentsType(ADJACENT_ALL) > 0);
}

void UEditorEngine::polySelectCoplanars(UModel *Model)
{
	// Allocate GFlags2
	check( GFlags2.Num() == 0 );
	FOR_EACH_UMODEL;
		BYTE* Ptr = new BYTE[MAXWORD+1];
		appMemzero( Ptr, MAXWORD+1 );
		GFlags2.AddItem( Ptr );
	END_FOR_EACH_UMODEL;

	/////////////
	// Tag coplanars.

	INT ModelIndex = 0;
	FOR_EACH_UMODEL;
		BYTE* Flags2 = GFlags2(ModelIndex++);
		for(INT SelectedNodeIndex = 0;SelectedNodeIndex < Model->Nodes.Num();SelectedNodeIndex++)
		{
			FBspNode&	SelectedNode = Model->Nodes(SelectedNodeIndex);
			FBspSurf&	SelectedSurf = Model->Surfs(SelectedNode.iSurf);

			if(SelectedSurf.PolyFlags & PF_Selected)
			{
				const FVector	SelectedBase = Model->Points(Model->Verts(SelectedNode.iVertPool).pVertex);
				const FVector SelectedNormal = Model->Vectors(SelectedSurf.vNormal);

				for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
				{
					FBspNode&	Node = Model->Nodes(NodeIndex);
					FBspSurf&	Surf = Model->Surfs(Node.iSurf);
					const FVector Base = Model->Points(Model->Verts(Node.iVertPool).pVertex);
					const FVector Normal = Model->Vectors(Surf.vNormal);

					if(FCoplanar(SelectedBase,SelectedNormal,Base,Normal) && !(Surf.PolyFlags & PF_Selected))
					{
						Flags2[Node.iSurf] = 1;
					}
				}
			}
		}
	END_FOR_EACH_UMODEL;

	do {} while (TagAdjacentsType(ADJACENT_COPLANARS) > 0);

	// Free GFlags2
	for ( INT i = 0 ; i < GFlags2.Num() ; ++i )
	{
		delete[] GFlags2(i);
	}
	GFlags2.Empty();
}

void UEditorEngine::polySelectMatchingBrush(UModel *Model)
{
	TArray<ABrush*> Brushes;
	GetListOfUniqueBrushes( &Brushes, Model );

	// Select all the faces.
	for( INT i = 0 ; i < Model->Surfs.Num() ; i++ )
	{
		FBspSurf* Surf = &Model->Surfs(i);
		ABrush* SurfBrushActor = Cast<ABrush>(Surf->Actor);
		if ( SurfBrushActor )
		{
			// Select all the polys on each brush in the unique list.
			for( INT brush = 0 ; brush < Brushes.Num() ; brush++ )
			{
				ABrush* CurBrush = Brushes(brush);
				if( SurfBrushActor == CurBrush )
				{
					for( INT poly = 0 ; poly < CurBrush->Brush->Polys->Element.Num() ; poly++ )
					{
						if( Surf->iBrushPoly == poly )
						{
							Model->ModifySurf( i, 0 );
							GEditor->SelectBSPSurf( Model, i, TRUE, FALSE );
						}
					}
				}
			}
		}
	}
	NoteSelectionChange();
}

/**
 * Selects surfaces whose material matches that of any selected surfaces.
 *
 * @param	bCurrentLevelOnly		If TRUE, consider onl surfaces in the current level.
 */
void UEditorEngine::polySelectMatchingMaterial(UBOOL bCurrentLevelOnly)
{
	// TRUE if at least one surface was selected.
	UBOOL bSurfaceWasSelected = FALSE;

	// TRUE if default material representations have already been added to the materials list.
	UBOOL bDefaultMaterialAdded = FALSE;

	TArray<UMaterialInterface*> Materials;

	if ( bCurrentLevelOnly )
	{
		// Get list of unique materials that are on selected faces.
		for ( TSelectedSurfaceIterator<FCurrentLevelSurfaceLevelFilter> It ; It ; ++It )
		{
			if ( It->Material && It->Material != GEngine->DefaultMaterial )
			{
				Materials.AddUniqueItem( It->Material );
			}
			else if ( !bDefaultMaterialAdded )
			{
				bDefaultMaterialAdded = TRUE;

				// Add both representations of the default material.
				Materials.AddUniqueItem( NULL );
				Materials.AddUniqueItem( GEngine->DefaultMaterial );
			}
		}

		// Select all surfaces with matching materials.
		for ( TSurfaceIterator<FCurrentLevelSurfaceLevelFilter> It ; It ; ++It )
		{
			// Map the default material to NULL, so that NULL assignments match manual default material assignments.
			if( Materials.ContainsItem( It->Material ) )
			{
				UModel* Model = It.GetModel();
				const INT SurfaceIndex = It.GetSurfaceIndex();
				Model->ModifySurf( SurfaceIndex, 0 );
				GEditor->SelectBSPSurf( Model, SurfaceIndex, TRUE, FALSE );
				bSurfaceWasSelected = TRUE;
			}
		}
	}
	else
	{
		// Get list of unique materials that are on selected faces.
		for ( TSelectedSurfaceIterator<> It ; It ; ++It )
		{
			if ( It->Material && It->Material != GEngine->DefaultMaterial )
			{
				Materials.AddUniqueItem( It->Material );
			}
			else if ( !bDefaultMaterialAdded )
			{
				bDefaultMaterialAdded = TRUE;

				// Add both representations of the default material.
				Materials.AddUniqueItem( NULL );
				Materials.AddUniqueItem( GEngine->DefaultMaterial );
			}
		}

		// Select all surfaces with matching materials.
		for ( TSurfaceIterator<> It ; It ; ++It )
		{
			// Map the default material to NULL, so that NULL assignments match manual default material assignments.
			if( Materials.ContainsItem( It->Material ) )
			{
				UModel* Model = It.GetModel();
				const INT SurfaceIndex = It.GetSurfaceIndex();
				Model->ModifySurf( SurfaceIndex, 0 );
				GEditor->SelectBSPSurf( Model, SurfaceIndex, TRUE, FALSE );
				bSurfaceWasSelected = TRUE;
			}
		}
	}

	if ( bSurfaceWasSelected )
	{
		NoteSelectionChange();
	}
}

/**
 * Selects surfaces whose lightmap resolution matches that of any selected surfaces.
 *
 * @param	bCurrentLevelOnly		If TRUE, select
 */
void UEditorEngine::polySelectMatchingResolution(UBOOL bCurrentLevelOnly)
{
	// TRUE if at least one surface was selected.
	UBOOL bSurfaceWasSelected = FALSE;

	TArray<FLOAT> SelectedResolutions;

	if (bCurrentLevelOnly == TRUE)
	{
		for (TSelectedSurfaceIterator<FCurrentLevelSurfaceLevelFilter> It; It; ++It)
		{
			SelectedResolutions.AddUniqueItem(It->ShadowMapScale);
		}

		if (SelectedResolutions.Num() > 0)
		{
			if (SelectedResolutions.Num() > 1)
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("BSPSelect_DifferentResolutionsSelected"));
			}
			else
			{
				// Select all surfaces with matching materials.
				for (TSurfaceIterator<FCurrentLevelSurfaceLevelFilter> It; It; ++It)
				{
					if (SelectedResolutions.ContainsItem(It->ShadowMapScale))
					{
						UModel* Model = It.GetModel();
						const INT SurfaceIndex = It.GetSurfaceIndex();
						Model->ModifySurf( SurfaceIndex, 0 );
						GEditor->SelectBSPSurf( Model, SurfaceIndex, TRUE, FALSE );
						bSurfaceWasSelected = TRUE;
					}
				}
			}
		}
	}
	else
	{
		for (TSelectedSurfaceIterator<> It; It; ++It)
		{
			SelectedResolutions.AddUniqueItem(It->ShadowMapScale);
		}

		if (SelectedResolutions.Num() > 0)
		{
			if (SelectedResolutions.Num() > 1)
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("BSPSelect_DifferentResolutionsSelected"));
			}
			else
			{
				// Select all surfaces with matching materials.
				for (TSurfaceIterator<> It; It; ++It)
				{
					if (SelectedResolutions.ContainsItem(It->ShadowMapScale))
					{
						UModel* Model = It.GetModel();
						const INT SurfaceIndex = It.GetSurfaceIndex();
						Model->ModifySurf( SurfaceIndex, 0 );
						GEditor->SelectBSPSurf( Model, SurfaceIndex, TRUE, FALSE );
						bSurfaceWasSelected = TRUE;
					}
				}
			}
		}
	}

	if ( bSurfaceWasSelected )
	{
		NoteSelectionChange();
	}
}

void UEditorEngine::polySelectAdjacentWalls(UModel *Model)
{
	do 
	{
	} 
	while (TagAdjacentsType(ADJACENT_WALLS) > 0);
}

void UEditorEngine::polySelectAdjacentFloors(UModel *Model)
{
	do
	{
	}
	while (TagAdjacentsType(ADJACENT_FLOORS) > 0);
}

void UEditorEngine::polySelectAdjacentSlants(UModel *Model)
{
	do 
	{
	} 
	while (TagAdjacentsType(ADJACENT_SLANTS) > 0);
}

void UEditorEngine::polySelectReverse(UModel *Model)
{
	for (INT i=0; i<Model->Surfs.Num(); i++)
	{
		FBspSurf *Poly = &Model->Surfs(i);
		Model->ModifySurf( i, 0 );
		Poly->PolyFlags ^= PF_Selected;
	}
}

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
			}
		}
		else
		{
			if (Poly->PolyFlags & PF_Memorized)
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Memorized);
			}
		}
	}
}

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
			}
		}
		else
		{
			if (Poly->PolyFlags & PF_Selected)
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Selected);
			}
		}
	}
}

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
			}
		}
		else
		{
			if (Poly->PolyFlags & PF_Selected)
			{
				Model->ModifySurf( i, 0 );
				Poly->PolyFlags &= (~PF_Selected);
			}
		}
	}
}

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
			}
		}
	}
}

void UEditorEngine::polyIntersectSet(UModel *Model)
{
	for (INT i=0; i<Model->Surfs.Num(); i++)
	{
		FBspSurf *Poly = &Model->Surfs(i);
		if ((Poly->PolyFlags & PF_Memorized) && !(Poly->PolyFlags & PF_Selected))
		{
			Model->ModifySurf( i, 0 );
			Poly->PolyFlags |= PF_Selected;
		}
	}
}

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
			{
				iZoneList.AddUniqueItem( Node->iZone[1] ); //front zone
			}

			if( Node->iZone[0] != 0 )
			{
				iZoneList.AddUniqueItem( Node->iZone[0] ); //back zone
			}
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
				Model->ModifySurf( i, 0 );
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

static void MapSelect( BRUSH_SEL_FUNC Func, INT Tag )
{
	for( FStaticBrushIterator It; It; ++It )
	{
		ABrush* Brush = CastChecked<ABrush>(*It);
		if ( !GEditor->bPrefabsLocked || !Brush->IsInPrefabInstance() )
		{
			if( Func( Brush, Tag ) )
			{
				GEditor->SelectActor( Brush, TRUE, NULL, FALSE );
			}
			else
			{
				GEditor->SelectActor( Brush, FALSE, NULL, FALSE );
			}
		}
	}

	GEditor->NoteSelectionChange();
}

/**
 * Selects no brushes.
 */
static INT BrushSelectNoneFunc( ABrush* Actor, INT Tag )
{
	return 0;
}

/**
 * Selects brushes by their CSG operation.
 */
static INT BrushSelectOperationFunc( ABrush* Actor, INT Tag )
{
	return ((ECsgOper)Actor->CsgOper == Tag) && !(Actor->PolyFlags & (PF_NotSolid | PF_Semisolid));
}
void UEditorEngine::mapSelectOperation(ECsgOper CsgOper)
{
	MapSelect( BrushSelectOperationFunc, CsgOper );
}

INT BrushSelectFlagsFunc( ABrush* Actor, INT Tag )
{
	return Actor->PolyFlags & Tag;
}
void UEditorEngine::mapSelectFlags(DWORD Flags)
{
	MapSelect( BrushSelectFlagsFunc, (int)Flags );
}

/*---------------------------------------------------------------------------------------
   Other map brush functions
---------------------------------------------------------------------------------------*/

//
// Put the first selected brush into the current Brush.
//
void UEditorEngine::mapBrushGet()
{
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor->IsABrush() && !Actor->IsABuilderBrush() )
		{
			ABrush* BrushActor = static_cast<ABrush*>( Actor );

			GWorld->GetBrush()->Modify();
			GWorld->GetBrush()->Brush->Polys->Element = BrushActor->Brush->Polys->Element;
			GWorld->GetBrush()->CopyPosRotScaleFrom( BrushActor );

			GWorld->GetBrush()->ClearComponents();
			GWorld->GetBrush()->ConditionalUpdateComponents();
			break;
		}
	}

	GEditor->SelectNone( FALSE, TRUE );
	GEditor->SelectActor( GWorld->GetBrush(), TRUE, NULL, TRUE );
}

//
// Replace all selected brushes with the current Brush.
//
void UEditorEngine::mapBrushPut()
{
	TArray<FEdMode*> ActiveModes; 
	GEditorModeTools().GetActiveModes( ActiveModes );

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		if( Actor->IsABrush() && !Actor->IsABuilderBrush() )
		{
			ABrush* BrushActor = static_cast<ABrush*>( Actor );

			BrushActor->Modify();
			BrushActor->Brush->Polys->Element = GWorld->GetBrush()->Brush->Polys->Element;
			BrushActor->CopyPosRotScaleFrom( GWorld->GetBrush() );

			BrushActor->ClearComponents();
			BrushActor->ConditionalUpdateComponents();

			for( INT ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
			{
				ActiveModes(ModeIndex)->UpdateInternalData();
			}
		}
	}
}

//
// Generic private routine for send to front / send to back
//
static void SendTo( INT bSendToFirst )
{
	ULevel*	Level = GWorld->CurrentLevel;
	Level->Actors.ModifyAllItems();
	
	// Fire CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied		LevelDirtyCallback;

	//@todo locked levels - do we need to skip locked levels?
	// Partition.
	TArray<AActor*> Lists[2];
	for( INT i=2; i<Level->Actors.Num(); i++ )
	{
		if( Level->Actors(i) )
		{
			Lists[Level->Actors(i)->IsSelected() ^ bSendToFirst ^ 1].AddItem( Level->Actors(i) );
			Level->Actors(i)->MarkPackageDirty();
			LevelDirtyCallback.Request();
		}
	}

	// Refill.
	check(Level->Actors.Num()>=2);
	Level->Actors.Remove(2,Level->Actors.Num()-2);
	for( INT i=0; i<2; i++ )
	{
		for( INT j=0; j<Lists[i].Num(); j++ )
		{
			Level->Actors.AddItem( Lists[i](j) );
		}
	}
}

//
// Send all selected brushes in a level to the front of the hierarchy
//
void UEditorEngine::mapSendToFirst()
{
	SendTo( 0 );
}

//
// Send all selected brushes in a level to the back of the hierarchy
//
void UEditorEngine::mapSendToLast()
{
	SendTo( 1 );
}

/**
 * Swaps position in the actor list for the first two selected actors in the current level
 */
void UEditorEngine::mapSendToSwap()
{
	INT			Count	= 0;
	ULevel*		Level	= GWorld->CurrentLevel;
	AActor**	Actors[2];

	// Fire CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied		LevelDirtyCallback;

	//@todo locked levels - skip for locked levels?
	for( INT i=2; i<Level->Actors.Num() && Count < 2; i++ )
	{
		AActor*& Actor = Level->Actors(i);
		if( Actor && Actor->IsSelected() )
		{
			Actors[Count] = &Actor;
			Count++;
			Actor->MarkPackageDirty();
			LevelDirtyCallback.Request();
		}
	}

	if( Count == 2 )
	{
		GWorld->CurrentLevel->Actors.ModifyAllItems();
		Exchange( *Actors[0], *Actors[1] );
	}
}

void UEditorEngine::mapSetBrush
(
	EMapSetBrushFlags	PropertiesMask,
	WORD				BrushColor,
	FName				GroupName,
	DWORD				SetPolyFlags,
	DWORD				ClearPolyFlags,
	DWORD				CSGOper,
	INT					DrawType
)
{
	// Fire CALLBACK_LevelDirtied when falling out of scope.
	FScopedLevelDirtied		LevelDirtyCallback;

	for( FStaticBrushIterator It; It; ++It )
	{
		ABrush* Brush = CastChecked<ABrush>(*It);
		if( !Brush->IsABuilderBrush() && Brush->IsSelected() )
		{
			if( PropertiesMask & MSB_PolyFlags )
			{
				Brush->Modify();
				Brush->PolyFlags = (Brush->PolyFlags & ~ClearPolyFlags) | SetPolyFlags;
				Brush->ForceUpdateComponents();
				Brush->MarkPackageDirty();
				LevelDirtyCallback.Request();
			}
			if( PropertiesMask & MSB_CSGOper )
			{
				Brush->Modify();
				Brush->CsgOper = CSGOper;
				Brush->ForceUpdateComponents();
				Brush->MarkPackageDirty();
				LevelDirtyCallback.Request();
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
		const FBspSurf&	Surf = Model->Surfs(SurfaceIndex);

		if(Surf.PolyFlags & PF_Selected)
		{
			if(Absolute)
			{
				Model->Points(Surf.pBase) = FVector(0,0,0);
			}

			const FVector TextureU = Model->Vectors(Surf.vTextureU);
			const FVector TextureV = Model->Vectors(Surf.vTextureV);

			Model->Points(Surf.pBase) += PanU * (TextureU / TextureU.SizeSquared());
			Model->Points(Surf.pBase) += PanV * (TextureV / TextureV.SizeSquared());

			polyUpdateMaster(Model,SurfaceIndex,1);
		}
	}
}

//
// Scale textures on selected polys. Doesn't do transaction tracking.
//
void UEditorEngine::polyTexScale( UModel* Model, FLOAT UU, FLOAT UV, FLOAT VU, FLOAT VV, UBOOL Absolute )
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
	}
}
