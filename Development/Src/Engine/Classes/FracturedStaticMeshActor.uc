//=============================================================================
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class FracturedStaticMeshActor extends Actor
	dependson(FracturedStaticMeshComponent)
	dependson(WorldInfo)
	ClassGroup(StaticMeshes)
	native(Mesh)
	placeable;


/** Maximum number of rigid body parts to spawn off per actor, per frame */
var() int MaxPartsToSpawnAtOnce;

/** Info about a fracture part that should be spawned in an upcoming Tick */
struct native DeferredPartToSpawn
{
	/** Index of the chunk to spawn */
	var int ChunkIndex;

	/** Exit velocity for this chunk */
	var vector InitialVel;

	/** Exit angular velocity for this chunk */
	var vector InitialAngVel;

	/** Relative scale */
	var float RelativeScale;

	/** TRUE if this part was spawned because of an explosion */
	var bool bExplosion;
};


var() const editconst FracturedStaticMeshComponent	FracturedStaticMeshComponent;

/** Skinned component which will handle rendering for FracturedStaticMeshComponent */
var const FracturedSkinnedMeshComponent  SkinnedComponent;

/** Current health of each chunk */
var array<int> ChunkHealth;

/** Used so we only display the 'missing sound' warning once */
var transient bool bHasShownMissingSoundWarning;

/** If true, detach parts when an Actor with bCanCauseFractureOnTouch contacts them. Actor must not be blocked by this FSMA. */
var()	bool	bBreakChunksOnActorTouch;

/** Set of damage types that can cause pieces to break off this FSAM. If empty, all damage types can do this. */
var() array< class<DamageType> > FracturedByDamageType;

/** Allows controlling how much 'health' chunks have on a per-instance basis */
var()	float	ChunkHealthScale;

/** Allows you to override particle effects to play when chunk is hidden for just this actor. */
var()	array<ParticleSystem>	OverrideFragmentDestroyEffects;

/** Minimum distance from player where actor will ALWAYS fracture, even when outside the view frustum (scaled by global settings.) */
var()	float	FractureCullMinDistance;

/** Maximum distance from player where actor will be allowed to fracture (scaled by global settings.) */
var()	float	FractureCullMaxDistance;

/** Array of parts that are waiting to be spawned in an upcoming tick */
var transient array< DeferredPartToSpawn > DeferredPartsToSpawn;

/** Cached info for part impacts */
var		PhysEffectInfo	PartImpactEffect;

/** Cached sound for large fractures. */
var		SoundCue		ExplosionFractureSound;
/** Cached sound for single chunk fractures. */
var		SoundCue		SingleChunkFractureSound;

/** This is the material that was set on this FracuredStaticMeshActor before we overrode it with LoseChunkOutsideMaterial **/
var transient MaterialInterface MI_LoseChunkPreviousMaterial;

/** Data relevant to checkpoint save/load, see CreateCheckpointRecord/ApplyCheckpointRecord below */
struct CheckpointRecord
{
	var bool bIsShutdown;
	var array<byte> FragmentVis;
};
/** whether this should be saved in checkpoints */
var() bool bShouldSaveForCheckpoint;

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void TickSpecial(FLOAT DeltaSeconds);
	virtual UBOOL InStasis();
};


/** Spawn one chunk of this mesh as its own Actor, with the supplied velocities and scale relative to this Actor. */
simulated native final function FracturedStaticMeshPart SpawnPart(int ChunkIndex, vector InitialVel, vector InitialAngVel, float RelativeScale, bool bExplosion);

/** Does the same as SpawnPart, but takes an array of chunks to make part of the new part. */
simulated native final function FracturedStaticMeshPart SpawnPartMulti(array<int> ChunkIndices, vector InitialVel, vector InitialAngVel, float RelativeScale, bool bExplosion);


simulated event PostBeginPlay()
{
	local PhysicalMaterial PhysMat;

	super.PostBeginPlay();
	ResetHealth();
	if (!bBreakChunksOnActorTouch)
	{
		SetTickIsDisabled(true);
	}

	// Cache effects for impact and sound for large fracture
	PhysMat = FracturedStaticMeshComponent.GetFracturedMeshPhysMaterial();
	PartImpactEffect = PhysMat.FindPhysEffectInfo(EPMET_Impact);

	PhysMat.FindFractureSounds(ExplosionFractureSound, SingleChunkFractureSound);

	ResetVisibility();
}

/** Used to init/reset health array. */
simulated native final function ResetHealth();

function bool ShouldSaveForCheckpoint()
{
	return bShouldSaveForCheckpoint;
}

/** Called when this actor is being saved in a checkpoint, records pertinent information for restoration via ApplyCheckpointRecord. */
function CreateCheckpointRecord(out CheckpointRecord Record)
{
	if (Physics == PHYS_None && bHidden)
	{
		Record.bIsShutdown = true;
	}
	else
	{
		Record.FragmentVis = FracturedStaticMeshComponent.GetVisibleFragments();
	}
}

/** Restores this actor from its checkpoint record */
function ApplyCheckpointRecord(const out CheckpointRecord Record)
{
	if (Record.bIsShutdown)
	{
		ShutDown();
	}
	else
	{
		FracturedStaticMeshComponent.SetVisibleFragments(Record.FragmentVis);
	}
}

/**
 *	Find all groups of chunks which are not connected to 'root' parts, and spawn them as new physics objects.
 *	Updates FragmentVis with new chunks that get hidden by the process.
 */
native simulated event BreakOffIsolatedIslands(out array<BYTE> FragmentVis, array<int> IgnoreFrags, vector ChunkDir, array<FracturedStaticMeshPart> DisableCollWithPart, bool bWantPhysChunks);

/**
  * Gives the actor a chance to spawn chunks that may have been deferred
  *
  * @return	Returns true if there are still any deferred parts left to spawn
  */
native simulated event bool SpawnDeferredParts();

/** Util for checking if this damage type will cause fracture. */
simulated function bool IsFracturedByDamageType(class<DamageType> DmgType)
{
	local int i;

	if(FracturedByDamageType.length == 0)
	{
		return TRUE;
	}

	for(i=0; i<FracturedByDamageType.length; i++)
	{
		if(DmgType == FracturedByDamageType[i])
		{
			return TRUE;
		}
	}

	return FALSE;
}

/** Specialized to handle explicit effect instigators.   Also, handles None instigators -- these are special
	weapons (eg dummyfire) and are always relevant.  Also, for fractured static meshes we only spawn effects if
	the mesh itself has been rendered recently. */
simulated function bool FractureEffectIsRelevant( bool bForceDedicated, Pawn EffectInstigator, out byte bWantPhysChunksAndParticles )
{
	local bool bResult;
	local PlayerController P;
	local float FinalMinDistance, FinalCullDistance, DistSq;
	local vector CameraLoc;
	local rotator CameraRot;

	bResult = true;

	// Default to wanting physics chunks and particles
	bWantPhysChunksAndParticles = 1;

	if( EffectInstigator == None )
	{
		// Probably 'dummy fire', so always let it slide
		return true;
	}
	else
	{
		if ( WorldInfo.NetMode == NM_DedicatedServer )
		{
			return bForceDedicated;
		}

		if ( (WorldInfo.NetMode == NM_ListenServer) && (WorldInfo.Game.NumPlayers > 1) )
		{
			if ( bForceDedicated )
			{
				return true;
			}
			if ( (EffectInstigator != None) && EffectInstigator.IsHumanControlled() && EffectInstigator.IsLocallyControlled() )
			{
				return true;
			}
		}
		else if ( (EffectInstigator != None) && EffectInstigator.IsHumanControlled() )
		{
			return true;
		}

		// Apply global system settings scale to cull distances
		FinalMinDistance = FractureCullMinDistance * WorldInfo.MyFractureManager.GetFSMFractureCullDistanceScale();
		FinalCullDistance = FractureCullMaxDistance * WorldInfo.MyFractureManager.GetFSMFractureCullDistanceScale();

		// Determine how far to the nearest local viewer
		DistSq = 10000000000.0;
		foreach LocalPlayerControllers(class'PlayerController', P)
		{
			P.GetPlayerViewPoint(CameraLoc, CameraRot);
			DistSq = FMin(DistSq, VSizeSq(Location - CameraLoc));
		}

		if ( DistSq > FinalCullDistance*FinalCullDistance )
		{
			bResult = false;
		}
		else if ( DistSq < FinalMinDistance*FinalMinDistance )
		{
			// If close enough, always spawn even if hidden
			return true;
		}

		// For fractured meshes, we only want to spawn effects if the mesh actor itself has been seen recently
		if( bResult )
		{
			if( WorldInfo.TimeSeconds - LastRenderTime < 0.5 )
			{
				// We're on screen and close enough to render!
				return true;
			}
			else
			{
				// We're off screen, but still close enough to allow the effect, but we'll skip
				// spawning rigid bodies.
				bWantPhysChunksAndParticles = 0;
				return true;
			}
		}
		else
		{
			// Too far away!
			bWantPhysChunksAndParticles = 0;
			return false;
		}
	}
}


/**
 * This function will remove all of the currently attached decals from the object.
 * Basically, we want to have decals attach to these objects and then on state change (due to damage usually), we will
 * just detach them all with the big particle effect that occurs it should not be noticeable.
 **/
simulated native final protected function RemoveDecals( int IndexToRemoveDecalsFrom );



/** TakeDamage will hide/spawn chunks when they get shot. */
simulated event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
{
	local array<byte> FragmentVis;
	local vector ChunkDir, MomentumDir;
	local FracturedStaticMesh FracMesh;
	local FracturedStaticMeshPart FracPart;
	local array<FracturedStaticMeshPart> NoCollParts;
	local int TotalVisible;
	local array<int> IgnoreFrags;
	local box ChunkBox;
	local ParticleSystem EffectPSys;
	local float PhysChance, PartScale;
	local byte bWantPhysChunksAndParticles;
	local Pawn InstigatorPawn;
	local WorldFractureSettings FractureSettings;
	local vector NewHitLocation, HitNormal;

	// call Actor's version to handle any SeqEvent_TakeDamage for scripting
	Super.TakeDamage(Damage, EventInstigator, HitLocation, Momentum, DamageType, HitInfo, DamageCauser);

	// Ignore invalid damage type, hits to the core, to parts that are not destroyable, or parts which are already hidden (somehow)
	if( (DamageType != None && !DamageType.default.bCausesFracture) || !IsFracturedByDamageType(DamageType) )
	{
		return;
	}

	if ( HitInfo.HitComponent == None )
	{
		// Perform trace to retrieve hit info
		if ( Momentum == vect(0,0,0) )
		{
			Momentum = Location - HitLocation;
		}
		TraceComponent(NewHitLocation, HitNormal, FracturedStaticMeshComponent, HitLocation+100*Normal(Momentum), HitLocation,, HitInfo, true);
	}

	if ( HitInfo.Item == FracturedStaticMeshComponent.GetCoreFragmentIndex() ||
		!FracturedStaticMeshComponent.IsFragmentVisible(HitInfo.Item) ||
		!FracturedStaticMeshComponent.IsFragmentDestroyable(HitInfo.Item) )
	{
		return;
	}

	// Make sure the impacted fractured mesh is visually relevant
	if (EventInstigator != None)
	{
		InstigatorPawn = EventInstigator.Pawn;
	}
	else if (DamageCauser != None)
	{
		InstigatorPawn = DamageCauser.Instigator;
	}
	if (!FractureEffectIsRelevant(false, InstigatorPawn, bWantPhysChunksAndParticles))
	{
		return;
	}
	// Take away from chunks health - if scripted, force to zero.
	if( RB_LineImpulseActor(DamageCauser) != None )
	{
		ChunkHealth[HitInfo.Item] = 0.0;
	}
	else if ( DamageType != None )
	{
		ChunkHealth[HitInfo.Item] -= WorldInfo.FracturedMeshWeaponDamage * DamageType.default.FracturedMeshDamage;
	}
	else
	{
		ChunkHealth[HitInfo.Item] -= WorldInfo.FracturedMeshWeaponDamage;
	}
	//`log("FSM:TAKEDAMAGE"@HitInfo.Item@ChunkHealth[HitInfo.Item]);

	// If its hit zero health, hide part and spawn part.
	if(ChunkHealth[HitInfo.Item] <= 0)
	{
		FracMesh = FracturedStaticMesh(FracturedStaticMeshComponent.StaticMesh);

		// Get fracture settings from relevant WorldInfo.
		FractureSettings = WorldInfo.GetWorldFractureSettings();

		FragmentVis = FracturedStaticMeshComponent.GetVisibleFragments();
		TotalVisible = FracturedStaticMeshComponent.GetNumVisibleFragments();

		// If physics object - ignore hits if you are the last part
		if(Physics == PHYS_RigidBody)
		{
			if(TotalVisible == 1)
			{
				return;
			}
		}

		// If we are losing the first chunk, change exterior material (if replacement is defined).
		if(TotalVisible == FragmentVis.length)
		{
			SetLoseChunkReplacementMaterial();
		}

		FragmentVis[HitInfo.Item] = 0;

		// Start with average exterior normal of chunk
		ChunkDir = FracturedStaticMeshComponent.GetFragmentAverageExteriorNormal(HitInfo.Item);

		// If bad normal, or its pointing away from us, add in the shot momentum
		MomentumDir = Normal(Momentum);
		if((VSize(ChunkDir) < 0.01) || (MomentumDir Dot ChunkDir > -0.2))
		{
			ChunkDir += MomentumDir;
		}

		// Take out any downwards force
		ChunkDir.Z = Max(ChunkDir.Z, 0.0);
		// Reduce Z vel
		ChunkDir.Z /= FracMesh.ChunkLinHorizontalScale;
		// Normalize
		ChunkDir = Normal(ChunkDir);

		// See if we want to spawn physics chunks, and take into account chance of it happening
		if ( WorldInfo.NetMode != NM_DedicatedServer )
		{
			PhysChance = FractureSettings.bEnableChanceOfPhysicsChunkOverride ? FractureSettings.ChanceOfPhysicsChunkOverride : FracMesh.ChanceOfPhysicsChunk;
			PhysChance *= WorldInfo.MyFractureManager.GetFSMDirectSpawnChanceScale();
			if( bWantPhysChunksAndParticles == 1 &&
				FracMesh.bSpawnPhysicsChunks &&
				(FRand() < PhysChance) &&
				!FracturedStaticMeshComponent.IsNoPhysFragment(HitInfo.Item) )
			{
				PartScale = FracMesh.NormalPhysicsChunkScaleMin + FRand() * (FracMesh.NormalPhysicsChunkScaleMax - FracMesh.NormalPhysicsChunkScaleMin);
				// Spawn part moving from center of mesh
				FracPart = SpawnPart(HitInfo.Item, (ChunkDir * FracMesh.ChunkLinVel) + Velocity, VRand() * FracMesh.ChunkAngVel, PartScale, FALSE);
				//RemoveDecals( HitInfo.Item );

				if (FracPart != None)
				{
					// Disable collision between spawned part and this mesh.
					FracPart.FracturedStaticMeshComponent.DisableRBCollisionWithSMC(FracturedStaticMeshComponent, TRUE);
				}
			}

			// Assign effect if there is one.
			if( bWantPhysChunksAndParticles == 1 )
			{
				// Look for override first
				if(OverrideFragmentDestroyEffects.length > 0)
				{
					// Pick randomly
					EffectPSys = OverrideFragmentDestroyEffects[Rand(OverrideFragmentDestroyEffects.length)];
				}
				// No override array, try the mesh
				else if(FracMesh.FragmentDestroyEffects.length > 0)
				{
					EffectPSys = FracMesh.FragmentDestroyEffects[Rand(FracMesh.FragmentDestroyEffects.length)];
				}

				// If we have an effect and a manager - spawn it
				if(EffectPSys != None && WorldInfo.MyFractureManager != None)
				{
					ChunkBox = FracturedStaticMeshComponent.GetFragmentBox(HitInfo.Item);
					WorldInfo.MyFractureManager.SpawnChunkDestroyEffect(EffectPSys, ChunkBox, ChunkDir, FracMesh.FragmentDestroyEffectScale);
				}
			}
		}

		// If no core (or 'always break off isolated parts'), and not overriding - we have to look for un-rooted 'islands'
		if((FracMesh.bAlwaysBreakOffIsolatedIslands || FracturedStaticMeshComponent.GetCoreFragmentIndex() == INDEX_NONE) && !FracMesh.bFixIsolatedChunks)
		{
			IgnoreFrags[0] = HitInfo.Item;
			if(FracPart != None)
			{
				NoCollParts[0] = FracPart;
			}
			BreakOffIsolatedIslands(
				FragmentVis,
				IgnoreFrags,
				ChunkDir,
				NoCollParts,
				bWantPhysChunksAndParticles == 1 ? true : false );
		}

		// Right at the end, change fragment visibility
		FracturedStaticMeshComponent.SetVisibleFragments(FragmentVis);

		// If this is a physical part - reset physics state, to take notice of new hidden parts.
		if (Physics == PHYS_RigidBody)
		{
			FracturedStaticMeshComponent.RecreatePhysState();
		}
	}
}

/**
 *	Break off all pieces in one go.
 */
simulated event Explode()
{
	local array<byte> FragmentVis;
	local int i;
	local vector SpawnDir;
	local FracturedStaticMesh FracMesh;
	local FracturedStaticMeshPart FracPart;
	local float PartScale;

	FracMesh = FracturedStaticMesh(FracturedStaticMeshComponent.StaticMesh);

	// Iterate over all visible fragments spawning them
	FragmentVis = FracturedStaticMeshComponent.GetVisibleFragments();
	for(i=0; i<FragmentVis.length; i++)
	{
		// If this is a currently-visible, non-core fragment, spawn it off.
		if((FragmentVis[i] != 0) && (i != FracturedStaticMeshComponent.GetCoreFragmentIndex()))
		{
			SpawnDir = FracturedStaticMeshComponent.GetFragmentAverageExteriorNormal(i);
			PartScale = FracMesh.ExplosionPhysicsChunkScaleMin + FRand() * (FracMesh.ExplosionPhysicsChunkScaleMax - FracMesh.ExplosionPhysicsChunkScaleMin);
			// Spawn part- inherit this actors velocity
			FracPart = SpawnPart(i, (0.5 * SpawnDir * FracMesh.ChunkLinVel) + Velocity, 0.5 * VRand() * FracMesh.ChunkAngVel, PartScale, TRUE);

			if(FracPart != None)
			{
				// When something explodes we disallow collisions between all those parts.
				FracPart.FracturedStaticMeshComponent.SetRBCollidesWithChannel(RBCC_FracturedMeshPart, FALSE);
			}

			FragmentVis[i] = 0;
		}
	}

	// Update the visibility of the actor being spawned off of
	FracturedStaticMeshComponent.SetVisibleFragments(FragmentVis);
}

/** Util to break off all parts within radius of the explosion */
simulated native event BreakOffPartsInRadius(vector Origin, float Radius, float RBStrength, bool bWantPhysChunksAndParticles);

/** Unhides all fragments */
simulated native event ResetVisibility();

/** Hide one fragment from this mesh - does not spawn physics pieces etc */
simulated event HideOneFragment()
{
	local array<byte> FragmentVis;
	local int i;

	// Iterate over all fragments
	FragmentVis = FracturedStaticMeshComponent.GetVisibleFragments();
	for(i=0; i<FragmentVis.length; i++)
	{
		// If this is a currently-visible fragment, hide it
		if((FragmentVis[i] != 0) && (i != FracturedStaticMeshComponent.GetCoreFragmentIndex()))
		{
			FragmentVis[i] = 0;
			FracturedStaticMeshComponent.SetVisibleFragments(FragmentVis);
			return; // done!
		}
	}
}


/** Hide some percentage of the fragments from this mesh to maximize memory usage - does not spawn physics pieces etc */
simulated event HideFragmentsToMaximizeMemoryUsage()
{
	local array<byte> FragmentVis;
	local int i;
	local int Incr;

	// we want to break 25% of the vis to maximize the memory usage
	Incr = 4;

	// Iterate over all fragments
	FragmentVis = FracturedStaticMeshComponent.GetVisibleFragments();
	for(i=0; i<FragmentVis.length; i+=Incr)
	{
		// If this is a currently-visible fragment, hide it
		if((FragmentVis[i] != 0) && (i != FracturedStaticMeshComponent.GetCoreFragmentIndex()))
		{
			FragmentVis[i] = 0;
		}
	}

	FracturedStaticMeshComponent.SetVisibleFragments(FragmentVis);
}


/** This will possibly set the material on the FracturedMesh after we have lost a chunk. **/
simulated event SetLoseChunkReplacementMaterial()
{
	local MaterialInterface LoseChunkOutsideMat;
	local FracturedStaticMesh FracMesh;

	// If we already have created this MI, do nothing
	if(MI_LoseChunkPreviousMaterial != None)
	{
		return;
	}

	FracMesh = FracturedStaticMesh(FracturedStaticMeshComponent.StaticMesh);

	// Check override in component before one set in mesh
	if(FracturedStaticMeshComponent.LoseChunkOutsideMaterialOverride != None)
	{
		LoseChunkOutsideMat = FracturedStaticMeshComponent.LoseChunkOutsideMaterialOverride;
	}
	else
	{
		LoseChunkOutsideMat = FracMesh.LoseChunkOutsideMaterial;
	}

	// If we have a material - apply it
	if(LoseChunkOutsideMat != None)
	{
		MI_LoseChunkPreviousMaterial = FracturedStaticMeshComponent.GetMaterial( FracMesh.OutsideMaterialIndex ).GetMaterial();
		FracturedStaticMeshComponent.SetMaterial(FracMesh.OutsideMaterialIndex, LoseChunkOutsideMat);
	}
}

/**
 * Let kismet know that we've been hit
 * 
 * @param InstigatorController - the player that caused the explosion
 * 
 * @param DamageAmount - how much damage was caused
 * 
 * @param DmgType - the type of damage caused
 */
simulated function NotifyHitByExplosion(Controller InstigatorController, float DamageAmount, class<DamageType> DmgType)
{
	local int idx;
	local SeqEvent_TakeDamage DmgEvent;

	// search for any damage events
	for (idx = 0; idx < GeneratedEvents.Length; idx++)
	{
		DmgEvent = SeqEvent_TakeDamage(GeneratedEvents[idx]);
		if (DmgEvent != None)
		{
			// notify the event of the damage received
			DmgEvent.HandleDamage(self, InstigatorController, DmgType, DamageAmount);
		}
	}
}

defaultproperties
{
	MaxPartsToSpawnAtOnce=6

	ChunkHealthScale=1.0
	FractureCullMinDistance=512.0
	FractureCullMaxDistance=4096.0

	bEdShouldSnap=TRUE
	bCollideActors=TRUE
	bBlockActors=TRUE
	bWorldGeometry=TRUE
	bProjTarget=TRUE
	bGameRelevant=TRUE
	bRouteBeginPlayEvenIfStatic=FALSE
	bCollideWhenPlacing=FALSE
	bStatic=FALSE
	bMovable=FALSE
	bNoDelete=TRUE
	bPathColliding=TRUE
	// Without this explosions won't do damage to them because they are world geometry
	bCanBeDamaged=true

	Begin Object Class=DynamicLightEnvironmentComponent Name=LightEnvironment0
		bEnabled=FALSE
		bDynamic=FALSE
		bForceNonCompositeDynamicLights=TRUE // needed since we are using a static light environment
	End Object
	Components.Add(LightEnvironment0)

	Begin Object Class=FracturedSkinnedMeshComponent Name=FracturedSkinnedComponent0
		bDisableAllRigidBody=TRUE
		LightEnvironment=LightEnvironment0
	End Object
	Components.Add(FracturedSkinnedComponent0)
	SkinnedComponent=FracturedSkinnedComponent0

	Begin Object Class=FracturedStaticMeshComponent Name=FracturedStaticMeshComponent0
		WireframeColor=(R=0,G=128,B=255,A=255)
		bAllowApproximateOcclusion=TRUE
		bForceDirectLightMap=TRUE
		BlockRigidBody=TRUE
		bAcceptsStaticDecals=FALSE
		bAcceptsDynamicDecals=TRUE
		bAcceptsDecalsDuringGameplay=FALSE
		bUseDynamicIndexBuffer=TRUE
		bUseDynamicIBWithHiddenFragments=TRUE
	End Object
	CollisionComponent=FracturedStaticMeshComponent0
	FracturedStaticMeshComponent=FracturedStaticMeshComponent0
	Components.Add(FracturedStaticMeshComponent0)
}
