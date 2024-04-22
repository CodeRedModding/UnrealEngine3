/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxForceFieldTornadoComponent extends NxForceFieldComponent
	native(ForceField);


/** Strength of the force applied by this actor.*/
var()	interp float	RadialStrength;

/** Rotational strength of the force applied around the cylinder axis.*/
var()	interp float	RotationalStrength;

/** Strength of the force applied along the cylinder axis */
var()	interp float	LiftStrength;

/** Radius of influence of the force at the bottom of the cylinder. */
var()	interp float	ForceRadius;

/** Radius of the force field at the top */
var()	interp float	ForceTopRadius;

/** Lift falloff height, 0-1, lift starts to fall off in a linear way above this height */
var()	interp float	LiftFalloffHeight;

/** Velocity above which the radial force is ignored. */
var()	interp float	EscapeVelocity;

/** Height of force cylinder */
var()	interp float	ForceHeight;

/** Offset from the actor base to the center of the force field */
var()	interp float	HeightOffset;

/**  */
var()	bool BSpecialRadialForceMode;

/** */
var()	interp float	SelfRotationStrength;

/** custom force field kernel */
var const native transient pointer		Kernel{class NxForceFieldKernelTornadoAngular};

cpptext
{
	virtual void  TermComponentRBPhys (FRBPhysScene *InScene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void CreateKernel();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
	
#endif
}

defaultproperties
{
	Begin Object Class=ForceFieldShapeCapsule Name=Shape0
	End Object
	
	Shape = Shape0
	
	ForceRadius=200.0
	ForceTopRadius=200.0
	ForceHeight=200.0
	LiftStrength=10.0
}


