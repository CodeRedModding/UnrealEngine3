/*=============================================================================
	ParticleSubUVVertexFactory.hlsl: Particle vertex factory shader code.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Scott Sherman.
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
	float2	TexCoord1	: TEXCOORD1;
	float2	Interp		: TEXCOORD2;
	float2  Sizer		: TEXCOORD3;
};

struct vertexFactoryInterpolants
{
	float2	TexCoord	: TEXCOORD0;
	float2	TexCoord1	: TEXCOORD1;
	float4  Color		: COLOR0;
	float2  Interp      : TEXCOORD2;
	float2  Sizer		: TEXCOORD3;
};

userShaderInput getUserShaderInputs(vertexFactoryInterpolants Interpolants)
{
	userShaderInput	Result;

#if NUM_USER_TEXCOORDS
	#if NUM_USER_TEXCOORDS >= 1
		Result.UserTexCoords[0] = Interpolants.TexCoord;
	#if NUM_USER_TEXCOORDS >= 2
		Result.UserTexCoords[1] = Interpolants.TexCoord1;
	#if NUM_USER_TEXCOORDS >= 3
		Result.UserTexCoords[2] = Interpolants.Interp;
	#if NUM_USER_TEXCOORDS >= 4
		Result.UserTexCoords[3] = Interpolants.Sizer;
	#endif	// >= 4
	#endif	// >= 3
	#endif	// >= 2
	#endif	// >= 1

	for(int CoordinateIndex = 4;CoordinateIndex < NUM_USER_TEXCOORDS;CoordinateIndex++)
		Result.UserTexCoords[CoordinateIndex] = Interpolants.TexCoord;
#endif

	Result.UserVertexColor = Interpolants.Color;
	Result.TangentNormal = Result.TangentCameraVector = Result.TangentReflectionVector = 0;
	return Result;
}

float2 getLightMapCoordinate(vertexFactoryInterpolants Interpolants)
{
	return Interpolants.TexCoord[0];
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

	WorldPosition += Input.Size.x * ( Input.Sizer.y - 0.5 ) * Right;
	WorldPosition += Input.Size.y * ( Input.Sizer.x - 0.5 ) * Up;

	Interpolants.TexCoord	= Input.TexCoord;
	Interpolants.TexCoord1	= Input.TexCoord1;
	Interpolants.Interp		= Input.Interp;
	Interpolants.Sizer      = Input.Sizer;
	Interpolants.Color		= Input.Color;

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
