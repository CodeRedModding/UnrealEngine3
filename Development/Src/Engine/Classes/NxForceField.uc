/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxForceField extends Actor
	native(ForceField)
	dependson(PrimitiveComponent)
	ClassGroup(Physics)
	abstract;

/** Channel id, used to identify which force field exclude volumes apply to this force field */
var()	int		ExcludeChannel;

/** Indicates whether the force is active at the moment. */
var()	bool	bForceActive;

/** Which types of object to apply this force field to */
var()	const RBCollisionChannelContainer CollideWithChannels;

/** enum indicating what collision filtering channel this force field should be in */
var()	const ERBCollisionChannel	RBChannel;

/* Pointer that stores force field */
var const native transient pointer	ForceField{class UserForceField};

/* Array storing pointers to convex meshes */
var array<const native transient pointer>	ConvexMeshes;

/* Array storing pointers to exclusion shapes (used to make them static) */
var array<const native transient pointer>	ExclusionShapes;

/* Array storing pointers to global shape poses (used to make them static) */
var array<const native transient pointer>	ExclusionShapePoses;

/*  Pointer to matrix that stores a possibly necessary rotation. */
var const native transient pointer			U2NRotation;

/** Physics scene index. */
var	native const int						SceneIndex;


cpptext
{
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void TickSpecial(FLOAT DeltaSeconds);

	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void SetForceFieldPose(FPointer ForceFieldDesc);

#if WITH_NOVODEX
	void CreateExclusionShapes(NxScene* nxScene);
#endif
}

replication
{
	if (bNetDirty)
		bForceActive;
}


/** 
 * This is used to InitRBPhys for a dynamically spawned ForceField.
 * Used for starting RBPhsys on dyanmically spawned force fields.  This will probably need to set some transient pointer to NULL  
 **/
native function DoInitRBPhys();


/** Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle inAction)
{
	if(inAction.InputLinks[0].bHasImpulse)
	{
		bForceActive = true;
	}
	else if(inAction.InputLinks[1].bHasImpulse)
	{
		bForceActive = false;
	}
	else if(inAction.InputLinks[2].bHasImpulse)
	{
		bForceActive = !bForceActive;
	}

	SetForcedInitialReplicatedProperty(Property'bForceActive', (bForceActive == default.bForceActive));
}

defaultproperties
{

	TickGroup=TG_PreAsyncWork

	RemoteRole=ROLE_SimulatedProxy
	bNoDelete=true
	bAlwaysRelevant=true
	NetUpdateFrequency=0.1
	bOnlyDirtyReplication=true

	bForceActive=true
	CollideWithChannels={(
                Default=True,
                Pawn=True,
                Vehicle=True,
                Water=True,
                GameplayPhysics=True,
                EffectPhysics=True,
                Untitled1=True,
                Untitled2=True,
                Untitled3=True,
                FluidDrain=True,
                Cloth=True,
                SoftBody=True
                )}

    RBChannel=RBCC_Nothing
}
