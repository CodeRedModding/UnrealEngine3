/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleVelocityCone extends ParticleModuleVelocityBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** The Min value represents the inner cone angle value and the Max value represents the outer cone angle value. */
var(Cone) rawdistributionfloat Angle;

/** The initial velocity of the particles. */
var(Cone) rawdistributionfloat Velocity;

/** The direction vector of the cone. */
var(Cone) vector Direction;

cpptext
{
	/**
	 *	Called on a particle that is freshly spawned by the emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	SpawnTime	The time of the spawn.
	 */
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	
	/**
	 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
	 *
	 *	@param	Owner				The particle emitter instance that is spawning
	 *	@param	Offset				The offset to the modules payload data
	 *	@param	SpawnTime			The time of the spawn
	 *	@param	InRandomStream		The random stream to use for retrieving random values
	 */
	void			SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream);

	/** 
	 *	Render the modules 3D visualization helper primitive.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the module.
	 *	@param	View		The scene view that is being rendered.
	 *	@param	PDI			The FPrimitiveDrawInterface to use for rendering.
	 */
	virtual void	Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI);
}

defaultproperties
{
	bSpawnModule=true
	bSupported3DDrawMode=true
	
	Begin Object Class=DistributionFloatUniform Name=DistributionAngle
	End Object
	Angle=(Distribution=DistributionAngle)
	
	Begin Object Class=DistributionFloatUniform Name=DistributionVelocity
	End Object
	Velocity=(Distribution=DistributionVelocity)
	
	Direction=(X=0,Y=0,Z=1)
}
