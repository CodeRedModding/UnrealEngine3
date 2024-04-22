/**
 * An actor which specifies a reflection primitive used by materials that use image based reflections.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ImageReflection extends Actor
	showcategories(Movement)
	AutoExpandCategories(ImageReflection)
	AutoExpandCategories(ImageBasedReflectionComponent)
	native(Mesh)
	placeable;

/** replicated copy of ImageBasedReflectionComponent's bEnabled property */
var repnotify bool bEnabled;

/** Image reflection component */
var deprecated ImageReflectionComponent ReflectionComponent;

/** Image reflection component */
var() ImageBasedReflectionComponent ImageReflectionComponent;

replication
{
	if (Role == ROLE_Authority)
		bEnabled;
}

event PostBeginPlay()
{
	Super.PostBeginPlay();

	bEnabled = ImageReflectionComponent.bEnabled;
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'bEnabled')
	{
		ImageReflectionComponent.SetEnabled(bEnabled);
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
		ImageReflectionComponent.SetEnabled(TRUE);
	}
	else if (action.InputLinks[1].bHasImpulse)
	{
		// turn off
		ImageReflectionComponent.SetEnabled(FALSE);
	}
	else if (action.InputLinks[2].bHasImpulse)
	{
		// toggle
		ImageReflectionComponent.SetEnabled(!ImageReflectionComponent.bEnabled);
	}
	bEnabled = ImageReflectionComponent.bEnabled;
	ForceNetRelevant();
	SetForcedInitialReplicatedProperty(Property'Engine.ImageReflection.bEnabled', (bEnabled == default.bEnabled));
}

cpptext
{
protected:
	virtual void PostLoad();
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

	Begin Object Class=ImageBasedReflectionComponent Name=ReflectionComponent0
	End Object
	ImageReflectionComponent=ReflectionComponent0
	Components.Add(ReflectionComponent0)
}
