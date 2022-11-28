class UTPickupFactory extends PickupFactory
	abstract;

var Controller TeamOwner[4];		// AI controller currently going after this pickup (for team coordination)

/* ShouldCamp()
Returns true if Bot should wait for me
*/
function bool ShouldCamp(UTBot B, float MaxWait)
{
	return false; // FIXMESTEVE ( ReadyToPickup(MaxWait) && (B.RatePickup(self) > 0) && !ReadyToPickup(0) );
}

defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+00030.000000
		CollisionHeight=+00044.000000
		CollideActors=true
	End Object
}
