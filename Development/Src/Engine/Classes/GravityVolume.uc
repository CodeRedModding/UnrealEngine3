/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GravityVolume extends PhysicsVolume
	native
	placeable;

/**
 *	Simple PhysicsVolume that modifies the gravity inside it.
 */

/** Gravity along Z axis applied to objects inside this volume. */
var()	float	GravityZ;

cpptext
{
	virtual FLOAT GetGravityZ() { return GravityZ; }
}

defaultproperties
{
	GravityZ = -520.0
}
