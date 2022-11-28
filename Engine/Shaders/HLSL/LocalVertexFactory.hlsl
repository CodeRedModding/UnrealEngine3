/*=============================================================================
	LocalVertexFactory.hlsl: Local vertex factory shader code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

float4x4	LocalToWorld;
float3x3	WorldToLocal;

struct vertexInput
{
	float4	Position	: POSITION;
	half3	TangentX	: TANGENT;
	half3	TangentY	: BINORMAL;
	half3	TangentZ	: NORMAL;
#if NUM_USER_TEXCOORDS
	float2	UserTexCoords[NUM_USER_TEXCOORDS] : TEXCOORD0;
#endif
	float2	LightMapCoordinate : COLOR;
};

struct vertexFactoryInterpolants
{
	float2	LightMapCoordinate					: TEXCOORD0;
#if NUM_USER_TEXCOORDS
	float2	UserTexCoords[NUM_USER_TEXCOORDS]	: TEXCOORD1;
#endif
};

userShaderInput getUserShaderInputs(vertexFactoryInterpolants Interpolants)
{
	userShaderInput	Result;
#if NUM_USER_TEXCOORDS
	for(int CoordinateIndex = 0;CoordinateIndex < NUM_USER_TEXCOORDS;CoordinateIndex++)
		Result.UserTexCoords[CoordinateIndex] = Interpolants.UserTexCoords[CoordinateIndex];
#endif
	Result.UserVertexColor = 1;
	Result.TangentNormal = Result.TangentCameraVector = Result.TangentReflectionVector = 0;
	return Result;
}

float2 getLightMapCoordinate(vertexFactoryInterpolants Interpolants)
{
	return Interpolants.LightMapCoordinate;
}

float4 vertexFactoryGetWorldPosition(vertexInput Input,out vertexFactoryInterpolants Interpolants)
{
#if NUM_USER_TEXCOORDS
	for(int CoordIndex = 0;CoordIndex < NUM_USER_TEXCOORDS;CoordIndex++)
		Interpolants.UserTexCoords[CoordIndex] = Input.UserTexCoords[CoordIndex];
#endif

	Interpolants.LightMapCoordinate = Input.LightMapCoordinate;

	return mul(LocalToWorld,Input.Position);
}

float3 vertexFactoryWorldToTangentSpace(vertexInput Input,half3 WorldVector)
{
	return mul(float3x3(Input.TangentX,Input.TangentY,Input.TangentZ) / 127.5 - 1,mul(WorldToLocal,WorldVector));
}