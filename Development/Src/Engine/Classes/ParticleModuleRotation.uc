class ParticleModuleRotation extends ParticleModuleRotationBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

/** Initial rotation distribution, in degrees. */
var() editinlinenotify export noclear	distributionfloat	StartRotation;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionFloatUniform Name=DistributionStartRotation
		Min=0.0
		Max=360.0
	End Object
	StartRotation=DistributionStartRotation
}
	