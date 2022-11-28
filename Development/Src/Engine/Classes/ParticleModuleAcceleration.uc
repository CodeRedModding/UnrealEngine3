class ParticleModuleAcceleration extends ParticleModuleAccelerationBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var() editinlinenotify export noclear	distributionvector	Acceleration;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
	virtual INT RequiredBytes();
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionAcceleration
	End Object
	Acceleration=DistributionAcceleration
}