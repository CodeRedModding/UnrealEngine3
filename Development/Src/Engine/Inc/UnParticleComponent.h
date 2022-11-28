/*=============================================================================
	UnParticleSystem.h: Particle system structure definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_UNPARTICLECOMPONENT
#define HEADER_UNPARTICLECOMPONENT

#include "UnParticleHelper.h"

//	UParticleSystemComponent					Particle system component being part of an actor.
//
//		UParticleSystem							Particle system template (collection of subemitters).
//			TArray<UParticleEmitter>			Sub- Emitter template (e.g. sprite, trail, ...)
//				TArray<UParticleModule>			Module affecting each particle (e.g. velocity, spawn location, ...)
//
//		TArray<FParticleEmitterInstance>		Instanced emitters created based on UParticleSystem/UParticleEmitter templates, providing data UParticleModules operate on directly
//
//	UParticleSystemComponent is part of an actor, UParticleSystem, UParticleEmitter and UParticleModule are stored in packages like e.g. materials and FParticleEmitterInstance is instanced data never stored.

//
//	FParticleEmitterInstance.
//
struct FParticleEmitterInstance
{
	UParticleEmitter*					Template;						// Template to base this instance on.
	class UParticleSystemComponent*		Component;						// Component owning this instance.

	BYTE*								ParticleData;					// Particle data, guaranteed to be 16 byte aligned.
	_WORD*								ParticleIndices;				// Indices, 0..ActiveParticles-1 are active particles ActiveParticles..MaxActiveParticles-1 are inactive ones.

	TMap<UParticleModule*,UINT>			ModuleOffsetMap;				// Associates module with offset into particle structure where module specific data is stored.

	UINT								PayloadOffset,					// Offset into emitter specific data (e.g. trail emitters require extra storage for trail index)
										ParticleSize,					// Size of particle structure used for this emitter.
										ParticleStride,					// Particle stride, usually != ParticleSize due to alignment and e.g. trail emitters store trailing emitters right after leading one.
										TrianglesToRender,				// Number of triangles to render.
										MaxVertexIndex,					// Max vertex index.
										ActiveParticles,				// ParticleIndices[ActiveParticles] is first INactive particle, 0..ActiveParticles-1 == active particles, and ActiveParticles == number of active particles.
										MaxActiveParticles;				// Maximum amount of active particles for which memory is allocated - code will resize accordingly to handle growing.

	FLOAT								SpawnFraction,					// Fractional part of particles that need to be spawned carried over from last tick.
										SecondsSinceCreation,			// Seconds since creation of instance.
										EmitterTime;					// Time within emitter loop. Will repeat Template->EmitterLoops times etc.

	FVector								Location,						// Instance location.
										OldLocation;					// Old location (used for interpolation).
										
	FBox								ParticleBoundingBox;			// Bounding box for this emitter.

	FParticleEmitterInstance()
	:	Template(NULL),
		Component(NULL)
	{
	}

	FParticleEmitterInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
	:	Template( InTemplate ),
		Component( InComponent )
	{}

	virtual ~FParticleEmitterInstance();

	virtual void Serialize(FArchive& Ar)	{}

	virtual void Init();
	virtual void Resize( UINT NewMaxActiveParticles );
	virtual void Tick( FLOAT DeltaTime, UBOOL bSuppressSpawning );
	void Rewind();
	virtual void Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI) = 0;
	FBox GetBoundingBox() { return ParticleBoundingBox; }
	virtual void UpdateBoundingBox(FLOAT DeltaTime);

	virtual UINT RequiredBytes();
	virtual UINT CalculateParticleStride( UINT ParticleSize );

	FLOAT Spawn( FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime );
	void PreSpawn( FBaseParticle* Particle );

	UBOOL HasCompleted();

	virtual void PostSpawn( FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime );
	virtual void KillParticles();
	virtual void RemovedFromScene()	{}

	void RenderDebug(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI, UBOOL bCrosses);
};

//
typedef FParticleEmitterInstance* FParticleEmitterInstancePointer;

//
//	FParticleSpriteEmitterInstance.
//
struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
	UParticleSpriteEmitter*				SpriteTemplate;					// Sprite emitter template.

	FParticleSpriteVertexBuffer			VertexBuffer;					// Vertex buffer.
	FQuadIndexBuffer					IndexBuffer;					// Index buffer.
	FParticleVertexFactory				VertexFactory;					// Sprite specific vertex factory for screen orientation.

	FParticleSpriteEmitterInstance(UParticleSpriteEmitter* InTemplate, UParticleSystemComponent* InComponent);

	virtual void Init();
	virtual void Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};

//
//	FParticleSpriteSubUVEmitterInstance
//
struct FParticleSpriteSubUVEmitterInstance : public FParticleEmitterInstance
{
	UParticleSpriteEmitter*				SpriteTemplate;					// Sprite emitter template.
	UParticleModuleTypeDataSubUV*		SubUVTypeData;

	FParticleSpriteSubUVVertexBuffer	VertexBuffer;		// Vertex buffer.
	FQuadIndexBuffer					IndexBuffer;		// Index buffer.
	FParticleSubUVVertexFactory			VertexFactory;		// Sprite specific vertex factory for screen orientation.

	FParticleSpriteSubUVEmitterInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);

	virtual void Init();
	virtual void Resize( UINT NewMaxActiveParticles );
	virtual void Tick( FLOAT DeltaTime, UBOOL bSuppressSpawning );
	virtual void Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);

	virtual UINT RequiredBytes();
	virtual UINT CalculateParticleStride( UINT ParticleSize );
	virtual void KillParticles();
};

struct FParticleSubUVDummyInstance : public FParticleEmitterInstance
{
	FParticleSubUVDummyInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) :
		FParticleEmitterInstance(0, 0)
		{}
	virtual ~FParticleSubUVDummyInstance()						{}
	virtual void Serialize(FArchive& Ar)						{}
	virtual void Init()											{}
	virtual void Resize(UINT NewMaxActiveParticles)				{}
	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)	{}
	virtual void Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)	{}
	virtual UINT RequiredBytes()								{	return 0;	}
	virtual UINT CalculateParticleStride(UINT ParticleSize)		{	return 0;	}
	virtual void PostSpawn(FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime)	{}
	virtual void KillParticles()								{}
	virtual void RemovedFromScene()								{}
};

//
//	FParticleMeshEmitterInstance.
//
struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
	UParticleSpriteEmitter*			    EmitterTemplate;	// Mesh emitter template.
	UParticleModuleTypeDataMesh*	    MeshTypeData;
	FScene*							    LastScene;
	TArray<UStaticMeshComponent*>	    MeshInstances;
    UBOOL                               MeshRotationActive;

	FParticleMeshEmitterInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
	virtual ~FParticleMeshEmitterInstance();

	virtual void Serialize(FArchive& Ar);

	void Init();
	void Resize( UINT NewMaxActiveParticles );
	void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);
	void Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);

	UINT CalculateParticleStride( UINT ParticleSize );
	void ChildSpawn( FBaseParticle* MainParticle, FBaseParticle* ChildParticle, FLOAT InterpolationPercentage );

	virtual void UpdateBoundingBox(FLOAT DeltaTime);

	UINT RequiredBytes();
	void PostSpawn( FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime );
	virtual void KillParticles();
	virtual void RemovedFromScene();
};

struct FParticleMeshDummyInstance : public FParticleEmitterInstance
{
	FParticleMeshDummyInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) :
		FParticleEmitterInstance(InTemplate, InComponent)
	{
	}

	virtual ~FParticleMeshDummyInstance()						{}
	virtual void Serialize(FArchive& Ar)						{}
	virtual void Init()											{}
	virtual void Resize(UINT NewMaxActiveParticles)				{}
	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)	{}
	virtual void Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)	{}
	virtual UINT RequiredBytes()								{	return 0;	}
	virtual UINT CalculateParticleStride(UINT ParticleSize)		{	return 0;	}
	virtual void PostSpawn(FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime)	{}
	virtual void KillParticles()								{}
	virtual void RemovedFromScene()								{}
};

//
//	FParticleTrailEmitterInstance
//
struct FParticleTrailEmitterInstance : public FParticleEmitterInstance
{
	UParticleSpriteEmitter*				SpriteTemplate;		// Sprite emitter template.
	UParticleModuleTypeDataTrail*		TrailTypeData;

	FParticleTrailVertexBuffer			VertexBuffer;		// Vertex buffer.
	FQuadIndexBuffer					IndexBuffer;		// Index buffer.
	FParticleTrailVertexFactory			VertexFactory;		// specific vertex factory

	FParticleTrailEmitterInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);

	virtual void Init();
	virtual void Resize(UINT NewMaxActiveParticles);
	virtual void Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning);
	virtual void Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI);

	virtual UINT RequiredBytes();
	virtual UINT CalculateParticleStride(UINT ParticleSize);
	virtual void KillParticles();
};

#endif
