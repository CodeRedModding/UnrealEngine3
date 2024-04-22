
#ifndef PARTICLE_EMITTER_INSTANCES_APEX_H
#define PARTICLE_EMITTER_INSTANCES_APEX_H

#if WITH_APEX_PARTICLES

class FParticleApexEmitterInstance : public FParticleEmitterInstance
{
public:
	DECLARE_PARTICLEEMITTERINSTANCE_TYPE(FParticleSpriteApexEmitterInstance, FParticleEmitterInstance);

	/** Constructor	*/
	FParticleApexEmitterInstance(class UParticleModuleTypeDataApex &TypeData);

	/** Destructor	*/
	virtual ~FParticleApexEmitterInstance(void);

	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);

	/** Forwards call to correspnding FParticleMeshPhysXEmitterRenderInstance object. */
	virtual FDynamicEmitterDataBase* GetDynamicData(UBOOL bSelected);

	class FApexEmitter * GetApexEmitter(void) const { return ApexEmitter; };


private:
	FRBPhysScene *GetScene(void);

	class UParticleModuleTypeDataApex &ApexTypeData;

	class FApexEmitter                *ApexEmitter;

	class FRBPhysScene                *CascadeScene;

};




#endif // #if WITH_APEX_PARTICLES

#endif
