class CarriedObject extends Actor
    native nativereplication abstract notplaceable;

cpptext
{
	INT* GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

var bool            bHome;
var bool            bHeld;

var PlayerReplicationInfo HolderPRI;
var Pawn      Holder;

var const NavigationPoint LastAnchor;		// recent nearest path
var		float	LastValidAnchorTime;	// last time a valid anchor was found

replication
{
    reliable if (Role == ROLE_Authority)
        bHome, bHeld, HolderPRI;
}

function Actor Position()
{
    if (bHeld)
        return Holder;

    return self;
}

defaultproperties
{
	Physics=PHYS_None
	bOrientOnSlope=true
	RemoteRole=ROLE_SimulatedProxy
	bReplicateMovement=true
	bIgnoreVehicles=true
}
