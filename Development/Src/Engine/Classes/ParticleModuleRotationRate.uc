class ParticleModuleRotationRate extends ParticleModuleRotationRateBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

/** Initial rotation rate distribution, in degrees per second. */
var() editinlinenotify export noclear	distributionfloat	StartRotationRate;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionFloatConstant Name=DistributionStartRotationRate
	End Object
	StartRotationRate=DistributionStartRotationRate
}
	