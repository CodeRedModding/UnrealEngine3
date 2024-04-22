/*=============================================================================
	UnParticleHelper.h: Particle helper definitions/ macros.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef HEADER_UNPARTICLEHELPER
#define HEADER_UNPARTICLEHELPER

#define _ENABLE_PARTICLE_LOD_INGAME_

#include "PrimitiveSceneProxyOcclusionTracker.h"

#define ALLOW_INDEXED_PARTICLE_SPRITES			1
#if XBOX && ALLOW_INDEXED_PARTICLE_SPRITES		
	#define PARTICLES_USE_INDEXED_SPRITES		1
#else
	#define PARTICLES_USE_INDEXED_SPRITES		0
#endif

/*-----------------------------------------------------------------------------
	Helper macros.
-----------------------------------------------------------------------------*/
//	Macro fun.
#define _PARTICLES_USE_PREFETCH_
#if defined(_PARTICLES_USE_PREFETCH_)
	#define	PARTICLE_PREFETCH(Index)					PREFETCH( ParticleData + ParticleStride * ParticleIndices[Index] )
	#define PARTICLE_INSTANCE_PREFETCH(Instance, Index)	PREFETCH( Instance->ParticleData + Instance->ParticleStride * Instance->ParticleIndices[Index] )
	#define	PARTICLE_OWNER_PREFETCH(Index)				PREFETCH( Owner->ParticleData + Owner->ParticleStride * Owner->ParticleIndices[Index] )
#else	//#if defined(_PARTICLES_USE_PREFETCH_)
	#define	PARTICLE_PREFETCH(Index)					
	#define	PARTICLE_INSTANCE_PREFETCH(Instance, Index)	
	#define	PARTICLE_OWNER_PREFETCH(Index)				
#endif	//#if defined(_PARTICLES_USE_PREFETCH_)

#define DECLARE_PARTICLE(Name,Address)		\
	FBaseParticle& Name = *((FBaseParticle*) (Address));

#define DECLARE_PARTICLE_PTR(Name,Address)		\
	FBaseParticle* Name = (FBaseParticle*) (Address);

#define BEGIN_UPDATE_LOOP																								\
	{																													\
		INT&			ActiveParticles = Owner->ActiveParticles;														\
		UINT			CurrentOffset	= Offset;																		\
		const BYTE*		ParticleData	= Owner->ParticleData;															\
		const UINT		ParticleStride	= Owner->ParticleStride;														\
		WORD*			ParticleIndices	= Owner->ParticleIndices;														\
		for(INT i=ActiveParticles-1; i>=0; i--)																			\
		{																												\
			const INT	CurrentIndex	= ParticleIndices[i];															\
			const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;									\
			FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);												\
			if ((Particle.Flags & STATE_Particle_Freeze) == 0)															\
			{																											\

#define END_UPDATE_LOOP																									\
			}																											\
			CurrentOffset				= Offset;																		\
		}																												\
	}

#define CONTINUE_UPDATE_LOOP																							\
		CurrentOffset = Offset;																							\
		continue;

#define SPAWN_INIT																										\
	const INT		ActiveParticles	= Owner->ActiveParticles;															\
	const UINT		ParticleStride	= Owner->ParticleStride;															\
	const BYTE*		ParticleBase	= Owner->ParticleData + Owner->ParticleIndices[ActiveParticles] * ParticleStride;	\
	UINT			CurrentOffset	= Offset;																			\
	FBaseParticle&	Particle		= *((FBaseParticle*) ParticleBase);

#define PARTICLE_ELEMENT(Type,Name)																						\
	Type& Name = *((Type*)(ParticleBase + CurrentOffset));																\
	CurrentOffset += sizeof(Type);

#define KILL_CURRENT_PARTICLE																							\
	{																													\
		ParticleIndices[i]					= ParticleIndices[ActiveParticles-1];										\
		ParticleIndices[ActiveParticles-1]	= CurrentIndex;																\
		ActiveParticles--;																								\
	}

/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/

inline void Particle_SetColorFromVector(const FVector& InColorVec, const FLOAT InAlpha, FLinearColor& OutColor)
{
	OutColor.R = InColorVec.X;
	OutColor.G = InColorVec.Y;
	OutColor.B = InColorVec.Z;
	OutColor.A = InAlpha;
}

/*-----------------------------------------------------------------------------
	Forward declarations
-----------------------------------------------------------------------------*/
//	Emitter and module types
class UParticleEmitter;
class UParticleSpriteEmitter;
class UParticleModule;
// Data types
class UParticleModuleTypeDataMesh;
class UParticleModuleTypeDataTrail;
class UParticleModuleTypeDataBeam;
class UParticleModuleTypeDataBeam2;
class UParticleModuleTypeDataTrail2;

class UStaticMeshComponent;

class UParticleSystem;
class UParticleSystemComponent;

class UParticleModuleBeamSource;
class UParticleModuleBeamTarget;
class UParticleModuleBeamNoise;
class UParticleModuleBeamModifier;

class UParticleModuleTrailSource;
class UParticleModuleTrailSpawn;
class UParticleModuleTrailTaper;

class UParticleModuleOrientationAxisLock;

class UParticleLODLevel;

class FParticleSystemSceneProxy;
class FParticleDynamicData;
struct FDynamicBeam2EmitterData;
struct FDynamicTrail2EmitterData;

struct FParticleSpriteEmitterInstance;
struct FParticleSpriteSubUVEmitterInstance;
struct FParticleMeshEmitterInstance;
struct FParticleTrailEmitterInstance;
struct FParticleBeamEmitterInstance;
struct FParticleBeam2EmitterInstance;
struct FParticleTrail2EmitterInstance;

struct FParticleEmitterInstance;

// Special module indices...
#define INDEX_TYPEDATAMODULE	(INDEX_NONE - 1)
#define INDEX_REQUIREDMODULE	(INDEX_NONE - 2)
#define INDEX_SPAWNMODULE		(INDEX_NONE - 3)

/*-----------------------------------------------------------------------------
	FBaseParticle
-----------------------------------------------------------------------------*/
// Mappings for 'standard' particle data
// Only used when required.
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
	INT				Flags;					// Flags indicating various particle states

	// 16 bytes
	FLinearColor	Color;					// Current color of particle.

	// 16 bytes
	FLinearColor	BaseColor;				// Base color of the particle
};

/*-----------------------------------------------------------------------------
	Particle State Flags
-----------------------------------------------------------------------------*/
enum EParticleStates
{
	/** Ignore updates to the particle						*/
	STATE_Particle_Freeze				 = 0x00000001,
	/** Ignore collision updates to the particle			*/
	STATE_Particle_IgnoreCollisions		 = 0x00000002,
	/**	Stop translations of the particle					*/
	STATE_Particle_FreezeTranslation	 = 0x00000004,
	/**	Stop rotations of the particle						*/
	STATE_Particle_FreezeRotation		 = 0x00000008,
	/** Combination for a single check of 'ignore' flags	*/
	STATE_Particle_CollisionIgnoreCheck	 = STATE_Particle_Freeze |STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation| STATE_Particle_FreezeRotation,
	/** Delay collision updates to the particle				*/
	STATE_Particle_DelayCollisions		 = 0x00000010,
	/** Flag indicating the particle has had at least one collision	*/
	STATE_Particle_CollisionHasOccurred	 = 0x00000020,
	/** Set when a particle first collides with an attractor */
	STATE_Particle_CollidedWithAttractor = 0x00000040,
};

/*-----------------------------------------------------------------------------
	FParticlesStatGroup
-----------------------------------------------------------------------------*/
enum EParticleStats
{
	STAT_SpriteParticles = STAT_ParticlesFirstStat,
	STAT_SpriteParticlesSpawned,
	STAT_SpriteParticlesUpdated,
	STAT_SpriteParticlesKilled,
	STAT_ParticleDrawCalls,
	STAT_SortingTime,
	STAT_SpriteRenderingTime,
	STAT_SpriteResourceUpdateTime,
	STAT_SpriteTickTime,
	STAT_PSysCompTickTime,
	STAT_ParticleCollisionTime,
	STAT_ParticleSkelMeshSurfTime,
	STAT_ParticleStaticMeshSurfTime,
	STAT_ParticlePoolTime,
	STAT_ParticleTickTime,
	STAT_ParticleSpawnTime,
	STAT_ParticleUpdateTime,
	STAT_ParticleRenderingTime,
	STAT_ParticlePackingTime,
	STAT_ParticleSetTemplateTime,
	STAT_ParticleInitializeTime,
	STAT_ParticleActivateTime,
	STAT_ParticleUpdateInstancesTime,
	STAT_ParticleAsyncTime,
	STAT_ParticleAsyncWaitTime,
	STAT_ParticleUpdateBounds,
#if STATS
	STAT_ParticleMemTime,
	STAT_GTParticleData,
	STAT_GTParticleData_MAX,
	STAT_RTParticleData,
	STAT_RTParticleData_MAX,
	STAT_RTParticleData_Largest,
	STAT_RTParticleData_Largest_MAX,
	STAT_DynamicPSysCompMem,
	STAT_DynamicPSysCompMem_MAX,
	STAT_DynamicEmitterMem,
	STAT_DynamicEmitterMem_MAX,
	STAT_DynamicEmitterGTMem_Waste,
	STAT_DynamicEmitterGTMem_Largest,
	STAT_DynamicEmitterGTMem_Waste_MAX,
	STAT_DynamicEmitterGTMem_Largest_MAX,
	STAT_DynamicPSysCompCount,
	STAT_DynamicPSysCompCount_MAX,
	STAT_DynamicEmitterCount,
	STAT_DynamicEmitterCount_MAX,
	STAT_DynamicSpriteCount,
	STAT_DynamicSpriteCount_MAX,
	STAT_DynamicSpriteGTMem,
	STAT_DynamicSpriteGTMem_MAX,
	STAT_DynamicSubUVCount,
	STAT_DynamicSubUVCount_MAX,
	STAT_DynamicSubUVGTMem,
	STAT_DynamicSubUVGTMem_Max,
	STAT_DynamicMeshCount,
	STAT_DynamicMeshCount_MAX,
	STAT_DynamicMeshGTMem,
	STAT_DynamicMeshGTMem_MAX,
	STAT_DynamicBeamCount,
	STAT_DynamicBeamCount_MAX,
	STAT_DynamicBeamGTMem,
	STAT_DynamicBeamGTMem_MAX,
	STAT_DynamicTrailCount,
	STAT_DynamicTrailCount_MAX,
	STAT_DynamicTrailGTMem,
	STAT_DynamicTrailGTMem_MAX,
	STAT_DynamicRibbonCount,
	STAT_DynamicRibbonCount_MAX,
	STAT_DynamicRibbonGTMem,
	STAT_DynamicRibbonGTMem_MAX,
	STAT_DynamicAnimTrailCount,
	STAT_DynamicAnimTrailCount_MAX,
	STAT_DynamicAnimTrailGTMem,
	STAT_DynamicAnimTrailGTMem_MAX,
	STAT_DynamicUntrackedGTMem,
	STAT_DynamicUntrackedGTMem_MAX,
#endif
};

enum EParticleMeshStats
{
	STAT_MeshParticles = STAT_MeshParticlesFirstStat,
	STAT_MeshRenderingTime,
	STAT_MeshTickTime,
};

//	FParticleSpriteVertex
struct FParticleSpriteVertex
{
	/** The position of the particle					*/
	FVector			Position;
	/** The previous position of the particle			*/
	FVector			OldPosition;
	/** The size of the particle						*/
	FVector			Size;
	/** The rotation of the particle					*/
	FLOAT			Rotation;
	FLOAT			SizerIndex;
	/** The color of the particle						*/
	FLinearColor	Color;
#if !PARTICLES_USE_INDEXED_SPRITES
	/** The UV values of the particle					*/
	FLOAT			Tex_U;
	FLOAT			Tex_V;
#endif	//#if !PARTICLES_USE_INDEXED_SPRITES
};

//	FParticleSpriteVertexDynamicParameter
struct FParticleSpriteVertexDynamicParameter : public FParticleSpriteVertex
{
	/** The dynamic parameter of the particle			*/
	FLOAT			DynamicValue[4];
};

//	FParticleSpriteSubUVVertex
struct FParticleSpriteSubUVVertex : public FParticleSpriteVertex
{
#if PARTICLES_USE_INDEXED_SPRITES
	// For alignment reasons, these can't be FLinearColor/FVector4
	/** 
	 *	The UV offsets
	 *	xy = the first sub-image
	 *	zw = the second sub-image
	 */
	FLOAT			Offsets[4];
	/**
	 *	x = the interpolation value
	 *	y = padding
	 *	z = size of sub-image in U
	 *	w = size of sub-image in V
	 */
	FLOAT			Interp_Sizer[4];
#else
	/** The second UV set for the particle				*/
	FLOAT			Tex_U2;
	FLOAT			Tex_V2;
	/** The interpolation value							*/
	FLOAT			Interp;
	/** Padding...										*/
	FLOAT			Padding;
	/** The size of the sub-image						*/
	FLOAT			SizeU;
	FLOAT			SizeV;
#endif
};

//	FParticleSpriteSubUVVertexDynamicParameter
struct FParticleSpriteSubUVVertexDynamicParameter : public FParticleSpriteSubUVVertex
{
	/** The dynamic parameter of the particle			*/
	FLOAT			DynamicValue[4];
};

//	FParticlePointSpriteVertex
struct FParticlePointSpriteVertex
{
	/** The position of the particle					*/
	FVector			Position;
	/** The size of the particle						*/
	FLOAT			Size;
	/** The color of the particle						*/
	DWORD			Color;
};

//	FParticleBeamTrailVertex
struct FParticleBeamTrailVertex : public FParticleSpriteVertex
{
#if PARTICLES_USE_INDEXED_SPRITES
	/** The UV values of the particle					*/
	FLOAT			Tex_U;
	FLOAT			Tex_V;
#endif
	/** The second UV set for the particle				*/
	FLOAT			Tex_U2;
	FLOAT			Tex_V2;
};

//	FParticleBeamTrailVertexDynamicParameter
struct FParticleBeamTrailVertexDynamicParameter : public FParticleBeamTrailVertex
{
	/** The dynamic parameter of the particle			*/
	FLOAT			DynamicValue[4];
};

//
//  Trail emitter flags and macros
//
// ForceKill: Indicates all the particles in the trail should be killed in the next KillParticles call.
#define TRAIL_EMITTER_FLAG_FORCEKILL	0x00000000
// DeadTrail: indicates that the particle is the start of a trail than should no longer spawn.
//			  It should just fade out as the particles die...
#define TRAIL_EMITTER_FLAG_DEADTRAIL	0x10000000
// Middle: indicates the particle is in the middle of a trail.
#define TRAIL_EMITTER_FLAG_MIDDLE       0x20000000
// Start: indicates the particle is the start of a trail.
#define TRAIL_EMITTER_FLAG_START        0x40000000
// End: indicates the particle is the end of a trail.
#define TRAIL_EMITTER_FLAG_END          0x80000000

//#define TRAIL_EMITTER_FLAG_ONLY	        (TRAIL_EMITTER_FLAG_START | TRAIL_EMITTER_FLAG_END)
#define TRAIL_EMITTER_FLAG_MASK         0xf0000000
#define TRAIL_EMITTER_PREV_MASK         0x0fffc000
#define TRAIL_EMITTER_PREV_SHIFT        14
#define TRAIL_EMITTER_NEXT_MASK         0x00003fff
#define TRAIL_EMITTER_NEXT_SHIFT        0

#define TRAIL_EMITTER_NULL_PREV			(TRAIL_EMITTER_PREV_MASK >> TRAIL_EMITTER_PREV_SHIFT)
#define TRAIL_EMITTER_NULL_NEXT			(TRAIL_EMITTER_NEXT_MASK >> TRAIL_EMITTER_NEXT_SHIFT)

// Helper macros
#define TRAIL_EMITTER_CHECK_FLAG(val, mask, flag)				((val & mask) == flag)
#define TRAIL_EMITTER_SET_FLAG(val, mask, flag)					((val & ~mask) | flag)
#define TRAIL_EMITTER_GET_PREVNEXT(val, mask, shift)			((val & mask) >> shift)
#define TRAIL_EMITTER_SET_PREVNEXT(val, mask, shift, setval)	((val & ~mask) | ((setval << shift) & mask))

// Start/end accessor macros
#define TRAIL_EMITTER_IS_START(index)       TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_START)
#define TRAIL_EMITTER_SET_START(index)      TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_START)

#define TRAIL_EMITTER_IS_END(index)			TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_END)
#define TRAIL_EMITTER_SET_END(index)		TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_END)

#define TRAIL_EMITTER_IS_MIDDLE(index)		TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_MIDDLE)
#define TRAIL_EMITTER_SET_MIDDLE(index)		TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_MIDDLE)

// Only is used for the first emission from the emitter
#define TRAIL_EMITTER_IS_ONLY(index)		(TRAIL_EMITTER_IS_START(index)	&& (TRAIL_EMITTER_GET_NEXT(index) == TRAIL_EMITTER_NULL_NEXT))
#define TRAIL_EMITTER_SET_ONLY(index)		TRAIL_EMITTER_SET_START(index)

#define TRAIL_EMITTER_IS_FORCEKILL(index)	TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_FORCEKILL)
#define TRAIL_EMITTER_SET_FORCEKILL(index)	TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_FORCEKILL)

#define TRAIL_EMITTER_IS_DEADTRAIL(index)	TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_DEADTRAIL)
#define TRAIL_EMITTER_SET_DEADTRAIL(index)	TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_DEADTRAIL)

#define TRAIL_EMITTER_IS_HEAD(index)		(TRAIL_EMITTER_IS_START(index) || TRAIL_EMITTER_IS_DEADTRAIL(index))
#define TRAIL_EMITTER_IS_HEADONLY(index)	((TRAIL_EMITTER_IS_START(index) || TRAIL_EMITTER_IS_DEADTRAIL(index)) && \
											(TRAIL_EMITTER_GET_NEXT(index) == TRAIL_EMITTER_NULL_NEXT))

// Prev/Next accessor macros
#define TRAIL_EMITTER_GET_PREV(index)       TRAIL_EMITTER_GET_PREVNEXT(index, TRAIL_EMITTER_PREV_MASK, TRAIL_EMITTER_PREV_SHIFT)
#define TRAIL_EMITTER_SET_PREV(index, prev) TRAIL_EMITTER_SET_PREVNEXT(index, TRAIL_EMITTER_PREV_MASK, TRAIL_EMITTER_PREV_SHIFT, prev)
#define TRAIL_EMITTER_GET_NEXT(index)       TRAIL_EMITTER_GET_PREVNEXT(index, TRAIL_EMITTER_NEXT_MASK, TRAIL_EMITTER_NEXT_SHIFT)
#define TRAIL_EMITTER_SET_NEXT(index, next) TRAIL_EMITTER_SET_PREVNEXT(index, TRAIL_EMITTER_NEXT_MASK, TRAIL_EMITTER_NEXT_SHIFT, next)

/**
 * Particle trail stats
 */
enum EParticleTrailStats
{
	STAT_TrailParticles = STAT_TrailParticlesFirstStat,
	STAT_TrailParticlesRenderCalls,
	STAT_TrailParticlesSpawned,
	STAT_TrailParticlesTickCalls,
	STAT_TrailParticlesUpdated,
	STAT_TrailParticlesKilled,
	STAT_TrailParticlesTrianglesRendered,
	STAT_TrailFillVertexTime,
	STAT_TrailFillIndexTime,
	STAT_TrailRenderingTime,
	STAT_TrailTickTime,
	STAT_TrailSpawnTime,
	STAT_TrailUpdateTime,
	STAT_TrailPSysCompTickTime,
	STAT_AnimTrailNotifyTime
};

/**
 * Beam particle stats
 */
enum EBeamParticleStats
{
	STAT_BeamParticles = STAT_BeamParticlesFirstStat,
	STAT_BeamParticlesRenderCalls,
	STAT_BeamParticlesSpawned,
	STAT_BeamParticlesUpdateCalls,
	STAT_BeamParticlesUpdated,
	STAT_BeamParticlesKilled,
	STAT_BeamParticlesTrianglesRendered,
	STAT_BeamSpawnTime,
	STAT_BeamFillVertexTime,
	STAT_BeamFillIndexTime,
	STAT_BeamRenderingTime,
	STAT_BeamTickTime
};

#define _BEAM2_USE_MODULES_

/** Structure for multiple beam targets.								*/
struct FBeamTargetData
{
	/** Name of the target.												*/
	FName		TargetName;
	/** Percentage chance the target will be selected (100 = always).	*/
	FLOAT		TargetPercentage;
};

//
//	Helper structures for payload data...
//

//
//	StoreSpawnTime -related payloads
//
struct FStoreSpawnTimePayload
{
	FLOAT SpawnTime;
};

//
//	SubUV-related payloads
//
struct FFullSubUVPayload
{
	FLOAT	RandomImageTime;
	// Sprite:	ImageH, ImageV, Interpolation
	// Mesh:	UVOffset
	FVector	ImageHVInterp_UVOffset;
	// Sprite:	Image2H, Image2V
	// Mesh:	UV2Offset
	FVector	Image2HV_UV2Offset;
};

//
//	AttractorParticle
//
struct FAttractorParticlePayload
{
	INT			SourceIndex;
	UINT		SourcePointer;
	FVector		SourceVelocity;
};

//
//	TypeDataBeam2 payload
//
#define BEAM2_TYPEDATA_LOCKED_MASK					0x80000000
#define	BEAM2_TYPEDATA_LOCKED(x)					((x & BEAM2_TYPEDATA_LOCKED_MASK) != 0)
#define	BEAM2_TYPEDATA_SETLOCKED(x, Locked)			(x = Locked ? (x | BEAM2_TYPEDATA_LOCKED_MASK) : (x & ~BEAM2_TYPEDATA_LOCKED_MASK))

#define BEAM2_TYPEDATA_NOISEMAX_MASK				0x40000000
#define	BEAM2_TYPEDATA_NOISEMAX(x)					((x & BEAM2_TYPEDATA_NOISEMAX_MASK) != 0)
#define	BEAM2_TYPEDATA_SETNOISEMAX(x, Max)			(x = Max ? (x | BEAM2_TYPEDATA_NOISEMAX_MASK) : (x & ~BEAM2_TYPEDATA_NOISEMAX_MASK))

#define BEAM2_TYPEDATA_NOISEPOINTS_MASK				0x00000fff
#define	BEAM2_TYPEDATA_NOISEPOINTS(x)				(x & BEAM2_TYPEDATA_NOISEPOINTS_MASK)
#define BEAM2_TYPEDATA_SETNOISEPOINTS(x, Count)		(x = (x & ~BEAM2_TYPEDATA_NOISEPOINTS_MASK) | Count)

#define BEAM2_TYPEDATA_FREQUENCY_MASK				0x00fff000
#define BEAM2_TYPEDATA_FREQUENCY_SHIFT				12
#define	BEAM2_TYPEDATA_FREQUENCY(x)					((x & BEAM2_TYPEDATA_FREQUENCY_MASK) >> BEAM2_TYPEDATA_FREQUENCY_SHIFT)
#define BEAM2_TYPEDATA_SETFREQUENCY(x, Freq)		(x = ((x & ~BEAM2_TYPEDATA_FREQUENCY_MASK) | (Freq << BEAM2_TYPEDATA_FREQUENCY_SHIFT)))

struct FBeam2TypeDataPayload
{
	/** The source of this beam											*/
	FVector		SourcePoint;
	/** The source tangent of this beam									*/
	FVector		SourceTangent;
	/** The stength of the source tangent of this beam					*/
	FLOAT		SourceStrength;

	/** The target of this beam											*/
	FVector		TargetPoint;
	/** The target tangent of this beam									*/
	FVector		TargetTangent;
	/** The stength of the Target tangent of this beam					*/
	FLOAT		TargetStrength;

	/** Target lock, extreme max, Number of noise points				*/
	INT			Lock_Max_NumNoisePoints;

	/** Number of segments to render (steps)							*/
	INT			InterpolationSteps;

	/** Direction to step in											*/
	FVector		Direction;
	/** StepSize (for each segment to be rendered)						*/
	FLOAT		StepSize;
	/** Number of segments to render (steps)							*/
	INT			Steps;
	/** The 'extra' amount to travel (partial segment)					*/
	FLOAT		TravelRatio;

	/** The number of triangles to render for this beam					*/
	INT			TriangleCount;

	/**
	 *	Type and indexing flags
	 * 3               1              0
	 * 1...|...|...|...5...|...|...|..0
	 * TtPppppppppppppppNnnnnnnnnnnnnnn
	 * Tt				= Type flags --> 00 = Middle of Beam (nothing...)
	 * 									 01 = Start of Beam
	 * 									 10 = End of Beam
	 * Ppppppppppppppp	= Previous index
	 * Nnnnnnnnnnnnnnn	= Next index
	 * 		INT				Flags;
	 * 
	 * NOTE: These values DO NOT get packed into the vertex buffer!
	 */
	INT			Flags;
};

/**	Particle Source/Target Data Payload									*/
struct FBeamParticleSourceTargetPayloadData
{
	INT			ParticleIndex;
};

/**	Particle Source Branch Payload										*/
struct FBeamParticleSourceBranchPayloadData
{
	INT			NoiseIndex;
};

/** Particle Beam Modifier Data Payload */
struct FBeamParticleModifierPayloadData
{
	BITFIELD	bModifyPosition:1;
	BITFIELD	bScalePosition:1;
	BITFIELD	bModifyTangent:1;
	BITFIELD	bScaleTangent:1;
	BITFIELD	bModifyStrength:1;
	BITFIELD	bScaleStrength:1;
	FVector		Position;
	FVector		Tangent;
	FLOAT		Strength;

	// Helper functions
	FORCEINLINE void UpdatePosition(FVector& Value)
	{
		if (bModifyPosition == TRUE)
		{
			if (bScalePosition == FALSE)
			{
				Value += Position;
			}
			else
			{
				Value *= Position;
			}
		}
	}

	FORCEINLINE void UpdateTangent(FVector& Value, UBOOL bAbsolute)
	{
		if (bModifyTangent == TRUE)
		{
			FVector ModTangent = Tangent;

			if (bAbsolute == FALSE)
			{
				// Transform the modified tangent so it is relative to the real tangent
				FQuat RotQuat = FQuatFindBetween(FVector(1.0f, 0.0f, 0.0f), Value);
				FMatrix RotMat = FQuatRotationTranslationMatrix(RotQuat, FVector(0.0f));

				ModTangent = RotMat.TransformNormal(Tangent);
			}

			if (bScaleTangent == FALSE)
			{
				Value += ModTangent;
			}
			else
			{
				Value *= ModTangent;
			}
		}
	}

	FORCEINLINE void UpdateStrength(FLOAT& Value)
	{
		if (bModifyStrength == TRUE)
		{
			if (bScaleStrength == FALSE)
			{
				Value += Strength;
			}
			else
			{
				Value *= Strength;
			}
		}
	}
};

//
//	Trail2 payload data
//
struct FTrail2TypeDataPayload
{
	/**
	 *	Type and indexing flags
	 * 3               1              0
	 * 1...|...|...|...5...|...|...|..0
	 * TtPppppppppppppppNnnnnnnnnnnnnnn
	 * Tt				= Type flags --> 00 = Middle of Beam (nothing...)
	 * 									 01 = Start of Beam
	 * 									 10 = End of Beam
	 * Ppppppppppppppp	= Previous index
	 * Nnnnnnnnnnnnnnn	= Next index
	 * 		INT				Flags;
	 * 
	 * NOTE: These values DO NOT get packed into the vertex buffer!
	 */
	INT			Flags;

	/** The trail index - START only							*/
	INT			TrailIndex;
	/** The number of triangle in the trail	- START only		*/
	INT			TriangleCount;
	/** The velocity of the particle - to allow moving trails	*/
	FVector		Velocity;
	/**	Tangent for the trail segment							*/
	FVector		Tangent;
};

/**	Particle Source Data Payload									*/
struct FTrailParticleSourcePayloadData
{
	INT			ParticleIndex;
};

/** Trails Base data payload */
struct FTrailsBaseTypeDataPayload
{
	/**
	 * TRAIL_EMITTER_FLAG_MASK         0xf0000000
	 * TRAIL_EMITTER_PREV_MASK         0x0fffc000
	 * TRAIL_EMITTER_PREV_SHIFT        14
	 * TRAIL_EMITTER_NEXT_MASK         0x00003fff
	 * TRAIL_EMITTER_NEXT_SHIFT        0

	 *	Type and indexing flags
	 *	3               1              0
	 *	1...|...|...|...5...|...|...|..0
	 *	TtttPpppppppppppppNnnnnnnnnnnnnn
	 *
	 *	Tttt = Type flags
	 *		0x0 = ForceKill	- the trail should be completely killed in the next KillParticles call.
	 *		0x1	= DeadTrail	- the trail should no longer spawn particles. Just let it die out as particles in it fade.
	 *		0x2	= Middle	- indicates this is a particle in the middle of a trail.
	 *		0x4	= Start		- indicates this is the first particle in a trail.
	 *		0x8	= End		- indicates this is the last particle in a trail.
	 *	Pppppppppppppp	= Previous index
	 *	Nnnnnnnnnnnnnn	= Next index
	 */
	INT Flags;
	/** The trail index - valid in a START particle only */
	INT TrailIndex;
	/** The number of triangles in the trail - valid in a START particle only */
	INT TriangleCount;
	/** The time that the particle was spawned */
	FLOAT SpawnTime;
	/** The time slice when the particle was spawned */
	FLOAT SpawnDelta;
	/** The starting tiled U value for this particle */
	FLOAT TiledU;
	/** The tessellated spawn points between this particle and the next one */
	INT SpawnedTessellationPoints;
	/** The number of points to interpolate between this particle and the next when rendering */
	INT RenderingInterpCount;
	/** The scale factor to use to shrink up in tight curves */
	FLOAT PinchScaleFactor;
	/** TRUE if the particle is an interpolated spawn, FALSE if true position based. */
	BITFIELD bInterpolatedSpawn:1;
	/** TRUE if the particle was spawn via movement, false if not. */
	BITFIELD bMovementSpawned:1;
};

struct FRibbonTypeDataPayload : public FTrailsBaseTypeDataPayload
{
	/**	Tangent for the trail segment */
	FVector Tangent;
	/**	The 'up' for the segment (render plane) */
	FVector Up;
	/** The source index tracker (particle index, etc.) */
	INT SourceIndex;
};

/** AnimTrail payload */
struct FAnimTrailTypeDataPayload : public FTrailsBaseTypeDataPayload
{
	/**	The first edge of the trail */
	FVector FirstEdge;
	/**	The first edge velocity of the trail */
	FVector FirstVelocity;
	/**	The second edge of the trail */
	FVector SecondEdge;
	/**	The second edge velocity of the trail */
	FVector SecondVelocity;
	/**	The control edge of the trail will be the particle position */
	//FVector ControlEdge;
	/**	The control edge velocity of the trail */
	FVector ControlVelocity;
};

/** Mesh rotation data payload										*/
struct FMeshRotationPayloadData
{
	FVector  Rotation;
	FVector  RotationRate;
	FVector  RotationRateBase;
};

/** Mesh emitter particle data payload */
struct FMeshTypeDataPayload
{
	/** Particle index needed for motion blur info lookup */
	INT		ParticleId;
};

/** Mesh emitter instance data payload */
struct FMeshTypeDataInstancePayload
{
	/** Particle index needed for motion blur info lookup per instance */
	INT		ParticleId;
};

/** ModuleLocationEmitter instance payload							*/
struct FLocationEmitterInstancePayload
{
	INT		LastSelectedIndex;
};

/** ModuleLocationBoneSocket instance payload */
struct FModuleLocationBoneSocketInstancePayload
{
	/** The skeletal mesh component used as the source of the sockets */
	class USkeletalMeshComponent* SourceComponent;
	/** The last selected index into the socket array */
	INT LastSelectedIndex;
	/** The index of the current 'unused' indices */
	INT CurrentUnused;
	/** The currently unselected indices */
	TArray<BYTE> Indices[2];
	/** Store off the last position for every index in use. */
	TArray<FVector> LastPosition;
};

/** ModuleLocationBoneSocket per-particle payload - only used if updating each frame */
struct FModuleLocationBoneSocketParticlePayload
{
	/** The index of the socket this particle is 'attached' to */
	INT SourceIndex;
};

/** ModuleLocationVertSurface instance payload */
struct FModuleLocationVertSurfaceInstancePayload
{
	/** The skeletal mesh component used as the source of the sockets */
	class USkeletalMeshComponent* SourceComponent;
	/** Bone indices for the associated bone names. */
	TArray<INT> ValidAssociatedBoneIndices;
	/** Valid material indices. */
	TArray<INT> ValidAssociatedMaterialIndices;
};

/** ModuleLocationVertSurface per-particle payload - only used if updating each frame */
struct FModuleLocationVertSurfaceParticlePayload
{
	/** The index of the socket this particle is 'attached' to */
	INT SourceIndex;
};

/** ModuleAttractorVertSurface instance payload */
struct FModuleAttractorVertSurfaceInstancePayload
{
	/** The skeletal mesh component used as the source of the sockets */
	class USkeletalMeshComponent* SourceComponent;
	/** Bone indices for the associated bone names. */
	TArray<INT> ValidAssociatedBoneIndices;
	/** Valid material indices. */
	TArray<INT> ValidAssociatedMaterialIndices;
	/** The index of the vertice this particle system spawns from */
	INT VertIndex;
};

/** ModuleAttractorVertSurface per-particle payload - only used if updating each frame */
struct FModuleAttractorVertSurfaceParticlePayload
{
	/** The index of the socket this particle is 'attached' to */
	INT SourceIndex;
};

/** ModuleLocationStaticVertSurface instance payload */
struct FModuleLocationStaticVertSurfaceInstancePayload
{
	/** The static mesh component used as the source of the vert positions */
	class UStaticMeshComponent* SourceComponent;
};

/** ModuleLocationBoneSocket instance payload */
struct FModuleAttractorBoneSocketInstancePayload
{
	/** The skeletal mesh component used as the source of the sockets */
	class USkeletalMeshComponent* SourceComponent;
	/** The last selected index into the socket array */
	INT LastSelectedIndex;
	/** The index of the current 'unused' indices */
	INT CurrentUnused;
	/** The currently unselected indices */
	TArray<BYTE> Indices[2];
	/** Store off the last position for every index in use. */
	TArray<FVector> LastPosition;
};

/** ModuleLocationBoneSocket per-particle payload - only used if updating each frame */
struct FModuleAttractorBoneSocketParticlePayload
{
	/** The index of the socket this particle is 'attached' to */
	INT SourceIndex;
};

/** ModuleAttractorBoneSocket per particle payload with random position along bone - only used when bAttractAlongLengthOfBone is TRUE */
struct FModuleAttractorBoneSocketParticlePayloadWithBoneLerpAlpha : public FModuleAttractorBoneSocketParticlePayload
{
	/** The alpha value to use when lerping between the ends of the given bone. */
	FLOAT RandomLerpAlpha;
};

/**
 *	Chain-able Orbit module instance payload
 */
struct FOrbitChainModuleInstancePayload
{
	/** The base offset of the particle from it's tracked location	*/
	FVector	BaseOffset;
	/** The offset of the particle from it's tracked location		*/
	FVector	Offset;
	/** The rotation of the particle at it's offset location		*/
	FVector	Rotation;
	/** The base rotation rate of the particle offset				*/
	FVector	BaseRotationRate;
	/** The rotation rate of the particle offset					*/
	FVector	RotationRate;
	/** The offset of the particle from the last frame				*/
	FVector	PreviousOffset;
};

/**
 *	Payload for instances which use the SpawnPerUnit module.
 */
struct FParticleSpawnPerUnitInstancePayload
{
	FLOAT	CurrentDistanceTravelled;
};

/** 
 *	SpawnInstance information
 */
struct FParticleSpawnInstanceInfo
{
	/** The LocalToWorld for the instance */
	FMatrix LocalToWorld;
	/** The current 'emitter time' of this instance */
	FLOAT EmitterTime;

};

/**
 *	Payload for instances which use the SpawnInstance module.
 */
struct FParticleSpawnInstanceInstancePayload
{
	
};

/**
 *	Collision module particle payload
 */
struct FParticleCollisionPayload
{
	FVector	UsedDampingFactor;
	FVector	UsedDampingFactorRotation;
	INT		UsedCollisions;
	FLOAT	Delay;
};

/** Collision module per instance payload */
struct FParticleCollisionInstancePayload
{
	/** Count for tracking how many times the bounds checking was skipped */
	BYTE CurrentLODBoundsCheckCount;
	/** Padding for potential future expansion */
	BYTE Padding1;
	BYTE Padding2;
	BYTE Padding3;
};

/**
 *	Helper struct for tracking pawn collisionss
 */
struct FParticlePawnCollisionInfo
{
	FBox	PawnBox;
	APawn*	Pawn;

	FParticlePawnCollisionInfo(FBox& InBox, APawn* InPawn) :
		  PawnBox(InBox)
		, Pawn(InPawn)
	{
	}
};

/**
 *	Collision actor module instance payload
 */
struct FParticleCollisionActorInstancePayload : public FParticleCollisionInstancePayload
{
	TArray<FParticlePawnCollisionInfo> PawnList;
	TArray<AActor*> ActorList;
};

/**
 *	General event instance payload.
 */
struct FParticleEventInstancePayload
{
	/** Spawn event instance payload. */
	UBOOL bSpawnEventsPresent;
	INT SpawnTrackingCount;
	/** Death event instance payload. */
	UBOOL bDeathEventsPresent;
	INT DeathTrackingCount;
	/** Collision event instance payload. */
	UBOOL bCollisionEventsPresent;
	INT CollisionTrackingCount;
	/** Attractor Collision event instance payload. */
	UBOOL bAttractorCollisionEventsPresent;
	INT AttractorCollisionTrackingCount;
};

/**
 *	DynamicParameter particle payload.
 */
struct FEmitterDynamicParameterPayload
{
#if WITH_MOBILE_RHI || WITH_EDITOR
	/** Index into DynamicParameterValue where there is a value for parameter named Time **/
	int TimeIndex;
#endif //WITH_MOBILE_RHI || WITH_EDITOR

	/** The float4 value to assign to the dynamic parameter. */
	FVector4 DynamicParameterValue;
};

/**
 *	ScaleSizeByLife payload
 */
struct FScaleSizeByLifePayload
{
	FLOAT AbsoluteTime;
};

/** Camera offset particle payload */
struct FCameraOffsetParticlePayload
{
	/** The base amount to offset the particle towards the camera */
	FLOAT	BaseOffset;
	/** The amount to offset the particle towards the camera */
	FLOAT	Offset;
};

/** Random-seed instance payload */
struct FParticleRandomSeedInstancePayload
{
	FRandomStream	RandomStream;
};

/*-----------------------------------------------------------------------------
	Particle Sorting Helper
-----------------------------------------------------------------------------*/
struct FParticleOrder
{
	INT		ParticleIndex;
	FLOAT	Z;
	
	FParticleOrder(INT InParticleIndex,FLOAT InZ):
		ParticleIndex(InParticleIndex),
		Z(InZ)
	{}
};


/*-----------------------------------------------------------------------------
	Async Fill Organizational Structure
-----------------------------------------------------------------------------*/

struct FAsyncBufferFillData
{
	/** View for this buffer fill task   */
	const FSceneView*					View;
	/** Number of verts in VertexData   */
	INT									VertexCount;
	/** Stride of verts, used only for error checking   */
	INT									VertexSize; 
	/** Pointer to vertex data   */
	void*								VertexData;
	/** Number of indices in IndexData   */
	INT									IndexCount;
	/** Pointer to index data   */
	void*								IndexData;
	/** Number of triangles filled in   */
	INT									OutTriangleCount;
	/** Number of degenerate triangles filled in   */
	INT									OutDegenerateTriangleCount;
	
	/** Constructor, just zeros everything   */
	FAsyncBufferFillData()
	{
		// this is all POD
		appMemzero(this,sizeof(FAsyncBufferFillData));
	}
	/** Destructor, frees memory and zeros everything   */
	~FAsyncBufferFillData()
	{
		appFree(VertexData);
		appFree(IndexData);
		appMemzero(this,sizeof(FAsyncBufferFillData));
	}
};

/*-----------------------------------------------------------------------------
	Async Fill Task, simple wrapper to forward the request to a FDynamicSpriteEmitterDataBase
-----------------------------------------------------------------------------*/

struct FAsyncParticleFill : FNonAbandonableTask
{
	/** Emitter to forward to   */
	struct FDynamicSpriteEmitterDataBase* Parent;

	/** Constructor, just sets up the parent pointer  
	  * @param InParent emitter to forward the eventual async call to
	*/
	FAsyncParticleFill(struct FDynamicSpriteEmitterDataBase* InParent)
		: Parent(InParent)
	{
	}

	/** Work function, just forwards the request to the parent  */
	void DoWork();

	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncParticleFill");
	}

	/** Allocate and return a new particle fill task
	  * @param InParent emitter to forward the eventual async call to
	*/
	static FAsyncTask<FAsyncParticleFill>* GetAsyncTask(struct FDynamicSpriteEmitterDataBase* InParent);
	/** Return a task to the async task pool for recycling. Will call EnsureCompletion
	  * @param TaskToRecycle task to recycle
	*/
	static void DisposeAsyncTask(FAsyncTask<FAsyncParticleFill>* TaskToRecycle);

};

// TAsyncBufferFillTasks - handy typedef for an inline array of buffer fill tasks
typedef TArray<FAsyncBufferFillData, TInlineAllocator<2> > TAsyncBufferFillTasks;


/*-----------------------------------------------------------------------------
	Particle vertex factory pools
-----------------------------------------------------------------------------*/
enum EParticleVertexFactoryType
{
	PVFT_Sprite,
	PVFT_Sprite_DynamicParameter,
	PVFT_SubUV,
	PVFT_SubUV_DynamicParameter,
	PVFT_PointSprite,
	PVFT_BeamTrail,
	PVFT_BeamTrail_DynamicParameter,
	PVFT_MAX
};

class FParticleVertexFactory;

class FParticleVertexFactoryPool
{
public:
	FParticleVertexFactoryPool()
	{
	}

	~FParticleVertexFactoryPool()
	{
		ClearPool();
	}

	FParticleVertexFactory* GetParticleVertexFactory(EParticleVertexFactoryType InType);

	UBOOL ReturnParticleVertexFactory(FParticleVertexFactory* InVertexFactory);

	void ClearPool();

	void FreePool();

#if STATS
	const TCHAR* GetTypeString(EParticleVertexFactoryType InType)
	{
		switch (InType)
		{
		case PVFT_Sprite:						return TEXT("Sprite");
		case PVFT_Sprite_DynamicParameter:		return TEXT("SpriteDynParam");
		case PVFT_SubUV:						return TEXT("SubUV");
		case PVFT_SubUV_DynamicParameter:		return TEXT("SubUVDynParam");
		case PVFT_PointSprite:					return TEXT("PointSprite");
		case PVFT_BeamTrail:					return TEXT("BeamTrail");
		case PVFT_BeamTrail_DynamicParameter:	return TEXT("BeamTrailDynParam");
		default:								return TEXT("UNKNOWN");
		}
	}

	INT GetTypeSize(EParticleVertexFactoryType InType);

	void DumpInfo(FOutputDevice& Ar);
#endif

protected:
	/** 
	 *	Create a vertex factory for the given type.
	 *
	 *	@param	InType						The type of vertex factory to create.
	 *
	 *	@return	FParticleVertexFactory*		The created VF; NULL if invalid InType
	 */
	FParticleVertexFactory* CreateParticleVertexFactory(EParticleVertexFactoryType InType);

	TArray<FParticleVertexFactory*>	VertexFactoriesAvailable[PVFT_MAX];
	TArray<FParticleVertexFactory*>	VertexFactories;
};

extern FParticleVertexFactoryPool GParticleVertexFactoryPool;

/** 
 *	Function to free up the resources in the ParticleVertexFactoryPool
 *	Should only be called at application exit
 */
void ParticleVertexFactoryPool_FreePool();

/*-----------------------------------------------------------------------------
	Particle order helper class
-----------------------------------------------------------------------------*/
class FParticleOrderPool
{
public:
	FParticleOrderPool() :
		  ParticleOrder(NULL)
		, CurrentSize(0)
		, MaxSize(0)
	{
	}

	~FParticleOrderPool()
	{
		FreePool();
	}

	FParticleOrder* GetParticleOrderData(INT InCount, UBOOL bZeroMem = FALSE)
	{
		if (InCount > MaxSize)
		{
			MaxSize = Max<INT>(InCount, 64);
			ParticleOrder = (FParticleOrder*)appRealloc(ParticleOrder, MaxSize * sizeof(FParticleOrder));
			check(ParticleOrder);
			if (bZeroMem == TRUE)
			{
				appMemzero(ParticleOrder, MaxSize * sizeof(FParticleOrder));
			}
		}
		CurrentSize = InCount;
		return ParticleOrder;
	}

	void FreePool()
	{
		appFree(ParticleOrder);
		ParticleOrder = NULL;
		CurrentSize = 0;
		MaxSize = 0;
	}

#if STATS
	void DumpInfo(FOutputDevice& Ar)
	{
		Ar.Logf(TEXT("Particle Order Pool Stats"));
		Ar.Logf(TEXT("%5d entries for %5d bytes"), MaxSize, MaxSize * sizeof(FParticleOrder));
	}
#endif

protected:
	FParticleOrder* ParticleOrder;
	INT CurrentSize;
	INT MaxSize;
};

extern FParticleOrderPool GParticleOrderPool;

/*-----------------------------------------------------------------------------
	Particle Dynamic Data
-----------------------------------------------------------------------------*/
/**
 *	The information required for rendering sprite-based particles
 */
struct FParticleSpriteData
{
	/** Current location of the particle.			*/
	FVector			Location;
	/** Last frame's location of the particle.		*/
	FVector			OldLocation;
	/** Rotation of the particle (in Radians).		*/
	FLOAT			Rotation;
	/** Current size of the particle.				*/
	FVector			Size;
	/** Current color of the particle.				*/
	FLinearColor	Color;
};

/**
 * Dynamic particle emitter types
 *
 * NOTE: These are serialized out for particle replay data, so be sure to update all appropriate
 *    when changing anything here.
 */
enum EDynamicEmitterType
{
	DET_Unknown = 0,
	DET_Sprite,
	DET_SubUV,
	DET_Mesh,
	DET_Beam,
	DET_Beam2,
	DET_Trail,
	DET_Trail2,
	DET_Ribbon,
	DET_AnimTrail,
	DET_Custom,
	DET_Apex
};

/** Source data base class for all emitter types */
struct FDynamicEmitterReplayDataBase
{
	/**	The type of emitter. */
	EDynamicEmitterType	eEmitterType;

	/**	The number of particles currently active in this emitter. */
	INT ActiveParticleCount;

	INT ParticleStride;
	TArray<BYTE> ParticleData;
	TArray<WORD> ParticleIndices;

	/** If the game thread manages memory for the render data, this will point directly to vertex data */
	BYTE* ParticleRenderData;

	/** If the game thread manages memory for the render data, this will point directly to index data */
	WORD* ParticleRenderIndices;

	FVector Scale;

	/** Whether this emitter requires sorting as specified by artist.	*/
	INT SortMode;

	/** MacroUV (override) data **/
	UBOOL   bOverrideSystemMacroUV;
	FLOAT   MacroUVRadius;
	FVector MacroUVPosition;


	/** Constructor */
	FDynamicEmitterReplayDataBase()
		: eEmitterType( DET_Unknown ),
		  ActiveParticleCount( 0 ),
		  ParticleStride( 0 ),
		  ParticleRenderData(NULL),
		  ParticleRenderIndices(NULL),
		  Scale( FVector( 1.0f ) ),
		  SortMode(0),	// Default to PSORTMODE_None
		  bOverrideSystemMacroUV(0),
	      MacroUVRadius(0.f),
	      MacroUVPosition(0.f,0.f,0.f)
	{
	}

	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		INT EmitterTypeAsInt = eEmitterType;
		Ar << EmitterTypeAsInt;
		eEmitterType = static_cast< EDynamicEmitterType >( EmitterTypeAsInt );

		Ar << ActiveParticleCount;
		Ar << ParticleStride;
		Ar << ParticleData;
		Ar << ParticleIndices;
		Ar << Scale;
		Ar << SortMode;
		Ar << bOverrideSystemMacroUV;
	    Ar << MacroUVRadius;
	    Ar << MacroUVPosition;
	}

};

/** Base class for all emitter types */
struct FDynamicEmitterDataBase
{
	FDynamicEmitterDataBase(const class UParticleModuleRequired* RequiredModule);
	
	virtual ~FDynamicEmitterDataBase()
	{
	}

	/**
	 *	Allow any pending async tasks to complete...
	 */
	virtual void WaitOnAsyncTasks()
	{
	}

	/**
	 *	Indicates whether this emitter has outstanding asynchronous operations or not
	 */
	virtual UBOOL HasPendingAsyncTasks()
	{
		return FALSE;
	}

	/**
	 *	Create the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
	{
		return TRUE;
	}

	/**
	 *	Release the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
	{
		return TRUE;
	}

	/**
	 *	Render thread only draw call
	 *
	 *	@param	Proxy		The scene proxy for the particle system that owns this emitter
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	View		The scene view being rendered
	 *	@param	DPGIndex	The depth priority group being rendered
	 */
	virtual INT Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex) = 0;

	/**
	 *	Retrieve the material render proxy to use for rendering this emitter. PURE VIRTUAL
	 *
	 *	@param	bSelected				Whether the object is selected
	 *
	 *	@return	FMaterialRenderProxy*	The material proxt to render with.
	 */
	virtual const FMaterialRenderProxy* GetMaterialRenderProxy(UBOOL bSelected) = 0;

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	Proxy			The 'owner' particle system scene proxy
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber) {}

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;

	/** Release the resource for the data */
	virtual void ReleaseResource() {}

	/**
	 *	Whether this emitter should render downsampled
	 *
	 *	@param	View		The scene view being rendered
	 *	@param	Bounds		The bounds of the owning particle system
	 *
	 *	@return	UBOOL		TRUE if it should be rendered downsampled, FALSE if not
	 */
	UBOOL ShouldRenderDownsampled(const FSceneView* View, const FBoxSphereBounds& Bounds) const;

	/** TRUE if this emitter is currently selected */
	BITFIELD	bSelected:1;
	/** TRUE if this emitter has valid rendering data */
	BITFIELD	bValid:1;

	/** Fraction of the screen that the particle system's bounds must be larger than for the module to be rendered downsampled. */
	FLOAT		DownsampleThresholdScreenFraction;	

	/** The scene proxy - only used during rendering!					*/
	FParticleSystemSceneProxy* SceneProxy;
};

/** Source data base class for Sprite emitters */
struct FDynamicSpriteEmitterReplayDataBase
	: public FDynamicEmitterReplayDataBase
{
	BYTE						ScreenAlignment;
	UBOOL						bUseLocalSpace;
	UBOOL						bAllowImageFlipping;
	UBOOL						bSquareImageFlipping;
	UBOOL						bLockAxis;
	BYTE						LockAxisFlag;
	INT							MaxDrawCount;
	INT							EmitterRenderMode;
	INT							OrbitModuleOffset;
	INT							DynamicParameterDataOffset;
	INT							CameraPayloadOffset;
	BYTE						EmitterNormalsMode;
	FVector						NormalsSphereCenter;
	FVector						NormalsCylinderDirection;
	UMaterialInterface*			MaterialInterface;


	/** Constructor */
	FDynamicSpriteEmitterReplayDataBase()
		: ScreenAlignment(0)
		, bUseLocalSpace(FALSE)
		, bAllowImageFlipping(FALSE)
		, bSquareImageFlipping(TRUE)
		, bLockAxis(FALSE)
		, LockAxisFlag(0)
		, MaxDrawCount(0)
		, EmitterRenderMode(0)
		, OrbitModuleOffset(0)
		, DynamicParameterDataOffset(0)
		, CameraPayloadOffset(0)
		, EmitterNormalsMode(0)
		, MaterialInterface( NULL )
	{
	}

	/** Serialization */
	virtual void Serialize( FArchive& Ar );

};

/** Base class for Sprite emitters and other emitter types that share similar features. */
struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	FDynamicSpriteEmitterDataBase(const UParticleModuleRequired* RequiredModule) : 
		FDynamicEmitterDataBase(RequiredModule),
		bAsyncTaskOutstanding(FALSE),
		AsyncTask(NULL),
		bUsesDynamicParameter( FALSE )
	{
		MaterialResource[0] = NULL;
		MaterialResource[1] = NULL;
	}

	virtual ~FDynamicSpriteEmitterDataBase()
	{
		// Make sure any AsyncTasks have completed...
		FAsyncParticleFill::DisposeAsyncTask(AsyncTask);
		AsyncTask = NULL;
	}

	virtual void WaitOnAsyncTasks()
	{
		FAsyncParticleFill::DisposeAsyncTask(AsyncTask);
		AsyncTask = NULL;
	}

	/**
	 *	Indicates whether this emitter has outstanding asynchronous operations or not
	 */
	virtual UBOOL HasPendingAsyncTasks()
	{
		if (AsyncTask != NULL)
		{
			return !AsyncTask->IsDone();
		}

		return FALSE;
	}

	/**
	 *	Retrieve the material render proxy to use for rendering this emitter. PURE VIRTUAL
	 *
	 *	@param	bSelected				Whether the object is selected
	 *
	 *	@return	FMaterialRenderProxy*	The material proxt to render with.
	 */
	const FMaterialRenderProxy* GetMaterialRenderProxy(UBOOL bSelected) 
	{ 
		return MaterialResource[bSelected]; 
	}

	/**
	 *	Sort the given sprite particles
	 *
	 *	@param	SorceMode			The sort mode to utilize (EParticleSortMode)
	 *	@param	bLocalSpace			TRUE if the emitter is using local space
	 *	@param	ParticleCount		The number of particles
	 *	@param	ParticleData		The actual particle data
	 *	@param	ParticleStride		The stride between entries in the ParticleData array
	 *	@param	ParticleIndices		Indirect index list into ParticleData
	 *	@param	View				The scene view being rendered
	 *	@param	LocalToWorld		The local to world transform of the component rendering the emitter
	 *	@param	ParticleOrder		The array to fill in with ordered indices
	 */
	void SortSpriteParticles(INT SortMode, UBOOL bLocalSpace, 
		INT ParticleCount, const TArray<BYTE>& ParticleData, INT ParticleStride, const TArray<WORD>& ParticleIndices,
		const FSceneView* View, FMatrix& LocalToWorld, FParticleOrder* ParticleOrder);

	/**
	 *	Get the vertex stride for the dynamic rendering data
	 */
	virtual INT GetDynamicVertexStride() const
	{
		checkf(0, TEXT("GetDynamicVertexStride MUST be overridden"));
		return 0;
	}

	/**
	 *	Get the vertex factory to use when rendering
	 */
	virtual class FParticleVertexFactory* GetVertexFactory()
	{
		checkf(0, TEXT("GetVertexFactory MUST be overridden"));
		return NULL;
	}

	/**
	 *	Get the source replay data for this emitter
	 */
	virtual const FDynamicSpriteEmitterReplayDataBase* GetSourceData() const
	{
		checkf(0, TEXT("GetSourceData MUST be overridden"));
		return NULL;
	}

	/**
	 *	Debug rendering
	 *
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	VIew		The scene view being rendered
	 *	@param	DPGIndex	The depth priority group being rendered
	 *	@param	bCrosses	If TRUE, render Crosses at particle position; FALSE, render points
	 */
	virtual void RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses);

	/**
	 *	Helper function for retrieving the dynamic payload of a particle.
	 *
	 *	@param	InDynamicPayloadOffset		The offset to the payload
	 *	@param	InParticle					The particle being processed
	 *	@param	OutDynamicData				The dynamic data from the particle
	 */
	FORCEINLINE void GetDynamicValueFromPayload(INT InDynamicPayloadOffset, FBaseParticle& InParticle, FVector4& OutDynamicData)
	{
		checkSlow(InDynamicPayloadOffset > 0);
		FEmitterDynamicParameterPayload* DynPayload = ((FEmitterDynamicParameterPayload*)((BYTE*)(&InParticle) + InDynamicPayloadOffset));
		OutDynamicData.X = DynPayload->DynamicParameterValue.X;
		OutDynamicData.Y = DynPayload->DynamicParameterValue.Y;
		OutDynamicData.Z = DynPayload->DynamicParameterValue.Z;
		OutDynamicData.W = DynPayload->DynamicParameterValue.W;
	}

	/**
	 *	Helper function for retrieving the camera offset payload of a particle.
	 *
	 *	@param	InSource			The replay data source
	 *	@param	InLocalToWorld		The local to world transform of the particle system component
	 *	@param	InParticle			The particle being processed
	 *	@param	InPosition			The position of the particle being processed
	 *	@param	OutPosition			The resulting position of the particle w/ camera offset applied
	 */
	FORCEINLINE void GetCameraOffsetFromPayload(
		const FDynamicSpriteEmitterReplayDataBase& InSource, 
		const FMatrix& InLocalToWorld, const FBaseParticle& InParticle, 
		const FVector& InPosition, FVector& OutPosition, UBOOL bCalculatingOldPosition)
	{
 		checkSlow(InSource.CameraPayloadOffset > 0);

		FVector ParticleLocation = InPosition;
		if (InSource.bUseLocalSpace == TRUE)
		{
			ParticleLocation = InLocalToWorld.TransformFVector(InPosition);
			if (bCalculatingOldPosition == TRUE)
			{
				// When calculating the old position, we want the camera offset based off
				// of the current position... So, In and Out positions will actually be
				// different values...
				OutPosition = InLocalToWorld.TransformFVector(OutPosition);
			}
			else
			{
				OutPosition = ParticleLocation;
			}
		}

		FVector DirToCamera = CameraPosition - ParticleLocation;
		FLOAT CheckSize = DirToCamera.SizeSquared();
		DirToCamera.Normalize();
		FCameraOffsetParticlePayload* CameraPayload = ((FCameraOffsetParticlePayload*)((BYTE*)(&InParticle) + InSource.CameraPayloadOffset));
		if (CheckSize > (CameraPayload->Offset * CameraPayload->Offset))
		{
			OutPosition = OutPosition + DirToCamera * CameraPayload->Offset;
		}
		else
		{
			// If the offset will push the particle behind the camera, then push it 
			// WAY behind the camera. This is a hack... but in the case of 
			// PSA_Velocity, it is required to ensure that the particle doesn't 
			// 'spin' flat and come into view.
			OutPosition = OutPosition + DirToCamera * CameraPayload->Offset * HALF_WORLD_MAX;
		}

		if (InSource.bUseLocalSpace == TRUE)
		{
			OutPosition = InLocalToWorld.InverseTransformFVector(OutPosition);
		}
	}

	/**
	 *	Fill index and vertex buffers. Often called from a different thread
	 *
	 */
	void DoBufferFill()
	{
		for (INT TaskIndex = 0; TaskIndex < AsyncBufferFillTasks.Num(); TaskIndex++) 
		{
			FAsyncBufferFillData& Data = AsyncBufferFillTasks(TaskIndex);
			DoBufferFill(Data);
		}
	}
	/**
	 *	Fill index and vertex buffers. Often called from a different thread
	 *
	 *	@param	Me			buffer pair to compute
	 */
	virtual void DoBufferFill(FAsyncBufferFillData& Me)
	{
		// this must be overridden, but in some cases a destructor call will leave this a no-op
		// because the vtable has been reset to the base class 
		// checkf(0, TEXT("DoBufferFill MUST be overridden"));
	}
	/**
	 *	Set up an buffer for async filling
	 *
	 *	@param	InBufferIndex			Index of this buffer
	 *	@param	InView					View for this buffer
	 *	@param	InVertexCount			Count of verts for this buffer
	 *	@param	InVertexSize			Stride of these verts, only used for verification
	 */
	void BuildViewFillData(INT InBufferIndex,const FSceneView *InView,INT InVertexCount,INT InVertexSize)
	{
		if (InBufferIndex >= AsyncBufferFillTasks.Num())
		{
			new (AsyncBufferFillTasks) FAsyncBufferFillData();
		}
		check(InBufferIndex < AsyncBufferFillTasks.Num()); // please add the views in order
		FAsyncBufferFillData& Data = AsyncBufferFillTasks(InBufferIndex);
		Data.View = InView;

		check(Data.VertexSize == 0 || Data.VertexSize == InVertexSize);
		if (Data.VertexData == NULL || Data.VertexCount < InVertexCount)
		{
			if (Data.VertexData)
			{
				appFree(Data.VertexData);
			}
			Data.VertexData = appMalloc(InVertexCount * InVertexSize);
			Data.VertexCount = InVertexCount;
			Data.VertexSize = InVertexSize;
		}
	}

	/**
	 *	Set up all buffers for async filling
	 *
	 *	@param	ViewFamily				View family to process
	 *	@param	VisibilityMap			Visibility map for the sub-views
	 *	@param	bOnlyOneView			If TRUE, then we don't need per-view buffers
	 *	@param	InVertexCount			Count of verts for this buffer
	 *	@param	InVertexSize			Stride of these verts, only used for verification
	 */
	void BuildViewFillDataAndSubmit(const FSceneViewFamily* ViewFamily,const DWORD VisibilityMap,UBOOL bOnlyOneView,INT InVertexCount,INT InVertexSize)
	{
		if (bAsyncTaskOutstanding)
		{
			check(AsyncTask);
			// this item was not rendered because of materials or other reasons, make sure it is done now
			AsyncTask->EnsureCompletion();
			bAsyncTaskOutstanding = FALSE;
		}
		if (!GHiPriThreadPoolNumThreads && !bOnlyOneView)
		{
			// @todo this wastes memory and cache space
			// we will have a separate buffer for each view 
			// even though they could all share
		}
		INT NumUsedViews = 0;
		for (INT ViewIndex = 0; ViewIndex < ViewFamily->Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1<<ViewIndex))
			{
				const FSceneView* View = ViewFamily->Views(ViewIndex);
				BuildViewFillData(NumUsedViews++,View,InVertexCount,InVertexSize);
				if (bOnlyOneView)
				{
					break;
				}
			}
		}
		if (AsyncBufferFillTasks.Num() > NumUsedViews)
		{
			AsyncBufferFillTasks.Remove(NumUsedViews,AsyncBufferFillTasks.Num() - NumUsedViews);
		}

		if (NumUsedViews)
		{
			if (!AsyncTask)
			{
				AsyncTask = FAsyncParticleFill::GetAsyncTask(this);
			}
			check(AsyncTask->GetTask().Parent == this); // this data structure has moved, leaving a stale pointer
			if (GIsGame == TRUE)
			{
				AsyncTask->StartHiPriorityTask();
				bAsyncTaskOutstanding = TRUE;
			}
			else
			{
				AsyncTask->StartSynchronousTask();
			}
		}
	}

	/**
	 *	Called to verify that a buffer is ready to use, blocks to wait and can sometimes execute the buffer fill on the current thread
	 *
	 *	@param	InView			View to look up in the buffer table
	 *  @return Completed buffers
	 */
	const FAsyncBufferFillData& EnsureFillCompletion(const FSceneView *InView)
	{
		check(AsyncBufferFillTasks.Num());
		if (AsyncTask)
		{
			SCOPE_CYCLE_COUNTER(STAT_ParticleAsyncWaitTime);
			AsyncTask->EnsureCompletion();
		}
		bAsyncTaskOutstanding = FALSE;
		// - 1 because we often fill only one, for _all_ views, if no match we always take the last one
		INT TaskIndex = 0;
		for (; TaskIndex < AsyncBufferFillTasks.Num() - 1; TaskIndex++) 
		{
			if (AsyncBufferFillTasks(TaskIndex).View == InView)
			{
				break;
			}
		}
		return AsyncBufferFillTasks(TaskIndex);
	}

	/** Async task that is queued in the hi priority pool */
	UBOOL									bAsyncTaskOutstanding;

	/** Async task that is queued in the hi priority pool */
	FAsyncTask<FAsyncParticleFill>*			AsyncTask;

	/** Array of buffers for filling by async task */
	TAsyncBufferFillTasks					AsyncBufferFillTasks;

	/** The material render proxies for this emitter */
	const FMaterialRenderProxy*	MaterialResource[2];
	/** TRUE if the particle emitter utilizes the DynamicParameter module */
	BITFIELD bUsesDynamicParameter:1;
	/** The position of the camera - used when rendering */
	FVector						CameraPosition;	
};

struct FDynamicSortableSpriteEmitterDataBase : public FDynamicSpriteEmitterDataBase
{
	FDynamicSortableSpriteEmitterDataBase(const UParticleModuleRequired* RequiredModule);

	virtual ~FDynamicSortableSpriteEmitterDataBase()
	{
	}

	/** Get the particle type for the RHI */
	virtual EParticleEmitterType GetParticleType() = 0;

	/**
	 *	Render thread only draw call
	 *
	 *	@param	Proxy		The scene proxy for the particle system that owns this emitter
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	View		The scene view being rendered
	 *	@param	DPGIndex	The depth priority group being rendered
	 */
	virtual INT Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex);

	/** 
	 *	Updates particle distance culling.
	 *
	 *	@param	ParticlePosition	The location of this particular particle
	 *	@param	NearCullDistanceSq	The near cull point
	 *	@param	NearFadeDistanceSq	The point to start fading for near culling
	 *	@param	FarCullDistanceSq	The far cull point
	 *	@param	FarFadeDistanceSq	The point to start fading for far culling
	 *	@param	OutParticleColor	The color of this particle
	 *	@param	OutSize				The size of this particle
	 */
	void UpdateParticleDistanceCulling
	( 
		const FVector& ParticlePosition, 
		const FLOAT NearCullDistanceSq, 
		const FLOAT NearFadeDistanceSq, 
		const FLOAT FarCullDistanceSq, 
		const FLOAT FarFadeDistanceSq, 
		FLinearColor &OutParticleColor, 
		FVector& OutSize 
	);

	/** The number of primitives to render for this emitter */
	INT PrimitiveCount;

	/** Flag to determine whether we are using near cull on this particle */
	BITFIELD					bEnableNearParticleCulling:1;

	/** Flag to determine whether we are using far cull on this particle */
	BITFIELD					bEnableFarParticleCulling:1;

	/* The near cull point for this particle */
	FLOAT						NearCullDistance;

	/* The distance where we should begin this particle when near culling */
	FLOAT						NearFadeDistance;

	/* The far cull point for this particle */
	FLOAT						FarFadeDistance;

	/* The distance where we should begin this particle when far culling */
	FLOAT						FarCullDistance;
};

/** Source data for Sprite emitters */
struct FDynamicSpriteEmitterReplayData
	: public FDynamicSpriteEmitterReplayDataBase
{
	// Nothing needed, yet


	/** Constructor */
	FDynamicSpriteEmitterReplayData()
	{
	}


	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		// Call parent implementation
		FDynamicSpriteEmitterReplayDataBase::Serialize( Ar );

		// ...
	}

};



/** Dynamic emitter data for sprite emitters */
struct FDynamicSpriteEmitterData : public FDynamicSortableSpriteEmitterDataBase
{
	FDynamicSpriteEmitterData(const UParticleModuleRequired* RequiredModule) :
		FDynamicSortableSpriteEmitterDataBase(RequiredModule),
		VertexFactory( NULL )
	{
	}

	virtual ~FDynamicSpriteEmitterData()
	{
		if (VertexFactory != NULL)
		{
			GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
			VertexFactory = NULL;
		}
	}

	/** Get the particle type for the RHI */
	virtual EParticleEmitterType GetParticleType()
	{
		return PET_Sprite;
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	void Init( UBOOL bInSelected );

	/**
	 *	Get the vertex stride for the dynamic rendering data
	 */
	virtual INT GetDynamicVertexStride() const
	{
		if (bUsesDynamicParameter == FALSE)
		{
			return sizeof(FParticleSpriteVertex);
		}
		else
		{
			return sizeof(FParticleSpriteVertexDynamicParameter);
		}
	}

	/**
	 *	Get the vertex factory to use when rendering
	 */
	virtual class FParticleVertexFactory* GetVertexFactory()
	{
		return VertexFactory;
	}

	/**
	 *	Get the source replay data for this emitter
	 */
	virtual const FDynamicSpriteEmitterReplayDataBase* GetSourceData() const
	{
		return &Source;
	}

	/**
	 *	Retrieve the vertex and (optional) index required to render this emitter.
	 *	Render-thread only
	 *
	 *	@param	VertexData		The memory to fill the vertex data into
	 *	@param	FillIndexData	The index data to fill in
	 *	@param	ParticleOrder	The (optional) particle ordering to use
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL GetVertexAndIndexData(void* VertexData, void* FillIndexData, FParticleOrder* ParticleOrder);

	/**
	 * Function to fill in the data when it's time to render
	 */
	UBOOL GetPointSpriteVertexData(void* VertexData, FParticleOrder* ParticleOrder);

	/**
	 *	Create the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Release the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const
	{
		return Source;
	}

	/** The frame source data for this particle system.  This is everything needed to represent this
	    this particle system frame.  It does not include any transient rendering thread data.  Also, for
		non-simulating 'replay' particle systems, this data may have come straight from disk! */
	FDynamicSpriteEmitterReplayData Source;

	/** The vertex factory used for rendering */
	FParticleVertexFactory*		VertexFactory;		// RENDER-THREAD USAGE ONLY!!!
};



/** Source data for SubUV emitters */
struct FDynamicSubUVEmitterReplayData
	: public FDynamicSpriteEmitterReplayDataBase
{
	INT								SubUVDataOffset;
	INT								SubImages_Horizontal;
	INT								SubImages_Vertical;
	UBOOL							bDirectUV;


	/** Constructor */
	FDynamicSubUVEmitterReplayData()
		: SubUVDataOffset( 0 ),
		  SubImages_Horizontal( 0 ),
		  SubImages_Vertical( 0 ),
		  bDirectUV( FALSE )
	{
	}


	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		// Call parent implementation
		FDynamicSpriteEmitterReplayDataBase::Serialize( Ar );

		Ar << SubUVDataOffset;
		Ar << SubImages_Horizontal;
		Ar << SubImages_Vertical;
		Ar << bDirectUV;
	}

};



/** Dynamic emitter data for SubUV emitters */
struct FDynamicSubUVEmitterData : public FDynamicSortableSpriteEmitterDataBase
{
	FDynamicSubUVEmitterData(const UParticleModuleRequired* RequiredModule) :
		FDynamicSortableSpriteEmitterDataBase(RequiredModule),
		VertexFactory( NULL )
	{
	}

	virtual ~FDynamicSubUVEmitterData()
	{
		if (VertexFactory != NULL)
		{
			GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
			VertexFactory = NULL;
		}
	}

	/** Get the particle type for the RHI */
	virtual EParticleEmitterType GetParticleType()
	{
		return PET_SubUV;
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	void Init( UBOOL bInSelected );

	/**
	 *	Get the vertex stride for the dynamic rendering data
	 */
	virtual INT GetDynamicVertexStride() const
	{
		if (bUsesDynamicParameter == FALSE)
		{
			return sizeof(FParticleSpriteSubUVVertex);
		}
		else
		{
			return sizeof(FParticleSpriteSubUVVertexDynamicParameter);
		}
	}

	/**
	 *	Get the vertex factory to use when rendering
	 */
	virtual class FParticleVertexFactory* GetVertexFactory()
	{
		return VertexFactory;
	}

	/**
	 *	Get the source replay data for this emitter
	 */
	virtual const FDynamicSpriteEmitterReplayDataBase* GetSourceData() const
	{
		return &Source;
	}

	/**
	 *	Retrieve the vertex and (optional) index required to render this emitter.
	 *	Render-thread only
	 *
	 *	@param	VertexData		The memory to fill the vertex data into
	 *	@param	FillIndexData	The index data to fill in
	 *	@param	ParticleOrder	The (optional) particle ordering to use
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL GetVertexAndIndexData(void* VertexData, void* FillIndexData, FParticleOrder* ParticleOrder);

	/**
	 *	Create the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Release the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 */
	virtual UBOOL ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const
	{
		return Source;
	}

	/** The frame source data for this particle system.  This is everything needed to represent this
	    this particle system frame.  It does not include any transient rendering thread data.  Also, for
		non-simulating 'replay' particle systems, this data may have come straight from disk! */
	FDynamicSubUVEmitterReplayData Source;


	/** The vertex factor used for rendering */
	FParticleSubUVVertexFactory*	VertexFactory;		// RENDER-THREAD USAGE ONLY!!!
};

/** */
class UStaticMesh;
class UMaterialInstanceConstant;

// The resource used to render a UMaterialInstanceConstant.
class FMeshEmitterMaterialInstanceResource : public FMaterialRenderProxy
{
public:
	FMeshEmitterMaterialInstanceResource() : 
	  FMaterialRenderProxy()
		  , Parent(NULL)
	  {
		  bCacheable = FALSE;
	  }

	  FMeshEmitterMaterialInstanceResource(FMaterialRenderProxy* InParent) : 
	  FMaterialRenderProxy()
		  , Parent(InParent)
	  {
		  bCacheable = FALSE;
	  }

	  virtual UBOOL GetVectorValue(const FName ParameterName,FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	  {
		  if (ParameterName == NAME_MeshEmitterVertexColor)
		  {
			  *OutValue = Param_MeshEmitterVertexColor;
			  return TRUE;
		  }
		  else
		  if (ParameterName == NAME_TextureOffsetParameter)
		  { 
			  *OutValue = Param_TextureOffsetParameter;
			  return TRUE;
		  }
		  else
		  if (ParameterName == NAME_TextureOffset1Parameter)
		  { 
			  *OutValue = Param_TextureOffset1Parameter;
			  return TRUE;
		  }
		  else
		  if (ParameterName == NAME_TextureScaleParameter)
		  {
			  *OutValue = Param_TextureScaleParameter;
			  return TRUE;
		  }
		  else
		  if (ParameterName == NAME_MeshEmitterDynamicParameter)
		  { 
			  *OutValue = Param_MeshEmitterDynamicParameter;
			  return TRUE;
		  }

		  if (Parent == NULL)
		  {
			  return FALSE;
		  }

		  return Parent->GetVectorValue(ParameterName, OutValue, Context);
	  }

	  UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	  {
		  return Parent->GetScalarValue(ParameterName, OutValue, Context);
	  }

	  UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	  {
		  return Parent->GetTextureValue(ParameterName, OutValue, Context);
	  }

	  virtual const FMaterial* GetMaterial() const
	  {
		  return Parent->GetMaterial();
	  }

#if WITH_MOBILE_RHI
	virtual FTexture* GetMobileTexture(const INT MobileTextureUnit) const
	{
		return Parent->GetMobileTexture(MobileTextureUnit);
	}

	virtual void FillMobileMaterialVertexParams(FMobileMaterialVertexParams& OutVertexParams) const 
	{
		Parent->FillMobileMaterialVertexParams( OutVertexParams );
		//force this on 
		OutVertexParams.bUseUniformColorMultiply = TRUE;
		OutVertexParams.UniformMultiplyColor = Param_MeshEmitterVertexColor;
	}

	virtual void FillMobileMaterialPixelParams(FMobileMaterialPixelParams& OutPixelParams) const
	{
		return Parent->FillMobileMaterialPixelParams( OutPixelParams );
	}
#endif

	  FMaterialRenderProxy* Parent;
	  FLinearColor Param_MeshEmitterVertexColor;
	  FLinearColor Param_TextureOffsetParameter;
	  FLinearColor Param_TextureOffset1Parameter;
	  FLinearColor Param_TextureScaleParameter;
	  FLinearColor Param_MeshEmitterDynamicParameter;
};

/** Source data for Mesh emitters */
struct FDynamicMeshEmitterReplayData
	: public FDynamicSpriteEmitterReplayDataBase
{
	INT					SubUVInterpMethod;
	INT					SubUVDataOffset;
	INT					SubImages_Horizontal;
	INT					SubImages_Vertical;
	UBOOL				bScaleUV;
	INT					MeshRotationOffset;
	BYTE				MeshAlignment;
	UBOOL				bMeshRotationActive;
	FVector				LockedAxis;	

	/** Constructor */
	FDynamicMeshEmitterReplayData() : 
		SubUVInterpMethod( 0 ),
		SubUVDataOffset( 0 ),
		SubImages_Horizontal( 0 ),
		SubImages_Vertical( 0 ),
		bScaleUV( FALSE ),
		MeshRotationOffset( 0 ),
		MeshAlignment( 0 ),
		bMeshRotationActive( FALSE ),
		LockedAxis(1.0f, 0.0f, 0.0f)
	{
	}


	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		// Call parent implementation
		FDynamicSpriteEmitterReplayDataBase::Serialize( Ar );

		Ar << SubUVInterpMethod;
		Ar << SubUVDataOffset;
		Ar << SubImages_Horizontal;
		Ar << SubImages_Vertical;
		Ar << bScaleUV;
		Ar << MeshRotationOffset;
		Ar << MeshAlignment;
		Ar << bMeshRotationActive;
		Ar << LockedAxis;
	}

};



/** Dynamic emitter data for Mesh emitters */
struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicMeshEmitterData(const UParticleModuleRequired* RequiredModule);

	virtual ~FDynamicMeshEmitterData();

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	void Init( UBOOL bInSelected,
			   const FParticleMeshEmitterInstance* InEmitterInstance,
			   UStaticMesh* InStaticMesh,
			   const UStaticMeshComponent* InStaticMeshComponent,
			   UBOOL UseNxFluid );

	/**
	 *	Create the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Release the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Render thread only draw call
	 *
	 *	@param	Proxy		The scene proxy for the particle system that owns this emitter
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	View		The scene view being rendered
	 *	@param	DPGIndex	The depth priority group being rendered
	 */
	virtual INT Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex);
	
	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	Proxy			The 'owner' particle system scene proxy
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber);

	/** Render using hardware instancing. */
	void RenderInstanced(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex);
	/** Debug rendering for NxFluid */
	void RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses);
	
	/** Initialized the vertex factory for a specific number of instances. */
	void InitInstancedResources(UINT NumInstances);

	/** Information used by the proxy about a single LOD of the mesh. */
	class FLODInfo
	{
	public:

		/** Information about an element of a LOD. */
		struct FElementInfo
		{
			UMaterialInterface* MaterialInterface;
		};
		TArray<FElementInfo> Elements;

		FLODInfo();
		FLODInfo(const UStaticMeshComponent* InStaticMeshComponent, const FParticleMeshEmitterInstance* MeshEmitInst, INT LODIndex, UBOOL bSelected);

		void Init(const UStaticMeshComponent* InStaticMeshComponent, const FParticleMeshEmitterInstance* MeshEmitInst, INT LODIndex, UBOOL bSelected);
	};
	
	struct FParticleInstancedMeshInstance
	{
		FVector Location;
		FVector XAxis;
		FVector YAxis;
		FVector ZAxis;
		FLinearColor Color;
	};
	
	class FParticleInstancedMeshInstanceBuffer : public FVertexBuffer
	{
	public:

		/** Initialization constructor. */
		FParticleInstancedMeshInstanceBuffer(const FDynamicMeshEmitterData& InRenderResources):
			RenderResources(InRenderResources)
		{}
		// FRenderResource interface.
		virtual void InitDynamicRHI();
		virtual void ReleaseDynamicRHI()
		{
			VertexBufferRHI.SafeRelease();
		}

		virtual FString GetFriendlyName() const { return TEXT("Instanced Particle Mesh Instances"); }

		void* CreateAndLockInstances(const UINT NumInstances);
		void UnlockInstances();

	private:
		const FDynamicMeshEmitterData &RenderResources;
	};

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const
	{
		return Source;
	}


	/** The frame source data for this particle system.  This is everything needed to represent this
	    this particle system frame.  It does not include any transient rendering thread data.  Also, for
		non-simulating 'replay' particle systems, this data may have come straight from disk! */
	FDynamicMeshEmitterReplayData Source;


	INT					LastFramePreRendered;

	UStaticMesh*		StaticMesh;
	FLODInfo			LODInfo;
	
	/** The instanced rendering supporting material to use on the particles. */
	UMaterialInterface                        *InstancedMaterialInterface;
	
	/** The vertex buffer used to hold the instance data. */
	FParticleInstancedMeshInstanceBuffer      *InstanceBuffer;
	
	/** The vertex factory used to render the instanced meshes. */
	class FParticleInstancedMeshVertexFactory *InstancedVertexFactory;
	/** True if motion blur data is used */
	BITFIELD bUseMotionBlurData:1;
	/** True if motion blur data should be updated for next frame. Used for pausing motion blur */
	BITFIELD bShouldUpdateMBTransforms:1;
	
	/** Particle instance data, filled by game thread, copied to VB by render thread. */
	FParticleInstancedMeshInstance            *PhysXParticleBuf;	

	TArray<FMeshEmitterMaterialInstanceResource> MEMatInstRes[2];

	/** offset to FMeshTypeDataPayload */
	UINT MeshTypeDataOffset;
	/** ptr based id of owner emitter instance. Only used for map lookup and not for dereferencing */
	const FParticleMeshEmitterInstance*	EmitterInstanceId;	

	// 'orientation' items...
	// These don't need to go into the replay data, as they are constant over the life of the emitter
	/** If TRUE, apply the 'pre-rotation' values to the mesh. */
	BITFIELD bApplyPreRotation:1;
	/** The pitch/roll/yaw to apply in the pre-rotation step */
	FVector RollPitchYaw;
	/** If TRUE, then use the locked axis setting supplied. Trumps locked axis module and/or TypeSpecific mesh settings. */
	BITFIELD bUseMeshLockedAxis:1;
	/** If TRUE, then use the camera facing options supplied. Trumps all other settings. */
	BITFIELD bUseCameraFacing:1;
	/** 
	 *	If TRUE, apply 'sprite' particle rotation about the orientation axis (direction mesh is pointing).
	 *	If FALSE, apply 'sprite' particle rotation about the camera facing axis.
	 */
	BITFIELD bApplyParticleRotationAsSpin:1;
	/** The EMeshCameraFacingOption setting to use if bUseCameraFacing is TRUE. */
	BYTE CameraFacingOption;
	/** Z-offset for instanced PhysX mesh particles */
	FLOAT ZOffset;
};

/** Source data for Beam emitters */
struct FDynamicBeam2EmitterReplayData
	: public FDynamicSpriteEmitterReplayDataBase
{
	INT									VertexCount;
	INT									IndexCount;
	INT									IndexStride;

	TArray<INT>							TrianglesPerSheet;
	INT									UpVectorStepSize;

	// Offsets to particle data
	INT									BeamDataOffset;
	INT									InterpolatedPointsOffset;
	INT									NoiseRateOffset;
	INT									NoiseDeltaTimeOffset;
	INT									TargetNoisePointsOffset;
	INT									NextNoisePointsOffset;
	INT									TaperValuesOffset;
	INT									NoiseDistanceScaleOffset;

	UBOOL								bLowFreqNoise_Enabled;
	UBOOL								bHighFreqNoise_Enabled;
	UBOOL								bSmoothNoise_Enabled;
	UBOOL								bUseSource;
	UBOOL								bUseTarget;
	UBOOL								bTargetNoise;
	INT									Sheets;
	INT									Frequency;
	INT									NoiseTessellation;
	FLOAT								NoiseRangeScale;
	FLOAT								NoiseTangentStrength;
	FVector								NoiseSpeed;
	FLOAT								NoiseLockTime;
	FLOAT								NoiseLockRadius;
	FLOAT								NoiseTension;

	INT									TextureTile;
	FLOAT								TextureTileDistance;
	BYTE								TaperMethod;
	INT									InterpolationPoints;

	/** Debugging rendering flags												*/
	UBOOL								bRenderGeometry;
	UBOOL								bRenderDirectLine;
	UBOOL								bRenderLines;
	UBOOL								bRenderTessellation;

	/** Constructor */
	FDynamicBeam2EmitterReplayData()
		: VertexCount(0)
		, IndexCount(0)
		, IndexStride(0)
		, TrianglesPerSheet()
		, UpVectorStepSize(0)
		, BeamDataOffset(-1)
		, InterpolatedPointsOffset(-1)
		, NoiseRateOffset(-1)
		, NoiseDeltaTimeOffset(-1)
		, TargetNoisePointsOffset(-1)
		, NextNoisePointsOffset(-1)
		, TaperValuesOffset(-1)
		, NoiseDistanceScaleOffset(-1)
		, bLowFreqNoise_Enabled( FALSE )
		, bHighFreqNoise_Enabled( FALSE )
		, bSmoothNoise_Enabled( FALSE )
		, bUseSource( FALSE )
		, bUseTarget( FALSE )
		, bTargetNoise( FALSE )
		, Sheets(1)
		, Frequency(1)
		, NoiseTessellation(1)
		, NoiseRangeScale(1)
		, NoiseTangentStrength( 0.0f )
		, NoiseSpeed( 0.0f, 0.0f, 0.0f )
		, NoiseLockTime( 0.0f )
		, NoiseLockRadius( 0.0f )
		, NoiseTension( 0.0f )
		, TextureTile(0)
		, TextureTileDistance(0)
		, TaperMethod(0)
		, InterpolationPoints(0)
		, bRenderGeometry(TRUE)
		, bRenderDirectLine(FALSE)
		, bRenderLines(FALSE)
		, bRenderTessellation(FALSE)
	{
	}


	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		// Call parent implementation
		FDynamicSpriteEmitterReplayDataBase::Serialize( Ar );

		Ar << VertexCount;
		Ar << IndexCount;
		Ar << IndexStride;

		Ar << TrianglesPerSheet;
		Ar << UpVectorStepSize;
		Ar << BeamDataOffset;
		Ar << InterpolatedPointsOffset;
		Ar << NoiseRateOffset;
		Ar << NoiseDeltaTimeOffset;
		Ar << TargetNoisePointsOffset;
		Ar << NextNoisePointsOffset;
		Ar << TaperValuesOffset;
		Ar << NoiseDistanceScaleOffset;

		Ar << bLowFreqNoise_Enabled;
		Ar << bHighFreqNoise_Enabled;
		Ar << bSmoothNoise_Enabled;
		Ar << bUseSource;
		Ar << bUseTarget;
		Ar << bTargetNoise;
		Ar << Sheets;
		Ar << Frequency;
		Ar << NoiseTessellation;
		Ar << NoiseRangeScale;
		Ar << NoiseTangentStrength;
		Ar << NoiseSpeed;
		Ar << NoiseLockTime;
		Ar << NoiseLockRadius;
		Ar << NoiseTension;

		Ar << TextureTile;
		Ar << TextureTileDistance;
		Ar << TaperMethod;
		Ar << InterpolationPoints;

		Ar << bRenderGeometry;
		Ar << bRenderDirectLine;
		Ar << bRenderLines;
		Ar << bRenderTessellation;
	}

};



/** Dynamic emitter data for Beam emitters */
struct FDynamicBeam2EmitterData : public FDynamicSpriteEmitterDataBase 
{
	static const UINT MaxBeams = 2 * 1024;
	static const UINT MaxInterpolationPoints = 250;
	static const UINT MaxNoiseFrequency = 250;

	FDynamicBeam2EmitterData(const UParticleModuleRequired* RequiredModule)
		: 
		  FDynamicSpriteEmitterDataBase(RequiredModule)
		, VertexFactory(NULL)
		, LastFramePreRendered(-1)
	{
	}

	virtual ~FDynamicBeam2EmitterData()
	{
		if (VertexFactory != NULL)
		{
			GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
			VertexFactory = NULL;
		}
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	void Init( UBOOL bInSelected );

	/**
	 *	Create the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Release the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Render thread only draw call
	 *
	 *	@param	Proxy		The scene proxy for the particle system that owns this emitter
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	View		The scene view being rendered
	 *	@param	DPGIndex	The depth priority group being rendered
	 */
	virtual INT Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex);

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	Proxy			The 'owner' particle system scene proxy
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber);

	// Debugging functions
	virtual void RenderDirectLine(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex);
	virtual void RenderLines(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex);

	virtual void RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses);

	// Data fill functions
	INT FillIndexData(struct FAsyncBufferFillData& Data);
	INT FillVertexData_NoNoise(struct FAsyncBufferFillData& Data);
	INT FillData_Noise(struct FAsyncBufferFillData& Data);
	INT FillData_InterpolatedNoise(struct FAsyncBufferFillData& Data);

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const
	{
		return Source;
	}

	/** Perform the actual work of filling the buffer, often called from another thread */
	virtual void DoBufferFill(FAsyncBufferFillData& Me);


	/** The frame source data for this particle system.  This is everything needed to represent this
	    this particle system frame.  It does not include any transient rendering thread data.  Also, for
		non-simulating 'replay' particle systems, this data may have come straight from disk! */
	FDynamicBeam2EmitterReplayData Source;

	FParticleBeamTrailVertexFactory*	VertexFactory;		// RENDER-THREAD USAGE ONLY!!!

	INT									LastFramePreRendered;
};

/** Source data for Trail emitters */
struct FDynamicTrail2EmitterReplayData
	: public FDynamicSpriteEmitterReplayDataBase
{
	INT									PrimitiveCount;
	INT									VertexCount;
	INT									IndexCount;
	INT									IndexStride;

	// Payload offsets
	INT									TrailDataOffset;
	INT									TaperValuesOffset;
	INT									ParticleSourceOffset;

	INT									TrailCount;
	INT									Sheets;
	INT									TessFactor;
	INT									TessStrength;
	FLOAT								TessFactorDistance;

    TArray<FLOAT>						TrailSpawnTimes;
    TArray<FVector>						SourcePosition;
    TArray<FVector>						LastSourcePosition;
    TArray<FVector>						CurrentSourcePosition;
    TArray<FVector>						LastSpawnPosition;
    TArray<FVector>						LastSpawnTangent;
    TArray<FLOAT>						SourceDistanceTravelled;
    TArray<FVector>						SourceOffsets;


	/** Constructor */
	FDynamicTrail2EmitterReplayData()
		: PrimitiveCount(0)
		, VertexCount(0)
		, IndexCount(0)
		, IndexStride(0)
		, TrailDataOffset(-1)
		, TaperValuesOffset(-1)
		, ParticleSourceOffset(-1)
		, TrailCount(1)
		, Sheets(1)
		, TessFactor(1)
		, TessStrength(1)
		, TessFactorDistance(0.0f)
		, TrailSpawnTimes()
		, SourcePosition()
		, LastSourcePosition()
		, CurrentSourcePosition()
		, LastSpawnPosition()
		, LastSpawnTangent()
		, SourceDistanceTravelled()
		, SourceOffsets()
	{
	}


	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		// Call parent implementation
		FDynamicSpriteEmitterReplayDataBase::Serialize( Ar );

		Ar << PrimitiveCount;
		Ar << VertexCount;
		Ar << IndexCount;
		Ar << IndexStride;

		Ar << TrailDataOffset;
		Ar << TaperValuesOffset;
		Ar << ParticleSourceOffset;

		Ar << TrailCount;
		Ar << Sheets;
		Ar << TessFactor;
		Ar << TessStrength;
		Ar << TessFactorDistance;

		Ar << TrailSpawnTimes;
		Ar << SourcePosition;
		Ar << LastSourcePosition;
		Ar << CurrentSourcePosition;
		Ar << LastSpawnPosition;
		Ar << LastSpawnTangent;
		Ar << SourceDistanceTravelled;
		Ar << SourceOffsets;
	}

};

/** Dynamic emitter data for Trail emitters */
struct FDynamicTrail2EmitterData : public FDynamicSpriteEmitterDataBase 
{
	FDynamicTrail2EmitterData(const UParticleModuleRequired* RequiredModule)
		: 
		  FDynamicSpriteEmitterDataBase(RequiredModule)
		, VertexFactory(NULL)
		, LastFramePreRendered(-1)
		, bClipSourceSegement(FALSE)
	{
	}

	virtual ~FDynamicTrail2EmitterData()
	{
		if (VertexFactory != NULL)
		{
			GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
			VertexFactory = NULL;
		}
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	void Init( UBOOL bInSelected );

	/**
	 *	Create the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Release the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Render thread only draw call
	 *
	 *	@param	Proxy		The scene proxy for the particle system that owns this emitter
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	View		The scene view being rendered
	 *	@param	DPGIndex	The depth priority group being rendered
	 */
	virtual INT Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex);

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	Proxy			The 'owner' particle system scene proxy
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber);

	virtual void RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses);

	// Data fill functions
	INT FillIndexData(struct FAsyncBufferFillData& Data);
	INT FillVertexData(struct FAsyncBufferFillData& Data);

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const
	{
		return Source;
	}

	/** Perform the actual work of filling the buffer, often called from another thread 
	* @param Me Fill data structure
	*/
	virtual void DoBufferFill(FAsyncBufferFillData& Me);

	/** The frame source data for this particle system.  This is everything needed to represent this
	    this particle system frame.  It does not include any transient rendering thread data.  Also, for
		non-simulating 'replay' particle systems, this data may have come straight from disk! */
	FDynamicTrail2EmitterReplayData Source;

	/**	The sprite particle data.										*/
	FParticleBeamTrailVertexFactory*	VertexFactory;		// RENDER-THREAD USAGE ONLY!!!
	INT									LastFramePreRendered;
	UBOOL 								bClipSourceSegement; // RENDER-THREAD USAGE ONLY!!!

};

/** Source data for trail-type emitters */
struct FDynamicTrailsEmitterReplayData : public FDynamicSpriteEmitterReplayDataBase
{
	INT					PrimitiveCount;
	INT					VertexCount;
	INT					IndexCount;
	INT					IndexStride;

	// Payload offsets
	INT					TrailDataOffset;

	INT					MaxActiveParticleCount;
	INT					TrailCount;
	INT					Sheets;

	/** Constructor */
	FDynamicTrailsEmitterReplayData()
		: PrimitiveCount(0)
		, VertexCount(0)
		, IndexCount(0)
		, IndexStride(0)
		, TrailDataOffset(-1)
		, MaxActiveParticleCount(0)
		, TrailCount(1)
		, Sheets(1)
	{
	}

	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		// Call parent implementation
		FDynamicSpriteEmitterReplayDataBase::Serialize( Ar );

		Ar << PrimitiveCount;
		Ar << VertexCount;
		Ar << IndexCount;
		Ar << IndexStride;

		Ar << TrailDataOffset;

		Ar << MaxActiveParticleCount;
		Ar << TrailCount;
		Ar << Sheets;
	}
};

/** Source data for Ribbon emitters */
struct FDynamicRibbonEmitterReplayData : public FDynamicTrailsEmitterReplayData
{
	// Payload offsets
	INT MaxTessellationBetweenParticles;

	/** Constructor */
	FDynamicRibbonEmitterReplayData()
		: FDynamicTrailsEmitterReplayData()
		, MaxTessellationBetweenParticles(0)
	{
	}

	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		// Call parent implementation
		FDynamicTrailsEmitterReplayData::Serialize( Ar );
		Ar << MaxTessellationBetweenParticles;
	}
};

/** Dynamic emitter data for Ribbon emitters */
struct FDynamicTrailsEmitterData : public FDynamicSpriteEmitterDataBase 
{
	FDynamicTrailsEmitterData(const UParticleModuleRequired* RequiredModule) : 
		  FDynamicSpriteEmitterDataBase(RequiredModule)
		, SourcePointer(NULL)
		, VertexFactory(NULL)
		, LastFramePreRendered(-1)
		, bClipSourceSegement(FALSE)
		, bRenderGeometry(TRUE)
		, bRenderParticles(FALSE)
		, bRenderTangents(FALSE)
		, bRenderTessellation(FALSE)
		, bTextureTileDistance(FALSE)
	    , DistanceTessellationStepSize(12.5f)
		, TangentTessellationScalar(25.0f)
		, TextureTileDistance(0.0f)
	{
	}

	virtual ~FDynamicTrailsEmitterData()
	{
		if (VertexFactory != NULL)
		{
			GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
			VertexFactory = NULL;
		}
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	virtual void Init(UBOOL bInSelected);

	/**
	 *	Create the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Release the render thread resources for this emitter data
	 *
	 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE if failed
	 */
	virtual UBOOL ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy);

	/**
	 *	Render thread only draw call
	 *
	 *	@param	Proxy		The scene proxy for the particle system that owns this emitter
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	View		The scene view being rendered
	 *	@param	DPGIndex	The depth priority group being rendered
	 */
	virtual INT Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex);

	virtual UBOOL ShouldUsePrerenderView()
	{
		return TRUE;
	}

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	Proxy			The 'owner' particle system scene proxy
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber);

	virtual void RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses);

	// Data fill functions
	virtual INT FillIndexData(struct FAsyncBufferFillData& Data);
	virtual INT FillVertexData(struct FAsyncBufferFillData& Data);

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const
	{
		check(SourcePointer);
		return *SourcePointer;
	}

	virtual void DoBufferFill(FAsyncBufferFillData& Me);

	FDynamicTrailsEmitterReplayData*	SourcePointer;
	/**	The sprite particle data.										*/
	FParticleBeamTrailVertexFactory*	VertexFactory;		
	INT									LastFramePreRendered;

	BITFIELD	bClipSourceSegement:1;
	BITFIELD	bRenderGeometry:1;
	BITFIELD	bRenderParticles:1;
	BITFIELD	bRenderTangents:1;
	BITFIELD	bRenderTessellation:1;
	BITFIELD	bTextureTileDistance:1;

    FLOAT DistanceTessellationStepSize;
    FLOAT TangentTessellationScalar;
	FLOAT TextureTileDistance;
};

/** Dynamic emitter data for Ribbon emitters */
struct FDynamicRibbonEmitterData : public FDynamicTrailsEmitterData
{
	FDynamicRibbonEmitterData(const UParticleModuleRequired* RequiredModule) : 
		  FDynamicTrailsEmitterData(RequiredModule)
	{
	}

	virtual ~FDynamicRibbonEmitterData()
	{
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	virtual void Init(UBOOL bInSelected);

	virtual UBOOL ShouldUsePrerenderView();
	virtual void RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses);

	// Data fill functions
	virtual INT FillVertexData(struct FAsyncBufferFillData& Data);

	/** 
	 *	The frame source data for this particle system.  This is everything needed to represent this
	 *	this particle system frame.  It does not include any transient rendering thread data.  Also, for
	 *	non-simulating 'replay' particle systems, this data may have come straight from disk!
	 */
	FDynamicRibbonEmitterReplayData Source;

	/**	The sprite particle data.										*/
	BITFIELD	RenderAxisOption:2;
};

/** Dynamic emitter data for AnimTrail emitters */
struct FDynamicAnimTrailEmitterData : public FDynamicTrailsEmitterData
{
	FDynamicAnimTrailEmitterData(const UParticleModuleRequired* RequiredModule) : 
		  FDynamicTrailsEmitterData(RequiredModule)
	{
	}

	virtual ~FDynamicAnimTrailEmitterData()
	{
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	virtual void Init(UBOOL bInSelected);

	virtual void RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses);

	// Data fill functions
	virtual INT FillVertexData(struct FAsyncBufferFillData& Data);

	/** 
	 *	The frame source data for this particle system.  This is everything needed to represent this
	 *	this particle system frame.  It does not include any transient rendering thread data.  Also, for
	 *	non-simulating 'replay' particle systems, this data may have come straight from disk!
	 */
	FDynamicTrailsEmitterReplayData Source;
	/** The time step the animation data was sampled at. */
	FLOAT AnimSampleTimeStep;
};

/*-----------------------------------------------------------------------------
 *	Particle dynamic data
 *	This is a copy of the particle system data needed to render the system in
 *	another thread.
 ----------------------------------------------------------------------------*/
class FParticleDynamicData
{
public:
	FParticleDynamicData()
		: DynamicEmitterDataArray()
		, bNeedsLODDistanceUpdate(FALSE)
	{
	}

	virtual ~FParticleDynamicData()
	{		
		ClearEmitterDataArray();
	}

	void ClearEmitterDataArray()
	{
		for (INT Index = 0; Index < DynamicEmitterDataArray.Num(); Index++)
		{
			FDynamicEmitterDataBase* Data =	DynamicEmitterDataArray(Index);
			Data->WaitOnAsyncTasks();
			delete Data;
			DynamicEmitterDataArray(Index) = NULL;
		}
		DynamicEmitterDataArray.Empty();
	}

	/**
	 *	Called when a dynamic data is being deleted when double-buffering.
	 *	This function is responsible for releasing all non-render resource data.
	 *	The complete deletion must be deferred to ensure the render thread is not
	 *	utilizing any of the resources.
	 */
	void CleanupDataForDeferredDeletion()
	{
		for (INT Index = 0; Index < DynamicEmitterDataArray.Num(); Index++)
		{
			FDynamicEmitterDataBase* Data =	DynamicEmitterDataArray(Index);
			if (Data)
			{
				Data->ReleaseResource();
			}
		}
	}

	DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + DynamicEmitterDataArray.GetAllocatedSize() ); }

	// Variables
	TArray<FDynamicEmitterDataBase*>	DynamicEmitterDataArray;
	UBOOL bNeedsLODDistanceUpdate;
	volatile INT RenderFlag;
	/** The Current Emmitter we are rendering **/
	UINT EmitterIndex;                  

	/** World space position that UVs generated with the ParticleMacroUV material node will be centered on. */
	FVector SystemPositionForMacroUVs;

	/** World space radius that UVs generated with the ParticleMacroUV material node will tile based on. */
	FLOAT SystemRadiusForMacroUVs;
};

//
//	Scene Proxies
//

class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** Initialization constructor. */
	FParticleSystemSceneProxy(const UParticleSystemComponent* Component);
	virtual ~FParticleSystemSceneProxy();

	// FPrimitiveSceneProxy interface.
	/** @return TRUE if the proxy requires occlusion queries */
	virtual UBOOL RequiresOcclusion(const FSceneView* View) const
	{
		return FALSE;
	}
	
	/** 
	 * Draw the scene proxy as a dynamic element
	 *
	 * @param	PDI - draw interface to render to
	 * @param	View - current view
	 * @param	DPGIndex - current depth priority 
	 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Helper function for determining the LOD distance for a given view.
	 *
	 *	@param	View			The view of interest.
	 *	@param	FrameNumber		The frame number being rendered.
	 */
	void ProcessPreRenderView(const FSceneView* View, INT FrameNumber);

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber);

	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 */
	virtual UBOOL CreateRenderThreadResources();

	/**
	 *	Called when the rendering thread removes the dynamic data from the scene.
	 */
	virtual UBOOL ReleaseRenderThreadResources();

	void UpdateData(FParticleDynamicData* NewDynamicData);
	void UpdateData_RenderThread(FParticleDynamicData* NewDynamicData);
	void UpdateViewRelevance(FMaterialViewRelevance& NewViewRelevance);
	void UpdateViewRelevance_RenderThread(FMaterialViewRelevance& NewViewRelevance);

	FParticleDynamicData* GetDynamicData()
	{
		return DynamicData;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const 
	{ 
		DWORD AdditionalSize = FPrimitiveSceneProxy::GetAllocatedSize();

		return( AdditionalSize ); 
	}

	void DetermineLODDistance(const FSceneView* View, INT FrameNumber);

	/** Object position in post projection space. */
	virtual void GetObjectPositionAndScale(const FSceneView& View, FVector& ObjectPostProjectionPosition, FVector& ObjectNDCPosition, FVector4& ObjectMacroUVScales) const;

	// While this isn't good OO design, access to everything is made public.
	// This is to allow custom emitter instances to easily be written when extending the engine.

	FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const	{	return PrimitiveSceneInfo;	}

	const FMatrix& GetLocalToWorld() const		{	return LocalToWorld;			}
	FMatrix& GetLocalToWorld()			{	return LocalToWorld;			}
	FMatrix GetWorldToLocal() const		{	return LocalToWorld.Inverse();	}
	FLOAT GetLocalToWorldDeterminant()	{	return LocalToWorldDeterminant;	}
	AActor* GetOwner()					{	return Owner;					}
	FLOAT GetCullDistance()				{	return CullDistance;			}
	UBOOL GetCastShadow()				{	return bCastShadow;				}
	const FMaterialViewRelevance& GetMaterialViewRelevance() const
	{
		return MaterialViewRelevance;
	}
	FLOAT GetPendingLODDistance()		{	return PendingLODDistance;		}
	FVector GetLODOrigin()				{	return LODOrigin;				}
	UBOOL GetNearClippingPlane(FPlane& OutNearClippingPlane) const;
	INT GetLODMethod()					{	return LODMethod;				}

	FColoredMaterialRenderProxy* GetSelectedWireframeMatInst()		{	return &SelectedWireframeMaterialInstance;		}
	FColoredMaterialRenderProxy* GetDeselectedWireframeMatInst()	{	return &DeselectedWireframeMaterialInstance;	}

	void GetAxisLockValues(FDynamicSpriteEmitterDataBase* DynamicData, UBOOL bUseLocalSpace, FVector& CameraUp, FVector& CameraRight);

protected:
	AActor* Owner;

	FLOAT CullDistance;
#if STATS
	DOUBLE LastStatCaptureTime;
	UBOOL bCountedThisFrame;
#endif

	BITFIELD bCastShadow : 1;
	
	FMaterialViewRelevance MaterialViewRelevance;

	FParticleDynamicData* DynamicData;			// RENDER THREAD USAGE ONLY

	FColoredMaterialRenderProxy SelectedWireframeMaterialInstance;
	FColoredMaterialRenderProxy DeselectedWireframeMaterialInstance;

	INT LODMethod;
	FLOAT PendingLODDistance;

	FVector LODOrigin;
	UBOOL LODHasNearClippingPlane;
	FPlane LODNearClippingPlane;

	INT LastFramePreRendered;

	friend struct FDynamicSpriteEmitterDataBase;
};

class FParticleSystemOcclusionSceneProxy : public FParticleSystemSceneProxy, public FPrimitiveSceneProxyOcclusionTracker
{
public:
	/** Initialization constructor. */
	FParticleSystemOcclusionSceneProxy(const UParticleSystemComponent* Component);
	virtual ~FParticleSystemOcclusionSceneProxy();

	// FPrimitiveSceneProxy interface.
	/** @return TRUE if the proxy requires occlusion queries */
	virtual UBOOL RequiresOcclusion(const FSceneView* View) const
	{
		return TRUE;
	}
	
	/** 
	 * Draw the scene proxy as a dynamic element
	 *
	 * @param	PDI - draw interface to render to
	 * @param	View - current view
	 * @param	DPGIndex - current depth priority 
	 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	/**
	 *	Returns whether the proxy utilizes custom occlusion bounds or not
	 *
	 *	@return	UBOOL		TRUE if custom occlusion bounds are used, FALSE if not;
	 */
	virtual UBOOL HasCustomOcclusionBounds() const
	{
		return bHasCustomOcclusionBounds;
	}

	// This MUST be overriden
	virtual FLOAT GetOcclusionPercentage(const FSceneView& View) const
	{
		if (View.Family->ShowFlags & SHOW_Game)
		{
			FSceneViewState* State = (FSceneViewState*)(View.State);
			if (State != NULL)
			{
				const FCoverageInfo* Coverage = CoverageMap.Find(State);
				if (Coverage != NULL)
				{
					return Coverage->Percentage;
				}
			}
			return 1.0f;
		}
		return CoveragePercentage;
	}

	/**
	 *	Return the custom occlusion bounds for this scene proxy.
	 *	
	 *	@return	FBoxSphereBounds		The custom occlusion bounds.
	 */
	virtual FBoxSphereBounds GetCustomOcclusionBounds() const
	{
		return OcclusionBounds.TransformBy(LocalToWorld);
	}

	BITFIELD	bHasCustomOcclusionBounds : 1;
};

/*-----------------------------------------------------------------------------
 *	ParticleDataManager
 *	Handles the collection of ParticleSystemComponents that are to be 
 *	submitted to the rendering thread for display.
 ----------------------------------------------------------------------------*/
struct FParticleDataManager
{
protected:
	/** The particle system components that need to be sent to the rendering thread */
	TMap<UParticleSystemComponent*, UBOOL>	PSysComponents;

public:
	/**
	 *	Update the dynamic data for all particle system componets
	 */
	virtual void UpdateDynamicData();
	
	/**
	 *	Add a particle system component to the list.
	 *
	 *	@param		InPSysComp		The particle system component to add.
	 */
	void AddParticleSystemComponent(UParticleSystemComponent* InPSysComp);

	/**
	 *	Remove a particle system component to the list.
	 *
	 *	@param		InPSysComp		The particle system component to remove.
	 */
	void RemoveParticleSystemComponent(UParticleSystemComponent* InPSysComp);

	/**
	 *	Return TRUE if the data manager has a copy of the given particle system component.
	 *
	 *	@param	InPSysComp		The particle system component to look for.
	 *
	 *	@return	UBOOL			TRUE if the PSysComp is in the data manager, FALSE if not
	 */
	UBOOL HasParticleSystemComponent(UParticleSystemComponent* InPSysComp);

	/**
	 *	Clear all pending components from the queue.
	 */
	void Clear();

#if STATS
	static DWORD DynamicPSysCompCount;
	static DWORD DynamicPSysCompMem;
	static DWORD DynamicEmitterCount;
	static DWORD DynamicEmitterMem;
	static DWORD TotalGTParticleData;
	static DWORD TotalRTParticleData;

	static DWORD DynamicSpriteCount;
	static DWORD DynamicSubUVCount;
	static DWORD DynamicMeshCount;
	static DWORD DynamicBeamCount;
	static DWORD DynamicTrailCount;
	static DWORD DynamicRibbonCount;
	static DWORD DynamicAnimTrailCount;

	static DWORD DynamicSpriteGTMem;
	static DWORD DynamicSubUVGTMem;
	static DWORD DynamicMeshGTMem;
	static DWORD DynamicBeamGTMem;
	static DWORD DynamicTrailGTMem;
	static DWORD DynamicRibbonGTMem;
	static DWORD DynamicAnimTrailGTMem;
	static DWORD DynamicUntrackedGTMem;

	static DWORD DynamicPSysCompCount_MAX;
	static DWORD DynamicPSysCompMem_MAX;
	static DWORD DynamicEmitterCount_MAX;
	static DWORD DynamicEmitterMem_MAX;
	static DWORD DynamicEmitterGTMem_Waste_MAX;
	static DWORD DynamicEmitterGTMem_Largest_MAX;
	static DWORD TotalGTParticleData_MAX;
	static DWORD TotalRTParticleData_MAX;
	static DWORD LargestRTParticleData_MAX;

	static DWORD DynamicSpriteCount_MAX;
	static DWORD DynamicSubUVCount_MAX;
	static DWORD DynamicMeshCount_MAX;
	static DWORD DynamicBeamCount_MAX;
	static DWORD DynamicTrailCount_MAX;
	static DWORD DynamicRibbonCount_MAX;
	static DWORD DynamicAnimTrailCount_MAX;

	static DWORD DynamicSpriteGTMem_MAX;
	static DWORD DynamicSubUVGTMem_MAX;
	static DWORD DynamicMeshGTMem_MAX;
	static DWORD DynamicBeamGTMem_MAX;
	static DWORD DynamicTrailGTMem_MAX;
	static DWORD DynamicRibbonGTMem_MAX;
	static DWORD DynamicAnimTrailGTMem_MAX;
	static DWORD DynamicUntrackedGTMem_MAX;

	static void ResetParticleMemoryMaxValues();

	static void DumpParticleMemoryStats(FOutputDevice& Ar);
#endif
};

#if WITH_APEX_PARTICLES

class FIApexScene;

struct FDynamicApexEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicApexEmitterReplayDataBase(void)
	{
		eEmitterType = DET_Apex;
	}
};

class FDynamicApexEmitterDataBase : public FDynamicEmitterDataBase
{
public:
	FDynamicApexEmitterDataBase(const UParticleModuleRequired *required,FIApexScene *apexScene,class FParticleApexEmitterInstance *emitterInstance);
	~FDynamicApexEmitterDataBase(void);

	/**
	 *	Render thread only draw call
	 *
	 *	@param	Proxy		The scene proxy for the particle system that owns this emitter
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	View		The scene view being rendered
	 *	@param	DPGIndex	The depth priority group being rendered
	 */
	virtual INT Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex);

	/**
	 *	Retrieve the material render proxy to use for rendering this emitter. PURE VIRTUAL
	 *
	 *	@param	bSelected				Whether the object is selected
	 *
	 *	@return	FMaterialRenderProxy*	The material proxt to render with.
	 */
	virtual const FMaterialRenderProxy* GetMaterialRenderProxy(UBOOL bSelected);

	/**
	 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
	 *  Only called for primitives that are visible and have bDynamicRelevance
 	 *
	 *	@param	Proxy			The 'owner' particle system scene proxy
	 *	@param	ViewFamily		The ViewFamily to pre-render for
	 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
	 *	@param	FrameNumber		The frame number of this pre-render
	 */
	virtual void PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const TBitArray<FDefaultBitArrayAllocator>& VisibilityMap, INT FrameNumber);

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const;

	/** Release the resource for the data */
	virtual void ReleaseResource();

private:
	FDynamicApexEmitterReplayDataBase	ReplaySource;
	class FIApexScene				*ApexScene;
	FParticleApexEmitterInstance	*EmitterInstance;
};

struct FDynamicApexEmitterReplayData
	: public FDynamicApexEmitterReplayDataBase
{
	// Nothing needed, yet


	/** Constructor */
	FDynamicApexEmitterReplayData()
	{
	}


	/** Serialization */
	virtual void Serialize( FArchive& Ar )
	{
		// Call parent implementation
		FDynamicApexEmitterReplayDataBase::Serialize( Ar );

		// ...
	}

};

struct FDynamicApexEmitterData : public FDynamicApexEmitterDataBase
{
	FDynamicApexEmitterData(const UParticleModuleRequired* RequiredModule,FIApexScene *apexScene,class FParticleApexEmitterInstance *emitterInstance) :
		FDynamicApexEmitterDataBase(RequiredModule,apexScene,emitterInstance)
	{
	}

	virtual ~FDynamicApexEmitterData()
	{
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
	void Init( UBOOL bInSelected )
	{
	}

	/**
	 *	Render thread only draw call
	 *
	 *	@param	Proxy		The scene proxy for the particle system that owns this emitter
	 *	@param	PDI			The primitive draw interface to render with
	 *	@param	View		The scene view being rendered
	 *	@param	DPGIndex	The draw primitive group being rendered
	 */
	virtual INT Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex)
	{
		return FDynamicApexEmitterDataBase::Render(Proxy,PDI,View,DPGIndex);
	}

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const
	{
		return Source;
	}

	/** The frame source data for this particle system.  This is everything needed to represent this
	    this particle system frame.  It does not include any transient rendering thread data.  Also, for
		non-simulating 'replay' particle systems, this data may have come straight from disk! */
	FDynamicApexEmitterReplayData Source;

};

#endif // end of WITH_APEX_PARTICLES

/**
*  General particle module utility class.
*/
class FParticleModuleUtils
{

public:

	/**
	 *	Retrieve the skeletal mesh component source to use for the current emitter instance.
	 *
	 *	@param	Owner						The particle emitter instance that is being setup
	 *	@param	ActorParameterName			The name of the parameter corresponding to the SkeletalMeshComponent
	 *
	 *	@return	USkeletalMeshComponent*		The skeletal mesh component to use as the source
	 */
	static USkeletalMeshComponent* GetSkeletalMeshComponentSource(FParticleEmitterInstance* Owner, FName ActorParameterName);

	/**
	 *	Retrieve the static mesh component source to use for the current emitter instance.
	 *
	 *	@param	Owner						The particle emitter instance that is being setup
	 *	@param	ActorParameterName			The name of the parameter corresponding to the SkeletalMeshComponent
	 *
	 *	@return	UStaticMeshComponent*		The skeletal mesh component to use as the source
	 */
	static UStaticMeshComponent* GetStaticMeshComponentSource(FParticleEmitterInstance* Owner, FName ActorParameterName);

	/**
	 *  Check to see if the vert is influenced by a bone on our approved list.
	 *
	 *	@param	Owner					The particle emitter instance that is being setup
	 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
	 *  @param  InVertexIndex			The vertex index of the vert to check.
	 *  @param	InValidBoneIndices		An array of valid bone indices to check against.
	 *	@param	InValidMatIndices		An array of valid material indices to check against.
	 *
	 *  @return UBOOL					TRUE if it is influenced by an approved bone, FALSE otherwise.
	 */
	static UBOOL VertInfluencedByActiveBone(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, INT InVertexIndex, TArray<INT>& InValidBoneIndices, TArray<INT>& InValidMatIndices);

	/**
	 *	Updates the indices list with the bone index for each named bone in the editor exposed values.
	 *	
	 *	@param	Owner					The FParticleEmitterInstance that 'owns' the particle.
	 *  @param	InValidNames			An array of valid names to update the index list with.
	 *  @param	InOutValidIndices			An array of valid indices to update.
	 *  @param	InSkelMeshActorParamName	The name of the emitter instance parameter that specifies the skel mesh actor.
	 */
	static void UpdateBoneIndicesList(FParticleEmitterInstance* Owner, TArrayNoInit<FName>& InValidNames, FName SkelMeshActorParamName, TArray<INT>& InOutValidIndices);

};

#endif
