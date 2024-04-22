/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxForceFieldGenericComponent extends NxForceFieldComponent
	DependsOn(NxForceFieldGeneric)
	native(ForceField);

/* the Shape's internal 3 directional radii, for level designers to know the rough size of the force field*/
var() float RoughExtentX;
var() float RoughExtentY;
var() float RoughExtentZ;

/** Type of Coordinates to define the force field */
var()	FFG_ForceFieldCoordinates	Coordinates;

/** Constant force vector that is applied inside force volume */
var()	vector	Constant;


/** Rows of matrix that defines force depending on position */
var()	vector	PositionMultiplierX;
var()	vector	PositionMultiplierY;
var()	vector	PositionMultiplierZ;

/** Vector that defines force depending on position */
var()	vector	PositionTarget;


/** Rows of matrix that defines force depending on velocity */
var()	vector	VelocityMultiplierX;
var()	vector	VelocityMultiplierY;
var()	vector	VelocityMultiplierZ;

/** Vector that defines force depending on velocity */
var()	vector	VelocityTarget;

/** Vector that scales random noise added to the force */
var()	vector	Noise;

/** Linear falloff for vector in chosen coordinate system */
var()	vector	FalloffLinear;

/** Quadratic falloff for vector in chosen coordinate system */
var()	vector	FalloffQuadratic;

/** Radius of torus in case toroidal coordinate system is used */
var()	float	TorusRadius;

/** linear force field kernel */
var const native transient pointer		Kernel{class UserForceFieldLinearKernel};


cpptext
{
	virtual void  TermComponentRBPhys (FRBPhysScene *InScene);
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void CreateKernel();
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
}

defaultproperties
{
	Begin Object Class=ForceFieldShapeBox Name=Shape0
	End Object
	
	Shape = Shape0
	
	Coordinates=FFG_CARTESIAN;
	Constant=(X=0.0,Y=0.0,Z=0.0);
	PositionMultiplierX=(X=0.0,Y=0.0,Z=0.0);
	PositionMultiplierY=(X=0.0,Y=0.0,Z=0.0);
	PositionMultiplierZ=(X=0.0,Y=0.0,Z=0.0);
	PositionTarget=(X=0.0,Y=0.0,Z=0.0);
	VelocityMultiplierX=(X=0.0,Y=0.0,Z=0.0);
	VelocityMultiplierY=(X=0.0,Y=0.0,Z=0.0);
	VelocityMultiplierZ=(X=0.0,Y=0.0,Z=0.0);
	VelocityTarget=(X=0.0,Y=0.0,Z=0.0);
	FalloffLinear=(X=0.0,Y=0.0,Z=0.0);
	FalloffQuadratic=(X=0.0,Y=0.0,Z=0.0);
	TorusRadius=1.0;
	Noise=(X=0.0,Y=0.0,Z=0.0);
	
	RoughExtentX=200
	RoughExtentY=200
	RoughExtentZ=200
}

