/*=============================================================================
	UnCollision.h: Common collision code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _UN_COLLISION_H
#define _UN_COLLISION_H

/**
 * Collision stats
 */
enum ECollisionStats
{
	STAT_SingleLineCheck = STAT_CollisionFirstStat,
	STAT_MultiLineCheck,
	STAT_Col_Level,
	STAT_Col_Actors,
	STAT_Col_Sort,

	/** Terrain checks */
	STAT_TerrainExtentTime,
	STAT_TerrainZeroExtentTime,
	STAT_TerrainPointTime,
	/** Static mesh checks */
	STAT_StaticMeshZeroExtentTime,
	STAT_StaticMeshExtentTime,
	STAT_StaticMeshPointTime,
	/** BSP checks */
	STAT_BSPZeroExtentTime,
	STAT_BSPExtentTime,
	STAT_BSPPointTime
};


/** 
 *  Enable line check tracing, captures call stacks relating to the use of line checks
 *  in script and in native code
 */
#if !SHIPPING_PC_GAME && _MSC_VER && !CONSOLE
#define LINE_CHECK_TRACING 1
#else
#define LINE_CHECK_TRACING 0
#endif

#if LINE_CHECK_TRACING

#include "StackTracker.h"

class LineCheckTracker
{
public:

	/** A struct to store information about individual callstacks during line checks*/
	struct FLineCheckData
	{
		/** Store the flags used by the line check at the time of the call */
		INT Flags;
		/** Was this a non zero extent line check */
		UBOOL IsNonZeroExtent;
		/** Store the name of objects used by the line check for this stack trace */
		typedef struct _LineCheckObj
		{
			FName	ObjectName;
			INT		Count;
			FString	DetailedInfo;	
			_LineCheckObj() {ObjectName=NAME_None; }
			_LineCheckObj(FName ObjName, INT Cnt, const FString & DetInfo) :ObjectName(ObjName), Count(Cnt), DetailedInfo(DetInfo) {}

		} LineCheckObj;

		TMap<const FName, LineCheckObj> LineCheckObjsMap;
	};

	/** Called at the beginning of each frame to check/reset spike count */
	static void Tick();

	/** Dump out the results of all line checks called in the game since the last call to ResetLineChecks() */
	static void DumpLineChecks(INT Threshold = 100);

	/** Reset the line check stack tracker (calls appFree() on all user data pointers)*/
	static void ResetLineChecks();

	/** Turn line check stack traces on and off, does not reset the actual data */
	static void ToggleLineChecks();

	/** Captures a single stack trace for a line check */
	static void CaptureLineCheck(INT LineCheckFlags, const FVector* Extent, const FFrame* ScriptStackFrame, const UObject * Object=NULL);

	/** Set the value which, if exceeded, will cause a dump of the line checks this frame */
	static void SetSpikeMinTraceCount(INT MinTraceCount);

private:

	/** Is tracking enabled */
	static INT bIsTrackingEnabled;
	/** If this count is nonzero, dump the log when we exceed this number in any given frame */
	static INT TraceCountForSpikeDump;
	/** Number of traces recorded this frame */
	static INT CurrentCountForSpike;

    /** Pointers to classes that do all the work */
	static FStackTracker* LineCheckStackTracker;
	static FScriptStackTracker* LineCheckScriptStackTracker;
};

#define LINE_CHECK_TICK() do { LineCheckTracker::Tick(); } while(0)
#define LINE_CHECK_DUMP() do { LineCheckTracker::DumpLineChecks(); } while(0)
#define LINE_CHECK_RESET() do { LineCheckTracker::ResetLineChecks(); } while(0)
#define LINE_CHECK_TOGGLE() do { LineCheckTracker::ToggleLineChecks(); } while(0)
#define LINE_CHECK_TOGGLESPIKES(MinTraceCountForSpike) do { LineCheckTracker::ToggleLineChecks(); LineCheckTracker::SetSpikeMinTraceCount(MinTraceCountForSpike); } while(0)
#define LINE_CHECK_TRACE(flags, extent, object) do { LineCheckTracker::CaptureLineCheck(flags, extent, NULL, object); } while(0)
#define LINE_CHECK_TRACE_SCRIPT(flags, stack) do { LineCheckTracker::CaptureLineCheck(flags, NULL, stack); } while(0)
#else
#define LINE_CHECK_TICK()
#define LINE_CHECK_DUMP()
#define LINE_CHECK_RESET()
#define LINE_CHECK_TOGGLE()
#define LINE_CHECK_TOGGLESPIKES(...)
#define LINE_CHECK_TRACE(...)
#define LINE_CHECK_TRACE_SCRIPT(...)
#endif //LINE_CHECK_TRACING


//
//	Separating axis code.
//

static void CheckMinIntersectTime(FLOAT& MinIntersectTime,FVector& HitNormal,FLOAT Time,const FVector& Normal)
{
	if(Time > MinIntersectTime)
	{
		MinIntersectTime = Time;
		HitNormal = Normal;
	}
}

UBOOL FindSeparatingAxis(
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	const FVector& Start,
	const FVector& End,
	const FVector& BoxExtent,
	const FVector& BoxX,
	const FVector& BoxY,
	const FVector& BoxZ,
	FLOAT& HitTime,
	FVector& OutHitNormal
	);

UBOOL FindSeparatingAxis(
	 const FVector& V0,
	 const FVector& V1,
	 const FVector& V2,
	 const FVector& Start,
	 const FVector& End,
	 const FVector& BoxExtent,
	 FLOAT& HitTime,
	 FVector& OutHitNormal
	 );

//
//	FSeparatingAxisPointCheck - Checks for intersection between an oriented bounding box and a triangle.
//	HitNormal: The normal of the separating axis the bounding box is penetrating the least.
//	BestDist: The amount the bounding box is penetrating the axis defined by HitNormal.
//

struct FSeparatingAxisPointCheck
{
	FVector	HitNormal;
	FLOAT	BestDist;
	UBOOL	Hit;

	const FVector&	V0,
					V1,
					V2;

	UBOOL TestSeparatingAxis(
		const FVector& Axis,
		FLOAT ProjectedPoint,
		FLOAT ProjectedExtent
		)
	{
		FLOAT	ProjectedV0 = Axis | V0,
				ProjectedV1 = Axis | V1,
				ProjectedV2 = Axis | V2,
				TriangleMin = Min(ProjectedV0,Min(ProjectedV1,ProjectedV2)) - ProjectedExtent,
				TriangleMax = Max(ProjectedV0,Max(ProjectedV1,ProjectedV2)) + ProjectedExtent;

		if(ProjectedPoint >= TriangleMin && ProjectedPoint <= TriangleMax)
		{
			// Use inverse sqrt because that is fast and we do more math with the inverse value anyway
			FLOAT	InvAxisMagnitude = appInvSqrt(Axis.X * Axis.X +	Axis.Y * Axis.Y + Axis.Z * Axis.Z),
					ScaledBestDist = BestDist / InvAxisMagnitude,
					MinPenetrationDist = ProjectedPoint - TriangleMin,
					MaxPenetrationDist = TriangleMax - ProjectedPoint;
			if(MinPenetrationDist < ScaledBestDist)
			{
				BestDist = MinPenetrationDist * InvAxisMagnitude;
				HitNormal = -Axis * InvAxisMagnitude;
			}
			if(MaxPenetrationDist < ScaledBestDist)
			{
				BestDist = MaxPenetrationDist * InvAxisMagnitude;
				HitNormal = Axis * InvAxisMagnitude;
			}
			return 1;
		}
		else
			return 0;
	}

	UBOOL TestSeparatingAxis(
		const FVector& Axis,
		const FVector& Point,
		FLOAT ProjectedExtent
		)
	{
		return TestSeparatingAxis(Axis,Axis | Point,ProjectedExtent);
	}

	UBOOL TestSeparatingAxis(
		const FVector& Axis,
		const FVector& Point,
		const FVector& BoxX,
		const FVector& BoxY,
		const FVector& BoxZ,
		const FVector& BoxExtent
		)
	{
		FLOAT	ProjectedExtent = BoxExtent.X * Abs(Axis | BoxX) + BoxExtent.Y * Abs(Axis | BoxY) + BoxExtent.Z * Abs(Axis | BoxZ);
		return TestSeparatingAxis(Axis,Axis | Point,ProjectedExtent);
	}

	UBOOL FindSeparatingAxis(
		const FVector& Point,
		const FVector& BoxExtent,
		const FVector& BoxX,
		const FVector& BoxY,
		const FVector& BoxZ
		)
	{
		// Box faces. We need to calculate this by crossing edges because BoxX etc are the _edge_ directions - not the faces.
		// The box may be sheared due to non-unfiform scaling and rotation so FaceX normal != BoxX edge direction

		if(!TestSeparatingAxis(BoxX ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(BoxY ^ BoxZ,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(BoxZ ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		// Triangle normal.

		if(!TestSeparatingAxis((V2 - V1) ^ (V1 - V0),Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		// Box X edge x triangle edges.

		if(!TestSeparatingAxis((V1 - V0) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V2 - V1) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V0 - V2) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		// Box Y edge x triangle edges.

		if(!TestSeparatingAxis((V1 - V0) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V2 - V1) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;
		
		if(!TestSeparatingAxis((V0 - V2) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;
		
		// Box Z edge x triangle edges.

		if(!TestSeparatingAxis((V1 - V0) ^ BoxZ,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V2 - V1) ^ BoxZ,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;
		
		if(!TestSeparatingAxis((V0 - V2) ^ BoxZ,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		return 1;
	}

	FSeparatingAxisPointCheck(
		const FVector& InV0,
		const FVector& InV1,
		const FVector& InV2,
		const FVector& Point,
		const FVector& BoxExtent,
		const FVector& BoxX,
		const FVector& BoxY,
		const FVector& BoxZ,
		FLOAT InBestDist
		):
		HitNormal(0,0,0),
		BestDist(InBestDist),
		Hit(0),
		V0(InV0),
		V1(InV1),
		V2(InV2)
	{
		Hit = FindSeparatingAxis(Point,BoxExtent,BoxX,BoxY,BoxZ);
	}

		
	UBOOL TestSeparatingAxis(
		const FVector& Axis,
		const FVector& Point,
		const FVector& BoxExtent
		)
	{
		FLOAT	ProjectedExtent = BoxExtent.X * Abs(Axis.X) + BoxExtent.Y * Abs(Axis.Y) + BoxExtent.Z * Abs(Axis.Z);
		return TestSeparatingAxis(Axis,Axis | Point,ProjectedExtent);
	}

	UBOOL FindSeparatingAxis(
		const FVector& Point,
		const FVector& BoxExtent
		)
	{
		// Triangle normal.

		if(!TestSeparatingAxis((V2 - V1) ^ (V1 - V0),Point,BoxExtent))
			return 0;

		const FVector& EdgeDir0 = V1 - V0;
		const FVector& EdgeDir1 = V2 - V1;
		const FVector& EdgeDir2 = V0 - V2;

		// Box Z edge x triangle edges.

		if(!TestSeparatingAxis(FVector(EdgeDir0.Y,-EdgeDir0.X,0.0f),Point,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(FVector(EdgeDir1.Y,-EdgeDir1.X,0.0f),Point,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(FVector(EdgeDir2.Y,-EdgeDir2.X,0.0f),Point,BoxExtent))
			return 0;

		// Box Y edge x triangle edges.

		if(!TestSeparatingAxis(FVector(-EdgeDir0.Z,0.0f,EdgeDir0.X),Point,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(FVector(-EdgeDir1.Z,0.0f,EdgeDir1.X),Point,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(FVector(-EdgeDir2.Z,0.0f,EdgeDir2.X),Point,BoxExtent))
			return 0;

		// Box X edge x triangle edges.

		if(!TestSeparatingAxis(FVector(0.0f,EdgeDir0.Z,-EdgeDir0.Y),Point,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(FVector(0.0f,EdgeDir1.Z,-EdgeDir1.Y),Point,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(FVector(0.0f,EdgeDir2.Z,-EdgeDir2.Y),Point,BoxExtent))
			return 0;

		// Box faces. We need to calculate this by crossing edges because BoxX etc are the _edge_ directions - not the faces.
		// The box may be sheared due to non-unfiform scaling and rotation so FaceX normal != BoxX edge direction

		if(!TestSeparatingAxis(FVector(0.0f,0.0f,1.0f),Point,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(FVector(1.0f,0.0f,0.0f),Point,BoxExtent))
			return 0;

		if(!TestSeparatingAxis(FVector(0.0f,1.0f,0.0f),Point,BoxExtent))
			return 0;

		return 1;
	}

		
		
		
	FSeparatingAxisPointCheck(
		const FVector& InV0,
		const FVector& InV1,
		const FVector& InV2,
		const FVector& Point,
		const FVector& BoxExtent,
		FLOAT InBestDist
		):
	HitNormal(0,0,0),
		BestDist(InBestDist),
		Hit(0),
		V0(InV0),
		V1(InV1),
		V2(InV2)
	{
		Hit = FindSeparatingAxis(Point,BoxExtent);
	}
};

/**
 *	Line Check With Triangle
 *	Algorithm based on "Fast, Minimum Storage Ray/Triangle Intersection"
 *	Returns TRUE if the line segment does hit the triangle
 */
FORCEINLINE UBOOL LineCheckWithTriangle(FCheckResult& Result,const FVector& V1,const FVector& V2,const FVector& V3,const FVector& Start,const FVector& End,const FVector& Direction)
{
	FVector	Edge1 = V3 - V1,
		Edge2 = V2 - V1,
		P = Direction ^ Edge2;
	FLOAT	Determinant = Edge1 | P;

	if(Determinant < DELTA)
	{
		return FALSE;
	}

	FVector	T = Start - V1;
	FLOAT	U = T | P;

	if(U < 0.0f || U > Determinant)
	{
		return FALSE;
	}

	FVector	Q = T ^ Edge1;
	FLOAT	V = Direction | Q;

	if(V < 0.0f || U + V > Determinant)
	{
		return FALSE;
	}

	FLOAT	Time = (Edge2 | Q) / Determinant;

	if(Time < 0.0f || Time > Result.Time)
	{
		return FALSE;
	}

	Result.Normal = ((V3-V2)^(V2-V1)).SafeNormal();
	Result.Time = ((V1 - Start)|Result.Normal) / (Result.Normal|Direction);

	return TRUE;
}


#if XBOX
	#include "UnCollisionXe.h"
#elif PS3 && 0 //@todo joeg -- do ps3 version
	#include "UnCollisionPS3.h"
#else
/**
 * Performs a sphere vs box intersection test using Arvo's algorithm:
 *
 *	for each i in (x, y, z)
 *		if (SphereCenter(i) < BoxMin(i)) d2 += (SphereCenter(i) - BoxMin(i)) ^ 2
 *		else if (SphereCenter(i) > BoxMax(i)) d2 += (SphereCenter(i) - BoxMax(i)) ^ 2
 *
 * @param Sphere the center of the sphere being tested against the AABB
 * @param RadiusSquared the size of the sphere being tested
 * @param AABB the box being tested against
 *
 * @return Whether the sphere/box intersect or not.
 */
FORCEINLINE UBOOL SphereAABBIntersectionTest(const FVector& SphereCenter,const FLOAT RadiusSquared,const FBox& AABB)
{
	// Accumulates the distance as we iterate axis
	FLOAT DistSquared = 0.f;
	// Check each axis for min/max and add the distance accordingly
	// NOTE: Loop manually unrolled for > 2x speed up
	if (SphereCenter.X < AABB.Min.X)
	{
		DistSquared += Square(SphereCenter.X - AABB.Min.X);
	}
	else if (SphereCenter.X > AABB.Max.X)
	{
		DistSquared += Square(SphereCenter.X - AABB.Max.X);
	}
	if (SphereCenter.Y < AABB.Min.Y)
	{
		DistSquared += Square(SphereCenter.Y - AABB.Min.Y);
	}
	else if (SphereCenter.Y > AABB.Max.Y)
	{
		DistSquared += Square(SphereCenter.Y - AABB.Max.Y);
	}
	if (SphereCenter.Z < AABB.Min.Z)
	{
		DistSquared += Square(SphereCenter.Z - AABB.Min.Z);
	}
	else if (SphereCenter.Z > AABB.Max.Z)
	{
		DistSquared += Square(SphereCenter.Z - AABB.Max.Z);
	}
	// If the distance is less than or equal to the radius, they intersect
	return DistSquared <= RadiusSquared;
}

/**
 * Converts a sphere into a point plus radius squared for the test above
 */
FORCEINLINE UBOOL SphereAABBIntersectionTest(const FSphere& Sphere,const FBox& AABB)
{
	FLOAT RadiusSquared = Square(Sphere.W);
	// If the distance is less than or equal to the radius, they intersect
	return SphereAABBIntersectionTest(Sphere.Center,RadiusSquared,AABB);
}
#endif

#endif
