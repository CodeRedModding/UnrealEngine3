/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RB_Spring extends ActorComponent
	native(Physics);

cpptext
{
	// ActorComponent interface

	virtual void Tick(FLOAT DeltaTime);
	virtual void TermComponentRBPhys(FRBPhysScene* InScene);
}

/** PrimitiveComponent attached to one end of this spring. */
var const PrimitiveComponent	Component1;

/** Optional name of bone inside Component1 that spring is attached to (for PhysicsAsset case). */
var const name					BoneName1;

/** PrimitiveComponent attached to other end of this spring. */
var const PrimitiveComponent	Component2;

/** Optional name of bone inside Component2 that spring is attached to (for PhysicsAsset case). */
var const name					BoneName2;

/** Physics scene index. */
var	native const int			SceneIndex;

/** Whether we are in the hardware or software scene. */
var native const bool			bInHardware;

/** Internal phyiscs engine use. */
var native const pointer		SpringData;

/** Zeroed when SetComponents is called, this indicates the time the spring has been acting. */
var native const float			TimeSinceActivation;

/** Minimum mass of bodies connected by spring. */
var const float					MinBodyMass;

/** Spring extension at which maximum spring force is applied. Force linear ramps up to this point and is constant beyond it. */
var() float					SpringSaturateDist;

/** Max linear force applied by spring. Multiplied by SpringMaxForceTimeScale before being passed to simulation. */
var() float					SpringMaxForce;

/** If bEnableForceMassRatio is true, this is maximum allowed ratio between MinBodyMass and the applied spring force. */
var() float					MaxForceMassRatio;

/** Allows you to limit the maximum force applied by spring based on MinBodyMass. */
var() bool					bEnableForceMassRatio;


/**
 *	Allows scaling of spring force over time. Time is zeroed when SetComponents is called,
 *	and this curve is a scaling of SpringMaxForce over time from then (in seconds).
 */
var() InterpCurveFloat		SpringMaxForceTimeScale;

/** Linear velocity (along spring direction) at which damping force is maximum. */
var() float					DampSaturateVel;

/** Maximum velocity damping force applied between sprung bodies. */
var() float					DampMaxForce;


native function SetComponents(PrimitiveComponent InComponent1, Name InBoneName1, vector Position1, PrimitiveComponent InComponent2, Name InBoneName2, vector Position2 );
native function Clear();

defaultproperties
{
	// Various physics related items need to be ticked pre physics update
	TickGroup=TG_PreAsyncWork

	SpringMaxForceTimeScale=(Points=((InVal=0,OutVal=1.0)))
}
