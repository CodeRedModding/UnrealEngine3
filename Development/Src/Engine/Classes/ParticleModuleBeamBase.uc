/**
 *	ParticleModuleBeamBase
 *
 *	The base class for all Beam modules.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ParticleModuleBeamBase extends ParticleModule
	native(Particle)
	editinlinenew
	hidecategories(Object)
	abstract;

/** The method to use in determining the source/target. */
enum Beam2SourceTargetMethod
{
	/** Default	- use the distribution. 
	 *	This is the fallback for when other modes can't be resolved.
	 */
	PEB2STM_Default,
	/** UserSet	- use the user set value. 
	 *	Primarily intended for weapon effects.
	 */
    PEB2STM_UserSet,
	/** Emitter	- use the emitter position as the source/target.
	 */
	PEB2STM_Emitter,
	/** Particle	- use the particles from a given emitter in the system.		
	 *	The name of the emitter should be set in <Source/Target>Name.
	 */
	PEB2STM_Particle,
	/** Actor		- use the actor as the source/target.
	 *	The name of the actor should be set in <Source/Target>Name.
	 */
	PEB2STM_Actor
};

/** The method to use in determining the source/target tangent. */
enum Beam2SourceTargetTangentMethod
{
	/** Direct - a direct line between source and target.				 */
	PEB2STTM_Direct,
	/** UserSet	- use the user set value.								 */
    PEB2STTM_UserSet,
	/** Distribution - use the distribution.							 */
    PEB2STTM_Distribution,
	/** Emitter	- use the emitter direction.							 */
	PEB2STTM_Emitter
};

cpptext
{
	virtual EModuleType	GetModuleType() const	{	return EPMT_Beam;	}
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=false
}
