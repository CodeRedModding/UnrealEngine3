//=============================================================================
// A sticky note.  Level designers can place these in the level and then
// view them as a batch in the error/warnings window.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class Note extends Actor
	placeable
	native;

var() notforconsole string Text;

cpptext
{
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif
}

defaultproperties
{
	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=200,B=255)
		ArrowSize=0.5
		bTreatAsASprite=True
		HiddenGame=true
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Notes"
	End Object
	Components.Add(Arrow)

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Note'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Notes"
	End Object
	Components.Add(Sprite)

	bStatic=true
	bHidden=true
	bNoDelete=true
	bMovable=false
	bRouteBeginPlayEvenIfStatic=false
}
