/*=============================================================================
	UnNovodexGeomUtils.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"


inline const NxVec3 * GetConvexMeshVertex( NxConvexMeshDesc* desc, NxU32 index )
{
	return (const NxVec3 *)((BYTE*)desc->points + index*desc->pointStrideBytes);
}

inline void GetConvexMeshTriangle( NxConvexMeshDesc* desc, NxU32 index, const NxVec3* tri[3] )
{
	NxU32 c = (desc->flags & NX_CF_FLIPNORMALS) != 0;
	if( desc->flags & NX_CF_16_BIT_INDICES )
	{
		const NxU16 * trig16 = (const NxU16 *)((BYTE*)desc->triangles + index*desc->triangleStrideBytes);
		tri[0] = GetConvexMeshVertex( desc, trig16[0] );
		tri[1] = GetConvexMeshVertex( desc, trig16[1+c] );
		tri[2] = GetConvexMeshVertex( desc, trig16[2-c] );
	}
	else
	{
		const NxU32 * trig32 = (const NxU32 *)((BYTE*)desc->triangles + index*desc->triangleStrideBytes);
		tri[0] = GetConvexMeshVertex( desc, trig32[0] );
		tri[1] = GetConvexMeshVertex( desc, trig32[1+c] );
		tri[2] = GetConvexMeshVertex( desc, trig32[2-c] );
	}
}

UBOOL RepresentConvexAsBox( NxActorDesc* ActorDesc, NxConvexMesh* ConvexMesh, UBOOL bCreateCCDSkel )
{
	const NxReal eps = (NxReal)0.001;
	const NxReal oneMinusHalfEps = (NxReal)1 - (NxReal)0.5*eps*eps;

	NxConvexMeshDesc desc;
	desc.numVertices         = ConvexMesh->getCount(0,  NX_ARRAY_VERTICES);
	desc.numTriangles        = ConvexMesh->getCount(0,  NX_ARRAY_TRIANGLES);
	desc.pointStrideBytes    = ConvexMesh->getStride(0, NX_ARRAY_VERTICES);
	desc.triangleStrideBytes = ConvexMesh->getStride(0, NX_ARRAY_TRIANGLES);
	desc.points              = ConvexMesh->getBase(0,   NX_ARRAY_VERTICES);
	desc.triangles           = ConvexMesh->getBase(0,   NX_ARRAY_TRIANGLES);
	desc.flags               = ConvexMesh->getFormat(0, NX_ARRAY_TRIANGLES)==NX_FORMAT_SHORT ? NX_CF_16_BIT_INDICES : 0;

	// A box should have eight vertices and twelve triangles
	if( desc.numVertices != 8 || desc.numTriangles != 12 )
	{
		return FALSE;
	}

	INT NormalsFound = 0;
	// Normals should bin into three groups, if we ignore overall factor of (-1)
	NxVec3 normal[3];
	INT NormalCount[3] = {0,0,0};	// Reality check, should find two of each
	INT AntiNormalCount[3] = {0,0,0};	// Reality check, should find two of each
	NxReal radii[3] = {0,0,0};

	INT i;
	for( i = 0; i < 12; ++i )
	{
		// Get the i(th) triangle and its normal
		const NxVec3* tri[3];
		GetConvexMeshTriangle( &desc, i, tri );
		NxVec3 n;
		n.cross( *(tri[1]) - *(tri[0]), *(tri[2]) - *(tri[0]) );
		n.normalize();

		// See if it matches any of the previously found normals
		INT j;
		for( j = 0; j < NormalsFound; ++j )
		{
			NxReal proj = n.dot( normal[j] );
			if( proj > oneMinusHalfEps )
			{
				++NormalCount[j];
				break;
			}
			if( proj < -oneMinusHalfEps )
			{
				if( !AntiNormalCount[j] )
				{	// Take this as an opportunity to calculate the box extent in this direction
					radii[j] += n.dot( *(tri[0]) );
					radii[j] = fabsf( (NxReal)0.5*radii[j] );
				}
				++AntiNormalCount[j];
				break;
			}
		}
		if( j == NormalsFound )
		{
			if( NormalsFound >= 3 )
			{
				return FALSE;	// More than three normals, can't be a cube
			}
			// New normal - add it
			normal[NormalsFound++] = n;
			radii[j] = n.dot( *(tri[0]) );	// This is a temporary value
			++NormalCount[j];
		}
	}

	// Should have three distinct normals
	if( NormalsFound != 3 )
	{
		return FALSE;
	}

	// Triangle normals should have come in pairs
	if( NormalCount[0] != 2 || NormalCount[1] != 2 || NormalCount[2] != 2 )
	{
		return FALSE;
	}

	// And corresponding triangles on the opposite faces
	if( AntiNormalCount[0] != 2 || AntiNormalCount[1] != 2 || AntiNormalCount[2] != 2 )
	{
		return FALSE;
	}

	// Make sure normals are orthogonal
	if( Abs(normal[0].dot(normal[1])) > eps ||
		Abs(normal[1].dot(normal[2])) > eps ||
		Abs(normal[2].dot(normal[0])) > eps )
	{
		return FALSE;
	}

	// Finally, make sure we have a right-handed coordinate system
	if( (normal[0].cross( normal[1] )).dot( normal[2] ) < 0 )
	{
		normal[2] *= -(NxReal)1;
	}

	// We have a box!  Find its center
	NxVec3 center( (NxReal)0, (NxReal)0, (NxReal)0 );
	for( i = 0; i < 8; ++i )
	{
		center += *GetConvexMeshVertex( &desc, i );
	}
	center *= (NxReal)0.125;

	// The three normal directions and center form the rows of the relative TM
	NxMat33 rot( normal[0], normal[1], normal[2] );
	rot.setTransposed( rot );
	NxMat34 RelativeTM( rot, center );

	// Create the box description
	NxBoxShapeDesc* BoxDesc = new NxBoxShapeDesc;
	BoxDesc->dimensions = NxVec3( radii[0], radii[1], radii[2] );
	BoxDesc->localPose = RelativeTM;
	if(bCreateCCDSkel)
	{
		MakeCCDSkelForBox(BoxDesc);
	}

	ActorDesc->shapes.pushBack(BoxDesc);

	return TRUE;
}

static const FLOAT CCDScaleAmount = 0.5f;
static const FLOAT RecipSqrtOfThree = 0.5773f;

static inline void AddTri(TArray<INT>& InIndexBuffer, INT Index0, INT Index1, INT Index2)
{
	InIndexBuffer.AddItem(Index0);
	InIndexBuffer.AddItem(Index1);
	InIndexBuffer.AddItem(Index2);
}

void MakeCCDSkelForSphere(NxSphereShapeDesc* SphereDesc)
{
	if(!GNovodexSDK)
	{
		return;
	}

	TArray<FVector>	TetraVerts;
	TetraVerts.Add(4);

	// Distance from origin to each vert is sqrt(3). 
	// So to make a tetrahedron whose pointer are (Radius * CCDShrinkAmount) away from origin, we divide by sqrt(3).
	TetraVerts(0) = SphereDesc->radius * CCDScaleAmount * FVector(1,1,1) * RecipSqrtOfThree;
	TetraVerts(1) = SphereDesc->radius * CCDScaleAmount * FVector(-1,-1,1) * RecipSqrtOfThree;
	TetraVerts(2) = SphereDesc->radius * CCDScaleAmount * FVector(-1,1,-1) * RecipSqrtOfThree;
	TetraVerts(3) = SphereDesc->radius * CCDScaleAmount * FVector(1,-1,-1) * RecipSqrtOfThree;

	// Make index buffer
	TArray<INT> TetraIndices;

	AddTri(TetraIndices, 0, 2, 1);
	AddTri(TetraIndices, 0, 1, 3);
	AddTri(TetraIndices, 0, 3, 2);
	AddTri(TetraIndices, 1, 2, 3);

	NxSimpleTriangleMesh nTriMesh;
	nTriMesh.points = TetraVerts.GetData();
	nTriMesh.numVertices = 4;
	nTriMesh.pointStrideBytes = sizeof(FVector);
	nTriMesh.numTriangles = 4;
	nTriMesh.triangles = TetraIndices.GetData();
	nTriMesh.triangleStrideBytes = sizeof(INT) * 3;
	nTriMesh.flags = 0;

	SphereDesc->ccdSkeleton = GNovodexSDK->createCCDSkeleton(nTriMesh);
}

void MakeCCDSkelForBox(NxBoxShapeDesc* BoxDesc)
{
	if(!GNovodexSDK)
	{
		return;
	}

	TArray<FVector>	BoxVerts;
	BoxVerts.Add(8);

	FVector SkelRadii = CCDScaleAmount * FVector(BoxDesc->dimensions.x, BoxDesc->dimensions.y, BoxDesc->dimensions.z);

	BoxVerts(0) = SkelRadii * FVector(-1,-1,-1);
	BoxVerts(1) = SkelRadii * FVector( 1,-1,-1);
	BoxVerts(2) = SkelRadii * FVector( 1, 1,-1);
	BoxVerts(3) = SkelRadii * FVector(-1, 1,-1);

	BoxVerts(4) = SkelRadii * FVector(-1,-1, 1);
	BoxVerts(5) = SkelRadii * FVector( 1,-1, 1);
	BoxVerts(6) = SkelRadii * FVector( 1, 1, 1);
	BoxVerts(7) = SkelRadii * FVector(-1, 1, 1);

	// Make index buffer
	TArray<INT> BoxIndices;

	AddTri(BoxIndices, 0, 2, 1);
	AddTri(BoxIndices, 0, 3, 2);

	AddTri(BoxIndices, 1, 6, 5);
	AddTri(BoxIndices, 1, 2, 6);

	AddTri(BoxIndices, 5, 7, 4);
	AddTri(BoxIndices, 5, 6, 7);

	AddTri(BoxIndices, 4, 3, 0);
	AddTri(BoxIndices, 4, 7, 3);

	AddTri(BoxIndices, 3, 6, 2);
	AddTri(BoxIndices, 3, 7, 6);

	AddTri(BoxIndices, 5, 0, 1);
	AddTri(BoxIndices, 5, 4, 0);

	NxSimpleTriangleMesh nTriMesh;
	nTriMesh.points = BoxVerts.GetData();
	nTriMesh.numVertices = 8;
	nTriMesh.pointStrideBytes = sizeof(FVector);
	nTriMesh.numTriangles = 12;
	nTriMesh.triangles = BoxIndices.GetData();
	nTriMesh.triangleStrideBytes = sizeof(INT) * 3;
	nTriMesh.flags = 0;

	BoxDesc->ccdSkeleton = GNovodexSDK->createCCDSkeleton(nTriMesh);
}

void MakeCCDSkelForSphyl(NxCapsuleShapeDesc* SphylDesc)
{
	if(!GNovodexSDK)
	{
		return;
	}

	TArray<FVector>	PrismVerts;
	PrismVerts.Add(6);

	// Main axis is Y. Verts for triangle at one end
	PrismVerts(0) = (0.5f * SphylDesc->height * FVector(0, 1, 0)) + (SphylDesc->radius * CCDScaleAmount * FVector(1,0,0));
	PrismVerts(1) = (0.5f * SphylDesc->height * FVector(0, 1, 0)) + (SphylDesc->radius * CCDScaleAmount * FVector(-0.5f,0,0.866025f));
	PrismVerts(2) = (0.5f * SphylDesc->height * FVector(0, 1, 0)) + (SphylDesc->radius * CCDScaleAmount * FVector(-0.5f,0,-0.866025f));

	// Triangle at the other end
	PrismVerts(3) = (-0.5f * SphylDesc->height * FVector(0, 1, 0)) + (SphylDesc->radius * CCDScaleAmount * FVector(1,0,0));
	PrismVerts(4) = (-0.5f * SphylDesc->height * FVector(0, 1, 0)) + (SphylDesc->radius * CCDScaleAmount * FVector(-0.5f,0,0.866025f));
	PrismVerts(5) = (-0.5f * SphylDesc->height * FVector(0, 1, 0)) + (SphylDesc->radius * CCDScaleAmount * FVector(-0.5f,0,-0.866025f));


	// Make index buffer
	TArray<INT> PrismIndices;

	// Four rectangular faces
	AddTri(PrismIndices, 0, 4, 3);
	AddTri(PrismIndices, 0, 1, 4);

	AddTri(PrismIndices, 1, 5, 4);
	AddTri(PrismIndices, 1, 2, 5);

	AddTri(PrismIndices, 2, 3, 5);
	AddTri(PrismIndices, 2, 0, 3);

	// End triangles
	AddTri(PrismIndices, 0, 2, 1);
	AddTri(PrismIndices, 5, 3, 4);

	NxSimpleTriangleMesh nTriMesh;
	nTriMesh.points = PrismVerts.GetData();
	nTriMesh.numVertices = 6;
	nTriMesh.pointStrideBytes = sizeof(FVector);
	nTriMesh.numTriangles = 8;
	nTriMesh.triangles = PrismIndices.GetData();
	nTriMesh.triangleStrideBytes = sizeof(INT) * 3;
	nTriMesh.flags = 0;

	SphylDesc->ccdSkeleton = GNovodexSDK->createCCDSkeleton(nTriMesh);
}

void MakeCCDSkelForConvex(NxConvexShapeDesc* ConvexDesc)
{
	if(!GNovodexSDK)
	{
		return;
	}

	NxConvexMesh* Mesh = ConvexDesc->meshData;

	// Extract vertex info from mesh
	INT VertCount = Mesh->getCount(0,NX_ARRAY_VERTICES);    

	const void* VertBase = Mesh->getBase(0,NX_ARRAY_VERTICES);    

	NxU32 VertStride = Mesh->getStride(0,NX_ARRAY_VERTICES);
	check(VertStride == sizeof(FVector));

	// Copy verts out, whilst calculating centroid.
	TArray<FVector>	ConvexVerts;
	ConvexVerts.Add(VertCount);
	FVector Centroid(0,0,0);
	for(INT i=0; i<VertCount; i++)
	{
		// Copy vertex from the mesh
		FVector* V = ((FVector*)VertBase) + i;
		ConvexVerts(i) = *V;

		// Accumulate vertex positions.
		Centroid += ConvexVerts(i);
	}
	Centroid = Centroid / (FLOAT)VertCount;

	// New smush verts back towards centroid.
	for(INT i=0; i<VertCount; i++)
	{
		FVector Delta = ConvexVerts(i) - Centroid;
		ConvexVerts(i) = Centroid + (CCDScaleAmount * Delta);
	}

	// Get index information
	INT TriCount = Mesh->getCount(0,NX_ARRAY_TRIANGLES);
	check(TriCount > 0);

	NxInternalFormat IndexFormat = Mesh->getFormat(0,NX_ARRAY_TRIANGLES);
	check(IndexFormat == NX_FORMAT_INT);

	INT* IndexBase = (INT*)(Mesh->getBase(0,NX_ARRAY_TRIANGLES));

	NxU32 IndexStride = Mesh->getStride(0,NX_ARRAY_TRIANGLES);
	check(IndexStride == sizeof(INT) * 3);

	// Copy indices out to make index buffer.
	TArray<INT>	ConvexIndices;
	for(INT i=0; i<TriCount; i++)
	{
		ConvexIndices.AddItem( IndexBase[(i*3)+0] );
		ConvexIndices.AddItem( IndexBase[(i*3)+1] );
		ConvexIndices.AddItem( IndexBase[(i*3)+2] );
	}

	NxSimpleTriangleMesh nTriMesh;
	nTriMesh.points = ConvexVerts.GetData();
	nTriMesh.numVertices = VertCount;
	nTriMesh.pointStrideBytes = sizeof(FVector);
	nTriMesh.numTriangles = TriCount;
	nTriMesh.triangles = ConvexIndices.GetData();
	nTriMesh.triangleStrideBytes = sizeof(INT) * 3;
	nTriMesh.flags = 0;

	ConvexDesc->ccdSkeleton = GNovodexSDK->createCCDSkeleton(nTriMesh);
}


#endif // WITH_NOVODEX
