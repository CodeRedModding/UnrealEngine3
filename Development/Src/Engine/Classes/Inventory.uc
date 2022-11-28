//=============================================================================
// Inventory
//
// Inventory is the parent class of all actors that can be carried by other actors.
// Inventory items are placed in the holding actor's inventory chain, a linked list
// of inventory actors.  Each inventory class knows what pickup can spawn it (its
// PickupClass).  When tossed out (using the DropFrom() function), inventory items
// spawn a DroppedPickup actor to hold them.
//
//=============================================================================

class Inventory extends Actor
	abstract
	native
	nativereplication;

//-----------------------------------------------------------------------------

var	bool				bRenderOverlays;		// If true, this inventory item will be given access to HUD Canvas. RenderOverlays() is called.
var bool				bReceiveOwnerEvents;	// If true, receive Owner events. OwnerEvent() is called.

var	Inventory			Inventory;				// Next Inventory in Linked List
var InventoryManager	InvManager;
var	localized string	ItemName;

//-----------------------------------------------------------------------------
// Pickup related properties
var()	float								RespawnTime;			// Respawn after this time, 0 for instant.
var bool bDelayedSpawn;
var		bool								bPredictRespawns;		// high skill bots may predict respawns for this item
var  float									MaxDesireability;		// Maximum desireability this item will ever have.
var() localized string						PickupMessage;			// Human readable description when picked up.
var() SoundCue PickupSound;
var() string PickupForce;  
var			class<DroppedPickup>			DroppedPickupClass;
var			TransformComponent			DroppedPickupMesh;
var			TransformComponent			PickupFactoryMesh;

cpptext
{
	// Constructors.
	AInventory() {}

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

// Network replication.
replication
{
	// Things the server should send to the client.
	unreliable if ( (Role==ROLE_Authority) && bNetDirty && bNetOwner )
		Inventory;
}


/**
 * Access to HUD and Canvas. Set bRenderOverlays=true to receive event.
 * Event called every frame when the item is in the InventoryManager
 *
 * @param	HUD H
 */
simulated function RenderOverlays( HUD H );

/**
 * Access to HUD and Canvas.
 * Event always called when the InventoryManager considers this Inventory Item currently "Active"
 * (for example active weapon)
 *
 * @param	HUD H
 */
simulated function ActiveRenderOverlays( HUD H );

simulated function String GetHumanReadableName()
{
	return Default.ItemName;
}


/* TravelPreAccept:
	Called after a travelling inventory item has been accepted into a level.
*/
event TravelPreAccept()
{
	super.TravelPreAccept();
	GiveTo( Pawn(Owner) );
}

event Destroyed()
{
	// Notify Pawn's inventory manager that this item is being destroyed (remove from inventory manager).
	if ( Pawn(Owner) != None && Pawn(Owner).InvManager != None )
	{
		Pawn(Owner).InvManager.RemoveFromInventory( Self );
	}
}

/* Inventory has an AI interface to allow AIControllers, such as bots, to assess the
 desireability of acquiring that pickup.  The BotDesireability() method returns a
 float typically between 0 and 1 describing how valuable the pickup is to the
 AIController.  This method is called when an AIController uses the
 FindPathToBestInventory() navigation intrinsic.
*/
static function float BotDesireability( pawn P )
{
	local Inventory AlreadyHas;
	local float desire;

	desire = Default.MaxDesireability;

	if ( Default.RespawnTime < 10 )
	{
		AlreadyHas = P.FindInventoryType(Default.class);
		if ( AlreadyHas != None )
		{
			return -1;
		}
	}
	return desire;
}

/* DetourWeight()
value of this path to take a quick detour (usually 0, used when on route to distant objective, but want to grab inventory for example)
*/
static function float DetourWeight(Pawn Other,float PathWeight)
{
	return 0;
}

/* GiveTo:
	Give this Inventory Item to this Pawn.
	InvManager.AddInventory implements the correct behavior.
*/
final function GiveTo( Pawn Other )
{
	if ( Other != None && Other.InvManager != None )
	{
		Other.InvManager.AddInventory( Self );
	}
}

/* AnnouncePickup
	This inventory item was just picked up (from a DroppedPickup or PickupFactory)
*/
function AnnouncePickup(Pawn Other)
{
	Other.HandlePickup(self);
	Other.PlaySound( PickupSound );
}

/**
 * This Inventory Item has just been given to this Pawn
 *
 * @param	thisPawn	new Inventory owner
 */
function GivenTo( Pawn thisPawn );

/**
 * Event called when Item is removed from Inventory Manager.
 * Network: Authority
 */
function ItemRemovedFromInvManager();


/* DenyPickupQuery
	Function which lets existing items in a pawn's inventory
	prevent the pawn from picking something up. Return true to abort pickup
	or if item handles pickup.
*/
function bool DenyPickupQuery( class<Inventory> ItemClass )
{
	// By default, you can only carry a single item of a given class.
	if ( ItemClass == class )
		return true;

	return false;
}

/* DropFrom
	Toss this item out.
*/
function DropFrom( vector StartLocation, vector StartVelocity )
{
	local DroppedPickup P;

	if ( Instigator != None && Instigator.InvManager != None )
	{
		Instigator.InvManager.RemoveFromInventory( Self );
	}

	P = Spawn(DroppedPickupClass,,, StartLocation);
	if ( P == None )
	{
		Destroy();
		return;
	}

	P.SetPhysics(PHYS_Falling);
	P.Inventory = self;
	P.Velocity = StartVelocity;

	// set up P to render this item
	if ( DroppedPickupMesh != None )
	{
		P.SetPickupMesh(DroppedPickupMesh);
	}
	Instigator = None;
	GotoState('');
}

static function string GetLocalString(
	optional int Switch,
	optional PlayerReplicationInfo RelatedPRI_1,
	optional PlayerReplicationInfo RelatedPRI_2
	)
{
	return Default.PickupMessage;
}

/* OwnerEvent:
	Used to inform inventory when owner event occurs (for example jumping or weapon change)
	set bReceiveOwnerEvents=true to receive events.
*/
function OwnerEvent(name EventName);

defaultproperties
{
	CollisionComponent=None
	Components.Remove(CollisionCylinder)
	bOnlyDirtyReplication=true
	bOnlyRelevantToOwner=true
	bTravel=true
	NetPriority=1.4
	bHidden=true
	Physics=PHYS_None
	bReplicateMovement=false
	RemoteRole=ROLE_SimulatedProxy
	DroppedPickupClass=class'DroppedPickup'
	PickupMessage="Snagged an item."
	MaxDesireability=0.1000
}