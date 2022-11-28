class PhATActor extends Actor
	native;

var PhATSkeletalMeshComponent	SkelComponent;
var	RB_Handle					MouseHandle;
var int							PhysicsAssetEditor;

defaultproperties
{
	Begin Object Class=PhATSkeletalMeshComponent Name=PhATSkeletalMeshComponent0
		CollideActors=false
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=false
		BlockRigidBody=true
		PhysicsWeight=1.0
	End Object
	SkelComponent=PhATSkeletalMeshComponent0
	CollisionComponent=PhATSkeletalMeshComponent0
	Components.Remove(CollisionCylinder)
	Components.Remove(Sprite)
	Components.Add(PhATSkeletalMeshComponent0)

	Begin Object Class=RB_Handle Name=RB_Handle0
	End Object
	Components.Add(RB_Handle0)
	MouseHandle=RB_Handle0

	Physics=PHYS_None
	bEdShouldSnap=false
	bStatic=false
	bCollideActors=false
	bBlockActors=false
	bWorldGeometry=false
	bCollideWorld=false
}
