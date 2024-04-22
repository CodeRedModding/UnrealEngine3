/*=============================================================================
	PhysXParticleSetSprite.h: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#ifndef __PHYSXPARTICLESETSPRITE_H__
#define __PHYSXPARTICLESETSPRITE_H__

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#include "PhysXParticleSet.h"

struct PhysXRenderParticleSprite: public PhysXRenderParticle
{	
	//No specific fields here, yet.
};

/**
Represents one FPhysXParticleSet connected to one FParticleSpritePhysXEmitterInstance.
As opposed to the mesh set, the sprite set corresponds to particles belonging to just one
sprite emitter instance. In this case, the rendering is taken care of the emitter instance 
itself. The FPhysXParticleSetSprite just provides the physical motion update to the sprite
emitter instance.
*/
class FPhysXParticleSetSprite: public FPhysXParticleSet
{
public:

	FPhysXParticleSetSprite(struct FParticleSpritePhysXEmitterInstance& InEmitterInstance);
	virtual ~FPhysXParticleSetSprite();

	virtual void AsyncUpdate(FLOAT DeltaTime, UBOOL bProcessSimulationStep);
	virtual void RemoveAllParticles();

	virtual INT RemoveParticle(INT RenderParticleIndex, bool bRemoveFromPSys);

	UParticleModuleTypeDataPhysX&				PhysXTypeData;
	FParticleSpritePhysXEmitterInstance&		EmitterInstance;		

	FORCEINLINE void AddParticle(const PhysXRenderParticleSprite& RenderParticle, const UParticleSystemComponent* const OwnerComponent)
	{
		const PhysXRenderParticle* Particle = (const PhysXRenderParticle*)&RenderParticle;
		FPhysXParticleSet::AddParticle(Particle, OwnerComponent);
	}



private:

	void UpdateParticles(FLOAT DeltaTime);

};

#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#endif
