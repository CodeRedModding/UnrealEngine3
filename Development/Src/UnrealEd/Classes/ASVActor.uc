class ASVActor extends Actor
	native;

var SkeletalMeshComponent	SkelComponent;

defaultproperties
{
	Begin Object Class=AnimNodeSequence Name=AnimNodeSeq0
	End Object

	Begin Object Class=SkeletalMeshComponent Name=SkeletalMeshComponent0
		Animations=AnimNodeSeq0
		CollideActors=False
		BlockActors=False
		BlockZeroExtent=False
		BlockNonZeroExtent=False
		BlockRigidBody=False
	End Object
	SkelComponent=SkeletalMeshComponent0
	CollisionComponent=SkeletalMeshComponent0
	Components.Remove(CollisionCylinder)
	Components.Remove(Sprite)
	Components.Add(SkeletalMeshComponent0)

	Physics=PHYS_None
	bEdShouldSnap=False
	bStatic=False
	bCollideActors=False
	bBlockActors=False
	bWorldGeometry=False
	bCollideWorld=False
}