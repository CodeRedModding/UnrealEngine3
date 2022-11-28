class KAsset extends Actor
	native(Physics)
	placeable;

var() const editconst SkeletalMeshComponent	SkeletalMeshComponent;

var()		bool		bDamageAppliesImpulse;
var()		bool		bWakeOnLevelStart;									

function PostBeginPlay()
{
	super.PostBeginPlay();

	if(bWakeOnLevelStart)
	{
		SkeletalMeshComponent.WakeRigidBody();
	}
}

/**
 * Default behaviour when shot is to apply an impulse and kick the KActor.
 */
function TakeDamage(int Damage, Pawn instigatedBy, Vector hitlocation, Vector momentum, 
					class<DamageType> damageType, optional TraceHitInfo HitInfo)
{
	local vector ApplyImpulse;

	// call Actor's version to handle any SeqEvent_TakeDamage for scripting
	Super.TakeDamage(Damage,instigatedBy,hitlocation,momentum,damageType,HitInfo);

	if ( bDamageAppliesImpulse && damageType.default.KDamageImpulse > 0 )
	{
		if ( VSize(momentum) < 0.001 )
		{
			Log("Zero momentum to KActor.TakeDamage");
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
 */
function TakeRadiusDamage
( 
	Pawn				InstigatedBy, 
	float				BaseDamage, 
	float				DamageRadius, 
	class<DamageType>	DamageType, 
	float				Momentum, 
	vector				HurtOrigin 
)
{
	if ( bDamageAppliesImpulse && damageType.default.KDamageImpulse > 0 )
	{
		CollisionComponent.AddRadialImpulse(HurtOrigin, DamageRadius, damageType.default.KDamageImpulse, RIF_Linear);
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

/** Not supported on KAssets. Ambiguous how to move all bones to a new location, if all should be moved etc. */
simulated function OnTeleport(SeqAct_Teleport inAction)
{
	Log("Cannot use Teleport action on KAssets!");
}

defaultproperties
{
	Begin Object Class=SkeletalMeshComponent Name=KAssetSkelMeshComponent
		CollideActors=true
		BlockActors=false
		BlockZeroExtent=true
		BlockNonZeroExtent=false
		BlockRigidBody=true
		PhysicsWeight=1.0
	End Object
	SkeletalMeshComponent=KAssetSkelMeshComponent
	CollisionComponent=KAssetSkelMeshComponent
	Components.Remove(Sprite)
	Components.Remove(CollisionCylinder)
	Components.Add(KAssetSkelMeshComponent)

	SupportedEvents.Add(class'SeqEvent_ConstraintBroken')

	bDamageAppliesImpulse=true
	bNetInitialRotation=true
	Physics=PHYS_Articulated
	bEdShouldSnap=true
	bStatic=false
	bCollideActors=true
	bBlockActors=false
	bWorldGeometry=false
	bCollideWorld=false
    bProjTarget=true

	bNoDelete=false
	bAlwaysRelevant=true
	bSkipActorPropertyReplication=false
	bUpdateSimulatedPosition=true
	bReplicateMovement=true
	RemoteRole=ROLE_SimulatedProxy
}
