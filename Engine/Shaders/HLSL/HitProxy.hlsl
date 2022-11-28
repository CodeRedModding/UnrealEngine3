/*=============================================================================
	HitProxy.hlsl: Shader for drawing a hit proxy ID.
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
	float4						ScreenPosition	: TEXCOORD6;
	float3						CameraVector	: TEXCOORD7;
};

float4 HitProxyId;

float4 pixelShader(pixelShaderInput Input) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);
	calcUserVectors(UserShaderInput,Input.CameraVector);

	userClipping(UserShaderInput);

	return HitProxyId;
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