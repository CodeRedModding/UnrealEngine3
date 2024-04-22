/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "BSPOps.h"

#if !CONSOLE

/** Errors encountered in Csg operation. */
INT FBSPOps::GErrors = 0;
UBOOL FBSPOps::GFastRebuild = FALSE;

static void TagReferencedNodes( UModel *Model, INT *NodeRef, INT *PolyRef, INT iNode )
{
	FBspNode &Node = Model->Nodes(iNode);

	NodeRef[iNode     ] = 0;
	PolyRef[Node.iSurf] = 0;

	if( Node.iFront != INDEX_NONE ) TagReferencedNodes(Model,NodeRef,PolyRef,Node.iFront);
	if( Node.iBack  != INDEX_NONE ) TagReferencedNodes(Model,NodeRef,PolyRef,Node.iBack );
	if( Node.iPlane != INDEX_NONE ) TagReferencedNodes(Model,NodeRef,PolyRef,Node.iPlane);
}

//
// Build a 64-bit zone mask for each node, with a bit set for every
// zone that's referenced by the node and its children.  This is used
// during rendering to reject entire sections of the tree when it's known
// that none of the zones in that section are active.
//
FZoneSet FBSPOps::BuildZoneMasks( UModel* Model, INT iNode )
{
	FBspNode& Node = Model->Nodes(iNode);
	FZoneSet ZoneMask = FZoneSet::NoZones();

	if( Node.iZone[0]!=0 ) ZoneMask.AddZone(Node.iZone[0]);
	if( Node.iZone[1]!=0 ) ZoneMask.AddZone(Node.iZone[1]);

	if( Node.iFront != INDEX_NONE )	ZoneMask |= BuildZoneMasks( Model, Node.iFront );
	if( Node.iBack  != INDEX_NONE )	ZoneMask |= BuildZoneMasks( Model, Node.iBack );
	if( Node.iPlane != INDEX_NONE )	ZoneMask |= BuildZoneMasks( Model, Node.iPlane );

	return ZoneMask;
}

//
// Update a bounding volume by expanding it to enclose a list of polys.
//
static void UpdateBoundWithPolys( FBox& Bound, FPoly** PolyList, INT nPolys )
{
	for( INT i=0; i<nPolys; i++ )
		for( INT j=0; j<PolyList[i]->Vertices.Num(); j++ )
			Bound += PolyList[i]->Vertices(j);
}

//
// Update a convolution hull with a list of polys.
//
static void UpdateConvolutionWithPolys( UModel *Model, INT iNode, FPoly **PolyList, int nPolys )
{
	FBox Box(0);

	FBspNode &Node = Model->Nodes(iNode);
	Node.iCollisionBound = Model->LeafHulls.Num();
	for( int i=0; i<nPolys; i++ )
	{
		if( PolyList[i]->iBrushPoly != INDEX_NONE )
		{
			int j;
			for( j=0; j<i; j++ )
				if( PolyList[j]->iBrushPoly == PolyList[i]->iBrushPoly )
					break;
			if( j >= i )
				Model->LeafHulls.AddItem(PolyList[i]->iBrushPoly);
		}
		for( int j=0; j<PolyList[i]->Vertices.Num(); j++ )
			Box += PolyList[i]->Vertices(j);
	}
	Model->LeafHulls.AddItem(INDEX_NONE);

	// Add bounds.
	Model->LeafHulls.AddItem( *(INT*)&Box.Min.X );
	Model->LeafHulls.AddItem( *(INT*)&Box.Min.Y );
	Model->LeafHulls.AddItem( *(INT*)&Box.Min.Z );
	Model->LeafHulls.AddItem( *(INT*)&Box.Max.X );
	Model->LeafHulls.AddItem( *(INT*)&Box.Max.Y );
	Model->LeafHulls.AddItem( *(INT*)&Box.Max.Z );

}

//
// Cut a partitioning poly by a list of polys, and add the resulting inside pieces to the
// front list and back list.
//
static void SplitPartitioner
(
	UModel*	Model,
	FPoly**	PolyList,
	FPoly**	FrontList,
	FPoly**	BackList,
	INT		n,
	INT		nPolys,
	INT&	nFront, 
	INT&	nBack, 
	FPoly	InfiniteEdPoly,
	TArray<FPoly*>& AllocatedFPolys
)
{
	FPoly FrontPoly,BackPoly;
	while( n < nPolys )
	{
		FPoly* Poly = PolyList[n];
		switch( InfiniteEdPoly.SplitWithPlane(Poly->Vertices(0),Poly->Normal,&FrontPoly,&BackPoly,0) )
		{
			case SP_Coplanar:
				// May occasionally happen.
//				debugf( NAME_Log, TEXT("FilterBound: Got inficoplanar") );
				break;

			case SP_Front:
				// Shouldn't happen if hull is correct.
//				debugf( NAME_Log, TEXT("FilterBound: Got infifront") );
				return;

			case SP_Split:
				InfiniteEdPoly = BackPoly;
				break;

			case SP_Back:
				break;
		}
		n++;
	}

	FPoly* New = new FPoly;
	*New = InfiniteEdPoly;
	New->Reverse();
	New->iBrushPoly |= 0x40000000;
	FrontList[nFront++] = New;
	AllocatedFPolys.AddItem( New );
	
	New = new FPoly;
	*New = InfiniteEdPoly;
	BackList[nBack++] = New;
	AllocatedFPolys.AddItem( New );
}

//
// Build an FPoly representing an "infinite" plane (which exceeds the maximum
// dimensions of the world in all directions) for a particular Bsp node.
//
FPoly FBSPOps::BuildInfiniteFPoly( UModel* Model, INT iNode )
{
	FBspNode &Node   = Model->Nodes  (iNode       );
	FBspSurf &Poly   = Model->Surfs  (Node.iSurf  );
	FVector  Base    = Poly.Plane * Poly.Plane.W;
	FVector  Normal  = Poly.Plane;
	FVector	 Axis1,Axis2;

	// Find two non-problematic axis vectors.
	Normal.FindBestAxisVectors( Axis1, Axis2 );

	// Set up the FPoly.
	FPoly EdPoly;
	EdPoly.Init();
	EdPoly.Normal      = Normal;
	EdPoly.Base        = Base;
	new(EdPoly.Vertices) FVector(Base + Axis1*WORLD_MAX + Axis2*WORLD_MAX);
	new(EdPoly.Vertices) FVector(Base - Axis1*WORLD_MAX + Axis2*WORLD_MAX);
	new(EdPoly.Vertices) FVector(Base - Axis1*WORLD_MAX - Axis2*WORLD_MAX);
	new(EdPoly.Vertices) FVector(Base + Axis1*WORLD_MAX - Axis2*WORLD_MAX);

	return EdPoly;
}

//
// Recursively filter a set of polys defining a convex hull down the Bsp,
// splitting it into two halves at each node and adding in the appropriate
// face polys at splits.
//
static void FilterBound
(
	UModel*			Model,
	FBox*			ParentBound,
	INT				iNode,
	FPoly**			PolyList,
	INT				nPolys,
	INT				Outside
)
{
	FMemMark Mark(GMainThreadMemStack);
	FBspNode&	Node	= Model->Nodes  (iNode);
	FBspSurf&	Surf	= Model->Surfs  (Node.iSurf);
	FVector		Base = Surf.Plane * Surf.Plane.W;
	FVector&	Normal	= Model->Vectors(Surf.vNormal);
	FBox		Bound(0);

	Bound.Min.X = Bound.Min.Y = Bound.Min.Z = +WORLD_MAX;
	Bound.Max.X = Bound.Max.Y = Bound.Max.Z = -WORLD_MAX;

	// Split bound into front half and back half.
	FPoly** FrontList = new(GMainThreadMemStack,nPolys*2+16)FPoly*; int nFront=0;
	FPoly** BackList  = new(GMainThreadMemStack,nPolys*2+16)FPoly*; int nBack=0;

	// Keeping track of allocated FPoly structures to delete later on.
	TArray<FPoly*> AllocatedFPolys;

	FPoly* FrontPoly  = new FPoly;
	FPoly* BackPoly   = new FPoly;

	// Keep track of allocations.
	AllocatedFPolys.AddItem( FrontPoly );
	AllocatedFPolys.AddItem( BackPoly );

	for( INT i=0; i<nPolys; i++ )
	{
		FPoly *Poly = PolyList[i];
		switch( Poly->SplitWithPlane( Base, Normal, FrontPoly, BackPoly, 0 ) )
		{
			case SP_Coplanar:
//				debugf( NAME_Log, TEXT("FilterBound: Got coplanar") );
				FrontList[nFront++] = Poly;
				BackList[nBack++] = Poly;
				break;
			
			case SP_Front:
				FrontList[nFront++] = Poly;
				break;
			
			case SP_Back:
				BackList[nBack++] = Poly;
				break;
			
			case SP_Split:
				FrontList[nFront++] = FrontPoly;
				BackList [nBack++] = BackPoly;

				FrontPoly = new FPoly;
				BackPoly  = new FPoly;

				// Keep track of allocations.
				AllocatedFPolys.AddItem( FrontPoly );
				AllocatedFPolys.AddItem( BackPoly );

				break;

			default:
				appErrorf( TEXT("FZoneFilter::FilterToLeaf: Unknown split code") );
		}
	}
	if( nFront && nBack )
	{
		// Add partitioner plane to front and back.
		FPoly InfiniteEdPoly = FBSPOps::BuildInfiniteFPoly( Model, iNode );
		InfiniteEdPoly.iBrushPoly = iNode;

		SplitPartitioner(Model,PolyList,FrontList,BackList,0,nPolys,nFront,nBack,InfiniteEdPoly,AllocatedFPolys);
	}
	else
	{
// 		if( !nFront ) debugf( NAME_Log, TEXT("FilterBound: Empty fronthull") );
// 		if( !nBack  ) debugf( NAME_Log, TEXT("FilterBound: Empty backhull") );
	}

	// Recursively update all our childrens' bounding volumes.
	if( nFront > 0 )
	{
		if( Node.iFront != INDEX_NONE )
			FilterBound( Model, &Bound, Node.iFront, FrontList, nFront, Outside || Node.IsCsg() );
		else if( Outside || Node.IsCsg() )
			UpdateBoundWithPolys( Bound, FrontList, nFront );
		else
			UpdateConvolutionWithPolys( Model, iNode, FrontList, nFront );
	}
	if( nBack > 0 )
	{
		if( Node.iBack != INDEX_NONE)
			FilterBound( Model, &Bound,Node.iBack, BackList, nBack, Outside && !Node.IsCsg() );
		else if( Outside && !Node.IsCsg() )
			UpdateBoundWithPolys( Bound, BackList, nBack );
		else
			UpdateConvolutionWithPolys( Model, iNode, BackList, nBack );
	}

	// Update parent bound to enclose this bound.
	if( ParentBound )
		*ParentBound += Bound;

	// Delete FPolys allocated above. We cannot use GMainThreadMemStack for FPoly as the array data FPoly contains will be allocated in regular memory.
	for( INT i=0; i<AllocatedFPolys.Num(); i++ )
	{
		FPoly* AllocatedFPoly = AllocatedFPolys(i);
		delete AllocatedFPoly;
	}

	Mark.Pop();
}

/*-----------------------------------------------------------------------------
	Bsp Splitting.
-----------------------------------------------------------------------------*/

//
// Find the best splitting polygon within a pool of polygons, and return its
// index (into the PolyList array).
//
static FPoly *FindBestSplit
(
	INT					NumPolys,
	FPoly**				PolyList,
	FBSPOps::EBspOptimization	Opt,
	INT					Balance,
	INT					InPortalBias
)
{
	check(NumPolys>0);

	// No need to test if only one poly.
	if( NumPolys==1 )
		return PolyList[0];

	FPoly   *Poly, *Best=NULL;
	FLOAT   Score, BestScore;
	int     i, Index, j, Inc;
	int     Splits, Front, Back, Coplanar, AllSemiSolids;

	//PortalBias -- added by Legend on 4/12/2000
	float	PortalBias = InPortalBias / 100.0f;
	Balance &= 0xFF;								// keep only the low byte to recover "Balance"
	//GLog->Logf( TEXT("Balance=%d PortalBias=%f"), Balance, PortalBias );

	if		(Opt==FBSPOps::BSP_Optimal)  Inc = 1;					// Test lots of nodes.
	else if (Opt==FBSPOps::BSP_Good)		Inc = Max(1,NumPolys/20);	// Test 20 nodes.
	else /* BSP_Lame */			Inc = Max(1,NumPolys/4);	// Test 4 nodes.

	// See if there are any non-semisolid polygons here.
	for( i=0; i<NumPolys; i++ )
		if( !(PolyList[i]->PolyFlags & PF_AddLast) )
			break;
	AllSemiSolids = (i>=NumPolys);

	// Search through all polygons in the pool and find:
	// A. The number of splits each poly would make.
	// B. The number of front and back nodes the polygon would create.
	// C. Number of coplanars.
	BestScore = 0;
	for( i=0; i<NumPolys; i+=Inc )
	{
		Splits = Front = Back = Coplanar = 0;
		Index = i-1;
		do
		{
			Index++;
			Poly = PolyList[Index];
		} while( Index<(i+Inc) && Index<NumPolys 
			&& ( (Poly->PolyFlags & PF_AddLast) && !(Poly->PolyFlags & PF_Portal) )
			&& !AllSemiSolids );
		if( Index>=i+Inc || Index>=NumPolys )
			continue;

		for( j=0; j<NumPolys; j+=Inc ) if( j != Index )
		{
			FPoly *OtherPoly = PolyList[j];
			switch( OtherPoly->SplitWithPlaneFast( FPlane( Poly->Vertices(0), Poly->Normal), NULL, NULL ) )
			{
				case SP_Coplanar:
					Coplanar++;
					break;

				case SP_Front:
					Front++;
					break;

				case SP_Back:
					Back++;
					break;

				case SP_Split:
					// Disfavor splitting polys that are zone portals.
					if( !(OtherPoly->PolyFlags & PF_Portal) )
						Splits++;
					else
						Splits += 16;
					break;
			}
		}
		// added by Legend 1/31/1999
		// Score optimization: minimize cuts vs. balance tree (as specified in BSP Rebuilder dialog)
		Score = ( 100.0 - float(Balance) ) * Splits + float(Balance) * Abs( Front - Back );
		if( Poly->PolyFlags & PF_Portal )
		{
			// PortalBias -- added by Legend on 4/12/2000
			//
			// PortalBias enables level designers to control the effect of Portals on the BSP.
			// This effect can range from 0.0 (ignore portals), to 1.0 (portals cut everything).
			//
			// In builds prior to this (since the 221 build dating back to 1/31/1999) the bias
			// has been 1.0 causing the portals to cut the BSP in ways that will potentially
			// degrade level performance, and increase the BSP complexity.
			// 
			// By setting the bias to a value between 0.3 and 0.7 the positive effects of 
			// the portals are preserved without giving them unreasonable priority in the BSP.
			//
			// Portals should be weighted high enough in the BSP to separate major parts of the
			// level from each other (pushing entire rooms down the branches of the BSP), but
			// should not be so high that portals cut through adjacent geometry in a way that
			// increases complexity of the room being (typically, accidentally) cut.
			//
			Score -= ( 100.0 - float(Balance) ) * Splits * PortalBias; // ignore PortalBias of the split polys -- bias toward portal selection for cutting planes!
		}
		//debugf( "  %4d: Score = %f (Front = %4d, Back = %4d, Splits = %4d, Flags = %08X)", Index, Score, Front, Back, Splits, Poly->PolyFlags ); //LEC

		if( Score<BestScore || !Best )
		{
			Best      = Poly;
			BestScore = Score;
		}
	}
	check(Best);
	return Best;
}

//
// Pick a splitter poly then split a pool of polygons into front and back polygons and
// recurse.
//
// iParent = Parent Bsp node, or INDEX_NONE if this is the root node.
// IsFront = 1 if this is the front node of iParent, 0 of back (undefined if iParent==INDEX_NONE)
//
void FBSPOps::SplitPolyList
(
	UModel				*Model,
	INT                 iParent,
	ENodePlace			NodePlace,
	INT                 NumPolys,
	FPoly				**PolyList,
	EBspOptimization	Opt,
	INT					Balance,
	INT					PortalBias,
	INT					RebuildSimplePolys
)
{
	FMemMark Mark(GMainThreadMemStack);

	// Keeping track of allocated FPoly structures to delete later on.
	TArray<FPoly*> AllocatedFPolys;

	// To account for big EdPolys split up.
	int NumPolysToAlloc = NumPolys + 8 + NumPolys/4;
	int NumFront=0; FPoly **FrontList = new(GMainThreadMemStack,NumPolysToAlloc)FPoly*;
	int NumBack =0; FPoly **BackList  = new(GMainThreadMemStack,NumPolysToAlloc)FPoly*;

	FPoly *SplitPoly = FindBestSplit( NumPolys, PolyList, Opt, Balance, PortalBias );

	// Add the splitter poly to the Bsp with either a new BspSurf or an existing one.
	if( RebuildSimplePolys )
	{
		SplitPoly->iLink = Model->Surfs.Num();
	}

	INT iOurNode	= bspAddNode(Model,iParent,NodePlace,0,SplitPoly);
	INT iPlaneNode	= iOurNode;

	// Now divide all polygons in the pool into (A) polygons that are
	// in front of Poly, and (B) polygons that are in back of Poly.
	// Coplanar polys are inserted immediately, before recursing.

	// If any polygons are split by Poly, we ignrore the original poly,
	// split it into two polys, and add two new polys to the pool.
	FPoly *FrontEdPoly = new FPoly;
	FPoly *BackEdPoly  = new FPoly;
	// Keep track of allocations.
	AllocatedFPolys.AddItem( FrontEdPoly );
	AllocatedFPolys.AddItem( BackEdPoly );

	for( INT i=0; i<NumPolys; i++ )
	{
		FPoly *EdPoly = PolyList[i];
		if( EdPoly == SplitPoly )
		{
			continue;
		}

		switch( EdPoly->SplitWithPlane( SplitPoly->Vertices(0), SplitPoly->Normal, FrontEdPoly, BackEdPoly, 0 ) )
		{
			case SP_Coplanar:
	            if( RebuildSimplePolys )
				{
					EdPoly->iLink = Model->Surfs.Num()-1;
				}
				iPlaneNode = bspAddNode( Model, iPlaneNode, NODE_Plane, 0, EdPoly );
				break;
			
			case SP_Front:
	            FrontList[NumFront++] = PolyList[i];
				break;
			
			case SP_Back:
	            BackList[NumBack++] = PolyList[i];
				break;
			
			case SP_Split:

				// Create front & back nodes.
				FrontList[NumFront++] = FrontEdPoly;
				BackList [NumBack ++] = BackEdPoly;

				FrontEdPoly = new FPoly;
				BackEdPoly  = new FPoly;
				// Keep track of allocations.
				AllocatedFPolys.AddItem( FrontEdPoly );
				AllocatedFPolys.AddItem( BackEdPoly );

				break;
		}
	}

	// Recursively split the front and back pools.
	if( NumFront > 0 ) SplitPolyList( Model, iOurNode, NODE_Front, NumFront, FrontList, Opt, Balance, PortalBias, RebuildSimplePolys );
	if( NumBack  > 0 ) SplitPolyList( Model, iOurNode, NODE_Back,  NumBack,  BackList,  Opt, Balance, PortalBias, RebuildSimplePolys );

	// Delete FPolys allocated above. We cannot use GMainThreadMemStack for FPoly as the array data FPoly contains will be allocated in regular memory.
	for( INT i=0; i<AllocatedFPolys.Num(); i++ )
	{
		FPoly* AllocatedFPoly = AllocatedFPolys(i);
		delete AllocatedFPoly;
	}

	Mark.Pop();
}

/** Prepare a moving brush. */
void FBSPOps::csgPrepMovingBrush( ABrush* Actor )
{
	check(Actor);
//	debugf( NAME_Log, TEXT("Preparing brush %s"), *Actor->GetName() );  // moved here so that we can easily debug when an actor has lost parts of its brush

	check(Actor->BrushComponent);
	check(Actor->Brush);
	check(Actor->Brush->RootOutside);

	// Allocate tables.
	Actor->ClearFlags( RF_NotForClient | RF_NotForServer );

	RebuildBrush(Actor->Brush);

	// Make sure simplified collision is up to date.
	Actor->BrushComponent->BuildSimpleBrushCollision();
}

/**
 * Duplicates the specified brush and makes it into a CSG-able level brush.
 * @return		The new brush, or NULL if the original was empty.
 */
void FBSPOps::csgCopyBrush( ABrush* Dest, ABrush* Src, DWORD PolyFlags, EObjectFlags ResFlags, UBOOL NeedsPrep, UBOOL bCopyPosRotScale )
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
	Dest->Brush			= new( Dest, NAME_None, ResFlags )UModel( NULL, Src->Brush->RootOutside );
	Dest->Brush->Polys	= new( Dest->Brush, NAME_None, ResFlags )UPolys;
	check(Dest->Brush->Polys->Element.GetOwner()==Dest->Brush->Polys);
	Dest->Brush->Polys->Element = Src->Brush->Polys->Element;
	check(Dest->Brush->Polys->Element.GetOwner()==Dest->Brush->Polys);
	Dest->BrushComponent->Brush = Dest->Brush;

	// Update poly textures.
	for( INT i=0; i<Dest->Brush->Polys->Element.Num(); i++ )
	{
		Dest->Brush->Polys->Element(i).iBrushPoly = INDEX_NONE;
	}

	// Copy positioning, and build bounding box.
	if(bCopyPosRotScale)
	{
		Dest->CopyPosRotScaleFrom( Src );
	}

	// If it's a moving brush, prep it.
	if( NeedsPrep )
	{
		csgPrepMovingBrush( Dest );
	}
}

/**
 * Adds a brush to the list of CSG brushes in the level, using a CSG operation.
 *
 * @return		A newly-created copy of the brush.
 */
ABrush*	FBSPOps::csgAddOperation( ABrush* Actor, DWORD PolyFlags, ECsgOper CsgOper )
{
	check(Actor);
	check(Actor->BrushComponent);
	check(Actor->Brush);
	check(Actor->Brush->Polys);

	// Can't do this if brush has no polys.
	if( !Actor->Brush->Polys->Element.Num() )
		return NULL;

	// Spawn a new actor for the brush.
	ABrush* Result  = GWorld->SpawnBrush();
	Result->SetFlags( RF_NotForClient | RF_NotForServer );

	// Duplicate the brush.
	csgCopyBrush
	(
		Result,
		Actor,
		PolyFlags,
		RF_NotForClient | RF_NotForServer | RF_Transactional,
		0,
		TRUE
	);
	check(Result->Brush);

	// Assign the default material to the brush's polys.
	for( INT i=0; i<Result->Brush->Polys->Element.Num(); i++ )
	{
		FPoly& CurrentPoly = Result->Brush->Polys->Element(i);
		if ( !CurrentPoly.Material )
		{
			CurrentPoly.Material = GEngine->DefaultMaterial;
		}
	}

	// Set add-info.
	Result->CsgOper = CsgOper;

	Result->ClearComponents();
	Result->ConditionalUpdateComponents();

	return Result;
}

/** Add a new point to the model (preventing duplicates) and return its index. */
static INT AddThing( TArray<FVector>& Vectors, FVector& V, FLOAT Thresh, int Check )
{
	if( Check )
	{
		// See if this is very close to an existing point/vector.		
		for( INT i=0; i<Vectors.Num(); i++ )
		{
			const FVector &TableVect = Vectors(i);
			FLOAT Temp=(V.X - TableVect.X);
			if( (Temp > -Thresh) && (Temp < Thresh) )
			{
				Temp=(V.Y - TableVect.Y);
				if( (Temp > -Thresh) && (Temp < Thresh) )
				{
					Temp=(V.Z - TableVect.Z);
					if( (Temp > -Thresh) && (Temp < Thresh) )
					{
						// Found nearly-matching vector.
						return i;
					}
				}
			}
		}
	}
	return Vectors.AddItem( V );
}

/** Add a new vector to the model, merging near-duplicates, and return its index. */
INT FBSPOps::bspAddVector( UModel* Model, FVector* V, UBOOL Exact )
{
	return AddThing
	(
		Model->Vectors,
		*V,
		Exact ? THRESH_NORMALS_ARE_SAME : THRESH_VECTORS_ARE_NEAR,
		1
	);
}

/** Add a new point to the model, merging near-duplicates, and return its index. */
INT FBSPOps::bspAddPoint( UModel* Model, FVector* V, UBOOL Exact )
{
	FLOAT Thresh = Exact ? THRESH_POINTS_ARE_SAME : THRESH_POINTS_ARE_NEAR;

	// Try to find a match quickly from the Bsp. This finds all potential matches
	// except for any dissociated from nodes/surfaces during a rebuild.
	FVector Temp;
	INT pVertex;
	FLOAT NearestDist = Model->FindNearestVertex(*V,Temp,Thresh,pVertex);
	if( (NearestDist >= 0.0) && (NearestDist <= Thresh) )
	{
		// Found an existing point.
		return pVertex;
	}
	else
	{
		// No match found; add it slowly to find duplicates.
		return AddThing( Model->Points, *V, Thresh, !GFastRebuild );
	}
}


//INT FBSPOps::bspNodeToFPoly( UModel* Model, INT iNode, FPoly* EdPoly );

/**
 * Builds Bsp from the editor polygon set (EdPolys) of a model.
 *
 * Opt     = Bsp optimization, BSP_Lame (fast), BSP_Good (medium), BSP_Optimal (slow)
 * Balance = 0-100, 0=only worry about minimizing splits, 100=only balance tree.
 */
void FBSPOps::bspBuild( UModel* Model, enum EBspOptimization Opt, INT Balance, INT PortalBias, INT RebuildSimplePolys, INT iNode )
{
	INT OriginalPolys = Model->Polys->Element.Num();

	// Empty the model's tables.
	if( RebuildSimplePolys==1 )
	{
		// Empty everything but polys.
		Model->EmptyModel( 1, 0 );
	}
	else if( RebuildSimplePolys==0 )
	{
		// Empty node vertices.
		for( INT i=0; i<Model->Nodes.Num(); i++ )
			Model->Nodes(i).NumVertices = 0;

		// Refresh the Bsp.
		bspRefresh(Model,1);
		
		// Empty nodes.
		Model->EmptyModel( 0, 0 );
	}
	if( Model->Polys->Element.Num() )
	{
		// Allocate polygon pool.
		FMemMark Mark(GMainThreadMemStack);
		FPoly** PolyList = new( GMainThreadMemStack, Model->Polys->Element.Num() )FPoly*;

		// Add all FPolys to active list.
		for( int i=0; i<Model->Polys->Element.Num(); i++ )
			if( Model->Polys->Element(i).Vertices.Num() )
				PolyList[i] = &Model->Polys->Element(i);

		// Now split the entire Bsp by splitting the list of all polygons.
		SplitPolyList
		(
			Model,
			INDEX_NONE,
			NODE_Root,
			Model->Polys->Element.Num(),
			PolyList,
			Opt,
			Balance,
			PortalBias,
			RebuildSimplePolys
		);

		// Now build the bounding boxes for all nodes.
		if( RebuildSimplePolys==0 )
		{
			// Remove unreferenced things.
			bspRefresh( Model, 1 );

			// Rebuild all bounding boxes.
			bspBuildBounds( Model );
		}

		Mark.Pop();
	}

//	debugf( NAME_Log, TEXT("bspBuild built %i convex polys into %i nodes"), OriginalPolys, Model->Nodes.Num() );
}

/**
 * If the Bsp's point and vector tables are nearly full, reorder them and delete unused ones.
 */
void FBSPOps::bspRefresh( UModel* Model, UBOOL NoRemapSurfs )
{
	FMemMark Mark(GMainThreadMemStack);
	INT *VectorRef, *PointRef, *NodeRef, *PolyRef, i;
	TArray<INT*>	VertexRef;
	BYTE  B;

	// Remove unreferenced Bsp surfs.
	NodeRef		= new(GMainThreadMemStack,MEM_Oned,Model->Nodes.Num())INT;
	PolyRef		= new(GMainThreadMemStack,MEM_Oned,Model->Surfs.Num())INT;
	if( Model->Nodes.Num() > 0 )
		TagReferencedNodes( Model, NodeRef, PolyRef, 0 );

	for(INT PortalIndex = 0;PortalIndex < Model->PortalNodes.Num();PortalIndex++)
		TagReferencedNodes(Model,NodeRef,PolyRef,Model->PortalNodes(PortalIndex));

	if( NoRemapSurfs )
		appMemzero(PolyRef,Model->Surfs.Num() * sizeof (INT));

	// Remap Bsp nodes and surfs.
	int n=0;
	for( i=0; i<Model->Surfs.Num(); i++ )
	{
		if( PolyRef[i]!=INDEX_NONE )
		{
			Model->Surfs(n) = Model->Surfs(i);
			PolyRef[i]=n++;
		}
	}
	//debugf( NAME_Log, TEXT("Polys: %i -> %i"), Model->Surfs.Num(), n );
	Model->Surfs.Remove( n, Model->Surfs.Num()-n );

	n=0;
	for( i=0; i<Model->Nodes.Num(); i++ ) if( NodeRef[i]!=INDEX_NONE )
	{
		Model->Nodes(n) = Model->Nodes(i);
		NodeRef[i]=n++;
	}
	//debugf( NAME_Log, TEXT("Nodes: %i -> %i"), Model->Nodes.Num(), n );
	Model->Nodes.Remove( n, Model->Nodes.Num()-n  );

	// Update Bsp nodes.
	for( i=0; i<Model->Nodes.Num(); i++ )
	{
		FBspNode *Node = &Model->Nodes(i);
		Node->iSurf = PolyRef[Node->iSurf];
		if (Node->iFront != INDEX_NONE) Node->iFront = NodeRef[Node->iFront];
		if (Node->iBack  != INDEX_NONE) Node->iBack  = NodeRef[Node->iBack];
		if (Node->iPlane != INDEX_NONE) Node->iPlane = NodeRef[Node->iPlane];
	}

	// Update portal list.
	for(INT PortalIndex = 0;PortalIndex < Model->PortalNodes.Num();PortalIndex++)
	{
		Model->PortalNodes(PortalIndex) = NodeRef[Model->PortalNodes(PortalIndex)];
	}

	// Remove unreferenced points and vectors.
	VectorRef = new(GMainThreadMemStack,MEM_Oned,Model->Vectors.Num())INT;
	PointRef  = new(GMainThreadMemStack,MEM_Oned,Model->Points .Num ())INT;

	// Check Bsp surfs.
	for( i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf = &Model->Surfs(i);
		VectorRef [Surf->vNormal   ] = 0;
		VectorRef [Surf->vTextureU ] = 0;
		VectorRef [Surf->vTextureV ] = 0;
		PointRef  [Surf->pBase     ] = 0;
	}

	// Check Bsp nodes.
	for( i=0; i<Model->Nodes.Num(); i++ )
	{
		// Tag all points used by nodes.
		FBspNode*	Node		= &Model->Nodes(i);
		FVert*		VertPool	= &Model->Verts(Node->iVertPool);
		for( B=0; B<Node->NumVertices;  B++ )
		{
			PointRef[VertPool->pVertex] = 0;
			VertPool++;
		}
		Node++;
	}

	// Remap points.
	n=0; 
	for( i=0; i<Model->Points.Num(); i++ ) if( PointRef[i]!=INDEX_NONE )
	{
		Model->Points(n) = Model->Points(i);
		PointRef[i] = n++;
	}
	//debugf( NAME_Log, TEXT("Points: %i -> %i"), Model->Points.Num(), n );
	Model->Points.Remove( n, Model->Points.Num()-n );
	check(Model->Points.Num()==n);

	// Remap vectors.
	n=0; for (i=0; i<Model->Vectors.Num(); i++) if (VectorRef[i]!=INDEX_NONE)
	{
		Model->Vectors(n) = Model->Vectors(i);
		VectorRef[i] = n++;
	}
	//debugf( NAME_Log, TEXT("Vectors: %i -> %i"), Model->Vectors.Num(), n );
	Model->Vectors.Remove( n, Model->Vectors.Num()-n );

	// Update Bsp surfs.
	for( i=0; i<Model->Surfs.Num(); i++ )
	{
		FBspSurf *Surf	= &Model->Surfs(i);
		Surf->vNormal   = VectorRef [Surf->vNormal  ];
		Surf->vTextureU = VectorRef [Surf->vTextureU];
		Surf->vTextureV = VectorRef [Surf->vTextureV];
		Surf->pBase     = PointRef  [Surf->pBase    ];
	}

	// Update Bsp nodes.
	for( i=0; i<Model->Nodes.Num(); i++ )
	{
		FBspNode*	Node		= &Model->Nodes(i);
		FVert*		VertPool	= &Model->Verts(Node->iVertPool);
		for( B=0; B<Node->NumVertices;  B++ )
		{			
			VertPool->pVertex = PointRef [VertPool->pVertex];
			VertPool++;
		}

		Node++;
	}

	// Shrink the objects.
	Model->ShrinkModel();

	Mark.Pop();
}

// Build bounding volumes for all Bsp nodes.  The bounding volume of the node
// completely encloses the "outside" space occupied by the nodes.  Note that 
// this is not the same as representing the bounding volume of all of the 
// polygons within the node.
//
// We start with a practically-infinite cube and filter it down the Bsp,
// whittling it away until all of its convex volume fragments land in leaves.
void FBSPOps::bspBuildBounds( UModel* Model )
{
	if( Model->Nodes.Num()==0 )
		return;

	BuildZoneMasks( Model, 0 );

	FPoly Polys[6], *PolyList[6];
	for( int i=0; i<6; i++ )
	{
		PolyList[i] = &Polys[i];
		PolyList[i]->Init();
		PolyList[i]->iBrushPoly = INDEX_NONE;
	}

	new(Polys[0].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,HALF_WORLD_MAX);
	new(Polys[0].Vertices)FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,HALF_WORLD_MAX);
	new(Polys[0].Vertices)FVector( HALF_WORLD_MAX, HALF_WORLD_MAX,HALF_WORLD_MAX);
	new(Polys[0].Vertices)FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,HALF_WORLD_MAX);
	Polys[0].Normal   =FVector( 0.000000,  0.000000,  1.000000 );
	Polys[0].Base     =Polys[0].Vertices(0);

	new(Polys[1].Vertices)FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
	new(Polys[1].Vertices)FVector( HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
	new(Polys[1].Vertices)FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	new(Polys[1].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[1].Normal   =FVector( 0.000000,  0.000000, -1.000000 );
	Polys[1].Base     =Polys[1].Vertices(0);

	new(Polys[2].Vertices)FVector(-HALF_WORLD_MAX,HALF_WORLD_MAX,-HALF_WORLD_MAX);
	new(Polys[2].Vertices)FVector(-HALF_WORLD_MAX,HALF_WORLD_MAX, HALF_WORLD_MAX);
	new(Polys[2].Vertices)FVector( HALF_WORLD_MAX,HALF_WORLD_MAX, HALF_WORLD_MAX);
	new(Polys[2].Vertices)FVector( HALF_WORLD_MAX,HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[2].Normal   =FVector( 0.000000,  1.000000,  0.000000 );
	Polys[2].Base     =Polys[2].Vertices(0);

	new(Polys[3].Vertices)FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	new(Polys[3].Vertices)FVector( HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
	new(Polys[3].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
	new(Polys[3].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[3].Normal   =FVector( 0.000000, -1.000000,  0.000000 );
	Polys[3].Base     =Polys[3].Vertices(0);

	new(Polys[4].Vertices)FVector(HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
	new(Polys[4].Vertices)FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
	new(Polys[4].Vertices)FVector(HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
	new(Polys[4].Vertices)FVector(HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[4].Normal   =FVector( 1.000000,  0.000000,  0.000000 );
	Polys[4].Base     =Polys[4].Vertices(0);

	new(Polys[5].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX);
	new(Polys[5].Vertices)FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX, HALF_WORLD_MAX);
	new(Polys[5].Vertices)FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
	new(Polys[5].Vertices)FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX,-HALF_WORLD_MAX);
	Polys[5].Normal   =FVector(-1.000000,  0.000000,  0.000000 );
	Polys[5].Base     =Polys[5].Vertices(0);
	// Empty hulls.
	Model->LeafHulls.Empty();
	for( int i=0; i<Model->Nodes.Num(); i++ )
		Model->Nodes(i).iCollisionBound  = INDEX_NONE;
	FilterBound( Model, NULL, 0, PolyList, 6, Model->RootOutside );
//	debugf( NAME_Log, TEXT("bspBuildBounds: Generated %i hulls"), Model->LeafHulls.Num() );
}

/**
 * Validate a brush, and set iLinks on all EdPolys to index of the
 * first identical EdPoly in the list, or its index if it's the first.
 * Not transactional.
 */
void FBSPOps::bspValidateBrush( UModel* Brush, UBOOL ForceValidate, UBOOL DoStatusUpdate )
{
	Brush->Modify();
	if( ForceValidate || !Brush->Linked )
	{
		Brush->Linked = 1;
		for( INT i=0; i<Brush->Polys->Element.Num(); i++ )
		{
			Brush->Polys->Element(i).iLink = i;
		}
		INT n=0;
		for( INT i=0; i<Brush->Polys->Element.Num(); i++ )
		{
			FPoly* EdPoly = &Brush->Polys->Element(i);
			if( EdPoly->iLink==i )
			{
				for( INT j=i+1; j<Brush->Polys->Element.Num(); j++ )
				{
					FPoly* OtherPoly = &Brush->Polys->Element(j);
					if
					(	OtherPoly->iLink == j
					&&	OtherPoly->Material == EdPoly->Material
					&&	OtherPoly->TextureU == EdPoly->TextureU
					&&	OtherPoly->TextureV == EdPoly->TextureV
					&&	OtherPoly->PolyFlags == EdPoly->PolyFlags
					&&	(OtherPoly->Normal | EdPoly->Normal)>0.9999 )
					{
						FLOAT Dist = FPointPlaneDist( OtherPoly->Vertices(0), EdPoly->Vertices(0), EdPoly->Normal );
						if( Dist>-0.001 && Dist<0.001 )
						{
							OtherPoly->iLink = i;
							n++;
						}
					}
				}
			}
		}
// 		debugf( NAME_Log, TEXT("BspValidateBrush linked %i of %i polys"), n, Brush->Polys->Element.Num() );
	}

	// Build bounds.
	Brush->BuildBound();
}

void FBSPOps::bspUnlinkPolys( UModel* Brush )
{
	Brush->Modify();
	Brush->Linked = 1;
	for( INT i=0; i<Brush->Polys->Element.Num(); i++ )
	{
		Brush->Polys->Element(i).iLink = i;
	}
}

// Add an editor polygon to the Bsp, and also stick a reference to it
// in the editor polygon's BspNodes list. If the editor polygon has more sides
// than the Bsp will allow, split it up into several sub-polygons.
//
// Returns: Index to newly-created node of Bsp.  If several nodes were created because
// of split polys, returns the parent (highest one up in the Bsp).
INT	FBSPOps::bspAddNode( UModel* Model, INT iParent, ENodePlace NodePlace, DWORD NodeFlags, FPoly* EdPoly )
{
	if( NodePlace == NODE_Plane )
	{
		// Make sure coplanars are added at the end of the coplanar list so that 
		// we don't insert NF_IsNew nodes with non NF_IsNew coplanar children.
		while( Model->Nodes(iParent).iPlane != INDEX_NONE )
		{
			iParent = Model->Nodes(iParent).iPlane;
		}
	}
	FBspSurf* Surf = NULL;
	if( EdPoly->iLink == Model->Surfs.Num() )
	{
		INT NewIndex = Model->Surfs.AddZeroed();
		Surf = &Model->Surfs(NewIndex);

		// This node has a new polygon being added by bspBrushCSG; must set its properties here.
		Surf->pBase     	= bspAddPoint  (Model,&EdPoly->Base,1);
		Surf->vNormal   	= bspAddVector (Model,&EdPoly->Normal,1);
		Surf->vTextureU 	= bspAddVector (Model,&EdPoly->TextureU,0);
		Surf->vTextureV 	= bspAddVector (Model,&EdPoly->TextureV,0);
		Surf->Material  	= EdPoly->Material;
		Surf->Actor			= NULL;

		Surf->PolyFlags 	= EdPoly->PolyFlags & ~PF_NoAddToBSP;
		Surf->ShadowMapScale= EdPoly->ShadowMapScale;
		Surf->LightingChannels.Bitfield = EdPoly->LightingChannels;

//		if (EdPoly->LightingChannels)
		// Find the LightmassPrimitiveSettings in the UModel...
		INT FoundLightmassIndex = INDEX_NONE;
		if (Model->LightmassSettings.FindItem(EdPoly->LightmassSettings, FoundLightmassIndex) == FALSE)
		{
			FoundLightmassIndex = Model->LightmassSettings.AddItem(EdPoly->LightmassSettings);
		}
		Surf->iLightmassIndex = FoundLightmassIndex;

		Surf->Actor	 		= EdPoly->Actor;
		Surf->iBrushPoly	= EdPoly->iBrushPoly;

		Surf->Plane			= FPlane(EdPoly->Vertices(0),EdPoly->Normal);
	}
	else
	{
		check(EdPoly->iLink!=INDEX_NONE);
		check(EdPoly->iLink<Model->Surfs.Num());
		Surf = &Model->Surfs(EdPoly->iLink);
	}

	// Set NodeFlags.
	if( Surf->PolyFlags & PF_NotSolid              ) NodeFlags |= NF_NotCsg;
	if( Surf->PolyFlags & (PF_Invisible|PF_Portal) ) NodeFlags |= NF_NotVisBlocking;

	if( EdPoly->Vertices.Num() > FBspNode::MAX_NODE_VERTICES )
	{
		// Split up into two coplanar sub-polygons (one with MAX_NODE_VERTICES vertices and
		// one with all the remaining vertices) and recursively add them.

		// EdPoly1 is just the first MAX_NODE_VERTICES from EdPoly.
		FMemMark Mark(GMainThreadMemStack);
		FPoly *EdPoly1 = new FPoly;
		*EdPoly1 = *EdPoly;
		EdPoly1->Vertices.Remove(FBspNode::MAX_NODE_VERTICES,EdPoly->Vertices.Num() - FBspNode::MAX_NODE_VERTICES);

		// EdPoly2 is the first vertex from EdPoly, and the last EdPoly->Vertices.Num() - MAX_NODE_VERTICES + 1.
		FPoly *EdPoly2 = new FPoly;
		*EdPoly2 = *EdPoly;
		EdPoly2->Vertices.Remove(1,FBspNode::MAX_NODE_VERTICES - 2);

		INT iNode = bspAddNode( Model, iParent, NodePlace, NodeFlags, EdPoly1 ); // Add this poly first.
		bspAddNode( Model, iNode,   NODE_Plane, NodeFlags, EdPoly2 ); // Then add other (may be bigger).

		delete EdPoly1;
		delete EdPoly2;

		Mark.Pop();
		return iNode; // Return coplanar "parent" node (not coplanar child)
	}
	else
	{
		// Add node.
		if( NodePlace!=NODE_Root )
		{
			Model->Nodes.ModifyItem( iParent );
		}
		INT iNode			 = Model->Nodes.AddZeroed();
		FBspNode& Node       = Model->Nodes(iNode);

		// Tell transaction tracking system that parent is about to be modified.
		FBspNode* Parent=NULL;
		if( NodePlace!=NODE_Root )
			Parent = &Model->Nodes(iParent);

		// Set node properties.
		Node.iSurf       	 = EdPoly->iLink;
		Node.NodeFlags   	 = NodeFlags;
		Node.iCollisionBound = INDEX_NONE;
		Node.Plane           = FPlane( EdPoly->Vertices(0), EdPoly->Normal );
		Node.iVertPool       = Model->Verts.Add(EdPoly->Vertices.Num());
		Node.iFront		     = INDEX_NONE;
		Node.iBack		     = INDEX_NONE;
		Node.iPlane		     = INDEX_NONE;
		if( NodePlace==NODE_Root )
		{
			Node.iLeaf[0]	 = INDEX_NONE;
			Node.iLeaf[1] 	 = INDEX_NONE;
			Node.iZone[0]	 = 0;
			Node.iZone[1]	 = 0;
		}
		else if( NodePlace==NODE_Front || NodePlace==NODE_Back )
		{
			INT ZoneFront=NodePlace==NODE_Front;
			Node.iLeaf[0]	 = Parent->iLeaf[ZoneFront];
			Node.iLeaf[1] 	 = Parent->iLeaf[ZoneFront];
			Node.iZone[0]	 = Parent->iZone[ZoneFront];
			Node.iZone[1]	 = Parent->iZone[ZoneFront];
		}
		else
		{
			INT IsFlipped    = (Node.Plane|Parent->Plane)<0.0;
			Node.iLeaf[0]    = Parent->iLeaf[IsFlipped  ];
			Node.iLeaf[1]    = Parent->iLeaf[1-IsFlipped];
			Node.iZone[0]    = Parent->iZone[IsFlipped  ];
			Node.iZone[1]    = Parent->iZone[1-IsFlipped];
		}

		// Link parent to this node.
		if     ( NodePlace==NODE_Front ) Parent->iFront = iNode;
		else if( NodePlace==NODE_Back  ) Parent->iBack  = iNode;
		else if( NodePlace==NODE_Plane ) Parent->iPlane = iNode;

		// Add all points to point table, merging nearly-overlapping polygon points
		// with other points in the poly to prevent criscrossing vertices from
		// being generated.

		// Must maintain Node->NumVertices on the fly so that bspAddPoint is always
		// called with the Bsp in a clean state.
		Node.NumVertices = 0;
		FVert* VertPool	 = &Model->Verts( Node.iVertPool );
		for( BYTE i=0; i<EdPoly->Vertices.Num(); i++ )
		{
			int pVertex = bspAddPoint(Model,&EdPoly->Vertices(i),0);
			if( Node.NumVertices==0 || VertPool[Node.NumVertices-1].pVertex!=pVertex )
			{
				VertPool[Node.NumVertices].iSide   = INDEX_NONE;
				VertPool[Node.NumVertices].pVertex = pVertex;
				Node.NumVertices++;
			}
		}
		if( Node.NumVertices>=2 && VertPool[0].pVertex==VertPool[Node.NumVertices-1].pVertex )
		{
			Node.NumVertices--;
		}
		if( Node.NumVertices < 3 )
		{
			GErrors++;
// 			debugf( NAME_Warning, TEXT("bspAddNode: Infinitesimal polygon %i (%i)"), Node.NumVertices, EdPoly->Vertices.Num() );
			Node.NumVertices = 0;
		}

		return iNode;
	}
}

/**
 * Rebuild some brush internals
 */
void FBSPOps::RebuildBrush(UModel* Brush)
{
	Brush->ClearFlags(RF_NotForClient | RF_NotForServer);
	Brush->EmptyModel(1, 0);

	// Build bounding box.
	Brush->BuildBound();

	// Build BSP for the brush.
	bspBuild(Brush, BSP_Good, 15, 70, 1, 0);
	bspRefresh(Brush, 1);
	bspBuildBounds(Brush);
}

/**
 * Rotates the specified brush's vertices.
 */
void FBSPOps::RotateBrushVerts(ABrush* Brush, const FRotator& Rotation, UBOOL bClearComponents)
{
	if(Brush->BrushComponent->Brush && Brush->BrushComponent->Brush->Polys)
	{
		for( INT poly = 0 ; poly < Brush->BrushComponent->Brush->Polys->Element.Num() ; poly++ )
		{
			FPoly* Poly = &(Brush->BrushComponent->Brush->Polys->Element(poly));

			// Rotate the vertices.
			for( INT vertex = 0 ; vertex < Poly->Vertices.Num() ; vertex++ )
			{
				Poly->Vertices(vertex) = Brush->PrePivot + FRotationMatrix( Rotation ).TransformNormal( Poly->Vertices(vertex) - Brush->PrePivot );
			}
			Poly->Base = Brush->PrePivot + FRotationMatrix( Rotation ).TransformNormal( Poly->Base - Brush->PrePivot );

			// Rotate the texture vectors.
			Poly->TextureU = FRotationMatrix( Rotation ).TransformNormal( Poly->TextureU );
			Poly->TextureV = FRotationMatrix( Rotation ).TransformNormal( Poly->TextureV );

			// Recalc the normal for the poly.
			Poly->Normal = FVector(0,0,0);
			Poly->Finalize(Brush,0);
		}

		Brush->BrushComponent->Brush->BuildBound();

		if( !Brush->IsStaticBrush() )
		{
			csgPrepMovingBrush( Brush );
		}

		if ( bClearComponents )
		{
			Brush->ClearComponents();
		}
	}
}


#else

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
INT BSPOpsLinkerHelper;

#endif // !CONSOLE

void FBSPOps::SubdividePolys(TArray<TempPoly>* nodes)
{
	TArray<TempPoly> orig = TArray<TempPoly>(*nodes);
	INT num = orig.Num();
	// debugf(TEXT("  Subdividing static mesh with %d polys"), num);
	for (INT i = 0; i < num; ++i)
	{
		TempPoly node = orig(i);
		SubdividePoly(nodes, &node, TRUE);
		// debugf(TEXT("  Done: %d -> %d"), num, nodes->Num());
	}
}

void FBSPOps::SubdividePoly(TArray<TempPoly>* nodes, TempPoly* node, UBOOL added)
{
	// debugf(TEXT("    Subdividing poly with %d verts"), node->Vertices.Num());
	if (TooBig(node) && nodes->Num() < 5000) 
	{
		INT ind = -1;
		if (added) 
		{
			INT ind = nodes->FindItemIndex(*node);
			nodes->Remove(ind);
		}
		TempPoly FrontPoly, BackPoly;
		switch(node->SplitWithPlane(BigMidPoint(node),BigNormal(node),&FrontPoly,&BackPoly,0))
		{
		case SP_Split:
			if (ind != -1)
			{
				nodes->Remove(ind);
			}
			SubdividePoly(nodes, &FrontPoly, FALSE);
			SubdividePoly(nodes, &BackPoly, FALSE);
			break;

		default:
			debugf(TEXT("REALD : Odd big poly"));
		}
	}
	else if (!added) 
	{
		nodes->AddItem(*node);
	}
}

UBOOL FBSPOps::TooBig(TempPoly* node) 
{
	if (node->Vertices.Num() == 0)
	{
		return FALSE;
	}

	INT max = 0;
	INT size = node->Vertices.Num();
	for (INT i = 0; i < size - 1; ++i)
	{
		if ((node->Vertices(i) - node->Vertices(i + 1)).SizeSquared() > 10000)
		{
			// debugf(TEXT("      l %f"), (node->Vertices(i) - node->Vertices(i + 1)).Size());
			return TRUE;
		}
	}
	return (node->Vertices(0) - node->Vertices(size - 1)).SizeSquared() > 10000;
}

FVector FBSPOps::BigMidPoint(TempPoly* node) 
{
	INT size = node->Vertices.Num();
	for (INT i = 0; i < size - 1; ++i)
	{
		if ((node->Vertices(i) - node->Vertices(i + 1)).SizeSquared() > 10000)
		{
			return (node->Vertices(i) + node->Vertices(i + 1)) / 2;
		}
	}
	return (node->Vertices(0) + node->Vertices(size - 1)) / 2;
}

FVector FBSPOps::BigNormal(TempPoly* node) 
{
	INT size = node->Vertices.Num();
	for (INT i = 0; i < size - 1; ++i)
	{
		if ((node->Vertices(i) - node->Vertices(i + 1)).SizeSquared() > 10000)
		{
			FVector res = (node->Vertices(i + 1) - node->Vertices(i));
			res.Normalize();
			return res;
		}
	}
	FVector res = (node->Vertices(size - 1) - node->Vertices(0));
	res.Normalize();
	return res;
}

NodePolys* NodePolys::create(UModel* TheModel, FBspNode* Node) 
{
	TempPoly tempPoly;
	for (INT VertexIndex = 0; VertexIndex < Node->NumVertices; VertexIndex++)
	{
		const FVert& Vert = TheModel->Verts(Node->iVertPool + VertexIndex);
		const FVector& pt = TheModel->Points(Vert.pVertex);
		tempPoly.Vertices.AddItem(pt);
		tempPoly.ShadowTexCoords.AddItem(Vert.ShadowTexCoord);
	}

	TArray<TempPoly> tempPolys;
	tempPolys.AddItem(tempPoly);
#if WITH_REALD && !CONSOLE
	if ((GIsGame || GIsCooking) && !GIsSimMobile)
	{
		FBSPOps::SubdividePolys(&tempPolys);
	}
#endif

	NodePolys* Polys = new NodePolys();
	for (INT PolyIndex = 0; PolyIndex < tempPolys.Num(); ++PolyIndex)
	{
		TempPoly tPoly = tempPolys(PolyIndex);
		IndexPoly iPoly;

		for (INT VIndex = 0; VIndex < tPoly.Vertices.Num(); ++VIndex)
		{
			FVector& vtx = tPoly.Vertices(VIndex);
			FVector2D& shtc = tPoly.ShadowTexCoords(VIndex);

			iPoly.Indices.AddItem(Polys->Vertices.Num());
			Polys->Vertices.AddItem(vtx);
			Polys->ShadowTexCoords.AddItem(shtc);
		}

		Polys->Polys.AddItem(iPoly);
	}

	return Polys;
}

/**
* Split with plane. Meant to be numerically stable.
*/
INT TempPoly::SplitWithPlane
	(
	const FVector& PlaneBase,
	const FVector& PlaneNormal,
	TempPoly* FrontPoly,
	TempPoly* BackPoly,
	INT VeryPrecise
	) const
{
	FVector Intersection;
	FVector PolyCenter(0.0f);

	FLOAT Dist = 0.0f;
	FLOAT MaxDist = 0.0f;
	FLOAT MinDist = 0.0f;
	FLOAT PrevDist,Thresh;
	enum {V_FRONT,V_BACK,V_EITHER} Status,PrevStatus=V_EITHER;
	INT i,j;

	if (VeryPrecise)
	{
		Thresh = THRESH_SPLIT_POLY_PRECISELY;   
	}
	else
	{
		Thresh = THRESH_SPLIT_POLY_WITH_PLANE;
	}

	// Find number of vertices.
	check(Vertices.Num()>=3);

	// See if the polygon is split by SplitPoly, or it's on either side, or the
	// polys are coplanar.  Go through all of the polygon points and
	// calculate the minimum and maximum signed distance (in the direction
	// of the normal) from each point to the plane of SplitPoly.
	for (i = 0; i < Vertices.Num(); i++)
	{
		Dist = FPointPlaneDist(Vertices(i), PlaneBase, PlaneNormal);

		if (i==0 || Dist>MaxDist)
		{
			MaxDist=Dist;
		}
		if (i==0 || Dist<MinDist)
		{
			MinDist=Dist;
		}

		if (Dist > +Thresh)
		{
			PrevStatus = V_FRONT;
		}
		else if (Dist < -Thresh)
		{
			PrevStatus = V_BACK;
		}

		PolyCenter += Vertices(i);
	}
	if (MaxDist<Thresh && MinDist>-Thresh)
	{
		return SP_Coplanar;
	}
	else if (MaxDist < Thresh)
	{
		return SP_Back;
	}
	else if (MinDist > -Thresh)
	{
		return SP_Front;
	}
	else
	{
		// Split.
		if (FrontPoly==NULL)
		{
			return SP_Split; // Caller only wanted status.
		}

		PolyCenter /= Vertices.Num();

		*FrontPoly = *this;
		FrontPoly->Vertices.Empty();
		FrontPoly->ShadowTexCoords.Empty();

		*BackPoly = *this;
		BackPoly->Vertices.Empty();
		BackPoly->ShadowTexCoords.Empty();

		j = Vertices.Num()-1; // Previous vertex; have PrevStatus already.

		for (i=0; i<Vertices.Num(); i++)
		{
			PrevDist    = Dist;
			Dist        = FPointPlaneDist(Vertices(i), PlaneBase, PlaneNormal);

			if (Dist > +Thresh)
			{
				Status = V_FRONT;
			}
			else if (Dist < -Thresh)
			{
				Status = V_BACK;
			}
			else
			{
				Status = PrevStatus;
			}

			if (Status != PrevStatus)
			{
				// Crossing.  Either Front-to-Back or Back-To-Front.
				// Intersection point is naturally on both front and back polys.
				if ((Dist >= -Thresh) && (Dist < +Thresh))
				{
					// This point lies on plane.
					if (PrevStatus == V_FRONT)
					{
						new(FrontPoly->Vertices) FVector(Vertices(i));
						new(FrontPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
						new(BackPoly->Vertices) FVector(Vertices(i));
						new(BackPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
					}
					else
					{
						new(BackPoly->Vertices) FVector(Vertices(i));
						new(BackPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
						new(FrontPoly->Vertices) FVector(Vertices(i));
						new(FrontPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
					}
				}
				else if ((PrevDist >= -Thresh) && (PrevDist < +Thresh))
				{
					// Previous point lies on plane.
					if (Status == V_FRONT)
					{
						new(FrontPoly->Vertices) FVector(Vertices(j));
						new(FrontPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(j));
						new(FrontPoly->Vertices) FVector(Vertices(i));
						new(FrontPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
					}
					else
					{
						new(BackPoly->Vertices) FVector(Vertices(j));
						new(BackPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(j));
						new(BackPoly->Vertices) FVector(Vertices(i));
						new(BackPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
					}
				}
				else
				{
					// Intersection point is in between.
					Intersection = FLinePlaneIntersection(Vertices(j), Vertices(i), PlaneBase, PlaneNormal);
					Intersection += (Intersection - PolyCenter).SafeNormal() * 0.05f;
					FVector2D ShadInter = FLinePlaneIntersectionShadow(
						Vertices(j), Vertices(i), ShadowTexCoords(j), ShadowTexCoords(i), PlaneBase, PlaneNormal);

					if (PrevStatus == V_FRONT)
					{
						new(FrontPoly->Vertices) FVector(Intersection);
						new(FrontPoly->ShadowTexCoords) FVector2D(ShadInter);
						new(BackPoly->Vertices) FVector(Intersection);
						new(BackPoly->ShadowTexCoords) FVector2D(ShadInter);
						new(BackPoly->Vertices) FVector(Vertices(i));
						new(BackPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
					}
					else
					{
						new(BackPoly->Vertices) FVector(Intersection);
						new(BackPoly->ShadowTexCoords) FVector2D(ShadInter);
						new(FrontPoly->Vertices) FVector(Intersection);
						new(FrontPoly->ShadowTexCoords) FVector2D(ShadInter);
						new(FrontPoly->Vertices) FVector(Vertices(i));
						new(FrontPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
					}
				}
			}
			else
			{
				if (Status==V_FRONT) 
				{
					new(FrontPoly->Vertices) FVector(Vertices(i));
					new(FrontPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
				}
				else 
				{
					new(BackPoly->Vertices)FVector(Vertices(i));
					new(BackPoly->ShadowTexCoords) FVector2D(ShadowTexCoords(i));
				}
			}
			j          = i;
			PrevStatus = Status;
		}

		// Handle possibility of sliver polys due to precision errors.
		if (FrontPoly->Fix()<3)
		{
			//             debugf(NAME_Warning, TEXT("FPoly::SplitWithPlane: Ignored front sliver"));
			return SP_Back;
		}
		else if (BackPoly->Fix()<3)
		{
			//             debugf(NAME_Warning, TEXT("FPoly::SplitWithPlane: Ignored back sliver"));
			return SP_Front;
		}
		else return SP_Split;
	}
}

INT TempPoly::Fix()
{
	INT i,j,prev;

	j=0;
	prev=Vertices.Num()-1;
	for (i=0; i<Vertices.Num(); i++)
	{
		if (!FPointsAreSame(Vertices(i), Vertices(prev)))
		{
			if (j != i) 
			{
				Vertices(j) = Vertices(i);
				ShadowTexCoords(j) = ShadowTexCoords(i);
			}
			prev = j;
			j    ++;
		}
		//else debugf(NAME_Warning, TEXT("FPoly::Fix: Collapsed a point"));
	}

	if(j < 3)
	{
		Vertices.Empty();
		ShadowTexCoords.Empty();
	}
	else if(j < Vertices.Num())
	{
		Vertices.Remove(j,Vertices.Num() - j);
		ShadowTexCoords.Remove(j,ShadowTexCoords.Num() - j);
	}
	return Vertices.Num();
}
