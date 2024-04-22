/**
 *	ParticleModuleUberBase
 *
 *	Base-class for 'uber' modules, which combine other modules together.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class ParticleModuleUberBase extends ParticleModule
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object)
	abstract;

//------------------------------------------------------------------------------------------------
// Members
//------------------------------------------------------------------------------------------------
/** Required modules																			*/
var				const								array<name>				RequiredModules;

//------------------------------------------------------------------------------------------------
// C++ Text
//------------------------------------------------------------------------------------------------
cpptext
{
	/** This function will determine the proper uber-module to utilize.					*/
	static	UParticleModule*	DetermineBestUberModule(UParticleEmitter* InputEmitter);

	/** Used by derived classes to indicate they could be used on the given emitter.	*/
	virtual	UBOOL				IsCompatible(UParticleEmitter* InputEmitter);
	
	/** Copy the contents of the modules to the UberModule								*/
	virtual	UBOOL				ConvertToUberModule(UParticleEmitter* InputEmitter);
}

//------------------------------------------------------------------------------------------------
// Default Properties
//------------------------------------------------------------------------------------------------
defaultproperties
{
	// Required modules
	//RequiredModules
}
