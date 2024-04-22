/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SpotLightComponent extends PointLightComponent
	native(Light)
	hidecategories(Object)
	editinlinenew;

var() float	InnerConeAngle;
var() float OuterConeAngle;

/** Angle from the spot direction that pixels must be in to affect light shafts from this light. */
var(LightShafts) float LightShaftConeAngle;

var const DrawLightConeComponent PreviewInnerCone;
var const DrawLightConeComponent PreviewOuterCone;

var() const rotator Rotation;

cpptext
{
	// UActorComponent interface.
	virtual void Attach();

	// ULightComponent interface.
	virtual FLightSceneInfo* CreateSceneInfo() const;
	virtual UBOOL AffectsBounds(const FBoxSphereBounds& Bounds) const;
	virtual FLinearColor GetDirectIntensity(const FVector& Point) const;
	virtual ELightComponentType GetLightType() const;
	virtual void PostLoad();

	// update the LocalToWorld matrix
	virtual void SetTransformedToWorld();
}

native final function SetRotation( rotator NewRotation );

defaultproperties
{
	InnerConeAngle=0
	OuterConeAngle=44
	LightShaftConeAngle=89
}
