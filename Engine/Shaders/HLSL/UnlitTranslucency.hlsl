/*=============================================================================
	UnlitTranslucency.hlsl: Shader for rendering unlit translucency
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
	half4						Fog				: TEXCOORD5;
	float4						ScreenPosition	: TEXCOORD6;
	float3						CameraVector	: TEXCOORD7;
};

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

	half	Opacity = userOpacity(UserShaderInput);

#if SHADERBLENDING_TRANSLUCENT
	return float4(userEmissive(UserShaderInput) * Input.Fog.a + Input.Fog.rgb,Opacity);
#elif SHADERBLENDING_ADDITIVE
	return float4(userEmissive(UserShaderInput) * Input.Fog.a,Opacity);
#else
	return 0;
#endif
}

struct vertexShaderOutput
{
	float4						Position		: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	half4						Fog				: TEXCOORD5;
	float4						ScreenPosition	: TEXCOORD6;
	float3						CameraVector	: TEXCOORD7;
};

float4x4	ViewProjectionMatrix;
float4		CameraPosition;

static const float FLT_EPSILON = 0.001f;

float4 linePlaneIntersection(float3 A,float3 B,float4 Z)
{
	return (Z - A.z) / (abs(B.z - A.z) <= FLT_EPSILON ? FLT_EPSILON : (B.z - A.z));
}

float4  FogDistanceScale;
float4	FogMinHeight;
float4	FogMaxHeight;
float3  FogInScattering[4];

vertexShaderOutput vertexShader(vertexInput Input)
{
	vertexShaderOutput	Result;

	float4	WorldPosition = vertexFactoryGetWorldPosition(Input,Result.FactoryInterpolants);

	float	Distance = length(WorldPosition - CameraPosition);

    float4	MinHeightPercent = linePlaneIntersection(CameraPosition,WorldPosition,FogMinHeight),
			MaxHeightPercent = linePlaneIntersection(CameraPosition,WorldPosition,FogMaxHeight),
			MinPercent = max(WorldPosition.z <= CameraPosition.z ? MaxHeightPercent : MinHeightPercent,0),
			MaxPercent = min(WorldPosition.z <= CameraPosition.z ? MinHeightPercent : MaxHeightPercent,1);
	half4	Scattering = exp2(Distance * FogDistanceScale * max(MaxPercent - MinPercent,0));

	Result.Fog.rgb = 0;
	Result.Fog.a = 1;
	for(int LayerIndex = 0;LayerIndex < 4;LayerIndex++)
	{
		Result.Fog *= Scattering[LayerIndex];
		Result.Fog.rgb += (Scattering[LayerIndex] - 1) * FogInScattering[LayerIndex];
	}

	Result.ScreenPosition = Result.Position = mul(ViewProjectionMatrix,WorldPosition);
	Result.CameraVector = vertexFactoryWorldToTangentSpace(Input,CameraPosition.xyz - WorldPosition.xyz * CameraPosition.w);

	return Result;
}