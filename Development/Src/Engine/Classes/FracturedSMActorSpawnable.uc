//=============================================================================
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class FracturedSMActorSpawnable extends FracturedStaticMeshActor;


defaultproperties
{
	Begin Object Name=LightEnvironment0
		bEnabled=TRUE
	End Object

	Begin Object Name=FracturedStaticMeshComponent0
		bForceDirectLightMap=FALSE
		LightEnvironment=LightEnvironment0
	End Object

	bNoDelete=FALSE
}