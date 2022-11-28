/*=============================================================================
	UnrealEdMisc.h: Misc UnrealEd helper functions.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

    Revision history:
		* Created by Jack Porter.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Not sure this is the best place for this yet.
-----------------------------------------------------------------------------*/

enum ELastDir
{
	LD_UNR		= 0,
	LD_BRUSH,
	LD_2DS,
	LD_PSA,
	LD_GENERIC,
	LD_MAX
};

#define SAFEDELETENULL(a)\
{\
  if( a )\
  {\
      DestroyWindow(*a);\
      a = NULL;\
  }\
}

/*-----------------------------------------------------------------------------
	FPolyBreaker.
-----------------------------------------------------------------------------*/

//
// Breaks a list of vertices into a set of convex FPolys.  The only requirement
// is the vertices are wound in edge order ... so that each vertex connects to the next.
// It can't be a random pool of vertices.  The winding direction doesn't matter.
//
class FPolyBreaker
{
public:
	TArray<FVector>* PolyVerts;
	FVector PolyNormal;
	TArray<FPoly> FinalPolys;	// The resulting polygons.

	// tor's
	FPolyBreaker();
	~FPolyBreaker();

	// FPolyBreaker interface
	void Process( TArray<FVector>* InPolyVerts, FVector InPolyNormal );
	UBOOL IsPolyConvex( FPoly* InPoly );
	UBOOL IsPolyConvex( TArray<FVector>* InVerts );
	void MakeConvexPoly( TArray<FVector>* InVerts );
	INT TryToMerge( FPoly *Poly1, FPoly *Poly2 );
	// Looks at the resulting polygons and tries to put polys with matching edges
	// together.  This reduces the total number of polys in the final shape.
	void Optimize();
	// Returns 1 if any polys were merged
	UBOOL OptimizeList( TArray<FPoly>* PolyList );
	// This is basically the same function as FPoly::SplitWithPlane, but modified
	// to work with this classes data structures.
	INT SplitWithPlane
	(
		TArray<FVector>			*Vertex,
		int						NumVertices,
		const FVector			&PlaneBase,
		const FVector			&PlaneNormal,
		TArray<FVector>			*FrontPoly,
		TArray<FVector>			*BackPoly
	) const;
};

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/
