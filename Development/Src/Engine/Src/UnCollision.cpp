/*=============================================================================
	UnActor.cpp: AActor implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnCollision.h"

/** What is considered 'too close' for a check co-planar to a separating plane. */
static const FLOAT TriangleSweptBox_ParallelRegion = 0.0001f;

/** Wraps the state used by an individual triangle versus swept box separating axis check. */
class FTriangleSweptBoxSeparatingAxisCheck
{
public:

	/** Indicates that the line check has become too close and parallel to a separating plane. */
	UBOOL bCloseAndParallel;

	/** If bCloseAndParallel is TRUE, this is the normal of the close feature. */
	FVector CloseFeatureNormal;

	/** Default constructor. */
	FTriangleSweptBoxSeparatingAxisCheck()
	:	bCloseAndParallel(FALSE)
	,	CloseFeatureNormal(0,0,0)
	{
	}

	/** Separating axis code. */
	FORCEINLINE UBOOL TestSeparatingAxis(
									const FVector& V0,
									const FVector& V1,
									const FVector& V2,
									const FVector& Line,
									FLOAT ProjectedStart,
									FLOAT ProjectedEnd,
									FLOAT ProjectedExtent,
									FLOAT& MinIntersectTime,
									FLOAT& MaxIntersectTime,
									FVector& HitNormal,
									FVector& ExitDir
									)
	{
		const FLOAT	ProjectedDirection = ProjectedEnd - ProjectedStart,
					ProjectedV0 = Line | V0,
					ProjectedV1 = Line | V1,
					ProjectedV2 = Line | V2,
					TriangleMin = Min(ProjectedV0,Min(ProjectedV1,ProjectedV2)) - ProjectedExtent,
					TriangleMax = Max(ProjectedV0,Max(ProjectedV1,ProjectedV2)) + ProjectedExtent;

		// Nearly parallel - dangerous condition - see if we are close (in 'danger region') and bail out.
		// We set time to zero to ensure this is the hit normal that is used at the end of the axis testing, 
		// and modified (see code in FindSeparatingAxis below).
		const FLOAT ProjDirMag = Abs(ProjectedDirection);
		if( ProjDirMag < 0.0001f )
		{
			if ( !bCloseAndParallel )
			{
				if( ProjectedStart < TriangleMin && ProjectedStart > (TriangleMin - TriangleSweptBox_ParallelRegion) )
				{
					CloseFeatureNormal = -Line;
					bCloseAndParallel = TRUE;
				}
					
				if( ProjectedStart > TriangleMax && ProjectedStart < (TriangleMax + TriangleSweptBox_ParallelRegion) )
				{
					CloseFeatureNormal = Line;
					bCloseAndParallel = TRUE;
				}
			}

			// If zero - check vector is perp to test axis, so just check if we start outside. If so, we can't collide.
			if( ProjDirMag < SMALL_NUMBER )
			{
				if( ProjectedStart < TriangleMin ||
					ProjectedStart > TriangleMax )
				{
					return FALSE;
				}
				else
				{
					return TRUE;
				}
			}
		}

		FLOAT OneOverProjectedDirection = 1.f / ProjectedDirection;
		// Moving in a positive direction - enter ConvexMin and exit ConvexMax
		FLOAT EntryTime, ExitTime;
		FVector ImpactNormal;
		if(ProjectedDirection > 0.f)
		{
			EntryTime = (TriangleMin - ProjectedStart) * OneOverProjectedDirection;
			ExitTime = (TriangleMax - ProjectedStart) * OneOverProjectedDirection;
			ImpactNormal = -Line;
		}
		// Moving in a negative direction - enter ConvexMax and exit ConvexMin
		else
		{
			EntryTime = (TriangleMax - ProjectedStart) * OneOverProjectedDirection;
			ExitTime = (TriangleMin - ProjectedStart) * OneOverProjectedDirection;
			ImpactNormal = Line;
		}	

		// See if entry time is further than current furthest entry time. If so, update and remember normal
		if(EntryTime > MinIntersectTime)
		{
			MinIntersectTime = EntryTime;
			HitNormal = ImpactNormal;
		}

		// See if exit time is close than current closest exit time.
		if( ExitTime < MaxIntersectTime )
		{
			MaxIntersectTime = ExitTime;
			ExitDir = -ImpactNormal;
		}

		// If exit is ever before entry - we don't intersect
		if( MaxIntersectTime < MinIntersectTime )
		{
			return FALSE;
		}

		// If exit is ever before start of line check - we don't intersect
		if( MaxIntersectTime < 0.f )
		{
			return FALSE;
		}

		return TRUE;
	}

	FORCEINLINE UBOOL TestSeparatingAxis(
							 const FVector& V0,
							 const FVector& V1,
							 const FVector& V2,
							 const FVector& TriangleEdge,
							 const FVector& BoxEdge,
							 const FVector& Start,
							 const FVector& End,
							 const FVector& BoxX,
							 const FVector& BoxY,
							 const FVector& BoxZ,
							 const FVector& BoxExtent,
							 FLOAT& MinIntersectTime,
							 FLOAT& MaxIntersectTime,
							 FVector& HitNormal,
							 FVector& ExitDir
							 )
	{
		// Separating axis is cross product of box and triangle edges.
		const FVector Line = BoxEdge ^ TriangleEdge;

		// Check separating axis is non-zero. If it is, just don't use this axis.
		if(Line.SizeSquared() < SMALL_NUMBER)
		{
			return TRUE;
		}

		// Calculate extent of box projected onto separating axis.
		const FLOAT ProjectedExtent = BoxExtent.X * Abs(Line | BoxX) + BoxExtent.Y * Abs(Line | BoxY) + BoxExtent.Z * Abs(Line | BoxZ);

		return TestSeparatingAxis(V0,V1,V2,Line,Line | Start,Line | End,ProjectedExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir);
	}

	FORCEINLINE UBOOL TestSeparatingAxis(
		const FVector& V0,
		const FVector& V1,
		const FVector& V2,
		const FVector& TriangleEdge,
		const FVector& BoxEdge,
		const FVector& Start,
		const FVector& End,
		const FVector& BoxExtent,
		FLOAT& MinIntersectTime,
		FLOAT& MaxIntersectTime,
		FVector& HitNormal,
		FVector& ExitDir
		)
	{
		// Separating axis is cross product of box and triangle edges.
		const FVector Line = BoxEdge ^ TriangleEdge;

		// Check separating axis is non-zero. If it is, just don't use this axis.
		if(Line.SizeSquared() < SMALL_NUMBER)
		{
			return TRUE;
		}

		// Calculate extent of box projected onto separating axis.
		const FLOAT ProjectedExtent = BoxExtent.X * Abs(Line.X) + BoxExtent.Y * Abs(Line.Y) + BoxExtent.Z * Abs(Line.Z);

		return TestSeparatingAxis(V0,V1,V2,Line,Line | Start,Line | End,ProjectedExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir);
	}
};

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
						)
{
	FTriangleSweptBoxSeparatingAxisCheck Check;

	FLOAT	MinIntersectTime = -BIG_NUMBER,
			MaxIntersectTime = BIG_NUMBER;

	// Calculate normalized edge directions. We normalize here to minimize precision issues when doing cross products later.
	const FVector& EdgeDir0 = (V1 - V0).SafeNormal();
	const FVector& EdgeDir1 = (V2 - V1).SafeNormal();
	const FVector& EdgeDir2 = (V0 - V2).SafeNormal();

	// Used to set the hit normal locally and apply the best normal only upon a full hit
	FVector HitNormal(0,0,0), ExitDir;

	// Box faces. We need to calculate this by crossing edges because BoxX etc are the _edge_ directions - not the faces.
	// The box may be sheared due to non-uniform scaling and rotation so FaceX normal != BoxX edge direction

	if(!Check.TestSeparatingAxis(V0,V1,V2,BoxX,BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,BoxY,BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,BoxZ,BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// Triangle normal.

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir1,EdgeDir0,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// Box X edge x triangle edges.

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir0,BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir1,BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir2,BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// Box Y edge x triangle edges.

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir0,BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir1,BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir2,BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// Box Z edge x triangle edges.

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir0,BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir1,BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir2,BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// If we are close and parallel, and there is not another axis which provides a good separation.
	if(Check.bCloseAndParallel && !(MinIntersectTime > HitTime && Check.CloseFeatureNormal != HitNormal && Check.CloseFeatureNormal != -HitNormal))
	{
		// If in danger, disallow movement, in case it puts us inside the object.
 		HitTime = 0.f;

		// Tilt the normal slightly back towards the direction of travel. 
		// This will cause the next movement to be away from the surface (hopefully out of the 'danger region')
		const FVector CheckDir = (End - Start).SafeNormal();
		OutHitNormal = (Check.CloseFeatureNormal - (0.05f * CheckDir)).SafeNormal();

		// Say we hit.
		return TRUE;
	}

	// If hit is beyond end of ray, no collision occurs.
	if(MinIntersectTime > HitTime)
	{
		return FALSE;
	}

	// If hit time is positive- we start outside and hit the hull
	if(MinIntersectTime >= 0.0f)
	{
		HitTime = MinIntersectTime;
		OutHitNormal = HitNormal;
	}
	// If hit time is negative- the entry is behind the start - so we started colliding
	else
	{
		FVector LineDir = (End - Start).SafeNormal();

		// If the exit surface in front is closer than the exit surface behind us, 
		// and we are moving towards the exit surface in front
		// allow movement (report no hit) to let us exit this triangle.
		if( (MaxIntersectTime < -MinIntersectTime) && (LineDir | ExitDir) > 0.f)
		{
			HitTime = 1.f;
			return FALSE;
		}
		else
		{
			HitTime = 0.f;

			// Use a vector pointing back along check vector as the hit normal
			OutHitNormal = -LineDir;
		}
	}

	return TRUE;
}

UBOOL FindSeparatingAxis(
						 const FVector& V0,
						 const FVector& V1,
						 const FVector& V2,
						 const FVector& Start,
						 const FVector& End,
						 const FVector& BoxExtent,
						 FLOAT& HitTime,
						 FVector& OutHitNormal
						 )
{
	FTriangleSweptBoxSeparatingAxisCheck Check;

	FLOAT	MinIntersectTime = -BIG_NUMBER,
		MaxIntersectTime = BIG_NUMBER;

	// Calculate normalized edge directions. We normalize here to minimize precision issues when doing cross products later.
	const FVector& EdgeDir0 = (V1 - V0).SafeNormal();
	const FVector& EdgeDir1 = (V2 - V1).SafeNormal();
	const FVector& EdgeDir2 = (V0 - V2).SafeNormal();

	// Used to set the hit normal locally and apply the best normal only upon a full hit
	FVector HitNormal(0,0,0), ExitDir;

	// Triangle normal.

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir1,EdgeDir0,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	static FVector BoxX = FVector(1.0f,0.f,0.f);
	static FVector BoxY = FVector(0.0f,1.0f,0.f);
	static FVector BoxZ = FVector(0.0f,0.f,1.0f);

	// Box Z edge x triangle edges.

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir0,BoxZ,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir1,BoxZ,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir2,BoxZ,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// Box Y edge x triangle edges.

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir0,BoxY,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir1,BoxY,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir2,BoxY,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// Box X edge x triangle edges.

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir0,BoxX,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir1,BoxX,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,EdgeDir2,BoxX,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// Box faces. We need to calculate this by crossing edges because BoxX etc are the _edge_ directions - not the faces.
	// The box may be sheared due to non-uniform scaling and rotation so FaceX normal != BoxX edge direction

	if(!Check.TestSeparatingAxis(V0,V1,V2,BoxX,BoxY,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,BoxY,BoxZ,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	if(!Check.TestSeparatingAxis(V0,V1,V2,BoxZ,BoxX,Start,End,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		return FALSE;

	// If we are close and parallel, and there is not another axis which provides a good separation.
	if(Check.bCloseAndParallel && !(MinIntersectTime > HitTime && Check.CloseFeatureNormal != HitNormal && Check.CloseFeatureNormal != -HitNormal))
	{
		// If in danger, disallow movement, in case it puts us inside the object.
		HitTime = 0.f;

		// Tilt the normal slightly back towards the direction of travel. 
		// This will cause the next movement to be away from the surface (hopefully out of the 'danger region')
		const FVector CheckDir = (End - Start).SafeNormal();
		OutHitNormal = (Check.CloseFeatureNormal - (0.05f * CheckDir)).SafeNormal();

		// Say we hit.
		return TRUE;
	}

	// If hit is beyond end of ray, no collision occurs.
	if(MinIntersectTime > HitTime)
	{
		return FALSE;
	}

	// If hit time is positive- we start outside and hit the hull
	if(MinIntersectTime >= 0.0f)
	{
		HitTime = MinIntersectTime;
		OutHitNormal = HitNormal;
	}
	// If hit time is negative- the entry is behind the start - so we started colliding
	else
	{
		FVector LineDir = (End - Start).SafeNormal();

		// If the exit surface in front is closer than the exit surface behind us, 
		// and we are moving towards the exit surface in front
		// allow movement (report no hit) to let us exit this triangle.
		if( (MaxIntersectTime < -MinIntersectTime) && (LineDir | ExitDir) > 0.f)
		{
			HitTime = 1.f;
			return FALSE;
		}
		else
		{
			HitTime = 0.f;

			// Use a vector pointing back along check vector as the hit normal
			OutHitNormal = -LineDir;
		}
	}

	return TRUE;
}


