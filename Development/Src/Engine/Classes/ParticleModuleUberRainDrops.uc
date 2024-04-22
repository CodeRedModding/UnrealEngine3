/**
 *	ParticleModuleUberRainDrops
 *
 *	Uber-module replacing the following classes:
 *		LT  - Lifetime
 *		IS  - Initial Size
 *		IV  - Initial Velocity
 *		COL - Color Over Life
 *		PC	- Primitive Cylinder (optional)
 *		IL	- Initial Location
 *
 *	Intended for use in the Rain particle system.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class ParticleModuleUberRainDrops extends ParticleModuleUberBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

//*-----------------------------------------------------------------------------*/
/** Lifetime Module Members														*/
//*-----------------------------------------------------------------------------*/
var(Lifetime)	float				LifetimeMin;
var(Lifetime)	float				LifetimeMax;

//*-----------------------------------------------------------------------------*/
/** Size Module Members															*/
//*-----------------------------------------------------------------------------*/
var(Size)		vector				StartSizeMin;
var(Size)		vector				StartSizeMax;

//*-----------------------------------------------------------------------------*/
/** Velocity Module Members														*/
//*-----------------------------------------------------------------------------*/
var(Velocity)	vector				StartVelocityMin;
var(Velocity)	vector				StartVelocityMax;
var(Velocity)	float				StartVelocityRadialMin;
var(Velocity)	float				StartVelocityRadialMax;

//*-----------------------------------------------------------------------------*/
/** ColorOverLife Module Members												*/
//*-----------------------------------------------------------------------------*/
var(Color)		vector				ColorOverLife;
var(Color)		float				AlphaOverLife;

//*-----------------------------------------------------------------------------*/
/** PrimitiveCylinder Module Members											*/
//*-----------------------------------------------------------------------------*/
var(Location)	bool				bIsUsingCylinder;
var(Location)	bool				bPositive_X;
var(Location)	bool				bPositive_Y;
var(Location)	bool				bPositive_Z;
var(Location)	bool				bNegative_X;
var(Location)	bool				bNegative_Y;
var(Location)	bool				bNegative_Z;
var(Location)	bool				bSurfaceOnly;
var(Location)	bool				bVelocity;
var(Location)	float				PC_VelocityScale;
var(Location)	vector				PC_StartLocation;
var(Location)	bool				bRadialVelocity;
var(Location)	float				PC_StartRadius;
var(Location)	float				PC_StartHeight;
var(Location)	CylinderHeightAxis	PC_HeightAxis;

//*-----------------------------------------------------------------------------*/
/** Location Module Members														*/
//*-----------------------------------------------------------------------------*/
var(Location)		vector					StartLocationMin;
var(Location)		vector					StartLocationMax;

//*-----------------------------------------------------------------------------*/
//*-----------------------------------------------------------------------------*/
cpptext
{
	virtual void	PostLoad();
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	virtual void	Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI);

	void	DetermineUnitDirection(FParticleEmitterInstance* Owner, FVector& vUnitDir);

	/** Used by derived classes to indicate they could be used on the given emitter.	*/
	virtual	UBOOL				IsCompatible(UParticleEmitter* InputEmitter);
	
	/** Copy the contents of the modules to the UberModule								*/
	virtual	UBOOL				ConvertToUberModule(UParticleEmitter* InputEmitter);
}

//*-----------------------------------------------------------------------------*/
//*-----------------------------------------------------------------------------*/
defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
	bSupported3DDrawMode=true

	//*-----------------------------------------------------------------------------*/
	/** Lifetime Module Defaults													*/
	//*-----------------------------------------------------------------------------*/
	LifetimeMin=1.0
	LifetimeMax=1.0

	//*-----------------------------------------------------------------------------*/
	/** Size Module Defaults														*/
	//*-----------------------------------------------------------------------------*/
	StartSizeMin=(X=1,Y=1,Z=1)
	StartSizeMax=(X=1,Y=1,Z=1)

	//*-----------------------------------------------------------------------------*/
	/** Velocity Module Defaults													*/
	//*-----------------------------------------------------------------------------*/
	StartVelocityMin=(X=1,Y=1,Z=1)
	StartVelocityMax=(X=1,Y=1,Z=1)
	StartVelocityRadialMin=0.0
	StartVelocityRadialMax=0.0

	//*-----------------------------------------------------------------------------*/
	/** ColorOverLife Module Defaults												*/
	//*-----------------------------------------------------------------------------*/
	ColorOverLife=(X=255.9f,Y=255.9f,Z=255.9f)
	AlphaOverLife=255.9f

	//*-----------------------------------------------------------------------------*/
	/** PrimitiveCylinder Module Defaults											*/
	//*-----------------------------------------------------------------------------*/
	bIsUsingCylinder=false
	bPositive_X=true
	bPositive_Y=true
	bPositive_Z=true
	bNegative_X=true
	bNegative_Y=true
	bNegative_Z=true
	bSurfaceOnly=false
	bVelocity=false
	PC_VelocityScale=1.0
	PC_StartLocation=(X=0,Y=0,Z=0)
	bRadialVelocity=true
	PC_StartRadius=50.0
	PC_StartHeight=50.0
	PC_HeightAxis=PMLPC_HEIGHTAXIS_Z

	//*-----------------------------------------------------------------------------*/
	/** Location Module Defaults													*/
	//*-----------------------------------------------------------------------------*/
	StartLocationMin=(X=0,Y=0,Z=0)
	StartLocationMax=(X=0,Y=0,Z=0)
}
