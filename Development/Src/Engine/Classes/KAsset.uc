/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class KAsset extends Actor
	native(Physics)
	nativereplication
	ClassGroup(Physics)
	placeable;

var() const editconst SkeletalMeshComponent	SkeletalMeshComponent;

var()		bool		bDamageAppliesImpulse;
var()		bool		bWakeOnLevelStart;

/** Whether this KAsset should block Pawns. */
var()		bool		bBlockPawns;

/** Used to replicate mesh to clients */
var repnotify transient SkeletalMesh ReplicatedMesh;

/** Used to replicate physics asset to clients */
var repnotify transient PhysicsAsset ReplicatedPhysAsset;

replication
{
	if ( Role == ROLE_Authority)
		ReplicatedMesh, ReplicatedPhysAsset;
}

cpptext
{
public:
	// AActor interface.
	virtual INT* GetOptimizedRepList(BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel);

	/**
	* Function that gets called from within Map_Check to allow this actor to check itself
	* for any potential errors and register them with map check dialog.
	*/
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	UBOOL ShouldTrace(UPrimitiveComponent* Primitive, AActor *SourceActor, DWORD TraceFlags);
	UBOOL IgnoreBlockingBy(const AActor* Other) const;
}

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	if (bWakeOnLevelStart)
	{
		SkeletalMeshComponent.WakeRigidBody();
	}
	ReplicatedMesh = SkeletalMeshComponent.SkeletalMesh;
	ReplicatedPhysAsset = SkeletalMeshComponent.PhysicsAsset;
}

/** Sets the new mesh and physics asset, along with the replicated properties for clients. */
final function SetMeshAndPhysAsset(SkeletalMesh NewMesh, PhysicsAsset NewPhysAsset)
{
	SkeletalMeshComponent.SetSkeletalMesh(NewMesh);
	ReplicatedMesh = NewMesh;
	SkeletalMeshComponent.SetPhysicsAsset(NewPhysAsset);
	ReplicatedPhysAsset = NewPhysAsset;
}

simulated event ReplicatedEvent( name VarName )
{
	if (VarName == 'ReplicatedMesh')
	{
		SkeletalMeshComponent.SetSkeletalMesh(ReplicatedMesh);
	}
	else if(VarName == 'ReplicatedPhysAsset')
	{
		SkeletalMeshComponent.SetPhysicsAsset(ReplicatedPhysAsset);
	}
}

/**
 * Default behaviour when shot is to apply an impulse and kick the KActor.
 */
simulated event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
{
	local vector ApplyImpulse;

	// call Actor's version to handle any SeqEvent_TakeDamage for scripting
	Super.TakeDamage(Damage, EventInstigator, HitLocation, Momentum, DamageType, HitInfo, DamageCauser);

	if ( bDamageAppliesImpulse && damageType.default.KDamageImpulse > 0 )
	{
		if ( VSize(momentum) < 0.001 )
		{
			`Log("Zero momentum to KActor.TakeDamage");
			return;
		}

		// Make sure we have a valid TraceHitInfo with our SkeletalMesh
		// we need a bone to apply proper impulse
		CheckHitInfo( HitInfo, SkeletalMeshComponent, Normal(Momentum), hitlocation );

		ApplyImpulse = Normal(momentum) * damageType.default.KDamageImpulse;
		if ( HitInfo.HitComponent != None )
		{
			HitInfo.HitComponent.AddImpulse(ApplyImpulse, HitLocation, HitInfo.BoneName);
		}
	}
}

/**
 * Take Radius Damage
 *
 * @param	InstigatedBy, instigator of the damage
 * @param	Base Damage
 * @param	Damage Radius (from Origin)
 * @param	DamageType class
 * @param	Momentum (float)
 * @param	HurtOrigin, origin of the damage radius.
 * @param DamageCauser the Actor that directly caused the damage (i.e. the Projectile that exploded, the Weapon that fired, etc)
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
	if ( bDamageAppliesImpulse && damageType.default.RadialDamageImpulse > 0 && (Role == ROLE_Authority) )
	{
		CollisionComponent.AddRadialImpulse(HurtOrigin, DamageRadius, damageType.default.RadialDamageImpulse, RIF_Linear, damageType.default.bRadialDamageVelChange);
	}
}

/** If this KAsset receives a Toggle ON event from Kismet, wake the physics up. */
simulated function OnToggle(SeqAct_Toggle action)
{
	if(action.InputLinks[0].bHasImpulse)
	{
		SkeletalMeshComponent.WakeRigidBody();
	}
}

simulated function OnTeleport(SeqAct_Teleport InAction)
{
	local Actor DestActor;

	DestActor = Actor(SeqVar_Object(InAction.VariableLinks[1].LinkedVariables[0]).GetObjectValue());
	if (DestActor != None)
	{
		SkeletalMeshComponent.SetRBPosition(DestActor.Location);
	}
	else
	{
		InAction.ScriptLog("No Destination for" @ InAction @ "on" @ self);
	}
}


/** Performs actual attachment. Can be subclassed for class specific behaviors. */
function DoKismetAttachment( Actor Attachment, SeqAct_AttachToActor Action )
{
	Attachment.SetBase( Self,, SkeletalMeshComponent, Action.BoneName );
}



defaultproperties
{
	TickGroup=TG_PostAsyncWork

	Begin Object Class=DynamicLightEnvironmentComponent Name=MyLightEnvironment
	End Object
	Components.Add(MyLightEnvironment)

	Begin Object Class=SkeletalMeshComponent Name=KAssetSkelMeshComponent
		CollideActors=true
		BlockActors=true
		BlockZeroExtent=true
		BlockNonZeroExtent=false
		BlockRigidBody=true
		bHasPhysicsAssetInstance=true
		bUpdateKinematicBonesFromAnimation=false
		bUseTickOptimization=false
		PhysicsWeight=1.0
		RBChannel=RBCC_GameplayPhysics
		RBCollideWithChannels=(Default=TRUE,BlockingVolume=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE)
		LightEnvironment=MyLightEnvironment
		bSkipAllUpdateWhenPhysicsAsleep=TRUE
		bBlockFootPlacement=false
	End Object
	CollisionComponent=KAssetSkelMeshComponent
	SkeletalMeshComponent=KAssetSkelMeshComponent
	Components.Add(KAssetSkelMeshComponent)

	SupportedEvents.Add(class'SeqEvent_ConstraintBroken')
	SupportedEvents.Add(class'SeqEvent_RigidBodyCollision')

	bDamageAppliesImpulse=true
	bNetInitialRotation=true
	Physics=PHYS_RigidBody
	bEdShouldSnap=true
	bStatic=false
	bCollideActors=true
	bBlockActors=true
	bWorldGeometry=false
	bCollideWorld=false
	bProjTarget=true

	bNoDelete=true
	bAlwaysRelevant=true
	bSkipActorPropertyReplication=false
	bUpdateSimulatedPosition=true
	bReplicateMovement=true
	RemoteRole=ROLE_SimulatedProxy
	bReplicateRigidBodyLocation=FALSE
}
