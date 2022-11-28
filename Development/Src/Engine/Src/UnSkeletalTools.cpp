/*=============================================================================
	UnSkeletalTools.cpp: Skeletal mesh helper classes.

	Copyright 2001,2004   Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"

//
//	PointsEqual
//

static inline UBOOL PointsEqual(FVector& V1,FVector& V2)
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

static inline UBOOL UVsEqual(FMeshWedge& V1,FMeshWedge& V2)
{
	if(Abs(V1.U - V2.U) > (1.0f / 1024.0f))
		return 0;

	if(Abs(V1.V - V2.V) > (1.0f / 1024.0f))
		return 0;

	return 1;
}

//
//	AddSkinVertex
//

static INT AddSkinVertex(TArray<FSoftSkinVertex>& Vertices,FSoftSkinVertex& Vertex)
{
	for(UINT VertexIndex = 0;VertexIndex < (UINT)Vertices.Num();VertexIndex++)
	{
		FSoftSkinVertex&	OtherVertex = Vertices(VertexIndex);

		if(!PointsEqual(OtherVertex.Position,Vertex.Position))
			continue;

		if(Abs(Vertex.U - OtherVertex.U) > (1.0f / 1024.0f))
			continue;

		if(Abs(Vertex.V - OtherVertex.V) > (1.0f / 1024.0f))
			continue;

		if(!(OtherVertex.TangentX == Vertex.TangentX))
			continue;

		if(!(OtherVertex.TangentY == Vertex.TangentY))
			continue;

		if(!(OtherVertex.TangentZ == Vertex.TangentZ))
			continue;

		UBOOL	InfluencesMatch = 1;
		for(UINT InfluenceIndex = 0;InfluenceIndex < 4;InfluenceIndex++)
		{
			if(Vertex.InfluenceBones[InfluenceIndex] != OtherVertex.InfluenceBones[InfluenceIndex] || Vertex.InfluenceWeights[InfluenceIndex] != OtherVertex.InfluenceWeights[InfluenceIndex])
			{
				InfluencesMatch = 0;
				break;
			}
		}
		if(!InfluencesMatch)
			continue;

		return VertexIndex;
	}

	return Vertices.AddItem(Vertex);
}

//
//	FindEdgeIndex
//

static INT FindEdgeIndex(TArray<FMeshEdge>& Edges,const TArray<FSoftSkinVertex>& Vertices,FMeshEdge& Edge)
{
	for(INT EdgeIndex = 0;EdgeIndex < Edges.Num();EdgeIndex++)
	{
		FMeshEdge&	OtherEdge = Edges(EdgeIndex);

		if(Vertices(OtherEdge.Vertices[0]).Position != Vertices(Edge.Vertices[1]).Position)
			continue;

		if(Vertices(OtherEdge.Vertices[1]).Position != Vertices(Edge.Vertices[0]).Position)
			continue;

		UBOOL	InfluencesMatch = 1;
		for(UINT InfluenceIndex = 0;InfluenceIndex < 4;InfluenceIndex++)
		{
			if(Vertices(OtherEdge.Vertices[0]).InfluenceBones[InfluenceIndex] != Vertices(Edge.Vertices[1]).InfluenceBones[InfluenceIndex] ||
				Vertices(OtherEdge.Vertices[0]).InfluenceWeights[InfluenceIndex] != Vertices(Edge.Vertices[1]).InfluenceWeights[InfluenceIndex] ||
				Vertices(OtherEdge.Vertices[1]).InfluenceBones[InfluenceIndex] != Vertices(Edge.Vertices[0]).InfluenceBones[InfluenceIndex] ||
				Vertices(OtherEdge.Vertices[1]).InfluenceWeights[InfluenceIndex] != Vertices(Edge.Vertices[0]).InfluenceWeights[InfluenceIndex])
			{
				InfluencesMatch = 0;
				break;
			}
		}

		if(!InfluencesMatch)
			break;

		if(OtherEdge.Faces[1] != INDEX_NONE)
			continue;

		OtherEdge.Faces[1] = Edge.Faces[0];
		return EdgeIndex;
	}

	new(Edges) FMeshEdge(Edge);

	return Edges.Num() - 1;
}

/**
 * Create all render specific (but serializable) data like e.g. the 'compiled' rendering stream,
 * mesh sections and index buffer.
 *
 * @todo: currently only handles LOD level 0.
 */
void USkeletalMesh::CreateSkinningStreams()
{
#ifndef CONSOLE
	check( LODModels.Num() );
	FStaticLODModel& LODModel = LODModels(0);
	
	// Allow multiple calls to CreateSkinningStreams for same model/LOD.

	LODModel.Sections.Empty();
	LODModel.SoftVertices.Empty();
	LODModel.RigidVertices.Empty();
	LODModel.ShadowIndices.Empty();
	LODModel.IndexBuffer.Indices.Empty();
	LODModel.IndexBuffer.Size = 0;
	LODModel.Edges.Empty();

	// Calculate face tangent vectors.

	TArray<FVector>	FaceTangentX(LODModel.Faces.Num());
	TArray<FVector>	FaceTangentY(LODModel.Faces.Num());

	for(INT FaceIndex = 0;FaceIndex < LODModel.Faces.Num();FaceIndex++)
	{
		FVector	P1 = LODModel.Points(LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[0]).iVertex),
				P2 = LODModel.Points(LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[1]).iVertex),
				P3 = LODModel.Points(LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[2]).iVertex);
		FVector	TriangleNormal = FPlane(P3,P2,P1);
		FMatrix	ParameterToLocal(
			FPlane(	P2.X - P1.X,	P2.Y - P1.Y,	P2.Z - P1.Z,	0	),
			FPlane(	P3.X - P1.X,	P3.Y - P1.Y,	P3.Z - P1.Z,	0	),
			FPlane(	P1.X,			P1.Y,			P1.Z,			0	),
			FPlane(	0,				0,				0,				1	)
			);

		FLOAT	U1 = LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[0]).U,
				U2 = LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[1]).U,
				U3 = LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[2]).U,
				V1 = LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[0]).V,
				V2 = LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[1]).V,
				V3 = LODModel.Wedges(LODModel.Faces(FaceIndex).iWedge[2]).V;

		FMatrix	ParameterToTexture(
			FPlane(	U2 - U1,	V2 - V1,	0,	0	),
			FPlane(	U3 - U1,	V3 - V1,	0,	0	),
			FPlane(	U1,			V1,			1,	0	),
			FPlane(	0,			0,			0,	1	)
			);

		FMatrix	TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;
		FVector	TangentX = TextureToLocal.TransformNormal(FVector(1,0,0)).SafeNormal(),
				TangentY = TextureToLocal.TransformNormal(FVector(0,1,0)).SafeNormal(),
				TangentZ;

		TangentX = TangentX - TriangleNormal * (TangentX | TriangleNormal);
		TangentY = TangentY - TriangleNormal * (TangentY | TriangleNormal);

		FaceTangentX(FaceIndex) = TangentX.SafeNormal();
		FaceTangentY(FaceIndex) = TangentY.SafeNormal();
	}

	// Find wedge influences.

	TArray<INT>	WedgeInfluenceIndices;
	INT InfIdx = 0;

	for(INT WedgeIndex = 0;WedgeIndex < LODModel.Wedges.Num();WedgeIndex++)
	{
		for(UINT LookIdx = 0;LookIdx < (UINT)LODModel.Influences.Num();LookIdx++)
		{
			if(LODModel.Influences(LookIdx).VertIndex == LODModel.Wedges(WedgeIndex).iVertex)
			{
				WedgeInfluenceIndices.AddItem(LookIdx);
				break;
			}
		}
	}
	check(LODModel.Wedges.Num() == WedgeInfluenceIndices.Num());

	// Calculate smooth wedge tangent vectors.

	TArray<FRawIndexBuffer>	SectionIndexBuffers;
	FSkelMeshSection*		Section = NULL;
	FRawIndexBuffer*		SectionIndexBuffer = NULL;
	TArray<FSoftSkinVertex>	Vertices;

	for(INT FaceIndex = 0;FaceIndex < LODModel.Faces.Num();FaceIndex++)
	{
		FMeshFace&	Face = LODModel.Faces(FaceIndex);

		FVector	VertexTangentX[3],
				VertexTangentY[3],
				VertexTangentZ[3];

        for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			VertexTangentX[VertexIndex] = FVector(0,0,0);
			VertexTangentY[VertexIndex] = FVector(0,0,0);
			VertexTangentZ[VertexIndex] = FVector(0,0,0);
		}

		FVector	TriangleNormal = FPlane(
			LODModel.Points(LODModel.Wedges(Face.iWedge[2]).iVertex),
			LODModel.Points(LODModel.Wedges(Face.iWedge[1]).iVertex),
			LODModel.Points(LODModel.Wedges(Face.iWedge[0]).iVertex)
			);
		FLOAT	Determinant = FTriple(FaceTangentX(FaceIndex),FaceTangentY(FaceIndex),TriangleNormal);
		for(INT OtherFaceIndex = 0;OtherFaceIndex < LODModel.Faces.Num();OtherFaceIndex++)
		{
			FMeshFace&	OtherFace = LODModel.Faces(OtherFaceIndex);
			FVector		OtherTriangleNormal = FPlane(
							LODModel.Points(LODModel.Wedges(OtherFace.iWedge[2]).iVertex),
							LODModel.Points(LODModel.Wedges(OtherFace.iWedge[1]).iVertex),
							LODModel.Points(LODModel.Wedges(OtherFace.iWedge[0]).iVertex)
							);
			FLOAT		OtherFaceDeterminant = FTriple(FaceTangentX(OtherFaceIndex),FaceTangentY(OtherFaceIndex),OtherTriangleNormal);

			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				for(INT OtherVertexIndex = 0;OtherVertexIndex < 3;OtherVertexIndex++)
				{
					if(PointsEqual(
						LODModel.Points(LODModel.Wedges(OtherFace.iWedge[OtherVertexIndex]).iVertex),
						LODModel.Points(LODModel.Wedges(Face.iWedge[VertexIndex]).iVertex)
						))					
					{
						if(Determinant * OtherFaceDeterminant > 0.0f && UVsEqual(LODModel.Wedges(OtherFace.iWedge[OtherVertexIndex]),LODModel.Wedges(Face.iWedge[VertexIndex])))
						{
							VertexTangentX[VertexIndex] += FaceTangentX(OtherFaceIndex);
							VertexTangentY[VertexIndex] += FaceTangentY(OtherFaceIndex);
						}

						// Only contribute 'normal' if the vertices are truly one and the same to obey hard "smoothing" edges baked into 
						// the mesh by vertex duplication.. - Erik
						if( LODModel.Wedges(OtherFace.iWedge[OtherVertexIndex]).iVertex == LODModel.Wedges(Face.iWedge[VertexIndex]).iVertex ) 
						{
							VertexTangentZ[VertexIndex] += OtherTriangleNormal;
						}
					}
				}
			}
		}

		// Add this triangle to a section.

		if(!Section || (Face.MeshMaterialIndex != Section->MaterialIndex))
		{
			// Create a new static mesh section.

			Section = new(LODModel.Sections)FSkelMeshSection;
			Section->FirstFace = FaceIndex;
			Section->MaterialIndex = Face.MeshMaterialIndex;	
			Section->TotalFaces = 0;

			SectionIndexBuffer = new(SectionIndexBuffers) FRawIndexBuffer;
		}

		Section->TotalFaces++;

		_WORD	TriangleIndices[3];

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			FSoftSkinVertex	Vertex;
			FVector			TangentX = VertexTangentX[VertexIndex].SafeNormal(),
							TangentY = VertexTangentY[VertexIndex].SafeNormal(),
							TangentZ = VertexTangentZ[VertexIndex].SafeNormal();

			TangentY -= TangentX * (TangentX | TangentY);
			TangentY.Normalize();

			TangentX -= TangentZ * (TangentZ | TangentX);
			TangentY -= TangentZ * (TangentZ | TangentY);

			TangentX.Normalize();
			TangentY.Normalize();

			Vertex.Position = LODModel.Points(LODModel.Wedges(Face.iWedge[VertexIndex]).iVertex);
			Vertex.TangentX = TangentX;
			Vertex.TangentY = TangentY;
			Vertex.TangentZ = TangentZ;
			Vertex.U = LODModel.Wedges(Face.iWedge[VertexIndex]).U;
			Vertex.V = LODModel.Wedges(Face.iWedge[VertexIndex]).V;

			// Count the influences.

			InfIdx = WedgeInfluenceIndices(Face.iWedge[VertexIndex]);
			INT LookIdx = InfIdx;

			UINT InfluenceCount = 0;
			while ( (LODModel.Influences(LookIdx).VertIndex == LODModel.Wedges(Face.iWedge[VertexIndex]).iVertex) && ( LODModel.Influences.Num() > LookIdx) )
			{			
				InfluenceCount++;
				LookIdx++;
			}

			if( InfluenceCount > 4 )
				InfluenceCount = 4;

			// Setup the vertex influences.

			Vertex.InfluenceBones[0] = 0;
			Vertex.InfluenceWeights[0] = 255;

			for(UINT i = 1;i < 4;i++)
			{
				Vertex.InfluenceBones[i] = 0;
				Vertex.InfluenceWeights[i] = 0;
			}

			UINT	TotalInfluenceWeight = 0;

			for(UINT i = 0;i < InfluenceCount;i++)
			{
				BYTE	BoneIndex = 0;

				BoneIndex = (BYTE)LODModel.Influences(InfIdx+i).BoneIndex;

				if( BoneIndex >= RefSkeleton.Num() )
					continue;

				Vertex.InfluenceBones[i] = LODModel.ActiveBoneIndices.AddUniqueItem(BoneIndex);
				Vertex.InfluenceWeights[i] = (BYTE)(LODModel.Influences(InfIdx+i).Weight * 255.0f);
				TotalInfluenceWeight += Vertex.InfluenceWeights[i];
			}

			Vertex.InfluenceWeights[0] += 255 - TotalInfluenceWeight;

			InfIdx = LookIdx;

			// Add the vertex.

 			INT	V = AddSkinVertex(Vertices,Vertex);
			check(V >= 0 && V <= MAXWORD);
			TriangleIndices[VertexIndex] = (_WORD)V;
		}

		if(TriangleIndices[0] != TriangleIndices[1] && TriangleIndices[0] != TriangleIndices[2] && TriangleIndices[1] != TriangleIndices[2])
		{
			for(UINT EdgeIndex = 0;EdgeIndex < 3;EdgeIndex++)
			{
				FMeshEdge	Edge;

				Edge.Vertices[0] = TriangleIndices[EdgeIndex];
				Edge.Vertices[1] = TriangleIndices[(EdgeIndex + 1) % 3];
				Edge.Faces[0] = LODModel.ShadowIndices.Num() / 3;
				Edge.Faces[1] = -1;

				FindEdgeIndex(LODModel.Edges,Vertices,Edge);
			}

			for(UINT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				SectionIndexBuffer->Indices.AddItem(TriangleIndices[VertexIndex]);
				LODModel.ShadowIndices.AddItem(TriangleIndices[VertexIndex]);
			}
			SectionIndexBuffer->Size += sizeof(_WORD) * 3;
		}
	}

	// Find mesh sections with open edges.

	TArray<INT>	SeparateTriangles;

	while(1)
	{
		INT	InitialSeparate = SeparateTriangles.Num();
		for(INT EdgeIndex = 0;EdgeIndex < LODModel.Edges.Num();EdgeIndex++)
		{
			FMeshEdge&	Edge = LODModel.Edges(EdgeIndex);
			if(Edge.Faces[1] == INDEX_NONE || SeparateTriangles.FindItemIndex(Edge.Faces[1]) != INDEX_NONE)
				SeparateTriangles.AddUniqueItem(Edge.Faces[0]);
			else if(SeparateTriangles.FindItemIndex(Edge.Faces[0]) != INDEX_NONE)
				SeparateTriangles.AddUniqueItem(Edge.Faces[1]);
		}
		if(SeparateTriangles.Num() == InitialSeparate)
			break;
	};

	LODModel.ShadowTriangleDoubleSided.Empty(LODModel.ShadowIndices.Num() / 3);
	LODModel.ShadowTriangleDoubleSided.AddZeroed(LODModel.ShadowIndices.Num() / 3);

	for(UINT TriangleIndex = 0;TriangleIndex < (UINT)SeparateTriangles.Num();TriangleIndex++)
		LODModel.ShadowTriangleDoubleSided(SeparateTriangles(TriangleIndex)) = 1;

	// Separate the vertices into soft and rigid vertices.

	TArray<_WORD>	VertexMap(Vertices.Num());

	for(UINT VertexIndex = 0;VertexIndex < (UINT)Vertices.Num();VertexIndex++)
	{
		const FSoftSkinVertex&	SoftVertex = Vertices(VertexIndex);

		if(SoftVertex.InfluenceWeights[1] == 0)
		{
			FRigidSkinVertex	RigidVertex;
			RigidVertex.Position = SoftVertex.Position;
			RigidVertex.TangentX = SoftVertex.TangentX;
			RigidVertex.TangentY = SoftVertex.TangentY;
			RigidVertex.TangentZ = SoftVertex.TangentZ;
			RigidVertex.U = SoftVertex.U;
			RigidVertex.V = SoftVertex.V;
			RigidVertex.Bone = SoftVertex.InfluenceBones[0];
			VertexMap(VertexIndex) = (_WORD)LODModel.RigidVertices.AddItem(RigidVertex);
		}
	}

	for(UINT VertexIndex = 0;VertexIndex < (UINT)Vertices.Num();VertexIndex++)
	{
		const FSoftSkinVertex&	SoftVertex = Vertices(VertexIndex);

		if(SoftVertex.InfluenceWeights[1] > 0)
			VertexMap(VertexIndex) = (_WORD)LODModel.RigidVertices.Num() + (_WORD)LODModel.SoftVertices.AddItem(SoftVertex);
	}

	// Remap the edge and shadow vertex indices.

	for(UINT EdgeIndex = 0;EdgeIndex < (UINT)LODModel.Edges.Num();EdgeIndex++)
	{
		LODModel.Edges(EdgeIndex).Vertices[0] = VertexMap(LODModel.Edges(EdgeIndex).Vertices[0]);
		LODModel.Edges(EdgeIndex).Vertices[1] = VertexMap(LODModel.Edges(EdgeIndex).Vertices[1]);
	}

	for(UINT Index = 0;Index < (UINT)LODModel.ShadowIndices.Num();Index++)
		LODModel.ShadowIndices(Index) = VertexMap(LODModel.ShadowIndices(Index));

	UINT	NumOpenEdges = 0;
	for(UINT EdgeIndex = 0;EdgeIndex < (UINT)LODModel.Edges.Num();EdgeIndex++)
		if(LODModel.Edges(EdgeIndex).Faces[1] == INDEX_NONE)
			NumOpenEdges++;

	debugf(TEXT("%u rigid, %u soft, %u double-sided, %u edges(%u open)"),LODModel.RigidVertices.Num(),LODModel.SoftVertices.Num(),SeparateTriangles.Num(),LODModel.Edges.Num(),NumOpenEdges);

	// Finish building the sections.

	for(UINT SectionIndex = 0,GlobalTriangleIndex = 0;SectionIndex < (UINT)LODModel.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection&	Section = LODModel.Sections(SectionIndex);
		FRawIndexBuffer&	SectionIndexBuffer = SectionIndexBuffers(SectionIndex);

		SectionIndexBuffer.CacheOptimize();

		Section.MinIndex = MAXWORD;
		Section.MaxIndex = 0;

		for(UINT Index = 0;Index < (UINT)SectionIndexBuffer.Indices.Num();Index++)
		{
			_WORD&	VertexIndex = SectionIndexBuffer.Indices(Index);
			VertexIndex = VertexMap(VertexIndex);
			Section.MinIndex = Min(Section.MinIndex,VertexIndex);
			Section.MaxIndex = Max(Section.MaxIndex,VertexIndex);
		}

		Section.FirstIndex = LODModel.IndexBuffer.Indices.Num();
		Section.TotalFaces = SectionIndexBuffer.Indices.Num() / 3;
		appMemcpy(&LODModel.IndexBuffer.Indices(LODModel.IndexBuffer.Indices.Add(SectionIndexBuffer.Indices.Num())),&SectionIndexBuffer.Indices(0),SectionIndexBuffer.Indices.Num() * sizeof(_WORD));

		Section.TotalVerts = (Section.MaxIndex - Section.MinIndex )+1;

		debugf(TEXT(" Smooth section constructed: [%3i], MinIndex  %3i, MaxIndex %3i, Material %3i Faces %3i FirstFace %3i Wedges: %i ActiveBones: %i"),
			SectionIndex,
			Section.MinIndex,
			Section.MaxIndex,
			Section.MaterialIndex,
			Section.TotalFaces,
			Section.FirstFace,
			LODModel.Faces.Num() * 3,
			Section.ActiveBoneIndices.Num()
		);
	}

	LODModel.IndexBuffer.Size = LODModel.IndexBuffer.Indices.Num() * sizeof(_WORD);
	GResourceManager->UpdateResource(&LODModel.IndexBuffer);
#else
	appErrorf(TEXT("Cannot call USkeletalMesh::CreateSkinningStreams on a console!"));
#endif
}

// Pre-calculate refpose-to-local transforms
void USkeletalMesh::CalculateInvRefMatrices()
{
	if( RefBasesInvMatrix.Num() != RefSkeleton.Num() )
	{
		RefBasesInvMatrix.Empty();
		RefBasesInvMatrix.Add( RefSkeleton.Num() );

		// Temporary storage for calculating mesh-space ref pose
		TArray<FMatrix> RefBases;
		RefBases.Add( RefSkeleton.Num() );

		// Precompute the Mesh.RefBasesInverse.
		for( INT b=0; b<RefSkeleton.Num(); b++)
		{
			// Render the default pose.
			RefBases(b) = GetRefPoseMatrix(b);

			// Construct mesh-space skeletal hierarchy.
			if( b>0 )
			{
				INT Parent = RefSkeleton(b).ParentIndex;
				RefBases(b) = RefBases(b) * RefBases(Parent);
			}

			// Precompute inverse so we can use from-refpose-skin vertices.
			RefBasesInvMatrix(b) = RefBases(b).Inverse(); 
		}
	}
}

// Find the most dominant bone for each vertex
static INT GetDominantBoneIndex(FSoftSkinVertex* SoftVert)
{
	BYTE MaxWeightBone = 0;
	BYTE MaxWeightWeight = 0;

	for(INT i=0; i<4; i++)
	{
		if(SoftVert->InfluenceWeights[i] > MaxWeightWeight)
		{
			MaxWeightWeight = SoftVert->InfluenceWeights[i];
			MaxWeightBone = SoftVert->InfluenceBones[i];
		}
	}

	return MaxWeightBone;
}

/**
 *	Calculate the verts associated weighted to each bone of the skeleton.
 *	The vertices returned are in the local space of the bone.
 *
 *	@param	Infos	The output array of vertices associated with each bone.
 *	@param	bOnlyDominant	Controls whether a vertex is added to the info for a bone if it is most controlled by that bone, or if that bone has ANY influence on that vert.
 */
void USkeletalMesh::CalcBoneVertInfos( TArray<FBoneVertInfo>& Infos, UBOOL bOnlyDominant )
{
	if( LODModels.Num() == 0)
		return;

	CalculateInvRefMatrices();
	check( RefSkeleton.Num() == RefBasesInvMatrix.Num() );

	Infos.Empty();
	Infos.AddZeroed( RefSkeleton.Num() );

	FStaticLODModel* LODModel = &LODModels(0);
	for(INT i=0; i<LODModel->RigidVertices.Num(); i++)
	{
		FRigidSkinVertex* RigidVert = &LODModel->RigidVertices(i);
		INT BoneIndex = LODModel->ActiveBoneIndices( RigidVert->Bone );

		FVector LocalPos = RefBasesInvMatrix(BoneIndex).TransformFVector(RigidVert->Position);
		Infos(BoneIndex).Positions.AddItem(LocalPos);

		FVector LocalNormal = RefBasesInvMatrix(BoneIndex).TransformNormal(RigidVert->TangentZ);
		Infos(BoneIndex).Normals.AddItem(LocalNormal);
	}

	for(INT i=0; i<LODModel->SoftVertices.Num(); i++)
	{
		FSoftSkinVertex* SoftVert = &LODModel->SoftVertices(i);

		if(bOnlyDominant)
		{
			INT BoneIndex = LODModel->ActiveBoneIndices( GetDominantBoneIndex(SoftVert) );

			FVector LocalPos = RefBasesInvMatrix(BoneIndex).TransformFVector(SoftVert->Position);
			Infos(BoneIndex).Positions.AddItem(LocalPos);

			FVector LocalNormal = RefBasesInvMatrix(BoneIndex).TransformNormal(SoftVert->TangentZ);
			Infos(BoneIndex).Normals.AddItem(LocalNormal);
		}
		else
		{
			for(INT j=0; j<4; j++)
			{
				if(SoftVert->InfluenceWeights[j] > 0.01f)
				{
					INT BoneIndex = LODModel->ActiveBoneIndices( SoftVert->InfluenceBones[j] );

					FVector LocalPos = RefBasesInvMatrix(BoneIndex).TransformFVector(SoftVert->Position);
					Infos(BoneIndex).Positions.AddItem(LocalPos);

					FVector LocalNormal = RefBasesInvMatrix(BoneIndex).TransformNormal(SoftVert->TangentZ);
					Infos(BoneIndex).Normals.AddItem(LocalNormal);
				}
			}
		}
	}
}

/**
 * Find if one bone index is a child of another.
 * Note - will return FALSE if ChildBoneIndex is the same as ParentBoneIndex ie. must be strictly a child.
 */
UBOOL USkeletalMesh::BoneIsChildOf(INT ChildBoneIndex, INT ParentBoneIndex )
{
	if(ChildBoneIndex == ParentBoneIndex)
		return false;

	INT BoneIndex = RefSkeleton(ChildBoneIndex).ParentIndex;
	while(1)
	{
		if(BoneIndex == ParentBoneIndex)
			return true;

		if(BoneIndex == 0)
			return false;

		BoneIndex = RefSkeleton(BoneIndex).ParentIndex;
	}
}

////// SKELETAL MESH THUMBNAIL SUPPORT ////////

struct FSkeletalMeshThumbnailScene: FPreviewScene
{
	FSceneView	View;

	// Constructor/destructor.

	FSkeletalMeshThumbnailScene(USkeletalMesh* InSkeletalMesh)
	{
		USkeletalMeshComponent*		SkeletalMeshComponent = ConstructObject<USkeletalMeshComponent>(USkeletalMeshComponent::StaticClass());
		SkeletalMeshComponent->SkeletalMesh = InSkeletalMesh;
		SkeletalMeshComponent->Scene = this;
		SkeletalMeshComponent->SetParentToWorld(FMatrix::Identity);
		SkeletalMeshComponent->Created();
		Components.AddItem(SkeletalMeshComponent);

		// We look at the bounding box to see which side to render it from.
		if(SkeletalMeshComponent->Bounds.BoxExtent.X > SkeletalMeshComponent->Bounds.BoxExtent.Y)
		{
			View.ViewMatrix = FTranslationMatrix(-(SkeletalMeshComponent->Bounds.Origin - FVector(0,SkeletalMeshComponent->Bounds.SphereRadius / (75.0f * (FLOAT)PI / 360.0f),0)));
			View.ViewMatrix = View.ViewMatrix * FInverseRotationMatrix(FRotator(0,16384,0));
		}
		else
		{
			View.ViewMatrix = FTranslationMatrix(-(SkeletalMeshComponent->Bounds.Origin - FVector(-SkeletalMeshComponent->Bounds.SphereRadius / (75.0f * (FLOAT)PI / 360.0f),0,0)));
			View.ViewMatrix = View.ViewMatrix * FInverseRotationMatrix(FRotator(0,32768,0));
		}

		View.ViewMatrix = View.ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));
		View.ProjectionMatrix = FPerspectiveMatrix(
			75.0f * (FLOAT)PI / 360.0f,
			1.0f,
			1.0f,
			NEAR_CLIPPING_PLANE
			);
		View.ShowFlags = SHOW_DefaultEditor;
		View.BackgroundColor = FColor(0,0,0);
	}
};

FString USkeletalMesh::GetDesc()
{
	check(LODModels.Num());
	LODModels(0).Faces.Load();
	return FString::Printf( TEXT("%d Polys, %d Bones"), LODModels(0).Faces.Num(), RefSkeleton.Num() );
}

void USkeletalMesh::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	FSkeletalMeshThumbnailScene	ThumbnailScene(this);

	InRI->DrawScene(
		FSceneContext(
			&ThumbnailScene.View,
			&ThumbnailScene,
			InX,
			InY,
			InFixedSz ? InFixedSz : 1024 * InZoom,
			InFixedSz ? InFixedSz : 1024 * InZoom
			)
		);

}

FThumbnailDesc USkeletalMesh::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : 1024*InZoom;
	td.Height = InFixedSz ? InFixedSz : 1024*InZoom;
	return td;

}

INT USkeletalMesh::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( GetName() );

	check(LODModels.Num());
	LODModels(0).Faces.Load();
	new( *InLabels )FString( FString::Printf( TEXT("%d Polys, %d Bones"), LODModels(0).Faces.Num(), RefSkeleton.Num() ) );

	return InLabels->Num();
}