/*=============================================================================
	SoftBodyEditor.cpp: SoftBody support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"

#if WIN32 && WITH_NOVODEX && !NX_DISABLE_SOFTBODY

#pragma pack(push,8)
#include "NxTetra.h"
#pragma pack(pop)

#ifdef _DEBUG
	#pragma comment(lib, "NxTetraDebug.lib")
#else
	#pragma comment(lib, "NxTetra.lib")
#endif

UBOOL ComputeSoftBodySectionVertices(FStaticLODModel* InLODModel, TArray<INT>& InSurfaceToGraphicsVertMap, TArray<FVector>& OutSoftBodySectionVerts, FLOAT InScale);



FLOAT SignedTetraVolume(const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3)
{
	FVector D1 = (V1-V0);
	FVector D2 = (V2-V0);
	FVector D3 = (V3-V0);
	return (D1 ^ D2) | D3;
}

UBOOL ComputeSoftBodyTetras(TArray<FVector>&			OutTetraVerts,
							TArray<INT>&				OutTetraIndices, 
							const TArray<FVector>	InSurfaceVerts, 
							const TArray<INT>		InSurfaceIndices,
							FLOAT					DetailLevel, 
							INT						SubdivisionLevel, 
							UBOOL					bGenIsoSurface)
{
	NxTetraInterface *TetraInterface = getTetraInterface();
	if(!TetraInterface)
	{
		return FALSE;
	}

	if(DetailLevel < 0.0f || DetailLevel > 1.0f)
	{
		debugf(TEXT("Error: SoftBodyDetailLevel must be within range [0.0, 1.0]."));
		DetailLevel = Clamp(DetailLevel, 0.0f, 1.0f);
	}
	if(SubdivisionLevel < 1)
	{
		debugf(TEXT("Error: SoftBodySubdivisionLevel must be bigger than zero."));
		SubdivisionLevel = 1;
	}

	OutTetraIndices.Empty();
	OutTetraVerts.Empty();

	TArray<FLOAT> SurfaceVerts;
	SurfaceVerts.Add(InSurfaceVerts.Num() * 3);

	for(INT i=0; i<InSurfaceVerts.Num(); i++)
	{
		FVector Vec = InSurfaceVerts(i) * U2PScale;
		
		SurfaceVerts(i*3 + 0) = Vec.X;
		SurfaceVerts(i*3 + 1) = Vec.Y;
		SurfaceVerts(i*3 + 2) = Vec.Z;
	}

	TArray<UINT> SurfaceIndices;
	SurfaceIndices.Add(InSurfaceIndices.Num());

	for(INT i=0; i<InSurfaceIndices.Num(); i++)
	{
		SurfaceIndices(i) = (UINT)InSurfaceIndices(i);
	}

	TetraInterface->setSubdivisionLevel(SubdivisionLevel);

	NxTetraMesh TriMesh;
	TetraInterface->createTetraMesh(TriMesh, 
									SurfaceVerts.Num() / 3, 
									SurfaceVerts.GetTypedData(), 
									SurfaceIndices.Num() / 3, 
									SurfaceIndices.GetTypedData(), 
									false);
	
	NxTetraMesh Surface;

	if(bGenIsoSurface)
	{
		UBOOL bIsoSingle = FALSE;	// Set to true for a single surface - need to expose
		TetraInterface->createIsoSurface(TriMesh, Surface, bIsoSingle == TRUE );
		TetraInterface->releaseTetraMesh( TriMesh );
		TriMesh = Surface;
	}

	if( DetailLevel < 1.0f )
	{
		INT SimpleSubdivision = Max( (INT)(DetailLevel*SubdivisionLevel), 1 );
		TetraInterface->setSubdivisionLevel(SimpleSubdivision);

		INT LastTriangleCount = 0;
		INT NextTriangleCount = 0;

		do
		{
			LastTriangleCount = TriMesh.mTcount;

			// Check for the degenerate case that the tetraheadral mesh was actually reduced to zero triangles
			if(0 == LastTriangleCount)
			{
				// At this point, the tetrahedral mesh was reduced to zero triangles, which means 
				// it can't be reduced anymore. So, we have to force ourselves out of the loop.
				NextTriangleCount = 0;
			}
			else
			{
				// We can continue simplifying the mesh
				NextTriangleCount = TetraInterface->simplifySurface(0.5f, TriMesh, TriMesh);
			}
		} while( NextTriangleCount < LastTriangleCount );

		TetraInterface->setSubdivisionLevel(SubdivisionLevel);
	}

	// In the event that a mesh was reduced to zero triangles, we can't continue because there is no mesh!
	if(0 == TriMesh.mTcount)
	{
		TetraInterface->releaseTetraMesh( TriMesh );
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_SoftBodyMeshSimplifiedTooMuch") );
		return FALSE;
	}

	NxTetraMesh TetraMesh;
	TetraInterface->createTetraMesh(TriMesh, TetraMesh);
	
	TetraInterface->releaseTetraMesh( TriMesh );

	check(TetraMesh.mIsTetra);

	OutTetraVerts.Add(TetraMesh.mVcount);
	for(INT i=0; i<OutTetraVerts.Num(); i++)
	{
		FVector Vec;
		Vec.X = TetraMesh.mVertices[i*3 + 0];
		Vec.Y = TetraMesh.mVertices[i*3 + 1];
		Vec.Z = TetraMesh.mVertices[i*3 + 2];

		OutTetraVerts(i) = Vec * P2UScale;
	}

	for(UINT i=0; i<TetraMesh.mTcount; i++)
	{
		UINT Idx0 = TetraMesh.mIndices[i*4+0];
		UINT Idx1 = TetraMesh.mIndices[i*4+1];
		UINT Idx2 = TetraMesh.mIndices[i*4+2];
		UINT Idx3 = TetraMesh.mIndices[i*4+3];
		const FVector& V0 = OutTetraVerts(Idx0);
		const FVector& V1 = OutTetraVerts(Idx1);
		const FVector& V2 = OutTetraVerts(Idx2);
		const FVector& V3 = OutTetraVerts(Idx3);

		// On bad input meshes, tetrahedra with an illegal winding order might be generated.
		// These tetras are removed here to avoid a crash when being used for simulation.
		// TODO: we could also remove degenerate tetras from the output as well.
		if(SignedTetraVolume(V0, V1, V2, V3) > 0.0f)
		{
			OutTetraIndices.AddItem(Idx0);
			OutTetraIndices.AddItem(Idx1);
			OutTetraIndices.AddItem(Idx2);
			OutTetraIndices.AddItem(Idx3);
		}
	}

	TetraInterface->releaseTetraMesh( TetraMesh );

	return TRUE;
}

static inline FVector ComputeBarycentric(const FVector& Pt, 
	const FVector& TetPt0, const FVector& TetPt1, const FVector& TetPt2, const FVector& TetPt3)
{
	FVector Q = Pt - TetPt3;
	FVector Q0 = TetPt0 - TetPt3;
	FVector Q1 = TetPt1 - TetPt3;
	FVector Q2 = TetPt2 - TetPt3;

	FMatrix M = FMatrix(Q0, Q1, Q2, FVector(0.0f, 0.0f, 1.0f));

	FLOAT Det= M.Determinant();

	FVector B;

	M.SetAxis(0, Q);
	B.X = M.Determinant();

	M.SetAxis(0, Q0); M.SetAxis(1, Q);
	B.Y = M.Determinant();

	M.SetAxis(1, Q1); M.SetAxis(2, Q);
	B.Z = M.Determinant();

	if(Det != 0.0f)
	{
		B /= Det;
	}

	return B;
}

void BuildSoftBodyLinks(const TArray<FVector>& MeshVerts,const TArray<FVector>& TetraVerts, 
									   const TArray<INT>& Tetra, TArray<FSoftBodyTetraLink>& TetraLinks)
{
	if(Tetra.Num() == 0)
	{
		return;
	}
	
	TetraLinks.Empty();

	check( (Tetra.Num() % 4) == 0 );

	for(INT i=0; i<MeshVerts.Num(); i++)
	{
		FVector Pt = MeshVerts(i);

		FSoftBodyTetraLink TetraLink;
		FLOAT MinDist = BIG_NUMBER;
		//Find Tetra for this Pt

		for(INT j=0; j<Tetra.Num(); j+=4)
		{
			INT Idx[4];

			Idx[0] = Tetra(j + 0);
			Idx[1] = Tetra(j + 1);
			Idx[2] = Tetra(j + 2);
			Idx[3] = Tetra(j + 3);

			FVector TetPt[4];
			TetPt[0] = TetraVerts(Idx[0]);
			TetPt[1] = TetraVerts(Idx[1]);
			TetPt[2] = TetraVerts(Idx[2]);
			TetPt[3] = TetraVerts(Idx[3]);


			FVector B = ComputeBarycentric(Pt, TetPt[0], TetPt[1], TetPt[2], TetPt[3]);

			if((B.X >= 0.0f) && (B.Y >= 0.0f) && (B.Z >= 0.0f) && (B.X + B.Y + B.Z) <= 1.0f)
			{

				TetraLink.Bary = B;
				TetraLink.TetIndex = j;
				break;
			}

			FLOAT Dist = 0.0f;

			if((B.X + B.Y + B.Z) > 1.0f) Dist = B.X + B.Y + B.Z - 1.0f;
			if(B.X < 0.0f) Dist = (-B.X < Dist) ? Dist : -B.X;
			if(B.Y < 0.0f) Dist = (-B.Y < Dist) ? Dist : -B.Y;
			if(B.Z < 0.0f) Dist = (-B.Z < Dist) ? Dist : -B.Z;

			if(Dist < MinDist)
			{
				MinDist = Dist;
				TetraLink.Bary = B;
				TetraLink.TetIndex = j;
			}

		}

		TetraLinks.AddItem(TetraLink);
	}
}

UBOOL BuildSoftBodyMapping(USkeletalMesh &Mesh)
{
	FStaticLODModel* LODModel = &Mesh.LODModels(0);
	FSoftBodyMeshInfo SoftBodyInfo;

	// Make array of indices of bones whose verts are to be considered 'cloth'
	TArray<BYTE> SoftBodyBoneIndices;
	for(INT i=0; i<Mesh.SoftBodyBones.Num(); i++)
	{
		INT BoneIndex = Mesh.MatchRefBone(Mesh.SoftBodyBones(i));
		if(BoneIndex != INDEX_NONE)
		{
			check(BoneIndex < 255);
			SoftBodyBoneIndices.AddItem( (BYTE)BoneIndex );
		}
	}

	// Add 'special' softbody bones. eg bones which are fixed to the physics asset
	for(INT i=0; i<Mesh.SoftBodySpecialBones.Num(); i++)
	{
		INT BoneIndex = Mesh.MatchRefBone(Mesh.SoftBodySpecialBones(i).BoneName);
		if(BoneIndex != INDEX_NONE)
		{
			check(BoneIndex < 255);
			SoftBodyBoneIndices.AddItem( (BYTE)BoneIndex );
		}
	}

	// Fail if no bones defined.
	if( SoftBodyBoneIndices.Num() == 0)
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_SoftBodyNoBonesDefined") );
		return FALSE;
	}

	// Now we find all the verts that are part of the SoftBody (ie weighted at all to SoftBody bones)
	// and add them to the SoftBodySurfaceToGraphicsVertMap.
	INT VertIndex = 0;
	for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);

		for(INT i=0; i<Chunk.RigidVertices.Num(); i++)
		{
			FRigidSkinVertex& RV = Chunk.RigidVertices(i);
			if( SoftBodyBoneIndices.ContainsItem(Chunk.BoneMap(RV.Bone)) )
			{
				SoftBodyInfo.SurfaceToGraphicsVertMap.AddItem(VertIndex);
			}

			VertIndex++;
		}

		for(INT i=0; i<Chunk.SoftVertices.Num(); i++)
		{
			FSoftSkinVertex& SV = Chunk.SoftVertices(i);
			if( (SV.InfluenceWeights[0] > 0 && SoftBodyBoneIndices.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[0]))) ||
				(SV.InfluenceWeights[1] > 0 && SoftBodyBoneIndices.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[1]))) ||
				(SV.InfluenceWeights[2] > 0 && SoftBodyBoneIndices.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[2]))) ||
				(SV.InfluenceWeights[3] > 0 && SoftBodyBoneIndices.ContainsItem(Chunk.BoneMap(SV.InfluenceBones[3]))) )
			{
				SoftBodyInfo.SurfaceToGraphicsVertMap.AddItem(VertIndex);
			}

			VertIndex++;
		}
	}

	INT NumSoftVerts = SoftBodyInfo.SurfaceToGraphicsVertMap.Num();

	// Iterate over triangles, finding connected non-SoftBody verts
	for(INT i=0; i<LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Num(); i+=3)
	{
		DWORD Index0 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+0);
		DWORD Index1 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+1);
		DWORD Index2 = LODModel->MultiSizeIndexContainer.GetIndexBuffer()->Get(i+2);

		// Its a 'free' vert if its in the array, and before NumSoftVerts.
		INT Index0InSoftVerts = SoftBodyInfo.SurfaceToGraphicsVertMap.FindItemIndex(Index0);
		UBOOL Index0IsSoft = (Index0InSoftVerts != INDEX_NONE) && (Index0InSoftVerts < NumSoftVerts);
		
		INT Index1InSoftVerts = SoftBodyInfo.SurfaceToGraphicsVertMap.FindItemIndex(Index1);
		UBOOL Index1IsSoft = (Index1InSoftVerts != INDEX_NONE) && (Index1InSoftVerts < NumSoftVerts);
		
		INT Index2InSoftVerts = SoftBodyInfo.SurfaceToGraphicsVertMap.FindItemIndex(Index2);
		UBOOL Index2IsSoft = (Index2InSoftVerts != INDEX_NONE) && (Index2InSoftVerts < NumSoftVerts);
		

		// If this is a triangle that should be part of the SoftBody mesh (at least one vert is 'free'), 
		// add it to the SoftBody index buffer.
		
		if( Index0IsSoft || Index1IsSoft || Index2IsSoft )
		{
			// If this vert is not free ...
			if(!Index0IsSoft)
			{
				// Add to SoftBodySurfaceToGraphicsVertMap (if not already present), and then to index buffer.
				const INT VIndex = SoftBodyInfo.SurfaceToGraphicsVertMap.AddUniqueItem(Index0);		
				SoftBodyInfo.SurfaceIndices.AddItem(VIndex);
			}
			// If this vert is part of the SoftBody, we have its location in the overall vertex buffer
			else
			{
				SoftBodyInfo.SurfaceIndices.AddItem(Index0InSoftVerts);
			}
		
			// We do vertex 2 now, to change the winding order (Novodex likes it this way).
			if(!Index2IsSoft)
			{
				INT VertIndex = SoftBodyInfo.SurfaceToGraphicsVertMap.AddUniqueItem(Index2);		
				SoftBodyInfo.SurfaceIndices.AddItem(VertIndex);
			}
			else
			{
				SoftBodyInfo.SurfaceIndices.AddItem(Index2InSoftVerts);
			}
		
			// Repeat for vertex 1
			if(!Index1IsSoft)
			{
				INT VertIndex = SoftBodyInfo.SurfaceToGraphicsVertMap.AddUniqueItem(Index1);		
				SoftBodyInfo.SurfaceIndices.AddItem(VertIndex);
			}
			else
			{
				SoftBodyInfo.SurfaceIndices.AddItem(Index1InSoftVerts);
			}
		}
		
	}

	//Build tables of softbody vertices which are to be attached to bodies associated with the PhysicsAsset
	SoftBodyInfo.SpecialBoneAttachedVertexIndicies.AddZeroed(Mesh.SoftBodySpecialBones.Num());

	for(INT i=0; i<Mesh.SoftBodySpecialBones.Num(); i++)
	{
		INT BoneIndex = Mesh.MatchRefBone(Mesh.SoftBodySpecialBones(i).BoneName);
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
						//Map the graphics index to the unwelded softbody index.
						
						INT SoftBodyVertexIndex = SoftBodyInfo.SurfaceToGraphicsVertMap.FindItemIndex(VertIndex);
												
						if(SoftBodyVertexIndex != INDEX_NONE)
						{
							SoftBodyInfo.SpecialBoneAttachedVertexIndicies(i).AddItem(SoftBodyVertexIndex);
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

	TArray<FVector> SoftBodySectionVerts;

	if(!ComputeSoftBodySectionVertices(LODModel, SoftBodyInfo.SurfaceToGraphicsVertMap, SoftBodySectionVerts, 1.0f))
	{
		debugf(TEXT("Cannot create tetra-mesh out of SkeletalMesh. No suitable vertices found."));
		return FALSE;
	}

	if(!ComputeSoftBodyTetras(	SoftBodyInfo.TetraVertsUnscaled, SoftBodyInfo.TetraIndices,
								SoftBodySectionVerts, SoftBodyInfo.SurfaceIndices,
								Mesh.SoftBodyDetailLevel, Mesh.SoftBodySubdivisionLevel, Mesh.bSoftBodyIsoSurface) )
	{
		debugf(TEXT("Cannot create tetra-mesh out of SkeletalMesh."));
		return FALSE;
	}

	BuildSoftBodyLinks(SoftBodySectionVerts, SoftBodyInfo.TetraVertsUnscaled, SoftBodyInfo.TetraIndices, SoftBodyInfo.TetraLinks);

	// The mesh info must be valid otherwise it may cause crashes later
	if(!SoftBodyInfo.IsValid())
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_SoftBodyMeshNotGenerated") );
		return FALSE;
	}

	// At this point, we have successfully created a new soft-body mesh for the given skeletal mesh.
	// Now, we must copy over the temporary mesh data that we accumulated to the given skeletal mesh.
	Mesh.RecreateSoftBody(SoftBodyInfo);

	return TRUE;
}


UBOOL ComputeSoftBodySectionVertices(FStaticLODModel* InLODModel, TArray<INT>& InSurfaceToGraphicsVertMap, TArray<FVector>& OutSoftBodySectionVerts, FLOAT InScale)
{
	if(InSurfaceToGraphicsVertMap.Num() == 0)
	{
		return FALSE;
	}
	
	// Build vertex buffer with _all_ verts in skeletal mesh.
	TArray<FVector>	SoftVerts;

	for(INT ChunkIndex = 0;ChunkIndex < InLODModel->Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = InLODModel->Chunks(ChunkIndex);

		if(Chunk.GetNumRigidVertices() > 0)
		{
			for(INT VertIdx=0; VertIdx<Chunk.GetNumRigidVertices(); VertIdx++)
			{
				SoftVerts.AddItem( InLODModel->VertexBufferGPUSkin.GetVertexPosition(Chunk.GetRigidVertexBufferIndex()+VertIdx) * InScale * U2PScale );
			}
		}

		if(Chunk.GetNumSoftVertices() > 0)
		{
			for(INT VertIdx=0; VertIdx<Chunk.GetNumSoftVertices(); VertIdx++)
			{
				SoftVerts.AddItem( InLODModel->VertexBufferGPUSkin.GetVertexPosition(Chunk.GetSoftVertexBufferIndex()+VertIdx) * InScale * U2PScale );
			}
		}
	}

	if (SoftVerts.Num() == 0)
	{
		return FALSE;
	}

	// Build initial vertex buffer for soft body section - pulling in just the verts we want.

	OutSoftBodySectionVerts.Empty();
	OutSoftBodySectionVerts.AddZeroed(InSurfaceToGraphicsVertMap.Num());

	for(INT i=0; i< InSurfaceToGraphicsVertMap.Num(); i++)
	{
		OutSoftBodySectionVerts(i) = SoftVerts(InSurfaceToGraphicsVertMap(i));
	}

	return TRUE;
}

#endif //WIN32 && WITH_NOVODEX && !NX_DISABLE_SOFTBODY
