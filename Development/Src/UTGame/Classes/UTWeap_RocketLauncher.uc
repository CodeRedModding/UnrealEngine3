class UTWeap_RocketLauncher extends UTWeapon;

const MAXLOADCOUNT=3;
const SPREADDIST=1000;

var int LoadedShotCount;
var SoundCue WeaponLoadSnd;
var byte FlockIndex;

var name AltFireAnim[3];

/*********************************************************************************************
 * Hud/Crosshairs
 *********************************************************************************************/

/**
 * This function Displays the current Shot Count on the hud

simulated function ActiveRenderOverlays( HUD H )
{
	local string s;
	local float xl,yl;

	super.ActiveRenderOverlays(H);

	H.Canvas.DrawColor = H.WhiteColor;
    s = "Ammo:"@AmmoCount@LoadedShotCount;
	H.Canvas.Font = class'Engine'.Default.LargeFont;
	H.Canvas.Strlen(S, xl, yl);

    H.Canvas.SetPos(H.Canvas.ClipX - 5 - XL - 10, H.Canvas.ClipY-5-YL);
    H.Canvas.DrawText(s);

}

 */

/*********************************************************************************************
 * Temp Ammo Functions.  Remove when the ammo system is completed.
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

/*********************************************************************************************
 * Utility Functions.
 *********************************************************************************************/

/**
 * Fire off a load of rockets.
 *
 * Network: Server Only
 */

function FireLoad()
{
    local int i,j,k;
    local vector SpreadVector;
    local rotator Aim;
	local float theta;
   	local vector Firelocation, RealStartLoc, HitLocation, HitNormal, AimDir, X,Y,Z;
	local TraceHitInfo	HitInfo;
	local Projectile	SpawnedProjectile;
	local UTProj_Rocket FiredRockets[4];
	local bool bCurl;

	local bool bFlocked;

	IncrementFlashCount(1);

	bFlocked = PendingFire[0] > 0;

	// Trace where crosshair is aiming at. Get hit info.
	CalcWeaponFire( HitLocation, HitNormal, AimDir, HitInfo );

	// this is the location where the projectile is spawned
	RealStartLoc	= GetPhysicalFireStartLoc();

	Aim = rotator(HitLocation - RealStartLoc);

	GetViewAxes(X,Y,Z);

    for (i = 0; i < LoadedShotCount; i++)
    {

    	if (!bFlocked)
    	{
   	    	// Give them some gradual spread.

	        theta = SPREADDIST*PI/32768*(i - float(LoadedShotCount-1)/2.0);
	        SpreadVector.X = Cos(theta);
	        SpreadVector.Y = Sin(theta);
	        SpreadVector.Z = 0.0;
			SpawnedProjectile = Spawn(GetProjectileClass(),,, RealStartLoc, Rotator(SpreadVector >> Aim));
			if ( SpawnedProjectile != None )
			{
				SpawnedProjectile.Init(SpreadVector >> Aim);
			}

		}
		else
		{
			Firelocation = RealStartLoc - 2* ( (Sin(i*2*PI/MAXLOADCOUNT)*8 - 7)*Y - (Cos(i*2*PI/MAXLOADCOUNT)*8 - 7)*Z) - X * 8 * FRand();
			SpawnedProjectile = Spawn(GetProjectileClass(),,, FireLocation, Aim);
			if ( SpawnedProjectile != None )
			{
				SpawnedProjectile.Init( Vector(Aim) );
			}

	        FiredRockets[i] = UTProj_Rocket(SpawnedProjectile);
		}
    }

	// Initialize the rockets so they flock towards each other

    if (bFlocked)
    {
		FlockIndex++;
		if ( FlockIndex == 0 )
		{
			FlockIndex = 1;
		}

	    // To get crazy flying, we tell each projectile in the flock about the others.
	    for ( i = 0; i < LoadedShotCount; i++ )
	    {
			if ( FiredRockets[i] != None )
			{
				FiredRockets[i].bCurl = bCurl;
				FiredRockets[i].FlockIndex = FlockIndex;
				j=0;
				for ( k=0; k<LoadedShotCount; k++ )
				{
					if ( (i != k) && (FiredRockets[k] != None) )
					{
						FiredRockets[i].Flock[j] = FiredRockets[k];
						j++;
					}
				}
				bCurl = !bCurl;
				if ( Level.NetMode != NM_DedicatedServer )
				{
					FiredRockets[i].SetTimer(0.1, true, 'FlockTimer');
				}
			}
		}

    }

}

/*********************************************************************************************
 * State WeaponLoadAmmo
 * In this state, ammo will continue to load up until MAXLOADCOUNT has been reached.  It's
 * similar to the firing state
 *********************************************************************************************/

simulated state WeaponLoadAmmo
{
	/**
	 * Adds a rocket to the count and uses up some ammo.  In Addition, it plays
	 * a sound so that other pawns in the world can here it.
	 */

	simulated function AddRocket()
	{
    	if ( LoadedShotCount < MAXLOADCOUNT && CheckAmmo(CurrentFireMode,1) )
    	{
			// Add the Rocket

			LoadedShotCount++;
			UseAmmo(CurrentFireMode,1);

			// Play a sound
			// FIXME: Wrap this once we have the ability to play on the client and everyone but the client,
			// but for now, just play on the server

			if (Role==ROLE_Authority)
			{
				PlaySound(WeaponLoadSnd);
			}

			// Play the que animation

			if ( Instigator.IsLocallyControlled() )
				PlayWeaponAnimation(AltFireAnim[ LoadedShotCount-1], FireInterval[1]);

			TimeWeaponFiring(CurrentFireMode);
		}
		else
		{
			WeaponFireLoad();
		}

	}

	simulated function WeaponFireLoad()
	{
	    Instigator.MakeNoise(1.0);

	    if (Role==Role_Authority)
	    {
			FireLoad();
		}

		if ( Instigator.IsLocallyControlled() )
		{
			PlayFireEffects( CurrentFireMode );
		}

		LoadedShotCount = 0;
		TimeWeaponFiring(CurrentFireMode);

		//FIXME: Add support for changing weapons when out of ammo
	}

	simulated event ShotFired()
	{
		if ( PendingFire[1]==0 )	// We are still firing
		{
			GotoState('Active');
		}
		else
		{
			AddRocket();
		}
	}

	/**
	 * We need to override WeaponEndFire so that we can correctly fire off the
	 * current load if we have any.
	 */

	simulated function WeaponEndFire(byte FireModeNum)
	{

		// Pass along to the global to handle everything

		Global.WeaponEndFire(FireModeNum);


		if ( FireModeNum == 1 )
		{
			if (LoadedShotCount>0)
			{
				WeaponFireLoad();
			}
		}
	}

	/**
	 * Insure that the LoadedShotCount is 0 when we leave this state
	 */

	simulated function EndState()
	{
		LoadedShotCount=0;
		super.EndState();
	}

begin:
	AddRocket();
}

defaultproperties
{
	WeaponColor=(R=255,G=0,B=0,A=255)
	FireInterval(0)=+0.9
	FireInterval(1)=+0.95
	ItemName="Rocket Launcher"
	PlayerViewOffset=(X=0.0,Y=7.0,Z=-9.0)

	FiringStatesArray(1)=WeaponLoadAmmo

	Begin Object class=AnimNodeSequence Name=MeshSequenceA
	End Object

	Begin Object Class=SkeletalMeshComponent Name=MeshComponentA
		SkeletalMesh=SkeletalMesh'RocketLauncher.Model.SK_Weapons_Model_RocketLauncher_Envy'
		PhysicsAsset=none
		AnimSets(0)=AnimSet'RocketLauncher.Anims.K_Weapons_Anims_RocketLauncher_Envy'
		Animations=MeshSequenceA
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
		CastShadow=false
	End Object
	Mesh=MeshComponentA

	// Weapon Mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMeshA
    	TransformedComponent=MeshComponentA
		Translation=(X=-5,Y=-10,Z=-5)
		Rotation=(Yaw=-16384)
		Scale3D=(X=1.0,Y=1.0,Z=1.0)
		scale=0.70
    End Object
	MeshTransform=TransformComponentMeshA

	AttachmentClass=class'UTGame.UTAttachment_RocketLauncher'
	Components.Add(TransformComponentMeshA)


	PickupMessage="You picked up a Rocket Launcher"

	// Pickup staticmesh

	// Pickup mesh Transform
	Begin Object Class=TransformComponent Name=TransformComponentMesh1

		Begin Object Class=StaticMeshComponent Name=StaticMeshComponent1
			StaticMesh=StaticMesh'RocketLauncher.S_Weapons_RocketLauncher3rdp'
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


	WeaponFireSnd(0)=SoundCue'RocketLauncher.Sounds.SC_RocketLauncher_Fire'
	WeaponFireSnd(1)=SoundCue'RocketLauncher.Sounds.SC_RocketLauncher_Fire'
	WeaponLoadSnd=SoundCue'RocketLauncher.Sounds.SC_RocketLauncher_Load'

 	WeaponFireTypes(0)=EWFT_Projectile
	WeaponFireTypes(1)=EWFT_Projectile

	WeaponProjectiles(0)=class'UTProj_Rocket'
	WeaponProjectiles(1)=class'UTProj_Rocket'

	WeaponFireAnim=WeaponFire
	WeaponPutDownAnim=WeaponPutDown
	WeaponEquipAnim=WeaponEquip

 	FireOffset=(X=70)

 	AltFireAnim(0)=AltFireQueue1
 	AltFireAnim(1)=AltFireQueue2
 	AltFireAnim(2)=AltFireQueue3
}
