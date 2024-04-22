/**
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *	This file is for shared structs and enums that need to be declared before the rest of Engine.
 *  The typical use case is for structs used in the renderer and also in script code.
 */
class EngineBaseTypes extends Object
	native(Base)
	abstract
	config(Engine);

/** 
 * Overrides for rendering settings that can be used to increase performance. 
 */
struct native RenderingPerformanceOverrides
{
	var() bool bAllowAmbientOcclusion;
	var() bool bAllowDominantWholeSceneDynamicShadows;
	var() bool bAllowMotionBlurSkinning;
	var() bool bAllowTemporalAA;
	var() bool bAllowLightShafts;

	structcpptext
	{
		/* default constructor, for script, values are overwritten by serialization afterwards */
		FRenderingPerformanceOverrides()
		{}

		FRenderingPerformanceOverrides(EForceInit)
		{
			bAllowAmbientOcclusion = TRUE;
			bAllowDominantWholeSceneDynamicShadows = TRUE;
			bAllowMotionBlurSkinning = TRUE;
			bAllowTemporalAA = TRUE;
			bAllowLightShafts = TRUE;
		}
	}

	structdefaultproperties
	{
		bAllowAmbientOcclusion=True
		bAllowDominantWholeSceneDynamicShadows=True
		bAllowMotionBlurSkinning=True
		bAllowTemporalAA=True
		bAllowLightShafts=True
	}
};
