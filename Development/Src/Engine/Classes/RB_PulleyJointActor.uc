//=============================================================================
// The Pulley joint class.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class RB_PulleyJointActor extends RB_ConstraintActor
    placeable;

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.S_KBSJoint'
	End Object

	Begin Object Class=RB_PulleyJointSetup Name=MyPulleyJointSetup
	End Object
	ConstraintSetup=MyPulleyJointSetup
}
