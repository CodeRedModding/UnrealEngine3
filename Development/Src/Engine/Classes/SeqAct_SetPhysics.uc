class SeqAct_SetPhysics extends SequenceAction
	native(Sequence);

/** Action for changing the physics mode of an Actor. */

/** Physics mode to change the Actor to. */
var()	Actor.EPhysics	NewPhysics;

defaultproperties
{
	ObjName="Set Physics"

	NewPhysics=PHYS_None
}