class ParticleModuleAccelerationOverLifetime extends ParticleModuleAccelerationBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var() editinlinenotify export noclear	distributionvector	AccelOverLife;

cpptext
{
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionAccelOverLife
	End Object
	AccelOverLife=DistributionAccelOverLife
}
