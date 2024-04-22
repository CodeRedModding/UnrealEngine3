/*================================================================================
	MeshPaintRendering.h: Mesh texture paint brush rendering
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#ifndef __MeshPaintRendering_h__
#define __MeshPaintRendering_h__

#ifdef _MSC_VER
	#pragma once
#endif



namespace MeshPaintRendering
{
	/** Batched element parameters for mesh paint shaders */
	struct FMeshPaintShaderParameters
	{

	public:

		// @todo MeshPaint: Should be serialized no?
		UTextureRenderTarget2D* CloneTexture;
		
		FMatrix WorldToBrushMatrix;

		FLOAT BrushRadius;
		FLOAT BrushRadialFalloffRange;
		FLOAT BrushDepth;
		FLOAT BrushDepthFalloffRange;
		FLOAT BrushStrength;
		FLinearColor BrushColor;
		UBOOL RedChannelFlag;
		UBOOL BlueChannelFlag;
		UBOOL GreenChannelFlag;
		UBOOL AlphaChannelFlag;
		UBOOL GenerateMaskFlag;


	};


	/** Batched element parameters for mesh paint dilation shaders used for seam painting */
	struct FMeshPaintDilateShaderParameters
	{

	public:

		UTextureRenderTarget2D* Texture0;
		UTextureRenderTarget2D* Texture1;
		UTextureRenderTarget2D* Texture2;

		FLOAT WidthPixelOffset;
		FLOAT HeightPixelOffset;

	};


	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void SetMeshPaintShaders_RenderThread( const FMatrix& InTransform,
										   const FLOAT InGamma,
										   const FMeshPaintShaderParameters& InShaderParams );

	/** Binds the mesh paint dilation vertex and pixel shaders to the graphics device */
	void SetMeshPaintDilateShaders_RenderThread( const FMatrix& InTransform,
												const FLOAT InGamma,
												const FMeshPaintDilateShaderParameters& InShaderParams );

}



#endif	// __MeshPaintRendering_h__
