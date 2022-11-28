class ParticleModuleRotationRateMultiplyLife extends ParticleModuleRotationRateBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

var() editinlinenotify export noclear	distributionfloat	LifeMultiplier;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionFloatConstant Name=DistributionLifeMultiplier
	End Object
	LifeMultiplier=DistributionLifeMultiplier
}