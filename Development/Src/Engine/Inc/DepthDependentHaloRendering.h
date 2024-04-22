/*=============================================================================
DepthDependentHaloRendering.h: Declarations used for the wireframe post-processing.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef __DEPTH_DEPENDENT_HALO_RENDERING_H__
#define __DEPTH_DEPENDENT_HALO_RENDERING_H__

struct FDepthDependentHaloSettings
{
	FLOAT FadeStartDistance;
	FLOAT FadeGradientDistance;
	FLOAT DepthAcceptanceFactor;
	UBOOL bEnablePostEffect;
};

#endif
