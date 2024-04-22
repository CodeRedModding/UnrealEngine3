//=============================================================================
// LightmassImportanceVolume:  a bounding volume outside of which Lightmass
// photon emissions are decreased
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class LightmassImportanceVolume extends Volume
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
	BrushColor=(R=255,G=255,B=25,A=255)

	bWorldGeometry=false
	bCollideActors=false
	bBlockActors=false
}
