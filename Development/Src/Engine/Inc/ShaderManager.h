/*=============================================================================
	ShaderManager.h: Shader manager definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SHADERMANAGER_H__
#define __SHADERMANAGER_H__

// Forward declarations.
class FShaderType;

/** The minimum package version which stores valid compiled shaders.  This can be used to force a recompile of all shaders. */
#define VER_MIN_SHADER	VER_INVALIDATE_SHADERCACHE5

/** Same as VER_MIN_SHADER, but for the licensee package version. */
#define LICENSEE_VER_MIN_SHADER	0

/** 
	On Playstation 3 we have two different options how to compress shaders.
	First version works on Xbox 360 and Playstation 3. In this version shaders 
	are compressed into larger chunks (depends on platform) during cooking and 
	decompressed as needed. Second version works only on Playstation 3 and works
	in a very different way. Shaders are compressed during shader compilation and 
	stored compressed one by one in the shader caches. Then during patching 
	shaders are decompressed on SPU. Decompressing during patching takes about 
	twice more time than usual patching, but this is done in the background using 
	SPU and it doesn't affect the performance in general. On Playstation 3, 
	default is to use the second method. 
	
	If You want to use the first method You need to make the following changes.
	
	In file PS3Tools.cpp in the method FPS3ShaderPrecompiler::PrecompileShader 
	change bCompressPixelShaders to FALSE. 
	Change PS3_SHADER_COMPRESSION_METHOD into PS3_SHADER_COMPRESSION_CHUNKS
	You should also add an another VER_INVALIDATE_SHADERCACHE* version to force 
	a recompile of all shaders and increase by one VER_LATEST_COOKED_PACKAGE.
*/

/** Shaders are compressed into larger chunks during cooking. */
#define PS3_SHADER_COMPRESSION_CHUNKS		1

/** Shaders are compressed one by one into the shader caches. */
#define PS3_SHADER_COMPRESSION_INDIVIDUAL	0

/** Whether to use first or second methond of compressing pixel shaders on Playstation 3. */
#define PS3_SHADER_COMPRESSION_METHOD	PS3_SHADER_COMPRESSION_INDIVIDUAL

/** 
 * Whether to initialize shaders on demand as needed or initialize all of them at load time. 
 * On PC, initializing shaders on demand causes hitches when streaming in new levels as the driver does slow shader optimizations during initialization.
 * On consoles, initializing shaders is fast and only initializing as needed saves memory.
 * This must be enabled for platforms that UseShaderCompression returns TRUE for.
 */
#define INIT_SHADERS_ON_DEMAND	(XBOX || PS3_SHADER_COMPRESSION_METHOD)

/** Can only return TRUE on platforms that don't ever save shader caches. */
inline UBOOL UseShaderCompression(EShaderPlatform Platform)
{
	return (Platform == SP_XBOXD3D)
#if PS3_SHADER_COMPRESSION_METHOD == PS3_SHADER_COMPRESSION_CHUNKS
		|| (Platform == SP_PS3)
#endif // PS3_SHADER_COMPRESSION_METHOD		
		;
}

/** Returns compression flags to be used when compressing shaders for the given platform. */
extern ECompressionFlags GetShaderCompressionFlags(EShaderPlatform Platform);

/** 
 * Returns the target shader chunk size for the given platform.
 * Larger values result in a better compression ratio, but make decompressing individual shaders at runtime cause more of a hitch. 
 */
extern UINT GetCompressedShaderChunkSizeTarget(EShaderPlatform Platform);

/** A shader parameter's register binding. */
class FShaderParameter
{
public:
	FShaderParameter()
	:	NumBytes(0)
#if WITH_MOBILE_RHI
	,	ShaderParamSlotIndex(-1)
#endif
#if _DEBUG
	,	bInitialized(FALSE)
#endif
	{}
	void Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,UBOOL bIsOptional = FALSE);
#if WITH_MOBILE_RHI
	void Unbind()
	{
		NumBytes = 0;
	}
#endif
	friend FArchive& operator<<(FArchive& Ar,FShaderParameter& P);
	UBOOL IsBound() const { return NumBytes > 0; }

	inline UBOOL IsInitialized() const 
	{ 
#if _DEBUG
		return bInitialized; 
#else 
		return TRUE;
#endif
	}

#if PLATFORM_SUPPORTS_D3D10_PLUS
	UINT GetBufferIndex() const { return BufferIndex; }
#else
	UINT GetBufferIndex() const { return 0; }
#endif
	UINT GetBaseIndex() const { return BaseIndex; }
	UINT GetNumBytes() const { return NumBytes; }
	void SetNumBytes(UINT InNumBytes) { NumBytes = InNumBytes; }

	/**
	 * For ES2: Sets the shader parameter name to the given value
	 *
	 * @param String name for the parameter that ES2 uses to set the parameter value by
	 */
	inline void SetShaderParamName(const TCHAR* Name)
	{
#if WITH_MOBILE_RHI
		ShaderParamName = Name;
		ShaderParamSlotIndex = RHIGetMobileUniformSlotIndexByName(Name, NumBytes);
#if _DEBUG
		bInitialized = TRUE;
#endif
#endif
	}

	/**
	 * For ES2: Gets the shader parameter name
	 *
	 * @return String name for the parameter that ES2 uses to set the parameter value by
	 */
	inline INT GetShaderParamName() const
	{
#if WITH_MOBILE_RHI
		return ShaderParamSlotIndex;
#else
		return -1;
#endif
	}

private:
#if PLATFORM_SUPPORTS_D3D10_PLUS
	WORD BufferIndex;
#endif
	WORD BaseIndex;
	// 0 if the parameter wasn't bound
	WORD NumBytes;

#if WITH_MOBILE_RHI
	/** Cached name and slot index of the parameter for mobile parameter setting */
	FName ShaderParamName;
	INT   ShaderParamSlotIndex;
#endif

#if _DEBUG
	UBOOL bInitialized;
#endif
};

/** A shader resource binding (just textures for now). */
class FShaderResourceParameter
{
public:
	FShaderResourceParameter(): NumResources(0) {}
	void Bind(const FShaderParameterMap& ParameterMap,const TCHAR* ParameterName,UBOOL bIsOptional = FALSE);
	friend FArchive& operator<<(FArchive& Ar,FShaderResourceParameter& P);
	UBOOL IsBound() const { return NumResources > 0; }
	UINT GetBaseIndex() const { return BaseIndex; }
	UINT GetNumResources() const { return NumResources; }
#if PLATFORM_SUPPORTS_D3D10_PLUS
	UINT GetSamplerIndex() const { return SamplerIndex; }
#else
	UINT GetSamplerIndex() const { return 0; }
#endif
#if WITH_MOBILE_RHI
	void SetBaseIndex( WORD InBaseIndex, UBOOL bForceBind=FALSE )
	{
		BaseIndex = InBaseIndex;
		if ( bForceBind )
		{
			NumResources = 1;
		}
	}
	void Unbind()
	{
		NumResources = 0;
	}
#endif
private:
	WORD BaseIndex;
	WORD NumResources;
#if PLATFORM_SUPPORTS_D3D10_PLUS
	WORD SamplerIndex;
#endif
};

/**
 * Pads a shader parameter value to the an integer number of shader registers. - General version
 * @param Value - A pointer to the shader parameter value.
 * @param NumBytes - The number of bytes in the shader parameter value.
 * @return A pointer to the padded shader parameter value.
 */
const void* GetPaddedShaderParameterValueGeneral(const void* Value,UINT NumBytes);

/**
 * Pads a shader parameter value to the an integer number of shader registers. - Inline version
 * @param Value - A pointer to the shader parameter value.
 * @param NumBytes - The number of bytes in the shader parameter value.
 * @return A pointer to the padded shader parameter value.
 */
FORCEINLINE const void* GetPaddedShaderParameterValue(const void* Value,UINT NumBytes)
{
	checkAtCompileTime(ShaderArrayElementAlignBytes==16,GetPaddedShaderParameterValue_handcoded_for_16);
	checkAtCompileTime(sizeof(DWORD)==4,GetPaddedShaderParameterValue_handcoded_for_DWORD4);
	checkSlow((NumBytes & 3)==0);
	// Compute the number of bytes of padding to add, assuming the shader array element alignment is a power of two.
	if(NumBytes & (ShaderArrayElementAlignBytes - 1))
	{
		if (NumBytes > 64)
		{
			return GetPaddedShaderParameterValueGeneral(Value,NumBytes);
		}
		extern DWORD GSmallPaddedShaderParameterValueBuffer[32];
		DWORD* RESTRICT Dest = &GSmallPaddedShaderParameterValueBuffer[15];
		const DWORD* RESTRICT Src = (const DWORD* RESTRICT)Value;
		Src += NumBytes/4 - 1;
		for (;NumBytes;NumBytes -= 4)
		{
			*Dest-- = *Src--;
		}
		return Dest + 1;
	}
	else
	{
		// If padding isn't needed, simply return a pointer to the input value.
		return Value;
	}
}

/**
 * Sets the value of a vertex shader parameter.
 * A template parameter specified the type of the parameter value.
 */
template<class ParameterType>
void SetVertexShaderValue(
	FVertexShaderRHIParamRef VertexShader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	UINT ElementIndex = 0
	)
{
	const UINT AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const INT NumBytesToSet = Min<INT>(sizeof(ParameterType),Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHISetVertexShaderParameter(
			VertexShader,
			Parameter.GetBufferIndex(),
#if WIIU
			// @todo wiiu: If the WiiUTools returned parameter indices in _BYTES_ of all things, this would work 
			// without needing a special case. Fix that all remove sll WIIU things like this in this file
			Parameter.GetBaseIndex() + ElementIndex,
#else
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
#endif
			(UINT)NumBytesToSet,
			&Value,
			Parameter.GetShaderParamName()
			);
	}
}

#if WITH_D3D11_TESSELLATION
/**
 * Sets the value of a domain shader parameter.
 * A template parameter specified the type of the parameter value.
 */
template<class ParameterType>
void SetDomainShaderValue(
	FDomainShaderRHIParamRef DomainShader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	UINT ElementIndex = 0
	)
{
	const UINT AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const INT NumBytesToSet = Min<INT>(sizeof(ParameterType),Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHISetDomainShaderParameter(
			DomainShader,
			Parameter.GetBufferIndex(),
#if WIIU
			Parameter.GetBaseIndex() + ElementIndex,
#else
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
#endif
			(UINT)NumBytesToSet,
			&Value,
			Parameter.GetShaderParamName()
			);
	}
}

/**
 * Sets the value of a hull shader parameter.
 * A template parameter specified the type of the parameter value.
 */
template<class ParameterType>
void SetHullShaderValue(
	FHullShaderRHIParamRef HullShader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	UINT ElementIndex = 0
	)
{
	const UINT AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const INT NumBytesToSet = Min<INT>(sizeof(ParameterType),Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHISetHullShaderParameter(
			HullShader,
			Parameter.GetBufferIndex(),
#if WIIU
			Parameter.GetBaseIndex() + ElementIndex,
#else
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
#endif
			(UINT)NumBytesToSet,
			&Value,
			Parameter.GetShaderParamName()
			);
	}
}

#endif

/**
 * Sets the value of a shader parameter.  
 * A template parameter specifies the type of the parameter value.
 * NOTE: Shader should be the param ref type, NOT the param type, since Shader is passed by value. 
 * Otherwise AddRef/ReleaseRef will be called many times.
 */
template<typename ShaderRHIParamRef, class ParameterType>
void SetShaderValue(
	ShaderRHIParamRef Shader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	UINT ElementIndex = 0
	)
{
	const UINT AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const INT NumBytesToSet = Min<INT>(sizeof(ParameterType),Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHISetShaderParameter(
			Shader,
			Parameter.GetBufferIndex(),
#if WIIU
			Parameter.GetBaseIndex() + ElementIndex,
#else
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
#endif
			(UINT)NumBytesToSet,
			&Value,
			Parameter.GetShaderParamName()
			);
	}
}

/**
 * Sets the value of a shader parameter array.  
 * A template parameter specifies the type of the parameter value.
 * NOTE: Shader should be the param ref type, NOT the param type, since Shader is passed by value. 
 * Otherwise AddRef/ReleaseRef will be called many times.
 */
template<typename ShaderRHIParamRef,class ParameterType>
void SetShaderValues(
	ShaderRHIParamRef Shader,
	const FShaderParameter& Parameter,
	const ParameterType* Values,
	UINT NumElements,
	UINT BaseElementIndex = 0
	)
{
	const UINT AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const INT NumBytesToSet = Min<INT>(NumElements * AlignedTypeSize,Parameter.GetNumBytes() - BaseElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHISetShaderParameter(
			Shader,
			Parameter.GetBufferIndex(),
#if WIIU
			Parameter.GetBaseIndex() + BaseElementIndex,
#else
			Parameter.GetBaseIndex() + BaseElementIndex * AlignedTypeSize,
#endif
			(UINT)NumBytesToSet,
			Values,
			Parameter.GetShaderParamName()
			);
	}
}

/**
 * Sets the value of a vertex shader bool parameter.
 */
extern void SetVertexShaderBool(
   FVertexShaderRHIParamRef VertexShader,
   const FShaderParameter& Parameter,
   UBOOL Value
   );

/**
 * Sets the value of a pixel shader bool parameter.
 */
extern void SetPixelShaderBool(
	FPixelShaderRHIParamRef PixelShader,
	const FShaderParameter& Parameter,
	UBOOL Value
	);

#if PLATFORM_SUPPORTS_D3D10_PLUS
/**
 * Sets the value of a shader bool parameter.  Template'd on shader type
 */
template<typename ShaderTypeRHIParamRef>
void SetShaderBool(
	ShaderTypeRHIParamRef Shader,
	const FShaderParameter& Parameter,
	UBOOL Value
	)
{
	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if (Parameter.GetNumBytes() > 0)
	{
		RHISetShaderBoolParameter(
			Shader,
			Parameter.GetBufferIndex(),
			Parameter.GetBaseIndex(),
			Value
			);
	}
}
#endif

/**
 * Sets the value of a pixel shader parameter.
 * A template parameter specified the type of the parameter value.
 */
template<class ParameterType>
void SetPixelShaderValue(
	FPixelShaderRHIParamRef PixelShader,
	const FShaderParameter& Parameter,
	const ParameterType& Value,
	UINT ElementIndex = 0
	)
{
	const UINT AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const INT NumBytesToSet = Min<INT>(sizeof(ParameterType),Parameter.GetNumBytes() - ElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHISetPixelShaderParameter(
			PixelShader,
			Parameter.GetBufferIndex(),
#if WIIU
			Parameter.GetBaseIndex() + ElementIndex,
#else
			Parameter.GetBaseIndex() + ElementIndex * AlignedTypeSize,
#endif
			(UINT)NumBytesToSet,
			&Value,
			Parameter.GetShaderParamName()
			);
	}
}

/**
 * Sets the value of a vertex shader parameter array.
 * A template parameter specified the type of the parameter value.
 */
template<class ParameterType>
void SetVertexShaderValues(
	FVertexShaderRHIParamRef VertexShader,
	const FShaderParameter& Parameter,
	const ParameterType* Values,
	UINT NumElements,
	UINT BaseElementIndex = 0
	)
{
	const UINT AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const INT NumBytesToSet = Min<INT>(NumElements * AlignedTypeSize,Parameter.GetNumBytes() - BaseElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHISetVertexShaderParameter(
			VertexShader,
			Parameter.GetBufferIndex(),
#if WIIU
			Parameter.GetBaseIndex() + BaseElementIndex,
#else
			Parameter.GetBaseIndex() + BaseElementIndex * AlignedTypeSize,
#endif
			(UINT)NumBytesToSet,
			Values,
			Parameter.GetShaderParamName()
			);
	}
}


/*
 * *** PERFORMANCE WARNING *****
 * This function is to support single float array parameter in shader
 * This pads 3 more floats for each element - at the expensive of CPU/stack memory
 * Do not overuse this. If you can, use float4 in shader. 
 * *** PERFORMANCE WARNING *****
 * Sets the value of a vertex shader parameter float array.
 * A template parameter specified the type of the parameter value.
 */
template<class ParameterType>
void SetVertexShaderFloats(
	FVertexShaderRHIParamRef VertexShader,
	const FShaderParameter& Parameter,
	const ParameterType* Values,
	UINT NumElements,
	UINT BaseElementIndex = 0
	)
{
	if(NumElements > 0)
	{
		RHISetVertexShaderFloatArray(
			VertexShader,
			Parameter.GetBufferIndex(),
#if WIIU
			Parameter.GetBaseIndex() + BaseElementIndex,
#else
			Parameter.GetBaseIndex() + BaseElementIndex * ShaderArrayElementAlignBytes,
#endif
			(UINT)NumElements,
			Values,
			Parameter.GetShaderParamName()
			);
	}
}
/**
 * Sets the value of a pixel shader parameter array.
 * A template parameter specified the type of the parameter value.
 */
template<class ParameterType>
void SetPixelShaderValues(
	FPixelShaderRHIParamRef PixelShader,
	const FShaderParameter& Parameter,
	const ParameterType* Values,
	UINT NumElements,
	UINT BaseElementIndex = 0
	)
{
	const UINT AlignedTypeSize = Align(sizeof(ParameterType),ShaderArrayElementAlignBytes);
	const INT NumBytesToSet = Min<INT>(NumElements * AlignedTypeSize,Parameter.GetNumBytes() - BaseElementIndex * AlignedTypeSize);

	// This will trigger if the parameter was not serialized
	checkSlow(Parameter.IsInitialized());

	if(NumBytesToSet > 0)
	{
		RHISetPixelShaderParameter(
			PixelShader,
			Parameter.GetBufferIndex(),
#if WIIU
			Parameter.GetBaseIndex() + BaseElementIndex,
#else
			Parameter.GetBaseIndex() + BaseElementIndex * AlignedTypeSize,
#endif
			(UINT)NumBytesToSet,
			Values,
			Parameter.GetShaderParamName()
			);
	}
}

/**
 * Sets the value of a shader texture parameter.  Template'd on shader type
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
template<typename ShaderTypeRHIParamRef>
FORCEINLINE void SetTextureParameter(
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& Parameter,
	const FTexture* Texture,
	UINT ElementIndex = 0,
	FLOAT MipBias = 0.0f,
	FLOAT LargestMip = -1.0f,
	FLOAT SmallestMip = -1.0f,
	UBOOL bForceLinearMinFilter = FALSE
	)
{
	if(Parameter.IsBound())
	{
		check(ElementIndex < Parameter.GetNumResources());
		Texture->LastRenderTime = GCurrentTime;
		RHISetSamplerState(
			Shader,
			Parameter.GetBaseIndex() + ElementIndex,
			Parameter.GetSamplerIndex() + ElementIndex,
			Texture->SamplerStateRHI,
			Texture->TextureRHI,
			MipBias,
			LargestMip,
			SmallestMip,
			bForceLinearMinFilter
			);
	}
}

/**
 * Sets the value of a shader texture parameter. Template'd on shader type.
 */
template<typename ShaderTypeRHIParamRef>
FORCEINLINE void SetTextureParameter(
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& Parameter,
	FSamplerStateRHIParamRef SamplerStateRHI,
	FTextureRHIParamRef TextureRHI,
	UINT ElementIndex = 0
	)
{
	if(Parameter.IsBound())
	{
		check(ElementIndex < Parameter.GetNumResources());
		RHISetSamplerState(
			Shader,
			Parameter.GetBaseIndex() + ElementIndex,
			Parameter.GetSamplerIndex() + ElementIndex,
			SamplerStateRHI,
			TextureRHI,
			0.0f,
			-1.0f,
			-1.0f,
			FALSE
			);
	}
}

/**
 * Sets the value of a shader texture parameter.  Template'd on shader type
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D10)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
template<typename ShaderTypeRHIParamRef>
FORCEINLINE void SetTextureParameterDirectly(
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& Parameter,
	const FTexture* Texture,
	UINT ElementIndex = 0,
	FLOAT MipBias = 0.0f,
	FLOAT LargestMip = -1.0f,
	FLOAT SmallestMip = -1.0f,
	UBOOL bForceLinearMinFilter = FALSE
	)
{
	if(Parameter.IsBound())
	{
		check(ElementIndex < Parameter.GetNumResources());
		Texture->LastRenderTime = GCurrentTime;
#if WITH_MOBILE_RHI
		if ( GUsingMobileRHI )
		{
			RHISetMobileTextureSamplerState(
				Shader,
				Parameter.GetBaseIndex() + ElementIndex,
				Texture->SamplerStateRHI,
				Texture->TextureRHI,
				MipBias,
				LargestMip,
				SmallestMip
				);
		}
		else
#endif
		{
			RHISetSamplerState(
				Shader,
				Parameter.GetBaseIndex() + ElementIndex,
				Parameter.GetSamplerIndex() + ElementIndex,
				Texture->SamplerStateRHI,
				Texture->TextureRHI,
				MipBias,
				LargestMip,
				SmallestMip,
				bForceLinearMinFilter
			);
		}
	}
}

/**
 * Sets the value of a shader texture parameter. Template'd on shader type.
 */
// @todo ngp clean: Remove the Directly version
template<typename ShaderTypeRHIParamRef>
void SetTextureParameterDirectly(
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& Parameter,
	FSamplerStateRHIParamRef SamplerStateRHI,
	FTextureRHIParamRef TextureRHI,
	UINT ElementIndex = 0
	)
{
	if(Parameter.IsBound())
	{
		check(ElementIndex < Parameter.GetNumResources());
#if WITH_MOBILE_RHI
		if ( GUsingMobileRHI )
		{
			RHISetMobileTextureSamplerState(
				Shader,
				Parameter.GetBaseIndex() + ElementIndex,
				SamplerStateRHI,
				TextureRHI,
				0.0f,
				-1.0f,
				-1.0f );
		}
		else
#endif
		{
			RHISetSamplerState(
				Shader,
				Parameter.GetBaseIndex() + ElementIndex,
				Parameter.GetSamplerIndex() + ElementIndex,
				SamplerStateRHI,
				TextureRHI,
				0.0f,
				-1.0f,
				-1.0f,
				FALSE
				);
		}
	}
}

/**
 * Sets the value of a shader surface parameter (e.g. to access MSAA samples).
 * Template'd on shader type (e.g. pixel shader or compute shader).
 */
template<typename ShaderTypeRHIParamRef>
void SetSurfaceParameter(
	ShaderTypeRHIParamRef Shader,
	const FShaderResourceParameter& Parameter,
	FSurfaceRHIParamRef NewTextureRHI
	)
{
	if(Parameter.IsBound())
	{
		RHISetSurfaceParameter(
			Shader,
			Parameter.GetBaseIndex(),
			NewTextureRHI
			);
	}
}

/**
* Sets the value of a vertex shader texture parameter.
*/
FORCEINLINE void SetVertexShaderTextureParameter(
	FVertexShaderRHIParamRef VertexShader,
	const FShaderResourceParameter& Parameter,
	FTextureRHIParamRef TextureRHI,
	UINT ElementIndex = 0
	)
{
	if(Parameter.IsBound())
	{
#if CONSOLE
		RHISetVertexTexture(Parameter.GetBaseIndex(), TextureRHI);
#else // CONSOLE
		check(ElementIndex < Parameter.GetNumResources());
		// Note: If using DX9, check the RHI for the actual sampling used.  FD3D9DynamicRHI::SetVertexTexture
		RHISetSamplerState(
			VertexShader,
			Parameter.GetBaseIndex() + ElementIndex,
			Parameter.GetSamplerIndex() + ElementIndex,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp,MIPBIAS_None>::GetRHI(),
			TextureRHI,
			0.0f,
			-1.0f,
			-1.0f,
			FALSE
			);
#endif // CONSOLE
	}
}

class FShaderKey
{
public:

	FShaderKey() : 
		ParameterMapCRC(0)
	{}

	FShaderKey(const TArray<BYTE>& InCode, const FShaderParameterMap& InParameterMap) : 
		Code(InCode)
	{
		ParameterMapCRC = InParameterMap.GetCRC();
	}

	TArray<BYTE> Code;
	DWORD ParameterMapCRC;
};

/**
 * A compiled shader and its parameter bindings.
 */
class FShader : public FRenderResource, public FDeferredCleanupInterface
{
	friend class FShaderType;
public:

	struct CompiledShaderInitializerType
	{
		FShaderType* Type;
		FShaderTarget Target;
		const TArray<BYTE>& Code;
		const FShaderParameterMap& ParameterMap;
		UINT NumInstructions;
		CompiledShaderInitializerType(
			FShaderType* InType,
			const FShaderCompilerOutput& CompilerOutput
			):
			Type(InType),
			Target(CompilerOutput.Target),
			Code(CompilerOutput.Code),
			ParameterMap(CompilerOutput.ParameterMap),
			NumInstructions(CompilerOutput.NumInstructions)
		{}
	};

	FShader();
	FShader(const CompiledShaderInitializerType& Initializer);
	virtual ~FShader();

	/**
	 * Can be overridden by FShader subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment) {}

	// Serializer.
	virtual UBOOL Serialize(FArchive& Ar);

	// FRenderResource interface.
	virtual void InitRHI();
	virtual void ReleaseRHI();

	// Reference counting.
	void AddRef();
	void Release();
	virtual void FinishCleanup();

	// Resource handling from the game thread
	void BeginInit();
	void BeginRelease();

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const { return TRUE; }

	void InitializeVertexShader();

	void InitializePixelShader();

	/**
	* @return the shader's vertex shader
	*/
	FORCEINLINE const FVertexShaderRHIParamRef GetVertexShader()
	{
		if (!IsInitialized())
		{
			InitializeVertexShader();
		}
		return VertexShader;
	}
	/**
	* @return the shader's pixel shader
	*/
	FORCEINLINE const FPixelShaderRHIParamRef GetPixelShader()
	{
		if (!IsInitialized())
		{
			InitializePixelShader();
		}
		return PixelShader;
	}

#if WITH_D3D11_TESSELLATION
	/**
	* @return the shader's hull shader
	*/
	const FHullShaderRHIRef& GetHullShader();

	/**
	* @return the shader's domain shader
	*/
	const FDomainShaderRHIRef& GetDomainShader();

	/**
	* @return the shader's geometry shader
	*/
	const FGeometryShaderRHIRef& GetGeometryShader();
	/**
	* @return the shader's compute shader
	*/
	const FComputeShaderRHIRef& GetComputeShader();
#endif

	// Accessors.
	const FGuid& GetId() const { return Id; }
	/** Returns the hash of the shader file that this shader was compiled with. */
	const FSHAHash& GetHash() const;
	virtual const FVertexFactoryParameterRef* GetVertexFactoryParameterRef() const { return NULL; }
	FShaderType* GetType() const { return Type; }
	const TArray<BYTE>& GetCode() const { return Key.Code; }
	const FShaderKey& GetKey() const { return Key; }
	UINT GetNumInstructions() const { return NumInstructions; }
	const FShaderTarget GetTarget() const { return Target; }

	/**
	 * Adds the guid from another shader to my guid alias table, unless I am already decompressed
	 * @param Other shader to get guid from
	 */
	void AddAlias(const FShader* Other);

	/**
	 * Serializes a shader reference by GUID.
	 */
	friend FArchive& operator<<(FArchive& Ar,FShader*& Ref);

private:

	FShaderKey Key;

	FShaderTarget Target;
	FVertexShaderRHIRef VertexShader;
	FPixelShaderRHIRef PixelShader;
#if WITH_D3D11_TESSELLATION
	FHullShaderRHIRef HullShader;
	FDomainShaderRHIRef DomainShader;
	FGeometryShaderRHIRef GeometryShader;
	FComputeShaderRHIRef ComputeShader;
#endif

	/** The shader type. */
	FShaderType* Type;

	/** A unique identifier for the shader. */
	FGuid Id;

	/** Other GUIDS to look for */
	TArray<FGuid> Aliases;

#if !CONSOLE
	// Hash of the shader's source file, used by the automatic versioning system to detect changes
	FSHAHash Hash;
#endif

	/** The number of references to this shader. */
	mutable UINT NumRefs;

	/** The shader's element id in the shader code map. */
	FSetElementId CodeMapId;

	/** The number of instructions the shader takes to execute. */
	UINT NumInstructions;

	/** 
	 * The number of times this shader has been requested to be initialized on the game thread. 
	 * This is used to know when to release the shader's resources even when it is not being deleted.
	 */
	mutable INT NumResourceInitRefs;
};

/**
 * An object which is used to serialize/deserialize, compile, and cache a particular shader class.
 */
class FShaderType
{
public:

	typedef class FShader* (*ConstructSerializedType)();
	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform,FShaderCompilerEnvironment&);

	/**
	* @return The global shader factory list.
	*/
	static TLinkedList<FShaderType*>*& GetTypeList();

	/**
	* @return The global shader name to type map
	*/
	static TMap<FName, FShaderType*>& GetNameToTypeMap();

	/**
	* Gets a list of FShaderTypes whose source file no longer matches what that type was compiled with
	*/
	static void GetOutdatedTypes(TArray<FShaderType*>& OutdatedShaderTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes);

	/**
	 * Minimal initialization constructor.
	 */
	FShaderType(
		const TCHAR* InName,
		const TCHAR* InSourceFilename,
		const TCHAR* InFunctionName,
		DWORD InFrequency,
		INT InMinPackageVersion,
		INT InMinLicenseePackageVersion,
		ConstructSerializedType InConstructSerializedRef,
		ModifyCompilationEnvironmentType InModifyCompilationEnvironmentRef
		):
		Name(InName),
		SourceFilename(InSourceFilename),
		FunctionName(InFunctionName),
		Frequency(InFrequency),
		MinPackageVersion(InMinPackageVersion),
		MinLicenseePackageVersion(InMinLicenseePackageVersion),
		ConstructSerializedRef(InConstructSerializedRef),
		ModifyCompilationEnvironmentRef(InModifyCompilationEnvironmentRef)
	{
		//make sure the name is shorter than the maximum serializable length
		check(appStrlen(InName) < NAME_SIZE);
		check(InMinPackageVersion <= VER_LATEST_ENGINE);
		check(InMinLicenseePackageVersion <= VER_LATEST_ENGINE_LICENSEE);

		// register this shader type
		(new TLinkedList<FShaderType*>(this))->Link(GetTypeList());
		GetNameToTypeMap().Set(FName(InName), this);

		// Assign the vertex factory type the next unassigned hash index.
		static DWORD NextHashIndex = 0;
		HashIndex = NextHashIndex++;
	}

	/**
	 * Registers a shader for lookup by ID or code.
	 */
	void RegisterShader(FShader* Shader);

	/**
	 * Removes a shader from the ID and code lookup maps.
	 */
	void DeregisterShader(FShader* Shader);

	/**
	 * Finds a shader of this type with the specified output.
	 * @return NULL if no shader with the specified code was found.
	 */
	FShader* FindShaderByOutput(const FShaderCompilerOutput& Output) const;

	/**
	 * Finds a shader of this type by ID.
	 * @return NULL if no shader with the specified ID was found.
	 */
	FShader* FindShaderById(const FGuid& Id) const;

	/**
	 * Constructs a new instance of the shader type for deserialization.
	 */
	FShader* ConstructForDeserialization() const;

	// Accessors.
	DWORD GetFrequency() const 
	{ 
		return Frequency; 
	}
	INT GetMinPackageVersion() const 
	{ 
		return MinPackageVersion; 
	}
	INT GetMinLicenseePackageVersion() const
	{
		return MinLicenseePackageVersion;
	}
	const TCHAR* GetName() const 
	{ 
		return Name; 
	}
	const TCHAR* GetShaderFilename() const 
	{ 
		return SourceFilename; 
	}
	/** 
	 * Returns the number of shaders of this type.
	 *
	 * @return number of shaders in shader code map
	 */
	INT GetNumShaders() const
	{
		return ShaderIdMap.Num();
	}

	/** Calculates a Hash based on this shader type's source code and includes */
	const FSHAHash& GetSourceHash() const;

	/**
	 * Serializes a shader type reference by name.
	 */
	friend FArchive& operator<<(FArchive& Ar,FShaderType*& Ref);
	
	// Hash function.
	friend DWORD GetTypeHash(FShaderType* Ref)
	{
		return Ref ? Ref->HashIndex : 0;
	}

	// Dynamic casts.
	virtual class FGlobalShaderType* GetGlobalShaderType() { return NULL; }
	virtual class FNGPGlobalShaderType* GetNGPGlobalShaderType() { return NULL; }
	virtual class FMaterialShaderType* GetMaterialShaderType() { return NULL; }
	virtual class FMeshMaterialShaderType* GetMeshMaterialShaderType() { return NULL; }

protected:

	/**
	 * Enqueues a shader to be compiled with the shader type's compilation parameters, using the provided shader environment.
	 * @param VFType - Optional vertex factory type that the shader belongs to.
	 * @param Platform - Platform to compile for.
	 * @param Environment - The environment to compile the shader in.
	 */
	void BeginCompileShader(UINT Id, FVertexFactoryType* VFType, EShaderPlatform Platform, const FShaderCompilerEnvironment& Environment);

private:
	DWORD HashIndex;
	const TCHAR* Name;
	const TCHAR* SourceFilename;
	const TCHAR* FunctionName;
	DWORD Frequency;
	INT MinPackageVersion;
	INT MinLicenseePackageVersion;

	ConstructSerializedType ConstructSerializedRef;
	ModifyCompilationEnvironmentType ModifyCompilationEnvironmentRef;

	/** A map from shader ID to shader.  A shader will be removed from it when deleted, so this doesn't need to use a TRefCountPtr. */
	TMap<FGuid,FShader*> ShaderIdMap;

	// DumpShaderStats needs to access ShaderIdMap.
	friend void DumpShaderStats( EShaderPlatform Platform, EShaderFrequency Frequency );

	/**
	 * Functions to extract the shader code from a FShader* as a key for TSet.
	 */
	struct FShaderCodeKeyFuncs : BaseKeyFuncs<FShader*,FShaderKey,TRUE>
	{
		static const KeyType& GetSetKey(FShader* Shader)
		{
			return Shader->GetKey();
		}

		static UBOOL Matches(const KeyType& A, const KeyType& B)
		{
			return A.ParameterMapCRC == B.ParameterMapCRC && A.Code == B.Code;
		}

		static DWORD GetKeyHash(const KeyType& ShaderKey)
		{
			return appMemCrc(&ShaderKey.Code(0), ShaderKey.Code.Num(), ShaderKey.ParameterMapCRC);
		}
	};

	/** A map from shader code to shader. */
	TSet<FShader*,FShaderCodeKeyFuncs> ShaderCodeMap;
};

/**
 * A macro to declare a new shader type.  This should be called in the class body of the new shader type.
 * @param ShaderClass - The name of the class representing an instance of the shader type.
 * @param ShaderMetaTypeShortcut - The shortcut for the shader meta type: simple, material, meshmaterial, etc.  The shader meta type
 *	controls 
 */
#define DECLARE_SHADER_TYPE(ShaderClass,ShaderMetaTypeShortcut) \
	public: \
	typedef F##ShaderMetaTypeShortcut##ShaderType ShaderMetaType; \
	static ShaderMetaType StaticType; \
	static FShader* ConstructSerializedInstance() { return new ShaderClass(); } \
	static FShader* ConstructCompiledInstance(const ShaderMetaType::CompiledShaderInitializerType& Initializer) \
	{ return new ShaderClass(Initializer); }

/**
 * A macro to implement a shader type.
 */
#define IMPLEMENT_SHADER_TYPE(TemplatePrefix,ShaderClass,SourceFilename,FunctionName,Frequency,MinPackageVersion,MinLicenseePackageVersion) \
	TemplatePrefix \
	ShaderClass::ShaderMetaType ShaderClass::StaticType( \
		TEXT(#ShaderClass), \
		SourceFilename, \
		FunctionName, \
		Frequency, \
		Max((UINT)VER_MIN_SHADER,(UINT)MinPackageVersion), \
		Max((UINT)LICENSEE_VER_MIN_SHADER,(UINT)MinLicenseePackageVersion), \
		ShaderClass::ConstructSerializedInstance, \
		ShaderClass::ConstructCompiledInstance, \
		ShaderClass::ModifyCompilationEnvironment, \
		ShaderClass::ShouldCache \
		);


 /**
 * A macro to implement a shader type, the function name and the source filename comes from the class.
 */
#define IMPLEMENT_SHADER_TYPE2(TemplatePrefix,ShaderClass,Frequency,MinPackageVersion,MinLicenseePackageVersion) \
	TemplatePrefix \
	ShaderClass::ShaderMetaType ShaderClass::StaticType( \
	TEXT(#ShaderClass), \
	ShaderClass::GetSourceFilename(), \
	ShaderClass::GetFunctionName(), \
	Frequency, \
	Max((UINT)VER_MIN_SHADER,(UINT)MinPackageVersion), \
	Max((UINT)LICENSEE_VER_MIN_SHADER,(UINT)MinLicenseePackageVersion), \
	ShaderClass::ConstructSerializedInstance, \
	ShaderClass::ConstructCompiledInstance, \
	ShaderClass::ModifyCompilationEnvironment, \
	ShaderClass::ShouldCache \
	);
/**
 * A collection of shaders of different types, but the same meta type.
 */
template<typename ShaderMetaType>
class TShaderMap
{
public:

	/** Default constructor. */
	TShaderMap():
		ResourceInitCount(0)
	{}

	/** Finds the shader with the given type.  Asserts on failure. */
	template<typename ShaderType>
	ShaderType* GetShader() const
	{
		const TRefCountPtr<FShader>* ShaderRef = Shaders.Find(&ShaderType::StaticType);
		checkf(ShaderRef != NULL && *ShaderRef != NULL, TEXT("Failed to find shader type %s"), ShaderType::StaticType.GetName());
		return (ShaderType*)(FShader*)*ShaderRef;
	}

	/** Finds the shader with the given type.  May return NULL. */
	FShader* GetShader(FShaderType* ShaderType) const
	{
		const TRefCountPtr<FShader>* ShaderRef = Shaders.Find(ShaderType);
		return ShaderRef ? (FShader*)*ShaderRef : NULL;
	}

	/** Finds the shader with the given type. */
	UBOOL HasShader(ShaderMetaType* Type) const
	{
		const TRefCountPtr<FShader>* ShaderRef = Shaders.Find(Type);
		return ShaderRef != NULL && *ShaderRef != NULL;
	}

	inline const TMap<FShaderType*,TRefCountPtr<FShader> >& GetShaders() const
	{
		return Shaders;
	}

	void Serialize(FArchive& Ar)
	{
		// Serialize the shader references by factory and GUID.
		Ar << Shaders;
	}

	void AddShader(ShaderMetaType* Type,FShader* Shader)
	{
		Shaders.Set(Type,Shader);

		// Increment the shader initialization count by the number of times this map has been initialized.
		for(INT ResourceInitIteration = 0;ResourceInitIteration < ResourceInitCount;ResourceInitIteration++)
		{
			Shader->BeginInit();
		}
	}

	/**
	 * Merges OtherShaderMap's shaders
	 */
	void Merge(const TShaderMap<ShaderMetaType>* OtherShaderMap)
	{
		check(OtherShaderMap);
		TMap<FGuid,FShader*> OtherShaders;
		OtherShaderMap->GetShaderList(OtherShaders);
		for(TMap<FGuid,FShader*>::TIterator ShaderIt(OtherShaders);ShaderIt;++ShaderIt)
		{
			FShader* CurrentShader = ShaderIt.Value();
			check(CurrentShader);
			ShaderMetaType* CurrentShaderType = (ShaderMetaType*)CurrentShader->GetType();
			if (!HasShader(CurrentShaderType))
			{
				AddShader(CurrentShaderType,CurrentShader);
			}
		}
	}

/**
 * AddGuidAliases - finds corresponding guids and adds them to the FShaders alias list
 * @param OtherShaderMap contains guids that will exist in a compressed shader cache, but will not necessarily have FShaders
 * @return FALSE if these two shader maps are not compatible
 */
	UBOOL AddGuidAliases(const TShaderMap<ShaderMetaType>* OtherShaderMap)
	{
		check(OtherShaderMap);
		TMap<FGuid,FShader*> OtherShaders;
		OtherShaderMap->GetShaderList(OtherShaders);
		for(TMap<FGuid,FShader*>::TIterator ShaderIt(OtherShaders);ShaderIt;++ShaderIt)
		{
			FShader* CurrentShader = ShaderIt.Value();
			check(CurrentShader);
			ShaderMetaType* CurrentShaderType = (ShaderMetaType*)CurrentShader->GetType();
			FShader* Shader = GetShader(CurrentShaderType);
			if (!Shader)
			{
				warnf(TEXT("Missing meta type %s"),CurrentShaderType->GetName());
				return FALSE;
			}
			else
			{
				Shader->AddAlias(CurrentShader);
			}
		}
		return TRUE;
	}

	/**
	 * Removes the shader of the given type from the shader map
	 * @param Type Shader type to remove the entry for 
	 */
	void RemoveShaderType(ShaderMetaType* Type)
	{
		Shaders.Remove(Type);
	}

	/**
	 * Builds a list of the shaders in a shader map.
	 */
	void GetShaderList(TMap<FGuid,FShader*>& OutShaders) const
	{
		for(TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator ShaderIt(Shaders);ShaderIt;++ShaderIt)
		{
			if(ShaderIt.Value())
			{
				OutShaders.Set(ShaderIt.Value()->GetId(),ShaderIt.Value());
			}
		}
	}
	
	/**
	 * Begins initializing the shaders used by the shader map.
	 */
	void BeginInit()
	{
		for(TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator ShaderIt(Shaders);ShaderIt;++ShaderIt)
		{
			if(ShaderIt.Value())
			{
				ShaderIt.Value()->BeginInit();
			}
		}
		ResourceInitCount++;
	}

	/**
	 * Begins releasing the shaders used by the shader map.
	 */
	void BeginRelease()
	{
		for(TMap<FShaderType*,TRefCountPtr<FShader> >::TConstIterator ShaderIt(Shaders);ShaderIt;++ShaderIt)
		{
			if(ShaderIt.Value())
			{
				ShaderIt.Value()->BeginRelease();
			}
		}
		ResourceInitCount--;
		check(ResourceInitCount >= 0);
	}

	/**
	 *	IsEmpty - Returns TRUE if the map is empty
	 */
	UBOOL IsEmpty()
	{
		return ((Shaders.Num() == 0) ? TRUE : FALSE);
	}

	UINT GetNumShaders() const
	{
		return Shaders.Num();
	}

	/**
	 *	Empty - clears out all shaders held in the map
	 */
	void Empty()
	{
		Shaders.Empty();
	}

protected:
	TMap<FShaderType*,TRefCountPtr<FShader> > Shaders;
	INT ResourceInitCount;
};

/**
 * A reference which is initialized with the requested shader type from a shader map.
 */
template<typename ShaderType>
class TShaderMapRef
{
public:
	TShaderMapRef(const TShaderMap<typename ShaderType::ShaderMetaType>* ShaderIndex):
	 Shader(ShaderIndex->template GetShader<ShaderType>()) // gcc3 needs the template quantifier so it knows the < is not a less-than
	{}
	ShaderType* operator->() const
	{
		return Shader;
	}
	ShaderType* operator*() const
	{
		return Shader;
	}
private:
	ShaderType* Shader;
};

/**
 * Recursively populates IncludeFilenames with the include filenames from Filename
 */
extern void GetShaderIncludes(const TCHAR* Filename, TArray<FString> &IncludeFilenames, UINT DepthLimit=7);

/**
 * Calculates a Hash for the given filename if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 */
extern const FSHAHash& GetShaderFileHash(const TCHAR* Filename);

/**
 * Flushes the shader file and CRC cache, and regenerates the binary shader files if necessary.
 * Allows shader source files to be re-read properly even if they've been modified since startup.
 */
extern void FlushShaderFileCache();

/**
 * Dumps shader stats to the log.
 * 
 * @param	Platform	Platform to dump shader info for, use SP_NumPlatforms for all
 * @para	Frequency	Whether to dump PS or VS info, use SF_NumFrequencies to dump both
 */
extern void DumpShaderStats( EShaderPlatform Platform, EShaderFrequency Frequency );

/**
 * Finds the shader type with a given name.
 *
 * @param ShaderTypeName - The name of the shader type to find.
 * @return The shader type, or NULL if none matched.
 */
extern FShaderType* FindShaderTypeByName(const TCHAR* ShaderTypeName);

/** Encapsulates scene texture shader parameter bindings. */
class FSceneTextureShaderParameters
{
public:
	/** Binds the parameters using a compiled shader's parameter map. */
	void Bind(const FShaderParameterMap& ParameterMap);

	void SetSceneColorTextureOnly(FShader* PixelShader) const;

	/** Sets the scene texture parameters for the given view. */
	void Set(const FSceneView* View,FShader* PixelShader, ESamplerFilter ColorFilter=SF_Point, ESceneDepthUsage DepthUsage=SceneDepthUsage_Normal) const;

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FSceneTextureShaderParameters& P);
	
private:
	/** Sets the scene texture parameters for the given view with a user-specified scene color texture. */
	void SetCustom(const FSceneView* View,FShader* PixelShader, ESamplerFilter ColorFilter, const FTexture2DRHIRef& DesiredSceneColorTexture, ESceneDepthUsage DepthUsage=SceneDepthUsage_Normal) const;
public:

#if !WITH_MOBILE_RHI // @todo ngp clean
private:
#endif
	/** The SceneColorTexture parameter for materials that use SceneColor */
	FShaderResourceParameter SceneColorTextureParameter;
	/** The SceneDepthTexture parameter for materials that use SceneDepth */
	FShaderResourceParameter SceneDepthTextureParameter;
	/** The SceneColorTextureMSAA parameter for materials that use SceneColorTextureMSAA */
	FShaderResourceParameter SceneDepthSurfaceParameter;
	/** Required parameter for using SceneDepthTexture on certain platforms. */
	FShaderParameter SceneDepthCalcParameter;
	/** Required parameter for using SceneColorTexture. */
	FShaderParameter ScreenPositionScaleBiasParameter;
	/** Required parameter for using SreenSize expression. */
	FShaderParameter ScreenAndTexelSizeParameter;
#if !CONSOLE
	/** Parameter to fix stereo offsets */
    FShaderResourceParameter NvStereoFixTextureParameter;
#endif
	/** true if GSceneRenderTargets.bSceneColorTextureIsRaw */
	FShaderParameter DecompressSceneColorParameter;
};


////////////////////////////////////
//
// Mobile shader initialization (OpenGL ES 2)
// 
////////////////////////////////////

// Set to 1 to perform the initial ES2 shader compiling asynchronously on the render thread during startup.
// Set to 0 to use the old version that compiled it while blocking the game thread.
#define MOBILESHADER_THREADED_INIT	(!FLASH && 1)

enum EMobileShaderInitState
{
	// Shader init has not yet been triggered
	MobileShaderInit_Pending = 0,
	// Shader init has started
	MobileShaderInit_Started,
	// Shader init is finished
	MobileShaderInit_Finished,
};

/**
 * Helper class to manage mobile shader initialization at startup.
 */
typedef unsigned int     GLuint;
class FMobileShaderInitialization
{
public:
	/** Constructor. */
	FMobileShaderInitialization()
	:	CurrentState(MobileShaderInit_Pending)
	,	CompletionFence(NULL)
	,	bTemporarilyEnableRenderthread(FALSE)
	,	bPauseGameRendering(FALSE)
	,	bAllShaderGroupsLoaded(FALSE)
	,	bCachedProgramKeysLoaded(FALSE)
	{
	}

	/** Destructor. */
	~FMobileShaderInitialization();

	/**
	 * Pauses game rendering while we're compiling mobile shaders on the rendering thread.
	 * Stops the rendering thread when compiling finished, if it was temporarily turned on during compiling.
	 */
	void	Tick();

	/**
	 * These functions start compiling shaders on the rendering thread.
	 * They start the rendering thread if it's not already running, even on single-core CPUs.
	 */
	void	StartCompilingShaderGroup( FName ShaderGroupName, UBOOL bPauseGameRenderingWhileCompiling );
	void	StartCompilingShaderGroupByMapName( FString MapName, UBOOL bPauseGameRenderingWhileCompiling );

	/** Returns the FName "None" if no group could be found for the MapName */
	FName 	GetShaderGroupNameFromMapName( FString MapName );

	UBOOL	IsProgramKeyInGroup( FProgramKey ProgramKey );

	/** Loads all defined shader groups */
	void	LoadAllShaderGroups();
	void	LoadCachedShaderKeys();
	void	LoadShaderSource(TArray<FString> & Keys, TArray<FProgramKey> & GroupedProgramKeys, TArray<FProgramKey> & UngroupedProgramKeys);

	EMobileShaderInitState	GetCurrentState() const
	{
		return CurrentState;
	}

	static UBOOL LoadShaderGroup( const FString& FilePath, TArray<FProgramKey>& ShaderGroup );

	GLuint* GetPixelShaderFromPixelMasterKey( const FProgramKey& ProgramKey, DWORD TextureUsageFlags);
	void SetPixelShaderForPixelMasterKey(const FProgramKey& ProgramKey, DWORD TextureUsageFlags, GLuint Shader);

	/** Current state of the mobile ES2 shader initialization. */
	EMobileShaderInitState	CurrentState;

	/** Flagged when the shader init has completed on the render thread. */
	class FRenderCommandFence*	CompletionFence;

	/** Set to TRUE if the renderthread is temporarily enabled to compile shaders. */
	UBOOL bTemporarilyEnableRenderthread;

	/** Will keep the game not rendering while the shaders are being compiled on the render thread, e.g. during startup. */
	UBOOL								bPauseGameRendering;

	/** RenderThread */
	TMap<FName, TArray<FName> >			ShaderGroupPackages;

	/** RenderThread. Shader groups that haven't been fully compiled yet. */
	TMap<FName, TArray<FProgramKey> >	PendingShaderGroups;

	/** RenderThread. Shader groups that are being compiled. */
	TArray<FName>						CompilingShaderGroups;

	/** Set to true when all known shader groups have been loaded. */
	UBOOL								bAllShaderGroupsLoaded;

	/** Maps to convert a program key to "master key", which is the canonical id for a program equivalence class */
	TMap<FProgramKey,FProgramKey> VertexKeyToEquivalentMasterKey;
	TMap<FProgramKey,FProgramKey> PixelKeyToEquivalentMasterKey;

	/** Map to convert a vertex "master key" into the compiled and ready GLSL shader */
	TMap<FProgramKey,GLuint> VertexMasterKeyToGLShader;
	
	UBOOL	bCachedProgramKeysLoaded;

	friend class FES2ShaderProgram;
	friend class FES2ShaderManager;

private:
	/** Map to convert a vertex "master key" into the compiled and ready GLSL shader */
#if FLASH
	TMap<FProgramKey, TMap<DWORD, GLuint> > PixelMasterKeyToGLShader;
#else
	TMap<FProgramKey,GLuint> PixelMasterKeyToGLShader;
#endif
};

/** Helper container for all information about ES2 shader startup initialization. */
extern FMobileShaderInitialization GMobileShaderInitialization;


#endif
