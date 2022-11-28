class ParticleModuleAttractorPoint extends ParticleModuleAttractorBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

// The position of the point attractor from the source of the emitter.
var() editinlinenotify export noclear	distributionvector	Position;
// The radial range of the attractor.
var() editinlinenotify export noclear	distributionfloat	Range;
// The strength of the point attractor.
var() editinlinenotify export noclear	distributionfloat	Strength;
// The strength curve is a function of distance or of time.
var() bool													StrengthByDistance;

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
	
	StrengthByDistance=true

	Begin Object Class=DistributionVectorConstant Name=DistributionPosition
	End Object
	Position=DistributionPosition

	Begin Object Class=DistributionFloatConstant Name=DistributionRange
	End Object
	Range=DistributionRange

	Begin Object Class=DistributionFloatConstant Name=DistributionStrength
	End Object
	Strength=DistributionStrength

	bSupported3DDrawMode=true
}
