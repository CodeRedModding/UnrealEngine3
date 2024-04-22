/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleEventBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

cpptext
{
	/**
	 *	Retrieve the ModuleType of this module.
	 *
	 *	@return	EModuleType		The type of module this is.
	 */
	virtual EModuleType	GetModuleType() const	{	return EPMT_Event;	}
}
