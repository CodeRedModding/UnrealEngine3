/*=============================================================================
LandscapeRender.h: New terrain rendering
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _LANDSCAPERENDER_H
#define _LANDSCAPERENDER_H

class FLandscapeVertexFactory;
class FLandscapeVertexBuffer;
class FLandscapeComponentSceneProxy;
#include "../Src/ScenePrivate.h"

#define LANDSCAPE_NEIGHBOR_NUM	4

#define LANDSCAPE_LOD_LEVELS 8
// Do we need this define?
#define LANDSCAPE_MAX_SUBSECTION_NUM 2


#if WITH_EDITOR
namespace ELandscapeViewMode
{
	enum Type
	{
		Invalid = -1,
		/** Color only */
		Normal = 0,
		EditLayer,
		/** Layer debug only */
		DebugLayer,
		LayerDensity,
		LOD,
	};
}

extern ELandscapeViewMode::Type GLandscapeViewMode;

namespace ELandscapeEditRenderMode
{
	enum Type
	{
		None = 0,
		Gizmo = 1,
		SelectRegion = 2,
		SelectComponent = 4,
		Select = SelectRegion | SelectComponent,
		Mask = 8, 
		InvertedMask = 16, // Should not be overlapped with other bits 
		BitMaskForMask = Mask | InvertedMask,

	};
}

extern INT GLandscapeEditRenderMode;

extern INT GLandscapePreviewMeshRenderMode;

extern UMaterialInstanceConstant* GMaskRegionMaterial;
#endif

#if PS3
//
// FPS3LandscapeHeightVertexBuffer
//
class FPS3LandscapeHeightVertexBuffer : public FVertexBuffer
{
public:
	/** Constructor. */
	FPS3LandscapeHeightVertexBuffer()
	{
		bInitialized = TRUE;
	}

	/** Destructor. */
	virtual ~FPS3LandscapeHeightVertexBuffer()
	{
	}

	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI()
	{
	}
};

#endif

/** vertex factory for VTF-heightmap terrain  */
class FLandscapeVertexFactory : public FVertexFactory, public FRefCountedObject
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeVertexFactory);

public:

	FLandscapeVertexFactory()
	{
	}

	virtual ~FLandscapeVertexFactory()
	{
		ReleaseResource();
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	struct DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;
#if PS3
		/** The stream created by the SPU code on PS3 */
		FVertexStreamComponent PS3HeightComponent;
#endif
	};

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory? 
	*/
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
#if MOBILE
		return FALSE;
#endif

		// only compile landscape materials for landscape vertex factory
		// The special engine materials must be compiled for the landscape vertex factory because they are used with it for wireframe, etc.
		return (Platform==SP_PCD3D_SM3 || Platform==SP_PCD3D_SM4 || Platform==SP_PCD3D_SM5 || Platform==SP_PCOGL || Platform==SP_XBOXD3D || Platform==SP_PS3) && (Material->IsUsedWithLandscape() || Material->IsSpecialEngineMaterial()); // && !Material->IsUsedWithDecals();
	}

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("PER_PIXEL_TANGENT_BASIS"),TEXT("1"));
	}

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FLandscapeVertexFactory& Other);

	/**
	* Vertex factory interface for creating a corresponding decal vertex factory
	* Copies the data from this existing vertex factory.
	*
	* @return new allocated decal vertex factory
	*/
	virtual class FDecalVertexFactoryBase* CreateDecalVertexFactory() const;

	// FRenderResource interface.
	virtual void InitRHI();

	static UBOOL SupportsTessellationShaders() { return TRUE; }

#if !PS3
	virtual void GetStaticBatchElementVisibility( const class FSceneView& View, const struct FMeshBatch* Batch, TArray<INT>& BatchesToRender ) const;
#endif

	/** stream component data bound to this vertex factory */
	DataType Data;  

#if PS3
	static FPS3LandscapeHeightVertexBuffer GPS3LandscapeHeightVertexBuffer;
#endif
};

/** Decal vertex factory */
class FLandscapeDecalVertexFactory : public FDecalVertexFactoryBase, public FLandscapeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeDecalVertexFactory);

public:
	typedef FLandscapeVertexFactory Super;

	FLandscapeDecalVertexFactory() : FDecalVertexFactoryBase(), FLandscapeVertexFactory()
	{
	}

	/**
	 * Should we cache the material's shader type on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("DECAL_FACTORY"),TEXT("1"));
		// decals always need WORLD_COORD usage in order to pass 2x2 matrix for normal transform
		// using the color interpolators used by WORLD_COORDS
		OutEnvironment.Definitions.Set(TEXT("WORLD_COORDS"),TEXT("1"));
	}

	/** Must match the value of the DECAL_FACTORY define */
	virtual UBOOL IsDecalFactory() const { return TRUE; }

	virtual FVertexFactory* CastToFVertexFactory()
	{
		return static_cast<FVertexFactory*>( this );
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	const FVector& GetDecalLocalNormal()
	{
		return DecalLocalNormal;
	}
};

//
// FLandscapeVertexBuffer
//
class FLandscapeVertexBuffer : public FVertexBuffer, public FRefCountedObject
{
	INT SizeVerts;
public:
	struct FLandscapeVertex
	{
		FLOAT VertexX;
		FLOAT VertexY;
	};
	
	/** Constructor. */
	FLandscapeVertexBuffer(INT InSizeVerts)
	: 	SizeVerts(InSizeVerts)
	{
		InitResource(); // BeginInitResource(this);
	}

	/** Destructor. */
	virtual ~FLandscapeVertexBuffer()
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
class FLandscapeIndexBuffer : public FRawStaticIndexBuffer, public FRefCountedObject
{
public:
	FLandscapeIndexBuffer(INT SizeQuads, INT VBSizeVertices);

	/** Destructor. */
	virtual ~FLandscapeIndexBuffer()
	{
		ReleaseResource();
	}
};

//
// FLandscapeSubRegionIndexBuffer
//
class FLandscapeSubRegionIndexBuffer : public FRawStaticIndexBuffer
{
public:
	FLandscapeSubRegionIndexBuffer();

	/** Destructor. */
	virtual ~FLandscapeSubRegionIndexBuffer()
	{
		ReleaseResource();
	}

	void AddSubsection(TArray<WORD>& NewIndices, INT MinX, INT MinY, INT MaxX, INT MaxY, INT VBSizeVertices);

	void Finalize(TArray<WORD>& NewIndices);

	inline INT GetTriCount(INT SubIndex) 
	{ 
		if (SubIndex >= 0 && SubIndex < NumSubIndex)
		{
			return TriCount[SubIndex]; 
		}
		return 0;
	}
private:
	INT TriCount[LANDSCAPE_MAX_SUBSECTION_NUM*LANDSCAPE_MAX_SUBSECTION_NUM];
	INT NumSubIndex;
};

//
// FLandscapeDecalIndexBuffers
//
class FLandscapeDecalIndexBuffers
{
public:
	FLandscapeDecalIndexBuffers(INT MinX[], INT MinY[], INT MaxX[], INT MaxY[], INT NumSubsections, INT SubSectionSize);
	/** Destructor. */
	virtual ~FLandscapeDecalIndexBuffers();
	void InitResources();

	FLandscapeSubRegionIndexBuffer* LODIndexBuffers[LANDSCAPE_LOD_LEVELS];

	inline INT GetTotalIndexNum() { return NumTotalIndex; }
	inline INT GetStartIndex(INT SubIndex, INT LODIndex)
	{
		if (SubIndex >= 0 && SubIndex < NumSubIndex
			&& LODIndex >= 0 && LODIndex < LANDSCAPE_LOD_LEVELS)
		{
			return StartIndex[SubIndex][LODIndex];
		}
		return 0;
	}
private:
	INT NumTotalIndex;
	INT StartIndex[LANDSCAPE_MAX_SUBSECTION_NUM*LANDSCAPE_MAX_SUBSECTION_NUM][LANDSCAPE_LOD_LEVELS];
	INT NumSubIndex;
};

//
// FLandscapeEditToolRenderData
//
struct FLandscapeEditToolRenderData
{
	enum
	{
		ST_NONE = 0,
		ST_COMPONENT = 1,
		ST_REGION = 2,
		// = 4...
	};
	FLandscapeEditToolRenderData()
	:	ToolMaterial(NULL),
		GizmoMaterial(NULL),
		LandscapeComponent(NULL),
		SelectedType(ST_NONE),
		DebugChannelR(INDEX_NONE),
		DebugChannelG(INDEX_NONE),
		DebugChannelB(INDEX_NONE),
		DataTexture(NULL)
	{}

	FLandscapeEditToolRenderData(ULandscapeComponent* InComponent)
		:	ToolMaterial(NULL),
			GizmoMaterial(NULL),
			LandscapeComponent(InComponent),
			SelectedType(ST_NONE),
			DebugChannelR(INDEX_NONE),
			DebugChannelG(INDEX_NONE),
			DebugChannelB(INDEX_NONE),
			DataTexture(NULL)
	{}

	// Material used to render the tool.
	UMaterialInterface* ToolMaterial;
	// Material used to render the gizmo selection region...
	UMaterialInterface* GizmoMaterial;

	ULandscapeComponent* LandscapeComponent;

	// Component is selected
	INT SelectedType;
	INT DebugChannelR, DebugChannelG, DebugChannelB;
	UTexture2D* DataTexture; // Data texture other than height/weight

#if WITH_EDITOR
	void UpdateDebugColorMaterial();
	void UpdateSelectionMaterial(INT InSelectedType);
#endif

	// Game thread update
	void Update( UMaterialInterface* InNewToolMaterial )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateEditToolRenderData,
			FLandscapeEditToolRenderData*, LandscapeEditToolRenderData, this,
			UMaterialInterface*, NewToolMaterial, InNewToolMaterial,
		{
			LandscapeEditToolRenderData->ToolMaterial = NewToolMaterial;
		});
	}

	void UpdateGizmo( UMaterialInterface* InNewGizmoMaterial )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateEditToolRenderData,
			FLandscapeEditToolRenderData*, LandscapeEditToolRenderData, this,
			UMaterialInterface*, NewGizmoMaterial, InNewGizmoMaterial,
		{
			LandscapeEditToolRenderData->GizmoMaterial = NewGizmoMaterial;
		});
	}

	// Allows game thread to queue the deletion by the render thread
	void Cleanup()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			CleanupEditToolRenderData,
			FLandscapeEditToolRenderData*, LandscapeEditToolRenderData, this,
		{
			delete LandscapeEditToolRenderData;
		});
	}
};

struct FLandscapeSubsectionParams
{
	FMatrix					LocalToWorld;
	FMatrix					WorldToLocal;
	FMatrix					LocalToWorldNoScaling;
	FMatrix					WorldToLocalNoScaling;
	FVector4				HeightmapUVScaleBias;
	FVector4				WeightmapUVScaleBias;
	FVector4				LandscapeLightmapScaleBias;
	FVector4				SubsectionSizeVertsLayerUVPan;
};

struct FLandscapeBatchElementParams
{
	const FLandscapeSubsectionParams* SubsectionParam;

	// LOD calculation-related params
	const class FLandscapeComponentSceneProxy* SceneProxy;
	INT SubX;
	INT SubY;
	INT	CurrentLOD;
};

//
// FLandscapeComponentSceneProxy
//
class FLandscapeComponentSceneProxy : public FPrimitiveSceneProxy
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
		/** A map from persistent light IDs to information about the light's interaction with the model element. */
		//TMap<FGuid,FLightInteraction> StaticLightInteractionMap;

		/** The light-map used by the element. */
		const ULandscapeComponent* Component;
	};

	INT						MaxLOD;
	INT						ComponentSizeQuads;	// Size of component in quads
	INT						NumSubsections;
	INT						SubsectionSizeQuads;
	INT						SubsectionSizeVerts;
	INT						SectionBaseX;
	INT						SectionBaseY;
	FLOAT					DrawScaleXY;
	FVector					ActorOrigin;
	FLOAT					StaticLightingResolution;

	// Storage for static draw list batch params
	TArray<FLandscapeBatchElementParams> StaticBatchParamArray;
	TArray<FLandscapeSubsectionParams> SubsectionParams;

	// Precomputed
	FLOAT					LODDistance;
	FLOAT					LODDistanceFactor;
	FLOAT					DistDiff;

	// Values for light-map
	INT						PatchExpandCount;

	FVector4 WeightmapScaleBias;
	FLOAT WeightmapSubsectionOffset;

	UTexture2D* HeightmapTexture;
	FVector4 HeightmapScaleBias;
	FLOAT HeightmapSubsectionOffsetU;
	FLOAT HeightmapSubsectionOffsetV;

	FLandscapeVertexFactory* VertexFactory;
	FLandscapeVertexBuffer* VertexBuffer;
	FLandscapeIndexBuffer** IndexBuffers;
	
	UMaterialInterface* MaterialInterface;
	FMaterialViewRelevance MaterialViewRelevance;

	// Reference counted vertex buffer shared among all landscape scene proxies
	static FLandscapeVertexBuffer* SharedVertexBuffer;
	static FLandscapeIndexBuffer** SharedIndexBuffers;

	FLandscapeEditToolRenderData* EditToolRenderData;

	// FLightCacheInterface
	FLandscapeLCI* ComponentLightInfo;

	FLinearColor LevelColor;

#if !PS3 // Not support for PS3 for now
	FVector2D				NeighborPosition[LANDSCAPE_NEIGHBOR_NUM];
	INT						ForcedLOD;
	INT						LODBias;
	BYTE					ForcedNeighborLOD[LANDSCAPE_NEIGHBOR_NUM];
	BYTE					NeighborLODBias[LANDSCAPE_NEIGHBOR_NUM];
#else
	// for PS3
	FVector4 LodDistancesValues;
	void* PlatformData;
#endif

	virtual ~FLandscapeComponentSceneProxy();

public:
	// Reference counted vertex buffer shared among all landscape scene proxies and decal
	static FLandscapeVertexFactory* SharedVertexFactory;

	FLandscapeComponentSceneProxy(ULandscapeComponent* InComponent, FLandscapeEditToolRenderData* InEditToolRenderData);

	// FPrimitiveSceneProxy interface.

	/**
	 * Adds a decal interaction to the primitive.  This is called in the rendering thread by AddDecalInteraction_GameThread.
	 */
	virtual void AddDecalInteraction_RenderingThread(const FDecalInteraction& DecalInteraction);
	/**
	 * Removes a decal interaction from the primitive.  This is called in the rendering thread by RemoveDecalInteraction_GameThread.
	 */
	virtual void RemoveDecalInteraction_RenderingThread(UDecalComponent* DecalComponent);

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

	/**
	 * Draws the primitive's static elements.  This is called from the game thread once when the scene proxy is created.
	 * The static elements will only be rendered if GetViewRelevance declares static relevance.
	 * Called in the game thread.
	 * @param PDI - The interface which receives the primitive elements.
	 */
#if !PS3
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI);
#endif

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
	*	Called when the rendering thread adds the proxy to the scene.
	*	This function allows for generating renderer-side resources.
	*/
	virtual UBOOL CreateRenderThreadResources();

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }

	UBOOL HasRelevantStaticDecals(const FSceneView* View) const;
	UBOOL HasRelevantDynamicDecals(const FSceneView* View) const;
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

	// FLandscapeComponentSceneProxy interface.
#if !PS3
	INT CalcLODForSubsection(INT SubX, INT SubY, const FVector2D& CameraLocalPos) const;
	INT CalcLODForSubsectionNoForced(INT SubX, INT SubY, const FVector2D& CameraLocalPos) const;
	void CalcLODParamsForSubsection(const class FSceneView& View, INT SubX, INT SubY, FLOAT& OutfLOD, FVector4& OutNeighborLODs) const;
	void GetStaticBatchElementVisibility( const class FSceneView& View, const struct FMeshBatch* Batch, TArray<INT>& BatchesToRender ) const;

	// FLandcapeSceneProxy
	void ChangeLODDistanceFactor_RenderThread(FVector2D InLODDistanceFactors);
#endif

	friend class FLandscapeVertexFactoryShaderParameters;
	friend class FLandscapeVertexFactoryPixelShaderParameters;
};

class FLandscapeDebugMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* RedTexture;
	const UTexture2D* GreenTexture;
	const UTexture2D* BlueTexture;
	const FLinearColor R;
	const FLinearColor G;
	const FLinearColor B;

	/** Initialization constructor. */
	FLandscapeDebugMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* TexR, const UTexture2D* TexG, const UTexture2D* TexB, 
		const FLinearColor& InR, const FLinearColor& InG, const FLinearColor& InB ):
			Parent(InParent),
			RedTexture(TexR),
			GreenTexture(TexG),
			BlueTexture(TexB),
			R(InR),
			G(InG),
			B(InB)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const
	{
		return Parent->GetMaterial();
	}
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == NAME_Landscape_RedMask )
		{
			*OutValue = R;
			return TRUE;
		}
		else if (ParameterName == NAME_Landscape_GreenMask )
		{
			*OutValue = G;
			return TRUE;
		}
		else if (ParameterName == NAME_Landscape_BlueMask )
		{
			*OutValue = B;
			return TRUE;
		}
		else
		{
			return Parent->GetVectorValue(ParameterName, OutValue, Context);
		}
	}
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == NAME_Landscape_RedTexture )
		{
			*OutValue = (RedTexture ? RedTexture->Resource : GBlackTexture);
			return TRUE;
		}
		else if (ParameterName == NAME_Landscape_GreenTexture )
		{
			*OutValue = (GreenTexture ? GreenTexture->Resource : GBlackTexture);
			return TRUE;
		}
		else if (ParameterName == NAME_Landscape_BlueTexture )
		{
			*OutValue = (BlueTexture ? BlueTexture->Resource : GBlackTexture);
			return TRUE;
		}
		else
		{
			return Parent->GetTextureValue(ParameterName, OutValue, Context);
		}
	}
};

class FLandscapeSelectMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* SelectTexture;

	/** Initialization constructor. */
	FLandscapeSelectMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* InTexture):
		Parent(InParent),
		SelectTexture(InTexture)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const
	{
		return Parent->GetMaterial();
	}
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("HighlightColor")) )
		{
			*OutValue = FLinearColor(1.f, 0.5f, 0.5f);
			return TRUE;
		}
		else
		{
			return Parent->GetVectorValue(ParameterName, OutValue, Context);
		}
	}
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("SelectedData")) )
		{
			*OutValue = (SelectTexture ? SelectTexture->Resource : GBlackTexture);
			return TRUE;
		}
		else
		{
			return Parent->GetTextureValue(ParameterName, OutValue, Context);
		}
	}
};

class FLandscapeMaskMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* SelectTexture;
	const UBOOL bInverted;

	/** Initialization constructor. */
	FLandscapeMaskMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* InTexture, const UBOOL InbInverted):
		Parent(InParent),
		SelectTexture(InTexture),
		bInverted(InbInverted)
	{}

	// FMaterialRenderProxy interface.
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
		if (ParameterName == FName(TEXT("bInverted")) )
		{
			*OutValue = bInverted;
			return TRUE;
		}
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("SelectedData")) )
		{
			*OutValue = (SelectTexture ? SelectTexture->Resource : GBlackTexture);
			return TRUE;
		}
		else
		{
			return Parent->GetTextureValue(ParameterName, OutValue, Context);
		}
	}
};

namespace 
{
	static FLOAT GetTerrainExpandPatchCount(FLOAT LightMapRes, INT& X, INT& Y, INT ComponentSize, INT LightmapSize, INT& DesiredSize)
	{
		if (LightMapRes <= 0) return 0.f;
		// Assuming DXT_1 compression at the moment...
		INT PixelPaddingX = GPixelFormats[PF_DXT1].BlockSizeX;
		INT PixelPaddingY = GPixelFormats[PF_DXT1].BlockSizeY;
/*
		if (GAllowLightmapCompression == FALSE)
		{
			PixelPaddingX = GPixelFormats[PF_A8R8G8B8].BlockSizeX;
			PixelPaddingY = GPixelFormats[PF_A8R8G8B8].BlockSizeY;
		}
*/
		INT PatchExpandCountX = (LightMapRes >= 1.f) ? (PixelPaddingX) / LightMapRes : (PixelPaddingX);
		INT PatchExpandCountY = (LightMapRes >= 1.f )? (PixelPaddingY) / LightMapRes : (PixelPaddingY);

		X = Max<INT>(1, PatchExpandCountX);
		Y = Max<INT>(1, PatchExpandCountY);

		DesiredSize = (LightMapRes >= 1.f) ? Min<INT>((INT)((ComponentSize + 1) * LightMapRes), 4096) : Min<INT>((INT)((LightmapSize) * LightMapRes), 4096);
		INT CurrentSize = (LightMapRes >= 1.f) ? Min<INT>((INT)((2*X + ComponentSize + 1) * LightMapRes), 4096) : Min<INT>((INT)((2*X + LightmapSize) * LightMapRes), 4096);

		// Find proper Lightmap Size
		if (CurrentSize > DesiredSize)
		{
			// Find maximum bit
			INT PriorSize = DesiredSize;
			while (DesiredSize > 0)
			{
				PriorSize = DesiredSize;
				DesiredSize = DesiredSize & ~(DesiredSize & ~(DesiredSize-1));
			}

			DesiredSize = PriorSize << 1; // next bigger size
			if ( CurrentSize * CurrentSize <= ((PriorSize * PriorSize) << 1)  )
			{
				DesiredSize = PriorSize;
			}
		}

		INT DestSize = (FLOAT)DesiredSize / CurrentSize * (ComponentSize*LightMapRes);
		FLOAT LightMapRatio = (FLOAT)DestSize / (ComponentSize*LightMapRes) * CurrentSize / DesiredSize;
		return LightMapRatio;
		//X = Y = 1;
		//return 1.0f;
	}
}

#endif // _LANDSCAPERENDER_H