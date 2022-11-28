class ParticleModuleMeshRotationRate extends ParticleModuleRotationRateBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

/** Initial rotation rate distribution, in degrees per second. */
var() editinlinenotify export noclear	distributionvector	StartRotationRate;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionStartRotationRate
		Min=(X=0.0,Y=0.0,Z=0.0)
		Max=(X=360.0,Y=360.0,Z=360.0)
	End Object
	StartRotationRate=DistributionStartRotationRate
}
