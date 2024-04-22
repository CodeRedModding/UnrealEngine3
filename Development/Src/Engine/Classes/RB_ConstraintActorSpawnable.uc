/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


/** RB_ConstraintActor spawnable during gameplay */
class RB_ConstraintActorSpawnable extends RB_ConstraintActor
	notplaceable;

defaultproperties
{
	bNoDelete=false

	Begin Object Class=RB_ConstraintSetup Name=MyConstraintSetup
	End Object
	ConstraintSetup=MyConstraintSetup
}
