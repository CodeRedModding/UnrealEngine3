/**************************************************************************

Filename    :   RHI_Shader.h
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef INC_SF_RHI_Shader_H
#define INC_SF_RHI_Shader_H

#if WITH_GFx

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Render/Render_Shader.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#include "RHI_Shaders.h"
#include "Engine.h"

namespace Scaleform
{
namespace Render
{
namespace RHI
{

class HAL;
class Texture;

struct VertexShader : public FShader
{
	const VertexShaderDesc* VDesc;
	FShaderParameter        Uniforms[Uniform::SU_Count];
	int                     Attributes[8];

	static UBOOL ShouldCache ( EShaderPlatform Platform )
	{
		return TRUE;
	}

	VertexShader ( int ShaderType, const FGlobalShaderType::CompiledShaderInitializerType& Initializer );
	VertexShader() {}

	virtual UBOOL Serialize ( FArchive& Ar );
	void InitMobile(int uniform);
};

template<int VertexShaderType>
struct VertexShaderImpl : public VertexShader
{
	DECLARE_SHADER_TYPE ( RHI::VertexShaderImpl<VertexShaderType>, Global );

	VertexShaderImpl ( const FGlobalShaderType::CompiledShaderInitializerType& Initializer )
		:  VertexShader ( VertexShaderType, Initializer ) {}
	VertexShaderImpl()
	{
		VDesc = VertexShaderDesc::Descs[VertexShaderType];
	}
};

struct FragShader : public FShader
{
	const FragShaderDesc*     FDesc;
	FShaderParameter          Uniforms[Uniform::SU_Count];
	FShaderResourceParameter  TexUniforms[Resource::SR_Count];

	static UBOOL ShouldCache ( EShaderPlatform Platform )
	{
		return TRUE;
	}

	FragShader ( int ShaderType, const FGlobalShaderType::CompiledShaderInitializerType& Initializer );
	FragShader() {}

	virtual UBOOL Serialize ( FArchive& Ar );
	void InitMobile(int uniform);
};

template<int FragShaderType>
struct FragShaderImpl : public FragShader
{
	DECLARE_SHADER_TYPE ( RHI::FragShaderImpl<FragShaderType>, Global );

	FragShaderImpl ( const FGlobalShaderType::CompiledShaderInitializerType& Initializer )
		:   FragShader ( FragShaderType, Initializer ) {}
	FragShaderImpl()
	{
		FDesc = FragShaderDesc::Descs[FragShaderType];
	}
};


struct ShaderPair
{
	VertexShader*           VS;
	const VertexShaderDesc* pVDesc;
	FVertexShaderRHIRef     VSRHI;
	FragShader*             FS;
	const FragShaderDesc*   pFDesc;
	FPixelShaderRHIRef      FSRHI;
	class SysVertexFormat*  VDecl;
	FBoundShaderStateRHIRef ShaderStateRHI;

	ShaderPair() : VS ( 0 ), pVDesc ( 0 ), FS ( 0 ), pFDesc ( 0 ) {}

	const ShaderPair* operator->() const
	{
		return this;
	}
};

class SysVertexFormat : public Render::SystemVertexFormat
{
	public:
		FVertexDeclarationElementList   Elements;
		FVertexDeclarationRHIRef        VDeclRHI;
		DWORD                           Strides[8];

		SysVertexFormat ( const VertexFormat* vf );
		SysVertexFormat ( const SysVertexFormat* psinglevf );
};


class ShaderInterface : public ShaderInterfaceBase<Uniform, ShaderPair>
{
		struct BoundShaderHashKey
		{
			bool operator==(const BoundShaderHashKey& key) const
			{
				return DeclPtr == key.DeclPtr && VShaderIndex == key.VShaderIndex &&
				FShaderIndex == key.FShaderIndex; 
			}
			UPInt   DeclPtr;
			UInt16  VShaderIndex;
			UInt16  FShaderIndex;
		};

		typedef Hash<BoundShaderHashKey, FBoundShaderStateRHIRef> ShaderStatesType;

		HAL*                Hal;
		ShaderPair          CurShaders;
		const FragShader*   LastFS;
		const VertexShader* LastVS;
		FSamplerStateRHIRef SamplerStates[8];
		ShaderStatesType    ShaderStates;

	public:
		typedef ShaderPair Shader;

		ShaderInterface ( HAL* hal ) : Hal ( hal ), LastFS ( 0 ), LastVS ( 0 ) {}

		void                BeginFrame()
		{
			LastVS = 0;
			LastFS = 0;
		}
		void                BeginPrimitive();
		const ShaderPair&   GetCurrentShaders() const
		{
			return CurShaders;
		}
		bool                SetStaticShader ( VertexShaderDesc::ShaderType vshader,
		                                      FragShaderDesc::ShaderType shader, const VertexFormat* pvf );

		FSamplerStateRHIRef GetSamplerState ( ImageFillMode InFill, UBOOL bUseMips );

		void                SetTexture ( ShaderPair sp, unsigned stage, Texture* ptexture, ImageFillMode fm );

		void                Finish ( unsigned meshCount );
};

class ShaderManager : public StaticShaderManager<FragShaderDesc, VertexShaderDesc, Uniform, ShaderInterface, Texture>
{
	public:
		typedef StaticShaderManager<FragShaderDesc, VertexShaderDesc, Uniform, ShaderInterface, Texture> Base;

		FragShader*         StaticFShaders[FragShaderDesc::FS_Count];
		VertexShader*       StaticVShaders[VertexShaderDesc::VS_Count];

		ShaderManager ( ProfileViews* profiler );

		void    Initialize();

		void    MapVertexFormat ( PrimitiveFillType fill, const VertexFormat* sourceFormat,
		                          const VertexFormat** single, const VertexFormat** batch, const VertexFormat** instanced );
};

}
}
}

#endif//WITH_GFx

#endif
