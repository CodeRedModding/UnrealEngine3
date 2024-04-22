/*=============================================================================
	ParticleModules_Parameter.cpp: 
	Parameter-related particle module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineMaterialClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleParameterBase);

/*-----------------------------------------------------------------------------
	UParticleModuleParameterDynamic implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleParameterDynamic);

/** Flags for optimizing update */
enum EDynamicParameterUpdateFlags
{
	/** No Update required */
	EDPU_UPDATE_NONE	= 0x00,
	/** Param1 requires an update */
	EDPU_UPDATE_0		= 0x01,
	/** Param2 requires an update */
	EDPU_UPDATE_1		= 0x02,
	/** Param3 requires an update */
	EDPU_UPDATE_2		= 0x04,
	/** Param4 requires an update */
	EDPU_UPDATE_3		= 0x08,
	/** Param1 and Param2 require an update */
	EDPU_UPDATE_01		= EDPU_UPDATE_0	| EDPU_UPDATE_1,
	/** Param1, Param2, and Param3 require an update */
	EDPU_UPDATE_012		= EDPU_UPDATE_0	| EDPU_UPDATE_1	| EDPU_UPDATE_2,
	/** ALL require an update */
	EDPU_UPDATE_ALL		= EDPU_UPDATE_0 | EDPU_UPDATE_1 | EDPU_UPDATE_2 | EDPU_UPDATE_3
};

FORCEINLINE INT ParticleDynamicParameter_GetIndexFromFlag(INT InFlags)
{
	switch (InFlags)
	{
	case EDPU_UPDATE_0:
		return 0;
	case EDPU_UPDATE_1:
		return 1;
	case EDPU_UPDATE_2:
		return 2;
	case EDPU_UPDATE_3:
		return 3;
	}
	return INDEX_NONE;
}

/**
 *	Called after an object has been loaded
 */
void UParticleModuleParameterDynamic::PostLoad()
{
	Super::PostLoad();
	UpdateUsageFlags();
}

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleParameterDynamic::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SpawnEx(Owner, Offset, SpawnTime, NULL);
}

/**
 *	Extended version of spawn, allows for using a random stream for distribution value retrieval
 *
 *	@param	Owner				The particle emitter instance that is spawning
 *	@param	Offset				The offset to the modules payload data
 *	@param	SpawnTime			The time of the spawn
 *	@param	InRandomStream		The random stream to use for retrieving random values
 */
void UParticleModuleParameterDynamic::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	{
		PARTICLE_ELEMENT(FEmitterDynamicParameterPayload, DynamicPayload);
		DynamicPayload.DynamicParameterValue[0] = GetParameterValue(DynamicParams(0), Particle, Owner, InRandomStream);
		DynamicPayload.DynamicParameterValue[1] = GetParameterValue(DynamicParams(1), Particle, Owner, InRandomStream);
		DynamicPayload.DynamicParameterValue[2] = GetParameterValue(DynamicParams(2), Particle, Owner, InRandomStream);
		DynamicPayload.DynamicParameterValue[3] = GetParameterValue(DynamicParams(3), Particle, Owner, InRandomStream);
#if WITH_MOBILE_RHI || WITH_EDITOR
		// Store the index to which dynamic parameter value is Time
		DynamicPayload.TimeIndex = ParticleDynamicParameter_GetTimeIndex(3);
#endif //WITH_MOBILE_RHI  || WITH_EDITOR
	}
}

/**
 *	Called on a particle that is being updated by its emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleParameterDynamic::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if (UpdateFlags == EDPU_UPDATE_NONE)
	{
		// Nothing to do here - they are all spawntime only
		return;
	}

	if ((Owner == NULL) || (Owner->ActiveParticles <= 0) || 
		(Owner->ParticleData == NULL) || (Owner->ParticleIndices == NULL))
	{
		return;
	}

	// 
	INT ParameterIndex = ParticleDynamicParameter_GetIndexFromFlag(UpdateFlags);

#if WITH_MOBILE_RHI || WITH_EDITOR
	// Retrieve the index to which dynamic parameter value is Time
	INT TimeIndex = ParticleDynamicParameter_GetTimeIndex(3);
#endif //WITH_MOBILE_RHI || WITH_EDITOR

	CONSOLE_PREFETCH(Owner->ParticleData + (Owner->ParticleIndices[0] * Owner->ParticleStride));
	CONSOLE_PREFETCH_NEXT_CACHE_LINE(Owner->ParticleData + (Owner->ParticleIndices[0] * Owner->ParticleStride));

	switch (UpdateFlags)
	{
	case EDPU_UPDATE_0:
	case EDPU_UPDATE_1:
	case EDPU_UPDATE_2:
	case EDPU_UPDATE_3:
		{
			// Only one parameter is updating...
			check(ParameterIndex != INDEX_NONE);
			FEmitterDynamicParameter& DynParam = DynamicParams(ParameterIndex);
			if (bUsesVelocity == FALSE)
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[ParameterIndex] = GetParameterValue_UserSet(DynParam, Particle, Owner, NULL);
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
			else
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[ParameterIndex] = GetParameterValue(DynParam, Particle, Owner, NULL);
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
		}
		break;
	case EDPU_UPDATE_01:
		{
			// Just 0 and 1 need to be updated...
			FEmitterDynamicParameter& DynParam0 = DynamicParams(0);
			FEmitterDynamicParameter& DynParam1 = DynamicParams(1);
			if (bUsesVelocity == FALSE)
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[0] = GetParameterValue_UserSet(DynParam0, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[1] = GetParameterValue_UserSet(DynParam1, Particle, Owner, NULL);
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
			else
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[0] = GetParameterValue(DynParam0, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[1] = GetParameterValue(DynParam1, Particle, Owner, NULL);
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
		}
		break;
	case EDPU_UPDATE_012:
		{
			// Just 0, 1 and 2 need to be updated...
			FEmitterDynamicParameter& DynParam0 = DynamicParams(0);
			FEmitterDynamicParameter& DynParam1 = DynamicParams(1);
			FEmitterDynamicParameter& DynParam2 = DynamicParams(2);
			if (bUsesVelocity == FALSE)
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[0] = GetParameterValue_UserSet(DynParam0, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[1] = GetParameterValue_UserSet(DynParam1, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[2] = GetParameterValue_UserSet(DynParam2, Particle, Owner, NULL);
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
			else
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[0] = GetParameterValue(DynParam0, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[1] = GetParameterValue(DynParam1, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[2] = GetParameterValue(DynParam2, Particle, Owner, NULL);
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
		}
		break;
	case EDPU_UPDATE_ALL:
		{
			FEmitterDynamicParameter& DynParam0 = DynamicParams(0);
			FEmitterDynamicParameter& DynParam1 = DynamicParams(1);
			FEmitterDynamicParameter& DynParam2 = DynamicParams(2);
			FEmitterDynamicParameter& DynParam3 = DynamicParams(3);
			if (bUsesVelocity == FALSE)
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[0] = GetParameterValue_UserSet(DynParam0, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[1] = GetParameterValue_UserSet(DynParam1, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[2] = GetParameterValue_UserSet(DynParam2, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[3] = GetParameterValue_UserSet(DynParam3, Particle, Owner, NULL);
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
			else
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[0] = GetParameterValue(DynParam0, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[1] = GetParameterValue(DynParam1, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[2] = GetParameterValue(DynParam2, Particle, Owner, NULL);
					DynamicPayload.DynamicParameterValue[3] = GetParameterValue(DynParam3, Particle, Owner, NULL);
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
		}
		break;
	default:
		{
			FEmitterDynamicParameter& DynParam0 = DynamicParams(0);
			FEmitterDynamicParameter& DynParam1 = DynamicParams(1);
			FEmitterDynamicParameter& DynParam2 = DynamicParams(2);
			FEmitterDynamicParameter& DynParam3 = DynamicParams(3);
			if (bUsesVelocity == FALSE)
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[0] = (UpdateFlags & EDPU_UPDATE_0) ? GetParameterValue_UserSet(DynParam0, Particle, Owner, NULL) : DynamicPayload.DynamicParameterValue[0];
					DynamicPayload.DynamicParameterValue[1] = (UpdateFlags & EDPU_UPDATE_1) ? GetParameterValue_UserSet(DynParam1, Particle, Owner, NULL) : DynamicPayload.DynamicParameterValue[1];
					DynamicPayload.DynamicParameterValue[2] = (UpdateFlags & EDPU_UPDATE_2) ? GetParameterValue_UserSet(DynParam2, Particle, Owner, NULL) : DynamicPayload.DynamicParameterValue[2];
					DynamicPayload.DynamicParameterValue[3] = (UpdateFlags & EDPU_UPDATE_3) ? GetParameterValue_UserSet(DynParam3, Particle, Owner, NULL) : DynamicPayload.DynamicParameterValue[3];
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
			else
			{
				BEGIN_UPDATE_LOOP;
				{
					FEmitterDynamicParameterPayload& DynamicPayload = *((FEmitterDynamicParameterPayload*)(ParticleBase + CurrentOffset));
					CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
					DynamicPayload.DynamicParameterValue[0] = (UpdateFlags & EDPU_UPDATE_0) ? GetParameterValue(DynParam0, Particle, Owner, NULL) : DynamicPayload.DynamicParameterValue[0];
					DynamicPayload.DynamicParameterValue[1] = (UpdateFlags & EDPU_UPDATE_1) ? GetParameterValue(DynParam1, Particle, Owner, NULL) : DynamicPayload.DynamicParameterValue[1];
					DynamicPayload.DynamicParameterValue[2] = (UpdateFlags & EDPU_UPDATE_2) ? GetParameterValue(DynParam2, Particle, Owner, NULL) : DynamicPayload.DynamicParameterValue[2];
					DynamicPayload.DynamicParameterValue[3] = (UpdateFlags & EDPU_UPDATE_3) ? GetParameterValue(DynParam3, Particle, Owner, NULL) : DynamicPayload.DynamicParameterValue[3];
#if WITH_MOBILE_RHI
					// Store the index to which dynamic parameter value is Time
					DynamicPayload.TimeIndex = TimeIndex;
#endif //WITH_MOBILE_RHI
				}
				END_UPDATE_LOOP;
			}
		}
		break;
	}
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleParameterDynamic::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FEmitterDynamicParameterPayload);
}

// For Cascade
/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleParameterDynamic::SetToSensibleDefaults(UParticleEmitter* Owner)
{
}

/** 
 *	PostEditChange...
 */
void UParticleModuleParameterDynamic::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdateUsageFlags();
}

/** 
 *	Fill an array with each Object property that fulfills the FCurveEdInterface interface.
 *
 *	@param	OutCurve	The array that should be filled in.
 */
void UParticleModuleParameterDynamic::GetCurveObjects(TArray<FParticleCurvePair>& OutCurves)
{
	FParticleCurvePair* NewCurve;

	for (INT ParamIndex = 0; ParamIndex < 4; ParamIndex++)
	{
		NewCurve = new(OutCurves) FParticleCurvePair;
		check(NewCurve);
		NewCurve->CurveObject = DynamicParams(ParamIndex).ParamValue.Distribution;
		NewCurve->CurveName = FString::Printf(TEXT("%s (DP%d)"), 
			*(DynamicParams(ParamIndex).ParamName.ToString()), ParamIndex);
	}
}

/**
 *	Retrieve the ParticleSysParams associated with this module.
 *
 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
 */
void UParticleModuleParameterDynamic::GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList)
{
}

/**
 *	Retrieve the distributions that use ParticleParameters in this module.
 *
 *	@param	ParticleParameterList	The list of ParticleParameter distributions to add to
 */
void UParticleModuleParameterDynamic::GetParticleParametersUtilized(TArray<FString>& ParticleParameterList)
{
}

/**
 *	Helper function for retriving the material from interface...
 */
UMaterial* UParticleModuleParameterDynamic_RetrieveMaterial(UMaterialInterface* InMaterialInterface)
{
	UMaterial* Material = Cast<UMaterial>(InMaterialInterface);
	UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(InMaterialInterface);
	UMaterialInstanceTimeVarying* MITV = Cast<UMaterialInstanceTimeVarying>(InMaterialInterface);

	if (MIC)
	{
		UMaterialInterface* Parent = MIC->Parent;
		Material = Cast<UMaterial>(Parent);
		while (!Material && Parent)
		{
			MIC = Cast<UMaterialInstanceConstant>(Parent);
			if (MIC)
			{
				Parent = MIC->Parent;
			}
			MITV = Cast<UMaterialInstanceTimeVarying>(Parent);
			if (MITV)
			{
				Parent = MITV->Parent;
			}

			Material = Cast<UMaterial>(Parent);
		}
	}

	if (MITV)
	{
		UMaterialInterface* Parent = MITV->Parent;
		Material = Cast<UMaterial>(Parent);
		while (!Material && Parent)
		{
			MIC = Cast<UMaterialInstanceConstant>(Parent);
			if (MIC)
			{
				Parent = MIC->Parent;
			}
			MITV = Cast<UMaterialInstanceTimeVarying>(Parent);
			if (MITV)
			{
				Parent = MITV->Parent;
			}

			Material = Cast<UMaterial>(Parent);
		}
	}

	return Material;
}

/**
 *	Helper function to find the DynamicParameter expression in a material
 */
UMaterialExpressionDynamicParameter* UParticleModuleParameterDynamic_GetDynamicParameterExpression(UMaterial* InMaterial, UBOOL bIsMeshEmitter)
{
	UMaterialExpressionDynamicParameter* DynParamExp = NULL;
	for (INT ExpIndex = 0; ExpIndex < InMaterial->Expressions.Num(); ExpIndex++)
	{
		if (bIsMeshEmitter == FALSE)
		{
			DynParamExp = Cast<UMaterialExpressionDynamicParameter>(InMaterial->Expressions(ExpIndex));
		}
		else
		{
			DynParamExp = Cast<UMaterialExpressionMeshEmitterDynamicParameter>(InMaterial->Expressions(ExpIndex));
		}

		if (DynParamExp != NULL)
		{
			break;
		}
	}

	return DynParamExp;
}

/**
 *	Update the parameter names with the given material...
 *
 *	@param	InMaterialInterface	Pointer to the material interface
 *
 */
void UParticleModuleParameterDynamic::UpdateParameterNames(UMaterialInterface* InMaterialInterface, UBOOL bIsMeshEmitter)
{
	UMaterial* Material = UParticleModuleParameterDynamic_RetrieveMaterial(InMaterialInterface);
	if (Material == NULL)
	{
		return;
	}

	// Check the expressions...
	UMaterialExpressionDynamicParameter* DynParamExp = UParticleModuleParameterDynamic_GetDynamicParameterExpression(Material, bIsMeshEmitter);
	if (DynParamExp == NULL)
	{
		return;
	}

	for (INT ParamIndex = 0; ParamIndex < 4; ParamIndex++)
	{
		DynamicParams(ParamIndex).ParamName = FName(*(DynParamExp->ParamNames(ParamIndex)));
	}
}

/**
 *	Refresh the module...
 */
void UParticleModuleParameterDynamic::RefreshModule(UInterpCurveEdSetup* EdSetup, UParticleEmitter* InEmitter, INT InLODLevel)
{
#if WITH_EDITOR
	// Find the material for this emitter...
	UParticleLODLevel* LODLevel = InEmitter->LODLevels((InLODLevel < InEmitter->LODLevels.Num()) ? InLODLevel : 0);
	if (LODLevel)
	{
		UBOOL bIsMeshEmitter = FALSE;
		if (LODLevel->TypeDataModule)
		{
			if (LODLevel->TypeDataModule->IsA(UParticleModuleTypeDataMesh::StaticClass()))
			{
				bIsMeshEmitter = TRUE;
			}
		}

		UMaterialInterface* MaterialInterface = LODLevel->RequiredModule ? LODLevel->RequiredModule->Material : NULL;
		if (MaterialInterface)
		{
			UpdateParameterNames(MaterialInterface, bIsMeshEmitter);
			for (INT ParamIndex = 0; ParamIndex < 4; ParamIndex++)
			{
				FString TempName = FString::Printf(TEXT("%s (DP%d)"), 
					*(DynamicParams(ParamIndex).ParamName.ToString()), ParamIndex);
				EdSetup->ChangeCurveName(
					DynamicParams(ParamIndex).ParamValue.Distribution, 
					TempName);
			}
		}
	}
#endif
}

/**
 *	Set the UpdatesFlags and bUsesVelocity
 */
void UParticleModuleParameterDynamic::UpdateUsageFlags()
{
#if !CONSOLE
	bUsesVelocity = FALSE;
	UpdateFlags = EDPU_UPDATE_ALL;
	for (INT Index = 0; Index < 4; Index++)
	{
		FEmitterDynamicParameter& DynParam = DynamicParams(Index);
		if (DynParam.bSpawnTimeOnly == TRUE)
		{
			UpdateFlags &= ~(1 << Index);
		}
		if ((DynParam.ValueMethod != EDPV_UserSet) || 
			(DynParam.bScaleVelocityByParamValue == TRUE))
		{
			bUsesVelocity = TRUE;
		}
	}

	// If it is none of the specially handled cases, see if there is a way to make it one...
	if (
		(UpdateFlags != EDPU_UPDATE_0) && (UpdateFlags != EDPU_UPDATE_1) && (UpdateFlags != EDPU_UPDATE_2) && (UpdateFlags != EDPU_UPDATE_3) && 
		(UpdateFlags != EDPU_UPDATE_01) && (UpdateFlags != EDPU_UPDATE_012) && (UpdateFlags != EDPU_UPDATE_ALL) && (UpdateFlags != EDPU_UPDATE_NONE)
		)
	{
		// See if any of the ones set to not update are constant
		for (INT Index = 0; Index < 4; Index++)
		{
			FEmitterDynamicParameter& DynParam = DynamicParams(Index);
			if ((DynParam.bSpawnTimeOnly == TRUE) && (DynParam.bScaleVelocityByParamValue == EDPV_UserSet))
			{
				// See what the distribution is...
				UDistributionFloatConstant* DistConstant = Cast<UDistributionFloatConstant>(DynParam.ParamValue.Distribution);
				if (DistConstant != NULL)
				{
					if (Index == 3)
					{
						if (UpdateFlags == EDPU_UPDATE_012)
						{
							// Don't bother setting it in this case as '012' is slightly faster than all
							continue;
						}
					}
					// It's constant, spawn-time only so it is safe to always update it.
					UpdateFlags &= (1 << Index);
				}
			}
		}
	}
#endif
}

/*-----------------------------------------------------------------------------
	UParticleModuleParameterDynamic_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleParameterDynamic_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleParameterDynamic_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	SpawnEx(Owner, Offset, SpawnTime, (Payload != NULL) ? &(Payload->RandomStream) : NULL);
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleParameterDynamic_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleParameterDynamic_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleParameterDynamic_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}
