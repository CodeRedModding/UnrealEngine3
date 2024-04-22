/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MantleReachSpec extends ForcedReachSpec
	native;

cpptext
{
	virtual INT CostFor(APawn* P);
	virtual FVector GetForcedPathSize( class ANavigationPoint* Start, class ANavigationPoint* End, AScout* Scout );
	void ReInitialize();
	virtual UBOOL CanBeSkipped( APawn* P );
}

/** This mantle spec climbs up a surface instead of jumping over */
var() bool bClimbUp;

defaultproperties
{
	bSkipPrune=TRUE
}
