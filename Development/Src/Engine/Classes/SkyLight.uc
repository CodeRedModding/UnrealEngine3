/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SkyLight extends Light
	native(Light)
	ClassGroup(Lights,SkyLights)
	placeable;

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.LightIcons.Light_SkyLight'
	End Object

	Begin Object Class=SkyLightComponent Name=SkyLightComponent0
		UseDirectLightMap=TRUE
		bCanAffectDynamicPrimitivesOutsideDynamicChannel=TRUE
	End Object
	LightComponent=SkylightComponent0
	Components.Add(SkyLightComponent0)
}
