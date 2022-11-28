class ParticleModuleRotationOverLifetime extends ParticleModuleRotationBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
		
var() editinlinenotify export noclear	distributionfloat	RotationOverLife;

cpptext
{
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=true

	Begin Object Class=DistributionFloatConstantCurve Name=DistributionRotOverLife
	End Object
	RotationOverLife=DistributionRotOverLife
}
