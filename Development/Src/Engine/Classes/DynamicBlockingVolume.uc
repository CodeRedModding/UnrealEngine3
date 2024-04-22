/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This is a movable blocking volume. It can be moved by matinee, being based on
 * dynamic objects, etc.
 */
class DynamicBlockingVolume extends BlockingVolume
	native
	showcategories(Movement)
	placeable;

/** Is the volume enabled by default? */
var() bool bEnabled;

cpptext
{
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	/**
	 * Force TRACE_LevelGeometry to still work with us even though bWorldGeometry is cleared
	 * bWorldGeometry is cleared so that actors can base properly on moving volumes
	 * 
	 * @param Primitive - the primitive to trace against
	 * 
	 * @param SourceActor - the actor doing the trace
	 * 
	 * @param TraceFlags - misc flags describing the trace
	 */
	virtual UBOOL ShouldTrace(UPrimitiveComponent* Primitive, AActor *SourceActor, DWORD TraceFlags);
}

struct CheckpointRecord
{
	var vector Location;
	var rotator Rotation;
	var bool bCollideActors;
	var bool bBlockActors;
	var bool bNeedsReplication;
};

/**
 * Overriden to set the default collision state. 
 */
simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	SetCollision(bEnabled, bBlockActors);
}

function CreateCheckpointRecord(out CheckpointRecord Record)
{
	Record.Location = Location;
	Record.Rotation = Rotation;
	Record.bCollideActors = bCollideActors;
	Record.bBlockActors = bBlockActors;
	Record.bNeedsReplication = (RemoteRole != ROLE_None);
}

function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	if (!bHardAttach)
	{
		SetLocation(Record.Location);
		SetRotation(Record.Rotation);
	}
	// always leave component colliding and just use the Actor flags
	CollisionComponent.SetActorCollision(true, true);
	CollisionComponent.SetTraceBlocking(false, true);
	CollisionComponent.SetBlockRigidBody(Record.bCollideActors);
	SetCollision(Record.bCollideActors, Record.bBlockActors);

	if (Record.bNeedsReplication)
	{
		ForceNetRelevant();
	}
}

defaultproperties
{
	//@todo - Change back to PHYS_None
	Physics=PHYS_Interpolating

	bStatic=false

	bMovable=true
	bAlwaysRelevant=true
	bReplicateMovement=true
	bOnlyDirtyReplication=true
	RemoteRole=ROLE_None
	bWorldGeometry=false

	BrushColor=(R=255,G=255,B=100,A=255)

	bEnabled=true
}
