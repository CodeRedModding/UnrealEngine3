/*=============================================================================
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "UnrealEd.h"
#include "GeomTools.h"

/** Util struct for storing a set of planes defining a convex region, with neighbour information. */
struct FHullPlanes
{
	TArray<FPlane>	Planes;
	TArray<INT>		PlaneNeighbour;
	TArray<FLOAT>	PlaneNeighbourArea;

	/** 
	 *	Util for removing redundany planes from the hull. 
	 *	Also calculates area of connection
	 */
	void RemoveRedundantPlanes()
	{
		check(Planes.Num() == PlaneNeighbour.Num());

		// For each plane
		for(INT i=Planes.Num()-1; i>=0; i--)
		{
			// Build big-ass polygon
			FPoly Polygon;
			Polygon.Normal = Planes(i);

			FVector AxisX, AxisY;
			Polygon.Normal.FindBestAxisVectors(AxisX,AxisY);

			const FVector Base = Planes(i) * Planes(i).W;

			new(Polygon.Vertices) FVector(Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);
			new(Polygon.Vertices) FVector(Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX);
			new(Polygon.Vertices) FVector(Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);
			new(Polygon.Vertices) FVector(Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX);

			// Clip away each plane
			for(INT j=0; j<Planes.Num(); j++)
			{
				if(i != j)
				{
					if(!Polygon.Split(-FVector(Planes(j)), Planes(j) * Planes(j).W))
					{
						// At this point we clipped away all the polygon - so this plane is redundant - remove!
						Polygon.Vertices.Empty();

						Planes.Remove(i);
						PlaneNeighbour.Remove(i);

						break;
					}
				}
			}

			// If we still have a polygon left - this is a neighbour - calculate area of connection.
			if(Polygon.Vertices.Num() > 0)
			{
				// Insert at front - we are walking planes from end to start.
				PlaneNeighbourArea.InsertItem(Polygon.Area(), 0);
			}
		}

		check(PlaneNeighbourArea.Num() == PlaneNeighbour.Num());
		check(Planes.Num() == PlaneNeighbour.Num());
	}
};
	
void CreateVoronoiRegions( const FBox& StartBox, const TArray<FVector>& Points, const FVector& PlaneBias, TArray<FVoronoiRegion>& OutRegions )
{
	OutRegions.Empty();

	// Return empty array if input was empty.
	if(Points.Num() == 0)
	{
		return;
	}

	// Initialise output array - one entry for each point. 
	OutRegions.AddZeroed(Points.Num());

	// Init temp storage for hull planes.
	TArray<FHullPlanes> RegionPlanes;
	RegionPlanes.AddZeroed(Points.Num());

	// Initialise first entry giving it entire region
	FKConvexElem& InitChunk = OutRegions(0).ConvexElem;

	FVector BoxExtent, BoxCenter;
	StartBox.GetCenterAndExtents(BoxCenter, BoxExtent);

	FLOAT	Signs[2] = { -1.0f, 1.0f };
	for(INT X = 0;X < 2;X++)
	{
		for(INT Y = 0;Y < 2;Y++)
		{
			for(INT Z = 0;Z < 2;Z++)
			{
				FVector	Corner(Signs[X] * BoxExtent.X, Signs[Y] * BoxExtent.Y, Signs[Z] * BoxExtent.Z);
				Corner += BoxCenter;

				InitChunk.VertexData.AddItem(Corner);
			}
		}
	}
	InitChunk.GenerateHullData();

	FMatrix PlaneBiasTM = FScaleMatrix(PlaneBias);

	// Remember the planes that make up the initial shape.
	TArray<FPlane> InitPlanes = InitChunk.FacePlaneData;
	// Init neighbours are all INDEX_NONE
	TArray<INT> InitNeighbours;
	InitNeighbours.Add(InitPlanes.Num());
	for(INT i=0; i<InitPlanes.Num(); i++)
	{
		InitNeighbours(i) = INDEX_NONE;
	}

	RegionPlanes(0).Planes = InitPlanes;
	RegionPlanes(0).PlaneNeighbour = InitNeighbours;

	// Then add each subsequent chunk - incrementally building the voronoi regions
	// We do this by looking at each existing chunk and seeing if the plane between the new point and their point affects their hull.
	for(INT PointIdx=1; PointIdx<Points.Num(); PointIdx++)
	{
		const FVector& Point = Points(PointIdx); 
		RegionPlanes(PointIdx).Planes = InitPlanes;
		RegionPlanes(PointIdx).PlaneNeighbour = InitNeighbours;

		TArray<FVector> DummySnap;

		// Iterate over each existing chunk
		for(INT TestIdx=0; TestIdx < PointIdx; TestIdx++)
		{
			const FVector& TestP = Points(TestIdx);
			// Find midpoint between points
			const FVector MidP = 0.5f * (Point + TestP);
			// Find normal - normalize vector between points
			FVector PNormal = (TestP - Point);
			// Apply scaling and renormalize
			PNormal = PlaneBiasTM.TransformNormal(PNormal).SafeNormal();
			
			// Splitting plane is then:
			const FPlane SplitPlane(MidP, PNormal);

			// See if the test chunk is completely on the other side of this split plane.
			// If it is, adding this point has no affect on it.
			if(!OutRegions(TestIdx).ConvexElem.IsOutsidePlane(SplitPlane))
			{
				// If this plane does affect the test chunk, add plane to both chunks
				RegionPlanes(TestIdx).Planes.AddItem(SplitPlane.Flip());
				RegionPlanes(TestIdx).PlaneNeighbour.AddItem(PointIdx);

				RegionPlanes(PointIdx).Planes.AddItem(SplitPlane);
				RegionPlanes(PointIdx).PlaneNeighbour.AddItem( TestIdx );

				// Rebuild test chunk - so verts are up to date for testing.
				OutRegions(TestIdx).ConvexElem.HullFromPlanes(RegionPlanes(TestIdx).Planes, DummySnap);
			}
		}

		// Now we finalise this chunk from all its planes.
		OutRegions(PointIdx).ConvexElem.HullFromPlanes(RegionPlanes(PointIdx).Planes, DummySnap);
	}

	// Remove redundant neighbours and copy info into output voronoi regions.
	check(OutRegions.Num() == RegionPlanes.Num());
	for(INT RegionIdx=0; RegionIdx<RegionPlanes.Num(); RegionIdx++)
	{
		// Remove redundant planes, calc neighbour connection info
		RegionPlanes(RegionIdx).RemoveRedundantPlanes();
		// Use plane info from voronoi, rather than after convex processing.
		OutRegions(RegionIdx).ConvexElem.FacePlaneData = RegionPlanes(RegionIdx).Planes;
		// Copy neighbour information
		OutRegions(RegionIdx).Neighbours = RegionPlanes(RegionIdx).PlaneNeighbour;
		OutRegions(RegionIdx).NeighbourDims = RegionPlanes(RegionIdx).PlaneNeighbourArea;
		check(OutRegions(RegionIdx).Neighbours.Num() == OutRegions(RegionIdx).ConvexElem.FacePlaneData.Num());
	}
}

/** */
static FClipSMVertex GetVert(const UStaticMesh* StaticMesh, INT VertIndex)
{
	FClipSMVertex Result;
	Result.Pos = StaticMesh->LODModels(0).PositionVertexBuffer.VertexPosition(VertIndex);
	Result.TangentX = StaticMesh->LODModels(0).VertexBuffer.VertexTangentX(VertIndex);
	Result.TangentY = StaticMesh->LODModels(0).VertexBuffer.VertexTangentY(VertIndex);
	Result.TangentZ = StaticMesh->LODModels(0).VertexBuffer.VertexTangentZ(VertIndex);
	const INT NumUVs = StaticMesh->LODModels(0).VertexBuffer.GetNumTexCoords();
	for(INT UVIndex = 0;UVIndex < NumUVs;UVIndex++)
	{
		Result.UVs[UVIndex] = StaticMesh->LODModels(0).VertexBuffer.GetVertexUV(VertIndex,UVIndex);
	}
	for(INT UVIndex = NumUVs;UVIndex < ARRAY_COUNT(Result.UVs);UVIndex++)
	{
		Result.UVs[UVIndex] = FVector2D(0,0);
	}
	Result.Color = FColor( 255, 255, 255 );
	if( StaticMesh->LODModels(0).ColorVertexBuffer.GetNumVertices() > 0 )
	{
		Result.Color = StaticMesh->LODModels(0).ColorVertexBuffer.VertexColor(VertIndex);
	}
	return Result;
}

/** */
static void SetVert(FStaticMeshTriangle* Tri, INT VertIndex, const FClipSMVertex& Vert)
{
	check(VertIndex>=0 && VertIndex<3);

	Tri->Vertices[VertIndex] = Vert.Pos;
	for(INT i=0; i<8; i++)
	{
		Tri->UVs[VertIndex][i] = Vert.UVs[i];
	}
	Tri->Colors[VertIndex] = Vert.Color;
	Tri->TangentX[VertIndex] = Vert.TangentX;
	Tri->TangentY[VertIndex] = Vert.TangentY;
	Tri->TangentZ[VertIndex] = Vert.TangentZ;
}

/** Take two static mesh verts and interpolate all values between them */
FClipSMVertex InterpolateVert(const FClipSMVertex& V0, const FClipSMVertex& V1, FLOAT Alpha)
{
	FClipSMVertex Result;

	// Handle dodgy alpha
	if(appIsNaN(Alpha) || !appIsFinite(Alpha))
	{
		Result = V1;
		return Result;
	}

	Result.Pos = Lerp(V0.Pos, V1.Pos, Alpha);
	Result.TangentX = Lerp(V0.TangentX,V1.TangentX,Alpha);
	Result.TangentY = Lerp(V0.TangentY,V1.TangentY,Alpha);
	Result.TangentZ = Lerp(V0.TangentZ,V1.TangentZ,Alpha);
	for(INT i=0; i<8; i++)
	{
		Result.UVs[i] = Lerp(V0.UVs[i], V1.UVs[i], Alpha);
	}
	
	Result.Color.R = Clamp( appTrunc(Lerp(FLOAT(V0.Color.R), FLOAT(V1.Color.R), Alpha)), 0, 255 );
	Result.Color.G = Clamp( appTrunc(Lerp(FLOAT(V0.Color.G), FLOAT(V1.Color.G), Alpha)), 0, 255 );
	Result.Color.B = Clamp( appTrunc(Lerp(FLOAT(V0.Color.B), FLOAT(V1.Color.B), Alpha)), 0, 255 );
	Result.Color.A = Clamp( appTrunc(Lerp(FLOAT(V0.Color.A), FLOAT(V1.Color.A), Alpha)), 0, 255 );
	return Result;
}

/** Extracts the triangles from a static-mesh as clippable triangles. */
void GetClippableStaticMeshTriangles(TArray<FClipSMTriangle>& OutClippableTriangles,const UStaticMesh* StaticMesh)
{
	const FStaticMeshRenderData& RenderData = StaticMesh->LODModels(0);
	for(INT ElementIndex = 0;ElementIndex < RenderData.Elements.Num();ElementIndex++)
	{
		const FStaticMeshElement& Element = RenderData.Elements(ElementIndex);
		for(INT FragmentIndex = 0;FragmentIndex < Element.Fragments.Num();FragmentIndex++)
		{
			const FFragmentRange& FragmentRange = Element.Fragments(FragmentIndex);
			for(INT TriangleIndex = 0;TriangleIndex < FragmentRange.NumPrimitives;TriangleIndex++)
			{
				FClipSMTriangle ClipTriangle(0);

				// Copy the triangle's attributes.
				ClipTriangle.FragmentIndex = FragmentIndex;
				ClipTriangle.MaterialIndex = Element.MaterialIndex;
				ClipTriangle.NumUVs = RenderData.VertexBuffer.GetNumTexCoords();
				ClipTriangle.SmoothingMask = 0;
				ClipTriangle.bOverrideTangentBasis = TRUE;

				// Extract the vertices for this triangle.
				const UINT BaseIndex = FragmentRange.BaseIndex + TriangleIndex * 3;
				for(UINT TriangleVertexIndex = 0;TriangleVertexIndex < 3;TriangleVertexIndex++)
				{
					const UINT VertexIndex = RenderData.IndexBuffer.Indices(BaseIndex + TriangleVertexIndex);
					ClipTriangle.Vertices[TriangleVertexIndex] = GetVert(StaticMesh,VertexIndex);
				}

				// Compute the triangle's gradients and normal.
				ClipTriangle.ComputeGradientsAndNormal();

				// Add the triangle to the output array.
				OutClippableTriangles.AddItem(ClipTriangle);
			}
		}
	}
}

/** Extracts the triangles from a static-mesh as clippable triangles. */
void GetRawStaticMeshTrianglesFromClipped(TArray<FStaticMeshTriangle>& OutStaticMeshTriangles,const TArray<FClipSMTriangle>& ClippedTriangles)
{
	for(INT TriangleIndex = 0;TriangleIndex < ClippedTriangles.Num();TriangleIndex++)
	{
		const FClipSMTriangle& ClipTriangle = ClippedTriangles(TriangleIndex);
		FStaticMeshTriangle RawTriangle;

		// Copy the triangle's attributes.
		RawTriangle.FragmentIndex = ClipTriangle.FragmentIndex;
		RawTriangle.MaterialIndex = ClipTriangle.MaterialIndex;
		RawTriangle.NumUVs = ClipTriangle.NumUVs;
		RawTriangle.SmoothingMask = ClipTriangle.SmoothingMask;
		RawTriangle.bOverrideTangentBasis = ClipTriangle.bOverrideTangentBasis;
		RawTriangle.bExplicitNormals = FALSE;

		// Copy the triangle's vertices.
		for(UINT TriangleVertexIndex = 0;TriangleVertexIndex < 3;TriangleVertexIndex++)
		{
			SetVert(&RawTriangle,TriangleVertexIndex,ClipTriangle.Vertices[TriangleVertexIndex]);
		}

		// Add the triangle to the output array.
		OutStaticMeshTriangles.AddItem(RawTriangle);
	}
}

/** Take the input mesh and cut it with supplied plane, creating new verts etc. Also outputs new edges created on the plane. */
void ClipMeshWithPlane( TArray<FClipSMTriangle>& OutTris, TArray<FUtilEdge3D>& OutClipEdges, const TArray<FClipSMTriangle>& InTris, const FPlane& Plane )
{
	// Iterate over each source triangle
	for(INT TriIdx=0; TriIdx<InTris.Num(); TriIdx++)
	{
		const FClipSMTriangle* SrcTri = &InTris(TriIdx);

		// Calculate which verts are beyond clipping plane
		FLOAT PlaneDist[3];
		for(INT i=0; i<3; i++)
		{
			PlaneDist[i] = Plane.PlaneDot(SrcTri->Vertices[i].Pos);
		}

		TArray<FClipSMVertex> FinalVerts;
		FUtilEdge3D NewClipEdge;
		INT ClippedEdges = 0;

		for(INT EdgeIdx = 0; EdgeIdx < 3; EdgeIdx++)
		{
			INT ThisVert = EdgeIdx;

			// If start vert is inside, add it.
			if(IsNegativeFloat(PlaneDist[ThisVert]))
			{
				FinalVerts.AddItem( SrcTri->Vertices[ThisVert] );
			}

			// If start and next vert are on opposite sides, add intersection
			INT NextVert = (EdgeIdx+1)%3;

			if(IsNegativeFloat(PlaneDist[EdgeIdx]) != IsNegativeFloat(PlaneDist[NextVert]))
			{
				// Find distance along edge that plane is
				FLOAT Alpha = -PlaneDist[ThisVert] / (PlaneDist[NextVert] - PlaneDist[ThisVert]);
				// Interpolate vertex params to that point
				FClipSMVertex InterpVert = InterpolateVert(SrcTri->Vertices[ThisVert], SrcTri->Vertices[NextVert], Clamp(Alpha,0.0f,1.0f));
				// Save vert
				FinalVerts.AddItem(InterpVert);

				// When we make a new edge on the surface of the clip plane, save it off.
				if(ClippedEdges == 0)
				{
					NewClipEdge.V0 = InterpVert.Pos;
				}
				else
				{
					NewClipEdge.V1 = InterpVert.Pos;
				}
				ClippedEdges++;
			}
		}

		// Triangulate the clipped polygon.
		for(INT VertexIndex = 2;VertexIndex < FinalVerts.Num();VertexIndex++)
		{
			FClipSMTriangle NewTri = *SrcTri;
			NewTri.Vertices[0] = FinalVerts(0);
			NewTri.Vertices[1] = FinalVerts(VertexIndex - 1);
			NewTri.Vertices[2] = FinalVerts(VertexIndex);
			NewTri.bOverrideTangentBasis = TRUE;
			OutTris.AddItem(NewTri);
		}

		// If we created a new edge, save that off here as well
		if(ClippedEdges == 2)
		{
			OutClipEdges.AddItem(NewClipEdge);
		}
	}
}

/** Take a set of 3D Edges and project them onto the supplied plane. Also returns matrix use to convert them back into 3D edges. */
void ProjectEdges( TArray<FUtilEdge2D>& Out2DEdges, FMatrix& ToWorld, const TArray<FUtilEdge3D>& In3DEdges, const FPlane& InPlane )
{
	// Build matrix to transform verts into plane space
	FVector BasisX, BasisY, BasisZ;
	BasisZ = InPlane;
	BasisZ.FindBestAxisVectors(BasisX, BasisY);
	ToWorld = FMatrix( BasisX, BasisY, InPlane, BasisZ * InPlane.W );

	Out2DEdges.Add( In3DEdges.Num() );
	for(INT i=0; i<In3DEdges.Num(); i++)
	{
		FVector P = ToWorld.InverseTransformFVector(In3DEdges(i).V0);
		Out2DEdges(i).V0.X = P.X;
		Out2DEdges(i).V0.Y = P.Y;

		P = ToWorld.InverseTransformFVector(In3DEdges(i).V1);
		Out2DEdges(i).V1.X = P.X;
		Out2DEdges(i).V1.Y = P.Y;
	}
}

/** End of one edge and start of next must be less than this to connect them. */
static FLOAT EdgeMatchTolerance = 0.01f;

/** Util to look for best next edge start from Start. Returns FALSE if no good next edge found. Edge is removed from InEdgeSet when found. */
static UBOOL FindNextEdge(FUtilEdge2D& OutNextEdge, const FVector2D& Start, TArray<FUtilEdge2D>& InEdgeSet)
{
	FLOAT ClosestDist = BIG_NUMBER;
	FUtilEdge2D OutEdge;
	INT OutEdgeIndex = INDEX_NONE;
	// Search set of edges for one that starts closest to Start
	for(INT i=0; i<InEdgeSet.Num(); i++)
	{
		FLOAT Dist = (InEdgeSet(i).V0 - Start).Size();
		if(Dist < ClosestDist)
		{
			ClosestDist = Dist;
			OutNextEdge = InEdgeSet(i);
			OutEdgeIndex = i;
		}

		Dist = (InEdgeSet(i).V1 - Start).Size();
		if(Dist < ClosestDist)
		{
			ClosestDist = Dist;
			OutNextEdge = InEdgeSet(i);
			Swap(OutNextEdge.V0, OutNextEdge.V1);
			OutEdgeIndex = i;
		}
	}

	// If next edge starts close enough return it
	if(ClosestDist < EdgeMatchTolerance)
	{
		check(OutEdgeIndex != INDEX_NONE);
		InEdgeSet.Remove(OutEdgeIndex);
		return TRUE;
	}
	// No next edge found.
	else
	{
		return FALSE;
	}
}

/** 
 *	Make sure that polygon winding is always consistent - cross product between successive edges is positive. 
 *	This function also remove co-linear edges.
 */
static void FixPolyWinding(FUtilPoly2D& Poly)
{
	FLOAT TotalAngle = 0.f;
	for(INT i=Poly.Verts.Num()-1; i>=0; i--)
	{
		// Triangle is 'this' vert plus the one before and after it
		INT AIndex = (i==0) ? Poly.Verts.Num()-1 : i-1;
		INT BIndex = i;
		INT CIndex = (i+1)%Poly.Verts.Num();

		FLOAT ABDist = (Poly.Verts(BIndex).Pos - Poly.Verts(AIndex).Pos).Size();
		FVector2D ABEdge = (Poly.Verts(BIndex).Pos - Poly.Verts(AIndex).Pos).SafeNormal();

		FLOAT BCDist = (Poly.Verts(CIndex).Pos - Poly.Verts(BIndex).Pos).Size();
		FVector2D BCEdge = (Poly.Verts(CIndex).Pos - Poly.Verts(BIndex).Pos).SafeNormal();

		// See if points are co-incident or edges are co-linear - if so, remove.
		if(ABDist < 0.01f || BCDist < 0.01f || ABEdge.Equals(BCEdge, 0.01f))
		{
			Poly.Verts.Remove(i);
		}
		else
		{
			TotalAngle += appAsin(ABEdge ^ BCEdge);
		}
	}

	// If total angle is negative, reverse order.
	if(TotalAngle < 0.f)
	{
		INT NumVerts = Poly.Verts.Num();

		TArray<FUtilVertex2D> NewVerts;
		NewVerts.Add(NumVerts);

		for(INT i=0; i<NumVerts; i++)
		{
			NewVerts(i) = Poly.Verts(NumVerts-(1+i));
		}
		Poly.Verts = NewVerts;
	}
}

/** Given a set of edges, find the set of closed polygons created by them. */
void Buid2DPolysFromEdges(TArray<FUtilPoly2D>& OutPolys, const TArray<FUtilEdge2D>& InEdges, const FColor& VertColor)
{
	TArray<FUtilEdge2D> EdgeSet = InEdges;

	// While there are still edges to process..
	while(EdgeSet.Num() > 0)
	{
		// Initialise new polygon with the first edge in the set
		FUtilPoly2D NewPoly;
		FUtilEdge2D FirstEdge = EdgeSet.Pop();

		NewPoly.Verts.AddItem(FUtilVertex2D(FirstEdge.V0, VertColor));
		NewPoly.Verts.AddItem(FUtilVertex2D(FirstEdge.V1, VertColor));

		// Now we keep adding edges until we can't find any more
		FVector2D PolyEnd = NewPoly.Verts( NewPoly.Verts.Num()-1 ).Pos;
		FUtilEdge2D NextEdge;
		while( FindNextEdge(NextEdge, PolyEnd, EdgeSet) )
		{
			NewPoly.Verts.AddItem(FUtilVertex2D(NextEdge.V1, VertColor));
			PolyEnd = NewPoly.Verts( NewPoly.Verts.Num()-1 ).Pos;
		}

		// After walking edges see if we have a closed polygon.
		FLOAT CloseDist = (NewPoly.Verts(0).Pos - NewPoly.Verts( NewPoly.Verts.Num()-1 ).Pos).Size();
		if(NewPoly.Verts.Num() >= 4 && CloseDist < EdgeMatchTolerance)
		{
			// Remove last vert - its basically a duplicate of the first.
			NewPoly.Verts.Remove( NewPoly.Verts.Num()-1 );

			// Make sure winding is correct.
			FixPolyWinding(NewPoly);

			// Add to set of output polys.
			OutPolys.AddItem(NewPoly);
		}
	}
}

/** Given three direction vectors, indicates if A and B are on the same 'side' of Vec. */
static UBOOL VectorsOnSameSide(const FVector& Vec, const FVector& A, const FVector& B)
{
	const FVector CrossA = Vec ^ A;
	const FVector CrossB = Vec ^ B;
	return !IsNegativeFloat(CrossA | CrossB);
}

/** Util to see if P lies within triangle created by A, B and C. */
static UBOOL PointInTriangle(const FVector& A, const FVector& B, const FVector& C, const FVector& P)
{
	// Cross product indicates which 'side' of the vector the point is on
	// If its on the same side as the remaining vert for all edges, then its inside.	
	if( VectorsOnSameSide(B-A, P-A, C-A) &&
		VectorsOnSameSide(C-B, P-B, A-B) &&
		VectorsOnSameSide(A-C, P-C, B-C) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** Compare all aspects of two verts of two triangles to see if they are the same. */
static UBOOL VertsAreEqual(const FClipSMVertex& A,const FClipSMVertex& B)
{
	if( !A.Pos.Equals(B.Pos, THRESH_POINTS_ARE_SAME) )
	{
		return FALSE;
	}

	if( !A.TangentX.Equals(B.TangentX, THRESH_NORMALS_ARE_SAME) )
	{
		return FALSE;
	}
	
	if( !A.TangentY.Equals(B.TangentY, THRESH_NORMALS_ARE_SAME) )
	{
		return FALSE;
	}
	
	if( !A.TangentZ.Equals(B.TangentZ, THRESH_NORMALS_ARE_SAME) )
	{
		return FALSE;
	}

	if( A.Color != B.Color )
	{
		return FALSE;
	}

	for(INT i=0; i<ARRAY_COUNT(A.UVs); i++)
	{
		if( !A.UVs[i].Equals(B.UVs[i], 1.0f / 1024.0f) )
		{
			return FALSE;
		}
	}

	return TRUE;
}

/** Determines whether two edges may be merged. */
static UBOOL AreEdgesMergeable(
	const FClipSMVertex& V0,
	const FClipSMVertex& V1,
	const FClipSMVertex& V2
	)
{
	const FVector MergedEdgeVector = V2.Pos - V0.Pos;
	const FLOAT MergedEdgeLengthSquared = MergedEdgeVector.SizeSquared();
	if(MergedEdgeLengthSquared > DELTA)
	{
		// Find the vertex closest to A1/B0 that is on the hypothetical merged edge formed by A0-B1.
		const FLOAT IntermediateVertexEdgeFraction =
			((V2.Pos - V0.Pos) | (V1.Pos - V0.Pos)) / MergedEdgeLengthSquared;
		const FClipSMVertex InterpolatedVertex = InterpolateVert(V0,V2,IntermediateVertexEdgeFraction);

		// The edges are merge-able if the interpolated vertex is close enough to the intermediate vertex.
		return VertsAreEqual(InterpolatedVertex,V1);
	}
	else
	{
		return TRUE;
	}
}

/** Given a polygon, decompose into triangles and append to OutTris. */
UBOOL TriangulatePoly(TArray<FClipSMTriangle>& OutTris, const FClipSMPolygon& InPoly)
{
	// Can't work if not enough verts for 1 triangle
	if(InPoly.Vertices.Num() < 3)
	{
		// Return true because poly is already a tri
		return TRUE;
	}

	// Vertices of polygon in order - make a copy we are going to modify.
	TArray<FClipSMVertex> PolyVerts = InPoly.Vertices;

	// Keep iterating while there are still vertices
	while(TRUE)
	{
		// Cull redundant vertex edges from the polygon.
		for(INT VertexIndex = 0;VertexIndex < PolyVerts.Num();VertexIndex++)
		{
			const INT I0 = (VertexIndex + 0) % PolyVerts.Num();
			const INT I1 = (VertexIndex + 1) % PolyVerts.Num();
			const INT I2 = (VertexIndex + 2) % PolyVerts.Num();
			if(AreEdgesMergeable(PolyVerts(I0),PolyVerts(I1),PolyVerts(I2)))
			{
				PolyVerts.Remove(I1);
				VertexIndex--;
			}
		}

		if(PolyVerts.Num() < 3)
		{
			break;
		}
		else
		{
			// Look for an 'ear' triangle
			UBOOL bFoundEar = FALSE;
			for(INT EarVertexIndex = 0;EarVertexIndex < PolyVerts.Num();EarVertexIndex++)
			{
				// Triangle is 'this' vert plus the one before and after it
				const INT AIndex = (EarVertexIndex==0) ? PolyVerts.Num()-1 : EarVertexIndex-1;
				const INT BIndex = EarVertexIndex;
				const INT CIndex = (EarVertexIndex+1)%PolyVerts.Num();

				// Check that this vertex is convex (cross product must be positive)
				const FVector ABEdge = PolyVerts(BIndex).Pos - PolyVerts(AIndex).Pos;
				const FVector ACEdge = PolyVerts(CIndex).Pos - PolyVerts(AIndex).Pos;
				const FLOAT TriangleDeterminant = (ABEdge ^ ACEdge) | InPoly.FaceNormal;
				if(IsNegativeFloat(TriangleDeterminant))
				{
					continue;
				}

				UBOOL bFoundVertInside = FALSE;
				// Look through all verts before this in array to see if any are inside triangle
				for(INT VertexIndex = 0;VertexIndex < PolyVerts.Num();VertexIndex++)
				{
					if(	VertexIndex != AIndex && VertexIndex != BIndex && VertexIndex != CIndex &&
						PointInTriangle(PolyVerts(AIndex).Pos, PolyVerts(BIndex).Pos, PolyVerts(CIndex).Pos, PolyVerts(VertexIndex).Pos) )
					{
						bFoundVertInside = TRUE;
						break;
					}
				}

				// Triangle with no verts inside - its an 'ear'! 
				if(!bFoundVertInside)
				{
					// Add to output list..
					FClipSMTriangle NewTri(0);
					NewTri.CopyFace(InPoly);
					NewTri.Vertices[0] = PolyVerts(AIndex);
					NewTri.Vertices[1] = PolyVerts(BIndex);
					NewTri.Vertices[2] = PolyVerts(CIndex);
					OutTris.AddItem(NewTri);

					// And remove vertex from polygon
					PolyVerts.Remove(EarVertexIndex);

					bFoundEar = TRUE;
					break;
				}
			}

			// If we couldn't find an 'ear' it indicates something is bad with this polygon - discard triangles and return.
			if(!bFoundEar)
			{
				debugf(TEXT("Triangulation of poly failed."));
				OutTris.Empty();
				return FALSE;
			}
		}
	}

	return TRUE;
}


/** Transform triangle from 2D to 3D static-mesh triangle. */
FClipSMPolygon Transform2DPolygonToSMPolygon(const FUtilPoly2D& InPoly, const FMatrix& InMatrix)
{
	FClipSMPolygon Result(0);

	for(INT VertexIndex = 0;VertexIndex < InPoly.Verts.Num();VertexIndex++)
	{
		const FUtilVertex2D& InVertex = InPoly.Verts(VertexIndex);

		FClipSMVertex* OutVertex = new(Result.Vertices) FClipSMVertex;
		appMemzero(OutVertex,sizeof(*OutVertex));
		OutVertex->Pos = InMatrix.TransformFVector( FVector(InVertex.Pos.X, InVertex.Pos.Y, 0.f) );
		OutVertex->Color = InVertex.Color;
		OutVertex->UVs[0] = InVertex.UV;
	}

	// Assume that the matrix defines the polygon's normal.
	Result.FaceNormal = InMatrix.TransformNormal(FVector(0,0,-1)).SafeNormal();

	return Result;
}

/** Does a simple box map onto this 2D polygon. */
void GeneratePolyUVs(FUtilPoly2D& Polygon)
{
	// First work out 2D bounding box for tris.
	FVector2D Min(BIG_NUMBER, BIG_NUMBER);
	FVector2D Max(-BIG_NUMBER, -BIG_NUMBER);
	for(INT VertexIndex = 0;VertexIndex < Polygon.Verts.Num();VertexIndex++)
	{
		const FUtilVertex2D& Vertex = Polygon.Verts(VertexIndex);
		Min.X = ::Min(Vertex.Pos.X, Min.X);
		Min.Y = ::Min(Vertex.Pos.Y, Min.Y);
		Max.X = ::Max(Vertex.Pos.X, Max.X);
		Max.Y = ::Max(Vertex.Pos.Y, Max.Y);
	}

	const FVector2D Extent = Max - Min;

	// Then use this to generate UVs
	for(INT VertexIndex = 0;VertexIndex < Polygon.Verts.Num();VertexIndex++)
	{
		FUtilVertex2D& Vertex = Polygon.Verts(VertexIndex);
		Vertex.UV.X = (Vertex.Pos.X - Min.X)/Extent.X;
		Vertex.UV.Y = (Vertex.Pos.Y - Min.Y)/Extent.Y;
	}
}


/** Computes a transform from triangle parameter space into the space defined by an attribute that varies on the triangle's surface. */
static FMatrix ComputeTriangleParameterToAttribute(
	const FVector AttributeV0,
	const FVector AttributeV1,
	const FVector AttributeV2
	)
{
	const FVector AttributeOverS = AttributeV1 - AttributeV0;
	const FVector AttributeOverT = AttributeV2 - AttributeV0;
	const FVector AttributeOverNormal = (AttributeOverS ^ AttributeOverT).SafeNormal();
	return FMatrix(
		FPlane(	AttributeOverS.X,		AttributeOverS.Y,		AttributeOverS.Z,		0	),
		FPlane(	AttributeOverT.X,		AttributeOverT.Y,		AttributeOverT.Z,		0	),
		FPlane(	AttributeOverNormal.X,	AttributeOverNormal.Y,	AttributeOverNormal.Z,	0	),
		FPlane(	0,						0,						0,						1	)
		);
}

/** Converts a color into a vector. */
static FVector ColorToVector(const FLinearColor& Color)
{
	return FVector(Color.R,Color.G,Color.B);
}

void FClipSMTriangle::ComputeGradientsAndNormal()
{
	// Compute the transform from triangle parameter space to local space.
	const FMatrix ParameterToLocal = ComputeTriangleParameterToAttribute(Vertices[0].Pos,Vertices[1].Pos,Vertices[2].Pos);
	const FMatrix LocalToParameter = ParameterToLocal.InverseSafe();

	// Compute the triangle's normal.
	FaceNormal = ParameterToLocal.TransformNormal(FVector(0,0,1));

	// Compute the normal's gradient in local space.
	const FMatrix ParameterToTangentX = ComputeTriangleParameterToAttribute(Vertices[0].TangentX,Vertices[1].TangentX,Vertices[2].TangentX);
	const FMatrix ParameterToTangentY = ComputeTriangleParameterToAttribute(Vertices[0].TangentY,Vertices[1].TangentY,Vertices[2].TangentY);
	const FMatrix ParameterToTangentZ = ComputeTriangleParameterToAttribute(Vertices[0].TangentZ,Vertices[1].TangentZ,Vertices[2].TangentZ);
	TangentXGradient = LocalToParameter * ParameterToTangentX;
	TangentYGradient = LocalToParameter * ParameterToTangentY;
	TangentZGradient = LocalToParameter * ParameterToTangentZ;

	// Compute the color's gradient in local space.
	const FVector Color0 = ColorToVector(Vertices[0].Color);
	const FVector Color1 = ColorToVector(Vertices[1].Color);
	const FVector Color2 = ColorToVector(Vertices[2].Color);
	const FMatrix ParameterToColor = ComputeTriangleParameterToAttribute(Color0,Color1,Color2);
	ColorGradient = LocalToParameter * ParameterToColor;

	for(INT UVIndex = 0;UVIndex < NumUVs;UVIndex++)
	{
		// Compute the UV's gradient in local space.
		const FVector UV0(Vertices[0].UVs[UVIndex],0);
		const FVector UV1(Vertices[1].UVs[UVIndex],0);
		const FVector UV2(Vertices[2].UVs[UVIndex],0);
		const FMatrix ParameterToUV = ComputeTriangleParameterToAttribute(UV0,UV1,UV2);
		UVGradient[UVIndex] = LocalToParameter * ParameterToUV;
	}
}

/** Util that tries to combine two triangles if possible. */
static UBOOL MergeTriangleIntoPolygon(
	FClipSMPolygon& Polygon,
	const FClipSMTriangle& Triangle)
{
	// The triangles' attributes must match the polygon.
	if(Polygon.MaterialIndex != Triangle.MaterialIndex)
	{
		return FALSE;
	}
	if(Polygon.bOverrideTangentBasis != Triangle.bOverrideTangentBasis)
	{
		return FALSE;
	}
	if(Polygon.NumUVs != Triangle.NumUVs)
	{
		return FALSE;
	}
	if(!Polygon.bOverrideTangentBasis && Polygon.SmoothingMask != Triangle.SmoothingMask)
	{
		return FALSE;
	}

	// The triangle must have the same normal as the polygon
	if(!Triangle.FaceNormal.Equals(Polygon.FaceNormal,THRESH_NORMALS_ARE_SAME))
	{
		return FALSE;
	}

	// The triangle must have the same attribute gradients as the polygon
	if(!Triangle.TangentXGradient.Equals(Polygon.TangentXGradient))
	{
		return FALSE;
	}
	if(!Triangle.TangentYGradient.Equals(Polygon.TangentYGradient))
	{
		return FALSE;
	}
	if(!Triangle.TangentZGradient.Equals(Polygon.TangentZGradient))
	{
		return FALSE;
	}
	if(!Triangle.ColorGradient.Equals(Polygon.ColorGradient))
	{
		return FALSE;
	}
	for(INT UVIndex = 0;UVIndex < Triangle.NumUVs;UVIndex++)
	{
		if(!Triangle.UVGradient[UVIndex].Equals(Polygon.UVGradient[UVIndex]))
		{
			return FALSE;
		}
	}

	for(INT PolygonEdgeIndex = 0;PolygonEdgeIndex < Polygon.Vertices.Num();PolygonEdgeIndex++)
	{
		const UINT PolygonEdgeVertex0 = PolygonEdgeIndex + 0;
		const UINT PolygonEdgeVertex1 = (PolygonEdgeIndex + 1) % Polygon.Vertices.Num();

		for(UINT TriangleEdgeIndex = 0;TriangleEdgeIndex < 3;TriangleEdgeIndex++)
		{
			const UINT TriangleEdgeVertex0 = TriangleEdgeIndex + 0;
			const UINT TriangleEdgeVertex1 = (TriangleEdgeIndex + 1) % 3;

			// If the triangle and polygon share an edge, then the triangle is in the same plane (implied by the above normal check),
			// and may be merged into the polygon.
			if(	VertsAreEqual(Polygon.Vertices(PolygonEdgeVertex0),Triangle.Vertices[TriangleEdgeVertex1]) &&
				VertsAreEqual(Polygon.Vertices(PolygonEdgeVertex1),Triangle.Vertices[TriangleEdgeVertex0]))
			{
				// Add the triangle's vertex that isn't in the adjacent edge to the polygon in between the vertices of the adjacent edge.
				const INT TriangleOppositeVertexIndex = (TriangleEdgeIndex + 2) % 3;
				Polygon.Vertices.InsertItem(Triangle.Vertices[TriangleOppositeVertexIndex],PolygonEdgeVertex1);

				return TRUE;
			}
		}
	}

	// Could not merge triangles.
	return FALSE;
}

/** Given a set of triangles, remove those which share an edge and could be collapsed into one triangle. */
void RemoveRedundantTriangles(TArray<FClipSMTriangle>& Tris)
{
	TArray<FClipSMPolygon> Polygons;

	// Merge the triangles into polygons.
	while(Tris.Num() > 0)
	{
		// Start building a polygon from the last triangle in the array.
		const FClipSMTriangle InitialTriangle = Tris.Pop();
		FClipSMPolygon MergedPolygon(0);
		MergedPolygon.CopyFace(InitialTriangle);
		MergedPolygon.Vertices.AddItem(InitialTriangle.Vertices[0]);
		MergedPolygon.Vertices.AddItem(InitialTriangle.Vertices[1]);
		MergedPolygon.Vertices.AddItem(InitialTriangle.Vertices[2]);

		// Find triangles that can be merged into the polygon.
		for(INT CandidateTriangleIndex = 0;CandidateTriangleIndex < Tris.Num();CandidateTriangleIndex++)
		{
			const FClipSMTriangle& MergeCandidateTriangle = Tris(CandidateTriangleIndex);
			if(MergeTriangleIntoPolygon(MergedPolygon, MergeCandidateTriangle))
			{
				// Remove the merged triangle from the array.
				Tris.RemoveSwap(CandidateTriangleIndex);

				// Restart the search for mergeable triangles from the start of the array.
				CandidateTriangleIndex = -1;
			}
		}

		// Add the merged polygon to the array.
		Polygons.AddItem(MergedPolygon);
	}

	// Triangulate each polygon and add it to the output triangle array.
	for(INT PolygonIndex = 0;PolygonIndex < Polygons.Num();PolygonIndex++)
	{
		TArray<FClipSMTriangle> Triangles;
		TriangulatePoly(Triangles,Polygons(PolygonIndex));
		Tris.Append(Triangles);
	}
}

/** Util class for clipping polygon to a half space in 2D */
class FSplitLine2D
{
private:
	FLOAT X, Y, W;

public:
	FSplitLine2D() 
	{}

	FSplitLine2D( const FVector2D& InBase, const FVector2D &InNormal )
	{
		X = InNormal.X;
		Y = InNormal.Y;
		W = (InBase | InNormal);
	}


	FLOAT PlaneDot( const FVector2D &P ) const
	{
		return X*P.X + Y*P.Y - W;
	}
};

/** Split 2D polygons with a 3D plane. */
void Split2DPolysWithPlane(FUtilPoly2DSet& PolySet, const FPlane& Plane, const FColor& ExteriorVertColor, const FColor& InteriorVertColor)
{
	// Break down world-space plane into normal and base
	FVector WNormal =  FVector(Plane.X, Plane.Y, Plane.Z);
	FVector WBase = WNormal * Plane.W;

	// Convert other plane into local space
	FVector LNormal = PolySet.PolyToWorld.InverseTransformNormal(WNormal);

	// If planes are parallel, see if it clips away everything
	if(Abs(LNormal.Z) > (1.f - 0.001f))
	{
		// Check distance of this plane from the other
		FLOAT Dist = Plane.PlaneDot(PolySet.PolyToWorld.GetOrigin());
		// Its in front - remove all polys
		if(Dist > 0.f)
		{
			PolySet.Polys.Empty();
		}

		return;
	}

	FVector LBase = PolySet.PolyToWorld.InverseTransformFVector(WBase);

	// Project 0-plane normal into other plane - we will trace along this line to find intersection of two planes.
	FVector NormInOtherPlane = FVector(0,0,1) - (LNormal * (FVector(0,0,1) | LNormal));

	// Find direction of plane-plane intersect line
	//FVector LineDir = LNormal ^ FVector(0,0,1);
	// Cross this with other plane normal to find vector in other plane which will intersect this plane (0-plane)
	//FVector V = LineDir ^ LNormal;

	// Find second point along vector
	FVector VEnd = LBase - (10.f * NormInOtherPlane);
	// Find intersection
	FVector Intersect = FLinePlaneIntersection(LBase, VEnd, FVector(0,0,0), FVector(0,0,1));
	check(!Intersect.ContainsNaN());

	// Make 2D line.
	FVector2D Normal2D = FVector2D(LNormal.X, LNormal.Y).SafeNormal();
	FVector2D Base2D = FVector2D(Intersect.X, Intersect.Y);
	FSplitLine2D Plane2D(Base2D, Normal2D);

	for(INT PolyIndex=PolySet.Polys.Num()-1; PolyIndex>=0; PolyIndex--)
	{
		FUtilPoly2D& Poly = PolySet.Polys(PolyIndex);
		INT NumPVerts = Poly.Verts.Num();

		// Calculate distance of verts from clipping line
		TArray<FLOAT> PlaneDist;
		PlaneDist.AddZeroed(NumPVerts);
		for(INT i=0; i<NumPVerts; i++)
		{
			PlaneDist(i) = Plane2D.PlaneDot(Poly.Verts(i).Pos);
		}

		TArray<FUtilVertex2D> FinalVerts;
		for(INT ThisVert=0; ThisVert<NumPVerts; ThisVert++)
		{
			UBOOL bStartInside = (PlaneDist(ThisVert) < 0.f);
			// If start vert is inside, add it.
			if(bStartInside)
			{
				FinalVerts.AddItem( Poly.Verts(ThisVert) );
			}

			// If start and next vert are on opposite sides, add intersection
			INT NextVert = (ThisVert+1)%NumPVerts;

			if(PlaneDist(ThisVert) * PlaneDist(NextVert) < 0.f)
			{
				// Find distance along edge that plane is
				FLOAT Alpha = -PlaneDist(ThisVert) / (PlaneDist(NextVert) - PlaneDist(ThisVert));
				FVector2D NewVertPos = Lerp(Poly.Verts(ThisVert).Pos, Poly.Verts(NextVert).Pos, Alpha);

				// New color based on whether we are cutting an 'interior' edge.
				FColor NewVertColor = (Poly.Verts(ThisVert).bInteriorEdge) ? InteriorVertColor : ExteriorVertColor;

				FUtilVertex2D NewVert = FUtilVertex2D(NewVertPos, NewVertColor);

				// We mark this the start of an interior edge if the edge we cut started inside.
				if(bStartInside || Poly.Verts(ThisVert).bInteriorEdge)
				{
					NewVert.bInteriorEdge = TRUE;
				}

				// Save vert
				FinalVerts.AddItem(NewVert);
			}
		}

		// If we have no verts left, all clipped away, remove from set of polys
		if(FinalVerts.Num() == 0)
		{
			PolySet.Polys.Remove(PolyIndex);
		}
		// Copy new set of verts back to poly.
		else
		{
			Poly.Verts = FinalVerts;
		}
	}
}

/** Given a set of triangles, calculate the average normal of them, weighted by triangle size. */
FVector CalcAverageTriNormal(TArray<FStaticMeshTriangle>& Tris)
{
	// Init accumulator
	FVector AverageNormal(0,0,0);

	// This cross product weights normals based on triangle area - we defer normalizing until the end.
	for(INT i=0; i<Tris.Num(); i++)
	{
		FVector Edge0 = Tris(i).Vertices[1] - Tris(i).Vertices[0];
		FVector Edge1 = Tris(i).Vertices[2] - Tris(i).Vertices[0];
		AverageNormal += (Edge1 ^ Edge0);
	}

	return AverageNormal.SafeNormal();
}

