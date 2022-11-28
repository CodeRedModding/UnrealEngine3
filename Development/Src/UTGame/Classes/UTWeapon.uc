/**
 * UTWeapon
 * UT Weapon implementation
 *
 * Created by:	Steven Polge
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class UTWeapon extends Weapon
	abstract;

/*********************************************************************************************
 Animations and Sounds
********************************************************************************************* */

/** Animation names to be played in the various events */

var(Animations)	name	WeaponFireAnim;
var(Animations) name	WeaponPutDownAnim;
var(Animations) name	WeaponEquipAnim;

/** Sounds the Weapon makes */

var(Sounds)	SoundCue	WeaponFireSnd[2];
var(Sounds) SoundCue 	WeaponPutDownSnd;
var(Sounds) SoundCue 	WeaponEquipSnd;

/*********************************************************************************************
 Ammunition management
********************************************************************************************* */

/** Current ammo count */
var int AmmoCount;

/** Max ammo count */
var int MaxAmmoCount;

/*********************************************************************************************
 Firing / Timing
********************************************************************************************* */

enum EWeaponFireType
{
	EWFT_InstantHit,
	EWFT_Projectile,
	EWFT_Custom,
	EWFT_None
};


/** Defines the type of fire (see Enum above) for each mode */
var  EWeaponFireType	WeaponFireTypes[2];

/** The Class of Projectile to spawn */

var	 class<UTProjectile>	WeaponProjectiles[2];

/** Selected firemode, used to cycle through available fire modes */
var	byte 	SelectedFireMode;

/** Holds the amount of time a single shot takes */

var	float 	FireInterval[2];

/** Holds the current "Fire" status for both firing modes */

var int 	PendingFire[2];

/** How much error/jitter to add to each shot */

var float 	AimError;

/** How long does it take to Equip this weapon */
var	float 	EquipTime;

/** How long does it take to put this weapon down */
var	float 	PutDownTime;

/** How much of a spread between shots */

var float Spread[2];

/** How mucb damange does a given instanthit shot do */

var float InstantHitDamage[2];

/** DamageTypes for Instant Hit Weapons */

var class<UTDamageType> InstantHitDamageTypes[2];

/** Holds an offest for spawning protectile effects. */

var(Weapon) vector FireOffset;

/*********************************************************************************************
 Mesh/Skins
********************************************************************************************* */

/** Offset from view center */
var(FirstPerson) vector	PlayerViewOffset;

/** Weapon SkelMesh */
var	MeshComponent 		Mesh;

/** Weapon skelmesh tranform component */
var	TransformComponent 	MeshTransform;


/*********************************************************************************************
 Attachments
********************************************************************************************* */

/** The class of the attachment to spawn */
var class<UTWeaponAttachment> 	AttachmentClass;

/** Holds a reference to the actual Attachment */
var UTWeaponAttachment 			WeaponAttachment;


/*********************************************************************************************
 Muzzle Flash
********************************************************************************************* */

/** Muzzle flash staticmesh, attached to weapon mesh */
var	StaticMeshComponent		MuzzleFlashMesh;

/** transform component on muzzle flash */
var	TransformComponent		MuzzleFlashTransform;

/** dynamic light */
var	PointLightComponent		MuzzleFlashLight;

/** transform component on dynamic light */
var	TransformComponent		MuzzleFlashLightTransform;

/** light brightness when flashed */
var	float					MuzzleFlashLightBrightness;

/** duration of the light */

var float					MuzzleFlashLightDuration;

/*********************************************************************************************
 Inventory Grouping/etc.
********************************************************************************************* */

/** The weapon/inventory set, 0-9. */
var	 byte			      InventoryGroup;

/** position within inventory group. (used by prevweapon and nextweapon) */
var	 byte			      GroupOffset;

/*********************************************************************************************
 Misc
********************************************************************************************* */

/** How much to damp view bob */
var() float		BobDamping;

/** The Color used when drawing the Weapon's Name on the Hud */
var color		WeaponColor;

/** Used to fade out the Weapon's name after switch */

var float WeaponFade, WeaponFadeTime;

/*********************************************************************************************
 Network replication.
********************************************************************************************* */

replication
{
	// Server->Client

	reliable if( Role==ROLE_Authority )
		ClientWeaponThrown;

	// Client->Server

	reliable if (Role<ROLE_Authority)
		ServerWeaponStartFire, ServerWeaponStopFire;

}

/*********************************************************************************************
 * Weapon Adjustment Functions
 *********************************************************************************************/

exec function AdjustMesh(string cmd)
{
	local string c,v;
	local vector t,s;
	local rotator r;
	local float sc;

	c = left(Cmd,InStr(Cmd,"="));
	v = mid(Cmd,InStr(Cmd,"=")+1);

	t  = MeshTransform.Translation;
	r  = MeshTransform.Rotation;
	s  = MeshTransform.Scale3D;
	sc = MeshTransform.Scale;

	if (c~="x")  t.X += float(v);
	if (c~="ax") t.X =  float(v);
	if (c~="y")  t.Y += float(v);
	if (c~="ay") t.Y =  float(v);
	if (c~="z")  t.Z += float(v);
	if (c~="az") t.Z =  float(v);

	if (c~="r")   R.Roll  += int(v);
	if (c~="ar")  R.Roll  =  int(v);
	if (c~="p")   R.Pitch += int(v);
	if (c~="ap")  R.Pitch =  int(v);
	if (c~="w")   R.Yaw   += int(v);
	if (c~="aw")  R.Yaw   =  int(v);

	if (c~="scalex") s.X = float(v);
	if (c~="scaley") s.Y = float(v);
	if (c~="scalez") s.Z = float(v);

	if (c~="scale") sc = float(v);

	MeshTransform.SetTranslation(t);
	MeshTransform.SetRotation(r);
	MeshTransform.SetScale(sc);
	MeshTransform.SetScale3D(s);

	log("#### AdjustMesh ####");
	log("####    Translation :"@MeshTransform.Translation);
	log("####    Rotation    :"@MeshTransform.Rotation);
	log("####    Scale3D     :"@MeshTransform.Scale3D);
	log("####    scale       :"@MeshTransform.Scale);

}

exec function AdjustFire(string cmd)
{
	local string c,v;
	local vector t;

	c = left(Cmd,InStr(Cmd,"="));
	v = mid(Cmd,InStr(Cmd,"=")+1);

	t  = FireOffset;

	if (c~="x")  t.X += float(v);
	if (c~="ax") t.X =  float(v);
	if (c~="y")  t.Y += float(v);
	if (c~="ay") t.Y =  float(v);
	if (c~="z")  t.Z += float(v);
	if (c~="az") t.Z =  float(v);

	FireOffset = t;
	log("#### FireOffset ####");
	log("####    Vector :"@FireOffset);

}


/*********************************************************************************************
 * Hud/Crosshairs
 *********************************************************************************************/

simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	local string T;

	super.DisplayDebug(HUD, out_YL, out_YPos);

	T = "Eyeheight "$Instigator.EyeHeight$" base "$Instigator.BaseEyeheight$" landbob "$Instigator.Landbob$" just landed "$Instigator.bJustLanded$" land recover "$Instigator.bLandRecovery;
    HUD.Canvas.DrawText(T, false);
    out_YPos += out_YL;
    HUD.Canvas.SetPos(4,out_YPos);
    HUD.Canvas.DrawText("PendingFires:"@PendingFire[0]@PendingFire[1]);
    out_YPos += out_YL;
    HUD.Canvas.SetPos(4,out_YPos);
    HUD.Canvas.DrawText("Owner:"@Owner);
    out_YPos += out_YL;
}

/**
 * Access to HUD and Canvas.
 * Event always called when the InventoryManager considers this Inventory Item currently "Active"
 * (for example active weapon)
 */
simulated function ActiveRenderOverlays( HUD H )
{
	local float		XL, YL;

	DrawWeaponCrosshair( H );
    H.Canvas.Font = class'Engine'.Default.MediumFont;
	// Draw weapon info
	H.Canvas.DrawColor	= WeaponColor;

	if (WeaponFade < Level.TimeSeconds)
		return;

	if ( WeaponFade - Level.TimeSeconds < (WeaponFadeTime/2) )
		H.Canvas.DrawColor.A = 255 * ( (WeaponFade-Level.TimeSeconds) / (WeaponFadeTime/2));

	H.Canvas.StrLen(ItemName, XL, YL);
	H.Canvas.SetPos( 0.5 * (H.Canvas.SizeX - XL), H.Canvas.SizeY - 1.5*YL );
	H.Canvas.DrawText( ItemName, false );
}

/**
 * Draw the Crosshairs
 */

simulated function DrawWeaponCrosshair( Hud H )
{
	local	float	CrosshairSize;

	// Draw Temporary Crosshair
	CrosshairSize = 4;
	H.Canvas.DrawColor	= WeaponColor;
	H.Canvas.SetPos((0.5 * H.Canvas.ClipX) - CrosshairSize, 0.5 * H.Canvas.ClipY);
	H.Canvas.DrawRect(2*CrosshairSize + 1, 1);

	H.Canvas.SetPos(0.5 * H.Canvas.ClipX, (0.5 * H.Canvas.ClipY) - CrosshairSize);
	H.Canvas.DrawRect(1, 2*CrosshairSize + 1);
}


/*********************************************************************************************
 * Interactions
 *********************************************************************************************/


/**
 * hook to override Next weapon call.
 * For example the physics gun uses it to have mouse wheel change the distance of the held object.
 * Warning: only use in firing state, otherwise it breaks weapon switching
 */

simulated function bool DoOverrideNextWeapon()
{
	return false;
}

/**
 * hook to override Previous weapon call.
 */

simulated function bool DoOverridePrevWeapon()
{
	return false;
}

/**
 * Drop this weapon out in to the world
 */

function DropFrom( vector StartLocation, vector StartVelocity )
{
    if ( !bCanThrow || !HasAnyAmmo() )
        return;

	// FIXMESTEVE- stop firing

	DetachWeaponFrom( Instigator.Mesh );
	Super.DropFrom(StartLocation, StartVelocity);
	ClientWeaponThrown();
}

simulated function ClientWeaponThrown();


/*********************************************************************************************
 * Ammunition
 *********************************************************************************************/

function UseAmmo( byte FireModeNum, int Amount )
{
	// Subtrace the Ammo

	AmmoCount = Max(0,AmmoCount-Amount);
}

/** @see Weapon::GetAmmoLeftPercent */
simulated function float GetAmmoLeftPercent( byte FireModeNum )
{
	return AmmoCount/MaxAmmoCount;
}

/** @see Weapon::HasAmmo */
simulated function bool CheckAmmo( byte FireModeNum, optional int Amount )
{

	return true;

	if (Amount<=0)
		Amount=1;

	return ( AmmoCount >= Amount );
}

/** @see Weapon::HasAnyAmmo */
simulated function bool HasAnyAmmo()
{
	return ( AmmoCount > 0 );
}


simulated function Loaded()
{
	AmmoCount = 999;
}

/**
 * Returns the type of projectile to spawn.  We use a function so subclasses can
 * override it if needed (case in point, homing rockets).
 */

function class<Projectile> GetProjectileClass()
{
	return WeaponProjectiles[CurrentFireMode];
}

/**
 * Returns the spread for this function */

function float GetSpread()
{
	return Spread[CurrentFireMode];
}

/*********************************************************************************************
 * AI
 *********************************************************************************************/

// STEVE: Put all of the AI here ;)


/*********************************************************************************************
 * Effects / Animations
 *********************************************************************************************/

/**
 * Play an animation on the weapon mesh
 * Network: Local Player and clients
 *
 * @param	Anim Sequence to play on weapon skeletal mesh
 * @param	desired duration, in seconds, animation should be played
 */

simulated function PlayWeaponAnimation( Name Sequence, float fDesiredDuration )
{
	local SkeletalMeshComponent	SkelMesh;
	local float fNewRate;

	// do not play on a dedicated server
	if ( Level.NetMode == NM_DedicatedServer )
		return;

	SkelMesh = SkeletalMeshComponent(Mesh);
	// Check we have access to mesh and animations
	if ( SkelMesh == None || AnimNodeSequence(SkelMesh.Animations) == None )
	{
		log("UTWeapon::PlayWeaponAnimation - no mesh or no animations :(");
		return;
	}

	// Set right animation sequence if needed
	if ( AnimNodeSequence(SkelMesh.Animations).AnimSeq == None || AnimNodeSequence(SkelMesh.Animations).AnimSeq.SequenceName != Sequence )
		AnimNodeSequence(SkelMesh.Animations).SetAnim( Sequence );

	if ( AnimNodeSequence(SkelMesh.Animations).AnimSeq == None )
	{
		log("UTWeapon::PlayWeaponAnimation - AnimNodeSequence(Mesh.Animations).AnimSeq == None");
		return;
	}

	// Calculate new playing rate based on desired duration
	fNewRate = AnimNodeSequence(SkelMesh.Animations).AnimSeq.SequenceLength / fDesiredDuration;

	// Play Animation
	AnimNodeSequence(SkelMesh.Animations).PlayAnim( false, fNewRate );
}

/**
 * PlayFireEffects Is the root function that handles all of the effects associated with
 * a weapon.  It should only be called on remote clients or the local player
 */

simulated event PlayFireEffects( byte FireModeNum )
{
	// Play Weapon fire animation

	if ( WeaponFireAnim != '' )
		PlayWeaponAnimation( WeaponFireAnim, FireInterval[FireModeNum] );

	// play weapon fire sound
	if ( WeaponFireSnd[FireModeNum] != None )
		Owner.PlaySound( WeaponFireSnd[FireModeNum] );

	// Start muzzle flash effect
	CauseMuzzleFlashLight();
}

/**
 * Turn the Muzzle Flash's light off
 */

simulated event MuzzleFlashLightTimer()
{
	if ( MuzzleFlashLight!=None )
		MuzzleFlashLight.bEnabled = false;
}

/**
 * Cause the Muzzle Flash's light to appear and set a timer to hide it
 */

simulated event CauseMuzzleFlashLight()
{
	// Quick exit if there isn't a light

	if (MuzzleFlashLight==None)
		return;

	MuzzleFlashLight.Brightness = MuzzleFlashLightBrightness;
	MuzzleFlashLight.bEnabled = true;
	SetTimer(MuzzleFlashLightDuration,false,'MuzzleFlashLightTimer');
}


/**
 * Show the weapon being put away
 */

simulated function PlayWeaponPutDown()
{
	// Play the animation for the weapon being put down

	if ( WeaponPutDownAnim != '' )
		PlayWeaponAnimation( WeaponPutDownAnim, PutDownTime );

	// play any assoicated sound
	if ( WeaponPutDownSnd != None )
		Owner.PlaySound( WeaponPutDownSnd );
}

/**
 * Show the weapon begin equipped
 */

simulated function PlayWeaponEquip()
{
	// Play the animation for the weapon being put down

	if ( WeaponEquipAnim != '' )
		PlayWeaponAnimation( WeaponEquipAnim, EquipTime );

	// play any assoicated sound
	if ( WeaponEquipSnd != None )
		Owner.PlaySound( WeaponEquipSnd );
}

/*********************************************************************************************
 * Timing
 *********************************************************************************************/


/**
 * Sets the timing for a single shot.  When the delay has expired, the ShotFired event is
 * triggered.
 */

simulated function TimeWeaponFiring( byte FireModeNum )
{
	SetTimer( FireInterval[FireModeNum], false, 'ShotFired' );
}

/**
 * Sets the timing for putting a weapon down.  The WeaponIsDown event is trigged when expired
*/

simulated function TimeWeaponPutDown()
{
	if ( Instigator.IsLocallyControlled() )
	{
		PlayWeaponPutDown();
	}

	SetTimer( PutDownTime, false, 'WeaponIsDown' );
}

/**
 * Sets the timing for equipping a weapon.  The WeaponEquipped event is trigged when expired
*/

simulated function TimeWeaponEquipping()
{

	// The weapon is equipped, attach it to the mesh.

	if ( UTPawn(Instigator) != none )
	{
		AttachWeaponTo( Instigator.Mesh, UTPawn(Instigator).WeaponBone );
	}
	else
	{
	  	AttachWeaponTo( Instigator.Mesh, 'spine' );
	}

	// Play the animation

	if ( Instigator.IsLocallyControlled() )
	{

		PlayWeaponEquip();
	}

	SetTimer( EquipTime, false ,'WeaponEquipped' );
}


/**
 * These events are called by the various timing functions above.
 */

/*

FIXME: This needs an engine fix so that enhanced timers repsect states.

simulated event ShotFired(); // Should be subclassed in the firing state to handle transitions
simulated event WeaponEquipped();   // Subclass if needed
simulated event WeaponIsDown();		// Subclass if needed

*/

/*********************************************************************************************
 * Third person weapon attachment functions
 *********************************************************************************************/

/**
 * Attach Weapon Mesh, Weapon MuzzleFlash and Muzzle Flash Dynamic Light to a SkeletalMesh
 *
 * @param	SkeletalMeshComponent where to attach weapon
 * @param	BoneName of SkeletalMeshComponent where to attach the weapon
 */
simulated function AttachWeaponTo( SkeletalMeshComponent MeshCpnt, Name BoneName )
{
	// Spawn the 3rd Person Attachment

	if (Role==ROLE_Authority && UTPawn(Instigator) != None)
	{
		UTPawn(Instigator).CurrentWeaponAttachmentClass = AttachmentClass;
		if ( Instigator.IsLocallyControlled() )
		{
  			UTPawn(Instigator).WeaponAttachmentChanged();
  		}
	}

	if ( Instigator.IsLocallyControlled() )
	{
		bHidden = false;

		// Muzzle Flash Mesh
		if ( MuzzleFlashTransform != None )
		{
			MeshCpnt.AttachComponent(MuzzleFlashTransform, BoneName);
		}
	    else if ( MuzzleFlashMesh != None )
	    {
			MeshCpnt.AttachComponent(MuzzleFlashMesh, BoneName);
		}

		// Muzzle Flash dynamic light
		if ( MuzzleFlashLightTransform != None )
		{
			MeshCpnt.AttachComponent(MuzzleFlashLightTransform, BoneName);
		}
		else if ( MuzzleFlashLight != None )
		{
		    MeshCpnt.AttachComponent(MuzzleFlashLight, BoneName);
		}
	}
}

/**
 * Detach weapon from skeletal mesh
 *
 * @param	SkeletalMeshComponent weapon is attached to.
 */
simulated function DetachWeaponFrom( SkeletalMeshComponent MeshCpnt )
{
	if (Role==ROLE_Authority && UTPawn(Instigator) != None)
	{
		UTPawn(Instigator).CurrentWeaponAttachmentClass = none;
		if ( Instigator.IsLocallyControlled() )
  			UTPawn(Instigator).WeaponAttachmentChanged();
	}

	if ( Instigator.IsLocallyControlled() )
	{
		bHidden = true;

		// Muzzle Flash mesh
		if ( MuzzleFlashTransform != None )
			MeshCpnt.DetachComponent( MuzzleFlashTransform );
	    else if ( MuzzleFlashMesh != None )
			MeshCpnt.DetachComponent( MuzzleFlashMesh );

		// Muzzle Flash dynamic light
		if ( MuzzleFlashLightTransform != None )
			MeshCpnt.DetachComponent( MuzzleFlashLightTransform );
		else if ( MuzzleFlashLight != None )
		    MeshCpnt.DetachComponent( MuzzleFlashLight );
	}
}

/*********************************************************************************************
 * Pawn/Controller/View functions
 *********************************************************************************************/

simulated function GetViewAxes( out vector xaxis, out vector yaxis, out vector zaxis )
{
    if ( Instigator.Controller == None )
        GetAxes( Instigator.Rotation, xaxis, yaxis, zaxis );
    else
        GetAxes( Instigator.Controller.Rotation, xaxis, yaxis, zaxis );
}

/*********************************************************************************************
 * Handling the actual Fire Commands
 *********************************************************************************************/

/* Weapon Firing Logic overiew:

	The weapon system here is designed to be a single code path that follows the same flow on both
	the Authoritive server and the local client.  Remote clients know nothing about the weapon and utilize
	the WeaponAttachment system to see the end results.


	1: The InventoryManager (IM) on the Local Client recieves a StartFire call.  It calls WeaponStartFire().

	2: If Local Client is not Authoritive it notifies the server via ServerWeaponStartFire().

	3: Both WeaponStartFire() and ServerWeaponStartFire() sync up by calling WeaponBeginFire().

	4: WeaponBeginFire sets the PendingFire flag for the incoming fire Mode

	5: WeaponBeginFire looks at the current state and if it's in the Active state, it begins the
	   firing sequence by transitioning to the new fire state as defined by the FiringStatesArray
	   array.  This is done by called SendToFiringState.

	6: The Firing Logic is handled in the various firing states.  Firing states are responsible for the
	   following:
	   				a: Continuing to fire if their associated PendingFire is hot
	   				b: Transitioning to a new weapon when out of ammo
	   				c: Transitioning to the "Active" state when no longer firing


    The weapon system also receives a WeaponStopFire() event from the IM.  When this occurs, the following
    logic is performed:

    1: The IM on the Local Client calls WeaponStopFire().

    2: If Weapon Stop fire is not on the Authoritive process, it notifes the server via the
	   ServerWeaponStopFire() event.

	3: Both WeaponStopFire() and ServerWeaponStopFire() sync up by calling WeaponEndFire().

	4: WeponEndFire() clears the PendingFire flag for this outgoing fire mode.


	Firing states should be identical in their execution, branching outwards as need.  For example,
	in the default firing state ('WeaponFiring') the function TakeShot() occurs in all applicable processes.
	Notice TakeShot() will only execute FireAmmunition() when running on the server while PlayFireEffects() will
	only be executed on a local client.

*/


simulated function WeaponStartFire( byte FireModeNum )
{
	if ( !CheckAmmo(FireModeNum) )		// No Ammo, the do a quick exit.
	{
		return;
	}

	// Setup the Firing logic for this weapon regardless of where it's
	// running (Client/Local/Server).

	WeaponBeginFire(FireModeNum);

	if (Role < Role_Authority)
	{

		// StartFire was called on a remote client so
		// Tell the server this Weapon has begun to fire

		ServerWeaponStartFire(FireModeNum);
	}

}

simulated function ServerWeaponStartFire(byte FireModeNum)
{
	// No Ammo, the do a quick exit.

	if ( !CheckAmmo(FireModeNum) )
	{
		return;
	}

	// A client has fired, so the server needs to
	// begin to fire as well

	WeaponBeginFire(FireModeNum);

}


simulated function WeaponBeginFire(byte FireModeNum)
{

	// Flag this mode as pending a fire.  The only thing that can remove
	// this flag is a Stop Fire/Putdown command.

	PendingFire[FireModeNum] = 1;

	// Fire is only initially processed in the Active State.  Otherwise,
	// it's marked as pending.

	if ( IsInState('Active') )
	{
		SendToFiringState( FireModeNum );
	}
}


simulated function WeaponStopFire( byte FireModeNum )
{
	// Locally shut down the fire sequence

	WeaponEndFire(FireModeNum);

	// Notify the server

	if ( Role < Role_Authority )
	{
		ServerWeaponStopFire( FireModeNum );
	}
}

function ServerWeaponStopFire( byte FireModeNum )
{
	WeaponEndFire(FireModeNum);
}

simulated function WeaponEndFire(byte FireModeNum)
{

	// Clear the firing flag for this mode

	PendingFire[FireModeNum] = 0;
}


simulated function SendToFiringState( byte FireModeNum )
{
	if (UTPawn(Instigator)!=None)
	{
		UTPawn(Instigator).FiringMode = FireModeNum;
	}

	super.SendToFiringState(FireModeNum);
}


/**
 * FireAmmunition is a server only function that can be equated with game logic for firing.  No effects should be
 * played from here out as this portion of the chain should is server only.  Effects are handled elsewhere.
 */

function FireAmmunition( byte FireModeNum )
{
	// Handle the different fire types

	switch ( WeaponFireTypes[FireModeNum] )
	{
		case EWFT_InstantHit:
			TraceFire();
			break;


		case EWFT_Projectile:
			ProjectileFire();
			break;

		case EWFT_Custom:
			CustomFire();
			break;

	}
}

/**
 * GetAdjustedAim begins a chain of function class that allows the weapon, the pawn and the controller to make
 * on the fly adjustments to where this weapon is pointing.
 */

simulated function Rotator GetAdjustedAim( vector StartFireLoc )
{
	local rotator R;

	// Start the chain, see Weapon.GetAdjustedAim()

	R = Instigator.GetAdjustedAimFor( Self, StartFireLoc );

	// Add in support for spread.

	return rotator(vector(R) + VRand() * FRand() * GetSpread() );
}

/**
 * When an instant-hit shot is fired, after the trace is processed and a hit is detected, this
 * function is called.  It's first goal is to set the FlashLocation for remote client effects.  It
 * then causes damage to the hit actor.  This is a server only function.
 */

function ProcessInstantHit( Actor HitActor, vector AimDir, Vector HitLocation, Vector HitNormal, TraceHitInfo HitInfo )
{
	SetFlashLocation(HitLocation);
	HitActor.TakeDamage(InstantHitDamage[CurrentFireMode], Instigator, HitLocation, AimDir, InstantHitDamageTypes[CurrentFireMode], HitInfo);
}

/**
 * Create the projectile, but also increment the flash count for remote client effects.
 */

function ProjectileFire()
{
	super.ProjectileFire();
	IncrementFlashCount(CurrentFireMode);
}


/**
 * If the weapon isn't an instant hit, or a simple projectile, it should use the tyoe EWFT_Custom.  In those cases
 * this function will be called.  It should be subclassed by the custom weapon.
 */
function CustomFire();


/**
 * This function turns the location for spawning the projectiles/effects
 */

simulated function Vector GetPhysicalFireStartLoc()
{
	return Instigator.GetPawnViewLocation() + (FireOffset >> Instigator.Rotation);
}


/*********************************************************************************************
 * Remote Client Firing Magic..
 *
 * UTPAWN contains two variables.  FlashCount and FlashLocation.  Depending on the firing mode,
 * one of these needs to be updated in order for remote clients to properly display firing
 * effects.
 *
 * For EWFT_InstantHit fire modes, you should called SetFlashLocation with the location of the hit.
 * for EWFT_PRojectile fire modes, you should call IncrementFlashCount.
 *
 * These will call their respected versions in UTPawn that will setup replication.
 *********************************************************************************************/

function IncrementFlashCount(byte FireModeNum)
{
	// Tell the pawn we have fired so it can notify it's attachments

	if ( UTPawn(Instigator) != none )
		UTPawn(Instigator).IncrementFlashCount(FireModeNum);
}

function SetFlashLocation(vector Location)
{
	// Tell the pawn we have fired so it can notify it's attachments

	if ( UTPawn(Instigator)!=None )
		UTPawn(Instigator).SetFlashLocation(Location);
}


/*********************************************************************************************
 * State Inactive
 * Default state for a weapon. It is not active, cannot fire and resides in player inventory.
 *********************************************************************************************/

auto state Inactive
{
	/**
	 * Clear out the PendingFires
	 */

	simulated function BeginState()
	{
		local int i;

		for ( i=0; i<ArrayCount(PendingFire); i++ )
		{
			PendingFire[i]=0;
		}
	}

	/**
	 * Begin equipping this weapon
	 */

	simulated function Activate()
	{
		GotoState('WeaponEquipping');
	}
}

/*********************************************************************************************
 * State Ative
 * A Weapon this is being held by a pawn should be in the active state.  In this state,
 * a weapon should loop any number of idle animations, as well as check the PendingFire flags
 * to see if a shot has been fired.
 *********************************************************************************************/

simulated state Active
{

	ignores Timer;

	/**
	 * In the active State, **IF** there is a pending Fire,
	 * Handle it.
	 */

	function BeginState()
	{
		local int i;

		super.BeginState();	// Play the Idle Animation

        // If either of the fire modes are pending, perform them

		for ( i=0; i<ArrayCount(PendingFire); i++ )
		{
			if (PendingFire[i]!=0)
			{
				WeaponStartFire(i);
			}
		}

	}

	/**
	 * Put the weapon down
	 */

	simulated function bool TryPutDown()
	{
		GotoState('WeaponPuttingDown');
		return true;
	}

}

/*********************************************************************************************
 * State WeaponFiring
 * This is the default Firing State.  It's performed on both the client and the server.
 *********************************************************************************************/

simulated state WeaponFiring
{
	/**
	 * Called when the weapon is done firing, handles what to do next.
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
				GotoState('Active');
			}
		}
		if ( bWeaponPutDown )
		{
			// if switched to another weapon, put down right away
			GotoState('WeaponPuttingDown');
			return;
		}

	}

	/**
	 * This is the work horse.
	 */

	simulated function TakeShot()
	{

		UseAmmo(CurrentFireMode,1);

		// If we are the server, then branch off and perform the
		// gameplay "fire" logic.

		if (Role==ROLE_Authority)
			FireAmmunition( CurrentFireMode );

		// if this is the local player, play the firing effects

		if ( Instigator.IsLocallyControlled() )
			PlayFireEffects( CurrentFireMode );

		// Time the shot

		TimeWeaponFiring(CurrentFireMode);
	}


begin:

	TakeShot();
}

/*********************************************************************************************
 * State WeaponEquipping
 * This state is entered when a weapon is becomeing active (ie: Being brought up)
 *********************************************************************************************/

simulated state WeaponEquipping
{

	/**
	 * We want to being this state by setting up the timing and then notifying the pawn
	 * that the weapon has changed.
	 */

	simulated function BeginState()
	{
		TimeWeaponEquipping();
		bWeaponPutDown	= false;

		// Notify the pawn that it's weapon has changed.

		if (Instigator.IsLocallyControlled() && UTPawn(Instigator)!=None)
		{
			UTPawn(Instigator).WeaponChanged(self);
			WeaponFade = Level.TimeSeconds + WeaponFadeTime;
		}
	}

	simulated event WeaponEquipped()
	{
		if ( bWeaponPutDown )
		{
			// if switched to another weapon, put down right away
			GotoState('WeaponPuttingDown');
			return;
		}

		GotoState('Active');
	}



}


/*********************************************************************************************
 * State WeaponPuttingDown
 * This state is entered when a weapon is being put away
 *********************************************************************************************/

simulated state WeaponPuttingDown
{
	/**
	 * Time the process and clear the Firing flags
	 */

	simulated function BeginState()
	{
		local int i;

		TimeWeaponPutDown();
		bWeaponPutDown = false;


		// Tell weapon to stop firing server side

		if (Role < ROLE_Authority)
		{
			for (i=0;i<ArrayCount(PendingFire);i++)
			{
				if ( PendingFire[i]>0 )
				{
					WeaponStopFire(i);
				}
			}
		}
	}

	/**
	 * We are done putting the weapon away, remove the mesh/etc.
	 */

	simulated event WeaponIsDown()
	{
		// This weapon is down, remove it from the mesh
		DetachWeaponFrom( Instigator.Mesh );

		// Switch to pending weapon
		if ( Instigator.IsLocallyControlled() )
		{
			InvManager.ChangedWeapon();
		}

		// Put weapon to sleep
		GotoState('Inactive');
	}

	/** Event called when weapon enters this state */
}

/*********************************************************************************************
 * State PendingClientWeaponSet
 * A weapon sets in this state on a remote client while it awaits full replication of all
 * properties.
 *********************************************************************************************/

simulated state PendingClientWeaponSet
{
	/** @see Weapon::Activate() */
	simulated function Activate()
	{
		GotoState('WeaponEquipping');
	}
}

defaultproperties
{
	PlayerViewOffset=(X=4,Y=4,Z=4)
	ItemName="Weapon"
	WeaponColor=(R=255,G=255,B=255,A=255)
	MuzzleFlashLightBrightness=2.0
	MaxAmmoCount=1
	FireInterval(0)=+1.0
	FireInterval(1)=+1.0
	bOnlyRelevantToOwner=true
    BobDamping=0.50000
	DroppedPickupClass=class'UTPickup'
	RespawnTime=+00030.000000
	FiringStatesArray(0)=WeaponFiring
	FiringStatesArray(1)=WeaponFiring
	EquipTime=+0.15
	PutDownTime=+0.15
	Components.Remove(Sprite)
	bRenderOverlays=true
	AimError=0

	WeaponFireTypes(0)=EWFT_InstantHit
	WeaponFireTypes(1)=EWFT_InstantHit

	InstantHitDamageTypes(0)=class<UTDamageType>
	InstantHitDamageTypes(1)=class<UTDamageType>
	WeaponFadeTime=2.0
}
