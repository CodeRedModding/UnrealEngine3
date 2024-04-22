/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ParticleModuleMaterialByParameter extends ParticleModuleMaterialBase
	native(Particle)
	editinlinenew
	hidecategories(Object);

/**
 * For Sprite and SubUV emitters only the first entry in these arrays will be valid.  
 * For Mesh emitters the code will try to match the order of the materials to the ones in the mesh material arrays.
 * 
 * @see UParticleModuleMaterialByParameter::Update
 **/
var() array<name> MaterialParameters;
/** The default materials to use when the MaterialParameter is not found. */
var() editfixedsize array<MaterialInterface> DefaultMaterials;

cpptext
{
	/**
	 *	Called on a particle that is being updated by its emitter.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 *	Auto-populates the Emitter actors Instance list.
	 */
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
	bSpawnModule=false
	bUpdateModule=true
}
