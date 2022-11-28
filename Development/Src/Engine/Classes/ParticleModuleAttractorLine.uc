class ParticleModuleAttractorLine extends ParticleModuleAttractorBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

// The endpoints of the line
var() vector												EndPoint0;
var() vector												EndPoint1;
// The range of the line attractor.
var() editinlinenotify export noclear	distributionfloat	Range;
// The strength of the line attractor.
var() editinlinenotify export noclear	distributionfloat	Strength;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );

	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
	
	Begin Object Class=DistributionFloatConstant Name=DistributionStrength
	End Object
	Strength=DistributionStrength

	Begin Object Class=DistributionFloatConstant Name=DistributionRange
	End Object
	Range=DistributionRange
	
	bSupported3DDrawMode=true
}
