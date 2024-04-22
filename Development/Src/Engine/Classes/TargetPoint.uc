/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class TargetPoint extends KeyPoint
	native;

//Texture to display in the editor when this point is being used as a spawn point
var transient editoronly Texture2D SpawnSpriteTexture;

//Amount of objects that are using this target point as a spawn point
var transient int SpawnRefCount;

cpptext
{
	/** Increment the number of spawning systems referencing this target point */
	void IncrementSpawnRef();
	/** Decrement the number of spawning systems referencing this target point */
	void DecrementSpawnRef();
}


defaultproperties
{
	SpawnSpriteTexture=Texture2D'EditorMaterials.TargetIconSpawn'

	Begin Object Name=Sprite
		Sprite=Texture2D'EditorMaterials.TargetIcon'
		Scale=0.35
		HiddenGame=true
		AlwaysLoadOnClient=false
		AlwaysLoadOnServer=false
		SpriteCategoryName="TargetPoint"
	End Object

	Begin Object Class=ArrowComponent Name=Arrow
		ArrowColor=(R=150,G=200,B=255)
		ArrowSize=0.5
		bTreatAsASprite=true
		HiddenGame=true
		AlwaysLoadOnClient=false
		AlwaysLoadOnServer=false
		SpriteCategoryName="TargetPoint"
	End Object
	Components.Add(Arrow)

	bStatic=false
	bNoDelete=true
	bMovable=true
}
