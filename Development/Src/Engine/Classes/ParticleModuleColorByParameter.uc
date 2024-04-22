/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleModuleColorByParameter extends ParticleModuleColorBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/** The name of the parameter to retrieve the color from. */
var(Color) name		ColorParam;
/** The default color to use in the even that the parameter is not set on the emitter. */
var(Color) color	DefaultColor;

cpptext
{
	virtual void	Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime);
	virtual void	AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp);

	/**
	 *	Retrieve the ParticleSysParams associated with this module.
	 *
	 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
	 */
	virtual void GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=false
	
	DefaultColor=(R=255,G=255,B=255,A=255)
}
