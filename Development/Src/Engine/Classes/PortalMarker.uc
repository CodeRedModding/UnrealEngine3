/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


/** this Actor marks PortalTeleporters on the navigation network */
class PortalMarker extends NavigationPoint
	native;

/** the portal being marked by this PortalMarker */
var PortalTeleporter MyPortal;

cpptext
{
#if WITH_EDITOR
	virtual void addReachSpecs(AScout* Scout, UBOOL bOnlyChanged);
#endif
	virtual UBOOL ReachedBy(APawn* P, const FVector& TestPosition, const FVector& Dest);
}

/** returns whether this NavigationPoint is a teleporter that can teleport the given Actor */
native function bool CanTeleport(Actor A);

defaultproperties
{
	bCollideWhenPlacing=false
	bHiddenEd=true
}
