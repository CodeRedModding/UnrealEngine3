class ParticleModuleTypeDataTrail extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
var(Trail)	bool				Tapered;
var(Trail)	int					TessellationFactor;

cpptext
{
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
	virtual void SetToSensibleDefaults();
	
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	virtual DWORD GetLayerMask() const;
}

defaultproperties
{
	Tapered=false
	TessellationFactor=1;
	
	AllowedEmitterClasses.Empty
	AllowedEmitterClasses.Add(class'Engine.ParticleSpriteEmitter')
}
