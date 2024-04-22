/*=============================================================================
	DepthRendering.h: Depth rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

template<UBOOL>
class TDepthOnlyVertexShader;

template<UBOOL>
class TDepthOnlyPixelShader;

/**
 * Used to write out depth for opaque and masked materials during the depth-only pass.
 */
class FDepthDrawingPolicy : public FMeshDrawingPolicy
{
public:

	FDepthDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		UBOOL bForceUsePixelShader=FALSE,
		UBOOL bForceDefaultPixelShader=FALSE,
		UBOOL bOutputDepthToSceneColorAlpha=FALSE
		);

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const FDepthDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) 
			&& bNeedsPixelShader == Other.bNeedsPixelShader
			&& VertexShader == Other.VertexShader
			&& PixelShader == Other.PixelShader;
	}

	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

	friend INT Compare(const FDepthDrawingPolicy& A,const FDepthDrawingPolicy& B);

private:
	UBOOL bNeedsPixelShader;
#if WITH_D3D11_TESSELLATION
	class FDepthOnlyHullShader *HullShader;
	class FDepthOnlyDomainShader *DomainShader;
#endif

	TDepthOnlyVertexShader<FALSE> * VertexShader;
	TDepthOnlyPixelShader<FALSE> * PixelShader;
};

/**
 * Writes out depth for opaque materials on meshes which support a position-only vertex buffer.
 * Using the position-only vertex buffer saves vertex fetch bandwidth during the z prepass.
 */
class FPositionOnlyDepthDrawingPolicy : public FMeshDrawingPolicy
{
public:

	FPositionOnlyDepthDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource
		);

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const FPositionOnlyDepthDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) && VertexShader == Other.VertexShader;
	}

	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

	friend INT Compare(const FPositionOnlyDepthDrawingPolicy& A,const FPositionOnlyDepthDrawingPolicy& B);

private:
	TDepthOnlyVertexShader<TRUE> * VertexShader;
};

enum EDepthDrawingMode
{
	DDM_NonMaskedOnly,
	DDM_AllOccluders,
	DDM_SoftMaskedOnly
};

/**
 * A drawing policy factory for the depth drawing policy.
 */
class FDepthDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = FALSE };
	struct ContextType
	{
		EDepthDrawingMode DepthDrawingMode;
		UBOOL bOutputDepthToSceneAlpha;

		ContextType(EDepthDrawingMode InDepthDrawingMode, UBOOL bInOutputDepthToSceneAlpha = FALSE) :
			DepthDrawingMode(InDepthDrawingMode),
			bOutputDepthToSceneAlpha(bInOutputDepthToSceneAlpha)
		{}
	};

	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh);
	static UBOOL DrawDynamicMesh(
		const FSceneView& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	static UBOOL DrawStaticMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy);
};
