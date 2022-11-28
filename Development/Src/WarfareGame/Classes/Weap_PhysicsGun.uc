/** 
 * Physics Gun
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class Weap_PhysicsGun extends WarWeapon;

var()				float			WeaponImpulse;
var()				float			HoldDistanceMin;
var()				float			HoldDistanceMax;
var()				float			ThrowImpulse;
var()				float			ChangeHoldDistanceIncrement;

var					RB_Handle		PhysicsGrabber;
var					float			HoldDistance;
var					Quat			HoldOrientation;

state Firing
{
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
		Spawn( class'Emit_BulletImpact',,, HitLocation + HitNormal*3, rotator(HitNormal) );

		// add impulse
		if ( HitInfo.HitComponent != None )
			HitInfo.HitComponent.AddImpulse(AimDir * WeaponImpulse, HitLocation, HitInfo.BoneName);
	}
}

/* holding an object */
state Holding 
{
	function ServerStartFire( byte FireModeNum )
	{
		local Rotator		Aim;

		// If holding and object, throw it
		if ( PhysicsGrabber.GrabbedComponent != None )
		{
			Aim	= GetAdjustedAim( Instigator.GetWeaponStartTraceLocation() );
			//PhysicsGrabber.GrabbedComponent.AddImpulse(Vector(Aim) * ThrowImpulse * 200, , PhysicsGrabber.GrabbedBoneName);
			PhysicsGrabber.ReleaseComponent();
		}
	}

	simulated function bool DoOverridePrevWeapon()
	{
		HoldDistance += ChangeHoldDistanceIncrement;
		HoldDistance = FMin(HoldDistance, HoldDistanceMax);
		return true;
	}

	simulated function bool DoOverrideNextWeapon()
	{
		HoldDistance -= ChangeHoldDistanceIncrement;
		HoldDistance = FMax(HoldDistance, HoldDistanceMin);
		return true;
	}

	simulated function BeginState()
	{
		local vector					StartShot, EndShot;
		local vector					HitLocation, HitNormal, Extent;
		local actor						HitActor;
		local float						HitDistance;
		local Quat						PawnQuat, InvPawnQuat, ActorQuat;
		local TraceHitInfo				HitInfo;
		local SkeletalMeshComponent		SkelComp;
		local Rotator					Aim;

		// Do ray check and grab actor
		StartShot	= Instigator.GetWeaponStartTraceLocation();
		Aim			= GetAdjustedAim( StartShot );
		EndShot		= StartShot + (10000.0 * Vector(Aim));
		Extent		= vect(0,0,0);
		HitActor	= Trace(HitLocation, HitNormal, EndShot, StartShot, True, Extent, HitInfo);
		HitDistance = VSize(HitLocation - StartShot);

		if( HitActor != None &&
			HitActor != Level &&
			HitDistance > HoldDistanceMin &&
			HitDistance < HoldDistanceMax )
		{
			// If grabbing a bone of a skeletal mesh, dont constrain orientation.
			PhysicsGrabber.GrabComponent(HitInfo.HitComponent, HitInfo.BoneName, HitLocation, PlayerController(Instigator.Controller).bRun==0);

			// If we succesfully grabbed something, store some details.
			if (PhysicsGrabber.GrabbedComponent != None)
			{
				HoldDistance	= HitDistance;
				PawnQuat		= QuatFromRotator( Rotation );
				InvPawnQuat		= QuatInvert( PawnQuat );

				if ( HitInfo.BoneName != '' )
				{
					SkelComp = SkeletalMeshComponent(HitInfo.HitComponent);
					ActorQuat = SkelComp.GetBoneQuaternion(HitInfo.BoneName);
				}
				else
					ActorQuat = QuatFromRotator( PhysicsGrabber.GrabbedComponent.Owner.Rotation );

				HoldOrientation = QuatProduct(InvPawnQuat, ActorQuat);
			}
		}
	}

	simulated function Tick( float DeltaTime )
	{
		local vector	NewHandlePos, StartLoc;
		local Quat		PawnQuat, NewHandleOrientation;
		local Rotator	Aim;

		if ( PhysicsGrabber.GrabbedComponent == None )
		{
			GotoState( 'Active' );
			return;
		}

		PhysicsGrabber.GrabbedComponent.WakeRigidBody( PhysicsGrabber.GrabbedBoneName );

		// Update handle position on grabbed actor.
		StartLoc		= Instigator.GetWeaponStartTraceLocation();
		Aim				= GetAdjustedAim( StartLoc );
		NewHandlePos	= StartLoc + (HoldDistance * Vector(Aim));
		PhysicsGrabber.SetLocation( NewHandlePos );

		// Update handle orientation on grabbed actor.
		PawnQuat				= QuatFromRotator( Rotation );
		NewHandleOrientation	= QuatProduct(PawnQuat, HoldOrientation);
		PhysicsGrabber.SetOrientation( NewHandleOrientation );
	}

	simulated function EndState()
	{
		if ( PhysicsGrabber.GrabbedComponent != None )
			PhysicsGrabber.ReleaseComponent();
	}
}

defaultproperties
{
	ItemName="Physics Gun"
	WeaponFireAnim=Shoot
	FireSound=SoundCue'COGAssaultRifleAmmo_SoundCue'
	FiringStatesArray(0)=Firing
	FireModeInfoArray(0)=(FireRate=480,Name="Poke")
	FiringStatesArray(1)=Holding
	FireModeInfoArray(1)=(FireRate=480,Name="Grab")

	HoldDistanceMin=50.0
	HoldDistanceMax=750.0
	WeaponImpulse=600.0
	ThrowImpulse=800.0
	ChangeHoldDistanceIncrement=50.0

	Begin Object Class=RB_Handle Name=RB_Handle0
	End Object
	Components.Add(RB_Handle0)
	PhysicsGrabber=RB_Handle0

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
}