/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FogVolumeLinearHalfspaceDensityInfo extends FogVolumeDensityInfo
	showcategories(Movement)
	native(FogVolume)
	placeable;

defaultproperties
{
	Begin Object Class=FogVolumeLinearHalfspaceDensityComponent Name=FogVolumeComponent0
	End Object
	DensityComponent=FogVolumeComponent0
	Components.Add(FogVolumeComponent0)
}
