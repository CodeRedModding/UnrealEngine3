/**
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *	This will hold all of our enums and types and such that we need to
 *	use in multiple files where the enum can'y be mapped to a specific file.
 */
class EngineTypes extends Object
	native
	abstract
	config(Engine);

enum EPathFindingError
{
	PATHERROR_STARTPOLYNOTFOUND, // could not determine starting poly
	PATHERROR_GOALPOLYNOTFOUND, // could not determine poly containing our goal
	PATHERROR_ANCHORPYLONNOTFOUND, // could not determine which pylon we are in
	PATHERROR_NOPATHFOUND, // found start and end polys, but could not construct a path between that suits our parameters
	PATHERROR_COMPUTEVALIDFINALDEST_FAIL, // ComputeValidFinalDestination was unable to build a good offset from goal to move to
	PATHERROR_GETNEXTMOVELOCATION_FAIL, // GetNextMoveLocation was unable to determine the next move point 
	PATHERROR_MOVETIMEOUT, // we timed out while moving (e.g. trying to move against a wall and never succeeded)
};

/**
 * A line of subtitle text and the time at which it should be displayed.
 */
struct native SubtitleCue
{
	/** The text too appear in the subtitle. */
	var() localized string	Text;

	/** The time at which the subtitle is to be displayed, in seconds relative to the beginning of the line. */
	var() localized float	Time;
};

/**
 *	A subtitle localized to a specific language.
 */
struct native LocalizedSubtitle
{
	/** The 3-letter language for this subtitle */
	var string LanguageExt;

	/**
	 * Subtitle cues.  If empty, use SoundNodeWave's SpokenText as the subtitle.  Will often be empty,
	 * as the contents of the subtitle is commonly identical to what is spoken.
	 */
	var array<SubtitleCue> Subtitles;

	/** TRUE if this sound is considered to contain mature content. */
	var bool bMature;

	/** TRUE if the subtitles have been split manually. */
	var bool bManualWordWrap;

	/** TRUE if the subtitles should be displayed one line at a time. */
	var bool bSingleLine;
};

/** Note: This is mirrored in Lightmass, be sure to update the blend mode structure and logic there if this changes. */
enum EBlendMode
{
	BLEND_Opaque,
	BLEND_Masked,
	BLEND_Translucent,
	BLEND_Additive,
	BLEND_Modulate,
	BLEND_ModulateAndAdd,
	BLEND_SoftMasked,
	BLEND_AlphaComposite,
	BLEND_DitheredTranslucent
};

enum EMaterialLightingModel
{
	MLM_Phong,
	MLM_NonDirectional,
	MLM_Unlit,
	MLM_SHPRT,
	MLM_Custom,
	MLM_Anisotropic
};

// This is used by the drawing passes to determine tessellation policy, so changes here need to be supported in native code.
enum EMaterialTessellationMode
{
	/** Tessellation disabled */
	MTM_NoTessellation,

	/** Simple tessellation */
	MTM_FlatTessellation,

	/** Simple Spline based tessellation */
	MTM_PNTriangles
};


/** Possible sources for mask values and such */
enum EMobileValueSource
{
	MVS_Constant,
	MVS_VertexColorRed,
	MVS_VertexColorGreen,
	MVS_VertexColorBlue,
	MVS_VertexColorAlpha,
	MVS_BaseTextureRed,
	MVS_BaseTextureGreen,
	MVS_BaseTextureBlue,
	MVS_BaseTextureAlpha,
	MVS_MaskTextureRed,
	MVS_MaskTextureGreen,
	MVS_MaskTextureBlue,
	MVS_MaskTextureAlpha,
	MVS_NormalTextureAlpha,
	MVS_EmissiveTextureRed,
	MVS_EmissiveTextureGreen,
	MVS_EmissiveTextureBlue,
	MVS_EmissiveTextureAlpha
};

/** Possible options for blend factor source for blending between textures */
// @todo: Should fold this into EMobileValueSource
enum EMobileTextureBlendFactorSource
{
	/** From the vertex color's red channel */
	MTBFS_VertexColor,

	/** From the mask texture's alpha */
	MTBFS_MaskTexture,
};

/** Possible vertex texture coordinate sets that may used to sample textures on mobile platforms */
enum EMobileTexCoordsSource
{
	/** First texture coordinate from mesh vertex */
	MTCS_TexCoords0,

	/** Second texture coordinate from mesh vertex */
	MTCS_TexCoords1,

	/** Third texture coordinate from mesh vertex */
	MTCS_TexCoords2,

	/** Forth texture coordinate from mesh vertex */
	MTCS_TexCoords3,
};

/** Possible vertex texture coordinate sets that may used to sample textures on mobile platforms */
enum EMobileAlphaValueSource
{
	/** Default in the diffuse channel alpha */
	MAVS_DiffuseTextureAlpha,

	/** Mask Texture Red Channel (platforms with no alpha texture compression) */
	MAVS_MaskTextureRed,

	/** Mask Texture Green Channel (platforms with no alpha texture compression) */
	MAVS_MaskTextureGreen,

	/** Mask Texture Blue Channel (platforms with no alpha texture compression) */
	MAVS_MaskTextureBlue,
};

/** Possible vertex texture coordinate sets that may used to sample textures on mobile platforms */
enum EMobileColorMultiplySource
{
	MCMS_None,
	MCMS_BaseTextureRed,
	MCMS_BaseTextureGreen,
	MCMS_BaseTextureBlue,
	MCMS_BaseTextureAlpha,
	MCMS_MaskTextureRed,
	MCMS_MaskTextureGreen,
	MCMS_MaskTextureBlue,
	MCMS_MaskTextureAlpha,
};

/** Possible sources for mobile emissive color */
enum EMobileEmissiveColorSource
{
	/** Emissive texture color */
	MECS_EmissiveTexture,

	/** Base texture color */
	MECS_BaseTexture,

	/** Constant color specified in the material properties */
	MECS_Constant,
};

/** Possible environment map blend modes */
enum EMobileEnvironmentBlendMode
{
	/** Add environment map to base color */
	MEBM_Add,

	/** Lerp between base color and environment color */
	MEBM_Lerp
};

// @todo: Should fold this into EMobileValueSource
enum EMobileSpecularMask
{
	MSM_Constant,
	MSM_Luminance,
	MSM_DiffuseRed,
	MSM_DiffuseGreen,
	MSM_DiffuseBlue,
	MSM_DiffuseAlpha,
	MSM_MaskTextureRGB,
	MSM_MaskTextureRed,
	MSM_MaskTextureGreen,
	MSM_MaskTextureBlue,
	MSM_MaskTextureAlpha
};

// @todo: Should fold this into EMobileValueSource
enum EMobileAmbientOcclusionSource
{
	MAOS_Disabled,
	MAOS_VertexColorRed,
	MAOS_VertexColorGreen,
	MAOS_VertexColorBlue,
	MAOS_VertexColorAlpha,
};

struct LightMapRef
{
	var native private const pointer Reference;
};

/**
 *	Lighting build quality enumeration
 */
enum ELightingBuildQuality
{
    Quality_Preview,
    Quality_Medium,
    Quality_High,
    Quality_Production,
    Quality_NoGlobalIllumination // this MUST be placed just before Quality_MAX...
};

struct native DominantShadowInfo
{
	/** Transform from world space to the coordinate space that the DominantLightShadowMap entries are stored in. */
	var Matrix WorldToLight;
	/** Inverse of WorldToLight */
	var Matrix LightToWorld;
	/** Bounding box of the area that the DominantLightShadowMap entries are stored for, in the coordinate space defined by WorldToLight. */
	var box LightSpaceImportanceBounds;
	/** Dimensions of DominantLightShadowMap */
	var int ShadowMapSizeX;
	var int ShadowMapSizeY;
};

/**
 *	Per-light settings for Lightmass
 */
struct native LightmassLightSettings
{
	/** Scale factor for the indirect lighting */
	var(General)	float		IndirectLightingScale <UIMin=0.0 | UIMax=4.0>;
	/** 0 will be completely desaturated, 1 will be unchanged */
	var(General)	float		IndirectLightingSaturation <UIMin=0.0 | UIMax=4.0>;
	/** Controls the falloff of shadow penumbras */
	var(General)	float		ShadowExponent <UIMin=0.1 | UIMax=4.0>;

	structdefaultproperties
	{
		IndirectLightingScale=1.0
		IndirectLightingSaturation=1.0
		ShadowExponent=2.0
	}
};

/**
 *	Point/spot settings for Lightmass
 */
struct native LightmassPointLightSettings extends LightmassLightSettings
{
	/** The radius of the light's emissive surface, not the light's influence. */
	var(Point)		float		LightSourceRadius <UIMin=8.0 | UIMax=1024.0>;

	/**
	 *	IMPORTANT NOTE: This is no longer the default property. It is 32.0.
	 *	However, to avoid breaking existing content, the value is set in
	 *	the APointLight::Spawned function... So if you want to change the
	 *	default again - do it there.
	 *	Yes - it will increase the size on disc to change the default
	 *	property using this method - but it will only be 4 bytes per point
	 *	light serialized to disk (assuming they are using the default
	 *	value). With the move to GI, levels will likely end up with far
	 *	fewer lights. And with compression on shipping content, this
	 *	should not be that big a deal.
	 *	(The alternative is to 'rename' the variable and deprecate the old
	 *	one fixing things up in post load. But we rarely go back and
	 *	remove deprecate members, so this would be a net result of
	 *	increasing each light by 4 bytes IN MEMORY)
	 */
	structdefaultproperties
	{
		LightSourceRadius=100.0
	}
};

/**
 *	Direcitonal light settings for Lightmass
 */
struct native LightmassDirectionalLightSettings extends LightmassLightSettings
{
	/** Angle that the directional light's emissive surface extends relative to a receiver, affects penumbra sizes. */
	var(Directional)	float	LightSourceAngle;

	structdefaultproperties
	{
		LightSourceAngle=3.0;
	}
};

/**
 *	Per-object settings for Lightmass
 */
//@warning: this structure is manually mirrored in UnObj.h
struct LightmassPrimitiveSettings
{
	/** If TRUE, this object will be lit as if it receives light from both sides of its polygons. */
	var()	bool		bUseTwoSidedLighting;
	/** If TRUE, this object will only shadow indirect lighting.  					*/
	var()	bool		bShadowIndirectOnly;
	/** If TRUE, allow using the emissive for static lighting.						*/
	var()	bool		bUseEmissiveForStaticLighting;
    /** Direct lighting falloff exponent for mesh area lights created from emissive areas on this primitive. */
	var()	float		EmissiveLightFalloffExponent;
	/**
	 * Direct lighting influence radius.
	 * The default is 0, which means the influence radius should be automatically generated based on the emissive light brightness.
	 * Values greater than 0 override the automatic method.
	 */
	var()	float		EmissiveLightExplicitInfluenceRadius;
	/** Scales the emissive contribution of all materials applied to this object.	*/
	var()	float		EmissiveBoost;
	/** Scales the diffuse contribution of all materials applied to this object.	*/
	var()	float		DiffuseBoost;
	/** Scales the specular contribution of all materials applied to this object.	*/
	var		float		SpecularBoost;
	/** Fraction of samples taken that must be occluded in order to reach full occlusion. */
	var()	float		FullyOccludedSamplesFraction;
	
	structdefaultproperties
	{
		bUseTwoSidedLighting=false
		bShadowIndirectOnly=false
		bUseEmissiveForstaticLighting=false
		EmissiveLightFalloffExponent=2.0
		EmissiveLightExplicitInfluenceRadius=0.0
		EmissiveBoost=1.0
		DiffuseBoost=1.0
		SpecularBoost=1.0
		FullyOccludedSamplesFraction=1.0
	}
};

/**
 *	Debug options for Lightmass
 */
struct native LightmassDebugOptions
{
	/**
	 *	If FALSE, UnrealLightmass.exe is launched automatically (default)
	 *	If TRUE, it must be launched manually (e.g. through a debugger) with the -debug command line parameter.
	 */
	var() bool	bDebugMode;

	/**
	 *	If TRUE, all participating Lightmass agents will report back detailed stats to the log.
	 */
	var() bool	bStatsEnabled;

	/**
	 *	If TRUE, BSP surfaces split across model components are joined into 1 mapping
	 */
	var() bool	bGatherBSPSurfacesAcrossComponents;

	/**
	 *	The tolerance level used when gathering BSP surfaces.
	 */
	var() float	CoplanarTolerance;

	/**
	 *	If TRUE, deterministic lighting mode will be used.
	 */
	var() bool	bUseDeterministicLighting;

	/**
	 *	If TRUE, Lightmass will import mappings immediately as they complete.
	 *	It will not process them, however.
	 */
	var() bool	bUseImmediateImport;

	/**
	 *	If TRUE, Lightmass will process appropriate mappings as they are imported.
	 *	NOTE: Requires ImmediateMode be enabled to actually work.
	 */
	var() bool	bImmediateProcessMappings;

	/**
	 *	If TRUE, Lightmass will sort mappings by texel cost.
	 */
	var() bool	bSortMappings;

	/**
	 *	If TRUE, the generate coefficients will be dumped to binary files.
	 */
	var() bool	bDumpBinaryFiles;

	/**
	 *	If TRUE, Lightmass will write out BMPs for each generated material property
	 *	sample to <GAME>\ScreenShots\Materials.
	 */
	var() bool	bDebugMaterials;

	/**
	 *	If TRUE, Lightmass will pad the calculated mappings to reduce/eliminate seams.
	 */
	var() bool	bPadMappings;

	/**
	 *	If TRUE, will fill padding of mappings with a color rather than the sampled edges.
	 *	Means nothing if bPadMappings is not enabled...
	 */
	var() bool	bDebugPaddings;

	/**
	 * If TRUE, only the mapping containing a debug texel will be calculated, all others
	 * will be set to white
	 */
	var() bool	bOnlyCalcDebugTexelMappings;

	/** If TRUE, color lightmaps a random color */
	var() bool	bUseRandomColors;

	/** If TRUE, a green border will be placed around the edges of mappings */
	var() bool	bColorBordersGreen;

	/**
	 * If TRUE, Lightmass will overwrite lightmap data with a shade of red relating to
	 * how long it took to calculate the mapping (Red = Time / ExecutionTimeDivisor)
	 */
	var() bool	bColorByExecutionTime;

	/**
	 * The amount of time that will be count as full red when bColorByExecutionTime is enabled
	 */
	var()   float	ExecutionTimeDivisor;

	var bool	bInitialized;

	structcpptext
	{
		//@lmtodo. For some reason, the global instance is not initializing to the default settings...
		// Be sure to update this function to properly set the desired initial values!!!!
		void Touch();
	}

	structdefaultproperties
	{
		bDebugMode=false
		bStatsEnabled=false
		bGatherBSPSurfacesAcrossComponents=true
		CoplanarTolerance=0.001f
		bUseDeterministicLighting=true
		bUseImmediateImport=true
		bImmediateProcessMappings=true
		bSortMappings=true
		bDumpBinaryFiles=false
		bDebugMaterials=false
		bPadMappings=true
		bDebugPaddings=false
		bOnlyCalcDebugTexelMappings=false
		bColorByExecutionTime=false
		ExecutionTimeDivisor=15.0f
	}
};

/**
 *	Debug options for Swarm
 */
struct native SwarmDebugOptions
{
	/**
	 *	If TRUE, Swarm will distribute jobs.
	 *	If FALSE, only the local machine will execute the jobs.
	 */
	var() bool	bDistributionEnabled;

	/**
	 *	If TRUE, Swarm will force content to re-export rather than using the cached version.
	 *	If FALSE, Swarm will attempt to use the cached version.
	 */
	var() bool	bForceContentExport;

	var bool	bInitialized;

	structcpptext
	{
		//@lmtodo. For some reason, the global instance is not initializing to the default settings...
		// Be sure to update this function to properly set the desired initial values!!!!
		void Touch();
	}

	structdefaultproperties
	{
		bDistributionEnabled=true
		bForceContentExport=false
	}
};

/**
 *	Contains precomputed curve of root motion for a particular animation
 */
struct native RootMotionCurve
{
	/**
	 *	Name of the animation this curve is associated with
	 */
	var()	Name	AnimName;

	/**
	 * List of vectors offset from the start of the curve
	 */
	var()	InterpCurveVector	Curve;
	/**
	 * The max input value of the curve
	 */
	var()	float	MaxCurveTime;
};

/** reference to a specific material in a PrimitiveComponent */
struct native PrimitiveMaterialRef
{
	var PrimitiveComponent Primitive;
	var int MaterialIndex;

	structcpptext
	{
		FPrimitiveMaterialRef()
		{}
		FPrimitiveMaterialRef(EEventParm)
		{
			appMemzero(this, sizeof(FPrimitiveMaterialRef));
		}
		FPrimitiveMaterialRef(UPrimitiveComponent* InPrimitive, INT InMaterialIndex)
		: Primitive(InPrimitive), MaterialIndex(InMaterialIndex)
		{}
	}
};

/** reference to a specific material in a MaterialEffect */
struct native PostProcessMaterialRef
{
	var MaterialEffect Effect;

	structcpptext
	{
		FPostProcessMaterialRef()
		{}
		FPostProcessMaterialRef(EEventParm)
		{
			appMemzero(this, sizeof(FPostProcessMaterialRef));
		}
		FPostProcessMaterialRef(UMaterialEffect* InEffect)
		: Effect(InEffect)
		{}
	}
};

/** used by matinee material parameter tracks to hold material references to modify */
struct native MaterialReferenceList
{
	var() MaterialInterface TargetMaterial;
	var edithide array<PrimitiveMaterialRef> AffectedMaterialRefs;
	var edithide array<PostProcessMaterialRef> AffectedPPChainMaterialRefs;
};

struct native VelocityObstacleStat
{
	var Vector	Position;
	var Vector	Velocity;
	var float   Radius;
	var int     Priority;
};