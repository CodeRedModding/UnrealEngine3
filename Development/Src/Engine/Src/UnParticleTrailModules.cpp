/*=============================================================================
	UnParticleTrailModules.cpp: Particle module implementations for trails.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UParticleModuleTrailBase);

/*-----------------------------------------------------------------------------
	UParticleModuleTrailSource implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTrailSource);
/***
var(Source)									ETrail2SourceMethod				SourceMethod;
var(Source)		export noclear				name							SourceName;
var(Source)		export noclear				distributionfloat				SourceStrength;
var(Source)									bool							bLockSourceStength;
***/
void UParticleModuleTrailSource::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleTrail2EmitterInstance*	TrailInst = CastEmitterInstance<FParticleTrail2EmitterInstance>(Owner);
	if (!TrailInst)
	{
		return;
	}

	UParticleSystemComponent*		Component	= Owner->Component;
	UParticleModuleTypeDataTrail2*	TrailTD		= TrailInst->TrailTypeData;

	SPAWN_INIT;

	FTrail2TypeDataPayload*	TrailData			= NULL;
	FLOAT*					TaperData			= NULL;

	INT	TempOffset	= TrailInst->TypeDataOffset;
	TrailInst->TrailTypeData->GetDataPointers(Owner, ParticleBase, TempOffset, 
		TrailData, TaperData);

	// Clear the initial data flags
	TrailData->Flags	= 0;
	TrailData->Velocity	= FVector(1.0f, 0.0f, 0.0f);
	TrailData->Tangent	= FVector(1.0f, 0.0f, 0.0f);

	switch (SourceMethod)
	{
	case PET2SRCM_Particle:
		{
			INT TempOffset2	= TrailInst->TrailModule_Source_Offset;
			FTrailParticleSourcePayloadData* ParticleSource	= NULL;
			GetDataPointers(TrailInst, (const BYTE*)&Particle, TempOffset2, ParticleSource);
			check(ParticleSource);

			ParticleSource->ParticleIndex = -1;
		}
		break;
	}

	//warnf( TEXT( "TrailInst->ActiveParticles: %d"), TrailInst->ActiveParticles ); 
	ResolveSourceData(TrailInst, ParticleBase, TrailData, Offset, TrailInst->ActiveParticles, TRUE);
}

UINT UParticleModuleTrailSource::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return (SourceMethod == PET2SRCM_Particle) ? sizeof(FTrailParticleSourcePayloadData) : 0;
}

void UParticleModuleTrailSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
//	SourceOffsetCount
//	SourceOffsetDefaults
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetFName() == FName(TEXT("SourceOffsetCount")))
		{
			if (SourceOffsetDefaults.Num() > 0)
			{
				if (SourceOffsetDefaults.Num() < SourceOffsetCount)
				{
					// Add additional slots
					SourceOffsetDefaults.InsertZeroed(SourceOffsetDefaults.Num(), SourceOffsetCount - SourceOffsetDefaults.Num());
				}
				else
				if (SourceOffsetDefaults.Num() > SourceOffsetCount)
				{
					// Remove the required slots
					INT	RemoveIndex	= SourceOffsetCount ? (SourceOffsetCount - 1) : 0;
					SourceOffsetDefaults.Remove(RemoveIndex, SourceOffsetDefaults.Num() - SourceOffsetCount);
				}
			}
			else
			{
				if (SourceOffsetCount > 0)
				{
					// Add additional slots
					SourceOffsetDefaults.InsertZeroed(0, SourceOffsetCount);
				}
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleTrailSource::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	switch (SourceMethod)
	{
	case PET2SRCM_Actor:
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
		break;
	}
}

/**
 *	Retrieve the ParticleSysParams associated with this module.
 *
 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
 */
void UParticleModuleTrailSource::GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList)
{
	if (SourceMethod == PET2SRCM_Actor)
	{
		ParticleSysParamList.AddItem(FString::Printf(TEXT("TrailSource: Actor: %s\n"), *(SourceName.ToString())));
	}
}

void UParticleModuleTrailSource::GetDataPointers(FParticleTrail2EmitterInstance* TrailInst, 
	const BYTE* ParticleBase, INT& CurrentOffset, FTrailParticleSourcePayloadData*& ParticleSource)
{
	if (SourceMethod == PET2SRCM_Particle)
	{
		PARTICLE_ELEMENT(FTrailParticleSourcePayloadData, LocalParticleSource);
		ParticleSource	= &LocalParticleSource;
	}
}

void UParticleModuleTrailSource::GetDataPointerOffsets(FParticleTrail2EmitterInstance* TrailInst, 
	const BYTE* ParticleBase, INT& CurrentOffset, INT& ParticleSourceOffset)
{
	ParticleSourceOffset = -1;
	if (SourceMethod == PET2SRCM_Particle)
	{
		ParticleSourceOffset = CurrentOffset;
	}
}

UBOOL UParticleModuleTrailSource::ResolveSourceData(FParticleTrail2EmitterInstance* TrailInst, 
	const BYTE* ParticleBase, FTrail2TypeDataPayload* TrailData, 
	INT& CurrentOffset, INT	ParticleIndex, UBOOL bSpawning)
{
	UBOOL	bResult	= FALSE;

	FBaseParticle& Particle	= *((FBaseParticle*) ParticleBase);

	if (bSpawning == TRUE)
	{
		ResolveSourcePoint(TrailInst, Particle, *TrailData, Particle.Location, TrailData->Tangent);
	}

	// For now, assume it worked...
	bResult	= TRUE;

	return bResult;
}

UBOOL UParticleModuleTrailSource::ResolveSourcePoint(FParticleTrail2EmitterInstance* TrailInst,
	FBaseParticle& Particle, FTrail2TypeDataPayload& TrailData, FVector& Position, FVector& Tangent)
{
	// Resolve the source point...
	switch (SourceMethod)
	{
	case PET2SRCM_Particle:
		{
			if (TrailInst->SourceEmitter == NULL)
			{
				TrailInst->ResolveSource();
				// Is this the first time?
			}

			UBOOL bFirstSelect	= FALSE;
			if (TrailInst->SourceEmitter)
			{
				INT	Offset	= TrailInst->TrailModule_Source_Offset;
				FTrailParticleSourcePayloadData* ParticleSource	= NULL;
				GetDataPointers(TrailInst, (const BYTE*)&Particle, Offset, ParticleSource);
				check(ParticleSource);

				if (ParticleSource->ParticleIndex == -1)
				{
					INT Index = 0;

					switch (SelectionMethod)
					{
					case EPSSM_Random:
						{
							Index = appTrunc(appFrand() * TrailInst->SourceEmitter->ActiveParticles);
						}
						break;
					case EPSSM_Sequential:
						{
							Index = ++(TrailInst->LastSelectedParticleIndex);
							if (Index >= TrailInst->SourceEmitter->ActiveParticles)
							{
								Index = 0;
							}
						}
						break;
					}
					ParticleSource->ParticleIndex	= Index;
					bFirstSelect	= TRUE;
				}

				// Grab the particle
				FBaseParticle* Source	= TrailInst->SourceEmitter->GetParticle(ParticleSource->ParticleIndex);
				if (Source)
				{
					Position	= Source->Location;
				}
				else
				{
					Position	= TrailInst->SourceEmitter->Component->LocalToWorld.GetOrigin();
				}

				if (SourceOffsetCount > 0)
				{
					FVector	TrailOffset = ResolveSourceOffset(TrailInst, Particle, TrailData);

					// Need to determine the offset relative to the particle orientation...

					Position	+= TrailInst->SourceEmitter->Component->LocalToWorld.TransformNormal(TrailOffset);
				}

				if (bInheritRotation)
				{
				}

				if (Source)
				{
					Tangent		= Source->Location - Source->OldLocation;
				}
				else
				{
					Tangent		= TrailInst->SourceEmitter->Component->LocalToWorld.GetAxis(0);
				}
				Tangent.Normalize();

				if (bFirstSelect)
				{
					TrailInst->SourcePosition(TrailData.TrailIndex)	= Position;
				}
			}
		}
		break;
	case PET2SRCM_Actor:
		if (SourceName != NAME_None)
		{
			if (TrailInst->SourceActor == NULL)
			{
				TrailInst->ResolveSource();
			}

			if (TrailInst->SourceActor)
			{
				FVector	TrailOffset = ResolveSourceOffset(TrailInst, Particle, TrailData);
				Position	= TrailInst->SourceActor->LocalToWorld().TransformFVector(TrailOffset);
				Tangent		= TrailInst->SourceActor->LocalToWorld().GetAxis(0);
				Tangent.Normalize();

				//GWorld->GetGameInfo()->DrawDebugBox( Position, FVector(3,3,3), 0,0,255, TRUE );
			}
		}
		break;
	default:
		{
			Position	= TrailInst->Component->LocalToWorld.GetOrigin();
			if (SourceOffsetCount > 0)
			{
				FVector	TrailOffset = ResolveSourceOffset(TrailInst, Particle, TrailData);
				// Need to determine the offset relative to the particle orientation...
				Position	+= TrailInst->Component->LocalToWorld.TransformNormal(TrailOffset);
			}

			Tangent		= TrailInst->Component->LocalToWorld.GetAxis(0);
			Tangent.Normalize();
		}
	}

	TrailInst->LastSourcePosition(TrailData.TrailIndex) = Position;

	return TRUE;
}

FVector	UParticleModuleTrailSource::ResolveSourceOffset(FParticleTrail2EmitterInstance* TrailInst, 
	FBaseParticle& Particle, FTrail2TypeDataPayload& TrailData)
{
	FVector	TrailOffset(0.0f);

	if (TrailInst->SourceOffsets.Num() > TrailData.TrailIndex)
	{
		TrailOffset	= TrailInst->SourceOffsets(TrailData.TrailIndex);
	}
	else
	if (SourceOffsetDefaults.Num() > TrailData.TrailIndex)
	{
		TrailOffset	= SourceOffsetDefaults(TrailData.TrailIndex);
	}
	else
	if (TrailInst->SourceOffsets.Num() == 1)
	{
		// There is a single offset value... assume it's 0
		TrailOffset	= TrailInst->SourceOffsets(0);
	}
	else
	if (SourceOffsetDefaults.Num() == 1)
	{
		TrailOffset	= SourceOffsetDefaults(0);
	}

	return TrailOffset;
}

/**
 *	Retrieve the SourceOffset for the given trail index.
 *	Currently, this is only intended for use by Ribbon emitters
 *
 *	@param	InTrailIdx			The index of the trail whose offset is being retrieved
 *	@param	InEmitterInst		The EmitterInstance requesting the SourceOffset
 *	@param	OutSourceOffset		The source offset for the trail of interest
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not (including no offset)
 */
UBOOL UParticleModuleTrailSource::ResolveSourceOffset(INT InTrailIdx, FParticleEmitterInstance* InEmitterInst, FVector& OutSourceOffset)
{
	// For now, we are only supporting the default values (for ribbon emitters)
	if (InTrailIdx < SourceOffsetDefaults.Num())
	{
		OutSourceOffset = SourceOffsetDefaults(InTrailIdx);
		return TRUE;
	}
	return FALSE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleTrailSpawn implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTrailSpawn);

void UParticleModuleTrailSpawn::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleTrailSpawn::PostLoad()
{
	Super::PostLoad();
	if (GIsGame && !IsTemplate())
	{
		if (SpawnDistanceMap == NULL)
		{
			UParticleModuleTrailSpawn* DefModule = Cast<UParticleModuleTrailSpawn>(StaticClass()->GetDefaultObject());
			check(DefModule);
			SpawnDistanceMap = DefModule->SpawnDistanceMap;
		}
	}
}

UINT UParticleModuleTrailSpawn::GetSpawnCount(FParticleTrail2EmitterInstance* TrailInst, FLOAT DeltaTime)
{
	UINT	Count	= 0;

	UBOOL	bFound		= FALSE;

//	FVector	LastUpdate;
//	LastUpdate = *((FVector*)(ParticleBase + TrailInst->TrailModule_Spawn_Offset));	

	FLOAT	Travelled	= TrailInst->SourceDistanceTravelled(0);

	// Determine the number of times to spawn the max
	INT		MaxCount	= appFloor(Travelled / SpawnDistanceMap->MaxInput);

	Count	+= MaxCount * appTrunc(SpawnDistanceMap->MaxOutput);
	
	FLOAT	Portion		= Travelled - (MaxCount * SpawnDistanceMap->MaxInput);
	if (Portion >= SpawnDistanceMap->MinInput)
	{
		FLOAT	Value		= SpawnDistanceMap->GetValue(Portion);
		INT		SmallCount	= appTrunc(Value);
		TrailInst->SourceDistanceTravelled(0) = Portion - SmallCount * SpawnDistanceMap->MinInput;
		Count	+= SmallCount;
	}
	else
	{
	}

	return Count;
}

/*-----------------------------------------------------------------------------
	UParticleModuleTrailTaper implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTrailTaper);

void UParticleModuleTrailTaper::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
}

void UParticleModuleTrailTaper::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
}

UINT UParticleModuleTrailTaper::RequiredBytes(FParticleEmitterInstance* Owner)
{
	FParticleTrail2EmitterInstance* TrailInst	= CastEmitterInstance<FParticleTrail2EmitterInstance>(Owner);
	if (TrailInst)
	{
		INT	TessFactor	= TrailInst->TrailTypeData->TessellationFactor ? TrailInst->TrailTypeData->TessellationFactor : 1;
		// Store the taper factor for each interpolation point
		return (sizeof(FLOAT) * (TessFactor + 1));
	}
	return 0;
}

void UParticleModuleTrailTaper::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataTrail2 implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTypeDataTrail2);

UINT UParticleModuleTypeDataTrail2::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FTrail2TypeDataPayload);
}

void UParticleModuleTypeDataTrail2::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FParticleEmitterInstance* UParticleModuleTypeDataTrail2::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	SetToSensibleDefaults(InEmitterParent);
	FParticleEmitterInstance* Instance;
	
	Instance = new FParticleTrail2EmitterInstance();
	check(Instance);

	Instance->InitParameters(InEmitterParent, InComponent);

	return Instance;
}

void UParticleModuleTypeDataTrail2::GetDataPointers(FParticleEmitterInstance* Owner, 
	const BYTE* ParticleBase, INT& CurrentOffset, FTrail2TypeDataPayload*& TrailData, FLOAT*& TaperValues)
{
	PARTICLE_ELEMENT(FTrail2TypeDataPayload, Data);
	TrailData	= &Data;
}

void UParticleModuleTypeDataTrail2::GetDataPointerOffsets(FParticleEmitterInstance* Owner, 
	const BYTE* ParticleBase, INT& CurrentOffset, INT& TrailDataOffset, INT& TaperValuesOffset)
{
	TrailDataOffset = CurrentOffset;
	TaperValuesOffset = -1;
}

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataRibbon implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTypeDataRibbon);

UINT UParticleModuleTypeDataRibbon::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FRibbonTypeDataPayload);
}

void UParticleModuleTypeDataRibbon::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged->GetName() == TEXT("MaxTessellationBetweenParticles"))
	{
		if (MaxTessellationBetweenParticles < 0)
		{
			MaxTessellationBetweenParticles = 0;
		}
	}
	else if (PropertyThatChanged->GetName() == TEXT("SheetsPerTrail"))
	{
		if (SheetsPerTrail <= 0)
		{
			SheetsPerTrail = 1;
		}
	}
	else if (PropertyThatChanged->GetName() == TEXT("MaxTrailCount"))
	{
		if (MaxTrailCount <= 0)
		{
			MaxTrailCount = 1;
		}
	}
	else if (PropertyThatChanged->GetName() == TEXT("MaxParticleInTrailCount"))
	{
		if (MaxParticleInTrailCount < 0)
		{
			MaxParticleInTrailCount = 0;
		}
	}
}

FParticleEmitterInstance* UParticleModuleTypeDataRibbon::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance* Instance = new FParticleRibbonEmitterInstance();
	check(Instance);
	Instance->InitParameters(InEmitterParent, InComponent);
	return Instance;
}

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataAnimTrail implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTypeDataAnimTrail);

UINT UParticleModuleTypeDataAnimTrail::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FAnimTrailTypeDataPayload);
}

void UParticleModuleTypeDataAnimTrail::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FParticleEmitterInstance* UParticleModuleTypeDataAnimTrail::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance* Instance = new FParticleAnimTrailEmitterInstance();
	check(Instance);
	Instance->InitParameters(InEmitterParent, InComponent);
	return Instance;
}
