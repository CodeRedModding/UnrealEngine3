/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FogVolumeSphericalDensityInfo extends FogVolumeDensityInfo
	showcategories(Movement)
	native(FogVolume)
	placeable;

defaultproperties
{
	Begin Object Name=AutomaticMeshComponent0
		StaticMesh=StaticMesh'EngineMeshes.Sphere'
		bCastDynamicShadow=FALSE
		BlockRigidBody=false
		bForceDirectLightMap=FALSE
		bAcceptsDynamicLights=FALSE
		bAcceptsLights=FALSE
		CastShadow=FALSE
		bUsePrecomputedShadows=FALSE
		bAcceptsStaticDecals=FALSE
		bAcceptsDynamicDecals=FALSE
		bUseAsOccluder=FALSE
		bSelectable=FALSE
		bIgnoreOwnerHidden=TRUE
		CollideActors=FALSE
	End Object

	Begin Object Class=DrawLightRadiusComponent Name=DrawSphereRadius0
	End Object
	Components.Add(DrawSphereRadius0)

	Begin Object Class=FogVolumeSphericalDensityComponent Name=FogVolumeComponent0
		PreviewSphereRadius=DrawSphereRadius0
	End Object
	DensityComponent=FogVolumeComponent0
	Components.Add(FogVolumeComponent0)
}
