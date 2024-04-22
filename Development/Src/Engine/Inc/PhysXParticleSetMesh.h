/*=============================================================================
	PhysXParticleSetMesh.h: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#ifndef __PHYSXPARTICLESETMESH_H__
#define __PHYSXPARTICLESETMESH_H__

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#include "PhysXParticleSet.h"

/**
Extends PhysXRenderParticle with information to render the mesh.
*/
struct PhysXRenderParticleMesh: public PhysXRenderParticle
{	
	FQuat Rot;
	FVector AngVel;
	FVector Size;
	FLinearColor Color;
};

/**
Represents one FPhysXParticleSet connected to one FParticleMeshPhysXEmitterInstance.
The mesh set corresponds to particles belonging to just one
mesh emitter instance. In this case, the rendering is taken care of the emitter instance 
itself. The FPhysXParticleSetMesh just provides the physical motion update to the mesh
emitter instance.
*/

class FPhysXParticleSetMesh: public FPhysXParticleSet
{
public:

	FPhysXParticleSetMesh(struct FParticleMeshPhysXEmitterInstance& InEmitterInstance);
	virtual ~FPhysXParticleSetMesh();

	virtual void AsyncUpdate(FLOAT DeltaTime, UBOOL bProcessSimulationStep);
	virtual void RemoveAllParticles();

	virtual INT RemoveParticle(INT RenderParticleIndex, bool bRemoveFromPSys);

	UParticleModuleTypeDataMeshPhysX&		PhysXTypeData;
	FParticleMeshPhysXEmitterInstance&		EmitterInstance;		

	FORCEINLINE void AddParticle(const PhysXRenderParticleMesh& RenderParticle, const UParticleSystemComponent* const OwnerComponent)
	{
		const PhysXRenderParticle* Particle = (const PhysXRenderParticle*)&RenderParticle;
		FPhysXParticleSet::AddParticle(Particle, OwnerComponent);
	}

private:

	void FillInVertexBuffer(FLOAT DeltaTime, BYTE FluidRotationMethod, FLOAT FluidRotationCoefficient);
};
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#endif
