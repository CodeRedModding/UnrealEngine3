/*=============================================================================
	OpenGLUtil.h: OpenGL RHI utility definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
* Convert from ECubeFace to GLenum type
* @param Face - ECubeFace type to convert
* @return OpenGL cube face enum value
*/
FORCEINLINE GLenum GetOpenGLCubeFace(ECubeFace Face)
{
	switch(Face)
	{
	case CubeFace_PosX:
	default:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
	case CubeFace_NegX:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
	case CubeFace_PosY:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
	case CubeFace_NegY:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
	case CubeFace_PosZ:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
	case CubeFace_NegZ:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
	};
}
