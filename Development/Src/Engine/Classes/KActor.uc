/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class KActor extends DynamicSMActor
	native(Physics)
	nativereplication
	placeable
	ClassGroup(Physics)
	showcategories(Navigation);

cpptext
{
	// AActor interface
	virtual void physRigidBody(FLOAT DeltaTime);
	virtual INT* GetOptimizedRepList(BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel);
	virtual void OnRigidBodyCollision(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData);
	UBOOL ShouldTrace(UPrimitiveComponent* Primitive, AActor *SourceActor, DWORD TraceFlags);

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	virtual void TickSpecial(FLOAT DeltaSeconds);
}

var()	bool	bDamageAppliesImpulse;
var() repnotify bool bWakeOnLevelStart;

// Impact effects
var				ParticleSystemComponent		ImpactEffectComponent;
var				AudioComponent				ImpactSoundComponent;
var				AudioComponent				ImpactSoundComponent2; // @TODO: This could be turned into a dynamic array; but for the moment just 2 will do.
var				float						LastImpactTime;
var				PhysEffectInfo				ImpactEffectInfo;

// Slide effects
var				ParticleSystemComponent		SlideEffectComponent;
var				AudioComponent				SlideSoundComponent;
var				bool						bCurrentSlide;
var				bool						bSlideActive;
var				float						LastSlideTime;
var				PhysEffectInfo				SlideEffectInfo;

/** Enable 'Stay upright' torque, that tries to keep Z axis of KActor pointing along world Z */
var(StayUprightSpring)	bool		bEnableStayUprightSpring;

/** Torque applied to try and keep KActor horizontal. */
var(StayUprightSpring)	float		StayUprightTorqueFactor;

/** Max torque that can be applied to try and keep KActor horizontal */
var(StayUprightSpring)	float		StayUprightMaxTorque;

/** If TRUE limit the maximum speed this object can move. */
var()	bool	bLimitMaxPhysicsVelocity;
/** If bLimitMaxPhysicsVelocity is TRUE, this is how fast the object can move. */
var()	float	MaxPhysicsVelocity;

var native const RigidBodyState RBState;
var	native const float			AngErrorAccumulator;
/** replicated version of DrawScale3D */
var repnotify vector ReplicatedDrawScale3D;

var transient vector InitialLocation;
var transient rotator InitialRotation;

/** whether we need to replicate RBState - used to avoid it for bNoDelete KActors that haven't moved or been awakened yet
 * as in that case the client should already have the same data
 */
var transient bool	bNeedsRBStateReplication;

/**
 * Set TRUE to disable collisions with Pawn rigid bodies on clients.  Set this to true if using optimizations that
 * could cause the server to miss or ignore contacts that the client might dtect with this KActor, which could cause
 * vibration, rubberbanding, and general visual badness.
 */
var bool			bDisableClientSidePawnInteractions;

replication
{
	if (bNeedsRBStateReplication && Role == ROLE_Authority)
		RBState;
	if (bNetInitial && Role == ROLE_Authority)
		bWakeOnLevelStart, ReplicatedDrawScale3D;
}

/** Util for getting the PhysicalMaterial applied to this KActor's StaticMesh. */
native final function PhysicalMaterial GetKActorPhysMaterial();

/** Forces the resolve the RBState regardless of whether the actor is sleeping */
native final function ResolveRBState();

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	if (bWakeOnLevelStart && (StaticMeshComponent != None) )
	{
		StaticMeshComponent.WakeRigidBody();
	}
	else
	{
		bNeedsRBStateReplication = !bNoDelete;
	}
	ReplicatedDrawScale3D = DrawScale3D * 1000.0f; // avoids effects of vector rounding

	// Initialise impact/slide components (if we are being notified of physics events, and have sounds/effects set up
	// in PhysicalMaterial applied to our static mesh.
	if((StaticMeshComponent != None) && StaticMeshComponent.bNotifyRigidBodyCollision)
	{
		SetPhysicalCollisionProperties();
	}

	InitialLocation = Location;
	InitialRotation = Rotation;

	if ( bDisableClientSidePawnInteractions && (Role != ROLE_Authority) && (StaticMeshComponent != None) )
	{
		// on clients, turn off collision with pawn RBs, Let replication handle collision response
		// to avoid server/client disagreement.
		StaticMeshComponent.SetRBCollidesWithChannel(RBCC_Pawn, FALSE);
	}
}

/** called when the actor falls out of the world 'safely' (below KillZ and such) */
simulated event FellOutOfWorld(class<DamageType> dmgType)
{
	ShutDown();
	Super.FellOutOfWorld(dmgType);
}

simulated event Destroyed()
{
 	// Let the components play out normally
 	if( ImpactEffectInfo.Sound != None )
 	{
		if( ImpactSoundComponent != none )
		{
			ImpactSoundComponent.bAutoDestroy = TRUE;
		}

		if( ImpactSoundComponent2 != none )
		{
			ImpactSoundComponent2.bAutoDestroy = TRUE;
		}
	}

 	if( SlideEffectInfo.Sound != None )
 	{
 		SlideSoundComponent.bAutoDestroy = TRUE;
 	}

 	Super.Destroyed();
 }

simulated function SetPhysicalCollisionProperties()
{
	local PhysicalMaterial PhysMat;
	PhysMat = GetKActorPhysMaterial();
	// cache effect info
	ImpactEffectInfo = PhysMat.FindPhysEffectInfo(EPMET_Impact);
	SlideEffectInfo = PhysMat.FindPhysEffectInfo(EPMET_Slide);

	if(ImpactEffectInfo.Effect != None)
	{
		ImpactEffectComponent = new(self) class'ParticleSystemComponent';
		//AttachComponent(ImpactEffectComponent);
		ImpactEffectComponent.bAutoActivate = FALSE;
		ImpactEffectComponent.SetTemplate(ImpactEffectInfo.Effect);
	}

	if(ImpactEffectInfo.Sound != None)
	{
		ImpactSoundComponent = new(self) class'AudioComponent';
		//AttachComponent(ImpactSoundComponent);
		ImpactSoundComponent.SoundCue = ImpactEffectInfo.Sound;

		ImpactSoundComponent2 = new(self) class'AudioComponent';
		//AttachComponent(ImpactSoundComponent2);
		ImpactSoundComponent2.SoundCue = ImpactEffectInfo.Sound;
	}

	if(SlideEffectInfo.Effect != None)
	{
		SlideEffectComponent = new(self) class'ParticleSystemComponent';
		//AttachComponent(SlideEffectComponent);
		SlideEffectComponent.bAutoActivate = FALSE;
		SlideEffectComponent.SetTemplate(SlideEffectInfo.Effect);
	}

	if(SlideEffectInfo.Sound != None)
	{
		SlideSoundComponent = new(self) class'AudioComponent';
		//AttachComponent(SlideSoundComponent);
		SlideSoundComponent.SoundCue = SlideEffectInfo.Sound;
	}
}


/** Makes sure these properties get set up on spawned meshes. Factories do not set the mesh before PostBeginPlay is called. */
simulated event SpawnedByKismet()
{
	if(StaticMeshComponent.bNotifyRigidBodyCollision)
	{
		SetPhysicalCollisionProperties();
	}

	InitialLocation = Location;
	InitialRotation = Rotation;
}

simulated event ReplicatedEvent(name VarName)
{
	local vector NewDrawScale3D;

	if (VarName == 'bWakeOnLevelStart')
	{
		if (bWakeOnLevelStart)
		{
			StaticMeshComponent.WakeRigidBody();
		}
	}
	else if (VarName == nameof(ReplicatedDrawScale3D))
	{
		NewDrawScale3D = ReplicatedDrawScale3D / 1000.0; // needs to match multiply in PostBeginPlay()
		SetDrawScale3D(NewDrawScale3D);
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

event ApplyImpulse( Vector ImpulseDir, float ImpulseMag, Vector HitLocation, optional TraceHitInfo HitInfo, optional class<DamageType> DamageType )
{
	local vector AppliedImpulse;

	AppliedImpulse = Normal(ImpulseDir) * ImpulseMag;

	if( HitInfo.HitComponent != None )
	{
		HitInfo.HitComponent.AddImpulse( AppliedImpulse, HitLocation, HitInfo.BoneName );
	}
	else
	{	// if no HitComponent is passed, default to our CollisionComponent
		CollisionComponent.AddImpulse( AppliedImpulse, HitLocation );
	}
}

/**
 * Default behaviour when shot is to apply an impulse and kick the KActor.
 */
event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
{
	// call Actor's version to handle any SeqEvent_TakeDamage for scripting
	Super.TakeDamage(Damage, EventInstigator, HitLocation, Momentum, DamageType, HitInfo, DamageCauser);

	if ( bDamageAppliesImpulse && DamageType.default.KDamageImpulse > 0 )
	{
		if ( VSize(momentum) < 0.001 )
		{
			`Log("Zero momentum to KActor.TakeDamage");
			return;
		}

		ApplyImpulse(Momentum,
					DamageType.default.KDamageImpulse,
					HitLocation,
					HitInfo,
					DamageType);
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
	if ( bDamageAppliesImpulse && damageType.default.RadialDamageImpulse > 0 && (Role == ROLE_Authority) )
	{
		CollisionComponent.AddRadialImpulse(HurtOrigin, DamageRadius, damageType.default.RadialDamageImpulse, RIF_Linear, damageType.default.bRadialDamageVelChange);
	}
}

/** If this KActor receives a Toggle ON event from Kismet, wake the physics up. */
simulated function OnToggle(SeqAct_Toggle action)
{
	if(action.InputLinks[0].bHasImpulse)
	{
		StaticMeshComponent.WakeRigidBody();
	}
}

/**
 * Called upon receiving a SeqAct_Teleport action.  Grabs
 * the first destination available and attempts to teleport
 * this actor.
 *
 * @param	inAction - teleport action that was activated
 */
simulated function OnTeleport(SeqAct_Teleport inAction)
{
	local array<Object> objVars;
	local int idx;
	local Actor destActor;

	// find the first supplied actor
	inAction.GetObjectVars(objVars,"Destination");
	for (idx = 0; idx < objVars.Length && destActor == None; idx++)
	{
		destActor = Actor(objVars[idx]);
	}

	// and set to that actor's location
	if (destActor != None)
	{
		StaticMeshComponent.SetRBPosition(destActor.Location);
		StaticMeshComponent.SetRBRotation(destActor.Rotation);
		PlayTeleportEffect(false, true);
	}
}

simulated function Reset()
{
	StaticMeshComponent.SetRBLinearVelocity( Vect(0,0,0) );
	StaticMeshComponent.SetRBAngularVelocity( Vect(0,0,0) );
	StaticMeshComponent.SetRBPosition( InitialLocation );
	StaticMeshComponent.SetRBRotation( InitialRotation );

	if (!bWakeOnLevelStart)
	{
		StaticMeshComponent.PutRigidBodyToSleep();
	}
	else
	{
		StaticMeshComponent.WakeRigidBody();
	}

	// Resolve the RBState and get all of the needed flags set
	ResolveRBState();

	// Force replication
	bForceNetUpdate = TRUE;

	super.Reset();
}

defaultproperties
{
	TickGroup=TG_PostAsyncWork

	SupportedEvents.Add(class'SeqEvent_RigidBodyCollision')

	Begin Object Name=StaticMeshComponent0
		WireframeColor=(R=0,G=255,B=128,A=255)
		BlockRigidBody=true
		RBChannel=RBCC_GameplayPhysics
		RBCollideWithChannels=(Default=TRUE,BlockingVolume=TRUE,GameplayPhysics=TRUE,EffectPhysics=TRUE)
		bBlockFootPlacement=false
	End Object

	bDamageAppliesImpulse=true
	bNetInitialRotation=true
	Physics=PHYS_RigidBody
	bStatic=false
	bCollideWorld=false
	bProjTarget=true
	bBlockActors=true
	bWorldGeometry=false

	bNoDelete=true
	bAlwaysRelevant=true
	bSkipActorPropertyReplication=false
	bUpdateSimulatedPosition=true
	bReplicateMovement=true
	RemoteRole=ROLE_SimulatedProxy

	bCollideActors=true
	bNoEncroachCheck=true
	bBlocksTeleport=true
	bBlocksNavigation=true
	bPawnCanBaseOn=false
	bSafeBaseIfAsleep=TRUE
	bNeedsRBStateReplication=true

	StayUprightTorqueFactor=1000.0
	StayUprightMaxTorque=1500.0

	MaxPhysicsVelocity=350.0

	ReplicatedDrawScale3D=(X=1000.0,Y=1000.0,Z=1000.0) // set so that default scale of (1,1,1) doesn't replicate anything

	bDisableClientSidePawnInteractions=TRUE
}
