class ParticleModuleSize extends ParticleModuleSizeBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var() editinlinenotify export noclear	distributionvector	StartSize;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=false

	Begin Object Class=DistributionVectorUniform Name=DistributionStartSize
		Min=(X=1,Y=1,Z=1)
		Max=(X=1,Y=1,Z=1)
	End Object
	StartSize=DistributionStartSize
}