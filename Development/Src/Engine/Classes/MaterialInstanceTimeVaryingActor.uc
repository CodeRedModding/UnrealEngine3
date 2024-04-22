/**
 *
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class MaterialInstanceTimeVaryingActor extends Actor
	placeable
	hidecategories(Movement)
	hidecategories(Advanced)
	hidecategories(Collision)
	hidecategories(Display)
	hidecategories(Actor)
	hidecategories(Attachment);

/** Pointer to MaterialInterface that we want to activate */
var()	MaterialInstanceTimeVarying	MatInst;

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.MatInstActSprite'
		HiddenGame=TRUE
		AlwaysLoadOnClient=FALSE
		AlwaysLoadOnServer=FALSE
		SpriteCategoryName="Materials"
	End Object
	Components.Add(Sprite)

	bNoDelete=TRUE
}
