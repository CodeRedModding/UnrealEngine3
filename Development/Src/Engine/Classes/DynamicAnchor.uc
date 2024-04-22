/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


/** a dynamic anchor is a NavigationPoint temporarily added to the navigation network during gameplay, when the AI is trying
 * to get on the network but there is no directly reachable NavigationPoint available. It tries to find something else that is
 * reachable (for example, part of a ReachSpec) and places one of these there and connects it to the network. Doing it this way
 * allows us to handle these situations without any special high-level code; as far as script is concerned, the AI is moving
 * along a perfectly normal NavigationPoint connected to the network just like any other.
 * DynamicAnchors handle destroying themselves and cleaning up any connections when they are no longer in use.
 */
class DynamicAnchor extends NavigationPoint
	native;

/** current controller that's using us to navigate */
var Controller CurrentUser;

cpptext
{
	/** initializes us with the given user and creates ReachSpecs to connect ourselves to the given endpoints,
	 * using the given ReachSpec as a template
	 * @param InUser the Controller that will be using us for navigation
	 * @param Point1 the first NavigationPoint to connect to
	 * @param Point2 the second NavigationPoint to connect to
	 * @param SpecTemplate the ReachSpec to use as a template for the ReachSpecs we create
	 */
	void Initialize(AController* InUser, ANavigationPoint* Point1, ANavigationPoint* Point2, UReachSpec* SpecTemplate);
	void InitHelper( ANavigationPoint* Start, ANavigationPoint* End, INT NewHeight, INT NewRadius, UReachSpec* SpecTemplate );

	virtual void PostScriptDestroyed();

	virtual void TickSpecial(FLOAT DeltaSeconds);
}

defaultproperties
{
	RemoteRole=ROLE_None
	bStatic=false
	bNoDelete=false
	bCollideWhenPlacing=false
}
