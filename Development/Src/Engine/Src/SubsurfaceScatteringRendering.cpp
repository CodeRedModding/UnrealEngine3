/*=============================================================================
	SubsurfaceScatteringRendering.cpp: Subsurface scattering rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

#if !CONSOLE
	/** A vertex shader for subsurface scattering. */
	class FSubsurfaceScatteringVertexShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE(FSubsurfaceScatteringVertexShader,Global);
	public:

		static UBOOL ShouldCache(EShaderPlatform Platform)
		{
			return Platform == SP_PCD3D_SM5;
		}

		FSubsurfaceScatteringVertexShader()	{}
		FSubsurfaceScatteringVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
			FGlobalShader(Initializer)
		{}

		void SetParameters(const FViewInfo& View)
		{}

		virtual UBOOL Serialize(FArchive& Ar)
		{
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
			return bShaderHasOutdatedParameters;
		}
	};

	IMPLEMENT_SHADER_TYPE(,FSubsurfaceScatteringVertexShader,TEXT("SubsurfaceScatteringVertexShader"),TEXT("Main"),SF_Vertex,0,0);

	enum EMSAAShaderFrequency
	{
		MSAASF_NoMSAA,
		MSAASF_PerFragment,
		MSAASF_PerPixel
	};

	/** A pixel shader for subsurface scattering. */
	template<UINT NumSamplePairs,UINT NumRadialStrata,UINT NumAngularStrata,EMSAAShaderFrequency MSAAShaderFrequency>
	class TSubsurfaceScatteringPixelShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE(TSubsurfaceScatteringPixelShader,Global)

		enum { NumSamples = NumSamplePairs * 2 };
		
		checkAtCompileTime(NumSamples == NumRadialStrata * NumAngularStrata,RadialTimesAngleStrataDoesntMatchNumSamples);

	public:

		static UBOOL ShouldCache(EShaderPlatform Platform)
		{
			return Platform == SP_PCD3D_SM5;
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
			OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLE_PAIRS"),*FString::Printf(TEXT("%u"),NumSamplePairs));
			OutEnvironment.Definitions.Set(TEXT("MSAA_ENABLED"),MSAAShaderFrequency != MSAASF_NoMSAA ? TEXT("1") : TEXT("0"));
			OutEnvironment.Definitions.Set(TEXT("PER_FRAGMENT"),MSAAShaderFrequency == MSAASF_PerFragment ? TEXT("1") : TEXT("0"));
		}

		TSubsurfaceScatteringPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
		{
			SceneTextureParameters.Bind(Initializer.ParameterMap);
			SampleDeltaUVsParameter.Bind(Initializer.ParameterMap,TEXT("SampleDeltaUVs"),TRUE);
			ClipToViewScaleXYParameter.Bind(Initializer.ParameterMap,TEXT("ClipToViewScaleXY"),TRUE);
			ViewToClipScaleXYParameter.Bind(Initializer.ParameterMap,TEXT("ViewToClipScaleXY"),TRUE);
			WorldFilterRadiusParameter.Bind(Initializer.ParameterMap,TEXT("WorldFilterRadius"),TRUE);
			SubsurfaceInscatteringTextureParameter.Bind(Initializer.ParameterMap,TEXT("SubsurfaceInscatteringTexture"),TRUE);
			SubsurfaceScatteringAttenuationTextureParameter.Bind(Initializer.ParameterMap,TEXT("SubsurfaceScatteringAttenuationTexture"),TRUE);
			SubsurfaceScatteringAttenuationSurfaceParameter.Bind(Initializer.ParameterMap,TEXT("SubsurfaceScatteringAttenuationSurface"),TRUE);
			RandomAngleTextureParameter.Bind(Initializer.ParameterMap,TEXT("RandomAngleTexture"),TRUE);
			NoiseScaleAndOffsetParameter.Bind(Initializer.ParameterMap,TEXT("NoiseScaleAndOffset"),TRUE);
		}

		TSubsurfaceScatteringPixelShader()
		{
		}

		void SetParameters(const FSceneView& View)
		{
			SceneTextureParameters.Set(&View,this,SF_Point);

			FRandomStream Random;

			// Create sample points in a stratified disc.
			// Note that it divides the disc's radius equally between strata, so the samples will not be uniformly distributed.
			// This corresponds to a lower sampling density far from the sample origin.
			static const FLOAT StrataAngle = 2.0f * PI / NumAngularStrata;
			static const FLOAT StrataRadius = 1.0f / NumRadialStrata;
			FVector2D SampleDeltaUVs[NumSamples];
			for(UINT RadialStrataIndex = 0;RadialStrataIndex < NumRadialStrata;++RadialStrataIndex)
			{
				for(UINT AngularStrataIndex = 0;AngularStrataIndex < NumAngularStrata;++AngularStrataIndex)
				{
					const UINT SampleIndex = RadialStrataIndex * NumAngularStrata + AngularStrataIndex;

					const FLOAT Angle = (AngularStrataIndex + Random.GetFraction()) * StrataAngle;
					const FLOAT Radius = (RadialStrataIndex + Random.GetFraction()) * StrataRadius;
					SampleDeltaUVs[SampleIndex] = FVector2D(
						appCos(Angle) * Radius,
						appSin(Angle) * Radius
						);
				}
			}
			
			// Set the random normal texture.
			UTexture2D* const RandomAngleTexture = GEngine->RandomAngleTexture;
			SetTextureParameter(
				GetPixelShader(),
				RandomAngleTextureParameter,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				RandomAngleTexture->Resource->TextureRHI
				);

			Random.Initialize(appCycles());
			const FVector4 NoiseScaleAndOffset = FVector4(
				GSceneRenderTargets.GetBufferSizeX() / (FLOAT)RandomAngleTexture->SizeX, 
				GSceneRenderTargets.GetBufferSizeY() / (FLOAT)RandomAngleTexture->SizeY,
				Random.GetFraction(),
				Random.GetFraction());
			SetPixelShaderValue(GetPixelShader(), NoiseScaleAndOffsetParameter, NoiseScaleAndOffset);

			// Pack the samples and put them in constant registers.
			for(UINT SamplePairIndex = 0;SamplePairIndex < NumSamplePairs;++SamplePairIndex)
			{
				const FVector4 PackedSamplePair(
					SampleDeltaUVs[SamplePairIndex * 2 + 0].X,
					SampleDeltaUVs[SamplePairIndex * 2 + 0].Y,
					SampleDeltaUVs[SamplePairIndex * 2 + 1].X,
					SampleDeltaUVs[SamplePairIndex * 2 + 1].Y
					);
				SetPixelShaderValue(GetPixelShader(),SampleDeltaUVsParameter,PackedSamplePair,SamplePairIndex);
			}

			const FVector2D ClipToViewScaleXY(
				1.0f / View.ProjectionMatrix.M[0][0],
				1.0f / View.ProjectionMatrix.M[1][1]
				);
			SetPixelShaderValue(GetPixelShader(),ClipToViewScaleXYParameter,ClipToViewScaleXY);

			const FVector2D ViewToClipScaleXY(
				View.ProjectionMatrix.M[0][0],
				View.ProjectionMatrix.M[1][1]
				);
			SetPixelShaderValue(GetPixelShader(),ViewToClipScaleXYParameter,ViewToClipScaleXY);

			SetTextureParameter(GetPixelShader(),SubsurfaceInscatteringTextureParameter,TStaticSamplerState<>::GetRHI(),GSceneRenderTargets.GetSubsurfaceInscatteringTexture());
			SetTextureParameter(GetPixelShader(),SubsurfaceScatteringAttenuationTextureParameter,TStaticSamplerState<>::GetRHI(),GSceneRenderTargets.GetSubsurfaceScatteringAttenuationTexture());
			
			SetSurfaceParameter(
				GetPixelShader(),
				SubsurfaceScatteringAttenuationSurfaceParameter,
				GSceneRenderTargets.GetSubsurfaceScatteringAttenuationSurface()
				);
		}

		virtual UBOOL Serialize(FArchive& Ar)
		{		
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
			Ar << SceneTextureParameters;
			Ar << SampleDeltaUVsParameter;
			Ar << ClipToViewScaleXYParameter;
			Ar << ViewToClipScaleXYParameter;
			Ar << WorldFilterRadiusParameter;
			Ar << SubsurfaceInscatteringTextureParameter;
			Ar << SubsurfaceScatteringAttenuationTextureParameter;
			Ar << SubsurfaceScatteringAttenuationSurfaceParameter;
			Ar << RandomAngleTextureParameter;
			Ar << NoiseScaleAndOffsetParameter;
			return bShaderHasOutdatedParameters;
		}

	private:

		FSceneTextureShaderParameters SceneTextureParameters;

		FShaderParameter SampleDeltaUVsParameter;
		FShaderParameter ClipToViewScaleXYParameter;
		FShaderParameter ViewToClipScaleXYParameter;
		FShaderParameter WorldFilterRadiusParameter;

		FShaderResourceParameter SubsurfaceInscatteringTextureParameter;
		FShaderResourceParameter SubsurfaceScatteringAttenuationTextureParameter;
		FShaderResourceParameter SubsurfaceScatteringAttenuationSurfaceParameter;
		
		FShaderResourceParameter RandomAngleTextureParameter;
		FShaderParameter NoiseScaleAndOffsetParameter;
	};

	/**
	 * Subsurface scattering accumulate pixel shader type implementation.
	 * The macro allows changing the number of samples to invalidate the compiled shader by embedding the sample count in the name.
	 */
	#define IMPLEMENT_SUBSURFACE_SCATTERING_SHADER(NumSamplePairs,NumRadialStrata,NumAngularStrata) \
		typedef TSubsurfaceScatteringPixelShader<NumSamplePairs,NumRadialStrata,NumAngularStrata,MSAASF_PerPixel> FPerPixelSubsurfaceScatteringPixelShader##NumSamplePairs; \
		typedef FPerPixelSubsurfaceScatteringPixelShader##NumSamplePairs FPerPixelSubsurfaceScatteringPixelShader; \
		IMPLEMENT_SHADER_TYPE(template<>,FPerPixelSubsurfaceScatteringPixelShader##NumSamplePairs,TEXT("SubsurfaceScatteringPixelShader"),TEXT("Main"),SF_Pixel,0,0); \
		typedef TSubsurfaceScatteringPixelShader<NumSamplePairs,NumRadialStrata,NumAngularStrata,MSAASF_PerFragment> FPerFragmentSubsurfaceScatteringPixelShader##NumSamplePairs; \
		typedef FPerFragmentSubsurfaceScatteringPixelShader##NumSamplePairs FPerFragmentSubsurfaceScatteringPixelShader; \
		IMPLEMENT_SHADER_TYPE(template<>,FPerFragmentSubsurfaceScatteringPixelShader##NumSamplePairs,TEXT("SubsurfaceScatteringPixelShader"),TEXT("Main"),SF_Pixel,0,0); \
		typedef TSubsurfaceScatteringPixelShader<NumSamplePairs,NumRadialStrata,NumAngularStrata,MSAASF_NoMSAA> FSubsurfaceScatteringPixelShader##NumSamplePairs; \
		typedef FSubsurfaceScatteringPixelShader##NumSamplePairs FSubsurfaceScatteringPixelShader; \
		IMPLEMENT_SHADER_TYPE(template<>,FSubsurfaceScatteringPixelShader##NumSamplePairs,TEXT("SubsurfaceScatteringPixelShader"),TEXT("Main"),SF_Pixel,0,0);
		
	IMPLEMENT_SUBSURFACE_SCATTERING_SHADER(9,3,6);
	#undef IMPLEMENT_SUBSURFACE_SCATTERING_SHADER

	/** The subsurface scattering vertex declaration resource type. */
	class FSubsurfaceScatteringVertexDeclaration : public FRenderResource
	{
	public:
		FVertexDeclarationRHIRef VertexDeclarationRHI;

		// Destructor
		virtual ~FSubsurfaceScatteringVertexDeclaration() {}

		virtual void InitRHI()
		{
			FVertexDeclarationElementList Elements;
			Elements.AddItem(FVertexElement(0,0,VET_Float2,VEU_Position,0));
			VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
		}

		virtual void ReleaseRHI()
		{
			VertexDeclarationRHI.SafeRelease();
		}
	};

	/** Vertex declaration for the light function fullscreen 2D quad. */
	TGlobalResource<FSubsurfaceScatteringVertexDeclaration> GSubsurfaceScatteringVertexDeclaration;

	/** The bound shader state for the subsurface scattering shaders without MSAA. */
	FGlobalBoundShaderState GSubsurfaceScatteringBoundShaderStateNoMSAA;

	/** The bound shader state for the per-pixel subsurface scattering shaders. */
	FGlobalBoundShaderState GPerPixelSubsurfaceScatteringBoundShaderState;

	/** The bound shader state for the per-sample subsurface scattering shaders. */
	FGlobalBoundShaderState GPerFragmentSubsurfaceScatteringBoundShaderState;
#endif

UBOOL FSceneRenderer::RenderSubsurfaceScattering(UINT DPGIndex)
{
	#if !CONSOLE
		if (DPGIndex == SDPG_World // Only allow materials in the world DPG to use SSS for now.
			&& GRHIShaderPlatform == SP_PCD3D_SM5
			&& GSystemSettings.bAllowSubsurfaceScattering)
		{
			GSceneRenderTargets.ResolveSubsurfaceScatteringSurfaces();

			static const FVector2D Vertices[4] =
			{
				FVector2D(-1,-1),
				FVector2D(-1,+1),
				FVector2D(+1,+1),
				FVector2D(+1,-1),
			};
			static const WORD Indices[6] =
			{
				0, 1, 2,
				0, 2, 3
			};

			GSceneRenderTargets.BeginRenderingSceneColor();

			for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				const FViewInfo& View = Views(ViewIndex);

				if(
					( ( View.Family->ShowFlags & SHOW_SubsurfaceScattering ) != 0 )
					&& ( View.Family->ShouldPostProcess() )
				)
				{
					SCOPED_CONDITIONAL_DRAW_EVENT(EventRenderSS,ViewIndex == 0)(DEC_SCENE_ITEMS,TEXT("Subsurface Scattering"));

					// Set the device viewport for the view.
					RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
					RHISetViewParameters(View);
					RHISetMobileHeightFogParams(View.HeightFogParams);

					// No depth or stencil tests, no backface culling.
					RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
					RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
					RHISetStencilState(TStaticStencilState<>::GetRHI());

					// Use additive blending for color, and keep the destination alpha.
					RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());

					if(GSystemSettings.UsesMSAA())
					{
						TShaderMapRef<FSubsurfaceScatteringVertexShader> VertexShader(GetGlobalShaderMap());

						{
							SCOPED_DRAW_EVENT(EventRenderPerSample)(DEC_SCENE_ITEMS,TEXT("PerSample SSS"));

							// Clear the stencil buffer to zero.
							RHIClear(FALSE,FLinearColor(),FALSE,0,TRUE,0);

							// Set stencil to one.
							RHISetStencilState(TStaticStencilState<
								TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
								FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
								0xff,0xff,1
								>::GetRHI());

							// Set the per-pixel subsurface scattering shaders.
							TShaderMapRef<FPerPixelSubsurfaceScatteringPixelShader> PerPixelPixelShader(GetGlobalShaderMap());
							SetGlobalBoundShaderState(
								GPerPixelSubsurfaceScatteringBoundShaderState,
								GSubsurfaceScatteringVertexDeclaration.VertexDeclarationRHI,
								*VertexShader,
								*PerPixelPixelShader,
								sizeof(FVector2D)
								);
							VertexShader->SetParameters(View);
							PerPixelPixelShader->SetParameters(View);

							// Draw a quad covering the view.
							RHIDrawIndexedPrimitiveUP(
								PT_TriangleList,
								0,
								ARRAY_COUNT(Vertices),
								2,
								Indices,
								sizeof(Indices[0]),
								Vertices,
								sizeof(Vertices[0])
								);
						}

						// Set the per-sample subsurface scattering shaders.
						TShaderMapRef<FPerFragmentSubsurfaceScatteringPixelShader> PerFragmentPixelShader(GetGlobalShaderMap());
						SetGlobalBoundShaderState(
							GPerFragmentSubsurfaceScatteringBoundShaderState,
							GSubsurfaceScatteringVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PerFragmentPixelShader,
							sizeof(FVector2D)
							);
						VertexShader->SetParameters(View);
						PerFragmentPixelShader->SetParameters(View);

						// Pass if stencil=0
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0,0
							>::GetRHI());

						// Draw a quad covering the view.
						RHIDrawIndexedPrimitiveUP(
							PT_TriangleList,
							0,
							ARRAY_COUNT(Vertices),
							2,
							Indices,
							sizeof(Indices[0]),
							Vertices,
							sizeof(Vertices[0])
							);
							
						// Restore default stencil state
						RHISetStencilState(TStaticStencilState<>::GetRHI());
					}
					else
					{
						// Set the non-MSAA subsurface scattering shaders.
						TShaderMapRef<FSubsurfaceScatteringVertexShader> VertexShader(GetGlobalShaderMap());
						TShaderMapRef<FSubsurfaceScatteringPixelShader> PixelShader(GetGlobalShaderMap());
						SetGlobalBoundShaderState(
							GSubsurfaceScatteringBoundShaderStateNoMSAA,
							GSubsurfaceScatteringVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FVector2D)
							);
						VertexShader->SetParameters(View);
						PixelShader->SetParameters(View);

						// Draw a quad covering the view.
						RHIDrawIndexedPrimitiveUP(
							PT_TriangleList,
							0,
							ARRAY_COUNT(Vertices),
							2,
							Indices,
							sizeof(Indices[0]),
							Vertices,
							sizeof(Vertices[0])
							);
					}
				}
			}
			return TRUE;
		}
	#endif

	return FALSE;
}
