/*=============================================================================
	UnSkeletalTools.cpp: Skeletal mesh helper classes.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/  

#include "EnginePrivate.h"
#include "UnMeshBuild.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif

#include "TessellationRendering.h"

//
//	SkeletalMesh_UVsEqual
//

static inline UBOOL SkeletalMesh_UVsEqual(const FMeshWedge& V1, const FMeshWedge& V2, const INT UVIndex = 0)
{
	const FVector2D& UV1 = V1.UVs[UVIndex];
	const FVector2D& UV2 = V2.UVs[UVIndex];

	if(Abs(UV1.X - UV2.X) > (1.0f / 1024.0f))
		return 0;

	if(Abs(UV1.Y - UV2.Y) > (1.0f / 1024.0f))
		return 0;

	return 1;
}

/** soft skin vertex with extra data about its source origins */
struct FSkinVertexMeta
{
	FSoftSkinVertex Vertex;
	/** optional set of influences */
	FVertexInfluence ExtraInfluence;
	DWORD PointWedgeIdx;
};

//
//	AddSkinVertex
//

static INT AddSkinVertex(TArray<FSkinVertexMeta>& Vertices,FSkinVertexMeta& VertexMeta )
{
	FSoftSkinVertex& Vertex = VertexMeta.Vertex;
	FVertexInfluence& ExtraInfluence = VertexMeta.ExtraInfluence;

	for(UINT VertexIndex = 0;VertexIndex < (UINT)Vertices.Num();VertexIndex++)
	{
		FSkinVertexMeta&	OtherVertexMeta = Vertices(VertexIndex);
		FSoftSkinVertex&	OtherVertex = OtherVertexMeta.Vertex;		

		if(!PointsEqual(OtherVertex.Position,Vertex.Position))
			continue;

		UBOOL bUVsEqual = TRUE;
		for( INT UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx )
		{
			if(Abs(Vertex.UVs[UVIdx].X - OtherVertex.UVs[UVIdx].X) > (1.0f / 1024.0f))
			{
				bUVsEqual = FALSE;
			};

			if(Abs(Vertex.UVs[UVIdx].Y - OtherVertex.UVs[UVIdx].Y) > (1.0f / 1024.0f))
			{
				bUVsEqual = FALSE;
			}
		}

		if( !bUVsEqual )
			continue;

		if(!NormalsEqual( OtherVertex.TangentX, Vertex.TangentX))
			continue;

		if(!NormalsEqual(OtherVertex.TangentY, Vertex.TangentY))
			continue;

		if(!NormalsEqual(OtherVertex.TangentZ, Vertex.TangentZ))
			continue;

		UBOOL	InfluencesMatch = 1;
		for(UINT InfluenceIndex = 0;InfluenceIndex < MAX_INFLUENCES;InfluenceIndex++)
		{
			if( Vertex.InfluenceBones[InfluenceIndex] != OtherVertex.InfluenceBones[InfluenceIndex] || 
				Vertex.InfluenceWeights[InfluenceIndex] != OtherVertex.InfluenceWeights[InfluenceIndex])
			{
				InfluencesMatch = 0;
				break;
			}
		}
		if(!InfluencesMatch)
			continue;

		// also check the extra influences since we don't want to weld vertices which may later be swapped to
		// use an alternate set of weights
		const FVertexInfluence&	OtherVertexExtraInfluence = OtherVertexMeta.ExtraInfluence;
		for(UINT InfluenceIndex = 0;InfluenceIndex < MAX_INFLUENCES;InfluenceIndex++)
		{
			if( ExtraInfluence.Bones.InfluenceBones[InfluenceIndex] != OtherVertexExtraInfluence.Bones.InfluenceBones[InfluenceIndex] || 
				ExtraInfluence.Weights.InfluenceWeights[InfluenceIndex] != OtherVertexExtraInfluence.Weights.InfluenceWeights[InfluenceIndex])
			{
				InfluencesMatch = 0;
				break;
			}
		}
		if(!InfluencesMatch)
			continue;

		return VertexIndex;
	}

	return Vertices.AddItem(VertexMeta);
}

// this is used for a sub-quadratic routine to find "equal" verts
struct FSkeletalMeshVertIndexAndZ
{
	INT Index;
	FLOAT Z;
};

// Sorting function for vertex Z/index pairs
IMPLEMENT_COMPARE_CONSTREF( FSkeletalMeshVertIndexAndZ, CreateSkinningStreamsFast, { if (A.Z < B.Z) return -1; if (A.Z > B.Z) return 1; return 0; } )
// Sorting function for ordering sections by material id usage (ascending order)
IMPLEMENT_COMPARE_CONSTREF( FSkelMeshSection, SortSectionsByMaterialId, { return A.MaterialIndex != B.MaterialIndex ? A.MaterialIndex - B.MaterialIndex : A.ChunkIndex - B.ChunkIndex; })
// Sorting function for ordering bones (ascending order)
IMPLEMENT_COMPARE_CONSTREF( BYTE, SortBones, { return A - B; })

// Sorting function for reference pose bones by the number of chunks they influence
class ChunkBoneMapCompare
{
public:
	static INT Compare(const TArray<INT>& BoneChunksA, const TArray<INT>& BoneChunksB)
	{
		INT BoneDiff = BoneChunksB.Num() - BoneChunksA.Num();
		if (BoneDiff == 0)
		{
		   return BoneChunksB(0) - BoneChunksA(0);
		}
		else
		{
		   return BoneDiff;
		}
	}
};

/** Calculate the required bones for this LOD, including possible extra influences */
void USkeletalMesh::CalculateRequiredBones(INT LODIdx)
{
	// RequiredBones for base model includes all bones.
	FStaticLODModel& LODModel = LODModels(LODIdx);
	INT RequiredBoneCount = RefSkeleton.Num();
	LODModel.RequiredBones.Add(RequiredBoneCount);
	for(INT i=0; i<RequiredBoneCount; i++)
	{
		LODModel.RequiredBones(i) = i;
	}

	// If this is the extra weights import, add "special" bones to the list (IK/etc)
	if (LODModel.VertexInfluences.Num() > 0)
	{
		// Determine the unique bones required for animation by the main model (only the skinned bones)
		TSet<BYTE> LODModelUniqueBones;
		for (INT ChunkIdx = 0; ChunkIdx < LODModel.Chunks.Num(); ChunkIdx++)
		{
			const FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIdx);
			for (INT BoneIdx = 0; BoneIdx < Chunk.BoneMap.Num(); BoneIdx++)
			{
				LODModelUniqueBones.Add(Chunk.BoneMap(BoneIdx));
			}
		}	

		for (INT VertInfluenceIdx=0; VertInfluenceIdx<LODModel.VertexInfluences.Num(); VertInfluenceIdx++)
		{
			// Determine the unique bones required for animation by the alternate weights model (only the skinned bones)
			FSkeletalMeshVertexInfluences& VertexInf = LODModel.VertexInfluences(VertInfluenceIdx);
			// Only keep track of reduce set of required bones when doing a full weight swap
			if (VertexInf.Usage != IWU_FullSwap)
			{
				continue;
			}

			TSet<BYTE> ExtraInfUniqueBones;
			for (INT ChunkIdx = 0; ChunkIdx < VertexInf.Chunks.Num(); ChunkIdx++)
			{
				const FSkelMeshChunk& Chunk = VertexInf.Chunks(ChunkIdx);
				for (INT BoneIdx = 0; BoneIdx < Chunk.BoneMap.Num(); BoneIdx++)
				{
					ExtraInfUniqueBones.Add(Chunk.BoneMap(BoneIdx));
				}
			}	

			// Find difference between the two models (one is always a superset of the other)
			TSet<BYTE> ChangedBones;
			if (LODModelUniqueBones.Num() > ExtraInfUniqueBones.Num())
			{
				ChangedBones = LODModelUniqueBones.Difference(ExtraInfUniqueBones);
			}
			else
			{
				// Have to update the LODModel.RequiredBones because its suboptimal 
				// Its using all bones when it could be using "LODModelUniqueBones + (Total - ExtraInfUniqueBones)"
				ChangedBones = ExtraInfUniqueBones.Difference(LODModelUniqueBones);
			}
			VertexInf.RequiredBones = LODModel.RequiredBones;

			// Remove the difference
			TArray<BYTE>& RequiredBones = LODModelUniqueBones.Num() > ExtraInfUniqueBones.Num() ? VertexInf.RequiredBones : LODModel.RequiredBones;
			for(TSet<BYTE>::TIterator BoneIt = TSet<BYTE>::TIterator(ChangedBones); BoneIt; ++BoneIt)
			{
				RequiredBones.RemoveItem(*BoneIt);
			}

			// Sort ascending for parent child relationship
			Sort<USE_COMPARE_CONSTREF(BYTE, SortBones)>( RequiredBones.GetData(), RequiredBones.Num() );
		}
	}
}

/**
* Create all render specific (but serializable) data like e.g. the 'compiled' rendering stream,
* mesh sections and index buffer.
*
* @todo: currently only handles LOD level 0.
*/
UBOOL USkeletalMesh::CreateSkinningStreams( 
	const TArray<FVertInfluence>& Influences, 
	const TArray<FMeshWedge>& Wedges, 
	const TArray<FMeshFace>& Faces, 
	const TArray<FVector>& Points,
	const FSkelMeshExtraInfluenceImportData* ExtraInfluenceData
	)
{
#if !CONSOLE
	UBOOL bHasMissingBoneSlots = FALSE;
	UBOOL bHasBadChunkBoneMapping = FALSE;
	UBOOL bHasMissingBoneMapping = FALSE;
	UBOOL bTooManyVerts = FALSE;
	check( LODModels.Num() );
	FStaticLODModel& LODModel = LODModels(0);

	if( ExtraInfluenceData != NULL )
	{
		// Validate some assumptions about the extra influence data
		FString ExtraErrorInfo;
		UBOOL bIsDataValid = TRUE;
		if (ExtraInfluenceData->Points.Num() != Points.Num() ||
			ExtraInfluenceData->Wedges.Num() != Wedges.Num() ||
			ExtraInfluenceData->Faces.Num() != Faces.Num())
		{	
			bIsDataValid = FALSE;
		}

		// Validate the skeleton hierarchy
		if (RefSkeleton.Num() == ExtraInfluenceData->RefSkeleton.Num())
		{
			for (INT BoneIdx=0; BoneIdx<RefSkeleton.Num(); BoneIdx++)
			{
				const FMeshBone& OrigBone = RefSkeleton(BoneIdx);
				const FMeshBone& WeightBone = ExtraInfluenceData->RefSkeleton(BoneIdx);
				if (OrigBone.Name != WeightBone.Name ||
					OrigBone.ParentIndex != WeightBone.ParentIndex)
				{
					ExtraErrorInfo = FString::Printf(TEXT("(%d)%s != %s"), BoneIdx, *OrigBone.Name.ToString(), *WeightBone.Name.ToString());
					warnf(TEXT("Importing weights: Mismatched skeletal hierarchy. %s Cannot import mesh weights"), *ExtraErrorInfo);
					bIsDataValid = FALSE;
					break;
				}
			}
		}
		else
		{		   
			warnf(TEXT("Importing weights: Mismatched skeletal hierarchy. Cannot import mesh weights"));
			bIsDataValid = FALSE;
		}

		// Wedge vert order matters
		for (INT WedgeIdx=0; WedgeIdx<ExtraInfluenceData->Wedges.Num(); WedgeIdx++)
		{
			const FMeshWedge& BaseWedge = Wedges(WedgeIdx);
			const FMeshWedge& AltWedge = ExtraInfluenceData->Wedges(WedgeIdx);

			if (BaseWedge.iVertex != AltWedge.iVertex)
			{
				bIsDataValid = FALSE;
				break;
			}
		}

		if (!bIsDataValid)
		{
			warnf(TEXT("Importing weights: Mismatched mesh data. Cannot import mesh weights"));
			const FString ErrorString = LocalizeUnrealEd("ImportMeshWeightsMismatch");
			appMsgf( AMT_OK, *(ErrorString + TEXT("\n") + ExtraErrorInfo));
			ExtraInfluenceData = NULL;
			return FALSE;
		}
	}

	// Allow multiple calls to CreateSkinningStreams for same model/LOD.

	LODModel.Sections.Empty();
	LODModel.Chunks.Empty();
	LODModel.NumVertices = 0;
	LODModel.VertexInfluences.Empty();
	if (LODModel.MultiSizeIndexContainer.IsIndexBufferValid())
	{
		LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Empty();
	}

	// Calculate face tangent vectors.

	TArray<FVector>	FaceTangentX(Faces.Num());
	TArray<FVector>	FaceTangentY(Faces.Num());

	for(INT FaceIndex = 0;FaceIndex < Faces.Num();FaceIndex++)
	{
		FVector	P1 = Points(Wedges(Faces(FaceIndex).iWedge[0]).iVertex),
				P2 = Points(Wedges(Faces(FaceIndex).iWedge[1]).iVertex),
				P3 = Points(Wedges(Faces(FaceIndex).iWedge[2]).iVertex);
		FVector	TriangleNormal = FPlane(P3,P2,P1);
		FMatrix	ParameterToLocal(
			FPlane(	P2.X - P1.X,	P2.Y - P1.Y,	P2.Z - P1.Z,	0	),
			FPlane(	P3.X - P1.X,	P3.Y - P1.Y,	P3.Z - P1.Z,	0	),
			FPlane(	P1.X,			P1.Y,			P1.Z,			0	),
			FPlane(	0,				0,				0,				1	)
			);

		FLOAT	U1 = Wedges(Faces(FaceIndex).iWedge[0]).UVs[0].X,
				U2 = Wedges(Faces(FaceIndex).iWedge[1]).UVs[0].X,
				U3 = Wedges(Faces(FaceIndex).iWedge[2]).UVs[0].X,
				V1 = Wedges(Faces(FaceIndex).iWedge[0]).UVs[0].Y,
				V2 = Wedges(Faces(FaceIndex).iWedge[1]).UVs[0].Y,
				V3 = Wedges(Faces(FaceIndex).iWedge[2]).UVs[0].Y;

		FMatrix	ParameterToTexture(
			FPlane(	U2 - U1,	V2 - V1,	0,	0	),
			FPlane(	U3 - U1,	V3 - V1,	0,	0	),
			FPlane(	U1,			V1,			1,	0	),
			FPlane(	0,			0,			0,	1	)
			);

		FMatrix	TextureToLocal = ParameterToTexture.InverseSlow() * ParameterToLocal;
		FVector	TangentX = TextureToLocal.TransformNormal(FVector(1,0,0)).SafeNormal(),
				TangentY = TextureToLocal.TransformNormal(FVector(0,1,0)).SafeNormal(),
				TangentZ;

		TangentX = TangentX - TriangleNormal * (TangentX | TriangleNormal);
		TangentY = TangentY - TriangleNormal * (TangentY | TriangleNormal);

		FaceTangentX(FaceIndex) = TangentX.SafeNormal();
		FaceTangentY(FaceIndex) = TangentY.SafeNormal();
	}

	TArray<INT>	WedgeInfluenceIndices;

	// Find wedge influences.
	TMap<DWORD,UINT> VertexIndexToInfluenceIndexMap;

	for(UINT LookIdx = 0;LookIdx < (UINT)Influences.Num();LookIdx++)
	{
		// Order matters do not allow the map to overwrite an existing value.
		if( !VertexIndexToInfluenceIndexMap.Find( Influences(LookIdx).VertIndex) )
		{
			VertexIndexToInfluenceIndexMap.Set( Influences(LookIdx).VertIndex, LookIdx );
		}
	}

	for(INT WedgeIndex = 0;WedgeIndex < Wedges.Num();WedgeIndex++)
	{
		UINT* InfluenceIndex = VertexIndexToInfluenceIndexMap.Find( Wedges(WedgeIndex).iVertex );

		check( InfluenceIndex );

		WedgeInfluenceIndices.AddItem( *InfluenceIndex );
	}

 	check(Wedges.Num() == WedgeInfluenceIndices.Num());

	// update wedge influences for optional data
	// assumes that Wedges/Faces map one-to-one to ExtraInfluenceData->Wedges/Faces
	TArray<INT> ExtraWedgeInfluenceIndices;	
	FSkeletalMeshVertexInfluences* ExtraVertInfluences = NULL;
	if( ExtraInfluenceData != NULL )
	{
		for(INT WedgeIndex = 0;WedgeIndex < Wedges.Num();WedgeIndex++)
		{
			for(UINT LookIdx = 0;LookIdx < (UINT)ExtraInfluenceData->Influences.Num();LookIdx++)
			{
				if(ExtraInfluenceData->Influences(LookIdx).VertIndex == Wedges(WedgeIndex).iVertex)
				{
					ExtraWedgeInfluenceIndices.AddItem(LookIdx);
					break;
				}
			}
		}
		check(Wedges.Num() == ExtraWedgeInfluenceIndices.Num());

		// add to LODModel.VertexInfluences for extra influence data	
		ExtraVertInfluences = new(LODModel.VertexInfluences) FSkeletalMeshVertexInfluences;
		ExtraVertInfluences->Usage = ExtraInfluenceData->Usage;
		ExtraVertInfluences->Chunks.Reserve(20);
	}

	// Calculate smooth wedge tangent vectors.

	if( IsInGameThread() )
	{
		// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
		GWarn->BeginSlowTask( *LocalizeUnrealEd("ProcessingSkeletalTriangles"), TRUE );
	}


	// To accelerate generation of adjacency, we'll create a table that maps each vertex index
	// to its overlapping vertices, and a table that maps a vertex to the its influenced faces
	TMultiMap<INT,INT> Vert2Duplicates;
	TMultiMap<INT,INT> Vert2Faces;
	{
		// Create a list of vertex Z/index pairs
		TArray<FSkeletalMeshVertIndexAndZ> VertIndexAndZ;
		VertIndexAndZ.Empty(Points.Num());
		for (INT i = 0; i < Points.Num(); i++)
		{
			FSkeletalMeshVertIndexAndZ iandz;
			iandz.Index = i;
			iandz.Z = Points(i).Z;
			VertIndexAndZ.AddItem(iandz);
		}

		// Sort the vertices by z value
		Sort<USE_COMPARE_CONSTREF(FSkeletalMeshVertIndexAndZ, CreateSkinningStreamsFast)>( VertIndexAndZ.GetTypedData(), VertIndexAndZ.Num() );

		// Search for duplicates, quickly!
		for (INT i = 0; i < VertIndexAndZ.Num(); i++)
		{
			// only need to search forward, since we add pairs both ways
			for (INT j = i + 1; j < VertIndexAndZ.Num(); j++)
			{
				if (Abs(VertIndexAndZ(j).Z - VertIndexAndZ(i).Z) > THRESH_POINTS_ARE_SAME * 4.01f)
				{
					// our list is sorted, so there can't be any more dupes
					break;
				}

				// check to see if the points are really overlapping
				if(PointsEqual(
					Points(VertIndexAndZ(i).Index),
					Points(VertIndexAndZ(j).Index) ))					
				{
					Vert2Duplicates.Add(VertIndexAndZ(i).Index,VertIndexAndZ(j).Index);
					Vert2Duplicates.Add(VertIndexAndZ(j).Index,VertIndexAndZ(i).Index);
				}
			}
		}

		// we are done with this
		VertIndexAndZ.Empty();

		// now create a map from vert indices to faces
		for(INT FaceIndex = 0;FaceIndex < Faces.Num();FaceIndex++)
		{
			const FMeshFace&	Face = Faces(FaceIndex);
			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				Vert2Faces.AddUnique(Wedges(Face.iWedge[VertexIndex]).iVertex,FaceIndex);
			}
		}
	}


	TArray<FRawIndexBuffer16or32*> SectionIndexBufferArray;
	TArray< TArray<FSkinVertexMeta>* > ChunkVerticesArray;
	TArray<INT> AdjacentFaces;
	TArray<INT> DupVerts;
	TArray<INT> DupFaces;

	for(INT FaceIndex = 0;FaceIndex < Faces.Num();FaceIndex++)
	{
		// Only update the status progress bar if we are in the gamethread and every thousand faces. 
		// Updating status is extremely slow
		if( IsInGameThread() && FaceIndex % 5000 == 0 )
		{
			// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
			GWarn->StatusUpdatef( FaceIndex, Faces.Num(), *LocalizeUnrealEd("ProcessingSkeletalTriangles") );
		}

		const FMeshFace&	Face = Faces(FaceIndex);

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
			Points(Wedges(Face.iWedge[2]).iVertex),
			Points(Wedges(Face.iWedge[1]).iVertex),
			Points(Wedges(Face.iWedge[0]).iVertex)
			);
		FLOAT	Determinant = FTriple(FaceTangentX(FaceIndex),FaceTangentY(FaceIndex),TriangleNormal);

		// Start building a list of faces adjacent to this triangle
		AdjacentFaces.Reset();
		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			INT vert = Wedges(Face.iWedge[VertexIndex]).iVertex;
			DupVerts.Reset();
			Vert2Duplicates.MultiFind(vert,DupVerts);
			DupVerts.AddItem(vert); // I am a "dupe" of myself
			for (INT k = 0; k < DupVerts.Num(); k++)
			{
				DupFaces.Reset();
				Vert2Faces.MultiFind(DupVerts(k),DupFaces);
				for (INT l = 0; l < DupFaces.Num(); l++)
				{
					AdjacentFaces.AddUniqueItem(DupFaces(l));
				}
			}
		}

		// Process adjacent faces
		for(INT AdjacentFaceIndex = 0;AdjacentFaceIndex < AdjacentFaces.Num();AdjacentFaceIndex++)
		{
			INT OtherFaceIndex = AdjacentFaces(AdjacentFaceIndex);
			const FMeshFace&	OtherFace = Faces(OtherFaceIndex);
			FVector		OtherTriangleNormal = FPlane(
							Points(Wedges(OtherFace.iWedge[2]).iVertex),
							Points(Wedges(OtherFace.iWedge[1]).iVertex),
							Points(Wedges(OtherFace.iWedge[0]).iVertex)
							);
			FLOAT		OtherFaceDeterminant = FTriple(FaceTangentX(OtherFaceIndex),FaceTangentY(OtherFaceIndex),OtherTriangleNormal);

			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				for(INT OtherVertexIndex = 0;OtherVertexIndex < 3;OtherVertexIndex++)
				{
					if(PointsEqual(
						Points(Wedges(OtherFace.iWedge[OtherVertexIndex]).iVertex),
						Points(Wedges(Face.iWedge[VertexIndex]).iVertex)
						))					
					{
						if(Determinant * OtherFaceDeterminant > 0.0f && SkeletalMesh_UVsEqual(Wedges(OtherFace.iWedge[OtherVertexIndex]),Wedges(Face.iWedge[VertexIndex])))
						{
							VertexTangentX[VertexIndex] += FaceTangentX(OtherFaceIndex);
							VertexTangentY[VertexIndex] += FaceTangentY(OtherFaceIndex);
						}

						// Only contribute 'normal' if the vertices are truly one and the same to obey hard "smoothing" edges baked into 
						// the mesh by vertex duplication
						if( Wedges(OtherFace.iWedge[OtherVertexIndex]).iVertex == Wedges(Face.iWedge[VertexIndex]).iVertex ) 
						{
							VertexTangentZ[VertexIndex] += OtherTriangleNormal;
						}
					}
				}
			}
		}

		// Find a section which matches this triangle.
		FSkelMeshSection* Section = NULL;
		FSkelMeshChunk* Chunk = NULL;
		FRawIndexBuffer16or32* SectionIndexBuffer = NULL;
		TArray<FSkinVertexMeta>* ChunkVertices = NULL;

		for(INT SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
		{
			FSkelMeshSection& ExistingSection = LODModel.Sections(SectionIndex);
			FSkelMeshChunk& ExistingChunk = LODModel.Chunks(ExistingSection.ChunkIndex);
			if(ExistingSection.MaterialIndex == Face.MeshMaterialIndex)
			{
				// Count the number of bones this triangles uses which aren't yet used by the section.
				TArray<WORD> UniqueBones;
				UBOOL NoSpaceForUniqueBones = FALSE;
				for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				{
					for(INT InfluenceIndex = WedgeInfluenceIndices(Face.iWedge[VertexIndex]);InfluenceIndex < Influences.Num();InfluenceIndex++)
					{
						const FVertInfluence& Influence = Influences(InfluenceIndex);
						if(Influence.VertIndex != Wedges(Face.iWedge[VertexIndex]).iVertex)
						{
							break;
						}
						if(ExistingChunk.BoneMap.FindItemIndex(Influence.BoneIndex) == INDEX_NONE)
						{
							// if the exiting chunk does not have space for new bones, then do not consider
 							if ( ExistingChunk.BoneMap.Num() >= MAX_GPUSKIN_BONES )
 							{
 								NoSpaceForUniqueBones = TRUE;
 								break;
 							}
	
							UniqueBones.AddUniqueItem(Influence.BoneIndex);
						}
					}

					// update unique bones using wedge influences for optional data
					// assumes that Wedges/Faces map one-to-one to ExtraInfluenceData->Wedges/Faces
					if( !NoSpaceForUniqueBones &&
						ExtraInfluenceData != NULL &&
						// For full swapping of weights then don't account for a superset of referenced bones when chunking
						ExtraInfluenceData->Usage != IWU_FullSwap &&
						ExtraWedgeInfluenceIndices.IsValidIndex(Face.iWedge[VertexIndex]) )
					{
						for( INT InfluenceIndex = ExtraWedgeInfluenceIndices(Face.iWedge[VertexIndex]);
							InfluenceIndex < ExtraInfluenceData->Influences.Num();
							InfluenceIndex++ )
						{
							const FVertInfluence& Influence = ExtraInfluenceData->Influences(InfluenceIndex);
							if(Influence.VertIndex != Wedges(Face.iWedge[VertexIndex]).iVertex)
							{
								break;
							}
							if(ExistingChunk.BoneMap.FindItemIndex(Influence.BoneIndex) == INDEX_NONE)
							{
								UniqueBones.AddUniqueItem(Influence.BoneIndex);
							}
						}
					}
				}

				if(!NoSpaceForUniqueBones && ExistingChunk.BoneMap.Num() + UniqueBones.Num() <= MAX_GPUSKIN_BONES)
				{
					// This section has enough room in its bone table to fit the bones used by this triangle.
					Section = &ExistingSection;
					Chunk = &ExistingChunk;
					SectionIndexBuffer = SectionIndexBufferArray(SectionIndex);
					ChunkVertices = ChunkVerticesArray(ExistingSection.ChunkIndex);
					break;
				}
			}
		}

		if(!Section)
		{
			// Create a new skeletal mesh section.
			Section = new(LODModel.Sections) FSkelMeshSection;
			Section->MaterialIndex = Face.MeshMaterialIndex;	

			SectionIndexBuffer = new FRawIndexBuffer16or32();
			SectionIndexBufferArray.AddItem(SectionIndexBuffer);

			// Create a chunk for the section.
			Chunk = new(LODModel.Chunks) FSkelMeshChunk;
			Section->ChunkIndex = LODModel.Chunks.Num() - 1;
			ChunkVertices = new TArray<FSkinVertexMeta>();
			ChunkVerticesArray.AddItem(ChunkVertices);
		}

		DWORD TriangleIndices[3];

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			FSoftSkinVertex	Vertex;

			Vertex.Position = Points(Wedges(Face.iWedge[VertexIndex]).iVertex);

			FVector TangentX,TangentY,TangentZ;

			if( !Face.bOverrideTangentBasis )
			{
				TangentX = VertexTangentX[VertexIndex].SafeNormal();
				TangentY = VertexTangentY[VertexIndex].SafeNormal();
				TangentZ = VertexTangentZ[VertexIndex].SafeNormal();

				TangentY -= TangentX * (TangentX | TangentY);
				TangentY.Normalize();

				TangentX -= TangentZ * (TangentZ | TangentX);
				TangentY -= TangentZ * (TangentZ | TangentY);

				TangentX.Normalize();
				TangentY.Normalize();
			}
			else
			{
				TangentX = Face.TangentX[VertexIndex];
				TangentY = Face.TangentY[VertexIndex];
				TangentZ = Face.TangentZ[VertexIndex];

				// Normalize overridden tangents.  Its possible for them to import un-normalized.
				TangentX.Normalize();
				TangentY.Normalize();
				TangentZ.Normalize();
			}

			Vertex.TangentX = TangentX;
			Vertex.TangentY = TangentY;
			Vertex.TangentZ = TangentZ;

			appMemcpy( Vertex.UVs,  Wedges(Face.iWedge[VertexIndex]).UVs, sizeof(FVector2D)*MAX_TEXCOORDS);	
			Vertex.Color = Wedges(Face.iWedge[VertexIndex]).Color;

			{
				// Count the influences.

				INT InfIdx = WedgeInfluenceIndices(Face.iWedge[VertexIndex]);
				INT LookIdx = InfIdx;

				UINT InfluenceCount = 0;
				while( Influences.IsValidIndex(LookIdx) && (Influences(LookIdx).VertIndex == Wedges(Face.iWedge[VertexIndex]).iVertex) )
				{			
					InfluenceCount++;
					LookIdx++;
				}
				InfluenceCount = Min<UINT>(InfluenceCount,MAX_INFLUENCES);

				// Setup the vertex influences.

				Vertex.InfluenceBones[0] = 0;
				Vertex.InfluenceWeights[0] = 255;
				for(UINT i = 1;i < MAX_INFLUENCES;i++)
				{
					Vertex.InfluenceBones[i] = 0;
					Vertex.InfluenceWeights[i] = 0;
				}

				UINT	TotalInfluenceWeight = 0;
				for(UINT i = 0;i < InfluenceCount;i++)
				{
					BYTE BoneIndex = (BYTE)Influences(InfIdx+i).BoneIndex;
					if( BoneIndex >= RefSkeleton.Num() )
						continue;

					LODModel.ActiveBoneIndices.AddUniqueItem(BoneIndex);
					Vertex.InfluenceBones[i] = Chunk->BoneMap.AddUniqueItem(BoneIndex);
					Vertex.InfluenceWeights[i] = (BYTE)(Influences(InfIdx+i).Weight * 255.0f);
					TotalInfluenceWeight += Vertex.InfluenceWeights[i];
				}
				Vertex.InfluenceWeights[0] += 255 - TotalInfluenceWeight;
			}

			// do the same thing for any extra vertex influences
			// add any extra bone indices which are needed to the active bones 
			FVertexInfluence ExtraInfluence;
			appMemzero(&ExtraInfluence,sizeof(ExtraInfluence));
			if( ExtraInfluenceData != NULL &&
				ExtraWedgeInfluenceIndices.IsValidIndex(Face.iWedge[VertexIndex]) )
			{
				// Count the influences.

				INT InfIdx = ExtraWedgeInfluenceIndices(Face.iWedge[VertexIndex]);
				INT LookIdx = InfIdx;

				UINT InfluenceCount = 0;
				while( ExtraInfluenceData->Influences.IsValidIndex(LookIdx) && 
					(ExtraInfluenceData->Influences(LookIdx).VertIndex == Wedges(Face.iWedge[VertexIndex]).iVertex) )
				{			
					InfluenceCount++;
					LookIdx++;
				}
				InfluenceCount = Min<UINT>(InfluenceCount,MAX_INFLUENCES);

				// Setup the vertex influences.

				ExtraInfluence.Bones.InfluenceBones[0] = 0;
				ExtraInfluence.Weights.InfluenceWeights[0] = 255;
				for(UINT i = 1;i < MAX_INFLUENCES;i++)
				{
					ExtraInfluence.Bones.InfluenceBones[i] = 0;
					ExtraInfluence.Weights.InfluenceWeights[i] = 0;
				}

				UINT	TotalInfluenceWeight = 0;
				for(UINT i = 0;i < InfluenceCount;i++)
				{
					BYTE BoneIndex = (BYTE)ExtraInfluenceData->Influences(InfIdx+i).BoneIndex;
					if( BoneIndex >= RefSkeleton.Num() )
						continue;

					LODModel.ActiveBoneIndices.AddUniqueItem(BoneIndex);
					// for full weights swap just store the index to the RefSkeleton instead of the index of the Chunk->BoneMap as it will be fixed up later once new bonemaps are created
					ExtraInfluence.Bones.InfluenceBones[i] = ExtraInfluenceData->Usage == IWU_FullSwap ? BoneIndex : Chunk->BoneMap.AddUniqueItem(BoneIndex);
					ExtraInfluence.Weights.InfluenceWeights[i] = (BYTE)(ExtraInfluenceData->Influences(InfIdx+i).Weight * 255.0f);
					TotalInfluenceWeight += ExtraInfluence.Weights.InfluenceWeights[i];
				}
				ExtraInfluence.Weights.InfluenceWeights[0] += 255 - TotalInfluenceWeight;
			}

			// Add the vertex as well as its original index in the points array
			FSkinVertexMeta VertexMeta = { Vertex, ExtraInfluence, Wedges(Face.iWedge[VertexIndex]).iVertex };

			INT	V = AddSkinVertex(*ChunkVertices,VertexMeta);


			// set the index entry for the newly added vertex
			// check(V >= 0 && V <= MAXWORD);
#if DISALLOW_32BIT_INDICES
			if (V > MAXWORD)
			{
				bTooManyVerts = TRUE;
			}
			TriangleIndices[VertexIndex] = (WORD)V;
#else
			if (V > (UINT)MAXDWORD)
			{
				bTooManyVerts = TRUE;
			}
			TriangleIndices[VertexIndex] = (DWORD)V;
#endif
		}

		if(TriangleIndices[0] != TriangleIndices[1] && TriangleIndices[0] != TriangleIndices[2] && TriangleIndices[1] != TriangleIndices[2])
		{
			for(UINT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				SectionIndexBuffer->Indices.AddItem(TriangleIndices[VertexIndex]);
			}
		}
	}

	// Sort sections by material id so that index buffer and chunk vertex buffer entries are contiguous for sections with matching material usage
	Sort<USE_COMPARE_CONSTREF(FSkelMeshSection, SortSectionsByMaterialId)>( LODModel.Sections.GetTypedData(), LODModel.Sections.Num() );
	// Reorder section index buffers to match new sorted section order
	TArray<FRawIndexBuffer16or32*> OldSectionIndexBufferArray = SectionIndexBufferArray;
	SectionIndexBufferArray.Empty(SectionIndexBufferArray.Num());
	for(INT SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection& ExistingSection = LODModel.Sections(SectionIndex);
		SectionIndexBufferArray.AddItem(OldSectionIndexBufferArray(ExistingSection.ChunkIndex));
	}
	// Reorder vertex arrays of chunks to match new sorted section order
	TArray< TArray<FSkinVertexMeta>* > OldChunkVerticesArray = ChunkVerticesArray;
	ChunkVerticesArray.Empty(ChunkVerticesArray.Num());
	for(INT SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection& ExistingSection = LODModel.Sections(SectionIndex);
		ChunkVerticesArray.AddItem(OldChunkVerticesArray(ExistingSection.ChunkIndex));
	}
	// Reorder chunks to match new sorted section order
	TArray<FSkelMeshChunk> OldChunks = LODModel.Chunks;
	LODModel.Chunks.Empty(LODModel.Chunks.Num());	
	for(INT SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection& ExistingSection = LODModel.Sections(SectionIndex);		
		const FSkelMeshChunk& ExistingChunk = OldChunks(ExistingSection.ChunkIndex);
		FSkelMeshChunk& NewChunk = *new(LODModel.Chunks) FSkelMeshChunk;
		NewChunk = ExistingChunk;
		ExistingSection.ChunkIndex = LODModel.Chunks.Num()-1;
	}

	// Keep track of index mapping to chunk vertex offsets
	TArray< TArray<DWORD> > VertexIndexRemap;
	// Pack the chunk vertices into a single vertex buffer.
	TArray<DWORD> RawPointIndices;	
	LODModel.NumVertices = 0;

	INT PrevMaterialIndex = -1;
	INT CurrentChunkBaseVertexIndex = -1; 	// base vertex index for all chunks of the same material
	INT CurrentChunkVertexCount = -1; 		// total vertex count for all chunks of the same material
	INT CurrentVertexIndex = 0; 			// current vertex index added to the index buffer for all chunks of the same material

	for(INT SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel.Sections(SectionIndex);
		INT ChunkIndex = Section.ChunkIndex;
		FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIndex); 
		TArray<FSkinVertexMeta>& ChunkVertices = *ChunkVerticesArray(ChunkIndex);

		if( IsInGameThread() )
		{
			// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
			GWarn->StatusUpdatef( ChunkIndex, LODModel.Chunks.Num(), *LocalizeUnrealEd("ProcessingChunks") );
		}

		UBOOL bFullSwap = (ExtraVertInfluences != NULL && ExtraVertInfluences->Usage == IWU_FullSwap);

		// If we are using full swap influences, all chunks of the same material
		// must start at the same base vertex because chunks merge ad hoc below
		// and their indices must all be relative to the same starting point
		if ((PrevMaterialIndex != Section.MaterialIndex) || !bFullSwap)
		{
			CurrentVertexIndex = 0;
			CurrentChunkVertexCount = 0;
			PrevMaterialIndex = Section.MaterialIndex;

			// Calculate the offset to this chunk's vertices in the vertex buffer.
			Chunk.BaseVertexIndex = CurrentChunkBaseVertexIndex = LODModel.NumVertices;
		}
		else
		{
			// All chunks with same material keep the same base vertex index
			Chunk.BaseVertexIndex = CurrentChunkBaseVertexIndex; 
		}

		// Update the size of the vertex buffer.  Allocate space in the vertex buffer for two versions of each vertex: unextruded and extruded for shadowing.
		LODModel.NumVertices += ChunkVertices.Num();

		// Separate the section's vertices into rigid and soft vertices.
		TArray<DWORD>& ChunkVertexIndexRemap = *new(VertexIndexRemap) TArray<DWORD>(ChunkVertices.Num());
		for(INT VertexIndex = 0;VertexIndex < ChunkVertices.Num();VertexIndex++)
		{
			const FSkinVertexMeta& VertexMeta = ChunkVertices(VertexIndex);
			const FSoftSkinVertex& SoftVertex = VertexMeta.Vertex;
			if(SoftVertex.InfluenceWeights[1] == 0)
			{
				FRigidSkinVertex RigidVertex;
				RigidVertex.Position = SoftVertex.Position;
				RigidVertex.TangentX = SoftVertex.TangentX;
				RigidVertex.TangentY = SoftVertex.TangentY;
				RigidVertex.TangentZ = SoftVertex.TangentZ;
				appMemcpy( RigidVertex.UVs, SoftVertex.UVs, sizeof(FVector2D)*MAX_TEXCOORDS );
				RigidVertex.Color = SoftVertex.Color;
				RigidVertex.Bone = SoftVertex.InfluenceBones[0];
				Chunk.RigidVertices.AddItem(RigidVertex);
				ChunkVertexIndexRemap(VertexIndex) = (DWORD)(Chunk.BaseVertexIndex + CurrentVertexIndex);
				CurrentVertexIndex++;
				// add the index to the original wedge point source of this vertex
				RawPointIndices.AddItem( VertexMeta.PointWedgeIdx );
				// add the optional vertex influence if it exists
				if( ExtraVertInfluences != NULL )
				{
					ExtraVertInfluences->Influences.AddItem(VertexMeta.ExtraInfluence);
				}
			}
		}
		for(INT VertexIndex = 0;VertexIndex < ChunkVertices.Num();VertexIndex++)
		{
			const FSkinVertexMeta& VertexMeta = ChunkVertices(VertexIndex);
			const FSoftSkinVertex& SoftVertex = VertexMeta.Vertex;
			if(SoftVertex.InfluenceWeights[1] > 0)
			{
				Chunk.SoftVertices.AddItem(SoftVertex);
				ChunkVertexIndexRemap(VertexIndex) = (DWORD)(Chunk.BaseVertexIndex + CurrentVertexIndex);
				CurrentVertexIndex++;
				// add the index to the original wedge point source of this vertex
				RawPointIndices.AddItem( VertexMeta.PointWedgeIdx );
				// add the optional vertex influence if it exists
				if( ExtraVertInfluences != NULL )
				{
					ExtraVertInfluences->Influences.AddItem(VertexMeta.ExtraInfluence);
				}
			}
		}

		// update total num of verts added
		if (bFullSwap)
		{
			// Treat everything as soft vertices when doing full swap
			// because they are all using the same base vertex index and
			// we aren't reordering the rigid vertices within
			Chunk.NumRigidVertices = 0;
			CurrentChunkVertexCount += Chunk.RigidVertices.Num() + Chunk.SoftVertices.Num();
			Chunk.NumSoftVertices = CurrentChunkVertexCount;
		}
		else
		{
			Chunk.NumRigidVertices = Chunk.RigidVertices.Num();
			Chunk.NumSoftVertices = Chunk.SoftVertices.Num();
		}

		// update max bone influences
		Chunk.CalcMaxBoneInfluences();

		// Log info about the chunk.
		debugf(TEXT("Chunk %u: %u rigid vertices, %u soft vertices, %u active bones"),
			ChunkIndex,
			Chunk.RigidVertices.Num(),
			Chunk.SoftVertices.Num(),
			Chunk.BoneMap.Num()
			);
	}
	// Delete chunk vertex data
	for(INT ChunkIndex = 0;ChunkIndex < LODModel.Chunks.Num();ChunkIndex++)
	{
		delete ChunkVerticesArray(ChunkIndex);
		ChunkVerticesArray(ChunkIndex) = NULL;
	}	

	// Copy raw point indices to LOD model.
	LODModel.RawPointIndices.RemoveBulkData();
	if( RawPointIndices.Num() )
	{
		LODModel.RawPointIndices.Lock(LOCK_READ_WRITE);
		void* Dest = LODModel.RawPointIndices.Realloc( RawPointIndices.Num() );
		appMemcpy( Dest, RawPointIndices.GetData(), LODModel.RawPointIndices.GetBulkDataSize() );
		LODModel.RawPointIndices.Unlock();
	}

	// TRUE if a section has no triangles
	UBOOL bHasBadSections = FALSE;
	UINT TotalNumberOfVerts = 0;

#if DISALLOW_32BIT_INDICES
	LODModel.MultiSizeIndexContainer.CreateIndexBuffer(sizeof(WORD));
#else
	LODModel.MultiSizeIndexContainer.CreateIndexBuffer((Wedges.Num() < MAXWORD)? sizeof(WORD): sizeof(DWORD));
#endif

	// Finish building the sections.
	for(INT SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		if( IsInGameThread() )
		{
			GWarn->StatusUpdatef( SectionIndex, LODModel.Sections.Num(), *LocalizeUnrealEd("ProcessingSections") );
		}

		FSkelMeshSection& Section = LODModel.Sections(SectionIndex);
		FRawIndexBuffer16or32& SectionIndexBuffer = *SectionIndexBufferArray(SectionIndex);
		
		// Reorder the section index buffer for better vertex cache efficiency.
		SectionIndexBuffer.CacheOptimize();

		// Calculate the number of triangles in the section.  Note that CacheOptimize may change the number of triangles in the index buffer!
		Section.NumTriangles = SectionIndexBuffer.Indices.Num() / 3;

		// Calculate the offset to this section's indices in the index buffer.
		Section.BaseIndex = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();

		// Remap the section index buffer.
		for(INT Index = 0;Index < SectionIndexBuffer.Indices.Num();Index++)
		{
			DWORD VertexIndex = VertexIndexRemap(Section.ChunkIndex)(SectionIndexBuffer.Indices(Index));
			LODModel.MultiSizeIndexContainer.GetIndexBuffer()->AddItem(VertexIndex);
		}

		if( Section.NumTriangles == 0 )
		{
			bHasBadSections = TRUE;
		}

		// Log info about the section.
		debugf(TEXT("Section %u: Material=%u, Chunk=%u, %u triangles"),
			SectionIndex,
			Section.MaterialIndex,
			Section.ChunkIndex,
			SectionIndexBuffer.Indices.Num() / 3
			);
	}
	// Delete section index data
	for(INT SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		delete SectionIndexBufferArray(SectionIndex);
		SectionIndexBufferArray(SectionIndex) = NULL;
	}

	// Add sections/chunks based on weighting of extra vert influences
	if (ExtraVertInfluences != NULL &&
		ExtraVertInfluences->Usage == IWU_FullSwap)
	{
		// Mapping of newly generated chunks and the bones used by each
		TMap< FSkelMeshChunk*,TArray<WORD> > ChunkUniqueBonesMap;
		// Array of indices to ref skeleton bones
		struct FBoneIndices
		{
			BYTE RefSkelBones[MAX_INFLUENCES];
		};
		FBoneIndices* BoneIndices = new FBoneIndices[ExtraVertInfluences->Influences.Num()];
		appMemzero(BoneIndices,sizeof(FBoneIndices)*ExtraVertInfluences->Influences.Num());

		// Iterate over base mesh sections and recreate unique ones for the new weighting
		for(INT BaseSectionIdx=0; BaseSectionIdx < LODModel.Sections.Num(); BaseSectionIdx++)
		{
			if( IsInGameThread() )
			{
				GWarn->StatusUpdatef( BaseSectionIdx, LODModel.Sections.Num(), *LocalizeUnrealEd("CreatingNewMeshSections") );
			}
			// original base mesh section/chunk 
			const FSkelMeshSection& BaseSection = LODModel.Sections(BaseSectionIdx);
			const FSkelMeshChunk& BaseChunk = LODModel.Chunks(BaseSection.ChunkIndex);
			// newly generated section/chunk
			FSkelMeshSection* Section = NULL;
			FSkelMeshChunk* Chunk = NULL;
			TArray<WORD>* ChunkUniqueBones = NULL;

			// Find an existing section based on material id with a chunk that has space based on bone usage
			for (INT SectionIdx=ExtraVertInfluences->Sections.Num()-1; SectionIdx >= 0; SectionIdx--)
			{
				FSkelMeshSection& ExistingSection = ExtraVertInfluences->Sections(SectionIdx);
				FSkelMeshChunk& ExistingChunk = ExtraVertInfluences->Chunks(ExistingSection.ChunkIndex);
				if (ExistingSection.MaterialIndex == BaseSection.MaterialIndex)
				{
					Section = &ExtraVertInfluences->Sections(SectionIdx);
					Chunk = &ExtraVertInfluences->Chunks(Section->ChunkIndex);
					// when merging chunks then keep track of total verts from all existing chunks
					if (BaseChunk.BaseVertexIndex != Chunk->BaseVertexIndex)
					{
						Chunk->NumSoftVertices += BaseChunk.GetNumVertices();
					}
					else
					{
						// When combining base chunks into chunks potentially made from a previous base
						// chunk split, take the greater of the two vertex counts
						Chunk->NumSoftVertices = Max<INT>(Chunk->NumSoftVertices, BaseChunk.GetNumVertices());
					}
					ChunkUniqueBones = ChunkUniqueBonesMap.Find(Chunk);
					break;
				}
			}
			// None found so create a new section/chunk entry for the material id
			if (Section == NULL)
			{	
				Section = new(ExtraVertInfluences->Sections) FSkelMeshSection;				
				Section->MaterialIndex = BaseSection.MaterialIndex;
				Section->BaseIndex = BaseSection.BaseIndex;
				Chunk = new(ExtraVertInfluences->Chunks) FSkelMeshChunk;
				Chunk->BaseVertexIndex = BaseChunk.BaseVertexIndex;
				Chunk->NumSoftVertices = BaseChunk.GetNumVertices();
				Chunk->MaxBoneInfluences = 4;
				Section->ChunkIndex = ExtraVertInfluences->Sections.Num() - 1;
				ChunkUniqueBones = &ChunkUniqueBonesMap.Set(Chunk,TArray<WORD>());
			}
			// Iterate over indices/verts of this section to rebuild new sections/chunks for the alternate weighting
			INT MaxIdx = Min<INT>(BaseSection.BaseIndex + BaseSection.NumTriangles * 3, LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num());
			for (INT Idx=BaseSection.BaseIndex; Idx < MaxIdx; Idx+=3)
			{
				// Find the unique bones used by the verts of the triangle not already in the bonemap of the existing chunk
				TArray<WORD> UniqueBones;
				for (INT TriIdx=0; TriIdx < 3; TriIdx++)
				{
					DWORD VertexIndex = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(Idx+TriIdx);
					FVertexInfluence& Vertex = ExtraVertInfluences->Influences(VertexIndex);
					FBoneIndices& VertBoneIndices = BoneIndices[VertexIndex];
					for (INT InfluenceIdx=0; InfluenceIdx < MAX_INFLUENCES; InfluenceIdx++)
					{
						BYTE BoneIdx = Vertex.Bones.InfluenceBones[InfluenceIdx];
						BYTE Weight = Vertex.Weights.InfluenceWeights[InfluenceIdx];
						if (Weight > 0 && 
							!ChunkUniqueBones->ContainsItem(BoneIdx))
						{
							UniqueBones.AddUniqueItem(BoneIdx);
						}
						// keep track of bone index into ref skeleton
						VertBoneIndices.RefSkelBones[InfluenceIdx] = BoneIdx;
					}
				}
				// Determine if adding this triangle would put us over the bone limit for the chunk/section
				if ((ChunkUniqueBones->Num() + UniqueBones.Num()) > ExtraInfluenceData->MaxBoneCountPerChunk)
				{
					// Create a new chunk/section
					Section = new(ExtraVertInfluences->Sections) FSkelMeshSection;				
					Section->MaterialIndex = BaseSection.MaterialIndex;
					Section->BaseIndex = Idx;
					Chunk = new(ExtraVertInfluences->Chunks) FSkelMeshChunk;
					// when splitting chunks then use the same range of [BaseVertexIndex,BaseVertexIndex+BaseChunk.GetNumVertices()]
					Chunk->BaseVertexIndex = BaseChunk.BaseVertexIndex;					
					Chunk->NumSoftVertices = BaseChunk.GetNumVertices();
					Chunk->MaxBoneInfluences = 4;
					Section->ChunkIndex = ExtraVertInfluences->Sections.Num() - 1;	
					ChunkUniqueBones = &ChunkUniqueBonesMap.Set(Chunk,UniqueBones);
				}
				else
				{
					// Add unique bones to existing chunk
					ChunkUniqueBones->Append(UniqueBones);
				}
				// Keep track of num triangles added
				Section->NumTriangles++;
			}
		}


		// Get a mapping of chunks to their material ID
		TArray<INT> ChunkMaterialID;
		ChunkMaterialID.AddZeroed(ExtraVertInfluences->Chunks.Num());
		for (INT SectionIdx=0; SectionIdx<ExtraVertInfluences->Sections.Num(); SectionIdx++)
		{
			const FSkelMeshSection& MeshSection = ExtraVertInfluences->Sections(SectionIdx);
			ChunkMaterialID(MeshSection.ChunkIndex) = MeshSection.MaterialIndex;
		}

		for (INT ChunkStartIdx=0; ChunkStartIdx < ExtraVertInfluences->Chunks.Num(); )
		{
			// [ChunkStartIdx,ChunkEndIdx] used to iterate over consecutive chunks using the same MaterialID (ie. split chunks)
			INT ChunkEndIdx = ChunkStartIdx;
			while (ChunkEndIdx < ExtraVertInfluences->Chunks.Num() &&
				ChunkMaterialID(ChunkEndIdx) == ChunkMaterialID(ChunkStartIdx))
			{
				ChunkEndIdx++;
			}

			// Keep track of the ref skel bones in use by each of the chunks
			TMap< INT,TArray<INT> > RefBonesToChunks;
			for (INT ChunkIdx=ChunkStartIdx; ChunkIdx < ChunkEndIdx; ChunkIdx++)
			{
				FSkelMeshChunk& Chunk = ExtraVertInfluences->Chunks(ChunkIdx);
				TArray<WORD>& ChunkBoneMap = *ChunkUniqueBonesMap.Find(&Chunk);
				// Add the current chunk index to the list of bones used it uses
				for (INT BoneMapIdx=0; BoneMapIdx < ChunkBoneMap.Num(); BoneMapIdx++)
				{
					INT RefSkelIdx = ChunkBoneMap(BoneMapIdx);
					TArray<INT>* RefChunks = RefBonesToChunks.Find(RefSkelIdx);
					if (RefChunks == NULL)
					{
						RefChunks = &RefBonesToChunks.Set(RefSkelIdx,TArray<INT>());
					}
					RefChunks->AddUniqueItem(ChunkIdx);
				}
			}
			// Initialize new chunk bone maps pre-sized to MAX_GPUSKIN_BONES and set to -1 values to represent unused entries
			TArray< TArray<WORD> > NewChunkBoneMaps;
			NewChunkBoneMaps.Empty(ExtraVertInfluences->Chunks.Num());
			for (INT ChunkIdx=0; ChunkIdx < ExtraVertInfluences->Chunks.Num(); ChunkIdx++)
			{
				TArray<WORD>& NewBoneMap = *new(NewChunkBoneMaps) TArray<WORD>();
				NewBoneMap.Empty(MAX_GPUSKIN_BONES);
				for (INT BoneMapIdx=0; BoneMapIdx < MAX_GPUSKIN_BONES; BoneMapIdx++)
				{
					NewBoneMap.AddItem((WORD)-1);
				}
			}

			// Sort the mapping of ref indices to chunks that use them in order of most shared bones to least shared
			RefBonesToChunks.ValueSort<ChunkBoneMapCompare>();

			// For each reference pose bone, find a unique index across all chunks and set the reference bone index
			for (TMap< INT, TArray<INT> >::TConstIterator It(RefBonesToChunks); It; ++It)
			{
				const INT RefBoneIdx = It.Key();
				const TArray<INT>& ChunksUsingRefBone = It.Value();

				const INT ChunkIdx = ChunksUsingRefBone(0);
				TArray<WORD>& CurrentBoneMap = NewChunkBoneMaps(ChunkIdx);

				// Find an empty slot from one of the chunks that is also free on every other chunk using this ref index
				UBOOL bFoundSlot = FALSE;
				for (INT BoneMapIdx=0; BoneMapIdx < MAX_GPUSKIN_BONES; BoneMapIdx++)
				{
					if (CurrentBoneMap(BoneMapIdx) == (WORD)-1)
					{
						UBOOL bAllChunksEmpty = TRUE;
						for (INT OtherChunkIdx=1; OtherChunkIdx<ChunksUsingRefBone.Num(); OtherChunkIdx++)
						{
							ensure(ChunksUsingRefBone(OtherChunkIdx) != ChunkIdx);
							TArray<WORD>& OtherBoneMap = NewChunkBoneMaps(ChunksUsingRefBone(OtherChunkIdx));
							if (OtherBoneMap(BoneMapIdx) != (WORD)-1)
							{
								bAllChunksEmpty = FALSE;
								break;
							}
						}

						// This slot is free across all chunks
						if (bAllChunksEmpty)
						{
							bFoundSlot = TRUE;
							for (INT AllChunkIdx=0; AllChunkIdx<ChunksUsingRefBone.Num(); AllChunkIdx++)
							{
								const INT ChunkIdx = ChunksUsingRefBone(AllChunkIdx);
								TArray<WORD>& ChunkBoneMap = NewChunkBoneMaps(ChunkIdx);
								ensure(ChunkBoneMap(BoneMapIdx) == (WORD)-1);
								ChunkBoneMap(BoneMapIdx) = RefBoneIdx;
							}
							break;
						}
					}
				}

				if (!bFoundSlot)
				{
					warnf(TEXT("Unable to find location to place ref bone %d"), RefBoneIdx);
					bHasMissingBoneSlots = TRUE;
				}
			}

			// Set new bonemaps for chunks
			for (INT ChunkIdx=ChunkStartIdx; ChunkIdx < ChunkEndIdx; ChunkIdx++)
			{
				FSkelMeshChunk& Chunk = ExtraVertInfluences->Chunks(ChunkIdx);
				Chunk.BoneMap = NewChunkBoneMaps(ChunkIdx);
			}

			// Verify that every chunk in a section uses the exact same bone mapping
			for (INT ChunkIdx=ChunkStartIdx; ChunkIdx < ChunkEndIdx; ChunkIdx++)
			{
				FSkelMeshChunk& Chunk = ExtraVertInfluences->Chunks(ChunkIdx);
				for (INT BoneMapIdx=0; BoneMapIdx<Chunk.BoneMap.Num(); BoneMapIdx++)
				{
					if (Chunk.BoneMap(BoneMapIdx) != (WORD)-1)
					{
						// What other chunks reference the same bones
						TArray<INT>& ChunksUsed	= *RefBonesToChunks.Find(Chunk.BoneMap(BoneMapIdx));
						for (INT OtherChunkIdx=0; OtherChunkIdx<ChunksUsed.Num(); OtherChunkIdx++)
						{
							if (ChunksUsed(OtherChunkIdx) != ChunkIdx)
							{
								FSkelMeshChunk& OtherChunk = ExtraVertInfluences->Chunks(ChunksUsed(OtherChunkIdx));
								if (OtherChunk.BoneMap(BoneMapIdx) != Chunk.BoneMap(BoneMapIdx))
								{
									warnf(TEXT("Importing weights: Not all chunks share the same bone mapping."));
									bHasBadChunkBoneMapping = TRUE;
								}
							}
						}
					}
				}
			}

			// Find the one chunk with the largest vertex count within the same material index
			INT MaxVertexCount = ExtraVertInfluences->Chunks(ChunkStartIdx).GetNumVertices();
			INT LargestChunkIndex = ChunkStartIdx;
			for (INT ChunkIdx=ChunkStartIdx + 1; ChunkIdx < ChunkEndIdx; ChunkIdx++)
			{
				FSkelMeshChunk& Chunk = ExtraVertInfluences->Chunks(ChunkIdx);
				if (MaxVertexCount < Chunk.GetNumVertices())
				{
					MaxVertexCount = Chunk.GetNumVertices();
					LargestChunkIndex = ChunkIdx;
				}
			}

			// Only have to process one of the chunks within the same material index
			// because they all start at the same base vertex index, just do it
			// on the one that spans all the vertices
			FSkelMeshChunk& Chunk = ExtraVertInfluences->Chunks(LargestChunkIndex);	

			// Fixup vertex influence bone indices so they map to the newly created bonemaps
			Chunk.MaxBoneInfluences = 0;
			for (UINT VertIdx=Chunk.BaseVertexIndex; VertIdx < (Chunk.BaseVertexIndex + Chunk.GetNumVertices()); VertIdx++)
			{
				if( IsInGameThread() && VertIdx % 5000 == 0 )
				{
					GWarn->StatusUpdatef( VertIdx-Chunk.BaseVertexIndex, Chunk.GetNumVertices(), *LocalizeUnrealEd("FixupBoneMaps") );
				}

				FBoneIndices& VertBoneIndices = BoneIndices[VertIdx];
				FVertexInfluence& Vertex = ExtraVertInfluences->Influences(VertIdx);
				INT BonesUsed=0;
				for (INT InfluenceIdx=0; InfluenceIdx < MAX_INFLUENCES; InfluenceIdx++)
				{
					BYTE RefSkelIdx = VertBoneIndices.RefSkelBones[InfluenceIdx];
					if (Vertex.Weights.InfluenceWeights[InfluenceIdx] > 0)
					{
						// Find the bone entry used for the vertex in any chunk bonemap and use the index for the influence bone
						// This works since chunks will have the same common entry bonemap entry for shared bones
						INT BoneMapIdx = INDEX_NONE;
						for (INT OtherChunkIdx=ChunkStartIdx; OtherChunkIdx < ChunkEndIdx; OtherChunkIdx++)
						{
							FSkelMeshChunk& OtherChunk = ExtraVertInfluences->Chunks(OtherChunkIdx);
							BoneMapIdx = OtherChunk.BoneMap.FindItemIndex(RefSkelIdx);
							if (BoneMapIdx != INDEX_NONE)
							{
								break;
							}
						}
						if (BoneMapIdx == INDEX_NONE)
						{
							warnf(TEXT("Importing weights: Couldn't find bone in any bonemap when fixing up influence bone indices!"));
							BoneMapIdx=0;
							bHasMissingBoneMapping = TRUE;
						}
						Vertex.Bones.InfluenceBones[InfluenceIdx] = (BYTE)BoneMapIdx;
						BonesUsed++;
					}
					else
					{
						Vertex.Bones.InfluenceBones[InfluenceIdx] = 0;
					}
				}
				Chunk.MaxBoneInfluences = Max<INT>(BonesUsed,Chunk.MaxBoneInfluences);
			}

			// reorder bones so that there aren't any unused influence entries within the [0,BonesUsed] range
			for (UINT VertIdx=Chunk.BaseVertexIndex; VertIdx < (Chunk.BaseVertexIndex + Chunk.GetNumVertices()); VertIdx++)
			{
				if( IsInGameThread() && VertIdx % 5000 == 0 )
				{
					GWarn->StatusUpdatef( VertIdx-Chunk.BaseVertexIndex, Chunk.GetNumVertices(), *LocalizeUnrealEd("ReorderingBoneWeights") );
				}
				FVertexInfluence& Vertex = ExtraVertInfluences->Influences(VertIdx);
				for (INT InfluenceIdx=0; InfluenceIdx < MAX_INFLUENCES; InfluenceIdx++)
				{
					if (Vertex.Weights.InfluenceWeights[InfluenceIdx] == 0)
					{
						for( INT ExchangeIdx=InfluenceIdx+1; ExchangeIdx < MAX_INFLUENCES; ExchangeIdx++ )
						{
							if( Vertex.Weights.InfluenceWeights[ExchangeIdx] != 0 )
							{
								Exchange(Vertex.Weights.InfluenceWeights[InfluenceIdx],Vertex.Weights.InfluenceWeights[ExchangeIdx]);
								Exchange(Vertex.Bones.InfluenceBones[InfluenceIdx],Vertex.Bones.InfluenceBones[ExchangeIdx]);
								break;
							}
						}
					}
				}
			}	

			// Iterate on the next set of chunks with matching MaterialIDs
			ChunkStartIdx = ChunkEndIdx;
		}
		delete[] BoneIndices;

		for (INT ChunkIndex=0; ChunkIndex < ExtraVertInfluences->Chunks.Num(); ChunkIndex++)
		{
			FSkelMeshChunk& Chunk = ExtraVertInfluences->Chunks(ChunkIndex);
			// remove all trailing invalid entries, but can't touch invalid entries in the middle (messes up shared bone indices)
 			for (INT BoneMapxIdx=Chunk.BoneMap.Num()-1; BoneMapxIdx >= 0; BoneMapxIdx--)
 			{
 				if (Chunk.BoneMap(BoneMapxIdx) != ((WORD)-1))
 				{
					break;
				}
 					
				Chunk.BoneMap.Remove(BoneMapxIdx);
 			}
			// fixup all other entries that will not be used
			for (INT BoneMapxIdx=0; BoneMapxIdx < Chunk.BoneMap.Num(); BoneMapxIdx++)
			{
				if (Chunk.BoneMap(BoneMapxIdx) == ((WORD)-1))
				{
					Chunk.BoneMap(BoneMapxIdx) = Chunk.BoneMap(0);
				}
			}
		}		

		// Determine the unique bones required for animation by the influence swap (only the skinned bones)
		for (INT ChunkIdx = 0; ChunkIdx < ExtraVertInfluences->Chunks.Num(); ChunkIdx++)
		{
			const FSkelMeshChunk& Chunk = ExtraVertInfluences->Chunks(ChunkIdx);
			for (INT BoneIdx = 0; BoneIdx < Chunk.BoneMap.Num(); BoneIdx++)
			{
				ExtraVertInfluences->RequiredBones.AddUniqueItem(Chunk.BoneMap(BoneIdx));
			}
		}

		// Sort the bones in ascending order (parent -> child)
		Sort<USE_COMPARE_CONSTREF(BYTE, SortBones)>( ExtraVertInfluences->RequiredBones.GetData(), ExtraVertInfluences->RequiredBones.Num() );
	}

	#if WITH_EDITOR && WITH_D3D11_TESSELLATION
	if ( GRHIShaderPlatform == SP_PCD3D_SM5 )
	{
		TArray<FSoftSkinVertex> Vertices;
		LODModel.GetVertices( Vertices );

		FMultiSizeIndexContainerData IndexData;
		LODModel.MultiSizeIndexContainer.GetIndexBufferData( IndexData );

		FMultiSizeIndexContainerData AdjacencyIndexData;
		AdjacencyIndexData.bNeedsCPUAccess = IndexData.bNeedsCPUAccess;
		AdjacencyIndexData.bSetUpForInstancing = IndexData.bSetUpForInstancing;
		AdjacencyIndexData.DataTypeSize = IndexData.DataTypeSize;
		AdjacencyIndexData.NumVertsPerInstance = IndexData.NumVertsPerInstance;

		BuildSkeletalAdjacencyIndexBuffer( Vertices, LODModel.NumTexCoords, IndexData.Indices, AdjacencyIndexData.Indices );
		LODModel.AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer( AdjacencyIndexData );
	}
	#endif // #if WITH_EDITOR && WITH_D3D11_TESSELLATION

	if( IsInGameThread() )
	{
		// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
		GWarn->EndSlowTask();
		
	}

	// Only show these warnings if in the game thread.  When importing morph targets, this function can run in another thread and these warnings dont prevent the mesh from importing
	if( IsInGameThread() )
	{
		if (bHasMissingBoneSlots)
		{
			appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_SkeletalMeshHasMissingBoneSlots"), *GetName() ) );
		}

		if (bHasBadChunkBoneMapping)
		{
			appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_SkeletalMeshHasBadChunkBoneMapping"), *GetName() ) );
		}

		if (bHasMissingBoneMapping)
		{
			appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_SkeletalMeshHasMissingBoneMapping"), *GetName() ) );
		}
												 
		if( bHasBadSections )
		{
			appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_SkeletalMeshHasBadSections"), *GetName() ) );
		}

		if (bTooManyVerts)
		{
			debugf(TEXT("Input mesh has too many vertices.  The generated mesh will be corrupt!"));
			appMsgf( AMT_OK,TEXT("Input mesh has too many vertices.  The generated mesh will be corrupt!  Consider adding extra materials to split up the source mesh into smaller chunks."));
		}
	}

	return TRUE;
#else
	appErrorf(TEXT("Cannot call USkeletalMesh::CreateSkinningStreams on a console!"));
	return FALSE;
#endif
}


// Pre-calculate refpose-to-local transforms
void USkeletalMesh::CalculateInvRefMatrices()
{
	if( RefBasesInvMatrix.Num() != RefSkeleton.Num() )
	{
		RefBasesInvMatrix.Empty(RefSkeleton.Num());
		RefBasesInvMatrix.Add(RefSkeleton.Num());

		// Temporary storage for calculating mesh-space ref pose
		TArray<FMatrix> RefBases;
		RefBases.Add( RefSkeleton.Num() );

		// Precompute the Mesh.RefBasesInverse.
		for( INT b=0; b<RefSkeleton.Num(); b++)
		{
			// Make sure quaternions are normalized. It's a little scary that we can get ref skeletons not normalized!
			RefSkeleton(b).BonePos.Orientation.Normalize();
			// Render the default pose.
			RefBases(b) = GetRefPoseMatrix(b);

			// Construct mesh-space skeletal hierarchy.
			if( b>0 )
			{
				INT Parent = RefSkeleton(b).ParentIndex;
				RefBases(b) = RefBases(b) * RefBases(Parent);
			}

			// Precompute inverse so we can use from-refpose-skin vertices.
			RefBasesInvMatrix(b) = FBoneAtom(RefBases(b).Inverse()); 
		}
	}
}

// Find the most dominant bone for each vertex
static INT GetDominantBoneIndex(FSoftSkinVertex* SoftVert)
{
	BYTE MaxWeightBone = 0;
	BYTE MaxWeightWeight = 0;

	for(INT i=0; i<MAX_INFLUENCES; i++)
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
	for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);
		for(INT i=0; i<Chunk.RigidVertices.Num(); i++)
		{
			FRigidSkinVertex* RigidVert = &Chunk.RigidVertices(i);
			INT BoneIndex = Chunk.BoneMap(RigidVert->Bone);

			FVector LocalPos = RefBasesInvMatrix(BoneIndex).TransformFVector(RigidVert->Position);
			Infos(BoneIndex).Positions.AddItem(LocalPos);

			FVector LocalNormal = RefBasesInvMatrix(BoneIndex).TransformNormal(RigidVert->TangentZ);
			Infos(BoneIndex).Normals.AddItem(LocalNormal);
		}

		for(INT i=0; i<Chunk.SoftVertices.Num(); i++)
		{
			FSoftSkinVertex* SoftVert = &Chunk.SoftVertices(i);

			if(bOnlyDominant)
			{
				INT BoneIndex = Chunk.BoneMap(GetDominantBoneIndex(SoftVert));

				FVector LocalPos = RefBasesInvMatrix(BoneIndex).TransformFVector(SoftVert->Position);
				Infos(BoneIndex).Positions.AddItem(LocalPos);

				FVector LocalNormal = RefBasesInvMatrix(BoneIndex).TransformNormal(SoftVert->TangentZ);
				Infos(BoneIndex).Normals.AddItem(LocalNormal);
			}
			else
			{
				for(INT j=0; j<MAX_INFLUENCES; j++)
				{
					if(SoftVert->InfluenceWeights[j] > 0.01f)
					{
						INT BoneIndex = Chunk.BoneMap(SoftVert->InfluenceBones[j]);

						FVector LocalPos = RefBasesInvMatrix(BoneIndex).TransformFVector(SoftVert->Position);
						Infos(BoneIndex).Positions.AddItem(LocalPos);

						FVector LocalNormal = RefBasesInvMatrix(BoneIndex).TransformNormal(SoftVert->TangentZ);
						Infos(BoneIndex).Normals.AddItem(LocalNormal);
					}
				}
			}
		}
	}
}

/**
 * Find if one bone index is a child of another.
 * Note - will return FALSE if ChildBoneIndex is the same as ParentBoneIndex ie. must be strictly a child.
 */
UBOOL USkeletalMesh::BoneIsChildOf(INT ChildBoneIndex, INT ParentBoneIndex) const
{
	// Bones are in strictly increasing order.
	// So child must have an index greater than his parent.
	if( ChildBoneIndex <= ParentBoneIndex )
	{
		return FALSE;
	}

	INT BoneIndex = RefSkeleton(ChildBoneIndex).ParentIndex;
	while(1)
	{
		if( BoneIndex == ParentBoneIndex )
		{
			return TRUE;
		}

		if( BoneIndex == 0 )
		{
			return FALSE;
		}
		BoneIndex = RefSkeleton(BoneIndex).ParentIndex;
	}
}

/** Allocate and initialise bone mirroring table for this skeletal mesh. Default is source = destination for each bone. */
void USkeletalMesh::InitBoneMirrorInfo()
{
	SkelMirrorTable.Empty(RefSkeleton.Num());
	SkelMirrorTable.AddZeroed(RefSkeleton.Num());

	// By default, no bone mirroring, and source is ourself.
	for(INT i=0; i<SkelMirrorTable.Num(); i++)
	{
		SkelMirrorTable(i).SourceIndex = i;
	}
}

/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::CopyMirrorTableFrom(USkeletalMesh* SrcMesh)
{
	// Do nothing if no mirror table in source mesh
	if(SrcMesh->SkelMirrorTable.Num() == 0)
	{
		return;
	}

	// First, allocate and default mirroring table.
	InitBoneMirrorInfo();

	// Keep track of which entries in the source we have already copied
	TArray<UBOOL> EntryCopied;
	EntryCopied.AddZeroed( SrcMesh->SkelMirrorTable.Num() );

	// Mirror table must always be size of ref skeleton.
	check(SrcMesh->SkelMirrorTable.Num() == SrcMesh->RefSkeleton.Num());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(INT i=0; i<SrcMesh->SkelMirrorTable.Num(); i++)
	{
		if(!EntryCopied(i))
		{
			// Get name of source and dest bone for this entry in the source table.
			FName DestBoneName = SrcMesh->RefSkeleton(i).Name;
			INT SrcBoneIndex = SrcMesh->SkelMirrorTable(i).SourceIndex;
			FName SrcBoneName = SrcMesh->RefSkeleton(SrcBoneIndex).Name;
			BYTE FlipAxis = SrcMesh->SkelMirrorTable(i).BoneFlipAxis;

			// Look up bone names in target mesh (this one)
			INT DestBoneIndexTarget = MatchRefBone(DestBoneName);
			INT SrcBoneIndexTarget = MatchRefBone(SrcBoneName);

			// If both bones found, copy data to this mesh's mirror table.
			if( DestBoneIndexTarget != INDEX_NONE && SrcBoneIndexTarget != INDEX_NONE )
			{
				SkelMirrorTable(DestBoneIndexTarget).SourceIndex = SrcBoneIndexTarget;
				SkelMirrorTable(DestBoneIndexTarget).BoneFlipAxis = FlipAxis;


				SkelMirrorTable(SrcBoneIndexTarget).SourceIndex = DestBoneIndexTarget;
				SkelMirrorTable(SrcBoneIndexTarget).BoneFlipAxis = FlipAxis;

				// Flag entries as copied, so we don't try and do it again.
				EntryCopied(i) = TRUE;
				EntryCopied(SrcBoneIndex) = TRUE;
			}
		}
	}
}

/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::ExportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo)
{
	// Do nothing if no mirror table in source mesh
	if( SkelMirrorTable.Num() == 0 )
	{
		return;
	}
	
	// Mirror table must always be size of ref skeleton.
	check(SkelMirrorTable.Num() == RefSkeleton.Num());

	MirrorExportInfo.Empty(SkelMirrorTable.Num());
	MirrorExportInfo.AddZeroed(SkelMirrorTable.Num());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(INT i=0; i<SkelMirrorTable.Num(); i++)
	{
		MirrorExportInfo(i).BoneName		= RefSkeleton(i).Name;
		MirrorExportInfo(i).SourceBoneName	= RefSkeleton(SkelMirrorTable(i).SourceIndex).Name;
		MirrorExportInfo(i).BoneFlipAxis	= SkelMirrorTable(i).BoneFlipAxis;
	}
}


/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::ImportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo)
{
	// Do nothing if no mirror table in source mesh
	if( MirrorExportInfo.Num() == 0 )
	{
		return;
	}

	// First, allocate and default mirroring table.
	InitBoneMirrorInfo();

	// Keep track of which entries in the source we have already copied
	TArray<UBOOL> EntryCopied;
	EntryCopied.AddZeroed( RefSkeleton.Num() );

	// Mirror table must always be size of ref skeleton.
	check(SkelMirrorTable.Num() == RefSkeleton.Num());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(INT i=0; i<MirrorExportInfo.Num(); i++)
	{
		FName DestBoneName	= MirrorExportInfo(i).BoneName;
		INT DestBoneIndex	= MatchRefBone(DestBoneName);

		if( DestBoneIndex != INDEX_NONE && !EntryCopied(DestBoneIndex) )
		{
			FName SrcBoneName	= MirrorExportInfo(i).SourceBoneName;
			INT SrcBoneIndex	= MatchRefBone(SrcBoneName);
			BYTE FlipAxis		= MirrorExportInfo(i).BoneFlipAxis;

			// If both bones found, copy data to this mesh's mirror table.
			if( SrcBoneIndex != INDEX_NONE )
			{
				SkelMirrorTable(DestBoneIndex).SourceIndex = SrcBoneIndex;
				SkelMirrorTable(DestBoneIndex).BoneFlipAxis = FlipAxis;

				SkelMirrorTable(SrcBoneIndex).SourceIndex = DestBoneIndex;
				SkelMirrorTable(SrcBoneIndex).BoneFlipAxis = FlipAxis;

				// Flag entries as copied, so we don't try and do it again.
				EntryCopied(DestBoneIndex)	= TRUE;
				EntryCopied(SrcBoneIndex)	= TRUE;
			}
		}
	}
}

/** 
 *	Utility for checking that the bone mirroring table of this mesh is good.
 *	Return TRUE if mirror table is OK, false if there are problems.
 *	@param	ProblemBones	Output string containing information on bones that are currently bad.
 */
UBOOL USkeletalMesh::MirrorTableIsGood(FString& ProblemBones)
{
	TArray<INT>	BadBoneMirror;

	for(INT i=0; i<SkelMirrorTable.Num(); i++)
	{
		INT SrcIndex = SkelMirrorTable(i).SourceIndex;
		if( SkelMirrorTable(SrcIndex).SourceIndex != i)
		{
			BadBoneMirror.AddItem(i);
		}
	}

	if(BadBoneMirror.Num() > 0)
	{
		for(INT i=0; i<BadBoneMirror.Num(); i++)
		{
			INT BoneIndex = BadBoneMirror(i);
			FName BoneName = RefSkeleton(BoneIndex).Name;

			ProblemBones += FString::Printf( TEXT("%s (%d)\n"), *BoneName.ToString(), BoneIndex );
		}

		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

#if WITH_EDITOR

/**
 * Retrieves the source model for this skeletal mesh.
 */
FStaticLODModel& USkeletalMesh::GetSourceModel()
{
	check( LODModels.Num() );
	if ( SourceData.IsInitialized() )
	{
		return *SourceData.GetModel();
	}
	return LODModels(0);
}

/**
 * Copies off the source model for this skeletal mesh if necessary and returns it. This function should always be called before
 * making destructive changes to the mesh's geometry, e.g. simplification.
 */
FStaticLODModel& USkeletalMesh::PreModifyMesh()
{
	if ( !SourceData.IsInitialized() && LODModels.Num() )
	{
		SourceData.Init( this, LODModels(0) );
	}
	check( SourceData.IsInitialized() );
	return GetSourceModel();
}

#endif // #if WITH_EDITOR

void USkeletalMesh::GenerateClothMovementScale()
{
	if(ClothMovementScaleGenMode == ECMDM_DistToFixedVert)
	{
		GenerateClothMovementScaleFromDistToFixed();
	}
	else if(ClothMovementScaleGenMode == ECMDM_VertexBoneWeight)
	{
		GenerateClothMovementScaleFromBoneWeight();
	}
	else if(ClothMovementScaleGenMode == ECMDM_Empty)
	{
		ClothMovementScale.Empty();
	}
}

/** Util to fill in the ClothMovementScale automatially based on how far a vertex is from a fixed one  */
void USkeletalMesh::GenerateClothMovementScaleFromDistToFixed()
{
	INT NumClothVerts = ClothToGraphicsVertMap.Num();
	INT NumFixedVerts = (NumClothVerts - NumFreeClothVerts);

	// No point doing this if no fixed verts!
	if(NumFixedVerts == 0)
	{
		return;
	}

	// Allocate info array
	ClothMovementScale.Empty();
	ClothMovementScale.AddZeroed(NumClothVerts);

	TArray<FVector> VertexPos;
	VertexPos.AddZeroed(NumClothVerts);

	FStaticLODModel& Model = LODModels(0);

	// Get all the positions for cloth verts (free and fixed)
	for(INT VertIdx=0; VertIdx<NumClothVerts; VertIdx++)
	{
		// Find the index of the graphics vertex that corresponds to this cloth vertex
		INT GraphicsIndex = ClothToGraphicsVertMap(VertIdx);

		// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
		INT ChunkIndex;
		INT VertIndex;
		UBOOL bSoftVertex;
		Model.GetChunkAndSkinType(GraphicsIndex, ChunkIndex, VertIndex, bSoftVertex);

		check(ChunkIndex < Model.Chunks.Num());
		const FSkelMeshChunk& Chunk = Model.Chunks(ChunkIndex);

		// Get the position
		if(bSoftVertex)
		{
			VertexPos(VertIdx) = Model.VertexBufferGPUSkin.GetVertexPosition(Chunk.GetSoftVertexBufferIndex()+VertIndex);
		}
		else
		{
			VertexPos(VertIdx) = Model.VertexBufferGPUSkin.GetVertexPosition(Chunk.GetRigidVertexBufferIndex()+VertIndex);
		}
	}

	// Furthest distance between any free vert and a fixed vert
	FLOAT FurthestDist = -BIG_NUMBER;

	// For each free vert..
	for(INT FreeVertIdx=0; FreeVertIdx<NumFreeClothVerts; FreeVertIdx++)
	{
		FVector& FreePos = VertexPos(FreeVertIdx);

		// Find closest distance to a fixed vert
		FLOAT ClosestDistSqr = BIG_NUMBER;
		for(INT FixedVertIdx=NumFreeClothVerts; FixedVertIdx<NumClothVerts; FixedVertIdx++)
		{
			FVector& FixedPos = VertexPos(FixedVertIdx);
			FLOAT DistSqr = (FixedPos - FreePos).SizeSquared();
			ClosestDistSqr = Min(DistSqr, ClosestDistSqr);
		}

		// Update table with this info
		ClothMovementScale(FreeVertIdx) = appSqrt(ClosestDistSqr);

		// Also update overall furthest distance between free and fixed vert
		FurthestDist = Max(ClothMovementScale(FreeVertIdx), FurthestDist);
	}

	// Finally rescale all ClothMovementScale entries to 0..1 range
	for(INT FreeVertIdx=0; FreeVertIdx<NumFreeClothVerts; FreeVertIdx++)
	{
		ClothMovementScale(FreeVertIdx) /= FurthestDist;
	}
}

/** Util to fill in the ClothMovementScale automatically based on how verts are weighted to cloth bones  */
void USkeletalMesh::GenerateClothMovementScaleFromBoneWeight()
{
	INT NumClothVerts = ClothToGraphicsVertMap.Num();
	INT NumFixedVerts = (NumClothVerts - NumFreeClothVerts);

	const INT RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();

	// Allocate info array
	ClothMovementScale.Empty();
	ClothMovementScale.AddZeroed(NumClothVerts);

	FStaticLODModel& Model = LODModels(0);

	// For each free vert..
	for(INT FreeVertIdx=0; FreeVertIdx<NumFreeClothVerts; FreeVertIdx++)
	{
		// Find the index of the graphics vertex that corresponds to this cloth vertex
		INT GraphicsIndex = ClothToGraphicsVertMap(FreeVertIdx);

		// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
		INT ChunkIndex;
		INT VertIndex;
		UBOOL bSoftVertex;
		Model.GetChunkAndSkinType(GraphicsIndex, ChunkIndex, VertIndex, bSoftVertex);

		check(ChunkIndex < Model.Chunks.Num());
		const FSkelMeshChunk& Chunk = Model.Chunks(ChunkIndex);

		// Get weight info from this vertex
		if(bSoftVertex)
		{
			const FGPUSkinVertexBase* SrcSoftVertex = Model.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetSoftVertexBufferIndex()+VertIndex);

#if !__INTEL_BYTE_ORDER__
			// BYTE[] elements in LOD.VertexBufferGPUSkin have been swapped for VET_UBYTE4 vertex stream use
			for(INT InfluenceIndex = MAX_INFLUENCES-1;InfluenceIndex >=  MAX_INFLUENCES-Chunk.MaxBoneInfluences;InfluenceIndex--)
#else
			for(INT InfluenceIndex = 0;InfluenceIndex < Chunk.MaxBoneInfluences;InfluenceIndex++)
#endif
			{
				BYTE BoneIndex = Chunk.BoneMap(SrcSoftVertex->InfluenceBones[InfluenceIndex]);
				FName BoneName = RefSkeleton(BoneIndex).Name;
				if(ClothBones.ContainsItem(BoneName))
				{
					// Accumulate for each cloth bone vert is weighted to
					ClothMovementScale(FreeVertIdx) += (FLOAT)SrcSoftVertex->InfluenceWeights[InfluenceIndex] / 255.0f;
				}
			}
		}
		else
		{
			const FGPUSkinVertexBase* SrcRigidVertex = Model.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()+VertIndex);

			BYTE BoneIndex = Chunk.BoneMap(SrcRigidVertex->InfluenceBones[RigidInfluenceIndex]);
			FName BoneName = RefSkeleton(BoneIndex).Name;
			if(ClothBones.ContainsItem(BoneName))
			{
				ClothMovementScale(FreeVertIdx) += 1.f;
			}
		}
	}
}

/** Uses the ClothBones array to analyze the graphics mesh and generate informaton needed to construct simulation mesh (ClothToGraphicsVertMap etc). */
void USkeletalMesh::BuildClothMapping()
{
	ClothToGraphicsVertMap.Empty();
	ClothIndexBuffer.Empty();

	FStaticLODModel* LODModel = &LODModels(0);

	// Make array of indices of bones whose verts are to be considered 'cloth'
	TArray<BYTE> ClothBoneIndices;
	for(INT i=0; i<ClothBones.Num(); i++)
	{
		INT BoneIndex = MatchRefBone(ClothBones(i));
		if(BoneIndex != INDEX_NONE)
		{
			check(BoneIndex < 255);
			ClothBoneIndices.AddItem( (BYTE)BoneIndex );
		}
	}
	
	// Add 'special' cloth bones. eg bones which are fixed to the physics asset
	for(INT i=0; i<ClothSpecialBones.Num(); i++)
	{
		INT BoneIndex = MatchRefBone(ClothSpecialBones(i).BoneName);
		if(BoneIndex != INDEX_NONE)
		{
			check(BoneIndex < 255);
			ClothBoneIndices.AddItem( (BYTE)BoneIndex );
		}
	}

	// Fail if no bones defined.
	if( ClothBoneIndices.Num() == 0)
	{
		return;
	}

	TArray<FVector> AllClothVertices;
	ClothWeldingMap.Empty();
	ClothWeldingDomain = 0;

	// Now we find all the verts that are part of the cloth (ie weighted at all to cloth bones)
	// and add them to the ClothToGraphicsVertMap.
	INT VertIndex = 0;
	for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);

		for(INT i=0; i<Chunk.RigidVertices.Num(); i++)
		{
			FRigidSkinVertex& RV = Chunk.RigidVertices(i);
			if( ClothBoneIndices.ContainsItem(Chunk.BoneMap(RV.Bone)) )
			{
				ClothToGraphicsVertMap.AddItem(VertIndex);
				INT NewIndex = AllClothVertices.AddUniqueItem(RV.Position);
				ClothWeldingDomain = Max(ClothWeldingDomain, NewIndex+1);
				ClothWeldingMap.AddItem(NewIndex);
			}

			VertIndex++;
		}

		for(INT i=0; i<Chunk.SoftVertices.Num(); i++)
		{
			FSoftSkinVertex& SV = Chunk.SoftVertices(i);
			if( (SV.InfluenceWeights[0] > 0 && ClothBoneIndices.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[0]))) ||
				(SV.InfluenceWeights[1] > 0 && ClothBoneIndices.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[1]))) ||
				(SV.InfluenceWeights[2] > 0 && ClothBoneIndices.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[2]))) ||
				(SV.InfluenceWeights[3] > 0 && ClothBoneIndices.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[3]))) )
			{
				ClothToGraphicsVertMap.AddItem(VertIndex);
				INT NewIndex = AllClothVertices.AddUniqueItem(SV.Position);
				ClothWeldingDomain = Max(ClothWeldingDomain, NewIndex+1);
				ClothWeldingMap.AddItem(NewIndex);
			}

			VertIndex++;
		}
	}

	// This is the divider between the 'free' vertices, and those that will be fixed.
	NumFreeClothVerts = ClothToGraphicsVertMap.Num();

	// Bail out if no cloth verts found.
	if(NumFreeClothVerts == 0)
	{
		return;
	}

	// The vertex buffer will have all the cloth verts, and then all the fixed verts.

	// Iterate over triangles, finding connected non-cloth verts
	for(INT i=0; i<LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Num(); i+=3)
	{
		DWORD Index0 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+0);
		DWORD Index1 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+1);
		DWORD Index2 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+2);

		// Its a 'free' vert if its in the array, and before NumFreeClothVerts.
		INT Index0InClothVerts = ClothToGraphicsVertMap.FindItemIndex(Index0);
		UBOOL Index0IsCloth = (Index0InClothVerts != INDEX_NONE) && (Index0InClothVerts < NumFreeClothVerts);

		INT Index1InClothVerts = ClothToGraphicsVertMap.FindItemIndex(Index1);
		UBOOL Index1IsCloth = (Index1InClothVerts != INDEX_NONE) && (Index1InClothVerts < NumFreeClothVerts);

		INT Index2InClothVerts = ClothToGraphicsVertMap.FindItemIndex(Index2);
		UBOOL Index2IsCloth = (Index2InClothVerts != INDEX_NONE) && (Index2InClothVerts < NumFreeClothVerts);

		// If this is a triangle that should be part of the cloth mesh (at least one vert is 'free'), 
		// add it to the cloth index buffer.
		if( Index0IsCloth || Index1IsCloth || Index2IsCloth )
		{
			// If this vert is not free ...
			if(!Index0IsCloth)
			{
				// Add to ClothToGraphicsVertMap (if not already present), and then to index buffer.
				const INT VIndex = ClothToGraphicsVertMap.AddUniqueItem(Index0);		
				ClothIndexBuffer.AddItem(VIndex);

				if (ClothToGraphicsVertMap.Num() > ClothWeldingMap.Num())
				{
					ClothWeldingMap.AddItem(ClothWeldingDomain++);
				}
			}
			// If this vert is part of the cloth, we have its location in the overall vertex buffer
			else
			{
				ClothIndexBuffer.AddItem(Index0InClothVerts);
			}

			// We do vertex 2 now, to change the winding order (Novodex likes it this way).
			if(!Index2IsCloth)
			{
				INT VertIndex = ClothToGraphicsVertMap.AddUniqueItem(Index2);		
				ClothIndexBuffer.AddItem(VertIndex);

				if (ClothToGraphicsVertMap.Num() > ClothWeldingMap.Num())
				{
					ClothWeldingMap.AddItem(ClothWeldingDomain++);
				}

			}
			else
			{
				ClothIndexBuffer.AddItem(Index2InClothVerts);
			}

			// Repeat for vertex 1
			if(!Index1IsCloth)
			{
				INT VertIndex = ClothToGraphicsVertMap.AddUniqueItem(Index1);		
				ClothIndexBuffer.AddItem(VertIndex);

				if (ClothToGraphicsVertMap.Num() > ClothWeldingMap.Num())
				{
					ClothWeldingMap.AddItem(ClothWeldingDomain++);
				}
			}
			else
			{
				ClothIndexBuffer.AddItem(Index1InClothVerts);
			}
		}
	}

	// Now we update the 'dist to fixed vert' info, based on whatever method is chosen
	GenerateClothMovementScale();

	// Check Welding map, if it's the identity then we don't need it at all
	// If it's not the identity then it means that the domain is bigger than range and thus the last value can't be the size of the domain
	if (bForceNoWelding || (ClothWeldingMap(ClothWeldingMap.Num()-1) == ClothWeldingMap.Num() - 1))
	{
		if (!bForceNoWelding)
		{
			check(ClothWeldingDomain == ClothWeldingMap.Num());
		}
		ClothWeldingMap.Empty();
		ClothWeldedIndices.Empty();
	}

	if (ClothWeldingMap.Num() > 0)
	{
		ClothWeldedIndices = ClothIndexBuffer;
		for (INT i = 0; i < ClothWeldedIndices.Num(); i++)
		{
			check(ClothWeldedIndices(i) < ClothWeldingMap.Num());
			ClothWeldedIndices(i) = ClothWeldingMap(ClothWeldedIndices(i));
		}
	}

	//Build tables of cloth vertices which are to be attached to bodies associated with the PhysicsAsset

	for(INT i=0; i<ClothSpecialBones.Num(); i++)
	{
		ClothSpecialBones(i).AttachedVertexIndices.Empty();

		INT BoneIndex = MatchRefBone(ClothSpecialBones(i).BoneName);
		if(BoneIndex != INDEX_NONE)
		{
			check(BoneIndex < 255);

			INT VertIndex = 0;
			for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
			{
				FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);

				for(INT j=0; j<Chunk.GetNumRigidVertices(); j++)
				{
					FRigidSkinVertex& RV = Chunk.RigidVertices(j);

					if(Chunk.BoneMap(RV.Bone) == BoneIndex)
					{
						//Map the graphics index to the unwelded cloth index.
						INT ClothVertexIndex = ClothToGraphicsVertMap.FindItemIndex(VertIndex);
						
						if(ClothVertexIndex != INDEX_NONE)
						{
							//We want the welded vertex index...
							if (ClothWeldingMap.Num() > 0)
							{
								ClothVertexIndex = ClothWeldingMap(ClothVertexIndex);
							}
							
							ClothSpecialBones(i).AttachedVertexIndices.AddItem(ClothVertexIndex);
						}
						//else the vertex is not in the cloth.
					}


					VertIndex++; 
				}

				//Only allow attachment to rigid skinned vertices.
				VertIndex += Chunk.GetNumSoftVertices();
			}
		}
	}
}

/* Build a mapping from a set of 3 indices(a triangle) to the location in the index buffer */
void USkeletalMesh::BuildClothTornTriMap()
{
	if( (!bEnableClothTearing && !bEnableValidBounds) || (ClothTornTriMap.Num() != 0) || (ClothWeldingMap.Num() != 0) )
	{
		return;
	}
	
	if( NumFreeClothVerts == 0 )
	{
		return;
	}

	FStaticLODModel* LODModel = &LODModels(0);

	GraphicsIndexIsCloth.Empty(LODModel->NumVertices);
	GraphicsIndexIsCloth.Add(LODModel->NumVertices);

	for(INT i=0; i<LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Num(); i+=3)
	{
		DWORD Index0 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+0);
		DWORD Index1 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+1);
		DWORD Index2 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+2);

		// Its a 'free' vert if its in the array, and before NumFreeClothVerts.
		INT Index0InClothVerts = ClothToGraphicsVertMap.FindItemIndex(Index0);
		UBOOL Index0IsCloth = (Index0InClothVerts != INDEX_NONE) && (Index0InClothVerts < NumFreeClothVerts);

		INT Index1InClothVerts = ClothToGraphicsVertMap.FindItemIndex(Index1);
		UBOOL Index1IsCloth = (Index1InClothVerts != INDEX_NONE) && (Index1InClothVerts < NumFreeClothVerts);

		INT Index2InClothVerts = ClothToGraphicsVertMap.FindItemIndex(Index2);
		UBOOL Index2IsCloth = (Index2InClothVerts != INDEX_NONE) && (Index2InClothVerts < NumFreeClothVerts);

		// save if vertices are cloth or not for rendering (FSkeletalMeshObjectCPUSkin::CacheVertices)
		GraphicsIndexIsCloth(Index0) = Index0IsCloth;
		GraphicsIndexIsCloth(Index1) = Index1IsCloth;
		GraphicsIndexIsCloth(Index2) = Index2IsCloth;

		if(Index0IsCloth || Index1IsCloth || Index2IsCloth)
		{
			check(Index0InClothVerts < 0xffFF);
			check(Index1InClothVerts < 0xffFF);
			check(Index2InClothVerts < 0xffFF);

			QWORD i0 = Index0InClothVerts;
			QWORD i1 = Index1InClothVerts;
			QWORD i2 = Index2InClothVerts;

			QWORD PackedTri = i0 + (i1 << 16) + (i2 << 32);

			//check(!ClothTornTriMap.HasKey(PackedTri));
			ClothTornTriMap.Set(PackedTri, i);
		}
	}

}

UBOOL USkeletalMesh::IsOnlyClothMesh() const
{
	const FStaticLODModel* LODModel = &LODModels(0);
	if(LODModel == NULL)
		return FALSE;

	//Cache this?
	INT VertexCount = 0;
	for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
	{
		const FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);

		VertexCount += Chunk.GetNumRigidVertices();
		VertexCount += Chunk.GetNumSoftVertices();
	}

	if(VertexCount == NumFreeClothVerts)
		return TRUE;
	else
		return FALSE;
}

#if WITH_NOVODEX && !NX_DISABLE_CLOTH

/**
 * We must factor out the retrieval of the cloth positions so that we can use it to initialize the 
 * cloth buffers on cloth creation. Sadly we cant just use saveToDesc() on the cloth mesh because
 * it returns permuted cloth vertices.
*/
UBOOL USkeletalMesh::ComputeClothSectionVertices(TArray<FVector>& ClothSectionVerts, FLOAT InScale, UBOOL ForceNoWelding)
{
		// Build vertex buffer with _all_ verts in skeletal mesh.
	FStaticLODModel* LODModel = &LODModels(0);
	TArray<FVector>	ClothVerts;

	for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);

		if(Chunk.GetNumRigidVertices() > 0)
		{
			for(INT VertIdx=0; VertIdx<Chunk.GetNumRigidVertices(); VertIdx++)
			{
				FGPUSkinVertexBase* SrcRigidVertex = LODModel->VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()+VertIdx);
				ClothVerts.AddItem( LODModel->VertexBufferGPUSkin.GetVertexPosition(SrcRigidVertex)* InScale * U2PScale );
			}
		}

		if(Chunk.GetNumSoftVertices() > 0)
		{
			for(INT VertIdx=0; VertIdx<Chunk.GetNumSoftVertices(); VertIdx++)
			{
				ClothVerts.AddItem( LODModel->VertexBufferGPUSkin.GetVertexPosition(Chunk.GetSoftVertexBufferIndex()+VertIdx)* InScale * U2PScale );
			}
		}
	}

	if (ClothVerts.Num() == 0)
	{
		return FALSE;
	}

	// Build initial vertex buffer for cloth section - pulling in just the verts we want.

	if (ClothWeldingMap.Num() == 0 || ForceNoWelding)
	{
		if(ClothSectionVerts.Num() < ClothToGraphicsVertMap.Num())
		{
			ClothSectionVerts.Empty();
			ClothSectionVerts.AddZeroed(ClothToGraphicsVertMap.Num());
		}

		for(INT i=0; i<ClothToGraphicsVertMap.Num(); i++)
		{
			ClothSectionVerts(i) = ClothVerts(ClothToGraphicsVertMap(i));
		}
	}
	else
	{
		if(ClothSectionVerts.Num() < ClothWeldingMap.Num())
		{
			ClothSectionVerts.Empty();
			ClothSectionVerts.AddZeroed(ClothWeldingDomain);
		}

		for(INT i=0; i<ClothWeldingMap.Num(); i++)
		{
			INT Mapped = ClothWeldingMap(i);
			ClothSectionVerts(Mapped) = ClothVerts(ClothToGraphicsVertMap(i));
		}
	}

	return TRUE;
}

/** Get the cooked NxClothMesh for this mesh at the given scale. */
NxClothMesh* USkeletalMesh::GetClothMeshForScale(FLOAT InScale)
{
#if WITH_PHYSX_COOKING
	check(ClothMesh.Num() == ClothMeshScale.Num());

	// Look to see if we already have this mesh at this scale.
	for(INT i=0; i<ClothMesh.Num(); i++)
	{
		if( Abs(InScale - ClothMeshScale(i)) < KINDA_SMALL_NUMBER )
		{
			return (NxClothMesh*)ClothMesh(i);
		}
	}

	// If we have no info about cloth mesh - we can't do anything.
	// This would be generated using BuildClothMapping.
	if(ClothToGraphicsVertMap.Num() == 0)
	{
		return NULL;
	}

	TArray<FVector> ClothSectionVerts;

	if(!ComputeClothSectionVertices(ClothSectionVerts, InScale))
	{
		return NULL;
	}

	// Fill in cloth description
	NxClothMeshDesc Desc;
	Desc.points = ClothSectionVerts.GetData();
	Desc.numVertices = ClothSectionVerts.Num();
	Desc.pointStrideBytes = sizeof(FVector);

	if (ClothWeldingMap.Num() == 0)
	{
		Desc.triangles = ClothIndexBuffer.GetData();
		Desc.numTriangles = ClothIndexBuffer.Num()/3;
		Desc.triangleStrideBytes = 3*sizeof(INT);
	}
	else
	{
		check(ClothWeldedIndices.Num() > 0);
		Desc.triangles = ClothWeldedIndices.GetData();
		Desc.numTriangles = ClothWeldedIndices.Num()/3;
		Desc.triangleStrideBytes = 3*sizeof(INT);
	}
	
	//Create a vert flag array to mark tearable verts
	TArray<INT> ClothVertFlags;

	if(bEnableClothTearing && (ClothWeldingMap.Num() == 0))
	{	
		Desc.flags |= NX_CLOTH_MESH_TEARABLE;

		bool bTearableBonesFound = false;

		//Size to the number of verts
		ClothVertFlags.AddZeroed( ClothSectionVerts.Num() );

		for(INT i=0; i<ClothSpecialBones.Num(); i++)
		{
			if( ClothSpecialBones(i).BoneType == CLOTHBONE_TearLine )
			{
				//We have found bones marked as tearlines, if they are empty, no verts will tear
				bTearableBonesFound = true;

				//Spin through the verts and mark them as tearable
				for(INT j=0; j<ClothSpecialBones(i).AttachedVertexIndices.Num(); j++)
				{
					INT VertexIndex = ClothSpecialBones(i).AttachedVertexIndices(j);
					ClothVertFlags(VertexIndex) = NX_CLOTH_VERTEX_TEARABLE;
				}
			}
		}
		if( bTearableBonesFound )
		{
			Desc.vertexFlagStrideBytes = sizeof(NxU32);
			Desc.vertexFlags = ClothVertFlags.GetData();
		}
	}

	// Set number if simpler meshes to generate for sim
	Desc.numHierarchyLevels = Max(ClothHierarchyLevels, 0);

	// Cook mesh
	TArray<BYTE> TempData;
	FNxMemoryBuffer Buffer(&TempData);
	GNovodexCooking->NxCookClothMesh(Desc, Buffer);
	NxClothMesh* NewClothMesh = GNovodexSDK->createClothMesh(Buffer);

	ClothMesh.AddItem( NewClothMesh );
	ClothMeshScale.AddItem( InScale );

	//Make sure we have built the data needed for tearing.
	//Could do this during editing if we had proper serialization for TMaps(ie in BuildClothMapping().

	BuildClothTornTriMap();

	return NewClothMesh;
#else
	return NULL;
#endif
}
#endif

/** Reset the store of cooked cloth meshes. Need to make sure you are not actually using any when you call this. */
void USkeletalMesh::ClearClothMeshCache()
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	for (INT i = 0; i < ClothMesh.Num(); i++)
	{
		NxClothMesh* CM = (NxClothMesh*)ClothMesh(i);
		check(CM);
		GNovodexPendingKillClothMesh.AddItem(CM);
	}
	ClothMesh.Empty();
	ClothMeshScale.Empty();

	ClothTornTriMap.Empty();

#endif // WITH_NOVODEX
}


////// SKELETAL MESH THUMBNAIL SUPPORT ////////

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString USkeletalMesh::GetDesc()
{
	check(LODModels.Num() > 0);
	return FString::Printf( TEXT("%d Triangles, %d Bones"), LODModels(0).GetTotalFaces(), RefSkeleton.Num() );
}

/** 
 * Returns detailed info to populate listview columns
 */
FString USkeletalMesh::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "%d Triangles" ), LODModels(0).GetTotalFaces() );
		break;
	case 1: 
		Description = FString::Printf( TEXT( "%d Bones" ), RefSkeleton.Num() );
		break;
	}
	return( Description );
}


