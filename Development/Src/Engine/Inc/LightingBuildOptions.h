/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LIGHTINGBUILDOPTIONS_H__
#define __LIGHTINGBUILDOPTIONS_H__

/**
 * A set of parameters specifying how static lighting is rebuilt.
 */
class FLightingBuildOptions
{
public:
	FLightingBuildOptions()
	:	bUseLightmass(TRUE)
	,	bUseErrorColoring(FALSE)
	,	bDumpBinaryResults(FALSE)
	,	bOnlyBuildSelected(FALSE)
	,	bOnlyBuildCurrentLevel(FALSE)
	,	bBuildBSP(TRUE)
	,	bBuildActors(TRUE)
	,	bOnlyBuildSelectedLevels(FALSE)
	,	bOnlyBuildVisibility(FALSE)
	,	bOnlyBuildVisibleLevels(FALSE)
	,	bGenerateBuildingLODTex(TRUE)
	,	bShowLightingBuildInfo(FALSE)
	,	QualityLevel(Quality_Preview)
	,	NumUnusedLocalCores(1)
	{}

	/**
	 * @return TRUE if the lighting should be built for the level, given the current set of lighting build options.
	 */
	UBOOL ShouldBuildLightingForLevel(ULevel* Level) const;

	/** Whether to use Lightmass or not												*/
	UBOOL					bUseLightmass;
	/** Whether to color problem objects (wrapping uvs, etc.)						*/
	UBOOL					bUseErrorColoring;
	/** Whether to dump binary results or not										*/
	UBOOL					bDumpBinaryResults;
	/** Whether to only build lighting for selected actors/brushes/surfaces			*/
	UBOOL					bOnlyBuildSelected;
	/** Whether to only build lighting for current level							*/
	UBOOL					bOnlyBuildCurrentLevel;
	/** Whether to build lighting for BSP											*/
	UBOOL					bBuildBSP;
	/** Whether to build lighting for Actors (e.g. static meshes)					*/
	UBOOL					bBuildActors;
	/** Whether to only build lighting for levels selected in the Level Browser.	*/
	UBOOL					bOnlyBuildSelectedLevels;
	/** Whether to only build visibility, and leave lighting untouched.				*/
	UBOOL					bOnlyBuildVisibility;
	/** Whether to only build visible levels, and leave hidden level lighting untouched. */
	UBOOL					bOnlyBuildVisibleLevels;
	/** Whether to generate LOD textures for ProcBuildings.							*/
	UBOOL					bGenerateBuildingLODTex;
	/** Whether to display the lighting build info following a build.				*/
	UBOOL					bShowLightingBuildInfo;
	/** The quality level to use for the lighting build. (0-3)						*/
	ELightingBuildQuality	QualityLevel;
	/** The quality level to use for half-resolution lightmaps (not exposed)		*/
	static ELightingBuildQuality	HalfResolutionLightmapQualityLevel;
	/** The number of cores to leave 'unused'										*/
	INT						NumUnusedLocalCores;
	/** The set of levels selected in the Level Browser.							*/
	TArray<ULevel*>			SelectedLevels;
};

#endif // __LIGHTINGBUILDOPTIONS_H__
