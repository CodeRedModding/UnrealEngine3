/*=============================================================================
	PrecomputedLightVolume.h: Declarations for precomputed light volumes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PRECOMPUTEDLIGHTVOLUME_H__
#define __PRECOMPUTEDLIGHTVOLUME_H__

#include "GenericOctree.h"

/** Incident radiance stored for a point. */
class FVolumeLightingSample
{
public:
	/** World space position of the sample. */
	FVector Position;
	/** World space radius that determines how far the sample can be interpolated. */
	FLOAT Radius;

	/** Quantized spherical coordinates for the average incident direction of indirect lighting at this sample. */
	BYTE IndirectDirectionTheta;
	BYTE IndirectDirectionPhi;

	/** Quantized spherical coordinates for the average incident direction of environment lighting at this sample. */
	BYTE EnvironmentDirectionTheta;
	BYTE EnvironmentDirectionPhi;

	/** RGBE radiances */
	FColor IndirectRadiance;
	FColor EnvironmentRadiance;
	FColor AmbientRadiance;

	/** Whether the sample is shadowed from all affecting dominant lights. */
	BYTE bShadowedFromDominantLights;

	FVolumeLightingSample() {}

	FVolumeLightingSample(
		const FVector4& PositionAndRadius,
		const FVector4& IndirectDirection,
		const FVector4& EnvironmentDirection,
		FColor IndirectRadiance,
		FColor EnvironmentRadiance,
		FColor AmbientRadiance,
		BYTE bShadowedFromDominantLights);

	friend FArchive& operator<<(FArchive& Ar, FVolumeLightingSample& Sample);

	/** Constructs an SH environment from this lighting sample. */
	void ToSHVector(FSHVectorRGB& SHVector, UBOOL bIsCharacterLightEnvironment) const;
};

struct FLightVolumeOctreeSemantics
{
	enum { MaxElementsPerLeaf = 4 };
	enum { MaxNodeDepth = 12 };

	/** Using the heap allocator instead of an inline allocator to trade off add/remove performance for memory. */
	typedef FDefaultAllocator ElementAllocator;

	FORCEINLINE static const FLOAT* GetBoundingBox(const FVolumeLightingSample& Sample)
	{
		CONSOLE_PREFETCH_NEXT_CACHE_LINE( &Sample );
		// here we require that the position and radius are contiguous in memory
		checkAtCompileTime(STRUCT_OFFSET( FVolumeLightingSample, Position ) + 3 * sizeof(FLOAT) ==  STRUCT_OFFSET( FVolumeLightingSample, Radius ), FVolumeLightingSample_Radius_Must_Follow_Position );
		return &Sample.Position.X;
	}

	static void SetElementId(const FVolumeLightingSample& Element, FOctreeElementId Id)
	{
	}
};

typedef TOctree<FVolumeLightingSample, FLightVolumeOctreeSemantics> FLightVolumeOctree;

/** Set of volume lighting samples belonging to one streaming level, which can be queried about the lighting at a given position. */
class FPrecomputedLightVolume
{
public:

	FPrecomputedLightVolume();
	~FPrecomputedLightVolume();

	friend FArchive& operator<<(FArchive& Ar, FPrecomputedLightVolume& Volume);

	/** Frees any previous samples, prepares the volume to have new samples added. */
	void Initialize(const FBox& NewBounds);

	/** Called when the volume is being added to the world. */
	void AddToWorld(UWorld* World);

	/** Adds a lighting sample. */
	void AddLightingSample(const FVolumeLightingSample& NewSample);

	/** Shrinks the octree and updates memory stats. */
	void FinalizeSamples();

	/** Invalidates anything produced by the last lighting build. */
	void InvalidateLightingCache();

	/** Interpolates incident radiance to Position. */
	void InterpolateIncidentRadiance(
		FVector Position, 
		UBOOL bIsCharacterLightEnvironment,
		UBOOL bDebugInterpolation, 
		TArray<FVolumeLightingSample>& DebugSamples, 
		FLOAT& Weight,
		FSHVectorRGB& IncidentRadiance) const;

	INT GetNumSamples() const { return NumSamples; }
	SIZE_T GetAllocatedBytes() const;

	UBOOL IsInitialized() const
	{
		return bInitialized;
	}
	FBox& GetBounds()
	{
		return Bounds;
	}

private:

	UBOOL bInitialized;
	FBox Bounds;
	INT NumSamples;

	/** Octree used to accelerate interpolation searches. */
	FLightVolumeOctree Octree;
};

#endif