/*=============================================================================
	SystemSettings.cpp: Unreal engine HW compat scalability system.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SYSTEMSETTINGS_H__
#define __SYSTEMSETTINGS_H__

/*-----------------------------------------------------------------------------
	System settings and scalability options.
-----------------------------------------------------------------------------*/

/** 
 * The type of the system setting
 */
enum ESystemSettingType
{
	SST_UNKNOWN = 0,
	SST_STRING,
	SST_INT,
	SST_ENUM,
	SST_FLOAT,
	SST_BOOL,
	SST_ANY
};

/**
 * The intent of the system setting
 */
enum ESystemSettingIntent
{
	// No idea what this setting is used for
	SSI_UNKNOWN = 0,
	// Only use this setting for debugging; do not use in a retail environment
	SSI_DEBUG,
	// Compatibility setting that needs to be set for certain hardware
	SSI_COMPATIBILITY,
	// Use the setting to adjust the performance/quality ratio
	SSI_SCALABILITY,
	// Use the setting to adjust the performance/quality ratio for mobile devices
	SSI_MOBILE_SCALABILITY,
	// Set according to the user's desire
	SSI_PREFERENCE,
};

/**
 * How to update to the next setting depending on type
 */
class FVSS
{
public:
	virtual UBOOL GetNextSetting( UBOOL CurrentSetting )
	{
		return FALSE;
	}

	virtual INT GetNextSetting( INT CurrentSetting )
	{
		return 0;
	}

	virtual FLOAT GetNextSetting( FLOAT CurrentSetting )
	{
		return 0.0f;
	}

	virtual INT GetMinIntSetting( void )
	{
		return 0;
	}

	virtual INT GetMaxIntSetting( void )
	{
		return 0;
	}

	virtual FLOAT GetMinFloatSetting( void )
	{
		return 0.0f;
	}

	virtual FLOAT GetMaxFloatSetting( void )
	{
		return 0.0f;
	}

	virtual INT GetSettingCount( void )
	{
		return 1;
	}
};

class FVSSBool : public FVSS
{
	virtual UBOOL GetNextSetting( UBOOL CurrentSetting )
	{
		return !CurrentSetting;
	}
};

class FVSSGenericInt : public FVSS
{
public:
	FVSSGenericInt( INT InMin, INT InMax, INT InStep )
	{
		Min = InMin;
		Max = InMax;
		Step = InStep;
		if( Step == 0 )
		{
			Step = 1;
		}
	}

	virtual INT GetNextSetting( INT CurrentSetting )
	{
		if( CurrentSetting + Step > Max )
		{
			return Min;
		}

		return CurrentSetting + Step;
	}

	virtual INT GetMinIntSetting( void )
 {
		return Min;
	}

	virtual INT GetMaxIntSetting( void )
	{
		return Max;
	}

	virtual INT GetSettingCount( void )
	{
		return ( Max - Min ) / Step;
	}

	INT Min;
	INT Max;
	INT Step;
};

class FVSSGenericFloat : public FVSS
{
public:
	FVSSGenericFloat( FLOAT InMin, FLOAT InMax )
	{
		Min = InMin;
		Max = InMax;
		Step = ( Max - Min ) / 100.0f;
	}

	virtual FLOAT GetNextSetting( FLOAT CurrentSetting )
	{
		if( CurrentSetting + Step > Max )
		{
			return Min;
		}

		return CurrentSetting + Step;
	}

	virtual FLOAT GetMinFloatSetting( void )
	{
		return Min;
	}

	virtual FLOAT GetMaxFloatSetting( void )
	{
		return Max;
	}

	virtual INT GetSettingCount( void )
	{
		return ( Max - Min ) / Step;
	}

	FLOAT Min;
	FLOAT Max;
	FLOAT Step;
};

/**
 * The container for a single system setting
 */
class FSystemSetting
{
public:
	ESystemSettingType SettingType;
	ESystemSettingIntent SettingIntent;
	const TCHAR* SettingName;
	void* SettingAddress;
	FVSS* SettingUpdate;
	const TCHAR* SettingHelp;
	UBOOL bFound;
 };


/** Augments TextureLODSettings with access to TextureLODGroups. */
struct FExposedTextureLODSettings : public FTextureLODSettings
{
public:
	/** @return		A handle to the indexed LOD group. */
	FTextureLODGroup& GetTextureLODGroup(INT GroupIndex)
	{
		check( GroupIndex >= 0 && GroupIndex < TEXTUREGROUP_MAX );
		return TextureLODGroups[GroupIndex];
	}
	/** @return		A handle to the indexed LOD group. */
	const FTextureLODGroup& GetTextureLODGroup(INT GroupIndex) const
	{
		check( GroupIndex >= 0 && GroupIndex < TEXTUREGROUP_MAX );
		return TextureLODGroups[GroupIndex];
	}
};

/**
 * Struct that holds the actual data for the system settings.
 */
class FSystemSettings : public FExec
{
public:
	/** Current detail mode; determines whether components of actors should be updated/ ticked.	*/
	INT		DetailMode;
	/** Scale applied to primitive's MaxDrawDistance. */
	FLOAT	MaxDrawDistanceScale;
	/** Whether to allow rendering of SpeedTree leaves.					*/
	UBOOL	bAllowSpeedTreeLeaves;
	/** Whether to allow rendering of SpeedTree fronds.					*/
	UBOOL	bAllowSpeedTreeFronds;
	/** Whether to allow static decals.									*/
	UBOOL	bAllowStaticDecals;
	/** Whether to allow dynamic decals.								*/
	UBOOL	bAllowDynamicDecals;
	/** Whether to allow decals that have not been placed in static draw lists and have dynamic view relevance */
	UBOOL	bAllowUnbatchedDecals;
	/** Scale factor for distance culling decals						*/
	FLOAT	DecalCullDistanceScale;
	/** Whether to allow dynamic lights.								*/
	UBOOL	bAllowDynamicLights;
	/** Whether to composte dynamic lights into light environments.		*/
	UBOOL	bUseCompositeDynamicLights;
	/** Whether to allow light environments to use SH lights for secondary lighting	*/
	UBOOL	bAllowSHSecondaryLighting;
	/**  Whether to allow directional lightmaps, which use the material's normal and specular. */
	UBOOL	bAllowDirectionalLightMaps;
	/** Whether to allow motion blur.									*/
	UBOOL	bAllowMotionBlur;
	/** Whether to allow motion blur to be paused.						*/
	UBOOL	bAllowMotionBlurPause;
	/** Whether to allow depth of field.								*/
	UBOOL	bAllowDepthOfField;
	/** Whether to allow ambient occlusion.								*/
	UBOOL	bAllowAmbientOcclusion;
	/** Whether to allow bloom.											*/
	UBOOL	bAllowBloom;
	/** Whether to allow light shafts.									*/
	UBOOL   bAllowLightShafts;
	/** Whether to allow distortion.									*/
	UBOOL	bAllowDistortion;
	/** Whether to allow distortion to use bilinear filtering when sampling the scene color during its apply pass	*/
	UBOOL	bAllowFilteredDistortion;
	/** Whether to allow dropping distortion on particles based on WorldInfo::bDropDetail. */
	UBOOL	bAllowParticleDistortionDropping;
	/** Whether to allow downsampled transluency.						*/
	UBOOL	bAllowDownsampledTranslucency;
	/** Whether to allow rendering of LensFlares.						*/
	UBOOL	bAllowLensFlares;
	/** Whether to allow fog volumes.									*/
	UBOOL	bAllowFogVolumes;
	/** Whether to allow floating point render targets to be used.		*/
	UBOOL	bAllowFloatingPointRenderTargets;
	/** Whether to allow the rendering thread to lag one frame behind the game thread.	*/
	UBOOL	bAllowOneFrameThreadLag;
	/** LOD bias for skeletal meshes.									*/
	INT		SkeletalMeshLODBias;
	/** LOD bias for particle systems.									*/
	INT		ParticleLODBias;
	/** Whether to use D3D11 when it's available.						*/
	UBOOL	bAllowD3D11;
	/** Whether to use OpenGL when it's available.						*/
	UBOOL	bAllowOpenGL;
	/** Whether to allow radial blur effects to render.					*/
	UBOOL	bAllowRadialBlur;
	/** Whether to allow sub-surface scattering to render.				*/
	UBOOL	bAllowSubsurfaceScattering;
	/** Whether to allow image reflections to render.					*/
	UBOOL	bAllowImageReflections;
	/** Whether to allow image reflections to be shadowed.				*/
	UBOOL	bAllowImageReflectionShadowing;
	/** State of the console variable MotionBlurSkinning.				*/
	INT		MotionBlurSkinning;
	/** Global tessellation factor multiplier */
	FLOAT	TessellationAdaptivePixelsPerTriangle;
	/** Whether to use high-precision GBuffers. */
	UBOOL	bHighPrecisionGBuffers;
	/** Whether to keep separate translucency (for better Depth of Field), experimental */
	UBOOL	bAllowSeparateTranslucency;
	/** Whether to allow post process MLAA to render. requires extra memory	*/
	UBOOL	bAllowPostprocessMLAA;
	/** Whether to use high quality materials when low quality exist	*/
	UBOOL	bAllowHighQualityMaterials;
	/** Max filter sample count (clamp can cause boxy appearance but allows for better performance, only numbers below 16 have effect)	*/
	INT		MaxFilterBlurSampleCount;
	/** Whether to allow fractured meshes to take damage.				*/
	UBOOL	bAllowFracturedDamage;
	/** Scales the game-specific number of fractured physics objects allowed.	*/
	FLOAT	NumFracturedPartsScale;
	/** Percent chance of a rigid body spawning after a fractured static mesh is damaged directly.  [0-1] */
	FLOAT	FractureDirectSpawnChanceScale;
	/** Percent chance of a rigid body spawning after a fractured static mesh is damaged by radial blast.  [0-1] */
	FLOAT	FractureRadialSpawnChanceScale;
	/** Distance scale for whether a fractured static mesh should actually fracture when damaged */
	FLOAT	FractureCullDistanceScale;

	/** Whether to allow dynamic shadows.								*/
	UBOOL	bAllowDynamicShadows;
	/** Whether to allow dynamic light environments to cast shadows.	*/
	UBOOL	bAllowLightEnvironmentShadows;
	/** Quality bias for projected shadow buffer filtering.	 Higher values use better quality filtering.		*/
	INT		ShadowFilterQualityBias;
	/** min dimensions (in texels) allowed for rendering shadow subject depths */
	INT		MinShadowResolution;
	/** min dimensions (in texels) allowed for rendering preshadow depths */
	INT		MinPreShadowResolution;
	/** max square dimensions (in texels) allowed for rendering shadow subject depths */
	INT		MaxShadowResolution;
	/** max square dimensions (in texels) allowed for rendering whole scene shadow depths */
	INT		MaxWholeSceneDominantShadowResolution;
	/** The ratio of subject pixels to shadow texels.					*/
	FLOAT	ShadowTexelsPerPixel;
	FLOAT	PreShadowResolutionFactor;
	/** Toggle Branching PCF implementation for projected shadows */
	UBOOL	bEnableBranchingPCFShadows;
	/** Whether to allow hardware filtering optimizations like hardware PCF and Fetch4. */
	UBOOL	bAllowHardwareShadowFiltering;
	/** hack to allow for foreground DPG objects to cast shadows on the world DPG */
	UBOOL	bEnableForegroundShadowsOnWorld;
	/** Whether to allow foreground DPG self-shadowing */
	UBOOL	bEnableForegroundSelfShadowing;
	/** Whether to allow whole scene dominant shadows. */
	UBOOL	bAllowWholeSceneDominantShadows;
	/** Whether to use safe and conservative shadow frustum creation that wastes some shadowmap space. */
	UBOOL	bUseConservativeShadowBounds;
	/** Radius, in shadowmap texels, of the filter disk */
	FLOAT	ShadowFilterRadius;
	/** Depth bias that is applied in the depth pass for all types of projected shadows except VSM */
	FLOAT	ShadowDepthBias;
	/** Higher values make the per object soft shadow comparison sharper, lower values make the transition softer. */
	FLOAT	PerObjectShadowTransition;
	/** Higher values make the per scene soft shadow comparison sharper, lower values make the transition softer. */
	FLOAT	PerSceneShadowTransition;
	/** Scale applied to the penumbra size of Cascaded Shadow Map splits, useful for minimizing the transition between splits. */
	FLOAT	CSMSplitPenumbraScale;
	/** Scale applied to the soft comparison transition distance of Cascaded Shadow Map splits, useful for minimizing the transition between splits. */
	FLOAT	CSMSplitSoftTransitionDistanceScale;
	/** Scale applied to the depth bias of Cascaded Shadow Map splits, useful for minimizing the transition between splits. */
	FLOAT	CSMSplitDepthBiasScale;
	/** Minimum camera FOV for CSM, this is used to prevent shadow shimmering when animating the FOV lower than the min, for example when zooming */
	FLOAT	CSMMinimumFOV;
	/** The FOV will be rounded by this factor for the purposes of CSM, which turns shadow shimmering into discrete jumps */
	FLOAT	CSMFOVRoundFactor;
	/** WholeSceneDynamicShadowRadius to use when using CSM to preview unbuilt lighting from a directional light. */
	FLOAT	UnbuiltWholeSceneDynamicShadowRadius;
	/** NumWholeSceneDynamicShadowCascades to use when using CSM to preview unbuilt lighting from a directional light. */
	INT		UnbuiltNumWholeSceneDynamicShadowCascades;
	/** How many unbuilt light-primitive interactions there can be for a light before the light switches to whole scene shadows. */
	INT		WholeSceneShadowUnbuiltInteractionThreshold;
	/** Resolution in texel below which shadows are faded out. */
	INT		ShadowFadeResolution;
	/** Resolution in texel below which preshadows are faded out. */
	INT		PreShadowFadeResolution;
	/** Controls the rate at which shadows are faded out. */
	FLOAT	ShadowFadeExponent;

	/** Global texture LOD settings.									*/
	FExposedTextureLODSettings TextureLODSettings;

	/** If enabled, texture will only be streamed in, not out.			*/
	UBOOL	bOnlyStreamInTextures;
	/** Maximum level of anisotropy used.								*/
	INT		MaxAnisotropy;
	/** Scene capture streaming texture update distance scalar.			*/
	FLOAT	SceneCaptureStreamingMultiplier;

	/** Whether to use VSync or not.									*/
	UBOOL	bUseVSync;

	/** Percentage of screen main view should take up.					*/
	FLOAT	ScreenPercentage;
	/** Whether to upscale the screen to take up the full front buffer.	*/
	UBOOL	bUpscaleScreenPercentage;

	/** Screen X resolution */
	INT ResX;
	/** Screen Y resolution */
	INT ResY;
	/** Fullscreen */
	UBOOL bFullscreen;

	/** The maximum number of MSAA samples to use.						*/
	INT		MaxMultiSamples;
	UBOOL	bAllowD3D9MSAA;
	UBOOL	bAllowTemporalAA;
	FLOAT	TemporalAA_MinDepth;
	FLOAT	TemporalAA_StartDepthVelocityScale;

	/** Whether to force CPU access to GPU skinned vertex data. */
	UBOOL bForceCPUAccessToGPUSkinVerts;
	/** Whether to disable instanced skeletal weights. */
	UBOOL bDisableSkeletalInstanceWeights;

	/** Whether to allow independent, external displays */
	UBOOL bAllowSecondaryDisplays;
	/** The maximum width and height of any potentially allowed secondary displays (requires bAllowSecondaryDisplays == TRUE) */
	INT SecondaryDisplayMaximumWidth;
	INT SecondaryDisplayMaximumHeight;

	/** Enables sleeping once a frame to smooth out CPU usage */
	UBOOL bAllowPerFrameSleep;
	/** Enables yielding once a frame to give other processes time to run. Note that bAllowPerFrameSleep takes precedence */
	UBOOL bAllowPerFrameYield;

#if WITH_MOBILE_RHI
	/** The baseline feature level of the device */
	INT MobileFeatureLevel;
	/** Whether to allow fog on mobile*/
	UBOOL bAllowMobileFog;
	/** Whether to use height-fog on mobile, or simple gradient fog. */
	UBOOL bAllowMobileHeightFog;
	/** Whether to allow vertex specular on mobile */
	UBOOL bAllowMobileSpecular;
	/** Whether to allow bump offset on mobile */
	UBOOL bAllowMobileBumpOffset;
	/** Whether to allow normal mapping on mobile */
	UBOOL bAllowMobileNormalMapping;
	/** Whether to allow environment mapping on mobile */
	UBOOL bAllowMobileEnvMapping;
	/** Whether to allow rim lighting on mobile */
	UBOOL bAllowMobileRimLighting;
	/** Whether to allow color blending on mobile */
	UBOOL bAllowMobileColorBlending;
	/** Whether to allow color grading on mobile */
	UBOOL bAllowMobileColorGrading;
	/** Whether to allow vertex movement on mobile */
	UBOOL bAllowMobileVertexMovement;
	/** Whether to allow occlusion queries on mobile */
	UBOOL bAllowMobileOcclusionQueries;
	/** Global setting for gamma correction state*/
	UBOOL bMobileGlobalGammaCorrection;
	/** Whether to allow a level to override the gamma correction*/
	UBOOL bMobileAllowGammaCorrectionLevelOverride;
	/** Whether to enable a rendering depth pre-pass on mobile. */
	UBOOL bMobileAllowDepthPrePass;
#if WITH_GFx
	/** Whether to include gamma adjustment code in the scale form shaders*/
	UBOOL bMobileGfxGammaCorrection;
#endif
	/** Whether to use preprocessed shaders on mobile */
	UBOOL bUsePreprocessedShaders;
	/** Whether to flash the screen red (non-final release only) when a cached shader is not found at runtime */
	UBOOL bFlashRedForUncachedShaders;
	/** Whether to issue a "warm-up" draw call for mobile shaders as they are compiled */
	UBOOL bWarmUpPreprocessedShaders;
	/** Whether to dump out preprocessed shaders for mobile as they are encountered/compiled */
	UBOOL bCachePreprocessedShaders;
	/** Whether to run dumped out preprocessed shaders through the shader profiler */
	UBOOL bProfilePreprocessedShaders;
	/** Whether to run the C preprocessor on shaders  */
	UBOOL bUseCPreprocessorOnShaders;
	/** Whether to load the C preprocessed source  */
	UBOOL bLoadCPreprocessedShaders;
	/** Whether to share pixel shaders across multiple unreal shaders  */
	UBOOL bSharePixelShaders;
	/** Whether to share vertex shaders across multiple unreal shaders  */
	UBOOL bShareVertexShaders;
	/** Whether to share shaders program across multiple unreal shaders  */
	UBOOL bShareShaderPrograms;
	/** Whether to enable MSAA, if the OS supports it */
	UBOOL bEnableMSAA;
	/** The default global content scale factor to use on device (largely iOS specific) */
	FLOAT MobileContentScaleFactor;
	/** How much to bias all texture mip levels on mobile (usually 0 or negative) */
	FLOAT MobileLODBias;
	/** Scales the blur vector when using mobile RHI. */
	FLOAT MobileLightShaftRadialBlurPercentScale;
	/** Scale factor for the blur vector length, for the first pass */
	FLOAT MobileLightShaftRadialBlurFirstPassRatio;
	/** Scale factor for the blur vector length, for the second pass */
	FLOAT MobileLightShaftRadialBlurSecondPassRatio;
	/** The maximum number of bones supported for skinning */
	INT MobileBoneCount;
	/** The maximum number of bones influences per vertex supported for skinning */
	INT MobileBoneWeightCount;
	/** The size of the scratch buffer for vertices (in kB) */
	INT MobileVertexScratchBufferSize;
	/** The size of the scratch buffer for indices (in kB) */
	INT MobileIndexScratchBufferSize;
	/** TRUE if we try to support mobile modulated shadow */
	UBOOL bMobileModShadows;
	/** TRUE to enable the mobile tilt shift effect */
	UBOOL bMobileTiltShift;
	/** Position of the focused center of the tilt shift effect (in percent of the screen height) */
	FLOAT MobileTiltShiftPosition;
	/** Width of focused area in the tilt shift effect (in percent of the screen height) */
	FLOAT MobileTiltShiftFocusWidth;
	/** Width of transition area in the tilt shift effect, where it transitions from full focus to full blur (in percent of the screen height) */
	FLOAT MobileTiltShiftTransitionWidth;

	/** How far a dynamic shadow can extend outside the shadow caster's bounding box. */
	FLOAT MobileMaxShadowRange;

	/** Whether to clear the depth buffer between DPGs */
	UBOOL bMobileClearDepthBetweenDPG;

	/** The resolution of the shadow texture */
	INT	MobileShadowTextureResolution;

	/** Our perceived maximum memory available on the device */
	INT MobileMaxMemory;

	/** Without the resolve we might get the old depth buffer values but artifacts are minimal in many cases and it saves quite some performance. For correct results keep this set to TRUE. */
	UBOOL bMobileSceneDepthResolveForShadows;

	/** If we have enabled higher resolution timing for this device */
	UBOOL bMobileUsingHighResolutionTiming;

	/** LOD bias for mobile landscape rendering on this device (in addition to any per-landscape bias set) */
	INT MobileLandscapeLodBias;

	/** Whether to automatically put cooked startup objects in the StartupPackages shader group */
	UBOOL bMobileUseShaderGroupForStartupObjects;

	/** Whether to disable generating both fog shader permutations on mobile.  When TRUE, it decreases load times but increases GPU cost for materials/levels with fog enabled */
	UBOOL bMobileMinimizeFogShaders;

	/** Mobile FXAA quality level.  0 is off. */
	INT  MobileFXAAQuality;

#endif	//WITH_MOBILE_RHI

#if WITH_APEX
	/** Resource budget for APEX LOD. Higher values indicate the system can handle more APEX load.*/
	FLOAT ApexLODResourceBudget;
	/** The maximum number of active PhysX actors which represent dynamic groups of chunks (islands).  
		If a fracturing event would cause more islands to be created, then oldest islands are released 
		and the chunks they represent destroyed.*/
	INT ApexDestructionMaxChunkIslandCount;
	/** The maximum number of PhysX shapes which represent destructible chunks.  
		If a fracturing event would cause more shapes to be created, then oldest islands are released 
		and the chunks they represent destroyed.*/
	INT ApexDestructionMaxShapeCount;
	/** Every destructible asset defines a min and max lifetime, and maximum separation distance for its chunks.
		Chunk islands are destroyed after this time or separation from their origins. This parameter sets the
		lifetimes and max separations within their min-max ranges. The valid range is [0,1]. */
	FLOAT ApexDestructionMaxChunkSeparationLOD;
	/** If TRUE, allow APEX clothing fetch (skinning etc) to be done on multiple threads */
	UBOOL bEnableParallelApexClothingFetch;
	/** If TRUE, allow APEX skinning to occur without blocking fetch results.  bEnableParallelApexClothingFetch must be enabled
		for this to work. */
	UBOOL bApexClothingAsyncFetchResults;
	/** If set to true, destructible chunks with the lowest benefit would get removed first instead of the oldest */
	UBOOL bApexDestructionSortByBenefit;
	/** Lets the user throttle the number of SDK actor creates per frame (per scene) due to destruction, as this can be quite costly.
		The default is 0xffffffff (unlimited). */
	INT ApexDestructionMaxActorCreatesPerFrame;
	/** Lets the user throttle the number of fractures processed per frame (per scene) due to destruction, as this can be quite costly.
		The default is 0xffffffff (unlimited). */
	INT ApexDestructionMaxFracturesProcessedPerFrame;
	/** Average Simulation Frequency is estimated with the last n frames. This is used in Clothing when
		bAllowAdaptiveTargetFrequency is enabled.*/
	INT ApexClothingAvgSimFrequencyWindow;
	/**	Whether or not to use GPU Rigid Bodies	*/
	UBOOL bEnableApexGRB;
	/**	The size of the cells to divide the world into for GPU collision detection	*/
	FLOAT ApexGRBMeshCellSize;
	/**	Collision skin width, as in PhysX. */
	FLOAT ApexGRBSkinWidth;
	/** Number of non-penetration solver iterations. */
	INT ApexGRBNonPenSolverPosIterCount;
	/** Number of friction solver position iterations. */
	INT ApexGRBFrictionSolverPosIterCount;
	/** Number of friction solver velocity iterations. */
	INT ApexGRBFrictionSolverVelIterCount;
	/** Maximum linear acceleration. */
	FLOAT ApexGRBMaxLinAcceleration;
	/** Amount (in MB) of GPU memory to allocate for GRB scene data (shapes, actors etc). */
	INT ApexGRBGpuMemSceneSize;
	/** Amount (in MB) of GPU memory to allocate for GRB temporary data (broadphase pairs, contacts etc). */
	INT ApexGRBGpuMemTempDataSize;
	/** ClothingActors will cook in a background thread to speed up creation time */
	UBOOL bApexClothingAllowAsyncCooking;
	/** Allow APEX SDK to interpolate clothing matrices between the substeps. */
	UBOOL bApexClothingAllowApexWorkBetweenSubsteps;
#endif

	/** Constructor, initializing all member variables. */
	FSystemSettings();

/** 
	 * Exec handler implementation.
	 *
	 * @param Cmd	Command to parse
	 * @param Ar	Output device to log to
	 *
	 * @return TRUE if command was handled, FALSE otherwise
	 */
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

/** 
	 * Initializes system settings and included texture LOD settings.
 *
	 * @param bSetupForEditor	Whether to initialize settings for Editor
 */
	void Initialize( UBOOL bSetupForEditor );

	/** loads settings from the given section in the given ini */
	void LoadFromIni( const FString IniSection, const TCHAR* IniFilename = GEngineIni, UBOOL bAllowMissingValues = TRUE );

	/** Loads settings from the ini. (purposely override the inherited name so people can't accidentally call it.) */
	UBOOL LoadFromIni( const TCHAR* Override );

	/** saves settings to the given section in the engine ini */
	void SaveToIni( const FString IniSection );

	/** Saves current settings to the ini. (purposely override the inherited name so people can't accidentally call it.) */
	void SaveToIni( void );

	/**
	 * Returns a string for the specified texture group LOD settings to the specified ini.
	 *
	 * @param	TextureGroupID		Index/enum of the group
	 * @param	GroupName			String representation of the texture group
	 */
	FString GetLODGroupString( TextureGroup TextureGroupID, const TCHAR* GroupName );

	void WriteTextureLODGroupToIni(TextureGroup TextureGroupID, const TCHAR* GroupName, const TCHAR* IniSection);

/**
	 * Writes all texture group LOD settings to the specified ini.
 *
	 * @param	IniFilename			The .ini file to save to.
	 * @param	IniSection			The .ini section to save to.
 */
	void WriteTextureLODGroupsToIni(const TCHAR* IniSection);

	/**
	 * Reads a single entry and parses it into the group array.
	 *
	 * @param	TextureGroupID		Index/enum of group to parse
	 * @param	MinLODSize			Minimum size, in pixels, below which the code won't bias.
	 * @param	MaxLODSize			Maximum size, in pixels, above which the code won't bias.
	 * @param	LODBias				Group LOD bias.
	 * @param	MipGenSettings		Defines how the the mip-map generation works, e.g. sharpening
	 */
	void SetTextureLODGroup( TextureGroup TextureGroupID, int MinLODSize, INT MaxLODSize, INT LODBias, TextureMipGenSettings MipGenSettings );

	/** Dumps the settings to the log file */
	void Dump( FOutputDevice& Ar, ESystemSettingIntent SettingIntent );
	void DumpTextures( FOutputDevice& Ar );
	void DumpTextureLODGroup( FOutputDevice& Ar, TextureGroup TextureGroupID, const TCHAR* GroupName );

	/** Finds the setting by name */
	FSystemSetting* FindSystemSetting( FString& SettingName, ESystemSettingType SettingType );

	/** Indicates whether upscaling is needed */
	UBOOL NeedsUpscale( void ) const;

	/** Indicates whether the hardware anti-aliasing is used */
	UBOOL UsesMSAA( void ) const
	{
		return MaxMultiSamples > 1 && ( GRHIShaderPlatform == SP_PCD3D_SM5 || ( GRHIShaderPlatform == SP_PCD3D_SM3 && bAllowD3D9MSAA ) );
	}

	/** 
	 * Sets the resolution and writes the values to Ini if changed but does not apply the changes (eg resize the viewport).
	 */
	void SetResolution(INT InSizeX, INT InSizeY, UBOOL InFullscreen);

	/**
	 * Scale X,Y offset/size of screen coordinates if the screen percentage is not at 100%
	 *
	 * @param X - in/out X screen offset
	 * @param Y - in/out Y screen offset
	 * @param SizeX - in/out X screen size
	 * @param SizeY - in/out Y screen size
	 */
	void ScaleScreenCoords( INT& X, INT& Y, UINT& SizeX, UINT& SizeY );

	/**
	 * Reverses the scale and offset done by ScaleScreenCoords() 
	 * if the screen percentage is not 100% and upscaling is allowed.
	 *
	 * @param OriginalX - out X screen offset
	 * @param OriginalY - out Y screen offset
	 * @param OriginalSizeX - out X screen size
	 * @param OriginalSizeY - out Y screen size
	 * @param InX - in X screen offset
	 * @param InY - in Y screen offset
	 * @param InSizeX - in X screen size
	 * @param InSizeY - in Y screen size
	 */
	void UnScaleScreenCoords( 
		INT &OriginalX, INT &OriginalY, 
		UINT &OriginalSizeX, UINT &OriginalSizeY, 
		FLOAT InX, FLOAT InY, 
		FLOAT InSizeX, FLOAT InSizeY);

	/** Applies setting overrides based on command line options. */
	void ApplyOverrides();

	/**
	 * Apply any settings that have changed
	 */
	void ApplySettings( FSystemSettings& OldSystemSettings );

	/**
	 * Sets new system settings (optionally writes out to the ini).
	 */ 
	void ApplyNewSettings( const FSystemSettings& NewSettings, UBOOL bWriteToIni );

	/**
	 * Ensures that the correct settings are being used based on split screen type.
	 */
	void UpdateSplitScreenSettings( void );

	/**
	 * Recreates texture resources and drops mips.
	 *
	 * @return		TRUE if the settings were applied, FALSE if they couldn't be applied immediately.
	 */
	UBOOL UpdateTextureStreaming( void );

	/**
	 * Makes System Settings take effect on the rendering thread
	 */
	static void SceneRenderTargetsUpdateRHI(const FSystemSettings& OldSettings, const FSystemSettings& NewSettings);

	/**
	 * Cause a call to UpdateRHI on GSceneRenderTargets
	 */
	static void UpdateSceneRenderTargetsRHI();
		
	/** Set to TRUE after this has been populated from the ini files */
	UBOOL bInit;

	/** Since System Settings is called into before GIsEditor is set, we must cache this value. */
	UBOOL bIsEditor;

	/** Name of ini section used to set data */
	FString SystemSettingName;

	/** The number of system settings that can be modified */
	INT NumberOfSystemSettings;

	/** The master list of all configurable system settings */
	static FSystemSetting SystemSettings[];
};

/**
 * Global system settings accessor
 */
extern FSystemSettings GSystemSettings;

#endif // __SYSTEMSETTINGS_H__
