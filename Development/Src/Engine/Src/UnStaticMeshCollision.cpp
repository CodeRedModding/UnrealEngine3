/*=============================================================================
	UnStaticMeshCollision.cpp: Static mesh collision code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnCollision.h"

//
//	UStaticMesh::LineCheck
//

#define TRACK_START_PENETRATING (0)

#if TRACK_START_PENETRATING
static FVector LastPassStart(0);
static FVector LastPassEnd(0);
#endif // TRACK_START_PENETRATING


UBOOL UStaticMeshComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	return LineCheck(Result, End, Start, Extent, TraceFlags, 0);
}


UBOOL UStaticMeshComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags, UINT LODIndex)
{
	if(!StaticMesh)
	{
		return Super::LineCheck(Result,End,Start,Extent,TraceFlags);
	}

	UBOOL	Hit = FALSE,
			ZeroExtent = Extent == FVector(0,0,0);
	// Reset the time otherwise it can default to zero
	Result.Time = 1.f;

#if STATS
	DWORD Counter = ZeroExtent ? (DWORD)STAT_StaticMeshZeroExtentTime : (DWORD)STAT_StaticMeshExtentTime;
	SCOPE_CYCLE_COUNTER(Counter);
#endif

	UBOOL bWantSimpleCheck = (StaticMesh->UseSimpleBoxCollision && !ZeroExtent) || (StaticMesh->UseSimpleLineCollision && ZeroExtent);
	
	// To support the case this is an InstancedStaticMeshComponent, we iterate over however many instances there are
	INT NumInstances = IsPrimitiveInstanced() ? GetInstanceCount() : 1;

	if( Owner && 
		bWantSimpleCheck && 
		!(TraceFlags & TRACE_ShadowCast) && // Don't use simple collision for shadows or if specifically forced to use complex
		!(TraceFlags & TRACE_ComplexCollision) )
	{
		// If physics model are present. Note that if we asked for it, and it's not present, we just fail.
		if(StaticMesh->BodySetup)
		{
			UBOOL bStopAtAnyHit = (TraceFlags & TRACE_StopAtAnyHit);

			for(INT InstanceIdx=0; InstanceIdx<NumInstances; InstanceIdx++)
			{
				FCheckResult TempResult(1.f);
				UBOOL bTempHit = FALSE;

				// Get transforms for this instance, and decompose into non-scaling transforms and 3D scale factor
				FMatrix InstanceLocalToWorld = GetInstanceLocalToWorld(InstanceIdx);
				const FVector InstanceScale3D = InstanceLocalToWorld.ExtractScaling();

				if( Abs(InstanceScale3D.X * InstanceScale3D.Y * InstanceScale3D.Z) > KINDA_SMALL_NUMBER )
				{
					// Check against aggregates to see if one of those were hit sooner than the UModel
					bTempHit = !StaticMesh->BodySetup->AggGeom.LineCheck(TempResult, InstanceLocalToWorld, InstanceScale3D, End, Start, Extent, bStopAtAnyHit, FALSE);
					if(bTempHit)
					{
						const FVector Vec = End - Start;
						const FLOAT Dist = Vec.Size();
						if (TraceFlags & TRACE_Accurate)
						{
							TempResult.Time = Clamp(TempResult.Time,0.0f,1.0f);
						}
						else
						{
							// Pull-back hit result
							TempResult.Time = Clamp(TempResult.Time - Clamp(0.1f,0.1f/Dist, 1.f/Dist),0.f,1.f);
						}

						// If better than best hit so far, copy result to 'real' result
						if (TempResult.Time < Result.Time || !Hit)
						{
							Result = TempResult;

							Result.Location = Start + (Vec * Result.Time);
							Result.Actor = Owner;
							Result.Component = this;

							// Get the physical material we hit.
							Result.PhysMaterial = StaticMesh->BodySetup->PhysMaterial;
						}

						Hit = TRUE;
					}
				}
			}
		}
	}
	else if (StaticMesh->kDOPTree.Nodes.Num())
	{
		for(INT InstanceIdx=0; InstanceIdx<NumInstances; InstanceIdx++)
		{
			FCheckResult TempResult(1.f);
			UBOOL bTempHit = FALSE;

			// Get transforms for this instance (avoid the .Determinant() call in the non-instance case).
			const FMatrix InstanceLocalToWorld = GetInstanceLocalToWorld(InstanceIdx);
			const FLOAT InstancedLocalToWorldDeterminant = IsPrimitiveInstanced() ? InstanceLocalToWorld.Determinant() : LocalToWorldDeterminant;

			// Create the object that knows how to extract information from the component/mesh
			FStaticMeshCollisionDataProvider Provider(this, InstanceLocalToWorld, InstancedLocalToWorldDeterminant, LODIndex);

			if (ZeroExtent == TRUE)
			{
				// Create the check structure with all the local space fun
				TkDOPLineCollisionCheck<FStaticMeshCollisionDataProvider,WORD,UStaticMesh::kDOPTreeType> kDOPCheck(Start,End,TraceFlags,Provider,&TempResult);
				// Do the line check
				bTempHit = StaticMesh->kDOPTree.LineCheck(kDOPCheck);
				if (bTempHit == 1)
				{
					// Transform the hit normal to world space if there was a hit
					// This is deferred until now because multiple triangles might get
					// hit in the search and I want to delay the expensive transformation
					// as late as possible
					TempResult.Normal = kDOPCheck.GetHitNormal();
				}
			}
			else
			{
				// Create the check structure with all the local space fun
				TkDOPBoxCollisionCheck<FStaticMeshCollisionDataProvider,WORD,UStaticMesh::kDOPTreeType> kDOPCheck(Start,End,Extent,TraceFlags,Provider,&TempResult);
				// And collide against it
				bTempHit = StaticMesh->kDOPTree.BoxCheck(kDOPCheck);
				if( bTempHit == 1 )
				{
					// Transform the hit normal to world space if there was a hit
					// This is deferred until now because multiple triangles might get
					// hit in the search and I want to delay the expensive transformation
					// as late as possible
					TempResult.Normal = kDOPCheck.GetHitNormal();
				}
			}

			// We hit this instance..
			if(bTempHit)
			{
				// If better than best hit so far, copy result to 'real' result
				if (TempResult.Time < Result.Time || !Hit)
				{
					Result = TempResult;
				}

				Hit = TRUE;
			}
		}


		// We hit this mesh, so update the common out values
		if (Hit == TRUE)
		{
			Result.Actor = Owner;
			Result.Component = this;
			if (TraceFlags & TRACE_Accurate)
			{
				Result.Time = Clamp(Result.Time,0.0f,1.0f);
			}
			else
			{
				Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f / (End - Start).Size(),4.0f / (End - Start).Size()),0.0f,1.0f);
			}
			Result.Location = Start + (End - Start) * Result.Time;
		}
	}
	return !Hit;
}

//
//	UStaticMeshComponent::PointCheck
//

UBOOL UStaticMeshComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags)
{
	if(!StaticMesh)
	{
		return Super::PointCheck(Result,Location,Extent,TraceFlags);
	}

	SCOPE_CYCLE_COUNTER(STAT_StaticMeshPointTime);

	UBOOL	Hit = FALSE;

	// To support the case this is an InstancedStaticMeshComponent, we iterate over however many instances there are
	INT NumInstances = IsPrimitiveInstanced() ? GetInstanceCount() : 1;

	UBOOL ZeroExtent = Extent.IsZero();
	if (!(TraceFlags & TRACE_ComplexCollision) && ((StaticMesh->UseSimpleBoxCollision && !ZeroExtent) || (StaticMesh->UseSimpleLineCollision && ZeroExtent)))
	{
		// Test against aggregate simplified collision in BodySetup
		if(StaticMesh->BodySetup)
		{
			for(INT InstanceIdx=0; InstanceIdx<NumInstances; InstanceIdx++)
			{
				// Get transforms for this instance, and decompose into non-scaling transforms and 3D scale factor
				FMatrix InstanceLocalToWorld = GetInstanceLocalToWorld(InstanceIdx);
				FVector InstanceScale3D = InstanceLocalToWorld.ExtractScaling();

				// Do the point check
				FCheckResult TempCheck(1.f);
				Hit = !StaticMesh->BodySetup->AggGeom.PointCheck(TempCheck, InstanceLocalToWorld, InstanceScale3D, Location, Extent);

				// If we hit, fill in the Result struct.
				if(Hit)
				{
					Result.Normal = TempCheck.Normal;
					Result.Location = TempCheck.Location;
					Result.Actor = Owner;
					Result.Component = this;
					Result.PhysMaterial = StaticMesh->BodySetup->PhysMaterial;

					break;
				}
			}
		}
	}
	else if(StaticMesh->kDOPTree.Nodes.Num())
	{ 
		for(INT InstanceIdx=0; InstanceIdx<NumInstances; InstanceIdx++)
		{
			// Get transform and determinant
			FMatrix InstanceLocalToWorld = GetInstanceLocalToWorld(InstanceIdx);
			const FLOAT InstancedLocalToWorldDeterminant = IsPrimitiveInstanced() ? InstanceLocalToWorld.Determinant() : LocalToWorldDeterminant;

			// Create the object that knows how to extract information from the component/mesh
			FStaticMeshCollisionDataProvider Provider(this, InstanceLocalToWorld, InstancedLocalToWorldDeterminant);
			// Create the check structure with all the local space fun
			TkDOPPointCollisionCheck<FStaticMeshCollisionDataProvider,WORD,UStaticMesh::kDOPTreeType> kDOPCheck(Location,Extent,Provider,&Result);

			Hit = StaticMesh->kDOPTree.PointCheck(kDOPCheck);
			// Transform the hit normal to world space if there was a hit
			// This is deferred until now because multiple triangles might get
			// hit in the search and I want to delay the expensive transformation
			// as late as possible. Same thing holds true for the hit location
			if(Hit)
			{
				Result.Normal = kDOPCheck.GetHitNormal();
				Result.Location = kDOPCheck.GetHitLocation();

				Result.Normal.Normalize();
				// Now calculate the location of the hit in world space
				Result.Actor = Owner;
				Result.Component = this;

				break;
			}
		}
	}

	return !Hit;
}

/**
* Calculates the closest point this component to another component
* @param PrimitiveComponent - Another Primitive Component
* @param PointOnComponentA - Point on this primitive closest to other primitive
* @param PointOnComponentB - Point on other primitive closest to this primitive
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
/*GJKResult*/ BYTE UStaticMeshComponent::ClosestPointOnComponentToComponent(class UPrimitiveComponent*& OtherComponent,FVector& PointOnComponentA,FVector& PointOnComponentB)
{
	GJKResult Result = GJK_Fail;

	if (StaticMesh && StaticMesh->BodySetup)
	{

		FKAggregateGeom& AggGeom = StaticMesh->BodySetup->AggGeom;
		//AggGeom.DrawAggGeom(GWorld->LineBatcher, LocalToWorld, FVector(1.0, 1.0, 1.0), FColor(255,0,0,255), NULL, FALSE, FALSE);

		Result = (GJKResult)AggGeom.ClosestPointOnAggGeomToComponent(LocalToWorld, OtherComponent, PointOnComponentA, PointOnComponentB);
	}

	return Result;
}

/**		**INTERNAL USE ONLY**
* Implementation required by a primitive component in order to properly work with the closest points algorithms below
* Given an interface to some other primitive, return the points on each object closest to each other
* @param ExtentHelper - Interface class returning the supporting points on some other primitive type
* @param OutPointA - The point closest on the 'other' primitive
* @param OutPointB - The point closest on this primitive
* 
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
BYTE UStaticMeshComponent::ClosestPointOnComponentInternal(IGJKHelper* ExtentHelper, FVector& OutPointA, FVector& OutPointB)
{
	GJKResult Result = GJK_Fail;

	if (StaticMesh && StaticMesh->BodySetup)
	{
		FKAggregateGeom& AggGeom = StaticMesh->BodySetup->AggGeom;
		//AggGeom.DrawAggGeom(GWorld->LineBatcher, LocalToWorld, FVector(1.0, 1.0, 1.0), FColor(255,0,0,255), NULL, FALSE, FALSE);

		Result = (GJKResult)AggGeom.ClosestPointOnAggGeomToPoint(LocalToWorld, ExtentHelper, OutPointA, OutPointB);		
	}

	return Result;
}

