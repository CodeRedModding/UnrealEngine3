/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SwatTurnReachSpec extends ForcedReachSpec
	native;

cpptext
{
	virtual INT CostFor(APawn* P);
	virtual INT defineFor( class ANavigationPoint *begin, class ANavigationPoint * dest, class APawn * Scout );
	virtual FVector GetForcedPathSize( class ANavigationPoint* Start, class ANavigationPoint* End, class AScout* Scout );
}

// Value CoverLink.ECoverDirection for movement direction along this spec
var() editconst Byte SpecDirection;

defaultproperties
{
	bSkipPrune=FALSE
	PruneSpecList(0)=class'ReachSpec'
}
