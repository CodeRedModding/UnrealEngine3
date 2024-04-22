/*=============================================================================
	UnParticleBeamModules.cpp: Particle module implementations for beams.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleBeamBase);

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataBeam2 implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTypeDataBeam2);

/**
 *	Spawn
 *	Called when a particle is being spawned.
 *	
 *	@param	Owner		The owning emitter instance of the particle.
 *	@param	Offset		The offset into the payload data for the module.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleTypeDataBeam2::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	// Make sure that the owner is a beam emitter instance.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (!BeamInst)
	{
		return;
	}

	UParticleSystemComponent* Component = Owner->Component;

	// Setup the particle data points with the SPAWN_INIT macro
	SPAWN_INIT;

	FBeam2TypeDataPayload*	BeamData			= NULL;
	FVector*				InterpolatedPoints	= NULL;
	FLOAT*					NoiseRate			= NULL;
	FLOAT*					NoiseDelta			= NULL;
	FVector*				TargetNoisePoints	= NULL;
	FVector*				NextNoisePoints		= NULL;
	FLOAT*					TaperValues			= NULL;
	FLOAT*					NoiseDistanceScale	= NULL;
	FBeamParticleModifierPayloadData* SourceModifier = NULL;
	FBeamParticleModifierPayloadData* TargetModifier = NULL;

	// Get the pointers to the data, but save the module offset that is passed in.
	INT TempOffset = (INT)CurrentOffset;
	GetDataPointers(Owner, ParticleBase, TempOffset, BeamData, InterpolatedPoints, 
		NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, TaperValues,
		NoiseDistanceScale, SourceModifier, TargetModifier);

	CurrentOffset	= TempOffset;

	// If there is no Source module, use the emitter position as the source point
	if (BeamInst->BeamModule_Source == NULL)
	{
		BeamData->SourcePoint	= Component->LocalToWorld.GetOrigin();
		BeamData->SourceTangent = Component->LocalToWorld.GetAxis(0);
		BeamData->SourceStrength = 1.0f;
	}

	// If the beam is set for distance, or there is no target module, determine the target point
	if ((BeamInst->BeamModule_Target == NULL) && (BeamInst->BeamMethod == PEB2M_Distance))
	{
		// Set the particle target based on the distance
		FLOAT	TotalDistance	= Distance.GetValue(Particle.RelativeTime, Component);
		// Always use the X-axis of the component as the direction
		FVector	Direction		= Component->LocalToWorld.GetAxis(0);
		Direction.Normalize();
		// Calculate the final target point
		BeamData->TargetPoint	= BeamData->SourcePoint + Direction * TotalDistance;
		BeamData->TargetTangent = -Direction;
		BeamData->TargetStrength = 1.0f;
	}

	// Modify the source and target positions, if modifiers are present
	if (SourceModifier != NULL)
	{
		// Position
		SourceModifier->UpdatePosition(BeamData->SourcePoint);
		// Tangent...
		SourceModifier->UpdateTangent(BeamData->SourceTangent, 
			BeamInst->BeamModule_SourceModifier ? BeamInst->BeamModule_SourceModifier->bAbsoluteTangent : FALSE);
		// Strength
		SourceModifier->UpdateStrength(BeamData->SourceStrength);
	}

	if (TargetModifier != NULL)
	{
		// Position
		TargetModifier->UpdatePosition(BeamData->TargetPoint);
		// Tangent...
		TargetModifier->UpdateTangent(BeamData->TargetTangent,
			BeamInst->BeamModule_TargetModifier ? BeamInst->BeamModule_TargetModifier->bAbsoluteTangent : FALSE);
		// Strength
		TargetModifier->UpdateStrength(BeamData->TargetStrength);
	}

	// If we are tapering, determine the taper points
	if (TaperMethod != PEBTM_None)
	{
		INT	TaperCount	= 2;
		INT	TotalSteps	= 2;

		if (BeamInst->BeamModule_Noise && BeamInst->BeamModule_Noise->bLowFreq_Enabled)
		{
			//@todo. determine the required number of taper points...
			// (Depends on interaction of interpolation points and noise freq.)
			INT Freq = BEAM2_TYPEDATA_FREQUENCY(BeamData->Lock_Max_NumNoisePoints);
			TaperCount = (Freq + 1) * 
				(BeamInst->BeamModule_Noise->NoiseTessellation ? BeamInst->BeamModule_Noise->NoiseTessellation : 1);
		}
		else
		{
			// The taper count is simply the number of interpolation points + 1.
			TaperCount	 = InterpolationPoints ? (InterpolationPoints + 1) : 2;
		}

		// Taper the beam for the FULL length, regardless of position
		// If the mode is set to partial, it will be handled in the GetData function
		FLOAT Increment	= 1.0f / (FLOAT)(TaperCount - 1);
		FLOAT CurrStep;
		for (INT TaperIndex = 0; TaperIndex < TaperCount; TaperIndex++)
		{
			CurrStep	= TaperIndex * Increment;
			TaperValues[TaperIndex] = TaperFactor.GetValue(CurrStep, Component) * TaperScale.GetValue(CurrStep, Component);
		}
	}
}

/**
 *	PreUpdate
 *	Called when a particle is being updated, prior to any modules having their
 *	Update call executed.
 *	
 *	@param	Owner		The owning emitter instance of the particle.
 *	@param	Offset		The offset into the payload data for the module.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleTypeDataBeam2::PreUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	//@todo.SAS. Remove this function...
}

/**
 *	Update
 *	Called when a particle is being updated.
 *	
 *	@param	Owner		The owning emitter instance of the particle.
 *	@param	Offset		The offset into the payload data for the module.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleTypeDataBeam2::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	UParticleSystemComponent*		Component	= Owner->Component;
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	UParticleModuleBeamNoise*		BeamNoise	= BeamInst->BeamModule_Noise;
	UParticleModuleBeamSource*		BeamSource	= BeamInst->BeamModule_Source;
	UParticleModuleBeamTarget*		BeamTarget	= BeamInst->BeamModule_Target;
	UParticleModuleBeamModifier*	SourceMod	= BeamInst->BeamModule_SourceModifier;
	UParticleModuleBeamModifier*	TargetMod	= BeamInst->BeamModule_TargetModifier;

	// If we are targeting, set the lock radius
	FLOAT	LockRadius	= 1.0f;
	if (BeamTarget)
	{
		LockRadius = BeamTarget->LockRadius;
	}

	UBOOL bSourceTangentAbsolute = BeamInst->BeamModule_SourceModifier ? BeamInst->BeamModule_SourceModifier->bAbsoluteTangent : FALSE;
	UBOOL bTargetTangentAbsolute = BeamInst->BeamModule_TargetModifier ? BeamInst->BeamModule_TargetModifier->bAbsoluteTangent : FALSE;

	// For each particle, run the Update loop
	BEGIN_UPDATE_LOOP;
	{
		FBeam2TypeDataPayload*	BeamData			= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;
		FLOAT*					NoiseDistanceScale	= NULL;
		FBeamParticleModifierPayloadData* SourceModifier = NULL;
		FBeamParticleModifierPayloadData* TargetModifier = NULL;

		// Setup the pointers for the payload data
		INT TempOffset = (INT)CurrentOffset;
		GetDataPointers(Owner, ParticleBase, TempOffset, BeamData, InterpolatedPoints, 
			NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, TaperValues,
			NoiseDistanceScale, SourceModifier, TargetModifier);

		// If there is no Source module, use the emitter location
		if (BeamInst->BeamModule_Source == NULL)
		{
			BeamData->SourcePoint	= Component->LocalToWorld.GetOrigin();
			BeamData->SourceTangent = Component->LocalToWorld.GetAxis(0);
		}

		// If the method is set for distance, or there is no target, determine the target point
		if ((BeamInst->BeamModule_Target == NULL) && (BeamInst->BeamMethod == PEB2M_Distance))
		{
			// Set the particle target based on the distance
			FLOAT	TotalDistance	= Distance.GetValue(Particle.RelativeTime, Component);
			FVector	Direction		= Component->LocalToWorld.GetAxis(0);
			Direction.Normalize();
			BeamData->TargetPoint	= BeamData->SourcePoint + Direction * TotalDistance;
			BeamData->TargetTangent = -Direction;
		}

		// Modify the source and target positions, if modifiers are present
		if (SourceModifier != NULL)
		{
			// Position
			SourceModifier->UpdatePosition(BeamData->SourcePoint);
			// Tangent...
			SourceModifier->UpdateTangent(BeamData->SourceTangent, bSourceTangentAbsolute);
			// Strength
			SourceModifier->UpdateStrength(BeamData->SourceStrength);
		}

		if (TargetModifier != NULL)
		{
			// Position
			TargetModifier->UpdatePosition(BeamData->TargetPoint);
			// Tangent...
			TargetModifier->UpdateTangent(BeamData->TargetTangent, bTargetTangentAbsolute);
			// Strength
			TargetModifier->UpdateStrength(BeamData->TargetStrength);
		}

		// The number of interpolated points (for 'bendy' beams)
		INT		InterpolationCount	= InterpolationPoints ? InterpolationPoints : 1;
		UBOOL	bLowFreqNoise		= (BeamNoise && BeamNoise->bLowFreq_Enabled) ? TRUE : FALSE;


		// Determine the current location of the particle
		//
		// If 'growing' the beam, determine the position along the source->target line
		// Otherwise, pop it right to the target
		if ((Speed != 0.0f) && (!BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints)))
		{
			// If the beam isn't locked, then move it towards the target...
			if (Particle.Location != BeamData->TargetPoint)
			{
				// Determine the direction of travel
				FVector Direction	= BeamData->TargetPoint - Particle.Location;
				Direction.Normalize();
				// Setup the offset and the current distance travelled
				FVector	Offset		= Direction * Speed * DeltaTime;
				FVector	Sum			= Particle.Location + Offset;
				if ((Abs(Sum.X - BeamData->TargetPoint.X) < LockRadius) && 
					(Abs(Sum.Y - BeamData->TargetPoint.Y) < LockRadius) &&
					(Abs(Sum.Z - BeamData->TargetPoint.Z) < LockRadius))
				{
					// We are within the lock radius, so lock the beam
					Particle.Location	= BeamData->TargetPoint;
					BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, 1);
				}
				else
				{
					// Otherwise, just set the location
					Particle.Location	= Sum;
				}
			}
		}
		else
		{
			// Pop right to the target and set the beam as locked
			Particle.Location = BeamData->TargetPoint;
			BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, 1);
		}

		// Determine the step size, count, and travelled ratio
		BeamData->Direction		= BeamData->TargetPoint - BeamData->SourcePoint;
		FLOAT	FullMagnitude	= BeamData->Direction.Size();
		BeamData->Direction.Normalize();

		INT InterpSteps = 0;

		if (bLowFreqNoise == FALSE)
		{
			// No noise branch...
			if (BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
			{
				// If the beam is locked to the target, the steps are the interpolation count
				BeamData->StepSize		= FullMagnitude / InterpolationCount;
				BeamData->Steps			= InterpolationCount;
				BeamData->TravelRatio	= 0.0f;
			}
			else
			{
				// Determine the number of steps we have traveled
				FVector	TrueDistance	= Particle.Location - BeamData->SourcePoint;
				FLOAT	TrueMagnitude	= TrueDistance.Size();
				if (TrueMagnitude > FullMagnitude)
				{
					// Lock to the target if we are over-shooting and determine the steps and step size
					Particle.Location	= BeamData->TargetPoint;
					TrueDistance		= Particle.Location - BeamData->SourcePoint;
					TrueMagnitude		= TrueDistance.Size();
					BEAM2_TYPEDATA_SETLOCKED(BeamData->Lock_Max_NumNoisePoints, TRUE);
					BeamData->StepSize		= FullMagnitude / InterpolationCount;
					BeamData->Steps			= InterpolationCount;
					BeamData->TravelRatio	= 0.0f;
				}
				else
				{
					// Determine the steps and step size
					BeamData->StepSize		= FullMagnitude / InterpolationCount;
					BeamData->TravelRatio	= TrueMagnitude / FullMagnitude;
					BeamData->Steps			= appFloor(BeamData->TravelRatio * InterpolationCount);
					// Readjust the travel ratio
					BeamData->TravelRatio	= (TrueMagnitude - (BeamData->StepSize * BeamData->Steps)) / BeamData->StepSize;
				}
			}
			InterpSteps = BeamData->Steps;
		}
		else
		{
			// Noise branch...
			InterpSteps = InterpolationCount;

			// Grab the frequency for this beam (particle)
			INT Freq = BEAM2_TYPEDATA_FREQUENCY(BeamData->Lock_Max_NumNoisePoints);

			if (BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
			{
				// Locked to the target
				if (BeamNoise->FrequencyDistance > 0.0f)
				{
					// The noise points are based on distance...
					// Determine the number of points to drop.
					INT Count = appTrunc(FullMagnitude / BeamNoise->FrequencyDistance);
					Count = Min<INT>(Count, Freq);
					BeamData->StepSize		= FullMagnitude / (Count + 1);
					BeamData->Steps			= Count;
					BeamData->TravelRatio	= 0.0f;
					if (NoiseDistanceScale != NULL)
					{
						FLOAT Delta = (FLOAT)Count / (FLOAT)(Freq);
						*NoiseDistanceScale = BeamNoise->NoiseScale.GetValue(Delta);
					}
				}
				else
				{
					// If locked, just use the noise frequency to determine steps
					BeamData->StepSize		= FullMagnitude / (Freq + 1);
					BeamData->Steps			= Freq;
					BeamData->TravelRatio	= 0.0f;
					if (NoiseDistanceScale != NULL)
					{
						*NoiseDistanceScale = 1.0f;
					}
				}
			}
			else
			{
				// Determine that actual distance traveled, and its magnitude
				FVector	TrueDistance	= Particle.Location - BeamData->SourcePoint;
				FLOAT	TrueMagnitude	= TrueDistance.Size();

				if (BeamNoise->FrequencyDistance > 0.0f)
				{
					INT Count = appTrunc(FullMagnitude / BeamNoise->FrequencyDistance);
					Count = Min<INT>(Count, Freq);
					BeamData->StepSize		= FullMagnitude / (Count + 1);
					// Determine the partial trail amount and the steps taken
					BeamData->TravelRatio	= TrueMagnitude / FullMagnitude;
					BeamData->Steps			= appFloor(BeamData->TravelRatio * (Count + 1));
					// Lock the steps to the frequency
					if (BeamData->Steps > Count)
					{
						BeamData->Steps = Count;
					}
					// Readjust the travel ratio
					if (BeamData->Steps == Count)
					{
						BeamData->TravelRatio	= 
							(TrueMagnitude - (BeamData->StepSize * BeamData->Steps)) / 
							(FullMagnitude - (BeamData->StepSize * BeamData->Steps));
					}
					else
					{
						BeamData->TravelRatio	= (TrueMagnitude - (BeamData->StepSize * BeamData->Steps)) / BeamData->StepSize;
					}

					if (NoiseDistanceScale != NULL)
					{
						FLOAT Delta = (FLOAT)Count / (FLOAT)(Freq);
						*NoiseDistanceScale = BeamNoise->NoiseScale.GetValue(Delta);
					}
				}
				else
				{
					// If we are not doing noisy interpolation
					// Determine the step size for the full beam
					BeamData->StepSize		= FullMagnitude / (Freq + 1);
					// Determine the partial trail amount and the steps taken
					BeamData->TravelRatio	= TrueMagnitude / FullMagnitude;
					BeamData->Steps			= appFloor(BeamData->TravelRatio * (Freq + 1));
					// Lock the steps to the frequency
					if (BeamData->Steps > Freq)
					{
						BeamData->Steps = Freq;
					}
					// Readjust the travel ratio
					if (BeamData->Steps == Freq)
					{
						BeamData->TravelRatio	= 
							(TrueMagnitude - (BeamData->StepSize * BeamData->Steps)) / 
							(FullMagnitude - (BeamData->StepSize * BeamData->Steps));
					}
					else
					{
						BeamData->TravelRatio	= (TrueMagnitude - (BeamData->StepSize * BeamData->Steps)) / BeamData->StepSize;
					}
				}

				if (NoiseDistanceScale != NULL)
				{
					*NoiseDistanceScale = 1.0f;
				}
			}
			BEAM2_TYPEDATA_SETNOISEPOINTS(BeamData->Lock_Max_NumNoisePoints, BeamData->Steps);
		}

		// Form the interpolated points
		//@todo. Handle interpolate & noise case!
		if (InterpolationPoints > 0)
		{
			BeamData->InterpolationSteps = InterpSteps;

			// Use the tangents
			FVector	SourcePosition;
			FVector	SourceTangent;
			FVector	TargetPosition;
			FVector	TargetTangent;

			FLOAT	InvTess	= 1.0f / InterpolationPoints;

			SourcePosition	 = BeamData->SourcePoint;
			SourceTangent	 = BeamData->SourceTangent;
#if defined(_BEAM2_TYPEDATA_NORMAL_TANGENTS_)
			SourceTangent.Normalize();
#endif	//#if defined(_BEAM2_TYPEDATA_NORMAL_TANGENTS_)
			if (SourceTangent.IsNearlyZero())
			{
				SourceTangent	= Component->LocalToWorld.GetAxis(0);
			}
			SourceTangent	*= BeamData->SourceStrength;

			TargetPosition	 = BeamData->TargetPoint;
			TargetTangent	 = BeamData->TargetTangent;
#if defined(_BEAM2_TYPEDATA_NORMAL_TANGENTS_)
			TargetTangent.Normalize();
#endif	//#if defined(_BEAM2_TYPEDATA_NORMAL_TANGENTS_)
			if (TargetTangent.IsNearlyZero())
			{
				TargetTangent	= Component->LocalToWorld.GetAxis(0);
			}
			TargetTangent	*= BeamData->TargetStrength;

			// Determine the interpolated points along the beam
			FVector	LastPosition	= SourcePosition;
			INT		ii;
			for (ii = 0; ii < InterpSteps; ii++)
			{
				InterpolatedPoints[ii] = CubicInterp(
					SourcePosition, SourceTangent,
					TargetPosition, TargetTangent,
					InvTess * (ii + 1));
				LastPosition		= InterpolatedPoints[ii];
			}

			BeamData->TriangleCount	= BeamData->Steps * 2;
			if (BeamData->TravelRatio > KINDA_SMALL_NUMBER)
			{
//070305.SAS.		BeamData->TriangleCount	+= 2;
			}

			// Grab the remaining steps...
			for (; ii < InterpSteps; ii++)
			{
				InterpolatedPoints[ii] = CubicInterp(
					SourcePosition, SourceTangent,
					TargetPosition, TargetTangent,
					InvTess * (ii + 1));
			}

			if (bLowFreqNoise == TRUE)
			{
				// Noisy interpolated beams!!!
				INT	NoiseTess = BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1;
				// Determine the triangle count
				BeamData->TriangleCount	 = BeamData->Steps * NoiseTess * 2;

					// If it is locked, there is an additional segment
				if (BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
				{
					// The final segment of the beam
					BeamData->TriangleCount	+= NoiseTess * 2;
				}
				else
				if (BeamData->TravelRatio > KINDA_SMALL_NUMBER)
				{
					//@todo.SAS. Fix this!
					// When the data fills in (vertices), it is incorrect.
					BeamData->TriangleCount	+= appFloor(BeamData->TravelRatio * NoiseTess) * 2;
				}
			}
		}
		else
		{
			BeamData->InterpolationSteps = 0;
			if (bLowFreqNoise == FALSE)
			{
				// Straight-line - 2 triangles
				BeamData->TriangleCount	= 2;
			}
			else
			{
				// Determine the triangle count
				INT	NoiseTess = BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1;
				BeamData->TriangleCount	 = BeamData->Steps * NoiseTess * 2;

				// If it is locked, there is an additional segment
				if (BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints))
				{
					// The final segment of the beam
					BeamData->TriangleCount	+= NoiseTess * 2;
				}
				else
				if (BeamData->TravelRatio > KINDA_SMALL_NUMBER)
				{
					//@todo.SAS. Fix this!
					// When the data fills in (vertices), it is incorrect.
//					BeamData->TriangleCount	+= appFloor(BeamData->TravelRatio * NoiseTess) * 2;
				}
			}
		}
	}
	END_UPDATE_LOOP;
}

/**
 *	RequiredBytes
 *	The number of bytes that are required per-particle for this module.
 *	
 *	@param	Owner		The owning emitter instance of the particle.
 *
 *	@return				The number of bytes required
 */
UINT UParticleModuleTypeDataBeam2::RequiredBytes(FParticleEmitterInstance* Owner)
{
	INT	Size		= 0;
	INT	TaperCount	= 2;

	// Every beam requires the Beam2PayloadData
	FParticleBeam2EmitterInstance* BeamInst = CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	Size	+= sizeof(FBeam2TypeDataPayload);		// Beam2 payload data

	// Store the interpolated points for each beam.
	if (InterpolationPoints >= 0)
	{
		Size		+= sizeof(FVector) * InterpolationPoints;
		TaperCount	 = InterpolationPoints ? (InterpolationPoints + 1) : 2;
	}

	UParticleModuleBeamNoise* BeamNoise = BeamInst->BeamModule_Noise;
	if (BeamNoise)
	{
		if (BeamNoise->bLowFreq_Enabled)
		{
			// This is ok as it will be the maximum number of points required...
			INT	Frequency	= BeamNoise->Frequency + 1;

			// For locking noise
//			if (NoiseLockTime > 0.0f)
			{
				Size	+= sizeof(FLOAT);				// Particle noise update time
				Size	+= sizeof(FLOAT);				// Delta time
			}
			Size	+= sizeof(FVector) * Frequency;		// The noise point positions
			if (BeamNoise->bSmooth)
			{
				Size	+= sizeof(FVector) * Frequency;	// The current noise point positions
			}

			//@todo. determine the required number of taper points...
			// (Depends on interaction of interpolation points and noise freq.
			TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

			if (BeamNoise->bApplyNoiseScale)
			{
				Size	+= sizeof(FLOAT);				// Noise point scale
			}
		}
	}

	// If tapering, we need to store the taper sizes as well
	if (TaperMethod != PEBTM_None)
	{
		Size	+= sizeof(FLOAT) * TaperCount;
	}

	return Size;
}

/**
 *	PostEditChange
 *	Called after a property in the class has changed.
 *
 *	@param	PropertyThatChanged		Pointer to the property that changed
 */
void UParticleModuleTypeDataBeam2::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		// Make sure that 0 <= beam count <= FDynamicBeam2EmitterData::MaxBeams.
		if (PropertyThatChanged->GetFName() == FName(TEXT("MaxBeamCount")))
		{
			MaxBeamCount = Clamp<INT>(MaxBeamCount, 0, FDynamicBeam2EmitterData::MaxBeams);
		}
		// Make sure that the interpolation count is > 0.
		if (PropertyThatChanged->GetFName() == FName(TEXT("InterpolationPoints")))
		{
			// Clamp the interpolation points to FDynamicBeam2EmitterData::MaxInterpolationPoints...
			InterpolationPoints = Clamp<INT>(InterpolationPoints, 0, FDynamicBeam2EmitterData::MaxInterpolationPoints);
		}

		// For now, we are restricting this setting to 0 (all points) or 1 (the start point)
		UpVectorStepSize = Clamp<INT>(UpVectorStepSize, 0, 1);
	}

	UParticleSystem* PartSys = CastChecked<UParticleSystem>(GetOuter());
	if (PartSys && PropertyThatChanged)
	{
		PartSys->PostEditChangeProperty(PropertyChangedEvent);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 *	CreateInstance
 *	Called by a particle system component to create an instance of an emitter
 *
 *	@param	InEmitterParent		The parent emitter
 *	@param	InComponent			The owning component
 *
 *	@return						A pointer to the created emitter instance
 */
FParticleEmitterInstance* UParticleModuleTypeDataBeam2::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	SetToSensibleDefaults(InEmitterParent);
	FParticleEmitterInstance* Instance = new FParticleBeam2EmitterInstance();
	check(Instance);

	Instance->InitParameters(InEmitterParent, InComponent);

	return Instance;
}

/** Add all curve-editable Objects within this module to the curve. */
void UParticleModuleTypeDataBeam2::AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup)
{
#if WITH_EDITORONLY_DATA
	//@todo. Once the old members are deprecated, open these functions back up...
	// Until then, any new distributions added to this module will have to be
	// hand-checked for in this function!!!!
	EdSetup->AddCurveToCurrentTab(Distance.Distribution, FString(TEXT("Distance")), ModuleEditorColor);
	EdSetup->AddCurveToCurrentTab(TaperFactor.Distribution, FString(TEXT("TaperFactor")), ModuleEditorColor);
#endif // WITH_EDITORONLY_DATA
}

/**
 *	GetDataPointers
 *	Retrieves the data pointers stored in the particle payload.
 *	
 *	@param	Owner				The owning emitter instance of the particle.
 *	@param	ParticleBase		Pointer to the particle of interest
 *	@param	CurrentOffset		The offset to the particle payload
 *	@param	BeamData			The FBeam2TypeDataPayload pointer - output
 *	@param	InterpolatedPoints	The FVector interpolated points pointer - output
 *	@param	NoiseRate			The FLOAT NoiseRate pointer - output
 *	@param	NoiseDeltaTime		The FLOAT NoiseDeltaTime pointer - output
 *	@param	TargetNoisePoints	The FVector TargetNoisePoints pointer - output
 *	@param	NextNoisePoints		The FVector NextNoisePoints pointer - output
 *	@param	TaperValues			The FLOAT TaperValues pointer - output
 *	@param	NoiseDistanceScale	The FLOAT NoiseDistanceScale pointer - output
 *	@param	SourceModifier		The FBeamParticleModifierPayloadData for the source - output
 *	@param	TargetModifier		The FBeamParticleModifierPayloadData for the target - output
 */
void UParticleModuleTypeDataBeam2::GetDataPointers(FParticleEmitterInstance* Owner, 
	const BYTE* ParticleBase, INT& CurrentOffset, FBeam2TypeDataPayload*& BeamData, 
	FVector*& InterpolatedPoints, FLOAT*& NoiseRate, FLOAT*& NoiseDeltaTime, 
	FVector*& TargetNoisePoints, FVector*& NextNoisePoints, FLOAT*& TaperValues, FLOAT*& NoiseDistanceScale,
	FBeamParticleModifierPayloadData*& SourceModifier, FBeamParticleModifierPayloadData*& TargetModifier)
{
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	UParticleModuleBeamNoise*		BeamNoise	= BeamInst->BeamModule_Noise;

	INT	TaperCount	= 2;

	// There will alwyas be a TypeDataPayload
	PARTICLE_ELEMENT(FBeam2TypeDataPayload, Data);
	BeamData	= &Data;

	if (InterpolationPoints > 0)
	{
		// Grab the interpolation points
		PARTICLE_ELEMENT(FVector, InterpPoints);
		InterpolatedPoints	 = &InterpPoints; 
		CurrentOffset		+= sizeof(FVector) * (InterpolationPoints - 1);
		TaperCount			 = InterpolationPoints ? (InterpolationPoints + 1) : 2;
	}

	if (BeamNoise)
	{
		if (BeamNoise->bLowFreq_Enabled)
		{
			INT	Frequency	= BeamNoise->Frequency + 1;

//			if (NoiseLockTime > 0.0f)
			{
				PARTICLE_ELEMENT(FLOAT, NoiseRateData);
				NoiseRate	= &NoiseRateData;
				PARTICLE_ELEMENT(FLOAT, NoiseDeltaTimeData);
				NoiseDeltaTime	= &NoiseDeltaTimeData;
			}
			
			PARTICLE_ELEMENT(FVector, TargetNoiseData);
			TargetNoisePoints	= &TargetNoiseData;
			CurrentOffset	+= sizeof(FVector) * (Frequency - 1);
			
			if (BeamNoise->bSmooth)
			{
				PARTICLE_ELEMENT(FVector, NextNoiseData);
				NextNoisePoints	= &NextNoiseData;
				CurrentOffset	+= sizeof(FVector) * (Frequency - 1);
			}

			//@todo. determine the required number of taper points...
			// (Depends on interaction of interpolation points and noise freq.
			TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

			if (BeamNoise->bApplyNoiseScale)
			{
				PARTICLE_ELEMENT(FLOAT, NoiseDistScale);
				NoiseDistanceScale = &NoiseDistScale;
			}
		}
	}

	if (TaperMethod != PEBTM_None)
	{
		PARTICLE_ELEMENT(FLOAT, TaperData);
		TaperValues		 = &TaperData;
		CurrentOffset	+= sizeof(FLOAT) * (TaperCount - 1);
	}

	if (BeamInst->BeamModule_SourceModifier_Offset != -1)
	{
		INT TempOffset = CurrentOffset;
		CurrentOffset = BeamInst->BeamModule_SourceModifier_Offset;
		PARTICLE_ELEMENT(FBeamParticleModifierPayloadData, SourceModPayload);
		SourceModifier = &SourceModPayload;
		CurrentOffset = TempOffset;
	}

	if (BeamInst->BeamModule_TargetModifier_Offset != -1)
	{
		INT TempOffset = CurrentOffset;
		CurrentOffset = BeamInst->BeamModule_TargetModifier_Offset;
		PARTICLE_ELEMENT(FBeamParticleModifierPayloadData, TargetModPayload);
		TargetModifier = &TargetModPayload;
		CurrentOffset = TempOffset;
	}
}

/**
 *	GetDataPointerOffsets
 *	Retrieves the offsets to the data stored in the particle payload.
 *	
 *	@param	Owner						The owning emitter instance of the particle.
 *	@param	ParticleBase				Pointer to the particle of interest
 *	@param	CurrentOffset				The offset to the particle payload
 *	@param	BeamDataOffset				The FBeam2TypeDataPayload pointer - output
 *	@param	InterpolatedPointsOffset	The FVector interpolated points pointer - output
 *	@param	NoiseRateOffset				The FLOAT NoiseRate pointer - output
 *	@param	NoiseDeltaTimeOffset		The FLOAT NoiseDeltaTime pointer - output
 *	@param	TargetNoisePointsOffset		The FVector TargetNoisePoints pointer - output
 *	@param	NextNoisePointsOffset		The FVector NextNoisePoints pointer - output
 *	@param	TaperCount					The INT TaperCount - output
 *	@param	TaperValuesOffset			The FLOAT TaperValues pointer - output
 *	@param	NoiseDistanceScaleOffset	The FLOAT NoiseDistanceScale pointer - output
 */
void UParticleModuleTypeDataBeam2::GetDataPointerOffsets(FParticleEmitterInstance* Owner, 
	const BYTE* ParticleBase, INT& CurrentOffset, INT& BeamDataOffset, 
	INT& InterpolatedPointsOffset, INT& NoiseRateOffset, INT& NoiseDeltaTimeOffset, 
	INT& TargetNoisePointsOffset, INT& NextNoisePointsOffset, 
	INT& TaperCount, INT& TaperValuesOffset, INT& NoiseDistanceScaleOffset)
{
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	UParticleModuleBeamNoise*		BeamNoise	= BeamInst->BeamModule_Noise;

	INT	LocalOffset = 0;
	
	NoiseRateOffset = -1;
	NoiseDeltaTimeOffset = -1;
	TargetNoisePointsOffset = -1;
	NextNoisePointsOffset = -1;
	InterpolatedPointsOffset = -1;
	TaperCount	= 2;
	TaperValuesOffset = -1;
	NoiseDistanceScaleOffset = -1;

	BeamDataOffset = CurrentOffset + LocalOffset;
	LocalOffset += sizeof(FBeam2TypeDataPayload);

	if (InterpolationPoints > 0)
	{
		InterpolatedPointsOffset = CurrentOffset + LocalOffset;
		LocalOffset += sizeof(FVector) * InterpolationPoints;
		TaperCount	 = InterpolationPoints ? (InterpolationPoints + 1) : 2;
	}

	if (BeamNoise)
	{
		if (BeamNoise->bLowFreq_Enabled)
		{
			INT Frequency	= BeamNoise->Frequency + 1;

			//if (NoiseLockTime > 0.0f)
			{
				NoiseRateOffset = CurrentOffset + LocalOffset;
				LocalOffset += sizeof(FLOAT);
				NoiseDeltaTimeOffset = CurrentOffset + LocalOffset;
				LocalOffset += sizeof(FLOAT);
			}

			TargetNoisePointsOffset = CurrentOffset + LocalOffset;
			LocalOffset += sizeof(FVector) * Frequency;

			if (BeamNoise->bSmooth)
			{
				NextNoisePointsOffset = CurrentOffset + LocalOffset;
				LocalOffset += sizeof(FVector) * Frequency;
			}

			//@todo. determine the required number of taper points...
			// (Depends on interaction of interpolation points and noise freq.
			TaperCount = (Frequency + 1) * (BeamNoise->NoiseTessellation ? BeamNoise->NoiseTessellation : 1);

			if (BeamNoise->bApplyNoiseScale)
			{
				NoiseDistanceScaleOffset = CurrentOffset + LocalOffset;
				LocalOffset += sizeof(FLOAT);
			}
		}
	}

	if (TaperMethod != PEBTM_None)
	{
		TaperValuesOffset = CurrentOffset + LocalOffset;
		LocalOffset	+= sizeof(FLOAT) * TaperCount;
	}
}

/**
 *	GetNoiseRange
 *	Retrieves the range of noise
 *	
 *	@param	NoiseMin		The minimum noise - output
 *	@param	NoiseMax		The maximum noise - output
 */
void UParticleModuleTypeDataBeam2::GetNoiseRange(FVector& NoiseMin, FVector& NoiseMax)
{
	NoiseMin	= FVector(0.0f, 0.0f, 0.0f);
	NoiseMax	= FVector(0.0f, 0.0f, 0.0f);
}

/*-----------------------------------------------------------------------------
	UParticleModuleBeamModifier implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleBeamModifier);

void UParticleModuleBeamModifier::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	// Make sure that the owner is a beam emitter instance.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (BeamInst == NULL)
	{
		return;
	}

	// Setup the particle data points with the SPAWN_INIT macro
	SPAWN_INIT;
	{
		FBeam2TypeDataPayload* BeamDataPayload = NULL;
		FBeamParticleModifierPayloadData* SourceModifierPayload = NULL;
		FBeamParticleModifierPayloadData* TargetModifierPayload = NULL;

		// Get the pointers to the data, but save the module offset that is passed in.
		GetDataPointers(Owner, (const BYTE*)&Particle, Offset, BeamDataPayload, SourceModifierPayload, TargetModifierPayload);

		FBeamParticleModifierPayloadData* ModifierPayload = (ModifierType == PEB2MT_Source) ? 
			SourceModifierPayload : TargetModifierPayload;

		if (ModifierPayload)
		{
			// Set the Position value
			ModifierPayload->bModifyPosition = PositionOptions.bModify;
			if (PositionOptions.bModify == TRUE)
			{
				ModifierPayload->Position = Position.GetValue(Owner->EmitterTime, Owner->Component);
				ModifierPayload->bScalePosition = PositionOptions.bScale;
			}

			// Set the Tangent value
			ModifierPayload->bModifyTangent = TangentOptions.bModify;
			if (TangentOptions.bModify == TRUE)
			{
				ModifierPayload->Tangent = Tangent.GetValue(Owner->EmitterTime, Owner->Component);
				ModifierPayload->bScaleTangent = TangentOptions.bScale;
			}

			// Set the strength value
			ModifierPayload->bModifyStrength = StrengthOptions.bModify;
			if (StrengthOptions.bModify == TRUE)
			{
				ModifierPayload->Strength = Strength.GetValue(Owner->EmitterTime, Owner->Component);
				ModifierPayload->bScaleStrength = StrengthOptions.bScale;
			}
		}
	}
}

void UParticleModuleBeamModifier::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	// Make sure that the owner is a beam emitter instance.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (BeamInst == NULL)
	{
		return;
	}

	BEGIN_UPDATE_LOOP;
	{
		FBeam2TypeDataPayload* BeamDataPayload = NULL;
		FBeamParticleModifierPayloadData* SourceModifierPayload = NULL;
		FBeamParticleModifierPayloadData* TargetModifierPayload = NULL;

		// Get the pointers to the data, but save the module offset that is passed in.
		GetDataPointers(Owner, (const BYTE*)&Particle, Offset, BeamDataPayload, SourceModifierPayload, TargetModifierPayload);

		FBeamParticleModifierPayloadData* ModifierPayload = (ModifierType == PEB2MT_Source) ? 
			SourceModifierPayload : TargetModifierPayload;

		if (ModifierPayload)
		{
			// Set the Position value
			ModifierPayload->bModifyPosition = PositionOptions.bModify;
			if ((PositionOptions.bModify == TRUE) && (PositionOptions.bLock == FALSE))
			{
				ModifierPayload->Position = Position.GetValue(Owner->EmitterTime, Owner->Component);
				ModifierPayload->bScalePosition = PositionOptions.bScale;
			}

			// Set the Tangent value
			ModifierPayload->bModifyTangent = TangentOptions.bModify;
			if ((TangentOptions.bModify == TRUE) && (TangentOptions.bLock == FALSE))
			{
				ModifierPayload->Tangent = Tangent.GetValue(Owner->EmitterTime, Owner->Component);
				ModifierPayload->bScaleTangent = TangentOptions.bScale;
			}

			// Set the strength value
			ModifierPayload->bModifyStrength = StrengthOptions.bModify;
			if ((StrengthOptions.bModify == TRUE) && (StrengthOptions.bLock == FALSE))
			{
				ModifierPayload->Strength = Strength.GetValue(Owner->EmitterTime, Owner->Component);
				ModifierPayload->bScaleStrength = StrengthOptions.bScale;
			}
		}
	}
	END_UPDATE_LOOP;
}

UINT UParticleModuleBeamModifier::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FBeamParticleModifierPayloadData);
}

void UParticleModuleBeamModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{

}

void UParticleModuleBeamModifier::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{

}

/**
 *	Retrieve the ParticleSysParams associated with this module.
 *
 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
 */
void UParticleModuleBeamModifier::GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList)
{
}

/** 
 *	Fill an array with each Object property that fulfills the FCurveEdInterface interface.
 *
 *	@param	OutCurve	The array that should be filled in.
 */
void UParticleModuleBeamModifier::GetCurveObjects(TArray<FParticleCurvePair>& OutCurves)
{
	FParticleCurvePair* NewCurve;

	NewCurve = new(OutCurves) FParticleCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = Position.Distribution;
	NewCurve->CurveName = FString(TEXT("Position"));
	NewCurve = new(OutCurves) FParticleCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = Tangent.Distribution;
	NewCurve->CurveName = FString(TEXT("Tangent"));
	NewCurve = new(OutCurves) FParticleCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = Strength.Distribution;
	NewCurve->CurveName = FString(TEXT("Strength"));
}

/** 
 *	Add all curve-editable Objects within this module to the curve editor.
 *
 *	@param	EdSetup		The CurveEd setup to use for adding curved.
 */
void UParticleModuleBeamModifier::AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup)
{
#if WITH_EDITORONLY_DATA
	EdSetup->AddCurveToCurrentTab(Position.Distribution, TEXT("Position"), ModuleEditorColor, FALSE);
	EdSetup->AddCurveToCurrentTab(Tangent.Distribution, TEXT("Tangent"), ModuleEditorColor, FALSE);
	EdSetup->AddCurveToCurrentTab(Strength.Distribution, TEXT("Strength"), ModuleEditorColor, FALSE);
#endif // WITH_EDITORONLY_DATA
}

void UParticleModuleBeamModifier::GetDataPointers(FParticleEmitterInstance* Owner, const BYTE* ParticleBase, 
	INT& CurrentOffset, FBeam2TypeDataPayload*& BeamDataPayload, 
	FBeamParticleModifierPayloadData*& SourceModifierPayload,
	FBeamParticleModifierPayloadData*& TargetModifierPayload)
{
	// Make sure that the owner is a beam emitter instance.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (BeamInst == NULL)
	{
		return;
	}

	if (BeamInst->BeamModule_SourceModifier)
	{
		SourceModifierPayload = (FBeamParticleModifierPayloadData*)(ParticleBase + BeamInst->BeamModule_SourceModifier_Offset);
	}
	else
	{
		SourceModifierPayload = NULL;
	}

	if (BeamInst->BeamModule_TargetModifier)
	{
		TargetModifierPayload = (FBeamParticleModifierPayloadData*)(ParticleBase + BeamInst->BeamModule_TargetModifier_Offset);
	}
	else
	{
		TargetModifierPayload = NULL;
	}
}

void UParticleModuleBeamModifier::GetDataPointerOffsets(FParticleEmitterInstance* Owner, const BYTE* ParticleBase, 
	INT& CurrentOffset, INT& BeamDataOffset, INT& SourceModifierOffset, INT& TargetModifierOffset)
{
	// Make sure that the owner is a beam emitter instance.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (BeamInst == NULL)
	{
		return;
	}

	BeamDataOffset = BeamInst->TypeDataOffset;
	SourceModifierOffset = BeamInst->BeamModule_SourceModifier_Offset;
	TargetModifierOffset = BeamInst->BeamModule_TargetModifier_Offset;
}

/*-----------------------------------------------------------------------------
	UParticleModuleBeamNoise implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleBeamNoise);

void UParticleModuleBeamNoise::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	if (bLowFreq_Enabled == FALSE)
	{
		// Noise is present but disabled...
		return;
	}

	// Make sure that the owner is a beam emitter instance.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (!BeamInst || !bLowFreq_Enabled || (Frequency == 0))
	{
		return;
	}

	UParticleSystemComponent*		Component	= Owner->Component;
	UParticleModuleTypeDataBeam2*	BeamTD		= BeamInst->BeamTypeData;

	SPAWN_INIT;

	FBeam2TypeDataPayload*	BeamData			= NULL;
	FVector*				InterpolatedPoints	= NULL;
	FLOAT*					NoiseRate			= NULL;
	FLOAT*					NoiseDelta			= NULL;
	FVector*				TargetNoisePoints	= NULL;
	FVector*				NextNoisePoints		= NULL;
	FLOAT*					TaperValues			= NULL;
	FLOAT*					NoiseDistanceScale	= NULL;
	FBeamParticleModifierPayloadData* SourceModifier = NULL;
	FBeamParticleModifierPayloadData* TargetModifier = NULL;

	// Retrieve the data points
	INT TempOffset = BeamInst->TypeDataOffset;
	BeamTD->GetDataPointers(Owner, ParticleBase, TempOffset, BeamData, InterpolatedPoints, 
		NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, TaperValues,
		NoiseDistanceScale, SourceModifier, TargetModifier);
	CurrentOffset	= TempOffset;

	// There should always be noise points
	check(TargetNoisePoints);
	if (bSmooth)
	{
		// There should be next noise points when smoothly moving them
		check(NextNoisePoints);
	}

	// If the frequency range mode is enabled, select a frequency
	INT CalcFreq = Frequency;
	if (Frequency_LowRange > 0)
	{
		CalcFreq = appTrunc((appSRand() * (Frequency - Frequency_LowRange)) + Frequency_LowRange);
	}
	BEAM2_TYPEDATA_SETFREQUENCY(BeamData->Lock_Max_NumNoisePoints, CalcFreq);
	
	// Pre-pick the initial noise points - for noise-lock cases
	FLOAT	StepSize		= 1.0f / (CalcFreq + 1);

	// Fill in the points...

	// See if we are oscillating
	UBOOL bLocalOscillate = FALSE;
	if (NoiseRange.IsUniform())
	{
		bLocalOscillate = TRUE;
	}

	// Handle bouncing between extremes
	INT	Extreme = -1;
	for (INT ii = 0; ii < (CalcFreq + 1); ii++)
	{
		if (bLocalOscillate && bOscillate)
		{
			Extreme = -Extreme;
		}
		else
		{
			Extreme = 0;
		}
		TargetNoisePoints[ii] = NoiseRange.GetValue(StepSize * ii, Owner->Component, Extreme);
		if (bSmooth)
		{
			Extreme = -Extreme;
			NextNoisePoints[ii] = NoiseRange.GetValue(StepSize * ii, Owner->Component, Extreme);
		}
	}
}

void UParticleModuleBeamNoise::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if (bLowFreq_Enabled == FALSE)
	{
		// Noise is present but disabled...
		return;
	}

	// Make sure that the owner is a beam emitter instance and there is noise.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (!BeamInst || (Frequency == 0))
	{
		return;
	}

	UParticleSystemComponent*		Component	= Owner->Component;
	UParticleModuleTypeDataBeam2*	BeamTD		= BeamInst->BeamTypeData;

	UBOOL bLocalOscillate = FALSE;
	if (NoiseRange.IsUniform())
	{
		bLocalOscillate = TRUE;
	}

	INT	Extreme = -1;

	BEGIN_UPDATE_LOOP;
	{
		FBeam2TypeDataPayload*	BeamData			= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;
		FLOAT*					NoiseDistanceScale	= NULL;
		FBeamParticleModifierPayloadData* SourceModifier = NULL;
		FBeamParticleModifierPayloadData* TargetModifier = NULL;

		INT						TempOffset	= BeamInst->TypeDataOffset;
		BeamTD->GetDataPointers(Owner, ParticleBase, TempOffset, BeamData, InterpolatedPoints, 
			NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, TaperValues,
			NoiseDistanceScale, SourceModifier, TargetModifier);

		check(TargetNoisePoints);
		if (bSmooth)
		{
			check(NextNoisePoints);
		}

		INT Freq = BEAM2_TYPEDATA_FREQUENCY(BeamData->Lock_Max_NumNoisePoints);

		{
			if (bLocalOscillate && bOscillate)
			{
				Extreme = -Extreme;
			}
			else
			{
				Extreme = 0;
			}
			if (NoiseLockTime < 0.0f)
			{
				// Do nothing...
			}
			else
			{
				FLOAT	StepSize	= 1.0f / (Freq + 1);

				// Fill in the points...
				if (NoiseLockTime > KINDA_SMALL_NUMBER)
				{
					//@todo. Add support for moving noise points!
					// Check the times...
					check(NoiseRate);
					*NoiseRate += DeltaTime;
					if (*NoiseRate > NoiseLockTime)
					{
						if (bSmooth)
						{
							for (INT ii = 0; ii < (Freq + 1); ii++)
							{
								NextNoisePoints[ii] = NoiseRange.GetValue(StepSize * ii, Owner->Component, Extreme);
							}
						}
						else
						{
							for (INT ii = 0; ii < (Freq + 1); ii++)
							{
								TargetNoisePoints[ii] = NoiseRange.GetValue(StepSize * ii, Owner->Component, Extreme);
							}
						}
						*NoiseRate	= 0.0f;
					}
					*NoiseDelta	= DeltaTime;
				}
				else
				{
					for (INT ii = 0; ii < (Freq + 1); ii++)
					{
						TargetNoisePoints[ii] = NoiseRange.GetValue(StepSize * ii, Owner->Component, Extreme);
					}
				}
			}
		}
	}
	END_UPDATE_LOOP;
}

void UParticleModuleBeamNoise::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	// Set the noise lock flag
	if (NoiseLockTime < 0.0f)
	{
		bNoiseLock	= TRUE;
	}
	else
	{
		bNoiseLock	= FALSE;
	}
}

void UParticleModuleBeamNoise::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Clamp the noise points to FDynamicBeam2EmitterData::MaxNoiseFrequency...
	if (Frequency > FDynamicBeam2EmitterData::MaxNoiseFrequency)
	{
		Frequency = FDynamicBeam2EmitterData::MaxNoiseFrequency;
	}

	if (Frequency_LowRange > Frequency)
	{
		if (Frequency_LowRange > FDynamicBeam2EmitterData::MaxNoiseFrequency)
		{
			Frequency_LowRange = FDynamicBeam2EmitterData::MaxNoiseFrequency;
		}
		Frequency = Frequency_LowRange;
	}

	UParticleSystem* PartSys = CastChecked<UParticleSystem>(GetOuter());
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PartSys && PropertyThatChanged)
	{
		PartSys->PostEditChangeProperty(PropertyChangedEvent);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleBeamNoise::GetNoiseRange(FVector& NoiseMin, FVector& NoiseMax)
{
#if BEAMS_TODO
	NoiseRange.GetOutRange(NoiseMin, NoiseMax);
#endif	//#if BEAMS_TODO

	FLOAT Min, Max;
	// get the min/max for x, y AND z
	NoiseRange.GetOutRange(Min, Max);
	// make vectors out of the floats
	NoiseMin.X = NoiseMin.Y = NoiseMin.Z = Min;
	NoiseMax.X = NoiseMax.Y = NoiseMax.Z = Max;
}

/*-----------------------------------------------------------------------------
	UParticleModuleBeamSource implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleBeamSource);

void UParticleModuleBeamSource::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	// Make sure that the owner is a beam emitter instance.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (!BeamInst)
	{
		return;
	}
	UParticleSystemComponent*		Component	= Owner->Component;
	UParticleModuleTypeDataBeam2*	BeamTD		= BeamInst->BeamTypeData;

	SPAWN_INIT;

	FBeam2TypeDataPayload*	BeamData			= NULL;
	FVector*				InterpolatedPoints	= NULL;
	FLOAT*					NoiseRate			= NULL;
	FLOAT*					NoiseDelta			= NULL;
	FVector*				TargetNoisePoints	= NULL;
	FVector*				NextNoisePoints		= NULL;
	FLOAT*					TaperValues			= NULL;
	FLOAT*					NoiseDistanceScale	= NULL;
	FBeamParticleModifierPayloadData* SourceModifier = NULL;
	FBeamParticleModifierPayloadData* TargetModifier = NULL;

	// Retrieve the data pointers from the payload
	INT	TempOffset	= BeamInst->TypeDataOffset;
	BeamInst->BeamTypeData->GetDataPointers(Owner, ParticleBase, TempOffset, BeamData, 
		InterpolatedPoints, NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, 
		TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);

	// Resolve the source data
	ResolveSourceData(BeamInst, BeamData, ParticleBase, Offset, BeamInst->ActiveParticles, TRUE, SourceModifier);

	// Set the location and clear the initial data flags
	Particle.Location					= BeamData->SourcePoint;
	BeamData->Lock_Max_NumNoisePoints	= 0;
	BeamData->StepSize					= 0.0f;
	BeamData->Steps						= 0;
	BeamData->TravelRatio				= 0.0f;
	BeamData->TriangleCount				= 0;
}

void UParticleModuleBeamSource::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	// If the source is locked, don't perform any update
	if (bLockSource && bLockSourceTangent && bLockSourceStength)
	{
		return;
	}

	// Make sure that the owner is a beam emitter instance.
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (!BeamInst)
	{
		return;
	}
	UParticleSystemComponent*		Component	= Owner->Component;
	UParticleModuleTypeDataBeam2*	BeamTD		= BeamInst->BeamTypeData;

	BEGIN_UPDATE_LOOP;
	{
		FBeam2TypeDataPayload*	BeamData			= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;
		FLOAT*					NoiseDistanceScale	= NULL;
		FBeamParticleModifierPayloadData* SourceModifier = NULL;
		FBeamParticleModifierPayloadData* TargetModifier = NULL;

		// Retrieve the payload data offsets
		INT	TempOffset	= BeamInst->TypeDataOffset;
		BeamTD->GetDataPointers(Owner, ParticleBase, TempOffset, BeamData, InterpolatedPoints, 
			NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, TaperValues,
			NoiseDistanceScale, SourceModifier, TargetModifier);

		// Resolve the source data
		ResolveSourceData(BeamInst, BeamData, ParticleBase, Offset, i, FALSE, SourceModifier);
	}
	END_UPDATE_LOOP;
}

UINT UParticleModuleBeamSource::RequiredBytes(FParticleEmitterInstance* Owner)
{
	INT	Size	= 0;

	FParticleBeam2EmitterInstance* BeamInst = CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (BeamInst)
	{
		UParticleModuleTypeDataBeam2*	BeamTD	= BeamInst->BeamTypeData;
		if (BeamTD)
		{
			if (SourceMethod == PEB2STM_Particle)
			{
				// Store the data for the particle source payload
				Size	+= sizeof(FBeamParticleSourceTargetPayloadData);
			}
			if (BeamTD->BeamMethod == PEB2M_Branch)
			{
				// Store the data for the particle branch payload
				Size	+= sizeof(FBeamParticleSourceBranchPayloadData);
			}
		}
	}

	return Size;
}

void UParticleModuleBeamSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UParticleSystem* PartSys = CastChecked<UParticleSystem>(GetOuter());
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PartSys && PropertyThatChanged)
	{
		PartSys->PostEditChangeProperty(PropertyChangedEvent);
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleBeamSource::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL	bFound	= FALSE;

	for (INT i = 0; i < PSysComp->InstanceParameters.Num(); i++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters(i));
		
		if (Param->Name == SourceName)
		{
			bFound	=	TRUE;
			break;
		}
	}

	if (!bFound)
	{
		INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters(NewParamIndex).Name		= SourceName;
		PSysComp->InstanceParameters(NewParamIndex).ParamType	= PSPT_Actor;
		PSysComp->InstanceParameters(NewParamIndex).Actor		= NULL;
	}
}

/**
 *	Retrieve the ParticleSysParams associated with this module.
 *
 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
 */
void UParticleModuleBeamSource::GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList)
{
	if (SourceMethod == PEB2STM_Actor)
	{
		ParticleSysParamList.AddItem(FString::Printf(TEXT("BeamSource : Actor: %s\n"), *(SourceName.ToString())));
	}
}

void UParticleModuleBeamSource::GetDataPointers(FParticleEmitterInstance* Owner, 
	const BYTE* ParticleBase, INT& CurrentOffset, 
	FBeamParticleSourceTargetPayloadData*& ParticleSource,
	FBeamParticleSourceBranchPayloadData*& BranchSource)
{
	FParticleBeam2EmitterInstance* BeamInst = CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (BeamInst)
	{
		UParticleModuleTypeDataBeam2*	BeamTD	= BeamInst->BeamTypeData;
		if (BeamTD)
		{
			if (SourceMethod == PEB2STM_Particle)
			{
				PARTICLE_ELEMENT(FBeamParticleSourceTargetPayloadData, LocalParticleSource);
				ParticleSource	= &LocalParticleSource;
			}
			if (BeamTD->BeamMethod == PEB2M_Branch)
			{
				PARTICLE_ELEMENT(FBeamParticleSourceBranchPayloadData, LocalBranchSource);
				BranchSource	= &LocalBranchSource;
			}
		}
	}
}

UBOOL UParticleModuleBeamSource::ResolveSourceData(FParticleBeam2EmitterInstance* BeamInst, 
	FBeam2TypeDataPayload* BeamData, const BYTE* ParticleBase, INT& Offset, INT	ParticleIndex,
	UBOOL bSpawning, FBeamParticleModifierPayloadData* ModifierData)
{
	UBOOL	bResult	= FALSE;

	FBaseParticle& Particle	= *((FBaseParticle*) ParticleBase);

	FBeamParticleSourceBranchPayloadData* BranchSource		= NULL;
	FBeamParticleSourceTargetPayloadData* ParticleSource	= NULL;
	GetDataPointers(BeamInst, ParticleBase, Offset, ParticleSource, BranchSource);

	if ((bSpawning == TRUE) || (bLockSource == FALSE))
	{
		// Resolve the source point...
		UBOOL bSetSource = FALSE;
		switch (SourceMethod)
		{
		case PEB2STM_UserSet:
			// User-set points are utilized directly.
			if (BeamInst->UserSetSourceArray.Num() > 0)
			{
				if (ParticleIndex >= BeamInst->UserSetSourceArray.Num())
				{
					//@todo. How to handle this situation???
					BeamData->SourcePoint	= BeamInst->UserSetSourceArray(0);
				}
				else
				{
					BeamData->SourcePoint	= BeamInst->UserSetSourceArray(ParticleIndex);
				}
				bSetSource	= TRUE;
			}
			break;
		case PEB2STM_Emitter:
			// The position of the owner component is the source
			BeamData->SourcePoint	= BeamInst->Component->LocalToWorld.GetOrigin();
			bSetSource				= TRUE;
			break;
		case PEB2STM_Particle:
			{
				if (BeamInst->BeamTypeData->BeamMethod == PEB2M_Branch)
				{
					// Branching beam - resolve the source emitter if needed
					if (BeamInst->SourceEmitter == NULL)
					{
						BeamInst->ResolveSource();
					}

					if (BeamInst->SourceEmitter)
					{
						if (bSpawning)
						{
							// Pick a particle index...
						}

						//@todo. fill this in correctly...
						BeamData->SourcePoint	= BeamInst->SourceEmitter->Component->LocalToWorld.GetOrigin();
						bSetSource = TRUE;
					}
				}
			}
			break;
		case PEB2STM_Actor:
			if (SourceName != NAME_None)
			{
				BeamInst->ResolveSource();
				// Use the actor position as the source
				if (BeamInst->SourceActor)
				{
					BeamData->SourcePoint	= BeamInst->SourceActor->LocalToWorld().GetOrigin();
					bSetSource = TRUE;
				}
			}
			break;
		}

		if (bSetSource == FALSE)
		{
			// If the source hasn't been set at this point, assume that we are using
			// the Source distribution.
			if (bSourceAbsolute)
			{
				// Use the value as a world space position
				BeamData->SourcePoint	= Source.GetValue(BeamInst->EmitterTime, BeamInst->Component);
			}
			else
			{
				// Use the value as a local space position.
				BeamData->SourcePoint	= BeamInst->Component->LocalToWorld.TransformFVector(
					Source.GetValue(BeamInst->EmitterTime, BeamInst->Component));
			}
		}
	}

	if ((bSpawning == TRUE) || (bLockSourceTangent == FALSE))
	{
		// If we are spawning and the source tangent is not locked, resolve it
		UBOOL bSetSourceTangent = FALSE;
		switch (SourceTangentMethod)
		{
		case PEB2STTM_Direct:
			// Use the emitter direction as the tangent
			BeamData->SourceTangent	= BeamInst->Component->LocalToWorld.GetAxis(0);
			bSetSourceTangent		= TRUE;
			break;
		case PEB2STTM_UserSet:
			// Use the user-set tangent directly
			if (BeamInst->UserSetSourceTangentArray.Num() > 0)
			{
				if (ParticleIndex >= BeamInst->UserSetSourceTangentArray.Num())
				{
					//@todo. How to handle this situation???
					BeamData->SourceTangent	= BeamInst->UserSetSourceTangentArray(0);
				}
				else
				{
					BeamData->SourceTangent	= BeamInst->UserSetSourceTangentArray(ParticleIndex);
				}
				bSetSourceTangent	= TRUE;
			}
			break;
		case PEB2STTM_Distribution:
			// Use the tangent contained in the distribution
			BeamData->SourceTangent	= SourceTangent.GetValue(Particle.RelativeTime, BeamInst->Component);
			bSetSourceTangent		= TRUE;
			break;
		case PEB2STTM_Emitter:
			// Use the emitter direction as the tangent
			BeamData->SourceTangent	= BeamInst->Component->LocalToWorld.GetAxis(0);
			bSetSourceTangent		= TRUE;
			break;
		}

		if (bSetSourceTangent == FALSE)
		{
			// By default, use the distribution. This will allow artists an easier setup phase...
//			BeamData->SourceTangent	= BeamInst->Component->LocalToWorld.GetAxis(0);
			BeamData->SourceTangent	= SourceTangent.GetValue(Particle.RelativeTime, BeamInst->Component);
			if (bSourceAbsolute == FALSE)
			{
				// If not tagged as absolute, transform it to world space
				BeamData->SourceTangent	= BeamInst->Component->LocalToWorld.TransformNormal(BeamData->SourceTangent);
			}
		}
	}

	if ((bSpawning == TRUE) || (bLockSourceStength == FALSE))
	{
		// If we are spawning and the source strength is not locked, resolve it
		UBOOL bSetSourceStrength = FALSE;
		if (SourceTangentMethod == PEB2STTM_UserSet)
		{
			if (BeamInst->UserSetSourceStrengthArray.Num() > 0)
			{
				if (ParticleIndex >= BeamInst->UserSetSourceStrengthArray.Num())
				{
					//@todo. How to handle this situation???
					BeamData->SourceStrength	= BeamInst->UserSetSourceStrengthArray(0);
				}
				else
				{
					BeamData->SourceStrength	= BeamInst->UserSetSourceStrengthArray(ParticleIndex);
				}
				bSetSourceStrength	= TRUE;
			}
		}

		if (!bSetSourceStrength)
		{
			BeamData->SourceStrength	= SourceStrength.GetValue(Particle.RelativeTime, BeamInst->Component);
		}
	}

	// For now, assume it worked...
	bResult	= TRUE;

	return bResult;
}

/*-----------------------------------------------------------------------------
	UParticleModuleBeamTarget implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleBeamTarget);

void UParticleModuleBeamTarget::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (!BeamInst)
	{
		return;
	}
	UParticleSystemComponent*		Component	= Owner->Component;
	UParticleModuleTypeDataBeam2*	BeamTD		= BeamInst->BeamTypeData;

	SPAWN_INIT;

	FBeam2TypeDataPayload*	BeamData			= NULL;
	FVector*				InterpolatedPoints	= NULL;
	FLOAT*					NoiseRate			= NULL;
	FLOAT*					NoiseDelta			= NULL;
	FVector*				TargetNoisePoints	= NULL;
	FVector*				NextNoisePoints		= NULL;
	FLOAT*					TaperValues			= NULL;
	FLOAT*					NoiseDistanceScale	= NULL;
	FBeamParticleModifierPayloadData* SourceModifier = NULL;
	FBeamParticleModifierPayloadData* TargetModifier = NULL;

	INT						TempOffset	= BeamInst->TypeDataOffset;
	BeamInst->BeamTypeData->GetDataPointers(Owner, ParticleBase, TempOffset, BeamData, 
		InterpolatedPoints, NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, 
		TaperValues, NoiseDistanceScale, SourceModifier, TargetModifier);
	CurrentOffset	= TempOffset;

	ResolveTargetData(BeamInst, BeamData, ParticleBase, Offset, BeamInst->ActiveParticles, TRUE, TargetModifier);
}

void UParticleModuleBeamTarget::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if (bLockTarget && bLockTargetTangent && bLockTargetStength)
	{
		return;
	}

	FParticleBeam2EmitterInstance*	BeamInst	= CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (!BeamInst)
	{
		return;
	}
	UParticleSystemComponent*		Component	= Owner->Component;
	UParticleModuleTypeDataBeam2*	BeamTD		= BeamInst->BeamTypeData;

	BEGIN_UPDATE_LOOP;
	{
		FBeam2TypeDataPayload*	BeamData			= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;
		FLOAT*					NoiseDistanceScale	= NULL;
		FBeamParticleModifierPayloadData* SourceModifier = NULL;
		FBeamParticleModifierPayloadData* TargetModifier = NULL;

		INT						TempOffset	= BeamInst->TypeDataOffset;
		BeamTD->GetDataPointers(Owner, ParticleBase, TempOffset, BeamData, InterpolatedPoints, 
			NoiseRate, NoiseDelta, TargetNoisePoints, NextNoisePoints, TaperValues,
			NoiseDistanceScale, SourceModifier, TargetModifier);

		ResolveTargetData(BeamInst, BeamData, ParticleBase, Offset, i, FALSE, TargetModifier);
	}
	END_UPDATE_LOOP;
}

void UParticleModuleBeamTarget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UParticleSystem* PartSys = CastChecked<UParticleSystem>(GetOuter());
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PartSys && PropertyThatChanged)
	{
		PartSys->PostEditChangeProperty(PropertyChangedEvent);
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleBeamTarget::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL	bFound	= FALSE;

	for (INT i = 0; i < PSysComp->InstanceParameters.Num(); i++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters(i));
		
		if (Param->Name == TargetName)
		{
			bFound	=	TRUE;
			break;
		}
	}

	if (!bFound)
	{
		INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters(NewParamIndex).Name		= TargetName;
		PSysComp->InstanceParameters(NewParamIndex).ParamType	= PSPT_Actor;
		PSysComp->InstanceParameters(NewParamIndex).Actor		= NULL;
	}
}

/**
 *	Retrieve the ParticleSysParams associated with this module.
 *
 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
 */
void UParticleModuleBeamTarget::GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList)
{
	if (TargetMethod == PEB2STM_Actor)
	{
		ParticleSysParamList.AddItem(FString::Printf(TEXT("BeamTarget : Actor: %s\n"), *(TargetName.ToString())));
	}
}

void UParticleModuleBeamTarget::GetDataPointers(FParticleEmitterInstance* Owner, const BYTE* ParticleBase, 
	INT& CurrentOffset, FBeamParticleSourceTargetPayloadData*& ParticleSource)
{
	FParticleBeam2EmitterInstance* BeamInst = CastEmitterInstance<FParticleBeam2EmitterInstance>(Owner);
	if (BeamInst)
	{
		UParticleModuleTypeDataBeam2*	BeamTD	= BeamInst->BeamTypeData;
		if (BeamTD)
		{
			if (TargetMethod == PEB2STM_Particle)
			{
				PARTICLE_ELEMENT(FBeamParticleSourceTargetPayloadData, LocalParticleSource);
				ParticleSource	= &LocalParticleSource;
			}
		}
	}
}
						
UBOOL UParticleModuleBeamTarget::ResolveTargetData(FParticleBeam2EmitterInstance* BeamInst, 
	FBeam2TypeDataPayload* BeamData, const BYTE* ParticleBase, INT& CurrentOffset, INT	ParticleIndex, UBOOL bSpawning, 
	FBeamParticleModifierPayloadData* ModifierData)
{
	UBOOL	bResult	= FALSE;

	FBaseParticle& Particle	= *((FBaseParticle*) ParticleBase);

	FBeamParticleSourceTargetPayloadData* ParticleSource	= NULL;
	GetDataPointers(BeamInst, ParticleBase, CurrentOffset, ParticleSource);

	if ((bSpawning == TRUE) || (bLockTarget == FALSE))
	{
		// Resolve the source point...
		UBOOL bSetTarget = FALSE;

		if (BeamInst->BeamTypeData->BeamMethod	== PEB2M_Distance)
		{
			// Set the particle target based on the distance
			FLOAT	Distance		= BeamInst->BeamTypeData->Distance.GetValue(Particle.RelativeTime, BeamInst->Component);
			if (Abs(Distance) < KINDA_SMALL_NUMBER)
			{
				Distance	= 0.001f;
			}
			FVector	Direction		= BeamInst->Component->LocalToWorld.GetAxis(0);
			Direction.Normalize();
			BeamData->TargetPoint	= BeamData->SourcePoint + Direction * Distance;
			bSetTarget				= TRUE;
		}

		if (bSetTarget == FALSE)
		{
			switch (TargetMethod)
			{
			case PEB2STM_UserSet:
				if (BeamInst->UserSetTargetArray.Num() > 0)
				{
					if (ParticleIndex >= BeamInst->UserSetTargetArray.Num())
					{
						//@todo. How to handle this situation???
						BeamData->TargetPoint	= BeamInst->UserSetTargetArray(0);
					}
					else
					{
						BeamData->TargetPoint	= BeamInst->UserSetTargetArray(ParticleIndex);
					}
					bSetTarget	= TRUE;
				}
				break;
			case PEB2STM_Emitter:
				//@todo. Fill in this case...
				break;
			case PEB2STM_Particle:
				//@todo. Fill in this case...
				break;
			case PEB2STM_Actor:
				if (TargetName != NAME_None)
				{
					BeamInst->ResolveTarget();
					if (BeamInst->TargetActor)
					{
						BeamData->TargetPoint	= BeamInst->TargetActor->LocalToWorld().GetOrigin();
						bSetTarget = TRUE;
					}
				}
				break;
			}
		}

		if (bSetTarget == FALSE)
		{
			if (bTargetAbsolute)
			{
				BeamData->TargetPoint	= Target.GetValue(BeamInst->EmitterTime, BeamInst->Component);
			}
			else
			{
				BeamData->TargetPoint	= BeamInst->Component->LocalToWorld.TransformFVector(
					Target.GetValue(BeamInst->EmitterTime, BeamInst->Component));
			}
		}
	}

	if ((bSpawning == TRUE) || (bLockTargetTangent == FALSE))
	{
		// Resolve the Target tangent
		UBOOL bSetTargetTangent = FALSE;
		switch (TargetTangentMethod)
		{
		case PEB2STTM_Direct:
			BeamData->TargetTangent	= BeamInst->Component->LocalToWorld.GetAxis(0);
			bSetTargetTangent		= TRUE;
			break;
		case PEB2STTM_UserSet:
			if (BeamInst->UserSetTargetTangentArray.Num() > 0)
			{
				if (ParticleIndex >= BeamInst->UserSetTargetTangentArray.Num())
				{
					//@todo. How to handle this situation???
					BeamData->TargetTangent	= BeamInst->UserSetTargetTangentArray(0);
				}
				else
				{
					BeamData->TargetTangent	= BeamInst->UserSetTargetTangentArray(ParticleIndex);
				}
				bSetTargetTangent	= TRUE;
			}
			break;
		case PEB2STTM_Distribution:
			BeamData->TargetTangent	= TargetTangent.GetValue(Particle.RelativeTime, BeamInst->Component);
			bSetTargetTangent		= TRUE;
			break;
		case PEB2STTM_Emitter:
			BeamData->TargetTangent	= BeamInst->Component->LocalToWorld.GetAxis(0);
			bSetTargetTangent		= TRUE;
			break;
		}

		if (bSetTargetTangent == FALSE)
		{
//			BeamData->TargetTangent	= BeamInst->Component->LocalToWorld.GetAxis(0);
			BeamData->TargetTangent	= TargetTangent.GetValue(Particle.RelativeTime, BeamInst->Component);
			if (bTargetAbsolute == FALSE)
			{
				BeamData->TargetTangent	= BeamInst->Component->LocalToWorld.TransformNormal(BeamData->TargetTangent);
			}
		}
	}

	if ((bSpawning == TRUE) || (bLockTargetStength == FALSE))
	{
		// Resolve the Target strength	
		UBOOL bSetTargetStrength = FALSE;
		if (TargetTangentMethod == PEB2STTM_UserSet)
		{
			if (BeamInst->UserSetTargetStrengthArray.Num() > 0)
			{
				if (ParticleIndex >= BeamInst->UserSetTargetStrengthArray.Num())
				{
					//@todo. How to handle this situation???
					BeamData->TargetStrength	= BeamInst->UserSetTargetStrengthArray(0);
				}
				else
				{
					BeamData->TargetStrength	= BeamInst->UserSetTargetStrengthArray(ParticleIndex);
				}
				bSetTargetStrength	= TRUE;
			}
		}

		if (!bSetTargetStrength)
		{
			BeamData->TargetStrength	= TargetStrength.GetValue(Particle.RelativeTime, BeamInst->Component);
		}
	}

	// For now, assume it worked...
	bResult	= TRUE;

	return bResult;
}
