class ParticleModuleColor extends ParticleModuleColorBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
/** Initial color for a particle as a function of Emitter time. Range is 0-255 for X/Y/Z, corresponding to R/G/B. */
var() editinlinenotify export noclear	distributionvector	StartColor;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=false
	bCurvesAsColor=true

	Begin Object Class=DistributionVectorConstant Name=DistributionStartColor
	End Object
	StartColor=DistributionStartColor
}