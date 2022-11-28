class UTWeaponPickupFactory extends UTPickupFactory;

var() class<Weapon>		WeaponPickupClass;
var   bool				bWeaponStay;
var	  float				RotationRate;

simulated function PreBeginPlay()
{
	InventoryType = WeaponPickupClass;
	SetWeaponStay();
	Super.PreBeginPlay();
}

function SetHidden()
{
	NetUpdateTime = Level.TimeSeconds - 1;
	bHidden = true;
}

function SetVisible()
{
	NetUpdateTime = Level.TimeSeconds - 1;
	bHidden = false;
}

simulated function Tick(float DeltaTime)
{
	// fixmesteve - move to C++
	local Rotator R;

	R = PickupMesh.Rotation;
	R.Yaw += DeltaTime * RotationRate;
	PickupMesh.SetRotation(R);
}

function bool CheckForErrors()
{
	if ( Super.CheckForErrors() )
		return true;

	if ( WeaponPickupClass == None )
	{
		log(self$" no weapon pickup class");
		return true;
	}

	return false;
}

function SetWeaponStay()
{
	bWeaponStay = ( bWeaponStay && UTGame(Level.Game).bWeaponStay );
}

function StartSleeping()
{
	if (!bWeaponStay)
	    GotoState('Sleeping');
}

function bool AllowRepeatPickup()
{
    return !bWeaponStay;
}

/*
*/
defaultproperties
{
	Components.Remove(Sprite)

	bWeaponStay=true
	RotationRate=32768
	bStatic=false

	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		StaticMesh=StaticMesh'S_Pickups.S_Pickups_Bases_TempSpawner'
		bOwnerNoSee=true
		CollideActors=false
	End Object

	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=StaticMeshComponent0
		Translation=(X=0.0,Y=0.0,Z=-44.0)
		Scale3D=(X=1.0,Y=1.0,Z=1.0)
    End Object
 	Components.Add(TransformComponentMesh0)
}

