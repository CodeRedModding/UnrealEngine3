class ParticleModuleTypeDataSubUV extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);
	
enum ESubUVInterpMethod
{
    PSSUVIM_Linear,
    PSSUVIM_Linear_Blend,
    PSSUVIM_Random,
    PSSUVIM_Random_Blend
};

var(Particle) int					SubImages_Horizontal;
var(Particle) int					SubImages_Vertical;
var(Particle) ESubUVInterpMethod	InterpolationMethod;

cpptext
{
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
	virtual void SetToSensibleDefaults();

	virtual void PostEditChange(UProperty* PropertyThatChanged);

}

defaultproperties
{
	InterpolationMethod=PSSUV_Linear;
	SubImages_Horizontal=1;
	SubImages_Vertical=1;

	AllowedEmitterClasses.Empty
	AllowedEmitterClasses.Add(class'Engine.ParticleSpriteEmitter')
}
