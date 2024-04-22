/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleEventReceiverKillParticles extends ParticleModuleEventReceiverBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** If TRUE, stop this emitter from spawning as well. */
var()	bool		bStopSpawning;

cpptext
{
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
}
