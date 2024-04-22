/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetPhysics extends SequenceAction
	native(Sequence);

/** Action for changing the physics mode of an Actor. */

/** Physics mode to change the Actor to. */
var()	Actor.EPhysics	NewPhysics<autocomment=true>;

defaultproperties
{
	bSuppressAutoComment=false

	ObjName="Set Physics"
	ObjCategory="Physics"

	NewPhysics=PHYS_None
}
