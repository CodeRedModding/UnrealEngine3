/**
 * AnimNodeBlendByPhysics.uc
 * Looks at the physics of the Pawn that owns this node and blends accordingly.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNodeBlendByPhysics extends AnimNodeBlendList
		native(Anim);

cpptext
{
	virtual	void TickAnim(FLOAT DeltaSeconds);
}

defaultproperties
{
	bFixNumChildren=true
	Children(0)=(Name="PHYS_None")
	Children(1)=(Name="PHYS_Walking")
	Children(2)=(Name="PHYS_Falling")
	Children(3)=(Name="PHYS_Swimming")
	Children(4)=(Name="PHYS_Flying")
	Children(5)=(Name="PHYS_Rotating")
	Children(6)=(Name="PHYS_Projectile")
	Children(7)=(Name="PHYS_Interpolating")
	Children(8)=(Name="PHYS_Spider")
	Children(9)=(Name="PHYS_Ladder")
	Children(10)=(Name="PHYS_RigidBody")
	Children(11)=(Name="PHYS_SoftBody")
	Children(12)=(Name="PHYS_NavMeshWalking")
	Children(13)=(Name="PHYS_Unused")
	Children(14)=(Name="PHYS_Custom")
}
