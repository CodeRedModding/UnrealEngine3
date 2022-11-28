/*=============================================================================
	Tile.hlsl: Shaders for drawing 2D canvas primitives.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

sampler TileTexture;
float4	HitProxyId;

float4 pixelShader(float3 TexCoord : TEXCOORD0,float4 Color : TEXCOORD1) : COLOR0
{
	return tex3D(TileTexture,TexCoord) * Color;
}

float4 hitProxyPixelShader(float3 TexCoord : TEXCOORD0,float4 Color : TEXCOORD1) : COLOR0
{
	return float4(HitProxyId.rgb,tex3D(TileTexture,TexCoord).a * Color.a);
}

float4 solidPixelShader(float4 Color : TEXCOORD1) : COLOR0
{
	return Color;
}

float4 solidHitProxyPixelShader(float4 Color : TEXCOORD1) : COLOR0
{
	return float4(HitProxyId.rgb,Color.a);
}

struct vertexShaderOutput
{
	float4	Position : POSITION;
	float3	TexCoord : TEXCOORD0;
	float4	Color : TEXCOORD1;
};

vertexShaderOutput vertexShader(float4 Position : POSITION,float3 TexCoord : TEXCOORD0,float4 Color : TEXCOORD1)
{
	vertexShaderOutput Result;
	Result.Position = Position;
	Result.TexCoord = TexCoord;
	Result.Color = Color;
	return Result;
}