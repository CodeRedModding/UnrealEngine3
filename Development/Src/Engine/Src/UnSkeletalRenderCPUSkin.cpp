/*=============================================================================
	UnSkeletalRenderCPUSkin.cpp: CPU skinned skeletal mesh rendering code.
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

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EnginePhysicsClasses.h"
#include "UnSkeletalRenderCPUSkin.h"
#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif

#if XBOX
	#include "UnSkeletalRenderCPUXe.h"
#endif

#define DEBUG_CPU_SKINNING 0

/** optimized skinning */
#if DEBUG_CPU_SKINNING

template<typename VertexType>
static void SkinVertices( FFinalSkinVertex* DestVertex, FBoneAtom* ReferenceToLocal, INT LODIndex, FStaticLODModel& LOD, TArray<FActiveMorph>& ActiveMorphs, UBOOL bUseBoneInfluences, const TArray<FVertexInfluence>& VertexInfluences, UBOOL bFullSwap );
#else
template<typename VertexType>
static FORCEINLINE void SkinVertices( FFinalSkinVertex* DestVertex, FBoneAtom* ReferenceToLocal, INT LODIndex, FStaticLODModel& LOD, TArray<FActiveMorph>& ActiveMorphs, UBOOL bUseBoneInfluences, const TArray<FVertexInfluence>& VertexInfluences, UBOOL bFullSwap );
#endif
/** 
 * Modify the vertex buffer to store bone weights in the UV coordinates for rendering 
 * @param DestVertex - already filled out vertex buffer from SkinVertices
 * @param LOD - LOD model corresponding to DestVertex 
 * @param BonesOfInterest - array of bones we want to display
 */
void CalculateBoneWeights(FFinalSkinVertex* DestVertex, FStaticLODModel& LOD, const TArray<INT>& BonesOfInterest, SkinColorRenderMode ColorMode, UBOOL bUseBoneInfluences, const TArray<FVertexInfluence>& VertexInfluences, UBOOL bFullSwap );

/*-----------------------------------------------------------------------------
	FFinalSkinVertexBuffer
-----------------------------------------------------------------------------*/

/** 
 * Initialize the dynamic RHI for this rendering resource 
 */
void FFinalSkinVertexBuffer::InitDynamicRHI()
{
	// all the vertex data for a single LOD of the skel mesh
	FStaticLODModel& LodModel = SkelMesh->LODModels(LODIdx);

	// Create the buffer rendering resource
	
	UINT Size;

	if(SkelMesh->bEnableClothTearing && (SkelMesh->ClothWeldingMap.Num() == 0))
	{
		/*
		We need to reserve extra vertices at the end of the buffer to accommodate vertices generated
		due to cloth tearing.
		*/
		Size = (LodModel.NumVertices + SkelMesh->ClothTearReserve) * sizeof(FFinalSkinVertex);
	}
	else
	{
		Size = LodModel.NumVertices * sizeof(FFinalSkinVertex);
	}

	VertexBufferRHI = RHICreateVertexBuffer(Size,NULL,RUF_Dynamic);

	// Lock the buffer.
	void* Buffer = RHILockVertexBuffer(VertexBufferRHI,0,Size,FALSE);

	// Initialize the vertex data
	// All chunks are combined into one (rigid first, soft next)
	check(LodModel.VertexBufferGPUSkin.GetNumVertices() == LodModel.NumVertices);
	FFinalSkinVertex* DestVertex = (FFinalSkinVertex*)Buffer;
	for( UINT VertexIdx=0; VertexIdx < LodModel.NumVertices; VertexIdx++ )
	{
		const FGPUSkinVertexBase* SrcVertex = LodModel.VertexBufferGPUSkin.GetVertexPtr(VertexIdx);

		DestVertex->Position = LodModel.VertexBufferGPUSkin.GetVertexPosition(VertexIdx);
		DestVertex->TangentX = SrcVertex->TangentX;
		// w component of TangentZ should already have sign of the tangent basis determinant
		DestVertex->TangentZ = SrcVertex->TangentZ;

		FVector2D UVs = LodModel.VertexBufferGPUSkin.GetVertexUV(VertexIdx,0);
		DestVertex->U = UVs.X;
		DestVertex->V = UVs.Y;

		DestVertex++;
	}

	// Unlock the buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

void FFinalSkinVertexBuffer::ReleaseDynamicRHI()
{
	VertexBufferRHI.SafeRelease();
}

/*-----------------------------------------------------------------------------
	FFinalDynamicIndexBuffer
-----------------------------------------------------------------------------*/

/** 
 * Initialize the dynamic RHI for this rendering resource 
 */
void FFinalDynamicIndexBuffer::InitDynamicRHI()
{
	// all the vertex data for a single LOD of the skel mesh
	FStaticLODModel& LodModel = SkelMesh->LODModels(LODIdx);

	// Create the buffer rendering resource
	
	UINT Size = LodModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();
	IndexBufferSize = LodModel.MultiSizeIndexContainer.GetDataTypeSize();
	if((!SkelMesh->bEnableClothTearing && !SkelMesh->bEnableValidBounds) || Size == 0 || (SkelMesh->ClothWeldingMap.Num() != 0))
	{
		return;
	}

	IndexBufferRHI = RHICreateIndexBuffer(IndexBufferSize, Size * IndexBufferSize, NULL, RUF_Dynamic);

	// Lock the buffer.
	void* Buffer = RHILockIndexBuffer(IndexBufferRHI, 0, Size * IndexBufferSize);

	if( IndexBufferSize == 4 )
	{
		DWORD* BufferWords = (DWORD *)Buffer;

		// Initialize the index data
		// Copy the index data from the LodModel.

		for(UINT i=0; i<Size; i++)
		{
			BufferWords[i] = LodModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(i);
		}
	}
	else
	{
		WORD* BufferWords = (WORD *)Buffer;

		// Initialize the index data
		// Copy the index data from the LodModel.

		for(UINT i=0; i<Size; i++)
		{
			BufferWords[i] = LodModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(i);
		}
	}

	// Unlock the buffer.
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FFinalDynamicIndexBuffer::ReleaseDynamicRHI()
{
	IndexBufferRHI.SafeRelease();
}

/*-----------------------------------------------------------------------------
	FSkeletalMeshObjectCPUSkin
-----------------------------------------------------------------------------*/

/** 
 * Constructor 
 * @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render 
 */
FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectCPUSkin(USkeletalMeshComponent* InSkeletalMeshComponent) 
:	FSkeletalMeshObject(InSkeletalMeshComponent)
,	DynamicData(NULL)
,	CachedVertexLOD(INDEX_NONE)
,	RenderColorMode(ESCRM_None)
{
	// create LODs to match the base mesh
	for( INT LODIndex=0;LODIndex < SkeletalMesh->LODModels.Num();LODIndex++ )
	{
		new(LODs) FSkeletalMeshObjectLOD(SkeletalMesh,LODIndex);
	}

	InitResources();
}

/** 
 * Destructor 
 */
FSkeletalMeshObjectCPUSkin::~FSkeletalMeshObjectCPUSkin()
{
	delete DynamicData;
}

/** 
 * Initialize rendering resources for each LOD. 
 * Blocks until init completes on rendering thread
 */
void FSkeletalMeshObjectCPUSkin::InitResources()
{
	for( INT LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs(LODIndex);
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(LODIndex);
		SkelLOD.InitResources(MeshLODInfo.bUseInstancedVertexInfluences);
	}
}

/** 
 * Release rendering resources for each LOD.
 * Blocks until release completes on rendering thread
 */
void FSkeletalMeshObjectCPUSkin::ReleaseResources()
{
	for( INT LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs(LODIndex);
		SkelLOD.ReleaseResources();
	}
}

/** 
* Enable color mode rendering in the editor
* @param color mode index - 
*/
void FSkeletalMeshObjectCPUSkin::EnableColorModeRendering(SkinColorRenderMode ColorIndex)
{
	RenderColorMode = ColorIndex;
}

/** 
 * Enable blend weight rendering in the editor
 * @param bEnabled - turn on or off the rendering mode
 * @param BonesOfInterest - array of bone indices to capture weights for
 */
void FSkeletalMeshObjectCPUSkin::EnableBlendWeightRendering(UBOOL bEnabled, const TArray<INT>& InBonesOfInterest)
{
	if ( bEnabled )
	{
		RenderColorMode = ESCRM_BoneWeights;
	}
	else
	{
		RenderColorMode = ESCRM_None;
	}

	BonesOfInterest.Empty(InBonesOfInterest.Num());
	BonesOfInterest.Append(InBonesOfInterest);
}

/**
* Called by the game thread for any dynamic data updates for this skel mesh object
* @param	LODIndex - lod level to update
* @param	InSkeletalMeshComponen - parent prim component doing the updating
* @param	ActiveMorphs - morph targets to blend with during skinning
*/
void FSkeletalMeshObjectCPUSkin::Update(INT LODIndex,USkeletalMeshComponent* InSkeletalMeshComponent,const TArray<FActiveMorph>& ActiveMorphs)
{
	// create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicSkelMeshObjectData* NewDynamicData = new FDynamicSkelMeshObjectDataCPUSkin(InSkeletalMeshComponent,LODIndex,ActiveMorphs,&DecalRequiredBoneIndices);
	
#if !NX_DISABLE_CLOTH
	((FDynamicSkelMeshObjectDataCPUSkin*)NewDynamicData)->bRemoveNonSimulatedTriangles = (InSkeletalMeshComponent->ClothSim != NULL);
#endif
	
	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SkelMeshObjectUpdateDataCommand,
		FSkeletalMeshObject*, MeshObject, this,
		FDynamicSkelMeshObjectData*, NewDynamicData, NewDynamicData,
	{
		MeshObject->UpdateDynamicData_RenderThread(NewDynamicData);
	}
	);

	if( GIsEditor )
	{
		// this does not need thread-safe update
		ProgressiveDrawingFraction = InSkeletalMeshComponent->ProgressiveDrawingFraction;
		CustomSortAlternateIndexMode = (ECustomSortAlternateIndexMode)InSkeletalMeshComponent->CustomSortAlternateIndexMode;
	}
}

/**
 * Called by the rendering thread to update the current dynamic data
 * @param	InDynamicData - data that was created by the game thread for use by the rendering thread
 */
void FSkeletalMeshObjectCPUSkin::UpdateDynamicData_RenderThread(FDynamicSkelMeshObjectData* InDynamicData)
{
	// we should be done with the old data at this point
	delete DynamicData;
	// update with new data
	DynamicData = (FDynamicSkelMeshObjectDataCPUSkin*)InDynamicData;	
	check(DynamicData);

	// update vertices using the new data
	CacheVertices(DynamicData->LODIndex,TRUE,TRUE);
}

/** 
 * Called by the game thread to toggle usage for the instanced vertex weights.
 * @param bEnabled - TRUE to enable the usage of influence weights
 * @param LODIdx - Index of the influences to toggle
 */
void FSkeletalMeshObjectCPUSkin::ToggleVertexInfluences(UBOOL bEnabled, INT LODIdx)
{
	FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(LODIdx);
	if( MeshLODInfo.bUseInstancedVertexInfluences != bEnabled && !GSystemSettings.bDisableSkeletalInstanceWeights )
	{
		ReleaseResources();
		FlushRenderingCommands();

		MeshLODInfo.bUseInstancedVertexInfluences = bEnabled;

		InitResources();
	}
}

/**
* Called by the game thread to update the instanced vertex weights
* @param LODIdx - LOD this update is for
* @param BonePairs - set of bone pairs used to find vertices that need to have their weights updated
* @param bResetInfluences - resets the array of instanced influences using the ones from the base mesh before updating
*/
void FSkeletalMeshObjectCPUSkin::UpdateVertexInfluences(INT LODIdx,
														const TArray<FBoneIndexPair>& BonePairs,
														UBOOL bResetInfluences)
{
	FDynamicUpdateVertexInfluencesData DynamicInfluencesData(
		LODIdx,
		BonePairs,
		bResetInfluences
		);

	// queue a call to update this weight data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SkelMeshObjectUpdateWeightsCommand,
		FSkeletalMeshObject*, MeshObject, this,
		FDynamicUpdateVertexInfluencesData, DynamicInfluencesData, DynamicInfluencesData,
	{
		MeshObject->UpdateVertexInfluences_RenderThread(&DynamicInfluencesData);
	}
	);
}

/**
* Called by the rendering thread to update the current dynamic weight data
* @param	InDynamicData - data that was created by the game thread for use by the rendering thread
*/
void FSkeletalMeshObjectCPUSkin::UpdateVertexInfluences_RenderThread(FDynamicUpdateVertexInfluencesData* InDynamicData)
{
	// make sure there is an instance vertex buffer that supports partial swapping of vertex weights
	// this usage requires a unique vertex buffer per skeletal mesh component instance
	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(InDynamicData->LODIdx);
	check(MeshLODInfo.InstanceWeightUsage == IWU_PartialSwap);

	// update instance weights for all LODs
	for( INT CurLODIdx=0; CurLODIdx < LODs.Num(); CurLODIdx++ )
	{
		const FSkeletalMeshObjectLOD& LOD = LODs(CurLODIdx);
		const FStaticLODModel& LODModel = SkeletalMesh->LODModels(CurLODIdx);

		if( LODModel.VertexInfluences.IsValidIndex(MeshLODInfo.InstanceWeightIdx) )
		{	
			const FSkeletalMeshVertexInfluences& VertexInfluences = LODModel.VertexInfluences(MeshLODInfo.InstanceWeightIdx);
			if( VertexInfluences.Influences.Num() > 0 &&
				VertexInfluences.Influences.Num() == LODModel.NumVertices )
			{
				//Reset all verts if requested
				if( InDynamicData->bResetInfluences )
				{
					const INT NumVertices = LODModel.VertexBufferGPUSkin.GetNumVertices();
					for (INT VertIdx=0; VertIdx<NumVertices; VertIdx++)
					{
						const FGPUSkinVertexBase* Vertex = LODModel.VertexBufferGPUSkin.GetVertexPtr(VertIdx);
						for( INT Idx=0; Idx < MAX_INFLUENCES; Idx++ )
						{
							BYTE BoneIndex = Vertex->InfluenceBones[Idx];

							LOD.VertexInfluenceBuffer.Influences(VertIdx).Weights.InfluenceWeights[Idx] = Vertex->InfluenceWeights[Idx];
							LOD.VertexInfluenceBuffer.Influences(VertIdx).Bones.InfluenceBones[Idx] = BoneIndex;
						}
					}
				}

				//Replace new influences per bone
				for( INT BonePairIdx=0; BonePairIdx < InDynamicData->BonePairs.Num();  BonePairIdx++ )
				{
					const FBoneIndexPair& BonePair = InDynamicData->BonePairs(BonePairIdx);
			
					const TArray<DWORD>* VertIndices = VertexInfluences.VertexInfluenceMapping.Find(BonePair);
					if (VertIndices)
					{
						INT NumVertices = (*VertIndices).Num();
						for (INT VertIdx=0; VertIdx<NumVertices; VertIdx++)
						{
							const INT VertexIndex = (INT)(*VertIndices)(VertIdx);
							const FVertexInfluence& VertexInfluence = VertexInfluences.Influences(VertexIndex);
							const FGPUSkinVertexBase* Vertex = LODModel.VertexBufferGPUSkin.GetVertexPtr(VertexIndex);
							for( INT Idx=0; Idx < MAX_INFLUENCES; Idx++ )
							{
								BYTE BoneIndex = VertexInfluence.Bones.InfluenceBones[Idx];
								LOD.VertexInfluenceBuffer.Influences(VertexIndex).Weights.InfluenceWeights[Idx] = VertexInfluence.Weights.InfluenceWeights[Idx];
								LOD.VertexInfluenceBuffer.Influences(VertexIndex).Bones.InfluenceBones[Idx] = BoneIndex;
							}
						}
					}
				}
			}
		}
	}
}

/**
 * Adds a decal interaction to the primitive.  This is called in the rendering thread by the skeletal mesh proxy's AddDecalInteraction_RenderingThread.
 */
void FSkeletalMeshObjectCPUSkin::AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction)
{
	for( INT LODIndex = 0 ; LODIndex < LODs.Num() ; ++LODIndex )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs(LODIndex);
		SkelLOD.AddDecalInteraction_RenderingThread( DecalInteraction );
	}
}

/**
 * Removes a decal interaction from the primitive.  This is called in the rendering thread by the skeletal mesh proxy's RemoveDecalInteraction_RenderingThread.
 */
void FSkeletalMeshObjectCPUSkin::RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent)
{
	for( INT LODIndex = 0 ; LODIndex < LODs.Num() ; ++LODIndex )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs(LODIndex);
		SkelLOD.RemoveDecalInteraction_RenderingThread( DecalComponent );
	}
}

static UBOOL ComputeTangent(FVector &t,
							const FVector &p0, const FVector2D &c0,
							const FVector &p1, const FVector2D &c1,
							const FVector &p2, const FVector2D &c2)
{
  const FLOAT epsilon = 0.0001f;
  UBOOL   Ret = FALSE;
  FVector dp1 = p1 - p0;
  FVector dp2 = p2 - p0;
  FLOAT   du1 = c1.X - c0.X;
  FLOAT   dv1 = c1.Y - c0.Y;
  if(Abs(dv1) < epsilon && Abs(du1) >= epsilon)
  {
	t = dp1 / du1;
	Ret = TRUE;
  }
  else
  {
	  FLOAT du2 = c2.X - c0.X;
	  FLOAT dv2 = c2.Y - c0.Y;
	  FLOAT det = dv1*du2 - dv2*du1;
	  if(Abs(det) >= epsilon)
	  {
		t = (dp2*dv1-dp1*dv2)/det;
		Ret = TRUE;
	  }
  }
  return Ret;
}

static inline INT ClothIdxToGraphics(INT ClothIndex, const TArray<INT>& ClothToGraphicsVertMap, INT LODNumVertices)
{
	if(ClothIndex < ClothToGraphicsVertMap.Num() )
	{
		return ClothToGraphicsVertMap(ClothIndex);
	}
	else
	{
		return (ClothIndex - ClothToGraphicsVertMap.Num()) + LODNumVertices;
	}
}

/**
 * Re-skin cached vertices for an LOD and update the vertex buffer. Note that this
 * function is called from the render thread!
 * @param	LODIndex - index to LODs
 * @param	bForce - force update even if LOD index hasn't changed
 * @param	bUpdateDecalVertices - whether to update the decal vertices
 */
void FSkeletalMeshObjectCPUSkin::CacheVertices(INT LODIndex, UBOOL bForce, UBOOL bUpdateDecalVertices) const
{
	// Source skel mesh and static lod model
	FStaticLODModel& LOD = SkeletalMesh->LODModels(LODIndex);

	// Get the destination mesh LOD.
	const FSkeletalMeshObjectLOD& MeshLOD = LODs(LODIndex);

	// only recache if lod changed
	if ( (LODIndex != CachedVertexLOD || bForce) &&
		DynamicData && 
		IsValidRef(MeshLOD.VertexBuffer.VertexBufferRHI) )
	{
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(LODIndex);

		// bone matrices
		FBoneAtom* ReferenceToLocal = &DynamicData->ReferenceToLocal(0);

		INT CachedFinalVerticesNum = LOD.NumVertices;
		if(SkeletalMesh->bEnableClothTearing && (SkeletalMesh->ClothWeldingMap.Num() == 0))
		{
			CachedFinalVerticesNum += SkeletalMesh->ClothTearReserve;
		}

		CachedFinalVertices.Empty(CachedFinalVerticesNum);
		CachedFinalVertices.Add(CachedFinalVerticesNum);

		// final cached verts
		FFinalSkinVertex* DestVertex = &CachedFinalVertices(0);

		if (DestVertex)
		{
			SCOPE_CYCLE_COUNTER(STAT_SkinningTime);
			if (LOD.VertexBufferGPUSkin.GetUseFullPrecisionUVs())
			{
				// do actual skinning
				SkinVertices< TGPUSkinVertexFloat32Uvs<> >( DestVertex, ReferenceToLocal, DynamicData->LODIndex, LOD, DynamicData->ActiveMorphs, MeshLODInfo.bUseInstancedVertexInfluences, MeshLOD.VertexInfluenceBuffer.Influences, MeshLODInfo.InstanceWeightUsage == IWU_FullSwap);
			}
			else
			{
				// do actual skinning
				SkinVertices< TGPUSkinVertexFloat16Uvs<> >( DestVertex, ReferenceToLocal, DynamicData->LODIndex, LOD, DynamicData->ActiveMorphs, MeshLODInfo.bUseInstancedVertexInfluences, MeshLOD.VertexInfluenceBuffer.Influences, MeshLODInfo.InstanceWeightUsage == IWU_FullSwap );
			}

			if (RenderColorMode!=ESCRM_None)
			{
				//Transfer bone weights we're interested in to the UV channels
				CalculateBoneWeights(DestVertex, LOD, BonesOfInterest, RenderColorMode, MeshLODInfo.bUseInstancedVertexInfluences, MeshLOD.VertexInfluenceBuffer.Influences, MeshLODInfo.InstanceWeightUsage == IWU_FullSwap);
			}
		}

#if !NX_DISABLE_CLOTH
	
		// Update graphics verts from physics info if we are running cloth
		if( DynamicData->ClothPosData.Num() > 0 && LODIndex == 0 && DynamicData->ClothBlendWeight > 0.f)
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateClothVertsTime);

			check(SkeletalMesh->NumFreeClothVerts <= SkeletalMesh->ClothToGraphicsVertMap.Num());
			check(DynamicData->ClothPosData.Num() == DynamicData->ClothNormalData.Num());
			//check(SkeletalMesh->NumFreeClothVerts <= DynamicData->ClothMeshVerts.Num());

			const INT NumIndices  = LOD.MultiSizeIndexContainer.GetIndexBuffer()->Num();
			const INT NumVertices = LOD.NumVertices;

			TArray<FVector> &ClothTangentData = CachedClothTangents;
			ClothTangentData.Reset();
			ClothTangentData.AddZeroed(NumVertices);

			
			INT NumSkinnedClothVerts = SkeletalMesh->NumFreeClothVerts;
			if(SkeletalMesh->bEnableClothTearing)
			{
				if(SkeletalMesh->ClothWeldingMap.Num() == 0)
				{
					NumSkinnedClothVerts = DynamicData->ActualClothPosDataNum;
				}
				else
				{
					NumSkinnedClothVerts = DynamicData->ClothPosData.Num();
				}
			}
			
			// Load cloth data...
			for(INT i=0; i<NumSkinnedClothVerts; i++)
			{
				// Find the index of the graphics vertex that corresponds to this cloth vertex
				const INT GraphicsIndex = ClothIdxToGraphics(i, SkeletalMesh->ClothToGraphicsVertMap, NumVertices);			
				check((GraphicsIndex >= 0) && (GraphicsIndex < CachedFinalVerticesNum));
				
				//Is this a new vertex created due to tearing...
				if(i >= SkeletalMesh->ClothToGraphicsVertMap.Num())
				{
					//Copy across texture coords for reserve vertices...
					//We track back over parents until we find the original.

					INT OrigIndex = DynamicData->ClothParentIndices(i);
					while(OrigIndex >= SkeletalMesh->ClothToGraphicsVertMap.Num())
					{
						OrigIndex = DynamicData->ClothParentIndices(OrigIndex);
					}

					INT OrigGraphicsIndex = SkeletalMesh->ClothToGraphicsVertMap(OrigIndex);

					DestVertex[GraphicsIndex].U = DestVertex[OrigGraphicsIndex].U;
					DestVertex[GraphicsIndex].V = DestVertex[OrigGraphicsIndex].V;
				}

				// Transform into local space
				const FVector LocalClothPos = DynamicData->WorldToLocal.TransformFVector( P2UScale * DynamicData->ClothPosData(i));
				
				// Blend between cloth and skinning
				DestVertex[GraphicsIndex].Position = Lerp<FVector>(DestVertex[GraphicsIndex].Position, LocalClothPos, DynamicData->ClothBlendWeight);

				// Transform normal.
				const FVector TangentZ  = DynamicData->WorldToLocal.TransformNormal( DynamicData->ClothNormalData(i) ).SafeNormal();
				DestVertex[GraphicsIndex].TangentZ = TangentZ;
			}
			
			// Compute tangents...
			for(INT i=0; i<NumIndices; i+=3)
			{
				const INT AI = LOD.MultiSizeIndexContainer.GetIndexBuffer()->Get(i+0);
				const INT BI = LOD.MultiSizeIndexContainer.GetIndexBuffer()->Get(i+1);
				const INT CI = LOD.MultiSizeIndexContainer.GetIndexBuffer()->Get(i+2);
				const FFinalSkinVertex &A = DestVertex[AI];
				const FFinalSkinVertex &B = DestVertex[BI];
				const FFinalSkinVertex &C = DestVertex[CI];
				FVector Tangent;
				if(ComputeTangent(Tangent, A.Position, FVector2D(A.U, A.V), B.Position, FVector2D(B.U, B.V), C.Position, FVector2D(C.U, C.V)))
				{
					ClothTangentData(AI) += Tangent;
					ClothTangentData(BI) += Tangent;
					ClothTangentData(CI) += Tangent;
				}
				else
				{
					ClothTangentData(AI) = DestVertex[AI].TangentX;
					ClothTangentData(BI) = DestVertex[BI].TangentX;
					ClothTangentData(CI) = DestVertex[CI].TangentX;
				}
			}

			// Fixup tangent basis...
			for(INT ClothVertIdx=0; ClothVertIdx < NumVertices; ClothVertIdx++)
			{
				// Generate contiguous tangent vectors. Slower but works without artifacts.
				FVector TangentZ = DestVertex[ClothVertIdx].TangentZ;
				FVector TangentX = ClothTangentData(ClothVertIdx).SafeNormal();
				FVector TangentY = TangentZ ^ TangentX;
				TangentX = TangentY ^ TangentZ;
				DestVertex[ClothVertIdx].TangentX = TangentX;

				// store sign of determinant in TangentZ.W
				DestVertex[ClothVertIdx].TangentZ.Vector.W = GetBasisDeterminantSign(TangentX,TangentY,TangentZ) < 0 ? 0 : 255;
			}
		}
#endif //!NX_DISABLE_CLOTH

#if !NX_DISABLE_SOFTBODY
		// Update graphics verts from physics info if we are running soft-bodies.
		if(DynamicData->SoftBodyTetraPosData.Num() > 0 && LODIndex == 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateSoftBodyVertsTime);

			TArray<INT>& SurfaceToGraphicsVertMap	= SkeletalMesh->SoftBodySurfaceToGraphicsVertMap;
			TArray<INT>& SurfaceIndices				= SkeletalMesh->SoftBodySurfaceIndices;			
			TArray<FVector>& TetraPosData			= DynamicData->SoftBodyTetraPosData;
			TArray<INT>& TetraIndices				= SkeletalMesh->SoftBodyTetraIndices;			
			TArray<FSoftBodyTetraLink>& TetraLinks	= SkeletalMesh->SoftBodyTetraLinks;
			
			INT NumVertices = TetraLinks.Num();			

			// Compute the position of each soft-body surface vertex by interpolating the positions
			// of the four corner vertices of the associated simulated tetrahedron.	
			CachedSoftBodyPositions.Reset();
			CachedSoftBodyPositions.Add(NumVertices);
			for(INT i=0; i<TetraLinks.Num(); i++)
			{
				INT GraphicsIndex = SurfaceToGraphicsVertMap(i);
				check((GraphicsIndex >= 0) && (GraphicsIndex < CachedFinalVerticesNum));

				INT LinkIdx = TetraLinks(i).TetIndex;
				FVector LinkBary = TetraLinks(i).Bary;

				INT TetIdx[4];
				TetIdx[0] = TetraIndices(LinkIdx + 0);
				TetIdx[1] = TetraIndices(LinkIdx + 1);
				TetIdx[2] = TetraIndices(LinkIdx + 2);
				TetIdx[3] = TetraIndices(LinkIdx + 3);

				FVector TetPt[4];
				TetPt[0] = TetraPosData(TetIdx[0]);
				TetPt[1] = TetraPosData(TetIdx[1]);
				TetPt[2] = TetraPosData(TetIdx[2]);
				TetPt[3] = TetraPosData(TetIdx[3]);

				CachedSoftBodyPositions(i) = 
					TetPt[0] * LinkBary.X + 
					TetPt[1] * LinkBary.Y + 
					TetPt[2] * LinkBary.Z + 
					TetPt[3] * (1.0f - LinkBary.X - LinkBary.Y - LinkBary.Z);
			}

			// Run through all triangles of the surface mesh to compute (averaged) normals/tangents at each surface vertex.	
			CachedSoftBodyNormals.Reset();
			CachedSoftBodyTangents.Reset();
			CachedSoftBodyNormals.AddZeroed(NumVertices);
			CachedSoftBodyTangents.AddZeroed(NumVertices);
			for(INT i=0; i<SurfaceIndices.Num(); i+=3)
			{
				INT A  = SurfaceIndices(i+0);
				INT B  = SurfaceIndices(i+1);
				INT C  = SurfaceIndices(i+2);
				const FVector &APos = CachedSoftBodyPositions(A);
				const FVector &BPos = CachedSoftBodyPositions(B);
				const FVector &CPos = CachedSoftBodyPositions(C);
				const FVector Normal = (BPos-APos)^(CPos-APos);
				CachedSoftBodyNormals(A) += Normal;
				CachedSoftBodyNormals(B) += Normal;
				CachedSoftBodyNormals(C) += Normal;
				const FVector2D AUV(DestVertex[A].U, DestVertex[A].V);
				const FVector2D BUV(DestVertex[B].U, DestVertex[B].V);
				const FVector2D CUV(DestVertex[C].U, DestVertex[C].V);
				FVector Tangent;
				if(ComputeTangent(Tangent, APos, AUV, BPos, BUV, CPos, CUV))
				{
					CachedSoftBodyTangents(A) += Tangent;
					CachedSoftBodyTangents(B) += Tangent;
					CachedSoftBodyTangents(C) += Tangent;
				}
			}

			// Update vertex array with final positions/normals/tangents. Transform & normalize.
			for(INT i=0; i<NumVertices; i++)
			{
				INT GraphicsIndex = SurfaceToGraphicsVertMap(i);
				check((GraphicsIndex >= 0) && (GraphicsIndex < CachedFinalVerticesNum));

				// position
				DestVertex[GraphicsIndex].Position = DynamicData->WorldToLocal.TransformFVector(P2UScale * CachedSoftBodyPositions(i));

				// normal
				FVector TangentZ = DynamicData->WorldToLocal.TransformNormal(CachedSoftBodyNormals(i)).SafeNormal();
				DestVertex[GraphicsIndex].TangentZ = TangentZ;
				
				// Generate contiguous tangent vectors. Slower but works without artifacts.
				FVector TangentX = DynamicData->WorldToLocal.TransformNormal(CachedSoftBodyTangents(i).SafeNormal());
				FVector TangentY = TangentZ ^ TangentX;
				TangentX = TangentY ^ TangentZ;
				DestVertex[GraphicsIndex].TangentX = TangentX;

				// store sign of determinant in TangentZ.W
				DestVertex[GraphicsIndex].TangentZ.Vector.W = GetBasisDeterminantSign(TangentX,TangentY,TangentZ) < 0 ? 0 : 255;
			}
		}
#endif //!NX_DISABLE_SOFTBODY

		// set lod level currently cached
		CachedVertexLOD = LODIndex;

#if !NX_DISABLE_CLOTH
		// copy to the vertex buffer
		if((SkeletalMesh->bEnableClothTearing || SkeletalMesh->bEnableValidBounds) && (SkeletalMesh->ClothWeldingMap.Num() == 0))
		{
			check(LOD.NumVertices <= (UINT)CachedFinalVertices.Num());
			MeshLOD.UpdateFinalSkinVertexBuffer( &CachedFinalVertices(0), CachedFinalVerticesNum * sizeof(FFinalSkinVertex) );

			//Generate dynamic index buffer, start with existing index data, followed by cloth index data.

			//Update dynamic index buffer(TODO: refactor into another function)
			if(IsValidRef(MeshLOD.DynamicIndexBuffer.IndexBufferRHI))
			{
				const FStaticLODModel& LODModel = SkeletalMesh->LODModels(LODIndex);
				const INT LODModelIndexNum = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();

				/* Note: we assume that the index buffer is CPU accessible and there is not a large 
				 * penalty for readback. This is true due to CPU decals already
				*/

				//Manual copy due to lack of appropriate assignment operator.
				TArray<INT> RemappedIndices;

				RemappedIndices.Empty(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num());
				RemappedIndices.Add(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num());

				// save the graphical indices, to find the ones that were NOT visited
				TArray<bool> IndicesInSimulation;
				IndicesInSimulation.Empty(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num());
				IndicesInSimulation.Add(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num());

				for(INT i=0; i<RemappedIndices.Num(); i++)
				{
					RemappedIndices(i) = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(i);
					IndicesInSimulation(i) = false;
				}

				for(INT i=0; i<DynamicData->ClothIndexData.Num(); i+=3)
				{				
					INT NewTriIndices[3], OrigTriIndices[3];
					
					// NOTE:
					// In USkeletalMesh::BuildClothMapping(), the triangle index-triples taken from 
					// USkeletalMesh.LODModels(0).IndexBuffer.Indices have their winding order changed 
					// from (0,1,2) to (0,2,1) when used as input for creating the cloth-mesh. The triples 
					// used as keys to ClothTornTriMap also use the original winding order. Therefore, we 
					// have to "rewind" the triples that are returned from the PhysX SDK before using them.

					NewTriIndices[0] = DynamicData->ClothIndexData(i + 0);
					NewTriIndices[1] = DynamicData->ClothIndexData(i + 2);
					NewTriIndices[2] = DynamicData->ClothIndexData(i + 1);


					for(INT j=0; j<3; j++)
					{
						// Chase back all the indices to the original triangle using the parent data.
						OrigTriIndices[j] = NewTriIndices[j];
						while(OrigTriIndices[j] >= SkeletalMesh->ClothToGraphicsVertMap.Num())
						{
							OrigTriIndices[j] = DynamicData->ClothParentIndices(OrigTriIndices[j]);
						}
					}	


					// Lookup the graphics triangle associated in the mesh.
					// Try all 3 permutations of the indices in case PhysX rotated the triangle.
					const static INT VertexRots[3][3] = { {0,1,2}, {1,2,0}, {2,0,1} };
					INT *TriMapVal = NULL;
					for(INT j=0; j<3; j++)
					{
						QWORD PackedTri = ((QWORD)OrigTriIndices[VertexRots[j][0]]      ) +
										  ((QWORD)OrigTriIndices[VertexRots[j][1]] << 16) +
										  ((QWORD)OrigTriIndices[VertexRots[j][2]] << 32);
						TriMapVal = SkeletalMesh->ClothTornTriMap.Find(PackedTri);
						if(TriMapVal)
							break;
					}
					
					// Update graphics indices to new indices.
					if(TriMapVal)
					{		
						INT GraphicsTriOffset = *TriMapVal;

						IndicesInSimulation(GraphicsTriOffset) = true;
						
						for(INT j=0; j<3; j++)
						{	
							RemappedIndices(GraphicsTriOffset + j) = 
								ClothIdxToGraphics(NewTriIndices[j], SkeletalMesh->ClothToGraphicsVertMap, LOD.NumVertices);
						}
					}
				}
				
				if (SkeletalMesh->GraphicsIndexIsCloth.Num() > 0) // check if cloth data structures have been initialized in USkeletalMesh::BuildClothTornTriMap
				{
					for (INT i = 0; i < RemappedIndices.Num(); i = i+3)
					{
						// go through the graphics triangle indices and consider the ones that have NOT been visited, i.e. they're not simulated
						if (DynamicData->bRemoveNonSimulatedTriangles && !IndicesInSimulation(i))
						{

							DWORD Index0 = LOD.MultiSizeIndexContainer.GetIndexBuffer()->Get(i+0);
							DWORD Index1 = LOD.MultiSizeIndexContainer.GetIndexBuffer()->Get(i+1);
							DWORD Index2 = LOD.MultiSizeIndexContainer.GetIndexBuffer()->Get(i+2);

							// if the triangle isn't supposed to be simulated we don't need to change RemappedIndices, otherwise the triangle has been deleted
							if(SkeletalMesh->GraphicsIndexIsCloth(Index0) || SkeletalMesh->GraphicsIndexIsCloth(Index1) || SkeletalMesh->GraphicsIndexIsCloth(Index2) )
							{
								// setting the same index for each triangle edge prevents triangle from being rendered
								RemappedIndices(i + 0) = 0;
								RemappedIndices(i + 1) = 0;
								RemappedIndices(i + 2) = 0;
							}
						}
					}
				}

				INT TotalNumIndices  = LODModelIndexNum;
				if( MeshLOD.DynamicIndexBuffer.GetIndexBufferSize() == sizeof(DWORD) )
				{
					DWORD* Buffer = (DWORD *)RHILockIndexBuffer(MeshLOD.DynamicIndexBuffer.IndexBufferRHI,0, TotalNumIndices * sizeof(DWORD));

					//Copy static mesh indices into the dynamic index buffer.
					for(INT i=0; i<LODModelIndexNum; i++)
					{

						check(RemappedIndices(i) < 0xffffffff);
						Buffer[i] = (DWORD)RemappedIndices(i);
					}

					RHIUnlockIndexBuffer(MeshLOD.DynamicIndexBuffer.IndexBufferRHI);
				}
				else
				{
					WORD* Buffer = (WORD *)RHILockIndexBuffer(MeshLOD.DynamicIndexBuffer.IndexBufferRHI,0, TotalNumIndices * sizeof(WORD));

					//Copy static mesh indices into the dynamic index buffer.
					for(INT i=0; i<LODModelIndexNum; i++)
					{
						check(RemappedIndices(i) < 0xffff);
						Buffer[i] = (WORD)RemappedIndices(i);
					}

					RHIUnlockIndexBuffer(MeshLOD.DynamicIndexBuffer.IndexBufferRHI);
				}
			}
		}
		else
#endif		// !NX_DISABLE_CLOTH
		{
			check((INT)LOD.NumVertices <= CachedFinalVertices.Num());
			MeshLOD.UpdateFinalSkinVertexBuffer( &CachedFinalVertices(0), LOD.NumVertices * sizeof(FFinalSkinVertex) );
		}
	}
}

/**
 * @param	LODIndex - each LOD has its own vertex data
 * @param	ChunkIdx - not used
 * @return	vertex factory for rendering the LOD
 */
const FVertexFactory* FSkeletalMeshObjectCPUSkin::GetVertexFactory(INT LODIndex,INT /*ChunkIdx*/) const
{
	check( LODs.IsValidIndex(LODIndex) );
	return &LODs(LODIndex).VertexFactory;
}

/**
 * @return		Vertex factory for rendering the specified decal at the specified LOD.
 */
FDecalVertexFactoryBase* FSkeletalMeshObjectCPUSkin::GetDecalVertexFactory(INT LODIndex,INT ChunkIdx,const FDecalInteraction* Decal)
{
	check( bDecalFactoriesEnabled );
	FDecalVertexFactoryBase* DecalVertexFactory = LODs(LODIndex).GetDecalVertexFactory( Decal->Decal );
	return DecalVertexFactory;
}

/** 
 * Init rendering resources for this LOD 
 */
void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::InitResources(UBOOL bUseInstancedVertexWeights)
{
	// upload vertex buffer
	BeginInitResource(&VertexBuffer);

	// queue a call to update the vertex influence buffer
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SkelMeshObjectUpdateInfluencesCommand,
		FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD*, MeshObject, this,
		UBOOL, bUseWeights, bUseInstancedVertexWeights,
	{
		if (bUseWeights)
		{
			// LOD of the skel mesh is used to find number of vertices in buffer
			FStaticLODModel& LodModel = MeshObject->VertexInfluenceBuffer.SkelMesh->LODModels(MeshObject->VertexInfluenceBuffer.LODIdx);
			INT NumVertices = LodModel.NumVertices;

			MeshObject->VertexInfluenceBuffer.Influences.Empty(NumVertices);
			MeshObject->VertexInfluenceBuffer.Influences.AddZeroed(NumVertices);

			for (INT VertIndex=0; VertIndex < NumVertices; ++VertIndex)
			{
				const FGPUSkinVertexBase* BaseVert = LodModel.VertexBufferGPUSkin.GetVertexPtr(VertIndex);

				for( INT Idx=0; Idx < MAX_INFLUENCES; Idx++ )
				{
					MeshObject->VertexInfluenceBuffer.Influences(VertIndex).Weights.InfluenceWeights[Idx] = BaseVert->InfluenceWeights[Idx];
					MeshObject->VertexInfluenceBuffer.Influences(VertIndex).Bones.InfluenceBones[Idx] = BaseVert->InfluenceBones[Idx];
				}
			}
		}
	}
	);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		InitSkeletalMeshCPUSkinVertexFactory,
		FLocalVertexFactory*,VertexFactory,&VertexFactory,
		FVertexBuffer*,VertexBuffer,&VertexBuffer,
		{
			FLocalVertexFactory::DataType Data;

			// position
			Data.PositionComponent = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,Position),sizeof(FFinalSkinVertex),VET_Float3);
			// tangents
			Data.TangentBasisComponents[0] = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentX),sizeof(FFinalSkinVertex),VET_PackedNormal);
			Data.TangentBasisComponents[1] = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentZ),sizeof(FFinalSkinVertex),VET_PackedNormal);
			// uvs
			Data.TextureCoordinates.AddItem(FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,U),sizeof(FFinalSkinVertex),VET_Float2));

			VertexFactory->SetData(Data);
		});
	BeginInitResource(&VertexFactory);

	// Initialize resources for decals drawn at this LOD.
	for ( INT DecalIndex = 0 ; DecalIndex < Decals.Num() ; ++DecalIndex )
	{
		FDecalLOD& Decal = Decals(DecalIndex);
		Decal.InitResources_GameThread( this );
	}
	BeginInitResource(&DynamicIndexBuffer);

	bResourcesInitialized = TRUE;
}

void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::FDecalLOD::InitResources_GameThread(FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD* LODObject)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		InitSkeletalMeshCPUSkinDecalVertexFactory,
		FLocalDecalVertexFactory*,VertexFactory,&DecalVertexFactory,
		FVertexBuffer*,VertexBuffer,&LODObject->VertexBuffer,
		{
			FLocalDecalVertexFactory::DataType Data;

			// position
			Data.PositionComponent = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,Position),sizeof(FFinalSkinVertex),VET_Float3);
			// tangents
			Data.TangentBasisComponents[0] = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentX),sizeof(FFinalSkinVertex),VET_PackedNormal);
			Data.TangentBasisComponents[1] = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentZ),sizeof(FFinalSkinVertex),VET_PackedNormal);
			// uvs
			Data.TextureCoordinates.AddItem(FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,U),sizeof(FFinalSkinVertex),VET_Float2));

			VertexFactory->SetData(Data);
		});
	BeginInitResource(&DecalVertexFactory);
}

void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::FDecalLOD::InitResources_RenderingThread(FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD* LODObject)
{
	FLocalDecalVertexFactory::DataType Data;

	// position
	Data.PositionComponent = FVertexStreamComponent(
		&LODObject->VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,Position),sizeof(FFinalSkinVertex),VET_Float3);
	// tangents
	Data.TangentBasisComponents[0] = FVertexStreamComponent(
		&LODObject->VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentX),sizeof(FFinalSkinVertex),VET_PackedNormal);
	Data.TangentBasisComponents[1] = FVertexStreamComponent(
		&LODObject->VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,TangentZ),sizeof(FFinalSkinVertex),VET_PackedNormal);
	// uvs
	Data.TextureCoordinates.AddItem(FVertexStreamComponent(
		&LODObject->VertexBuffer,STRUCT_OFFSET(FFinalSkinVertex,U),sizeof(FFinalSkinVertex),VET_Float2));

	// copy it
	DecalVertexFactory.SetData(Data);
	DecalVertexFactory.InitResource();
}

/** 
 * Release rendering resources for this LOD 
 */
void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::ReleaseResources()
{	
	BeginReleaseResource(&VertexFactory);
	BeginReleaseResource(&VertexBuffer);

	BeginReleaseResource(&DynamicIndexBuffer);

	// Release resources for decals drawn at this LOD.
	for ( INT DecalIndex = 0 ; DecalIndex < Decals.Num() ; ++DecalIndex )
	{
		FDecalLOD& Decal = Decals(DecalIndex);
		Decal.ReleaseResources_GameThread();
	}


	// queue a call to empty the influence weights buffer
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		SkelMeshObjectDeleteInfluencesCommand,
		FSkeletalMeshObjectLOD*, MeshObject, this,
	{
		MeshObject->VertexInfluenceBuffer.Influences.Empty();
	}
	);

	bResourcesInitialized = FALSE;
}

/** 
 * Update the contents of the vertex buffer with new data
 * @param	NewVertices - array of new vertex data
 * @param	Size - size of new vertex data aray 
 */
void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::UpdateFinalSkinVertexBuffer(void* NewVertices, DWORD Size) const
{
	void* Buffer = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI,0,Size,FALSE);
	appMemcpy(Buffer,NewVertices,Size);
	RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
}

/** Creates resources for drawing the specified decal at this LOD. */
void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction)
{
	checkSlow( FindDecalObjectIndex(DecalInteraction.Decal) == INDEX_NONE );

	const INT DecalIndex = Decals.AddItem( DecalInteraction.Decal );
	if ( bResourcesInitialized )
	{
		FDecalLOD& Decal = Decals(DecalIndex);
		Decal.InitResources_RenderingThread( this );
	}
}

/** Releases resources for the specified decal at this LOD. */
void FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent)
{
	const INT DecalIndex = FindDecalObjectIndex( DecalComponent );
	if( Decals.IsValidIndex(DecalIndex) )
	{
		if ( bResourcesInitialized )
		{
			FDecalLOD& Decal = Decals(DecalIndex);
			Decal.ReleaseResources_RenderingThread();
		}
		Decals.Remove(DecalIndex);
	}
}

/**
 * @return		The vertex factory associated with the specified decal at this LOD.
 */
FLocalDecalVertexFactory* FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::GetDecalVertexFactory(const UDecalComponent* DecalComponent)
{
	const INT DecalIndex = FindDecalObjectIndex( DecalComponent );
	if( Decals.IsValidIndex(DecalIndex) )
	{
		FDecalLOD& Decal = Decals(DecalIndex);
		return &(Decal.DecalVertexFactory);
	}
	else
	{
		return NULL;
	}
}

/**
 * @return		The index into the decal objects list for the specified decal, or INDEX_NONE if none found.
 */
INT FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD::FindDecalObjectIndex(const UDecalComponent* DecalComponent) const
{
	for ( INT DecalIndex = 0 ; DecalIndex < Decals.Num() ; ++DecalIndex )
	{
		const FDecalLOD& Decal = Decals(DecalIndex);
		if ( Decal.DecalComponent == DecalComponent )
		{
			return DecalIndex;
		}
	}
	return INDEX_NONE;
}

/** 
 *	Get the array of component-space bone transforms. 
 *	Not safe to hold this point between frames, because it exists in dynamic data passed from main thread.
 */
TArray<FBoneAtom>* FSkeletalMeshObjectCPUSkin::GetSpaceBases() const
{
#if !FINAL_RELEASE
	if(DynamicData)
	{
		return &(DynamicData->MeshSpaceBases);
	}
	else
#endif
	{
		return NULL;
	}
}

/** 
 *	Get the array of tetrahedron vertex positions. 
 *	Not safe to hold this point between frames, because it exists in dynamic data passed from main thread.
 */
TArray<FVector>* FSkeletalMeshObjectCPUSkin::GetSoftBodyTetraPosData() const
{
#if !NX_DISABLE_SOFTBODY
	if(DynamicData)
	{
		return &(DynamicData->SoftBodyTetraPosData);
	}
	else
#endif
	{
		return NULL;
	}
}

/**
 * Transforms the decal from refpose space to local space, in preparation for application
 * to post-skinned (ie local space) verts on the GPU.
 */
void FSkeletalMeshObjectCPUSkin::TransformDecalState(const FDecalState& DecalState,
													 FMatrix& OutDecalMatrix,
													 FVector& OutDecalLocation,
													 FVector2D& OutDecalOffset, 
													 FBoneAtom& OutDecalRefToLocal)
{
	OutDecalOffset = FVector2D(DecalState.OffsetX, DecalState.OffsetY);

	if( DynamicData && DecalState.HitBoneIndex != INDEX_NONE )
	{
		if( DynamicData->ReferenceToLocal.IsValidIndex(DecalState.HitBoneIndex) )
		{
			OutDecalRefToLocal = DynamicData->ReferenceToLocal(DecalState.HitBoneIndex);
		}
		else
		{
//			debugf(NAME_Warning,TEXT("Invalid bone index specified for decal: HitBoneIndex=%d"),DecalState.HitBoneIndex);
			OutDecalRefToLocal = FBoneAtom::Identity;
		}

		OutDecalLocation = OutDecalRefToLocal.TransformFVector4( DecalState.HitLocation );
		const FVector LocTangent = OutDecalRefToLocal.TransformNormal( DecalState.HitTangent );
		const FVector LocBinormal = OutDecalRefToLocal.TransformNormal( DecalState.HitBinormal );
		const FVector LocNormal = OutDecalRefToLocal.TransformNormal( DecalState.HitNormal );
		OutDecalMatrix = FMatrix( 
			/*TileX**/LocTangent/DecalState.Width,
			/*TileY**/LocBinormal/DecalState.Height,
			LocNormal,
			FVector(0.f,0.f,0.f) 
			).Transpose();
	}
	else
	{
		OutDecalMatrix = DecalState.WorldTexCoordMtx;
		OutDecalLocation = DecalState.HitLocation;
		OutDecalOffset = FVector2D(DecalState.OffsetX, DecalState.OffsetY);
		OutDecalRefToLocal = FBoneAtom::Identity; // default
	}
}

/**
 * Get the origin and direction vectors for TRISORT_CustomLeftRight sections
 */
const FTwoVectors& FSkeletalMeshObjectCPUSkin::GetCustomLeftRightVectors(INT SectionIndex) const
{
	if( DynamicData && DynamicData->CustomLeftRightVectors.IsValidIndex(SectionIndex) )
	{
		return DynamicData->CustomLeftRightVectors(SectionIndex);
	}
	else
	{
		static FTwoVectors Bad( FVector(0.f,0.f,0.f), FVector(1.f,0.f,0.f) );
		return Bad;
	}
}

/*-----------------------------------------------------------------------------
FDynamicSkelMeshObjectDataCPUSkin
-----------------------------------------------------------------------------*/

/**
* Constructor - Called on Game Thread
* Updates the ReferenceToLocal matrices using the new dynamic data.
* @param	InSkelMeshComponent - parent skel mesh component
* @param	InLODIndex - each lod has its own bone map 
* @param	InActiveMorphs - morph targets to blend with during skinning
* @param	DecalRequiredBoneIndices - any bones needed to render decals
*/
FDynamicSkelMeshObjectDataCPUSkin::FDynamicSkelMeshObjectDataCPUSkin(
	USkeletalMeshComponent* InSkelMeshComponent,
	INT InLODIndex,
	const TArray<FActiveMorph>& InActiveMorphs,
	const TArray<WORD>* DecalRequiredBoneIndices
	)
:	LODIndex(InLODIndex)
,	ActiveMorphs(InActiveMorphs)
#if !NX_DISABLE_CLOTH
,	ClothBlendWeight(1.0)
,	bRemoveNonSimulatedTriangles(TRUE)
#endif
{
	UpdateRefToLocalMatrices( ReferenceToLocal, InSkelMeshComponent, LODIndex, DecalRequiredBoneIndices );

	UpdateCustomLeftRightVectors( CustomLeftRightVectors, InSkelMeshComponent, LODIndex );

#if !FINAL_RELEASE
	MeshSpaceBases = InSkelMeshComponent->SpaceBases;
#endif

#if !NX_DISABLE_CLOTH
	WorldToLocal = InSkelMeshComponent->LocalToWorld.Inverse();

	// Copy cloth vertex information to rendering thread.
	UBOOL bTearing = InSkelMeshComponent->SkeletalMesh && InSkelMeshComponent->SkeletalMesh->bEnableClothTearing;
	if(InSkelMeshComponent->ClothSim && (!InSkelMeshComponent->bClothFrozen || bTearing) && InSkelMeshComponent->ClothMeshPosData.Num() > 0 && LODIndex == 0)
	{
		if (InSkelMeshComponent->SkeletalMesh->ClothWeldingMap.Num() > 0)
		{
			// Last step of Welding
			check(InSkelMeshComponent->ClothMeshPosData.Num() == InSkelMeshComponent->ClothMeshNormalData.Num());
			check(InSkelMeshComponent->ClothMeshPosData.Num() == InSkelMeshComponent->SkeletalMesh->ClothWeldingMap.Num());

			TArray<INT>& weldingMap = InSkelMeshComponent->SkeletalMesh->ClothWeldingMap;
			for (INT i = 0; i < InSkelMeshComponent->ClothMeshPosData.Num(); i++)
			{
				INT welded = weldingMap(i);
				InSkelMeshComponent->ClothMeshPosData(i) = InSkelMeshComponent->ClothMeshWeldedPosData(welded);
				InSkelMeshComponent->ClothMeshNormalData(i) = InSkelMeshComponent->ClothMeshWeldedNormalData(welded);
			}
		}
		INT NumClothVerts = InSkelMeshComponent->ClothMeshPosData.Num();

		// TODO: ClothPosData etc. arrays are NULL every time this code is executed, they must be emptied somewhere
		// This causes an alloc each frame for each cloth
		ClothPosData = InSkelMeshComponent->ClothMeshPosData;
		ClothNormalData = InSkelMeshComponent->ClothMeshNormalData;

		ClothBlendWeight = bTearing ? InSkelMeshComponent->ClothBlendWeight : InSkelMeshComponent->ClothDynamicBlendWeight;

		ClothIndexData = InSkelMeshComponent->ClothMeshIndexData;
		
		ActualClothPosDataNum = InSkelMeshComponent->NumClothMeshVerts;
		ClothParentIndices = InSkelMeshComponent->ClothMeshParentData;

	}
	else
	{
		ActualClothPosDataNum = 0;
	}

#endif


#if !NX_DISABLE_SOFTBODY
		
	if(InSkelMeshComponent->SoftBodyTetraPosData.Num() > 0 && LODIndex == 0)
	{
		SoftBodyTetraPosData = InSkelMeshComponent->SoftBodyTetraPosData;
	}

#endif //!NX_DISABLE_SOFTBODY

}

/*-----------------------------------------------------------------------------
	FSkeletalMeshObjectCPUSkin - morph target blending implementation
-----------------------------------------------------------------------------*/

/**
 * Since the vertices in the active morphs are sorted based on the index of the base mesh vert
 * that they affect we keep track of the next valid morph vertex to apply
 *
 * @param	OutMorphVertIndices		[out] Llist of vertex indices that need a morph target blend
 * @return							number of active morphs that are valid
 */
UINT GetMorphVertexIndices(const TArray<FActiveMorph>& ActiveMorphs, INT LODIndex, TArray<INT>& OutMorphVertIndices)
{
	UINT NumValidMorphs=0;

	for( INT MorphIdx=0; MorphIdx < ActiveMorphs.Num(); MorphIdx++ )
	{
		const FActiveMorph& ActiveMorph = ActiveMorphs(MorphIdx);
		if( ActiveMorph.Target &&
			ActiveMorph.Weight >= MinMorphBlendWeight && 
			ActiveMorph.Weight <= MaxMorphBlendWeight &&				
			ActiveMorph.Target->MorphLODModels.IsValidIndex(LODIndex) &&
			ActiveMorph.Target->MorphLODModels(LODIndex).Vertices.Num() )
		{
			// start at the first vertex since they affect base mesh verts in ascending order
			OutMorphVertIndices.AddItem(0);
			NumValidMorphs++;
		}
		else
		{
			// invalidate the indices for any invalid morph models
			OutMorphVertIndices.AddItem(INDEX_NONE);
		}			
	}
	return NumValidMorphs;
}


/*-----------------------------------------------------------------------------
	FSkeletalMeshObjectCPUSkin - optimized skinning code
-----------------------------------------------------------------------------*/

#pragma warning(push)
#pragma warning(disable : 4730) //mixing _m64 and floating point expressions may result in incorrect code


const VectorRegister		VECTOR_PACK_127_5		= { 127.5f, 127.5f, 127.5f, 0.f };
const VectorRegister		VECTOR4_PACK_127_5		= { 127.5f, 127.5f, 127.5f, 127.5f };

const VectorRegister		VECTOR_INV_127_5		= { 1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f, 0.f };
const VectorRegister		VECTOR4_INV_127_5		= { 1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f, 1.f / 127.5f };

const VectorRegister		VECTOR_UNPACK_MINUS_1	= { -1.f, -1.f, -1.f, 0.f };
const VectorRegister		VECTOR4_UNPACK_MINUS_1	= { -1.f, -1.f, -1.f, -1.f };

const VectorRegister		VECTOR_0001				= { 0.f, 0.f, 0.f, 1.f };



#if DEBUG_CPU_SKINNING
template<typename VertexType>
void SkinVertices( FFinalSkinVertex* DestVertex, FBoneAtom* ReferenceToLocal, INT LODIndex, FStaticLODModel& LOD, TArray<FActiveMorph>& ActiveMorphs, UBOOL bUseBoneInfluences, const TArray<FVertexInfluence>& VertexInfluences, UBOOL bFullSwap )
#else
template<typename VertexType>
FORCEINLINE void SkinVertices( FFinalSkinVertex* DestVertex, FBoneAtom* ReferenceToLocal, INT LODIndex, FStaticLODModel& LOD, TArray<FActiveMorph>& ActiveMorphs, UBOOL bUseBoneInfluences, const TArray<FVertexInfluence>& VertexInfluences, UBOOL bFullSwap )
#endif
{
	DWORD StatusRegister = VectorGetControlRegister();
	VectorSetControlRegister( StatusRegister | VECTOR_ROUND_TOWARD_ZERO );

	TArray<INT> MorphVertIndices;
	UINT NumValidMorphs = GetMorphVertexIndices(ActiveMorphs,LODIndex,MorphVertIndices);

	// Prefetch all matrices
	for ( INT MatrixIndex=0; MatrixIndex < MAX_GPUSKIN_BONES; MatrixIndex+=2 )
	{
		PREFETCH( ReferenceToLocal + MatrixIndex );
	}

	INT CurBaseVertIdx = 0;
	// VertexCopy for morph. Need to allocate right struct
	// To avoid re-allocation, create 2 statics, and assign right struct
	VertexType  VertexCopy;

	const INT RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();
	INT VertexBufferBaseIndex=0;

	for(INT SectionIndex= 0;SectionIndex< LOD.Sections.Num();SectionIndex++)
	{
		FSkelMeshSection& Section = LOD.Sections(SectionIndex);
		FSkelMeshChunk& Chunk = LOD.Chunks(Section.ChunkIndex);

		// TODO: optimization? too many branching here
		if (bFullSwap)
		{
			if (SectionIndex > 0)
			{
				// if this material index is same as previous and if full swap
				// we need to accumulate # of vert count from previous chunk
				FSkelMeshSection& PrevSection = LOD.Sections(SectionIndex-1);
				if (PrevSection.MaterialIndex == Section.MaterialIndex)
				{
					VertexBufferBaseIndex += LOD.Chunks(PrevSection.ChunkIndex).GetNumVertices();
				}
				else
				{
					VertexBufferBaseIndex = 0;
				}
			}
		}

		// Prefetch all bone indices
		WORD* BoneMap = Chunk.BoneMap.GetTypedData();
		PREFETCH( BoneMap );
		PREFETCH( BoneMap + 64 );
		PREFETCH( BoneMap + 128 );
		PREFETCH( BoneMap + 192 );

		INC_DWORD_STAT_BY(STAT_CPUSkinVertices,Chunk.GetNumRigidVertices());
		INC_DWORD_STAT_BY(STAT_CPUSkinVertices,Chunk.GetNumSoftVertices());

		VertexType* SrcRigidVertex = NULL;
		if (Chunk.GetNumRigidVertices() > 0)
		{
			// Prefetch first vertex
			PREFETCH( LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()) );
		}

		for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumRigidVertices();VertexIndex++,DestVertex++)
		{
			INT VertexBufferIndex = Chunk.GetRigidVertexBufferIndex() + VertexIndex;
			SrcRigidVertex = (VertexType*)LOD.VertexBufferGPUSkin.GetVertexPtr(VertexBufferIndex);
			PREFETCH_NEXT_CACHE_LINE( SrcRigidVertex );	// Prefetch next vertices
			VertexType* MorphedVertex = SrcRigidVertex;
			if( NumValidMorphs ) 
			{
				MorphedVertex = &VertexCopy;
				UpdateMorphedVertex<VertexType>( &LOD.VertexBufferGPUSkin, ActiveMorphs, *MorphedVertex, *SrcRigidVertex, CurBaseVertIdx, LODIndex, MorphVertIndices );
			}

			VectorRegister SrcNormals[3];
			VectorRegister DstNormals[3];
			const FVector VertexPosition = LOD.VertexBufferGPUSkin.GetVertexPosition(MorphedVertex);
			SrcNormals[0] = VectorLoadFloat3_W1( &VertexPosition );
			SrcNormals[1] = Unpack3( &MorphedVertex->TangentX.Vector.Packed );
			SrcNormals[2] = Unpack4( &MorphedVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Unpack().
			
			BYTE BoneIndex;
			if (bUseBoneInfluences)
			{
				BoneIndex = VertexInfluences(VertexBufferIndex).Bones.InfluenceBones[RigidInfluenceIndex];
			}
			else
			{
				BoneIndex =	MorphedVertex->InfluenceBones[RigidInfluenceIndex];
			}

			const FMatrix BoneMatrix = ReferenceToLocal[BoneMap[BoneIndex]].ToMatrix();
			VectorRegister M00	= VectorLoadAligned( &BoneMatrix.M[0][0] );
			VectorRegister M10	= VectorLoadAligned( &BoneMatrix.M[1][0] );
			VectorRegister M20	= VectorLoadAligned( &BoneMatrix.M[2][0] );
			VectorRegister M30	= VectorLoadAligned( &BoneMatrix.M[3][0] );

			VectorRegister N_xxxx = VectorReplicate( SrcNormals[0], 0 );
			VectorRegister N_yyyy = VectorReplicate( SrcNormals[0], 1 );
			VectorRegister N_zzzz = VectorReplicate( SrcNormals[0], 2 );
			DstNormals[0] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiplyAdd( N_zzzz, M20, M30 ) ) );

			N_xxxx = VectorReplicate( SrcNormals[1], 0 );
			N_yyyy = VectorReplicate( SrcNormals[1], 1 );
			N_zzzz = VectorReplicate( SrcNormals[1], 2 );
			DstNormals[1] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) );

			N_xxxx = VectorReplicate( SrcNormals[2], 0 );
			N_yyyy = VectorReplicate( SrcNormals[2], 1 );
			N_zzzz = VectorReplicate( SrcNormals[2], 2 );
			DstNormals[2] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) );

			// carry over the W component (sign of basis determinant) 
			DstNormals[2] = VectorMultiplyAdd( VECTOR_0001, SrcNormals[2], DstNormals[2] );

			// Write to 16-byte aligned memory:
			VectorStore( DstNormals[0], &DestVertex->Position );
			Pack3( DstNormals[1], &DestVertex->TangentX.Vector.Packed );
			Pack4( DstNormals[2], &DestVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Pack().
		
			// Copy UVs.
			FVector2D UVs = LOD.VertexBufferGPUSkin.GetVertexUV(VertexBufferIndex,0);
			DestVertex->U = UVs.X;
			DestVertex->V = UVs.Y;

			CurBaseVertIdx++;
		}

		VertexType* SrcSoftVertex = NULL;
		if (Chunk.GetNumSoftVertices() > 0)
		{
			// Prefetch first vertex
			PREFETCH( LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetSoftVertexBufferIndex()) );
		}
		for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumSoftVertices();VertexIndex++,DestVertex++)
		{
			const INT VertexBufferIndex = Chunk.GetSoftVertexBufferIndex() + VertexIndex;
			SrcSoftVertex = (VertexType*)LOD.VertexBufferGPUSkin.GetVertexPtr(VertexBufferIndex);
			PREFETCH_NEXT_CACHE_LINE( SrcSoftVertex );	// Prefetch next vertices
			VertexType* MorphedVertex = SrcSoftVertex;
			if( NumValidMorphs ) 
			{
				MorphedVertex = &VertexCopy;
				UpdateMorphedVertex<VertexType>( &LOD.VertexBufferGPUSkin, ActiveMorphs, *MorphedVertex, *SrcSoftVertex, CurBaseVertIdx, LODIndex, MorphVertIndices );
			}

			const BYTE* RESTRICT BoneIndices;
			const BYTE* RESTRICT BoneWeights;
			if (bUseBoneInfluences)
			{
				BoneIndices = VertexInfluences(VertexBufferIndex).Bones.InfluenceBones;
				BoneWeights = VertexInfluences(VertexBufferIndex).Weights.InfluenceWeights;
			}
			else
			{
				BoneIndices = MorphedVertex->InfluenceBones;
				BoneWeights = MorphedVertex->InfluenceWeights;
			}

			static VectorRegister	SrcNormals[3];
			VectorRegister			DstNormals[3];
			const FVector VertexPosition = LOD.VertexBufferGPUSkin.GetVertexPosition(MorphedVertex);
			SrcNormals[0] = VectorLoadFloat3_W1( &VertexPosition );
			SrcNormals[1] = Unpack3( &MorphedVertex->TangentX.Vector.Packed );
			SrcNormals[2] = Unpack4( &MorphedVertex->TangentZ.Vector.Packed );
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
			SrcNormals[0] = VectorSet_W1(VectorMultiply(SrcNormals[0], BlendScale));

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
			DstNormals[0] = VectorMultiplyAdd(VECTOR_2222, VectorCross(BlendDQ0YZW, VectorMultiplyAdd(BlendDQ0XXXX, SrcNormals[0], VectorCross(BlendDQ0YZW, SrcNormals[0]))), SrcNormals[0]);
			VectorRegister Trans = VectorMultiplyAdd(BlendDQ0XXXX, BlendDQ1YZW, VectorAdd(VectorNegate(VectorMultiply(BlendDQ1XXXX, BlendDQ0YZW)), VectorCross(BlendDQ0YZW, BlendDQ1YZW)));
			DstNormals[0] = VectorMultiplyAdd(VECTOR_2222, Trans, DstNormals[0]);
		
			DstNormals[1] = VectorMultiplyAdd(VECTOR_2222, VectorCross(BlendDQ0YZW, VectorMultiplyAdd(BlendDQ0XXXX, SrcNormals[1], VectorCross(BlendDQ0YZW, SrcNormals[1]))), SrcNormals[1]);
			DstNormals[2] = VectorMultiplyAdd(VECTOR_2222, VectorCross(BlendDQ0YZW, VectorMultiplyAdd(BlendDQ0XXXX, SrcNormals[2], VectorCross(BlendDQ0YZW, SrcNormals[2]))), SrcNormals[2]);
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

			VectorRegister N_xxxx = VectorReplicate( SrcNormals[0], 0 );
			VectorRegister N_yyyy = VectorReplicate( SrcNormals[0], 1 );
			VectorRegister N_zzzz = VectorReplicate( SrcNormals[0], 2 );
			DstNormals[0] = VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiplyAdd( N_zzzz, M20, M30 ) ) );

			DstNormals[1] = VectorZero();
			N_xxxx = VectorReplicate( SrcNormals[1], 0 );
			N_yyyy = VectorReplicate( SrcNormals[1], 1 );
			N_zzzz = VectorReplicate( SrcNormals[1], 2 );
			DstNormals[1] = VectorNormalize(VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) ));

			N_xxxx = VectorReplicate( SrcNormals[2], 0 );
			N_yyyy = VectorReplicate( SrcNormals[2], 1 );
			N_zzzz = VectorReplicate( SrcNormals[2], 2 );
			DstNormals[2] = VectorZero();
			DstNormals[2] = VectorNormalize(VectorMultiplyAdd( N_xxxx, M00, VectorMultiplyAdd( N_yyyy, M10, VectorMultiply( N_zzzz, M20 ) ) ));


			// carry over the W component (sign of basis determinant) 
			DstNormals[2] = VectorMultiplyAdd( VECTOR_0001, SrcNormals[2], DstNormals[2] );
#endif

			// Write to 16-byte aligned memory:
			VectorStore( DstNormals[0], &DestVertex->Position );
			Pack3( DstNormals[1], &DestVertex->TangentX.Vector.Packed );
			Pack4( DstNormals[2], &DestVertex->TangentZ.Vector.Packed );
			VectorResetFloatRegisters(); // Need to call this to be able to use regular floating point registers again after Pack().
				
			// Copy UVs.
			FVector2D UVs = LOD.VertexBufferGPUSkin.GetVertexUV(Chunk.GetSoftVertexBufferIndex()+VertexIndex,0);
			DestVertex->U = UVs.X;
			DestVertex->V = UVs.Y;

			CurBaseVertIdx++;
		}
	}

	VectorSetControlRegister( StatusRegister );
}

/**
 * Convert FPackedNormal to 0-1 FVector4
 */
FVector4 GetTangetToColor(FPackedNormal Tangent)
{
	VectorRegister VectorToUnpack = Tangent.GetVectorRegister();
	// Write to FVector and return it.
	FVector4 UnpackedVector;
	VectorStore( VectorToUnpack, &UnpackedVector );

	FVector4 Src = UnpackedVector;
	Src = Src + FVector4(1.f, 1.f, 1.f, 1.f);
	Src = Src/2.f;
	return Src;
}
/** 
 * Modify the vertex buffer to store bone weights in the UV coordinates for rendering 
 * @param DestVertex - already filled out vertex buffer from SkinVertices
 * @param LOD - LOD model corresponding to DestVertex 
 * @param BonesOfInterest - array of bones we want to display
 */
void CalculateBoneWeights(FFinalSkinVertex* DestVertex, FStaticLODModel& LOD, const TArray<INT>& BonesOfInterest, SkinColorRenderMode ColorMode, UBOOL bUseBoneInfluences, const TArray<FVertexInfluence>& VertexInfluences, UBOOL bFullSwap)
{
	const FLOAT INV255 = 1.f/255.f;

	const INT RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();

	FVector4 TangentColor;
	FGPUSkinVertexBase* SrcRigidVertex = NULL;
	FGPUSkinVertexBase* SrcSoftVertex = NULL;

	INT VertexBufferBaseIndex = 0;

	if ( ColorMode == ESCRM_BoneWeights )
	{
		for(INT SectionIndex= 0;SectionIndex< LOD.Sections.Num();SectionIndex++)
		{
			FSkelMeshSection& Section = LOD.Sections(SectionIndex);
			FSkelMeshChunk& Chunk = LOD.Chunks(Section.ChunkIndex);

			if (bFullSwap)
			{
				if (SectionIndex > 0)
				{
					FSkelMeshSection& PrevSection = LOD.Sections(SectionIndex-1);
					if (PrevSection.MaterialIndex == Section.MaterialIndex)
					{
						VertexBufferBaseIndex += LOD.Chunks(PrevSection.ChunkIndex).GetNumVertices();
					}
					else
					{
						VertexBufferBaseIndex = 0;
					}
				}
			}

			//array of bone mapping
			WORD* BoneMap = Chunk.BoneMap.GetTypedData();

			for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumRigidVertices();VertexIndex++,DestVertex++)
			{
				INT VertexBufferIndex = Chunk.GetRigidVertexBufferIndex() + VertexIndex;
				SrcRigidVertex = LOD.VertexBufferGPUSkin.GetVertexPtr(VertexBufferIndex);

				BYTE BoneIndex;
				if (bUseBoneInfluences)
				{
					BoneIndex = VertexInfluences(VertexBufferIndex).Bones.InfluenceBones[RigidInfluenceIndex];
				}
				else
				{
					BoneIndex =	SrcRigidVertex->InfluenceBones[RigidInfluenceIndex];
				}

				if (BonesOfInterest.ContainsItem(BoneMap[BoneIndex]))
				{
					DestVertex->U = 1.f; 
					DestVertex->V = 1.f; 
				}
				else
				{
					DestVertex->U = 0.0f;
					DestVertex->V = 0.0f;
				}
			}

			for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumSoftVertices();VertexIndex++,DestVertex++)
			{
				const INT VertexBufferIndex = Chunk.GetSoftVertexBufferIndex() + VertexIndex;
				SrcSoftVertex = LOD.VertexBufferGPUSkin.GetVertexPtr(VertexBufferIndex);

				//Zero out the UV coords
				DestVertex->U = 0.0f;
				DestVertex->V = 0.0f;

				const BYTE* RESTRICT BoneIndices;
				const BYTE* RESTRICT BoneWeights;
				if (bUseBoneInfluences)
				{
					BoneIndices = VertexInfluences(VertexBufferIndex).Bones.InfluenceBones;
					BoneWeights = VertexInfluences(VertexBufferIndex).Weights.InfluenceWeights;
				}
				else
				{
					BoneIndices = SrcSoftVertex->InfluenceBones;
					BoneWeights = SrcSoftVertex->InfluenceWeights;
				}

				for (INT i=0; i<MAX_INFLUENCES; i++)
				{
					if (BonesOfInterest.ContainsItem(BoneMap[BoneIndices[i]]))
					{
						DestVertex->U += BoneWeights[i] * INV255; 
						DestVertex->V += BoneWeights[i] * INV255;
					}
				}
			}
		}
	}	
	else if ( ColorMode == ESCRM_VertexTangent )
	{
		for(INT SectionIndex= 0;SectionIndex< LOD.Sections.Num();SectionIndex++)
		{
			FSkelMeshSection& Section = LOD.Sections(SectionIndex);
			FSkelMeshChunk& Chunk = LOD.Chunks(Section.ChunkIndex);

			if (bFullSwap)
			{
				if (SectionIndex > 0)
				{
					FSkelMeshSection& PrevSection = LOD.Sections(SectionIndex-1);
					if (PrevSection.MaterialIndex == Section.MaterialIndex)
					{
						VertexBufferBaseIndex += LOD.Chunks(PrevSection.ChunkIndex).GetNumVertices();
					}
					else
					{
						VertexBufferBaseIndex = 0;
					}
				}
			}

			//array of bone mapping
			WORD* BoneMap = Chunk.BoneMap.GetTypedData();

			for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumRigidVertices();VertexIndex++,DestVertex++)
			{
				SrcRigidVertex = LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()+VertexIndex);

				TangentColor = GetTangetToColor(DestVertex->TangentX);
				DestVertex->U = TangentColor.X;
				DestVertex->V = TangentColor.Y;
			}
			for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumSoftVertices();VertexIndex++,DestVertex++)
			{
				SrcSoftVertex = LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetSoftVertexBufferIndex()+VertexIndex);

				TangentColor = GetTangetToColor(DestVertex->TangentX);
				DestVertex->U = TangentColor.X;
				DestVertex->V = TangentColor.Y;
			}
		}
	}
	else if ( ColorMode == ESCRM_VertexNormal )
	{
		for(INT SectionIndex= 0;SectionIndex< LOD.Sections.Num();SectionIndex++)
		{
			FSkelMeshSection& Section = LOD.Sections(SectionIndex);
			FSkelMeshChunk& Chunk = LOD.Chunks(Section.ChunkIndex);

			if (SectionIndex > 0)
			{
				FSkelMeshSection& PrevSection = LOD.Sections(SectionIndex-1);
				if (PrevSection.MaterialIndex == Section.MaterialIndex)
				{
					VertexBufferBaseIndex += LOD.Chunks(PrevSection.ChunkIndex).GetNumVertices();
				}
				else
				{
					VertexBufferBaseIndex = 0;
				}
			}

			//array of bone mapping
			WORD* BoneMap = Chunk.BoneMap.GetTypedData();

			for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumRigidVertices();VertexIndex++,DestVertex++)
			{
				SrcRigidVertex = LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()+VertexIndex);

				TangentColor = GetTangetToColor(DestVertex->TangentZ);
				DestVertex->U = TangentColor.Z;
				DestVertex->V = TangentColor.Z;
			}

			for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumSoftVertices();VertexIndex++,DestVertex++)
			{
				SrcSoftVertex = LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetSoftVertexBufferIndex()+VertexIndex);

				TangentColor = GetTangetToColor(DestVertex->TangentZ);
				DestVertex->U = TangentColor.Z;
				DestVertex->V = TangentColor.Z;
			}
		}
	}
	else if ( ColorMode == ESCRM_VertexMirror )
	{
		for(INT SectionIndex= 0;SectionIndex< LOD.Sections.Num();SectionIndex++)
		{
			FSkelMeshSection& Section = LOD.Sections(SectionIndex);
			FSkelMeshChunk& Chunk = LOD.Chunks(Section.ChunkIndex);

			if (bFullSwap)
			{
				if (SectionIndex > 0)
				{
					FSkelMeshSection& PrevSection = LOD.Sections(SectionIndex-1);
					if (PrevSection.MaterialIndex == Section.MaterialIndex)
					{
						VertexBufferBaseIndex += LOD.Chunks(PrevSection.ChunkIndex).GetNumVertices();
					}
					else
					{
						VertexBufferBaseIndex = 0;
					}
				}
			}

			//array of bone mapping
			WORD* BoneMap = Chunk.BoneMap.GetTypedData();

			for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumRigidVertices();VertexIndex++,DestVertex++)
			{
				SrcRigidVertex = LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()+VertexIndex);

				TangentColor = GetTangetToColor(DestVertex->TangentZ);
				DestVertex->U = TangentColor.W;
				DestVertex->V = TangentColor.W;
			}

			for(INT VertexIndex = VertexBufferBaseIndex;VertexIndex < Chunk.GetNumSoftVertices();VertexIndex++,DestVertex++)
			{
				SrcSoftVertex = LOD.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetSoftVertexBufferIndex()+VertexIndex);

				TangentColor = GetTangetToColor(DestVertex->TangentZ);
				DestVertex->U = TangentColor.W;
				DestVertex->V = TangentColor.W;
			}
		}
	}
}

#pragma warning(pop)


