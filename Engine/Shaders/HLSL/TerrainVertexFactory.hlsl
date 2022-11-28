/*=============================================================================
	TerrainVertexFactory.hlsl: Local vertex factory shader code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

float4x4	LocalToWorld;

struct vertexInput
{
	float4	Position		: POSITION;
	float	Displacement	: BLENDWEIGHT;
	float2	Gradients		: TANGENT;
};

float	InvMaxTesselationLevel,
		ZScale;
float2	SectionBase,
		InvTerrainSize,
		InvLightMapSize,
		InvSectionSize;

#define TERRAIN_WEIGHTMAP_RESOLUTION	1.0

struct vertexFactoryInterpolants
{
	float3	LocalPosition		: TEXCOORD0;
	float2	WeightMapTexCoord	: TEXCOORD1,
			LightMapTexCoord	: TEXCOORD2;
};

userShaderInput getUserShaderInputs(vertexFactoryInterpolants Interpolants)
{
	userShaderInput	Result;
#if NUM_USER_TEXCOORDS >= 1
	Result.UserTexCoords[0] = Interpolants.WeightMapTexCoord;
#if NUM_USER_TEXCOORDS >= 2
	Result.UserTexCoords[1] = Interpolants.LightMapTexCoord;
#if NUM_USER_TEXCOORDS >= 3
	Result.UserTexCoords[2] = Interpolants.LocalPosition.xy;
#if NUM_USER_TEXCOORDS >= 4
	Result.UserTexCoords[3] = Interpolants.LocalPosition.xz;
#if NUM_USER_TEXCOORDS >= 5
	Result.UserTexCoords[4] = Interpolants.LocalPosition.yz;
#endif
#endif
#endif
#endif
#endif
	Result.UserVertexColor = 1;
	Result.TangentNormal = Result.TangentCameraVector = Result.TangentReflectionVector = 0;
	return Result;
}

float2 getLightMapCoordinate(vertexFactoryInterpolants Interpolants)
{
	return Interpolants.LightMapTexCoord;
}

float4 vertexFactoryGetWorldPosition(vertexInput Input,out vertexFactoryInterpolants Interpolants)
{
	float3	LocalPosition = float3(Input.Position.xy * InvMaxTesselationLevel,(-32768 + (Input.Position.z + 256.0 * Input.Position.w)) * ZScale),
			LocalTangentX = float3(1,0,Input.Gradients.x * ZScale),
			LocalTangentY = float3(0,1,Input.Gradients.y * ZScale),
			LocalTangentZ = normalize(cross(LocalTangentX,LocalTangentY));

	Interpolants.LocalPosition = float3(SectionBase,0) + LocalPosition;
	Interpolants.WeightMapTexCoord = (Interpolants.LocalPosition.xy + 0.5 / TERRAIN_WEIGHTMAP_RESOLUTION) * InvTerrainSize;
	Interpolants.LightMapTexCoord = LocalPosition * InvSectionSize + 0.5 * InvLightMapSize;

	return mul(LocalToWorld,float4(LocalPosition + LocalTangentZ * Input.Displacement,1));
}

float3 vertexFactoryWorldToTangentSpace(vertexInput Input,float3 WorldVector)
{
	float3	LocalTangentX = float3(1,0,Input.Gradients.x * ZScale),
			LocalTangentY = float3(0,1,Input.Gradients.y * ZScale),
			LocalTangentZ = normalize(cross(LocalTangentX,LocalTangentY));

	return mul(float3x3(LocalTangentX,LocalTangentY,LocalTangentZ),mul(WorldVector,LocalToWorld));
}