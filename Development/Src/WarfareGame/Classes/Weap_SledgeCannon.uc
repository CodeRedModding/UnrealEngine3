/** 
 * COG Sledge Cannon
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Weap_SledgeCannon extends WarWeapon;

/** Ramped Up FireRate */
var()	config	float	RampedUpFireRate;

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

/** @see Weapon::StartFire */
simulated function StartFire( byte FireModeNum )
{
	// Make sure player can fire weapon
	if( CanInstigatorFireWeapon() )
	{
		super.StartFire( FireModeNum );
	}
}

/** Checks if the instigator is able to fire the weapon (ie Human Player must be in targeting mode) */
simulated function bool CanInstigatorFireWeapon()
{
	if( Instigator == None || Instigator.Controller == None )
	{
		return false;
	}

	// human player has to be in Targeting Mode to start firing this weapon
	if( WarPC(Instigator.Controller) != None && !WarPC(Instigator.Controller).bTargetingMode )
	{
		return false;
	}

	return true;
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

	/** @see WarWeapon::ShouldRefire() */
	simulated function bool ShouldRefire( byte F )
	{
		// Make sure Instigator is able to fire weapon (in targeting mode)
		if( !CanInstigatorFireWeapon() )
		{
			return false;
		}

		return super.ShouldRefire( F );
	}
}

/** 
 * SpeedUp fire mode.
 * Rate of Fire increases over time while firing
 */
state RampedUpFiring extends Firing
{
	/** @see WarWeapon::GetFireModeRateOfFire */
	simulated function float GetFireModeRateOfFire( byte FireModeNum )
	{
		local float		Pct;

		// Ramp up rate of fire
		Pct = float(ConsShotCount) / float(MagazineSize);
		return Lerp(Pct, FireModeInfoArray[FireModeNum].FireRate, RampedUpFireRate);
	}
}


defaultproperties
{
	FireOffset=(X=-60,Y=3,Z=13)
	MagazineSize=60
	CriticalAmmoCount=10
	ReloadDuration=4.5
	ItemName="Sledge Cannon"
	WeaponFireAnim=RifleShoot
	FireSound=SoundCue'COGAssaultRifleAmmo_SoundCue'
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(Damage=15,FireRate=360,Recoil=200,Inaccuracy=3,Name="Standard")
	FiringStatesArray(1)=RampedUpFiring
	FireModeInfoArray(1)=(Damage=15,FireRate=360,Recoil=200,Inaccuracy=3,Name="Speed Up")

	RampedUpFireRate=1200

	// Weapon Animation
	Begin Object Class=AnimNodeSequence Name=aWeaponIdleAnim
    	AnimSeqName=RifleShoot
	End Object

	// Weapon SkeletalMesh
	Begin Object Class=SkeletalMeshComponent Name=SkeletalMeshComponent0
		SkeletalMesh=SkeletalMesh'COG_AssaultRifle.COG_AssaultRifle_AMesh'
		Animations=aWeaponIdleAnim
		AnimSets(0)=AnimSet'COG_AssaultRifle.COG_AssaultRifleAnimSet'
		CollideActors=false
	End Object
    Mesh=SkeletalMeshComponent0

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=SkeletalMeshComponent0
		Translation=(X=-9.0,Y=3.0,Z=3.50)
		Scale3D=(X=0.5,Y=0.5,Z=0.5)
    End Object
	MeshTranform=TransformComponentMesh0

    // Muzzle Flash StaticMesh
    Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
    	StaticMesh=StaticMesh'Cog_MuzzleFlash1_Smesh'
        HiddenGame=true
        CastShadow=false
		CollideActors=false
		AlwaysLoadOnClient=true
    End Object
    MuzzleFlashMesh=StaticMeshComponent0

	// Muzzle Flash Transform component
    Begin Object Class=TransformComponent Name=TransformComponent0
    	TransformedComponent=StaticMeshComponent0
		Translation=(X=-60,Y=2.5,Z=8.50)
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
		Translation=(X=-60,Y=2.5,Z=8.50)
    End Object
    MuzzleFlashLightTransform=PointLightTransformComponent0

	HUDElements(0)=(bEnabled=True,DrawX=0.045000,DrawY=0.024000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=128),DrawW=256,DrawH=128,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Weapon_T_AssaultRifleIcon',DrawU=0,DrawV=0,DrawUSize=256,DrawVSize=128,DrawScale=0.700000,DrawFont=None,DrawLabel="")
	HUDElements(1)=(bEnabled=True,DrawX=0.145000,DrawY=0.125000,bCenterX=False,bCenterY=False,DrawColor=(B=200,G=200,R=200,A=128),DrawW=0,DrawH=0,DrawIcon=None,DrawU=0,DrawV=0,DrawUSize=-1,DrawVSize=-1,DrawScale=1.000000,DrawFont=Font'EngineFonts.TinyFont',DrawLabel="AUTO")
}