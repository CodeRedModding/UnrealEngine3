/*=============================================================================
	MaterialShader.h: Material shader definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "ImageReflectionRendering.h"
#include "DepthOfFieldCommon.h"				// FDepthOfFieldParams, FDOFShaderParameters

/** The minimum package version to load FMaterialShaderMaps with. Bump this to force existing FMaterialShaderMaps to be discarded on load. */
#define VER_MIN_MATERIALSHADERMAP				VER_INVALIDATE_SHADERCACHE5
/** The minimum package version to load material pixel shaders with. */
#define VER_MIN_MATERIAL_PIXELSHADER			VER_INVALIDATE_SHADERCACHE5
/** The minimum package version to load material vertex shaders with. */
#define VER_MIN_MATERIAL_VERTEXSHADER			VER_INVALIDATE_SHADERCACHE5

/** Same as VER_MIN_MATERIALSHADERMAP, but for the licensee package version. */
#define LICENSEE_VER_MIN_MATERIALSHADERMAP		0
/** Same as VER_MIN_MATERIAL_PIXELSHADER, but for the licensee package version. */
#define LICENSEE_VER_MIN_MATERIAL_PIXELSHADER	0
/** Same as VER_MIN_MATERIAL_VERTEXSHADER, but for the licensee package version. */
#define LICENSEE_VER_MIN_MATERIAL_VERTEXSHADER	0

/** A macro to implement material shaders which checks the package version for VER_MIN_MATERIAL_*SHADER and LICENSEE_VER_MIN_MATERIAL_*SHADER. */
#define IMPLEMENT_MATERIAL_SHADER_TYPE(TemplatePrefix,ShaderClass,SourceFilename,FunctionName,Frequency,MinPackageVersion,MinLicenseePackageVersion) \
	IMPLEMENT_SHADER_TYPE( \
		TemplatePrefix, \
		ShaderClass, \
		SourceFilename, \
		FunctionName, \
		Frequency, \
		Max((UINT)MinPackageVersion,Frequency == SF_Pixel ? Max((UINT)VER_MIN_COMPILEDMATERIAL, (UINT)VER_MIN_MATERIAL_PIXELSHADER) : Max((UINT)VER_MIN_COMPILEDMATERIAL, (UINT)VER_MIN_MATERIAL_VERTEXSHADER)), \
		Max((UINT)MinLicenseePackageVersion,Frequency == SF_Pixel ? Max((UINT)LICENSEE_VER_MIN_COMPILEDMATERIAL, (UINT)LICENSEE_VER_MIN_MATERIAL_PIXELSHADER) : Max((UINT)LICENSEE_VER_MIN_COMPILEDMATERIAL, (UINT)LICENSEE_VER_MIN_MATERIAL_VERTEXSHADER)) \
		);

/** Converts an EMaterialLightingModel to a string description. */
extern FString GetLightingModelString(EMaterialLightingModel LightingModel);

/** Converts an EBlendMode to a string description. */
extern FString GetBlendModeString(EBlendMode BlendMode);

/** Called for every material shader to update the appropriate stats. */
extern void UpdateMaterialShaderCompilingStats(const FMaterial* Material);

/**
 * Dump material stats for a given platform.
 * 
 * @param	Platform	Platform to dump stats for.
 */
extern void DumpMaterialStats( EShaderPlatform Platform );

template<typename ParameterType> 
struct TUniformParameter
{
	INT Index;
	ParameterType ShaderParameter;
	friend FArchive& operator<<(FArchive& Ar,TUniformParameter<ParameterType>& P)
	{
		return Ar << P.Index << P.ShaderParameter;
	}
};

/** Base class of the material parameters for a shader. */
class FMaterialShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap, EShaderFrequency Frequency);

	/** Sets pixel parameters that are material specific but not FMeshBatch specific. */
	template<typename ShaderRHIParamRef>
	void SetShader(
		const ShaderRHIParamRef ShaderRHI, 
		const FShaderFrequencyUniformExpressions& InExpressions, 
		const FMaterialRenderContext& MaterialRenderContext,
		FShaderFrequencyUniformExpressionValues& InValues) const;
	
	/**
	* Set the material shader parameters which depend on the mesh element being rendered.
	* @param Shader - The shader to set the parameters for.
	* @param View - The view that is being rendered.
	* @param Mesh - The mesh that is being rendered.
	*/
	template<typename ShaderRHIParamRef>
	void SetMeshShader(
		const ShaderRHIParamRef& Shader,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const FSceneView& View
		) const;

	UBOOL IsUniformExpressionSetValid(const FShaderFrequencyUniformExpressions& UniformExpressions) const;

	friend FArchive& operator<<(FArchive& Ar,FMaterialShaderParameters& Parameters);

protected:
	/** matrix parameter for materials with a world transform */
	FShaderParameter LocalToWorldParameter;
	/** matrix parameter for materials with a local transform */
	FShaderParameter WorldToLocalParameter;
	/** matrix parameter for materials with a view transform */
	FShaderParameter WorldToViewParameter;
	/** matrix parameter for materials with a view to world transform */
	FShaderParameter ViewToWorldParameter;
	/** matrix parameter for materials with a world position transform */
	FShaderParameter InvViewProjectionParameter;
	/** matrix parameter for materials with a world position node */
	FShaderParameter ViewProjectionParameter;
	/** world-space camera position */
	FShaderParameter CameraWorldPositionParameter;
	FShaderParameter TemporalAAParameters;
	/** Primitive component bounds origin and radius. */
	FShaderParameter ObjectWorldPositionAndRadiusParameter;
	/** MT-> world position of actor that owns primitive! */
	FShaderParameter ActorWorldPositionParameter;
	FShaderParameter ObjectOrientationParameter;
	FShaderParameter WindDirectionAndSpeedParameter;
	FShaderParameter FoliageImpulseDirectionParameter;
	FShaderParameter FoliageNormalizedRotationAxisAndAngleParameter;
	/** The parameters needs to calculate depth-of-field blur amount for the DOFFunction material expression. */
	FDOFShaderParameters DOFParameters;

	TArray<TUniformParameter<FShaderParameter> > UniformScalarShaderParameters;
	TArray<TUniformParameter<FShaderParameter> > UniformVectorShaderParameters;
	TArray<TUniformParameter<FShaderResourceParameter> > Uniform2DShaderResourceParameters;
};

/** An encapsulation of the material parameters for a pixel shader. */
class FMaterialPixelShaderParameters : public FMaterialShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets pixel parameters that are material specific but not FMeshBatch specific. */
	void Set(FShader* PixelShader,const FMaterialRenderContext& MaterialRenderContext, ESceneDepthUsage DepthUsage = SceneDepthUsage_Normal) const;
	
	/**
	* Set the material shader parameters which depend on the mesh element being rendered.
	* @param PixelShader - The pixel shader to set the parameters for.
	* @param View - The view that is being rendered.
	* @param Mesh - The mesh that is being rendered.
	* @param bBackFace - True if the backfaces of a two-sided material are being rendered.
	*/
	void SetMesh(
		FShader* PixelShader,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const FSceneView& View,
		UBOOL bBackFace
		) const;

	friend FArchive& operator<<(FArchive& Ar,FMaterialPixelShaderParameters& Parameters);

	UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& InSet) const;

private:
	TArray<TUniformParameter<FShaderResourceParameter> > UniformPixelCubeShaderResourceParameters;
	/** The scene texture parameters. */
	FSceneTextureShaderParameters SceneTextureParameters;
	/** Parameter indicating whether the front-side or the back-side of a two-sided material is being rendered. */
	FShaderParameter TwoSidedSignParameter;
	/** Inverse gamma parameter. Only used when USE_GAMMA_CORRECTION 1 */
	FShaderParameter InvGammaParameter;
	/** Parameter for distance to [near,far] plane for the decal (local or world space) */
	FShaderParameter DecalNearFarPlaneDistanceParameter;
	/** Object position in post projection space. */
	FShaderParameter ObjectPostProjectionPositionParameter;
	/** Object position in Normalized Device Coordinates. */
	FShaderParameter ObjectNDCPositionParameter;
	/** Scales to turn object position in post projection space into UVs in xy, NDC position into UVs in zw. */
	FShaderParameter ObjectMacroUVScalesParameter;
	/** Parameter for occlusion percentage of the object being rendered */
	FShaderParameter OcclusionPercentageParameter;
	/** Enables screen door clip masking in the pixel shader (via static branch.) */
	FShaderParameter EnableScreenDoorFadeParameter;
	/** Settings for screen door fade effect (opacity, noise scale, noise bias, noise texture scale) */
	FShaderParameter ScreenDoorFadeSettingsParameter;
	/** Additional settings for screen door fade effect (texture offset) */
	FShaderParameter ScreenDoorFadeSettings2Parameter;
	/** Noise texture which is mapped to screen space for screen door */
	FShaderResourceParameter ScreenDoorNoiseTextureParameter;
	/** Alpha sample texture. */
	FShaderResourceParameter AlphaSampleTextureParameter;
	/** Texture parameter used by the Fluid Normal node. */
	FShaderResourceParameter FluidDetailNormalTextureParameter;
};

#if WITH_D3D11_TESSELLATION

/** An encapsulation of the material parameters for a domain shader. */
class FMaterialDomainShaderParameters : public FMaterialShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets domain shader parameters that are material specific but not FMeshBatch specific. */
	void Set(FShader* DomainShader,const FMaterialRenderContext& MaterialRenderContext) const;
	
	/**
	* Set the material shader parameters which depend on the mesh element being rendered.
	* @param DomainShader - The domain shader to set the parameters for.
	* @param View - The view that is being rendered.
	* @param Mesh - The mesh that is being rendered.
	* @param bBackFace - True if the backfaces of a two-sided material are being rendered.
	*/
	void SetMesh(
		FShader* DomainShader,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const FSceneView& View
		) const;

	UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& ExpressionSet) const
	{
		return FMaterialShaderParameters::IsUniformExpressionSetValid(ExpressionSet.GetExpresssions(SF_Domain));
	}
};

/** An encapsulation of the material parameters for a hull shader. */
class FMaterialHullShaderParameters : public FMaterialShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets hull shader parameters that are material specific but not FMeshBatch specific. */
	void Set(FShader* HullShader,const FMaterialRenderContext& MaterialRenderContext) const;
	
	/**
	* Set the material shader parameters which depend on the mesh element being rendered.
	* @param HullShader - The hull shader to set the parameters for.
	* @param View - The view that is being rendered.
	* @param Mesh - The mesh that is being rendered.
	* @param bBackFace - True if the backfaces of a two-sided material are being rendered.
	*/
	void SetMesh(
		FShader* HullShader,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const FSceneView& View
		) const;

	UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& ExpressionSet) const
	{
		return FMaterialShaderParameters::IsUniformExpressionSetValid(ExpressionSet.GetExpresssions(SF_Hull));
	}
};

#endif

/** An encapsulation of the material parameters for a vertex shader. */
class FMaterialVertexShaderParameters : public FMaterialShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets vertex parameters that are material specific but not FMeshBatch specific. */
	void Set(FShader* VertexShader,const FMaterialRenderContext& MaterialRenderContext) const;

	/**
	 * Set the material shader parameters which depend on the mesh element being rendered.
	 * @param View - The view that is being rendered.
	 * @param Mesh - The mesh that is being rendered.
	 */
	void SetMesh(
		FShader* VertexShader,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		const FSceneView& View
		) const;

	UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& ExpressionSet) const
	{
		return FMaterialShaderParameters::IsUniformExpressionSetValid(ExpressionSet.GetExpresssions(SF_Vertex));
	}
};

/**
 * A shader meta type for material-linked shaders.
 */
class FMaterialShaderType : public FShaderType
{
public:

	/**
	 * Finds a FMaterialShaderType by name.
	 */
	static FMaterialShaderType* GetTypeByName(const FString& TypeName);

	struct CompiledShaderInitializerType : FGlobalShaderType::CompiledShaderInitializerType
	{
		CompiledShaderInitializerType(
			FShaderType* InType,
			const FShaderCompilerOutput& CompilerOutput
			):
			FGlobalShaderType::CompiledShaderInitializerType(InType,CompilerOutput)
		{}
	};
	typedef FShader* (*ConstructCompiledType)(const CompiledShaderInitializerType&);
	typedef UBOOL (*ShouldCacheType)(EShaderPlatform,const FMaterial*);

	FMaterialShaderType(
		const TCHAR* InName,
		const TCHAR* InSourceFilename,
		const TCHAR* InFunctionName,
		DWORD InFrequency,
		INT InMinPackageVersion,
		INT InMinLicenseePackageVersion,
		ConstructSerializedType InConstructSerializedRef,
		ConstructCompiledType InConstructCompiledRef,
		ModifyCompilationEnvironmentType InModifyCompilationEnvironmentRef,
		ShouldCacheType InShouldCacheRef
		):
		FShaderType(InName,InSourceFilename,InFunctionName,InFrequency,InMinPackageVersion,InMinLicenseePackageVersion,InConstructSerializedRef,InModifyCompilationEnvironmentRef),
		ConstructCompiledRef(InConstructCompiledRef),
		ShouldCacheRef(InShouldCacheRef)
	{}

	/**
	 * Enqueues a compilation for a new shader of this type.
	 * @param Material - The material to link the shader with.
	 * @param MaterialShaderCode - The shader code for the material.
	 */
	void BeginCompileShader(
		UINT ShaderMapId,
		const FMaterial* Material,
		const ANSICHAR* MaterialShaderCode,
		EShaderPlatform Platform
		);

	/**
	 * Either creates a new instance of this type or returns an equivalent existing shader.
	 * @param Material - The material to link the shader with.
	 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
	 */
	FShader* FinishCompileShader(
		const FUniformExpressionSet& UniformExpressionSet,
		const FShaderCompileJob& CurrentJob
		);

	/**
	 * Checks if the shader type should be cached for a particular platform and material.
	 * @param Platform - The platform to check.
	 * @param Material - The material to check.
	 * @return True if this shader type should be cached.
	 */
	UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material) const
	{
		return (*ShouldCacheRef)(Platform,Material);
	}

	// Dynamic casting.
	virtual FMaterialShaderType* GetMaterialShaderType() { return this; }

private:
	ConstructCompiledType ConstructCompiledRef;
	ShouldCacheType ShouldCacheRef;
};

/**
 * The set of material shaders for a single material.
 */
class FMaterialShaderMap : public TShaderMap<FMaterialShaderType>, public FRefCountedObject
{
public:

	/**
	 * Finds the shader map for a material.
	 * @param StaticParameterSet - The static parameter set identifying the shader map
	 * @param Platform - The platform to lookup for
	 * @return NULL if no cached shader map was found.
	 */
	static FMaterialShaderMap* FindId(const FStaticParameterSet& StaticParameterSet, EShaderPlatform Platform);

	/** Flushes the given shader types from any loaded FMaterialShaderMap's. */
	static void FlushShaderTypes(TArray<FShaderType*>& ShaderTypesToFlush, TArray<const FVertexFactoryType*>& VFTypesToFlush);

	FMaterialShaderMap() :
		CompilingId(1),
		bRegistered(FALSE),
		bCompilationFinalized(TRUE),
		bCompiledSuccessfully(TRUE),
		bIsPersistent(TRUE)
	{}

	// Destructor.
	~FMaterialShaderMap();

	/**
	 * Compiles the shaders for a material and caches them in this shader map.
	 * @param Material - The material to compile shaders for.
	 * @param InStaticParameters - the set of static parameters to compile for
	 * @param MaterialShaderCode - The shader code for Material.
	 * @param Platform - The platform to compile to
	 * @param OutErrors - Upon compilation failure, OutErrors contains a list of the errors which occured.
	 * @param bDebugDump - Dump out the preprocessed and disassembled shader for debugging.
	 * @return True if the compilation succeeded.
	 */
	UBOOL Compile(
		FMaterial* Material,
		const FStaticParameterSet* InStaticParameters, 
		const TCHAR* MaterialShaderCode,
		const FUniformExpressionSet& InUniformExpressionSet,
		EShaderPlatform Platform,
		TArray<FString>& OutErrors,
		UBOOL bDebugDump = FALSE);

	/**
	 * Checks whether the material shader map is missing any shader types necessary for the given material.
	 * @param Material - The material which is checked.
	 * @return True if the shader map has all of the shader types necessary.
	 */
	UBOOL IsComplete(const FMaterial* Material, UBOOL bSilent) const;

	/** Returns TRUE if all the shaders in this shader map have their compressed shader code in Cache. */
	UBOOL IsCompressedShaderCacheComplete(const FCompressedShaderCodeCache* const Cache) const;

	UBOOL IsUniformExpressionSetValid() const;

	/**
	 * Builds a list of the shaders in a shader map.
	 */
	void GetShaderList(TMap<FGuid,FShader*>& OutShaders) const;

	/**
	 * Begins initializing the shaders used by the material shader map.
	 */
	void BeginInit();

	/**
	 * Begins releasing the shaders used by the material shader map.
	 */
	void BeginRelease();

	/**
	 * Registers a material shader map in the global map so it can be used by materials.
	 */
	void Register();

	FMaterialShaderMap* AttemptRegistration();

	/**
	 * Merges in OtherMaterialShaderMap's shaders and FMeshMaterialShaderMaps
	 */
	void Merge(const FMaterialShaderMap* OtherMaterialShaderMap);

	/**
     * AddGuidAliases - finds corresponding guids and adds them to the FShaders alias list
	 * @param OtherMaterialShaderMap contains guids that will exist in a compressed shader cache, but will not necessarily have FShaders
	 * @return FALSE if these two shader maps are not compatible
	 */
	UBOOL AddGuidAliases(const FMaterialShaderMap* OtherMaterialShaderMap);

	/**
	 * Removes all entries in the cache with exceptions based on a shader type
	 * @param ShaderType - The shader type to flush
	 */
	void FlushShadersByShaderType(FShaderType* ShaderType);

	/**
	 * Removes all entries in the cache with exceptions based on a vertex factory type
	 * @param ShaderType - The shader type to flush
	 */
	void FlushShadersByVertexFactoryType(const FVertexFactoryType* VertexFactoryType);

	// Serializer.
	void Serialize(FArchive& Ar);

	// Accessors.
	const class FMeshMaterialShaderMap* GetMeshShaderMap(FVertexFactoryType* VertexFactoryType) const;
	const FStaticParameterSet& GetMaterialId() const { return StaticParameters; }
	EShaderPlatform GetShaderPlatform() const { return Platform; }
	const FString& GetFriendlyName() const { return FriendlyName; }
	UINT GetCompilingId() const { return CompilingId; }
	UBOOL IsCompilationFinalized() const { return bCompilationFinalized; }
	UBOOL CompiledSuccessfully() const { return bCompiledSuccessfully; }

	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& GetUniform2DTextureExpressions() const { return UniformExpressionSet.PixelExpressions.Uniform2DTextureExpressions; }
	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& GetUniformCubeTextureExpressions() const { return UniformExpressionSet.UniformCubeTextureExpressions; }
	const FUniformExpressionSet& GetUniformExpressionSet() const { return UniformExpressionSet; }
	void SetUniformExpressions(const FUniformExpressionSet& InSet) { UniformExpressionSet = InSet; }

	/** Removes a material from ShaderMapsBeingCompiled. */
	static void RemovePendingMaterial(FMaterial* Material);

	UBOOL IdenticalToCompressedCache(const FCompressedShaderCodeCache* TestCache) const
	{
		return CompressedCache == TestCache;
	}

private:

	/** A global map from a material's static parameter set to any shader map cached for that material. */
	static TMap<FStaticParameterSet,FMaterialShaderMap*> GIdToMaterialShaderMap[SP_NumPlatforms];

	/** The material's cached shaders for vertex factory type dependent shaders. */
	TIndirectArray<class FMeshMaterialShaderMap> MeshShaderMaps;

	/** The material's mesh shader maps, indexed by VFType->GetId(), for fast lookup at runtime. */
	TArray<FMeshMaterialShaderMap*> OrderedMeshShaderMaps;

	/** 
	 * Compressed shader cache used with this material's shaders.  The cache is not accessed through this member, 
	 * But the ref counted pointer keeps the cache around as long as the shader map's shaders need it.
	 */
	TRefCountPtr<FCompressedShaderCodeCache> CompressedCache;

	/** The persistent GUID of this material shader map. */
	FGuid MaterialId;

	/** The material's user friendly name, typically the object name. */
	FString FriendlyName;

	/** The platform this shader map was compiled with */
	EShaderPlatform Platform;

	/** The static parameter set that this shader map was compiled with */
	FStaticParameterSet StaticParameters;

	/** Uniform expressions generated from the material compile. */
	FUniformExpressionSet UniformExpressionSet;

	/** Next value for CompilingId. */
	static UINT NextCompilingId;

	/** Tracks material resources and their shader maps that need to be compiled but whose compilation is being deferred. */
	static TMap<FMaterialShaderMap*, TArray<FMaterial*> > ShaderMapsBeingCompiled;

	/** 
	 * Map from shader map CompilingId to an ANSI version of the material's shader code.
	 * This is stored outside of the shader's environment includes to reduce memory usage, since many shaders share the same material shader code.
	 */
	static TMap<UINT, const ANSICHAR*> MaterialCodeBeingCompiled;

	/** Uniquely identifies this shader map during compilation, needed for deferred compilation where shaders from multiple shader maps are compiled together. */
	UINT CompilingId;

	/** Indicates whether this shader map has been registered in GIdToMaterialShaderMap */
	BITFIELD bRegistered : 1;

	/** 
	 * Indicates whether this shader map has had ProcessCompilationResults called after Compile.
	 * The shader map must not be used on the rendering thread unless bCompilationFinalized is TRUE.
	 */
	BITFIELD bCompilationFinalized : 1;

	BITFIELD bCompiledSuccessfully : 1;

	/** Indicates whether the shader map should be stored in the shader cache. */
	BITFIELD bIsPersistent : 1;

	/**
	 * Initializes OrderedMeshShaderMaps from the contents of MeshShaderMaps.
	 */
	void InitOrderedMeshShaderMaps();

	/** 
	 * Processes an array of completed shader compile jobs. 
	 * This is called by FShaderCompilingThreadManager after compilation of this shader map's shaders has completed.E.
	 */
	void ProcessCompilationResults(const TArray<TRefCountPtr<FShaderCompileJob> >& InCompilationResults, UBOOL bSuccess);

	friend class UShaderCache;
	friend void DumpMaterialStats( EShaderPlatform Platform );
	friend class FShaderCompilingThreadManager;
};
