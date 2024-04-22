/*=============================================================================
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __GEOMTOOLS_H__
#define __GEOMTOOLS_H__

/** An edge in 3D space, used by these utility functions. */
struct FUtilEdge3D
{
	/** Start of edge in 3D space. */
	FVector V0;
	/** End of edge in 3D space. */
	FVector V1;
};

/** An edge in 2D space, used by these utility functions. */
struct FUtilEdge2D
{
	/** Start of edge in 2D space. */
	FVector2D V0;
	/** End of edge in 2D space. */
	FVector2D V1;
};

/** A triangle vertex in 2D space, with UV information. */
struct FUtilVertex2D
{
	FVector2D Pos;
	FColor Color;
	FVector2D UV;
	UBOOL bInteriorEdge;

	FUtilVertex2D() {}

	FUtilVertex2D(const FVector2D& InPos) 
		: Pos(InPos), Color(255,255,255,255), UV(0.f, 0.f), bInteriorEdge(FALSE)
	{}

	FUtilVertex2D(const FVector2D& InPos, const FColor& InColor) 
		: Pos(InPos), Color(InColor), UV(0.f, 0.f), bInteriorEdge(FALSE)
	{}
};

/** A polygon in 2D space, used by utility function. */
struct FUtilPoly2D
{
	/** Set of verts, in order, for polygon. */
	TArray<FUtilVertex2D> Verts;
};

/** A set of 2D polygons, along with a transform for going into world space. */
struct FUtilPoly2DSet
{
	TArray<FUtilPoly2D>	Polys;
	FMatrix PolyToWorld;
};

/** Triangle in 2D space. */
struct FUtilTri2D
{
	FUtilVertex2D Verts[3];
};

/** Temp vertex struct for one vert of a static mesh triangle. */
struct FClipSMVertex
{
	FVector Pos;
	FVector TangentX;
	FVector TangentY;
	FVector TangentZ;
	FVector2D UVs[8];
	FColor Color;
};

/** Properties of a clipped static mesh face. */
struct FClipSMFace
{
	INT MaterialIndex;
	INT FragmentIndex;
	DWORD SmoothingMask;
	INT NumUVs;
	UBOOL bOverrideTangentBasis;

	FVector FaceNormal;

	FMatrix TangentXGradient;
	FMatrix TangentYGradient;
	FMatrix TangentZGradient;
	FMatrix UVGradient[8];
	FMatrix ColorGradient;

	void CopyFace(const FClipSMFace& OtherFace)
	{
		appMemcpy(this,&OtherFace,sizeof(FClipSMFace));
	}
};

/** Properties of a clipped static mesh triangle. */
struct FClipSMTriangle : FClipSMFace
{
	FClipSMVertex Vertices[3];

	/** Compute the triangle's normal, and the gradients of the triangle's vertex attributes over XYZ. */
	void ComputeGradientsAndNormal();

	FClipSMTriangle(INT Init)
	{
		appMemzero(this, sizeof(FClipSMTriangle));
	}
};

/** Properties of a clipped static mesh polygon. */
struct FClipSMPolygon : FClipSMFace
{
	TArray<FClipSMVertex> Vertices;

	FClipSMPolygon(INT Init)
	{
		appMemzero(this, sizeof(FClipSMPolygon));
	}
};

/** Used as output from the CreateVoronoiRegions function. */
struct FVoronoiRegion
{
	/** Convex hull defining this region. */
	FKConvexElem	ConvexElem;
	/** Neighbours of this chunk - corresponds to planes in ConvexElem.FacePlaneData, and needs to match its size. INDEX_NONE indicates an exterior face or invalid neighbour. */
	TArray<INT>		Neighbours;
	/** Size of connection to neighbour. Also needs to be same size as Neighbours and ConvexElem.FacePlaneData. */
	TArray<FLOAT>	NeighbourDims;
};

/** Extracts the triangles from a static-mesh as clippable triangles. */
void GetClippableStaticMeshTriangles(TArray<FClipSMTriangle>& OutClippableTriangles,const UStaticMesh* StaticMesh);

/** Extracts the triangles from a static-mesh as clippable triangles. */
void GetRawStaticMeshTrianglesFromClipped(TArray<FStaticMeshTriangle>& OutStaticMeshTriangles,const TArray<FClipSMTriangle>& ClippedTriangles);

/** Break StartBox into convex hulls representing spaces closes to the supplied points. */
void CreateVoronoiRegions( const FBox& StartBox, const TArray<FVector>& Points, const FVector& PlaneBias, TArray<FVoronoiRegion>& OutRegions );

/** Take the input mesh and cut it with supplied plane, creating new verts etc. Also outputs new edges created on the plane. */
void ClipMeshWithPlane( TArray<FClipSMTriangle>& OutTris, TArray<FUtilEdge3D>& OutClipEdges, const TArray<FClipSMTriangle>& InTriangles,const FPlane& Plane );

/** Take a set of 3D Edges and project them onto the supplied plane. Also returns matrix use to convert them back into 3D edges. */
void ProjectEdges( TArray<FUtilEdge2D>& Out2DEdges, FMatrix& ToWorld, const TArray<FUtilEdge3D>& In3DEdges, const FPlane& InPlane );

/** Given a set of edges, find the set of closed polygons created by them. */
void Buid2DPolysFromEdges(TArray<FUtilPoly2D>& OutPolys, const TArray<FUtilEdge2D>& InEdges, const FColor& VertColor);

/** Given a polygon, decompose into triangles and append to OutTris.
  * @return	TRUE if the triangulation was successful.
  */
UBOOL TriangulatePoly(TArray<FClipSMTriangle>& OutTris, const FClipSMPolygon& InPoly);

/** Transform triangle from 2D to 3D static-mesh triangle. */
FClipSMPolygon Transform2DPolygonToSMPolygon(const FUtilPoly2D& InTri, const FMatrix& InMatrix);

/** Does a simple box map onto this 2D polygon. */
void GeneratePolyUVs(FUtilPoly2D& Polygon);

/** Given a set of triangles, remove those which share an edge and could be collapsed into one triangle. */
void RemoveRedundantTriangles(TArray<FClipSMTriangle>& Tris);

/** Split 2D polygons with a 3D plane. */
void Split2DPolysWithPlane(FUtilPoly2DSet& PolySet, const FPlane& Plane, const FColor& ExteriorVertColor, const FColor& InteriorVertColor);

/** Given a set of triangles, calculate the average normal of them, weighted by triangle size. */
FVector CalcAverageTriNormal(TArray<FStaticMeshTriangle>& Tris);

#endif
