/*=============================================================================
	D3DMeshShaders.h: Unreal Direct3D mesh shader definitions.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	ED3DVertexFactoryType
//

enum ED3DVertexFactoryType
{
	VFT_Local,
	VFT_Terrain,
	VFT_Foliage,
	VFT_Particle,
	VFT_ParticleSubUV, 
	VFT_Null
};

//
//	GetD3DVertexFactoryType
//

ED3DVertexFactoryType GetD3DVertexFactoryType(FResourceType* Type);

//
//	FD3DVertexFactoryParameters
//

struct FD3DVertexFactoryParameters
{
	// Destructor.

	virtual ~FD3DVertexFactoryParameters() {}

	// FD3DVertexFactoryParameters interface.

	virtual void Set(FVertexFactory* VertexFactory) = 0;
	virtual const TCHAR* GetShaderFilename() = 0;
};

//
//	FD3DLocalVertexFactoryParameters
//

struct FD3DLocalVertexFactoryParameters: FD3DVertexFactoryParameters
{
	// Constant register indices.

	FD3DShaderParameter	LocalToWorldParameter,
						WorldToLocalParameter;

	// Constructor.

	FD3DLocalVertexFactoryParameters(FD3DShader* Shader):
		LocalToWorldParameter(Shader,TEXT("LocalToWorld")),
		WorldToLocalParameter(Shader,TEXT("WorldToLocal"))
	{}

	// FD3DVertexFactoryParameters interface.

	virtual void Set(FVertexFactory* VertexFactory);
	virtual const TCHAR* GetShaderFilename() { return TEXT("LocalVertexFactory.hlsl"); }
};

//
//	FD3DTerrainVertexFactoryParameters
//

struct FD3DTerrainVertexFactoryParameters: FD3DVertexFactoryParameters
{
	// Constant register indices.

	FD3DShaderParameter	LocalToWorldParameter,
						InvMaxTesselationLevelParameter,
						InvTerrainSizeParameter,
						InvLightMapSizeParameter,
						InvSectionSizeParameter,
						ZScaleParameter,
						SectionBaseParameter;

	// Constructor.

	FD3DTerrainVertexFactoryParameters(FD3DShader* Shader):
		LocalToWorldParameter(Shader,TEXT("LocalToWorld")),
		InvMaxTesselationLevelParameter(Shader,TEXT("InvMaxTesselationLevel")),
		InvTerrainSizeParameter(Shader,TEXT("InvTerrainSize")),
		InvLightMapSizeParameter(Shader,TEXT("InvLightMapSize")),
		InvSectionSizeParameter(Shader,TEXT("InvSectionSize")),
		ZScaleParameter(Shader,TEXT("ZScale")),
		SectionBaseParameter(Shader,TEXT("SectionBase"))
	{}

	// FD3DVertexFactoryParameters interface.

	virtual void Set(FVertexFactory* VertexFactory);
	virtual const TCHAR* GetShaderFilename() { return TEXT("TerrainVertexFactory.hlsl"); }
};

//
//	FD3DFoliageVertexFactoryParameters
//

struct FD3DFoliageVertexFactoryParameters: FD3DVertexFactoryParameters
{
	// Constant register indices.

	FD3DShaderParameter	WindPointSourcesParameter;
	FD3DShaderParameter	WindDirectionalSourcesParameter;
	FD3DShaderParameter	InvWindSpeedParameter;

	// Constructor.

	FD3DFoliageVertexFactoryParameters(FD3DShader* Shader):
		WindPointSourcesParameter(Shader,TEXT("WindPointSources")),
		WindDirectionalSourcesParameter(Shader,TEXT("WindDirectionalSources")),
		InvWindSpeedParameter(Shader,TEXT("InvWindSpeed"))
	{}

	// FD3DVertexFactoryParameters interface.

	virtual void Set(FVertexFactory* VertexFactory);
	virtual const TCHAR* GetShaderFilename() { return TEXT("FoliageVertexFactory.hlsl"); }
};

//
//	FD3DParticleVertexFactoryParameters
//

struct FD3DParticleVertexFactoryParameters: FD3DVertexFactoryParameters
{
	// Constant register indices.

	FD3DShaderParameter	LocalToWorldParameter,
						CameraWorldPositionParameter,
						CameraRightParameter,
						CameraUpParameter,
						ScreenAlignmentParameter;

	// Constructor.

	FD3DParticleVertexFactoryParameters(FD3DShader* Shader):
		LocalToWorldParameter(Shader,TEXT("LocalToWorld")),
		CameraWorldPositionParameter(Shader,TEXT("CameraWorldPosition")),
		CameraRightParameter(Shader,TEXT("CameraRight")),
		CameraUpParameter(Shader,TEXT("CameraUp")),
		ScreenAlignmentParameter(Shader,TEXT("ScreenAlignment"))
	{}

	// FD3DVertexFactoryParameters interface.

	virtual void Set(FVertexFactory* VertexFactory);
	virtual const TCHAR* GetShaderFilename() { return TEXT("ParticleVertexFactory.hlsl"); }
};

// 
//	FD3DParticleSubUVVertexFactoryParameters//
struct FD3DParticleSubUVVertexFactoryParameters : FD3DParticleVertexFactoryParameters
{
	// Constructor.
	FD3DParticleSubUVVertexFactoryParameters(FD3DShader* Shader):
		FD3DParticleVertexFactoryParameters(Shader)
	{}

	// FD3DParticleSubUVVertexFactoryParameters interface.

	virtual void Set(FVertexFactory* VertexFactory);
	virtual const TCHAR* GetShaderFilename() { return TEXT("ParticleSubUVVertexFactory.hlsl"); }
};


//
//	FD3DMeshShader
//

struct FD3DMeshShader: FD3DMaterialShader
{
	FResourceType*					VertexFactoryType;
	FD3DVertexFactoryParameters*	VertexFactoryParameters;

	FD3DShaderParameter				ViewProjectionMatrixParameter,
									CameraPositionParameter;

	// Constructor/destructor.

	FD3DMeshShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,INT InMaterialResourceIndex,FGuid InPersistentMaterialId,FResourceType* VertexFactoryType);
	virtual ~FD3DMeshShader();

	// Cache

	void Cache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,FMaterial* Material);

	// Set

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance);

	// GetShaderSource - Allows the shader class to supply the contents of a named source file.

	virtual FString GetShaderSource(const TCHAR* Filename);

	// FD3DShader interface.

	virtual TArray<FString> CompileShaders(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,INT& VertexShaderCode,INT& PixelShaderCode,TArray<FD3DUnmappedShaderParameter>& UnmappedParameters);

	// FD3DResource interface.

	virtual void Dump(FOutputDevice& Ar);
};

//
//	FD3DDepthOnlyShader
//

struct FD3DDepthOnlyShader: FD3DMeshShader
{
	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("DepthOnly.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DDepthOnlyShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,InFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType)
	{
		Cache(Material);
	}

	void Set();
	void SetInstance(UPrimitiveComponent* Primitive,FLocalVertexFactory* VertexFactory);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Depth-only shader"); }
};

//
//	FD3DEmissiveShader
//

struct FD3DEmissiveShader: FD3DMeshShader
{
	FD3DShaderParameter	SkyColorParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("Emissive.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DEmissiveShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,InFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		SkyColorParameter(this,TEXT("SkyColor"))
	{
		Cache(Material);
	}

	// FD3DShader interface.

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Emissive shader"); }
};

//
//	FD3DUnlitTranslucencyShader
//

struct FD3DUnlitTranslucencyShader: FD3DMeshShader
{
	FD3DShaderParameter	FogDistanceScaleParameter,
						FogMinHeightParameter,
						FogMaxHeightParameter,
						FogInScatteringParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("UnlitTranslucency.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DUnlitTranslucencyShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,InFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		FogDistanceScaleParameter(this,TEXT("FogDistanceScale")),
		FogMinHeightParameter(this,TEXT("FogMinHeight")),
		FogMaxHeightParameter(this,TEXT("FogMaxHeight")),
		FogInScatteringParameter(this,TEXT("FogInScattering"))
	{
		Cache(Material);
	}

	// FD3DShader interface.

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Unlit translucency shader"); }
};

//
//	FD3DUnlitShader
//

struct FD3DUnlitShader: FD3DMeshShader
{
	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("Unlit.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DUnlitShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,InFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType)
	{
		Cache(Material);
	}

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Unlit shader"); }
};

//
//	FD3DVertexLightingShader
//

struct FD3DVertexLightingShader: FD3DMeshShader
{
	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("VertexLighting.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DVertexLightingShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,InFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType)
	{
		Cache(Material);
	}

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Vertex lighting shader"); }
};

//
//	FD3DTranslucentLayerShader - Finds the nearest layer of translucency that's further than previousDepth
//

struct FD3DTranslucentLayerShader: FD3DMeshShader
{
	FD3DShaderParameter	LayerIndexParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("Translucency.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DTranslucentLayerShader(FD3DResourceSet* InSet,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,SF_TranslucentLayer,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		LayerIndexParameter(this,TEXT("LayerIndex"))
	{
		Cache(Material);
	}

	// Set

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,INT LayerIndex);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Translucent layer shader"); }
};

//
//	FD3DHitProxyShader
//

struct FD3DHitProxyShader: FD3DMeshShader
{
	FD3DShaderParameter	HitProxyIdParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("HitProxy.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DHitProxyShader(FD3DResourceSet* InSet,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,SF_HitProxy,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		HitProxyIdParameter(this,TEXT("HitProxyId"))
	{
		Cache(Material);
	}

	// FD3DShader interface.

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Hit proxy shader"); }
};

//
//	FD3DWireframeShader
//

struct FD3DWireframeShader: FD3DMeshShader
{
	FD3DShaderParameter	WireColorParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("Wireframe.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DWireframeShader(FD3DResourceSet* InSet,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,SF_Wireframe,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		WireColorParameter(this,TEXT("WireColor"))
	{
		Cache(Material);
	}

	// FD3DShader interface.

	void Set(FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FLinearColor& WireColor);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Wireframe shader"); }
};

//
//	FD3DWireframeHitProxyShader
//

struct FD3DWireframeHitProxyShader: FD3DMeshShader
{
	FD3DShaderParameter	HitProxyIdParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("Wireframe.hlsl"),TEXT("vertexShader"),TEXT("hitProxyPixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DWireframeHitProxyShader(FD3DResourceSet* InSet,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,SF_WireframeHitProxy,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		HitProxyIdParameter(this,TEXT("HitProxyId"))
	{
		Cache(Material);
	}

	// FD3DShader interface.

	void Set(FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FLinearColor& WireColor);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Wireframe hit proxy shader"); }
};

//
//	FD3DShadowDepthTestShader
//

struct FD3DShadowDepthTestShader: FD3DMeshShader
{
	FD3DShaderParameter	ShadowMatrixParameter,
						ShadowDepthTextureParameter,
						InvMaxShadowDepthParameter,
						SampleOffsetsParameter,
						ShadowTexCoordScaleParameter;

	// GetNumSamples

	virtual UINT GetNumSamples() const { return 16; }

	// GetSampleOffset

	virtual FVector2D GetSampleOffset(UINT Index) const;

	// Cache

	void Cache(FMaterial* Material,const TCHAR* PixelShaderFunction);

	// Constructor.

	FD3DShadowDepthTestShader(FD3DResourceSet* InSet,FMaterial* Material,FResourceType* InVertexFactoryType,ED3DShaderFunction InFunction = SF_ShadowDepthTest):
		FD3DMeshShader(InSet,InFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		ShadowMatrixParameter(this,TEXT("ShadowMatrix")),
		ShadowDepthTextureParameter(this,TEXT("ShadowDepthTexture")),
		InvMaxShadowDepthParameter(this,TEXT("InvMaxShadowDepth")),
		SampleOffsetsParameter(this,TEXT("SampleOffsets")),
		ShadowTexCoordScaleParameter(this,TEXT("ShadowTexCoordScale"))
	{
		if((Function & SF_FunctionMask) == SF_ShadowDepthTest)
			Cache(Material,TEXT("pixelShader"));
	}

	// FD3DShader interface.

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FMatrix& ShadowMatrix,FLOAT MaxShadowDepth,UBOOL DirectionalLight,UINT Resolution);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Shadow depth test shader"); }
};

//
//	FD3DNVShadowDepthTestShader
//

struct FD3DNVShadowDepthTestShader: FD3DShadowDepthTestShader
{
	FD3DShaderParameter	HWShadowDepthTextureParameter;

	// Constructor.

	FD3DNVShadowDepthTestShader(FD3DResourceSet* InSet,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DShadowDepthTestShader(InSet,Material,InVertexFactoryType,SF_NVShadowDepthTest),
		HWShadowDepthTextureParameter(this,TEXT("HWShadowDepthTexture"))
	{
		Cache(Material,TEXT("nvPixelShader"));
	}

	// Set

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FMatrix& ShadowMatrix,FLOAT MaxShadowDepth,UBOOL DirectionalLight,UINT Resolution);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("NV shadow depth test shader"); }
};

//
//	FD3DShadowDepthShader
//

struct FD3DShadowDepthShader: FD3DMeshShader
{
	FD3DShaderParameter	ShadowMatrixParameter,
						ProjectionMatrixParameter,
						MaxShadowDepthParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("ShadowDepth.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DShadowDepthShader(FD3DResourceSet* InSet,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,SF_ShadowDepth,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		ShadowMatrixParameter(this,TEXT("ShadowMatrix")),
		ProjectionMatrixParameter(this,TEXT("ProjectionMatrix")),
		MaxShadowDepthParameter(this,TEXT("MaxShadowDepth"))
	{
		Cache(Material);
	}

	// FD3DShader interface.

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FMatrix& ShadowMatrix,const FMatrix& ProjectionMatrix,FLOAT MaxShadowDepth);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Shadow depth shader"); }
};

//
//	FD3DLightFunctionShader
//

struct FD3DLightFunctionShader: FD3DMeshShader
{
	FD3DShaderParameter	ScreenToLightParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshShader::Cache(TEXT("LightFunction.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DLightFunctionShader(FD3DResourceSet* InSet,FMaterial* Material,FResourceType* InVertexFactoryType):
		FD3DMeshShader(InSet,SF_LightFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType),
		ScreenToLightParameter(this,TEXT("ScreenToLight"))
	{
		Cache(Material);
	}

	// FD3DShader interface.

	void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,const FMatrix& WorldToLight);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Light function shader"); }
};

//
//	FD3DMeshShaderId
//

struct FD3DMeshShaderId
{
	ED3DShaderFunction		Function;
	INT						MaterialResourceIndex;
	FResourceType*			VertexFactoryType;

	// Constructor.

	FD3DMeshShaderId() {}
	FD3DMeshShaderId(ED3DShaderFunction InFunction,INT InMaterialResourceIndex,FResourceType* InVertexFactoryType):
		Function(InFunction),
		MaterialResourceIndex(InMaterialResourceIndex),
		VertexFactoryType(InVertexFactoryType)
	{}

	// Equality test.

	friend UBOOL operator==(const FD3DMeshShaderId& IdA,const FD3DMeshShaderId& IdB)
	{
		return	IdA.Function == IdB.Function &&
				IdA.MaterialResourceIndex == IdB.MaterialResourceIndex &&
				IdA.VertexFactoryType == IdB.VertexFactoryType;
	}

	// GetTypeHash

	friend FORCEINLINE DWORD GetTypeHash(const FD3DMeshShaderId& Id)
	{
		DWORD	CRC = 0;
		CRC = appMemCrc(&Id.Function,sizeof(ED3DShaderFunction),CRC);
		CRC = appMemCrc(&Id.MaterialResourceIndex,sizeof(INT),CRC);
		CRC = appMemCrc(&Id.VertexFactoryType,sizeof(FResourceType*),CRC);
		return CRC;
	}
};

//
//	FD3DPersistentMeshShaderId
//

struct FD3DPersistentMeshShaderId
{
	FGuid	PersistentMaterialId;
	BYTE	Function;
	BYTE	VertexFactoryType;

	// Constructor.

	FD3DPersistentMeshShaderId() {}
	FD3DPersistentMeshShaderId(BYTE InFunction,FGuid InPersistentMaterialId,FResourceType* InVertexFactoryType):
		Function(InFunction),
		PersistentMaterialId(InPersistentMaterialId),
		VertexFactoryType(GetD3DVertexFactoryType(InVertexFactoryType))
	{}

	// Equality test.

	friend UBOOL operator==(const FD3DPersistentMeshShaderId& IdA,const FD3DPersistentMeshShaderId& IdB)
	{
		return	IdA.Function == IdB.Function &&
				IdA.PersistentMaterialId == IdB.PersistentMaterialId &&
				IdA.VertexFactoryType == IdB.VertexFactoryType;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FD3DPersistentMeshShaderId& Id)
	{
		return Ar << Id.PersistentMaterialId << Id.Function << Id.VertexFactoryType;
	}

	// GetTypeHash

	friend FORCEINLINE DWORD GetTypeHash(const FD3DPersistentMeshShaderId& Id)
	{
		DWORD	CRC = 0;
		CRC = appMemCrc(&Id.PersistentMaterialId,sizeof(FGuid),CRC);
		CRC = appMemCrc(&Id.Function,sizeof(BYTE),CRC);
		CRC = appMemCrc(&Id.VertexFactoryType,sizeof(BYTE),CRC);
		return CRC;
	}
};

//
//	FD3DMeshShaderCache
//

struct FD3DMeshShaderCache: TD3DResourceCache<FD3DMeshShaderId,FD3DMeshShader>
{
	// Constructor.

	FD3DMeshShaderCache(UD3DRenderDevice* InRenderDevice):
		TD3DResourceCache<FD3DMeshShaderId,FD3DMeshShader>(InRenderDevice)
	{}

	// GetCachedShader

	FD3DMeshShader* GetCachedShader(ED3DShaderFunction Function,FMaterial* Material,FResourceType* VertexFactoryType);

	// FD3DResourceSet interface.

	virtual void FreeResource(FResource* Resource)
	{
		if( Resource->Type()->IsA(FMaterial::StaticType) )
		{
			for(TDynamicMap<FD3DMeshShaderId,FD3DMeshShader*>::TIterator It(ResourceMap);It;++It)
			{
				if(It.Key().MaterialResourceIndex == Resource->ResourceIndex)
				{
					FD3DMeshShader*	CachedShader = It.Value();
					It.RemoveCurrent();
					CachedShader->Cached = 0;
					CachedShader->RemoveRef();
				}
			}
		}
	}
};

//
//	ED3DStaticLightingType
//

enum ED3DStaticLightingType
{
	SLT_Dynamic,
	SLT_VertexMask,
	SLT_TextureMask
};

//
//	FD3DMeshLightShader
//

struct FD3DMeshLightShader: FD3DMeshShader
{
	ED3DStaticLightingType	StaticLightingType;
	FD3DShaderParameter		VisibilityTextureParameter;

	// Constructor.

	FD3DMeshLightShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,INT InMaterialResourceIndex,FGuid InPersistentMaterialId,FResourceType* InVertexFactoryType,ED3DStaticLightingType InStaticLightingType);

	// Set

	virtual void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,ULightComponent* Light,FD3DTexture2D* VisibilityTexture);

	// Cache

	void Cache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,FMaterial* Material);

	// FD3DShader interface.

	virtual TArray<FString> CompileShaders(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,INT& VertexShaderCode,INT& PixelShaderCode,TArray<FD3DUnmappedShaderParameter>& UnmappedParameters);

	// FD3DResource interface.

	virtual void Dump(FOutputDevice& Ar);
};

//
//	FD3DPointLightShader
//

struct FD3DPointLightShader: FD3DMeshLightShader
{
	FD3DShaderParameter	LightPositionParameter,
						LightColorParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshLightShader::Cache(TEXT("SinglePointLight.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DPointLightShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,FMaterial* Material,FResourceType* InVertexFactoryType,ED3DStaticLightingType InStaticLightingType):
		FD3DMeshLightShader(InSet,InFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType,InStaticLightingType),
		LightPositionParameter(this,TEXT("LightPosition")),
		LightColorParameter(this,TEXT("LightColor"))
	{
		if((Function & SF_FunctionMask) == SF_PointLight)
			Cache(Material);
	}

	// Set

	virtual void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,ULightComponent* Light,FD3DTexture2D* VisibilityTexture);
	
	// FD3DResource interface.

	virtual FString GetTypeDescription() { return FString::Printf(TEXT("Point light shader %s %s"),VertexFactoryType->Name,StaticLightingType == SLT_Dynamic ? TEXT("Dynamic") : (StaticLightingType == SLT_TextureMask ? TEXT("TextureMask") : (StaticLightingType == SLT_VertexMask ? TEXT("VertexMask") : TEXT("")))); }
};

//
//	FD3DSpotLightShader
//

struct FD3DSpotLightShader: FD3DPointLightShader
{
	FD3DShaderParameter	SpotAnglesParameter,
						SpotDirectionParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshLightShader::Cache(TEXT("SingleSpotLight.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DSpotLightShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,FMaterial* Material,FResourceType* InVertexFactoryType,ED3DStaticLightingType InStaticLightingType):
		FD3DPointLightShader(InSet,InFunction,Material,InVertexFactoryType,InStaticLightingType),
		SpotAnglesParameter(this,TEXT("SpotAngles")),
		SpotDirectionParameter(this,TEXT("SpotDirection"))
	{
		Cache(Material);
	}

	// Set

	virtual void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,ULightComponent* Light,FD3DTexture2D* VisibilityTexture);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return FString::Printf(TEXT("Spot light shader %s %s"),VertexFactoryType->Name,StaticLightingType == SLT_Dynamic ? TEXT("Dynamic") : (StaticLightingType == SLT_TextureMask ? TEXT("TextureMask") : (StaticLightingType == SLT_VertexMask ? TEXT("VertexMask") : TEXT("")))); }
};

//
//	FD3DDirectionalLightShader
//

struct FD3DDirectionalLightShader: FD3DMeshLightShader
{
	FD3DShaderParameter	LightDirectionParameter,
						LightColorParameter;

	// Cache

	void Cache(FMaterial* Material)
	{
		FD3DMeshLightShader::Cache(TEXT("SingleSkyLight.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Constructor.

	FD3DDirectionalLightShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,FMaterial* Material,FResourceType* InVertexFactoryType,ED3DStaticLightingType InStaticLightingType):
		FD3DMeshLightShader(InSet,InFunction,Material->ResourceIndex,Material->GetPersistentId(),InVertexFactoryType,InStaticLightingType),
		LightDirectionParameter(this,TEXT("LightDirection")),
		LightColorParameter(this,TEXT("LightColor"))
	{
		Cache(Material);
	}

	// Set

	virtual void Set(UPrimitiveComponent* Primitive,FVertexFactory* VertexFactory,FMaterialInstance* MaterialInstance,ULightComponent* Light,FD3DTexture2D* VisibilityTexture);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return FString::Printf(TEXT("Directional light shader %s %s"),VertexFactoryType->Name,StaticLightingType == SLT_Dynamic ? TEXT("Dynamic") : (StaticLightingType == SLT_TextureMask ? TEXT("TextureMask") : (StaticLightingType == SLT_VertexMask ? TEXT("VertexMask") : TEXT("")))); }
};

//
//	FD3DMeshLightShaderId
//

struct FD3DMeshLightShaderId
{
	INT		MaterialResourceIndex;
	BYTE	Function;
	BYTE	VertexFactoryType;
	BYTE	StaticLightingType;

	// Constructor.

	FD3DMeshLightShaderId() {}
	FD3DMeshLightShaderId(BYTE InFunction,INT InMaterialResourceIndex,FResourceType* InVertexFactoryType,ED3DStaticLightingType InStaticLightingType):
		Function(InFunction),
		MaterialResourceIndex(InMaterialResourceIndex),
		VertexFactoryType(GetD3DVertexFactoryType(InVertexFactoryType)),
		StaticLightingType(InStaticLightingType)
	{}

	// Equality test.

	friend UBOOL operator==(const FD3DMeshLightShaderId& IdA,const FD3DMeshLightShaderId& IdB)
	{
		return	IdA.MaterialResourceIndex == IdB.MaterialResourceIndex &&
				IdA.Function == IdB.Function &&
				IdA.VertexFactoryType == IdB.VertexFactoryType &&
				IdA.StaticLightingType == IdB.StaticLightingType;
	}

	// GetTypeHash

	friend FORCEINLINE DWORD GetTypeHash(const FD3DMeshLightShaderId& Id)
	{
		DWORD	CRC = 0;
		CRC = appMemCrc(&Id.MaterialResourceIndex,sizeof(INT),CRC);
		CRC = appMemCrc(&Id.Function,sizeof(BYTE),CRC);
		CRC = appMemCrc(&Id.VertexFactoryType,sizeof(BYTE),CRC);
		CRC = appMemCrc(&Id.StaticLightingType,sizeof(BYTE),CRC);
		return CRC;
	}
};

//
//	FD3DPersistentMeshLightShaderId
//

struct FD3DPersistentMeshLightShaderId
{
	FGuid	PersistentMaterialId;
	BYTE	Function;
	BYTE	VertexFactoryType;
	BYTE	StaticLightingType;

	// Constructor.

	FD3DPersistentMeshLightShaderId() {}
	FD3DPersistentMeshLightShaderId(FGuid InPersistentMaterialId,BYTE InFunction,FResourceType* InVertexFactoryType,ED3DStaticLightingType InStaticLightingType):
		PersistentMaterialId(InPersistentMaterialId),
		Function(InFunction),
		VertexFactoryType(GetD3DVertexFactoryType(InVertexFactoryType)),
		StaticLightingType(InStaticLightingType)
	{}

	// Equality test.

	friend UBOOL operator==(const FD3DPersistentMeshLightShaderId& IdA,const FD3DPersistentMeshLightShaderId& IdB)
	{
		return	IdA.PersistentMaterialId == IdB.PersistentMaterialId &&
				IdA.Function == IdB.Function &&
				IdA.VertexFactoryType == IdB.VertexFactoryType &&
				IdA.StaticLightingType == IdB.StaticLightingType;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FD3DPersistentMeshLightShaderId& Id)
	{
		return Ar << Id.PersistentMaterialId << Id.Function << Id.VertexFactoryType << Id.StaticLightingType;
	}

	// GetTypeHash

	friend FORCEINLINE DWORD GetTypeHash(const FD3DPersistentMeshLightShaderId& Id)
	{
		DWORD	CRC = 0;
		CRC = appMemCrc(&Id.PersistentMaterialId,sizeof(FGuid),CRC);
		CRC = appMemCrc(&Id.Function,sizeof(BYTE),CRC);
		CRC = appMemCrc(&Id.VertexFactoryType,sizeof(BYTE),CRC);
		CRC = appMemCrc(&Id.StaticLightingType,sizeof(BYTE),CRC);
		return CRC;
	}
};

//
//	FD3DMeshLightShaderCache
//

struct FD3DMeshLightShaderCache: TD3DResourceCache<FD3DMeshLightShaderId,FD3DMeshLightShader>
{
	// Constructor.

	FD3DMeshLightShaderCache(UD3DRenderDevice* InRenderDevice):
		TD3DResourceCache<FD3DMeshLightShaderId,FD3DMeshLightShader>(InRenderDevice)
	{}

	// GetCachedShader

	FD3DMeshLightShader* GetCachedShader(ED3DShaderFunction Function,FMaterial* Material,FResourceType* VertexFactoryType,ED3DStaticLightingType StaticLightingType);

	// FD3DResourceSet interface.

	virtual void FreeResource(FResource* Resource)
	{
		if( Resource->Type()->IsA(FMaterial::StaticType) )
		{
			for(TDynamicMap<FD3DMeshLightShaderId,FD3DMeshLightShader*>::TIterator It(ResourceMap);It;++It)
			{
				if(It.Key().MaterialResourceIndex == Resource->ResourceIndex)
				{
					FD3DMeshLightShader*	CachedShader = It.Value();
					It.RemoveCurrent();
					CachedShader->Cached = 0;
					CachedShader->RemoveRef();
				}
			}
		}
	}
};
