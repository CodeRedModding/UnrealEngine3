//=============================================================================
// StaticMeshActor.
// An actor that is drawn using a static mesh(a mesh that never changes, and
// can be cached in video memory, resulting in a speed boost).
//=============================================================================

class StaticMeshActor extends Actor
	native
	placeable;

var() const editconst StaticMeshComponent	StaticMeshComponent;

var() bool bExactProjectileCollision;		// nonzero extent projectiles should shrink to zero when hitting this actor

defaultproperties
{
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
	End Object
	StaticMeshComponent=StaticMeshComponent0
	CollisionComponent=StaticMeshComponent0
	Components.Remove(Sprite)
	Components.Remove(CollisionCylinder)
	Components.Add(StaticMeshComponent0)

	bEdShouldSnap=true
	bStatic=true
	bCollideActors=true
	bBlockActors=true
	bWorldGeometry=true
	bExactProjectileCollision=true
}