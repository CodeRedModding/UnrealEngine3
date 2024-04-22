/*=============================================================================
	GlobalShaderNGP.h: Shader manager definitions for NGP.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __GLOBALSHADERNGP_H__
#define __GLOBALSHADERNGP_H__


class FNGPGlobalShaderType : public FGlobalShaderType 
{
public:
	FNGPGlobalShaderType(
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
	FGlobalShaderType(InName,InSourceFilename,InFunctionName,InFrequency,InMinPackageVersion,InMinLicenseePackageVersion,InConstructSerializedRef,InConstructCompiledRef,InModifyCompilationEnvironmentRef,InShouldCacheRef)
	{}

	virtual class FNGPGlobalShaderType* GetNGPGlobalShaderType() { return this; }
};

struct FNGPShaderCompileInfo
{
	FNGPShaderCompileInfo( const FProgramKey& InProgramKey, EShaderFrequency InFrequency )
	:	ProgramKey(InProgramKey)
	,	Frequency(InFrequency)
	{
	}
	FProgramKey ProgramKey;
	EShaderFrequency Frequency;
};

/**
 * Base shader type for NGP.
 */
class FShaderNGP : public FGlobalShader
{
public:
	FShaderNGP( ) { }
	FShaderNGP( const ShaderMetaType::CompiledShaderInitializerType& Initializer );

	virtual void		Setup( const FNGPShaderCompileInfo& Info );
	const FProgramKey&	GetProgramKey( )	{ return ProgramKey; }
	
	static FShaderNGP*	FindShader( const FProgramKey& ProgramKey, EShaderFrequency Frequency );

	// FShader interface.
	static UBOOL		ShouldCache(EShaderPlatform Platform);
	virtual UBOOL		Serialize(FArchive& Ar);

protected:
	FProgramKey			ProgramKey;
};

/**
 * Vertex shader for NGP.
 */
class FVertexShaderNGP : public FShaderNGP
{
	DECLARE_SHADER_TYPE(FVertexShaderNGP,NGPGlobal);
public:
	FVertexShaderNGP( )	{ }
	FVertexShaderNGP( const ShaderMetaType::CompiledShaderInitializerType& Initializer );

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar);
};

/**
 * Pixel shader for NGP.
 */
class FPixelShaderNGP : public FShaderNGP
{
	DECLARE_SHADER_TYPE(FPixelShaderNGP,NGPGlobal);
public:
	FPixelShaderNGP( )	{ }
	FPixelShaderNGP( const ShaderMetaType::CompiledShaderInitializerType& Initializer );

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar);
};

void SerializeGlobalShadersNGP( FArchive& Ar );
void NGPBeginCompileShader( TArray<FNGPShaderCompileInfo>& ShaderCompileInfos, const TCHAR* VertexFileName, const TCHAR* PixelFileName, const FProgramKey& ProgramKey );
void NGPFinishCompileShaders( const TArray<FNGPShaderCompileInfo>& ShaderCompileInfos );

#endif
