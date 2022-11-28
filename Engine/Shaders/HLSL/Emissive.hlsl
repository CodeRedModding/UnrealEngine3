/*=============================================================================
	Emissive.hlsl: Shader for outputting the emissive channel of a shader.
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
	float4						ScreenPosition	: TEXCOORD5;
	float3						CameraVector	: TEXCOORD6;
	float3						SkyVector		: TEXCOORD7;
};

float3 SkyColor;

float4 pixelShader(pixelShaderInput Input) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);
	calcUserVectors(UserShaderInput,Input.CameraVector.xyz);

#if (SHADERBLENDING_TRANSLUCENT || SHADERBLENDING_ADDITIVE) && !SHADER_UNLIT
	#if OPAQUELAYER
		// Only draw pixels which are farther than previousDepth.
		clip(Input.ScreenPosition.w - previousDepth(Input.ScreenPosition) - getDepthCompareEpsilon(Input.ScreenPosition.w));
	#else
		// Only draw pixels which are nearer than previousDepth.
		clip(previousDepth(Input.ScreenPosition) - Input.ScreenPosition.w - getDepthCompareEpsilon(Input.ScreenPosition.w));
	#endif
#endif

	return userBlendBase(
			UserShaderInput,
			Input.ScreenPosition,
			userEmissive(UserShaderInput)
#if !SHADER_UNLIT
			+ userHemisphereLightTransfer(UserShaderInput,normalize(Input.SkyVector)) * SkyColor
#endif
			);
}

struct vertexShaderOutput
{
	float4						Position		: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	float4						ScreenPosition	: TEXCOORD5;
	float3						CameraVector	: TEXCOORD6;
	float3						SkyVector		: TEXCOORD7;
};

float4x4	ViewProjectionMatrix;
float4		CameraPosition;

vertexShaderOutput vertexShader(vertexInput Input)
{
	vertexShaderOutput	Result;

	float4	WorldPosition = vertexFactoryGetWorldPosition(Input,Result.FactoryInterpolants);
	Result.ScreenPosition = Result.Position = mul(ViewProjectionMatrix,WorldPosition);
	Result.CameraVector = vertexFactoryWorldToTangentSpace(Input,CameraPosition.xyz - WorldPosition.xyz * CameraPosition.w);
	Result.SkyVector = vertexFactoryWorldToTangentSpace(Input,float3(0,0,1));

	return Result;
}