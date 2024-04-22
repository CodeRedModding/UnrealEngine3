/*=============================================================================
	UnTerrainRender.h: Definitions and inline code for rendering TerrainComponet
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef TERRAIN_RENDER_HEADER
#define TERRAIN_RENDER_HEADER

#include "ScenePrivate.h"
#include "GenericOctree.h"

#if __INTEL_BYTE_ORDER__ || PS3 || WIIU
	#define	UBYTE4_BYTEORDER_XYZW		1
#else
	#define	UBYTE4_BYTEORDER_XYZW		0
#endif

// Forward declarations.
class FDecalInteraction;
class FDecalState;
class FDynamicTerrainData;
class FTerrainComponentSceneProxy;
struct FTerrainObject;
class UTerrainComponent;

/**
 *	Flags identifying morphing setup.
 */
enum ETerrainMorphing
{
	/** No morphing is applied to this terrain.			*/
	ETMORPH_Disabled	= 0x00,
	/** Morph the terrain height.						*/
	ETMORPH_Height		= 0x01,
	/** Morph the terrain gradients.					*/
	ETMORPH_Gradient	= 0x02,
	/** Morph both the terrain height and gradients.	*/
	ETMORPH_Full		= ETMORPH_Height | ETMORPH_Gradient
};

/**
 *	FTerrainVertex
 *	The vertex structure used for terrain. 
 */
struct FTerrainVertex
{
	union
	{
		DWORD PackedCoordinates;
		struct
		{
		#if UBYTE4_BYTEORDER_XYZW
			BYTE	X,
					Y,
					Z_LOBYTE,
					Z_HIBYTE;
		#else
			BYTE	Z_HIBYTE,
					Z_LOBYTE,
					Y,
					X;
		#endif
		};
	};
	SWORD	GradientX,
			GradientY;
};

/**
 *	FTerrainMorphingVertex
 *	Vertex structure used when morphing terrain.
 *	Contains the transitional Height values
 */
struct FTerrainMorphingVertex : FTerrainVertex
{
	union
	{
		DWORD PackedData;
		struct
		{
			#if UBYTE4_BYTEORDER_XYZW
				BYTE	TESS_DATA_INDEX_LO,
						TESS_DATA_INDEX_HI,
						Z_TRANS_LOBYTE,
						Z_TRANS_HIBYTE;
			#else
				BYTE	Z_TRANS_HIBYTE,
						Z_TRANS_LOBYTE,
						TESS_DATA_INDEX_HI,
						TESS_DATA_INDEX_LO;
			#endif
		};
	};
};

/**
 *	FTerrainFullMorphingVertex
 *	Vertex structure used when morphing terrain.
 *	Contains the transitional Height and Gradient values
 */
struct FTerrainFullMorphingVertex : FTerrainMorphingVertex
{
	SWORD	TransGradientX,
			TransGradientY;
};

//
//	FTerrainVertexBuffer
//
struct FTerrainVertexBuffer: FVertexBuffer
{
	/**
	 * Constructor.
	 * @param InMeshRenderData pointer to parent structure
	 */
	FTerrainVertexBuffer(const FTerrainObject* InTerrainObject, const UTerrainComponent* InComponent, INT InMaxTessellation, UBOOL bInIsDynamic = FALSE) :
		  bIsDynamic(bInIsDynamic)
		, TerrainObject(InTerrainObject)
		, Component(InComponent)
		, MaxTessellation(InMaxTessellation)
		, MaxVertexCount(0)
		, CurrentTessellation(-1)
		, VertexCount(0)
		, bRepackRequired(bInIsDynamic)
		, MorphingFlags(ETMORPH_Disabled)
	{
		if (InComponent)
		{
			ATerrain* Terrain = InComponent->GetTerrain();
			if (Terrain)
			{
				if (Terrain->bMorphingEnabled)
				{
					MorphingFlags = ETMORPH_Height;
					if (Terrain->bMorphingGradientsEnabled)
					{
						MorphingFlags = ETMORPH_Full;
					}
				}
			}
		}
	}

	// FRenderResource interface.
	virtual void InitRHI();

	/** 
	 * Initialize the dynamic RHI for this rendering resource 
	 */
	virtual void InitDynamicRHI();

	/** 
	 * Release the dynamic RHI for this rendering resource 
	 */
	virtual void ReleaseDynamicRHI();

	virtual FString GetFriendlyName() const
	{
		return TEXT("Terrain component vertices");
	}

	inline INT GetCurrentTessellation()	{	return CurrentTessellation;	}
	inline INT GetVertexCount()			{	return VertexCount;			}
	inline UBOOL GetRepackRequired()	{	return bRepackRequired;		}
	inline void ForceRepackRequired()	{	bRepackRequired = TRUE;		}
	inline void ClearRepackRequired()	{	bRepackRequired = FALSE;	}

	virtual void SetCurrentTessellation(INT InCurrentTessellation)
	{
		CurrentTessellation = Clamp<INT>(InCurrentTessellation, 0, MaxTessellation);
	}

	virtual UBOOL FillData(INT TessellationLevel);

private:
	/** Flag indicating it is dynamic						*/
	UBOOL				bIsDynamic;
	/** The owner terrain object							*/
	const FTerrainObject*		TerrainObject;
	/** The 'owner' component								*/
	const UTerrainComponent*	Component;
	/** The maximum tessellation to create vertices for		*/
	INT					MaxTessellation;
	/** The maximum number of vertices in the buffer		*/
	INT					MaxVertexCount;
	/** The maximum tessellation to create vertices for		*/
	INT					CurrentTessellation;
	/** The number of vertices in the buffer				*/
	INT					VertexCount;
	/** A repack is required								*/
	UBOOL				bRepackRequired;
	/** Flag indicating it is for morphing terrain			*/
	BYTE				MorphingFlags;
};




// Forward declarations.
struct FTerrainIndexBuffer;
struct TerrainTessellationIndexBufferType;
struct TerrainDecalTessellationIndexBufferType;

//
//	FTerrainObject
//
struct FTerrainObject : public FDeferredCleanupInterface
{
public:
	FTerrainObject(UTerrainComponent* InTerrainComponent, INT InMaxTessellation) :
		  bIsInitialized(FALSE)
		, bRepackRequired(TRUE)
		, MorphingFlags(ETMORPH_Disabled)
	    , TerrainComponent(InTerrainComponent)
		, TessellationLevels(NULL)
		, ScaleFactorX(1.0f)
		, ScaleFactorY(1.0f)
		, LayerCoordinateOffset(0.f,0.f,0.f)
		, VertexFactory(NULL)
		, DecalVertexFactory(NULL)
		, VertexBuffer(NULL)
		, SmoothIndexBuffer(NULL)
	{
		check(TerrainComponent);
		ATerrain* Terrain = TerrainComponent->GetTerrain();
		if (Terrain)
		{
			ScaleFactorX = Terrain->DrawScale3D.Z / Terrain->DrawScale3D.X;
			ScaleFactorY = Terrain->DrawScale3D.Z / Terrain->DrawScale3D.Y;
			if (Terrain->bMorphingEnabled)
			{
				MorphingFlags = ETMORPH_Height;
				if (Terrain->bMorphingGradientsEnabled)
				{
					MorphingFlags = ETMORPH_Full;
				}
			}
			if( Terrain->bUseWorldOriginTextureUVs )
			{
				LayerCoordinateOffset = FVector(Terrain->Location.X / Terrain->DrawScale3D.X, Terrain->Location.Y / Terrain->DrawScale3D.Y, 0.f);
			}
		}
		Init();
	}
	
	virtual ~FTerrainObject();

	void Init();

	virtual void InitResources();
	virtual void ReleaseResources();
	virtual void Update();
	virtual const FVertexFactory* GetVertexFactory() const;

	UBOOL GetRepackRequired() const
	{
		return bRepackRequired;
	}

	void SetRepackRequired(UBOOL bInRepackRequired)
	{
		bRepackRequired = bInRepackRequired;
	}

#if 1	//@todo. Remove these as we depend on the component anyway!
	inline INT		GetComponentSectionSizeX() const		{	return ComponentSectionSizeX;		}
	inline INT		GetComponentSectionSizeY() const		{	return ComponentSectionSizeY;		}
	inline INT		GetComponentSectionBaseX() const		{	return ComponentSectionBaseX;		}
	inline INT		GetComponentSectionBaseY() const		{	return ComponentSectionBaseY;		}
	inline INT		GetComponentTrueSectionSizeX() const	{	return ComponentTrueSectionSizeX;	}
	inline INT		GetComponentTrueSectionSizeY() const	{	return ComponentTrueSectionSizeY;	}
#endif	//#if 1	//@todo. Remove these as we depend on the component anyway!
	inline INT		GetNumVerticesX() const					{	return NumVerticesX;				}
	inline INT		GetNumVerticesY() const					{	return NumVerticesY;				}
	inline INT		GetMaxTessellationLevel() const			{	return MaxTessellationLevel;		}
	inline FLOAT	GetTerrainHeightScale() const			{	return TerrainHeightScale;			}
	inline FLOAT	GetTessellationDistanceScale() const	{	return TessellationDistanceScale;	}
	inline BYTE		GetTessellationLevel(INT Index) const	{	return TessellationLevels[Index];	}
	inline FLOAT	GetScaleFactorX() const					{	return ScaleFactorX;				}
	inline FLOAT	GetScaleFactorY() const					{	return ScaleFactorY;				}
	inline INT		GetLightMapResolution() const			{	return LightMapResolution;			}
	inline FLOAT	GetShadowCoordinateScaleX() const		{	return ShadowCoordinateScale.X;		}
	inline FLOAT	GetShadowCoordinateScaleY() const		{	return ShadowCoordinateScale.Y;		}
	inline FLOAT	GetShadowCoordinateBiasX() const		{	return ShadowCoordinateBias.X;		}
	inline FLOAT	GetShadowCoordinateBiasY() const		{	return ShadowCoordinateBias.Y;		}
	inline const FVector&	GetLayerCoordinateOffset() const{	return LayerCoordinateOffset;		}
	inline FMatrix&	GetLocalToWorld() const				
	{
		check(TerrainComponent);
		return TerrainComponent->LocalToWorld;
	}

	inline void		SetShadowCoordinateScale(const FVector2D& InShadowCoordinateScale)
	{
		ShadowCoordinateScale = InShadowCoordinateScale;
	}
	inline void		SetShadowCoordinateBias(const FVector2D& InShadowCoordinateBias)
	{
		ShadowCoordinateBias = InShadowCoordinateBias;
	}

	/** Called by FTerrainComponentSceneProxy; repacks vertex and index buffers as needed. */
	UBOOL UpdateResources_RenderingThread(INT TessellationLevel, TArray<FDecalInteraction*>& ProxyDecals);

	// FDeferredCleanupInterface
	virtual void FinishCleanup()
	{
		delete this;
	}

	ATerrain* GetTerrain()
	{
		return TerrainComponent->GetTerrain();
	}

	const ATerrain* GetTerrain() const
	{
		return TerrainComponent->GetTerrain();
	}

	/** Adds a decal interaction to the game object. */
	void AddDecalInteraction_RenderingThread(FDecalInteraction& DecalInteraction, UINT ProxyMaxTesellation);
	void GenerateDecalRenderData(class FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const;

	// allow access to mesh component
	friend class UTerrainComponent;
	friend class FDynamicTerrainData;
	friend class FTerrainComponentSceneProxy;
	friend struct FTerrainIndexBuffer;
	template<typename TerrainQuadRelevance> friend struct FTerrainTessellationIndexBuffer;
	friend struct FTerrainDetessellationIndexBuffer;

	void CalcLOD(const FSceneView* View);

protected:
	/** Set to TRUE in InitResources() and FALSE in ReleaseResources()	*/
	UBOOL					bIsInitialized;
	UBOOL					bRepackRequired;
	/** Morphing is enabled flag...										*/
	BYTE					MorphingFlags;
	/** The owner component												*/
	UTerrainComponent*		TerrainComponent;

	/** The component section size and base (may not need these...)		*/
#if 1	//@todo. Remove these as we depend on the component anyway!
	INT						ComponentSectionSizeX;
	INT						ComponentSectionSizeY;
	INT						ComponentSectionBaseX;
	INT						ComponentSectionBaseY;
	INT						ComponentTrueSectionSizeX;
	INT						ComponentTrueSectionSizeY;
#endif	//#if 1	//@todo. Remove these as we depend on the component anyway!
	INT						NumVerticesX;
	INT						NumVerticesY;
	/** The maximum tessellation level of the terrain					*/
	INT						MaxTessellationLevel;
	/** The minimum tessellation level of the terrain					*/
	INT						MinTessellationLevel;
	/** The editor-desired tessellation level to display at				*/
	INT						EditorTessellationLevel;
	FLOAT					TerrainHeightScale;
	FLOAT					TessellationDistanceScale;
	INT						LightMapResolution;
	FVector2D				ShadowCoordinateScale;
	FVector2D				ShadowCoordinateBias;
	/** Copy of the ATerrain::NumPatchesX. */
	INT						NumPatchesX;
	/** Copy of the ATerrain::NumPatchesY. */
	INT						NumPatchesY;

	/** The TessellationLevels arrays (per-batch)						*/
	BYTE*								TessellationLevels;

	/** The parent scale factors... */
	FLOAT					ScaleFactorX;
	FLOAT					ScaleFactorY;

	/** Offset for layer UVs in LocalPosition space */
	FVector					LayerCoordinateOffset;

	/** The vertex factory												*/
	FTerrainVertexFactory*				VertexFactory;
	/** The decal vertex factory										*/
	FTerrainDecalVertexFactoryBase*		DecalVertexFactory;
	/** The vertex buffer containing the vertices for the component		*/
	FTerrainVertexBuffer*				VertexBuffer;
	/** The index buffers for each batch material						*/
	TerrainTessellationIndexBufferType*	SmoothIndexBuffer;
	/** The material resources for each batch							*/
	TArray<FMaterialRenderProxy*>		BatchMaterialResources;

	void RepackDecalIndexBuffers_RenderingThread(INT TessellationLevel, INT MaxTessellation, TArray<FDecalInteraction*>& Decals);
	void ReinitDecalResources_RenderThread();
};

//
//	FTerrainIndexBuffer
//
struct FTerrainIndexBuffer: FIndexBuffer
{
	const FTerrainObject* TerrainObject;
	INT	SectionSizeX;
	INT	SectionSizeY;
	INT NumVisibleTriangles;

	// Constructor.
	FTerrainIndexBuffer(const FTerrainObject* InTerrainObject) :
		  TerrainObject(InTerrainObject)
		, SectionSizeX(InTerrainObject->GetComponentSectionSizeX())
		, SectionSizeY(InTerrainObject->GetComponentSectionSizeY())
		, NumVisibleTriangles(INDEX_NONE)
	{
	}

	// FRenderResource interface.
	virtual void InitRHI();

	virtual FString GetFriendlyName() const
	{
		return TEXT("Terrain component indices (full batch)");
	}
};

//
//	FTerrainComponentSceneProxy
//
class FTerrainComponentSceneProxy : public FPrimitiveSceneProxy
{
private:
	class FTerrainComponentInfo : public FLightCacheInterface
	{
	public:

		/** Initialization constructor. */
		FTerrainComponentInfo(const UTerrainComponent& Component)
		{
			// Build the static light interaction map.
			for (INT LightIndex = 0; LightIndex < Component.IrrelevantLights.Num(); LightIndex++)
			{
				StaticLightInteractionMap.Set(Component.IrrelevantLights(LightIndex), FLightInteraction::Irrelevant());
			}
			
			LightMap = Component.LightMap;
			if (LightMap)
			{
				for (INT LightIndex = 0; LightIndex < LightMap->LightGuids.Num(); LightIndex++)
				{
					StaticLightInteractionMap.Set(LightMap->LightGuids(LightIndex), FLightInteraction::LightMap());
				}
			}

			for (INT LightIndex = 0; LightIndex < Component.ShadowMaps.Num(); LightIndex++)
			{
				UShadowMap2D* ShadowMap = Component.ShadowMaps(LightIndex);
				if (ShadowMap && ShadowMap->IsValid())
				{
					StaticLightInteractionMap.Set(
						ShadowMap->GetLightGuid(),
						FLightInteraction::ShadowMap2D(
							ShadowMap->GetTexture(),
							ShadowMap->GetCoordinateScale(),
							ShadowMap->GetCoordinateBias(),
							ShadowMap->IsShadowFactorTexture()
							)
						);

					Component.TerrainObject->SetShadowCoordinateBias(ShadowMap->GetCoordinateBias());
					Component.TerrainObject->SetShadowCoordinateScale(ShadowMap->GetCoordinateScale());
				}
			}
		}

		// FLightCacheInterface.
		virtual FLightInteraction GetInteraction(const FLightSceneInfo* LightSceneInfo) const
		{
			// Check for a static light interaction.
			const FLightInteraction* Interaction = StaticLightInteractionMap.Find(LightSceneInfo->LightmapGuid);
			if (!Interaction)
			{
				Interaction = StaticLightInteractionMap.Find(LightSceneInfo->LightGuid);
			}
			return Interaction ? *Interaction : FLightInteraction::Uncached();
		}

		virtual FLightMapInteraction GetLightMapInteraction() const
		{
			return LightMap ? LightMap->GetInteraction() : FLightMapInteraction();
		}

	private:
		/** A map from persistent light IDs to information about the light's interaction with the model element. */
		TMap<FGuid,FLightInteraction> StaticLightInteractionMap;

		/** The light-map used by the element. */
		const FLightMap* LightMap;
	};

	/** */
	struct FTerrainBatchInfo
	{
		FTerrainBatchInfo(UTerrainComponent* Component, INT BatchIndex);
		~FTerrainBatchInfo();

        FMaterialRenderProxy* MaterialRenderProxy;
		UBOOL bIsTerrainMaterialResourceInstance;
		TArray<UTexture2D*> WeightMaps;
	};

	struct FTerrainMaterialInfo
	{
		FTerrainMaterialInfo(UTerrainComponent* Component);
		~FTerrainMaterialInfo();
		
		FTerrainBatchInfo BatchInfo;
		FTerrainComponentInfo* ComponentLightInfo;
	};

public:
	/** Initialization constructor. */
	FTerrainComponentSceneProxy(UTerrainComponent* Component, FLOAT InCheckTessellationDistance, WORD InCheckTessellationOffset = 0);
	virtual ~FTerrainComponentSceneProxy();

	/**
	 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
	 */
	virtual void AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction);

	// FPrimitiveSceneProxy interface.
	
	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	/**
	* Draws the primitive's dynamic decal elements.  This is called from the rendering thread for each frame of each view.
	* The dynamic elements will only be rendered if GetViewRelevance declares dynamic relevance.
	* Called in the rendering thread.
	*
	* @param	PDI						The interface which receives the primitive elements.
	* @param	View					The view which is being rendered.
	* @param	InDepthPriorityGroup	The DPG which is being rendered.
	* @param	bDynamicLightingPass	TRUE if drawing dynamic lights, FALSE if drawing static lights.
	* @param	bDrawOpaqueDecals		TRUE if we want to draw opaque decals
	* @param	bDrawTransparentDecals	TRUE if we want to draw transparent decals
	* @param	bTranslucentReceiverPass	TRUE during the decal pass for translucent receivers, FALSE for opaque receivers.
	*/
	virtual void DrawDynamicDecalElements(
		FPrimitiveDrawInterface* PDI,
		const FSceneView* View,
		UINT InDepthPriorityGroup,
		UBOOL bDynamicLightingPass,
		UBOOL bDrawOpaqueDecals,
		UBOOL bDrawTransparentDecals,
		UBOOL bTranslucentReceiverPass
		);

	/**
	* Draws the primitive's static decal elements.  This is called from the game thread whenever this primitive is attached
	* as a receiver for a decal.
	*
	* The static elements will only be rendered if GetViewRelevance declares both static and decal relevance.
	* Called in the game thread.
	*
	* @param PDI - The interface which receives the primitive elements.
	*/
	virtual void DrawStaticDecalElements(
		FStaticPrimitiveDrawInterface* PDI,
		const FDecalInteraction& DecalInteraction
		);

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Determines the relevance of this primitive's elements to the given light.
	 *	@param	LightSceneInfo			The light to determine relevance for
	 *	@param	bDynamic (output)		The light is dynamic for this primitive
	 *	@param	bRelevant (output)		The light is relevant for this primitive
	 *	@param	bLightMapped (output)	The light is light mapped for this primitive
	 */
	virtual void GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const;

	/**
	 *	Helper function for determining if a given view requires a tessellation check based on distance.
	 *
	 *	@param	View		The view of interest.
	 *	@return	UBOOL		TRUE if it does, FALSE if it doesn't.
	 */
	UBOOL CheckViewDistance(const FSceneView* View, const FVector& Position, const FVector& MaxMinusMin, const FLOAT ComponentSize);
	/**
	 *	Helper function for calculating the tessellation for a given view.
	 *
	 *	@param	View		The view of interest.
	 *	@param	Terrain		The terrain of interest.
	 *	@param	bFirstTime	The first time this call was made in a frame.
	 */
	void ProcessPreRenderView(const FSceneView* View, ATerrain* Terrain, UBOOL bFirstTime);

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber);

	void UpdateData(UTerrainComponent* Component);
	void UpdateData_RenderThread(FTerrainMaterialInfo* NewMaterialInfo);

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	/** @return			Cached value of MaxTessellationLevel, as computed in DrawDynamicElements. */
	UINT GetMaxTessellation() const
	{
		return MaxTessellation;
	}

protected:
	AActor* GetOwner();

private:
	/** Counter to determine when to check the tessellation */
	INT CheckTessellationCounter;
	/** Random offset to check tessellation */
	INT CheckTessellationOffset;
	/** The last frame that the tessellation check was performed */
	INT LastTessellationCheck;
	/**
	 *	The radius from the view origin that terrain tessellation checks should be performed.
	 *	If 0.0, every component will be checked for tessellation changes each frame.
	 */
	FLOAT CheckTessellationDistance;

	/**
	 *	To catch when visibility changes...
	 */
	FLOAT TrackedLastVisibilityChangeTime;

	AActor* Owner;
	UTerrainComponent* ComponentOwner;

	FTerrainObject* TerrainObject;

	FLinearColor LinearLevelColor;
	FLinearColor LinearPropertyColor;

	FLOAT CullDistance;

	BITFIELD bCastShadow : 1;
	/** TRUE once this scene proxy has been added to the scene */
	BITFIELD bInitialized : 1;

	FTerrainMaterialInfo*	CurrentMaterialInfo;

	/** Cache of MaxTessellationLevel, as computed in DrawDynamicElements. */
	UINT MaxTessellation;

	/** Array of meshes, one for each batch material.  Populated by DrawDynamicElements. */
	TArray<FMeshBatch> Meshes;
};

#endif	//#ifndef TERRAIN_RENDER_HEADER
