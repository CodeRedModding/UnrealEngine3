/*=============================================================================
	Wireframe.hlsl: Shader for rendering a wireframe object.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "Common.hlsl"
#include "NoUserShader.hlsl"
#include "VertexFactory.hlsl"

float3	WireColor;

float4 pixelShader(float4 ScreenPosition : TEXCOORD0) : COLOR0
{
	return float4(WireColor,ScreenPosition.w);
}

float4	HitProxyId;

float4 hitProxyPixelShader() : COLOR0
{
	return HitProxyId;
}

struct vertexShaderOutput
{
	float4	Position		: POSITION;
	float4	ScreenPosition	: TEXCOORD0;
};

float4x4	ViewProjectionMatrix;

vertexShaderOutput vertexShader(vertexInput Input)
{
	vertexFactoryInterpolants	Interpolants;
	vertexShaderOutput			Result;
	Result.ScreenPosition = Result.Position = mul(ViewProjectionMatrix,vertexFactoryGetWorldPosition(Input,Interpolants));
	return Result;
}