//=============================================================================
// DynamicSMActor.
// A non-static version of StaticMeshActor. This class is abstract, but used as a
// base class for things like KActor and InterpActor.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class DynamicSMActor extends Actor
	native
	abstract;

var() const editconst StaticMeshComponent	StaticMeshComponent;
var() const editconst DynamicLightEnvironmentComponent LightEnvironment;
/** Used to replicate mesh to clients */
var repnotify transient StaticMesh ReplicatedMesh;
/** used to replicate the materials in indices 0 and 1 */
var repnotify MaterialInterface ReplicatedMaterial0, ReplicatedMaterial1;
/** used to replicate StaticMeshComponent.bForceStaticDecals */
var repnotify bool bForceStaticDecals;

/** Extra component properties to replicate */
var repnotify vector ReplicatedMeshTranslation;
var repnotify rotator ReplicatedMeshRotation;
var repnotify vector ReplicatedMeshScale3D;

/** If a Pawn can be 'based' on this KActor. If not, they will 'bounce' off when they try to. */
var() bool	bPawnCanBaseOn;
/** Pawn can base on this KActor if it is asleep -- Pawn will disable KActor physics while based */
var() bool	bSafeBaseIfAsleep;

cpptext
{
	/**
	* Function that gets called from within Map_Check to allow this actor to check itself
	* for any potential errors and register them with map check dialog.
	*/
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

protected:
#if USE_GAMEPLAY_PROFILER
    /** 
     * This function actually does the work for the GetProfilerAssetObject and is virtual.  
     * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
     */
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif
	/**
     * This function actually does the work for the GetDetailInfo and is virtual.
     * It should only be called from GetDetailedInfo as GetDetailedInfo is safe to call on NULL object pointers
     */
	virtual FString GetDetailedInfoInternal() const;
}

replication
{
	if (bNetDirty)
		ReplicatedMesh, ReplicatedMaterial0, ReplicatedMaterial1, ReplicatedMeshTranslation, ReplicatedMeshRotation, ReplicatedMeshScale3D, bForceStaticDecals;
}

event PostBeginPlay()
{
	Super.PostBeginPlay();

	if( StaticMeshComponent != none )
	{
		ReplicatedMesh = StaticMeshComponent.StaticMesh;
		bForceStaticDecals = StaticMeshComponent.bForceStaticDecals;
	}
}

simulated event ReplicatedEvent(name VarName)
{
	if (VarName == 'ReplicatedMesh')
	{
		if (ReplicatedMesh != StaticMeshComponent.StaticMesh)
		{
			// Enable the light environment if it is not already
			LightEnvironment.bCastShadows = false;
			LightEnvironment.SetEnabled(TRUE);

			StaticMeshComponent.SetStaticMesh(ReplicatedMesh);
		}
	}
	else if (VarName == nameof(ReplicatedMaterial0))
	{
		StaticMeshComponent.SetMaterial(0, ReplicatedMaterial0);
	}
	else if (VarName == nameof(ReplicatedMaterial1))
	{
		StaticMeshComponent.SetMaterial(1, ReplicatedMaterial1);
	}
	else if (VarName == 'ReplicatedMeshTranslation')
	{
		StaticMeshComponent.SetTranslation(ReplicatedMeshTranslation);
	}
	else if (VarName == 'ReplicatedMeshRotation')
	{
		StaticMeshComponent.SetRotation(ReplicatedMeshRotation);
	}
	else if (VarName == 'ReplicatedMeshScale3D')
	{
		StaticmeshComponent.SetScale3D(ReplicatedMeshScale3D / 100.0); // remove compensation for replication rounding
	}
	else if (VarName == nameof(bForceStaticDecals))
	{
		StaticMeshComponent.SetForceStaticDecals(bForceStaticDecals);
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

function OnSetMesh(SeqAct_SetMesh Action)
{
	local bool bForce;
	if( Action.MeshType == MeshType_StaticMesh )
	{
		bForce = Action.bIsAllowedToMove == StaticMeshComponent.bForceStaticDecals || Action.bAllowDecalsToReattach;

		if( (Action.NewStaticMesh != None) &&
			(Action.NewStaticMesh != StaticMeshComponent.StaticMesh || bForce) )
		{
			// Enable the light environment if it is not already
			LightEnvironment.bCastShadows = false;
			LightEnvironment.SetEnabled(TRUE);
			// force decals on this mesh to be treated as movable or not (if False then decals will use fastpath)
			bForceStaticDecals = !Action.bIsAllowedToMove;
			StaticMeshComponent.SetForceStaticDecals(bForceStaticDecals);
			// Don't allow decals to reattach since we are changing the static mesh
			StaticMeshComponent.bAllowDecalAutomaticReAttach = Action.bAllowDecalsToReattach;
			StaticMeshComponent.SetStaticMesh( Action.NewStaticMesh, Action.bAllowDecalsToReattach );
			StaticMeshComponent.bAllowDecalAutomaticReAttach = true;
			ReplicatedMesh = Action.NewStaticMesh;
			ForceNetRelevant();
		}
	}
}

function OnSetMaterial(SeqAct_SetMaterial Action)
{
	StaticMeshComponent.SetMaterial( Action.MaterialIndex, Action.NewMaterial );
	if (Action.MaterialIndex == 0)
	{
		ReplicatedMaterial0 = Action.NewMaterial;
		ForceNetRelevant();
	}
	else if (Action.MaterialIndex == 1)
	{
		ReplicatedMaterial1 = Action.NewMaterial;
		ForceNetRelevant();
	}
}

function SetStaticMesh(StaticMesh NewMesh, optional vector NewTranslation, optional rotator NewRotation, optional vector NewScale3D)
{
	StaticMeshComponent.SetStaticMesh(NewMesh);
	StaticMeshComponent.SetTranslation(NewTranslation);
	StaticMeshComponent.SetRotation(NewRotation);
	if (!IsZero(NewScale3D))
	{
		StaticMeshComponent.SetScale3D(NewScale3D);
		ReplicatedMeshScale3D = NewScale3D * 100.0; // avoid rounding in replication code
	}
	ReplicatedMesh = NewMesh;
	ReplicatedMeshTranslation = NewTranslation;
	ReplicatedMeshRotation = NewRotation;
	ForceNetRelevant();
}

/**
 *	Query to see if this DynamicSMActor can base the given Pawn
 */
simulated function bool CanBasePawn( Pawn P )
{
	// Can base pawn if...
	//		Pawns can be based always OR
	//		Pawns can be based if physics is not awake
	if( bPawnCanBaseOn ||
			(bSafeBaseIfAsleep &&
			 StaticMeshComponent != None &&
			!StaticMeshComponent.RigidBodyIsAwake()) )
	{
		return TRUE;
	}

	return FALSE;
}

/**
 *	If pawn is attached while asleep, turn off physics while pawn is on it
 */
event Attach( Actor Other )
{
	local Pawn P;

	super.Attach( Other );

	if( bSafeBaseIfAsleep )
	{
		P = Pawn(Other);
		if( P != None )
		{
			SetPhysics( PHYS_None );
		}
	}
}

/**
 *	If pawn is detached, turn back on physics (make sure no other pawns are based on it)
 */
event Detach( Actor Other )
{
	local int Idx;
	local Pawn P, Test;
	local bool bResetPhysics;

	super.Detach( Other );

	P = Pawn(Other);
	if( P != None )
	{
		bResetPhysics = TRUE;
		for( Idx = 0; Idx < Attached.Length; Idx++ )
		{
			Test = Pawn(Attached[Idx]);
			if( Test != None && Test != P )
			{
				bResetPhysics = FALSE;
				break;
			}
		}

		if( bResetPhysics )
		{
			SetPhysics( PHYS_RigidBody );
		}
	}
}


/**
 * This will turn "off" the light environment so it will no longer update.
 * This is useful for having a Timer call this once something has come to a stop and doesn't need 100% correct lighting.
 **/
simulated final function SetLightEnvironmentToNotBeDynamic()
{
	if( LightEnvironment != none )
	{
		LightEnvironment.bDynamic = FALSE;
	}
}


defaultproperties
{
	Begin Object Class=DynamicLightEnvironmentComponent Name=MyLightEnvironment
		bEnabled=TRUE
	End Object
	LightEnvironment=MyLightEnvironment
	Components.Add(MyLightEnvironment)

	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
	    BlockRigidBody=false
		LightEnvironment=MyLightEnvironment
		bUsePrecomputedShadows=FALSE
	End Object
	CollisionComponent=StaticMeshComponent0
	StaticMeshComponent=StaticMeshComponent0
	Components.Add(StaticMeshComponent0)

	bEdShouldSnap=true
	bWorldGeometry=false
	bGameRelevant=true
	RemoteRole=ROLE_SimulatedProxy
	bPathColliding=true

	// DynamicSMActor do not have collision as a default.  Having collision on them
	// can be very slow (e.g. matinees where the matinee is controlling where
	// the actors move and then they are trying to collide also!)
	// The overall idea is that it is really easy to see when something doesn't
	// collide correct and rectify it.  On the other hand, it is hard to see
	// something testing collision when it should not be while you wonder where
	// your framerate went.

	bCollideActors=false
	bPawnCanBaseOn=true

	// Automatically shadow parent to whatever this actor is attached to by default
	bShadowParented=true
}
