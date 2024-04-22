/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleCollisionActor extends ParticleModuleCollision
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 *	List of actor parameter names (set on the placed instance) that this particle emitter should collide against
 */
var(Actors) array<name>	ActorsToCollideWith;

/**
 *	If TRUE, then collide with any pawns as well
 */
var(Actors) bool bCheckPawnCollisions;

cpptext
{
	virtual void	Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	/**
	 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *
	 *	@return UINT		The number of bytes the module needs per emitter instance.
	 */
	virtual UINT RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL);

	/**
	 *	Allows the module to prep its 'per-instance' data block.
	 *	
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	InstData	Pointer to the data block for this module.
	 */
	virtual UINT PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData);

	/**
	 *	Perform the desired collision check for this module.
	 *	
	 *	@param	Owner			The emitter instance that owns the particle being checked
	 *	@param	InParticle		The particle being checked for a collision
	 *	@param	Hit				The hit results to fill in for a collision
	 *	@param	SourceActor		The source actor for the check
	 *	@param	End				The end position for the check
	 *	@param	Start			The start position for the check
	 *	@param	TraceFlags		The trace flags to use for the check
	 *	@param	Extent			The extent to use for the check
	 *	
	 *	@return UBOOL			TRUE if a collision occurred.
	 */
	virtual UBOOL PerformCollisionCheck(FParticleEmitterInstance* Owner, FBaseParticle* InParticle, 
		FCheckResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, DWORD TraceFlags, const FVector& Extent);

	/**
	 *	Helper function used by the editor to auto-populate a placed AEmitter with any
	 *	instance parameters that are utilized.
	 *
	 *	@param	PSysComp		The particle system component to be populated.
	 */
	virtual void AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp);
}

defaultproperties
{
}
