/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryAmbientSoundMovable extends ActorFactoryAmbientSound
	config( Editor )
	hidecategories( Object )
	native;

cpptext
{
	virtual AActor* CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData );
}

defaultproperties
{
	MenuName="Add AmbientSoundMovable"
	NewActorClass=class'Engine.AmbientSoundMovable'
	bShowInEditorQuickMenu=false;
}
