/**
 * Version of KActor that can be dynamically spawned and destroyed during gameplay
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 **/
class KActorFromStatic extends KActor
	native(Physics)
	notplaceable
	transient;

/** Actor from whome this static mesh came */
var Actor MyStaticMeshActor;

/** Max speed from an impulse */
var float MaxImpulseSpeed;

cpptext
{
	virtual void NotifyBump(AActor *Other, UPrimitiveComponent* OtherComp, const FVector &HitNormal);
	virtual UBOOL IgnoreBlockingBy( const AActor *Other ) const;
}

/** Disable precomputed lighting when become dynamic */
final native function DisablePrecomputedLighting();

auto state Initializing
{
	/** Delay disabling lightmaps by one tick to avoid lighting pop */
	event Tick(float DeltaTime)
	{
		DisablePreComputedLighting();
		GotoState('');
	}
}

/**
  * A little while after it goes to sleep, make the static mesh static again
  */
event OnSleepRBPhysics()
{
	BecomeStatic();
}

/**
  * Cancel becoming static
  */
event OnWakeRBPhysics()
{
	ClearTimer('BecomeStatic');
}

/** If still asleep, make static */
function BecomeStatic()
{
	if ( !WorldInfo.bDropDetail && (WorldInfo.TimeSeconds - LastRenderTime < 0.15) )
	{
		// wait till not rendered to avoid having lighting pop be visible
		SetTimer(1.0, false, 'BecomeStatic');
		return;
	}

	// if woke up again, don't make static
	if ( StaticMeshComponent.RigidBodyIsAwake() )
	{
		return;
	}

	MakeStatic();
	Destroy();
}

/**
  * Move the StaticMeshComponent back to its original actor owner (MyStaticMeshActor), and fix its position again.
  */
static native function MakeStatic();

/**
  * @PARAM MovableMesh:  Move this StaticMeshComponent to a KActorFromStatic
  * @RETURNS a KActorSpawnable actor with MovableMesh as its mesh (removing MovableMesh from its current owner)
  */
static native function KActorFromStatic MakeDynamic(StaticMeshComponent MovableMesh);

/**
  * Figure out velocity change here so we can bound it.
  */
event ApplyImpulse( Vector ImpulseDir, float ImpulseMag, Vector HitLocation, optional TraceHitInfo HitInfo, optional class<DamageType> DamageType )
{
	local float BodyMass;

	BodyMass = StaticMeshComponent.BodyInstance.GetBodyMass();
	if ( (BodyMass > 0.0) && ((DamageType == None) || !DamageType.default.bRadialDamageVelChange) )
	{
		if ( BodyMass < 1.0 )
		{
			BodyMass = Sqrt(BodyMass);
		}

		ImpulseMag = FMin(ImpulseMag/BodyMass, MaxImpulseSpeed);
	}

	CollisionComponent.AddImpulse( Normal(ImpulseDir) * ImpulseMag, HitLocation,, TRUE );
}

/** 
  *  Bumped or touched by pawn, so take impulse from it.
  */
function ReceiveImpulse(Pawn Other, Vector HitLocation, Vector HitNormal)
{
	local vector HitDir;
	local float ImpulseMag;

	HitDir = Location - HitLocation;
	HitDir.Z = FMax(HitDir.Z, 0.0);
	HitDir = Normal(HitDir);
	ImpulseMag = FMax( 0.5*Other.GroundSpeed, ((Other.Velocity - Velocity) dot HitDir));
	ApplyImpulse(HitDir, ImpulseMag, Location);
}

/**
  *  Bump event is only called for KActorFromStatic if Other is a Pawn
  */
event Bump( Actor Other, PrimitiveComponent OtherComp, Vector HitNormal )
{
	ReceiveImpulse(Pawn(Other), Other.Location, HitNormal);
}

/** 
  * Pawns will Touch rather than Bump 
  */
event Touch( Actor Other, PrimitiveComponent OtherComp, vector HitLocation, vector HitNormal )
{
	if ( Pawn(Other) != None )
	{
		ReceiveImpulse(Pawn(Other), HitLocation, HitNormal);
	}
}

/**
 * Respond to radial damage as well.
 */
simulated function TakeRadiusDamage
(
	Controller			InstigatedBy,
	float				BaseDamage,
	float				DamageRadius,
	class<DamageType>	DamageType,
	float				Momentum,
	vector				HurtOrigin,
	bool				bFullDamage,
	Actor               DamageCauser,
	optional float      DamageFalloffExponent=1.f
)
{
	local int Idx;
	local SeqEvent_TakeDamage DmgEvt;
	// search for any damage events
	for (Idx = 0; Idx < GeneratedEvents.Length; Idx++)
	{
		DmgEvt = SeqEvent_TakeDamage(GeneratedEvents[Idx]);
		if (DmgEvt != None)
		{
			// notify the event of the damage received
			DmgEvt.HandleDamage(self, InstigatedBy, DamageType, BaseDamage);
		}
	}
	if ( bDamageAppliesImpulse && DamageType.default.RadialDamageImpulse > 0 && (Role == ROLE_Authority) )
	{
		ApplyImpulse(Location-HurtOrigin, DamageType.default.RadialDamageImpulse, Location,, DamageType);
	}
}

defaultproperties
{
	RemoteRole=ROLE_None
	bNoDelete=false
	bCallRigidBodyWakeEvents=true
	MaxImpulseSpeed=900.0
	bCanStepUpOn=false

	CollisionComponent=None
	StaticMeshComponent=None
	Components.Remove(StaticMeshComponent0)
	LightEnvironment=None
	Components.Remove(MyLightEnvironment)
}
