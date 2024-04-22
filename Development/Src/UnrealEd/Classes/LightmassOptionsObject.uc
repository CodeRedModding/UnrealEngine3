/**
 *	Lightmass Options Object
 *	Property window wrapper for various Lightmass settings
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LightmassOptionsObject extends Object
	hidecategories(Object)
	dependson(EngineTypes)
	editinlinenew
	native;

var(Debug)	LightmassDebugOptions	DebugSettings;
var(Swarm)	SwarmDebugOptions	SwarmSettings;

cpptext
{
}
