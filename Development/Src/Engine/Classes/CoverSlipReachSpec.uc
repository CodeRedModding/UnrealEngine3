/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CoverSlipReachSpec extends ForcedReachSpec
	native;

cpptext
{
	virtual INT CostFor(APawn* P);
	virtual FVector GetForcedPathSize( class ANavigationPoint* Start, class ANavigationPoint* End, class AScout* Scout );
}

// Value CoverLink.ECoverDirection for movement direction along this spec
var() editconst Byte SpecDirection;

defaultproperties
{
	bSkipPrune=TRUE
}
