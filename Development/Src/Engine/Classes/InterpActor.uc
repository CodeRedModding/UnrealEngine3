class InterpActor extends StaticMeshActor
	native
	placeable;

defaultproperties
{
	Begin Object Name=StaticMeshComponent0
		WireframeColor=(R=255,G=0,B=255,A=255)
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
	End Object

	bStatic=false
	bWorldGeometry=false
	Physics=PHYS_Interpolating

	bNoDelete=false
	bAlwaysRelevant=true
	bSkipActorPropertyReplication=false
	bUpdateSimulatedPosition=true
	bReplicateMovement=true
	RemoteRole=ROLE_SimulatedProxy
}