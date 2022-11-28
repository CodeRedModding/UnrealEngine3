/*=============================================================================
	UnCollision.h: Common collision code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

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

static UBOOL TestSeparatingAxis(
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	const FVector& Line,
	FLOAT ProjectedStart,
	FLOAT ProjectedEnd,
	FLOAT ProjectedExtent,
	FLOAT& MinIntersectTime,
	FLOAT& MaxIntersectTime,
	FVector& HitNormal
	)
{
	FLOAT	ProjectedDirection = ProjectedEnd - ProjectedStart,
			ProjectedV0 = Line | V0,
			ProjectedV1 = Line | V1,
			ProjectedV2 = Line | V2,
			TriangleMin = Min(ProjectedV0,Min(ProjectedV1,ProjectedV2)) - ProjectedExtent,
			TriangleMax = Max(ProjectedV0,Max(ProjectedV1,ProjectedV2)) + ProjectedExtent;

	if(ProjectedStart < TriangleMin)
	{
		if(ProjectedDirection < DELTA)
			return 0;
		CheckMinIntersectTime(MinIntersectTime,HitNormal,(TriangleMin - ProjectedStart) / ProjectedDirection,-Line);
		MaxIntersectTime = Min(MaxIntersectTime,(TriangleMax - ProjectedStart) / ProjectedDirection);
	}
	else if(ProjectedStart > TriangleMax)
	{
		if(ProjectedDirection > -DELTA)
			return 0;
		CheckMinIntersectTime(MinIntersectTime,HitNormal,(TriangleMax - ProjectedStart) / ProjectedDirection,Line);
		MaxIntersectTime = Min(MaxIntersectTime,(TriangleMin - ProjectedStart) / ProjectedDirection);
	}
	else
	{
		if(ProjectedDirection > DELTA)
			MaxIntersectTime = Min(MaxIntersectTime,(TriangleMax - ProjectedStart) / ProjectedDirection);
		else if(ProjectedDirection < -DELTA)
			MaxIntersectTime = Min(MaxIntersectTime,(TriangleMin - ProjectedStart) / ProjectedDirection);
	}

	return MaxIntersectTime >= MinIntersectTime;
}

static UBOOL TestSeparatingAxis(
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	const FVector& Line,
	const FVector& Start,
	const FVector& End,
	FLOAT ProjectedExtent,
	FLOAT& MinIntersectTime,
	FLOAT& MaxIntersectTime,
	FVector& HitNormal
	)
{
	return TestSeparatingAxis(V0,V1,V2,Line,Line | Start,Line | End,ProjectedExtent,MinIntersectTime,MaxIntersectTime,HitNormal);
}

static UBOOL TestSeparatingAxis(
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
	FVector& HitNormal
	)
{
	FVector	Line = BoxEdge ^ TriangleEdge;

	if(Line.SizeSquared() < DELTA)
		return 1;

	FLOAT	ProjectedExtent = BoxExtent.X * Abs(Line | BoxX) + BoxExtent.Y * Abs(Line | BoxY) + BoxExtent.Z * Abs(Line | BoxZ);
	return TestSeparatingAxis(V0,V1,V2,Line,Line | Start,Line | End,ProjectedExtent,MinIntersectTime,MaxIntersectTime,HitNormal);
}

static UBOOL FindSeparatingAxis(
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
	FVector& HitNormal
	)
{
	FLOAT	MinIntersectTime = -1.0f,
			MaxIntersectTime = HitTime;

	// Box faces.

	if(!TestSeparatingAxis(V0,V1,V2,BoxX,Start,End,BoxExtent.X * BoxX.SizeSquared(),MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,BoxY,Start,End,BoxExtent.Y * BoxY.SizeSquared(),MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,BoxZ,Start,End,BoxExtent.Z * BoxZ.SizeSquared(),MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	// Triangle normal.

	if(!TestSeparatingAxis(V0,V1,V2,(V2 - V1),(V1 - V0),Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	// Box faces +/- X x triangle edges.

	if(!TestSeparatingAxis(V0,V1,V2,(V1 - V0),BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,(V2 - V1),BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,(V0 - V2),BoxX,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	// Box faces +/- Y x triangle edges.

	if(!TestSeparatingAxis(V0,V1,V2,(V1 - V0),BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,(V2 - V1),BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;
	
	if(!TestSeparatingAxis(V0,V1,V2,(V0 - V2),BoxY,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;
	
	// Box faces +/- Z x triangle edges.

	if(!TestSeparatingAxis(V0,V1,V2,(V1 - V0),BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(!TestSeparatingAxis(V0,V1,V2,(V2 - V1),BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;
	
	if(!TestSeparatingAxis(V0,V1,V2,(V0 - V2),BoxZ,Start,End,BoxX,BoxY,BoxZ,BoxExtent,MinIntersectTime,MaxIntersectTime,HitNormal))
		return 0;

	if(MinIntersectTime >= 0.0f)
	{
		HitTime = MinIntersectTime;
		return 1;
	}
	else
		return 0;
}

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
			FLOAT	AxisMagnitude = Axis.Size(),
					MinPenetrationDist = ProjectedPoint - TriangleMin,
					MaxPenetrationDist = TriangleMax - ProjectedPoint;
			if(MinPenetrationDist < BestDist * AxisMagnitude)
			{
				check(AxisMagnitude > 0.0f);
				FLOAT	InvAxisMagnitude = 1.0f / AxisMagnitude;
				BestDist = MinPenetrationDist * InvAxisMagnitude;
				HitNormal = -Axis * InvAxisMagnitude;
			}
			if(MaxPenetrationDist < BestDist * AxisMagnitude)
			{
				check(AxisMagnitude > 0.0f);
				FLOAT	InvAxisMagnitude = 1.0f / AxisMagnitude;
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
		// Box faces.

		if(!TestSeparatingAxis(BoxX,Point,BoxExtent.X * BoxX.SizeSquared()))
			return 0;

		if(!TestSeparatingAxis(BoxY,Point,BoxExtent.Y * BoxY.SizeSquared()))
			return 0;

		if(!TestSeparatingAxis(BoxZ,Point,BoxExtent.Z * BoxZ.SizeSquared()))
			return 0;

		// Triangle normal.

		if(!TestSeparatingAxis((V2 - V1) ^ (V1 - V0),Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		// Box faces +/- X x triangle edges.

		if(!TestSeparatingAxis((V1 - V0) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V2 - V1) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V0 - V2) ^ BoxX,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		// Box faces +/- Y x triangle edges.

		if(!TestSeparatingAxis((V1 - V0) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;

		if(!TestSeparatingAxis((V2 - V1) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;
		
		if(!TestSeparatingAxis((V0 - V2) ^ BoxY,Point,BoxX,BoxY,BoxZ,BoxExtent))
			return 0;
		
		// Box faces +/- Z x triangle edges.

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
		BestDist(InBestDist),
		HitNormal(0,0,0),
		Hit(0),
		V0(InV0),
		V1(InV1),
		V2(InV2)
	{
		Hit = FindSeparatingAxis(Point,BoxExtent,BoxX,BoxY,BoxZ);
	}
};
