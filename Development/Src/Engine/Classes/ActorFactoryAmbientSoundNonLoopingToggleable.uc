/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryAmbientSoundNonLoopingToggleable extends ActorFactoryAmbientSoundSimpleToggleable
	config( Editor )
	collapsecategories
	hidecategories( Object )
	native;

cpptext
{
	virtual AActor* CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData );
}

defaultproperties
{
	MenuName="Add AmbientSoundNonLoopingToggleable"
	NewActorClass=class'Engine.AmbientSoundNonLoopingToggleable'
}
