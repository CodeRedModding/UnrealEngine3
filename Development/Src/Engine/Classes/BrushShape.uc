//=============================================================================
// BrushShape: A brush that acts as a template for geometry mode modifiers like "Lathe"
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class BrushShape extends Brush
	placeable
	native;

cpptext
{
	virtual UBOOL IsABrushShape() const {return TRUE;}
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=false
		bAcceptsLights=false
		LightingChannels=(Dynamic=TRUE,bInitialized=TRUE)
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=false
		AlwaysLoadOnClient=True
		AlwaysLoadOnServer=False
	End Object
}
