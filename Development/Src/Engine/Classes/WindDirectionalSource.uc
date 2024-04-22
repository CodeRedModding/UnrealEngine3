/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class WindDirectionalSource extends Info
	ClassGroup(Wind)
	placeable;

var() const editconst WindDirectionalSourceComponent	Component;

defaultproperties
{
	Begin Object Class=WindDirectionalSourceComponent Name=WindDirectionalSourceComponent0
	End Object
	Component=WindDirectionalSourceComponent0
	Components.Add(WindDirectionalSourceComponent0)

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=150,G=200,B=255)
		bTreatAsASprite=True
		SpriteCategoryName="Wind"
	End Object
	Components.Add(ArrowComponent0)
	
	Begin Object Name=Sprite
		SpriteCategoryName="Wind"
	End Object

	bNoDelete=true
}
