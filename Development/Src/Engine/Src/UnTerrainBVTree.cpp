/*=============================================================================
	UnTerrainBVTree.cpp: bounding-volume tree for terrain
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"

// Amount to expand the kDOP by
#define FUDGE_SIZE 0.1f

#define MAX_LEAFREGION_SIZE 2
#define DRAW_DEBUG_INFO 0
#define NULL_NODE ((WORD)-1)

/************************************************************************/
/* FTerrainBV */
/************************************************************************/

/** Sets the bounding volume to encompass this subregion of the terrain. */
void FTerrainBV::AddTerrainRegion(const FTerrainSubRegion& InRegion, const TArray<FVector>& TerrainVerts, INT TerrainSizeX)
{
	Bounds = FBox(0);

	for(INT Y=InRegion.YPos; Y<(InRegion.YPos + InRegion.YSize + 1); Y++)
	{
		for(INT X=InRegion.XPos; X<(InRegion.XPos + InRegion.XSize + 1); X++)
		{
			Bounds += TerrainVerts((Y*(TerrainSizeX+1))+X);
		}
	}

	// Just to be safe - slightly expand the bounds.
	Bounds.ExpandBy(FUDGE_SIZE);
}

/**
 * Checks a line against this BV. 
 *
 * @param	Check		The aggregated line check structure.
 * @param	OutHitTime	[Out] The value indicating hit time.
 * @return				TRUE if an intersection occurs.
 */
UBOOL FTerrainBV::LineCheck(const FTerrainBVTreeLineCollisionCheck& Check, FLOAT& OutHitTime) const
{
	FVector	Time(0.f,0.f,0.f);
	UBOOL Inside = 1;

	OutHitTime = 0.0f;  // always initialize (prevent valgrind whining) --ryan.

	if(Check.LocalStart.X < Bounds.Min.X)
	{
		if(Check.LocalDir.X <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (Bounds.Min.X - Check.LocalStart.X) * Check.LocalOneOverDir.X;
		}
	}
	else if(Check.LocalStart.X > Bounds.Max.X)
	{
		if(Check.LocalDir.X >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (Bounds.Max.X - Check.LocalStart.X) * Check.LocalOneOverDir.X;
		}
	}

	if(Check.LocalStart.Y < Bounds.Min.Y)
	{
		if(Check.LocalDir.Y <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (Bounds.Min.Y - Check.LocalStart.Y) * Check.LocalOneOverDir.Y;
		}
	}
	else if(Check.LocalStart.Y > Bounds.Max.Y)
	{
		if(Check.LocalDir.Y >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (Bounds.Max.Y - Check.LocalStart.Y) * Check.LocalOneOverDir.Y;
		}
	}

	if(Check.LocalStart.Z < Bounds.Min.Z)
	{
		if(Check.LocalDir.Z <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (Bounds.Min.Z - Check.LocalStart.Z) * Check.LocalOneOverDir.Z;
		}
	}
	else if(Check.LocalStart.Z > Bounds.Max.Z)
	{
		if(Check.LocalDir.Z >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (Bounds.Max.Z - Check.LocalStart.Z) * Check.LocalOneOverDir.Z;
		}
	}

	if(Inside)
	{
		OutHitTime = 0.f;
		return 1;
	}

	OutHitTime = Time.GetMax();

	if(OutHitTime >= 0.0f && OutHitTime <= 1.0f)
	{
		const FVector& Hit = Check.LocalStart + Check.LocalDir * OutHitTime;

		return (Hit.X > Bounds.Min.X - FUDGE_SIZE && Hit.X < Bounds.Max.X + FUDGE_SIZE &&
				Hit.Y > Bounds.Min.Y - FUDGE_SIZE && Hit.Y < Bounds.Max.Y + FUDGE_SIZE &&
				Hit.Z > Bounds.Min.Z - FUDGE_SIZE && Hit.Z < Bounds.Max.Z + FUDGE_SIZE);
	}
	return 0;
}

/**
 * Checks a point with extent against this BV. The extent is already added in
 * to the BV being tested (Minkowski sum), so this code just checks to see if
 * the point is inside the BV. 
 *
 * @param	Check	The aggregated point check structure.
 * @return			TRUE if an intersection occurs.
 */
UBOOL FTerrainBV::PointCheck(const FTerrainBVTreeLineCollisionCheck& Check) const
{
	return	Check.LocalStart.X >= Bounds.Min.X && Check.LocalStart.X <= Bounds.Max.X && 
			Check.LocalStart.Y >= Bounds.Min.Y && Check.LocalStart.Y <= Bounds.Max.Y && 
			Check.LocalStart.Z >= Bounds.Min.Z && Check.LocalStart.Z <= Bounds.Max.Z;
}

/**
 * Check (local space) AABB against this BV.
 *
 * @param		LocalAABB box in local space.
 * @return		TRUE if an intersection occurs.
 */
UBOOL FTerrainBV::AABBOverlapCheck(const FBox& LocalAABB) const
{
	return Bounds.Intersect(LocalAABB);
}


/************************************************************************/
/* FTerrainBVNode */
/************************************************************************/

static UBOOL CollisionInRegion(const FTerrainSubRegion& InRegion, const UTerrainComponent* TerrainComp)
{
	ATerrain* Terrain = TerrainComp->GetTerrain();
	check(Terrain);

	UBOOL bContainsVisible = FALSE;
	for(INT Y=InRegion.YPos; Y<(InRegion.YPos+InRegion.YSize) && !bContainsVisible; Y++)
	{
		const INT GlobalY = TerrainComp->SectionBaseY + Y;
		for(INT X=InRegion.XPos; X<(InRegion.XPos+InRegion.XSize) && !bContainsVisible; X++)
		{
			const INT GlobalX = TerrainComp->SectionBaseX + X;

			bContainsVisible |= Terrain->IsTerrainQuadVisible(GlobalX, GlobalY);
		}
	}

	return bContainsVisible;
}

/**
 *	Take a region of the terrain and either store it in this node or,
 * if its too big, split into child nodes along its longest dimension.
 */
void FTerrainBVNode::SplitTerrain(const FTerrainSubRegion& InRegion, const UTerrainComponent* TerrainComp, TArray<FTerrainBVNode>& Nodes)
{
	const TArray<FVector>& TerrainVerts = TerrainComp->CollisionVertices;
	const INT TerrainSizeX = TerrainComp->TrueSectionSizeX;

	// Add all of the triangles to the bounding volume
	BoundingVolume.AddTerrainRegion(InRegion, TerrainVerts, TerrainSizeX);

	// Should never be passing in empty region.
	check(InRegion.XSize > 0 && InRegion.YSize > 0);

	// If we need to split this region more - do it now
	if(InRegion.XSize > MAX_LEAFREGION_SIZE || InRegion.YSize > MAX_LEAFREGION_SIZE)
	{
		bIsLeaf = FALSE;

		// Calculate regions.
		//  Y
		//  ^
		//  |C  D
		//  |A  B
		//   --> X
		FTerrainSubRegion A,B,C,D;

		const WORD ASize = Max(InRegion.XSize/2, InRegion.YSize/2);

		A.XPos = InRegion.XPos;
		A.YPos = InRegion.YPos;
		A.XSize = Min(ASize, InRegion.XSize);
		A.YSize = Min(ASize, InRegion.YSize);
			
		B.XPos = InRegion.XPos + ASize;
		B.YPos = InRegion.YPos;
		B.XSize = (InRegion.XSize > ASize) ? InRegion.XSize - ASize : 0;
		B.YSize = A.YSize;

		C.XPos = InRegion.XPos;
		C.YPos = InRegion.YPos + ASize;
		C.XSize = A.XSize;
		C.YSize = (InRegion.YSize > ASize) ? InRegion.YSize - ASize : 0;

		D.XPos = InRegion.XPos + ASize;
		D.YPos = InRegion.YPos + ASize;
		D.XSize = B.XSize;
		D.YSize = C.YSize;

		n.NodeIndex[0]	= n.NodeIndex[1] = n.NodeIndex[2] = n.NodeIndex[3] = NULL_NODE;

		// If regions have valid area, recurse into them. Otherwise set to -1.
		if(CollisionInRegion(A, TerrainComp))
		{
			n.NodeIndex[0] = Nodes.Add();
			Nodes(n.NodeIndex[0]).SplitTerrain(A, TerrainComp, Nodes);
		}

		if(B.XSize > 0 && B.YSize > 0 && CollisionInRegion(B, TerrainComp))
		{
			n.NodeIndex[1] = Nodes.Add();
			Nodes(n.NodeIndex[1]).SplitTerrain(B, TerrainComp, Nodes);
		}

		if(C.XSize > 0 && C.YSize > 0 && CollisionInRegion(C, TerrainComp))
		{
			n.NodeIndex[2] = Nodes.Add();
			Nodes(n.NodeIndex[2]).SplitTerrain(C, TerrainComp, Nodes);
		}

		if(D.XSize > 0 && D.YSize > 0 && CollisionInRegion(D, TerrainComp))
		{
			n.NodeIndex[3] = Nodes.Add();
			Nodes(n.NodeIndex[3]).SplitTerrain(D, TerrainComp, Nodes);
		}
	}
	// No need to subdivide further so make this a leaf node
	else
	{
		bIsLeaf = TRUE;

		// Remember the region at this leaf.
		Region = InRegion;
	}
}

static void DrawTransformedBox(const FBox& Box, const FMatrix& TM)
{
	FVector	B[2],P,Q;
	INT ai,aj;
	const FMatrix& L2W = TM;
	B[0]=Box.Min;
	B[1]=Box.Max;

	for( ai=0; ai<2; ai++ ) for( aj=0; aj<2; aj++ )
	{
		P.X=B[ai].X; Q.X=B[ai].X;
		P.Y=B[aj].Y; Q.Y=B[aj].Y;
		P.Z=B[0].Z; Q.Z=B[1].Z;
		GWorld->LineBatcher->DrawLine(L2W.TransformFVector(P), L2W.TransformFVector(Q), FColor(255,0,0), SDPG_World);

		P.Y=B[ai].Y; Q.Y=B[ai].Y;
		P.Z=B[aj].Z; Q.Z=B[aj].Z;
		P.X=B[0].X; Q.X=B[1].X;
		GWorld->LineBatcher->DrawLine(L2W.TransformFVector(P), L2W.TransformFVector(Q), FColor(255,0,0), SDPG_World);

		P.Z=B[ai].Z; Q.Z=B[ai].Z;
		P.X=B[aj].X; Q.X=B[aj].X;
		P.Y=B[0].Y; Q.Y=B[1].Y;
		GWorld->LineBatcher->DrawLine(L2W.TransformFVector(P), L2W.TransformFVector(Q), FColor(255,0,0), SDPG_World);
	}
}

/** 
 * Determines the line in the FTerrainBVTreeLineCollisionCheck intersects this node. It
 * also will check the child nodes if it is not a leaf, otherwise it will check
 * against the triangle data.
 *
 * @param Check -- The aggregated line check data
 */
UBOOL FTerrainBVNode::LineCheck(FTerrainBVTreeLineCollisionCheck& Check) const
{
#if 0//DRAW_DEBUG_INFO
	DrawTransformedBox(BoundingVolume.Bounds, Check.LocalToWorld)
#endif

	UBOOL bHit = FALSE;
	// If this is a node, check the two child nodes and pick the closest one
	// to recursively check against and only check the second one if there is
	// not a hit or the hit returned is further out than the second node
	if (bIsLeaf == 0)
	{
		const UBOOL bStopAtAnyHit = Check.TraceFlags & TRACE_StopAtAnyHit;
		FLOAT ClosestHit = BIG_NUMBER;

		// Iterate over each node.
		for(INT i=0; i<4; i++)
		{
			const INT N = Check.NodeCheckOrder[i];
			// Check its a valid node
			if(n.NodeIndex[N] != NULL_NODE)
			{
				// Check the line hits this node bounds, and check the hit time is not further out than our closest hit so far.
				FLOAT BoundsHitTime;
				const UBOOL bBoundsCheck = Check.Nodes(n.NodeIndex[N]).BoundingVolume.LineCheck(Check, BoundsHitTime);
				if(bBoundsCheck && BoundsHitTime < ClosestHit)
				{
					// Line check against actual node (and subnodes).
					const UBOOL bGotHit = Check.Nodes(n.NodeIndex[N]).LineCheck(Check);
					if(bGotHit)
					{
						// If we hit - set bHit to TRUE, and update ClosestHit time.
						bHit = TRUE;
						ClosestHit = Min(ClosestHit, Check.Result->Time);
					}
				}

				// If we have a hit, and we don't want the first one - bail out now.
				if (bHit && bStopAtAnyHit)
				{
					return TRUE;
				}
			}
		}
	}
	else
	{
		// This is a leaf, check the triangles for a hit
		bHit = LineCheckTriangles(Check);
	}

	return bHit;
}

#define VERT(AX,AY) (((AY)*(TerrainSizeX+1))+(AX))

/**
 * Works through the list of triangles in this node checking each one for a collision.
 *
 * @param Check -- The aggregated line check data
 */
UBOOL FTerrainBVNode::LineCheckTriangles(FTerrainBVTreeLineCollisionCheck& Check) const
{
	// Assume a miss
	UBOOL bHit = FALSE;

	// Check for an early out
	const UBOOL bStopAtAnyHit = Check.TraceFlags & TRACE_StopAtAnyHit;

	const TArray<FVector>& TerrainVerts = Check.TerrainComp->CollisionVertices;
	const INT TerrainSizeX = Check.TerrainComp->TrueSectionSizeX;
	const ATerrain* Terrain = Check.TerrainComp->GetTerrain();

	for(INT Y=Region.YPos; Y<(Region.YPos+Region.YSize) && (bHit == FALSE || bStopAtAnyHit == FALSE); Y++)
	{
		const INT GlobalY = Check.TerrainComp->SectionBaseY + Y;
		for(INT X=Region.XPos; X<(Region.XPos+Region.XSize) && (bHit == FALSE || bStopAtAnyHit == FALSE); X++)
		{
			const INT GlobalX = Check.TerrainComp->SectionBaseX + X;

			INT CheckPatchX = GlobalX - (GlobalX % Terrain->MaxTesselationLevel);
			INT CheckPatchY = GlobalY - (GlobalY % Terrain->MaxTesselationLevel);
			if(Terrain->IsTerrainQuadVisible(CheckPatchX, CheckPatchY))
			{
				if (Terrain->IsTerrainQuadFlipped(GlobalX, GlobalY) == FALSE)
				{
					const FVector& A1 = TerrainVerts(VERT(X+0, Y+0));
					const FVector& A2 = TerrainVerts(VERT(X+0, Y+1));
					const FVector& A3 = TerrainVerts(VERT(X+1, Y+1));

					bHit |= LineCheckTriangle(Check,A1,A2,A3);
					if(bHit == FALSE || bStopAtAnyHit == FALSE)
					{
						const FVector& B1 = TerrainVerts(VERT(X+0, Y+0));
						const FVector& B2 = TerrainVerts(VERT(X+1, Y+1));
						const FVector& B3 = TerrainVerts(VERT(X+1, Y+0));

						bHit |= LineCheckTriangle(Check,B1,B2,B3);
					}
				}
				else
				{
					const FVector& A1 = TerrainVerts(VERT(X+0, Y+0));
					const FVector& A2 = TerrainVerts(VERT(X+0, Y+1));
					const FVector& A3 = TerrainVerts(VERT(X+1, Y+0));

					bHit |= LineCheckTriangle(Check,A1,A2,A3);
					if(bHit == FALSE || bStopAtAnyHit == FALSE)
					{
						const FVector& B1 = TerrainVerts(VERT(X+1, Y+0));
						const FVector& B2 = TerrainVerts(VERT(X+0, Y+1));
						const FVector& B3 = TerrainVerts(VERT(X+1, Y+1));

						bHit |= LineCheckTriangle(Check,B1,B2,B3);
					}
				}
			}
		}
	}

	return bHit;
}

/**
 * Performs collision checking against the triangle using the old collision code to handle it.
 *
 * @param Check -- The aggregated line check data
 * @param v1 -- The first vertex of the triangle
 * @param v2 -- The second vertex of the triangle
 * @param v3 -- The third vertex of the triangle
 */
UBOOL FTerrainBVNode::LineCheckTriangle(FTerrainBVTreeLineCollisionCheck& Check, const FVector& v1,const FVector& v2,const FVector& v3) const
{
#if DRAW_DEBUG_INFO
	FMatrix L2W = Check.LocalToWorld;
	GWorld->LineBatcher->DrawLine(L2W.TransformFVector(v1), L2W.TransformFVector(v2), FColor(0,255,0), SDPG_World);
	GWorld->LineBatcher->DrawLine(L2W.TransformFVector(v2), L2W.TransformFVector(v3), FColor(0,255,0), SDPG_World);
	GWorld->LineBatcher->DrawLine(L2W.TransformFVector(v3), L2W.TransformFVector(v1), FColor(0,255,0), SDPG_World);
#endif

	// Calculate the hit normal the same way the old code
	// did so things are the same
	const FVector& LocalNormal = ((v2 - v3) ^ (v1 - v3)).SafeNormal();
	// Calculate the hit time the same way the old code
	// did so things are the same
	const FPlane TrianglePlane(v1,LocalNormal);
	const FLOAT StartDist = TrianglePlane.PlaneDot(Check.LocalStart);
	const FLOAT EndDist = TrianglePlane.PlaneDot(Check.LocalEnd);
	if ((StartDist == EndDist) || (StartDist < -0.001f && EndDist < -0.001f) || (StartDist > 0.001f && EndDist > 0.001f))
	{
		return FALSE;
	}
	// Figure out when it will hit the triangle
	const FLOAT Time = -StartDist / (EndDist - StartDist);
	// If this triangle is not closer than the previous hit, reject it
	if (Time < 0.f || Time >= Check.Result->Time)
	{
		return FALSE;
	}
	// Calculate the line's point of intersection with the node's plane
	const FVector& Intersection = Check.LocalStart + Check.LocalDir * Time;
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
	Check.LocalHitNormal = LocalNormal;
	Check.Result->Time = Time;
	Check.Result->Material = NULL;
	return TRUE;
}

/**
 * Determines the line + extent in the FTerrainBVTreeBoxCollisionCheck intersects this
 * node. It also will check the child nodes if it is not a leaf, otherwise it
 * will check against the triangle data.
 *
 * @param Check -- The aggregated box check data
 */
UBOOL FTerrainBVNode::BoxCheck(FTerrainBVTreeBoxCollisionCheck& Check) const
{
#if 0//DRAW_DEBUG_INFO
	DrawTransformedBox( FBox(Check.Start - Check.Extent, Check.Start + Check.Extent), FMatrix::Identity );
#endif

	UBOOL bHit = FALSE;
	// If this is a node, check the two child nodes and pick the closest one
	// to recursively check against and only check the second one if there is
	// not a hit or the hit returned is further out than the second node
	if (bIsLeaf == 0)
	{
		const UBOOL bStopAtAnyHit = Check.TraceFlags & TRACE_StopAtAnyHit;
		FLOAT ClosestHit = BIG_NUMBER;

		// Iterate over each node.
		for(INT i=0; i<4; i++)
		{
			const INT N = Check.NodeCheckOrder[i];
			// Check its a valid node
			if(n.NodeIndex[N] != NULL_NODE)
			{
				// Check the line hits this node's extended bounds, and check the hit time is not further out than our closest hit so far.
				FLOAT BoundsHitTime;
				const FTerrainBV ExtendedBV(Check.Nodes(n.NodeIndex[N]).BoundingVolume, Check.LocalExtent);
				const UBOOL bBoundsCheck = ExtendedBV.LineCheck(Check, BoundsHitTime);
				if(bBoundsCheck && BoundsHitTime < ClosestHit)
				{
					// Line check against actual node (and subnodes).
					const UBOOL bGotHit = Check.Nodes(n.NodeIndex[N]).BoxCheck(Check);
					if(bGotHit)
					{
						// If we hit - set bHit to TRUE, and update ClosestHit time.
						bHit = TRUE;
						ClosestHit = Min(ClosestHit, Check.Result->Time);
					}
				}

				// If we have a hit, and we don't want the first one - bail out now.
				if (bHit && bStopAtAnyHit)
				{
					return TRUE;
				}
			}
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
 * Works through the list of triangles in this node checking each one for a collision.
 *
 * @param Check -- The aggregated box check data
 */
UBOOL FTerrainBVNode::BoxCheckTriangles(FTerrainBVTreeBoxCollisionCheck& Check) const
{
	// Assume a miss
	UBOOL bHit = 0;
	// Use an early out if possible
	const UBOOL bStopAtAnyHit = Check.TraceFlags & TRACE_StopAtAnyHit;

	const TArray<FVector>& TerrainVerts = Check.TerrainComp->CollisionVertices;
	const INT TerrainSizeX = Check.TerrainComp->TrueSectionSizeX;
	const ATerrain* Terrain = Check.TerrainComp->GetTerrain();

	for(INT Y=Region.YPos; Y<(Region.YPos+Region.YSize) && (bHit == FALSE || bStopAtAnyHit == FALSE); Y++)
	{
		const INT GlobalY = Check.TerrainComp->SectionBaseY + Y;
		for(INT X=Region.XPos; X<(Region.XPos+Region.XSize) && (bHit == FALSE || bStopAtAnyHit == FALSE); X++)
		{
			const INT GlobalX = Check.TerrainComp->SectionBaseX + X;

			INT CheckPatchX = GlobalX - (GlobalX % Terrain->MaxTesselationLevel);
			INT CheckPatchY = GlobalY - (GlobalY % Terrain->MaxTesselationLevel);
			if(Terrain->IsTerrainQuadVisible(CheckPatchX, CheckPatchY))
			{
				if (Terrain->IsTerrainQuadFlipped(GlobalX, GlobalY) == FALSE)
				{
					const FVector& A1 = TerrainVerts(VERT(X+0, Y+0));
					const FVector& A2 = TerrainVerts(VERT(X+0, Y+1));
					const FVector& A3 = TerrainVerts(VERT(X+1, Y+1));

					bHit |= BoxCheckTriangle(Check,A1,A2,A3);

					if(bHit == FALSE || bStopAtAnyHit == FALSE)
					{
						const FVector& B1 = TerrainVerts(VERT(X+0, Y+0));
						const FVector& B2 = TerrainVerts(VERT(X+1, Y+1));
						const FVector& B3 = TerrainVerts(VERT(X+1, Y+0));

						bHit |= BoxCheckTriangle(Check,B1,B2,B3);
					}
				}
				else
				{
					const FVector& A1 = TerrainVerts(VERT(X+0, Y+0));
					const FVector& A2 = TerrainVerts(VERT(X+0, Y+1));
					const FVector& A3 = TerrainVerts(VERT(X+1, Y+0));

					bHit |= BoxCheckTriangle(Check,A1,A2,A3);

					if(bHit == FALSE || bStopAtAnyHit == FALSE)
					{
						const FVector& B1 = TerrainVerts(VERT(X+1, Y+0));
						const FVector& B2 = TerrainVerts(VERT(X+0, Y+1));
						const FVector& B3 = TerrainVerts(VERT(X+1, Y+1));

						bHit |= BoxCheckTriangle(Check,B1,B2,B3);
					}
				}
			}
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
 */
UBOOL FTerrainBVNode::BoxCheckTriangle(FTerrainBVTreeBoxCollisionCheck& Check, const FVector& v1,const FVector& v2,const FVector& v3) const
{
#if DRAW_DEBUG_INFO
	FMatrix L2W = Check.LocalToWorld;
	GWorld->LineBatcher->DrawLine(L2W.TransformFVector(v1), L2W.TransformFVector(v2), FColor(0,0,255), SDPG_World);
	GWorld->LineBatcher->DrawLine(L2W.TransformFVector(v2), L2W.TransformFVector(v3), FColor(0,0,255), SDPG_World);
	GWorld->LineBatcher->DrawLine(L2W.TransformFVector(v3), L2W.TransformFVector(v1), FColor(0,0,255), SDPG_World);
#endif

	FLOAT HitTime = 1.f;
	FVector HitNormal(0.f,0.f,0.f);
	// Now check for an intersection using the Separating Axis Theorem
	UBOOL Result = FindSeparatingAxis(v1,v2,v3,Check.LocalStart,
		Check.LocalEnd,Check.Extent,Check.LocalBoxX,Check.LocalBoxY,
		Check.LocalBoxZ,HitTime,HitNormal);
	if (Result)
	{
		if (HitTime < Check.Result->Time)
		{
			// Store the better time
			Check.Result->Time = HitTime;
			// Get the material that was hit
			Check.Result->Material = NULL;
			// Normal will get transformed to world space at end of check
			Check.LocalHitNormal = HitNormal;
		}
		else
		{
			Result = FALSE;
		}
	}
	return Result;
}

/**
 * Determines the point + extent in the FTerrainBVTreePointCollisionCheck intersects
 * this node. It also will check the child nodes if it is not a leaf, otherwise
 * it will check against the triangle data.
 *
 * @param Check -- The aggregated point check data
 */
UBOOL FTerrainBVNode::PointCheck(FTerrainBVTreePointCollisionCheck& Check) const
{
	UBOOL bHit = FALSE;
	// If this is a node, check the two child nodes recursively
	if (bIsLeaf == 0)
	{
		const UBOOL bStopAtAnyHit = Check.TraceFlags & TRACE_StopAtAnyHit;
		FLOAT ClosestHit = BIG_NUMBER;

		// Iterate over each node.
		for(INT i=0; i<4; i++)
		{
			// Check its a valid node
			if(n.NodeIndex[i] != NULL_NODE)
			{
				// Expand node bounds by check extent
				const FTerrainBV ExtendedBV(Check.Nodes(n.NodeIndex[i]).BoundingVolume, Check.LocalExtent);
				// See if check hits bounds.
				if(ExtendedBV.PointCheck(Check))
				{
					// Point check against actual node (and subnodes).
					bHit |= Check.Nodes(n.NodeIndex[i]).PointCheck(Check);
				}
			}
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
 * Works through the list of triangles in this node checking each one for a collision.
 *
 * @param Check -- The aggregated point check data
 */
UBOOL FTerrainBVNode::PointCheckTriangles(FTerrainBVTreePointCollisionCheck& Check) const
{
	// Assume a miss
	UBOOL bHit = FALSE;

	const TArray<FVector>& TerrainVerts = Check.TerrainComp->CollisionVertices;
	const INT TerrainSizeX = Check.TerrainComp->TrueSectionSizeX;
	const ATerrain* Terrain = Check.TerrainComp->GetTerrain();

	for(INT Y=Region.YPos; Y<(Region.YPos+Region.YSize); Y++)
	{
		INT GlobalY = Check.TerrainComp->SectionBaseY + Y;
		for(INT X=Region.XPos; X<(Region.XPos+Region.XSize); X++)
		{
			INT	GlobalX = Check.TerrainComp->SectionBaseX + X;

			INT CheckPatchX = GlobalX - (GlobalX % Terrain->MaxTesselationLevel);
			INT CheckPatchY = GlobalY - (GlobalY % Terrain->MaxTesselationLevel);
			if(Terrain->IsTerrainQuadVisible(CheckPatchX, CheckPatchY))
			{
				if (Terrain->IsTerrainQuadFlipped(GlobalX, GlobalY) == FALSE)
				{
					const FVector& A1 = TerrainVerts(VERT(X+0, Y+0));
					const FVector& A2 = TerrainVerts(VERT(X+0, Y+1));
					const FVector& A3 = TerrainVerts(VERT(X+1, Y+1));

					bHit |= PointCheckTriangle(Check,A1,A2,A3);

					const FVector& B1 = TerrainVerts(VERT(X+0, Y+0));
					const FVector& B2 = TerrainVerts(VERT(X+1, Y+1));
					const FVector& B3 = TerrainVerts(VERT(X+1, Y+0));

					bHit |= PointCheckTriangle(Check,B1,B2,B3);
				}
				else
				{
					const FVector& A1 = TerrainVerts(VERT(X+0, Y+0));
					const FVector& A2 = TerrainVerts(VERT(X+0, Y+1));
					const FVector& A3 = TerrainVerts(VERT(X+1, Y+0));

					bHit |= PointCheckTriangle(Check,A1,A2,A3);

					const FVector& B1 = TerrainVerts(VERT(X+1, Y+0));
					const FVector& B2 = TerrainVerts(VERT(X+0, Y+1));
					const FVector& B3 = TerrainVerts(VERT(X+1, Y+1));

					bHit |= PointCheckTriangle(Check,B1,B2,B3);
				}
			}
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
 */
UBOOL FTerrainBVNode::PointCheckTriangle(FTerrainBVTreePointCollisionCheck& Check,const FVector& v1,const FVector& v2,const FVector& v3) const
{
	// Use the separating axis theorem to see if we hit
	FSeparatingAxisPointCheck ThePointCheck(v1,v2,v3,Check.LocalStart,Check.Extent,
		Check.LocalBoxX,Check.LocalBoxY,Check.LocalBoxZ,Check.BestDistance);

	// If we hit and it is closer update the out values
	if (ThePointCheck.Hit && ThePointCheck.BestDist < Check.BestDistance)
	{
		// Get the material that was hit
		Check.Result->Material = NULL;
		// Normal will get transformed to world space at end of check
		Check.LocalHitNormal = ThePointCheck.HitNormal;
		// Copy the distance for push out calculations
		Check.BestDistance = ThePointCheck.BestDist;
		return TRUE;
	}
	return FALSE;
}


/************************************************************************/
/* FTerrainBVTree  */
/************************************************************************/

void FTerrainBVTree::Build(const UTerrainComponent* TerrainComp)
{
	const TArray<FVector>& TerrainVerts = TerrainComp->CollisionVertices;
	const INT TerrainSizeX = TerrainComp->TrueSectionSizeX;
	const INT TerrainSizeY = TerrainComp->TrueSectionSizeY;

	// Size should be in quads - so number of verts should be this:
	check(TerrainVerts.Num() == ((TerrainSizeX+1)*(TerrainSizeY+1)));

	// Empty the current set of nodes and preallocate the memory so it doesn't
	// reallocate memory while we are recursively walking the tree
	Nodes.Empty(TerrainSizeX * TerrainSizeY * 2);
	// Add the root node
	Nodes.Add();

	// Now tell that node to recursively subdivide the entire terrain
	FTerrainSubRegion Region;
	Region.XPos = 0;
	Region.YPos = 0;
	Region.XSize = TerrainSizeX;
	Region.YSize = TerrainSizeY;
	
	Nodes(0).SplitTerrain(Region, TerrainComp, Nodes);

	// Don't waste memory.
 	Nodes.Shrink();
}


UBOOL FTerrainBVTree::LineCheck(FTerrainBVTreeLineCollisionCheck& Check) const
{
	UBOOL bHit = FALSE;

	if(Nodes.Num() == 0)
	{
		return bHit;
	}

	FLOAT HitTime;
	// Check against the first bounding volume and decide whether to go further
	if (Nodes(0).BoundingVolume.LineCheck(Check,HitTime))
	{
		// Recursively check for a hit
		bHit = Nodes(0).LineCheck(Check);
	}
	return bHit;
}

UBOOL FTerrainBVTree::BoxCheck(FTerrainBVTreeBoxCollisionCheck& Check) const
{
	UBOOL bHit = FALSE;

	if(Nodes.Num() == 0)
	{
		return bHit;
	}

	FLOAT HitTime;
	// Check the root node's bounding volume expanded by the extent
	const FTerrainBV BV(Nodes(0).BoundingVolume, Check.LocalExtent);
	// Check against the first bounding volume and decide whether to go further
	if (BV.LineCheck(Check,HitTime))
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
UBOOL FTerrainBVTree::PointCheck(FTerrainBVTreePointCollisionCheck& Check) const
{
	UBOOL bHit = FALSE;

	if(Nodes.Num() == 0)
	{
		return bHit;
	}

	// Check the root node's bounding volume expanded by the extent
	const FTerrainBV BV(Nodes(0).BoundingVolume, Check.LocalExtent);
	// Check against the first bounding volume and decide whether to go further
	if (BV.PointCheck(Check))
	{
		// Recursively check for a hit
		bHit = Nodes(0).PointCheck(Check);
	}
	return bHit;
}


/************************************************************************/
/* FTerrainBVTreeCollisionCheck */
/************************************************************************/

FTerrainBVTreeCollisionCheck::FTerrainBVTreeCollisionCheck(const UTerrainComponent* InTerrainComp) :
	TerrainComp(InTerrainComp),	
	BVTree(TerrainComp->BVTree),
	Nodes(BVTree.Nodes)
{
	// Collision data is now stored in world space, and so transforming ray
	// start/end positions and hit points/normals is no longer necessary.
	LocalToWorld = FMatrix::Identity;
	WorldToLocal = FMatrix::Identity;
}

/************************************************************************/
/* FTerrainBVTreeLineCollisionCheck */
/************************************************************************/

FTerrainBVTreeLineCollisionCheck::FTerrainBVTreeLineCollisionCheck(const FVector& InStart,const FVector& InEnd, DWORD InTraceFlags, const UTerrainComponent* InTerrainComp, FCheckResult* InResult) :
	FTerrainBVTreeCollisionCheck(InTerrainComp),
	Result(InResult), 
	Start(InStart), 
	End(InEnd), 
	TraceFlags(InTraceFlags)
{
	// Move start and end to local space
	LocalStart = WorldToLocal.TransformFVector(Start);
	LocalEnd = WorldToLocal.TransformFVector(End);
	// Calculate the vector's direction in local space
	LocalDir = LocalEnd - LocalStart;
	// Build the one over dir
	LocalOneOverDir.X = LocalDir.X ? 1.f / LocalDir.X : 0.f;
	LocalOneOverDir.Y = LocalDir.Y ? 1.f / LocalDir.Y : 0.f;
	LocalOneOverDir.Z = LocalDir.Z ? 1.f / LocalDir.Z : 0.f;
	// Clear the closest hit time
	Result->Time = MAX_FLT;

	// Calculate the best order to walk nodes in.
	if(LocalDir.X > 0)
	{
		if(LocalDir.Y > 0)
		{
			NodeCheckOrder[0] = 0;
			NodeCheckOrder[1] = 1;
			NodeCheckOrder[2] = 2;
			NodeCheckOrder[3] = 3;
		}
		else
		{
			NodeCheckOrder[0] = 2;
			NodeCheckOrder[1] = 0;
			NodeCheckOrder[2] = 3;
			NodeCheckOrder[3] = 1;
		}
	}
	else
	{
		if(LocalDir.Y > 0)
		{
			NodeCheckOrder[0] = 1;
			NodeCheckOrder[1] = 3;
			NodeCheckOrder[2] = 0;
			NodeCheckOrder[3] = 2;
		}
		else
		{
			NodeCheckOrder[0] = 3;
			NodeCheckOrder[1] = 2;
			NodeCheckOrder[2] = 1;
			NodeCheckOrder[3] = 0;
		}
	}

}

/**
 * Transforms the local hit normal into a world space normal using the transpose
 * adjoint and flips the normal if need be.
 */
FVector FTerrainBVTreeLineCollisionCheck::GetHitNormal(void) const
{
	// Transform the hit back into world space using the transpose adjoint
	FVector Normal = LocalToWorld.TransposeAdjoint().TransformNormal(LocalHitNormal).SafeNormal();
	// Flip the normal if the triangle is inverted
	if (LocalToWorld.Determinant() < 0.f)
	{
		Normal = -Normal;
	}
	return Normal;
}

/************************************************************************/
/* FTerrainBVTreeBoxCollisionCheck */
/************************************************************************/

FTerrainBVTreeBoxCollisionCheck::FTerrainBVTreeBoxCollisionCheck(const FVector& InStart,const FVector& InEnd, const FVector& InExtent,DWORD InTraceFlags, const UTerrainComponent* InTerrainComp, FCheckResult* InResult) :
	FTerrainBVTreeLineCollisionCheck(InStart,InEnd,InTraceFlags,InTerrainComp,InResult),
	Extent(InExtent)
{
	// Move extent to local space
	LocalExtent = FBox(-Extent,Extent).TransformBy(WorldToLocal).GetExtent();
	// Transform the PlaneNormals into local space.
	LocalBoxX = WorldToLocal.TransformNormal(FVector(1,0,0));
	LocalBoxY = WorldToLocal.TransformNormal(FVector(0,1,0));
	LocalBoxZ = WorldToLocal.TransformNormal(FVector(0,0,1));
}

/************************************************************************/
/* FTerrainBVTreePointCollisionCheck */
/************************************************************************/

/**
 * @return		The transformed hit location.
 */
FVector FTerrainBVTreePointCollisionCheck::GetHitLocation(void) const
{
	// Push out the hit location from the point along the hit normal and convert into world units
	return LocalToWorld.TransformFVector(LocalStart + LocalHitNormal * BestDistance);
}
