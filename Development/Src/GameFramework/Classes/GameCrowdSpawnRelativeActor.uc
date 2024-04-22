/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdSpawnRelativeActor extends Actor
	native;

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_NavP'
		HiddenGame=FALSE
		HiddenEditor=FALSE
		AlwaysLoadOnClient=FALSE
		AlwaysLoadOnServer=FALSE
	End Object
	Components.Add(Sprite)
}