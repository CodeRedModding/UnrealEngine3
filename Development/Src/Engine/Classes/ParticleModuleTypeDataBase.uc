class ParticleModuleTypeDataBase extends ParticleModule
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object)
	abstract;
	
cpptext
{
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
	virtual void SetToSensibleDefaults();
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=false
}