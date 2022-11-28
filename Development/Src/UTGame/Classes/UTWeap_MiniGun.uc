/**
 * UTWeap_MiniGun
 *
 * The minigun
 *
 * Created By:	Joe Wilcox
 * Copyright:	(c) 2004
 * Company:		Epic Games
*/

class UTWeap_MiniGun extends UTWeapon;

var(Weapon)	int	RoundsPerRotation[2];
var(Weapon) float WindUpTime[2];
var(Weapon) float WindDownTime[2];

var			SoundCue WeaponSpinSnd[2];

/*********************************************************************************************
 * Temp Functions.  Remove when no longer needed
 *********************************************************************************************/

function ConsumeAmmo( byte FireModeNum ) {}

simulated function float GetAmmoLeftPercent( byte FireModeNum )
{
	return 1;
}

simulated function bool CheckAmmo( byte FireModeNum, optional int Amount )
{
	return true;
}

simulated function bool HasAnyAmmo()
{
	return true;
}

simulated function ActiveRenderOverlays( HUD H )
{
	local string s;
	local float xl,yl;
	super.ActiveRenderOverlays(H);

    H.Canvas.Font = class'Engine'.Default.SmallFont;

	if ( IsInState('WeaponWindUp') )
		s = "(Winding Up)";
	else if ( IsInState('WeaponWindDown') )
		s = "(Winding Down)";
	else
		s = "("@GetStateName()$")";

	if (s!="")
	{
		H.Canvas.StrLen(s, XL, YL);
		H.Canvas.SetPos( 0.5 * (H.Canvas.SizeX - XL), H.Canvas.SizeY - 3.5*YL );
		H.Canvas.DrawText( s, false );
	}

}

/**
 * Cause Damage and spawn the hit effects
 */
/*
function ProcessInstantHit( Actor HitActor, vector AimDir, Vector HitLocation, Vector HitNormal, TraceHitInfo HitInfo )
{
	HitActor.TakeDamage(7, Instigator, HitLocation, AimDir, class'DamageType', HitInfo);
}
*/
/*********************************************************************************************
 * state WindUp
 * The Minigun will first enter this state during the firing sequence.  It winds up the barrel
 *
 * Because we don't have animations or sounds, it just delays a bit then fires.
 *********************************************************************************************/

simulated state WeaponWindUp
{
	simulated function Timer()
	{
		GotoState('WeaponFiring');
	}

begin:
	Owner.PlaySound( WeaponSpinSnd[CurrentFireMode] );
	SetTimer(WindUpTime[CurrentFireMode],False);
}

/*********************************************************************************************
 * State WindDown
 * The Minigun enteres this state when it stops firing.  It slows the barrels down and when
 * done, goes active/down/etc.
 *
 * Because we don't have animations or sounds, it just delays a bit then exits
 *********************************************************************************************/

simulated state WeaponWindDown
{
	simulated function Timer()
	{
		if ( bWeaponPutDown )
		{
			// if switched to another weapon, put down right away
			GotoState('WeaponPuttingDown');
		}
		else
		{
			// Return to the active state
			GotoState('Active');
		}
	}


begin:
	Owner.PlaySound( WeaponSpinSnd[CurrentFireMode] );
	SetTimer(WindDownTime[CurrentFireMode],false);
}

/*********************************************************************************************
 * State WeaponFiring
 * See UTWeapon.WeaponFiring
 *********************************************************************************************/

simulated state WeaponFiring
{
	/**
	 * Called when the weapon is done firing, handles what to do next.
	 * We override the default here so as to go to the WindDown state.
	 */

	simulated event ShotFired()
	{
		// Check to see if we have run out of ammo or if
		// we are being put down.


		if ( HasAmmo(CurrentFireMode) && !bWeaponPutDown )
		{

			// No, then see if we should take another shot

			if ( PendingFire[CurrentFireMode]>0 )	// Are we still firing?
			{
				TakeShot();
			}
			else
			{
				GotoState('WeaponWindDown');
			}
		}
		if ( bWeaponPutDown )
		{
			// if switched to another weapon, put down right away
			GotoState('WeaponWindDown');
			return;
		}

	}



begin:

	TakeShot();
}


defaultproperties
{
	WeaponColor=(R=255,G=255,B=0,A=255)
	FireInterval(0)=+0.066
	FireInterval(1)=+0.15
	ItemName="Mini-Gun"
	PlayerViewOffset=(X=0.0,Y=7.0,Z=-9.0)

	FiringStatesArray(0)=WeaponWindUp
	FiringStatesArray(1)=WeaponWindUp

	// Weapon SkeletalMesh
	Begin Object Class=StaticMeshComponent Name=MeshComponentA
		StaticMesh=StaticMesh'Stinger.Model.S_Weapons_Stinger_Model_3P'
		bOnlyOwnerSee=true
        CastShadow=false
		CollideActors=false
	End Object
	Mesh=MeshComponentA

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMeshA
    	TransformedComponent=MeshComponentA
		Translation=(X=0.0,Y=0.0,Z=0.0)
		Rotation=(Yaw=32768)
		Scale3D=(X=0.2,Y=0.2,Z=0.2)
    End Object
	MeshTransform=TransformComponentMeshA

	AttachmentClass=class'UTGame.UTAttachment_MiniGun'
	Components.Add(TransformComponentMeshA)


	PickupMessage="You picked up a Mini-Gun"

	// Pickup staticmesh

	// Pickup mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh1

		Begin Object Class=StaticMeshComponent Name=StaticMeshComponent1
			StaticMesh=StaticMesh'Stinger.Model.S_Weapons_Stinger_Model_3P'
			bOnlyOwnerSee=false
	        CastShadow=false
			CollideActors=false
		End Object


    	TransformedComponent=StaticMeshComponent1
		Translation=(X=0.0,Y=0.0,Z=-10.0)
		Rotation=(Yaw=32768)
		Scale3D=(X=0.3,Y=0.3,Z=0.3)
    End Object
	DroppedPickupMesh=TransformComponentMesh1
	PickupFactoryMesh=TransformComponentMesh1

	// Lighting

	Begin Object class=PointLightComponent name=MuzzleFlashLightC
		Brightness=1.0
		Color=(R=255,G=255,B=128)
		Radius=255
		CastShadows=True
		bEnabled=false
	End Object
	MuzzleFlashLight=MuzzleFlashLightC

	// Muzzle Flashlight Positioning
	Begin Object Class=TransformComponent Name=TransformComponentMesh2
    	TransformedComponent=MuzzleFlashLightC
    	Translation=(X=-60,Z=5)
    End Object
	MuzzleFlashLightTransform=TransformComponentMesh2

	MuzzleFlashLightDuration=0.3
	MuzzleFlashLightBrightness=2.0


//	WeaponFireSnd(0)=SoundCue'RocketLauncher.Sounds.SC_RocketLauncher_Fire'
//	WeaponFireSnd(1)=SoundCue'RocketLauncher.Sounds.SC_RocketLauncher_Fire'
//	WeaponLoadSnd=SoundCue'RocketLauncher.Sounds.SC_RocketLauncher_Load'

 	WeaponFireTypes(0)=EWFT_InstantHit
	WeaponFireTypes(1)=EWFT_InstantHit

	WindUpTime(0)=0.27
	WindUpTime(1)=0.27
	WindDownTime(0)=0.27
	WindDownTime(1)=0.27
	RoundsPerRotation=5
	Spread(0)=0.08
	Spread(1)=0.03

	InstantHitDamage(0)=7
	InstantHitDamage(1)=15

 	InstantHitDamageTypes(0)=class<UTDmgType_MinigunPrimary>
	InstantHitDamageTypes(1)=class<UTDmgType_MinigunSecondary>

    WeaponEquipSnd=SoundCue'Stinger.Sounds.SC_SwitchToMiniGun'
    WeaponPutDownSnd=SoundCue'Stinger.Sounds.SC_SwitchToMiniGun'

//    WeaponFireSnd(0)=SoundCue'Stinger.Sounds.SC_MiniFire'
//    WeaponFireSnd(1)=SoundCue'Stinger.Sounds.SC_MiniFire'

	WeaponSpinSnd(0)=SoundCue'Stinger.Sounds.SC_MiniSpin';
	WeaponSpinSnd(1)=SoundCue'Stinger.Sounds.SC_MiniSpin';

}
