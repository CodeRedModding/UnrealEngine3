class ParticleModuleColorOverLife extends ParticleModuleColorBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var() editinlinenotify export noclear	distributionvector	ColorOverLife;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true
	bCurvesAsColor=true

	Begin Object Class=DistributionVectorConstant Name=DistributionColor
	End Object
	ColorOverLife=DistributionColor
}