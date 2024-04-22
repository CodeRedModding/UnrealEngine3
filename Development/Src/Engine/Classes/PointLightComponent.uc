/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PointLightComponent extends LightComponent
	native(Light)
	hidecategories(Object)
	editinlinenew;

/** used to control when point light shadow mapping goes to a hack mode, the ShadowRadiusMultiplier is multiplied by the radius of object's bounding sphere */
var float	ShadowRadiusMultiplier;

var() interp float	Radius<UIMin=8.0 | UIMax=1024.0>;
/** Controls the radial falloff of the light */
var() interp float	FalloffExponent;
/** falloff for shadow when using LightShadow_Modulate */
var() float ShadowFalloffExponent;
/** The minimum radius at which the point light's shadow begins to attenuate. */
var float MinShadowFalloffRadius;

var   const matrix							CachedParentToWorld; //@todo remove me please
var() const vector							Translation;

/** Plane used for planar shadows on mobile.  */
var const plane ShadowPlane;

var const DrawLightRadiusComponent PreviewLightRadius;

/** The Lightmass settings for this object. */
var(Lightmass) LightmassPointLightSettings LightmassSettings <ScriptOrder=true>;
var const DrawLightRadiusComponent PreviewLightSourceRadius;

cpptext
{
protected:
	/**
	 * Updates the light's PreviewLightRadius.
	 */
	void UpdatePreviewLightRadius();

	// UActorComponent interface.
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();
	virtual void UpdateTransform();
public:

	// ULightComponent interface.
	virtual FLightSceneInfo* CreateSceneInfo() const;
	virtual UBOOL AffectsBounds(const FBoxSphereBounds& Bounds) const;
	virtual FVector4 GetPosition() const;
	virtual FBox GetBoundingBox() const;
	virtual FLinearColor GetDirectIntensity(const FVector& Point) const;
	virtual ELightComponentType GetLightType() const;

	// update the LocalToWorld matrix
	virtual void SetTransformedToWorld();

	/**
	 * Called after property has changed via e.g. property window or set command.
	 *
	 * @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual void PostLoad();

	/** Update the PreviewLightSourceRadius */
	virtual void UpdatePreviewLightSourceRadius();
}

native final function SetTranslation(vector NewTranslation);

/** Called from matinee code when LightColor property changes. */
function OnUpdatePropertyLightColor()
{
	UpdateColorAndBrightness();
}

/** Called from matinee code when Brightness property changes. */
function OnUpdatePropertyBrightness()
{
	UpdateColorAndBrightness();
}

defaultproperties
{
	Radius=1024.0
	FalloffExponent=2
	ShadowFalloffExponent=2
	ShadowRadiusMultiplier=1.1
	ShadowPlane=(X=0,Y=0,Z=1,W=0)
}
