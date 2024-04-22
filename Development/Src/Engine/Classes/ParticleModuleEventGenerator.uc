/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleEventGenerator extends ParticleModuleEventBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 */
struct native ParticleEvent_GenerateInfo
{
	/** The type of event. */
	var() EParticleEventType Type;
	/** How often to trigger the event (<= 1 means EVERY time). */
	var() int Frequency;
	/** Frequency range? (-1 indicates no - else [LowFreq..Frequency]. */
	var() int LowFreq;
	/** How often to trigger the event per particle (<= 1 means EVERY time) (collision only). */
	var() int ParticleFrequency;
	/** Only fire the first time (collision only). */
	var() bool FirstTimeOnly;
	/** Only fire the last time (collision only). */
	var() bool LastTimeOnly;
	/** Use the impact vector not the hit normal (collision only). */
	var() bool UseReflectedImpactVector;
	/** Should the event tag with a custom name? Leave blank for the default. */
	var() name CustomName;
	/** The events we want to fire off when this event has been generated */
	var() editinline array< ParticleModuleEventSendToGame > ParticleModuleEventsToSendToGame;

	structdefaultproperties
	{
		LowFreq=-1
	}
};

/**
 */
var(Events) export noclear	array<ParticleEvent_GenerateInfo> Events;

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
	 *	Called on a particle that is being updated by its emitter.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	/**
	 *	Returns the number of bytes that the module requires in the particle payload block.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return	UINT		The number of bytes the module needs per particle.
	 */
	virtual UINT	RequiredBytes(FParticleEmitterInstance* Owner = NULL);
	/**
	 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return UINT		The number fo bytes the module needs per emitter instance.
	 */
	virtual UINT	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL);
	/**
	 *	Allows the module to prep its 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	InstData	Pointer to the data block for this module.
	 */
	virtual UINT	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData);

	/**
	 *	Called when the properties change in the property window.
	 *
	 *	@param	PropertyThatChanged		The property that was edited...
	 */
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 *	Called when a particle is spawned and an event payload is present.
	 *
	 *	@param	Owner			Pointer to the owning FParticleEmitterInstance.
	 *	@param	EventPayload	Pointer to the event instance payload data.
	 *	@param	NewParticle		Pointer to the particle that was spawned.
	 *
	 *	@return	UBOOL			TRUE if processed, FALSE if not.
	 */
	virtual UBOOL	HandleParticleSpawned(FParticleEmitterInstance* Owner, FParticleEventInstancePayload* EventPayload, FBaseParticle* NewParticle);

	/**
	 *	Called when a particle is killed and an event payload is present.
	 *
	 *	@param	Owner			Pointer to the owning FParticleEmitterInstance.
	 *	@param	EventPayload	Pointer to the event instance payload data.
	 *	@param	DeadParticle	Pointer to the particle that is being killed.
	 *
	 *	@return	UBOOL			TRUE if processed, FALSE if not.
	 */
	virtual UBOOL	HandleParticleKilled(FParticleEmitterInstance* Owner, FParticleEventInstancePayload* EventPayload, FBaseParticle* DeadParticle);

	/**
	 *	Called when a particle collides and an event payload is present.
	 *
	 *	@param	Owner				Pointer to the owning FParticleEmitterInstance.
	 *	@param	EventPayload		Pointer to the event instance payload data.
	 *	@param	CollidePayload		Pointer to the collision payload data.
	 *	@param	Hit					The CheckResult for the collision.
	 *	@param	CollideParticle		Pointer to the particle that has collided.
	 *	@param	CollideDirection	The direction the particle was traveling when the collision occurred.
	 *
	 *	@return	UBOOL				TRUE if processed, FALSE if not.
	 */
	virtual UBOOL	HandleParticleCollision(FParticleEmitterInstance* Owner, FParticleEventInstancePayload* EventPayload, 
		FParticleCollisionPayload* CollidePayload, FCheckResult* Hit, FBaseParticle* CollideParticle, FVector& CollideDirection);

	/**
	 *	Called when a particle collides with a world attractor and an event payload is present.
	 *
	 *	@param	Owner				Pointer to the owning FParticleEmitterInstance.
	 *	@param	EventPayload		Pointer to the event instance payload data.
	 *	@param	CollidePayload		Pointer to the collision payload data.
	 *	@param	Hit					The CheckResult for the collision.
	 *	@param	CollideParticle		Pointer to the particle that has collided.
	 *	@param	CollideDirection	The direction the particle was traveling when the collision occurred.
	 *
	 *	@return	UBOOL				TRUE if processed, FALSE if not.
	 */
	virtual UBOOL	HandleParticleAttractorCollision(FParticleEmitterInstance* Owner, FParticleEventInstancePayload* EventPayload, 
		FParticleCollisionPayload* CollidePayload, FCheckResult* Hit, FBaseParticle* CollideParticle, FVector& CollideDirection);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
}
