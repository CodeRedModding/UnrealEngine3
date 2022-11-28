/*=============================================================================
	ParticleVertexFactory.hlsl: Particle vertex factory shader code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel.
=============================================================================*/

float4x4	LocalToWorld;
float4		CameraWorldPosition;
float4		CameraRight;
float4		CameraUp;
float4		ScreenAlignment;

struct vertexInput
{
	float4	Position	: POSITION;
	float4	OldPosition	: NORMAL;
	float3	Size		: TANGENT;
	float2	TexCoord	: TEXCOORD0;
	float	Rotation	: BLENDWEIGHT0;
	float4	Color		: COLOR0;
};

struct vertexFactoryInterpolants
{
	float2	TexCoord : TEXCOORD0;
	float4  Color	 : COLOR0;
};

userShaderInput getUserShaderInputs(vertexFactoryInterpolants Interpolants)
{
	userShaderInput	Result;
#if NUM_USER_TEXCOORDS
	for(int CoordinateIndex = 0;CoordinateIndex < NUM_USER_TEXCOORDS;CoordinateIndex++)
		Result.UserTexCoords[CoordinateIndex] = Interpolants.TexCoord;
#endif
	Result.UserVertexColor = Interpolants.Color;
	Result.TangentNormal = Result.TangentCameraVector = Result.TangentReflectionVector = 0;
	return Result;
}

float2 getLightMapCoordinate(vertexFactoryInterpolants Interpolants)
{
	return Interpolants.TexCoord;
}

float3 safeNormalize(float3 V)
{
	return V / sqrt(max(dot(V,V),0.01));
}

void getTangents(vertexInput Input,out float4 Right,out float4 Up)
{
	float4	Position			= mul( LocalToWorld, Input.Position ),
			OldPosition			= mul( LocalToWorld, Input.OldPosition );

	float3	CameraDirection		= safeNormalize(CameraWorldPosition.xyz - Position.xyz),
			ParticleDirection	= safeNormalize(Position.xyz - OldPosition.xyz);

#if 0
	// In case we really want them to face the camera.
	float4	Right_Square		= float4( -safeNormalize( cross( CameraDirection, CameraUp.xyz		) ), 0.0 ),
			Up_Square			= float4(  safeNormalize( cross( CameraDirection, CameraRight.xyz	) ), 0.0 );
#else
	float4	Right_Square		= CameraRight,
			Up_Square			= CameraUp;			
			
	float4	Right_Rotated		= (-1.0 * cos(Input.Rotation) * Up_Square) + (sin(Input.Rotation) * Right_Square),
			Up_Rotated			= (       sin(Input.Rotation) * Up_Square) + (cos(Input.Rotation) * Right_Square);
#endif
	float4	Right_Velocity		= float4( safeNormalize( cross( CameraDirection, ParticleDirection	) ), 0.0 ),
			Up_Velocity			= float4( ParticleDirection, 0.0 );

	//	enum EParticleScreenAlignment
	//	{
	//		PSA_Square,
	//		PSA_Rectangle,
	//		PSA_Velocity
	//	};
	Right				= ScreenAlignment.x > 1.5f ? Right_Velocity : Right_Rotated;
	Up					= ScreenAlignment.x > 1.5f ? Up_Velocity	: Up_Rotated;
}

float4 vertexFactoryGetWorldPosition(vertexInput Input,out vertexFactoryInterpolants Interpolants)
{
	float4	WorldPosition = mul( LocalToWorld, Input.Position ),
			Right,
			Up;

	getTangents(Input,Right,Up);

	WorldPosition += Input.Size.x * ( Input.TexCoord.y - 0.5 ) * Right;
	WorldPosition += Input.Size.y * ( Input.TexCoord.x - 0.5 ) * Up;

	Interpolants.TexCoord	= Input.TexCoord;
	Interpolants.Color		= Input.Color;
	//Interpolants.Color		= Up;

	return WorldPosition;
}

float3 vertexFactoryWorldToTangentSpace(vertexInput Input,float3 WorldVector)
{
	float4	Right,
			Up;
	getTangents(Input,Right,Up);
	return mul(
		float3x3(
			Right.xyz,
			Up.xyz,
			-normalize(cross(Right.xyz,Up.xyz))
			),
		WorldVector
		);
}
