/*=============================================================================
    PhysXVerticalEmitter.h: Emitter Vertical Component.
    Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#ifndef __PHYSXVERTICALEMITTER_H__
#define __PHYSXVERTICALEMITTER_H__

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

/**
Emitter Vertical LOD manager class. In the current implementation it 
is both, provider for LOD budgets and PhysXParticleSystem manager at 
the same time. It picks up LOD settings from the vertical framework and 
parameters of the particle systems in order to figure out particle budgets.
*/
class FPhysXVerticalEmitter
{
public:

	FPhysXVerticalEmitter(FRBPhysScene& InOwner);
	~FPhysXVerticalEmitter();
	
	void AddParticleSystem(class UPhysXParticleSystem*);
	void RemoveParticleSystem(class UPhysXParticleSystem*);
	void PreSyncPhysXData();
	void PostSyncPhysXData();
	void Tick(FLOAT DeltaTime);
	void TickEditor(FLOAT DeltaTime);

	FLOAT GetEffectiveStepSize();

private:

	FRBPhysScene& OwnerScene;

	UBOOL bParamsDisableLod;
	INT ParamsMaxParticles;
	INT ParamsMinParticles;
	FLOAT ParamsSpawnLodVsFifoBias;

	void TickCommon();
	void AssignReduction(UINT ParticleBudget);
	void AssignPacketReduction();
	void AssignEmissionReduction(UINT ParticleBudget);
	void AddParticleSystemInternal(class UPhysXParticleSystem* Ps);
	void RemoveParticleSystemInternal(class UPhysXParticleSystem* Ps);
	void SetBudgets();
	void SetEffectiveStepSize(FLOAT DeltaTime);

	FORCEINLINE UBOOL NeedsTick()
	{
		if(GFrameCounter == LastFrameTicked)
			return false;

		LastFrameTicked = GFrameCounter;
		return true;
	}

	TArray<class UPhysXParticleSystem*> ParticleSystems;
	TArray<class UPhysXParticleSystem*> UnconnectedParticleSystems; 

	class FPhysXDistribution* Distributer;

	UINT GlobalParticleBudget;
	UINT GlobalPacketPerPsBudget;

	QWORD LastFrameTicked;
	UBOOL WasEditorSceneTicked;

	FLOAT RemainingSimTime;
	FLOAT EffectiveStepSize;
};

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#endif
