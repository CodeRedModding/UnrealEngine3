/*=============================================================================
	UnTerrainBVTree.h: bounding-volume tree for terrain
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnCollision.h"

#ifndef _TERRAINBVTREE_H
#define _TERRAINBVTREE_H

struct FTerrainBVTree;
struct FTerrainBVNode;

/**
 * Struct indicating one subregion of a terrain.
 */
struct FTerrainSubRegion
{
	WORD	XPos;
	WORD	YPos;
	WORD	XSize;
	WORD	YSize;

	// Serialization
	friend FArchive& operator<<(FArchive& Ar, FTerrainSubRegion& R)
	{
		Ar << R.XPos << R.YPos << R.XSize << R.YSize;
		return Ar;
	}
};

/**
* Base struct for all collision checks. 
*/
struct FTerrainBVTreeCollisionCheck
{
	/** Terrain component the tree is for. */
	const class UTerrainComponent* TerrainComp;
	/** Cached world to local. */
	FMatrix WorldToLocal;
	/** Cached local to world. */
	FMatrix LocalToWorld;
	/** The BV tree */
	const FTerrainBVTree& BVTree;
	/** The array of the nodes for the BV tree */
	const TArray<FTerrainBVNode>& Nodes;
	/** Hide the default ctor */
	FTerrainBVTreeCollisionCheck(const UTerrainComponent* InTerrainComp);
};

/**
* This struct holds the information used to do a line check against the BV tree.
*/
struct FTerrainBVTreeLineCollisionCheck : public FTerrainBVTreeCollisionCheck
{
	/** Where the collision results get stored */
	FCheckResult* Result;
	// Constant input vars
	const FVector& Start;
	const FVector& End;
	/** Flags for optimizing a trace */
	const DWORD TraceFlags;
	// Locally calculated vectors
	FVector LocalStart;
	FVector LocalEnd;
	FVector LocalDir;
	FVector LocalOneOverDir;

	/** Order to walk . */
	INT NodeCheckOrder[4];

	// Normal in local space which gets transformed to world at the very end
	FVector LocalHitNormal;

	/**
	* Sets up the FkDOPLineCollisionCheck structure for performing line checks
	* against a kDOPTree. Initializes all of the variables that are used
	* throughout the line check.
	*
	* @param InStart -- The starting point of the trace
	* @param InEnd -- The ending point of the trace
	* @param InTraceFlags -- The trace flags that might modify searches
	* @param InTerrainComp --The terrain component
	* @param InResult -- The out param for hit result information
	*/
	FTerrainBVTreeLineCollisionCheck(const FVector& InStart,const FVector& InEnd, DWORD InTraceFlags, const UTerrainComponent* InTerrainComp, FCheckResult* InResult);

	/**
	 * Transforms the local hit normal into a world space normal using the transpose
	 * adjoint and flips the normal if need be.
	 */
	FVector GetHitNormal(void) const;
};

/**
* This struct holds the information used to do a box and/or point check
* against the terrain BV tree. It is used purely to gather multiple function
* parameters into a single structure for smaller stack overhead.
*/
struct FTerrainBVTreeBoxCollisionCheck : public FTerrainBVTreeLineCollisionCheck
{
	// Constant input vars
	const FVector& Extent;
	// Calculated vars
	FVector LocalExtent;
	FVector LocalBoxX;
	FVector LocalBoxY;
	FVector LocalBoxZ;

	/**
	* Sets up the FTerrainBVTreeBoxCollisionCheck structure for performing swept box checks
	* against a kDOPTree. Initializes all of the variables that are used
	* throughout the check.
	*
	* @param InStart -- The starting point of the trace
	* @param InEnd -- The ending point of the trace
	* @param InExtent -- The extent to check
	* @param InTraceFlags -- The trace flags that might modify searches
	* @param InTerrainComp -- The terrain component
	* @param InResult -- The out param for hit result information
	*/
	FTerrainBVTreeBoxCollisionCheck(const FVector& InStart,const FVector& InEnd, const FVector& InExtent,DWORD InTraceFlags, const UTerrainComponent* InTerrainComp, FCheckResult* InResult);
};

/**
* This struct holds the information used to do a point check against the terrain BV
* tree. It is used purely to gather multiple function parameters into a
* single structure for smaller stack overhead.
*/
struct FTerrainBVTreePointCollisionCheck : public FTerrainBVTreeBoxCollisionCheck
{
	// Holds the minimum penetration distance for push out calculations
	FLOAT BestDistance;
	FVector LocalHitLocation;

	/**
	* Sets up the FTerrainBVTreePointCollisionCheck structure for performing point checks
	* (point plus extent) against a terrain. Initializes all of the variables
	* that are used throughout the check.
	*
	* @param InLocation -- The point to check for intersection
	* @param InExtent -- The extent to check
	* @param InTerrainComp -- The terrain component
	* @param InResult -- The out param for hit result information
	*/
	FTerrainBVTreePointCollisionCheck(const FVector& InLocation,const FVector& InExtent, const UTerrainComponent* InTerrainComp,FCheckResult* InResult) :
		FTerrainBVTreeBoxCollisionCheck(InLocation,InLocation,InExtent,0,InTerrainComp,InResult),
		BestDistance(100000.f)
	{
	}

	/**
	 * @return		The transformed hit location.
	 */
	FVector GetHitLocation(void) const;
};

/**
 * Holds the min/max planes that make up a bounding volume
 */
struct FTerrainBV
{
	/** Bounding volume. */ 
	FBox Bounds;

	/** Initializes the box to invalid state so that adding any point makes the volume update */
	FORCEINLINE FTerrainBV() :
		Bounds(0)
	{}

	/**
	 * Copies the passed in BV and expands it by the extent.
	 *
	 * @param BV -- The BV to copy
	 * @param Extent -- The extent to expand it by
	 */
	FORCEINLINE FTerrainBV(const FTerrainBV BV, const FVector& Extent)
	{
		Bounds.Min[0] = BV.Bounds.Min[0] - Extent.X;
		Bounds.Min[1] = BV.Bounds.Min[1] - Extent.Y;
		Bounds.Min[2] = BV.Bounds.Min[2] - Extent.Z;
		Bounds.Max[0] = BV.Bounds.Max[0] + Extent.X;
		Bounds.Max[1] = BV.Bounds.Max[1] + Extent.Y;
		Bounds.Max[2] = BV.Bounds.Max[2] + Extent.Z;
	}

	/**
	 * Checks a line against this BV. 
	 *
	 * @param	Check		The aggregated line check structure.
	 * @param	OutHitTime	[Out] The value indicating hit time.
	 * @return				TRUE if an intersection occurs.
	 */
	UBOOL LineCheck(const FTerrainBVTreeLineCollisionCheck& Check, FLOAT& OutHitTime) const;	
	
	/**
	 * Checks a point with extent against this BV. The extent is already added in
	 * to the BV being tested (Minkowski sum), so this code just checks to see if
	 * the point is inside the BV. 
	 *
	 * @param	Check	The aggregated point check structure.
	 * @return			TRUE if an intersection occurs.
	 */
	UBOOL PointCheck(const FTerrainBVTreeLineCollisionCheck& Check) const;

	/**
	 * Check (local space) AABB against this BV.
	 *
	 * @param		LocalAABB box in local space.
	 * @return		TRUE if an intersection occurs.
	 */
	UBOOL AABBOverlapCheck(const FBox& LocalAABB) const;

	/** Sets the bounding volume to encompass this subregion of the terrain. */
	void AddTerrainRegion(const FTerrainSubRegion& InRegion, const TArray<FVector>& TerrainVerts, INT TerrainSizeX);

	// Serialization
	friend FArchive& operator<<(FArchive& Ar, FTerrainBV& BV)
	{
		Ar << BV.Bounds;
		return Ar;
	}
};

/**
 * A node in the Terrain BV tree. The node contains the BV that encompasses it's children and/or triangles
 */
struct FTerrainBVNode
{
	/** The bounding volume for this node. */
	FTerrainBV BoundingVolume;

	/** Note this isn't smaller since 4 byte alignment will take over anyway. */
	UBOOL bIsLeaf;

	// Union of either child kDOP nodes or a list of enclosed triangles
	union
	{
		/**
		 * This structure contains the left and right child kDOP indices.
		 * These index values correspond to the array in the FkDOPTree.
		 */
		struct
		{
			WORD NodeIndex[4];
		} n;

		/** This structure indicates the enclosed region of the terrain. */
		FTerrainSubRegion Region;
	};

	/**
	 * Inits the data to no child nodes and an inverted volume
	 */
	FORCEINLINE FTerrainBVNode()
	{
		n.NodeIndex[0]	= n.NodeIndex[1] = n.NodeIndex[2] = n.NodeIndex[3] = ((WORD)-1);
	}

	/** 
	 * Determines the line in the FTerrainBVTreeLineCollisionCheck intersects this node. It
	 * also will check the child nodes if it is not a leaf, otherwise it will check
	 * against the triangle data.
	 *
	 * @param Check -- The aggregated line check data
	 */
	UBOOL LineCheck(FTerrainBVTreeLineCollisionCheck& Check) const;
	

	/**
	 * Works through the list of triangles in this node checking each one for a collision.
	 *
	 * @param Check -- The aggregated line check data
	 */
	UBOOL LineCheckTriangles(FTerrainBVTreeLineCollisionCheck& Check) const;

	/**
	 * Performs collision checking against the triangle using the old collision code to handle it.
	 *
	 * @param Check -- The aggregated line check data
	 * @param v1 -- The first vertex of the triangle
	 * @param v2 -- The second vertex of the triangle
	 * @param v3 -- The third vertex of the triangle
	 */
	UBOOL LineCheckTriangle(FTerrainBVTreeLineCollisionCheck& Check, const FVector& v1,const FVector& v2,const FVector& v3) const;

	/**
	 * Determines the line + extent in the FTerrainBVTreeBoxCollisionCheck intersects this
	 * node. It also will check the child nodes if it is not a leaf, otherwise it
	 * will check against the triangle data.
	 *
	 * @param Check -- The aggregated box check data
	 */
	UBOOL BoxCheck(FTerrainBVTreeBoxCollisionCheck& Check) const;

	/**
	 * Works through the list of triangles in this node checking each one for a collision.
	 *
	 * @param Check -- The aggregated box check data
	 */
	UBOOL BoxCheckTriangles(FTerrainBVTreeBoxCollisionCheck& Check) const;

	/**
	 * Uses the separating axis theorem to check for triangle box collision.
	 *
	 * @param Check -- The aggregated box check data
	 * @param v1 -- The first vertex of the triangle
	 * @param v2 -- The second vertex of the triangle
	 * @param v3 -- The third vertex of the triangle
	 */
	UBOOL BoxCheckTriangle(FTerrainBVTreeBoxCollisionCheck& Check, const FVector& v1,const FVector& v2,const FVector& v3) const;

	/**
	 * Determines the point + extent in the FTerrainBVTreePointCollisionCheck intersects
	 * this node. It also will check the child nodes if it is not a leaf, otherwise
	 * it will check against the triangle data.
	 *
	 * @param Check -- The aggregated point check data
	 */
	UBOOL PointCheck(FTerrainBVTreePointCollisionCheck& Check) const;

	/**
	 * Works through the list of triangles in this node checking each one for a collision.
	 *
	 * @param Check -- The aggregated point check data
	 */
	UBOOL PointCheckTriangles(FTerrainBVTreePointCollisionCheck& Check) const;

	/**
	 * Uses the separating axis theorem to check for triangle box collision.
	 *
	 * @param Check -- The aggregated box check data
	 * @param v1 -- The first vertex of the triangle
	 * @param v2 -- The second vertex of the triangle
	 * @param v3 -- The third vertex of the triangle
	 */
	UBOOL PointCheckTriangle(FTerrainBVTreePointCollisionCheck& Check, const FVector& v1,const FVector& v2,const FVector& v3) const;

	/**
	 *	Take a region of the terrain and either store it in this node or,
	 * if its too big, split into child nodes along its longest dimension.
	 */
	void SplitTerrain(const FTerrainSubRegion& InRegion, const UTerrainComponent* TerrainComp, TArray<FTerrainBVNode>& Nodes);

	// Serialization
	friend FArchive& operator<<(FArchive& Ar, FTerrainBVNode& Node)
	{
		// @warning BulkSerialize: FTerrainBVNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.

		Ar << Node.BoundingVolume << Node.bIsLeaf;
		// If we are a leaf node, serialize out the child node indices
		if (Node.bIsLeaf)
		{
			Ar << Node.n.NodeIndex[0] << Node.n.NodeIndex[1] << Node.n.NodeIndex[2] << Node.n.NodeIndex[3];
		}
		// Otherwise serialize out the triangle collision data
		else
		{
			Ar << Node.Region;
		}
		return Ar;
	}
};

/**
 * This is the tree of bounding volumes (boxes) that spatially divides the terrain. It is a tree of BV nodes.
 */
struct FTerrainBVTree
{
	/** The list of nodes contained within this tree. Node 0 is always the root node. */
	TArray<FTerrainBVNode> Nodes;

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated line check data
	 */
	UBOOL LineCheck(FTerrainBVTreeLineCollisionCheck& Check) const;

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated box check data
	 */
	UBOOL BoxCheck(FTerrainBVTreeBoxCollisionCheck& Check) const;

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated point check data
	 */
	UBOOL PointCheck(FTerrainBVTreePointCollisionCheck& Check) const;

	/** 
	 *	Create the BV tree for this terrain mesh, by recursively splitting the terrain into nodes. 
	 *	TerrainSizeX and TerrainSizeY are number of quads
	 */
	void Build(const UTerrainComponent* TerrainComp);

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,FTerrainBVTree& Tree)
	{
		Ar << Tree.Nodes;
		return Ar;
	}
};

#endif // _TERRAINBVTREE_H
