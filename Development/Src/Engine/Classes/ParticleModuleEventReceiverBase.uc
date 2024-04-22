/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleEventReceiverBase extends ParticleModuleEventBase
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

/** The type of event that will generate the kill. */
var(Source)		EParticleEventType			EventGeneratorType;

/** The name of the emitter of interest for generating the event. */
var(Source)		name						EventName;

cpptext
{
	/**
	 *	Is the module interested in events of the given type?
	 *
	 *	@param	InEventType		The event type to check
	 *
	 *	@return	UBOOL			TRUE if interested.
	 */
	virtual UBOOL WillProcessParticleEvent(EParticleEventType InEventType)
	{
		if ((EventGeneratorType == EPET_Any) || (InEventType == EventGeneratorType))
		{
			return TRUE;
		}

		return FALSE;
	}

	/**
	 *	Process the event...
	 *
	 *	@param	Owner		The FParticleEmitterInstance this module is contained in.
	 *	@param	InEvent		The FParticleEventData that occurred.
	 *	@param	DeltaTime	The time slice of this frame.
	 *
	 *	@return	UBOOL		TRUE if the event was processed; FALSE if not.
	 */
	virtual UBOOL ProcessParticleEvent(FParticleEmitterInstance* Owner, FParticleEventData& InEvent, FLOAT DeltaTime)
	{
		return FALSE;
	}
}
