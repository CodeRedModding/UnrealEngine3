/*=============================================================================
	DepthOnly.hlsl: Shader for the depth prepass.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "Common.hlsl"
#include "NoUserShader.hlsl"
#include "VertexFactory.hlsl"

float4 pixelShader() : COLOR0
{
	return 0;
}

float4x4 ViewProjectionMatrix;

float4 vertexShader(vertexInput Input) : POSITION
{
	vertexFactoryInterpolants FactoryInterpolants;
	return mul(ViewProjectionMatrix,vertexFactoryGetWorldPosition(Input,FactoryInterpolants));
}