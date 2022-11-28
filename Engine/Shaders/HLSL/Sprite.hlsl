/*=============================================================================
	Sprite.hlsl: Shader for rendering a sprite.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

sampler	SpriteTexture;
float4	SpriteColor;

float4 pixelShader(float2 UV : TEXCOORD0,float Depth : TEXCOORD1) : COLOR0
{
	float4	Result = tex2D(SpriteTexture,UV) * SpriteColor;
	clip(Result.a - 0.5);
	return float4(Result.rgb,Depth);
}

float4	HitProxyId;

float4 hitProxyPixelShader(float2 UV : TEXCOORD0,float Depth : TEXCOORD1) : COLOR0
{
	float4	Result = tex2D(SpriteTexture,UV) * SpriteColor;
	clip(Result.a - 0.5);
	return HitProxyId;
}

struct vertexShaderOutput
{
	float4	Position	: POSITION;
	float2	UV			: TEXCOORD0;
	float	Depth		: TEXCOORD1;
};

vertexShaderOutput vertexShader(float4 Position : POSITION,float2 UV : TEXCOORD0)
{
	vertexShaderOutput	Result;
	Result.Position = Position;
	Result.UV = UV;
	Result.Depth = Result.Position.w;
	return Result;
}