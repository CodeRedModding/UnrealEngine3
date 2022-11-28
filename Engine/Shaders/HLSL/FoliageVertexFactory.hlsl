/*=============================================================================
	FoliageVertexFactory.hlsl: Foliage vertex factory shader code.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

struct vertexInput
{
	float4	Position		: POSITION;
	float3	TangentX		: TANGENT,
			TangentY		: BINORMAL,
			TangentZ		: NORMAL;
	float2	TexCoord		: TEXCOORD0,
			LightMapCoord	: TEXCOORD1;
	int4	PointSources	: BLENDINDICES;
	float	SwayStrength	: BLENDWEIGHT;
};

struct vertexFactoryInterpolants
{
	float2	TexCoord		: TEXCOORD0,
			LightMapCoord	: TEXCOORD1;
};

userShaderInput getUserShaderInputs(vertexFactoryInterpolants Interpolants)
{
	userShaderInput	Result;
#if NUM_USER_TEXCOORDS
	for(int CoordinateIndex = 0;CoordinateIndex < NUM_USER_TEXCOORDS;CoordinateIndex++)
		Result.UserTexCoords[CoordinateIndex] = Interpolants.TexCoord;
#endif
	Result.UserVertexColor = 1;
	Result.TangentNormal = Result.TangentCameraVector = Result.TangentReflectionVector = 0;
	return Result;
}

float2 getLightMapCoordinate(vertexFactoryInterpolants Interpolants)
{
	return Interpolants.LightMapCoord;
}

float4	WindPointSources[200];
float4	WindDirectionalSources[8];
float	InvWindSpeed;

float2 PointSourceWind(int SourceIndex,float3 Position)
{
	if(SourceIndex < 100)
	{
		float3	SourceLocation = WindPointSources[SourceIndex * 2 + 0].xyz;
		float	Strength = WindPointSources[SourceIndex * 2 + 0].w,
				Phase = WindPointSources[SourceIndex * 2 + 1].x,
				Frequency = WindPointSources[SourceIndex * 2 + 1].y,
				InvRadius = WindPointSources[SourceIndex * 2 + 1].z,
				InvDuration = WindPointSources[SourceIndex * 2 + 1].w;

		float3	WindVector = Position - SourceLocation;
		float	Distance = length(WindVector),
				LocalPhase = Phase - Distance * InvWindSpeed;

		if(LocalPhase < 0.0)
			return 0;
		else
			return normalize(WindVector.xy) * sin(LocalPhase * Frequency * 2.0 * PI) * max(1.0 - LocalPhase * InvDuration,0.0) * Strength * max(1.0f - Distance * InvRadius,0.0);
	}
	else
		return 0;
}

float2 DirectionSourceWind(int SourceIndex,float3 Position)
{
	float3	SourceDirection = WindDirectionalSources[SourceIndex * 2 + 0].xyz;
	float	Strength = WindDirectionalSources[SourceIndex * 2 + 0].w,
			Phase = WindDirectionalSources[SourceIndex * 2 + 1].x,
			Frequency = WindDirectionalSources[SourceIndex * 2 + 1].y;

	float	Distance = dot(SourceDirection,Position),
			LocalPhase = Phase - Distance * InvWindSpeed;

	return SourceDirection.xy * Strength * sin(LocalPhase * Frequency * 2.0 * PI);
}

float4 vertexFactoryGetWorldPosition(vertexInput Input,out vertexFactoryInterpolants Interpolants)
{
	float4	WorldPosition = Input.Position;

	WorldPosition.xy += PointSourceWind(Input.PointSources[0],Input.Position) * Input.SwayStrength;
	WorldPosition.xy += PointSourceWind(Input.PointSources[1],Input.Position) * Input.SwayStrength;
	WorldPosition.xy += PointSourceWind(Input.PointSources[2],Input.Position) * Input.SwayStrength;
	WorldPosition.xy += PointSourceWind(Input.PointSources[3],Input.Position) * Input.SwayStrength;

	WorldPosition.xy += DirectionSourceWind(0,Input.Position) * Input.SwayStrength;
	WorldPosition.xy += DirectionSourceWind(1,Input.Position) * Input.SwayStrength;
	WorldPosition.xy += DirectionSourceWind(2,Input.Position) * Input.SwayStrength;
	WorldPosition.xy += DirectionSourceWind(3,Input.Position) * Input.SwayStrength;

	Interpolants.TexCoord = Input.TexCoord;
	Interpolants.LightMapCoord = Input.LightMapCoord;

	return WorldPosition;
}

float3 vertexFactoryWorldToTangentSpace(vertexInput Input,float3 WorldVector)
{
	float3x3	TangentBasis = float3x3(Input.TangentX,Input.TangentY,Input.TangentZ) / 127.5 - 1;
	return mul(TangentBasis,WorldVector);
}