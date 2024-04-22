/*=============================================================================
	DynamicPrimitiveDrawing.h: Dynamic primitive drawing definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DYNAMICPRIMITIVEDRAWING_H__
#define __DYNAMICPRIMITIVEDRAWING_H__

/**
 * An implementation of the dynamic primitive definition interface to draw the elements passed to it on a given RHI command interface.
 */
template<typename DrawingPolicyFactoryType>
class TDynamicPrimitiveDrawer : public FPrimitiveDrawInterface
{
public:

	/**
	* Init constructor
	*
	* @param InView - view region being rendered
	* @param InDPGIndex - current depth priority group of the scene
	* @param InDrawingContext - contex for the given draw policy type
	* @param InPreFog - rendering is occuring before fog
	* @param bInIsHitTesting - rendering is occuring during hit testing
	* @param bInIsVelocityRendering - rendering is occuring during velocity pass
	*/
	TDynamicPrimitiveDrawer(
		const FViewInfo* InView,
		UINT InDPGIndex,
		const typename DrawingPolicyFactoryType::ContextType& InDrawingContext,
		UBOOL InPreFog,
		UBOOL bInIsHitTesting = FALSE,
		UBOOL bInIsVelocityRendering = FALSE,
		UBOOL bInIsLitTranslucencyDepthDrawing = FALSE
		):
		FPrimitiveDrawInterface(InView),
		View(InView),
		DPGIndex(InDPGIndex),
		DrawingContext(InDrawingContext),
		bPreFog(InPreFog),
		bDirty(FALSE),
		bIsHitTesting(bInIsHitTesting),
		bIsVelocityRendering(bInIsVelocityRendering),
		bIsLitTranslucencyDepthDrawing(bInIsLitTranslucencyDepthDrawing)
	{}

	~TDynamicPrimitiveDrawer();

	void SetPrimitive(const FPrimitiveSceneInfo* NewPrimitiveSceneInfo);

	// FPrimitiveDrawInterface interface.
	virtual UBOOL IsHitTesting();
	virtual void SetHitProxy(HHitProxy* HitProxy);
	virtual void RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource);
	virtual UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy) const;
	virtual INT DrawMesh(const FMeshBatch& Mesh);
	virtual void DrawSprite(
		const FVector& Position,
		FLOAT SizeX,
		FLOAT SizeY,
		const FTexture* Sprite,
		const FLinearColor& Color,
		BYTE DepthPriorityGroup,
		FLOAT U,
		FLOAT UL,
		FLOAT V,
		FLOAT VL,
		BYTE BlendMode = SE_BLEND_Masked
		);
	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		BYTE DepthPriorityGroup,
		const FLOAT Thickness = 0.0f
		);
	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		FLOAT PointSize,
		BYTE DepthPriorityGroup
		);

	// Accessors.
	UBOOL IsPreFog() const
	{
		return bPreFog;
	}
	UBOOL IsDirty() const
	{
		return bDirty;
	}
	void ClearDirtyFlag()
	{
		bDirty = FALSE;
	}

	/**
	 * @return TRUE if rendering is occuring during velocity pass 
	 */
	virtual UBOOL IsRenderingVelocities() const { return bIsVelocityRendering; }

private:
	/** The view which is being rendered. */
	const FViewInfo* const View;

	/** The DPG which is being rendered. */
	const UINT DPGIndex;

	/** The drawing context passed to the drawing policy for the mesh elements rendered by this drawer. */
	typename DrawingPolicyFactoryType::ContextType DrawingContext;

	/** The primitive being rendered. */
	const FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** The current hit proxy ID being rendered. */
	FHitProxyId HitProxyId;

	/** The batched simple elements. */
	FBatchedElements BatchedElements;

	/** The dynamic resources which have been registered with this drawer. */
	TArray<FDynamicPrimitiveResource*,SceneRenderingAllocator> DynamicResources;

	/** TRUE if fog has not yet been rendered. */
	BITFIELD bPreFog : 1;

	/** Tracks whether any elements have been rendered by this drawer. */
	BITFIELD bDirty : 1;

	/** TRUE if hit proxies are being drawn. */
	BITFIELD bIsHitTesting : 1;

	/** TRUE if rendering is occuring during velocity pass */
	BITFIELD bIsVelocityRendering : 1;

	/** TRUE if we are rendering depth pass for dynamic lit translucency */
	BITFIELD bIsLitTranslucencyDepthDrawing : 1;
};

/**
 * Draws a view's elements with the specified drawing policy factory type.
 * @param View - The view to draw the meshes for.
 * @param DrawingContext - The drawing policy type specific context for the drawing.
 * @param DPGIndex - The depth priority group to draw the elements from.
 * @param bPreFog - TRUE if the draw call is occurring before fog has been rendered.
 */
template<class DrawingPolicyFactoryType>
UBOOL DrawViewElements(
	const FViewInfo& View,
	const typename DrawingPolicyFactoryType::ContextType& DrawingContext,
	UINT DPGIndex,
	UBOOL bPreFog
	);

/**
 * Draws a given set of dynamic primitives to a RHI command interface, using the specified drawing policy type.
 * @param View - The view to draw the meshes for.
 * @param DrawingContext - The drawing policy type specific context for the drawing.
 * @param DPGIndex - The depth priority group to draw the elements from.
 * @param bPreFog - TRUE if the draw call is occurring before fog has been rendered.
 */
template<class DrawingPolicyFactoryType>
UBOOL DrawDynamicPrimitiveSet(
	const FViewInfo& View,
	const typename DrawingPolicyFactoryType::ContextType& DrawingContext,
	UINT DPGIndex,
	UBOOL bPreFog,
	UBOOL bIsHitTesting = FALSE
	);

/** A primitive draw interface which adds the drawn elements to the view's batched elements. */
class FViewElementPDI : public FPrimitiveDrawInterface
{
public:

	FViewElementPDI(FViewInfo* InViewInfo,FHitProxyConsumer* InHitProxyConsumer);

	// FPrimitiveDrawInterface interface.
	virtual UBOOL IsHitTesting();
	virtual void SetHitProxy(HHitProxy* HitProxy);
	virtual void RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource);
	virtual void DrawSprite(
		const FVector& Position,
		FLOAT SizeX,
		FLOAT SizeY,
		const FTexture* Sprite,
		const FLinearColor& Color,
		BYTE DepthPriorityGroup,
		FLOAT U,
		FLOAT UL,
		FLOAT V,
		FLOAT VL,
		BYTE BlendMode = SE_BLEND_Masked
		);
	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		BYTE DepthPriorityGroup,
		const FLOAT Thickness = 0.0f
		);
	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		FLOAT PointSize,
		BYTE DepthPriorityGroup
		);
	virtual INT DrawMesh(const FMeshBatch& Mesh);

private:
	FViewInfo* ViewInfo;
	TRefCountPtr<HHitProxy> CurrentHitProxy;
	FHitProxyConsumer* HitProxyConsumer;
};

#include "DynamicPrimitiveDrawing.inl"

#endif
