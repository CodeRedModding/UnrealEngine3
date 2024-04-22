/*================================================================================
	LightmapResRatioAdjust.h: Lightmap Resolution Ratio Adjustment helper
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#ifndef __LightmapResRatioAdjust_h__
#define __LightmapResRatioAdjust_h__

#ifdef _MSC_VER
	#pragma once
#endif

/** LightmapResRatioAdjust level options */
namespace ELightmapResRatioAdjustLevels
{
	enum Options
	{
		/** Current level only */
		Current,
		/** Selected levels only */
		Selected,
		/** All loaded levels */
		AllLoaded
	};
}

/**
 *	LightmapResRatioAdjust settings
 */
class FLightmapResRatioAdjustSettings
{

public:

	/** Static: Returns global lightmap resolution ratio adjust settings */
	static FLightmapResRatioAdjustSettings& Get()
	{
		return LightmapResRatioAdjustSettings;
	}

	static UBOOL ApplyRatioAdjustment();

protected:

	/** Static: Global lightmap resolution ratio adjust settings */
	static FLightmapResRatioAdjustSettings LightmapResRatioAdjustSettings;

public:

	/** Constructor */
	FLightmapResRatioAdjustSettings() :
		  Ratio(1.0f)
		, Min_StaticMeshes(32)
		, Max_StaticMeshes(256)
		, Min_BSPSurfaces(1)
		, Max_BSPSurfaces(512)
		, Min_Terrains(1)
		, Max_Terrains(32)
		, Min_FluidSurfaces(8)
		, Max_FluidSurfaces(32)
		, bStaticMeshes(FALSE)
		, bBSPSurfaces(FALSE)
		, bTerrains(FALSE)
		, bFluidSurfaces(FALSE)
		, LevelOptions(ELightmapResRatioAdjustLevels::Current)
		, bSelectedObjectsOnly(FALSE)
		, WindowPositionX(-1)
		, WindowPositionY(-1)
	{
	}

	/** Ratio to apply */
	FLOAT Ratio;

	/** Min/Max values */
	/** Static meshes */
	INT Min_StaticMeshes;
	INT Max_StaticMeshes;
	/** BSP Surfaces */
	INT Min_BSPSurfaces;
	INT Max_BSPSurfaces;
	/** Terrains */
	INT Min_Terrains;
	INT Max_Terrains;
	/** Fluid Surfaces */
	INT Min_FluidSurfaces;
	INT Max_FluidSurfaces;

	/** If TRUE, apply to static meshes */
	UBOOL bStaticMeshes;
	/** If TRUE, apply to BSP surfaces*/
	UBOOL bBSPSurfaces;
	/** If TRUE, apply to terrains */
	UBOOL bTerrains;
	/** If TRUE, apply to fluid surfaces */
	UBOOL bFluidSurfaces;

	/** The primitives to apply the adjustment to */
	BYTE PrimitiveFlags;

	/** The level(s) to check */
	ELightmapResRatioAdjustLevels::Options LevelOptions;

	/** If TRUE, then only do selected primitives in the level(s) */
	UBOOL bSelectedObjectsOnly;

	/**
	 *	Window settings
	 */
	/** Horizontal window position */
	INT WindowPositionX;

	/** Vertical window position */
	INT WindowPositionY;
};

#endif	// __LightmapResRatioAdjust_h__
