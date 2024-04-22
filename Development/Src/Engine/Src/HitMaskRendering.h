/*=============================================================================
HitMaskRendering.h: Hit Mask Rendering drawing/shader related code
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __HitMaskRendering_h__
#define __HitMaskRendering_h__

/** Hit information struct for rendering data **/
struct HitInfoStruct
{
	/** Hit Location - world location **/
	FVector HitLocation;
	/** Hit Start Location - world location **/
	FVector	HitStartLocation;
	/** Radius of the mask **/
	FLOAT	HitRadius;
	/** Hit Test Cull distance - outside of distance is ignored **/
	FLOAT	HitCullDistance;
	/** Check facing or not **/
	UBOOL	OnlyWhenFacing;
	/** Current Hit Mask Texture - for Accumulation**/
	FTextureRenderTarget2DResource* HitMaskTexture;

	/** Constructor **/
	HitInfoStruct(	
		const FVector& InHitLocation, 
		const FVector & InHitStartLocation, 
		const FLOAT InHitRadius, 
		const FLOAT &InHitCullDistance, 
		const UBOOL &InOnlyWhenFacing,
		FTextureRenderTarget2DResource* InHitMaskTexture)
		: HitLocation(InHitLocation), 
		HitStartLocation(InHitStartLocation),
		HitRadius(InHitRadius), 
		HitCullDistance(InHitCullDistance), 
		OnlyWhenFacing(InOnlyWhenFacing),
		HitMaskTexture(InHitMaskTexture)
		{};
};

/**
 * FHitMaskDrawingPolicy - uses HitMaskVertexShader/HitMaskPixelShader
 * Hit Mask Drawing Policy
 **/
class FHitMaskDrawingPolicy : public FMeshDrawingPolicy
{
public:

	/** Data Structure for rendering**/
	typedef HitInfoStruct ElementDataType;

	/*FHitMaskDrawingPolicy  Constructor */
	FHitMaskDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource
		);

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const FHitMaskDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) 
			&& VertexShader == Other.VertexShader
			&& PixelShader == Other.PixelShader;
	}

	// FMeshDrawingPolicy interface.
	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState, const ElementDataType& ElementData) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	// FMeshDrawingPolicy interface.
	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

	friend INT Compare(const FHitMaskDrawingPolicy& A,const FHitMaskDrawingPolicy& B);

private:
	// Vertex/Pixel Shader of FHitMaskDrawingPolicy
	// HitMaskVertexShader/HitMaskPixelShader.usf
	class FHitMaskVertexShader * VertexShader;
	class FHitMaskPixelShader * PixelShader;
};

/**
* Factory for FHitMaskDrawingPolicy
* This factory is used when rendered by SceneCapture2DHitMaskComponent
*/
class FHitMaskDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = FALSE };
	typedef HitInfoStruct ContextType;

	// Draw Dynamic Mesh
	static UBOOL DrawDynamicMesh(
		const FSceneView& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy);
};


#endif	// __HitMaskRendering_h__
