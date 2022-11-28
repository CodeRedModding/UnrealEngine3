class ParticleMeshEmitter extends ParticleEmitter
	native(Particle)
	collapsecategories		
	hidecategories(Object)
	editinlinenew
	deprecated;

var(Particle) StaticMesh		Mesh;			// The Base Mesh
var(Particle) bool				CastShadows;
var(Particle) bool				DoCollisions;

cpptext
{
	virtual FParticleEmitterInstance* CreateInstance( UParticleSystemComponent* InComponent );
	virtual void SetToSensibleDefaults();

	virtual DWORD GetLayerMask() const;
}
