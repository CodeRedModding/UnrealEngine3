/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class SplineLoftActorMovable extends SplineLoftActor
	placeable
	native(Spline);



defaultproperties
{
	Begin Object Class=DynamicLightEnvironmentComponent Name=MyMeshLightEnvironment
		bEnabled=TRUE
	End Object
	MeshLightEnvironment=MyMeshLightEnvironment
	Components.Add(MyMeshLightEnvironment)

	Physics=PHYS_Interpolating

	bNoDelete=true
	bStatic=false
	bMovable=true
}