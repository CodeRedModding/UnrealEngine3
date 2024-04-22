/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SlotToSlotReachSpec extends ForcedReachSpec
	native;

cpptext
{
	virtual INT defineFor( class ANavigationPoint *begin, class ANavigationPoint * dest, class APawn * Scout );
	virtual INT CostFor(APawn* P);
	virtual UBOOL CanBeSkipped( APawn* P );
	virtual UBOOL PrepareForMove(AController * C);
}

// Value CoverLink.ECoverDirection for movement direction along this spec
var() editconst Byte SpecDirection;

defaultproperties
{
	bSkipPrune=FALSE
	PruneSpecList(0)=class'ReachSpec'
}
