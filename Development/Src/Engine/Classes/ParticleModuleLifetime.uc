class ParticleModuleLifetime extends ParticleModuleLifetimeBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var() editinlinenotify export noclear	distributionfloat	Lifetime;	

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionFloatUniform Name=DistributionLifetime
	End Object
	Lifetime=DistributionLifetime
}