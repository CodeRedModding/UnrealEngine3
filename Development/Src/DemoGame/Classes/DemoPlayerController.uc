/**
 * DemoPlayerController
 * Demo specific playercontroller
 * implements physics spectating state for demoing purpose
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class DemoPlayerController extends PlayerController
	config(Game);

var()				float			WeaponImpulse;
var()				float			HoldDistanceMin;
var()				float			HoldDistanceMax;
var()				float			ThrowImpulse;
var()				float			ChangeHoldDistanceIncrement;

var					RB_Handle		PhysicsGrabber;
var					float			HoldDistance;
var					Quat			HoldOrientation;

exec function TogglePhysicsMode()
{
	local Rotator	CurrentRot;

	CurrentRot = Rotation;
	PlayerReplicationInfo.bOnlySpectator = true;
	if ( Pawn != None )
	{
		Pawn.TakeDamage(1000,None,Location,vect(0,0,0),class'DemoDamageType');
		SetViewTarget( Self );
	}
	SetRotation( CurrentRot );
	GotoState('PhysicsSpectator');
}

/** Start as PhysicsSpectator by default */
auto state PlayerWaiting
{
Begin:
	PlayerReplicationInfo.bOnlySpectator = false;
	Level.Game.bRestartLevel = false;
	Level.Game.RestartPlayer( Self );
	Level.Game.bRestartLevel = true;

	Pawn.Mesh.bOwnerNoSee = false;
}

exec function PrevWeapon()
{
	HoldDistance += ChangeHoldDistanceIncrement;
	HoldDistance = FMin(HoldDistance, HoldDistanceMax);
}

exec function NextWeapon()
{
	HoldDistance -= ChangeHoldDistanceIncrement;
	HoldDistance = FMax(HoldDistance, HoldDistanceMin);
}

exec function StartFire( optional byte FireModeNum )
{
	local vector		CamLoc, StartShot, EndShot, X, Y, Z;
	local vector		HitLocation, HitNormal, ZeroVec;
	local actor			HitActor;
	local TraceHitInfo	HitInfo;
	local rotator		CamRot;

	GetPlayerViewPoint(CamLoc, CamRot);

	GetAxes( CamRot, X, Y, Z );
	ZeroVec = vect(0,0,0);

	if ( PhysicsGrabber.GrabbedComponent == None )
	{
		// Do simple line check then apply impulse
		StartShot	= CamLoc;
		EndShot		= StartShot + (10000.0 * X);
		HitActor	= Trace(HitLocation, HitNormal, EndShot, StartShot, True, ZeroVec, HitInfo);
		// log("HitActor:"@HitActor@"Hit Bone:"@HitInfo.BoneName);
		if ( HitActor != None && HitInfo.HitComponent != None )
		{
			HitInfo.HitComponent.AddImpulse(X * WeaponImpulse, HitLocation, HitInfo.BoneName);
		}
	}
	else
	{
		PhysicsGrabber.GrabbedComponent.AddImpulse(X * ThrowImpulse, , PhysicsGrabber.GrabbedBoneName);
		PhysicsGrabber.ReleaseComponent();
	}
}

exec function StartAltFire( optional byte FireModeNum )
{
	local vector					CamLoc, StartShot, EndShot, X, Y, Z;
	local vector					HitLocation, HitNormal, Extent;
	local actor						HitActor;
	local float						HitDistance;
	local Quat						PawnQuat, InvPawnQuat, ActorQuat;
	local TraceHitInfo				HitInfo;
	local SkeletalMeshComponent		SkelComp;
	local rotator					CamRot;

	GetPlayerViewPoint(CamLoc, CamRot);

	// Do ray check and grab actor
	GetAxes( CamRot, X, Y, Z );
	StartShot	= CamLoc;
	EndShot		= StartShot + (10000.0 * X);
	Extent		= vect(0,0,0);
	HitActor	= Trace(HitLocation, HitNormal, EndShot, StartShot, True, Extent, HitInfo);
	HitDistance = VSize(HitLocation - StartShot);

	if( HitActor != None &&
		HitActor != Level &&
		HitDistance > HoldDistanceMin &&
		HitDistance < HoldDistanceMax )
	{
		// If grabbing a bone of a skeletal mesh, dont constrain orientation.
		PhysicsGrabber.GrabComponent(HitInfo.HitComponent, HitInfo.BoneName, HitLocation, bRun==0);

		// If we succesfully grabbed something, store some details.
		if( PhysicsGrabber.GrabbedComponent != None )
		{
			HoldDistance	= HitDistance;
			PawnQuat		= QuatFromRotator( CamRot );
			InvPawnQuat		= QuatInvert( PawnQuat );

			if ( HitInfo.BoneName != '' )
			{
				SkelComp = SkeletalMeshComponent(HitInfo.HitComponent);
				ActorQuat = SkelComp.GetBoneQuaternion(HitInfo.BoneName);
			}
			else
			{
				ActorQuat = QuatFromRotator( PhysicsGrabber.GrabbedComponent.Owner.Rotation );
			}

			HoldOrientation = QuatProduct(InvPawnQuat, ActorQuat);
		}
	}
}

exec function StopAltFire( optional byte FireModeNum )
{
	if ( PhysicsGrabber.GrabbedComponent != None )
	{
		PhysicsGrabber.ReleaseComponent();
	}
}

function PlayerTick(float DeltaTime)
{
	local vector	CamLoc, NewHandlePos, X, Y, Z;
	local Quat		PawnQuat, NewHandleOrientation;
	local rotator	CamRot;

	super.PlayerTick(DeltaTime);

	if ( PhysicsGrabber.GrabbedComponent == None )
	{
		return;
	}

	PhysicsGrabber.GrabbedComponent.WakeRigidBody( PhysicsGrabber.GrabbedBoneName );

	// Update handle position on grabbed actor.
	GetPlayerViewPoint(CamLoc, CamRot);
	GetAxes( CamRot, X, Y, Z );
	NewHandlePos = CamLoc + (HoldDistance * X);
	PhysicsGrabber.SetLocation( NewHandlePos );

	// Update handle orientation on grabbed actor.
	PawnQuat = QuatFromRotator( CamRot );
	NewHandleOrientation = QuatProduct(PawnQuat, HoldOrientation);
	PhysicsGrabber.SetOrientation( NewHandleOrientation );
}

// default state, physics spectator!
state PhysicsSpectator extends BaseSpectating
{
ignores NotifyBump, PhysicsVolumeChange, SwitchToBestWeapon;

	exec function ShaneMode()
	{
		TogglePhysicsMode();
		global.ShaneMode();
	}

	exec function TogglePhysicsMode()
	{
		local Vector	CurrentLoc;
		local Rotator	CurrentRot;

		PlayerReplicationInfo.bOnlySpectator = false;
		CurrentLoc = Location;
		CurrentRot = Rotation;
		//@fixme - temp workaround for players not being able to spawn, figure out why this is now necessary
		Level.Game.bRestartLevel = false;
		Level.Game.RestartPlayer( Self );
		Level.Game.bRestartLevel = true;
		if (Pawn == None)
		{
			Warn("Failed to create new pawn?");
		}
		else
		{
			Pawn.SetLocation( CurrentLoc );
			Pawn.SetRotation( CurrentRot );
		}
		SetRotation( CurrentRot );
	}

	function bool CanRestartPlayer()
	{
		//return false;
		return !PlayerReplicationInfo.bOnlySpectator;
	}

	exec function Jump() {}

	exec function Suicide() {}

	function ChangeTeam( int N )
	{
        Level.Game.ChangeTeam(self, N, true);
	}

    function ServerRestartPlayer()
	{
		if ( Level.TimeSeconds < WaitDelay )
			return;
		if ( Level.NetMode == NM_Client )
			return;
		if ( Level.Game.bWaitingToStartMatch )
			PlayerReplicationInfo.bReadyToPlay = true;
		else
			Level.Game.RestartPlayer(self);
	}



	function EndState()
	{
		bCollideWorld = false;
	}

	function BeginState()
	{
		bCollideWorld = true;
	}
}

defaultproperties
{
	HoldDistanceMin=50.0
	HoldDistanceMax=750.0
	WeaponImpulse=600.0
	ThrowImpulse=800.0
	ChangeHoldDistanceIncrement=50.0

	Begin Object Class=RB_Handle Name=RB_Handle0
	End Object
	Components.Add(RB_Handle0)
	PhysicsGrabber=RB_Handle0
}
