/*=============================================================================
	ParticleModuleTypeDataPhysX.uc: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

class ParticleModuleTypeDataPhysX extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

/** Actual wrapper for NxFluid PhsyX SDK object */
var(PhysXEmitter) PhysXParticleSystem PhysXParSys<DisplayName="PhysX Par Sys">;

/** Configuration parameters for LOD behaviour */
struct native PhysXEmitterVerticalLodProperties
{
	/** 
	Priority for removing old particles from this emitter.
	Relative low values give other emitters precedence for giving 
	up old particles.
	*/ 
	var() float WeightForFifo;
	
	/**
	Priority for spawn time particle culling and lifetime reduction.
	Relative low values give other emitters precedence for dropping 
	particles and reducing lifetimes at spawn time. 
	*/
	var() float WeightForSpawnLod;
	
	/**
	Bias for spawn time LOD. Range: [0,1]
	1.0: spawn volume reduction by culling spawned particles. 
	0.0: spawn volume reduction by lowering particle lifetimes.
	*/
    var() float SpawnLodRateVsLifeBias;
    
    /**
    Defines the fraction of the particle lifetime that is used for 
    early fading out. This setting should correspond with 
    the time span which is used to fade out particles, reducing 
    size or opacity. Range: [0,1]
    */
    var() float RelativeFadeoutTime;

	structdefaultproperties
	{
		WeightForFifo=1.0
		WeightForSpawnLod=1.0
		SpawnLodRateVsLifeBias=1.0
		RelativeFadeoutTime=0.0
	}
};

var(PhysXEmitter) PhysXEmitterVerticalLodProperties VerticalLod;

cpptext
{
	virtual FParticleEmitterInstance *CreateInstance(UParticleEmitter *InEmitterParent, UParticleSystemComponent *InComponent);

	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void FinishDestroy();

	virtual UBOOL	SupportsSubUV() const	{ return TRUE; }
}

defaultproperties
{
	PhysXParSys = none
}
