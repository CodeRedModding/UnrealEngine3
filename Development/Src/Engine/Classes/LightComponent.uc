/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LightComponent extends ActorComponent
	native(Light)
	noexport
	abstract;


//@warning: this structure is manually mirrored in UnActorComponent.h
struct LightingChannelContainer
{
	/** Whether the lighting channel has been initialized. Used to determine whether UPrimitveComponent::Attach should set defaults. */
	var		bool	bInitialized;
	// User settable channels that are auto set and true for lights
	var()	bool	BSP;
	var()	bool	Static;
	var()	bool	Dynamic;
	// User set channels.
	var()	bool	CompositeDynamic;
	var()	bool	Skybox;
	var()	bool	Unnamed_1;
	var()	bool	Unnamed_2;
	var()	bool	Unnamed_3;
	var()	bool	Unnamed_4;
	var()	bool	Unnamed_5;
	var()	bool	Unnamed_6;
	var()	bool	Cinematic_1;
	var()	bool	Cinematic_2;
	var()	bool	Cinematic_3;
	var()	bool	Cinematic_4;
	var()	bool	Cinematic_5;
	var()	bool	Cinematic_6;
	var()	bool	Cinematic_7;
	var()	bool	Cinematic_8;
	var()	bool	Cinematic_9;
	var()	bool	Cinematic_10;
	var()	bool	Gameplay_1;
	var()	bool	Gameplay_2;
	var()	bool	Gameplay_3;
	var()	bool	Gameplay_4;
	var()	bool	Crowd;
};

var native private	transient noimport const pointer	SceneInfo;

var native const	transient matrix			WorldToLight;
var native const	transient matrix			LightToWorld;

/**
 * GUID used to associate a light component with precomputed shadowing information across levels.
 * The GUID changes whenever the light position changes.
 */
var	const duplicatetransient guid	LightGuid;
/**
 * GUID used to associate a light component with precomputed shadowing information across levels.
 * The GUID changes whenever any of the lighting relevant properties changes.
 */
var const duplicatetransient guid	LightmapGuid;

var() const interp float Brightness <UIMin=0.0 | UIMax=20.0>;
var() const interp color LightColor;

/** 
 * The light function to be applied to this light.
 * Note that only non-lightmapped lights (UseDirectLightMap=False) can have a light function. 
 */
var() const editinline export LightFunction Function;

/** Is this light enabled? */
var() const bool bEnabled;

/**
 * Whether the light should cast any shadows.
 **/
var() const bool CastShadows;

/**
 * Whether the light should cast shadows from static objects.  Also requires Cast Shadows to be set to True.
 */
var() const bool CastStaticShadows;

/**
 * Whether the light should cast shadows from dynamic objects.  Also requires Cast Shadows to be set to True.
 **/
var() bool CastDynamicShadows;

/** Whether the light should cast shadows from objects with composite lighting (i.e. an enabled light environment). */
var() bool bCastCompositeShadow;

/** If bCastCompositeShadow=TRUE, whether the light should affect the composite shadow direction. */
var() bool bAffectCompositeShadowDirection;

/** 
 * If enabled and the light casts modulated shadows, this will cause self-shadowing of shadows rendered from this light to use normal shadow blending. 
 * This is useful to get better quality self shadowing while still having a shadow on the lightmapped environment.  
 * When enabled it incurs most of the rendering overhead of both approaches combined.
 */
var() bool bNonModulatedSelfShadowing;

/** Whether the light should cast shadows only from a dynamic object onto itself. */
var() interp bool bSelfShadowOnly;

/** Whether to allow preshadows (the static environment casting dynamic shadows on dynamic objects) from this light. */
var bool bAllowPreShadow;

/** 
 * Whether the light should cast shadows as if it was movable, regardless of its class. 
 * This is useful for gameplay lights dynamically spawned and attached to a static actor.
 */
var const bool bForceDynamicLight;

/** If set to false on a static light, forces it to use precomputed shadowing instead of precomputed lighting. */
var const bool UseDirectLightMap;

/** Whether light has ever been built into a lightmap */
var const bool bHasLightEverBeenBuiltIntoLightMap;

/** Whether the light can affect dynamic primitives even though the light is not affecting the dynamic channel. */
var const bool bCanAffectDynamicPrimitivesOutsideDynamicChannel;

/** Whether to render light shafts from this light.  Only non-static lights can render light shafts (toggleable, movable or dominant types). */
var(LightShafts) bool bRenderLightShafts;

/** Whether to replace this light's analytical specular with image based specular on materials that support it. */
var(ImageReflection) bool bUseImageReflectionSpecular <bShowOnlyWhenTrue=bShowD3D11Properties>;

/** The precomputed lighting for that light source is valid. It might become invalid if some properties change (e.g. position, brightness). */
var protected const bool bPrecomputedLightingIsValid;

/** Whether this light is being used as the OverrideLightComponent on a primitive and shouldn't affect any other primitives. */
var protected const bool bExplicitlyAssignedLight;

/** Whether this light can be combined into the DLE normally.  Overriden to false in the case of muzzle flashes to prevent SH artifacts */
var bool bAllowCompositingIntoDLE;

/**
 * The light environment which the light affects.
 * NULL represents an implicit default light environment including all primitives and lights with LightEnvironment=NULL.
 */
var const LightEnvironmentComponent LightEnvironment;

/** Lighting channels controlling light/ primitive interaction. Only allows interaction if at least one channel is shared */
var() const LightingChannelContainer LightingChannels;

//@warning: this structure is manually mirrored in UnActorComponent.h
enum ELightAffectsClassification
{
	LAC_USER_SELECTED,
	LAC_DYNAMIC_AFFECTING,
	LAC_STATIC_AFFECTING,
	LAC_DYNAMIC_AND_STATIC_AFFECTING
};

/**
 * This is the classification of this light.  This is used for placing a light for an explicit
 * purpose.  Basically you can now have "type" information with lights and understand the
 * intent of why a light was placed.  This is very useful for content people getting maps
 * from others and understanding why there is a dynamic affect light in the middle of the world
 * with a radius of 32k!  And also useful for being able to do searches such as the following:
 * show me all lights which effect dynamic objects.  Now show me the set of lights which are
 * not explicitly set as Dynamic Affecting lights.
 *
 **/
var() const editconst ELightAffectsClassification LightAffectsClassification;

enum ELightShadowMode
{
	/** Shadows rendered due to absence of light when doing dynamic lighting. High overhead per-light, especially on xbox 360. */
	LightShadow_Normal,
	/** Shadows rendered as a fullscreen pass by modulating entire scene by a shadow factor.  Least expensive, Default. */
	LightShadow_Modulate,
	/** Deprecated */
	LightShadow_ModulateBetter
};

/** The type of shadowing to apply for the light. */
var() ELightShadowMode LightShadowMode;

/** The color to modulate with pixels that receive a dynamic shadow from this light (if it casts modulated shadows). */
var() LinearColor ModShadowColor;

/** Time since the caster was last visible at which the mod shadow will fade out completely.  */
var float ModShadowFadeoutTime;

/** Exponent that controls mod shadow fadeout curve. */
var float ModShadowFadeoutExponent;

/** 
 * The munged index of this light in the light list 
 * 
 * > 0 == static light list
 *   0 == not part of any light list
 * < 0 == dynamic light list
 */
var const native duplicatetransient int LightListIndex;

enum EShadowProjectionTechnique
{
	/** Shadow projection is rendered using either PCF/VSM based on global settings  */
	ShadowProjTech_Default,
	/** Shadow projection is rendered using the PCF (Percentage Closer Filtering) technique. May have heavy banding artifacts */
	ShadowProjTech_PCF,
	/** Shadow projection is rendered using the VSM (Variance Shadow Map) technique. May have shadow offset and light bleed artifacts */
	ShadowProjTech_VSM,
	/** Shadow projection is rendered using the Low quality Branching PCF technique. May have banding and penumbra detection artifacts */
	ShadowProjTech_BPCF_Low,
	/** Shadow projection is rendered using the Medium quality Branching PCF technique. May have banding and penumbra detection artifacts */
	ShadowProjTech_BPCF_Medium,
	/** Shadow projection is rendered using the High quality Branching PCF technique. May have banding and penumbra detection artifacts */
	ShadowProjTech_BPCF_High
};
/** Type of shadow projection to use for this light */
var EShadowProjectionTechnique ShadowProjectionTechnique;

enum EShadowFilterQuality
{
	SFQ_Low,
	SFQ_Medium,
	SFQ_High
};
/** The quality of filtering to use for dynamic shadows cast by the light. */
var() EShadowFilterQuality ShadowFilterQuality;

/**
 * Override for min dimensions (in texels) allowed for rendering shadow subject depths.
 * This also controls shadow fading, once the shadow resolution reaches MinShadowResolution it will be faded out completely.
 * A value of 0 defaults to MinShadowResolution in SystemSettings.
 */
var() int MinShadowResolution;

/**
 * Override for max square dimensions (in texels) allowed for rendering shadow subject depths.
 * A value of 0 defaults to MaxShadowResolution in SystemSettings.
 */
var() int MaxShadowResolution;

/** 
 * Resolution in texels below which shadows begin to be faded out. 
 * Once the shadow resolution reaches MinShadowResolution it will be faded out completely.
 * A value of 0 defaults to ShadowFadeResolution in SystemSettings.
 */
var() int ShadowFadeResolution;

/** Everything closer to the camera than this distance will occlude light shafts for directional lights. */
var(LightShafts) float OcclusionDepthRange;

/** 
 * Scales additive color near the light source.  A value of 0 will result in no additive term. 
 * If BloomScale is 0 and OcclusionMaskDarkness is 1, light shafts will effectively be disabled.
 */
var(LightShafts) interp float BloomScale;

/** Scene color luminance must be larger than this to create bloom in light shafts. */
var(LightShafts) float BloomThreshold;

/** 
 * Scene color luminance must be less than this to receive bloom from light shafts. 
 * This behaves like Photoshop's screen blend mode and prevents over-saturation from adding bloom to already bright areas.
 * The default value of 1 means that a pixel with a luminance of 1 won't receive any bloom, but a pixel with a luminance of .5 will receive half bloom.
 */
var(LightShafts) float BloomScreenBlendThreshold;

/** Multiplies against scene color to create the bloom color. */
var(LightShafts) interp color BloomTint;

/** 100 is maximum blur length, 0 is no blur. */
var(LightShafts) float RadialBlurPercent;

/** 
 * Controls how dark the occlusion masking is, a value of .5 would mean that an occlusion of 0 only darkens underlying color by half. 
 * A value of 1 results in no darkening term.  If BloomScale is 0 and OcclusionMaskDarkness is 1, light shafts will effectively be disabled.
 */
var(LightShafts) interp float OcclusionMaskDarkness;

/** Scales the contribution of the reflection specular highlight. */
var(ImageReflection) float ReflectionSpecularBrightness <bShowOnlyWhenTrue=bShowD3D11Properties>;

/**
 * Toggles the light on or off
 *
 * @param bSetEnabled TRUE to enable the light or FALSE to disable it
 */
native final function SetEnabled(bool bSetEnabled);

/** sets Brightness, LightColor, and/or LightFunction */
native final function SetLightProperties(optional float NewBrightness = Brightness, optional color NewLightColor = LightColor, optional LightFunction NewLightFunction = Function);

/** Script interface to retrieve light location. */
native final function vector GetOrigin();

/** Script interface to retrieve light direction. */
native final function vector GetDirection();

/** Script interface to update the color and brightness on the render thread. */
native final function UpdateColorAndBrightness();

/** Script interface to update light shaft parameters on the render thread. */
native final function UpdateLightShaftParameters();

/** Called from matinee code when BloomScale property changes. */
function OnUpdatePropertyBloomScale()
{
	UpdateLightShaftParameters();
}

/** Called from matinee code when BloomTint property changes. */
function OnUpdatePropertyBloomTint()
{
	UpdateLightShaftParameters();
}

/** Called from matinee code when OcclusionMaskDarkness property changes. */
function OnUpdatePropertyOcclusionMaskDarkness()
{
	UpdateLightShaftParameters();
}

/** Called from matinee code when Brightness property changes. */
function OnUpdatePropertyBrightness()
{
	UpdateColorAndBrightness();
}

/** Called from matinee code when LightColor property changes. */
function OnUpdatePropertyLightColor()
{
	UpdateColorAndBrightness();
}



defaultproperties
{
	LightAffectsClassification=LAC_USER_SELECTED

	Brightness=1.0
	LightColor=(R=255,G=255,B=255)
	bEnabled=TRUE

	// for now we are leaving this as people may be depending on it in script and we just
    // set the specific default settings in each light as they are all pretty different
	CastShadows=TRUE
	CastStaticShadows=TRUE
	CastDynamicShadows=TRUE
	bCastCompositeShadow=TRUE
	bAffectCompositeShadowDirection=TRUE
	bForceDynamicLight=FALSE
	UseDirectLightMap=FALSE
	bPrecomputedLightingIsValid=TRUE

	//All lights default to being able to be composited normally.
	bAllowCompositingIntoDLE=TRUE

	LightingChannels=(BSP=TRUE,Static=TRUE,Dynamic=TRUE,CompositeDynamic=TRUE,bInitialized=TRUE)

	// Use cheap modulated shadowing by default
	LightShadowMode=LightShadow_Modulate
	ModShadowFadeoutExponent=3.0
	// default to PCF shadow projection
	ShadowProjectionTechnique=ShadowProjTech_Default
	ShadowFilterQuality=SFQ_Low

	bRenderLightShafts=false
	OcclusionDepthRange=20000
	BloomScale=2
	BloomThreshold=0
	BloomScreenBlendThreshold=1
	BloomTint=(R=255,G=255,B=255)
	RadialBlurPercent=100
	OcclusionMaskDarkness=.3

	bUseImageReflectionSpecular=false
	ReflectionSpecularBrightness=.2
}


/*

 Notes on all of the various Affecting Classifications


USER SELECTED:
   settings that god knows what they do


DYNAMIC AFFECTING:  // pawns, characters, kactors
	CastShadows=TRUE
	CastStaticShadows=FALSE
	CastDynamicShadows=TRUE
	bForceDynamicLight=TRUE
	UseDirectLightMap=FALSE

    LightingChannels:  Dynamic


STATIC AFFECTING:
	CastShadows=TRUE
	CastStaticShadows=TRUE
	CastDynamicShadows=FALSE
	bForceDynamicLight=FALSE
	UseDirectLightMap=TRUE   // For Toggleables this is UseDirectLightMap=FALSE

    LightingChannels:  BSP, Static


DYNAMIC AND STATIC AFFECTING:
	CastShadows=TRUE
	CastStaticShadows=TRUE
	CastDynamicShadows=TRUE
	bForceDynamicLight=FALSE
	UseDirectLightMap=FALSE

    LightingChannels:  BSP, Dynamic, Static


how to light the skybox?

  -> make a user selected affecting light with the skybox channel checked.
     - if we need to have a special classification for this then we will make it at a later time

SKYLIGHT:
	CastShadows=FALSE
	CastStaticShadows=FALSE
	CastDynamicShadows=FALSE
	bForceDynamicLight=FALSE
	UseDirectLightMap=TRUE

    LightingChannels:  SkyLight


how to only light character then?

  -> Character Lighting Channel  not at this time as people will mis use it
  -> for cinematics (where character only lighting could be used) we just use the unamed_#
	    lighting channels!


*/


