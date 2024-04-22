//=============================================================================
// RB_ForceFieldExcludeVolume:  a bounding volume which removes the effect of a physical force field.
// * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class RB_ForceFieldExcludeVolume extends Volume
	native(ForceField)
	placeable;

/** Used to identify which force fields this excluder applies to, ie if the channel ID matches then the excluder
excludes this force field*/
var()	int ForceFieldChannel;

/** Physics scene index. */
var	native const int						SceneIndex;

cpptext
{
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=true
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=false
		bDisableAllRigidBody=false
	End Object

	bCollideActors=True
	ForceFieldChannel=1
}
