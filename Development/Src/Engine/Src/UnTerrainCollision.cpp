/*=============================================================================
UnTerrainCollision.cpp: Terrain collision
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "UnCollision.h"

/**
* Uses the kDOP code to determine whether a point with extent encroaches
*/
UBOOL UTerrainComponent::PointCheck(FCheckResult& Result,const FVector& InPoint,const FVector& InExtent,DWORD TraceFlags)
{
	UBOOL Hit = FALSE;
	if (BVTree.Nodes.Num())
	{ 
		FTerrainBVTreePointCollisionCheck BVCheck(InPoint, InExtent, this, &Result);
		Hit = BVTree.PointCheck(BVCheck);
		if (Hit == TRUE)
		{
			// Transform the hit normal to world space if there was a hit
			// This is deferred until now because multiple triangles might get
			// hit in the search and I want to delay the expensive transformation
			// as late as possible
			Result.Normal = BVCheck.GetHitNormal();
			Result.Location = BVCheck.GetHitLocation();
			Result.Actor = Owner;
			Result.Component = this;
		}
	}
	// Values are reversed here...need to fix this one day
	return !Hit;
}

//
//	LineCheckWithBox
//

static UBOOL LineCheckWithBox
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

	FVector LocalStart = Start - BoxCenter;

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
			return 0;
	}
	else
	{
		// If it is parallel, early return if start is outiside slab.
		if(!(Abs(LocalStart.X) <= BoxRadii.X))
		{
			return 0;
		}
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
			return 0;
	}
	else
	{
		if(!(Abs(LocalStart.Y) <= BoxRadii.Y))
			return 0;
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
			return 0;
	}
	else
	{
		if(!(Abs(LocalStart.Z) <= BoxRadii.Z))
		{
			return 0;
		}
	}

	// we hit!
	return 1;
}

UBOOL ClipLineWithBox(const FBox& Box, const FVector& Start, const FVector& End, FVector& IntersectedStart, FVector& IntersectedEnd)
{
	IntersectedStart = Start;
	IntersectedEnd = End;

	FVector Dir;
	FLOAT TEdgeOfBox,TLineLength;
	UBOOL StartCulled,EndCulled;

	// Bound by neg X
	StartCulled = IntersectedStart.X < Box.Min.X;
	EndCulled = IntersectedEnd.X < Box.Min.X;
	if (StartCulled && EndCulled)
	{
		IntersectedStart = Start;
		IntersectedEnd = Start;
		return FALSE;
	}
	else if (StartCulled)
	{
		check(IntersectedEnd.X > IntersectedStart.X); // div by 0 should be impossible by check above

		Dir = IntersectedStart - IntersectedEnd;
		TEdgeOfBox = Box.Min.X - IntersectedEnd.X;
		TLineLength = IntersectedStart.X - IntersectedEnd.X;
		IntersectedStart = IntersectedEnd + Dir*(TEdgeOfBox/TLineLength);
	}
	else if (EndCulled)
	{
		check(IntersectedStart.X > IntersectedEnd.X); // div by 0 should be impossible by check above

		Dir = IntersectedEnd - IntersectedStart;
		TEdgeOfBox = Box.Min.X - IntersectedStart.X;
		TLineLength = IntersectedEnd.X - IntersectedStart.X;
		IntersectedEnd = IntersectedStart + Dir*(TEdgeOfBox/TLineLength);
	}

	// Bound by pos X
	StartCulled = IntersectedStart.X > Box.Max.X;
	EndCulled = IntersectedEnd.X > Box.Max.X;
	if (StartCulled && EndCulled)
	{
		IntersectedStart = Start;
		IntersectedEnd = Start;
		return FALSE;
	}
	else if (StartCulled)
	{
		check(IntersectedEnd.X < IntersectedStart.X); // div by 0 should be impossible by check above

		Dir = IntersectedStart - IntersectedEnd;
		TEdgeOfBox = Box.Max.X - IntersectedEnd.X;
		TLineLength = IntersectedStart.X - IntersectedEnd.X;
		IntersectedStart = IntersectedEnd + Dir*(TEdgeOfBox/TLineLength);
	}
	else if (EndCulled)
	{
		check(IntersectedStart.X < IntersectedEnd.X); // div by 0 should be impossible by check above

		Dir = IntersectedEnd - IntersectedStart;
		TEdgeOfBox = Box.Max.X - IntersectedStart.X;
		TLineLength = IntersectedEnd.X - IntersectedStart.X;
		IntersectedEnd = IntersectedStart + Dir*(TEdgeOfBox/TLineLength);
	}

	// Bound by neg Y
	StartCulled = IntersectedStart.Y < Box.Min.Y;
	EndCulled = IntersectedEnd.Y < Box.Min.Y;
	if (StartCulled && EndCulled)
	{
		IntersectedStart = Start;
		IntersectedEnd = Start;
		return FALSE;
	}
	else if (StartCulled)
	{
		check(IntersectedEnd.Y > IntersectedStart.Y); // div by 0 should be impossible by check above

		Dir = IntersectedStart - IntersectedEnd;
		TEdgeOfBox = Box.Min.Y - IntersectedEnd.Y;
		TLineLength = IntersectedStart.Y - IntersectedEnd.Y;
		IntersectedStart = IntersectedEnd + Dir*(TEdgeOfBox/TLineLength);
	}
	else if (EndCulled)
	{
		check(IntersectedStart.Y > IntersectedEnd.Y); // div by 0 should be impossible by check above

		Dir = IntersectedEnd - IntersectedStart;
		TEdgeOfBox = Box.Min.Y - IntersectedStart.Y;
		TLineLength = IntersectedEnd.Y - IntersectedStart.Y;
		IntersectedEnd = IntersectedStart + Dir*(TEdgeOfBox/TLineLength);
	}

	// Bound by pos Y
	StartCulled = IntersectedStart.Y > Box.Max.Y;
	EndCulled = IntersectedEnd.Y > Box.Max.Y;
	if (StartCulled && EndCulled)
	{
		IntersectedStart = Start;
		IntersectedEnd = Start;
		return FALSE;
	}
	else if (StartCulled)
	{
		check(IntersectedEnd.Y < IntersectedStart.Y); // div by 0 should be impossible by check above

		Dir = IntersectedStart - IntersectedEnd;
		TEdgeOfBox = Box.Max.Y - IntersectedEnd.Y;
		TLineLength = IntersectedStart.Y - IntersectedEnd.Y;
		IntersectedStart = IntersectedEnd + Dir*(TEdgeOfBox/TLineLength);
	}
	else if (EndCulled)
	{
		check(IntersectedStart.Y < IntersectedEnd.Y); // div by 0 should be impossible by check above

		Dir = IntersectedEnd - IntersectedStart;
		TEdgeOfBox = Box.Max.Y - IntersectedStart.Y;
		TLineLength = IntersectedEnd.Y - IntersectedStart.Y;
		IntersectedEnd = IntersectedStart + Dir*(TEdgeOfBox/TLineLength);
	}

	// Bound by neg Z
	StartCulled = IntersectedStart.Z < Box.Min.Z;
	EndCulled = IntersectedEnd.Z < Box.Min.Z;
	if (StartCulled && EndCulled)
	{
		IntersectedStart = Start;
		IntersectedEnd = Start;
		return FALSE;
	}
	else if (StartCulled)
	{
		check(IntersectedEnd.Z > IntersectedStart.Z); // div by 0 should be impossible by check above

		Dir = IntersectedStart - IntersectedEnd;
		TEdgeOfBox = Box.Min.Z - IntersectedEnd.Z;
		TLineLength = IntersectedStart.Z - IntersectedEnd.Z;
		IntersectedStart = IntersectedEnd + Dir*(TEdgeOfBox/TLineLength);
	}
	else if (EndCulled)
	{
		check(IntersectedStart.Z > IntersectedEnd.Z); // div by 0 should be impossible by check above

		Dir = IntersectedEnd - IntersectedStart;
		TEdgeOfBox = Box.Min.Z - IntersectedStart.Z;
		TLineLength = IntersectedEnd.Z - IntersectedStart.Z;
		IntersectedEnd = IntersectedStart + Dir*(TEdgeOfBox/TLineLength);
	}

	// Bound by pos Z
	StartCulled = IntersectedStart.Z > Box.Max.Z;
	EndCulled = IntersectedEnd.Z > Box.Max.Z;
	if (StartCulled && EndCulled)
	{
		IntersectedStart = Start;
		IntersectedEnd = Start;
		return FALSE;
	}
	else if (StartCulled)
	{
		check(IntersectedEnd.Z < IntersectedStart.Z); // div by 0 should be impossible by check above

		Dir = IntersectedStart - IntersectedEnd;
		TEdgeOfBox = Box.Max.Z - IntersectedEnd.Z;
		TLineLength = IntersectedStart.Z - IntersectedEnd.Z;
		IntersectedStart = IntersectedEnd + Dir*(TEdgeOfBox/TLineLength);
	}
	else if (EndCulled)
	{
		check(IntersectedStart.Z < IntersectedEnd.Z); // div by 0 should be impossible by check above

		Dir = IntersectedEnd - IntersectedStart;
		TEdgeOfBox = Box.Max.Z - IntersectedStart.Z;
		TLineLength = IntersectedEnd.Z - IntersectedStart.Z;
		IntersectedEnd = IntersectedStart + Dir*(TEdgeOfBox/TLineLength);
	}
	return TRUE;
}


//
//	UTerrainComponent::LineCheck
//

UBOOL UTerrainComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
#if WITH_EDITOR
	if ((GIsEditor == TRUE) && ((TraceFlags & TRACE_Visible) != 0))
	{
		ATerrain* Terrain = Cast<ATerrain>(GetOwner());
		if (Terrain && Terrain->IsHiddenEd())
		{
			return TRUE;
		}
	}
#endif

	UBOOL	ZeroExtent = (Extent == FVector(0,0,0));

#if STATS
	DWORD Counter = ZeroExtent ? STAT_TerrainZeroExtentTime : STAT_TerrainExtentTime;
	SCOPE_CYCLE_COUNTER(Counter);
#endif

	Result.Time = 1.0f;
	UBOOL Hit = FALSE;

	if (ZeroExtent)
	{
#if	!CONSOLE
		const UBOOL bUseInGameCollision = GWorld->HasBegunPlay();
#else
		// No need for the function call cost on console
		const UBOOL bUseInGameCollision = TRUE;
		// we want to know IMMEDIATELY if we have no nodes in our Tree on consoles as it will be using the slow path then
		// 07/12/19 SAS: Disabling this check as we no longer stream out BVTree data for terrains with
		//				 collision disabled on them. To avoid having any crashing it is being removed.
		//check(BVTree.Nodes.Num() > 0); 
#endif
		// Use the fast kDOP-tree for the all of the traces except in the editor where we use the high poly version
		// NOTE: Always use the slow method in the editor
		if (bUseInGameCollision && BVTree.Nodes.Num() > 0)
		{
			FTerrainBVTreeLineCollisionCheck BVCheck(Start, End, TraceFlags, this, &Result);
			Hit = BVTree.LineCheck(BVCheck);
			if (Hit == TRUE)
			{
				// Transform the hit normal to world space if there was a hit
				// This is deferred until now because multiple triangles might get
				// hit in the search and I want to delay the expensive transformation
				// as late as possible
				Result.Normal = BVCheck.GetHitNormal();
				Result.Component = this;
			}
		}
		else
		{
			if (GIsGame == TRUE)
			{
				warnf(TEXT("UTerrainComponent::LineCheck> GIsGame is TRUE, skipping old checks"));
				return TRUE;
			}

			// Generate the patch bounds if they aren't present
			if (PatchBounds.Num() != (SectionSizeX * SectionSizeY))
			{
				UpdatePatchBounds();
			}

			FVector BoundedStart, BoundedEnd;
			// Clip the trace against the bounding box for a tighter line bounding box
			if (ClipLineWithBox(Bounds.GetBox(), Start, End, BoundedStart, BoundedEnd) == FALSE)
			{
				return TRUE;
			}

			// Get the transform from the terrain
			ATerrain* Terrain = GetTerrain();
			const FMatrix& WorldToLocal = Terrain->WorldToLocal();

			// Transform the trace info to local space
			const FVector& LocalStart = WorldToLocal.TransformFVector(Start);
			const FVector& LocalEnd = WorldToLocal.TransformFVector(End);
			const FVector& LocalDirection = LocalEnd - LocalStart;
			FVector LocalOneOverDirection;
			LocalOneOverDirection.X = Square(LocalDirection.X) > Square(DELTA) ? 1.0f / LocalDirection.X : 0.0f;
			LocalOneOverDirection.Y = Square(LocalDirection.Y) > Square(DELTA) ? 1.0f / LocalDirection.Y : 0.0f;
			LocalOneOverDirection.Z = Square(LocalDirection.Z) > Square(DELTA) ? 1.0f / LocalDirection.Z : 0.0f;

			// Use the clipped line to generate the X/Y region to check
			const FVector& BoundedLocalStart = WorldToLocal.TransformFVector(BoundedStart);
			const FVector& BoundedLocalEnd = WorldToLocal.TransformFVector(BoundedEnd);
			// Clamp the line by the components section
			INT	MinX = Clamp(appTrunc(Min(BoundedLocalStart.X,BoundedLocalEnd.X)),SectionBaseX,SectionBaseX + TrueSectionSizeX - 1),
				MinY = Clamp(appTrunc(Min(BoundedLocalStart.Y,BoundedLocalEnd.Y)),SectionBaseY,SectionBaseY + TrueSectionSizeY - 1),
				MaxX = Clamp(appTrunc(Max(BoundedLocalStart.X,BoundedLocalEnd.X)),SectionBaseX,SectionBaseX + TrueSectionSizeX - 1),
				MaxY = Clamp(appTrunc(Max(BoundedLocalStart.Y,BoundedLocalEnd.Y)),SectionBaseY,SectionBaseY + TrueSectionSizeY - 1);

			if(!Result.Actor)
				Result.Time = 1.0f;

			INT	TesselationLevel = 1;

			// Check against the patches within the line's bounding box,
			// clipped to this component via the min/max values for the section
			for(INT PatchY = MinY;PatchY <= MaxY;PatchY++)
			{
				// Check each patch within the clipped row
				for (INT PatchX = MinX; PatchX <= MaxX; PatchX++)
				{
					// PatchX, PatchY will already be in the terrain grid space
					// (not the component space)
					if ((TraceFlags & TRACE_TerrainIgnoreHoles) == 0)
					{
						if (Terrain->IsTerrainQuadVisible(PatchX, PatchY) == FALSE)
						{
							continue;
						}
					}

					const FTerrainPatchBounds& PatchBound = PatchBounds(
						(PatchY - SectionBaseY) / Terrain->MaxTesselationLevel * SectionSizeX + 
						(PatchX - SectionBaseX) / Terrain->MaxTesselationLevel);

					FLOAT CenterHeight = (PatchBound.MaxHeight + PatchBound.MinHeight) * 0.5f;

					if (!LineCheckWithBox(
						FVector(PatchX + 0.5f,PatchY + 0.5f,CenterHeight),
						FVector(
						0.5f + PatchBound.MaxDisplacement,
						0.5f + PatchBound.MaxDisplacement,
						PatchBound.MaxHeight - CenterHeight
						),
						LocalStart,
						LocalDirection,
						LocalOneOverDirection
						)
						)
					{
						continue;
					}

					const FTerrainPatch&	Patch = Terrain->GetPatch(PatchX,PatchY);

					FVector	PatchVertexCache[2][TERRAIN_MAXTESSELATION + 1];
					INT		NextCacheRow = 0;

					for(INT SubX = 0;SubX <= TesselationLevel;SubX++)
					{
						PatchVertexCache[NextCacheRow][SubX] = Terrain->GetCollisionVertex(Patch,PatchX,PatchY,SubX,0,TesselationLevel);
					}

					NextCacheRow = 1 - NextCacheRow;

					for(INT SubY = 0;SubY < TesselationLevel;SubY++)
					{
						for(INT SubX = 0;SubX <= TesselationLevel;SubX++)
						{
							PatchVertexCache[NextCacheRow][SubX] = Terrain->GetCollisionVertex(Patch,PatchX,PatchY,SubX,SubY + 1,TesselationLevel);
						}

						for(INT SubX = 0;SubX < TesselationLevel;SubX++)
						{
							const FVector&	V00 = PatchVertexCache[1 - NextCacheRow][SubX],
								V10 = PatchVertexCache[1 - NextCacheRow][SubX + 1],
								V01 = PatchVertexCache[NextCacheRow][SubX],
								V11 = PatchVertexCache[NextCacheRow][SubX + 1];
							UBOOL			TesselationHit = 0;

							if (Terrain->IsTerrainQuadFlipped(PatchX+SubX, PatchY+SubY) == FALSE)
							{
								TesselationHit |= LineCheckWithTriangle(Result,V00,V01,V11,LocalStart,LocalEnd,LocalDirection);
								TesselationHit |= LineCheckWithTriangle(Result,V00,V11,V10,LocalStart,LocalEnd,LocalDirection);
							}
							else
							{
								TesselationHit |= LineCheckWithTriangle(Result,V00,V01,V10,LocalStart,LocalEnd,LocalDirection);
								TesselationHit |= LineCheckWithTriangle(Result,V10,V01,V11,LocalStart,LocalEnd,LocalDirection);
							}

							if(TesselationHit)
							{
								Result.Component = this;
								Result.Actor = Owner;
								Result.Material = NULL;
								Result.Normal.Normalize();
								Result.Normal = Terrain->LocalToWorld().TransposeAdjoint().TransformNormal(Result.Normal).SafeNormal(); 

								goto Finished;
							}
						}

						NextCacheRow = 1 - NextCacheRow;
					}
				}
			}
		}
	}
	// Swept box check
	else
	{
		if (BVTree.Nodes.Num() > 0)
		{
			FTerrainBVTreeBoxCollisionCheck BVCheck(Start, End, Extent, TraceFlags, this, &Result);
			Hit = BVTree.BoxCheck(BVCheck);
			if(Hit == TRUE)
			{
				// Transform the hit normal to world space if there was a hit
				// This is deferred until now because multiple triangles might get
				// hit in the search and I want to delay the expensive transformation
				// as late as possible
				Result.Normal = BVCheck.GetHitNormal();
				Result.Component = this;
			}
		}
	}

Finished:
	if(Result.Component == this)
	{
		Result.Actor = Owner;
		if (TraceFlags & TRACE_Accurate)
		{
			Result.Time = Clamp(Result.Time,0.0f,1.0f);
		}
		else
		{
			Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f / (End - Start).Size(),4.0f / (End - Start).Size()),0.0f,1.0f);
		}
		Result.Location	= Start + (End - Start) * Result.Time;
	}
	return (Result.Component != this);
}

UBOOL ATerrain::ActorLineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	UBOOL Hit = 0;
	for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Components(ComponentIndex));
		if(Primitive && !Primitive->LineCheck(Result,End,Start,Extent,TraceFlags))
		{
			Hit = 1;
		}
	}
	for(INT ComponentIndex = 0;ComponentIndex < TerrainComponents.Num();ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = TerrainComponents(ComponentIndex);
		if(Primitive && !Primitive->LineCheck(Result,End,Start,Extent,TraceFlags))
		{
			Hit = 1;
		}
	}
	return !Hit;
}
