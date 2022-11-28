/*=============================================================================
	SingleSpotLight.hlsl: Shaders for calculating a single spot light.
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
	half3						TangentLightVector	: TEXCOORD4;
	half3						WorldLightVector	: TEXCOORD5;
	float3						CameraVector		: TEXCOORD6;
	float4						ScreenPosition		: TEXCOORD7;
#if STATICLIGHTING_VERTEXMASK
	half						LightMask			: COLOR0;
#endif
};

float3  LightColor;
float3	SpotDirection;
float2	SpotAngles;
sampler	VisibilityTexture;

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
			userPointLightTransfer(UserShaderInput,normalize((half3)Input.TangentLightVector),Input.WorldLightVector) *
			LightColor *
			square(clamp((dot(normalize((half3)Input.WorldLightVector),-SpotDirection) - SpotAngles.x) * SpotAngles.y,0,1))
			);
}

struct vertexShaderOutput
{
	float4						Position			: POSITION;
	vertexFactoryInterpolants	FactoryInterpolants;
	float3						TangentLightVector	: TEXCOORD4;
	float3						WorldLightVector	: TEXCOORD5;
	float3						CameraVector		: TEXCOORD6;
	float4						ScreenPosition		: TEXCOORD7;
#if STATICLIGHTING_VERTEXMASK
	float						LightMask			: COLOR0;
#endif
};

float4		LightPosition; // w = 1.0 / Radius
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
	Result.TangentLightVector = vertexFactoryWorldToTangentSpace(Input,LightPosition.xyz - WorldPosition.xyz);
	Result.WorldLightVector = (LightPosition.xyz - WorldPosition.xyz) * LightPosition.w;
#if STATICLIGHTING_VERTEXMASK
	Result.LightMask = LightMask;
#endif

	return Result;
}