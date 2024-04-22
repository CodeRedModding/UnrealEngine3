/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FogVolumeSphericalDensityComponent extends FogVolumeDensityComponent
	native(FogVolume)
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/** This is the density at the center of the sphere, which will be the maximum. */
var()	interp	float	MaxDensity;

/** The sphere's center in world space. */
var	vector SphereCenter;

/** The sphere's radius. */
var	float	SphereRadius;

/** A preview component for visualizing the sphere in the editor. */
var const DrawLightRadiusComponent PreviewSphereRadius;

cpptext
{
protected:
	// ActorComponent interface.
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Attach();

public:
	// FogVolumeDensityComponent interface.
	virtual class FFogVolumeDensitySceneInfo* CreateFogVolumeDensityInfo(const UPrimitiveComponent* MeshComponent) const;
}

defaultproperties
{
	MaxDensity=0.002
	SphereCenter=(X=0.0,Y=0.0,Z=0.0)
	SphereRadius=600.0
}
