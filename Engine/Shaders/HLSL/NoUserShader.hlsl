/*=============================================================================
	NoUserShader.hlsl: Empty shader definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#define NUM_USER_TEXTURES	0
#define NUM_USER_TEXCOORDS	0

struct userShaderInput
{
	float3	TangentNormal,
			TangentReflectionVector,
			TangentCameraVector,
			UserVertexColor;
};