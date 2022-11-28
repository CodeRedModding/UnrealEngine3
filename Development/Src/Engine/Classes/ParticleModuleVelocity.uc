class ParticleModuleVelocity extends ParticleModuleVelocityBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
		
var() editinlinenotify export noclear	distributionvector	StartVelocity;
var() editinlinenotify export noclear	distributionfloat	StartVelocityRadial;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true
	
	Begin Object Class=DistributionVectorUniform Name=DistributionStartVelocity
	End Object
	StartVelocity=DistributionStartVelocity
	
	Begin Object Class=DistributionFloatUniform Name=DistributionStartVelocityRadial
	End Object
	StartVelocityRadial=DistributionStartVelocityRadial
}