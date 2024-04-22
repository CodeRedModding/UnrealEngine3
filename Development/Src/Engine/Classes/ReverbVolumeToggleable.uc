/**
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ReverbVolumeToggleable extends ReverbVolume
	showcategories(Toggle);

simulated function OnToggle(SeqAct_Toggle Action)
{
	// Turn ON
	if (Action.InputLinks[0].bHasImpulse)
	{
		bEnabled = true;
	}
	// Turn OFF
	else if (Action.InputLinks[1].bHasImpulse)
	{
		bEnabled = false;
	}
	// Toggle
	else if (Action.InputLinks[2].bHasImpulse)
	{
		bEnabled = !bEnabled;
	}

	ForceNetRelevant();

	SetForcedInitialReplicatedProperty(Property'Engine.ReverbVolume.bEnabled', (bEnabled == default.bEnabled));
}

defaultproperties
{
	bStatic=false
	bNoDelete=true
}
