/*=============================================================================
	ShadowRendering.cpp: Shadow rendering implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "UnTextureLayout.h"
#if WITH_REALD
	#include "RealD/RealD.h"
#endif	//WITH_REALD

//@DEBUG: Set to 1 to output the shadow depth buffer as color. Do the same in ShadowProjectionPixelShader.msf.
#define VISUALIZE_SHADOW_DEPTH 0

// Globals
const UINT SHADOW_BORDER=5; 

/** 
 * Whether to render whole scene point light shadows in one pass where supported (SM5), 
 * Or to render them using 6 spotlight projections.
 */
UBOOL GRenderOnePassPointLightShadows = TRUE;

static FLOAT GetShadowDepthBias(const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
{
	FLOAT DepthBias = GSystemSettings.ShadowDepthBias * 512.0f / Max(ShadowInfo->ResolutionX,ShadowInfo->ResolutionY);
#if WITH_MOBILE_RHI
	if ( GUsingMobileRHI )
	{
		DepthBias = GSystemSettings.ShadowDepthBias;
	}
#endif


	if (!ShadowInfo->bFullSceneShadow)
	{
		// Apply per-material depth bias 
		DepthBias += MaterialRenderProxy->GetMaterial()->GetShadowDepthBias();
	}

	if (ShouldUseBranchingPCF(ShadowInfo->LightSceneInfo->ShadowProjectionTechnique))
	{
		//small tweakable to make the effect of the offset used with Branching PCF match up with the default PCF
		DepthBias += .001f;
	}
	// Preshadows don't need a depth bias since there is no self shadowing
	DepthBias = ShadowInfo->bPreShadow ? 0.0f : DepthBias;
	// Reduce the depth bias that has been tweaked for per-object shadows when rendering a full scene shadow
	//@todo - need a separate tweaked depth bias for full scene shadows
	DepthBias = ShadowInfo->bFullSceneShadow ? DepthBias * .6f : DepthBias;
	if (ShadowInfo->SplitIndex > 0 && ShadowInfo->bDirectionalLight)
	{
		DepthBias *= ShadowInfo->SplitIndex * GSystemSettings.CSMSplitDepthBiasScale;
	}

	return DepthBias;
}

/** Shader parameters for rendering the depth of a mesh for shadowing. */
class FShadowDepthShaderParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ProjectionMatrixParameter.Bind(ParameterMap,TEXT("ProjectionMatrix"),TRUE);
		InvMaxSubjectDepthParameter.Bind(ParameterMap,TEXT("InvMaxSubjectDepth"),TRUE);
		DepthBiasParameter.Bind(ParameterMap,TEXT("DepthBias"),TRUE);
		ClampToNearPlaneParameter.Bind(ParameterMap,TEXT("bClampToNearPlane"),TRUE);
	}

	template<typename ShaderRHIParamRef>
	void Set(ShaderRHIParamRef Shader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		FMatrix ProjectionMatrix = FTranslationMatrix(ShadowInfo->PreShadowTranslation - View.PreViewTranslation) * ShadowInfo->SubjectAndReceiverMatrix;

		SetShaderValue(
			Shader,
			ProjectionMatrixParameter,
			ProjectionMatrix
			);
		FLOAT InvMaxSubjectDepth = 1.0f / ShadowInfo->MaxSubjectDepth;
		FLOAT DepthBias = GetShadowDepthBias(ShadowInfo, MaterialRenderProxy);
#if WITH_ES2_RHI
		if (GUsingES2RHI)
		{
			// This is equivalent to remapping the projected Z in the shader from [-MaxSubjectDepth:MaxSubjectDepth] prior to the divide by MaxSubjectDepth
			// which gives us the depth range [-1:1] we want in OpenGL
			InvMaxSubjectDepth *= 2.f;
			DepthBias -= 1.f;
		}
#endif
		SetShaderValue(Shader,InvMaxSubjectDepthParameter,InvMaxSubjectDepth);
		SetShaderValue(Shader,DepthBiasParameter,DepthBias);
		// Only clamp vertices to the near plane when rendering whole scene directional light shadow depths or preshadows from directional lights
		SetShaderValue(Shader,ClampToNearPlaneParameter,(FLOAT)((ShadowInfo->bFullSceneShadow || ShadowInfo->bPreShadow) && ShadowInfo->bDirectionalLight));
	}

	/** Set the vertex shader parameter values. */
	void SetVertexShader(FShader* VertexShader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		Set(VertexShader->GetVertexShader(), View, ShadowInfo, MaterialRenderProxy);
	}

#if WITH_D3D11_TESSELLATION
	/** Set the domain shader parameter values. */
	void SetDomainShader(FShader* DomainShader, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FMaterialRenderProxy* MaterialRenderProxy)
	{
		Set(DomainShader->GetDomainShader(), View, ShadowInfo, MaterialRenderProxy);
	}
#endif

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FShadowDepthShaderParameters& P)
	{
		Ar << P.ProjectionMatrixParameter;
		Ar << P.InvMaxSubjectDepthParameter;
		Ar << P.DepthBiasParameter;
		Ar << P.ClampToNearPlaneParameter;

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			P.ProjectionMatrixParameter.SetShaderParamName(TEXT("ProjectionMatrix"));
			P.InvMaxSubjectDepthParameter.SetShaderParamName(TEXT("InvMaxSubjectDepth"));
			P.DepthBiasParameter.SetShaderParamName(TEXT("DepthBias"));
		}
#endif

		return Ar;
	}

private:
	FShaderParameter ProjectionMatrixParameter;
	FShaderParameter InvMaxSubjectDepthParameter;
	FShaderParameter DepthBiasParameter;
	FShaderParameter ClampToNearPlaneParameter;
};

/**
 * A vertex shader for rendering the depth of a mesh.
 */
class FShadowDepthVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(FShadowDepthVertexShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile for default, masked or lit translucent materials
		return Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->CastLitTranslucencyShadowAsMasked() || Material->MaterialModifiesMeshPosition();
	}

	FShadowDepthVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
		ShadowParameters.Bind(Initializer.ParameterMap);
	}

	FShadowDepthVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		Ar << ShadowParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& MaterialResource,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);

		extern UBOOL GCachePreshadows;
		FMaterialRenderContext MaterialRenderContext(
			MaterialRenderProxy, 
			MaterialResource,
			// Override the context's time to 0 for preshadows, so that the shadow will not animate.
			// This is partly to be consistent with precomputed shadowing, 
			// And also so that preshadows stored in the preshadow cache will not cause jerky animation.
			ShadowInfo->bPreShadow ? 0 : View.Family->CurrentWorldTime, 
			ShadowInfo->bPreShadow ? 0 : View.Family->CurrentRealTime, 
			&View,
			// Force this context's uniform expressions to not get cached (or use existing cached values), since we are overriding time
			!ShadowInfo->bPreShadow || !GCachePreshadows);

		MaterialParameters.Set(this,MaterialRenderContext);

		ShadowParameters.SetVertexShader(this, View, ShadowInfo, MaterialRenderProxy);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this, Mesh, BatchElementIndex, View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo, Mesh, BatchElementIndex, View);
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;
	FShadowDepthShaderParameters ShadowParameters;
};

enum EShadowDepthVertexShaderMode
{
	VertexShadowDepth_PerspectiveCorrect,
	VertexShadowDepth_OutputDepth,
	VertexShadowDepth_OutputDepthToColor,
	VertexShadowDepth_OnePassPointLight
};

/**
 * A vertex shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode>
class TShadowDepthVertexShader : public FShadowDepthVertexShader
{
	DECLARE_SHADER_TYPE(TShadowDepthVertexShader,MeshMaterial);
public:

	TShadowDepthVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShadowDepthVertexShader(Initializer)
	{
	}

	TShadowDepthVertexShader() {}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FShadowDepthVertexShader::ShouldCache(Platform, Material, VertexFactoryType)
			// Compile the version that outputs depth to a depth buffer for all platforms,
			// Only compile the version that outputs depth to color for PC platforms.
			// @todo wiiu: WiiU doesn't need this when depth textures are working
			&& (ShaderMode != VertexShadowDepth_OutputDepthToColor || IsPCPlatform(Platform) || Platform == SP_WIIU)
			// Only compile one pass point light shaders for SM5
			&& (ShaderMode != VertexShadowDepth_OnePassPointLight || Platform == SP_PCD3D_SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("OUTPUT_DEPTH_TO_COLOR"),*FString::Printf(TEXT("%u"),(UBOOL)(ShaderMode == VertexShadowDepth_OutputDepthToColor)));
		OutEnvironment.Definitions.Set(TEXT("PERSPECTIVE_CORRECT_DEPTH"),*FString::Printf(TEXT("%u"),(UBOOL)(ShaderMode == VertexShadowDepth_PerspectiveCorrect)));
		OutEnvironment.Definitions.Set(TEXT("ONEPASS_POINTLIGHT_SHADOW"),*FString::Printf(TEXT("%u"),(UBOOL)(ShaderMode == VertexShadowDepth_OnePassPointLight)));
	}
};

#if WITH_D3D11_TESSELLATION

/**
 * A Hull shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode>
class TShadowDepthHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(TShadowDepthHullShader,MeshMaterial);
public:

	
	TShadowDepthHullShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHullShader(Initializer)
	{}

	TShadowDepthHullShader() {}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use ShouldCache from vertex shader
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TShadowDepthVertexShader<ShaderMode>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use compilation env from vertex shader
		TShadowDepthVertexShader<ShaderMode>::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}
};

/**
 * A domain shader for rendering the depth of a mesh.
 */
class FShadowDepthDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(FShadowDepthDomainShader,MeshMaterial);
public:

	FShadowDepthDomainShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDomainShader(Initializer)
	{
		ShadowParameters.Bind(Initializer.ParameterMap);
	}

	FShadowDepthDomainShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FBaseDomainShader::Serialize(Ar);
		Ar << ShadowParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FBaseDomainShader::SetParameters(MaterialRenderProxy, View);
		ShadowParameters.SetDomainShader(this, View, ShadowInfo, MaterialRenderProxy);
	}

private:
	FShadowDepthShaderParameters ShadowParameters;
};

/**
 * A Domain shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode>
class TShadowDepthDomainShader : public FShadowDepthDomainShader
{
	DECLARE_SHADER_TYPE(TShadowDepthDomainShader,MeshMaterial);
public:

	TShadowDepthDomainShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShadowDepthDomainShader(Initializer)
	{}

	TShadowDepthDomainShader() {}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use ShouldCache from vertex shader
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TShadowDepthVertexShader<ShaderMode>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use compilation env from vertex shader
		TShadowDepthVertexShader<ShaderMode>::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}
};

/** Geometry shader that allows one pass point light shadows by cloning triangles to all faces of the cube map. */
class FOnePassPointShadowProjectionGeometryShader : public FShader
{
	DECLARE_SHADER_TYPE(FOnePassPointShadowProjectionGeometryShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return TShadowDepthVertexShader<VertexShadowDepth_OnePassPointLight>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TShadowDepthVertexShader<VertexShadowDepth_OnePassPointLight>::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FOnePassPointShadowProjectionGeometryShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		ShadowViewProjectionMatricesParameter.Bind(Initializer.ParameterMap, TEXT("ShadowViewProjectionMatrices"), TRUE);
		MeshVisibleToFaceParameter.Bind(Initializer.ParameterMap, TEXT("MeshVisibleToFace"), TRUE);
	}

	FOnePassPointShadowProjectionGeometryShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ShadowViewProjectionMatricesParameter;
		Ar << MeshVisibleToFaceParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		const FMatrix Translation = FTranslationMatrix(-View.PreViewTranslation);

		FMatrix TranslatedShadowViewProjectionMatrices[6];
		for (INT FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			// Have to apply the pre-view translation to the view - projection matrices
			TranslatedShadowViewProjectionMatrices[FaceIndex] = Translation * ShadowInfo->OnePassShadowViewProjectionMatrices(FaceIndex);
		}

		// Set the view projection matrices that will transform positions from world to cube map face space
		SetShaderValues<FGeometryShaderRHIParamRef, FMatrix>(
			GetGeometryShader(),
			ShadowViewProjectionMatricesParameter,
			TranslatedShadowViewProjectionMatrices,
			ARRAY_COUNT(TranslatedShadowViewProjectionMatrices)
			);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FProjectedShadowInfo* ShadowInfo, const FSceneView& View)
	{
		if (MeshVisibleToFaceParameter.IsBound())
		{
			FVector4 MeshVisibleToFace[6];
			for (INT FaceIndex = 0; FaceIndex < 6; FaceIndex++)
			{
				MeshVisibleToFace[FaceIndex] = FVector4(ShadowInfo->OnePassShadowFrustums(FaceIndex).IntersectBox(PrimitiveSceneInfo->Bounds.Origin, PrimitiveSceneInfo->Bounds.BoxExtent), 0, 0, 0);
			}

			// Set the view projection matrices that will transform positions from world to cube map face space
			SetShaderValues<FGeometryShaderRHIParamRef, FVector4>(
				GetGeometryShader(),
				MeshVisibleToFaceParameter,
				MeshVisibleToFace,
				ARRAY_COUNT(MeshVisibleToFace)
				);
		}
	}

private:
	FShaderParameter ShadowViewProjectionMatricesParameter;
	FShaderParameter MeshVisibleToFaceParameter;
};

#define IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(ShaderMode) \
	typedef TShadowDepthVertexShader<ShaderMode> TShadowDepthVertexShader##ShaderMode;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVertexShader##ShaderMode,TEXT("ShadowDepthVertexShader"),TEXT("Main"),SF_Vertex,0,0);	\
	typedef TShadowDepthHullShader<ShaderMode> TShadowDepthHullShader##ShaderMode;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthHullShader##ShaderMode,TEXT("ShadowDepthVertexShader"),TEXT("MainHull"),SF_Hull,0,0);	\
	typedef TShadowDepthDomainShader<ShaderMode> TShadowDepthDomainShader##ShaderMode;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthDomainShader##ShaderMode,TEXT("ShadowDepthVertexShader"),TEXT("MainDomain"),SF_Domain,0,0);

IMPLEMENT_SHADER_TYPE(,FOnePassPointShadowProjectionGeometryShader,TEXT("ShadowDepthVertexShader"),TEXT("MainOnePassPointLightGS"),SF_Geometry,0,0);

#else // #if WITH_D3D11_TESSELLATION

#define IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(ShaderMode) \
	typedef TShadowDepthVertexShader<ShaderMode> TShadowDepthVertexShader##ShaderMode;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVertexShader##ShaderMode,TEXT("ShadowDepthVertexShader"),TEXT("Main"),SF_Vertex,0,0);	

#endif

IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_PerspectiveCorrect); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OutputDepth); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OutputDepthToColor);
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OnePassPointLight);

/**
 * A pixel shader for rendering the depth of a mesh.
 */
class FShadowDepthPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FShadowDepthPixelShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsSpecialEngineMaterial();
	}

	FShadowDepthPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
		InvMaxSubjectDepthParameter.Bind(Initializer.ParameterMap,TEXT("InvMaxSubjectDepth"),TRUE);
		DepthBiasParameter.Bind(Initializer.ParameterMap,TEXT("DepthBias"),TRUE);
		ShadowCasterPositionParameter.Bind(Initializer.ParameterMap,TEXT("ShadowCasterPosition"),TRUE);
		ModShadowColorParameter.Bind(Initializer.ParameterMap,TEXT("ModShadowColor"),TRUE);
	}

	FShadowDepthPixelShader() {}

	void SetParameters(
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& MaterialResource,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		extern UBOOL GCachePreshadows;
		FMaterialRenderContext MaterialRenderContext(
			MaterialRenderProxy, 
			MaterialResource,
			// Override the context's time to 0 for preshadows, so that the shadow will not animate.
			// This is partly to be consistent with precomputed shadowing, 
			// And also so that preshadows stored in the preshadow cache will not cause jerky animation.
			ShadowInfo->bPreShadow ? 0 : View.Family->CurrentWorldTime, 
			ShadowInfo->bPreShadow ? 0 : View.Family->CurrentRealTime, 
			&View,
			// Force this context's uniform expressions to not get cached (or use existing cached values), since we are overriding time
			!ShadowInfo->bPreShadow || !GCachePreshadows);

		MaterialParameters.Set(this,MaterialRenderContext);

		SetPixelShaderValue(GetPixelShader(),InvMaxSubjectDepthParameter,1.0f / ShadowInfo->MaxSubjectDepth);
		const FLOAT DepthBias = GetShadowDepthBias(ShadowInfo, MaterialRenderProxy);
		SetPixelShaderValue(GetPixelShader(),DepthBiasParameter,DepthBias);

#if WITH_MOBILE_RHI
		if (GUsingMobileRHI)
		{
			SetPixelShaderValue(GetPixelShader(),ShadowCasterPositionParameter, ShadowInfo->ParentSceneInfo->Bounds.Origin + View.PreViewTranslation);
			const FLinearColor ModShadowColor = Lerp(FLinearColor::White, ShadowInfo->LightSceneInfo->ModShadowColor, ShadowInfo->FadeAlphas(0));
			SetPixelShaderValue(GetPixelShader(), ModShadowColorParameter, ModShadowColor);
		}
#endif
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View,UBOOL bBackFace)
	{
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaterialParameters;
		Ar << InvMaxSubjectDepthParameter;
		Ar << DepthBiasParameter;
		Ar << ShadowCasterPositionParameter;
		Ar << ModShadowColorParameter;

		ShadowCasterPositionParameter.SetShaderParamName(TEXT("ShadowCasterWorldPosition"));
		ModShadowColorParameter.SetShaderParamName(TEXT("ModShadowColor"));

		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderParameter InvMaxSubjectDepthParameter;
	FShaderParameter DepthBiasParameter;
	FShaderParameter ShadowCasterPositionParameter;
	FShaderParameter ModShadowColorParameter;
};

enum EShadowDepthPixelShaderMode
{
	PixelShadowDepth_NonPerspectiveCorrect,
	PixelShadowDepth_PerspectiveCorrect,
	PixelShadowDepth_OnePassPointLight
};

template <EShadowDepthPixelShaderMode ShaderMode, UBOOL bUseScreenDoorFade>
class TShadowDepthPixelShader : public FShadowDepthPixelShader
{
	DECLARE_SHADER_TYPE(TShadowDepthPixelShader,MeshMaterial);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return (FShadowDepthPixelShader::ShouldCache(Platform, Material, VertexFactoryType) ||
			// Only compile the non-screendoor fade version for masked or lit translucent materials
			!bUseScreenDoorFade && (Material->IsMasked() || Material->CastLitTranslucencyShadowAsMasked()))
			// Only compile one pass point light shaders for SM5
			&& (ShaderMode != PixelShadowDepth_OnePassPointLight || Platform == SP_PCD3D_SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("PERSPECTIVE_CORRECT_DEPTH"),*FString::Printf(TEXT("%u"),(UBOOL)(ShaderMode == PixelShadowDepth_PerspectiveCorrect)));
		OutEnvironment.Definitions.Set(TEXT("ONEPASS_POINTLIGHT_SHADOW"),*FString::Printf(TEXT("%u"),(UBOOL)(ShaderMode == PixelShadowDepth_OnePassPointLight)));
		if (bUseScreenDoorFade)
		{
			// Enable screendoor fading for the versions of this shader that are compiled for the default material
			OutEnvironment.Definitions.Set(TEXT("MATERIAL_USE_SCREEN_DOOR_FADE"),*FString::Printf(TEXT("%u"),1));
		}
	}

	TShadowDepthPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShadowDepthPixelShader(Initializer)
	{
	}

	TShadowDepthPixelShader() {}
};

// typedef required to get around macro expansion failure due to commas in template argument list for TShadowDepthPixelShader
#define IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(ShaderMode,bUseScreenDoorFade) \
	typedef TShadowDepthPixelShader<ShaderMode,bUseScreenDoorFade> TShadowDepthPixelShader##ShaderMode##bUseScreenDoorFade; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthPixelShader##ShaderMode##bUseScreenDoorFade,TEXT("ShadowDepthPixelShader"),TEXT("Main"),SF_Pixel,0,0);

IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_NonPerspectiveCorrect,TRUE);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_NonPerspectiveCorrect,FALSE);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_PerspectiveCorrect,TRUE);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_PerspectiveCorrect,FALSE);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_OnePassPointLight,TRUE);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_OnePassPointLight,FALSE);

/** The shadow frustum vertex declaration. */
TGlobalResource<FShadowFrustumVertexDeclaration> GShadowFrustumVertexDeclaration;

/*-----------------------------------------------------------------------------
	FShadowProjectionVertexShader
-----------------------------------------------------------------------------*/

UBOOL FShadowProjectionVertexShader::ShouldCache(EShaderPlatform Platform)
{
	return TRUE;
}

FShadowProjectionVertexShader::FShadowProjectionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
{
	ScreenToShadowMatrixParameter.Bind(Initializer.ParameterMap, TEXT("ScreenToShadowMatrix"), TRUE);
}

void FShadowProjectionVertexShader::SetParameters(const FSceneView& View, const FProjectedShadowInfo* ShadowInfo)
{
	// Set the transform from screen coordinates to shadow depth texture coordinates.
	const FMatrix ScreenToShadow = ShadowInfo->GetScreenToShadowMatrix(View, FALSE);
	SetShaderValue(GetVertexShader(), ScreenToShadowMatrixParameter, ScreenToShadow);
}

UBOOL FShadowProjectionVertexShader::Serialize(FArchive& Ar)
{
	UBOOL bRet = FShader::Serialize(Ar);
	Ar << ScreenToShadowMatrixParameter;

#if WITH_MOBILE_RHI
	if (GUsingMobileRHI)
	{
		ScreenToShadowMatrixParameter.SetShaderParamName(TEXT("ScreenToShadowMatrix"));
	}
#endif


	return bRet;
}

IMPLEMENT_SHADER_TYPE(,FShadowProjectionVertexShader,TEXT("ShadowProjectionVertexShader"),TEXT("Main"),SF_Vertex,0,0);


/*-----------------------------------------------------------------------------
	FModShadowProjectionVertexShader
-----------------------------------------------------------------------------*/

/**
 * Constructor - binds all shader params
 * @param Initializer - init data from shader compiler
 */
FModShadowProjectionVertexShader::FModShadowProjectionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
:	FShadowProjectionVertexShader(Initializer)
{		
}

/**
 * Sets the current vertex shader
 * @param View - current view
 * @param ShadowInfo - projected shadow info for a single light
 */
void FModShadowProjectionVertexShader::SetParameters(	
	const FSceneView& View,
	const FProjectedShadowInfo* ShadowInfo)
{
	FShadowProjectionVertexShader::SetParameters(View, ShadowInfo);
}

/**
 * Serialize the parameters for this shader
 * @param Ar - archive to serialize to
 */
UBOOL FModShadowProjectionVertexShader::Serialize(FArchive& Ar)
{
	return FShadowProjectionVertexShader::Serialize(Ar);
}

IMPLEMENT_SHADER_TYPE(,FModShadowProjectionVertexShader,TEXT("ModShadowProjectionVertexShader"),TEXT("Main"),SF_Vertex,0,0);

/**
 * Implementations for TShadowProjectionPixelShader.  
 */

//Cheap version that uses Hardware PCF
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionPixelShader<F4SampleHwPCF>,TEXT("ShadowProjectionPixelShader"),TEXT("HardwarePCFMain"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
//Cheap version
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionPixelShader<F4SampleManualPCFPerPixel>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionPixelShader<F4SampleManualPCFPerFragment>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
//Full version that uses Hardware PCF
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionPixelShader<F16SampleHwPCF>,TEXT("ShadowProjectionPixelShader"),TEXT("HardwarePCFMain"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
//Full version that uses Fetch4
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionPixelShader<F16SampleFetch4PCF>,TEXT("ShadowProjectionPixelShader"),TEXT("Fetch4Main"),SF_Pixel,0,0);
//Full version
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionPixelShader<F16SampleManualPCFPerPixel>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionPixelShader<F16SampleManualPCFPerFragment>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel,VER_CONTENT_RESAVE_AUGUST_2007_QA_BUILD,0);
//Implement a geometry shader for rendering one pass point light shadows
IMPLEMENT_SHADER_TYPE(,FOnePassPointShadowProjectionPixelShader,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel,0,0);

// Per-fragment mask shader.
IMPLEMENT_SHADER_TYPE(,FShadowPerFragmentMaskPixelShader,TEXT("PerFragmentMaskShader"),TEXT("Main"),SF_Pixel,0,0);

/**
* Get the version of TShadowProjectionPixelShader that should be used based on the hardware's capablities
* @return a pointer to the chosen shader
*/
FShadowProjectionPixelShaderInterface* GetProjPixelShaderRef(BYTE LightShadowQuality,UBOOL bPerFragment)
{
	FShadowProjectionPixelShaderInterface* PixelShader = NULL;

	//apply the system settings bias to the light's shadow quality
	BYTE EffectiveShadowFilterQuality = Max(LightShadowQuality + GSystemSettings.ShadowFilterQualityBias, 0);

	if (EffectiveShadowFilterQuality == SFQ_Low)
	{
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef<TShadowProjectionPixelShader<F4SampleHwPCF> > FourSamplePixelShader(GetGlobalShaderMap());
			PixelShader = *FourSamplePixelShader;
		}
		else
		{
			if(bPerFragment)
			{
				TShaderMapRef<TShadowProjectionPixelShader<F4SampleManualPCFPerFragment> > FourSamplePixelShader(GetGlobalShaderMap());
				PixelShader = *FourSamplePixelShader;
			}
			else
			{
				TShaderMapRef<TShadowProjectionPixelShader<F4SampleManualPCFPerPixel> > FourSamplePixelShader(GetGlobalShaderMap());
				PixelShader = *FourSamplePixelShader;
			}
		}
	}
	//todo - implement medium quality path, 9 samples?
	else
	{
		if (GSceneRenderTargets.IsHardwarePCFSupported())
		{
			TShaderMapRef<TShadowProjectionPixelShader<F16SampleHwPCF> > SixteenSamplePixelShader(GetGlobalShaderMap());
			PixelShader = *SixteenSamplePixelShader;
		}
		else if (GSceneRenderTargets.IsFetch4Supported())
		{
			TShaderMapRef<TShadowProjectionPixelShader<F16SampleFetch4PCF> > SixteenSamplePixelShader(GetGlobalShaderMap());
			PixelShader = *SixteenSamplePixelShader;
		}
		else
		{
			if(bPerFragment)
			{
				TShaderMapRef<TShadowProjectionPixelShader<F16SampleManualPCFPerFragment> > SixteenSamplePixelShader(GetGlobalShaderMap());
				PixelShader = *SixteenSamplePixelShader;
			}
			else
			{
				TShaderMapRef<TShadowProjectionPixelShader<F16SampleManualPCFPerPixel> > SixteenSamplePixelShader(GetGlobalShaderMap());
				PixelShader = *SixteenSamplePixelShader;
			}
		}
	}
	return PixelShader;
}

/** 
 * ChooseBoundShaderState - decides which bound shader state should be used based on quality settings
 * @param LightShadowQuality - light's filter quality setting
 * @return FGlobalBoundShaderState - the bound shader state chosen
 */
FGlobalBoundShaderState* ChooseBoundShaderState(
	BYTE LightShadowQuality,
	TStaticArray<FGlobalBoundShaderState,SFQ_Num>& BoundShaderStates
	)
{
	//apply the system settings bias to the light's shadow quality
	BYTE EffectiveShadowFilterQuality = Clamp(LightShadowQuality + GSystemSettings.ShadowFilterQualityBias, 0, SFQ_Num - 1);
	return &BoundShaderStates[EffectiveShadowFilterQuality];
}

/*-----------------------------------------------------------------------------
	FShadowDepthDrawingPolicy
-----------------------------------------------------------------------------*/

const FProjectedShadowInfo* FShadowDepthDrawingPolicy::ShadowInfo = NULL;

FShadowDepthDrawingPolicy::FShadowDepthDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	UBOOL bInPreShadow,
	UBOOL bInTranslucentPreShadow,
	UBOOL bInFullSceneShadow,
	UBOOL bInDirectionalLight,
	UBOOL bInUseScreenDoorDefaultMaterialShader,
	UBOOL bInCastShadowAsTwoSided,
	UBOOL bInReverseCulling,
	UBOOL bInOnePassPointLightShadow
	):
	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,FALSE,bInCastShadowAsTwoSided),
	GeometryShader(NULL),
	bPreShadow(bInPreShadow),
	bTranslucentPreShadow(bInTranslucentPreShadow),
	bFullSceneShadow(bInFullSceneShadow),
	bDirectionalLight(bInDirectionalLight),
	bUseScreenDoorDefaultMaterialShader(bInUseScreenDoorDefaultMaterialShader),
	bReverseCulling(bInReverseCulling),
	bOnePassPointLightShadow(bInOnePassPointLightShadow)
{
	// Use perspective correct shadow depths for shadow types which typically render low poly meshes into the shadow depth buffer.
	// Depth will be interpolated to the pixel shader and written out, which disables HiZ and double speed Z.
	// Directional light shadows use an ortho projection and can use the non-perspective correct path without artifacts.
	// One pass point lights don't output a linear depth, so they are already perspective correct.
	const UBOOL bUsePerspectiveCorrectShadowDepths = (bInPreShadow || bInFullSceneShadow) && !bInDirectionalLight && !bInOnePassPointLightShadow;

	// If the material is not masked, get the shaders from the default material.
	// This still handles two-sided materials because we are only overriding which material the shaders come from,
	// Not which material's two sided flag is checked.
	const FMaterial& DefaultMaterialResource = *GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();

	// Vertex related shaders 

	// Get the vertex shader from the original material if the material uses vertex position offset or if it is masked.
	const FMaterial& VertexShaderMaterialResource = (InMaterialResource.IsMasked() || InMaterialResource.CastLitTranslucencyShadowAsMasked() || InMaterialResource.MaterialModifiesMeshPosition()) ? InMaterialResource : DefaultMaterialResource;

#if WITH_D3D11_TESSELLATION
	HullShader = NULL;
	DomainShader = NULL;

	const EMaterialTessellationMode TessellationMode = InMaterialResource.GetD3D11TessellationMode();
	const UBOOL bInitializeTessellationShaders = 
		GRHIShaderPlatform == SP_PCD3D_SM5
		&& InVertexFactory->GetType()->SupportsTessellationShaders()
		&& TessellationMode != MTM_NoTessellation;
#endif

	// Vertex related shaders
	if (bOnePassPointLightShadow)
	{
		VertexShader = VertexShaderMaterialResource.GetShader<TShadowDepthVertexShader<VertexShadowDepth_OnePassPointLight> >(InVertexFactory->GetType());	
#if WITH_D3D11_TESSELLATION
		// Use the geometry shader which will clone output triangles to all faces of the cube map
		GeometryShader = VertexShaderMaterialResource.GetShader<FOnePassPointShadowProjectionGeometryShader>(InVertexFactory->GetType());
		if(bInitializeTessellationShaders)
		{
			HullShader = VertexShaderMaterialResource.GetShader<TShadowDepthHullShader<VertexShadowDepth_OnePassPointLight> >(InVertexFactory->GetType());	
			DomainShader = VertexShaderMaterialResource.GetShader<TShadowDepthDomainShader<VertexShadowDepth_OnePassPointLight> >(InVertexFactory->GetType());	
		}
#endif
	}
	else if (bUsePerspectiveCorrectShadowDepths)
	{
		VertexShader = VertexShaderMaterialResource.GetShader<TShadowDepthVertexShader<VertexShadowDepth_PerspectiveCorrect> >(InVertexFactory->GetType());	
#if WITH_D3D11_TESSELLATION
		if(bInitializeTessellationShaders)
		{
			HullShader = VertexShaderMaterialResource.GetShader<TShadowDepthHullShader<VertexShadowDepth_PerspectiveCorrect> >(InVertexFactory->GetType());	
			DomainShader = VertexShaderMaterialResource.GetShader<TShadowDepthDomainShader<VertexShadowDepth_PerspectiveCorrect> >(InVertexFactory->GetType());	
		}
#endif
	}
	else if (!bTranslucentPreShadow && (GSceneRenderTargets.IsFetch4Supported() || GSceneRenderTargets.IsHardwarePCFSupported()) || GSupportsDepthTextures)
	{
		VertexShader = VertexShaderMaterialResource.GetShader<TShadowDepthVertexShader<VertexShadowDepth_OutputDepth> >(InVertexFactory->GetType());	
#if WITH_D3D11_TESSELLATION
		if(bInitializeTessellationShaders)
		{
			HullShader = VertexShaderMaterialResource.GetShader<TShadowDepthHullShader<VertexShadowDepth_OutputDepth> >(InVertexFactory->GetType());	
			DomainShader = VertexShaderMaterialResource.GetShader<TShadowDepthDomainShader<VertexShadowDepth_OutputDepth> >(InVertexFactory->GetType());	
		}
#endif
	}
	else
	{
		VertexShader = VertexShaderMaterialResource.GetShader<TShadowDepthVertexShader<VertexShadowDepth_OutputDepthToColor> >(InVertexFactory->GetType());	
#if WITH_D3D11_TESSELLATION
		if(bInitializeTessellationShaders)
		{
			HullShader = VertexShaderMaterialResource.GetShader<TShadowDepthHullShader<VertexShadowDepth_OutputDepthToColor> >(InVertexFactory->GetType());	
			DomainShader = VertexShaderMaterialResource.GetShader<TShadowDepthDomainShader<VertexShadowDepth_OutputDepthToColor> >(InVertexFactory->GetType());	
		}
#endif
	}

	// Pixel shaders
	if (InMaterialResource.IsMasked() || InMaterialResource.CastLitTranslucencyShadowAsMasked())
	{
		if (bUsePerspectiveCorrectShadowDepths)
		{
			// Nothing to do if bInUseScreenDoorDefaultMaterialShader == TRUE because fading will already work since we are not using the default material's shaders
			PixelShader = InMaterialResource.GetShader<TShadowDepthPixelShader<PixelShadowDepth_PerspectiveCorrect,FALSE> >(InVertexFactory->GetType());
		}
		else if (bOnePassPointLightShadow)
		{
			PixelShader = InMaterialResource.GetShader<TShadowDepthPixelShader<PixelShadowDepth_OnePassPointLight,FALSE> >(InVertexFactory->GetType());
		}
		else
		{
			// Nothing to do if bInUseScreenDoorDefaultMaterialShader == TRUE because fading will already work since we are not using the default material's shaders
			PixelShader = InMaterialResource.GetShader<TShadowDepthPixelShader<PixelShadowDepth_NonPerspectiveCorrect,FALSE> >(InVertexFactory->GetType());
		}
	}
	else
	{
		if (bUsePerspectiveCorrectShadowDepths)
		{
			if (bInUseScreenDoorDefaultMaterialShader)
			{
				// If we were going to get the pixel shader from the default material, use the screen door shader if fading is happening
				PixelShader = DefaultMaterialResource.GetShader<TShadowDepthPixelShader<PixelShadowDepth_PerspectiveCorrect,TRUE> >(InVertexFactory->GetType());
			}
			else
			{
				PixelShader = DefaultMaterialResource.GetShader<TShadowDepthPixelShader<PixelShadowDepth_PerspectiveCorrect,FALSE> >(InVertexFactory->GetType());
			}
		}
		else
		{
			// Platforms that support depth textures use a null pixel shader for opaque materials to get double speed Z
			// Other platforms need to output depth as color and still need the pixel shader
			if (!bInTranslucentPreShadow && (GSceneRenderTargets.IsFetch4Supported() || GSceneRenderTargets.IsHardwarePCFSupported()) || GSupportsDepthTextures)
			{
				if (bInUseScreenDoorDefaultMaterialShader)
				{
					// If we were going to get the pixel shader from the default material, use the screen door shader if fading is happening
					PixelShader = DefaultMaterialResource.GetShader<TShadowDepthPixelShader<PixelShadowDepth_NonPerspectiveCorrect,TRUE> >(InVertexFactory->GetType());
				}
				else
				{
					PixelShader = NULL;
				}
			}
			else
			{
				if (bInUseScreenDoorDefaultMaterialShader)
				{
					// If we were going to get the pixel shader from the default material, use the screen door shader if fading is happening
					PixelShader = DefaultMaterialResource.GetShader<TShadowDepthPixelShader<PixelShadowDepth_NonPerspectiveCorrect,TRUE> >(InVertexFactory->GetType());
				}
				else if (bOnePassPointLightShadow)
				{
					// Don't use a pixel shader for opaque materials rendering one pass point light shadows, since we are outputting non-linear depth (Z/W)
					PixelShader = NULL;
				}
				else
				{
					PixelShader = DefaultMaterialResource.GetShader<TShadowDepthPixelShader<PixelShadowDepth_NonPerspectiveCorrect,FALSE> >(InVertexFactory->GetType());
				}
			}
		}
	}
}

void FShadowDepthDrawingPolicy::DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const
{
	checkSlow(bPreShadow == ShadowInfo->bPreShadow
		&& bFullSceneShadow == ShadowInfo->bFullSceneShadow
		&& bDirectionalLight == ShadowInfo->bDirectionalLight);

	VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*MaterialResource,*View,ShadowInfo);

#if WITH_D3D11_TESSELLATION
	if (GeometryShader)
	{
		GeometryShader->SetParameters(*View,ShadowInfo);
	}

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(MaterialRenderProxy,*View);
		DomainShader->SetParameters(MaterialRenderProxy,*View,ShadowInfo);
	}
#endif

	if (PixelShader)
	{
		PixelShader->SetParameters(MaterialRenderProxy,*MaterialResource,*View,ShadowInfo);
	}

	// Set the shared mesh resources.
	FMeshDrawingPolicy::DrawShared(View);

	// Set the actual shader & vertex declaration state
	RHISetBoundShaderState(BoundShaderState);

	// Set the rasterizer state only once per draw list bucket, instead of once per mesh in SetMeshRenderState as an optimization
	const FRasterizerStateInitializerRHI Initializer = {
		FM_Solid,
		IsTwoSided() ? CM_None :
		// Have to flip culling when doing one pass point light shadows for some reason
		(XOR(View->bReverseCulling, XOR(bReverseCulling, bOnePassPointLightShadow)) ? CM_CCW : CM_CW),
		DepthBias,
		0,
		TRUE
	};

	RHISetRasterizerStateImmediate(Initializer);
}

/** 
 * Create bound shader state using the vertex decl from the mesh draw policy
 * as well as the shaders needed to draw the mesh
 * @param DynamicStride - optional stride for dynamic vertex data
 * @return new bound shader state object
 */
FBoundShaderStateRHIRef FShadowDepthDrawingPolicy::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);
	if (DynamicStride)
	{
		StreamStrides[0] = DynamicStride;
	}
#if WITH_D3D11_TESSELLATION
	return RHICreateBoundShaderStateD3D11(
		VertexDeclaration, 
		StreamStrides, 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		GETSAFERHISHADER_PIXEL(PixelShader),
		GETSAFERHISHADER_GEOMETRY(GeometryShader),
		EGST_None);
#else
	return RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), GETSAFERHISHADER_PIXEL(PixelShader), EGST_None);	
#endif
}

void FShadowDepthDrawingPolicy::SetMeshRenderState(
	const FSceneView& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	UBOOL bBackFace,
	const ElementDataType& ElementData
	) const
{
	EmitMeshDrawEvents(PrimitiveSceneInfo, Mesh);
	VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
#if WITH_D3D11_TESSELLATION

	if (GeometryShader)
	{
		GeometryShader->SetMesh(PrimitiveSceneInfo, ShadowInfo, View);
	}

	if (HullShader && DomainShader)
	{
		HullShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
		DomainShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	}
#endif
	if (PixelShader)
	{
		PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
	}
	// Not calling FMeshDrawingPolicy::SetMeshRenderState as DrawShared sets the rasterizer state
}

#define COMPARESHADOWPARAMS(A, B) \
	if(A < B) { return -1; } \
	else if(A > B) { return +1; }

// Note: this should match the criteria in FProjectedShadowInfo::RenderDepth for deciding when to call DrawShared on a static mesh element for best performance
IMPLEMENT_COMPARE_CONSTREF(FShadowStaticMeshElement,ShadowRendering, \
{  \
	COMPARESHADOWPARAMS(A.Mesh->VertexFactory, B.Mesh->VertexFactory);  \
	COMPARESHADOWPARAMS(A.RenderProxy, B.RenderProxy);  \
	return 0;  \
});

#undef COMPARESHADOWPARAMS

INT Compare(const FShadowDepthDrawingPolicy& A,const FShadowDepthDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
#if WITH_D3D11_TESSELLATION
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
	COMPAREDRAWINGPOLICYMEMBERS(GeometryShader);
#endif
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	COMPAREDRAWINGPOLICYMEMBERS(bPreShadow);
	COMPAREDRAWINGPOLICYMEMBERS(bTranslucentPreShadow);
	COMPAREDRAWINGPOLICYMEMBERS(bFullSceneShadow);
	COMPAREDRAWINGPOLICYMEMBERS(bDirectionalLight);
	COMPAREDRAWINGPOLICYMEMBERS(bUseScreenDoorDefaultMaterialShader);
	// Have to sort on two sidedness because rasterizer state is set in DrawShared and not SetMeshRenderState
	COMPAREDRAWINGPOLICYMEMBERS(bIsTwoSidedMaterial);
	COMPAREDRAWINGPOLICYMEMBERS(bReverseCulling);
	COMPAREDRAWINGPOLICYMEMBERS(bOnePassPointLightShadow);
	return 0;
}

void FShadowDepthDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh)
{
	if (StaticMesh->CastShadow)
	{
		const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial();
		const EBlendMode BlendMode = Material->GetBlendMode();

		if ((!IsTranslucentBlendMode(BlendMode) && BlendMode != BLEND_DitheredTranslucent) || Material->CastLitTranslucencyShadowAsMasked())
		{
			if (!Material->IsMasked() && !Material->IsTwoSided() && !Material->CastLitTranslucencyShadowAsMasked() && !Material->MaterialModifiesMeshPosition())
			{
				// Override with the default material for opaque materials that are not two sided
				MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
			}

			// Add the static mesh to the shadow's subject draw list.
			Scene->DPGs[StaticMesh->DepthPriorityGroup].WholeSceneShadowDepthDrawList.AddMesh(
				StaticMesh,
				FShadowDepthDrawingPolicy::ElementDataType(),
				FShadowDepthDrawingPolicy(
					StaticMesh->VertexFactory,
					MaterialRenderProxy,
					*MaterialRenderProxy->GetMaterial(),
					FALSE,
					FALSE,
					TRUE,
					TRUE,
					FALSE,
					StaticMesh->PrimitiveSceneInfo->bCastShadowAsTwoSided,
					StaticMesh->ReverseCulling,
					FALSE)
				);
		}
	}
}

UBOOL FShadowDepthDrawingPolicyFactory::DrawDynamicMesh(
	const FSceneView& View,
	ContextType Context,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	UBOOL bDirty = FALSE;

	// Use a per-FMeshBatch check on top of the per-primitive check because dynamic primitives can submit multiple FMeshBatches.
	if (Mesh.CastShadow)
	{
		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial();
		const EBlendMode BlendMode = Material->GetBlendMode();

		if ((!IsTranslucentBlendMode(BlendMode) && BlendMode != BLEND_DitheredTranslucent) || Material->CastLitTranslucencyShadowAsMasked() )
		{
			UBOOL bUseScreenDoorDefaultMaterialShader = FALSE;
			if (!Material->IsMasked() && !Material->IsTwoSided() && !Material->CastLitTranslucencyShadowAsMasked() && !Material->MaterialModifiesMeshPosition() )
			{
				// Check to see if the primitive is currently fading in or out using the screen door effect.
				// Only need to check for fading if the default material's shaders are going to be used, otherwise fading will already work.
				const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View.State );
				bUseScreenDoorDefaultMaterialShader =
					SceneViewState != NULL && PrimitiveSceneInfo != NULL &&
					SceneViewState->IsPrimitiveFading( PrimitiveSceneInfo->Component );

				// Override with the default material for opaque materials that are not two sided
				MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
			}

#if PS3 && USE_PS3_PREVERTEXCULLING
			// we can't use prevertex culling because we are rendering from the light's point of view, not the 
			// camera's point of view, so the culling is wrong (it's not as necessary, since the pixel shader
			// here is dead simple)
			extern UBOOL GUseEdgeGeometry;
			UBOOL bSavedUseEdge = GUseEdgeGeometry;
			GUseEdgeGeometry = FALSE;
#endif

			FShadowDepthDrawingPolicy DrawingPolicy(
				Mesh.VertexFactory,
				MaterialRenderProxy,
				*MaterialRenderProxy->GetMaterial(),
				Context.ShadowInfo->bPreShadow,
				Context.bTranslucentPreShadow,
				Context.ShadowInfo->bFullSceneShadow,
				Context.ShadowInfo->bDirectionalLight,
				bUseScreenDoorDefaultMaterialShader,
				PrimitiveSceneInfo->bCastShadowAsTwoSided,
				Mesh.ReverseCulling,
				ShouldRenderOnePassPointLightShadow(Context.ShadowInfo));
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
			{
				DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
				DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
			}
			bDirty = TRUE;

#if PS3 && USE_PS3_PREVERTEXCULLING
			// and restore
			GUseEdgeGeometry = bSavedUseEdge;
#endif
		}
	}
	
	return bDirty;
}

UBOOL FShadowDepthDrawingPolicyFactory::DrawStaticMesh(
	const FViewInfo& View,
	ContextType DrawingContext,
	const FStaticMesh& StaticMesh,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	return DrawDynamicMesh(
		View,
		DrawingContext,
		StaticMesh,
		FALSE,
		bPreFog,
		PrimitiveSceneInfo,
		HitProxyId
		);
}

/*-----------------------------------------------------------------------------
	FProjectedShadowInfo
-----------------------------------------------------------------------------*/

UBOOL GSkipShadowDepthRendering = FALSE;

void FProjectedShadowInfo::RenderDepth(const FSceneRenderer* SceneRenderer, BYTE DepthPriorityGroup, UBOOL bTranslucentPreShadow)
{
#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	if (GEmitDrawEvents)
	{
		const FName ParentName = ParentSceneInfo && ParentSceneInfo->Owner ? ParentSceneInfo->Owner->GetFName() : NAME_None;
		EventName = bFullSceneShadow ? 
			FString(TEXT("Fullscene split ")) + appItoa(SplitIndex) : 
			(ParentName.ToString() + (bPreShadow ? TEXT(" Preshadow") : TEXT("")));
	}
	SCOPED_DRAW_EVENT(EventShadowDepthActor)(DEC_SCENE_ITEMS,*EventName);
#endif
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderWholeSceneShadowDepthsTime, bFullSceneShadow);
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderPerObjectShadowDepthsTime, !bFullSceneShadow);

	const UBOOL bOnePassPointLightShadow = ShouldRenderOnePassPointLightShadow(this);
	if (bOnePassPointLightShadow)
	{
		// Set the viewport to the whole render target since it's a cube map, don't leave any border space
		RHISetViewport(
			0,
			0,
			0.0f,
			ResolutionX,
			ResolutionY,
			1.0f
			);

		// Clear depth only.
		RHIClear(FALSE,FColor(255,255,255),TRUE,1.0f,FALSE,0);
	}
	else
	{
#if WITH_MOBILE_RHI
		if ( GUsingMobileRHI )
		{
			// On mobile we clear the entire shadowbuffer outside the loop.
		}
		else
#endif
		{
			// Set the viewport for the shadow.
			RHISetViewport(
				X,
				Y,
				0.0f,
				X + SHADOW_BORDER*2 + ResolutionX,
				Y + SHADOW_BORDER*2 + ResolutionY,
				1.0f
				);

			if( GSupportsDepthTextures || !bTranslucentPreShadow && (GSceneRenderTargets.IsHardwarePCFSupported() || GSceneRenderTargets.IsFetch4Supported()))
			{
				// Clear depth only.
				RHIClear(FALSE,FColor(255,255,255),TRUE,1.0f,FALSE,0);
			}
			else
			{
				// Clear color and depth.
				RHIClear(TRUE,FColor(255,255,255),TRUE,1.0f,FALSE,0);
			}	
		}

		// Set the viewport for the shadow.
		RHISetViewport(
			X + SHADOW_BORDER,
			Y + SHADOW_BORDER,
			0.0f,
			X + SHADOW_BORDER + ResolutionX,
			Y + SHADOW_BORDER + ResolutionY,
			1.0f
			);
	}

	// Opaque blending, depth tests and writes.
	RHISetBlendState(TStaticBlendState<>::GetRHI());	
	RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());

	// Choose an arbitrary view where this shadow is in the right DPG to use for rendering the depth.
	const FViewInfo* FoundView = NULL;
	ESceneDepthPriorityGroup DPGToUseForDepths = (ESceneDepthPriorityGroup)DepthPriorityGroup;
	FindViewAndDPGForRenderDepth(SceneRenderer->Views, DepthPriorityGroup, LightSceneInfo->Id, bTranslucentPreShadow, FoundView, DPGToUseForDepths);
	// Save off some variables to make it easier to debug in a Release build crash dump:
	static const FSceneRenderer* GSceneRenderer;
	static ESceneDepthPriorityGroup GDPGToUseForDepths;
	static ESceneDepthPriorityGroup GDepthPriorityGroup;
	static FProjectedShadowInfo* GThis;
	GSceneRenderer = SceneRenderer;
	GDPGToUseForDepths = DPGToUseForDepths;
	GDepthPriorityGroup = (ESceneDepthPriorityGroup)DepthPriorityGroup;
	GThis = this;
	check( FoundView );

	RHISetViewParameters(*FoundView);
	RHISetMobileHeightFogParams(FoundView->HeightFogParams);

	if (GSkipShadowDepthRendering)
	{
		return;
	}

	FShadowDepthDrawingPolicy::ShadowInfo = this;

	if (IsWholeSceneDominantShadow())
	{
		checkSlow(SceneRenderer->Scene->NumWholeSceneShadowLights > 0 || GIsEditor);
		// Use the scene's shadow depth draw list with this shadow's visibility map
		SceneRenderer->Scene->DPGs[DepthPriorityGroup].WholeSceneShadowDepthDrawList.DrawVisible(*FoundView, StaticMeshWholeSceneShadowDepthMap);
	}
	else if (SubjectMeshElements.Num() > 0)
	{
		// Draw the subject's static elements.
		const FShadowStaticMeshElement& FirstShadowMesh = SubjectMeshElements(0);
		// Initialize the shared drawing policy from the first static element
		FShadowDepthDrawingPolicy SharedDrawingPolicy(
			FirstShadowMesh.Mesh->VertexFactory,
			FirstShadowMesh.RenderProxy,
			*FirstShadowMesh.RenderProxy->GetMaterial(),
			bPreShadow,
			bTranslucentPreShadow,
			bFullSceneShadow,
			bDirectionalLight,
			FALSE,
			FirstShadowMesh.bIsTwoSided,
			FirstShadowMesh.Mesh->ReverseCulling,
			bOnePassPointLightShadow);

		// Call DrawShared even if the first static element is not visible, since other static elements in the same state group may be visible
		SharedDrawingPolicy.DrawShared(FoundView, SharedDrawingPolicy.CreateBoundShaderState(FirstShadowMesh.Mesh->GetDynamicVertexStride()));
		if (FoundView->StaticMeshShadowDepthMap(FirstShadowMesh.Mesh->Id))
		{
			if( FirstShadowMesh.Mesh->Elements.Num() == 1 )
			{
				SharedDrawingPolicy.SetMeshRenderState(*FoundView, FirstShadowMesh.Mesh->PrimitiveSceneInfo, *FirstShadowMesh.Mesh, 0, FALSE, FMeshDrawingPolicy::ElementDataType());
				SharedDrawingPolicy.DrawMesh(*FirstShadowMesh.Mesh, 0);
			}
			else
			{
				// Render only those batch elements that match the current LOD
				TArray<INT> BatchesToRender;
				BatchesToRender.Empty(FirstShadowMesh.Mesh->Elements.Num());
				FirstShadowMesh.Mesh->VertexFactory->GetStaticBatchElementVisibility(*FoundView, FirstShadowMesh.Mesh, BatchesToRender);

				for (INT Index=0;Index<BatchesToRender.Num();Index++)
				{
					INT BatchElementIndex = BatchesToRender(Index);
					SharedDrawingPolicy.SetMeshRenderState(*FoundView, FirstShadowMesh.Mesh->PrimitiveSceneInfo, *FirstShadowMesh.Mesh, BatchElementIndex, FALSE, FMeshDrawingPolicy::ElementDataType());
					SharedDrawingPolicy.DrawMesh(*FirstShadowMesh.Mesh, BatchElementIndex);
				}
			}
		}

		for (INT ElementIndex = 1; ElementIndex < SubjectMeshElements.Num(); ElementIndex++)
		{
			const FShadowStaticMeshElement& ShadowMesh = SubjectMeshElements(ElementIndex);
			// Only process this static element if it is visible
			if (FoundView->StaticMeshShadowDepthMap(ShadowMesh.Mesh->Id))
			{
				// Create a new drawing policy if this static element has different state than the one that created SharedDrawingPolicy
				// Note: This criteria should match the FShadowStaticMeshElement sort comparison function for best performance
				// Warning: Everything that the FShadowDepthDrawingPolicy ctor or DrawShared functions depend on that changes per FStaticMesh must be compared here
				if (SharedDrawingPolicy.GetMaterialRenderProxy() != ShadowMesh.RenderProxy
					|| SharedDrawingPolicy.GetVertexFactory() != ShadowMesh.Mesh->VertexFactory
					|| SharedDrawingPolicy.IsTwoSided() != ShadowMesh.bIsTwoSided
					|| SharedDrawingPolicy.IsReversingCulling() != ShadowMesh.Mesh->ReverseCulling)
				{
					SharedDrawingPolicy = FShadowDepthDrawingPolicy(
						ShadowMesh.Mesh->VertexFactory,
						ShadowMesh.RenderProxy,
						*ShadowMesh.RenderProxy->GetMaterial(),
						bPreShadow,
						bTranslucentPreShadow,
						bFullSceneShadow,
						bDirectionalLight,
						FALSE,
						ShadowMesh.bIsTwoSided,
						ShadowMesh.Mesh->ReverseCulling,
						bOnePassPointLightShadow);
					// Only call draw shared when the vertex factory or material have changed
					SharedDrawingPolicy.DrawShared(FoundView, SharedDrawingPolicy.CreateBoundShaderState(ShadowMesh.Mesh->GetDynamicVertexStride()));
				}

#if _DEBUG
				FShadowDepthDrawingPolicy DebugPolicy(
					ShadowMesh.Mesh->VertexFactory,
					ShadowMesh.RenderProxy,
					*ShadowMesh.RenderProxy->GetMaterial(),
					bPreShadow,
					bTranslucentPreShadow,
					bFullSceneShadow,
					bDirectionalLight,
					FALSE,
					ShadowMesh.bIsTwoSided,
					ShadowMesh.Mesh->ReverseCulling,
					bOnePassPointLightShadow);
				// Verify that SharedDrawingPolicy can be used to draw this mesh without artifacts by checking the comparison functions that static draw lists use
				checkSlow(DebugPolicy.Matches(SharedDrawingPolicy));
				checkSlow(Compare(DebugPolicy, SharedDrawingPolicy) == 0);
#endif
				if( ShadowMesh.Mesh->Elements.Num() == 1 )
				{
					SharedDrawingPolicy.SetMeshRenderState(*FoundView, ShadowMesh.Mesh->PrimitiveSceneInfo, *ShadowMesh.Mesh, 0, FALSE, FMeshDrawingPolicy::ElementDataType());
					SharedDrawingPolicy.DrawMesh(*ShadowMesh.Mesh, 0);
				}
				else
				{
					// Render only those batch elements that match the current LOD
					TArray<INT> BatchesToRender;
					BatchesToRender.Empty(ShadowMesh.Mesh->Elements.Num());
					ShadowMesh.Mesh->VertexFactory->GetStaticBatchElementVisibility(*FoundView, ShadowMesh.Mesh, BatchesToRender);

					for (INT Index=0;Index<BatchesToRender.Num();Index++)
					{
						INT BatchElementIndex = BatchesToRender(Index);
						SharedDrawingPolicy.SetMeshRenderState(*FoundView, ShadowMesh.Mesh->PrimitiveSceneInfo, *ShadowMesh.Mesh, BatchElementIndex, FALSE, FMeshDrawingPolicy::ElementDataType());
						SharedDrawingPolicy.DrawMesh(*ShadowMesh.Mesh, BatchElementIndex);
					}
				}
			}
		}
	}

	// Draw the subject's dynamic elements.
	TDynamicPrimitiveDrawer<FShadowDepthDrawingPolicyFactory> Drawer(FoundView, DPGToUseForDepths, FShadowDepthDrawingPolicyFactory::ContextType(this, bTranslucentPreShadow), TRUE);
	for (INT PrimitiveIndex = 0; PrimitiveIndex < SubjectPrimitives.Num(); PrimitiveIndex++)
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = SubjectPrimitives(PrimitiveIndex);

		// Lookup the primitive's cached view relevance
		FPrimitiveViewRelevance ViewRelevance = FoundView->PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

		if (!ViewRelevance.bInitializedThisFrame)
		{
			// Compute the subject primitive's view relevance since it wasn't cached
			ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(FoundView);
		}
		
		// Only draw if the subject primitive's shadow is view relevant.
		if (ViewRelevance.IsRelevant() || ViewRelevance.bShadowRelevance)
		{
			Drawer.SetPrimitive(SubjectPrimitives(PrimitiveIndex));
			SubjectPrimitives(PrimitiveIndex)->Proxy->DrawDynamicElements(&Drawer, FoundView, DPGToUseForDepths);
		}
	}

	FShadowDepthDrawingPolicy::ShadowInfo = NULL;
}

/** Renders a sphere, used for stenciling out Cascaded Shadow Map splits. */
void DrawStencilingSphere(const FSphere& Sphere, const FVector& PreViewTranslation)
{
	const INT NumSides = 18;
	const INT NumRings = 12;
	const INT NumVerts = (NumSides + 1) * (NumRings + 1);
	const FLOAT RadiansPerRingSegment = PI / (FLOAT)NumRings;

	static TArray<FVector, TInlineAllocator<NumVerts> > UnitSphereVerts;
	static TArray<WORD, TInlineAllocator<NumSides * NumRings * 6> > Indices;

	// We'll only generate the unit sphere mesh the very first time through
	if( UnitSphereVerts.Num() == 0 )
	{
		TArray<FVector, TInlineAllocator<NumRings + 1> > ArcVerts;
		ArcVerts.Empty(NumRings + 1);
		// Calculate verts for one arc
		for (INT i = 0; i < NumRings + 1; i++)
		{
			const FLOAT Angle = i * RadiansPerRingSegment;
			ArcVerts.AddItem(FVector(0.0f, appSin(Angle), appCos(Angle)));
		}

		UnitSphereVerts.Empty(NumVerts);
		// Then rotate this arc NumSides + 1 times.
		for (INT s = 0; s < NumSides + 1; s++)
		{
			FRotator ArcRotator(0, appTrunc(65536.f * ((FLOAT)s / NumSides)), 0);
			FRotationMatrix ArcRot( ArcRotator );

			for (INT v = 0; v < NumRings + 1; v++)
			{
				UnitSphereVerts.AddItem(ArcRot.TransformFVector(ArcVerts(v)));
			}
		}

		Indices.Empty(NumSides * NumRings * 6);
		// Add triangles for all the vertices generated
		for (INT s = 0; s < NumSides; s++)
		{
			const INT a0start = (s + 0) * (NumRings + 1);
			const INT a1start = (s + 1) * (NumRings + 1);

			for (INT r = 0; r < NumRings; r++)
			{
				Indices.AddItem(a0start + r + 0);
				Indices.AddItem(a1start + r + 0);
				Indices.AddItem(a0start + r + 1);
				Indices.AddItem(a1start + r + 0);
				Indices.AddItem(a1start + r + 1);
				Indices.AddItem(a0start + r + 1);
			}
		}
	}

	const FVector* RESTRICT UnaliasedUnitSphereVerts = UnitSphereVerts.GetData();
	const FVector SpherePosition = Sphere.Center + PreViewTranslation;

	// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
	const FLOAT Radius = Sphere.W / appCos(RadiansPerRingSegment);

	// @todo: Use vertex shader to transform verts to avoid all CPU cost here!
	FVector SphereVerts[ NumVerts ];
	for( INT VertIndex = 0; VertIndex < NumVerts; ++VertIndex )
	{
		SphereVerts[ VertIndex ] = SpherePosition + Radius * UnaliasedUnitSphereVerts[ VertIndex ];
	}

	RHIDrawIndexedPrimitiveUP(PT_TriangleList, 0, NumVerts, Indices.Num() / 3, Indices.GetData(), Indices.GetTypeSize(), SphereVerts, sizeof(FVector));
}

/** Renders a cone with a spherical cap, used for rendering spot lights in deferred passes. */
void DrawStencilingCone(const FMatrix& ConeToWorld, FLOAT ConeAngle, FLOAT SphereRadius, const FVector& PreViewTranslation)
{
	// A side is a line of vertices going from the cone's origin to the edge of its SphereRadius
	const INT NumSides = 18;
	// A slice is a circle of vertices in the cone's XY plane
	const INT NumSlices = 12;

	TArray<FVector, TInlineAllocator<NumSides * NumSlices * 2> > Verts;
	TArray<WORD, TInlineAllocator<(NumSlices - 1) * NumSides * 12> > Indices;

	const FLOAT InvCosRadiansPerSide = 1.0f / appCos(PI / (FLOAT)NumSides);
	// Use Cos(Theta) = Adjacent / Hypotenuse to solve for the distance of the end of the cone along the cone's Z axis
	const FLOAT ZRadius = SphereRadius * appCos(ConeAngle);
	const FLOAT TanConeAngle = appTan(ConeAngle);

	// Generate vertices for the cone shape
	for (INT SliceIndex = 0; SliceIndex < NumSlices; SliceIndex++)
	{
		for (INT SideIndex = 0; SideIndex < NumSides; SideIndex++)
		{
			const FLOAT CurrentAngle = SideIndex * 2 * PI / (FLOAT)NumSides;
			const FLOAT DistanceDownConeDirection = ZRadius * SliceIndex / (FLOAT)(NumSlices - 1);
			// Use Tan(Theta) = Opposite / Adjacent to solve for the radius of this slice
			// Boost the effective radius so that the edges of the circle lie on the cone, instead of the vertices
			const FLOAT SliceRadius = DistanceDownConeDirection * TanConeAngle * InvCosRadiansPerSide;
			// Create a position in the local space of the cone, forming a circle in the XY plane, and offsetting along the Z axis
			const FVector LocalPosition(SliceRadius * appCos(CurrentAngle), SliceRadius * appSin(CurrentAngle), ZRadius * SliceIndex / (FLOAT)(NumSlices - 1));
			// Transform to world space and apply pre-view translation, since these vertices will be used with a shader that has pre-view translation removed
			Verts.AddItem(ConeToWorld.TransformFVector(LocalPosition) + PreViewTranslation);
		}
	}

	// Generate triangles for the vertices of the cone shape
	for (INT SliceIndex = 0; SliceIndex < NumSlices - 1; SliceIndex++)
	{
		for (INT SideIndex = 0; SideIndex < NumSides; SideIndex++)
		{
			const INT CurrentIndex = SliceIndex * NumSides + SideIndex % NumSides;
			const INT NextSideIndex = SliceIndex * NumSides + (SideIndex + 1) % NumSides;
			const INT NextSliceIndex = (SliceIndex + 1) * NumSides + SideIndex % NumSides;
			const INT NextSliceAndSideIndex = (SliceIndex + 1) * NumSides + (SideIndex + 1) % NumSides;
			
			Indices.AddItem(CurrentIndex);
			Indices.AddItem(NextSideIndex);
			Indices.AddItem(NextSliceIndex);
			Indices.AddItem(NextSliceIndex);
			Indices.AddItem(NextSideIndex);
			Indices.AddItem(NextSliceAndSideIndex);
		}
	}

	const INT CapIndexStart = Verts.Num();
	// Radius of the flat cap of the cone
	const FLOAT CapRadius = ZRadius * appTan(ConeAngle);

	// Generate vertices for the spherical cap
	for (INT SliceIndex = 0; SliceIndex < NumSlices; SliceIndex++)
	{
		const FLOAT UnadjustedSliceRadius = CapRadius * SliceIndex / (FLOAT)(NumSlices - 1);
		// Boost the effective radius so that the edges of the circle lie on the cone, instead of the vertices
		const FLOAT SliceRadius = UnadjustedSliceRadius * InvCosRadiansPerSide;
		// Solve for the Z axis distance that this slice should be at using the Pythagorean theorem
		const FLOAT ZDistance = appSqrt(Square(SphereRadius) - Square(UnadjustedSliceRadius));
		for (INT SideIndex = 0; SideIndex < NumSides; SideIndex++)
		{
			const FLOAT CurrentAngle = SideIndex * 2 * PI / (FLOAT)NumSides;
			const FVector LocalPosition(SliceRadius * appCos(CurrentAngle), SliceRadius * appSin(CurrentAngle), ZDistance);
			Verts.AddItem(ConeToWorld.TransformFVector(LocalPosition) + PreViewTranslation);
		}
	}

	// Generate triangles for the vertices of the spherical cap
	for (INT SliceIndex = 0; SliceIndex < NumSlices - 1; SliceIndex++)
	{
		for (INT SideIndex = 0; SideIndex < NumSides; SideIndex++)
		{
			const INT CurrentIndex = SliceIndex * NumSides + SideIndex % NumSides + CapIndexStart;
			const INT NextSideIndex = SliceIndex * NumSides + (SideIndex + 1) % NumSides + CapIndexStart;
			const INT NextSliceIndex = (SliceIndex + 1) * NumSides + SideIndex % NumSides + CapIndexStart;
			const INT NextSliceAndSideIndex = (SliceIndex + 1) * NumSides + (SideIndex + 1) % NumSides + CapIndexStart;

			Indices.AddItem(CurrentIndex);
			Indices.AddItem(NextSliceIndex);
			Indices.AddItem(NextSideIndex);
			Indices.AddItem(NextSideIndex);
			Indices.AddItem(NextSliceIndex);
			Indices.AddItem(NextSliceAndSideIndex);
		}
	}

	RHIDrawIndexedPrimitiveUP(PT_TriangleList, 0, Verts.Num(), Indices.Num() / 3, Indices.GetData(), Indices.GetTypeSize(), Verts.GetData(), Verts.GetTypeSize());
}

FGlobalBoundShaderState FProjectedShadowInfo::MaskBoundShaderState;
FGlobalBoundShaderState FProjectedShadowInfo::PerFragmentMaskBoundShaderState;
TStaticArray<FGlobalBoundShaderState,SFQ_Num> FProjectedShadowInfo::ShadowProjectionBoundShaderStates;
TStaticArray<FGlobalBoundShaderState,SFQ_Num> FProjectedShadowInfo::PerFragmentShadowProjectionBoundShaderStates;
TStaticArray<FGlobalBoundShaderState,SFQ_Num> FProjectedShadowInfo::BranchingPCFBoundShaderStates;
FGlobalBoundShaderState FProjectedShadowInfo::ShadowProjectionHiStencilClearBoundShaderState;
FGlobalBoundShaderState FProjectedShadowInfo::ShadowProjectionPointLightBoundShaderState;

UBOOL GUseHiStencil = TRUE;

void FProjectedShadowInfo::RenderProjection(INT ViewIndex, const FViewInfo* View, BYTE DepthPriorityGroup, UBOOL bRenderingBeforeLight) const
{
	if(!TEST_PROFILEEXSTATE(0x1, View->Family->CurrentRealTime))
	{
		// we want to profile with the feature off
		return;
	}

#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	if (GEmitDrawEvents)
	{
		const FName ParentName = ParentSceneInfo && ParentSceneInfo->Owner ? ParentSceneInfo->Owner->GetFName() : NAME_None;
		EventName = bFullSceneShadow ? 
			FString(TEXT("Fullscene split ")) + appItoa(SplitIndex) : 
			(ParentName.ToString() + TEXT(" ") + appItoa(LightSceneInfo->Id) + (bPreShadow ? TEXT(" Preshadow") : TEXT("")));
	}
	SCOPED_DRAW_EVENT(EventShadowProjectionActor)(DEC_SCENE_ITEMS,*EventName);
#endif
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderWholeSceneShadowProjectionsTime, bFullSceneShadow);
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderPerObjectShadowProjectionsTime, !bFullSceneShadow);
	// Find the shadow's view relevance.
	const FVisibleLightViewInfo& VisibleLightViewInfo = View->VisibleLightInfos(LightSceneInfo->Id);
	FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ShadowId);

	// Don't render shadows for subjects which aren't view relevant.
	if (ViewRelevance.bShadowRelevance == FALSE)
	{
		return;
	}

	if (!ViewRelevance.GetDPG(DepthPriorityGroup) && !bForegroundCastingOnWorld)
	{
		return;
	}

#if WITH_MOBILE_RHI
	if (GUsingMobileRHI && !GSupportsDepthTextures)
	{
		// Only modulated shadows supported
		check(!bRenderingBeforeLight);
		RenderForwardProjection(View, DepthPriorityGroup);
		return;
	}
#endif

	// The shadow transforms and view transforms are relative to different origins, so the world coordinates need to be translated.
	const FVector4 PreShadowToPreViewTranslation(View->PreViewTranslation - PreShadowTranslation,0);

	FVector FrustumVertices[8];
	
	for(UINT Z = 0;Z < 2;Z++)
	{
		for(UINT Y = 0;Y < 2;Y++)
		{
			for(UINT X = 0;X < 2;X++)
			{
				const FVector4 UnprojectedVertex = InvReceiverMatrix.TransformFVector4(
					FVector4(
						(X ? -1.0f : 1.0f),
						(Y ? -1.0f : 1.0f),
						(Z ?  0.0f : 1.0f),
						1.0f
						)
					);
				const FVector ProjectedVertex = UnprojectedVertex / UnprojectedVertex.W + PreShadowToPreViewTranslation;
				FrustumVertices[GetCubeVertexIndex(X,Y,Z)] = ProjectedVertex;
			}
		}
	}

	// Calculate whether the camera is inside the shadow frustum, or the near plane is potentially intersecting the frustum.
	UBOOL bCameraInsideShadowFrustum = TRUE;

	if (IsWholeSceneDominantShadow())
	{
		// Mark that the whole scene dominant shadow projection is in the light attenuation alpha
		GSceneRenderTargets.SetWholeSceneDominantShadowValid(TRUE);
	}
	else
	{
		const FVector FrontTopRight = FrustumVertices[GetCubeVertexIndex(0,0,1)] - View->PreViewTranslation;
		const FVector FrontTopLeft = FrustumVertices[GetCubeVertexIndex(1,0,1)] - View->PreViewTranslation;
		const FVector FrontBottomLeft = FrustumVertices[GetCubeVertexIndex(1,1,1)] - View->PreViewTranslation;
		const FVector FrontBottomRight = FrustumVertices[GetCubeVertexIndex(0,1,1)] - View->PreViewTranslation;
		const FVector BackTopRight = FrustumVertices[GetCubeVertexIndex(0,0,0)] - View->PreViewTranslation;
		const FVector BackTopLeft = FrustumVertices[GetCubeVertexIndex(1,0,0)] - View->PreViewTranslation;
		const FVector BackBottomLeft = FrustumVertices[GetCubeVertexIndex(1,1,0)] - View->PreViewTranslation;
		const FVector BackBottomRight = FrustumVertices[GetCubeVertexIndex(0,1,0)] - View->PreViewTranslation;

		const FPlane Front(FrontTopRight, FrontTopLeft, FrontBottomLeft);
		const FLOAT FrontDistance = Front.PlaneDot(View->ViewOrigin) / FVector(Front).Size();

		const FPlane Right(BackTopRight, FrontTopRight, FrontBottomRight);
		const FLOAT RightDistance = Right.PlaneDot(View->ViewOrigin) / FVector(Right).Size();

		const FPlane Back(BackTopLeft, BackTopRight, BackBottomRight);
		const FLOAT BackDistance = Back.PlaneDot(View->ViewOrigin) / FVector(Back).Size();

		const FPlane Left(FrontTopLeft, BackTopLeft, BackBottomLeft);
		const FLOAT LeftDistance = Left.PlaneDot(View->ViewOrigin) / FVector(Left).Size();

		const FPlane Top(BackTopRight, BackTopLeft, FrontTopLeft);
		const FLOAT TopDistance = Top.PlaneDot(View->ViewOrigin) / FVector(Top).Size();

		const FPlane Bottom(FrontBottomRight, FrontBottomLeft, BackBottomLeft);
		const FLOAT BottomDistance = Bottom.PlaneDot(View->ViewOrigin) / FVector(Bottom).Size();

		// Use a distance threshold to treat the case where the near plane is intersecting the frustum as the camera being inside
		// The near plane handling is not exact since it just needs to be conservative about saying the camera is outside the frustum
		const FLOAT DistanceThreshold = -View->NearClippingDistance * 3.0f;
		bCameraInsideShadowFrustum = 
			FrontDistance > DistanceThreshold && 
			RightDistance > DistanceThreshold && 
			BackDistance > DistanceThreshold && 
			LeftDistance > DistanceThreshold && 
			TopDistance > DistanceThreshold && 
			BottomDistance > DistanceThreshold;
	}

	// Find the projection shaders.
	TShaderMapRef<FShadowProjectionVertexShader> VertexShader(GetGlobalShaderMap());

	// Shadow types that setup stencil based on the intersection with the scene set this to TRUE.
	// Shadow types that just mask out an area set this to FALSE.
	// This is used to know what stencil value to test against later.
	UBOOL bStenciledIntersectionWithScene = FALSE;

	if( GSystemSettings.bEnableForegroundShadowsOnWorld &&
		DepthPriorityGroup == SDPG_Foreground &&
		LightSceneInfo->LightShadowMode == LightShadow_Modulate )
	{
		SCOPED_DRAW_EVENT(EventMaskSubjects)(DEC_SCENE_ITEMS,TEXT("Stencil Mask Subjects"));

		// For foreground subjects, only do self-shadowing in the foreground DPG. 
		// Shadowing from the foreground subjects onto the world will have been done in the world DPG,
		// to take advantage of the world DPG's depths being in the depth buffer,
		// and therefore the intersection of the shadow frustum and the scene can be stenciled.
		if (GUseHiStencil)
		{
			RHIBeginHiStencilRecord(TRUE, 0);
		}

		RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
		RHISetColorWriteEnable(FALSE);

		// Set stencil to one.
		RHISetStencilState(TStaticStencilState<
			TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
			FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff,1
			>::GetRHI());

		// Draw the subject's dynamic elements
		TDynamicPrimitiveDrawer<FDepthDrawingPolicyFactory> Drawer(View,SDPG_Foreground,FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders),TRUE);
		for(INT PrimitiveIndex = 0;PrimitiveIndex < SubjectPrimitives.Num();PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* SubjectPrimitiveSceneInfo = SubjectPrimitives(PrimitiveIndex);
			if(View->PrimitiveVisibilityMap(SubjectPrimitiveSceneInfo->Id))
			{
				Drawer.SetPrimitive(SubjectPrimitiveSceneInfo);
				SubjectPrimitiveSceneInfo->Proxy->DrawDynamicElements(&Drawer,View,SDPG_Foreground);
			}
		}

		if (GUseHiStencil)
		{
			RHIBeginHiStencilPlayback(TRUE);
		}
	}
	else
	{
		// Depth test wo/ writes, no color writing.
		RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
		RHISetColorWriteEnable(FALSE);
		RHISetBlendState(TStaticBlendState<>::GetRHI());	

		if (GUseHiStencil)
		{
			RHIBeginHiStencilRecord(TRUE, 0);
		}

		// If this is a preshadow, mask the projection by the receiver primitives.
		if (bPreShadow)
		{
			SCOPED_DRAW_EVENT(EventMaskSubjects)(DEC_SCENE_ITEMS,TEXT("Stencil Mask Subjects"));

			// Set stencil to one.
			RHISetStencilState(TStaticStencilState<
				TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
				FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
				0xff,0xff,1
				>::GetRHI());

			// Draw the receiver's dynamic elements.
			TDynamicPrimitiveDrawer<FDepthDrawingPolicyFactory> Drawer(View,DepthPriorityGroup,FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders),TRUE);
			for(INT PrimitiveIndex = 0;PrimitiveIndex < ReceiverPrimitives.Num();PrimitiveIndex++)
			{
				const FPrimitiveSceneInfo* ReceiverPrimitiveSceneInfo = ReceiverPrimitives(PrimitiveIndex);
				if(View->PrimitiveVisibilityMap(ReceiverPrimitiveSceneInfo->Id))
				{
					const FPrimitiveViewRelevance& ViewRelevance = View->PrimitiveViewRelevanceMap(ReceiverPrimitiveSceneInfo->Id);
					if( ViewRelevance.bDynamicRelevance )
					{
						Drawer.SetPrimitive(ReceiverPrimitiveSceneInfo);
						ReceiverPrimitiveSceneInfo->Proxy->DrawDynamicElements(&Drawer,View,DepthPriorityGroup);
					}
					if( ViewRelevance.bStaticRelevance )
					{
						for( INT StaticMeshIdx=0; StaticMeshIdx < ReceiverPrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++ )
						{
							const FStaticMesh& StaticMesh = ReceiverPrimitiveSceneInfo->StaticMeshes(StaticMeshIdx);
							if (View->StaticMeshVisibilityMap(StaticMesh.Id))
							{
								FDepthDrawingPolicyFactory::DrawStaticMesh(
									*View,
									FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders),
									StaticMesh,
									TRUE,
									ReceiverPrimitiveSceneInfo,
									StaticMesh.HitProxyId
									);
							}
						}
					}
				}
			}
 		}
		// If this shadow should only self-shadow, mask the projection by the subject's pixels.
		else if ((bSelfShadowOnly || LightSceneInfo->bNonModulatedSelfShadowing && bRenderingBeforeLight)
			// Skip setting up a stencil mask if this light supports deferred,
			// We can use the G buffer lighting channel as a mask in that case which avoids having to re-render meshes
			&& (!ShouldUseDeferredShading() || LightSceneInfo->LightingChannels.GetDeferredShadingChannelMask() == 0))
		{
			checkSlow(!bPreShadow && !bFullSceneShadow);
			SCOPED_DRAW_EVENT(EventMaskSubjects)(DEC_SCENE_ITEMS,TEXT("Stencil Mask Subjects"));

			// Set stencil to one.
			RHISetStencilState(TStaticStencilState<
				TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
				FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
				0xff,0xff,1
				>::GetRHI());

			// Draw the subject's dynamic opaque elements
			const EDepthDrawingMode DrawingMode = DDM_AllOccluders;
			TDynamicPrimitiveDrawer<FDepthDrawingPolicyFactory> Drawer(View,DepthPriorityGroup,FDepthDrawingPolicyFactory::ContextType(DrawingMode),TRUE);
			for(INT PrimitiveIndex = 0;PrimitiveIndex < SubjectPrimitives.Num();PrimitiveIndex++)
			{
				const FPrimitiveSceneInfo* SubjectPrimitiveSceneInfo = SubjectPrimitives(PrimitiveIndex);
				if(View->PrimitiveVisibilityMap(SubjectPrimitiveSceneInfo->Id))
				{
					Drawer.SetPrimitive(SubjectPrimitiveSceneInfo);
					SubjectPrimitiveSceneInfo->Proxy->DrawDynamicElements(&Drawer,View,DepthPriorityGroup);
				}
			}
		}
		else if (IsWholeSceneDominantShadow())
		{
			bStenciledIntersectionWithScene = TRUE;

			// Increment stencil on front-facing zfail, decrement on back-facing zfail.
			RHISetStencilState(TStaticStencilState<
				TRUE,CF_Always,SO_Keep,SO_Increment,SO_Keep,
				TRUE,CF_Always,SO_Keep,SO_Decrement,SO_Keep,
				0xff,0xff
				>::GetRHI());

			RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

			checkSlow(SplitIndex >= 0);
			checkSlow(bDirectionalLight);

			const FSphere SplitBounds = LightSceneInfo->GetShadowSplitBounds(*View, SplitIndex);

			VertexShader->SetParameters(*View, this);

			SetGlobalBoundShaderState(MaskBoundShaderState,GShadowFrustumVertexDeclaration.VertexDeclarationRHI,*VertexShader,NULL,sizeof(FVector));

			// Setup stencil so that the next step will only project shadows inside the bounding sphere of the CSM split
			DrawStencilingSphere(SplitBounds, View->PreViewTranslation);

			if (SplitIndex > 0)
			{
				const INT PreviousSplitIndex = SplitIndex - 1;
				// Decrement stencil on front-facing zfail, increment on back-facing zfail.
				RHISetStencilState(TStaticStencilState<
					TRUE,CF_Always,SO_Keep,SO_Decrement,SO_Keep,
					TRUE,CF_Always,SO_Keep,SO_Increment,SO_Keep,
					0xff,0xff
					>::GetRHI());

				const FSphere OtherSplitBounds = LightSceneInfo->GetShadowSplitBounds(*View, PreviousSplitIndex);

				VertexShader->SetParameters(*View, this);

				SetGlobalBoundShaderState(MaskBoundShaderState,GShadowFrustumVertexDeclaration.VertexDeclarationRHI,*VertexShader,NULL,sizeof(FVector));

				// Mark the pixels inside the split closer to the camera so that those pixels will not be projected on in the next step
				DrawStencilingSphere(OtherSplitBounds, View->PreViewTranslation);
			}
		}
		// Not bSelfShadowOnly or a preshadow, mask the projection to any pixels inside the frustum.
		else
		{
			// Solid rasterization wo/ backface culling.
			RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());

			bStenciledIntersectionWithScene = TRUE;

			if (bCameraInsideShadowFrustum)
			{
				// Use zfail stenciling when the camera is inside the frustum or the near plane is potentially clipping, 
				// Because zfail handles these cases while zpass does not.
				// zfail stenciling is somewhat slower than zpass because on xbox 360 HiZ will be disabled when setting up stencil.
				// Increment stencil on front-facing zfail, decrement on back-facing zfail.
				RHISetStencilState(TStaticStencilState<
					TRUE,CF_Always,SO_Keep,SO_Increment,SO_Keep,
					TRUE,CF_Always,SO_Keep,SO_Decrement,SO_Keep,
					0xff,0xff
					>::GetRHI());
			}
			else
			{
				// Increment stencil on front-facing zpass, decrement on back-facing zpass.
				// HiZ will be enabled on xbox 360 which will save a little GPU time.
				RHISetStencilState(TStaticStencilState<
					TRUE,CF_Always,SO_Keep,SO_Keep,SO_Increment,
					TRUE,CF_Always,SO_Keep,SO_Keep,SO_Decrement,
					0xff,0xff
					>::GetRHI());
			}
			
			// Set the projection vertex shader parameters
			VertexShader->SetParameters(*View, this);

			// Cache the bound shader state
			SetGlobalBoundShaderState(MaskBoundShaderState,GShadowFrustumVertexDeclaration.VertexDeclarationRHI,*VertexShader,NULL,sizeof(FVector),0,EGST_PositionOnly);

			// Draw the frustum using the stencil buffer to mask just the pixels which are inside the shadow frustum.
			RHIDrawIndexedPrimitiveUP( PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(WORD), FrustumVertices, sizeof(FVector));

			if (bForegroundCastingOnWorld)
			{
				// We are rendering the shadow from a foreground DPG subject onto the world DPG,
				// so setup a stencil mask for the subject so that we don't waste shadow computations on those pixels,
				// which will just be overwritten later, in the foreground DPG.
				SCOPED_DRAW_EVENT(EventMaskSubjects)(DEC_SCENE_ITEMS,TEXT("Stencil Mask Subjects"));

				// No depth test or writes 
				RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

				// Set stencil to zero.
				RHISetStencilState(TStaticStencilState<
					TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
					FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
					0xff,0xff,0
					>::GetRHI());

				// Draw the subject's dynamic opaque elements
				const EDepthDrawingMode DrawingMode = DDM_AllOccluders;
				TDynamicPrimitiveDrawer<FDepthDrawingPolicyFactory> Drawer(View,SDPG_Foreground,FDepthDrawingPolicyFactory::ContextType(DrawingMode),TRUE);
				for(INT PrimitiveIndex = 0;PrimitiveIndex < SubjectPrimitives.Num();PrimitiveIndex++)
				{
					const FPrimitiveSceneInfo* SubjectPrimitiveSceneInfo = SubjectPrimitives(PrimitiveIndex);
					if(View->PrimitiveVisibilityMap(SubjectPrimitiveSceneInfo->Id))
					{
						Drawer.SetPrimitive(SubjectPrimitiveSceneInfo);
						SubjectPrimitiveSceneInfo->Proxy->DrawDynamicElements(&Drawer,View,SDPG_Foreground);
					}
				}
			}
			else if (LightSceneInfo->bNonModulatedSelfShadowing 
				&& !bRenderingBeforeLight
				// Skip setting up a stencil mask if this light supports deferred,
				// We can use the G buffer lighting channel as a mask in that case which avoids having to re-render meshes
				&& (!ShouldUseDeferredShading() || LightSceneInfo->LightingChannels.GetDeferredShadingChannelMask() == 0))
			{
				// If we are rendering the modulated shadow and the light is using normal shadowing for self shadowing,
				// Mask out the subjects so they will not be projected onto.
				SCOPED_DRAW_EVENT(EventMaskSubjects)(DEC_SCENE_ITEMS,TEXT("Stencil Mask Subjects"));

				// Set stencil to zero.
				RHISetStencilState(TStaticStencilState<
					TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
					FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
					0xff,0xff,0
					>::GetRHI());

				// Draw the subject's dynamic opaque elements
				const EDepthDrawingMode DrawingMode = DDM_AllOccluders;
				TDynamicPrimitiveDrawer<FDepthDrawingPolicyFactory> Drawer(View,DepthPriorityGroup,FDepthDrawingPolicyFactory::ContextType(DrawingMode),TRUE);
				for(INT PrimitiveIndex = 0;PrimitiveIndex < SubjectPrimitives.Num();PrimitiveIndex++)
				{
					const FPrimitiveSceneInfo* SubjectPrimitiveSceneInfo = SubjectPrimitives(PrimitiveIndex);
					if(View->PrimitiveVisibilityMap(SubjectPrimitiveSceneInfo->Id))
					{
						Drawer.SetPrimitive(SubjectPrimitiveSceneInfo);
						SubjectPrimitiveSceneInfo->Proxy->DrawDynamicElements(&Drawer,View,DepthPriorityGroup);
					}
				}
			}
		}

		if (GUseHiStencil)
		{
			RHIBeginHiStencilPlayback(TRUE);
		}
	}	//DPGIndex != SDPG_Foreground

	// no depth test or writes, solid rasterization w/ back-face culling.
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

	if (GOnePassDominantLight && IsDominantLightType(LightSceneInfo->LightType) && !IsWholeSceneDominantShadow())
	{
		const INT ChannelIndex = View->DominantLightChannelAllocator.GetLightChannel(LightSceneInfo->Id);
		// Only write to the allowed light attenuation channel
		RHISetColorWriteMask(1 << ChannelIndex);
	}
	else
	{
		RHISetColorWriteEnable(TRUE);
	}

	RHISetRasterizerState(
		View->bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI());

	// Test stencil for non-zero.
	// Note: Writing stencil while testing disables stencil cull on all Nvidia cards! (including RSX) It's better to just clear instead.
//@DEBUG:
#if VISUALIZE_SHADOW_DEPTH
	RHISetStencilState(TStaticStencilState<
		FALSE,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep,
		FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
		0xff,0xff,0
	>::GetRHI());
#else
	RHISetStencilState(TStaticStencilState<
		TRUE,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep,
		FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
		0xff,0xff,0
	>::GetRHI());
#endif


	const UBOOL bModulatedShadow = !(LightSceneInfo->bNonModulatedSelfShadowing && bRenderingBeforeLight) && 
		LightSceneInfo->LightShadowMode == LightShadow_Modulate;

	if(bModulatedShadow)
	{
		// modulated blending, preserve alpha
//@DEBUG:
#if VISUALIZE_SHADOW_DEPTH
		RHISetBlendState(TStaticBlendState<>::GetRHI());
#else
		RHISetBlendState(TStaticBlendState<BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI());
#endif

		if(ShouldUseBranchingPCF(LightSceneInfo->ShadowProjectionTechnique))
		{
			// Set the Branching PCF modulated shadow projection shaders.
			TShaderMapRef<FModShadowProjectionVertexShader> ModShadowProjVertexShader(GetGlobalShaderMap());
			FBranchingPCFProjectionPixelShaderInterface* BranchingPCFModShadowProjPixelShader = LightSceneInfo->GetBranchingPCFModProjPixelShader(bRenderingBeforeLight);
			checkSlow(BranchingPCFModShadowProjPixelShader);
			
			ModShadowProjVertexShader->SetParameters(*View,this);
			BranchingPCFModShadowProjPixelShader->SetParameters(ViewIndex,*View,this);

			SetGlobalBoundShaderState( *LightSceneInfo->GetBranchingPCFModProjBoundShaderState(bRenderingBeforeLight), GShadowFrustumVertexDeclaration.VertexDeclarationRHI, 
				*ModShadowProjVertexShader, BranchingPCFModShadowProjPixelShader, sizeof(FVector), NULL, EGST_PositionOnly);
		}
		else
		{
			// Set the modulated shadow projection shaders.
			TShaderMapRef<FModShadowProjectionVertexShader> ModShadowProjVertexShader(GetGlobalShaderMap());
			
			FShadowProjectionPixelShaderInterface* ModShadowProjPixelShader = LightSceneInfo->GetModShadowProjPixelShader(bRenderingBeforeLight);
			checkSlow(ModShadowProjPixelShader);

			ModShadowProjVertexShader->SetParameters(*View,this);
			ModShadowProjPixelShader->SetParameters(ViewIndex,*View,this);

			SetGlobalBoundShaderState( *LightSceneInfo->GetModShadowProjBoundShaderState(bRenderingBeforeLight), GShadowFrustumVertexDeclaration.VertexDeclarationRHI, 
				*ModShadowProjVertexShader, ModShadowProjPixelShader, sizeof(FVector), NULL, EGST_ShadowProjection);
		}
	}
	else
	{
		// When rendering preshadows in the foreground DPG on shadows from a dominant light being rendered before the base pass,
		// Overwrite the existing results because we never cleared the light attenuation buffer.
		if (GOnePassDominantLight 
			&& bPreShadow 
			&& DepthPriorityGroup == SDPG_Foreground 
			&& IsDominantLightType(LightSceneInfo->LightType))
		{
			RHISetBlendState(TStaticBlendState<>::GetRHI());
		}
		else
		{
			if (IsWholeSceneDominantShadow())
			{
				// Store whole scene dominant shadows to Alpha, preserve RGB which has per object shadows
				RHISetBlendState(TStaticBlendState<BO_Add,BF_Zero,BF_One,BO_Add,BF_Zero,BF_SourceAlpha>::GetRHI());
			}
			else
			{
				// Use modulated blending to RGB since shadows may overlap, preserve Alpha
				RHISetBlendState(TStaticBlendState<BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI());
			}
		}
	
		// Set the shadow projection vertex shader.
		VertexShader->SetParameters(*View, this);

		if (ShouldUseBranchingPCF(LightSceneInfo->ShadowProjectionTechnique))
		{
			// Branching PCF shadow projection pixel shader
			FShader* BranchingPCFPixelShader = SetBranchingPCFParameters(ViewIndex,*View,this,LightSceneInfo->ShadowFilterQuality);

			FGlobalBoundShaderState* CurrentBPCFBoundShaderState = ChooseBoundShaderState(LightSceneInfo->ShadowFilterQuality,BranchingPCFBoundShaderStates);
			
			SetGlobalBoundShaderState(*CurrentBPCFBoundShaderState, GShadowFrustumVertexDeclaration.VertexDeclarationRHI, 
				*VertexShader, BranchingPCFPixelShader, sizeof(FVector));
		} 
		else
		{
			// On SM5 hardware, run a per-sample shader on pixels with significantly different depth samples.
			if (GRHIShaderPlatform == SP_PCD3D_SM5 && GSystemSettings.UsesMSAA())
			{
				SCOPED_DRAW_EVENT(EventRenderPerSample)(DEC_SCENE_ITEMS,TEXT("PerSample Shadow"));

				// Force testing against 1 if we didn't stencil the intersection with the scene
				if (bCameraInsideShadowFrustum || !bStenciledIntersectionWithScene)
				{
					// Test stencil for 1 and write 2 to stencil.
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Increment,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff,1
						>::GetRHI());
				}
				else
				{
					// Test stencil for -1 and write -2 to stencil.
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Decrement,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff,255
						>::GetRHI());
				}

				// Use the per-fragment mask pixel shader.
				TShaderMapRef<FShadowPerFragmentMaskPixelShader> MaskPixelShader(GetGlobalShaderMap());
				SetGlobalBoundShaderState(PerFragmentMaskBoundShaderState,GShadowFrustumVertexDeclaration.VertexDeclarationRHI,*VertexShader,*MaskPixelShader,sizeof(FVector));
				MaskPixelShader->SetParameters(*View);

				// Draw the frustum using the projection shader..
				RHISetColorWriteEnable(FALSE);
				RHIDrawIndexedPrimitiveUP( PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(WORD), FrustumVertices, sizeof(FVector));
				RHISetColorWriteEnable(TRUE);

				// Use the per-fragment shadow pixel shader.
				FShadowProjectionPixelShaderInterface* PerFragmentPixelShader = GetProjPixelShaderRef(LightSceneInfo->ShadowFilterQuality,TRUE);
				FGlobalBoundShaderState* PerFragmentBoundShaderState = ChooseBoundShaderState(LightSceneInfo->ShadowFilterQuality,PerFragmentShadowProjectionBoundShaderStates);
				SetGlobalBoundShaderState(
					*PerFragmentBoundShaderState,
					GShadowFrustumVertexDeclaration.VertexDeclarationRHI, 
					*VertexShader,
					PerFragmentPixelShader,
					sizeof(FVector)
					);
				PerFragmentPixelShader->SetParameters(ViewIndex,*View,this);

				if(bCameraInsideShadowFrustum || !bStenciledIntersectionWithScene)
				{
					// Test stencil for 1.
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff,1
						>::GetRHI());
				}
				else
				{
					// Test stencil for -1.
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff,255
						>::GetRHI());
				}

				// Draw the frustum using the projection shader..
				RHIDrawIndexedPrimitiveUP( PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(WORD), FrustumVertices, sizeof(FVector));

				if(bCameraInsideShadowFrustum || !bStenciledIntersectionWithScene)
				{
					// Test stencil for 2.
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff,2
						>::GetRHI());
				}
				else
				{
					// Test stencil for -2.
					RHISetStencilState(TStaticStencilState<
						TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
						FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff,254
						>::GetRHI());
				}
			}

			FShadowProjectionPixelShaderInterface * PixelShader = GetProjPixelShaderRef(LightSceneInfo->ShadowFilterQuality,FALSE);
			PixelShader->SetParameters(ViewIndex,*View,this);

			FGlobalBoundShaderState* CurrentBoundShaderState = ChooseBoundShaderState(LightSceneInfo->ShadowFilterQuality,ShadowProjectionBoundShaderStates);

			SetGlobalBoundShaderState(*CurrentBoundShaderState, GShadowFrustumVertexDeclaration.VertexDeclarationRHI, 
				*VertexShader, PixelShader, sizeof(FVector));
		}
	}

	// Draw the shadow by drawing the frustum with the projection shader.
	RHIDrawIndexedPrimitiveUP( PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(WORD), FrustumVertices, sizeof(FVector));

#if XBOX
	// If the camera is inside the shadow frustum, or if the shadow is fairly large on the screen, use a full screen stencil clear as that will be faster
	if (bCameraInsideShadowFrustum || MaxScreenPercent > .7f)
	{
		// Clear the stencil buffer to 0.
		RHIClear(FALSE,FColor(0,0,0),FALSE,0,TRUE,0);
	}
	else
	{
		// If the camera is outside the shadow frustum, re-render the shadow frustum and just write to stencil as that will be faster than clearing stencil
		// Write 0 to any pixels whose stencil value is non-zero
		RHISetStencilState(TStaticStencilState<
			TRUE,CF_NotEqual,SO_Keep,SO_Keep,SO_Replace,
			FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff,0
			>::GetRHI());

		RHISetColorWriteEnable(FALSE);
		// Enable HiZ tests to save on fillrate
		// This works because we only changed stencil to be non-zero for pixels inside the shadow frustum earlier
		RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());

		// Draw the frontfaces of the shadow frustum to save fillrate, 
		// Which is valid because this clear method is only used when the camera is outside the frustum
		RHISetRasterizerState(
			View->bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI());

		if (GUseHiStencil)
		{
			// Set HiStencil to 'Don't cull'
			RHIBeginHiStencilRecord(TRUE, 0);
		}

		// Use the projection vertex shader and a NULL pixel shader to get double speed Z where supported
		SetGlobalBoundShaderState(ShadowProjectionHiStencilClearBoundShaderState, GShadowFrustumVertexDeclaration.VertexDeclarationRHI, 
			*VertexShader, NULL, sizeof(FVector));

		// Render the shadow frustum geometry again
		RHIDrawIndexedPrimitiveUP( PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(WORD), FrustumVertices, sizeof(FVector));
		
		// Restore default states
		if (GUseHiStencil)
		{
			RHIBeginHiStencilPlayback(TRUE);
		}
	}
#else
	// Clear the stencil buffer to 0.
	RHIClear(FALSE,FColor(0,0,0),FALSE,0,TRUE,0);
#endif

	// Reset the depth and stencil state.
	RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
	RHISetStencilState(TStaticStencilState<>::GetRHI());
	RHISetColorWriteEnable(TRUE);

	if (GUseHiStencil)
	{
		RHIEndHiStencil();
	}

	// Fetch4 was enabled on one of the samplers and it needs to be reset to the default state
	RHIClearSamplerBias();
}

/** Render one pass point light shadow projections. */
void FProjectedShadowInfo::RenderOnePassPointLightProjection(INT ViewIndex, const FViewInfo& View, BYTE DepthPriorityGroup) const
{
	SCOPE_CYCLE_COUNTER(STAT_RenderWholeSceneShadowProjectionsTime);

	checkSlow(ShouldRenderOnePassPointLightShadow(this));
	
	const FSphere LightBounds = LightSceneInfo->GetBoundingSphere();

	// Use modulated blending to RGB since shadows may overlap, preserve Alpha
	RHISetBlendState(TStaticBlendState<BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI());

	const UBOOL bCameraInsideLightGeometry = ((FVector)View.ViewOrigin - LightBounds.Center).SizeSquared() < Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f);

	if (bCameraInsideLightGeometry)
	{
		RHISetDepthState(TStaticDepthState<FALSE, CF_Always>::GetRHI());
		// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light geometry
		RHISetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI());
	}
	else
	{
		// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light geometry
		RHISetDepthState(TStaticDepthState<FALSE, CF_LessEqual>::GetRHI());
		RHISetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI());
	}

	TShaderMapRef<FShadowProjectionVertexShader> VertexShader(GetGlobalShaderMap());
	VertexShader->SetParameters(View, this);

	TShaderMapRef<FOnePassPointShadowProjectionPixelShader> PixelShader(GetGlobalShaderMap());
	PixelShader->SetParameters(ViewIndex, View, this);

	SetGlobalBoundShaderState(
		ShadowProjectionPointLightBoundShaderState,
		GShadowFrustumVertexDeclaration.VertexDeclarationRHI,
		*VertexShader,
		*PixelShader,
		sizeof(FVector)
		);

	// Project the point light shadow with some approximately bounding geometry, 
	// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
	DrawStencilingSphere(LightBounds, View.PreViewTranslation);
}

/** Renders the subjects of the shadow with a matrix that flattens the vertices onto a plane. */
void FProjectedShadowInfo::RenderPlanarProjection(const class FSceneRenderer* SceneRenderer, BYTE DPGIndex) const
{
#if WITH_MOBILE_RHI
	if( GUsingMobileRHI )
	{
		// From ES2RHIImplementation.cpp
		extern UBOOL GMobileRenderingShadowDepth;
		extern UBOOL GMobileRenderingDepthOnly;
		GMobileRenderingDepthOnly = TRUE;
		GMobileRenderingShadowDepth = TRUE;

		// Only standard per-object shadows are supported
		checkSlow(!bPreShadow && !bFullSceneShadow && ParentSceneInfo);

		// stencil test passes if != 0 and writes 0 to cull pixels for where we don't want shadow
		RHISetStencilState(TStaticStencilState<
			TRUE,CF_NotEqual,SO_Keep,SO_Keep,SO_Replace,
			FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff,1>::GetRHI());

		UBOOL bStencilDirty = FALSE;

		FShadowDepthDrawingPolicy::ShadowInfo = this;
		for (INT ViewIndex = 0; ViewIndex < SceneRenderer->Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = SceneRenderer->Views(ViewIndex);

			// Modulate existing color with the modulated shadow color using the blend factor
			const FLinearColor ModShadowColor = Lerp(FLinearColor::White, LightSceneInfo->ModShadowColor, FadeAlphas(ViewIndex));
			const FBlendStateInitializerRHI BlendState(BO_Add, BF_Zero, BF_ConstantBlendColor, BO_Add, BF_One, BF_Zero, CF_Always, 255, ModShadowColor);
			RHISetBlendState(RHICreateBlendState(BlendState));

			const FPlane ShadowPlane = LightSceneInfo->GetShadowPlane();

			// Translate the shadow plane by the PreViewTranslation since it will be applied to vertices also translated by PreViewTranslation
			const FPlane TranslatedShadowPlane(ShadowPlane.X, ShadowPlane.Y, ShadowPlane.Z, ShadowPlane.W + ((FVector)ShadowPlane | View.PreViewTranslation));
			const FVector LightDirection = (ParentSceneInfo->Bounds.Origin - (FVector)LightSceneInfo->GetPosition()).SafeNormal();
			// Treat the light like a directional light (W = 0) regardless of what type it really is
			const FVector4 LightPosition(View.ViewOrigin + View.PreViewTranslation - HALF_WORLD_MAX * LightDirection, 0.0f);

			const FLOAT PlaneDot = TranslatedShadowPlane.PlaneDot(LightPosition);
			// Create a matrix which flattens meshes onto a plane
			// See the docs for D3DXMatrixShadow
			FMatrix FlattenMatrix(
				FPlane(
					PlaneDot - LightPosition.X * TranslatedShadowPlane.X, 
					0.0f - LightPosition.X * TranslatedShadowPlane.Y, 
					0.0f - LightPosition.X * TranslatedShadowPlane.Z, 
					0.0f + LightPosition.X * TranslatedShadowPlane.W),
				FPlane(
					0.0f - LightPosition.Y * TranslatedShadowPlane.X, 
					PlaneDot - LightPosition.Y * TranslatedShadowPlane.Y, 
					0.0f - LightPosition.Y * TranslatedShadowPlane.Z, 
					0.0f + LightPosition.Y * TranslatedShadowPlane.W),
				FPlane(
					0.0f  - LightPosition.Z * TranslatedShadowPlane.X, 
					0.0f - LightPosition.Z * TranslatedShadowPlane.Y, 
					PlaneDot - LightPosition.Z * TranslatedShadowPlane.Z, 
					0.0f + LightPosition.Z * TranslatedShadowPlane.W),
				FPlane(
					0.0f - LightPosition.W * TranslatedShadowPlane.X, 
					0.0f - LightPosition.W * TranslatedShadowPlane.Y, 
					0.0f - LightPosition.W * TranslatedShadowPlane.Z, 
					PlaneDot + LightPosition.W * TranslatedShadowPlane.W));

			// Transpose for OpenGL
			FlattenMatrix = FlattenMatrix.Transpose();

			// Set the device viewport for the view.
			RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
			// Merge the flattening matrix into the view projection matrix
			RHISetViewParametersWithOverrides(View, FlattenMatrix * View.TranslatedViewProjectionMatrix, View.DiffuseOverrideParameter, View.SpecularOverrideParameter);
			RHISetMobileHeightFogParams(View.HeightFogParams);

			// Draw the subject's dynamic elements.
			TDynamicPrimitiveDrawer<FShadowDepthDrawingPolicyFactory> Drawer(&View, DPGIndex, FShadowDepthDrawingPolicyFactory::ContextType(this, FALSE), TRUE);
			for (INT PrimitiveIndex = 0; PrimitiveIndex < SubjectPrimitives.Num(); PrimitiveIndex++)
			{
				Drawer.SetPrimitive(SubjectPrimitives(PrimitiveIndex));
				SubjectPrimitives(PrimitiveIndex)->Proxy->DrawDynamicElements(&Drawer, &View, DPGIndex);
			}
			bStencilDirty = bStencilDirty || Drawer.IsDirty();
		}
		FShadowDepthDrawingPolicy::ShadowInfo = NULL;

		if (bStencilDirty)
		{
			RHIClear(FALSE,FColor(0,0,0),FALSE,0,TRUE,0);
		}
		RHISetStencilState(TStaticStencilState<>::GetRHI());

		GMobileRenderingShadowDepth = FALSE;
		GMobileRenderingDepthOnly = FALSE;
	}
#endif	// WITH_MOBILE_RHI
}

/** 
 * Renders a dynamic shadow using a forward projection. 
 * This works when depth textures are not supported but causes many meshes to be re-rendered and pixels to be shaded.
 */
void FProjectedShadowInfo::RenderForwardProjection(const class FViewInfo* View, BYTE DepthPriorityGroup) const
{
#if WITH_MOBILE_RHI
	
	extern UBOOL GMobileRenderingForwardShadowProjections;
	GMobileRenderingForwardShadowProjections = TRUE;

	RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
	RHISetColorWriteEnable(TRUE);
	// Modulative blending
	RHISetBlendState(TStaticBlendState<BO_Add,BF_DestColor,BF_Zero>::GetRHI());

	FShadowDepthDrawingPolicy::ShadowInfo = this;

	// Draw the receiver's dynamic elements.
	TDynamicPrimitiveDrawer<FShadowDepthDrawingPolicyFactory> Drawer(View, DepthPriorityGroup, FShadowDepthDrawingPolicyFactory::ContextType(this, FALSE), TRUE);

	for (INT PrimitiveIndex = 0; PrimitiveIndex < ReceiverPrimitives.Num(); PrimitiveIndex++)
	{
		const FPrimitiveSceneInfo* ReceiverPrimitiveSceneInfo = ReceiverPrimitives(PrimitiveIndex);

		if (View->PrimitiveVisibilityMap(ReceiverPrimitiveSceneInfo->Id))
		{
			const FPrimitiveViewRelevance& ViewRelevance = View->PrimitiveViewRelevanceMap(ReceiverPrimitiveSceneInfo->Id);

			if (ViewRelevance.bDynamicRelevance)
			{
				Drawer.SetPrimitive(ReceiverPrimitiveSceneInfo);
				ReceiverPrimitiveSceneInfo->Proxy->DrawDynamicElements(&Drawer, View, DepthPriorityGroup);
			}

			if (ViewRelevance.bStaticRelevance)
			{
				for (INT StaticMeshIdx = 0; StaticMeshIdx < ReceiverPrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
				{
					const FStaticMesh& StaticMesh = ReceiverPrimitiveSceneInfo->StaticMeshes(StaticMeshIdx);

					if (View->StaticMeshVisibilityMap(StaticMesh.Id))
					{
						FShadowDepthDrawingPolicyFactory::DrawStaticMesh(
							*View,
							FShadowDepthDrawingPolicyFactory::ContextType(this, FALSE),
							StaticMesh,
							TRUE,
							ReceiverPrimitiveSceneInfo,
							StaticMesh.HitProxyId
							);
					}
				}
			}
		}
	}

	FShadowDepthDrawingPolicy::ShadowInfo = NULL;

	GMobileRenderingForwardShadowProjections = FALSE;

#endif
}

void FProjectedShadowInfo::RenderFrustumWireframe(FPrimitiveDrawInterface* PDI) const
{
	// Find the ID of an arbitrary subject primitive to use to color the shadow frustum.
	INT SubjectPrimitiveId = 0;
	if(SubjectPrimitives.Num())
	{
		SubjectPrimitiveId = SubjectPrimitives(0)->Id;
	}

	const FMatrix InvShadowTransform = (bFullSceneShadow || bPreShadow) ? SubjectAndReceiverMatrix.Inverse() : InvReceiverMatrix;
	// Render the wireframe for the frustum derived from ReceiverMatrix.
	DrawFrustumWireframe(
		PDI,
		InvShadowTransform * FTranslationMatrix(-PreShadowTranslation),
		FColor(FLinearColor::FGetHSV(((SubjectPrimitiveId + LightSceneInfo->Id) * 31) & 255,0,255)),
		SDPG_World
		);
}

FMatrix FProjectedShadowInfo::GetScreenToShadowMatrix(const FSceneView& View, UBOOL bTranslucentPreShadow) const
{
	const FIntPoint ShadowBufferResolution = GetShadowBufferResolution(bTranslucentPreShadow);
	const FLOAT InvBufferResolutionX = 1.0f / (FLOAT)ShadowBufferResolution.X;
	const FLOAT ShadowResolutionFractionX = 0.5f * (FLOAT)ResolutionX * InvBufferResolutionX;
	const FLOAT InvBufferResolutionY = 1.0f / (FLOAT)ShadowBufferResolution.Y;
	const FLOAT ShadowResolutionFractionY = 0.5f * (FLOAT)ResolutionY * InvBufferResolutionY;
	// Calculate the matrix to transform a screenspace position into shadow map space
	// Translucent preshadows start from post projection space since the position was not derived off of the depth buffer like deferred shadows
	FMatrix ScreenToShadow = bTranslucentPreShadow ? 
		FMatrix::Identity : 
		// Z of the position being transformed is actually view space Z, 
		// Transform it into post projection space by applying the projection matrix,
		// Which is the required space before applying View.InvTranslatedViewProjectionMatrix
		FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,View.ProjectionMatrix.M[2][2],1),
			FPlane(0,0,View.ProjectionMatrix.M[3][2],0)
		);

	// OpenGL render-to-textures will be upside down when read.
	FLOAT ShadowScaleX = ShadowResolutionFractionX;
	FLOAT ShadowBiasX  = (X + SHADOW_BORDER + GPixelCenterOffset) * InvBufferResolutionX + ShadowResolutionFractionX;
	FLOAT ShadowScaleY = -ShadowResolutionFractionY;
	FLOAT ShadowBiasY  = (Y + SHADOW_BORDER + GPixelCenterOffset) * InvBufferResolutionY + ShadowResolutionFractionY;
#if WITH_ES2_RHI
	if ( GUsingES2RHI )
	{
		ShadowScaleY = -ShadowScaleY;
	}
#endif

	ScreenToShadow *=
		// Transform the post projection space position into translated world space
		// Translated world space is normal world space translated to the view's origin, 
		// Which prevents floating point imprecision far from the world origin.
		View.InvTranslatedViewProjectionMatrix *
		// Translate to the origin of the shadow's translated world space
		FTranslationMatrix(PreShadowTranslation - View.PreViewTranslation) *
		// Transform into the shadow's post projection space
		// This has to be the same transform used to render the shadow depths
		SubjectAndReceiverMatrix *
		// Scale and translate x and y to be texture coordinates into the ShadowInfo's rectangle in the shadow depth buffer
		// Normalize z by MaxSubjectDepth, as was done when writing shadow depths
		FMatrix(
			FPlane(ShadowScaleX,			0,							0,							0),
			FPlane(0,						ShadowScaleY,				0,							0),
			FPlane(0,						0,							1.0f / MaxSubjectDepth,		0),
			FPlane(ShadowBiasX,				ShadowBiasY,				0,							1)
		);
	return ScreenToShadow;
}

/** Returns the resolution of the shadow buffer used for this shadow, based on the shadow's type. */
FIntPoint FProjectedShadowInfo::GetShadowBufferResolution(UBOOL bTranslucentPreShadow) const
{
	if (bAllocatedInPreshadowCache)
	{
		return GSceneRenderTargets.GetPreshadowCacheTextureResolution();
	}
	else
	{
		if (bTranslucentPreShadow)
		{
			return GSceneRenderTargets.GetTranslucencyShadowDepthTextureResolution();
		}
		else
		{
			return GSceneRenderTargets.GetShadowDepthTextureResolution(IsPrimaryWholeSceneDominantShadow());
		}
	}
}

void FProjectedShadowInfo::SortSubjectMeshElements()
{
	Sort<USE_COMPARE_CONSTREF(FShadowStaticMeshElement,ShadowRendering)>(&SubjectMeshElements(0),SubjectMeshElements.Num());
}

/**
 * Find the view and DPG that this shadow is relevant to
 *
 * @param Views - The views taken from SceneRenderer that are about to be rendered.
 * @param DepthPriorityGroup - The Priority group of the PrimtiveSceneInfo to receive this shadow
 * @param LightSceneInfoID - The ID of the light within a view
 * @param bTranslucentPreShadow - TRUE if this pre shadow is for translucency
 * @return FoundView - NULL if no relevant view is found.  Otherwise points to a view in which this shadow is relevant
 * @return DPGToUseForDepths - The DPG to use for rendering this object to the depth map
 */
void FProjectedShadowInfo::FindViewAndDPGForRenderDepth(const TArray<FViewInfo>& Views, const UINT DepthPriorityGroup, const INT LightSceneInfoId, UBOOL bTranslucentPreShadow, const FViewInfo*& FoundView, ESceneDepthPriorityGroup& DPGToUseForDepths)
{
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo* CheckView = &Views(ViewIndex);
		const FVisibleLightViewInfo& VisibleLightViewInfo = CheckView->VisibleLightInfos(LightSceneInfoId);
		FPrimitiveViewRelevance ViewRel = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ShadowId);
		if (ViewRel.GetDPG(DepthPriorityGroup))
		{
			FoundView = CheckView;
			checkSlow(!bTranslucentPreShadow || ViewRel.bTranslucentRelevance);
			if (bPreShadow && DepthPriorityGroup == SDPG_Foreground)
			{
				DPGToUseForDepths = SDPG_World;
			}
			break;
		}
		// Allow a view with the shadow relevant to the foreground DPG if we are in the WorldDPG and any view has the subject in the foreground DPG
		else if (bForegroundCastingOnWorld && ViewRel.GetDPG(SDPG_Foreground))
		{
			FoundView = CheckView;
			DPGToUseForDepths = SDPG_Foreground;
			break;
		}
	}
}


/*-----------------------------------------------------------------------------
FSceneRenderer
-----------------------------------------------------------------------------*/

/** Returns TRUE if any dominant lights are casting dynamic shadows. */
UBOOL FSceneRenderer::AreDominantShadowsActive(UINT DPGIndex) const
{
	if (GOnePassDominantLight && (ViewFamily.ShowFlags & SHOW_Lighting))
	{
		for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			const FLightSceneInfo* const LightSceneInfo = LightIt->LightSceneInfo;

			if (IsDominantLightType(LightSceneInfo->LightType)
				&& LightSceneInfo->LightShadowMode == LightShadow_Normal)
			{
				const UBOOL bIsLightBlack =
					Square(LightSceneInfoCompact.Color.R) < DELTA &&
					Square(LightSceneInfoCompact.Color.G) < DELTA &&
					Square(LightSceneInfoCompact.Color.B) < DELTA;

				if( bIsLightBlack )
				{
					continue;
				}

				// Check for a need to use the attenuation buffer
				if(ViewFamily.ShouldDrawShadows() && (CheckForProjectedShadows( LightSceneInfo, DPGIndex ) || CheckForLightFunction( LightSceneInfo, DPGIndex )))
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/** Renders whole scene dominant shadow depths. */
void FSceneRenderer::RenderWholeSceneDominantShadowDepth(UINT DPGIndex)
{
	if (bDominantShadowsActive && DPGIndex == SDPG_World)
	{
		SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime, !bIsSceneCapture);
		for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			const FLightSceneInfo* const LightSceneInfo = LightIt->LightSceneInfo;

			if (IsDominantLightType(LightSceneInfo->LightType)
				&& LightSceneInfo->LightShadowMode == LightShadow_Normal)
			{
				const UBOOL bIsLightBlack =
					Square(LightSceneInfoCompact.Color.R) < DELTA &&
					Square(LightSceneInfoCompact.Color.G) < DELTA &&
					Square(LightSceneInfoCompact.Color.B) < DELTA;

				if( bIsLightBlack )
				{
					continue;
				}

				// Check for a need to use the attenuation buffer
				if(ViewFamily.ShouldDrawShadows())
				{
					FProjectedShadowInfo* WholeSceneShadowInfo = NULL;
					FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);
					for(INT ShadowIndex = 0;ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num();ShadowIndex++)
					{
						FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows(ShadowIndex);
						if (ProjectedShadowInfo->IsPrimaryWholeSceneDominantShadow())
						{
							WholeSceneShadowInfo = ProjectedShadowInfo;
							break;
						}
					}

					if (WholeSceneShadowInfo)
					{
						SCOPED_DRAW_EVENT(EventWholeSceneDominantShadowDepth)(DEC_SCENE_ITEMS,TEXT("Whole scene dominant shdw depth"));
						// Set the shadow z surface as the depth buffer
						RHISetRenderTarget(FSurfaceRHIRef(), GSceneRenderTargets.GetShadowDepthZSurface(TRUE));   
						// Disable color writes since we only want z depths
						RHISetColorWriteEnable(FALSE);
						// Manually allocate the shadow since it will not be packed in FSceneRenderer::RenderProjectedShadows
						WholeSceneShadowInfo->X = 0;
						WholeSceneShadowInfo->Y = 0;
						WholeSceneShadowInfo->RenderDepth(this, DPGIndex, FALSE);
						WholeSceneShadowInfo->bAllocated = TRUE;
						// Resolve the shadow depth z surface.
						RHICopyToResolveTarget(GSceneRenderTargets.GetShadowDepthZSurface(TRUE), FALSE, FResolveParams());
						RHISetColorWriteEnable(TRUE);
						break;
					}
				}
			}
		}
	}
}

/** Accumulates normal shadows from the dominant light into the light attenuation buffer. */
void FSceneRenderer::RenderDominantLightShadowsForBasePass(UINT DPGIndex)
{
	if (bDominantShadowsActive)
	{
		SCOPED_DRAW_EVENT(EventDominantShadows)(DEC_SCENE_ITEMS,TEXT("Dominant light shadows"));

		if (DPGIndex == SDPG_World)
		{
			//@todo - can this scene depth resolve be combined with the one done after the base pass?
			GSceneRenderTargets.ResolveSceneDepthTexture();
		}

		// Build a map of light to the closest distance from any view to any shadow of that light
		TMap<const FLightSceneInfo*, FLOAT, SceneRenderingSetAllocator> ClosestDistancesPerLight;
	
		for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views(ViewIndex);
			// Using RGB of the light attenuation buffer for per-object shadows, Alpha is used for whole scene shadows so only support 3 channels.
			// There are 2 light attenuation buffers for dominant light shadows.  If there are more visible dominant shadow casting lights than 2x3 = 6, 
			// The extra ones will share the 3rd channel of the second light attenuation texture which will result in artifacts.
			View.DominantLightChannelAllocator.Reset(3);

			for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
			{
				const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
				const FLightSceneInfo* LightSceneInfo = LightIt->LightSceneInfo;

				if (IsDominantLightType(LightSceneInfo->LightType)
					&& LightSceneInfo->LightShadowMode == LightShadow_Normal)
				{
					const UBOOL bIsLightBlack =
						Square(LightSceneInfoCompact.Color.R) < DELTA &&
						Square(LightSceneInfoCompact.Color.G) < DELTA &&
						Square(LightSceneInfoCompact.Color.B) < DELTA;

					if( bIsLightBlack )
					{
						continue;
					}

					FLOAT ClosestShadowDistanceFromView = FLT_MAX;
					const UBOOL bHasShadows = CheckForProjectedShadows(View, LightSceneInfo, DPGIndex, ClosestShadowDistanceFromView);
					FLOAT ClosestFunctionDistanceFromView = FLT_MAX;
					const UBOOL bHasLightFunction = CheckForLightFunction(View, LightSceneInfo, DPGIndex, ClosestFunctionDistanceFromView);
					if (ViewFamily.ShouldDrawShadows() && (bHasShadows || bHasLightFunction))
					{
						const FLOAT ClosestDistanceFromView = Min(ClosestShadowDistanceFromView, ClosestFunctionDistanceFromView);
						FLOAT* FoundDistance = ClosestDistancesPerLight.Find(LightSceneInfo);
						if (FoundDistance)
						{
							// Track the minimum distance
							*FoundDistance = Min(*FoundDistance, ClosestDistanceFromView);
						}
						else
						{
							// Add a new entry
							FoundDistance = &ClosestDistancesPerLight.Set(LightSceneInfo, ClosestDistanceFromView);
						}
					}
				}
			}
		}
	
		// Go through all the shadow casting dominant lights and assign each light to a light attenuation buffer channel
		// Having the shadow factors in different channels allows us to only let a shadow affect its light and not any other lights
		// Each view gets the same light<->channel allocation, so that we don't have to bind, clear and resolve the light attenuation buffer per view
		// The downside of this is that artifacts from too many shadowing dominant lights may show up worse in splitscreen
		for (TMap<const FLightSceneInfo*, FLOAT, SceneRenderingSetAllocator>::TIterator It(ClosestDistancesPerLight); It; ++It)
		{
			if (!bIsSceneCapture)
			{
				INC_DWORD_STAT(STAT_ShadowCastingDominantLights);
			}
			const FLightSceneInfo* const LightSceneInfo = It.Key();
			const FLOAT ClosestDistanceFromAnyView = It.Value();
			for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				FViewInfo& View = Views(ViewIndex);
				// Allocate each light using the same distances for each view
				View.DominantLightChannelAllocator.AllocateLight(LightSceneInfo->Id, ClosestDistanceFromAnyView, LightSceneInfo->LightType == LightType_DominantDirectional);
			}
		}

		TArray<const FLightSceneInfo*, SceneRenderingAllocator> LightsPerAttenuationTexture[2];
		appMemzero(&LightsPerAttenuationTexture, sizeof(LightsPerAttenuationTexture));

		// Sort the lights by which light attenuation texture they have been allocated to
		for (TMap<const FLightSceneInfo*, FLOAT, SceneRenderingSetAllocator>::TIterator It(ClosestDistancesPerLight); It; ++It)
		{
			const FLightSceneInfo* LightSceneInfo = It.Key();
#if _DEBUG
			for (TMap<const FLightSceneInfo*, FLOAT, SceneRenderingSetAllocator>::TIterator It(ClosestDistancesPerLight); It; ++It)
			{
				const FLightSceneInfo* OtherLightSceneInfo = It.Key();
				if (LightSceneInfo != OtherLightSceneInfo)
				{
					const INT Channel0 = Views(0).DominantLightChannelAllocator.GetLightChannel(LightSceneInfo->Id);
					const INT Texture0 = Views(0).DominantLightChannelAllocator.GetTextureIndex(LightSceneInfo->Id);
					const INT Channel1 = Views(0).DominantLightChannelAllocator.GetLightChannel(OtherLightSceneInfo->Id);
					const INT Texture1 = Views(0).DominantLightChannelAllocator.GetTextureIndex(OtherLightSceneInfo->Id);
					// Verify that no two lights share the same channel and texture unless they are in the final 'overflow' channel
					checkSlow(Channel0 != Channel1 || Texture0 != Texture1 || Texture0 == 1 && Channel0 == 2);
				}
			}
#endif
			// Allocations are the same for every view
			const INT TextureIndex = Views(0).DominantLightChannelAllocator.GetTextureIndex(LightSceneInfo->Id);
			LightsPerAttenuationTexture[TextureIndex].AddItem(LightSceneInfo);
		}

		// DominantShadowDepthZ is sharing the same main memory with LightAttenuation1
		// ShadowDepthZ is sharing the same main memory with LightAttenuation0
		// We have to render shadows to and resolve LightAttenuation1 first,
		// Because rendering shadows will resolve to ShadowDepthZ which would have stomped on LightAttenuation0 if we had rendered LightAttenuation0 first.
		// The dominant directional light also has to be rendered to LightAttenuation1, if LightAttenuation1 is being used,
		// So that DominantShadowDepthZ will no longer be used before LightAttenuation1 gets resolved to.
		for (INT TextureIndex = ARRAY_COUNT(LightsPerAttenuationTexture) - 1; TextureIndex >= 0; TextureIndex--)
		{
			if (LightsPerAttenuationTexture[TextureIndex].Num() > 0)
			{
				// Bind the appropriate light attenuation buffer
				GSceneRenderTargets.BeginRenderingLightAttenuation(TextureIndex == 0);

				// The light attenuation buffer overlaps scene color on xbox 360.
				// In the world DPG we are rendering before the base pass so we don't have to worry about stomping on scene color, and we can clear the whole surface.
				// In the foreground DPG, scene color (and therefore light attenuation) contains the results of the previous DPG's, so we can't clear the whole thing.
				// One approach would be to resolve scene color and restore after accumulating the dominant shadow factors.
				// A cheaper option which is being used is to temporarily overwrite the scene color with the dominant shadow factor, 
				// And then the foreground DPG base pass will overwrite the shadow factors with the foreground DPG scene color.
				// This works because there are usually no overlapping projected shadows in the foreground DPG so we can use an opaque blend mode in FProjectedShadowInfo::RenderProjection.
				if (DPGIndex == SDPG_World)
				{
					// Clear the light attenuation surface to white.
					RHIClear(TRUE, FLinearColor::White, FALSE, 0, FALSE, 0);
				}

				for (INT LightIndex = 0; LightIndex < LightsPerAttenuationTexture[TextureIndex].Num(); LightIndex++)
				{
					const FLightSceneInfo* const LightSceneInfo = LightsPerAttenuationTexture[TextureIndex](LightIndex);
					const FLightSceneInfoCompact& LightSceneInfoCompact = Scene->Lights(LightSceneInfo->Id);

					if (IsDominantLightType(LightSceneInfo->LightType) && LightSceneInfo->LightShadowMode == LightShadow_Normal)
					{
						check(LightSceneInfo->LightType != LightType_DominantDirectional || TextureIndex == 1 || LightsPerAttenuationTexture[1].Num() == 0);
						const UBOOL bIsLightBlack =
							Square(LightSceneInfoCompact.Color.R) < DELTA &&
							Square(LightSceneInfoCompact.Color.G) < DELTA &&
							Square(LightSceneInfoCompact.Color.B) < DELTA;

						if (bIsLightBlack)
						{
							continue;
						}

						// Check for a need to use the attenuation buffer
						UBOOL bDrawShadows = ViewFamily.ShouldDrawShadows();
						
						UBOOL bShadowsRendered = FALSE;
						if (bDrawShadows && CheckForProjectedShadows(LightSceneInfo, DPGIndex))
						{
							// Render non-modulated projected shadows to the attenuation buffer.
							bShadowsRendered = RenderProjectedShadows(LightSceneInfo, DPGIndex, TRUE);
						}

						if (bDrawShadows && CheckForLightFunction(LightSceneInfo, DPGIndex))
						{
							// Render non-modulated projected shadows to the attenuation buffer.
							const UBOOL bStencilDirty = RenderLightFunction(LightSceneInfo, DPGIndex, bShadowsRendered);
							if (bStencilDirty)
							{
								RHIClear(FALSE,FLinearColor::Black,FALSE,0,TRUE,0);
							}
						}
					}
				}
	
				// Resolve the appropriate light attenuation buffer
				GSceneRenderTargets.FinishRenderingLightAttenuation(TextureIndex == 0);

#if !DISABLE_TRANSLUCENCY_DOMINANT_LIGHT_ATTENUATION
				const INT DominantDirectionalLightTexture = Views(0).DominantLightChannelAllocator.GetDominantDirectionalLightTexture();
				if (bHasInheritDominantShadowMaterials 
					&& TextureIndex == DominantDirectionalLightTexture
					&& DPGIndex == SDPG_World)
				{
					SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("CopyToTranslucencyAttenTex"));
					// Resolve the dominant directional light's screenspace shadow factors to the TranslucencyDominantLightAttenuationTexture, so it can be used later on translucency
					// This is a resolve from RGBA8 to L8, which copies just the R channel on Xbox 360
					// FLightChannelAllocator always allocates the dominant directional light to the R channel so we can be consistent about which light's shadows get used.
					RHICopyToResolveTarget(GSceneRenderTargets.GetLightAttenuationSurface(), FALSE, FResolveParams(FResolveRect(-1, -1, -1, -1), CubeFace_PosX, GSceneRenderTargets.GetTranslucencyDominantLightAttenuationTexture()));
					GSceneRenderTargets.bResolvedTranslucencyDominantLightAttenuationTexture = TRUE;
				}
#endif
			}
		}
	}
	GSceneRenderTargets.SetLightAttenuationMode(bDominantShadowsActive);
}

/** Renders shadow depths for lit translucency. */
const FProjectedShadowInfo* FSceneRenderer::RenderTranslucentShadowDepths(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneInfo* PrimitiveSceneInfo, UINT DPGIndex)
{
#if !PS3 && !WIIU
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime, !bIsSceneCapture);
	// Find the translucent preshadow cast by this light onto the translucent primitive
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);
	FProjectedShadowInfo* FoundShadowInfo = NULL;
	for (INT ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ProjectedPreShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ProjectedPreShadows(ShadowIndex);
		if (ProjectedShadowInfo->ParentSceneInfo == PrimitiveSceneInfo)
		{
			//ensure that there is a View that will use this shadow depth
			const FViewInfo* FoundView = NULL;
			ESceneDepthPriorityGroup DPGToUseForDepths = (ESceneDepthPriorityGroup)DPGIndex;
			ProjectedShadowInfo->FindViewAndDPGForRenderDepth(Views, DPGIndex, LightSceneInfo->Id, TRUE, FoundView, DPGToUseForDepths);

			if (FoundView)
			{
				checkSlow(ProjectedShadowInfo->bPreShadow);
				if (ProjectedShadowInfo->bAllocatedInPreshadowCache && !ProjectedShadowInfo->bDepthsCached)
				{
					// If the preshadow has been allocated in the preshadow cache, but its depths were not cached,
					// Remove it from the preshadow cache since we will have to render the depths now.
					// This will be the case if RenderCachedPreshadows did not render the shadow depths, 
					// Which can happen if the shadow has been determined invisible by an occlusion query.
					verify(Scene->PreshadowCacheLayout.RemoveElement(
						ProjectedShadowInfo->X,
						ProjectedShadowInfo->Y,
						ProjectedShadowInfo->ResolutionX + SHADOW_BORDER * 2,
						ProjectedShadowInfo->ResolutionY + SHADOW_BORDER * 2));
					Scene->CachedPreshadows.RemoveItem(ProjectedShadowInfo);
					ProjectedShadowInfo->bAllocatedInPreshadowCache = FALSE;
					ProjectedShadowInfo->bAllocated = FALSE;
				}
				FoundShadowInfo = ProjectedShadowInfo;
				break;
			}
		}
	}

	if (FoundShadowInfo)
	{
		// Render the preshadow depths to the translucency shadow depth buffer if they are not already cached
		if (!FoundShadowInfo->bDepthsCached)
		{
			SCOPED_DRAW_EVENT(EventTranslucencyPreShadowDepth)(DEC_SCENE_ITEMS,TEXT("Translucency PreShadow Depth"));

			if(GSupportsDepthTextures)
			{
				// set the shadow z surface as the depth buffer
				RHISetRenderTarget(FSurfaceRHIRef(), GSceneRenderTargets.GetTranslucencyShadowDepthZSurface());   
				// disable color writes since we only want z depths
				RHISetColorWriteEnable(FALSE);
			}
			else
			{
				// Set the shadow color surface as the render target, and the shadow z surface as the depth buffer
				RHISetRenderTarget(GSceneRenderTargets.GetTranslucencyShadowDepthColorSurface(), GSceneRenderTargets.GetTranslucencyShadowDepthZSurface());
			}

			// Manually allocate the shadow since it will not be packed in FSceneRenderer::RenderProjectedShadows
			FoundShadowInfo->X = 0;
			FoundShadowInfo->Y = 0;
			FoundShadowInfo->RenderDepth(this, DPGIndex, TRUE);
			FoundShadowInfo->bAllocated = TRUE;

			INC_DWORD_STAT(STAT_TranslucentPreShadows);

			FResolveRect ResolveRect;
			ResolveRect.X1 = 0;
			ResolveRect.Y1 = 0;
			ResolveRect.X2 = FoundShadowInfo->X + FoundShadowInfo->ResolutionX + SHADOW_BORDER*2;
			ResolveRect.Y2 = FoundShadowInfo->Y + FoundShadowInfo->ResolutionY + SHADOW_BORDER*2;

			if( GSupportsDepthTextures )
			{
				// Resolve the shadow depth z surface.
				RHICopyToResolveTarget(GSceneRenderTargets.GetTranslucencyShadowDepthZSurface(), FALSE, FResolveParams(ResolveRect));
				// restore color writes
				RHISetColorWriteEnable(TRUE);
			}
			else
			{
				// Resolve the shadow depth color surface.
				RHICopyToResolveTarget(GSceneRenderTargets.GetTranslucencyShadowDepthColorSurface(), FALSE, FResolveParams(ResolveRect));
			}
		}
		
		FoundShadowInfo->bRendered = TRUE;
		return FoundShadowInfo;
	}
#endif
	return NULL;
}

/**
 * Used by RenderLights to figure out if projected shadows need to be rendered to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @return TRUE if anything needs to be rendered
 */
UBOOL FSceneRenderer::CheckForProjectedShadows( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex ) const
{
	// Find the projected shadows cast by this light.
	const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);
	for( INT ShadowIndex=0; ShadowIndex<VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++ )
	{
		const FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows(ShadowIndex);

		// Check that the shadow is visible in at least one view before rendering it.
		UBOOL bShadowIsVisible = FALSE;
		for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightSceneInfo->Id);
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ShadowIndex);
			const UBOOL bForegroundCastingOnWorld = 
				DPGIndex == SDPG_World 
				&& ViewRelevance.GetDPG(SDPG_Foreground) 
				&& GSystemSettings.bEnableForegroundShadowsOnWorld
				&& !ProjectedShadowInfo->bPreShadow
				&& !ProjectedShadowInfo->bFullSceneShadow;

			bShadowIsVisible |= (bForegroundCastingOnWorld || ViewRelevance.GetDPG(DPGIndex)) && VisibleLightViewInfo.ProjectedShadowVisibilityMap(ShadowIndex);
		}

		if(bShadowIsVisible)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/** Returns TRUE if any projected shadows need to be rendered for the given view. */
UBOOL FSceneRenderer::CheckForProjectedShadows( const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, UINT DPGIndex, FLOAT& ClosestDistanceFromViews ) const
{
	ClosestDistanceFromViews = FLT_MAX;
	UBOOL bAnyShadowIsVisible = FALSE;
	// Find the projected shadows cast by this light.
	const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);
	for( INT ShadowIndex=0; ShadowIndex<VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++ )
	{
		const FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows(ShadowIndex);
		if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
		{
			continue;
		}
		const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightSceneInfo->Id);
		const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ShadowIndex);
		const UBOOL bForegroundCastingOnWorld = 
			DPGIndex == SDPG_World 
			&& ViewRelevance.GetDPG(SDPG_Foreground) 
			&& GSystemSettings.bEnableForegroundShadowsOnWorld
			&& !ProjectedShadowInfo->bPreShadow
			&& !ProjectedShadowInfo->bFullSceneShadow;

		if ((bForegroundCastingOnWorld || ViewRelevance.GetDPG(DPGIndex)) && VisibleLightViewInfo.ProjectedShadowVisibilityMap(ShadowIndex))
		{
			bAnyShadowIsVisible = TRUE;
			const FLOAT DistanceToView = Max((ProjectedShadowInfo->ShadowBounds.Center - View.ViewOrigin).Size() - ProjectedShadowInfo->ShadowBounds.W, 0.0f);
			ClosestDistanceFromViews = Min(ClosestDistanceFromViews, DistanceToView);
		}
	}
	return bAnyShadowIsVisible;
}

/** Renders preshadow depths for any preshadows whose depths aren't cached yet, and renders the projections of preshadows with opaque relevance. */
UBOOL FSceneRenderer::RenderCachedPreshadows(
	const FLightSceneInfo* LightSceneInfo, 
	UINT DPGIndex, 
	UBOOL bRenderingBeforeLight)
{
	UBOOL bAttenuationBufferDirty = FALSE;

	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> CachedPreshadows;
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> OpaqueCachedPreshadows;
	UBOOL bHasDepthsToRender = FALSE;

	for (INT ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ProjectedPreShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ProjectedPreShadows(ShadowIndex);

		// Check that the shadow is visible in at least one view before rendering it.
		UBOOL bShadowIsVisible = FALSE;
		UBOOL bOpaqueRelevance = FALSE;
		for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightSceneInfo->Id);
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ProjectedShadowInfo->ShadowId);

			bShadowIsVisible |= (ViewRelevance.GetDPG(DPGIndex) && VisibleLightViewInfo.ProjectedShadowVisibilityMap(ProjectedShadowInfo->ShadowId));
			bOpaqueRelevance |= ViewRelevance.bOpaqueRelevance;
		}

		if (!ProjectedShadowInfo->bPreShadow && DPGIndex == SDPG_Foreground && !GSystemSettings.bEnableForegroundSelfShadowing)
		{
			// Disable foreground self-shadowing based on system settings
			bShadowIsVisible = FALSE;
		}

		if (ProjectedShadowInfo->bPreShadow && DPGIndex == SDPG_World && !LightSceneInfo->bStaticShadowing)
		{
			bShadowIsVisible = FALSE;
		}

		if (ProjectedShadowInfo->bAllocatedInPreshadowCache && bShadowIsVisible)
		{
#if STATS
			if (!bIsSceneCapture && ProjectedShadowInfo->bPreShadow)
			{
				if (ProjectedShadowInfo->bDepthsCached)
				{
					INC_DWORD_STAT(STAT_CachedPreShadows);
				}
				else
				{
					INC_DWORD_STAT(STAT_PreShadows);
				}
			}
#endif

			// Build the list of cached preshadows which need their depths rendered now
			CachedPreshadows.AddItem(ProjectedShadowInfo);
			bHasDepthsToRender |= !ProjectedShadowInfo->bDepthsCached;
			if (bOpaqueRelevance)
			{
				// Build the list of cached preshadows which need their projections rendered, which is the subset of preshadows affecting opaque materials
				OpaqueCachedPreshadows.AddItem(ProjectedShadowInfo);
			}
		}
	}

	if (CachedPreshadows.Num() > 0)
	{
		if (bHasDepthsToRender)
		{
			SCOPED_DRAW_EVENT(EventShadowDepths)(DEC_SCENE_ITEMS,TEXT("Preshadow Cache Depths"));

			GSceneRenderTargets.BeginRenderingPreshadowCacheDepth();			

			// Render depth for each shadow
			for (INT ShadowIndex = 0; ShadowIndex < CachedPreshadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = CachedPreshadows(ShadowIndex);
				// Only render depths for shadows which haven't already cached their depths
				if (!ProjectedShadowInfo->bDepthsCached)
				{
					ProjectedShadowInfo->RenderDepth(this, DPGIndex, FALSE);
					ProjectedShadowInfo->bDepthsCached = TRUE;

					GSceneRenderTargets.ResolvePreshadowCacheDepth(
						FResolveParams(FResolveRect(
							ProjectedShadowInfo->X,
							ProjectedShadowInfo->Y,
							ProjectedShadowInfo->X + ProjectedShadowInfo->ResolutionX + SHADOW_BORDER * 2,
							ProjectedShadowInfo->Y + ProjectedShadowInfo->ResolutionY + SHADOW_BORDER * 2))
						);
				}
			}

			// Restore color writes since they may have been disabled by GSceneRenderTargets.BeginRenderingPreshadowCacheDepth
			RHISetColorWriteEnable(TRUE);
		}

		// Render the shadow projections.
		{
			SCOPED_DRAW_EVENT(EventShadowProj)(DEC_SCENE_ITEMS,TEXT("Cached Preshadow Projections"));
			RenderProjections(LightSceneInfo, OpaqueCachedPreshadows, DPGIndex, bRenderingBeforeLight);

			// Mark the attenuation buffer as dirty.
			bAttenuationBufferDirty = TRUE;
		}
	}

	return bAttenuationBufferDirty;
}

/** Renders one pass point light shadows. */
UBOOL FSceneRenderer::RenderOnePassPointLightShadows(
	const FLightSceneInfo* LightSceneInfo, 
	UINT DPGIndex, 
	UBOOL bRenderingBeforeLight)
{
	UBOOL bDirty = FALSE;
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
	for (INT ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows(ShadowIndex);

		// Check that the shadow is visible in at least one view before rendering it.
		UBOOL bShadowIsVisible = FALSE;
		for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightSceneInfo->Id);
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ShadowIndex);

			bShadowIsVisible |= (ViewRelevance.GetDPG(DPGIndex) 
				&& ViewRelevance.bOpaqueRelevance
				&& VisibleLightViewInfo.ProjectedShadowVisibilityMap(ShadowIndex));
		}

		if (bShadowIsVisible && ShouldRenderOnePassPointLightShadow(ProjectedShadowInfo))
		{
			Shadows.AddItem(ProjectedShadowInfo);

			if (!bIsSceneCapture)
			{
				INC_DWORD_STAT(STAT_WholeSceneShadows);
			}
		}
	}

	for (INT ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* CurrentShadow = Shadows(ShadowIndex);
		{
			GSceneRenderTargets.BeginRenderingCubeShadowDepth(CurrentShadow->ResolutionX);
			CurrentShadow->RenderDepth(this, DPGIndex, FALSE);
			CurrentShadow->bAllocated = TRUE;
			GSceneRenderTargets.FinishRenderingCubeShadowDepth(CurrentShadow->ResolutionX);
		}
		{
			check(LightSceneInfo->LightShadowMode == LightShadow_Normal);
			GSceneRenderTargets.BeginRenderingLightAttenuation();

			for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENT(EventView, Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"), ViewIndex);

				const FViewInfo& View = Views(ViewIndex);

				// Set the device viewport for the view.
				RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
				RHISetViewParameters(View);
				RHISetMobileHeightFogParams(View.HeightFogParams);

				CurrentShadow->RenderOnePassPointLightProjection(ViewIndex, View, DPGIndex);
			}
		}
		bDirty = TRUE;
	}
	return bDirty;
}

/** Renders the projections of the given Shadows to the appropriate color render target. */
void FSceneRenderer::RenderProjections(
	const FLightSceneInfo* LightSceneInfo, 
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& Shadows, 
	UINT DPGIndex, 
	UBOOL bRenderingBeforeLight)
{
#if !FLASH
	// Modulated shadows render to scene color directly
	if (!(LightSceneInfo->bNonModulatedSelfShadowing && bRenderingBeforeLight) && 
		LightSceneInfo->LightShadowMode == LightShadow_Modulate)
	{
		GSceneRenderTargets.BeginRenderingSceneColor(RTUsage_Default);
	}
	else
	{
		// Dominant light allocations are the same for every view
		const INT TextureIndex = Views(0).DominantLightChannelAllocator.GetTextureIndex(LightSceneInfo->Id);
		// Normal shadows render to light attenuation
		GSceneRenderTargets.BeginRenderingLightAttenuation(TextureIndex == 0 || TextureIndex == INDEX_NONE);
	}
#endif

	for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENT(EventView, Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"), ViewIndex);

		const FViewInfo& View = Views(ViewIndex);

		// Set the device viewport for the view.
		RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
		RHISetViewParameters(View);
		RHISetMobileHeightFogParams(View.HeightFogParams);

		if (!bIsSceneCapture)
		{
			// Set the light's scissor rectangle.
			LightSceneInfo->SetScissorRect(&View);
		}

		// Project the shadow depth buffers onto the scene.
		for (INT ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows(ShadowIndex);
			if (ProjectedShadowInfo->bAllocated
				// Only project view dependent shadows on to the view for which it was created.
				&& ((ProjectedShadowInfo->DependentView == NULL)
					|| (ProjectedShadowInfo->DependentView == &View)
#if WITH_REALD
					|| RealD::IsStereoEnabled()
#endif
					)
				)
			{
				// Only project the shadow if it's large enough in this particular view (split screen, etc... may have shadows that are large in one view but irrelevantly small in others)
				INT AlphasIndex = ViewIndex;
#if WITH_REALD
				if (RealD::IsStereoEnabled())
				{
					AlphasIndex = 0;
				}
#endif
				if (ProjectedShadowInfo->FadeAlphas(AlphasIndex) > 1.0f / 256.0f)
				{
					ProjectedShadowInfo->RenderProjection(ViewIndex, &View, DPGIndex, bRenderingBeforeLight);
				}
			}
		}

		// Reset the scissor rectangle.
		RHISetScissorRect(FALSE, 0, 0, 0, 0);
	}

	// Restore color write mask state
	RHISetColorWriteMask(CW_RGBA);
}

#if WITH_MOBILE_RHI
/** Renders the projections of the given Shadows to the appropriate color render target. */
void FSceneRenderer::RenderMobileProjections( UINT DPGIndex )
{
	for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENT(EventView, Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"), ViewIndex);

		const FViewInfo& View = Views(ViewIndex);

		// Set the device viewport for the view.
		RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
		RHISetViewParameters(View);
		RHISetMobileHeightFogParams(View.HeightFogParams);

		// Project the shadow depth buffers onto the scene.
		for (INT ShadowIndex = 0; ShadowIndex < MobileProjectedShadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = MobileProjectedShadows(ShadowIndex);
			if (ProjectedShadowInfo->bAllocated)
			{
				// Only project the shadow if it's large enough in this particular view (split screen, etc... may have shadows that are large in one view but irrelevantly small in others)
				if (ProjectedShadowInfo->FadeAlphas(ViewIndex) > 1.0f / 256.0f)
				{
					ProjectedShadowInfo->RenderProjection(ViewIndex, &View, DPGIndex, FALSE);
				}
			}
		}
	}
}
#endif

// FSceneRenderer::RenderProjectedShadows relies on the primary whole scene dominant shadow being first in the sorted array of FProjectedShadowInfo's
IMPLEMENT_COMPARE_POINTER(FProjectedShadowInfo,ShadowRendering,{ return B->IsPrimaryWholeSceneDominantShadow() ? 1 : (B->ResolutionX*B->ResolutionY - A->ResolutionX*A->ResolutionY); });

/**
 * Used by RenderLights to render shadows to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @return TRUE if anything got rendered
 */
UBOOL FSceneRenderer::RenderProjectedShadows( const FLightSceneInfo* LightSceneInfo, UINT DPGIndex, UBOOL bRenderingBeforeLight )
{
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime, !bIsSceneCapture);
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderNormalShadowsTime, !bIsSceneCapture && bRenderingBeforeLight);

	UBOOL bAttenuationBufferDirty = FALSE;

	// Find the projected shadows cast by this light.
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
	for (INT ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows(ShadowIndex);

		// Check that the shadow is visible in at least one view before rendering it.
		UBOOL bShadowIsVisible = FALSE;
		UBOOL bForegroundCastingOnWorld = FALSE;
		for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightSceneInfo->Id);
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ShadowIndex);

			// Mark shadows whose subjects are in the foreground DPG but are casting on the world DPG.
			bForegroundCastingOnWorld |= 
				DPGIndex == SDPG_World 
				&& ViewRelevance.GetDPG(SDPG_Foreground) 
				&& GSystemSettings.bEnableForegroundShadowsOnWorld
				&& !ProjectedShadowInfo->bPreShadow
				&& !ProjectedShadowInfo->bFullSceneShadow;

			bShadowIsVisible |= (ViewRelevance.GetDPG(DPGIndex) 
				&& ViewRelevance.bOpaqueRelevance
				&& VisibleLightViewInfo.ProjectedShadowVisibilityMap(ShadowIndex));
		}

		if (!ProjectedShadowInfo->bPreShadow && DPGIndex == SDPG_Foreground && !GSystemSettings.bEnableForegroundSelfShadowing)
		{
			// Disable foreground self-shadowing based on system settings
			bShadowIsVisible = FALSE;
		}

		if (ProjectedShadowInfo->bPreShadow && DPGIndex == SDPG_World && !LightSceneInfo->bStaticShadowing)
		{
			bShadowIsVisible = FALSE;
		}

		// Don't render the modulated shadow for a light using bNonModulatedSelfShadowing on a primitive that only self shadows
		if (ProjectedShadowInfo->bSelfShadowOnly && LightSceneInfo->bNonModulatedSelfShadowing && !bRenderingBeforeLight)
		{
			bShadowIsVisible = FALSE;
		}

		// Skip shadows which will be handled in RenderOnePassPointLightShadows
		if (ShouldRenderOnePassPointLightShadow(ProjectedShadowInfo))
		{
			bShadowIsVisible = FALSE;
		}

		if ((bShadowIsVisible || bForegroundCastingOnWorld) 
			&& (!ProjectedShadowInfo->bPreShadow || ProjectedShadowInfo->HasSubjectPrims())
			&& !ProjectedShadowInfo->bAllocatedInPreshadowCache)
		{
			// Add the shadow to the list of visible shadows cast by this light.
#if STATS
			if (!bIsSceneCapture)
			{
				if (ProjectedShadowInfo->bFullSceneShadow)
				{
					INC_DWORD_STAT(STAT_WholeSceneShadows);
				}
				else if (ProjectedShadowInfo->bPreShadow)
				{
					INC_DWORD_STAT(STAT_PreShadows);
				}
				else
				{
					INC_DWORD_STAT(STAT_PerObjectShadows);
				}
			}
#endif
			ProjectedShadowInfo->bForegroundCastingOnWorld = bForegroundCastingOnWorld;
			Shadows.AddItem(ProjectedShadowInfo);
		}
	}

	// Sort the projected shadows by resolution.
	Sort<USE_COMPARE_POINTER(FProjectedShadowInfo,ShadowRendering)>(&Shadows(0), Shadows.Num());

	for (INT ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = Shadows(ShadowIndex);
		ProjectedShadowInfo->bRendered = FALSE;
	}

	// Add a draw event with the light name if we are rendering modulated shadows, so that we can track back the shadows to which light caused them
	SCOPED_CONDITIONAL_DRAW_EVENT(EventModShadowLight,!bRenderingBeforeLight && Shadows.Num() > 0)(DEC_SCENE_ITEMS,*LightSceneInfo->GetLightName().ToString());

	INT PassNumber			= 0;
	INT NumShadowsRendered	= 0;
	while (NumShadowsRendered < Shadows.Num())
	{
		const UBOOL bRenderingWholeSceneDominantShadow = PassNumber == 0 && Shadows(0)->IsPrimaryWholeSceneDominantShadow();
		if (bRenderingWholeSceneDominantShadow)
		{
			Shadows(0)->bAllocated = TRUE;
			Shadows(0)->X = 0;
			Shadows(0)->Y = 0;
		}
		else
		{
			INT NumAllocatedShadows = 0;

			// Allocate shadow texture space to the shadows.
			const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
			FTextureLayout ShadowLayout(1, 1, ShadowBufferResolution.X, ShadowBufferResolution.Y);
			for (INT ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows(ShadowIndex);
				if (!ProjectedShadowInfo->bRendered)
				{
					// This function assumes the whole scene dominant shadow (if any) is the largest shadow and is not packed
					check(!ProjectedShadowInfo->IsPrimaryWholeSceneDominantShadow());
					if (ShadowLayout.AddElement(
							ProjectedShadowInfo->X,
							ProjectedShadowInfo->Y,
							ProjectedShadowInfo->ResolutionX + SHADOW_BORDER * 2,
							ProjectedShadowInfo->ResolutionY + SHADOW_BORDER * 2)
						)
					{
						ProjectedShadowInfo->bAllocated = TRUE;
						NumAllocatedShadows++;
					}
				}
			}

			// Abort if we encounter a shadow that doesn't fit in the render target.
			if (!NumAllocatedShadows)
			{
				break;
			}
		}

		// Skip rendering shadow depths for the whole scene dominant shadow if GOnePassDominantLight is enabled, because it was rendered earlier
		UBOOL bRenderDepthPass = (PassNumber > 0 || !(GOnePassDominantLight && Shadows(0)->IsPrimaryWholeSceneDominantShadow()));

		if (bRenderDepthPass)
		{
			// Render the shadow depths.
			SCOPED_DRAW_EVENT(EventShadowDepths)(DEC_SCENE_ITEMS,TEXT("Shadow Depths"));

			GSceneRenderTargets.BeginRenderingShadowDepth(bRenderingWholeSceneDominantShadow);			

			// keep track of the max RECT needed for resolving the depth surface	
			FResolveRect ResolveRect;
			ResolveRect.X1 = 0;
			ResolveRect.Y1 = 0;
			UBOOL bResolveRectInit = FALSE;
			// render depth for each shadow
			for (INT ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows(ShadowIndex);
				if (ProjectedShadowInfo->bAllocated)
				{
					ProjectedShadowInfo->RenderDepth(this, DPGIndex, FALSE);

					// init values
					if (!bResolveRectInit)
					{
						ResolveRect.X2 = ProjectedShadowInfo->X + ProjectedShadowInfo->ResolutionX + SHADOW_BORDER*2;
						ResolveRect.Y2 = ProjectedShadowInfo->Y + ProjectedShadowInfo->ResolutionY + SHADOW_BORDER*2;
						bResolveRectInit = TRUE;
					}
					// keep track of max extents
					else 
					{
						ResolveRect.X2 = Max<UINT>(ProjectedShadowInfo->X + ProjectedShadowInfo->ResolutionX + SHADOW_BORDER*2,ResolveRect.X2);
						ResolveRect.Y2 = Max<UINT>(ProjectedShadowInfo->Y + ProjectedShadowInfo->ResolutionY + SHADOW_BORDER*2,ResolveRect.Y2);
					}
				}
			}

			// only resolve the portion of the shadow buffer that we rendered to
			GSceneRenderTargets.FinishRenderingShadowDepth(bRenderingWholeSceneDominantShadow, ResolveRect);
		}

		// Render the shadow projections.
		{
			SCOPED_DRAW_EVENT(EventShadowProj)(DEC_SCENE_ITEMS,TEXT("Shadow Projection"));
			RenderProjections(LightSceneInfo, Shadows, DPGIndex, bRenderingBeforeLight);

			// Mark and count the rendered shadows.
			for (INT ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows(ShadowIndex);
				if (ProjectedShadowInfo->bAllocated)
				{
					ProjectedShadowInfo->bAllocated = FALSE;
					ProjectedShadowInfo->bRendered = TRUE;
					NumShadowsRendered++;
				}
			}

			// Mark the attenuation buffer as dirty.
			bAttenuationBufferDirty = TRUE;
		}

		// Increment pass number used by stats.
		PassNumber++;
	}

	bAttenuationBufferDirty |= RenderCachedPreshadows(LightSceneInfo, DPGIndex, bRenderingBeforeLight);
	if (GRHIShaderPlatform == SP_PCD3D_SM5)
	{
		bAttenuationBufferDirty |= RenderOnePassPointLightShadows(LightSceneInfo, DPGIndex, bRenderingBeforeLight);
	}

	return bAttenuationBufferDirty;
}

#if WITH_MOBILE_RHI
/**
 * Three-pass projected shadow rendering for mobile
 * 1. Gather the shadows to render
 * 2. Render the shadows into appropriate shadow buffers
 * 3. Apply the rendered shadows to the scene
 *
 * @param LightSceneInfo Represents the current light
 * @return TRUE if anything got rendered
 */
UBOOL FSceneRenderer::GatherMobileProjectedShadows( UINT DPGIndex, const FLightSceneInfo* LightSceneInfo )
{
	const UBOOL bRenderingBeforeLight = FALSE;

	// Find the projected shadows cast by this light.
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightSceneInfo->Id);
	for (INT ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows(ShadowIndex);

		// Check that the shadow is visible in at least one view before rendering it.
		UBOOL bShadowIsVisible = FALSE;
		UBOOL bForegroundCastingOnWorld = FALSE;
		for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views(ViewIndex);
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos(LightSceneInfo->Id);
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap(ShadowIndex);

			// Mark shadows whose subjects are in the foreground DPG but are casting on the world DPG.
			bForegroundCastingOnWorld |= 
				DPGIndex == SDPG_World 
				&& ViewRelevance.GetDPG(SDPG_Foreground) 
				&& GSystemSettings.bEnableForegroundShadowsOnWorld
				&& !ProjectedShadowInfo->bPreShadow
				&& !ProjectedShadowInfo->bFullSceneShadow;

			bShadowIsVisible |= (ViewRelevance.GetDPG(DPGIndex) 
				&& ViewRelevance.bOpaqueRelevance
				&& VisibleLightViewInfo.ProjectedShadowVisibilityMap(ShadowIndex));
		}

		if (!ProjectedShadowInfo->bPreShadow && DPGIndex == SDPG_Foreground && !GSystemSettings.bEnableForegroundSelfShadowing)
		{
			// Disable foreground self-shadowing based on system settings
			bShadowIsVisible = FALSE;
		}

		if (ProjectedShadowInfo->bPreShadow && DPGIndex == SDPG_World && !LightSceneInfo->bStaticShadowing)
		{
			bShadowIsVisible = FALSE;
		}

		// Don't render the modulated shadow for a light using bNonModulatedSelfShadowing on a primitive that only self shadows
		if (ProjectedShadowInfo->bSelfShadowOnly && LightSceneInfo->bNonModulatedSelfShadowing && !bRenderingBeforeLight)
		{
			bShadowIsVisible = FALSE;
		}

		// Skip shadows which will be handled in RenderOnePassPointLightShadows
		if (ShouldRenderOnePassPointLightShadow(ProjectedShadowInfo))
		{
			bShadowIsVisible = FALSE;
		}

		if ((bShadowIsVisible || bForegroundCastingOnWorld) 
			&& (!ProjectedShadowInfo->bPreShadow || ProjectedShadowInfo->HasSubjectPrims())
			&& !ProjectedShadowInfo->bAllocatedInPreshadowCache)
		{
			// Add the shadow to the list of visible shadows cast by this light.
#if STATS
			if (!bIsSceneCapture)
			{
				if (ProjectedShadowInfo->bFullSceneShadow)
				{
					INC_DWORD_STAT(STAT_WholeSceneShadows);
				}
				else if (ProjectedShadowInfo->bPreShadow)
				{
					INC_DWORD_STAT(STAT_PreShadows);
				}
				else
				{
					INC_DWORD_STAT(STAT_PerObjectShadows);
				}
			}
#endif
			ProjectedShadowInfo->bForegroundCastingOnWorld = bForegroundCastingOnWorld;
			MobileProjectedShadows.AddItem(ProjectedShadowInfo);
		}
	}
	return FALSE;
}

UBOOL FSceneRenderer::RenderMobileProjectedShadows( UINT DPGIndex )
{
	const UBOOL bRenderingBeforeLight = FALSE;
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime, !bIsSceneCapture);
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_RenderNormalShadowsTime, !bIsSceneCapture && bRenderingBeforeLight);

	checkf(GUsingMobileRHI, TEXT(""));

	if( GSystemSettings.bMobileModShadows )
	{
		// Sort the projected shadows by resolution.
		Sort<USE_COMPARE_POINTER(FProjectedShadowInfo,ShadowRendering)>(&MobileProjectedShadows(0), MobileProjectedShadows.Num());

		for (INT ShadowIndex = 0; ShadowIndex < MobileProjectedShadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = MobileProjectedShadows(ShadowIndex);
			ProjectedShadowInfo->bRendered = FALSE;
		}

		// Add a draw event with the light name if we are rendering modulated shadows, so that we can track back the shadows to which light caused them
		//SCOPED_CONDITIONAL_DRAW_EVENT(EventModShadowLight,!bRenderingBeforeLight && MobileProjectedShadows.Num() > 0)(DEC_SCENE_ITEMS,*LightSceneInfo->GetLightName().ToString());

		INT PassNumber			= 0;
		INT NumShadowsRendered	= 0;
		if( MobileProjectedShadows.Num() )
		{
			const UBOOL bRenderingWholeSceneDominantShadow = PassNumber == 0 && MobileProjectedShadows(0)->IsPrimaryWholeSceneDominantShadow();
			if (bRenderingWholeSceneDominantShadow)
			{
				MobileProjectedShadows(0)->bAllocated = TRUE;
				MobileProjectedShadows(0)->X = 0;
				MobileProjectedShadows(0)->Y = 0;
			}
			else
			{
				INT NumAllocatedShadows = 0;

				// Allocate shadow texture space to the shadows.
				const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
				FTextureLayout ShadowLayout(1, 1, ShadowBufferResolution.X, ShadowBufferResolution.Y);
				for (INT ShadowIndex = 0; ShadowIndex < MobileProjectedShadows.Num(); ShadowIndex++)
				{
					FProjectedShadowInfo* ProjectedShadowInfo = MobileProjectedShadows(ShadowIndex);
					if (!ProjectedShadowInfo->bRendered)
					{
						// This function assumes the whole scene dominant shadow (if any) is the largest shadow and is not packed
						check(!ProjectedShadowInfo->IsPrimaryWholeSceneDominantShadow());
						if (ShadowLayout.AddElement(
							ProjectedShadowInfo->X,
							ProjectedShadowInfo->Y,
							ProjectedShadowInfo->ResolutionX + SHADOW_BORDER * 2,
							ProjectedShadowInfo->ResolutionY + SHADOW_BORDER * 2)
							)
						{
							ProjectedShadowInfo->bAllocated = TRUE;
							NumAllocatedShadows++;
						}
					}
				}

// 				// Abort if we encounter a shadow that doesn't fit in the render target.
// 				if (!NumAllocatedShadows)
// 				{
// 					break;
// 				}
			}

			// Skip rendering shadow depths for the whole scene dominant shadow if GOnePassDominantLight is enabled, because it was rendered earlier
			UBOOL bRenderDepthPass = (PassNumber > 0 || !(GOnePassDominantLight && MobileProjectedShadows(0)->IsPrimaryWholeSceneDominantShadow()));

			if (!GSupportsDepthTextures)
			{
				bRenderDepthPass = FALSE;
			}

			if (bRenderDepthPass)
			{
				// Render the shadow depths.
				SCOPED_DRAW_EVENT(EventShadowDepths)(DEC_SCENE_ITEMS,TEXT("Shadow Depths"));

				// render depth for each shadow
				for (INT ShadowIndex = 0; ShadowIndex < MobileProjectedShadows.Num(); ShadowIndex++)
				{
					FProjectedShadowInfo* ProjectedShadowInfo = MobileProjectedShadows(ShadowIndex);
					if (ProjectedShadowInfo->bAllocated)
					{
						ProjectedShadowInfo->RenderDepth(this, DPGIndex, FALSE);
					}
				}
			}

			// Increment pass number used by stats.
			PassNumber++;
		}
	}
	else
	{
		if (DPGIndex == SDPG_World)
		{
			// Depth test with the rest of the world
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());

			for (INT ShadowIndex = 0; ShadowIndex < MobileProjectedShadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* Shadow = MobileProjectedShadows(ShadowIndex);
				Shadow->RenderPlanarProjection(this, DPGIndex);
			}
		}
	}

	return FALSE;
}

/** Forces the ES2 implementation of RHISetRenderTarget() to switch to the new FBO, causing a resolve. */
UBOOL GMobileForceSetRenderTarget = FALSE;

UBOOL FSceneRenderer::ApplyMobileProjectedShadows( UINT DPGIndex )
{
	SCOPED_DRAW_EVENT(EventShadowProj)(DEC_SCENE_ITEMS,TEXT("Shadow Projection"));
	
	if(GSystemSettings.bMobileSceneDepthResolveForShadows && TEST_PROFILEEXSTATE(0x10, ViewFamily.CurrentRealTime))
	{
		// We need to resolve out the depth buffer before we can use it in a texture.
		// Do this by using color only for a single draw call, which will flush the
		// depth surface data out to the backing texture.
		GMobileForceSetRenderTarget = TRUE;
		RHISetRenderTarget( GSceneRenderTargets.GetSceneColorSurface(), NULL );
		GMobileForceSetRenderTarget = FALSE;
		{
			static FGlobalBoundShaderState BoundShaderState;
			TShaderMapRef<FShadowProjectionVertexShader> VertexShader(GetGlobalShaderMap());
			SetGlobalBoundShaderState(
				BoundShaderState, GShadowFrustumVertexDeclaration.VertexDeclarationRHI, *VertexShader,
				NULL, sizeof(FVector), 0, EGST_PositionOnly
				);
			DrawDenormalizedQuad( 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 );
		}
		RHISetRenderTarget( GSceneRenderTargets.GetSceneColorSurface(), GSceneRenderTargets.GetSceneDepthSurface() );
	}

	// Render the prepared shadows
	RenderMobileProjections( DPGIndex );

	// Mark and count the rendered shadows.
	for (INT ShadowIndex = 0; ShadowIndex < MobileProjectedShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = MobileProjectedShadows(ShadowIndex);
		if (ProjectedShadowInfo->bAllocated)
		{
			ProjectedShadowInfo->bAllocated = FALSE;
			ProjectedShadowInfo->bRendered = TRUE;
		}
	}
	return TRUE;
}
#endif
