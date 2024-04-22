/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxForceFieldComponent extends PrimitiveComponent
	native(ForceField)
	hidecategories(Collision,Lighting,Physics,Rendering,Object)
    collapsecategories
    editinlinenew
	abstract;

var() instanced editinline ForceFieldShape Shape;

var ActorComponent DrawComponent;

/** Channel id, used to identify which force field exclude volumes apply to this force field */
var()	int		ExcludeChannel;

/** Indicates whether the force is active at the moment. */
var()	bool	bForceActive;

/** Destroy the forcefield from the actor when it become inactive */
var()	bool	bDestroyWhenInactive;

/** Which types of object to apply this force field to */
var()	const RBCollisionChannelContainer CollideWithChannels;

/** Time in seconds until force field expiration for FMT_CONSTANT type force mode.  0 = Infinite*/
var()   float   Duration;

/* Pointer that stores force field */
var const native transient pointer	ForceField{class UserForceField};

/* Array storing pointers to convex meshes */
var array<const native transient pointer>	ConvexMeshes;

/* Array storing pointers to exclusion shapes (used to make them static) */
var array<const native transient pointer>	ExclusionShapes;

/* Array storing pointers to global shape poses (used to make them static) */
var array<const native transient pointer>	ExclusionShapePoses;

/** Physics scene index. */
var	native const int						SceneIndex;

/** Internal variable for storing the elapsed time of the force field */
var     float   ElapsedTime;

/** Internal variable to hold the render component for attaching and detaching*/
var     PrimitiveComponent RenderComponent;

/* Pointer that stores the physics Scene */
var native pointer RBPhysScene {class FRBPhysScene};


native function DoInitRBPhys();

cpptext
{
	virtual void Tick(FLOAT DeltaSeconds); //This will call ApplyForces on derived classes if the force is active
	virtual void CreateKernel();
	virtual UBOOL AllowBeingMarkedPendingKill(void);
	virtual void  InitComponentRBPhys (UBOOL bFixed);
	virtual void  InitForceField(FRBPhysScene *InScene);
	virtual void  TermComponentRBPhys (FRBPhysScene *InScene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void SetForceFieldPose(FPointer ForceFieldDesc);
	virtual void PostLoad();
	virtual void UpdateTransform();

#if WITH_NOVODEX
	void CreateExclusionShapes(NxScene* nxScene);
#endif
}

defaultproperties
{
	TickGroup=TG_PreAsyncWork
    bForceActive=True
	bDestroyWhenInactive=True
	Duration = 0.0
	ElapsedTime = 0.0
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
                Cloth=True
                )}

	RBChannel=RBCC_Nothing;
}

