/*=============================================================================
	UnSkeletalRender.h: Definitions and inline code for rendering SkeletalMeshComponet
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef SKEL_RENDER_HEADER
#define SKEL_RENDER_HEADER

// Forward declarations.
class FSkelMeshDecalVertexFactoryBase;
class FDecalInteraction;
class FDecalState;
class UDecalComponent;
class FIApexClothing;

// smallest blend weight for morph targets
extern const FLOAT MinMorphBlendWeight;
// largest blend weight for morph targets
extern const FLOAT MaxMorphBlendWeight;

enum SkinColorRenderMode;

/**
* Interface for mesh rendering data
*/
class FSkeletalMeshObject : public FDeferredCleanupInterface
{
public:
	FSkeletalMeshObject(USkeletalMeshComponent* InSkeletalMeshComponent) 
	:	MinDesiredLODLevel(0)
	,	MaxDistanceFactor(0.f)
	,	WorkingMinDesiredLODLevel(0)
	,	WorkingMaxDistanceFactor(0.f)
	,   bHasBeenUpdatedAtLeastOnce(FALSE)
	,   ApexClothing( InSkeletalMeshComponent->GetApexClothing() )	
#if WITH_EDITORONLY_DATA
	,   ChunkIndexPreview(InSkeletalMeshComponent->ChunkIndexPreview)
	,   SectionIndexPreview(InSkeletalMeshComponent->SectionIndexPreview)
#endif
	,	SkeletalMesh(InSkeletalMeshComponent->SkeletalMesh)
	,	LastFrameNumber(0)
	,	bDecalFactoriesEnabled(InSkeletalMeshComponent->bAcceptsStaticDecals || InSkeletalMeshComponent->bAcceptsDynamicDecals)
	,	bUseLocalVertexFactory(FALSE)
	,	ProgressiveDrawingFraction(InSkeletalMeshComponent->ProgressiveDrawingFraction)
	,	CustomSortAlternateIndexMode((ECustomSortAlternateIndexMode)InSkeletalMeshComponent->CustomSortAlternateIndexMode)
	,	bUsePerBoneMotionBlur(InSkeletalMeshComponent->bPerBoneMotionBlur)
	{
		check(SkeletalMesh);

#if WITH_EDITORONLY_DATA
		if ( !GIsEditor )
		{
			ChunkIndexPreview = -1;
			SectionIndexPreview = -1;
		}
#endif // #if WITH_EDITORONLY_DATA

		// we want to access GSystemSettings.MotionBlurSkinning
		// !IsInRenderingThread() would not work here
		{
			checkSlow(IsInGameThread());

			INT MotionBlurSkinning = GSystemSettings.MotionBlurSkinning;

			if(MotionBlurSkinning == 0)
			{
				// force per bone motionblur off
				bUsePerBoneMotionBlur = FALSE;
			}
			else if(MotionBlurSkinning == 2)
			{
				// force per bone motionblur on
				bUsePerBoneMotionBlur = TRUE;
			}
		}

		// We want to restore the most recent value of the MaxDistanceFactor the SkeletalMeshComponent
		// cached, which will be 0.0 when first created, and a valid, updated value when recreating
		// this mesh object (e.g. during a component reattach), avoiding issues with a transient
		// assignment of 0.0 and then back to an updated value the next frame.
		MaxDistanceFactor = InSkeletalMeshComponent->MaxDistanceFactor;
		WorkingMaxDistanceFactor = MaxDistanceFactor;

#if !FINAL_RELEASE
		if(GIsUCC)
		{
			bUsePerBoneMotionBlur = FALSE;
		}
#endif //!FINAL_RELEASE

		if(!GSupportsVertexTextureFetch)
		{
			bUsePerBoneMotionBlur = FALSE;
		}

		InitLODInfos(InSkeletalMeshComponent);
	}
	virtual ~FSkeletalMeshObject() {}
	virtual void InitResources() = 0;
	virtual void ReleaseResources() = 0;
	virtual void Update(INT LODIndex,USkeletalMeshComponent* InSkeletalMeshComponent,const TArray<FActiveMorph>& ActiveMorphs) = 0;
	virtual void UpdateDynamicData_RenderThread(class FDynamicSkelMeshObjectData* InDynamicData) = 0;
	/** 
	 * Called by the game thread to toggle usage for the instanced vertex weights.
	 * @param bEnabled - TRUE to enable the usage of influence weights
	 * @param LODIdx - Index of the influences to toggle
	 */
	virtual void ToggleVertexInfluences(UBOOL bEnabled, INT LODIdx) = 0;
	/**
	 * Called by the game thread to update the instanced vertex weights
	 * @param LODIdx - LOD this update is for
	 * @param BonePairs - set of bone pairs used to find vertices that need to have their weights updated
	 * @param bResetInfluences - resets the array of instanced influences using the ones from the base mesh before updating
	 */
	virtual void UpdateVertexInfluences(INT LODIdx,const TArray<FBoneIndexPair>& BonePairs,UBOOL bResetInfluences) = 0;
	virtual void UpdateVertexInfluences_RenderThread(class FDynamicUpdateVertexInfluencesData* InDynamicData) = 0;
	virtual const FVertexFactory* GetVertexFactory(INT LODIndex,INT ChunkIdx) const = 0;
	virtual FDecalVertexFactoryBase* GetDecalVertexFactory(INT LODIndex,INT ChunkIdx,const FDecalInteraction* Decal) = 0;
	virtual void CacheVertices(INT LODIndex, UBOOL bForce, UBOOL bUpdateDecalVertices) const = 0;
	virtual TArray<FBoneAtom>* GetSpaceBases() const = 0;
	virtual INT GetLOD() const = 0;
	virtual TArray<FVector>* GetSoftBodyTetraPosData() const = 0;
	virtual UBOOL SupportsDecalRendering() const = 0;	
	/** 
	 * Enable blend weight rendering in the editor
	 * @param bEnabled - turn on or off the rendering mode
	 * @param BonesOfInterest - array of bone indices to capture weights for
	 */
	virtual void EnableBlendWeightRendering(UBOOL bEnabled, const TArray<INT>& InBonesOfInterest) {}
	
	/** 
	* Enable color mode rendering in the editor
	* @param color mode index - 
	*/
	virtual void EnableColorModeRendering(SkinColorRenderMode ColorIndex) {}

	/**
	 * Returns an index buffer used for storing dynamically generated triangle(indices). 
	 * Only valid for CPU skinned meshes.
	 */
	virtual const FIndexBuffer* GetDynamicIndexBuffer(INT Lod) const { return NULL; }

	/**
	 * Adds a decal interaction to the primitive.  This is called in the rendering thread by the skeletal mesh proxy's AddDecalInteraction_RenderingThread.
	 */
	virtual void AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction) {}

	/**
	 * Removes a decal interaction from the primitive.  This is called in the rendering thread by the skeletal mesh proxy's RemoveDecalInteraction_RenderingThread.
	 */
	virtual void RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent) {}

	/**
	 * Allows derived types to transform decal state into a space that's appropriate for the given skinning algorithm.
	 */
	virtual void TransformDecalState(const FDecalState& Decal, FMatrix& OutDecalMatrix, FVector& OutDecalLocation, FVector2D& OutDecalOffset, FBoneAtom& OutDecalRefToLocal) = 0;

	/** 
	 *	Given a set of views, update the MinDesiredLODLevel member to indicate the minimum (ie best) LOD we would like to use to render this mesh. 
	 *	This is called from the rendering thread (PreRender) so be very careful what you read/write to.
	 */
	void UpdateMinDesiredLODLevel(const FSceneView* View, const FBoxSphereBounds& Bounds, INT FrameNumber);

	/**
	 * Get the origin and direction vectors for TRISORT_CustomLeftRight sections
	 */
	virtual const FTwoVectors& GetCustomLeftRightVectors(INT SectionIndex) const = 0;

	/**
	 *	Return true if this does have valid dynamic data to render
	 */
	virtual UBOOL HaveValidDynamicData() = 0;

	// allow access to mesh component
	friend class FDynamicSkelMeshObjectDataCPUSkin;
	friend class FDynamicSkelMeshObjectDataGPUSkin;
	friend class FSkeletalMeshSceneProxy;
	friend class FSkeletalMeshSectionIter;

	/** @return if per-bone motion blur is enabled for this object. This includes is the system overwrites the skeletal mesh setting. */
	UBOOL ShouldUsePerBoneMotionBlur() const { return bUsePerBoneMotionBlur; }

	// FDeferredCleanupInterface
	virtual void FinishCleanup()
	{
		delete this;
	}
	
	/**
	* Returns the size of memory allocated by render data
	*/
	virtual INT GetResourceSize() = 0;

	/**
     * List of chunks to be rendered based on instance weight usage. Full swap of weights will render with its own chunks.
	 * @return Chunks to iterate over for rendering
	 */
	const TArray<FSkelMeshChunk>& GetRenderChunks(INT InLODIndex) const;

	/**
	 * Update the hidden material section flags for an LOD entry
	 *
	 * @param InLODIndex - LOD entry to update hidden material flags for
	 * @param HiddenMaterials - array of hidden material sections
	 */
	void SetHiddenMaterials(INT InLODIndex,const TArray<UBOOL>& HiddenMaterials);
	
	/**
	 * Determine if the material section entry for an LOD is hidden or not
	 *
	 * @param InLODIndex - LOD entry to get hidden material flags for
	 * @param MaterialIdx - index of the material section to check
	 */
	UBOOL IsMaterialHidden(INT InLODIndex,INT MaterialIdx) const;
	
	/**
	 * Initialize the array of LODInfo based on the settings of the current skel mesh component
	 */
	void InitLODInfos(const USkeletalMeshComponent* SkelComponent);

	/** Setup for rendering a specific LOD entry of the component */
	struct FSkelMeshObjectLODInfo
	{
		/** Hidden Material Section Flags for rendering - That is Material Index, not Section Index  */
		TArray<UBOOL>	HiddenMaterials;
		/** Whether the instance weights are used for a partial/full swap */
		EInstanceWeightUsage InstanceWeightUsage;
		/** Current index into the skeletal mesh VertexInfluences for the current LOD */
		INT InstanceWeightIdx;
		/** 
		 * TRUE if the instanced vertex buffer containing influence weights/bones should be used instead of 
		 * the default weights from the USkeletalMesh vertex buffer.  
		 */
		UBOOL bUseInstancedVertexInfluences;

		FSkelMeshObjectLODInfo() :
			InstanceWeightUsage(IWU_PartialSwap)
			,InstanceWeightIdx(INDEX_NONE)
			,bUseInstancedVertexInfluences(FALSE)
		{}
	};	
	TArray<FSkelMeshObjectLODInfo> LODInfo;

	/** 
	 *	Lowest (best) LOD that was desired for rendering this SkeletalMesh last frame. 
	 *	This should only ever be WRITTEN by the RENDER thread (in FSkeletalMeshProxy::PreRenderView) and READ by the GAME thread (in USkeletalMeshComponent::UpdateSkelPose).
	 */
	INT MinDesiredLODLevel;

	/** 
	 *	High (best) DistanceFactor that was desired for rendering this SkeletalMesh last frame. Represents how big this mesh was in screen space  
	 *	This should only ever be WRITTEN by the RENDER thread (in FSkeletalMeshProxy::PreRenderView) and READ by the GAME thread (in USkeletalMeshComponent::UpdateSkelPose).
	 */
	FLOAT MaxDistanceFactor;

	/** This frames min desired LOD level. This is copied (flipped) to MinDesiredLODLevel at the beginning of the next frame. */
	INT WorkingMinDesiredLODLevel;

	/** This frames max distance factor. This is copied (flipped) to MaxDistanceFactor at the beginning of the next frame. */
	FLOAT WorkingMaxDistanceFactor;

    /** This is set to TRUE when we have sent our Mesh data to the rendering thread at least once as it needs to have have a datastructure created there for each MeshObject **/
	UBOOL           bHasBeenUpdatedAtLeastOnce;

	/** These are bones that need to be transformed because they have a decal attached to them, even if they are not used for skinning in all LODs */
	TArray<WORD>	DecalRequiredBoneIndices;

	/** Pointer to the source skeletal mesh component */
	FIApexClothing *ApexClothing;
	
#if WITH_EDITORONLY_DATA
	/** Index of the chunk to preview... If set to -1, all chunks will be rendered */
	INT				ChunkIndexPreview;
	
	/** Index of the section to preview... If set to -1, all section will be rendered */
	INT				SectionIndexPreview;
#endif

protected:
	USkeletalMesh*	SkeletalMesh;

	/** Used to keep track of the first call to UpdateMinDesiredLODLevel each frame. */
	UINT			LastFrameNumber;

	/** TRUE if decal vertex factories are enabled (because this skeletal mesh can accept decals). */
	UBOOL			bDecalFactoriesEnabled;

	/** TRUE if we are only using local vertex factories for this mesh (when bForceRefpose = TRUE) */
	UBOOL			bUseLocalVertexFactory;

	/** Editor only. Used for visualizing drawing order in Animset Viewer. If < 1.0,
	 * only the specified fraction of triangles will be rendered
	 */
	FLOAT			ProgressiveDrawingFraction;

	/** Use the 2nd copy of indices for separate left/right sort order (when TRISORT_CustomLeftRight) 
	 * Set manually by the AnimSetViewer when editing sort order, or based on viewing angle otherwise.
	 */
	ECustomSortAlternateIndexMode CustomSortAlternateIndexMode;
	/** 
	 *	If TRUE, per-bone motion blur is enabled for this object. This includes is the system overwrites the skeletal mesh setting.
	 */
	UBOOL			bUsePerBoneMotionBlur;
};

/** Dynamic data updates needed by the rendering thread are sent with this */
class FDynamicSkelMeshObjectData
{
public:
	FDynamicSkelMeshObjectData(){}
	virtual ~FDynamicSkelMeshObjectData(){}
};

/** 
* Stores the data for updating instanced weights
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicUpdateVertexInfluencesData
{
public:
	/**
	* Constructor
	*/
	FDynamicUpdateVertexInfluencesData(
		INT InLODIdx,
		const TArray<FBoneIndexPair>& InBonePairs,
		UBOOL InbResetInfluences )
		:	LODIdx(InLODIdx)
		,	BonePairs(InBonePairs)
		,	bResetInfluences(InbResetInfluences)
	{
	}

	/** LOD this update is for */
	INT LODIdx;
	/** set of bone pairs used to find vertices that need to have their weights updated */
	TArray<FBoneIndexPair> BonePairs;
	/** resets the array of instanced weights/bones using the ones from the base mesh defaults before udpating */
	UBOOL bResetInfluences;
};

/**
* Utility function that fills in the array of ref-pose to local-space matrices using 
* the mesh component's updated space bases
* @param	ReferenceToLocal - matrices to update
* @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
* @param	LODIndex - each LOD has its own mapping of bones to update
* @param	ExtraRequiredBoneIndices - any extra bones apart from those active in the LOD that we'd like to update
*/
void UpdateRefToLocalMatrices( TArray<FBoneAtom>& ReferenceToLocal, const USkeletalMeshComponent* SkeletalMeshComponent, INT LODIndex, const TArray<WORD>* ExtraRequiredBoneIndices=NULL );

/**
 * Utility function that calculates the local-space origin and bone direction vectors for the
 * current pose for any TRISORT_CustomLeftRight sections.
 * @param	OutCustomLeftRightVectors - origin and direction vectors to update
 * @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
 * @param	LODIndex - current LOD
 */
void UpdateCustomLeftRightVectors( TArray<FTwoVectors>& OutCustomLeftRightVectors, const USkeletalMeshComponent* SkeletalMeshComponent, INT LODIndex );

extern const VectorRegister		VECTOR_PACK_127_5;
extern const VectorRegister		VECTOR4_PACK_127_5;

extern const VectorRegister		VECTOR_INV_127_5;
extern const VectorRegister		VECTOR4_INV_127_5;

extern const VectorRegister		VECTOR_UNPACK_MINUS_1;
extern const VectorRegister		VECTOR4_UNPACK_MINUS_1;

extern const VectorRegister		VECTOR_0001;

/**
* Apply scale/bias to packed normal byte values and store result in register
* Only first 3 components are copied. W component is always 0
* 
* @param PackedNormal - source vector packed with byte components
* @return vector register with unpacked float values
*/
static FORCEINLINE VectorRegister Unpack3( const DWORD *PackedNormal )
{
#if XBOX
	return VectorMultiplyAdd( VectorLoadByte4Reverse(PackedNormal), VECTOR_INV_127_5, VECTOR_UNPACK_MINUS_1 );
#else
	return VectorMultiplyAdd( VectorLoadByte4(PackedNormal), VECTOR_INV_127_5, VECTOR_UNPACK_MINUS_1 );
#endif
}

/**
* Apply scale/bias to float register values and store results in memory as packed byte values 
* Only first 3 components are copied. W component is always 0
* 
* @param Normal - source vector register with floats
* @param PackedNormal - destination vector packed with byte components
*/
static FORCEINLINE void Pack3( VectorRegister Normal, DWORD *PackedNormal )
{
	Normal = VectorMultiplyAdd(Normal, VECTOR_PACK_127_5, VECTOR_PACK_127_5);
#if XBOX
	// Need to reverse the order on Xbox.
	Normal = VectorSwizzle( Normal, 3, 2, 1, 0 );
#endif
	VectorStoreByte4( Normal, PackedNormal );
}

/**
* Apply scale/bias to packed normal byte values and store result in register
* All 4 components are copied. 
* 
* @param PackedNormal - source vector packed with byte components
* @return vector register with unpacked float values
*/
static FORCEINLINE VectorRegister Unpack4( const DWORD *PackedNormal )
{
#if XBOX
	return VectorMultiplyAdd( VectorLoadByte4Reverse(PackedNormal), VECTOR4_INV_127_5, VECTOR4_UNPACK_MINUS_1 );
#else
	return VectorMultiplyAdd( VectorLoadByte4(PackedNormal), VECTOR4_INV_127_5, VECTOR4_UNPACK_MINUS_1 );
#endif
}

/**
* Apply scale/bias to float register values and store results in memory as packed byte values 
* All 4 components are copied. 
* 
* @param Normal - source vector register with floats
* @param PackedNormal - destination vector packed with byte components
*/
static FORCEINLINE void Pack4( VectorRegister Normal, DWORD *PackedNormal )
{
	Normal = VectorMultiplyAdd(Normal, VECTOR4_PACK_127_5, VECTOR4_PACK_127_5);
#if XBOX
	// Need to reverse the order on Xbox.
	Normal = VectorSwizzle( Normal, 3, 2, 1, 0 );
#endif
	VectorStoreByte4( Normal, PackedNormal );
}

/** 
* Derive the tanget/binormal using the new normal and the base tangent vectors for a vertex 
*/
template<typename VertexType>
FORCEINLINE void RebuildTangentBasis( VertexType& DestVertex )
{
	// derive the new tangent by orthonormalizing the new normal against
	// the base tangent vector (assuming these are normalized)
	FVector Tangent( DestVertex.TangentX );
	FVector Normal( DestVertex.TangentZ );
	Tangent = Tangent - ((Tangent | Normal) * Normal);
	Tangent.Normalize();
	DestVertex.TangentX = Tangent;
}

/**
* Applies the vertex deltas to a vertex.
*/
template<typename VertexType>
FORCEINLINE void ApplyMorphBlend( FSkeletalMeshVertexBuffer	* VertexBuffer, VertexType& DestVertex, const FMorphTargetVertex& SrcMorph, FLOAT Weight )
{
	// Add position offset 
	DestVertex.Position += SrcMorph.PositionDelta * Weight;

	// Save W before = operator. That overwrites W to be 127.
	BYTE W = DestVertex.TangentZ.Vector.W;
	// add normal offset. can only apply normal deltas up to a weight of 1
	DestVertex.TangentZ = FVector(FVector(DestVertex.TangentZ) + FVector(SrcMorph.TangentZDelta) * Min(Weight,1.0f)).UnsafeNormal();
	// Recover W
	DestVertex.TangentZ.Vector.W = W;
} 

static const FVector VecZero(0,0,0);

/**
* Blends the source vertex with all the active morph targets.
*/
template<typename VertexType>
FORCEINLINE void UpdateMorphedVertex( FSkeletalMeshVertexBuffer	* VertexBuffer, TArray<FActiveMorph>& ActiveMorphs, VertexType& MorphedVertex, VertexType& SrcVertex, INT CurBaseVertIdx, INT LODIndex, TArray<INT>& MorphVertIndices )
{
	MorphedVertex = SrcVertex;

	// iterate over all active morphs
	for( INT MorphIdx=0; MorphIdx < MorphVertIndices.Num(); MorphIdx++ )
	{
		// get the next affected vertex index
		INT NextMorphVertIdx = MorphVertIndices(MorphIdx);
		// get the lod model (assumed valid)
		FMorphTargetLODModel& MorphModel = ActiveMorphs(MorphIdx).Target->MorphLODModels(LODIndex);
		// if the current base vertex matches the next affected morph index 
		if( MorphModel.Vertices.IsValidIndex(NextMorphVertIdx) &&
			MorphModel.Vertices(NextMorphVertIdx).SourceIdx == CurBaseVertIdx )
		{
			ApplyMorphBlend( VertexBuffer, MorphedVertex, MorphModel.Vertices(NextMorphVertIdx), ActiveMorphs(MorphIdx).Weight );
			MorphVertIndices(MorphIdx) += 1;
		}
	}

	// rebuild orthonormal tangents
	RebuildTangentBasis( MorphedVertex );
}
/**
* Since the vertices in the active morphs are sorted based on the index of the base mesh vert
* that they affect we keep track of the next valid morph vertex to apply
*
* @param	OutMorphVertIndices		[out] Llist of vertex indices that need a morph target blend
* @return							number of active morphs that are valid
*/
UINT GetMorphVertexIndices(const TArray<FActiveMorph>& ActiveMorphs, INT LODIndex, TArray<INT>& OutMorphVertIndices);

#endif //SKEL_RENDER_HEADER
