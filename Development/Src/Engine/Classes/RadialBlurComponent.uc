/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RadialBlurComponent extends ActorComponent
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/**
 *	Draws blur eminating outward from the world location of the component.
 */

/** Material to affect radial blur opacity/color */
var() const MaterialInterface Material;
/** Scene DPG determines order in which effect is drawn */
var() const ESceneDepthPriorityGroup DepthPriorityGroup;
/** Scale for the overall blur vectors */
var() const interp float BlurScale;
/** Exponent for falloff rate of blur vectors */
var() const interp float BlurFalloffExponent;
/** Amount to alpha blend the blur effect with the existing scene */
var() const interp float BlurOpacity;
/** Max distance where effect is rendered. If further than this then culled */
var() const float MaxCullDistance;
/** Rate of falloff based on distance from view origin */
var() const float DistanceFalloffExponent;
/** 
 * if TRUE then radial blur vectors are rendered to the velocity buffer 
 * instead of being used to manually sampling scene color values 
 */
var() const bool bRenderAsVelocity;
/** if TRUE the effect is enabled and rendered in the scene */
var() const bool bEnabled;

/** The current parent to world transform of the component */
var native transient const matrix LocalToWorld;

// Accessors
native function SetMaterial(MaterialInterface InMaterial);
native function SetBlurScale(float InBlurScale);
native function SetBlurFalloffExponent(float InBlurFalloffExponent);
native function SetBlurOpacity(float InBlurOpacity);
native function SetEnabled(bool bInEnabled);

/** Called from matinee code when BlurScale property changes. */
function OnUpdatePropertyBlurScale()
{
	SetBlurScale(BlurScale);
}

/** Called from matinee code when BlurFalloffExponent property changes. */
function OnUpdatePropertyBlurFalloffExponent()
{
	SetBlurFalloffExponent(BlurFalloffExponent);
}

/** Called from matinee code when BlurOpacity property changes. */
function OnUpdatePropertyBlurOpacity()
{
	SetBlurOpacity(BlurOpacity);
}

cpptext
{
protected:
	// ActorComponent interface.
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
}

defaultproperties
{
	BlurScale=1.0
	BlurFalloffExponent=1.5
	BlurOpacity=1.0
	MaxCullDistance=2000
	DistanceFalloffExponent=1.5
	bRenderAsVelocity=TRUE
	bEnabled=TRUE

	DepthPriorityGroup=SDPG_Foreground
}
