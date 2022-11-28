/*=============================================================================
	SingleSkyLight.hlsl: Shaders for calculating a single sky light.
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
	half3						TangentLightVector	: TEXCOORD5;
	float3						CameraVector		: TEXCOORD6;
	float4						ScreenPosition		: TEXCOORD7;
#if STATICLIGHTING_VERTEXMASK
	half						LightMask			: COLOR0;
#endif
};

float3   LightColor;
sampler  VisibilityTexture;

float4 pixelShader(pixelShaderInput Input) : COLOR0
{
	userShaderInput	UserShaderInput = getUserShaderInputs(Input.FactoryInterpolants);
	calcUserVectors(UserShaderInput,Input.CameraVector);

#if STATICLIGHTING_TEXTUREMASK
	half	LightMask = tex2D(VisibilityTexture,getLightMapCoordinate(Input.FactoryInterpolants)).r;
#elif STATICLIGHTING_VERTEXMASK
	half	LightMask = Input.LightMask;
#else
	half	LightMask = 1;
#endif

	return userBlendAdd(
			UserShaderInput,
			Input.ScreenPosition,
			lightFunction(Input.ScreenPosition) *
			LightMask *
			userDirectionalLightTransfer(UserShaderInput,normalize((half3)Input.TangentLightVector)) * 
			LightColor
			);
}

struct vertexShaderOutput
{
	float4						Position			: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	float3						TangentLightVector	: TEXCOORD5;
	float3						CameraVector		: TEXCOORD6;
	float4						ScreenPosition		: TEXCOORD7;
#if STATICLIGHTING_VERTEXMASK
	float						LightMask			: COLOR0;
#endif
};

float3		LightDirection;
float4		CameraPosition;

float4x4	ViewProjectionMatrix;

#if STATICLIGHTING_VERTEXMASK
vertexShaderOutput vertexShader(vertexInput Input,float LightMask : BLENDWEIGHT)
#else
vertexShaderOutput vertexShader(vertexInput Input)
#endif
{
	vertexShaderOutput	Result;

	float4	WorldPosition = vertexFactoryGetWorldPosition(Input,Result.FactoryInterpolants);
	Result.ScreenPosition = Result.Position = mul(ViewProjectionMatrix,WorldPosition);
	Result.CameraVector.xyz = vertexFactoryWorldToTangentSpace(Input,CameraPosition.xyz - WorldPosition.xyz * CameraPosition.w);
	Result.TangentLightVector = vertexFactoryWorldToTangentSpace(Input,LightDirection);
#if STATICLIGHTING_VERTEXMASK
	Result.LightMask = LightMask;
#endif

	return Result;
}