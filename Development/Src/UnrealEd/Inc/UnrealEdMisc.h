/*=============================================================================
	UnrealEdMisc.h: Misc UnrealEd helper functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UnrealEdMisc_h__
#define __UnrealEdMisc_h__

#ifdef _MSC_VER
	#pragma once
#endif

/*-----------------------------------------------------------------------------
	Not sure this is the best place for this yet.
-----------------------------------------------------------------------------*/

enum ELastDir
{
	LD_UNR		= 0,
	LD_BRUSH,
	LD_2DS,
	LD_PSA,
#if WITH_FBX
	LD_FBX_ANIM,
#endif // WITH_FBX
	LD_GENERIC_IMPORT,
	LD_GENERIC_EXPORT,
	LD_GENERIC_OPEN,
	LD_GENERIC_SAVE,
	LD_MESH_IMPORT_EXPORT,

	NUM_LAST_DIRECTORIES
};

#define SAFEDELETENULL(a)\
{\
  if( a )\
  {\
      DestroyWindow(*a);\
      a = NULL;\
  }\
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FPolyBreaker
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// @todo DB: Relocate; this is only used by the 2D shape editor.

/**
* Breaks a list of vertices into a set of convex FPolys.  The only requirement
* is the vertices are wound in edge order ... so that each vertex connects to the next.
* It can't be a random pool of vertices.  The winding direction doesn't matter.
*/
class FPolyBreaker
{
public:
	TArray<FVector>* PolyVerts;
	FVector PolyNormal;
	/** The resulting polygons. */
	TArray<FPoly> FinalPolys;

	FPolyBreaker();
	~FPolyBreaker();

	void Process( TArray<FVector>* InPolyVerts, FVector InPolyNormal );
	UBOOL IsPolyConvex( FPoly* InPoly );
	UBOOL IsPolyConvex( TArray<FVector>* InVerts );
	void MakeConvexPoly( TArray<FVector>* InVerts );
	INT TryToMerge( FPoly *Poly1, FPoly *Poly2 );

	/**
	* Looks at the resulting polygons and tries to put polys with matching edges
	* together.  This reduces the total number of polys in the final shape.
	*/
	void Optimize();

	/** Returns 1 if any polys were merged. */
	UBOOL OptimizeList( TArray<FPoly>* PolyList );

	/**
	* This is basically the same function as FPoly::SplitWithPlane, but modified
	* to work with this classes data structures.
	*/
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




/**
 * EGADCollection::Type.  Types of collections that the Game Asset Database supports.  Used by the Content
 * Browser and various editor commandlets.
 */
namespace EGADCollection
{
	enum Type
	{
		/** Shared collection */
		Shared,

		/** Private collection */
		Private,

		/** Local unsaved collection */
		Local,
	};
}

extern void OpenMaterialInMaterialEditor(UMaterial* MaterialToEdit);
extern void OpenMaterialFunctionInMaterialEditor(UMaterialFunction* MaterialFunctionToEdit);

#endif	// __UnrealEdMisc_h__

