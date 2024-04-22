/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryFogVolumeSphericalDensityInfo extends ActorFactoryFogVolumeConstantDensityInfo
	config(Editor)
	native(FogVolume);

cpptext
{
	virtual AActor* CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData );
}

defaultproperties
{
	MenuName="Add FogVolumeSphericalDensityInfo"
	NewActorClass=class'Engine.FogVolumeSphericalDensityInfo'
}
