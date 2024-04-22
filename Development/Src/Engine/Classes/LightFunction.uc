/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LightFunction extends Object
	native(Light)
	hidecategories(Object)
	collapsecategories
	editinlinenew;

var() const MaterialInterface	SourceMaterial;

var() vector					Scale;

/** 
 * Brightness factor applied to the light when the light function is specified but disabled, for example in scene captures that use SceneCapView_LitNoShadows. 
 * This should be set to the average brightness of the light function material's emissive input, which should be between 0 and 1.
 */
var() float						DisabledBrightness;

defaultproperties
{
	Scale=(X=1024.0,Y=1024.0,Z=1024.0)
	DisabledBrightness=1
}
