/*=============================================================================
	UnParticleHelper.h: Particle helper definitions/ macros.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel.
=============================================================================*/

#ifndef HEADER_UNPARTICLEHELPER
#define HEADER_UNPARTICLEHELPER

//	Macro fun.
#define DECLARE_PARTICLE(Name,Address)		\
	FBaseParticle& ##Name = *((FBaseParticle*) (Address));

#define DECLARE_PARTICLE_PTR(Name,Address)		\
	FBaseParticle* ##Name = (FBaseParticle*) (Address);

//	Forward declarations.
class UParticleEmitter;
class UParticleSpriteEmitter;
class UParticleMeshEmitter;
class UParticleSpriteSubUVEmitter;
class UParticleModule;
// Data types
class UParticleModuleTypeDataSubUV;
class UParticleModuleTypeDataMesh;
class UParticleModuleTypeDataTrail;

class UStaticMeshComponent;

struct FParticleSpriteEmitterInstance;
struct FParticleSpriteSubUVEmitterInstance;
struct FParticleTrailEmitterInstance;

//	FBaseParticle.
//
//	Make sure to also update UnParticleComponent::Created when updating the below struct as
//	it assumes FBaseParticle being 96 bytes.	
struct FBaseParticle
{
	// 16 bytes
	FVector			OldLocation;			// Last frame's location, used for collision
	FLOAT			RelativeTime;			// Relative time, range is 0 (==spawn) to 1 (==death)

	// 16 bytes
	FVector			Location;				// Current location
	FLOAT			OneOverMaxLifetime;		// Reciprocal of lifetime

	// 16 bytes
	FVector			BaseVelocity;			// Velocity = BaseVelocity at the start of each frame.
	FLOAT			Rotation;				// Rotation of particle (in Radians)

	// 16 bytes
	FVector			Velocity;				// Current velocity, gets reset to BaseVelocity each frame to allow 
	FLOAT			BaseRotationRate;		// Initial angular velocity of particle (in Radians per second)

	// 16 bytes
	FVector			BaseSize;				// Size = BaseSize at the start of each frame
	FLOAT			RotationRate;			// Current rotation rate, gets reset to BaseRotationRate each frame

	// 16 bytes
	FVector			Size;					// Current size, gets reset to BaseSize each frame
	FColor			Color;					// Current color of particle.
};

/***
{
	// 16
	FVector			Location;
	FLOAT			RelativeTime;
	// 16
	FVector			OldLocation;
	FLOAT			OneOverMaxLifetime;
	// 16
	FVector			Size;
	FColor			Color;
	// 16
	FVector			SizeBase;
	FColor			ColorBase;
	// 
//	FVector			Rotation;
//	FVector			RotationBase;
};
***/

//	FParticleSpriteVertex
struct FParticleSpriteVertex
{
	FVector			Position,
					OldPosition,
					Size;
	FLOAT			U,
					V,
					Rotation;
	FColor			Color;
};

//
//	FQuadIndexBuffer.
//
struct FQuadIndexBuffer : public FIndexBuffer
{
	INT NumQuads;

	FQuadIndexBuffer();
	void SetSize( INT InNumQuads );
	void GetData( void* Buffer );
};

//
//	FParticleSpriteVertexBuffer.
//
struct FParticleSpriteVertexBuffer : public FVertexBuffer
{
	FParticleSpriteEmitterInstance* Owner;

	FParticleSpriteVertexBuffer( FParticleSpriteEmitterInstance* InOwner );
	void SetSize( INT InNumQuads );
	void GetData( void* Buffer );
};

//	FParticleSpriteSubVertex
struct FParticleSpriteSubUVVertex : public FParticleSpriteVertex
{
	FLOAT			U2,
					V2;
	FLOAT			Interp;
	FLOAT			Padding;
	FLOAT			SizeU,
					SizeV;
};

//
//	FParticleSpriteSubUVVertexBuffer.
//
struct FParticleSpriteSubUVVertexBuffer : public FVertexBuffer
{
	FParticleSpriteSubUVEmitterInstance* Owner;

	FParticleSpriteSubUVVertexBuffer(FParticleSpriteSubUVEmitterInstance* InOwner);
	void SetSize(INT InNumQuads);
	virtual void GetData(void* Buffer);
};

//
//	FParticleTrailVertexBuffer.
//
struct FParticleTrailVertexBuffer : public FVertexBuffer
{
	FParticleTrailEmitterInstance* Owner;

	FParticleTrailVertexBuffer(FParticleTrailEmitterInstance* InOwner);
	void SetSize(INT InNumQuads);
	virtual void GetData(void* Buffer);
};

//
//  Trail emitter flags and macros
//
#define TRAIL_EMITTER_FLAG_MIDDLE       0x00000000
#define TRAIL_EMITTER_FLAG_START        0x40000000
#define TRAIL_EMITTER_FLAG_END          0x80000000
#define TRAIL_EMITTER_FLAG_MASK         0xC0000000
#define TRAIL_EMITTER_PREV_MASK         0x3fff8000
#define TRAIL_EMITTER_PREV_SHIFT        15
#define TRAIL_EMITTER_NEXT_MASK         0x00007fff
#define TRAIL_EMITTER_NEXT_SHIFT        0

#define TRAIL_EMITTER_IS_START(index)       ((index & TRAIL_EMITTER_FLAG_MASK) == TRAIL_EMITTER_FLAG_START)
#define TRAIL_EMITTER_IS_END(index)         ((index & TRAIL_EMITTER_FLAG_MASK) == TRAIL_EMITTER_FLAG_END)
#define TRAIL_EMITTER_GET_PREV(index)       ((index & TRAIL_EMITTER_PREV_MASK) >> TRAIL_EMITTER_PREV_SHIFT)
#define TRAIL_EMITTER_SET_PREV(index, prev) (index | ((prev << TRAIL_EMITTER_PREV_SHIFT) & TRAIL_EMITTER_PREV_MASK))
#define TRAIL_EMITTER_GET_NEXT(index)       ((index & TRAIL_EMITTER_NEXT_MASK) >> TRAIL_EMITTER_NEXT_SHIFT)
#define TRAIL_EMITTER_SET_NEXT(index, next) (index | ((next << TRAIL_EMITTER_NEXT_SHIFT) & TRAIL_EMITTER_NEXT_MASK))

#endif
