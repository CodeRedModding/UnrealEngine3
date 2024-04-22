/*=============================================================================
BokehDOF.h: High quality Depth of Field post process
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


#ifndef _BORKEH_DOF_H
#define _BORKEH_DOF_H

#if WITH_D3D11_TESSELLATION

/** 
* Vertex buffer
*/
class FBokehVertexBuffer : public FVertexBuffer
{
public:
	/** 
	* Initialize the RHI for this rendering resource 
	*/
	void InitRHI();
};

class FBokehDOFRenderer
{
public:
	// @param BokehTexture must not be 0
	void RenderBokehDOF(
		FViewInfo& View,
		const FDepthOfFieldParams& Params,
		FLOAT BlurKernelSize,
		EDOFQuality QualityLevel,
		UTexture2D* BokehTexture,
		UBOOL bSeparateTranslucency);

	/** Has the verts for the technique */
	static TGlobalResource<FBokehVertexBuffer> VertexBuffer;

	// ---------------------------------------------------------

	// @param BokehTexture can be 0
	template <UBOOL bSeparateTranslucency>
	void RenderBokehDOFQualityTempl(
		FViewInfo& View,
		const FDepthOfFieldParams& DepthOfFieldParams,
		FLOAT BlurKernelSize,
		UTexture2D* BokehTexture,
		EDOFQuality QualityLevel);
};

#endif // WITH_D3D11_TESSELLATION
#endif //_BORKEH_DOF_H
