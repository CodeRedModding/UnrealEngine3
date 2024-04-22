/*=============================================================================
	UnDOFEffect.cpp: DOF (Depth of Field) post process effect implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

IMPLEMENT_CLASS(UDOFEffect);

/*-----------------------------------------------------------------------------
UDOFEffect
-----------------------------------------------------------------------------*/

/** callback for changed property */
void UDOFEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	MaxNearBlurAmount = Clamp<FLOAT>( MaxNearBlurAmount, 0.f, 1.f );
	MaxFarBlurAmount = Clamp<FLOAT>( MaxFarBlurAmount, 0.f, 1.f );

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Creates a proxy to represent the render info for a post process effect
 * @param WorldSettings - The world's post process settings for the view.
 * @return The proxy object.
 */
FPostProcessSceneProxy* UDOFEffect::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	//deprecated
	return NULL;
}

/**
 * @param View - current view
 * @return TRUE if the effect should be rendered
 */
UBOOL UDOFEffect::IsShown(const FSceneView* View) const
{
	return GSystemSettings.bAllowDepthOfField && Super::IsShown( View );
}


