//=============================================================================
// Inventory
//
// Inventory is the parent class of all actors that can be carried by other actors.
// Inventory items are placed in the holding actor's inventory chain, a linked list
// of inventory actors.  Each inventory class knows what pickup can spawn it (its
// PickupClass).  When tossed out (using the DropFrom() function), inventory items
// spawn a DroppedPickup actor to hold them.
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class Inventory extends Actor
	abstract
	native
	nativereplication;

//-----------------------------------------------------------------------------

var	Inventory			Inventory;				// Next Inventory in Linked List
var InventoryManager	InvManager;
var	localized string	ItemName;

/** if true, this inventory item should be dropped if the owner dies */
var bool bDropOnDeath;

//-----------------------------------------------------------------------------
// Pickup related properties
var bool bDelayedSpawn;
var		bool								bPredictRespawns;		// high skill bots may predict respawns for this item
var()	float								RespawnTime;			// Respawn after this time, 0 for instant.
var  float									MaxDesireability;		// Maximum desireability this item will ever have.
var() localized string						PickupMessage;			// Human readable description when picked up.
var() SoundCue PickupSound;
var() string PickupForce;
var			class<DroppedPickup>			DroppedPickupClass;
var			PrimitiveComponent				DroppedPickupMesh;
var			PrimitiveComponent				PickupFactoryMesh;
var			ParticleSystemComponent			DroppedPickupParticles;

cpptext
{
	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

// Network replication.
replication
{
	// Things the server should send to the client.
	if ( (Role==ROLE_Authority) && bNetDirty && bNetOwner )
		Inventory, InvManager;
}

simulated function String GetHumanReadableName()
{
	return Default.ItemName;
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
 * desireability of acquiring that pickup.  The BotDesireability() method returns a
 * float typically between 0 and 1 describing how valuable the pickup is to the
 * AIController.  This method is called when an AIController uses the
 * FindPathToBestInventory() navigation intrinsic.
 * @param PickupHolder - Actor in the world that holds the inventory item (usually DroppedPickup or PickupFactory)
 * @param P - the Pawn the AI is evaluating this item for
 * @param C - the Controller that is evaluating this item. Might not be P.Controller - the AI may choose to
 * 		evaluate the usability of the item by the driver Pawn of a vehicle it is currently controlling, for example
 */
static function float BotDesireability(Actor PickupHolder, Pawn P, Controller C)
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

	if (PickupSound != None)
	{
		Other.PlaySound( PickupSound );
	}
}

/**
 * This Inventory Item has just been given to this Pawn
 * (server only)
 *
 * @param	thisPawn			new Inventory owner
 * @param	bDoNotActivate		If true, this item will not try to activate
 */
function GivenTo( Pawn thisPawn, optional bool bDoNotActivate )
{
	`LogInv(thisPawn @ "Weapon:" @ Self);
	Instigator = ThisPawn;
	ClientGivenTo(thisPawn, bDoNotActivate);
}

/**
 * This Inventory Item has just been given to this Pawn
 * (owning client only)
 *
 * @param	thisPawn			new Inventory owner
 * @param	bDoNotActivate		If true, this item will not try to activate
 */
reliable client function ClientGivenTo(Pawn NewOwner, bool bDoNotActivate)
{
	// make sure Owner is set - if Inventory item fluctuates Owners there is a chance this might not get updated normally
	SetOwner(NewOwner);
	Instigator = NewOwner;

	`LogInv(NewOwner @ "Weapon:" @ Self);

	if( NewOwner != None && NewOwner.Controller != None )
	{
		NewOwner.Controller.NotifyAddInventory(Self);
	}
}

/**
 * Event called when Item is removed from Inventory Manager.
 * Network: Authority
 */
function ItemRemovedFromInvManager();


/** DenyPickupQuery
	Function which lets existing items in a pawn's inventory
	prevent the pawn from picking something up.
 * @param ItemClass Class of Inventory our Owner is trying to pick up
 * @param Pickup the Actor containing that item (this may be a PickupFactory or it may be a DroppedPickup)
 * @return true to abort pickup or if item handles pickup
 */
function bool DenyPickupQuery(class<Inventory> ItemClass, Actor Pickup)
{
	// By default, you can only carry a single item of a given class.
	if ( ItemClass == class )
	{
		return true;
	}

	return false;
}


/**
 * Drop this item out in to the world
 *
 * @param	StartLocation 		- The World Location to drop this item from
 * @param	StartVelocity		- The initial velocity for the item when dropped
 */
function DropFrom(vector StartLocation, vector StartVelocity)
{
	local DroppedPickup P;

	if( Instigator != None && Instigator.InvManager != None )
	{
		Instigator.InvManager.RemoveFromInventory(Self);
	}

	// if cannot spawn a pickup, then destroy and quit
	if( DroppedPickupClass == None || DroppedPickupMesh == None )
	{
		Destroy();
		return;
	}

	P = Spawn(DroppedPickupClass,,, StartLocation);
	if( P == None )
	{
		Destroy();
		return;
	}

	P.SetPhysics(PHYS_Falling);
	P.Inventory	= self;
	P.InventoryClass = class;
	P.Velocity = StartVelocity;
	P.Instigator = Instigator;
	P.SetPickupMesh(DroppedPickupMesh);
	P.SetPickupParticles(DroppedPickupParticles);

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

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Actor'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Inventory"
	End Object
	Components.Add(Sprite)

	bOnlyDirtyReplication=true
	bOnlyRelevantToOwner=true
	NetPriority=1.4
	bHidden=true
	Physics=PHYS_None
	bReplicateMovement=false
	RemoteRole=ROLE_SimulatedProxy
	DroppedPickupClass=class'DroppedPickup'
	MaxDesireability=0.1000
}
