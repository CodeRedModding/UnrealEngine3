/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleEventReceiverSpawn extends ParticleModuleEventReceiverBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** The number of particles to spawn. */
var(Spawn)		rawdistributionfloat		SpawnCount;

/** 
 *	For Death-based event receiving, if this is TRUE, it indicates that the 
 *	ParticleTime of the event should be used to look-up the SpawnCount.
 *	Otherwise (and in all other events received), use the emitter time of 
 *	the event.
 */
var(Spawn)		bool						bUseParticleTime;

/**
 *	If TRUE, use the location of the particle system component for spawning.
 *	if FALSE (default), use the location of the particle event.
 */
var(Location)	bool						bUsePSysLocation;

/**
 *	If TRUE, use the velocity of the dying particle as the start velocity of 
 *	the spawned particle.
 */
var(Velocity)	bool						bInheritVelocity;

/**
 *	If bInheritVelocity is TRUE, scale the velocity with this.
 */
var(Velocity)	rawdistributionvector		InheritVelocityScale;

cpptext
{
	// UObject functionality
	
	// Event Receiver functionality
	/**
	 *	Process the event...
	 *
	 *	@param	Owner		The FParticleEmitterInstance this module is contained in.
	 *	@param	InEvent		The FParticleEventData that occurred.
	 *	@param	DeltaTime	The time slice of this frame.
	 *
	 *	@return	UBOOL		TRUE if the event was processed; FALSE if not.
	 */
	virtual UBOOL ProcessParticleEvent(FParticleEmitterInstance* Owner, FParticleEventData& InEvent, FLOAT DeltaTime);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionFloatConstant Name=RequiredDistributionSpawnCount
		Constant=0.0
	End Object
	SpawnCount=(Distribution=RequiredDistributionSpawnCount)

	Begin Object Class=DistributionVectorConstant Name=RequiredDistributionInheritVelocityScale
		Constant=(X=1.0,Y=1.0,Z=1.0)
	End Object
	InheritVelocityScale=(Distribution=RequiredDistributionInheritVelocityScale)

}
