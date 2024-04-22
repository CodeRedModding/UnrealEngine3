/**
 * This is the PhysicalMaterialPropertyBase which the PhysicalMaterial has.
 * Individual games should derive their own MyGamPhysicalMaterialProperty.
 *
 * Then inside that object you can either have a bunch of properties or have it 
 * point to your game specific objects.
 *
 * (e.g.  You have have impact sounds and impact effects for all of the weapons
 * in your game.  So you have an .uc object which contains the data needed per
 * material type and then you have your MyGamePhysicalMaterialProperty point to 
 * that. )
 *
 * class MyGamePhysicalMaterialProperty extends PhysicalMaterialPropertyBase
 *    editinlinenew;
 *
 * var() editinline MyGameSpecificImpactEffects ImpactEffects;
 * var() editinline MyGameSpecificImpactSounds ImpactSounds;
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class PhysicalMaterialPropertyBase extends Object
	native(Physics)
	collapsecategories
	hidecategories(Object)
	editinlinenew
	abstract;


defaultproperties
{
}
