/*=============================================================================
	TextureDensityRendering.h: Definitions for rendering texture density.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * 
 */
class FTextureDensityDrawingPolicy : public FMeshDrawingPolicy
{
public:
	FTextureDensityDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterialRenderProxy* InOriginalRenderProxy
		);

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const FTextureDensityDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) &&
#if WITH_D3D11_TESSELLATION
			HullShader == Other.HullShader &&
			DomainShader == Other.DomainShader &&
#endif
			VertexShader == Other.VertexShader &&
			PixelShader == Other.PixelShader;
	}
	void DrawShared( const FSceneView* View, FBoundShaderStateRHIRef ShaderState ) const;
	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	friend INT Compare(const FTextureDensityDrawingPolicy& A,const FTextureDensityDrawingPolicy& B);

private:
	class FTextureDensityVertexShader*	VertexShader;
	class FTextureDensityPixelShader*	PixelShader;

#if WITH_D3D11_TESSELLATION
	class FTextureDensityHullShader* HullShader;
	class FTextureDensityDomainShader* DomainShader;
#endif

	const FMaterialRenderProxy*			OriginalRenderProxy;
};

/**
 * A drawing policy factory for rendering texture density.
 */
class FTextureDensityDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = FALSE };
	struct ContextType {};

	static UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);
	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
	{
		return FALSE;
	}
};
