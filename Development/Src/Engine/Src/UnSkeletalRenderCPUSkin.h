/*=============================================================================
	UnSkeletalRenderCPUSkin.h: CPU skinned mesh object and resource definitions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_SKELETALRENDERCPUSKIN
#define _INC_SKELETALRENDERCPUSKIN

#include "UnSkeletalRender.h"
#include "SkelMeshDecalVertexFactory.h"
#include "UnDecalRenderData.h"
#include "LocalDecalVertexFactory.h"
#include "GPUSkinVertexFactory.h"


#if XBOX
#define USING_COOKED_DATA 1
#elif PS3
#define USING_COOKED_DATA 1
#elif WIIU
#define USING_COOKED_DATA 1
#else
#define USING_COOKED_DATA 0
#endif


#if USING_COOKED_DATA
// BYTE[] elements in LOD.VertexBufferGPUSkin have been swapped for VET_UBYTE4 vertex stream use
#define INFLUENCE_0		3
#define INFLUENCE_1		2
#define INFLUENCE_2		1
#define INFLUENCE_3		0
#else
#define INFLUENCE_0		0
#define INFLUENCE_1		1
#define INFLUENCE_2		2
#define INFLUENCE_3		3
#endif

/** data for a single skinned skeletal mesh vertex */
struct FFinalSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX;
	FPackedNormal	TangentZ;
	FLOAT			U;
	FLOAT			V;
};

/**
 * Skeletal mesh vertices which have been skinned to their final positions 
 */
class FFinalSkinVertexBuffer : public FVertexBuffer
{
public:

	/** 
	 * Constructor
	 * @param	InSkelMesh - parent mesh containing the static model data for each LOD
	 * @param	InLODIdx - index of LOD model to use from the parent mesh
	 */
	FFinalSkinVertexBuffer(USkeletalMesh* InSkelMesh, INT InLODIdx)
	:	LODIdx(InLODIdx)
	,	SkelMesh(InSkelMesh)
	{
		check(SkelMesh);
		check(SkelMesh->LODModels.IsValidIndex(LODIdx));
	}
	/** 
	 * Initialize the dynamic RHI for this rendering resource 
	 */
	virtual void InitDynamicRHI();

	/** 
	 * Release the dynamic RHI for this rendering resource 
	 */
	virtual void ReleaseDynamicRHI();

	/** 
	 * Cpu skinned vertex name 
	 */
	virtual FString GetFriendlyName() const { return TEXT("CPU skinned mesh vertices"); }

	/**
	 * Get Resource Size : mostly copied from InitDynamicRHI - how much they allocate when initialize
	 */
	virtual UINT GetResourceSize()
	{
		UINT Size;
		// all the vertex data for a single LOD of the skel mesh
		FStaticLODModel& LodModel = SkelMesh->LODModels(LODIdx);

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

		return Size;
	}

private:
	/** index to the SkelMesh.LODModels */
	INT	LODIdx;
	/** parent mesh containing the source data */
	USkeletalMesh* SkelMesh;
};

/**
 * Dynamic triangle indices associated with the skeletal mesh. For example used for torn cloth.
 */
class FFinalDynamicIndexBuffer : public FIndexBuffer
{
public:

	/** 
	 * Constructor
	 * @param	InSkelMesh - parent mesh containing the static model data for each LOD
	 * @param	InLODIdx - index of LOD model to use from the parent mesh
	 */
	FFinalDynamicIndexBuffer(USkeletalMesh* InSkelMesh, INT InLODIdx)
	:	LODIdx(InLODIdx)
	,	SkelMesh(InSkelMesh)
	,	IndexBufferSize(sizeof(WORD))
	{
		check(SkelMesh);
		check(SkelMesh->LODModels.IsValidIndex(LODIdx));
	}
	/** 
	 * Initialize the dynamic RHI for this rendering resource 
	 */
	virtual void InitDynamicRHI();

	/** 
	 * Release the dynamic RHI for this rendering resource 
	 */
	virtual void ReleaseDynamicRHI();

	/** 
	 * Cpu skinned vertex name 
	 */
	virtual FString GetFriendlyName() const { return TEXT("CPU skinned dynamic indices"); }

	/**
	 * Get Resource Size : mostly copied from InitDynamicRHI - how much they allocate when initialize
	 */
	virtual UINT GetResourceSize()
	{
		// all the vertex data for a single LOD of the skel mesh
		FStaticLODModel& LodModel = SkelMesh->LODModels(LODIdx);

		// Create the buffer rendering resource
		UINT Size = LodModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();

		if(!SkelMesh->bEnableClothTearing || Size == 0 || (SkelMesh->ClothWeldingMap.Num() != 0))
		{
			return 0;
		}

		return Size * IndexBufferSize;
	}

	BYTE GetIndexBufferSize() const {return IndexBufferSize;}
private:
	/** index to the SkelMesh.LODModels */
	INT	LODIdx;
	/** parent mesh containing the source data */
	USkeletalMesh* SkelMesh;
	/** Size of the index buffer index type in bytes*/
	BYTE IndexBufferSize;
};

/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicSkelMeshObjectDataCPUSkin : public FDynamicSkelMeshObjectData
{
public:

	/**
	* Constructor
	* Updates the ReferenceToLocal matrices using the new dynamic data.
	* @param	InSkelMeshComponent - parent skel mesh component
	* @param	InLODIndex - each lod has its own bone map 
	* @param	ActiveMorphs - morph targets to blend with during skinning
	*/
	FDynamicSkelMeshObjectDataCPUSkin(
		USkeletalMeshComponent* InSkelMeshComponent,
		INT InLODIndex,
		const TArray<FActiveMorph>& InActiveMorphs,
		const TArray<WORD>* DecalRequiredBoneIndices
		);

	/** ref pose to local space transforms */
	TArray<FBoneAtom> ReferenceToLocal;

	/** origin and direction vectors for TRISORT_CustomLeftRight sections */
	TArray<FTwoVectors> CustomLeftRightVectors;

#if !FINAL_RELEASE 
	/** component space bone transforms*/
	TArray<FBoneAtom> MeshSpaceBases;
#endif
	/** currently LOD for bones being updated */
	INT LODIndex;
	/** morph targets to blend when skinning verts */
	TArray<FActiveMorph> ActiveMorphs;

#if !NX_DISABLE_CLOTH
	/** Component's transform. Just used when updating cloth vertices (have to be transformed from world to local space) */
	FMatrix	WorldToLocal;
	/** Array of cloth mesh vertex positions, passed from game thread (where simulation happens) to rendering thread (to be drawn). */
	TArray<FVector> ClothPosData;
	/** Array of cloth mesh normals, also passed from game to rendering thread. */
	TArray<FVector> ClothNormalData;
	/** Amount to blend in cloth */
	FLOAT ClothBlendWeight;

	/** Array of cloth indices for tangent calculations. */
	TArray<INT> ClothIndexData;
	
	/** Number of vertices returned by the physics SDK. Can differ from the array size due to tearing */
	INT ActualClothPosDataNum;

	TArray<INT> ClothParentIndices;

	UBOOL bRemoveNonSimulatedTriangles;

#endif

#if !NX_DISABLE_SOFTBODY

	TArray<FVector> SoftBodyTetraPosData;

#endif //!NX_DISABLE_SOFTBODY

	/**
	* Returns the size of memory allocated by render data
	*/
	virtual INT GetResourceSize()
 	{
		INT ResourceSize = sizeof(*this);
 		
 		ResourceSize += ReferenceToLocal.GetAllocatedSize();
 		ResourceSize += ActiveMorphs.GetAllocatedSize();
 		
 		#if !NX_DISABLE_CLOTH
 			ResourceSize += ClothPosData.GetAllocatedSize();
 			ResourceSize += ClothNormalData.GetAllocatedSize();
 			ResourceSize += ClothIndexData.GetAllocatedSize();
 			ResourceSize += ClothParentIndices.GetAllocatedSize();
 		#endif
 		
 		#if !NX_DISABLE_SOFTBODY
 			ResourceSize += SoftBodyTetraPosData.GetAllocatedSize();
 		#endif

		return ResourceSize;
 	}
};

/**
 * Render data for a CPU skinned mesh
 */
class FSkeletalMeshObjectCPUSkin : public FSkeletalMeshObject
{
public:

	/** 
	 * Constructor
	 * @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render 
	 */
	FSkeletalMeshObjectCPUSkin(USkeletalMeshComponent* InSkeletalMeshComponent);

	/** 
	 * Destructor 
	 */
	virtual ~FSkeletalMeshObjectCPUSkin();

	/** 
	 * Initialize rendering resources for each LOD 
	 */
	virtual void InitResources();
	
	/** 
	 * Release rendering resources for each LOD 
	 */
	virtual void ReleaseResources();

	/**
	 * Called by the game thread for any dynamic data updates for this skel mesh object
	 * @param	LODIndex - lod level to update
	 * @param	InSkeletalMeshComponen - parent prim component doing the updating
	 * @param	ActiveMorphs - morph targets to blend with during skinning
	 */
	virtual void Update(INT LODIndex,USkeletalMeshComponent* InSkeletalMeshComponent,const TArray<FActiveMorph>& ActiveMorphs);

	/**
	 * Called by the rendering thread to update the current dynamic data
	 * @param	InDynamicData - data that was created by the game thread for use by the rendering thread
	 */
	virtual void UpdateDynamicData_RenderThread(FDynamicSkelMeshObjectData* InDynamicData);

	/** 
	 * Called by the game thread to toggle usage for the instanced vertex weights.
	 * @param bEnabled - TRUE to enable the usage of influence weights
	 * @param LODIdx - Index of the influences to toggle
	 */
	virtual void ToggleVertexInfluences(UBOOL bEnabled, INT LODIdx);

	/**
	 * Called by the game thread to update the instanced vertex weights
	 * @param LODIdx - LOD this update is for
	 * @param BonePairs - set of bone pairs used to find vertices that need to have their weights updated
	 * @param bResetInfluences - resets the array of instanced influences using the ones from the base mesh before updating
	 */
	virtual void UpdateVertexInfluences(INT LODIdx,
		const TArray<FBoneIndexPair>& BonePairs,
		UBOOL bResetInfluences);

	/**
	 * Called by the rendering thread to update the current dynamic weight data
	 * @param	InDynamicData - data that was created by the game thread for use by the rendering thread
	 */
	virtual void UpdateVertexInfluences_RenderThread(FDynamicUpdateVertexInfluencesData* InDynamicData);

	/** 
	 * Enable blend weight rendering in the editor
	 * @param bEnabled - turn on or off the rendering mode
	 * @param BonesOfInterest - array of bone indices to capture weights for
	 */
	virtual void EnableBlendWeightRendering(UBOOL bEnabled, const TArray<INT>& InBonesOfInterest);

	/** 
	 * Enable color mode rendering in the editor
	 * @param color mode index - 
	 */
	virtual void EnableColorModeRendering(SkinColorRenderMode ColorIndex);

	/**
	 * Re-skin cached vertices for an LOD and update the vertex buffer. Note that this
	 * function is called from the render thread!
	 * @param	LODIndex - index to LODs
	 * @param	bForce - force update even if LOD index hasn't changed
	 * @param	bUpdateDecalVertices - whether to update the decal vertices
	 */
	virtual void CacheVertices(INT LODIndex, UBOOL bForce, UBOOL bUpdateDecalVertices) const;

	/**
	 * @param	LODIndex - each LOD has its own vertex data
	 * @param	ChunkIdx - not used
	 * @return	vertex factory for rendering the LOD
	 */
	virtual const FVertexFactory* GetVertexFactory(INT LODIndex,INT ChunkIdx) const;

	/**
	 * @return		Vertex factory for rendering the specified decal at the specified LOD.
	 */
	virtual FDecalVertexFactoryBase* GetDecalVertexFactory(INT LODIndex,INT ChunkIdx,const FDecalInteraction* Decal);

	/** 
	 *	Get the array of component-space bone transforms. 
	 *	Not safe to hold this point between frames, because it exists in dynamic data passed from main thread.
	 */
	virtual TArray<FBoneAtom>* GetSpaceBases() const;

	/** Get the LOD to render this mesh at. */
	virtual INT GetLOD() const
	{
		if(DynamicData)
		{
			return DynamicData->LODIndex;
		}
		else
		{
			return 0;
		}
	}

	/** 
	 *	Get the array of tetrahedron vertex positions. 
	 *	Not safe to hold this point between frames, because it exists in dynamic data passed from main thread.
	 */
	virtual TArray<FVector>* GetSoftBodyTetraPosData() const;

	/** 
	* @return TRUE if the primitive component can render decals
	*/
	virtual UBOOL SupportsDecalRendering() const
	{
		//@todo decal - fix detachment and resource cleanup of decals on CPU skinned skeletal meshes
		return FALSE;
	}

	/**
	 * Adds a decal interaction to the primitive.  This is called in the rendering thread by the skeletal mesh proxy's AddDecalInteraction_RenderingThread.
	 */
	virtual void AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction);

	/**
	 * Removes a decal interaction from the primitive.  This is called in the rendering thread by the skeletal mesh proxy's RemoveDecalInteraction_RenderingThread.
	 */
	virtual void RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent);

	/**
	 * Transforms the decal from refpose space to local space, in preparation for application
	 * to post-skinned (ie local space) verts on the GPU.
	 */
	virtual void TransformDecalState(const FDecalState& DecalState, FMatrix& OutDecalMatrix, FVector& OutDecalLocation, FVector2D& OutDecalOffset, FBoneAtom& OutDecalRefToLocal);
	
	virtual const FIndexBuffer* GetDynamicIndexBuffer(INT InLOD) const
	{
		return &(LODs(InLOD).DynamicIndexBuffer);
	}

	/**
	 * Get the origin and direction vectors for TRISORT_CustomLeftRight sections
	 */
	virtual const FTwoVectors& GetCustomLeftRightVectors(INT SectionIndex) const;

	/**
	*	Return true if this does have valid dynamic data to render
	*/
	virtual UBOOL HaveValidDynamicData() 
	{ 
		return ( DynamicData!=NULL ); 
	}

	/**
	* Returns the size of memory allocated by render data
	*/
	virtual INT GetResourceSize()
	{
		INT ResourceSize=sizeof(*this);

		if(DynamicData)
		{
			ResourceSize += DynamicData->GetResourceSize();
		}

		ResourceSize += LODs.GetAllocatedSize(); 

		// include extra data from LOD
		for (INT I=0; I<LODs.Num(); ++I)
		{
			ResourceSize += LODs(I).GetResourceSize();
		}

		ResourceSize += CachedFinalVertices.GetAllocatedSize();
		ResourceSize += CachedClothTangents.GetAllocatedSize();
		ResourceSize += CachedSoftBodyPositions.GetAllocatedSize();
		ResourceSize += CachedSoftBodyNormals.GetAllocatedSize();
		ResourceSize += CachedSoftBodyTangents.GetAllocatedSize();
		ResourceSize += BonesOfInterest.GetAllocatedSize();

		return ResourceSize;
	}
private:
	/** vertex data for rendering a single LOD */
	struct FSkeletalMeshObjectLOD
	{
		FLocalVertexFactory				VertexFactory;
		mutable FFinalSkinVertexBuffer	VertexBuffer;

		mutable FFinalDynamicIndexBuffer DynamicIndexBuffer;

		/** Vertex buffer that stores instanced influence weights and bones CPU Skin Version*/
		struct VertexWeightsBuffer
		{					 
			INT LODIdx;
			USkeletalMesh* SkelMesh;
			TArray<FVertexInfluence> Influences;

			VertexWeightsBuffer(USkeletalMesh* InSkelMesh, INT InLODIdx) : LODIdx(InLODIdx), SkelMesh(InSkelMesh) 
			{}
		};

		mutable VertexWeightsBuffer VertexInfluenceBuffer;

		/** Resources for a decal drawn at a particular LOD. */
		class FDecalLOD
		{
		public:
			const UDecalComponent*		DecalComponent;
			FLocalDecalVertexFactory	DecalVertexFactory;

			FDecalLOD(const UDecalComponent* InDecalComponent=NULL)
				: DecalComponent( InDecalComponent )
			{}

			void InitResources_GameThread(FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD* LODObject);
			void InitResources_RenderingThread(FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectLOD* LODObject);
			void ReleaseResources_GameThread()
			{
				BeginReleaseResource(&DecalVertexFactory);
			}
			void ReleaseResources_RenderingThread()
			{
				DecalVertexFactory.ReleaseResource();
			}
		};
		/** List of decals drawn at this LOD. */
		mutable TArray<FDecalLOD>	Decals;
		/** TRUE if resources for this LOD have already been initialized. */
		UBOOL						bResourcesInitialized;

		FSkeletalMeshObjectLOD(USkeletalMesh* InSkelMesh,INT InLOD)
		:	VertexBuffer(InSkelMesh,InLOD)
		,	DynamicIndexBuffer(InSkelMesh, InLOD)
		,	VertexInfluenceBuffer(InSkelMesh, InLOD)
		,	bResourcesInitialized( FALSE )
		{
		}
		/** 
		 * Init rendering resources for this LOD 
		 */
		void InitResources(UBOOL bUseInstancedVertexWeights);
		/** 
		 * Release rendering resources for this LOD 
		 */
		void ReleaseResources();
		/** 
		 * Update the contents of the vertex buffer with new data
		 * @param	NewVertices - array of new vertex data
		 * @param	Size - size of new vertex data aray 
		 */
		void UpdateFinalSkinVertexBuffer(void* NewVertices, DWORD Size) const;

		/** Creates resources for drawing the specified decal at this LOD. */
		void AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction);

		/** Releases resources for the specified decal at this LOD. */
		void RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent);

		/**
		 * @return		The vertex factory associated with the specified decal at this LOD.
		 */
		FLocalDecalVertexFactory* GetDecalVertexFactory(const UDecalComponent* Decal);

		/**
	 	 * Get Resource Size : return the size of Resource this allocates
	 	 */
		UINT GetResourceSize()
		{
			UINT Size = VertexBuffer.GetResourceSize();
			Size += DynamicIndexBuffer.GetResourceSize();
			Size += sizeof(VertexInfluenceBuffer);
			Size += VertexInfluenceBuffer.Influences.GetAllocatedSize();
			Size += Decals.GetAllocatedSize();

			return Size;
		}
	protected:
		/**
		 * @return		The index into the decal objects list for the specified decal, or INDEX_NONE if none found.
		 */
		INT FindDecalObjectIndex(const UDecalComponent* DecalComponent) const;
	};

    /** Render data for each LOD */
	TArray<struct FSkeletalMeshObjectLOD> LODs;

	/** Data that is updated dynamically and is needed for rendering */
	class FDynamicSkelMeshObjectDataCPUSkin* DynamicData;

 	/** Index of LOD level's vertices that are currently stored in CachedFinalVertices */
 	mutable INT	CachedVertexLOD;

 	/** Cached skinned vertices. Only updated/accessed by the rendering thread */
 	mutable TArray<FFinalSkinVertex> CachedFinalVertices;

 	/** Cached tangent normals. Only updated/accessed by the rendering thread */
 	mutable TArray<FVector> CachedClothTangents;


 	/** Cached vertex positions of the soft-body surface mesh. Only updated/accessed by the rendering thread */
 	mutable TArray<FVector> CachedSoftBodyPositions;

 	/** Cached vertex normals of the soft-body surface mesh. Only updated/accessed by the rendering thread */
 	mutable TArray<FVector> CachedSoftBodyNormals;

 	/** Cached vertex tangents of the soft-body surface mesh. Only updated/accessed by the rendering thread */
 	mutable TArray<FVector> CachedSoftBodyTangents;

	/** Array of bone's to render bone weights for */
	TArray<INT> BonesOfInterest;

	/** Bone weight viewing in editor */
	SkinColorRenderMode RenderColorMode;
};

#if QUAT_SKINNING
const VectorRegister		VECTOR_2222				= { 2.f, 2.f, 2.f, 2.f };
#endif
#endif
