class ParticleModuleColorByParameter extends ParticleModuleColorBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var() name	ColorParam;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=false
}