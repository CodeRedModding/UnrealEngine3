/*=============================================================================
	ShadowRendering.h: Shadow rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Forward declarations.
class FProjectedShadowInfo;
// Globals
extern const UINT SHADOW_BORDER;

/** Draws a sphere using RHIDrawIndexedPrimitiveUP, useful as approximate bounding geometry for deferred passes. */
extern void DrawStencilingSphere(const FSphere& Sphere, const FVector& PreViewTranslation);

/** Renders a cone with a spherical cap, used for rendering spot lights in deferred passes. */
extern void DrawStencilingCone(const FMatrix& ConeToWorld, FLOAT ConeAngle, FLOAT SphereRadius, const FVector& PreViewTranslation);

/**
 * Outputs no color, but can be used to write the mesh's depth values to the depth buffer.
 */
class FShadowDepthDrawingPolicy : public FMeshDrawingPolicy
{
public:

	FShadowDepthDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		UBOOL bInPreShadow,
		UBOOL bInTranslucentPreShadow,
		UBOOL bInFullSceneShadow,
		UBOOL bInDirectionalLight,
		UBOOL bInUseScreenDoorDefaultMaterialShader,
		UBOOL bInCastShadowAsTwoSided,
		UBOOL bReverseCulling,
		UBOOL bInOnePassPointLightShadow
		);

	FShadowDepthDrawingPolicy& operator = (const FShadowDepthDrawingPolicy& Other)
	{ 
		VertexShader = Other.VertexShader;
		GeometryShader = Other.GeometryShader;
#if WITH_D3D11_TESSELLATION
		HullShader = Other.HullShader;
		DomainShader = Other.DomainShader;
#endif
		PixelShader = Other.PixelShader;
		bPreShadow = Other.bPreShadow;
		bTranslucentPreShadow = Other.bTranslucentPreShadow;
		bFullSceneShadow = Other.bFullSceneShadow;
		bDirectionalLight = Other.bDirectionalLight;
		bUseScreenDoorDefaultMaterialShader = Other.bUseScreenDoorDefaultMaterialShader;
		bReverseCulling = Other.bReverseCulling;
		bOnePassPointLightShadow = Other.bOnePassPointLightShadow;
		(FMeshDrawingPolicy&)*this = (const FMeshDrawingPolicy&)Other;
		return *this; 
	}

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const FShadowDepthDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) 
			&& VertexShader == Other.VertexShader
			&& GeometryShader == Other.GeometryShader
#if WITH_D3D11_TESSELLATION
			&& HullShader == Other.HullShader
			&& DomainShader == Other.DomainShader
#endif
			&& PixelShader == Other.PixelShader
			&& bPreShadow == Other.bPreShadow
			&& bTranslucentPreShadow == Other.bTranslucentPreShadow
			&& bFullSceneShadow == Other.bFullSceneShadow
			&& bDirectionalLight == Other.bDirectionalLight
			&& bUseScreenDoorDefaultMaterialShader == Other.bUseScreenDoorDefaultMaterialShader
			&& bReverseCulling == Other.bReverseCulling
			&& bOnePassPointLightShadow == Other.bOnePassPointLightShadow;
	}
	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const;

	/** 
	 * Create bound shader state using the vertex decl from the mesh draw policy
	 * as well as the shaders needed to draw the mesh
	 * @param DynamicStride - optional stride for dynamic vertex data
	 * @return new bound shader state object
	 */
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;
	friend INT Compare(const FShadowDepthDrawingPolicy& A,const FShadowDepthDrawingPolicy& B);

	/** 
	 * Shadow currently being rendered by a FShadowDepthDrawingPolicy.  
	 * This is global so that different shadows can be used with the same static draw list. 
	 */
	static const FProjectedShadowInfo* ShadowInfo;

	UBOOL IsReversingCulling() const
	{
		return bReverseCulling;
	}

private:

	class FShadowDepthVertexShader* VertexShader;
	class FOnePassPointShadowProjectionGeometryShader* GeometryShader;
	class FShadowDepthPixelShader* PixelShader;

#if WITH_D3D11_TESSELLATION
	class FBaseHullShader* HullShader;
	class FShadowDepthDomainShader* DomainShader;
#endif

	BITFIELD bPreShadow:1;
	BITFIELD bTranslucentPreShadow:1;
	BITFIELD bFullSceneShadow:1;
	BITFIELD bDirectionalLight:1;
	BITFIELD bUseScreenDoorDefaultMaterialShader:1;
	BITFIELD bReverseCulling:1;
	BITFIELD bOnePassPointLightShadow:1;
};

/**
 * A drawing policy factory for the emissive drawing policy.
 */
class FShadowDepthDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = FALSE };

	struct ContextType
	{
		const FProjectedShadowInfo* ShadowInfo;
		UBOOL bTranslucentPreShadow;

		ContextType(const FProjectedShadowInfo* InShadowInfo, UBOOL bInTranslucentPreShadow) :
			ShadowInfo(InShadowInfo),
			bTranslucentPreShadow(bInTranslucentPreShadow)
		{}
	};

	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh);

	static UBOOL DrawDynamicMesh(
		const FSceneView& View,
		ContextType Context,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	static UBOOL DrawStaticMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
	{
		return FALSE;
	}
};

/**
 * A projected shadow transform.
 */
class FProjectedShadowInitializer
{
public:

	/** A translation that is applied to world-space before transforming by one of the shadow matrices. */
	FVector PreShadowTranslation;

	FMatrix PreSubjectMatrix;	// Z range from MinLightW to MaxSubjectW.
	FMatrix SubjectMatrix;		// Z range from MinSubjectW to MaxSubjectW.
	FMatrix PostSubjectMatrix;	// Z range from MinSubjectW to MaxShadowW.
	FMatrix SubjectMatrixFudged;// Z range from MinSubjectW to MaxSubjectW, with MinSubjectW pushed out somewhat (avoiding artifacts caused by near plane clamping on preshadows)

	FLOAT MaxSubjectDepth;
	FLOAT MaxPreSubjectDepth;
	FLOAT AspectRatio;

	FLOAT MinPreSubjectZ;
	FLOAT BoundingRadius;

	BITFIELD bDirectionalLight : 1;
	BITFIELD bFullSceneShadow : 1;

	INT SplitIndex;

	/**
	 * Calculates the shadow transforms for the given parameters.
	 */
	UBOOL CalcObjectShadowTransforms(
		const FVector& InPreShadowTranslation,
		const FMatrix& WorldToLight,
		const FVector& FaceDirection,
		const FBoxSphereBounds& SubjectBounds,
		const FVector4& WAxis,
		FLOAT MinLightW,
		FLOAT MaxDistanceToCastInLightW,
		UBOOL bInDirectionalLight
		);

	UBOOL CalcWholeSceneShadowTransforms(
		const FVector& InPreShadowTranslation,
		const FMatrix& WorldToLight,
		const FVector& FaceDirection,
		const FBoxSphereBounds& SubjectBounds,
		const FVector4& WAxis,
		FLOAT MinLightW,
		FLOAT MaxDistanceToCastInLightW,
		UBOOL bInDirectionalLight,
		UBOOL bInOnePassPointLight,
		INT InSplitIndex
		);
};

/** A single static mesh element for shadow depth rendering. */
class FShadowStaticMeshElement
{
public:

	FShadowStaticMeshElement(const FMaterialRenderProxy* InRenderProxy, const FStaticMesh* InMesh) :
		RenderProxy(InRenderProxy),
		Mesh(InMesh),
		bIsTwoSided(InRenderProxy->GetMaterial()->IsTwoSided() || InMesh->PrimitiveSceneInfo->bCastShadowAsTwoSided)
	{}

	FShadowStaticMeshElement& operator = (const FShadowStaticMeshElement& Other)
	{ 
		RenderProxy = Other.RenderProxy;
		Mesh = Other.Mesh;
		bIsTwoSided = Other.bIsTwoSided;
		return *this; 
	}

	/** Store the FMaterialRenderProxy pointer since it may be different from the one that FStaticMesh stores. */
	const FMaterialRenderProxy* RenderProxy;
	const FStaticMesh* Mesh;
	UBOOL bIsTwoSided;
};

/**
 * Information about a projected shadow.
 */
class FProjectedShadowInfo : public FRefCountedObject
{
public:

	friend class FShadowDepthVertexShader;
	friend class FShadowDepthPixelShader;
	friend class FShadowProjectionVertexShader;
	friend class FShadowProjectionPixelShader;
	friend class FShadowDepthDrawingPolicyFactory;

	typedef TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> PrimitiveArrayType;

	const FLightSceneInfo* const LightSceneInfo;
	const FLightSceneInfoCompact LightSceneInfoCompact;

	/** Parent primitive of the shadow group that created this shadow. */
	const FPrimitiveSceneInfo* const ParentSceneInfo;
	const FLightPrimitiveInteraction* const ParentInteraction;

	/** The view this shadow must be rendered in, or NULL for a view independent shadow. */
	const FViewInfo* DependentView;

	/** Index of the shadow into FVisibleLightInfo::AllProjectedShadows. */
	INT ShadowId;

	/** A translation that is applied to world-space before transforming by one of the shadow matrices. */
	FVector PreShadowTranslation;

	FMatrix SubjectAndReceiverMatrix;
	FMatrix ReceiverMatrix;

	FMatrix InvReceiverMatrix;

	FLOAT MaxSubjectDepth;

	FConvexVolume PreSubjectFrustum;
	FConvexVolume SubjectAndReceiverFrustum;
	FConvexVolume ReceiverFrustum;

	FLOAT MinPreSubjectZ;

	FSphere ShadowBounds;

	/** X and Y position of the shadow in the appropriate depth buffer.  These are only initialized after the shadow has been allocated. */
	UINT X;
	UINT Y;

	/** Resolution of the shadow. */
	UINT ResolutionX;
	UINT ResolutionY;

	/** The largest percent of either the width or height of any view. */
	FLOAT MaxScreenPercent;

	/** Fade Alpha per view. */
	TArray<FLOAT, TInlineAllocator<2> > FadeAlphas;

	/** 
	 * Index of the split if this is a full scene shadow from a directional light, 
	 * Or index of the direction if this is a full scene shadow from a point light, otherwise INDEX_NONE. 
	 */
	INT SplitIndex;

	/** Whether the shadow has been allocated in the shadow depth buffer, and its X and Y properties have been initialized. */
	BITFIELD bAllocated : 1;

	/** Whether the shadow's projection has been rendered. */
	BITFIELD bRendered : 1;

	/** Whether the shadow has been allocated in the preshadow cache, so its X and Y properties offset into the preshadow cache depth buffer. */
	BITFIELD bAllocatedInPreshadowCache : 1;

	/** Whether the shadow is in the preshadow cache and its depths are up to date. */
	BITFIELD bDepthsCached : 1;

	BITFIELD bDirectionalLight : 1;

	/** Whether the shadow is a full scene shadow or per object shadow. */
	BITFIELD bFullSceneShadow : 1;

	/** Whether the shadow is a preshadow or not.  A preshadow is a per object shadow that handles the static environment casting on a dynamic receiver. */
	BITFIELD bPreShadow : 1;

	/** Indicates whether the shadow has subjects in the foreground DPG but should cast shadows onto receivers in the world DPG */
	BITFIELD bForegroundCastingOnWorld : 1;

	/** True if any of the subjects only self shadow and do not cast shadows on other primitives. */
	BITFIELD bSelfShadowOnly : 1;

	TBitArray<SceneRenderingBitArrayAllocator> StaticMeshWholeSceneShadowDepthMap;

	/** View projection matrices for each cubemap face, used by one pass point light shadows. */
	TArray<FMatrix> OnePassShadowViewProjectionMatrices;

	/** Frustums for each cubemap face, used for object culling one pass point light shadows. */
	TArray<FConvexVolume> OnePassShadowFrustums;

private:

	/** dynamic shadow casting elements */
	PrimitiveArrayType SubjectPrimitives;
	/** For preshadows, this contains the receiver primitives to mask the projection to. */
	PrimitiveArrayType ReceiverPrimitives;

	/** Static shadow casting elements. */
	TArray<FShadowStaticMeshElement,SceneRenderingAllocator> SubjectMeshElements;

	/** bound shader state for stencil masking the shadow projection */
	static FGlobalBoundShaderState MaskBoundShaderState;
	/** bound shader state for pixel shader that stencil-masks pixels that need per-fragment shadowing. */
	static FGlobalBoundShaderState PerFragmentMaskBoundShaderState;
	/** bound shader states for shadow projection pass */
	static TStaticArray<FGlobalBoundShaderState,SFQ_Num> ShadowProjectionBoundShaderStates;
	static TStaticArray<FGlobalBoundShaderState,SFQ_Num> PerFragmentShadowProjectionBoundShaderStates;
	/** bound shader states for Branching PCF shadow projection pass */
	static TStaticArray<FGlobalBoundShaderState,SFQ_Num> BranchingPCFBoundShaderStates;
	static FGlobalBoundShaderState ShadowProjectionHiStencilClearBoundShaderState;
	static FGlobalBoundShaderState ShadowProjectionPointLightBoundShaderState;

public:

	/** Initialization constructor for an individual primitive shadow. */
	FProjectedShadowInfo(
		FLightSceneInfo* InLightSceneInfo,
		const FPrimitiveSceneInfo* InParentSceneInfo,
		const FLightPrimitiveInteraction* const InParentInteraction,
		const FProjectedShadowInitializer& Initializer,
		UBOOL bInPreShadow,
		UINT InResolutionX,
		UINT InResolutionY,
		FLOAT MaxScreenPercent,
		const TArray<FLOAT, TInlineAllocator<2> >& InFadeAlphas
		);

	/** Initialization constructor for a whole-scene shadow. */
	FProjectedShadowInfo(
		FLightSceneInfo* InLightSceneInfo,
		const FViewInfo* InDependentView,
		const FProjectedShadowInitializer& Initializer,
		UINT InResolutionX,
		UINT InResolutionY,
		const TArray<FLOAT, TInlineAllocator<2> >& InFadeAlphas
		);

	/**
	 * Renders the shadow subject depth.
	 */
	void RenderDepth(const class FSceneRenderer* SceneRenderer, BYTE DepthPriorityGroup, UBOOL bTranslucentPreShadow);

	/**
	 * Projects the shadow onto the scene for a particular view.
	 */
	void RenderProjection(INT ViewIndex, const class FViewInfo* View, BYTE DepthPriorityGroup, UBOOL bRenderingBeforeLight) const;

	/** Render one pass point light shadow projections. */
	void RenderOnePassPointLightProjection(INT ViewIndex, const FViewInfo& View, BYTE DepthPriorityGroup) const;

	/** Renders the subjects of the shadow with a matrix that flattens the vertices onto a plane. */
	void RenderPlanarProjection(const class FSceneRenderer* SceneRenderer, BYTE DepthPriorityGroup) const;

	/** 
	 * Renders a dynamic shadow using a forward projection. 
	 * This works when depth textures are not supported but causes many meshes to be re-rendered and pixels to be shaded.
	 */
	void RenderForwardProjection(const class FViewInfo* View, BYTE DepthPriorityGroup) const;

	/**
	 * Renders the projected shadow's frustum wireframe with the given FPrimitiveDrawInterface.
	 */
	void RenderFrustumWireframe(FPrimitiveDrawInterface* PDI) const;

	/**
	 * Adds a primitive to the whole scene shadow's subject list.
	 */
	void AddWholeSceneSubjectPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/**
	 * Adds a primitive to the shadow's subject list.
	 */
	void AddSubjectPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, TArray<FViewInfo>& Views);

	/**
	 * Adds a primitive to the shadow's receiver list.
	 */
	void AddReceiverPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/**
	* @return TRUE if this shadow info has any casting subject prims to render
	*/
	UBOOL HasSubjectPrims() const;

	UBOOL IsSubjectPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo) const
	{
		return SubjectPrimitives.ContainsItem(PrimitiveSceneInfo);
	}

	/** 
	 * @param View view to check visibility in
	 * @return TRUE if this shadow info has any subject prims visible in the view
	 */
	UBOOL SubjectsVisible(const FViewInfo& View) const;

	/** Clears arrays allocated with the scene rendering allocator. */
	void ClearTransientArrays();

	/** Hash function. */
	friend DWORD GetTypeHash(const FProjectedShadowInfo* ProjectedShadowInfo)
	{
		return PointerHash(ProjectedShadowInfo);
	}

	/** Returns a matrix that transforms a screen space position into shadow space. */
	FMatrix GetScreenToShadowMatrix(const FSceneView& View, UBOOL bTranslucentPreShadow) const;

	/** Returns the resolution of the shadow buffer used for this shadow, based on the shadow's type. */
	FIntPoint GetShadowBufferResolution(UBOOL bTranslucentPreShadow) const;

	inline UBOOL IsWholeSceneDominantShadow() const 
	{ 
		return SplitIndex >= 0 && bFullSceneShadow && LightSceneInfo->LightType == LightType_DominantDirectional; 
	}

	inline UBOOL IsPrimaryWholeSceneDominantShadow() const 
	{ 
		return SplitIndex == 0 && bFullSceneShadow && LightSceneInfo->LightType == LightType_DominantDirectional; 
	}

	inline UBOOL IsWholeScenePointLightShadow() const
	{
		return bFullSceneShadow && (LightSceneInfo->LightType == LightType_Point || LightSceneInfo->LightType == LightType_DominantPoint);
	}

	/** Sorts SubjectMeshElements based on state so that rendering the static elements will set as little state as possible. */
	void SortSubjectMeshElements();

	/** Draws the subject mesh elements */
	void VisualizeSubjectMeshElements(FPrimitiveDrawInterface* PDI)
	{
		for (INT i = 0; i < SubjectMeshElements.Num(); ++i)
		{
			const FStaticMesh* Mesh = SubjectMeshElements(i).Mesh;

			//FPrimitiveSceneInfo* SceneInfo = Mesh->PrimitiveSceneInfo
			//const FName ParentName = (SceneInfo && SceneInfo->Owner) ? SceneInfo->Owner->GetFName() : NAME_None;

			PDI->DrawMesh(*Mesh);
		}
	}

	/**
	 * Find the view and DPG that this shadow is relevant to
	 *
	 * @param Views - The views taken from SceneRenderer that are about to be rendered.
	 * @param DepthPriorityGroup - The Priority group of the PrimtiveSceneInfo to receive this shadow
	 * @param LightSceneInfoID - The ID of the light within a view
	 * @param bTranslucentPreShadow - TRUE if this pre shadow is for translucency
	 * @return FoundView - NULL if no relevant view is found.  Otherwise points to a view in which this shadow is relevant
	 * @return DPGToUseForDepths - The DPG to use for rendering this object to the depth map
	 */
	void FindViewAndDPGForRenderDepth(const TArray<FViewInfo>& Views, const UINT DepthPriorityGroup, const INT LightSceneInfoId, UBOOL bTranslucentPreShadow, const FViewInfo*& FoundView, ESceneDepthPriorityGroup& DPGToUseForDepths);

};

/** Stores information about a planar reflection shadow used by image reflections. */
class FReflectionPlanarShadowInfo 
{
public:

	/** Reflection plane in world space. */
	FPlane MirrorPlane;

	/** Volume of the reflected view frustum. */
	FConvexVolume ViewFrustum;

	/** Primitives visible by this reflection shadow view. */
	TArray<FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleDynamicPrimitives;
};

/*-----------------------------------------------------------------------------
FShadowFrustumVertexDeclaration
-----------------------------------------------------------------------------*/

/** The shadow frustum vertex declaration resource type. */
class FShadowFrustumVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FShadowFrustumVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,0,VET_Float3,VEU_Position,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern TGlobalResource<FShadowFrustumVertexDeclaration> GShadowFrustumVertexDeclaration;

/**
* A vertex shader for projecting a shadow depth buffer onto the scene.
*/
class FShadowProjectionVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowProjectionVertexShader,Global);
public:

	FShadowProjectionVertexShader() {}
	FShadowProjectionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	static UBOOL ShouldCache(EShaderPlatform Platform);

	void SetParameters(const FSceneView& View, const FProjectedShadowInfo* ShadowInfo);
	virtual UBOOL Serialize(FArchive& Ar);

	FShaderParameter ScreenToShadowMatrixParameter;
};


/**
* A vertex shader for projecting a shadow depth buffer onto the scene.
* For use with modulated shadows
*/
class FModShadowProjectionVertexShader : public FShadowProjectionVertexShader
{
	DECLARE_SHADER_TYPE(FModShadowProjectionVertexShader,Global);
public:

	/**
	* Constructor
	*/
	FModShadowProjectionVertexShader() {}

	/**
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
	FModShadowProjectionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	/**
	* Sets the current vertex shader
	* @param View - current view
	* @param ShadowInfo - projected shadow info for a single light
	*/
	void SetParameters(const FSceneView& View,const FProjectedShadowInfo* ShadowInfo);

	/**
	* Serialize the parameters for this shader
	* @param Ar - archive to serialize to
	*/
	virtual UBOOL Serialize(FArchive& Ar);
};

/**
* 
*/
class F4SampleHwPCF
{
public:
	static const UINT NumSamples = 4;
	static const UBOOL bUseHardwarePCF = TRUE;
	static const UBOOL bUseFetch4 = FALSE;
	static const UBOOL bPerFragment = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform != SP_XBOXD3D;
	}
};

/**
* 
*/
class F4SampleManualPCFPerPixel
{
public:
	static const UINT NumSamples = 4;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = FALSE;
	static const UBOOL bPerFragment = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

/**
* 
*/
class F4SampleManualPCFPerFragment
{
public:
	static const UINT NumSamples = 4;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = FALSE;
	static const UBOOL bPerFragment = TRUE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}
};

/**
 * Policy to use on SM3 hardware that supports Hardware PCF
 */
class F16SampleHwPCF
{
public:
	static const UINT NumSamples = 16;
	static const UBOOL bUseHardwarePCF = TRUE;
	static const UBOOL bUseFetch4 = FALSE;
	static const UBOOL bPerFragment = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM3 || Platform == SP_PS3;
	}
};

/**
 * Policy to use on SM3 hardware that supports Fetch4
 */
class F16SampleFetch4PCF
{
public:
	static const UINT NumSamples = 16;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = TRUE;
	static const UBOOL bPerFragment = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM3;
	}
};

/**
 * Policy to use on SM3 hardware with no support for Hardware PCF
 */
class F16SampleManualPCFPerPixel
{
public:
	static const UINT NumSamples = 16;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = FALSE;
	static const UBOOL bPerFragment = FALSE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

/**
 * Policy to use on SM3 hardware with no support for Hardware PCF
 */
class F16SampleManualPCFPerFragment
{
public:
	static const UINT NumSamples = 16;
	static const UBOOL bUseHardwarePCF = FALSE;
	static const UBOOL bUseFetch4 = FALSE;
	static const UBOOL bPerFragment = TRUE;

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}
};

/**
 * Samples used with the SM3 version
 */
static const FVector2D SixteenSampleOffsets[] =
{
	FVector2D(-1.5f,-1.5f), FVector2D(-0.5f,-1.5f), FVector2D(+0.5f,-1.5f), FVector2D(+1.5f,-1.5f),
	FVector2D(-1.5f,-0.5f), FVector2D(-0.5f,-0.5f), FVector2D(+0.5f,-0.5f), FVector2D(+1.5f,-0.5f),
	FVector2D(-1.5f,+0.5f), FVector2D(-0.5f,+0.5f), FVector2D(+0.5f,+0.5f), FVector2D(+1.5f,+0.5f),
	FVector2D(-1.5f,+1.5f), FVector2D(-0.5f,+1.5f), FVector2D(+0.5f,+1.5f), FVector2D(+1.5f,+1.5f)
};

static const FVector2D FourSampleOffsets[] = 
{
	FVector2D(-0.5f,-0.5f), FVector2D(+0.5f,-0.5f), FVector2D(-0.5f,+0.5f), FVector2D(+0.5f,+0.5f)
};

/**
 * FShadowProjectionPixelShaderInterface - used to handle templated versions
 */

class FShadowProjectionPixelShaderInterface : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowProjectionPixelShaderInterface,Global);
public:

	FShadowProjectionPixelShaderInterface() 
		:	FGlobalShader()
	{}

	/**
	 * Constructor - binds all shader params and initializes the sample offsets
	 * @param Initializer - init data from shader compiler
	 */
	FShadowProjectionPixelShaderInterface(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{ }

	/**
	 * Sets the current pixel shader params
	 * @param View - current view
	 * @param ShadowInfo - projected shadow info for a single light
	 */
	virtual void SetParameters(
		INT ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{ }

};

/** Shadow projection parameters used by multiple shaders. */
class FShadowProjectionShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		DeferredParameters.Bind(ParameterMap);
		ScreenToShadowMatrixParameter.Bind(ParameterMap,TEXT("ScreenToShadowMatrix"));
		HomShadowStartPosParameter.Bind(ParameterMap,TEXT("HomShadowStartPos"), TRUE);
		ShadowBufferSizeAndSoftTransitionScaleParameter.Bind(ParameterMap,TEXT("ShadowBufferSizeAndSoftTransitionScale"),TRUE);
		ShadowTexelSizeParameter.Bind(ParameterMap,TEXT("ShadowTexelSize"),TRUE);
		ShadowDepthTextureParameter.Bind(ParameterMap,TEXT("ShadowDepthTexture"));
	}

	void Set(FShader* Shader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, UBOOL bUseHardwarePCF, UBOOL bUseFetch4)
	{
		DeferredParameters.Set(View, Shader, SceneDepthUsage_ProjectedShadows);

		// Set the transform from screen coordinates to shadow depth texture coordinates.
		const FMatrix ScreenToShadow = ShadowInfo->GetScreenToShadowMatrix(View, FALSE);
		SetShaderValue(Shader->GetPixelShader(), ScreenToShadowMatrixParameter, ScreenToShadow);

		FVector4 HomShadowStartPosValue = ScreenToShadow.TransformFVector4(FVector4(0.0f, 0.0f, 0.0f, 1.0f));
		SetShaderValue(Shader->GetPixelShader(), HomShadowStartPosParameter, HomShadowStartPosValue);

		const FIntPoint ShadowBufferResolution = ShadowInfo->GetShadowBufferResolution(FALSE);

		if (ShadowBufferSizeAndSoftTransitionScaleParameter.IsBound() || ShadowTexelSizeParameter.IsBound())
		{
			// Scale up the size passed to the shader for Cascaded Shadow Maps so that the penumbra size of distant splits will better match up with the closer ones
			const FLOAT SizeScale = (ShadowInfo->SplitIndex > 0 && ShadowInfo->bDirectionalLight) ? ShadowInfo->SplitIndex / GSystemSettings.CSMSplitPenumbraScale : 1.0f;

			FLOAT TransitionScale;

			if (ShadowInfo->bFullSceneShadow && ShadowInfo->bDirectionalLight)
			{
				// Scale down the soft transition distance for distant splits
				TransitionScale = GSystemSettings.PerSceneShadowTransition * (ShadowInfo->SplitIndex > 0 ? GSystemSettings.CSMSplitSoftTransitionDistanceScale * ShadowInfo->SplitIndex : 1.0f); 
			}
			else
			{
				TransitionScale = GSystemSettings.PerObjectShadowTransition;
			}

			SetPixelShaderValue(Shader->GetPixelShader(), ShadowBufferSizeAndSoftTransitionScaleParameter, FVector(
				(FLOAT)ShadowBufferResolution.X * SizeScale, 
				(FLOAT)ShadowBufferResolution.Y * SizeScale,
				TransitionScale));

			SetPixelShaderValue(Shader->GetPixelShader(), ShadowTexelSizeParameter, FVector2D(
				1.0f / ((FLOAT)ShadowBufferResolution.X * SizeScale), 
				1.0f / ((FLOAT)ShadowBufferResolution.Y * SizeScale)));
		}

		FTexture2DRHIRef ShadowDepthSampler;
		FSamplerStateRHIParamRef DepthSamplerState;

		if (bUseHardwarePCF)
		{
			//take advantage of linear filtering on nvidia depth stencil textures
			DepthSamplerState = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			//sample the depth texture
			ShadowDepthSampler = GSceneRenderTargets.GetShadowDepthZTexture(ShadowInfo->IsPrimaryWholeSceneDominantShadow(), ShadowInfo->bAllocatedInPreshadowCache);
		}
		else if (GSupportsDepthTextures)
		{
			DepthSamplerState = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			//sample the depth texture
			ShadowDepthSampler = GSceneRenderTargets.GetShadowDepthZTexture(ShadowInfo->IsPrimaryWholeSceneDominantShadow(), ShadowInfo->bAllocatedInPreshadowCache);
		} 
		else if (bUseFetch4)
		{
			//enable Fetch4 on this sampler
			DepthSamplerState = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp,MIPBIAS_Get4>::GetRHI();
			//sample the depth texture
			ShadowDepthSampler = GSceneRenderTargets.GetShadowDepthZTexture(ShadowInfo->IsPrimaryWholeSceneDominantShadow(), ShadowInfo->bAllocatedInPreshadowCache);
		}
		else
		{
			DepthSamplerState = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			//sample the color depth texture
			ShadowDepthSampler = GSceneRenderTargets.GetShadowDepthColorTexture(ShadowInfo->IsPrimaryWholeSceneDominantShadow(), ShadowInfo->bAllocatedInPreshadowCache);
		}

		SetTextureParameterDirectly(
			Shader->GetPixelShader(),
			ShadowDepthTextureParameter,
			DepthSamplerState,
			ShadowDepthSampler
			);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FShadowProjectionShaderParameters& P)
	{
		Ar << P.DeferredParameters;
		Ar << P.ScreenToShadowMatrixParameter;
		Ar << P.HomShadowStartPosParameter;
		Ar << P.ShadowBufferSizeAndSoftTransitionScaleParameter;
		Ar << P.ShadowTexelSizeParameter;
		Ar << P.ShadowDepthTextureParameter;

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			P.DeferredParameters.SceneTextureParameters.SceneColorTextureParameter.SetBaseIndex( 0 );
			P.DeferredParameters.SceneTextureParameters.SceneDepthTextureParameter.SetBaseIndex( 1, TRUE );

			P.ShadowDepthTextureParameter.SetBaseIndex( 2, TRUE );
			P.ScreenToShadowMatrixParameter.SetShaderParamName(TEXT("ScreenToShadowMatrix"));
			P.HomShadowStartPosParameter.SetShaderParamName(TEXT("HomShadowStartPos"));
			P.ShadowBufferSizeAndSoftTransitionScaleParameter.SetShaderParamName(TEXT("ShadowBufferSizeAndSoftTransitionScale"));
			P.ShadowTexelSizeParameter.SetShaderParamName(TEXT("ShadowTexelSize"));
		}
#endif

		return Ar;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter ScreenToShadowMatrixParameter;
	FShaderParameter HomShadowStartPosParameter;
	FShaderParameter ShadowBufferSizeAndSoftTransitionScaleParameter;
	FShaderParameter ShadowTexelSizeParameter;
	FShaderResourceParameter ShadowDepthTextureParameter;
};

/**
 * TShadowProjectionPixelShader
 * A pixel shader for projecting a shadow depth buffer onto the scene.  Used with any light type casting normal shadows.
 */
template<class UniformPCFPolicy> 
class TShadowProjectionPixelShader : public FShadowProjectionPixelShaderInterface
{
	DECLARE_SHADER_TYPE(TShadowProjectionPixelShader,Global);
public:

	TShadowProjectionPixelShader()
		: FShadowProjectionPixelShaderInterface()
	{ 
		SetSampleOffsets(); 
	}

	/**
	 * Constructor - binds all shader params and initializes the sample offsets
	 * @param Initializer - init data from shader compiler
	 */
	TShadowProjectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FShadowProjectionPixelShaderInterface(Initializer)
	{
		ProjectionParameters.Bind(Initializer.ParameterMap);
		SampleOffsetsParameter.Bind(Initializer.ParameterMap,TEXT("SampleOffsets"),TRUE);
		ShadowFadeFractionParameter.Bind(Initializer.ParameterMap,TEXT("ShadowFadeFraction"),TRUE);
		LightingChannelMaskParameter.Bind(Initializer.ParameterMap,TEXT("LightingChannelMask"),TRUE);

		SetSampleOffsets();
	}

	/**
	 *  Initializes the sample offsets
	 */
	void SetSampleOffsets()
	{
		check(UniformPCFPolicy::NumSamples == 4 || UniformPCFPolicy::NumSamples == 16);

		if (UniformPCFPolicy::NumSamples == 4)
		{
			appMemcpy(SampleOffsets, FourSampleOffsets, 4 * sizeof(FVector2D));
		}
		else if (UniformPCFPolicy::NumSamples == 16)
		{
			appMemcpy(SampleOffsets, SixteenSampleOffsets, 16 * sizeof(FVector2D));
		}
	}

	/**
	 * @param Platform - hardware platform
	 * @return TRUE if this shader should be cached
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return UniformPCFPolicy::ShouldCache(Platform);
	}

	/**
	 * Add any defines required by the shader
	 * @param OutEnvironment - shader environment to modify
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLE_CHUNKS"),*FString::Printf(TEXT("%u"),UniformPCFPolicy::NumSamples / 4));
		OutEnvironment.Definitions.Set(TEXT("PER_FRAGMENT"),UniformPCFPolicy::bPerFragment ? TEXT("1") : TEXT("0"));
	}

	virtual void SetLightChannelParameter(const FProjectedShadowInfo* ShadowInfo)
	{
		// Using the G buffer's lighting channels to implement self shadowing modes
		if (ShouldUseDeferredShading() && ShadowInfo->LightSceneInfo->LightingChannels.GetDeferredShadingChannelMask() > 0)
		{
			FLightingChannelContainer AllChannels;
			AllChannels.SetAllChannels();

			const FLOAT Mask = (ShadowInfo->bSelfShadowOnly || ShadowInfo->LightSceneInfo->bNonModulatedSelfShadowing) ? 
				// Self shadow only and bNonModulatedSelfShadowing in the normal shadow pass want to affect just the shadow caster
				ShadowInfo->LightSceneInfo->LightingChannels.GetDeferredShadingChannelMask() : 
				AllChannels.GetDeferredShadingChannelMask();

			SetPixelShaderValue(GetPixelShader(), LightingChannelMaskParameter, Mask);
		}
	}

	/**
	 * Sets the pixel shader's parameters
	 * @param View - current view
	 * @param ShadowInfo - projected shadow info for a single light
	 */
	virtual void SetParameters(
		INT ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		ProjectionParameters.Set(this, View, ShadowInfo, UniformPCFPolicy::bUseHardwarePCF, UniformPCFPolicy::bUseFetch4);

		const FIntPoint ShadowBufferResolution = ShadowInfo->GetShadowBufferResolution(FALSE);

		SetLightChannelParameter(ShadowInfo);

#if WITH_REALD
		SetPixelShaderValue(GetPixelShader(),ShadowFadeFractionParameter, ShadowInfo->FadeAlphas(0));
#else
		SetPixelShaderValue(GetPixelShader(),ShadowFadeFractionParameter, ShadowInfo->FadeAlphas(ViewIndex));
#endif

		const static FLOAT CosRotation = appCos(0.25f * (FLOAT)PI);
		const static FLOAT SinRotation = appSin(0.25f * (FLOAT)PI);
		const FLOAT InvBufferResolution = 1.0f / (FLOAT)Max(ShadowBufferResolution.X, ShadowBufferResolution.Y);
		const FLOAT TexelRadius = GSystemSettings.ShadowFilterRadius / 2 * InvBufferResolution;
		//set the sample offsets
		for(INT SampleIndex = 0;SampleIndex < UniformPCFPolicy::NumSamples;SampleIndex += 2)
		{
			SetPixelShaderValue(
				FShader::GetPixelShader(),
				SampleOffsetsParameter,
				FVector4(
				(SampleOffsets[SampleIndex + 0].X * +CosRotation + SampleOffsets[SampleIndex + 0].Y * +SinRotation) * TexelRadius,
				(SampleOffsets[SampleIndex + 0].X * -SinRotation + SampleOffsets[SampleIndex + 0].Y * +CosRotation) * TexelRadius,
				(SampleOffsets[SampleIndex + 1].X * +CosRotation + SampleOffsets[SampleIndex + 1].Y * +SinRotation) * TexelRadius,
				(SampleOffsets[SampleIndex + 1].X * -SinRotation + SampleOffsets[SampleIndex + 1].Y * +CosRotation) * TexelRadius
				),
				SampleIndex / 2
				);
		}
	}

	/**
	 * Serialize the parameters for this shader
	 * @param Ar - archive to serialize to
	 */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ProjectionParameters;
		Ar << SampleOffsetsParameter;
		Ar << ShadowFadeFractionParameter;
		Ar << LightingChannelMaskParameter;

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			ShadowFadeFractionParameter.SetShaderParamName(TEXT("ShadowFadeFraction"));
		}
#endif

		return bShaderHasOutdatedParameters;
	}

protected:
	FVector2D SampleOffsets[UniformPCFPolicy::NumSamples];
	FShadowProjectionShaderParameters ProjectionParameters;
	FShaderParameter SampleOffsetsParameter;	
	FShaderParameter ShadowFadeFractionParameter;
	FShaderParameter LightingChannelMaskParameter;
};

/**
 * FShadowPerFragmentMaskPixelShader
 * A pixel shader for finding pixels with MSAA depth samples that need per-fragment shadowing.
 */
class FShadowPerFragmentMaskPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowPerFragmentMaskPixelShader,Global);
public:

	FShadowPerFragmentMaskPixelShader()
		: FGlobalShader()
	{ 
	}

	/**
	 * Constructor - binds all shader params and initializes the sample offsets
	 * @param Initializer - init data from shader compiler
	 */
	FShadowPerFragmentMaskPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParams.Bind(Initializer.ParameterMap);
	}

	/**
	 * Sets the pixel shader's parameters
	 * @param View - current view
	 * @param ShadowInfo - projected shadow info for a single light
	 */
	void SetParameters(const FSceneView& View)
	{
		SceneTextureParams.Set(&View, this, SF_Point, SceneDepthUsage_ProjectedShadows);
	}

	// FShader interface.
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParams;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParams;
};

/**
 * TModShadowProjectionPixelShader - pixel shader used with lights casting modulative shadows
 * Attenuation is based on light type so the modulated shadow projection is coupled with a LightTypePolicy type
 */
template<class LightTypePolicy, class UniformPCFPolicy>
class TModShadowProjectionPixelShader : public TShadowProjectionPixelShader<UniformPCFPolicy>, public LightTypePolicy::ModShadowPixelParamsType
{
	DECLARE_SHADER_TYPE(TModShadowProjectionPixelShader,Global);
public:
	typedef typename LightTypePolicy::SceneInfoType LightSceneInfoType;

	/**
	 * Constructor
	 */
	TModShadowProjectionPixelShader() {}

	/**
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
	TModShadowProjectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	TShadowProjectionPixelShader<UniformPCFPolicy>(Initializer)
	{
		ShadowModulateColorParam.Bind(Initializer.ParameterMap,TEXT("ShadowModulateColor"));
		ScreenToWorldParam.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"),TRUE);
		LightTypePolicy::ModShadowPixelParamsType::Bind(Initializer.ParameterMap);
	}

	/**
	 * @param Platform - hardware platform
	 * @return TRUE if this shader should be cached
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TShadowProjectionPixelShader<UniformPCFPolicy>::ShouldCache(Platform);
	}

	/**
	 * Add any defines required by the shader or light policy
	 * @param OutEnvironment - shader environment to modify
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TShadowProjectionPixelShader<UniformPCFPolicy>::ModifyCompilationEnvironment(Platform, OutEnvironment);
        LightTypePolicy::ModShadowPixelParamsType::ModifyCompilationEnvironment(Platform, OutEnvironment);	
	}

	virtual void SetLightChannelParameter(const FProjectedShadowInfo* ShadowInfo)
	{
		// Using the G buffer's lighting channels to implement self shadowing modes
		if (ShouldUseDeferredShading() && ShadowInfo->LightSceneInfo->LightingChannels.GetDeferredShadingChannelMask() > 0)
		{
			FLightingChannelContainer AllChannels;
			AllChannels.SetAllChannels();

			const BYTE DeferredShadingChannelMask = ShadowInfo->LightSceneInfo->LightingChannels.GetDeferredShadingChannelMask();
			const FLOAT Mask = ShadowInfo->LightSceneInfo->bNonModulatedSelfShadowing ? 
				// Self shadow only wants to affect only the shadow caster
				// bNonModulatedSelfShadowing in the modulated shadow pass wants to affect everything but the shadow caster
				(ShadowInfo->bSelfShadowOnly ? DeferredShadingChannelMask : (DeferredShadingChannelMask ^ 255)) :
				AllChannels.GetDeferredShadingChannelMask();

			SetPixelShaderValue(FShader::GetPixelShader(), TShadowProjectionPixelShader<UniformPCFPolicy>::LightingChannelMaskParameter, Mask);
		}
	}

	/**
	 * Sets the pixel shader's parameters
	 * @param View - current view
	 * @param ShadowInfo - projected shadow info for a single light
	 */
	virtual void SetParameters(
		INT ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		TShadowProjectionPixelShader<UniformPCFPolicy>::SetParameters(ViewIndex,View,ShadowInfo);		
		const FLightSceneInfo* LightSceneInfo = ShadowInfo->LightSceneInfo;

		// color to modulate shadowed areas on screen
		SetPixelShaderValue(
			FShader::GetPixelShader(),
			ShadowModulateColorParam,
			Lerp(FLinearColor::White,LightSceneInfo->ModShadowColor,ShadowInfo->FadeAlphas(ViewIndex))
			);
		// screen space to world space transform
		FMatrix ScreenToWorld = FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) * 
			View.InvTranslatedViewProjectionMatrix;	
		SetPixelShaderValue( FShader::GetPixelShader(), ScreenToWorldParam, ScreenToWorld );

		LightTypePolicy::ModShadowPixelParamsType::SetModShadowLight( this, (const LightSceneInfoType*) ShadowInfo->LightSceneInfo, &View );
	}

	/**
	 * Serialize the parameters for this shader
	 * @param Ar - archive to serialize to
	 */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = TShadowProjectionPixelShader<UniformPCFPolicy>::Serialize(Ar);
		Ar << ShadowModulateColorParam;
		Ar << ScreenToWorldParam;
		LightTypePolicy::ModShadowPixelParamsType::Serialize(Ar);

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			ShadowModulateColorParam.SetShaderParamName(TEXT("ShadowModulateColor"));
		}
#endif

		return bShaderHasOutdatedParameters;
	}

private:
	/** color to modulate shadowed areas on screen */
	FShaderParameter ShadowModulateColorParam;	
	/** needed to get world positions from deferred scene depth values */
	FShaderParameter ScreenToWorldParam;
};

/**
 * Pixel shader used to project one pass point light shadows.
 */
class FOnePassPointShadowProjectionPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOnePassPointShadowProjectionPixelShader,Global);
public:

	FOnePassPointShadowProjectionPixelShader() {}

	FOnePassPointShadowProjectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParams.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		ShadowDepthTextureParameter.Bind(Initializer.ParameterMap,TEXT("ShadowDepthCubeTexture"), TRUE);
		ShadowDepthCubeComparisonSamplerParameter.Bind(Initializer.ParameterMap,TEXT("ShadowDepthCubeComparisonSampler"), TRUE);
		ShadowDepthCubeSamplerParameter.Bind(Initializer.ParameterMap,TEXT("ShadowDepthCubeSampler"), TRUE);
		LightPositionParameter.Bind(Initializer.ParameterMap,TEXT("LightPositionAndInvRadius"), TRUE);
		ShadowViewProjectionMatricesParameter.Bind(Initializer.ParameterMap, TEXT("ShadowViewProjectionMatrices"), TRUE);
		ShadowmapResolutionParameter.Bind(Initializer.ParameterMap, TEXT("ShadowmapResolution"), TRUE);
		ShadowFadeFractionParameter.Bind(Initializer.ParameterMap,TEXT("ShadowFadeFraction"),TRUE);
	}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	void SetParameters(
		INT ViewIndex,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
#if PLATFORM_SUPPORTS_D3D10_PLUS
		SceneTextureParams.Set(&View, this, SF_Point, SceneDepthUsage_ProjectedShadows);
		DeferredParameters.Set(View, this);

		if (ShadowDepthTextureParameter.IsBound())
		{
			RHISetTextureParameter(
				FShader::GetPixelShader(), 
				ShadowDepthTextureParameter.GetBaseIndex(), 
				GSceneRenderTargets.GetCubeShadowDepthZTexture(ShadowInfo->ResolutionX));
		}

		if (ShadowDepthCubeComparisonSamplerParameter.IsBound())
		{
			RHISetSamplerStateOnly(
				FShader::GetPixelShader(), 
				ShadowDepthCubeComparisonSamplerParameter.GetSamplerIndex(), 
				// Use a comparison sampler to do hardware PCF
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, MIPBIAS_None, 0, 0, SCF_Less>::GetRHI());
		}

		if (ShadowDepthCubeSamplerParameter.IsBound())
		{
			RHISetSamplerStateOnly(
				FShader::GetPixelShader(), 
				ShadowDepthCubeSamplerParameter.GetSamplerIndex(), 
				TStaticSamplerState<SF_Point>::GetRHI());
		}

		SetPixelShaderValue(GetPixelShader(), LightPositionParameter, FVector4(ShadowInfo->LightSceneInfo->GetPosition(), 1.0f / ShadowInfo->LightSceneInfo->GetRadius()));

		SetShaderValues<FPixelShaderRHIParamRef, FMatrix>(
			GetPixelShader(),
			ShadowViewProjectionMatricesParameter,
			ShadowInfo->OnePassShadowViewProjectionMatrices.GetData(),
			ShadowInfo->OnePassShadowViewProjectionMatrices.Num()
			);

		SetPixelShaderValue(GetPixelShader(),ShadowmapResolutionParameter,(FLOAT)ShadowInfo->ResolutionX);

		SetPixelShaderValue(GetPixelShader(),ShadowFadeFractionParameter, ShadowInfo->FadeAlphas(ViewIndex));
#endif
	}

	/**
	 * Serialize the parameters for this shader
	 * @param Ar - archive to serialize to
	 */
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParams;
		Ar << DeferredParameters;
		Ar << ShadowDepthTextureParameter;
		Ar << ShadowDepthCubeComparisonSamplerParameter;
		Ar << ShadowDepthCubeSamplerParameter;
		Ar << LightPositionParameter;
		Ar << ShadowViewProjectionMatricesParameter;
		Ar << ShadowmapResolutionParameter;
		Ar << ShadowFadeFractionParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParams;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter ShadowDepthTextureParameter;
	FShaderResourceParameter ShadowDepthCubeComparisonSamplerParameter;
	FShaderResourceParameter ShadowDepthCubeSamplerParameter;
	FShaderParameter LightPositionParameter;
	FShaderParameter ShadowViewProjectionMatricesParameter;
	FShaderParameter ShadowmapResolutionParameter;
	FShaderParameter ShadowFadeFractionParameter;
};

/**
 * Get the version of TModShadowProjectionPixelShader that should be used based on the hardware's capablities
 * @return a pointer to the chosen shader
 */
template<class LightTypePolicy>
FShadowProjectionPixelShaderInterface* GetModProjPixelShaderRef(BYTE LightShadowQuality)
{
	//apply the system settings bias to the light's shadow quality
	BYTE EffectiveShadowFilterQuality = Max(LightShadowQuality + GSystemSettings.ShadowFilterQualityBias, 0);

	if (EffectiveShadowFilterQuality == SFQ_Low)
	{
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef<TModShadowProjectionPixelShader<LightTypePolicy,F4SampleHwPCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
		else
		{
			TShaderMapRef<TModShadowProjectionPixelShader<LightTypePolicy,F4SampleManualPCFPerPixel> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
	}
	else
	{
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef<TModShadowProjectionPixelShader<LightTypePolicy,F16SampleHwPCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
		else if (GSceneRenderTargets.IsFetch4Supported())
		{
			TShaderMapRef<TModShadowProjectionPixelShader<LightTypePolicy,F16SampleFetch4PCF> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
		else
		{
			TShaderMapRef<TModShadowProjectionPixelShader<LightTypePolicy,F16SampleManualPCFPerPixel> > ModShadowShader(GetGlobalShaderMap());
			return *ModShadowShader;
		}
	}
}

/** 
 * ChooseBoundShaderState - decides which bound shader state should be used based on quality settings
 * @param LightShadowQuality - light's filter quality setting
 * @return FGlobalBoundShaderState - the bound shader state chosen
 */
extern FGlobalBoundShaderState* ChooseBoundShaderState(
	BYTE LightShadowQuality,
	TStaticArray<FGlobalBoundShaderState,SFQ_Num>& BoundShaderStates
	);

extern void DrawStencilingSphere(const FSphere& Sphere, const FVector& PreViewTranslation);

/** 
 * Whether to render whole scene point light shadows in one pass where supported (SM5), 
 * Or to render them using 6 spotlight projections.
 */
extern UBOOL GRenderOnePassPointLightShadows;

/** Returns TRUE if the given shadow belongs to a point light that should be rendered in one pass. */
inline UBOOL ShouldRenderOnePassPointLightShadow(const FProjectedShadowInfo* Shadow)
{
	return GRenderOnePassPointLightShadows 
		&& GRHIShaderPlatform == SP_PCD3D_SM5
		&& Shadow->IsWholeScenePointLightShadow();
}
