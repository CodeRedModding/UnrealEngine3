/** 
 * COG Scorch Rifle
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Weap_ScorchRifle extends WarWeapon;

/** Beam Range in unreal units*/
var()	float	fBeamRange;

var()	int		BeamDamage;
var()	float	BeamMomentumTransfer;

/** geist fire sound version */
var()	SoundCue	GeistFireSound;

/** beam effect */
var()	FX_ScorchBeam	Beam;

/** muzzle light color */
var Color	BlueColor, RedColor;

State Firing
{
	/** @see Weapon::FireAmmunition */
	function FireAmmunition( byte FireModeNum )
	{
		ProjectileFire();
	}

	/** @see Weapon::GetProjectileClass */
	simulated function class<Projectile> GetProjectileClass()
	{
		// color depending on team
		if ( Instigator.GetTeamNum() == 1 )
		{
			return class'Proj_ScorchLaserRed';
		}
		return class'Proj_ScorchLaser';
	}
}


/**
 * function called to abort the beam firing state
 */
simulated function AbortBeamFiring()
{
	GotoState( 'Active' );
}

/** @see AWarWeapon::PlayFireEffects */
simulated function PlayWeaponFireEffects( byte FireModeNum )
{
	// disable effects for beam firing
	if ( FireModeNum == 1 )
	{
		return;
	}

	// color depending on team
	if ( Instigator.GetTeamNum() == 1 )
	{
		MuzzleFlashLight.Color = RedColor;
		FireSound = default.GeistFireSound;
	}
	else
	{
		MuzzleFlashLight.Color = BlueColor;
		FireSound = default.FireSound;
	}

	super.PlayWeaponFireEffects( FireModeNum );
}

State BeamFiring extends Firing
{
	/** @see Weapon::FireAmmunition */
	function FireAmmunition( byte FireModeNum )
	{
		TraceFire();
	}

	/** @see Weapon::ProcessInstantHit */
	function ProcessInstantHit( Actor HitActor, vector AimDir, Vector HitLocation, Vector HitNormal, TraceHitInfo HitInfo )
	{
		super.ProcessInstantHit( HitActor, AimDir, HitLocation, HitNormal, HitInfo );
		if ( Pawn(HitActor) == None )
		{
			Spawn( class'Emit_BulletImpact',,, HitLocation + HitNormal*3, rotator(HitNormal) );
		}
	}

	/**
	 * returns the damage done by the current fire mode
	 */
	function float GetFireModeDamage()
	{
		return BeamDamage;
	}

	/** @see Weapon::GetTraceRange */
	simulated function float GetTraceRange()
	{
		return fBeamRange;
	}

	simulated function StopFire( byte FireModeNum )
	{
		if ( Role < Role_Authority )
		{
			ServerStopFire( FireModeNum );
		}
		AbortBeamFiring();
	}

	function ServerStopFire( byte FireModeNum )
	{
		AbortBeamFiring();
	}

	simulated function BeginState()
	{
		super.BeginState();

		if ( Role == Role_Authority && (Beam == None) )
		{		
			Beam = Spawn(class'FX_ScorchBeam');
		}

		LocalFire( SelectedFireMode );
	}

	simulated function EndState()
	{
		super.EndState();

		if ( Role == Role_Authority && Beam != None )
		{
			Beam.Destroy();
			Beam = None;
		}
	}
}

defaultproperties
{
	FireSound=SoundCue'LD_TempUTSounds.ScorchPrimary'
	GeistFireSound=SoundCue'COG_LaserRifle.GeistScorchFireSoundCue'

	BeamDamage=10
	BeamMomentumTransfer=200
	fBeamRange=630			// this is roughly 8 meters, 1UU = 1.27cm
	ReloadDuration=3.0
	MagazineSize=32
	FireOffset=(X=-80,Y=3,Z=13)
	CriticalAmmoCount=8

	ItemName="Scorch Rifle"
	WeaponFireAnim=Shoot
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(FireRate=540,Recoil=200,Inaccuracy=3,Name="Auto")
	FiringStatesArray(1)=BeamFiring
	FireModeInfoArray(1)=(FireRate=540,Name="Beam")

	// Weapon Animation
	Begin Object Class=AnimNodeSequence Name=aWeaponIdleAnim
    	AnimSeqName=Idle
	End Object

	// Weapon SkeletalMesh
	Begin Object Class=SkeletalMeshComponent Name=SkeletalMeshComponent0
		SkeletalMesh=SkeletalMesh'COG_LaserRifle.COG_LaserRifle_AMesh'
		Animations=aWeaponIdleAnim
		AnimSets(0)=AnimSet'COG_LaserRifle.COG_LaserRifleAnimSet'
		CollideActors=false
	End Object
    Mesh=SkeletalMeshComponent0

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh0
    	TransformedComponent=SkeletalMeshComponent0
		Translation=(X=-9.0,Y=3.0,Z=3.50)
		Scale3D=(X=0.57,Y=0.57,Z=0.57)
		Rotation=(Pitch=0,Roll=0,Yaw=-32768)
    End Object
	MeshTranform=TransformComponentMesh0

	// Muzzle Flash point light
    Begin Object Class=PointLightComponent Name=LightComponent0
		Brightness=0
        Color=(R=64,G=160,B=255,A=255)
        Radius=256
    End Object
    MuzzleFlashLight=LightComponent0

	BlueColor=(R=64,G=160,B=255,A=255) 
	RedColor=(R=255,G=160,B=64,A=255) 

	// Muzzle Flash point light Transform component
    Begin Object Class=TransformComponent Name=PointLightTransformComponent0
    	TransformedComponent=LightComponent0
		Translation=(X=-100,Y=0,Z=20)
    End Object
    MuzzleFlashLightTransform=PointLightTransformComponent0

	HUDElements(0)=(bEnabled=True,DrawX=0.040000,DrawY=0.018000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=128),DrawW=256,DrawH=128,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Weapon_T_ScorchRifleIcon',DrawU=0,DrawV=0,DrawUSize=256,DrawVSize=128,DrawScale=0.700000,DrawFont=None,DrawLabel="")
	HUDElements(1)=(bEnabled=True,DrawX=0.150000,DrawY=0.060000,bCenterX=False,bCenterY=False,DrawColor=(B=200,G=200,R=200,A=128),DrawW=0,DrawH=0,DrawIcon=None,DrawU=0,DrawV=0,DrawUSize=-1,DrawVSize=-1,DrawScale=1.000000,DrawFont=Font'EngineFonts.TinyFont',DrawLabel="AUTO")
}