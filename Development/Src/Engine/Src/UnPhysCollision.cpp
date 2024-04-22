/*=============================================================================
	UnPhysCollision.cpp: Skeletal mesh collision code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "UnCollision.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

#define MIN_HULL_VERT_DISTANCE		(0.1f * U2PScale)
#define MIN_HULL_VALID_DIMENSION	(0.5f * U2PScale)

#if __HAS_SSE__ && ENABLE_VECTORINTRINSICS
	static const VectorRegister VKindaSmallNumber = { KINDA_SMALL_NUMBER,KINDA_SMALL_NUMBER,KINDA_SMALL_NUMBER,KINDA_SMALL_NUMBER };
	static const VectorRegister VBigNumber = { BIG_NUMBER,BIG_NUMBER,BIG_NUMBER,BIG_NUMBER };
	static const VectorRegister VNegBigNumber = { -BIG_NUMBER,-BIG_NUMBER,-BIG_NUMBER,-BIG_NUMBER };
#endif

#define TOL_FUNC(x,y,absTol,relTol)  (Abs(x - y) <= Max(absTol, relTol * Max(Abs(x), Abs(y))))

///////////////////////////////////////
///////////// UKMeshProps /////////////
///////////////////////////////////////


void UKMeshProps::CopyMeshPropsFrom(UKMeshProps* fromProps)
{
	COMNudge = fromProps->COMNudge;
	AggGeom = fromProps->AggGeom;
}

void UKMeshProps::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
}

///////////////////////////////////////
/////////// FKAggregateGeom ///////////
///////////////////////////////////////

FBox FKAggregateGeom::CalcAABB(const FMatrix& BoneTM, const FVector& Scale3D)
{
	FBox Box(0);

	// If uniformly scaled, we look at sphere/box/sphyl bounds.
	if(Scale3D.IsUniform())
	{
		const FLOAT ScaleFactor = Scale3D.X;

		for(INT i=0; i<SphereElems.Num(); i++)
			Box += SphereElems(i).CalcAABB(BoneTM, ScaleFactor);

		for(INT i=0; i<BoxElems.Num(); i++)
			Box += BoxElems(i).CalcAABB(BoneTM, ScaleFactor);

		for(INT i=0; i<SphylElems.Num(); i++)
			Box += SphylElems(i).CalcAABB(BoneTM, ScaleFactor);
	}

	// Accumulate convex element bounding boxes.
	for(INT i=0; i<ConvexElems.Num(); i++)
		Box += ConvexElems(i).CalcAABB(BoneTM, Scale3D);

	return Box;

}

/**
  * Calculates a tight box-sphere bounds for the aggregate geometry; this is more expensive than CalcAABB
  * (tight meaning the sphere may be smaller than would be required to encompass the AABB, but all individual components lie within both the box and the sphere)
  *
  * @param Output The output box-sphere bounds calculated for this set of aggregate geometry
  *	@param BoneTM Transform matrix
  *	@param Scale3D Scale vector
  */
void FKAggregateGeom::CalcBoxSphereBounds(FBoxSphereBounds& Output, const FMatrix& BoneTM, const FVector& Scale3D)
{
	// Calculate the AABB
	const FBox AABB = CalcAABB(BoneTM, Scale3D);

	if ((SphereElems.Num() == 0) && (SphylElems.Num() == 0) && (BoxElems.Num() == 0))
	{
		// For bounds that only consist of convex shapes (such as anything generated from a BSP model),
		// we can get nice tight bounds by considering just the points of the convex shape
		const FMatrix LocalToWorld = FScaleMatrix(Scale3D) * BoneTM;
		const FVector Origin = AABB.GetCenter();

		FLOAT RadiusSquared = 0.0f;
		for (INT i = 0; i < ConvexElems.Num(); i++)
		{
			FKConvexElem& Elem = ConvexElems(i);
			for (INT j = 0; j < Elem.VertexData.Num(); ++j)
			{
				const FVector Point = LocalToWorld.TransformFVector(Elem.VertexData(j));
				RadiusSquared = Max(RadiusSquared, (Point - Origin).SizeSquared());
			}
		}

		// Push the resulting AABB and sphere into the output
		AABB.GetCenterAndExtents(Output.Origin, Output.BoxExtent);
		Output.SphereRadius = appSqrt(RadiusSquared);
	}
	else
	{
		// Just use the loose sphere bounds that totally fit the AABB
		Output = FBoxSphereBounds(AABB);
	}
}

// These line-checks do the normal weird Unreal thing of returning 0 if we hit or 1 otherwise.
UBOOL FKAggregateGeom::LineCheck(FCheckResult& Result, const FMatrix& Matrix, const FVector& Scale3D,  const FVector& End, const FVector& Start, const FVector& Extent, UBOOL bStopAtAnyHit, UBOOL bOnlyPerPolyShapes) const
{
	// JTODO: Do some kind of early rejection here! This is a bit rubbish...
	Result.Time = 1.0f;

	FCheckResult TempResult;

	// If we have some convex hulls to test - calculate some matrices up front.
	if(ConvexElems.Num() > 0)
	{
		// Total transform and scale for this convex hull.
		const FMatrix LocalToWorld = FScaleMatrix(Scale3D) * Matrix;
		const FMatrix WorldToLocal = LocalToWorld.Inverse();

		// Transform the line and box into local space of the hull.
		const FVector LocalStart = WorldToLocal.TransformFVector(Start);
		const FVector LocalEnd = WorldToLocal.TransformFVector(End);
		const FVector LocalExtent = FBox(-Extent, Extent).TransformBy(WorldToLocal).GetExtent();

#if 0
		// Transform world space box into a local space box.
		FMatrix WorldToBox = FMatrix::Identity;
		FVector BoxExtent = LocalExtent;
#else
		// Collide against sheared/rotated box that world space box becomes in local space.
		const FMatrix WorldToBox = WorldToLocal;
		const FVector BoxExtent = Extent;
#endif

		// Zero extent check
		if(	Extent.IsZero() )
		{
			// Iterate over each convex hull
			for(INT i=0; i<ConvexElems.Num(); i++)
			{
				const FKConvexElem& Convex = ConvexElems(i);

				TempResult.Time = Result.Time;
				Convex.LineCheck(TempResult, WorldToBox, LocalEnd, LocalStart, BoxExtent, bSkipCloseAndParallelChecks);

				if(TempResult.Time < Result.Time)
				{
					// Set this as the lowest (closest) hit.
					Result.Time = TempResult.Time;
					Result.Normal = TempResult.Normal;

					// If we just want any hit - quit out of loop over hulls now
					if(bStopAtAnyHit)
					{
						break;
					}
				}
			}
		}
		// Swept box check
		else
		{
			// Create local-space box around entire swept-box ray check.
			FBox LocalBox = FBox(0);
			LocalBox += LocalStart;
			LocalBox += LocalEnd;
			LocalBox.Min -= (LocalExtent + FVector(0.1f)); // The 0.1f is just to inflate the box a little to avoid issues on faces.
			LocalBox.Max += (LocalExtent + FVector(0.1f));

			// Iterate over each convex hull
			for(INT i=0; i<ConvexElems.Num(); i++)
			{
				const FKConvexElem& Convex = ConvexElems(i);

				// Check swept-box bounding box against hull bounding box.
				if( Convex.ElemBox.Intersect(LocalBox) )
				{
					// If we hit that, check against hull.
					TempResult.Time = 1.f;
					Convex.LineCheck(TempResult, WorldToBox, LocalEnd, LocalStart, BoxExtent, bSkipCloseAndParallelChecks);

					if(TempResult.Time < Result.Time)
					{
						// Set this as the lowest (closest) hit.
						Result.Time = TempResult.Time;
						Result.Normal = TempResult.Normal;
						Result.bStartPenetrating = TempResult.bStartPenetrating;

						// If we just want any hit - quit out of loop over hulls now
						if(bStopAtAnyHit)
						{
							break;
						}
					}
				}
			}
		}

		// If we did get a hit - process results (ie turn into world space). We do this at the end to only do it once.
		if(Result.Time < 1.f)
		{
			const FMatrix LocalToWorldTA = LocalToWorld.TransposeAdjoint();
			const FLOAT LocalToWorldDet = LocalToWorld.RotDeterminant();

			// Transform normal into world space (flip if necessary)
			Result.Normal = LocalToWorldTA.TransformNormal(Result.Normal);
			if(LocalToWorldDet < 0.f)
			{
				Result.Normal = -Result.Normal;
			}
			Result.Normal = Result.Normal.SafeNormal();

			// Calculate hit location using the time we were given.
			Result.Location = Start + (End - Start) * Result.Time;

			// If we just want any hit - we have one - exit now.
			if(bStopAtAnyHit) 
			{
				return FALSE;
			}
		}
	}

	// If we are uniformly scaled, test against other primitive types.
	// Note that at this point and onwards Result.Normal is in world space (we transformed at end of convex section).
	if( Scale3D.IsUniform() )
	{
		for(INT i=0; i<SphereElems.Num(); i++)
		{
			if(!bOnlyPerPolyShapes || SphereElems(i).bPerPolyShape)
			{
				TempResult.Time = 1.0f;

				FMatrix ElemTM = SphereElems(i).TM;
				ElemTM.ScaleTranslation(Scale3D);
				ElemTM *= Matrix;

				SphereElems(i).LineCheck(TempResult, ElemTM, Scale3D.X, End, Start, Extent);

				if(TempResult.Time < Result.Time)
				{
					Result = TempResult;

					if(bStopAtAnyHit)
					{
						return FALSE;
					}
				}
			}
		}

		for(INT i=0; i<BoxElems.Num(); i++)
		{
			if(!bOnlyPerPolyShapes || BoxElems(i).bPerPolyShape)
			{
				TempResult.Time = 1.0f;

				FMatrix ElemTM = BoxElems(i).TM;
				ElemTM.ScaleTranslation(Scale3D);
				ElemTM *= Matrix;

				BoxElems(i).LineCheck(TempResult, ElemTM, Scale3D.X, End, Start, Extent, bSkipCloseAndParallelChecks);

				if(TempResult.Time < Result.Time)
				{
					Result = TempResult;

					if(bStopAtAnyHit)
					{
						return FALSE;
					}
				}
			}
		}

		for(INT i=0; i<SphylElems.Num(); i++)
		{
			if(!bOnlyPerPolyShapes || SphylElems(i).bPerPolyShape)
			{
				TempResult.Time = 1.0f;

				FMatrix ElemTM = SphylElems(i).TM;
				ElemTM.ScaleTranslation(Scale3D);
				ElemTM *= Matrix;

				SphylElems(i).LineCheck(TempResult, ElemTM, Scale3D.X, End, Start, Extent);

				if(TempResult.Time < Result.Time)
				{
					Result = TempResult;

					if(bStopAtAnyHit)
					{
						return FALSE;
					}
				}
			}
		}
	}



	if(Result.Time < 1.0f)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


UBOOL FKAggregateGeom::PointCheck(FCheckResult& Result, const FMatrix& Matrix, const FVector& Scale3D, const FVector& Location, const FVector& Extent) const
{
	UBOOL bHit = FALSE;
	FLOAT BestDistance = BIG_NUMBER;


	// If the scale is uniform, we test against box elements
	if(Scale3D.IsUniform())
	{
		for(INT i=0; i<BoxElems.Num(); i++)
		{
			FMatrix ElemTM = BoxElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= Matrix;

			// This test returns results in WORLD space.
			const UBOOL bBoxHit = !BoxElems(i).PointCheck(Result, BestDistance, ElemTM, Scale3D.X, Location, Extent);

			// If we got a hit, calculate location.
			if(bBoxHit)
			{
				Result.Location = Location + (Result.Normal * BestDistance);

				bHit = TRUE;
			}
		}
	}

	// If we have some convex hulls to test - calculate some matrices up front.
	if(ConvexElems.Num() > 0)
	{
		// Total transform and scale for this convex hull.
		const FMatrix LocalToWorld = FScaleMatrix(Scale3D) * Matrix;
		const FMatrix WorldToLocal = LocalToWorld.Inverse();

		const FMatrix LocalToWorldTA = LocalToWorld.TransposeAdjoint();
		const FLOAT LocalToWorldDet = LocalToWorld.RotDeterminant();

		// Transform the point location into local space
		const FVector LocalLocation = WorldToLocal.TransformFVector(Location);

		// Transform bounding box into 
		const FBox LocalBox = FBox(Location-Extent, Location+Extent).TransformBy(WorldToLocal);

		// Collide against sheared/rotated box that world space box becomes in local space.
		const FMatrix WorldToBox = WorldToLocal;
		const FVector BoxExtent = Extent;

		// Iterate over each convex hull
		for(INT i=0; i<ConvexElems.Num(); i++)
		{
			const FKConvexElem& Convex = ConvexElems(i);

			// Quick reject based on bounding box
			if( Convex.ElemBox.Intersect(LocalBox) )
			{
				// This will only update the Normal in Result if it gets a hit closer than BestDistance.
				// Returns results in LOCAL space
				const UBOOL bConvexHit = !ConvexElems(i).PointCheck(Result, BestDistance, WorldToBox, LocalLocation, BoxExtent);

				// If we hit, transform results into world space
				if(bConvexHit)
				{
					// Calculate hit location in local space, and transform into world space.
					Result.Location = LocalToWorld.TransformFVector(LocalLocation + (Result.Normal * BestDistance));

					// Transform normal into world space (flip if necessary)
					Result.Normal = LocalToWorldTA.TransformNormal(Result.Normal);
					if(LocalToWorldDet < 0.f)
					{
						Result.Normal = -Result.Normal;
					}
					Result.Normal = Result.Normal.SafeNormal();

					bHit = TRUE;
				}
			}
		}
	}

#if 0
	// Draw the box we are testing (red for hit, blue for miss)
	DrawWireBox( GWorld->LineBatcher, FBox(Location-Extent, Location+Extent), bHit?FColor(255,0,0):FColor(0,0,255), SDPG_World );
#endif

	return !bHit;
}


/*GJKResult*/ BYTE FKAggregateGeom::ClosestPointOnAggGeomToPoint(const FMatrix& LocalToWorld, IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB)
{
	GJKResult Result = GJK_Fail;
	GJKResult CurResult;

	INT TotalAggCount = GetElementCount();
	TArray<FVector> AResults, BResults;
	AResults.Reserve(TotalAggCount);
	BResults.Reserve(TotalAggCount);

	//Accumulate results
	for (INT i=0; i<ConvexElems.Num(); i++)
	{
		const FKConvexElem& ConvexElem = ConvexElems(i);
		GJKHelperConvex ConvexHelper(ConvexElem, LocalToWorld);

		CurResult = ClosestPointsBetweenConvexPrimitives(ExtentHelper, &ConvexHelper, OutPointA, OutPointB);
		if (CurResult == GJK_Intersect)
		{
			//We intersected, just return immediately
			return CurResult;
		}

		AResults.AddItem(OutPointA);
		BResults.AddItem(OutPointB);
	}

	for (INT i=0; i<SphereElems.Num(); i++)
	{
		const FKSphereElem& SphereElem = SphereElems(i);
		GJKHelperSphere SphereHelper(SphereElem, LocalToWorld);
		CurResult = ClosestPointsBetweenConvexPrimitives(ExtentHelper, &SphereHelper, OutPointA, OutPointB);
		if (CurResult == GJK_Intersect)
		{
			//We intersected, just return immediately
			return CurResult;
		}

		AResults.AddItem(OutPointA);
		BResults.AddItem(OutPointB);
	}

	for (INT i=0; i<BoxElems.Num(); i++)
	{
		const FKBoxElem& BoxElem = BoxElems(i);
		GJKHelperBox BoxHelper(BoxElem, LocalToWorld);
		CurResult = ClosestPointsBetweenConvexPrimitives(ExtentHelper, &BoxHelper, OutPointA, OutPointB);
		if (CurResult == GJK_Intersect)
		{
			//We intersected, just return immediately
			return CurResult;
		}

		AResults.AddItem(OutPointA);
		BResults.AddItem(OutPointB);
	}

	//Figure out the closest point from all aggregate geom
	check(AResults.Num() == BResults.Num());
	if (AResults.Num() > 0)
	{
		//GWorld->LineBatcher->DrawLine(AResults(0), BResults(0), FLinearColor(1,0,0,1), SDPG_World);

		Result = GJK_NoIntersection;
		FLOAT MinDist = (AResults(0) - BResults(0)).SizeSquared();
		INT MinIndex = 0;
		for (INT i=1; i<AResults.Num(); i++)
		{
			FLOAT CurDist = (AResults(i) - BResults(i)).SizeSquared();
			//GWorld->LineBatcher->DrawLine(AResults(i), BResults(i), FLinearColor(1,0,0,1), SDPG_World);

			if (CurDist < MinDist)
			{
				MinDist = CurDist;
				MinIndex = i;
			}
		}

		OutPointA = AResults(MinIndex);
		OutPointB = BResults(MinIndex);
	}
	
	return Result;
}

/*GJKResult*/ BYTE FKAggregateGeom::ClosestPointOnAggGeomToComponent(const FMatrix& LocalToWorld, class UPrimitiveComponent*& OtherComponent, FVector& PointOnComponentA, FVector& PointOnComponentB)
{
	GJKResult Result = GJK_Fail;
	GJKResult CurResult;

	FVector OutPointA, OutPointB;

	INT TotalAggCount = GetElementCount();
	TArray<FVector> AResults, BResults;
	AResults.Reserve(TotalAggCount);
	BResults.Reserve(TotalAggCount);

	//Accumulate results
	for (INT i=0; i<ConvexElems.Num(); i++)
	{
		const FKConvexElem& ConvexElem = ConvexElems(i);
		GJKHelperConvex ConvexHelper(ConvexElem, LocalToWorld);

		CurResult = (GJKResult)OtherComponent->ClosestPointOnComponentInternal(&ConvexHelper, OutPointA, OutPointB);
		if (CurResult == GJK_Intersect)
		{
			//We intersected, just return immediately
			return CurResult;
		}

		AResults.AddItem(OutPointA);
		BResults.AddItem(OutPointB);
	}

	for (INT i=0; i<SphereElems.Num(); i++)
	{
		const FKSphereElem& SphereElem = SphereElems(i);
		GJKHelperSphere SphereHelper(SphereElem, LocalToWorld);
		CurResult = (GJKResult)OtherComponent->ClosestPointOnComponentInternal(&SphereHelper, OutPointA, OutPointB);
		if (CurResult == GJK_Intersect)
		{
			//We intersected, just return immediately
			return CurResult;
		}

		AResults.AddItem(OutPointA);
		BResults.AddItem(OutPointB);
	}

	for (INT i=0; i<BoxElems.Num(); i++)
	{
		const FKBoxElem& BoxElem = BoxElems(i);
		GJKHelperBox BoxHelper(BoxElem, LocalToWorld);
		CurResult = (GJKResult)OtherComponent->ClosestPointOnComponentInternal(&BoxHelper, OutPointA, OutPointB);
		if (CurResult == GJK_Intersect)
		{
			//We intersected, just return immediately
			return CurResult;
		}

		AResults.AddItem(OutPointA);
		BResults.AddItem(OutPointB);
	}

	//Figure out the closest point from all aggregate geom
	check(AResults.Num() == BResults.Num());
	if (AResults.Num() > 0)
	{
		//GWorld->LineBatcher->DrawLine(AResults(0), BResults(0), FLinearColor(1,0,0,1), SDPG_World);

		Result = GJK_NoIntersection;
		FLOAT MinDist = (AResults(0) - BResults(0)).SizeSquared();
		INT MinIndex = 0;
		for (INT i=1; i<AResults.Num(); i++)
		{
			FLOAT CurDist = (AResults(i) - BResults(i)).SizeSquared();
			//GWorld->LineBatcher->DrawLine(AResults(i), BResults(i), FLinearColor(1,0,0,1), SDPG_World);

			if (CurDist < MinDist)
			{
				MinDist = CurDist;
				MinIndex = i;
			}
		}

		PointOnComponentA = AResults(MinIndex);
		PointOnComponentB = BResults(MinIndex);
	}
	
	return Result;
}


#if WITH_NOVODEX
extern void ScaleNovodexTMPosition(NxMat34& TM, const FVector& Scale3D);
#endif // WITH_NOVODEX

static void AddVertexIfNotPresent(TArray<FVector> &Vertices, const FVector& NewVertex)
{
	UBOOL bIsPresent = 0;

	for(INT i=0; i<Vertices.Num() && !bIsPresent; i++)
	{
		const FLOAT DiffSqr = (NewVertex - Vertices(i)).SizeSquared();
		if(DiffSqr < MIN_HULL_VERT_DISTANCE * MIN_HULL_VERT_DISTANCE)
		{
			bIsPresent = TRUE;
		}
	}

	if(!bIsPresent)
	{
		Vertices.AddItem(NewVertex);
	}
}

static void AddVertexIfNotPresent(TArray<FSimplexVertex> &Vertices, const FSimplexVertex& NewVertex)
{
	UBOOL bIsPresent = 0;

	for(INT i=0; i<Vertices.Num() && !bIsPresent; i++)
	{
		const FLOAT DiffSqr = (NewVertex.Vertex - Vertices(i).Vertex).SizeSquared();
		if(DiffSqr < MIN_HULL_VERT_DISTANCE * MIN_HULL_VERT_DISTANCE)
		{
			bIsPresent = TRUE;
		}
	}

	if(!bIsPresent)
	{
		Vertices.AddItem(NewVertex);
	}
}

static void RemoveDuplicateVerts(TArray<FVector>& InVerts)
{
	TArray<FVector> BackupVerts = InVerts;
	InVerts.Empty();

	for(INT i=0; i<BackupVerts.Num(); i++)
	{
		AddVertexIfNotPresent(InVerts, BackupVerts(i));
	}
}

// Weisstein, Eric W. "Point-Line Distance--3-Dimensional." From MathWorld--A Wolfram Web Resource. http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html 
static FLOAT DistanceToLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point)
{
	const FVector StartToEnd = LineEnd - LineStart;
	const FVector PointToStart = LineStart - Point;

	const FVector Cross = StartToEnd ^ PointToStart;
	return Cross.Size()/StartToEnd.Size();
}

/** 
 *	Utility that ensures the verts supplied form a valid hull. Will modify the verts to remove any duplicates. 
 *	Positions should be in physics scale.
 *	Returns TRUE is hull is valid.
 */
static UBOOL EnsureHullIsValid(TArray<FVector>& InVerts)
{
	RemoveDuplicateVerts(InVerts);

	if(InVerts.Num() < 3)
	{
		return FALSE;
	}

	// Take any vert. In this case - the first one.
	const FVector FirstVert = InVerts(0);

	// Now find vert furthest from this one.
	FLOAT FurthestDist = 0.f;
	INT FurthestVertIndex = INDEX_NONE;
	for(INT i=1; i<InVerts.Num(); i++)
	{
		const FLOAT TestDist = (InVerts(i) - FirstVert).Size();
		if(TestDist > FurthestDist)
		{
			FurthestDist = TestDist;
			FurthestVertIndex = i;
		}
	}

	// If smallest dimension is too small - hull is invalid.
	if(FurthestVertIndex == INDEX_NONE || FurthestDist < MIN_HULL_VALID_DIMENSION)
	{
		return FALSE;
	}

	// Now find point furthest from line defined by these 2 points.
	FLOAT ThirdPointDist = 0.f;
	INT ThirdPointIndex = INDEX_NONE;
	for(INT i=1; i<InVerts.Num(); i++)
	{
		if(i != FurthestVertIndex)
		{
			const FLOAT TestDist = DistanceToLine(FirstVert, InVerts(FurthestVertIndex), InVerts(i));
			if(TestDist > ThirdPointDist)
			{
				ThirdPointDist = TestDist;
				ThirdPointIndex = i;
			}
		}
	}

	// If this dimension is too small - hull is invalid.
	if(ThirdPointIndex == INDEX_NONE || ThirdPointDist < MIN_HULL_VALID_DIMENSION)
	{
		return FALSE;
	}

	// Now we check each remaining point against this plane.

	// First find plane normal.
	const FVector Dir1 = InVerts(FurthestVertIndex) - InVerts(0);
	const FVector Dir2 = InVerts(ThirdPointIndex) - InVerts(0);
	FVector PlaneNormal = Dir1 ^ Dir2;
	const UBOOL bNormalizedOk = PlaneNormal.Normalize();
	if(!bNormalizedOk)
	{
		return FALSE;
	}

	// Now iterate over all remaining vertices.
	FLOAT MaxThickness = 0.f;
	for(INT i=1; i<InVerts.Num(); i++)
	{
		if((i != FurthestVertIndex) && (i != ThirdPointIndex))
		{
			const FLOAT PointPlaneDist = Abs((InVerts(i) - InVerts(0)) | PlaneNormal);
			MaxThickness = ::Max(PointPlaneDist, MaxThickness);
		}
	}

	if(MaxThickness < MIN_HULL_VALID_DIMENSION)
	{
		return FALSE;
	}

	return TRUE;
}


#define DUMP_PROBLEM_CONVEX		(0)

static INT ConvexDumpIndex = 0;

/**
 *	Take an FKAggregateGeom and cook the convex hull data into a FKCachedConvexData for a particular scale.
 *	This is a slow process, so is best done off-line if possible, and the results stored.
 *
 *	@param		OutCacheData	Will be filled with cooked data, one entry for each convex hull.
 *	@param		ConvexElems		The input aggregate geometry. Each convex element will be cooked.
 *	@param		Scale3D			The 3D scale that the geometry should be cooked at.
 *	@param		debugName		Debug name string, used for printing warning messages and the like.
 */
void MakeCachedConvexDataForAggGeom(FKCachedConvexData* OutCacheData, const TArray<FKConvexElem>& ConvexElems, const FVector& Scale3D, const TCHAR* debugName)
{
#if WITH_NOVODEX && WITH_PHYSX_COOKING
	check(OutCacheData);

	OutCacheData->CachedConvexElements.Empty();

	for(INT i=0; i<ConvexElems.Num(); i++)
	{
		const FKConvexElem* ConvexElem = &(ConvexElems(i));

		// First scale verts stored into physics scsale - including any non-uniform scaling for this mesh.
		TArray<FVector> PhysScaleConvexVerts;
		PhysScaleConvexVerts.Add( ConvexElem->VertexData.Num() );

		for(INT j=0; j<ConvexElem->VertexData.Num(); j++)
		{
			PhysScaleConvexVerts(j) = ConvexElem->VertexData(j) * Scale3D * U2PScale;
		}

		// Do nothing if array is empty.
		if(PhysScaleConvexVerts.Num() > 0)
		{
			// Create mesh. Will compute convex hull of supplied verts.
			NxConvexMeshDesc ConvexMeshDesc;
			ConvexMeshDesc.numVertices = PhysScaleConvexVerts.Num();
			ConvexMeshDesc.pointStrideBytes = sizeof(FVector);
			ConvexMeshDesc.points = PhysScaleConvexVerts.GetData();
			ConvexMeshDesc.flags = NX_CF_COMPUTE_CONVEX | NX_CF_USE_UNCOMPRESSED_NORMALS | NX_CF_INFLATE_CONVEX;

			const INT NewElementIndex = OutCacheData->CachedConvexElements.AddZeroed();
			FNxMemoryBuffer Buffer(&(OutCacheData->CachedConvexElements(NewElementIndex).ConvexElementData));
#if USE_QUICKLOAD_CONVEX
			UBOOL bSuccess = GNovodeXQuickLoad->cookConvexMesh(ConvexMeshDesc, Buffer, *GNovodexCooking);
#else
			UBOOL bSuccess = GNovodexCooking->NxCookConvexMesh(ConvexMeshDesc, Buffer);
#endif

			// If we fail, remove from array and warn.
			if(!bSuccess)
			{
				debugfSuppressed( NAME_DevPhysics, TEXT("FKAggregateGeom::InstanceNovodexGeom: (%s) Problem instancing ConvexElems(%d)."), debugName, i );

				OutCacheData->CachedConvexElements.Remove(NewElementIndex);

				// Code for dumping verts of convex hulls that fail to cook.
#if DUMP_PROBLEM_CONVEX
				// Make the filename
				TCHAR File[MAX_SPRINTF]=TEXT("");
				TCHAR* FileNameBase = TEXT("ConvexDump_");
				appSprintf( File, TEXT("%s%03i.txt"), FileNameBase, ConvexDumpIndex++ );

				// Make the text to dump.
				FString DumpString(TEXT(""));

				DumpString += FString::Printf( TEXT("%s%s"), debugName, LINE_TERMINATOR );
				DumpString += FString::Printf( TEXT("Hull Index: %d%s%s"), i, LINE_TERMINATOR, LINE_TERMINATOR );

				for(INT VertIndex = 0; VertIndex < PhysScaleConvexVerts.Num(); VertIndex++)
				{
					FVector Vert = PhysScaleConvexVerts(VertIndex);
					DumpString += FString::Printf( TEXT("%f %f %f%s"), Vert.X, Vert.Y, Vert.Z, LINE_TERMINATOR, LINE_TERMINATOR );
				}

				// Dump text to a text file.
				appSaveStringToFile( DumpString, File );
#endif
			}
		}
		else
		{
			debugf( TEXT("Convex with no verts (%s)"), debugName );
		}
	}
#endif // WITH_NOVODEX && WITH_PHYSX_COOKING
}


#if PERF_SHOW_PHYS_INIT_COSTS
extern DOUBLE TotalInstanceGeomTime;
extern INT TotalConvexGeomCount;
#endif

#if WITH_NOVODEX

NxActorDesc* FKAggregateGeom::InstanceNovodexGeom(const FVector& uScale3D, FKCachedConvexData* InCacheData, UBOOL bCreateCCDSkel, const TCHAR* debugName)
{
#if PERF_SHOW_PHYS_INIT_COSTS || defined(SHOW_SLOW_CONVEX)
	DOUBLE Start = appSeconds();
#endif

	// Convert scale to physical units.
	FVector pScale3D = uScale3D * U2PScale;

	UINT NumElems;
	if (pScale3D.IsUniform())
	{
		NumElems = GetElementCount();
	}
	else
	{
		NumElems = ConvexElems.Num();
	}

	if (NumElems == 0)
	{
		if (!pScale3D.IsUniform() && GetElementCount() > 0)
		{
			debugf(TEXT("FKAggregateGeom::InstanceNovodexGeom: (%s) Cannot 3D-Scale rigid-body primitives (sphere, box, sphyl)."), debugName);
		}
		else
		{
			debugf(TEXT("FKAggregateGeom::InstanceNovodexGeom: (%s) No geometries in FKAggregateGeom."), debugName);
		}
	}

	NxActorDesc* ActorDesc = new NxActorDesc;

	// Include spheres, boxes and sphyls only when the scale is uniform.
	if (pScale3D.IsUniform())
	{
		// Sphere primitives
		for (int i = 0; i < SphereElems.Num(); i++)
		{
			FKSphereElem* SphereElem = &SphereElems(i);
			if(!SphereElem->bNoRBCollision)
			{
				NxMat34 RelativeTM = U2NMatrixCopy(SphereElem->TM);
				ScaleNovodexTMPosition(RelativeTM, pScale3D);

				NxSphereShapeDesc* SphereDesc = new NxSphereShapeDesc;
				SphereDesc->radius = (SphereElem->Radius * pScale3D.X) + PhysSkinWidth;
				SphereDesc->localPose = RelativeTM;
				if(bCreateCCDSkel)
				{
					MakeCCDSkelForSphere(SphereDesc);
				}

				ActorDesc->shapes.pushBack(SphereDesc);
			}
		}

		// Box primitives
		for (int i = 0; i < BoxElems.Num(); i++)
		{
			FKBoxElem* BoxElem = &BoxElems(i);
			if(!BoxElem->bNoRBCollision)
			{
				NxMat34 RelativeTM = U2NMatrixCopy(BoxElem->TM);
				ScaleNovodexTMPosition(RelativeTM, pScale3D);

				NxBoxShapeDesc* BoxDesc = new NxBoxShapeDesc;
				BoxDesc->dimensions = (0.5f * NxVec3(BoxElem->X * pScale3D.X, BoxElem->Y * pScale3D.X, BoxElem->Z * pScale3D.X)) + NxVec3(PhysSkinWidth, PhysSkinWidth, PhysSkinWidth);
				BoxDesc->localPose = RelativeTM;
				if(bCreateCCDSkel)
				{
					MakeCCDSkelForBox(BoxDesc);
				}

				ActorDesc->shapes.pushBack(BoxDesc);
			}
		}

		// Sphyl (aka Capsule) primitives
		for (int i =0; i < SphylElems.Num(); i++)
		{
			FKSphylElem* SphylElem = &SphylElems(i);
			if(!SphylElem->bNoRBCollision)
			{
				// The stored sphyl transform assumes the sphyl axis is down Z. In Novodex, it points down Y, so we twiddle the matrix a bit here (swap Y and Z and negate X).
				FMatrix SphylRelTM = FMatrix::Identity;
				SphylRelTM.SetAxis( 0, -1.f * SphylElem->TM.GetAxis(0) );
				SphylRelTM.SetAxis( 1, SphylElem->TM.GetAxis(2) );
				SphylRelTM.SetAxis( 2, SphylElem->TM.GetAxis(1) );
				SphylRelTM.SetOrigin( SphylElem->TM.GetOrigin() );

				NxMat34 RelativeTM = U2NMatrixCopy(SphylRelTM);
				ScaleNovodexTMPosition(RelativeTM, pScale3D);

				NxCapsuleShapeDesc* CapsuleDesc = new NxCapsuleShapeDesc;
				CapsuleDesc->radius = (SphylElem->Radius * pScale3D.X) + PhysSkinWidth;
				CapsuleDesc->height = SphylElem->Length * pScale3D.X;
				CapsuleDesc->localPose = RelativeTM;
				if(bCreateCCDSkel)
				{
					MakeCCDSkelForSphyl(CapsuleDesc);
				}

				ActorDesc->shapes.pushBack(CapsuleDesc);
			}
		}
	}

	// Convex mesh primitives

	FKCachedConvexData TempCacheData;
	FKCachedConvexData* UseCacheData = NULL;

	// If we were passed in the cooked convex data, use that.
	if(InCacheData && bUsePrecookedPhysData)
	{
		UseCacheData = InCacheData;
	}
	// If not, cook it now into TempCacheData and use that
	else
	{
#if !FINAL_RELEASE
		if((!GIsEditor || GIsPlayInEditorWorld) && ConvexElems.Num() > 0)
		{
#if WIIU
			debugf( TEXT("WARNING: Cooking Convex For %s (Scale: %0.7f %0.7f %0.7f)"),
				debugName ? debugName : TEXT("* No Name *"), uScale3D.X, uScale3D.Y, uScale3D.Z );
			debugf( TEXT("\tPlease go and set PreCachedPhysScale on the mesh to have this scale for RunTime spawned objects."));
#else
			debugf( TEXT("WARNING: Cooking Convex For %s (Scale: %f %f %f) Please go and set PreCachedPhysScale on the mesh to have this scale for RunTime spawned objects."), 
				debugName ? debugName : TEXT("* No Name *"), uScale3D.X, uScale3D.Y, uScale3D.Z );
#endif
		}
#endif

		MakeCachedConvexDataForAggGeom(&TempCacheData, ConvexElems, uScale3D, debugName);
		UseCacheData = &TempCacheData;
	}

	// Iterate over each element in the cached data.
	for (INT i=0; i<UseCacheData->CachedConvexElements.Num(); i++)
	{
#if XBOX
		if( GetCookedPhysDataEndianess(UseCacheData->CachedConvexElements(i).ConvexElementData) == CPDE_LittleEndian )
		{
			debugf( TEXT("InstanceNovodexGeom: Found Little Endian Data: %s"), debugName );
		}

		check( GetCookedPhysDataEndianess(UseCacheData->CachedConvexElements(i).ConvexElementData) != CPDE_LittleEndian );
#endif

		// Create convex mesh from the cached data
		FNxMemoryBuffer Buffer( &(UseCacheData->CachedConvexElements(i).ConvexElementData) );
#if USE_QUICKLOAD_CONVEX
		NxConvexMesh* ConvexMesh = GNovodeXQuickLoad->createConvexMesh(Buffer);
#else 
		NxConvexMesh* ConvexMesh = GNovodexSDK->createConvexMesh(Buffer);
#endif
#if PERF_SHOW_PHYS_INIT_COSTS
		TotalConvexGeomCount++;
#endif

		if(ConvexMesh)
		{
			SetNxConvexMeshRefCount(ConvexMesh, DelayNxMeshDestruction);
			GNumPhysXConvexMeshes++;
		}

		// If we have a convex, and this object is uniformly scaled, we see if we could use a box instead.
		if( ConvexMesh && pScale3D.IsUniform() )
		{     
			if( RepresentConvexAsBox( ActorDesc, ConvexMesh, bCreateCCDSkel ) )
			{
#if USE_QUICKLOAD_CONVEX
				GNovodeXQuickLoad->releaseConvexMesh(*ConvexMesh);
				GNumPhysXConvexMeshes--;
#else
				GNovodexPendingKillConvex.AddItem(ConvexMesh);
#endif
				ConvexMesh = NULL;				
			}
		}

		// Convex mesh creation may fail, or may have decided to use box - in which case we do nothing more.
		if(ConvexMesh)
		{
			NxConvexShapeDesc* ConvexShapeDesc = new NxConvexShapeDesc;
			ConvexShapeDesc->meshData = ConvexMesh;
			ConvexShapeDesc->meshFlags = 0;
			if(bCreateCCDSkel)
			{
				MakeCCDSkelForConvex(ConvexShapeDesc);
			}

			ActorDesc->shapes.pushBack(ConvexShapeDesc);
		}
	}

#if PERF_SHOW_PHYS_INIT_COSTS || defined(SHOW_SLOW_CONVEX)
	// Update total time.
	DOUBLE InstanceGeomTime = (appSeconds() - Start);
#endif
	
#ifdef SHOW_SLOW_CONVEX
	if((InstanceGeomTime*1000.f) > 1.f)
	{
		debugf( TEXT("INIT SLOW GEOM: %s took %f ms (%d hulls)"), debugName, InstanceGeomTime * 1000.f, UseCacheData->CachedConvexElements.Num());
	}
#endif

#if PERF_SHOW_PHYS_INIT_COSTS
	TotalInstanceGeomTime += InstanceGeomTime;
#endif

	if(ActorDesc->shapes.size() == 0)
	{
		delete ActorDesc;
		return NULL;
	}
	else
	{
		return ActorDesc;
	}
}

#endif


/**
 *	Take a UStaticMesh and cook the per-tri data into an OutCacheData for a particular scale.
 *	This is a slow process, so is best done off-line if possible, and the results stored.
 */
void MakeCachedPerTriMeshDataForStaticMesh(FKCachedPerTriData* OutCacheData, UStaticMesh* InMesh, const FVector& Scale3D, const TCHAR* DebugName)
{
	// If we have a negative determinant for this matrix, we need to flip the surface normals because it
	// will invert the winding order.
	const FLOAT Det = Scale3D.X * Scale3D.Y * Scale3D.Z;
	const UBOOL bFlipTris = (Det < 0.f);

	// Scale all verts into temporary vertex buffer.
	const UINT NumVerts = InMesh->LODModels(0).NumVertices;
	TArray<FVector>	TransformedVerts;
	TransformedVerts.Add(NumVerts);
	for(UINT i=0; i<NumVerts; i++)
	{
		TransformedVerts(i) = InMesh->LODModels(0).PositionVertexBuffer.VertexPosition(i) * Scale3D * U2PScale;
	}

#if WITH_NOVODEX && WITH_PHYSX_COOKING
	// Create Novodex mesh descriptor and fill it in.
	NxTriangleMeshDesc StaticMeshDesc;

	StaticMeshDesc.numVertices = TransformedVerts.Num();
	StaticMeshDesc.pointStrideBytes = sizeof(FVector);
	StaticMeshDesc.points = TransformedVerts.GetData();

	StaticMeshDesc.numTriangles = InMesh->LODModels(0).IndexBuffer.Indices.Num()/3;
	StaticMeshDesc.triangleStrideBytes = 3*sizeof(WORD);
	StaticMeshDesc.triangles = InMesh->LODModels(0).IndexBuffer.Indices.GetData();

	StaticMeshDesc.flags = NX_MF_16_BIT_INDICES;
	if(!bFlipTris)
	{
		StaticMeshDesc.flags |= NX_MF_FLIPNORMALS;
	}

	// Create Novodex mesh from that info.
	OutCacheData->CachedPerTriData.Empty();
	FNxMemoryBuffer Buffer(&OutCacheData->CachedPerTriData);
	if( GNovodexCooking->NxGetCookingParams().targetPlatform == PLATFORM_PC )
	{
		StaticMeshDesc.flags |= NX_MF_HARDWARE_MESH;
	}
	GNovodexCooking->NxCookTriangleMesh(StaticMeshDesc, Buffer);
#endif // WITH_NOVODEX && WITH_PHYSX_COOKING
}

///////////////////////////////////////
///////////// FKSphereElem ////////////
///////////////////////////////////////

FBox FKSphereElem::CalcAABB(const FMatrix& BoneTM, FLOAT Scale)
{
	FMatrix ElemTM = TM;
	ElemTM.ScaleTranslation( FVector(Scale) );
	ElemTM *= BoneTM;

	const FVector BoxCenter = ElemTM.GetOrigin();
	const FVector BoxExtents(Radius * Scale);

	return FBox(BoxCenter - BoxExtents, BoxCenter + BoxExtents);
}

static UBOOL SphereLineIntersect(FCheckResult& Result, const FVector& Origin, FLOAT Radius, const FVector& Start, const FVector& Dir, FLOAT Length)
{
	const FVector StartToOrigin = Origin - Start;

	const FLOAT L2 = StartToOrigin.SizeSquared(); // Distance of line start from sphere centre (squared).
	const FLOAT R2 = Radius*Radius;

	// If we are starting inside sphere, return a hit.
	if ( L2 < R2 )
	{
		Result.Time = 0.0f;
		Result.Location = Start;
		Result.Normal = -StartToOrigin.SafeNormal();

		return 0;
	}

	if(Length < KINDA_SMALL_NUMBER)
		return 1; // Zero length and not starting inside - doesn't hit.

	const FLOAT D = StartToOrigin | Dir; // distance of sphere centre in direction of query.
	if ( D < 0.0f )
		return 1; // sphere is behind us, but we are not inside it.

	const FLOAT M2 = L2 - (D*D); // pythag - triangle involving StartToCenter, (Dir * D) and vec between SphereCenter & (Dir * D)
	if (M2 > R2) 
		return 1; // ray misses sphere

	// this does pythag again. Q2 = R2 (radius squared) - M2
	const FLOAT t = D - appSqrt(R2 - M2);
	if ( t > Length ) 
		return 1; // Ray doesn't reach sphere, reject here.

	Result.Location = Start + Dir * t;
	Result.Normal = (Result.Location - Origin).SafeNormal();
	Result.Time = t * (1.0f/Length);

	return 0;

}

UBOOL FKSphereElem::LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale, const FVector& End, const FVector& Start, const FVector& Extent) const
{
	if( !Extent.IsZero() )
	{
		return TRUE;
	}

	const FVector SphereCenter = Matrix.GetOrigin();
	FVector Dir = End - Start;
	const FLOAT Length = Dir.Size();
	if(Length > KINDA_SMALL_NUMBER)
	{
		Dir *= (1.0f/Length);
	}

	return SphereLineIntersect(Result, SphereCenter, Radius*Scale, Start, Dir, Length);

}

/** What is considered 'too close' for a check co-planar to a separating plane. */
static const FLOAT BoxSweptBox_ParallelRegion = 0.01f;

/** Wraps the state used by an individual triangle versus swept box separating axis check. */
class FBoxSweptBoxSeparatingAxisCheck
{
public:

	/** Indicates that the line check has become too close and parallel to a separating plane. */
	UBOOL bCloseAndParallel;

	/** If bCloseAndParallel is TRUE, this is the normal of the close feature. */
	FVector CloseFeatureNormal;

	/** Default constructor. */
	FBoxSweptBoxSeparatingAxisCheck()
	:	bCloseAndParallel(FALSE)
	,	CloseFeatureNormal(0,0,0)
	{
	}

	UBOOL TestSeparatingAxis(
		const FOrientedBox& TraceBox, const FOrientedBox& LocalBox,
		const FVector& Axis,
		const FVector& LocalStart, const FVector& LocalEnd,
		FLOAT& MinIntersectTime, FLOAT& MaxIntersectTime,
		FVector& HitNormal, FVector& ExitDir)
	{
		// Project box shape onto separating axis
		FInterval ProjectionInterval(LocalBox.Project(Axis));

		// Project trace box onto separating axis to find extent
		FLOAT ProjectedExtent = TraceBox.ExtentX * Abs(Axis | TraceBox.AxisX)
							  + TraceBox.ExtentY * Abs(Axis | TraceBox.AxisY)
							  + TraceBox.ExtentZ * Abs(Axis | TraceBox.AxisZ);

		// Add projected extent to box (convolving shapes basically)
		ProjectionInterval.Expand(ProjectedExtent);

		const FLOAT ProjectedStart = Axis | LocalStart;
		const FLOAT ProjectedEnd = Axis | LocalEnd;

		// This gives the component of the check vector in the test axis.
		const FLOAT ProjectedDirection = ProjectedEnd - ProjectedStart;

		const FLOAT ProjDirMag = Abs(ProjectedDirection);
		// If zero - check vector is perp to test axis, so just check if we start outside. If so, we can't collide.
		if( ProjDirMag < 0.01f )
		{
			if( ProjectedStart < ProjectionInterval.Min && ProjectedStart > (ProjectionInterval.Min - BoxSweptBox_ParallelRegion) )
			{
				CloseFeatureNormal = -Axis;
				bCloseAndParallel = TRUE;
			}

			if( ProjectedStart > ProjectionInterval.Max && ProjectedStart < (ProjectionInterval.Max + BoxSweptBox_ParallelRegion) )
			{
				CloseFeatureNormal = Axis;
				bCloseAndParallel = TRUE;
			}

			if( ProjDirMag < SMALL_NUMBER )
			{
				if( ProjectedStart < ProjectionInterval.Min ||
					ProjectedStart > ProjectionInterval.Max )
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
			EntryTime = (ProjectionInterval.Min - ProjectedStart) * OneOverProjectedDirection;
			ExitTime = (ProjectionInterval.Max - ProjectedStart) * OneOverProjectedDirection;
			ImpactNormal = -Axis;
		}
		// Moving in a negative direction - enter ConvexMax and exit ConvexMin
		else
		{
			EntryTime = (ProjectionInterval.Max - ProjectedStart) * OneOverProjectedDirection;
			ExitTime = (ProjectionInterval.Min - ProjectedStart) * OneOverProjectedDirection;
			ImpactNormal = Axis;
		}	

		// See if entry time is further than current furthest entry time. If so, update and remember normal
		if(EntryTime > MinIntersectTime)
		{
			MinIntersectTime = EntryTime;
			HitNormal = ImpactNormal;
		}

		// See if exit time is closer than current closest exit time.
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

	UBOOL TestEdgeSeparatingAxis(
		const FOrientedBox& TraceBox, const FOrientedBox& LocalBox,
		const FVector& FirstEdge, const FVector& SecondEdge,
		const FVector& LocalStart, const FVector& LocalEnd,
		FLOAT& MinIntersectTime, FLOAT& MaxIntersectTime,
		FVector& HitNormal, FVector& ExitDir)
	{
		FVector Axis = FirstEdge ^ SecondEdge;
		if (Axis.SizeSquared() < DELTA)
		{
			return TRUE;
		}
		return TestSeparatingAxis(TraceBox, LocalBox, Axis, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir);
	}

	UBOOL TestFaceSeparatingAxis(
		const FOrientedBox& TraceBox, const FOrientedBox& LocalBox,
		const FVector& FaceNormal,
		const FVector& LocalStart, const FVector& LocalEnd,
		FLOAT& MinIntersectTime, FLOAT& MaxIntersectTime,
		FVector& HitNormal, FVector& ExitDir)
	{
		return TestSeparatingAxis(TraceBox, LocalBox, FaceNormal, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir);
	}

	/**
	 * Checks two OBBs for overlap using separating axis theorem
	 *
	 * @param TraceBox the box being swept
	 * @param LocalBox the box of the object being checked
	 * @param LocalStart the starting point of the swept box
	 * @param LocalEnd the ending point of the swept box
	 * @param MinIntersectTime the earliest time for collision
	 * @param MaxIntersectTime the latest time for collision
	 * @param HitNormal the resulting normal (axis of least penetration)
	 *
	 * @return TRUE if they overlap, FALSE if they are disjoint
	 */
	UBOOL FindSeparatingAxis(const FOrientedBox& TraceBox, const FOrientedBox& LocalBox,
		const FVector& LocalStart, const FVector& LocalEnd,
		FLOAT& MinIntersectTime, FLOAT& MaxIntersectTime,
		FVector& HitNormal, FVector& ExitDir)
	{
		// Test separating axes from TraceBox faces (TraceBox.AxisX, TraceBox.AxisY, TraceBox.AxisY).
		if (!TestFaceSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisX, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestFaceSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisY, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestFaceSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisZ, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		// Test separating axes from LocalBox faces ([1 0 0], [0 1 0], [0 0 1]).
		if (!TestFaceSeparatingAxis(TraceBox, LocalBox, LocalBox.AxisX, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestFaceSeparatingAxis(TraceBox, LocalBox, LocalBox.AxisY, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestFaceSeparatingAxis(TraceBox, LocalBox, LocalBox.AxisZ, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		// Test separating axes from TraceBox edges x LocalBox edges (cross products of the above).
		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisX, LocalBox.AxisX, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisX, LocalBox.AxisY, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisX, LocalBox.AxisZ, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisY, LocalBox.AxisX, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisY, LocalBox.AxisY, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisY, LocalBox.AxisZ, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisZ, LocalBox.AxisX, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisZ, LocalBox.AxisY, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		if (!TestEdgeSeparatingAxis(TraceBox, LocalBox, TraceBox.AxisZ, LocalBox.AxisZ, LocalStart, LocalEnd, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
		{
			return FALSE;
		}

		// No collision if min is further than max
		if(MinIntersectTime > MaxIntersectTime)
		{
			return FALSE;
		}

		// If we get to this point, there was a collision

		return TRUE;
	}
};

///////////////////////////////////////
////////////// FKBoxElem //////////////
///////////////////////////////////////

FBox FKBoxElem::CalcAABB(const FMatrix& BoneTM, FLOAT Scale)
{
	FMatrix ElemTM = TM;
	ElemTM.ScaleTranslation( FVector(Scale) );
	ElemTM *= BoneTM;

	FVector Extent(0.5f * Scale * X, 0.5f * Scale * Y, 0.5f * Scale * Z);
	FBox LocalBox(-Extent, Extent);

	return LocalBox.TransformBy(ElemTM);

}

UBOOL FKBoxElem::LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale,  const FVector& End, const FVector& Start, const FVector& Extent, UBOOL bSkipCloseAndParallelChecks) const
{
	// Transform start and end points into box space.
	FVector LocalStart = Matrix.InverseTransformFVectorNoScale(Start);
	FVector LocalEnd = Matrix.InverseTransformFVectorNoScale(End);

	FVector Radii = Scale * 0.5f * FVector(X, Y, Z);

	if( Extent.IsZero() )
	{
		FBox LocalBox( -Radii, Radii);

		FVector LocalHitLocation, LocalHitNormal;
		FLOAT HitTime;

		UBOOL bHit = FLineExtentBoxIntersection(LocalBox, LocalStart, LocalEnd, FVector(0,0,0), LocalHitLocation, LocalHitNormal, HitTime);

		if(bHit)
		{
			Result.Location = Matrix.TransformFVector(LocalHitLocation);
			Result.Normal = Matrix.TransformNormal(LocalHitNormal);
			Result.Time = HitTime;

			return FALSE;
		}
	}
	else
	{
		// Construct representation of trace box in local (ie this box primitive's) space.
		FOrientedBox TraceBox;
		TraceBox.Center = LocalStart;
		// Transform the axis into local space
		TraceBox.AxisX = Matrix.InverseTransformNormalNoScale(FVector(1.f,0.f,0.f));
		TraceBox.AxisY = Matrix.InverseTransformNormalNoScale(FVector(0.f,1.f,0.f));
		TraceBox.AxisZ = Matrix.InverseTransformNormalNoScale(FVector(0.f,0.f,1.f));
		TraceBox.ExtentX = Extent.X;
		TraceBox.ExtentY = Extent.Y;
		TraceBox.ExtentZ = Extent.Z;

		FOrientedBox LocalBox;
		LocalBox.ExtentX = Radii.X;
		LocalBox.ExtentY = Radii.Y;
		LocalBox.ExtentZ = Radii.Z;

		FLOAT MinIntersectTime = -BIG_NUMBER;
		FLOAT MaxIntersectTime = BIG_NUMBER;
		FVector HitNormal(0,0,0), ExitDir;

		FBoxSweptBoxSeparatingAxisCheck Check;

		// Check for overlap
		if (Check.FindSeparatingAxis(TraceBox,LocalBox,LocalStart,LocalEnd,MinIntersectTime,MaxIntersectTime,HitNormal,ExitDir))
		{
			// If we are close and parallel, and there is not another axis which provides a good separation.
			if(Check.bCloseAndParallel && !(MinIntersectTime > Result.Time && Check.CloseFeatureNormal != HitNormal && Check.CloseFeatureNormal != -HitNormal) && !bSkipCloseAndParallelChecks)
			{
				// If in danger, disallow movement, in case it puts us inside the object.
				Result.Time = 0.f;

				// Tilt the normal slightly back towards the direction of travel. 
				// This will cause the next movement to be away from the surface (hopefully out of the 'danger region')
				FVector CheckDir = (LocalEnd - LocalStart).SafeNormal();
				Result.Normal = (Check.CloseFeatureNormal.SafeNormal() - (0.05f * CheckDir)).SafeNormal();

				// Say we hit.
				return FALSE;
			}

			if(MinIntersectTime > Result.Time)
			{
				return TRUE;
			}

			// If hit is after start of ray - normal hit
			if(MinIntersectTime >= 0.f)
			{
				Result.Normal = Matrix.TransformNormal(HitNormal);
				Result.Normal.Normalize();
				Result.Time = MinIntersectTime;
			}
			// If hit time is negative- the entry is behind the start - so we started colliding
			else
			{
				FVector LineDir = (LocalEnd - LocalStart).SafeNormal();
				ExitDir = ExitDir.SafeNormal();

				// If the exit surface in front is closer than the exit surface behind us, 
				// and we are moving towards the exit surface in front
				// allow movement (report no hit) to let us exit this hull.
				if( (MaxIntersectTime < -MinIntersectTime) && (LineDir | ExitDir) > 0.0f)
				{
					Result.Time = 1.f;
					return TRUE;
				}
				else
				{
					Result.Time = 0.f;
					Result.bStartPenetrating = TRUE;

					// Use a vector pointing back along check vector as the hit normal
					Result.Normal = Matrix.TransformNormal(-LineDir);
					Result.Normal.Normalize();
				}
			}

			Result.Location = Start + (End - Start) * Result.Time;

			// Return that the trace was not completed end to end (hit something)
			return FALSE;
		}
	}
	return TRUE;
}

inline UBOOL TestBoxBoxSeparatingAxis(
	const FVector& Line,
	const FVector& PointLocation,
	const FVector& PointExtent,
	const FVector& BoxLocation,
	const FVector& BoxExtent,
	const FVector& BoxX,
	const FVector& BoxY,
	const FVector& BoxZ,
	FLOAT& MinPenetration,
	FVector& HitNormal)
{
	// Discard any test axes that are zero (eg cross product of 2 parallel edges)
	if(Line.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return TRUE;
	}

	// Project location of Box onto axis
	const FLOAT ProjectedBox = Line | BoxLocation;

	// Calculate world box extent projected along test axis.
	const FLOAT ProjectedBoxExtent = BoxExtent.X * Abs(Line | BoxX) + BoxExtent.Y * Abs(Line | BoxY) + BoxExtent.Z * Abs(Line | BoxZ);

	// Project location of Point onto axis
	const FLOAT ProjectedPoint = Line | PointLocation;

	// Note that Abs(Line | PointX) is the same as Line.X becuase PointX is (1,0,0)
	const FLOAT ProjectedPointExtent = PointExtent.X * Abs(Line.X) + PointExtent.Y * Abs(Line.Y) + PointExtent.Z * Abs(Line.Z);

	// Find Min and Max of Box projected onto test axis.
	FLOAT BoxMin = ProjectedBox - ProjectedBoxExtent;
	FLOAT BoxMax = ProjectedBox + ProjectedBoxExtent;

	// Expand by the extent of the point - projected along the test axis.
	BoxMin -= ProjectedPointExtent;
	BoxMax += ProjectedPointExtent;

	// If we have an intersection along this axis - take note of the min penetration if its the lowest so far.
	if(ProjectedPoint >= BoxMin && ProjectedPoint <= BoxMax)
	{
		// Use inverse sqrt because that is fast and we do more math with the inverse value anyway
		FLOAT InvAxisMagnitude = appInvSqrt(Line.X * Line.X +	Line.Y * Line.Y + Line.Z * Line.Z);
		FLOAT ScaledBestDist = MinPenetration / InvAxisMagnitude;
		FLOAT MinPenetrationDist = ProjectedPoint - BoxMin;
		FLOAT MaxPenetrationDist = BoxMax - ProjectedPoint;

		if(MinPenetrationDist < ScaledBestDist)
		{
			MinPenetration = MinPenetrationDist * InvAxisMagnitude;
			HitNormal = -Line * InvAxisMagnitude;
		}
		if(MaxPenetrationDist < ScaledBestDist)
		{
			MinPenetration = MaxPenetrationDist * InvAxisMagnitude;
			HitNormal = Line * InvAxisMagnitude;
		}

		return TRUE;
	}
	// No overlap - we cannot have a collision.
	else
	{
		return FALSE;
	}
}

/** 
 *	Returns results in WORLD space. 
 *	Does not fill in Location.
 */
UBOOL FKBoxElem::PointCheck(FCheckResult& Result, FLOAT& OutBestDistance, const FMatrix& BoxMatrix, FLOAT BoxScale, const FVector& Location, const FVector& Extent) const
{
	// Information on both boxes in world reference frame.
	const FVector BoxX = BoxMatrix.GetAxis(0);
	const FVector BoxY = BoxMatrix.GetAxis(1);
	const FVector BoxZ = BoxMatrix.GetAxis(2);
	const FVector BoxLocation = BoxMatrix.GetOrigin();
	const FVector BoxExtent = BoxScale * 0.5f * FVector(X, Y, Z); // XYZ are length not radii, so half them here.

	FVector PointAxes[3];
	PointAxes[0] = FVector(1,0,0);
	PointAxes[1] = FVector(0,1,0);
	PointAxes[2] = FVector(0,0,1);
	const FVector PointLocation = Location;
	const FVector PointExtent = Extent;

	FLOAT MinPenetration = BIG_NUMBER;
	FVector HitNormal(0,0,0);

	// Box faces
	if(!TestBoxBoxSeparatingAxis(BoxX, PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
		return TRUE;

	if(!TestBoxBoxSeparatingAxis(BoxY, PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
		return TRUE;

	if(!TestBoxBoxSeparatingAxis(BoxZ, PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
		return TRUE;

	// Point faces
	if(!TestBoxBoxSeparatingAxis(PointAxes[0], PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
		return TRUE;

	if(!TestBoxBoxSeparatingAxis(PointAxes[1], PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
		return TRUE;

	if(!TestBoxBoxSeparatingAxis(PointAxes[2], PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
		return TRUE;

	// Each Box edge crossed with each Point edge.
	for(INT i=0; i<3; i++)
	{
		if(!TestBoxBoxSeparatingAxis(BoxX ^ PointAxes[i], PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
			return TRUE;

		if(!TestBoxBoxSeparatingAxis(BoxY ^ PointAxes[i], PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
			return TRUE;

		if(!TestBoxBoxSeparatingAxis(BoxZ ^ PointAxes[i], PointLocation, PointExtent, BoxLocation, BoxExtent, BoxX, BoxY, BoxZ, MinPenetration, HitNormal))
			return TRUE;
	}

	// If we reach this point some collision occurs

	Result.Normal = HitNormal;
	OutBestDistance = MinPenetration;

	return FALSE;
}

///////////////////////////////////////
////////////// FKSphylElem ////////////
///////////////////////////////////////

FBox FKSphylElem::CalcAABB(const FMatrix& BoneTM, FLOAT Scale)
{
	FMatrix ElemTM = TM;
	ElemTM.ScaleTranslation( FVector(Scale) );
	ElemTM *= BoneTM;

	const FVector SphylCenter = ElemTM.GetOrigin();

	// Get sphyl axis direction
	const FVector Axis = ElemTM.GetAxis(2);
	// Get abs of that vector
	const FVector AbsAxis(Abs(Axis.X), Abs(Axis.Y), Abs(Axis.Z));
	// Scale by length of sphyl
	const FVector AbsDist = (Scale * 0.5f * Length) * AbsAxis;

	const FVector MaxPos = SphylCenter + AbsDist;
	const FVector MinPos = SphylCenter - AbsDist;
	const FVector Extent(Scale * Radius);

	FBox Result(MinPos - Extent, MaxPos + Extent);

	return Result;
}

UBOOL FKSphylElem::LineCheck(FCheckResult& Result, const FMatrix& Matrix, FLOAT Scale,  const FVector& End, const FVector& Start, const FVector& Extent) const
{
	if(!Extent.IsZero())
	{
		return TRUE;
	}

	FVector LocalStart = Matrix.InverseTransformFVectorNoScale(Start);
	FVector LocalEnd = Matrix.InverseTransformFVectorNoScale(End);
	FLOAT HalfHeight = Scale*0.5f*Length;

	// bools indicate if line enters different areas of sphyl
	UBOOL doTest[3] = {0, 0, 0};
	if(LocalStart.Z >= HalfHeight) // start in top
	{
		doTest[0] = 1;

		if(LocalEnd.Z < HalfHeight)
		{
			doTest[1] = 1;

			if(LocalEnd.Z < -HalfHeight)
				doTest[2] = 1;
		}
	}
	else if(LocalStart.Z >= -HalfHeight) // start in middle
	{
		doTest[1] = 1;

		if(LocalEnd.Z >= HalfHeight)
			doTest[0] = 1;

		if(LocalEnd.Z < -HalfHeight)
			doTest[2] = 1;
	}
	else // start at bottom
	{
		doTest[2] = 1;

		if(LocalEnd.Z >= -HalfHeight)
		{
			doTest[1] = 1;

			if(LocalEnd.Z >= HalfHeight)
				doTest[0] = 1;
		}
	}


	// find line in terms of point and unit direction vector
	FVector RayDir = LocalEnd - LocalStart;
	FLOAT RayLength = RayDir.Size();
	FLOAT RecipRayLength = (1.0f/RayLength);
	if(RayLength > KINDA_SMALL_NUMBER)
		RayDir *= RecipRayLength;

	FVector SphereCenter(0);
	FLOAT R = Radius*Scale;
	FLOAT R2 = R*R;

	// Check line against each sphere, then the cylinder Because shape
	// is convex, once we hit something, we dont have to check any more.
	UBOOL bNoHit = 1;
	FCheckResult LocalResult;

	if(doTest[0])
	{
		SphereCenter.Z = HalfHeight;
		bNoHit = SphereLineIntersect(LocalResult, SphereCenter, R, LocalStart, RayDir, RayLength);

		// Discard hit if in cylindrical region.
		if(!bNoHit && LocalResult.Location.Z < HalfHeight)
			bNoHit = 1;
	}

	if(doTest[2] && bNoHit)
	{
		SphereCenter.Z = -HalfHeight;
		bNoHit = SphereLineIntersect(LocalResult, SphereCenter, R, LocalStart, RayDir, RayLength);

		if(!bNoHit && LocalResult.Location.Z > -HalfHeight)
			bNoHit = 1;
	}

	if(doTest[1] && bNoHit)
	{
		// First, check if start is inside cylinder. If so - just return it.
		if( LocalStart.SizeSquared2D() <= R2 && LocalStart.Z <= HalfHeight && LocalStart.Z >= -HalfHeight )
		{
			Result.Location = Start;
			Result.Normal = -RayDir;
			Result.Time = 0.0f;

			return 0;
		}
		else // if not.. solve quadratic
		{
			FLOAT A = RayDir.SizeSquared2D();
			FLOAT B = 2*(LocalStart.X*RayDir.X + LocalStart.Y*RayDir.Y);
			FLOAT C = LocalStart.SizeSquared2D() - R2;
			FLOAT disc = B*B - 4*A*C;

			if (disc >= 0 && Abs(A) > KINDA_SMALL_NUMBER*KINDA_SMALL_NUMBER) 
			{
				FLOAT root = appSqrt(disc);
				FLOAT s = (-B-root)/(2*A);
				FLOAT z1 = LocalStart.Z + s*RayDir.Z;

				// if its not before the start of the line, or past its end, or beyond the end of the cylinder, we have a hit!
				if(s > 0 && s < RayLength && z1 <= HalfHeight && z1 >= -HalfHeight)
				{
					LocalResult.Time = s * RecipRayLength;

					LocalResult.Location.X = LocalStart.X + s*RayDir.X;
					LocalResult.Location.Y = LocalStart.Y + s*RayDir.Y;
					LocalResult.Location.Z = z1;

					LocalResult.Normal.X = LocalResult.Location.X;
					LocalResult.Normal.Y = LocalResult.Location.Y;
					LocalResult.Normal.Z = 0;
					LocalResult.Normal.Normalize();

					bNoHit = 0;
				}
			}
		}
	}

	// If we didn't hit anything - return
	if(bNoHit)
		return 1;

	Result.Location = Matrix.TransformFVector(LocalResult.Location);
	Result.Normal = Matrix.TransformNormal(LocalResult.Normal);
	Result.Time = LocalResult.Time;

	// the physical material is set up higher in the calling chain
    // @see UPhysicsAsset::LineCheck
	
	return FALSE;

}

///////////////////////////////////////
///////////// FKConvexElem ////////////
///////////////////////////////////////

/** Reset the hull to empty all arrays */
void FKConvexElem::Reset()
{
	VertexData.Empty();
	PermutedVertexData.Empty();
	FaceTriData.Empty();
	EdgeDirections.Empty();
	FaceNormalDirections.Empty();
	FacePlaneData.Empty();
	ElemBox.Init();
}

FBox FKConvexElem::CalcAABB(const FMatrix& BoneTM, const FVector& Scale3D)
{
	const FMatrix LocalToWorld = FScaleMatrix(Scale3D) * BoneTM;
	return ElemBox.TransformBy(LocalToWorld);
}


/** What is considered 'too close' for a check co-planar to a separating plane. */
static const FLOAT ConvexSweptBox_ParallelRegion = 0.01f;

/** Wraps the state used by an individual triangle versus swept box separating axis check. */
class FConvexSweptBoxSeparatingAxisCheck
{
public:

	/** Indicates that the line check has become too close and parallel to a separating plane. */
	UBOOL bCloseAndParallel;

	/** If bCloseAndParallel is TRUE, this is the normal of the close feature. */
	FVector CloseFeatureNormal;

	/** Default constructor. */
	FConvexSweptBoxSeparatingAxisCheck()
	:	bCloseAndParallel(FALSE)
	,	CloseFeatureNormal(0,0,0)
	{
	}

	UBOOL TestConvexSweptBoxSeparatingAxis(
									const TArray<FVector>& ConvexVerts,
									const TArray<FPlane>& PermutedConvexVerts,
									const FVector& Line,
									FLOAT ProjectedStart,
									FLOAT ProjectedEnd,
									FLOAT ProjectedExtent,
									FLOAT& MinIntersectTime,
									FLOAT& MaxIntersectTime,
									FVector& HitNormal,
									FVector& ExitDir)
	{
	#if __HAS_SSE__ && ENABLE_VECTORINTRINSICS // SSE version is ~30% faster

		FLOAT ConvexMin;
		FLOAT ConvexMax;
		// Make sure we have the right number of permuted verts	(3 planes for every 4 verts)
		checkSlow(PermutedConvexVerts.Num() % 3 == 0);

		// Splat the line components across the vectors
		VectorRegister LineX = VectorLoad(&Line);
		VectorRegister LineY = VectorReplicate(LineX, 1);
		VectorRegister LineZ = VectorReplicate(LineX, 2);
		LineX = VectorReplicate( LineX, 0 );

		// Since we are moving straight through get a pointer to the data
		const FPlane* RESTRICT PermutedConvexVertsPtr = (FPlane*)PermutedConvexVerts.GetData();

		// Process four planes at a time until we have < 4 left
		VectorRegister VConvexMin = VBigNumber;
		VectorRegister VConvexMax = VNegBigNumber;
		for (INT Count = 0; Count < PermutedConvexVerts.Num(); Count += 3)
		{
			// Load 4 verts that are in all Xs, Ys, Zs form
			VectorRegister VertsX = VectorLoadAligned( PermutedConvexVertsPtr );
			PermutedConvexVertsPtr++;
			VectorRegister VertsY = VectorLoadAligned( PermutedConvexVertsPtr );
			PermutedConvexVertsPtr++;
			VectorRegister VertsZ = VectorLoadAligned( PermutedConvexVertsPtr );
			PermutedConvexVertsPtr++;
			// Calculate the dot products
			VectorRegister Dot = VectorMultiplyAdd( LineZ, VertsZ, VectorMultiplyAdd( LineY, VertsY, VectorMultiply( LineX, VertsX ) ) );
			// Do the Mins/Maxes of each components
			VConvexMin = VectorMin(Dot,VConvexMin);
			VConvexMax = VectorMax(Dot,VConvexMax);
		}
		// We now have 4 min/max values and need to get that to one
		// Shuffle the data so we can do a horizontal min
		VectorRegister MinY = VectorReplicate( VConvexMin, 1 );
		VectorRegister MinZ = VectorReplicate( VConvexMin, 2 );
		VectorRegister MinW = VectorReplicate( VConvexMin, 3 );
		VConvexMin = VectorMin( VConvexMin, MinY );
		VConvexMin = VectorMin( VConvexMin, MinZ );
		VConvexMin = VectorMin( VConvexMin, MinW );
		// Shuffle the data so we can do a horizontal max
		VectorRegister MaxY = VectorReplicate( VConvexMax, 1 );
		VectorRegister MaxZ = VectorReplicate( VConvexMax, 2 );
		VectorRegister MaxW = VectorReplicate( VConvexMax, 3 );
		VConvexMax = VectorMax( VConvexMax, MaxY );
		VConvexMax = VectorMax( VConvexMax, MaxZ );
		VConvexMax = VectorMax( VConvexMax, MaxW );
		// Store the result
		VectorStoreFloat1( VConvexMin, &ConvexMin );
		VectorStoreFloat1( VConvexMax, &ConvexMax );

	#else // NOTE: PPC base consoles did not benefit from SIMD for this

		// Project each vertex of the convex hull along the test axis, and find min and max.
		FLOAT ConvexMin = BIG_NUMBER;
		FLOAT ConvexMax = -BIG_NUMBER;
		for(INT i=0; i<ConvexVerts.Num(); i++)
		{
			const FLOAT ProjectedVert = Line | ConvexVerts(i);
			ConvexMin = Min(ProjectedVert, ConvexMin);
			ConvexMax = Max(ProjectedVert, ConvexMax);
		}

	#endif

		// Expand by the extent of the swept box - projected along the test axis.
		ConvexMin -= ProjectedExtent;
		ConvexMax += ProjectedExtent;

		// This gives the component of the check vector in the test axis.
		const FLOAT ProjectedDirection = ProjectedEnd - ProjectedStart;

		// Nearly parallel - dangerous condition - see if we are close (in 'danger region') and bail out.
		// We set time to zero to ensure this is the hit normal that is used at the end of the axis testing, 
		// and modified (see code in TestConvexSweptBoxSeparatingAxis below).
		const FLOAT ProjDirMag = Abs(ProjectedDirection);
		if( ProjDirMag < 0.01f )
		{
			if( ProjectedStart < ConvexMin && ProjectedStart > (ConvexMin - BoxSweptBox_ParallelRegion) )
			{
				CloseFeatureNormal = -Line;
				bCloseAndParallel = TRUE;
			}
				
			if( ProjectedStart > ConvexMax && ProjectedStart < (ConvexMax + BoxSweptBox_ParallelRegion) )
			{
				CloseFeatureNormal = Line;
				bCloseAndParallel = TRUE;
			}

			// If zero - check vector is perp to test axis, so just check if we start outside. If so, we can't collide.
			if( ProjDirMag < SMALL_NUMBER )
			{
				if( ProjectedStart < ConvexMin ||
					ProjectedStart > ConvexMax )
				{
					return FALSE;
				}
				else
				{
					return TRUE;
				}
			}
		}

		const FLOAT OneOverProjectedDirection = 1.f / ProjectedDirection;
		// Moving in a positive direction - enter ConvexMin and exit ConvexMax
		FLOAT EntryTime, ExitTime;
		FVector ImpactNormal;
		if(ProjectedDirection > 0.f)
		{
			EntryTime = (ConvexMin - ProjectedStart) * OneOverProjectedDirection;
			ExitTime = (ConvexMax - ProjectedStart) * OneOverProjectedDirection;
			ImpactNormal = -Line;
		}
		// Moving in a negative direction - enter ConvexMax and exit ConvexMin
		else
		{
			EntryTime = (ConvexMax - ProjectedStart) * OneOverProjectedDirection;
			ExitTime = (ConvexMin - ProjectedStart) * OneOverProjectedDirection;
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

	UBOOL TestConvexSweptBoxSeparatingAxis(
									const TArray<FVector>& ConvexVerts,
									const TArray<FPlane>& PermutedConvexVerts,
									const FVector& Line,
									const FVector& Start,
									const FVector& End,
									const FVector& BoxX,
									const FVector& BoxY,
									const FVector& BoxZ,
									const FVector& BoxExtent,
									FLOAT& MinIntersectTime,
									FLOAT& MaxIntersectTime,
									FVector& HitNormal,
									FVector& ExitDir)
	{
		// Discard any test axes that are zero (eg cross product of 2 parallel edges)
		if(Line.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			return TRUE;
		}

		// Calculate box projected along test axis.
		const FLOAT ProjectedExtent = BoxExtent.X * Abs(Line | BoxX) + BoxExtent.Y * Abs(Line | BoxY) + BoxExtent.Z * Abs(Line | BoxZ);

		return TestConvexSweptBoxSeparatingAxis(ConvexVerts, PermutedConvexVerts, Line, Line | Start, Line | End, ProjectedExtent, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir);
	}
};

UBOOL FKConvexElem::LineCheck(FCheckResult& Result, const FMatrix& WorldToBox,  const FVector& LocalEnd, const FVector& LocalStart, const FVector& BoxExtent, UBOOL bSkipCloseAndParallelChecks) const
{
	// If no face data - can't check.
	if(FaceTriData.Num() == 0 || FacePlaneData.Num() == 0)
	{
		return TRUE;
	}

	const INT NumTris = FaceTriData.Num()/3;

	// Line check case
	if(BoxExtent.IsZero())
	{
		// Calc direction vector of ray and ray length.
		const FVector LocalDir = LocalEnd - LocalStart;

		FLOAT TNear = -BIG_NUMBER;
		FLOAT TFar = BIG_NUMBER;
		FVector HitPlaneNormal(0.f,0.f,0.f);

		for(INT i=0; i<FacePlaneData.Num(); i++)
		{
			const FPlane& P = FacePlaneData(i);
			const FLOAT RayDotPlaneNorm = FVector(P) | LocalDir;
			const FLOAT StartPlaneDist = P.PlaneDot(LocalStart);

			// If ray is parallel to face - we check if it starts 'outside' the face
			// If so, we cannot collide.
			if( Abs(RayDotPlaneNorm) < SMALL_NUMBER )
			{
				if(StartPlaneDist > 0.f)
				{
					return TRUE;
				}
			}
			else
			{
				// Time to hit plane - 0.0 is start and 1.0 is end
				const FLOAT THit = -StartPlaneDist / RayDotPlaneNorm;

				// Hitting back of this face - update TFar (exit) time if THit is closer.
				if( RayDotPlaneNorm > 0.f )
				{
					TFar = Min(TFar, THit);
				}
				// Hitting front of this face - update TNear (entry) time if THit is further.
				else
				{
					if(THit > TNear)
					{
						TNear = THit;
						HitPlaneNormal = P;
					}
				}

				// If Far ever overlaps Near then we missed the hull.
				if(TNear > TFar)
				{
					return TRUE;
				}

				// If our 'exit' is ever behind the line - the hull is behind us, so fail.
				if(TFar < 0.f)
				{
					return TRUE;
				}
			}	
		}

		// If hit is beyond end of ray, no collision occurs.
		if(TNear > Result.Time)
		{
			return TRUE;
		}

		// If TNear is negative, we started the line check _inside_ the hull.
		if(TNear < 0.f)
		{
			Result.Time = 0.f;
			Result.Normal = -LocalDir.SafeNormal();

			if(Result.Normal.SizeSquared() < SMALL_NUMBER)
			{
				// If the check was zero length, we just fall back to a vertical up normal when penetrating.
				Result.Normal = FVector(0.f,0.f,1.f);
			}
		}
		// Normal hit - fill in info.
		else
		{
			check(!HitPlaneNormal.IsZero()); // Check we have a valid normal
			Result.Time = TNear;
			Result.Normal = HitPlaneNormal;
		}

		return FALSE;
	}
	// Swept box check case
	else
	{
		const FVector BoxX = WorldToBox.GetAxis(0);
		const FVector BoxY = WorldToBox.GetAxis(1);
		const FVector BoxZ = WorldToBox.GetAxis(2);

		FLOAT MinIntersectTime = -BIG_NUMBER;
		FLOAT MaxIntersectTime = BIG_NUMBER;
		FVector HitNormal(0,0,0), ExitDir;

		FConvexSweptBoxSeparatingAxisCheck Check;

		// Box faces
		if(!Check.TestConvexSweptBoxSeparatingAxis(VertexData, PermutedVertexData, BoxX ^ BoxY, LocalStart, LocalEnd, BoxX, BoxY, BoxZ, BoxExtent, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
			return TRUE;

		if(!Check.TestConvexSweptBoxSeparatingAxis(VertexData, PermutedVertexData, BoxY ^ BoxZ, LocalStart, LocalEnd, BoxX, BoxY, BoxZ, BoxExtent, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
			return TRUE;

		if(!Check.TestConvexSweptBoxSeparatingAxis(VertexData, PermutedVertexData, BoxZ ^ BoxX, LocalStart, LocalEnd, BoxX, BoxY, BoxZ, BoxExtent, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
			return TRUE;


		// Convex faces
		for(INT i=0; i<FaceNormalDirections.Num(); i++)
		{
			if(!Check.TestConvexSweptBoxSeparatingAxis(VertexData, PermutedVertexData, FaceNormalDirections(i), LocalStart, LocalEnd, BoxX, BoxY, BoxZ, BoxExtent, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
				return TRUE;
		}

		// Each convex edges crossed with each box edge
		for(INT i=0; i<EdgeDirections.Num(); i++)
		{
			if(!Check.TestConvexSweptBoxSeparatingAxis(VertexData, PermutedVertexData, EdgeDirections(i) ^ BoxX, LocalStart, LocalEnd, BoxX, BoxY, BoxZ, BoxExtent, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
				return TRUE;

			if(!Check.TestConvexSweptBoxSeparatingAxis(VertexData, PermutedVertexData, EdgeDirections(i) ^ BoxY, LocalStart, LocalEnd, BoxX, BoxY, BoxZ, BoxExtent, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
				return TRUE;

			if(!Check.TestConvexSweptBoxSeparatingAxis(VertexData, PermutedVertexData, EdgeDirections(i) ^ BoxZ, LocalStart, LocalEnd, BoxX, BoxY, BoxZ, BoxExtent, MinIntersectTime, MaxIntersectTime, HitNormal, ExitDir))
				return TRUE;
		}

		// If we are close and parallel, and there is not another axis which provides a good separation.
		if(Check.bCloseAndParallel && !(MinIntersectTime > Result.Time && Check.CloseFeatureNormal != HitNormal && Check.CloseFeatureNormal != -HitNormal) && !bSkipCloseAndParallelChecks)
		{
			// If in danger, disallow movement, in case it puts us inside the object.
			Result.Time = 0.f;

			// Tilt the normal slightly back towards the direction of travel. 
			// This will cause the next movement to be away from the surface (hopefully out of the 'danger region')
			FVector CheckDir = (LocalEnd - LocalStart).SafeNormal();
			Result.Normal = (Check.CloseFeatureNormal.SafeNormal() - (0.05f * CheckDir)).SafeNormal();

			// Say we hit.
			return FALSE;
		}

		// If hit is after end of ray, there is no collision.
		if(MinIntersectTime > Result.Time)
		{
			return TRUE;
		}

		// If we reach this point some collision occurs

		// If hit time is positive- we start outside and hit the hull
		if(MinIntersectTime >= 0.0f)
		{
			Result.Time = MinIntersectTime;
			Result.Normal = HitNormal;
		}
		// If hit time is negative- the entry is behind the start - so we started colliding
		else
		{
			FVector LineDir = (LocalEnd - LocalStart).SafeNormal();
			ExitDir = ExitDir.SafeNormal();

			// If the exit surface in front is closer than the exit surface behind us, 
			// and we are moving towards the exit surface in front
			// allow movement (report no hit) to let us exit this hull.
			if( (MaxIntersectTime < -MinIntersectTime) && (LineDir | ExitDir) > 0.0f)
			{
				Result.Time = 1.f;
				return TRUE;
			}
			else
			{
				Result.Time = 0.f;
				Result.bStartPenetrating = TRUE;

				// Use a vector pointing back along check vector as the hit normal
				Result.Normal = -LineDir;

#if !FINAL_RELEASE
				if(Result.Normal.SizeSquared() < SMALL_NUMBER)
				{
					debugfSuppressed(NAME_DevCollision, TEXT("FKConvexElem::LineCheck (NZE): Starting penetrating but zero-length line check - zero normal returned.") );
				}
#endif
			}
		}

		return FALSE;
	}
}

/** Check a convex against static box for overlap. */
static UBOOL TestConvexBoxSeparatingAxis(
	const TArray<FVector>& ConvexVerts,
	const FVector& Line,
	const FLOAT ProjectedPoint,
	const FLOAT ProjectedExtent,
	FLOAT& MinPenetration,
	FVector& HitNormal)
{
	// Project each vertex of the convex hull along the test axis, and find min and max.
	FLOAT ConvexMin = BIG_NUMBER;
	FLOAT ConvexMax = -BIG_NUMBER;
	for(INT i=0; i<ConvexVerts.Num(); i++)
	{
		const FLOAT ProjectedVert = Line | ConvexVerts(i);
		ConvexMin = Min(ProjectedVert, ConvexMin);
		ConvexMax = Max(ProjectedVert, ConvexMax);
	}

	// Expand by the extent of the swept box - projected along the test axis.
	ConvexMin -= ProjectedExtent;
	ConvexMax += ProjectedExtent;

	// If we have an intersection along this axis - take note of the min penetration if its the lowest so far.
	if(ProjectedPoint >= ConvexMin && ProjectedPoint <= ConvexMax)
	{
		// Use inverse sqrt because that is fast and we do more math with the inverse value anyway
		const FLOAT InvAxisMagnitude = appInvSqrt(Line.X * Line.X +	Line.Y * Line.Y + Line.Z * Line.Z);
		const FLOAT ScaledBestDist = MinPenetration / InvAxisMagnitude;
		FLOAT MinPenetrationDist = ProjectedPoint - ConvexMin;
		FLOAT MaxPenetrationDist = ConvexMax - ProjectedPoint;

		if(MinPenetrationDist < ScaledBestDist)
		{
			MinPenetration = MinPenetrationDist * InvAxisMagnitude;
			HitNormal = -Line * InvAxisMagnitude;
		}
		if(MaxPenetrationDist < ScaledBestDist)
		{
			MinPenetration = MaxPenetrationDist * InvAxisMagnitude;
			HitNormal = Line * InvAxisMagnitude;
		}

		return TRUE;
	}
	// No overlap - we cannot have a collision.
	else
	{
		return FALSE;
	}
}

inline UBOOL TestConvexBoxSeparatingAxis(
	const TArray<FVector>& ConvexVerts,
	const FVector& Line,
	const FVector& Point,
	const FVector& BoxX,
	const FVector& BoxY,
	const FVector& BoxZ,
	const FVector& BoxExtent,
	FLOAT& MinPenetration,
	FVector& HitNormal)
{
	// Discard any test axes that are zero (eg cross product of 2 parallel edges)
	if(Line.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return TRUE;
	}

	// Calculate box projected along test axis.
	const FLOAT ProjectedExtent = BoxExtent.X * Abs(Line | BoxX) + BoxExtent.Y * Abs(Line | BoxY) + BoxExtent.Z * Abs(Line | BoxZ);

	return TestConvexBoxSeparatingAxis(ConvexVerts, Line, Line | Point, ProjectedExtent, MinPenetration, HitNormal);
}

/** 
 *	Returns results in LOCAL space. 
 *	Does not fill in Location field.
 */
UBOOL FKConvexElem::PointCheck(FCheckResult& Result, FLOAT& OutBestDistance, const FMatrix& WorldToBox, const FVector& LocalLocation, const FVector& BoxExtent) const
{
	const FVector BoxX = WorldToBox.GetAxis(0);
	const FVector BoxY = WorldToBox.GetAxis(1);
	const FVector BoxZ = WorldToBox.GetAxis(2);

	FLOAT MinPenetration = BIG_NUMBER;
	FVector HitNormal;

	// Box faces
	if(!TestConvexBoxSeparatingAxis(VertexData, BoxX ^ BoxY, LocalLocation, BoxX, BoxY, BoxZ, BoxExtent, MinPenetration, HitNormal))
		return TRUE;

	if(!TestConvexBoxSeparatingAxis(VertexData, BoxY ^ BoxZ, LocalLocation, BoxX, BoxY, BoxZ, BoxExtent, MinPenetration, HitNormal))
		return TRUE;

	if(!TestConvexBoxSeparatingAxis(VertexData, BoxZ ^ BoxX, LocalLocation, BoxX, BoxY, BoxZ, BoxExtent, MinPenetration, HitNormal))
		return TRUE;


	// Convex faces
	for(INT i=0; i<FaceNormalDirections.Num(); i++)
	{
		if(!TestConvexBoxSeparatingAxis(VertexData, FaceNormalDirections(i), LocalLocation, BoxX, BoxY, BoxZ, BoxExtent, MinPenetration, HitNormal))
			return TRUE;
	}

	// Each convex edges crossed with each box edge
	for(INT i=0; i<EdgeDirections.Num(); i++)
	{
		if(!TestConvexBoxSeparatingAxis(VertexData, EdgeDirections(i) ^ BoxX, LocalLocation, BoxX, BoxY, BoxZ, BoxExtent, MinPenetration, HitNormal))
			return TRUE;

		if(!TestConvexBoxSeparatingAxis(VertexData, EdgeDirections(i) ^ BoxY, LocalLocation, BoxX, BoxY, BoxZ, BoxExtent, MinPenetration, HitNormal))
			return TRUE;

		if(!TestConvexBoxSeparatingAxis(VertexData, EdgeDirections(i) ^ BoxZ, LocalLocation, BoxX, BoxY, BoxZ, BoxExtent, MinPenetration, HitNormal))
			return TRUE;
	}

	// If we reach this point some collision occurs

	Result.Normal = HitNormal;
	OutBestDistance = MinPenetration;

	return FALSE;
}

/** 
 *	Utility that determines if a point is completely contained within this convex hull. 
 *	If it returns TRUE (the point is within the hull), will also give you the normal of the nearest surface, and distance from it.
 */
UBOOL FKConvexElem::PointIsWithin(const FVector& LocalLocation, FVector& OutLocalNormal, FLOAT& OutBestDistance) const
{
	// Easy early out if no planes
	if(FacePlaneData.Num() == 0)
	{
		return FALSE;
	}

	// See if point is 'inside' all planes.
	FLOAT BestDist = BIG_NUMBER;
	FVector BestNormal(0,0,1);
	for(INT i=0; i<FacePlaneData.Num(); i++)
	{
		const FPlane& Plane = FacePlaneData(i);

		// Calculate distance of point from plane. If positive, point is outside (so not within hull).
		const FLOAT Dist = Plane.PlaneDot(LocalLocation);
		if(Dist > 0.f)
		{
			return FALSE;
		}
		// If within plane (Dist is negative), see if this is the closest plane to us.
		else if(-Dist < BestDist)
		{
			BestDist = -Dist;
			BestNormal = Plane;
		}
	}

	// If we tested against all planes and were never outside - we are within.
	OutBestDistance = BestDist;
	OutLocalNormal = BestNormal;

	return TRUE;
}

/** Returns TRUE if this convex hull is entirely above the supplied plane. */
UBOOL FKConvexElem::IsOutsidePlane(const FPlane& Plane)
{
	for(INT i=0; i<VertexData.Num(); i++)
	{
		// Calculate distance of each vertex from plane. 
		// If negative, point is behind plane, so return FALSE.
		const FLOAT Dist = Plane.PlaneDot(VertexData(i));
		if(Dist < 0.f)
		{
			return FALSE;
		}
	}

	// No vertex behind plane - all in front - its outside.
	return TRUE;
}

static const FLOAT DIST_COMPARE_THRESH = 0.1f;
static const FLOAT DIR_COMPARE_THRESH = 0.0003f; // about 1 degree

/** Add NewPlane to the Planes array if it is not there already. */
static void AddPlaneIfNotPresent(TArray<FPlane>& Planes, const FPlane& NewPlane)
{
	// See if plane is already in array.
	for(INT i=0; i<Planes.Num(); i++)
	{
		const FPlane& TestPlane = Planes(i);
		const FLOAT NormalDot = (TestPlane.X * NewPlane.X) + (TestPlane.Y * NewPlane.Y) + (TestPlane.Z * NewPlane.Z);

		// If plane normals are parallel and distances are also parallel.
		if( Abs(NormalDot - 1.f) < DIR_COMPARE_THRESH && 
			Abs(TestPlane.W - NewPlane.W) < DIST_COMPARE_THRESH )
		{
			// It's there - return and do nothing more.
			return;
		}
	}

	// We did not find it so add it now.
	Planes.AddItem(NewPlane);
}

/** Add NewDir to the Dirs array if it (or its negative version) is not already there. */
static void AddDirIfNotPresent(TArray<FVector>& Dirs, const FVector& NewDir)
{
	// See if vector is already in array.
	for(INT i=0; i<Dirs.Num(); i++)
	{
		const FVector& TestDir = Dirs(i);
		const FLOAT DirDot = Abs(TestDir | NewDir); // Abs here to compare against +/-ve versions.

		// If vector directions are parallel
		if( Abs(DirDot - 1.f) < DIR_COMPARE_THRESH )
		{
			// It's there - return and do nothing more.
			return;
		}
	}

	// We did not find it so add it now.
	Dirs.AddItem(NewDir);
}

static UBOOL TriHasEdge(INT T0, INT T1, INT T2, INT Edge0, INT Edge1)
{
	if(	(T0 == Edge0 && T1 == Edge1) || (T1 == Edge0 && T0 == Edge1) || 
		(T0 == Edge0 && T2 == Edge1) || (T2 == Edge0 && T0 == Edge1) || 
		(T1 == Edge0 && T2 == Edge1) || (T2 == Edge0 && T1 == Edge1) )
	{
		return TRUE;
	}

	return FALSE;
}

static void GetTriIndicesUsingEdge(const INT Edge0, const INT Edge1, const TArray<INT> TriData, INT& Tri0Index, INT& Tri1Index)
{
	Tri0Index = INDEX_NONE;
	Tri1Index = INDEX_NONE;

	const INT NumTris = TriData.Num()/3;

	// Iterate over triangles, looking for ones that contain this edge.
	for(INT i=0; i<NumTris; i++)
	{
		if( TriHasEdge(TriData((i*3)+0), TriData((i*3)+1), TriData((i*3)+2), Edge0, Edge1) )
		{
			if(Tri0Index == INDEX_NONE)
			{
				Tri0Index = i;
			}
			else if(Tri1Index == INDEX_NONE)
			{
				Tri1Index = i;
			}
			else
			{
				debugf( TEXT("GetTriIndicesUsingEdge : 3 tris share an edge.") );
			}
		}
	}
}

static void AddEdgeIfNotPresent(TArray<INT>& Edges, INT Edge0, INT Edge1)
{
	const INT NumEdges = Edges.Num()/2;

	// See if this edge is already present.
	for(INT i=0; i<NumEdges; i++)
	{
		INT TestEdge0 = Edges((i*2)+0);
		INT TestEdge1 = Edges((i*2)+1);

		if( (TestEdge0 == Edge0 && TestEdge1 == Edge1) ||
			(TestEdge1 == Edge0 && TestEdge0 == Edge1) )
		{
			// It is - do nothing
			return;
		}
	}

	Edges.AddItem(Edge0);
	Edges.AddItem(Edge1);
}

/** 
 *	Process the VertexData array to generate other data that is needed. Will modify existing VertexData. 
 *	Returns FALSE if failed - you probably want to remove this convex at that point.
 */
UBOOL FKConvexElem::GenerateHullData()
{
	// First- clear any existing data.
	FaceTriData.Empty();
	EdgeDirections.Empty();
	FaceNormalDirections.Empty();
	FacePlaneData.Empty();
	ElemBox.Init();

#if WITH_NOVODEX && WITH_PHYSX_COOKING
	// First scale verts stored into physics scale
	// Do this so any tolerances in PhysX are about right (welding verts etc).
	TArray<FVector> PhysScaleConvexVerts;
	PhysScaleConvexVerts.Add( VertexData.Num() );
	for(INT j=0; j<VertexData.Num(); j++)
	{
		PhysScaleConvexVerts(j) = VertexData(j) * U2PScale;
	}	

	// Clean up verts, and see if they look like a good hull.
	const UBOOL bIsValid = EnsureHullIsValid(PhysScaleConvexVerts);
	if(!bIsValid)
	{
		return FALSE;
	}

	// Create mesh. Will compute convex hull of supplied verts.
	NxConvexMeshDesc ConvexMeshDesc;
	ConvexMeshDesc.numVertices = PhysScaleConvexVerts.Num();
	ConvexMeshDesc.pointStrideBytes = sizeof(FVector);
	ConvexMeshDesc.points = PhysScaleConvexVerts.GetData();
	ConvexMeshDesc.flags = NX_CF_COMPUTE_CONVEX;

	TArray<BYTE> TempBuffer;
	FNxMemoryBuffer Buffer( &TempBuffer );
	const UBOOL bSuccess = GNovodexCooking->NxCookConvexMesh(ConvexMeshDesc, Buffer);
	if(bSuccess)
	{
		NxConvexMesh* TempMesh = GNovodexSDK->createConvexMesh(Buffer);
		if(TempMesh)
		{
			// Extract vertex info from mesh
			INT VertCount = TempMesh->getCount(0,NX_ARRAY_VERTICES);    

			const void* VertBase = TempMesh->getBase(0,NX_ARRAY_VERTICES);    

			NxU32 VertStride = TempMesh->getStride(0,NX_ARRAY_VERTICES);
			check(VertStride == sizeof(FVector));

			// Copy verts out, whilst calculating centroid.
			VertexData.Empty(VertCount);
			VertexData.Add(VertCount);
			for(INT i=0; i<VertCount; i++)
			{
				// Copy vertex from the mesh, and scale back to Unreal scale
				FVector* V = ((FVector*)VertBase) + i;
				VertexData(i) = (*V) * P2UScale;
			
				// Update bounding box
				ElemBox += VertexData(i);
			}

			// Get index information
			const INT TriCount = TempMesh->getCount(0,NX_ARRAY_TRIANGLES);
			check(TriCount > 0);

			const NxInternalFormat IndexFormat = TempMesh->getFormat(0,NX_ARRAY_TRIANGLES);
			check(IndexFormat == NX_FORMAT_INT);

			INT* IndexBase = (INT*)(TempMesh->getBase(0,NX_ARRAY_TRIANGLES));

			NxU32 IndexStride = TempMesh->getStride(0,NX_ARRAY_TRIANGLES);
			check(IndexStride == sizeof(INT) * 3);

			TArray<INT>	EdgeStore; // List of all unique edges in the mesh
			TArray<FVector> TriNormals; // Normal of each triangle in the mesh (size == TriCount).

			// Copy indices out to make index buffer.
			for(INT i=0; i<TriCount; i++)
			{
				const INT I0 = IndexBase[(i*3)+0];
				const INT I1 = IndexBase[(i*3)+2];
				const INT I2 = IndexBase[(i*3)+1];

				FVector& V0 = VertexData(I0);
				FVector& V1 = VertexData(I1);
				FVector& V2 = VertexData(I2);

				// Add to triangle store
				FaceTriData.AddItem(I0);
				FaceTriData.AddItem(I1);
				FaceTriData.AddItem(I2);

				// Add edge to store - we cull these later.
				AddEdgeIfNotPresent(EdgeStore, I0, I1);
				AddEdgeIfNotPresent(EdgeStore, I1, I2);
				AddEdgeIfNotPresent(EdgeStore, I2, I0);

				// Add face normal to lists.
				FVector TriNormal = ((V2-V0)^(V1-V0)).SafeNormal();
				TriNormals.AddItem(TriNormal);
				AddDirIfNotPresent( FaceNormalDirections, TriNormal );

				// Add face plane to list.
				AddPlaneIfNotPresent(FacePlaneData, FPlane(V0, TriNormal));
			}

			// Now we pick out the edges that are real 'edges' and not within a face.
			// We look at the normals of the triangles on either side of an edge.
			const INT NumEdges = EdgeStore.Num()/2;
			for(INT i=0; i<NumEdges; i++)
			{
				const INT TestEdge0 = EdgeStore((i*2)+0);
				const INT TestEdge1 = EdgeStore((i*2)+1);

				INT Tri0, Tri1;
				GetTriIndicesUsingEdge(TestEdge0, TestEdge1, FaceTriData, Tri0, Tri1);
				if(Tri0 == INDEX_NONE || Tri1 == INDEX_NONE)
				{
					debugf( TEXT("FKConvexElem::GenerateHullData : Open mesh.") );
				}
				else
				{
					const FVector Tri0Normal = TriNormals(Tri0);
					const FVector Tri1Normal = TriNormals(Tri1);
					const FLOAT FaceDot = Tri0Normal | Tri1Normal;

					// If faces are not parallel so this is not an edge internal to a poly face. Add edge to list.
					if( FaceDot < (1.f - DIR_COMPARE_THRESH) )
					{
						FVector EdgeDir = (VertexData(TestEdge0) - VertexData(TestEdge1)).SafeNormal();
						AddDirIfNotPresent(EdgeDirections, EdgeDir);
					}
				}
			}

			GNovodexSDK->releaseConvexMesh(*TempMesh);

			// Build a SIMD version of the vertex data
			PermuteVertexData();
		}
		// Could not create the PhysX mesh - we don't want this hull
		else
		{
			return FALSE;
		}
	}
	// PhysX cooking failed - we don't want this hull
	else
	{
		return FALSE;
	}
#endif // WITH_NOVODEX && WITH_PHYSX_COOKING

	// Convex hull is OK
	return TRUE;
}

/**
 * Creates a copy of the vertex data in SIMD ready form
 */
void FKConvexElem::PermuteVertexData(void)
{
	// Figure out how many verts will be left over
	const INT NumRemaining = VertexData.Num() % 4;
	// Going to do 4 verts at a time
	const INT NumToAdd = ((VertexData.Num() / 4) * 3);
	// The number of verts to process as planes without duplicating verts
	const INT NumToProcess = VertexData.Num() - NumRemaining;
	// Presize the array
	PermutedVertexData.Empty(NumToAdd + (NumRemaining ? 3 : 0));
	// For each set of four verts
	for (INT Count = 0; Count < NumToProcess; Count += 4)
	{
		// Add them in SIMD ready form
		new(PermutedVertexData)FPlane(VertexData(Count + 0).X,VertexData(Count + 1).X,VertexData(Count + 2).X,VertexData(Count + 3).X);
		new(PermutedVertexData)FPlane(VertexData(Count + 0).Y,VertexData(Count + 1).Y,VertexData(Count + 2).Y,VertexData(Count + 3).Y);
		new(PermutedVertexData)FPlane(VertexData(Count + 0).Z,VertexData(Count + 1).Z,VertexData(Count + 2).Z,VertexData(Count + 3).Z);
	}
	// Pad the last set so we have an even 4 planes of vert data
	if (NumRemaining)
	{
		FVector Last1, Last2, Last3, Last4;
		// Read the last set of verts
		switch (NumRemaining)
		{
			case 3:
			{
				Last1 = VertexData(NumToProcess + 0);
				Last2 = VertexData(NumToProcess + 1);
				Last3 = VertexData(NumToProcess + 2);
				Last4 = Last1;
				break;
			}
			case 2:
			{
				Last1 = VertexData(NumToProcess + 0);
				Last2 = VertexData(NumToProcess + 1);
				Last3 = Last4 = Last1;
				break;
			}
			case 1:
			{
				Last1 = VertexData(NumToProcess + 0);
				Last2 = Last3 = Last4 = Last1;
				break;
			}
			default:
			{
				Last1 = FVector(0.0f, 0.0f, 0.0f);
				Last2 = Last3 = Last4 = Last1;
				break;
			}
		}
		// Add them in SIMD ready form
		new(PermutedVertexData)FPlane(Last1.X,Last2.X,Last3.X,Last4.X);
		new(PermutedVertexData)FPlane(Last1.Y,Last2.Y,Last3.Y,Last4.Y);
		new(PermutedVertexData)FPlane(Last1.Z,Last2.Z,Last3.Z,Last4.Z);
	}
}

/** Cut away the part of this hull that is in front of this plane. */
void FKConvexElem::SliceHull(const FPlane& SlicePlane)
{
	TArray<FVector> SnapVerts = VertexData;
	TArray<FPlane> Planes = FacePlaneData;
	Planes.AddItem(SlicePlane);
	this->HullFromPlanes(Planes, SnapVerts);
}

#define LOCAL_EPS (0.01f)

UBOOL FKConvexElem::HullFromPlanes(const TArray<FPlane>& InPlanes, const TArray<FVector>& SnapVerts)
{
	// Start by clearing this convex.
	Reset();

	FLOAT TotalPolyArea = 0;

	for(INT i=0; i<InPlanes.Num(); i++)
	{
		FPoly Polygon;
		Polygon.Normal = InPlanes(i);

		FVector AxisX, AxisY;
		Polygon.Normal.FindBestAxisVectors(AxisX,AxisY);

		const FVector Base = InPlanes(i) * InPlanes(i).W;

		new(Polygon.Vertices) FVector(Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);
		new(Polygon.Vertices) FVector(Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);
		new(Polygon.Vertices) FVector(Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);
		new(Polygon.Vertices) FVector(Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);

		for(INT j=0; j<InPlanes.Num(); j++)
		{
			if(i != j)
			{
				if(!Polygon.Split(-FVector(InPlanes(j)), InPlanes(j) * InPlanes(j).W))
				{
					Polygon.Vertices.Empty();
					break;
				}
			}
		}

		// Do nothing if poly was completely clipped away.
		if(Polygon.Vertices.Num() > 0)
		{
			TotalPolyArea += Polygon.Area();

			// Add vertices of polygon to convex primitive.
			for(INT j=0; j<Polygon.Vertices.Num(); j++)
			{
				// We try and snap the vert to on of the ones supplied.
				INT NearestVert = INDEX_NONE;
				FLOAT NearestDistSqr = BIG_NUMBER;

				for(INT k=0; k<SnapVerts.Num(); k++)
				{
					const FLOAT DistSquared = (Polygon.Vertices(j) - SnapVerts(k)).SizeSquared();

					if( DistSquared < NearestDistSqr )
					{
						NearestVert = k;
						NearestDistSqr = DistSquared;
					}
				}

				// If we have found a suitably close vertex, use that
				if( NearestVert != INDEX_NONE && NearestDistSqr < LOCAL_EPS )
				{
					const FVector localVert = SnapVerts(NearestVert);
					AddVertexIfNotPresent(VertexData, localVert);
				}
				else
				{
					const FVector localVert = Polygon.Vertices(j);
					AddVertexIfNotPresent(VertexData, localVert);
				}
			}
		}
	}

	// If the collision volume isn't closed, return an error so the model can be discarded
	if(TotalPolyArea < 0.001f)
	{
		debugf( TEXT("Total Polygon Area invalid: %f"), TotalPolyArea );
		return FALSE;
	}

	// We need at least 4 vertices to make a convex hull with non-zero volume.
	// We shouldn't have the same vertex multiple times (using AddVertexIfNotPresent above)
	if(VertexData.Num() < 4)
	{
		return TRUE;
	}

	// Check that not all vertices lie on a line (ie. find plane)
	// Again, this should be a non-zero vector because we shouldn't have duplicate verts.
	UBOOL bFound = FALSE;
	FVector Dir2, Dir1;

	Dir1 = VertexData(1) - VertexData(0);
	Dir1.Normalize();

	for(INT i=2; i<VertexData.Num() && !bFound; i++)
	{
		Dir2 = VertexData(i) - VertexData(0);
		Dir2.Normalize();

		// If line are non-parallel, this vertex forms our plane
		if((Dir1 | Dir2) < (1.f - LOCAL_EPS))
		{
			bFound = TRUE;
		}
	}

	if(!bFound)
	{
		return TRUE;
	}

	// Now we check that not all vertices lie on a plane, by checking at least one lies off the plane we have formed.
	FVector Normal = Dir1 ^ Dir2;
	Normal.Normalize();

	const FPlane Plane(VertexData(0), Normal);

	bFound = FALSE;
	for(INT i=2; i<VertexData.Num() ; i++)
	{
		if(Plane.PlaneDot(VertexData(i)) > LOCAL_EPS)
		{
			bFound = TRUE;
			break;
		}
	}

	// If we did not find a vert off the line - discard this hull.
	if(!bFound)
	{
		return TRUE;
	}

	// Generate face/surface/plane data needed for Unreal collision.
	const UBOOL bHullIsGood = GenerateHullData();

	// We can continue adding primitives (mesh is not horribly broken)
	return TRUE;
}

/** Calculate the surface area and volume. */
void FKConvexElem::CalcSurfaceAreaAndVolume( FLOAT & Area, FLOAT & Volume ) const
{
	Area = 0.0f;
	Volume = 0.0f;

	if( ElemBox.IsValid && VertexData.Num() > 0 )
	{
		FVector Centroid(0,0,0);
		for( INT j = 0; j < VertexData.Num(); ++j )
		{
			Centroid += VertexData(j);
		}
		Centroid *= 1.0f/VertexData.Num();
		for( INT j = 0; j < FaceTriData.Num(); j += 3 )
		{
			FVector V0 = VertexData(FaceTriData(j+2));
			FVector V1 = VertexData(FaceTriData(j+1));
			FVector V2 = VertexData(FaceTriData(j+0));
			Area += ((V1-V0)^(V2-V1)).Size();
			Volume += ((V0-Centroid)^(V1-Centroid))|(V2-Centroid);
		}
	}

	Area *= 0.5f;
	Volume *= 0.166666667f;
}

/*-----------------------------------------------------------------------------
 FSimplex Implementation
-----------------------------------------------------------------------------*/

/*
*  Initialize the simplex
*  @param	Dimensions - dimensionality of this simplex	(NOT IMPLEMENTED YET)
*  @param   GJKHelper - Helper Interface for accessing the convex object representation
*/
void FSimplex::Init(INT Dimensions, IGJKHelper* GJKHelper)
{
	check(Dimensions < 4);

	BaryCoords.Set(1.0f, 0.0f, 0.0f, 0.0f);

	Vertices.Empty();

	//Pick a random direction to seed
	const FSimplexVertex& SeedVertex = GJKHelper->GetSupportingVertex(FVector(0.0f,1.0f,0.0f));
	Vertices.AddItem(SeedVertex);
}

/*
*  Given a point on the simplex, possibly reduce the simplex to the minimum set 
*  of points necessary to describe the given point
*	@param	Point - a point in world coordinates
*/
void FSimplex::Reduce(const FSimplexVertex& Point)
{
	FLOAT T = 0.0f;
	FVector TempAxis;

	FVector4 TempBary(0.0f,0.0f,0.0f,0.0f);
	switch (Vertices.Num() - 1)
	{
	case 0:	 //Point simplex
		//Can't reduce
		TempBary = FVector4(1.0f,0.0f,0.0f,0.0f);
		break;
	case 1:  //Line simplex
		//unless this point is one of the vertices, can't reduce

		//Piecewise subtract to find axis with greatest separation
		TempAxis = Vertices(1).Vertex - Vertices(0).Vertex;

		//Calculate the proper barycentric coordinates for a line segment
		if (TempAxis.X != 0.0f)
		{
			T = (Point.Vertex.X - Vertices(0).Vertex.X) / TempAxis.X;
		}
		else if (TempAxis.Y != 0.0f)
		{
			T = (Point.Vertex.Y - Vertices(0).Vertex.Y) / TempAxis.Y;
		}
		else if (TempAxis.Z != 0.0f)
		{
			T = (Point.Vertex.Z - Vertices(0).Vertex.Z) / TempAxis.Z;
		}
		
		TempBary = FVector4(1.0f - T, T, 0.0f, 0.0f);
		break;
	case 2: //Triangle simplex
		TempBary = FVector4(ComputeBaryCentric2D(Point.Vertex, Vertices(0).Vertex, Vertices(1).Vertex, Vertices(2).Vertex), 0.0f);
		break;
	case 3: //Tetrahedron simplex
		TempBary = ComputeBaryCentric3D(Point.Vertex, Vertices(0).Vertex, Vertices(1).Vertex, Vertices(2).Vertex, Vertices(3).Vertex);
		break;
	default:
		debugf(TEXT("FSimplex::Reduce() - Unexpected dimensionality %d"), Vertices.Num() - 1);
		break;
	}

	//Keep only vertices that are required to describe the given point
	for (INT i=Vertices.Num()-1; i>=0; i--)
	{
		if (TempBary[i] < 0.001f)
		{
			Vertices.Remove(i);
		}
	}

	//Collapse Barycentric coordinates
	INT BaryCount = 0;
	BaryCoords.Set(0.0f, 0.0f, 0.0f, 0.0f);
	for (INT i=0; i<4; i++)
	{
		if (TempBary[i] > 0.001f)
		{
			BaryCoords[BaryCount++] = TempBary[i];
		}
	}
}

/* 
*  Increase the simplex dimension by using this additional point
*	@param	Point - a point in world coordinates
*/
void FSimplex::Increase(const FSimplexVertex& Point)
{
	AddVertexIfNotPresent(Vertices, Point);
}

/*
*  Returns the point on the simplex
*  closest to the given point in world space
*  @param	Point - a point in world coordinates
*/
FSimplexVertex FSimplex::ComputeMinimumNorm(const FVector& Point)
{
	FSimplexVertex Result;

	//Chose the appropriate calculation based on the current dimensionality of the simplex
	switch (Vertices.Num() - 1)
	{
	case 0:
		Result.Vertex = Vertices(0).Vertex;
		break;
	case 1:
		PointDistToSegment(Point, Vertices(0).Vertex, Vertices(1).Vertex, Result.Vertex);
		break;
	case 2:
		Result.Vertex = ClosestPointOnTriangleToPoint(Point, Vertices(0).Vertex, Vertices(1).Vertex, Vertices(2).Vertex);
		break;
	case 3:
		Result.Vertex = ClosestPointOnTetrahedronToPoint(Point, Vertices(0).Vertex, Vertices(1).Vertex, Vertices(2).Vertex, Vertices(3).Vertex);
		break;
	default:
		debugf(TEXT("FSimplex::ComputeMinimumNorm() - Unexpected dimensionality %d"), Vertices.Num() - 1);
		break;
	}

	return Result;
}

/**
* Calculates the closest point on a given convex primitive to a point given
* @param POI - Point in world space to determine closest point to
* @param Primitive - Convex primitive 
* @param OutPoint - Point on primitive closest to POI given 
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
#define GJK_TESTING 0
GJKResult ClosestPointOnConvexPrimitive(const FVector& POI, IGJKHelper* Primitive, FVector& OutPoint)
{
	GJKResult Result = GJK_Fail;

	//Use support mapping in arbitrary direction to seed the first point
	FSimplex SimplexQ;
	// Compute Q from d+1 points from the convex hull (3D -> 4 points needed)
	SimplexQ.Init(3, Primitive);

#if GJK_TESTING
	extern TArray<UGJKRenderingComponent::TestData>* CurTestData;
	UGJKRenderingComponent::TestData SomeTestData;
	if (CurTestData) 
		CurTestData->Empty();
#endif

	// Limit the algorithm
	const INT MaxIters = 20;

	FSimplexVertex P; //Current point closest to POI on current simplex
	FSimplexVertex V; //Current supporting Vertex in direction POI - P

	INT Iter = 0;

	for (Iter=0; Iter<MaxIters; Iter++)
	{
		// Find point P (closest to POI) in convex hull of Q (point on line, point on face, point on point)
		P = SimplexQ.ComputeMinimumNorm(POI);

		// Calculate the direction vector from point on hull toward POI
		FVector OrigDir = POI - P.Vertex;
		FLOAT NegPLen = OrigDir.Size();
		// If P is POI, return 0 and exit
		if (TOL_FUNC(NegPLen, 0.001f, 0.001f, 0.001f))
		{
			Result = GJK_Intersect;
			break;
		}

#if GJK_TESTING
		if (CurTestData)
		{
			//Copy the simplex before reduction
			SomeTestData.SimplexCopy = SimplexQ;
		}
#endif

		// Reduce Q to Q` such that P is still in Q` (barycentric coords to remove)
		SimplexQ.Reduce(P);

		// Find supporting vertex V on original hull in direction from P to POI
		OrigDir.Normalize();
		V = Primitive->GetSupportingVertex(OrigDir);

		// If V no more extreme on vector than P return P
		// (dot(v - p, -p) <= epsilon * max)
		FLOAT VDist = (V.Vertex - P.Vertex) | OrigDir;
		//FLOAT VDist = (POI - V.Vertex) | OrigDir;

#if GJK_TESTING
		if (CurTestData)
		{
			SomeTestData.OrigDir = OrigDir;
			SomeTestData.P = P;
			SomeTestData.V = V;
			SomeTestData.NegPLen = NegPLen;
			SomeTestData.VDist = VDist;
			CurTestData->AddItem(SomeTestData);
		}
#endif

		//if (TOL_FUNC(VDist, NegPLen, 0.001f, 0.001f)) //projection of V on P == P
		if (VDist <= .001f)
		{
			//Found the closest point on Q
			Result = GJK_NoIntersection;
			break;
		}

		// otherwise add V to Q and start again
		SimplexQ.Increase(V);
	}

	//Initialize the output
	OutPoint = POI;
	if (Result != GJK_Intersect)
	{
		OutPoint = P.Vertex;
	}

	if (Iter == MaxIters)
	{
		debugf(TEXT("ClosestPointOnConvexPrimitive() - MaxIters exceeded"));
	}

	return Result;
}

/**
* Calculates the closest point on a given convex primitive to another given convex primitive
* @param PrimitiveA - Convex primitive
* @param PrimitiveB - Convex primitive  
* @param OutPointA - Point on primitive A closest to primitive B
* @param OutPointB - Point on primitive B closest to primitive A
* 
* Note: There seems to be numerous cases where oscillation around the true answer occurs contrary
*       to any papers read on the Internet that would indicate this as a problem.  The code should
*       probably store the best answer so far and use that when the iteration boundary is exceeded
*       but at the moment the "bad" answers seem good enough to not warrant all that extra data copying.
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
static FLOAT GJK_ZERO_TOL = 0.005f;
GJKResult ClosestPointsBetweenConvexPrimitives(IGJKHelper* PrimitiveA, IGJKHelper* PrimitiveB, FVector& OutPointA, FVector& OutPointB)
{
	GJKResult Result = GJK_Fail;

	GJKHelperMinkowski MinHelper(PrimitiveA, PrimitiveB);

	FSimplex SimplexQ;
	//Use support mapping in arbitrary direction to seed the first point
	SimplexQ.Init(3, &MinHelper);

#if GJK_TESTING
	extern TArray<UGJKRenderingComponent::TestData>* CurTestData;
	UGJKRenderingComponent::TestData SomeTestData;
	if (CurTestData) 
		CurTestData->Empty();
#endif

	//Minkowski deals with the origin
	const FVector POI(0.0f,0.0f,0.0f);

	INT Iter = 0;
	const INT MaxIters = 20;

	FSimplexVertex v; //Current point closest to POI on current simplex
	v.Vertex = SimplexQ.Vertices(0).Vertex; 
	FSimplexVertex w; //Current supporting Vertex in direction POI - P

	FLOAT Mu = 0.0f;
	while (Result == GJK_Fail)
	{
		//Get new supporting point in direction of origin
		FVector OrigDir = -(v.Vertex); 
		w = MinHelper.GetSupportingVertex(OrigDir);

		FLOAT Sigma = v.Vertex.SafeNormal() | w.Vertex;
		Mu = Max(Mu, Sigma);

		FLOAT vLen = OrigDir.Size();

		//The closest point is the origin itself so we intersect
		if (vLen < GJK_ZERO_TOL)
		{
			Result = GJK_Intersect;
			break;
		}

#if GJK_TESTING
		if (CurTestData)
		{
			SomeTestData.OrigDir = OrigDir;
			SomeTestData.SimplexCopy = SimplexQ;
			SomeTestData.V = w;
			SomeTestData.P = v;

			SomeTestData.VDist = vLen - Mu;
			SomeTestData.NegPLen = (GJK_ZERO_TOL * vLen);

			for (INT VertIdx=0; VertIdx<SimplexQ.Vertices.Num(); VertIdx++)
			{
				const FSimplexVertex& VertexOnQ = SimplexQ.Vertices(VertIdx);
				SomeTestData.P.VertexInA += SimplexQ.BaryCoords[VertIdx] * VertexOnQ.VertexInA;
				SomeTestData.P.VertexInB += SimplexQ.BaryCoords[VertIdx] * VertexOnQ.VertexInB;
			}

			CurTestData->AddItem(SomeTestData);
		}
#endif

		//If we are within a tolerance of being the most extreme toward the origin then finish
		FLOAT Delta = vLen - Mu;
		Result = (Delta <= (GJK_ZERO_TOL * vLen)) ? GJK_NoIntersection : GJK_Fail;
		if (Result == GJK_Fail)
		{
			SimplexQ.Increase(w); 
			v = SimplexQ.ComputeMinimumNorm(POI);
			SimplexQ.Reduce(v);
		}

		Iter++;
		if (Iter > MaxIters)
		{
			debugf(TEXT("ClosestPointsBetweenConvexPrimitives() - MaxIters exceeded"));
			break;
		}
	}

	//Initialize the output
	OutPointA.Set(0,0,0);
	OutPointB.Set(0,0,0);

	if (Result != GJK_Intersect)
	{
		//Now we have a simplex Q and a point P represented with barycoords
		//Get back the vertices in hulls A,B that contribute to Q and get the points from the barycentric coords
		for (INT VertIdx=0; VertIdx<SimplexQ.Vertices.Num(); VertIdx++)
		{
			const FSimplexVertex& VertexOnQ = SimplexQ.Vertices(VertIdx);
			OutPointA += SimplexQ.BaryCoords[VertIdx] * VertexOnQ.VertexInA;
			OutPointB += SimplexQ.BaryCoords[VertIdx] * VertexOnQ.VertexInB;
		}
	}

	return Result;
}


// Callback for OverlapAABBs
struct AxisRep
{
	FLOAT	Value;
	INT		Index;
};

static INT BoundaryCmp( const AxisRep * A, const AxisRep * B )
{
	// If values are unequal, sort by comparing them.
	// Otherwise, sort as equal if types are equal.  Otherwise sort such that min. bound > max. bound, to reduce overlap count.
	if( A->Value < B->Value )
	{
		return -1;
	}
	if( A->Value > B->Value )
	{
		return 1;
	}
	return (INT)((A->Index >> 31) & 1) - (INT)((B->Index >> 31) & 1);
}

/** Given a set of AABBs, returns all overlapping pairs.  Each pair is packed into a DWORD, with indices
	in the high and low WORDs.  Each index refers to the AABBs array.
 */
void OverlapAABBs( const TArray<FBox> & AABBs, TArray<FIntPair> & Pairs )
{
	check( AABBs.Num() <= 0x3FFFFFFF );	// We'll be creating an array of twice this size

	Pairs.Empty();

	TArray<AxisRep> Axes[3];	// Each element is an index into the AABBs array, except the high bit is used to denote min (0) or max(1).

	DWORD AxisOverlapCount[3];

	const UINT AxisSize = 2*AABBs.Num();
	for( INT AxisN = 0; AxisN < 3; ++AxisN )
	{
		// Initialize axis
		Axes[AxisN].Empty( AxisSize );
		for( INT Index = 0; Index < AABBs.Num(); ++Index )
		{
			const FBox & AABB = AABBs( Index );
			FLOAT MinValue = AABB.Min[AxisN];
			FLOAT MaxValue = AABB.Max[AxisN];
			if( MinValue >= MaxValue )
			{	// Keep bounds apart
				FLOAT Mean = 0.5f*(MinValue+MaxValue);
				MinValue = (1.0f - KINDA_SMALL_NUMBER)*Mean;
				MaxValue = (1.0f + KINDA_SMALL_NUMBER)*Mean;
			}
			AxisRep Rep1, Rep2;
			Rep1.Value = MinValue;
			Rep1.Index = Index;
			Rep2.Value = MaxValue;
			Rep2.Index = Index|0x80000000;
			INT NextIndex = Axes[AxisN].Add( 2 );
			Axes[AxisN]( NextIndex ) = Rep1;
			Axes[AxisN]( NextIndex+1 ) = Rep2;
		}
		// Sort axis
		appQsort( &Axes[AxisN](0), AxisSize, sizeof(AxisRep), (QSORT_COMPARE)BoundaryCmp );
		// Count overlaps on axis
		AxisOverlapCount[AxisN] = 0;
		INT CurrentOverlap = 0;
		for( INT MarkerN = 0; MarkerN < Axes[AxisN].Num(); ++MarkerN )
		{
			AxisRep & Rep = Axes[AxisN]( MarkerN );
			if( !(Rep.Index & 0x80000000) )
			{
				AxisOverlapCount[AxisN] += CurrentOverlap;
				++CurrentOverlap;
			}
			else
			{
				check( CurrentOverlap > 0 );
				--CurrentOverlap;
			}
		}
	}

	// Find axis with fewest overlaps
	INT MinOverlapAxisN = AxisOverlapCount[0] < AxisOverlapCount[1] ? 0 : 1;
	if( AxisOverlapCount[2] < AxisOverlapCount[MinOverlapAxisN] )
	{
		MinOverlapAxisN = 2;
	}

	// Record overlaps
	TArray<INT> Stack;
	TArray<AxisRep> & Axis = Axes[MinOverlapAxisN];
	const INT AxisN1 = (MinOverlapAxisN+1)%3;
	const INT AxisN2 = (MinOverlapAxisN+2)%3;
	for( INT MarkerN = 0; MarkerN < Axis.Num(); ++MarkerN )
	{
		AxisRep & Rep = Axis( MarkerN );
		if( !(Rep.Index & 0x80000000) )
		{
			const FBox & Box = AABBs( Rep.Index );
			for( INT StackIndex = 0; StackIndex < Stack.Num(); ++StackIndex )
			{
				const INT OtherIndex = Stack( StackIndex );
				const FBox & OtherBox = AABBs( OtherIndex );
				// They overlap on MinOverlapAxisN, test the other two axes
				if( Box.Min[AxisN1] < OtherBox.Max[AxisN1] && Box.Max[AxisN1] > OtherBox.Min[AxisN1] &&
					Box.Min[AxisN2] < OtherBox.Max[AxisN2] && Box.Max[AxisN2] > OtherBox.Min[AxisN2] )
				{	// They overlap on all three axes
					INT PairIndex = Pairs.Add( 1 );
					FIntPair & Pair = Pairs( PairIndex );
					Pair.A = Rep.Index;
					Pair.B = OtherIndex;
				}
			}
			Stack.AddItem( Rep.Index );
		}
		else
		{	// Remove from stack
			INT Index = Rep.Index&0x7FFFFFFF;
			INT StackIndex = Stack.FindItemIndex( Index );
			check( StackIndex != INDEX_NONE );
			Stack.Remove( StackIndex );
		}
	}
}

inline UBOOL GetExtent( TArray<FVector> & Verts, const FVector & Dir, FLOAT & MinValue, FLOAT & MaxValue )
{
	if( Verts.Num() == 0 )
	{
		return FALSE;
	}

	MinValue = BIG_NUMBER;
	MaxValue = -BIG_NUMBER;

	for( INT VertIndex = 0; VertIndex < Verts.Num(); ++VertIndex )
	{
		FLOAT D = Verts( VertIndex ) | Dir;
		if( D < MinValue )
		{
			MinValue = D;
		}
		if( D > MaxValue )
		{
			MaxValue = D;
		}
	}

	return TRUE;
}

inline UBOOL DirectionOverlap( TArray<FVector> & Verts1, TArray<FVector> & Verts2, const FVector & Dir, FLOAT Padding )
{
	FLOAT MinValue1, MaxValue1;
	if( GetExtent( Verts1, Dir, MinValue1, MaxValue1 ) )
	{
		FLOAT MinValue2, MaxValue2;
		if( GetExtent( Verts2, Dir, MinValue2, MaxValue2 ) )
		{
			if( MinValue1 - MaxValue2 > Padding )
			{
				return FALSE;
			}
			if( MinValue2 - MaxValue1 > Padding )
			{
				return FALSE;
			}
			return TRUE;
		}
	}
	return FALSE;
}

UBOOL ConvexOverlap( FKConvexElem & Elem1, const FMatrix & LocalToWorld1, const FVector & Scale3D1,
					 FKConvexElem & Elem2, const FMatrix & LocalToWorld2, const FVector & Scale3D2,
					 FLOAT Padding )
{
	if( Elem1.VertexData.Num() == 0 || Elem2.VertexData.Num() == 0 )
	{
		return FALSE;
	}

	// Put the scale back into the TMs
	FVector Axis0, Axis1, Axis2;
	LocalToWorld1.GetAxes( Axis0, Axis1, Axis2 );
	FMatrix ScaledLocalToWorld1( Axis0*Scale3D1[0], Axis1*Scale3D1[1], Axis2*Scale3D1[2], LocalToWorld1.GetOrigin() );
	FMatrix ScaledLocalToWorld1TA = ScaledLocalToWorld1.TransposeAdjoint();	// For use with normal transform
	LocalToWorld2.GetAxes( Axis0, Axis1, Axis2 );
	FMatrix ScaledLocalToWorld2( Axis0*Scale3D1[0], Axis1*Scale3D1[1], Axis2*Scale3D1[2], LocalToWorld2.GetOrigin() );
	FMatrix ScaledLocalToWorld2TA = ScaledLocalToWorld2.TransposeAdjoint();	// For use with normal transform

	// Create world convex data
	TArray<FVector> Verts1( Elem1.VertexData.Num() );
	TArray<FVector> Verts2( Elem2.VertexData.Num() );
	for( INT i = 0; i < Elem1.VertexData.Num(); ++i )
	{
		Verts1( i ) = ScaledLocalToWorld1.TransformFVector( Elem1.VertexData( i ) );
	}
	for( INT i = 0; i < Elem2.VertexData.Num(); ++i )
	{
		Verts2( i ) = ScaledLocalToWorld2.TransformFVector( Elem2.VertexData( i ) );
	}

	// Create test directions and perform overlap tests
	for( INT i = 0; i < Elem1.FaceNormalDirections.Num(); ++i )
	{
		FVector Dir = ScaledLocalToWorld1TA.TransformNormal( Elem1.FaceNormalDirections( i ) );
		if( Dir.SizeSquared() > SMALL_NUMBER )
		{
			if( !DirectionOverlap( Verts1, Verts2, Dir.UnsafeNormal(), Padding ) )
			{
				return FALSE;
			}
		}
	}
	for( INT i = 0; i < Elem2.FaceNormalDirections.Num(); ++i )
	{
		FVector Dir = ScaledLocalToWorld2TA.TransformNormal( Elem2.FaceNormalDirections( i ) );
		if( Dir.SizeSquared() > SMALL_NUMBER )
		{
			if( !DirectionOverlap( Verts1, Verts2, Dir.UnsafeNormal(), Padding ) )
			{
				return FALSE;
			}
		}
	}
	for( INT i = 0; i < Elem1.EdgeDirections.Num(); ++i )
	{
		FVector Edge1 = ScaledLocalToWorld1.TransformNormal( Elem1.EdgeDirections( i ) );
		for( INT j = 0; j < Elem2.EdgeDirections.Num(); ++j )
		{
			FVector Edge2 = ScaledLocalToWorld2.TransformNormal( Elem2.EdgeDirections( j ) );
			FVector Dir = Edge1^Edge2;
			if( Dir.SizeSquared() > SMALL_NUMBER )
			{
				if( !DirectionOverlap( Verts1, Verts2, Dir.UnsafeNormal(), Padding ) )
				{
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}
