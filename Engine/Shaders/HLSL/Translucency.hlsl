/*=============================================================================
	Translucency.hlsl: Finds the nearest layer of translucency that's further than previousDepth.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "Common.hlsl"
#include "UserShader.hlsl"
#include "VertexFactory.hlsl"

struct pixelShaderInput
{
	vertexFactoryInterpolants	FactoryInterpolants;
	float3						CameraVector	: TEXCOORD6;
	float4						ScreenPosition	: TEXCOORD7;
};

float LayerIndex;

float4 pixelShader(pixelShaderInput Input) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);
	calcUserVectors(UserShaderInput,Input.CameraVector.xyz);

	userClipping(UserShaderInput);

	if(LayerIndex > 0.5)
	{
		// Only draw pixels which are farther than previousDepth.
		clip(Input.ScreenPosition.w - previousDepth(Input.ScreenPosition) - getDepthCompareEpsilon(Input.ScreenPosition.w));
	}
	return float4(previousLighting(Input.ScreenPosition),Input.ScreenPosition.w);
}

struct vertexShaderOutput
{
	float4						Position		: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	float3						CameraVector	: TEXCOORD6;
	float4						ScreenPosition	: TEXCOORD7;
};

float4x4	ViewProjectionMatrix;
float4		CameraPosition;

vertexShaderOutput vertexShader(vertexInput Input)
{
	vertexShaderOutput Result;
	float4	WorldPosition = vertexFactoryGetWorldPosition(Input,Result.FactoryInterpolants);
	Result.ScreenPosition = Result.Position = mul(ViewProjectionMatrix,WorldPosition);
	Result.CameraVector = vertexFactoryWorldToTangentSpace(Input,CameraPosition.xyz - WorldPosition.xyz * CameraPosition.w);
	return Result;
}