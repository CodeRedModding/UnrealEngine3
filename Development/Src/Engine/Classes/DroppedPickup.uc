//=============================================================================
// Pickup items.
//
// PickupFactory should be used to place items in the level.  This class is for dropped inventory, which should attach
// itself to this pickup, and set the appropriate mesh
//
//=============================================================================
class DroppedPickup extends Actor
	notplaceable
	native;

//-----------------------------------------------------------------------------
// AI related info.
var		Inventory					Inventory;			// the dropped inventory item which spawned this pickup
var		NavigationPoint				PickupCache;		// navigationpoint this pickup is attached to

native final function AddToNavigation();			// cache dropped inventory in navigation network
native final function RemoveFromNavigation();

function Destroyed()
{
	if (Inventory != None )
		Inventory.Destroy();
}

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	destroy();
}

function SetPickupMesh(ActorComponent PickupMesh)
{
	AddComponent(PickupMesh, false);
}

/* DetourWeight()
value of this path to take a quick detour (usually 0, used when on route to distant objective, but want to grab inventory for example)
*/
function float DetourWeight(Pawn Other,float PathWeight)
{
	return Inventory.DetourWeight(Other, PathWeight);
}

/* Inventory has an AI interface to allow AIControllers, such as bots, to assess the
 desireability of acquiring that pickup.  The BotDesireability() method returns a
 float typically between 0 and 1 describing how valuable the pickup is to the
 AIController.  This method is called when an AIController uses the
 FindPathToBestInventory() navigation intrinsic.
*/
function float BotDesireability( pawn Bot )
{
	return Inventory.BotDesireability(Bot);
}

event Landed(Vector HitNormal)
{
	NetUpdateTime = Level.TimeSeconds - 1;
	NetUpdateFrequency = 3;
    AddToNavigation();
}

//=============================================================================
// Pickup state: this inventory item is sitting on the ground.

auto state Pickup
{
	/* ValidTouch()
	 Validate touch (if valid return true to let other pick me up and trigger event).
	*/
	function bool ValidTouch( actor Other )
	{
		// make sure its a live player
		if ( (Pawn(Other) == None) || !Pawn(Other).bCanPickupInventory || (Pawn(Other).DrivenVehicle == None && Pawn(Other).Controller == None) )
			return false;

		// make sure thrower doesn't run over own weapon
		if ( (Physics == PHYS_Falling) && (Velocity.Z > 0) && ((Velocity dot Other.Velocity) > 0) && ((Velocity dot (Location - Other.Location)) > 0) )
			return false;
		
		// make sure not touching through wall
		if ( !FastTrace(Other.Location, Location) )
			return false;

		log("GOT HERE");

		// make sure game will let player pick me up
		if( Level.Game.PickupQuery(Pawn(Other), Inventory.class) )
		{
			return true;
		}
		return false;
	}

	// When touched by an actor.
	event Touch( Actor Other, vector HitLocation, vector HitNormal )
	{
		// If touched by a player pawn, let him pick this up.
		if( ValidTouch(Other) )
		{
			if ( Inventory != None )
			{
				Inventory.AnnouncePickup(Pawn(Other));
				Inventory.GiveTo( Pawn(Other) );
				Inventory = None;
			}
			Destroy();
		}
	}

	function Timer()
	{
		GotoState('FadeOut');
	}

	function BeginState()
	{
		AddToNavigation();
		SetTimer(LifeSpan - 1, false);
	}

	function EndState()
	{
		RemoveFromNavigation();
	}

Begin:
}

State FadeOut extends Pickup
{
	function Tick(float DeltaTime)
	{
		SetDrawScale(FMax(0.01, DrawScale - Default.DrawScale * DeltaTime));
	}

	function BeginState()
	{
		RotationRate.Yaw=60000;
		SetPhysics(PHYS_Rotating);
		LifeSpan = 1.0;
	}
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_Inventory'
	End Object

	Begin Object NAME=CollisionCylinder
		CollisionRadius=+00030.000000
		CollisionHeight=+00020.000000
		CollideActors=true
	End Object

	bOnlyDirtyReplication=true
    NetUpdateFrequency=8
	RemoteRole=ROLE_SimulatedProxy
	bHidden=false
	NetPriority=+1.4
	bCollideActors=true
	bCollideWorld=true
	RotationRate=(Yaw=5000)
	DesiredRotation=(Yaw=30000)
	bOrientOnSlope=true
	bShouldBaseAtStartup=true
	bIgnoreEncroachers=false
	bIgnoreVehicles=true
    LifeSpan=+16.0
}