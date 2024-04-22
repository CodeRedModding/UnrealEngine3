/**
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 *	Utility class designed to allow you to connect a MaterialInterface to a Matinee action.
 */

class MaterialInstanceActor extends Actor
	native
	placeable
	hidecategories(Movement)
	hidecategories(Advanced)
	hidecategories(Collision)
	hidecategories(Display)
	hidecategories(Actor)
	hidecategories(Attachment);

/** Pointer to MaterialInterface that we want to control paramters of using Matinee. */
var()	MaterialInstanceConstant	MatInst;

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.MatInstActSprite'
		HiddenGame=true
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Materials"
	End Object
	Components.Add(Sprite)

	bNoDelete=true
}
