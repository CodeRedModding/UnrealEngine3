/*=============================================================================
	UnStaticMeshBuild.cpp: Static mesh building.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

//
//	PointsEqual
//

inline UBOOL PointsEqual(FVector& V1,FVector& V2)
{
	if(Abs(V1.X - V2.X) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	if(Abs(V1.Y - V2.Y) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	if(Abs(V1.Z - V2.Z) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	return 1;
}

//
//	UVsEqual
//

inline UBOOL UVsEqual(FVector2D& UV1,FVector2D& UV2)
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

//
//	FindVertexIndex
//

INT FindVertexIndex(UStaticMesh* StaticMesh,FVector Position,FPackedNormal TangentX,FPackedNormal TangentY,FPackedNormal TangentZ,FColor Color,FVector2D* UVs,INT NumUVs)
{
	// Find any identical vertices already in the vertex buffer.

	INT	VertexBufferIndex = INDEX_NONE;

	for(INT VertexIndex = 0;VertexIndex < StaticMesh->Vertices.Num();VertexIndex++)
	{
		// Compare vertex position and normal.

		FStaticMeshVertex*	CompareVertex = &StaticMesh->Vertices(VertexIndex);

		if(!PointsEqual(CompareVertex->Position,Position))
			continue;

		if(!(CompareVertex->TangentX == TangentX))
			continue;

		if(!(CompareVertex->TangentY == TangentY))
			continue;
		
		if(!(CompareVertex->TangentZ == TangentZ))
			continue;

		// Compare vertex color.

#if VIEWPORT_ACTOR_DISABLED
		if(StaticMesh->ColorBuffer.Colors(VertexIndex) != Color)
			continue;
#endif

		// Compare vertex UVs.

		UBOOL	UVsMatch = 1;

		for(INT UVIndex = 0;UVIndex < NumUVs;UVIndex++)
		{
			if(!UVsEqual(StaticMesh->UVBuffers(UVIndex).UVs(VertexIndex),UVs[UVIndex]))
			{
				UVsMatch = 0;
				break;
			}
		}

		if(!UVsMatch)
			continue;

		// The vertex matches!

		VertexBufferIndex = VertexIndex;
		break;
	}

	// If there is no identical vertex already in the vertex buffer...

	if(VertexBufferIndex == INDEX_NONE)
	{
		// Add a new vertex to the vertex buffers.

		FStaticMeshVertex	Vertex;

		Vertex.Position = Position;
		Vertex.TangentX = TangentX;
		Vertex.TangentY = TangentY;
		Vertex.TangentZ = TangentZ;

		VertexBufferIndex = StaticMesh->Vertices.AddItem(Vertex);

#if VIEWPORT_ACTOR_DISABLED
		verify(StaticMesh->ColorBuffer.Colors.AddItem(Color) == VertexBufferIndex);
		verify(StaticMesh->AlphaBuffer.Colors.AddItem(FColor(255,255,255,Color.A)) == VertexBufferIndex);
#endif

		for(INT UVIndex = 0;UVIndex < NumUVs;UVIndex++)
		{
			verify(StaticMesh->UVBuffers(UVIndex).UVs.AddItem(UVs[UVIndex]) == VertexBufferIndex);
			StaticMesh->UVBuffers(UVIndex).Size += StaticMesh->UVBuffers(UVIndex).Stride;
		}

		for(INT UVIndex = NumUVs;UVIndex < StaticMesh->UVBuffers.Num();UVIndex++)
		{
			verify(StaticMesh->UVBuffers(UVIndex).UVs.AddZeroed() == VertexBufferIndex);
			StaticMesh->UVBuffers(UVIndex).Size += StaticMesh->UVBuffers(UVIndex).Stride;
		}
	}

	return VertexBufferIndex;

}

//
//	FindEdgeIndex
//

INT FindEdgeIndex(UStaticMesh* StaticMesh,FMeshEdge& Edge)
{
	for(INT EdgeIndex = 0;EdgeIndex < StaticMesh->Edges.Num();EdgeIndex++)
	{
		FMeshEdge&	OtherEdge = StaticMesh->Edges(EdgeIndex);

		if(StaticMesh->Vertices(OtherEdge.Vertices[0]).Position != StaticMesh->Vertices(Edge.Vertices[1]).Position)
			continue;

		if(StaticMesh->Vertices(OtherEdge.Vertices[1]).Position != StaticMesh->Vertices(Edge.Vertices[0]).Position)
			continue;

		if(OtherEdge.Faces[1] != INDEX_NONE)
			continue;

		OtherEdge.Faces[1] = Edge.Faces[0];
		return EdgeIndex;
	}

	new(StaticMesh->Edges) FMeshEdge(Edge);

	return StaticMesh->Edges.Num() - 1;

}

//
//	ClassifyTriangleVertices
//

ESplitType ClassifyTriangleVertices(FPlane Plane,FVector* Vertices)
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

//
//	UStaticMesh::Build
//

void UStaticMesh::Build()
{
	FStaticMeshComponentRecreateContext	ComponentRecreateContext(this);

	GWarn->BeginSlowTask(*FString::Printf(TEXT("(%s) Building"),*GetPathName()),1);

	// Mark the parent package as dirty.

	UObject* Outer = GetOuter();
	while( Outer && Outer->GetOuter() )
		Outer = Outer->GetOuter();
	if( Outer && Cast<UPackage>(Outer) )
		Cast<UPackage>(Outer)->bDirty = 1;

	// Clear old data.

	Vertices.Empty();
#if VIEWPORT_ACTOR_DISABLED
	ColorBuffer.Colors.Empty();
	AlphaBuffer.Colors.Empty();
#endif
	UVBuffers.Empty();

	IndexBuffer.Indices.Empty();
	IndexBuffer.Size = 0;

	WireframeIndexBuffer.Indices.Empty();
	WireframeIndexBuffer.Size = 0;

	Edges.Empty();
	ShadowTriangleDoubleSided.Empty();

	// Load the source data.

	if(!RawTriangles.Num())
		RawTriangles.Load();

	// Calculate triangle normals.

	TArray<FVector>	TriangleTangentX(RawTriangles.Num());
	TArray<FVector>	TriangleTangentY(RawTriangles.Num());
	TArray<FVector>	TriangleTangentZ(RawTriangles.Num());

	for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.Num();TriangleIndex++)
	{
		FStaticMeshTriangle*	Triangle = &RawTriangles(TriangleIndex);
		INT						UVIndex = 0;
		FVector					TriangleNormal = FPlane(
											Triangle->Vertices[2],
											Triangle->Vertices[1],
											Triangle->Vertices[0]
											);

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

		FMatrix	TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;
		FVector	TangentX = TextureToLocal.TransformNormal(FVector(1,0,0)).SafeNormal(),
				TangentY = TextureToLocal.TransformNormal(FVector(0,1,0)).SafeNormal(),
				TangentZ;

		TangentX = TangentX - TriangleNormal * (TangentX | TriangleNormal);
		TangentY = TangentY - TriangleNormal * (TangentY | TriangleNormal);
		TangentZ = TriangleNormal;

		TriangleTangentX(TriangleIndex) = TangentX.SafeNormal();
		TriangleTangentY(TriangleIndex) = TangentY.SafeNormal();
		TriangleTangentZ(TriangleIndex) = TangentZ.SafeNormal();
	}

	// Initialize material index buffers.

	TArray<FRawIndexBuffer>	MaterialIndices;

	for(INT MaterialIndex = 0;MaterialIndex < Materials.Num();MaterialIndex++)
		new(MaterialIndices) FRawIndexBuffer();

	// Create the necessary number of UV buffers.

	for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.Num();TriangleIndex++)
	{
		FStaticMeshTriangle*	Triangle = &RawTriangles(TriangleIndex);

		while(UVBuffers.Num() < Triangle->NumUVs)
			new(UVBuffers) FStaticMeshUVBuffer();
	}

	TArray<FkDOPBuildCollisionTriangle> kDOPBuildTriangles;

	// Process each triangle.
    INT	NumDegenerates = 0; 
	for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.Num();TriangleIndex++)
	{
		FStaticMeshTriangle*	Triangle = &RawTriangles(TriangleIndex);
		FStaticMeshMaterial*	Material = &Materials(Triangle->MaterialIndex);

        if( PointsEqual(Triangle->Vertices[0],Triangle->Vertices[1])
		    ||	PointsEqual(Triangle->Vertices[0],Triangle->Vertices[2])
		    ||	PointsEqual(Triangle->Vertices[1],Triangle->Vertices[2])
			|| TriangleTangentZ(TriangleIndex).IsZero()
		)
		{
            NumDegenerates++;
			continue;
		}

		GWarn->StatusUpdatef(TriangleIndex,RawTriangles.Num(),TEXT("(%s) Indexing vertices..."),*GetPathName());

		// Calculate smooth vertex normals.

		FVector	VertexTangentX[3],
				VertexTangentY[3],
				VertexTangentZ[3];

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			VertexTangentX[VertexIndex] = FVector(0,0,0);
			VertexTangentY[VertexIndex] = FVector(0,0,0);
			VertexTangentZ[VertexIndex] = FVector(0,0,0);
		}

		FLOAT	Determinant = FTriple(
					TriangleTangentX(TriangleIndex),
					TriangleTangentY(TriangleIndex),
					TriangleTangentZ(TriangleIndex)
					);
		
		// Determine contributing faces for correct smoothing group behaviour  according to the orthodox Max interpretation of smoothing groups.    O(n^2)      - EDN

		TArray<FanFace> RelevantFacesForVertex[3];

		for(INT OtherTriangleIndex = 0;OtherTriangleIndex < RawTriangles.Num();OtherTriangleIndex++)
		{
			for(INT OurVertexIndex = 0; OurVertexIndex < 3; OurVertexIndex++)
			{		
				FStaticMeshTriangle*	OtherTriangle = &RawTriangles(OtherTriangleIndex);
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
						if( PointsEqual(Triangle->Vertices[OurVertexIndex], OtherTriangle->Vertices[OtherVertexIndex]) )
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
						FStaticMeshTriangle* OtherTriangle = &RawTriangles( RelevantFacesForVertex[VertexIndex](OtherFaceIdx).FaceIndex );
						for( INT MoreFaceIdx = 0; MoreFaceIdx < RelevantFacesForVertex[VertexIndex].Num(); MoreFaceIdx ++ )
						{								
							if( ! RelevantFacesForVertex[VertexIndex]( MoreFaceIdx).Filled )
							{
								FStaticMeshTriangle* FreshTriangle = &RawTriangles( RelevantFacesForVertex[VertexIndex](MoreFaceIdx).FaceIndex );
								if( ( FreshTriangle->SmoothingMask &  OtherTriangle->SmoothingMask ) &&  ( MoreFaceIdx != OtherFaceIdx) )
								{				
									INT CommonVertices = 0;
									for(INT OtherVertexIndex = 0; OtherVertexIndex < 3; OtherVertexIndex++)
									{											
										for(INT OrigVertexIndex = 0; OrigVertexIndex < 3; OrigVertexIndex++)
										{
											if( PointsEqual ( FreshTriangle->Vertices[OrigVertexIndex],  OtherTriangle->Vertices[OtherVertexIndex]  )	)
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

					FStaticMeshTriangle*	OtherTriangle = &RawTriangles(OtherTriangleIndex);
					FLOAT OtherDeterminant = FTriple(
						TriangleTangentX(OtherTriangleIndex),
						TriangleTangentY(OtherTriangleIndex),
						TriangleTangentZ(OtherTriangleIndex)
						);

					if( Determinant * OtherDeterminant > 0.0f && UVsEqual(Triangle->UVs[VertexIndex][0],OtherTriangle->UVs[OtherVertexIndex][0]) )
					{
						VertexTangentX[VertexIndex] += TriangleTangentX(OtherTriangleIndex);
						VertexTangentY[VertexIndex] += TriangleTangentY(OtherTriangleIndex);
					}
					VertexTangentZ[VertexIndex] += TriangleTangentZ(OtherTriangleIndex);
				}
			}
		}


		// Normalization.

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			VertexTangentX[VertexIndex].Normalize();
			VertexTangentY[VertexIndex].Normalize();
			VertexTangentZ[VertexIndex].Normalize();

			VertexTangentY[VertexIndex] -= VertexTangentX[VertexIndex] * (VertexTangentX[VertexIndex] | VertexTangentY[VertexIndex]);
			VertexTangentY[VertexIndex].Normalize();

			VertexTangentX[VertexIndex] -= VertexTangentZ[VertexIndex] * (VertexTangentZ[VertexIndex] | VertexTangentX[VertexIndex]);
			VertexTangentX[VertexIndex].Normalize();
			VertexTangentY[VertexIndex] -= VertexTangentZ[VertexIndex] * (VertexTangentZ[VertexIndex] | VertexTangentY[VertexIndex]);
			VertexTangentY[VertexIndex].Normalize();
		}

		// Index the triangle's vertices.

		INT	VertexIndices[3];

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			VertexIndices[VertexIndex] = FindVertexIndex(
											this,
											Triangle->Vertices[VertexIndex],
											VertexTangentX[VertexIndex],
											VertexTangentY[VertexIndex],
											VertexTangentZ[VertexIndex],
											Triangle->Colors[VertexIndex],
											Triangle->UVs[VertexIndex],
											Triangle->NumUVs
											);

		// Reject degenerate triangles.

		if(VertexIndices[0] == VertexIndices[1] || VertexIndices[1] == VertexIndices[2] || VertexIndices[0] == VertexIndices[2])
			continue;

		// Put the indices in the material index buffer.

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			MaterialIndices(Triangle->MaterialIndex).Indices.AddItem(VertexIndices[VertexIndex]);

		if(Material->EnableCollision)
		{
			// Build a new kDOP collision triangle
			new (kDOPBuildTriangles) FkDOPBuildCollisionTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2],Triangle);
		}
	}

	if(NumDegenerates)
    	debugf(TEXT("%s StaticMesh had %i degenerates"), GetName(), NumDegenerates );

	PositionVertexBuffer.Update();
	TangentVertexBuffer.Update();

	for(INT UVIndex = 0;UVIndex < UVBuffers.Num();UVIndex++)
		GResourceManager->UpdateResource(&UVBuffers(UVIndex));

	// Build a cache optimized triangle list for each material and copy it into the shared index buffer.

	for(INT MaterialIndex = 0;MaterialIndex < MaterialIndices.Num();MaterialIndex++)
	{
		FStaticMeshMaterial&	Material = Materials(MaterialIndex);
		UINT					FirstIndex = IndexBuffer.Indices.Num();

		GWarn->StatusUpdatef(MaterialIndex,Materials.Num(),TEXT("(%s) Optimizing render data..."),*GetPathName());

		if(MaterialIndices(MaterialIndex).Indices.Num())
		{
			MaterialIndices(MaterialIndex).CacheOptimize();

			_WORD*	DestPtr = &IndexBuffer.Indices(IndexBuffer.Indices.Add(MaterialIndices(MaterialIndex).Indices.Num()));
			_WORD*	SrcPtr = &MaterialIndices(MaterialIndex).Indices(0);

			Material.FirstIndex = FirstIndex;
			Material.NumTriangles = MaterialIndices(MaterialIndex).Indices.Num() / 3;
			Material.MinVertexIndex = *SrcPtr;
			Material.MaxVertexIndex = *SrcPtr;

			for(INT Index = 0;Index < MaterialIndices(MaterialIndex).Indices.Num();Index++)
			{
				Material.MinVertexIndex = Min<UINT>(*SrcPtr,Material.MinVertexIndex);
				Material.MaxVertexIndex = Max<UINT>(*SrcPtr,Material.MaxVertexIndex);

				*DestPtr++ = *SrcPtr++;
				IndexBuffer.Size += IndexBuffer.Stride;
			}
		}
		else
		{
			Material.FirstIndex = 0;
			Material.NumTriangles = 0;
			Material.MinVertexIndex = 0;
			Material.MaxVertexIndex = 0;
		}
	}

	GResourceManager->UpdateResource(&IndexBuffer);

	// Build a list of wireframe edges in the static mesh.

	for(INT TriangleIndex = 0;TriangleIndex < IndexBuffer.Indices.Num() / 3;TriangleIndex++)
	{
		_WORD*	TriangleIndices = &IndexBuffer.Indices(TriangleIndex * 3);

		for(INT EdgeIndex = 0;EdgeIndex < 3;EdgeIndex++)
		{
			FMeshEdge	Edge;

			Edge.Vertices[0] = TriangleIndices[EdgeIndex];
			Edge.Vertices[1] = TriangleIndices[(EdgeIndex + 1) % 3];
			Edge.Faces[0] = TriangleIndex;
			Edge.Faces[1] = -1;

			FindEdgeIndex(this,Edge);
		}
	}

	for(INT EdgeIndex = 0;EdgeIndex < Edges.Num();EdgeIndex++)
	{
		FMeshEdge&	Edge = Edges(EdgeIndex);

		WireframeIndexBuffer.Indices.AddItem(Edge.Vertices[0]);
		WireframeIndexBuffer.Indices.AddItem(Edge.Vertices[1]);
		WireframeIndexBuffer.Size += WireframeIndexBuffer.Stride * 2;
	}

	GResourceManager->UpdateResource(&WireframeIndexBuffer);

	// Find triangles that aren't part of a closed mesh.

	TArray<INT>	SeparateTriangles;

	while(1)
	{
		INT	InitialSeparate = SeparateTriangles.Num();
		for(INT EdgeIndex = 0;EdgeIndex < Edges.Num();EdgeIndex++)
		{
			FMeshEdge&	Edge = Edges(EdgeIndex);
			if(Edge.Faces[1] == INDEX_NONE || SeparateTriangles.FindItemIndex(Edge.Faces[1]) != INDEX_NONE)
				SeparateTriangles.AddUniqueItem(Edge.Faces[0]);
			else if(SeparateTriangles.FindItemIndex(Edge.Faces[0]) != INDEX_NONE)
				SeparateTriangles.AddUniqueItem(Edge.Faces[1]);
		}
		if(SeparateTriangles.Num() == InitialSeparate)
			break;
	};

	ShadowTriangleDoubleSided.AddZeroed(IndexBuffer.Indices.Num() / 3);

	for(UINT TriangleIndex = 0;TriangleIndex < (UINT)SeparateTriangles.Num();TriangleIndex++)
		ShadowTriangleDoubleSided(SeparateTriangles(TriangleIndex)) = 1;

	// Calculate the bounding box.

	FBox	BoundingBox(0);

	for(INT VertexIndex = 0;VertexIndex < Vertices.Num();VertexIndex++)
		BoundingBox += Vertices(VertexIndex).Position;
	BoundingBox.GetCenterAndExtents(Bounds.Origin,Bounds.BoxExtent);

	// Calculate the bounding sphere, using the center of the bounding box as the origin.

	Bounds.SphereRadius = 0.0f;
	for(INT VertexIndex = 0;VertexIndex < Vertices.Num();VertexIndex++)
		Bounds.SphereRadius = Max((Vertices(VertexIndex).Position - Bounds.Origin).Size(),Bounds.SphereRadius);

	kDOPTree.Build(Vertices,kDOPBuildTriangles);

	if( !GIsEditor )
		RawTriangles.Unload();

	GWarn->EndSlowTask();

}

IMPLEMENT_COMPARE_CONSTREF( FLOAT, UnStaticMeshBuild, { return B - A; } )


/**
 * Returns the scale dependent texture factor used by the texture streaming code.	
 *
 * @param RequestedUVIndex UVIndex to look at
 * @return scale dependent texture factor
 */
FLOAT UStaticMesh::GetStreamingTextureFactor( INT RequestedUVIndex )
{
	FLOAT StreamingTextureFactor = 0.f;

	if( !RawTriangles.Num() )
		RawTriangles.Load();

	if( RawTriangles.Num() )
	{
		TArray<FLOAT> TexelRatios;
		TexelRatios.Empty( RawTriangles.Num() );
		
		// Figure out texel to unreal unit ratios.
		for(INT TriangleIndex=0; TriangleIndex<RawTriangles.Num(); TriangleIndex++ )
		{
			FStaticMeshTriangle& Triangle = RawTriangles(TriangleIndex);

			FLOAT	L1	= (Triangle.Vertices[0] - Triangle.Vertices[1]).Size(),
					L2	= (Triangle.Vertices[0] - Triangle.Vertices[2]).Size();

			INT	UVIndex = Min( Triangle.NumUVs - 1, RequestedUVIndex );

			FLOAT	T1	= (Triangle.UVs[0][UVIndex] - Triangle.UVs[1][UVIndex]).Size(),
					T2	= (Triangle.UVs[0][UVIndex] - Triangle.UVs[2][UVIndex]).Size();
		
			if( Abs(T1 * T2) > Square(SMALL_NUMBER) )
			{
				TexelRatios.AddItem( Max( L1 / T1, L1 / T2 ) );
			}
		}

		if( TexelRatios.Num() )
		{
			// Disregard upper 75% of texel ratios.
			Sort<USE_COMPARE_CONSTREF(FLOAT,UnStaticMeshBuild)>( &TexelRatios(0), TexelRatios.Num() );
			StreamingTextureFactor = TexelRatios( TexelRatios.Num() * 0.75 );
		}
	}

	// We don't unload the lazy array in the editor as we could potentially loose data in certain cases if it hasn't been saved yet (e.g. replacing an existing mesh).
	if( !GIsEditor )
		RawTriangles.Unload();

	return StreamingTextureFactor;
}
