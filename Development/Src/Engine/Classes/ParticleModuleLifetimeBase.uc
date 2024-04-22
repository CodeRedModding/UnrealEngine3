/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleLifetimeBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

cpptext
{
	/** Return the maximum lifetime this module would return. */
	virtual FLOAT	GetMaxLifetime()
	{
		return 0.0f;
	}

	/**
	 *	Call to retrieve the lifetime value at the given time.
	 *
	 *	@param	Owner		The emitter instance that owns this module
	 *	@param	InTime		The time input for retrieving the lifetime value
	 *	@param	Data		The data associated with the distribution
	 *
	 *	@return	FLOAT		The Lifetime value
	 */
	virtual FLOAT	GetLifetimeValue(FParticleEmitterInstance* Owner, FLOAT InTime, UObject* Data = NULL)
		PURE_VIRTUAL(UParticleModuleLifetimeBase::GetLifetimeValue,return 0.0f;);
}
