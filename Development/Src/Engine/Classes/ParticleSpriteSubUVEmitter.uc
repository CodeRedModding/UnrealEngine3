class ParticleSpriteSubUVEmitter extends ParticleSpriteEmitter
	native(Particle)
	collapsecategories		
	hidecategories(Object)
	editinlinenew
	deprecated;

enum ESubUVInterpolationMethod
{
	PSSUV_Linear,
	PSSUV_Linear_Blend,
	PSSUV_Random, 
	PSSUV_Random_Blend
};

var(Particle) int							SubImages_Horizontal;
var(Particle) int							SubImages_Vertical;
var(Particle) ESubUVInterpolationMethod		InterpolationMethod;

cpptext
{
	virtual FParticleEmitterInstance* CreateInstance( UParticleSystemComponent* InComponent );
	virtual void SetToSensibleDefaults();

	virtual DWORD GetLayerMask() const;
}

defaultproperties
{
	InterpolationMethod=PSSUV_Linear;
	SubImages_Horizontal=1;
	SubImages_Vertical=1;
}