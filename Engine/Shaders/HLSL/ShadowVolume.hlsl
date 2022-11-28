/*=============================================================================
	ShadowVolume.hlsl: Shader for render a shadow volume.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "NoUserShader.hlsl"
#include "Common.hlsl"

float4 pixelShader() : COLOR0
{
	return float4(1,1,1,1);
}

float4x4	LocalToWorld;
float4x4	ViewProjectionMatrix;
float4		LightPosition;

float4 vertexShader(float4 Position : POSITION,float Extrusion : TEXCOORD0) : POSITION
{
	float3	WorldPosition = mul(LocalToWorld,Position);

	return mul(ViewProjectionMatrix,Extrusion > 0.5 ? float4(WorldPosition * LightPosition.w - LightPosition.xyz,0) : float4(WorldPosition,1));
}