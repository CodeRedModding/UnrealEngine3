/** 
 * WarWeapon
 * Warfare Weapon implementation
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class WarWeapon extends Weapon
	config(Weapon)
	native;

/** Set when player requires a manual weapon reload, to delay until it is possible to process */
var		bool	bForceReload;
/** Set to true if weapon is firing. Replicated to play weapon animations */
var		bool	bIsFiring;

/** 
 * FireMode definition struct 
 */
struct native FireModeStruct
{
	/** Name of FireMode */
	var() Localized	String					Name;
	/** Rate of Fire in rounds per minute */
	var() config	float					FireRate;
	/** Weapon Recoil applied to player view rotation every time a shot is fired */
	var() config	int						Recoil;
	/** Base Accuracy value. Max angle offset in degrees from perfect aim. (0 = perfect accuracy/aim) */
	var() config	float					Inaccuracy;
	/** Damage for instant traces */
	var() config	int						Damage;
	/** Damage type for this fire mode */
	var() config	class<WarDamageType>	DamageTypeClass;

	structdefaults
	{
		FireRate=240
		Damage=10
		DamageTypeClass=class'WarDamageType'
	};
};
/** Array of fire mode definition */
var() config	Array<FireModeStruct>	FireModeInfoArray;	

/** abitrary firemode for melee */
const MELEE_FIREMODE	= 128;

/** abitrary firemode for reloading */
const RELOAD_FIREMODE	= 129;


/** Animation to play on weapon */
var()	config	Name		WeaponFireAnim;
/** Weapon fire sound to play when firing bullet */
var()	config	SoundCue	FireSound;
/** Selected firemode, used to cycle through available fire modes */
var				byte		SelectedFireMode;
/** Number of shots fired consecutively */
var				byte		ConsShotCount;
/** Fire mode cycle sound */
var()	config	SoundCue	FireModeCycleSound;
//
// Ammo
//

/** Size of a magazine. When it is empty and more ammo is left, then reloading is required */
var()	config		int		MagazineSize;

/** Ammo used from magazine */
var()				int		AmmoUsedCount;

/** When reaching this amount, ammo becomes critical (warnings, flashing, beeping, whatever..). */
var()	config		int		CriticalAmmoCount;

/** duration in seconds of reload sequence */
var()	config		float	ReloadDuration;

//
// Weapon Mesh
//

/** Weapon SkelMesh */
var()				MeshComponent			Mesh;

/** weapon skelmesh tranform component */
var()	config		TransformComponent		MeshTranform;

/** Fire Offset from Bone Hand location */
var()	config		vector					FireOffset;

//
// Muzzle Flash
//

/** Muzzle flash staticmesh, attached to weapon mesh */
var()				MeshComponent			MuzzleFlashMesh;

/** transform component on muzzle flash */
var()	config		TransformComponent		MuzzleFlashTransform;

/** dynamic light */
var()				PointLightComponent		MuzzleFlashLight;

/** transform component on dynamic light */
var()	config		TransformComponent		MuzzleFlashLightTransform;

/** light brightness when flashed */
var()	config		float					MuzzleFlashLightBrightness;

//
// Melee
//

/**	range of melee attack using this weapon */
var()	config		float			fMeleeRange;

/** damage done by melee attack */
var()	config		int				MeleeDamage;

/**	Length in seconds of Melee Attack */
var()	config		float			fMeleeDuration;

//
// Player animations
//

/** Firing animation played by Pawn */
var()	config		Name	PawnFiringAnim;
/** Pawn Idle animation played while carrying this weapon */
var()	config		Name	PawnIdleAnim;
/** reload animation played while carrying this weapon */
var()	config		Name	PawnReloadAnim;
/** Melee attack animation */
var()	config		Name	PawnMeleeAnim;

//
// Sounds
//

/** Sound played when reloading weapon */
var()	config		SoundCue	WeaponReloadSound;
/** Sound played when equiping weapon */
var()	config		SoundCue	WeaponEquipSound;

/** Scale that defines how much the crosshair expands when being inaccurate */
var()	config		float		CrosshairExpandStrength;

/** list of crosshair HUD elements to draw when this weapon is active*/
var()	config	Array<WarHUD.HUDElementInfo>	CrosshairElements;

/** List of HUD elements to draw when this weapon is active */
var()	config	array<WarHUD.HUDElementInfo>	HUDElements;

/** Weapon name */
var()	config	WarHUD.HUDElementInfo	WeapNameHUDElemnt;

/** Ammo instanced material (updated in script) */
var		transient	MaterialInstanceConstant	HUDAmmoMaterialInstance;
/** Ammo material */
var					Material					HUDAmmoMaterial;

cpptext
{
	void PreNetReceive();
	void PostNetReceive();
}

//
// Network replication
//

replication
{
	// Things the server should send to ALL clients but the local player.
	reliable if( Role==ROLE_Authority && !bNetOwner && bNetDirty )
		bIsFiring;

	// Things the server should send to local player.
	reliable if ( Role==ROLE_Authority && bNetOwner && bNetDirty )
		AmmoUsedCount;

	// functions called by client on server
	reliable if( Role<ROLE_Authority )
		ServerSetFireMode, ServerForceReload, ServerPutDown;
}

/** Event called when weapon actor is destroyed */
simulated event Destroyed()
{
	if ( Instigator != None )
	{
		DetachWeaponFrom( Instigator.Mesh );
	}

	super.Destroyed();
}

/**
 * Set Weapon Recoil effect, which modifies the player view rotation
 *
 * @param	PitchRecoil, Pitch offset added to player view rotation
 */
simulated function SetWeaponRecoil( int PitchRecoil )
{
	local float	fRampUpScale;

	if ( WarInventoryManager(InvManager) != None )
	{
		// Ramp up recoil based on number of consecutive shots fired
		if ( MagazineSize > 0 )
		{
			fRampUpScale = float(ConsShotCount) / float(MagazineSize);
			PitchRecoil *= fRampUpScale;
		}
		WarInventoryManager(InvManager).SetWeaponRecoil( PitchRecoil );
	}
}

/**
 * Called before firing weapon to adjust Trace/Projectile aiming
 * State scoped function. Override in proper state
 * Network: Server
 *
 * @param	StartFireLoc,	world location of weapon start trace or projectile spawning
 */
simulated function Rotator GetAdjustedAim( vector StartFireLoc )
{
	local Rotator	AimRot;
	local float		fAdjustedInaccuracy, fMaxInaccuracy;

	AimRot			= super.GetAdjustedAim( StartFireLoc );
	fMaxInaccuracy	= GetFireModeAccuracy();

	// modify Aim to add player (in)accuracy
	if ( fMaxInaccuracy > 0.f )
	{
		// Scale by player accuracy and convert from degrees to Unreal Units ( * 65536 / 360 )
		fAdjustedInaccuracy	 = fMaxInaccuracy * WarInventoryManager(InvManager).GetPlayerAccuracy() * 182.044;

		AimRot.Pitch	+= fAdjustedInaccuracy * (0.5 - FRand());
		AimRot.Yaw		+= fAdjustedInaccuracy * (0.5 - FRand());
		AimRot.Roll		+= fAdjustedInaccuracy * (0.5 - FRand());
	}

	return AimRot;
}

/**
 * returns current fire mode's inaccuracy, which is the maximum error angle in degrees.
 * A value of 0 means perfect aim. (no aim error).
 *
 * @return	current fire mode's inaccuracy. Float, which represents the maximum error angle in degrees.
 */
simulated function float GetFireModeAccuracy()
{
	// make melee accurate for now
	if ( CurrentFireMode == MELEE_FIREMODE )
	{
		return 0.f;
	}

	return FireModeInfoArray[SelectedFireMode].Inaccuracy;
}

/**
 * Physical fire start location. (from weapon's barrel in world coordinates) 
 * State scoped function. Override in proper state
 */
simulated function Vector GetPhysicalFireStartLoc()
{
	if( WarPawn(Instigator) != None )
	{
		return WarPawn(Instigator).GetPhysicalFireStartLoc( FireOffset );
	}
	else if( Turret(Instigator) != None )
	{
		return Turret(Instigator).GetPhysicalFireStartLoc( FireOffset );
	}
}

/**
 * Performs an 'Instant Hit' trace and processes a hit 
 * State scoped function. Override in proper state
 * Network: Server
 *
 * @param	damage given assigned to HitActor
 * @param	DamageType class
 */
function TraceFire()
{
	local Vector		HitLocation, HitNormal, AimDir;
	local Actor			HitActor;
	local TraceHitInfo	HitInfo;
	local Controller	CheckPlayer;

	HitActor = CalcWeaponFire( HitLocation, HitNormal, AimDir, HitInfo );
	
	if ( ShouldSpawnTracerFX() )
	{
		SpawnTracerEffect( HitLocation );
	}

	if ( HitActor != None )
    {
		ProcessInstantHit( HitActor, AimDir, HitLocation, HitNormal, HitInfo );
	}
	
	// if we didn't hit a player
	if ( Pawn(HitActor) == None )
	{
		// notify any nearby players of the near miss
		for ( CheckPlayer = Level.ControllerList; CheckPlayer != None; CheckPlayer = CheckPlayer.nextController )
		{
			// check to see if they have a valid pawn in line with the shot fired
			// and we hit nothing, hit near them, or hit past them
			if ( CheckPlayer.Pawn != None &&
				 CheckPlayer.Pawn != Instigator &&
				 PointDistToLine( CheckPlayer.Pawn.Location, AimDir, Instigator.Location ) <= CheckPlayer.GetCollisionRadius() * 3.f &&
				 ( HitActor == None ||
                   VSize( HitLocation - Instigator.Location ) + CheckPlayer.GetCollisionRadius() * 3.f > VSize( CheckPlayer.Pawn.Location - Instigator.Location ) ) )
			{
				CheckPlayer.NotifyNearMiss( Instigator );
			}
		}
	}
}

/**
 * Processes an 'Instant Hit' trace and spawns any effects 
 * State scoped function. Override in proper state
 * Network: Server
 *
 * @param	HitActor = Actor hit by trace
 * @param	HitLocation = world location vector where HitActor was hit by trace
 * @param	HitNormal = hit normal vector
 * @param	HitInto	= TraceHitInfo struct returning useful info like component hit, bone, material..
 */
function ProcessInstantHit( Actor HitActor, vector AimDir, Vector HitLocation, Vector HitNormal, TraceHitInfo HitInfo )
{
	HitActor.TakeDamage(GetFireModeDamage(), Instigator, HitLocation, AimDir, GetFireModeDamageType(), HitInfo);
}

/** return true if should spawn tracer effect for instant trace hits */
function bool ShouldSpawnTracerFX()
{
	return false;
}

/**
 * Spawn tracer effect for instant hit shots
 *
 * @param HitLocation	Location instant trace did hit
 */
function SpawnTracerEffect( vector HitLocation );

/**
 * Returns the damage type of the current firing mode
 */
function class<WarDamageType> GetFireModeDamageType()
{
	// Default damage type for Warfare
	return FireModeInfoArray[CurrentFireMode].DamageTypeClass;
}

/**
 * returns the damage done by the current fire mode
 */
function float GetFireModeDamage()
{
	return FireModeInfoArray[CurrentFireMode].Damage;
}

/** 
 * hook to override Previous weapon call. 
 * For example the physics gun uses it to have mouse wheel change the distance of the held object.
 * Warning: only use in firing state, otherwise it breaks weapon switching
 * 
 * @return	true to override previous weapon.
 */
simulated function bool DoOverridePrevWeapon()
{
	return false;
}

/** 
 * hook to override Next weapon call. 
 * For example the physics gun uses it to have mouse wheel change the distance of the held object.
 * Warning: only use in firing state, otherwise it breaks weapon switching
 * 
 * @return	true to override Next weapon.
 */
simulated function bool DoOverrideNextWeapon()
{
	return false;
}

/**
 * Called from PlayerController::UpdateRotation() -> PlayerController::ProcessViewRotation() -> 
 * Pawn::ProcessViewRotation() -> WarInventoryManager::::ProcessViewRotation() to (pre)process player ViewRotation.
 * adds delta rot (player input), applies any limits and post-processing
 * returns the final ViewRotation set on PlayerController
 *
 * @param	DeltaTime, time since last frame
 * @param	ViewRotation, actual PlayerController view rotation
 * @input	out_DeltaRot, delta rotation to be applied on ViewRotation. Represents player's input.
 * @return	processed ViewRotation to be set on PlayerController.
 */
simulated function ProcessViewRotation( float DeltaTime, out rotator out_ViewRotation, out Rotator out_DeltaRot );

/**
 * Allows Weapon to force or deny player walking.
 * Sets Pawn.bIsWalking. true means Pawn walks.. affects velocity.
 *
 * @param	bNewIsWalking, new bIsWalking flag to set on Pawn.
 */
simulated function bool ForceSetWalking( bool bNewIsWalking )
{
	return bNewIsWalking;
}

/**
 * Overrides Player Targeting mode
 *
 * @return	true if TargetingMode should be ignored.
 */
simulated function bool OverrideTargetingMode()
{
	return false;
}

/**
 * Cycle to next available FireMode
 * Network: Local Player
 */
simulated function SetNextFireMode()
{
	local Byte NewFireMode;

	NewFireMode = SelectedFireMode;
	NewFireMode = ++NewFireMode % FiringStatesArray.Length;

	if ( SelectedFireMode != NewFireMode )
	{
		// Send new firemode to server if necessary
		if ( Role < Role_Authority )
			ServerSetFireMode( NewFireMode );

		SetFireMode( NewFireMode );
	}
}

/**
 * Set new fire mode on server
 * called from local player to server
 *
 * @param	NewFireMode to set
 */
function ServerSetFireMode( byte NewFireMode )
{
	SetFireMode( NewFireMode );
}

/**
 * Set a new fire mode
 *
 * @param	NewFireMode to set
 */
simulated function SetFireMode( byte NewFireMode )
{
	if( Level.NetMode != NM_DedicatedServer )
	{
		Owner.PlaySound( default.FireModeCycleSound );
	}
	SelectedFireMode = NewFireMode;
}

/**
 * Set internal fire mode (CurrentFireMode).
 * CurrentFireMode variable is replicated to all clients (but local player).
 * Event FireModeChanged() is called on all clients + server
 */
simulated function SetCurrentFireMode( byte NewFireMode )
{
	if ( NewFireMode != CurrentFireMode )
	{
		FireModeChanged( CurrentFireMode, NewFireMode );
		CurrentFireMode = NewFireMode;
	}
}

/**
 * Called when CurrentFireMode variable has changed.
 * Network: ALL
 */
simulated event FireModeChanged( byte OldFireModeNum, byte NewFireModeNum )
{
	if ( NewFireModeNum == RELOAD_FIREMODE )
	{
		// if we're reloading weapon, play desired effects.
		StartWeaponReloading();
	}
}

/**
 * Called by Player to force current weapon to be reloaded
 * Network: LocalPlayer
 */
simulated function ForceReload()
{
	// if weapon has no magazine, then just don't bother!
	if ( !CanReload() )
		return;

	bForceReload = true;
	if ( Role < Role_Authority )
		ServerForceReload();
}

/**
 * Called by Player to force current weapon to be reloaded
 * Network: Server
 */
function ServerForceReload()
{
	bForceReload = true;
}

/** @see Weapon::StartFire */
simulated function StartFire( byte FireModeNum )
{
	if ( FireModeNum != MELEE_FIREMODE )
	{
		// Force selected fire mode
		super.StartFire( SelectedFireMode );
	}
	else
	{
		super.StartFire( FireModeNum );
	}
}

/**
 * Returns true if weapon can potentially be reloaded
 *
 * @return	true if weapon can be reloaded
 */
simulated function bool CanReload()
{
	return (AmmoUsedCount>0 && MagazineSize>0);
}

/**
 * returns true if the weapon's magazine is empty and needs to be reloaded
 *
 * @return	true if weapon needs to reload
 */
simulated function bool NeedsToReload()
{
	return (CanReload() && AmmoUsedCount >= MagazineSize);
}

/**
 * Returns true if the weapon has ammo to fire for the current firemode
 *
 * @param	FireModeNum		fire mode number
 * 
 * @return	true if ammo is available for Firemode FireModeNum.
 */
simulated function bool HasAmmo( byte FireModeNum )
{
	// If we have fired more than Magazine capacity, then we need to reload
	if ( NeedsToReload() )
	{
		return false;
	}

	return super.HasAmmo( FireModeNum );
}

/** Returns true is ammo is critical for this weapon */
simulated function bool IsAmmoCritical()
{
	if( CriticalAmmoCount > 0 &&
		MagazineSize > 0 &&
		(MagazineSize - AmmoUsedCount) <= CriticalAmmoCount )
	{
		return true;
	}

	return false;
}

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
	if( Level.NetMode == NM_DedicatedServer )
	{
		return;
	}

	SkelMesh = SkeletalMeshComponent(Mesh);
	// Check we have access to mesh and animations
	if( SkelMesh == None || AnimNodeSequence(SkelMesh.Animations) == None )
	{
		//log("WarWeapon::PlayWeaponAnimation - no mesh or no animations :(");
		return;
	}

	// Set right animation sequence if needed
	if( AnimNodeSequence(SkelMesh.Animations).AnimSeq == None || AnimNodeSequence(SkelMesh.Animations).AnimSeq.SequenceName != Sequence )
	{	
		AnimNodeSequence(SkelMesh.Animations).SetAnim( Sequence );
	}

	if( AnimNodeSequence(SkelMesh.Animations).AnimSeq == None )
	{
		log("WarWeapon::PlayWeaponAnimation - AnimNodeSequence(Mesh.Animations).AnimSeq == None" @ Self @ "Instigator:" @ Instigator );
		return;
	}

	// Calculate new playing rate based on desired duration
	fNewRate = AnimNodeSequence(SkelMesh.Animations).AnimSeq.SequenceLength / fDesiredDuration;

	// Play Animation
	AnimNodeSequence(SkelMesh.Animations).PlayAnim( false, fNewRate );
}

/** @see Weapon::TimeWeaponFiring */
simulated function TimeWeaponFiring( byte FireModeNum )
{
	local float fDuration;

	// translate rate of fire into fire duration (seconds)
	fDuration = 60.f / GetFireModeRateOfFire(FireModeNum);

	// Timer is what calls SynchEvent()
	SetTimer( fDuration, false );
}

/** 
 * Returns rate of fire for firemode FireModeNum 
 *
 * @param	FireModeNum		FireMode to get fire rate from
 *
 * @return	rate of fire for specified firemode in Round Mer Minute (RPM)
 */
simulated function float GetFireModeRateOfFire( byte FireModeNum )
{
	return FireModeInfoArray[FireModeNum].FireRate;
}

/**
 * Set firing state of weapon, and fire proper events.
 *
 * @param	bNewState	true means weapon is now firing (send a StartFiringEvent), false means weapon stopped firing (sends a EndFiringEvent).
 */
simulated function SetWeaponFiringState( bool bNewState )
{
	// if same state as before, just skip
	if ( bIsFiring == bNewState )
	{
		return;
	}

	// Set new firing state. This is replicated to all (non local) clients.
	bIsFiring = bNewState;

	// Fire events for dedicated server and local player
	// Other clients get these called from native replication.
	if ( bIsFiring )
	{
		StartFiringEvent( CurrentFireMode );
	}
	else
	{
		EndFiringEvent( CurrentFireMode );
	}
}

/**
 * Called when weapon firing state switched to true.
 * Network: ALL
 *
 * @param	FireModeNum		Fire mode
 */
simulated event StartFiringEvent( byte FireModeNum );

/**
 * Called when weapon firing state switched to false.
 * Network: ALL
 *
 * @param	FireModeNum		Fire mode
 */
simulated event EndFiringEvent( byte FireModeNum );

/**
 * Called when a Shot is fired.
 * Called by C++ AWeapon::PostNetReceive() on the client if ShotCount incremented on the server
 * OR called locally for local player.
 * NOTE:	this is not a state scoped function (non local clients ignore the state the weapon is in)
 *			non local clients use the replicated CurrentFireMode (passed as parameter F from native code).
 * Network: ALL
 *
 * @param	FireMode F.
 */
simulated event PlayFireEffects( byte F )
{
	if ( F == MELEE_FIREMODE )
	{
		PlayMeleeAttacking();
		return;
	}
	
	PlayWeaponFireEffects( F );
}

/** @see AWarWeapon::PlayFireEffects */
simulated function PlayWeaponFireEffects( byte FireModeNum )
{
	local float fDuration;

	if( Instigator == None )
	{
		log("PlayWeaponFireEffects, Instigator == None" @ Self );
	}

	// Start muzzle flash effect
	if( Level.NetMode != NM_DedicatedServer )
	{
		// Play Weapon fire animation
		fDuration = 60.f / GetFireModeRateOfFire( FireModeNum );
		if( WeaponFireAnim != '' )
		{
			PlayWeaponAnimation( WeaponFireAnim, fDuration );
		}
		DoMuzzleFlashEffect( fDuration / 1.67 );

		// play weapon fire sound
		if( FireSound != None && Instigator != None )
		{
			Instigator.PlaySound( FireSound );
		}
	}

	// Add weapon recoil effect to player's viewrotation
	if( Instigator != None && Instigator.IsLocallyControlled() )
	{
		SetWeaponRecoil( FireModeInfoArray[FireModeNum].Recoil );
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
	if ( MuzzleFlashMesh != None )
	{
		MuzzleFlashMesh.SetHidden( false );
	}

	if ( MuzzleFlashLight != None )
		MuzzleFlashLight.Brightness = MuzzleFlashLightBrightness;

	SetTimer( fDuration, false, 'StopMuzzleFlashEffect' );
}

/**
 * Stop muzzle flash effect
 * Network: Local Player and Clients
 */
simulated function StopMuzzleFlashEffect()
{
	if ( MuzzleFlashMesh != None )
	{
		MuzzleFlashMesh.SetHidden(true);
	}

	if ( MuzzleFlashLight != None )
		MuzzleFlashLight.Brightness = 0;
}

/**
 * Attach Weapon Mesh, Weapon MuzzleFlash and Muzzle Flash Dynamic Light to a SkeletalMesh
 *
 * @param	SkeletalMeshComponent where to attach weapon
 * @param	BoneName of SkeletalMeshComponent where to attach the weapon
 */
simulated function AttachWeaponTo( SkeletalMeshComponent MeshCpnt, Name BoneName )
{
	// Attach Weapon Mesh to player skelmesh
	if ( MeshTranform != None )
	{
		MeshCpnt.AttachComponent(MeshTranform, BoneName);
	}
	else if( Mesh != None )
	{
		MeshCpnt.AttachComponent(Mesh, BoneName);
	}

	// Weapon Mesh Shadow
	if ( Mesh != None )
	{
		Mesh.ShadowParent = MeshCpnt;
	}

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

/**
 * Detach weapon from skeletal mesh
 *
 * @param	SkeletalMeshComponent weapon is attached to.
 */
simulated function DetachWeaponFrom( SkeletalMeshComponent MeshCpnt )
{
	// detach weapon mesh to player skelmesh
	if ( MeshTranform != None )
	{
		MeshCpnt.DetachComponent( MeshTranform );
	}
	else if ( Mesh != None )
	{
		MeshCpnt.DetachComponent( Mesh );
	}

	// Weapon Mesh Shadow
	if ( Mesh != None )
	{
		Mesh.ShadowParent = None;
	}

	// Muzzle Flash Mesh
	if ( MuzzleFlashTransform != None )
	{
		MeshCpnt.DetachComponent( MuzzleFlashTransform );
	}
    else if ( MuzzleFlashMesh != None )
	{
		MeshCpnt.DetachComponent( MuzzleFlashMesh );
	}

	// Muzzle Flash dynamic light
	if ( MuzzleFlashLightTransform != None )
	{
		MeshCpnt.DetachComponent( MuzzleFlashLightTransform );
	}
	else if ( MuzzleFlashLight != None )
	{
	    MeshCpnt.DetachComponent( MuzzleFlashLight );
	}
}

/** @see Weapon::TimeWeaponPutDown()  */
simulated function TimeWeaponPutDown()
{
	SetTimer( 0.05, false );
}

/** @see Weapon::TimeWeaponEquiping()  */
simulated function TimeWeaponEquiping()
{
	// Play weapon equiping sound locally
	if ( WeaponEquipSound != None && Level.NetMode != NM_DedicatedServer )
	{
		Instigator.PlaySound( WeaponEquipSound );
	}

	SetTimer( 0.05, false );
}

/**
 * Access to HUD and Canvas.
 * Event always called when the InventoryManager considers this Inventory Item currently "Active"
 * (for example active weapon)
 *
 * @param	HUD H
 */
simulated function ActiveRenderOverlays( HUD H )
{
	DrawWeaponInfo( H );
	DrawWeaponCrosshair( H );
}

/**
 * Draw weapon info on HUD
 *
 * @param	HUD H
 */
simulated function DrawWeaponInfo( HUD H )
{
	local WarHUD	wfHUD;
	local int		idx;

	//@fixme - clean this up post greenlight
	wfHUD = WarHUD(H);
	// update the firing mode label
	if( HUDElements.Length > 1 )
	{
		HUDElements[1].DrawLabel = Caps(FireModeInfoArray[SelectedFireMode].Name);
	}

	// draw the elements first
	for (idx = 0; idx < HUDElements.Length; idx++)
	{
		wfHUD.DrawHUDElement( HUDElements[idx] );
	}

	// Draw Weapon Name
	WeapNameHUDElemnt.DrawLabel = ItemName;
	wfHUD.DrawHUDElement( WeapNameHUDElemnt );

	// Draw Ammo
	DrawAmmo( H );
}

/** Draw Weapon Ammo on HUD */
simulated function DrawAmmo( HUD H )
{
	local	int		drawX, drawY, barCnt, idx;
	local	float	drawW, drawH, ammoPct, fPulse;

	// draw the ammo bars on top
	drawX	= 93;
	drawY	= 40;
	drawW	= 16 * 0.8f;
	drawH	= 32 * 0.8f;
	ammoPct = 1.f - (AmmoUsedCount/float(MagazineSize));
	barCnt	= 14;
	
	H.Canvas.DrawColor = MakeColor(255,255,255,255);

	for (idx = 0; idx < barCnt; idx++)
	{
		H.Canvas.SetPos(drawX + idx * 6,drawY);
		if (idx/float(barCnt) < ammoPct)
		{		
			if( IsAmmoCritical() )
			{
				if( HUDAmmoMaterialInstance == None )
				{
					HUDAmmoMaterialInstance = new(None) class'MaterialInstanceConstant';
					HUDAmmoMaterialInstance.SetParent( HUDAmmoMaterial );
				}
				if( HUDAmmoMaterialInstance != None )
				{
					fPulse = (Level.TimeSeconds * 3) % 1.f;
					HUDAmmoMaterialInstance.SetVectorParameterValue('HUD_Weapon_Ammo_Critical_Color', MakeLinearColor(fPulse*2,0,0,fPulse));
					H.Canvas.DrawMaterialTile( HUDAmmoMaterialInstance, drawW, drawH);
				}
			}
			else
			{
				H.Canvas.DrawMaterialTile( Material'WarfareHudGFX.Weapons.HUD_Weapon_Ammo_ON', drawW, drawH);
			}
		}
		else
		{
			H.Canvas.DrawMaterialTile( Material'WarfareHudGFX.Weapons.HUD_Weapon_Ammo_OFF', drawW, drawH);
		}
	}
}

/**
 * Draw various weapon debug info on HUD
 */
simulated function DrawWeaponDebug( HUD H )
{
	local float		XL, YL, YPos, XOffset, fAccuracy;
	local String	DebugText;

    H.Canvas.Font = class'Engine'.Default.SmallFont;
	
	// Draw weapon info
	H.Canvas.DrawColor	= H.ConsoleColor;
	H.Canvas.StrLen("MAG", XL, YL);
	YPos		= H.SizeY - YL*4;
	XOffset		= XL * 0.33;

	DebugText = GetHumanReadableName() @ "State:"$GetStateName();
	H.Canvas.StrLen(DebugText, XL, YL);
	H.Canvas.SetPos( H.SizeX - XL - XOffset, YPos );
	H.Canvas.DrawText( DebugText, false );
	
	DebugText = "FireMode:" $(SelectedFireMode+1) $ "(" $(CurrentFireMode+1) $")/"$FiringStatesArray.Length @ FireModeInfoArray[SelectedFireMode].Name;
	YPos += YL;
	H.Canvas.StrLen(DebugText, XL, YL);
	H.Canvas.SetPos( H.SizeX - XL - XOffset, YPos );
	H.Canvas.DrawText( DebugText, false );
	
	DebugText = "ShotCount:" $ ShotCount;
	if ( MagazineSize > 0 )
		DebugText = DebugText @ "MagazineSize:" $ MagazineSize;
	YPos += YL;
	H.Canvas.StrLen(DebugText, XL, YL);
	H.Canvas.SetPos( H.SizeX - XL - XOffset, YPos );
	H.Canvas.DrawText( DebugText, false );

	fAccuracy = WarInventoryManager(InvManager).GetPlayerAccuracy();
	DebugText = "Player InAccuracy:" $ fAccuracy @ "FM:" $ FireModeInfoArray[SelectedFireMode].Inaccuracy $ "d";
	YPos += YL;
	H.Canvas.StrLen(DebugText, XL, YL);
	H.Canvas.SetPos( H.SizeX - XL - XOffset, YPos );
	H.Canvas.DrawText( DebugText, false );
}

/** Draw Weapon Crosshair */
simulated function DrawWeaponCrosshair( Hud H )
{
	local int	idx;
	local Color	CHDrawColor;
	local float	Accuracy, AccuracyScale;

	// DrawColor
	if( !WarPC(Instigator.Controller).CanFireWeapon() )
	{
		return;
		CHDrawColor = MakeColor(255, 0, 0, 255);	// can't fire
	}
	else if ( !WarPC(Instigator.Controller).IsAimingAtEnemyInRange() )
	{
		CHDrawColor = MakeColor(182, 200, 255, 255);	// enemy targeted
	}
	else
	{
		CHDrawColor = MakeColor(118, 157, 255, 255);	// normal
	}

	Accuracy		= GetCrosshairAccuracy();
	AccuracyScale	= 1.f + Accuracy * CrosshairExpandStrength;

	for(idx=0; idx<CrosshairElements.Length; idx++)
	{
		CrosshairElements[idx].DrawColor = CHDrawColor;

		CrosshairElements[idx].DrawX		= (default.CrosshairElements[idx].DrawX - 0.5) * AccuracyScale + 0.5;
		CrosshairElements[idx].DrawY		= (default.CrosshairElements[idx].DrawY - 0.5) * AccuracyScale + 0.5;
		//CrosshairElements[idx].DrawScale	= AccuracyScale;

		WarHUD(H).DrawHUDElement( CrosshairElements[idx] );
	}
}

/**
 * returns inaccuracy scaling for crosshair
 * 0.f = no accuracy. >0.f = inaccurate
 */
simulated function float GetCrosshairAccuracy()
{
	if( FireModeInfoArray[SelectedFireMode].Inaccuracy > 0.f )
	{
		return FireModeInfoArray[SelectedFireMode].Inaccuracy * WarInventoryManager(InvManager).GetPlayerAccuracy();
	}

	return 0.f;
}

/** 
 * State Active 
 * When a weapon is in the active state, it's up, ready but not doing anything. (idle)
 */
state Active
{
	/**
	* Called by Player to force current weapon to be reloaded
	* Network: LocalPlayer
	*/
	simulated function ForceReload()
	{
		// if weapon has no magazine, then just don't bother!
		if ( !CanReload() )
			return;

		GotoState('Reloading');
		if ( Role < Role_Authority )
			ServerForceReload();
	}

	/**
	* Called by Player to force current weapon to be reloaded
	* Network: Server
	*/
	function ServerForceReload()
	{
		GotoState('Reloading');
	}

	simulated function BeginState()
	{
		if ( bForceReload || NeedsToReload() )
		{
			GotoState('Reloading');
			return;
		}
	}
}

/** @see Weapon::ConsumeAmmo */
function ConsumeAmmo( byte FireModeNum )
{
	AmmoUsedCount++;
}

/** 
 * State Firing
 * Basic default firing state. Handles continuous, auto firing behavior.
 */
state Firing
{
	/** @see Weapon::LocalFire */
	simulated function LocalFire( byte FireModeNum )
	{
		super.LocalFire( FireModeNum );
		ConsShotCount++;
	}

	/** Called when weapon enters firing state */
	simulated function BeginState()
	{
		SetWeaponFiringState( true );
		super.BeginState();
	}

	/** Called when weapon leaves firing state */
	simulated function EndState()
	{
		super.EndState();
		SetWeaponFiringState( false );
		ConsShotCount = 0;
	}
}

/**
 * Play weapon reloading effects.
 * Called when weapon enters reloading state.
 * Network: ALL
 */
simulated function StartWeaponReloading()
{
	local AnimNodeCrossfader	ACrossfader;
	//local AnimNodeSequence		ASequence;
	local float					fNewRate;
	local AnimSequence			AnimSeq;

	AnimSeq = Instigator.Mesh.FindAnimSequence( PawnReloadAnim );
	fNewRate = AnimSeq.SequenceLength / ReloadDuration;

	// update Standing Fire Sequence
	ACrossfader = AnimNodeCrossfader( WarPawn(Instigator).Mesh.Animations.FindAnimNode('anFire') );
	if ( ACrossfader != None )
	{
		// play idle
		/*
		ASequence = ACrossFader.GetActiveChild();
		ASequence.SetAnim( PawnIdleAnim );
		ASequence.PlayAnim( true, 1.f, 0.f );
		*/
		// blend to firing animation
		ACrossfader.PlayOneShotAnim( PawnReloadAnim, 0.15, 0.2, false, fNewRate );
	}

	// update standing idle animation
	// that's a hack to counter upper body per bone blend
	ACrossfader = AnimNodeCrossfader( WarPawn(Instigator).Mesh.Animations.FindAnimNode('anStandIdle') );
	if ( ACrossfader != None && ACrossFader.GetActiveAnimSeq() != PawnReloadAnim )
	{
		ACrossfader.PlayOneShotAnim( PawnReloadAnim, 0.15, 0.2, false, fNewRate );
	}

	// Play weapon reloading sound
	if ( Level.NetMode != NM_DedicatedServer )
	{
		Instigator.PlaySound( WeaponReloadSound );
	}
}

/** Called on server to tell player is putting down weapon */
function ServerPutDown()
{
	bForceFire = false;
	GotoState('Inactive');
}

/**
 * Is the weapon currently reloading?  Only if in the Reloading state.
 */
event bool IsReloading()
{
	return false;
}

/**
 * State Reloading
 * State the weapon is in when it is being reloaded (current magazine replaced with a new one, related animations and effects played).
 */
state Reloading
{
	/**
	 * Overridden to return true.  Woo.
	 */
	event bool IsReloading()
	{
		return true;
	}

	/** @ WarWeapon::ForceReload */
	simulated function ForceReload();

	/** @see Weapon::StartFire */
	simulated function StartFire( byte FireModeNum )
	{
		if ( Role < Role_Authority )
		{
			ServerStartFire( FireModeNum );
		}
	}

	/** @see Weapon::ServerStartFire */
	function ServerStartFire( byte FireModeNum )
	{
		bForceFire		= true;
		ForceFireMode	= FireModeNum;
	}

	/** @see Weapon::TryPutDown() */
	simulated function bool TryPutDown()
	{
		if ( Role < Role_Authority )
		{
			ServerPutDown();
		}

		bWeaponPutDown = true;
		GotoState('PuttingDown');
		return true;
	}

	simulated function SynchEvent()
	{
		AmmoUsedCount = 0;
		GotoState('Active');
	}

	simulated function BeginState()
	{
		SetCurrentFireMode( RELOAD_FIREMODE );
		SetWeaponFiringState( true );	// hack - to blend weapon animation while running. fix when designing new player anim tree
		SetTimer( ReloadDuration, false );
	}

	simulated function EndState()
	{
		bForceReload = false;
		SetCurrentFireMode( SelectedFireMode );
		SetWeaponFiringState( false );	// hack - to blend weapon animation while running. fix when designing new player anim tree

		if ( bWeaponPutDown )
		{
			GotoState('PuttingDown');
			return;
		}

		// if pressing fire (local player), fire
		if ( Instigator.IsLocallyControlled() && Instigator.IsPressingFire(SelectedFireMode) /*&& HasAmmo(SelectedFireMode)*/ )		
		{
			Global.StartFire( SelectedFireMode );
			return;
		}

		// pressing fire, dedicated server
		if ( !Instigator.IsLocallyControlled() && bForceFire )
		{
			bForceFire = false;
			Global.ServerStartFire( ForceFireMode );
			return;
		}
	}
}

/**
 * return true is player can perform a melee attack with this weapon
 */
simulated function bool CanPerformMeleeAttack()
{
	if ( IsIdle() )
	{
		return true;
	}
	return false;
}

/**
 * Start a melee attack with this weapon
 * @network		LocalPlayer
 */
simulated function StartMeleeAttack()
{
	if ( CanPerformMeleeAttack() )
	{
		StartFire( MELEE_FIREMODE );
	}
}

/**
 * Play melee attack effects.
 * Called when melee attack starts.
 * This is not a state scoped function.
 * Network: ALL
 *
 * @see AWarWeapon::PlayFireEffects()
 */
simulated function PlayMeleeAttacking()
{
	local AnimNodeCrossfader	ACrossfader;
	//local AnimNodeSequence		ASequence;
	local float					fNewRate;
	local AnimSequence			AnimSeq;

	AnimSeq = Instigator.Mesh.FindAnimSequence( PawnMeleeAnim );
	if ( AnimSeq == None )
	{
		log("WarWeapon::PlayMeleeAttacking, Animation not found" @ PawnMeleeAnim);
		return;
	}

	fNewRate = AnimSeq.SequenceLength / fMeleeDuration;

	// update Standing Fire Sequence
	ACrossfader = AnimNodeCrossfader( WarPawn(Instigator).Mesh.Animations.FindAnimNode('anFire') );
	if ( ACrossfader != None )
	{
		/*
		// play idle
		ASequence = ACrossFader.GetActiveChild();
		ASequence.SetAnim( PawnIdleAnim );
		ASequence.PlayAnim( true, 1.f, 0.f );
		*/
		// blend to firing animation
		ACrossfader.PlayOneShotAnim( PawnMeleeAnim, 0.15, 0.3, false, fNewRate );
	}

	// update standing idle animation
	// that's a hack to counter upper body per bone blend
	ACrossfader = AnimNodeCrossfader( WarPawn(Instigator).Mesh.Animations.FindAnimNode('anStandIdle') );
	if ( ACrossfader != None && ACrossFader.GetActiveAnimSeq() != PawnMeleeAnim )
	{
		ACrossfader.PlayOneShotAnim( PawnMeleeAnim, 0.15, 0.3, false, fNewRate );
	}
}

/** @see Weapon::SendToFiringState */
simulated function SendToFiringState( byte FireModeNum )
{
	if ( FireModeNum == MELEE_FIREMODE )
	{
		SetCurrentFireMode( MELEE_FIREMODE );
		GotoState('MeleeAttacking');
		return;
	}

	// make sure fire mode is valid
	if ( FireModeNum >= FiringStatesArray.Length )
	{
		WeaponLog("Invalid FireModeNum", "WarWeapon::SendToFiringState");
		GotoState('Active');
		return;
	}

	SetCurrentFireMode( FireModeNum );
	GotoState( FiringStatesArray[FireModeNum] );
}

/**
 * State when player is using this weapon for a melee attack
 */
state MeleeAttacking
{
	/** no tracer effect for melee instant hit trace */
	function SpawnTracerEffect( vector HitLocation );

	/** return true is player can perform a melee attack with this weapon */
	simulated function bool CanPerformMeleeAttack()
	{	// if we're here.. player can keep on spamming! :)
		return true;
	}

	/** render something, so we know we're doing a melee attack */
	simulated function ActiveRenderOverlays( HUD H )
	{
		local float		XL, YL;
		local String	Text;

		Global.ActiveRenderOverlays( H );
		
		Text = "MELEE ATTACK";
		H.Canvas.StrLen( Text , XL, YL);
		H.Canvas.SetPos( H.Canvas.SizeX*0.5 - XL*0.5, H.Canvas.SizeY*0.67 );
		XL = FRand() * 255.f;
		H.Canvas.DrawColor = MakeColor( XL, XL, XL, 255 );
		H.Canvas.DrawText( Text );
	}

	/** @see Weapon::LocalFire() */
	simulated function LocalFire( byte FireModeNum )
	{
		if ( Role == Role_Authority )
		{
			FireAmmunition( FireModeNum );
		}

		ShotCount++;
		PlayFireEffects( FireModeNum );

		if ( Role == Role_Authority )
			Owner.MakeNoise(1.0);
	}

	/** @see Weapon::GetTraceRange() */
	simulated function float GetTraceRange()
	{
		return fMeleeRange;
	}

	/** @see WarWeapon::GetFireModeDamage() */
	function float GetFireModeDamage()
	{
		return MeleeDamage;
	}

	/** @see WarWeapon:: GetFireModeDamageType() */
	function class<WarDamageType> GetFireModeDamageType()
	{
		// Default damage type for Warfare Melee
		return class'WarDamageTypeMelee';
	}

	/** @see Weapon::ProcessInstantHit */
	function ProcessInstantHit( Actor HitActor, vector AimDir, Vector HitLocation, Vector HitNormal, TraceHitInfo HitInfo )
	{
		HitActor.TakeDamage(GetFireModeDamage(), Instigator, HitLocation, AimDir, GetFireModeDamageType(), HitInfo);

		if ( Pawn(HitActor) == None )
		{
			Spawn( class'Emit_BulletImpact',,, HitLocation + HitNormal*3, rotator(HitNormal) );
		}
	}

	/** @see Weapon::SynchEvent */
	simulated function SynchEvent()
	{
		if ( bWeaponPutDown )
		{
			GotoState('PuttingDown');
			return;
		}

		GotoState('Active');
	}

	simulated function BeginState()
	{
		// Time Melee Attack
		SetTimer( fMeleeDuration, false );
		LocalFire( MELEE_FIREMODE );
		SetWeaponFiringState( true );	// hack - to blend weapon animation while running. fix when designing new player anim tree
	}

	simulated function EndState()
	{
		SetWeaponFiringState( false );	// hack - to blend weapon animation while running. fix when designing new player anim tree
	}
}

defaultproperties
{
	CrosshairExpandStrength=0.25

	FireOffset=(X=-80,Y=3,Z=13)
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(FireRate=240)
	
	PawnFiringAnim=Fire_Auto_01
	PawnIdleAnim=Idle_Rdy_01
	PawnReloadAnim=PSTIdRd01
	PawnMeleeAnim=ATTBkGl01

	FireModeCycleSound=SoundCue'InterfaceSounds.GenericClick1Cue'
	WeaponEquipSound=SoundCue'LD_TempUTSounds.WeaponEquip'
	WeaponReloadSound=SoundCue'LD_TempUTSounds.WeaponReload'
	ReloadDuration=0.01
	MuzzleFlashLightBrightness=2.0

	fMeleeRange=128
	MeleeDamage=50
	fMeleeDuration=0.5f

	HUDElements(0)=(bEnabled=True,DrawX=0.045000,DrawY=0.024000,bCenterX=False,bCenterY=False,DrawColor=(B=255,G=255,R=255,A=128),DrawW=256,DrawH=128,DrawIcon=Texture2D'WarfareHudGFX.HUD.HUD_Weapon_T_AssaultRifleIcon',DrawU=0,DrawV=0,DrawUSize=256,DrawVSize=128,DrawScale=0.700000,DrawFont=None,DrawLabel="")
	HUDElements(1)=(bEnabled=True,DrawX=0.145000,DrawY=0.125000,bCenterX=False,bCenterY=False,DrawColor=(B=200,G=200,R=200,A=128),DrawW=0,DrawH=0,DrawIcon=None,DrawU=0,DrawV=0,DrawUSize=-1,DrawVSize=-1,DrawScale=1.000000,DrawFont=Font'EngineFonts.TinyFont',DrawLabel="AUTO")
	
	WeapNameHUDElemnt=(bEnabled=True,DrawX=0.075,DrawY=0.18,bCenterX=False,bCenterY=False,DrawColor=(B=200,G=200,R=200,A=128),DrawW=0,DrawH=0,DrawIcon=None,DrawU=0,DrawV=0,DrawUSize=-1,DrawVSize=-1,DrawScale=1.000000,DrawFont=Font'EngineFonts.TinyFont',DrawLabel="Weapon")

	CrosshairElements(0)=(bEnabled=True,DrawX=0.535000,DrawY=0.500000,bCenterX=True,bCenterY=True,DrawColor=(B=255,G=255,R=255,A=255),DrawW=16,DrawH=16,DrawIcon=None,DrawMaterial=Material'WarfareHudGFX.HUD.LD_Crosshair_R_Mat',DrawU=0.000000,DrawV=0.000000,DrawUSize=1.000000,DrawVSize=1.000000,DrawScale=1.000000,DrawFont=None,DrawLabel="")
	CrosshairElements(1)=(bEnabled=True,DrawX=0.465000,DrawY=0.500000,bCenterX=True,bCenterY=True,DrawColor=(B=255,G=255,R=255,A=255),DrawW=16,DrawH=16,DrawIcon=None,DrawMaterial=Material'WarfareHudGFX.HUD.LD_Crosshair_R_Mat',DrawU=0.000000,DrawV=0.000000,DrawUSize=-1.000000,DrawVSize=1.000000,DrawScale=1.000000,DrawFont=None,DrawLabel="")
	CrosshairElements(2)=(bEnabled=True,DrawX=0.517000,DrawY=0.518000,bCenterX=True,bCenterY=True,DrawColor=(B=255,G=255,R=255,A=255),DrawW=32,DrawH=32,DrawIcon=None,DrawMaterial=Material'WarfareHudGFX.HUD.LD_Crosshair_BR_Mat',DrawU=0.000000,DrawV=0.000000,DrawUSize=1.000000,DrawVSize=1.000000,DrawScale=1.000000,DrawFont=None,DrawLabel="")
	CrosshairElements(3)=(bEnabled=True,DrawX=0.483000,DrawY=0.518000,bCenterX=True,bCenterY=True,DrawColor=(B=255,G=255,R=255,A=255),DrawW=32,DrawH=32,DrawIcon=None,DrawMaterial=Material'WarfareHudGFX.HUD.LD_Crosshair_BR_Mat',DrawU=0.000000,DrawV=0.000000,DrawUSize=-1.000000,DrawVSize=1.000000,DrawScale=1.000000,DrawFont=None,DrawLabel="")
	CrosshairElements(4)=(bEnabled=True,DrawX=0.517000,DrawY=0.482000,bCenterX=True,bCenterY=True,DrawColor=(B=255,G=255,R=255,A=255),DrawW=32,DrawH=32,DrawIcon=None,DrawMaterial=Material'WarfareHudGFX.HUD.LD_Crosshair_BR_Mat',DrawU=0.000000,DrawV=0.000000,DrawUSize=1.000000,DrawVSize=-1.000000,DrawScale=1.000000,DrawFont=None,DrawLabel="")
	CrosshairElements(5)=(bEnabled=True,DrawX=0.483000,DrawY=0.482000,bCenterX=True,bCenterY=True,DrawColor=(B=255,G=255,R=255,A=255),DrawW=32,DrawH=32,DrawIcon=None,DrawMaterial=Material'WarfareHudGFX.HUD.LD_Crosshair_BR_Mat',DrawU=0.000000,DrawV=0.000000,DrawUSize=-1.000000,DrawVSize=-1.000000,DrawScale=1.000000,DrawFont=None,DrawLabel="")
	
	HUDAmmoMaterial=Material'WarfareHudGFX.Weapons.HUD_Weapon_Ammo_Critical'
}
