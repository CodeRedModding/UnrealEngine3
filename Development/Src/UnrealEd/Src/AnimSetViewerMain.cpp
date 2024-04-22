/*=============================================================================
	AnimSetViewerMain.cpp: AnimSet viewer main
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	This code contains embedded portions of source code from dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0, Copyright ?2006-2007 University of Dublin, Trinity College, All Rights Reserved, which have been altered from their original version.

	The following terms apply to dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0:

	This software is provided 'as-is', without any express or implied warranty.  In no event will the author(s) be held liable for any damages arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
=============================================================================*/

#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"
#include "AnimSetViewer.h"
#include "MouseDeltaTracker.h"
#include "PropertyWindow.h"
#include "PropertyWindowManager.h"
#include "DlgGenericComboEntry.h"
#include "SocketManager.h"
#include "DlgAnimationCompression.h"
#include "AnimationUtils.h"
#include "UnSkelRenderPublic.h"
#include "UnSkeletalMesh.h"
#include "GPUSkinVertexFactory.h"
#include "UnSkeletalMeshSorting.h"
#include "SkelImport.h"

#if WITH_SIMPLYGON
#include "SkeletalMeshSimplificationWindow.h"
#endif // #if WITH_SIMPLYGON

#if WITH_MANAGED_CODE
#include "FileSystemNotificationShared.h"
#endif


IMPLEMENT_CLASS(UASVSkelComponent);

static const FColor ExtraMesh1WireColor = FColor(255,215,215,255);
static const FColor ExtraMesh2WireColor = FColor(215,255,215,255);
static const FColor ExtraMesh3WireColor = FColor(215,215,255,255);

static const FLOAT	AnimSetViewer_TranslateSpeed = 0.25f;
static const FLOAT	AnimSetViewer_RotateSpeed = 0.02f;
static const FLOAT	AnimSetViewer_LightRotSpeed = 40.0f;
static const FLOAT	AnimSetViewer_WindStrengthSpeed = 0.1f;
#if WITH_APEX
static const FLOAT  AnimSetViewer_WindVelocityBlendTimeSpeed = 0.001f;
#endif

/*
*  Hit proxy for game stats visualization
*  Routes the rendered item selected to the proper visualizer
*/
struct HVertInfluenceProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HVertInfluenceProxy, HHitProxy);

	/** LOD this vert is for */
	INT LODIdx;
	/** Influence being used */
	INT InfluenceIdx;
	/** Vert index corresponding to the sprite */
	INT VertIndex;
	/** Is it already part of the influence swap */
	UBOOL bContributingInfluence;

	HVertInfluenceProxy(INT InLODIdx, INT InInfluenceIdx, INT InVertIndex, UBOOL bInContributingInfluence):
	HHitProxy(HPP_UI),
		LODIdx(InLODIdx), InfluenceIdx(InInfluenceIdx), VertIndex(InVertIndex), bContributingInfluence(bInContributingInfluence) {}
};

/**
 * A hit proxy class for sockets.
 */
struct HASVSocketProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( HASVSocketProxy, HHitProxy );

	INT		SocketIndex;

	HASVSocketProxy(INT InSocketIndex)
		:	HHitProxy( HPP_UI )
		,	SocketIndex( InSocketIndex )
	{}
};

/**
* Triangle sort strip HitProxy, for selecting hair strands.
*/
struct HSortStripProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HSortStripProxy,HHitProxy);	
	class FASVSkelSceneProxy* SceneProxy;
	INT LODModelIndex;
	INT SectionIndex;
	INT StripIndex;
	HSortStripProxy( class FASVSkelSceneProxy* InSceneProxy, INT InLODModelIndex, INT InSectionIndex, INT InStripIndex )
		:	HHitProxy( HPP_UI )
		,	SceneProxy(InSceneProxy)
		,	LODModelIndex(InLODModelIndex)
		,	SectionIndex(InSectionIndex)
		,	StripIndex(InStripIndex)
	{}

	/**
	 * Method that specifies whether the hit proxy *always* allows translucent primitives to be associated with it or not,
	 * regardless of any other engine/editor setting. For example, if translucent selection was disabled, any hit proxies
	 * returning TRUE would still allow translucent selection. In this specific case, TRUE is always returned because we
	 * always need to be able to select translucent strips.
	 * 
	 * @return	TRUE if translucent primitives are always allowed with this hit proxy; FALSE otherwise
	 */
	virtual UBOOL AlwaysAllowsTranslucentPrimitives() const
	{
		return TRUE;
	}
};

/*
 *  Hit proxy for vertex info
 *  Select which vertex and will show the proper information
 */
struct HVertexInfoProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HVertexInfoProxy, HHitProxy);

	/** Vert index corresponding to the sprite */
	INT		VertexID;
	FVector VertexPosition;
	TArray<INT>	InfluencedIndices;
	TArray<FLOAT>	InfluencedWeights;
	
	HVertexInfoProxy(const INT InVertexID, const FVector& InVertexPosition):
	HHitProxy(HPP_UI),
	VertexID(InVertexID), VertexPosition(InVertexPosition) {}

	void AddInfluences(TArray<INT> Indices, TArray<FLOAT> Weights)
	{
		InfluencedIndices.Empty();
		InfluencedIndices.Append(Indices);
		InfluencedWeights.Empty();
		InfluencedWeights.Append(Weights);
	}
};

/** Draw the verts that are part of the cloth, colorised to show how far they can move from animated pose */
void DrawClothMovementDistanceScale(FPrimitiveDrawInterface* PDI, const USkeletalMeshComponent* SkelComp)
{
	if(!SkelComp || !SkelComp->SkeletalMesh)
	{
		return;
	}

	USkeletalMesh* SkelMesh = SkelComp->SkeletalMesh;

	// Check array is right size before we draw
	if(SkelMesh->ClothToGraphicsVertMap.Num() != SkelMesh->ClothMovementScale.Num())
	{
		return;
	}

	for(INT VertIdx=0; VertIdx<SkelMesh->ClothToGraphicsVertMap.Num(); VertIdx++)
	{
		UBOOL bFreeVert = (VertIdx < SkelMesh->NumFreeClothVerts);

		// Find the index of the graphics vertex that corresponds to this cloth vertex
		INT GraphicsIndex = SkelMesh->ClothToGraphicsVertMap(VertIdx);
		FVector SkinnedPos = SkelComp->GetSkinnedVertexPosition(GraphicsIndex);
		FVector WorldPos = SkelComp->LocalToWorld.TransformFVector(SkinnedPos); // Transform into world space

		FLinearColor RedColor(1,0,0);
		FLinearColor BlueColor(0,0,1);

		FLinearColor VertColor(1,1,1);
		if(bFreeVert)
		{
			FLOAT DistanceScale = SkelMesh->ClothMovementScale(VertIdx);
			VertColor = Lerp(BlueColor, RedColor, DistanceScale);
		}

		PDI->DrawPoint(WorldPos, VertColor, 5.0f, SDPG_World);
	}
}

/*
 *   Skin and draw as point sprites all given vertices in a skeletal mesh
 *  @param PDI - Draw interface
 *  @param SkelComp - Skeletal component to draw verts from
 *  @param BoneInfluenceMappings - Array of vertex indices to draw
 *  @param VertexColor - color to draw the sprites
 *  @param bContributingVerts - do these verts contribute to the influence 
 */
void DrawVertInfluenceLocations(FPrimitiveDrawInterface* PDI, const USkeletalMeshComponent* SkelComp, const TSet<INT>& BoneInfluenceVerts, const FLinearColor& VertexColor, UBOOL bContributingVerts)
{
	if (BoneInfluenceVerts.Num() > 0)
	{
		DWORD StatusRegister = VectorGetControlRegister();
		VectorSetControlRegister( StatusRegister | VECTOR_ROUND_TOWARD_ZERO );

		FMatrix LocalToWorldMat = SkelComp->LocalToWorld;

		//@TODO handle multiple LOD and influence tracks
		if (ensure (SkelComp->SkeletalMesh->LODModels.IsValidIndex( SkelComp->PredictedLODLevel ) ))
		{
			const INT LODIdx = SkelComp->PredictedLODLevel;
			const INT InfluenceIdx = 0;
			const FStaticLODModel& LODModel = SkelComp->SkeletalMesh->LODModels(LODIdx);
			const INT NumVerts = BoneInfluenceVerts.Num();

			//Calculate the current bone transforms
			TArray<FBoneAtom> RefToLocal;
			UpdateRefToLocalMatrices(RefToLocal, SkelComp, LODIdx);
			const FBoneAtom* RESTRICT ReferenceToLocal = RefToLocal.GetTypedData();

			//Find the delimiters that separate chunks in the vertex buffer
			TArray<INT> ChunkMarkers;
			for(INT ChunkIndex = 0;ChunkIndex < LODModel.Chunks.Num();ChunkIndex++)
			{
				const FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIndex);
				ChunkMarkers.AddItem(Chunk.BaseVertexIndex + Chunk.GetNumVertices());
			}

			MS_ALIGN(16) FLOAT Float128[4] GCC_ALIGN(4);
			FVector* TransformedPosition = (FVector*)Float128;

			INT CurrentChunk = 0;
			FVector WorldPos;
			const FGPUSkinVertexBase* SrcRigidVertex = NULL;

			UBOOL bUseInfluences = bContributingVerts && SkelComp->LODInfo(LODIdx).bAlwaysUseInstanceWeights && LODModel.VertexInfluences.Num() > 0 && LODModel.VertexInfluences(InfluenceIdx).Influences.Num() > 0;
			for( TSet<INT>::TConstIterator VertIter( BoneInfluenceVerts ); VertIter != NULL; ++VertIter )
			{
				//Get the bind pose position of the vertex
				const INT VertexBufferIndex = *VertIter;
				SrcRigidVertex = LODModel.VertexBufferGPUSkin.GetVertexPtr(VertexBufferIndex);
				const FVector PosePosition = LODModel.VertexBufferGPUSkin.GetVertexPosition(SrcRigidVertex);

				//Find the chunk that this vert belongs to (assumes vert list is sorted so we only ever increase chunk index)
				if (VertexBufferIndex >= ChunkMarkers(CurrentChunk))
				{
					while (VertexBufferIndex >= ChunkMarkers(CurrentChunk) && CurrentChunk < ChunkMarkers.Num() - 1)
					{
						CurrentChunk++;
					}
				}

				if (CurrentChunk < LODModel.Chunks.Num())
				{
					//Get the bone map relevant for this vertex
					const WORD* RESTRICT BoneMap = LODModel.Chunks(CurrentChunk).BoneMap.GetTypedData();

					//Get the bone influences from the original mesh or the swapped bone influences array
					const BYTE* RESTRICT BoneIndices;
					const BYTE* RESTRICT BoneWeights;
					if (bUseInfluences)
					{
						BoneIndices = LODModel.VertexInfluences(InfluenceIdx).Influences(VertexBufferIndex).Bones.InfluenceBones;
						BoneWeights = LODModel.VertexInfluences(InfluenceIdx).Influences(VertexBufferIndex).Weights.InfluenceWeights;
					}
					else
					{
						BoneIndices = SrcRigidVertex->InfluenceBones;
						BoneWeights = SrcRigidVertex->InfluenceWeights;
					}

					//Skin the vertex
					VectorRegister SrcPoints;
					VectorRegister DstPoints;
					SrcPoints = VectorLoadFloat3_W1( &PosePosition );

					VectorRegister Weights = VectorMultiply( VectorLoadByte4(BoneWeights), VECTOR_INV_255 );
					VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Unpack and VectorLoadByte4.
#if QUAT_SKINNING
					// Dual Quaternion part
					// Linearly blend DQs
					FBoneQuat DualQuat;
					DualQuat.SetBoneAtom(ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_0]]]);
					VectorRegister Weight0 = VectorReplicate( Weights, INFLUENCE_0 );
					VectorRegister BlendDQ0	= VectorMultiply( VectorLoad( &DualQuat.DQ1[0] ), Weight0 );
					VectorRegister BlendDQ1	= VectorMultiply( VectorLoad( &DualQuat.DQ2[0] ), Weight0 );
					FLOAT	Scale = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_0]]].GetScale();
					VectorRegister BlendScale = VectorMultiply( VectorLoadFloat1( &Scale ), Weight0 );

					// Save first DQ0 for testing shortest route for blending
					VectorRegister BaseQuat = BlendDQ0;
					DualQuat.SetBoneAtom(ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_1]]]);

					VectorRegister Weight1 = VectorReplicate( Weights, INFLUENCE_1 );

					VectorRegister DQ0 = VectorLoad( &DualQuat.DQ1[0] );
					VectorRegister DQ1 = VectorLoad( &DualQuat.DQ2[0] );

					// blend scale - need to be done before negate weight
					Scale = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_1]]].GetScale();
					BlendScale	= VectorMultiplyAdd( VectorLoadFloat1( &Scale ), Weight1, BlendScale );

					// If not shortest route, negate weight
					if ( VectorAnyGreaterThan(VectorZero(), VectorDot4( BaseQuat, DQ0 )) )
					{
						Weight1 = VectorNegate(Weight1);
					}

					BlendDQ0	= VectorMultiplyAdd( DQ0 , Weight1, BlendDQ0 );
					BlendDQ1	= VectorMultiplyAdd( DQ1 , Weight1, BlendDQ1 );

					DualQuat.SetBoneAtom(ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_2]]]);

					VectorRegister Weight2 = VectorReplicate( Weights, INFLUENCE_2 );

					DQ0 = VectorLoad( &DualQuat.DQ1[0] );
					DQ1 = VectorLoad( &DualQuat.DQ2[0] );

					Scale = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_2]]].GetScale();
					BlendScale	= VectorMultiplyAdd( VectorLoadFloat1( &Scale ), Weight2, BlendScale );

					// If not shortest route, negate weight
					if ( VectorAnyGreaterThan(VectorZero(), VectorDot4( BaseQuat, DQ0 )) )
					{
						Weight2 = VectorNegate(Weight2);
					}

					BlendDQ0	= VectorMultiplyAdd( DQ0 , Weight2, BlendDQ0 );
					BlendDQ1	= VectorMultiplyAdd( DQ1 , Weight2, BlendDQ1 );

					DualQuat.SetBoneAtom(ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_3]]]);
					VectorRegister Weight3 = VectorReplicate( Weights, INFLUENCE_3 );
					DQ0 = VectorLoad( &DualQuat.DQ1[0] );
					DQ1 = VectorLoad( &DualQuat.DQ2[0] );

					Scale = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_3]]].GetScale();
					BlendScale	= VectorMultiplyAdd( VectorLoadFloat1( &Scale ), Weight3, BlendScale );

					// If not shortest route, negate weight
					if ( VectorAnyGreaterThan(VectorZero(), VectorDot4( BaseQuat, DQ0 )) )
					{
						Weight3 = VectorNegate(Weight3);
					}

					BlendDQ0	= VectorMultiplyAdd( DQ0 , Weight3, BlendDQ0 );
					BlendDQ1	= VectorMultiplyAdd( DQ1 , Weight3, BlendDQ1 );

					// Scale the position
					SrcPoints = VectorSet_W1(VectorMultiply(SrcPoints, BlendScale));

					// Normalize
					VectorRegister RecipLen = VectorReciprocalLen(BlendDQ0);
					BlendDQ0 = VectorMultiply(BlendDQ0, RecipLen);
					BlendDQ1 = VectorMultiply(BlendDQ1, RecipLen);

					// Cache variables to transform
					VectorRegister BlendDQ0YZW = VectorSet_W0(VectorSwizzle(BlendDQ0, 1, 2, 3, 0)); 
					VectorRegister BlendDQ1YZW = VectorSet_W0(VectorSwizzle(BlendDQ1, 1, 2, 3, 0)); 
					VectorRegister BlendDQ0XXXX = VectorReplicate(BlendDQ0, 0);
					VectorRegister BlendDQ1XXXX = VectorReplicate(BlendDQ1, 0);

					/* 
					* Dual Quaternion - http://isg.cs.tcd.ie/projects/DualQuaternions/
					* Convert DQ to Matrix
					* This is faster in our case since it calculates DQ result once and reuse it for position/tangents
					* If you use transform, you need to transform at least 3 times - more expensive (a lot of cross products)
					* DQToMatrix does not work with Scale - so I'm saving this for reference in the future
					*/
					DstPoints = VectorMultiplyAdd(VECTOR_2222, VectorCross(BlendDQ0YZW, VectorMultiplyAdd(BlendDQ0XXXX, SrcPoints, VectorCross(BlendDQ0YZW, SrcPoints))), SrcPoints);
					VectorRegister Trans = VectorMultiplyAdd(BlendDQ0XXXX, BlendDQ1YZW, VectorAdd(VectorNegate(VectorMultiply(BlendDQ1XXXX, BlendDQ0YZW)), VectorCross(BlendDQ0YZW, BlendDQ1YZW)));
					DstPoints = VectorMultiplyAdd(VECTOR_2222, Trans, DstPoints);
#else
					//Bone 1
					const FMatrix BoneMatrix0 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_0]]].ToMatrix();
					VectorRegister Weight0 = VectorReplicate( Weights, INFLUENCE_0 );
					VectorRegister M00	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[0][0] ), Weight0 );
					VectorRegister M10	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[1][0] ), Weight0 );
					VectorRegister M20	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[2][0] ), Weight0 );
					VectorRegister M30	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[3][0] ), Weight0 );

					//Bone 2
					const FMatrix BoneMatrix1 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_1]]].ToMatrix();
					VectorRegister Weight1 = VectorReplicate( Weights, INFLUENCE_1 );
					M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[0][0] ), Weight1, M00 );
					M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[1][0] ), Weight1, M10 );
					M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[2][0] ), Weight1, M20 );
					M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[3][0] ), Weight1, M30 );

					//Bone 3
					const FMatrix BoneMatrix2 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_2]]].ToMatrix();
					VectorRegister Weight2 = VectorReplicate( Weights, INFLUENCE_2 );
					M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[0][0] ), Weight2, M00 );
					M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[1][0] ), Weight2, M10 );
					M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[2][0] ), Weight2, M20 );
					M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[3][0] ), Weight2, M30 );

					//Bone 4
					const FMatrix BoneMatrix3 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_3]]].ToMatrix();
					VectorRegister Weight3 = VectorReplicate( Weights, INFLUENCE_3 );
					M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[0][0] ), Weight3, M00 );
					M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[1][0] ), Weight3, M10 );
					M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[2][0] ), Weight3, M20 );
					M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[3][0] ), Weight3, M30 );

					//Transform the point
					VectorRegister N_xxxx = VectorReplicate( SrcPoints, 0 );
					VectorRegister N_yyyy = VectorReplicate( SrcPoints, 1 );
					VectorRegister N_zzzz = VectorReplicate( SrcPoints, 2 );
					DstPoints = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiplyAdd( N_zzzz, M20, M30 ) ) );
#endif
					// Write to 16-byte aligned memory:
					VectorStore( DstPoints, TransformedPosition );

					// Render the now skinned point sprite
					WorldPos = LocalToWorldMat.TransformFVector(*TransformedPosition);

					PDI->SetHitProxy(new HVertInfluenceProxy(LODIdx, InfluenceIdx, VertexBufferIndex, bContributingVerts));
					PDI->DrawPoint(WorldPos, VertexColor, 5.0f, SDPG_World);
					PDI->SetHitProxy(NULL);
				}
			}
		}
	}
}

/*
 *   Skin and draw as point sprites all vertices in a skeletal mesh	not found in the given sets
 *  @param PDI - Draw interface
 *  @param SkelComp - Skeletal component to draw verts from
 *  @param InfluenceVerts - Array of vertex indices not to draw
 *  @param NonInfluenceVerts - Array of vertex indices not to draw
 */
void DrawAllOtherVertexLocations(FPrimitiveDrawInterface* PDI, const USkeletalMeshComponent* SkelComp, const TSet<INT>& InfluenceVerts, const TSet<INT>& NonInfluenceVerts )
{
	TSet<INT> AllOtherVerts;

	if ( SkelComp->SkeletalMesh )
	{
		const INT LODIdx = ::Clamp(SkelComp->PredictedLODLevel, SkelComp->MinLodModel, SkelComp->SkeletalMesh->LODModels.Num()-1);
		const FStaticLODModel& LODModel = SkelComp->SkeletalMesh->LODModels(LODIdx);
		const INT NumVertices = LODModel.VertexBufferGPUSkin.GetNumVertices();
		for (INT VertIdx=0; VertIdx<NumVertices; VertIdx++)
		{
			if (InfluenceVerts.Contains(VertIdx) || NonInfluenceVerts.Contains(VertIdx))
			{
				continue;
			}

			AllOtherVerts.Add(VertIdx);
		}

		DrawVertInfluenceLocations(PDI, SkelComp, AllOtherVerts, FLinearColor(0.f, 0.f, 1.f), FALSE);
	}
}

#pragma warning(push)
#pragma warning(disable : 4730) //mixing _m64 and floating point expressions may result in incorrect code

/** Get Chunk Index from Material Index 
 * Go through all LOD Sections to find which section uses input Material Index 
 * This function also considers LODMaterialMap
 */
INT GetChunkIndexFromMaterialIndex(USkeletalMeshComponent* SkelComp, INT MaterialIndex)
{
	const INT LODIdx = ::Clamp(SkelComp->PredictedLODLevel, 0, SkelComp->SkeletalMesh->LODModels.Num()-1);
	FStaticLODModel& LOD = SkelComp->SkeletalMesh->LODModels(LODIdx);
	const FSkeletalMeshLODInfo& Info = SkelComp->SkeletalMesh->LODInfo(LODIdx);

	INT SelectedChunkIndex=INDEX_NONE;	

	for ( INT SectionID = 0; SectionID<LOD.Sections.Num(); ++SectionID )
	{
		const FSkelMeshSection& Section = LOD.Sections(SectionID);

		// If we are at a dropped LOD, route material index through the LODMaterialMap in the LODInfo struct.
		INT UseMaterialIndex = Section.MaterialIndex;			
		if(Section.MaterialIndex < Info.LODMaterialMap.Num())
		{
			UseMaterialIndex = Info.LODMaterialMap(Section.MaterialIndex);
			UseMaterialIndex = ::Clamp( UseMaterialIndex, 0, SkelComp->SkeletalMesh->Materials.Num() );
		}

		// check if used material index is same as selected mateiral index
		if (MaterialIndex == UseMaterialIndex)
		{
			SelectedChunkIndex = Section.ChunkIndex;
			break;
		}
	}

	return SelectedChunkIndex;
}

template<typename VertexType>
void DrawVertexInformation( FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, INT SelectedMaterialIndex, INT SelectedVertexIndex, UBOOL bDrawTangent, UBOOL bDrawNormal )
{
	DWORD StatusRegister = VectorGetControlRegister();
	VectorSetControlRegister( StatusRegister | VECTOR_ROUND_TOWARD_ZERO );

	const INT RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();
	FMatrix LocalToWorldMat = SkelComp->LocalToWorld;

	//@TODO handle multiple LOD and influence tracks
	const INT LODIdx = ::Clamp(SkelComp->PredictedLODLevel, 0, SkelComp->SkeletalMesh->LODModels.Num()-1);
	const INT InfluenceIdx = 0;
	FStaticLODModel& LOD = SkelComp->SkeletalMesh->LODModels(LODIdx);

	INT SelectedChunkIndex=GetChunkIndexFromMaterialIndex(SkelComp, SelectedMaterialIndex);	
	if (LOD.Chunks.IsValidIndex(SelectedChunkIndex) == FALSE)
	{
		return;
	}

	//Calculate the current bone transforms
	TArray<FBoneAtom> RefToLocal;
	UpdateRefToLocalMatrices(RefToLocal, SkelComp, LODIdx);
	const FBoneAtom* RESTRICT ReferenceToLocal = RefToLocal.GetTypedData();

	INT CachedFinalVerticesNum = LOD.NumVertices;

	// TODO: LS
	if(SkelComp->SkeletalMesh && SkelComp->SkeletalMesh->bEnableClothTearing && (SkelComp->SkeletalMesh->ClothWeldingMap.Num() == 0))
	{
		CachedFinalVerticesNum += SkelComp->SkeletalMesh->ClothTearReserve;
	}
	
 	TArray<FFinalSkinVertex> TransformedVertices;
	TArray<TArray<INT>> InfluencedIndicesList;
	TArray<TArray<FLOAT>> InfluencedWeightsList;

	TransformedVertices.Empty(CachedFinalVerticesNum);
	TransformedVertices.Add(CachedFinalVerticesNum);
	InfluencedIndicesList.Empty(CachedFinalVerticesNum);
	InfluencedIndicesList.AddZeroed(CachedFinalVerticesNum);
	InfluencedWeightsList.Empty(CachedFinalVerticesNum);
	InfluencedWeightsList.AddZeroed(CachedFinalVerticesNum);

	TArray<INT> MorphVertIndices;
	UINT NumValidMorphs = GetMorphVertexIndices(SkelComp->ActiveMorphs,LODIdx,MorphVertIndices);
	UBOOL bUseInfluences = SkelComp->LODInfo(LODIdx).bAlwaysUseInstanceWeights && LOD.VertexInfluences.Num() > 0 && LOD.VertexInfluences(InfluenceIdx).Influences.Num() > 0;
	INT CurBaseVertIdx = 0;
	// VertexCopy for morph. Need to allocate right struct
	// To avoid re-allocation, create 2 statics, and assign right struct
	VertexType  VertexCopy;
	if (LOD.Sections.IsValidIndex(SelectedChunkIndex))
	{
		FSkelMeshChunk& Chunk = LOD.Chunks(SelectedChunkIndex);

		// Prefetch all bone indices
		WORD* BoneMap = Chunk.BoneMap.GetTypedData();
		PREFETCH( BoneMap );
		PREFETCH( BoneMap + 64 );
		PREFETCH( BoneMap + 128 );
		PREFETCH( BoneMap + 192 );

		VertexType* SrcRigidVertex = NULL;
		if (Chunk.GetNumRigidVertices() > 0)
		{
			// Prefetch first vertex
			PREFETCH( LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()) );
		}
		FFinalSkinVertex * DestVertex = TransformedVertices.GetData();

		for(INT VertexIndex = 0;VertexIndex < Chunk.GetNumRigidVertices();VertexIndex++,DestVertex++)
		{
			INT VertexBufferIndex = Chunk.GetRigidVertexBufferIndex() + VertexIndex;
			SrcRigidVertex = (VertexType*)LOD.VertexBufferGPUSkin.GetVertexPtr(VertexBufferIndex);
			PREFETCH_NEXT_CACHE_LINE( SrcRigidVertex );	// Prefetch next vertices
			VertexType* MorphedVertex = SrcRigidVertex;

			if( NumValidMorphs ) 
			{
				MorphedVertex = &VertexCopy;
				UpdateMorphedVertex<VertexType>( &LOD.VertexBufferGPUSkin, SkelComp->ActiveMorphs, *MorphedVertex, *SrcRigidVertex, CurBaseVertIdx, LODIdx, MorphVertIndices );
			}

			VectorRegister SrcPoints[3];
			VectorRegister DstPoints[3];
			const FVector VertexPosition = LOD.VertexBufferGPUSkin.GetVertexPosition(MorphedVertex);
			SrcPoints[0] = VectorLoadFloat3_W1( &VertexPosition );
			SrcPoints[1] = Unpack3( &MorphedVertex->TangentX.Vector.Packed );
			SrcPoints[2] = Unpack4( &MorphedVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Unpack().

			BYTE BoneIndex;
			if (bUseInfluences)
			{
				BoneIndex = LOD.VertexInfluences(InfluenceIdx).Influences(VertexBufferIndex).Bones.InfluenceBones[RigidInfluenceIndex];
			}
			else
			{
				BoneIndex =	MorphedVertex->InfluenceBones[RigidInfluenceIndex];
			}

			InfluencedIndicesList(CurBaseVertIdx).AddItem(BoneIndex);
			InfluencedWeightsList(CurBaseVertIdx).AddItem(1.0f);

			const FMatrix BoneMatrix = ReferenceToLocal[BoneMap[BoneIndex]].ToMatrix();
			VectorRegister M00	= VectorLoadAligned( &BoneMatrix.M[0][0] );
			VectorRegister M10	= VectorLoadAligned( &BoneMatrix.M[1][0] );
			VectorRegister M20	= VectorLoadAligned( &BoneMatrix.M[2][0] );
			VectorRegister M30	= VectorLoadAligned( &BoneMatrix.M[3][0] );

			VectorRegister N_xxxx = VectorReplicate( SrcPoints[0], 0 );
			VectorRegister N_yyyy = VectorReplicate( SrcPoints[0], 1 );
			VectorRegister N_zzzz = VectorReplicate( SrcPoints[0], 2 );
			DstPoints[0] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiplyAdd( N_zzzz, M20, M30 ) ) );

			N_xxxx = VectorReplicate( SrcPoints[1], 0 );
			N_yyyy = VectorReplicate( SrcPoints[1], 1 );
			N_zzzz = VectorReplicate( SrcPoints[1], 2 );
			DstPoints[1] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) );

			N_xxxx = VectorReplicate( SrcPoints[2], 0 );
			N_yyyy = VectorReplicate( SrcPoints[2], 1 );
			N_zzzz = VectorReplicate( SrcPoints[2], 2 );
			DstPoints[2] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) );

			// carry over the W component (sign of basis determinant) 
			DstPoints[2] = VectorMultiplyAdd( VECTOR_0001, SrcPoints[2], DstPoints[2] );

			// Write to 16-byte aligned memory:
			VectorStore( DstPoints[0], &DestVertex->Position );
			Pack3( DstPoints[1], &DestVertex->TangentX.Vector.Packed );
			Pack4( DstPoints[2], &DestVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Pack().

			CurBaseVertIdx++;
		}

		VertexType* SrcSoftVertex = NULL;
		if (Chunk.GetNumSoftVertices() > 0)
		{
			// Prefetch first vertex
			PREFETCH( LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetSoftVertexBufferIndex()) );
		}
		for(INT VertexIndex = 0;VertexIndex < Chunk.GetNumSoftVertices();VertexIndex++,DestVertex++)
		{
			const INT VertexBufferIndex = Chunk.GetSoftVertexBufferIndex() + VertexIndex;
			SrcSoftVertex = (VertexType*)LOD.VertexBufferGPUSkin.GetVertexPtr(VertexBufferIndex);
			PREFETCH_NEXT_CACHE_LINE( SrcSoftVertex );	// Prefetch next vertices
			VertexType* MorphedVertex = SrcSoftVertex;
			if( NumValidMorphs ) 
			{
				MorphedVertex = &VertexCopy;
				UpdateMorphedVertex<VertexType>( &LOD.VertexBufferGPUSkin, SkelComp->ActiveMorphs, *MorphedVertex, *SrcSoftVertex, CurBaseVertIdx, LODIdx, MorphVertIndices );
			}

			const BYTE* RESTRICT BoneIndices;
			const BYTE* RESTRICT BoneWeights;

			if (bUseInfluences)
			{
				BoneIndices = LOD.VertexInfluences(InfluenceIdx).Influences(VertexBufferIndex).Bones.InfluenceBones;
				BoneWeights = LOD.VertexInfluences(InfluenceIdx).Influences(VertexBufferIndex).Weights.InfluenceWeights;
			}
			else
			{
				BoneIndices = MorphedVertex->InfluenceBones;
				BoneWeights = MorphedVertex->InfluenceWeights;

				for (INT I=0; I<4; ++I)
				{
					if ( BoneWeights[I] != 0 )
					{
						InfluencedIndicesList(CurBaseVertIdx).AddItem(BoneIndices[I]);
						FLOAT Weight = BoneWeights[I];
						InfluencedWeightsList(CurBaseVertIdx).AddItem(Weight/255.f);
					}
				}
			}

			static VectorRegister	SrcPoints[3];
			VectorRegister			DstPoints[3];
			const FVector VertexPosition = LOD.VertexBufferGPUSkin.GetVertexPosition(MorphedVertex);
			SrcPoints[0] = VectorLoadFloat3_W1( &VertexPosition );
			SrcPoints[1] = Unpack3( &MorphedVertex->TangentX.Vector.Packed );
			SrcPoints[2] = Unpack4( &MorphedVertex->TangentZ.Vector.Packed );
			VectorRegister Weights = VectorMultiply( VectorLoadByte4(BoneWeights), VECTOR_INV_255 );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Unpack and VectorLoadByte4.

	#if QUAT_SKINNING
			// Dual Quaternion part
			// Linearly blend DQs
			FBoneQuat DualQuat;
			DualQuat.SetBoneAtom(ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_0]]]);
			VectorRegister Weight0 = VectorReplicate( Weights, INFLUENCE_0 );
			VectorRegister BlendDQ0	= VectorMultiply( VectorLoad( &DualQuat.DQ1[0] ), Weight0 );
			VectorRegister BlendDQ1	= VectorMultiply( VectorLoad( &DualQuat.DQ2[0] ), Weight0 );
			FLOAT Scale = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_0]]].GetScale();
			VectorRegister BlendScale = VectorMultiply( VectorLoadFloat1( &Scale ), Weight0 );

			if ( Chunk.MaxBoneInfluences > 1 )
			{
				// Save first DQ0 for testing shortest route for blending
				VectorRegister BaseQuat = BlendDQ0;
				DualQuat.SetBoneAtom(ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_1]]]);

				VectorRegister Weight1 = VectorReplicate( Weights, INFLUENCE_1 );

				VectorRegister DQ0 = VectorLoad( &DualQuat.DQ1[0] );
				VectorRegister DQ1 = VectorLoad( &DualQuat.DQ2[0] );

				// blend scale - need to be done before negate weight
				Scale = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_1]]].GetScale();
				BlendScale	= VectorMultiplyAdd( VectorLoadFloat1( &Scale ), Weight1, BlendScale );

				// If not shortest route, negate weight
				if ( VectorAnyGreaterThan(VectorZero(), VectorDot4( BaseQuat, DQ0 )) )
				{
					Weight1 = VectorNegate(Weight1);
				}

				BlendDQ0	= VectorMultiplyAdd( DQ0 , Weight1, BlendDQ0 );
				BlendDQ1	= VectorMultiplyAdd( DQ1 , Weight1, BlendDQ1 );

				if ( Chunk.MaxBoneInfluences > 2 )
				{
					DualQuat.SetBoneAtom(ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_2]]]);

					VectorRegister Weight2 = VectorReplicate( Weights, INFLUENCE_2 );

					DQ0 = VectorLoad( &DualQuat.DQ1[0] );
					DQ1 = VectorLoad( &DualQuat.DQ2[0] );

					// blend scale - need to be done before negate weight
					Scale = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_2]]].GetScale();
					BlendScale	= VectorMultiplyAdd( VectorLoadFloat1( &Scale ), Weight2, BlendScale );

					// If not shortest route, negate weight
					if ( VectorAnyGreaterThan(VectorZero(), VectorDot4( BaseQuat, DQ0 )) )
					{
						Weight2 = VectorNegate(Weight2);
					}

					BlendDQ0	= VectorMultiplyAdd( DQ0 , Weight2, BlendDQ0 );
					BlendDQ1	= VectorMultiplyAdd( DQ1 , Weight2, BlendDQ1 );

					if ( Chunk.MaxBoneInfluences > 3 )
					{
						DualQuat.SetBoneAtom(ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_3]]]);
						VectorRegister Weight3 = VectorReplicate( Weights, INFLUENCE_3 );
						DQ0 = VectorLoad( &DualQuat.DQ1[0] );
						DQ1 = VectorLoad( &DualQuat.DQ2[0] );
						// blend scale - need to be done before negate weight
						Scale = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_3]]].GetScale();
						BlendScale	= VectorMultiplyAdd( VectorLoadFloat1( &Scale ), Weight3, BlendScale );

						// If not shortest route, negate weight
						if ( VectorAnyGreaterThan(VectorZero(), VectorDot4( BaseQuat, DQ0 )) )
						{
							Weight3 = VectorNegate(Weight3);
						}

						BlendDQ0	= VectorMultiplyAdd( DQ0 , Weight3, BlendDQ0 );
						BlendDQ1	= VectorMultiplyAdd( DQ1 , Weight3, BlendDQ1 );
					}
				}
			}

			// Scale the position
			SrcPoints[0] = VectorSet_W1(VectorMultiply(SrcPoints[0], BlendScale));

			// Normalize
			VectorRegister RecipLen = VectorReciprocalLen(BlendDQ0);
			BlendDQ0 = VectorMultiply(BlendDQ0, RecipLen);
			BlendDQ1 = VectorMultiply(BlendDQ1, RecipLen);

			// Cache variables to transform
			VectorRegister BlendDQ0YZW = VectorSet_W0(VectorSwizzle(BlendDQ0, 1, 2, 3, 0)); 
			VectorRegister BlendDQ1YZW = VectorSet_W0(VectorSwizzle(BlendDQ1, 1, 2, 3, 0)); 
			VectorRegister BlendDQ0XXXX = VectorReplicate(BlendDQ0, 0);
			VectorRegister BlendDQ1XXXX = VectorReplicate(BlendDQ1, 0);

			/* 
			* Dual Quaternion - http://isg.cs.tcd.ie/projects/DualQuaternions/
			* Convert DQ to Matrix
			* This is faster in our case since it calculates DQ result once and reuse it for position/tangents
			* If you use transform, you need to transform at least 3 times - more expensive (a lot of cross products)
			* DQToMatrix does not work with Scale - so I'm saving this for reference in the future
			*/
			DstPoints[0] = VectorMultiplyAdd(VECTOR_2222, VectorCross(BlendDQ0YZW, VectorMultiplyAdd(BlendDQ0XXXX, SrcPoints[0], VectorCross(BlendDQ0YZW, SrcPoints[0]))), SrcPoints[0]);
			VectorRegister Trans = VectorMultiplyAdd(BlendDQ0XXXX, BlendDQ1YZW, VectorAdd(VectorNegate(VectorMultiply(BlendDQ1XXXX, BlendDQ0YZW)), VectorCross(BlendDQ0YZW, BlendDQ1YZW)));
			DstPoints[0] = VectorMultiplyAdd(VECTOR_2222, Trans, DstPoints[0]);

			DstPoints[1] = VectorMultiplyAdd(VECTOR_2222, VectorCross(BlendDQ0YZW, VectorMultiplyAdd(BlendDQ0XXXX, SrcPoints[1], VectorCross(BlendDQ0YZW, SrcPoints[1]))), SrcPoints[1]);
			DstPoints[2] = VectorMultiplyAdd(VECTOR_2222, VectorCross(BlendDQ0YZW, VectorMultiplyAdd(BlendDQ0XXXX, SrcPoints[2], VectorCross(BlendDQ0YZW, SrcPoints[2]))), SrcPoints[2]);
	#else

			const FMatrix BoneMatrix0 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_0]]].ToMatrix();
			VectorRegister Weight0 = VectorReplicate( Weights, INFLUENCE_0 );
			VectorRegister M00	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[0][0] ), Weight0 );
			VectorRegister M10	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[1][0] ), Weight0 );
			VectorRegister M20	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[2][0] ), Weight0 );
			VectorRegister M30	= VectorMultiply( VectorLoadAligned( &BoneMatrix0.M[3][0] ), Weight0 );

			if ( Chunk.MaxBoneInfluences > 1 )
			{
				const FMatrix BoneMatrix1 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_1]]].ToMatrix();
				VectorRegister Weight1 = VectorReplicate( Weights, INFLUENCE_1 );
				M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[0][0] ), Weight1, M00 );
				M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[1][0] ), Weight1, M10 );
				M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[2][0] ), Weight1, M20 );
				M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix1.M[3][0] ), Weight1, M30 );

				if ( Chunk.MaxBoneInfluences > 2 )
				{
					const FMatrix BoneMatrix2 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_2]]].ToMatrix();
					VectorRegister Weight2 = VectorReplicate( Weights, INFLUENCE_2 );
					M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[0][0] ), Weight2, M00 );
					M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[1][0] ), Weight2, M10 );
					M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[2][0] ), Weight2, M20 );
					M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix2.M[3][0] ), Weight2, M30 );

					if ( Chunk.MaxBoneInfluences > 3 )
					{
						const FMatrix BoneMatrix3 = ReferenceToLocal[BoneMap[BoneIndices[INFLUENCE_3]]].ToMatrix();
						VectorRegister Weight3 = VectorReplicate( Weights, INFLUENCE_3 );
						M00	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[0][0] ), Weight3, M00 );
						M10	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[1][0] ), Weight3, M10 );
						M20	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[2][0] ), Weight3, M20 );
						M30	= VectorMultiplyAdd( VectorLoadAligned( &BoneMatrix3.M[3][0] ), Weight3, M30 );
					}
				}
			}

			VectorRegister N_xxxx = VectorReplicate( SrcPoints[0], 0 );
			VectorRegister N_yyyy = VectorReplicate( SrcPoints[0], 1 );
			VectorRegister N_zzzz = VectorReplicate( SrcPoints[0], 2 );
			DstPoints[0] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiplyAdd( N_zzzz, M20, M30 ) ) );

			DstPoints[1] = VectorZero();
			N_xxxx = VectorReplicate( SrcPoints[1], 0 );
			N_yyyy = VectorReplicate( SrcPoints[1], 1 );
			N_zzzz = VectorReplicate( SrcPoints[1], 2 );
			DstPoints[1] = VectorNormalize(VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) ));

			N_xxxx = VectorReplicate( SrcPoints[2], 0 );
			N_yyyy = VectorReplicate( SrcPoints[2], 1 );
			N_zzzz = VectorReplicate( SrcPoints[2], 2 );
			DstPoints[2] = VectorZero();
			DstPoints[2] = VectorNormalize(VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) ));

			// carry over the W component (sign of basis determinant) 
			DstPoints[2] = VectorMultiplyAdd( VECTOR_0001, SrcPoints[2], DstPoints[2] );
	#endif

			// Write to 16-byte aligned memory:
			VectorStore( DstPoints[0], &DestVertex->Position );
			Pack3( DstPoints[1], &DestVertex->TangentX.Vector.Packed );
			Pack4( DstPoints[2], &DestVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Pack().

			// Copy UVs.
			FVector2D UVs = LOD.VertexBufferGPUSkin.GetVertexUV(Chunk.GetSoftVertexBufferIndex()+VertexIndex,0);
			DestVertex->U = UVs.X;
			DestVertex->V = UVs.Y;

			CurBaseVertIdx++;
		}
	}

	// now render information you need
	FVector Tangent, Normal, Position, WorldPos;
	UBOOL Selected, MinusZ;
	FColor DrawColor;
	for (INT I=0; I<TransformedVertices.Num(); ++I)
	{
		// selected
		Selected = (I == SelectedVertexIndex);
	
		// set proxy with information
		Position = TransformedVertices(I).Position;
		WorldPos = LocalToWorldMat.TransformFVector(Position);
		HVertexInfoProxy * Proxy = new HVertexInfoProxy(I, WorldPos);
		Proxy->AddInfluences(InfluencedIndicesList(I), InfluencedWeightsList(I));
		PDI->SetHitProxy(Proxy);

		// draw tangent
		if ( bDrawTangent )
		{
			Tangent = TransformedVertices(I).TangentX;
			MinusZ = (Tangent.Z <= 0.f);

			Tangent *= 5.f; 
			Tangent += Position;

			Tangent = LocalToWorldMat.TransformFVector(Tangent);
			
			if ( MinusZ )
			{
				DrawColor = FColor(0, 0, 255);
			}
			else
			{
				DrawColor = FColor(255, 0, 0);
			}

			if ( Selected )
			{
				PDI->DrawLine(WorldPos, Tangent, DrawColor, SDPG_Foreground, 0.5f);
			}
			else
			{
				PDI->DrawLine(WorldPos, Tangent, DrawColor, SDPG_World);
			}
		}

		// draw normal
		if ( bDrawNormal )
		{
			Normal = TransformedVertices(I).TangentZ;
			MinusZ = (Normal.Z <= 0.f);

			Normal *= 5.f;
			Normal += Position;

			Normal = LocalToWorldMat.TransformFVector(Normal);

			if ( MinusZ )
			{
				DrawColor = FColor(0, 255, 0);
			}
			else
			{
				DrawColor = FColor(255, 255, 0);
			}

			if ( Selected )
			{
				PDI->DrawLine(WorldPos, Normal, DrawColor, SDPG_Foreground, 0.5f);
			}
			else
			{
				PDI->DrawLine(WorldPos, Normal, DrawColor, SDPG_World);
			}
		}

		// if selected, draw highlight point
		if ( I == SelectedVertexIndex )
		{
			PDI->DrawPoint(WorldPos, FColor(255, 255, 0), 10.f, SDPG_World);
		}
		else
		{
			PDI->DrawPoint(WorldPos, FColor(255, 255, 255), 3.f, SDPG_World);
		}

		PDI->SetHitProxy(NULL);
	}
}

#pragma warning(pop)

FASVViewportClient::FASVViewportClient( WxAnimSetViewer* InASV ):
	PreviewScene(FRotator(-8192,-8192,0),0.25f,1.f,TRUE)
{
	bDraggingTrackPos = FALSE;
	DraggingNotifyIndex = -1;
	DraggingMetadataIndex = -1;
	DraggingMetadataKeyIndex = -1;

	AnimSetViewer = InASV;

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = FALSE;
	DrawHelper.bDrawWorldBox = FALSE;
	DrawHelper.bDrawKillZ = FALSE;
	DrawHelper.GridColorHi = FColor(80,80,80);
	DrawHelper.GridColorLo = FColor(72,72,72);
	DrawHelper.PerspectiveGridSize = 32767;

	// Create AnimNodeSequence for previewing animation playback
	InASV->PreviewAnimNode = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass());
	InASV->PreviewAnimNode->bEditorOnlyAddRefPoseToAdditiveAnimation = TRUE;
	InASV->PreviewAnimNodeRaw = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass());
	InASV->PreviewAnimNodeRaw->bEditorOnlyAddRefPoseToAdditiveAnimation = TRUE;

	// Mirror node for testing that.
	InASV->PreviewAnimMirror = ConstructObject<UAnimNodeMirror>(UAnimNodeMirror::StaticClass());
	InASV->PreviewAnimMirror->Children(0).Anim = InASV->PreviewAnimNode;
	InASV->PreviewAnimMirror->bEnableMirroring = FALSE; // Default to mirroring off.

	InASV->PreviewAnimMirrorRaw = ConstructObject<UAnimNodeMirror>(UAnimNodeMirror::StaticClass());
	InASV->PreviewAnimMirrorRaw->Children(0).Anim = InASV->PreviewAnimNodeRaw;
	InASV->PreviewAnimMirrorRaw->bEnableMirroring = FALSE; // Default to mirroring off.

	// Morph pose node for previewing morph targets.
	InASV->PreviewMorphPose = ConstructObject<UMorphNodeMultiPose>(UMorphNodeMultiPose::StaticClass());

	// Finally, AnimTree to hold them all
	InASV->PreviewAnimTree = ConstructObject<UAnimTree>(UAnimTree::StaticClass());
	InASV->PreviewAnimTree->Children(0).Anim = InASV->PreviewAnimMirror;
	InASV->PreviewAnimTree->RootMorphNodes.AddItem(InASV->PreviewMorphPose);


	// Create a skel control to handle bone manipulation
	FSkelControlListHead SkelControlEntry(EC_EventParm);
	SkelControlEntry.BoneName = NAME_None;
	
	USkelControlSingleBone* SkelControl = ConstructObject<USkelControlSingleBone>(USkelControlSingleBone::StaticClass());

	SkelControl->bApplyTranslation = TRUE;
	SkelControl->bApplyRotation = TRUE;
	SkelControl->bAddTranslation = FALSE;
	SkelControl->bAddRotation = FALSE;
	SkelControl->BoneTranslationSpace = BCS_BoneSpace;
	SkelControl->BoneRotationSpace = BCS_BoneSpace;

	SkelControlEntry.ControlHead = SkelControl;
	InASV->PreviewAnimTree->SkelControlLists.AddItem(SkelControlEntry);


	InASV->PreviewAnimTreeRaw = ConstructObject<UAnimTree>(UAnimTree::StaticClass());
	InASV->PreviewAnimTreeRaw->Children(0).Anim = InASV->PreviewAnimMirrorRaw;
	InASV->PreviewAnimTreeRaw->RootMorphNodes.AddItem(InASV->PreviewMorphPose);

	// Create SkeletalMeshComponent for rendering skeletal mesh
	InASV->PreviewSkelComp = ConstructObject<UASVSkelComponent>(UASVSkelComponent::StaticClass());
	InASV->PreviewSkelComp->Animations = InASV->PreviewAnimTree;
	InASV->PreviewSkelComp->AnimSetViewerPtr = InASV;
	// Setting bUpdateSkelWhenNotRendered to true below forces updates to the renderer thread on each frame.
	// [[ When this is not set, the preview rendering in the ASV may fail because the code in
	//    USkeletalMeshComponent::Tick that tries to determine whether the component was recently rendered
	//    seems to fail due to bad timing data. ]]
	InASV->PreviewSkelComp->bUpdateSkelWhenNotRendered = TRUE;
	// Don't cache animations by default.
	InASV->PreviewSkelComp->bCacheAnimSequenceNodes = FALSE;
	PreviewScene.AddComponent(InASV->PreviewSkelComp,FMatrix::Identity);

	LineBatcher = ConstructObject<ULineBatchComponent>(ULineBatchComponent::StaticClass());
	PreviewScene.AddComponent(LineBatcher,FMatrix::Identity);

	InASV->PreviewSkelCompRaw = ConstructObject<UASVSkelComponent>(UASVSkelComponent::StaticClass());
	InASV->PreviewSkelCompRaw->Animations = InASV->PreviewAnimTreeRaw;
	InASV->PreviewSkelCompRaw->AnimSetViewerPtr = InASV;
	InASV->PreviewSkelCompRaw->bUseRawData = TRUE;
	InASV->PreviewSkelCompRaw->BoneColor = FColor(255, 128, 0);
	InASV->PreviewSkelCompRaw->bDrawMesh = FALSE;
	InASV->PreviewSkelCompRaw->bUpdateSkelWhenNotRendered = TRUE;
	// Don't cache animations by default.
	InASV->PreviewSkelCompRaw->bCacheAnimSequenceNodes = FALSE;
	PreviewScene.AddComponent(InASV->PreviewSkelCompRaw,FMatrix::Identity);

	// Create 3 more SkeletalMeshComponents to show extra 'overlay' meshes.
	InASV->PreviewSkelCompAux1 = ConstructObject<USkeletalMeshComponent>(USkeletalMeshComponent::StaticClass());
	InASV->PreviewSkelCompAux1->ParentAnimComponent = InASV->PreviewSkelComp;
	InASV->PreviewSkelCompAux1->WireframeColor = ExtraMesh1WireColor;
	InASV->PreviewSkelCompAux1->bUseOnePassLightingOnTranslucency = TRUE;
	PreviewScene.AddComponent(InASV->PreviewSkelCompAux1,FMatrix::Identity);

	InASV->PreviewSkelCompAux2 = ConstructObject<USkeletalMeshComponent>(USkeletalMeshComponent::StaticClass());
	InASV->PreviewSkelCompAux2->ParentAnimComponent = InASV->PreviewSkelComp;
	InASV->PreviewSkelCompAux2->WireframeColor = ExtraMesh2WireColor;
	InASV->PreviewSkelCompAux2->bUseOnePassLightingOnTranslucency = TRUE;
	PreviewScene.AddComponent(InASV->PreviewSkelCompAux2,FMatrix::Identity);

	InASV->PreviewSkelCompAux3 = ConstructObject<USkeletalMeshComponent>(USkeletalMeshComponent::StaticClass());
	InASV->PreviewSkelCompAux3->ParentAnimComponent = InASV->PreviewSkelComp;
	InASV->PreviewSkelCompAux3->WireframeColor = ExtraMesh3WireColor;
	InASV->PreviewSkelCompAux3->bUseOnePassLightingOnTranslucency = TRUE;
	PreviewScene.AddComponent(InASV->PreviewSkelCompAux3,FMatrix::Identity);

	ShowFlags = SHOW_DefaultEditor;

	// Set the viewport to be fully lit.
	ShowFlags &= ~SHOW_ViewMode_Mask;
	ShowFlags |= SHOW_ViewMode_Lit;

	bShowBoneNames = FALSE;
	bShowFloor = FALSE;
	bShowSockets = FALSE;
	bShowMorphKeys = FALSE;

	bManipulating = FALSE;
	SocketManipulateAxis = AXIS_None;
	BoneManipulateAxis = AXIS_None;
	DragDirX = 0.f;
	DragDirY = 0.f;
	WorldManDir = FVector(0.f);
	LocalManDir = FVector(0.f);

	NearPlane = 1.0f;

	SetRealtime( TRUE );

	bAllowMayaCam = TRUE;

	bTriangleSortMode = FALSE;

	// Clothing may have been created in another RBPhysScene; release it so it can be created in the right one
	InASV->PreviewSkelComp->ReleaseApexClothing();
	InASV->PreviewSkelComp->InitApexClothing(InASV->RBPhysScene);

	// Override global realtime audio settings
	bAudioRealtimeOverride = TRUE;
	bWantAudioRealtime = FALSE;

#if WITH_SIMPLYGON
	SimplygonLogo = LoadObject<UTexture2D>(NULL, TEXT("EditorResources.SimplygonLogo_Med"), NULL, LOAD_None, NULL);
#endif // #if WITH_SIMPLYGON
}

FASVViewportClient::~FASVViewportClient()
{
}

void FASVViewportClient::Serialize(FArchive& Ar)
{ 
	Ar << Input; 
	Ar << PreviewScene;
}

FLinearColor FASVViewportClient::GetBackgroundColor()
{
	return FColor(64,64,64);
}

/*
 *   Draw the rotation/translation delta during widget manipulation
 *	@Canvas - canvas to drawn on
 *	@XPos - Screen space x coord
 *	@YPos - Screen space y coord
 *	@ManipAxis - axis currently being manipulated
 *	@MoveMode - movement mode (rotation/translation)
 *	@Rotation - rotation delta
 *	@Translation - translation delta
 */
void DrawAngles(FCanvas* Canvas, INT XPos, INT YPos, EAxis ManipAxis, EASVSocketMoveMode MoveMode, const FRotator& Rotation, const FVector& Translation)
{
	FString OutputString(TEXT(""));
	if (MoveMode == SMM_Rotate && Rotation.IsZero() == FALSE)
	{
		//Only one value moves at a time
		const FVector EulerAngles = Rotation.Euler();
		if (ManipAxis == AXIS_X)
		{
			OutputString += FString::Printf(TEXT("Roll: %0.2f"), EulerAngles.X);
		}
		else if (ManipAxis == AXIS_Y)
		{
			OutputString += FString::Printf(TEXT("Pitch: %0.2f"), EulerAngles.Y);
		}
		else if (ManipAxis == AXIS_Z)
		{
			OutputString += FString::Printf(TEXT("Yaw: %0.2f"), EulerAngles.Z);
		}
	}
	else if (MoveMode == SMM_Translate && Translation.IsZero() == FALSE)
	{
		//Only one value moves at a time
		if (ManipAxis == AXIS_X)
		{
			OutputString += FString::Printf(TEXT(" %0.2f"), Translation.X);
		}
		else if (ManipAxis == AXIS_Y)
		{
			OutputString += FString::Printf(TEXT(" %0.2f"), Translation.Y);
		}
		else if (ManipAxis == AXIS_Z)
		{
			OutputString += FString::Printf(TEXT(" %0.2f"), Translation.Z);
		}
	}

	if (OutputString.Len() > 0)
	{
		DrawString(Canvas, XPos, YPos, *OutputString, GEngine->SmallFont, FLinearColor::White);
	}
}


#define HITPROXY_SOCKET	1
#define HITPROXY_BONE   2
void FASVViewportClient::Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Draw common controls.
	DrawHelper.Draw(View, PDI);

	// Generate matrices for all sockets if we are drawing sockets.
	if( bShowSockets || AnimSetViewer->SocketMgr )
	{
		for(INT i=0; i<AnimSetViewer->SelectedSkelMesh->Sockets.Num(); i++)
		{
			USkeletalMeshSocket* Socket = AnimSetViewer->SelectedSkelMesh->Sockets(i);

			FMatrix SocketTM;
			if( Socket->GetSocketMatrix(SocketTM, AnimSetViewer->PreviewSkelComp) )
			{
				PDI->SetHitProxy( new HASVSocketProxy(i) );
				DrawWireDiamond(PDI, SocketTM, 2.f, FColor(255,128,128), SDPG_Foreground );
				PDI->SetHitProxy( NULL );

				// Draw widget for selected socket if Socket Manager is up.
				if(AnimSetViewer->SocketMgr && AnimSetViewer->SelectedSocket && AnimSetViewer->SelectedSocket == Socket)
				{
					const EAxis HighlightAxis = SocketManipulateAxis;

					// Info1 is just an int to determine if its coming from socket manager or bone manipulation, Info2 unused
					if(AnimSetViewer->SocketMgr->MoveMode == SMM_Rotate)
					{
						FUnrealEdUtils::DrawWidget(View, PDI, SocketTM, HITPROXY_SOCKET, 0, HighlightAxis, WMM_Rotate);
					}
					else
					{
						FUnrealEdUtils::DrawWidget(View, PDI, SocketTM, HITPROXY_SOCKET, 0, HighlightAxis, WMM_Translate);
					}
				}
			}
		}
	}
	
    if (AnimSetViewer->SelectedSocket == NULL && AnimSetViewer->PreviewSkelComp && AnimSetViewer->PreviewSkelComp->BonesOfInterest.Num() == 1)
	{
		const EAxis HighlightAxis = BoneManipulateAxis;
		FMatrix BoneMatrix = AnimSetViewer->PreviewSkelComp->GetBoneMatrix(AnimSetViewer->PreviewSkelComp->BonesOfInterest(0));
		// Info1 is just an int to determine if its coming from socket manager or bone manipulation, Info2 unused
		if(AnimSetViewer->BoneMoveMode == SMM_Rotate)
		{
			FUnrealEdUtils::DrawWidget(View, PDI, BoneMatrix, HITPROXY_BONE, 0, HighlightAxis, WMM_Rotate);
		}
		else
		{
			FUnrealEdUtils::DrawWidget(View, PDI, BoneMatrix, HITPROXY_BONE, 0, HighlightAxis, WMM_Translate);
		}
	}

	if (AnimSetViewer->PreviewSkelComp->bDrawBoneInfluences)
	{
		const WxAnimSetViewer::FLODInfluenceInfo& LODInfluenceInfo = AnimSetViewer->BoneInfluenceLODInfo(AnimSetViewer->PreviewSkelComp->PredictedLODLevel);
        //Draw the influenced swapped set
		DrawVertInfluenceLocations(PDI, AnimSetViewer->PreviewSkelComp, LODInfluenceInfo.InfluencedBoneVerts, FLinearColor::White, TRUE);
		//Draw the non-swapped influence set
		DrawVertInfluenceLocations(PDI, AnimSetViewer->PreviewSkelComp, LODInfluenceInfo.NonInfluencedBoneVerts, FLinearColor(1.0f, 0.0f, 0.0f), FALSE);
		//Draw all other vertices
		if (AnimSetViewer->bDrawAllBoneInfluenceVertices)
		{
		   DrawAllOtherVertexLocations(PDI, AnimSetViewer->PreviewSkelComp, LODInfluenceInfo.InfluencedBoneVerts, LODInfluenceInfo.NonInfluencedBoneVerts);
		}
	}

	if (AnimSetViewer->VertexInfo.bShowTangent || AnimSetViewer->VertexInfo.bShowNormal)
	{
		//Draw the influenced swapped set
		if ( AnimSetViewer->PreviewSkelComp->SkeletalMesh->bUseFullPrecisionUVs )
		{
			DrawVertexInformation<TGPUSkinVertexFloat32Uvs<>>(PDI, AnimSetViewer->PreviewSkelComp, AnimSetViewer->VertexInfo.SelectedMaterialIndex, AnimSetViewer->VertexInfo.SelectedVertexIndex, AnimSetViewer->VertexInfo.bShowTangent, AnimSetViewer->VertexInfo.bShowNormal);
		}
		else
		{
			DrawVertexInformation<TGPUSkinVertexFloat16Uvs<>>(PDI, AnimSetViewer->PreviewSkelComp, AnimSetViewer->VertexInfo.SelectedMaterialIndex, AnimSetViewer->VertexInfo.SelectedVertexIndex, AnimSetViewer->VertexInfo.bShowTangent, AnimSetViewer->VertexInfo.bShowNormal);
		}
	}

	if(AnimSetViewer->bShowClothMovementScale)
	{
		DrawClothMovementDistanceScale(PDI, AnimSetViewer->PreviewSkelComp);
	}

	//FEditorLevelViewportClient::Draw(View, PDI);
}

struct TrackSlots
{
	struct Slot
	{
		int X, W;
	};
	TArray<Slot> Slots;

	bool AddSlot(int X, int W)
	{
		for (INT Idx = 0; Idx < Slots.Num(); Idx++)
		{
			if (X >= Slots(Idx).X && X < Slots(Idx).X + Slots(Idx).W)
			{
				return FALSE;
			}
			if (Slots(Idx).X >= X && Slots(Idx).X < X + W)
			{
				return FALSE;
			}
		}
		INT Idx = Slots.AddZeroed();
		Slots(Idx).X = X;
		Slots(Idx).W = W;
		return TRUE;
	}
};

struct HNotifyProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( HNotifyProxy, HHitProxy );
	INT NotifyIdx;
	UBOOL bTail;
	HNotifyProxy(INT InNotifyIdx, UBOOL InbTail = FALSE)
		:	HHitProxy( HPP_UI )
		,	NotifyIdx( InNotifyIdx )
		,	bTail( InbTail )
	{}
};

struct HBoneControlProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( HBoneControlProxy, HHitProxy );
	INT ControlModifierID;
	INT TimeModifierID;
	HBoneControlProxy(INT InControlModifierID, INT InTimeModifierID)
		:	HHitProxy( HPP_UI )
		,	ControlModifierID( InControlModifierID )
		,	TimeModifierID( InTimeModifierID )
	{}
};

struct HTrackPosProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( HTrackPosProxy, HHitProxy );
	HTrackPosProxy()
		:	HHitProxy( HPP_UI )
	{}
};


/**
* Strip data for manual triangle sorting 
*/
struct FSortStripData
{
	FSortStripData( INT InFirstIndex, INT InNumTris )
		:	FirstIndex(InFirstIndex)
		,	NumTris(InNumTris)
		,	HitProxy(NULL)
	{}

	INT FirstIndex;
	INT NumTris;
	TRefCountPtr<HSortStripProxy> HitProxy;
};


/*-----------------------------------------------------------------------------
FASVSkelSceneProxy
-----------------------------------------------------------------------------*/

/**
* A skeletal mesh component scene proxy.
*/
class FASVSkelSceneProxy : public FSkeletalMeshSceneProxy
{
public:
	/** 
	* Constructor. 
	* @param	Component - skeletal mesh primitive being added
	*/
	FASVSkelSceneProxy(const UASVSkelComponent* InComponent, const FColor& InBoneColor = FColor(230, 230, 255), const FColor& InWireframeOverlayColor = FColor(255, 255, 255)) :
		FSkeletalMeshSceneProxy( InComponent, InBoneColor ),
		bHasBoneLine(FALSE)
	{
		SkeletalMeshComponent = const_cast<UASVSkelComponent*>(InComponent);
		WxAnimSetViewer* ASV = (WxAnimSetViewer*)SkeletalMeshComponent->AnimSetViewerPtr;
		const FASVViewportClient* ASVPreviewVC = ASV->GetPreviewVC();

		// If showing bones, then show root motion delta
		if( bDisplayBones)
		{
			UAnimNodeSequence* AnimNodeSeq = ASV->PreviewAnimNode;
			if( ASV->SelectedAnimSet && ASV->SelectedAnimSeq &&
				AnimNodeSeq && AnimNodeSeq->AnimSeq == ASV->SelectedAnimSeq  && 
				AnimNodeSeq->SkelComponent == SkeletalMeshComponent && 
				ASV->SelectedAnimSet == AnimNodeSeq->AnimSeq->GetAnimSet() )
			{
				UAnimSet* AnimSet = ASV->SelectedAnimSet;
				const INT AnimLinkupIndex = AnimNodeSeq->AnimLinkupIndex;

				check(AnimLinkupIndex != INDEX_NONE);
				check(AnimLinkupIndex < AnimSet->LinkupCache.Num());
				FAnimSetMeshLinkup* AnimLinkup = &AnimSet->LinkupCache(AnimLinkupIndex);
				check(AnimLinkup);

				// Find which track in the sequence we look in for the Root Bone data
				const INT TrackIndex = AnimLinkup->BoneToTrackTable(0);
				FBoneAtom RootBoneAtom;

				// If there is no track for this bone, we just use the reference pose.
				if( TrackIndex == INDEX_NONE )
				{
					TArray<FMeshBone>& RefSkel = SkeletalMeshComponent->SkeletalMesh->RefSkeleton;
					RootBoneAtom.SetComponents(RefSkel(0).BonePos.Orientation, RefSkel(0).BonePos.Position);					
				}
				else 
				{
					FMemMark Mark(GMainThreadMemStack);
					// get the exact translation of the root bone on the first frame of the animation
					ASV->SelectedAnimSeq->GetBoneAtom(RootBoneAtom, TrackIndex, 0.f, FALSE, SkeletalMeshComponent->bUseRawData, NULL);

					Mark.Pop();
				}

				// convert mesh space root loc to world space
				BoneRootMotionLine[0] = SkeletalMeshComponent->LocalToWorld.TransformFVector(RootBoneAtom.GetTranslation());
				BoneRootMotionLine[1] = SkeletalMeshComponent->GetBoneAtom(0).GetOrigin();

				bHasBoneLine = TRUE;
			}
		}

		WireframeOverlayColor = InWireframeOverlayColor;
		bShowSoftBodyTetra = InComponent->bShowSoftBodyTetra;
	}

	virtual ~FASVSkelSceneProxy() {}

	// FPrimitiveSceneProxy interface.
	virtual HHitProxy* CreateHitProxies(const UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
	{
		WxAnimSetViewer* ASV = (WxAnimSetViewer*)SkeletalMeshComponent->AnimSetViewerPtr;

		// Clear sort strip data.
		ASV->SelectedSortStripIndex = INDEX_NONE;
		ASV->SelectedSortStripLodIndex = INDEX_NONE;
		ASV->SelectedSortStripSectionIndex = INDEX_NONE;
		ASV->MouseOverSortStripIndex = INDEX_NONE;
		ASV->bSortStripMoveForward = FALSE;
		ASV->bSortStripMoveBackward = FALSE;
		VisualizeSortStripData.Empty();

		if( SkeletalMeshComponent->SkeletalMesh != NULL && SkeletalMeshComponent == ASV->PreviewSkelComp )
		{
			VisualizeSortStripData.AddZeroed(SkeletalMeshComponent->SkeletalMesh->LODModels.Num());

			// Get connected strips for any TRISORT_Custom sections.
			for( INT LodIndex=0;LodIndex<SkeletalMeshComponent->SkeletalMesh->LODModels.Num();LodIndex++ )
			{
				FStaticLODModel& LODModel = SkeletalMeshComponent->SkeletalMesh->LODModels(LodIndex);

				VisualizeSortStripData(LodIndex).AddZeroed(LODModel.Sections.Num());

				for( INT SectionIndex=0;SectionIndex < LODModel.Sections.Num();SectionIndex++ )
				{
					FSkelMeshSection& Section = LODModel.Sections(SectionIndex);

					if( Section.TriangleSorting == TRISORT_Custom || Section.TriangleSorting == TRISORT_CustomLeftRight )
					{
						TArray<FSortStripData>& StripData = VisualizeSortStripData(LodIndex)(SectionIndex);

						INT BaseIndex = Section.BaseIndex;
						if( Section.TriangleSorting == TRISORT_CustomLeftRight && SkeletalMeshComponent->CustomSortAlternateIndexMode==CSAIM_Left )
						{
							BaseIndex += Section.NumTriangles*3;
						}

						TArray<UINT> TriSet;
						TArray<DWORD> Indices;
						LODModel.MultiSizeIndexContainer.GetIndexBuffer( Indices );
						GetConnectedTriangleSets( Section.NumTriangles, Indices.GetData() + BaseIndex, TriSet );

						INT StripCount = 0;
						UINT TriIndex = 0;
						while(TriIndex < Section.NumTriangles)
						{
							// First triangle in a new strip
							UINT StripTriSet = TriSet(TriIndex);
							INT StripFirstIndex = BaseIndex + TriIndex*3;
							INT StripNumTris = 0;

							// Loop through the triangles until we see a change in triangle set
							while(TriIndex < Section.NumTriangles && TriSet(TriIndex) == StripTriSet)
							{
								StripNumTris++;
								TriIndex++;
							}

							// Make a hit proxy for the triangle strip and save the info in the StripData array.
							HSortStripProxy* NewProxy = new HSortStripProxy(this, LodIndex, SectionIndex, StripCount);
							OutHitProxies.AddItem(NewProxy);
							FSortStripData* NewStripData = new(StripData) FSortStripData(StripFirstIndex, StripNumTris);
							NewStripData->HitProxy = NewProxy;
							StripCount++;
						}
					}
				}
			}
		}

		return FSkeletalMeshSceneProxy::CreateHitProxies(Component,OutHitProxies);
	}

	/** 
	* Render bones for debug display
	*/
	virtual void DebugDrawBonesSubset(FPrimitiveDrawInterface* PDI,const FSceneView* View, const TArray<FBoneAtom>& InSpaceBases, const class FStaticLODModel& LODModel, const TArray<INT>& BonesOfInterest, const FColor& LineColor, INT ChunkIndexPreview)
	{
		FMatrix LocalToWorld, WorldToLocal;
		GetWorldMatrices( View, LocalToWorld, WorldToLocal );

		TArray<INT> BonesToDraw(BonesOfInterest);

		TArray<FBoneAtom> WorldBases;
		WorldBases.Add( InSpaceBases.Num() );
		for(INT i=0; i<LODModel.RequiredBones.Num(); i++)
		{
			INT BoneIndex = LODModel.RequiredBones(i);
			check(BoneIndex < InSpaceBases.Num());

			// transform bone mats to world space
			WorldBases(BoneIndex) = InSpaceBases(BoneIndex) * FBoneAtom(LocalToWorld);
			WorldBases(BoneIndex).NormalizeRotation();

			// If previewing a specific chunk, only show the bones that belong to it
			if ((ChunkIndexPreview >= 0) && !LODModel.Chunks(ChunkIndexPreview).BoneMap.ContainsItem(BoneIndex))
			{
				continue;
			}

			const FColor& BoneColor = SkeletalMesh->RefSkeleton(BoneIndex).BoneColor;

			UBOOL DrawBone = BonesToDraw.ContainsItem(BoneIndex);

			if( BoneIndex == 0 && DrawBone )
			{
				PDI->DrawLine(WorldBases(BoneIndex).GetOrigin(), LocalToWorld.GetOrigin(), FColor(255, 0, 255), SDPG_Foreground, 0.7f);
			}
			else
			{
				INT ParentIdx = SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;

				DrawBone = DrawBone || BonesToDraw.ContainsItem(ParentIdx);

				if (DrawBone)
				{
					BonesToDraw.AddItem(BoneIndex);
					PDI->DrawLine(WorldBases(BoneIndex).GetOrigin(), WorldBases(ParentIdx).GetOrigin(), BoneColor, SDPG_Foreground, 0.7f);
				}
			}

			if (DrawBone)
			{
				// Display colored coordinate system axes for each joint.			
				// Red = X
				FVector XAxis = WorldBases(BoneIndex).TransformNormal( FVector(1.0f,0.0f,0.0f));
				XAxis.Normalize();
				PDI->DrawLine( WorldBases(BoneIndex).GetOrigin(), WorldBases(BoneIndex).GetOrigin() + XAxis * 3.75f, FColor( 255, 80, 80), SDPG_Foreground );			
				// Green = Y
				FVector YAxis = WorldBases(BoneIndex).TransformNormal( FVector(0.0f,1.0f,0.0f));
				YAxis.Normalize();
				PDI->DrawLine( WorldBases(BoneIndex).GetOrigin(), WorldBases(BoneIndex).GetOrigin() + YAxis * 3.75f, FColor( 80, 255, 80), SDPG_Foreground ); 
				// Blue = Z
				FVector ZAxis = WorldBases(BoneIndex).TransformNormal( FVector(0.0f,0.0f,1.0f));
				ZAxis.Normalize();
				PDI->DrawLine( WorldBases(BoneIndex).GetOrigin(), WorldBases(BoneIndex).GetOrigin() + ZAxis * 3.75f, FColor( 80, 80, 255), SDPG_Foreground ); 
			}
		}
	}

	void DrawSortStrip(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags, WxAnimSetViewer* ASV)
	{
		if( !MeshObject )
		{
			return;
		}

		INT LODIndex = MeshObject->GetLOD();
		if( LODIndex >= VisualizeSortStripData.Num() )
		{
			return;
		}

		check(LODIndex < SkeletalMesh->LODModels.Num());
		const FStaticLODModel& LODModel = SkeletalMesh->LODModels(LODIndex);
		const FLODSectionElements& LODSection = LODSections(LODIndex);


		// Hit proxies should be rendered at the same DPG the primitive.
		// Wireframe selection is rendered in SDPG_Foreground.
		BYTE DrawDPG = PDI->IsHitTesting() ? GetDepthPriorityGroup(View) : SDPG_Foreground;

		if (DrawDPG == DPGIndex)
		{
			for(INT SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
			{
				TArray<FSortStripData>& SortStripData = VisualizeSortStripData(LODIndex)(SectionIndex);
				if (SortStripData.Num() > 0)
				{
					check(SkeletalMesh->LODInfo.Num() == SkeletalMesh->LODModels.Num());
					check(LODSection.SectionElements.Num()==LODModel.Sections.Num());

					FLinearColor WireframeLinearColor(WireframeOverlayColor);
					const FSkelMeshSection& Section = LODModel.Sections(SectionIndex);
					const FSkelMeshChunk& Chunk = LODModel.Chunks(Section.ChunkIndex);
					const FSectionElementInfo& SectionElementInfo = LODSection.SectionElements(SectionIndex);

					FMeshBatch Mesh;		
					FMeshBatchElement& BatchElement = Mesh.Elements(0);
					BatchElement.IndexBuffer = LODModel.MultiSizeIndexContainer.GetIndexBuffer();
					BatchElement.MaxVertexIndex = LODModel.NumVertices - 1;
					Mesh.VertexFactory = MeshObject->GetVertexFactory(LODIndex,Section.ChunkIndex);
					Mesh.DynamicVertexData = NULL;
					Mesh.LCI = NULL;
					GetWorldMatrices( View, BatchElement.LocalToWorld, BatchElement.WorldToLocal );
					BatchElement.MinVertexIndex = Chunk.BaseVertexIndex;
					Mesh.UseDynamicData = FALSE;
					Mesh.ReverseCulling = (LocalToWorldDeterminant < 0.0f);
					Mesh.CastShadow = SectionElementInfo.bEnableShadowCasting;
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)DPGIndex;

					INT MaxNumPrimitives = appRound(((FLOAT)Section.NumTriangles)*Clamp<FLOAT>(SkeletalMeshComponent->ProgressiveDrawingFraction,0.f,1.f));

					INT BaseIndex = Section.BaseIndex;
					if( Section.TriangleSorting == TRISORT_CustomLeftRight && SkeletalMeshComponent->CustomSortAlternateIndexMode==CSAIM_Left )
					{
						BaseIndex += Section.NumTriangles*3;
					}

					if (PDI->IsHitTesting())
					{
						// Render hit proxies
						Mesh.bWireframe = FALSE;
						Mesh.MaterialRenderProxy = SectionElementInfo.Material->GetRenderProxy(bSelected);
						for (INT i=0;i<SortStripData.Num();i++)
						{
							if (LODIndex==ASV->SelectedSortStripLodIndex && SectionIndex==ASV->SelectedSortStripSectionIndex)
							{
								if (ASV->bSortStripMoveBackward && i >= ASV->SelectedSortStripIndex)
								{
									// If we're moving backwards, don't draw hit proxies for strips in front of the selected strip.
									continue;
								}
								else
								if (ASV->bSortStripMoveForward && i <= ASV->SelectedSortStripIndex)
								{
									// If we're moving forwards, don't draw hit proxies for strips behind the selected strip.
									continue;
								}
							}

							BatchElement.FirstIndex = SortStripData(i).FirstIndex;
							BatchElement.NumPrimitives = Min<INT>(SortStripData(i).NumTris, MaxNumPrimitives-((BatchElement.FirstIndex-BaseIndex)/3));

							if( BatchElement.NumPrimitives > 0 )
							{
								PDI->SetHitProxy(SortStripData(i).HitProxy);
								DrawRichMesh(PDI, Mesh, WireframeLinearColor, LevelColor, PropertyColor, PrimitiveSceneInfo, bSelected);
								PDI->SetHitProxy(NULL);
							}
						}
					}
					else
					if( ASV->SelectedSortStripIndex != INDEX_NONE && 
						ASV->SelectedSortStripLodIndex == LODIndex &&
						(ASV->SelectedSortStripSectionIndex == SectionIndex) )
					{
						FSortStripData* SortStripData = &VisualizeSortStripData(LODIndex)(SectionIndex)(ASV->SelectedSortStripIndex);

						// Re-draw the strip in wireframe.
						Mesh.bWireframe = TRUE;
						Mesh.MaterialRenderProxy = GEngine->WireframeMaterial->GetRenderProxy(FALSE);
						BatchElement.FirstIndex = SortStripData->FirstIndex;
						BatchElement.NumPrimitives = Min<INT>(SortStripData->NumTris, MaxNumPrimitives-((BatchElement.FirstIndex-BaseIndex)/3));

						if( BatchElement.NumPrimitives > 0 )
						{
							DrawRichMesh(PDI, Mesh, WireframeLinearColor, LevelColor, PropertyColor, PrimitiveSceneInfo,bSelected);
							// Draw backface
							Mesh.ReverseCulling = !Mesh.ReverseCulling;
							DrawRichMesh(PDI, Mesh, WireframeLinearColor, LevelColor, PropertyColor, PrimitiveSceneInfo,bSelected);

							// Draw the strip we want to move in front or behind of.
							if((ASV->bSortStripMoveForward||ASV->bSortStripMoveBackward) && ASV->MouseOverSortStripIndex != INDEX_NONE)
							{
								SortStripData = &VisualizeSortStripData(LODIndex)(SectionIndex)(ASV->MouseOverSortStripIndex);
								BatchElement.FirstIndex = SortStripData->FirstIndex;
								BatchElement.NumPrimitives = SortStripData->NumTris;
								DrawRichMesh(PDI, Mesh, FLinearColor(0.f,1.f,1.f), LevelColor, PropertyColor, PrimitiveSceneInfo,bSelected, SHOW_Wireframe);
								Mesh.ReverseCulling = !Mesh.ReverseCulling;
								DrawRichMesh(PDI, Mesh, FLinearColor(0.f,1.f,1.f), LevelColor, PropertyColor, PrimitiveSceneInfo,bSelected, SHOW_Wireframe);						
							}
						}
					}
				}
			}
		}
	}

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		// Display base pose for additive animations, to visualize deltas.
		WxAnimSetViewer* ASV = (WxAnimSetViewer*)SkeletalMeshComponent->AnimSetViewerPtr;

		// We don't want to draw the mesh geometry to the hit testing render target
		// so that we can get to triangle strips that are partially obscured by other
		// triangle strips easier.
		if (!PDI->IsHitTesting())
		{
			if( ASV && ASV->bShowAdditiveBase && ASV->SelectedAnimSeq && ASV->SelectedAnimSeq->bIsAdditive )
			{
				UAnimNodeSequence* AnimNodeSeq = ASV->PreviewAnimNode;
				if( AnimNodeSeq && AnimNodeSeq->AnimSeq == ASV->SelectedAnimSeq 
					&& AnimNodeSeq->SkelComponent == SkeletalMeshComponent
					&& ASV->SelectedAnimSet == AnimNodeSeq->AnimSeq->GetAnimSet() )
				{
					TArray<FMeshBone>& RefSkel = SkeletalMeshComponent->SkeletalMesh->RefSkeleton;

					UAnimSet* AnimSet = ASV->SelectedAnimSet;
					const INT AnimLinkupIndex = AnimNodeSeq->AnimLinkupIndex;

					check(AnimLinkupIndex != INDEX_NONE);
					check(AnimLinkupIndex < AnimSet->LinkupCache.Num());
					FAnimSetMeshLinkup* AnimLinkup = &AnimSet->LinkupCache(AnimLinkupIndex);
					check(AnimLinkup);

					const INT LODIndex = ::Clamp(SkeletalMeshComponent->PredictedLODLevel, 0, ASV->SelectedSkelMesh->LODModels.Num()-1);
					FStaticLODModel& LODModel = ASV->SelectedSkelMesh->LODModels( LODIndex );
					FColor SkeletonColor = FColor(192,64,255,255);

					TArray<FBoneAtom> SpaceBases, WorldBases;
					SpaceBases.Add( RefSkel.Num() );
					WorldBases.Add( RefSkel.Num() );

					for(INT i=0; i<LODModel.RequiredBones.Num(); i++)
					{
						const INT BoneIndex	= LODModel.RequiredBones(i);
						// Find which track in the sequence we look in for this bones data
						const INT	TrackIndex = AnimLinkup->BoneToTrackTable(BoneIndex);

						FBoneAtom LocalAtom;
						if( TrackIndex != INDEX_NONE )
						{
							ASV->SelectedAnimSeq->GetAdditiveBasePoseBoneAtom(LocalAtom, TrackIndex, AnimNodeSeq->CurrentTime, AnimNodeSeq->bLooping);
							// If doing 'rotation only' case, use ref pose for all non-root bones that are not in the BoneUseAnimTranslation array.
							if(	BoneIndex > 0 && ((AnimSet->bAnimRotationOnly && !AnimSet->BoneUseAnimTranslation(TrackIndex)) || AnimSet->ForceUseMeshTranslation(TrackIndex)) )
							{
								LocalAtom.SetTranslation(RefSkel(BoneIndex).BonePos.Position);
							}
						}
						else
						{
							LocalAtom.SetComponents(RefSkel(BoneIndex).BonePos.Orientation, RefSkel(BoneIndex).BonePos.Position);
						}

						// transform bone mats to world space
						if( BoneIndex > 0 )
						{
							// Apply quaternion fix for ActorX-exported quaternions.
							LocalAtom.FlipSignOfRotationW();
							const INT ParentIndex = ASV->SelectedSkelMesh->RefSkeleton(BoneIndex).ParentIndex;
							SpaceBases(BoneIndex) = LocalAtom * SpaceBases(ParentIndex);
						}
						else
						{
							SpaceBases(BoneIndex) = LocalAtom;
						}
						// Turn this from component space to world space.
						WorldBases(BoneIndex) = SpaceBases(BoneIndex) * SkeletalMeshComponent->LocalToWorldBoneAtom;

						// Draw only bones that haven't been masked out
						const FColor& BoneColor = RefSkel(BoneIndex).BoneColor;
						if( BoneColor.A != 0 )
						{
							if( BoneIndex > 0 )
							{
								const INT ParentIndex = RefSkel(BoneIndex).ParentIndex;
								PDI->DrawLine(WorldBases(BoneIndex).GetOrigin(), WorldBases(ParentIndex).GetOrigin(), SkeletonColor, SDPG_Foreground);
							}
							else
							{
								PDI->DrawLine(WorldBases(BoneIndex).GetOrigin(), SkeletalMeshComponent->LocalToWorld.GetOrigin(), FColor(255, 0, 255), SDPG_Foreground);
							}

							// Display colored coordinate system axes for each joint.			
							// Red = X
							FVector XAxis = WorldBases(BoneIndex).TransformNormal( FVector(1.0f,0.0f,0.0f));
							XAxis.Normalize();
							PDI->DrawLine( WorldBases(BoneIndex).GetOrigin(), WorldBases(BoneIndex).GetOrigin() + XAxis * 3.75f, FColor( 255, 80, 80), SDPG_Foreground );			
							// Green = Y
							FVector YAxis = WorldBases(BoneIndex).TransformNormal( FVector(0.0f,1.0f,0.0f));
							YAxis.Normalize();
							PDI->DrawLine( WorldBases(BoneIndex).GetOrigin(), WorldBases(BoneIndex).GetOrigin() + YAxis * 3.75f, FColor( 80, 255, 80), SDPG_Foreground ); 
							// Blue = Z
							FVector ZAxis = WorldBases(BoneIndex).TransformNormal( FVector(0.0f,0.0f,1.0f));
							ZAxis.Normalize();
							PDI->DrawLine( WorldBases(BoneIndex).GetOrigin(), WorldBases(BoneIndex).GetOrigin() + ZAxis * 3.75f, FColor( 80, 80, 255), SDPG_Foreground ); 
						}
					}
				}
			}

			if(SkeletalMeshComponent->bDrawMesh)
			{
				FSkeletalMeshSceneProxy::DrawDynamicElements(PDI, View, DPGIndex,Flags);
			}

			// Render a wireframe skeleton of the raw animation data.
			else if(this->bDisplayBones)
			{	
				INT LODIndex = GetCurrentLODIndex();
				const FStaticLODModel& LODModel = SkeletalMesh->LODModels(LODIndex);
				DebugDrawBones(PDI, View, SkeletalMeshComponent->SpaceBases, LODModel, BoneColor, SkeletalMeshComponent->ChunkIndexPreview);
			}

			if (SkeletalMeshComponent->BonesOfInterest.Num() == 1)
			{ 
				//Draw the bone of interest and its children
				INT LODIndex = GetCurrentLODIndex();
				const FStaticLODModel& LODModel = SkeletalMesh->LODModels(LODIndex);
				DebugDrawBonesSubset(PDI, View, SkeletalMeshComponent->SpaceBases, LODModel, SkeletalMeshComponent->BonesOfInterest, BoneColor, SkeletalMeshComponent->ChunkIndexPreview);
			}

			// Draw root motion delta for bones.
			if(bHasBoneLine && (this->bDisplayBones || SkeletalMeshComponent->bDrawMesh))
			{
				PDI->DrawLine(BoneRootMotionLine[0], BoneRootMotionLine[1], FColor(255, 0, 0), SDPG_Foreground);
			}

			if(bShowSoftBodyTetra)
			{
				DebugDrawSoftBodyTetras(PDI, View);
			}
		}

		// Draw sort strip visualization
		if( ASV->GetPreviewVC()->bTriangleSortMode )
		{
			if (PDI->IsHitTesting())
			{
				// By default hit proxy rendering has depth writes enabled. We disable it 
				// as want the hit proxy pixels to reflect the hair's draw order.
				RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
			}

			DrawSortStrip(PDI, View, DPGIndex, Flags, ASV);

			if (PDI->IsHitTesting())
			{
				// restore depth writes
				RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
			}
		}
	}

	/** Ensure its always in the foreground DPG. */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result = FSkeletalMeshSceneProxy::GetViewRelevance(View);
		Result.SetDPG(SDPG_Foreground,TRUE);
		// For sort strip wireframe rendering
		Result.bOpaqueRelevance = TRUE;
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FSkeletalMeshSceneProxy::GetAllocatedSize() ); }

private:
	/** Holds onto the skeletal mesh component that created it so it can handle the rendering of bones properly. */
	UASVSkelComponent* SkeletalMeshComponent;
	BITFIELD bHasBoneLine : 1;
	FVector BoneRootMotionLine[2];
	BITFIELD bShowSoftBodyTetra : 1;

	/** Sort strip visualization info */
	friend struct FASVViewportClient;
	TArray<TArray<TArray<FSortStripData> > > VisualizeSortStripData;
};

/*-----------------------------------------------------------------------------
FASVViewportClient
-----------------------------------------------------------------------------*/

static FVector2D ClampUVRange(FLOAT U, FLOAT V)
{
	// This handles both the + and - values correctly by putting all values between positive 0 and 1
	if(abs(U) > 1.0f)
	{
		U -= appFloor(U);
	}

	if(abs(V) > 1.0f)
	{
		V -= appFloor(V);
	}

	return FVector2D(U, V);
}

void FASVViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	USkeletalMeshComponent* MeshComp = AnimSetViewer->PreviewSkelComp;
	USkeletalMesh*			SkelMesh = MeshComp ? MeshComp->SkeletalMesh : NULL;

	// mark this mesh component so that the renderer is able to highlight any selection sections
	if( MeshComp )
	{
		MeshComp->bCanHighlightSelectedSections = TRUE;
	}

	// Do main viewport drawing stuff.
	FEditorLevelViewportClient::Draw(Viewport, Canvas);

	// Clear out the lines from the previous frame
	PreviewScene.ClearLineBatcher();
	ULineBatchComponent* LineBatcher = PreviewScene.GetLineBatcher();

	INT XL, YL;
	StringSize( GEngine->SmallFont,  XL, YL, TEXT("L") );

	FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
	FSceneView* View = CalcSceneView(&ViewFamily);

	const INT HalfX = Viewport->GetSizeX()/2;
	const INT HalfY = Viewport->GetSizeY()/2;

	check(AnimSetViewer->PreviewSkelComp);
	const INT LODIndex = ::Clamp(AnimSetViewer->PreviewSkelComp->PredictedLODLevel, 0, AnimSetViewer->SelectedSkelMesh->LODModels.Num()-1);

	FStaticLODModel& LODModel = AnimSetViewer->SelectedSkelMesh->LODModels( LODIndex );
	const FSkelMeshComponentLODInfo& MeshLODInfo = AnimSetViewer->PreviewSkelComp->LODInfo( LODIndex );

#if WITH_SIMPLYGON
	if ( AnimSetViewer->SelectedSkelMesh->bHasBeenSimplified && SimplygonLogo && SimplygonLogo->Resource )
	{
		const FLOAT LogoSizeX = 128.0f;
		const FLOAT LogoSizeY = 128.0f;
		const FLOAT PaddingX = 6.0f;
		const FLOAT PaddingY = -18.0f;
		const FLOAT LogoX = Viewport->GetSizeX() - PaddingX - LogoSizeX;
		const FLOAT LogoY = PaddingY;

		DrawTile(
			Canvas,
			LogoX,
			LogoY,
			LogoSizeX,
			LogoSizeY,
			0.0f,
			0.0f,
			1.0f,
			1.0f,
			FLinearColor::White,
			SimplygonLogo->Resource,
			SE_BLEND_Translucent );
	}
#endif // #if WITH_SIMPLYGON

	// Make sure the textures for the mesh component are always resident while viewing the mesh in the animset viewer. Note that we do not
	// block and update all resources here, so on the initial drawing in the viewer, the textures will have to stream in.
	if ( MeshComp )
	{
		MeshComp->PrestreamTextures( 30.0f, FALSE );
	}

	if( bShowBoneNames && SkelMesh )
	{
		for(INT i=0; i< LODModel.RequiredBones.Num(); i++)
		{
			const INT BoneIndex	= LODModel.RequiredBones(i);

			// If previewing a specific chunk, only show the bone names that belong to it
			if ((MeshComp->ChunkIndexPreview >= 0) && !LODModel.Chunks(MeshComp->ChunkIndexPreview).BoneMap.ContainsItem(BoneIndex))
			{
				continue;
			}

			FColor& BoneColor = SkelMesh->RefSkeleton(BoneIndex).BoneColor;
			if( BoneColor.A != 0 )
			{
				const FVector BonePos = MeshComp->LocalToWorld.TransformFVector(MeshComp->SpaceBases(BoneIndex).GetOrigin());

				const FPlane proj = View->Project(BonePos);
				if( proj.W > 0.f )
				{
					const INT XPos = HalfX + ( HalfX * proj.X );
					const INT YPos = HalfY + ( HalfY * (proj.Y * -1) );

					const FName BoneName		= SkelMesh->RefSkeleton(BoneIndex).Name;
					const FString BoneString	= FString::Printf( TEXT("%d: %s"), BoneIndex, *BoneName.ToString() );

					DrawString(Canvas,XPos, YPos, *BoneString, GEngine->SmallFont, BoneColor);
				}
			}
		}
	}

	// Draw socket names if desired.
	if( bShowSockets  || AnimSetViewer->SocketMgr )
	{
		for(INT i=0; i<AnimSetViewer->SelectedSkelMesh->Sockets.Num(); i++)
		{
			USkeletalMeshSocket* Socket = AnimSetViewer->SelectedSkelMesh->Sockets(i);

			FMatrix SocketTM;
			if( Socket->GetSocketMatrix(SocketTM, MeshComp) )
			{
				const FVector SocketPos	= SocketTM.GetOrigin();
				const FPlane proj		= View->Project( SocketPos );
				if(proj.W > 0.f)
				{
					const INT XPos = HalfX + ( HalfX * proj.X );
					const INT YPos = HalfY + ( HalfY * (proj.Y * -1) );
					DrawString(Canvas, XPos, YPos, *Socket->SocketName.ToString(), GEngine->SmallFont, FColor(255,196,196));

					if (bManipulating && AnimSetViewer->SelectedSocket == Socket)
					{
						//Figure out the text height
						FTextSizingParameters Parameters(GEngine->SmallFont, 1.0f, 1.0f);
						UCanvas::CanvasStringSize(Parameters, *Socket->SocketName.ToString());
						INT YL = appTrunc(Parameters.DrawYL);

						DrawAngles(Canvas, XPos, YPos + YL, 
									SocketManipulateAxis, AnimSetViewer->SocketMgr->MoveMode, 
									AnimSetViewer->SelectedSocket->RelativeRotation,
									AnimSetViewer->SelectedSocket->RelativeLocation);
					}
				}
			}
		}
	}

	// Draw manipulation angles for bones
	if (bManipulating && AnimSetViewer->PreviewSkelComp->BonesOfInterest.Num() == 1)
	{
		FSkelControlListHead& SkelListControl = AnimSetViewer->PreviewAnimTree->SkelControlLists(0);
		USkelControlSingleBone* SkelControl = Cast<USkelControlSingleBone>(SkelListControl.ControlHead);
		if (SkelControl)
		{
			FMatrix BoneMatrix = AnimSetViewer->PreviewSkelComp->GetBoneMatrix(AnimSetViewer->PreviewSkelComp->BonesOfInterest(0));
			const FPlane proj = View->Project(BoneMatrix.GetOrigin());
			if( proj.W > 0.f )
			{
				const INT XPos = HalfX + ( HalfX * proj.X );
				const INT YPos = HalfY + ( HalfY * (proj.Y * -1) );

				DrawAngles(Canvas, XPos, YPos, BoneManipulateAxis, AnimSetViewer->BoneMoveMode, SkelControl->BoneRotation, SkelControl->BoneTranslation);
			}
		}
	}

	// Draw stats about the mesh
	const FBoxSphereBounds& SkelBounds = AnimSetViewer->PreviewSkelComp->Bounds;
	const FPlane ScreenPosition = View->Project(SkelBounds.Origin);
	const FLOAT ScreenRadius = Max((FLOAT)HalfX * View->ProjectionMatrix.M[0][0], (FLOAT)HalfY * View->ProjectionMatrix.M[1][1]) * SkelBounds.SphereRadius / Max(ScreenPosition.W,1.0f);
	const FLOAT LODFactor = ScreenRadius / 320.0f;

	INT NumBonesInUse;
	INT NumChunksInUse;
	INT NumSectionsInUse;
	FString WeightUsage;

	// for full weight swap show new section/chunk info instead of base info
	if (AnimSetViewer != NULL && 
		AnimSetViewer->bPreviewInstanceWeights && 
		MeshLODInfo.InstanceWeightUsage == IWU_FullSwap &&
		LODModel.VertexInfluences.Num() > 0)
	{
		NumBonesInUse  = LODModel.VertexInfluences(0).RequiredBones.Num();
		NumChunksInUse =  LODModel.VertexInfluences(0).Chunks.Num();
		NumSectionsInUse = LODModel.VertexInfluences(0).Sections.Num();
		WeightUsage = LocalizeUnrealEd("ToggleMeshWeightsFull");
	}
	else
	{
		NumBonesInUse = LODModel.RequiredBones.Num();
		NumChunksInUse = LODModel.Chunks.Num();
		NumSectionsInUse = LODModel.Sections.Num();
	}

	FString InfoString = FString::Printf( TEXT("LOD [%d] Bones:%d Polys:%d (Display Factor:%3.2f, FOV:%3.0f)"), 
		LODIndex, 
		NumBonesInUse,	   
		LODModel.GetTotalFaces(), 
		LODFactor,
		ViewFOV);

	INT CurYOffset=5;
	DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );

	CurYOffset += 1; // --


	UINT TotalRigidVertices = 0;
	UINT TotalSoftVertices = 0;
	for(INT ChunkIndex = 0;ChunkIndex < LODModel.Chunks.Num();ChunkIndex++)
	{
		INT ChunkRigidVerts = LODModel.Chunks(ChunkIndex).GetNumRigidVertices();
		INT ChunkSoftVerts = LODModel.Chunks(ChunkIndex).GetNumSoftVertices();
		INT ClothingVertices = 0;
#if WITH_APEX_CLOTHING
		if(MeshComp->ApexClothing)
		{
			ClothingVertices = MeshComp->GetApexClothingNumVertices(LODIndex, ChunkIndex);
			if(ClothingVertices > 0)
			{
				ChunkRigidVerts = ChunkSoftVerts = 0;
			}
		}
#endif		
		InfoString = FString::Printf( TEXT(" [Chunk %d] Verts:%d (Rigid:%d Soft:%d Clothing: %d)"), 
			ChunkIndex,
			ChunkRigidVerts + ChunkSoftVerts,
			ChunkRigidVerts,
			ChunkSoftVerts,
			ClothingVertices);
		CurYOffset += YL + 2;
		DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(200,200,200) );

		TotalRigidVertices += ChunkRigidVerts;
		TotalSoftVertices += ChunkSoftVerts;
	}

	InfoString = FString::Printf( TEXT("TOTAL Verts:%d (Rigid:%d Soft:%d)"), 
		LODModel.NumVertices,
		TotalRigidVertices,
		TotalSoftVertices );

	CurYOffset += 1; // --


	CurYOffset += YL + 2;
	DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );

	InfoString = FString::Printf( TEXT("Chunks:%d Sections:%d %s"), 
		NumChunksInUse, 
		NumSectionsInUse,
		*WeightUsage
		);

	CurYOffset += YL + 2;
	DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );

	if ( AnimSetViewer->VertexInfo.bShowNormal || AnimSetViewer->VertexInfo.bShowTangent )
	{
		if (AnimSetViewer->VertexInfo.SelectedVertexIndex != 0)
		{
			InfoString = FString::Printf( TEXT("Selected Vertex Index:%d Vertex Position: %s"), 
					AnimSetViewer->VertexInfo.SelectedVertexIndex, 
					*AnimSetViewer->VertexInfo.SelectedVertexPosition.ToString());

			CurYOffset += YL + 2;
			DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );

			INT SelectedChunkIndex=GetChunkIndexFromMaterialIndex(AnimSetViewer->PreviewSkelComp, AnimSetViewer->VertexInfo.SelectedMaterialIndex);	
			const WORD* RESTRICT BoneMap = LODModel.Chunks(SelectedChunkIndex).BoneMap.GetTypedData();

			for ( INT I=0; I<AnimSetViewer->VertexInfo.BoneIndices.Num(); ++I )
			{
				INT BoneIndex = BoneMap[AnimSetViewer->VertexInfo.BoneIndices(I)];
				FMatrix TM = AnimSetViewer->PreviewSkelComp->GetBoneMatrix(BoneIndex);
				InfoString = FString::Printf( TEXT("Bone (%s) Weight (%0.2f) [%s]"),*AnimSetViewer->SelectedSkelMesh->RefSkeleton(BoneIndex).Name.ToString(), AnimSetViewer->VertexInfo.BoneWeights(I), *TM.ToString() );

				CurYOffset += YL + 2;
				DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );
			}
		}
	}

	if( AnimSetViewer->SelectedAnimSeq && AnimSetViewer->PreviewAnimNode && LODModel.RequiredBones.Num() > 0 )
	{
		const INT RootBoneIndex	= LODModel.RequiredBones(0);

		if( MeshComp && MeshComp->SpaceBases.Num() > RootBoneIndex )
		{
			const UAnimSequence* AnimSeq = AnimSetViewer->SelectedAnimSeq;
			UAnimSet* AnimSet = AnimSeq->GetAnimSet();
			check(AnimSet);
			if( AnimSetViewer->PreviewAnimNode->AnimLinkupIndex != INDEX_NONE )
			{
				FAnimSetMeshLinkup* AnimLinkup = &AnimSet->LinkupCache(AnimSetViewer->PreviewAnimNode->AnimLinkupIndex);
				check(AnimLinkup);
				const INT RootBoneTrackIndex = AnimLinkup->BoneToTrackTable(RootBoneIndex);
				if(RootBoneTrackIndex != INDEX_NONE)
				{
					FMemMark Mark(GMainThreadMemStack);
					FBoneAtom FirstFrameAtom;

					// I just like to get current curvekeys and apply
					FCurveKeyArray CurveKeys;
					AnimSeq->GetBoneAtom(FirstFrameAtom, RootBoneTrackIndex, 0.f, FALSE, MeshComp->bUseRawData);

					FBoneAtom CurrentFrameAtom;
					AnimSeq->GetBoneAtom(CurrentFrameAtom, RootBoneTrackIndex, AnimSetViewer->PreviewAnimNode->CurrentTime, FALSE, MeshComp->bUseRawData, &CurveKeys);

					MeshComp->ApplyCurveKeys(CurveKeys);

					const FVector RootBoneDelta = MeshComp->LocalToWorld.TransformNormal(CurrentFrameAtom.GetTranslation() - FirstFrameAtom.GetTranslation());
					InfoString = FString::Printf( TEXT("Root Bone Delta: %s"), *RootBoneDelta.ToString());

					CurYOffset += YL + 2;
					DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );


					// Print the number of keyframes in the current animation
					FLOAT SamplingRate = 0.f;
					if(AnimSeq->SequenceLength > 0)
					{
						SamplingRate = static_cast<FLOAT>(AnimSeq->NumFrames) / AnimSeq->SequenceLength;
					}
					InfoString = FString::Printf( TEXT("KeyFrame Count: %i (%.4g keys / second)"), AnimSeq->NumFrames, SamplingRate);

					CurYOffset += YL + 2;
					DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );

					Mark.Pop();
				}
			}
		}

		if( AnimSetViewer->PreviewSkelComp != NULL )
		{
			const UAnimSequence* AnimSeq = AnimSetViewer->SelectedAnimSeq;
			UAnimSet* AnimSet = AnimSeq->GetAnimSet();
			check(AnimSet);
			if( AnimSetViewer->PreviewAnimNode->AnimLinkupIndex != INDEX_NONE )
			{
				FAnimSetMeshLinkup* AnimLinkup = &AnimSet->LinkupCache(AnimSetViewer->PreviewAnimNode->AnimLinkupIndex);
				check(AnimLinkup);

				const UASVSkelComponent* PrevSkelComp = AnimSetViewer->PreviewSkelComp;
				for( INT InterestIdx = 0; InterestIdx < PrevSkelComp->BonesOfInterest.Num(); InterestIdx++ )
				{
					const INT BoneIdx = PrevSkelComp->BonesOfInterest(InterestIdx);

					const INT BoneTrackIdx = AnimLinkup->BoneToTrackTable(BoneIdx);
					if( BoneTrackIdx != INDEX_NONE )
					{
						if( BoneIdx > INDEX_NONE && BoneIdx < PrevSkelComp->SpaceBases.Num() )
						{
							FMemMark Mark(GMainThreadMemStack);

							FName BoneName = (PrevSkelComp->SkeletalMesh != NULL && PrevSkelComp->SkeletalMesh->RefSkeleton.IsValidIndex(BoneIdx)) ? PrevSkelComp->SkeletalMesh->RefSkeleton(BoneIdx).Name : NAME_None;

							FBoneAtom CurrentFrameAtom;
							FCurveKeyArray CurveKeys;
							AnimSeq->GetBoneAtom(CurrentFrameAtom, BoneTrackIdx, AnimSetViewer->PreviewAnimNode->CurrentTime, FALSE, MeshComp->bUseRawData, &CurveKeys);

							MeshComp->ApplyCurveKeys(CurveKeys);

							// would like to show relative to root, so will need to apply parent space base, otherwise, it's all local atom
							INT ParentIndex = (PrevSkelComp->SkeletalMesh != NULL && PrevSkelComp->SkeletalMesh->RefSkeleton.IsValidIndex(BoneIdx)) ? PrevSkelComp->SkeletalMesh->RefSkeleton(BoneIdx).ParentIndex : INDEX_NONE;
							if (ParentIndex !=INDEX_NONE)
							{
								CurrentFrameAtom = CurrentFrameAtom*PrevSkelComp->SpaceBases(ParentIndex);
							}

							const FVector BoneDelta = MeshComp->LocalToWorld.TransformNormal(CurrentFrameAtom.GetTranslation() - PrevSkelComp->SpaceBases(LODModel.RequiredBones(0)).GetOrigin());

							InfoString = FString::Printf( TEXT("%s -- Bone Delta: %s"), *BoneName.ToString(), *BoneDelta.ToString() );
							CurYOffset += YL + 2;
							DrawString( Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );

							InfoString = FString::Printf( TEXT("%s -- Bone Rotation: %s"), *BoneName.ToString(), *CurrentFrameAtom.GetRotation().Rotator().GetNormalized().ToString() );
							CurYOffset += YL + 2;
							DrawString( Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );
							Mark.Pop();
						}
					}
				}
			}
		}
	}

	// If bAnimRotationOnly has been changed, display warning that the sequence needs to be recompressed
	if( AnimSetViewer->SelectedAnimSeq && AnimSetViewer->SelectedAnimSet && (AnimSetViewer->SelectedAnimSeq->bWasCompressedWithoutTranslations != AnimSetViewer->SelectedAnimSet->bAnimRotationOnly))
	{
		InfoString = FString::Printf( TEXT("Warning: WILL BE BROKEN IN GAME!!!  %s needs to be recompressed; displaying RAW animation data only"), *AnimSetViewer->SelectedAnimSeq->SequenceName.ToString());

		CurYOffset += YL + 2;
		DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,0,0) );
	}

	// Draw anim notify viewer.
	if( AnimSetViewer->SelectedAnimSeq )
	{
		const INT NotifyViewBorderX = 10;
		const INT NotifyViewBorderY = 10;
		const INT NotifyViewEndHeight = 10;

		const INT SizeX = Viewport->GetSizeX();
		const INT SizeY = Viewport->GetSizeY();

		const UAnimSequence* AnimSeq = AnimSetViewer->SelectedAnimSeq;

		const FLOAT PixelsPerSecond = (FLOAT)(SizeX - (2*NotifyViewBorderX))/AnimSeq->SequenceLength;

		// Draw ends of track
		DrawLine2D(Canvas, FVector2D(NotifyViewBorderX, SizeY - NotifyViewBorderY - NotifyViewEndHeight), FVector2D(NotifyViewBorderX, SizeY - NotifyViewBorderY), FColor(255,255,255) );
		DrawLine2D(Canvas, FVector2D(SizeX - NotifyViewBorderX, SizeY - NotifyViewBorderY - NotifyViewEndHeight), FVector2D(SizeX - NotifyViewBorderX, SizeY - NotifyViewBorderY), FColor(255,255,255) );

		// Draw track itself
		const INT TrackY = SizeY - NotifyViewBorderY - (0.5f*NotifyViewEndHeight);
		DrawLine2D(Canvas, FVector2D(NotifyViewBorderX, TrackY), FVector2D(SizeX - NotifyViewBorderX, TrackY), FColor(255,255,255) );

		// Draw notifies on the track
		TArray<TrackSlots> Tracks;
		INT NotifyY = TrackY;
		for(INT i=0; i<AnimSeq->Notifies.Num(); i++)
		{
			const FAnimNotifyEvent& NotifyEvent = AnimSeq->Notifies(i);

			INT NotifyX = NotifyViewBorderX + (PixelsPerSecond * NotifyEvent.Time);

			// Get color from notify object, if present
			FColor NotifyColor = (NotifyEvent.Notify) ? NotifyEvent.Notify->GetEditorColor() : FColor(255,200,200);

			// Combine comment from notify struct and from function on object
			FString NotifyComment;
			if (NotifyEvent.Comment != NAME_None)
			{
				NotifyComment = FString::Printf(TEXT("[%s]"),*NotifyEvent.Comment.ToString());
			}
			if(NotifyEvent.Notify)
			{
				NotifyComment += NotifyEvent.Notify->GetEditorComment();
			}

			// guarantee some sort of label
			if( NotifyComment.Len() == 0 )
			{
				NotifyComment = TEXT("??");
			}
			// do the deed
			{
				INT W, H;
				// figure out what height to draw this notify at
				StringSize(GEngine->SmallFont, W, H, *NotifyComment);
				
				INT LabelX = Clamp(NotifyX - W/2,NotifyViewBorderX,SizeX-NotifyViewBorderX-W);
				INT TrackIdx = -1;

				INT TrackW = W;
				if( NotifyEvent.Duration > 0.f )
				{
					// Double track with for extra label
					TrackW *= 2.f;
					// Stretch track for the duration
					TrackW += (NotifyEvent.Duration / AnimSeq->SequenceLength) * (SizeX - (2.f * NotifyViewBorderX));
				}
				for (INT Idx = 0; Idx < Tracks.Num(); Idx++)
				{
					if (Tracks(Idx).AddSlot(LabelX,TrackW))
					{
						TrackIdx = Idx;
						break;
					}
				}
				if (TrackIdx == -1)
				{
					// add new track, no room on existing ones
					TrackIdx = Tracks.AddItem(TrackSlots());
					Tracks(TrackIdx).AddSlot(LabelX,TrackW);
				}
				NotifyY = TrackY - ((H + 8) * (TrackIdx + 1));
				
				// draw a line from the track to see the actual position
				DrawLine2D(Canvas, FVector2D(NotifyX, NotifyY + H), FVector2D(NotifyX, TrackY), NotifyColor );
				// draw a box and comment with a hit proxy for time dragging
				if (Canvas->IsHitTesting())	{Canvas->SetHitProxy(new HNotifyProxy(i));}
					DrawTile(Canvas,LabelX-2,NotifyY-2,W+2,H+2,0.f,0.f,1.f,1.f,FLinearColor(0.05f,0.05f,0.05f,1.f));
					DrawBox2D(Canvas,FVector2D( LabelX - 2, NotifyY - 2 ),FVector2D( LabelX + W + 1, NotifyY + H + 1 ),NotifyColor );
					DrawString(Canvas, LabelX, NotifyY, *NotifyComment, GEngine->SmallFont, NotifyColor );
					// if we've got a duration then draw a matching notifying widget
					if (NotifyEvent.Duration > 0.f)
					{
						INT RightEdge = LabelX + W + 2;
						NotifyX = NotifyViewBorderX + (PixelsPerSecond * (NotifyEvent.Time + NotifyEvent.Duration));
						LabelX = Clamp(NotifyX - W/2,NotifyViewBorderX,SizeX-NotifyViewBorderX-W);
						// draw a line connecting the two boxes
						DrawLine2D(Canvas,FVector2D(LabelX-2,NotifyY + H/2),FVector2D(RightEdge,NotifyY + H/2),NotifyColor);
						if (Canvas->IsHitTesting())	{Canvas->SetHitProxy(new HNotifyProxy(i,TRUE));}
						DrawBox2D(Canvas,FVector2D( LabelX - 2, NotifyY - 2 ),FVector2D( LabelX + W + 1, NotifyY + H + 1 ),NotifyColor );
						DrawString(Canvas, LabelX, NotifyY, *NotifyComment, GEngine->SmallFont, NotifyColor );
						DrawLine2D(Canvas, FVector2D(NotifyX, NotifyY + H), FVector2D(NotifyX, TrackY), NotifyColor );
					}
				if (Canvas->IsHitTesting())	{Canvas->SetHitProxy(NULL);}
			}
		}
		
		// Bone Control Modifiers are displayed in animset viewer 
		// Using same track slot with notifier, and display which bone control and what is strength
		for(INT MetadataIndex=0; MetadataIndex<AnimSeq->MetaData.Num(); MetadataIndex++)
		{
			UAnimMetaData* Metadata = AnimSeq->MetaData(MetadataIndex);
			UAnimMetaData_SkelControlKeyFrame* MetadataKeyFrame = Cast<UAnimMetaData_SkelControlKeyFrame>(Metadata);
			if( MetadataKeyFrame )
			{
				for (INT KeyIndex=0; KeyIndex<MetadataKeyFrame->KeyFrames.Num(); KeyIndex++)
				{
					FTimeModifier& Key = MetadataKeyFrame->KeyFrames(KeyIndex);
					const INT NotifyX = NotifyViewBorderX + (PixelsPerSecond * Key.Time);

					// Get color from notify object, if present
					FColor NotifyColor = FColor(112,154,209);

					// Combine comment from notify struct and from function on object
					FString BoneControlName;
					if( MetadataKeyFrame->SkelControlNameList.Num() > 0 )
					{
						BoneControlName = FString::Printf( TEXT("%s"), (MetadataKeyFrame->SkelControlNameList(0)==NAME_None)?TEXT("None"): *MetadataKeyFrame->SkelControlNameList(0).GetNameString());
						for(INT NameIdx=1; NameIdx<MetadataKeyFrame->SkelControlNameList.Num(); NameIdx++)
						{
							BoneControlName = FString::Printf( TEXT("%s,%s"), *BoneControlName, (MetadataKeyFrame->SkelControlNameList(NameIdx)==NAME_None)?TEXT("None"): *MetadataKeyFrame->SkelControlNameList(NameIdx).GetNameString());
						}
					}
					else
					{
						BoneControlName = TEXT("None");
					}
					FString NotifyComment = FString::Printf(TEXT("[%s:%0.2f]"),*BoneControlName, Key.TargetStrength);

					// do the deed
					{
						INT W, H;
						// figure out what height to draw this notify at
						StringSize(GEngine->SmallFont, W, H, *NotifyComment);
						INT LabelX = Clamp(NotifyX - W/2,NotifyViewBorderX,SizeX-NotifyViewBorderX-W);
						INT TrackIdx = -1;
						for (INT Idx = 0; Idx < Tracks.Num(); Idx++)
						{
							if (Tracks(Idx).AddSlot(LabelX,W))
							{
								TrackIdx = Idx;
								break;
							}
						}
						if (TrackIdx == -1)
						{
							// add new track, no room on existing ones
							TrackIdx = Tracks.AddItem(TrackSlots());
							Tracks(TrackIdx).AddSlot(LabelX,W);
						}
						NotifyY = TrackY - ((H + 8) * (TrackIdx + 1));

						// draw a line from the track to see the actual position
						DrawLine2D(Canvas, FVector2D(NotifyX, NotifyY + H), FVector2D(NotifyX, TrackY), NotifyColor );
						// draw a box and comment with a hit proxy for time dragging
						if (Canvas->IsHitTesting())	{Canvas->SetHitProxy(new HBoneControlProxy(MetadataIndex, KeyIndex));}
						DrawTile(Canvas,LabelX-2,NotifyY-2,W+2,H+2,0.f,0.f,1.f,1.f,FLinearColor(0.05f,0.05f,0.05f,1.f));
						DrawBox2D(Canvas,FVector2D( LabelX - 2, NotifyY - 2 ),FVector2D( LabelX + W + 1, NotifyY + H + 1 ),NotifyColor );
						DrawString(Canvas, LabelX, NotifyY, *NotifyComment, GEngine->SmallFont, NotifyColor );
						if (Canvas->IsHitTesting())	{Canvas->SetHitProxy(NULL);}
					}
				}
			}
		}

		// Draw current position on the track.
		if (Canvas->IsHitTesting())	{Canvas->SetHitProxy(new HTrackPosProxy());}
		const INT CurrentPosX = NotifyViewBorderX + (PixelsPerSecond * AnimSetViewer->PreviewAnimNode->CurrentTime);
			DrawTriangle2D(Canvas, 
				FVector2D(CurrentPosX, TrackY-1), FVector2D(0.f, 0.f), 
				FVector2D(CurrentPosX+5, TrackY-6), FVector2D(0.f, 0.f), 
				FVector2D(CurrentPosX-5, TrackY-6), FVector2D(0.f, 0.f), 
				bDraggingTrackPos ? FColor(200,0,200) : FColor(255,255,255) );
		if (Canvas->IsHitTesting())	{Canvas->SetHitProxy(NULL);}

		INT XS, YS;
		// Draw morph curve if show morph curve on and if curve data exists
		if ( bShowMorphKeys && AnimSeq->CurveData.Num() && AnimSetViewer->PreviewAnimNode )
		{
			FMemMark Mark(GMainThreadMemStack);

			FCurveKeyArray CurveKeys;
			AnimSeq->GetCurveData(AnimSetViewer->PreviewAnimNode->CurrentTime, AnimSetViewer->PreviewAnimNode->bLooping, CurveKeys);

			for ( INT CurveIdx = 0; CurveIdx < CurveKeys.Num(); ++CurveIdx )
			{
				UMorphTarget * MorphTarget = AnimSetViewer->PreviewSkelComp->FindMorphTarget(CurveKeys(CurveIdx).CurveName);
				INT NumOfVertices = 0;
				if (MorphTarget)
				{
					const INT LODIndex = ::Clamp(AnimSetViewer->PreviewSkelComp->PredictedLODLevel, 0, AnimSetViewer->SelectedSkelMesh->LODModels.Num()-1);
					const FMorphTargetLODModel & MorphLOD = MorphTarget->MorphLODModels(LODIndex);
					NumOfVertices = MorphLOD.Vertices.Num();
				}
				InfoString = FString::Printf(TEXT("Morph Target Name: %s, Weight: %0.4f, Verts:%d"), 
					*CurveKeys(CurveIdx).CurveName.GetNameString(), 
					CurveKeys(CurveIdx).Weight, 
					NumOfVertices);

				StringSize(GEngine->SmallFont, XS, YS, *InfoString);

				DrawString(Canvas,SizeX - XS - XL, (YS + 5.f)*CurveIdx + YL, *InfoString, GEngine->SmallFont, FColor(200,10,10));
			}
			Mark.Pop();
		}

		// Draw anim position
		InfoString = FString::Printf(TEXT("Pos: %3.1f pct, Time: %4.4fs, Len: %4.4fs"), 
						100.f*AnimSetViewer->PreviewAnimNode->CurrentTime/AnimSeq->SequenceLength, 
						AnimSetViewer->PreviewAnimNode->CurrentTime, 
						AnimSeq->SequenceLength);

		StringSize(GEngine->SmallFont, XS, YS, *InfoString);
		DrawString(Canvas,SizeX - XS - XL - NotifyViewBorderX, SizeY - YS - YL - NotifyViewBorderY - NotifyViewEndHeight, *InfoString, GEngine->SmallFont, FColor(255,255,255));

		// Draw if it's an ADDITIVE animation
		if( AnimSetViewer->SelectedAnimSeq->bIsAdditive )
		{
			InfoString = FString::Printf(TEXT("ADDITIVE ANIMATION (Reference: %s)"), *AnimSetViewer->SelectedAnimSeq->AdditiveRefName.ToString());
			CurYOffset += YL + 2;
			DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,0,0) );
		}
		else if( AnimSetViewer->SelectedAnimSeq->RelatedAdditiveAnimSeqs.Num() > 0)
		{
			// list out additive animation that has been created
			InfoString = FString::Printf(TEXT("Base/Target pose for the following additive animation(s) : "));
			CurYOffset += YL + 2;
			DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(180,0,0) );
			for (INT I=0; I<AnimSetViewer->SelectedAnimSeq->RelatedAdditiveAnimSeqs.Num(); ++I)
			{
				InfoString = FString::Printf(TEXT("%s:%s"), *AnimSetViewer->SelectedAnimSeq->GetAnimSet()->GetPathName(),  
					*AnimSetViewer->SelectedAnimSeq->RelatedAdditiveAnimSeqs(I)->SequenceName.GetNameString());
				CurYOffset += YL + 2;
				DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(180,0,0) );
			}
		}
	}

	// If doing cloth sim, show wind direction
	if(AnimSetViewer->PreviewSkelComp->bEnableClothSimulation)
	{
		INT NumClothVerts = AnimSetViewer->SelectedSkelMesh->ClothToGraphicsVertMap.Num();
		INT NumFixedClothVerts = NumClothVerts - AnimSetViewer->SelectedSkelMesh->NumFreeClothVerts;

		InfoString = FString::Printf( TEXT("Cloth Verts:%d (%d Fixed)"), 
			NumClothVerts, 
			NumFixedClothVerts );

		CurYOffset += YL + 2;
		DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );
	}

	if(AnimSetViewer->PreviewSkelComp->bEnableClothSimulation || AnimSetViewer->PreviewSkelComp->GetApexClothing() != NULL)
	{
		DrawWindDir(Viewport, Canvas);
	}

		
	if(AnimSetViewer->SelectedSkelMesh && AnimSetViewer->SelectedSkelMesh->SoftBodySurfaceToGraphicsVertMap.Num() > 0)
	{
		INT NumSurfaceVerts	= AnimSetViewer->SelectedSkelMesh->SoftBodySurfaceToGraphicsVertMap.Num();
		INT NumSurfaceTris	= AnimSetViewer->SelectedSkelMesh->SoftBodySurfaceIndices.Num() / 3;
		INT NumTetraVerts	= AnimSetViewer->SelectedSkelMesh->SoftBodyTetraVertsUnscaled.Num();				
		INT NumTetras		= AnimSetViewer->SelectedSkelMesh->SoftBodyTetraIndices.Num() / 4;		

		InfoString = FString::Printf( TEXT("Soft-Body Surface Triangles: %d, Surface Verts: %d, Tetras: %d, Tetra Verts: %d"), 
			NumSurfaceTris, NumSurfaceVerts, NumTetras, NumTetraVerts);

		CurYOffset += YL + 2;
		DrawString(Canvas, 5, CurYOffset, *InfoString, GEngine->SmallFont, FColor(255,255,255) );
	}

	if(bTriangleSortMode)
	{
		if( AnimSetViewer->SelectedSortStripIndex == INDEX_NONE )
		{
			// Check if we have a TRISORT_Custom section.
			UBOOL bFoundCustom=FALSE;
			for( INT LodIndex=0;LodIndex<AnimSetViewer->SelectedSkelMesh->LODModels.Num();LodIndex++ )
			{
				FStaticLODModel& LODModel = AnimSetViewer->SelectedSkelMesh->LODModels(LodIndex);
				for( INT SectionIndex=0;SectionIndex < LODModel.Sections.Num();SectionIndex++ )
				{
					FSkelMeshSection& Section = LODModel.Sections(SectionIndex);
					if( Section.TriangleSorting == TRISORT_Custom || Section.TriangleSorting == TRISORT_CustomLeftRight )
					{
						bFoundCustom = TRUE;
						break;
					}
				}
				if( bFoundCustom )
				{
					break;
				}
			}
			InfoString = bFoundCustom ? LocalizeUnrealEd("ClickToSelectSortStrip") : LocalizeUnrealEd("NoCustomSortSections");
		}
		else
		{
			if( AnimSetViewer->bSortStripMoveForward )
			{
				InfoString = AnimSetViewer->MouseOverSortStripIndex==INDEX_NONE ?
					LocalizeUnrealEd("SelectStripToMoveInFrontOf") : LocalizeUnrealEd("ClickToMoveInFront");
			}
			else
			if( AnimSetViewer->bSortStripMoveBackward )
			{
				InfoString = AnimSetViewer->MouseOverSortStripIndex==INDEX_NONE ?
					LocalizeUnrealEd("SelectStripToMoveBehind") : LocalizeUnrealEd("ClickToMoveBehind");
			}
			else
			{
				InfoString = LocalizeUnrealEd("PressBorFToSort");
			}
		}

		// Draw the appropriate text for the current triangle sorting operation.
		DrawString(Canvas, 5, Viewport->GetSizeY() - 80, *InfoString, GEngine->SmallFont, FColor(255,255,0) );
	}

	if( AnimSetViewer->UVSetToDisplay != -1 )
	{
		CurYOffset += YL + 2;
		DrawString(Canvas, 5, CurYOffset, *FString::Printf( TEXT("Showing UV set %d for LOD %d"), AnimSetViewer->UVSetToDisplay, LODIndex ), GEngine->SmallFont, FColor(255,255,255) );
		//calculate scaling
		const UINT BorderWidth = 5;
		const UINT MinY = 80 + BorderWidth;
		const UINT MinX = BorderWidth;
		const FVector2D Origin(MinX, MinY);
		const UINT SizeOfSquare = Min(Viewport->GetSizeX() - MinX, Viewport->GetSizeY() - MinY) - BorderWidth;

		//draw texture border
		DrawLine2D(Canvas, Origin, FVector2D(MinX + SizeOfSquare, MinY), FLinearColor::White);
		DrawLine2D(Canvas, FVector2D(MinX + SizeOfSquare, MinY), FVector2D(MinX + SizeOfSquare, MinY + SizeOfSquare), FLinearColor::White);
		DrawLine2D(Canvas, FVector2D(MinX + SizeOfSquare, MinY + SizeOfSquare), FVector2D(MinX, MinY + SizeOfSquare), FLinearColor::White);
		DrawLine2D(Canvas, FVector2D(MinX, MinY + SizeOfSquare), Origin, FLinearColor::White);   

		if( AnimSetViewer->UVSetToDisplay < (INT)LODModel.NumTexCoords )
		{
			//draw triangles
			TArray<DWORD> Indices;
			LODModel.MultiSizeIndexContainer.GetIndexBuffer( Indices );
			UINT NumIndices = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();
			for (UINT i = 0; i < NumIndices - 2; i += 3)
			{
				FVector2D UV1(LODModel.VertexBufferGPUSkin.GetVertexUV(Indices(i), AnimSetViewer->UVSetToDisplay )); 
				FVector2D UV2(LODModel.VertexBufferGPUSkin.GetVertexUV(Indices(i + 1), AnimSetViewer->UVSetToDisplay )); 
				FVector2D UV3(LODModel.VertexBufferGPUSkin.GetVertexUV(Indices(i + 2), AnimSetViewer->UVSetToDisplay )); 

				// Draw lines in black unless the UVs are outside of the 0.0 - 1.0 range.  For out-of-bounds
				// UVs, we'll draw the line segment in red
				FLinearColor UV12LineColor = FLinearColor::Black;
				if( UV1.X < -0.01f || UV1.X > 1.01f ||
					UV2.X < -0.01f || UV2.X > 1.01f ||
					UV1.Y < -0.01f || UV1.Y > 1.01f ||
					UV2.Y < -0.01f || UV2.Y > 1.01f )
				{
					UV12LineColor = FLinearColor( 0.6f, 0.0f, 0.0f );
				}
				FLinearColor UV23LineColor = FLinearColor::Black;
				if( UV3.X < -0.01f || UV3.X > 1.01f ||
					UV2.X < -0.01f || UV2.X > 1.01f ||
					UV3.Y < -0.01f || UV3.Y > 1.01f ||
					UV2.Y < -0.01f || UV2.Y > 1.01f )
				{
					UV23LineColor = FLinearColor( 0.6f, 0.0f, 0.0f );
				}
				FLinearColor UV31LineColor = FLinearColor::Black;
				if( UV3.X < -0.01f || UV3.X > 1.01f ||
					UV1.X < -0.01f || UV1.X > 1.01f ||
					UV3.Y < -0.01f || UV3.Y > 1.01f ||
					UV1.Y < -0.01f || UV1.Y > 1.01f )
				{
					UV31LineColor = FLinearColor( 0.6f, 0.0f, 0.0f );
				}

				UV1 = ClampUVRange(UV1.X, UV1.Y) * SizeOfSquare + Origin;
				UV2 = ClampUVRange(UV2.X, UV2.Y) * SizeOfSquare + Origin;
				UV3 = ClampUVRange(UV3.X, UV3.Y) * SizeOfSquare + Origin;

				DrawLine2D(Canvas, UV1, UV2, UV12LineColor);
				DrawLine2D(Canvas, UV2, UV3, UV23LineColor);
				DrawLine2D(Canvas, UV3, UV1, UV31LineColor);
			}
		}
	}

}

/** Util for drawing wind direction. */
void FASVViewportClient::DrawWindDir(FViewport* Viewport, FCanvas* Canvas)
{
	FRotationMatrix ViewTM( this->ViewRotation );

	const INT SizeX = Viewport->GetSizeX();
	const INT SizeY = Viewport->GetSizeY();

	const FIntPoint AxisOrigin( 80, SizeY - 30 );
	const FLOAT AxisSize = 25.f;

	FVector AxisVec = AxisSize * ViewTM.InverseTransformNormal( AnimSetViewer->WindRot.Vector() );
	FIntPoint AxisEnd = AxisOrigin + FIntPoint( AxisVec.Y, -AxisVec.Z );
	DrawLine2D( Canvas, AxisOrigin, AxisEnd, FColor(255,255,128) );

	FString WindString = FString::Printf( TEXT("Wind: %4.1f"), AnimSetViewer->WindStrength );
	DrawString( Canvas, 65, SizeY - 15, *WindString, GEngine->SmallFont, FColor(255,255,128) );
	// Display windvelocityblendtime in the preview window for Apex Clothing Wind
#if WITH_APEX
	if(AnimSetViewer->PreviewSkelComp->GetApexClothing() != NULL)
	{
		FString WindVelBlendTimeString = FString::Printf( TEXT("BlendTime: %4.3f"), AnimSetViewer->WindVelocityBlendTime);
		DrawString( Canvas, 150, SizeY - 15, *WindVelBlendTimeString, GEngine->SmallFont, FColor(255,255,128));
	}
#endif
}

void FASVViewportClient::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);
	AnimSetViewer->TickViewer(DeltaSeconds * AnimSetViewer->PlaybackSpeed);
}

/**
 * Templated helper function to allow for the updating of custom sorting strips, regardless of index buffer size
 *
 * @tparam	IndexBufferType	Type of the index buffer for the provided LOD model (WORD or DWORD)
 *
 * @param	InStrips			Sort strip data to be updated
 * @param	InLODModel			LOD model to update
 * @param	InSection			Section to update
 * @param	AlternateIndexMode	Indexing mode for TRISORT_CustomLeftRight sections
 */
template<typename IndexBufferType> void UpdateCustomSortStrips( TArray<FSortStripData>& InStrips, FStaticLODModel& InLODModel, FSkelMeshSection& InSection, BYTE AlternateIndexMode )
{
	INT BaseIndex = InSection.BaseIndex;
	if( InSection.TriangleSorting == TRISORT_CustomLeftRight && AlternateIndexMode == CSAIM_Left )
	{
		BaseIndex += InSection.NumTriangles * 3;
	}

	// Allocate a temporary buffer with a type matching that of the LOD model's index buffer
	IndexBufferType* Temp = new IndexBufferType[ InSection.NumTriangles * 3 ];
	INT OutputIndex = 0;
	for( INT OutputStripIndex = 0; OutputStripIndex < InStrips.Num(); ++OutputStripIndex )
	{
		FSortStripData& SortStripData = InStrips(OutputStripIndex);
		appMemcpy( &Temp[OutputIndex], InLODModel.MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo( SortStripData.FirstIndex ), SortStripData.NumTris * 3 * InLODModel.MultiSizeIndexContainer.GetDataTypeSize() );
		// Adjust the starting triangle index based on the new position.
		SortStripData.FirstIndex = OutputIndex + BaseIndex;
		// Adjust the HitProxy's strip index.
		SortStripData.HitProxy->StripIndex = OutputStripIndex;
		OutputIndex += SortStripData.NumTris*3;
	}
	// Copy to the real index buffer.
	appMemcpy( InLODModel.MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo( BaseIndex ), Temp, InSection.NumTriangles * 3 * InLODModel.MultiSizeIndexContainer.GetDataTypeSize() );
	delete[] Temp;

	// Tell render thread we've updated the index buffer.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		UpdateSkelMeshIndexBuffer,
		FRawStaticIndexBuffer16or32<IndexBufferType>*,IndexBuffer,(FRawStaticIndexBuffer16or32<IndexBufferType>*)InLODModel.MultiSizeIndexContainer.GetIndexBuffer(),
	{
		IndexBuffer->InitRHI();
	});	
}

UBOOL FASVViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed,UBOOL Gamepad)
{
	// Hide and lock mouse cursor if we're capturing mouse input
	Viewport->ShowCursor( !Viewport->HasMouseCapture() );
	Viewport->LockMouseToWindow( Viewport->HasMouseCapture() );

	const INT HitX = Viewport->GetMouseX();
	const INT HitY = Viewport->GetMouseY();

	if(Key == KEY_LeftMouseButton)
	{
		HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
		if (Event == IE_DoubleClick)
		{
			if (HitResult)
			{
				if (HitResult->IsA(HNotifyProxy::StaticGetType()))
				{
					// bring up the properties for this notify
					AnimSetViewer->PropNotebook->SetSelection(2);

					// expand the notify node - along with all parents
					HNotifyProxy* Notify = ( HNotifyProxy* )HitResult;
					AnimSetViewer->AnimSeqProps->ExpandToItem("Notifies", Notify->NotifyIdx);

					// focus on the notify node
					FPropertyNode* NotifyNode = AnimSetViewer->AnimSeqProps->FindPropertyNode(TEXT("Notifies"));
					if( NotifyNode && ( Notify->NotifyIdx >= 0 ) && ( Notify->NotifyIdx < NotifyNode->GetNumChildNodes() ) )
					{
						FPropertyNode* ChildNode = NotifyNode->GetChildNode( Notify->NotifyIdx );
						if( ChildNode )
						{
							AnimSetViewer->AnimSeqProps->ClearActiveFocus();
							AnimSetViewer->AnimSeqProps->SetActiveFocus( ChildNode->GetNodeWindow(), TRUE );
						}
					}
				}
				else if (HitResult->IsA(HBoneControlProxy::StaticGetType()))
				{
					// bring up the properties for this notify
					AnimSetViewer->PropNotebook->SetSelection(2);
					FPropertyNode* MetadataNode = AnimSetViewer->AnimSeqProps->FindPropertyNode(TEXT("MetaData"));
					if( MetadataNode )
					{
						MetadataNode->SetExpanded(FALSE, TRUE);
					}
					AnimSetViewer->AnimSeqProps->ExpandItem(TEXT("MetaData"),((HBoneControlProxy*)HitResult)->ControlModifierID);
				}
				else if (HitResult->IsA(HVertInfluenceProxy::StaticGetType()))
				{
					//We want to swap in or out a vertex we double clicked from the alternate weight track
					HVertInfluenceProxy* InfluenceProxy = static_cast<HVertInfluenceProxy*>(HitResult);
					AnimSetViewer->UpdateInfluenceWeight(InfluenceProxy->LODIdx, InfluenceProxy->InfluenceIdx, InfluenceProxy->VertIndex, InfluenceProxy->bContributingInfluence);

					//Check for any "equivalent" vertices and adjust them too (handles z-fighting of the hit proxies)
					TArray<INT> EquivalentVerts;
					if (AnimSetViewer->GetEquivalentVertices(InfluenceProxy->LODIdx, InfluenceProxy->InfluenceIdx, InfluenceProxy->VertIndex, EquivalentVerts) > 0)
					{
					   for (INT OtherVertIdx=0; OtherVertIdx < EquivalentVerts.Num(); OtherVertIdx++)
					   {
						   AnimSetViewer->UpdateInfluenceWeight(InfluenceProxy->LODIdx, InfluenceProxy->InfluenceIdx, EquivalentVerts(OtherVertIdx), InfluenceProxy->bContributingInfluence);
					   }
					}

					// Flush hit proxies
					FASVViewportClient* ASVPreviewVC = AnimSetViewer->GetPreviewVC();
					check( ASVPreviewVC );
					ASVPreviewVC->Viewport->Invalidate();
				}
			}
		}
		else if(Event == IE_Pressed)
		{
			if(HitResult)
			{
				if(HitResult->IsA(HWidgetUtilProxy::StaticGetType()))
				{
					HWidgetUtilProxy* WidgetProxy = (HWidgetUtilProxy*)HitResult;

					if (WidgetProxy->Info1 == HITPROXY_SOCKET)
					{
						SocketManipulateAxis = WidgetProxy->Axis;
						BoneManipulateAxis = AXIS_None;
					}
					else
					{
						BoneManipulateAxis = WidgetProxy->Axis;
						SocketManipulateAxis = AXIS_None;
					}

					// Calculate the scree-space directions for this drag.
					FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
					FSceneView* View = CalcSceneView(&ViewFamily);
					WidgetProxy->CalcVectors(View, FViewportClick(View, this, Key, Event, HitX, HitY), LocalManDir, WorldManDir, DragDirX, DragDirY);

					bManipulating = true;
				}
				else if(HitResult->IsA(HASVSocketProxy::StaticGetType()))
				{
					HASVSocketProxy* SocketProxy = (HASVSocketProxy*)HitResult;

					USkeletalMeshSocket* Socket = NULL;
					if(SocketProxy->SocketIndex < AnimSetViewer->SelectedSkelMesh->Sockets.Num())
					{
						Socket = AnimSetViewer->SelectedSkelMesh->Sockets(SocketProxy->SocketIndex);
					}

					if(Socket)
					{
						AnimSetViewer->SetSelectedSocket(Socket);
					}
				}
				else if (HitResult->IsA(HTrackPosProxy::StaticGetType()))
				{
					bDraggingTrackPos = TRUE;
				}
				else if (HitResult->IsA(HNotifyProxy::StaticGetType()))
				{
					DraggingNotifyIndex = ((HNotifyProxy*)HitResult)->NotifyIdx;
					bDraggingNotifyTail = ((HNotifyProxy*)HitResult)->bTail;
				}
				else if( HitResult->IsA(HBoneControlProxy::StaticGetType()))
				{
					DraggingMetadataKeyIndex = ((HBoneControlProxy*)HitResult)->TimeModifierID;
					DraggingMetadataIndex = ((HBoneControlProxy*)HitResult)->ControlModifierID;
				}
				else if (HitResult->IsA(HVertexInfoProxy::StaticGetType()))
				{
					// set the selected vertex
					HVertexInfoProxy* VertexInfoProxy = static_cast<HVertexInfoProxy*>(HitResult);
					AnimSetViewer->VertexInfo.SelectedVertexIndex = VertexInfoProxy->VertexID;
					AnimSetViewer->VertexInfo.SelectedVertexPosition = VertexInfoProxy->VertexPosition;
					AnimSetViewer->VertexInfo.BoneIndices.Empty();
					AnimSetViewer->VertexInfo.BoneIndices.Append(VertexInfoProxy->InfluencedIndices);
					AnimSetViewer->VertexInfo.BoneWeights.Empty();
					AnimSetViewer->VertexInfo.BoneWeights.Append(VertexInfoProxy->InfluencedWeights);
				}
				else if (HitResult->IsA(HSortStripProxy::StaticGetType()))
				{
					HSortStripProxy* SortStripProxy = (HSortStripProxy*)HitResult;

					if( AnimSetViewer->SelectedSortStripIndex != INDEX_NONE && (AnimSetViewer->bSortStripMoveForward || AnimSetViewer->bSortStripMoveBackward) )
					{
						// Reorder the strips in the VisualizeSortStripData array.
						TArray<FSortStripData>& Strips = SortStripProxy->SceneProxy->VisualizeSortStripData(SortStripProxy->LODModelIndex)(SortStripProxy->SectionIndex);
						FSortStripData SelectedStrip = Strips(AnimSetViewer->SelectedSortStripIndex);
						Strips.Remove(AnimSetViewer->SelectedSortStripIndex);

						// If we are the selected strip in front of the clicked strip, the selected strip takes its index.
						// If we are moving the selected strip after the clicked strip, we want to use the clicked strip's index +1.
						// However we just removed our item from before the clicked strip which reduced its index by 1, so no need for the +1.
						INT NewPositionIndex = SortStripProxy->StripIndex;

						// Move the selected strip to its new position.
						Strips.InsertItem( SelectedStrip, NewPositionIndex );

						AnimSetViewer->SelectedSortStripIndex = NewPositionIndex;
						AnimSetViewer->MouseOverSortStripIndex = INDEX_NONE;

						// Write the index data in new VisualizeSortStripData order to a temporary index buffer.
						FStaticLODModel& LODModel = AnimSetViewer->SelectedSkelMesh->LODModels(AnimSetViewer->SelectedSortStripLodIndex);
						FSkelMeshSection& Section = LODModel.Sections(AnimSetViewer->SelectedSortStripSectionIndex);

						// Update the custom sort strips
						if ( LODModel.MultiSizeIndexContainer.GetDataTypeSize() == sizeof( WORD ) )
						{
							UpdateCustomSortStrips<WORD>( Strips, LODModel, Section, AnimSetViewer->PreviewSkelComp->CustomSortAlternateIndexMode );
						}
						else
						{
							UpdateCustomSortStrips<DWORD>( Strips, LODModel, Section, AnimSetViewer->PreviewSkelComp->CustomSortAlternateIndexMode );
						}
				
						AnimSetViewer->SelectedSkelMesh->MarkPackageDirty();

						// Invalidate hit proxies as we've changed the order and so which should be rendered.
						Viewport->InvalidateHitProxy();
					}
					else
					{
						if( SortStripProxy->StripIndex != AnimSetViewer->SelectedSortStripIndex )
						{
							// Selected a different strip.
							AnimSetViewer->SelectedSortStripIndex = SortStripProxy->StripIndex;
							AnimSetViewer->SelectedSortStripLodIndex = SortStripProxy->LODModelIndex;
							AnimSetViewer->SelectedSortStripSectionIndex = SortStripProxy->SectionIndex;
							AnimSetViewer->MouseOverSortStripIndex = INDEX_NONE;
							AnimSetViewer->bSortStripMoveForward = FALSE;
							AnimSetViewer->bSortStripMoveBackward = FALSE;
						}
						else
						{
							// Selected same strip, so deselect it.
							if( !AnimSetViewer->bSortStripMoveForward && !AnimSetViewer->bSortStripMoveBackward )
							{
								AnimSetViewer->SelectedSortStripIndex = INDEX_NONE;
								AnimSetViewer->SelectedSortStripLodIndex = INDEX_NONE;
								AnimSetViewer->SelectedSortStripSectionIndex = INDEX_NONE;
								AnimSetViewer->MouseOverSortStripIndex = INDEX_NONE;
								AnimSetViewer->bSortStripMoveForward = FALSE;
								AnimSetViewer->bSortStripMoveBackward = FALSE;
							}
						}
					}
				}
			}
		}
		else if(Event == IE_Released)
		{
			if( bManipulating )
			{	
				SocketManipulateAxis = AXIS_None;
				BoneManipulateAxis = AXIS_None;
				bManipulating = FALSE;
			}
			if (bDraggingTrackPos)
			{
				bDraggingTrackPos = FALSE;
			}
			if (DraggingNotifyIndex != -1)
			{
				if (DraggingNotifyIndex >= 0 && DraggingNotifyIndex < AnimSetViewer->SelectedAnimSeq->Notifies.Num())
				{
					// the Time and/or Duration property of the AnimNotify has changed, invoke the EventChanged method
					FAnimNotifyEvent* OwnerEvent = &(AnimSetViewer->SelectedAnimSeq->Notifies(DraggingNotifyIndex));
					if (OwnerEvent && OwnerEvent->Notify)
					{
						OwnerEvent->Notify->AnimNotifyEventChanged(AnimSetViewer->PreviewAnimNode, OwnerEvent);
					}
				}
			}

			DraggingNotifyIndex = -1;
			DraggingMetadataIndex = -1;
			DraggingMetadataKeyIndex = -1;
			if( AnimSetViewer->SelectedAnimSeq != NULL )
			{
				GCallbackEvent->Send( CALLBACK_ObjectPropertyChanged, AnimSetViewer->SelectedAnimSeq );
			}
		}
	}

	if( Event == IE_Pressed )
	{
		if(Key == KEY_SpaceBar)
		{
			if(AnimSetViewer->SocketMgr)
			{
				if(AnimSetViewer->SocketMgr->MoveMode == SMM_Rotate)
				{
					AnimSetViewer->SetSocketMoveMode( SMM_Translate );
				}
				else
				{
					AnimSetViewer->SetSocketMoveMode( SMM_Rotate );
				}
			}
			
			if (AnimSetViewer->PreviewSkelComp && AnimSetViewer->PreviewSkelComp->BonesOfInterest.Num() == 1)
			{
				if(AnimSetViewer->BoneMoveMode == SMM_Rotate)
				{
					AnimSetViewer->BoneMoveMode = SMM_Translate;
				}
				else
				{
					AnimSetViewer->BoneMoveMode = SMM_Rotate;
				}

				// Flush hit proxies so we get them for the new widget.
				FASVViewportClient* ASVPreviewVC = AnimSetViewer->GetPreviewVC();
				check( ASVPreviewVC );
				ASVPreviewVC->Viewport->Invalidate();
			}
		}
	}	

	// Do stuff for FEditorLevelViewportClient camera movement.
	if( Event == IE_Pressed) 
	{
		if( Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton || Key == KEY_MiddleMouseButton )
		{
			MouseDeltaTracker->StartTracking( this, HitX, HitY );
		}
	}
	else if( Event == IE_Released )
	{
		if( Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton || Key == KEY_MiddleMouseButton )
		{
			MouseDeltaTracker->EndTracking( this );
		}
	}

	if( Event == IE_Pressed && AnimSetViewer->SelectedSortStripIndex != INDEX_NONE )
	{
		if( Key == KEY_F )
		{
			AnimSetViewer->bSortStripMoveForward = TRUE;
		}
		else
		if( Key == KEY_B )
		{
			AnimSetViewer->bSortStripMoveBackward = TRUE;
		}
	}

	if( Event == IE_Released && (Key == KEY_B || Key == KEY_F) )
	{
		AnimSetViewer->MouseOverSortStripIndex = INDEX_NONE;
		AnimSetViewer->bSortStripMoveForward = FALSE;
		AnimSetViewer->bSortStripMoveBackward = FALSE;
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );

	return TRUE;
}

/** Handles mouse being moved while input is captured - in this case, while a mouse button is down. */
UBOOL FASVViewportClient::InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	// Get some useful info about buttons being held down
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bLightMoveDown = Viewport->KeyState(KEY_L);
	UBOOL bWindMoveDown = Viewport->KeyState(KEY_W);
	UBOOL bRightMouseDown = Viewport->KeyState(KEY_RightMouseButton);

	// Look at which axis is being dragged and by how much
	FLOAT DragX = (Key == KEY_MouseX) ? Delta : 0.f;
	FLOAT DragY = (Key == KEY_MouseY) ? Delta : 0.f;

	if (bDraggingTrackPos)
	{
		FLOAT NewPos = AnimSetViewer->PreviewAnimNode->CurrentTime + (DragX/400.f * AnimSetViewer->SelectedAnimSeq->SequenceLength);
		AnimSetViewer->PreviewAnimNode->SetPosition(NewPos,FALSE);
		AnimSetViewer->PreviewAnimNodeRaw->SetPosition(NewPos,FALSE);
	}
	else if (DraggingNotifyIndex != -1)
	{
		if (DraggingNotifyIndex >= 0 && DraggingNotifyIndex < AnimSetViewer->SelectedAnimSeq->Notifies.Num())
		{
			// check to see if we're dragging the tail, and should update duration instead of time
			if (bDraggingNotifyTail)
			{
				FLOAT &Duration = AnimSetViewer->SelectedAnimSeq->Notifies(DraggingNotifyIndex).Duration;
				Duration = Max(0.f,Duration + (DragX/400.f * AnimSetViewer->SelectedAnimSeq->SequenceLength));
				// clamp by the end of the anim seq
				FLOAT Time = AnimSetViewer->SelectedAnimSeq->Notifies(DraggingNotifyIndex).Time;
				if (Time + Duration > AnimSetViewer->SelectedAnimSeq->SequenceLength)
				{
					Duration = AnimSetViewer->SelectedAnimSeq->SequenceLength - Time;
				}
				// if shift is down then match the anim track pos
				if (bShiftDown)
				{
					AnimSetViewer->PreviewAnimNode->SetPosition(Time,FALSE);
					AnimSetViewer->PreviewAnimNodeRaw->SetPosition(Time,FALSE);
				}
			}
			else
			{
				FLOAT &Time = AnimSetViewer->SelectedAnimSeq->Notifies(DraggingNotifyIndex).Time;
				Time = Clamp<FLOAT>(Time + (DragX/400.f * AnimSetViewer->SelectedAnimSeq->SequenceLength),0.f,AnimSetViewer->SelectedAnimSeq->SequenceLength);
				// check to see if the duration needs to be clamped
				FLOAT &Duration = AnimSetViewer->SelectedAnimSeq->Notifies(DraggingNotifyIndex).Duration;
				if (Time + Duration > AnimSetViewer->SelectedAnimSeq->SequenceLength)
				{
					debugf(TEXT("clamping: %.2f vs %.2f"),Time+Duration,AnimSetViewer->SelectedAnimSeq->SequenceLength);
					Duration = AnimSetViewer->SelectedAnimSeq->SequenceLength - Time;
				}
				// if shift is down then match the anim track pos
				if (bShiftDown)
				{
					AnimSetViewer->PreviewAnimNode->SetPosition(Time,FALSE);
					AnimSetViewer->PreviewAnimNodeRaw->SetPosition(Time,FALSE);
				}
			}
		}
	}
	else if( DraggingMetadataKeyIndex != -1 )
	{
		if (DraggingMetadataIndex >= 0 && DraggingMetadataIndex < AnimSetViewer->SelectedAnimSeq->MetaData.Num())
		{
			UAnimMetaData_SkelControlKeyFrame* SkelControlKeyFrame = Cast<UAnimMetaData_SkelControlKeyFrame>(AnimSetViewer->SelectedAnimSeq->MetaData(DraggingMetadataIndex));
			if( SkelControlKeyFrame )
			{
				FLOAT &Time = SkelControlKeyFrame->KeyFrames(DraggingMetadataKeyIndex).Time;
				Time = Clamp<FLOAT>(Time + (DragX/200.f * AnimSetViewer->SelectedAnimSeq->SequenceLength),0.f,AnimSetViewer->SelectedAnimSeq->SequenceLength);
				// if shift is down then match the anim track pos
				if (bShiftDown)
				{
					AnimSetViewer->PreviewAnimNode->SetPosition(Time,FALSE);
					AnimSetViewer->PreviewAnimNodeRaw->SetPosition(Time,FALSE);
				}
			}
		}
	}
	else if(bManipulating)
	{
		FLOAT DragMag = (DragX * DragDirX) + (DragY * DragDirY);

		if(AnimSetViewer->SocketMgr && AnimSetViewer->SelectedSocket && SocketManipulateAxis != AXIS_None)
		{
			if(AnimSetViewer->SocketMgr->MoveMode == SMM_Rotate)
			{
				FQuat CurrentQuat = AnimSetViewer->SelectedSocket->RelativeRotation.Quaternion();
				FQuat DeltaQuat( LocalManDir, -DragMag * AnimSetViewer_RotateSpeed );
				FQuat NewQuat = CurrentQuat * DeltaQuat;

				AnimSetViewer->SelectedSocket->RelativeRotation = FRotator(NewQuat);

				AnimSetViewer->SocketMgr->UpdateTextEntry();
			}
			else
			{
				FRotationMatrix SocketRotTM( AnimSetViewer->SelectedSocket->RelativeRotation );
				FVector SocketMove = SocketRotTM.TransformNormal( LocalManDir * DragMag * AnimSetViewer_TranslateSpeed );

				AnimSetViewer->SelectedSocket->RelativeLocation += SocketMove;
			}

			AnimSetViewer->UpdateSocketPreviews();

			AnimSetViewer->SelectedSkelMesh->MarkPackageDirty();
		}
		
		if (BoneManipulateAxis != AXIS_None && AnimSetViewer->PreviewSkelComp && AnimSetViewer->PreviewSkelComp->BonesOfInterest.Num() == 1)
		{
			//Get the skel control manipulating this bone
			FSkelControlListHead& SkelListControl =  AnimSetViewer->PreviewAnimTree->SkelControlLists(0);
			USkelControlSingleBone* SkelControl = Cast<USkelControlSingleBone>(SkelListControl.ControlHead);
			if (SkelControl)
			{
				//Get the accumulation from control up to this point
				FRotationTranslationMatrix CurrentSkelControlMat(SkelControl->BoneRotation, SkelControl->BoneTranslation);

				if(AnimSetViewer->BoneMoveMode == SMM_Rotate)
				{
					FLOAT ManipulateRotation = -AnimSetViewer_RotateSpeed * DragMag;
					ManipulateRotation = AnimSetViewer_RotateSpeed * appFloor(ManipulateRotation/AnimSetViewer_RotateSpeed);

					//Calculate the new delta rotation
					FQuat DeltaQuat(LocalManDir, ManipulateRotation);
					FQuatRotationTranslationMatrix ManipulateMatrix( DeltaQuat, FVector(0.f) );
					SkelControl->BoneRotation = (ManipulateMatrix * CurrentSkelControlMat).Rotator();
				}
				else
				{
					FLOAT ManipulateTranslation = AnimSetViewer_TranslateSpeed * DragMag;
					ManipulateTranslation = AnimSetViewer_TranslateSpeed * appFloor(ManipulateTranslation/AnimSetViewer_TranslateSpeed);
					
					//Calculate the new orientation of the axis after skel control 
					FVector BoneSpaceDir = CurrentSkelControlMat.TransformNormal(LocalManDir);
					SkelControl->BoneTranslation += (BoneSpaceDir * ManipulateTranslation);
				}
			}
		}
		if (AnimSetViewer->SelectedSocket)
		{
			GCallbackEvent->Send( CALLBACK_ObjectPropertyChanged, AnimSetViewer->SelectedSocket );
		}
	}
	else if(bLightMoveDown)
	{
		FRotator LightDir = PreviewScene.GetLightDirection();
		
		LightDir.Yaw += -DragX * AnimSetViewer_LightRotSpeed;
		LightDir.Pitch += -DragY * AnimSetViewer_LightRotSpeed;

		PreviewScene.SetLightDirection(LightDir);
	}
	else if(bWindMoveDown)
	{
		// Display parameter in the preview window for Apex Clothing Wind
		if(bRightMouseDown)
		{

			// If right mouse button and ctrl is down, turn Y movement into modifying windvelocityblendtime
			if(bCtrlDown)
			{
#if WITH_APEX
				AnimSetViewer->WindVelocityBlendTime = ::Max(AnimSetViewer->WindVelocityBlendTime + DragY * AnimSetViewer_WindVelocityBlendTimeSpeed, 0.0f);
#endif
			}
			// If right mouse button is down, turn Y movement into modifying strength
			else
			{
				AnimSetViewer->WindStrength = ::Max(AnimSetViewer->WindStrength + (DragY * AnimSetViewer_WindStrengthSpeed), 0.f);
			}
		}
		// Otherwise, orbit the wind
		else
		{
			AnimSetViewer->WindRot.Yaw += -DragX * AnimSetViewer_LightRotSpeed;
			AnimSetViewer->WindRot.Pitch += -DragY * AnimSetViewer_LightRotSpeed;
		}

		AnimSetViewer->UpdateClothWind();
	}
	// If we are not manipulating an axis, use the MouseDeltaTracker to update camera.
	else
	{
		MouseDeltaTracker->AddDelta( this, Key, Delta, 0 );
		const FVector DragDelta = MouseDeltaTracker->GetDelta();

		GEditor->MouseMovement += DragDelta;

		if( !DragDelta.IsZero() )
		{
			// Convert the movement delta into drag/rotation deltas
			if ( bAllowMayaCam && GEditor->bUseMayaCameraControls )
			{
				FVector TempDrag;
				FRotator TempRot;
				InputAxisMayaCam( Viewport, DragDelta, TempDrag, TempRot );
			}
			else
			{
				FVector Drag;
				FRotator Rot;
				FVector Scale;
				MouseDeltaTracker->ConvertMovementDeltaToDragRot( this, DragDelta, Drag, Rot, Scale );
				MoveViewportCamera( Drag, Rot );
			}

			MouseDeltaTracker->ReduceBy( DragDelta );
		}
	}

	Viewport->Invalidate();

	return TRUE;
}

void FASVViewportClient::MouseMove(FViewport* Viewport,INT x, INT y)
{
	AnimSetViewer->MouseOverSortStripIndex = INDEX_NONE;
	AnimSetViewer->bSortStripMoveForward = AnimSetViewer->SelectedSortStripIndex != INDEX_NONE && Viewport->KeyState(KEY_F);
	AnimSetViewer->bSortStripMoveBackward = AnimSetViewer->SelectedSortStripIndex != INDEX_NONE && Viewport->KeyState(KEY_B);

	// If we are not currently moving the widget - update the ManipulateAxis to the one we are mousing over.
	if(!bManipulating)
	{
		INT	HitX = Viewport->GetMouseX();
		INT HitY = Viewport->GetMouseY();

		HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
		if(HitResult && HitResult->IsA(HWidgetUtilProxy::StaticGetType()))
		{
			HWidgetUtilProxy* WidgetProxy = (HWidgetUtilProxy*)HitResult;
			if (WidgetProxy->Info1 == HITPROXY_BONE)
			{  
				BoneManipulateAxis = WidgetProxy->Axis;
				SocketManipulateAxis = AXIS_None;
			}
			else
			{
				BoneManipulateAxis = AXIS_None;
				SocketManipulateAxis = WidgetProxy->Axis;
			}
		}
		else
		if( HitResult && HitResult->IsA(HSortStripProxy::StaticGetType()) )
		{
			if( AnimSetViewer->bSortStripMoveBackward || AnimSetViewer->bSortStripMoveForward )
			{
				HSortStripProxy* SortStripProxy = (HSortStripProxy*)HitResult;
				if( SortStripProxy->LODModelIndex == AnimSetViewer->SelectedSortStripLodIndex &&
					SortStripProxy->SectionIndex == AnimSetViewer->SelectedSortStripSectionIndex )
				{
					AnimSetViewer->MouseOverSortStripIndex = SortStripProxy->StripIndex;
				}			
			}
		}
		else 
		{
			BoneManipulateAxis = AXIS_None;
			SocketManipulateAxis = AXIS_None;
		}
	}
}

/*-----------------------------------------------------------------------------
	UASVSkelComponent
-----------------------------------------------------------------------------*/

/** Create the scene proxy needed for rendering a ASV skeletal mesh */
FPrimitiveSceneProxy* UASVSkelComponent::CreateSceneProxy()
{
	FASVSkelSceneProxy* Result = NULL;

	// only create a scene proxy for rendering if
	// properly initialized
	if( SkeletalMesh && 
		SkeletalMesh->LODModels.IsValidIndex(PredictedLODLevel) &&
		!bHideSkin &&
		MeshObject )
	{
		const FColor WireframeMeshOverlayColor(102,205,170,255);
		Result = ::new FASVSkelSceneProxy(this, BoneColor, WireframeMeshOverlayColor);
	}

	return Result;
}

/**
 * Function that returns whether or not CPU skinning should be applied
 * Allows the editor to override the skinning state for editor tools
 */
UBOOL UASVSkelComponent::ShouldCPUSkin()
{
	return (ColorRenderMode!=ESCRM_None);
}

/** 
 * Function to operate on mesh object after its created, 
 * but before it's attached.
 * @param MeshObject - Mesh Object owned by this component
 */
void UASVSkelComponent::PostInitMeshObject(FSkeletalMeshObject* MeshObject)
{
	if (MeshObject)
	{
		//Setup the bone weights we want to render
		switch (ColorRenderMode)
		{
		case ESCRM_VertexTangent:
		case ESCRM_VertexNormal:
		case ESCRM_VertexMirror:
			MeshObject->EnableColorModeRendering((SkinColorRenderMode)ColorRenderMode);
			break;
		case ESCRM_BoneWeights:
			MeshObject->EnableBlendWeightRendering(TRUE , BonesOfInterest);
			break;
		default:
			MeshObject->EnableColorModeRendering(ESCRM_None);
			break;
		}
	}
}

/**
* Update material information depending on color render mode 
* Refresh/replace materials 
*/
void UASVSkelComponent::ApplyColorRenderMode(INT InColorRenderMode)
{
	SkinColorRenderMode OldColorRenderMode = (SkinColorRenderMode)ColorRenderMode;
	if ( InColorRenderMode >= ESCRM_None && InColorRenderMode<ESCRM_Max )
	{
		ColorRenderMode = InColorRenderMode;
	}

	if ( OldColorRenderMode == ESCRM_None  && ColorRenderMode != ESCRM_None)
	{
		// back up all materials
		SkelMaterials.Empty();
		INT NumMaterials = GetNumElements();
		for (INT i=0; i<NumMaterials; i++)
		{
			SkelMaterials.AddItem(GetMaterial(i));
		}

		// add mesh edges
		WxAnimSetViewer* ASV = (WxAnimSetViewer*)AnimSetViewerPtr;
		ASV->PreviewWindow->ASVPreviewVC->ShowFlags |= SHOW_MeshEdges;
	}

	switch (ColorRenderMode)
	{
	case ESCRM_VertexTangent:
		{
			INT NumMaterials = GetNumElements();
			for (INT i=0; i<NumMaterials; i++)
			{
				SetMaterial(i, GEngine->TangentColorMaterial);
			}
		}
		break;
	case ESCRM_VertexNormal:
	case ESCRM_VertexMirror:
	case ESCRM_BoneWeights:
		{
			INT NumMaterials = GetNumElements();
			for (INT i=0; i<NumMaterials; i++)
			{
				SetMaterial(i, GEngine->BoneWeightMaterial);
			}
		}
		break;
	default:
		{
			if ( SkelMaterials.Num() > 0 )
			{
				INT NumMaterials = GetNumElements();
				check(NumMaterials == SkelMaterials.Num());
				for (INT i=0; i<NumMaterials; i++)
				{
					SetMaterial(i, SkelMaterials(i));
				}

			}

			SkelMaterials.Empty();

			// remove mesh edges
			WxAnimSetViewer* ASV = (WxAnimSetViewer*)AnimSetViewerPtr;
			ASV->PreviewWindow->ASVPreviewVC->ShowFlags &= ~SHOW_MeshEdges;
		}
		break;
	}
}
/*-----------------------------------------------------------------------------
	WxASVPreview
-----------------------------------------------------------------------------*/

WxASVPreview::WxASVPreview(wxWindow* InParent, wxWindowID InID, class WxAnimSetViewer* InASV)
:	wxWindow( InParent, InID )
,	ASVPreviewVC( NULL )
{
	CreateViewport( InASV );
}

WxASVPreview::~WxASVPreview()
{
	DestroyViewport();
}

/**
 * Calls DestoryViewport(), then creates a new viewport client and associated viewport.
 */
void WxASVPreview::CreateViewport(class WxAnimSetViewer* InASV)
{
	DestroyViewport();

	ASVPreviewVC = new FASVViewportClient(InASV);
	ASVPreviewVC->Viewport = GEngine->Client->CreateWindowChildViewport(ASVPreviewVC, (HWND)GetHandle());
	check( ASVPreviewVC->Viewport );
	ASVPreviewVC->Viewport->CaptureJoystickInput(false);
}

/**
 * Destroys any existing viewport client and associated viewport.
 */
void WxASVPreview::DestroyViewport()
{
	if ( ASVPreviewVC )
	{
		GEngine->Client->CloseViewport( ASVPreviewVC->Viewport );
		ASVPreviewVC->Viewport = NULL;

		delete ASVPreviewVC;
		ASVPreviewVC = NULL;
	}
}

void WxASVPreview::OnSize( wxSizeEvent& In )
{
	if ( ASVPreviewVC )
	{
		checkSlow( ASVPreviewVC->Viewport );
		const wxRect rc = GetClientRect();
		::MoveWindow( (HWND)ASVPreviewVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );

		ASVPreviewVC->Viewport->Invalidate();
	}
}

BEGIN_EVENT_TABLE( WxASVPreview, wxWindow )
	EVT_SIZE( WxASVPreview::OnSize )
END_EVENT_TABLE()

/*-----------------------------------------------------------------------------
	WxAnimSetViewer
-----------------------------------------------------------------------------*/


BEGIN_EVENT_TABLE( WxAnimSetViewer, WxTrackableFrame )
	EVT_SIZE(WxAnimSetViewer::OnSize)
	EVT_COMBOBOX( IDM_ANIMSET_SKELMESHCOMBO, WxAnimSetViewer::OnSkelMeshComboChanged )
	EVT_BUTTON( IDM_ANIMSET_SKELMESHUSE, WxAnimSetViewer::OnSkelMeshUse )
	EVT_COMBOBOX( IDM_ANIMSET_ANIMSETCOMBO, WxAnimSetViewer::OnAnimSetComboChanged )
	EVT_BUTTON( IDM_ANIMSET_ANIMSETUSE, WxAnimSetViewer::OnAnimSetUse )
	EVT_BUTTON( IDM_ANIMSET_ANIMSEQSEARCHALL, WxAnimSetViewer::OnAnimSeqSearchTypeChange )
	EVT_TEXT( IDM_ANIMSET_ANIMSEQFILTER, WxAnimSetViewer::OnAnimSeqFilterTextChanged )
	EVT_TEXT_ENTER( IDM_ANIMSET_ANIMSEQFILTER, WxAnimSetViewer::OnAnimSeqFilterEnterPressed )
	EVT_BUTTON( IDM_ANIMSET_ANIMSEQCLEARFILTER, WxAnimSetViewer::OnClearAnimSeqFilter )
	EVT_UPDATE_UI( IDM_ANIMSET_ANIMSEQCLEARFILTER, WxAnimSetViewer::OnClearAnimSeqFilter_UpdateUI)
	EVT_TIMER( IDM_ANIMSET_ANIMSEQFILTERTIMER, WxAnimSetViewer::OnAnimSeqFilterTimer )
	EVT_COMBOBOX( IDM_ANIMSET_SKELMESHAUX1COMBO, WxAnimSetViewer::OnAuxSkelMeshComboChanged )
	EVT_COMBOBOX( IDM_ANIMSET_SKELMESHAUX2COMBO, WxAnimSetViewer::OnAuxSkelMeshComboChanged )
	EVT_COMBOBOX( IDM_ANIMSET_SKELMESHAUX3COMBO, WxAnimSetViewer::OnAuxSkelMeshComboChanged )
	EVT_BUTTON( IDM_ANIMSET_SKELMESH_AUX1USE, WxAnimSetViewer::OnAuxSkelMeshUse )
	EVT_BUTTON( IDM_ANIMSET_SKELMESH_AUX2USE, WxAnimSetViewer::OnAuxSkelMeshUse )
	EVT_BUTTON( IDM_ANIMSET_SKELMESH_AUX3USE, WxAnimSetViewer::OnAuxSkelMeshUse )
	EVT_LISTBOX( IDM_ANIMSET_ANIMSEQLIST, WxAnimSetViewer::OnAnimSeqListChanged )
#if WITH_ACTORX
	EVT_MENU( IDM_ANIMSET_IMPORTPSA, WxAnimSetViewer::OnImportPSA )
#endif
#if WITH_FBX
	EVT_MENU( IDM_ANIMSET_IMPORTFBXANIM, WxAnimSetViewer::OnImportFbxAnim )
	EVT_MENU( IDM_ANIMSET_EXPORTFBXANIM, WxAnimSetViewer::OnExportFbxAnim )
#endif // WITH_FBX
	EVT_MENU( IDM_ANIMSET_UNDO, WxAnimSetViewer::OnMenuEditUndo )
	EVT_MENU( IDM_ANIMSET_REDO, WxAnimSetViewer::OnMenuEditRedo )
#if WITH_SIMPLYGON
	EVT_MENU( IDM_ANIMSET_GENERATELOD, WxAnimSetViewer::OnGenerateLOD )
#endif // #if WITH_SIMPLYGON
	EVT_MENU( IDM_ANIMSET_IMPORTMESHLOD, WxAnimSetViewer::OnImportMeshLOD )
	EVT_MENU( IDM_ANIMSET_IMPORTMESHWEIGHTS, WxAnimSetViewer::OnImportMeshWeights )	
	EVT_MENU( IDM_ANIMSET_TOGGLEMESHWEIGHTS, WxAnimSetViewer::OnToggleMeshWeights )		
	EVT_MENU( IDM_ANIMSET_BONEEDITING_SHOWALLMESHVERTS, WxAnimSetViewer::OnToggleShowAllMeshVerts )	
	EVT_MENU( IDM_ANIMSET_NEWANISMET, WxAnimSetViewer::OnNewAnimSet )
	EVT_MENU( IDM_ANIMSET_NEWMORPHSET, WxAnimSetViewer::OnNewMorphTargetSet )
	EVT_MENU( IDM_ANIMSET_IMPORTMORPHTARGET, WxAnimSetViewer::OnImportMorphTarget )
	EVT_MENU( IDM_ANIMSET_IMPORTMORPHTARGETS, WxAnimSetViewer::OnImportMorphTargets )
	EVT_MENU( IDM_ANIMSET_IMPORTMORPHTARGET_LOD, WxAnimSetViewer::OnImportMorphTargetLOD )		
	EVT_MENU( IDM_ANIMSET_IMPORTMORPHTARGETS_LOD, WxAnimSetViewer::OnImportMorphTargetsLOD )		
	EVT_COMBOBOX( IDM_ANIMSET_MORPHSETCOMBO, WxAnimSetViewer::OnMorphSetComboChanged )
	EVT_BUTTON( IDM_ANIMSET_MORPHSETUSE, WxAnimSetViewer::OnMorphSetUse )
	EVT_COMMAND_RANGE( IDM_ANIMSET_MORPHTARGETS_START, IDM_ANIMSET_MORPHTARGETS_END, wxEVT_COMMAND_TEXT_UPDATED, WxAnimSetViewer::OnMorphTargetTextChanged )
	EVT_COMMAND_RANGE( IDM_ANIMSET_MORPHTARGETWEIGHTS_START, IDM_ANIMSET_MORPHTARGETWEIGHTS_END, wxEVT_COMMAND_SLIDER_UPDATED, WxAnimSetViewer::OnMorphTargetWeightChanged )
	EVT_COMMAND_RANGE( IDM_ANIMSET_MORPHTARGETSELECT_START, IDM_ANIMSET_MORPHTARGETSELECT_END, wxEVT_COMMAND_CHECKBOX_CLICKED, WxAnimSetViewer::OnSelectMorphTarget )

	EVT_BUTTON(IDM_ANIMSET_RESETMORPHTARGETPREVIEW, WxAnimSetViewer::OnResetMorphTargetPreview)
	EVT_CHECKBOX(IDM_ANIMSET_SELECTALLMORPHTARGETS, WxAnimSetViewer::OnSelectAllMorphTargets)
	EVT_COMMAND_SCROLL( IDM_ANIMSET_TIMESCRUB, WxAnimSetViewer::OnTimeScrub )
	EVT_MENU( IDM_ANIMSET_VIEWBONES, WxAnimSetViewer::OnViewBones )
	EVT_MENU( IDM_ANIMSET_ShowRawAnimation, WxAnimSetViewer::OnShowRawAnimation )
	EVT_MENU( IDM_ANIMSET_VIEWBONENAMES, WxAnimSetViewer::OnViewBoneNames )
	EVT_MENU( IDM_ANIMSET_VIEWFLOOR, WxAnimSetViewer::OnViewFloor )
	EVT_MENU( IDM_ANIMSET_VIEWWIREFRAME, WxAnimSetViewer::OnViewWireframe )
	EVT_MENU( IDM_ANIMSET_VIEWADDITIVEBASE, WxAnimSetViewer::OnViewAdditiveBase )
	EVT_MENU( IDM_ANIMSET_VIEWGRID, WxAnimSetViewer::OnViewGrid )
	EVT_MENU( IDM_ANIMSET_VIEWSOCKETS, WxAnimSetViewer::OnViewSockets )
	EVT_MENU( IDM_ANIMSET_VIEWMORPHKEYS, WxAnimSetViewer::OnViewMorphKeys )
	EVT_MENU( IDM_ANIMSET_VIEWREFPOSE, WxAnimSetViewer::OnViewRefPose )
	EVT_MENU( IDM_ANIMSET_VIEWMIRROR, WxAnimSetViewer::OnViewMirror )
	EVT_MENU( IDM_ANIMSET_VIEWBOUNDS, WxAnimSetViewer::OnViewBounds )
	EVT_MENU( IDM_ANIMSET_VIEWCOLLISION, WxAnimSetViewer::OnViewCollision )
	EVT_MENU( IDM_ANIMSET_VIEWSOFTBODYTETRA, WxAnimSetViewer::OnViewSoftBodyTetra )
	EVT_MENU( IDM_ANIMSET_VIEWALTBONEWEIGHTINGMODE, WxAnimSetViewer::OnEditAltBoneWeightingMode )
	EVT_MENU( IDM_ANIMSET_VIEWCLOTHMOVEDISTSCALE, WxAnimSetViewer::OnViewClothMoveDistScale )
	EVT_MENU( IDM_ANIMSET_VERTEX_SHOWTANGENT_AS_VECTOR, WxAnimSetViewer::OnViewVertexMode )
	EVT_MENU( IDM_ANIMSET_VERTEX_SHOWTANGENT_AS_TEXTURE, WxAnimSetViewer::OnViewVertexMode )
	EVT_MENU( IDM_ANIMSET_VERTEX_SHOWNORMAL_AS_VECTOR, WxAnimSetViewer::OnViewVertexMode )
	EVT_MENU( IDM_ANIMSET_VERTEX_SHOWNORMAL_AS_TEXTURE, WxAnimSetViewer::OnViewVertexMode )
	EVT_MENU( IDM_ANIMSET_VERTEX_SHOWMIRROR, WxAnimSetViewer::OnViewVertexMode )
	EVT_MENU( IDM_ANIMSET_VERTEX_SECTION_0, WxAnimSetViewer::OnSectionSelected )
	EVT_MENU( IDM_ANIMSET_VERTEX_SECTION_1, WxAnimSetViewer::OnSectionSelected )
	EVT_MENU( IDM_ANIMSET_VERTEX_SECTION_2, WxAnimSetViewer::OnSectionSelected )
	EVT_MENU( IDM_ANIMSET_VERTEX_SECTION_3, WxAnimSetViewer::OnSectionSelected )
	EVT_MENU( IDM_ANIMSET_VERTEX_SECTION_4, WxAnimSetViewer::OnSectionSelected )
	EVT_BUTTON( IDM_ANIMSET_LOOPBUTTON, WxAnimSetViewer::OnLoopAnim )
	EVT_BUTTON( IDM_ANIMSET_PLAYBUTTON, WxAnimSetViewer::OnPlayAnim )
	EVT_MENU( IDM_ANIMSET_EMPTYSET, WxAnimSetViewer::OnEmptySet )
	EVT_MENU_RANGE( IDM_ANIMSET_SHOW_UV_START, IDM_ANIMSET_SHOW_UV_END, WxAnimSetViewer::OnShowUVSet )
	EVT_UPDATE_UI_RANGE( IDM_ANIMSET_SHOW_UV_START, IDM_ANIMSET_SHOW_UV_END, WxAnimSetViewer::OnShowUVSet_UpdateUI )
	EVT_MENU( IDM_ANIMSET_DELETETRACK, WxAnimSetViewer::OnDeleteTrack )
	EVT_MENU( IDM_ANIMSET_DELETEMORPHTRACK, WxAnimSetViewer::OnDeleteMorphTrack )
	EVT_MENU( IDM_ANIMSET_COPYTRANSLATIONBONENAMES, WxAnimSetViewer::OnCopyTranslationBoneNames )
	EVT_MENU( IDM_ANIMSET_ANALYZEANIMSET, WxAnimSetViewer::OnAnalyzeAnimSet )
	EVT_MENU( IDM_ANIMSET_RENAMESEQ, WxAnimSetViewer::OnRenameSequence )
	EVT_MENU( IDM_ANIMSET_REMOVE_PREFIX, WxAnimSetViewer::OnRemovePrefixFromSequences )
	EVT_MENU( IDM_ANIMSET_DELETESEQ, WxAnimSetViewer::OnDeleteSequence )
	EVT_MENU( IDM_ANIMSET_COPYSEQ, WxAnimSetViewer::OnCopySequence )
	EVT_MENU( IDM_ANIMSET_MOVESEQ, WxAnimSetViewer::OnMoveSequence )
	EVT_MENU( IDM_ANIMSET_MAKESEQADDITIVE, WxAnimSetViewer::OnMakeSequencesAdditive )
	EVT_MENU( IDM_ANIMSET_ADDADDITIVETOSEQ, WxAnimSetViewer::OnAddAdditiveAnimationToSelectedSequence )
	EVT_MENU( IDM_ANIMSET_REBUILDADDITIVE, WxAnimSetViewer::OnRebuildAdditiveAnimation )
	EVT_MENU( IDM_ANIMSET_SUBTRACTADDITIVETOSEQ, WxAnimSetViewer::OnAddAdditiveAnimationToSelectedSequence )
	EVT_MENU( IDM_ANIMSET_SEQAPPLYROT, WxAnimSetViewer::OnSequenceApplyRotation )
	EVT_MENU( IDM_ANIMSET_SEQREZERO, WxAnimSetViewer::OnSequenceReZero )
	EVT_MENU( IDM_ANIMSET_SEQDELBEFORE, WxAnimSetViewer::OnSequenceCrop )
	EVT_MENU( IDM_ANIMSET_SEQDELAFTER, WxAnimSetViewer::OnSequenceCrop )
	EVT_MENU( IDM_ANIMSET_NOTIFYNEW, WxAnimSetViewer::OnNewNotify )
	EVT_MENU( IDM_ANIMSET_NOTIFYCUSTOM1, WxAnimSetViewer::OnNewNotify )
	EVT_MENU( IDM_ANIMSET_NOTIFYCUSTOM2, WxAnimSetViewer::OnNewNotify )
	EVT_MENU( IDM_ANIMSET_NOTIFYSORT, WxAnimSetViewer::OnNotifySort )
	EVT_MENU( IDM_ANIMSET_NOTIFYREMOVE, WxAnimSetViewer::OnNotifiesRemove )
	EVT_MENU( IDM_ANIMSET_NOTIFY_ENABLEALLPSYS, WxAnimSetViewer::OnAllParticleNotifies )
	EVT_MENU( IDM_ANIMSET_NOTIFY_DISABLEALLPSYS, WxAnimSetViewer::OnAllParticleNotifies )
	EVT_MENU( IDM_ANIMSET_NOTIFY_REFRESH_ALL_DATA, WxAnimSetViewer::OnRefreshAllNotifierData )
	EVT_MENU( IDM_ANIMSET_COPYSEQNAME, WxAnimSetViewer::OnCopySequenceName )
	EVT_MENU( IDM_ANIMSET_COPYSEQNAMELIST, WxAnimSetViewer::OnCopySequenceNameList )
	EVT_MENU( IDM_ANIMSET_UPDATEBOUNDS, WxAnimSetViewer::OnUpdateBounds )
	EVT_MENU( IDM_ANIMSET_COPYMESHBONES, WxAnimSetViewer::OnCopyMeshBoneNames )
	EVT_MENU( IDM_ANIMSET_COPYWEIGHTEDMESHBONES, WxAnimSetViewer::OnCopyWeightedMeshBoneNames )
	EVT_MENU( IDM_ANIMSET_FIXUPMESHBONES, WxAnimSetViewer::OnFixupMeshBoneNames )
	EVT_MENU( IDM_ANIMSET_SWAPLODSECTIONS, WxAnimSetViewer::OnSwapLODSections )
	EVT_MENU( IDM_ANIMSET_MERGEMATERIALS, WxAnimSetViewer::OnMergeMaterials )
	EVT_MENU( IDM_ANIMSET_AUTOMIRRORTABLE, WxAnimSetViewer::OnAutoMirrorTable )
	EVT_MENU( IDM_ANIMSET_CHECKMIRRORTABLE, WxAnimSetViewer::OnCheckMirrorTable )
	EVT_MENU( IDM_ANIMSET_COPYMIRRORTABLE, WxAnimSetViewer::OnCopyMirrorTable )
	EVT_MENU( IDM_ANIMSET_COPYMIRRORTABLEFROM, WxAnimSetViewer::OnCopyMirroTableFromMesh )
	EVT_MENU( IDM_ANIMSET_NOTIFYCOPY, WxAnimSetViewer::OnNotifyCopy )
	EVT_MENU( IDM_ANIMSET_NOTIFYSHIFT, WxAnimSetViewer::OnNotifyShift )
	EVT_TOOL( IDM_ANIMSET_OPENSOCKETMGR, WxAnimSetViewer::OnOpenSocketMgr )
	EVT_TOOL( IDM_ANIMSET_OpenAnimationCompressionDlg, WxAnimSetViewer::OnOpenAnimationCompressionDlg )
	EVT_BUTTON( IDM_ANIMSET_NEWSOCKET, WxAnimSetViewer::OnNewSocket )
	EVT_BUTTON( IDM_ANIMSET_DELSOCKET, WxAnimSetViewer::OnDeleteSocket )
	EVT_LISTBOX( IDM_ANIMSET_SOCKETLIST, WxAnimSetViewer::OnClickSocket )
	EVT_LISTBOX_DCLICK( IDM_ANIMSET_SOCKETLIST, WxAnimSetViewer::OnRenameSockets )
	EVT_TOOL( IDM_ANIMSET_SOCKET_TRANSLATE, WxAnimSetViewer::OnSocketMoveMode )
	EVT_TOOL( IDM_ANIMSET_SOCKET_ROTATE, WxAnimSetViewer::OnSocketMoveMode )
	EVT_TOOL( IDM_ANIMSET_CLEARPREVIEWS, WxAnimSetViewer::OnClearPreviewMeshes )
	EVT_TOOL( IDM_ANIMSET_COPYSOCKETS, WxAnimSetViewer::OnCopySockets )
	EVT_TOOL( IDM_ANIMSET_PASTESOCKETS, WxAnimSetViewer::OnPasteSockets )
	EVT_MENU( IDM_ANIMSET_LOD_AUTO, WxAnimSetViewer::OnForceLODLevel )
	EVT_MENU( IDM_ANIMSET_LOD_BASE, WxAnimSetViewer::OnForceLODLevel )
	EVT_MENU( IDM_ANIMSET_LOD_1, WxAnimSetViewer::OnForceLODLevel )
	EVT_MENU( IDM_ANIMSET_LOD_2, WxAnimSetViewer::OnForceLODLevel )
	EVT_MENU( IDM_ANIMSET_LOD_3, WxAnimSetViewer::OnForceLODLevel )
	EVT_MENU( IDM_ANIMSET_REMOVELOD,  WxAnimSetViewer::OnRemoveLOD )
	EVT_COMBOBOX( IDM_ANIMSET_CHUNKS, WxAnimSetViewer::OnViewChunk )
	EVT_COMBOBOX( IDM_ANIMSET_SECTIONS, WxAnimSetViewer::OnViewSection )
	EVT_TOOL( IDM_ANIMSET_SPEED_100,	WxAnimSetViewer::OnSpeed )
	EVT_TOOL( IDM_ANIMSET_SPEED_50, WxAnimSetViewer::OnSpeed )
	EVT_TOOL( IDM_ANIMSET_SPEED_25, WxAnimSetViewer::OnSpeed )
	EVT_TOOL( IDM_ANIMSET_SPEED_10, WxAnimSetViewer::OnSpeed )
	EVT_TOOL( IDM_ANIMSET_SPEED_1, WxAnimSetViewer::OnSpeed )
	EVT_MENU( IDM_ANIMSET_DELETE_MORPH, WxAnimSetViewer::OnDeleteMorphTarget )
	EVT_MENU( IDM_ANIMSET_UPDATE_MORPHTARGET, WxAnimSetViewer::OnUpdateMorphTarget)
	EVT_TOOL( IDM_ANIMSET_TOGGLECLOTH, WxAnimSetViewer::OnToggleClothSim )
	EVT_TOOL( IDM_ANIMSET_SOFTBODYGENERATE, WxAnimSetViewer::OnSoftBodyGenerate )
	EVT_TOOL( IDM_ANIMSET_SOFTBODYTOGGLESIM, WxAnimSetViewer::OnSoftBodyToggleSim )


	EVT_TREE_ITEM_RIGHT_CLICK( IDM_ANIMSET_SKELETONTREECTRL, WxAnimSetViewer::OnSkeletonTreeItemRightClick )
	EVT_TREE_SEL_CHANGED( IDM_ANIMSET_SKELETONTREECTRL, WxAnimSetViewer::OnSkeletonTreeSelectionChange )
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_SHOWBONE, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_HIDEBONE, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_SETBONECOLOR, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_SHOWCHILDBONE, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_HIDECHILDBONE, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_SETCHILDBONECOLOR, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_COPYBONENAME, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_AUTODETECT, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_RIGIDPREFERRED, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_DELETEBONEBREAK, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_RESETBONEBREAKS, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)
	EVT_MENU( IDM_ANIMSET_SKELETONTREE_SHOWBLENDWEIGHTS, WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand)

	EVT_TOOL(IDM_ANIMSET_COPY, WxAnimSetViewer::OnCopySelectedAnimSeqToClipboard)

	EVT_COMMAND_SCROLL( IDM_ANIMSET_PROGRESSIVESLIDER, WxAnimSetViewer::OnProgressiveSliderChanged )
	EVT_TOOL( IDM_ANIMSET_FOVRESET, WxAnimSetViewer::OnFOVReset)
	EVT_COMMAND_SCROLL( IDM_ANIMSET_FOVSLIDER, WxAnimSetViewer::OnFOVSliderChanged )
	EVT_TOOL( IDM_ANIMSET_TRIANGLESORTMODE, WxAnimSetViewer::OnToggleTriangleSortMode )
	EVT_TOOL( IDM_ANIMSET_TRIANGLESORTMODELR, WxAnimSetViewer::OnToggleTriangleSortModeLR )
	EVT_MENU( IDM_ANIMSET_ACTIVELY_LISTEN_FOR_FILE_CHANGES, WxAnimSetViewer::OnToggleActiveFileListen)
	EVT_UPDATE_UI( IDM_ANIMSET_ACTIVELY_LISTEN_FOR_FILE_CHANGES, WxAnimSetViewer::UI_ActiveFileListen)

	EVT_MENU(IDM_ANIMSET_PHYSX_CLEAR_ALL, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_WORLD_AXES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_BODY_AXES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_BODY_MASS_AXES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_BODY_LIN_VELOCITY, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_BODY_ANG_VELOCITY, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_BODY_JOINT_GROUPS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_JOINT_LOCAL_AXES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_JOINT_WORLD_AXES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_JOINT_LIMITS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CONTACT_POINT, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CONTACT_NORMAL, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CONTACT_ERROR, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CONTACT_FORCE, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_ACTOR_AXES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_AABBS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_SHAPES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_AXES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_COMPOUNDS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_VNORMALS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_FNORMALS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_EDGES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_SPHERES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_STATIC, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_DYNAMIC, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_FREE, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_CCD, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_COLLISION_SKELETONS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_EMITTERS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_POSITION, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_VELOCITY, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_KERNEL_RADIUS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_BOUNDS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_PACKETS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_MOTION_LIMIT, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_DYN_COLLISION, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_STC_COLLISION, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_MESH_PACKETS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_DRAINS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_FLUID_PACKET_DATA, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_MESH, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_COLLISIONS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_SELFCOLLISIONS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_WORKPACKETS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_SLEEP, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_SLEEP_VERTEX, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_TEARABLE_VERTICES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_TEARING, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_ATTACHMENT, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_CLOTH_VALIDBOUNDS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_MESH, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_COLLISIONS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_WORKPACKETS, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_SLEEP, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_SLEEP_VERTEX, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_TEARABLE_VERTICES, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_TEARING, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_ATTACHMENT, WxAnimSetViewer::OnPhysXDebug )
	EVT_MENU(IDM_ANIMSET_PHYSX_DEBUG_SOFTBODY_VALIDBOUNDS, WxAnimSetViewer::OnPhysXDebug )
#if WITH_APEX
	EVT_MENU(wxID_ANY, WxAnimSetViewer::OnApexDebug)
#endif

END_EVENT_TABLE()


static INT AnimSetViewer_ControlBorder = 4;

/*-----------------------------------------------------------------------------
WxMorphTargetPane
-----------------------------------------------------------------------------*/
WxMorphTargetPane::WxMorphTargetPane(wxWindow* InParent)
		:	wxScrolledWindow( InParent, -1, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL )
{
 	SetScrollbars(1,1,0,0);
 	// Set the x and y scrolling increments.  This is necessary for the scrolling to work!
 	SetScrollRate( 1, 1 );

 	MorphTargetSizer = new wxFlexGridSizer(5);
 	// second column is growable
	MorphTargetSizer->AddGrowableCol(1);
 	MorphTargetSizer->AddGrowableCol(3);
 	MorphTargetSizer->SetFlexibleDirection(wxHORIZONTAL);
	MorphTargetSizer->SetHGap(5);
	MorphTargetSizer->SetVGap(10);
	SetSizer(MorphTargetSizer);
}

/**
* Layout all children components for input list
*/
void WxMorphTargetPane::LayoutWindows( const TArray<UMorphTarget*>& MorphTargetList )
{
	// clean up
	MorphTargetSizer->Clear(true);

	MorphTargetTextNames.Empty();
	MorphTargetWeightSliders.Empty();
	MorphTargetSelectChecks.Empty();

	// verify if less than 100 - this is due to resourceIDs. 
	// You'd need to reserver more resource ID if you'd like more than 100. 
	check (MorphTargetList.Num() < IDM_ANIMSET_MORPHTARGETS_END-IDM_ANIMSET_MORPHTARGETS_START);

	MorphTargetTextNames.Add(MorphTargetList.Num());
	MorphTargetWeightSliders.Add(MorphTargetList.Num());
	MorphTargetSelectChecks.Add(MorphTargetList.Num());

	Freeze();

	const wxString SelectText(TEXT("Select"));
	const wxString Min(TEXT("0")), Max(TEXT("1"));

	// go through items, add to the panel
	for(INT I=0; I<MorphTargetList.Num(); I++)
	{
		UMorphTarget* Target = MorphTargetList(I);
		FString TargetString = FString::Printf( TEXT("%s"), *Target->GetName() );

		// Add the item 
		// Add slider 0 <slider> 1
		wxStaticText * MinText = new wxStaticText( this, -1, Min);
		MorphTargetSizer->Add( MinText, 0, wxALIGN_CENTER|wxALL);
		
		MorphTargetWeightSliders(I) = new wxSlider( this, IDM_ANIMSET_MORPHTARGETWEIGHTS_START+I, 0, 0, 100, wxDefaultPosition, wxSize(150, 20));
		MorphTargetSizer->Add( MorphTargetWeightSliders(I), 1, wxALIGN_CENTER|wxALL|wxLEFT|wxRIGHT|wxEXPAND);

		wxStaticText * MaxText = new wxStaticText( this, -1, Max);
		MorphTargetSizer->Add( MaxText, 0, wxALIGN_CENTER|wxALL, AnimSetViewer_ControlBorder);
		
		// Add text box for name
		MorphTargetTextNames(I) = new wxTextCtrl( this, IDM_ANIMSET_MORPHTARGETS_START+I, wxString(*TargetString), wxDefaultPosition, wxSize(100, 15));
		MorphTargetSizer->Add( MorphTargetTextNames(I), 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT, AnimSetViewer_ControlBorder);

		// selection check box - used for deletion/property window
		MorphTargetSelectChecks(I) = new wxCheckBox( this, IDM_ANIMSET_MORPHTARGETSELECT_START+I, SelectText, wxDefaultPosition);
		MorphTargetSizer->Add( MorphTargetSelectChecks(I), 0, wxALIGN_CENTER|wxALL, AnimSetViewer_ControlBorder);
	}

	// Tell the scrolling window how large the virtual area is.
	FitInside();
	SetVirtualSize( MorphTargetSizer->GetSize() );
	
	Thaw();
	Layout();
}

/*
* Return TRUE, if the index item is selected
*/
UBOOL WxMorphTargetPane::IsSelected( INT Index )
{
	check (MorphTargetSelectChecks.Num() > Index);

	return MorphTargetSelectChecks(Index)->GetValue();
}

/*
* Reset all weights on slider list
*/
void WxMorphTargetPane::ResetAllWeights()
{
	for ( INT I=0; I<MorphTargetWeightSliders.Num(); ++I )
	{
		MorphTargetWeightSliders(I)->SetValue(0);
	}
}

/*
* Select all if bSelect is true. Deselect otherwise. 
*/
void WxMorphTargetPane::SelectAll( UBOOL bSelect )
{
	for ( INT I=0; I<MorphTargetSelectChecks.Num(); ++I )
	{
		MorphTargetSelectChecks(I)->SetValue(bSelect == TRUE);
	}
}

/*-----------------------------------------------------------------------------
WxAnimSetViewer
-----------------------------------------------------------------------------*/
WxAnimSetViewer::WxAnimSetViewer(wxWindow* InParent, wxWindowID InID, USkeletalMesh* InSkelMesh, UAnimSet* InAnimSet, UMorphTargetSet* InMorphSet)
	:	WxTrackableFrame( InParent, InID, *LocalizeUnrealEd("AnimSetViewer"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
	,	FDockingParent( this )
	,	PlaybackSpeed( 1.0f )
	,	DlgAnimationCompression( NULL )
	,	bPreviewInstanceWeights(FALSE)
	,   bDrawAllBoneInfluenceVertices(TRUE)
	,	bTransactionInProgress(FALSE)
	,	SelectedSortStripIndex(INDEX_NONE)
	,	SelectedSortStripLodIndex(INDEX_NONE)
	,	SelectedSortStripSectionIndex(INDEX_NONE)
	,	bSortStripMoveForward(FALSE)
	,	bSortStripMoveBackward(FALSE)
	,	bResampleAnimNotifyData(FALSE)
	,	bSearchAllAnimSequences(FALSE)
	,	bPromptUserToLoadAllAnimSets(TRUE)
{
	wxEvtHandler::Connect(wxID_ANY, IDM_ANIMSET_FILLSKELETONTREE, wxCommandEventHandler(WxAnimSetViewer::OnFillSkeletonTree));

	SelectedSkelMesh = NULL;
	SelectedAnimSet = NULL;
	SelectedAnimSeq = NULL;
	SelectedMorphSet = NULL;
	SelectedSocket = NULL;

	SocketMgr = NULL;

	UVSetToDisplay = -1;

	GCallbackEvent->Register(CALLBACK_FileChanged, this);
	GCallbackEvent->Register(CALLBACK_PropertySelectionChange, this);

	// Reset this to zero.
	WireframeCycleCounter = 0;
	bShowAdditiveBase = FALSE;

	bShowClothMovementScale = FALSE;

	// clear memory
	appMemzero(&VertexInfo, sizeof(VertexInfo));

	/////////////////////////////////
	// Create all the GUI stuff.

	PlayB.Load( TEXT("ASV_Play") );
	StopB.Load( TEXT("ASV_Stop") );
	LoopB.Load( TEXT("ASV_Loop") );
	NoLoopB.Load( TEXT("ASV_NoLoop") );
	UseButtonB.Load( TEXT("Prop_Use") );
	SearchAllB.Load( TEXT("MaterialInstanceEditor_ShowAllParams") );
	ClearB.Load( "Cancel.png" );
	ClearDisabledB.Load( "Cancel_Disabled.png" );

	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

	//////// LEFT PANEL ////////
	wxPanel* LeftPanel = new wxPanel( this, -1 );
	wxBoxSizer* LeftSizerV = new wxBoxSizer( wxVERTICAL );
	LeftPanel->SetSizer(LeftSizerV);
	{
		ResourceNotebook = new wxNotebook( LeftPanel, -1, wxDefaultPosition, wxSize(350, -1) );
		ResourceNotebook->SetPageSize(wxSize(350, -1));
		LeftSizerV->Add( ResourceNotebook, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

		// SkeletalMesh
		{
			wxPanel* SkeletalMeshPanel = new wxPanel( ResourceNotebook, -1 );
			ResourceNotebook->AddPage( SkeletalMeshPanel, *LocalizeUnrealEd("SkelMesh"), true );

			wxBoxSizer* SkeletalMeshPanelSizer = new wxBoxSizer(wxVERTICAL);
			SkeletalMeshPanel->SetSizer(SkeletalMeshPanelSizer);
			SkeletalMeshPanel->SetAutoLayout(true);

			wxStaticText* SkelMeshLabel = new wxStaticText( SkeletalMeshPanel, -1, *LocalizeUnrealEd("SkeletalMesh") );
			SkeletalMeshPanelSizer->Add( SkelMeshLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder );

			wxBoxSizer* SkelMeshSizer = new wxBoxSizer( wxHORIZONTAL );
			SkeletalMeshPanelSizer->Add( SkelMeshSizer, 0, wxGROW|wxALL, 0 );

			SkelMeshCombo = new WxComboBox( SkeletalMeshPanel, IDM_ANIMSET_SKELMESHCOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_SORT );
			SkelMeshSizer->Add( SkelMeshCombo, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

			wxBitmapButton* SkelMeshUseButton = new wxBitmapButton( SkeletalMeshPanel, IDM_ANIMSET_SKELMESHUSE, UseButtonB );
			SkelMeshUseButton->SetToolTip( *LocalizeUnrealEd("ToolTip_2" ) );
			SkelMeshSizer->Add( SkelMeshUseButton, 0, wxGROW|wxALL, AnimSetViewer_ControlBorder );


			// Additional skeletal meshes
			{
				wxStaticText* Aux1SkelMeshLabel = new wxStaticText( SkeletalMeshPanel, -1, *LocalizeUnrealEd("ExtraMesh1") );
				SkeletalMeshPanelSizer->Add( Aux1SkelMeshLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder );

				wxBoxSizer* Aux1Sizer = new wxBoxSizer( wxHORIZONTAL );
				SkeletalMeshPanelSizer->Add( Aux1Sizer, 0, wxGROW|wxALL, 0 );

				SkelMeshAux1Combo = new WxComboBox( SkeletalMeshPanel, IDM_ANIMSET_SKELMESHAUX1COMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_SORT );
				Aux1Sizer->Add( SkelMeshAux1Combo, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

				wxBitmapButton* Aux1UseButton = new wxBitmapButton( SkeletalMeshPanel, IDM_ANIMSET_SKELMESH_AUX1USE, UseButtonB );
				Aux1UseButton->SetToolTip( *LocalizeUnrealEd("ToolTip_2") );
				Aux1Sizer->Add( Aux1UseButton, 0, wxGROW|wxALL, AnimSetViewer_ControlBorder );
			}

			{
				wxStaticText* Aux2SkelMeshLabel = new wxStaticText( SkeletalMeshPanel, -1, *LocalizeUnrealEd("ExtraMesh2") );
				SkeletalMeshPanelSizer->Add( Aux2SkelMeshLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder );

				wxBoxSizer* Aux2Sizer = new wxBoxSizer( wxHORIZONTAL );
				SkeletalMeshPanelSizer->Add( Aux2Sizer, 0, wxGROW|wxALL, 0 );

				SkelMeshAux2Combo = new WxComboBox( SkeletalMeshPanel, IDM_ANIMSET_SKELMESHAUX2COMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_SORT );
				Aux2Sizer->Add( SkelMeshAux2Combo, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

				wxBitmapButton* Aux2UseButton = new wxBitmapButton( SkeletalMeshPanel, IDM_ANIMSET_SKELMESH_AUX2USE, UseButtonB );
				Aux2UseButton->SetToolTip( *LocalizeUnrealEd("ToolTip_2") );
				Aux2Sizer->Add( Aux2UseButton, 0, wxGROW|wxALL, AnimSetViewer_ControlBorder );
			}

			{
				wxStaticText* Aux3SkelMeshLabel = new wxStaticText( SkeletalMeshPanel, -1, *LocalizeUnrealEd("ExtraMesh3") );
				SkeletalMeshPanelSizer->Add( Aux3SkelMeshLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder );

				wxBoxSizer* Aux3Sizer = new wxBoxSizer( wxHORIZONTAL );
				SkeletalMeshPanelSizer->Add( Aux3Sizer, 0, wxGROW|wxALL, 0 );

				SkelMeshAux3Combo = new WxComboBox( SkeletalMeshPanel, IDM_ANIMSET_SKELMESHAUX3COMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_SORT );
				Aux3Sizer->Add( SkelMeshAux3Combo, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

				wxBitmapButton* Aux3UseButton = new wxBitmapButton( SkeletalMeshPanel, IDM_ANIMSET_SKELMESH_AUX3USE, UseButtonB );
				Aux3UseButton->SetToolTip( *LocalizeUnrealEd("ToolTip_2") );
				Aux3Sizer->Add( Aux3UseButton, 0, wxGROW|wxALL, AnimSetViewer_ControlBorder );
			}
		}

		// AnimSet
		{
			wxPanel* AnimPanel = new wxPanel( ResourceNotebook, -1 );
			ResourceNotebook->AddPage( AnimPanel, *LocalizeUnrealEd("Anim"), true );

			wxBoxSizer* AnimPanelSizer = new wxBoxSizer(wxVERTICAL);
			AnimPanel->SetSizer(AnimPanelSizer);
			AnimPanel->SetAutoLayout(true);

			wxStaticText* AnimSetLabel = new wxStaticText( AnimPanel, -1, *LocalizeUnrealEd("AnimSet") );
			AnimPanelSizer->Add( AnimSetLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder );

			wxBoxSizer* AnimSetSizer = new wxBoxSizer( wxHORIZONTAL );
			AnimPanelSizer->Add( AnimSetSizer, 0, wxGROW|wxALL, 0 );

			AnimSetCombo = new WxComboBox( AnimPanel, IDM_ANIMSET_ANIMSETCOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_SORT );
			AnimSetSizer->Add( AnimSetCombo, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

			wxBitmapButton*	AnimSetUseButton = new wxBitmapButton( AnimPanel, IDM_ANIMSET_ANIMSETUSE, UseButtonB );
			AnimSetUseButton->SetToolTip( *LocalizeUnrealEd("ToolTip_1") );
			AnimSetSizer->Add( AnimSetUseButton, 0, wxGROW|wxALL, AnimSetViewer_ControlBorder );
 
			AnimSeqSearchAllButton = new wxBitmapButton( AnimPanel, IDM_ANIMSET_ANIMSEQSEARCHALL, SearchAllB );
			AnimSeqSearchAllButton->SetToolTip( *LocalizeUnrealEd("AnimSetViewer_ShowAllSequences") );
			AnimSetSizer->Add( AnimSeqSearchAllButton, 0, wxGROW|wxTOP|wxBOTTOM|wxRIGHT, AnimSetViewer_ControlBorder );

			wxBoxSizer* AnimSeqFilterSizer = new wxBoxSizer( wxHORIZONTAL );
			AnimPanelSizer->Add( AnimSeqFilterSizer, 0, wxGROW|wxALL, 0 );

			wxStaticText* AnimSeqFilterLabel = new wxStaticText( AnimPanel, -1, *LocalizeUnrealEd("SearchQ") );
			AnimSeqFilterSizer->Add( AnimSeqFilterLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, AnimSetViewer_ControlBorder );
			
			// Create text control to allow user to filter animseqs
			AnimSeqFilter = new wxTextCtrl( AnimPanel, IDM_ANIMSET_ANIMSEQFILTER, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER );
			AnimSeqFilterSizer->Add( AnimSeqFilter, 1, wxGROW|wxTOP|wxBOTTOM|wxLEFT, AnimSetViewer_ControlBorder );

			// Create button to clear search filter
			wxBitmapButton* AnimSeqClearFilterButton = new wxBitmapButton( AnimPanel, IDM_ANIMSET_ANIMSEQCLEARFILTER, ClearB );
			AnimSeqClearFilterButton->SetBitmapDisabled( ClearDisabledB );
			AnimSeqClearFilterButton->SetToolTip( *LocalizeUnrealEd("ClearQ") );
			AnimSeqFilterSizer->Add( AnimSeqClearFilterButton, 0, wxGROW|wxTOP|wxBOTTOM|wxRIGHT, AnimSetViewer_ControlBorder );
			
			// Initialize the filter timer
			AnimSeqFilterTimer.SetOwner( this, IDM_ANIMSET_ANIMSEQFILTERTIMER );

			// AnimSequence list
			wxStaticText* AnimSeqLabel = new wxStaticText( AnimPanel, -1, *LocalizeUnrealEd("AnimationSequences") );
			AnimPanelSizer->Add( AnimSeqLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder );

			AnimSeqList = new wxListBox( AnimPanel, IDM_ANIMSET_ANIMSEQLIST, wxDefaultPosition, wxSize(250, -1), 0, NULL, wxLB_EXTENDED | wxLB_SORT);
			AnimSeqList->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(WxAnimSetViewer::OnAnimSeqListRightClick), NULL, this);
			AnimPanelSizer->Add( AnimSeqList, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder  );
		}


		// Morph target
		{
			wxPanel* MorphPanel = new wxPanel( ResourceNotebook, -1 );
			ResourceNotebook->AddPage( MorphPanel, *LocalizeUnrealEd("Morph"), true );

			wxBoxSizer* MorphPanelSizer = new wxBoxSizer(wxVERTICAL);
			MorphPanel->SetSizer(MorphPanelSizer);
			MorphPanel->SetAutoLayout(true);

			wxStaticText* MorphSetLabel = new wxStaticText( MorphPanel, -1, *LocalizeUnrealEd("MorphTargetSet") );
			MorphPanelSizer->Add( MorphSetLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder );

			wxBoxSizer* MorphSetSizer = new wxBoxSizer( wxHORIZONTAL );
			MorphPanelSizer->Add( MorphSetSizer, 0, wxGROW|wxALL, 0 );

			MorphSetCombo = new WxComboBox( MorphPanel, IDM_ANIMSET_MORPHSETCOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_SORT );
			MorphSetSizer->Add( MorphSetCombo, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

			wxBitmapButton* MorphSetUseButton = new wxBitmapButton( MorphPanel, IDM_ANIMSET_MORPHSETUSE, UseButtonB );
			MorphSetUseButton->SetToolTip( *LocalizeUnrealEd("UseSelectedMorphTargetSet") );
			MorphSetSizer->Add( MorphSetUseButton, 0, wxGROW|wxALL, AnimSetViewer_ControlBorder );

			// add space
			MorphPanelSizer->AddSpacer(3);

			wxBoxSizer* MorphTargetOptionSizer = new wxBoxSizer(wxHORIZONTAL);

			// reset button
			wxButton * ResetMorphTarget = new wxButton(MorphPanel, IDM_ANIMSET_RESETMORPHTARGETPREVIEW, *LocalizeUnrealEd("ResetMorphTargetPreview"));
			MorphTargetOptionSizer->Add( ResetMorphTarget, 1, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder);

			// select all
			wxCheckBox * SelectAll = new wxCheckBox(MorphPanel, IDM_ANIMSET_SELECTALLMORPHTARGETS, *LocalizeUnrealEd("SelectAllMorphTargets"));
			MorphTargetOptionSizer->Add( SelectAll, 1, wxALIGN_RIGHT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder);

			MorphPanelSizer->Add( MorphTargetOptionSizer );
			// add space
			MorphPanelSizer->AddSpacer(3);

			// MorphTarget list
			wxStaticText* MorphTargetLabel = new wxStaticText( MorphPanel, -1, *LocalizeUnrealEd("MorphTargets") );
			MorphPanelSizer->Add( MorphTargetLabel, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, AnimSetViewer_ControlBorder );
			// add space			
			MorphPanelSizer->AddSpacer(3);

			// Create morph target panel
			MorphTargetPanel = new WxMorphTargetPane( MorphPanel );
			MorphPanelSizer->Add( MorphTargetPanel, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );
		}
	}

	//////// MIDDLE PANEL ////////
	wxPanel* MidPanel = new wxPanel( this, -1 );
	wxBoxSizer* MidSizerV = new wxBoxSizer( wxVERTICAL );
	MidPanel->SetSizer(MidSizerV);
	{
		// 3D Preview
		PreviewWindow = new WxASVPreview( MidPanel, -1, this );
		MidSizerV->Add( PreviewWindow, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

		// Sizer for controls at bottom
		wxBoxSizer* ControlSizerH = new wxBoxSizer( wxHORIZONTAL );
		MidSizerV->Add( ControlSizerH, 0, wxGROW|wxALL, AnimSetViewer_ControlBorder );

		// Stop/Play/Loop buttons
		LoopButton = new wxBitmapButton( MidPanel, IDM_ANIMSET_LOOPBUTTON, NoLoopB, wxDefaultPosition, wxDefaultSize, 0 );
		ControlSizerH->Add( LoopButton, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

		PlayButton = new wxBitmapButton( MidPanel, IDM_ANIMSET_PLAYBUTTON, PlayB, wxDefaultPosition, wxDefaultSize, 0 );
		ControlSizerH->Add( PlayButton, 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5 );

		// Time scrubber
		TimeSlider = new wxSlider( MidPanel, IDM_ANIMSET_TIMESCRUB, 0, 0, ASV_SCRUBRANGE );
		ControlSizerH->Add( TimeSlider, 1, wxGROW|wxALL, 0 );
	}

	//////// RIGHT PANEL ////////
	wxPanel* RightPanel = new wxPanel( this, -1 );
	wxBoxSizer* RightSizerV = new wxBoxSizer( wxVERTICAL );
	RightPanel->SetSizer(RightSizerV);
	{
		PropNotebook = new wxNotebook( RightPanel, -1, wxDefaultPosition, wxSize(350, -1) );
		PropNotebook->SetPageSize(wxSize(350, -1));
		RightSizerV->Add( PropNotebook, 1, wxGROW|wxALL, AnimSetViewer_ControlBorder );

		// Mesh properties
		wxPanel* MeshPropsPanel = new wxPanel( PropNotebook, -1 );
		PropNotebook->AddPage( MeshPropsPanel, *LocalizeUnrealEd("Mesh"), true );

		wxBoxSizer* MeshPropsPanelSizer = new wxBoxSizer(wxVERTICAL);
		MeshPropsPanel->SetSizer(MeshPropsPanelSizer);
		MeshPropsPanel->SetAutoLayout(true);

		MeshProps = new WxPropertyWindowHost;
		MeshProps->Create( MeshPropsPanel, this );
		MeshPropsPanelSizer->Add( MeshProps, 1, wxGROW|wxALL, 0 );

		// AnimSet properties
		wxPanel* AnimSetPropsPanel = new wxPanel( PropNotebook, -1 );
		PropNotebook->AddPage( AnimSetPropsPanel, *LocalizeUnrealEd("AnimSet"), false );

		wxBoxSizer* AnimSetPropsPanelSizer = new wxBoxSizer(wxVERTICAL);
		AnimSetPropsPanel->SetSizer(AnimSetPropsPanelSizer);
		AnimSetPropsPanel->SetAutoLayout(true);

		AnimSetProps = new WxPropertyWindowHost;
		AnimSetProps->Create( AnimSetPropsPanel, this );
		AnimSetPropsPanelSizer->Add( AnimSetProps, 1, wxGROW|wxALL, 0 );


		// AnimSequence properties
		wxPanel* AnimSeqPropsPanel = new wxPanel( PropNotebook, -1 );
		PropNotebook->AddPage( AnimSeqPropsPanel, *LocalizeUnrealEd("AnimSequence"), false );

		wxBoxSizer* AnimSeqPropsPanelSizer = new wxBoxSizer(wxVERTICAL);
		AnimSeqPropsPanel->SetSizer(AnimSeqPropsPanelSizer);
		AnimSeqPropsPanel->SetAutoLayout(true);

		AnimSeqProps = new WxPropertyWindowHost;
		AnimSeqProps->Create( AnimSeqPropsPanel, this );
		AnimSeqPropsPanelSizer->Add( AnimSeqProps, 1, wxGROW|wxALL, 0 );

		// MorphTarget properties
		wxPanel* MorphTargetPropsPanel = new wxPanel( PropNotebook, -1 );
		PropNotebook->AddPage( MorphTargetPropsPanel, *LocalizeUnrealEd("MorphTarget"), false );

		wxBoxSizer* MorphTargetPropsPanelSizer = new wxBoxSizer(wxVERTICAL);
		MorphTargetPropsPanel->SetSizer(MorphTargetPropsPanelSizer);
		MorphTargetPropsPanel->SetAutoLayout(true);

		MorphTargetProps = new WxPropertyWindowHost;
		MorphTargetProps->Create( MorphTargetPropsPanel, this );
		MorphTargetPropsPanelSizer->Add( MorphTargetProps, 1, wxGROW|wxALL, 0 );
		//wxPanel* TempPanel = new wxPanel( RightPanel, -1, wxDefaultPosition, wxSize(350, -1) );
		//RightSizerV->Add(TempPanel, 0, wxGROW, 0 );

#if WITH_SIMPLYGON
		wxPanel* SimplificationPanel = new wxPanel( PropNotebook, -1 );
		PropNotebook->AddPage( SimplificationPanel, *LocalizeUnrealEd("MeshSimp_WindowTitle"), false );
		wxBoxSizer* SimplificationPanelSizer = new wxBoxSizer(wxVERTICAL);
		SimplificationPanel->SetSizer(SimplificationPanelSizer);
		SimplificationPanel->SetAutoLayout(true);
		SimplificationWindow = new WxSkeletalMeshSimplificationWindow( this, SimplificationPanel );
		SimplificationPanelSizer->Add( SimplificationWindow, 1, wxGROW|wxALL, 0 );
#endif // #if WITH_SIMPLYGON
	}

	// Create tree control
	SkeletonTreeCtrl = new WxTreeCtrl;
	SkeletonTreeCtrl->Create( this, IDM_ANIMSET_SKELETONTREECTRL, NULL, wxTR_HAS_BUTTONS|wxTR_MULTIPLE|wxTR_LINES_AT_ROOT );

	SetSize(1200,800);
	// Load Window Options.
	LoadSettings();

	// Add docking windows.
	{
		AddDockingWindow( LeftPanel, FDockingParent::DH_Left, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("BrowserCaption_F"), *InAnimSet->GetPathName())), *LocalizeUnrealEd("Preview") );
		AddDockingWindow( RightPanel, FDockingParent::DH_Left, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("PropertiesCaption_F"), *InAnimSet->GetPathName())), *LocalizeUnrealEd("Properties") );
		AddDockingWindow( SkeletonTreeCtrl, FDockingParent::DH_Left, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("SkeletonTreeCaption_F"), *InAnimSet->GetPathName())), *LocalizeUnrealEd(TEXT("SkeletonTree")) );
		AddDockingWindow( MidPanel, FDockingParent::DH_None, NULL );

		// Try to load a existing layout for the docking windows.
		LoadDockingLayout();
	}

	// Create menu bar
	MenuBar = new WxASVMenuBar();
	AppendWindowMenu(MenuBar);
	SetMenuBar( MenuBar );
	// disable alt. bone weighting tool first. 
	MenuBar->EnableAltBoneWeightingMenu(FALSE,PreviewSkelComp);

	// Create tool bar
	ToolBar = new WxASVToolBar( this, -1 );
	SetToolBar( ToolBar );
	// Update the FOV so it shows the last used value
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		SetFOV( GApp->EditorFrame->ViewportConfigData->GetCustomFOV() );
	}

	// Create status bar
	StatusBar = new WxASVStatusBar( this, -1 );
	SetStatusBar( StatusBar );

	// Set camera location
	// TODO: Set the automatically based on bounds? Update when changing mesh? Store per-mesh?
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	ASVPreviewVC->ViewLocation = FVector(0,-256,0);
	ASVPreviewVC->ViewRotation = FRotator(0,16384,0);	

	// Init wind dir.
	WindStrength = 10.f;
	WindRot = FRotator(0,0,0);
#if WITH_APEX
	WindVelocityBlendTime = 0.1f;
#endif
	UpdateClothWind();


	// Use default WorldInfo to define the gravity and stepping params.
	AWorldInfo* Info = (AWorldInfo*)(AWorldInfo::StaticClass()->GetDefaultObject());
	check(Info);
	FVector Gravity(0, 0, Info->DefaultGravityZ);
	RBPhysScene = CreateRBPhysScene( Gravity );

#if WITH_APEX
	if ( (RBPhysScene != NULL) && (RBPhysScene->ApexScene != NULL) && (GApexManager != NULL) )
	{
		// Set infinite budget here so all clothing will be visible in AnimSetViewer
		GApexManager->SetApexLODResourceBudget( RBPhysScene->ApexScene, FLT_MAX );
	}
#endif
	
	// Fill in skeletal mesh combos box with all loaded skeletal meshes.
	SkelMeshCombo->Freeze();
	SkelMeshAux1Combo->Freeze();
	SkelMeshAux2Combo->Freeze();
	SkelMeshAux3Combo->Freeze();

	SkelMeshAux1Combo->Append( *LocalizeUnrealEd("-None-"), (void*)NULL );
	SkelMeshAux2Combo->Append( *LocalizeUnrealEd("-None-"), (void*)NULL );
	SkelMeshAux3Combo->Append( *LocalizeUnrealEd("-None-"), (void*)NULL );

	UBOOL bFoundSkelMesh = FALSE;
	for(TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* ItSkelMesh = *It;
		SkelMeshCombo->Append( *ItSkelMesh->GetName(), ItSkelMesh );
		SkelMeshAux1Combo->Append( *ItSkelMesh->GetName(), ItSkelMesh );
		SkelMeshAux2Combo->Append( *ItSkelMesh->GetName(), ItSkelMesh );
		SkelMeshAux3Combo->Append( *ItSkelMesh->GetName(), ItSkelMesh );
		bFoundSkelMesh = TRUE;
	}

	if (bFoundSkelMesh == FALSE)
	{
		// Fall back to the default skeletal mesh in the EngineMeshes package.
		// This is statically loaded as the package is likely not fully loaded
		// (otherwise, it would have been found in the above iteration).
		USkeletalMesh* DefaultSkelMesh = (USkeletalMesh*)UObject::StaticLoadObject(
			USkeletalMesh::StaticClass(), NULL, TEXT("EngineMeshes.SkeletalCube"), NULL, LOAD_None, NULL);
		check(DefaultSkelMesh);
		SkelMeshCombo->Append( *DefaultSkelMesh->GetName(), DefaultSkelMesh );
		SkelMeshAux1Combo->Append( *DefaultSkelMesh->GetName(), DefaultSkelMesh );
		SkelMeshAux2Combo->Append( *DefaultSkelMesh->GetName(), DefaultSkelMesh );
		SkelMeshAux3Combo->Append( *DefaultSkelMesh->GetName(), DefaultSkelMesh );
	}

	SkelMeshCombo->Thaw();
	SkelMeshAux1Combo->Thaw();
	SkelMeshAux2Combo->Thaw();
	SkelMeshAux3Combo->Thaw();

	// Fill in MorphTargetSet combo.
	UpdateMorphSetCombo();

	// Select the skeletal mesh we were passed in.
	if(InSkelMesh)
	{
		// make sure animset PreviewSkelMeshName matches our mesh at launch */
		if( InAnimSet )
		{
			FString MeshName = SelectedSkelMesh->GetPathName();
			InAnimSet->PreviewSkelMeshName = FName(*MeshName);
		}
		SetSelectedSkelMesh(InSkelMesh, TRUE);
	}
	// Try to find a matching skeletal mesh
	else if( InAnimSet && InAnimSet->TrackBoneNames.Num() )
	{
		//ensures that even with all 0.0f match ratios, at least one mesh will be taken
		FLOAT HighestRatio = -1.f;
		USkeletalMesh*	BestSkeletalMeshMatch = NULL;

		// Test preview skeletal mesh
		USkeletalMesh* DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *InAnimSet->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
		FLOAT DefaultMatchRatio = 0.f;
		if( DefaultSkeletalMesh )
		{
			DefaultMatchRatio = InAnimSet->GetSkeletalMeshMatchRatio(DefaultSkeletalMesh);
			BestSkeletalMeshMatch = DefaultSkeletalMesh;
		}

		// If our default mesh doesn't have a full match ratio, then see if we can find a better fit.
		if( DefaultMatchRatio < 1.f )
		{
			// Find the most suitable SkeletalMesh for this AnimSet
			for( TObjectIterator<USkeletalMesh> ItMesh; ItMesh; ++ItMesh )
			{
				USkeletalMesh* SkelMeshCandidate = *ItMesh;
				if( SkelMeshCandidate != DefaultSkeletalMesh )
				{
					FLOAT MatchRatio = InAnimSet->GetSkeletalMeshMatchRatio(SkelMeshCandidate);
					if( MatchRatio > HighestRatio )
					{
						BestSkeletalMeshMatch = SkelMeshCandidate;
						HighestRatio = MatchRatio;

						// If we have found a perfect match, we can abort.
						if( Abs(1.f - MatchRatio) <= KINDA_SMALL_NUMBER )
						{
							break;
						}
					}
				}
			}
		}

		if( BestSkeletalMeshMatch )
		{
			// make sure animset PreviewSkelMeshName matches our mesh at launch */
			FString MeshName = BestSkeletalMeshMatch->GetPathName();
			InAnimSet->PreviewSkelMeshName = FName(*MeshName);
			SetSelectedSkelMesh(BestSkeletalMeshMatch, TRUE);
		}
	}
	// If none passed in, pick first in combo.
	else
	{
		if( SkelMeshCombo->GetCount() > 0 )
		{
			USkeletalMesh* DefaultMesh = (USkeletalMesh*)SkelMeshCombo->GetClientData(0);
			SetSelectedSkelMesh(DefaultMesh, TRUE);
		}
	}

	// If an animation set was passed in, try and select it. If not, select first one.
	if(InAnimSet)
	{
		SetSelectedAnimSet(InAnimSet, true);

		// If AnimSet supplied, set to Anim page.
		ResourceNotebook->SetSelection(1);
	}
	else
	{
		if( !SelectedAnimSet && AnimSetCombo->GetCount() > 0)
		{
			UAnimSet* DefaultAnimSet = (UAnimSet*)AnimSetCombo->GetClientData(0);
			SetSelectedAnimSet(DefaultAnimSet, true);
		}

		// If no AnimSet, set to Mesh page.
		ResourceNotebook->SetSelection(0);
	}

	// If a morphtargetset was passed in, try and select it.
	if(InMorphSet)
	{
		SetSelectedMorphSet(InMorphSet, TRUE);

		// If AnimSet supplied, set to Anim page.
		ResourceNotebook->SetSelection(2);
	}
	else
	{
		// if no morph is selected, then selecte default one
		if( !SelectedMorphSet && MorphSetCombo->GetCount() > 0)
		{
			UMorphTargetSet* DefaultMorphSet = (UMorphTargetSet*)MorphSetCombo->GetClientData(0);
			SetSelectedMorphSet(DefaultMorphSet, FALSE);
		}

		// If no morph set, set to Mesh page.
		ResourceNotebook->SetSelection(0);
	}

	// Fill Skeleton Tree.
	FillSkeletonTree();
	if ( PreviewSkelComp )
	{
		// Clothing may have been created in another RBPhysScene; release it so it can be created in the right one
		PreviewSkelComp->ReleaseApexClothing();
		PreviewSkelComp->InitApexClothing(RBPhysScene);
	}

	//set up accelerator keys
	wxAcceleratorEntry entries[1];
	entries[0].Set(wxACCEL_CTRL,  (int) 'C',     IDM_ANIMSET_COPY);
	wxAcceleratorTable accel(1, entries);
	SetAcceleratorTable(accel);

	// Create floor component
	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(NULL, TEXT("EditorMeshes.PhAT_FloorBox"), NULL, LOAD_None, NULL);
	check(FloorMesh);

	EditorFloorComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
	EditorFloorComp->StaticMesh = FloorMesh;
	EditorFloorComp->BlockRigidBody = TRUE;
	EditorFloorComp->Scale = 4.f;
	ASVPreviewVC->PreviewScene.AddComponent(EditorFloorComp,FMatrix::Identity);

	EditorFloorComp->BodyInstance = ConstructObject<URB_BodyInstance>( URB_BodyInstance::StaticClass() );
	EditorFloorComp->BodyInstance->InitBody(EditorFloorComp->StaticMesh->BodySetup, FMatrix::Identity, FVector(4.f), true, EditorFloorComp, RBPhysScene);

	//hide or show floor (and update collision)
	UpdateFloorComponent();

	//Set the system notifications based on the ini setting
	if (GEditor->AccessUserSettings().bAutoReimportAnimSets)
	{
		SetFileSystemNotifications(TRUE);
	}
}

WxAnimSetViewer::~WxAnimSetViewer()
{
	GCallbackEvent->UnregisterAll(this);

	//make sure to only turn off file extensions if we previously as to listen to them
	if (GEditor->AccessUserSettings().bAutoReimportAnimSets)
	{
		SetFileSystemNotifications(FALSE);
	}

	SaveSettings();
	FlushRenderingCommands();

	if ( PreviewSkelComp )
	{
		PreviewSkelComp->ReleaseApexClothing();
		PreviewSkelComp->AnimSets.Empty();
	}

	if( PreviewSkelCompRaw )
	{
		PreviewSkelCompRaw->AnimSets.Empty();
	}

	PreviewWindow->DestroyViewport();


	DestroyRBPhysScene(RBPhysScene);
}

/** Called to mark the beginning of a transaction entry for undo/redo 
 * Be sure to call Modify() on the UObject afterward
 * @param pcTransaction - unique transaction identifier
 */
bool WxAnimSetViewer::BeginTransaction(const TCHAR* pcTransaction)
{
	if (TransactionInProgress())
	{
		FString kError(*LocalizeUnrealEd("Error_FailedToBeginTransaction"));
		kError += kTransactionName;
		checkf(0, TEXT("%s"), *kError);
		return FALSE;
	}

	GEditor->BeginTransaction(pcTransaction);
	kTransactionName = FString(pcTransaction);
	bTransactionInProgress = TRUE;

	return TRUE;
}

/** Called to mark the end of a transaction entry for undo/redo 
 * @param pcTransaction - unique transaction identifier (must match last call to BeginTransaction()
 */
bool WxAnimSetViewer::EndTransaction(const TCHAR* pcTransaction)
{
	if (!TransactionInProgress())
	{
		FString kError(*LocalizeUnrealEd("Error_FailedToEndTransaction"));
		kError += kTransactionName;
		checkf(0, TEXT("%s"), *kError);
		return FALSE;
	}

	if (appStrcmp(*kTransactionName, pcTransaction) != 0)
	{
		debugf(TEXT("AnimSetViewer -   EndTransaction = %s --- Curr = %s"), 
			pcTransaction, *kTransactionName);
		return FALSE;
	}

	GEditor->EndTransaction();

	kTransactionName = TEXT("");
	bTransactionInProgress = FALSE;

	return TRUE;
}

/** Called to rollback previous begin/end transaction */
void WxAnimSetViewer::UndoTransaction()
{
	GUnrealEd->Exec( TEXT("TRANSACTION UNDO") );
}

/** Called to redo the previously undone begin/end transaction */
void WxAnimSetViewer::RedoTransaction()
{
	GUnrealEd->Exec( TEXT("TRANSACTION REDO") );
}

/** Have we called BeginTransaction() without an EndTransaction() */
bool WxAnimSetViewer::TransactionInProgress()
{
	return bTransactionInProgress;
}

/**
 * Event wrapper around copying selected animseq to the clipboard
 */
void WxAnimSetViewer::OnCopySelectedAnimSeqToClipboard(wxCommandEvent& In)
{
	if( SelectedAnimSeq )
	{
		appClipboardCopy(*SelectedAnimSeq->SequenceName.GetNameString());
	}
}


/**
 * Open a PSA file with the given name, and returns how many animations this PSA has and list of names. 
 * This is partially duplicated function with UEditorEngine::ImportPSAIntoAnimSet
 * @param Filename - PSA file name
 * @param ListOfAnimations - out list of animations this PSA has
 * @return # of animations it found
 */
INT GetListOfAnimations(const TCHAR* Filename, TArray<FString> & ListOfAnimations )
{
	// Open skeletal animation key file and read header.
	FArchive* AnimFile = GFileManager->CreateFileReader( Filename, 0, GLog );
	if( !AnimFile )
	{
		debugf( LocalizeSecure(LocalizeUnrealEd("Error_ErrorOpeningAnimFile"), Filename) );
		return 0;
	}

	// Read main header
	VChunkHeader ChunkHeader;
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );
	if( AnimFile->IsError() )
	{
		debugf( LocalizeSecure(LocalizeUnrealEd("Error_ErrorReadingAnimFile"), Filename) );
		delete AnimFile;
		return 0;
	}

	// First I need to verify if curve data would exist
	// if version is higher than this, curve data is expected
	UBOOL bCheckCurveData = ChunkHeader.TypeFlag >= 20090127;
	
	/////// Read the bone names - convert into array of FNames.
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	INT NumPSATracks = ChunkHeader.DataCount; // Number of tracks of animation. One per bone.

	struct FNamedBoneBinaryNoAlign
	{
		ANSICHAR   Name[64];	// Bone's name
		DWORD      Flags;		// reserved
		INT        NumChildren; //
		INT		   ParentIndex;	// 0/NULL if this is the root bone.  
		VJointPosNoAlign  BonePos;	    //
	};

	/** This is due to keep the alignment working with ActorX format 
	 * FQuat in ActorX isn't 16 bit aligned, so when serialize using sizeof, this does not match up, 
	 */
	TArray<FNamedBoneBinaryNoAlign> RawBoneNamesBinNoAlign;
	RawBoneNamesBinNoAlign.Add(NumPSATracks);
	AnimFile->Serialize( &RawBoneNamesBinNoAlign(0), sizeof(FNamedBoneBinaryNoAlign) * ChunkHeader.DataCount);
	/////// Read the animation sequence infos
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	INT NumPSASequences = ChunkHeader.DataCount;

	// emtpy list
	ListOfAnimations.Empty(NumPSASequences);

	// read raw anim seq info
	TArray<AnimInfoBinary> RawAnimSeqInfo; // Array containing per-sequence information (sequence name, key range etc)
	RawAnimSeqInfo.Add(NumPSASequences);
	AnimFile->Serialize( &RawAnimSeqInfo(0), sizeof(AnimInfoBinary) * ChunkHeader.DataCount);

	// iterate through and copy back to output data
	for ( INT I=0; I<RawAnimSeqInfo.Num(); ++I )
	{
		// add the list of anims to
		ListOfAnimations.AddItem(RawAnimSeqInfo(I).Name);
	}

	// Have now read all information from file - can release handle.
	delete AnimFile;
	AnimFile = NULL;

	return ListOfAnimations.Num();
}

/* do I need to reimport from the file name given to current animset?
 * @param Filename: PSA filename
 * @param CurrentAnimSet : animset that I need to compare to
*/
UBOOL NeedToReimportAnimations(const TCHAR* Filename, UAnimSet * CurrentAnimset)
{
	TArray<FString> AnimList;
	// get list of animations in the file name
	if ( GetListOfAnimations(Filename, AnimList) > 0 )
	{
		// if so compare with CurrrentAnimSEt
		for ( INT I=0; I<AnimList.Num(); ++I )
		{
			// this is a bit tricky - if we don't want everything to be reimported?
			// for now we decide to import all of them if we find same one after talking to Jay H
			if ( CurrentAnimset->FindAnimSequence(FName(*AnimList(I))) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/** Handle callback events. */
void WxAnimSetViewer::Send( ECallbackEventType InType )
{
	/** Handle property selection change events. */
	if( InType == CALLBACK_PropertySelectionChange )
	{
		if( PreviewSkelComp )
		{
			USkeletalMesh* SkelMesh = PreviewSkelComp->SkeletalMesh;

			// check to see if any properties selected in the property window are specific to each section - if so, mark that section as "selected"
			for( INT LodIndex = 0; LodIndex < SkelMesh->LODModels.Num(); LodIndex++ )
			{
				FStaticLODModel& LodModel = ( FStaticLODModel& )SkelMesh->LODModels( LodIndex );
				for( INT Section = 0; Section < LodModel.Sections.Num(); Section++ )
				{
					FSkelMeshSection& MeshSection = LodModel.Sections( Section );
					UBOOL bIsSelected = MeshProps->IsPropertyOrChildrenSelected( TEXT( "bEnableShadowCasting" ), Section )
						|| MeshProps->IsPropertyOrChildrenSelected( TEXT( "TriangleSortSettings" ), Section )
						|| MeshProps->IsPropertyOrChildrenSelected( TEXT( "Materials" ), MeshSection.MaterialIndex )
						|| MeshProps->IsPropertyOrChildrenSelected( TEXT( "ClothingAssets" ), Section );

					if( bIsSelected != MeshSection.bSelected )
					{
						MeshSection.bSelected = bIsSelected;
						SkelMesh->PostEditChange();
					}
				}
			}
		}
	}
}

/** Handle callback events. */
void WxAnimSetViewer::Send( ECallbackEventType InType, const FString& InString, UObject* InObject)
{
	/** Handle global file changed events. */
	if( InType == CALLBACK_FileChanged )
	{
		if (GEditor->AccessUserSettings().bAutoReimportAnimSets)
		{
			FFilename FileName = InString;
			FString FileExtension = FileName.GetExtension();
			//extra guard in case of other file listeners sending events
			//if (!((FileExtension == "psk") || (FileExtension == "psa") || (FileExtension == "fbx")))
			// for now we only support PSA. 
			// PSK takes too long to import and FBX is still WIP
			if (!(FileExtension == "psa"))
			{
				//not one of the file extensions to listen for.  Reject.
				return;
			}

			if ( !SelectedAnimSet )
			{
				return;
			}

			FString AnimationName = FileName.GetBaseFilename();

			//assume this isn't a file we care about
			UBOOL bPerformImport = NeedToReimportAnimations(*FileName, SelectedAnimSet);

			// if we need to reimport
			if (bPerformImport)
			{
				//Actually perform the import of the animation
				GApp->LastDir[LD_PSA] = FileName.GetPath(); // Save path as default for next time.

				UEditorEngine::ImportPSAIntoAnimSet( SelectedAnimSet, *FileName, SelectedSkelMesh, TRUE );

				SelectedAnimSet->MarkPackageDirty();

				// balloon message
				FString BalloonMessage = FString::Printf(TEXT("Animation %s Reimported: Check log for detail messages. "), *AnimationName);

				// Display, via balloon, the results of the auto-import
				FShowBalloonNotification::ShowNotification( LocalizeUnrealEd("AnimSetEditor_AutoReimportTitle"), BalloonMessage, ID_BALLOON_NOTIFY_ID );
				// log out to log window - balloon message does not work on 64 bit
				debugf(*BalloonMessage);

				//cause a refresh of the available animation sets
				UpdateAnimSeqList();
				//reselect what was previously selected
				SetSelectedAnimSequence( SelectedAnimSeq );
			}
		}
	}
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxAnimSetViewer::OnSelected()
{
	Raise();
}

/**
* Called once to load AnimSetViewer settings, including window position.
*/

void WxAnimSetViewer::LoadSettings()
{
	// Load Window Position.
	FWindowUtil::LoadPosSize(TEXT("AnimSetViewer"), this, 256,256,1200,800);

	// Load the preview scene
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );
	ASVPreviewVC->PreviewScene.LoadSettings( TEXT( "AnimSetViewer" ) );
}

/**
* Writes out values we want to save to the INI file.
*/

void WxAnimSetViewer::SaveSettings()
{
	SaveDockingLayout();

	// Save the preview scene
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );
	ASVPreviewVC->PreviewScene.SaveSettings( TEXT( "AnimSetViewer" ) );

	// Update the custom fov setting, so the last closed animset editor window is the saved setting
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData && ToolBar && ToolBar->FOVSlider )
	{
		GApp->EditorFrame->ViewportConfigData->SetCustomFOV( ToolBar->FOVSlider->GetValue() );
	}

	// Save Window Position.
	FWindowUtil::SavePosSize(TEXT("AnimSetViewer"), this);
}



/**
 * Returns a handle to the anim set viewer's preview viewport client.
 */
FASVViewportClient* WxAnimSetViewer::GetPreviewVC()
{
	checkSlow( PreviewWindow );
	return PreviewWindow->ASVPreviewVC;
}

/**
 * Serializes the preview scene so it isn't garbage collected
 *
 * @param Ar The archive to serialize with
 */
void WxAnimSetViewer::Serialize(FArchive& Ar)
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	ASVPreviewVC->Serialize(Ar);
	Ar << SocketPreviews;
}

///////////////////////////////////////////////////////////////////////////////////////
// Properties window NotifyHook stuff

void WxAnimSetViewer::NotifyDestroy( void* Src )
{

}

void WxAnimSetViewer::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	if( PropertyAboutToChange )
	{
		// If it is the FaceFXAsset, NULL out the DefaultSkelMesh.
		if( PropertyAboutToChange->GetFName() == FName(TEXT("FaceFXAsset")) )
		{
			if( SelectedSkelMesh && SelectedSkelMesh->FaceFXAsset )
			{
				SelectedSkelMesh->FaceFXAsset->DefaultSkelMesh = NULL;
#if WITH_FACEFX
				OC3Ent::Face::FxActor* Actor = SelectedSkelMesh->FaceFXAsset->GetFxActor();
				if( Actor )
				{
					Actor->SetShouldClientRelink(FxTrue);
				}
#endif
			}
		}
#if WITH_APEX
		if(PropertyAboutToChange->GetFName() == FName(TEXT("ClothingAssets")))
		{
			FPropertyNode* ClothingLodNode = MeshProps->FindPropertyNode(TEXT("ClothingLodMap"));
			if(ClothingLodNode)
			{
				ClothingLodNode->SetExpanded(FALSE, TRUE);
			}
		}
#endif
	}
}

void WxAnimSetViewer::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	if( PropertyThatChanged )
	{
		// If it is the FaceFXAsset, NULL out the DefaultSkelMesh.
		if( PropertyThatChanged->GetFName() == FName(TEXT("FaceFXAsset")) )
		{
			if( SelectedSkelMesh && SelectedSkelMesh->FaceFXAsset )
			{
				SelectedSkelMesh->FaceFXAsset->DefaultSkelMesh = SelectedSkelMesh;
#if WITH_FACEFX
				OC3Ent::Face::FxActor* Actor = SelectedSkelMesh->FaceFXAsset->GetFxActor();
				if( Actor )
				{
					Actor->SetShouldClientRelink(FxTrue);
				}
#endif
				SelectedSkelMesh->FaceFXAsset->MarkPackageDirty();
			}
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("ClothingAssets")))
		{
			// If a property relating to the clothing asset assignment has changed, rebuild the apex clothing binding
			if ( PreviewSkelComp )
			{
				PreviewSkelComp->ReleaseApexClothing();
				PreviewSkelComp->InitApexClothing(RBPhysScene);
			}
		}
		else if(PropertyThatChanged->GetFName() == FName(TEXT("ClothingSectionInfo")))
		{
			// If a property relating to the clothing lod has changed, rebuild the apex clothing binding
			if ( PreviewSkelComp )
			{
				PreviewSkelComp->ReleaseApexClothing();
				PreviewSkelComp->InitApexClothing(RBPhysScene);
			}
		}
#if WITH_APEX
		else if(PropertyThatChanged->GetFName() == FName(TEXT("Materials")))
		{
			// If a material has changed, set the Apex Clothing with the new material
			if (PreviewSkelComp->GetApexClothing() && PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("Materials")) )
			{
				for ( INT i = 0; i < PreviewSkelComp->SkeletalMesh->Materials.Num(); ++i )
				{
					PreviewSkelComp->SetMaterial(i, PreviewSkelComp->SkeletalMesh->Materials(i));
				}
			}
		}
#endif
		else if(PropertyThatChanged->GetFName() == FName(TEXT("bForceCPUSkinning")))
		{
			FComponentReattachContext ReattachContext(PreviewSkelComp);
			FComponentReattachContext ReattachContextRaw(PreviewSkelCompRaw);
			FComponentReattachContext ReattachContext1(PreviewSkelCompAux1);
			FComponentReattachContext ReattachContext2(PreviewSkelCompAux2);
			FComponentReattachContext ReattachContext3(PreviewSkelCompAux3);
		}
		else if( PropertyThatChanged->GetFName() == FName(TEXT("Notifies")) )
		{
			if( SelectedAnimSeq && GPropertyWindowManager && GPropertyWindowManager->ChangeType == EPropertyChangeType::ValueSet )
			{
				// Ensure all the notifies have their parent set as their outer
				for ( INT iIndex = 0; iIndex < SelectedAnimSeq->Notifies.Num(); iIndex++ )
				{
					FAnimNotifyEvent* AnimNotifyEvent = &SelectedAnimSeq->Notifies(iIndex);
					if( AnimNotifyEvent )
					{
						UObject* AnimNotify = AnimNotifyEvent->Notify;
						if( AnimNotify && AnimNotify->GetOuter() != SelectedAnimSeq )
						{
							// By replacing the notify completely, it solves the issue of shallow copies. 
							AnimNotifyEvent->Notify = Cast<UAnimNotify>(UObject::StaticDuplicateObject(AnimNotify, AnimNotify, SelectedAnimSeq,NULL));
						}
					}
				}
			}
		}
		else if( PropertyThatChanged->GetFName() == FName(TEXT("MetaData")) )
		{
			if( SelectedAnimSeq && GPropertyWindowManager && GPropertyWindowManager->ChangeType == EPropertyChangeType::ValueSet )
			{
				// Ensure all the metadata have their parent set as their outer
				for ( INT iIndex = 0; iIndex < SelectedAnimSeq->MetaData.Num(); iIndex++ )
				{
					UAnimMetaData* AnimMetaData = SelectedAnimSeq->MetaData(iIndex);
					if( AnimMetaData && AnimMetaData->GetOuter() != SelectedAnimSeq )
					{
						// By replacing the notify completely, it solves the issue of shallow copies. 
						SelectedAnimSeq->MetaData(iIndex) = Cast<UAnimMetaData>(UObject::StaticDuplicateObject(AnimMetaData, AnimMetaData, SelectedAnimSeq,NULL));
					}
				}
			}
		}

		//@todo. Do this the 'right' way...
		bResampleAnimNotifyData = FALSE;
		UObject* Outer = PropertyThatChanged->GetOuter();
		UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Outer);
		if (ScriptStruct != NULL)
		{
			FAnimNotifyEvent* Event = (FAnimNotifyEvent*)ScriptStruct;
			if (ScriptStruct->GetName() == TEXT("AnimNotifyEvent"))
			{
				bResampleAnimNotifyData = TRUE;
			}
		}
		UAnimNotify_Trails* TrailsNotify = Cast<UAnimNotify_Trails>(Outer);
		if (TrailsNotify)
		{
			bResampleAnimNotifyData = TRUE;
		}

		UClass* OuterClass = Cast<UClass>(Outer);
		if (OuterClass)
		{
			if (OuterClass->GetName() == TEXT("AnimNotify_Trails"))
			{
				bResampleAnimNotifyData = TRUE;
			}
		}
	}

	// Might have changed UseTranslationBoneNames array. We have to fix any existing FAnimSetMeshLinkup objects in selected AnimSet.
	// Would be nice to not do this all the time, but PropertyThatChanged seems flaky when editing arrays (sometimes NULL)
	if( SelectedAnimSet)
	{
		SelectedAnimSet->BoneUseAnimTranslation.Empty();
		SelectedAnimSet->ForceUseMeshTranslation.Empty();
		SelectedAnimSet->LinkupCache.Empty();
		SelectedAnimSet->SkelMesh2LinkupCache.Empty();

		// We need to re-init any skeletal mesh components now, because they might still have references to linkups in this set.
		for(TObjectIterator<USkeletalMeshComponent> It;It;++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if(!SkelComp->IsPendingKill() && !SkelComp->IsTemplate())
			{
				SkelComp->InitAnimTree();
			}
		}
	}

	// Might have changed ClothBones array - so need to rebuild cloth data.
	// Would be nice to not do this all the time, but PropertyThatChanged seems flaky when editing arrays (sometimes NULL)
	{
		PreviewSkelComp->TermClothSim(NULL);
		check(SelectedSkelMesh);
		SelectedSkelMesh->ClearClothMeshCache();
		SelectedSkelMesh->BuildClothMapping();
		if(PreviewSkelComp->bEnableClothSimulation)
		{
			PreviewSkelComp->InitClothSim(RBPhysScene);
		}
		// Push any changes to cloth param to the running cloth sim.
		PreviewSkelComp->UpdateClothParams();

		PreviewSkelComp->TermSoftBodySim(NULL);
		if(PreviewSkelComp->bEnableSoftBodySimulation)
		{
			PreviewSkelComp->InitSoftBodySim(RBPhysScene, TRUE);
		}
		PreviewSkelComp->UpdateSoftBodyParams();
	}

	// In case we change Origin/RotOrigin, update the components.
	UpdateSkelComponents();
}

void WxAnimSetViewer::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}

/** Toggle the 'Socket Manager' window. */
void WxAnimSetViewer::OnOpenSocketMgr(wxCommandEvent &In)
{
	if(!SocketMgr)
	{
		SocketMgr = new WxSocketManager( this, -1 );
		SocketMgr->Show();

		ToolBar->ToggleTool(IDM_ANIMSET_OPENSOCKETMGR, true);

		UpdateSocketList();
		SetSelectedSocket(NULL);

		FASVViewportClient* ASVPreviewVC = GetPreviewVC();
		check( ASVPreviewVC );
		ASVPreviewVC->Viewport->Invalidate();
	}
	else
	{
		SocketMgr->Close();

		ToolBar->ToggleTool(IDM_ANIMSET_OPENSOCKETMGR, false);
	}
}

/** Toggles the modal animation Animation compression dialog. */
void WxAnimSetViewer::OnOpenAnimationCompressionDlg(wxCommandEvent &In)
{
	if ( !DlgAnimationCompression )
	{
		DlgAnimationCompression = new WxDlgAnimationCompression( this, -1 );
		DlgAnimationCompression->Show();

		ToolBar->ToggleTool( IDM_ANIMSET_OpenAnimationCompressionDlg, true );
		MenuBar->Check( IDM_ANIMSET_OpenAnimationCompressionDlg, true );
	}
	else
	{
		DlgAnimationCompression->Close();
		DlgAnimationCompression = NULL;
		ToolBar->ToggleTool( IDM_ANIMSET_OpenAnimationCompressionDlg, false );
		MenuBar->Check( IDM_ANIMSET_OpenAnimationCompressionDlg, false );
	}
}

/** Updates the contents of the status bar, based on e.g. the selected set/sequence. */
void WxAnimSetViewer::UpdateStatusBar()
{
	StatusBar->UpdateStatusBar( this );
}

/** Updates the contents of the status bar, based on e.g. the selected set/sequence. */
void WxAnimSetViewer::UpdateMaterialList()
{
	INT I=0;
	
 	if (PreviewSkelComp && PreviewSkelComp->SkeletalMesh)
 	{
		INT TotalNum = Min(PreviewSkelComp->SkeletalMesh->Materials.Num(), 5);

 		for (; I<TotalNum; ++I)
		{
			MenuBar->SectionSubMenu->Enable(IDM_ANIMSET_VERTEX_SECTION_0+I, true);
		}
	}

	for (; I<5; ++I)
	{
		MenuBar->SectionSubMenu->Enable(IDM_ANIMSET_VERTEX_SECTION_0+I, false);
	}

	// select section
	SelectSection(0);
}

/** Create a new Socket object and add it to the selected SkeletalMesh's Sockets array. */
void WxAnimSetViewer::OnNewSocket( wxCommandEvent& In )
{
	if(SocketMgr)
	{
		// We only allow sockets to be created to bones that are in all LODs.

		// Get the bones we'll be updating if using the lowest LOD.
		FStaticLODModel& LODModel = SelectedSkelMesh->LODModels( SelectedSkelMesh->LODModels.Num()-1 );
		TArray<BYTE> LODBones = LODModel.RequiredBones;

		// Make list of bone names in lowest LOD.
		TArray<FString> BoneNames;
		BoneNames.AddZeroed( LODBones.Num() );
		for(INT i=0; i<LODBones.Num(); i++)
		{
			FName BoneName = SelectedSkelMesh->RefSkeleton( LODBones(i) ).Name;
			BoneNames(i) = BoneName.ToString();
		}

		// Display dialog and let user pick which bone to create a Socket for.
		WxDlgGenericComboEntry BoneDlg;
		if( BoneDlg.ShowModal( TEXT("NewSocket"), TEXT("BoneName"), BoneNames, 0, TRUE ) == wxID_OK )
		{
			FName BoneName = FName( *BoneDlg.GetSelectedString() );
			if(BoneName != NAME_None)
			{
				WxDlgGenericStringEntry NameDlg;
				UBOOL bLoopAddSocket = TRUE;

				while(bLoopAddSocket)
				{
					if( NameDlg.ShowModal( TEXT("NewSocket"), TEXT("SocketName"), TEXT("") ) == wxID_OK )
					{
						const FString& NewName = NameDlg.GetEnteredString();
						UBOOL bNameAlreadyExists = FALSE;
						for(INT SocketIndex = 0; SocketIndex < SelectedSkelMesh->Sockets.Num(); ++SocketIndex)
						{
							if(NewName == SelectedSkelMesh->Sockets(SocketIndex)->SocketName.GetNameString())
							{
								bNameAlreadyExists = TRUE;
								break;
							}
						}

						if(bNameAlreadyExists)
						{
							appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("SocketAlreadyExists"), *NewName));
							continue;
						}

						FName NewSocketName = FName( *NewName );
						if(NewSocketName != NAME_None)
						{
							USkeletalMeshSocket* NewSocket = ConstructObject<USkeletalMeshSocket>( USkeletalMeshSocket::StaticClass(), SelectedSkelMesh );
							check(NewSocket);

							NewSocket->SocketName = NewSocketName;
							NewSocket->BoneName = BoneName;

							SelectedSkelMesh->Sockets.AddItem(NewSocket);
							SelectedSkelMesh->MarkPackageDirty();

							UpdateSocketList();
							SetSelectedSocket(NewSocket);
						}
					}

					// If a valid name wasn't entered then continue was called to loop over which means you can't get to this point
					bLoopAddSocket = FALSE;
				}
			}
		}
	}
}

/** Delete the currently selected Socket. */
void WxAnimSetViewer::OnDeleteSocket( wxCommandEvent& In )
{
	if(SocketMgr)
	{
		if(SelectedSocket)
		{
			INT CurrentSelection = SocketMgr->SocketList->GetSelection();

			SelectedSkelMesh->Sockets.RemoveItem(SelectedSocket);
			SelectedSkelMesh->MarkPackageDirty();

			UpdateSocketList();

			// Select the next best socket at this index
			const INT NumSockets = static_cast<INT>(SocketMgr->SocketList->GetCount());
			if ( CurrentSelection > NumSockets - 1 )
			{
				CurrentSelection = NumSockets - 1;
			}

			USkeletalMeshSocket* NewSkelMeshSocket = NULL;
			if( CurrentSelection >= 0 )
			{
				NewSkelMeshSocket = ( USkeletalMeshSocket* )SocketMgr->SocketList->GetClientData( CurrentSelection );
			}
			
			SetSelectedSocket( NewSkelMeshSocket );

			RecreateSocketPreviews();

			FASVViewportClient* ASVPreviewVC = GetPreviewVC();
			check( ASVPreviewVC );
			ASVPreviewVC->Viewport->Invalidate();
		}
	}
}

/** 
*	Handler called when clicking on a socket from the list. 
*	Sets the socket properties into the property window, and set the SelectedSocket variable.
*/
void WxAnimSetViewer::OnClickSocket( wxCommandEvent& In )
{
	if(SocketMgr)
	{
		// Get the index of the item we clicked on.
		INT SocketIndex = SocketMgr->SocketList->GetSelection();
		if( SocketIndex != -1 )
		{
			USkeletalMeshSocket* SkelMeshSocket = ( USkeletalMeshSocket* )SocketMgr->SocketList->GetClientData( SocketIndex );
			if( SkelMeshSocket )
			{
				SetSelectedSocket( SkelMeshSocket );
			}
		}
	}
}

/** Called when clicking any of the socket movement mode buttons, to change the current mode. */
void WxAnimSetViewer::OnSocketMoveMode( wxCommandEvent& In )
{
	if(SocketMgr)
	{
		INT Id = In.GetId();
		if(Id == IDM_ANIMSET_SOCKET_TRANSLATE)
		{
			SetSocketMoveMode(SMM_Translate);
		}
		else if(Id == IDM_ANIMSET_SOCKET_ROTATE)
		{
			SetSocketMoveMode(SMM_Rotate);
		}
	}
}

/**
 * Toggles whether or not active file listening should be engaged
 */
void WxAnimSetViewer::OnToggleActiveFileListen( wxCommandEvent& In )
{
	GEditor->AccessUserSettings().bAutoReimportAnimSets = !GEditor->AccessUserSettings().bAutoReimportAnimSets;
	GEditor->SaveUserSettings();
	SetFileSystemNotifications(GEditor->AccessUserSettings().bAutoReimportAnimSets);
}

/**
 * Helper function to enable and disable file listening that puts the file extensions local to one function
 */
void WxAnimSetViewer::SetFileSystemNotifications(const UBOOL bListenOnOff)
{
#if WITH_MANAGED_CODE
	SetFileSystemNotificationsForAnimSet(bListenOnOff);
#endif
}

/**
 * Event that fires to update menu items for active file listening
 */
void WxAnimSetViewer::UI_ActiveFileListen( wxUpdateUIEvent& In )
{
	In.Check( GEditor->AccessUserSettings().bAutoReimportAnimSets == TRUE );
}


/** Iterate over all sockets clearing their preview skeletal/static mesh. */
void WxAnimSetViewer::OnClearPreviewMeshes( wxCommandEvent& In )
{
	ClearSocketPreviews();

	for(INT i=0; i<SelectedSkelMesh->Sockets.Num(); i++)
	{
		USkeletalMeshSocket* Socket = SelectedSkelMesh->Sockets(i);
		Socket->PreviewSkelMesh = NULL;
		Socket->PreviewStaticMesh = NULL;
		Socket->PreviewSkelComp = NULL;
		Socket->PreviewParticleSystem = NULL;
	}

	if(SocketMgr)
	{
		SocketMgr->SocketProps->SetObject( SelectedSocket, EPropertyWindowFlags::NoFlags );
	}
}

/** Copy socket info from mesh to clipboard. */
void WxAnimSetViewer::OnCopySockets( wxCommandEvent& In )
{
	// First, clear existing buffer
	GUnrealEd->SkelSocketPasteBuffer.Empty();

	// Then copy data into it.
	GUnrealEd->SkelSocketPasteBuffer.AddZeroed(SelectedSkelMesh->Sockets.Num());
	for(INT i=0; i<SelectedSkelMesh->Sockets.Num(); i++)
	{
		USkeletalMeshSocket* Socket = SelectedSkelMesh->Sockets(i);
		GUnrealEd->SkelSocketPasteBuffer(i).SocketName			= Socket->SocketName;
		GUnrealEd->SkelSocketPasteBuffer(i).BoneName			= Socket->BoneName;
		GUnrealEd->SkelSocketPasteBuffer(i).RelativeLocation	= Socket->RelativeLocation;
		GUnrealEd->SkelSocketPasteBuffer(i).RelativeRotation	= Socket->RelativeRotation;
		GUnrealEd->SkelSocketPasteBuffer(i).RelativeScale		= Socket->RelativeScale;
	}
}

/** Rename a socket specified by the user */
void WxAnimSetViewer::OnRenameSockets( wxCommandEvent& In )
{
// Get the item we clicked on.
	INT SocketIndex = SocketMgr->SocketList->GetSelection();
	USkeletalMeshSocket* SkelMeshSocket = ( USkeletalMeshSocket* )SocketMgr->SocketList->GetClientData( SocketIndex );

	if (SkelMeshSocket)
	{
		WxDlgGenericStringEntry NameDlg;
		UBOOL bLoopAddSocket = TRUE;
		while(bLoopAddSocket)
		{
			//Present the rename dialog
			if( NameDlg.ShowModal( TEXT("RenameSocket"), TEXT("SocketName"), *SkelMeshSocket->SocketName.ToString() ) == wxID_OK )
			{
				//Repeat until user cancels or gets a unique name
				const FString& NewName = NameDlg.GetEnteredString();

				UBOOL bNameAlreadyExists = FALSE;
				UBOOL bSameSocket = FALSE;
				for(INT AllSocketsIndex = 0; AllSocketsIndex < SelectedSkelMesh->Sockets.Num(); ++AllSocketsIndex)
				{
					if(NewName == SelectedSkelMesh->Sockets(AllSocketsIndex)->SocketName.GetNameString())
					{
						bNameAlreadyExists = TRUE;
						if (SocketIndex == AllSocketsIndex)
						{
							bSameSocket = FALSE;
						}
						break;
					}
				}

				if(bNameAlreadyExists)
				{
					if (bSameSocket)
					{
						//Just exit out without doing anything
						bLoopAddSocket = FALSE;
					}
					appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("SocketAlreadyExists"), *NewName));
					continue;
				}

				//Change the name
				FName NewSocketName = FName( *NewName );
				if(NewSocketName != NAME_None)
				{
					SkelMeshSocket->SocketName = NewSocketName;
					SelectedSkelMesh->MarkPackageDirty();

					// Rename - in order to make sorting work we need to delete and add the item (rather than doing a SetText())
					SocketMgr->SocketList->Delete( SocketIndex );
					SocketMgr->SocketList->Append( *( SkelMeshSocket->SocketName.ToString() ), SkelMeshSocket );
				}
			}

			// If a valid name wasn't entered then continue was called to loop over which means you can't get to this point
			bLoopAddSocket = FALSE;
		}
	}
}

/** Paste socket info from clipboard to current mesh. */
void WxAnimSetViewer::OnPasteSockets( wxCommandEvent& In )
{
	// First check buffer is not empty.
	if(GUnrealEd->SkelSocketPasteBuffer.Num() == 0)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("NoSocketsToPaste") );
		return;
	}

	// Then iterate over each one
	for(INT i=0; i<GUnrealEd->SkelSocketPasteBuffer.Num(); i++)
	{
		FName SocketName = GUnrealEd->SkelSocketPasteBuffer(i).SocketName;

		// See if we have a socket with this name.
		UBOOL bNameAlreadyExists = FALSE;
		for(INT SocketIndex = 0; SocketIndex < SelectedSkelMesh->Sockets.Num(); ++SocketIndex)
		{
			if(SocketName == SelectedSkelMesh->Sockets(SocketIndex)->SocketName)
			{
				bNameAlreadyExists = TRUE;
				break;
			}
		}

		if(bNameAlreadyExists)
		{
			appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("SocketAlreadyExists"), *SocketName.ToString()));
			continue;
		}

		// Check there is a bone with the bone name.
		FName BoneName = GUnrealEd->SkelSocketPasteBuffer(i).BoneName;
		INT BoneIndex = SelectedSkelMesh->MatchRefBone( BoneName );
		if(BoneIndex == INDEX_NONE)
		{
			appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("SocketBoneNotFound"), *BoneName.ToString(), *SocketName.ToString()));
			continue;
		}

		// Things look good - create socket, and add to skel mesh
		USkeletalMeshSocket* NewSocket = ConstructObject<USkeletalMeshSocket>( USkeletalMeshSocket::StaticClass(), SelectedSkelMesh );
		if(NewSocket)
		{
			NewSocket->SocketName = SocketName;
			NewSocket->BoneName = BoneName;

			NewSocket->RelativeLocation = GUnrealEd->SkelSocketPasteBuffer(i).RelativeLocation;
			NewSocket->RelativeRotation = GUnrealEd->SkelSocketPasteBuffer(i).RelativeRotation;
			NewSocket->RelativeScale = GUnrealEd->SkelSocketPasteBuffer(i).RelativeScale;

			SelectedSkelMesh->Sockets.AddItem(NewSocket);
		}
	}

	SelectedSkelMesh->MarkPackageDirty();
	UpdateSocketList();
}

/** When the user slides the progressive drawing slider */
void WxAnimSetViewer::OnProgressiveSliderChanged(wxScrollEvent& In)
{
	PreviewSkelComp->ProgressiveDrawingFraction = ((FLOAT)In.GetPosition()) / 10000.f;
}

/** Resets the FOV slider */
void WxAnimSetViewer::OnFOVReset( wxCommandEvent& In )
{
	SetFOV( ( GEditor ? ((INT)GEditor->FOVAngle) : 90 ) );
}

/** When the user slides the FOV slider */
void WxAnimSetViewer::OnFOVSliderChanged(wxScrollEvent& In)
{
	SetFOV( In.GetPosition() );
}

/** Sets the FOV and updated the icons */
void WxAnimSetViewer::SetFOV(const INT iFOV)
{
	// Set the viewport to the parsed value
	if ( PreviewWindow && PreviewWindow->ASVPreviewVC )
	{
		PreviewWindow->ASVPreviewVC->ViewFOV = ((FLOAT)iFOV);
	}
	// Update the custom FOV so that any additional animset windows that open while this is still open have this value too
	if ( GApp && GApp->EditorFrame && GApp->EditorFrame->ViewportConfigData )
	{
		GApp->EditorFrame->ViewportConfigData->SetCustomFOV( iFOV );
	}
	// Update the slider to also show the new value
	if ( ToolBar && ToolBar->FOVSlider )
	{
		ToolBar->FOVSlider->SetValue( iFOV );
	}
}

//////////////////////////////////////////////////////////////////////////

/** Set the movement mode for the selected socket to the one supplied. */
void WxAnimSetViewer::SetSocketMoveMode( EASVSocketMoveMode NewMode )
{
	if(SocketMgr)
	{
		SocketMgr->MoveMode = NewMode;

		if(SocketMgr->MoveMode == SMM_Rotate)
		{
			SocketMgr->ToolBar->ToggleTool( IDM_ANIMSET_SOCKET_TRANSLATE, false );
			SocketMgr->ToolBar->ToggleTool( IDM_ANIMSET_SOCKET_ROTATE, true );
		}
		else
		{
			SocketMgr->ToolBar->ToggleTool( IDM_ANIMSET_SOCKET_TRANSLATE, true );
			SocketMgr->ToolBar->ToggleTool( IDM_ANIMSET_SOCKET_ROTATE, false );
		}

		SocketMgr->UpdateTextEntry();

		// Flush hit proxies so we get them for the new widget.
		FASVViewportClient* ASVPreviewVC = GetPreviewVC();
		check( ASVPreviewVC );
		ASVPreviewVC->Viewport->Invalidate();
	}
}

/** Set the currently selected socket to the supplied one */
void WxAnimSetViewer::SetSelectedSocket(USkeletalMeshSocket* InSocket)
{
	if( InSocket && SocketMgr )
	{
		INT SocketIndex = INDEX_NONE;
		for( UINT iSocket = 0; iSocket < SocketMgr->SocketList->GetCount(); iSocket++ )
		{
			if( ( ( USkeletalMeshSocket* )SocketMgr->SocketList->GetClientData( iSocket ) ) == InSocket )
			{
				SocketIndex = iSocket;
			}
		}
		if( SocketIndex != INDEX_NONE )
		{
			SelectedSocket = InSocket;
			SocketMgr->SocketProps->SetObject( SelectedSocket, EPropertyWindowFlags::NoFlags );
			SocketMgr->SocketList->SetSelection( SocketIndex );

			// Flush hit proxies so we get them for the new widget.
			FASVViewportClient* ASVPreviewVC = GetPreviewVC();
			check( ASVPreviewVC );
			ASVPreviewVC->Viewport->Invalidate();
			return;
		}
	}

	SelectedSocket = NULL;

	if(SocketMgr)
	{
		SocketMgr->SocketProps->SetObject(NULL, EPropertyWindowFlags::NoFlags );
		SocketMgr->SocketList->DeselectAll();
	}
}

/** Update the list of sockets shown in the Socket Manager list box to match that of the selected SkeletalMesh. */
void WxAnimSetViewer::UpdateSocketList()
{
	if(SocketMgr)
	{
		// Remember the current selection.
		UINT CurrentSelection = static_cast<UINT>(SocketMgr->SocketList->GetSelection());

		SocketMgr->SocketList->Clear();

		// Remove empty sockets
		for(INT i=0; i<SelectedSkelMesh->Sockets.Num(); i++)
		{
			USkeletalMeshSocket* Socket = SelectedSkelMesh->Sockets(i);
			if (NULL == Socket)
			{
				SelectedSkelMesh->Sockets.Remove(i);
				i--;
			}
		}

		for(INT i=0; i<SelectedSkelMesh->Sockets.Num(); i++)
		{
			USkeletalMeshSocket* Socket = SelectedSkelMesh->Sockets(i);
			SocketMgr->SocketList->Append( *(Socket->SocketName.ToString()), Socket );
		}

		// Restore the selection we had.
		if(CurrentSelection >= 0 && CurrentSelection < SocketMgr->SocketList->GetCount())
		{
			SocketMgr->SocketList->SetSelection(CurrentSelection);
		}
	}
}

/** Destroy all components being used to preview sockets. */
void WxAnimSetViewer::ClearSocketPreviews()
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );

	for(INT i=0; i<SocketPreviews.Num(); i++)
	{
		UPrimitiveComponent* PrimComp = SocketPreviews(i).PreviewComp;
		PreviewSkelComp->DetachComponent(PrimComp);
		ASVPreviewVC->PreviewScene.RemoveComponent(PrimComp);
	}
	SocketPreviews.Empty();
}


/** Update the Skeletal and StaticMesh Components used to preview attachments in the editor. */
void WxAnimSetViewer::RecreateSocketPreviews()
{
	ClearSocketPreviews();

	if(SelectedSkelMesh)
	{
		FASVViewportClient* ASVPreviewVC = GetPreviewVC();
		check( ASVPreviewVC );

		// Then iterate over Sockets creating component for any preview skeletal/static meshes.
		for(INT i=0; i<SelectedSkelMesh->Sockets.Num(); i++)
		{
			USkeletalMeshSocket* Socket = SelectedSkelMesh->Sockets(i);
			
			if(Socket && Socket->PreviewSkelMesh)
			{
				// Create SkeletalMeshComponent and fill in mesh and scene.
				USkeletalMeshComponent* NewSkelComp = ConstructObject<USkeletalMeshComponent>(USkeletalMeshComponent::StaticClass());
				NewSkelComp->SkeletalMesh = Socket->PreviewSkelMesh;

				// Attach component to this socket.
				PreviewSkelComp->AttachComponentToSocket(NewSkelComp, Socket->SocketName);

				// Assign SkelComp, so we can edit its properties.
				Socket->PreviewSkelComp = NewSkelComp;
				// Create AnimNodeSequence for previewing animation playback
				Socket->PreviewSkelComp->Animations = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass());

				// And keep track of it.
				new( SocketPreviews )FASVSocketPreview(Socket, NewSkelComp);
			}

			if(Socket && Socket->PreviewStaticMesh)
			{
				// Create StaticMeshComponent and fill in mesh and scene.
				UStaticMeshComponent* NewMeshComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
				NewMeshComp->StaticMesh = Socket->PreviewStaticMesh;

				// Attach component to this socket.
				PreviewSkelComp->AttachComponentToSocket(NewMeshComp, Socket->SocketName);

				// And keep track of it
				new( SocketPreviews )FASVSocketPreview(Socket, NewMeshComp);
			}


			if(Socket && Socket->PreviewParticleSystem)
			{
				// Create UParticleSystemComponent and fill in template and scene.
				UParticleSystemComponent* NewPSysComp = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass());
				NewPSysComp->SetTemplate(Socket->PreviewParticleSystem);
				NewPSysComp->SetTickGroup(TG_PostUpdateWork);

				// Attach component to this socket.
				PreviewSkelComp->AttachComponentToSocket(NewPSysComp, Socket->SocketName);

				// And keep track of it
				new( SocketPreviews )FASVSocketPreview(Socket, NewPSysComp);
			}
		}
	}
}

/** Update the components being used to preview sockets. Need to call this when you change a Socket to see its affect. */
void WxAnimSetViewer::UpdateSocketPreviews()
{
	for(INT i=0; i<SocketPreviews.Num(); i++)
	{
		PreviewSkelComp->DetachComponent( SocketPreviews(i).PreviewComp );
		PreviewSkelComp->AttachComponentToSocket( SocketPreviews(i).PreviewComp, SocketPreviews(i).Socket->SocketName );
	}
}


/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxAnimSetViewer::GetDockingParentName() const
{
	return TEXT("AnimSetViewer");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxAnimSetViewer::GetDockingParentVersion() const
{
	return 0;
}

/**
 *	Update the preview window.
 */
void WxAnimSetViewer::UpdatePreviewWindow()
{
	if (PreviewWindow)
	{
		if (PreviewWindow->ASVPreviewVC)
		{
			if (PreviewWindow->ASVPreviewVC->Viewport)
			{
				PreviewWindow->ASVPreviewVC->Viewport->Invalidate();
			}
		}
	}
}

/** 
 * Update Vertex Color Render Mode : for tangents/normals
 */
void WxAnimSetViewer::UpdateVertexMode()
{
	if ( PreviewSkelComp )
	{
		PreviewSkelComp->ApplyColorRenderMode(VertexInfo.ColorOption);
		FComponentReattachContext PreviewSkelReattach(PreviewSkelComp);
		PreviewWindow->ASVPreviewVC->Invalidate();
	}
}

/** Returns the current LOD index being viewed. */
INT WxAnimSetViewer::GetCurrentLODIndex() const
{
	INT LODIndex = 0;
	if ( PreviewSkelComp )
	{
		if ( PreviewSkelComp->ForcedLodModel > 0 )
		{
			LODIndex = PreviewSkelComp->ForcedLodModel - 1;
		}
	}
	return LODIndex;
}
