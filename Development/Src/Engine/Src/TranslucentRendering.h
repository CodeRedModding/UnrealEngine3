/*=============================================================================
	TranslucentRendering.h: Translucent rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
* Vertex shader used for combining LDR translucency with scene color when floating point blending isn't supported
*/
class FLDRExtractVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLDRExtractVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		//this is used for shader complexity, so needs to compile for all platforms
		return TRUE;
	}

	/** Default constructor. */
	FLDRExtractVertexShader() {}

public:

	/** Initialization constructor. */
	FLDRExtractVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
	}
};

/**
* Translucent draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
class FTranslucencyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = TRUE };
	struct ContextType 
	{
		ContextType(
			UBOOL& bInRenderingToLowResTranslucencyBuffer, 
			UBOOL& bInAllowDownsampling, 
			UBOOL& bInRenderingToDoFBlurBuffer, 
			const FProjectedShadowInfo*& InTranslucentPreShadowInfo)
		:	bRenderingToLowResTranslucencyBuffer(bInRenderingToLowResTranslucencyBuffer)
		,	bAllowDownsampling(bInAllowDownsampling)
		,	bRenderingToDoFBlurBuffer(bInRenderingToDoFBlurBuffer)
		,	TranslucentPreShadowInfo(InTranslucentPreShadowInfo)
		{}

		// Tracks the state of whether translucency is currently being rendered to the downsampled buffer
		UBOOL& bRenderingToLowResTranslucencyBuffer;
		// Whether downsampling can be used
		UBOOL& bAllowDownsampling;
		// Tracks the state of whether translucency DoF blur is currently being rendered to the DoF blur buffer
		UBOOL& bRenderingToDoFBlurBuffer;
		// The projected preshadow that translucency should receive, if any
		const FProjectedShadowInfo*& TranslucentPreShadowInfo;
	};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawStaticMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
	{
		return !IsTranslucentBlendMode(MaterialRenderProxy->GetMaterial()->GetBlendMode());
	}
};

/**
* Translucent depth-only draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
template<UBOOL bIsPostRenderDepthPass>
class TDynamicLitTranslucencyDepthDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = TRUE };
	struct ContextType {};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawStaticMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
	{
		UBOOL bRender = bIsPostRenderDepthPass ? MaterialRenderProxy->GetMaterial()->RenderLitTranslucencyDepthPostpass() : MaterialRenderProxy->GetMaterial()->RenderLitTranslucencyPrepass();

		return !bRender;
	}
};

