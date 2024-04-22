//=============================================================================
// Used as a simple, one-mesh LOD for a procedural building
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class ProcBuilding_SimpleLODActor extends StaticMeshActor
	native(ProcBuilding);




DefaultProperties
{
	// Turn off Collision OFF
	Begin Object Name=StaticMeshComponent0
		bDisableAllRigidBody=TRUE
	End Object

	bCollideActors=FALSE
	bBlockActors=FALSE
}
