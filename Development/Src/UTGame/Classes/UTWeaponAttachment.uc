class UTWeaponAttachment extends Actor
	abstract;

//TODO: Add support for changing the shadows on the fly.

/*********************************************************************************************
 Animations and Sounds
********************************************************************************************* */

/*
var		byte	FlashCount;			// when incremented, draw muzzle flash for current frame
var		byte	FiringMode;			// replicated to identify what type of firing/reload animations to play
var		byte	SpawnHitCount;		// when incremented, spawn hit effect at mHitLocation
var		bool	bAutoFire;			// When set to true.. begin auto fire sequence (used to play looping anims)
var		vector  mHitLocation;		// used for spawning hit effects client side
var		bool	bMatchWeapons;		// for team beacons (link gun potential links)
var		color	BeaconColor;		// if bMatchWeapons, what color team beacon should be
*/

var class<Actor> SplashEffect;

/*********************************************************************************************
 Weapon Components
********************************************************************************************* */

/** Weapon SkelMesh */
var	MeshComponent			Mesh;

/** weapon skelmesh tranform component */
var	TransformComponent		MeshTransform;

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
var float					MuzzleFlashLightDuration;

//replication
//{
//	// Things the server should send to the client.
//	reliable if( bNetDirty && !bNetOwner && (Role==ROLE_Authority) )
//		FlashCount, FiringMode, bAutoFire;
//
//	reliable if ( bNetDirty && (Role==ROLE_Authority) )
//		mHitLocation, SpawnHitCount;
//}


/**
 * Called on a client, this function Attaches the WeaponAttachment
 * to the Mesh.
 */
function AttachTo( SkeletalMeshComponent MeshCpnt, name BoneName )
{
	// Attach Weapon mesh to player skelmesh

	if ( MeshTransform != None )
		MeshCpnt.AttachComponent(MeshTransform, BoneName);
	else
		MeshCpnt.AttachComponent(Mesh, BoneName);

	// Weapon Mesh Shadow

	if ( Mesh != None )
		Mesh.ShadowParent = MeshCpnt;

	// Muzzle Flash mesh

	if ( MuzzleFlashTransform != None )
		MeshCpnt.AttachComponent(MuzzleFlashTransform, BoneName);
    else if ( MuzzleFlashMesh != None )
		MeshCpnt.AttachComponent(MuzzleFlashMesh, BoneName);

	// Muzzle Flash dynamic light

	if ( MuzzleFlashLightTransform != None )
		MeshCpnt.AttachComponent(MuzzleFlashLightTransform, BoneName);
	else if ( MuzzleFlashLight != None )
	    MeshCpnt.AttachComponent(MuzzleFlashLight, BoneName);

	// If our pawn is locally controlled, respect the behindview flag.

	if (Instigator.IsLocallyControlled() && UTPawn(Instigator) != none && UTPawn(Instigator).bBehindView)
	{
		mesh.bOwnerNoSee = !UTPawn(Instigator).bBehindView;
	}

}

/**
 * Detach weapon from skeletal mesh
 */

simulated function DetachFrom( SkeletalMeshComponent MeshCpnt )
{
	// detach weapon mesh to player skelmesh

	if ( MeshTransform != None )
		MeshCpnt.DetachComponent( MeshTransform );
	else if ( Mesh != None )
		MeshCpnt.DetachComponent( mesh );

	// Weapon Mesh Shadow

	if ( Mesh != None )
		Mesh.ShadowParent = None;

	// Muzzle Flash Mesh

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


/**
 * Turns the MuzzleFlashlight off
 */

simulated event MuzzleFlashLightTimer()
{
	if ( MuzzleFlashLight!=None )
		MuzzleFlashLight.bEnabled = false;
}

/**
 * Causes the muzzle flashlight to turn on and setup a time to
 * turn it back off again.
 */

simulated event CauseMuzzleFlashLight()
{
	if (MuzzleFlashLight==None) // Quick exit if there isn't a light
		return;

	MuzzleFlashLight.Brightness = MuzzleFlashLightBrightness;
	MuzzleFlashLight.bEnabled = true;

	// Set when to turn it off.

	SetTimer(MuzzleFlashLightDuration,false,'MuzzleFlashLightTimer');
}


/**
 * Spawn all of the effects that will be seen in behindview/remote clients.  This
 * function is called from the pawn, and should only be called when on a remote client or
 * if the local client is in a 3rd person mode.
*/
simulated event ThirdPersonFireEffects()
{

	// Light it up

	CauseMuzzleFlashLight();

	// Have pawn play firing anim
	if ( Instigator != none && UTPawn(Instigator)!=None )
	{
		// FIXME: Come up with a better way to support the animspeed.

		if ( UTPawn(Instigator).FiringMode == 1 )
			Instigator.PlayFiring(1.0,'1');
		else
			Instigator.PlayFiring(1.0,'0');
	}
}

/**
 * Spawn any effects that occur at the impact point.  It's called from the pawn.
 */

simulated event ImpactEffects();	// Should be subclassed.


/* FIXMESTEVE
simulated function CheckForSplash()
{
	local Actor HitActor;
	local vector HitNormal, HitLocation;

	if ( !Level.bDropDetail && (Level.DetailMode != DM_Low) && (SplashEffect != None) && !Instigator.PhysicsVolume.bWaterVolume )
	{
		// check for splash
		HitActor = Trace(HitLocation,HitNormal,mHitLocation,Instigator.Location,true,,true);
		if ( (PhysicsVolume(HitActor) != None) && PhysicsVolume(HitActor).bWaterVolume )
			Spawn(SplashEffect,,,HitLocation,rot(16384,0,0));
	}
}
*/


defaultproperties
{
	NetUpdateFrequency=10
	RemoteRole=ROLE_SimulatedProxy
	bReplicateInstigator=true
//	BeaconColor=(G=255,A=255)
	MuzzleFlashLightDuration=0.1
}
