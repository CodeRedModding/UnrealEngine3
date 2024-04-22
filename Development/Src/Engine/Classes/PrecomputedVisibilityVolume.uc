//=============================================================================
// PrecomputedVisibilityVolume:  Indicates where visibility should be precomputed
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class PrecomputedVisibilityVolume extends Volume
	native
	hidecategories(Collision,Brush,Attachment,Physics,Volume)
	placeable;

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=false
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=false
		RBChannel=RBCC_Nothing
	End Object

	bColored=true
	BrushColor=(R=25,G=255,B=25,A=255)

	bWorldGeometry=false
	bCollideActors=false
	bBlockActors=false
}
