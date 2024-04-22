//=============================================================================
// DefaultPhysicsVolume:  the default physics volume for areas of the level with
// no physics volume specified
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class DefaultPhysicsVolume extends PhysicsVolume
	native
	notplaceable
	transient;

event Destroyed()
{
	`log(self$" destroyed!");
	assert(false);
}

defaultproperties
{
	// Visual things should be ticked in parallel with physics
	TickGroup=TG_DuringAsyncWork

	bStatic=false
	bNoDelete=false
}
