/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxCylindricalForceField extends NxForceField
	native(ForceField)
	abstract;


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

/** Whether to use a special radial force */
var()	bool	UseSpecialRadialForce;

/** custom force field kernel */
var const native transient pointer		Kernel{class NxForceFieldKernelSample};

cpptext
{
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
}


defaultproperties
{
	ForceRadius=200.0
}
