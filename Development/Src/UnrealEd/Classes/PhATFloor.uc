class PhATFloor extends StaticMeshActor
	native;

defaultproperties
{
	DrawScale=1.5

	Begin Object Name=StaticMeshComponent0
		StaticMesh=StaticMesh'EditorMeshes.PhAT_FloorBox'
		CollideActors=true
		BlockActors=true
		BlockZeroExtent=true
		BlockNonZeroExtent=true
		BlockRigidBody=true
	End Object
}