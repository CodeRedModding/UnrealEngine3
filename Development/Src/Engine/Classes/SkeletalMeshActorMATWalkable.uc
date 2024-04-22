/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SkeletalMeshActorMATWalkable extends SkeletalMeshActorMAT;

defaultproperties
{
	Physics=PHYS_Walking

	Begin Object Class=CylinderComponent Name=CollisionCylinder
		CollisionRadius=+0034.000000
		CollisionHeight=+0078.000000
		BlockNonZeroExtent=true
		BlockZeroExtent=true
		BlockActors=true
		CollideActors=true
	End Object
	CollisionComponent=CollisionCylinder
	Components.Add(CollisionCylinder)

	bCollideWorld=TRUE
	bCollideActors=TRUE
	bBlockActors=TRUE

}
