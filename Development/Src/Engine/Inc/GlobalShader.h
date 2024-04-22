/*=============================================================================
	GlobalShader.h: Shader manager definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __GLOBALSHADER_H__
#define __GLOBALSHADER_H__

#include "ShaderManager.h"

/**
 * A shader meta type for the simplest shaders; shaders which are not material or vertex factory linked.
 * There should only a single instance of each simple shader type.
 */
class FGlobalShaderType : public FShaderType
{
public:

	typedef FShader::CompiledShaderInitializerType CompiledShaderInitializerType;
	typedef FShader* (*ConstructCompiledType)(const CompiledShaderInitializerType&);
	typedef UBOOL (*ShouldCacheType)(EShaderPlatform);

	FGlobalShaderType(
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
	 * Enqueues compilation of a shader of this type.  
	 * @param Compiler - The shader compiler to use.
	 * @param OutErrors - Upon compilation failure, OutErrors contains a list of the errors which occured.
	 * @param bDebugDump - Dump out the preprocessed and disassembled shader for debugging.
	 * @param ShaderSubDir - Sub directory for dumping out preprocessor output.
	 */
	void BeginCompileShader(EShaderPlatform Platform);

	/** Either returns an equivalent existing shader of this type, or constructs a new instance. */
	FShader* FinishCompileShader(const FShaderCompileJob& CompileJob);

	/**
	 * Checks if the shader type should be cached for a particular platform.
	 * @param Platform - The platform to check.
	 * @return True if this shader type should be cached.
	 */
	UBOOL ShouldCache(EShaderPlatform Platform) const
	{
		return (*ShouldCacheRef)(Platform);
	}

	// Dynamic casting.
	virtual FGlobalShaderType* GetGlobalShaderType() { return this; }

private:
	ConstructCompiledType ConstructCompiledRef;
	ShouldCacheType ShouldCacheRef;
};

/**
 * FGlobalShader
 * 
 * Global shaders derive from this class to set their default recompile group as a global one
 */
class FGlobalShader : public FShader
{
	DECLARE_SHADER_TYPE(FGlobalShader,Global);
public:
	FGlobalShader() : FShader() {}

	FGlobalShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FShader(Initializer) {}
};

/**
 * Vertex shader for rendering a single, constant color.
 */
class FOneColorVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOneColorVertexShader,Global);
public:
	FOneColorVertexShader( )	{ }
	FOneColorVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader( Initializer )
	{
	}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}
};

/**
 * Pixel shader for rendering a single, constant color.
 */
class FOneColorPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOneColorPixelShader,Global);
public:
	FOneColorPixelShader( )	{ }
	FOneColorPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader( Initializer )
	{
		ColorParameter.Bind( Initializer.ParameterMap, TEXT("DrawColor") );
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ColorParameter;
		return bShaderHasOutdatedParameters;
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	/** The parameter to use for setting the draw Color. */
	FShaderParameter ColorParameter;
};

/**
 * An internal dummy pixel shader to use when the user calls RHISetPixelShader(NULL).
 */
class FNULLPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FNULLPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	FNULLPixelShader( )	{ }
	FNULLPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
	}
};

/**
 * Vertex shader for rendering a splash screen.
 */
class FSplashVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSplashVertexShader,Global);
public:
	FSplashVertexShader()	{ }
	FSplashVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
	}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return ((Platform == SP_XBOXD3D) ? TRUE : FALSE);
	}
};

/**
 * Pixel shader for rendering a splash screen.
 */
class FSplashPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSplashPixelShader,Global);
public:
	FSplashPixelShader()	{ }
	FSplashPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SplashTextureParameter.Bind(Initializer.ParameterMap, TEXT("SplashTexture"));
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SplashTextureParameter;
		return bShaderHasOutdatedParameters;
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return ((Platform == SP_XBOXD3D) ? TRUE : FALSE);
	}

	/** The parameter to use for setting the texture to render as the splash. */
	FShaderResourceParameter SplashTextureParameter;
};

// Restore color and depth shaders
/**
 * Vertex shader for restoring the color and depth buffers
 */
class FRestoreColorAndDepthVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRestoreColorAndDepthVertexShader,Global);
public:
	FRestoreColorAndDepthVertexShader()	{ }
	FRestoreColorAndDepthVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
	}

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return ((Platform == SP_XBOXD3D) ? TRUE : FALSE);
	}
};

/**
 * Pixel shader for restoring the depth buffer
 */
class FRestoreDepthOnlyPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRestoreDepthOnlyPixelShader,Global);
public:
	FRestoreDepthOnlyPixelShader()	{ }
	FRestoreDepthOnlyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		DepthTextureParameter.Bind(Initializer.ParameterMap, TEXT("DepthTex"));
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DepthTextureParameter;
		return bShaderHasOutdatedParameters;
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return ((Platform == SP_XBOXD3D) ? TRUE : FALSE);
	}

	/** The parameter to use for setting the texture to render as the splash. */
	FShaderResourceParameter DepthTextureParameter;
};

/**
 * Pixel shader for restoring the downsampled depth buffer
 */
class FRestoreDownsamplingDepthOnlyPixelShader : public FRestoreDepthOnlyPixelShader
{
	DECLARE_SHADER_TYPE(FRestoreDownsamplingDepthOnlyPixelShader,Global);
public:
	FRestoreDownsamplingDepthOnlyPixelShader() :
		FRestoreDepthOnlyPixelShader()
	{
	}
	FRestoreDownsamplingDepthOnlyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FRestoreDepthOnlyPixelShader(Initializer)
	{
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FRestoreDepthOnlyPixelShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return ((Platform == SP_XBOXD3D) ? TRUE : FALSE);
	}
};

/**
 * Pixel shader for restoring the color and depth buffers
 */
class FRestoreColorAndDepthPixelShader : public FRestoreDepthOnlyPixelShader
{
	DECLARE_SHADER_TYPE(FRestoreColorAndDepthPixelShader,Global);
public:
	FRestoreColorAndDepthPixelShader() :
		FRestoreDepthOnlyPixelShader()
	{
	}
	
	FRestoreColorAndDepthPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FRestoreDepthOnlyPixelShader(Initializer)
	{
		ColorTextureParameter.Bind(Initializer.ParameterMap, TEXT("ColorTex"));
		ColorBiasParameter.Bind(Initializer.ParameterMap, TEXT("ColorBias"));
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FRestoreDepthOnlyPixelShader::Serialize(Ar);
		Ar << ColorTextureParameter;
		Ar << ColorBiasParameter;
		return bShaderHasOutdatedParameters;
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return ((Platform == SP_XBOXD3D) ? TRUE : FALSE);
	}

	/** The parameter to use for setting the texture to render as the splash. */
	FShaderResourceParameter ColorTextureParameter;
	FShaderParameter ColorBiasParameter;
};

/**
 * Pixel shader for restoring the color and depth buffers
 */
class FRestoreDownsamplingColorAndDepthPixelShader : public FRestoreColorAndDepthPixelShader
{
	DECLARE_SHADER_TYPE(FRestoreDownsamplingColorAndDepthPixelShader,Global);
public:
	FRestoreDownsamplingColorAndDepthPixelShader() :
		FRestoreColorAndDepthPixelShader()
	{
	}
	
	FRestoreDownsamplingColorAndDepthPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FRestoreColorAndDepthPixelShader(Initializer)
	{
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FRestoreColorAndDepthPixelShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return ((Platform == SP_XBOXD3D) ? TRUE : FALSE);
	}
};

/**
 * Vertex shader for copying memory on Xbox
 */
class FMemCopyVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMemCopyVertexShader,Global);
public:
	FMemCopyVertexShader()	{ }
	FMemCopyVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		StreamConstantParameter.Bind(Initializer.ParameterMap, TEXT("StreamConstant"));
		OffsetInVertsParameter.Bind(Initializer.ParameterMap, TEXT("OffsetInVerts"));
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StreamConstantParameter;
		Ar << OffsetInVertsParameter;
		return bShaderHasOutdatedParameters;
	}
	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return ((Platform == SP_XBOXD3D) ? TRUE : FALSE);
	}

	FShaderParameter StreamConstantParameter;
	FShaderParameter OffsetInVertsParameter;
};

/** Encapsulates the gamma correction pixel shader. */
class FGammaCorrectionPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGammaCorrectionPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FGammaCorrectionPixelShader() {}

public:

	FShaderResourceParameter SceneTextureParameter;
	FShaderParameter InverseGammaParameter;
	FShaderParameter ColorScaleParameter;
	FShaderParameter OverlayColorParameter;

	/** Initialization constructor. */
	FGammaCorrectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar);
};

/** Encapsulates the gamma correction vertex shader. */
class FGammaCorrectionVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGammaCorrectionVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	/** Default constructor. */
	FGammaCorrectionVertexShader() {}

public:

	/** Initialization constructor. */
	FGammaCorrectionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
};

/** Serializes the global shader map using the specified archive. */
extern void SerializeGlobalShaders(EShaderPlatform Platform,FArchive& Ar);

/**
 * Makes sure all global shaders are loaded and/or compiled for the passed in platform.
 *
 * @param	Platform	Platform to verify global shaders for
 */
extern void VerifyGlobalShaders(EShaderPlatform Platform=GRHIShaderPlatform);

/**
 * Accesses the global shader map.  This is a global TShaderMap<FGlobalShaderType> which contains an instance of each global shader type.
 *
 * @param Platform Which platform's global shader map to use
 * @return A reference to the global shader map.
 */
extern TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(EShaderPlatform Platform=GRHIShaderPlatform);

/**
 * Forces a recompile of the global shaders.
 */
extern void RecompileGlobalShaders();

/**
* Recompiles the specified global shader types, and flushes their bound shader states.
*/
extern void RecompileGlobalShaders(const TArray<FShaderType*>& OutdatedShaderTypes);

#endif
