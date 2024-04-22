//=============================================================================
// The Hinge joint class.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class RB_HingeActor extends RB_ConstraintActor
    placeable;

defaultproperties
{
	Begin Object Class=RB_HingeSetup Name=MyHingeSetup
	End Object
	ConstraintSetup=MyHingeSetup

	
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.S_KHinge'
		HiddenGame=True
	End Object

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowColor=(R=255,G=64,B=64)
		bTreatAsASprite=True
		SpriteCategoryName="Physics"
	End Object
	Components.Add(ArrowComponent0)
}
