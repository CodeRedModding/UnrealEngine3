/*=============================================================================
	UnOctree.cpp: Octree implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnOctree.h"

#define USE_REDUCED_SLACK_ALLOCATOR	(1)

// urgh
#define MY_FLTMAX (3.402823466e+38F)
#define MY_INTMAX (2147483647)

//////////////////////////////////////////// UTIL //////////////////////////////////////////////////
#define MAX_PRIMITIVES_PER_NODE	(10)		// Max number of actors held in a node before adding children.
#define MIN_NODE_SIZE			(100)	// Mininmum size a node can ever be.
#define MAX_NODES_PER_PRIMITIVE	(1000)	// Maximum number of nodes a primitive can be in with multi-node insert before be just use single-node method.

#define CHECK_FALSE_NEG			(0)		// Check for hits with primitive, but not with bounding box.

#if USE_SIMD_LINE_BOX
#define TEST_SIMD_LINE_BOX (0)
#else
#define TEST_SIMD_LINE_BOX (0) // this must stay zero
#endif

#if USE_SIMD_ZELINE
#define TEST_SIMD_ZELINE (0)
#else
#define TEST_SIMD_ZELINE (0) // this must stay zero
#endif


static const FOctreeNodeBounds RootNodeBounds(FVector(0,0,0),HALF_WORLD_MAX);

/**
 * Enum values for octree stats
 */
enum EOctreeStats
{
	STAT_OverlapCheckTime = STAT_OctreeFirstStat,
	STAT_EncroachCheckCount,
	STAT_EncroachCheckTime,
	STAT_ZE_LineBox_Count,
	STAT_ZE_LineBox_Time,
	STAT_ZE_SNF_Count,
	STAT_ZE_SNF_Time,
	STAT_ZE_MNF_Count,
	STAT_ZE_MNF_Time,
	STAT_NZE_LineBox_Count,
	STAT_NZE_LineBox_Time,
	STAT_NZE_SNF_Count,
	STAT_NZE_SNF_Time,
	STAT_NZE_MNF_Count,
	STAT_NZE_MNF_Time,
	STAT_BoxBoxCount,
	STAT_BoxBoxTime,
	STAT_AddCount,
	STAT_AddTime,
	STAT_RemoveCount,
	STAT_RemoveTime,
	STAT_PointCheckCount,
	STAT_PointCheckTime,
	STAT_RadiusCheckCount,
	STAT_RadiusCheckTime,
	STAT_ZE_LineCheck_Count,
	STAT_ZE_LineCheck_Time,
	STAT_NZE_LineCheck_Count,
	STAT_NZE_LineCheck_Time,
	STAT_Octree_Memory
};

/**
 * Octree stats objects
 */
DECLARE_STATS_GROUP(TEXT("Octree"),STATGROUP_Octree);
DECLARE_DWORD_COUNTER_STAT(TEXT("Encroach Checks"),STAT_EncroachCheckCount,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("Encroach Check Time"),STAT_EncroachCheckTime,STATGROUP_Octree);

DECLARE_CYCLE_STAT(TEXT("Overlap Check Time"),STAT_OverlapCheckTime,STATGROUP_Octree);

DECLARE_DWORD_COUNTER_STAT_SLOW(TEXT("NZE LineBox Checks"),STAT_NZE_LineBox_Count,STATGROUP_Octree);
DECLARE_CYCLE_STAT_SLOW(TEXT("NZE LineBox Time"),STAT_NZE_LineBox_Time,STATGROUP_Octree);
DECLARE_DWORD_COUNTER_STAT_SLOW(TEXT("ZE LineBox Checks"),STAT_ZE_LineBox_Count,STATGROUP_Octree);
DECLARE_CYCLE_STAT_SLOW(TEXT("ZE LineBox Time"),STAT_ZE_LineBox_Time,STATGROUP_Octree);

DECLARE_DWORD_COUNTER_STAT(TEXT("NZE SNF Checks"),STAT_NZE_SNF_Count,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("NZE SNF Time"),STAT_NZE_SNF_Time,STATGROUP_Octree);
DECLARE_DWORD_COUNTER_STAT(TEXT("NZE MNF Checks"),STAT_NZE_MNF_Count,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("NZE MNF Time"),STAT_NZE_MNF_Time,STATGROUP_Octree);

DECLARE_DWORD_COUNTER_STAT(TEXT("ZE SNF Checks"),STAT_ZE_SNF_Count,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("ZE SNF Time"),STAT_ZE_SNF_Time,STATGROUP_Octree);
DECLARE_DWORD_COUNTER_STAT(TEXT("ZE MNF Checks"),STAT_ZE_MNF_Count,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("ZE MNF Time"),STAT_ZE_MNF_Time,STATGROUP_Octree);

DECLARE_DWORD_COUNTER_STAT_SLOW(TEXT("BoxBox Count"),STAT_BoxBoxCount,STATGROUP_Octree);
DECLARE_CYCLE_STAT_SLOW(TEXT("BoxBox Time"),STAT_BoxBoxTime,STATGROUP_Octree);

DECLARE_DWORD_COUNTER_STAT(TEXT("Add Count"),STAT_AddCount,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("Add Time"),STAT_AddTime,STATGROUP_Octree);
DECLARE_DWORD_COUNTER_STAT(TEXT("Remove Count"),STAT_RemoveCount,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("Remove Time"),STAT_RemoveTime,STATGROUP_Octree);

DECLARE_DWORD_COUNTER_STAT(TEXT("Point Check Count"),STAT_PointCheckCount,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("Point Check Time"),STAT_PointCheckTime,STATGROUP_Octree);

DECLARE_DWORD_COUNTER_STAT(TEXT("Radius Check Count"),STAT_RadiusCheckCount,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("Radius Check Time"),STAT_RadiusCheckTime,STATGROUP_Octree);

DECLARE_DWORD_COUNTER_STAT(TEXT("ZE Line Checks"),STAT_ZE_LineCheck_Count,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("ZE Line Time"),STAT_ZE_LineCheck_Time,STATGROUP_Octree);
DECLARE_DWORD_COUNTER_STAT(TEXT("NZE Line Checks"),STAT_NZE_LineCheck_Count,STATGROUP_Octree);
DECLARE_CYCLE_STAT(TEXT("NZE Line Time"),STAT_NZE_LineCheck_Time,STATGROUP_Octree);

DECLARE_MEMORY_STAT(TEXT("Octree Mem"),STAT_Octree_Memory,STATGROUP_Octree);

//
//	FOctreeNodeBounds::FOctreeNodeBounds - Given a child index and the parent's bounding cube, construct the child's bounding cube.
//

FOctreeNodeBounds::FOctreeNodeBounds(const FOctreeNodeBounds& InParentBounds,INT InChildIndex)
{
	Extent = InParentBounds.Extent * 0.5f;
	Center.X = InParentBounds.Center.X + (((InChildIndex & CHILDXMAX) >> 1) - 1) * Extent;
	Center.Y = InParentBounds.Center.Y + (((InChildIndex & CHILDYMAX)     ) - 1) * Extent;
	Center.Z = InParentBounds.Center.Z + (((InChildIndex & CHILDZMAX) << 1) - 1) * Extent;
}

#if TEST_SIMD_LINE_BOX || TEST_SIMD_ZELINE || !USE_SIMD_LINE_BOX

// New, hopefully faster, 'slabs' box test.
static UBOOL LineBoxIntersect
(
	const FVector&	BoxCenter,
	const FVector&  BoxRadii,
	const FVector&	Start,
	const FVector&	Direction,
	const FVector&	OneOverDirection
)
{
	//const FVector* boxPlanes = &Box.Min;
	
	FLOAT tf, tb;
	FLOAT tnear = 0.f;
	FLOAT tfar = 1.f;
	
	const FVector LocalStart = Start - BoxCenter;

	// X //
	// First - see if ray is parallel to slab.
	if(Direction.X != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.X * OneOverDirection.X) - BoxRadii.X * Abs(OneOverDirection.X);
		tb = - (LocalStart.X * OneOverDirection.X) + BoxRadii.X * Abs(OneOverDirection.X);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return FALSE;
	}
	else
	{
		// If it is parallel, early return if start is outiside slab.
		if(!(Abs(LocalStart.X) <= BoxRadii.X))
			return FALSE;
	}

	// Y //
	if(Direction.Y != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Y * OneOverDirection.Y) - BoxRadii.Y * Abs(OneOverDirection.Y);
		tb = - (LocalStart.Y * OneOverDirection.Y) + BoxRadii.Y * Abs(OneOverDirection.Y);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return FALSE;
	}
	else
	{
		if(!(Abs(LocalStart.Y) <= BoxRadii.Y))
			return FALSE;
	}

	// Z //
	if(Direction.Z != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Z * OneOverDirection.Z) - BoxRadii.Z * Abs(OneOverDirection.Z);
		tb = - (LocalStart.Z * OneOverDirection.Z) + BoxRadii.Z * Abs(OneOverDirection.Z);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return FALSE;
	}
	else
	{
		if(!(Abs(LocalStart.Z) <= BoxRadii.Z))
			return FALSE;
	}

	// we hit!
	return TRUE;
}
#define LINE_BOX LineBoxIntersect
#endif

#if USE_SIMD_LINE_BOX
static VectorRegister VReplace4thMask = MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)-1, (DWORD)0 );

/**
* determine if a "fat" line interesects a bounding box
* @param InBoxCenter center of the target bounding box
* @param InBoxRadii half extents of bounding box
* @param o octree the line parameters come from here, as is convention for this module
* @return TRUE if the line intersects the box
*/
static FORCEINLINE UBOOL LineBoxIntersectSIMD(const FVector& InBoxCenter, const FVector& InBoxRadii,const FPrimitiveOctree& o)
{

	const FLOAT* RESTRICT BoxCenterPtr = &InBoxCenter.X;
	const FLOAT* RESTRICT BoxRadiiPtr = &InBoxRadii.X;
	const FLOAT* RESTRICT LocalOneOverDirPtr = &o.ChkOneOverDir.X;
	const FLOAT* RESTRICT LocalStartPtr = &o.ChkStart.X;
	const FLOAT* RESTRICT LocalExtendPadPtr = &o.ChkExtent.X;
#if !IPHONE
	// disabling for now. Intermittent crashes with vector intrinsics turned on.
	checkSlow(
		VectorIsAligned(LocalOneOverDirPtr) &&
		VectorIsAligned(LocalStartPtr) &&
		VectorIsAligned(LocalExtendPadPtr)
		);
#endif

	VectorRegister BoxCenter = VectorLoadFloat3(BoxCenterPtr);
	VectorRegister BoxRadii = VectorLoadFloat3(BoxRadiiPtr);
	VectorRegister LocalExtendPad = VectorLoadAligned(LocalExtendPadPtr);
	VectorRegister LocalOneOverDir = VectorLoadAligned(LocalOneOverDirPtr);
	VectorRegister LocalStart = VectorLoadAligned(LocalStartPtr);

	VectorRegister LeftMin = VectorSubtract(BoxCenter,BoxRadii);
	VectorRegister LeftMax = VectorAdd(BoxCenter,BoxRadii);

	LeftMin = VectorSubtract(LeftMin,LocalExtendPad);
	LeftMax = VectorAdd(LeftMax,LocalExtendPad);

	VectorRegister LeftMinTime = VectorMultiply(VectorSubtract(LeftMin,LocalStart),LocalOneOverDir);  // can't be a MADD because we need to properly handle INF, which is an acceptable output here
	VectorRegister LeftMaxTime = VectorMultiply(VectorSubtract(LeftMax,LocalStart),LocalOneOverDir); // can't be a MADD because we need to properly handle INF, which is an acceptable output here

	VectorRegister LeftTimeMin = VectorMin(LeftMinTime,LeftMaxTime);
	VectorRegister LeftTimeMax = VectorMax(LeftMinTime,LeftMaxTime);

	VectorRegister LeftTimeMaxOfMins = VectorSelect(VReplace4thMask,LeftTimeMin,VectorZero());
	VectorRegister LeftTimeMinOfMaxs = VectorSelect(VReplace4thMask,LeftTimeMax,VectorOne());

	VectorRegister LeftMinsS1 = VectorSwizzle(LeftTimeMaxOfMins,1,2,3,0); 
	VectorRegister LeftMinsS2 = VectorSwizzle(LeftTimeMaxOfMins,2,3,0,1); 
	VectorRegister LeftMinsS3 = VectorSwizzle(LeftTimeMaxOfMins,3,0,1,2); 
	VectorRegister LeftMaxsS1 = VectorSwizzle(LeftTimeMinOfMaxs,1,2,3,0); 
	VectorRegister LeftMaxsS2 = VectorSwizzle(LeftTimeMinOfMaxs,2,3,0,1); 
	VectorRegister LeftMaxsS3 = VectorSwizzle(LeftTimeMinOfMaxs,3,0,1,2); 

	VectorRegister LeftMinsS12 = VectorMax(LeftMinsS1,LeftMinsS2);
	VectorRegister LeftMaxsS12 = VectorMin(LeftMaxsS1,LeftMaxsS2);


	LeftTimeMaxOfMins = VectorMax(LeftMinsS3,LeftTimeMaxOfMins);
	LeftTimeMaxOfMins = VectorMax(LeftMinsS12,LeftTimeMaxOfMins);
	LeftTimeMinOfMaxs = VectorMin(LeftMaxsS3,LeftTimeMinOfMaxs);
	LeftTimeMinOfMaxs = VectorMin(LeftMaxsS12,LeftTimeMinOfMaxs);

	return !VectorAnyGreaterThan(LeftTimeMaxOfMins,LeftTimeMinOfMaxs);

}

/**
* determine if a "fat" line interesects a bounding box, incuding a "time limit"
* @param InBoxCenter center of the target bounding box
* @param InBoxRadii half extents of bounding box
* @param o octree the line parameters come from here, as is convention for this module
* @param TimeLimit pointer to a float containing the max time encountered so far, so that we do not look for further intersections
* @return TRUE if the line intersects the box
*/
static FORCEINLINE UBOOL LineBoxIntersectSIMD(const FVector& InBoxCenter, const FVector& InBoxRadii,const FPrimitiveOctree& o,FLOAT *TimeLimit)
{

	const FLOAT* RESTRICT BoxCenterPtr = &InBoxCenter.X;
	const FLOAT* RESTRICT BoxRadiiPtr = &InBoxRadii.X;
	const FLOAT* RESTRICT LocalOneOverDirPtr = &o.ChkOneOverDir.X;
	const FLOAT* RESTRICT LocalStartPtr = &o.ChkStart.X;
	const FLOAT* RESTRICT LocalExtendPadPtr = &o.ChkExtent.X;
	const FLOAT* RESTRICT TimeLimitPtr = TimeLimit;
#if !IPHONE
	// disabling for now. Intermittent crashes with vector intrinsics turned on.
	checkSlow(
		VectorIsAligned(LocalOneOverDirPtr) &&
		VectorIsAligned(LocalStartPtr) &&
		VectorIsAligned(LocalExtendPadPtr) &&
		VectorIsAligned(TimeLimitPtr)
		);
#endif

	VectorRegister BoxCenter = VectorLoadFloat3(BoxCenterPtr);
	VectorRegister BoxRadii = VectorLoadFloat3(BoxRadiiPtr);
	VectorRegister LocalExtendPad = VectorLoadAligned(LocalExtendPadPtr);
	VectorRegister LocalOneOverDir = VectorLoadAligned(LocalOneOverDirPtr);
	VectorRegister LocalStart = VectorLoadAligned(LocalStartPtr);
	VectorRegister VTimeLimit = VectorLoadAligned(TimeLimitPtr);

	VectorRegister LeftMin = VectorSubtract(BoxCenter,BoxRadii);
	VectorRegister LeftMax = VectorAdd(BoxCenter,BoxRadii);

	LeftMin = VectorSubtract(LeftMin,LocalExtendPad);
	LeftMax = VectorAdd(LeftMax,LocalExtendPad);

	VectorRegister LeftMinTime = VectorMultiply(VectorSubtract(LeftMin,LocalStart),LocalOneOverDir);  // can't be a MADD because we need to properly handle INF, which is an acceptable output here
	VectorRegister LeftMaxTime = VectorMultiply(VectorSubtract(LeftMax,LocalStart),LocalOneOverDir); // can't be a MADD because we need to properly handle INF, which is an acceptable output here

	VectorRegister LeftTimeMin = VectorMin(LeftMinTime,LeftMaxTime);
	VectorRegister LeftTimeMax = VectorMax(LeftMinTime,LeftMaxTime);

	VectorRegister LeftTimeMaxOfMins = VectorSelect(VReplace4thMask,LeftTimeMin,VectorZero());
	VectorRegister LeftTimeMinOfMaxs = VectorSelect(VReplace4thMask,LeftTimeMax,VTimeLimit);

	VectorRegister LeftMinsS1 = VectorSwizzle(LeftTimeMaxOfMins,1,2,3,0); 
	VectorRegister LeftMinsS2 = VectorSwizzle(LeftTimeMaxOfMins,2,3,0,1); 
	VectorRegister LeftMinsS3 = VectorSwizzle(LeftTimeMaxOfMins,3,0,1,2); 
	VectorRegister LeftMaxsS1 = VectorSwizzle(LeftTimeMinOfMaxs,1,2,3,0); 
	VectorRegister LeftMaxsS2 = VectorSwizzle(LeftTimeMinOfMaxs,2,3,0,1); 
	VectorRegister LeftMaxsS3 = VectorSwizzle(LeftTimeMinOfMaxs,3,0,1,2); 

	VectorRegister LeftMinsS12 = VectorMax(LeftMinsS1,LeftMinsS2);
	VectorRegister LeftMaxsS12 = VectorMin(LeftMaxsS1,LeftMaxsS2);


	LeftTimeMaxOfMins = VectorMax(LeftMinsS3,LeftTimeMaxOfMins);
	LeftTimeMaxOfMins = VectorMax(LeftMinsS12,LeftTimeMaxOfMins);
	LeftTimeMinOfMaxs = VectorMin(LeftMaxsS3,LeftTimeMinOfMaxs);
	LeftTimeMinOfMaxs = VectorMin(LeftMaxsS12,LeftTimeMinOfMaxs);

	return !VectorAnyGreaterThan(LeftTimeMaxOfMins,LeftTimeMinOfMaxs);

}

/** These masks encode the conversion from octree child index to an actual bbox */
static VectorRegister VChildMaxMasks[8] = 
{
	MakeVectorRegister( (DWORD)0, (DWORD)0, (DWORD)0, (DWORD)0 ),
	MakeVectorRegister( (DWORD)0, (DWORD)0, (DWORD)-1, (DWORD)0 ),
	MakeVectorRegister( (DWORD)0, (DWORD)-1, (DWORD)0, (DWORD)0 ),
	MakeVectorRegister( (DWORD)0, (DWORD)-1, (DWORD)-1, (DWORD)0 ),
	MakeVectorRegister( (DWORD)-1, (DWORD)0, (DWORD)0, (DWORD)0 ),
	MakeVectorRegister( (DWORD)-1, (DWORD)0, (DWORD)-1, (DWORD)0 ),
	MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)0, (DWORD)0 ),
	MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)-1, (DWORD)0 ),
};

/** self evident constant 1/2 */
static VectorRegister V_Half = {0.5f,0.5f,0.5f,0.5f};

/**
* Determine if a "fat" line interesects an octree child and compute the child bbox in that case
* @param InParentBounds bounding box of the node under consideration
* @param LeftChildIndex Index of the child to test. "left" is of no significance; it is left in place in case we want to pipeline multiple children
* @param o octree the line parameters come from here, as is convention for this module
* @param OutBounds the bounds of the child given by LeftChildIndex
* @return TRUE if the line intersects the child box
*/
static FORCEINLINE_DEBUGGABLE UBOOL LineBoxIntersectSIMD(const FOctreeNodeBounds& InParentBounds,INT LeftChildIndex,const FPrimitiveOctree& o,FOctreeNodeBounds& OutBounds)
{
	const FLOAT* RESTRICT BoxCenterPtr = &InParentBounds.Center.X;
	const FLOAT* RESTRICT LocalOneOverDirPtr = &o.ChkOneOverDir.X;
	const FLOAT* RESTRICT LocalStartPtr = &o.ChkStart.X;
	const FLOAT* RESTRICT LocalExtendPadPtr = &o.ChkExtent.X;
	FLOAT* RESTRICT OutBoundsPtr = &OutBounds.Center.X;
#if !IPHONE
	// disabling for now. Intermittent crashes with vector intrinsics turned on.
	checkSlow(
		VectorIsAligned(BoxCenterPtr) &&
		VectorIsAligned(LocalOneOverDirPtr) &&
		VectorIsAligned(LocalStartPtr) &&
		VectorIsAligned(LocalExtendPadPtr) &&
		VectorIsAligned(OutBoundsPtr) &&
		((void *)(BoxCenterPtr + 3))==(void *)&InParentBounds.Extent
		);
#endif

	VectorRegister BoxCenter = VectorLoadAligned(BoxCenterPtr);
	VectorRegister BoxRadii = VectorSwizzle(BoxCenter,3,3,3,3); 
	VectorRegister HalfBoxRadii = VectorMultiply(BoxRadii,V_Half);
	BoxCenter = VectorSubtract(BoxCenter,HalfBoxRadii);
	VectorRegister LocalExtendPad = VectorLoadAligned(LocalExtendPadPtr);
	VectorRegister LocalOneOverDir = VectorLoadAligned(LocalOneOverDirPtr);
	VectorRegister LocalStart = VectorLoadAligned(LocalStartPtr);

	VectorRegister LeftCenterAdd = VectorSelect(VChildMaxMasks[LeftChildIndex],BoxRadii,VectorZero());
	VectorRegister LeftBoxCenter = VectorAdd(BoxCenter,LeftCenterAdd);
	VectorRegister LeftMin = VectorSubtract(LeftBoxCenter,HalfBoxRadii);
	VectorRegister LeftMax = VectorAdd(LeftBoxCenter,HalfBoxRadii);
	// works out without this LeftBoxCenter = VectorSelect(VReplace4thMask,LeftBoxCenter,HalfBoxRadii);

	LeftMin = VectorSubtract(LeftMin,LocalExtendPad);
	LeftMax = VectorAdd(LeftMax,LocalExtendPad);

	VectorRegister LeftMinTime = VectorMultiply(VectorSubtract(LeftMin,LocalStart),LocalOneOverDir);  // can't be a MADD because we need to properly handle INF, which is an acceptable output here
	VectorRegister LeftMaxTime = VectorMultiply(VectorSubtract(LeftMax,LocalStart),LocalOneOverDir); // can't be a MADD because we need to properly handle INF, which is an acceptable output here

	VectorRegister LeftTimeMin = VectorMin(LeftMinTime,LeftMaxTime);
	VectorRegister LeftTimeMax = VectorMax(LeftMinTime,LeftMaxTime);

	VectorRegister LeftTimeMaxOfMins = VectorSelect(VReplace4thMask,LeftTimeMin,VectorZero());
	VectorRegister LeftTimeMinOfMaxs = VectorSelect(VReplace4thMask,LeftTimeMax,VectorOne());

	VectorRegister LeftMinsS1 = VectorSwizzle(LeftTimeMaxOfMins,1,2,3,0); 
	VectorRegister LeftMinsS2 = VectorSwizzle(LeftTimeMaxOfMins,2,3,0,1); 
	VectorRegister LeftMinsS3 = VectorSwizzle(LeftTimeMaxOfMins,3,0,1,2); 
	VectorRegister LeftMaxsS1 = VectorSwizzle(LeftTimeMinOfMaxs,1,2,3,0); 
	VectorRegister LeftMaxsS2 = VectorSwizzle(LeftTimeMinOfMaxs,2,3,0,1); 
	VectorRegister LeftMaxsS3 = VectorSwizzle(LeftTimeMinOfMaxs,3,0,1,2); 

	VectorRegister LeftMinsS12 = VectorMax(LeftMinsS1,LeftMinsS2);
	VectorRegister LeftMaxsS12 = VectorMin(LeftMaxsS1,LeftMaxsS2);


	LeftTimeMaxOfMins = VectorMax(LeftMinsS3,LeftTimeMaxOfMins);
	LeftTimeMaxOfMins = VectorMax(LeftMinsS12,LeftTimeMaxOfMins);
	LeftTimeMinOfMaxs = VectorMin(LeftMaxsS3,LeftTimeMinOfMaxs);
	LeftTimeMinOfMaxs = VectorMin(LeftMaxsS12,LeftTimeMinOfMaxs);

	VectorStoreAligned(LeftBoxCenter,OutBoundsPtr);
	return !VectorAnyGreaterThan(LeftTimeMaxOfMins,LeftTimeMinOfMaxs);

}


#endif

static UBOOL BoxBoxIntersect(const FBox& box1,const FBox& box2)
{
	if( box1.Min.X > box2.Max.X || box2.Min.X > box1.Max.X )
		return FALSE;
	if( box1.Min.Y > box2.Max.Y || box2.Min.Y > box1.Max.Y )
		return FALSE;
	if( box1.Min.Z > box2.Max.Z || box2.Min.Z > box1.Max.Z )
		return FALSE;
	return TRUE;
}


// If TRACE_SingleResult, only return 1 result (the first hit).
// This code has to ignore fake-backdrop hits during shadow casting though (can't do that in ShouldTrace).
FCheckResult* FindFirstResult(FCheckResult* Hits, DWORD TraceFlags)
{
	FCheckResult* FirstResult = NULL;

	if(Hits)
	{
		FLOAT firstTime = MY_FLTMAX;
		for(FCheckResult* res = Hits; res!=NULL; res=res->GetNext())
		{
			if(res->Time < firstTime)
			{
				FirstResult = res;
				firstTime = res->Time;
			}
		}

		if(FirstResult)
			FirstResult->GetNext() = NULL;
	}

	return FirstResult;

}

/** For primitive array, only store slack up to 4 elements */
INT FOctreePrimArrayAllocator::ForAnyElementType::CalculateSlack(INT NumElements, INT NumAllocatedElements, INT NumBytesPerElement) const
{
#if USE_REDUCED_SLACK_ALLOCATOR
	if((NumElements < NumAllocatedElements) || (NumElements > 0))
	{
		// round up to nearest 4
		return ((NumElements/4)+1) * 4;
	}
	else
	{
		return 0;
	}
#else
	// Use default implementation
	return FHeapAllocator::ForAnyElementType::CalculateSlack(NumElements, NumAllocatedElements, NumBytesPerElement);
#endif // USE_REDUCED_SLACK_ALLOCATOR
}

///////////////////////////////////////////// NODE /////////////////////////////////////////////////
FOctreeNode::FOctreeNode() 
: Primitives( )
{
	Children = NULL;
	// Bounding box is set up by FOctreeNode.StoreActor
	// (or FPrimitiveOctree constructor for root node).

}

FOctreeNode::~FOctreeNode()
{
	// We call RemovePrimitives on nodes in the Octree destructor, 
	// so we should never have primitives in nodes when we destroy them.
	check(Primitives.Num() == 0);

	if(Children)
	{
		delete[] Children;
		DEC_MEMORY_STAT_BY(STAT_Octree_Memory, 8 * sizeof(FOctreeNode));
		Children = NULL;
	}
}

// Remove all primitives in this node from the octree
void FOctreeNode::RemoveAllPrimitives(FPrimitiveOctree* o)
{
	// All primitives found at this octree node, remove from octree.
	while(Primitives.Num() != 0)
	{
		UPrimitiveComponent* Primitive = Primitives(0);

		if(Primitive->OctreeNodes.Num() > 0)
		{
			o->RemovePrimitive(Primitive);
		}
		else
		{
			Primitives.RemoveItemSwap(Primitive);
			DEC_MEMORY_STAT_BY(STAT_Octree_Memory,sizeof(UPrimitiveComponent*));
			debugf(TEXT("PrimitiveComponent (%s) in Octree, but Primitive->OctreeNodes empty."), *Primitive->GetName());
		}
	}

	// Then do the same for the children (if present).
	if(Children)
	{
		for(INT i=0; i<8; i++)
			Children[i].RemoveAllPrimitives(o);
	}

}

// Filter node through tree, allowing Actor to only reside in one node.
// Assumes that Actor fits inside this node, but it might go lower.
void FOctreeNode::SingleNodeFilter(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	INT childIx = FindChild(Bounds, Primitive->Bounds.GetBox());

	if(!Children || childIx == -1)
	{
		this->StoreActor(Primitive, o, Bounds);
	}
	else
	{
		this->Children[childIx].SingleNodeFilter(Primitive, o, FOctreeNodeBounds(Bounds,childIx));
	}

}

// Filter node through tree, allowing actor to reside in multiple nodes.
// Assumes that Actor overlaps this node, but it might go lower.
// If we want to store, but the primitive is in too many nodes, we return 
// false from here and do not continue filtering primitive into tree.
UBOOL FOctreeNode::MultiNodeFilter(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// If there are no children, or this primitives bounding box completely contains this nodes
	// bounding box, store the actor at this node.
	if(!Children || Bounds.IsInsideBox(Primitive->Bounds.GetBox()) )
	{
		// If we are in too many nodes - bail out here returning false.
		if(Primitive->OctreeNodes.Num() >= MAX_NODES_PER_PRIMITIVE)
		{
			return FALSE;
		}
		else
		{
			this->StoreActor(Primitive, o, Bounds);
			return TRUE;
		}
	}
	else
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, Primitive->Bounds.GetBox(), childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			UBOOL bSuccess = this->Children[childIXs[i]].MultiNodeFilter(Primitive, o, FOctreeNodeBounds(Bounds,childIXs[i]));
			
			// If we failed to insert (prim in too many nodes), propagate failure back up tree and do not try to filter any further.
			if(!bSuccess)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

//
//	FOctreeNode::GetPrimitives - Return all primitives contained by this octree node or children.
//

void FOctreeNode::GetPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	for(INT PrimitiveIndex = 0;PrimitiveIndex < Primitives.Num();PrimitiveIndex++)
	{
		if(Primitives(PrimitiveIndex)->Tag != UPrimitiveComponent::CurrentTag)
		{
			Primitives(PrimitiveIndex)->Tag = UPrimitiveComponent::CurrentTag;
			OutPrimitives.AddItem(Primitives(PrimitiveIndex));
		}
	}
	if(Children)
	{
		for(INT ChildIndex = 0;ChildIndex < 8;ChildIndex++)
		{
			Children[ChildIndex].GetPrimitives(OutPrimitives);
		}
	}
}

//
//	FOctreeNode::GetIntersectingPrimitives - Filter bounding box through octree, returning primitives which intersect the bounding box.
//

void FOctreeNode::GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& OutPrimitives,FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds)
{
	for(INT PrimitiveIndex = 0;PrimitiveIndex < Primitives.Num();PrimitiveIndex++)
	{
		if(Primitives(PrimitiveIndex)->Tag != UPrimitiveComponent::CurrentTag)
		{
			Primitives(PrimitiveIndex)->Tag = UPrimitiveComponent::CurrentTag;
			if(BoxBoxIntersect(Box,Primitives(PrimitiveIndex)->Bounds.GetBox()))
			{
				OutPrimitives.AddItem(Primitives(PrimitiveIndex));
			}
		}
	}
	if(Children)
	{
		INT	ChildIndices[8];
		INT	NumChildren = FindChildren(Bounds,Box,ChildIndices);
		for(INT ChildIndex = 0;ChildIndex < NumChildren;ChildIndex++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,ChildIndices[ChildIndex]);
			if(ChildBounds.IsInsideBox(Box))
			{
				Children[ChildIndices[ChildIndex]].GetPrimitives(OutPrimitives);
			}
			else
			{
				Children[ChildIndices[ChildIndex]].GetIntersectingPrimitives(Box,OutPrimitives,Octree,ChildBounds);
			}
		}
	}
}

// Just for testing, this tells you which nodes the box should be in. 
void FOctreeNode::FilterTest(const FBox& TestBox, UBOOL bMulti, TArray<FOctreeNode*> *Nodes, const FOctreeNodeBounds& Bounds)
{
	if(bMulti) // Multi-Node Filter
	{
		if(!Children || Bounds.IsInsideBox(TestBox) )
		{
			Nodes->AddItem(this);
		}
		else
		{
			for(INT i=0; i<8; i++)
			{
				this->Children[i].FilterTest(TestBox, 1, Nodes, FOctreeNodeBounds(Bounds,i));
			}
		}
	}
	else // Single Node Filter
	{
		INT childIx = FindChild(Bounds, TestBox);

		if(!Children || childIx == -1)
		{
			Nodes->AddItem(this);
		}
		else
		{
			INT childIXs[8];
			INT numChildren = FindChildren(Bounds, TestBox, childIXs);
			for(INT i=0; i<numChildren; i++)
			{
				this->Children[childIXs[i]].FilterTest(TestBox, 0, Nodes, FOctreeNodeBounds(Bounds,childIXs[i]));
			}
		}
	}
}

/** Iterate over this node and children, trying to collapse up the tree, up to the point that there are MAX_PRIMITIVES_PER_NODE in a node.*/
void FOctreeNode::CollapseChildren()
{
	// Only does something if we have children!
	if(Children)
	{
		// First, call Collapse on all children
		// Then see if all child nodes are 'leaf' nodes
		UBOOL bChildrenAllLeafNodes = TRUE;
		for(INT ChildIndex = 0; ChildIndex < 8; ChildIndex++)
		{
			Children[ChildIndex].CollapseChildren();

			if(Children[ChildIndex].Children != NULL)
			{
				bChildrenAllLeafNodes = FALSE;
			}
		}

		// If all children are leaf nodes, we might be able to compact..
		if(bChildrenAllLeafNodes)
		{
			// We now find the set of primitives that are in child nodes, but not in this node
			TArray<UPrimitiveComponent*> NewChildPrimitives;

			// Get all prims from child nodes
			for(INT ChildIndex=0; ChildIndex<8; ChildIndex++)
			{
				for(INT PrimIndex=0; PrimIndex<Children[ChildIndex].Primitives.Num(); PrimIndex++)
				{
					UPrimitiveComponent* PrimComp = Children[ChildIndex].Primitives(PrimIndex);

					// If not in this node already, add to list
					if(!Primitives.ContainsItem(PrimComp))
					{
						NewChildPrimitives.AddUniqueItem( Children[ChildIndex].Primitives(PrimIndex) );
					}
				}
			}

			// If we could just put all primitives into this single node, go ahead and do that.
			if((NewChildPrimitives.Num() + Primitives.Num()) <= MAX_PRIMITIVES_PER_NODE)
			{
				// Go over each child node
				for(INT ChildIndex=0; ChildIndex<8; ChildIndex++)
				{
					FOctreeNode* ChildNode = &Children[ChildIndex];

					// For each prim in this child node, remove pointer to child node
					for(INT PrimIndex=0; PrimIndex<ChildNode->Primitives.Num(); PrimIndex++)
					{
						ChildNode->Primitives(PrimIndex)->OctreeNodes.RemoveItemSwap(ChildNode);
					}

					// then clear child's prim array
					DEC_MEMORY_STAT_BY(STAT_Octree_Memory, Primitives.Num() * sizeof(UPrimitiveComponent*));
					ChildNode->Primitives.Empty();
				}

				// Can now get rid of all child nodes
				delete[] Children;
				DEC_MEMORY_STAT_BY(STAT_Octree_Memory, 8 * sizeof(FOctreeNode));
				Children = NULL;

				// Add child primitives to this node
				for(INT PrimIndex=0; PrimIndex<NewChildPrimitives.Num(); PrimIndex++)
				{
					UPrimitiveComponent* PrimComp = NewChildPrimitives(PrimIndex);
					check(!Primitives.ContainsItem(PrimComp));
					Primitives.AddItem(PrimComp);
					INC_MEMORY_STAT_BY(STAT_Octree_Memory, sizeof(UPrimitiveComponent*));

					PrimComp->OctreeNodes.AddItem(this);
				}
			}
		}
	}
}

// We have decided to actually add an Actor at this node, so do whatever work needs doing.
void FOctreeNode::StoreActor(UPrimitiveComponent* Primitive, FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// If we are over the limit of primitives in a node, and have not already split,
	// and are not already too small, add children, and re-check each actors held here against this node.
	// 
	// Note, because we don't only hold things in leaves, 
	// we can still have more than MAX_PRIMITIVES_PER_NODE in a node.
	if(	Primitives.Num() >= MAX_PRIMITIVES_PER_NODE
		&& Children == NULL 
		&& 0.5f * Bounds.Extent > MIN_NODE_SIZE)
	{
		// Allocate memory for children nodes.
		Children = new FOctreeNode[8];
		INC_MEMORY_STAT_BY(STAT_Octree_Memory, 8 * sizeof(FOctreeNode));

		// Set up children. Calculate child bounding boxes
		//for(INT i=0; i<8; i++)
		//	CalcChildBox(&Children[i], this, i);

		// Now we need to remove each actor from this node and re-check it,
		// in case it needs to move down the Octree.
		TArray<UPrimitiveComponent*> PendingPrimitives = Primitives;
		PendingPrimitives.AddItem(Primitive);

		DEC_MEMORY_STAT_BY(STAT_Octree_Memory, Primitives.Num() * sizeof(UPrimitiveComponent*));
		Primitives.Empty();

		for(INT i=0; i<PendingPrimitives.Num(); i++)
		{
			// Remove this primitives reference to this node.
			PendingPrimitives(i)->OctreeNodes.RemoveSingleItemSwap(this);

			// Then re-check it against this node, which will then check against children.
			if(PendingPrimitives(i)->bWasSNFiltered)
			{
				this->SingleNodeFilter(PendingPrimitives(i), o, Bounds);
			}
			else
			{
				UBOOL bFilterSuccess = this->MultiNodeFilter(PendingPrimitives(i), o, Bounds);
				if(!bFilterSuccess)
				{
					o->FailedPrims.AddUniqueItem(PendingPrimitives(i));
				}
			}
		}
	}
	// We are just going to add this actor here.
	else
	{
		// Add actor to this nodes list of primitives,
		Primitives.AddItem(Primitive);
		INC_MEMORY_STAT_BY(STAT_Octree_Memory, sizeof(UPrimitiveComponent*));

		// and add this node to the primitives list of nodes.
		Primitive->OctreeNodes.AddItem(this);
	}
}

/*-----------------------------------------------------------------------------
	Recursive ZERO EXTENT line checker
-----------------------------------------------------------------------------*/
// Returns the INT whose corresponding FLOAT is smallest.
static inline INT GetNextNode(FLOAT X, INT nX, FLOAT Y, INT nY, FLOAT Z, INT nZ)
{
	if(X<Y)
		if(X<Z)
			return nX;
		else
			return nZ;
	else
		if(Y<Z)
			return nY;
		else
			return nZ;
}

#if TEST_SIMD_ZELINE || !USE_SIMD_ZELINE
// Given plane crossing points, find first sub-node that plane hits
static inline INT FindFirstNode(FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
							    FLOAT TMX, FLOAT TMY, FLOAT TMZ)
{
	// First, figure out which plane ray hits first.
	INT FirstNode = 0;
	if(T0X > T0Y)
		if(T0X > T0Z)
		{ // T0X is max - Entry Plane is YZ
			if(TMY < T0X) FirstNode |= 2;
			if(TMZ < T0X) FirstNode |= 1;
		}
		else
		{ // T0Z is max - Entry Plane is XY
			if(TMX < T0Z) FirstNode |= 4;
			if(TMY < T0Z) FirstNode |= 2;
		}
	else
		if(T0Y > T0Z)
		{ // T0Y is max - Entry Plane is XZ
			if(TMX < T0Y) FirstNode |= 4;
			if(TMZ < T0Y) FirstNode |= 1;
		}
		else
		{ // T0Z is max - Entry Plane is XY
			if(TMX < T0Z) FirstNode |= 4;
			if(TMY < T0Z) FirstNode |= 2;
		}

	return FirstNode;
}

void FOctreeNode::ActorZeroExtentLineCheck(FPrimitiveOctree* o, 
										   FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
										   FLOAT T1X, FLOAT T1Y, FLOAT T1Z, const FOctreeNodeBounds& Bounds)
{
	// If node if before start of line (ie. exit times are negative)
	if(T1X < 0.0f || T1Y < 0.0f || T1Z < 0.0f)
		return;

	FLOAT MaxHitTime;

	// If we are only looking for the first hit, dont check this node if its beyond the
	// current first hit time.
	if((o->ChkTraceFlags & TRACE_SingleResult) && o->ChkFirstResult )
		MaxHitTime = o->ChkFirstResult->Time; // Check a little beyond current best hit.
	else
		MaxHitTime = 1.0f;

	// If node is beyond end of line (ie. any entry times are > MaxHitTime)
	if(T0X > MaxHitTime || T0Y > MaxHitTime || T0Z > MaxHitTime)
		return;

#if DRAW_LINE_TRACE
	Draw(GWorld->LineBatcher, FColor(255,0,0), 0, Bounds);
#endif

	// If it does touch this box, first check line against each thing in the node.
	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);
		if(TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			// Check collision.
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;

			AActor* PrimOwner = TestPrimitive->GetOwner();

			if( !PrimOwner || PrimOwner == o->ChkActor )
			{
				continue;
			}

			if( !(o->ChkTraceFlags & TRACE_ShadowCast) )
			{
				if( !TestPrimitive->BlockZeroExtent || !TestPrimitive->ShouldCollide() )
				{
					continue;
				}
			}
			else
			{
				check(o->ChkLight);
				if( !TestPrimitive->CastShadow 
				||	!TestPrimitive->HasStaticShadowing()
				||	!o->ChkLight->AffectsPrimitive( TestPrimitive, FALSE ) 
				)
				{
					continue;
				}
			}

			UBOOL hitActorBox;

			// Check line against actor's bounding box
			//FBox ActorBox = testActor->OctreeBox;
			{
				SCOPE_CYCLE_COUNTER_SLOW(STAT_ZE_LineBox_Time);
#if !USE_SIMD_LINE_BOX
				hitActorBox = LINE_BOX(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);
#else
				hitActorBox = LineBoxIntersectSIMD(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, *o);
#if TEST_SIMD_LINE_BOX
				{
					UBOOL hitActorBox2 = LINE_BOX(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);
					if (hitActorBox2!=hitActorBox)
					{
						hitActorBox = LineBoxIntersectSIMD(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, *o);  // breakpoint this to inspect failure in the debugger
						check(hitActorBox2==hitActorBox);
					}

				}
#endif
#endif
			}
			INC_DWORD_STAT_SLOW(STAT_ZE_LineBox_Count);

#if !CHECK_FALSE_NEG
			if(!hitActorBox)
			{
				continue;
			}
#endif
			if( !o->ChkActor->IsOwnedBy(PrimOwner) 
			&& !PrimOwner->IsOwnedBy(o->ChkActor)
			&&	PrimOwner->ShouldTrace(TestPrimitive,o->ChkActor, o->ChkTraceFlags) )
			{
				UBOOL lineChkRes;
				FCheckResult Hit(0);

				{
#if STATS
					DWORD Counter = TestPrimitive->bWasSNFiltered ? (DWORD)STAT_ZE_SNF_Time : (DWORD)STAT_ZE_MNF_Time;
					DWORD Counter2 = TestPrimitive->bWasSNFiltered ? (DWORD)STAT_ZE_SNF_Count : (DWORD)STAT_ZE_MNF_Count;
					INC_DWORD_STAT(Counter2);
					SCOPE_CYCLE_COUNTER(Counter);
#endif
					lineChkRes = TestPrimitive->LineCheck(Hit, 
						o->ChkEnd, 
						o->ChkStart, 
						o->ChkExtent, 
						o->ChkTraceFlags)==0;
				}

				if( lineChkRes )
				{
#if CHECK_FALSE_NEG
					if(!hitActorBox)
						debugf(TEXT("ZELC False Neg! : %s"), *testActor->GetName());
#endif

#if !FINAL_RELEASE
					if(Hit.Normal.SizeSquared() < Square(0.99f))
					{
						debugf( TEXT("ZELC: Component '%s' with Owner '%s' returned normal with incorrect length (%f).  ChkActor: %s (%s)"),
							*TestPrimitive->GetDetailedInfo(),*PrimOwner->GetDetailedInfo(),Hit.Normal.Size(), *o->ChkActor->GetName(), *o->ChkActor->GetFullName() );

					}
#endif

					FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(Hit);
					NewResult->GetNext() = o->ChkResult;
					o->ChkResult = NewResult;

#if 0
					// DEBUG CHECK - Hit time should never before any of the entry times for this node.
					if(T0X > NewResult->Time || T0Y > NewResult->Time || T0Z > NewResult->Time)
					{
						int sdf = 0;
					}
#endif
					// Keep track of smallest hit time.
					if(!o->ChkFirstResult || NewResult->Time < o->ChkFirstResult->Time)
						o->ChkFirstResult = NewResult;

					// If we only wanted one result - our job is done!
					if (o->ChkTraceFlags & TRACE_StopAtAnyHit)
						return;
				}
			}
		}
	}

	// If we have children - then traverse them, in the order the ray passes through them.
	if(Children)
	{
		// Find middle point of node.
		FLOAT TMX = 0.5f*(T0X+T1X);
		FLOAT TMY = 0.5f*(T0Y+T1Y);
		FLOAT TMZ = 0.5f*(T0Z+T1Z);

		// Fix for parallel-axis case.
		if(o->ParallelAxis)
		{
			if(o->ParallelAxis & 4)
				TMX = (o->RayOrigin.X < Bounds.Center.X) ? MY_FLTMAX : -MY_FLTMAX;
			if(o->ParallelAxis & 2)
				TMY = (o->RayOrigin.Y < Bounds.Center.Y) ? MY_FLTMAX : -MY_FLTMAX;
			if(o->ParallelAxis & 1)
				TMZ = (o->RayOrigin.Z < Bounds.Center.Z) ? MY_FLTMAX : -MY_FLTMAX;
		}

		INT currNode = FindFirstNode(T0X, T0Y, T0Z, TMX, TMY, TMZ);
		do 
		{
			FOctreeNodeBounds	ChildBounds(Bounds, currNode ^ o->NodeTransform);
			switch(currNode) 
			{
			case 0:
				Children[0 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, T0Y, T0Z, TMX, TMY, TMZ, ChildBounds);
				currNode = GetNextNode(TMX, 4, TMY, 2, TMZ, 1);
				break;
			case 1:
				Children[1 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, T0Y, TMZ, TMX, TMY, T1Z, ChildBounds);
				currNode = GetNextNode(TMX, 5, TMY, 3, T1Z, 8);
				break;
			case 2:
				Children[2 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, TMY, T0Z, TMX, T1Y, TMZ, ChildBounds);
				currNode = GetNextNode(TMX, 6, T1Y, 8, TMZ, 3);
				break;
			case 3:
				Children[3 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, TMY, TMZ, TMX, T1Y, T1Z, ChildBounds);
				currNode = GetNextNode(TMX, 7, T1Y, 8, T1Z, 8);
				break;
			case 4:
				Children[4 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, T0Y, T0Z, T1X, TMY, TMZ, ChildBounds);
				currNode = GetNextNode(T1X, 8, TMY, 6, TMZ, 5);
				break;
			case 5:
				Children[5 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, T0Y, TMZ, T1X, TMY, T1Z, ChildBounds);
				currNode = GetNextNode(T1X, 8, TMY, 7, T1Z, 8);
				break;
			case 6:
				Children[6 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, TMY, T0Z, T1X, T1Y, TMZ, ChildBounds);
				currNode = GetNextNode(T1X, 8, T1Y, 8, TMZ, 7);
				break;
			case 7:
				Children[7 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, TMY, TMZ, T1X, T1Y, T1Z, ChildBounds);
				currNode = 8;
				break;
			}
		} while(currNode < 8);
	}

}
#endif
#if USE_SIMD_ZELINE

static VectorRegister V_FLTMAX = {MY_FLTMAX,MY_FLTMAX,MY_FLTMAX,MY_FLTMAX};
static VectorRegister V_FLTMAXNeg = {-MY_FLTMAX,-MY_FLTMAX,-MY_FLTMAX,-MY_FLTMAX};

static VectorRegister V_INT4 = MakeVectorRegister( (DWORD)4, (DWORD)4, (DWORD)4, (DWORD)0 );
static VectorRegister V_INT2 = MakeVectorRegister( (DWORD)2, (DWORD)2, (DWORD)2, (DWORD)0 );
static VectorRegister V_INT1 = MakeVectorRegister( (DWORD)1, (DWORD)1, (DWORD)1, (DWORD)0 );

static VectorRegister VNextChildren[8] = 
{
	MakeVectorRegister( (DWORD)4, (DWORD)2, (DWORD)1, (DWORD)0 ),
	MakeVectorRegister( (DWORD)5, (DWORD)3, (DWORD)8, (DWORD)0 ),
	MakeVectorRegister( (DWORD)6, (DWORD)8, (DWORD)3, (DWORD)0 ),
	MakeVectorRegister( (DWORD)7, (DWORD)8, (DWORD)8, (DWORD)0 ),
	MakeVectorRegister( (DWORD)8, (DWORD)6, (DWORD)5, (DWORD)0 ),
	MakeVectorRegister( (DWORD)8, (DWORD)7, (DWORD)8, (DWORD)0 ),
	MakeVectorRegister( (DWORD)8, (DWORD)8, (DWORD)7, (DWORD)0 ),
	MakeVectorRegister( (DWORD)8, (DWORD)8, (DWORD)8, (DWORD)0 ),
};

MS_ALIGN(16) struct FZETraversalNumbers
{
	FLOAT T0[4];
	// T0[3] == o->ChkFirstResult->Time or 1.0 depending on "stop at any hit" flag
	FLOAT TM[4];
	FLOAT T1[4];

	FZETraversalNumbers()
	{
	}

	FZETraversalNumbers(FLOAT T0X,FLOAT T0Y,FLOAT T0Z,FLOAT T1X,FLOAT T1Y,FLOAT T1Z,FLOAT Limit)
	{
		T0[0] = T0X;
		T0[1] = T0Y;
		T0[2] = T0Z;
		T0[3] = Limit;
		T1[0] = T1X;
		T1[1] = T1Y;
		T1[2] = T1Z;
		T1[3] = 0.0f;
		TM[0] = 0.0f;
		TM[1] = 0.0f;
		TM[2] = 0.0f;
		TM[3] = 0.0f;
	}
	// Given plane crossing points, find first sub-node that plane hits
	FORCEINLINE_DEBUGGABLE INT FindFirstNodeOld()
	{
		// First, figure out which plane ray hits first.
		INT FirstNode = 0;
		if(T0[0] > T0[1])
		{
			if(T0[0] > T0[2])
			{ // T0[0] is max - Entry Plane is YZ
				if(TM[0] < T0[0]) FirstNode |= 0;
				if(TM[1] < T0[0]) FirstNode |= 2;
				if(TM[2] < T0[0]) FirstNode |= 1;
			}
			else
			{ // T0[2] is max - Entry Plane is XY
				if(TM[0] < T0[2]) FirstNode |= 4;
				if(TM[1] < T0[2]) FirstNode |= 2;
				if(TM[2] < T0[2]) FirstNode |= 0;
			}
		}
		else
		{
			if(T0[1] > T0[2])
			{ // T0[1] is max - Entry Plane is XZ
				if(TM[0] < T0[1]) FirstNode |= 4;
				if(TM[1] < T0[1]) FirstNode |= 0;
				if(TM[2] < T0[1]) FirstNode |= 1;
			}
			else
			{ // T0[2] is max - Entry Plane is XY
				if(TM[0] < T0[2]) FirstNode |= 4;
				if(TM[1] < T0[2]) FirstNode |= 2;
				if(TM[2] < T0[2]) FirstNode |= 0;
			}
		}
		return FirstNode;
	}

	FORCEINLINE_DEBUGGABLE void FindFirstNode(FPrimitiveOctree* o,const FOctreeNodeBounds& Bounds,DWORD *OutNextChild)
	{
		DWORD* RESTRICT OutNextChildPtr = OutNextChild;
		checkSlow(
			VectorIsAligned(OutNextChildPtr)
			);
		VectorRegister VParallelMask = VectorLoadAligned(o->ParallelMask);
		VectorRegister VBounds = VectorLoadAligned(&Bounds.Center.X);
		VectorRegister VRayOrigin = VectorLoadAligned(&o->RayOrigin.X);
		VectorRegister VT0 = VectorLoadAligned(T0);
		VectorRegister VT1 = VectorLoadAligned(T1);

		VectorRegister VTM = VectorMultiply(VectorAdd(VT0,VT1),V_Half);

		VectorRegister VRayLessThanCenter = VectorCompareGT(VBounds,VRayOrigin);
		VectorRegister VInfSides = VectorSelect(VRayLessThanCenter,V_FLTMAX,V_FLTMAXNeg);
		VTM = VectorSelect(VParallelMask,VInfSides,VTM);

		VectorRegister VT0S1 = VectorSwizzle(VT0,1,2,0,3);
		VectorRegister VT0S2 = VectorSwizzle(VT0,2,0,1,3);

		VectorRegister VLessThan = VectorCompareGT(VT0,VT0S1);
		VectorRegister VFirstX = VectorSwizzle(VectorSelect(VLessThan,VectorZero(),V_INT4),0,3,3,3);
		VectorRegister VFirstY = VectorSwizzle(VectorSelect(VLessThan,V_INT2,VectorZero()),3,0,3,3);
		VectorRegister VFirst = VectorBitwiseOr(VFirstX,VFirstY);
		VT0S1 = VectorSelect(VLessThan,VT0,VT0S1);
		VLessThan = VectorCompareGT(VT0S1,VT0S2);
		VectorRegister VFirstZ = VectorSwizzle(VectorSelect(VLessThan,V_INT1,VectorZero()),3,3,0,3);
		VectorRegister VFirstY2 = VectorSwizzle(VectorSelect(VLessThan,VectorZero(),V_INT2),3,0,3,3);
		VectorRegister VFirstX2 = VectorSwizzle(VectorSelect(VLessThan,VectorZero(),V_INT4),0,3,3,3);
		VT0S1 = VectorSelect(VLessThan,VT0S1,VT0S2);
		VLessThan = VectorCompareGT(VT0S1,VTM);

		VFirst = VectorBitwiseOr(VectorBitwiseOr(VFirst,VFirstX2),VectorBitwiseOr(VFirstZ,VFirstY2));
		VFirst = VectorSelect(VLessThan,VFirst,VectorZero());
		VectorRegister VFirstS1 = VectorSwizzle(VFirst,1,2,0,3);
		VectorRegister VFirstS2 = VectorSwizzle(VFirst,2,0,1,3);
		VFirst = VectorBitwiseOr(VectorBitwiseOr(VFirstS1,VFirst),VFirstS2);
		VectorStoreAligned(VFirst,OutNextChildPtr);

		VectorStoreAligned(VTM,TM);

	}

	FORCEINLINE_DEBUGGABLE DWORD FromParentCull(const FZETraversalNumbers &Parent,INT Index,const FOctreeNodeBounds& InParentBounds,INT SwizzleIndex,FOctreeNodeBounds& OutBounds,DWORD *OutNextChild)
	{
		const FLOAT* RESTRICT BoxCenterPtr = &InParentBounds.Center.X;
		FLOAT* RESTRICT OutBoundsPtr = &OutBounds.Center.X;
		DWORD* RESTRICT OutNextChildPtr = OutNextChild;
		checkSlow(
			VectorIsAligned(BoxCenterPtr) &&
			VectorIsAligned(OutBoundsPtr) &&
			VectorIsAligned(OutNextChildPtr)
			);

		VectorRegister VT0P = VectorLoadAligned(Parent.T0);
		VectorRegister MaxTime = VectorSwizzle(VT0P,3,3,3,3);
		VectorRegister VTMP = VectorLoadAligned(Parent.TM);
		VectorRegister VT1P = VectorLoadAligned(Parent.T1);
		VectorRegister VMask = VChildMaxMasks[Index];

		VectorRegister VT1 = VectorSelect(VMask,VT1P,VTMP);
		VectorRegister VT0 = VectorSelect(VMask,VTMP,VT0P);

		VectorRegister VNextNodes = VNextChildren[Index];

		VectorRegister VNextNodesS1 = VectorSwizzle(VNextNodes,1,2,0,3);
		VectorRegister VNextNodesS2 = VectorSwizzle(VNextNodes,2,0,1,3);
		VectorRegister VT1S1 = VectorSwizzle(VT1,1,2,0,3);
		VectorRegister VT1S2 = VectorSwizzle(VT1,2,0,1,3);

		VectorRegister VLessThan = VectorCompareGT(VT1S1,VT1);
		VNextNodes = VectorSelect(VLessThan,VNextNodes,VNextNodesS1);
		VT1S1 = VectorSelect(VLessThan,VT1,VT1S1);
		VLessThan = VectorCompareGT(VT1S2,VT1S1);
		VNextNodes = VectorSelect(VLessThan,VNextNodes,VNextNodesS2);

		VectorRegister BoxCenter = VectorLoadAligned(BoxCenterPtr);
		VectorStoreAligned(VNextNodes,OutNextChildPtr);
		VectorRegister BoxRadii = VectorSwizzle(BoxCenter,3,3,3,3); 
		VectorRegister HalfBoxRadii = VectorMultiply(BoxRadii,V_Half);
		BoxCenter = VectorSubtract(BoxCenter,HalfBoxRadii);
		VectorRegister LeftCenterAdd = VectorSelect(VChildMaxMasks[SwizzleIndex],BoxRadii,VectorZero());
		VectorRegister LeftBoxCenter = VectorAdd(BoxCenter,LeftCenterAdd);
		// unneeded LeftBoxCenter = VectorSelect(VReplace4thMask,LeftBoxCenter,HalfBoxRadii);


		VectorStoreAligned(LeftBoxCenter,OutBoundsPtr);
		VectorStoreAligned(VT0,T0);
		VectorStoreAligned(VT1,T1);
		DWORD Ret = VectorAnyGreaterThan(VectorZero(),VT1);
		Ret |= VectorAnyGreaterThan(VT0,MaxTime);

		return Ret;
	}
} GCC_ALIGN(16);

void FOctreeNode::ActorZeroExtentLineCheck(FPrimitiveOctree* o, FZETraversalNumbers &Traversal, const FOctreeNodeBounds& Bounds)
{
#if DRAW_LINE_TRACE
	Draw(GWorld->LineBatcher, FColor(255,0,0), 0, Bounds);
#endif
	INT PrimitivesNum = Primitives.Num();
	INT PrimitiveIndex = 0;
	for(INT i=0; i<PrimitivesNum && i < 6; i++)
	{
		CONSOLE_PREFETCH(&Primitives(i)->Tag);
	}
	// If we have children - then traverse them, in the order the ray passes through them.
	MS_ALIGN(16) DWORD NextChild[4] GCC_ALIGN(16);
	if(Children)
	{ 
		CONSOLE_PREFETCH(Children);
		// this is moved up here because there is a LHS on the NextChild
		Traversal.FindFirstNode(o,Bounds,NextChild);
	}

	// If it does touch this box, first check line against each thing in the node.
	for(;PrimitiveIndex<PrimitivesNum;)
	{
		UPrimitiveComponent* TestPrimitive = Primitives(PrimitiveIndex);
		PrimitiveIndex++;

		if(TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			// Check collision.
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;

			AActor* PrimOwner = TestPrimitive->GetOwner();

			if( !PrimOwner || PrimOwner == o->ChkActor )
			{
				continue;
			}


			if( !(o->ChkTraceFlags & TRACE_ShadowCast) )
			{
				if( !TestPrimitive->BlockZeroExtent || !TestPrimitive->ShouldCollide() )
				{
					continue;
				}
			}
			else
			{
				check(o->ChkLight);
				if( !TestPrimitive->CastShadow 
					||	!TestPrimitive->HasStaticShadowing()
					||	!o->ChkLight->AffectsPrimitive( TestPrimitive, FALSE ) 
					)
				{
					continue;
				}
			}

			UBOOL hitActorBox;

			// Check line against actor's bounding box
			//FBox ActorBox = testActor->OctreeBox;
			{
				SCOPE_CYCLE_COUNTER_SLOW(STAT_ZE_LineBox_Time);
#if !USE_SIMD_LINE_BOX
				hitActorBox = LINE_BOX(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);
#else
				hitActorBox = LineBoxIntersectSIMD(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, *o, Traversal.T0);
#if TEST_SIMD_LINE_BOX
				{
					UBOOL hitActorBox2 = LINE_BOX(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);
					if (hitActorBox2!=hitActorBox)
					{
						hitActorBox = LineBoxIntersectSIMD(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, *o);  // breakpoint this to inspect failure in the debugger
						check(hitActorBox2==hitActorBox);
					}

				}
#endif
#endif
			}
			INC_DWORD_STAT_SLOW(STAT_ZE_LineBox_Count);

#if !CHECK_FALSE_NEG
			if(!hitActorBox)
			{
				continue;
			}
#endif
			if( !o->ChkActor->IsOwnedBy(PrimOwner) 
				&& !PrimOwner->IsOwnedBy(o->ChkActor)
				&&	PrimOwner->ShouldTrace(TestPrimitive,o->ChkActor, o->ChkTraceFlags) )
			{
				UBOOL lineChkRes;
				FCheckResult Hit(0);

				{
#if STATS
					DWORD Counter = TestPrimitive->bWasSNFiltered ? (DWORD)STAT_ZE_SNF_Time : (DWORD)STAT_ZE_MNF_Time;
					DWORD Counter2 = TestPrimitive->bWasSNFiltered ? (DWORD)STAT_ZE_SNF_Count : (DWORD)STAT_ZE_MNF_Count;
					INC_DWORD_STAT(Counter2);
					SCOPE_CYCLE_COUNTER(Counter);
#endif
					lineChkRes = TestPrimitive->LineCheck(Hit, 
						o->ChkEnd, 
						o->ChkStart, 
						o->ChkExtent, 
						o->ChkTraceFlags)==0;
				}

				if( lineChkRes )
				{
#if CHECK_FALSE_NEG
					if(!hitActorBox)
						debugf(TEXT("ZELC False Neg! : %s"), *testActor->GetName());
#endif

#if !FINAL_RELEASE
					if(Hit.Normal.SizeSquared() < Square(0.99f))
					{
						debugf( TEXT("ZELC: Component '%s' with Owner '%s' returned normal with incorrect length (%f).  ChkActor: %s (%s)"),
							*TestPrimitive->GetDetailedInfo(),*PrimOwner->GetDetailedInfo(),Hit.Normal.Size(), *o->ChkActor->GetName(), *o->ChkActor->GetFullName() );

					}
#endif

					FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(Hit);
					NewResult->GetNext() = o->ChkResult;
					o->ChkResult = NewResult;

#if 0
					// DEBUG CHECK - Hit time should never before any of the entry times for this node.
					if(T0X > NewResult->Time || T0Y > NewResult->Time || T0Z > NewResult->Time)
					{
						int sdf = 0;
					}
#endif
					// Keep track of smallest hit time.
					if(!o->ChkFirstResult || NewResult->Time < o->ChkFirstResult->Time)
					{
						o->ChkFirstResult = NewResult;
						if((o->ChkTraceFlags & TRACE_SingleResult))
						{
							Traversal.T0[3] = o->ChkFirstResult->Time;
						}
					}

					// If we only wanted one result - our job is done!
					if (o->ChkTraceFlags & TRACE_StopAtAnyHit)
						return;
				}
			}
		}
	}

	// If we have children - then traverse them, in the order the ray passes through them.
	if(Children)
	{
		FZETraversalNumbers Child;
		FOctreeNodeBounds	ChildBounds;
		INT currNode;

#if TEST_SIMD_ZELINE
		if (Traversal.FindFirstNodeOld() != NextChild[0])
		{
			debugf(TEXT("SIMD was %d, old was %d"), NextChild[0],Traversal.FindFirstNodeOld() );
			Traversal.FindFirstNode(o,Bounds,NextChild);
			check(Traversal.FindFirstNodeOld() == NextChild[0])
		}
#endif

		while (1) 
		{
			currNode = NextChild[0];
			if (currNode > 7)
			{
				break;
			}

			if (!Child.FromParentCull(Traversal,currNode,Bounds,currNode ^ o->NodeTransform,ChildBounds,NextChild))
			{
				Children[currNode ^ o->NodeTransform].ActorZeroExtentLineCheck(o, Child , ChildBounds);
				Traversal.T0[3] = Child.T0[3];  // propagate any shortened times up
			}
#if TEST_SIMD_ZELINE
			switch(currNode) 
			{
			case 0:
				currNode = GetNextNode(Traversal.TM[0], 4, Traversal.TM[1], 2, Traversal.TM[2], 1);
				break;
			case 1:
				currNode = GetNextNode(Traversal.TM[0], 5, Traversal.TM[1], 3, Traversal.T1[2], 8);
				break;
			case 2:
				currNode = GetNextNode(Traversal.TM[0], 6, Traversal.T1[1], 8, Traversal.TM[2], 3);
				break;
			case 3:
				currNode = GetNextNode(Traversal.TM[0], 7, Traversal.T1[1], 8, Traversal.T1[2], 8);
				break;
			case 4:
				currNode = GetNextNode(Traversal.T1[0], 8, Traversal.TM[1], 6, Traversal.TM[2], 5);
				break;
			case 5:
				currNode = GetNextNode(Traversal.T1[0], 8, Traversal.TM[1], 7, Traversal.T1[2], 8);
				break;
			case 6:
				currNode = GetNextNode(Traversal.T1[0], 8, Traversal.T1[1], 8, Traversal.TM[2], 7);
				break;
			case 7:
				currNode = 8;
				break;
			}
			check(currNode == NextChild[0]);
#endif
		}
	}

}
#endif

/*-----------------------------------------------------------------------------
	Recursive NON-ZERO EXTENT line checker
-----------------------------------------------------------------------------*/
// This assumes that the ray check overlaps this node.
void FOctreeNode::ActorNonZeroExtentLineCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	INT childIXs[8];
	INT numChildren = 0;
	INT PrimitiveIndex = 0;
	INT PrimitivesNum = Primitives.Num();
	for(INT i=0; i<PrimitivesNum && i < 6; i++)
	{
		CONSOLE_PREFETCH(&Primitives(i)->Tag);
	}
	if(Children)
	{
		CONSOLE_PREFETCH(Children);
		numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
	}
	for(;PrimitiveIndex<PrimitivesNum;)
	{
		UPrimitiveComponent* TestPrimitive = Primitives(PrimitiveIndex);
		PrimitiveIndex++;

		if(TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;

			AActor* PrimOwner = TestPrimitive->GetOwner();

			if( !PrimOwner || PrimOwner == o->ChkActor )
			{
				continue;
			}

			// Check line against actor's bounding box
			UBOOL hitActorBox;
			{
				INC_DWORD_STAT_SLOW(STAT_NZE_LineBox_Count);
				SCOPE_CYCLE_COUNTER_SLOW(STAT_NZE_LineBox_Time);
#if !USE_SIMD_LINE_BOX
				hitActorBox = LINE_BOX(TestPrimitive->Bounds.Origin, 
					TestPrimitive->Bounds.BoxExtent + o->ChkExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);
#else
				hitActorBox = LineBoxIntersectSIMD(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, *o);
#if TEST_SIMD_LINE_BOX
				{
					UBOOL hitActorBox2 = LINE_BOX(TestPrimitive->Bounds.Origin, 
						TestPrimitive->Bounds.BoxExtent + o->ChkExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);
					if (hitActorBox2!=hitActorBox)
					{
						hitActorBox = LineBoxIntersectSIMD(TestPrimitive->Bounds.Origin, TestPrimitive->Bounds.BoxExtent, *o);  // breakpoint this to inspect failure in the debugger
						check(hitActorBox2==hitActorBox);
					}
				}
#endif
#endif
			}
#if !CHECK_FALSE_NEG
			if(!hitActorBox)
			{
				continue;
			}
#endif

			// Check collision.
			if( TestPrimitive->BlockNonZeroExtent &&
				TestPrimitive->ShouldCollide() &&
				!o->ChkActor->IsOwnedBy(PrimOwner) &&
				!PrimOwner->IsOwnedBy(o->ChkActor) && 
				PrimOwner->ShouldTrace(TestPrimitive, o->ChkActor, o->ChkTraceFlags) )
			{
				FCheckResult TestHit(0);
				UBOOL lineChkRes;
				{
#if STATS
					DWORD Counter = (DWORD)STAT_NZE_SNF_Count;
					DWORD Counter2 = (DWORD)STAT_NZE_SNF_Time;
					if (TestPrimitive->bWasSNFiltered == FALSE)
					{
						Counter = (DWORD)STAT_NZE_MNF_Count;
						Counter2 = (DWORD)STAT_NZE_MNF_Time;
					}
					INC_DWORD_STAT(Counter);
					SCOPE_CYCLE_COUNTER(Counter2);
#endif
					lineChkRes = TestPrimitive->LineCheck(TestHit, 
						o->ChkEnd, 
						o->ChkStart, 
						o->ChkExtent, 
						o->ChkTraceFlags)==0; 
				}

				if(lineChkRes)
				{
#if CHECK_FALSE_NEG
					if(!hitActorBox)
						debugf(TEXT("NZELC False Neg! : %s"), *testActor->GetName());
#endif

#if !FINAL_RELEASE
					// Check for normals that are less than 1.f long
					if(TestHit.Normal.SizeSquared() < Square(0.99f))
					{
						debugfSuppressed(NAME_DevCollision, TEXT("NZELC: Component '%s' with Owner '%s' returned normal with incorrect length (%f) ChkActor: %s"),
							*TestPrimitive->GetDetailedInfo(),*PrimOwner->GetDetailedInfo(),TestHit.Normal.Size(), *o->ChkActor->GetName());
					}
#endif

					FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
					NewResult->GetNext() = o->ChkResult;
					o->ChkResult = NewResult;

					// If we only wanted one result - our job is done!
					if (o->ChkTraceFlags & TRACE_StopAtAnyHit)
						return;
				}

			}

		}
	}

	// Now traverse children of this node if present.
#if !USE_SIMD_LINE_BOX
	if(numChildren)
	{
		for(INT i=0; i<numChildren; i++)
		{
			UBOOL hitsChild;
			FOctreeNodeBounds	ChildBounds(Bounds,childIXs[i]);

			{
				INC_DWORD_STAT_SLOW(STAT_NZE_LineBox_Count);
				SCOPE_CYCLE_COUNTER_SLOW(STAT_NZE_LineBox_Time);
				// First - check extent line against child bounding box. 
				// We expand box it by the extent of the line.
				hitsChild = LINE_BOX(ChildBounds.Center, 
					FVector(ChildBounds.Extent + o->ChkExtent.X, ChildBounds.Extent + o->ChkExtent.Y, ChildBounds.Extent + o->ChkExtent.Z),
					o->ChkStart, o->ChkDir, o->ChkOneOverDir);
			}

			// If ray hits child node - go into it.
			if(hitsChild)
			{
				this->Children[childIXs[i]].ActorNonZeroExtentLineCheck(o, ChildBounds);

				// If that child resulted in a hit, and we only want one, return now.
				if ( o->ChkResult && (o->ChkTraceFlags & TRACE_StopAtAnyHit) )
					return;
			}
		}
	}
#else
	// Now traverse children of this node if present.
	if(numChildren)
	{
		for(INT i=0; i<numChildren; i++)
		{
			UBOOL hitsChild;
			FOctreeNodeBounds	ChildBounds;

			{
				INC_DWORD_STAT_SLOW(STAT_NZE_LineBox_Count);
				SCOPE_CYCLE_COUNTER_SLOW(STAT_NZE_LineBox_Time);
				// First - check extent line against child bounding box. 
				// We expand box it by the extent of the line.
				hitsChild = LineBoxIntersectSIMD(Bounds,childIXs[i],*o,ChildBounds);
#if TEST_SIMD_LINE_BOX
				{
					UBOOL hitsChild2 = LINE_BOX(ChildBounds.Center, 
						FVector(ChildBounds.Extent + o->ChkExtent.X, ChildBounds.Extent + o->ChkExtent.Y, ChildBounds.Extent + o->ChkExtent.Z),
						o->ChkStart, o->ChkDir, o->ChkOneOverDir);
					if (hitsChild2!=hitsChild)
					{
						hitsChild = LineBoxIntersectSIMD(Bounds,childIXs[i],*o,ChildBounds);
						check(hitsChild2==hitsChild);
					}
				}
#endif
			}

			// If ray hits child node - go into it.
			if(hitsChild)
			{
				this->Children[childIXs[i]].ActorNonZeroExtentLineCheck(o, ChildBounds);

				// If that child resulted in a hit, and we only want one, return now.
				if ( o->ChkResult && (o->ChkTraceFlags & TRACE_StopAtAnyHit) )
					return;
			}
		}
	}
#endif
}

/*-----------------------------------------------------------------------------
	Recursive encroachment check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorEncroachmentCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// o->ChkActor is the non-cylinder thing that is moving (mover, karma etc.).
	// Actors(i) is the thing (Pawn, Volume, Projector etc.) that its moving into.
	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);

		AActor* PrimOwner = TestPrimitive->GetOwner();
		if(	TestPrimitive->Tag != UPrimitiveComponent::CurrentTag &&
			PrimOwner && (PrimOwner->OverlapTag != UPrimitiveComponent::CurrentTag || GIsEditor))
		{
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;

			// Check bounding boxes against each other
			UBOOL hitActorBox;
			{
				SCOPE_CYCLE_COUNTER_SLOW(STAT_BoxBoxTime);
				hitActorBox = BoxBoxIntersect(TestPrimitive->Bounds.GetBox(), o->ChkBox);
			}
			INC_DWORD_STAT_SLOW(STAT_BoxBoxCount);

#if !CHECK_FALSE_NEG
			if(!hitActorBox)
			{
				continue;
			}
#endif

			// Skip if we've already checked this actor, or we're joined to the encroacher,
			// or this is a mover and the other thing is the world (static mesh, terrain etc.)
			if (TestPrimitive->ShouldCollide() &&
				!PrimOwner->IsBasedOn(o->ChkActor) &&
				PrimOwner->ShouldTrace(TestPrimitive,o->ChkActor,o->ChkTraceFlags) &&
				!((o->ChkActor->Physics == PHYS_Interpolating) && PrimOwner->bWorldGeometry) )
			{
				FCheckResult TestHit(1.f);
				if(o->ChkActor->IsOverlapping(PrimOwner, &TestHit, TestPrimitive,o->ChkPrimComp))
				{
#if CHECK_FALSE_NEG
					if(!hitActorBox)
						debugf(TEXT("ENC False Neg! : %s %s"), *o->ChkActor->GetName(), *PrimOwner->GetName());
#endif
					TestHit.Actor = PrimOwner;				

					FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
					NewResult->GetNext() = o->ChkResult;
					o->ChkResult = NewResult;

					// we only want to return any given Actor once - mark it so it is not checked again
					PrimOwner->OverlapTag = UPrimitiveComponent::CurrentTag;
				}
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,childIXs[i]);
			this->Children[childIXs[i]].ActorEncroachmentCheck(o, ChildBounds);
			// JTODO: Should we check TRACE_StopAtAnyHit and bail out early for Encroach check?
		}
	}
}

/*-----------------------------------------------------------------------------
	Recursive point (with extent) check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorPointCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// First, see if this actors box overlaps tthe query point
	// If it doesn't - return straight away.
	//FBox TestBox = FBox(o->ChkStart - o->ChkExtent, o->ChkStart + o->ChkExtent);
	//if( !Box.Intersect(o->ChkBox) )
	//	return;

	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);

		// Skip if we've already checked this actor.
		if (TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;

			AActor* PrimOwner = TestPrimitive->GetOwner();		
			if(PrimOwner)
			{
				UBOOL hitActorBox;
				{
					SCOPE_CYCLE_COUNTER_SLOW(STAT_BoxBoxTime);
					// Check actor box against query box.
					hitActorBox = BoxBoxIntersect(TestPrimitive->Bounds.GetBox(), o->ChkBox);
				}
				INC_DWORD_STAT_SLOW(STAT_BoxBoxCount);

#if !CHECK_FALSE_NEG
				if(!hitActorBox)
				{
					continue;
				}
#endif
				if ((o->bChkExtentIsZero ? TestPrimitive->BlockZeroExtent : TestPrimitive->BlockNonZeroExtent) &&
					TestPrimitive->ShouldCollide() &&
					PrimOwner->ShouldTrace(TestPrimitive,NULL, o->ChkTraceFlags) )
				{
					// Collision test.
					FCheckResult TestHit(1.f);
					if (TestPrimitive->PointCheck(TestHit, o->ChkStart, o->ChkExtent, o->ChkTraceFlags) == 0)
					{
						check(TestHit.Actor == PrimOwner);

#if CHECK_FALSE_NEG
				if(!hitActorBox)
					debugf(TEXT("PC False Neg! : %s %s"), testActor->GetName());
#endif

						FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
						NewResult->GetNext() = o->ChkResult;
						o->ChkResult = NewResult;
						if (o->ChkTraceFlags & TRACE_StopAtAnyHit)
							return;
					}
				}
			}
		}
	}
	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,childIXs[i]);
			this->Children[childIXs[i]].ActorPointCheck(o,ChildBounds);
			// JTODO: Should we check TRACE_StopAtAnyHit and bail out early for Encroach check?
		}
	}
}

/*-----------------------------------------------------------------------------
	Recursive radius check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorRadiusCheck(FPrimitiveOctree* o, const FOctreeNodeBounds& Bounds)
{
	// First, see if this actors box overlaps tthe query point
	// If it doesn't - return straight away.
	//FBox TestBox = FBox(o->ChkStart - o->ChkExtent, o->ChkStart + o->ChkExtent);
	//if( !Box.Intersect(o->ChkBox) )
	//	return;

	for(INT i=0; i<Primitives.Num(); i++)
	{
		UPrimitiveComponent*	TestPrimitive = Primitives(i);

		// Skip if we've already checked this actor.
		if( TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;

			AActor* PrimOwner = TestPrimitive->GetOwner();
			if (PrimOwner)
			{
				// Only check the actor once unless forced
				if ( (PrimOwner->OverlapTag != UPrimitiveComponent::CurrentTag) || PrimOwner->ForceReturnComponent(TestPrimitive) )
				{
					if( (TestPrimitive->Bounds.Origin - o->ChkStart).SizeSquared() < o->ChkRadiusSqr )
					{
						// Add to result linked list
						FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult;
						NewResult->Actor		= PrimOwner;
						NewResult->Component	= TestPrimitive;
						NewResult->GetNext()	= o->ChkResult;
						o->ChkResult			= NewResult;
						PrimOwner->OverlapTag = UPrimitiveComponent::CurrentTag;
					}
				}
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FOctreeNodeBounds	ChildBounds(Bounds,childIXs[i]);
			this->Children[childIXs[i]].ActorRadiusCheck(o,ChildBounds);
			// JTODO: Should we check TRACE_StopAtAnyHit and bail out early for Encroach check?
		}
	}
}

/**
 * Filters a sphere through the octree nodes finding actors that overlap with
 * the sphere
 *
 * @param Octree the octree being searched
 * @param Bounds the bounds of the node
 * @param Check the precalculated info for this check
 */
void FOctreeNode::ActorRadiusOverlapCheck(FPrimitiveOctree* Octree,
	const FOctreeNodeBounds& Bounds,FRadiusOverlapCheck& Check)
{
	// Test the primitives contained within this node
	for (INT Index = 0; Index < Primitives.Num(); Index++)
	{
		UPrimitiveComponent* TestPrimitive = Primitives(Index);
		// Skip if we've already checked this component
		if (TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;
			AActor* PrimOwner = TestPrimitive->GetOwner();
			if (PrimOwner && PrimOwner->OverlapTag != UPrimitiveComponent::CurrentTag)
			{
				// Add this primitive if it overlaps
				if (Check.SphereBoundsTest(TestPrimitive->Bounds))
				{
					FCheckResult* NewResult = new(*(Octree->ChkMem))FCheckResult;
					NewResult->Actor		= PrimOwner;
					NewResult->Component	= TestPrimitive;
					NewResult->GetNext()	= Octree->ChkResult;
					Octree->ChkResult		= NewResult;
					// tag this actor so that we don't check it again
					PrimOwner->OverlapTag = UPrimitiveComponent::CurrentTag;
				}
			}
		}
	}

	// Now traverse children of this node if present.
	if (Children)
	{
		INT ChildIXs[8];
		INT NumChildren = FindChildren(Bounds, Octree->ChkBox, ChildIXs);
		for (INT Index = 0; Index < NumChildren; Index++)
		{
			FOctreeNodeBounds ChildBounds(Bounds,ChildIXs[Index]);
			Children[ChildIXs[Index]].ActorRadiusOverlapCheck(Octree,ChildBounds,Check);
		}
	}
}

/**
 * Filters a sphere through the octree nodes finding actors that overlap with
 * the sphere. Optionally only affects physics objects.
 *
 * @param	Octree				The octree being searched.
 * @param	Bounds				The bounds of the node.
 * @param	Check				The precalculated info for this check.
 * @param	bAllComponents		If FALSE, report only the first component per actor.
 */
void FOctreeNode::ActorOverlapCheck(FPrimitiveOctree* Octree,
	const FOctreeNodeBounds& Bounds,FRadiusOverlapCheck& Check,UBOOL bAllComponents)
{
	// Test the primitives contained within this node
	for (INT Index = 0; Index < Primitives.Num(); Index++)
	{
		UPrimitiveComponent* TestPrimitive = Primitives(Index);
		// Skip if we've already checked this component
		if (TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;
			AActor* PrimOwner = TestPrimitive->GetOwner();
			if (PrimOwner && PrimOwner != Octree->ChkActor)
			{
				// If CollideActors is false, or we have an owner with bCollideActors == false, do not return this Actor.
				if (TestPrimitive->CollideActors && PrimOwner->bCollideActors)
				{
					// If all components have not been requested, only check the actor.
					if (bAllComponents || PrimOwner->OverlapTag != UPrimitiveComponent::CurrentTag)
					{
                        // Add this primitive if it overlaps
						if (Check.SphereBoundsTest(TestPrimitive->Bounds))
						{
							FCheckResult* NewResult = new(*(Octree->ChkMem))FCheckResult;
							NewResult->Actor		= PrimOwner;
							NewResult->Component	= TestPrimitive;
							NewResult->GetNext()	= Octree->ChkResult;
							Octree->ChkResult		= NewResult;
							PrimOwner->OverlapTag = UPrimitiveComponent::CurrentTag;
						}
					}
				}
			}
		}
	}

	// Now traverse children of this node if present.
	if (Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, Octree->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			Children[childIXs[i]].ActorOverlapCheck(Octree,FOctreeNodeBounds(Bounds,childIXs[i]),Check,bAllComponents);
		}
	}

}

/**
 * Finds all pawns that are touched by a sphere (point + radius).
 *
 * @param	Octree				The octree being searched.
 * @param	Bounds				The bounds of the node.
 * @param	Check				The precalculated info for this check.
 */
void FOctreeNode::PawnOverlapCheck(FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds, FRadiusOverlapCheck& Check)
{
	// Test the primitives contained within this node
	for (INT Index = 0; Index < Primitives.Num(); Index++)
	{
		UPrimitiveComponent* TestPrimitive = Primitives(Index);
		// Skip if we've already checked this component
		if (TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;
			AActor* PrimOwner = TestPrimitive->GetOwner();
			if (PrimOwner && PrimOwner != Octree->ChkActor && PrimOwner->GetAPawn())
			{
				// If CollideActors is false, or we have an owner with bCollideActors == false, do not return this Actor.
				if (TestPrimitive->CollideActors && PrimOwner->bCollideActors)
				{
					// If all components have not been requested, only check the actor.
					if (PrimOwner->OverlapTag != UPrimitiveComponent::CurrentTag)
					{
						// Add this primitive if it overlaps
						if (Check.SphereBoundsTest(TestPrimitive->Bounds))
						{
							FCheckResult* NewResult = new(*(Octree->ChkMem))FCheckResult;
							NewResult->Actor		= PrimOwner;
							NewResult->Component	= TestPrimitive;
							NewResult->GetNext()	= Octree->ChkResult;
							Octree->ChkResult		= NewResult;
							PrimOwner->OverlapTag = UPrimitiveComponent::CurrentTag;
						}
					}
				}
			}
		}
	}

	// Now traverse children of this node if present.
	if (Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, Octree->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			Children[childIXs[i]].PawnOverlapCheck(Octree,FOctreeNodeBounds(Bounds,childIXs[i]),Check);
		}
	}

}

/**
 * Finds all actors that are touched by a sphere (point + radius) and pass chkActor->WantsOverlapCheckWith().
 *
 * @param	Octree				The octree being searched.
 * @param	Bounds				The bounds of the node.
 * @param	Check				The precalculated info for this check.
 */
void FOctreeNode::RestrictedOverlapCheck(FPrimitiveOctree* Octree,const FOctreeNodeBounds& Bounds, FRadiusOverlapCheck& Check)
{
	// Test the primitives contained within this node
	for (INT Index = 0; Index < Primitives.Num(); Index++)
	{
		UPrimitiveComponent* TestPrimitive = Primitives(Index);
		// Skip if we've already checked this component
		if (TestPrimitive->Tag != UPrimitiveComponent::CurrentTag)
		{
			TestPrimitive->Tag = UPrimitiveComponent::CurrentTag;
			AActor* PrimOwner = TestPrimitive->GetOwner();
			if (PrimOwner && PrimOwner != Octree->ChkActor && Octree->ChkActor->WantsOverlapCheckWith(PrimOwner) )
			{
				// If CollideActors is false, or we have an owner with bCollideActors == false, do not return this Actor.
				if (TestPrimitive->CollideActors && PrimOwner->bCollideActors)
				{
					// If all components have not been requested, only check the actor.
					if (PrimOwner->OverlapTag != UPrimitiveComponent::CurrentTag)
					{
						// Add this primitive if it overlaps
						if (Check.SphereBoundsTest(TestPrimitive->Bounds))
						{
							FCheckResult* NewResult = new(*(Octree->ChkMem))FCheckResult;
							NewResult->Actor		= PrimOwner;
							NewResult->Component	= TestPrimitive;
							NewResult->GetNext()	= Octree->ChkResult;
							Octree->ChkResult		= NewResult;
							PrimOwner->OverlapTag = UPrimitiveComponent::CurrentTag;
						}
					}
				}
			}
		}
	}

	// Now traverse children of this node if present.
	if (Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(Bounds, Octree->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			Children[childIXs[i]].RestrictedOverlapCheck(Octree,FOctreeNodeBounds(Bounds,childIXs[i]),Check);
		}
	}

}

/*-----------------------------------------------------------------------------
	Debug drawing function
-----------------------------------------------------------------------------*/


void FOctreeNode::Draw(FPrimitiveDrawInterface* PDI, FColor DrawColor, UBOOL bAndChildren, const FOctreeNodeBounds& Bounds)
{
	// Draw this node
	DrawWireBox(PDI,Bounds.GetBox(),DrawColor,SDPG_World);

	// And draw children, if desired.
	if(Children && bAndChildren)
	{
		for(INT i=0; i<8; i++)
		{
			this->Children[i].Draw( PDI, DrawColor, bAndChildren, FOctreeNodeBounds(Bounds,i));
		}
	}
}

///////////////////////////////////////////// TREE ////////////////////////////////////////////////

//
// Constructor
//
FPrimitiveOctree::FPrimitiveOctree():
	bShowOctree(0)
{
	INC_MEMORY_STAT_BY(STAT_Octree_Memory, sizeof(FPrimitiveOctree));

	// Create root node (doesn't use block allocate)
	RootNode = new FOctreeNode;
	INC_MEMORY_STAT_BY(STAT_Octree_Memory, sizeof(FOctreeNode));
}

//
// Destructor
//
FPrimitiveOctree::~FPrimitiveOctree()
{
	DEC_MEMORY_STAT_BY(STAT_Octree_Memory, sizeof(FPrimitiveOctree));

	RootNode->RemoveAllPrimitives(this);

	delete RootNode;
	DEC_MEMORY_STAT_BY(STAT_Octree_Memory, sizeof(FOctreeNode));
}

//
//	Tick
//
void FPrimitiveOctree::Tick()
{
	// Draw entire Octree.
	if(bShowOctree)
	{
		RootNode->Draw(GWorld->LineBatcher,FColor(0,255,255),1,RootNodeBounds);
	}
}

//
//	AddPrimitive
//
void FPrimitiveOctree::AddPrimitive(UPrimitiveComponent* Primitive)
{
	INC_DWORD_STAT(STAT_AddCount);
	SCOPE_CYCLE_COUNTER(STAT_AddTime);

	//debugf(TEXT("Add: %x : %s CollideActors: %d"), Primitive, *Primitive->GetName(), Primitive->CollideActors);

	// Just to be sure - if the actor is already in the octree, remove it and re-add it.
	if(Primitive->OctreeNodes.Num() > 0)
	{
		if(!GIsEditor)
		{
			debugf(TEXT("Octree Warning (AddActor): %s (Owner: %s) Already In Octree."), *Primitive->GetName(), Primitive->GetOwner() ? *Primitive->GetOwner()->GetPathName() : TEXT("None"));
		}
		RemovePrimitive(Primitive);
	}

	// Check if this actor is within world limits!
	const FBox&	PrimitiveBox = Primitive->Bounds.GetBox();
	if(	PrimitiveBox.Max.X < -HALF_WORLD_MAX || PrimitiveBox.Min.X > HALF_WORLD_MAX ||
		PrimitiveBox.Max.Y < -HALF_WORLD_MAX || PrimitiveBox.Min.Y > HALF_WORLD_MAX ||
		PrimitiveBox.Max.Z < -HALF_WORLD_MAX || PrimitiveBox.Min.Z > HALF_WORLD_MAX)
	{
		debugf(TEXT("Octree Warning (AddPrimitive): %s (Owner: %s) Outside World."), *Primitive->GetName(), Primitive->GetOwner() ? *Primitive->GetOwner()->GetPathName() : TEXT("None"));

		// Set flag which will cause Owner to get eventOutsideWorldBounds called inside AActor::Tick
 		if( Primitive->GetOwner() != NULL )
 		{
			Primitive->GetOwner()->bComponentOutsideWorld = TRUE;
 		}

		return;
	}

	AActor* PrimOwner = Primitive->GetOwner();

	// Reset list of 'failed' primitives.
	FailedPrims.Empty();

	// Always filter bStatic actors as multi-node as they don't get dynamically spawned,
	// so their being added is due to editactor or "set" or loading
	if( (GWorld->HasBegunPlayAndNotAssociatingLevel() && (PrimOwner == NULL || !PrimOwner->bForceOctreeMNFilter) && ((PrimOwner == NULL || PrimOwner->IsStatic() == FALSE))  || (PrimOwner && PrimOwner->bForceOctreeSNFilter)) )
	{
		Primitive->bWasSNFiltered = 1;
		RootNode->SingleNodeFilter(Primitive, this, RootNodeBounds);
	}
	// If we have not yet started play, filter things into many leaves,
	// this is slower to add, but is more accurate.
	else
	{
		// Set flag to indicate PrimitiveComponent is multi-node filtered
		Primitive->bWasSNFiltered = 0;

		// Add to Octree.
		UBOOL bSuccess = RootNode->MultiNodeFilter(Primitive, this, RootNodeBounds);

		// If we failed to insert (was going to hit too many nodes), we just use single-node method.
		if(!bSuccess)
		{
			// First, remove from any nodes it's currently in.
			RemovePrimitive(Primitive);

			// Then use SN method.
			Primitive->bWasSNFiltered = 1;
			RootNode->SingleNodeFilter(Primitive, this, RootNodeBounds);
		}
	}

	// Iterate over any PrimComps that 'indirectly' failed - thats is failed when they were being re-inserted due to node splitting.
	// For each one, re-insert using single-node filtering.
	for(INT i=0; i<FailedPrims.Num(); i++)
	{
		UPrimitiveComponent* FailedPrim = FailedPrims(i);
		RemovePrimitive(FailedPrim);
		FailedPrim->bWasSNFiltered = 1;
		RootNode->SingleNodeFilter(FailedPrim, this, RootNodeBounds);
	}
}

//
//	RemovePrimitive
//
void FPrimitiveOctree::RemovePrimitive(UPrimitiveComponent* Primitive)
{
	INC_DWORD_STAT(STAT_RemoveCount);
	SCOPE_CYCLE_COUNTER(STAT_RemoveTime);

	//debugf(TEXT("RemovePrimitive: %s (Owner: %s)."), *Primitive->GetName(), Primitive->GetOwner() ? *Primitive->GetOwner()->GetName() : TEXT("None"));

	// Work through this actors list of nodes, removing itself from each one.
	for(INT i=0; i<Primitive->OctreeNodes.Num(); i++)
	{
		FOctreeNode* node = Primitive->OctreeNodes(i);
		check(node);
		node->Primitives.RemoveItemSwap(Primitive);
		DEC_DWORD_STAT_BY(STAT_Octree_Memory,sizeof(UPrimitiveComponent*));
	}
	// Then empty the list of nodes.
	Primitive->OctreeNodes.Empty();
}

static FLOAT ToInfinity(FLOAT f)
{
	if(f > 0)
	{
		return MY_FLTMAX;
	}
	else
	{
		return -MY_FLTMAX;
	}
}

//
//	ActorLineCheck
//
FCheckResult* FPrimitiveOctree::ActorLineCheck(FMemStack& Mem, 
											   const FVector& End, 
											   const FVector& Start, 
											   const FVector& Extent, 
											   DWORD TraceFlags,
											   AActor *SourceActor,
											   ULightComponent* SourceLight)
{
#if STATS
	DWORD Counter = (DWORD)STAT_ZE_LineCheck_Count;
	DWORD Counter2 = (DWORD)STAT_ZE_LineCheck_Time;
	if (Extent.IsZero() == FALSE)
	{
		Counter = (DWORD)STAT_NZE_LineCheck_Count;
		Counter2 = (DWORD)STAT_NZE_LineCheck_Time;
	}
	INC_DWORD_STAT(Counter);
	SCOPE_CYCLE_COUNTER(Counter2);
#endif

	// Fill in temporary data.
	UPrimitiveComponent::CurrentTag++;
	//debugf(TEXT("LINE CHECK: CT: %d"), OctreeTag);

	ChkResult = NULL;

	ChkMem = &Mem;
	ChkEnd = End;
	ChkStart = Start;
	ChkExtent = Extent;
	ChkTraceFlags = TraceFlags;
	ChkActor = SourceActor;
	ChkLight = SourceLight;

	check( ChkLight || !(TraceFlags & TRACE_ShadowCast) );

	ChkDir = (End-Start);
	ChkOneOverDir = FVector(1.0f/ChkDir.X, 1.0f/ChkDir.Y, 1.0f/ChkDir.Z);

	ChkFirstResult = NULL;

	bChkExtentIsZero = Extent.IsZero();
	// This will recurse down, adding results to ChkResult as it finds them.
	// Taken from the Revelles/Urena/Lastra paper: http://wscg.zcu.cz/wscg2000/Papers_2000/X31.pdf
	if (bChkExtentIsZero)
	{		
		FVector RayDir = ChkDir;
		RayOrigin = ChkStart;
		NodeTransform = 0;

		if(RayDir.X < 0.0f)
		{
			RayOrigin.X = -RayOrigin.X;
			RayDir.X = -RayDir.X;
			NodeTransform |= 4;
		}

		if(RayDir.Y < 0.0f)
		{
			RayOrigin.Y = -RayOrigin.Y;
			RayDir.Y = -RayDir.Y;
			NodeTransform |= 2;
		}

		if(RayDir.Z < 0.0f)
		{
			RayOrigin.Z = -RayOrigin.Z;
			RayDir.Z = -RayDir.Z;
			NodeTransform |= 1;
		}

		// T's should be between 0 and 1 for a hit on the tested ray.
		FVector T0, T1;

#if USE_SIMD_ZELINE
		ParallelMask[0] = 0;
		ParallelMask[1] = 0;
		ParallelMask[2] = 0;
#endif
		ParallelAxis = 0;

		// Check for parallel cases.
		// X //
		if(RayDir.X > 0.0f)
		{
			T0.X = (RootNodeBounds.Center.X - RootNodeBounds.Extent - RayOrigin.X)/RayDir.X;
			T1.X = (RootNodeBounds.Center.X + RootNodeBounds.Extent - RayOrigin.X)/RayDir.X;
		}
		else
		{
			T0.X = ToInfinity(RootNodeBounds.Center.X - RootNodeBounds.Extent - RayOrigin.X);
			T1.X = ToInfinity(RootNodeBounds.Center.X + RootNodeBounds.Extent - RayOrigin.X);
#if USE_SIMD_ZELINE
			ParallelMask[0] = (DWORD)-1;
#endif
			ParallelAxis |= 4;
		}

		// Y //
		if(RayDir.Y > 0.0f)
		{
			T0.Y = (RootNodeBounds.Center.Y - RootNodeBounds.Extent - RayOrigin.Y)/RayDir.Y;
			T1.Y = (RootNodeBounds.Center.Y + RootNodeBounds.Extent - RayOrigin.Y)/RayDir.Y;
		}
		else
		{
			T0.Y = ToInfinity(RootNodeBounds.Center.Y - RootNodeBounds.Extent - RayOrigin.Y);
			T1.Y = ToInfinity(RootNodeBounds.Center.Y + RootNodeBounds.Extent - RayOrigin.Y);
#if USE_SIMD_ZELINE
			ParallelMask[1] = (DWORD)-1;
#endif
			ParallelAxis |= 2;
		}

		// Z //
		if(RayDir.Z > 0.0f)
		{
			T0.Z = (RootNodeBounds.Center.Z - RootNodeBounds.Extent - RayOrigin.Z)/RayDir.Z;
			T1.Z = (RootNodeBounds.Center.Z + RootNodeBounds.Extent - RayOrigin.Z)/RayDir.Z;
		}
		else
		{
			T0.Z = ToInfinity(RootNodeBounds.Center.Z - RootNodeBounds.Extent - RayOrigin.Z);
			T1.Z = ToInfinity(RootNodeBounds.Center.Z + RootNodeBounds.Extent - RayOrigin.Z);
#if USE_SIMD_ZELINE
			ParallelMask[2] = (DWORD)-1;
#endif
			ParallelAxis |= 1;
		}

		// Only traverse if ray hits RootNode box.
		if(T0.GetMax() < T1.GetMax())
		{
#if USE_SIMD_ZELINE
#if TEST_SIMD_ZELINE
			RootNode->ActorZeroExtentLineCheck(this, T0.X, T0.Y, T0.Z, T1.X, T1.Y, T1.Z, RootNodeBounds);
			INT NumResults = 0;
			FCheckResult*	Scan = ChkResult;
			while (Scan)
			{
				NumResults++;
				Scan = Scan->GetNext();
			}
			Scan = ChkFirstResult; 
			ChkResult = NULL;
			ChkFirstResult = NULL;
			UPrimitiveComponent::CurrentTag++;
#endif
			FZETraversalNumbers Traversal(T0.X, T0.Y, T0.Z, T1.X, T1.Y, T1.Z,1.0f);
			RootNode->ActorZeroExtentLineCheck(this,Traversal,RootNodeBounds);
#if TEST_SIMD_ZELINE
			if (!((!Scan && !ChkFirstResult) || (Scan && ChkFirstResult && Scan->Time == ChkFirstResult->Time)))
			{
				ChkResult = NULL;
				ChkFirstResult = NULL;
				Traversal.T0[3] = 1.0f;
				UPrimitiveComponent::CurrentTag++;
				RootNode->ActorZeroExtentLineCheck(this,Traversal,RootNodeBounds);
				check((!Scan && !ChkFirstResult) || (Scan && ChkFirstResult && Scan->Time == ChkFirstResult->Time));
			}
			INT NumResults2 = 0;
			Scan = ChkResult;
			while (Scan)
			{
				NumResults2++;
				Scan = Scan->GetNext();
			}
			check(NumResults2 == NumResults);
#endif
#else
			RootNode->ActorZeroExtentLineCheck(this, T0.X, T0.Y, T0.Z, T1.X, T1.Y, T1.Z, RootNodeBounds);
#endif
		}

		// Only return one (first) result if TRACE_SingleResult set.
		if(TraceFlags & TRACE_SingleResult)
		{
			ChkResult = ChkFirstResult;
			if(ChkResult)
				ChkResult->GetNext() = NULL;
		}
	}
	else
	{
		// Create box around fat ray check.
		ChkBox = FBox(0);
		ChkBox += Start;
		ChkBox += End;
		ChkBox.Min -= Extent;
		ChkBox.Max += Extent;

		// Then recurse through Octree
		RootNode->ActorNonZeroExtentLineCheck(this, RootNodeBounds);
	}
	// If TRACE_SingleResult, only return 1 result (the first hit).
	// This code has to ignore fake-backdrop hits during shadow casting though (can't do that in ShouldTrace)
	if(ChkResult && TraceFlags & TRACE_SingleResult)
	{
		return FindFirstResult(ChkResult, TraceFlags);
	}
	return ChkResult;
}

//
//	ActorPointCheck
//
FCheckResult* FPrimitiveOctree::ActorPointCheck(FMemStack& Mem, 
												const FVector& Location, 
												const FVector& Extent, 
												DWORD TraceFlags)
{
	INC_DWORD_STAT(STAT_PointCheckCount);
	SCOPE_CYCLE_COUNTER(STAT_PointCheckTime);

	// Fill in temporary data.
	UPrimitiveComponent::CurrentTag++;
	ChkResult		= NULL;
	ChkMem			= &Mem;
	ChkStart		= Location;
	ChkExtent		= Extent;
	bChkExtentIsZero = ChkExtent.IsZero();
	ChkTraceFlags	= TraceFlags;
	ChkBox			= FBox(ChkStart - ChkExtent, ChkStart + ChkExtent);

	RootNode->ActorPointCheck(this, RootNodeBounds);

	return ChkResult;
}

//
//	ActorRadiusCheck
//
FCheckResult* FPrimitiveOctree::ActorRadiusCheck(FMemStack& Mem, 
												 const FVector& Location, 
												 FLOAT Radius,
												 UBOOL bUseOverlap)
{
	INC_DWORD_STAT(STAT_RadiusCheckCount);
	SCOPE_CYCLE_COUNTER(STAT_RadiusCheckTime);

	// Fill in temporary data.
	UPrimitiveComponent::CurrentTag++;
	ChkResult = NULL;

	ChkMem = &Mem;
	ChkBox = FBox(Location - FVector(Radius, Radius, Radius), Location + FVector(Radius, Radius, Radius));
	//@todo joeg -- remove this if once the overlap is the used everywhere
	if (bUseOverlap == FALSE)
	{
		ChkStart = Location;
		ChkRadiusSqr = Radius * Radius;

		RootNode->ActorRadiusCheck(this, RootNodeBounds);
	}
	else
	{
		// Build the precalculated info
		FRadiusOverlapCheck CheckInfo(Location,Radius);
		// Recursively search the tree
		RootNode->ActorRadiusOverlapCheck(this,RootNodeBounds,CheckInfo);
	}

	return ChkResult;
}

//
//	ActorEncroachmentCheck
//
FCheckResult* FPrimitiveOctree::ActorEncroachmentCheck(FMemStack& Mem, 
													   AActor* Actor, 
													   FVector Location, 
													   FRotator Rotation, 
													   DWORD TraceFlags)
{
	INC_DWORD_STAT(STAT_EncroachCheckCount);
	SCOPE_CYCLE_COUNTER(STAT_EncroachCheckTime);

	// Fill in temporary data.
	ChkResult = NULL;

	UPrimitiveComponent::CurrentTag++;

	ChkMem = &Mem;
	ChkActor = Actor;
	

	// Save actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	if(Actor->CollisionComponent != NULL && Actor->CollisionComponent->IsValidComponent() && !Actor->CollisionComponent->IsAttached())
	{
		debugf( TEXT("ActorEncroachmentCheck: Actor '%s' has uninitialised CollisionComponent!"), *Actor->GetName() );
	}

	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Actor->Components.Num();ComponentIndex++)
	{
		UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));

		if( primComp != NULL && 
			primComp->IsAttached() &&
			(primComp == Actor->CollisionComponent || primComp->AlwaysCheckCollision) &&
			primComp->CollideActors &&
			primComp->IsValidComponent()
			)
		{
			ChkBox = primComp->Bounds.GetBox();
			ChkPrimComp = primComp;		
			
			ChkTraceFlags = TraceFlags;
			if(primComp != Actor->CollisionComponent)
			{
				// give component a chance to override its trace flags when it's not the collision component
				primComp->OverrideTraceFlagsForNonCollisionComponentChecks(ChkTraceFlags);
			}
			
			if(ChkBox.IsValid)
			{
				UPrimitiveComponent::CurrentTag++;
				Actor->OverlapAdjust = Actor->Location - Location;
				ChkBox.Min += Actor->OverlapAdjust;
				ChkBox.Max += Actor->OverlapAdjust;
				RootNode->ActorEncroachmentCheck(this, RootNodeBounds);
				Actor->OverlapAdjust = FVector(0.f,0.f,0.f);
			}
		}
	}

	// Restore actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	return ChkResult;
}

/**
 * Finds all actors that are touched by a sphere (point + radius).
 *
 * @param	Mem			The mem stack to allocate results from.
 * @param	Actor		The actor to ignore overlaps with.
 * @param	Location	The center of the sphere.
 * @param	Radius		The size of the sphere to check for overlaps with.
 */
FCheckResult* FPrimitiveOctree::ActorOverlapCheck(FMemStack& Mem,AActor* Actor,
	const FVector& Location,FLOAT Radius)
{
	SCOPE_CYCLE_COUNTER(STAT_OverlapCheckTime);

	UPrimitiveComponent::CurrentTag++;
	ChkResult	= NULL;

	ChkBox = FBox(Location - FVector(Radius, Radius, Radius), Location + FVector(Radius, Radius, Radius));
	ChkActor	= Actor;
	ChkMem		= &Mem;

	// Build the precalculated info
	FRadiusOverlapCheck CheckInfo(Location,Radius);
	RootNode->ActorOverlapCheck(this, RootNodeBounds, CheckInfo, FALSE);

	return ChkResult;
}

/**
 * Finds all actors that are touched by a sphere (point + radius).
 *
 * @param	Mem			The mem stack to allocate results from.
 * @param	Actor		The actor to ignore overlaps with.
 * @param	Location	The center of the sphere.
 * @param	Radius		The size of the sphere to check for overlaps with.
 * @param	TraceFlags	Options for the trace.
 */
FCheckResult* FPrimitiveOctree::ActorOverlapCheck(FMemStack& Mem,AActor* Actor,
	const FVector& Location,FLOAT Radius,DWORD TraceFlags)
{
	SCOPE_CYCLE_COUNTER(STAT_OverlapCheckTime);

	UPrimitiveComponent::CurrentTag++;
	ChkResult	= NULL;

	ChkBox = FBox(Location - FVector(Radius, Radius, Radius), Location + FVector(Radius, Radius, Radius));
	ChkActor	= Actor;
	ChkMem		= &Mem;

	// Build the precalculated info
	FRadiusOverlapCheck CheckInfo(Location,Radius);
	RootNode->ActorOverlapCheck(this, RootNodeBounds, CheckInfo, TraceFlags&TRACE_AllComponents);

	return ChkResult;
}

/**
 * Finds all pawns that are touched by a sphere (point + radius).
 *
 * @param	Mem			The mem stack to allocate results from.
 * @param	Actor		The actor to ignore overlaps with.
 * @param	Location	The center of the sphere.
 * @param	Radius		The size of the sphere to check for overlaps with.
 */
FCheckResult* FPrimitiveOctree::PawnOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius)
{
	SCOPE_CYCLE_COUNTER(STAT_OverlapCheckTime);

	UPrimitiveComponent::CurrentTag++;
	ChkResult	= NULL;

	ChkBox = FBox(Location - FVector(Radius, Radius, Radius), Location + FVector(Radius, Radius, Radius));
	ChkActor	= Actor;
	ChkMem		= &Mem;

	// Build the precalculated info
	FRadiusOverlapCheck CheckInfo(Location,Radius);
	RootNode->PawnOverlapCheck(this, RootNodeBounds, CheckInfo);

	return ChkResult;
}

/**
 * Finds all actors that are touched by a sphere (point + radius) and pass Actor->WantsOverlapCheckWith().
 *
 * @param	Mem			The mem stack to allocate results from.
 * @param	Actor		The actor to ignore overlaps with.
 * @param	Location	The center of the sphere.
 * @param	Radius		The size of the sphere to check for overlaps with.
 */
FCheckResult* FPrimitiveOctree::RestrictedOverlapCheck(FMemStack& Mem, AActor* Actor, const FVector& Location, FLOAT Radius)
{
	SCOPE_CYCLE_COUNTER(STAT_OverlapCheckTime);

	UPrimitiveComponent::CurrentTag++;
	ChkResult	= NULL;

	ChkBox = FBox(Location - FVector(Radius, Radius, Radius), Location + FVector(Radius, Radius, Radius));
	ChkActor	= Actor;
	ChkMem		= &Mem;

	// Build the precalculated info
	FRadiusOverlapCheck CheckInfo(Location,Radius);
	RootNode->RestrictedOverlapCheck(this, RootNodeBounds, CheckInfo);

	return ChkResult;
}
/**
 * Retrieves all primitives in hash.
 *
 * @param	Primitives [out]	Array primitives are being added to
 */
void FPrimitiveOctree::GetPrimitives(TArray<UPrimitiveComponent*>& Primitives) const
{
	UPrimitiveComponent::CurrentTag++;
	RootNode->GetPrimitives(Primitives);
}

/** Try and remove empty/nearly empty nodes.  */
void FPrimitiveOctree::CollapseTreeChildren()
{
	RootNode->CollapseChildren();
}

//
//	FPrimitiveOctree::GetIntersectingPrimitives - Returns the primitives contained by the given bounding box.
//

void FPrimitiveOctree::GetIntersectingPrimitives(const FBox& Box,TArray<UPrimitiveComponent*>& Primitives)
{
	UPrimitiveComponent::CurrentTag++;
	RootNode->GetIntersectingPrimitives(Box,Primitives,this,RootNodeBounds);
}

static void GetNodeChildren(FOctreeNode *Parent, TArray<FOctreeNode*> &Kids)
{
	if (Parent != NULL &&
		Parent->Children != NULL)
	{
		for (INT ChildIdx = 0; ChildIdx < 8; ChildIdx++)
		{
			GetNodeChildren(&Parent->Children[ChildIdx],Kids);
			Kids.AddItem(&Parent->Children[ChildIdx]);
		}
	}
}

//
//	FPrimitiveOctree::Exec
//

static void AddToHisto(TArray<INT>& Histo, INT Data)
{
	if(Histo.Num() <= Data)
	{
		Histo.AddZeroed((Data - Histo.Num()) + 1);
	}

	Histo(Data) += 1;
}

static void PrintHisto(const TArray<INT>& Histo, FOutputDevice& Ar)
{
	for(INT ItemIdx=0; ItemIdx<Histo.Num(); ItemIdx++)
	{
		if(Histo(ItemIdx) > 0)
		{
			Ar.Logf(TEXT("%d: %d"), ItemIdx, Histo(ItemIdx));
		}
	}
}

UBOOL FPrimitiveOctree::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(ParseCommand(&Cmd,TEXT("SHOWOCTREE")))
	{
		bShowOctree = !bShowOctree;
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("ANALYZEOCTREE")))
	{
		const UBOOL bVerbose = (ParseToken(Cmd, 0) == TEXT("VERBOSE"));

		UPrimitiveComponent::CurrentTag++;

		TArray<FOctreeNode*> Nodes;
		GetNodeChildren(RootNode,Nodes);
		Ar.Logf(TEXT("-------ANALYZEOCTREE------------"));
		INT PrimitiveCnt = 0, CollidingPrimitiveCnt = 0, EmptyNodes = 0;
		TArray<INT> PrimHisto;
		INT NumNodesWithOnePrimitive = 0;
		INT NumNodesWithTwoPrimitives = 0;
		TArray<INT> SlackHisto;
		INT PrimArraySlack = 0;

		for (INT Idx = 0; Idx < Nodes.Num(); Idx++)
		{
			if (Nodes(Idx)->Primitives.Num() == 0)
			{
				EmptyNodes++;
			}
			else
			{
				const INT NumPrimInNode = Nodes(Idx)->Primitives.Num();
				if( NumPrimInNode == 1 )
				{
					NumNodesWithOnePrimitive++;
				}
				else if( NumPrimInNode == 2 )
				{
					NumNodesWithTwoPrimitives++;
				}

				AddToHisto(PrimHisto, NumPrimInNode);

				// Count array slack (in bytes)
				const INT SlackMem = (Nodes(Idx)->Primitives.GetSlack() * sizeof(UPrimitiveComponent*));
				PrimArraySlack += SlackMem;
				AddToHisto(SlackHisto, SlackMem);

				INT NodeCollidingPrimitiveCnt = 0;
				for (INT PrimIdx = 0; PrimIdx < Nodes(Idx)->Primitives.Num(); PrimIdx++)
				{
					UPrimitiveComponent* Prim = Nodes(Idx)->Primitives(PrimIdx);
					const UBOOL bPrimCollides = Prim->ShouldCollide();

					// If this is the first time we have come across this primitive - add it to prim totals
					if(Prim->Tag != UPrimitiveComponent::CurrentTag)
					{
						Prim->Tag = UPrimitiveComponent::CurrentTag; // Use tag to indicate we have already seen it

						PrimitiveCnt++; // Total prim count

						if(bPrimCollides)
						{
							CollidingPrimitiveCnt++; // colliding prim count
						}
					}

					if(bPrimCollides)
					{
						NodeCollidingPrimitiveCnt++;
					}

					if(bVerbose)
					{
						Ar.Logf(TEXT("Node %4d: Primitive: %s"),Idx,*(Nodes(Idx)->Primitives(PrimIdx)->GetFullName()));
					}
				}

				if(bVerbose)
				{
					const FLOAT NodeCollidingPrimitiveRatio = 1.f - ((Nodes(Idx)->Primitives.Num()-NodeCollidingPrimitiveCnt)/(FLOAT)Nodes(Idx)->Primitives.Num());
					Ar.Logf(TEXT("Node %4d: %2d Primitives, %2d Colliding Primitives [%2.1f%%]"),Idx,Nodes(Idx)->Primitives.Num(),NodeCollidingPrimitiveCnt,NodeCollidingPrimitiveRatio*100.f);
				}
			}
		}

#if STATS
		// read octree mem stat
		const DWORD OctreeMemStat = GStatManager.GetStatValueDWORD(STAT_Octree_Memory);
#endif
		Ar.Logf(TEXT("-------------------"));
		Ar.Logf(TEXT("%d Total Nodes, %d Empty Nodes, %d Nodes With One Primitive, %d Nodes With Two Primitives"),Nodes.Num(),EmptyNodes, NumNodesWithOnePrimitive, NumNodesWithTwoPrimitives );
		Ar.Logf(TEXT("%d Total Primitives, %d Total Colliding Primitives"),PrimitiveCnt,CollidingPrimitiveCnt);
#if STATS
		Ar.Logf(TEXT("%d Octree Mem Stat"), OctreeMemStat);
#endif
		Ar.Logf(TEXT("%d Primitive Array Slack (bytes)"), PrimArraySlack);
		Ar.Logf(TEXT("-------------------"));
		Ar.Logf(TEXT("PRIMITIVES"));
		PrintHisto(PrimHisto,Ar);
		Ar.Logf(TEXT("-------------------"));
		Ar.Logf(TEXT("SLACK"));
		PrintHisto(SlackHisto,Ar);
		Ar.Logf(TEXT("-------------------"));

		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("SHRINKOCTREE")))
	{
		DOUBLE Start = appSeconds();
		TArray<FOctreeNode*> Nodes;
		GetNodeChildren(RootNode,Nodes);
		for (INT Idx = 0; Idx < Nodes.Num(); Idx++)
		{
			Nodes(Idx)->Primitives.Shrink();
		}
		debugf(TEXT("Shrink Octree Slack: Took %f ms"), 1000.f * (appSeconds() - Start));
		return TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("COLLAPSEOCTREE")))
	{
		DOUBLE Start = appSeconds();
		CollapseTreeChildren();
		debugf(TEXT("CollapseTreeChildren: Took %f ms"), 1000.f * (appSeconds() - Start));
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
