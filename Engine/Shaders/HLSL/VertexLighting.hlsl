/*=============================================================================
	VertexLighting.hlsl: Shader for rendering vertex-lit meshes.
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
	float3						VertexLighting	: TEXCOORD5;
	float4						ScreenPosition	: TEXCOORD6;
	float3						CameraVector	: TEXCOORD7;
};

float4 pixelShader(pixelShaderInput Input) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);
	calcUserVectors(UserShaderInput,Input.CameraVector);

#if SHADERBLENDING_TRANSLUCENT || SHADERBLENDING_ADDITIVE
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
			userEmissive(UserShaderInput) + userDiffuseColor(UserShaderInput) * Input.VertexLighting
			);
}

struct vertexShaderOutput
{
	float4						Position		: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	float3						VertexLighting	: TEXCOORD5;
	float4						ScreenPosition	: TEXCOORD6;
	float3						CameraVector	: TEXCOORD7;
};

float4x4	ViewProjectionMatrix;
float4		CameraPosition;

vertexShaderOutput vertexShader(vertexInput Input,float3 VertexLighting : BLENDWEIGHT1)
{
	vertexShaderOutput	Result;

	float4	WorldPosition = vertexFactoryGetWorldPosition(Input,Result.FactoryInterpolants);
	Result.ScreenPosition = Result.Position = mul(ViewProjectionMatrix,WorldPosition);
	Result.CameraVector.xyz = vertexFactoryWorldToTangentSpace(Input,CameraPosition.xyz - WorldPosition.xyz * CameraPosition.w);
	Result.VertexLighting = VertexLighting;

	return Result;
}