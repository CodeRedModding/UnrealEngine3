/*=============================================================================
	ImageReflectionRendering.h: Definitions for rendering image based reflections.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __IMAGEREFLECTIONRENDERING_H__
#define __IMAGEREFLECTIONRENDERING_H__

/** Represents an image reflection object to the rendering thread. */
class FImageReflectionSceneInfo
{
public:

	/** Texture used for this image reflection. */
	const UTexture2D* ReflectionTexture;

	/** Parameters needed in the shader to do a ray-quad intersection. */
	FPlane ReflectionPlane;
	FVector ReflectionOrigin;
	FVector4 ReflectionXAxisAndYScale;

	/** Per instance reflection information. */
	FLinearColor ReflectionColor;
	UBOOL bTwoSided;
	UBOOL bLightReflection;
	UBOOL bEnabled;

	FImageReflectionSceneInfo(const UActorComponent* InComponent, UTexture2D* InReflectionTexture, FLOAT ReflectionScale, const FLinearColor& InReflectionColor, UBOOL bInTwoSided, UBOOL bEnabled);
};

#endif