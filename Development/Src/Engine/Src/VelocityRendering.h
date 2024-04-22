/*=============================================================================
	VelocityRendering.h: Velocity rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Maximum offset in screen pixels for motion based velocity vector from opaque prims
#define MAX_PIXELVELOCITY				(16.0f/1280.0f)
// Maximum offset in screen pixels for raw non-motion based velocities of translucent prims
#define MAX_TRANSLUCENT_PIXELVELOCITY	(64.0f/1280.0f)

/**
 * Outputs a 2d velocity vector.
 */
class FVelocityDrawingPolicy : public FMeshDrawingPolicy
{
public:
	FVelocityDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource
		);

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const FVelocityDrawingPolicy& Other) const
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
		const FViewInfo& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	friend INT Compare(const FVelocityDrawingPolicy& A,const FVelocityDrawingPolicy& B);

	UBOOL SupportsVelocity( ) const;

	/** Determines whether this primitive has motionblur velocity to render */
	static UBOOL HasVelocity(const FViewInfo& View, const FPrimitiveSceneInfo* PrimitiveSceneInfo);

private:
	class FVelocityVertexShader* VertexShader;
	class FVelocityPixelShader* PixelShader;

#if WITH_D3D11_TESSELLATION
	class FVelocityHullShader* HullShader;
	class FVelocityDomainShader* DomainShader;
#endif
};

/**
 * A drawing policy factory for rendering motion velocity.
 */
class FVelocityDrawingPolicyFactory : public FDepthDrawingPolicyFactory
{
public:
	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,ContextType = ContextType(DDM_AllOccluders));
	static UBOOL DrawDynamicMesh(	
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);
};
