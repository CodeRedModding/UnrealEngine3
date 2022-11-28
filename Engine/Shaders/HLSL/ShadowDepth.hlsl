/*=============================================================================
	ShadowDepth.hlsl: Shader for writing shadow depth.
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
	float4						ShadowPosition	: TEXCOORD7;
};

float	MaxShadowDepth;

float4 pixelShader(pixelShaderInput Input, out float fDepth: DEPTH) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);

	userClipping(UserShaderInput);

	fDepth = max(0.0,Input.ShadowPosition.z / MaxShadowDepth);
	return Input.ShadowPosition.z / MaxShadowDepth;
}

struct vertexShaderOutput
{
	float4						Position		: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	float4						ShadowPosition	: TEXCOORD7;
};

float4x4	ShadowMatrix;
float4x4	ProjectionMatrix;

vertexShaderOutput vertexShader(vertexInput Input)
{
	vertexShaderOutput	Result;

	float4	WorldPosition = vertexFactoryGetWorldPosition(Input,Result.FactoryInterpolants);
	Result.Position = mul(ProjectionMatrix,WorldPosition);
	Result.ShadowPosition = mul(ShadowMatrix,WorldPosition);

	return Result;
}