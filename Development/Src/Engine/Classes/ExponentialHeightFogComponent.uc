/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ExponentialHeightFogComponent extends ActorComponent
	native(FogVolume)
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/** True if the fog is enabled. */
var()	const			bool	bEnabled;

/** z-height for the fog plane - updated by the owning actor */
var		const			float	FogHeight;

/** Global density factor. */
var()	const	interp	float	FogDensity;

/** 
 * Height density factor, controls how the density increases as height decreases.  
 * Smaller values make the visible transition larger.
 */
var()	const	interp	float	FogHeightFalloff;

/** 
 * Maximum opacity of the fog.  
 * A value of 1 means the fog can become fully opaque at a distance and replace scene color completely,
 * A value of 0 means the fog color will not be factored in at all.
 */
var()	const	interp	float	FogMaxOpacity;

/** Distance from the camera that the fog will start, in world units. */
var()	const	interp	float	StartDistance;

/** 
 * LightInscatteringColor is used in the direction of the dominant directional light, and OppositeLightColor is used in the opposite direction.
 * LightTerminatorAngle is the angle in degrees from the dominant directional light that an even amount of OppositeLightColor and LightInscatteringColor are used for the final fog color.
 * If there is no dominant directional light enabled, LightInscatteringColor will correspond to up in world space.
 */
var()	const	interp	float	LightTerminatorAngle;

/** Scales OppositeLightColor. */
var()	const	interp	float	OppositeLightBrightness;

/** Fog Color used for the opposite direction from the dominant directional light. */
var()	const	interp	color	OppositeLightColor;

/** Scales LightInscatteringColor. */
var()	const	interp	float	LightInscatteringBrightness;

/** Fog Color used for the direction of the dominant directional light. */
var()	const	interp	color	LightInscatteringColor;

cpptext
{
protected:
	// ActorComponent interface.
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

/**
 * Changes the enabled state of the height fog component.
 * @param bSetEnabled - The new value for bEnabled.
 */
final native function SetEnabled(bool bSetEnabled);

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	bEnabled=TRUE

	FogDensity=0.02
	FogHeightFalloff=0.2
	FogMaxOpacity=1
	StartDistance=0
	LightTerminatorAngle=45
	OppositeLightBrightness=.2
	OppositeLightColor=(R=177,G=208,B=255)
	LightInscatteringBrightness=1
	LightInscatteringColor=(R=245,G=212,B=41)
}
