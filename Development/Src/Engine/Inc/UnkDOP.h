/*=============================================================================
	UnkDOP.h: k-DOP collision
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Joe Graf

	This file contains the definition of the kDOP static mesh collision classes,
	and structures. This code is heavily based upon work done by Michael Mounier.
	Details on the algorithm can be found at:

	ftp://ams.sunysb.edu/pub/geometry/jklosow/tvcg98.pdf

	There is also information on the topic in Real Time Rendering

=============================================================================*/

#ifndef _KDOP_H
#define _KDOP_H

//#define KDOP_DEBUG

// Indicates how many "k / 2" there are in the k-DOP. 3 == AABB == 6 DOP
#define NUM_PLANES	3
// The number of triangles to store in each leaf
#define MAX_TRIS_PER_LEAF 5
// Copied from float.h
#define MAX_FLT 3.402823466e+38F
// Line triangle epsilon (see Real-Time Rendering page 581)
#define LINE_TRIANGLE_EPSILON 1e-5f
// Parallel line kDOP plane epsilon
#define PARALLEL_LINE_KDOP_EPSILON 1e-30f
// Amount to expand the kDOP by
#define FUDGE_SIZE 0.1f

// Forward decl
class FkDOPLineCollisionCheck;
class FkDOPBoxCollisionCheck;
class FkDOPPointCollisionCheck;
class FkDOPSphereQuery;

// Represents a single triangle. A kDOP may have 0 or more triangles contained
// within the node. If it has any triangles, it will be in list (allocated
// memory) form.
struct FkDOPCollisionTriangle
{
	// Triangle index (indexes into Vertices)
	_WORD v1, v2, v3;
	// The material of this triangle
	_WORD MaterialIndex;

	// Set up indices
	FkDOPCollisionTriangle(_WORD Index1,_WORD Index2,_WORD Index3) :
		v1(Index1), v2(Index2), v3(Index3), MaterialIndex(0)
	{
		MaterialIndex = 0;
	}
	// Defualt constructor for serialization
	FkDOPCollisionTriangle(void) : 
		v1(0), v2(0), v3(0), MaterialIndex(0)
	{
	}

	// Serialization
	friend FArchive& operator<<(FArchive& Ar, FkDOPCollisionTriangle& Tri)
	{
		return Ar << Tri.v1 << Tri.v2 << Tri.v3 << Tri.MaterialIndex;
	}
};

// This structure is used during the build process. It contains the triangle's
// centroid for calculating which plane it should be split or not with
struct FkDOPBuildCollisionTriangle : public FkDOPCollisionTriangle
{
	FVector Centroid;

	// Sets the indices, material index, and calculates the centroid
	FkDOPBuildCollisionTriangle(_WORD Index1,_WORD Index2,_WORD Index3,FStaticMeshTriangle* pSMTriangle) :
		FkDOPCollisionTriangle(Index1,Index2,Index3)
	{
		check(pSMTriangle);
		// Copy over the material ID
		MaterialIndex = (_WORD)pSMTriangle->MaterialIndex;
		// Now calculate the centroid for the triangle
		Centroid = (pSMTriangle->Vertices[0] + pSMTriangle->Vertices[1] + pSMTriangle->Vertices[2]) / 3.f;
	}
};

// This structure holds the min & max bounding planes that make up the DOP,
// where k = NUM_PLANES * 2
struct FkDOP
{
	FLOAT Min[NUM_PLANES];
	FLOAT Max[NUM_PLANES];

	// Adds a point to this kDOP (expands the bounding volume)
	void AddPoint(const FVector& Point);
	// Adds a list of triangles to this volume
	void AddTriangles(TArray<FStaticMeshVertex>& Vertices,INT Start,INT NumTris,TArray<FkDOPBuildCollisionTriangle>& BuildTriangles);
	// Sets the data to an invalid state (inside out volume)
	void Init(void)
	{
		for (INT nPlane = 0; nPlane < NUM_PLANES; nPlane++)
		{
			Min[nPlane] = MAX_FLT;
			Max[nPlane] = -MAX_FLT;
		}
	}
	// Checks for a line intersecting this kDOP
	FORCEINLINE UBOOL LineCheck(FkDOPLineCollisionCheck& Check,
		FLOAT& HitTime);
	// Checks to see if a point with extent intesects the kDOP
	FORCEINLINE UBOOL PointCheck(FkDOPPointCollisionCheck& Check);
	// Checks a bounding box against this kdop.
	FORCEINLINE UBOOL AABBOverlapCheck(const FBox& LocalAABB);

	// Constructors
	FkDOP() { Init(); }
	// Constructor that copies and expands by the extent
	FkDOP(const FkDOP& kDOP,const FVector& Extent);

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,FkDOP& kDOP)
	{
		// Serialize the min planes
		for (INT nIndex = 0; nIndex < NUM_PLANES; nIndex++)
		{
			Ar << kDOP.Min[nIndex];
		}
		// Serialize the max planes
		for (INT nIndex = 0; nIndex < NUM_PLANES; nIndex++)
		{
			Ar << kDOP.Max[nIndex];
		}
		return Ar;
	}
};

// A node in the kDOP tree. The node contains the kDOP volume that 
struct FkDOPNode
{
	// Note this isn't smaller since 4 byte alignment will take over anyway
	UBOOL bIsLeaf;
	// The bounding kDOP for this node
	FkDOP BoundingVolume;

	// Union of either child kDOP nodes or a list of enclosed triangles
	union
	{
		// This structure contains the left and right child kDOP indices
		// These index values correspond to the array in the FkDOPTree
		struct
		{
			_WORD LeftNode;
			_WORD RightNode;
		} n;
		// This structure contains the list of enclosed triangles
		// These index values correspond to the triangle information in the
		// FkDOPTree using the start and count as the means of delineating
		// which triangles are involved
		struct
		{
			_WORD NumTriangles;
			_WORD StartIndex;
		} t;
	};

	// Inits the data
	FkDOPNode()
	{
		n.LeftNode = ((_WORD) -1);
        n.RightNode = ((_WORD) -1);
		BoundingVolume.Init();
	}

	// Recursively subdivide a triangle list into hierarchical volumes
	void SplitTriangleList(TArray<FStaticMeshVertex>& Vertices,INT Start,INT NumTris,TArray<FkDOPBuildCollisionTriangle>& BuildTriangles,TArray<FkDOPNode>& Nodes);
	// Performs a line check against the node
	UBOOL LineCheck(FkDOPLineCollisionCheck& Check);
	// Performs a line check against the triangles in this node
	FORCEINLINE UBOOL LineCheckTriangles(FkDOPLineCollisionCheck& Check);
	// Performs a line check against a single triangle
	FORCEINLINE UBOOL LineCheckTriangle(FkDOPLineCollisionCheck& Check,const FStaticMeshVertex& v1,const FStaticMeshVertex& v2,const FStaticMeshVertex& v3,INT MaterialIndex);
	// Sweeps a box against this node and its children
	UBOOL BoxCheck(FkDOPBoxCollisionCheck& Check);
	// Checks for an intersection of the box and this node's triangles
	FORCEINLINE UBOOL BoxCheckTriangles(FkDOPBoxCollisionCheck& Check);
	// Checks for an intersection of the box and a single triangle
	FORCEINLINE UBOOL BoxCheckTriangle(FkDOPBoxCollisionCheck& Check,const FVector& v1,const FVector& v2,const FVector& v3,INT MaterialIndex);
	// Checks to see if a point with extent intesects with this node
	UBOOL PointCheck(FkDOPPointCollisionCheck& Check);
	// Checks for an intersection of the point (plus extent) and this node's triangles
	FORCEINLINE UBOOL PointCheckTriangles(FkDOPPointCollisionCheck& Check);
	// Checks for an intersection of the point (plus extent) and a single triangle
	FORCEINLINE UBOOL PointCheckTriangle(FkDOPPointCollisionCheck& Check, const FVector& v1,const FVector& v2,const FVector& v3, INT MaterialIndex);
	// Query tree to find overlapping triangles.
	void SphereQuery(FkDOPSphereQuery& Query);

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,FkDOPNode& Node)
	{
		Ar << Node.BoundingVolume << Node.bIsLeaf;
		// If we are a leaf node, serialize out the child node indices
		if (Node.bIsLeaf)
		{
			Ar << Node.n.LeftNode << Node.n.RightNode;
		}
		// Otherwise serialize out the triangle collision data
		else
		{
			Ar << Node.t.NumTriangles << Node.t.StartIndex;
		}
		return Ar;
	}
};

// This is the tree of kDOPs that spatially divides the static mesh. It is
// a binary tree of kDOP nodes.
struct FkDOPTree
{
	// This is the list of nodes contained within this tree. Node 0 is always
	// the root node
	TArray<FkDOPNode> Nodes;
	// This is the list of collision triangles in this tree
	TArray<FkDOPCollisionTriangle> Triangles;

#ifdef KDOP_COLL_DEBUG
	INT Level;
	FkDOPTree() : Level(0) {}
#endif

	// Build the root node for the tree and have it recursively subdivide
	void Build(TArray<FStaticMeshVertex>& Vertices,TArray<FkDOPBuildCollisionTriangle>& BuildTriangles);
	// Performs a line check against the tree
	UBOOL LineCheck(FkDOPLineCollisionCheck& Check);
	// Performs a swept box check against the tree
	UBOOL BoxCheck(FkDOPBoxCollisionCheck& Check);
	// Checks to see if a point with extent intesects the tree
	UBOOL PointCheck(FkDOPPointCollisionCheck& Check);
	// Query a sphere against the tree and return any 
	void SphereQuery(FkDOPSphereQuery& Query);

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,FkDOPTree& Tree)
	{
		return Ar << Tree.Nodes << Tree.Triangles;
	}
};

//
//	FCollisionCheck
//
class FCollisionCheck
{
public:

	FCheckResult*			Result;
	AActor*					Owner;
	UStaticMeshComponent*	Component;
	UStaticMesh*			StaticMesh;

	// Constructor.

	FCollisionCheck(FCheckResult* InResult,UStaticMeshComponent* InComponent);
};

// This class holds the information used to do a line check against the kDOP
// tree. It is used purely to gather multiple function parameters into a
// single structure for smaller stack overhead.
class FkDOPLineCollisionCheck : public FCollisionCheck
{
public:
	// Constant input vars
	const FVector& Start;
	const FVector& End;
	// Matrix transformations
	FMatrix WorldToLocal;
	// Locally calculated vectors
	FVector LocalStart;
	FVector LocalEnd;
	FVector LocalDir;
	FVector LocalOneOverDir;
	// Normal in local space which gets transformed to world at the very end
	FVector LocalHitNormal;
	// The kDOP tree
	FkDOPTree& kDOPTree;
	// The array of the nodes for the kDOP tree
	TArray<FkDOPNode>& Nodes;
	// The collision triangle data for the kDOP tree
	const TArray<FkDOPCollisionTriangle>& CollisionTriangles;
	// The rendering triangle data for the mesh
	const TArray<FStaticMeshVertex>& Triangles;

	// Basic constructor
	FkDOPLineCollisionCheck(FCheckResult* InResult,UStaticMeshComponent* InComponent,const FVector& InStart,const FVector& InEnd);

	// Transforms the local hit normal into a world space normal using the transpose
	// adjoint and flips the normal if need be
	FORCEINLINE FVector GetHitNormal(void)
	{
		// Transform the hit back into world space using the transpose adjoint
		return ((UPrimitiveComponent*)Component)->LocalToWorld.TransposeAdjoint().TransformNormal(LocalHitNormal).SafeNormal() * Sgn(((UPrimitiveComponent*)Component)->LocalToWorldDeterminant);
	}
};

// This class holds the information used to do a box and/or point check
// against the kDOP tree. It is used purely to gather multiple function
// parameters into a single structure for smaller stack overhead.
class FkDOPBoxCollisionCheck : public FkDOPLineCollisionCheck
{
public:
	// Constant input vars
	const FVector& Extent;
	// Calculated vars
	FVector LocalExtent;
	FVector LocalBoxX;
	FVector LocalBoxY;
	FVector LocalBoxZ;

	// Basic constructor
	FkDOPBoxCollisionCheck(FCheckResult* InResult,UStaticMeshComponent* InComponent,const FVector& InStart,const FVector& InEnd,const FVector& InExtent);
};

// This class holds the information used to do a point check against the kDOP
// tree. It is used purely to gather multiple function parameters into a
// single structure for smaller stack overhead.
class FkDOPPointCollisionCheck : public FkDOPBoxCollisionCheck
{
public:
	// Holds the minimum pentration distance for push out calculations
	FLOAT BestDistance;
	FVector LocalHitLocation;

	// Basic constructor
	FkDOPPointCollisionCheck(FCheckResult* InResult,UStaticMeshComponent* InComponent,const FVector& InLocation,const FVector& InExtent);

	// Returns the transformed hit location
	FORCEINLINE FVector GetHitLocation(void)
	{
		// Push out the hit location from the point along the hit normal and
		// convert into world units
		return ((UPrimitiveComponent*)Component)->LocalToWorld.TransformFVector(LocalStart + 
			LocalHitNormal * BestDistance);
	}

};


// Info for sphere vs. kdop tree query
class FkDOPSphereQuery 
{
public:
	FBox									LocalBox;
	TArray<INT>&							ReturnTriangles; // Index of FkDOPCollisionTriangle in KDOPTree

	TArray<FkDOPNode>&						Nodes;
	const TArray<FkDOPCollisionTriangle>&	CollisionTriangles;

	FkDOPSphereQuery(UStaticMeshComponent* InComponent, const FSphere& InSphere, TArray<INT>& OutTriangles);
};

#endif
