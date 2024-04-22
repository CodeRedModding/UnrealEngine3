/*=============================================================================
LUTBlender.cpp: LUT Blender for efficient Color Grading (LUT: color look up table, RGB_new = LUT[RGB_old]) blender.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "SceneFilterRendering.h"
#include "ScenePrivate.h"

// including the neutral one at index 0
const UINT GMaxLUTBlendCount = 5;


FLUTBlender::FLUTBlender()
{
	// required as we might be in unreal script
	appMemzero(this, sizeof(FLUTBlender));

	bHasChanged = TRUE;
}

UBOOL FLUTBlender::IsLUTEmpty() const
{
	check(LUTTextures.Num() == LUTWeights.Num());

	return LUTTextures.Num() == 0;
}

/** add a LUT(look up table) to the ones that are blended together */
void FLUTBlender::PushLUT(UTexture* Texture, FLOAT Weight)
{
	check(Weight >= 0.0f && Weight <= 1.0f);

	LUTTextures.AddItem(Texture);
	LUTWeights.AddItem(Weight);
}

/** @return 0xffffffff if not found */
UINT FLUTBlender::FindIndex(UTexture* Tex) const
{
	for(UINT i = 0; i < (UINT)LUTTextures.Num(); ++i)
	{
		if(LUTTextures(i) == Tex)
		{
			return i;
		}
	}

	return 0xffffffff;
}


void FLUTBlender::SetLUT(UTexture* Texture)
{
	// intentionally no deallocations
	LUTTextures.Empty();
	LUTWeights.Empty();
	
	PushLUT(Texture, 1.0f);
}

/** new = lerp(old, Rhs, Weight) 
*
* @param Texture can be 0 then the call is ignored
* @param Weight 0..1
*/
void FLUTBlender::LerpTo(UTexture* InTexture, FLOAT Weight)
{
	// call Reset() before using LerpTo()
	check(!IsLUTEmpty());

	check(Weight >= 0 && Weight <= 1.0f);
	check(LUTTextures.Num() == LUTWeights.Num());

	if(Weight > 254.0f/255.0f || !LUTTextures.Num())
	{
		SetLUT(InTexture);
		return;
	}

	for(UINT i = 0; i < (UINT)LUTTextures.Num(); ++i)
	{
		LUTWeights(i) *= 1.0f - Weight;
	}

	UINT ExistingIndex = FindIndex(InTexture);
	if(ExistingIndex == 0xffffffff)
	{
		PushLUT(InTexture, Weight);
	}
	else
	{
		LUTWeights(ExistingIndex) += Weight;
	}
}

/**
* A vertex shader for rendering a textured screen element.
*/
class FLUTBlenderVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FLUTBlenderVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FLUTBlenderVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FShader(Initializer)
	{
	}
	FLUTBlenderVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		return FShader::Serialize(Ar);
	}
};

/**
* A pixel shader for blending multiple LUT to one
*
* @param BlendCount >0
*/
template<UINT BlendCount>
class FLUTBlenderPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FLUTBlenderPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FLUTBlenderPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
		,	GammaParameters(Initializer.ParameterMap)
		,	MaterialParameters(Initializer.ParameterMap)
	{
		// starts as 1 as 0 is the neutral one
		for(UINT i = 1; i < BlendCount; ++i)
		{
			FString Name = FString::Printf(TEXT("Texture%d"), i);

			TextureParameter[i].Bind(Initializer.ParameterMap, *Name, TRUE);
		}
		WeightsParameter.Bind(Initializer.ParameterMap, TEXT("LUTWeights"), TRUE);
	}
	FLUTBlenderPixelShader() {}

	void SetParameters(FTexture* Texture[BlendCount], FLOAT Weights[BlendCount], FViewInfo& View, const struct ColorTransformMaterialProperties& ColorTransform)
	{
		for(UINT i = 0; i < BlendCount; ++i)
		{
			// we don't need to set the neutral one
			if(i != 0)
			{
				SetTextureParameterDirectly(GetPixelShader(), TextureParameter[i], Texture[i]);
			}

			// can be optimized?
			SetPixelShaderValue(GetPixelShader(), WeightsParameter, Weights[i], i);
		}

		FLOAT DisplayGamma = View.Family->RenderTarget->GetDisplayGamma();

		if( !View.Family->bResolveScene )
		{
			// disable gamma correction (1.0 transforms from gamma to linear)
			DisplayGamma = 1.0f;
		}

		// Forcibly disable gamma correction if mobile emulation is enabled
		if( GEmulateMobileRendering && !GUseGammaCorrectionForMobileEmulation )
		{
			DisplayGamma = 1.0f;
		}

		// Set the gamma correction parameters
		GammaParameters.Set(
			this,
			DisplayGamma,
			View.ColorScale,
			View.OverlayColor);

		// Set the material colorization parameters
		MaterialParameters.Set(
			this,
			ColorTransform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		TCHAR StrCount[2];
		
		StrCount[0] = '0' + BlendCount;
		StrCount[1] = 0;

		OutEnvironment.Definitions.Set(TEXT("BLENDCOUNT"), StrCount);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);

		for(UINT i = 0; i < BlendCount; ++i)
		{
			Ar << TextureParameter[i];
		}
		Ar << WeightsParameter;
		Ar << GammaParameters;
		Ar << MaterialParameters;

		WeightsParameter.SetShaderParamName(TEXT("LUTWeights"));

		return bShaderHasOutdatedParameters;
	}

private: // ---------------------------------------------------

	// [0] is not used as it's the neutral one we do in the shader
	FShaderResourceParameter		TextureParameter[GMaxLUTBlendCount];
	//
	FShaderParameter				WeightsParameter;
	// to integrate the procedural color correction into the pass with fewer pixels (speeds up postprocessing)
	FGammaShaderParameters			GammaParameters;
	FColorRemapShaderParameters     MaterialParameters;
};


IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPixelShader<1>,TEXT("LUTBlender"),TEXT("MainPS"),SF_Pixel,VER_UBERPOST_REFACTOR2,0);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPixelShader<2>,TEXT("LUTBlender"),TEXT("MainPS"),SF_Pixel,VER_UBERPOST_REFACTOR2,0);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPixelShader<3>,TEXT("LUTBlender"),TEXT("MainPS"),SF_Pixel,VER_UBERPOST_REFACTOR2,0);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPixelShader<4>,TEXT("LUTBlender"),TEXT("MainPS"),SF_Pixel,VER_UBERPOST_REFACTOR2,0);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPixelShader<5>,TEXT("LUTBlender"),TEXT("MainPS"),SF_Pixel,VER_UBERPOST_REFACTOR2,0);
IMPLEMENT_SHADER_TYPE(,FLUTBlenderVertexShader,TEXT("LUTBlender"),TEXT("MainVS"),SF_Vertex,0,0);

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

void SetLUTBlenderShader(UINT BlendCount, FTexture* Texture[], FLOAT Weights[], FViewInfo& View, const struct ColorTransformMaterialProperties& ColorTransform)
{
	check(BlendCount > 0);
	TShaderMapRef<FLUTBlenderVertexShader> VertexShader(GetGlobalShaderMap());

	// A macro to handle setting the filter shader for a specific number of samples.
#define CASE_COUNT(BlendCount) \
case BlendCount: \
	{ \
		TShaderMapRef<FLUTBlenderPixelShader<BlendCount> > PixelShader(GetGlobalShaderMap()); \
		PixelShader->SetParameters(Texture, Weights, View, ColorTransform); \
		static FGlobalBoundShaderState BoundShaderState; \
		SetGlobalBoundShaderState( BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex)); \
	}; \
	break;

	switch(BlendCount)
	{
		// starts at 1 as we always have at least the neutral one
		CASE_COUNT(1);
		CASE_COUNT(2);
		CASE_COUNT(3);
		CASE_COUNT(4);
		CASE_COUNT(5);
//	default:
//		appErrorf(TEXT("Invalid number of samples: %u"),BlendCount);
	}
#undef CASE_COUNT

}

// is updated every frame if GColorGrading is set to debug mode
static FString GFLUTBlenderDebug;

UINT FLUTBlender::GenerateFinalTable(FTexture* OutTextures[], FLOAT OutWeights[], UINT MaxCount) const
{
	// find the n strongest contributors, drop small contributors
	// (inefficient implementation for many items but count should be small)

	UINT LocalCount = 1;

	// add the neutral one (done in the shader) as it should be the first and always there
	OutTextures[0] = 0;
	{
		UINT NeutralIndex = FindIndex(0);

		OutWeights[0] = NeutralIndex == 0xffffffff ? 0.0f : LUTWeights(NeutralIndex);
	}

	if(GColorGrading == 2)
	{
		// "LUT"=off, but ColorTransform still visible 
		MaxCount = 1;
	}

	FLOAT OutWeightsSum = OutWeights[0];
	for(; LocalCount < MaxCount; ++LocalCount)
	{
		UINT BestIndex = 0xffffffff;
		// find the one with the strongest weight, add until full
		for(UINT i = 0; i < (UINT)LUTTextures.Num(); ++i)
		{
			UBOOL AlreadyInArray = FALSE;
			{
				UTexture* LUTTexture = LUTTextures(i); 
				FTexture* Internal = LUTTexture ? LUTTexture->Resource : 0; 
				for(UINT e = 0; e < LocalCount; ++e)
				{
					if(Internal == OutTextures[e])
					{
						AlreadyInArray = TRUE;
						break;
					}
				}
			}

			if(AlreadyInArray)
			{
				// we already have this one
				continue;
			}

			if(BestIndex != 0xffffffff
			&& LUTWeights(BestIndex) > LUTWeights(i))
			{
				// we have a better ones, maybe add next time
				continue;
			}

			BestIndex = i;
		}

		if(BestIndex == 0xffffffff)
		{
			// no more elements to process 
			break;
		}

		FLOAT BestWeight = LUTWeights(BestIndex);

		if(BestWeight < 1.0f / 512.0f)
		{
			// drop small contributor 
			break;
		}

		UTexture* BestLUTTexture = LUTTextures(BestIndex); 
		FTexture* BestInternal = BestLUTTexture ? BestLUTTexture->Resource : 0; 

		OutTextures[LocalCount] = BestInternal;
		OutWeights[LocalCount] = BestWeight;
		OutWeightsSum += BestWeight;
	}

	// normalize
	if(OutWeightsSum > 0.001f)
	{
		FLOAT InvOutWeightsSum = 1.0f / OutWeightsSum;

		for(UINT i = 0; i < LocalCount; ++i)
		{
			OutWeights[i] *= InvOutWeightsSum;
		}
	}
	else
	{
		// neutral only is fully utilized
		OutWeights[0] = 1.0f;
		LocalCount = 1;
	}
	
	return LocalCount;
}

/** resolve to one LUT (look up table) */
const FTextureRHIRef FLUTBlender::ResolveLUT(FViewInfo& View, const struct ColorTransformMaterialProperties& ColorTransform)
{
	if ( HasChanged() )
	{
		FTexture* LocalTextures[GMaxLUTBlendCount];
		FLOAT LocalWeights[GMaxLUTBlendCount];

		UINT LocalCount = GenerateFinalTable(LocalTextures, LocalWeights, GMaxLUTBlendCount);

		if(LocalCount)
		{
			SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("LUTBlender"));

			RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
			RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
			RHISetBlendState(TStaticBlendState<>::GetRHI());

			GSceneRenderTargets.BeginRenderingLUTBlend();

			SetLUTBlenderShader(LocalCount, LocalTextures, LocalWeights, View, ColorTransform);

#if XBOX
			const UINT Scale = 2;		// SourceX needs to be 32 pixel aligned as this is a Xbox360 Resolve() requirement
/*			// Xbox360 uses 16 slices
			for(UINT Slice = 0; Slice < 16; ++Slice)
			{
				DrawDenormalizedQuad(
					0, 16 * Slice * Scale,	// XY
					16, 16,					// SizeXY
					0, 16 * Slice,			// UV
					16, 16,					// SizeUV
					16, 16 * 16 * Scale,	// TargetSize
					16, 16 * 16				// TextureSize
					);
			}
			*/
			DrawDenormalizedQuad( 
				0, 0,						// XY
				16, 16 * 16 * Scale,		// SizeXY
				0, 0,						// UV
				16, 16 * 16 * Scale,		// SizeUV
				16, 16 * 16 * Scale,		// TargetSize
				16, 16 * 16 * Scale			// TextureSize
				); 
#else // XBOX
			// other platforms use unwrapped 2d texture
			DrawDenormalizedQuad( 
				0, 0,						// XY
				16 * 16, 16,				// SizeXY
				0, 0,						// UV
				16 * 16, 16,				// SizeUV
				16 * 16, 16,				// TargetSize
				16 * 16, 16					// TextureSize
				); 
#endif // XBOX

			GSceneRenderTargets.FinishRenderingLUTBlend();
		}
		else
		{
			return NULL;
		}
	}
	return GSceneRenderTargets.GetLUTBlendTexture();
}

/** which LUT blend and at which state they are */
UBOOL FLUTBlender::GetDebugInfo(FString &Out)
{
	if(GColorGrading >= 0
	|| !GFLUTBlenderDebug.Len())
	{
		return FALSE;
	}

	check(!Out.Len());

	Out = GFLUTBlenderDebug;

	GFLUTBlenderDebug = TEXT("LUTBlender: not used this frame");

	return TRUE;
}

/**
* Clean the container and adds the neutral LUT. 
* should be called after the render thread copied the data
*/
void FLUTBlender::Reset()
{
	LUTTextures.Reset();
	LUTWeights.Reset();
	SetLUT(0);
}

void FLUTBlender::CopyToRenderThread(FLUTBlender &Dest) const
{
	Dest = *this;

	if(GColorGrading == -1)
	{
		// debug color grading, show incoming to LUTBlender
		GFLUTBlenderDebug = FString::Printf(TEXT("ColorGrading: LUTBlender input(%d): "), LUTTextures.Num());

		for(UINT i = 0; i < (UINT)LUTTextures.Num(); ++i)
		{
			if(GFLUTBlenderDebug.Len())
			{
				GFLUTBlenderDebug += TEXT("    ");
			}

			UTexture* LUTTexture = LUTTextures(i); 
			FTexture* Internal = LUTTexture ? LUTTexture->Resource : 0; 

			GFLUTBlenderDebug += FString::Printf(TEXT("%s:%d%%"), 
				Internal ? *(Internal->GetFriendlyName()) : TEXT("Neutral(Code)"),
				(UINT)(LUTWeights(i) * 100 + 0.5f));
		}
	}
	else if(GColorGrading == -2)
	{
		// debug color grading, show outgoing from LUTBlender
		FTexture* LocalTextures[GMaxLUTBlendCount];
		FLOAT LocalWeights[GMaxLUTBlendCount];

		UINT LocalCount = GenerateFinalTable(LocalTextures, LocalWeights, GMaxLUTBlendCount);

		GFLUTBlenderDebug = FString::Printf(TEXT("ColorGrading: LUTBlender output(%d/%d): "), LocalCount, GMaxLUTBlendCount);

		for(UINT i = 0; i < LocalCount; ++i)
		{
			if(GFLUTBlenderDebug.Len())
			{
				GFLUTBlenderDebug += TEXT("    ");
			}
			GFLUTBlenderDebug += FString::Printf(TEXT("%s:%d%%"), 
				LocalTextures[i] ? *(LocalTextures[i]->GetFriendlyName()) : TEXT("Neutral(Code)"),
				(UINT)(LocalWeights[i] * 100 + 0.5f));
		}
	}
}

/**
 * Check if the parameters are different, compared to the previous LUT Blender parameters.
 */
void FLUTBlender::CheckForChanges( const FLUTBlender& PreviousLUTBlender )
{
	if ( LUTTextures.Num() != PreviousLUTBlender.LUTTextures.Num() || LUTWeights.Num() != PreviousLUTBlender.LUTWeights.Num() )
	{
		bHasChanged = TRUE;
		return;
	}

	for ( INT TextureIndex=0; TextureIndex < LUTTextures.Num(); ++TextureIndex )
	{
		if ( LUTTextures(TextureIndex) != PreviousLUTBlender.LUTTextures(TextureIndex) )
		{
			bHasChanged = TRUE;
			return;
		}
	}

	for ( INT WeightIndex=0; WeightIndex < LUTWeights.Num(); ++WeightIndex )
	{
		if ( LUTWeights(WeightIndex) != PreviousLUTBlender.LUTWeights(WeightIndex) )
		{
			bHasChanged = TRUE;
			return;
		}
	}
	bHasChanged = FALSE;
}
