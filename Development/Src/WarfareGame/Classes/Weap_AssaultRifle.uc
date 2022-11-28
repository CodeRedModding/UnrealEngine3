/** 
 * COG AR-15 Assault Rifle
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Weap_AssaultRifle extends WarWeapon;

/** counts shots for burst fire */
var		byte		BurstCount;
/** Max Shots fire for Burst */
var()	byte		MaxBurstCount;
/** Geist fire sound */
var()	SoundCue	GeistFireSound;

/**
 * Spawn tracer effect for instant hit shots
 *
 * @param HitLocation	Location instant trace did hit
 */
function SpawnTracerEffect( vector HitLocation )
{
	local vector		SpawnLoc;
	local Projectile	SpawnedProjectile;

	SpawnLoc = GetPhysicalFireStartLoc();
	SpawnedProjectile = Spawn(class'Proj_BulletTracer',,, SpawnLoc);
	if ( SpawnedProjectile != None )
	{
		SpawnedProjectile.Init( Normal(HitLocation-SpawnLoc) );
	}
}

/** @see Weapon::ProcessInstantHit */
function ProcessInstantHit( Actor HitActor, vector AimDir, Vector HitLocation, Vector HitNormal, TraceHitInfo HitInfo )
{
	super.ProcessInstantHit( HitActor, AimDir, HitLocation, HitNormal, HitInfo );

	// don't spawn bullet impacts on pawns.
	// skip if spawning a tracer, because that will spawn an effect client side.
	if ( Pawn(HitActor) == None && !ShouldSpawnTracerFX() )
	{
		Spawn( class'Emit_BulletImpact',,, HitLocation + HitNormal*3, rotator(HitNormal) );
	}
}

/**
 * Start muzzle flash effect
 * Network: Local Player and Clients
 *
 * @param	fDuration: length of muzzle flash effect
 */
simulated function DoMuzzleFlashEffect( float fDuration )
{
	local Vector	V;
	Local Rotator	R;

	super.DoMuzzleFlashEffect( fDuration );

	// Randomize muzzle flash mesh size
	V.X = MuzzleFlashTransform.default.Scale3D.X * ( 0.9 + 0.2*FRand() );
    V.Y = MuzzleFlashTransform.default.Scale3D.Y * ( 0.9 + 0.2*FRand() );
    V.Z = MuzzleFlashTransform.default.Scale3D.Z * ( 0.9 + 0.2*FRand() );
	MuzzleFlashTransform.SetScale3D( V );
    
	// randomize muzzle flash mesh roll angle
	R.Roll = Rand(65535);
    MuzzleFlashTransform.SetRotation( R );
}

/** Auto fire state */
state Firing
{
	/** @see WarWeapon::ShouldSpawnTracerFX */
	function bool ShouldSpawnTracerFX()
	{
		// spawn tracer every N consecutive shots.
		if ( ConsShotCount % 2 == 0 )
		{
			return true;
		}
		return false;
	}
}

/** 
 * State SingleShotFiring
 * Fires a bullet at a time
 */
state SingleShotFiring extends Firing
{
	/** @see WarWeapon::ShouldSpawnTracerFX */
	function bool ShouldSpawnTracerFX()
	{
		// tracer for every single shot
		return true;
	}

	/** @see WarWeapon::ShouldRefire() */
	simulated function bool ShouldRefire( byte F )
	{
		// in single fire more, it is not possible to refire. You have to release and re-press fire to shot every time
		return false;
	}
}

/** 
 * State BurstFiring
 * Fires MaxBurstCount bullets in row (providing we have enough ammunition)
 */
state BurstFiring extends Firing
{
	/** @see Weapon::LocalFire */
	simulated function LocalFire( byte FireModeNum )
	{
		BurstCount++;
		super.LocalFire( FireModeNum );
	}

	/** @see WarWeapon::ShouldSpawnTracerFX */
	function bool ShouldSpawnTracerFX()
	{
		// spawn tracer every N consecutive shots.
		if ( ConsShotCount % 2 == 0 )
		{
			return true;
		}
		return false;
	}

	/** @see WarWeapon::ShouldRefire() */
	simulated function bool ShouldRefire( byte FireModeNum )
	{
		// If we have enough ammo, we fire MaxBurstCount shots in a row.
		if ( HasAmmo( FireModeNum ) && BurstCount < MaxBurstCount )
			return true;
		
		return false;
	}

	simulated function EndState()
	{
		super.EndState();
		BurstCount = 0;
	}
}

/** @see AWarWeapon::PlayFireEffects */
simulated function PlayWeaponFireEffects( byte FireModeNum )
{
	// color depending on team
	if ( Instigator.GetTeamNum() == 1 )
	{
		FireSound = default.GeistFireSound;
	}
	else
	{
		FireSound = default.FireSound;
	}

	super.PlayWeaponFireEffects( FireModeNum );
}

defaultproperties
{
	FireOffset=(X=-70,Y=3,Z=15.5)
	MaxBurstCount=6
	MagazineSize=30
	CriticalAmmoCount=6
	ReloadDuration=2.5
	ItemName="STL-4 'Lancer' Assault Rifle"
	WeaponFireAnim=RifleShoot
	FireSound=SoundCue'COG_AssaultRifle.COGAssaultRifleAmmo_SoundCue'
	GeistFireSound=SoundCue'COG_AssaultRifle.Geist_RifleFire_Soundcue'
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(Damage=15,FireRate=600,Recoil=200,Inaccuracy=3,Name="Auto")
	FiringStatesArray(1)=BurstFiring
	FireModeInfoArray(1)=(Damage=20,FireRate=720,Recoil=200,Inaccuracy=6,Name="Burst")
	FiringStatesArray(2)=SingleShotFiring
	FireModeInfoArray(2)=(Damage=25,FireRate=320,Recoil=200,Inaccuracy=2,Name="Single")
	SelectedFireMode=1 // start with burst

	// Weapon Animation
	Begin Object Class=AnimNodeSequence Name=aWeaponIdleAnim
    	AnimSeqName=RifleShoot
	End Object

	// Weapon SkeletalMesh
	Begin Object Class=StaticMeshComponent Name=RifleMesh
		CollideActors=false
		StaticMesh=StaticMesh'COG_AssaultRifle_Final.COG_AssaultRifle_PH_SMesh'
	End Object
    Mesh=RifleMesh

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=RifleMesh
		Rotation=(Yaw=16384)
		Translation=(X=-22.0,Y=3.0,Z=6.0)
		Scale=0.8
    End Object
	MeshTranform=TransformComponentMesh0

    // Muzzle Flash StaticMesh
    Begin Object Class=StaticMeshComponent Name=MuzFlashSMCpnt
    	StaticMesh=StaticMesh'COG_AssaultRifle.Mesh.Cog_MuzzleFlash1_Smesh'
        HiddenGame=true
        CastShadow=false
		CollideActors=false
		AlwaysLoadOnClient=true
    End Object
    MuzzleFlashMesh=MuzFlashSMCpnt

	// Muzzle Flash Transform component
    Begin Object Class=TransformComponent Name=TransformComponent0
    	TransformedComponent=MuzFlashSMCpnt
		Translation=(X=-70,Y=2.5,Z=11.0)
    End Object
    MuzzleFlashTransform=TransformComponent0

	// Muzzle Flash point light
    Begin Object Class=PointLightComponent Name=LightComponent0
		Brightness=0
        Color=(R=192,G=180,B=144,A=255)
        Radius=256
    End Object
    MuzzleFlashLight=LightComponent0

	// Muzzle Flash point light Transform component
    Begin Object Class=TransformComponent Name=PointLightTransformComponent0
    	TransformedComponent=LightComponent0
		Translation=(X=-70,Y=2.5,Z=11.0)
    End Object
    MuzzleFlashLightTransform=PointLightTransformComponent0
}