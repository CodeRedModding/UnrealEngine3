/*=============================================================================
	UnSkeletalRenderGPUSkin.cpp: GPU skinned skeletal mesh rendering code.
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
#include "GPUSkinVertexFactory.h"
#include "UnSkeletalRenderGPUSkin.h"
#include "UnSkeletalRenderCPUSkin.h"
#include "UnDecalRenderData.h" 

#if XBOX
#include "UnSkeletalRenderGPUXe.h"
#endif

// 0/1
#define UPDATE_PER_BONE_DATA_ONLY_FOR_OBJECT_BEEN_VISIBLE 1

enum EMorphStats
{
	STAT_MorphVertexBuffer_Update = 	STAT_MorphFirstStat,
	STAT_MorphVertexBuffer_Init,
	STAT_MorphVertexBuffer_ApplyDelta,
};

DECLARE_STATS_GROUP(TEXT("Morph"),STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Update"),STAT_MorphVertexBuffer_Update,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Init"),STAT_MorphVertexBuffer_Init,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Apply Delta"),STAT_MorphVertexBuffer_ApplyDelta,STATGROUP_MorphTarget);


/*-----------------------------------------------------------------------------
FMorphVertexBuffer
-----------------------------------------------------------------------------*/

/** 
* Initialize the dynamic RHI for this rendering resource 
*/
void FMorphVertexBuffer::InitDynamicRHI()
{
	// LOD of the skel mesh is used to find number of vertices in buffer
	FStaticLODModel& LodModel = SkelMesh->LODModels(LODIdx);

	// Create the buffer rendering resource
	UINT Size = LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);
	VertexBufferRHI = RHICreateVertexBuffer(Size,NULL,RUF_Volatile);

	// Lock the buffer.
	FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*) RHILockVertexBuffer(VertexBufferRHI,0,Size,FALSE);

	// zero all deltas (NOTE: DeltaTangentZ is FPackedNormal, so we can't just appMemzero)
	for (UINT VertIndex=0; VertIndex < LodModel.NumVertices; ++VertIndex)
	{
		Buffer[VertIndex].DeltaPosition = FVector(0,0,0);
		Buffer[VertIndex].DeltaTangentZ = FVector(0,0,0);
	}

	// Unlock the buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);
	
	// hasn't been updated yet
	HasBeenUpdated = FALSE;
}

/** 
* Release the dynamic RHI for this rendering resource 
*/
void FMorphVertexBuffer::ReleaseDynamicRHI()
{
	VertexBufferRHI.SafeRelease();
}

/*-----------------------------------------------------------------------------
FInfluenceWeightsVertexBuffer
-----------------------------------------------------------------------------*/

void ResetInfluences (FVertexInfluence* Buffer, const FSkeletalMeshVertexBuffer& InDefaultGPUSkin, UINT NumVertices)
{
	checkAtCompileTime( MAX_INFLUENCES == 4, MaxInfluencesAssumedToBe4 );

	const INT SrcStride = InDefaultGPUSkin.GetStride();
	const INT DestStride = sizeof(FVertexInfluence);
	const FGPUSkinVertexBase * RESTRICT Src = InDefaultGPUSkin.GetVertexPtr(0);
	FVertexInfluence * RESTRICT Dest = Buffer;

	// zero all deltas (NOTE: DeltaTangentZ is FPackedNormal, so we can't just appMemzero)
	for (UINT VertIndex=0; VertIndex < NumVertices; ++VertIndex)
	{
		CONSOLE_PREFETCH_NEXT_CACHE_LINE( Src );
		CONSOLE_PREFETCH_NEXT_CACHE_LINE( Dest );
		Dest->Weights.InfluenceWeightsDWORD = *((DWORD*)Src->InfluenceWeights);
		Dest->Bones.InfluenceBonesDWORD = *((DWORD*)Src->InfluenceBones);
		Src = (FGPUSkinVertexBase*)((BYTE*)Src + SrcStride);
		Dest = (FVertexInfluence*)((BYTE*)Dest + DestStride);
	}
}

/** 
* Initialize the dynamic RHI for this rendering resource 
*/
void FInfluenceWeightsVertexBuffer::InitDynamicRHI()
{
	SCOPE_CYCLE_COUNTER(STAT_InfluenceWeightsUpdateRTTime);
	// LOD of the skel mesh is used to find number of vertices in buffer
	FStaticLODModel& LodModel = SkelMesh->LODModels(LODIdx);

	// Create the buffer rendering resource
	UINT Size = LodModel.NumVertices * sizeof(FVertexInfluence);
	VertexBufferRHI = RHICreateVertexBuffer(Size,NULL,RUF_Volatile);

	// just weights copy from the base skelmesh vertex buffer for defaults
	// Lock the buffer.
	FVertexInfluence* Buffer = (FVertexInfluence*) RHILockVertexBuffer(VertexBufferRHI,0,Size,FALSE);

	ResetInfluences(Buffer, LodModel.VertexBufferGPUSkin, LodModel.NumVertices);

	// Unlock the buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

/** 
* Release the dynamic RHI for this rendering resource 
*/
void FInfluenceWeightsVertexBuffer::ReleaseDynamicRHI()
{
	VertexBufferRHI.SafeRelease();
}

/*-----------------------------------------------------------------------------
FSkeletalMeshObjectGPUSkin
-----------------------------------------------------------------------------*/

/** 
 * Constructor 
 * @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render 
 */
FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectGPUSkin(USkeletalMeshComponent* InSkeletalMeshComponent) 
:	FSkeletalMeshObject(InSkeletalMeshComponent)
,	DynamicData(NULL)
,	bMorphResourcesInitialized(FALSE)
,	bInfluenceWeightsInitialized(FALSE)
{
	// create LODs to match the base mesh
	LODs.Empty(SkeletalMesh->LODModels.Num());
	for( INT LODIndex=0;LODIndex < SkeletalMesh->LODModels.Num();LODIndex++ )
	{
		new(LODs) FSkeletalMeshObjectLOD(SkeletalMesh,LODIndex,bDecalFactoriesEnabled);
	}

	InitResources();
}

/** 
 * Destructor 
 */
FSkeletalMeshObjectGPUSkin::~FSkeletalMeshObjectGPUSkin()
{
	delete DynamicData;
}

/** 
 * Initialize rendering resources for each LOD. 
 */
void FSkeletalMeshObjectGPUSkin::InitResources()
{
	for( INT LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs(LODIndex);
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(LODIndex);
		SkelLOD.InitResources(FALSE,MeshLODInfo, bUsePerBoneMotionBlur);
	}
}

/** 
 * Release rendering resources for each LOD.
 */
void FSkeletalMeshObjectGPUSkin::ReleaseResources()
{
	for( INT LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs(LODIndex);
		SkelLOD.ReleaseResources();
	}
	// also release morph resources
	ReleaseMorphResources();
}

/** 
* Initialize morph rendering resources for each LOD 
*/
void FSkeletalMeshObjectGPUSkin::InitMorphResources(UBOOL bInUsePerBoneMotionBlur)
{
	if( bMorphResourcesInitialized )
	{
		// release first if already initialized
		ReleaseMorphResources();
	}

	for( INT LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs(LODIndex);
		// init any morph vertex buffers for each LOD
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(LODIndex);
		SkelLOD.InitMorphResources(MeshLODInfo,bInUsePerBoneMotionBlur);
	}
	bMorphResourcesInitialized = TRUE;
}

/** 
* Release morph rendering resources for each LOD. 
*/
void FSkeletalMeshObjectGPUSkin::ReleaseMorphResources()
{
	for( INT LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs(LODIndex);
		// release morph vertex buffers and factories if they were created
		SkelLOD.ReleaseMorphResources();
	}
	bMorphResourcesInitialized = FALSE;
}

/**
* Called by the game thread for any dynamic data updates for this skel mesh object
* @param	LODIndex - lod level to update
* @param	InSkeletalMeshComponen - parent prim component doing the updating
* @param	ActiveMorphs - morph targets to blend with during skinning
*/
void FSkeletalMeshObjectGPUSkin::Update(INT LODIndex,USkeletalMeshComponent* InSkeletalMeshComponent,const TArray<FActiveMorph>& ActiveMorphs)
{
	// make sure morph data has been initialized for each LOD
	if( !bMorphResourcesInitialized && ActiveMorphs.Num() > 0 )
	{
		// initialized on-the-fly in order to avoid creating extra vertex streams for each skel mesh instance
		InitMorphResources(InSkeletalMeshComponent->bPerBoneMotionBlur);		
	}

	// create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicSkelMeshObjectDataGPUSkin* NewDynamicData = new FDynamicSkelMeshObjectDataGPUSkin(InSkeletalMeshComponent,LODIndex,ActiveMorphs,&DecalRequiredBoneIndices);


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
void FSkeletalMeshObjectGPUSkin::UpdateDynamicData_RenderThread(FDynamicSkelMeshObjectData* InDynamicData)
{
	SCOPE_CYCLE_COUNTER(STAT_GPUSkinUpdateRTTime);
	UBOOL bMorphNeedsUpdate=FALSE;
	// figure out if the morphing vertex buffer needs to be updated. compare old vs new active morphs
	bMorphNeedsUpdate = DynamicData ? (DynamicData->LODIndex != ((FDynamicSkelMeshObjectDataGPUSkin*)InDynamicData)->LODIndex ||
		!DynamicData->ActiveMorphTargetsEqual(((FDynamicSkelMeshObjectDataGPUSkin*)InDynamicData)->ActiveMorphs))
		: TRUE;

	// we should be done with the old data at this point
	delete DynamicData;
	// update with new data
	DynamicData = (FDynamicSkelMeshObjectDataGPUSkin*)InDynamicData;
	checkSlow(DynamicData);

	FSkeletalMeshObjectLOD& LOD = LODs(DynamicData->LODIndex);
	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(DynamicData->LODIndex);

	// if hasn't been updated, force update again
	bMorphNeedsUpdate = LOD.MorphVertexBuffer.HasBeenUpdated? bMorphNeedsUpdate:TRUE;

	const FStaticLODModel& LODModel = SkeletalMesh->LODModels(DynamicData->LODIndex);
	const TArray<FSkelMeshChunk>& Chunks = GetRenderChunks(DynamicData->LODIndex);

	// use correct vertex factories based on alternate weights usage
	const UBOOL bUseAltWeights = MeshLODInfo.bUseInstancedVertexInfluences && LOD.GPUSkinVertexFactoriesAltWeights.VertexFactories.Num() > 0;
	FVertexFactoryData& VertexFactoryData = bUseAltWeights ? LOD.GPUSkinVertexFactoriesAltWeights : LOD.GPUSkinVertexFactories;

	UBOOL DataPresent = FALSE;

	if(DynamicData->NumWeightedActiveMorphs > 0) 
	{
		DataPresent = TRUE;
		checkSlow((VertexFactoryData.MorphVertexFactories.Num() == Chunks.Num()));
	}
	else
	{
//		checkSlow(VertexFactoryData.MorphVertexFactories.Num() == 0);
		DataPresent = VertexFactoryData.VertexFactories.Num() > 0;
	}

	if(DataPresent)
	{
		for( INT ChunkIdx=0; ChunkIdx < Chunks.Num(); ChunkIdx++ )
		{
			const FSkelMeshChunk& Chunk = Chunks(ChunkIdx);
			FGPUSkinVertexFactory::ShaderDataType& ShaderData = DynamicData->NumWeightedActiveMorphs > 0 ? 
				VertexFactoryData.MorphVertexFactories(ChunkIdx).GetShaderData() : VertexFactoryData.VertexFactories(ChunkIdx).GetShaderData();

			TArray<FBoneSkinning>& ChunkMatrices = ShaderData.BoneMatrices;

			// update bone matrix shader data for the vertex factory of each chunk
			ChunkMatrices.Reset(); // remove all elts but leave allocated

			const INT NumBones = Chunk.BoneMap.Num();
			ChunkMatrices.Reserve( NumBones ); // we are going to keep adding data to this for each bone

			const INT NumToAdd = NumBones - ChunkMatrices.Num();
			ChunkMatrices.Add( NumToAdd );

#if QUAT_SKINNING
			TArray<FBoneScale>& ChunkScales = ShaderData.BoneScales;
			ChunkScales.Reset();
			ChunkScales.Reserve(NumBones);
			ChunkScales.Add( NumToAdd );
#endif

#if QUAT_SKINNING
			//FBoneQuat is sizeof() == 32
			// CACHE_LINE_SIZE (128) / 32 = 4
			//  sizeof(FBoneAtom) == 32
			// CACHE_LINE_SIZE (128) / 32 = 4
			const INT PreFetchStride = 4; // PREFETCH stride
#else
			//FSkinMatrix3x4 is sizeof() == 48
			// CACHE_LINE_SIZE (128) / 48 = 2.6
			//  sizeof(FMatrix) == 64
			// CACHE_LINE_SIZE (128) / 64 = 2
			const INT PreFetchStride = 2; // PREFETCH stride
#endif
			TArray<FBoneAtom>& ReferenceToLocalMatrices = DynamicData->ReferenceToLocal;
			const INT NumReferenceToLocal = ReferenceToLocalMatrices.Num();
			for( INT BoneIdx=0; BoneIdx < NumBones; BoneIdx++ )
			{
				CONSOLE_PREFETCH( ChunkMatrices.GetTypedData() + BoneIdx + PreFetchStride ); 
				CONSOLE_PREFETCH_NEXT_CACHE_LINE( ChunkMatrices.GetTypedData() + BoneIdx + PreFetchStride ); 
				CONSOLE_PREFETCH( ReferenceToLocalMatrices.GetTypedData() + BoneIdx + PreFetchStride );
				CONSOLE_PREFETCH_NEXT_CACHE_LINE( ReferenceToLocalMatrices.GetTypedData() + BoneIdx + PreFetchStride );

#if QUAT_SKINNING
				FBoneSkinning& BoneMat = ChunkMatrices(BoneIdx);
				SET_BONE_DATA(BoneMat, ReferenceToLocalMatrices(Chunk.BoneMap(BoneIdx)));

				FBoneScale& BoneScale = ChunkScales(BoneIdx);
				BoneScale = ReferenceToLocalMatrices(Chunk.BoneMap(BoneIdx)).GetScale();
#else
				FBoneSkinning& BoneMat = ChunkMatrices(BoneIdx);
				const WORD RefToLocalIdx = Chunk.BoneMap(BoneIdx);
				const FBoneAtom& RefToLocal = ReferenceToLocalMatrices(RefToLocalIdx);
				RefToLocal.To3x4MatrixTranspose( (FLOAT*)BoneMat.M );
#endif
			}
		}
	}

	// Decal factories
	if ( bDecalFactoriesEnabled )
	{
		TIndirectArray<FGPUSkinDecalVertexFactory>& DecalVertexFactories = VertexFactoryData.DecalVertexFactories;
		TIndirectArray<FGPUSkinMorphDecalVertexFactory>& MorphDecalVertexFactories = VertexFactoryData.MorphDecalVertexFactories;	

		checkSlow( DynamicData->NumWeightedActiveMorphs == 0 || (MorphDecalVertexFactories.Num()==Chunks.Num()) );
		if (DynamicData->NumWeightedActiveMorphs > 0 || DecalVertexFactories.Num() > 0)
		{
			for( INT ChunkIdx=0; ChunkIdx < Chunks.Num(); ChunkIdx++ )
			{
				const FSkelMeshChunk& Chunk = Chunks(ChunkIdx);
				FGPUSkinVertexFactory::ShaderDataType& ShaderData = DynamicData->NumWeightedActiveMorphs > 0 ? 
					MorphDecalVertexFactories(ChunkIdx).GetShaderData() : DecalVertexFactories(ChunkIdx).GetShaderData();

				// update bone matrix shader data for the vertex factory of each chunk
				TArray<FBoneSkinning>& ChunkMatrices = ShaderData.BoneMatrices;
				ChunkMatrices.Reset(); // remove all elts but leave allocated

				const INT NumBones = Chunk.BoneMap.Num();
				ChunkMatrices.Reserve( NumBones ); // we are going to keep adding data to this for each bone

				const INT NumToAdd = NumBones - ChunkMatrices.Num();
				ChunkMatrices.Add( NumToAdd );

#if QUAT_SKINNING
				TArray<FBoneScale>& ChunkScales = ShaderData.BoneScales;
				ChunkScales.Reset();
				ChunkScales.Reserve(NumBones);
				ChunkScales.Add( NumToAdd );
#endif

#if QUAT_SKINNING
				//FBoneQuat is sizeof() == 32
				// CACHE_LINE_SIZE (128) / 32 = 4
				//  sizeof(FBoneAtom) == 32
				// CACHE_LINE_SIZE (128) / 32 = 4
				const INT PreFetchStride = 4; // PREFETCH stride
#else
				//FSkinMatrix3x4 is sizeof() == 48
				// CACHE_LINE_SIZE (128) / 48 = 2.6
				//  sizeof(FMatrix) == 64
				// CACHE_LINE_SIZE (128) / 64 = 2
				const INT PreFetchStride = 2; // PREFETCH stride
#endif

				TArray<FBoneAtom>& ReferenceToLocalMatrices = DynamicData->ReferenceToLocal;
				const INT NumReferenceToLocal = ReferenceToLocalMatrices.Num();

				for( INT BoneIdx=0; BoneIdx < NumBones; BoneIdx++ )
				{
					CONSOLE_PREFETCH( ChunkMatrices.GetTypedData() + BoneIdx + PreFetchStride ); 
					CONSOLE_PREFETCH_NEXT_CACHE_LINE( ChunkMatrices.GetTypedData() + BoneIdx + PreFetchStride ); 
					CONSOLE_PREFETCH( ReferenceToLocalMatrices.GetTypedData() + BoneIdx + PreFetchStride );
					CONSOLE_PREFETCH_NEXT_CACHE_LINE( ReferenceToLocalMatrices.GetTypedData() + BoneIdx + PreFetchStride );

#if QUAT_SKINNING
					FBoneSkinning& BoneMat = ChunkMatrices(BoneIdx);
					SET_BONE_DATA(BoneMat, ReferenceToLocalMatrices(Chunk.BoneMap(BoneIdx)));

					FBoneScale& BoneScale = ChunkScales(BoneIdx);
					BoneScale = ReferenceToLocalMatrices(Chunk.BoneMap(BoneIdx)).GetScale();
#else
					FBoneSkinning& BoneMat = ChunkMatrices(BoneIdx);
					const WORD RefToLocalIdx = Chunk.BoneMap(BoneIdx);
					const FBoneAtom& RefToLocal = ReferenceToLocalMatrices(RefToLocalIdx);
					RefToLocal.To3x4MatrixTranspose( (FLOAT*)BoneMat.M );
#endif					
				}
			}
		}
	}

	// only update if the morph data changed and there are weighted morph targets
	if( bMorphNeedsUpdate &&
		DynamicData->NumWeightedActiveMorphs > 0 )
	{
		// update the morph data for the lod
		LOD.UpdateMorphVertexBuffer( DynamicData->ActiveMorphs );
	}
}

/** 
 * Called by the game thread to toggle usage for the instanced vertex weights.
 * @param bEnabled - TRUE to enable the usage of influence weights
 * @param LODIdx - Index of the influences to toggle
 */
void FSkeletalMeshObjectGPUSkin::ToggleVertexInfluences(UBOOL bEnabled, INT LODIdx)
{
	bEnabled = bEnabled && !GSystemSettings.bDisableSkeletalInstanceWeights;

	// queue a call to update this weight data
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		SkelMeshObjectToggleVertInfluences,
		FSkeletalMeshObject*, MeshObject, this,
		UBOOL, bEnabled, bEnabled,
		INT, LODIdx, LODIdx,
	{
		FSkelMeshObjectLODInfo& MeshLODInfo = MeshObject->LODInfo(LODIdx);
		MeshLODInfo.bUseInstancedVertexInfluences = bEnabled;
	}
	);
} 

/**
 * Called by the game thread to update the instanced vertex weights
 * @param LODIdx - LOD this update is for
 * @param BonePairs - set of bone pairs used to find vertices that need to have their weights updated
 * @param bResetInfluences - resets the array of instanced influences using the ones from the base mesh before updating
 */
void FSkeletalMeshObjectGPUSkin::UpdateVertexInfluences(INT LODIdx, 
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
void FSkeletalMeshObjectGPUSkin::UpdateVertexInfluences_RenderThread(FDynamicUpdateVertexInfluencesData* InDynamicData)
{
	// make sure there is an instance vertex buffer that supports partial swapping of vertex weights
	// this usage requires a unique vertex buffer per skeletal mesh component instance
	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(InDynamicData->LODIdx);
	if (MeshLODInfo.bUseInstancedVertexInfluences &&
		MeshLODInfo.InstanceWeightUsage == IWU_PartialSwap)
	{
		// update instance weights for all LODs
		for( INT CurLODIdx=0; CurLODIdx < LODs.Num(); CurLODIdx++ )
		{
			FSkeletalMeshObjectLOD& LOD = LODs(CurLODIdx);
			const FStaticLODModel& LODModel = SkeletalMesh->LODModels(CurLODIdx);

			if(MeshLODInfo.bUseInstancedVertexInfluences &&
				!IsValidRef(LOD.WeightsVertexBuffer.VertexBufferRHI))
			{
				// we defer InitResource() on WeightsVertexBuffer until we need it, now it's the time
				LOD.WeightsVertexBuffer.InitResource();
			}

			if( IsValidRef(LOD.WeightsVertexBuffer.VertexBufferRHI) && 
				LODModel.VertexInfluences.IsValidIndex(MeshLODInfo.InstanceWeightIdx) )
			{	
				const FSkeletalMeshVertexInfluences& VertexInfluences = LODModel.VertexInfluences(MeshLODInfo.InstanceWeightIdx);

				const INT NumVertices = LODModel.NumVertices;

				if( VertexInfluences.Influences.Num() > 0 &&
					VertexInfluences.Influences.Num() == NumVertices )
				{
					// Lock the buffer.
					const UINT Size = NumVertices * sizeof(FVertexInfluence);
					FVertexInfluence* Buffer = (FVertexInfluence*)RHILockVertexBuffer(LOD.WeightsVertexBuffer.VertexBufferRHI,0,Size,FALSE);

					//Reset all verts if requested
					if( InDynamicData->bResetInfluences )
					{
						ResetInfluences(Buffer, LODModel.VertexBufferGPUSkin, NumVertices);
					}

#if 0 //Swap ALL verts for testing
					for (INT VertIdx=0; VertIdx<NumVertices; VertIdx++)
					{
						const FVertexInfluence& VertexInfluence = VertexInfluences.Influences(VertIdx);

						const FGPUSkinVertexBase* Vertex = LODModel.VertexBufferGPUSkin.GetVertexPtr(VertIdx);
						for( INT Idx=0; Idx < MAX_INFLUENCES; Idx++ )
						{
							Buffer[VertIdx].Weights.InfluenceWeights[Idx] = VertexInfluence.Weights.InfluenceWeights[Idx];
							Buffer[VertIdx].Bones.InfluenceBones[Idx] = VertexInfluence.Bones.InfluenceBones[Idx];
						}
					}
#else
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
								for( INT Idx=0; Idx < MAX_INFLUENCES; Idx++ )
								{
									Buffer[VertexIndex].Weights.InfluenceWeights[Idx] = VertexInfluence.Weights.InfluenceWeights[Idx];
									Buffer[VertexIndex].Bones.InfluenceBones[Idx] = VertexInfluence.Bones.InfluenceBones[Idx];
								}
							}
						}
					}
#endif
					// Unlock the buffer.
					RHIUnlockVertexBuffer(LOD.WeightsVertexBuffer.VertexBufferRHI);
				}
			}
		}
		
	}
}

/**
* Update the contents of the morph target vertex buffer by accumulating all 
* delta positions and delta normals from the set of active morph targets
* @param ActiveMorphs - morph targets to accumulate. assumed to be weighted and have valid targets
*/
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::UpdateMorphVertexBuffer( const TArray<FActiveMorph>& ActiveMorphs )
{
	SCOPE_CYCLE_COUNTER( STAT_MorphVertexBuffer_Update );
	// static variables to initialize vertex buffer, FPackedNormal can't be initialized as 0, so preset arrays to init them
	static FMorphGPUSkinVertex ZeroVertex(	FVector::ZeroVector, FPackedNormal::ZeroNormal	);
	static TArray<FMorphGPUSkinVertex> ZeroVertexArray;

#define DEBUG_MORPH_TANGENT 0

	if( IsValidRef(MorphVertexBuffer.VertexBufferRHI) )
	{
		// LOD of the skel mesh is used to find number of vertices in buffer
		FStaticLODModel& LodModel = SkelMesh->LODModels(LODIndex);
		UINT Size = LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);

#if CONSOLE
		// Lock the buffer.
		FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*)RHILockVertexBuffer(MorphVertexBuffer.VertexBufferRHI,0,Size,FALSE);
#else
		FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*)appMalloc(Size);
#endif

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Init);
			INT MaxZeroVertCount = 2048;	//32K of extra memory
			if (ZeroVertexArray.Num() == 0)
			{
				ZeroVertexArray.Init(ZeroVertex, MaxZeroVertCount);
			}
			// zero all deltas (NOTE: DeltaTangentZ is FPackedNormal, so we can't just appMemzero)
			UINT VertIndex =0;
			for (; VertIndex + (ZeroVertexArray.Num()-1) < LodModel.NumVertices; VertIndex += ZeroVertexArray.Num())
			{
				appMemcpy(&Buffer[VertIndex], ZeroVertexArray.GetData(), sizeof(FMorphGPUSkinVertex)*ZeroVertexArray.Num());
			}
			if ( VertIndex < LodModel.NumVertices )
			{
				appMemcpy(&Buffer[VertIndex], ZeroVertexArray.GetData(), sizeof(FMorphGPUSkinVertex)*(LodModel.NumVertices-VertIndex));
			}
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_ApplyDelta);

			// iterate over all active morph targets and accumulate their vertex deltas
			for( INT MorphIdx=0; MorphIdx < ActiveMorphs.Num(); MorphIdx++ )
			{
				const FActiveMorph& Morph = ActiveMorphs(MorphIdx);
				checkSlow(Morph.Target && Morph.Target->MorphLODModels.IsValidIndex(LODIndex) && Morph.Target->MorphLODModels(LODIndex).Vertices.Num());
				checkSlow(Morph.Weight >= MinMorphBlendWeight && Morph.Weight <= MaxMorphBlendWeight);
				const FMorphTargetLODModel& MorphLODModel = Morph.Target->MorphLODModels(LODIndex);

				FLOAT ClampedMorphWeight = Min(Morph.Weight,1.0f);
				FLOAT ReverseScale = 1.f/(1.f + ClampedMorphWeight);

#if CONSOLE
				VectorRegister MorphTangentZDelta, TangentZDelta, ReverseScaleVect;
				ReverseScaleVect = VectorLoadFloat1(&ReverseScale);

#if DEBUG_MORPH_TANGENT
				FPackedNormal DeltaTangentZ, OriginalDeltaTangentZ;
#endif //DEBUG_MORPH_TANGENT
#endif
				// iterate over the vertices that this lod model has changed
				UBOOL bNumVerticesError = FALSE;
				for( INT MorphVertIdx=0; MorphVertIdx < MorphLODModel.Vertices.Num(); MorphVertIdx++ )
				{
					const FMorphTargetVertex& MorphVertex = MorphLODModel.Vertices(MorphVertIdx);
					if (MorphVertex.SourceIdx >= LodModel.NumVertices)	// # Used to alloc Buffer (above)
					{
						bNumVerticesError = TRUE;
						continue;
					}
					FMorphGPUSkinVertex& DestVertex = Buffer[MorphVertex.SourceIdx];
					if (MorphIdx == 0)
					{
						//if the first morph, use direct assignment and do not blend with what is there (zeros)
						DestVertex.DeltaPosition =  MorphVertex.PositionDelta * Morph.Weight;

#if CONSOLE
						TangentZDelta = Unpack3(&DestVertex.DeltaTangentZ.Vector.Packed);
						TangentZDelta = VectorMultiply(TangentZDelta, ReverseScaleVect);
						Pack3(TangentZDelta, &DestVertex.DeltaTangentZ.Vector.Packed);
#endif
					}
					else
					{
						DestVertex.DeltaPosition += MorphVertex.PositionDelta * Morph.Weight;
#if CONSOLE
						// vectorized method of below function to avoid humongous LHS
						MorphTangentZDelta = Unpack3(&MorphVertex.TangentZDelta.Vector.Packed);
						TangentZDelta = Unpack3(&DestVertex.DeltaTangentZ.Vector.Packed);
	#if DEBUG_MORPH_TANGENT
						OriginalDeltaTangentZ = DestVertex.DeltaTangentZ;
	#endif //DEBUG_MORPH_TANGENT
						TangentZDelta = VectorMultiplyAdd(MorphTangentZDelta, VectorLoadFloat1(&ClampedMorphWeight), TangentZDelta);
						TangentZDelta = VectorMultiply(TangentZDelta, ReverseScaleVect);
	#if DEBUG_MORPH_TANGENT
						Pack3(TangentZDelta, &DeltaTangentZ.Vector.Packed);
						DestVertex.DeltaTangentZ = (FVector(DestVertex.DeltaTangentZ) + FVector(MorphVertex.TangentZDelta) * ClampedMorphWeight)*ReverseScale;	// we scale back to -1 to 1
	#else
						Pack3(TangentZDelta, &DestVertex.DeltaTangentZ.Vector.Packed);
	#endif //DEBUG_MORPH_TANGENT

	#if DEBUG_MORPH_TANGENT
						if (DeltaTangentZ != DestVertex.DeltaTangentZ)
						{
							debugf(TEXT("Original DestDeltaTangent (%s),  Morph TangentDeltaZ (%s), ClampedMorphWeight(%0.5f), Final DeltaTangentZ (%s), DestVertex DeltaTangetZ (%s))"), *OriginalDeltaTangentZ.ToString(), *MorphVertex.TangentZDelta.ToString(), ClampedMorphWeight, *DeltaTangentZ.ToString(), *DestVertex.DeltaTangentZ.ToString());
						}
	#endif //DEBUG_MORPH_TANGENT
#endif
					}
#if !CONSOLE
					DestVertex.DeltaTangentZ = (FVector(DestVertex.DeltaTangentZ) + FVector(MorphVertex.TangentZDelta) * ClampedMorphWeight)*ReverseScale;	// we scale back to -1 to 1
#endif
				}
				if ( bNumVerticesError )
				{
					debugf(TEXT("MorphLODModel (%d) references vertices higher than the vertex count (%d) at this SkeletalMeshLOD (%d) NumVertices (%d) - Mesh render may appear to have corrupt blend"), MorphIdx, MorphLODModel.Vertices.Num(), LODIndex, LodModel.NumVertices );
				}
			}
		}

#if !CONSOLE
		// Lock the real buffer.
		FMorphGPUSkinVertex* ActualBuffer = (FMorphGPUSkinVertex*)RHILockVertexBuffer(MorphVertexBuffer.VertexBufferRHI,0,Size,FALSE);
		appMemcpy(ActualBuffer,Buffer,Size);
		appFree(Buffer);
#endif
		// Unlock the buffer.
		RHIUnlockVertexBuffer(MorphVertexBuffer.VertexBufferRHI);
		// set update flag
		MorphVertexBuffer.HasBeenUpdated = TRUE;
	}
}

/**
 * @param	LODIndex - each LOD has its own vertex data
 * @param	ChunkIdx - index for current mesh chunk
 * @return	vertex factory for rendering the LOD
 */
const FVertexFactory* FSkeletalMeshObjectGPUSkin::GetVertexFactory(INT LODIndex,INT ChunkIdx) const
{
	checkSlow( LODs.IsValidIndex(LODIndex) );
	checkSlow( DynamicData );

	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(LODIndex);
	const FSkeletalMeshObjectLOD& LOD = LODs(LODIndex);

	// use the morph enabled vertex factory if any active morphs are set
	if( DynamicData->NumWeightedActiveMorphs > 0 )
	{
		if (MeshLODInfo.bUseInstancedVertexInfluences && 
			LOD.GPUSkinVertexFactoriesAltWeights.MorphVertexFactories.IsValidIndex(ChunkIdx))
		{
			return &LOD.GPUSkinVertexFactoriesAltWeights.MorphVertexFactories(ChunkIdx);
		}
		return &LOD.GPUSkinVertexFactories.MorphVertexFactories(ChunkIdx);
	}
	// use the local gpu vertex factory (when bForceRefpose is true)
	else if ( bUseLocalVertexFactory )
	{
		return LOD.LocalVertexFactory.GetOwnedPointer();
	}
	// use the default gpu skin vertex factory
	else
	{
		if (MeshLODInfo.bUseInstancedVertexInfluences && 
			LOD.GPUSkinVertexFactoriesAltWeights.VertexFactories.IsValidIndex(ChunkIdx))
		{
			return &LOD.GPUSkinVertexFactoriesAltWeights.VertexFactories(ChunkIdx);
		}
		return &LOD.GPUSkinVertexFactories.VertexFactories(ChunkIdx);
	}
}

/**
 * @param	LODIndex - each LOD has its own vertex data
 * @param	ChunkIdx - index for current mesh chunk
 * @return	Decal vertex factory for rendering the LOD
 */
FDecalVertexFactoryBase* FSkeletalMeshObjectGPUSkin::GetDecalVertexFactory(INT LODIndex,INT ChunkIdx,const FDecalInteraction* Decal)
{
	checkSlow( bDecalFactoriesEnabled );
	checkSlow( LODs.IsValidIndex(LODIndex) );

	FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo(LODIndex);
	FSkeletalMeshObjectLOD& LOD = LODs(LODIndex);

	// use the morph enabled decal vertex factory if any active morphs are set
	if( DynamicData->NumWeightedActiveMorphs > 0 )
	{
		if (MeshLODInfo.bUseInstancedVertexInfluences && 
			LOD.GPUSkinVertexFactoriesAltWeights.MorphDecalVertexFactories.IsValidIndex(ChunkIdx))
		{
			return &LOD.GPUSkinVertexFactoriesAltWeights.MorphDecalVertexFactories(ChunkIdx);
		}
		return &LOD.GPUSkinVertexFactories.MorphDecalVertexFactories(ChunkIdx);
	}
	// use the local decal vertex factory (when bForceRefpose is true)
	else if ( bUseLocalVertexFactory && LOD.LocalDecalVertexFactory )
	{
		return LOD.LocalDecalVertexFactory.GetOwnedPointer();
	}
	// use the default gpu skin decal vertex factory
	else
	{
		if (MeshLODInfo.bUseInstancedVertexInfluences && 
			LOD.GPUSkinVertexFactoriesAltWeights.DecalVertexFactories.IsValidIndex(ChunkIdx))
		{
			return &LOD.GPUSkinVertexFactoriesAltWeights.DecalVertexFactories(ChunkIdx);
		}
		return &LOD.GPUSkinVertexFactories.DecalVertexFactories(ChunkIdx);
	}
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitGPUSkinVertexFactoryComponents(typename VertexFactoryType::DataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	// tangents
	VertexFactoryData->TangentBasisComponents[0] = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(FGPUSkinVertexBase,TangentX),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_PackedNormal);
	VertexFactoryData->TangentBasisComponents[1] = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(FGPUSkinVertexBase,TangentZ),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_PackedNormal);
	
	if (VertexBuffers.AltVertexWeightsBuffer != NULL &&
		VertexBuffers.AltVertexWeightsBuffer->IsInitialized())
	{
		// use the weights/bones stored with this skeletal mesh asset
		
		// bone indices
		VertexFactoryData->BoneIndices = FVertexStreamComponent(
			VertexBuffers.AltVertexWeightsBuffer,STRUCT_OFFSET(FVertexInfluence,Bones.InfluenceBones),sizeof(FVertexInfluence),VET_UByte4);
		// bone weights
		VertexFactoryData->BoneWeights = FVertexStreamComponent(
			VertexBuffers.AltVertexWeightsBuffer,STRUCT_OFFSET(FVertexInfluence,Weights),sizeof(FVertexInfluence),VET_UByte4N);
	}
	else if (VertexBuffers.InstancedWeightsBuffer != NULL)
	{
		// use the weights/bones stored with this skeletal mesh object instance		

		// we don't check if InstancedWeightsBuffer is initialized as the is deferred until needed

		// instanced bones
		VertexFactoryData->BoneIndices = FVertexStreamComponent(
			VertexBuffers.InstancedWeightsBuffer,STRUCT_OFFSET(FVertexInfluence,Bones.InfluenceBones),sizeof(FVertexInfluence),VET_UByte4);

		// instanced weights
		VertexFactoryData->BoneWeights = FVertexStreamComponent(
			VertexBuffers.InstancedWeightsBuffer,STRUCT_OFFSET(FVertexInfluence,Weights),sizeof(FVertexInfluence),VET_UByte4N);
	}
	else
	{
		// bone indices
		VertexFactoryData->BoneIndices = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(FGPUSkinVertexBase,InfluenceBones),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4);
		// bone weights
		VertexFactoryData->BoneWeights = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(FGPUSkinVertexBase,InfluenceWeights),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4N);
	}

	// uvs
	if( !VertexBuffers.VertexBufferGPUSkin->GetUseFullPrecisionUVs() )
	{
#if CONSOLE
		if (VertexBuffers.VertexBufferGPUSkin->GetUsePackedPosition())
		{
			VertexFactoryData->PositionComponent = FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(TGPUSkinVertexFloat16Uvs32Xyz<MAX_TEXCOORDS>,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Pos3N);
		
			// Add a texture coordinate for each texture coordinate set we have
			for( UINT UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
			{
				VertexFactoryData->TextureCoordinates.AddItem(FVertexStreamComponent(
					VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(TGPUSkinVertexFloat16Uvs32Xyz<MAX_TEXCOORDS>,UVs) + sizeof(FVector2DHalf) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(), VET_Half2));
			}
		}
		else
#endif
		{
			VertexFactoryData->PositionComponent = FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float3);

			// Add a texture coordinate for each texture coordinate set we have
			for( UINT UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
			{
				VertexFactoryData->TextureCoordinates.AddItem(FVertexStreamComponent(
					VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2DHalf) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Half2));
			}
		}
	}
	else
	{	
#if CONSOLE
		if (VertexBuffers.VertexBufferGPUSkin->GetUsePackedPosition())
		{
			VertexFactoryData->PositionComponent = FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(TGPUSkinVertexFloat32Uvs32Xyz<MAX_TEXCOORDS>,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Pos3N);

			// Add a texture coordinate for each texture coordinate set we have
			for( UINT UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
			{
				VertexFactoryData->TextureCoordinates.AddItem(FVertexStreamComponent(
					VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(TGPUSkinVertexFloat32Uvs32Xyz<MAX_TEXCOORDS>,UVs) + sizeof(FVector2D) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float2));
			}
		}
		else
#endif
		{
			VertexFactoryData->PositionComponent = FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float3);

			// Add a texture coordinate for each texture coordinate set we have
			for( UINT UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
			{
				VertexFactoryData->TextureCoordinates.AddItem(FVertexStreamComponent(
					VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2D) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float2));
			}
		}
	}

	// Color data may be NULL
	if( VertexBuffers.ColorVertexBuffer != NULL && 
		VertexBuffers.ColorVertexBuffer->IsInitialized() )
	{
		// Color
		VertexFactoryData->ColorComponent = FVertexStreamComponent(
			VertexBuffers.ColorVertexBuffer,STRUCT_OFFSET(FGPUSkinVertexColor,VertexColor),VertexBuffers.ColorVertexBuffer->GetStride(),VET_Color);
	}
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitMorphVertexFactoryComponents(typename VertexFactoryType::DataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	// delta positions
	VertexFactoryData->DeltaPositionComponent = FVertexStreamComponent(
		VertexBuffers.MorphVertexBuffer,STRUCT_OFFSET(FMorphGPUSkinVertex,DeltaPosition),sizeof(FMorphGPUSkinVertex),VET_Float3);
	// delta normals
	VertexFactoryData->DeltaTangentZComponent = FVertexStreamComponent(
		VertexBuffers.MorphVertexBuffer,STRUCT_OFFSET(FMorphGPUSkinVertex,DeltaTangentZ),sizeof(FMorphGPUSkinVertex),VET_PackedNormal);
}

/** 
* Initialize the stream components for using local vertex factory with gpu skin vertex components
*
* @param VertexFactoryData - context for setting the vertex factory stream components. commited later
* @param VertexBuffer - vertex buffer which contains the data and also stride info
*/
template<class VertexFactoryType>
void InitLocalVertexFactoryComponents( typename VertexFactoryType::DataType* VertexFactoryData, const FSkeletalMeshVertexBuffer* VertexBuffer )
{
	checkMsg (FALSE, TEXT("InitLocalVertexFactoryComponents: Doesn't work anymore. If you need to use this, need to fix LocalVertexFactory.usf to work with packed position."));

	// tangents
	VertexFactoryData->TangentBasisComponents[0] = FVertexStreamComponent(
		VertexBuffer,STRUCT_OFFSET(FGPUSkinVertexBase,TangentX),VertexBuffer->GetStride(),VET_PackedNormal);
	VertexFactoryData->TangentBasisComponents[1] = FVertexStreamComponent(
		VertexBuffer,STRUCT_OFFSET(FGPUSkinVertexBase,TangentZ),VertexBuffer->GetStride(),VET_PackedNormal);
	// uvs
	if( !VertexBuffer->GetUseFullPrecisionUVs() )
	{
#if CONSOLE
		if (VertexBuffer->GetUsePackedPosition())
		{
			// set the flag and offset
			// position
			VertexFactoryData->PositionComponent = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(TGPUSkinVertexFloat16Uvs32Xyz<MAX_TEXCOORDS>,Position),VertexBuffer->GetStride(),VET_Pos3N);
			VertexFactoryData->TextureCoordinates.AddItem(FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(TGPUSkinVertexFloat16Uvs32Xyz<MAX_TEXCOORDS>,UVs),VertexBuffer->GetStride(),VET_Half2));
		}
		else
#endif
		{
			// position
			VertexFactoryData->PositionComponent = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>,Position),VertexBuffer->GetStride(),VET_Float3);
			VertexFactoryData->TextureCoordinates.AddItem(FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>,UVs),VertexBuffer->GetStride(),VET_Half2));
		}
	}
	else
	{	
#if CONSOLE
		if (VertexBuffer->GetUsePackedPosition())
		{
			VertexFactoryData->PositionComponent = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(TGPUSkinVertexFloat32Uvs32Xyz<MAX_TEXCOORDS>,Position),VertexBuffer->GetStride(),VET_Pos3N);
			VertexFactoryData->TextureCoordinates.AddItem(FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(TGPUSkinVertexFloat32Uvs32Xyz<MAX_TEXCOORDS>,UVs),VertexBuffer->GetStride(),VET_Float2));
		}
		else
#endif
		{
			VertexFactoryData->PositionComponent = FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>,Position),VertexBuffer->GetStride(),VET_Float3);
			VertexFactoryData->TextureCoordinates.AddItem(FVertexStreamComponent(
				VertexBuffer,STRUCT_OFFSET(TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>,UVs),VertexBuffer->GetStride(),VET_Float2));
		}
	}
}

/** 
 * Handles transferring data between game/render threads when initializing vertex factory components 
 */
template <class VertexFactoryType>
class TDynamicUpdateVertexFactoryData
{
public:
	TDynamicUpdateVertexFactoryData(
		VertexFactoryType* InVertexFactory,
		const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers)
		:	VertexFactory(InVertexFactory)
		,	VertexBuffers(InVertexBuffers)
	{}
	VertexFactoryType* VertexFactory;
	const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers VertexBuffers;
	
};

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryType>
static void CreateVertexFactory(TIndirectArray<VertexFactoryType>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 UBOOL bInUsePerBoneMotionBlur,
						 TArray<FBoneSkinning>& InBoneMatrices
#if QUAT_SKINNING
						 ,TArray<FBoneScale>& InBoneScales
#endif
						 )

{
#if QUAT_SKINNING
	VertexFactoryType* VertexFactory = new(VertexFactories) VertexFactoryType(bInUsePerBoneMotionBlur, InBoneMatrices, InBoneScales);
#else
	VertexFactoryType* VertexFactory = new(VertexFactories) VertexFactoryType(bInUsePerBoneMotionBlur, InBoneMatrices);
#endif

	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory,InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
	InitGPUSkinVertexFactory,
	TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData,VertexUpdateData,
	{
		FGPUSkinVertexFactory::DataType Data;
		InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
		VertexUpdateData.VertexFactory->SetData(Data);
		VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
		VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
	});
	// init rendering resource	
	BeginInitResource(VertexFactory);
}

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryType>
static void CreateVertexFactoryMorph(TIndirectArray<VertexFactoryType>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 UBOOL bInUsePerBoneMotionBlur,
						 TArray<FBoneSkinning>& InBoneMatrices
#if QUAT_SKINNING
						 ,TArray<FBoneScale>& InBoneScales
#endif
						 )

{
#if QUAT_SKINNING
	VertexFactoryType* VertexFactory = new(VertexFactories) VertexFactoryType(bInUsePerBoneMotionBlur, InBoneMatrices, InBoneScales);
#else
	VertexFactoryType* VertexFactory = new(VertexFactories) VertexFactoryType(bInUsePerBoneMotionBlur, InBoneMatrices);
#endif
					 	
	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
	InitGPUSkinVertexFactoryMorph,
	TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData,VertexUpdateData,
	{
		FGPUSkinMorphVertexFactory::DataType Data;
		InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
		InitMorphVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
		VertexUpdateData.VertexFactory->SetData(Data);
		VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
		VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
	});
	// init rendering resource	
	BeginInitResource(VertexFactory);
}

/**
 * Determine the current vertex buffers valid for the current LOD
 *
 * @param OutVertexBuffers output vertex buffers
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::GetVertexBuffers(
	FVertexFactoryBuffers& OutVertexBuffers,
	FStaticLODModel& LODModel,
	const FSkelMeshObjectLODInfo& MeshLODInfo,
	UBOOL bAllowAltWeights)
{
	OutVertexBuffers.VertexBufferGPUSkin = &LODModel.VertexBufferGPUSkin;
	OutVertexBuffers.ColorVertexBuffer = &LODModel.ColorVertexBuffer;
	OutVertexBuffers.MorphVertexBuffer = &MorphVertexBuffer;

	if (bAllowAltWeights)
	{
		check(LODModel.VertexInfluences.IsValidIndex(MeshLODInfo.InstanceWeightIdx));
		if (MeshLODInfo.InstanceWeightUsage == IWU_FullSwap)
		{
			// full swap of weights come from shared skel mesh stream
			OutVertexBuffers.AltVertexWeightsBuffer = &LODModel.VertexInfluences(MeshLODInfo.InstanceWeightIdx);
		}
		else
		{
			// partial swap of weights come from instanced buffer
			OutVertexBuffers.InstancedWeightsBuffer = &WeightsVertexBuffer;
		}
	}
}

/**
 * Init one array of matrices for each chunk (shared across vertex factory types)
 *
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitPerChunkBoneMatrices(const TArray<FSkelMeshChunk>& Chunks)
{
	// !IsInRenderingThread() would not work here
	checkSlow(IsInGameThread());

	// one array of matrices for each chunk (shared across vertex factory types)
	if (PerChunkBoneMatricesArray.Num() != Chunks.Num())
	{
		PerChunkBoneMatricesArray.Empty(Chunks.Num());
		PerChunkBoneMatricesArray.AddZeroed(Chunks.Num());
	}
#if QUAT_SKINNING
	if (PerChunkBoneScalesArray.Num() != Chunks.Num())
	{
		PerChunkBoneScalesArray.Empty(Chunks.Num());
		PerChunkBoneScalesArray.AddZeroed(Chunks.Num());
	}
#endif
}

#if QUAT_SKINNING
	#define OPTIONAL_QUAT_SKINNING ,PerChunkBoneScalesArray(FactoryIdx)
#else
	#define OPTIONAL_QUAT_SKINNING
#endif

/** 
 * Init vertex factory resources for this LOD 
 *
 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 * @param bInitDecals - also init corresponding decal factory
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshChunk>& Chunks,
	UBOOL bInitDecals,
	UBOOL bInUsePerBoneMotionBlur)
{
	// one array of matrices for each chunk (shared across vertex factory types)
	InitPerChunkBoneMatrices(Chunks);

	// first clear existing factories (resources assumed to have been released already)
	// then [re]create the factories

	VertexFactories.Empty(Chunks.Num());
	{
		for(INT FactoryIdx = 0; FactoryIdx < Chunks.Num(); ++FactoryIdx)
		{
			CreateVertexFactory(VertexFactories,VertexBuffers,bInUsePerBoneMotionBlur,PerChunkBoneMatricesArray(FactoryIdx) OPTIONAL_QUAT_SKINNING);
		}
	}

	DecalVertexFactories.Empty(Chunks.Num());
	if(bInitDecals)
	{
		for(INT FactoryIdx = 0; FactoryIdx < Chunks.Num(); ++FactoryIdx)
		{
			CreateVertexFactory<FGPUSkinDecalVertexFactory>(DecalVertexFactories,VertexBuffers,FALSE,PerChunkBoneMatricesArray(FactoryIdx) OPTIONAL_QUAT_SKINNING);
		}
	}
}

#undef OPTIONAL_QUAT_SKINNING

/** 
 * Release vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseVertexFactories()
{
	// Default factories
	for( INT FactoryIdx=0; FactoryIdx < VertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(&VertexFactories(FactoryIdx));
	}
	// Decal factories
	for( INT FactoryIdx=0; FactoryIdx < DecalVertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(&DecalVertexFactories(FactoryIdx));
	}
}

/** 
 * Init morph vertex factory resources for this LOD 
 *
 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 * @param bInitDecals - also init corresponding decal factory
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitMorphVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshChunk>& Chunks,
	UBOOL bInitDecals,
	UBOOL bInUsePerBoneMotionBlur)
{
	// one array of matrices for each chunk (shared across vertex factory types)
	InitPerChunkBoneMatrices(Chunks);
	// clear existing factories (resources assumed to have been released already)
	MorphVertexFactories.Empty(Chunks.Num());
	for( INT FactoryIdx=0; FactoryIdx < Chunks.Num(); FactoryIdx++ )
	{
#if QUAT_SKINNING
		CreateVertexFactoryMorph<FGPUSkinMorphVertexFactory>(MorphVertexFactories,VertexBuffers,bInUsePerBoneMotionBlur,PerChunkBoneMatricesArray(FactoryIdx),PerChunkBoneScalesArray(FactoryIdx));
#else
		CreateVertexFactoryMorph<FGPUSkinMorphVertexFactory>(MorphVertexFactories,VertexBuffers,bInUsePerBoneMotionBlur,PerChunkBoneMatricesArray(FactoryIdx));
#endif
	}

	if ( bInitDecals )
	{
		// clear existing decal factories (resources assumed to have been released already)	
		MorphDecalVertexFactories.Empty(Chunks.Num());
		for( INT FactoryIdx=0; FactoryIdx < Chunks.Num(); FactoryIdx++ )
		{
#if QUAT_SKINNING
			CreateVertexFactoryMorph<FGPUSkinMorphDecalVertexFactory>(MorphDecalVertexFactories,VertexBuffers,bInUsePerBoneMotionBlur,PerChunkBoneMatricesArray(FactoryIdx),PerChunkBoneScalesArray(FactoryIdx));
#else
			CreateVertexFactoryMorph<FGPUSkinMorphDecalVertexFactory>(MorphDecalVertexFactories,VertexBuffers,bInUsePerBoneMotionBlur,PerChunkBoneMatricesArray(FactoryIdx));
#endif
		}
	}
}

/** 
 * Release morph vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseMorphVertexFactories()
{
	// Default morph factories
	for( INT FactoryIdx=0; FactoryIdx < MorphVertexFactories.Num(); FactoryIdx++ )
	{
		FGPUSkinMorphVertexFactory& MorphVertexFactory = MorphVertexFactories(FactoryIdx);
		BeginReleaseResource(&MorphVertexFactory);
	}
	// Decal morph factories
	for( INT FactoryIdx=0; FactoryIdx < MorphDecalVertexFactories.Num(); FactoryIdx++ )
	{
		FGPUSkinMorphDecalVertexFactory& MorphDecalVertexFactory = MorphDecalVertexFactories(FactoryIdx);
		BeginReleaseResource(&MorphDecalVertexFactory);
	}
}

/** 
 * Init rendering resources for this LOD 
 * @param bUseLocalVertexFactory - use non-gpu skinned factory when rendering in ref pose
 * @param MeshLODInfo - information about the state of the bone influence swapping
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitResources(
	UBOOL bUseLocalVertexFactory,
	const FSkelMeshObjectLODInfo& MeshLODInfo,
	UBOOL bInUsePerBoneMotionBlur
	)
{
	check(SkelMesh);
	check(SkelMesh->LODModels.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FStaticLODModel& LODModel = SkelMesh->LODModels(LODIndex);
	
	// initialize vertex buffer of partial (per skelmeshcomponent) weight/bone influences if it exists
	if( LODModel.VertexInfluences.IsValidIndex(MeshLODInfo.InstanceWeightIdx) && !GSystemSettings.bDisableSkeletalInstanceWeights)
	{
		const FSkeletalMeshVertexInfluences& VertexInfluences = LODModel.VertexInfluences(MeshLODInfo.InstanceWeightIdx);
		
		if (VertexInfluences.Usage == IWU_PartialSwap && 
			MeshLODInfo.bUseInstancedVertexInfluences)
		{
			// If possible we defer InitResource on WeightsVertexBuffer until needed
			BeginInitResource(&WeightsVertexBuffer);
		}
	}

	LocalVertexFactory.Reset();
	if (bUseLocalVertexFactory)
	{
		// clear gpu skin factories since they be wont used
		GPUSkinVertexFactories.ClearFactories();
		GPUSkinVertexFactoriesAltWeights.ClearFactories();

		// only need one local vertex factory because this is no unique data per chunk when using the local factory.
		LocalVertexFactory.Reset(new FLocalVertexFactory());

		// update vertex factory components and sync it
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitGPUSkinLocalVertexFactory,
			FLocalVertexFactory*,VertexFactory,LocalVertexFactory.GetOwnedPointer(),
			FStaticLODModel*,LODModel,&LODModel,
			{
				FLocalVertexFactory::DataType Data;
				InitLocalVertexFactoryComponents<FLocalVertexFactory>(&Data,&LODModel->VertexBufferGPUSkin);
				VertexFactory->SetData(Data);
			});
		// init rendering resource	
		BeginInitResource(LocalVertexFactory.GetOwnedPointer());

		if (bDecalFactoriesEnabled)
		{
			// only need one local vertex factory because this is no unique data per chunk when using the local factory.
			LocalDecalVertexFactory.Reset(new FLocalDecalVertexFactory());

			// update vertex factory components and sync it
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					InitGPUSkinDecalVertexFactory,
					FLocalDecalVertexFactory*,DecalVertexFactory,LocalDecalVertexFactory.GetOwnedPointer(),
					FStaticLODModel*,LODModel,&LODModel,
				{
					FLocalDecalVertexFactory::DataType Data;
					// init default gpu skin components
					InitLocalVertexFactoryComponents<FLocalDecalVertexFactory>(&Data,&LODModel->VertexBufferGPUSkin);
					DecalVertexFactory->SetData(Data);
				});
			// init rendering resource	
			BeginInitResource(LocalDecalVertexFactory.GetOwnedPointer());
		}
	}
	else
	{
		// Vertex buffers available for the LOD
		FVertexFactoryBuffers VertexBuffers;
		GetVertexBuffers(VertexBuffers,LODModel,MeshLODInfo,FALSE);
		// init gpu skin factories
		GPUSkinVertexFactories.InitVertexFactories(VertexBuffers,LODModel.Chunks,bDecalFactoriesEnabled,bInUsePerBoneMotionBlur);

		// determine if there are alternate weights
		if (LODModel.VertexInfluences.IsValidIndex(MeshLODInfo.InstanceWeightIdx) && !GSystemSettings.bDisableSkeletalInstanceWeights)
		{
			const FSkeletalMeshVertexInfluences& VertInfluences = LODModel.VertexInfluences(MeshLODInfo.InstanceWeightIdx);
			// for a full swap then use alternate set of chunks
			const UBOOL bUseInstanceChunks = VertInfluences.Usage == IWU_FullSwap && VertInfluences.Chunks.Num() > 0;
			const TArray<FSkelMeshChunk>& ChunksAltWeight = bUseInstanceChunks ? VertInfluences.Chunks : LODModel.Chunks;

			FVertexFactoryBuffers VertexBuffersAltWeights;
			GetVertexBuffers(VertexBuffersAltWeights,LODModel,MeshLODInfo,TRUE);

			// init gpu skin factories for alternate weights
			GPUSkinVertexFactoriesAltWeights.InitVertexFactories(VertexBuffersAltWeights,ChunksAltWeight,bDecalFactoriesEnabled,bInUsePerBoneMotionBlur);
		}
	}
}

/** 
 * Release rendering resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseResources()
{	
	// Release gpu skin default/decal vertex factories
	GPUSkinVertexFactories.ReleaseVertexFactories();
	// Release gpu skin default/decal vertex factories for alt weights
	GPUSkinVertexFactoriesAltWeights.ReleaseVertexFactories();
	// Release local default/decal vertex factories
	if (LocalVertexFactory)
	{
		BeginReleaseResource(LocalVertexFactory.GetOwnedPointer());
	}
	if (LocalDecalVertexFactory)
	{
		BeginReleaseResource(LocalDecalVertexFactory.GetOwnedPointer());
	}
	// Release the influence weight vertex buffer
	BeginReleaseResource(&WeightsVertexBuffer);
}

/** 
 * Init rendering resources for the morph stream of this LOD
 * @param MeshLODInfo - information about the state of the bone influence swapping
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, UBOOL bInUsePerBoneMotionBlur)
{
	check(SkelMesh);
	check(SkelMesh->LODModels.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FStaticLODModel& LODModel = SkelMesh->LODModels(LODIndex);

	// init the delta vertex buffer for this LOD
	BeginInitResource(&MorphVertexBuffer);

	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers,LODModel,MeshLODInfo,FALSE);
	// init morph skin factories
	GPUSkinVertexFactories.InitMorphVertexFactories(VertexBuffers,LODModel.Chunks,bDecalFactoriesEnabled,bInUsePerBoneMotionBlur);

	// determine if there are alternate weights
	if (LODModel.VertexInfluences.IsValidIndex(MeshLODInfo.InstanceWeightIdx) && !GSystemSettings.bDisableSkeletalInstanceWeights)
	{
		const FSkeletalMeshVertexInfluences& VertInfluences = LODModel.VertexInfluences(MeshLODInfo.InstanceWeightIdx);
		// for a full swap then use alternate set of chunks
		const UBOOL bUseInstanceChunks = VertInfluences.Usage == IWU_FullSwap && VertInfluences.Chunks.Num() > 0;
		const TArray<FSkelMeshChunk>& ChunksAltWeight = bUseInstanceChunks ? VertInfluences.Chunks : LODModel.Chunks;

		FVertexFactoryBuffers VertexBuffersAltWeights;
		GetVertexBuffers(VertexBuffersAltWeights,LODModel,MeshLODInfo,TRUE);

		// init morph skin factories for alternate weights
		GPUSkinVertexFactoriesAltWeights.InitMorphVertexFactories(VertexBuffersAltWeights,ChunksAltWeight,bDecalFactoriesEnabled,bInUsePerBoneMotionBlur);
	}
}

/** 
* Release rendering resources for the morph stream of this LOD
*/
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseMorphResources()
{
	// Release morph default/decal vertex factories
	GPUSkinVertexFactories.ReleaseMorphVertexFactories();
	// Release morph default/decal vertex factories for alt weights
	GPUSkinVertexFactoriesAltWeights.ReleaseMorphVertexFactories();
	// release the delta vertex buffer
	BeginReleaseResource(&MorphVertexBuffer);
}

/** 
 *	Get the array of component-space bone transforms. 
 *	Not safe to hold this point between frames, because it exists in dynamic data passed from main thread.
 */
TArray<FBoneAtom>* FSkeletalMeshObjectGPUSkin::GetSpaceBases() const
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

TArray<FVector>* FSkeletalMeshObjectGPUSkin::GetSoftBodyTetraPosData() const
{
	// Soft-Bodies only work with CPU skinning currently.
	return NULL;	
}

/**
 * Allows derived types to transform decal state into a space that's appropriate for the given skinning algorithm.
 */
void FSkeletalMeshObjectGPUSkin::TransformDecalState(const FDecalState& DecalState,
													 FMatrix& OutDecalMatrix,
													 FVector& OutDecalLocation,
													 FVector2D& OutDecalOffset,
													 FBoneAtom& OutDecalRefToLocal)
{
	// The decal is already in the 'reference pose' space; just pass values along.
	OutDecalMatrix = DecalState.WorldTexCoordMtx;
	OutDecalLocation = DecalState.HitLocation;
	OutDecalOffset = FVector2D(DecalState.OffsetX, DecalState.OffsetY);
	
	if( DecalState.HitBoneIndex != INDEX_NONE  && DynamicData )
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
	}
	else
	{
		OutDecalRefToLocal = FBoneAtom::Identity; 	// default
	}
}

/**
 * Get the origin and direction vectors for TRISORT_CustomLeftRight sections
 */
const FTwoVectors& FSkeletalMeshObjectGPUSkin::GetCustomLeftRightVectors(INT SectionIndex) const
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
FDynamicSkelMeshObjectDataGPUSkin
-----------------------------------------------------------------------------*/

/**
* Constructor
* Updates the ReferenceToLocal matrices using the new dynamic data.
* @param	InSkelMeshComponent - parent skel mesh component
* @param	InLODIndex - each lod has its own bone map 
* @param	InActiveMorphs - morph targets active for the mesh
* @param	DecalRequiredBoneIndices - any bones needed to render decals
*/
FDynamicSkelMeshObjectDataGPUSkin::FDynamicSkelMeshObjectDataGPUSkin(
	USkeletalMeshComponent* InSkelMeshComponent,
	INT InLODIndex,
	const TArray<FActiveMorph>& InActiveMorphs,
	const TArray<WORD>* DecalRequiredBoneIndices
	)
	:	LODIndex(InLODIndex)
	,	ActiveMorphs(InActiveMorphs)
	,	NumWeightedActiveMorphs(0)
{
	// update ReferenceToLocal
	UpdateRefToLocalMatrices( ReferenceToLocal, InSkelMeshComponent, LODIndex, DecalRequiredBoneIndices );

	UpdateCustomLeftRightVectors( CustomLeftRightVectors, InSkelMeshComponent, LODIndex );


#if !FINAL_RELEASE
	MeshSpaceBases = InSkelMeshComponent->SpaceBases;
#endif

	// find number of morphs that are currently weighted and will affect the mesh
	for( INT MorphIdx=ActiveMorphs.Num()-1; MorphIdx >= 0; MorphIdx-- )
	{
		const FActiveMorph& Morph = ActiveMorphs(MorphIdx);
		if( Morph.Weight >= MinMorphBlendWeight &&
			Morph.Weight <= MaxMorphBlendWeight &&
			Morph.Target &&
			Morph.Target->MorphLODModels.IsValidIndex(LODIndex) &&
			Morph.Target->MorphLODModels(LODIndex).Vertices.Num() )
		{
			NumWeightedActiveMorphs++;
		}
		else
		{
			ActiveMorphs.Remove(MorphIdx);
		}
	}
}

/**
* Compare the given set of active morph targets with the current list to check if different
* @param CompareActiveMorphs - array of morph targets to compare
* @return TRUE if boths sets of active morph targets are equal
*/
UBOOL FDynamicSkelMeshObjectDataGPUSkin::ActiveMorphTargetsEqual( const TArray<FActiveMorph>& CompareActiveMorphs )
{
	UBOOL Result=TRUE;
	if( CompareActiveMorphs.Num() == ActiveMorphs.Num() )
	{
		const FLOAT MorphWeightThreshold = 0.001f;
		for( INT MorphIdx=0; MorphIdx < ActiveMorphs.Num(); MorphIdx++ )
		{
			const FActiveMorph& Morph = ActiveMorphs(MorphIdx);
			const FActiveMorph& CompMorph = CompareActiveMorphs(MorphIdx);

			if( Morph.Target != CompMorph.Target ||
				Abs(Morph.Weight - CompMorph.Weight) >= MorphWeightThreshold )
			{
				Result=FALSE;
				break;
			}
		}
	}
	else
	{
		Result = FALSE;
	}
	return Result;
}



/*-----------------------------------------------------------------------------
FPreviousPerBoneMotionBlur
-----------------------------------------------------------------------------*/

/** constructor */
FPreviousPerBoneMotionBlur::FPreviousPerBoneMotionBlur()
	:BufferIndex(0), InvSizeX(0), LockedData(0), LockedTexelPosition(0), LockedTexelCount(0)
{
}

/** 
* call from render thread
* @param TotalTexelCount sum of all chunks bone count
*/
void FPreviousPerBoneMotionBlur::SetTexelSizeAndInitResource(UINT TotalTexelCount)
{
	checkSlow(IsInRenderingThread());

	// without support for that we don't need the textures 
	if(!GSupportsVertexTextureFetch)
	{
		return;
	}

	for(int i = 0; i < PER_BONE_BUFFER_COUNT; ++i)
	{
		PerChunkBoneMatricesTexture[i].SetTexelSize(TotalTexelCount);
		PerChunkBoneMatricesTexture[i].InitResource();
	}

	InvSizeX = 1.0f / TotalTexelCount; 
}

/** 
* call from render thread
*/
void FPreviousPerBoneMotionBlur::ReleaseResources()
{
	checkSlow(IsInRenderingThread());

	for(int i = 0; i < PER_BONE_BUFFER_COUNT; ++i)
	{
		PerChunkBoneMatricesTexture[i].ReleaseResource();
	}
}

/** Returns the width of the texture in pixels. */
UINT FPreviousPerBoneMotionBlur::GetSizeX() const
{
	return PerChunkBoneMatricesTexture[0].GetSizeX();
}

/** Returns the 1 / width of the texture (cached for better performance). */
FLOAT FPreviousPerBoneMotionBlur::GetInvSizeX() const
{
	return InvSizeX;
}

/** So we update only during velocity rendering pass. */
UBOOL FPreviousPerBoneMotionBlur::IsLocked() const
{
	return LockedData != 0;
}

/** needed before AppendData() ccan be called */
void FPreviousPerBoneMotionBlur::LockData()
{
	checkSlow(!LockedData);
	checkSlow(IsInRenderingThread());

	FBoneDataTexture& WriteTexture = PerChunkBoneMatricesTexture[GetWriteBufferIndex()];

	if(IsValidRef(WriteTexture.GetTexture2DRHI()))
	{
		LockedData = WriteTexture.LockData();
		LockedTexelPosition = 0;
		LockedTexelCount = WriteTexture.GetSizeX();
	}
}

/**
 * use between LockData() and UnlockData()
 * @param	DataStart, must not be 0
 * @param	BoneCount number of FBoneSkinning elements, must not be 0
 * @return	StartIndex where the data can be referenced in the texture, 0xffffffff if this failed (not enough space in the buffer, will be fixed next frame)
 */
UINT FPreviousPerBoneMotionBlur::AppendData(FBoneSkinning *DataStart, UINT BoneCount)
{
	checkSlow(LockedData);
	checkSlow(DataStart);
	checkSlow(BoneCount);

	UINT TexelCount = BoneCount * sizeof(FBoneSkinning) / sizeof(FLOAT) / 4;

	UINT OldLockedTexelPosition = LockedTexelPosition;
	
	LockedTexelPosition += TexelCount;

	if(LockedTexelPosition <= LockedTexelCount)
	{
		appMemcpy(&LockedData[OldLockedTexelPosition * 4], DataStart, BoneCount * sizeof(FBoneSkinning));

		return OldLockedTexelPosition;
	}
	else
	{
		// Not enough space in the texture, we should increase the texture size. The new bigger size 
		// can be found in LockedTexelPosition. This is currently not done - so we might not see motion blur
		// skinning on all objects.
		checkSlow(0);
		return 0xffffffff;
	}
}

// only call if LockData()
void FPreviousPerBoneMotionBlur::UnlockData()
{
	if(IsLocked())
	{
		LockedTexelPosition = 0;
		LockedTexelCount = 0;
		LockedData = 0;

		PerChunkBoneMatricesTexture[GetWriteBufferIndex()].UnlockData();

		AdvanceBufferIndex();
	}
}

/** @return 0 if there should be no bone based motion blur (no previous data available or it's not active) */
FBoneDataTexture* FPreviousPerBoneMotionBlur::GetReadData()
{
	return &PerChunkBoneMatricesTexture[GetReadBufferIndex()];
}

/** @return 0 .. PER_BONE_BUFFER_COUNT-1 */
UINT FPreviousPerBoneMotionBlur::GetReadBufferIndex() const
{
	return BufferIndex;
}

/** @return 0 .. PER_BONE_BUFFER_COUNT-1 */
UINT FPreviousPerBoneMotionBlur::GetWriteBufferIndex() const
{
	UINT ret = BufferIndex + 1;

	if(ret >= PER_BONE_BUFFER_COUNT)
	{
		ret = 0;
	}
	return ret;
}

/** to cycle the internal buffer counter */
void FPreviousPerBoneMotionBlur::AdvanceBufferIndex()
{
	++BufferIndex;

	if(BufferIndex >= PER_BONE_BUFFER_COUNT)
	{
		BufferIndex = 0;
	}
}