/*=============================================================================
	UnParticleModules.cpp: Particle module implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"
#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UParticleModuleAccelerationBase);
IMPLEMENT_CLASS(UParticleModuleAttractorBase);
IMPLEMENT_CLASS(UParticleModuleKillBase);
IMPLEMENT_CLASS(UParticleModuleLifetimeBase);
IMPLEMENT_CLASS(UParticleModuleOrientationBase);
IMPLEMENT_CLASS(UParticleModuleRotationBase);
IMPLEMENT_CLASS(UParticleModuleRotationRateBase);
IMPLEMENT_CLASS(UParticleModuleSubUVBase);
IMPLEMENT_CLASS(UParticleModuleTypeDataBase);
IMPLEMENT_CLASS(UParticleModuleUberBase);

IMPLEMENT_CLASS(UDistributionFloatParticleParameter);
IMPLEMENT_CLASS(UDistributionVectorParticleParameter);

/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	UParticleModule implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModule);

void UParticleModule::PostLoad()
{
	Super::PostLoad();

	if (GIsEditor && ParseParam(appCmdLine(), TEXT("FIXUPTANGENTS")))
	{
		// Fix up any legacy tangents in distributions automatically...
		TArray<FParticleCurvePair> Curves;
		GetCurveObjects(Curves);
		for (INT CurveIndex = 0; CurveIndex < Curves.Num(); CurveIndex++)
		{
			FParticleCurvePair& Curve = Curves(CurveIndex);
			if (Curve.CurveObject)
			{
				UDistributionFloatConstantCurve* FloatConstantCurve = Cast<UDistributionFloatConstantCurve>(Curve.CurveObject);
				UDistributionFloatUniformCurve* FloatUniformCurve = Cast<UDistributionFloatUniformCurve>(Curve.CurveObject);
				UDistributionVectorConstantCurve* VectorConstantCurve = Cast<UDistributionVectorConstantCurve>(Curve.CurveObject);
				UDistributionVectorUniformCurve* VectorUniformCurve = Cast<UDistributionVectorUniformCurve>(Curve.CurveObject);

				if (FloatConstantCurve)
				{
					if (FloatConstantCurve->UsingLegacyInterpMethod())
					{
						FloatConstantCurve->UpgradeInterpMethod();
					}
				}
				if (FloatUniformCurve)
				{
					if (FloatUniformCurve->UsingLegacyInterpMethod())
					{
						FloatUniformCurve->UpgradeInterpMethod();
					}
				}
				if (VectorConstantCurve)
				{
					if (VectorConstantCurve->UsingLegacyInterpMethod())
					{
						VectorConstantCurve->UpgradeInterpMethod();
					}
				}
				if (VectorUniformCurve)
				{
					if (VectorUniformCurve->UsingLegacyInterpMethod())
					{
						VectorUniformCurve->UpgradeInterpMethod();
					}
				}
			}
		}
	}

#if !CONSOLE
	if (GetLinker() && (GetLinker()->Ver() < VER_FIXUP_SEEDED_MODULE_DISTRIBUTIONS))
	{
		if (IsTemplate() == FALSE)
		{
			TArray<FParticleCurvePair> ModuleCurves;
			GetCurveObjects(ModuleCurves);
			for (INT CurveIdx = 0; CurveIdx < ModuleCurves.Num(); CurveIdx++)
			{
				FParticleCurvePair& Pair = ModuleCurves(CurveIdx);
				if (Pair.CurveObject != NULL)
				{
					// Check if distribution archetype is wrong
					UObject* ArchetypeObj = Pair.CurveObject->GetArchetype();
					if (ArchetypeObj != NULL)
					{
						if (ArchetypeObj->GetOutermost()->GetName() != TEXT("Engine"))
						{
							debugf(TEXT("Fixing up bad archetype on %s (%s) in particle module %s"),
								*(Pair.CurveObject->GetFullName()), *(ArchetypeObj->GetFullName()), *GetPathName());
							Pair.CurveObject->SetArchetype(Pair.CurveObject->GetClass()->GetDefaultObject());
						}
					}
					else
					{
						warnf(NAME_Warning, TEXT("Found distribution with no archetype: %s (%s)"),
							*(Pair.CurveObject->GetPathName()), *GetPathName());
					}
				}
			}
		}
	}
#endif
}

void UParticleModule::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
}

void UParticleModule::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
}

/**
 *	Called on an emitter when all other update operations have taken place
 *	INCLUDING bounding box cacluations!
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModule::FinalUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
}

UINT UParticleModule::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return 0;
}

UINT UParticleModule::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return 0;
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModule::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return 0xffffffff;
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModule::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	// The default implementation does nothing...
}

/** Fill an array with each Object property that fulfills the FCurveEdInterface interface. */
void UParticleModule::GetCurveObjects(TArray<FParticleCurvePair>& OutCurves)
{
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		// attempt to get a distribution from a random struct property
		UObject* Distribution = FRawDistribution::TryGetDistributionObjectFromRawDistributionProperty(*It, (BYTE*)this);
		if (Distribution)
		{
			FParticleCurvePair* NewCurve = new(OutCurves)FParticleCurvePair;
			check(NewCurve);
			NewCurve->CurveObject = Distribution;
			NewCurve->CurveName = It->GetName();
		}
	}
}

/** Add all curve-editable Objects within this module to the curve. */
void UParticleModule::AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup)
{
#if WITH_EDITORONLY_DATA
	TArray<FParticleCurvePair> OutCurves;
	GetCurveObjects(OutCurves);
	for (INT CurveIndex = 0; CurveIndex < OutCurves.Num(); CurveIndex++)
	{
		UObject* Distribution = OutCurves(CurveIndex).CurveObject;
		if (Distribution)
		{
			EdSetup->AddCurveToCurrentTab(Distribution, OutCurves(CurveIndex).CurveName, ModuleEditorColor, bCurvesAsColor, bCurvesAsColor);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** Remove all curve-editable Objects within this module from the curve. */
void UParticleModule::RemoveModuleCurvesFromEditor(UInterpCurveEdSetup* EdSetup)
{
	TArray<FParticleCurvePair> OutCurves;
	GetCurveObjects(OutCurves);
	for (INT CurveIndex = 0; CurveIndex < OutCurves.Num(); CurveIndex++)
	{
		UObject* Distribution = OutCurves(CurveIndex).CurveObject;
		if (Distribution)
		{
			EdSetup->RemoveCurve(Distribution);
		}
	}
}

/** Returns true if this Module has any curves that can be pushed into the curve editor. */
UBOOL UParticleModule::ModuleHasCurves()
{
	TArray<FParticleCurvePair> Curves;
	GetCurveObjects(Curves);

	return (Curves.Num() > 0);
}

/** Returns whether any property of this module is displayed in the supplied CurveEd setup. */
UBOOL UParticleModule::IsDisplayedInCurveEd(UInterpCurveEdSetup* EdSetup)
{
	TArray<FParticleCurvePair> Curves;
	GetCurveObjects(Curves);

	for(INT i=0; i<Curves.Num(); i++)
	{
		if(EdSetup->ShowingCurve(Curves(i).CurveObject))
		{
			return true;
		}	
	}

	return false;
}

void UParticleModule::ChangeEditorColor(FColor& Color, UInterpCurveEdSetup* EdSetup)
{
#if WITH_EDITORONLY_DATA
	ModuleEditorColor	= Color;

	TArray<FParticleCurvePair> Curves;
	GetCurveObjects(Curves);

	for (INT TabIndex = 0; TabIndex < EdSetup->Tabs.Num(); TabIndex++)
	{
		FCurveEdTab*	Tab = &(EdSetup->Tabs(TabIndex));
		for (INT CurveIndex = 0; CurveIndex < Tab->Curves.Num(); CurveIndex++)
		{
			FCurveEdEntry* Entry	= &(Tab->Curves(CurveIndex));
			for (INT MyCurveIndex = 0; MyCurveIndex < Curves.Num(); MyCurveIndex++)
			{
				if (Curves(MyCurveIndex).CurveObject == Entry->CurveObject)
				{
					Entry->CurveColor	= Color;
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UParticleModule::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		// attempt to get a distribution from a random struct property
		UObject* Distribution = FRawDistribution::TryGetDistributionObjectFromRawDistributionProperty(*It, (BYTE*)this);
		if (Distribution)
		{
		    BYTE ParamType	= PSPT_None;
			FName ParamName;

			// only handle particle param types
			UDistributionFloatParticleParameter* DistFloatParam = Cast<UDistributionFloatParticleParameter>(Distribution);
			UDistributionVectorParticleParameter* DistVectorParam = Cast<UDistributionVectorParticleParameter>(Distribution);
			if (DistFloatParam != NULL)
			{
			    ParamType = PSPT_Scalar;
				ParamName = DistFloatParam->ParameterName;
			}
			else 
			if (DistVectorParam != NULL)
			{
				ParamType = PSPT_Vector;
				ParamName = DistVectorParam->ParameterName;
			}

			if (ParamType != PSPT_None)
			{
				UBOOL	bFound	= FALSE;
				for (INT i = 0; i < PSysComp->InstanceParameters.Num(); i++)
				{
					FParticleSysParam* Param = &(PSysComp->InstanceParameters(i));
					
					if (Param->Name == ParamName)
					{
						bFound	=	TRUE;
						break;
					}
				}

				if (!bFound)
				{
					INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
					PSysComp->InstanceParameters(NewParamIndex).Name		= ParamName;
					PSysComp->InstanceParameters(NewParamIndex).ParamType	= ParamType;
					PSysComp->InstanceParameters(NewParamIndex).Actor		= NULL;
				}
			}
		}
	}

	FParticleRandomSeedInfo* SeedInfo = GetRandomSeedInfo();
	if (SeedInfo != NULL)
	{
		if (SeedInfo->ParameterName != NAME_None)
		{
			UBOOL	bFound	= FALSE;
			for (INT ParamIdx = 0; ParamIdx < PSysComp->InstanceParameters.Num(); ParamIdx++)
			{
				FParticleSysParam* Param = &(PSysComp->InstanceParameters(ParamIdx));
				if (Param->Name == SeedInfo->ParameterName)
				{
					bFound = TRUE;
					break;
				}
			}

			if (bFound == FALSE)
			{
				INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
				PSysComp->InstanceParameters(NewParamIndex).Name = SeedInfo->ParameterName;
				PSysComp->InstanceParameters(NewParamIndex).ParamType = PSPT_Scalar;
			}
		}
	}
}

/**
 *	GenerateLODModuleValues
 *	Default implementation.
 *	Function is intended to generate the required values by multiplying the source module
 *	values by the given percentage.
 *	
 *	@param	SourceModule		The module to use as the source for values
 *	@param	Percentage			The percentage of the source values to set
 *	@param	LODLevel			The LOD level being generated
 *
 *	@return	TRUE	if successful
 *			FALSE	if failed
 */
UBOOL UParticleModule::GenerateLODModuleValues(UParticleModule* SourceModule, FLOAT Percentage, UParticleLODLevel* LODLevel)
{
	return TRUE;
}

/**
 *	Store the given percentage of the SourceFloat distribution in the FloatDist
 *
 *	@param	FloatDist			The distribution to put the result into.
 *	@param	SourceFloatDist		The distribution of use as the source.
 *	@param	Percentage			The percentage of the source value to use [0..100]
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not.
 */
UBOOL UParticleModule::ConvertFloatDistribution(UDistributionFloat* FloatDist, UDistributionFloat* SourceFloatDist, FLOAT Percentage)
{
	FLOAT	Multiplier	= Percentage / 100.0f;

	UDistributionFloatConstant*				DistConstant		= Cast<UDistributionFloatConstant>(FloatDist);
	UDistributionFloatConstantCurve*		DistConstantCurve	= Cast<UDistributionFloatConstantCurve>(FloatDist);
	UDistributionFloatUniform*				DistUniform			= Cast<UDistributionFloatUniform>(FloatDist);
	UDistributionFloatUniformCurve*			DistUniformCurve	= Cast<UDistributionFloatUniformCurve>(FloatDist);
	UDistributionFloatParticleParameter*	DistParticleParam	= Cast<UDistributionFloatParticleParameter>(FloatDist);

	if (DistParticleParam)
	{
		DistParticleParam->MinOutput	*= Multiplier;
		DistParticleParam->MaxOutput	*= Multiplier;
	}
	else
	if (DistUniformCurve)
	{
		for (INT KeyIndex = 0; KeyIndex < DistUniformCurve->GetNumKeys(); KeyIndex++)
		{
			for (INT SubIndex = 0; SubIndex < DistUniformCurve->GetNumSubCurves(); SubIndex++)
			{
				FLOAT	Value	= DistUniformCurve->GetKeyOut(SubIndex, KeyIndex);
				DistUniformCurve->SetKeyOut(SubIndex, KeyIndex, Value * Multiplier);
			}
		}
	}
	else
	if (DistConstant)
	{
		UDistributionFloatConstant*	SourceConstant	= Cast<UDistributionFloatConstant>(SourceFloatDist);
		check(SourceConstant);
		DistConstant->SetKeyOut(0, 0, SourceConstant->Constant * Multiplier);
	}
	else
	if (DistConstantCurve)
	{
		UDistributionFloatConstantCurve* SourceConstantCurve	= Cast<UDistributionFloatConstantCurve>(SourceFloatDist);
		check(SourceConstantCurve);

		for (INT KeyIndex = 0; KeyIndex < SourceConstantCurve->GetNumKeys(); KeyIndex++)
		{
			DistConstantCurve->CreateNewKey(SourceConstantCurve->GetKeyIn(KeyIndex));
			for (INT SubIndex = 0; SubIndex < SourceConstantCurve->GetNumSubCurves(); SubIndex++)
			{
				FLOAT	Value	= SourceConstantCurve->GetKeyOut(SubIndex, KeyIndex);
				DistConstantCurve->SetKeyOut(SubIndex, KeyIndex, Value * Multiplier);
			}
		}
	}
	else
	if (DistUniform)
	{
		DistUniform->SetKeyOut(0, 0, DistUniform->Min * Multiplier);
		DistUniform->SetKeyOut(1, 0, DistUniform->Max * Multiplier);
	}
	else
	{
		debugf(TEXT("UParticleModule::ConvertFloatDistribution> Invalid distribution?"));
		return FALSE;
	}

	// Safety catch to ensure that the distribution lookup tables get rebuilt...
	FloatDist->bIsDirty = TRUE;
	return TRUE;
}

/**
 *	Store the given percentage of the SourceVector distribution in the VectorDist
 *
 *	@param	VectorDist			The distribution to put the result into.
 *	@param	SourceVectorDist	The distribution of use as the source.
 *	@param	Percentage			The percentage of the source value to use [0..100]
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not.
 */
UBOOL UParticleModule::ConvertVectorDistribution(UDistributionVector* VectorDist, UDistributionVector* SourceVectorDist, FLOAT Percentage)
{
	FLOAT	Multiplier	= Percentage / 100.0f;

	UDistributionVectorConstant*			DistConstant		= Cast<UDistributionVectorConstant>(VectorDist);
	UDistributionVectorConstantCurve*		DistConstantCurve	= Cast<UDistributionVectorConstantCurve>(VectorDist);
	UDistributionVectorUniform*				DistUniform			= Cast<UDistributionVectorUniform>(VectorDist);
	UDistributionVectorUniformCurve*		DistUniformCurve	= Cast<UDistributionVectorUniformCurve>(VectorDist);
	UDistributionVectorParticleParameter*	DistParticleParam	= Cast<UDistributionVectorParticleParameter>(VectorDist);

	if (DistParticleParam)
	{
		DistParticleParam->MinOutput.X	*= Multiplier;
		DistParticleParam->MinOutput.Y	*= Multiplier;
		DistParticleParam->MinOutput.Z	*= Multiplier;
		DistParticleParam->MaxOutput.X	*= Multiplier;
		DistParticleParam->MaxOutput.Y	*= Multiplier;
		DistParticleParam->MaxOutput.Z	*= Multiplier;
	}
	else
	if (DistUniformCurve)
	{
		for (INT KeyIndex = 0; KeyIndex < DistUniformCurve->GetNumKeys(); KeyIndex++)
		{
			for (INT SubIndex = 0; SubIndex < DistUniformCurve->GetNumSubCurves(); SubIndex++)
			{
				FLOAT	Value	= DistUniformCurve->GetKeyOut(SubIndex, KeyIndex);
				DistUniformCurve->SetKeyOut(SubIndex, KeyIndex, Value * Multiplier);
			}
		}
	}
	else
	if (DistConstant)
	{
		DistConstant->Constant.X *= Multiplier;
		DistConstant->Constant.Y *= Multiplier;
		DistConstant->Constant.Z *= Multiplier;
	}
	else
	if (DistConstantCurve)
	{
		for (INT KeyIndex = 0; KeyIndex < DistConstantCurve->GetNumKeys(); KeyIndex++)
		{
			for (INT SubIndex = 0; SubIndex < DistConstantCurve->GetNumSubCurves(); SubIndex++)
			{
				FLOAT	Value	= DistConstantCurve->GetKeyOut(SubIndex, KeyIndex);
				DistConstantCurve->SetKeyOut(SubIndex, KeyIndex, Value * Multiplier);
			}
		}
	}
	else
	if (DistUniform)
	{
		DistUniform->Min.X	*= Multiplier;
		DistUniform->Min.Y	*= Multiplier;
		DistUniform->Min.Z	*= Multiplier;
		DistUniform->Max.X	*= Multiplier;
		DistUniform->Max.Y	*= Multiplier;
		DistUniform->Max.Z	*= Multiplier;
	}
	else
	{
		debugf(TEXT("UParticleModule::ConvertVectorDistribution> Invalid distribution?"));
		return FALSE;
	}

	// Safety catch to ensure that the distribution lookup tables get rebuilt...
	VectorDist->bIsDirty = TRUE;
	return TRUE;
}

/**
 *	Comparison routine...
 *	Intended for editor-use only, this function will return TRUE if the given
 *	particle module settings are identical to the one the function is called on.
 *
 *	@param	InModule	The module to compare against.
 *
 *	@return	TRUE		If the modules have all the relevant settings the same.
 *			FALSE		If they don't.
 */
UBOOL UParticleModule::IsIdentical_Deprecated(const UParticleModule* InModule) const
{
	// Valid module?
	if (InModule == NULL)
	{
		return FALSE;
	}

	// Same class?
	if (InModule->GetClass() != GetClass())
	{
		return FALSE;
	}

	for (UProperty* Prop = GetClass()->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
	{
		// only the properties that could have been modified in the editor should be compared
		// (skipping the name and archetype properties, since name will almost always be different)
		UBOOL bConsiderProperty = Prop->ShouldDuplicateValue();
		if (PropertyIsRelevantForIsIdentical_Deprecated(Prop->GetPureName()) == FALSE)
		{
			bConsiderProperty = FALSE;
		}

		if (bConsiderProperty)
		{
			for (INT i = 0; i < Prop->ArrayDim; i++)
			{
				if (!Prop->Matches(this, InModule, i, FALSE, PPF_DeepComparison))
				{
					return FALSE;
				}
			}
		}
	}

	return TRUE;
}

/**
 *	Used by the comparison routine to check for properties that are irrelevant.
 *
 *	@param	InPropName	The name of the property being checked.
 *
 *	@return	TRUE		If the property is relevant.
 *			FALSE		If it isn't.
 */
UBOOL UParticleModule::PropertyIsRelevantForIsIdentical_Deprecated(const FName& InPropName) const
{
	static TArray<FName> IdenticalIgnoreProperties_ParticleModule;
	static TArray<FName> IdenticalIgnoreProperties_ParticleModuleRequired;
	if( IdenticalIgnoreProperties_ParticleModule.Num() == 0 )
	{
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("bSpawnModule")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("bUpdateModule")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("bFinalUpdateModule")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("bCurvesAsColor")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("b3DDrawMode")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("bSupported3DDrawMode")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("bEditable")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("ModuleEditorColor")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("IdenticalIgnoreProperties")));
		IdenticalIgnoreProperties_ParticleModule.AddItem(FName(TEXT("LODValidity")));
		IdenticalIgnoreProperties_ParticleModuleRequired.AddItem(FName(TEXT("SpawnRate")));
		IdenticalIgnoreProperties_ParticleModuleRequired.AddItem(FName(TEXT("ParticleBurstMethod")));
		IdenticalIgnoreProperties_ParticleModuleRequired.AddItem(FName(TEXT("BurstList")));
	}
	for (INT IgnoreIndex = 0; IgnoreIndex < IdenticalIgnoreProperties_ParticleModule.Num(); IgnoreIndex++)
	{
		if (IdenticalIgnoreProperties_ParticleModule(IgnoreIndex) == InPropName)
		{
			return FALSE;
		}
	}
	if( IsA( UParticleModuleRequired::StaticClass() ) )
	{
		for (INT IgnoreIndex = 0; IgnoreIndex < IdenticalIgnoreProperties_ParticleModuleRequired.Num(); IgnoreIndex++)
		{
			if (IdenticalIgnoreProperties_ParticleModuleRequired(IgnoreIndex) == InPropName)
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}

/**
 *	Generates a new module for LOD levels, setting the values appropriately.
 *	Note that the module returned could simply be the module it was called on.
 *
 *	@param	SourceLODLevel		The source LODLevel
 *	@param	DestLODLevel		The destination LODLevel
 *	@param	Percentage			The percentage value that should be used when setting values
 *
 *	@return	UParticleModule*	The generated module, or this if percentage == 100.
 */
UParticleModule* UParticleModule::GenerateLODModule(UParticleLODLevel* SourceLODLevel, UParticleLODLevel* DestLODLevel, FLOAT Percentage, 
	UBOOL bGenerateModuleData, UBOOL bForceModuleConstruction)
{
	if (WillGeneratedModuleBeIdentical(SourceLODLevel, DestLODLevel, Percentage) && !bForceModuleConstruction)
	{
		LODValidity |= (1 << DestLODLevel->Level);
		return this;
	}

	// Otherwise, construct a new object and set the values appropriately... if required.
	UParticleModule* NewModule = NULL;

	UObject* DupObject = UObject::StaticDuplicateObject(this, this, GetOuter(), TEXT("None"));
	if (DupObject)
	{
		NewModule = CastChecked<UParticleModule>(DupObject);
		NewModule->LODValidity = (1 << DestLODLevel->Level);
		if (bGenerateModuleData)
		{
			if (NewModule->GenerateLODModuleValues(this, Percentage, DestLODLevel) == FALSE)
			{
				FString NameDump;
				GetName(NameDump);
				debugf(TEXT("ERROR - GenerateFromLODLevel - Failed to generate LOD module values for %s!"), *NameDump);
				NewModule = NULL;
			}
		}
	}
	
	return NewModule;
}

/**
 *	Returns TRUE if the module validiy flags indicate this module is used in the given LOD level.
 *
 *	@param	SourceLODIndex		The index of the source LODLevel
 *
 *	@return	UBOOL				TRUE if the generated module is used, FALSE if not.
 */
UBOOL UParticleModule::IsUsedInLODLevel(INT SourceLODIndex) const
{
	if ((SourceLODIndex >= 0) && (SourceLODIndex <= 7))
	{
		return ((LODValidity & (1 << SourceLODIndex)) != 0);
	}
	return FALSE;
}

/**
 *	Retrieve the ParticleSysParams associated with this module.
 *
 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
 */
void UParticleModule::GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList)
{
}
						   
/**
 *	Retrieve the distributions that use ParticleParameters in this module.
 *
 *	@param	ParticleParameterList	The list of ParticleParameter distributions to add to
 */
void UParticleModule::GetParticleParametersUtilized(TArray<FString>& ParticleParameterList)
{
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		// attempt to get a distribution from a random struct property
		UObject* Distribution = FRawDistribution::TryGetDistributionObjectFromRawDistributionProperty(*It, (BYTE*)this);
		if (Distribution)
		{
			UDistributionFloatParticleParameter* FloatPP = Cast<UDistributionFloatParticleParameter>(Distribution);
			UDistributionVectorParticleParameter* VectorPP = Cast<UDistributionVectorParticleParameter>(Distribution);

			// only handle particle param types
			if (FloatPP)
			{
				ParticleParameterList.AddItem(
					FString::Printf(
						TEXT("FLOAT : %32s - MinIn %10.5f, MaxIn %10.5f, MinOut %10.5f, MaxOut %10.5f, Mode %10s, Constant %10.5f\n"),
						*(FloatPP->ParameterName.ToString()),
						FloatPP->MinInput,
						FloatPP->MaxInput,
						FloatPP->MinOutput,
						FloatPP->MaxOutput,
						(FloatPP->ParamMode == DPM_Normal) ? TEXT("Normal") :
							(FloatPP->ParamMode == DPM_Abs) ? TEXT("Absolute") :
								(FloatPP->ParamMode == DPM_Direct) ? TEXT("Direct") :
									TEXT("????"),
						FloatPP->Constant)
						);
			}
			else 
			if (VectorPP)
			{
				FString ParamString;

				ParamString = FString::Printf(TEXT("VECTOR: %32s - "), *(VectorPP->ParameterName.ToString()));
				ParamString += FString::Printf(TEXT("MinIn %10.5f,%10.5f,%10.5f, "), 
					VectorPP->MinInput.X, VectorPP->MinInput.Y, VectorPP->MinInput.Z);
				ParamString += FString::Printf(TEXT("MaxIn %10.5f,%10.5f,%10.5f, "),
					VectorPP->MaxInput.X, VectorPP->MaxInput.Y, VectorPP->MaxInput.Z);
				ParamString += FString::Printf(TEXT("MinOut %10.5f,%10.5f,%10.5f, "),
						VectorPP->MinOutput.X, VectorPP->MinOutput.Y, VectorPP->MinOutput.Z);
				ParamString += FString::Printf(TEXT("MaxOut %10.5f,%10.5f,%10.5f, "),
						VectorPP->MaxOutput.X, VectorPP->MaxOutput.Y, VectorPP->MaxOutput.Z);
				ParamString += FString::Printf(TEXT("Mode %10s,%10s,%10s, "),
						(VectorPP->ParamModes[0] == DPM_Normal) ? TEXT("Normal") :
							(VectorPP->ParamModes[0] == DPM_Abs) ? TEXT("Absolute") :
								(VectorPP->ParamModes[0] == DPM_Direct) ? TEXT("Direct") :
									TEXT("????"),
						(VectorPP->ParamModes[1] == DPM_Normal) ? TEXT("Normal") :
							(VectorPP->ParamModes[1] == DPM_Abs) ? TEXT("Absolute") :
								(VectorPP->ParamModes[1] == DPM_Direct) ? TEXT("Direct") :
									TEXT("????"),
						(VectorPP->ParamModes[2] == DPM_Normal) ? TEXT("Normal") :
							(VectorPP->ParamModes[2] == DPM_Abs) ? TEXT("Absolute") :
								(VectorPP->ParamModes[2] == DPM_Direct) ? TEXT("Direct") :
									TEXT("????"));
				ParamString += FString::Printf(TEXT("Constant %10.5f,%10.5f,%10.5f\n"),
						VectorPP->Constant.X, VectorPP->Constant.Y, VectorPP->Constant.Z);
				ParticleParameterList.AddItem(ParamString);
			}
		}
	}
}

/** 
 *	Prepare a random seed instance payload...
 *
 *	@param	Owner					The emitter instance that owns this module
 *	@param	InRandSeedPayload		The random seed instance payload to initialize
 *	@param	InRandSeedInfo			The random seed info of the module
 *
 *	@return	UINT					0xffffffff is failed
 */
UINT UParticleModule::PrepRandomSeedInstancePayload(FParticleEmitterInstance* Owner, FParticleRandomSeedInstancePayload* InRandSeedPayload, const FParticleRandomSeedInfo& InRandSeedInfo)
{
	if (InRandSeedPayload != NULL)
	{
		appMemzero(InRandSeedPayload, sizeof(FParticleRandomSeedInstancePayload));

		// See if the parameter is set on the instance...
		if ((Owner != NULL) && (Owner->Component != NULL) && (InRandSeedInfo.bGetSeedFromInstance == TRUE))
		{
			FLOAT SeedValue;
			if (Owner->Component->GetFloatParameter(InRandSeedInfo.ParameterName, SeedValue) == TRUE)
			{
				if (InRandSeedInfo.bInstanceSeedIsIndex == FALSE)
				{
					InRandSeedPayload->RandomStream.Initialize(appRound(SeedValue));
				}
				else
				{
					if (InRandSeedInfo.RandomSeeds.Num() > 0)
					{
						INT Index = Min<INT>((InRandSeedInfo.RandomSeeds.Num() - 1), appTrunc(SeedValue));
						InRandSeedPayload->RandomStream.Initialize(InRandSeedInfo.RandomSeeds(Index));
						return 0;
					}
					else
					{
						return 0xffffffff;
					}
				}
				return 0;
			}
		}

		// Pick a seed to use and initialize it!!!!
		if (InRandSeedInfo.RandomSeeds.Num() > 0)
		{
			InRandSeedPayload->RandomStream.Initialize(InRandSeedInfo.RandomSeeds(0));
			return 0;
		}
	}
	return 0xffffffff;
}

/**
 *	Set the random seed info entry at the given index to the given seed
 *
 *	@param	InIndex			The index of the entry to set
 *	@param	InRandomSeed	The seed to set the entry to
 *
 *	@return	UBOOL			TRUE if successful; FALSE if not (not found, etc.)
 */
UBOOL UParticleModule::SetRandomSeedEntry(INT InIndex, INT InRandomSeed)
{
	FParticleRandomSeedInfo* SeedInfo = GetRandomSeedInfo();
	if (SeedInfo != NULL)
	{
		if (SeedInfo->RandomSeeds.Num() <= InIndex)
		{
			SeedInfo->RandomSeeds.AddZeroed(InIndex - SeedInfo->RandomSeeds.Num() + 1);
		}

		SeedInfo->RandomSeeds(InIndex) = InRandomSeed;
		return TRUE;
	}
	return FALSE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleSourceMovement implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSourceMovement);

/**
 *	Called on an emitter when all other update operations have taken place
 *	INCLUDING bounding box calculations!
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	DeltaTime	The time since the last update.
 */
void UParticleModuleSourceMovement::FinalUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	Super::FinalUpdate(Owner, Offset, DeltaTime);
	// If enabled, update the source offset translation for all particles...
	if (Owner && Owner->Component)
	{
		FVector FrameDiff = Owner->Component->LocalToWorld.GetOrigin() - Owner->Component->OldPosition;
		BEGIN_UPDATE_LOOP;
		{
			// Rough estimation of the particle being alive for more than a frame
			if (Particle.RelativeTime > (2.0f * DeltaTime * Particle.OneOverMaxLifetime))
			{
				FVector	Scale = SourceMovementScale.GetValue(Particle.RelativeTime, Owner->Component);
				Particle.Location += (Scale * FrameDiff);
			}
		}
		END_UPDATE_LOOP;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleOrientationAxisLock implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleOrientationAxisLock);

//    BYTE LockAxisFlags;
//    FVector LockAxis;
void UParticleModuleOrientationAxisLock::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
}

void UParticleModuleOrientationAxisLock::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
}

void UParticleModuleOrientationAxisLock::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject* OuterObj = GetOuter();
	check(OuterObj);
	UParticleLODLevel* LODLevel = Cast<UParticleLODLevel>(OuterObj);
	if (LODLevel)
	{
		// The outer is incorrect - warn the user and handle it
		warnf(NAME_Warning, TEXT("UParticleModuleOrientationAxisLock has an incorrect outer... run FixupEmitters on package %s"),
			*(OuterObj->GetOutermost()->GetPathName()));
		OuterObj = LODLevel->GetOuter();
		UParticleEmitter* Emitter = Cast<UParticleEmitter>(OuterObj);
		check(Emitter);
		OuterObj = Emitter->GetOuter();
	}
	UParticleSystem* PartSys = PartSys = CastChecked<UParticleSystem>(OuterObj);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetFName() == FName(TEXT("LockAxisFlags")))
		{
			PartSys->PostEditChangeProperty(PropertyChangedEvent);
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleOrientationAxisLock::SetLockAxis(EParticleAxisLock eLockFlags)
{
	LockAxisFlags = eLockFlags;
}

/*-----------------------------------------------------------------------------
	UParticleModuleRequired implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleRequired);

void UParticleModuleRequired::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (SubImages_Horizontal < 1)
	{
		SubImages_Horizontal = 1;
	}
	if (SubImages_Vertical < 1)
	{
		SubImages_Vertical = 1;
	}

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetFName() == FName(TEXT("MaxDrawCount")))
		{
			if (MaxDrawCount >= 0)
			{
				bUseMaxDrawCount = TRUE;
			}
			else
			{
				bUseMaxDrawCount = FALSE;
			}
		}
	}

	NearCullDistance = Max( GNearClippingPlane, NearCullDistance );
	NearFadeDistance = Max( NearCullDistance+1.0f, NearFadeDistance );

	FarFadeDistance = Max( NearFadeDistance+1.0f, FarFadeDistance );
	FarCullDistance = Max( FarFadeDistance+1.0f, FarCullDistance );

	// Refresh UI.
	if( PropertyChangedEvent.Property != NULL )
	{
		FString PropertyName = PropertyChangedEvent.Property->GetName();
		if
			( 
			PropertyName == TEXT("NearCullDistance") ||
			PropertyName == TEXT("NearFadeDistance") ||
			PropertyName == TEXT("FarFadeDistance") ||
			PropertyName == TEXT("FarCullDistance")
			)
		{
			PostEditChange();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleRequired::PostLoad()
{
	Super::PostLoad();

	if (SubImages_Horizontal < 1)
	{
		SubImages_Horizontal = 1;
	}
	if (SubImages_Vertical < 1)
	{
		SubImages_Vertical = 1;
	}

	if (GetLinker() && (GetLinker()->Ver() < VER_NEW_PARTICLE_SORT_MODES))
	{
		if (bRequiresSorting_DEPRECATED == TRUE)
		{
			SortMode = PSORTMODE_ViewProjDepth;
		}
	}	
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleRequired::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	Super::SetToSensibleDefaults(Owner);
	bUseLegacyEmitterTime = FALSE;
	bAllowImageFlipping = TRUE;
}

UBOOL UParticleModuleRequired::GenerateLODModuleValues(UParticleModule* SourceModule, FLOAT Percentage, UParticleLODLevel* LODLevel)
{
	// Convert the module values
	UParticleModuleRequired*	RequiredSource	= Cast<UParticleModuleRequired>(SourceModule);
	if (!RequiredSource)
	{
		return FALSE;
	}

	UBOOL bResult	= TRUE;

	Material = RequiredSource->Material;
	ScreenAlignment = RequiredSource->ScreenAlignment;

	//bUseLocalSpace
	//bKillOnDeactivate
	//bKillOnCompleted
	//EmitterDuration
	//EmitterLoops
	//SpawnRate
	//InterpolationMethod
	//SubImages_Horizontal
	//SubImages_Vertical
	//bScaleUV
	//RandomImageTime
	//RandomImageChanges
	//bDirectUV
	//SubUVDataOffset
	//EmitterRenderMode
	//EmitterEditorColor

	return bResult;
}

/*-----------------------------------------------------------------------------
	UParticleModuleMeshRotation implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMeshRotation);

void UParticleModuleMeshRotation::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleMeshRotation::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	{
		FParticleMeshEmitterInstance* MeshInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
		if (MeshInst)
		{
			FVector Rotation = StartRotation.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
			if (bInheritParent)
			{
				FRotator	Rotator	= Owner->Component->LocalToWorld.Rotator();
				FVector		ParentAffectedRotation	= Rotator.Euler();
				Rotation.X	+= ParentAffectedRotation.X / 360.0f;
				Rotation.Y	+= ParentAffectedRotation.Y / 360.0f;
				Rotation.Z	+= ParentAffectedRotation.Z / 360.0f;
			}
			FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
			PayloadData->Rotation.X	+= Rotation.X * 360.0f;
			PayloadData->Rotation.Y	+= Rotation.Y * 360.0f;
			PayloadData->Rotation.Z	+= Rotation.Z * 360.0f;
		}
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleMeshRotation_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMeshRotation_Seeded);
/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleMeshRotation_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
UINT UParticleModuleMeshRotation_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleMeshRotation_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleMeshRotation_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleMeshRotationRate implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMeshRotationRate);

void UParticleModuleMeshRotationRate::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleMeshRotationRate::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	{
		FParticleMeshEmitterInstance* MeshInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
		if (MeshInst)
		{
			FVector StartRate = StartRotationRate.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);// * ((FLOAT)PI/180.f);
			FVector StartValue;
			StartValue.X = StartRate.X * 360.0f;
			StartValue.Y = StartRate.Y * 360.0f;
			StartValue.Z = StartRate.Z * 360.0f;

			FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
			PayloadData->RotationRateBase	+= StartValue;
			PayloadData->RotationRate		+= StartValue;
		}
	}
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleMeshRotationRate::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionVectorUniform* StartRotationRateDist = Cast<UDistributionVectorUniform>(StartRotationRate.Distribution);
	if (StartRotationRateDist)
	{
		StartRotationRateDist->Min = FVector(0.0f,0.0f,0.0f);
		StartRotationRateDist->Max = FVector(1.0f,1.0f,1.0f);
		StartRotationRateDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleMeshRotationRate_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMeshRotationRate_Seeded);
/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleMeshRotationRate_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
UINT UParticleModuleMeshRotationRate_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleMeshRotationRate_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleMeshRotationRate_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleMeshRotationRateMultiplyLife implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMeshRotationRateMultiplyLife);

void UParticleModuleMeshRotationRateMultiplyLife::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleMeshEmitterInstance* MeshInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
	if ((MeshInst == NULL) || (MeshInst->MeshRotationOffset == 0))
	{
		return;
	}

	SPAWN_INIT;
	{
		FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
		FVector RateScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
		PayloadData->RotationRate *= RateScale;
	}
}

void UParticleModuleMeshRotationRateMultiplyLife::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	FParticleMeshEmitterInstance* MeshInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
	if ((MeshInst == NULL) || (MeshInst->MeshRotationOffset == 0))
	{
		return;
	}

	BEGIN_UPDATE_LOOP;
	{
		FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
		FVector RateScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
		PayloadData->RotationRate *= RateScale;
	}
	END_UPDATE_LOOP;
}

void UParticleModuleMeshRotationRateMultiplyLife::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionVectorUniform* LifeMultiplierDist = Cast<UDistributionVectorUniform>(LifeMultiplier.Distribution);
	if (LifeMultiplierDist)
	{
		LifeMultiplierDist->Min = FVector(0.0f,0.0f,0.0f);
		LifeMultiplierDist->Max = FVector(1.0f,1.0f,1.0f);
		LifeMultiplierDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleMeshRotationRateOverLife implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleMeshRotationRateOverLife);

/**
 *	Called during the spawning of a particle.
 *	
 *	@param	Owner		The emitter instance that owns the particle.
 *	@param	Offset		The offset into the particle payload for this module.
 *	@param	SpawnTime	The spawn time of the particle.
 */
void UParticleModuleMeshRotationRateOverLife::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleMeshEmitterInstance* MeshInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
	if ((MeshInst == NULL) || (MeshInst->MeshRotationOffset == 0))
	{
		return;
	}

	SPAWN_INIT;
	{
		FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
		FVector RateValue = RotRate.GetValue(Particle.RelativeTime, Owner->Component);// * ((FLOAT)PI/180.f);
		RateValue.X = RateValue.X * 360.0f;
		RateValue.Y = RateValue.Y * 360.0f;
		RateValue.Z = RateValue.Z * 360.0f;

		if (bScaleRotRate == FALSE)
		{
			PayloadData->RotationRate += RateValue;
		}
		else
		{
			PayloadData->RotationRate *= RateValue;
		}
	}
}

/**
 *	Called during the spawning of particles in the emitter instance.
 *	
 *	@param	Owner		The emitter instance that owns the particle.
 *	@param	Offset		The offset into the particle payload for this module.
 *	@param	DeltaTime	The time slice for this update.
 */
void UParticleModuleMeshRotationRateOverLife::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	FParticleMeshEmitterInstance* MeshInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
	if ((MeshInst == NULL) || (MeshInst->MeshRotationOffset == 0))
	{
		return;
	}

	FMeshRotationPayloadData* PayloadData;
	FVector RateValue;
	if (bScaleRotRate == FALSE)
	{
		BEGIN_UPDATE_LOOP;
		{
			PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
			RateValue = RotRate.GetValue(Particle.RelativeTime, Owner->Component);// * ((FLOAT)PI/180.f);
			RateValue.X = RateValue.X * 360.0f;
			RateValue.Y = RateValue.Y * 360.0f;
			RateValue.Z = RateValue.Z * 360.0f;
			PayloadData->RotationRate += RateValue;
		}
		END_UPDATE_LOOP;
	}
	else
	{
		BEGIN_UPDATE_LOOP;
		{
			PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
			RateValue = RotRate.GetValue(Particle.RelativeTime, Owner->Component);// * ((FLOAT)PI/180.f);
			RateValue.X = RateValue.X * 360.0f;
			RateValue.Y = RateValue.Y * 360.0f;
			RateValue.Z = RateValue.Z * 360.0f;
			PayloadData->RotationRate *= RateValue;
		}
		END_UPDATE_LOOP;
	}
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleMeshRotationRateOverLife::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionVectorConstantCurve* RotRateDist = Cast<UDistributionVectorConstantCurve>(RotRate.Distribution);
	if (RotRateDist)
	{
		RotRateDist->ConstantCurve.AddPoint(0.0f, FVector(0.0f, 0.0f, 0.0f));
		RotRateDist->ConstantCurve.AddPoint(1.0f, FVector(1.0f, 1.0f, 1.0f));
		RotRateDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleRotation implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleRotation);

void UParticleModuleRotation::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleRotation::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	{
		Particle.Rotation += (PI/180.f) * 360.0f * StartRotation.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleRotation_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleRotation_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleRotation_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
UINT UParticleModuleRotation_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleRotation_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleRotation_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleRotationRate implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleRotationRate);

void UParticleModuleRotationRate::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleRotationRate::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	{
		FLOAT StartRotRate = (PI/180.f) * 360.0f * StartRotationRate.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
		Particle.RotationRate += StartRotRate;
		Particle.BaseRotationRate += StartRotRate;
	}
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleRotationRate::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	StartRotationRate.Distribution = Cast<UDistributionFloatUniform>(StaticConstructObject(UDistributionFloatUniform::StaticClass(), this));
	UDistributionFloatUniform* StartRotationRateDist = Cast<UDistributionFloatUniform>(StartRotationRate.Distribution);
	if (StartRotationRateDist)
	{
		StartRotationRateDist->Min = 0.0f;
		StartRotationRateDist->Max = 1.0f;
		StartRotationRateDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleRotationRate_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleRotationRate_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleRotationRate_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
UINT UParticleModuleRotationRate_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleRotationRate_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleRotationRate_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleRotationOverLifetime implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleRotationOverLifetime);

void UParticleModuleRotationOverLifetime::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if (Scale)
	{
		BEGIN_UPDATE_LOOP;
		{
			FLOAT Rotation = RotationOverLife.GetValue(Particle.RelativeTime, Owner->Component);
			// For now, we are just using the X-value
			Particle.Rotation = (Particle.Rotation * (Rotation * (PI/180.f) * 360.0f));
		}
		END_UPDATE_LOOP;
	}
	else
	{
		BEGIN_UPDATE_LOOP;
		{
			FLOAT Rotation = RotationOverLife.GetValue(Particle.RelativeTime, Owner->Component);
			// For now, we are just using the X-value
			Particle.Rotation = (Particle.Rotation + (Rotation * (PI/180.f) * 360.0f));
		}
		END_UPDATE_LOOP;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleSubUV implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSubUV);

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	check(Owner->SpriteTemplate);

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	// Grab the interpolation method...
	EParticleSubUVInterpMethod eMethod = (EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
	if ((eMethod == PSUVIM_None) || (Owner->SubUVDataOffset == 0))
	{
		return;
	}

	UParticleModuleTypeDataBase* TypeDataBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
	UBOOL bSpawn = (TypeDataBase == NULL) ? TRUE : TypeDataBase->SupportsSubUV();
	UBOOL bMeshEmitter = (TypeDataBase == NULL) ? FALSE : TypeDataBase->IsAMeshEmitter();
	if (bSpawn == TRUE)
	{
		UParticleLODLevel* HighestLODLevel = Owner->SpriteTemplate->GetLODLevel(0);
		check(HighestLODLevel);

		// This is NOT directUV!
		LODLevel->RequiredModule->bDirectUV = false;

		INT	PayloadOffset	= Owner->SubUVDataOffset;
		INT iTotalSubImages = LODLevel->RequiredModule->SubImages_Horizontal * LODLevel->RequiredModule->SubImages_Vertical;
		EParticleSubUVInterpMethod eMethod = (EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
		if ((eMethod == PSUVIM_None) || (PayloadOffset == 0))
		{
			return;
		}

		FLOAT baseU = 1.0f;
		FLOAT baseV = 1.0f;
		if (bMeshEmitter == TRUE)
		{
			baseU /= LODLevel->RequiredModule->SubImages_Horizontal;
			baseV /= LODLevel->RequiredModule->SubImages_Vertical;
		}

		FLOAT fInterp;
		INT iImageIndex;
		INT iImageH;
		INT iImageV;

		SPAWN_INIT;
		{
			INT	TempOffset	= CurrentOffset;
			CurrentOffset	= PayloadOffset;
			PARTICLE_ELEMENT(FFullSubUVPayload, SubUVPayload);
			CurrentOffset	= TempOffset;

			UBOOL bUpdateImage = DetermineImageIndex(Owner, CurrentOffset, &Particle, eMethod, SubUVPayload, iImageIndex, fInterp, SpawnTime);
			if (bUpdateImage)
			{
				iImageH = iImageIndex % LODLevel->RequiredModule->SubImages_Horizontal;
				iImageV = iImageIndex / LODLevel->RequiredModule->SubImages_Horizontal;

				// Update the payload
				SubUVPayload.ImageHVInterp_UVOffset.X	= baseU * iImageH;
				SubUVPayload.ImageHVInterp_UVOffset.Y	= baseV * iImageV;
			}
		}
	}
}

void UParticleModuleSubUV::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	check(Owner->SpriteTemplate);

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	// Grab the interpolation method...
	EParticleSubUVInterpMethod eMethod = 
		(EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
	if ((eMethod == PSUVIM_None) || (Owner->SubUVDataOffset == 0))
	{
		return;
	}

	// Quick-out in case of Random that only uses a single image for the whole lifetime...
	if ((eMethod == PSUVIM_Random) || (eMethod == PSUVIM_Random_Blend))
	{
		if (LODLevel->RequiredModule->RandomImageChanges == 0)
		{
			// Never change the random image...
			return;
		}
	}

	UParticleModuleTypeDataBase* TypeDataBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
	UBOOL bUpdate = (TypeDataBase == NULL) ? TRUE : TypeDataBase->SupportsSubUV();
	UBOOL bMeshEmitter = (TypeDataBase == NULL) ? FALSE : TypeDataBase->IsAMeshEmitter();
	if (bUpdate == TRUE)
	{
		UParticleLODLevel* HighestLODLevel = Owner->SpriteTemplate->GetLODLevel(0);
		check(HighestLODLevel);
		// This is NOT directUV!
		LODLevel->RequiredModule->bDirectUV = false;

		INT	PayloadOffset	= Owner->SubUVDataOffset;
		INT iTotalSubImages = LODLevel->RequiredModule->SubImages_Horizontal * LODLevel->RequiredModule->SubImages_Vertical;
		EParticleSubUVInterpMethod eMethod = (EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
		if ((eMethod == PSUVIM_None) || (PayloadOffset == 0))
		{
			return;
		}

		FLOAT baseU = 1.0f;
		FLOAT baseV = 1.0f;
		if (bMeshEmitter == TRUE)
		{
			baseU /= LODLevel->RequiredModule->SubImages_Horizontal;
			baseV /= LODLevel->RequiredModule->SubImages_Vertical;
		}

		FLOAT fInterp;
		INT iImageIndex;
		INT iImageH;
		INT iImageV;
		INT iImage2H;
		INT iImage2V	= 0;

		BEGIN_UPDATE_LOOP;
		if (Particle.RelativeTime > 1.0f)
		{
			CONTINUE_UPDATE_LOOP;
		}

		INT	TempOffset	= CurrentOffset;
		CurrentOffset	= PayloadOffset;
		PARTICLE_ELEMENT(FFullSubUVPayload, SubUVPayload);
		CurrentOffset	= TempOffset;

		UBOOL bUpdateImage = DetermineImageIndex(Owner, CurrentOffset, &Particle, eMethod, SubUVPayload, iImageIndex, fInterp, DeltaTime);
		iImageH = iImageIndex % LODLevel->RequiredModule->SubImages_Horizontal;
		iImageV = iImageIndex / LODLevel->RequiredModule->SubImages_Horizontal;
		if (bUpdateImage)
		{
			if (iImageH == (LODLevel->RequiredModule->SubImages_Horizontal - 1))
			{
				iImage2H = 0;
				if (iImageV == (LODLevel->RequiredModule->SubImages_Vertical - 1))
				{
					iImage2V = 0;
				}
				else
				{
					iImage2V = iImageV + 1;
				}
			}
			else
			{
				iImage2H = iImageH + 1;
				iImage2V = iImageV;
			}
		}
		else
		{
			iImage2H = iImageH;
			iImage2V = iImageV;
		}

		// Update the payload
		SubUVPayload.ImageHVInterp_UVOffset.Z = fInterp;
		SubUVPayload.ImageHVInterp_UVOffset.X = baseU * (FLOAT)iImageH;
		SubUVPayload.ImageHVInterp_UVOffset.Y = baseV * (FLOAT)iImageV;
		SubUVPayload.Image2HV_UV2Offset.X = baseU * (FLOAT)iImage2H;
		SubUVPayload.Image2HV_UV2Offset.Y = baseV * (FLOAT)iImage2V;

		END_UPDATE_LOOP;
	}
}

/**
 *	Determine the current image index to use...
 *
 *	@param	Owner					The emitter instance being updated.
 *	@param	Offset					The offset to the particle payload for this module.
 *	@param	Particle				The particle that the image index is being determined for.
 *	@param	eMethod					The EParticleSubUVInterpMethod method used to update the subUV.
 *	@param	SubUVPayload			The FFullSubUVPayload for this particle.
 *	@param	ImageIndex		[out]	The image index to use for the particle.
 *	@param	Interp			[out]	The current interpolation value (for blending 2 sub-images).
 *	@param	DeltaTime				The time slice for this update.
 *
 *	@return	UBOOL					TRUE if the image should be updated (index changed), FALSE if not.
 */
UBOOL UParticleModuleSubUV::DetermineImageIndex(FParticleEmitterInstance* Owner, INT Offset, FBaseParticle* Particle, 
	EParticleSubUVInterpMethod eMethod, FFullSubUVPayload& SubUVPayload, 
	INT& ImageIndex, FLOAT& Interp, FLOAT DeltaTime)
{
	UBOOL	bUpdateImage	= TRUE;

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	
	INT TotalSubImages	= LODLevel->RequiredModule->SubImages_Horizontal * LODLevel->RequiredModule->SubImages_Vertical;

	ImageIndex	= appTrunc(SubUVPayload.ImageHVInterp_UVOffset.X + SubUVPayload.ImageHVInterp_UVOffset.Y * LODLevel->RequiredModule->SubImages_Horizontal);

	if ((eMethod == PSUVIM_Linear) || (eMethod == PSUVIM_Linear_Blend))
	{
		if (bUseRealTime == FALSE)
		{
			Interp = SubImageIndex.GetValue(Particle->RelativeTime, Owner->Component);
		}
		else
		{
			if ((GWorld != NULL) && (GWorld->GetWorldInfo() != NULL))
			{
				Interp = SubImageIndex.GetValue(Particle->RelativeTime / GWorld->GetWorldInfo()->TimeDilation, Owner->Component);
			}
			else
			{
				Interp = SubImageIndex.GetValue(Particle->RelativeTime, Owner->Component);
			}
		}
		// Assuming a 0..<# sub images> range here...
		ImageIndex = appTrunc(Interp);
		ImageIndex = Clamp(ImageIndex, 0, TotalSubImages - 1);

		if (Interp > (FLOAT)ImageIndex)
		{
			Interp = Interp - (FLOAT)ImageIndex;
		}
		else
		{
			Interp = (FLOAT)ImageIndex - Interp;
		}

		if (eMethod == PSUVIM_Linear)
		{
			Interp = 0.0f;
		}
	}
	else
	if ((eMethod == PSUVIM_Random) || (eMethod == PSUVIM_Random_Blend))
	{
		if ((LODLevel->RequiredModule->RandomImageTime == 0.0f) ||
			((Particle->RelativeTime - SubUVPayload.RandomImageTime) > LODLevel->RequiredModule->RandomImageTime) ||
			(SubUVPayload.RandomImageTime == 0.0f))
		{
			Interp = appSRand();
			ImageIndex = appTrunc(Interp * TotalSubImages);

			SubUVPayload.RandomImageTime	= Particle->RelativeTime;
		}
		else
		{
			bUpdateImage	= FALSE;
		}
		if (eMethod == PSUVIM_Random)
		{
			Interp = 0.0f;
		}
	}
	else
	{
		Interp		= 0.0f;
		ImageIndex	= 0;
	}

	return bUpdateImage;
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleSubUV::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	SubImageIndex.Distribution = Cast<UDistributionFloatConstantCurve>(StaticConstructObject(UDistributionFloatConstantCurve::StaticClass(), this));
	UDistributionFloatConstantCurve* SubImageIndexDist = Cast<UDistributionFloatConstantCurve>(SubImageIndex.Distribution);
	if (SubImageIndexDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = SubImageIndexDist->CreateNewKey(Key * 1.0f);
			SubImageIndexDist->SetKeyOut(0, KeyIndex, 0.0f);
		}
		SubImageIndexDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleSubUVMovie implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSubUVMovie);

/**
 *	Helper structure for the particle payload of the SubUVMovie module.
 */
struct FSubUVMovieParticlePayload
{
	/** The time the particle has been alive, in realtime (seconds) */
	FLOAT	Time;
	INT		CurrImage;
};


void UParticleModuleSubUVMovie::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	check(Owner->SpriteTemplate);

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	// Grab the interpolation method...
	EParticleSubUVInterpMethod eMethod = (EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
	if ((eMethod == PSUVIM_None) || (Owner->SubUVDataOffset == 0))
	{
		return;
	}

	// Movies only work w/ Linear modes...
	if ((eMethod != PSUVIM_Linear) && (eMethod != PSUVIM_Linear_Blend))
	{
		return UParticleModuleSubUV::Spawn(Owner, Offset, SpawnTime);
	}

	UParticleModuleTypeDataBase* TypeDataBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
	UBOOL bSpawn = (TypeDataBase == NULL) ? TRUE : TypeDataBase->SupportsSubUV();
	UBOOL bMeshEmitter = (TypeDataBase == NULL) ? FALSE : TypeDataBase->IsAMeshEmitter();
	if (bSpawn == TRUE)
	{
		UParticleLODLevel* HighestLODLevel = Owner->SpriteTemplate->GetLODLevel(0);
		check(HighestLODLevel);

		// This is NOT directUV!
		LODLevel->RequiredModule->bDirectUV = false;

		INT	PayloadOffset	= Owner->SubUVDataOffset;
		INT iTotalSubImages = LODLevel->RequiredModule->SubImages_Horizontal * LODLevel->RequiredModule->SubImages_Vertical;
		if (iTotalSubImages == 0)
		{
			iTotalSubImages = 1;
		}

		FLOAT baseU = 1.0f;
		FLOAT baseV = 1.0f;
		if (bMeshEmitter == TRUE)
		{
			baseU /= LODLevel->RequiredModule->SubImages_Horizontal;
			baseV /= LODLevel->RequiredModule->SubImages_Vertical;
		}

		INT iImageIndex;
		INT iImageH;
		INT iImageV;

		SPAWN_INIT;
		{
			INT	TempOffset	= CurrentOffset;
			CurrentOffset	= PayloadOffset;
			PARTICLE_ELEMENT(FFullSubUVPayload, SubUVPayload);
			CurrentOffset	= TempOffset;

			PARTICLE_ELEMENT(FSubUVMovieParticlePayload, MoviePayload);
			MoviePayload.Time = 0.0f;
			if (StartingFrame > 1)
			{
				// Clamp to the max...
				MoviePayload.CurrImage = Clamp<INT>(StartingFrame, 0, iTotalSubImages-1);
			}
			else if (StartingFrame == 0)
			{
				MoviePayload.CurrImage = appTrunc(appSRand() * (iTotalSubImages-1));
			}
			else
			{
				// Catch-all: Default to the first frame...
				MoviePayload.CurrImage = 0;
			}
			iImageIndex = MoviePayload.CurrImage;

			iImageH = iImageIndex % LODLevel->RequiredModule->SubImages_Horizontal;
			iImageV = iImageIndex / LODLevel->RequiredModule->SubImages_Horizontal;

			// Update the payload
			SubUVPayload.ImageHVInterp_UVOffset.X	= baseU * iImageH;
			SubUVPayload.ImageHVInterp_UVOffset.Y	= baseV * iImageV;
			SubUVPayload.Image2HV_UV2Offset.X		= SubUVPayload.ImageHVInterp_UVOffset.X;
			SubUVPayload.Image2HV_UV2Offset.Y		= SubUVPayload.ImageHVInterp_UVOffset.Y;
		}
	}
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */
UINT UParticleModuleSubUVMovie::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FSubUVMovieParticlePayload);
}

/**
 *	Determine the current image index to use...
 *
 *	@param	Owner					The emitter instance being updated.
 *	@param	Offset					The offset to the particle payload for this module.
 *	@param	Particle				The particle that the image index is being determined for.
 *	@param	eMethod					The EParticleSubUVInterpMethod method used to update the subUV.
 *	@param	SubUVPayload			The FFullSubUVPayload for this particle.
 *	@param	ImageIndex		[out]	The image index to use for the particle.
 *	@param	Interp			[out]	The current interpolation value (for blending 2 sub-images).
 *	@param	DeltaTime				The time slice for this update.
 *
 *	@return	UBOOL					TRUE if successful, FALSE if not.
 */
UBOOL UParticleModuleSubUVMovie::DetermineImageIndex(FParticleEmitterInstance* Owner, INT Offset, FBaseParticle* Particle, 
	EParticleSubUVInterpMethod eMethod, FFullSubUVPayload& SubUVPayload, 
	INT& ImageIndex, FLOAT& Interp, FLOAT DeltaTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	FSubUVMovieParticlePayload& MoviePayload = *((FSubUVMovieParticlePayload*)((BYTE*)Particle + Offset));
	FLOAT UserSetFrameRate = FrameRate.GetValue(bUseEmitterTime ? Owner->EmitterTime : Particle->RelativeTime, Owner->Component);
	if (bUseRealTime == FALSE)
	{
		MoviePayload.Time += DeltaTime;
	}
	else
	{
		if ((GWorld != NULL) && (GWorld->GetWorldInfo() != NULL))
		{
			MoviePayload.Time += DeltaTime / GWorld->GetWorldInfo()->TimeDilation;
		}
		else
		{
			MoviePayload.Time += DeltaTime;
		}
	}
	
	FLOAT FrameRateValue = 1.0f / UserSetFrameRate;

	INT TotalSubImages	= LODLevel->RequiredModule->SubImages_Horizontal * LODLevel->RequiredModule->SubImages_Vertical;
	ImageIndex	= appTrunc(SubUVPayload.ImageHVInterp_UVOffset.X + SubUVPayload.ImageHVInterp_UVOffset.Y * LODLevel->RequiredModule->SubImages_Horizontal);

	UBOOL bImageChanged = FALSE;
	if (MoviePayload.Time > FrameRateValue)
	{
		MoviePayload.CurrImage++;
		if (MoviePayload.CurrImage == TotalSubImages)
		{
			MoviePayload.CurrImage = 0;
		}
		bImageChanged = TRUE;
 		MoviePayload.Time -= FrameRateValue;
	}
	ImageIndex = MoviePayload.CurrImage;
	if (eMethod == PSUVIM_Linear_Blend)
	{
		Interp = Clamp<FLOAT>(MoviePayload.Time / FrameRateValue, 0.0f, 1.0f);
	}
	else
	{
		Interp = 0.0f;
	}
	return TRUE;
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleSubUVMovie::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UParticleModuleSubUV::SetToSensibleDefaults(Owner);
}

/** Fill an array with each Object property that fulfills the FCurveEdInterface interface. */
void UParticleModuleSubUVMovie::GetCurveObjects(TArray<FParticleCurvePair>& OutCurves)
{
	UParticleModule::GetCurveObjects(OutCurves);

	INT RemoveIdx = -1;
	for (INT CurveIdx = 0; CurveIdx < OutCurves.Num(); CurveIdx++)
	{
		FParticleCurvePair& Curve = OutCurves(CurveIdx);
		if (Curve.CurveName == TEXT("SubImageIndex"))
		{
			RemoveIdx = CurveIdx;
			break;
		}
	}

	if (RemoveIdx != -1)
	{
		OutCurves.Remove(RemoveIdx);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleSubUVSelect implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSubUVSelect);

void UParticleModuleSubUVSelect::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	check(Owner->SpriteTemplate);

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	// Grab the interpolation method...
	EParticleSubUVInterpMethod eMethod = (EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
	if (eMethod == PSUVIM_None)
	{
		return;
	}

	UParticleModuleTypeDataBase* TypeDataBase = Cast<UParticleModuleTypeDataBase>(LODLevel->TypeDataModule);
	UBOOL bUpdate = (TypeDataBase == NULL) ? TRUE : TypeDataBase->SupportsSubUV();
	UBOOL bMeshEmitter = (TypeDataBase == NULL) ? FALSE : TypeDataBase->IsAMeshEmitter();
	if (bUpdate == TRUE)
	{
		UParticleLODLevel* HighestLODLevel = Owner->SpriteTemplate->GetLODLevel(0);
		check(HighestLODLevel);

		LODLevel->RequiredModule->bDirectUV = false;

		INT	PayloadOffset	= Owner->SubUVDataOffset;
		INT iTotalSubImages = LODLevel->RequiredModule->SubImages_Horizontal * LODLevel->RequiredModule->SubImages_Vertical;

		FLOAT baseU = 1.0f;
		FLOAT baseV = 1.0f;
		if (bMeshEmitter == TRUE)
		{
			baseU /= LODLevel->RequiredModule->SubImages_Horizontal;
			baseV /= LODLevel->RequiredModule->SubImages_Vertical;
		}

		FLOAT	fInterp = 0.0f;
		INT     iImageH;
		INT     iImageV;
		INT     iImage2H;
		INT     iImage2V = 0;

		BEGIN_UPDATE_LOOP;
		{
			if (Particle.RelativeTime > 1.0f)
			{
				CONTINUE_UPDATE_LOOP;
			}

			if ((eMethod == PSUVIM_Linear) || (eMethod == PSUVIM_Linear_Blend))
			{
				FVector vInterp = SubImageSelect.GetValue(Particle.RelativeTime, Owner->Component);
				iImageH = appTrunc(vInterp.X);
				iImageV = appTrunc(vInterp.Y);
			}
			else
			if ((eMethod == PSUVIM_Random) || (eMethod == PSUVIM_Random_Blend))
			{
				iImageH = appTrunc(appSRand() * iTotalSubImages);
				iImageV = appTrunc(appSRand() * iTotalSubImages);
			}
			else
			{
				iImageH = 0;
				iImageV = 0;
			}

			if (iImageH == (LODLevel->RequiredModule->SubImages_Horizontal - 1))
			{
				iImage2H = 0;
				if (iImageV == (LODLevel->RequiredModule->SubImages_Vertical - 1))
				{
					iImage2V = 0;
				}
				else
				{
					iImage2V = iImageV + 1;
				}
			}
			else
			{
				iImage2H = iImageH + 1;
				iImage2V = iImageV;
			}

			// Update the payload
			FFullSubUVPayload* SubUVPayload = (FFullSubUVPayload*)(((BYTE*)&Particle) + PayloadOffset);
			
			SubUVPayload->ImageHVInterp_UVOffset.Z = fInterp;
			SubUVPayload->ImageHVInterp_UVOffset.X = baseU * (FLOAT)iImageH;
			SubUVPayload->ImageHVInterp_UVOffset.Y = baseV * (FLOAT)iImageV;
			SubUVPayload->Image2HV_UV2Offset.X = baseU * (FLOAT)iImage2H;
			SubUVPayload->Image2HV_UV2Offset.Y = baseV * (FLOAT)iImage2V;
		}
		END_UPDATE_LOOP;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleSubUVDirect implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleSubUVDirect);

void UParticleModuleSubUVDirect::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	check(Owner->SpriteTemplate);

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	EParticleSubUVInterpMethod eMethod = 
		(EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
	if (eMethod == PSUVIM_None)
	{
		return;
	}

	if (LODLevel->TypeDataModule)
	{
		// Only do SubUV on MeshEmitters - not trails or beams.
		if (LODLevel->TypeDataModule->IsA(UParticleModuleTypeDataMesh::StaticClass()))
		{
			UpdateMesh(Owner, Offset, DeltaTime);
		}
	}
	else
	{
		UpdateSprite(Owner, Offset, DeltaTime);
	}
}

void UParticleModuleSubUVDirect::UpdateSprite(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	FParticleSpriteSubUVEmitterInstance* Instance = CastEmitterInstanceChecked<FParticleSpriteSubUVEmitterInstance>(Owner);
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	UParticleLODLevel* HighestLODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(HighestLODLevel);

	LODLevel->RequiredModule->bDirectUV = true;

	INT	PayloadOffset	= Owner->SubUVDataOffset;
	INT iTotalSubImages = LODLevel->RequiredModule->SubImages_Horizontal * LODLevel->RequiredModule->SubImages_Vertical;
	EParticleSubUVInterpMethod eMethod = 
		(EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
	if ((eMethod == PSUVIM_None) || (PayloadOffset == 0))
	{
		return;
	}

	BEGIN_UPDATE_LOOP;
	{
		if (Particle.RelativeTime > 1.0f)
		{
			CONTINUE_UPDATE_LOOP;
		}

		FVector vPosition = SubUVPosition.GetValue(Particle.RelativeTime, Owner->Component);
		FVector vSize = SubUVSize.GetValue(Particle.RelativeTime, Owner->Component);

		// Update the payload
		FFullSubUVPayload* SubUVPayload = (FFullSubUVPayload*)(((BYTE*)&Particle) + PayloadOffset);
		
		SubUVPayload->ImageHVInterp_UVOffset.Z = 0.0f;
		SubUVPayload->ImageHVInterp_UVOffset.X = vPosition.X;
		SubUVPayload->ImageHVInterp_UVOffset.Y = vPosition.Y;
		SubUVPayload->Image2HV_UV2Offset.X = vSize.X;
		SubUVPayload->Image2HV_UV2Offset.Y = vSize.Y;
	}
	END_UPDATE_LOOP;
}

void UParticleModuleSubUVDirect::UpdateMesh(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	UParticleLODLevel* HighestLODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(HighestLODLevel);

	LODLevel->RequiredModule->bDirectUV = true;

	INT	PayloadOffset	= Owner->SubUVDataOffset;
	INT iTotalSubImages = LODLevel->RequiredModule->SubImages_Horizontal * LODLevel->RequiredModule->SubImages_Vertical;
	EParticleSubUVInterpMethod eMethod = 
		(EParticleSubUVInterpMethod)(LODLevel->RequiredModule->InterpolationMethod);
	if ((eMethod == PSUVIM_None) || (PayloadOffset == 0))
	{
		return;
	}

	FLOAT baseU = (1.0f / LODLevel->RequiredModule->SubImages_Horizontal);
	FLOAT baseV = (1.0f / LODLevel->RequiredModule->SubImages_Vertical);

	BEGIN_UPDATE_LOOP;
	{
		if (Particle.RelativeTime > 1.0f)
		{
			CONTINUE_UPDATE_LOOP;
		}

		FVector vPosition = SubUVPosition.GetValue(Particle.RelativeTime, Owner->Component);
		FVector vSize = SubUVSize.GetValue(Particle.RelativeTime, Owner->Component);

		// Update the payload
		FFullSubUVPayload* PayloadData = (FFullSubUVPayload*)(((BYTE*)&Particle) + PayloadOffset);

		PayloadData->ImageHVInterp_UVOffset.X	= baseU * vPosition.X;
		PayloadData->ImageHVInterp_UVOffset.Y	= baseV * vPosition.Y;
		//@todo. How to pass in the size?????
		// There is no simple way to do this without custom shader code!
	}
	END_UPDATE_LOOP;
}

/*-----------------------------------------------------------------------------
	UParticleModuleRotationRateMultiplyLife implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleRotationRateMultiplyLife);

void UParticleModuleRotationRateMultiplyLife::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
	FLOAT RateScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
	Particle.RotationRate *= RateScale;
}

void UParticleModuleRotationRateMultiplyLife::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
		FLOAT RateScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
		Particle.RotationRate *= RateScale;
	END_UPDATE_LOOP;
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleRotationRateMultiplyLife::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	LifeMultiplier.Distribution = Cast<UDistributionFloatConstantCurve>(StaticConstructObject(UDistributionFloatConstantCurve::StaticClass(), this));
	UDistributionFloatConstantCurve* LifeMultiplierDist = Cast<UDistributionFloatConstantCurve>(LifeMultiplier.Distribution);
	if (LifeMultiplierDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = LifeMultiplierDist->CreateNewKey(Key * 1.0f);
			LifeMultiplierDist->SetKeyOut(0, KeyIndex, 1.0f);
		}
		LifeMultiplierDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleAcceleration implementation.
-----------------------------------------------------------------------------*/
/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleAccelerationBase::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	bAlwaysInWorldSpace = TRUE;
	UParticleModule::SetToSensibleDefaults(Owner);
}

/*-----------------------------------------------------------------------------
	UParticleModuleAcceleration implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleAcceleration);

void UParticleModuleAcceleration::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
	PARTICLE_ELEMENT(FVector, UsedAcceleration);
 	UsedAcceleration = Acceleration.GetValue(Owner->EmitterTime, Owner->Component);
	if ((bApplyOwnerScale == TRUE) && Owner && Owner->Component)
	{
		FVector Scale = FVector(1.0f, 1.0f, 1.0f);
		Scale *= Owner->Component->Scale * Owner->Component->Scale3D;
		AActor* Actor = Owner->Component->GetOwner();
		if (Actor && !Owner->Component->AbsoluteScale)
		{
			Scale *= Actor->DrawScale * Actor->DrawScale3D;
		}
		UsedAcceleration *= Scale;
	}
 	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
 	check(LODLevel);
 	if (bAlwaysInWorldSpace && LODLevel->RequiredModule->bUseLocalSpace)
 	{
		FMatrix InvMat = Owner->Component->LocalToWorld.Inverse();
 		FVector TempUsedAcceleration = InvMat.TransformNormal(UsedAcceleration);
		Particle.Velocity		+= TempUsedAcceleration * SpawnTime;
		Particle.BaseVelocity	+= TempUsedAcceleration * SpawnTime;
 	}
	else
	{
		Particle.Velocity		+= UsedAcceleration * SpawnTime;
		Particle.BaseVelocity	+= UsedAcceleration * SpawnTime;
	}
}

void UParticleModuleAcceleration::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if ((Owner == NULL) || (Owner->ActiveParticles <= 0) || 
		(Owner->ParticleData == NULL) || (Owner->ParticleIndices == NULL))
	{
		return;
	}
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	CONSOLE_PREFETCH(Owner->ParticleData + (Owner->ParticleIndices[0] * Owner->ParticleStride));
	CONSOLE_PREFETCH_NEXT_CACHE_LINE(Owner->ParticleData + (Owner->ParticleIndices[0] * Owner->ParticleStride));
	if (bAlwaysInWorldSpace && LODLevel->RequiredModule->bUseLocalSpace)
	{
		FMatrix InvMat = Owner->Component->LocalToWorld.Inverse();
		BEGIN_UPDATE_LOOP;
		{
			FVector& UsedAcceleration = *((FVector*)(ParticleBase + CurrentOffset));																\
			FVector TransformedUsedAcceleration = InvMat.TransformNormal(UsedAcceleration);
			CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
			CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
			Particle.Velocity		+= TransformedUsedAcceleration * DeltaTime;
			Particle.BaseVelocity	+= TransformedUsedAcceleration * DeltaTime;
		}
		END_UPDATE_LOOP;
	}
	else
	{
		BEGIN_UPDATE_LOOP;
		{
			FVector& UsedAcceleration = *((FVector*)(ParticleBase + CurrentOffset));																\
			CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
			CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
			Particle.Velocity		+= UsedAcceleration * DeltaTime;
			Particle.BaseVelocity	+= UsedAcceleration * DeltaTime;
		}
		END_UPDATE_LOOP;
	}
}

UINT UParticleModuleAcceleration::RequiredBytes(FParticleEmitterInstance* Owner)
{
	// FVector UsedAcceleration
	return sizeof(FVector);
}

/*-----------------------------------------------------------------------------
	UParticleModuleAccelerationOverLifetime implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleAccelerationOverLifetime);

void UParticleModuleAccelerationOverLifetime::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (bAlwaysInWorldSpace && LODLevel->RequiredModule->bUseLocalSpace)
	{
		FMatrix InvMat = Owner->Component->LocalToWorld.Inverse();
		BEGIN_UPDATE_LOOP;
			// Acceleration should always be in world space...
			FVector Accel = AccelOverLife.GetValue(Particle.RelativeTime, Owner->Component);
			Accel = InvMat.TransformNormal(Accel);
			Particle.Velocity		+= Accel * DeltaTime;
			Particle.BaseVelocity	+= Accel * DeltaTime;
		END_UPDATE_LOOP;
	}
	else
	{
		BEGIN_UPDATE_LOOP;
		// Acceleration should always be in world space...
		FVector Accel = AccelOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		Particle.Velocity		+= Accel * DeltaTime;
		Particle.BaseVelocity	+= Accel * DeltaTime;
		END_UPDATE_LOOP;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataBase implementation.
-----------------------------------------------------------------------------*/
FParticleEmitterInstance* UParticleModuleTypeDataBase::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	return NULL;
}

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataMesh implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTypeDataMesh);

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	SetToSensibleDefaults(InEmitterParent);
	FParticleEmitterInstance* Instance = new FParticleMeshEmitterInstance();
	check(Instance);

	Instance->InitParameters(InEmitterParent, InComponent);

	return Instance;
}

void UParticleModuleTypeDataMesh::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	if ((Mesh == NULL) && (GIsGame == FALSE))
	{
		Mesh = (UStaticMesh*)UObject::StaticLoadObject(UStaticMesh::StaticClass(),NULL,TEXT("EngineMeshes.ParticleCube"),NULL,LOAD_None,NULL);
	}
}

void UParticleModuleTypeDataMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetFName() == FName(TEXT("Mesh")))
		{
			UObject* OuterObj = GetOuter();
			check(OuterObj);
			UParticleLODLevel* LODLevel = Cast<UParticleLODLevel>(OuterObj);
			if (LODLevel)
			{
				// The outer is incorrect - warn the user and handle it
				warnf(NAME_Warning, TEXT("UParticleModuleTypeDataMesh has an incorrect outer... run FixupEmitters on package %s"),
					*(OuterObj->GetOutermost()->GetPathName()));
				OuterObj = LODLevel->GetOuter();
				UParticleEmitter* Emitter = Cast<UParticleEmitter>(OuterObj);
				check(Emitter);
				OuterObj = Emitter->GetOuter();
			}
			UParticleSystem* PartSys = CastChecked<UParticleSystem>(OuterObj);

			PartSys->PostEditChangeProperty(PropertyChangedEvent);
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleTypeDataMesh::PostLoad()
{
	Super::PostLoad();
	if (GetLinker() && (GetLinker()->Ver() < VER_MESH_EMITTER_CAMERA_FACING_OPTIONS))
	{
		switch (CameraFacingUpAxisOption_DEPRECATED)
		{
		case CameraFacing_ZUp:
			CameraFacingOption = XAxisFacing_ZUp;
			break;
		case CameraFacing_NegativeZUp:
			CameraFacingOption = XAxisFacing_NegativeZUp;
			break;
		case CameraFacing_YUp:
			CameraFacingOption = XAxisFacing_YUp;
			break;
		case CameraFacing_NegativeYUp:
			CameraFacingOption = XAxisFacing_NegativeYUp;
			break;
		}
	}
}

/**
 *	Called on a particle that is freshly spawned by the emitter.
 *	
 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
 *	@param	Offset		The modules offset into the data payload of the particle.
 *	@param	SpawnTime	The time of the spawn.
 */
void UParticleModuleTypeDataMesh::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleMeshEmitterInstance* MeshEmitterInstance = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
	if (MeshEmitterInstance != NULL)
	{
		FMeshTypeDataInstancePayload* InstanceData = (FMeshTypeDataInstancePayload*)Owner->GetTypeDataModuleInstanceData();
		if (InstanceData != NULL)
		{
			SPAWN_INIT;
			PARTICLE_ELEMENT(FMeshTypeDataPayload, DataPayload);
			// increment instance Id at spawn time so that we have a unique Id for each particle
			DataPayload.ParticleId = InstanceData->ParticleId++;
		}
	}
}

/**
 *	Returns the number of bytes that the module requires in the particle payload block.
 *
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return	UINT		The number of bytes the module needs per particle.
 */	
UINT UParticleModuleTypeDataMesh::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return bAllowMotionBlur ? sizeof(FMeshTypeDataPayload) : 0;
}

/**
 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *
 *	@return UINT		The number of bytes the module needs per emitter instance.
 */
UINT UParticleModuleTypeDataMesh::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	UINT Size=0;
	if (bAllowMotionBlur)
	{
		Size += sizeof(FMeshTypeDataInstancePayload);
	}
	return Size;
}

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataTrail implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTypeDataTrail);

FParticleEmitterInstance* UParticleModuleTypeDataTrail::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	SetToSensibleDefaults(InEmitterParent);
	FParticleEmitterInstance* Instance = new FParticleTrailEmitterInstance();
	check(Instance);

	Instance->InitParameters(InEmitterParent, InComponent);

	return Instance;
}

void UParticleModuleTypeDataTrail::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataBeam implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleTypeDataBeam);

void UParticleModuleTypeDataBeam::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	if (EndPointMethod == PEBEPM_Distribution_Constant)
	{
		SPAWN_INIT;
		PARTICLE_ELEMENT(FVector, Tangent);
		Tangent = EndPointDirection.GetValue(Owner->EmitterTime, Owner->Component);
	}
}

FVector UParticleModuleTypeDataBeam::DetermineEndPointPosition(FParticleEmitterInstance* Owner, FLOAT DeltaTime)
{
	FVector	vPosition	= Owner->Component->LocalToWorld.GetOrigin();
	FLOAT	fLoopTime	= Owner->EmitterTime;

	FParticleBeamEmitterInstance* pkBeamInst = CastEmitterInstance<FParticleBeamEmitterInstance>(Owner);
	check(pkBeamInst);
	return vPosition;
}

FVector UParticleModuleTypeDataBeam::DetermineParticlePosition(FParticleEmitterInstance* Owner, FBaseParticle* pkParticle, FLOAT DeltaTime)
{
	FVector	vPosition	= Owner->Component->LocalToWorld.GetOrigin();
	return vPosition;
}

UINT UParticleModuleTypeDataBeam::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return (EndPointMethod == PEBEPM_Distribution_Constant) ? sizeof(FVector) : 0;
}

FParticleEmitterInstance* UParticleModuleTypeDataBeam::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	SetToSensibleDefaults(InEmitterParent);
	FParticleEmitterInstance* Instance = new FParticleBeamEmitterInstance();
	check(Instance);

	Instance->InitParameters(InEmitterParent, InComponent);

	return Instance;
}

void UParticleModuleTypeDataBeam::PreUpdate(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	check(!TEXT("BeamEmitterInstance deprecated"));
}

void UParticleModuleTypeDataBeam::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*-----------------------------------------------------------------------------
	UParticleModuleUberBase implementation.
-----------------------------------------------------------------------------*/
/** This function will determine the proper uber-module to utilize.					*/
UParticleModule* UParticleModuleUberBase::DetermineBestUberModule(UParticleEmitter* InputEmitter)
{
	// Check each available uber module to see if it is compatible...
	// Go largest module replacement count to smallest...
	UParticleModuleUberLTISIVCLILIRSSBLIRR* LTISIVCLILIRSSBLIRR = 
		ConstructObject<UParticleModuleUberLTISIVCLILIRSSBLIRR>(
			UParticleModuleUberLTISIVCLILIRSSBLIRR::StaticClass(), 
			InputEmitter->GetOuter());
	if (LTISIVCLILIRSSBLIRR)
	{
		if (LTISIVCLILIRSSBLIRR->IsCompatible(InputEmitter))
		{
			// We've got one...
			return LTISIVCLILIRSSBLIRR;
		}
	}

	UParticleModuleUberLTISIVCLIL* LTISIVCLIL = 
		ConstructObject<UParticleModuleUberLTISIVCLIL>(
			UParticleModuleUberLTISIVCLIL::StaticClass(), 
			InputEmitter->GetOuter());
	if (LTISIVCLIL)
	{
		if (LTISIVCLIL->IsCompatible(InputEmitter))
		{
			// We've got one...
			return LTISIVCLIL;
		}
	}

	UParticleModuleUberLTISIVCL* LTISIVCL = 
		ConstructObject<UParticleModuleUberLTISIVCL>(
			UParticleModuleUberLTISIVCL::StaticClass(), 
			InputEmitter->GetOuter());
	if (LTISIVCL)
	{
		if (LTISIVCL->IsCompatible(InputEmitter))
		{
			// We've got one...
			return LTISIVCL;
		}
	}

	return NULL;
}

/** Used by derived classes to indicate they could be used on the given emitter.	*/
UBOOL UParticleModuleUberBase::IsCompatible(UParticleEmitter* InputEmitter)
{
	return FALSE;
}

/** Copy the contents of the modules to the UberModule								*/
UBOOL UParticleModuleUberBase::ConvertToUberModule(UParticleEmitter* InputEmitter)
{
	return FALSE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleUberLTISIVCL implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleUberLTISIVCL);

/** Copy the contents of the modules to the UberModule								*/
UBOOL UParticleModuleUberLTISIVCL::ConvertToUberModule(UParticleEmitter* InputEmitter)
{
	return FALSE;
}

/** Spawn - called when spawning particles											*/
#if defined(_PSYS_PASS_PARTICLE_)
void UParticleModuleUberLTISIVCL::Spawn(FParticleEmitterInstance* Owner, FBaseParticle* Particle, 
		const UINT ActiveParticles, const UINT ParticleStride, INT ParticleIndex, 
		INT Offset, FLOAT SpawnTime)
{
	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if(Particle->OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle->OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle->OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle->OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle->RelativeTime = SpawnTime * Particle->OneOverMaxLifetime;

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle->Size		+= Size;
	Particle->BaseSize	+= Size;

	// Velocity
	FVector FromOrigin;
	FVector Vel = StartVelocity.GetValue(Owner->EmitterTime, Owner->Component);
	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		FromOrigin = Particle->Location.SafeNormal();
	}
	else
	{
		FromOrigin = (Particle->Location - Owner->Location).SafeNormal();
		Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
	}

	Vel += FromOrigin * StartVelocityRadial.GetValue(Owner->EmitterTime, Owner->Component);
	Particle->Velocity		+= Vel;
	Particle->BaseVelocity	+= Vel;

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle->RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle->RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle->Color);
}
#else	//#if defined(_PSYS_PASS_PARTICLE_)
void UParticleModuleUberLTISIVCL::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;

	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if(Particle.OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;

	// Velocity
	FVector FromOrigin;
	FVector Vel = StartVelocity.GetValue(Owner->EmitterTime, Owner->Component);
	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		FromOrigin = Particle.Location.SafeNormal();
	}
	else
	{
		FromOrigin = (Particle.Location - Owner->Location).SafeNormal();
		Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
	}

	Vel += FromOrigin * StartVelocityRadial.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Velocity		+= Vel;
	Particle.BaseVelocity	+= Vel;

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);
}
#endif	//#if defined(_PSYS_PASS_PARTICLE_)

/** Update - called when updating particles											*/
void UParticleModuleUberLTISIVCL::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	{
		// Lifetime
		//@todo. Remove this commented out code, and in turn, the Update function 
		// of the Lifetime module?
		// Updating of relative time is a common occurance in any emitter, so that 
		// was moved up to the base emitter class. In the Tick function, when 
		// resetting the velocity and size, the following also occurs:
		//     Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
		// The killing of particles was moved to a virtual function, KillParticles, 
		// in the UParticleEmitterInstance class. This allows for custom emitters, 
		// such as the MeshEmitter, to properly 'kill' a particle - in that case, 
		// removing the mesh from the scene.

		// Size
		// No Update operation required

		// Velocity
		// No Update operation required

		// ColorOverLife
		FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);
	}
	END_UPDATE_LOOP;
}

/*-----------------------------------------------------------------------------
	UParticleModuleUberLTISIVCLIL implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleUberLTISIVCLIL);

/** Copy the contents of the modules to the UberModule								*/
UBOOL UParticleModuleUberLTISIVCLIL::ConvertToUberModule(UParticleEmitter* InputEmitter)
{
	return FALSE;
}

/** Spawn - called when spawning particles											*/
#if defined(_PSYS_PASS_PARTICLE_)
void UParticleModuleUberLTISIVCLIL::Spawn(FParticleEmitterInstance* Owner, FBaseParticle* Particle, 
		const UINT ActiveParticles, const UINT ParticleStride, INT ParticleIndex, 
		INT Offset, FLOAT SpawnTime)
{
	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if(Particle->OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle->OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle->OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle->OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle->RelativeTime = SpawnTime * Particle->OneOverMaxLifetime;

	// Location
	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		Particle->Location += StartLocation.GetValue(Owner->EmitterTime, Owner->Component);
	}
	else
	{
		FVector StartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component);
		StartLoc = Owner->Component->LocalToWorld.TransformNormal(StartLoc);
		Particle->Location += StartLoc;
	}

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle->Size		+= Size;
	Particle->BaseSize	+= Size;

	// Velocity
	FVector FromOrigin;
	FVector Vel = StartVelocity.GetValue(Owner->EmitterTime, Owner->Component);
	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		FromOrigin = Particle->Location.SafeNormal();
	}
	else
	{
		FromOrigin = (Particle->Location - Owner->Location).SafeNormal();
		Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
	}

	Vel += FromOrigin * StartVelocityRadial.GetValue(Owner->EmitterTime, Owner->Component);
	Particle->Velocity		+= Vel;
	Particle->BaseVelocity	+= Vel;

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle->RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle->RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle->Color);
}
#else	//#if defined(_PSYS_PASS_PARTICLE_)
void UParticleModuleUberLTISIVCLIL::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;

	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if(Particle.OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;

	// Location
	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		Particle.Location += StartLocation.GetValue(Owner->EmitterTime, Owner->Component);
	}
	else
	{
		FVector StartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component);
		StartLoc = Owner->Component->LocalToWorld.TransformNormal(StartLoc);
		Particle.Location += StartLoc;
	}

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;

	// Velocity
	FVector FromOrigin;
	FVector Vel = StartVelocity.GetValue(Owner->EmitterTime, Owner->Component);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		FromOrigin = Particle.Location.SafeNormal();
	}
	else
	{
		FromOrigin = (Particle.Location - Owner->Location).SafeNormal();
		Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
	}

	Vel += FromOrigin * StartVelocityRadial.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Velocity		+= Vel;
	Particle.BaseVelocity	+= Vel;

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);
}
#endif	//#if defined(_PSYS_PASS_PARTICLE_)

/** Update - called when updating particles											*/
void UParticleModuleUberLTISIVCLIL::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	{
		// Lifetime
		//@todo. Remove this commented out code, and in turn, the Update function 
		// of the Lifetime module?
		// Updating of relative time is a common occurance in any emitter, so that 
		// was moved up to the base emitter class. In the Tick function, when 
		// resetting the velocity and size, the following also occurs:
		//     Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
		// The killing of particles was moved to a virtual function, KillParticles, 
		// in the UParticleEmitterInstance class. This allows for custom emitters, 
		// such as the MeshEmitter, to properly 'kill' a particle - in that case, 
		// removing the mesh from the scene.

		// Location
		// No Update operation required

		// Size
		// No Update operation required

		// Velocity
		// No Update operation required

		// ColorOverLife
		FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);
	}
	END_UPDATE_LOOP;
}

/*-----------------------------------------------------------------------------
	UParticleModuleUberLTISIVCLILIRSSBLIRR implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleUberLTISIVCLILIRSSBLIRR);

/** Copy the contents of the modules to the UberModule								*/
UBOOL UParticleModuleUberLTISIVCLILIRSSBLIRR::ConvertToUberModule(UParticleEmitter* InputEmitter)
{
	return FALSE;
}

/** Spawn - called when spawning particles											*/
#if defined(_PSYS_PASS_PARTICLE_)
void UParticleModuleUberLTISIVCLILIRSSBLIRR::Spawn(FParticleEmitterInstance* Owner, FBaseParticle* Particle, 
		const UINT ActiveParticles, const UINT ParticleStride, INT ParticleIndex, 
		INT Offset, FLOAT SpawnTime)
{
	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if(Particle->OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle->OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle->OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle->OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle->RelativeTime = SpawnTime * Particle->OneOverMaxLifetime;

	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(LODLevel);

	// Location
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		Particle->Location += StartLocation.GetValue(Owner->EmitterTime, Owner->Component);
	}
	else
	{
		FVector StartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component);
		StartLoc = Owner->Component->LocalToWorld.TransformNormal(StartLoc);
		Particle->Location += StartLoc;
	}

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle->Size		+= Size;
	Particle->BaseSize	+= Size;

	// Velocity
	FVector FromOrigin;
	FVector Vel = StartVelocity.GetValue(Owner->EmitterTime, Owner->Component);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		FromOrigin = Particle->Location.SafeNormal();
	}
	else
	{
		FromOrigin = (Particle->Location - Owner->Location).SafeNormal();
		Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
	}

	Vel += FromOrigin * StartVelocityRadial.GetValue(Owner->EmitterTime, Owner->Component);
	Particle->Velocity		+= Vel;
	Particle->BaseVelocity	+= Vel;

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle->RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle->RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle->Color);

	// Rotation
	Particle->Rotation += (PI/180.f) * 360.0f * StartRotation.GetValue(Owner->EmitterTime, Owner->Component);

	// SizeMultipleByLife
	FVector SizeScale = LifeMultiplier.GetValue(Particle->RelativeTime, Owner->Component);
	if (MultiplyX)
	{
		Particle->Size.X *= SizeScale.X;
	}
	if (MultiplyY)
	{
		Particle->Size.Y *= SizeScale.Y;
	}
	if (MultiplyZ)
	{
		Particle->Size.Z *= SizeScale.Z;
	}

	// RotationRate
	FLOAT StartRotRate = (PI/180.f) * 360.0f * StartRotationRate.GetValue(Owner->EmitterTime, Owner->Component);
	Particle->RotationRate += StartRotRate;
	Particle->BaseRotationRate += StartRotRate;
}
#else	//#if defined(_PSYS_PASS_PARTICLE_)
void UParticleModuleUberLTISIVCLILIRSSBLIRR::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;

	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if(Particle.OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;

	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(LODLevel);

	// Location
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		Particle.Location += StartLocation.GetValue(Owner->EmitterTime, Owner->Component);
	}
	else
	{
		FVector StartLoc = StartLocation.GetValue(Owner->EmitterTime, Owner->Component);
		StartLoc = Owner->Component->LocalToWorld.TransformNormal(StartLoc);
		Particle.Location += StartLoc;
	}

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;

	// Velocity
	FVector FromOrigin;
	FVector Vel = StartVelocity.GetValue(Owner->EmitterTime, Owner->Component);
	if (LODLevel->RequiredModule->bUseLocalSpace)
	{
		FromOrigin = Particle.Location.SafeNormal();
	}
	else
	{
		FromOrigin = (Particle.Location - Owner->Location).SafeNormal();
		Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
	}

	Vel += FromOrigin * StartVelocityRadial.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Velocity		+= Vel;
	Particle.BaseVelocity	+= Vel;

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);

	// Rotation
	Particle.Rotation += (PI/180.f) * 360.0f * StartRotation.GetValue(Owner->EmitterTime, Owner->Component);

	// SizeMultipleByLife
	FVector SizeScale = SizeLifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
	if (SizeMultiplyX)
	{
		Particle.Size.X *= SizeScale.X;
	}
	if (SizeMultiplyY)
	{
		Particle.Size.Y *= SizeScale.Y;
	}
	if (SizeMultiplyZ)
	{
		Particle.Size.Z *= SizeScale.Z;
	}

	// RotationRate
	FLOAT StartRotRate = (PI/180.f) * 360.0f * StartRotationRate.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.RotationRate += StartRotRate;
	Particle.BaseRotationRate += StartRotRate;
}
#endif	//#if defined(_PSYS_PASS_PARTICLE_)

/** Update - called when updating particles											*/
void UParticleModuleUberLTISIVCLILIRSSBLIRR::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	{
		// Lifetime
		//@todo. Remove this commented out code, and in turn, the Update function 
		// of the Lifetime module?
		// Updating of relative time is a common occurance in any emitter, so that 
		// was moved up to the base emitter class. In the Tick function, when 
		// resetting the velocity and size, the following also occurs:
		//     Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
		// The killing of particles was moved to a virtual function, KillParticles, 
		// in the UParticleEmitterInstance class. This allows for custom emitters, 
		// such as the MeshEmitter, to properly 'kill' a particle - in that case, 
		// removing the mesh from the scene.

		// Location
		// No Update operation required

		// Size
		// No Update operation required

		// Velocity
		// No Update operation required

		// ColorOverLife
		FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);

		// Rotation
		// No Update operation required

		// SizeMultipleLife
		FVector SizeScale = SizeLifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
		if (SizeMultiplyX)
		{
			Particle.Size.X *= SizeScale.X;
		}
		if (SizeMultiplyY)
		{
			Particle.Size.Y *= SizeScale.Y;
		}
		if (SizeMultiplyZ)
		{
			Particle.Size.Z *= SizeScale.Z;
		}

		// RotationRate
		// No Update operation required
	}
	END_UPDATE_LOOP;
}

/*-----------------------------------------------------------------------------
	UParticleModuleUberRainDrops implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleUberRainDrops);

void UParticleModuleUberRainDrops::PostLoad()
{
	Super::PostLoad();
}

void UParticleModuleUberRainDrops::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	UBOOL	bUseLocalSpace	= LODLevel->RequiredModule->bUseLocalSpace;


	FLOAT	Rand;

	SPAWN_INIT;

	// Lifetime
	Rand = appSRand();
	FLOAT MaxLifetime = (LifetimeMax * Rand) + (LifetimeMin * (1.0f - Rand));
	if (Particle.OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;

	// Size
	FVector Size;
	Rand = appSRand();
	Size.X		= (StartSizeMax.X * Rand) + (StartSizeMin.X * (1.0f - Rand));
	Rand = appSRand();
	Size.Y		= (StartSizeMax.Y * Rand) + (StartSizeMin.Y * (1.0f - Rand));
	Rand = appSRand();
	Size.Z		= (StartSizeMax.Z * Rand) + (StartSizeMin.Z * (1.0f - Rand));
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;

	// Velocity
	FVector FromOrigin;
	FVector Vel;

	Rand = appSRand();
	Vel.X = (StartVelocityMax.X * Rand) + (StartVelocityMin.X * (1.0f - Rand));
	Rand = appSRand();
	Vel.Y = (StartVelocityMax.Y * Rand) + (StartVelocityMin.Y * (1.0f - Rand));
	Rand = appSRand();
	Vel.Z = (StartVelocityMax.Z * Rand) + (StartVelocityMin.Z * (1.0f - Rand));

	if (bUseLocalSpace)
	{
		FromOrigin = Particle.Location.SafeNormal();
	}
	else
	{
		FromOrigin = (Particle.Location - Owner->Location).SafeNormal();
		Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
	}

	Rand = appSRand();
	Vel += FromOrigin * ((StartVelocityRadialMax * Rand) + (StartVelocityRadialMax * (1.0f - Rand)));
	Particle.Velocity		+= Vel;
	Particle.BaseVelocity	+= Vel;

	// ColorOverLife
	Particle_SetColorFromVector(ColorOverLife, AlphaOverLife, Particle.Color);
	Particle.BaseColor	= Particle.Color;

	// Cylinder
	if (bIsUsingCylinder)
	{
		INT	RadialIndex0	= 0;	//X
		INT	RadialIndex1	= 1;	//Y
		INT	HeightIndex		= 2;	//Z

		switch (PC_HeightAxis)
		{
		case PMLPC_HEIGHTAXIS_X:
			RadialIndex0	= 1;	//Y
			RadialIndex1	= 2;	//Z
			HeightIndex		= 0;	//X
			break;
		case PMLPC_HEIGHTAXIS_Y:
			RadialIndex0	= 0;	//X
			RadialIndex1	= 2;	//Z
			HeightIndex		= 1;	//Y
			break;
		case PMLPC_HEIGHTAXIS_Z:
			break;
		}

		// Determine the unit direction
		FVector UnitDir, UnitDirTemp;
		DetermineUnitDirection(Owner, UnitDirTemp);
		UnitDir[RadialIndex0]	= UnitDirTemp[RadialIndex0];
		UnitDir[RadialIndex1]	= UnitDirTemp[RadialIndex1];
		UnitDir[HeightIndex]	= UnitDirTemp[HeightIndex];

		FVector NormalizedDir = UnitDir;
		NormalizedDir.Normalize();

		FVector2D UnitDir2D(UnitDir[RadialIndex0], UnitDir[RadialIndex1]);
		FVector2D NormalizedDir2D = UnitDir2D.SafeNormal();

		// Determine the position
		FVector Offset(0.0f);

		// Always want Z in the [-Height, Height] range
		Offset[HeightIndex] = UnitDir[HeightIndex] * PC_StartHeight;

		NormalizedDir[RadialIndex0] = NormalizedDir2D.X;
		NormalizedDir[RadialIndex1] = NormalizedDir2D.Y;

		if (bSurfaceOnly)
		{
			// Clamp the X,Y to the outer edge...

			if (Abs(Offset[HeightIndex]) == PC_StartHeight)
			{
				// On the caps, it can be anywhere within the 'circle'
				Offset[RadialIndex0] = UnitDir[RadialIndex0] * PC_StartRadius;
				Offset[RadialIndex1] = UnitDir[RadialIndex1] * PC_StartRadius;
			}
			else
			{
				// On the sides, it must be on the 'circle'
				Offset[RadialIndex0] = NormalizedDir[RadialIndex0] * PC_StartRadius;
				Offset[RadialIndex1] = NormalizedDir[RadialIndex1] * PC_StartRadius;
			}
		}
		else
		{
			Offset[RadialIndex0] = UnitDir[RadialIndex0] * PC_StartRadius;
			Offset[RadialIndex1] = UnitDir[RadialIndex1] * PC_StartRadius;
		}

		// Clamp to the radius...
		FVector	Max;
		
		Max[RadialIndex0]	= Abs(NormalizedDir[RadialIndex0]) * PC_StartRadius;
		Max[RadialIndex1]	= Abs(NormalizedDir[RadialIndex1]) * PC_StartRadius;
		Max[HeightIndex]	= PC_StartHeight;

		Offset[RadialIndex0]	= Clamp<FLOAT>(Offset[RadialIndex0], -Max[RadialIndex0], Max[RadialIndex0]);
		Offset[RadialIndex1]	= Clamp<FLOAT>(Offset[RadialIndex1], -Max[RadialIndex1], Max[RadialIndex1]);
		Offset[HeightIndex]		= Clamp<FLOAT>(Offset[HeightIndex],  -Max[HeightIndex],  Max[HeightIndex]);

		// Add in the start location
		Offset[RadialIndex0]	+= PC_StartLocation[RadialIndex0];
		Offset[RadialIndex1]	+= PC_StartLocation[RadialIndex1];
		Offset[HeightIndex]		+= PC_StartLocation[HeightIndex];

		if (bUseLocalSpace == FALSE)
		{
			Offset = Owner->Component->LocalToWorld.TransformNormal(Offset);
		}
		Particle.Location += Offset;

		if (bVelocity)
		{
			FVector Velocity;
			Velocity[RadialIndex0]	= Offset[RadialIndex0]	- PC_StartLocation[RadialIndex0];
			Velocity[RadialIndex1]	= Offset[RadialIndex1]	- PC_StartLocation[RadialIndex1];
			Velocity[HeightIndex]	= Offset[HeightIndex]	- PC_StartLocation[HeightIndex];

			if (bRadialVelocity)
			{
				Velocity[HeightIndex]	= 0.0f;
			}
			Velocity	*= PC_VelocityScale;

			Particle.Velocity		+= Velocity;
			Particle.BaseVelocity	+= Velocity;
		}
	}

	// Location
	FVector	StartLoc;

	Rand	= appSRand();
	StartLoc.X	= (StartLocationMax.X * Rand) + (StartLocationMin.X * (1.0f - Rand));
	Rand	= appSRand();
	StartLoc.Y	= (StartLocationMax.Y * Rand) + (StartLocationMin.Y * (1.0f - Rand));
	Rand	= appSRand();
	StartLoc.Z	= (StartLocationMax.Z * Rand) + (StartLocationMin.Z * (1.0f - Rand));

	if (LODLevel->RequiredModule->bUseLocalSpace == FALSE)
	{
		StartLoc = Owner->Component->LocalToWorld.TransformNormal(StartLoc);
	}
	Particle.Location += StartLoc;
}

void UParticleModuleUberRainDrops::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
}

void UParticleModuleUberRainDrops::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Lifetime

	// Size

	// Velocity

	// ColorOverLife

	// Cylinder

	// Location
}

void UParticleModuleUberRainDrops::DetermineUnitDirection(FParticleEmitterInstance* Owner, FVector& UnitDir)
{
	FVector Rand;

	// Grab 3 random numbers for the axes
	Rand.X = appSRand();
	Rand.Y = appSRand();
	Rand.Z = appSRand();

	// Set the unit dir
	if (bPositive_X && bNegative_X)
	{
		UnitDir.X = Rand.X * 2 - 1;
	}
	else
	if (bPositive_X)
	{
		UnitDir.X = Rand.X;
	}
	else
	if (bNegative_X)
	{
		UnitDir.X = -Rand.X;
	}
	else
	{
		UnitDir.X = 0.0f;
	}

	if (bPositive_Y && bNegative_Y)
	{
		UnitDir.Y = Rand.Y * 2 - 1;
	}
	else
	if (bPositive_Y)
	{
		UnitDir.Y = Rand.Y;
	}
	else
	if (bNegative_Y)
	{
		UnitDir.Y = -Rand.Y;
	}
	else
	{
		UnitDir.Y = 0.0f;
	}

	if (bPositive_Z && bNegative_Z)
	{
		UnitDir.Z = Rand.Z * 2 - 1;
	}
	else
	if (bPositive_Z)
	{
		UnitDir.Z = Rand.Z;
	}
	else
	if (bNegative_Z)
	{
		UnitDir.Z = -Rand.Z;
	}
	else
	{
		UnitDir.Z = 0.0f;
	}
}

UBOOL UParticleModuleUberRainDrops::IsCompatible(UParticleEmitter* InputEmitter)
{
	UBOOL bFoundAll	= FALSE;

	if (InputEmitter)
	{
		UParticleLODLevel* LODLevel	= InputEmitter->LODLevels(0);
		check(LODLevel);

		if (LODLevel->Modules.Num() == 5)
		{
			// They MUST be in this order
			// Lifetime
			// Size
			// Velocity
			// ColorOverLife
			// Location
			if (LODLevel->Modules(0)->IsA(UParticleModuleLifetime::StaticClass())					&&
				LODLevel->Modules(1)->IsA(UParticleModuleSize::StaticClass())						&&
				LODLevel->Modules(2)->IsA(UParticleModuleVelocity::StaticClass())					&&
				LODLevel->Modules(3)->IsA(UParticleModuleColorOverLife::StaticClass())				&&
				LODLevel->Modules(4)->IsA(UParticleModuleLocation::StaticClass()))
			{
				bFoundAll	= TRUE;
			}
		}
		else
		if (LODLevel->Modules.Num() == 6)
		{
			// They MUST be in this order
			// Lifetime
			// Size
			// Velocity
			// ColorOverLife
			// Cylinder
			// Location
			if (LODLevel->Modules(0)->IsA(UParticleModuleLifetime::StaticClass())					&&
				LODLevel->Modules(1)->IsA(UParticleModuleSize::StaticClass())						&&
				LODLevel->Modules(2)->IsA(UParticleModuleVelocity::StaticClass())					&&
				LODLevel->Modules(3)->IsA(UParticleModuleColorOverLife::StaticClass())				&&
				LODLevel->Modules(4)->IsA(UParticleModuleLocationPrimitiveCylinder::StaticClass())	&&
				LODLevel->Modules(5)->IsA(UParticleModuleLocation::StaticClass()))
			{
				bFoundAll	= TRUE;
			}
		}
	}

	return bFoundAll;
}

UBOOL UParticleModuleUberRainDrops::ConvertToUberModule(UParticleEmitter* InputEmitter)
{
	if (InputEmitter->LODLevels.Num() > 2)
	{
		appMsgf(AMT_OK, TEXT("Can't convert an emitter with specific LOD levels!"));
		return FALSE;
	}

	// Lifetime
	// Size
	// Velocity
	// ColorOverLife
	// Cylinder
	// Location
	UParticleModuleLifetime*					LifetimeModule		= NULL;
	UParticleModuleSize*						SizeModule			= NULL;
	UParticleModuleVelocity*					VelocityModule		= NULL;
	UParticleModuleColorOverLife*				ColorOverLifeModule	= NULL;
	UParticleModuleLocationPrimitiveCylinder*	CylinderModule		= NULL;
	UParticleModuleLocation*					LocationModule		= NULL;
	
	bIsUsingCylinder = FALSE;

	// Copy the module data from each module to the new one...
	UParticleLODLevel* LODLevel	= InputEmitter->LODLevels(0);
	check(LODLevel);

	for (INT SourceModuleIndex = 0; SourceModuleIndex < LODLevel->Modules.Num(); SourceModuleIndex++)
	{
		UParticleModule* SourceModule	= LODLevel->Modules(SourceModuleIndex);

		// Lifetime
		if (SourceModule->IsA(UParticleModuleLifetime::StaticClass()))
		{
			// Copy the contents
			if (LifetimeModule == NULL)
			{
				LifetimeModule	= Cast<UParticleModuleLifetime>(SourceModule);
				LifetimeModule->Lifetime.Distribution->GetOutRange(LifetimeMin, LifetimeMax);
			}
		}
		else
		// Size
		if (SourceModule->IsA(UParticleModuleSize::StaticClass()))
		{
			if (SizeModule == NULL)
			{
				SizeModule		= Cast<UParticleModuleSize>(SourceModule);
				SizeModule->StartSize.Distribution->GetRange(StartSizeMin, StartSizeMax);
			}
		}
		else
		// Velocity
		if (SourceModule->IsA(UParticleModuleVelocity::StaticClass()))
		{
			if (VelocityModule == NULL)
			{
				VelocityModule		= Cast<UParticleModuleVelocity>(SourceModule);
				VelocityModule->StartVelocity.Distribution->GetRange(StartVelocityMin, StartVelocityMax);
				VelocityModule->StartVelocityRadial.Distribution->GetOutRange(StartVelocityRadialMin, StartVelocityRadialMax);
			}
		}
		else
		// ColorOverLife
		if (SourceModule->IsA(UParticleModuleColorOverLife::StaticClass()))
		{
			if (ColorOverLifeModule == NULL)
			{
				ColorOverLifeModule	= Cast<UParticleModuleColorOverLife>(SourceModule);
				ColorOverLifeModule->ColorOverLife.Distribution->GetRange(ColorOverLife, ColorOverLife);
				ColorOverLifeModule->AlphaOverLife.Distribution->GetOutRange(AlphaOverLife, AlphaOverLife);
			}
		}
		else
		// PrimitiveCylinder
		if (SourceModule->IsA(UParticleModuleLocationPrimitiveCylinder::StaticClass()))
		{
			if (CylinderModule == NULL)
			{
				CylinderModule	= Cast<UParticleModuleLocationPrimitiveCylinder>(SourceModule);

				bIsUsingCylinder	= TRUE;
				bPositive_X			= CylinderModule->Positive_X;
				bPositive_Y			= CylinderModule->Positive_Y;
				bPositive_Z			= CylinderModule->Positive_Z;
				bNegative_X			= CylinderModule->Negative_X;
				bNegative_Y			= CylinderModule->Negative_Y;
				bNegative_Z			= CylinderModule->Negative_Z;
				bSurfaceOnly		= CylinderModule->SurfaceOnly;
				bVelocity			= CylinderModule->Velocity;
				bRadialVelocity		= CylinderModule->RadialVelocity;
				CylinderModule->VelocityScale.Distribution->GetOutRange(PC_VelocityScale, PC_VelocityScale);
				CylinderModule->StartLocation.Distribution->GetRange(PC_StartLocation, PC_StartLocation);
				CylinderModule->StartRadius.Distribution->GetOutRange(PC_StartRadius, PC_StartRadius);
				CylinderModule->StartHeight.Distribution->GetOutRange(PC_StartHeight, PC_StartHeight);
				PC_HeightAxis		= CylinderModule->HeightAxis;
			}
		}
		else
		// InitialLocation
		if (SourceModule->IsA(UParticleModuleLocation::StaticClass()))
		{
			if (LocationModule == NULL)
			{
				LocationModule	= Cast<UParticleModuleLocation>(SourceModule);
				LocationModule->StartLocation.Distribution->GetRange(StartLocationMin, StartLocationMax);
			}
		}
		else
		{
			// Just leave the module in its place...
		}
	}

	// Ensure that we found all the required modules
	if (LODLevel->Modules.Num() == 5)
	{
		if ((LifetimeModule			== NULL) || 
			(SizeModule				== NULL) || 
			(VelocityModule			== NULL) || 
			(ColorOverLifeModule	== NULL) || 
			(LocationModule			== NULL))
		{
			// Failed the conversion!
			return FALSE;
		}
	}
	else
	if (LODLevel->Modules.Num() == 6)
	{
		if ((LifetimeModule			== NULL) || 
			(SizeModule				== NULL) || 
			(VelocityModule			== NULL) || 
			(ColorOverLifeModule	== NULL) || 
			(CylinderModule			== NULL) || 
			(LocationModule			== NULL))
		{
			// Failed the conversion!
			return FALSE;
		}
	}

	// Remove the modules
	LODLevel->Modules.RemoveItem(LifetimeModule);
	LODLevel->Modules.RemoveItem(SizeModule);
	LODLevel->Modules.RemoveItem(VelocityModule);
	LODLevel->Modules.RemoveItem(ColorOverLifeModule);
	if (CylinderModule)
	{
		LODLevel->Modules.RemoveItem(CylinderModule);
	}
	LODLevel->Modules.RemoveItem(LocationModule);

	// Add the Uber
	LODLevel->Modules.AddItem(this);

	//
	LODLevel->UpdateModuleLists();

	return TRUE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleUberRainImpacts implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleUberRainImpacts);

void UParticleModuleUberRainImpacts::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	FParticleMeshEmitterInstance* MeshInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	UBOOL	bUseLocalSpace	= LODLevel->RequiredModule->bUseLocalSpace;

	SPAWN_INIT;

	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if (Particle.OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;

	// InitialMeshRotation
	if (MeshInst)
	{
		FVector Rotation = StartRotation.GetValue(Owner->EmitterTime, Owner->Component);

		if (bInheritParent)
		{
			FRotator	Rotator	= Owner->Component->LocalToWorld.Rotator();
			FVector		ParentAffectedRotation	= Rotator.Euler();
			Rotation.X	+= ParentAffectedRotation.X / 360.0f;
			Rotation.Y	+= ParentAffectedRotation.Y / 360.0f;
			Rotation.Z	+= ParentAffectedRotation.Z / 360.0f;
		}

		FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
		PayloadData->Rotation.X	+= Rotation.X * 360.0f;
		PayloadData->Rotation.Y	+= Rotation.Y * 360.0f;
		PayloadData->Rotation.Z	+= Rotation.Z * 360.0f;
	}

	// SizeByLife
	FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
	if(MultiplyX)
	{
		Particle.Size.X *= SizeScale.X;
	}
	if(MultiplyY)
	{
		Particle.Size.Y *= SizeScale.Y;
	}
	if(MultiplyZ)
	{
		Particle.Size.Z *= SizeScale.Z;
	}

	// Cylinder
	if (bIsUsingCylinder)
	{
		INT	RadialIndex0	= 0;	//X
		INT	RadialIndex1	= 1;	//Y
		INT	HeightIndex		= 2;	//Z

		switch (PC_HeightAxis)
		{
		case PMLPC_HEIGHTAXIS_X:
			RadialIndex0	= 1;	//Y
			RadialIndex1	= 2;	//Z
			HeightIndex		= 0;	//X
			break;
		case PMLPC_HEIGHTAXIS_Y:
			RadialIndex0	= 0;	//X
			RadialIndex1	= 2;	//Z
			HeightIndex		= 1;	//Y
			break;
		case PMLPC_HEIGHTAXIS_Z:
			break;
		}

		// Determine the start location for the sphere
		FVector StartLoc = PC_StartLocation.GetValue(Owner->EmitterTime, Owner->Component);

		// Determine the unit direction
		FVector UnitDir, UnitDirTemp;
		DetermineUnitDirection(Owner, UnitDirTemp);
		UnitDir[RadialIndex0]	= UnitDirTemp[RadialIndex0];
		UnitDir[RadialIndex1]	= UnitDirTemp[RadialIndex1];
		UnitDir[HeightIndex]	= UnitDirTemp[HeightIndex];

		FVector NormalizedDir = UnitDir;
		NormalizedDir.Normalize();

		FVector2D UnitDir2D(UnitDir[RadialIndex0], UnitDir[RadialIndex1]);
		FVector2D NormalizedDir2D = UnitDir2D.SafeNormal();

		// Determine the position
		FVector Offset(0.0f);
		FLOAT	StartRadius	= PC_StartRadius.GetValue(Owner->EmitterTime, Owner->Component);
		FLOAT	StartHeight	= PC_StartHeight.GetValue(Owner->EmitterTime, Owner->Component) / 2.0f;

		// Always want Z in the [-Height, Height] range
		Offset[HeightIndex] = UnitDir[HeightIndex] * StartHeight;

		NormalizedDir[RadialIndex0] = NormalizedDir2D.X;
		NormalizedDir[RadialIndex1] = NormalizedDir2D.Y;

		if (bSurfaceOnly)
		{
			// Clamp the X,Y to the outer edge...

			if (Abs(Offset[HeightIndex]) == StartHeight)
			{
				// On the caps, it can be anywhere within the 'circle'
				Offset[RadialIndex0] = UnitDir[RadialIndex0] * StartRadius;
				Offset[RadialIndex1] = UnitDir[RadialIndex1] * StartRadius;
			}
			else
			{
				// On the sides, it must be on the 'circle'
				Offset[RadialIndex0] = NormalizedDir[RadialIndex0] * StartRadius;
				Offset[RadialIndex1] = NormalizedDir[RadialIndex1] * StartRadius;
			}
		}
		else
		{
			Offset[RadialIndex0] = UnitDir[RadialIndex0] * StartRadius;
			Offset[RadialIndex1] = UnitDir[RadialIndex1] * StartRadius;
		}

		// Clamp to the radius...
		FVector	Max;
		
		Max[RadialIndex0]	= Abs(NormalizedDir[RadialIndex0]) * StartRadius;
		Max[RadialIndex1]	= Abs(NormalizedDir[RadialIndex1]) * StartRadius;
		Max[HeightIndex]	= StartHeight;

		Offset[RadialIndex0]	= Clamp<FLOAT>(Offset[RadialIndex0], -Max[RadialIndex0], Max[RadialIndex0]);
		Offset[RadialIndex1]	= Clamp<FLOAT>(Offset[RadialIndex1], -Max[RadialIndex1], Max[RadialIndex1]);
		Offset[HeightIndex]		= Clamp<FLOAT>(Offset[HeightIndex],  -Max[HeightIndex],  Max[HeightIndex]);

		// Add in the start location
		Offset[RadialIndex0]	+= StartLoc[RadialIndex0];
		Offset[RadialIndex1]	+= StartLoc[RadialIndex1];
		Offset[HeightIndex]		+= StartLoc[HeightIndex];

		if (bUseLocalSpace == FALSE)
		{
			Offset = Owner->Component->LocalToWorld.TransformNormal(Offset);
		}
		Particle.Location += Offset;

		if (bVelocity)
		{
			FVector Velocity;
			Velocity[RadialIndex0]	= Offset[RadialIndex0]	- StartLoc[RadialIndex0];
			Velocity[RadialIndex1]	= Offset[RadialIndex1]	- StartLoc[RadialIndex1];
			Velocity[HeightIndex]	= Offset[HeightIndex]	- StartLoc[HeightIndex];

			if (bRadialVelocity)
			{
				Velocity[HeightIndex]	= 0.0f;
			}
			Velocity	*= PC_VelocityScale.GetValue(Owner->EmitterTime, Owner->Component);

			Particle.Velocity		+= Velocity;
			Particle.BaseVelocity	+= Velocity;
		}
	}

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);
	Particle.BaseColor	= Particle.Color;

}

void UParticleModuleUberRainImpacts::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;

		// Lifetime

		// Size

		// InitialMeshRotation

		// SizeByLife
		FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
		if(MultiplyX)
		{
			Particle.Size.X *= SizeScale.X;
		}
		if(MultiplyY)
		{
			Particle.Size.Y *= SizeScale.Y;
		}
		if(MultiplyZ)
		{
			Particle.Size.Z *= SizeScale.Z;
		}

		// Cylinder

		// ColorOverLife
		FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);

	END_UPDATE_LOOP;
}

void UParticleModuleUberRainImpacts::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Lifetime

	// Size

	// InitialMeshRotation

	// SizeByLife

	// Cylinder

	// ColorOverLife
}

void UParticleModuleUberRainImpacts::DetermineUnitDirection(FParticleEmitterInstance* Owner, FVector& UnitDir)
{
	FVector Rand;

	// Grab 3 random numbers for the axes
	Rand.X = appSRand();
	Rand.Y = appSRand();
	Rand.Z = appSRand();

	// Set the unit dir
	if (bPositive_X && bNegative_X)
	{
		UnitDir.X = Rand.X * 2 - 1;
	}
	else
	if (bPositive_X)
	{
		UnitDir.X = Rand.X;
	}
	else
	if (bNegative_X)
	{
		UnitDir.X = -Rand.X;
	}
	else
	{
		UnitDir.X = 0.0f;
	}

	if (bPositive_Y && bNegative_Y)
	{
		UnitDir.Y = Rand.Y * 2 - 1;
	}
	else
	if (bPositive_Y)
	{
		UnitDir.Y = Rand.Y;
	}
	else
	if (bNegative_Y)
	{
		UnitDir.Y = -Rand.Y;
	}
	else
	{
		UnitDir.Y = 0.0f;
	}

	if (bPositive_Z && bNegative_Z)
	{
		UnitDir.Z = Rand.Z * 2 - 1;
	}
	else
	if (bPositive_Z)
	{
		UnitDir.Z = Rand.Z;
	}
	else
	if (bNegative_Z)
	{
		UnitDir.Z = -Rand.Z;
	}
	else
	{
		UnitDir.Z = 0.0f;
	}
}

UBOOL UParticleModuleUberRainImpacts::IsCompatible(UParticleEmitter* InputEmitter)
{
	UBOOL bFoundAll	= FALSE;

	if (InputEmitter)
	{
		UParticleLODLevel* LODLevel	= InputEmitter->LODLevels(0);
		check(LODLevel);

		if (LODLevel->TypeDataModule)
		{
			if (LODLevel->TypeDataModule->IsA(UParticleModuleTypeDataMesh::StaticClass()))
			{
				if (LODLevel->Modules.Num() == 6)
				{
					// They MUST be in this order
					// Lifetime
					// Size
					// InitialMeshRotation
					// SizeByLife
					// Cylinder
					// ColorOverLife
					if (LODLevel->Modules(0)->IsA(UParticleModuleLifetime::StaticClass())					&&
						LODLevel->Modules(1)->IsA(UParticleModuleSize::StaticClass())						&&
						LODLevel->Modules(2)->IsA(UParticleModuleMeshRotation::StaticClass())				&&
						LODLevel->Modules(3)->IsA(UParticleModuleSizeMultiplyLife::StaticClass())			&&
						LODLevel->Modules(4)->IsA(UParticleModuleLocationPrimitiveCylinder::StaticClass())	&&
						LODLevel->Modules(5)->IsA(UParticleModuleColorOverLife::StaticClass()))
					{
						bFoundAll	= TRUE;
					}
				}
			}
		}
	}

	return bFoundAll;
}

UBOOL UParticleModuleUberRainImpacts::ConvertToUberModule(UParticleEmitter* InputEmitter)
{
	if (InputEmitter->LODLevels.Num() > 2)
	{
		appMsgf(AMT_OK, TEXT("Can't convert an emitter with specific LOD levels!"));
		return FALSE;
	}

	// Lifetime
	// Size
	// InitialMeshRotation
	// SizeByLife
	// Cylinder
	// ColorOverLife
	UParticleModuleLifetime*					LifetimeModule		= NULL;
	UParticleModuleSize*						SizeModule			= NULL;
	UParticleModuleMeshRotation*				MeshRotationModule	= NULL;
	UParticleModuleSizeMultiplyLife*			SizeByLifeModule	= NULL;
	UParticleModuleLocationPrimitiveCylinder*	CylinderModule		= NULL;
	UParticleModuleColorOverLife*				ColorOverLifeModule	= NULL;
	UObject*									DupObject;

	bIsUsingCylinder = TRUE;

	// Copy the module data from each module to the new one...
	UParticleLODLevel* LODLevel	= InputEmitter->LODLevels(0);
	check(LODLevel);

	UObject*	DestOuter	= this;

	for (INT SourceModuleIndex = 0; SourceModuleIndex < LODLevel->Modules.Num(); SourceModuleIndex++)
	{
		UParticleModule* SourceModule	= LODLevel->Modules(SourceModuleIndex);

		// Lifetime
		if (SourceModule->IsA(UParticleModuleLifetime::StaticClass()))
		{
			// Copy the contents
			if (LifetimeModule == NULL)
			{
				LifetimeModule	= Cast<UParticleModuleLifetime>(SourceModule);
				DupObject		= StaticDuplicateObject(LifetimeModule->Lifetime.Distribution, LifetimeModule->Lifetime.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				Lifetime.Distribution		= Cast<UDistributionFloat>(DupObject);
			}
		}
		else
		// Size
		if (SourceModule->IsA(UParticleModuleSize::StaticClass()))
		{
			if (SizeModule == NULL)
			{
				SizeModule		= Cast<UParticleModuleSize>(SourceModule);
				DupObject		= StaticDuplicateObject(SizeModule->StartSize.Distribution, SizeModule->StartSize.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				StartSize.Distribution		= Cast<UDistributionVector>(DupObject);
			}
		}
		else
		// InitialMeshRotation
		if (SourceModule->IsA(UParticleModuleMeshRotation::StaticClass()))
		{
			if (MeshRotationModule == NULL)
			{
				MeshRotationModule	= Cast<UParticleModuleMeshRotation>(SourceModule);
				DupObject		= StaticDuplicateObject(MeshRotationModule->StartRotation.Distribution, MeshRotationModule->StartRotation.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				StartRotation.Distribution	= Cast<UDistributionVector>(DupObject);
				bInheritParent	= MeshRotationModule->bInheritParent;
			}
			else
			{
			}
		}
		else
		// SizeByLife
		if (SourceModule->IsA(UParticleModuleSizeMultiplyLife::StaticClass()))
		{
			if (SizeByLifeModule == NULL)
			{
				SizeByLifeModule	= Cast<UParticleModuleSizeMultiplyLife>(SourceModule);
				DupObject		= StaticDuplicateObject(SizeByLifeModule->LifeMultiplier.Distribution, SizeByLifeModule->LifeMultiplier.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				LifeMultiplier.Distribution	= Cast<UDistributionVector>(DupObject);
				MultiplyX		= SizeByLifeModule->MultiplyX;
				MultiplyY		= SizeByLifeModule->MultiplyY;
				MultiplyZ		= SizeByLifeModule->MultiplyZ;
			}
			else
			{
			}
		}
		else
		// PrimitiveCylinder
		if (SourceModule->IsA(UParticleModuleLocationPrimitiveCylinder::StaticClass()))
		{
			if (CylinderModule == NULL)
			{
				CylinderModule	= Cast<UParticleModuleLocationPrimitiveCylinder>(SourceModule);

				bIsUsingCylinder	= TRUE;
				bPositive_X			= CylinderModule->Positive_X;
				bPositive_Y			= CylinderModule->Positive_Y;
				bPositive_Z			= CylinderModule->Positive_Z;
				bNegative_X			= CylinderModule->Negative_X;
				bNegative_Y			= CylinderModule->Negative_Y;
				bNegative_Z			= CylinderModule->Negative_Z;
				bSurfaceOnly		= CylinderModule->SurfaceOnly;
				bVelocity			= CylinderModule->Velocity;
				DupObject			= StaticDuplicateObject(CylinderModule->VelocityScale.Distribution, CylinderModule->VelocityScale.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				PC_VelocityScale.Distribution	= Cast<UDistributionFloat>(DupObject);
				DupObject			= StaticDuplicateObject(CylinderModule->StartLocation.Distribution, CylinderModule->StartLocation.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				PC_StartLocation.Distribution	= Cast<UDistributionVector>(DupObject);
				bRadialVelocity		= CylinderModule->RadialVelocity;
				DupObject			= StaticDuplicateObject(CylinderModule->StartRadius.Distribution, CylinderModule->StartRadius.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				PC_StartRadius.Distribution		= Cast<UDistributionFloat>(DupObject);
				DupObject			= StaticDuplicateObject(CylinderModule->StartHeight.Distribution, CylinderModule->StartHeight.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				PC_StartHeight.Distribution		= Cast<UDistributionFloat>(DupObject);
				PC_HeightAxis		= CylinderModule->HeightAxis;
			}
		}
		else
		// ColorOverLife
		if (SourceModule->IsA(UParticleModuleColorOverLife::StaticClass()))
		{
			if (ColorOverLifeModule == NULL)
			{
				ColorOverLifeModule	= Cast<UParticleModuleColorOverLife>(SourceModule);
				DupObject			= StaticDuplicateObject(ColorOverLifeModule->ColorOverLife.Distribution, ColorOverLifeModule->ColorOverLife.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				ColorOverLife.Distribution		= Cast<UDistributionVector>(DupObject);
				DupObject			= StaticDuplicateObject(ColorOverLifeModule->AlphaOverLife.Distribution, ColorOverLifeModule->AlphaOverLife.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				AlphaOverLife.Distribution		= Cast<UDistributionFloat>(DupObject);
			}
		}
		else
		{
			// Just leave the module in its place...
		}
	}

	// Ensure that we found all the required modules
	if ((LifetimeModule			== NULL) || 
		(SizeModule				== NULL) || 
		(MeshRotationModule		== NULL) || 
		(SizeByLifeModule		== NULL) || 
		(CylinderModule			== NULL) || 
		(ColorOverLifeModule	== NULL))
	{
		// Failed the conversion!
		return FALSE;
	}

	// Remove the modules
	LODLevel->Modules.RemoveItem(LifetimeModule);
	LODLevel->Modules.RemoveItem(SizeModule);
	LODLevel->Modules.RemoveItem(MeshRotationModule);
	LODLevel->Modules.RemoveItem(SizeByLifeModule);
	LODLevel->Modules.RemoveItem(CylinderModule);
	LODLevel->Modules.RemoveItem(ColorOverLifeModule);

	// Add the Uber
	LODLevel->Modules.AddItem(this);

	//
	LODLevel->UpdateModuleLists();

	return TRUE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleUberRainSplashA implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleUberRainSplashA);

void UParticleModuleUberRainSplashA::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	UBOOL	bUseLocalSpace	= LODLevel->RequiredModule->bUseLocalSpace;

	SPAWN_INIT;

	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if (Particle.OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;

	// MeshRotation
	FParticleMeshEmitterInstance* MeshInst = CastEmitterInstance<FParticleMeshEmitterInstance>(Owner);
	if (MeshInst)
	{
		FVector Rotation = StartRotation.GetValue(Owner->EmitterTime, Owner->Component);

		if (bInheritParent)
		{
			FRotator	Rotator	= Owner->Component->LocalToWorld.Rotator();
			FVector		ParentAffectedRotation	= Rotator.Euler();
			Rotation.X	+= ParentAffectedRotation.X / 360.0f;
			Rotation.Y	+= ParentAffectedRotation.Y / 360.0f;
			Rotation.Z	+= ParentAffectedRotation.Z / 360.0f;
		}

		FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + MeshInst->MeshRotationOffset);
		PayloadData->Rotation.X	+= Rotation.X * 360.0f;
		PayloadData->Rotation.Y	+= Rotation.Y * 360.0f;
		PayloadData->Rotation.Z	+= Rotation.Z * 360.0f;
	}

	// SizeByLife
	FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
	if(MultiplyX)
	{
		Particle.Size.X *= SizeScale.X;
	}
	if(MultiplyY)
	{
		Particle.Size.Y *= SizeScale.Y;
	}
	if(MultiplyZ)
	{
		Particle.Size.Z *= SizeScale.Z;
	}

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);
	Particle.BaseColor	= Particle.Color;
}

void UParticleModuleUberRainSplashA::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;

		// Lifetime

		// Size

		// MeshRotation

		// SizeByLife
		FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
		if(MultiplyX)
		{
			Particle.Size.X *= SizeScale.X;
		}
		if(MultiplyY)
		{
			Particle.Size.Y *= SizeScale.Y;
		}
		if(MultiplyZ)
		{
			Particle.Size.Z *= SizeScale.Z;
		}

		// ColorOverLife
		FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);

	END_UPDATE_LOOP;
}

UBOOL UParticleModuleUberRainSplashA::IsCompatible(UParticleEmitter* InputEmitter)
{
	UBOOL bFoundAll	= FALSE;

	if (InputEmitter)
	{
		UParticleLODLevel* LODLevel	= InputEmitter->LODLevels(0);
		check(LODLevel);

		if (LODLevel->Modules.Num() == 5)
		{
			// They MUST be in this order
			// Lifetime
			// Size
			// MeshRotation Module Members
			// SizeByLife Module Members
			// ColorOverLife
			if (LODLevel->Modules(0)->IsA(UParticleModuleLifetime::StaticClass())					&&
				LODLevel->Modules(1)->IsA(UParticleModuleSize::StaticClass())						&&
				LODLevel->Modules(2)->IsA(UParticleModuleMeshRotation::StaticClass())				&&
				LODLevel->Modules(3)->IsA(UParticleModuleSizeMultiplyLife::StaticClass())			&&
				LODLevel->Modules(4)->IsA(UParticleModuleColorOverLife::StaticClass()))
			{
				bFoundAll	= TRUE;
			}
		}
	}

	return bFoundAll;
}

UBOOL UParticleModuleUberRainSplashA::ConvertToUberModule(UParticleEmitter* InputEmitter)
{
	if (InputEmitter->LODLevels.Num() > 2)
	{
		appMsgf(AMT_OK, TEXT("Can't convert an emitter with specific LOD levels!"));
		return FALSE;
	}

	// Lifetime
	// Size
	// ColorOverLife
	UParticleModuleLifetime*					LifetimeModule		= NULL;
	UParticleModuleSize*						SizeModule			= NULL;
	UParticleModuleMeshRotation*				MeshRotationModule	= NULL;
	UParticleModuleSizeMultiplyLife*			SizeByLifeModule	= NULL;
	UParticleModuleColorOverLife*				ColorOverLifeModule	= NULL;
	UObject*									DupObject;

	// Copy the module data from each module to the new one...
	UParticleLODLevel* LODLevel	= InputEmitter->LODLevels(0);
	check(LODLevel);

	UObject*	DestOuter	= this;

	for (INT SourceModuleIndex = 0; SourceModuleIndex < LODLevel->Modules.Num(); SourceModuleIndex++)
	{
		UParticleModule* SourceModule	= LODLevel->Modules(SourceModuleIndex);

		// Lifetime
		if (SourceModule->IsA(UParticleModuleLifetime::StaticClass()))
		{
			// Copy the contents
			if (LifetimeModule == NULL)
			{
				LifetimeModule	= Cast<UParticleModuleLifetime>(SourceModule);
				DupObject		= StaticDuplicateObject(LifetimeModule->Lifetime.Distribution, LifetimeModule->Lifetime.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				Lifetime.Distribution		= Cast<UDistributionFloat>(DupObject);
			}
		}
		else
		// Size
		if (SourceModule->IsA(UParticleModuleSize::StaticClass()))
		{
			if (SizeModule == NULL)
			{
				SizeModule		= Cast<UParticleModuleSize>(SourceModule);
				DupObject		= StaticDuplicateObject(SizeModule->StartSize.Distribution, SizeModule->StartSize.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				StartSize.Distribution		= Cast<UDistributionVector>(DupObject);
			}
		}
		else
		// MeshRotation
		if (SourceModule->IsA(UParticleModuleMeshRotation::StaticClass()))
		{
			if (MeshRotationModule == NULL)
			{
				MeshRotationModule	= Cast<UParticleModuleMeshRotation>(SourceModule);
				DupObject			= StaticDuplicateObject(MeshRotationModule->StartRotation.Distribution, MeshRotationModule->StartRotation.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				StartRotation.Distribution		= Cast<UDistributionVector>(DupObject);
				bInheritParent		= MeshRotationModule->bInheritParent;
			}
		}
		else
		// SizeByLife
		if (SourceModule->IsA(UParticleModuleSizeMultiplyLife::StaticClass()))
		{
			if (SizeByLifeModule == NULL)
			{
				SizeByLifeModule	= Cast<UParticleModuleSizeMultiplyLife>(SourceModule);
				DupObject			= StaticDuplicateObject(SizeByLifeModule->LifeMultiplier.Distribution, SizeByLifeModule->LifeMultiplier.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				LifeMultiplier.Distribution		= Cast<UDistributionVector>(DupObject);
				MultiplyX			= SizeByLifeModule->MultiplyX;
				MultiplyY			= SizeByLifeModule->MultiplyY;
				MultiplyZ			= SizeByLifeModule->MultiplyZ;
			}
		}
		else
		// ColorOverLife
		if (SourceModule->IsA(UParticleModuleColorOverLife::StaticClass()))
		{
			if (ColorOverLifeModule == NULL)
			{
				ColorOverLifeModule	= Cast<UParticleModuleColorOverLife>(SourceModule);
				DupObject			= StaticDuplicateObject(ColorOverLifeModule->ColorOverLife.Distribution, ColorOverLifeModule->ColorOverLife.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				ColorOverLife.Distribution		= Cast<UDistributionVector>(DupObject);
				DupObject			= StaticDuplicateObject(ColorOverLifeModule->AlphaOverLife.Distribution, ColorOverLifeModule->AlphaOverLife.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				AlphaOverLife.Distribution		= Cast<UDistributionFloat>(DupObject);
			}
		}
		else
		{
			// Just leave the module in its place...
		}
	}

	// Ensure that we found all the required modules
	if ((LifetimeModule			== NULL) || 
		(SizeModule				== NULL) || 
		(MeshRotationModule		== NULL) || 
		(SizeByLifeModule		== NULL) || 
		(ColorOverLifeModule	== NULL))
	{
		// Failed the conversion!
		return FALSE;
	}

	// Remove the modules
	LODLevel->Modules.RemoveItem(LifetimeModule);
	LODLevel->Modules.RemoveItem(SizeModule);
	LODLevel->Modules.RemoveItem(MeshRotationModule);
	LODLevel->Modules.RemoveItem(SizeByLifeModule);
	LODLevel->Modules.RemoveItem(ColorOverLifeModule);

	// Add the Uber
	LODLevel->Modules.AddItem(this);

	//
	LODLevel->UpdateModuleLists();

	return TRUE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleUberRainSplashB implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleUberRainSplashB);

void UParticleModuleUberRainSplashB::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	UBOOL	bUseLocalSpace	= LODLevel->RequiredModule->bUseLocalSpace;

	SPAWN_INIT;

	// Lifetime
	FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component);
	if (Particle.OneOverMaxLifetime > 0.f)
	{
		// Another module already modified lifetime.
		Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
	}
	else
	{
		// First module to modify lifetime.
		Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
	}
	Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;

	// Size
	FVector Size		 = StartSize.GetValue(Owner->EmitterTime, Owner->Component);
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;

	// ColorOverLife
	FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);
	Particle.BaseColor	= Particle.Color;

	// SizeByLife
	FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
	if(MultiplyX)
	{
		Particle.Size.X *= SizeScale.X;
	}
	if(MultiplyY)
	{
		Particle.Size.Y *= SizeScale.Y;
	}
	if(MultiplyZ)
	{
		Particle.Size.Z *= SizeScale.Z;
	}

	// InitialRotRate
}

void UParticleModuleUberRainSplashB::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;

		// Lifetime

		// Size

		// ColorOverLife
		FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		Particle_SetColorFromVector(ColorVec, fAlpha, Particle.Color);

		// SizeByLife
		FVector SizeScale = LifeMultiplier.GetValue(Particle.RelativeTime, Owner->Component);
		if(MultiplyX)
		{
			Particle.Size.X *= SizeScale.X;
		}
		if(MultiplyY)
		{
			Particle.Size.Y *= SizeScale.Y;
		}
		if(MultiplyZ)
		{
			Particle.Size.Z *= SizeScale.Z;
		}

		// InitialRotRate
	END_UPDATE_LOOP;
}

UBOOL UParticleModuleUberRainSplashB::IsCompatible(UParticleEmitter* InputEmitter)
{
	UBOOL bFoundAll	= FALSE;

	if (InputEmitter)
	{
		UParticleLODLevel* LODLevel	= InputEmitter->LODLevels(0);
		check(LODLevel);

		if (LODLevel->Modules.Num() == 5)
		{
			// They MUST be in this order
			// Lifetime
			// Size
			// ColorOverLife
			// SizeByLife
			// InitialRotRate
			if (LODLevel->Modules(0)->IsA(UParticleModuleLifetime::StaticClass())					&&
				LODLevel->Modules(1)->IsA(UParticleModuleSize::StaticClass())						&&
				LODLevel->Modules(2)->IsA(UParticleModuleColorOverLife::StaticClass())				&&
				LODLevel->Modules(3)->IsA(UParticleModuleSizeMultiplyLife::StaticClass())			&&
				LODLevel->Modules(4)->IsA(UParticleModuleRotationRate::StaticClass())
				)
			{
				bFoundAll	= TRUE;
			}
		}
	}

	return bFoundAll;
}

UBOOL UParticleModuleUberRainSplashB::ConvertToUberModule(UParticleEmitter* InputEmitter)
{
	if (InputEmitter->LODLevels.Num() > 2)
	{
		appMsgf(AMT_OK, TEXT("Can't convert an emitter with specific LOD levels!"));
		return FALSE;
	}

	// Lifetime
	// Size
	// ColorOverLife
	// SizeByLife
	// InitialRotRate
	UParticleModuleLifetime*					LifetimeModule		= NULL;
	UParticleModuleSize*						SizeModule			= NULL;
	UParticleModuleColorOverLife*				ColorOverLifeModule	= NULL;
	UParticleModuleSizeMultiplyLife*			SizeByLifeModule	= NULL;
	UParticleModuleRotationRate*				InitRotRateModule	= NULL;
	UObject*									DupObject;

	// Copy the module data from each module to the new one...
	UParticleLODLevel* LODLevel	= InputEmitter->LODLevels(0);
	check(LODLevel);

	UObject*	DestOuter	= this;

	for (INT SourceModuleIndex = 0; SourceModuleIndex < LODLevel->Modules.Num(); SourceModuleIndex++)
	{
		UParticleModule* SourceModule	= LODLevel->Modules(SourceModuleIndex);

		// Lifetime
		if (SourceModule->IsA(UParticleModuleLifetime::StaticClass()))
		{
			// Copy the contents
			if (LifetimeModule == NULL)
			{
				LifetimeModule	= Cast<UParticleModuleLifetime>(SourceModule);
				DupObject		= StaticDuplicateObject(LifetimeModule->Lifetime.Distribution, LifetimeModule->Lifetime.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				Lifetime.Distribution		= Cast<UDistributionFloat>(DupObject);
			}
		}
		else
		// Size
		if (SourceModule->IsA(UParticleModuleSize::StaticClass()))
		{
			if (SizeModule == NULL)
			{
				SizeModule		= Cast<UParticleModuleSize>(SourceModule);
				DupObject		= StaticDuplicateObject(SizeModule->StartSize.Distribution, SizeModule->StartSize.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				StartSize.Distribution		= Cast<UDistributionVector>(DupObject);
			}
		}
		else
		// ColorOverLife
		if (SourceModule->IsA(UParticleModuleColorOverLife::StaticClass()))
		{
			if (ColorOverLifeModule == NULL)
			{
				ColorOverLifeModule	= Cast<UParticleModuleColorOverLife>(SourceModule);
				DupObject			= StaticDuplicateObject(ColorOverLifeModule->ColorOverLife.Distribution, ColorOverLifeModule->ColorOverLife.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				ColorOverLife.Distribution		= Cast<UDistributionVector>(DupObject);
				DupObject			= StaticDuplicateObject(ColorOverLifeModule->AlphaOverLife.Distribution, ColorOverLifeModule->AlphaOverLife.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				AlphaOverLife.Distribution		= Cast<UDistributionFloat>(DupObject);
			}
		}
		else
		// SizeByLife
		if (SourceModule->IsA(UParticleModuleSizeMultiplyLife::StaticClass()))
		{
			if (SizeByLifeModule == NULL)
			{
				SizeByLifeModule	= Cast<UParticleModuleSizeMultiplyLife>(SourceModule);
				DupObject			= StaticDuplicateObject(SizeByLifeModule->LifeMultiplier.Distribution, SizeByLifeModule->LifeMultiplier.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				LifeMultiplier.Distribution		= Cast<UDistributionVector>(DupObject);
				MultiplyX			= SizeByLifeModule->MultiplyX;
				MultiplyY			= SizeByLifeModule->MultiplyY;
				MultiplyZ			= SizeByLifeModule->MultiplyZ;
			}
		}
		else
		// InitialRotRate
		if (SourceModule->IsA(UParticleModuleRotationRate::StaticClass()))
		{
			if (InitRotRateModule == NULL)
			{
				InitRotRateModule	= Cast<UParticleModuleRotationRate>(SourceModule);
				DupObject			= StaticDuplicateObject(InitRotRateModule->StartRotationRate.Distribution, InitRotRateModule->StartRotationRate.Distribution, DestOuter, TEXT("None"));
				check(DupObject);
				StartRotationRate.Distribution	= Cast<UDistributionFloat>(DupObject);
			}
		}
		else
		{
			// Just leave the module in its place...
		}
	}

	// Ensure that we found all the required modules
	if ((LifetimeModule			== NULL) || 
		(SizeModule				== NULL) || 
		(ColorOverLifeModule	== NULL) || 
		(SizeByLifeModule		== NULL) || 
		(InitRotRateModule		== NULL))
	{
		// Failed the conversion!
		return FALSE;
	}

	// Remove the modules
	LODLevel->Modules.RemoveItem(LifetimeModule);
	LODLevel->Modules.RemoveItem(SizeModule);
	LODLevel->Modules.RemoveItem(ColorOverLifeModule);
	LODLevel->Modules.RemoveItem(SizeByLifeModule);
	LODLevel->Modules.RemoveItem(InitRotRateModule);

	// Add the Uber
	LODLevel->Modules.AddItem(this);

	//
	LODLevel->UpdateModuleLists();

	return TRUE;
}

/*-----------------------------------------------------------------------------
	UParticleModuleColorScaleOverDensity implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleColorScaleOverDensity);

void UParticleModuleColorScaleOverDensity::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	FParticleSpritePhysXEmitterInstance* EmitInst = CastEmitterInstance<FParticleSpritePhysXEmitterInstance>(Owner);
	if (EmitInst != NULL)
	{
		SPAWN_INIT;
		{
			FLOAT Density = *((FLOAT*)(ParticleBase + EmitInst->DensityPayloadOffset));
			FVector ColorVec = ColorScaleOverDensity.GetValue(Density, Owner->Component);
			FLOAT	fAlpha = AlphaScaleOverDensity.GetValue(Density, Owner->Component);

			Particle.Color.R *= ColorVec.X;
			Particle.Color.G *= ColorVec.Y;
			Particle.Color.B *= ColorVec.Z;
			Particle.Color.A *= fAlpha;
		}
	}
#endif
}

void UParticleModuleColorScaleOverDensity::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
	FParticleSpritePhysXEmitterInstance* EmitInst = CastEmitterInstance<FParticleSpritePhysXEmitterInstance>(Owner);
	if (EmitInst != NULL)
	{
		const FRawDistribution* FastColorScaleOverDensity = ColorScaleOverDensity.GetFastRawDistribution();
		const FRawDistribution* FastAlphaScaleOverDensity = AlphaScaleOverDensity.GetFastRawDistribution();
		FVector ColorVec;
		FLOAT	fAlpha;
		FLOAT Density;
		if( FastColorScaleOverDensity && FastAlphaScaleOverDensity )
		{
			BEGIN_UPDATE_LOOP;
			{
				Density = *((FLOAT*)(ParticleBase + EmitInst->DensityPayloadOffset));
				FastColorScaleOverDensity->GetValue3None(Density, &ColorVec.X);
				FastAlphaScaleOverDensity->GetValue1None(Density, &fAlpha);
				Particle.Color.R *= ColorVec.X;
				Particle.Color.G *= ColorVec.Y;
				Particle.Color.B *= ColorVec.Z;
				Particle.Color.A *= fAlpha;
			}
			END_UPDATE_LOOP;
		}
		else
		{
			BEGIN_UPDATE_LOOP;
			{
				Density = *((FLOAT*)(ParticleBase + EmitInst->DensityPayloadOffset));
				ColorVec = ColorScaleOverDensity.GetValue(Density, Owner->Component);
				fAlpha = AlphaScaleOverDensity.GetValue(Density, Owner->Component);
				Particle.Color.R *= ColorVec.X;
				Particle.Color.G *= ColorVec.Y;
				Particle.Color.B *= ColorVec.Z;
				Particle.Color.A *= fAlpha;
			}
			END_UPDATE_LOOP;
		}
	}
#endif
}
void UParticleModuleColorScaleOverDensity::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	ColorScaleOverDensity.Distribution = Cast<UDistributionVectorConstantCurve>(StaticConstructObject(UDistributionVectorConstantCurve::StaticClass(), this));
	UDistributionVectorConstantCurve* ColorScaleOverDensityDist = Cast<UDistributionVectorConstantCurve>(ColorScaleOverDensity.Distribution);
	if (ColorScaleOverDensityDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = ColorScaleOverDensityDist->CreateNewKey(Key * 1.0f);
			for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
			{
				ColorScaleOverDensityDist->SetKeyOut(SubIndex, KeyIndex, 1.0f);
			}
		}
		ColorScaleOverDensityDist->bIsDirty = TRUE;
	}

	AlphaScaleOverDensity.Distribution = Cast<UDistributionFloatConstantCurve>(StaticConstructObject(UDistributionFloatConstantCurve::StaticClass(), this));
	UDistributionFloatConstantCurve* AlphaScaleOverDensityDist = Cast<UDistributionFloatConstantCurve>(AlphaScaleOverDensity.Distribution);
	if (AlphaScaleOverDensityDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = AlphaScaleOverDensityDist->CreateNewKey(Key * 1.0f);
			AlphaScaleOverDensityDist->SetKeyOut(0, KeyIndex, 1.0f);
		}
		AlphaScaleOverDensityDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleKillBox implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleKillBox);

	//## BEGIN PROPS ParticleModuleKillBox
//	struct FRawDistributionVector LowerLeftCorner;
//	struct FRawDistributionVector UpperRightCorner;
//	BITFIELD bAbsolute:1;
	//## END PROPS ParticleModuleKillBox

void UParticleModuleKillBox::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);

	FVector CheckLL = LowerLeftCorner.GetValue(Owner->EmitterTime, Owner->Component);
	FVector CheckUR = UpperRightCorner.GetValue(Owner->EmitterTime, Owner->Component);
	if (bAbsolute == FALSE)
	{
		CheckLL += Owner->Component->LocalToWorld.GetOrigin();
		CheckUR += Owner->Component->LocalToWorld.GetOrigin();
	}
	FBox CheckBox = FBox(CheckLL, CheckUR);

	BEGIN_UPDATE_LOOP;
	{
		FVector Position = Particle.Location;

		if (LODLevel->RequiredModule->bUseLocalSpace)
		{
			Position = Owner->Component->LocalToWorld.TransformNormal(Position);
		}
		else if ((bAxisAlignedAndFixedSize == FALSE) && (bAbsolute == FALSE))
		{
			Position = Owner->Component->LocalToWorld.Inverse().TransformFVector(Position) + Owner->Component->LocalToWorld.GetOrigin();
		}

		// Determine if the particle is inside the box
		UBOOL bIsInside = CheckBox.IsInside(Position);

		// If we are killing on the inside, and it is inside
		//	OR
		// If we are killing on the outside, and it is outside
		// kill the particle
		if (bKillInside == bIsInside)
		{
			// Kill the particle...
			Owner->KillParticle(i);
		}
	}
	END_UPDATE_LOOP;
}

void UParticleModuleKillBox::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	FVector CheckLL = LowerLeftCorner.GetValue(Owner->EmitterTime, Owner->Component);
	FVector CheckUR = UpperRightCorner.GetValue(Owner->EmitterTime, Owner->Component);
	
	TArray<FVector> KillboxVerts;
	if (bAxisAlignedAndFixedSize == FALSE)
	{
		KillboxVerts.Add(8);
		KillboxVerts(0).Set(CheckLL.X, CheckLL.Y, CheckLL.Z);
		KillboxVerts(1).Set(CheckLL.X, CheckUR.Y, CheckLL.Z);
		KillboxVerts(2).Set(CheckUR.X, CheckUR.Y, CheckLL.Z);
		KillboxVerts(3).Set(CheckUR.X, CheckLL.Y, CheckLL.Z);
		KillboxVerts(4).Set(CheckLL.X, CheckLL.Y, CheckUR.Z);
		KillboxVerts(5).Set(CheckLL.X, CheckUR.Y, CheckUR.Z);
		KillboxVerts(6).Set(CheckUR.X, CheckUR.Y, CheckUR.Z);
		KillboxVerts(7).Set(CheckUR.X, CheckLL.Y, CheckUR.Z);
	}
	
	if ((bAbsolute == FALSE) && (Owner != NULL) && (Owner->Component != NULL))
	{
		if (bAxisAlignedAndFixedSize == FALSE)
		{
			for (INT i = 0; i < 8; ++i)
			{
				KillboxVerts(i) = Owner->Component->LocalToWorld.TransformFVector(KillboxVerts(i));
			}
		}
		else
		{
			CheckLL += Owner->Component->LocalToWorld.GetOrigin();
			CheckUR += Owner->Component->LocalToWorld.GetOrigin();
		}
	}

	if (bAxisAlignedAndFixedSize == FALSE)
	{
		PDI->DrawLine(KillboxVerts(0), KillboxVerts(1), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(1), KillboxVerts(2), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(2), KillboxVerts(3), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(3), KillboxVerts(0), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(4), KillboxVerts(5), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(5), KillboxVerts(6), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(6), KillboxVerts(7), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(7), KillboxVerts(4), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(0), KillboxVerts(4), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(1), KillboxVerts(5), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(2), KillboxVerts(6), ModuleEditorColor, SDPG_World);
		PDI->DrawLine(KillboxVerts(3), KillboxVerts(7), ModuleEditorColor, SDPG_World);
	}
	else
	{
		FBox CheckBox = FBox(CheckLL, CheckUR);

		DrawWireBox(PDI, CheckBox, ModuleEditorColor, SDPG_World);
	}
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleKillHeight implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleKillHeight);

	//## BEGIN PROPS ParticleModuleKillHeight
//	struct FRawDistributionFloat Height;
//	BITFIELD bAbsolute:1;
	//## END PROPS ParticleModuleKillHeight
void UParticleModuleKillHeight::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);

	FLOAT CheckHeight = Height.GetValue(Owner->EmitterTime, Owner->Component);
	if (bApplyPSysScale == TRUE)
	{
		FVector OwnerScale = Owner->Component->LocalToWorld.GetScaleVector();
		CheckHeight *= OwnerScale.Z;
	}

	if (bAbsolute == FALSE)
	{
		CheckHeight += Owner->Component->LocalToWorld.GetOrigin().Z;
	}

	BEGIN_UPDATE_LOOP;
	{
		FVector Position = Particle.Location;

		if (LODLevel->RequiredModule->bUseLocalSpace)
		{
			Position = Owner->Component->LocalToWorld.TransformNormal(Position);
		}

		if ((bFloor == TRUE) && (Position.Z < CheckHeight))
		{
			// Kill the particle...
			Owner->KillParticle(i);
		}
		else if ((bFloor == FALSE) && (Position.Z > CheckHeight))
		{
			// Kill the particle...
			Owner->KillParticle(i);
		}
	}
	END_UPDATE_LOOP;
}

void UParticleModuleKillHeight::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	FVector OwnerPosition = FVector(0.0f);
	if ((Owner != NULL) && (Owner->Component != NULL))
	{
		OwnerPosition = Owner->Component->LocalToWorld.GetOrigin();
	}
	FLOAT CheckHeight = Height.GetValue(Owner->EmitterTime, Owner->Component);
	FLOAT ScaleValue = 1.0f;
	if (bApplyPSysScale == TRUE)
	{
		FVector OwnerScale = Owner->Component->LocalToWorld.GetScaleVector();
		ScaleValue = OwnerScale.Z;
	}
	CheckHeight *= ScaleValue;
	if (bAbsolute == FALSE)
	{
		CheckHeight += OwnerPosition.Z;
	}

	FLOAT Offset = 100.0f * ScaleValue;
	FVector Pt1 = FVector(OwnerPosition.X - Offset, OwnerPosition.Y - Offset, CheckHeight);
	FVector Pt2 = FVector(OwnerPosition.X + Offset, OwnerPosition.Y - Offset, CheckHeight);
	FVector Pt3 = FVector(OwnerPosition.X - Offset, OwnerPosition.Y + Offset, CheckHeight);
	FVector Pt4 = FVector(OwnerPosition.X + Offset, OwnerPosition.Y + Offset, CheckHeight);

	PDI->DrawLine(Pt1, Pt2, ModuleEditorColor, SDPG_World);
	PDI->DrawLine(Pt1, Pt3, ModuleEditorColor, SDPG_World);
	PDI->DrawLine(Pt2, Pt4, ModuleEditorColor, SDPG_World);
	PDI->DrawLine(Pt3, Pt4, ModuleEditorColor, SDPG_World);
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleLifetime implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLifetime);

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleLifetime::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	{
		FLOAT MaxLifetime = Lifetime.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
		if(Particle.OneOverMaxLifetime > 0.f)
		{
			// Another module already modified lifetime.
			Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
		}
		else
		{
			// First module to modify lifetime.
			Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
		}
		Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;
	}
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleLifetime::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionFloatUniform* LifetimeDist = Cast<UDistributionFloatUniform>(Lifetime.Distribution);
	if (LifetimeDist)
	{
		LifetimeDist->Min = 1.0f;
		LifetimeDist->Max = 1.0f;
		LifetimeDist->bIsDirty = TRUE;
	}
}

FLOAT UParticleModuleLifetime::GetMaxLifetime()
{
	// Check the distribution for the max value
	FLOAT Min, Max;
	Lifetime.GetOutRange(Min, Max);
	return Max;
}

/**
 *	Call to retrieve the lifetime value at the given time.
 *
 *	@param	Owner		The emitter instance that owns this module
 *	@param	InTime		The time input for retrieving the lifetime value
 *	@param	Data		The data associated with the distribution
 *
 *	@return	FLOAT		The Lifetime value
 */
FLOAT UParticleModuleLifetime::GetLifetimeValue(FParticleEmitterInstance* Owner, FLOAT InTime, UObject* Data)
{
	return Lifetime.GetValue(InTime, Data);
}

/*-----------------------------------------------------------------------------
	UParticleModuleLifetime_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleLifetime_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleLifetime_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
UINT UParticleModuleLifetime_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleLifetime_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleLifetime_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/**
 *	Call to retrieve the lifetime value at the given time.
 *
 *	@param	Owner		The emitter instance that owns this module
 *	@param	InTime		The time input for retrieving the lifetime value
 *	@param	Data		The data associated with the distribution
 *
 *	@return	FLOAT		The Lifetime value
 */
FLOAT UParticleModuleLifetime_Seeded::GetLifetimeValue(FParticleEmitterInstance* Owner, FLOAT InTime, UObject* Data )
{
	FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
	if (Payload != NULL)
	{		
		return Lifetime.GetValue(InTime, Data, &(Payload->RandomStream));
	}
	return UParticleModuleLifetime::GetLifetimeValue(Owner, InTime, Data);
}

/*-----------------------------------------------------------------------------
	UParticleModuleAttractorLine implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleAttractorLine);

void UParticleModuleAttractorLine::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	FVector Line = EndPoint1 - EndPoint0;
	FVector LineNorm = Line;
	LineNorm.Normalize();

	BEGIN_UPDATE_LOOP;
		// Determine the position of the particle projected on the line
		FVector AdjustedLocation = Particle.Location - Owner->Component->LocalToWorld.GetOrigin();
		FVector EP02Particle = AdjustedLocation - EndPoint0;

		FVector ProjectedParticle = Line * (Line | EP02Particle) / (Line | Line);

		// Determine the 'ratio' of the line that has been traveled by the particle
		FLOAT VRatioX = 0.0f;
		FLOAT VRatioY = 0.0f;
		FLOAT VRatioZ = 0.0f;

		if (Line.X)
			VRatioX = (ProjectedParticle.X - EndPoint0.X) / Line.X;
		if (Line.Y)
			VRatioY = (ProjectedParticle.Y - EndPoint0.Y) / Line.Y;
		if (Line.Z)
			VRatioZ = (ProjectedParticle.Z - EndPoint0.Z) / Line.Z;

		bool bProcess = false;
		FLOAT fRatio = 0.0f;

		if (VRatioX || VRatioY || VRatioZ)
		{
			// If there are multiple ratios, they should be the same...
			if (VRatioX)
				fRatio = VRatioX;
			else
			if (VRatioY)
				fRatio = VRatioY;
			else
			if (VRatioZ)
				fRatio = VRatioZ;
		}

		if ((fRatio >= 0.0f) && (fRatio <= 1.0f))
			bProcess = true;

		if (bProcess)
		{
			// Look up the Range and Strength at that position on the line
			FLOAT AttractorRange = Range.GetValue(fRatio, Owner->Component);
	        
			FVector LineToPoint = AdjustedLocation - ProjectedParticle;
    		FLOAT Distance = LineToPoint.Size();

			if ((AttractorRange > 0) && (Distance <= AttractorRange))
			{
				// Adjust the strength based on the range ratio
				FLOAT AttractorStrength = Strength.GetValue((AttractorRange - Distance) / AttractorRange, Owner->Component);
				FVector Direction = LineToPoint^Line;
    			// Adjust the VELOCITY of the particle based on the attractor... 
        		Particle.Velocity += Direction * AttractorStrength * DeltaTime;
			}
		}
	END_UPDATE_LOOP;
}

void UParticleModuleAttractorLine::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	PDI->DrawLine(EndPoint0, EndPoint1, ModuleEditorColor, SDPG_World);

	UParticleLODLevel* LODLevel = Owner->SpriteTemplate->GetLODLevel(0);
	check(LODLevel);
	FLOAT CurrRatio = Owner->EmitterTime / LODLevel->RequiredModule->EmitterDuration;
	FLOAT LineRange = Range.GetValue(CurrRatio, Owner->Component);

	// Determine the position of the range at this time.
	FVector LinePos = EndPoint0 + CurrRatio * (EndPoint1 - EndPoint0);

	// Draw a wire star at the position of the range.
	DrawWireStar(PDI,LinePos, 10.0f, ModuleEditorColor, SDPG_World);
	// Draw bounding circle for the current range.
	// This should be oriented such that it appears correctly... ie, as 'open' to the camera as possible
	DrawCircle(PDI,LinePos, FVector(1,0,0), FVector(0,1,0), ModuleEditorColor, LineRange, 32, SDPG_World);
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleModuleAttractorParticle implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleAttractorParticle);
/**
 *	UParticleModuleAttractorParticle::Spawn
 */
void UParticleModuleAttractorParticle::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	// We need to look up the emitter instance...
	// This may not need to be done every Spawn, but in the short term, it will to be safe.
	// (In the editor, the source emitter may be deleted, etc...)
	FParticleEmitterInstance* AttractorEmitterInst = NULL;
	if (EmitterName != NAME_None)
	{
		for (INT ii = 0; ii < Owner->Component->EmitterInstances.Num(); ii++)
		{
			FParticleEmitterInstance* pkEmitInst = Owner->Component->EmitterInstances(ii);
			if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == EmitterName))
			{
				AttractorEmitterInst = pkEmitInst;
				break;
			}
		}
	}

	if (AttractorEmitterInst == NULL)
	{
		// No source emitter, so we don't spawn??
		return;
	}

	SPAWN_INIT
	{
		PARTICLE_ELEMENT(FAttractorParticlePayload,	Data);
		
		FBaseParticle* Source = AttractorEmitterInst->GetParticle(LastSelIndex);
		if (Source == NULL)
		{
			switch (SelectionMethod)
			{
			case EAPSM_Random:
				LastSelIndex		= appTrunc(appSRand() * AttractorEmitterInst->ActiveParticles);
				Data.SourceIndex	= LastSelIndex;
				break;
			case EAPSM_Sequential:
				{
					for (INT ui = 0; ui < AttractorEmitterInst->ActiveParticles; ui++)
					{
						Source = AttractorEmitterInst->GetParticle(ui);
						if (Source)
						{
							LastSelIndex		= ui;
							Data.SourceIndex	= LastSelIndex;
							break;
						}
					}
				}
				break;
			}
			Data.SourcePointer	= (UINT)(PTRINT)Source;
			if (Source)
			{
				Data.SourceVelocity	= Source->Velocity;
			}
		}
		else
		{
			Data.SourceIndex = LastSelIndex++;
		}
	}
}

/**
 *	UParticleModuleAttractorParticle::Update
 */
void UParticleModuleAttractorParticle::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	// We need to look up the emitter instance...
	// This may not need to be done every Spawn, but in the short term, it will to be safe.
	// (In the editor, the source emitter may be deleted, etc...)
	FParticleEmitterInstance* AttractorEmitterInst = NULL;
	if (EmitterName != NAME_None)
	{
		for (INT ii = 0; ii < Owner->Component->EmitterInstances.Num(); ii++)
		{
			FParticleEmitterInstance* pkEmitInst = Owner->Component->EmitterInstances(ii);
			if (pkEmitInst && (pkEmitInst->SpriteTemplate->EmitterName == EmitterName))
			{
				AttractorEmitterInst = pkEmitInst;
				break;
			}
		}
	}

	if (AttractorEmitterInst == NULL)
	{
		// No source emitter, so we don't update??
		return;
	}

	UParticleLODLevel* LODLevel		= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	UParticleLODLevel* SrcLODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(AttractorEmitterInst);
	check(SrcLODLevel);

	UBOOL bUseLocalSpace = LODLevel->RequiredModule->bUseLocalSpace;
	UBOOL bSrcUseLocalSpace = SrcLODLevel->RequiredModule->bUseLocalSpace;

	BEGIN_UPDATE_LOOP;
	{
		// Find the source particle
		PARTICLE_ELEMENT(FAttractorParticlePayload,	Data);

		if (Data.SourceIndex == 0xffffffff)
		{
#if 1
			if (bInheritSourceVel)
			{
				Particle.Velocity	+= Data.SourceVelocity;
			}
#endif
			CONTINUE_UPDATE_LOOP;
		}

		FBaseParticle* Source = AttractorEmitterInst->GetParticle(Data.SourceIndex);
		if (!Source)
		{
			CONTINUE_UPDATE_LOOP;
		}

		if ((Data.SourcePointer != 0) && 
			(Source != (FBaseParticle*)(PTRINT)(Data.SourcePointer)) && 
			(bRenewSource == FALSE))
		{
			Data.SourceIndex	= 0xffffffff;
			Data.SourcePointer	= 0;
#if 0
			if (bInheritSourceVel)
			{
				Particle.Velocity		+= Data.SourceVelocity;
				Particle.BaseVelocity	+= Data.SourceVelocity;
			}
#endif
			CONTINUE_UPDATE_LOOP;
		}

		FLOAT	AttractorRange		= Range.GetValue(Source->RelativeTime, Owner->Component);
		FVector SrcLocation			= Source->Location;
		FVector	ParticleLocation	= Particle.Location;
		if (bUseLocalSpace != bSrcUseLocalSpace)
		{
			if (bSrcUseLocalSpace)
			{
				SrcLocation = Owner->Component->LocalToWorld.TransformNormal(SrcLocation);
			}
			if (bUseLocalSpace)
			{
				ParticleLocation = Owner->Component->LocalToWorld.TransformNormal(Particle.Location);
			}
		}

		FVector Dir			= SrcLocation - ParticleLocation;
		FLOAT	Distance	= Dir.Size();
		if (Distance <= AttractorRange)
		{
			// Determine the strength
			FLOAT AttractorStrength = 0.0f;

			if (bStrengthByDistance)
			{
				// on actual distance
				AttractorStrength = Strength.GetValue((AttractorRange - Distance) / AttractorRange);
			}
			else
			{
				// on emitter time
				AttractorStrength = Strength.GetValue(Source->RelativeTime, Owner->Component);
			}

			// Adjust the VELOCITY of the particle based on the attractor... 
			Dir.Normalize();
    		Particle.Velocity	+= Dir * AttractorStrength * DeltaTime;
			Data.SourceVelocity	 = Source->Velocity;
			if (bAffectBaseVelocity)
			{
				Particle.BaseVelocity	+= Dir * AttractorStrength * DeltaTime;
			}
		}
	}
	END_UPDATE_LOOP;
}

/**
 *	Retrieve the require number of bytes for the particle payload
 *
 *	@param	Owner		The emitter instance that is inquiring
 *
 *	@return	UINT		The size of the particle payload required
 */
UINT UParticleModuleAttractorParticle::RequiredBytes(FParticleEmitterInstance* Owner)
{
	return sizeof(FAttractorParticlePayload);
}

/*-----------------------------------------------------------------------------
	UParticleModuleAttractorPoint implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleAttractorPoint);

void UParticleModuleAttractorPoint::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	check(Owner);
	UParticleSystemComponent* Component = Owner->Component;

	// Grab the position of the attractor in Emitter time???
	FVector AttractorPosition = Position.GetValue(Owner->EmitterTime, Component);
	FLOAT AttractorRange = Range.GetValue(Owner->EmitterTime, Component);

	FVector Scale = FVector(1.0f, 1.0f, 1.0f);

	UParticleLODLevel* LODLevel	= Owner->SpriteTemplate->GetCurrentLODLevel(Owner);
	check(LODLevel);
	check(LODLevel->RequiredModule);
	if ( (LODLevel->RequiredModule->bUseLocalSpace == FALSE) && ( bUseWorldSpacePosition == FALSE ) )
	{
		// Transform the attractor into world space
		AttractorPosition = Component->LocalToWorld.TransformFVector(AttractorPosition);

		Scale *= Component->Scale * Component->Scale3D;
		AActor* Actor = Component->GetOwner();
		if (Actor && !Component->AbsoluteScale)
		{
			Scale *= Actor->DrawScale * Actor->DrawScale3D;
		}
	}
	FLOAT ScaleSize = Scale.Size();

	AttractorRange *= ScaleSize;

	BEGIN_UPDATE_LOOP;
		// If the particle is within range...
		FVector Dir = AttractorPosition - Particle.Location;
		FLOAT Distance = Dir.Size();
		if (Distance <= AttractorRange)
		{
			// Determine the strength
			FLOAT AttractorStrength = 0.0f;

			if (StrengthByDistance)
			{
				// on actual distance
				if(AttractorRange == 0.0f)
				{
					AttractorStrength = 0.0f;
				}
				else
				{
					AttractorStrength = Strength.GetValue((AttractorRange - Distance) / AttractorRange);
				}
			}
			else
			{
				// on emitter time
				AttractorStrength = Strength.GetValue(Owner->EmitterTime, Component);
			}
			if ( (LODLevel->RequiredModule->bUseLocalSpace == FALSE) && ( bUseWorldSpacePosition == FALSE ) )
			{
				AttractorStrength *= ScaleSize;
			}

			// Adjust the VELOCITY of the particle based on the attractor... 
			Dir.Normalize();
    		Particle.Velocity	+= Dir * AttractorStrength * DeltaTime;
			if (bAffectBaseVelocity)
			{
				Particle.BaseVelocity	+= Dir * AttractorStrength * DeltaTime;
			}
		}
	END_UPDATE_LOOP;
}

void UParticleModuleAttractorPoint::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR
	FVector PointPos = Position.GetValue(Owner->EmitterTime, Owner->Component);
//    FLOAT PointStr = Strength.GetValue(Owner->EmitterTime, Owner->Component);
	FLOAT PointRange = Range.GetValue(Owner->EmitterTime, Owner->Component);

	// Draw a wire star at the position.
	DrawWireStar(PDI,PointPos, 10.0f, ModuleEditorColor, SDPG_World);
	
	// Draw bounding circles for the range.
	DrawCircle(PDI,PointPos, FVector(1,0,0), FVector(0,1,0), ModuleEditorColor, PointRange, 32, SDPG_World);
	DrawCircle(PDI,PointPos, FVector(1,0,0), FVector(0,0,1), ModuleEditorColor, PointRange, 32, SDPG_World);
	DrawCircle(PDI,PointPos, FVector(0,1,0), FVector(0,0,1), ModuleEditorColor, PointRange, 32, SDPG_World);

	// Draw lines showing the path of travel...
	INT	NumKeys = Position.Distribution->GetNumKeys();
	INT	NumSubCurves = Position.Distribution->GetNumSubCurves();

	FVector InitialPosition;
	FVector SamplePosition[2];

	for (INT i = 0; i < NumKeys; i++)
	{
		FLOAT X = Position.Distribution->GetKeyOut(0, i);
		FLOAT Y = Position.Distribution->GetKeyOut(1, i);
		FLOAT Z = Position.Distribution->GetKeyOut(2, i);

		if (i == 0)
		{
			InitialPosition.X = X;
			InitialPosition.Y = Y;
			InitialPosition.Z = Z;
			SamplePosition[1].X = X;
			SamplePosition[1].Y = Y;
			SamplePosition[1].Z = Z;
		}
		else
		{
			SamplePosition[0].X = SamplePosition[1].X;
			SamplePosition[0].Y = SamplePosition[1].Y;
			SamplePosition[0].Z = SamplePosition[1].Z;
			SamplePosition[1].X = X;
			SamplePosition[1].Y = Y;
			SamplePosition[1].Z = Z;

			// Draw a line...
			PDI->DrawLine(SamplePosition[0], SamplePosition[1], ModuleEditorColor, SDPG_World);
		}
	}
#endif	//#if WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	Parameter-based distributions
-----------------------------------------------------------------------------*/

UBOOL UDistributionFloatParticleParameter::GetParamValue(UObject* Data, FName ParamName, FLOAT& OutFloat)
{
	UBOOL bFoundParam = false;

	UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Data);
	if(ParticleComp)
	{
		bFoundParam = ParticleComp->GetFloatParameter(ParameterName, OutFloat);
	}

	return bFoundParam;
}

UBOOL UDistributionVectorParticleParameter::GetParamValue(UObject* Data, FName ParamName, FVector& OutVector)
{
	UBOOL bFoundParam = false;

	UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Data);
	if(ParticleComp)
	{
		bFoundParam = ParticleComp->GetVectorParameter(ParameterName, OutVector);

		// If we failed to get a Vector parameter with the given name, see if we can get a Color parameter or Float parameter
		if(!bFoundParam)
		{
			FColor OutColor;
			bFoundParam = ParticleComp->GetColorParameter(ParameterName, OutColor);
			if(bFoundParam)
			{
				OutVector = FVector(OutColor.R/255.0f, OutColor.G/255.0f, OutColor.B/255.0f);
			}
			else
			{
				FLOAT OutFloat;
				bFoundParam = ParticleComp->GetFloatParameter(ParameterName, OutFloat);
				if(bFoundParam)
				{
					OutVector = FVector(OutFloat);
				}
			}
		}
	}

	return bFoundParam;
}

/**
*	Retrieve the skeletal mesh component source to use for the current emitter instance.
*
*	@param	Owner						The particle emitter instance that is being setup
*	@param	ActorParameterName			The name of the parameter corresponding to the SkeletalMeshComponent
*
*	@return	USkeletalMeshComponent*		The skeletal mesh component to use as the source
*/
USkeletalMeshComponent* FParticleModuleUtils::GetSkeletalMeshComponentSource(FParticleEmitterInstance* Owner, FName ActorParameterName)
{
	if (Owner == NULL)
	{
		return NULL;
	}

	UParticleSystemComponent* PSysComp = Owner->Component;
	if (PSysComp == NULL)
	{
		return NULL;
	}

	AActor* Actor;
	if (PSysComp->GetActorParameter(ActorParameterName, Actor) == TRUE)
	{
		ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Actor);
		if (SkelMeshActor != NULL)
		{
			return SkelMeshActor->SkeletalMeshComponent;
		}
		else
		{
			APawn* PawnActor = Cast<APawn>(Actor);
			if (PawnActor != NULL)
			{
				return PawnActor->Mesh;
			}
			//@todo. Warn about this...
		}
	}
	return NULL;
}

/**
*	Retrieve the static mesh component source to use for the current emitter instance.
*
*	@param	Owner						The particle emitter instance that is being setup
*	@param	ActorParameterName			The name of the parameter corresponding to the SkeletalMeshComponent
*
*	@return	UStaticMeshComponent*		The skeletal mesh component to use as the source
*/
UStaticMeshComponent* FParticleModuleUtils::GetStaticMeshComponentSource(FParticleEmitterInstance* Owner, FName ActorParameterName)
{
	if (Owner == NULL)
	{
		return NULL;
	}

	UParticleSystemComponent* PSysComp = Owner->Component;
	if (PSysComp == NULL)
	{
		return NULL;
	}

	AActor* Actor;
	if (PSysComp->GetActorParameter(ActorParameterName, Actor) == TRUE)
	{
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);
		if (StaticMeshActor != NULL)
		{
			return StaticMeshActor->StaticMeshComponent;
		}
	}
	return NULL;
}
