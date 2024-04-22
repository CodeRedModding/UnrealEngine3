/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class NxGenericForceFieldBrush extends Volume
	native(ForceField)
	dependson(PrimitiveComponent)
	placeable;


/** Type of Coordinates to define the force field */
// TODO how to use the ForceFieldCoordinates from NxGenericForceField?
enum FFB_ForceFieldCoordinates
{
	FFB_CARTESIAN,
	FFB_SPHERICAL,
	FFB_CYLINDRICAL,
	FFB_TOROIDAL
};


/** Channel id, used to identify which force field exclude volumes apply to this force field */
var()	int		ExcludeChannel;

/** Which types of object to apply this force field to */
var()	RBCollisionChannelContainer	CollideWithChannels;

/** enum indicating what collision filtering channel this force field should be in */
var()	const ERBCollisionChannel	RBChannel;

var()	FFB_ForceFieldCoordinates	Coordinates;

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


/** Value to scale force on fluid */
//var()	float	FluidScale;

/** Value to scale force on cloth */
//var()	float	ClothScale;

/** Value to scale force on rigid body */
//var()	float	RigidBodyScale;

/** Value to scale force on soft body */
//var()	float	SoftBodyScale;

/* Pointer that stores force field */
var const native transient pointer	ForceField{class UserForceField};

/* Array storing pointers to convex meshes */
var array<const native transient pointer>	ConvexMeshes;

/* Array storing pointers to exclusion shapes (used to make them static) */
var array<const native transient pointer>	ExclusionShapes;

/* Array storing pointers to global shape poses (used to make them static) */
var array<const native transient pointer>	ExclusionShapePoses;

/** linear force field kernel */
var const native transient pointer		LinearKernel{class UserForceFieldLinearKernel};

cpptext
{
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
	virtual void TickSpecial(FLOAT DeltaSeconds);
}

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();

	// match bProjTarget to weapons (zero extent) collision setting
	if (BrushComponent != None)
	{
		bProjTarget = BrushComponent.BlockZeroExtent;
	}
}

simulated function bool StopsProjectile(Projectile P)
{
	return false;
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		bDisableAllRigidBody=false
	End Object

	bStatic=false
	bColored=true
	BrushColor=(R=100,G=255,B=100,A=255)

	bCollideActors=true
	bProjTarget=true
	SupportedEvents.Empty

	ExcludeChannel=1

	Coordinates=FFB_CARTESIAN;
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
	//FluidScale=1.0;
	//ClothScale=1.0;
	//SoftBodyScale=1.0;
	//RigidBodyScale=1.0;

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

    RBChannel=RBCC_Untitled1

}
