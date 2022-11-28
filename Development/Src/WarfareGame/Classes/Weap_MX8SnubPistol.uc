/** 
 * COG MX-8 "Snub" Pistol
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Weap_MX8SnubPistol extends WarWeapon;

/** counts shots for burst fire */
var		byte	BurstCount;

/** Max Shots fire for Burst */
var()	byte	MaxBurstCount;


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
	V.X = MuzzleFlashTransform.default.Scale3D.X * 0.9 + MuzzleFlashTransform.default.Scale3D.X * 0.2 * FRand();
    V.Y = MuzzleFlashTransform.default.Scale3D.Y * 0.9 + MuzzleFlashTransform.default.Scale3D.Y * 0.2 * FRand();
    V.Z = MuzzleFlashTransform.default.Scale3D.Z * 0.9 + MuzzleFlashTransform.default.Scale3D.Z * 0.2 * FRand();
	MuzzleFlashTransform.SetScale3D( V );
    
	// randomize muzzle flash mesh roll angle
	R.Roll = Rand(65535);
    MuzzleFlashTransform.SetRotation( R );
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

defaultproperties
{
	FireOffset=(X=-40,Y=3,Z=13)
	MaxBurstCount=4
	MagazineSize=12
	ReloadDuration=2.5
	CriticalAmmoCount=4

	ItemName="MX-8 'Snub' Pistol"

	WeaponFireAnim=Shoot
	PawnFiringAnim=PSTIdFr01
	PawnIdleAnim=PSTId01
	PawnReloadAnim=PSTIdRd01

	FireSound=SoundCue'COG_Pistol.Sounds.PistolFireSound'
	FiringStatesArray(0)=SingleShotFiring
	FireModeInfoArray(0)=(Damage=10,FireRate=600,Recoil=200,Name="Single Shot")
	FiringStatesArray(1)=BurstFiring
	FireModeInfoArray(1)=(Damage=10,FireRate=600,Recoil=200,Name="Burst")

	// Weapon Animation
	Begin Object Class=AnimNodeSequence Name=aWeaponIdleAnim
    	AnimSeqName=Idle
	End Object

	// Weapon SkeletalMesh
	Begin Object Class=SkeletalMeshComponent Name=SkeletalMeshComponent0
		SkeletalMesh=SkeletalMesh'COG_Pistol.COG_Pistol_AMesh'
		Animations=aWeaponIdleAnim
		AnimSets(0)=AnimSet'COG_Pistol.COG_Pistol_AnimSet'
		CollideActors=false
	End Object
    Mesh=SkeletalMeshComponent0

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=SkeletalMeshComponent0
		Translation=(X=-9.0,Y=3.0,Z=3.50)
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
		Translation=(X=-30,Y=2.5,Z=8.50)
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

	//HUDElements.Empty
	HUDElements(0)=(bEnabled=True,DrawX=0.040000,DrawY=0.018000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=128),DrawW=256,DrawH=128,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Weapon_T_PistolIcon',DrawU=0,DrawV=0,DrawUSize=256,DrawVSize=128,DrawScale=0.700000,DrawFont=None,DrawLabel="")
}