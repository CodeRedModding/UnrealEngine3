//=============================================================================
// LightmassCharacterIndirectDetailVolume:  Defines areas where Lightmass should place more indirect lighting samples than normal.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class LightmassCharacterIndirectDetailVolume extends Volume
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
	BrushColor=(R=155,G=185,B=25,A=255)

	bWorldGeometry=false
	bCollideActors=false
	bBlockActors=false
}
