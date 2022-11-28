class WarActorFactoryAI extends ActorFactory
	native;

cpptext
{
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);
};

/**
 * Type of AI to spawn.
 */
enum ESpawnType
{
	LOCUST_Attack,
	LOCUST_Defend,
	GEAR_Follow,
	GEAR_Defend
};
var() ESpawnType SpawnType;

/**
 * Name of squad to assign this AI to.
 */
var() Name SquadName;

/**
 * Contains information for a spawn set.
 */
struct native SpawnSet
{
	var() class<AIController>		ControllerClass;
	var() class<Pawn>				PawnClass;
	var() array<class<Inventory> >	InventoryList;
	var() int						TeamIndex;
};

/** List of all known spawn sets, indexed by ESpawnType. */
var const array<SpawnSet> SpawnSets;

/** Inventory list, exposed outside of set for easier modification */
var() array<class<Inventory> > InventoryList;

defaultproperties
{
	SpawnSets(0)={(ControllerClass=class'WarAI_LocustAttack',
				   PawnClass=class'Pawn_GeistLocust',
				   InventoryList=(class'Weap_ScorchRifle'),
				   TeamIndex=1)}
	SpawnSets(1)={(ControllerClass=class'WarAI_LocustDefend',
				   PawnClass=class'Pawn_GeistLocust',
				   InventoryList=(class'Weap_ScorchRifle'),
				   TeamIndex=1)}
	SpawnSets(2)={(ControllerClass=class'WarAI_GearFollow',
				   PawnClass=class'Pawn_COGGear',
				   InventoryList=(class'Weap_AssaultRifle'),
				   TeamIndex=0)}
	SpawnSets(3)={(ControllerClass=class'WarAI_GearDefend',
				   PawnClass=class'Pawn_COGGear',
				   InventoryList=(class'Weap_AssaultRifle'),
				   TeamIndex=0)}

	SquadName=Alpha
}
