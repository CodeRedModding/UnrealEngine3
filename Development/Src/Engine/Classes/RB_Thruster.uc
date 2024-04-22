/** 
 *	Base one of these on an Actor using PHYS_RigidBody and it will apply a force down the negative-X direction
 *	ie. point X in the direction you want the thrust in.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class RB_Thruster extends RigidBodyBase
	placeable
	native(Physics);

cpptext
{
	virtual UBOOL Tick( FLOAT DeltaSeconds, ELevelTick TickType );
}
	

/** If thrust should be applied at the moment. */
var()			bool	bThrustEnabled;

/** Strength of thrust force applied to the base object. */
var()	interp	float	ThrustStrength;

/** Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle action)
{
	// Turn ON
	if (action.InputLinks[0].bHasImpulse)
	{
		bThrustEnabled = true;
	}
	// Turn OFF
	else if (action.InputLinks[1].bHasImpulse)
	{
		bThrustEnabled = false;
	}
	// Toggle
	else if (action.InputLinks[2].bHasImpulse)
	{
		bThrustEnabled = !bThrustEnabled;
	}
}

defaultproperties
{
	// Various physics related items need to be ticked pre physics update
	TickGroup=TG_PreAsyncWork

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		ArrowSize=1.7
		ArrowColor=(R=255,G=180,B=0)
		bTreatAsASprite=True
		SpriteCategoryName="Physics"
	End Object
	Components.Add(ArrowComponent0)

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Thruster'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Physics"
	End Object
	Components.Add(Sprite)

	bHardAttach=true
	bEdShouldSnap=true

	ThrustStrength=100.0
}
