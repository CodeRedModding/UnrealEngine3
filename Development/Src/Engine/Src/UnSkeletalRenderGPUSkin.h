/*=============================================================================
	UnSkeletalRenderGPUSkin.h: GPU skinned mesh object and resource definitions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNSKELETALRENDERGPUSKIN_H__
#define __UNSKELETALRENDERGPUSKIN_H__

#include "UnSkeletalRender.h"
#include "GPUSkinVertexFactory.h" 
#include "LocalDecalVertexFactory.h"

/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicSkelMeshObjectDataGPUSkin : public FDynamicSkelMeshObjectData
{
public:

	/**
	* Constructor
	* Updates the ReferenceToLocal matrices using the new dynamic data.
	* @param	InSkelMeshComponent - parent skel mesh component
	* @param	InLODIndex - each lod has its own bone map 
	* @param	InActiveMorphs - morph targets active for the mesh
	* @param	DecalRequiredBoneIndices - any bones needed to render decals
	*/
	FDynamicSkelMeshObjectDataGPUSkin(
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
	/** current morph targets active on this mesh */
	TArray<FActiveMorph> ActiveMorphs;
	/** number of active morphs with weights > 0 */
	INT NumWeightedActiveMorphs;

	/**
	* Compare the given set of active morph targets with the current list to check if different
	* @param CompareActiveMorphs - array of morph targets to compare
	* @return TRUE if boths sets of active morph targets are equal
	*/
	UBOOL ActiveMorphTargetsEqual( const TArray<FActiveMorph>& CompareActiveMorphs );
	
	/**
	* Returns the size of memory allocated by render data
	*/
	virtual INT GetResourceSize()
 	{
 		INT ResourceSize = sizeof(*this);
 		
 		ResourceSize += ReferenceToLocal.GetAllocatedSize();
 		ResourceSize += ActiveMorphs.GetAllocatedSize();

		return ResourceSize;
 	}
};

/** morph target mesh data for a single vertex delta */
struct FMorphGPUSkinVertex
{
	FVector			DeltaPosition;
	FPackedNormal	DeltaTangentZ;

	FMorphGPUSkinVertex() {};
	
	/** Construct for special case **/
	FMorphGPUSkinVertex(const FVector & InDeltaPosition, const FPackedNormal& InDeltaTangentZ)
	{
		DeltaPosition = InDeltaPosition;
		DeltaTangentZ = InDeltaTangentZ;
	}
};

/**
* Morph target vertices which have been combined into single position/tangentZ deltas
*/
class FMorphVertexBuffer : public FVertexBuffer
{
public:

	/** 
	* Constructor
	* @param	InSkelMesh - parent mesh containing the static model data for each LOD
	* @param	InLODIdx - index of LOD model to use from the parent mesh
	*/
	FMorphVertexBuffer(USkeletalMesh* InSkelMesh, INT InLODIdx)
		:	HasBeenUpdated(FALSE)	
		,	LODIdx(InLODIdx)
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
	* Morph target vertex name 
	*/
	virtual FString GetFriendlyName() const { return TEXT("Morph target mesh vertices"); }

	/**
	 * Get Resource Size : mostly copied from InitDynamicRHI - how much they allocate when initialize
	 */
	UINT GetResourceSize()
	{
		INT ResourceSize = sizeof(*this);
#if !USE_NULL_RHI
#if XBOX
		if ((FXeVertexBuffer*)VertexBufferRHI)
#else
		if (VertexBufferRHI)
#endif
		{
			// LOD of the skel mesh is used to find number of vertices in buffer
			FStaticLODModel& LodModel = SkelMesh->LODModels(LODIdx);

			// Create the buffer rendering resource
			ResourceSize += LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);
		}
#endif
		return ResourceSize;
	}

	/** Has been updated or not by UpdateMorphVertexBuffer**/
	UBOOL HasBeenUpdated;

private:
	/** index to the SkelMesh.LODModels */
	INT	LODIdx;
	/** parent mesh containing the source data */
	USkeletalMesh* SkelMesh;
};

/** Vertex buffer that stores instanced bone influence weights */
class FInfluenceWeightsVertexBuffer : public FVertexBuffer
{
public:

	/** 
	* Constructor
	* @param	InSkelMesh - parent mesh containing the static model data for each LOD
	* @param	InLODIdx - index of LOD model to use from the parent mesh
	*/
	FInfluenceWeightsVertexBuffer(USkeletalMesh* InSkelMesh, INT InLODIdx)
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
	* Morph target vertex name 
	*/
	virtual FString GetFriendlyName() const { return TEXT("Instance skel mesh weight vertices"); }

	/**
	 * Get Resource Size : mostly copied from InitDynamicRHI - how much they allocate when initialize
	 */
	UINT GetResourceSize()
	{
		INT ResourceSize = sizeof(*this);
#if !USE_NULL_RHI
#if XBOX
		if ((FXeVertexBuffer*)VertexBufferRHI)
#else
		if (VertexBufferRHI)
#endif
		{
			// LOD of the skel mesh is used to find number of vertices in buffer
			FStaticLODModel& LodModel = SkelMesh->LODModels(LODIdx);

			// Create the buffer rendering resource
			ResourceSize += LodModel.NumVertices * sizeof(FVertexInfluence);
		}
#endif
		return ResourceSize;
	}
private:
	/** index to the SkelMesh.LODModels */
	INT	LODIdx;
	/** parent mesh containing the source data */
	USkeletalMesh* SkelMesh;
};

// 1 for a single buffer, this creates and releases textures every frame (the driver has to keep the reference and need to defer the release, low memory as it only occupies rendered buffers (up to 3 copies), best Xbox360 method?)
// 2 for double buffering (works well for PC, caused Xbox360 to stall)
// 3 for triple buffering (works well for PC and Xbox360, wastes a bit of memory)
#define PER_BONE_BUFFER_COUNT 2


class FPreviousPerBoneMotionBlur
{
public:
	/** constructor */
	FPreviousPerBoneMotionBlur();

	/** 
	* call from render thread
	* @param TotalTexelCount sum of all chunks bone count
	*/
	void SetTexelSizeAndInitResource(UINT TotalTexelCount);

	/** 
	* call from render thread
	*/
	void ReleaseResources();

	/** Returns the width of the texture in pixels. */
	UINT GetSizeX() const;

	/** Returns the 1 / width of the texture (cached for better performance). */
	FLOAT GetInvSizeX() const;

	/** so we update only during velocity rendering pass */
	UBOOL IsLocked() const;

	/** needed before AppendData() ccan be called */
	void LockData();

	/**
	 * use between LockData() and UnlockData()
	 * @param	DataStart, must not be 0
	 * @param	BoneCount number of FBoneSkinning elements, must not be 0
	 * @return	StartIndex where the data can be referenced in the texture, 0xffffffff if this failed (not enough space in the buffer, will be fixed next frame)
	 */
	UINT AppendData(FBoneSkinning *DataStart, UINT BoneCount);

	/** only call if LockData() */
	void UnlockData();

	/** @return 0 if there should be no bone based motion blur (no previous data available or it's not active) */
	FBoneDataTexture* GetReadData();

private:

	/** Stores the bone information with one frame delay, required for per bone motion blur. Buffered to avoid stalls on texture Lock() */
	FBoneDataTexture PerChunkBoneMatricesTexture[PER_BONE_BUFFER_COUNT];
	/** cycles between the buffers to avoid stalls (when CPU would need to wait on GPU) */
	UINT BufferIndex;
	/** Cached for better performance */
	FLOAT InvSizeX;
	/* !=0 if data is locked, does not change during the lock */
	FLOAT* LockedData;
	/** only valid if LockedData != 0, advances with every Append() */
	UINT LockedTexelPosition;
	/** only valid if LockedData != 0 */
	UINT LockedTexelCount;

	/** @return 0 .. PER_BONE_BUFFER_COUNT-1 */
	UINT GetReadBufferIndex() const;

	/** @return 0 .. PER_BONE_BUFFER_COUNT-1 */
	UINT GetWriteBufferIndex() const;

	/** to cycle the internal buffer counter */
	void AdvanceBufferIndex();
};



/**
 * Render data for a GPU skinned mesh
 */
class FSkeletalMeshObjectGPUSkin : public FSkeletalMeshObject
{
public:

	/** 
	 * Constructor
	 * @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render 
	 */
	FSkeletalMeshObjectGPUSkin(USkeletalMeshComponent* InSkeletalMeshComponent);

	/** 
	 * Destructor 
	 */
	virtual ~FSkeletalMeshObjectGPUSkin();

	/** 
	 * Initialize rendering resources for each LOD 
	 */
	virtual void InitResources();

	/** 
	 * Release rendering resources for each LOD. 
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
	virtual void UpdateVertexInfluences(
		INT LODIdx,
		const TArray<FBoneIndexPair>& BonePairs,
		UBOOL bResetInfluences);

	/**
	* Called by the rendering thread to update the current dynamic weight data
	* @param	InDynamicData - data that was created by the game thread for use by the rendering thread
	*/
	virtual void UpdateVertexInfluences_RenderThread(FDynamicUpdateVertexInfluencesData* InDynamicData);

	/**
	 * @param	LODIndex - each LOD has its own vertex data
	 * @param	ChunkIdx - index for current mesh chunk
	 * @return	vertex factory for rendering the LOD
	 */
	virtual const FVertexFactory* GetVertexFactory(INT LODIndex,INT ChunkIdx) const;

	/**
	 * @param	LODIndex - each LOD has its own vertex data
	 * @param	ChunkIdx - index for current mesh chunk
	 * @return	vertex factory for rendering the LOD
	 */
	virtual FDecalVertexFactoryBase* GetDecalVertexFactory(INT LODIndex,INT ChunkIdx,const FDecalInteraction* Decal);

	virtual void CacheVertices(INT LODIndex, UBOOL bForce, UBOOL bUpdateDecalVertices) const {}

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
		return TRUE;
	}

	/**
	 * Allows derived types to transform decal state into a space that's appropriate for the given skinning algorithm.
	 */
	virtual void TransformDecalState(const FDecalState& DecalState, FMatrix& OutDecalMatrix, FVector& OutDecalLocation, FVector2D& OutDecalOffset, FBoneAtom& OutDecalRefToLocal);

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
 		INT ResourceSize = sizeof(*this);
 		
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

		return ResourceSize;
 	}

	/** 
	 * Vertex buffers that can be used for GPU skinning factories 
	 */
	class FVertexFactoryBuffers
	{
	public:
		FVertexFactoryBuffers()
			: VertexBufferGPUSkin(NULL)
			, ColorVertexBuffer(NULL)
			, MorphVertexBuffer(NULL)
			, AltVertexWeightsBuffer(NULL)
			, InstancedWeightsBuffer(NULL)
		{}

		FSkeletalMeshVertexBuffer* VertexBufferGPUSkin;
		FSkeletalMeshVertexColorBuffer*	ColorVertexBuffer;
		FMorphVertexBuffer* MorphVertexBuffer;
		FSkeletalMeshVertexInfluences* AltVertexWeightsBuffer;
		FInfluenceWeightsVertexBuffer* InstancedWeightsBuffer;
	};

private:

	/**
	 * Vertex factories and their matrix arrays
	 */
	class FVertexFactoryData
	{
	public:
		FVertexFactoryData() {}

		/** one vertex factory for each chunk */
		TIndirectArray<FGPUSkinVertexFactory> VertexFactories;
		TIndirectArray<FGPUSkinDecalVertexFactory> DecalVertexFactories;		

		/** Vertex factory defining both the base mesh as well as the morph delta vertex decals */
		TIndirectArray<FGPUSkinMorphVertexFactory> MorphVertexFactories;
		TIndirectArray<FGPUSkinMorphDecalVertexFactory> MorphDecalVertexFactories;		

		/** shared ref pose to local space matrices */
		TArray< TArray<FBoneSkinning>, TInlineAllocator<1> > PerChunkBoneMatricesArray;
#if QUAT_SKINNING
		/** shared bone scale array for local space matrices */
		TArray< TArray<FBoneScale>, TInlineAllocator<1> > PerChunkBoneScalesArray;
#endif
		/** 
		 * Init default vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 * @param bInitDecals - also init corresponding decal factory
		 */
		void InitVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshChunk>& Chunks, UBOOL bInitDecals, UBOOL bInUsePerBoneMotionBlur);
		/** 
		 * Release default vertex factory resources for this LOD 
		 */
		void ReleaseVertexFactories();
		/** 
		 * Init morph vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 * @param bInitDecals - also init corresponding decal factory
		 */
		void InitMorphVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshChunk>& Chunks, UBOOL bInitDecals, UBOOL bInUsePerBoneMotionBlur);
		/** 
		 * Release morph vertex factory resources for this LOD 
		 */
		void ReleaseMorphVertexFactories();
		/**
		 * Init one array of matrices for each chunk (shared across vertex factory types)
		 *
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitPerChunkBoneMatrices(const TArray<FSkelMeshChunk>& Chunks);
		/**
		 * Clear factory arrays
		 */
		void ClearFactories()
		{
			VertexFactories.Empty();
			DecalVertexFactories.Empty();
			MorphVertexFactories.Empty();
			MorphDecalVertexFactories.Empty();	
		}

		/**
		 * @return memory in bytes of size of the vertex factories and their matrices
		 */
		UINT GetResourceSize()
		{
			UINT Size = 0;
			Size += VertexFactories.GetAllocatedSize();
			Size += DecalVertexFactories.GetAllocatedSize();

			Size += MorphVertexFactories.GetAllocatedSize();
			Size += MorphDecalVertexFactories.GetAllocatedSize();

			Size += PerChunkBoneMatricesArray.GetAllocatedSize();
		#if QUAT_SKINNING
			Size += PerChunkBoneScalesArray.GetAllocatedSize();
		#endif
			return Size;
		}	
	};

	/** vertex data for rendering a single LOD */
	struct FSkeletalMeshObjectLOD
	{
		FSkeletalMeshObjectLOD(USkeletalMesh* InSkelMesh,INT InLOD,UBOOL bInDecalFactoriesEnabled)
		:	SkelMesh(InSkelMesh)
		,	LODIndex(InLOD)
		,	bDecalFactoriesEnabled(bInDecalFactoriesEnabled)
		,	MorphVertexBuffer(InSkelMesh,LODIndex)
		,	WeightsVertexBuffer(InSkelMesh,LODIndex)
		{
		}

		/** 
		 * Init rendering resources for this LOD 
		 * @param bUseLocalVertexFactory - use non-gpu skinned factory when rendering in ref pose
		 * @param MeshLODInfo - information about the state of the bone influence swapping
         * @param bInUsePerBoneMotionBlur - use per-bone motion blur on this object
		 */
		void InitResources(UBOOL bUseLocalVertexFactory, const FSkelMeshObjectLODInfo& MeshLODInfo, UBOOL bInUsePerBoneMotionBlur);		

		/** 
		 * Release rendering resources for this LOD 
		 */
		void ReleaseResources();

		/** 
		 * Init rendering resources for the morph stream of this LOD
		 * @param MeshLODInfo - information about the state of the bone influence swapping
         * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, UBOOL bInUsePerBoneMotionBlur);

		/** 
		 * Release rendering resources for the morph stream of this LOD
		 */
		void ReleaseMorphResources();

		/** 
		 * Update the contents of the vertex buffer with new data. Note that this
		 * function is called from the render thread.
		 * @param	NewVertices - array of new vertex data
		 * @param	NumVertices - Number of vertices
		 */
		void UpdateShadowVertexBuffer( const FVector* NewVertices, UINT NumVertices ) const;

		/**
		 * @return memory in bytes of size of the resources for this LOD
		 */
		UINT GetResourceSize()
		{
			UINT Size = WeightsVertexBuffer.GetResourceSize();
			Size += MorphVertexBuffer.GetResourceSize();

			Size += GPUSkinVertexFactories.GetResourceSize();
			Size += GPUSkinVertexFactoriesAltWeights.GetResourceSize();

			if ( LocalVertexFactory.IsValid() )
			{
				Size += sizeof(LocalVertexFactory);
			}

			if ( LocalDecalVertexFactory.IsValid() )
			{
				Size += sizeof(LocalDecalVertexFactory);
			}
			return Size;
		}		

		USkeletalMesh* SkelMesh;
		INT LODIndex;

		/** TRUE if the owning skeletal mesh data object accepts decals.  If FALSE, DecalVertexFactories are not updated. */
		UBOOL bDecalFactoriesEnabled;

		/** Vertex buffer that stores the morph target vertex deltas. Updated on the CPU */
		FMorphVertexBuffer MorphVertexBuffer;

		/** Vertex buffer that stores instanced influence weights and bones */
		FInfluenceWeightsVertexBuffer WeightsVertexBuffer;

		/** Default GPU skinning vertex factories and matrices */
		FVertexFactoryData GPUSkinVertexFactories;
		/** Alt weighting GPU skinning vertex factories and matrices */
		FVertexFactoryData GPUSkinVertexFactoriesAltWeights;

		/** one vertex factory for each chunk */
		TScopedPointer<FLocalVertexFactory> LocalVertexFactory;
		TScopedPointer<FLocalDecalVertexFactory> LocalDecalVertexFactory;

		/**
		 * Update the contents of the morph target vertex buffer by accumulating all 
		 * delta positions and delta normals from the set of active morph targets
		 * @param ActiveMorphs - morph targets to accumulate. assumed to be weighted and have valid targets
		 */
		void UpdateMorphVertexBuffer( const TArray<FActiveMorph>& ActiveMorphs );

		/**
		 * Determine the current vertex buffers valid for this LOD
		 *
		 * @param OutVertexBuffers output vertex buffers
		 */
		void GetVertexBuffers(FVertexFactoryBuffers& OutVertexBuffers,FStaticLODModel& LODModel,const FSkelMeshObjectLODInfo& MeshLODInfo,UBOOL bAllowAltWeights);
	};

	/** 
	* Initialize morph rendering resources for each LOD 
	*/
	void InitMorphResources(UBOOL bInUsePerBoneMotionBlur);

	/** 
	* Release morph rendering resources for each LOD. 
	*/
	void ReleaseMorphResources();

	/** Render data for each LOD */
	TArray<struct FSkeletalMeshObjectLOD> LODs;

	/** Data that is updated dynamically and is needed for rendering */
	FDynamicSkelMeshObjectDataGPUSkin* DynamicData;

	/** TRUE if the morph resources have been initialized */
	UBOOL bMorphResourcesInitialized;

	/** TRUE if the instanced vertex influence weights have been initialized */
	UBOOL bInfluenceWeightsInitialized;
};

#endif // __UNSKELETALRENDERGPUSKIN_H__
