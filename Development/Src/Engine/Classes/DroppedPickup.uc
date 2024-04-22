//=============================================================================
// Pickup items.
//
// PickupFactory should be used to place items in the level.  This class is for dropped inventory, which should attach
// itself to this pickup, and set the appropriate mesh
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class DroppedPickup extends Actor
	notplaceable
	native;

//-----------------------------------------------------------------------------
// AI related info.
var				Inventory					Inventory;			// the dropped inventory item which spawned this pickup
var	repnotify	class<Inventory>			InventoryClass;		// Class of the inventory object to pickup
var				NavigationPoint				PickupCache;		// navigationpoint this pickup is attached to
var	repnotify	bool						bFadeOut;

native final function AddToNavigation();			// cache dropped inventory in navigation network
native final function RemoveFromNavigation();

replication
{
	if( Role==ROLE_Authority )
		InventoryClass, bFadeOut;
}

event Destroyed()
{
	if (Inventory != None )
		Inventory.Destroy();
}

simulated event ReplicatedEvent(name VarName)
{
	if( VarName == 'InventoryClass' )
	{
		SetPickupMesh( InventoryClass.default.DroppedPickupMesh );
		SetPickupParticles( InventoryClass.default.DroppedPickupParticles );
	}
	else if ( VarName == 'bFadeOut' )
	{
		GotoState('Fadeout');
	}
	else
	{
		super.ReplicatedEvent(VarName);
	}
}
/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	Destroy();
}

/**
 * Set Pickup mesh to use.
 * Replicated through InventoryClass to remote clients using Inventory.DroppedPickup component as default mesh.
 */
simulated event SetPickupMesh(PrimitiveComponent PickupMesh)
{
	local ActorComponent Comp;

	if (PickupMesh != None && WorldInfo.NetMode != NM_DedicatedServer )
	{
		Comp = new(self) PickupMesh.Class(PickupMesh);
		AttachComponent(Comp);
	}
}

/**
 * Set Pickup particles to use.
 * Replicated through InventoryClass to remote clients using Inventory.DroppedPickup component as default mesh.
 */
simulated event SetPickupParticles(ParticleSystemComponent PickupParticles)
{
	local ParticleSystemComponent Comp;

	if (PickupParticles != None && WorldInfo.NetMode != NM_DedicatedServer )
	{
		Comp = new(self) PickupParticles.Class(PickupParticles);
		AttachComponent(Comp);
		Comp.SetActive(true);
	}
}

event EncroachedBy(Actor Other)
{
	Destroy();
}

/* DetourWeight()
value of this path to take a quick detour (usually 0, used when on route to distant objective, but want to grab inventory for example)
*/
function float DetourWeight(Pawn Other,float PathWeight)
{
	return Inventory.DetourWeight(Other, PathWeight);
}

event Landed(Vector HitNormal, Actor FloorActor)
{
	// force full net update
	bForceNetUpdate = TRUE;
	bNetDirty = true;
	// reduce frequency since the pickup isn't moving anymore
	NetUpdateFrequency = 3;

	AddToNavigation();
}

/** give pickup to player */
function GiveTo( Pawn P )
{
	if( Inventory != None )
	{
		Inventory.AnnouncePickup(P);
		Inventory.GiveTo(P);
		Inventory = None;
	}
	PickedUpBy(P);
}

function PickedUpBy(Pawn P)
{
	Destroy();
}

function RecheckValidTouch();

//=============================================================================
// Pickup state: this inventory item is sitting on the ground.

auto state Pickup
{
	/*
	 Validate touch (if valid return true to let other pick me up and trigger event).
	*/
	function bool ValidTouch(Pawn Other)
	{
		// make sure its a live player
		if (Other == None || !Other.bCanPickupInventory || (Other.DrivenVehicle == None && Other.Controller == None))
		{
			return false;
		}

		// make sure thrower doesn't run over own weapon
		if ( (Physics == PHYS_Falling) && (Other == Instigator) && (Velocity.Z > 0) )
		{
			return false;
		}

		// make sure not touching through wall
		if ( !FastTrace(Other.Location, Location) )
		{
			SetTimer( 0.5, false, nameof(RecheckValidTouch) );
			return false;
		}

		// make sure game will let player pick me up
		if (WorldInfo.Game.PickupQuery(Other, Inventory.class, self))
		{
			return true;
		}
		return false;
	}

	/**
	Pickup was touched through a wall.  Check to see if touching pawn is no longer obstructed
	*/
	function RecheckValidTouch()
	{
		CheckTouching();
	}

	// When touched by an actor.
	event Touch( Actor Other, PrimitiveComponent OtherComp, vector HitLocation, vector HitNormal )
	{
		local Pawn P;

		// If touched by a player pawn, let him pick this up.
		P = Pawn(Other);
		if( P != None && ValidTouch(P) )
		{
			GiveTo(P);
		}
	}

	event Timer()
	{
		GotoState('FadeOut');
	}

	function CheckTouching()
	{
		local Pawn P;

		foreach TouchingActors(class'Pawn', P)
		{
			Touch( P, None, Location, vect(0,0,1) );
		}
	}

	event BeginState(Name PreviousStateName)
	{
		AddToNavigation();
		if( LifeSpan > 0.f )
		{
			SetTimer(LifeSpan - 1, false);
		}
	}

	event EndState(Name NextStateName)
	{
		RemoveFromNavigation();
	}

Begin:
		CheckTouching();
}

State FadeOut extends Pickup
{
	simulated event BeginState(Name PreviousStateName)
	{
		bFadeOut = true;
		RotationRate.Yaw=60000;
		SetPhysics(PHYS_Rotating);
		LifeSpan = 1.0;
	}
}

defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_Inventory'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Inventory"
	End Object
	Components.Add(Sprite)

	Begin Object Class=CylinderComponent NAME=CollisionCylinder
		CollisionRadius=+00030.000000
		CollisionHeight=+00020.000000
		CollideActors=true
	End Object
	CollisionComponent=CollisionCylinder
	Components.Add(CollisionCylinder)


	bOnlyDirtyReplication=true
	NetUpdateFrequency=8
	RemoteRole=ROLE_SimulatedProxy
	bHidden=false
	NetPriority=+1.4
	bCollideActors=true
	bCollideWorld=true
	RotationRate=(Yaw=5000)

	bOrientOnSlope=true
	bShouldBaseAtStartup=true
	bIgnoreEncroachers=false
	bIgnoreRigidBodyPawns=true
	bUpdateSimulatedPosition=true
	LifeSpan=+16.0
}
