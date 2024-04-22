/*=============================================================================
	FogRendering.cpp: Fog rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "AmbientOcclusionRendering.h"

/** Binds the parameters. */
void FExponentialHeightFogShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	ExponentialFogParameters.Bind(ParameterMap,TEXT("SharedFogParameter0"), TRUE);
	ExponentialFogColorParameter.Bind(ParameterMap,TEXT("SharedFogParameter1"), TRUE);
	LightInscatteringColorParameter.Bind(ParameterMap,TEXT("SharedFogParameter2"), TRUE);
	LightVectorParameter.Bind(ParameterMap,TEXT("SharedFogParameter3"), TRUE);
}

/** Serializer. */
FArchive& operator<<(FArchive& Ar,FExponentialHeightFogShaderParameters& Parameters)
{
	Ar << Parameters.ExponentialFogParameters;
	Ar << Parameters.ExponentialFogColorParameter;
	Ar << Parameters.LightInscatteringColorParameter;
	Ar << Parameters.LightVectorParameter;
	return Ar;
}

/** Binds the parameters. */
void FHeightFogShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	bUseExponentialHeightFogParameter.Bind(ParameterMap,TEXT("bUseExponentialHeightFog"), TRUE);
	FogMinHeightParameter.Bind(ParameterMap,TEXT("SharedFogParameter3"), TRUE);
	FogMaxHeightParameter.Bind(ParameterMap,TEXT("FogMaxHeight"), TRUE);
	FogDistanceScaleParameter.Bind(ParameterMap,TEXT("SharedFogParameter0"), TRUE);
	FogExtinctionDistanceParameter.Bind(ParameterMap,TEXT("SharedFogParameter1"), TRUE);
	FogInScatteringParameter.Bind(ParameterMap,TEXT("FogInScattering"), TRUE);
	FogStartDistanceParameter.Bind(ParameterMap,TEXT("SharedFogParameter2"), TRUE);
	ExponentialParameters.Bind(ParameterMap);
}

static const FLOAT DefaultFogParameters[4] = { 0.f, 0.f, 0.f, 0.f };
static const FLOAT DefaultFogExtinctionDistance[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
static const FLinearColor DefaultFogInScattering[4] = { FLinearColor::Black, FLinearColor::Black, FLinearColor::Black, FLinearColor::Black };

/** 
* Sets the parameter values, this must be called before rendering the primitive with the shader applied. 
* @param Shader - the shader to set the parameters on
*/
template<typename ShaderRHIParamRef>
void FHeightFogShaderParameters::Set(
	const FVertexFactory* VertexFactory, 
	const FMaterialRenderProxy* MaterialRenderProxy, 
	const FMaterial& Material,
	const FSceneView* View, 
	const UBOOL bAllowGlobalFog, 
	ShaderRHIParamRef Shader) const
{
	const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(View);

	// Set the fog constants.
	//@todo - translucent decals on translucent receivers currently don't handle fog
	if ( bAllowGlobalFog && ( Material.AllowsFog() && !(VertexFactory->IsDecalFactory() && VertexFactory->IsGPUSkinned()) ))
	{
		SetShaderValue(Shader, bUseExponentialHeightFogParameter, ViewInfo->bRenderExponentialFog ? 1.0f : 0.0f);
		
		if (ViewInfo->bRenderExponentialFog)
		{
			SetShaderValue(Shader, ExponentialParameters.ExponentialFogParameters, ViewInfo->ExponentialFogParameters);
			SetShaderValue(Shader, ExponentialParameters.ExponentialFogColorParameter, FVector4(ViewInfo->ExponentialFogColor, 1.0f - ViewInfo->FogMaxOpacity));
			SetShaderValue(Shader, ExponentialParameters.LightInscatteringColorParameter, ViewInfo->LightInscatteringColor);
			SetShaderValue(Shader, ExponentialParameters.LightVectorParameter, ViewInfo->DominantDirectionalLightDirection);
		}
		else
		{
			TStaticArray<FLOAT,4> TranslatedMinHeight;
			TStaticArray<FLOAT,4> TranslatedMaxHeight;
			for(UINT LayerIndex = 0;LayerIndex < 4;++LayerIndex)
			{
				TranslatedMinHeight[LayerIndex] = ViewInfo->HeightFogParams.FogMinHeight[LayerIndex] + View->PreViewTranslation.Z;
				TranslatedMaxHeight[LayerIndex] = ViewInfo->HeightFogParams.FogMaxHeight[LayerIndex] + View->PreViewTranslation.Z;
			}

			SetShaderValue(Shader,FogMinHeightParameter,TranslatedMinHeight);
			SetShaderValue(Shader,FogMaxHeightParameter,TranslatedMaxHeight);
			SetShaderValue(Shader,FogInScatteringParameter,ViewInfo->HeightFogParams.FogInScattering);
			SetShaderValue(Shader,FogDistanceScaleParameter,ViewInfo->HeightFogParams.FogDistanceScale);
			SetShaderValue(Shader,FogExtinctionDistanceParameter,ViewInfo->HeightFogParams.FogExtinctionDistance);
			SetShaderValue(Shader,FogStartDistanceParameter,ViewInfo->HeightFogParams.FogStartDistance);
		}
	}
	else
	{
		// Set the default values which effectively disable vertex fog
		SetShaderValue(Shader,bUseExponentialHeightFogParameter, 0.0f);
		SetShaderValue(Shader,FogMinHeightParameter,DefaultFogParameters);
		SetShaderValue(Shader,FogMaxHeightParameter,DefaultFogParameters);
		SetShaderValue(Shader,FogInScatteringParameter,DefaultFogInScattering);
		SetShaderValue(Shader,FogDistanceScaleParameter,DefaultFogParameters);
		SetShaderValue(Shader,FogExtinctionDistanceParameter,DefaultFogExtinctionDistance);
		SetShaderValue(Shader,FogStartDistanceParameter,DefaultFogParameters);
	}
}

/** 
* Sets the parameter values, this must be called before rendering the primitive with the shader applied. 
* @param VertexShader - the vertex shader to set the parameters on
*/
void FHeightFogShaderParameters::SetVertexShader(
	const FVertexFactory* VertexFactory, 
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FSceneView* View,
	const UBOOL bAllowGlobalFog,
	FShader* VertexShader) const
{
	Set(VertexFactory, MaterialRenderProxy,Material,View,bAllowGlobalFog,VertexShader->GetVertexShader());
}

#if WITH_D3D11_TESSELLATION
/** 
* Sets the parameter values, this must be called before rendering the primitive with the shader applied. 
* @param DomainShader - the vertex shader to set the parameters on
*/
void FHeightFogShaderParameters::SetDomainShader(
	const FVertexFactory* VertexFactory, 
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FSceneView* View,
	FShader* DomainShader) const
{
	const UBOOL bAllowGlobalFog = FALSE;
	Set(VertexFactory, MaterialRenderProxy,Material,View,bAllowGlobalFog,DomainShader->GetDomainShader());
}
#endif

/** Serializer. */
FArchive& operator<<(FArchive& Ar,FHeightFogShaderParameters& Parameters)
{
	Ar << Parameters.bUseExponentialHeightFogParameter;
	Ar << Parameters.FogDistanceScaleParameter;
	Ar << Parameters.FogExtinctionDistanceParameter;
	Ar << Parameters.FogMinHeightParameter;
	Ar << Parameters.FogMaxHeightParameter;
	Ar << Parameters.FogInScatteringParameter;
	Ar << Parameters.FogStartDistanceParameter;
	Ar << Parameters.ExponentialParameters;
	return Ar;
}


/** A vertex shader for rendering height fog. */
template<UINT NumLayers>
class THeightFogVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(THeightFogVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	THeightFogVertexShader( )	{ }
	THeightFogVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ScreenPositionScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("ScreenPositionScaleBias"));
		FogMinHeightParameter.Bind(Initializer.ParameterMap,TEXT("FogMinHeight"));
		FogMaxHeightParameter.Bind(Initializer.ParameterMap,TEXT("FogMaxHeight"));
		ScreenToWorldParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"));
		FogStartZParameter.Bind(Initializer.ParameterMap,TEXT("FogStartZ"), TRUE);
	}

	void SetParameters(const FViewInfo& View)
	{
		// Set the transform from screen coordinates to scene texture coordinates.
		// NOTE: Need to set explicitly, since this is a vertex shader!
		SetVertexShaderValue(GetVertexShader(),ScreenPositionScaleBiasParameter,View.ScreenPositionScaleBias);

		// Set the fog constants.
		SetVertexShaderValue(GetVertexShader(),FogMinHeightParameter,View.HeightFogParams.FogMinHeight);
		SetVertexShaderValue(GetVertexShader(),FogMaxHeightParameter,View.HeightFogParams.FogMaxHeight);

		FMatrix ScreenToWorld = FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) *
			View.InvViewProjectionMatrix;

		// Set the view constants, as many as were bound to the parameter.
		SetVertexShaderValue(GetVertexShader(),ScreenToWorldParameter,ScreenToWorld);

		{
			// The fog can be set to start at a certain euclidean distance.
			// We clamp the value to be behind the near plane z.
			// (not the exact value as a distance that near would not allow any optimization anyway)
			FLOAT FogStartDistance = Max(30.0f, View.ExponentialFogParameters.W);

			// Here we compute the nearest z value the fog can start
			// to render the quad at this z value with depth test enabled.
			// This means with a bigger distance specified more pixels are
			// are culled and don't need to be rendered. This is faster if
			// there is opaque content nearer than the computed z.

			FVector ViewSpaceCorner = View.InvProjectionMatrix.TransformFVector4(FVector4(1, 1, 1, 1));

			FLOAT Ratio = ViewSpaceCorner.Z / ViewSpaceCorner.Size();

			FVector ViewSpaceStartFogPoint(0.0f, 0.0f, FogStartDistance * Ratio);
			FVector4 ClipSpaceMaxDistance = View.ProjectionMatrix.TransformFVector(ViewSpaceStartFogPoint);

			FLOAT FogClipSpaceZ = Max(0.0f, ClipSpaceMaxDistance.Z / ClipSpaceMaxDistance.W);

			SetVertexShaderValue(GetVertexShader(),FogStartZParameter, FogClipSpaceZ);
		}
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ScreenPositionScaleBiasParameter;
		Ar << FogMinHeightParameter;
		Ar << FogMaxHeightParameter;
		Ar << ScreenToWorldParameter;
		Ar << FogStartZParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ScreenPositionScaleBiasParameter;
	FShaderParameter FogMinHeightParameter;
	FShaderParameter FogMaxHeightParameter;
	FShaderParameter ScreenToWorldParameter;
	FShaderParameter FogStartZParameter;
};

IMPLEMENT_SHADER_TYPE(template<>,THeightFogVertexShader<1>,TEXT("HeightFogVertexShader"),TEXT("OneLayerMain"),SF_Vertex,0,0);
IMPLEMENT_SHADER_TYPE(template<>,THeightFogVertexShader<4>,TEXT("HeightFogVertexShader"),TEXT("FourLayerMain"),SF_Vertex,0,0);

enum EMSAAShaderFrequency
{
	MSAASF_NoMSAA = 0,
	MSAASF_PerFragment,
	MSAASF_PerPixel,
	MSAASF_Num
};

/** A pixel shader for rendering exponential height fog. */
template<EMSAAShaderFrequency MSAAShaderFrequency>
class TExponentialHeightFogPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TExponentialHeightFogPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return MSAAShaderFrequency == MSAASF_NoMSAA || Platform == SP_PCD3D_SM5;
	}

	/**
	* Add any compiler flags/defines required by the shader
	* @param OutEnvironment - shader environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("MSAA_ENABLED"),MSAAShaderFrequency != MSAASF_NoMSAA ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("PER_FRAGMENT"),MSAAShaderFrequency == MSAASF_PerFragment ? TEXT("1") : TEXT("0"));
	}

	TExponentialHeightFogPixelShader( )	{ }
	TExponentialHeightFogPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		CameraWorldPositionParameter.Bind(Initializer.ParameterMap,TEXT("CameraWorldPosition"), TRUE);
		ExponentialParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FViewInfo& View)
	{
		SceneTextureParameters.Set(&View, this);

		SetPixelShaderValue(GetPixelShader(), CameraWorldPositionParameter, (FVector)View.ViewOrigin);

		SetPixelShaderValue(GetPixelShader(), ExponentialParameters.ExponentialFogParameters, View.ExponentialFogParameters);
		SetPixelShaderValue(GetPixelShader(), ExponentialParameters.ExponentialFogColorParameter, FVector4(View.ExponentialFogColor, 1.0f - View.FogMaxOpacity));
		SetPixelShaderValue(GetPixelShader(), ExponentialParameters.LightInscatteringColorParameter, View.LightInscatteringColor);
		SetPixelShaderValue(GetPixelShader(), ExponentialParameters.LightVectorParameter, View.DominantDirectionalLightDirection);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << CameraWorldPositionParameter;
		Ar << ExponentialParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter CameraWorldPositionParameter;
	FExponentialHeightFogShaderParameters ExponentialParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,TExponentialHeightFogPixelShader<MSAASF_NoMSAA>,TEXT("HeightFogPixelShader"), TEXT("ExponentialPixelMain"),SF_Pixel,0,0)
IMPLEMENT_SHADER_TYPE(template<>,TExponentialHeightFogPixelShader<MSAASF_PerPixel>,TEXT("HeightFogPixelShader"),TEXT("ExponentialPixelMain"),SF_Pixel,0,0)
IMPLEMENT_SHADER_TYPE(template<>,TExponentialHeightFogPixelShader<MSAASF_PerFragment>,TEXT("HeightFogPixelShader"),TEXT("ExponentialPixelMain"),SF_Pixel,0,0)

/** A pixel shader for rendering height fog. */
template<UINT NumLayers,EMSAAShaderFrequency MSAAShaderFrequency>
class THeightFogPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(THeightFogPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		// Only compile the downsampled version (NumLayers == 0) for xbox
		return (NumLayers != 0 || Platform == SP_XBOXD3D) && (MSAAShaderFrequency == MSAASF_NoMSAA || Platform == SP_PCD3D_SM5);
	}

	/**
	* Add any compiler flags/defines required by the shader
	* @param OutEnvironment - shader environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		//The HLSL compiler for xenon will not always use predicated instructions without this flag.  
		//On PC the compiler consistently makes the right decision.
		new(OutEnvironment.CompilerFlags) ECompilerFlags(CFLAG_PreferFlowControl);
		if( Platform == SP_XBOXD3D )
		{
			//The xenon compiler complains about the [ifAny] attribute
			new(OutEnvironment.CompilerFlags) ECompilerFlags(CFLAG_SkipValidation);
		}

		OutEnvironment.Definitions.Set(TEXT("MSAA_ENABLED"),MSAAShaderFrequency != MSAASF_NoMSAA ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("PER_FRAGMENT"),MSAAShaderFrequency == MSAASF_PerFragment ? TEXT("1") : TEXT("0"));
	}

	THeightFogPixelShader( )	{ }
	THeightFogPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		FogDistanceScaleParameter.Bind(Initializer.ParameterMap,TEXT("SharedFogParameter0"));
		FogExtinctionDistanceParameter.Bind(Initializer.ParameterMap,TEXT("SharedFogParameter1"));
		FogInScatteringParameter.Bind(Initializer.ParameterMap,TEXT("FogInScattering"), TRUE);
		FogStartDistanceParameter.Bind(Initializer.ParameterMap,TEXT("SharedFogParameter2"));
		FogMinStartDistanceParameter.Bind(Initializer.ParameterMap,TEXT("FogMinStartDistance"), TRUE);
		EncodePowerParameter.Bind(Initializer.ParameterMap,TEXT("EncodePower"), TRUE);
	}

	void SetParameters(const FViewInfo& View, INT NumSceneFogLayers)
	{
		check(NumSceneFogLayers > 0);
		SceneTextureParameters.Set( &View, this);

		// Set the fog constants.
		SetPixelShaderValue(GetPixelShader(),FogInScatteringParameter,View.HeightFogParams.FogInScattering);
		SetPixelShaderValue(GetPixelShader(),FogDistanceScaleParameter,View.HeightFogParams.FogDistanceScale);
		SetPixelShaderValue(GetPixelShader(),FogExtinctionDistanceParameter,View.HeightFogParams.FogExtinctionDistance);
		SetPixelShaderValue(GetPixelShader(),FogStartDistanceParameter,View.HeightFogParams.FogStartDistance);
		SetPixelShaderValue(GetPixelShader(),FogMinStartDistanceParameter,*MinElement(&View.HeightFogParams.FogStartDistance[0], (&View.HeightFogParams.FogStartDistance[0]) + NumSceneFogLayers));
		SetPixelShaderValue(GetPixelShader(),EncodePowerParameter, 1.0f);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << FogDistanceScaleParameter;
		Ar << FogExtinctionDistanceParameter;
		Ar << FogInScatteringParameter;
		Ar << FogStartDistanceParameter;
		Ar << FogMinStartDistanceParameter;
		Ar << EncodePowerParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter FogDistanceScaleParameter;
	FShaderParameter FogExtinctionDistanceParameter;
	FShaderParameter FogInScatteringParameter;
	FShaderParameter FogStartDistanceParameter;
	FShaderParameter FogMinStartDistanceParameter;
	FShaderParameter EncodePowerParameter;
};

typedef THeightFogPixelShader<0,MSAASF_NoMSAA> FDownsampleDepthAndFogPixelShader;
IMPLEMENT_SHADER_TYPE(template<>,FDownsampleDepthAndFogPixelShader,TEXT("HeightFogPixelShader"),TEXT("DownsampleDepthAndFogMain"),SF_Pixel,0,0);

typedef THeightFogPixelShader<1,MSAASF_NoMSAA> FOneLayerFogPixelShader;
IMPLEMENT_SHADER_TYPE(template<>,FOneLayerFogPixelShader,TEXT("HeightFogPixelShader"),TEXT("OneLayerMain"),SF_Pixel,VER_HEIGHTFOG_PIXELSHADER_START_DIST_FIX,0);

typedef THeightFogPixelShader<4,MSAASF_NoMSAA> FFourLayerFogPixelShader;
IMPLEMENT_SHADER_TYPE(template<>,FFourLayerFogPixelShader,TEXT("HeightFogPixelShader"),TEXT("FourLayerMain"),SF_Pixel,VER_HEIGHTFOG_PIXELSHADER_START_DIST_FIX,0);

typedef THeightFogPixelShader<1,MSAASF_PerPixel> FPerPixelOneLayerFogPixelShader;
IMPLEMENT_SHADER_TYPE(template<>,FPerPixelOneLayerFogPixelShader,TEXT("HeightFogPixelShader"),TEXT("OneLayerMain"),SF_Pixel,VER_HEIGHTFOG_PIXELSHADER_START_DIST_FIX,0);

typedef THeightFogPixelShader<4,MSAASF_PerPixel> FPerPixelFourLayerFogPixelShader;
IMPLEMENT_SHADER_TYPE(template<>,FPerPixelFourLayerFogPixelShader,TEXT("HeightFogPixelShader"),TEXT("FourLayerMain"),SF_Pixel,VER_HEIGHTFOG_PIXELSHADER_START_DIST_FIX,0);

typedef THeightFogPixelShader<1,MSAASF_PerFragment>  FPerFragmentOneLayerFogPixelShader;
IMPLEMENT_SHADER_TYPE(template<>,FPerFragmentOneLayerFogPixelShader,TEXT("HeightFogPixelShader"),TEXT("OneLayerMain"),SF_Pixel,VER_HEIGHTFOG_PIXELSHADER_START_DIST_FIX,0);

typedef THeightFogPixelShader<4,MSAASF_PerFragment>  FPerFragmentFourLayerFogPixelShader;
IMPLEMENT_SHADER_TYPE(template<>,FPerFragmentFourLayerFogPixelShader,TEXT("HeightFogPixelShader"),TEXT("FourLayerMain"),SF_Pixel,VER_HEIGHTFOG_PIXELSHADER_START_DIST_FIX,0);

/** The fog vertex declaration resource type. */
class FFogVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor
	virtual ~FFogVertexDeclaration() {}

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
TGlobalResource<FFogVertexDeclaration> GFogVertexDeclaration;

void FSceneRenderer::InitFogConstants()
{
	// console command override
	FLOAT FogDensityOverride = -1.0f;
	FLOAT FogStartDistanceOverride = -1.0f;

#if !FINAL_RELEASE
	{
		// console variable override
		static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("FogDensity")); 

		FogDensityOverride = CVar->GetFloat();
	}

	{
		// console variable override
		static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("FogStartDistance")); 

		FogStartDistanceOverride = CVar->GetFloat();
	}
#endif // !FINAL_RELEASE

	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views(ViewIndex);

		// set fog consts based on height fog components
		if(ShouldRenderFog(View.Family->ShowFlags))
		{
			// Remap the fog layers into back to front order.
			INT FogLayerMap[4];
			INT NumFogLayers = 0;
			for(INT AscendingFogIndex = Min(Scene->Fogs.Num(),4) - 1;AscendingFogIndex >= 0;AscendingFogIndex--)
			{
				const FHeightFogSceneInfo& FogSceneInfo = Scene->Fogs(AscendingFogIndex);
				if(FogSceneInfo.Height > View.ViewOrigin.Z)
				{
					for(INT DescendingFogIndex = 0;DescendingFogIndex <= AscendingFogIndex;DescendingFogIndex++)
					{
						FogLayerMap[NumFogLayers++] = DescendingFogIndex;
					}
					break;
				}
				FogLayerMap[NumFogLayers++] = AscendingFogIndex;
			}

			// Calculate the fog constants.
			for(INT LayerIndex = 0;LayerIndex < NumFogLayers;LayerIndex++)
			{
				// remapped fog layers in ascending order
				const FHeightFogSceneInfo& FogSceneInfo = Scene->Fogs(FogLayerMap[LayerIndex]);
				// log2(1-density)
				View.HeightFogParams.FogDistanceScale[LayerIndex] = appLoge(1.0f - FogSceneInfo.Density) / appLoge(2.0f);
				if(FogLayerMap[LayerIndex] + 1 < NumFogLayers)
				{
					// each min height is adjusted to aligned with the max height of the layer above
					View.HeightFogParams.FogMinHeight[LayerIndex] = Scene->Fogs(FogLayerMap[LayerIndex] + 1).Height;
				}
				else
				{
					// lowest layer extends down
					View.HeightFogParams.FogMinHeight[LayerIndex] = -HALF_WORLD_MAX;
				}
				// max height is set by the actor's height
				View.HeightFogParams.FogMaxHeight[LayerIndex] = FogSceneInfo.Height;
				// This formula is incorrect, but must be used to support legacy content.  The in-scattering color should be computed like this:
				// FogInScattering[LayerIndex] = FLinearColor(FogComponent->LightColor) * (FogComponent->LightBrightness / (appLoge(2.0f) * FogDistanceScale[LayerIndex]));
				View.HeightFogParams.FogInScattering[LayerIndex] = FogSceneInfo.LightColor / appLoge(0.5f);
				// anything beyond the extinction distance goes to full fog
				View.HeightFogParams.FogExtinctionDistance[LayerIndex] = FogSceneInfo.ExtinctionDistance;
				// start distance where fog affects the scene
				View.HeightFogParams.FogStartDistance[LayerIndex] = Max( 0.f, FogSceneInfo.StartDistance );			
			}

			if (Scene->ExponentialFogs.Num() > 0)
			{
				const FLightSceneInfo* DominantDirectionalLight = NULL;
				for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
				{
					const FLightSceneInfo* const LightSceneInfo = LightIt->LightSceneInfo;

					if (LightSceneInfo->LightType == LightType_DominantDirectional)
					{
						DominantDirectionalLight = LightSceneInfo;
						break;
					}
				}

				const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs(0);
				const FLOAT CosTerminatorAngle = Clamp(appCos(FogInfo.LightTerminatorAngle * PI / 180.0f), -1.0f + DELTA, 1.0f - DELTA);

				FLOAT FogDensity = FogInfo.FogDensity;

				// console variable override
				if(FogDensityOverride >= 0.0f)
				{
					// Scale the densities back down to their real scale
					// Artists edit the densities scaled up so they aren't entering in minuscule floating point numbers
					FogDensity = FogDensityOverride / 1000.0f;
				}

				FLOAT FogStartDistance = FogInfo.StartDistance;

				// console variable override
				if(FogStartDistanceOverride >= 0.0f)
				{
					FogStartDistance = FogStartDistanceOverride;
				}

				const FLOAT CollapsedFogParameter = FogDensity * appPow(2.0f, -FogInfo.FogHeightFalloff * (View.ViewOrigin.Z - FogInfo.FogHeight));

				View.bRenderExponentialFog = TRUE;

				// bring -1..1 into range 1..0
				FLOAT NormalizedAngle = 0.5f - 0.5f * CosTerminatorAngle;
				// LogHalf = log(0.5f)
				const FLOAT LogHalf = -0.30103f;
				// precompute a constant so the shader needs less ALU
				FLOAT PowFactor = LogHalf / appLoge(NormalizedAngle);

				View.ExponentialFogParameters = FVector4(CollapsedFogParameter, FogInfo.FogHeightFalloff, PowFactor, FogStartDistance);
				View.ExponentialFogColor = FVector(FogInfo.OppositeLightColor.R, FogInfo.OppositeLightColor.G, FogInfo.OppositeLightColor.B);
				View.LightInscatteringColor = FVector(FogInfo.LightInscatteringColor.R, FogInfo.LightInscatteringColor.G, FogInfo.LightInscatteringColor.B);
				// Use up in world space if there is no dominant directional light
				View.DominantDirectionalLightDirection = DominantDirectionalLight ? -DominantDirectionalLight->GetDirection() : FVector(0,0,1);
				View.FogMaxOpacity = FogInfo.FogMaxOpacity;
			}
		}
	}
}

FGlobalBoundShaderState DownsampleDepthAndFogBoundShaderState;

FGlobalBoundShaderState ExponentialBoundShaderState[MSAASF_Num];
FGlobalBoundShaderState OneLayerFogBoundShaderState[MSAASF_Num];
FGlobalBoundShaderState FourLayerFogBoundShaderState[MSAASF_Num];

/** Sets the bound shader state for either the per-pixel or per-sample fog pass. */
template<EMSAAShaderFrequency MSAAShaderFrequency>
void SetFogShaders(FScene* Scene,const FViewInfo& View)
{
	const INT NumSceneFogs = Clamp<INT>(Scene->Fogs.Num(), 0, 4);
	if (Scene->ExponentialFogs.Num() > 0)
	{
		TShaderMapRef<THeightFogVertexShader<1> > VertexShader(GetGlobalShaderMap());
		TShaderMapRef<TExponentialHeightFogPixelShader<MSAAShaderFrequency> > ExponentialHeightFogPixelShader(GetGlobalShaderMap());

		SetGlobalBoundShaderState(ExponentialBoundShaderState[MSAAShaderFrequency], GFogVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ExponentialHeightFogPixelShader, sizeof(FVector2D));
		VertexShader->SetParameters(View);
		ExponentialHeightFogPixelShader->SetParameters(View);
	}
	//use the optimized one layer version if there is only one height fog layer
	else if (NumSceneFogs == 1)
	{
		TShaderMapRef<THeightFogVertexShader<1> > VertexShader(GetGlobalShaderMap());
		TShaderMapRef<THeightFogPixelShader<1,MSAAShaderFrequency> > OneLayerHeightFogPixelShader(GetGlobalShaderMap());

		SetGlobalBoundShaderState(OneLayerFogBoundShaderState[MSAAShaderFrequency], GFogVertexDeclaration.VertexDeclarationRHI, *VertexShader, *OneLayerHeightFogPixelShader, sizeof(FVector2D));
		VertexShader->SetParameters(View);
		OneLayerHeightFogPixelShader->SetParameters(View, NumSceneFogs);
	}
	//otherwise use the four layer version
	else
	{
		TShaderMapRef<THeightFogVertexShader<4> > VertexShader(GetGlobalShaderMap());
		TShaderMapRef<THeightFogPixelShader<4,MSAAShaderFrequency> > FourLayerHeightFogPixelShader(GetGlobalShaderMap());

		SetGlobalBoundShaderState(FourLayerFogBoundShaderState[MSAAShaderFrequency], GFogVertexDeclaration.VertexDeclarationRHI, *VertexShader, *FourLayerHeightFogPixelShader, sizeof(FVector2D));
		VertexShader->SetParameters(View);
		FourLayerHeightFogPixelShader->SetParameters(View, NumSceneFogs);
	}
}

UBOOL FSceneRenderer::RenderFog(UINT DPGIndex)
{
	const INT NumSceneFogLayers = Scene->Fogs.Num();
	if (DPGIndex == SDPG_World && (NumSceneFogLayers > 0 || Scene->ExponentialFogs.Num() > 0))
	{
		SCOPED_DRAW_EVENT(EventFog)(DEC_SCENE_ITEMS,TEXT("Fog"));

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
			if (View.bOneLayerHeightFogRenderedInAO || !View.IsPerspectiveProjection())
			{
				// Skip one layer height fog for this view since it was already rendered with ambient occlusion
				continue;
			}

			// Set the device viewport for the view.
			RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
			RHISetViewParameters(View);
			RHISetMobileHeightFogParams(View.HeightFogParams);

			if(GRHIShaderPlatform == SP_PCD3D_SM5 && GSystemSettings.UsesMSAA())
			{
				// Clear the stencil buffer to zero.
				RHIClear(FALSE,FLinearColor(0,0,0),FALSE,0,TRUE,0);
			}

			// depth tests (to cull pixels in front of the start distance), no backface culling.
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());

			RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

			RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_SourceAlpha>::GetRHI());

			// disable alpha writes in order to preserve scene depth values on PC
			RHISetColorWriteMask(CW_RED|CW_GREEN|CW_BLUE);

			// Do a separate per-pixel and per-sample pass on D3D11, but only the per-pixel pass on other platforms.
			if(GRHIShaderPlatform == SP_PCD3D_SM5 && GSystemSettings.UsesMSAA())
			{
				for(UINT PassIndex = 0;PassIndex < 2;++PassIndex)
				{
					SCOPED_CONDITIONAL_DRAW_EVENT(EventRenderPerSample,PassIndex==1)(DEC_SCENE_ITEMS,TEXT("PerSample"));

					if(PassIndex == 0)
					{
						// Set the per-pixel shaders, and stencil state that sets the stencil buffer to 1 where the per-pixel shader executes.
						SetFogShaders<MSAASF_PerPixel>(Scene,View);
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0xff,1
							>::GetRHI());
					}
					else
					{
						// Set the per-sample shaders, and stencil state that culls fragments which were written to by the per-pixel shader.
						SetFogShaders<MSAASF_PerFragment>(Scene,View);
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0xff,0
							>::GetRHI());
					}

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
			else
			{
				// Set the non-MSAA shaders.
				SetFogShaders<MSAASF_NoMSAA>(Scene,View);

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

			// restore color write mask
			RHISetColorWriteMask(CW_RED|CW_GREEN|CW_BLUE|CW_ALPHA);

			// Restore the stencil state.
			RHISetStencilState(TStaticStencilState<>::GetRHI());
		}

		//no need to resolve since we used alpha blending
		GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
		return TRUE;
	}

	return FALSE;
}

/** 
 * Renders fog scattering values and max depths into 1/4 size RT0 and RT1. 
 * Called from AmbientOcclusion's DownsampleDepth()
 *
 * @param Scene - current scene
 * @param View - current view
 * @param DPGIndex - depth priority group
 * @param DownsampleDimensions - dimensions for downsampling
 */
UBOOL RenderQuarterDownsampledDepthAndFog(const FScene* Scene, const FViewInfo& View, UINT DPGIndex, const FDownsampleDimensions& DownsampleDimensions)
{
#if XBOX
	const INT NumSceneFogLayers = Scene->Fogs.Num();
	// Currently only works with one layer height fog
	if (DPGIndex == SDPG_World && NumSceneFogLayers == 1)
	{
		// Verify that the ambient occlusion downsample factor matches the fog downsample factor
		checkSlow(DownsampleDimensions.Factor == 2);

		SCOPED_DRAW_EVENT(EventFog)(DEC_SCENE_ITEMS,TEXT("DepthAndFogDownsample"));

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

		GSceneRenderTargets.BeginRenderingFogBuffer();

		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		// Disable depth test and writes
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());

		// Set the viewport to the current view in the occlusion buffer.
		RHISetViewport(DownsampleDimensions.TargetX, DownsampleDimensions.TargetY, 0.0f, 
			DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX, DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY, 1.0f);				

		TShaderMapRef<THeightFogVertexShader<1> > VertexShader(GetGlobalShaderMap());
		TShaderMapRef<THeightFogPixelShader<0,MSAASF_NoMSAA> > DownsampleDepthAndFogPixelShader(GetGlobalShaderMap());

		SetGlobalBoundShaderState(DownsampleDepthAndFogBoundShaderState, GFogVertexDeclaration.VertexDeclarationRHI, *VertexShader, *DownsampleDepthAndFogPixelShader, sizeof(FVector2D));
		VertexShader->SetParameters(View);
		DownsampleDepthAndFogPixelShader->SetParameters(View, NumSceneFogLayers);

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

		GSceneRenderTargets.FinishRenderingFogBuffer(
			FResolveParams(FResolveRect(
				DownsampleDimensions.TargetX,
				DownsampleDimensions.TargetY, 
				DownsampleDimensions.TargetX + DownsampleDimensions.TargetSizeX,
				DownsampleDimensions.TargetY + DownsampleDimensions.TargetSizeY
				))
			);

		return TRUE;
	}
#endif
	return FALSE;
}

UBOOL ShouldRenderFog(const EShowFlags& ShowFlags)
{
	return (ShowFlags & SHOW_Fog) 
		&& (ShowFlags & SHOW_Materials) 
		&& !(ShowFlags & SHOW_TextureDensity) 
		&& !(ShowFlags & SHOW_ShaderComplexity) 
		&& !(ShowFlags & SHOW_LightMapDensity);
}
