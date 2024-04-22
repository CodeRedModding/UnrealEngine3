/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxForceFieldGeneric extends NxForceField
	native(ForceField)
	placeable;

var() editinline ForceFieldShape Shape;

var ActorComponent DrawComponent;

/* the Shape's internal 3 directional radii, for level designers to know the rough size of the force field*/
var() float RoughExtentX;
var() float RoughExtentY;
var() float RoughExtentZ;

/** Type of Coordinates that can be used to define the force field */
enum FFG_ForceFieldCoordinates
{
	FFG_CARTESIAN,
	FFG_SPHERICAL,
	FFG_CYLINDRICAL,
	FFG_TOROIDAL
};

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

	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual FPointer DefineForceFieldShapeDesc();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);
#endif
	virtual void PostLoad();
}


/** 
 * This is used to InitRBPhys for a dynamically spawned ForceField.
 * Used for starting RBPhsys on dyanmically spawned force fields.  This will probably need to set some transient pointer to NULL  
 **/
native function DoInitRBPhys();


defaultproperties
{
	Begin Object Class=SpriteComponent Name=Sprite
		Sprite=Texture2D'EditorResources.S_RadForce'
		HiddenGame=True
		AlwaysLoadOnClient=False
		AlwaysLoadOnServer=False
		SpriteCategoryName="Physics"
	End Object
	Components.Add(Sprite)

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

	RoughExtentX=200
	RoughExtentY=200
	RoughExtentZ=200
}
