class ParticleModuleTypeDataMesh extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var(Particle) StaticMesh		Mesh;			// The Base Mesh
var(Particle) bool				CastShadows;
var(Particle) bool				DoCollisions;

cpptext
{
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
	virtual void SetToSensibleDefaults();
	
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	virtual DWORD GetLayerMask() const;
}

defaultproperties
{
	CastShadows=false
	DoCollisions=false
	
	AllowedEmitterClasses.Empty
	AllowedEmitterClasses.Add(class'Engine.ParticleSpriteEmitter')
}
