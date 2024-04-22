/*================================================================================
	LightingTools.h: Lighting Tools helper
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#ifndef __LightingTools_h__
#define __LightingTools_h__

#ifdef _MSC_VER
	#pragma once
#endif

/**
 *	LightingTools settings
 */
class FLightingToolsSettings
{

public:

	/** Static: Returns global lighting tools adjust settings */
	static FLightingToolsSettings& Get()
	{
		return LightingToolsSettings;
	}

	static void Init();

	static UBOOL ApplyToggle();

	static void Reset();

protected:

	/** Static: Global lightmap resolution ratio adjust settings */
	static FLightingToolsSettings LightingToolsSettings;

public:

	/** Constructor */
	FLightingToolsSettings() :
		  bShowLightingBounds(FALSE)
		, bShowShadowTraces(FALSE)
		, bShowDirectOnly(FALSE)
		, bShowIndirectOnly(FALSE)
		, bShowIndirectSamples(FALSE)
		, bShowAffectingDominantLights(FALSE)
		, bSavedShowSelection(FALSE)
		, WindowPositionX(-1)
		, WindowPositionY(-1)
	{
	}

	UBOOL bShowLightingBounds;
	UBOOL bShowShadowTraces;
	UBOOL bShowDirectOnly;
	UBOOL bShowIndirectOnly;
	UBOOL bShowIndirectSamples;
	UBOOL bShowAffectingDominantLights;

	UBOOL bSavedShowSelection;

	/**
	 *	Window settings
	 */
	/** Horizontal window position */
	INT WindowPositionX;

	/** Vertical window position */
	INT WindowPositionY;
};

#endif	// __LightingTools_h__
