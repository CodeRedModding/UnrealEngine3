/*=============================================================================
	ShaderManager.cpp: Shader manager implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "DiagnosticTable.h"

EShaderPlatform GRHIShaderPlatform = SP_PCD3D_SM3;
/** Shader platform to cook for, not meaningful unless GIsCooking is TRUE */
EShaderPlatform GCookingShaderPlatform = SP_PCD3D_SM3;
/** Material quality to cook for, not meaningful unless GIsCooking is TRUE */
EMaterialShaderQuality GCookingMaterialQuality = MSQ_HIGH;

/** Force minimal shader compiling 
 *  Experimental: This is used for commandlets that do not need physical shaders for anything.
**/
#if !CONSOLE
UBOOL GForceMinimalShaderCompilation = FALSE;
#endif

// Pull the shader cache stats into this context
STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_CompressedShaderMemory);
STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_UncompressedShaderMemory);

/** Returns compression flags to be used when compressing shaders for the given platform. */
ECompressionFlags GetShaderCompressionFlags(EShaderPlatform Platform)
{
	check(UseShaderCompression(Platform));
	if (Platform == SP_XBOXD3D)
	{
		// Using LZX on xbox 360 as it tends to compress ~6x faster and produce ~8% smaller results than LZO, although with about half as fast decompression.
		// LZX compresses ~8x slower than zlib, but with ~4% smaller results and ~50% faster decompression.
		return (ECompressionFlags)(COMPRESS_LZX | COMPRESS_BiasMemory);
	}
	else if (Platform == SP_PS3)
	{
		// Using ZLib as that's the only decompressor supported by appUncompressMemory on PS3
		//@todo - implement and try other compressors to get smaller results or faster decompression
		return (ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory);
	}
	return COMPRESS_None;
}

/** 
 * Returns the target shader chunk size for the given platform.
 * Larger values result in a better compression ratio, but make decompressing individual shaders at runtime cause more of a hitch. 
 */
UINT GetCompressedShaderChunkSizeTarget(EShaderPlatform Platform)
{
	check(UseShaderCompression(Platform));
	if (Platform == SP_XBOXD3D)
	{
		// Tweaked for a good tradeoff between compression ratio and decompression hitches
		return 32 * 1024;
	}
	else if (Platform == SP_PS3)
	{
		// Currently using PPU decompression which is quite slow, so use a smaller chunk size
		return 4 * 1024;
	}
	return 0;
}

/** 
* When enabled, shaders will be initialized individually when needed for rendering, 
* instead of all the shaders in a material shader map getting initialized when the material is loaded.
* This is useful for working around Nvidia drivers running out of paged pool memory, 
* but as a result there will be a hitch the first time a new shader is needed for rendering.
*/
static UBOOL ShouldInitShadersOnDemand()
{
	static UBOOL bInitShadersOnDemand = FALSE;
	static UBOOL bInitialized = FALSE;
	#if !CONSOLE
		if(!bInitialized)
		{
			bInitialized = TRUE;
			// get the option to initialize shaders on demand from the engine ini
			GConfig->GetBool( TEXT("Engine.ISVHacks"), TEXT("bInitializeShadersOnDemand"), bInitShadersOnDemand, GEngineIni );
		}
	#endif
	return bInitShadersOnDemand;
}

/** The shader file cache, used to minimize shader file reads */
TMap<FFilename, FString> GShaderFileCache;

/** Protects GShaderFileCache from simultaneous access by multiple threads. */
FCriticalSection FileCacheCriticalSection;

/** The shader file hash cache, used to minimize loading and hashing shader files */
TMap<FString, FSHAHash> GShaderHashCache;

UBOOL FShaderParameterMap::FindParameterAllocation(const TCHAR* ParameterName,WORD& OutBufferIndex,WORD& OutBaseIndex,WORD& OutSize,WORD& OutSamplerIndex) const
{
	const FParameterAllocation* Allocation = ParameterMap.Find(ParameterName);
	if(Allocation)
	{
		OutBufferIndex = Allocation->BufferIndex;
		OutBaseIndex = Allocation->BaseIndex;
		OutSize = Allocation->Size;
		OutSamplerIndex = Allocation->SamplerIndex;
		Allocation->bBound = TRUE;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void FShaderParameterMap::AddParameterAllocation(const TCHAR* ParameterName,WORD BufferIndex,WORD BaseIndex,WORD Size,WORD SamplerIndex)
{
	FParameterAllocation Allocation;
	Allocation.BufferIndex = BufferIndex;
	Allocation.BaseIndex = BaseIndex;
	Allocation.Size = Size;
	Allocation.SamplerIndex = SamplerIndex;
	ParameterMap.Set(ParameterName,Allocation);
}

/** Returns TRUE if the specified parameter is bound by RHISetViewParameters instead of by the shader */
static UBOOL IsParameterBoundByView(const FString& ParameterName, EShaderFrequency Frequency)
{
	UBOOL bParameterBoundByView = FALSE;
	if (Frequency == SF_Vertex)
	{
		bParameterBoundByView = ParameterName == TEXT("ViewProjectionMatrix") 
			|| ParameterName == TEXT("CameraPositionVS")
#if WITH_REALD
			|| ParameterName == TEXT("VSRRealDCoefficients1")
#endif	//WITH_REALD
			|| ParameterName == TEXT("PreViewTranslation");
	}
	else if (Frequency == SF_Hull)
	{
		bParameterBoundByView = ParameterName == TEXT("ViewProjectionMatrixHS") 
			|| ParameterName == TEXT("AdaptiveTessellationFactor")
			|| ParameterName == TEXT("ProjectionScaleY");
	}
	else if (Frequency == SF_Domain)
	{
		bParameterBoundByView = ParameterName == TEXT("ViewProjectionMatrixDS")
			|| ParameterName == TEXT("CameraPositionDS");
	}
	else if (Frequency == SF_Pixel)
	{
		bParameterBoundByView = ParameterName == TEXT("ScreenPositionScaleBias") 
			|| ParameterName == TEXT("MinZ_MaxZRatio") 
			|| ParameterName == TEXT("SCENE_COLOR_BIAS_FACTOR")
			|| ParameterName == TEXT("NvStereoEnabled")
			|| ParameterName == TEXT("DiffuseOverrideParameter")
			|| ParameterName == TEXT("SpecularOverrideParameter")
#if WITH_REALD
			|| ParameterName == TEXT("PSRRealDCoefficients1")
#endif	//WITH_REALD
			|| ParameterName == TEXT("CameraPositionPS");
	}

	return bParameterBoundByView;
}

/** Returns TRUE if the specified parameter being unbound is a known issue */
static UBOOL IsKnownUnboundParameter(const FString& ParameterName, EShaderFrequency Frequency)
{
	UBOOL bMakeException = FALSE;
	if (Frequency == SF_Pixel)
	{
		bMakeException = 
			// TTP 84767 is open to fix these
			ParameterName == TEXT("DecalLocalBinormal") 
			|| ParameterName == TEXT("DecalLocalTangent");
	}
	return bMakeException;
}

static UBOOL IsRelevantParameter(const FString& ParameterName, EShaderFrequency Frequency)
{
	if (Frequency != SF_Vertex && ParameterName.InStr(TEXT("UniformVertex")) != INDEX_NONE)
	{
		return FALSE;
	}
	else if (Frequency != SF_Hull && ParameterName.InStr(TEXT("UniformHull")) != INDEX_NONE)
	{
		return FALSE;
	}
	else if (Frequency != SF_Domain && ParameterName.InStr(TEXT("UniformDomain")) != INDEX_NONE)
	{
		return FALSE;
	}
	else if (Frequency != SF_Pixel && ParameterName.InStr(TEXT("UniformPixel")) != INDEX_NONE)
	{
		return FALSE;
	}
	return TRUE;
}

/** Checks that all parameters are bound and asserts if any aren't in a debug build */
void FShaderParameterMap::VerifyBindingsAreComplete(const TCHAR* ShaderTypeName, EShaderFrequency Frequency, FVertexFactoryType* InVertexFactoryType) const
{
#if _DEBUG && !CONSOLE
	// Only people working on shaders (and therefore have DevShaders unsuppressed) will want to see these errors
	if (!FName::SafeSuppressed(NAME_DevShaders))
	{
		const TCHAR* VertexFactoryName = InVertexFactoryType ? InVertexFactoryType->GetName() : TEXT("?");

		UBOOL bBindingsComplete = TRUE;
		FString UnBoundParameters = TEXT("");
		for (TMap<FString,FParameterAllocation>::TConstIterator ParameterIt(ParameterMap);ParameterIt;++ParameterIt)
		{
			const FString& ParamName = ParameterIt.Key();
			const FParameterAllocation& ParamValue = ParameterIt.Value();
			if (!ParamValue.bBound 
				&& !IsParameterBoundByView(ParamName, Frequency) 
				&& !IsKnownUnboundParameter(ParamName, Frequency)
				&& IsRelevantParameter(ParamName, Frequency))
			{
				// Only valid parameters should be in the shader map
				checkSlow(ParamValue.Size > 0);
				bBindingsComplete = bBindingsComplete && ParamValue.bBound;
				UnBoundParameters += FString(TEXT("		Parameter ")) + ParamName + TEXT(" not bound!\n");
			}
		}
	
		if (!bBindingsComplete)
		{
			FString ErrorMessage = FString(TEXT("Found unbound parameters being used in shadertype ")) + ShaderTypeName + TEXT(" (VertexFactory: ") + VertexFactoryName + TEXT(")\n") + UnBoundParameters;
			// An unbound parameter means the engine is not going to set its value (because it was never bound) 
			// but it will be used in rendering, which will most likely cause artifacts
			appErrorf(*ErrorMessage);
		}
	}
#endif
}

void FShaderParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,UBOOL bIsOptional)
{
	WORD UnusedSamplerIndex = 0;
#if !PLATFORM_SUPPORTS_D3D10_PLUS
	WORD BufferIndex = 0;
#endif

#if _DEBUG
	bInitialized = TRUE;
#endif

	if(!ParameterMap.FindParameterAllocation(ParameterName,BufferIndex,BaseIndex,NumBytes,UnusedSamplerIndex) && !bIsOptional)
	{
		if (FName::SafeSuppressed(NAME_DevShaders))
		{
			appErrorf(TEXT("Failure to bind non-optional shader parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
		}
		else
		{
			appMsgf(AMT_OK, 
				TEXT("Failure to bind non-optional shader parameter %s! ")
				TEXT("The parameter is either not present in the shader, or the shader compiler optimized it out. \n\n ")
				TEXT("This will be an assert with DevShaders suppressed!"),
				ParameterName);
		}
	}
}

FArchive& operator<<(FArchive& Ar,FShaderParameter& P)
{
#if _DEBUG
	if (Ar.IsLoading())
	{
		P.bInitialized = TRUE;
	}
#endif

#if PLATFORM_SUPPORTS_D3D10_PLUS
	WORD& PBufferIndex = P.BufferIndex;
#else
	WORD PBufferIndex = 0;
#endif
	return Ar << P.BaseIndex << P.NumBytes << PBufferIndex;
}

void FShaderResourceParameter::Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,UBOOL bIsOptional)
{
	WORD UnusedBufferIndex = 0;
#if !PLATFORM_SUPPORTS_D3D10_PLUS
	WORD SamplerIndex = 0;
#endif
	if(!ParameterMap.FindParameterAllocation(ParameterName,UnusedBufferIndex,BaseIndex,NumResources,SamplerIndex) && !bIsOptional)
	{
		if (FName::SafeSuppressed(NAME_DevShaders))
		{
			appErrorf(TEXT("Failure to bind non-optional shader resource parameter %s!  The parameter is either not present in the shader, or the shader compiler optimized it out."),ParameterName);
		}
		else
		{
			appMsgf(AMT_OK, 
				TEXT("Failure to bind non-optional shader parameter %s! ")
				TEXT("The parameter is either not present in the shader, or the shader compiler optimized it out. \n\n ")
				TEXT("This will be an assert with DevShaders suppressed!"),
				ParameterName);
		}
	}
}

FArchive& operator<<(FArchive& Ar,FShaderResourceParameter& P)
{
#if PLATFORM_SUPPORTS_D3D10_PLUS
	WORD& PSamplerIndex = P.SamplerIndex;
#else
	WORD PSamplerIndex = 0;
#endif
	return Ar << P.BaseIndex << P.NumResources << PSamplerIndex;
}

/**
 * Sets the value of a pixel shader bool parameter.
 */
void SetPixelShaderBool(
	FPixelShaderRHIParamRef PixelShader,
	const FShaderParameter& Parameter,
	UBOOL Value
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if (Parameter.GetNumBytes() > 0)
	{
		RHISetPixelShaderBoolParameter(
			PixelShader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex(),
			Value
			);
	}
}

/**
* Sets the value of a pixel shader bool parameter.
*/
void SetVertexShaderBool(
	FVertexShaderRHIParamRef VertexShader,
	const FShaderParameter& Parameter,
	UBOOL Value
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if (Parameter.GetNumBytes() > 0)
	{
		RHISetVertexShaderBoolParameter(
			VertexShader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex(),
			Value
			);
	}
}

TLinkedList<FShaderType*>*& FShaderType::GetTypeList()
{
	static TLinkedList<FShaderType*>* TypeList = NULL;
	return TypeList;
}

/**
* @return The global shader name to type map
*/
TMap<FName, FShaderType*>& FShaderType::GetNameToTypeMap()
{
	static TMap<FName, FShaderType*> NameToTypeMap;
	return NameToTypeMap;
}

/**
* Gets a list of FShaderTypes whose source file no longer matches what that type was compiled with
*/
void FShaderType::GetOutdatedTypes(TArray<FShaderType*>& OutdatedShaderTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes)
{
	for(TLinkedList<FShaderType*>::TIterator It(GetTypeList()); It; It.Next())
	{
		FShaderType* Type = *It;
		for(TMap<FGuid,FShader*>::TConstIterator ShaderIt(Type->ShaderIdMap);ShaderIt;++ShaderIt)
		{
			FShader* Shader = ShaderIt.Value();
			const FVertexFactoryParameterRef* VFParameterRef = Shader->GetVertexFactoryParameterRef();
			const FSHAHash& SavedHash = Shader->GetHash();
			const FSHAHash& CurrentHash = Type->GetSourceHash();
			const UBOOL bOutdatedShader = SavedHash != CurrentHash;
			const UBOOL bOutdatedVertexFactory =
				VFParameterRef && VFParameterRef->GetVertexFactoryType() && VFParameterRef->GetVertexFactoryType()->GetSourceHash() != VFParameterRef->GetHash();

			if (bOutdatedShader)
			{
				OutdatedShaderTypes.AddUniqueItem(Shader->Type);
			}

			if (bOutdatedVertexFactory)
			{
				OutdatedFactoryTypes.AddUniqueItem(VFParameterRef->GetVertexFactoryType());
			}
		}
	}

	for (INT TypeIndex = 0; TypeIndex < OutdatedShaderTypes.Num(); TypeIndex++)
	{
		warnf(TEXT("		Recompiling %s"), OutdatedShaderTypes(TypeIndex)->GetName());
	}
	for (INT TypeIndex = 0; TypeIndex < OutdatedFactoryTypes.Num(); TypeIndex++)
	{
		warnf(TEXT("		Recompiling %s"), OutdatedFactoryTypes(TypeIndex)->GetName());
	}
}

FArchive& operator<<(FArchive& Ar,FShaderType*& Ref)
{
	if(Ar.IsSaving())
	{
		FName FactoryName = Ref ? FName(Ref->Name) : NAME_None;
		Ar << FactoryName;
	}
	else if(Ar.IsLoading())
	{
		FName FactoryName = NAME_None;
		Ar << FactoryName;
		
		Ref = NULL;

		if(FactoryName != NAME_None)
		{
			// look for the shader type in the global name to type map
			FShaderType** ShaderType = FShaderType::GetNameToTypeMap().Find(FactoryName);
			if (ShaderType)
			{
				// if we found it, use it
				Ref = *ShaderType;
			}
		}
	}
	return Ar;
}

void FShaderType::RegisterShader(FShader* Shader)
{
	ShaderIdMap.Set(Shader->GetId(),Shader);
#if !CONSOLE
	Shader->CodeMapId = ShaderCodeMap.Add(Shader);
#endif
}

void FShaderType::DeregisterShader(FShader* Shader)
{
	ShaderIdMap.Remove(Shader->GetId());
#if !CONSOLE
	// Verify that CodeMapId is still valid
	// This catches double removes
	checkSlow(ShaderCodeMap.IsValidId(Shader->CodeMapId));
	checkSlow(ShaderCodeMap(Shader->CodeMapId) == Shader);
	ShaderCodeMap.Remove(Shader->CodeMapId);
#endif
}

FShader* FShaderType::FindShaderByOutput(const FShaderCompilerOutput& Output) const
{
#if !CONSOLE
	FSetElementId CodeMapId = ShaderCodeMap.FindId(FShaderKey(Output.Code, Output.ParameterMap));
	return CodeMapId.IsValidId() ? ShaderCodeMap(CodeMapId) : NULL;
#else
	return NULL;
#endif
}

FShader* FShaderType::FindShaderById(const FGuid& Id) const
{
	return ShaderIdMap.FindRef(Id);
}

FShader* FShaderType::ConstructForDeserialization() const
{
	return (*ConstructSerializedRef)();
}

/** Calculates a Hash based on this shader type's source code and includes */
const FSHAHash& FShaderType::GetSourceHash() const
{
#if CONSOLE
	static FSHAHash CurrentHash = {0};
	return CurrentHash;
#else
	return GetShaderFileHash(GetShaderFilename());
#endif
}

/**
 * Enqueues a shader to be compiled with the shader type's compilation parameters, using the provided shader environment.
 * @param VFType - Optional vertex factory type that the shader belongs to.
 * @param Platform - Platform to compile for.
 * @param Environment - The environment to compile the shader in.
 */
void FShaderType::BeginCompileShader(UINT Id, FVertexFactoryType* VFType, EShaderPlatform Platform, const FShaderCompilerEnvironment& InEnvironment)
{
	// Allow the shader type to modify its compile environment.
	FShaderCompilerEnvironment Environment = InEnvironment;
	(*ModifyCompilationEnvironmentRef)(Platform, Environment);

	// Construct shader target for the shader type's frequency and the specified platform.
	FShaderTarget Target;
	Target.Platform = Platform;
	Target.Frequency = Frequency;

	// Compile the shader environment passed in with the shader type's source code.
	::BeginCompileShader(
		Id,
		VFType,
		this,
		SourceFilename,
		FunctionName,
		Target,
		Environment
		);
}

FShader::FShader() : 
	Type(NULL), 
	NumRefs(0),
	NumResourceInitRefs(0)
{
	// set to undefined (currently shared with SF_Vertex)
	Target.Frequency = 0;
	Target.Platform = GRHIShaderPlatform;
	INC_DWORD_STAT_BY(STAT_ShaderCompression_NumShadersLoaded, 1);
}

FShader::FShader(const FGlobalShaderType::CompiledShaderInitializerType& Initializer):
	Key(Initializer.Code, Initializer.ParameterMap),
	Target(Initializer.Target),
	Type(Initializer.Type),
	NumRefs(0),
	NumInstructions(Initializer.NumInstructions),
	NumResourceInitRefs(0)
{
	check(Initializer.Code.Num() > 0);
	Id = appCreateGuid();
	if (Type)
	{
		Type->RegisterShader(this);
#if !CONSOLE
		Hash = Type->GetSourceHash();
#endif
	}

	INC_DWORD_STAT_BY(GetMemoryStatType((EShaderFrequency)Target.Frequency), Key.Code.Num());
	INC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, Key.Code.Num());
	INC_DWORD_STAT_BY(STAT_ShaderCompression_NumShadersLoaded, 1);
}

FShader::~FShader()
{
	DEC_DWORD_STAT_BY(GetMemoryStatType((EShaderFrequency)Target.Frequency), Key.Code.Num());
	DEC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, Key.Code.Num());
	DEC_DWORD_STAT_BY(STAT_ShaderCompression_NumShadersLoaded, 1);
}

void FShader::InitializeVertexShader() 
{ 
#if INIT_SHADERS_ON_DEMAND
	if (!IsInitialized())
	{
		STAT(DOUBLE ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_ShaderCompression_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);
			InitResource();
		}

		INC_FLOAT_STAT_BY(STAT_ShaderCompression_TotalRTShaderInitForRenderingTime,(FLOAT)ShaderInitializationTime);
	}

#else
	// If the shader resource hasn't been initialized yet, initialize it.
	// In game, shaders are initialized on load by default, but they will be initialized on demand if ShouldInitShadersOnDemand() is true.
	if(!CONSOLE && 
		(GIsEditor || ShouldInitShadersOnDemand()) && 
		!IsInitialized())
	{
		InitResource();
	}
#endif

	checkSlow(IsInitialized());
}

void FShader::InitializePixelShader() 
{ 
#if INIT_SHADERS_ON_DEMAND
	if (!IsInitialized())
	{
		STAT(DOUBLE ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_ShaderCompression_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);
			InitResource();
		}

		INC_FLOAT_STAT_BY(STAT_ShaderCompression_TotalRTShaderInitForRenderingTime,(FLOAT)ShaderInitializationTime);
	}

#else
	// If the shader resource hasn't been initialized yet, initialize it.
	// In game, shaders are initialized on load by default, but they will be initialized on demand if ShouldInitShadersOnDemand() is true.
	if(!CONSOLE && 
		(GIsEditor || ShouldInitShadersOnDemand()) && 
		!IsInitialized())
	{
		InitResource();
	}
#endif

	checkSlow(IsInitialized());
}

#if WITH_D3D11_TESSELLATION
/**
* @return the shader's hull shader
*/
const FHullShaderRHIRef& FShader::GetHullShader() 
{ 
#if INIT_SHADERS_ON_DEMAND
	if (!IsInitialized())
	{
		STAT(DOUBLE ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_ShaderCompression_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);
			InitResource();
		}

		INC_FLOAT_STAT_BY(STAT_ShaderCompression_TotalRTShaderInitForRenderingTime,(FLOAT)ShaderInitializationTime);
	}
	
#else
	// If the shader resource hasn't been initialized yet, initialize it.
	// In game, shaders are initialized on load by default, but they will be initialized on demand if ShouldInitShadersOnDemand() is true.
	if(	(GIsEditor || ShouldInitShadersOnDemand()) && !IsInitialized())
	{
		InitResource();
	}
#endif

	checkSlow(IsInitialized());

	return HullShader; 
}

/**
* @return the shader's domain shader
*/
const FDomainShaderRHIRef& FShader::GetDomainShader() 
{ 
#if INIT_SHADERS_ON_DEMAND
	if (!IsInitialized())
	{
		STAT(DOUBLE ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_ShaderCompression_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);
			InitResource();
		}
		
		INC_FLOAT_STAT_BY(STAT_ShaderCompression_TotalRTShaderInitForRenderingTime,(FLOAT)ShaderInitializationTime);
	}
	
#else
	// If the shader resource hasn't been initialized yet, initialize it.
	// In game, shaders are initialized on load by default, but they will be initialized on demand if ShouldInitShadersOnDemand() is true.
	if(	(GIsEditor || ShouldInitShadersOnDemand()) && !IsInitialized())
	{
		InitResource();
	}
#endif

	checkSlow(IsInitialized());

	return DomainShader; 
}

/**
* @return the shader's geometry shader
*/
const FGeometryShaderRHIRef& FShader::GetGeometryShader() 
{ 
#if INIT_SHADERS_ON_DEMAND
	if (!IsInitialized())
	{
		STAT(DOUBLE ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_ShaderCompression_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);
			InitResource();
		}

		INC_FLOAT_STAT_BY(STAT_ShaderCompression_TotalRTShaderInitForRenderingTime,(FLOAT)ShaderInitializationTime);
	}

#else
	// If the shader resource hasn't been initialized yet, initialize it.
	// In game, shaders are initialized on load by default, but they will be initialized on demand if ShouldInitShadersOnDemand() is true.
	if(	(GIsEditor || ShouldInitShadersOnDemand()) && !IsInitialized())
	{
		InitResource();
	}
#endif

	checkSlow(IsInitialized());

	return GeometryShader; 
}

/**
* @return the shader's compute shader
*/
const FComputeShaderRHIRef& FShader::GetComputeShader() 
{ 
#if INIT_SHADERS_ON_DEMAND
	if (!IsInitialized())
	{
		STAT(DOUBLE ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_ShaderCompression_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);
			InitResource();
		}

		INC_FLOAT_STAT_BY(STAT_ShaderCompression_TotalRTShaderInitForRenderingTime,(FLOAT)ShaderInitializationTime);
	}

#else
	// If the shader resource hasn't been initialized yet, initialize it.
	// In game, shaders are initialized on load by default, but they will be initialized on demand if ShouldInitShadersOnDemand() is true.
	if(	(GIsEditor || ShouldInitShadersOnDemand()) && !IsInitialized())
	{
		InitResource();
	}
#endif

	checkSlow(IsInitialized());

	return ComputeShader; 
}
#endif

/** Returns the hash of the shader file that this shader was compiled with. */
const FSHAHash& FShader::GetHash() const 
{ 
#if CONSOLE
	static FSHAHash Dummy = {0};
	return Dummy;
#else
	return Hash;
#endif
}

/**
 * Adds the guid from another shader to my guid alias table, unless I am already decompressed
 * @param Other shader to get guid from
 */
void FShader::AddAlias(const FShader* Other)
{
	if (Other==this)
	{
		return;
	}
	check(GetId() != Other->GetId()); // this would be nonsense, but we could check that and return
	if (UseShaderCompression((EShaderPlatform)Target.Platform))
	{
		if (!IsInitialized())
		{
			Aliases.AddUniqueItem(Other->Id);
			for (INT AliasIndex = 0; AliasIndex < Other->Aliases.Num(); AliasIndex++)
			{
				Aliases.AddUniqueItem(Other->Aliases(AliasIndex));
			}
		}
	}
}


UBOOL FShader::Serialize(FArchive& Ar)
{
	BYTE TargetPlatform = Target.Platform;
	BYTE TargetFrequency = Target.Frequency;
	Ar << TargetPlatform << TargetFrequency;
	Target.Platform = TargetPlatform;
	Target.Frequency = TargetFrequency;

	UBOOL bDiscardShaderSource = FALSE;

	// Don't serialize uncompressed shader source for cooked platforms that implement shader compression. We do however want
	// to save the raw uncompressed shader source for the local shader cache being kept on the PC. This does not discard
	// shader source from the global shader cache as it is serialized as a binary file and not a UPackage.
	if( GIsCooking
	&&	UseShaderCompression((EShaderPlatform)Target.Platform)
	&&	Ar.ContainsCookedData() )
	{
		bDiscardShaderSource = TRUE;
	}

	// Don't serialize uncompressed shader source for any mobile cooked platforms that don't have their
	// own shader platform
	// (only strip shaders from cooked packages, so we don't strip from the PC caches
	// that may be saved during mobile cooking)
	if( Ar.GetLinker() && (Ar.GetLinker()->LinkerRoot->PackageFlags & PKG_Cooked) && 
		GIsCooking && (GCookingTarget & UE3::PLATFORM_Mobile) &&
		Target.Platform == SP_PCD3D_SM3)
	{
		bDiscardShaderSource = TRUE;
	}

	if( Ar.IsSaving() && bDiscardShaderSource )
	{
		TArray<BYTE> Empty;
		Ar << Empty;
	}
	else
	{
		Ar << Key.Code;
	}

	// Discard uncompressed shader source from global shader cache. The above check won't catch this as the global shader file
	// is serialized as a raw binary file.
	if( Ar.IsLoading() 
	&&	GRHIShaderPlatform == (EShaderPlatform)Target.Platform
	&&	UseShaderCompression((EShaderPlatform)Target.Platform) )
	{
		Key.Code.Empty();
	}

	Ar << Key.ParameterMapCRC;
	Ar << Id << Type;

#if CONSOLE
	FSHAHash Dummy;
	Ar << Dummy;
#else
	Ar << Hash;
#endif

	if(Ar.IsLoading() && Type)
	{
		Type->RegisterShader(this);
	}

	Ar << NumInstructions;
	
	if (Ar.IsLoading())
	{
		INC_DWORD_STAT_BY(GetMemoryStatType((EShaderFrequency)Target.Frequency), Key.Code.Num());
		INC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, Key.Code.Num());
	}
	return FALSE;
}

void FShader::InitRHI()
{
	// we can't have this called on the wrong platform's shaders
	if (Target.Platform != GRHIShaderPlatform)
	{
#if CONSOLE
		appErrorf( TEXT("FShader::Init got platform %s but expected %s"), ShaderPlatformToText((EShaderPlatform)Target.Platform), ShaderPlatformToText(GRHIShaderPlatform) );
#endif
		return;
	}

	INC_DWORD_STAT_BY(STAT_ShaderCompression_NumShadersUsedForRendering, 1);
#if !INIT_SHADERS_ON_DEMAND
	STAT(DOUBLE ShaderInitializationTime = 0);
	{
	SCOPE_SECONDS_COUNTER(ShaderInitializationTime);
	checkf(!UseShaderCompression((EShaderPlatform)Target.Platform), TEXT("Shader compression requires INIT_SHADERS_ON_DEMAND to be enabled!"));
#endif

	if (UseShaderCompression((EShaderPlatform)Target.Platform))
	{
		checkSlow(!Key.Code.Num());
		UBOOL bAnyDecompressionSucceeded = FALSE;

		// Search through the loaded compressed shader caches and decompress the shader's code from the cache that contains it
		for (INT CacheIndex = 0; CacheIndex < GCompressedShaderCaches[Target.Platform].Num(); CacheIndex++)
		{
			const FCompressedShaderCodeCache* CompressedCache = GCompressedShaderCaches[Target.Platform](CacheIndex);
			if (CompressedCache->DecompressShaderCode(this, GetId(), (EShaderPlatform)Target.Platform, Key.Code))
			{
				bAnyDecompressionSucceeded = TRUE;
				break;
			}
		}
		for (INT AliasIndex = 0; AliasIndex < Aliases.Num() && !bAnyDecompressionSucceeded; AliasIndex++)
		{
			for (INT CacheIndex = 0; CacheIndex < GCompressedShaderCaches[Target.Platform].Num(); CacheIndex++)
			{
				const FCompressedShaderCodeCache* CompressedCache = GCompressedShaderCaches[Target.Platform](CacheIndex);
				if (CompressedCache->DecompressShaderCode(this, Aliases(AliasIndex), (EShaderPlatform)Target.Platform, Key.Code))
				{
					bAnyDecompressionSucceeded = TRUE;
					break;
				}
			}
		}

		checkf(bAnyDecompressionSucceeded, TEXT("Failed to find compressed shader code for %s in any shader cache!"), Type->GetName());

	}
	Aliases.Empty(); // will never need these again

	if(Target.Frequency == SF_Vertex)
	{
		VertexShader = RHICreateVertexShader(Key.Code);
	}
	else if(Target.Frequency == SF_Pixel)
	{
		PixelShader = RHICreatePixelShader(Key.Code);
	}
#if WITH_D3D11_TESSELLATION
	else if(Target.Frequency == SF_Hull)
	{
		HullShader = RHICreateHullShader(Key.Code);
	}
	else if(Target.Frequency == SF_Domain)
	{
		DomainShader = RHICreateDomainShader(Key.Code);
	}
	else if(Target.Frequency == SF_Geometry)
	{
		GeometryShader = RHICreateGeometryShader(Key.Code);
	}
	else if(Target.Frequency == SF_Compute)
	{
		ComputeShader = RHICreateComputeShader(Key.Code);
	}
#endif
	

#if CONSOLE
	if (!UseShaderCompression((EShaderPlatform)Target.Platform))
	{
		DEC_DWORD_STAT_BY(GetMemoryStatType((EShaderFrequency)Target.Frequency), Key.Code.Num());
		DEC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_UncompressedShaderMemory, Key.Code.Num());
	}

	// We need to hold onto the key information
	// Memory has been duplicated at this point.
	if( !GAllowFullRHIReset )
	{
		Key.Code.Empty();
	}

#endif

#if !INIT_SHADERS_ON_DEMAND
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompression_RTShaderLoadTime,(FLOAT)ShaderInitializationTime);
	INC_DWORD_STAT_BY(STAT_ShaderCompression_NumShadersLoaded, 1);
#endif
}

void FShader::ReleaseRHI()
{
	DEC_DWORD_STAT_BY(STAT_ShaderCompression_NumShadersUsedForRendering, 1);

	VertexShader.SafeRelease();
	PixelShader.SafeRelease();
}

void FShader::AddRef()
{
	++NumRefs;
}

void FShader::Release()
{
	check(NumRefs != 0);
	if(--NumRefs == 0)
	{
		// Deregister the shader now to eliminate references to it by the type's ShaderIdMap and ShaderCodeMap.
		Type->DeregisterShader(this);

		// Send a release message to the rendering thread when the shader loses its last reference.
		BeginReleaseResource(this);

		BeginCleanup(this);
	}
}

void FShader::FinishCleanup()
{
	delete this;
}

void FShader::BeginInit()
{
	NumResourceInitRefs++;
	// Initialize the shader's resources the first time it is requested
	// unless we are initializing on demand only, in which case the shader will be initialized 
	// in GetPixelShader or GetVertexShader when it is requested for rendering.
	if (NumResourceInitRefs == 1 && !ShouldInitShadersOnDemand())
	{
		BeginInitResource(this);
	}
}

void FShader::BeginRelease()
{
	// No need to ever release shader resources through this mechanism on console.  
	// Instead, shader resources will be released when the FShader is destroyed, which happens on map transition.
	// This allows us to throw away the game thread copy of the shader data when initializing the resource (Key.Code)
#if !CONSOLE
	// In the editor shader resources are initialized individually on demand, and currently not released
	check(!GIsEditor);
	NumResourceInitRefs--;
	check(NumResourceInitRefs >= 0);
	// Release the shader's resources the last time it is released
	if (NumResourceInitRefs == 0)
	{
		BeginReleaseResource(this);
	}
#endif
}

FArchive& operator<<(FArchive& Ar,FShader*& Ref)
{
	if(Ar.IsSaving())
	{
		if(Ref)
		{
			// Serialize the shader's ID and type.
			FGuid ShaderId = Ref->GetId();
			FShaderType* ShaderType = Ref->GetType();
			Ar << ShaderId << ShaderType;
		}
		else
		{
			FGuid ShaderId(0,0,0,0);
			FShaderType* ShaderType = NULL;
			Ar << ShaderId << ShaderType;
		}
	}
	else if(Ar.IsLoading())
	{
		// Deserialize the shader's ID and type.
		FGuid ShaderId;
		FShaderType* ShaderType = NULL;
		Ar << ShaderId << ShaderType;

		Ref = NULL;

		if(ShaderType)
		{
			// Find the shader using the ID and type.
			Ref = ShaderType->FindShaderById(ShaderId);
		}
	}

	return Ar;
}

/**
 * Recursively populates IncludeFilenames with the unique include filenames found in the shader file named Filename.
 */
void GetShaderIncludes(const TCHAR* Filename, TArray<FString> &IncludeFilenames, UINT DepthLimit)
{
	FString FileContents = LoadShaderSourceFile(Filename);
	//avoid an infinite loop with a 0 length string
	check(FileContents.Len() > 0);

	//find the first include directive
	TCHAR* IncludeBegin = appStrstr(*FileContents, TEXT("#include "));

	UINT SearchCount = 0;
	const UINT MaxSearchCount = 20;
	//keep searching for includes as long as we are finding new ones and haven't exceeded the fixed limit
	while (IncludeBegin != NULL && SearchCount < MaxSearchCount && DepthLimit > 0)
	{
		//find the first double quotation after the include directive
		TCHAR* IncludeFilenameBegin = appStrstr(IncludeBegin, TEXT("\""));
		//find the trailing double quotation
		TCHAR* IncludeFilenameEnd = appStrstr(IncludeFilenameBegin + 1, TEXT("\""));
		//construct a string between the double quotations
		FString ExtractedIncludeFilename((INT)(IncludeFilenameEnd - IncludeFilenameBegin - 1), IncludeFilenameBegin + 1);

		//CRC the template, not the filled out version so that this shader's CRC will be independent of which material references it.
		if (ExtractedIncludeFilename == TEXT("Material.usf"))
		{
			ExtractedIncludeFilename = TEXT("MaterialTemplate.usf");
		}

		//vertex factories need to be handled separately
		if (ExtractedIncludeFilename != TEXT("VertexFactory.usf"))
		{
#if !WITH_REALD
			if ((ExtractedIncludeFilename.InStr(TEXT("RealD/"), FALSE, TRUE) == INDEX_NONE) &&
				(ExtractedIncludeFilename.InStr(TEXT("RealD\\"), FALSE, TRUE) == INDEX_NONE))
#endif	//#if !WITH_REALD
			{
				GetShaderIncludes(*ExtractedIncludeFilename, IncludeFilenames, DepthLimit - 1);
				FFilename ExtractedInclude(ExtractedIncludeFilename);
				ExtractedIncludeFilename = ExtractedInclude.GetBaseFilename();
#if WITH_REALD
				if ((ExtractedInclude.InStr(TEXT("RealD/"), FALSE, TRUE) != INDEX_NONE) ||
					(ExtractedInclude.InStr(TEXT("RealD\\"), FALSE, TRUE) != INDEX_NONE))
				{
					ExtractedIncludeFilename = FString(TEXT("RealD")) * ExtractedIncludeFilename;
				}
#endif	//WITH_REALD
				IncludeFilenames.AddUniqueItem(ExtractedIncludeFilename);
			}
		}
		
		//find the next include directive
		IncludeBegin = appStrstr(IncludeFilenameEnd + 1, TEXT("#include "));
		SearchCount++;
	}
}

static void UpdateShaderFileHashState(FSHA1 &HashState, const FString &Contents)
{
#if PLATFORM_MACOSX
	// On Macs we use the same shader cache files as on PC. The problem is that the hash values stored in shader cache
	// are calculated using UTF16 encoding, which is what Windows uses and differ from what would be calculated using
	// Mac-native UTF32 encoding. For this reason, on Macs we convert shader source code to UTF16 and calculate the hash
	// from converted text, so we get the same results as stored in shader cache.
	extern void *appMacUTF32ToUTF16(const unsigned int *Source, unsigned short *Dest, int Length);
	WORD *UTF16Contents = (WORD *)appMalloc((Contents.Len() + 1) * 2);
	appMacUTF32ToUTF16((DWORD *)*Contents, UTF16Contents, Contents.Len());
	HashState.Update((const BYTE*)UTF16Contents, Contents.Len() * sizeof(WORD));
	appFree(UTF16Contents);
#else
	HashState.Update((const BYTE*)*Contents, Contents.Len() * sizeof(TCHAR));
#endif
}

/**
 * Calculates a Hash for the given filename and its includes if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 */
const FSHAHash& GetShaderFileHash(const TCHAR* Filename)
{
	// Make sure we are only accessing GShaderHashCache from one thread
	check(IsInGameThread());
	STAT(DOUBLE HashTime = appSeconds());

	FSHAHash* CachedHash = GShaderHashCache.Find(Filename);

	// If a hash for this filename has been cached, use that
	if (CachedHash)
	{
		return *CachedHash;
	}

	// Get the list of includes this file contains
	TArray<FString> IncludeFilenames;
	GetShaderIncludes(Filename, IncludeFilenames);

	FSHA1 HashState;
	for (INT IncludeIndex = 0; IncludeIndex < IncludeFilenames.Num(); IncludeIndex++)
	{
		// Load the include file and hash it
		const FString IncludeFileContents = LoadShaderSourceFile(*IncludeFilenames(IncludeIndex));
		UpdateShaderFileHashState(HashState, IncludeFileContents);
	}

	// Load the source file and hash it
	const FString FileContents = LoadShaderSourceFile(Filename);
	UpdateShaderFileHashState(HashState, FileContents);
	HashState.Final();

	// Update the hash cache
	FSHAHash& NewHash = GShaderHashCache.Set(*FString(Filename), FSHAHash());
	HashState.GetHash(&NewHash.Hash[0]);

	STAT(HashTime = appSeconds() - HashTime);
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HashingShaderFiles,(FLOAT)HashTime);

	return NewHash;
}

/**
 * Flushes the shader file and CRC cache, and regenerates the binary shader files if necessary.
 * Allows shader source files to be re-read properly even if they've been modified since startup.
 */
void FlushShaderFileCache()
{
	GShaderHashCache.Empty();
	GShaderFileCache.Empty();

	GenerateBinaryShaderFiles();
}

/** Timer class used to report information on the 'recompileshaders' console command. */
class FRecompileShadersTimer
{
public:
	FRecompileShadersTimer(const TCHAR* InInfoStr=TEXT("Test")) :
		InfoStr(InInfoStr),
		bAlreadyStopped(FALSE)
	{
		StartTime = appSeconds();
	}

	FRecompileShadersTimer(FString InInfoStr) :
		InfoStr(InInfoStr),
		bAlreadyStopped(FALSE)
	{
		StartTime = appSeconds();
	}

	void Stop(UBOOL DisplayLog = TRUE)
	{
		if (!bAlreadyStopped)
		{
			bAlreadyStopped = TRUE;
			EndTime = appSeconds();
			TimeElapsed = EndTime-StartTime;
			if (DisplayLog)
			{
				warnf(TEXT("		[%s] took [%.4f] s"),*InfoStr,TimeElapsed);
			}
		}
	}

	~FRecompileShadersTimer()
	{
		Stop(TRUE);
	}

protected:
	DOUBLE StartTime,EndTime;
	DOUBLE TimeElapsed;
	FString InfoStr;
	UBOOL bAlreadyStopped;
};

/** Implementation of the 'recompileshaders' console command.  Recompiles shaders at runtime based on various criteria. */
UBOOL RecompileShaders(const TCHAR* Cmd, FOutputDevice& Ar)
{
#if WITH_ES2_RHI
	// handle recompiling ES2 shaders on PC while using -es2 / -simmobile
	if (GUsingES2RHI)
	{
		FlushRenderingCommands();
		ENQUEUE_UNIQUE_RENDER_COMMAND (
			RecompileCommand,
		{
			extern void RecompileES2Shaders();
			RecompileES2Shaders();
		});
		FlushRenderingCommands();
		return TRUE;
	}
#endif

	FString FlagStr(ParseToken(Cmd, 0));
	if( FlagStr.Len() > 0 )
	{
		// Flush the shader file cache so that any changes to shader source files will be detected
		FlushShaderFileCache();
		FlushRenderingCommands();

		if( appStricmp(*FlagStr,TEXT("Changed"))==0)
		{
			TArray<FShaderType*> OutdatedShaderTypes;
			TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
			{
				FRecompileShadersTimer SearchTimer(TEXT("Searching for changed files"));
				FShaderType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedFactoryTypes);
			}

			if (OutdatedShaderTypes.Num() > 0 || OutdatedFactoryTypes.Num() > 0)
			{
				FRecompileShadersTimer TestTimer(TEXT("RecompileShaders Changed"));
				UMaterial::UpdateMaterialShaders(OutdatedShaderTypes, OutdatedFactoryTypes);
				RecompileGlobalShaders(OutdatedShaderTypes);
			}
			else
			{
				warnf(TEXT("No Shader changes found."));
			}
		}
		else if( appStricmp(*FlagStr,TEXT("Global"))==0)
		{
			FRecompileShadersTimer TestTimer(TEXT("RecompileShaders Global"));
			RecompileGlobalShaders();
		}
		else if( appStricmp(*FlagStr,TEXT("Material"))==0)
		{
			FString RequestedMaterialName(ParseToken(Cmd, 0));
			FRecompileShadersTimer TestTimer(FString::Printf(TEXT("Recompile Material %s"), *RequestedMaterialName));
			UBOOL bMaterialFound = FALSE;
			for( TObjectIterator<UMaterial> It; It; ++It )
			{
				UMaterial* Material = *It;
				if( Material && Material->GetName() == RequestedMaterialName)
				{
					bMaterialFound = TRUE;
					// <Pre/Post>EditChange will force a re-creation of the resource,
					// in turn recompiling the shader.
					Material->PreEditChange(NULL);
					Material->PostEditChange();
					break;
				}
			}

			if (!bMaterialFound)
			{
				TestTimer.Stop(FALSE);
				warnf(TEXT("Couldn't find Material %s!"), *RequestedMaterialName);
			}
		}
		else if( appStricmp(*FlagStr,TEXT("All"))==0)
		{
			FRecompileShadersTimer TestTimer(TEXT("RecompileShaders"));
			RecompileGlobalShaders();
			for( TObjectIterator<UMaterial> It; It; ++It )
			{
				UMaterial* Material = *It;
				if( Material )
				{
					debugf(TEXT("recompiling [%s]"),*Material->GetFullName());

					// <Pre/Post>EditChange will force a re-creation of the resource,
					// in turn recompiling the shader.
					Material->PreEditChange(NULL);
					Material->PostEditChange();
				}
			}
		}

		return 1;
	}

	warnf( TEXT("Invalid parameter. Options are: \n'Changed', 'Global', 'Material [name]', 'All'"));
	return 1;
}

// IMPORTANT: Must match ShaderCompileWorker.cpp
#define BINARY_SHADER_FILE_VERSION		1
#define BINARY_SHADER_FILE_HEADER_SIZE	24

/**
 * Generates binary version of the shader files in Engine\Shaders\Binary\*.bin,
 * if they're out of date.
 */
void GenerateBinaryShaderFiles( )
{
	DOUBLE StartTime = appSeconds();

	const FString SearchString = FString( appShaderDir() ) * TEXT("*.usf");
	TArray<FString> ShaderFiles;
	GFileManager->FindFiles(ShaderFiles, *SearchString, TRUE, FALSE);
	INT RealDShaderFiles_StartIndex = ShaderFiles.Num();
#if WITH_REALD
	const FString SearchString_RealD = FString( appShaderDir() ) * TEXT("RealD") * TEXT("*.usf");
	TArray<FString> ShaderFiles_RealD;
	GFileManager->FindFiles(ShaderFiles_RealD, *SearchString_RealD, TRUE, FALSE);
	ShaderFiles += ShaderFiles_RealD;
#endif	//WITH_REALD
	for (INT ShaderIndex = 0; ShaderIndex < ShaderFiles.Num(); ShaderIndex++)
	{
		FString SourcePath = FString( appShaderDir() ) * ShaderFiles(ShaderIndex);
		FString DestFolder = FString( appShaderDir() ) * TEXT("Binaries");
		if (ShaderIndex >= RealDShaderFiles_StartIndex)
		{
			SourcePath = FString( appShaderDir() ) * TEXT("RealD") * ShaderFiles(ShaderIndex);
			DestFolder = FString( appShaderDir() ) * TEXT("Binaries") * TEXT("RealD");
		}
		FFilename DestPath = DestFolder * FFilename(ShaderFiles(ShaderIndex)).GetBaseFilename(TRUE);
		if (DestPath.GetExtension() != TEXT("bin"))
		{
			DestPath += TEXT(".bin");
		}
		TArray<BYTE> FileContent;
		UBOOL bLoadedFile = appLoadFileToArray(FileContent, *SourcePath, GFileManager, FILEREAD_Silent);
		UBOOL bSavedFile = FALSE;
		if ( bLoadedFile )
		{
			// Try to read the header of the destination file.
			BYTE SourceHash[20];
			FSHA1::HashBuffer(FileContent.GetData(), FileContent.Num(), SourceHash );
			FArchive* DestAr = GFileManager->CreateFileReader( *DestPath, FILEREAD_Silent );
			if ( DestAr )
			{
				// Compare file version and hash
				BYTE Header[BINARY_SHADER_FILE_HEADER_SIZE] = { 0 };
				DestAr->Serialize( Header, sizeof(Header) );
				if ( *((DWORD*)Header) == BINARY_SHADER_FILE_VERSION )
				{
					if ( appMemcmp(SourceHash, Header + sizeof(DWORD), 20) == 0 )
					{
						// The destination file is up to date.
						bSavedFile = TRUE;
					}
				}
			}
			delete DestAr;
			if ( !bSavedFile )
			{
				// Generate a new .bin file
				DestAr = GFileManager->CreateFileWriter( *DestPath );
				if ( DestAr )
				{
					DWORD FileVersion = BINARY_SHADER_FILE_VERSION;
					DestAr->Serialize( &FileVersion, sizeof(FileVersion) );
					DestAr->Serialize( SourceHash, sizeof(SourceHash) );
					SecurityByObscurityEncryptAndDecrypt(FileContent);
					DestAr->Serialize(FileContent.GetData(), FileContent.Num());
					bSavedFile = DestAr->Close();
					delete DestAr;
				}
			}
		}
		if ( !bSavedFile )
		{
			warnf( TEXT("Failed to encrypt %s!"), *ShaderFiles(ShaderIndex) );
		}
	}

	DOUBLE Duration = appSeconds() - StartTime;
}

/**
 * Loads the shader file with the given name.
 * @return The contents of the shader file.
 */
FString LoadShaderSourceFile(const TCHAR* Filename)
{
	// Protect GShaderFileCache from simultaneous access by multiple threads
	FScopeLock ScopeLock(&FileCacheCriticalSection);

	FString	FileContents;
	STAT(DOUBLE ShaderFileLoadingTime = 0);
	{
		SCOPE_SECONDS_COUNTER(ShaderFileLoadingTime);

		// Load the specified file from the System/Shaders directory.
		FFilename ShaderFilename = FString(appBaseDir()) * appShaderDir() * FFilename(Filename).GetCleanFilename();
		UBOOL bRealDFile = FALSE;
#if WITH_REALD
		if (
			(appStristr(Filename, TEXT("RealD/")) != 0) ||
			(appStristr(Filename, TEXT("RealD\\")) != 0))
		{
			ShaderFilename = FString(appBaseDir()) * appShaderDir() * FFilename(Filename);
			bRealDFile = TRUE;
		}
#endif

		if (ShaderFilename.GetExtension() != TEXT("usf"))
		{
			ShaderFilename += TEXT(".usf");
		}

		FString* CachedFile = GShaderFileCache.Find(ShaderFilename);

		//if this file has already been loaded and cached, use that
		if (CachedFile)
		{
			FileContents = *CachedFile;
		}
		else
		{
			// Load the binary version of the shader file.
			FFilename BinaryShaderFilename = FString(appBaseDir()) * appShaderDir() * TEXT("Binaries") * FFilename(Filename).GetBaseFilename(TRUE);
#if WITH_REALD
			if (bRealDFile == TRUE)
			{
				BinaryShaderFilename = FString(appBaseDir()) * appShaderDir() * TEXT("Binaries") * TEXT("RealD") * FFilename(Filename).GetBaseFilename(TRUE);
			}
#endif	//WITH_REALD
			if (BinaryShaderFilename.GetExtension() != TEXT("bin"))
			{
				BinaryShaderFilename += TEXT(".bin");
			}

			// verify SHA hash of shader files on load. missing entries trigger an error
			UBOOL bLoadedSuccessfully = FALSE;
			TArray<BYTE> BinaryContent;
			if ( appLoadFileToArray(BinaryContent, *BinaryShaderFilename) )
			{
				// Decrypt and convert to FString.
				SecurityByObscurityEncryptAndDecrypt(BinaryContent, BINARY_SHADER_FILE_HEADER_SIZE);
				if ( *((DWORD*)BinaryContent.GetData()) == BINARY_SHADER_FILE_VERSION )
				{
					appBufferToString( FileContents, BinaryContent.GetTypedData() + BINARY_SHADER_FILE_HEADER_SIZE, BinaryContent.Num() - BINARY_SHADER_FILE_HEADER_SIZE );
					bLoadedSuccessfully = TRUE;
				}
			}

			if ( !bLoadedSuccessfully )
			{
				appErrorf(TEXT("Couldn't load shader file \'%s\'"),Filename);
			}

			//update the shader file cache
			GShaderFileCache.Set(ShaderFilename, *FileContents);
		}
	}

	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_LoadingShaderFiles,(FLOAT)ShaderFileLoadingTime);
	return FileContents;
}

/**
 * Dumps shader stats to the log.
 * 
 * @param	Platform	Platform to dump shader info for, use SP_NumPlatforms for all
 * @para	Frequency	Whether to dump PS or VS info, use SF_NumFrequencies to dump both
 */
void DumpShaderStats( EShaderPlatform Platform, EShaderFrequency Frequency )
{
#if ALLOW_DEBUG_FILES
	FDiagnosticTableViewer ShaderTypeViewer(*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(TEXT("ShaderStats")));

	// Iterate over all shader types and log stats.
	INT TotalShaderCount		= 0;
	INT TotalTypeCount			= 0;
	INT TotalInstructionCount	= 0;
	INT TotalSize				= 0;
	FLOAT TotalSizePerType		= 0;

	// Write a row of headings for the table's columns.
	ShaderTypeViewer.AddColumn(TEXT("Type"));
	ShaderTypeViewer.AddColumn(TEXT("Instances"));
	ShaderTypeViewer.AddColumn(TEXT("Average instructions"));
	ShaderTypeViewer.AddColumn(TEXT("Size"));
	ShaderTypeViewer.AddColumn(TEXT("AvgSizePerInstance"));
	ShaderTypeViewer.CycleRow();

	for( TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next() )
	{
		const FShaderType* Type = *It;
		if(Type->GetNumShaders())
		{
			// Calculate the average instruction count and total size of instances of this shader type.
			FLOAT AverageNumInstructions	= 0.0f;
			INT NumInitializedInstructions	= 0;
			INT Size						= 0;
			INT NumShaders					= 0;
			for(TMap<FGuid,FShader*>::TConstIterator ShaderIt(Type->ShaderIdMap);ShaderIt;++ShaderIt)
			{
				const FShader* Shader = ShaderIt.Value();
				// Skip shaders that don't match frequency.
				if( Shader->GetTarget().Frequency != Frequency && Frequency != SF_NumFrequencies )
				{
					continue;
				}
				// Skip shaders that don't match platform.
				if( Shader->GetTarget().Platform != Platform && Platform != SP_NumPlatforms )
				{
					continue;
				}
				NumInitializedInstructions += Shader->GetNumInstructions();
				Size += Shader->GetCode().Num();
				NumShaders++;
			}
			AverageNumInstructions = (FLOAT)NumInitializedInstructions / (FLOAT)Type->GetNumShaders();
			
			// Only add rows if there is a matching shader.
			if( NumShaders )
			{
				// Write a row for the shader type.
				ShaderTypeViewer.AddColumn(Type->GetName());
				ShaderTypeViewer.AddColumn(TEXT("%u"),NumShaders);
				ShaderTypeViewer.AddColumn(TEXT("%.1f"),AverageNumInstructions);
				ShaderTypeViewer.AddColumn(TEXT("%u"),Size);
				ShaderTypeViewer.AddColumn(TEXT("%.1f"),Size / (FLOAT)NumShaders);
				ShaderTypeViewer.CycleRow();

				TotalShaderCount += NumShaders;
				TotalInstructionCount += NumInitializedInstructions;
				TotalTypeCount++;
				TotalSize += Size;
				TotalSizePerType += Size / (FLOAT)NumShaders;
			}
		}
	}

	// Write a total row.
	ShaderTypeViewer.AddColumn(TEXT("Total"));
	ShaderTypeViewer.AddColumn(TEXT("%u"),TotalShaderCount);
	ShaderTypeViewer.AddColumn(TEXT("%u"),TotalInstructionCount);
	ShaderTypeViewer.AddColumn(TEXT("%u"),TotalSize);
	ShaderTypeViewer.AddColumn(TEXT("0"));
	ShaderTypeViewer.CycleRow();

	// Write an average row.
	ShaderTypeViewer.AddColumn(TEXT("Average"));
	ShaderTypeViewer.AddColumn(TEXT("%.1f"),TotalShaderCount / (FLOAT)TotalTypeCount);
	ShaderTypeViewer.AddColumn(TEXT("%.1f"),(FLOAT)TotalInstructionCount / TotalShaderCount);
	ShaderTypeViewer.AddColumn(TEXT("%.1f"),TotalSize / (FLOAT)TotalShaderCount);
	ShaderTypeViewer.AddColumn(TEXT("%.1f"),TotalSizePerType / TotalTypeCount);
	ShaderTypeViewer.CycleRow();
#endif
}

//
FShaderType* FindShaderTypeByName(const TCHAR* ShaderTypeName)
{
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		if(!appStricmp(ShaderTypeIt->GetName(),ShaderTypeName))
		{
			return *ShaderTypeIt;
		}
	}
	return NULL;
}

static TArray<BYTE> PaddedShaderParameterValueBuffer;
// we will start in the middle and copy down to ensure zero padded at the end
// it only needs to be 80 bytes, but I made it 128 bytes instead
DWORD GSmallPaddedShaderParameterValueBuffer[32] = {0};
const void* GetPaddedShaderParameterValueGeneral(const void* Value,UINT NumBytes)
{
	// Compute the number of bytes of padding to add, assuming the shader array element alignment is a power of two.
	const UINT NumPaddedBytes = Align(NumBytes,ShaderArrayElementAlignBytes);
	checkSlow(NumPaddedBytes != NumBytes); // inline version should have dealt with this
	// If padding is needed, use a lazily sized global buffer to hold the padded shader parameter value.
	if((UINT)PaddedShaderParameterValueBuffer.Num() < NumPaddedBytes)
	{
		PaddedShaderParameterValueBuffer.Empty(NumPaddedBytes);
		PaddedShaderParameterValueBuffer.Add(NumPaddedBytes);
	}

	// Copy the shader parameter value into the padded shader buffer.
	appMemcpy(&PaddedShaderParameterValueBuffer(0),Value,NumBytes);

	// Write zeros to the rest of the padded buffer.
	appMemzero(&PaddedShaderParameterValueBuffer(NumBytes),NumPaddedBytes - NumBytes);

	return &PaddedShaderParameterValueBuffer(0);
}





