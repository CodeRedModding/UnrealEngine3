/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MeshComponentFactory extends PrimitiveComponentFactory
	native
	abstract;

var(Rendering) array<MaterialInterface>	Materials;

cpptext
{
	virtual UPrimitiveComponent* CreatePrimitiveComponent(UObject* InOuter) { return NULL; }
}

defaultproperties
{
	CastShadow=True
}
