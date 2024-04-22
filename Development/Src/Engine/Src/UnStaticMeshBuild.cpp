/*=============================================================================
	UnStaticMeshBuild.cpp: Static mesh building.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#include "UnMeshBuild.h"

#if WITH_D3D11_TESSELLATION
#include "TessellationRendering.h"
#endif // #if WITH_D3D11_TESSELLATION

#if !CONSOLE

//
//	StaticMesh_UVsEqual
//

inline UBOOL StaticMesh_UVsEqual(const FVector2D& UV1,const FVector2D& UV2)
{
	if(Abs(UV1.X - UV2.X) > (1.0f / 1024.0f))
		return 0;

	if(Abs(UV1.Y - UV2.Y) > (1.0f / 1024.0f))
		return 0;

	return 1;
}

//
// FanFace - Smoothing group interpretation helper structure.
//

struct FanFace
{
	INT FaceIndex;
	INT LinkedVertexIndex;
	UBOOL Filled;		
};


/**
 * Returns FALSE if the two vertices differ in some significant way
 */
static UBOOL CompareVertex( FVector Position,
							FPackedNormal TangentX,
							FPackedNormal TangentY,
							FPackedNormal TangentZ,
							FColor Color,
							const FVector2D* UVs,
							INT NumUVs,
							INT FragmentIndex,
							const FStaticMeshBuildVertex& CompareVertex,
							UBOOL bIgnoreTangents,
							UBOOL bRemoveDegenerates)
{
	// Position
	if( !PointsEqual( CompareVertex.Position, Position, bRemoveDegenerates ))
	{
		return FALSE;
	}

	if (!bIgnoreTangents)
	{
		// Tangent
		if( !NormalsEqual( CompareVertex.TangentX, TangentX ) )
		{
			return FALSE;
		}

		// Binormal
		if( !NormalsEqual( CompareVertex.TangentY, TangentY ) )
		{
			return FALSE;
		}

		// Normal
		if( !NormalsEqual( CompareVertex.TangentZ, TangentZ ) )
		{
			return FALSE;
		}
	}

	// Color
	if( CompareVertex.Color != Color )
	{
		return FALSE;
	}

	// UVs
	for( INT UVIndex = 0; UVIndex < Min<INT>( NumUVs, MAX_TEXCOORDS ); UVIndex++ )
	{
		if( !StaticMesh_UVsEqual( CompareVertex.UVs[UVIndex], UVs[UVIndex] ) )
		{
			return FALSE;
		}
	}

	// Fragment index
	if( CompareVertex.FragmentIndex != FragmentIndex )
	{
		return FALSE;
	}

	return TRUE;
}


static INT AddVertex(
	FVector Position,
	FPackedNormal TangentX,
	FPackedNormal TangentY,
	FPackedNormal TangentZ,
	FColor Color,
	const FVector2D* UVs,
	INT NumUVs,
	INT FragmentIndex,
	TArray<FStaticMeshBuildVertex>& Vertices)
{
	// Add a new vertex to the vertex buffers.

	FStaticMeshBuildVertex Vertex;

	Vertex.Position = Position;
	Vertex.TangentX = TangentX;
	Vertex.TangentY = TangentY;
	Vertex.TangentZ = TangentZ;
	Vertex.Color = Color;
	Vertex.FragmentIndex = FragmentIndex;

	for(INT UVIndex = 0;UVIndex < Min<INT>(NumUVs,MAX_TEXCOORDS);UVIndex++)
	{
		Vertex.UVs[UVIndex] = UVs[UVIndex];
	}

	for(INT UVIndex = NumUVs;UVIndex < MAX_TEXCOORDS;UVIndex++)
	{
		Vertex.UVs[UVIndex] = FVector2D(0,0);
	}

	return Vertices.AddItem(Vertex);
}

//
//	ClassifyTriangleVertices
//

ESplitType ClassifyTriangleVertices(const FPlane& Plane,FVector* Vertices)
{
	ESplitType	Classification = SP_Coplanar;

	for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
	{
		FLOAT	Dist = Plane.PlaneDot(Vertices[VertexIndex]);

		if(Dist < -0.0001f)
		{
			if(Classification == SP_Front)
				Classification = SP_Split;
			else if(Classification != SP_Split)
				Classification = SP_Back;
		}
		else if(Dist >= 0.0001f)
		{
			if(Classification == SP_Back)
				Classification = SP_Split;
			else if(Classification != SP_Split)
				Classification = SP_Front;
		}
	}

	return Classification;
}
#endif

//
//	UStaticMesh::Build
//

void UStaticMesh::Build(UBOOL bSingleTangentSetOverride, UBOOL bSilent)
{
#if !CONSOLE
	check(LODModels.Num() > 0);
	if( GetOutermost()->PackageFlags & PKG_Cooked )
	{
		// The mesh has been cooked and therefore the raw mesh data has been stripped.
		return;
	}

	if(!bSilent)
	{
		GWarn->BeginSlowTask(*FString::Printf(TEXT("(%s) Building"),*GetPathName()),TRUE);	
	}

	// Detach all instances of this static mesh from the scene.
	FStaticMeshComponentReattachContext	ComponentReattachContext(this);

	// Release the static mesh's resources.
	ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
	// allocated, and potentially accessing the UStaticMesh.
	ReleaseResourcesFence.Wait();

	// Mark the parent package as dirty.
	MarkPackageDirty();

	TArray<FkDOPBuildCollisionTriangle<WORD> > kDOPBuildTriangles;
	for(INT i=0;i<LODModels.Num();i++)
	{
		// NOTE: Only building kdop for LOD 0
		LODModels(i).Build(kDOPBuildTriangles,(i==0),this, bSingleTangentSetOverride, bRemoveDegenerates, bSilent);
	}

	// Calculate the bounding box.

	FBox	BoundingBox(0);

	for(UINT VertexIndex = 0;VertexIndex < LODModels(0).NumVertices;VertexIndex++)
	{
		BoundingBox += LODModels(0).PositionVertexBuffer.VertexPosition(VertexIndex);
	}
	BoundingBox.GetCenterAndExtents(Bounds.Origin,Bounds.BoxExtent);

	// Calculate the bounding sphere, using the center of the bounding box as the origin.

	Bounds.SphereRadius = 0.0f;
	for(UINT VertexIndex = 0;VertexIndex < LODModels(0).NumVertices;VertexIndex++)
	{
		Bounds.SphereRadius = Max((LODModels(0).PositionVertexBuffer.VertexPosition(VertexIndex) - Bounds.Origin).Size(),Bounds.SphereRadius);
	}
	kDOPTree.Build(kDOPBuildTriangles);

	// Increment the vertex position version number of the mesh as it has been rebuilt. This will trigger components relying on this
	// mesh to update their override colors as necessary.
	VertexPositionVersionNumber++;

	// Find any static mesh components that use this mesh and fixup their override colors if necessary.
	for( TObjectIterator<UStaticMeshComponent> It; It; ++It )
	{
		if ( It->StaticMesh == this )
		{
			It->FixupOverrideColorsIfNecessary();
		}
	}

	// Reinitialize the static mesh's resources.
	InitResources();

	// Force the static mesh to re-export next time lighting is built
	SetLightingGuid();

	if(!bSilent)
	{
		GWarn->EndSlowTask();
	}
	
#else
	appErrorf(TEXT("UStaticMesh::Build should not be called on a console"));
#endif
}


/**
* Fill an array with triangles which will be used to build a KDOP tree
* @param kDOPBuildTriangles - the array to fill
*/

void FStaticMeshRenderData::GetKDOPTriangles(TArray<FkDOPBuildCollisionTriangle<WORD> >& kDOPBuildTriangles) 
{
	for(INT TriangleIndex = 0; TriangleIndex < IndexBuffer.Indices.Num(); TriangleIndex += 3)
	{
		UINT IndexOne = IndexBuffer.Indices(TriangleIndex);
		UINT IndexTwo = IndexBuffer.Indices(TriangleIndex + 1);
		UINT IndexThree = IndexBuffer.Indices(TriangleIndex + 2);

		//add a triangle to the array
		new (kDOPBuildTriangles) FkDOPBuildCollisionTriangle<WORD>(IndexOne,
			IndexTwo, IndexThree, 0,
			PositionVertexBuffer.VertexPosition(IndexOne), PositionVertexBuffer.VertexPosition(IndexTwo), PositionVertexBuffer.VertexPosition(IndexThree));
	}
}


// this is used for a sub-quardatic routine to find "equal" verts
struct FStaticMeshVertIndexAndZ
{
	INT Index;  // this will be TriangleIndex * 3 + VertIndex
	FLOAT Z;
};

// Sorting function for vertex Z/index pairs
IMPLEMENT_COMPARE_CONSTREF( FStaticMeshVertIndexAndZ, StaticMeshRenderDataBuild, { if (A.Z < B.Z) return -1; if (A.Z > B.Z) return 1; return 0; } )

// A simple integer sorting function
IMPLEMENT_COMPARE_CONSTREF( INT, StaticMeshRenderDataBuild, { if (A < B) return -1; if (A > B) return 1; return 0; } )

/**
* Build rendering data from a raw triangle stream
* @param kDOPBuildTriangles output collision tree. A dummy can be passed if you do not specify BuildKDop as TRUE
* @param Whether to build and return a kdop tree from the mesh data
* @param Parent Parent mesh
*/
void FStaticMeshRenderData::Build(TArray<FkDOPBuildCollisionTriangle<WORD> >& kDOPBuildTriangles, UBOOL BuildKDop, class UStaticMesh* Parent, UBOOL bSingleTangentSetOverride, UBOOL bRemoveDegenerates, UBOOL bSilent)
{
#if !CONSOLE
	check(Parent);
	// Load raw data.
	FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) RawTriangles.Lock(LOCK_READ_ONLY);

	INT MaxMaterialIndex = -1;
	for (INT TriangleIndex = 0; TriangleIndex < RawTriangles.GetElementCount(); TriangleIndex++)
	{
		FStaticMeshTriangle* Triangle = &RawTriangleData[TriangleIndex];
		MaxMaterialIndex = Max<INT>(Triangle->MaterialIndex, MaxMaterialIndex);
	}

	if (MaxMaterialIndex >= Elements.Num())
	{
		warnf(NAME_Warning, TEXT("RAW TRIANGLE MATERIAL MISMATCH: Adding %2d elements to mesh %s"), 
			MaxMaterialIndex - Elements.Num() + 1, *(Parent->GetPathName()));
		INT StartIndex = Elements.Num();
		Elements.AddZeroed(MaxMaterialIndex - Elements.Num() + 1);
		for (INT AddIdx = StartIndex; AddIdx < Elements.Num(); AddIdx++)
		{
			FStaticMeshElement& NewElement = Elements(AddIdx);
			NewElement.Material = GEngine->DefaultMaterial;
			NewElement.MaterialIndex = AddIdx;
		}
	}


	// Clear old data.
	TArray<FStaticMeshBuildVertex> Vertices;
	IndexBuffer.Indices.Empty();
	WireframeIndexBuffer.Indices.Empty();
	AdjacencyIndexBuffer.Indices.Empty();
	TArray<FMeshEdge> Edges;
	Edges.Empty();
	VertexBuffer.CleanUp();
	PositionVertexBuffer.CleanUp();
	ColorVertexBuffer.CleanUp();

	// force 32 bit floats if needed
	VertexBuffer.SetUseFullPrecisionUVs(Parent->UseFullPrecisionUVs);	

	// Calculate triangle normals.

	TArray<FVector>	TriangleTangentX(RawTriangles.GetElementCount());
	TArray<FVector>	TriangleTangentY(RawTriangles.GetElementCount());
	TArray<FVector>	TriangleTangentZ(RawTriangles.GetElementCount());

	INT MaxFragmentIndex = 0;
	for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.GetElementCount();TriangleIndex++)
	{
		FStaticMeshTriangle*	Triangle = &RawTriangleData[TriangleIndex];
        if(!Triangle->bOverrideTangentBasis)
        {
            MaxFragmentIndex = Max<INT>(MaxFragmentIndex, Triangle->FragmentIndex);
            INT						UVIndex = 0;

			// When keeping degenerates use no tolerance as very tiny triangles will usually return a normal of zero length when any threshold is used.
			const FLOAT Tolerance = bRemoveDegenerates ? SMALL_NUMBER : 0.0f;
			const FVector Normal = FVector( ((Triangle->Vertices[1] - Triangle->Vertices[2])^(Triangle->Vertices[0] - Triangle->Vertices[2])).SafeNormal(Tolerance) );
			const FLOAT W = Triangle->Vertices[2] | ((Triangle->Vertices[1] - Triangle->Vertices[2])^(Triangle->Vertices[0]- Triangle->Vertices[2])).SafeNormal(Tolerance);
			FVector	TriangleNormal = FPlane( Normal, W );

            FVector	P1 = Triangle->Vertices[0],
                P2 = Triangle->Vertices[1],
                P3 = Triangle->Vertices[2];
            FMatrix	ParameterToLocal(
                FPlane(	P2.X - P1.X,	P2.Y - P1.Y,	P2.Z - P1.Z,	0	),
                FPlane(	P3.X - P1.X,	P3.Y - P1.Y,	P3.Z - P1.Z,	0	),
                FPlane(	P1.X,			P1.Y,			P1.Z,			0	),
                FPlane(	0,				0,				0,				1	)
                );

            FVector2D	T1 = Triangle->UVs[0][UVIndex],
                T2 = Triangle->UVs[1][UVIndex],
                T3 = Triangle->UVs[2][UVIndex];
            FMatrix		ParameterToTexture(
                FPlane(	T2.X - T1.X,	T2.Y - T1.Y,	0,	0	),
                FPlane(	T3.X - T1.X,	T3.Y - T1.Y,	0,	0	),
                FPlane(	T1.X,			T1.Y,			1,	0	),
                FPlane(	0,				0,				0,	1	)
                );

			// Use InverseSlow to catch singular matrices.  InverseSafe can miss this sometimes.
            const FMatrix TextureToLocal = ParameterToTexture.InverseSlow() * ParameterToLocal;

            TriangleTangentX(TriangleIndex) = TextureToLocal.TransformNormal(FVector(1,0,0)).SafeNormal();
            TriangleTangentY(TriangleIndex) = TextureToLocal.TransformNormal(FVector(0,1,0)).SafeNormal();
            TriangleTangentZ(TriangleIndex) = TriangleNormal;

            CreateOrthonormalBasis(
                TriangleTangentX(TriangleIndex),
                TriangleTangentY(TriangleIndex),
                TriangleTangentZ(TriangleIndex)
                );
        }
	}

	INT NumFragments = 1;
	for(INT ElementIndex = 0;ElementIndex < Elements.Num();ElementIndex++)
	{
		NumFragments = Max<INT>(NumFragments, Elements(ElementIndex).Fragments.Num());
	}
	// Check for fragment index out of range
	check(MaxFragmentIndex < NumFragments);

	// Initialize material/fragment index buffers, one for each material/fragment pair.
	TArray<FRawStaticIndexBuffer>	ElementFragmentIndexBuffers;
	for(INT ElementIndex = 0;ElementIndex < Elements.Num() * NumFragments;ElementIndex++)
	{
		new(ElementFragmentIndexBuffers) FRawStaticIndexBuffer(TRUE);
	}

	// Determine the number of texture coordinates/vertex used by the static mesh.
	UINT NumTexCoords = 1;
	for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.GetElementCount();TriangleIndex++)
	{
		const FStaticMeshTriangle* Triangle = &RawTriangleData[TriangleIndex];
		NumTexCoords = Clamp<UINT>(Triangle->NumUVs,NumTexCoords,MAX_TEXCOORDS);
	}


	// To accelerate generation of adjacency, we'll create a table that maps each vertex index
	// to its overlapping vertices, and a table that maps a vertex to the its influenced faces
	// NOTE: generally an index is the triangleindex *3 + the vert index
	TMultiMap<INT,INT> Vert2Duplicates;
	{
		// Create a list of vertex Z/index pairs
		TArray<FStaticMeshVertIndexAndZ> VertIndexAndZ;
		VertIndexAndZ.Empty(RawTriangles.GetElementCount() * 3);
		for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.GetElementCount();TriangleIndex++)
		{
			const FStaticMeshTriangle& Triangle = RawTriangleData[TriangleIndex];
			for (INT i = 0; i < 3; i++)
			{
				FStaticMeshVertIndexAndZ iandz;
				iandz.Index = TriangleIndex * 3 + i;
				iandz.Z = 0.30f * Triangle.Vertices[i].X + 0.33f * Triangle.Vertices[i].Y + 0.37f * Triangle.Vertices[i].Z;
				VertIndexAndZ.AddItem(iandz);
			}
		}

		// Sort the vertices by z value
		Sort<USE_COMPARE_CONSTREF(FStaticMeshVertIndexAndZ, StaticMeshRenderDataBuild)>( VertIndexAndZ.GetTypedData(), VertIndexAndZ.Num() );

		// Search for duplicates, quickly!
		for (INT i = 0; i < VertIndexAndZ.Num(); i++)
		{
			// only need to search forward, since we add pairs both ways
			for (INT j = i + 1; j < VertIndexAndZ.Num(); j++)
			{
				if (Abs(VertIndexAndZ(j).Z - VertIndexAndZ(i).Z) > THRESH_POINTS_ARE_SAME * 4.01f)
					break; // can't be any more dups

				if(PointsEqual(
					RawTriangleData[VertIndexAndZ(i).Index/3].Vertices[VertIndexAndZ(i).Index%3],
					RawTriangleData[VertIndexAndZ(j).Index/3].Vertices[VertIndexAndZ(j).Index%3],
					bRemoveDegenerates
					))					
				{
					Vert2Duplicates.Add(VertIndexAndZ(i).Index,VertIndexAndZ(j).Index);
					Vert2Duplicates.Add(VertIndexAndZ(j).Index,VertIndexAndZ(i).Index);
				}
			}
		}
	}


	UBOOL bTooManyCollisionTris = FALSE;


	// declared in outer scope to avoid reallocation
	TArray<INT> AdjacentFaces;
	TArray<INT> DupVerts;

	// Maps a face * 3 + vertindex vert to the final index in the vertex array
	TMap<INT,INT> FinalVerts;


	// Process each triangle.
    INT	NumDegenerates = 0; 
	for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.GetElementCount();TriangleIndex++)
	{
		const FStaticMeshTriangle* Triangle = &RawTriangleData[TriangleIndex];

        FVector	VertexTangentX[3],
            VertexTangentY[3],
            VertexTangentZ[3];

        for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
        {
            VertexTangentX[VertexIndex] = FVector(0,0,0);
            VertexTangentY[VertexIndex] = FVector(0,0,0);
            VertexTangentZ[VertexIndex] = FVector(0,0,0);
        }

        FStaticMeshElement& Element = Elements(Triangle->MaterialIndex);

        if(!Triangle->bOverrideTangentBasis)
        {

            if( (PointsEqual(Triangle->Vertices[0],Triangle->Vertices[1], bRemoveDegenerates)
                ||	PointsEqual(Triangle->Vertices[0],Triangle->Vertices[2], bRemoveDegenerates)
                ||	PointsEqual(Triangle->Vertices[1],Triangle->Vertices[2], bRemoveDegenerates)
                )
                )
            {
                NumDegenerates++;
                continue;
            }

            // Calculate smooth vertex normals.
            FLOAT	Determinant = FTriple(
                TriangleTangentX(TriangleIndex),
                TriangleTangentY(TriangleIndex),
                TriangleTangentZ(TriangleIndex)
                );

            // Determine contributing faces for correct smoothing group behaviour  according to the orthodox Max interpretation of smoothing groups.    O(n^2)      - EDN

            // Start building a list of faces adjacent to this triangle
            AdjacentFaces.Reset();
            for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
            {
                INT vert = TriangleIndex * 3 +  VertexIndex;
                DupVerts.Reset();
                Vert2Duplicates.MultiFind(vert,DupVerts);
                DupVerts.AddItem(vert); // I am a "dup" of myself
                for (INT k = 0; k < DupVerts.Num(); k++)
                {
                    AdjacentFaces.AddUniqueItem(DupVerts(k) / 3);
                }
            }

            // we need to sort these here because the criteria for point equality is exact, so we must ensure the exact same order for all dups
            Sort<USE_COMPARE_CONSTREF(INT, StaticMeshRenderDataBuild)>( AdjacentFaces.GetTypedData(), AdjacentFaces.Num() );

            TArray<FanFace> RelevantFacesForVertex[3];

            // Process adjacent faces
            for(INT AdjacentFaceIndex = 0;AdjacentFaceIndex < AdjacentFaces.Num();AdjacentFaceIndex++)
            {
                INT OtherTriangleIndex = AdjacentFaces(AdjacentFaceIndex);
                for(INT OurVertexIndex = 0; OurVertexIndex < 3; OurVertexIndex++)
                {		
                    const FStaticMeshTriangle* OtherTriangle = &RawTriangleData[OtherTriangleIndex];
                    FanFace NewFanFace;
                    INT CommonIndexCount = 0;				
                    // Check for vertices in common.
                    if(TriangleIndex == OtherTriangleIndex)
                    {
                        CommonIndexCount = 3;		
                        NewFanFace.LinkedVertexIndex = OurVertexIndex;
                    }
                    else
                    {
                        // Check matching vertices against main vertex .
                        for(INT OtherVertexIndex=0; OtherVertexIndex<3; OtherVertexIndex++)
                        {
                            if( PointsEqual(Triangle->Vertices[OurVertexIndex], OtherTriangle->Vertices[OtherVertexIndex], bRemoveDegenerates) )
                            {
                                CommonIndexCount++;
                                NewFanFace.LinkedVertexIndex = OtherVertexIndex;
                            }
                        }
                    }
                    //Add if connected by at least one point. Smoothing matches are considered later.
                    if(CommonIndexCount > 0)
                    { 					
                        NewFanFace.FaceIndex = OtherTriangleIndex;
                        NewFanFace.Filled = ( OtherTriangleIndex == TriangleIndex ); // Starter face for smoothing floodfill.
                        RelevantFacesForVertex[OurVertexIndex].AddItem( NewFanFace );
                    }
                }
            }

            // Find true relevance of faces for a vertex normal by traversing smoothing-group-compatible connected triangle fans around common vertices.

            for(INT VertexIndex = 0; VertexIndex < 3; VertexIndex++)
            {
                INT NewConnections = 1;
                while( NewConnections )
                {
                    NewConnections = 0;
                    for( INT OtherFaceIdx=0; OtherFaceIdx < RelevantFacesForVertex[VertexIndex].Num(); OtherFaceIdx++ )
                    {															
                        // The vertex' own face is initially the only face with  .Filled == true.
                        if( RelevantFacesForVertex[VertexIndex]( OtherFaceIdx ).Filled )  
                        {				
                            const FStaticMeshTriangle* OtherTriangle = &RawTriangleData[ RelevantFacesForVertex[VertexIndex](OtherFaceIdx).FaceIndex ];
                            for( INT MoreFaceIdx = 0; MoreFaceIdx < RelevantFacesForVertex[VertexIndex].Num(); MoreFaceIdx ++ )
                            {								
                                if( ! RelevantFacesForVertex[VertexIndex]( MoreFaceIdx).Filled )
                                {
                                    const FStaticMeshTriangle* FreshTriangle = &RawTriangleData[ RelevantFacesForVertex[VertexIndex](MoreFaceIdx).FaceIndex ];
                                    if( ( FreshTriangle->SmoothingMask &  OtherTriangle->SmoothingMask ) &&  ( MoreFaceIdx != OtherFaceIdx) )
                                    {				
                                        INT CommonVertices = 0;
                                        for(INT OtherVertexIndex = 0; OtherVertexIndex < 3; OtherVertexIndex++)
                                        {											
                                            for(INT OrigVertexIndex = 0; OrigVertexIndex < 3; OrigVertexIndex++)
                                            {
                                                if( PointsEqual ( FreshTriangle->Vertices[OrigVertexIndex],  OtherTriangle->Vertices[OtherVertexIndex], bRemoveDegenerates  )	)
                                                {
                                                    CommonVertices++;
                                                }
                                            }										
                                        }
                                        // Flood fill faces with more than one common vertices which must be touching edges.
                                        if( CommonVertices > 1)
                                        {
                                            RelevantFacesForVertex[VertexIndex]( MoreFaceIdx).Filled = true;
                                            NewConnections++;
                                        }								
                                    }
                                }
                            }
                        }
                    }
                } 
            }
        

		    // Vertex normal construction.

		    for(INT VertexIndex = 0; VertexIndex < 3; VertexIndex++)
		    {
			    for(INT RelevantFaceIdx = 0; RelevantFaceIdx < RelevantFacesForVertex[VertexIndex].Num(); RelevantFaceIdx++)
			    {
				    if( RelevantFacesForVertex[VertexIndex](RelevantFaceIdx).Filled )
				    {
					    INT OtherTriangleIndex = RelevantFacesForVertex[VertexIndex]( RelevantFaceIdx).FaceIndex;
					    INT OtherVertexIndex	= RelevantFacesForVertex[VertexIndex]( RelevantFaceIdx).LinkedVertexIndex;

					    const FStaticMeshTriangle*	OtherTriangle = &RawTriangleData[OtherTriangleIndex];
					    FLOAT OtherDeterminant = FTriple(
						    TriangleTangentX(OtherTriangleIndex),
						    TriangleTangentY(OtherTriangleIndex),
						    TriangleTangentZ(OtherTriangleIndex)
						    );

					    if( Determinant * OtherDeterminant > 0.0f &&
						    StaticMesh_UVsEqual(Triangle->UVs[VertexIndex][0],OtherTriangle->UVs[OtherVertexIndex][0]) )
                        {
                            VertexTangentX[VertexIndex] += TriangleTangentX(OtherTriangleIndex);
                            VertexTangentY[VertexIndex] += TriangleTangentY(OtherTriangleIndex);
                        }
                        VertexTangentZ[VertexIndex] += TriangleTangentZ(OtherTriangleIndex);
				    }
			    }
		    }
        }


		// Normalization.
        for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
        {
            if( Triangle->bOverrideTangentBasis && !Triangle->TangentX[VertexIndex].IsZero() )
            {
                VertexTangentX[VertexIndex] = Triangle->TangentX[VertexIndex];
            }
            if( Triangle->bOverrideTangentBasis && !Triangle->TangentY[VertexIndex].IsZero() )
            {
                VertexTangentY[VertexIndex] = Triangle->TangentY[VertexIndex];
            }
            if( ( Triangle->bOverrideTangentBasis || Triangle->bExplicitNormals ) && !Triangle->TangentZ[VertexIndex].IsZero() )
            {
                VertexTangentZ[VertexIndex] = Triangle->TangentZ[VertexIndex];
            }

            if(!Triangle->bOverrideTangentBasis)
            {
                VertexTangentX[VertexIndex].Normalize();
                VertexTangentY[VertexIndex].Normalize();
                VertexTangentZ[VertexIndex].Normalize();
                // Gram-Schmidt orthogonalization
                VertexTangentY[VertexIndex] -= VertexTangentX[VertexIndex] * (VertexTangentX[VertexIndex] | VertexTangentY[VertexIndex]);
                VertexTangentY[VertexIndex].Normalize();

                VertexTangentX[VertexIndex] -= VertexTangentZ[VertexIndex] * (VertexTangentZ[VertexIndex] | VertexTangentX[VertexIndex]);
                VertexTangentX[VertexIndex].Normalize();
                VertexTangentY[VertexIndex] -= VertexTangentZ[VertexIndex] * (VertexTangentZ[VertexIndex] | VertexTangentY[VertexIndex]);
                VertexTangentY[VertexIndex].Normalize();
            }

        }

		// Index the triangle's vertices.

		INT	VertexIndices[3];

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			INT Vert = TriangleIndex * 3 +  VertexIndex;
			DupVerts.Reset();
			Vert2Duplicates.MultiFind(Vert,DupVerts);

			// we sort here to get the identical behavior as the old algorithm and also so we can stop early
			Sort<USE_COMPARE_CONSTREF(INT, StaticMeshRenderDataBuild)>( DupVerts.GetTypedData(), DupVerts.Num() );
			INT Index = INDEX_NONE;
			for (INT k = 0; k < DupVerts.Num(); k++)
			{
				if (DupVerts(k) >= Vert)
				{
					// the verts beyond me haven't been placed yet, so these duplicates are not relevant
					break;
				}

				INT *Location = FinalVerts.Find(DupVerts(k));
				if (Location)
				{
					if (CompareVertex(
											Triangle->Vertices[VertexIndex],
											VertexTangentX[VertexIndex],
											VertexTangentY[VertexIndex],
											VertexTangentZ[VertexIndex],
											Triangle->Colors[VertexIndex],
											Triangle->UVs[VertexIndex],
											Triangle->NumUVs,
											Triangle->FragmentIndex,
											Vertices(*Location),
											bSingleTangentSetOverride,
											bRemoveDegenerates ))
					{
						Index = *Location;
						break;
					}
				}

			}
			if (Index == INDEX_NONE)
			{
				Index = AddVertex(
											Triangle->Vertices[VertexIndex],
 											VertexTangentX[VertexIndex],
 											VertexTangentY[VertexIndex],
 											VertexTangentZ[VertexIndex],
											Triangle->Colors[VertexIndex],
											Triangle->UVs[VertexIndex],
											Triangle->NumUVs,
											Triangle->FragmentIndex,
											Vertices
											);
				FinalVerts.Set(Vert,Index);
			}
			VertexIndices[VertexIndex] = Index;
		}

		// Reject degenerate triangles.

		if(VertexIndices[0] == VertexIndices[1] || VertexIndices[1] == VertexIndices[2] || VertexIndices[0] == VertexIndices[2])
		{
			continue;
		}

		// Put the indices in the material index buffer.

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			ElementFragmentIndexBuffers(Triangle->MaterialIndex * NumFragments + Triangle->FragmentIndex).Indices.AddItem(VertexIndices[VertexIndex]);
		}

		if(Element.EnableCollision && BuildKDop)
		{
			// Build a new kDOP collision triangle
			if (kDOPBuildTriangles.Num() < 65535)
			{
				new (kDOPBuildTriangles) FkDOPBuildCollisionTriangle<WORD>(VertexIndices[0],
					VertexIndices[1],VertexIndices[2],Triangle->MaterialIndex * NumFragments + Triangle->FragmentIndex,
					Triangle->Vertices[0],Triangle->Vertices[1],Triangle->Vertices[2]);
			}
			else
			{
				// We don't support more than 65535 collision triangles
				bTooManyCollisionTris = TRUE;
			}
		}
	}

	if( bTooManyCollisionTris && !bSilent )
	{
		appMsgf( AMT_OK,TEXT( "Model has too many faces for collision.  Only the first 65535 faces will support collision.  Consider adding extra materials to split up the source mesh into smaller chunks." ) );
	}

	// Initialize the vertex buffer.
	NumVertices = Vertices.Num();
	VertexBuffer.Init(Vertices,NumTexCoords);
	PositionVertexBuffer.Init(Vertices);
	ColorVertexBuffer.Init(Vertices);

	if(NumDegenerates)
	{
    	debugf(TEXT("StaticMesh had %i degenerates"), NumDegenerates );
	}


	// Build a cache optimized triangle list for each material and copy it into the shared index buffer.
	for(INT ElementIndex = 0;ElementIndex < Elements.Num();ElementIndex++)
	{
		// Get element ranges
		FStaticMeshElement& Element = Elements(ElementIndex);
		Element.FirstIndex = IndexBuffer.Indices.Num();
		Element.NumTriangles = 0;
		Element.MinVertexIndex = NumVertices;
		Element.MaxVertexIndex = 0;
		Element.Fragments.Empty(NumFragments);
		Element.Fragments.AddZeroed(NumFragments);

		UBOOL bFoundValidFragment = FALSE;
		// Get fragment ranges
		for(INT FragmentIndex = 0;FragmentIndex < NumFragments;FragmentIndex++)
		{
			FRawStaticIndexBuffer& ElementFragmentIndexBuffer = ElementFragmentIndexBuffers(ElementIndex * NumFragments + FragmentIndex);
			FFragmentRange& FragmentRange = Element.Fragments(FragmentIndex);
			if(ElementFragmentIndexBuffer.Indices.Num())
			{
				ElementFragmentIndexBuffer.CacheOptimize();

				FragmentRange.BaseIndex = IndexBuffer.Indices.Num();
				FragmentRange.NumPrimitives = ElementFragmentIndexBuffer.Indices.Num() / 3;

				WORD*	DestPtr = &IndexBuffer.Indices(IndexBuffer.Indices.Add(ElementFragmentIndexBuffer.Indices.Num()));
				WORD*	SrcPtr = &ElementFragmentIndexBuffer.Indices(0);

				Element.NumTriangles += ElementFragmentIndexBuffer.Indices.Num() / 3;
				Element.MinVertexIndex = Min<UINT>(Element.MinVertexIndex, *SrcPtr);
				Element.MaxVertexIndex = Max<UINT>(Element.MaxVertexIndex, *SrcPtr);

				for(INT Index = 0;Index < ElementFragmentIndexBuffer.Indices.Num();Index++)
				{
					Element.MinVertexIndex = Min<UINT>(*SrcPtr,Element.MinVertexIndex);
					Element.MaxVertexIndex = Max<UINT>(*SrcPtr,Element.MaxVertexIndex);

					*DestPtr++ = *SrcPtr++;
				}

				bFoundValidFragment = TRUE;
			}
		}

		if (!bFoundValidFragment)
		{
			Element.FirstIndex = 0;
			Element.NumTriangles = 0;
			Element.MinVertexIndex = 0;
			Element.MaxVertexIndex = 0;
		}
	}

	
	// Build a list of wireframe edges in the static mesh.
//	FEdgeBuilder(IndexBuffer.Indices,Vertices,Edges).FindEdges();
	FStaticMeshEdgeBuilder(IndexBuffer.Indices,Vertices,Edges).FindEdges();

	// Pre-size the wireframe indices array to avoid extra memcpys
	WireframeIndexBuffer.Indices.Empty(2 * Edges.Num());

	for(INT EdgeIndex = 0;EdgeIndex < Edges.Num();EdgeIndex++)
	{
		FMeshEdge&	Edge = Edges(EdgeIndex);

		WireframeIndexBuffer.Indices.AddItem(Edge.Vertices[0]);
		WireframeIndexBuffer.Indices.AddItem(Edge.Vertices[1]);
	}

#if WITH_EDITOR && WITH_D3D11_TESSELLATION
	// Create adjacency index buffer.
	if ( GRHIShaderPlatform == SP_PCD3D_SM5 )
	{
		BuildStaticAdjacencyIndexBuffer( PositionVertexBuffer, VertexBuffer, IndexBuffer.Indices, AdjacencyIndexBuffer.Indices );
	}
#endif // #if WITH_EDITOR && WITH_D3D11_TESSELLATION

	RawTriangles.Unlock();
#else
	appErrorf(TEXT("FStaticMeshRenderData::Build should not be called on a console"));
#endif
}

IMPLEMENT_COMPARE_CONSTREF( FLOAT, UnStaticMeshBuild, { return (B - A) > 0 ? 1 : -1 ; } )


/**
 * Returns the scale dependent texture factor used by the texture streaming code.	
 *
 * @param RequestedUVIndex UVIndex to look at
 * @return scale dependent texture factor
 */
FLOAT UStaticMesh::GetStreamingTextureFactor( INT RequestedUVIndex)
{
	check(RequestedUVIndex >= 0);
	check(RequestedUVIndex < MAX_TEXCOORDS);

	// If the streaming texture factor cache doesn't have the right number of entries, it needs to be updated.
	if(CachedStreamingTextureFactors.Num() != MAX_TEXCOORDS)
	{
		if(!CONSOLE)
		{
			// Reset the cached texture factors.
			CachedStreamingTextureFactors.Empty(MAX_TEXCOORDS);
			CachedStreamingTextureFactors.AddZeroed(MAX_TEXCOORDS);

			FStaticMeshRenderData& StaticMeshRenderData = LODModels(0);
			WORD* SrcIndices = 0;
			
			if (StaticMeshRenderData.GetTriangleCount())
			{
				SrcIndices = (WORD*)StaticMeshRenderData.IndexBuffer.Indices.GetResourceData();
			}

			if( SrcIndices )
			{
				//Lock Index Buffer
				INT NumTriangles = StaticMeshRenderData.GetTriangleCount();
				INT NumIndices   = NumTriangles*3;

				TArray<FLOAT> TexelRatios[MAX_TEXCOORDS];
				FLOAT MaxTexelRatio = 0.0f;

				for(INT UVIndex = 0;UVIndex < MAX_TEXCOORDS;UVIndex++)
				{
					TexelRatios[UVIndex].Empty( StaticMeshRenderData.GetTriangleCount() );
				}

				for (INT ElementScan = 0; ElementScan < StaticMeshRenderData.Elements.Num(); ++ElementScan)
				{
					// Ignore triangles whose material index matches ElementToIgnoreForTexFactor
					const FStaticMeshElement& MeshElement = StaticMeshRenderData.Elements(ElementScan);
					if (MeshElement.MaterialIndex == ElementToIgnoreForTexFactor)
					{
						continue;
					}

					// Figure out Unreal unit per texel ratios.
					for(UINT TriangleIndex=0; TriangleIndex<MeshElement.NumTriangles; TriangleIndex++ )
					{
						//retrieve indices
						WORD Index0 = SrcIndices[MeshElement.FirstIndex + TriangleIndex*3];
						WORD Index1 = SrcIndices[MeshElement.FirstIndex + TriangleIndex*3+1];
						WORD Index2 = SrcIndices[MeshElement.FirstIndex + TriangleIndex*3+2];

						const FVector& Pos0 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Index0);
						const FVector& Pos1 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Index1);
						const FVector& Pos2 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Index2);
						FLOAT	L1	= (Pos0 - Pos1).Size(),
								L2	= (Pos0 - Pos2).Size();

						INT NumUVs = StaticMeshRenderData.VertexBuffer.GetNumTexCoords();
						for(INT UVIndex = 0;UVIndex < Min(NumUVs,(INT)MAX_TEXCOORDS);UVIndex++)
						{
							FVector2D UV0 = StaticMeshRenderData.VertexBuffer.GetVertexUV(Index0, UVIndex);
							FVector2D UV1 = StaticMeshRenderData.VertexBuffer.GetVertexUV(Index1, UVIndex);
							FVector2D UV2 = StaticMeshRenderData.VertexBuffer.GetVertexUV(Index2, UVIndex);

							FLOAT	T1	= (UV0 - UV1).Size(),
									T2	= (UV0 - UV2).Size();

							if( Abs(T1 * T2) > Square(SMALL_NUMBER) )
							{
								const FLOAT TexelRatio = Max( L1 / T1, L2 / T2 );
								TexelRatios[UVIndex].AddItem( TexelRatio );

								// Update max texel ratio
								if( TexelRatio > MaxTexelRatio )
								{
									MaxTexelRatio = TexelRatio;
								}
							}
						}
					}
				}

				for(INT UVIndex = 0;UVIndex < MAX_TEXCOORDS;UVIndex++)
				{
					// If the mesh is configured to use the maximum ratio for all triangles in the mesh
					// then we'll use that.  This is great for meshes that are known to have virtually no
					// "wasted" UV mappings.  That is, large triangles that map to texels that won't
					// usually be visible in game.  For meshes that we create procedurally (such as ProcBuilding
					// LOD meshes), it's desirable to use the maximum texel ratio for streaming mips.
					if( bUseMaximumStreamingTexelRatio )
					{
						CachedStreamingTextureFactors(UVIndex) = MaxTexelRatio * StreamingDistanceMultiplier;
					}
					else
					{
						if( TexelRatios[UVIndex].Num() )
						{
							// Disregard upper 75% of texel ratios.
							// This is to ignore backfacing surfaces or other non-visible surfaces that tend to map a small number of texels to a large surface.
							Sort<USE_COMPARE_CONSTREF(FLOAT,UnStaticMeshBuild)>( &(TexelRatios[UVIndex](0)), TexelRatios[UVIndex].Num() );
							FLOAT TexelRatio = TexelRatios[UVIndex]( appTrunc(TexelRatios[UVIndex].Num() * 0.75f) );
							if ( UVIndex == 0 )
							{
								TexelRatio *= StreamingDistanceMultiplier;
							}
							CachedStreamingTextureFactors(UVIndex) = TexelRatio;
						}
					}
				}
			}
		}
		else
		{
			// Streaming texture factors cannot be computed on consoles, since the raw data has been cooked out.
			debugfSuppressed( TEXT("UStaticMesh::GetStreamingTextureFactor is being called on the console which is slow.  You need to resave the map to have the editor precalculate the StreamingTextureFactor for:  %s  Please resave your map. If you are calling this directly then we just return 0.0f instead of appErrorfing"), *GetFullName() );
			return 0.0f;
		}		
	}

	return CachedStreamingTextureFactors(RequestedUVIndex);
}

#if WITH_EDITOR
struct FStaticMeshMergeElement
{
	FVector2D LightMapScale;
	FVector2D LightMapBias;
	UMaterialInterface* Material;
	UStaticMeshComponent* Component;
	INT ElementIndex;
	INT LightMapCoordIndex;
};

static INT MergedStaticMesh_CompareElements( const FStaticMeshMergeElement& A, const FStaticMeshMergeElement& B )
{
	if ( A.Material != B.Material )
	{
		return A.Material - B.Material;
	}
	if ( A.Component->StaticMesh != B.Component->StaticMesh )
	{
		return A.Component->StaticMesh - B.Component->StaticMesh;
	}
	if ( A.ElementIndex != B.ElementIndex )
	{
		return A.ElementIndex - B.ElementIndex;
	}
	return A.Component - B.Component;
}
IMPLEMENT_COMPARE_CONSTREF( FStaticMeshMergeElement, UnStaticMesh, { return MergedStaticMesh_CompareElements( A, B ); } );

static void MergedStaticMesh_CombineCollision( URB_BodySetup* SrcBody, const FMatrix& Transform, URB_BodySetup* DestBody )
{
	const FKAggregateGeom& SrcGeometry = SrcBody->AggGeom;
	FKAggregateGeom& DestGeometry = DestBody->AggGeom;

	// Transform spheres.
	const INT SrcSphereCount = SrcGeometry.SphereElems.Num();
	for ( INT SphereIndex = 0; SphereIndex < SrcSphereCount; ++SphereIndex )
	{
		const FKSphereElem& SrcSphere = SrcGeometry.SphereElems(SphereIndex);
		FKSphereElem& NewSphere = *new(DestGeometry.SphereElems) FKSphereElem();
		NewSphere.TM = SrcSphere.TM * Transform;
		NewSphere.Radius = SrcSphere.Radius;
		NewSphere.bNoRBCollision = SrcSphere.bNoRBCollision;
		NewSphere.bPerPolyShape = SrcSphere.bPerPolyShape;
	}

	// Transform boxes.
	const INT SrcBoxCount = SrcGeometry.BoxElems.Num();
	for ( INT BoxIndex = 0; BoxIndex < SrcBoxCount; ++BoxIndex )
	{
		const FKBoxElem& SrcBox = SrcGeometry.BoxElems(BoxIndex);
		FKBoxElem& NewBox = *new(DestGeometry.BoxElems) FKBoxElem();
		NewBox.TM = SrcBox.TM * Transform;
		NewBox.X = SrcBox.X;
		NewBox.Y = SrcBox.Y;
		NewBox.Z = SrcBox.Z;
		NewBox.bNoRBCollision = SrcBox.bNoRBCollision;
		NewBox.bPerPolyShape = SrcBox.bPerPolyShape;
	}

	// Transform sphyls.
	const INT SrcSphylCount = SrcGeometry.SphylElems.Num();
	for ( INT SphylIndex = 0; SphylIndex < SrcSphylCount; ++SphylIndex )
	{
		const FKSphylElem& SrcSphyl = SrcGeometry.SphylElems(SphylIndex);
		FKSphylElem& NewSphyl = *new(DestGeometry.SphylElems) FKSphylElem();
		NewSphyl.TM = SrcSphyl.TM * Transform;
		NewSphyl.Radius = SrcSphyl.Radius;
		NewSphyl.Length = SrcSphyl.Length;
		NewSphyl.bNoRBCollision = SrcSphyl.bNoRBCollision;
		NewSphyl.bPerPolyShape = SrcSphyl.bPerPolyShape;
	}

	// Transform convex elements.
	const INT SrcConvexCount = SrcGeometry.ConvexElems.Num();
	for ( INT ConvexIndex = 0; ConvexIndex < SrcConvexCount; ++ConvexIndex )
	{
		const FKConvexElem& SrcConvex = SrcGeometry.ConvexElems(ConvexIndex);
		const INT NewConvexIndex = DestGeometry.ConvexElems.AddZeroed();
		FKConvexElem& NewConvex = DestGeometry.ConvexElems(NewConvexIndex);
		const INT VertexCount = SrcConvex.VertexData.Num();
		NewConvex.VertexData.Add( VertexCount );
		const FVector* SrcVerts = SrcConvex.VertexData.GetTypedData();
		FVector* DestVerts = NewConvex.VertexData.GetTypedData();
		for ( INT VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex )
		{
			DestVerts[VertexIndex] = Transform.TransformFVector( SrcVerts[VertexIndex] );
		}
		NewConvex.GenerateHullData();
	}
}

static void MergedStaticMesh_BuildFromElements( UStaticMesh* StaticMesh, const TArray<FStaticMeshMergeElement>& MergeElements, const FVector& MergeOrigin )
{
	check( StaticMesh );

	// Find a light map coordinate index for the mesh.
	const INT MergeElementCount = MergeElements.Num();
	INT TexCoordCount = 0;
	INT DestLightMapCoordIndex = MergeElements(0).LightMapCoordIndex;
	if ( DestLightMapCoordIndex == 0 )
	{
		DestLightMapCoordIndex = MergeElements(0).Component->StaticMesh->LODModels(0).VertexBuffer.GetNumTexCoords();
	}
	for ( INT MergeElementIndex = 0; MergeElementIndex < MergeElementCount; ++MergeElementIndex )
	{
		const FStaticMeshMergeElement& MergeElement = MergeElements( MergeElementIndex );
		const INT ElementTexCoordCount = MergeElement.Component->StaticMesh->LODModels(0).VertexBuffer.GetNumTexCoords();
		TexCoordCount = Max<INT>( TexCoordCount, ElementTexCoordCount );
		if ( DestLightMapCoordIndex != MergeElement.LightMapCoordIndex && DestLightMapCoordIndex < ElementTexCoordCount )
		{
			DestLightMapCoordIndex = ElementTexCoordCount;
		}
	}
	check( TexCoordCount <= 8 );
	check( DestLightMapCoordIndex < 7 );
	debugf( TEXT("++ DestLightMapCoordIndex = %d"), DestLightMapCoordIndex );
	StaticMesh->LightMapCoordinateIndex = DestLightMapCoordIndex;

	// Create a new LOD model.
	FStaticMeshRenderData* LODModel = new ( StaticMesh->LODModels ) FStaticMeshRenderData();
	FStaticMeshLODInfo* LODInfo = new ( StaticMesh->LODInfo ) FStaticMeshLODInfo();
	check( StaticMesh->LODModels.Num() == 1 );
	check( StaticMesh->LODInfo.Num() == 1 );

	// Merge all triangles.
	TArray<FStaticMeshTriangle> Triangles;
	TArray<UMaterialInterface*> UniqueMaterials;
	for ( INT MergeElementIndex = 0; MergeElementIndex < MergeElementCount; ++MergeElementIndex )
	{
		const FStaticMeshMergeElement& MergeElement = MergeElements( MergeElementIndex );
		const FStaticMeshComponentLODInfo& SrcLODInfo = MergeElement.Component->LODData( 0 );
		FStaticMeshRenderData& SrcLODModel = MergeElement.Component->StaticMesh->LODModels( 0 );
		const FStaticMeshElement& SrcElement = MergeElement.Component->StaticMesh->LODModels( 0 ).Elements( MergeElement.ElementIndex );
		const INT SrcMaterialIndex = MergeElement.ElementIndex;
		const INT DestMaterialIndex = UniqueMaterials.AddUniqueItem( MergeElement.Material );
		const FStaticMeshTriangle* SrcTriangles = (const FStaticMeshTriangle*)SrcLODModel.RawTriangles.Lock( LOCK_READ_ONLY );
		const INT SrcTriCount = SrcLODModel.RawTriangles.GetElementCount();
		FMatrix ElementToMergeSpace = MergeElement.Component->LocalToWorld.ConcatTranslation( -MergeOrigin );
		const UBOOL bReverseWinding = MergeElement.Component->LocalToWorldDeterminant < 0.0f;
		INT SrcLightMapCoordIndex = MergeElement.LightMapCoordIndex;
		if ( SrcLightMapCoordIndex == 0 )
		{
			warnf( TEXT("!! Spoofing LightMapCoord for %s [%s]"), *MergeElement.Component->GetPathName(), *MergeElement.Component->StaticMesh->GetPathName() );
			warnf( TEXT("     SrcCoordIndex=%d DestCoordIndex=%d"), SrcLightMapCoordIndex, DestLightMapCoordIndex );
		}
		check( SrcLightMapCoordIndex >= 0 && SrcLightMapCoordIndex < 8 );
		check( DestLightMapCoordIndex >= 0 && DestLightMapCoordIndex < 8 );

		// Generate merged triangles.
		for ( INT SrcTriIndex = 0; SrcTriIndex < SrcTriCount; ++SrcTriIndex )
		{
			const FStaticMeshTriangle& SrcTri = SrcTriangles[SrcTriIndex];
			if ( SrcTri.MaterialIndex == SrcMaterialIndex )
			{
				FStaticMeshTriangle* DestTri = new( Triangles ) FStaticMeshTriangle;
				appMemZero( *DestTri );
				DestTri->MaterialIndex = DestMaterialIndex;
				DestTri->FragmentIndex = 0;
				DestTri->SmoothingMask = SrcTri.SmoothingMask;
				DestTri->NumUVs = TexCoordCount;
				DestTri->bOverrideTangentBasis = FALSE;
				DestTri->bExplicitNormals = FALSE;

				for ( INT VertexIndex = 0; VertexIndex < 3; ++VertexIndex )
				{
					const INT SrcIndex = bReverseWinding ? (2 - VertexIndex) : VertexIndex;
					DestTri->Vertices[VertexIndex] = ElementToMergeSpace.TransformFVector( SrcTri.Vertices[SrcIndex] );
					DestTri->Colors[VertexIndex] = SrcTri.Colors[SrcIndex];

					for ( INT TexCoordIndex = 0; TexCoordIndex < TexCoordCount; ++TexCoordIndex )
					{
						DestTri->UVs[VertexIndex][TexCoordIndex] = SrcTri.UVs[SrcIndex][TexCoordIndex];
					}

					DestTri->UVs[VertexIndex][DestLightMapCoordIndex].X = SrcTri.UVs[SrcIndex][SrcLightMapCoordIndex].X * MergeElement.LightMapScale.X + MergeElement.LightMapBias.X;
					DestTri->UVs[VertexIndex][DestLightMapCoordIndex].Y = SrcTri.UVs[SrcIndex][SrcLightMapCoordIndex].Y * MergeElement.LightMapScale.Y + MergeElement.LightMapBias.Y;
				}
			}
		}
		SrcLODModel.RawTriangles.Unlock();

		// Generate merged collision elements.
		if ( MergeElement.Component->StaticMesh->BodySetup )
		{
			if ( StaticMesh->BodySetup == NULL )
			{
				StaticMesh->BodySetup = ConstructObject<URB_BodySetup>( URB_BodySetup::StaticClass(), StaticMesh );
			}
			MergedStaticMesh_CombineCollision( MergeElement.Component->StaticMesh->BodySetup, ElementToMergeSpace, StaticMesh->BodySetup );
		}
	}

	// Create elements.
	const INT MaterialCount = UniqueMaterials.Num();
	for ( INT MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex )
	{
		FStaticMeshElement* Element = new( LODModel->Elements ) FStaticMeshElement( UniqueMaterials( MaterialIndex ), MaterialIndex );
		Element->EnableCollision = TRUE;
		Element->OldEnableCollision = TRUE;
	}

	// Dump triangles in to the LOD model.
	LODModel->RawTriangles.RemoveBulkData();
	LODModel->RawTriangles.Lock( LOCK_READ_WRITE );
	void* RawTriangleData = LODModel->RawTriangles.Realloc( Triangles.Num() );
	appMemcpy( RawTriangleData, Triangles.GetTypedData(), Triangles.GetTypeSize() * Triangles.Num() );
	LODModel->RawTriangles.Unlock();

	// Finally, build the mesh.
	StaticMesh->Build( FALSE, FALSE );
}

#include "UnTextureLayout.h"

UBOOL StaticMesh_MergeComponents( UObject* Outer, const TArray<UStaticMeshComponent*>& ComponentsToMerge, UStaticMesh** OutStaticMesh, FVector* OutMergeOrigin )
{
	// This is an approximate number. 2^16 is conservative, but needed for collision.
	const UINT MaxTrianglesPerMesh = (1 << 16);

	check( OutStaticMesh );
	check( OutMergeOrigin );
	check( ComponentsToMerge.Num() > 0 );

	// First find out how many texels will be needed for lightmaps.
	INT ComponentCount = ComponentsToMerge.Num();
	const INT LightMapPadding = 2;
	INT LightMapTexels = 0;
	INT MaxRes = 0;
	for ( INT ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex )
	{
		UStaticMeshComponent* Component = ComponentsToMerge( ComponentIndex );
		check( Component );
		check( Component->StaticMesh );
		check( Component->StaticMesh->LODModels.Num() );
		INT ResX = 0;
		INT ResY = 0;
		Component->GetLightMapResolution( ResX, ResY );
		ResX += LightMapPadding * 2;
		ResY += LightMapPadding * 2;
		LightMapTexels += (ResX * ResY);
		MaxRes = Max<INT>( MaxRes, Max<INT>( ResX, ResY ) );
	}
	INT MinRes = appCeil( appSqrt( (FLOAT)LightMapTexels ) );
	MaxRes = Max<INT>( MinRes, appCeil( appSqrt( (FLOAT)ComponentCount ) ) * MaxRes );
	debugf( TEXT("++ MinRes=%d MaxRes=%d"), MinRes, MaxRes );

	// Layout each component in the atlas.
	FTextureLayout LightMapAtlasLayout( MinRes, MinRes, MaxRes, MaxRes, FALSE, FALSE );
	TArray<FIntRect> LightMapRects;
	for ( INT ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex )
	{
		UStaticMeshComponent* Component = ComponentsToMerge( ComponentIndex );
		INT ResX = 0;
		INT ResY = 0;
		Component->GetLightMapResolution( ResX, ResY );
		ResX += LightMapPadding * 2;
		ResY += LightMapPadding * 2;
		UINT X = 0;
		UINT Y = 0;
		if ( LightMapAtlasLayout.AddElement( X, Y, ResX, ResY ) == FALSE )
		{
			warnf( TEXT("!! Cannot assign light map rect for %s [%s]"), *Component->GetPathName(), *Component->StaticMesh->GetPathName() );
			warnf( TEXT("     ResX=%d ResY=%d MinRes=%d MaxRes=%d"), ResX, ResY, MinRes, MaxRes );
		}
		FIntRect* Rect = new( LightMapRects ) FIntRect;
		Rect->Min.X = X + LightMapPadding;
		Rect->Min.Y = Y + LightMapPadding;
		Rect->Max.X = X + ResX - LightMapPadding;
		Rect->Max.Y = Y + ResY - LightMapPadding;
		debugf( TEXT("++ Light map rect for %s [%s]"), *Component->GetPathName(), *Component->StaticMesh->GetPathName() );
		debugf( TEXT("     X=%d Y=%d Width=%d Height=%d"), Rect->Min.X, Rect->Min.Y, Rect->Max.X - Rect->Min.X, Rect->Max.Y - Rect->Min.Y );
	}
	check( LightMapRects.Num() == ComponentCount );
	FLOAT LightMapWidth = (FLOAT)LightMapAtlasLayout.GetSizeX();
	FLOAT LightMapHeight = (FLOAT)LightMapAtlasLayout.GetSizeY();
	debugf( TEXT("++ Light map size: %dx%d"), LightMapAtlasLayout.GetSizeX(), LightMapAtlasLayout.GetSizeY() );

	// Break the components in to individual elements and sort.
	TArray<FStaticMeshMergeElement> ElementsToMerge;
	FBox MergedBounds;
	MergedBounds.Init();
	INT TotalTriCount = 0;
	for ( INT ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex )
	{
		UStaticMeshComponent* Component = ComponentsToMerge( ComponentIndex );
		FIntRect& LightMapRect = LightMapRects( ComponentIndex );
		FVector2D LightMapScale(
			(FLOAT)LightMapRect.Width() / LightMapWidth,
			(FLOAT)LightMapRect.Height() / LightMapHeight );
		FVector2D LightMapBias(
			(FLOAT)LightMapRect.Min.X / LightMapWidth,
			(FLOAT)LightMapRect.Min.Y / LightMapHeight );
		MergedBounds += Component->Bounds.GetBox();
		const FStaticMeshRenderData& LODModel = Component->StaticMesh->LODModels( 0 );
		TotalTriCount += LODModel.RawTriangles.GetElementCount();
		INT ElementCount = LODModel.Elements.Num();
		for ( INT ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex )
		{
			UMaterialInterface* Material = Component->GetMaterial( ElementIndex, /*LOD*/ 0 );
			if ( Material )
			{
				FStaticMeshMergeElement* MergeElement = new( ElementsToMerge ) FStaticMeshMergeElement;
				MergeElement->LightMapScale = LightMapScale;
				MergeElement->LightMapBias = LightMapBias;
				MergeElement->Material = Material;
				MergeElement->Component = Component;
				MergeElement->ElementIndex = ElementIndex;
				MergeElement->LightMapCoordIndex = Component->HasLightmapTextureCoordinates() ? Component->StaticMesh->LightMapCoordinateIndex : 0;
			}
		}
	}

	if ( TotalTriCount > MaxTrianglesPerMesh )
	{
		appMsgf( AMT_OK, TEXT("Merged meshes may not have more than %d triangles. The selected mesh instances have a total of %d triangles."),
			MaxTrianglesPerMesh, TotalTriCount );
		return FALSE;
	}

	if ( ElementsToMerge.Num() == 0 )
	{
		return FALSE;
	}

	Sort<USE_COMPARE_CONSTREF( FStaticMeshMergeElement, UnStaticMesh )>( &ElementsToMerge(0), ElementsToMerge.Num() );

	// Merge the elements in to a single static mesh asset.
	UStaticMesh* StaticMesh = ConstructObject<UStaticMesh>( UStaticMesh::StaticClass(), Outer, UObject::MakeUniqueObjectName( Outer, UStaticMesh::StaticClass(), TEXT("MergedStaticMesh") ) );
	check( StaticMesh );
	MergedStaticMesh_BuildFromElements( StaticMesh, ElementsToMerge, MergedBounds.GetCenter() );
	StaticMesh->LightMapResolution = Max<UINT>( LightMapAtlasLayout.GetSizeX(), LightMapAtlasLayout.GetSizeY() );

	*OutStaticMesh = StaticMesh;
	*OutMergeOrigin = MergedBounds.GetCenter();

	return TRUE;
}
#endif // #if WITH_EDITOR
