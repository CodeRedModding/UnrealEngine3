/*=============================================================================
	PhysXVerticalEmitter.cpp: Emitter Vertical Component.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
#include "UnNovodexSupport.h"
#include "PhysXVerticalEmitter.h"
#include "PhysXParticleSystem.h"
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

IMPLEMENT_CLASS(UPhysicsLODVerticalEmitter);

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

void FPhysXVerticalEmitter::SetBudgets()
{
	AWorldInfo* Info = GWorld->GetWorldInfo();
	check(Info);
	
	bParamsDisableLod = Info->VerticalProperties.Emitters.bDisableLod;
	ParamsMinParticles = NxMath::clamp(Info->VerticalProperties.Emitters.ParticlesLodMin, 20000, 0);
	ParamsMaxParticles = NxMath::clamp(Info->VerticalProperties.Emitters.ParticlesLodMax, 20000, ParamsMinParticles);
	ParamsSpawnLodVsFifoBias = NxMath::clamp(Info->VerticalProperties.Emitters.SpawnLodVsFifoBias, 1.0f, 0.0f);

	GlobalParticleBudget = ParamsMaxParticles;

	//Poll vertical framework parameters
	AWorldInfo * Wi = GWorld->GetWorldInfo();
	if(Wi)
	{
		UPhysicsLODVerticalEmitter * VeEm = Wi->EmitterVertical;
		check(VeEm);
		
		INT Percentage = Clamp( VeEm->ParticlePercentage, 0, 100 );
		GlobalParticleBudget = appFloor(Percentage*0.01f*(ParamsMaxParticles-ParamsMinParticles) + ParamsMinParticles);
	}

	if(bParamsDisableLod)
	{
		GlobalPacketPerPsBudget = 1024;
	}
	else
	{
		GlobalPacketPerPsBudget = NxMath::clamp(Info->VerticalProperties.Emitters.PacketsPerPhysXParticleSystemMax, 900, 1);
	}
}

FLOAT FPhysXVerticalEmitter::GetEffectiveStepSize()
{
	return EffectiveStepSize;
}

void FPhysXVerticalEmitter::SetEffectiveStepSize(FLOAT DeltaTime)
{
	FLOAT MaxTimestep;
	UINT MaxIter;
	NxTimeStepMethod Method;
	NxCompartment* FluidCompartment = OwnerScene.GetNovodexFluidCompartment();
	check(FluidCompartment);
	FluidCompartment->getTiming(MaxTimestep, MaxIter, Method);

	AWorldInfo* Info = (AWorldInfo*)(AWorldInfo::StaticClass()->GetDefaultObject());
	FLOAT MaxDeltaTime = Info->MaxPhysicsDeltaTime;
	DeltaTime = ::Min(DeltaTime, MaxDeltaTime);

	//This should be the same timing the SDK does apply to fluid simulation... 
	//Doesn't support NxCompartmentDesc::timeScale
	if(Method == NX_TIMESTEP_FIXED)
	{
		EffectiveStepSize = 0.0f;
		FLOAT TimeStep = MaxTimestep*MaxIter;
		DeltaTime = ::Min(DeltaTime, TimeStep);
		RemainingSimTime += DeltaTime;
		if(RemainingSimTime >= TimeStep)
		{
			RemainingSimTime -= TimeStep;
			EffectiveStepSize = TimeStep;
		}
	}
	else
	{
		EffectiveStepSize = DeltaTime;
	}
}

FPhysXVerticalEmitter::FPhysXVerticalEmitter(FRBPhysScene& InOwner):
	OwnerScene(InOwner)
{
	RemainingSimTime = 0.0f;
	EffectiveStepSize = 0.0f;
	GlobalParticleBudget = 0;
	GlobalPacketPerPsBudget = 0;

	LastFrameTicked = GFrameCounter - 1;
	WasEditorSceneTicked = FALSE;

	Distributer = new FPhysXDistribution();
}

FPhysXVerticalEmitter::~FPhysXVerticalEmitter()
{
	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		UPhysXParticleSystem& Ps = *ParticleSystems(i);
		Ps.RemovedFromScene();
		Ps.SyncDisconnect();
	}

	//We don't need to deal with UnconnectedParticleSystems

	delete Distributer;
}

void FPhysXVerticalEmitter::AddParticleSystem(class UPhysXParticleSystem* Ps)
{
	if(Ps->PSys)
		return;

	UnconnectedParticleSystems.AddUniqueItem(Ps);
}

void FPhysXVerticalEmitter::RemoveParticleSystem(class UPhysXParticleSystem* Ps)
{
	if(Ps->PSys)
		RemoveParticleSystemInternal(Ps);
	else
		UnconnectedParticleSystems.RemoveItem(Ps);
}
void FPhysXVerticalEmitter::PreSyncPhysXData()
{
	SCOPE_CYCLE_COUNTER(STAT_PhysXEmitterVerticalSync);

	plotf( TEXT("DEBUG_VE_PLOT FRAME"));

	for(INT i=UnconnectedParticleSystems.Num()-1; i>=0; i--)
	{
		UPhysXParticleSystem* Ps = UnconnectedParticleSystems(i);
		if(Ps->SyncConnect())
		{
			AddParticleSystemInternal(Ps);
		}
		UnconnectedParticleSystems.RemoveItem(Ps);
	}

	for(INT i=ParticleSystems.Num()-1; i>=0; i--)
	{
		UPhysXParticleSystem* Ps = ParticleSystems(i);
		
		if(Ps->SyncDisconnect())
		{
			RemoveParticleSystemInternal(Ps);
			UnconnectedParticleSystems.AddUniqueItem(Ps);
		}
	}

	SetBudgets();
	//plotf( TEXT("DEBUG_VE_PLOT GlobalParticleBudget %d"), GlobalParticleBudget);

	AssignEmissionReduction(GlobalParticleBudget);

	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		ParticleSystems(i)->PreSyncPhysXData();
	}
	
	AssignReduction(GlobalParticleBudget);
}
void FPhysXVerticalEmitter::PostSyncPhysXData()
{
	SCOPE_CYCLE_COUNTER(STAT_PhysXEmitterVerticalSync);

	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		ParticleSystems(i)->PostSyncPhysXData();
	}
}
void FPhysXVerticalEmitter::TickCommon()
{
	SetBudgets();
	AssignPacketReduction();
}

void FPhysXVerticalEmitter::Tick(FLOAT DeltaTime)
{
	if(!NeedsTick()) 
		return;

	SCOPE_CYCLE_COUNTER(STAT_PhysXEmitterVerticalTick);

	if(GIsGame)
	{
		TickCommon();
		SetEffectiveStepSize(DeltaTime);
		DeltaTime = GetEffectiveStepSize();
		for(INT i=0; i<ParticleSystems.Num(); i++)
		{
			ParticleSystems(i)->Tick(DeltaTime);
		}
	}
	else
	{
		if(ParticleSystems.Num() == 0 || ParticleSystems.Num() > 1)
		{
			return;
		}

		check(ParticleSystems(0));
		UPhysXParticleSystem& Ps = *ParticleSystems(0);
		
		//Simulate Scene (Necessary to create the PSys synchronous
		if(WasEditorSceneTicked)
		{
			WaitRBPhysScene(Ps.CascadeScene);
			WasEditorSceneTicked = FALSE;
		}

		TickRBPhysScene(Ps.CascadeScene, DeltaTime);
		WasEditorSceneTicked = TRUE;

		TickCommon();
		SetEffectiveStepSize(DeltaTime);
		DeltaTime = GetEffectiveStepSize();
		Ps.Tick(DeltaTime);
	}

}

void FPhysXVerticalEmitter::AssignReduction(UINT ParticleBudget)
{
	if((GIsEditor == TRUE && GIsGame == FALSE) || bParamsDisableLod) //cascade or disabled
	{
		for(INT i=0; i<ParticleSystems.Num(); i++)
		{
			ParticleSystems(i)->PSys->SyncParticleReduction(0);
		}
		return;
	}

	UINT ParticleSum = 0;
	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		FPhysXParticleSystem& Pi = *ParticleSystems(i)->PSys;
		ParticleSum += Pi.GetNumParticles();
	}	

	if(ParticleSum <= ParticleBudget)
	{
		for(INT i=0; i<ParticleSystems.Num(); i++)
		{
			ParticleSystems(i)->PSys->SyncParticleReduction(0);
		}
		return;
	}

	UINT GlobalParticleReduction = ParticleSum - ParticleBudget;

	TArray<FPhysXDistribution::Input>& InputBuffer = Distributer->GetInputBuffer(ParticleSystems.Num());
	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		FPhysXParticleSystem& Pi = *ParticleSystems(i)->PSys;
		InputBuffer.AddItem(FPhysXDistribution::Input(Pi.GetNumParticles(), Pi.GetWeightForFifo()));
	}
	
	const TArray<FPhysXDistribution::Result>& ResultBuffer = Distributer->GetResult(GlobalParticleReduction);

	for(INT i=0; i<ResultBuffer.Num(); i++)
	{
		FPhysXParticleSystem& Pi = *ParticleSystems(i)->PSys;
		const FPhysXDistribution::Result& R = ResultBuffer(i);
		Pi.SyncParticleReduction(R.Piece);
	}
}

void FPhysXVerticalEmitter::AssignEmissionReduction(UINT ParticleBudget)
{
	
	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		FPhysXParticleSystem& Pi = *ParticleSystems(i)->PSys;
		Pi.SetEmissionBudget(INT_MAX);
	}
	
	if((GIsEditor == TRUE && GIsGame == FALSE) || bParamsDisableLod) //cascade or disabled
		return;

	//Compute budget for each ps
	UINT SpawnVolumeEstimateSum = 0;
	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		FPhysXParticleSystem& Pi = *ParticleSystems(i)->PSys;
		SpawnVolumeEstimateSum += Pi.GetSpawnVolumeEstimate();
	}	

	//In this case, all emitters are free to emmit as much as they want.
	if(SpawnVolumeEstimateSum <= ParticleBudget)
		return;

	//This allows for blending out the spawn culling.
	FLOAT Temp = ParamsSpawnLodVsFifoBias*ParticleBudget + (1.0f-ParamsSpawnLodVsFifoBias)*SpawnVolumeEstimateSum;
	UINT ParticleBudgetSpawning = NxMath::clamp((UINT)Temp, SpawnVolumeEstimateSum, ParticleBudget);


	TArray<FPhysXDistribution::Input>& InputBuffer = Distributer->GetInputBuffer(ParticleSystems.Num());
	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		FPhysXParticleSystem& Pi = *ParticleSystems(i)->PSys;
		InputBuffer.AddItem(FPhysXDistribution::Input(Pi.GetSpawnVolumeEstimate(), Pi.GetWeightForSpawnLod()));
	}
	
	const TArray<FPhysXDistribution::Result>& ResultBuffer = Distributer->GetResult(ParticleBudgetSpawning);

	for(INT i=0; i<ResultBuffer.Num(); i++)
	{
		FPhysXParticleSystem& Pi = *ParticleSystems(i)->PSys;
		const FPhysXDistribution::Result& R = ResultBuffer(i);
		Pi.SetEmissionBudget(R.Piece);
	}
}

void FPhysXVerticalEmitter::AssignPacketReduction()
{
	for(INT i=0; i<ParticleSystems.Num(); i++)
	{
		FPhysXParticleSystem& Pi = *ParticleSystems(i)->PSys;
		Pi.VerticalPacketLimit = GlobalPacketPerPsBudget;
	}
}

void FPhysXVerticalEmitter::AddParticleSystemInternal(class UPhysXParticleSystem* Ps)
{
	ParticleSystems.AddUniqueItem(Ps);
}

void FPhysXVerticalEmitter::RemoveParticleSystemInternal(class UPhysXParticleSystem* Ps)
{
	ParticleSystems.RemoveItem(Ps);
}

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
