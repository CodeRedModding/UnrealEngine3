/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SphericalHarmonicLightComponent extends LightComponent
	native(Light)
	hidecategories(Object)
	editinlinenew;

/** Colored SH coefficients for the light intensity, parameterized by the world-space incident angle. */
var() SHVectorRGB WorldSpaceIncidentLighting;

/**
 * If TRUE, the SH light can be combined into the base pass as an optimization.  
 * If FALSE, the SH light will be rendered after modulated shadows.
 */
var bool bRenderBeforeModShadows;

cpptext
{
	// ULightComponent interface.
	virtual FLightSceneInfo* CreateSceneInfo() const;
	virtual FVector4 GetPosition() const;
	virtual ELightComponentType GetLightType() const;
}

defaultproperties
{
	CastShadows=False
	bRenderBeforeModShadows=False
}
