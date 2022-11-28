/*=============================================================================
	ShadowDepthTest.hlsl: Shader for performing shadow depth test.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "NoUserShader.hlsl"
#include "Common.hlsl"
#include "VertexFactory.hlsl"

struct pixelShaderInput
{
	vertexFactoryInterpolants	FactoryInterpolants;
	float4						ScreenPosition : TEXCOORD7;
};

sampler		ShadowDepthTexture;
float		InvMaxShadowDepth;

float4x4	ShadowMatrix;

float4	SampleOffsets[NUM_SAMPLE_CHUNKS * 2];

float2	ShadowTexCoordScale;

float4 pixelShader(pixelShaderInput Input) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);

	float4	ShadowPosition = mul(ShadowMatrix,float4((Input.ScreenPosition.xy/Input.ScreenPosition.w)*previousDepth(Input.ScreenPosition),previousDepth(Input.ScreenPosition),1));
	ShadowPosition.xy = ShadowPosition.xy * ShadowTexCoordScale;
	ShadowPosition.z = min(ShadowPosition.z * InvMaxShadowDepth,1.0) - 0.008;

	float	Shadow = 0.0;
	float2	ShadowTexCoord = ShadowPosition.xy / ShadowPosition.w;
	float	TestDepth = ShadowPosition.z;

	for(int ChunkIndex = 0;ChunkIndex < NUM_SAMPLE_CHUNKS;ChunkIndex++)
	{
		float4	ShadowDepths;

		ShadowDepths.r = tex2D(ShadowDepthTexture,ShadowTexCoord + SampleOffsets[ChunkIndex * 2 + 0].xy).r;
		ShadowDepths.g = tex2D(ShadowDepthTexture,ShadowTexCoord + SampleOffsets[ChunkIndex * 2 + 0].wz).r;
		ShadowDepths.b = tex2D(ShadowDepthTexture,ShadowTexCoord + SampleOffsets[ChunkIndex * 2 + 1].xy).r;
		ShadowDepths.a = tex2D(ShadowDepthTexture,ShadowTexCoord + SampleOffsets[ChunkIndex * 2 + 1].wz).r;

		Shadow += dot(TestDepth < ShadowDepths,1.0 / (4.0 * NUM_SAMPLE_CHUNKS));
	}

	return ShadowPosition.w > 0.0 ? Shadow : 1.0;
}

sampler		HWShadowDepthTexture;

float4 nvPixelShader(pixelShaderInput Input) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);

	float4	ShadowPosition = mul(ShadowMatrix,float4((Input.ScreenPosition.xy/Input.ScreenPosition.w)*previousDepth(Input.ScreenPosition),previousDepth(Input.ScreenPosition),1));
	ShadowPosition.xy = ShadowPosition.xy * ShadowTexCoordScale;
	ShadowPosition.z = (min(ShadowPosition.z * InvMaxShadowDepth,1.0) - 0.008) * ShadowPosition.w;

	float	Shadow = 0.0,
			SampleWeight = 1.0 / (4.0 * NUM_SAMPLE_CHUNKS);
	for(int ChunkIndex = 0;ChunkIndex < NUM_SAMPLE_CHUNKS;ChunkIndex++)
	{
		float4	ShadowTexCoords[4] = { ShadowPosition, ShadowPosition, ShadowPosition, ShadowPosition };
		ShadowTexCoords[0].xy += SampleOffsets[ChunkIndex * 2 + 0].xy * ShadowPosition.w;
		ShadowTexCoords[1].xy += SampleOffsets[ChunkIndex * 2 + 0].wz * ShadowPosition.w;
		ShadowTexCoords[2].xy += SampleOffsets[ChunkIndex * 2 + 1].xy * ShadowPosition.w;
		ShadowTexCoords[3].xy += SampleOffsets[ChunkIndex * 2 + 1].wz * ShadowPosition.w;
		Shadow += tex2Dproj(HWShadowDepthTexture,ShadowTexCoords[0]).r * SampleWeight;
		Shadow += tex2Dproj(HWShadowDepthTexture,ShadowTexCoords[1]).r * SampleWeight;
		Shadow += tex2Dproj(HWShadowDepthTexture,ShadowTexCoords[2]).r * SampleWeight;
		Shadow += tex2Dproj(HWShadowDepthTexture,ShadowTexCoords[3]).r * SampleWeight;
	}

	return ShadowPosition.w > 0.0 ? Shadow : 1.0;
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