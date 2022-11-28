class ParticleModuleVelocityInheritParent extends ParticleModuleVelocityBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
		
cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
}

defaultproperties
{
	bSpawnModule=true
}