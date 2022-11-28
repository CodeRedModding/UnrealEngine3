//=============================================================================
// PickupFactory.
// Produces pickups when active and touched by valid toucher
// Combines functionality of old Pickup and InventorySpot classes
// Pickup class now just used for dropped/individual items
//=============================================================================
class PickupFactory extends NavigationPoint
	abstract
	placeable
	native
	nativereplication;

var		bool						bOnlyReplicateHidden;	// only replicate changes in bPickupHidden and bHidden
var		RepNotify bool				bPickupHidden;			// Whether the pickup mesh should be hidden
var		bool						bPredictRespawns;		// high skill bots may predict respawns for this item

var		class<Inventory>			InventoryType;
var		float						RespawnEffectTime;
var		TransformComponent		PickupMesh;

cpptext
{
    virtual APickupFactory* GetAPickupFactory() { return this; }
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
}

replication
{
	// Things the server should send to the client.
	reliable if ( bNetDirty && (Role == Role_Authority) )
		bPickupHidden;
}

// FIXMESTEVE - make this native?
simulated event ReplicatedEvent(string VarName)
{
	if ( VarName ~= "bPickupHidden" )
	{
		if ( bPickupHidden )
			SetHidden();
		else
			SetVisible();
	}
}

simulated function PostBeginPlay()
{
	Super.PostBeginPlay();

	bPredictRespawns = InventoryType.Default.bPredictRespawns;

	if ( InventoryType == None )
	{
		warn("No inventory type for "$self);
		GotoState('Disabled');
		return;
	}

	SetPickupMesh();

	if ( InventoryType.Default.bDelayedSpawn )
	{
		GotoState('WaitingForMatch');
	}
}

simulated function SetPickupMesh()
{
	if ( InventoryType.Default.PickupFactoryMesh != None )
	{
		PickupMesh = TransformComponent(AddComponent(InventoryType.Default.PickupFactoryMesh, true));
	}
}

static function StaticPrecache(LevelInfo L);

/* Reset()
reset actor to initial state - used when restarting level without reloading.
*/
function Reset()
{
	if ( InventoryType.Default.bDelayedSpawn )
		GotoState('Sleeping', 'DelayedSpawn');
	else
		GotoState('Pickup');
	Super.Reset();
}

function bool CheckForErrors()
{
	local Actor HitActor;
	local vector HitLocation, HitNormal;

	HitActor = Trace(HitLocation, HitNormal,Location - Vect(0,0,10), Location,false);
	if ( HitActor == None )
	{
		log(self$" FLOATING");
		return true;
	}
	return Super.CheckForErrors();
}

//
// Set up respawn waiting if desired.
//
function SetRespawn()
{
	if( (InventoryType.Default.RespawnTime != 0) && Level.Game.ShouldRespawn(self) )
		StartSleeping();
	else
		GotoState('Disabled');
}

function StartSleeping()
{
    GotoState('Sleeping');
}

function bool IsSuperItem()
{
	return InventoryType.Default.bDelayedSpawn;
}

/* DetourWeight()
value of this path to take a quick detour (usually 0, used when on route to distant objective, but want to grab inventory for example)
*/
event float DetourWeight(Pawn Other,float PathWeight)
{
	return InventoryType.static.DetourWeight(Other,PathWeight);
}	

/* Inventory has an AI interface to allow AIControllers, such as bots, to assess the
 desireability of acquiring that pickup.  The BotDesireability() method returns a
 float typically between 0 and 1 describing how valuable the pickup is to the
 AIController.  This method is called when an AIController uses the
 FindPathToBestInventory() navigation intrinsic.
*/
function float BotDesireability( pawn Bot )
{
	return InventoryType.static.BotDesireability(Bot);
}

function SpawnCopyFor( Pawn Recipient )
{
	local Inventory Inv;

	Inv = spawn(InventoryType);
	if ( Inv != None )
	{
		Inv.GiveTo(Recipient);
		Inv.AnnouncePickup(Recipient);
	}
}

//=============================================================================
// Pickup state: this inventory item is sitting on the ground.

auto state Pickup
{
	function bool ReadyToPickup(float MaxWait)
	{
		return true;
	}

	/* ValidTouch()
	 Validate touch (if valid return true to let other pick me up and trigger event).
	*/
	function bool ValidTouch( Pawn Other )
	{
		// make sure its a live player
		if ( (Other == None) || !Other.bCanPickupInventory || (Other.DrivenVehicle == None && Other.Controller == None) )
			return false;

		// make sure not touching through wall
		if ( !FastTrace(Other.Location, Location) )
			return false;

		// make sure game will let player pick me up
		if( Level.Game.PickupQuery(Other, InventoryType) )
		{
			return true;
		}
		return false;
	}

	// When touched by an actor.
	event Touch( Actor Other, vector HitLocation, vector HitNormal )
	{
		local Pawn P;

		// If touched by a player pawn, let him pick this up.
		P = Pawn(Other);

		if( (P != None) && ValidTouch(P) )
		{
			SpawnCopyFor(Pawn(Other));
            SetRespawn();

			if ( (P.Controller != None) && (P.Controller.MoveTarget == self) )
			{
				P.Anchor = self;
				P.Controller.MoveTimer = -1.0;
			}
 		}
	}

	// Make sure no pawn already touching (while touch was disabled in sleep).
	function CheckTouching()
	{
		local Pawn P;

		ForEach TouchingActors(class'Pawn', P)
			Touch(P, Location, Normal(Location-P.Location) );
	}

Begin:
	CheckTouching();
}

//=============================================================================
// Sleeping state: Sitting hidden waiting to respawn.
function float GetRespawnTime()
{
	return InventoryType.Default.RespawnTime;
}

function RespawnEffect();

function SetHidden()
{
	NetUpdateTime = Level.TimeSeconds - 1;
	bPickupHidden = true;
	if ( PickupMesh != None && PrimitiveComponent(PickupMesh.TransformedComponent) != None )
		PrimitiveComponent(PickupMesh.TransformedComponent).SetHidden(true);
}

function SetVisible()
{
	NetUpdateTime = Level.TimeSeconds - 1;
	bPickupHidden = false;
	if ( PickupMesh != None && PrimitiveComponent(PickupMesh.TransformedComponent) != None )
		PrimitiveComponent(PickupMesh.TransformedComponent).SetHidden(true);
}

State WaitingForMatch
{
	ignores Touch;

	function MatchStarting()
	{
		GotoState( 'Sleeping', 'DelayedSpawn' );
	}

	function BeginState()
	{
		SetHidden();
	}
}

State Sleeping
{
	ignores Touch;

	function bool ReadyToPickup(float MaxWait)
	{
		return ( bPredictRespawns && (LatentFloat < MaxWait) );
	}

	function StartSleeping() {}

	function BeginState()
	{
		SetHidden();
	}

	function EndState()
	{
		SetVisible();
	}

DelayedSpawn:
	if ( Level.NetMode == NM_Standalone )
		Sleep(FMin(30, Level.Game.GameDifficulty * 8));
	else
		Sleep(30);
	Goto('Respawn');
Begin:
	Sleep( GetReSpawnTime() - RespawnEffectTime );
Respawn:
	RespawnEffect();
	Sleep(RespawnEffectTime);
    GotoState('Pickup');
}

State Disabled
{
	function float BotDesireability( pawn Bot )
	{
		return 0;
	}

	function bool ReadyToPickup(float MaxWait)
	{
		return false;
	}

	function Reset() {}
	function StartSleeping() {}

	simulated function BeginState()
	{
		bHidden = true;
		SetCollision(false,false);
	}
}

defaultproperties
{
	Begin Object NAME=CollisionCylinder
		CollisionRadius=+00040.000000
		CollisionHeight=+00080.000000
		CollideActors=true
	End Object

	bCollideWhenPlacing=False
	bHiddenEd=false
    bOnlyReplicateHidden=true
	bStatic=true
	RemoteRole=ROLE_SimulatedProxy
	bAlwaysRelevant=true
	bCollideActors=true
	bCollideWorld=false
	bBlockActors=false
	bIgnoreEncroachers=true
	bHidden=false
}
