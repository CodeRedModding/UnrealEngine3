/*=============================================================================
	Unlit.hlsl: Shader for drawing a material unlit.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "Common.hlsl"
#include "UserShader.hlsl"
#include "VertexFactory.hlsl"

struct pixelShaderInput
{
	vertexFactoryInterpolants	FactoryInterpolants;
	float4						ScreenPosition	: TEXCOORD6;
	float3						CameraVector	: TEXCOORD7;
};

float4 pixelShader(pixelShaderInput Input) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);
	calcUserVectors(UserShaderInput,Input.CameraVector);

#if (SHADERBLENDING_TRANSLUCENT || SHADERBLENDING_ADDITIVE) && !SHADER_UNLIT
	#if OPAQUELAYER
		// Only draw pixels which are farther than previousDepth.
		clip(Input.ScreenPosition.w - previousDepth(Input.ScreenPosition) - getDepthCompareEpsilon(Input.ScreenPosition.w));
	#else
		// Only draw pixels which are nearer than previousDepth.
		clip(previousDepth(Input.ScreenPosition) - Input.ScreenPosition.w - getDepthCompareEpsilon(Input.ScreenPosition.w));
	#endif
#endif

	return
#if SHADER_UNLIT
		userBlendBase(UserShaderInput,Input.ScreenPosition,userEmissive(UserShaderInput));
#else
		userBlendBase(UserShaderInput,Input.ScreenPosition,userEmissive(UserShaderInput) + userDiffuseColor(UserShaderInput));
#endif
}

struct vertexShaderOutput
{
	float4						Position		: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	float4						ScreenPosition	: TEXCOORD6;
	float3						CameraVector	: TEXCOORD7;
};

float4x4	ViewProjectionMatrix;
float4		CameraPosition;

vertexShaderOutput vertexShader(vertexInput Input)
{
	vertexShaderOutput	Result;

	float4	WorldPosition = vertexFactoryGetWorldPosition(Input,Result.FactoryInterpolants);
	Result.ScreenPosition = Result.Position = mul(ViewProjectionMatrix,WorldPosition);
	Result.CameraVector.xyz = vertexFactoryWorldToTangentSpace(Input,CameraPosition.xyz - WorldPosition.xyz * CameraPosition.w);

	return Result;
}