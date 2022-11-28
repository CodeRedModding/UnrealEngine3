class ParticleModuleLocation extends ParticleModuleLocationBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var() editinlinenotify export noclear	distributionvector	StartLocation;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
}

defaultproperties
{
	bSpawnModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionStartLocation
	End Object
	StartLocation=DistributionStartLocation

	bSupported3DDrawMode=true
}
	