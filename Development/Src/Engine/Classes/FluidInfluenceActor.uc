/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class FluidInfluenceActor extends Actor
	dependson(FluidInfluenceComponent)
	native(Fluid)
	ClassGroup(Fluid)
	placeable;


/** Direction of a flow influence. */
var private ArrowComponent FlowDirection;

var private SpriteComponent Sprite;

var() editconst const FluidInfluenceComponent InfluenceComponent;

/** replicated flags to pass to component */
var repnotify bool bActive, bToggled;

replication
{
	if (bNetDirty)
		bActive, bToggled;
}

/**
 * Handling Toggle event from Kismet.
 */
simulated function OnToggle( SeqAct_Toggle inAction )
{
	// Turn ON
	if( inAction.InputLinks[0].bHasImpulse )
	{
		InfluenceComponent.bActive = true;
	}
	// Turn OFF
	else if( inAction.InputLinks[1].bHasImpulse )
	{
		InfluenceComponent.bActive = false;
	}
	// Toggle
	else if( inAction.InputLinks[2].bHasImpulse )
	{
		InfluenceComponent.bActive = !InfluenceComponent.bActive;
		InfluenceComponent.bIsToggleTriggered = true;
	}

	bActive = InfluenceComponent.bActive;
	bToggled = InfluenceComponent.bIsToggleTriggered;
	bForceNetUpdate = true;
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == nameof(bActive))
	{
		InfluenceComponent.bActive = bActive;
	}
	else if (VarName == nameof(bToggled))
	{
		InfluenceComponent.bIsToggleTriggered = bToggled;
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

defaultproperties
{
	bStatic=false
	bMovable=true
	bProjTarget=false
	bCollideActors=false
	bBlockActors=false
	RemoteRole=ROLE_SimulatedProxy
	bNoDelete=true
	bAlwaysRelevant=true
	NetUpdateFrequency=0.1
	bOnlyDirtyReplication=true

	Begin Object Class=SpriteComponent Name=NewSprite
		Sprite=Texture2D'EditorResources.S_FluidSurfOsc'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Fluids"
	End Object
	Sprite=NewSprite
	Components.Add(NewSprite)

	Begin Object Class=ArrowComponent Name=NewArrowComponent
		bTreatAsASprite=True
		HiddenGame=True
		SpriteCategoryName="Fluids"
	End Object
	FlowDirection=NewArrowComponent
	Components.Add(NewArrowComponent)

	Begin Object Class=FluidInfluenceComponent Name=NewInfluenceComponent
	End Object
	InfluenceComponent=NewInfluenceComponent
	Components.Add(NewInfluenceComponent)
}
