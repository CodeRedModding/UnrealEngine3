/*=============================================================================
	NullVertexFactory.hlsl: Null vertex factory shader code.  Vertices are already in world space.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

struct vertexInput
{
	float4	Position	: POSITION;
	float3	TangentX	: TANGENT;
	float3	TangentY	: BINORMAL;
	float3	TangentZ	: NORMAL;
#if NUM_USER_TEXCOORDS
	float2	TexCoords[NUM_USER_TEXCOORDS] : TEXCOORD0;
#endif
};

struct vertexFactoryInterpolants
{
#if NUM_USER_TEXCOORDS
	float2	TexCoords[NUM_USER_TEXCOORDS] : TEXCOORD0;
#endif
};

float4 vertexFactoryGetWorldPosition(vertexInput Input,vertexFactoryInterpolants Interpolants)
{
#if NUM_USER_TEXCOORDS
	for(int CoordIndex = 0;CoordIndex < NUM_USER_TEXCOORDS;CoordIndex++)
		Interpolants.TexCoords[CoordIndex] = Input.TexCoords[CoordIndex];
#endif

	return Input.Position;
}

float3 vertexFactoryWorldToTangentSpace(vertexInput Input,float3 WorldVector)
{
	return mul(float3x3(Input.TangentX,Input.TangentY,Input.TangentZ) / 127.5 - 1,WorldVector);
}