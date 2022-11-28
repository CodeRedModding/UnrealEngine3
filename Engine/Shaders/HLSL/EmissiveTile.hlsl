/*=============================================================================
	EmissiveTile.hlsl: Shader for outputting the emissive channel of a shader on a tile.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Common.hlsl"
#include "UserShader.hlsl"

float4 pixelShader(float2 TexCoord : TEXCOORD0) : COLOR0
{
	userShaderInput	UserShaderInput;
#if NUM_USER_TEXCOORDS
	for(int CoordinateIndex = 0;CoordinateIndex < NUM_USER_TEXCOORDS;CoordinateIndex++)
		UserShaderInput.UserTexCoords[CoordinateIndex] = TexCoord;
#endif
	UserShaderInput.UserVertexColor = 1;
	UserShaderInput.TangentNormal = UserShaderInput.TangentCameraVector = UserShaderInput.TangentReflectionVector = 0;
	calcUserVectors(UserShaderInput,float3(0,0,1));

	userClipping(UserShaderInput);

	return float4(pow(userEmissive(UserShaderInput),1.0 / 2.2),userOpacity(UserShaderInput));
}

float4	HitProxyId;

float4 hitProxyPixelShader(float2 TexCoord : TEXCOORD0) : COLOR0
{
	userShaderInput	UserShaderInput;
#if NUM_USER_TEXCOORDS
	for(int CoordinateIndex = 0;CoordinateIndex < NUM_USER_TEXCOORDS;CoordinateIndex++)
		UserShaderInput.UserTexCoords[CoordinateIndex] = TexCoord;
#endif
	UserShaderInput.UserVertexColor = 1;
	UserShaderInput.TangentNormal = UserShaderInput.TangentCameraVector = UserShaderInput.TangentReflectionVector = 0;
	calcUserVectors(UserShaderInput,float3(0,0,1));

	userClipping(UserShaderInput);

	return HitProxyId;
}

struct vertexShaderOutput
{
	float4	Position : POSITION;
	float2	TexCoord : TEXCOORD0;
};

vertexShaderOutput vertexShader(float4 Position : POSITION,float2 TexCoord : TEXCOORD0)
{
	vertexShaderOutput Result;
	Result.Position = Position;
	Result.TexCoord = TexCoord;
	return Result;
}