/*=============================================================================
	ParticleModules_Material.cpp: 
	Material-related particle module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMaterialBase);

/*-----------------------------------------------------------------------------
	UParticleModuleMeshMaterial
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMeshMaterial);

	//## BEGIN PROPS ParticleModuleMeshMaterial
//	TArrayNoInit<class UMaterialInstance*> MeshMaterials;
	//## END PROPS ParticleModuleMeshMaterial

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleMeshMaterial::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{

}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number fo bytes the module needs per emitter instance.
 */
UINT UParticleModuleMeshMaterial::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	FParticleMeshEmitterInstance* MeshEmitInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
	if (MeshEmitInst == NULL)
	{
		return 0;
	}

	// Cheat and setup the emitter instance material array here...
	//debugf(TEXT("Set up the mesh material array here!!!"));

	if (Owner && bEnabled)
	{
		MeshEmitInst->CurrentMaterials.Empty();

		for (INT MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); MaterialIndex++)
		{
			INT CheckIndex = MeshEmitInst->CurrentMaterials.AddZeroed();
			check(CheckIndex == MaterialIndex);
			MeshEmitInst->CurrentMaterials(MaterialIndex) = MeshMaterials(MaterialIndex);
		}
	}
	return 0;
}

/*-----------------------------------------------------------------------------
    UParticleModuleMaterialByParameter
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMaterialByParameter);

void UParticleModuleMaterialByParameter::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	// check to see if we are a MeshEmitter first.  Need to different code for them as the mesh itself could have materials
	FParticleMeshEmitterInstance* MeshEmitInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
	if (MeshEmitInst != NULL)
	{
		// make sure there are enough elements
		if (MeshEmitInst->CurrentMaterials.Num() < MaterialParameters.Num())
		{
			MeshEmitInst->CurrentMaterials.AddZeroed(MaterialParameters.Num() - MeshEmitInst->CurrentMaterials.Num());
		}
		// update the materials
		for (INT MaterialIndex = 0; MaterialIndex < MaterialParameters.Num(); MaterialIndex++)
		{
			UMaterialInterface* Material = NULL;
			if ((MeshEmitInst->Component == NULL || !MeshEmitInst->Component->GetMaterialParameter(MaterialParameters(MaterialIndex), Material)) && DefaultMaterials.IsValidIndex(MaterialIndex))
			if ((
				(MeshEmitInst->Component == NULL) || 
				(MeshEmitInst->Component->GetMaterialParameter(MaterialParameters(MaterialIndex), Material) == FALSE)) && 
				DefaultMaterials.IsValidIndex(MaterialIndex))
			{
				Material = DefaultMaterials(MaterialIndex);
			}

			if (MeshEmitInst->CurrentMaterials(MaterialIndex) != Material)
			{
				Owner->Component->bIsViewRelevanceDirty = TRUE;
			}
			MeshEmitInst->CurrentMaterials(MaterialIndex) = Material;
		}
	}
	// FParticleSpriteEmitterInstance and FParticleSubUVEmitterInstance  both derive from FParticleEmitterInstance
    // so we can check for either as we know that we are not a MeshEmitter from above 
	else if (Owner != NULL)
	{
		// so now we need to look up the material in the parameters array which has been set by SetMaterialParameter
		if( DefaultMaterials.Num() > 0 )
		{
			UMaterialInterface* Material = NULL;

			if ((Owner->Component == NULL || !Owner->Component->GetMaterialParameter(MaterialParameters(0), Material)) && DefaultMaterials.IsValidIndex(0))
			{
				Material = DefaultMaterials(0);
			}

			// so we could have a content error and have a null material.  This will check that and then use the orig material!
			if (Material != NULL)
			{
				if (Owner->CurrentMaterial != Material)
				{
					Owner->Component->bIsViewRelevanceDirty = TRUE;
				}
				Owner->CurrentMaterial = Material;
			}
		}
	}
}

void UParticleModuleMaterialByParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const INT Diff = DefaultMaterials.Num() - MaterialParameters.Num();

	if (Diff > 0)
	{
		DefaultMaterials.Remove(DefaultMaterials.Num() - Diff, Diff);
	}
	else if (Diff < 0)
	{
		DefaultMaterials.AddZeroed(-Diff);
	}
}

/**
 *	Auto-populates the Emitter actors Instance list.
 */
void UParticleModuleMaterialByParameter::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL	bFound	= FALSE;

	for (INT ParamIndex = 0; ParamIndex < MaterialParameters.Num(); ParamIndex++)
	{
		for (INT i = 0; i < PSysComp->InstanceParameters.Num(); i++)
		{
			FParticleSysParam* Param = &(PSysComp->InstanceParameters(i));

				if (Param->Name == MaterialParameters(ParamIndex))
				{
					bFound	=	TRUE;
					break;
				}
			}

		if (!bFound)
		{
			INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
			PSysComp->InstanceParameters(NewParamIndex).Name		= MaterialParameters(ParamIndex);
			PSysComp->InstanceParameters(NewParamIndex).ParamType	= PSPT_Material;
			PSysComp->InstanceParameters(NewParamIndex).Actor		= NULL;
			PSysComp->InstanceParameters(NewParamIndex).Material	= DefaultMaterials.Num() > ParamIndex ? DefaultMaterials(ParamIndex) : NULL;
		}
	}
}

/**
 *	Retrieve the ParticleSysParams associated with this module.
 *
 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
 */
void UParticleModuleMaterialByParameter::GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList)
{
	for (INT ParamIndex = 0; ParamIndex < MaterialParameters.Num(); ParamIndex++)
	{
		ParticleSysParamList.AddItem(FString::Printf(TEXT("MaterialParam: %s, Default = %s\n"), 
			*(MaterialParameters(ParamIndex).ToString()),
			(DefaultMaterials.Num() < ParamIndex) ? *(DefaultMaterials(ParamIndex)->GetPathName()) : TEXT("None")));
	}
}

