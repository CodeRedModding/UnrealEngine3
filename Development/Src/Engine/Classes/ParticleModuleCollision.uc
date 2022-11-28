class ParticleModuleCollision extends ParticleModuleCollisionBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);


var() editinlinenotify export noclear	distributionvector	DampingFactor;
var() editinlinenotify export noclear	distributionfloat	MaxCollisions;

var() bool	bApplyPhysics;
var() editinlinenotify export noclear	distributionfloat	ParticleMass;

cpptext
{
	virtual void Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime );
	virtual void Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime );
	virtual INT RequiredBytes();
}

defaultproperties
{
	bSpawnModule=true
	bUpdateModule=true

	Begin Object Class=DistributionVectorUniform Name=DistributionDampingFactor
	End Object
	DampingFactor=DistributionDampingFactor
	
	Begin Object Class=DistributionFloatUniform Name=DistributionMaxCollisions
	End Object
	MaxCollisions=DistributionMaxCollisions

	bApplyPhysics=false

	Begin Object Class=DistributionFloatConstant Name=DistributionParticleMass
		Constant=0.1
	End Object
	ParticleMass=DistributionParticleMass

}