class ParticleSpriteEmitter extends ParticleEmitter
	native(Particle)
	collapsecategories		
	hidecategories(Object)
	editinlinenew;

enum EParticleScreenAlignment
{
	PSA_Square,
	PSA_Rectangle,
	PSA_Velocity
};

var(Particle) EParticleScreenAlignment			ScreenAlignment;
var(Particle) MaterialInstance					Material;

cpptext
{
	virtual FParticleEmitterInstance* CreateInstance( UParticleSystemComponent* InComponent );
	virtual void SetToSensibleDefaults();

	virtual DWORD GetLayerMask() const;
}