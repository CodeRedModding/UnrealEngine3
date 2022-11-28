//=============================================================================
// BlockingVolume:  a bounding volume
// used to block certain classes of actors
// primary use is to provide collision for non-zero extent traces around static meshes 

//=============================================================================

class BlockingVolume extends Volume
	native;

var() bool bClampFluid;

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=true
		BlockActors=true
		BlockZeroExtent=false
		BlockNonZeroExtent=true
		BlockRigidBody=true
	End Object

	bWorldGeometry=true
    bCollideActors=True
    bBlockActors=True
    bClampFluid=True
}