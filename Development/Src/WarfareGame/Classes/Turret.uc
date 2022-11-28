//=============================================================================
// Turret: The base class of all turrets.
//=============================================================================
// This is a built-in Unreal Engine class and it shouldn't be modified.
//=============================================================================

class Turret extends Vehicle
	config(Game)
	abstract;

/** Default inventory added via AddDefaultInventory() */
var config array<class<Inventory> >	DefaultInventory;

var Rotator	DesiredAim;

var()	Vector	CannonFireOffset;	// from pitch bone, aligned to DesiredAim
var		name	PitchBone;
var		name	BaseBone;

struct sPointOfView
{
	var()	vector	DirOffset;		// BaseRotation relative Offset
	var()	float	Distance;		// relative offset dist
	var()	float	fZAdjust;		// Adjust Z based on pitch
};

var() sPointOfView	POV;	// Camera info

replication
{
	unreliable if( !bNetOwner && bNetDirty && Role==ROLE_Authority )
        DesiredAim;
}

function DriverEnter(Pawn P)
{
	// We don't have pre-defined exit positions here, so we use the original player location as an exit point
	if( !bRelativeExitPos )
	{
		ExitPositions[0] =  P.Location + Vect(0,0,16);
	}

	super.DriverEnter( P );
}

/**
 * Overridden to iterate through the DefaultInventory array and
 * give each item to this Pawn.
 * 
 * @see			GameInfo.AddDefaultInventory
 */
function AddDefaultInventory()
{
	local int		i;
	local Inventory	Inv;
	local WarPC wfPC;
	wfPC = WarPC(Controller);
	if (wfPC == None)
	{
		for (i=0; i<DefaultInventory.Length; i++)
		{
			// Ensure we don't give duplicate items
			if (FindInventoryType( DefaultInventory[i] ) == None)
			{
				Inv = CreateInventory( DefaultInventory[i] );
				if (Weapon(Inv) != None)
				{
					// don't allow default weapon to be thrown out
					Weapon(Inv).bCanThrow = false;
				}
			}
		}
	}
	else
	{
		CreateInventory(WarPC(Controller).SecondaryWeaponClass);
		CreateInventory(WarPC(Controller).PrimaryWeaponClass);
	}
}

function UpdateRocketAcceleration(float deltaTime, float YawChange, float PitchChange)
{
	local rotator DeltaRot;

	DeltaRot.Yaw	+= YawChange;
	DeltaRot.Pitch	+= PitchChange;

	ProcessViewRotation( deltaTime, DesiredAim, DeltaRot );
	Controller.SetRotation(DesiredAim);
}

simulated function Tick( float DeltaTime )
{
	local Rotator BaseRotation, CannonRotation;

	// Control bones rotation, to match desired aim
	if ( Mesh != None )
	{
		// first set base rotation (only Yaw)
		BaseRotation.Pitch	= 0;
		BaseRotation.Yaw	= DesiredAim.Yaw;
		BaseRotation.Roll	= 0;
		Mesh.SetBoneRotation(BaseBone, BaseRotation);

		// set cannon rotation: only pitch, as Yaw is inherited from parent bone.
		CannonRotation.Pitch	= -1 * DesiredAim.Pitch;
		CannonRotation.Yaw		= 0;
		CannonRotation.Roll		= 0;
		Mesh.SetBoneRotation(PitchBone, CannonRotation);
	}
}

/**
 *	Calculate camera view point, when viewing this pawn.
 *
 * @param	fDeltaTime	delta time seconds since last update
 * @param	out_CamLoc	Camera Location
 * @param	out_CamRot	Camera Rotation
 * @param	out_FOV		Field of View
 *
 * @return	true if Pawn should provide the camera point of view.
 */
simulated function bool PawnCalcCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
{
	local Rotator	TempRotation, AimRot;
	local vector	CamLookAt, HitLocation, HitNormal;
	local vector	dirX, dirY, dirZ;
	local float		AimAngle;

	AimRot = GetTurretAimDir();
	//
	// Figure out cam location
	//
	CamLookAt = Mesh.GetBoneLocation( PitchBone ) + vector(AimRot) * 2048;

	// Base Direction (Only YAW)
	TempRotation.Pitch	= 0;
	TempRotation.Yaw	= AimRot.Yaw;
	TempRotation.Roll	= 0;
	GetAxes(TempRotation, dirX, dirY, dirZ);
	out_CamLoc = Location + POV.Distance * (dirX*POV.DirOffset.X + dirY*POV.DirOffset.Y + dirZ*POV.DirOffset.Z);

	AimAngle = float(AimRot.Pitch & 65535);	// angle the controller is aiming at
	// bring this value between +32767 and -32768
	if ( AimAngle >= 32768 )
	{
		AimAngle = AimAngle - 65536;
	}

	// Z Adjust based on pitch
	out_CamLoc = out_CamLoc + vect(0,0,1) * POV.fZAdjust * (AimAngle / FMax(-ViewPitchMin,ViewPitchMax));

	if ( Trace( HitLocation, HitNormal, out_CamLoc, Location, false, vect(10, 10, 10) ) != None )
	{
		out_CamLoc = HitLocation + HitNormal * 10;
	}

	//
	// rotate camera to match focus point
	//
	out_CamRot = Rotator(CamLookAt-out_CamLoc);
	return true;
}

/** Physical fire start location. (from weapon's barrel in world coordinates) */
simulated function vector GetPhysicalFireStartLoc( vector FireOffset )
{
	local Vector	X, Y, Z;
	local Rotator	AimDir;

	AimDir = GetTurretAimDir();
	GetAxes(AimDir, X, Y, Z);
	return (Mesh.GetBoneLocation( PitchBone ) + CannonFireOffset.X*X + CannonFireOffset.Y*Y + CannonFireOffset.Z*Z);
}

/**
 * returns base Aim Rotation without any adjustment (no aim error, no autolock, no adhesion.. just clean initial aim rotation!)
 *
 * @return	base Aim rotation.
 */
simulated function Rotator GetBaseAimRotation()
{
	local vector	POVLoc;
	local rotator	POVRot;

	// If we have a controller, by default we aim at the player's 'eyes' direction
	// that is by default Controller.Rotation for AI, and camera (crosshair) rotation for human players.
	if( Controller != None && !InFreeCam() )
	{
		Controller.GetPlayerViewPoint( POVLoc, POVRot );
		return POVRot;
	}

	// If we have no controller, we simply use our rotation
	POVRot = GetTurretAimDir();

	// If our Pitch is 0, then use RemoveViewPitch
	if ( POVRot.Pitch == 0 )
	{
		POVRot.Pitch = RemoteViewPitch << 8;
	}

	return POVRot;
}

/** returns Aim direction of turret */
simulated function rotator GetTurretAimDir()
{
	return (Rotation+DesiredAim);
}

simulated function name GetDefaultCameraMode( PlayerController RequestedBy )
{
	return 'Default';
}

defaultproperties
{
	LandMovementState=PlayerTurreting
	ViewPitchMin=-4096
	ViewPitchMax=8192
	POV=(DirOffset=(X=-5,Y=0,Z=4),Distance=200,fZAdjust=-350)
}