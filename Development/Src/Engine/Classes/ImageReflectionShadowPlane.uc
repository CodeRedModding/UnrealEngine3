/**
 * An actor which specifies a reflection plane used to render dynamic shadows for image reflections.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ImageReflectionShadowPlane extends Actor
	showcategories(Movement)
	native(Mesh)
	placeable;

/** replicated copy of ImageReflectionShadowPlaneComponent's bEnabled property */
var repnotify bool bEnabled;

/** Image reflection component */
var() ImageReflectionShadowPlaneComponent ReflectionShadowComponent;

replication
{
	if (Role == ROLE_Authority)
		bEnabled;
}

event PostBeginPlay()
{
	Super.PostBeginPlay();

	bEnabled = ReflectionShadowComponent.bEnabled;
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'bEnabled')
	{
		ReflectionShadowComponent.SetEnabled(bEnabled);
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

/* epic ===============================================
* ::OnToggle
*
* Scripted support for toggling image reflections, checks which
* operation to perform by looking at the action input.
*
* Input 1: turn on
* Input 2: turn off
* Input 3: toggle
*
* =====================================================
*/
simulated function OnToggle(SeqAct_Toggle action)
{
	if (action.InputLinks[0].bHasImpulse)
	{
		// turn on
		ReflectionShadowComponent.SetEnabled(TRUE);
	}
	else if (action.InputLinks[1].bHasImpulse)
	{
		// turn off
		ReflectionShadowComponent.SetEnabled(FALSE);
	}
	else if (action.InputLinks[2].bHasImpulse)
	{
		// toggle
		ReflectionShadowComponent.SetEnabled(!ReflectionShadowComponent.bEnabled);
	}
	bEnabled = ReflectionShadowComponent.bEnabled;
	ForceNetRelevant();
	SetForcedInitialReplicatedProperty(Property'Engine.ImageReflectionShadowPlane.bEnabled', (bEnabled == default.bEnabled));
}

defaultproperties
{
	bEnabled=true
	bStatic=FALSE
	bNoDelete=true

	// Network settings taken from AInfo
	RemoteRole=ROLE_None
	NetUpdateFrequency=10
	bOnlyDirtyReplication=TRUE
	bSkipActorPropertyReplication=TRUE

	Physics=PHYS_Interpolating

	Begin Object Class=ImageReflectionShadowPlaneComponent Name=ReflectionComponent0
	End Object
	ReflectionShadowComponent=ReflectionComponent0
	Components.Add(ReflectionComponent0)

	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Actor'
		HiddenGame=TRUE
		AlwaysLoadOnClient=FALSE
		AlwaysLoadOnServer=FALSE
		SpriteCategoryName="ImageReflection"
	End Object
	Components.Add(Sprite)
}
