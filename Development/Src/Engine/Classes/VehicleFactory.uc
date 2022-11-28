
class VehicleFactory extends Actor
	placeable;

var()	bool			bSpawnAtStartup;
var()	class<Vehicle>	VehicleClass;
var		Vehicle			Child;
var()	float			fSpawnDelay;

function PostBeginPlay()
{
	if ( bSpawnAtStartup )
		SpawnVehicle();
}

function SpawnVehicle()
{
	SetTimer(fSpawnDelay, false);
}

function Timer()
{
	Child = Spawn(VehicleClass,,, Location, Rotation);
	if ( Child == None )
		log("Couldn't Spawn Vehicle" @ VehicleClass );
}

function Reset()
{
	// Force vehicle destruction at level reset
	if ( Child != None )
	{
		Child.Destroy();
		Child = None;
	}

	// Spawn at startup
	if ( bSpawnAtStartup )
		SpawnVehicle();
}

defaultproperties
{
	fSpawnDelay=0.1
	bSpawnAtStartup=true

	Begin Object Class=ArrowComponent Name=ArrowComponent0
		HiddenGame=true
	End Object
	Components.Add(ArrowComponent0)
}
