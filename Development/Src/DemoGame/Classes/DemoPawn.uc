/**
 * DemoPawn
 * Demo pawn demonstrating animating character.
 *
 * Created by:	Daniel Vogel
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class DemoPawn extends Pawn
	config(Game);

simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	return 'ThirdPerson';
}

defaultproperties
{
	Begin Object Class=SkeletalMeshComponent Name=DemoPawnSkeletalMeshComponent
		SkeletalMesh=SkeletalMesh'COG_Grunt.COG_Grunt_AMesh'
		PhysicsAsset=PhysicsAsset'COG_Grunt.COG_Grunt_AMesh_Physics'
		AnimSets(0)=AnimSet'COG_Grunt.COG_Grunt_BasicAnims'
		AnimTreeTemplate=AnimTree'COG_Grunt.Grunt_AnimTree'
		bOwnerNoSee=false
	End Object
	Mesh=DemoPawnSkeletalMeshComponent

	Begin Object Class=TransformComponent Name=DemoPawnTransformComponent
    	TransformedComponent=DemoPawnSkeletalMeshComponent
    End Object

    MeshTransform=DemoPawnTransformComponent
	Components.Add(DemoPawnTransformComponent)
	Components.Remove(Sprite)
}
