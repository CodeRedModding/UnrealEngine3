class ParticleModuleSubUV extends ParticleModuleSubUVBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

var() editinlinenotify export noclear	distributionfloat	SubImageIndex;

cpptext
{
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
}

defaultproperties
{
	bSpawnModule=false
	bUpdateModule=true

	Begin Object Class=DistributionFloatConstant Name=DistributionSubImage
	End Object
	SubImageIndex=DistributionSubImage
	
	AllowedEmitterClasses.Empty
	AllowedEmitterClasses.Add(class'Engine.ParticleSpriteEmitter')
}