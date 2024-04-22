/*=============================================================================
	UnkDOP.h: k-DOP collision
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	This file contains the definition of the kDOP static mesh collision classes,
	and structures.
	Details on the algorithm can be found at:

	ftp://ams.sunysb.edu/pub/geometry/jklosow/tvcg98.pdf

	There is also information on the topic in Real Time Rendering

=============================================================================*/

#include "UnCollision.h"

#ifndef _KDOP_H
#define _KDOP_H

/** Enable the relative fast kDop verification, and everything for tree building */
#define TEST_COMPACT_KDOP (0)
/** Enable exhaustive kDop verification probably a severe performance impact */
#define TEST_COMPACT_KDOP_SLOW (0)
/** Enables SIMD for node decompression */
#define COMPACT_KDOP_SIMD_DECOMPRESSION (1)

// inlines in templates are impossible to debug
#if TEST_COMPACT_KDOP_SLOW
#define KDOP_INLINE 
#else
#define KDOP_INLINE FORCEINLINE
#endif

/** The compact version has a pad node for a few different reasons: SIMD overfetch and since leaves don't actually exist, we need a junk node for that */
#define NUM_PAD_NODES (1)
/** The number of triangles to store in each leaf */
#define MAX_TRIS_PER_LEAF_COMPACT 5


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

/** container to get these constants out of the global namespace
* cannot be in the class because then we have a billion redundant copies 
**/
struct KDopSIMD
{
	/** MakeVectorRegister( (DWORD)0, (DWORD)-1, (DWORD)-1, (DWORD)0 ); */
	static const VectorRegister VMaxMergeMask;
	/** MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)0, (DWORD)0 ); */
	static const VectorRegister VMinMergeMask;
	/** MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)-1, (DWORD)0 ) */
	static const VectorRegister VReplace4thMask;
	/** { 127.5f,127.5f,127.5f,127.5f }; */
	static const VectorRegister V_127p5;
	/** { .5f,.5f,.5f,.5f }; */
	static const VectorRegister V_p5;
	/**  = { -.5f,-.5f,-.5f,-.5f }; */
	static const VectorRegister V_p5Neg;
	/** { 1.0f / 127.0f, 1.0f / 127.0f, 1.0f / 127.0f, 1.0f / 127.0f }; */
	static const VectorRegister V_127Inv;
	/** MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)-1, (DWORD)0 ); */
	static const VectorRegister VMinMergeOut;
	/** MakeVectorRegister( FUDGE_SIZE, FUDGE_SIZE, FUDGE_SIZE, FUDGE_SIZE ); */
	static const VectorRegister VFudgeFactor;
	/** MakeVectorRegister( 2.0f * FUDGE_SIZE, 2.0f * FUDGE_SIZE, 2.0f * FUDGE_SIZE, 2.0f * FUDGE_SIZE ); */
	static const VectorRegister VTwoFudgeFactor;
	/** MakeVectorRegister( -FUDGE_SIZE, -FUDGE_SIZE, -FUDGE_SIZE, -FUDGE_SIZE ); */
	static const VectorRegister VNegFudgeFactor;
	/** MakeVectorRegister( 1.0f + FUDGE_SIZE, 1.0f + FUDGE_SIZE, 1.0f + FUDGE_SIZE, 1.0f + FUDGE_SIZE ); */
	static const VectorRegister VOnePlusFudgeFactor;
	/** { -.5f / 127.0f, -.5f / 127.0f, -.5f / 127.0f, -.5f / 127.0f } */
	static const VectorRegister V_p5Neg_m_127Inv;
	/** { -1.0f / 127.0f, -1.0f / 127.0f, -1.0f / 127.0f, -1.0f / 127.0f } */
	static const VectorRegister V_127InvNeg;
	/**
		MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)-1, (DWORD)-1 ),
		MakeVectorRegister( (DWORD)0, (DWORD)0, (DWORD)0, (DWORD)0 )
	**/
	static const VectorRegister VAlignMasks[2];
};

/** Used to pad and align bounding boxes on the stack so that SIMD can use them
*/
template<typename BOUND_TYPE>
struct MS_ALIGN(16) FEnsurePadding : public BOUND_TYPE
{
	FLOAT Pad[2];
	FEnsurePadding()
	{
	}
	FEnsurePadding(INT)
		: BOUND_TYPE(0)
	{
	}
	FEnsurePadding(const BOUND_TYPE& InBound)
		: BOUND_TYPE(InBound)
	{
	}
} GCC_ALIGN(16);

/**
 * Represents a single triangle. A kDOP may have 0 or more triangles contained
 * within the node. If it has any triangles, it will be in list (allocated
 * memory) form.
 */
template<typename KDOP_IDX_TYPE> struct FkDOPCollisionTriangle
{
	// Triangle index (indexes into Vertices)
	KDOP_IDX_TYPE v1, v2, v3;
	// The material of this triangle
	KDOP_IDX_TYPE MaterialIndex;

	// Set up indices
	FkDOPCollisionTriangle(KDOP_IDX_TYPE Index1,KDOP_IDX_TYPE Index2,KDOP_IDX_TYPE Index3) :
		v1(Index1), v2(Index2), v3(Index3), MaterialIndex(0)
	{
		}
	/**
	 * Full constructor that sets indices and material
	 */
	FkDOPCollisionTriangle(KDOP_IDX_TYPE Index1,KDOP_IDX_TYPE Index2,KDOP_IDX_TYPE Index3,KDOP_IDX_TYPE Material) :
		v1(Index1), v2(Index2), v3(Index3), MaterialIndex(Material)
	{
	}
	// Default constructor for serialization
	FkDOPCollisionTriangle(void) : 
		v1(0), v2(0), v3(0), MaterialIndex(0)
	{
	}

	// Serialization
	friend FArchive& operator<<(FArchive& Ar, FkDOPCollisionTriangle<KDOP_IDX_TYPE>& Tri)
	{
		// @warning BulkSerialize: FkDOPCollisionTriangle is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << Tri.v1 << Tri.v2 << Tri.v3 << Tri.MaterialIndex;
		return Ar;
	}
};

// This structure is used during the build process. It contains the triangle's
// centroid for calculating which plane it should be split or not with
template<typename KDOP_IDX_TYPE> struct FkDOPBuildCollisionTriangle : public FkDOPCollisionTriangle<KDOP_IDX_TYPE>
{
	/**
	 * Centroid of the triangle used for determining which bounding volume to
	 * place the triangle in
	 */
	FVector Centroid;
	/**
	 * First vertex in the triangle
	 */
	FVector V0;
	/**
	 * Second vertex in the triangle
	 */
	FVector V1;
	/**
	 * Third vertex in the triangle
	 */
	FVector V2;

	/**
	 * Sets the indices, material index, calculates the centroid using the
	 * specified triangle vertex positions
	 */
	FkDOPBuildCollisionTriangle(KDOP_IDX_TYPE Index1,KDOP_IDX_TYPE Index2,KDOP_IDX_TYPE Index3,
		KDOP_IDX_TYPE InMaterialIndex,
		const FVector& vert0,const FVector& vert1,const FVector& vert2) :
		FkDOPCollisionTriangle<KDOP_IDX_TYPE>(Index1,Index2,Index3,InMaterialIndex),
		V0(vert0), V1(vert1), V2(vert2)
	{
		// Now calculate the centroid for the triangle
		Centroid = (V0 + V1 + V2) / 3.f;
	}
};

// Forward declarations
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE> struct TkDOP;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE> struct TkDOPNode;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE> struct TkDOPTree;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE> struct TkDOPCompact;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE> struct TkDOPNodeCompact;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE> struct TkDOPTreeCompact;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE,typename TREE_TYPE = TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> > struct TkDOPCollisionCheck;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE,typename TREE_TYPE = TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> > struct TkDOPLineCollisionCheck;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE,typename TREE_TYPE = TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> > struct TkDOPBoxCollisionCheck;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE,typename TREE_TYPE = TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> > struct TkDOPPointCollisionCheck;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE,typename TREE_TYPE = TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> > struct TkDOPSphereQuery;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE,typename TREE_TYPE = TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> > struct TkDOPFrustumQuery;
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE,typename TREE_TYPE = TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> > struct TkDOPAABBQuery;


// Calculates the physical material that was hit from the physical material mask on the hit material (if it exists)
// Templated struct for partial template specialization because not all collision data providers are setup to support this type of check
template<typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE> 
struct TkDOPPhysicalMaterialCheck
{

	static UPhysicalMaterial* DetermineMaskedPhysicalMaterial(	const COLL_DATA_PROVIDER& CollDataProvider, 
																const FVector& Intersection,
																const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri,
																KDOP_IDX_TYPE MaterialIndex )
	{
		// All non specialized templates return NULL as they dont have masked physical materials
		return NULL;
	}
};

/**
 * Contains the set of planes we check against.
 *
 * @todo templatize for higher numbers of planes (assuming this is faster)
 */
struct FkDOPPlanes
{
	/**
	 * The set of plane normals that have been pushed out in the min/max list
	 */
	static FVector PlaneNormals[NUM_PLANES];
};

/**
	* Hold basic tree-independent data about line checks
 */
struct FLineCollisionInfo
{
	/**
	 * Where the collision results get stored
	 */
	FCheckResult* Result;
	/**
	 * Flags for optimizing a trace
	 */
	const DWORD TraceFlags;
	// Constant input vars
	const FVector& Start;
	const FVector& End;
	// Locally calculated vectors
	MS_ALIGN(16) FVector LocalStart GCC_ALIGN(16);  
	FVector LocalEnd;
	MS_ALIGN(16) FVector LocalDir GCC_ALIGN(16);
	MS_ALIGN(16) FVector LocalOneOverDir GCC_ALIGN(16);   
	MS_ALIGN(16) FVector LocalExtentPad GCC_ALIGN(16);   
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
	 * @param InResult -- The out param for hit result information
	 * @param WorldToLocal -- As obtained from the collison provider
	 */
	FLineCollisionInfo(const FVector& InStart,const FVector& InEnd,DWORD InTraceFlags,FCheckResult* InResult,const FMatrix& WorldToLocal) :
		Result(InResult), TraceFlags(InTraceFlags), Start(InStart), End(InEnd), 
		LocalExtentPad(FUDGE_SIZE,FUDGE_SIZE,FUDGE_SIZE)
	{
		// Move start and end to local space
		LocalStart = WorldToLocal.TransformFVector(Start);
		LocalEnd = WorldToLocal.TransformFVector(End);
		// Calculate the vector's direction in local space
		LocalDir = LocalEnd - LocalStart;
		// Build the one over dir
		// can be infinity and that is ok, even good!
		LocalOneOverDir.X = 1.f / LocalDir.X;
		LocalOneOverDir.Y = 1.f / LocalDir.Y;
		LocalOneOverDir.Z = 1.f / LocalDir.Z;
#if PS3  // -XFastmath option has does not produce INF, which we need for this to work
		if (LocalDir.X == 0.0f)
		{
			LocalOneOverDir.X = 1.0f / 0.0f;
		}
		if (LocalDir.Y == 0.0f)
		{
			LocalOneOverDir.Y = 1.0f / 0.0f;
		}
		if (LocalDir.Z == 0.0f)
		{
			LocalOneOverDir.Z = 1.0f / 0.0f;
		}
#endif
		// Clear the closest hit time
		Result->Time = MAX_FLT;
	}
};

/**
 * Holds the min/max planes that make up a bounding volume
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE> struct TkDOP :
	public FkDOPPlanes
{
	/** Exposes data provider type to clients. */
	typedef COLL_DATA_PROVIDER DataProviderType;

	/**
	 * Min planes for this bounding volume
	 */
	FLOAT Min[NUM_PLANES];
	/**
	 * Max planes for this bounding volume
	 */
	FLOAT Max[NUM_PLANES];

	/**
	 * Initializes the planes to invalid states so that adding any point
	 * makes the volume update
	 */
	KDOP_INLINE TkDOP()
	{
		Init();
	}

	/**
	 * No intialization for performance critical things that will do their own intialization 
	 */
	KDOP_INLINE TkDOP(INT)
	{
	}

	/**
	 * Checks the box for validity
	 * @return TRUE if all numbers are finite and the box is not inside out
	 */
	UBOOL IsValid() const
	{
		for (INT Plane = 0; Plane < NUM_PLANES; Plane++)
		{
			if (
				appIsNaN(Min[Plane]) ||
				appIsNaN(Max[Plane]) ||
				!appIsFinite(Min[Plane]) ||
				!appIsFinite(Max[Plane]) ||
				Min[Plane] > Max[Plane])
			{
				return FALSE;
			}
		}
		return TRUE;
	}
	/**
	 * Returns TRUE if this is contained in the given bound
	 * @param Inner bound to check
	 * @return TRUE if Inner is completely inside of this
	 */
	UBOOL IsContainedIn(const TkDOP& Outer) const
	{
		for (INT PlaneIndex = 0; PlaneIndex < NUM_PLANES; PlaneIndex++)
		{
			if (Min[PlaneIndex] < Outer.Min[PlaneIndex] ||
				Max[PlaneIndex] > Outer.Max[PlaneIndex])
			{
				return FALSE;
			}
		}
		return TRUE;
	}
	/**
	 * Compute the surface area of the box
	 * @return the surface area
	 */
	FLOAT SurfaceArea() const
	{
		return 2.0f * (
						(Max[0] - Min[0]) * (Max[1] - Min[1]) +
						(Max[0] - Min[0]) * (Max[2] - Min[2]) +
						(Max[1] - Min[1]) * (Max[2] - Min[2]) );
	}
	/**
	 * Replace this with the union of this and another bound
	 * @param Other other bound
	 */
	void Union(const TkDOP& Other)
	{
		check(IsValid());
		check(Other.IsValid());
		for (INT PlaneIndex = 0; PlaneIndex < NUM_PLANES; PlaneIndex++)
		{
			Min[PlaneIndex] = ::Min<FLOAT>(Min[PlaneIndex],Other.Min[PlaneIndex]);
			Max[PlaneIndex] = ::Max<FLOAT>(Max[PlaneIndex],Other.Max[PlaneIndex]);
		}
	}

	/**
	 * equality operator
	 * @param Other bound to test against me for equality
	 */
	UBOOL operator==(const TkDOP& Other) const
	{
		return IsContainedIn(Other) && Other.IsContainedIn(*this);
	}

	/**
	 * Copies the passed in FkDOP and expands it by the extent. Note assumes AABB.
	 *
	 * @param kDOP -- The kDOP to copy
	 * @param Extent -- The extent to expand it by
	 */
	KDOP_INLINE TkDOP(const TkDOP<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& kDOP,const FVector& Extent)
	{
		Min[0] = kDOP.Min[0] - Extent.X;
		Min[1] = kDOP.Min[1] - Extent.Y;
		Min[2] = kDOP.Min[2] - Extent.Z;
		Max[0] = kDOP.Max[0] + Extent.X;
		Max[1] = kDOP.Max[1] + Extent.Y;
		Max[2] = kDOP.Max[2] + Extent.Z;
	}

	/**
	 * Adds a new point to the kDOP volume, expanding it if needed.
	 *
	 * @param Point The vector to add to the volume
	 */
	void AddPoint(const FVector& Point)
	{
		// Dot against each plane and expand the volume out to encompass it if the
		// point is further away than either (min/max) of the previous points
		for (INT Plane = 0; Plane < NUM_PLANES; Plane++)
		{
			// Project this point onto the plane normal
			FLOAT Dot = Point | FkDOPPlanes::PlaneNormals[Plane];
			// Move the plane out as needed
			if (Dot < Min[Plane])
			{
				Min[Plane] = Dot;
			}
			if (Dot > Max[Plane])
			{
				Max[Plane] = Dot;
			}
		}
	}

	/**
	 * Adds a list of triangles to this volume
	 *
	 * @param Start the starting point in the build triangles array
	 * @param NumTris the number of tris to iterate through adding from the array
	 * @param BuildTriangles the list of triangles used for building the collision data
	 */
	void AddTriangles(KDOP_IDX_TYPE StartIndex,KDOP_IDX_TYPE NumTris,
		TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles)
	{
		// Reset the min/max planes
		Init();
		// Go through the list and add each of the triangle verts to our volume
		for (KDOP_IDX_TYPE Triangle = StartIndex; Triangle < StartIndex + NumTris; Triangle++)
		{
			AddPoint(BuildTriangles(Triangle).V0);
			AddPoint(BuildTriangles(Triangle).V1);
			AddPoint(BuildTriangles(Triangle).V2);
		}
	}

	/**
	 * Sets the data to an invalid state (inside out volume)
	 */
	KDOP_INLINE void Init(void)
	{
		for (INT nPlane = 0; nPlane < NUM_PLANES; nPlane++)
		{
			Min[nPlane] = MAX_FLT;
			Max[nPlane] = -MAX_FLT;
		}
	}

	/**
	 * Checks a line against this kDOP. Note this assumes a AABB. If more planes
	 * are to be used, this needs to be rewritten. Also note, this code is Andrew's
	 * original code modified to work with FkDOP
	 *
	 * input:	LCI The aggregated line check structure
	 *			HitTime The out value indicating hit time
	 */
	UBOOL LineCheck(const FLineCollisionInfo& LCI,FLOAT& HitTime) const
	{
		FVector	Time(0.f,0.f,0.f);
		UBOOL Inside = 1;

		HitTime = 0.0f;  // always initialize (prevent valgrind whining) --ryan.

		if(LCI.LocalStart.X < Min[0])
		{
			if(LCI.LocalDir.X <= 0.0f)
				return 0;
			else
			{
				Inside = 0;
				Time.X = (Min[0] - LCI.LocalStart.X) * LCI.LocalOneOverDir.X;
			}
		}
		else if(LCI.LocalStart.X > Max[0])
		{
			if(LCI.LocalDir.X >= 0.0f)
				return 0;
			else
			{
				Inside = 0;
				Time.X = (Max[0] - LCI.LocalStart.X) * LCI.LocalOneOverDir.X;
			}
		}

		if(LCI.LocalStart.Y < Min[1])
		{
			if(LCI.LocalDir.Y <= 0.0f)
				return 0;
			else
			{
				Inside = 0;
				Time.Y = (Min[1] - LCI.LocalStart.Y) * LCI.LocalOneOverDir.Y;
			}
		}
		else if(LCI.LocalStart.Y > Max[1])
		{
			if(LCI.LocalDir.Y >= 0.0f)
				return 0;
			else
			{
				Inside = 0;
				Time.Y = (Max[1] - LCI.LocalStart.Y) * LCI.LocalOneOverDir.Y;
			}
		}

		if(LCI.LocalStart.Z < Min[2])
		{
			if(LCI.LocalDir.Z <= 0.0f)
				return 0;
			else
			{
				Inside = 0;
				Time.Z = (Min[2] - LCI.LocalStart.Z) * LCI.LocalOneOverDir.Z;
			}
		}
		else if(LCI.LocalStart.Z > Max[2])
		{
			if(LCI.LocalDir.Z >= 0.0f)
				return 0;
			else
			{
				Inside = 0;
				Time.Z = (Max[2] - LCI.LocalStart.Z) * LCI.LocalOneOverDir.Z;
			}
		}

		if(Inside)
		{
			HitTime = 0.f;
			return 1;
		}

		HitTime = Time.GetMax();

		if(HitTime >= 0.0f && HitTime <= 1.0f)
		{
			const FVector& Hit = LCI.LocalStart + LCI.LocalDir * HitTime;

			return (Hit.X > Min[0] - FUDGE_SIZE && Hit.X < Max[0] + FUDGE_SIZE &&
					Hit.Y > Min[1] - FUDGE_SIZE && Hit.Y < Max[1] + FUDGE_SIZE &&
					Hit.Z > Min[2] - FUDGE_SIZE && Hit.Z < Max[2] + FUDGE_SIZE);
		}
		return 0;
	}
	
	/**
	 * Checks a point to see if it is int the bound
	 *
	 * @param Who the point to check
	 */
	UBOOL PointCheck(const FVector& Who) const
	{
		return Who.X >= Min[0] && Who.X <= Max[0] 
			&& Who.Y >= Min[1] && Who.Y <= Max[1] 
			&& Who.Z >= Min[2] && Who.Z <= Max[2];
	}

	/**
	 * check()s a point to see if it is int the bound
	 *
	 * @param Who the point to check
	 */
	void CheckPointCheck(const FVector& Who) const
	{
		UBOOL Ret = PointCheck(Who);
		if (!Ret)
		{
			debugf(TEXT("Min  %7.4f,%7.4f,%7.4f"),Min[0],Min[1],Min[2]);
			debugf(TEXT("Max  %7.4f,%7.4f,%7.4f"),Max[0],Max[1],Max[2]);
			debugf(TEXT("Test %7.4f,%7.4f,%7.4f"),Who.X,Who.Y,Who.Z);

			check(Ret);
		}
	}

	/**
	 * Checks a point with extent against this kDOP. The extent is already added in
	 * to the kDOP being tested (Minkowski sum), so this code just checks to see if
	 * the point is inside the kDOP. Note this assumes a AABB. If more planes are 
	 * to be used, this needs to be rewritten.
	 *
	 * @param FLineCollisionInfo The aggregated point check structure
	 */
	UBOOL PointCheck(FLineCollisionInfo& LCI) const
	{
		return PointCheck(LCI.LocalStart);
	}

	/**
	 * Check (local space) AABB against this kDOP.
	 *
	 * @param LocalAABB box in local space
	 */
	UBOOL AABBOverlapCheck(const FBox& LocalAABB) const
	{
		return Min[0] <= LocalAABB.Max.X && LocalAABB.Min.X <= Max[0] &&
			Min[1] <= LocalAABB.Max.Y && LocalAABB.Min.Y <= Max[1] &&
			Min[2] <= LocalAABB.Max.Z && LocalAABB.Min.Z <= Max[2];
	}

	/**
	 * Check frustum planes against this kDOP.
	 *
	 * input:	frustum planes
	 */
	UBOOL FrustumCheck(const TArray<FPlane>& FrustumPlanes) const
	{
		const FVector Extent((Max[0] - Min[0]) * 0.5f, (Max[1] - Min[1]) * 0.5f, (Max[2] - Min[2]) * 0.5f);
		const FVector Center(Min[0] + Extent.X, Min[1] + Extent.Y, Min[2] + Extent.Z);

		for( INT PlaneIdx = 0; PlaneIdx < FrustumPlanes.Num() ; PlaneIdx++ )
		{
			const FLOAT PushOut = FBoxPushOut( FrustumPlanes(PlaneIdx), Extent );
			const FLOAT Dist = FrustumPlanes(PlaneIdx).PlaneDot(Center);
			if ( Dist > PushOut )
			{
				return FALSE;
			}
		}
		return TRUE;
	}

	/** Create an FBox for these bounds. */
	FBox ToFBox() const
	{
		FVector NodeMin(Min[0], Min[1], Min[2]);
		FVector NodeMax(Max[0], Max[1], Max[2]);
		return FBox(NodeMin, NodeMax);
	}

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,TkDOP<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& kDOP)
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

/**
 * A node in the kDOP tree. The node contains the kDOP volume that encompasses
 * it's children and/or triangles
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE> struct TkDOPNode
{
	/** Exposes data provider type to clients. */
	typedef COLL_DATA_PROVIDER							DataProviderType;

	/** Exposes node type to clients. */
	typedef TkDOPNode<DataProviderType,KDOP_IDX_TYPE>	NodeType;

	/** Exposes kDOP type to clients. */
	typedef TkDOP<DataProviderType,KDOP_IDX_TYPE>		kDOPType;

	// The bounding kDOP for this node
	kDOPType BoundingVolume;

	// Note this isn't smaller since 4 byte alignment will take over anyway
	UBOOL bIsLeaf;

	// Union of either child kDOP nodes or a list of enclosed triangles
	union
	{
		// This structure contains the left and right child kDOP indices
		// These index values correspond to the array in the FkDOPTree
		struct
		{
			KDOP_IDX_TYPE LeftNode;
			KDOP_IDX_TYPE RightNode;
		} n;
		// This structure contains the list of enclosed triangles
		// These index values correspond to the triangle information in the
		// FkDOPTree using the start and count as the means of delineating
		// which triangles are involved
		struct
		{
			KDOP_IDX_TYPE NumTriangles;
			KDOP_IDX_TYPE StartIndex;
		} t;
	};

	/**
	 * Inits the data to no child nodes and an inverted volume
	 */
	KDOP_INLINE TkDOPNode()
	{
		n.LeftNode = ((KDOP_IDX_TYPE) -1);
        n.RightNode = ((KDOP_IDX_TYPE) -1);
		BoundingVolume.Init();
	}

	/**
	 * Find the best split plane for a list of build triangles
	 * Uses the mean (splatter method). 
	 *
	 * @param Start -- The triangle index to start processing with
	 * @param NumTris -- The number of triangles to process
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 * @return Index of best plane
	 */
	static INT FindBestPlane(INT Start,INT NumTris,
		TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles,
		FLOAT &BestMean)
	{
		INT BestPlane = -1;
		FLOAT BestVariance = 0.f;
		// Determine how to split using the splatter algorithm
		for (INT nPlane = 0; nPlane < NUM_PLANES; nPlane++)
		{
			FLOAT Mean = 0.f;
			FLOAT Variance = 0.f;
			// Compute the mean for the triangle list
			for (INT nTriangle = Start; nTriangle < Start + NumTris; nTriangle++)
			{
				// Project the centroid of the triangle against the plane
				// normals and accumulate to find the total projected
				// weighting
				Mean += BuildTriangles(nTriangle).Centroid | FkDOPPlanes::PlaneNormals[nPlane];
			}
			// Divide by the number of triangles to get the average
			Mean /= FLOAT(NumTris);
			// Compute variance of the triangle list
			for (INT nTriangle = Start; nTriangle < Start + NumTris;nTriangle++)
			{
				// Project the centroid again
				FLOAT Dot = BuildTriangles(nTriangle).Centroid | FkDOPPlanes::PlaneNormals[nPlane];
				// Now calculate the variance and accumulate it
				Variance += (Dot - Mean) * (Dot - Mean);
			}
			// Get the average variance
			Variance /= FLOAT(NumTris);
			// Determine if this plane is the best to split on or not
			if (Variance >= BestVariance)
			{
				BestPlane = nPlane;
				BestVariance = Variance;
				BestMean = Mean;
			}
		}
		return BestPlane;
	}


	/**
	 * Determines if the node is a leaf or not. If it is not a leaf, it subdivides
	 * the list of triangles again adding two child nodes and splitting them on
	 * the mean (splatter method). Otherwise it sets up the triangle information.
	 *
	 * @param Start -- The triangle index to start processing with
	 * @param NumTris -- The number of triangles to process
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 * @param Nodes -- The list of nodes in this tree
	 */
	void SplitTriangleList(INT Start,INT NumTris,
		TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles,
		TArray<NodeType>& Nodes)
	{
		// Add all of the triangles to the bounding volume
		BoundingVolume.AddTriangles(Start,NumTris,BuildTriangles);
		// Figure out if we are a leaf node or not
		if (NumTris > MAX_TRIS_PER_LEAF)
		{
			// Still too many triangles, so continue subdividing the triangle list
			bIsLeaf = 0;
			FLOAT BestMean = 0.0f;
			INT BestPlane = FindBestPlane(Start,NumTris,BuildTriangles,BestMean);

			// Now that we have the plane to split on, work through the triangle
			// list placing them on the left or right of the splitting plane
			INT Left = Start - 1;
			INT Right = Start + NumTris;
			// Keep working through until the left index passes the right
			while (Left < Right)
			{
				FLOAT Dot;
				// Find all the triangles to the "left" of the splitting plane
				do
				{
					Dot = BuildTriangles(++Left).Centroid | FkDOPPlanes::PlaneNormals[BestPlane];
				}
				while (Dot < BestMean && Left < Right && Left+1 != (Start+NumTris));
				// Find all the triangles to the "right" of the splitting plane
				do
				{
					Dot = BuildTriangles(--Right).Centroid | FkDOPPlanes::PlaneNormals[BestPlane];
				}
				while (Dot >= BestMean && Right > 0 && Left < Right);
				// Don't swap the triangle data if we just hit the end
				if (Left < Right)
				{
					// Swap the triangles since they are on the wrong sides of the
					// splitting plane
					FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> Temp = BuildTriangles(Left);
					BuildTriangles(Left) = BuildTriangles(Right);
					BuildTriangles(Right) = Temp;
				}
			}
			// Check for wacky degenerate case where more than MAX_TRIS_PER_LEAF
			// fall all in the same kDOP
			if (Left == Start + NumTris || Right == Start)
			{
				Left = Start + (NumTris / 2);
			}
			// Add the two child nodes
			n.LeftNode = Nodes.Add(2);
			n.RightNode = n.LeftNode + 1;
			// Have the left node recursively subdivide it's list
			Nodes(n.LeftNode).SplitTriangleList(Start,Left - Start,BuildTriangles,Nodes);
			// And now have the right node recursively subdivide it's list
			Nodes(n.RightNode).SplitTriangleList(Left,Start + NumTris - Left,BuildTriangles,Nodes);
		}
		else
		{
			// No need to subdivide further so make this a leaf node
			bIsLeaf = 1;
			// Copy in the triangle information
			t.StartIndex = Start;
			t.NumTriangles = NumTris;
		}
	}

	/* 
	 * Determines the line in the FkDOPLineCollisionCheck intersects this node. It
	 * also will check the child nodes if it is not a leaf, otherwise it will check
	 * against the triangle data.
	 *
	 * @param Check -- The aggregated line check data
	 */
	inline UBOOL LineCheck(TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		UBOOL bHit = 0;
		// If this is a node, check the two child nodes and pick the closest one
		// to recursively check against and only check the second one if there is
		// not a hit or the hit returned is further out than the second node
		if (bIsLeaf == 0)
		{
			// Holds the indices for the closest and farthest nodes
			INT NearNode = -1;
			INT FarNode = -1;
			// Holds the hit times for the child nodes
			FLOAT NodeHitTime, NearTime = 0.f, FarTime = 0.f;
			// Assume the left node is closer (it will be adjusted later)
			if (Check.Nodes(n.LeftNode).BoundingVolume.LineCheck(Check.LCI,NodeHitTime))
			{
				NearNode = n.LeftNode;
				NearTime = NodeHitTime;
			}
			// Find out if the second node is closer
			if (Check.Nodes(n.RightNode).BoundingVolume.LineCheck(Check.LCI,NodeHitTime))
			{
				// See if the left node was a miss and make the right the near node
				if (NearNode == -1)
				{
					NearNode = n.RightNode;
					NearTime = NodeHitTime;
				}
				else
				{
					FarNode = n.RightNode;
					FarTime = NodeHitTime;
				}
			}
			// Swap the Near/FarNodes if the right node is closer than the left
			if (NearNode != -1 && FarNode != -1 && FarTime < NearTime)
			{
				Exchange(NearNode,FarNode);
				Exchange(NearTime,FarTime);
			}
			// See if we need to search the near node or not
			if (NearNode != -1 && Check.LCI.Result->Time > NearTime)
			{
				bHit = Check.Nodes(NearNode).LineCheck(Check);
			}
			// Check for an early out
			const UBOOL bStopAtAnyHit = Check.LCI.TraceFlags & TRACE_StopAtAnyHit;
			// Now do the same for the far node. This will only happen if a miss in
			// the near node or the nodes overlapped and this one is closer
			if (FarNode != -1 &&
				(Check.LCI.Result->Time > FarTime || bHit == FALSE) &&
				(bHit == FALSE || bStopAtAnyHit == FALSE))
			{
				bHit |= Check.Nodes(FarNode).LineCheck(Check);
			}
		}
		else
		{
			// This is a leaf, check the triangles for a hit
			bHit = LineCheckTriangles(Check);
		}
		return bHit;
	}

	/**
	 * Works through the list of triangles in this node checking each one for a
	 * collision.
	 *
	 * @param Check -- The aggregated line check data
	 */
	KDOP_INLINE UBOOL LineCheckTriangles(TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// Assume a miss
		UBOOL bHit = FALSE;
		// Check for an early out
		const UBOOL bStopAtAnyHit = Check.LCI.TraceFlags & TRACE_StopAtAnyHit;
		// Loop through all of our triangles. We need to check them all in case
		// there are two (or more) potential triangles that would collide and let
		// the code choose the closest
		for (KDOP_IDX_TYPE CollTriIndex = t.StartIndex;
			CollTriIndex < t.StartIndex + t.NumTriangles &&
			(bHit == FALSE || bStopAtAnyHit == FALSE);
			CollTriIndex++)
		{
			// Get the collision triangle that we are checking against
			const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri =	Check.CollisionTriangles(CollTriIndex);
			if(Check.CollDataProvider.ShouldCheckMaterial(CollTri.MaterialIndex))
			{
				// Now check for an intersection
				bHit |= LineCheckTriangle(Check,CollTri,CollTri.MaterialIndex);
			}
		}
		return bHit;
	}

	/**
	 * Performs collision checking against the triangle using the old collision
	 * code to handle it. This is done to provide consistency in collision.
	 *
	 * @param Check -- The aggregated line check data
	 * @param CollTri -- The triangle to check for intersection
	 * @param MaterialIndex -- The material for this triangle if it is hit
	 */
#if 1
	template<typename CHECK>
	static UBOOL LineCheckTriangle(CHECK& Check,
		const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri, KDOP_IDX_TYPE MaterialIndex)
	{
		// Get refs to the 3 verts of the triangle check against
		const FVector& v1 = Check.CollDataProvider.GetVertex(CollTri.v1);
		const FVector& v2 = Check.CollDataProvider.GetVertex(CollTri.v2);
		const FVector& v3 = Check.CollDataProvider.GetVertex(CollTri.v3);

		// Calculate the hit time the same way the old code
		// did so things are the same
		const FPlane TrianglePlane(v1,(v2 - v3) ^ (v1 - v3));
		const FLOAT StartDist = TrianglePlane.PlaneDot(Check.LCI.LocalStart);
		const FLOAT EndDist = TrianglePlane.PlaneDot(Check.LCI.LocalEnd);
		if ((StartDist < -0.001f && EndDist < -0.001f) || (StartDist > 0.001f && EndDist > 0.001f))
		{
			return FALSE;
		}
		// Figure out when it will hit the triangle
		FLOAT Time = StartDist / (StartDist - EndDist);
		// If this triangle is not closer than the previous hit, reject it
		if (!(Time >= 0.f && Time < Check.LCI.Result->Time)) 
		{
			return FALSE;
		}
		// Calculate the line's point of intersection with the node's plane
		const FVector Intersection = Check.LCI.LocalStart + Check.LCI.LocalDir * Time;

		const FVector SideDirection1 = TrianglePlane ^ (v2 - v1);
		const FLOAT SideW1 = SideDirection1 | v1;
		const UBOOL Test1 = ((SideDirection1 | Intersection) - SideW1) >= 0.001f;

		const FVector SideDirection2 = TrianglePlane ^ (v3 - v2);
		const FLOAT SideW2 = SideDirection2 | v2;
		const UBOOL Test2 = ((SideDirection2 | Intersection) - SideW2) >= 0.001f;

		const FVector SideDirection3 = TrianglePlane ^ (v1 - v3);
		const FLOAT SideW3 = SideDirection3 | v3;
		const UBOOL Test3 = ((SideDirection3 | Intersection) - SideW3) >= 0.001f;

		if ( Test1 | Test2 | Test3 )  // bitwise to avoid excessive branches
		{
			return FALSE;
		}
		// Return results
		Check.LCI.LocalHitNormal = TrianglePlane.SafeNormal();
		Check.LCI.Result->Time = Time;
		Check.LCI.Result->Material = Check.CollDataProvider.GetMaterial(MaterialIndex);
		Check.LCI.Result->Item = Check.CollDataProvider.GetItemIndex(MaterialIndex);
		// The hit material may have a physical material mask.  Check which physical material off the mask should be used.
		Check.LCI.Result->PhysMaterial = TkDOPPhysicalMaterialCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>::DetermineMaskedPhysicalMaterial( Check.CollDataProvider, Intersection, CollTri, MaterialIndex );
		
		return TRUE;
	}
#else
	template<typename CHECK>
	static UBOOL LineCheckTriangle(CHECK& Check,
		const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri, KDOP_IDX_TYPE MaterialIndex)
	{
		// Get refs to the 3 verts of the triangle check against
		const FVector& v1 = Check.CollDataProvider.GetVertex(CollTri.v1);
		const FVector& v2 = Check.CollDataProvider.GetVertex(CollTri.v2);
		const FVector& v3 = Check.CollDataProvider.GetVertex(CollTri.v3);

		// Calculate the hit normal the same way the old code
		// did so things are the same
		const FVector& LocalNormal = ((v2 - v3) ^ (v1 - v3)).SafeNormal();
		// Calculate the hit time the same way the old code
		// did so things are the same
		FPlane TrianglePlane(v1,LocalNormal);
		const FLOAT StartDist = TrianglePlane.PlaneDot(Check.LCI.LocalStart);
		const FLOAT EndDist = TrianglePlane.PlaneDot(Check.LCI.LocalEnd);
		if ((StartDist == EndDist) || (StartDist < -0.001f && EndDist < -0.001f) || (StartDist > 0.001f && EndDist > 0.001f))
		{
			return FALSE;
		}
		// Figure out when it will hit the triangle
		FLOAT Time = -StartDist / (EndDist - StartDist);
		// If this triangle is not closer than the previous hit, reject it
		if (!(Time >= 0.f && Time < Check.LCI.Result->Time)) 
		{
			return FALSE;
		}
		// Calculate the line's point of intersection with the node's plane
		const FVector& Intersection = Check.LCI.LocalStart + Check.LCI.LocalDir * Time;
		const FVector* Verts[3] = 
		{ 
			&v1, &v2, &v3
		};
		// Check if the point of intersection is inside the triangle's edges.
		for( INT SideIndex = 0; SideIndex < 3; SideIndex++ )
		{
			const FVector& SideDirection = LocalNormal ^
				(*Verts[(SideIndex + 1) % 3] - *Verts[SideIndex]);
			const FLOAT SideW = SideDirection | *Verts[SideIndex];
			if (((SideDirection | Intersection) - SideW) >= 0.001f)
			{
				return FALSE;
			}
		}
		// Return results
		Check.LCI.LocalHitNormal = LocalNormal;
		Check.LCI.Result->Time = Time;
		Check.LCI.Result->Material = Check.CollDataProvider.GetMaterial(MaterialIndex);
		Check.LCI.Result->Item = Check.CollDataProvider.GetItemIndex(MaterialIndex);
		// The hit material may have a physical material mask.  Check which physical material off the mask should be used.
		Check.LCI.Result->PhysMaterial = TkDOPPhysicalMaterialCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>::DetermineMaskedPhysicalMaterial( Check.CollDataProvider, Intersection, CollTri, MaterialIndex );
		
		return TRUE;
	}
#endif
	/**
	 * Determines the line + extent in the FkDOPBoxCollisionCheck intersects this
	 * node. It also will check the child nodes if it is not a leaf, otherwise it
	 * will check against the triangle data.
	 *
	 * @param Check -- The aggregated box check data
	 */
	UBOOL BoxCheck(TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Check) const
	{
		UBOOL bHit = FALSE;
		// If this is a node, check the two child nodes and pick the closest one
		// to recursively check against and only check the second one if there is
		// not a hit or the hit returned is further out than the second node
		if (bIsLeaf == 0)
		{
			// Holds the indices for the closest and farthest nodes
			INT NearNode = -1;
			INT FarNode = -1;
			// Holds the hit times for the child nodes
			FLOAT NodeHitTime = 0.f, NearTime = 0.f, FarTime = 0.f;
			// Update the kDOP with the extent and test against that
			kDOPType kDOPNear(Check.Nodes(n.LeftNode).BoundingVolume,
				Check.LocalExtent);
			// Assume the left node is closer (it will be adjusted later)
			if (kDOPNear.LineCheck(Check.LCI,NodeHitTime))
			{
				NearNode = n.LeftNode;
				NearTime = NodeHitTime;
			}
			// Update the kDOP with the extent and test against that
			kDOPType kDOPFar(Check.Nodes(n.RightNode).BoundingVolume,
				Check.LocalExtent);
			// Find out if the second node is closer
			if (kDOPFar.LineCheck(Check.LCI,NodeHitTime))
			{
				// See if the left node was a miss and make the right the near node
				if (NearNode == -1)
				{
					NearNode = n.RightNode;
					NearTime = NodeHitTime;
				}
				else
				{
					FarNode = n.RightNode;
					FarTime = NodeHitTime;
				}
			}
			// Swap the Near/FarNodes if the right node is closer than the left
			if (NearNode != -1 && FarNode != -1 && FarTime < NearTime)
			{
				Exchange(NearNode,FarNode);
				Exchange(NearTime,FarTime);
			}
			// See if we need to search the near node or not
			if (NearNode != -1 && Check.LCI.Result->Time > NearTime)
			{
				bHit = Check.Nodes(NearNode).BoxCheck(Check);
			}
			// Check for an early out
			const UBOOL bStopAtAnyHit = Check.LCI.TraceFlags & TRACE_StopAtAnyHit;
			// Now do the same for the far node. This will only happen if a miss in
			// the near node or the nodes overlapped and this one is closer
			if (FarNode != -1 &&
				(Check.LCI.Result->Time > FarTime || bHit == FALSE) &&
				(bHit == FALSE || bStopAtAnyHit == FALSE))
			{
				bHit |= Check.Nodes(FarNode).BoxCheck(Check);
			}
		}
		else
		{
			// This is a leaf, check the triangles for a hit
			bHit = BoxCheckTriangles(Check);
		}
		return bHit;
	}

	/**
	 * Works through the list of triangles in this node checking each one for a
	 * collision.
	 *
	 * @param Check -- The aggregated box check data
	 */
	KDOP_INLINE UBOOL BoxCheckTriangles(TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Check) const
	{
		// Assume a miss
		UBOOL bHit = 0;
		// Use an early out if possible
		const UBOOL bStopAtAnyHit = Check.LCI.TraceFlags & TRACE_StopAtAnyHit;
		// Loop through all of our triangles. We need to check them all in case
		// there are two (or more) potential triangles that would collide and let
		// the code choose the closest
		for (KDOP_IDX_TYPE CollTriIndex = t.StartIndex;
			CollTriIndex < t.StartIndex + t.NumTriangles &&
			(bHit == FALSE || bStopAtAnyHit == FALSE);
			CollTriIndex++)
		{
			// Get the collision triangle that we are checking against
			const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri = Check.CollisionTriangles(CollTriIndex);
			if(Check.CollDataProvider.ShouldCheckMaterial(CollTri.MaterialIndex))
			{
				// Now check for an intersection using the Separating Axis Theorem
				bHit |= BoxCheckTriangle(Check,CollTri,CollTri.MaterialIndex);
			}
		}
		return bHit;
	}

	/**
	 * Uses the separating axis theorem to check for triangle box collision.
	 *
	 * @param Check -- The aggregated box check data
	 * @param v1 -- The first vertex of the triangle
	 * @param v2 -- The second vertex of the triangle
	 * @param v3 -- The third vertex of the triangle
	 * @param MaterialIndex -- The material for this triangle if it is hit
	 */
	template<typename CHECK>
	static KDOP_INLINE UBOOL BoxCheckTriangle(CHECK& Check,
		const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri, INT MaterialIndex)
	{
		FLOAT HitTime = 1.f;
		FVector HitNormal(0.f,0.f,0.f);

		// Get refs to the 3 verts of the triangle check against
		const FVector& v1 = Check.CollDataProvider.GetVertex(CollTri.v1);
		const FVector& v2 = Check.CollDataProvider.GetVertex(CollTri.v2);
		const FVector& v3 = Check.CollDataProvider.GetVertex(CollTri.v3);

		// Now check for an intersection using the Separating Axis Theorem
		UBOOL Result = FindSeparatingAxis(v1,v2,v3,Check.LCI.LocalStart,
			Check.LCI.LocalEnd,Check.Extent,Check.LocalBoxX,Check.LocalBoxY,
			Check.LocalBoxZ,HitTime,HitNormal);
		if (Result)
		{
			if (HitTime < Check.LCI.Result->Time)
			{
				// Store the better time
				Check.LCI.Result->Time = HitTime;
				// Get the material and item that was hit
				Check.LCI.Result->Material = Check.CollDataProvider.GetMaterial(MaterialIndex);
				Check.LCI.Result->Item = Check.CollDataProvider.GetItemIndex(MaterialIndex);
				// Normal will get transformed to world space at end of check
				Check.LCI.LocalHitNormal = HitNormal;
				
				// Calculate the intersection of the line check and the triangle
				const FVector Intersection = Check.LCI.LocalStart + (Check.LCI.LocalEnd - Check.LCI.LocalStart) * HitTime;
				// The hit material may have a physical material mask.  Check which physical material off the mask should be used.
				Check.LCI.Result->PhysMaterial = TkDOPPhysicalMaterialCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>::DetermineMaskedPhysicalMaterial( Check.CollDataProvider, Intersection, CollTri, MaterialIndex );
			}
			else
			{
				Result = FALSE;
			}
		}
		return Result;
	}

	/**
	 * Determines the point + extent in the FkDOPPointCollisionCheck intersects
	 * this node. It also will check the child nodes if it is not a leaf, otherwise
	 * it will check against the triangle data.
	 *
	 * @param Check -- The aggregated point check data
	 */
	UBOOL PointCheck(TkDOPPointCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Check) const
	{
		UBOOL bHit = FALSE;
		// If this is a node, check the two child nodes recursively
		if (bIsLeaf == 0)
		{
			// Holds the indices for the closest and farthest nodes
			INT NearNode = -1;
			INT FarNode = -1;
			// Update the kDOP with the extent and test against that
			kDOPType kDOPNear(Check.Nodes(n.LeftNode).BoundingVolume,
				Check.LocalExtent);
			// Assume the left node is closer (it will be adjusted later)
			if (kDOPNear.PointCheck(Check.LCI))
			{
				NearNode = n.LeftNode;
			}
			// Update the kDOP with the extent and test against that
			kDOPType kDOPFar(Check.Nodes(n.RightNode).BoundingVolume,
				Check.LocalExtent);
			// Find out if the second node is closer
			if (kDOPFar.PointCheck(Check.LCI))
			{
				// See if the left node was a miss and make the right the near node
				if (NearNode == -1)
				{
					NearNode = n.RightNode;
				}
				else
				{
					FarNode = n.RightNode;
				}
			}
			// See if we need to search the near node or not
			if (NearNode != -1)
			{
				bHit = Check.Nodes(NearNode).PointCheck(Check);
			}
			// Now do the same for the far node
			if (FarNode != -1)
			{
				bHit |= Check.Nodes(FarNode).PointCheck(Check);
			}
		}
		else
		{
			// This is a leaf, check the triangles for a hit
			bHit = PointCheckTriangles(Check);
		}
		return bHit;
	}

	/**
	 * Works through the list of triangles in this node checking each one for a
	 * collision.
	 *
	 * @param Check -- The aggregated point check data
	 */
	KDOP_INLINE UBOOL PointCheckTriangles(TkDOPPointCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Check) const
	{
		// Assume a miss
		UBOOL bHit = FALSE;
		// Loop through all of our triangles. We need to check them all in case
		// there are two (or more) potential triangles that would collide and let
		// the code choose the closest
		for (KDOP_IDX_TYPE CollTriIndex = t.StartIndex;
			CollTriIndex < t.StartIndex + t.NumTriangles;
			CollTriIndex++)
		{
			// Get the collision triangle that we are checking against
			const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri =	Check.CollisionTriangles(CollTriIndex);
			if(Check.CollDataProvider.ShouldCheckMaterial(CollTri.MaterialIndex))
			{
				// Now get refs to the 3 verts to check against
				const FVector& v1 = Check.CollDataProvider.GetVertex(CollTri.v1);
				const FVector& v2 = Check.CollDataProvider.GetVertex(CollTri.v2);
				const FVector& v3 = Check.CollDataProvider.GetVertex(CollTri.v3);
				// Now check for an intersection using the Separating Axis Theorem
				bHit |= PointCheckTriangle(Check,v1,v2,v3,CollTri.MaterialIndex);
			}
		}
		return bHit;
	}

	/**
	 * Uses the separating axis theorem to check for triangle box collision.
	 *
	 * @param Check -- The aggregated box check data
	 * @param v1 -- The first vertex of the triangle
	 * @param v2 -- The second vertex of the triangle
	 * @param v3 -- The third vertex of the triangle
	 * @param MaterialIndex -- The material for this triangle if it is hit
	 */
	template<typename CHECK>
	static KDOP_INLINE UBOOL PointCheckTriangle(CHECK& Check,
		const FVector& v1,const FVector& v2,const FVector& v3,INT InMaterialIndex)
	{
		// Use the separating axis theorem to see if we hit
		FSeparatingAxisPointCheck ThePointCheck(v1,v2,v3,Check.LCI.LocalStart,Check.Extent,
			Check.LocalBoxX,Check.LocalBoxY,Check.LocalBoxZ,Check.BestDistance);

		// If we hit and it is closer update the out values
		if (ThePointCheck.Hit && ThePointCheck.BestDist < Check.BestDistance)
		{
			// Get the material and item that was hit
			Check.LCI.Result->Material = Check.CollDataProvider.GetMaterial(InMaterialIndex);
			Check.LCI.Result->Item = Check.CollDataProvider.GetItemIndex(InMaterialIndex);
			// Normal will get transformed to world space at end of check
			Check.LCI.LocalHitNormal = ThePointCheck.HitNormal;
			// Copy the distance for push out calculations
			Check.BestDistance = ThePointCheck.BestDist;
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Find triangles that overlap the given sphere. We assume that the supplied box overlaps this node.
	 *
	 * @param Query -- Query information
	 */
	KDOP_INLINE void SphereQuery(TkDOPSphereQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Query) const
	{
		// If not leaf, check against each child.
		if(bIsLeaf == 0)
		{
			if( Query.Nodes(n.LeftNode).BoundingVolume.AABBOverlapCheck(Query.LocalBox) )
			{
				Query.Nodes(n.LeftNode).SphereQuery(Query);
			}

			if( Query.Nodes(n.RightNode).BoundingVolume.AABBOverlapCheck(Query.LocalBox) )
			{
				Query.Nodes(n.RightNode).SphereQuery(Query);
			}
		}
		else // Otherwise, add all the triangles in this node to the list.
		{
			for (KDOP_IDX_TYPE TriIndex = t.StartIndex;
				TriIndex < t.StartIndex + t.NumTriangles; TriIndex++)
			{
				Query.ReturnTriangles.AddItem(TriIndex);
			}
		}

	}

	/**
	 * Find triangles that overlap the given AABB. We assume that the supplied box overlaps this node.
	 *
	 * @param Query -- Query information
	 */
	void AABBQuery(TkDOPAABBQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Query) const
	{
		// If not leaf, check against each child.
		if(bIsLeaf == 0)
		{
			if( Query.Nodes(n.LeftNode).BoundingVolume.AABBOverlapCheck(Query.LocalBox) )
			{
				Query.Nodes(n.LeftNode).AABBQuery(Query);
			}

			if( Query.Nodes(n.RightNode).BoundingVolume.AABBOverlapCheck(Query.LocalBox) )
			{
				Query.Nodes(n.RightNode).AABBQuery(Query);
			}
		}
		else // Otherwise, add all the triangles in this node to the list.
		{
			for (KDOP_IDX_TYPE TriIndex = t.StartIndex;
				TriIndex < t.StartIndex + t.NumTriangles; TriIndex++)
			{
				const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri =	Query.CollisionTriangles(TriIndex);
				if(Query.CollDataProvider.ShouldCheckMaterial(CollTri.MaterialIndex))
				{
					Query.ReturnTriangles.AddItem(TriIndex);
				}
			}
		}

	}

	/**
	 * Find nodes that overlap the given frustum. We assume that the supplied
	 * frustum overlaps this node.
	 *
	 * input:	Query -- Query information
	 */
	UBOOL FrustumQuery( TkDOPFrustumQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Query ) const
	{
		if( !BoundingVolume.FrustumCheck(Query.LocalFrustumPlanes) )
			return 0;

		// left node
		if( Query.Nodes.IsValidIndex( n.LeftNode ) )
		{
			const NodeType& Left = Query.Nodes(n.LeftNode);
			if( !Left.bIsLeaf )
			{
				// recurse
				Left.FrustumQuery( Query );
			}
			else
			{
				// add index to this leaf node
				Query.AddTriangleRun( Left.t.StartIndex, Left.t.NumTriangles );
			}
		}

		// right node
		if( Query.Nodes.IsValidIndex( n.RightNode ) )
		{
			const NodeType& Right = Query.Nodes(n.RightNode);
			if( !Right.bIsLeaf )
			{
				// recurse
				Right.FrustumQuery( Query );
			}
			else
			{
				// add index to this leaf node
				Query.AddTriangleRun( Right.t.StartIndex, Right.t.NumTriangles );
			}
		}

		return 1;
	}

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,NodeType& Node)
	{
		// @warning BulkSerialize: FkDOPNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << Node.BoundingVolume << Node.bIsLeaf;
		// If we are a leaf node, serialize out the child node indices, otherwise serialize out the triangle collision data.
		// n.LeftNode overlaps t.NumTriangles in memory
		// n.RightNode overlaps t.StartIndex in memory
		Ar << Node.n.LeftNode << Node.n.RightNode;
		return Ar;
	}
};

/**
 * This is the tree of kDOPs that spatially divides the static mesh. It is
 * a binary tree of kDOP nodes.
 */
template<typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE> struct TkDOPTree
{
	/** Exposes data provider type to clients. */
	typedef COLL_DATA_PROVIDER							DataProviderType;

	/** Exposes node type to clients. */
	typedef TkDOPNode<DataProviderType,KDOP_IDX_TYPE>	NodeType;

	/** Exposes kDOP type to clients. */
	typedef typename NodeType::kDOPType					kDOPType;

	/** The list of nodes contained within this tree. Node 0 is always the root node. */
	TArray<NodeType> Nodes;

	/** The list of collision triangles in this tree. */
	TArray<FkDOPCollisionTriangle<KDOP_IDX_TYPE> > Triangles;

	/**
	 * Creates the root node and recursively splits the triangles into smaller
	 * volumes
	 *
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 */
	void Build(TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles)
	{
		// Empty the current set of nodes and preallocate the memory so it doesn't
		// reallocate memory while we are recursively walking the tree
		Nodes.Empty(BuildTriangles.Num() * 2);
		// Add the root node
		Nodes.Add();
		// Now tell that node to recursively subdivide the entire set of triangles
		Nodes(0).SplitTriangleList(0,BuildTriangles.Num(),BuildTriangles,Nodes);
		// Don't waste memory.
		Nodes.Shrink();
		// Copy over the triangle information afterward, since they will have
		// been sorted into their bounding volumes at this point
		Triangles.Empty(BuildTriangles.Num());
		Triangles.Add(BuildTriangles.Num());
		// Copy the triangles from the build list into the full list
		for (INT nIndex = 0; nIndex < BuildTriangles.Num(); nIndex++)
		{
			Triangles(nIndex) = BuildTriangles(nIndex);
		}
	}

	/**
	 * Remaps the indices of the tree and its nodes to match the provided mapping table,
	 * in case the underlying mesh's vertices get reordered.
	 * 
	 * e.g. NewIndex[i] = IndexRemapping[OldIndex[i]]
	 */
	void RemapIndices(const TArray<INT>& IndexRemapping)
	{
		for(INT TriangleIndex = 0; TriangleIndex < Triangles.Num(); ++TriangleIndex)
		{
			KDOP_IDX_TYPE index1 = Triangles(TriangleIndex).v1;
			KDOP_IDX_TYPE index2 = Triangles(TriangleIndex).v2;
			KDOP_IDX_TYPE index3 = Triangles(TriangleIndex).v3;

			Triangles(TriangleIndex).v1 = IndexRemapping(index1);
			Triangles(TriangleIndex).v2 = IndexRemapping(index2);
			Triangles(TriangleIndex).v3 = IndexRemapping(index3);
		}
	}

	/**
	 * Returns the root bound of the tree
	 *
	 * @param OutBox if the tree is valid and has nodes, this gives the root bounding box
	 * @return TRUE if OutBox was filled in, FALSE if the try is empty or otheriwse unusable
	 */
	UBOOL GetRootBound(FBox &OutBox)
	{
		if (Triangles.Num() > 0)
		{
			check(Nodes.Num() > 0);
			OutBox = Nodes(0).BoundingVolume.ToFBox();
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated line check data
	 */
	UBOOL LineCheck(TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		UBOOL bHit = FALSE;
		FLOAT HitTime;
		// Check against the first bounding volume and decide whether to go further
		if (Nodes(0).BoundingVolume.LineCheck(Check.LCI,HitTime))
		{
			// Recursively check for a hit
			bHit = Nodes(0).LineCheck(Check);
		}
		return bHit;
	}

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated box check data
	 */
	UBOOL BoxCheck(TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Check) const
	{
		UBOOL bHit = FALSE;
		FLOAT HitTime;
		// Check the root node's bounding volume expanded by the extent
		kDOPType kDOP(Nodes(0).BoundingVolume,Check.LocalExtent);
		// Check against the first bounding volume and decide whether to go further
		if (kDOP.LineCheck(Check.LCI,HitTime))
		{
			// Recursively check for a hit
			bHit = Nodes(0).BoxCheck(Check);
		}
		return bHit;
	}

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated point check data
	 */
	UBOOL PointCheck(TkDOPPointCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Check) const
	{
		UBOOL bHit = FALSE;
		// Check the root node's bounding volume expanded by the extent
		kDOPType kDOP(Nodes(0).BoundingVolume,Check.LocalExtent);
		// Check against the first bounding volume and decide whether to go further
		if (kDOP.PointCheck(Check.LCI))
		{
			// Recursively check for a hit
			bHit = Nodes(0).PointCheck(Check);
		}
		return bHit;
	}

	/**
	 * Find all triangles in static mesh that overlap a supplied bounding sphere.
	 *
	 * @param Query -- The aggregated sphere query data
	 */
	void SphereQuery(TkDOPSphereQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Query) const
	{
		// Check the query box overlaps the root node KDOP. If so, run query recursively.
		if( Query.Nodes(0).BoundingVolume.AABBOverlapCheck( Query.LocalBox ) )
		{
			Query.Nodes(0).SphereQuery( Query );
		}
	}

	/**
	 * Find all triangles in static mesh that overlap a supplied AABB.
	 *
	 * @param Query -- The aggregated AABB query data
	 */
	void AABBQuery(TkDOPAABBQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Query) const
	{
		// Check the query box overlaps the root node KDOP. If so, run query recursively.
		if( Query.Nodes(0).BoundingVolume.AABBOverlapCheck( Query.LocalBox ) )
		{
			Query.Nodes(0).AABBQuery( Query );
		}
	}

	/**
	 * Find all kdop nodes in static mesh that overlap given frustum.
	 * This is just the entry point to the tree
	 *
	 * @param Query -- The aggregated frustum query data
	 */
	UBOOL FrustumQuery( TkDOPFrustumQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Query ) const
	{
		if ( Query.Nodes.Num() > 1 )
		{
			// Begin at the root.
			Query.Nodes(0).FrustumQuery( Query );
		}
		else if( Query.Nodes.Num() == 1 )
		{
			// Special case for when the kDOP tree contains only a single node (and thus that node is a leaf).
			if( Query.Nodes(0).BoundingVolume.FrustumCheck(Query.LocalFrustumPlanes) )
			{
				Query.AddTriangleRun(Query.Nodes(0).t.StartIndex, Query.Nodes(0).t.NumTriangles);
			}
		}
		// The query was successful only if there were intersecting nodes.
		return Query.ReturnRuns.Num() > 0;
	}

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,TkDOPTree<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Tree)
	{
		Tree.Nodes.BulkSerialize( Ar );
		Tree.Triangles.BulkSerialize( Ar );
		return Ar;
	}

	/**
	 * Dumps the kDOPTree 
	 */
	void Dump() const
	{
		debugf(TEXT("kDOPTree [%x], %d nodes, %d triangles"), this, Nodes.Num(), Triangles.Num());
		for (INT TriIndex = 0; TriIndex < Triangles.Num(); TriIndex++)
		{
			debugf(TEXT(" Tri %03d: %d, %d, %d"), TriIndex, Triangles(TriIndex).v1, Triangles(TriIndex).v2, Triangles(TriIndex).v3);
		}
		debugf(TEXT(""));
		for (INT NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
		{
			const NodeType& Node = Nodes(NodeIndex);
			debugf(TEXT(" Node %03d: [%.2f, %.2f, %.2f :: %.2f, %.2f, %.2f]  %s"), NodeIndex, 
				Node.BoundingVolume.Min[0], Node.BoundingVolume.Min[1], Node.BoundingVolume.Min[2], 
				Node.BoundingVolume.Max[0], Node.BoundingVolume.Max[1], Node.BoundingVolume.Max[2], 
				Node.bIsLeaf ? TEXT("leaf") : TEXT("nonleaf"));
			if (Node.bIsLeaf)
			{
				debugf(TEXT("  StartIndex = %d, NumTris = %d"), Node.t.StartIndex, Node.t.NumTriangles);
			}
			else
			{
				debugf(TEXT("  LeftChild = %d, RightChild= %d"), Node.n.LeftNode, Node.n.RightNode);
			}
		}
		debugf(TEXT(""));
		debugf(TEXT(""));
	}

};


/**
 * Very similar to TkDOP, only a compressed compact version
 * Encodes the bounding boxes of two child nodes relative to the parent BBox, which isn't stored
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE> struct TkDOPCompact :
	public FkDOPPlanes
{
	/** Exposes data provider type to clients. */
	typedef COLL_DATA_PROVIDER DataProviderType;

	/** This is just the data type of the uncompressed node */
	typedef TkDOP<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> BoundType;

	/**
	 * Encoded Min planes for this bounding volume
	 */
	BYTE Min[NUM_PLANES];
	/**
	 * Encoded Max planes for this bounding volume
	 */
	BYTE Max[NUM_PLANES];

	// these are ones-complement signed bytes, but we deal with that explicitly

	/**
	 * Initializes the planes to invalid states so that adding any point
	 * makes the volume update
	 */
	KDOP_INLINE TkDOPCompact()
	{
		Init();
	}

	/**
	 * Sets the data to an invalid state
	 */
	KDOP_INLINE void Init(void)
	{
		for (INT PlaneIndex = 0; PlaneIndex < NUM_PLANES; PlaneIndex++)
		{
			Min[PlaneIndex] = 0;
			Max[PlaneIndex] = 0;
		}
	}

	/**
	 * Compressed a component of an axis
	 * @param TargetBound, element of the parents bound; if computing a min, then this is the min of the parent bound on this axis
	 * @param OtherBound, element of the parents bound; if computing a min, then this is the max of the parent bound on this axis
	 * @param LeftChild, left childs corresponding bound for this axis; if computing a min, then this is the min of the left child bound on this axis
	 * @param RightChild, right childs corresponding bound for this axis; if computing a min, then this is the min of the left child bound on this axis
	 * @return compressed value for the axis and bound
	 */
	static KDOP_INLINE BYTE CompressAxis(FLOAT TargetBound, FLOAT OtherBound, FLOAT LeftChild, FLOAT RightChild)
	{
		FLOAT Range = (OtherBound - TargetBound);
		if (Range == 0.0f)
		{
			// we are flat, so it doesn't matter what we do
			return 1;
		}
		FLOAT LFrac = (LeftChild - TargetBound) / Range;
		FLOAT RFrac = (RightChild - TargetBound) / Range;
#if TEST_COMPACT_KDOP
		check(LFrac >= 0.0f && LFrac <= 1.0f);
		check(RFrac >= 0.0f && RFrac <= 1.0f);
#endif
		INT Result;
		if (LFrac >= RFrac)
		{  // left is the "new" value
			Result = 128 + ::Max<INT>(appFloor(127.0f * LFrac) - 1, 0); 
		}
		else
		{
			Result = 127 - ::Max<INT>(appFloor(127.0f * RFrac) - 1, 0);
		}
		check(Result > 0 && Result < 255);
		return BYTE(Result);
	}
#if COMPACT_KDOP_SIMD_DECOMPRESSION
	/**
	 * Given a parent BBox, computes the two child bboxes based on the encoded data I hold SIMD
	 * @param Me, bounding box of this node
	 * @param OutLeftChild, computed bounding box of the left child
	 * @param OutRightChild, computed bounding box of the right child
	 */
	void KDOP_INLINE Decompress(const BoundType& Me,BoundType& OutLeftChild,BoundType& OutRightChild) const
	{
#if TEST_COMPACT_KDOP_SLOW
		check((PTRINT(Min) & 3) == 0 || (PTRINT(Min) & 3) == 2); // we only support two possible alignments of the compressed input data
		check(Min + 3 == Max); // these two must be adjacent and in this order

		check(VectorIsAligned(Me.Min)); // must be aligned for SIMD
		check(Me.Min + 3 == Me.Max); // must be adjacent and in this order
		check(VectorIsAligned(OutLeftChild.Min)); // must be aligned for SIMD
		check(OutLeftChild.Min + 3 == OutLeftChild.Max); // must be adjacent and in this order
		check(VectorIsAligned(OutRightChild.Min)); // must be aligned for SIMD
		check(OutRightChild.Min + 3 == OutRightChild.Max); // must be adjacent and in this order
#endif
		// round down to the nearest DWORD. This looks trickier than it is because I wasn't sure ~3 would give the right mask for 64 bit
		// this really can't page fault. node 0 should be mod 4 == 0 and that is check'd elsewhere
		const BYTE* RESTRICT MinPtr = (const BYTE*)((PTRINT(Min)&3)^PTRINT(Min));  
		VectorRegister VMaskMod4EQ0 =  KDopSIMD::VAlignMasks[(PTRINT(Min) & 2) >> 1]; // if -1, then we were 4 byte aligned, else we are %4==2

		const FLOAT* RESTRICT MeMinPtr = Me.Min;
		FLOAT* RESTRICT OutLeftMinPtr = OutLeftChild.Min;
		FLOAT* RESTRICT OutRightMinPtr = OutRightChild.Min;
		// load up Me bbox here; it can be stuffed into unused execution slots in the critical path below
		VectorRegister VMeMin = VectorLoadAligned(MeMinPtr);
		VectorRegister TempMeMax = VectorLoadAligned(MeMinPtr + 4);
		VectorRegister TempMeMaxFromMin = VectorSwizzle(VMeMin,3,0,0,0); // 0 = junk
		VectorRegister VMeMax = VectorSelect(KDopSIMD::VMaxMergeMask,VectorSwizzle(TempMeMax,3,0,1,3),TempMeMaxFromMin); // 3 = junk

		// Extent and negative extent, also can fill execution slots
		VectorRegister VRange = VectorSubtract(VMeMax,VMeMin);
		VectorRegister VRangeNeg = VectorSubtract(VMeMin,VMeMax);

		VectorRegister VComp1 = VectorLoadByte4(MinPtr);
		VectorRegister VComp2 = VectorLoadByte4(MinPtr + 4);

		// mod 4 == 0 path
		VectorRegister VMinMod4EQ0 = VComp1;
		VectorRegister TempMaxFromMin = VectorSwizzle(VMinMod4EQ0,3,0,0,0); // 0 = junk
		VectorRegister VMaxMod4EQ0 = VectorSelect(KDopSIMD::VMaxMergeMask,VectorSwizzle(VComp2,3,0,1,3),TempMaxFromMin); // 3 = junk

		// mod 4 == 2 path
		// note that MinPtr has been aligned, above
		VectorRegister TempMin = VectorSwizzle(VComp1,2,3,0,0); // 0 = junk 
		VectorRegister VMaxMod4EQ2 = VComp2; 
		VectorRegister TempMinFromMax = VectorSwizzle(VMaxMod4EQ2,3,3,0,3); // 3 = junk
		VMaxMod4EQ2 = VectorSwizzle(VMaxMod4EQ2,1,2,3,0); // 0 = junk
		VectorRegister VMinMod4EQ2 = VectorSelect(KDopSIMD::VMinMergeMask,TempMin,TempMinFromMax); 

		VectorRegister VMin = VectorSelect(VMaskMod4EQ0,VMinMod4EQ0,VMinMod4EQ2);
		VectorRegister VMax = VectorSelect(VMaskMod4EQ0,VMaxMod4EQ0,VMaxMod4EQ2);

		VectorRegister SignedMin = VectorSubtract(VMin,KDopSIMD::V_127p5);
		VectorRegister SignedMax = VectorSubtract(VMax,KDopSIMD::V_127p5);

		VectorRegister SignedMinMaxp5 = VectorMax(SignedMin,KDopSIMD::V_p5);
		VectorRegister SignedMaxMaxp5 = VectorMax(SignedMax,KDopSIMD::V_p5);
		VectorRegister SignedMinMinnp5 = VectorMin(SignedMin,KDopSIMD::V_p5Neg);
		VectorRegister SignedMaxMinnp5 = VectorMin(SignedMax,KDopSIMD::V_p5Neg);

		VectorRegister LeftFracMin = VectorMultiplyAdd(SignedMinMaxp5,KDopSIMD::V_127Inv,KDopSIMD::V_p5Neg_m_127Inv);  
		VectorRegister LeftFracMax = VectorMultiplyAdd(SignedMaxMaxp5,KDopSIMD::V_127Inv,KDopSIMD::V_p5Neg_m_127Inv);  
		VectorRegister RightFracMin = VectorMultiplyAdd(SignedMinMinnp5,KDopSIMD::V_127InvNeg,KDopSIMD::V_p5Neg_m_127Inv);  
		VectorRegister RightFracMax = VectorMultiplyAdd(SignedMaxMinnp5,KDopSIMD::V_127InvNeg,KDopSIMD::V_p5Neg_m_127Inv);  

		VectorRegister LeftMin = VectorMultiplyAdd(LeftFracMin, VRange, VMeMin );
		VectorRegister LeftMax = VectorMultiplyAdd(LeftFracMax, VRangeNeg, VMeMax );
		VectorRegister RightMin = VectorMultiplyAdd(RightFracMin, VRange, VMeMin );
		VectorRegister RightMax = VectorMultiplyAdd(RightFracMax, VRangeNeg, VMeMax );

		// now we need to pack them
		VectorRegister LeftMaxToMin = VectorSwizzle(LeftMax,3,3,3,0); // 3 = junk;
		LeftMin = VectorSelect(KDopSIMD::VMinMergeOut,LeftMin,LeftMaxToMin); 
		LeftMax = VectorSwizzle(LeftMax,1,2,3,3); // 3 = junk;

		VectorRegister RightMaxToMin = VectorSwizzle(RightMax,3,3,3,0); // 3 = junk;
		RightMin = VectorSelect(KDopSIMD::VMinMergeOut,RightMin,RightMaxToMin); 
		RightMax = VectorSwizzle(RightMax,1,2,3,3); // 3 = junk;

		VectorStoreAligned(LeftMin,OutLeftMinPtr);
		VectorStoreAligned(LeftMax,OutLeftMinPtr + 4);
		VectorStoreAligned(RightMin,OutRightMinPtr);
		VectorStoreAligned(RightMax,OutRightMinPtr + 4);

		VectorResetFloatRegisters();
	}

	/**
	 * Given a parent BBox, computes the two child bboxes based on the encoded data I hold SIMD
	 * @param Me, bounding box of this node
	 * @param OutLeftChild, computed bounding box of the left child
	 * @param OutRightChild, computed bounding box of the right child
	 * @param HitCodes DWORD[2] Left,Right respective non-zero if there was a ray hit
	 * @param LCI std query parameters from the Check
	 * @return non-zero if the left child should be traversed first
	 */
	DWORD KDOP_INLINE DecompressAndLineCheck(const BoundType& Me,BoundType& OutLeftChild,BoundType& OutRightChild,DWORD* HitCodes, const FLineCollisionInfo& LCI) const
	{
#if TEST_COMPACT_KDOP_SLOW
		check((PTRINT(Min) & 3) == 0 || (PTRINT(Min) & 3) == 2); // we only support two possible alignments of the compressed input data
		check(Min + 3 == Max); // these two must be adjacent and in this order

		check(VectorIsAligned(Me.Min)); // must be aligned for SIMD
		check(Me.Min + 3 == Me.Max); // must be adjacent and in this order
		check(VectorIsAligned(OutLeftChild.Min)); // must be aligned for SIMD
		check(OutLeftChild.Min + 3 == OutLeftChild.Max); // must be adjacent and in this order
		check(VectorIsAligned(OutRightChild.Min)); // must be aligned for SIMD
		check(OutRightChild.Min + 3 == OutRightChild.Max); // must be adjacent and in this order
#endif
		// round down to the nearest DWORD. This looks trickier than it is because I wasn't sure ~3 would give the right mask for 64 bit
		// this really can't page fault. node 0 should be mod 4 == 0 and that is check'd elsewhere
		const BYTE* RESTRICT MinPtr = (const BYTE*)((PTRINT(Min)&3)^PTRINT(Min));  
		VectorRegister VMaskMod4EQ0 =  KDopSIMD::VAlignMasks[(PTRINT(Min) & 2) >> 1]; // if -1, then we were 4 byte aligned, else we are %4==2

		const FLOAT* RESTRICT MeMinPtr = Me.Min;
		FLOAT* RESTRICT OutLeftMinPtr = OutLeftChild.Min;
		FLOAT* RESTRICT OutRightMinPtr = OutRightChild.Min;

		// load up Me bbox here; it can be stuffed into unused execution slots in the critical path below
		VectorRegister VMeMin = VectorLoadAligned(MeMinPtr);
		VectorRegister TempMeMax = VectorLoadAligned(MeMinPtr + 4);
		VectorRegister TempMeMaxFromMin = VectorSwizzle(VMeMin,3,0,0,0); // 0 = junk
		VectorRegister VMeMax = VectorSelect(KDopSIMD::VMaxMergeMask,VectorSwizzle(TempMeMax,3,0,1,3),TempMeMaxFromMin); // 3 = junk

		// Extent and negative extent, also can fill execution slots
		VectorRegister VRange = VectorSubtract(VMeMax,VMeMin);
		VectorRegister VRangeNeg = VectorSubtract(VMeMin,VMeMax);

		VectorRegister VComp1 = VectorLoadByte4(MinPtr);
		VectorRegister VComp2 = VectorLoadByte4(MinPtr + 4);

		// mod 4 == 0 path
		VectorRegister VMinMod4EQ0 = VComp1;
		VectorRegister TempMaxFromMin = VectorSwizzle(VMinMod4EQ0,3,0,0,0); // 0 = junk
		VectorRegister VMaxMod4EQ0 = VectorSelect(KDopSIMD::VMaxMergeMask,VectorSwizzle(VComp2,3,0,1,3),TempMaxFromMin); // 3 = junk

		// mod 4 == 2 path
		// note that MinPtr has been aligned, above
		VectorRegister TempMin = VectorSwizzle(VComp1,2,3,0,0); // 0 = junk 
		VectorRegister VMaxMod4EQ2 = VComp2; 
		VectorRegister TempMinFromMax = VectorSwizzle(VMaxMod4EQ2,3,3,0,3); // 3 = junk
		VMaxMod4EQ2 = VectorSwizzle(VMaxMod4EQ2,1,2,3,0); // 0 = junk
		VectorRegister VMinMod4EQ2 = VectorSelect(KDopSIMD::VMinMergeMask,TempMin,TempMinFromMax); 

		VectorRegister VMin = VectorSelect(VMaskMod4EQ0,VMinMod4EQ0,VMinMod4EQ2);
		VectorRegister VMax = VectorSelect(VMaskMod4EQ0,VMaxMod4EQ0,VMaxMod4EQ2);

		VectorRegister SignedMin = VectorSubtract(VMin,KDopSIMD::V_127p5);
		VectorRegister SignedMax = VectorSubtract(VMax,KDopSIMD::V_127p5);

		VectorRegister SignedMinMaxp5 = VectorMax(SignedMin,KDopSIMD::V_p5);
		VectorRegister SignedMaxMaxp5 = VectorMax(SignedMax,KDopSIMD::V_p5);
		VectorRegister SignedMinMinnp5 = VectorMin(SignedMin,KDopSIMD::V_p5Neg);
		VectorRegister SignedMaxMinnp5 = VectorMin(SignedMax,KDopSIMD::V_p5Neg);

		VectorRegister LeftFracMin = VectorMultiplyAdd(SignedMinMaxp5,KDopSIMD::V_127Inv,KDopSIMD::V_p5Neg_m_127Inv);  
		VectorRegister LeftFracMax = VectorMultiplyAdd(SignedMaxMaxp5,KDopSIMD::V_127Inv,KDopSIMD::V_p5Neg_m_127Inv);  
		VectorRegister RightFracMin = VectorMultiplyAdd(SignedMinMinnp5,KDopSIMD::V_127InvNeg,KDopSIMD::V_p5Neg_m_127Inv);  
		VectorRegister RightFracMax = VectorMultiplyAdd(SignedMaxMinnp5,KDopSIMD::V_127InvNeg,KDopSIMD::V_p5Neg_m_127Inv);  

		VectorRegister LeftMin = VectorMultiplyAdd(LeftFracMin, VRange, VMeMin );
		VectorRegister LeftMax = VectorMultiplyAdd(LeftFracMax, VRangeNeg, VMeMax );
		VectorRegister RightMin = VectorMultiplyAdd(RightFracMin, VRange, VMeMin );
		VectorRegister RightMax = VectorMultiplyAdd(RightFracMax, VRangeNeg, VMeMax );

		// now we need to pack them
		VectorRegister LeftMaxToMin = VectorSwizzle(LeftMax,3,3,3,0); // 3 = junk;
		VectorRegister LeftMinPack = VectorSelect(KDopSIMD::VMinMergeOut,LeftMin,LeftMaxToMin); 
		VectorRegister LeftMaxPack = VectorSwizzle(LeftMax,1,2,3,3); // 3 = junk;

		VectorRegister RightMaxToMin = VectorSwizzle(RightMax,3,3,3,0); // 3 = junk;
		VectorRegister RightMinPack = VectorSelect(KDopSIMD::VMinMergeOut,RightMin,RightMaxToMin); 
		VectorRegister RightMaxPack = VectorSwizzle(RightMax,1,2,3,3); // 3 = junk;

		const FLOAT* RESTRICT LocalOneOverDirPtr = &LCI.LocalOneOverDir.X;
		const FLOAT* RESTRICT LocalStartPtr = &LCI.LocalStart.X;
		const FLOAT* RESTRICT LocalExtendPadPtr = &LCI.LocalExtentPad.X;
		DWORD* RESTRICT HitCodePtr = HitCodes;
		const FLOAT* RESTRICT ResultTimePtr = &((FCheckResult* RESTRICT )LCI.Result)->Time;

		VectorStoreAligned(LeftMinPack,OutLeftMinPtr);
		VectorStoreAligned(LeftMaxPack,OutLeftMinPtr + 4);
		VectorStoreAligned(RightMinPack,OutRightMinPtr);
		VectorStoreAligned(RightMaxPack,OutRightMinPtr + 4);

		VectorRegister LocalExtendPad = VectorLoadAligned(LocalExtendPadPtr);
		VectorRegister LocalOneOverDir = VectorLoadAligned(LocalOneOverDirPtr);
		VectorRegister LocalStart = VectorLoadAligned(LocalStartPtr);
		VectorRegister HitTime = VectorLoadFloat1(ResultTimePtr);
		HitTime = VectorMin(HitTime,VectorOne());  // we are only interested in the zero-one interval though hit time starts out at FLT_MAX

		LeftMin = VectorSubtract(LeftMin,LocalExtendPad);
		LeftMax = VectorAdd(LeftMax,LocalExtendPad);
		RightMin = VectorSubtract(RightMin,LocalExtendPad);
		RightMax = VectorAdd(RightMax,LocalExtendPad);

		VectorRegister LeftMinTime = VectorMultiply(VectorSubtract(LeftMin,LocalStart),LocalOneOverDir);
		VectorRegister LeftMaxTime = VectorMultiply(VectorSubtract(LeftMax,LocalStart),LocalOneOverDir);
		VectorRegister RightMinTime = VectorMultiply(VectorSubtract(RightMin,LocalStart),LocalOneOverDir);
		VectorRegister RightMaxTime = VectorMultiply(VectorSubtract(RightMax,LocalStart),LocalOneOverDir);

		VectorRegister LeftTimeMin = VectorMin(LeftMinTime,LeftMaxTime);
		VectorRegister LeftTimeMax = VectorMax(LeftMinTime,LeftMaxTime);
		VectorRegister RightTimeMin = VectorMin(RightMinTime,RightMaxTime);
		VectorRegister RightTimeMax = VectorMax(RightMinTime,RightMaxTime);

		VectorRegister LeftTimeMaxOfMins = VectorSelect(KDopSIMD::VReplace4thMask,LeftTimeMin,VectorZero());
		VectorRegister LeftTimeMinOfMaxs = VectorSelect(KDopSIMD::VReplace4thMask,LeftTimeMax,HitTime);
		VectorRegister RightTimeMaxOfMins = VectorSelect(KDopSIMD::VReplace4thMask,RightTimeMin,VectorZero());
		VectorRegister RightTimeMinOfMaxs = VectorSelect(KDopSIMD::VReplace4thMask,RightTimeMax,HitTime);

		VectorRegister LeftMinsS1 = VectorSwizzle(LeftTimeMaxOfMins,1,2,3,0); 
		VectorRegister LeftMinsS2 = VectorSwizzle(LeftTimeMaxOfMins,2,3,0,1); 
		VectorRegister LeftMinsS3 = VectorSwizzle(LeftTimeMaxOfMins,3,0,1,2); 
		VectorRegister LeftMaxsS1 = VectorSwizzle(LeftTimeMinOfMaxs,1,2,3,0); 
		VectorRegister LeftMaxsS2 = VectorSwizzle(LeftTimeMinOfMaxs,2,3,0,1); 
		VectorRegister LeftMaxsS3 = VectorSwizzle(LeftTimeMinOfMaxs,3,0,1,2); 
		VectorRegister RightMinsS1 = VectorSwizzle(RightTimeMaxOfMins,1,2,3,0); 
		VectorRegister RightMinsS2 = VectorSwizzle(RightTimeMaxOfMins,2,3,0,1); 
		VectorRegister RightMinsS3 = VectorSwizzle(RightTimeMaxOfMins,3,0,1,2); 
		VectorRegister RightMaxsS1 = VectorSwizzle(RightTimeMinOfMaxs,1,2,3,0); 
		VectorRegister RightMaxsS2 = VectorSwizzle(RightTimeMinOfMaxs,2,3,0,1); 
		VectorRegister RightMaxsS3 = VectorSwizzle(RightTimeMinOfMaxs,3,0,1,2); 

		VectorRegister LeftMinsS12 = VectorMax(LeftMinsS1,LeftMinsS2);
		VectorRegister LeftMaxsS12 = VectorMax(LeftMaxsS1,LeftMaxsS2);
		VectorRegister RightMinsS12 = VectorMax(RightMinsS1,RightMinsS2);
		VectorRegister RightMaxsS12 = VectorMax(RightMaxsS1,RightMaxsS2);


		LeftTimeMaxOfMins = VectorMax(LeftMinsS3,LeftTimeMaxOfMins);
		LeftTimeMaxOfMins = VectorMax(LeftMinsS12,LeftTimeMaxOfMins);
		LeftTimeMinOfMaxs = VectorMin(LeftMaxsS3,LeftTimeMinOfMaxs);
		LeftTimeMinOfMaxs = VectorMin(LeftMaxsS12,LeftTimeMinOfMaxs);
		RightTimeMaxOfMins = VectorMax(RightMinsS3,RightTimeMaxOfMins);
		RightTimeMaxOfMins = VectorMax(RightMinsS12,RightTimeMaxOfMins);
		RightTimeMinOfMaxs = VectorMin(RightMaxsS3,RightTimeMinOfMaxs);
		RightTimeMinOfMaxs = VectorMin(RightMaxsS12,RightTimeMinOfMaxs);

		HitCodePtr[0] = !VectorAnyGreaterThan(LeftTimeMaxOfMins,LeftTimeMinOfMaxs);
		HitCodePtr[1] = !VectorAnyGreaterThan(RightTimeMaxOfMins,RightTimeMinOfMaxs);
		DWORD LeftFirst = VectorAnyGreaterThan(RightTimeMaxOfMins,LeftTimeMaxOfMins);

		VectorResetFloatRegisters();
		return LeftFirst;
	}
#else
	/**
	 * Decompressed a component of an axis
	 * @param CompressedValue, compressed data for this axis and bound
	 * @param TargetBound, element of the parents bound; if computing a min, then this is the min of the parent bound on this axis
	 * @param OtherBound, element of the parents bound; if computing a min, then this is the max of the parent bound on this axis
	 * @param OutLeftChild, left childs corresponding bound for this axis; if computing a min, then this is the output min of the left child bound on this axis
	 * @param OutRightChild, right childs corresponding bound for this axis; if computing a min, then this is the output min of the left child bound on this axis
	 */
	static KDOP_INLINE void DecompressAxis(BYTE CompressedValue, FLOAT TargetBound, FLOAT OtherBound, FLOAT& OutLeftChild, FLOAT& OutRightChild)
	{
#if TEST_COMPACT_KDOP_SLOW
		check(CompressedValue > 0 && CompressedValue < 255);
#endif
		FLOAT Signed = (FLOAT(CompressedValue) - 127.5f);
		FLOAT LeftFrac = (floorf(::Max<FLOAT>(Signed,0.5f)) / 127.0f);
//		FLOAT LeftFrac = ((::Max<FLOAT>(Signed,0.5f)-0.5f) / 127.0f);
		OutLeftChild = TargetBound + LeftFrac * (OtherBound - TargetBound);
		FLOAT RightFrac = (floorf(::Max<FLOAT>(-Signed,0.5f)) / 127.0f);
//		FLOAT RightFrac = ((::Max<FLOAT>(-Signed,0.5f)-0.5f) / 127.0f);
		OutRightChild = TargetBound + RightFrac * (OtherBound - TargetBound);
#if TEST_COMPACT_KDOP_SLOW
		check(LeftFrac >= 0.0f && LeftFrac <= 1.0f);
		check(RightFrac >= 0.0f && RightFrac <= 1.0f);
#endif
	}
	/**
	 * Given a parent BBox, computes the two child bboxes based on the encoded data I hold
	 * @param Me, bounding box of this node
	 * @param OutLeftChild, computed bounding box of the left child
	 * @param OutRightChild, computed bounding box of the right child
	 */
	void Decompress(const BoundType& Me,BoundType& OutLeftChild,BoundType& OutRightChild) const
	{
#if TEST_COMPACT_KDOP_SLOW
		check(Me.IsValid());
#endif		
		for (INT PlaneIndex = 0; PlaneIndex < NUM_PLANES; PlaneIndex++)
		{
			DecompressAxis(Min[PlaneIndex], Me.Min[PlaneIndex], Me.Max[PlaneIndex], OutLeftChild.Min[PlaneIndex], OutRightChild.Min[PlaneIndex]);
			DecompressAxis(Max[PlaneIndex], Me.Max[PlaneIndex], Me.Min[PlaneIndex], OutLeftChild.Max[PlaneIndex], OutRightChild.Max[PlaneIndex]);
		}
#if TEST_COMPACT_KDOP_SLOW
		// postconditions
		check(OutLeftChild.IsValid());
		check(OutRightChild.IsValid());
		check(OutLeftChild.IsContainedIn(Me));
		check(OutRightChild.IsContainedIn(Me));
		// make sure the bounds did not get any smaller
		BoundType TestUnion(OutLeftChild);
		TestUnion.Union(OutRightChild);
		check(Me==TestUnion);
#endif
	}
	/**
	 * Given a parent BBox, computes the two child bboxes based on the encoded data I hold SIMD
	 * @param Me, bounding box of this node
	 * @param OutLeftChild, computed bounding box of the left child
	 * @param OutRightChild, computed bounding box of the right child
	 * @param HitCodes DWORD[2] Left,Right respective non-zero if there was a ray hit
	 * @param LCI std query parameters from the Check
	 * @return non-zero if the left child should be traversed first
	 */
	DWORD KDOP_INLINE DecompressAndLineCheck(const BoundType& Me,BoundType& OutLeftChild,BoundType& OutRightChild,DWORD* HitCodes, const FLineCollisionInfo& LCI) const
	{
		Decompress(Me,OutLeftChild,OutRightChild);
		FLOAT LeftTime = MAX_FLT;
		FLOAT RightTime = MAX_FLT;
		HitCodes[0] = 0;
		HitCodes[1] = 0;
		if (OutLeftChild.LineCheck(LCI,LeftTime))
		{
			HitCodes[0] = 1;
		}
		if (OutRightChild.LineCheck(LCI,LeftTime))
		{
			HitCodes[1] = 1;
		}
		return LeftTime < RightTime;
	}
#endif
	/**
	 * Given a parent BBox and two child BBoxes, encodes the data for this node
	 * @param Me, bounding box of this node
	 * @param LeftChild, bounding box of the left child
	 * @param RightChild, bounding box of the right child
	 */
	void Compress(const BoundType& Me,const BoundType& LeftChild,const BoundType& RightChild)
	{
#if TEST_COMPACT_KDOP
		// preconditions
		check(Me.IsValid());
		check(LeftChild.IsValid());
		check(RightChild.IsValid());
		check(LeftChild.IsContainedIn(Me));
		check(RightChild.IsContainedIn(Me));
#endif
		for (INT PlaneIndex = 0; PlaneIndex < NUM_PLANES; PlaneIndex++)
		{
			Min[PlaneIndex] = CompressAxis(Me.Min[PlaneIndex], Me.Max[PlaneIndex], LeftChild.Min[PlaneIndex], RightChild.Min[PlaneIndex]);
			Max[PlaneIndex] = CompressAxis(Me.Max[PlaneIndex], Me.Min[PlaneIndex], LeftChild.Max[PlaneIndex], RightChild.Max[PlaneIndex]);
		}
#if TEST_COMPACT_KDOP
		// postconditions
		FEnsurePadding<BoundType> TestLeftChild;
		FEnsurePadding<BoundType> TestRightChild;
		Decompress(Me,TestLeftChild,TestRightChild); // this performs several tests internally
		// make sure the bounds did not get any smaller
		check(LeftChild.IsContainedIn(TestLeftChild));
		check(RightChild.IsContainedIn(TestRightChild));
#endif		
	}

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,TkDOPCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& kDOP)
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

/**
 * A node in the kDOP tree. The node contains the kDOP volume that encompasses
 * it's children and/or triangles
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE> struct TkDOPNodeCompact
{
	/** Exposes data provider type to clients. */
	typedef COLL_DATA_PROVIDER									DataProviderType;

	/** This is the compact node type. */
	typedef TkDOPNodeCompact<DataProviderType,KDOP_IDX_TYPE>	NodeCompactType;

	/** This is the non-compact node type. */
	typedef TkDOPNode<DataProviderType,KDOP_IDX_TYPE>			NodeUncompressedType;

	/** This is the compact bound type. */
	typedef TkDOPCompact<DataProviderType,KDOP_IDX_TYPE>		kDOPCompactType;

	/** This is the original kDop node type. It is used after decompression and in preprocessing */
	typedef TkDOP<DataProviderType,KDOP_IDX_TYPE>				kDOPUncompressedType;

	// The bounding kDOP for this node
	kDOPCompactType BoundingVolume;

	struct FTraversalData
	{
		/** bound for this node */
		FEnsurePadding<kDOPUncompressedType>	Bound;
		/** index of this node */
		INT						NodeIndex;
		/** start of range of triangle included in this node and all of its children */
		INT						FirstTriangle;
		/** number of triangles in this node and all of its children */
		INT						NumTriangles;

		/** Constructor that sets up for the root of the tree
		* @param RootBound		The root bound for the tree
		* @param InNumTriangles		The total number of triangles in the tree
		*/
		FTraversalData(const kDOPUncompressedType& RootBound,INT InNumTriangles) :
				Bound(RootBound),
				NodeIndex(0),
				FirstTriangle(0),
				NumTriangles(InNumTriangles)
		{
		}
		/** NULL Constructor
		* since these are performance routines, we have a null constructor
		* this is always going to be followed by a call to BuildChildren
		* the bound will soon be filled directly by decompression
		*/
		KDOP_INLINE FTraversalData() :
				Bound(0) // means no construction
		{
		}

		/** use the node index to determine if this index is a leaf
		* all leaves are pointing to the pad node usually, but it doesn't matter, if it is off the array it must be a leaf
		* @param NumNodes number of nodes in the entire tree, includes a pad node
		* @return TRUE if we actually have children and something was done
		**/
		KDOP_INLINE UBOOL IsLeaf(INT NumNodes) const
		{
#if TEST_COMPACT_KDOP_SLOW
			check(NumTriangles > 0 && NodeIndex >= 0 && FirstTriangle >= 0);
#endif
			if (NodeIndex >= NumNodes - NUM_PAD_NODES)
			{
				return TRUE;
			}
#if TEST_COMPACT_KDOP_SLOW
			check(NumTriangles > 1);
#endif
			return FALSE;
		}
		/** compute the child node and triangle "pointers" based on the implicit nature of the tree
		* @param NumNodes number of nodes in the entire tree, includes a pad node
		* @param OutLeftChild Left child traversal struct to fill in
		* @param OutRightChild Right child traversal struct to fill in
		**/
		KDOP_INLINE void UpdateChildren(INT NumNodes, FTraversalData& OutLeftChild, FTraversalData& OutRightChild) const
		{
			OutRightChild.NumTriangles = NumTriangles / 2;
			OutLeftChild.NumTriangles = NumTriangles - OutRightChild.NumTriangles;
			OutRightChild.FirstTriangle = FirstTriangle + OutLeftChild.NumTriangles;
			OutLeftChild.FirstTriangle = FirstTriangle;
			OutLeftChild.NodeIndex = Min<INT>(NodeIndex * 2 + 1,NumNodes - 1);  // for a leaf, this isn't actually a legit node
			OutRightChild.NodeIndex = Min<INT>(OutLeftChild.NodeIndex + 1,NumNodes - 1); // for a leaf, this isn't actually a legit node
		}

		/** Generate children for recursive call
		* @param CompressedBound The compressed data for this node
		* @param OutLeftChild Left child traversal struct to fill in
		* @param OutRightChild Right child traversal struct to fill in
		* @return TRUE if we actually have children and something was done
		*/
		KDOP_INLINE UBOOL BuildChildren(const kDOPCompactType& CompressedBound, INT NumNodes, FTraversalData& OutLeftChild, FTraversalData& OutRightChild) const
		{
			if (IsLeaf(NumNodes))
			{
				return FALSE;
			}
			CompressedBound.Decompress(Bound, OutLeftChild.Bound, OutRightChild.Bound);
			UpdateChildren(NumNodes,OutLeftChild,OutRightChild);
			return TRUE;
		}
	};

	/**
	 * Inits the data to invalid values
	 */
	KDOP_INLINE TkDOPNodeCompact()
	{
		BoundingVolume.Init();
	}

	/**
	 * Swap two build triangles
	 *
	 * @param Index1 -- Index of one of the triangles to swap
	 * @param Index2 -- Index of another triangle to swap
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 */
	static KDOP_INLINE void Swap(INT Index1,INT Index2,
		TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles)
	{
#if TEST_COMPACT_KDOP
		check(Index1 >= 0 && Index1 < BuildTriangles.Num());
		check(Index2 >= 0 && Index2 < BuildTriangles.Num());
#endif
		if (Index1 != Index2)
		{
			appMemswap(&BuildTriangles(Index1),&BuildTriangles(Index2),sizeof(FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE>));
		}
	}

	/**
	 * classic partition algorithm customized for our needs
	 * On return elements < pivot have indices < the new pivot index, and
	 * elements > pivot have indices > the new pivot index
	 * (for the subset of the list we are operating on)
	 *
	 * @param StartIndex -- The triangle index to start processing with
	 * @param EndIndex -- The index of the last triangle to process
	 * @param PivotIndex -- Index to pivot on
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 * @param BestPlaneNormal -- normal of the split plane
	 * @return New index of pivot
	 */
	static INT Partition(INT StartIndex,INT EndIndex,INT PivotIndex,
		TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles,
		const FVector& BestPlaneNormal)
	{
#if TEST_COMPACT_KDOP
		// preconditions
		check(StartIndex <= EndIndex);
		check(StartIndex <= PivotIndex);
		check(PivotIndex <= EndIndex);
		check(EndIndex < BuildTriangles.Num());
#endif
		FLOAT PivotDot = BuildTriangles(PivotIndex).Centroid | BestPlaneNormal;

		Swap(PivotIndex,EndIndex,BuildTriangles);
		INT WriteIndex = StartIndex;

		for (INT Index = StartIndex; Index < EndIndex; Index++)
		{
			FLOAT IndexDot = BuildTriangles(Index).Centroid | BestPlaneNormal;
			if (IndexDot <= PivotDot)
			{
				Swap(Index,WriteIndex++,BuildTriangles);
			}
		}
		Swap(EndIndex,WriteIndex,BuildTriangles);
#if TEST_COMPACT_KDOP
		// postconditions
		for (INT Index = StartIndex; Index <= EndIndex; Index++)
		{
			FLOAT IndexDot = BuildTriangles(Index).Centroid | BestPlaneNormal;
			if (IndexDot < PivotDot)
			{
				check(Index < WriteIndex);
			}
			else if (IndexDot > PivotDot)
			{
				check(Index > WriteIndex);
			}
		}
		check(WriteIndex >= StartIndex && WriteIndex <= EndIndex);
#endif
		return WriteIndex;
	}

	/**
	 * classic partial sort algorithm customized for our needs
	 * On return triangles with indices < SplitIndex are <= triangles with indices >= split index
	 * (for the subset of the list we are operating on)
	 *
	 * @param StartIndex -- The triangle index to start processing with
	 * @param EndIndex -- The index of the last triangle to process
	 * @param SplitIndex -- Index to pivot on, as currently used this is EndIndex - (EndIndex - StartIndex)/2 but that isn't required
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 * @param BestPlaneNormal -- normal of the split plane
	 */
	static void PartialSort(INT StartIndex,INT EndIndex,INT SplitIndex,
		TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles,
		const FVector& BestPlaneNormal)
	{
#if TEST_COMPACT_KDOP
		// preconditions
		check(StartIndex <= EndIndex);
		check(StartIndex <= SplitIndex);
		check(SplitIndex <= EndIndex);
		check(EndIndex < BuildTriangles.Num());
#endif
		while (StartIndex < EndIndex)
		{
			INT PivotIndex = Partition(StartIndex,EndIndex,(StartIndex + EndIndex)/2,BuildTriangles,BestPlaneNormal);
			if (PivotIndex < SplitIndex)
			{
				EndIndex = PivotIndex - 1;
			}
			else
			{
				StartIndex = PivotIndex + 1;
			}
		}

#if TEST_COMPACT_KDOP
		// postconditions
		FLOAT SplitDot = BuildTriangles(SplitIndex).Centroid | BestPlaneNormal;
		for (INT Index = StartIndex; Index <= EndIndex; Index++)
		{
			FLOAT IndexDot = BuildTriangles(Index).Centroid | BestPlaneNormal;
			if (IndexDot < SplitDot)
			{
				check(Index < SplitIndex);
			}
			else if (IndexDot > SplitDot)
			{
				check(Index > SplitIndex);
			}
		}
#endif
	}

	/**
	 * Find the best split plane for a list of build triangles
	 * Uses the mean (splatter method). 
	 *
	 * @param Start -- The triangle index to start processing with
	 * @param NumTris -- The number of triangles to process
	 * @param PivotIndex -- The number of triangles to process
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 * @return Index of best plane
	 */
	static INT FindBestPlane(INT Start,INT NumTris,INT PivotIndex,
		TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles)
	{
		FLOAT BestMean = 0.0f;
		return NodeUncompressedType::FindBestPlane(Start,NumTris,BuildTriangles,BestMean);
	}


	/**
	 * Determines if the node is a leaf or not. If it is not a leaf, it subdivides
	 * the list of triangles again adding two child nodes and splitting then in half
	 *
	 * @param Start -- The triangle index to start processing with
	 * @param NumTris -- The number of triangles to process
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 * @param MyBound -- The bounding box for this node
	 * @param Nodes -- The list of compact nodes in this tree
	 */
	void SplitTriangleList(INT Start,INT NumTris,
		TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles,
		const kDOPUncompressedType& MyBound,
		TArray<NodeCompactType>& Nodes)
	{
#if TEST_COMPACT_KDOP
		{
			// Add all of the triangles to the bounding volume for some testing
			kDOPUncompressedType TestBound;
			TestBound.AddTriangles(Start,NumTris,BuildTriangles);
			check(TestBound.IsContainedIn(MyBound));
		}
#endif
		INT NumRight = NumTris / 2;
		INT NumLeft = NumTris - NumRight;
		INT FirstRight = Start + NumLeft;
		check(NumLeft + NumRight == NumTris);
		check(NumRight && NumLeft);

		INT BestPlane = FindBestPlane(Start,NumTris,FirstRight,BuildTriangles);

		PartialSort(Start,Start + NumTris - 1,FirstRight,BuildTriangles,FkDOPPlanes::PlaneNormals[BestPlane]);
		
		kDOPUncompressedType LeftBound;
		LeftBound.AddTriangles(Start,NumLeft,BuildTriangles);
		kDOPUncompressedType RightBound;
		RightBound.AddTriangles(FirstRight,NumRight,BuildTriangles);
		
#if TEST_COMPACT_KDOP
		{
			check(LeftBound.IsContainedIn(MyBound));
			check(RightBound.IsContainedIn(MyBound));
		}
#endif
		
		BoundingVolume.Compress(MyBound,LeftBound,RightBound);

		FEnsurePadding<kDOPUncompressedType> NewLeftBound;
		FEnsurePadding<kDOPUncompressedType> NewRightBound;

		// this step is very important
		// compression is lossy and decompression is recursive
		// so we need to be sure to propagate the same errors
		BoundingVolume.Decompress(MyBound,NewLeftBound,NewRightBound);
#if TEST_COMPACT_KDOP
		check(LeftBound.IsContainedIn(NewLeftBound));
		check(RightBound.IsContainedIn(NewRightBound));
#endif

		INT MyIndex = this - &Nodes(0);
		check(MyIndex >= 0 && MyIndex < Nodes.Num() - NUM_PAD_NODES);
		INT LeftIndex = 2 * MyIndex + 1;
		INT RightIndex = LeftIndex + 1;
		if (LeftIndex < Nodes.Num() - NUM_PAD_NODES)
		{
			check(RightIndex < Nodes.Num() - NUM_PAD_NODES); // otherwise only one legit child = wrong
			Nodes(LeftIndex).SplitTriangleList(Start,NumLeft,BuildTriangles,NewLeftBound,Nodes);
			Nodes(RightIndex).SplitTriangleList(FirstRight,NumRight,BuildTriangles,NewRightBound,Nodes);
		}
		// else this means we are off the end of the tree...i.e. this is a leaf		
	}

	/* 
	 * Determines the line in the FkDOPLineCollisionCheck intersects this node. It
	 * also will check the child nodes if it is not a leaf, otherwise it will check
	 * against the triangle data.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated line check data
	 * @return TRUE if there was a hit
	 */
	UBOOL LineCheck(const FTraversalData& Traversal,TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// !! caution, this pointer might point to the pad node which is not legit
		// it was done to avoid having to rewrite the code to hoise the computation one level up the tree
		UBOOL bHit = FALSE;
		// If this is a node, check the two child nodes and pick the closest one
		// to recursively check against and only check the second one if there is
		// not a hit or the hit returned is further out than the second node
		if (!Traversal.IsLeaf(Check.Nodes.Num()))
		{
		FTraversalData LeftTraversal,RightTraversal;
			DWORD HitLeftRight[2];
			DWORD LeftFirst = BoundingVolume.DecompressAndLineCheck(Traversal.Bound,LeftTraversal.Bound,RightTraversal.Bound,HitLeftRight,Check.LCI);			
#if TEST_COMPACT_KDOP_SLOW
			// this test is not definitive...the old code under-culled and there is roundoff error etc
			// it is good for debugging
			if (0)
			{
				FLOAT LeftHitTime = MAX_FLT;
				UBOOL bIssue = FALSE;
				if (LeftTraversal.Bound.LineCheck(Check.LCI,LeftHitTime))
				{
					if (!HitLeftRight[0])
					{
						bIssue = TRUE;
						debugf(TEXT("Old hit, I didn't Left"));
					}
				}
				else
		{
					if (HitLeftRight[0])
			{
						bIssue = TRUE;
						debugf(TEXT("Old didn't hit, I did Left"));
					}
			}
				FLOAT RightHitTime = MAX_FLT;
				if (RightTraversal.Bound.LineCheck(Check.LCI,RightHitTime))
			{
					if (!HitLeftRight[1])
				{
						bIssue = TRUE;
						debugf(TEXT("Old hit, I didn't Right"));
					}
				}
				else
				{
					if (HitLeftRight[1])
					{
						bIssue = TRUE;
						debugf(TEXT("Old didn't hit, I did Right"));
					}
				}
				if (!LeftFirst && LeftHitTime < RightHitTime - 0.1f)
				{
					bIssue = TRUE;
					debugf(TEXT("Wrong Order Should be leftfirst"));
				}
				if (LeftFirst && LeftHitTime > RightHitTime + 0.1f)
				{
					bIssue = TRUE;
					debugf(TEXT("Wrong Order Should not be leftfirst"));
			}
				if (bIssue)
			{
					BoundingVolume.DecompressAndLineCheck(Traversal.Bound,LeftTraversal.Bound,RightTraversal.Bound,HitLeftRight,Check.LCI);	
					check(0);
				}
			}
#endif
			// branchless fun
			DWORD RightFirst = (!HitLeftRight[0]) | ((!LeftFirst) & !!HitLeftRight[1]);
			FTraversalData* NearNode = IfPThenAElseB(RightFirst,IfPThenAElseB(HitLeftRight[1],&RightTraversal,(FTraversalData*)0),IfPThenAElseB(HitLeftRight[0],&LeftTraversal,(FTraversalData*)0)); 
			FTraversalData* FarNode = IfPThenAElseB(RightFirst,IfPThenAElseB(HitLeftRight[0],&LeftTraversal,(FTraversalData*)0),IfPThenAElseB(HitLeftRight[1],&RightTraversal,(FTraversalData*)0)); 
			Traversal.UpdateChildren(Check.Nodes.Num(),LeftTraversal,RightTraversal);
#if TEST_COMPACT_KDOP_SLOW
			check(NearNode || (!HitLeftRight[0] && !HitLeftRight[1])); // if we have only one, it better be the near node
			check(!(FarNode && !NearNode)); // if we have only one, it better be the near node
			check(FarNode != NearNode || (!FarNode && !NearNode));
			check(FarNode == 0 || FarNode == &LeftTraversal || FarNode == &RightTraversal);
			check(NearNode == 0 || NearNode == &LeftTraversal || NearNode == &RightTraversal);
#endif
			if (NearNode)
			{
				bHit = Check.Nodes(NearNode->NodeIndex).LineCheck(*NearNode,Check);
				// we don't check "far is beyond" here, the culler will get that on the next level
				// don't want multiple branches here, so use bitwise operations
				if ((!!FarNode) & ((!bHit) | !(Check.LCI.TraceFlags & TRACE_StopAtAnyHit)))
			{
				bHit |= Check.Nodes(FarNode->NodeIndex).LineCheck(*FarNode,Check);
			}
		}
		}
		else
		{
			// This is a leaf, check the triangles for a hit
			bHit = LineCheckTriangles(Traversal,Check);
		}
		return bHit;
	}
	/**
	 * Works through the list of triangles in this node checking each one for a
	 * collision.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated line check data
	 * @return TRUE if there was a hit
	 */
	KDOP_INLINE UBOOL LineCheckTriangles(const FTraversalData& Traversal,TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// Assume a miss
		UBOOL bHit = FALSE;
		// Check for an early out
		const UBOOL bStopAtAnyHit = Check.LCI.TraceFlags & TRACE_StopAtAnyHit;
		// Loop through all of our triangles. We need to check them all in case
		// there are two (or more) potential triangles that would collide and let
		// the code choose the closest
		for (INT CollTriIndex = Traversal.FirstTriangle;
			CollTriIndex < Traversal.FirstTriangle + Traversal.NumTriangles &&
			(bHit == FALSE || bStopAtAnyHit == FALSE);
			CollTriIndex++)
		{
			// Get the collision triangle that we are checking against
			const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri =	Check.CollisionTriangles(CollTriIndex);
			if(Check.CollDataProvider.ShouldCheckMaterial(CollTri.MaterialIndex))
			{
				// Now check for an intersection
				bHit |= NodeUncompressedType::LineCheckTriangle(Check,CollTri,CollTri.MaterialIndex);
			}
		}
		return bHit;
	}
#if PS3

	// @ todo, performance regression here on PS3. This code works around the problem pending a proper fix

	UBOOL BoxCheck(const FTraversalData& Traversal,TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// !! caution, this pointer might point to the pad node which is not legit
		// it was done to avoid having to rewrite the code to hoise the computation one level up the tree
		UBOOL bHit = FALSE;
		// If this is a node, check the two child nodes and pick the closest one
		// to recursively check against and only check the second one if there is
		// not a hit or the hit returned is further out than the second node
		FTraversalData LeftTraversal,RightTraversal;
		if (Traversal.BuildChildren(BoundingVolume,Check.Nodes.Num(),LeftTraversal,RightTraversal))
		{
			FTraversalData* NearNode = 0; 
			FTraversalData* FarNode = 0;
			// Holds the hit times for the child nodes
			FLOAT NodeHitTime = 0.f, NearTime = 0.f, FarTime = 0.f;
			// Update the kDOP with the extent and test against that
			kDOPUncompressedType kDOPNear(LeftTraversal.Bound,Check.LocalExtent);
			// Assume the left node is closer (it will be adjusted later)
			if (kDOPNear.LineCheck(Check.LCI,NodeHitTime))
			{
				NearNode = &LeftTraversal;
				NearTime = NodeHitTime;
			}
			// Update the kDOP with the extent and test against that
			kDOPUncompressedType kDOPFar(RightTraversal.Bound,Check.LocalExtent);
			// Find out if the second node is closer
			if (kDOPFar.LineCheck(Check.LCI,NodeHitTime))
			{
				// See if the left node was a miss and make the right the near node
				if (!NearNode)
				{
					NearNode = &RightTraversal;
					NearTime = NodeHitTime;
				}
				else
				{
					FarNode = &RightTraversal;
					FarTime = NodeHitTime;
				}
			}
			// Swap the Near/FarNodes if the right node is closer than the left
			if (NearNode && FarNode && FarTime < NearTime)
			{
				Exchange(NearNode,FarNode);
				Exchange(NearTime,FarTime);
			}
			// See if we need to search the near node or not
			if (NearNode && Check.LCI.Result->Time > NearTime)
			{
				bHit = Check.Nodes(NearNode->NodeIndex).BoxCheck(*NearNode,Check); 
			}
			// Check for an early out
			const UBOOL bStopAtAnyHit = Check.LCI.TraceFlags & TRACE_StopAtAnyHit;
			// Now do the same for the far node. This will only happen if a miss in
			// the near node or the nodes overlapped and this one is closer
			if (FarNode &&
				(Check.LCI.Result->Time > FarTime || bHit == FALSE) &&
				(bHit == FALSE || bStopAtAnyHit == FALSE))
			{
				bHit |= Check.Nodes(FarNode->NodeIndex).BoxCheck(*FarNode,Check);
			}
		}
		else
		{
			// This is a leaf, check the triangles for a hit
			bHit = BoxCheckTriangles(Traversal,Check);
		}
		return bHit;
	}
#else
	/**
	 * Determines the line + extent in the FkDOPBoxCollisionCheck intersects this
	 * node. It also will check the child nodes if it is not a leaf, otherwise it
	 * will check against the triangle data.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated box check data
	 * @return TRUE if there was a hit
	 */
	UBOOL BoxCheck(const FTraversalData& Traversal,TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// !! caution, this pointer might point to the pad node which is not legit
		// it was done to avoid having to rewrite the code to hoise the computation one level up the tree
		UBOOL bHit = FALSE;
		// If this is a node, check the two child nodes and pick the closest one
		// to recursively check against and only check the second one if there is
		// not a hit 
		if (!Traversal.IsLeaf(Check.Nodes.Num()))
		{
		FTraversalData LeftTraversal,RightTraversal;
			DWORD HitLeftRight[2];
			DWORD LeftFirst = BoundingVolume.DecompressAndLineCheck(Traversal.Bound,LeftTraversal.Bound,RightTraversal.Bound,HitLeftRight,Check.LCI);			
			// branchless fun
			DWORD RightFirst = (!HitLeftRight[0]) | ((!LeftFirst) & !!HitLeftRight[1]);
			FTraversalData* NearNode = IfPThenAElseB(RightFirst,IfPThenAElseB(HitLeftRight[1],&RightTraversal,(FTraversalData*)0),IfPThenAElseB(HitLeftRight[0],&LeftTraversal,(FTraversalData*)0)); 
			FTraversalData* FarNode = IfPThenAElseB(RightFirst,IfPThenAElseB(HitLeftRight[0],&LeftTraversal,(FTraversalData*)0),IfPThenAElseB(HitLeftRight[1],&RightTraversal,(FTraversalData*)0)); 
			Traversal.UpdateChildren(Check.Nodes.Num(),LeftTraversal,RightTraversal);
#if TEST_COMPACT_KDOP_SLOW
			check(NearNode || (!HitLeftRight[0] && !HitLeftRight[1])); // if we have only one, it better be the near node
			check(!(FarNode && !NearNode)); // if we have only one, it better be the near node
			check(FarNode != NearNode || (!FarNode && !NearNode));
			check(FarNode == 0 || FarNode == &LeftTraversal || FarNode == &RightTraversal);
			check(NearNode == 0 || NearNode == &LeftTraversal || NearNode == &RightTraversal);
#endif
			if (NearNode)
			{
				bHit = Check.Nodes(NearNode->NodeIndex).BoxCheck(*NearNode,Check); 
				// we don't check "far is beyond" here, the culler will get that on the next level
				// don't want multiple branches here, so use bitwise operations
				if ((!!FarNode) & ((!bHit) | !(Check.LCI.TraceFlags & TRACE_StopAtAnyHit)))
			{
				bHit |= Check.Nodes(FarNode->NodeIndex).BoxCheck(*FarNode,Check);
			}
		}
		}
		else
		{
			// This is a leaf, check the triangles for a hit
			bHit = BoxCheckTriangles(Traversal,Check);
		}
		return bHit;
	}
#endif
	/**
	 * Works through the list of triangles in this node checking each one for a
	 * collision.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated box check data
	 * @return TRUE if there was a hit
	 */
	KDOP_INLINE UBOOL BoxCheckTriangles(const FTraversalData& Traversal,TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// Assume a miss
		UBOOL bHit = 0;
		// Use an early out if possible
		const UBOOL bStopAtAnyHit = Check.LCI.TraceFlags & TRACE_StopAtAnyHit;
		// Loop through all of our triangles. We need to check them all in case
		// there are two (or more) potential triangles that would collide and let
		// the code choose the closest
		for (INT CollTriIndex = Traversal.FirstTriangle;
			CollTriIndex < Traversal.FirstTriangle + Traversal.NumTriangles &&
			(bHit == FALSE || bStopAtAnyHit == FALSE);
			CollTriIndex++)
		{
			// Get the collision triangle that we are checking against
			const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri = Check.CollisionTriangles(CollTriIndex);
			if(Check.CollDataProvider.ShouldCheckMaterial(CollTri.MaterialIndex))
			{
				// Now check for an intersection using the Separating Axis Theorem
				bHit |= NodeUncompressedType::BoxCheckTriangle(Check,CollTri,CollTri.MaterialIndex);
			}
		}
		return bHit;
	}


	/**
	 * Determines the point + extent in the FkDOPPointCollisionCheck intersects
	 * this node. It also will check the child nodes if it is not a leaf, otherwise
	 * it will check against the triangle data.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated point check data
	 * @return TRUE if there was a hit
	 */
	UBOOL PointCheck(const FTraversalData& Traversal,TkDOPPointCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// !! caution, this pointer might point to the pad node which is not legit
		// it was done to avoid having to rewrite the code to hoise the computation one level up the tree
		UBOOL bHit = FALSE;
		// If this is a node, check the two child nodes recursively
		FTraversalData LeftTraversal,RightTraversal;
		if (Traversal.BuildChildren(BoundingVolume,Check.Nodes.Num(),LeftTraversal,RightTraversal))
		{
			FTraversalData* NearNode = 0; 
			FTraversalData* FarNode = 0;
			// Update the kDOP with the extent and test against that
			kDOPUncompressedType kDOPNear(LeftTraversal.Bound,Check.LocalExtent);
			// Assume the left node is closer (it will be adjusted later)
			if (kDOPNear.PointCheck(Check.LCI))
			{
				NearNode = &LeftTraversal;
			}
			// Update the kDOP with the extent and test against that
			kDOPUncompressedType kDOPFar(RightTraversal.Bound,Check.LocalExtent);
			// Find out if the second node is closer
			if (kDOPFar.PointCheck(Check.LCI))
			{
				// See if the left node was a miss and make the right the near node
				if (!NearNode)
				{
					NearNode = &RightTraversal;
				}
				else
				{
					FarNode = &RightTraversal;
				}
			}
			// See if we need to search the near node or not
			if (NearNode)
			{
				bHit = Check.Nodes(NearNode->NodeIndex).PointCheck(*NearNode,Check);
			}
			// Now do the same for the far node
			if (FarNode)
			{
				bHit |= Check.Nodes(FarNode->NodeIndex).PointCheck(*FarNode,Check);
			}
		}
		else
		{
			// This is a leaf, check the triangles for a hit
			bHit = PointCheckTriangles(Traversal,Check);
		}
		return bHit;
	}

	/**
	 * Works through the list of triangles in this node checking each one for a
	 * collision.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated point check data
	 * @return TRUE if there was a hit
	 */
	KDOP_INLINE UBOOL PointCheckTriangles(const FTraversalData& Traversal,TkDOPPointCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE, TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// Assume a miss
		UBOOL bHit = FALSE;
		// Loop through all of our triangles. We need to check them all in case
		// there are two (or more) potential triangles that would collide and let
		// the code choose the closest
		for (KDOP_IDX_TYPE CollTriIndex = Traversal.FirstTriangle;
			CollTriIndex < Traversal.FirstTriangle + Traversal.NumTriangles;
			CollTriIndex++)
		{
			// Get the collision triangle that we are checking against
			const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri =	Check.CollisionTriangles(CollTriIndex);
			if(Check.CollDataProvider.ShouldCheckMaterial(CollTri.MaterialIndex))
			{
				// Now get refs to the 3 verts to check against
				const FVector& v1 = Check.CollDataProvider.GetVertex(CollTri.v1);
				const FVector& v2 = Check.CollDataProvider.GetVertex(CollTri.v2);
				const FVector& v3 = Check.CollDataProvider.GetVertex(CollTri.v3);
				// Now check for an intersection using the Separating Axis Theorem
				bHit |= NodeUncompressedType::PointCheckTriangle(Check,v1,v2,v3,CollTri.MaterialIndex);
			}
		}
		return bHit;
	}


	/**
	 * Find triangles that overlap the given sphere. We assume that the supplied box overlaps this node.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Query -- Query information
	 */
	KDOP_INLINE void SphereQuery(const FTraversalData& Traversal,TkDOPSphereQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE, TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Query) const
	{
		// !! caution, this pointer might point to the pad node which is not legit
		// it was done to avoid having to rewrite the code to hoise the computation one level up the tree

		// If not leaf, check against each child.
		FTraversalData LeftTraversal,RightTraversal;
		if (Traversal.BuildChildren(BoundingVolume,Query.Nodes.Num(),LeftTraversal,RightTraversal))
		{
			if( LeftTraversal.Bound.AABBOverlapCheck(Query.LocalBox) )
			{
				Query.Nodes(LeftTraversal.NodeIndex).SphereQuery(LeftTraversal,Query);
			}

			if( RightTraversal.Bound.AABBOverlapCheck(Query.LocalBox) )
			{
				Query.Nodes(RightTraversal.NodeIndex).SphereQuery(RightTraversal,Query);
			}
		}
		else // Otherwise, add all the triangles in this node to the list.
		{
			for (INT TriIndex = Traversal.FirstTriangle;
				TriIndex < Traversal.FirstTriangle + Traversal.NumTriangles; TriIndex++)
			{
				Query.ReturnTriangles.AddItem(TriIndex);
			}
		}

	}

	/**
	 * Find triangles that overlap the given AABB. We assume that the supplied box overlaps this node.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Query -- Query information
	 */
	void AABBQuery(const FTraversalData& Traversal,TkDOPAABBQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Query) const
	{
		// !! caution, this pointer might point to the pad node which is not legit
		// it was done to avoid having to rewrite the code to hoise the computation one level up the tree

		// If not leaf, check against each child.
		FTraversalData LeftTraversal,RightTraversal;
		if (Traversal.BuildChildren(BoundingVolume,Query.Nodes.Num(),LeftTraversal,RightTraversal))
		{
			if( LeftTraversal.Bound.AABBOverlapCheck(Query.LocalBox) )
			{
				Query.Nodes(LeftTraversal.NodeIndex).AABBQuery(LeftTraversal,Query);
			}

			if( RightTraversal.Bound.AABBOverlapCheck(Query.LocalBox) )
			{
				Query.Nodes(RightTraversal.NodeIndex).AABBQuery(RightTraversal,Query);
			}
		}
		else // Otherwise, add all the triangles in this node to the list.
		{
			for (INT TriIndex = Traversal.FirstTriangle;
				TriIndex < Traversal.FirstTriangle + Traversal.NumTriangles; TriIndex++)
			{
				const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri =	Query.CollisionTriangles(TriIndex);
				if(Query.CollDataProvider.ShouldCheckMaterial(CollTri.MaterialIndex))
				{
					Query.ReturnTriangles.AddItem(TriIndex);
				}
			}
		}
	}

	/**
	 * Find nodes that overlap the given frustum. We assume that the supplied
	 * frustum overlaps this node.
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Query -- Query information
	 * @return FALSE if the root node was culled; seems like a uninteresting choice
	 */
	UBOOL FrustumQuery(const FTraversalData& Traversal, TkDOPFrustumQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE, TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Query ) const
	{
		// !! caution, this pointer might point to the pad node which is not legit
		// it was done to avoid having to rewrite the code to hoise the computation one level up the tree

		if( !Traversal.Bound.FrustumCheck(Query.LocalFrustumPlanes) )
			return FALSE;

		FTraversalData LeftTraversal,RightTraversal;
		if (Traversal.BuildChildren(BoundingVolume,Query.Nodes.Num(),LeftTraversal,RightTraversal))
		{
			Query.Nodes(LeftTraversal.NodeIndex).FrustumQuery(LeftTraversal, Query);
			Query.Nodes(RightTraversal.NodeIndex).FrustumQuery(RightTraversal, Query);
		}
		else
		{
			Query.AddTriangleRun( Traversal.FirstTriangle, Traversal.NumTriangles );
		}
		return TRUE;
	}

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,NodeCompactType& Node)
	{
		// @warning BulkSerialize: FkDOPNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << Node.BoundingVolume;
		return Ar;
	}
#if TEST_COMPACT_KDOP_SLOW
	/**
	 * Traverses the tree, checking the fundamental properties
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated point check data
	 */
	void VerifyTree(const FTraversalData& Traversal,TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		extern TSet<INT> GVerifyTriangleArray;
		VerifyTreeInner(Traversal,Check);
		check(GVerifyTriangleArray.Num() == Traversal.NumTriangles);
		check(GVerifyTriangleArray.Num() == Check.CollisionTriangles.Num());
		for( INT Index = 0; Index < Check.CollisionTriangles.Num(); Index++)
		{
			check(GVerifyTriangleArray.Contains(Index));
		}
		GVerifyTriangleArray.Empty();
	}
	/**
	 * Traverses the tree, checking the fundamental properties
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated point check data
	 */
	void VerifyTreeInner(const FTraversalData& Traversal,TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		// !! caution, this pointer might point to the pad node which is not legit
		// it was done to avoid having to rewrite the code to hoise the computation one level up the tree
		VerifyTriangles(Traversal,Check);
		// If this is a node, check the two child nodes recursively
		FTraversalData LeftTraversal,RightTraversal;
		if (Traversal.BuildChildren(BoundingVolume,Check.Nodes.Num(),LeftTraversal,RightTraversal))
		{
			check(Traversal.NodeIndex < Check.Nodes.Num() - NUM_PAD_NODES );
			check(LeftTraversal.NodeIndex < Check.Nodes.Num());
			check(RightTraversal.NodeIndex < Check.Nodes.Num());
			Check.Nodes(LeftTraversal.NodeIndex).VerifyTreeInner(LeftTraversal,Check);
			Check.Nodes(RightTraversal.NodeIndex).VerifyTreeInner(RightTraversal,Check);
		}
		else
		{
			CheckCoverage(Traversal);
		}
	}

	/**
	 * Adds a node of triangles to the check list and verifies no duplicates
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 */
	void CheckCoverage(const FTraversalData& Traversal) const
	{
		extern TSet<INT> GVerifyTriangleArray;
		for (KDOP_IDX_TYPE CollTriIndex = Traversal.FirstTriangle;
			CollTriIndex < Traversal.FirstTriangle + Traversal.NumTriangles;
			CollTriIndex++)
		{
			check(!GVerifyTriangleArray.Contains(INT(CollTriIndex)));
			GVerifyTriangleArray.Add(INT(CollTriIndex));
		}
	}

	/**
	 * Check a node for validity
	 *
	 * @param Traversal -- Standard recursive data for recursive decompression
	 * @param Check -- The aggregated point check data
	 */
	KDOP_INLINE void VerifyTriangles(const FTraversalData& Traversal,TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE, TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		for (KDOP_IDX_TYPE CollTriIndex = Traversal.FirstTriangle;
			CollTriIndex < Traversal.FirstTriangle + Traversal.NumTriangles;
			CollTriIndex++)
		{
			// Get the collision triangle that we are checking against
			const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri =	Check.CollisionTriangles(CollTriIndex);
			// Now get refs to the 3 verts to check against
			const FVector& v1 = Check.CollDataProvider.GetVertex(CollTri.v1);
			const FVector& v2 = Check.CollDataProvider.GetVertex(CollTri.v2);
			const FVector& v3 = Check.CollDataProvider.GetVertex(CollTri.v3);
			Traversal.Bound.CheckPointCheck(v1);
			Traversal.Bound.CheckPointCheck(v2);
			Traversal.Bound.CheckPointCheck(v3);
		}
	}
#endif
};

/**
 * This is the tree of kDOPs that spatially divides the static mesh. It is
 * a binary tree of kDOP nodes.
 */
template<typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE> struct TkDOPTreeCompact
{
	/** Exposes data provider type to clients. */
	typedef COLL_DATA_PROVIDER									DataProviderType;

	/** Exposes node type to clients. */
	typedef TkDOPNodeCompact<DataProviderType,KDOP_IDX_TYPE>	NodeType;

	/** Exposes compact kDOP type to clients. */
	typedef typename NodeType::kDOPCompactType					kDOPCompactType;

	/** Exposes uncompressed kDOP type to clients. */
	typedef typename NodeType::kDOPUncompressedType				kDOPUncompressedType;

	/** Exposes traversal data to clients */
	typedef typename NodeType::FTraversalData					FTraversalData;

	/** The list of nodes contained within this tree. Node 0 is always the root node. */
	TArray<NodeType>											Nodes;

	/** The list of collision triangles in this tree. */
	TArray<FkDOPCollisionTriangle<KDOP_IDX_TYPE> >				Triangles;

	/** Bounding box of the entire tree, critical to starting the recursion */
	FEnsurePadding<kDOPUncompressedType>						RootBound;


	 FORCEINLINE void Prefetch() const
	 {
		 const BYTE* RESTRICT Start = (const BYTE * RESTRICT)&Nodes(0);
		 CONSOLE_PREFETCH(Start);
	 }

	/**
	 * Creates the root node and recursively splits the triangles into smaller
	 * volumes
	 *
	 * @param BuildTriangles -- The list of triangles to use for the build process
	 */
	void Build(TArray<FkDOPBuildCollisionTriangle<KDOP_IDX_TYPE> >& BuildTriangles)
	{
		INT NumTriangles = BuildTriangles.Num();
		RootBound.AddTriangles(0,NumTriangles,BuildTriangles);
		if (NumTriangles == 0)
		{
			check(!RootBound.IsValid());
			Nodes.Empty();
			Triangles.Empty();
		}
		else
		{
			check(RootBound.IsValid());
			INT NumLeaves = 0;
			INT NumNodes = 0;
			if (NumTriangles > MAX_TRIS_PER_LEAF_COMPACT)
			{
				NumLeaves = 1;
				while ((NumTriangles + NumLeaves - 1) / NumLeaves > MAX_TRIS_PER_LEAF_COMPACT * 2)  // *2 because each node actually encodes the _child_ bboxes
				{
					NumLeaves *= 2;
				}
				NumNodes = 2 * NumLeaves - 1; // this will be a complete, balanced binary tree
			}

			check(!NumLeaves || NumTriangles / NumLeaves > 0); // every leaf should have at least one triangle
			check(NumTriangles > MAX_TRIS_PER_LEAF_COMPACT || NumNodes == 0); // small set edge case checking

			Nodes.Empty(NumNodes + NUM_PAD_NODES);
			Nodes.AddZeroed(NumNodes + NUM_PAD_NODES);
			if (NumNodes && NumTriangles > 1)
			{
				check((PTRINT(&Nodes(0)) & 3) == 0); // SIMD will need this avoid the potential for page faults
				// Now tell that node to recursively subdivide the entire set of triangles
				Nodes(0).SplitTriangleList(0,NumTriangles,BuildTriangles,RootBound,Nodes);
			}
			// Copy over the triangle information afterward, since they will have
			// been sorted into their bounding volumes at this point
			Triangles.Empty(NumTriangles);
			Triangles.Add(NumTriangles);
			// Copy the triangles from the build list into the full list
			for (INT nIndex = 0; nIndex < BuildTriangles.Num(); nIndex++)
			{
				Triangles(nIndex) = BuildTriangles(nIndex);
			}
		}
		check(!Nodes.Num() || (PTRINT(&Nodes(0)) & 3) == 0); // SIMD will need this avoid the potential for page faults
	}
	/**
	 * Returns the root bound of the tree
	 *
	 * @param OutBox if the tree is valid and has nodes, this gives the root bounding box
	 * @return TRUE if OutBox was filled in, FALSE if the try is empty or otheriwse unusable
	 */
	UBOOL GetRootBound(FBox &OutBox)
	{
		if (Triangles.Num() > 0)
		{
			check(RootBound.IsValid());
			OutBox = RootBound.ToFBox();
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Remaps the indices of the tree and its nodes to match the provided mapping table,
	 * in case the underlying mesh's vertices get reordered.
	 * 
	 * e.g. NewIndex[i] = IndexRemapping[OldIndex[i]]
	 */
	void RemapIndices(const TArray<INT>& IndexRemapping)
	{
		for(INT TriangleIndex = 0; TriangleIndex < Triangles.Num(); ++TriangleIndex)
		{
			KDOP_IDX_TYPE index1 = Triangles(TriangleIndex).v1;
			KDOP_IDX_TYPE index2 = Triangles(TriangleIndex).v2;
			KDOP_IDX_TYPE index3 = Triangles(TriangleIndex).v3;

			Triangles(TriangleIndex).v1 = IndexRemapping(index1);
			Triangles(TriangleIndex).v2 = IndexRemapping(index2);
			Triangles(TriangleIndex).v3 = IndexRemapping(index3);
		}
	}

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated line check data
	 */
	UBOOL LineCheck(TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		UBOOL bHit = FALSE;
		FLOAT HitTime;
		Prefetch();
		// Check against the first bounding volume and decide whether to go further
		if (RootBound.LineCheck(Check.LCI,HitTime))
		{
			// Recursively check for a hit
			FTraversalData Traversal(RootBound,Triangles.Num());
#if TEST_COMPACT_KDOP_SLOW
			check(Nodes.Num());  // must have a pad node
			check(NUM_PAD_NODES > 0);
			if (Nodes(Nodes.Num()-1).BoundingVolume.Min[0] == 0) // we will only do this once and we will use a pad node to do store that state
			{
				Nodes(0).VerifyTree(Traversal,Check);
				*const_cast<BYTE *>(Nodes(Nodes.Num()-1).BoundingVolume.Min) = 255;
			}
#endif
			bHit = Nodes(0).LineCheck(Traversal,Check);
		}
		return bHit;
	}

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated box check data
	 */
	UBOOL BoxCheck(TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		UBOOL bHit = FALSE;
		FLOAT HitTime;
		Prefetch();
		// Check the root node's bounding volume expanded by the extent
		kDOPUncompressedType kDOP(RootBound,Check.LocalExtent);
		// Check against the first bounding volume and decide whether to go further
		if (kDOP.LineCheck(Check.LCI,HitTime))
		{
			// Recursively check for a hit
			FTraversalData Traversal(RootBound,Triangles.Num());
#if TEST_COMPACT_KDOP_SLOW
			check(Nodes.Num());  // must have a pad node
			check(NUM_PAD_NODES > 0);
			if (Nodes(Nodes.Num()-1).BoundingVolume.Min[0] == 0) // we will only do this once and we will use a pad node to do store that state
			{
				Nodes(0).VerifyTree(Traversal,Check);
				*const_cast<BYTE *>(Nodes(Nodes.Num()-1).BoundingVolume.Min) = 255;
			}
#endif
			bHit = Nodes(0).BoxCheck(Traversal,Check);
		}
		return bHit;
	}

	/**
	 * Figures out whether the check even hits the root node's bounding volume. If
	 * it does, it recursively searches for a triangle to hit.
	 *
	 * @param Check -- The aggregated point check data
	 */
	UBOOL PointCheck(TkDOPPointCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Check) const
	{
		UBOOL bHit = FALSE;
		// Check the root node's bounding volume expanded by the extent
		kDOPUncompressedType kDOP(RootBound,Check.LocalExtent);
		// Check against the first bounding volume and decide whether to go further
		if (kDOP.PointCheck(Check.LCI))
		{
			// Recursively check for a hit
			FTraversalData Traversal(RootBound,Triangles.Num());
#if TEST_COMPACT_KDOP_SLOW
			check(Nodes.Num());  // must have a pad node
			check(NUM_PAD_NODES > 0);
			if (Nodes(Nodes.Num()-1).BoundingVolume.Min[0] == 0) // we will only do this once and we will use a pad node to do store that state
			{
				Nodes(0).VerifyTree(Traversal,Check);
				*const_cast<BYTE *>(Nodes(Nodes.Num()-1).BoundingVolume.Min) = 255;
			}
#endif
			bHit = Nodes(0).PointCheck(Traversal,Check);
		}
		return bHit;
	}

	/**
	 * Find all triangles in static mesh that overlap a supplied bounding sphere.
	 *
	 * @param Query -- The aggregated sphere query data
	 */
	void SphereQuery(TkDOPSphereQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Query) const
	{
		// Check the query box overlaps the root node KDOP. If so, run query recursively.
		if( RootBound.AABBOverlapCheck( Query.LocalBox ) )
		{
			check(Nodes.Num() > NUM_PAD_NODES);
			FTraversalData Traversal(RootBound,Triangles.Num());
#if TEST_COMPACT_KDOP_SLOW
			check(Nodes.Num());  // must have a pad node
			check(NUM_PAD_NODES > 0);
			if (Nodes(Nodes.Num()-1).BoundingVolume.Min[0] == 0) // we will only do this once and we will use a pad node to do store that state
			{
				Nodes(0).VerifyTree(Traversal,Query);
				*const_cast<BYTE *>(Nodes(Nodes.Num()-1).BoundingVolume.Min) = 255;
			}
#endif
			Nodes(0).SphereQuery(Traversal, Query);
		}
	}

	/**
	 * Find all triangles in static mesh that overlap a supplied AABB.
	 *
	 * @param Query -- The aggregated AABB query data
	 */
	void AABBQuery(TkDOPAABBQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Query) const
	{
		// Check the query box overlaps the root node KDOP. If so, run query recursively.
		if( RootBound.AABBOverlapCheck( Query.LocalBox ) )
		{
			FTraversalData Traversal(RootBound,Triangles.Num());
#if TEST_COMPACT_KDOP_SLOW
			check(Nodes.Num());  // must have a pad node
			check(NUM_PAD_NODES > 0);
			if (Nodes(Nodes.Num()-1).BoundingVolume.Min[0] == 0) // we will only do this once and we will use a pad node to do store that state
			{
				Nodes(0).VerifyTree(Traversal,Query);
				*const_cast<BYTE *>(Nodes(Nodes.Num()-1).BoundingVolume.Min) = 255;
			}
#endif
			Nodes(0).AABBQuery(Traversal, Query);
		}
	}

	/**
	 * Find all kdop nodes in static mesh that overlap given frustum.
	 * This is just the entry point to the tree
	 *
	 * @param Query -- The aggregated frustum query data
	 */
	UBOOL FrustumQuery( TkDOPFrustumQuery<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE> >& Query ) const
	{
		if (Query.Nodes.Num())
		{
			// Begin at the root.
			FTraversalData Traversal(RootBound,Triangles.Num());
#if TEST_COMPACT_KDOP_SLOW
			check(Nodes.Num());  // must have a pad node
			check(NUM_PAD_NODES > 0);
			if (Nodes(Nodes.Num()-1).BoundingVolume.Min[0] == 0) // we will only do this once and we will use a pad node to do store that state
			{
				Nodes(0).VerifyTree(Traversal,Query);
				*const_cast<BYTE *>(Nodes(Nodes.Num()-1).BoundingVolume.Min) = 255;
			}
#endif
			Nodes(0).FrustumQuery(Traversal, Query);
		}
		// The query was successful only if there were intersecting nodes.
		return Query.ReturnRuns.Num() > 0;
	}

	// Serialization
	friend FArchive& operator<<(FArchive& Ar,TkDOPTreeCompact<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>& Tree)
	{
		Ar << Tree.RootBound;
		Tree.Nodes.BulkSerialize( Ar );
		Tree.Triangles.BulkSerialize( Ar );
		check(!Tree.Nodes.Num() || (PTRINT(&Tree.Nodes(0)) & 3) == 0); // SIMD will need this avoid the potential for page faults
		return Ar;
	}

};

/**
 * Base struct for all collision checks. Holds a reference to the collision
 * data provider, which is a struct that abstracts out the access to a
 * particular mesh/primitives data
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE, typename TREE_TYPE > struct TkDOPCollisionCheck
{
	/** Exposes data provider type to clients. */
	typedef COLL_DATA_PROVIDER DataProviderType;

	/** Exposes tree type to clients. */
	typedef TREE_TYPE TreeType;

	/** Exposes node type to clients. */
	typedef typename TreeType::NodeType NodeType;

	/**
	 * Used to get access to local->world, vertices, etc. without using virtuals
	 */
	const DataProviderType& CollDataProvider;
	/**
	 * The kDOP tree
	 */
	const TreeType& kDOPTree;
	/**
	 * The array of the nodes for the kDOP tree
	 */
	const TArray<NodeType>& Nodes;
	/**
	 * The collision triangle data for the kDOP tree
	 */
	const TArray<FkDOPCollisionTriangle<KDOP_IDX_TYPE> >& CollisionTriangles;

	/**
	 * Hide the default ctor
	 */
	TkDOPCollisionCheck(const COLL_DATA_PROVIDER& InCollDataProvider) :
		CollDataProvider(InCollDataProvider),
		kDOPTree(CollDataProvider.GetkDOPTree()),
		Nodes(kDOPTree.Nodes),
		CollisionTriangles(kDOPTree.Triangles)
	{
	}
};


/**
 * This struct holds the information used to do a line check against the kDOP
 * tree. The collision provider gives access to various matrices, vertex data
 * etc. without having to use virtual functions.
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE, typename TREE_TYPE> struct TkDOPLineCollisionCheck :
	public TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>
{
	FLineCollisionInfo LCI;

	/**
	 * Sets up the FkDOPLineCollisionCheck structure for performing line checks
	 * against a kDOPTree. Initializes all of the variables that are used
	 * throughout the line check.
	 *
	 * @param InStart -- The starting point of the trace
	 * @param InEnd -- The ending point of the trace
	 * @param InTraceFlags -- The trace flags that might modify searches
	 * @param InCollDataProvider -- The struct that provides access to mesh/primitive
	 *		specific data, such as L2W, W2L, Vertices, and so on
	 * @param InResult -- The out param for hit result information
	 */
	TkDOPLineCollisionCheck(const FVector& InStart,const FVector& InEnd,
		DWORD InTraceFlags,const COLL_DATA_PROVIDER& InCollDataProvider,
		FCheckResult* InResult) :
		TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>(InCollDataProvider),
			LCI(InStart,InEnd,InTraceFlags,InResult,InCollDataProvider.GetWorldToLocal())
	{
	}

	/**
	 * Transforms the local hit normal into a world space normal using the transpose
	 * adjoint and flips the normal if need be
	 */
	KDOP_INLINE FVector GetHitNormal(void)
	{
		// Transform the hit back into world space using the transpose adjoint
		FVector Normal = TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetLocalToWorldTransposeAdjoint().TransformNormal(LCI.LocalHitNormal).SafeNormal();
		// Flip the normal if the triangle is inverted
		if (TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetDeterminant() < 0.f)
		{
			Normal = -Normal;
		}
		return Normal;
	}
};

/**
 * This struct holds the information used to do a box and/or point check
 * against the kDOP tree. It is used purely to gather multiple function
 * parameters into a single structure for smaller stack overhead.
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE, typename TREE_TYPE > struct TkDOPBoxCollisionCheck :
	public TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>
{
	// Constant input vars
	const FVector& Extent;
	// Calculated vars
	FVector LocalExtent;
	FVector LocalBoxX;
	FVector LocalBoxY;
	FVector LocalBoxZ;

	/**
	 * Sets up the TkDOPBoxCollisionCheck structure for performing swept box checks
	 * against a kDOPTree. Initializes all of the variables that are used
	 * throughout the check.
	 *
	 * @param InStart -- The starting point of the trace
	 * @param InEnd -- The ending point of the trace
	 * @param InExtent -- The extent to check
	 * @param InTraceFlags -- The trace flags that might modify searches
	 * @param InCollDataProvider -- The struct that provides access to mesh/primitive
	 *		specific data, such as L2W, W2L, Vertices, and so on
	 * @param InResult -- The out param for hit result information
	 */
	TkDOPBoxCollisionCheck(const FVector& InStart,const FVector& InEnd,
		const FVector& InExtent,DWORD InTraceFlags,
		const COLL_DATA_PROVIDER& InCollDataProvider,FCheckResult* InResult) :
		TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>(InStart,InEnd,InTraceFlags,InCollDataProvider,InResult),
		Extent(InExtent)
	{
		// Move extent to local space
		LocalExtent = FBox(-Extent,Extent).TransformBy(TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetWorldToLocal()).GetExtent();
		TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::LCI.LocalExtentPad += LocalExtent;

		// Transform the PlaneNormals into local space.
		LocalBoxX = TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetWorldToLocal().TransformNormal(FkDOPPlanes::PlaneNormals[0]);
		LocalBoxY = TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetWorldToLocal().TransformNormal(FkDOPPlanes::PlaneNormals[1]);
		LocalBoxZ = TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetWorldToLocal().TransformNormal(FkDOPPlanes::PlaneNormals[2]);
	}
};

/**
 * This struct holds the information used to do a point check against the kDOP
 * tree. It is used purely to gather multiple function parameters into a
 * single structure for smaller stack overhead.
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE, typename TREE_TYPE> struct TkDOPPointCollisionCheck :
	public TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>
{
	// Holds the minimum pentration distance for push out calculations
	FLOAT BestDistance;
	FVector LocalHitLocation;

	/**
	 * Sets up the TkDOPPointCollisionCheck structure for performing point checks
	 * (point plus extent) against a kDOPTree. Initializes all of the variables
	 * that are used throughout the check.
	 *
	 * @param InLocation -- The point to check for intersection
	 * @param InExtent -- The extent to check
	 * @param InCollDataProvider -- The struct that provides access to mesh/primitive
	 *		specific data, such as L2W, W2L, Vertices, and so on
	 * @param InResult -- The out param for hit result information
	 */
	TkDOPPointCollisionCheck(const FVector& InLocation,const FVector& InExtent,
		const COLL_DATA_PROVIDER& InCollDataProvider,FCheckResult* InResult) :
		TkDOPBoxCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>(InLocation,InLocation,InExtent,0,InCollDataProvider,InResult),
		BestDistance(100000.f)
	{
	}

	/**
	 * Returns the transformed hit location
	 */
	KDOP_INLINE FVector GetHitLocation(void)
	{
		// Push out the hit location from the point along the hit normal and
		// convert into world units
		return TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetLocalToWorld().TransformFVector(
			TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::LCI.LocalStart + 
			TkDOPLineCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::LCI.LocalHitNormal * BestDistance);
	}
};

/**
 * Builds all of the data needed to find all triangles inside a sphere
 */
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE, typename TREE_TYPE > struct TkDOPSphereQuery :
	public TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>
{
	FBox LocalBox;
	// Index of FkDOPCollisionTriangle in KDOPTree
	TArray<INT>& ReturnTriangles;

	/**
	 * Sets up the FkDOPSphereQuery structure for finding the set of triangles
	 * in the static mesh that overlap the give sphere.
	 *
	 * @param InSphere -- Sphere to query against 
	 * @param OutTriangles -- Array of collision triangles that overlap sphere
	 * @param InCollDataProvider -- The struct that provides access to mesh/primitive
	 *		specific data, such as L2W, W2L, Vertices, and so on
	 */
	TkDOPSphereQuery(const FSphere& InSphere,TArray<INT>& OutTriangles,
		const COLL_DATA_PROVIDER& InCollDataProvider) :
		TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE>(InCollDataProvider),
		ReturnTriangles(OutTriangles)
	{
		// Find bounding box we are querying triangles against in local space.
		const FPlane RadiusVec(InSphere.W, InSphere.W, InSphere.W, 0.f);
		const FBox WorldBox(InSphere.Center - RadiusVec, InSphere.Center + RadiusVec);
		LocalBox = WorldBox.TransformBy(TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetWorldToLocal());
	}
};

	/**
 * Builds all of the data needed to find all triangles inside an AABB
 */
template <typename COLL_DATA_PROVIDER,typename KDOP_IDX_TYPE, typename TREE_TYPE> struct TkDOPAABBQuery :
	public TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>
{
	FBox LocalBox;
	// Index of FkDOPCollisionTriangle in KDOPTree
	TArray<INT>& ReturnTriangles;

	/**
	 * Sets up the TkDOPAABBQuery structure for finding the set of triangles
	 * in the static mesh that overlap the given AABB
	 *
	 * @param InBox -- FBox to query against 
	 * @param OutTriangles -- Array of collision triangles that overlap sphere
	 * @param InCollDataProvider -- The struct that provides access to mesh/primitive
	 * @param bNeedsTransform -- indicate whether we need to transform into local space when constructing our box (defaults to TRUE)
	 *		specific data, such as L2W, W2L, Vertices, and so on
	 */
	TkDOPAABBQuery(const FBox& InBox,TArray<INT>& OutTriangles,
		const COLL_DATA_PROVIDER& InCollDataProvider, UBOOL bNeedsTransform=TRUE) :
		TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>(InCollDataProvider),
		ReturnTriangles(OutTriangles)
	{
		if ( !bNeedsTransform )
		{
			LocalBox = InBox;

		}
		else
		{
			LocalBox = InBox.TransformBy(TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetWorldToLocal());
		}
	}
};

/**
 * Sets up all of the data needed to find all triangles in a given frustum
 */
template <typename COLL_DATA_PROVIDER, typename KDOP_IDX_TYPE, typename TREE_TYPE> struct TkDOPFrustumQuery :
	public TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>
{
	// frustum planes in local space
	TArray<FPlane> LocalFrustumPlanes;

	/*8 Defines a run of triangles as output from this query */
	struct FTriangleRun
	{
		/** First triangle in the run */
		KDOP_IDX_TYPE FirstTriangle;
		/** Number of triangles in the run */
		KDOP_IDX_TYPE NumTriangles;

		/* Constructor */
		FTriangleRun(KDOP_IDX_TYPE InFirstTriangle, KDOP_IDX_TYPE InNumTriangles) :
			FirstTriangle(InFirstTriangle),
			NumTriangles(InNumTriangles)
		{
		}
	};
	// indices for kdop node leaves that intersect the frustum
	TArray<FTriangleRun>& ReturnRuns;
	// number of triangles in the ReturnLeaves
	INT ReturnNumTris;

	/**
	 * Sets up the TkDOPFrustumQuery structure for finding the set of nodes
	 * in the static mesh that overlap the given frustum planes.
	 *
	 * @param InFrustumPlanes -- planes to check against
	 * @param InNumFrustumPlanes -- number of FPlanes in InFrustumPlanes
	 * @param OutLeaves -- Array of indices to kdop node leaves intersected
	 * @param InCollDataProvider -- The struct that provides access to mesh/primitive
	 *		specific data, such as L2W, W2L, Vertices, and so on
	 */
	TkDOPFrustumQuery(const FPlane* InFrustumPlanes,INT InNumFrustumPlanes,
		TArray<FTriangleRun>& OutRuns,const COLL_DATA_PROVIDER& InCollDataProvider) :
		TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>(InCollDataProvider),
		ReturnRuns( OutRuns ),
		ReturnNumTris(0)
	{
		// matrices needed to go from world to local space
		const FMatrix& ToLocalMat = TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetWorldToLocal();
		const FMatrix& ToLocalMatTA = ToLocalMat.TransposeAdjoint();
		const FLOAT Determinant = TkDOPCollisionCheck<COLL_DATA_PROVIDER,KDOP_IDX_TYPE,TREE_TYPE>::CollDataProvider.GetDeterminant();

		LocalFrustumPlanes.Add( InNumFrustumPlanes );
		for( INT PlaneIdx=0; PlaneIdx < InNumFrustumPlanes; PlaneIdx++ )
		{
			// the TransformByUsingAdjointT just transforms the plane normal (X,Y,Z) by using the
			// transpose adjoint of the w2l matrix (ToLocalMatTA).  Then it transforms a point on
			// the plane W*(X,Y,Z) by the w2l matrix (ToLocalMat). Then the new W (distance from 
			// plane to origin) can be derived.
			LocalFrustumPlanes(PlaneIdx) = InFrustumPlanes[PlaneIdx].TransformByUsingAdjointT( ToLocalMat, Determinant, ToLocalMatTA );        
		}
	}

	/** Adds a run of triangles to the list that will be returned to the caller
	 *
	 * @param FirstTriangle first triangle of run
	 * @param NumTriangles number of triangles in the run
	*/
	KDOP_INLINE void AddTriangleRun( KDOP_IDX_TYPE FirstTriangle, KDOP_IDX_TYPE NumTriangles )
	{
		if (NumTriangles) // this would only not be true for empty kdops 
		{
			ReturnRuns.AddItem(FTriangleRun(FirstTriangle,NumTriangles));
			ReturnNumTris += NumTriangles;
		}
	}
};
#endif
