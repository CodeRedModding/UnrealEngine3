/** 
 * Weap_FragGrenade
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class Weap_FragGrenade extends Weap_Grenade;

defaultproperties
{
	GrenadeProjectileClass=class'Proj_FragGrenade'
	ItemName="Frag Grenade"

	// Weapon SkeletalMesh
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		StaticMesh=StaticMesh'COG_Grunt_FragGrenade.COG_Grunt_FragGrenade_SMesh'
		CollideActors=false
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=false
	End Object
    Mesh=StaticMeshComponent0

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=StaticMeshComponent0
		Translation=(X=-9.0,Y=3.0,Z=3.50)
		Scale3D=(X=0.5,Y=0.5,Z=0.5)
    End Object
	MeshTranform=TransformComponentMesh0
}