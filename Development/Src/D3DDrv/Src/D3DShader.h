/*=============================================================================
	D3DShader.h: Unreal Direct3D shader definitions.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	ED3DShaderFunction
//

enum ED3DShaderFunction
{
	SF_PointLight,			// Mesh lighting
	SF_DirectionalLight,
	SF_SpotLight,
	SF_DepthOnly,			// Mesh
	SF_Emissive,
	SF_UnlitTranslucency,
	SF_Unlit,
	SF_VertexLighting,
	SF_ShadowDepth,
	SF_Wireframe,
	SF_WireframeHitProxy,
	SF_HitProxy,

	SF_ShadowVolume,
	SF_ShadowDepthTest,
	SF_NVShadowDepthTest,

	SF_LightFunction,

	SF_Blur,
	SF_AccumulateImage,
	SF_BlendImage,
	SF_Exposure,
	SF_FinishImage,
	SF_Copy,
	SF_CopyFill,
	SF_Fill,

	SF_Fog,

	SF_Tile,
	SF_TileHitProxy,

	SF_SolidTile,
	SF_SolidTileHitProxy,

	SF_EmissiveTile,
	SF_EmissiveTileHitProxy,

	SF_Sprite,
	SF_SpriteHitProxy,

	SF_OcclusionQuery,

	SF_TranslucentLayer,

	SF_FunctionMask = 0x3f,

	// Mesh flags.
	SF_UseFPBlending = 0x40,
	SF_OpaqueLayer = 0x80,

	SF_FlagsMask = 0xC0,
};

//
//	ED3DShaderParameterType
//

enum ED3DShaderParameterType
{
	SPT_VertexShaderConstant,
	SPT_PixelShaderConstant,
	SPT_PixelShaderSampler
};

//
//	ED3DShaderType
//

enum ED3DShaderType
{
	ST_VertexShader,
	ST_PixelShader
};

//
//	FD3DShaderParameter
//

struct FD3DShaderParameter
{
	INT						VertexRegisterIndex,
							PixelRegisterIndex,
							SamplerIndex;
	UINT					NumVertexRegisters,
							NumPixelRegisters,
							NumSamplers;

	FD3DShaderParameter*	NextParameter;
	const TCHAR*			ParameterName;

	// Constructors.

	FD3DShaderParameter(struct FD3DShader* Shader,const TCHAR* InParameterName);

	// Register setting helpers.

	void SetVectors(const FLOAT* Vectors,UINT Index,UINT NumVectors) const;
	void SetScalar(FLOAT Scalar,UINT Index = 0) const
	{
		FLOAT	Vector[4] = { Scalar, Scalar, Scalar, Scalar };
		SetVectors(Vector,Index,1);
	}
	void SetVector(FLOAT X,FLOAT Y = 0,FLOAT Z = 0,FLOAT W = 0,UINT Index = 0) const
	{
		FLOAT Vector[4] = { X, Y, Z, W };
		SetVectors(Vector,Index,1);
	}
	void SetVector(const FVector& XYZ,FLOAT W = 0,UINT Index = 0) const
	{
		FLOAT Vector[4] = { XYZ.X, XYZ.Y, XYZ.Z, W };
		SetVectors(Vector,Index,1);
	}
	void SetColor(const FLinearColor& Color,UINT Index = 0) const
	{
		SetVectors((FLOAT*)&Color,Index,1);
	}
	void SetMatrix3(const FMatrix& Matrix,UINT Index = 0) const
	{
		SetVectors((FLOAT*)&Matrix,Index,3);
	}
	void SetMatrix4(const FMatrix& Matrix,UINT Index = 0) const
	{
		SetVectors((FLOAT*)&Matrix,Index,4);
	}

	void SetTexture(IDirect3DBaseTexture9* Texture,DWORD Filter,UBOOL Clamp = 0,INT ElementIndex = 0,UBOOL SRGB = 0,UINT Index = 0) const;
	void SetUserTexture(FD3DTexture* Texture,UINT Index = 0) const;
};

//
//	FD3DUnmappedShaderParameter
//

struct FD3DUnmappedShaderParameter
{
	FString	Name;
	UINT	RegisterIndex;
	UINT	NumRegisters;
	BYTE	Type;

	// Constructor.

	FD3DUnmappedShaderParameter() {}
	FD3DUnmappedShaderParameter(BYTE InType,const TCHAR* InName,UINT InRegisterIndex,UINT InNumRegisters):
		Type(InType),
		Name(InName),
		RegisterIndex(InRegisterIndex),
		NumRegisters(InNumRegisters)
	{}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FD3DUnmappedShaderParameter& P)
	{
		return Ar << P.Name << P.RegisterIndex << P.NumRegisters << P.Type;
	}
};

//
//	FD3DShader
//

struct FD3DShader: FD3DResource
{
	ED3DShaderFunction				Function;

	FD3DShaderParameter*			FirstParameter;

	TD3DRef<IDirect3DVertexShader9>	VertexShader;
	TD3DRef<IDirect3DPixelShader9>	PixelShader;

	UINT	NumVertexShaderInstructions,
			NumPixelShaderInstructions;

	// Register indices.

	FD3DShaderParameter	PreviousLightingTextureParameter,
						ScreenPositionScaleBiasParameter,
						LightTextureParameter,
						TwoSidedSignParameter;

	// Constructor/destructor.

	FD3DShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction);
	virtual ~FD3DShader() {}

	// MapParameter - Maps a parameter to a shader register.

	UBOOL MapParameter(ED3DShaderParameterType ParameterType,const TCHAR* ParameterName,UINT RegisterIndex,UINT NumRegisters);

	// Cache - Attempts to compile the shader, returning any errors that occur.

	TArray<FString> Cache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions);

	// SafeCache - Attempts to compile the shader, triggering a fatal exception if any errors occur.

	void SafeCache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions);

	// GetPixelShaderTarget

	virtual const ANSICHAR* GetPixelShaderTarget() { return "ps_3_0"; }

	// GetVertexShaderTarget

	virtual const ANSICHAR* GetVertexShaderTarget() { return "vs_3_0"; }

	// CompileShaders - Compiles the vertex and pixel shaders into D3D shader code.  This function may be overridden to use a compiled shader cache.

	virtual TArray<FString> CompileShaders(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,INT& VertexShaderCode,INT& PixelShaderCode,TArray<FD3DUnmappedShaderParameter>& Parameters);

	// Set

	void Set();

	// GetShaderSource - Allows the shader class to supply the contents of a named source file.

	virtual FString GetShaderSource(const TCHAR* Filename);

	// GetDumpName

	virtual FString GetDumpName() const;

	// FD3DResource interface.

	virtual void Dump(FOutputDevice& Ar);
	virtual void DumpShader();
};

/**
 * A shader used to render simple canvas primitives.
 */
struct FD3DTileShader : FD3DShader
{
	FD3DShaderParameter	HitProxyIdParameter,
						TileTextureParameter;

	// Constructor.
	FD3DTileShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,InFunction),
		HitProxyIdParameter(this,TEXT("HitProxyId")),
		TileTextureParameter(this,TEXT("TileTexture"))
	{
		Cache(
			TEXT("Tile.hlsl"),
			TEXT("vertexShader"),
			Function == SF_Tile ?
				TEXT("pixelShader") :
				(Function == SF_TileHitProxy ?
					TEXT("hitProxyPixelShader") :
					(Function == SF_SolidTile ?
						TEXT("solidPixelShader") :
						TEXT("solidHitProxyPixelShader"))),
			NO_DEFINITIONS
			);
	}

	// Set

	void Set(FD3DTexture* TileTexture);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Tile shader"); }
};

//
//	FD3DMaterialUserInput
//

struct FD3DMaterialUserInput
{
	virtual ~FD3DMaterialUserInput() {}
	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime) = 0;
	virtual UBOOL ShouldEmbedCode() { return 0; }
};

//
//	FD3DMaterialShader
//

struct FD3DMaterialShader: FD3DShader
{
	FGuid	PersistentMaterialId;
	INT		MaterialResourceIndex;
	FString	MaterialName;

	TArray<FMaterialError>	Errors;

	FString							UserShaderCode;
	TArray<FD3DTexture*>			UserTextures;
	UINT							NumUserTexCoords;
	TArray<FD3DMaterialUserInput*>	UserInputs;
	TArray<FD3DMaterialUserInput*>	UserVectorInputs;
	TArray<FD3DMaterialUserInput*>	UserScalarInputs;

	FD3DTextureCube*		SHBasisTexture;
	FD3DTextureCube*		SHSkyBasisTexture;

	FD3DShaderParameter	UserTexturesParameter,
						UserVectorInputsParameter,
						UserScalarInputsParameter,
						SHBasisTextureParameter,
						SHSkyBasisTextureParameter;

	// Constructor/destructor.

	FD3DMaterialShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,INT InMaterialResourceIndex,FGuid InPersistentMaterialId);
	virtual ~FD3DMaterialShader();

	// Cache

	void Cache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,FMaterial* Material);

	// Set

	void Set(FMaterialInstance* MaterialInstance,FLOAT ObjectTime);

	// GetMaxCoordinateIndex - Returns the maximum user texture coordinate index used outside of the user shader.

	virtual UINT GetMaxCoordinateIndex() { return 0; }

	// FD3DShader interface.

	virtual FString GetShaderSource(const TCHAR* Filename);
	virtual FString GetDumpName() const { return FString::Printf(TEXT("%s.%s"),*MaterialName,*FD3DShader::GetDumpName()); }

	// FD3DResource interface.

	virtual void Dump(FOutputDevice& Ar);
	virtual void DumpShader()
	{
		FD3DShader::DumpShader();
		appSaveStringToFile(UserShaderCode,*FString::Printf(TEXT("%s\\Shaders\\%s.UserShader.txt"),appBaseDir(),*GetDumpName()));
	}
};

//
//	FD3DEmissiveTileShader
//

struct FD3DEmissiveTileShader: FD3DMaterialShader
{
	UBOOL				HitProxy;
	FD3DShaderParameter	HitProxyIdParameter;

	// Constructor.

	FD3DEmissiveTileShader(FD3DResourceSet* InSet,FMaterial* Material,UBOOL InHitProxy):
		FD3DMaterialShader(InSet,InHitProxy ? SF_EmissiveTile : SF_EmissiveTileHitProxy,Material->ResourceIndex,Material->GetPersistentId()),
		HitProxy(InHitProxy),
		HitProxyIdParameter(this,TEXT("HitProxyId"))
	{
		Cache(TEXT("EmissiveTile.hlsl"),TEXT("vertexShader"),HitProxy ? TEXT("hitProxyPixelShader") : TEXT("pixelShader"),NO_DEFINITIONS,Material);
	}

	// Set

	void Set(FMaterialInstance* MaterialInstance);

	// FD3DShader interface.

	virtual TArray<FString> CompileShaders(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,INT& VertexShaderCode,INT& PixelShaderCode,TArray<FD3DUnmappedShaderParameter>& UnmappedParameters);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Emissive tile shader"); }
};

//
//	FD3DEmissiveTileShaderCache
//

struct FD3DEmissiveTileShaderCache: TD3DResourceCache<INT,FD3DEmissiveTileShader>
{
	UBOOL	HitProxy;

	// Constructor.

	FD3DEmissiveTileShaderCache(UD3DRenderDevice* InRenderDevice,UBOOL InHitProxy):
		TD3DResourceCache<INT,FD3DEmissiveTileShader>(InRenderDevice),
		HitProxy(InHitProxy)
	{}

	// GetCachedShader

	FD3DEmissiveTileShader* GetCachedShader(FMaterial* Material)
	{
		FD3DEmissiveTileShader*	CachedShader = GetCachedResource(Material->ResourceIndex);
		if(!CachedShader)
		{
			CachedShader = new FD3DEmissiveTileShader(this,Material,HitProxy);
			AddCachedResource(Material->ResourceIndex,CachedShader);
		}
		return CachedShader;
	}

	// FD3DResourceSet interface.

	virtual void FreeResource(FResource* Resource)
	{
		if( Resource->Type()->IsA(FMaterial::StaticType) )
		{
			FD3DEmissiveTileShader*	Shader = ResourceMap.FindRef(Resource->ResourceIndex);
			if(Shader)
			{
				ResourceMap.Remove(Resource->ResourceIndex);
				Shader->Cached = 0;
				Shader->RemoveRef();
			}
		}
	}
};

//
//	TD3DGlobalShaderCache - A cached version of a globally unique shader.
//

template<class ShaderType,ED3DShaderFunction ShaderFunction> struct TD3DGlobalShaderCache: FD3DResourceSet
{
	ShaderType*			CachedShader;

	// Constructor.

	TD3DGlobalShaderCache(UD3DRenderDevice* InRenderDevice):
		FD3DResourceSet(InRenderDevice),
		CachedShader(NULL)
	{}

	// Dereference/caching operator.

	ShaderType* operator*()
	{
		if(!CachedShader)
		{
			CachedShader = new ShaderType(this,ShaderFunction);
			CachedShader->AddRef();
		}
		return CachedShader;
	}

	ShaderType* operator->()
	{
		return **this;
	}

	// FD3DResourceSet interface.

	virtual void Flush()
	{
		if(CachedShader)
		{
			check(CachedShader->NumRefs == 1);
			delete CachedShader;
			CachedShader = NULL;
		}
	}
};

//
//	FD3DSpriteShader
//

struct FD3DSpriteShader: FD3DShader
{
	FD3DShaderParameter	SpriteTextureParameter,
						SpriteColorParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("Sprite.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DSpriteShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_Sprite),
		SpriteTextureParameter(this,TEXT("SpriteTexture")),
		SpriteColorParameter(this,TEXT("SpriteColor"))
	{
		Cache();
	}

	// FD3DShader interface.

	void Set(FTexture2D* Texture,FColor Color);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Sprite shader"); }
};

//
//	FD3DSpriteHitProxyShader
//

struct FD3DSpriteHitProxyShader: FD3DShader
{
	FD3DShaderParameter	SpriteTextureParameter,
						SpriteColorParameter,
						HitProxyIdParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("Sprite.hlsl"),TEXT("vertexShader"),TEXT("hitProxyPixelShader"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DSpriteHitProxyShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_SpriteHitProxy),
		SpriteTextureParameter(this,TEXT("SpriteTexture")),
		SpriteColorParameter(this,TEXT("SpriteColor")),
		HitProxyIdParameter(this,TEXT("HitProxyId"))
	{
		Cache();
	}

	// FD3DShader interface.

	void Set(FTexture2D* Texture,FColor Color);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Sprite hit proxy shader"); }
};

//
//	FD3DShadowVolumeShader
//

struct FD3DShadowVolumeShader: FD3DShader
{
	FD3DShaderParameter	LocalToWorldParameter,
						ViewProjectionMatrixParameter,
						LightPositionParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("ShadowVolume.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DShadowVolumeShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_ShadowVolume),
		LightPositionParameter(this,TEXT("LightPosition")),
		ViewProjectionMatrixParameter(this,TEXT("ViewProjectionMatrix")),
		LocalToWorldParameter(this,TEXT("LocalToWorld"))
	{
		Cache();
	}

	// Set

	void Set(FVertexFactory* VertexFactory,ULightComponent* Light);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Shadow volume shader"); }
};

//
//	FD3DBlurShader
//

struct FD3DBlurShader: FD3DShader
{
	INT					NumSamples;
	FD3DShaderParameter	BlurTextureParameter,
						SampleOffsetsParameter,
						SampleWeightsParameter;

	// Cache

	void Cache();

	// Constructor.

	FD3DBlurShader(FD3DResourceSet* InSet,INT InNumSamples):
		FD3DShader(InSet,SF_Blur),
		NumSamples(InNumSamples),
		BlurTextureParameter(this,TEXT("BlurTexture")),
		SampleOffsetsParameter(this,TEXT("SampleOffsets")),
		SampleWeightsParameter(this,TEXT("SampleWeights"))
	{
		Cache();
	}

	// Set

    void Set(const TD3DRef<IDirect3DTexture9>& BlurTexture,const FVector2D* Offsets,const FPlane* Weights,const FPlane& FinalMultiple,UBOOL Filtered = 0);
	
	// FD3DShader interface.

	virtual FString GetDumpName() const { return FString::Printf(TEXT("SF_Blur%u"),NumSamples); }

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Blur shader"); }
};

//
//	FD3DBlurShaderCache
//

struct FD3DBlurShaderCache: TD3DResourceCache<INT,FD3DBlurShader>
{
	// Constructor.

	FD3DBlurShaderCache(UD3DRenderDevice* InRenderDevice):
		TD3DResourceCache<INT,FD3DBlurShader>(InRenderDevice)
	{}

	// GetCachedShader

	FD3DBlurShader* GetCachedShader(INT NumSamples);
};

//
//	FD3DAccumulateImageShader
//

struct FD3DAccumulateImageShader: FD3DShader
{
	FD3DShaderParameter	LinearImageParameter,
						ScaleFactorParameter,
						LinearImageScaleBiasParameter,
						LinearImageResolutionParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("PostProcess.hlsl"),TEXT("vertexShader"),TEXT("accumulateImage"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DAccumulateImageShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_AccumulateImage),
		LinearImageParameter(this,TEXT("LinearImage")),
		ScaleFactorParameter(this,TEXT("ScaleFactor")),
		LinearImageScaleBiasParameter(this,TEXT("LinearImageScaleBias")),
		LinearImageResolutionParameter(this,TEXT("LinearImageResolution"))
	{
		Cache();
	}

	// Set

	void Set(const TD3DRef<IDirect3DTexture9>& LinearImage,const FLinearColor& ScaleFactor);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Accumulate image shader"); }
};

//
//	FD3DBlendImageShader
//

struct FD3DBlendImageShader: FD3DShader
{
	FD3DShaderParameter	LinearImageParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("PostProcess.hlsl"),TEXT("vertexShader"),TEXT("blendImage"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DBlendImageShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_BlendImage),
		LinearImageParameter(this,TEXT("LinearImage"))
	{
		Cache();
	}

	// Set

	void Set(const TD3DRef<IDirect3DTexture9>& LinearImage);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Blend image shader"); }
};

//
//	FD3DExposureShader
//

struct FD3DExposureShader: FD3DShader
{
	FD3DShaderParameter	LinearImageParameter,
						LinearImageResolutionParameter,
						ExposureTextureParameter,
						MaxDeltaExposureParameter,
						ManualExposureParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("PostProcess.hlsl"),TEXT("vertexShader"),TEXT("calcExposure"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DExposureShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_Exposure),
		LinearImageParameter(this,TEXT("LinearImage")),
		LinearImageResolutionParameter(this,TEXT("LinearImageResolution")),
		ExposureTextureParameter(this,TEXT("ExposureTexture")),
		MaxDeltaExposureParameter(this,TEXT("MaxDeltaExposure")),
		ManualExposureParameter(this,TEXT("ManualExposure"))
	{
		Cache();
	}

	// Set

	void Set(const TD3DRef<IDirect3DTexture9>& LinearImage,const TD3DRef<IDirect3DTexture9>& ExposureTexture,FLOAT MaxDeltaExposure,FLOAT ManualExposure);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Exposure shader"); }
};

//
//	FD3DFinishImageShader
//

struct FD3DFinishImageShader: FD3DShader
{
	FD3DShaderParameter	LinearImageParameter,
						ScaleFactorParameter,
						ExposureTextureParameter,
						OverlayColorParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("PostProcess.hlsl"),TEXT("vertexShader"),TEXT("finishImage"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DFinishImageShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_FinishImage),
		LinearImageParameter(this,TEXT("LinearImage")),
		ScaleFactorParameter(this,TEXT("ScaleFactor")),
		ExposureTextureParameter(this,TEXT("ExposureTexture")),
		OverlayColorParameter(this,TEXT("OverlayColor"))
	{
		Cache();
	}

	// Set

	void Set(const TD3DRef<IDirect3DTexture9>& LinearImage,const TD3DRef<IDirect3DTexture9>& ExposureTexture,const FLinearColor& ScaleFactor,const FLinearColor& OverlayColor);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Finish image shader"); }
};

//
//	FD3DCopyShader
//

struct FD3DCopyShader: FD3DShader
{
	FD3DShaderParameter	CopyTextureParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("PostProcess.hlsl"),TEXT("vertexShader"),TEXT("copy"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DCopyShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,InFunction),
		CopyTextureParameter(this,TEXT("CopyTexture"))
	{
		Cache();
	}

	// Set

	void Set(const TD3DRef<IDirect3DTexture9>& CopyTexture);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Copy shader"); }
};

//
//	FD3DCopyFillShader
//

struct FD3DCopyFillShader: FD3DShader
{
	FD3DShaderParameter	CopyTextureParameter,
						FillValueParameter,
						FillAlphaParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("PostProcess.hlsl"),TEXT("vertexShader"),TEXT("copyFill"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DCopyFillShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,InFunction),
		CopyTextureParameter(this,TEXT("CopyTexture")),
		FillValueParameter(this,TEXT("FillValue")),
		FillAlphaParameter(this,TEXT("FillAlpha"))
	{
		Cache();
	}

	// Set

	void Set(const TD3DRef<IDirect3DTexture9>& CopyTexture,const FLinearColor& FillColor,const FPlane& FillAlpha);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Copy/fill shader"); }
};

//
//	FD3DFillShader
//

struct FD3DFillShader: FD3DShader
{
	FD3DShaderParameter	FillValueParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("PostProcess.hlsl"),TEXT("vertexShader"),TEXT("fill"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DFillShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_Fill),
		FillValueParameter(this,TEXT("FillValue"))
	{
		Cache();
	}

	// Set

	void Set(const FLinearColor& FillValue);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Fill shader"); }
};

//
//	FD3DFogShader
//

struct FD3DFogShader: FD3DShader
{
	FD3DShaderParameter	SceneTextureParameter,
						ScreenToWorldParameter,
						CameraPositionParameter,
						FogDistanceScaleParameter,
						FogMinHeightParameter,
						FogMaxHeightParameter,
						FogInScatteringParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("Fog.hlsl"),TEXT("vertexShader"),TEXT("fogScene"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DFogShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_Fog),
		SceneTextureParameter(this,TEXT("SceneTexture")),
		ScreenToWorldParameter(this,TEXT("ScreenToWorld")),
		CameraPositionParameter(this,TEXT("CameraPosition")),
		FogDistanceScaleParameter(this,TEXT("FogDistanceScale")),
		FogMinHeightParameter(this,TEXT("FogMinHeight")),
		FogMaxHeightParameter(this,TEXT("FogMaxHeight")),
		FogInScatteringParameter(this,TEXT("FogInScattering"))
	{
		Cache();
	}

	// Set

	void Set(const TD3DRef<IDirect3DTexture9>& SceneTexture);

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Fog shader"); }
};

//
//	FD3DOcclusionQueryShader
//

struct FD3DOcclusionQueryShader: FD3DShader
{
	FD3DShaderParameter	ViewProjectionMatrixParameter;

	// Cache

	void Cache()
	{
		FD3DShader::SafeCache(TEXT("OcclusionQuery.hlsl"),TEXT("vertexShader"),TEXT("pixelShader"),NO_DEFINITIONS);
	}

	// Constructor.

	FD3DOcclusionQueryShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
		FD3DShader(InSet,SF_OcclusionQuery),
		ViewProjectionMatrixParameter(this,TEXT("ViewProjectionMatrix"))
	{
		Cache();
	}

	// Set

	void Set();

	// FD3DResource interface.

	virtual FString GetTypeDescription() { return TEXT("Occlusion query shader"); }
};

//
//	AddDefinition
//

static void AddDefinition(TArray<FD3DDefinition>& Definitions,const TCHAR* Name,const TCHAR* Format,...)
{
	TCHAR	DefinitionText[1024];
	GET_VARARGS(DefinitionText,ARRAY_COUNT(DefinitionText),Format,Format);

	FD3DDefinition	Definition;
	Definition.Name = Name;
	Definition.Value = DefinitionText;
	new(Definitions) FD3DDefinition(Definition);
}
