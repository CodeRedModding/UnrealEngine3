class KActor extends StaticMeshActor
	native(Physics)
	placeable;

cpptext
{
	virtual void PreNetReceive();
	virtual void PostNetReceive();
}

var() bool bDamageAppliesImpulse;
var() bool bWakeOnLevelStart;		

var RigidBodyState RBState;

replication
{
	reliable if ( Role == ROLE_Authority )
		RBState;
}

function PostBeginPlay()
{
	Super.PostBeginPlay();

	if(bWakeOnLevelStart)
	{
		StaticMeshComponent.WakeRigidBody();
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
	
		ApplyImpulse = Normal(momentum) * damageType.default.KDamageImpulse;
		if ( HitInfo.HitComponent != None )
		{
			HitInfo.HitComponent.AddImpulse(ApplyImpulse, HitLocation, HitInfo.BoneName);
		}
		else
		{	// if no HitComponent is passed, default to our CollisionComponent
			CollisionComponent.AddImpulse(ApplyImpulse, HitLocation);
		}
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
		StaticMeshComponent.SetRBPosition( destActor.Location, destActor.Rotation );
		PlayTeleportEffect(false, true);
	}
}


defaultproperties
{
	Begin Object Name=StaticMeshComponent0
		WireframeColor=(R=0,G=255,B=128,A=255)
	End Object

	bDamageAppliesImpulse=true
	bNetInitialRotation=true
    Physics=PHYS_RigidBody
	bStatic=false
	bCollideWorld=false
    bProjTarget=true
	bBlockActors=true
	bWorldGeometry=false

	bNoDelete=false
	bAlwaysRelevant=true
	bSkipActorPropertyReplication=false
	bUpdateSimulatedPosition=true
	bReplicateMovement=true
	RemoteRole=ROLE_SimulatedProxy
}

