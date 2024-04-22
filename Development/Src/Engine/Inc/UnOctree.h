/*=============================================================================
	UnOctree.h: Octree implementation header
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __OCTREEPRIVATE_H
#define __OCTREEPRIVATE_H

#include "UnCollision.h"

#define USE_SIMD_LINE_BOX (1)

#if USE_SIMD_LINE_BOX
#define USE_SIMD_ZELINE (1)
#else
#define USE_SIMD_ZELINE (0)  // must stay zero
#endif

enum 
{
	CHILDXMAX = 0x004,
	CHILDYMAX = 0x002,
	CHILDZMAX = 0x001
};

//
//	FOctreeNodeBounds - An octree node bounding cube.
//
#if USE_SIMD_LINE_BOX
MS_ALIGN(16)
#endif
struct FOctreeNodeBounds
{
	FVector	Center;
	FLOAT	Extent;

	FOctreeNodeBounds(const FVector& InCenter,FLOAT InExtent):
		Center(InCenter),
		Extent(InExtent)
	{}
#if USE_SIMD_LINE_BOX
	FOctreeNodeBounds()
	{
	}
#endif
	// Given a child index and the parent's bounding cube, construct the child's bounding cube.
	FOctreeNodeBounds(const FOctreeNodeBounds& InParentCube,INT InChildIndex);

	// GetBox - Returns a FBox representing the cube.
	FORCEINLINE FBox GetBox() const
	{
		return FBox(Center - FVector(Extent,Extent,Extent),Center + FVector(Extent,Extent,Extent));	}

	// IsInsideBox - Returns whether this cube is inside the given box.
	FORCEINLINE UBOOL IsInsideBox(const FBox& InBox) const
	{
		if(Center.X - Extent < InBox.Min.X || Center.X + Extent > InBox.Max.X)
			return 0;
		else if(Center.Y - Extent < InBox.Min.Y || Center.Y + Extent > InBox.Max.Y)
			return 0;
		else if(Center.Z - Extent < InBox.Min.Z || Center.Z + Extent > InBox.Max.Z )
			return 0;
		else
			return 1;
	}
}
#if USE_SIMD_LINE_BOX
GCC_ALIGN(16)
#endif
;

class FOctreeNodeBase
{
public:
	/** Create array of children node indices that this box overlaps. */
	inline INT FindChildren(const FOctreeNodeBounds& ParentBounds, const FBox& testBox, INT* childIXs)
	{
		INT childCount = 0;

		if(testBox.Max.X > ParentBounds.Center.X) // XMAX
		{ 
			if(testBox.Max.Y > ParentBounds.Center.Y) // YMAX
			{
				if(testBox.Max.Z > ParentBounds.Center.Z) // ZMAX
					childIXs[childCount++] = CHILDXMAX+CHILDYMAX+CHILDZMAX;
				if(testBox.Min.Z <= ParentBounds.Center.Z) // ZMIN
					childIXs[childCount++] = CHILDXMAX+CHILDYMAX          ;
			}

			if(testBox.Min.Y <= ParentBounds.Center.Y) // YMIN
			{
				if(testBox.Max.Z > ParentBounds.Center.Z) // ZMAX
					childIXs[childCount++] = CHILDXMAX+          CHILDZMAX;
				if(testBox.Min.Z <= ParentBounds.Center.Z) // ZMIN
					childIXs[childCount++] = CHILDXMAX                    ;
			}
		}

		if(testBox.Min.X <= ParentBounds.Center.X) // XMIN
		{ 
			if(testBox.Max.Y > ParentBounds.Center.Y) // YMAX
			{
				if(testBox.Max.Z > ParentBounds.Center.Z) // ZMAX
					childIXs[childCount++] =           CHILDYMAX+CHILDZMAX;
				if(testBox.Min.Z <= ParentBounds.Center.Z) // ZMIN
					childIXs[childCount++] =           CHILDYMAX          ;	
			}

			if(testBox.Min.Y <= ParentBounds.Center.Y) // YMIN
			{
				if(testBox.Max.Z > ParentBounds.Center.Z) // ZMAX
					childIXs[childCount++] =                     CHILDZMAX;
				if(testBox.Min.Z <= ParentBounds.Center.Z) // ZMIN
					childIXs[childCount++] = 0                            ;
			}
		}

		return childCount;
	}

	/** Returns which child node 'testBox' would fit into.
	 * Returns -1 if box overlaps any planes, and therefore wont fit into a child.
	 * Assumes testBox would fit into this (parent) node.
	 */
	inline INT FindChild(const FOctreeNodeBounds& ParentBounds, const FBox& testBox)
	{
		INT result = 0;

		if(testBox.Min.X > ParentBounds.Center.X)
			result |= CHILDXMAX;
		else if(testBox.Max.X > ParentBounds.Center.X)
			return -1;

		if(testBox.Min.Y > ParentBounds.Center.Y)
			result |= CHILDYMAX;
		else if(testBox.Max.Y > ParentBounds.Center.Y)
			return -1;

		if(testBox.Min.Z > ParentBounds.Center.Z)
			result |= CHILDZMAX;
		else if(testBox.Max.Z > ParentBounds.Center.Z)
			return -1;

		return result;

	}
};

#if XBOX
#include "UnOctreeXe.h"
#elif PS3 && 0 //@todo - joeg do a PS3 intrinsics version
#else
/**
 * Struct containing pre-calculated info for sphere/box tests
 */
struct FRadiusOverlapCheck
{
	/** Center of the sphere */
	const FVector SphereCenter;
	/** Size of the sphere pre-squared */
	const FLOAT RadiusSquared;

	/**
	 * Precalculates the information for faster compares
	 *
	 * @param Location the center of the sphere being tested
	 * @param Radius the size of the sphere being tested
	 */
	FRadiusOverlapCheck(const FVector& Sphere,FLOAT Radius) :
		SphereCenter(Sphere),
		RadiusSquared(Square(Radius))
	{
	}

	/**
	 * Tests the sphere against a FBoxSphereBounds
	 *
	 * @param Bounds the bounds to test against
	 */
	FORCEINLINE UBOOL SphereBoundsTest(const FBoxSphereBounds& Bounds)
	{
		const FBox AABB(Bounds.Origin - Bounds.BoxExtent,Bounds.Origin + Bounds.BoxExtent);
		return SphereAABBIntersectionTest(SphereCenter,RadiusSquared,AABB);
	}
};
#endif

/** Custom allocater, to reduce slack in Primitive arrays */
class FOctreePrimArrayAllocator
{
public:
	enum { NeedsElementType = FALSE };

	class ForAnyElementType : public FHeapAllocator::ForAnyElementType
	{
	public:
		INT CalculateSlack(INT NumElements,INT NumAllocatedElements,INT NumBytesPerElement) const;
	};

	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	};
};

/** Class for a single node in the Octree. */
class FOctreeNode : public FOctreeNodeBase
{
public:
	TArray<UPrimitiveComponent*,FOctreePrimArrayAllocator>	Primitives;	// Primitives held at this node.
	class FOctreeNode*				Children;	// Child nodes. If NULL, this is a leaf. Otherwise, always size 8.

	FOctreeNode();
	~FOctreeNode();

	void ActorNonZeroExtentLineCheck(class FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);
	void ActorZeroExtentLineCheck(class FPrimitiveOctree* octree, 
										   FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
										   FLOAT T1X, FLOAT T1Y, FLOAT T1Z, const FOctreeNodeBounds& Bounds);
	void ActorZeroExtentLineCheck(FPrimitiveOctree* o,struct FZETraversalNumbers &Traversal, const FOctreeNodeBounds& Bounds);
	void ActorEncroachmentCheck(FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);
	void ActorPointCheck(FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);
	void ActorRadiusCheck(FPrimitiveOctree* octree, const FOctreeNodeBounds& Bounds);

	/**
	 * Filters a sphere through the octree nodes finding actors that overlap with
	 * the sphere
	 *
	 * @param octree the octree being searched
	 * @param Bounds the bounds of the node
	 * @param Check the precalculated info for this check
	 */
	void ActorRadiusOverlapCheck(FPrimitiveOctree* octree,const FOctreeNodeBounds& Bounds,FRadiusOverlapCheck& Check);

	/**
	 * Filters a sphere through the octree nodes finding actors that overlap with
	 * the sphere. Optionally only affects physics objects.
	 *
	 * @param	Octree				The octree being searched.
	 * @param	Bounds				The bounds of the node.
	 * @param	Check				The precalculated info for this check.
	 * @param	bAllComponents		If FALSE, report only the first component per actor.
	 */
	void ActorOverlapCheck(FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds,FRadiusOverlapCheck& Check,UBOOL bAllComponents);

	/**
	 * Finds all pawns that are touched by a sphere (point + radius).
	 *
	 * @param	Octree				The octree being searched.
	 * @param	Bounds				The bounds of the node.
	 * @param	Check				The precalculated info for this check.
	 */
	void PawnOverlapCheck(FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds, FRadiusOverlapCheck& Check);

	/**
	 * Finds all actors that are touched by a sphere (point + radius) and pass chkActor->WantsOverlapCheckWith().
	 *
	 * @param	Octree				The octree being searched.
	 * @param	Bounds				The bounds of the node.
	 * @param	Check				The precalculated info for this check.
	 */
	void RestrictedOverlapCheck(FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds, FRadiusOverlapCheck& Check);

	void SingleNodeFilter(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds);
	UBOOL MultiNodeFilter(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds);
	void RemoveAllPrimitives(FPrimitiveOctree* o);

	void GetPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives) const;

	void GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& OutPrimitives,FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds);

	void Draw(FPrimitiveDrawInterface* PDI, FColor DrawColor, UBOOL bAndChildren, const FOctreeNodeBounds& Bounds);

	void FilterTest(const FBox& TestBox, UBOOL bMulti, TArray<FOctreeNode*> *Nodes, const FOctreeNodeBounds& Bounds);

	/** Iterate over this node and children, trying to collapse up the tree, up to the point that there are MAX_PRIMITIVES_PER_NODE in a node.*/
	void CollapseChildren();

private:
	void StoreActor(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds);
};

class FPrimitiveOctree : public FPrimitiveHashBase
{
public:
	// Root node - assumed to have size WORLD_MAX
	FOctreeNode*	RootNode;
	INT				OctreeTag;

	/// This is a bit nasty...
	// Temporary storage while recursing for line checks etc.
	FCheckResult*					ChkResult;
	FMemStack*						ChkMem;
	MS_ALIGN(16) FVector			ChkStart GCC_ALIGN(16); // aka Location
	DWORD							ChkTraceFlags;
	MS_ALIGN(16) FVector			ChkOneOverDir GCC_ALIGN(16);
	AActor*							ChkActor;
	MS_ALIGN(16) FVector			ChkExtent GCC_ALIGN(16);
	UPrimitiveComponent*			ChkPrimComp;
#if USE_SIMD_ZELINE
	MS_ALIGN(16) DWORD				ParallelMask[3] GCC_ALIGN(16);
#endif
	INT								ParallelAxis;
	FLOAT							ChkRadiusSqr;
	MS_ALIGN(16) FVector			RayOrigin GCC_ALIGN(16);

	FVector							ChkEnd;
	FVector							ChkDir;
	FRotator						ChkRotation;
	ULightComponent*				ChkLight;
	FBox							ChkBox;
	UBOOL							ChkBlockRigidBodyOnly;
	/** whether or not ChkExtent.IsZero() is TRUE */
	UBOOL							bChkExtentIsZero;

	TArray<UPrimitiveComponent*>	FailedPrims;
	/// 

	// Keeps track of shortest hit time so far.
	FCheckResult*	ChkFirstResult;

	INT				NodeTransform;

	UBOOL			bShowOctree;

	// FPrimitiveHashBase Interface
	FPrimitiveOctree();
	virtual ~FPrimitiveOctree();

	virtual void Tick();
	virtual void AddPrimitive(UPrimitiveComponent* Primitive);
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive);
	virtual FCheckResult* ActorLineCheck(FMemStack& Mem, 
		const FVector& End, 
		const FVector& Start, 
		const FVector& Extent, 
		DWORD TraceFlags, 
		AActor *SourceActor,
		class ULightComponent* SourceLight);
	virtual FCheckResult* ActorPointCheck(FMemStack& Mem, 
		const FVector& Location, 
		const FVector& Extent, 
		DWORD TraceFlags);

	/**
	 * Finds all actors that are touched by a sphere (point + radius). If
	 * bUseOverlap is false, only the centers of the bounding boxes are
	 * considered. If true, it does a full sphere/box check.
	 *
	 * @param Mem the mem stack to allocate results from
	 * @param Location the center of the sphere
	 * @param Radius the size of the sphere to check for overlaps with
	 * @param bUseOverlap whether to use the full box or just the center
	 */
	virtual FCheckResult* ActorRadiusCheck(FMemStack& Mem,const FVector& Location,FLOAT Radius,UBOOL bUseOverlap = FALSE);

	virtual FCheckResult* ActorEncroachmentCheck(FMemStack& Mem, 
		AActor* Actor, 
		FVector Location, 
		FRotator Rotation, 
		DWORD TraceFlags);

	/**
	 * Finds all actors that are touched by a sphere (point + radius).
	 *
	 * @param	Mem			The mem stack to allocate results from.
	 * @param	Actor		The actor to ignore overlaps with.
	 * @param	Location	The center of the sphere.
	 * @param	Radius		The size of the sphere to check for overlaps with.
	 */
	virtual FCheckResult* ActorOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius);

	/**
	 * Finds all actors that are touched by a sphere (point + radius).
	 *
	 * @param	Mem			The mem stack to allocate results from.
	 * @param	Actor		The actor to ignore overlaps with.
	 * @param	Location	The center of the sphere.
	 * @param	Radius		The size of the sphere to check for overlaps with.
	 * @param	TraceFlags	Options for the trace.
	 */
	virtual FCheckResult* ActorOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius, DWORD TraceFlags);

	/**
	 * Finds all pawns that are touched by a sphere (point + radius).
	 *
	 * @param	Mem			The mem stack to allocate results from.
	 * @param	Actor		The actor to ignore overlaps with.
	 * @param	Location	The center of the sphere.
	 * @param	Radius		The size of the sphere to check for overlaps with.
	 */
	virtual FCheckResult* PawnOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius);

	/**
	 * Finds all actors that are touched by a sphere (point + radius) and pass Actor->WantsOverlapCheckWith().
	 *
	 * @param	Mem			The mem stack to allocate results from.
	 * @param	Actor		The actor to ignore overlaps with.
	 * @param	Location	The center of the sphere.
	 * @param	Radius		The size of the sphere to check for overlaps with.
	 */
	virtual FCheckResult* RestrictedOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius);

	virtual void GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& Primitives);
	/**
	 * Retrieves all primitives in hash.
	 *
	 * @param	Primitives [out]	Array primitives are being added to
	 */
	virtual void GetPrimitives(TArray<UPrimitiveComponent*>& Primitives) const;

	/** Try and remove empty/nearly empty nodes.  */
	virtual void CollapseTreeChildren();

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);
};

FCheckResult* FindFirstResult(FCheckResult* Hits, DWORD TraceFlags);

#endif
