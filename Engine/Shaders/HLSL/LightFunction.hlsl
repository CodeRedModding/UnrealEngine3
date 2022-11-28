/*=============================================================================
	LightFunction.hlsl: Shader for computing a light function.
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
	float4						ScreenPosition	: TEXCOORD7;
};

float4x4	ScreenToLight;

float4 pixelShader(pixelShaderInput Input) : COLOR0
{
	half	SceneDepth = previousDepth(Input.ScreenPosition);
	float3	LightPosition = float4(mul(ScreenToLight,float4((Input.ScreenPosition.xy/Input.ScreenPosition.w) * SceneDepth,SceneDepth,1)).xyz,1);

	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);

#if NUM_USER_TEXCOORDS
	UserShaderInput.UserTexCoords[0] = LightPosition.xy;
#endif

	UserShaderInput.TangentCameraVector = LightPosition;
	UserShaderInput.TangentNormal = UserShaderInput.TangentReflectionVector = 0;

	return float4(userEmissive(UserShaderInput),1);
}

struct vertexShaderOutput
{
	float4						Position		: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	float4						ScreenPosition	: TEXCOORD7;
};

float4x4	ViewProjectionMatrix;

vertexShaderOutput vertexShader(vertexInput Input)
{
	vertexShaderOutput	Result;

	Result.ScreenPosition = Result.Position = mul(ViewProjectionMatrix,vertexFactoryGetWorldPosition(Input,Result.FactoryInterpolants));

	return Result;
}