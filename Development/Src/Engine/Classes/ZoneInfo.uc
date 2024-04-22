/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ZoneInfo extends Info
	native;

var() float KillZ;		// any actor falling below this level gets destroyed
var() float SoftKill;   // units of grace until land
var() class<KillZDamageType> KillZDamageType<AllowAbstract>;    // damage type for KillZ
var() bool bSoftKillZ;	// enable SoftKill

defaultproperties
{
	KillZ=-262143.0  // this is HALF_WORLD_MAX1
	SoftKill=2500.0
	bStatic=true
	bNoDelete=true
	bGameRelevant=true
	KillZDamageType=class'KillZDamageType'
}
