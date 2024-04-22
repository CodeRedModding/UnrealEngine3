/**
 *	ParticleModuleTrailBase
 *	Provides the base class for Trail emitter modules
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleTrailBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

//*************************************************************************************************
// C++ Text
//*************************************************************************************************
cpptext
{
	virtual EModuleType	GetModuleType() const	{	return EPMT_Trail;	}
}

//*************************************************************************************************
// Default properties
//*************************************************************************************************
defaultproperties
{
	bSpawnModule=false
	bUpdateModule=false
}
