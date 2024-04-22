/*=============================================================================
	PhysXParticleSetSprite.cpp: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#include "UnNovodexSupport.h"
#include "PhysXVerticalEmitter.h"
#include "PhysXParticleSystem.h"
#include "PhysXParticleSetSprite.h"

FPhysXParticleSetSprite::FPhysXParticleSetSprite(struct FParticleSpritePhysXEmitterInstance& InEmitterInstance):
	FPhysXParticleSet(sizeof(PhysXRenderParticleSprite), InEmitterInstance.PhysXTypeData.VerticalLod, InEmitterInstance.PhysXTypeData.PhysXParSys),
	PhysXTypeData(InEmitterInstance.PhysXTypeData),
	EmitterInstance(InEmitterInstance)
{
	check(InEmitterInstance.PhysXTypeData.PhysXParSys);
}

FPhysXParticleSetSprite::~FPhysXParticleSetSprite()
{
}

void FPhysXParticleSetSprite::AsyncUpdate(FLOAT DeltaTime, UBOOL bProcessSimulationStep)
{
	FPhysXParticleSystem& PSys = GetPSys();

	UpdateParticles(DeltaTime);
	if(bProcessSimulationStep)
	{
		DeathRowManagment();
		AsyncParticleReduction(DeltaTime);
	}
}

void FPhysXParticleSetSprite::RemoveAllParticles()
{
	RemoveAllParticlesInternal();
	check(EmitterInstance.ActiveParticles == EmitterInstance.NumSpawnedParticles);
	EmitterInstance.RemoveParticles();
}

INT FPhysXParticleSetSprite::RemoveParticle(INT RenderParticleIndex, bool bRemoveFromPSys)
{
	if(EmitterInstance.ActiveParticles > 0) //In the other case, the emitter instance has been cleared befor.
	{
		EmitterInstance.RemoveParticleFromActives(RenderParticleIndex);
	}
	
	return RemoveParticleFast(RenderParticleIndex, bRemoveFromPSys);
}

void FPhysXParticleSetSprite::UpdateParticles(FLOAT DeltaTime)
{
	FPhysXParticleSystem& PSys = GetPSys();
	check(EmitterInstance.ActiveParticles == GetNumRenderParticles() + EmitterInstance.NumSpawnedParticles);
	TmpRenderIndices.Empty();

	if(GetNumRenderParticles() == 0)
		return;

	PhysXParticle* SdkParticles = PSys.ParticlesSdk;
	PhysXParticleEx* SdkParticlesEx = PSys.ParticlesEx;
	
	FLOAT Temp = NxMath::clamp(PhysXTypeData.VerticalLod.RelativeFadeoutTime, 1.0f, 0.0f);
	FLOAT TimeUntilFadeout = 1.0f - Temp;
	for (INT i=0; i<GetNumRenderParticles(); i++)
	{
		PhysXRenderParticleSprite& NParticle = *(PhysXRenderParticleSprite*)GetRenderParticle(i);
		PhysXParticle& SdkParticle = SdkParticles[NParticle.ParticleIndex];
		// ensure that particle is in sync
		check(NParticle.Id == SdkParticle.Id);
		NParticle.RelativeTime += NParticle.OneOverMaxLifetime * DeltaTime;
		if(NParticle.RelativeTime >= TimeUntilFadeout && (NParticle.Flags & PhysXRenderParticle::PXRP_DeathQueue) == 0)
			TmpRenderIndices.Push(i);

		DECLARE_PARTICLE(Particle, EmitterInstance.ParticleData + EmitterInstance.ParticleStride*EmitterInstance.ParticleIndices[i]);
		
		Particle.Location = N2UPosition(SdkParticle.Pos);
		Particle.BaseVelocity = Particle.Velocity = N2UPosition(SdkParticle.Vel);

		// Calculate OldLocation based on Velocity and Location
		// (The shader uses the reverse calculation to determine direction of velocity).
		Particle.OldLocation = Particle.Location - Particle.Velocity * DeltaTime;

		FLOAT* Density = (FLOAT*)(((BYTE*)&Particle) + EmitterInstance.DensityPayloadOffset);
		*Density = SdkParticle.Density;
	}
	
}

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
