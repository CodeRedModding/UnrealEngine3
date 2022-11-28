class ParticleModuleVelocityOverLifetime extends ParticleModuleVelocityBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
		
var() editinlinenotify export noclear	distributionvector	VelOverLife;

cpptext
{
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=true

	Begin Object Class=DistributionVectorConstantCurve Name=DistributionVelOverLife
	End Object
	VelOverLife=DistributionVelOverLife
}
