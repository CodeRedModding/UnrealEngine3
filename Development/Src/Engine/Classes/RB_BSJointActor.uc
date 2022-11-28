//=============================================================================
// The Ball-and-Socket joint class.
//=============================================================================

class RB_BSJointActor extends RB_ConstraintActor
    placeable;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_KBSJoint'
	End Object

	Begin Object Class=RB_BSJointSetup Name=MyBSJointSetup
	End Object
	ConstraintSetup=MyBSJointSetup
}