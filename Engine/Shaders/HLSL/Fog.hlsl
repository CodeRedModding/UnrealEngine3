/*=============================================================================
	Fog.hlsl: Scene fogging pixel shaders.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

sampler		SceneTexture;

static const float FLT_EPSILON = 0.001f;

float4 linePlaneIntersection(float3 RelativeB,float4 RelativeZ)
{
	return RelativeZ / (abs(RelativeB.z) <= FLT_EPSILON ? FLT_EPSILON : RelativeB.z);
}

float4  FogDistanceScale;
float3  FogInScattering[4];

float4 fogScene(float2 UV : TEXCOORD0,float4 ScreenVector : TEXCOORD1,float4 MinHeightRelativeZ : TEXCOORD2,float4 MaxHeightRelativeZ : TEXCOORD3) : COLOR0
{
	half3	SceneColor = tex2D(SceneTexture,UV).rgb;
	half	SceneDepth = tex2D(SceneTexture,UV).a;

	float3	WorldPosition = ScreenVector * clamp(SceneDepth,1,65500);

	float	Distance = length(WorldPosition);

    float4	MinHeightPercent = linePlaneIntersection(WorldPosition,MinHeightRelativeZ),
			MaxHeightPercent = linePlaneIntersection(WorldPosition,MaxHeightRelativeZ),
			MinPercent = max(WorldPosition.z <= 0 ? MaxHeightPercent : MinHeightPercent,0),
			MaxPercent = min(WorldPosition.z <= 0 ? MinHeightPercent : MaxHeightPercent,1);
	half4	Scattering = exp2(Distance * FogDistanceScale * max(MaxPercent - MinPercent,0)),
			InScattering = Scattering - 1;

	for(int LayerIndex = 0;LayerIndex < 4;LayerIndex++)
	{
		SceneColor = SceneColor * Scattering[LayerIndex] + InScattering[LayerIndex] * FogInScattering[LayerIndex];
	}

	return float4(SceneColor,SceneDepth);
}

struct vertexShaderOutput
{
	float4	Position : POSITION;
	float2	TexCoord : TEXCOORD0;
	float4	ScreenVector : TEXCOORD1;
	float4	MinHeightRelativeZ : TEXCOORD2;
	float4	MaxHeightRelativeZ : TEXCOORD3;
	float4	Dummy : TEXCOORD4;
};

float4	FogMinHeight;
float4	FogMaxHeight;

float4		ScreenPositionScaleBias;
float4x4	ScreenToWorld;
float3		CameraPosition;

vertexShaderOutput vertexShader(float4 Position : POSITION,float2 TexCoord : TEXCOORD0)
{
	vertexShaderOutput Result;
	Result.Position = Position;
	Result.TexCoord = TexCoord;
	Result.ScreenVector = mul(ScreenToWorld,float4((TexCoord - ScreenPositionScaleBias.wz) / ScreenPositionScaleBias.xy,1,0));
	Result.Dummy = mul(ScreenToWorld,float4((TexCoord - ScreenPositionScaleBias.wz) / ScreenPositionScaleBias.xy,1,1));
	Result.MinHeightRelativeZ = FogMinHeight - CameraPosition.z;
	Result.MaxHeightRelativeZ = FogMaxHeight - CameraPosition.z;
	return Result;
}