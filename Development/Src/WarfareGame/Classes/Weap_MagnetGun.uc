/** 
 * Geist Magnet Gun
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 *
 * @todo		- add limited range. charges can be attached to infinity.
 */
class Weap_MagnetGun extends WarWeapon;

/** Charges */
var				Proj_MagnetChargePositive	PosCharge;
var				Proj_MagnetCharge			NegCharge;

var()	config	float						MagDuration;

/** Sound played when charges are activated */
var()			SoundCue					ActivationSound;

var()	config	float						MGLifeTime;


simulated function PostBeginPlay()
{
	super.PostBeginPlay();
	LifeSpan = MGLifeTime;
}

simulated function Destroyed()
{
	if( PosCharge != None )
	{
		PosCharge.Destroy();
		PosCharge = None;
	}

	if( NegCharge != None )
	{
		NegCharge.Destroy();
		NegCharge = None;
	}

	super.Destroyed();
}

/** @see Inventory::ItemRemovedFromInvManager */
function ItemRemovedFromInvManager()
{
	InvManager.SwitchToBestWeapon();
}

/** Cannot switch to another weapon */
simulated function bool DoOverridePrevWeapon()
{
	return true;
}

simulated function bool DoOverrideNextWeapon()
{
	return true;
}

/** force weapon switch */
simulated function ClientWeaponSet( bool bOptionalSet )
{
	super.ClientWeaponSet( false );
}

/** force switch to this weapon */
simulated function float GetWeaponRating()
{
	return 2.0;
}

simulated function DrawWeaponInfo( HUD H )
{
	local string	Text;
	local float		XL, YL;

	super.DrawWeaponInfo( H );

	if( LifeSpan > 0.f )
	{
		H.Canvas.DrawColor = MakeColor(255*FRand(),255*FRand(),255*FRand(),255);
		Text = "Magnet Charge life time:" @ int(LifeSpan);
		H.Canvas.TextSize( Text, XL, YL );
		H.Canvas.SetPos( H.CenterX - XL*0.5, H.CenterY*0.25 - YL*0.5 );
		H.Canvas.DrawText( Text );
	}
}

/** Draw Weapon Ammo on HUD */
simulated function DrawAmmo( HUD H )
{
	local int	drawX, drawY, barCnt, idx;
	local float drawW, drawH;

	// draw the ammo bars on top
	drawX = 93;
	drawY = 40;
	drawW = 16 * 0.8f;
	drawH = 32 * 0.8f;
	barCnt = 3;
	H.Canvas.SetDrawColor(255,255,255,128);
	for (idx = 0; idx < barCnt; idx++)
	{
		H.Canvas.SetPos(drawX + idx * 6,drawY);
		if (idx < 3 - ShotCount)
		{
			H.Canvas.DrawTile(Texture'HUD_Weapon_T_Ammo_On',
							  drawW,drawH,
							  0,0,
							  16,32);
		}
		else
		{
			H.Canvas.DrawTile(Texture'HUD_Weapon_T_Ammo_Off',
							  drawW,drawH,
							  0,0,
							  16,32);
		}
	}
}

/** @see WarWeapon::PlayWeaponFireEffects() */
simulated function PlayWeaponFireEffects( byte FireModeNum )
{
	if ( ShotCount == 0 || ShotCount > 2 )
	{
		// Activate charges
		FireSound = default.ActivationSound;
		ShotCount = 0;
		MuzzleFlashLight.Color = MakeColor(192,180,144,255);
	}
	else
	{
		// Fire charges, play normal effects
		FireSound = default.FireSound;

		if ( ShotCount == 1 )
		{
			// Positive Charge
			MuzzleFlashLight.Color = MakeColor(0,255,0,255);
		}
		else
		{
			MuzzleFlashLight.Color = MakeColor(255,0,0,255);
		}
	}
	
	super.PlayWeaponFireEffects( FireModeNum );
}

State Firing
{
	/** @see Weapon::FireAmmunition */
	function FireAmmunition( byte FireModeNum )
	{
		if ( ShotCount == 0 )
		{
			// Activate charges and reset weapon
			ActivateCharges();
		}
		else
		{
			// we fire the first two shots
			PlaceCharge(ShotCount);
		}
	}

	/**
	 * get projectile class to spawn
	 * State scoped accessor function. Override in proper state
	 */
	simulated function class<Projectile> GetProjectileClass()
	{
		return class'Proj_MagnetCharge';
	}

	/** @see WarWeapon::ShouldRefire */
	simulated function bool ShouldRefire( byte FireModeNum )
	{
		// single shot fire mode
		return false;
	}
}

/**
 * Activate magnets
 */
function ActivateCharges()
{
	PosCharge.LifeSpan = MagDuration;
	NegCharge.LifeSpan = MagDuration;

	// Create spring between two components we hit. Ends of the spring are current locations of charges.
	PosCharge.ChargeSpring.SetComponents(
		PosCharge.HitPrimComp, PosCharge.HitBoneName, PosCharge.Location, 
		NegCharge.HitPrimComp, NegCharge.HitBoneName, NegCharge.Location );

	PosCharge = None;
	NegCharge = None;
}

/**
 * Place a magnet charge where player is aiming
 */
function PlaceCharge( int ShotCount )
{
	local vector				HitLocation, HitNormal, AimDir;
	local TraceHitInfo			HitInfo;
	local Proj_MagnetCharge		NewCharge;
	local actor					HitActor;
	local SkeletalMeshComponent HitSkelComp;

	// Trace where crosshair is aiming at. Get hit info.
	HitActor = CalcWeaponFire( HitLocation, HitNormal, AimDir, HitInfo );

	// Spawn Charge
	if( ShotCount == 1 )
	{
		NewCharge = Spawn(class'Proj_MagnetChargePositive',,, HitLocation, Rotator(HitNormal));
		PosCharge = Proj_MagnetChargePositive(NewCharge);
	}
	else
	{
		NewCharge = Spawn(class'Proj_MagnetCharge',,, HitLocation, Rotator(HitNormal));
		NegCharge = NewCharge;
	}

	// Remember the component, and possibly bone name, that we hit.
	NewCharge.HitPrimComp = HitInfo.HitComponent;
	NewCharge.HitBoneName = HitInfo.BoneName;

	// Attach charge to actor we hit. 
	if ( HitActor != None )
	{
		HitSkelComp = SkeletalMeshComponent( HitInfo.HitComponent );
		NewCharge.SetBase(HitActor,, HitSkelComp, HitInfo.BoneName);
	}
	else
	{
		log("Weap_MagnetGun::PlaceCharge HitActor==None");
	}
}

defaultproperties
{
	ItemName="Magnet Gun"
	WeaponFireAnim=Shoot
	FireSound=SoundCue'LD_TempUTSounds.MagnetGunPrimary'
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(FireRate=600,Recoil=200,Inaccuracy=3,Name="Normal Fire")

	ActivationSound=SoundCue'COG_MagnetGun.MAG_ActivateCue'
	MagDuration=4.0
	MGLifeTime=8.0

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

	// Muzzle Flash point light Transform component
    Begin Object Class=TransformComponent Name=PointLightTransformComponent0
    	TransformedComponent=LightComponent0
		Translation=(X=-100,Y=0,Z=20)
    End Object
    MuzzleFlashLightTransform=PointLightTransformComponent0
	
	HUDElements.Empty
	HUDElements(0)=(bEnabled=True,DrawX=0.040000,DrawY=0.018000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=128),DrawW=256,DrawH=128,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Weapon_T_MagnetGunIcon',DrawU=0,DrawV=0,DrawUSize=256,DrawVSize=128,DrawScale=0.700000,DrawFont=None,DrawLabel="")
}