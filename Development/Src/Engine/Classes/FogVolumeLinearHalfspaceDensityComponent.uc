/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FogVolumeLinearHalfspaceDensityComponent extends FogVolumeDensityComponent
	native(FogVolume)
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/** The linear distance based density coefficient */
var()	interp	float	PlaneDistanceFactor;

/** The plane that defines the fogged halfspace.  The normal of this plane faces away from the fogged halfspace. */
var		interp	plane	HalfspacePlane;

cpptext
{
protected:
	// ActorComponent interface.
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);

public:
	// FogVolumeDensityComponent interface.
	virtual class FFogVolumeDensitySceneInfo* CreateFogVolumeDensityInfo(const UPrimitiveComponent* MeshComponent) const;
}

defaultproperties
{
	PlaneDistanceFactor=0.1
	HalfspacePlane=(X=0.0,Y=0.0,Z=1.0,W=-300.0)
}
