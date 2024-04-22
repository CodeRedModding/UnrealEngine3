/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FractureManager extends Actor
	native(Mesh)
	config(Game);

/* Number of FSM parts in the pool */
var int FSMPartPoolSize;

/** If TRUE, look for vibrating FSM parts and kill them */
var()	bool	bEnableAntiVibration;
/** How much vibration (defined as changes in angular velocity direction) must occur before part is killed. */
var()	float	DestroyVibrationLevel;
/** Min angular velocity of part to be killed by vibration detection code. */
var()	float	DestroyMinAngVel;

/** If TRUE, spawn effect for chunks falling off when doing radial damage (ie removing many chunks at once)*/
var()	bool	bEnableSpawnChunkEffectForRadialDamage;

/** */
var()	float	ExplosionVelScale;

/* 
 * Time after spawning a part during which the part is guaranteed to not be recycled again.
 * Used to avoid redundant work when spawning a lot of parts relative to the pool size.
 */
const FSM_DEFAULTRECYCLETIME = 0.2;

var array<FracturedStaticMeshPart>	PartPool;
var array<int>						FreeParts;

/** List of actors that have fracture parts that have been deferred to spawn on upcoming frames */
var transient array< FracturedStaticMeshActor > ActorsWithDeferredPartsToSpawn;

/** Function use to spawn particle effect when a chunk is destroyed. */
simulated event SpawnChunkDestroyEffect(ParticleSystem Effect, box ChunkBox, vector ChunkDir, float Scale)
{
	local vector ChunkMiddle;
	local ParticleSystemComponent EffectComp;

	ChunkMiddle = 0.5 * (ChunkBox.Min + ChunkBox.Max);
	EffectComp = WorldInfo.MyEmitterPool.SpawnEmitter(Effect, ChunkMiddle, rotator(ChunkDir));
	EffectComp.SetScale(Scale);
}

native function float GetNumFSMPartsScale();

/** Returns a scalar to the percentage chance of a fractured static mesh spawning a rigid body after
	taking direct damage */
native function float GetFSMDirectSpawnChanceScale();

/** Returns a scalar to the percentage chance of a fractured static mesh spawning a rigid body after
	taking radial damage, such as from an explosion */
native function float GetFSMRadialSpawnChanceScale();

/** Returns a distance scale for whether a fractured static mesh should actually fracture when damaged */
native function float GetFSMFractureCullDistanceScale();

cpptext
{
	virtual void TickSpecial( FLOAT DeltaSeconds );
};

simulated event PreBeginPlay()
{
	Super.PreBeginPlay();

	CreateFSMParts();
}

simulated event Destroyed()
{
	Super.Destroyed();

	CleanUpFSMParts();
}



simulated final function CleanUpFSMParts()
{
	local int Idx;

	for( Idx=0; Idx<PartPool.length; Idx++ )
	{
		PartPool[Idx].Destroy();
		PartPool[Idx] = None;
	}
}

/** */
native function CreateFSMParts();

/** Recycles any active parts */
simulated native function ResetPoolVisibility();

/** Grab a FSMP from the free pool, or forcibly recycle a suitable one from the world. */
native function FracturedStaticMeshPart GetFSMPart(FracturedStaticMeshActor Parent, Vector SpawnLocation, Rotator SpawnRotation);

/** Function to actually spawn a FSMP. Allows game-specific pooling/capping of actors. */
simulated event FracturedStaticMeshPart SpawnPartActor(FracturedStaticMeshActor Parent, vector SpawnLocation, rotator SpawnRotation)
{
	local FracturedStaticMeshPart NewPart;

	//get a new part from the pool
	NewPart = GetFSMPart(Parent, SpawnLocation, SpawnRotation);

	if (NewPart != None)
	{
		NewPart.SetTimer( 10.f, FALSE, nameof(NewPart.TryToCleanUp) );
	}

	return NewPart;
}

/** Return a part to the pool. */
simulated event ReturnPartActor(FracturedStaticMeshPart Part)
{
	FreeParts.AddItem(Part.PartPoolIndex);
}

/** Called every frame to update the object */
simulated function Tick( float DeltaTime )
{
	local int CurActorIndex;

	// Call parent implementation skipped because no implementation in Actor
	//super.Tick( DeltaTime );

	// Give any actors with deferred chunks a chance to spawn now
	if( ActorsWithDeferredPartsToSpawn.length > 0 )
	{
		for( CurActorIndex = 0; CurActorIndex < ActorsWithDeferredPartsToSpawn.length; ++CurActorIndex )
		{
			if( ActorsWithDeferredPartsToSpawn[ CurActorIndex ].SpawnDeferredParts() )
			{
				// No chunks left to spawn, so we can remove it from our list
				ActorsWithDeferredPartsToSpawn.remove( CurActorIndex, 1 );
				--CurActorIndex;
			}
		}
	}
}

defaultproperties
{
	FSMPartPoolSize=50

	bEnableAntiVibration=FALSE
	DestroyVibrationLevel=3.0
	DestroyMinAngVel=2.5
	ExplosionVelScale=1.0
}
