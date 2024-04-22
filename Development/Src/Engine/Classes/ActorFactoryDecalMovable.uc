/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ActorFactoryDecalMovable extends ActorFactoryDecal
	config(Editor)
	native(Decal);

defaultproperties
{
	MenuName="Add Movable Decal"
	NewActorClass=class'Engine.DecalActorMovable'
}
