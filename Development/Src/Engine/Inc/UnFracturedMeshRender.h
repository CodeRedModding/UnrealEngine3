/*=============================================================================
	UnFracturedMeshRender.h: Class definitions for fractured mesh rendering.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef UNFRACTUREDMESHRENDER_H
#define UNFRACTUREDMESHRENDER_H

#include "GPUSkinVertexFactory.h"

/** Vertex buffer containing a single FInfluenceWeights item. */
class FDummyWeightsVertexBuffer : public FVertexBuffer
{
public:

	FDummyWeightsVertexBuffer() {}
	virtual void InitRHI();
	virtual void ReleaseRHI();
};

/** Global FDummyWeightsVertexBuffer resource */
extern TGlobalResource<FDummyWeightsVertexBuffer> GDummyWeightsVertexBuffer;

/** Resources used by a UFracturedBaseMeshComponent for rendering. */
class FFracturedBaseResources : public FDeferredCleanupInterface
{
public:

	/* Index buffer used by this component for rendering. */
	FRawIndexBuffer InstanceIndexBuffer;

	// FDeferredCleanupInterface
	virtual void FinishCleanup()
	{
		delete this;
	}
};

/** Resources used by a UFracturedSkinnedMeshComponent for rendering. */
class FFracturedSkinResources : public FDeferredCleanupInterface
{
public:
	
	/* A FGPUSkinVertexFactory for MAX_GPUSKIN_BONES fragments of the skinned component. */
	TArray<FGPUSkinVertexFactory> ChunkVertexFactories;

	/** Per chunk bone matrices array. */
	TArray< TArray<FBoneSkinning>, TInlineAllocator<2> > PerChunkBoneMatricesArray;
#if QUAT_SKINNING
	/** shared ref pose to local space matrices */
	TArray< TArray<FBoneScale>, TInlineAllocator<1> >				PerChunkBoneScalesArray;
#endif

	FFracturedSkinResources(INT NumBones);

	// FDeferredCleanupInterface
	virtual void FinishCleanup()
	{
		delete this;
	}
};

struct FFracturedElementRange
{
	INT BaseIndex;
	INT NumPrimitives;
};

/**
* Base proxy class for fractured rendering.
*/
class FFracturedBaseSceneProxy : public FStaticMeshSceneProxy
{
public:

	/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
	FFracturedBaseSceneProxy(const UFracturedBaseComponent* Component);

protected:

	/** Whether to use the component's index buffer or the resource's, with multiple draw calls. */
	const BITFIELD bUseDynamicIndexBuffer : 1;

	/** An array of index ranges for each element. */
	TArray<TArray<FFracturedElementRange> > ElementRanges;

	/* The fractured static mesh whose resources are shared among multiple fractured components */
	const UFracturedStaticMesh* FracturedStaticMesh;

	/* The component's resources */
	const FFracturedBaseResources* ComponentBaseResources;
};

/**
 * Rendering thread mirror for UFracturedStaticMeshComponent
 */
class FFracturedStaticMeshSceneProxy : public FFracturedBaseSceneProxy
{
public:

	/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
	FFracturedStaticMeshSceneProxy(const UFracturedStaticMeshComponent* Component);

	/**
	* Rebuilds the static mesh elements for decals that have a missing FStaticMesh* entry for their interactions
	* enqued by BuildMissingDecalStaticMeshElements_GameThread on the render thread
	*/
	virtual void BuildMissingDecalStaticMeshElements_RenderThread();

protected:

	/**
	 * Sets IndexBuffer, FirstIndex and NumPrimitives of OutMeshElement.
	 */
	virtual void SetIndexSource(INT LODIndex, INT ElementIndex, INT FragmentIndex, FMeshBatch& OutMeshElement, UBOOL bWireframe, UBOOL bRequiresAdjacencyInformation) const;
};

/**
 * Rendering thread mirror for UFracturedSkinnedMeshComponent
 */
class FFracturedSkinnedMeshSceneProxy : public FFracturedBaseSceneProxy
{
public:

	/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
	FFracturedSkinnedMeshSceneProxy(const UFracturedSkinnedMeshComponent* Component);

	/** 
	 * Draw the scene proxy as a dynamic element
	 *
	 * @param	PDI - draw interface to render to
	 * @param	View - current view
	 * @param	DPGIndex - current depth priority 
	 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, DWORD Flags);

protected:

	/* The component's resources needed for skinning */
	const FFracturedSkinResources* ComponentSkinResources;
};

#endif
