/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxForceFieldSpawnable extends Actor
	native(ForceField)
	dependson(PrimitiveComponent);
	
var() NxForceFieldComponent ForceFieldComponent;


cpptext
{

}

/** Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle inAction)
{
	if(inAction.InputLinks[0].bHasImpulse)
	{
		ForceFieldComponent.bForceActive = true;
	}
	else if(inAction.InputLinks[1].bHasImpulse)
	{
		ForceFieldComponent.bForceActive = false;
	}
	else if(inAction.InputLinks[2].bHasImpulse)
	{
		ForceFieldComponent.bForceActive = !ForceFieldComponent.bForceActive;
	}
}


defaultproperties
{
	TickGroup=TG_PreAsyncWork
	RemoteRole=ROLE_SimulatedProxy
	bStatic = false
	bNoDelete=false
	bAlwaysRelevant=true
	NetUpdateFrequency=0.1
	bOnlyDirtyReplication=true
    
}
