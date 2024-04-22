/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxGenericForceField extends NxForceField
	native(ForceField)
	abstract;

/** Type of Coordinates that can be used to define the force field */
/*enum FFG_ForceFieldCoordinates
{
	FFG_CARTESIAN,
	FFG_SPHERICAL,
	FFG_CYLINDRICAL,
	FFG_TOROIDAL
};*/

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
var const native transient pointer		LinearKernel{class UserForceFieldLinearKernel};

cpptext
{
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);

	virtual void TickSpecial(FLOAT DeltaSeconds);

	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
}

defaultproperties
{

	TickGroup=TG_PreAsyncWork

	RemoteRole=ROLE_SimulatedProxy
	bNoDelete=true
	bAlwaysRelevant=true
	NetUpdateFrequency=0.1
	bOnlyDirtyReplication=true

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
}
