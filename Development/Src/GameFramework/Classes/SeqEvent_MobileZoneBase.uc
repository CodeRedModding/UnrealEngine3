/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class for all mobile sequence events that require access to a specific zone.
 */
class SeqEvent_MobileZoneBase extends SeqEvent_MobileBase
	native
	abstract;

cpptext
{
	/**
	 * Called each frame.  
	 * @param Originator is a reference to the PC that caused the input
	 * @param OriginatorInput is a reference to the mobile player input assoicated with this object
	 * @param OriginatorZone is a reference to the zone that caused this update
	 */
	virtual void UpdateZone(APlayerController* Originator, UMobilePlayerInput* OriginatorInput, UMobileInputZone* OriginatorZone);
}

/** Holds the name of the zone we want to be assoicated with */	
var() string TargetZoneName;

/**
 * Try to find the mobile input zone this is assocated with and add it
 */
event AddToMobileInput(MobilePlayerInput MPI)
{
	local MobileInputZone Zone;
	Zone = MPI.Findzone(TargetZoneName);
	if (Zone != none)
	{
		Zone.AddKismetEventHandler(self);
	}
}


defaultproperties
{
}