class ParticleModuleMeshRotation extends ParticleModuleRotationBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

/** Initial rotation distribution, in degrees. */
var() editinlinenotify export noclear	distributionvector	StartRotation;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionStartRotation
		Min=(X=0.0,Y=0.0,Z=0.0)
		Max=(X=360.0,Y=360.0,Z=360.0)
	End Object
	StartRotation=DistributionStartRotation
}
