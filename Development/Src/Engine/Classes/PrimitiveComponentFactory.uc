/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PrimitiveComponentFactory extends Object
	native
	abstract;

// Collision flags.

var(Collision) const bool	CollideActors,
							BlockActors,
							BlockZeroExtent,
							BlockNonZeroExtent,
							BlockRigidBody;

// Rendering flags.

var(Rendering) bool	HiddenGame,
					HiddenEditor,
					CastShadow;

cpptext
{
	virtual UBOOL FactoryIsValid() { return 1; }
	virtual UPrimitiveComponent* CreatePrimitiveComponent(UObject* InOuter) { return NULL; }
}

defaultproperties
{
}
