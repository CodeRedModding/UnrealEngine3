//=============================================================================
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class FracturedStaticMeshPart extends FracturedStaticMeshActor
	native(Mesh)
	notplaceable;

/** This part will be destroyed if it gets further than BaseFracturedMeshActor's radius * DestroyPartRadiusFactor from BaseFracturedMeshActor. */
var	float DestroyPartRadiusFactor;

/** Base FracturedStaticMeshActor that this part was spawned off of. */
var transient FracturedStaticMeshActor BaseFracturedMeshActor;

/** Indicates whether this part has already been recycled and just needs to be initialized to be used again. */
var bool bHasBeenRecycled;

/** Last time that this pieces was spawned. */
var float	LastSpawnTime;

/** Used to know index within pool in FractureManager, so we replace it in the free pool. */
var int		PartPoolIndex;

/** Additional gravity scaling for fracture pieces*/
var float	FracPartGravScale;

/** If TRUE, check when piece goes to sleep, and then change its RBChannel. */
var bool bChangeRBChannelWhenAsleep;
/** If bChangeRBChannelWhenAsleep is TRUE, RBChannel to switch to when asleep. */
var ERBCollisionChannel	AsleepRBChannel;

/** Used to store preview frames velocity, to look for sudden velocity changes. */
var vector	OldVelocity;

/** If oldvel dot newvel < -DestroyVelChangeThresh, destroy piece. */
var float	CurrentVibrationLevel;

/** Indicates that this part consists of more than 1 chunk - and should explode upon impact. */
var	bool	bCompositeThatExplodesOnImpact;

/** Time since last physics impact sound */
var	float	LastImpactSoundTime;

/** Revive part, set physics to PHYS_RigidBody, enabled collision etc */
simulated native function Initialize();
/** Reset state of this part - clear mesh reference, put in PHYS_None, disable collision etc */
simulated native function RecyclePart(bool bAddToFreePool);

cpptext
{
	virtual void TickSpecial(FLOAT DeltaSeconds);
	virtual FLOAT GetGravityZ();
	virtual void OnRigidBodyCollision(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData);
#if WITH_NOVODEX
	virtual void ModifyNxActorDesc(NxActorDesc& ActorDesc, UPrimitiveComponent* PrimComp, const class NxGroupsMask& GroupsMask, UINT MatIndex);
	virtual void PostInitRigidBody(NxActor* nActor, NxActorDesc& ActorDesc, UPrimitiveComponent* PrimComp);
#endif // WITH_NOVODEX
};


/** Used so weapons etc move parts. */
simulated event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
{
	Super.TakeDamage(Damage, EventInstigator, HitLocation, Momentum, DamageType, HitInfo, DamageCauser);

	FracturedStaticMeshComponent.AddImpulse(Normal(momentum) * damageType.default.KDamageImpulse, HitLocation);
}

/** Try to clean up this part if its not seen or it's parent has been removed. If it is seen, keep trying until it isn't. */
simulated function TryToCleanUp()
{
	if(BaseFracturedMeshActor == None || WorldInfo.TimeSeconds - BaseFracturedMeshActor.SkinnedComponent.LastRenderTime > 1.0)
	{
		RecyclePart(TRUE);
	}
	else
	{
		SetTimer( 2.0, FALSE, nameof(TryToCleanUp) );
	}
}

simulated event FellOutOfWorld(class<DamageType> dmgType)
{
	RecyclePart(TRUE);
}

simulated event Explode()
{
	// Explode is called from OnRigidBodyCollision, which can be fired off multiple times for the same actor, so we must prevent re-entrance
	if (!bHasBeenRecycled)
	{
		super.Explode();
		RecyclePart(TRUE);
	}
}

/** For broken off pieces, we only want to spawn more parts if its a shatterable composite. */
simulated event BreakOffPartsInRadius(vector Origin, float Radius, float RBStrength, bool bWantPhysChunksAndParticles)
{
	if(bCompositeThatExplodesOnImpact)
	{
		super.BreakOffPartsInRadius(Origin, Radius, RBStrength, bWantPhysChunksAndParticles);
	}
}

defaultproperties
{
	Components.Remove(LightEnvironment0)
	Components.Remove(FracturedSkinnedComponent0)
	SkinnedComponent=none

	Begin Object Name=FracturedStaticMeshComponent0
		bCastDynamicShadow=FALSE
		bForceDirectLightMap=FALSE
		bAllowApproximateOcclusion=TRUE
		BlockRigidBody=TRUE
		bAcceptsStaticDecals=FALSE
		bAcceptsDynamicDecals=FALSE
		bSkipRBGeomCreation=TRUE // ModifyNxActorDesc fills in geometry
		BlockNonZeroExtent=FALSE
		BlockZeroExtent=FALSE
		RBChannel=RBCC_FracturedMeshPart
		RBCollideWithChannels=(Default=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE,FracturedMeshPart=TRUE)
		bUseVisibleVertsForBounds=TRUE
		bUseDynamicIndexBuffer=FALSE
		bUseDynamicIBWithHiddenFragments=FALSE
		bUseSkinnedRendering=TRUE
		bInitialVisibilityValue=FALSE
		bUsePrecomputedShadows=FALSE
	End Object

	DestroyPartRadiusFactor=10;
	bNoDelete=FALSE
	bMovable=TRUE
	bWorldGeometry=FALSE
	bPathColliding=FALSE
	TickGroup=TG_PostAsyncWork
	bNetInitialRotation=true
	bCollideActors=TRUE
	bBlockActors=FALSE
	bNoEncroachCheck=TRUE
	Physics=PHYS_RigidBody
	RemoteRole=ROLE_None
	LifeSpan=15.0
	FracPartGravScale=2.0

	bChangeRBChannelWhenAsleep=FALSE
	AsleepRBChannel=RBCC_GameplayPhysics
}
