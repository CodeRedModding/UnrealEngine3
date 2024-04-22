/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
// An ambient sound that moves in the world
 
class AmbientSoundMovable extends AmbientSound
	native( Sound );

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.AmbientSoundIcons.S_Ambient_Sound_Moveable'
		Scale=0.25
	End Object

	Begin Object Name=DrawSoundRadius0
		SphereColor=(R=102,G=204,B=51)
	End Object

	TickGroup=TG_DuringAsyncWork
	Physics=PHYS_Interpolating
	bMovable=TRUE
	bStatic=FALSE
}
