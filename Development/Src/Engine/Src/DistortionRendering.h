/*=============================================================================
	DistortionRendering.h: Distortion rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** 
* Set of distortion scene prims  
*/
class FDistortionPrimSet
{
public:

	/** 
	* Iterate over the distortion prims and draw their accumulated offsets
	* @param ViewInfo - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @return TRUE if anything was drawn
	*/
	UBOOL DrawAccumulatedOffsets(const class FViewInfo* ViewInfo,UINT DPGIndex,UBOOL bInitializeOffsets);

	/** 
	* Apply distortion using the accumulated offsets as a fullscreen quad
	* @param ViewInfo - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @param CanvasTransform - default canvas transform used by scene rendering
	* @return TRUE if anything was drawn
	*/
	void DrawScreenDistort(const class FViewInfo* ViewInfo,UINT DPGIndex,const FMatrix& CanvasTransform, const FIntRect& QuadRect, const FTexture2DRHIRef& SceneTexture);

	/**
	* Add a new primitive to the list of distortion prims
	* @param PrimitiveSceneInfo - primitive info to add.
	* @param ViewInfo - used to transform bounds to view space
	*/
	void AddScenePrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo,const FViewInfo& ViewInfo);

	/** 
	* @return number of prims to render
	*/
	INT NumPrims() const
	{
		return Prims.Num();
	}

	/** 
	* @return a prim currently set to render
	*/
	const FPrimitiveSceneInfo* GetPrim(INT i)const
	{
		check(i>=0 && i<NumPrims());
		return Prims(i);
	}

private:
	/** list of distortion prims added from the scene */
	TArray<FPrimitiveSceneInfo*> Prims;

	/** bound shader state for applying fullscreen distort */
	static FGlobalBoundShaderState ApplyScreenBoundShaderState;

	/** bound shader state for transfering shader complexity from the accumulation target to scene color */
	static FGlobalBoundShaderState ShaderComplexityTransferBoundShaderState;
};
