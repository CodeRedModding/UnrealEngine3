/*=============================================================================
	UnMotionBlurEffect.cpp: Motion blur post process effect implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

IMPLEMENT_CLASS(UMotionBlurEffect);
IMPLEMENT_CLASS(UDOFBloomMotionBlurEffect);

extern UBOOL GMotionBlurWithoutUberPostProcessWarning;

//=============================================================================


/**
 * Creates a proxy to represent the render info for a post process effect
 * @param WorldSettings - The world's post process settings for the view.
 * @return The proxy object.
 */
FPostProcessSceneProxy* UMotionBlurEffect::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	// motion blur now done in uberpostprocesing (we no longer support motionblur without uberpostprocessing in the postprocessing chain)
	return NULL;
}

/**
 * @param View - current view
 * @return TRUE if the effect should be rendered
 */
UBOOL UMotionBlurEffect::IsShown(const FSceneView* View) const
{
	// motion blur is now done exclusively in uber post processing
	return Super::IsShown( View );
}

/**
 * Creates a proxy to represent the render info for a post process effect
 * @param WorldSettings - The world's post process settings for the view.
 * @return The proxy object.
 */
FPostProcessSceneProxy* UDOFBloomMotionBlurEffect::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	// UDOFBloomMotionBlurEffect is only used as a base class for uberpostprocess and doesn't render itself
	return NULL;
}

/**
 * @param View - current view
 * @return TRUE if the effect should be rendered
 */
UBOOL UDOFBloomMotionBlurEffect::IsShown(const FSceneView* View) const
{
	return (GSystemSettings.bAllowBloom || GSystemSettings.bAllowDepthOfField) && Super::IsShown( View );
}

