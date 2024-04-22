/**************************************************************************

Filename    :   RHI_Shader.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "GFxUI.h"

#if WITH_GFx

#include "Render/RHI_Shader.h"
#include "Render/RHI_HAL.h"
#include "Render/RHI_Shaders.inl"


namespace Scaleform
{
namespace Render
{
namespace RHI
{

extern const char* ShaderUniformNames[Uniform::SU_Count];

VertexShader::VertexShader ( int ShaderType, const FGlobalShaderType::CompiledShaderInitializerType& Initializer ) : FShader ( Initializer )
{
	VDesc = VertexShaderDesc::Descs[ShaderType];

	for ( unsigned i = 0; i < Uniform::SU_Count; i++ )
	{
		if ( VDesc->Uniforms[i].Location >= 0 )
		{
			Uniforms[i].Bind(Initializer.ParameterMap, FANSIToTCHAR(ShaderUniformNames[i]));
			InitMobile(i);
		}
	}
}

void VertexShader::InitMobile(int i)
{
#if WITH_MOBILE_RHI
	switch (i)
	{
		case Uniform::SU_mvp:
			if (VDesc->Uniforms[i].ElementSize == 16)
			{
				Uniforms[i].SetShaderParamName( TEXT("Transform") );
			}
			else
			{
				Uniforms[i].SetShaderParamName( TEXT("Transform2D") );
			}
			break;

        case Uniform::SU_cxmul:
            if (VDesc->Uniforms[i].ElementSize == 16)
			{
                Uniforms[i].SetShaderParamName( TEXT("ColorMatrix") );
			}
			else
			{
                Uniforms[i].SetShaderParamName( TEXT("ColorScale") );
			}
			break;

		default:
			Uniforms[i].SetShaderParamName( FANSIToTCHAR(ShaderUniformNames[i]) );
			break;
	}
#endif
}

UBOOL VertexShader::Serialize ( FArchive& Ar )
{
	UBOOL Result = FShader::Serialize ( Ar );

	for ( unsigned i = 0; i < Uniform::SU_Count; i++ )
	{
		Ar << Uniforms[i];
		InitMobile(i);
	}

	for ( unsigned i = 0; i < 8; i++ )
	{
		Ar << Attributes[i];
	}

	return Result;
}

void FragShader::InitMobile(int i)
{
#if WITH_MOBILE_RHI
    switch (i)
    {
    case Uniform::SU_cxmul:
        if (FDesc->Uniforms[i].ElementSize == 16)
        {
            Uniforms[i].SetShaderParamName( TEXT("ColorMatrix") );
        }
        else
        {
            Uniforms[i].SetShaderParamName( TEXT("ColorScale") );
        }
        break;

    default:
        Uniforms[i].SetShaderParamName( FANSIToTCHAR(ShaderUniformNames[i]) );
        break;
    }
#endif
}

FragShader::FragShader ( int ShaderType, const FGlobalShaderType::CompiledShaderInitializerType& Initializer ) : FShader ( Initializer )
{
	FDesc = FragShaderDesc::Descs[ShaderType];

	for ( unsigned i = 0; i < Uniform::SU_Count; i++ )
	{
		if ( FDesc->Uniforms[i].Location >= 0 )
		{
			// optional params for scolor2
			Uniforms[i].Bind(Initializer.ParameterMap, FANSIToTCHAR(ShaderUniformNames[i]), TRUE);
            InitMobile(i);
		}
	}

	for ( unsigned i = 0; i < Resource::SR_Count; i++ )
	{
		if ( FDesc->Resources[i].Location >= 0 )
		{
			TexUniforms[i].Bind ( Initializer.ParameterMap, FANSIToTCHAR ( ShaderResourceNames[i] ) );
		}
	}
}

UBOOL FragShader::Serialize ( FArchive& Ar )
{
	UBOOL Result = FShader::Serialize ( Ar );

	for ( unsigned i = 0; i < Uniform::SU_Count; i++ )
	{
		Ar << Uniforms[i];
        InitMobile(i);
	}
	for ( unsigned i = 0; i < Resource::SR_Count; i++ )
	{
		Ar << TexUniforms[i];
	}

	return Result;
}

ShaderManager::ShaderManager ( ProfileViews* profiler )
	:   Base ( profiler )
{
}

void ShaderManager::Initialize()
{
	for ( unsigned i = 0; i < VertexShaderDesc::VS_Count; i++ )
	{
		if ( VertexShaderDesc::Descs[i] )
		{
			StaticVShaders[i] = VertexShaderDesc::GetShader ( ( VertexShaderDesc::ShaderType ) i );
		}
	}

	for ( unsigned i = 0; i < FragShaderDesc::FS_Count; i++ )
	{
		if ( FragShaderDesc::Descs[i] )
		{
			StaticFShaders[i] = FragShaderDesc::GetShader ( ( FragShaderDesc::ShaderType ) i );
		}
	}
}

void ShaderInterface::BeginPrimitive()
{
	ShaderInterfaceBase<Uniform, ShaderPair>::BeginPrimitive();

	RenderTargetData* RT = ( RenderTargetData* ) Hal->RenderTargetStack.Back().pRenderTarget->GetHALData();
	if ( GetUniformSize ( CurShaders, Uniform::SU_InverseGamma ) )
	{
		SetUniform ( CurShaders, Uniform::SU_InverseGamma, &RT->Resource.InverseGamma, 1 );
	}
}

bool ShaderInterface::SetStaticShader ( VertexShaderDesc::ShaderType vshader, FragShaderDesc::ShaderType shader, const VertexFormat* InVF )
{
	CurShaders.VS     = Hal->SManager.StaticVShaders[vshader];
	CurShaders.pVDesc = CurShaders.VS->VDesc;
	CurShaders.VSRHI  = CurShaders.VS->GetVertexShader();
	CurShaders.FS     = Hal->SManager.StaticFShaders[shader];
	CurShaders.pFDesc = CurShaders.FS->FDesc;
	CurShaders.FSRHI  = CurShaders.FS->GetPixelShader();
	CurShaders.VDecl  = ( ( SysVertexFormat* ) InVF->pSysFormat.GetPtr() );

	BoundShaderHashKey Key = { UPInt(CurShaders.VDecl), (UInt16)vshader, (UInt16)shader };

	if ( !ShaderStates.Get ( Key, &CurShaders.ShaderStateRHI ) )
	{
        INT MobileShader = EGST_GFxBegin + (shader << 1) + (vshader == FragShaderDesc::VShaderForFShader[shader] + VertexShaderDesc::VS_base_Position3d);
        CurShaders.ShaderStateRHI = RHICreateBoundShaderState(CurShaders.VDecl->VDeclRHI, CurShaders.VDecl->Strides,
                                                              CurShaders.VSRHI, CurShaders.FSRHI, (EMobileGlobalShaderType) MobileShader);
        ShaderStates.Add(Key, CurShaders.ShaderStateRHI);
	}

	RHISetBoundShaderState ( CurShaders.ShaderStateRHI );
	return true;
}

void ShaderInterface::Finish ( unsigned meshCount )
{
	for ( int i = 0; i < Uniform::SU_Count; i++ )
	{
		if ( UniformSet[i] )
		{
			if ( CurShaders.pFDesc->Uniforms[i].Location >= 0 )
			{
				RHISetPixelShaderParameter ( CurShaders.FSRHI, CurShaders.FS->Uniforms[i].GetBufferIndex(),
												CurShaders.FS->Uniforms[i].GetBaseIndex(), CurShaders.FS->Uniforms[i].GetNumBytes(),
												UniformData + CurShaders.pFDesc->Uniforms[i].ShadowOffset,
												CurShaders.FS->Uniforms[i].GetShaderParamName() );
			}
			else
			{
				check ( CurShaders.pVDesc->Uniforms[i].Location >= 0 );

				unsigned size;
				if ( CurShaders.pVDesc->Uniforms[i].BatchSize > 0 )
				{
					size = meshCount * CurShaders.pVDesc->Uniforms[i].BatchSize * CurShaders.pVDesc->Uniforms[i].ElementSize;
				}
				else
				{
					size = CurShaders.pVDesc->Uniforms[i].Size;
				}
				check ( size > 0 );

				RHISetVertexShaderParameter ( CurShaders.VSRHI, CurShaders.VS->Uniforms[i].GetBufferIndex(),
												CurShaders.VS->Uniforms[i].GetBaseIndex(), size << 2,
												UniformData + CurShaders.pVDesc->Uniforms[i].ShadowOffset,
												CurShaders.VS->Uniforms[i].GetShaderParamName() );
			}
		}
	}
	appMemset ( UniformSet, 0, Uniform::SU_Count );

#if NGP
    // Reset GlobalShader since InverseGamma resets it.
    RHISetBoundShaderState ( CurShaders.ShaderStateRHI );
#endif
}

FSamplerStateRHIRef ShaderInterface::GetSamplerState ( ImageFillMode InFill, UBOOL bUseMips )
{
	unsigned SamplerStateIndex = InFill.Fill | ( bUseMips ? Mipmap_2D : 0 );
	if ( SamplerStates[SamplerStateIndex] )
	{
		return SamplerStates[SamplerStateIndex];
	}

	FSamplerStateInitializerRHI SamplerInitializer ( ( InFill.GetSampleMode() == Sample_Linear ) ? SF_Trilinear : SF_Point );
	SamplerInitializer.AddressU = ( InFill.GetWrapMode() == Wrap_Clamp ) ? AM_Clamp : AM_Wrap;
	SamplerInitializer.AddressV = SamplerInitializer.AddressU;
	SamplerInitializer.AddressW = SamplerInitializer.AddressU;
	SamplerInitializer.MipBias = bUseMips ? MIPBIAS_None : MIPBIAS_HigherResolution_13;

	FSamplerStateRHIRef NewSamplerState = RHICreateSamplerState ( SamplerInitializer );
	check ( IsValidRef ( NewSamplerState ) );
	SamplerStates[SamplerStateIndex] = NewSamplerState;

	return NewSamplerState;
}

void ShaderInterface::SetTexture ( Shader InShader, unsigned InStage, Texture* InTexture, ImageFillMode InFill )
{
	for ( unsigned plane = 0; plane < InTexture->GetTextureStageCount(); plane++ )
	{
		int stageIndex = InStage + plane;

		// Expected texture uniform does not exist in this shader.
		check ( InShader.pFDesc->TexParams[stageIndex] >= 0 );

		// @todo: Temporary crash work-around until proper solution is implemented: Early-out if there isn't a valid RHI
		if ( !IsValidRef ( InTexture->pTextures[plane].GetRHI() ) )
		{
			return;
		}


#if WITH_MOBILE_RHI
        if ( GUsingMobileRHI )
        {
            RHISetMobileTextureSamplerState( InShader.FSRHI, stageIndex, GetSamplerState ( InFill, InTexture->MipLevels > 1 ),
                                             InTexture->pTextures[plane].GetRHI(), 0, -1, InTexture->MipLevels > 1 ? InTexture->MipLevels : -1 );
        }
        else
#endif
        {
		SetTextureParameter ( InShader.FSRHI, InShader.FS->TexUniforms[InShader.FS->FDesc->TexParams[stageIndex]],
								GetSamplerState ( InFill, InTexture->MipLevels > 1 ), InTexture->pTextures[plane].GetRHI() );
	}
    }
	check ( InStage < 4 );
	Textures[InStage] = InTexture;
}

class VertexBuilder
{
public:
    FVertexDeclarationElementList&   Elements;

    VertexBuilder ( FVertexDeclarationElementList& InElements ) : Elements( InElements ) {}

    void Add ( int i, int Attr, int Count, int Offset )
	{
		EVertexElementType Vet = ::VET_None;
		EVertexElementUsage Usage = VEU_Tangent;
        int UsageIndex = ( Attr & VET_Index_Mask ) >> VET_Index_Shift;

        switch ( ( Attr & VET_CompType_Mask ) | Count )
		{
#if XBOX
			case VET_U8  | 1:
            Offset = Offset - 3;
#else
			case VET_U8  | 1:
#endif
			case VET_U8  | 2:
			case VET_U8  | 4:
				Vet = VET_UByte4;
				break;
#if XBOX
			case VET_U8N | 1:
            Offset = Offset - 3;
#else
			case VET_U8N | 1:
#endif
			case VET_U8N | 2:
			case VET_U8N | 4:
				Vet = VET_UByte4N;
				break;
			case VET_S16 | 2:
				Vet = VET_Short2;
				break;
			case VET_F32 | 2:
				Vet = VET_Float2;
				break;
			case VET_F32 | 4:
				Vet = VET_Float4;
				break;
			case VET_U32 | 1:
				Vet = ::VET_Color;
				break;
			default:
				SF_ASSERT ( 0 );
				break;
		}

        switch ( Attr & VET_Usage_Mask )
		{
			case VET_Pos:
				Usage = VEU_Position;
				break;
			case VET_Color:
				Usage = VEU_Color;
#if XBOX
            if ( UsageIndex > 1 )
            {
                UsageIndex = 1;
            }
#endif
				break;
			case VET_TexCoord:
				Usage = VEU_TextureCoordinate;
				break;
			case VET_Instance:
				Usage = VEU_BlendIndices;
				break;
		}

        Elements.AddItem ( FVertexElement ( 0, Offset, Vet, Usage, UsageIndex ) );
		}
    void Finish(int)
		{
		}
};


SysVertexFormat::SysVertexFormat ( const VertexFormat* pvf )
{
    VertexBuilder Builder (Elements);
    BuildVertexArray( pvf, Builder );

	appMemset ( Strides, 0, sizeof ( Strides ) );
	Strides[0] = pvf->Size;

	VDeclRHI = RHICreateVertexDeclaration ( Builder.Elements );
}

SysVertexFormat::SysVertexFormat ( const SysVertexFormat* InSingleVF )
	: Elements ( InSingleVF->Elements )
{
	appMemcpy ( Strides, InSingleVF->Strides, sizeof ( Strides ) );

	VDeclRHI = RHICreateVertexDeclaration ( Elements );
}

void    ShaderManager::MapVertexFormat ( PrimitiveFillType fill, const VertexFormat* sourceFormat,
		const VertexFormat** single, const VertexFormat** batch, const VertexFormat** instanced )
{
	unsigned             fillflags = 0;
	FShaderType          shader = this->StaticShaderForFill ( fill, fillflags, 0 );
	const VertexShaderDesc*   pshader = VertexShaderDesc::Descs[FragShaderDesc::VShaderForFShader[shader]];
	const VertexAttrDesc*     psvf = pshader->Attributes;
	const unsigned       maxVertexElements = 8;
	VertexElement        outf[maxVertexElements];
	unsigned             size = 0;
	int                  j = 0;
    int                  batchOffset = -1, batchElement = -1;

	for ( int i = 0; i < pshader->NumAttribs; i++ )
	{
		if ( ( psvf[i].Attr & ( VET_Usage_Mask | VET_Index_Mask | VET_Components_Mask ) ) == ( VET_Color | ( 1 << VET_Index_Shift ) | 4 ) )
		{
			// XXX - change shaders to use .rg instead of .ra for these
			outf[j + 0].Offset = size;
			outf[j + 1].Offset = size + 3;
#if XBOX
				outf[j + 1].Attribute = VET_T0Weight8;
				outf[j + 0].Attribute = VET_FactorAlpha8;
            batchOffset = size+1;
            batchElement = j+1;
#else
				outf[j + 0].Attribute = VET_T0Weight8;
				outf[j + 1].Attribute = VET_FactorAlpha8;
            batchOffset = size+2;
            batchElement = j+1;
#endif
			j += 2;
			size += 4;
			continue;
		}

		const VertexElement* pv = sourceFormat->GetElement ( psvf[i].Attr & ( VET_Usage_Mask | VET_Index_Mask ), VET_Usage_Mask | VET_Index_Mask );
		if ( !pv )
		{
			*batch = *single = *instanced = NULL;
			return;
		}

#if XBOX
			// Adjust offset so that 1 byte attributes will be swapped into the right place
			// 2-3 byte attributes will not work
			if ( pv->Size() == 1 )
			{
				outf[j] = *pv;
				outf[j].Offset = size + 3;
				j++;
				size += 4;
				continue;
			}
#endif

		outf[j] = *pv;
		outf[j].Offset = size;

#if !XBOX
#if _WINDOWS
		if ( GRHIShaderPlatform == SP_PCOGL || GUsingMobileRHI || GRHIShaderPlatform == SP_PCD3D_SM4 || GRHIShaderPlatform == SP_PCD3D_SM5)
#endif
		{
			if ( ( pv->Attribute & ( VET_Usage_Mask | VET_CompType_Mask | VET_Components_Mask ) ) == VET_ColorARGB8 )
			{
				outf[j].Attribute = VET_ColorRGBA8 | ( pv->Attribute & ~ ( VET_Usage_Mask | VET_CompType_Mask | VET_Components_Mask ) );
			}
		}
#if _WINDOWS
	if ( GRHIShaderPlatform == SP_PCD3D_SM4 || GRHIShaderPlatform == SP_PCD3D_SM5 )
	{
		if ( ( outf[i].Attribute & VET_CompType_Mask ) == VET_S16 )
		{
			outf[j].Attribute &= ~VET_CompType_Mask;
			outf[j].Attribute |= VET_F32;
		}
	}
#endif
#endif

		SF_ASSERT ( ( outf[j].Attribute & VET_Components_Mask ) > 0 && ( outf[j].Attribute & VET_Components_Mask ) <= 4 );
		size += outf[j].Size();
		j++;
	}
	outf[j].Attribute = VET_None;
	outf[j].Offset = 0;
	*single = GetVertexFormat ( outf, j + 1, ( size + 3 ) & ~3 );
	*instanced = 0;

    if ( batchOffset >= 0 )
    {
        for (int i = j-1; i >= batchElement; i--)
            outf[i+1] = outf[i];

        outf[batchElement].Attribute = VET_Instance8;
        outf[batchElement].Offset = batchOffset;
    }
    else
    {
	outf[j].Attribute = VET_Instance8;
#if XBOX
		outf[j].Offset = size + 3;
#else
		outf[j].Offset = size;
#endif
	    size += outf[j].Size();
    }
	outf[j + 1].Attribute = VET_None;
	outf[j + 1].Offset = 0;

	if ( ! ( *single )->pSysFormat )
	{
		const_cast<VertexFormat*> ( *single )->pSysFormat = * ( Render::SystemVertexFormat* ) SF_NEW SysVertexFormat ( *single );
	}
	*batch = GetVertexFormat ( outf, j + 2, ( size + 3 ) & ~3 );
	if ( ! ( *batch )->pSysFormat )
	{
		const_cast<VertexFormat*> ( *batch )->pSysFormat = * ( Render::SystemVertexFormat* ) SF_NEW SysVertexFormat ( *batch );
	}

	//const_cast<VertexFormat*>(*instanced)->pSysFormat = (Render::SystemVertexFormat*)
	//    SF_NEW SysVertexFormat((const SysVertexFormat*)(*single)->pSysFormat.GetPtr(), PrimitiveBatch::DP_Instanced);
	*instanced = 0;
}

}
}
}

#endif//WITH_GFx
