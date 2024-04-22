//=============================================================================
// ParticleModuleLocationPrimitiveCylinder
// Location primitive spawning within a cylinder.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class ParticleModuleLocationPrimitiveCylinder extends ParticleModuleLocationPrimitiveBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** If TRUE, get the particle velocity form the radial distance inside the primitive. */
var(Location) bool					RadialVelocity;
/** The radius of the cylinder. */
var(Location) rawdistributionfloat	StartRadius;
/** The height of the cylinder, centered about the location. */
var(Location) rawdistributionfloat	StartHeight;

enum CylinderHeightAxis
{
	PMLPC_HEIGHTAXIS_X,
	PMLPC_HEIGHTAXIS_Y,
	PMLPC_HEIGHTAXIS_Z
};

/** Determines particle particle system axis that should represent the height of the cylinder.
 *	Can be one of the following:
 *		PMLPC_HEIGHTAXIS_X		Orient the height along the particle system X-axis.
 *		PMLPC_HEIGHTAXIS_Y		Orient the height along the particle system Y-axis.
 *		PMLPC_HEIGHTAXIS_Z		Orient the height along the particle system Z-axis.
 */
var(Location) CylinderHeightAxis	HeightAxis;

/** If TRUE and the emitter is using world space, this will more acurately calculate particles' velocity. */
var(Location) private bool			bAdjustForWorldSpace;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	/**
	 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
	 *
	 *	@param	Owner				The particle emitter instance that is spawning
	 *	@param	Offset				The offset to the modules payload data
	 *	@param	SpawnTime			The time of the spawn
	 *	@param	InRandomStream		The random stream to use for retrieving random values
	 */
	void SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream);
	virtual void	Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI);
	
	/**
	 *	Called when the module is created, this function allows for setting values that make
	 *	sense for the type of emitter they are being used in.
	 *
	 *	@param	Owner			The UParticleEmitter that the module is being added to.
	 */
	virtual void	SetToSensibleDefaults(UParticleEmitter* Owner);
}

defaultproperties
{
	RadialVelocity=true

	Begin Object Class=DistributionFloatConstant Name=DistributionStartRadius
		Constant=50.0
	End Object
	StartRadius=(Distribution=DistributionStartRadius)

	Begin Object Class=DistributionFloatConstant Name=DistributionStartHeight
		Constant=50.0
	End Object
	StartHeight=(Distribution=DistributionStartHeight)

	bSupported3DDrawMode=true

	HeightAxis=PMLPC_HEIGHTAXIS_Z
	
	bAdjustForWorldSpace=false
}
