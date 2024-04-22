//=============================================================================
// PrecomputedVisibilityOverrideVolume:  Overrides visibility for a set of actors
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class PrecomputedVisibilityOverrideVolume extends Volume
	native
	hidecategories(Collision,Brush,Attachment,Physics,Volume)
	placeable;

/** Array of actors that will always be considered visible by Precomputed Visibility when viewed from inside this volume. */
var() array<actor> OverrideVisibleActors;

/** Array of actors that will always be considered invisible by Precomputed Visibility when viewed from inside this volume. */
var() array<actor> OverrideInvisibleActors;

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
	BrushColor=(R=25,G=120,B=90,A=255)

	bWorldGeometry=false
	bCollideActors=false
	bBlockActors=false
}
