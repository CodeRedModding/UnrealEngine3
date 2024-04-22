/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "UnFracturedMeshRender.h"

void FDummyWeightsVertexBuffer::InitRHI()
{
	//create a vertex buffer with one FInfluenceWeights element 
	const UINT Size = 1 * sizeof(FInfluenceWeights);
	VertexBufferRHI = RHICreateVertexBuffer(Size,NULL,RUF_Static);
	FInfluenceWeights* Buffer = (FInfluenceWeights*)RHILockVertexBuffer(VertexBufferRHI,0,Size,FALSE);

	//set the first bone to get weighted in completely
#if __INTEL_BYTE_ORDER__
	Buffer[0].InfluenceWeights[0] = 255;
	Buffer[0].InfluenceWeights[1] = 0;
	Buffer[0].InfluenceWeights[2] = 0;
	Buffer[0].InfluenceWeights[3] = 0;
#else
	Buffer[0].InfluenceWeights[0] = 0;
	Buffer[0].InfluenceWeights[1] = 0;
	Buffer[0].InfluenceWeights[2] = 0;
	Buffer[0].InfluenceWeights[3] = 255;
#endif

	RHIUnlockVertexBuffer(VertexBufferRHI);
}

void FDummyWeightsVertexBuffer::ReleaseRHI()
{
	VertexBufferRHI.SafeRelease();
}

TGlobalResource<FDummyWeightsVertexBuffer> GDummyWeightsVertexBuffer;

FBoneInfluenceVertexBuffer::FBoneInfluenceVertexBuffer(UFracturedStaticMesh* InMesh) :
	Mesh(InMesh)
{}

void FBoneInfluenceVertexBuffer::InitRHI()
{
	check(Mesh && Mesh->LODModels.Num() > 0);
	const FStaticMeshRenderData& RenderData = Mesh->LODModels(0);
	const UINT Size = RenderData.PositionVertexBuffer.GetNumVertices() * sizeof(FInfluenceBones);
	VertexBufferRHI = RHICreateVertexBuffer(Size,NULL,RUF_Static);
	FInfluenceBones* Buffer = (FInfluenceBones*)RHILockVertexBuffer(VertexBufferRHI,0,Size,FALSE);
	appMemzero(Buffer, Size);

	//setup a fragment index for each vertex
	for (INT ElementIndex = 0; ElementIndex < RenderData.Elements.Num(); ElementIndex++)
	{
		const FStaticMeshElement& CurrentElement = RenderData.Elements(ElementIndex);
		for (INT FragmentIndex = 0; FragmentIndex < CurrentElement.Fragments.Num(); FragmentIndex++)
		{
			const FFragmentRange& CurrentIndexRange = CurrentElement.Fragments(FragmentIndex);
			for (INT CurrentIndex = CurrentIndexRange.BaseIndex; CurrentIndex < CurrentIndexRange.BaseIndex + CurrentIndexRange.NumPrimitives * 3; CurrentIndex++)
			{
				const INT VertexIndex = RenderData.IndexBuffer.Indices(CurrentIndex);
				//multiple chunks will be used for rendering to fit in constant register space, so the index will never be higher than MAX_GPUSKIN_BONES
				const INT EffectiveFragmentIndex = FragmentIndex % MAX_GPUSKIN_BONES;
				//Note - this would have to be byte-swapped if they weren't all the same
				Buffer[VertexIndex].InfluenceBones[0] = EffectiveFragmentIndex;
				Buffer[VertexIndex].InfluenceBones[1] = EffectiveFragmentIndex;
				Buffer[VertexIndex].InfluenceBones[2] = EffectiveFragmentIndex;
				Buffer[VertexIndex].InfluenceBones[3] = EffectiveFragmentIndex;
			}
		}
	}

	RHIUnlockVertexBuffer(VertexBufferRHI);
}

void FBoneInfluenceVertexBuffer::ReleaseRHI()
{
	VertexBufferRHI.SafeRelease();
}

INT FBoneInfluenceVertexBuffer::GetStride() const
{
	return sizeof(FInfluenceBones);
}

FFracturedSkinResources::FFracturedSkinResources(INT NumBones)
{
	INT NumFactories = (NumBones + MAX_GPUSKIN_BONES - 1) / MAX_GPUSKIN_BONES;
	PerChunkBoneMatricesArray.Empty(NumFactories);
	PerChunkBoneMatricesArray.AddZeroed(NumFactories);

#if QUAT_SKINNING
	PerChunkBoneScalesArray.Empty(NumFactories);
	PerChunkBoneScalesArray.AddZeroed(NumFactories);
#endif

	for( INT FactoryIndex=0; FactoryIndex<NumFactories; FactoryIndex++ )
	{
#if QUAT_SKINNING
		//add a chunk for each MAX_GPUSKIN_BONES fragments
		ChunkVertexFactories.AddItem(FGPUSkinVertexFactory(FALSE, PerChunkBoneMatricesArray(FactoryIndex),PerChunkBoneScalesArray(FactoryIndex)));
#else
		//add a chunk for each MAX_GPUSKIN_BONES fragments
		ChunkVertexFactories.AddItem(FGPUSkinVertexFactory(FALSE, PerChunkBoneMatricesArray(FactoryIndex)));
#endif
	}
}

/*-----------------------------------------------------------------------------
	FFracturedBaseSceneProxy
-----------------------------------------------------------------------------*/

FFracturedBaseSceneProxy::FFracturedBaseSceneProxy(const UFracturedBaseComponent* Component):
	FStaticMeshSceneProxy(Component),
	bUseDynamicIndexBuffer(Component->bUseDynamicIndexBuffer),
	FracturedStaticMesh(CastChecked<UFracturedStaticMesh>(Component->StaticMesh)),
	ComponentBaseResources(Component->ComponentBaseResources)
{
	const FStaticMeshRenderData& RenderData = FracturedStaticMesh->LODModels(0);
	//check fractured mesh usage for all child component types, even if they do not actually use fractured shaders for rendering
	//this is to make sure the incorrect usage shows up all the time and not just when spawning some detached parts
	for (INT ElementIndex = 0; ElementIndex < RenderData.Elements.Num(); ElementIndex++)
	{
		UMaterialInterface* AssignedMaterial = LODs(0).Elements(ElementIndex).Material;
		if (!AssignedMaterial || !AssignedMaterial->CheckMaterialUsage(MATUSAGE_FracturedMeshes))
		{
			LODs(0).Elements(ElementIndex).Material = GEngine->DefaultMaterial;
		}
	}
}

/*-----------------------------------------------------------------------------
	FFracturedStaticMeshSceneProxy
-----------------------------------------------------------------------------*/

/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
FFracturedStaticMeshSceneProxy::FFracturedStaticMeshSceneProxy(const UFracturedStaticMeshComponent* Component):
	FFracturedBaseSceneProxy(Component)
{
	//check that neighbor visibility has been cached
	check(Component->VisibleFragments.Num() == Component->FragmentNeighborsVisible.Num());

	const INT InteriorElementIndex = FracturedStaticMesh->GetInteriorElementIndex();
	const INT CoreFragmentIndex = FracturedStaticMesh->GetCoreFragmentIndex();

	//@todo: handle multiple LOD's
	const FStaticMeshRenderData& RenderData = FracturedStaticMesh->LODModels(0);
	const FRawStaticIndexBuffer& ResourceIndexBuffer = RenderData.IndexBuffer;
	//make sure the index buffer is setup for triangle lists
	check(ResourceIndexBuffer.Indices.Num() % 3 == 0);

	//add an entry for each element in the resource
	ElementRanges.AddZeroed(RenderData.Elements.Num());

	//check if any fragments are hidden, this will be used later in determining the visibility of the core
	UBOOL bAnyFragmentsHidden = FALSE;
	for (INT FragmentIndex = 0; FragmentIndex < Component->VisibleFragments.Num(); FragmentIndex++)
	{
		if (Component->VisibleFragments(FragmentIndex) == 0)
		{
			bAnyFragmentsHidden = TRUE;
			break;
		}
	}

	if (bUseDynamicIndexBuffer)
	{
		//using the dynamic index buffer method, pack the component's index buffer so that each element can be rendered with one draw call
		INT DestIndex = 0;

		//update ElementRanges, which store index ranges for each element taking into account hidden fragments
		for (INT ElementIndex = 0; ElementIndex < RenderData.Elements.Num(); ElementIndex++)
		{
			FFracturedElementRange NewFragmentRange;
			NewFragmentRange.BaseIndex = ResourceIndexBuffer.Indices.Num();
			NewFragmentRange.NumPrimitives = 0;
			const FStaticMeshElement& CurrentElement = RenderData.Elements(ElementIndex);
			for (INT FragmentIndex = 0; FragmentIndex < CurrentElement.Fragments.Num(); FragmentIndex++)
			{
				if (Component->IsElementFragmentVisible(ElementIndex, FragmentIndex, InteriorElementIndex, CoreFragmentIndex, bAnyFragmentsHidden))
				{
					FFragmentRange CurrentFragment = CurrentElement.Fragments(FragmentIndex);
					NewFragmentRange.BaseIndex = Min(NewFragmentRange.BaseIndex, DestIndex);
					NewFragmentRange.NumPrimitives += CurrentFragment.NumPrimitives;
					DestIndex += CurrentFragment.NumPrimitives * 3;
				}
			}
			//only add one range for the element
			ElementRanges(ElementIndex).AddItem(NewFragmentRange);
		}
	}
	else
	{
		//not using a packed index buffer, each element will have to be rendered with multiple draw calls (one for each consecutive range of indices)
		check(LODs(0).Elements.Num() == RenderData.Elements.Num());
		//update ElementRanges, which store index ranges for each element taking into account hidden fragments
		for (INT ElementIndex = 0; ElementIndex < RenderData.Elements.Num(); ElementIndex++)
		{
			const FStaticMeshElement& CurrentElement = RenderData.Elements(ElementIndex);
			for (INT FragmentIndex = 0; FragmentIndex < CurrentElement.Fragments.Num(); FragmentIndex++)
			{
				if (Component->IsElementFragmentVisible(ElementIndex, FragmentIndex, InteriorElementIndex, CoreFragmentIndex, bAnyFragmentsHidden))
				{
					FFragmentRange CurrentFragment = CurrentElement.Fragments(FragmentIndex);
					if (ElementRanges(ElementIndex).Num() == 0)
					{
						FFracturedElementRange NewRange;
						NewRange.BaseIndex = CurrentFragment.BaseIndex;
						NewRange.NumPrimitives = CurrentFragment.NumPrimitives;
						ElementRanges(ElementIndex).AddItem(NewRange);
					}
					else
					{
						//check for an existing continuous range
						//Fragments are stored in ascending order based on BaseIndex, so we only have to check the previous range
						FFracturedElementRange& LastRange = ElementRanges(ElementIndex)(ElementRanges(ElementIndex).Num() - 1);
						if (LastRange.BaseIndex + LastRange.NumPrimitives * 3 == CurrentFragment.BaseIndex)
						{
							LastRange.NumPrimitives += CurrentFragment.NumPrimitives;
						}
						else
						{	
							FFracturedElementRange NewRange;
							NewRange.BaseIndex = CurrentFragment.BaseIndex;
							NewRange.NumPrimitives = CurrentFragment.NumPrimitives;
							ElementRanges(ElementIndex).AddItem(NewRange);
						}
					}
				}
			}

			//set NumFragments to the required number so that NumFragments FMeshElements will be submitted
			LODs(0).Elements(ElementIndex).NumFragments = ElementRanges(ElementIndex).Num();
		}
	}
}

/**
 * Sets IndexBuffer, FirstIndex and NumPrimitives of OutMeshElement.
 */
void FFracturedStaticMeshSceneProxy::SetIndexSource(INT LODIndex, INT ElementIndex, INT FragmentIndex, FMeshBatch& OutMeshElement, UBOOL bWireframe, UBOOL bRequiresAdjacencyInformation) const
{
	const FStaticMeshRenderData& LODModel = FracturedStaticMesh->LODModels(LODIndex);
	FMeshBatchElement& BatchElement = OutMeshElement.Elements(0);
	if (bWireframe)
	{
		if (LODIndex == 0 && bUseDynamicIndexBuffer)
		{
			//@todo - handle wireframe when !bUseDynamicIndexBuffer
			const FRawIndexBuffer& FracturedIndexBuffer = ComponentBaseResources->InstanceIndexBuffer;
			BatchElement.IndexBuffer = &FracturedIndexBuffer;
			BatchElement.NumPrimitives = FracturedIndexBuffer.Indices.Num() / 3;
		}
		else
		{
			BatchElement.IndexBuffer = &LODModel.IndexBuffer;
			BatchElement.NumPrimitives = LODModel.IndexBuffer.Indices.Num() / 3;
		}
		OutMeshElement.Type = PT_TriangleList;
		BatchElement.FirstIndex = 0;
		OutMeshElement.bWireframe = TRUE;
	}
	else
	{
		const FStaticMeshElement& Element = LODModel.Elements(ElementIndex);
		if (LODIndex == 0)
		{
			OutMeshElement.Type = PT_TriangleList;
			if (bUseDynamicIndexBuffer)
			{
				//override the resource's index buffer with the fractured mesh component's index buffer
				const FRawIndexBuffer& FracturedIndexBuffer = ComponentBaseResources->InstanceIndexBuffer;
				BatchElement.IndexBuffer = &FracturedIndexBuffer;
				//there should only be one index range per element when bUseDynamicIndexBuffer is true
				checkSlow(ElementRanges(ElementIndex).Num() == 1);
				FFracturedElementRange ElementRange = ElementRanges(ElementIndex)(0);
				BatchElement.FirstIndex = ElementRange.BaseIndex;
				BatchElement.NumPrimitives = ElementRange.NumPrimitives;
			}
			else
			{
				//use the appropriate range in the resource index buffer
				BatchElement.IndexBuffer = &LODModel.IndexBuffer;
				FFracturedElementRange ElementRange = ElementRanges(ElementIndex)(FragmentIndex);
				BatchElement.FirstIndex = ElementRange.BaseIndex;
				BatchElement.NumPrimitives = ElementRange.NumPrimitives;
			}
		}
		else
		{
			OutMeshElement.Type = PT_TriangleList;
			BatchElement.IndexBuffer = &LODModel.IndexBuffer;
			BatchElement.FirstIndex = Element.FirstIndex;
			BatchElement.NumPrimitives = Element.NumTriangles;
		}
	}

#if WITH_D3D11_TESSELLATION
	if ( bRequiresAdjacencyInformation )
	{
		check( LODModel.AdjacencyIndexBuffer.Indices.Num() > 0 );
		BatchElement.IndexBuffer = &LODModel.AdjacencyIndexBuffer;
		OutMeshElement.Type = PT_12_ControlPointPatchList;
		BatchElement.FirstIndex *= 4;
	}
#endif // #if WITH_D3D11_TESSELLATION
}

/**
* Rebuilds the static mesh elements for decals that have a missing FStaticMesh* entry for their interactions
* enqued by BuildMissingDecalStaticMeshElements_GameThread on the render thread
*/
void FFracturedStaticMeshSceneProxy::BuildMissingDecalStaticMeshElements_RenderThread()
{
	// regeneration of any missing static mesh elements for decal interactions which remain attached
	for( INT DecalIdx=0; DecalIdx < Decals[DYNAMIC_DECALS].Num(); DecalIdx++ )
	{
		FDecalInteraction* DecalInteraction = Decals[DYNAMIC_DECALS](DecalIdx);
		if( DecalInteraction )
		{
			DecalInteraction->CreateDecalStaticMesh(PrimitiveSceneInfo);
		}
	}
}

/*-----------------------------------------------------------------------------
FFracturedSkinnedMeshSceneProxy
-----------------------------------------------------------------------------*/

FFracturedSkinnedMeshSceneProxy::FFracturedSkinnedMeshSceneProxy(const UFracturedSkinnedMeshComponent* Component):
	FFracturedBaseSceneProxy(Component),
	ComponentSkinResources(Component->ComponentSkinResources)
{
	bMovable = TRUE;

	//@todo: handle multiple LOD's
	const FStaticMeshRenderData& RenderData = FracturedStaticMesh->LODModels(0);
	const FRawStaticIndexBuffer& ResourceIndexBuffer = RenderData.IndexBuffer;
	//make sure the index buffer is setup for triangle lists
	check(ResourceIndexBuffer.Indices.Num() % 3 == 0);

	//add an entry for each element in the resource
	ElementRanges.AddZeroed(RenderData.Elements.Num());

	INT DestIndex = 0;
	//using the dynamic index buffer method, pack the component's index buffer so that each element can be rendered with one draw call
	//update ElementRanges, which store index ranges for each element taking into account hidden fragments
	//@todo - support !bUseDynamicIndexBuffer
	checkSlow(bUseDynamicIndexBuffer);
	for (INT ElementIndex = 0; ElementIndex < RenderData.Elements.Num(); ElementIndex++)
	{
		const FStaticMeshElement& CurrentElement = RenderData.Elements(ElementIndex);
		//add a range for each chunk whose bone transforms can fit into constant register space at one time
		for (INT ChunkIndex = 0; ChunkIndex < (CurrentElement.Fragments.Num() - 1) / MAX_GPUSKIN_BONES + 1; ChunkIndex++)
		{
			FFracturedElementRange NewFragmentRange;
			NewFragmentRange.BaseIndex = ResourceIndexBuffer.Indices.Num();
			NewFragmentRange.NumPrimitives = 0;
			const INT MinFragmentIndex = ChunkIndex * MAX_GPUSKIN_BONES;
			const INT MaxFragmentIndex = Min<INT>((ChunkIndex + 1) * MAX_GPUSKIN_BONES, CurrentElement.Fragments.Num());
			for (INT FragmentIndex = MinFragmentIndex; FragmentIndex < MaxFragmentIndex; FragmentIndex++)
			{
				if (Component->VisibleFragments(FragmentIndex) != 0)
				{
					FFragmentRange CurrentFragment = CurrentElement.Fragments(FragmentIndex);
					NewFragmentRange.BaseIndex = Min(NewFragmentRange.BaseIndex, DestIndex);
					NewFragmentRange.NumPrimitives += CurrentFragment.NumPrimitives;
					DestIndex += CurrentFragment.NumPrimitives * 3;
				}
			}
			//add a range for every MAX_GPUSKIN_BONES fragments
			ElementRanges(ElementIndex).AddItem(NewFragmentRange);
		}
	}
}

void FFracturedSkinnedMeshSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	// Determine the DPG the primitive should be drawn in for this view.
	if (GetDepthPriorityGroup(View) == DPGIndex)
	{
		const INT LODIndex = 0;
		const FStaticMeshRenderData& LODModel = FracturedStaticMesh->LODModels(LODIndex);

		if( !FracturedStaticMesh->InfluenceVertexBuffer || 
			!IsValidRef(FracturedStaticMesh->InfluenceVertexBuffer->VertexBufferRHI) )
		{
			debugf(TEXT("FFracturedSkinnedMeshSceneProxy: invalid influence buffer for %s"),
				*FracturedStaticMesh->GetPathName());
			return;
		}

		// Draw the static mesh elements.
		for(INT ElementIndex = 0;ElementIndex < LODModel.Elements.Num();ElementIndex++)
		{
			const FStaticMeshElement& Element = LODModel.Elements(ElementIndex);

			FMeshBatch MeshElement;
			FMeshBatchElement& BatchElement = MeshElement.Elements(0);

			//override the resource's index buffer with the fractured mesh component's index buffer
			checkSlow(bUseDynamicIndexBuffer);
			const FRawIndexBuffer& FracturedIndexBuffer = ComponentBaseResources->InstanceIndexBuffer;
			BatchElement.IndexBuffer = &FracturedIndexBuffer;
			check(ElementRanges(ElementIndex).Num() == ComponentSkinResources->ChunkVertexFactories.Num());

			MeshElement.DynamicVertexData = NULL;
			MeshElement.MaterialRenderProxy = LODs(LODIndex).Elements(ElementIndex).Material->GetRenderProxy(bSelected, bHovered);
			MeshElement.LCI = &LODs(LODIndex);

			//@todo - VelocityRendering depends on LocalToWorld changing between frames
#if QUAT_SKINNING
			BatchElement.LocalToWorld = LocalToWorld;
			BatchElement.WorldToLocal = LocalToWorld.Inverse();
#else
			BatchElement.LocalToWorld = FMatrix::Identity;
			BatchElement.WorldToLocal = FMatrix::Identity;
#endif
			BatchElement.MinVertexIndex = Element.MinVertexIndex;
			BatchElement.MaxVertexIndex = Element.MaxVertexIndex;
			MeshElement.UseDynamicData = FALSE;
			MeshElement.ReverseCulling = LocalToWorldDeterminant < 0.0f ? TRUE : FALSE;
			MeshElement.CastShadow = bCastShadow && Element.bEnableShadowCasting;
			MeshElement.Type = PT_TriangleList;
			MeshElement.DepthPriorityGroup = (ESceneDepthPriorityGroup)DPGIndex;
			MeshElement.bUsePreVertexShaderCulling = FALSE;
			MeshElement.PlatformMeshData = Element.PlatformData;

			for (INT ChunkIndex = 0; ChunkIndex < ElementRanges(ElementIndex).Num(); ChunkIndex++)
			{
				FFracturedElementRange ElementRange = ElementRanges(ElementIndex)(ChunkIndex);
				BatchElement.FirstIndex = ElementRange.BaseIndex;
				BatchElement.NumPrimitives = ElementRange.NumPrimitives;

				if (ElementRange.NumPrimitives > 0)
				{
					MeshElement.VertexFactory = &ComponentSkinResources->ChunkVertexFactories(ChunkIndex);

					DrawRichMesh(
						PDI,
						MeshElement,
						WireframeColor,
						FLinearColor::White,
						PropertyColor,
						PrimitiveSceneInfo,
						bSelected
						);
				}
			}
		}
	}
}



/**
 * Called after all objects referenced by this object have been serialized. Order of PostLoad routed to 
 * multiple objects loaded in one set is not deterministic though ConditionalPostLoad can be forced to
 * ensure an object has been "PostLoad"ed.
 */
void UFracturedBaseComponent::PostLoad()
{
	// Call parent implementation
	Super::PostLoad();
}



void UFracturedBaseComponent::InitResources()
{
	//allocate the component index buffer if necessary
	if (!ComponentBaseResources && bUseDynamicIndexBuffer && StaticMesh)
	{
		ComponentBaseResources = new FFracturedBaseResources();
		//force a rebuild of the index buffer on the next attach
		bVisibilityHasChanged = TRUE;
		//enqueue a rendering command to initialize the index buffer
		BeginInitResource(&ComponentBaseResources->InstanceIndexBuffer);
	}
}

void UFracturedBaseComponent::ReleaseResources()
{
	DEC_DWORD_STAT_BY(STAT_FracturedMeshIndexMemory, ComponentBaseResources ? ComponentBaseResources->InstanceIndexBuffer.Indices.GetAllocatedSize() : 0);
	ReleaseBaseResources();
}

void UFracturedBaseComponent::ReleaseBaseResources()
{
	if (ComponentBaseResources)
	{
		//enqueue a rendering command to release the index buffer
		BeginReleaseResource(&ComponentBaseResources->InstanceIndexBuffer);
		//enqueue the deletion of ComponentBaseResources
		//this ensures that ComponentBaseResources is deleted after the rendering thread has processed the release command
		BeginCleanup(ComponentBaseResources);
		//set ComponentBaseResources to NULL which indicates to the game thread that it is not initialized
		ComponentBaseResources = NULL;
		ReleaseResourcesFence.BeginFence();
	}
}

/** Specifies an index range that needs to be copied */
struct FIndexCopyRange
{
	INT SourceOffset;
	INT DestOffset;
	INT NumIndices;
};

/** Copies ranges specified in CopyRanges from ResourceIndexBuffer to ComponentIndexBuffer, and initializes ComponentIndexBuffer. */
static void UpdateComponentIndexBuffer_RenderThread(const FRawStaticIndexBuffer* ResourceIndexBuffer, FRawIndexBuffer* ComponentIndexBuffer, const TArray<FIndexCopyRange>& CopyRanges)
{
	//release the existing index buffer
	//this could be a Lock/Unlock except the size of the index buffer will change based on how many fragments are visible
	ComponentIndexBuffer->ReleaseResource();

	INT NumInstanceIndices = 0;
	//calculate total number of indices to be copied
	for (INT RangeIndex = 0; RangeIndex < CopyRanges.Num(); RangeIndex++)
	{
		const FIndexCopyRange& CurrentRange = CopyRanges(RangeIndex);
		NumInstanceIndices += CurrentRange.NumIndices;
	}

	DEC_DWORD_STAT_BY(STAT_FracturedMeshIndexMemory, ComponentIndexBuffer->Indices.GetAllocatedSize());
	ComponentIndexBuffer->Indices.Empty(NumInstanceIndices);
	ComponentIndexBuffer->Indices.Add(NumInstanceIndices);
	INC_DWORD_STAT_BY(STAT_FracturedMeshIndexMemory, ComponentIndexBuffer->Indices.GetAllocatedSize());

	checkSlow(ResourceIndexBuffer->Indices.GetTypeSize() == ComponentIndexBuffer->Indices.GetTypeSize());
	WORD* SourceIndices = (WORD*)ResourceIndexBuffer->Indices.GetResourceData();
	WORD* DestIndices = (WORD*)ComponentIndexBuffer->Indices.GetData();

	//copy the specified ranges from the resource index buffer to the component's index buffer
	for (INT RangeIndex = 0; RangeIndex < CopyRanges.Num(); RangeIndex++)
	{
		const FIndexCopyRange& CurrentRange = CopyRanges(RangeIndex);
		appMemcpy(DestIndices + CurrentRange.DestOffset, SourceIndices + CurrentRange.SourceOffset, CurrentRange.NumIndices * ResourceIndexBuffer->Indices.GetTypeSize());
	}

	ComponentIndexBuffer->InitResource();
}

/** 
* Determine if the mesh currently has any hidden fragments
* @return TRUE if >0 hidden fragments
*/
UBOOL UFracturedBaseComponent::HasHiddenFragments() const
{
	UBOOL bAnyFragmentsHidden = FALSE;
	for (INT FragmentIndex = 0; FragmentIndex < VisibleFragments.Num(); FragmentIndex++)
	{
		if (VisibleFragments(FragmentIndex) == 0)
		{
			bAnyFragmentsHidden = TRUE;
			break;
		}
	}
	return bAnyFragmentsHidden;
}

/** Enqueues a rendering command to update the component's dynamic index buffer. */
void UFracturedBaseComponent::UpdateComponentIndexBuffer()
{
	//only update if the component is using a dynamic index buffer
	if (StaticMesh && bUseDynamicIndexBuffer && (appGetPlatformType() & UE3::PLATFORM_WindowsServer) == 0)
	{
		UFracturedStaticMesh* FracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);
		
		//verify assumptions
		check(!IsAttached());
		check(VisibleFragments.Num() == FracturedStaticMesh->GetNumFragments());
		check(FracturedStaticMesh->LODModels.Num() > 0);
		//only handling LOD0 for now
		const FStaticMeshRenderData& RenderData = FracturedStaticMesh->LODModels(0);
		const FRawStaticIndexBuffer& ResourceIndexBuffer = RenderData.IndexBuffer;
		check(RenderData.Elements.Num() > 0);

		//only update if dirty or the number of resource indices is different from the last time we built the instance index buffer
		if (bVisibilityHasChanged || NumResourceIndices != ResourceIndexBuffer.Indices.Num())
		{
			const INT InteriorElementIndex = FracturedStaticMesh->GetInteriorElementIndex();
			const INT CoreFragmentIndex = FracturedStaticMesh->GetCoreFragmentIndex();

			//check if any fragments are hidden
			UBOOL bAnyFragmentsHidden = HasHiddenFragments();

			//store the number of resource indices so we can regenerate when it changes
			NumResourceIndices = ResourceIndexBuffer.Indices.Num();
			//make sure the index buffer is setup for triangle lists
			check(ResourceIndexBuffer.Indices.Num() % 3 == 0);
			//make sure that the index buffer can be accessed by the CPU
			check(ResourceIndexBuffer.Indices.GetAllowCPUAccess());

			//@todo - what should this be presized to?
			TArray<FIndexCopyRange> CopyRanges;

			INT DestIndex = 0;

			//gather index ranges that need to be copied from the resource to this component's index buffer
			for (INT ElementIndex = 0; ElementIndex < RenderData.Elements.Num(); ElementIndex++)
			{
				const FStaticMeshElement& CurrentElement = RenderData.Elements(ElementIndex);
				for (INT FragmentIndex = 0; FragmentIndex < CurrentElement.Fragments.Num(); FragmentIndex++)
				{
					if (IsElementFragmentVisible(ElementIndex, FragmentIndex, InteriorElementIndex, CoreFragmentIndex, bAnyFragmentsHidden))
					{
						//copy only the indices from visible fragments
						FFragmentRange CurrentFragment = CurrentElement.Fragments(FragmentIndex);
						FIndexCopyRange CopyRange;
						CopyRange.SourceOffset = CurrentFragment.BaseIndex;
						CopyRange.DestOffset = DestIndex;
						CopyRange.NumIndices = CurrentFragment.NumPrimitives * 3;
						CopyRanges.AddItem(CopyRange);
						DestIndex += CurrentFragment.NumPrimitives * 3;
					}
				}
			}

			// Enqueue a rendering command to handle releasing the index buffer, modifying it and recreating it
			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				UpdateComponentIndexBuffer,
				const FRawStaticIndexBuffer*, ResourceIndexBuffer, &RenderData.IndexBuffer,
				FRawIndexBuffer*, ComponentIndexBuffer, &ComponentBaseResources->InstanceIndexBuffer,
				TArray<FIndexCopyRange>, CopyRanges, CopyRanges,
			{
				UpdateComponentIndexBuffer_RenderThread(ResourceIndexBuffer, ComponentIndexBuffer, CopyRanges);
			});
		}
	}

	bVisibilityHasChanged = FALSE;
}

FPrimitiveSceneProxy* UFracturedStaticMeshComponent::CreateSceneProxy()
{
	//don't create the proxy if using skinned rendering, because SkinnedComponent will handle rendering
	if (!bUseSkinnedRendering && StaticMesh && StaticMesh->IsA(UFracturedStaticMesh::StaticClass()))
	{
		FPrimitiveSceneProxy* Proxy = new FFracturedStaticMeshSceneProxy(this);
#if WITH_EDITOR
		if (GIsEditor && Proxy)
		{
			SetupLightmapResolutionViewInfo(*Proxy);
		}
#endif
		return Proxy;
	}
	else
	{
		return NULL;
	}
}

FPrimitiveSceneProxy* UFracturedSkinnedMeshComponent::CreateSceneProxy()
{
	if (StaticMesh && StaticMesh->IsA(UFracturedStaticMesh::StaticClass()))
	{
		FPrimitiveSceneProxy* Proxy = new FFracturedSkinnedMeshSceneProxy(this);
#if WITH_EDITOR
		if (GIsEditor && Proxy)
		{
			SetupLightmapResolutionViewInfo(*Proxy);
		}
#endif
		return Proxy;
	}
	else
	{
		return NULL;
	}
}

UBOOL UFracturedSkinnedMeshComponent::ShouldRecreateProxyOnUpdateTransform() const
{
	return FALSE;
}


static void InitVertexFactory_RenderThread(FFracturedSkinResources* ComponentResources, FStaticMeshRenderData* ResourceRenderData, FBoneInfluenceVertexBuffer* InfluenceVertexBuffer)
{
	for (INT VFIndex = 0; VFIndex < ComponentResources->ChunkVertexFactories.Num(); VFIndex++)
	{
		//@todo - using FGPUSkinVertexFactory when really we only need 1 bone skinning, and no influence weight vertex input
		FGPUSkinVertexFactory::DataType Data;

		// position - use the same vertex buffer that LocalVertexFactory uses
		Data.PositionComponent = FVertexStreamComponent(
			&ResourceRenderData->PositionVertexBuffer,STRUCT_OFFSET(FPositionVertex,Position),ResourceRenderData->PositionVertexBuffer.GetStride(),VET_Float3);

		// tangents - use the same vertex buffer that LocalVertexFactory uses
		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&ResourceRenderData->VertexBuffer,STRUCT_OFFSET(FStaticMeshFullVertex,TangentX),ResourceRenderData->VertexBuffer.GetStride(),VET_PackedNormal);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&ResourceRenderData->VertexBuffer,STRUCT_OFFSET(FStaticMeshFullVertex,TangentZ),ResourceRenderData->VertexBuffer.GetStride(),VET_PackedNormal);

		// bone indices - use the specially created InfluenceVertexBuffer which stores the affecting fragment index for each vertex
		Data.BoneIndices = FVertexStreamComponent(
			InfluenceVertexBuffer,0,sizeof(FInfluenceBones),VET_UByte4);

		// bone weights - using a stride of 0 to use the same weights for every vertex
		// this wastes vertex bandwidth but avoids needing another vertex factory
		Data.BoneWeights = FVertexStreamComponent(
			&GDummyWeightsVertexBuffer,0,0,VET_UByte4N);

		// uvs
		if (!ResourceRenderData->VertexBuffer.GetUseFullPrecisionUVs())
		{
			Data.TextureCoordinates.AddItem(FVertexStreamComponent(
				&ResourceRenderData->VertexBuffer,STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>,UVs),ResourceRenderData->VertexBuffer.GetStride(),VET_Half2));
		}
		else
		{	
			Data.TextureCoordinates.AddItem(FVertexStreamComponent(
				&ResourceRenderData->VertexBuffer,STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>,UVs),ResourceRenderData->VertexBuffer.GetStride(),VET_Float2));
		}

		ComponentResources->ChunkVertexFactories(VFIndex).SetData(Data);

		ComponentResources->ChunkVertexFactories(VFIndex).GetShaderData().MeshOrigin = FVector(0,0,0);
		ComponentResources->ChunkVertexFactories(VFIndex).GetShaderData().MeshExtension = FVector(1,1,1);
	}
}

void UFracturedSkinnedMeshComponent::InitResources()
{
	Super::InitResources();

	//initialize if necessary
	if (!ComponentSkinResources && StaticMesh)
	{
		check(StaticMesh->LODModels.Num() > 0);
		UFracturedStaticMesh* FracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);
		const UINT NumSourceFragments = FracturedStaticMesh->GetNumFragments();
		ComponentSkinResources = new FFracturedSkinResources(NumSourceFragments);

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			InitFractureSkinVertexFactory,
			FFracturedSkinResources*,ComponentSkinResources,ComponentSkinResources,
			FStaticMeshRenderData*,ResourceRenderData,&StaticMesh->LODModels(0),
			FBoneInfluenceVertexBuffer*,InfluenceVertexBuffer,FracturedStaticMesh->InfluenceVertexBuffer,
		{
			InitVertexFactory_RenderThread(ComponentSkinResources, ResourceRenderData, InfluenceVertexBuffer);
		});

		for (INT VFIndex = 0; VFIndex < ComponentSkinResources->ChunkVertexFactories.Num(); VFIndex++)
		{
			BeginInitResource(&ComponentSkinResources->ChunkVertexFactories(VFIndex));
		}
	}
}

void UFracturedSkinnedMeshComponent::ReleaseResources()
{
	ReleaseSkinResources();
	ReleaseBaseResources();
}

void UFracturedSkinnedMeshComponent::ReleaseSkinResources()
{
	if (ComponentSkinResources)
	{
		//enqueue rendering commands to release the vertex factories
		for (INT VFIndex = 0; VFIndex < ComponentSkinResources->ChunkVertexFactories.Num(); VFIndex++)
		{
			BeginReleaseResource(&ComponentSkinResources->ChunkVertexFactories(VFIndex));
		}
		//enqueue the deletion of ComponentSkinResources
		//this ensures that ComponentSkinResources is deleted after the rendering thread has processed the release command
		BeginCleanup(ComponentSkinResources);
		//set ComponentSkinResources to NULL which indicates to the game thread that it is not initialized
		ComponentSkinResources = NULL;
		//start a fence on the render thread to track the process of the release command
		ReleaseResourcesFence.BeginFence();
	}
}


/** Static: Updates the GPU with bone matrices for this skinned fractured mesh */
void UFracturedSkinnedMeshComponent::UpdateDynamicBoneData_RenderThread(FFracturedSkinResources* ComponentSkinResources, const TArray<FMatrix>& FragmentTransforms)
{
	for (INT VFIndex = 0; VFIndex < ComponentSkinResources->ChunkVertexFactories.Num(); VFIndex++)
	{
		FGPUSkinVertexFactory::ShaderDataType& ShaderData = ComponentSkinResources->ChunkVertexFactories(VFIndex).GetShaderData();
		const INT BoneOffset = VFIndex * MAX_GPUSKIN_BONES;
		const INT CurrentNumBones = Min<INT>(FragmentTransforms.Num() - BoneOffset, MAX_GPUSKIN_BONES);
		ShaderData.BoneMatrices.Empty(CurrentNumBones);
		ShaderData.BoneMatrices.Add(CurrentNumBones);

#if QUAT_SKINNING
		ShaderData.BoneScales.Empty(CurrentNumBones);
		ShaderData.BoneScales.Add(CurrentNumBones);
#endif
		//set the bone matrices which will be used to transform each fragment
		for (INT BoneIndex = 0; BoneIndex < CurrentNumBones; BoneIndex++)
		{
#if QUAT_SKINNING
			FBoneSkinning& BoneMatrix = ShaderData.BoneMatrices(BoneIndex);
			// WARN - DQ skinning can't support non-uniform rendering
			// We need error/warning to prevent that
			FBoneAtom FragTransform;
			FragTransform.SetMatrix(FragmentTransforms(BoneIndex + BoneOffset));
			SET_BONE_DATA(BoneMatrix, FragTransform);
			FBoneScale& BoneScale = ShaderData.BoneScales(BoneIndex);
			BoneScale = FragTransform.GetScale();
#else
			FBoneSkinning& BoneMatrix = ShaderData.BoneMatrices(BoneIndex);
			SET_BONE_DATA(BoneMatrix, FragmentTransforms(BoneIndex + BoneOffset));
#endif
		}
	}
}

void UFracturedSkinnedMeshComponent::UpdateTransform()
{
	Super::UpdateTransform();

	// Make sure bone matrices are up to date
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			SkinnedComponentUpdateDataCommand,
			FFracturedSkinResources*, ComponentSkinResources, ComponentSkinResources,
			TArray<FMatrix>, FragmentTransforms, FragmentTransforms,
			{
				UpdateDynamicBoneData_RenderThread(ComponentSkinResources, FragmentTransforms);
			}
		);

		bFragmentTransformsChanged = FALSE;
	}
}

