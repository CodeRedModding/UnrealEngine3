/*=============================================================================
LandscapeRenderMobile.h: Mobile landscape rendering
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _LANDSCAPERENDERMOBILE_H
#define _LANDSCAPERENDERMOBILE_H

class FLandscapeComponentSceneProxyMobile;

struct FLandscapeMobileVertex
{
	BYTE Position[4];
	BYTE NextMipPosition[4];
	FPackedNormal Normal;
};

/** vertex factory for mobile landscape rendering  */
class FLandscapeVertexFactoryMobile : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeVertexFactoryMobile);
public:

	FLandscapeVertexFactoryMobile(FLandscapeComponentSceneProxyMobile* InSceneProxy)
	:	SceneProxy(InSceneProxy)
	{
	}

	virtual ~FLandscapeVertexFactoryMobile()
	{
		ReleaseResource();
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory? 
	*/
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		//!! should only compile if this is a mobile emulation material!
		return (Platform==SP_PCD3D_SM3 || Platform==SP_PCD3D_SM5) && (Material->IsUsedWithMobileLandscape() || Material->IsSpecialEngineMaterial());
	}

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("IS_LANDSCAPE_SIMMOBILE"),TEXT("1"));
	}

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FLandscapeVertexFactory& Other);

	void GetStaticBatchElementVisibility( const class FSceneView& View, const struct FMeshBatch* Batch, TArray<INT>& BatchesToRender ) const;

	FLandscapeComponentSceneProxyMobile* SceneProxy;
	INT LOD;
};

//
// FLandscapeVertexBuffer
//
class FLandscapeVertexBufferMobile : public FVertexBuffer
{
	void* Data;
	INT DataSize;
	INT SkipDataSize;
public:

	/** Constructor. */
	FLandscapeVertexBufferMobile(void* InData, INT InDataSize, INT InSkipDataSize)
	: 	Data(InData)
	,	DataSize(InDataSize)
	,	SkipDataSize(InSkipDataSize)
	{
		InitResource();
	}

	/** Destructor. */
	virtual ~FLandscapeVertexBufferMobile()
	{
		ReleaseResource();
	}

	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI();
};

//
// FLandscapeIndexBuffer
//
class FLandscapeIndexBufferMobile : public FRawStaticIndexBuffer, public FRefCountedObject
{
	QWORD CachedKey;
public:
	// destructor
	virtual ~FLandscapeIndexBufferMobile();

	static FLandscapeIndexBufferMobile* GetLandscapeIndexBufferMobile(INT SubsectionSizeQuads, INT NumSubsections, INT StartVertexIndex);

private:
	// constructor
	FLandscapeIndexBufferMobile(QWORD Key, INT SubsectionSizeQuads, INT NumSubsections, INT StartVertexIndex);

	static QWORD GetKey(INT SubsectionSizeQuads, INT NumSubsections, INT StartVertexIndex);
	static TMap<QWORD, FLandscapeIndexBufferMobile*> SharedIndexBufferMap;
};


//
// Material render proxy to override alphamap parameter
//
class FLandscapeMobileMaterialRenderProxy : public FMaterialRenderProxy
{
	FMaterialRenderProxy* Parent;
	FLandscapeComponentSceneProxyMobile* SceneProxy;
public:
	FLandscapeMobileMaterialRenderProxy(FMaterialRenderProxy* InParent, FLandscapeComponentSceneProxyMobile* InSceneProxy)
		:	Parent(InParent)
		,	SceneProxy(InSceneProxy)
	{}
	virtual const class FMaterial* GetMaterial() const
	{
		return Parent->GetMaterial();
	}
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		static FName IsLandscape(TEXT("IS_LANDSCAPE"));
		if( ParameterName == IsLandscape )
		{
			*OutValue = 1.f;
			return TRUE;
		}

		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const;
	virtual FLOAT GetDistanceFieldPenumbraScale() const
	{
		return Parent->GetDistanceFieldPenumbraScale();
	}
	virtual UBOOL IsSelected() const
	{ 
		return Parent->IsSelected();
	}
	virtual UBOOL IsHovered() const
	{ 
		return Parent->IsHovered();
	}

#if WITH_MOBILE_RHI
	virtual FTexture* GetMobileTexture(const INT MobileTextureUnit) const;
	virtual void FillMobileMaterialVertexParams (FMobileMaterialVertexParams& OutVertexParams) const
	{
		return Parent->FillMobileMaterialVertexParams( OutVertexParams );
	}
	virtual void FillMobileMaterialPixelParams (FMobileMaterialPixelParams& OutPixelParams) const
	{ 
		return Parent->FillMobileMaterialPixelParams( OutPixelParams );
	}
#endif
}; 

struct FLandscapeMobileBatchParams
{
	const FLandscapeComponentSceneProxyMobile* SceneProxy;
	INT BiasedLodIndex;
	FVector4 LodParameters;
		
	FLandscapeMobileBatchParams(const FLandscapeComponentSceneProxyMobile* InSceneProxy, INT InBiasedLodIndex, const FVector4& InLodParameters)
	:	SceneProxy(InSceneProxy)
	,	BiasedLodIndex(InBiasedLodIndex)
	,	LodParameters(InLodParameters)
	{}
};

//
// FLandscapeComponentSceneProxy
//
class FLandscapeComponentSceneProxyMobile : public FPrimitiveSceneProxy
{
	class FLandscapeLCI : public FLightCacheInterface
	{
	public:
		/** Initialization constructor. */
		FLandscapeLCI(const ULandscapeComponent* InComponent)
		{
			Component = InComponent;
		}

		const ULandscapeComponent* GetLandscapeComponent()
		{
			return Component;
		}

		// FLightCacheInterface
		virtual FLightInteraction GetInteraction(const class FLightSceneInfo* LightSceneInfo) const
		{
			if( Component->IrrelevantLights.ContainsItem(LightSceneInfo->LightGuid) )
			{
				return FLightInteraction::Irrelevant();
			}

			if(Component->LightMap && Component->LightMap->LightGuids.ContainsItem(LightSceneInfo->LightmapGuid))
			{
				return FLightInteraction::LightMap();
			}

			for(INT LightIndex = 0;LightIndex < Component->ShadowMaps.Num();LightIndex++)
			{
				const UShadowMap2D* const ShadowMap = Component->ShadowMaps(LightIndex);
				if(ShadowMap && ShadowMap->IsValid() && ShadowMap->GetLightGuid() == LightSceneInfo->LightGuid)
				{
					return FLightInteraction::ShadowMap2D(
						ShadowMap->GetTexture(),
						ShadowMap->GetCoordinateScale(),
						ShadowMap->GetCoordinateBias(),
						ShadowMap->IsShadowFactorTexture()
						);
				}
			}

			return FLightInteraction::Uncached();
		}

		virtual FLightMapInteraction GetLightMapInteraction() const
		{
			if (Component->LightMap)
			{
				return Component->LightMap->GetInteraction();
			}
			else
			{
				return FLightMapInteraction();
			}
		}

	private:
		/** The light-map used by the element. */
		const ULandscapeComponent* Component;
	};

	INT						MaxLOD;
	INT						StaticLodBias;		// Mobile LOD bias set in the Lansdcape actor.
	INT						RuntimeLodBias;		// Device-specific LOD bias set in SystemSettings.
	INT						EffectiveLodBias;	// Sum of StaticLodBias and RuntimeLodBias

	INT						ComponentSizeQuads;	// Size of component in quads
	INT						NumSubsections;
	INT						SubsectionSizeQuads;
	INT						SubsectionSizeVerts;
	INT						SectionBaseX;
	INT						SectionBaseY;
	FMatrix					WorldToLocal;
	FLOAT					StaticLightingResolution;

	// Precomputed
	FLOAT					LODDistance;
	FLOAT					DistDiff;

	FLandscapeVertexFactoryMobile VertexFactory;
	FLandscapeVertexBufferMobile* VertexBuffer;
	TArray<FLandscapeIndexBufferMobile*> IndexBuffers;
	TArray<FLandscapeMobileBatchParams> BatchParameters;
	
	FMaterialViewRelevance MaterialViewRelevance;
	FLandscapeMobileMaterialRenderProxy* MaterialRenderProxy;

	// FLightCacheInterface
	FLandscapeLCI* ComponentLightInfo;

	// Cooked data
	void* PlatformData;
	UTexture2D* WeightTexture;
	UBOOL bNeedToDestroyPlatformData;

	FVector4 LightmapScaleBias;

	virtual ~FLandscapeComponentSceneProxyMobile();

public:
	// Reference counted vertex buffer shared among all landscape scene proxies and decal
	static FLandscapeVertexFactory* SharedVertexFactory;

	FLandscapeComponentSceneProxyMobile(ULandscapeComponent* InComponent);

	/**
	 * Draws the primitive's static elements.  This is called from the game thread once when the scene proxy is created.
	 * The static elements will only be rendered if GetViewRelevance declares static relevance.
	 * Called in the game thread.
	 * @param PDI - The interface which receives the primitive elements.
	 */
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI);

#if WITH_EDITOR
	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);
#endif

	/**
	*	Called when the rendering thread adds the proxy to the scene.
	*	This function allows for generating renderer-side resources.
	*/
	virtual UBOOL CreateRenderThreadResources();

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }

	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Determines the relevance of this primitive's elements to the given light.
	 *	@param	LightSceneInfo			The light to determine relevance for
	 *	@param	bDynamic (output)		The light is dynamic for this primitive
	 *	@param	bRelevant (output)		The light is relevant for this primitive
	 *	@param	bLightMapped (output)	The light is light mapped for this primitive
	 */
	virtual void GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const;

	/**
	 * Called to notify the proxy when its transform has been updated.
	 * Called in the thread that owns the proxy; game or rendering.
	 */
	virtual void OnTransformChanged();

	void GetStaticBatchElementVisibility( const class FSceneView& View, const struct FMeshBatch* Batch, TArray<INT>& BatchesToRender ) const;

	friend class FLandscapeVertexFactoryMobileVertexShaderParameters;
	friend class FLandscapeMobileMaterialRenderProxy;
	friend class FLandscapeVertexBufferMobile;
};

#endif // _LANDSCAPERENDERMOBILE_H