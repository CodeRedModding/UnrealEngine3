/*=============================================================================
	PrecomputedLightVolume.cpp: Implementation of a precomputed light volume.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "PrecomputedLightVolume.h"

FVolumeLightingSample::FVolumeLightingSample(
	const FVector4& PositionAndRadius,
	const FVector4& IndirectDirection,
	const FVector4& EnvironmentDirection,
	FColor InIndirectRadiance,
	FColor InEnvironmentRadiance,
	FColor InAmbientRadiance,
	BYTE bInShadowedFromDominantLights) 
	:
	Position(PositionAndRadius),
	Radius(PositionAndRadius.W),
	IndirectRadiance(InIndirectRadiance),
	EnvironmentRadiance(InEnvironmentRadiance),
	AmbientRadiance(InAmbientRadiance),
	bShadowedFromDominantLights(bInShadowedFromDominantLights)
{
	Position = PositionAndRadius;
	Radius = PositionAndRadius.W;

	if (FVector(IndirectDirection).SizeSquared() > DELTA)
	{
		// Convert to spherical coordinates and quantize
		const FVector2D SphericalIndirectDirection = UnitCartesianToSpherical(IndirectDirection);
		checkSlow(SphericalIndirectDirection.X >= 0 && SphericalIndirectDirection.X <= (FLOAT)PI);
		checkSlow(SphericalIndirectDirection.Y >= -(FLOAT)PI && SphericalIndirectDirection.Y <= (FLOAT)PI);
		IndirectDirectionTheta = appRound(SphericalIndirectDirection.X / (FLOAT)PI * UCHAR_MAX);
		IndirectDirectionPhi = appRound((SphericalIndirectDirection.Y + (FLOAT)PI) / (2.0f * (FLOAT)PI) * UCHAR_MAX);
	}
	else
	{
		IndirectDirectionTheta = 0;
		IndirectDirectionPhi = 0;
	}

	if (FVector(EnvironmentDirection).SizeSquared() > DELTA)
	{
		FVector2D SphericalEnvironmentDirection = UnitCartesianToSpherical(EnvironmentDirection);
		checkSlow(SphericalEnvironmentDirection.X >= 0 && SphericalEnvironmentDirection.X <= (FLOAT)PI);
		checkSlow(SphericalEnvironmentDirection.Y >= -(FLOAT)PI && SphericalEnvironmentDirection.Y <= (FLOAT)PI);
		EnvironmentDirectionTheta = appRound(SphericalEnvironmentDirection.X / (FLOAT)PI * UCHAR_MAX);
		EnvironmentDirectionPhi = appRound((SphericalEnvironmentDirection.Y + (FLOAT)PI) / (2.0f * (FLOAT)PI) * UCHAR_MAX);
	}
	else
	{
		EnvironmentDirectionTheta = 0;
		EnvironmentDirectionPhi = 0;
	}
}

FArchive& operator<<(FArchive& Ar, FVolumeLightingSample& Sample)
{
	Ar << Sample.Position;
	Ar << Sample.Radius;
	if (Ar.Ver() < VER_CHARACTER_INDIRECT_CONTROLS)
	{
		FQuantizedSHVectorRGB Dummy;
		Ar << Dummy;
		FSHVectorRGB ExtractedSH(Dummy);

		Sample.IndirectDirectionTheta = 0;
		Sample.IndirectDirectionPhi = 0;
		Sample.EnvironmentDirectionTheta = 0;
		Sample.EnvironmentDirectionPhi = 0;
		Sample.IndirectRadiance = FColor(0,0,0);
		Sample.EnvironmentRadiance = FColor(0,0,0);
		Sample.AmbientRadiance = (ExtractedSH.CalcIntegral() / FSHVector::ConstantBasisIntegral).ToRGBE();
		Sample.bShadowedFromDominantLights = FALSE;
	}
	else
	{
		Ar << Sample.IndirectDirectionTheta << Sample.IndirectDirectionPhi;
		Ar << Sample.EnvironmentDirectionTheta << Sample.EnvironmentDirectionPhi;
		Ar << Sample.IndirectRadiance << Sample.EnvironmentRadiance << Sample.AmbientRadiance;
		Ar << Sample.bShadowedFromDominantLights;
	}
	return Ar;
}

/** Constructs an SH environment from this lighting sample. */
void FVolumeLightingSample::ToSHVector(FSHVectorRGB& SHVector, UBOOL bIsCharacterLightEnvironment) const
{
	// Dequantize and convert from spherical to cartesian
	const FVector2D ReconstructedSphericalIndirectDirection = FVector2D(IndirectDirectionTheta / (FLOAT)UCHAR_MAX * (FLOAT)PI, IndirectDirectionPhi / (FLOAT)UCHAR_MAX * 2.0f * (FLOAT)PI - (FLOAT)PI);
	const FVector ReconstructedIndirectDirection = SphericalToUnitCartesian(ReconstructedSphericalIndirectDirection);
	
	const FVector2D ReconstructedSphericalEnvironmentDirection = FVector2D(EnvironmentDirectionTheta / (FLOAT)UCHAR_MAX * (FLOAT)PI, EnvironmentDirectionPhi / (FLOAT)UCHAR_MAX * 2.0f * (FLOAT)PI - (FLOAT)PI);
	const FVector ReconstructedEnvironmentDirection = SphericalToUnitCartesian(ReconstructedSphericalEnvironmentDirection);

	// Reconstruct linear colors from RGBE
	const FLinearColor IndirectDirectionalIntensity = IndirectRadiance.FromRGBE();
	const FLinearColor EnvironmentDirectionalIntensity = EnvironmentRadiance.FromRGBE();
	const FLinearColor Ambient = AmbientRadiance.FromRGBE();

	const AWorldInfo* StreamingWorldInfo = GWorld->GetWorldInfo(TRUE);
	const FLOAT BrightnessFactor = bIsCharacterLightEnvironment ? 
		(bShadowedFromDominantLights ? StreamingWorldInfo->CharacterShadowedIndirectBrightness : StreamingWorldInfo->CharacterLitIndirectBrightness) :
		1.0f;
	const FLOAT ContrastFactor = bIsCharacterLightEnvironment ? 
		(bShadowedFromDominantLights ? StreamingWorldInfo->CharacterShadowedIndirectContrastFactor : StreamingWorldInfo->CharacterLitIndirectContrastFactor) :
		1.0f;
	// Accumulate stored lighting terms
	// Assuming SHVector has already been initialized to 0
	SHVector.AddIncomingRadiance(IndirectDirectionalIntensity, BrightnessFactor * ContrastFactor, ReconstructedIndirectDirection);
	SHVector.AddIncomingRadiance(EnvironmentDirectionalIntensity, BrightnessFactor * ContrastFactor, ReconstructedEnvironmentDirection);
	SHVector.AddAmbient(Ambient * BrightnessFactor / ContrastFactor);
}

FPrecomputedLightVolume::FPrecomputedLightVolume() :
	bInitialized(FALSE),
	NumSamples(0),
	Octree(FVector(0,0,0), HALF_WORLD_MAX)
{}

FPrecomputedLightVolume::~FPrecomputedLightVolume()
{
	if (bInitialized)
	{
		const SIZE_T VolumeBytes = GetAllocatedBytes();
		DEC_DWORD_STAT_BY(STAT_PrecomputedLightVolumeMemory, VolumeBytes);
	}
}

FArchive& operator<<(FArchive& Ar,FPrecomputedLightVolume& Volume)
{
	if (Ar.IsCountingMemory())
	{
		const INT AllocatedBytes = Volume.GetAllocatedBytes();
		Ar.CountBytes(AllocatedBytes, AllocatedBytes);
	}
	else if (Ar.IsLoading())
	{
		Ar << Volume.bInitialized;
		if (Volume.bInitialized)
		{
			FBox Bounds;
			Ar << Bounds;
			FLOAT SampleSpacing = 0.0f;
			Ar << SampleSpacing;
			Volume.Initialize(Bounds);
			TArray<FVolumeLightingSample> Samples;
			// Deserialize samples as an array, and add them to the octree
			Ar << Samples;
			for(INT SampleIndex = 0; SampleIndex < Samples.Num(); SampleIndex++)
			{
				Volume.AddLightingSample(Samples(SampleIndex));
			}
			Volume.FinalizeSamples();
		}
	}
	else if (Ar.IsSaving())
	{
		Ar << Volume.bInitialized;
		if (Volume.bInitialized)
		{
			Ar << Volume.Bounds;
			FLOAT SampleSpacing = 0.0f;
			Ar << SampleSpacing;
			TArray<FVolumeLightingSample> Samples;
			// Gather an array of samples from the octree
			for(FLightVolumeOctree::TConstIterator<> NodeIt(Volume.Octree); NodeIt.HasPendingNodes(); NodeIt.Advance())
			{
				const FLightVolumeOctree::FNode& CurrentNode = NodeIt.GetCurrentNode();

				FOREACH_OCTREE_CHILD_NODE(ChildRef)
				{
					if(CurrentNode.HasChild(ChildRef))
					{
						NodeIt.PushChild(ChildRef);
					}
				}

				for (FLightVolumeOctree::ElementConstIt ElementIt(CurrentNode.GetElementIt()); ElementIt; ++ElementIt)
				{
					const FVolumeLightingSample& Sample = *ElementIt;
					Samples.AddItem(Sample);
				}
			}
			Ar << Samples;
		}
	}
	return Ar;
}

/** Frees any previous samples, prepares the volume to have new samples added. */
void FPrecomputedLightVolume::Initialize(const FBox& NewBounds)
{
	InvalidateLightingCache();
	bInitialized = TRUE;
	NumSamples = 0;
	Bounds = NewBounds;
	// Initialize the octree based on the passed in bounds
	Octree = FLightVolumeOctree(NewBounds.GetCenter(), NewBounds.GetExtent().GetMax());
}

/** Called when the volume is being added to the world. */
void FPrecomputedLightVolume::AddToWorld(UWorld* World)
{
	if (bInitialized && GetNumSamples() > 0)
	{
		// Mark light environments as needing a static update if we added new precomputed lighting information
		for(TSparseArray<ULightEnvironmentComponent*>::TConstIterator EnvironmentIt(World->LightEnvironmentList);EnvironmentIt;++EnvironmentIt)
		{
			ULightEnvironmentComponent* LightEnvironmentComponent = *EnvironmentIt;
			if (!LightEnvironmentComponent->HasAnyFlags(RF_Unreachable))
			{
				LightEnvironmentComponent->SetNeedsStaticUpdate();
			}
		}
	}
}

/** Adds a lighting sample. */
void FPrecomputedLightVolume::AddLightingSample(const FVolumeLightingSample& NewSample)
{
	check(bInitialized);
	Octree.AddElement(NewSample);
	NumSamples++;
}

/** Shrinks the octree and updates memory stats. */
void FPrecomputedLightVolume::FinalizeSamples()
{
	check(bInitialized);
	// No more samples will be added, shrink octree node element arrays
	Octree.ShrinkElements();
	const SIZE_T VolumeBytes = GetAllocatedBytes();
	INC_DWORD_STAT_BY(STAT_PrecomputedLightVolumeMemory, VolumeBytes);
}

/** Invalidates anything produced by the last lighting build. */
void FPrecomputedLightVolume::InvalidateLightingCache()
{
	if (bInitialized)
	{
		// Release existing samples
		const SIZE_T VolumeBytes = GetAllocatedBytes();
		DEC_DWORD_STAT_BY(STAT_PrecomputedLightVolumeMemory, VolumeBytes);
		Octree.Destroy();
		bInitialized = FALSE;
	}
}

/** Interpolates incident radiance to Position. */
void FPrecomputedLightVolume::InterpolateIncidentRadiance(
	FVector Position, 
	UBOOL bIsCharacterLightEnvironment,
	UBOOL bDebugInterpolation, 
	TArray<FVolumeLightingSample>& DebugSamples, 
	FLOAT& Weight,
	FSHVectorRGB& IncidentRadiance) const
{
	// Handle being called on a NULL volume, which can happen for a newly created level,
	// Or a volume that hasn't been initialized yet, which can happen if lighting hasn't been built yet.
	if (this && bInitialized)
	{
		FLOAT TotalWeight = 0.0f;
		FSHVectorRGB TotalIncidentRadiance;

		// Iterate over the octree nodes containing the query point.
		for (FLightVolumeOctree::TConstElementBoxIterator<> OctreeIt(Octree, FBoxCenterAndExtent(Position, FVector(0,0,0)));
			OctreeIt.HasPendingElements();
			OctreeIt.Advance())
		{
			const FVolumeLightingSample& VolumeSample = OctreeIt.GetCurrentElement();
			const FLOAT DistanceSquared = (VolumeSample.Position - Position).SizeSquared();
			const FLOAT RadiusSquared = Square(VolumeSample.Radius);
			if (DistanceSquared < RadiusSquared)
			{
				// Weight each sample with the fraction that Position is to the center of the sample, and inversely to the sample radius.
				// The weight goes to 0 when Position is on the bounding radius of the sample, so the interpolated result is continuous.
				// The sample's radius size is a factor so that smaller samples contribute more than larger, low detail ones.
				//@Note: Be sure to update FStaticLightingSystem::InterpolatePrecomputedVolumeIncidentRadiance in Lightmass if interpolation changes in any way!
				const FLOAT SampleWeight = (1.0f - DistanceSquared / RadiusSquared) / RadiusSquared;
				// Convert the quantized SH vector into floating point so it can be filtered
				FSHVectorRGB IncidentRadiance;
				VolumeSample.ToSHVector(IncidentRadiance, bIsCharacterLightEnvironment);
				// Accumulate weighted results and the total weight for normalization later
				TotalIncidentRadiance += IncidentRadiance * SampleWeight;
				TotalWeight += SampleWeight;
				if (bDebugInterpolation)
				{
					DebugSamples.AddItem(VolumeSample);
				}
			}
		}

		Weight = TotalWeight;
		IncidentRadiance = TotalIncidentRadiance;
	}
	else
	{
		Weight = 0.0f;
		IncidentRadiance = FSHVectorRGB();
	}
}

SIZE_T FPrecomputedLightVolume::GetAllocatedBytes() const
{
	SIZE_T NodeBytes = 0;
	for (FLightVolumeOctree::TConstIterator<> NodeIt(Octree); NodeIt.HasPendingNodes(); NodeIt.Advance())
	{
		const FLightVolumeOctree::FNode& CurrentNode = NodeIt.GetCurrentNode();
		NodeBytes += sizeof(FLightVolumeOctree::FNode);
		NodeBytes += CurrentNode.GetElements().GetAllocatedSize();

		FOREACH_OCTREE_CHILD_NODE(ChildRef)
		{
			if(CurrentNode.HasChild(ChildRef))
			{
				NodeIt.PushChild(ChildRef);
			}
		}
	}
	return NodeBytes;
}
