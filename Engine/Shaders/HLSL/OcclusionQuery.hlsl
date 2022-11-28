/*=============================================================================
	OcclusionQuery.hlsl: Shader for performing an occlusion query.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

float4 pixelShader() : COLOR0
{
	return 0;
}

float4x4	ViewProjectionMatrix;

float4 vertexShader(float4 Position : POSITION) : POSITION
{
	return mul(ViewProjectionMatrix,Position);
}