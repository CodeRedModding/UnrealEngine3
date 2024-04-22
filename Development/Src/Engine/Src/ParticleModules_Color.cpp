/*=============================================================================
	ParticleModules_Color.cpp: 
	Color-related particle module implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UParticleModuleColorBase);

/*-----------------------------------------------------------------------------
	UParticleModuleColor implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleColor);

void UParticleModuleColor::PostLoad()
{
	Super::PostLoad();
}

void UParticleModuleColor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetFName() == FName(TEXT("bClampAlpha")))
		{
			UObject* OuterObj = GetOuter();
			check(OuterObj);
			UParticleLODLevel* LODLevel = Cast<UParticleLODLevel>(OuterObj);
			if (LODLevel)
			{
				// The outer is incorrect - warn the user and handle it
				warnf(NAME_Warning, TEXT("UParticleModuleColor has an incorrect outer... run FixupEmitters on package %s"),
					*(OuterObj->GetOutermost()->GetPathName()));
				OuterObj = LODLevel->GetOuter();
				UParticleEmitter* Emitter = Cast<UParticleEmitter>(OuterObj);
				check(Emitter);
				OuterObj = Emitter->GetOuter();
			}
			UParticleSystem* PartSys = CastChecked<UParticleSystem>(OuterObj);
			PartSys->UpdateColorModuleClampAlpha(this);
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleColor::AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup)
{
#if WITH_EDITORONLY_DATA
	// Iterate over object and find any InterpCurveFloats or UDistributionFloats
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		// attempt to get a distribution from a random struct property
		UObject* Distribution = FRawDistribution::TryGetDistributionObjectFromRawDistributionProperty(*It, (BYTE*)this);
		if (Distribution)
		{
			if(Distribution->IsA(UDistributionFloat::StaticClass()))
			{
				if (bClampAlpha == TRUE)
				{
					EdSetup->AddCurveToCurrentTab(Distribution, It->GetName(), ModuleEditorColor, TRUE, TRUE, TRUE, 0.0f, 1.0f);
				}
				else
				{
					EdSetup->AddCurveToCurrentTab(Distribution, It->GetName(), ModuleEditorColor, TRUE, TRUE);
				}
			}
			else
			{
				EdSetup->AddCurveToCurrentTab(Distribution, It->GetName(), ModuleEditorColor, TRUE, TRUE);
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
void UParticleModuleColor::SpawnEx(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime, class FRandomStream* InRandomStream)
{
	SPAWN_INIT;
	{
		FVector ColorVec	= StartColor.GetValue(Owner->EmitterTime, Owner->Component, 0, InRandomStream);
		FLOAT	Alpha		= StartAlpha.GetValue(Owner->EmitterTime, Owner->Component, InRandomStream);
		Particle_SetColorFromVector(ColorVec, Alpha, Particle.Color);
		Particle.BaseColor	= Particle.Color;
	}
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleColor::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UDistributionVectorConstant* StartColorDist = Cast<UDistributionVectorConstant>(StartColor.Distribution);
	if (StartColorDist)
	{
		StartColorDist->Constant = FVector(1.0f,1.0f,1.0f);
		StartColorDist->bIsDirty = TRUE;
	}

}

/*-----------------------------------------------------------------------------
	UParticleModuleColor_Seeded implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleColor_Seeded);

/**
 *	Called on a particle when it is spawned.
 *
 *	@param	Owner			The emitter instance that spawned the particle
 *	@param	Offset			The payload data offset for this module
 *	@param	SpawnTime		The spawn time of the particle
 */
void UParticleModuleColor_Seeded::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
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
UINT UParticleModuleColor_Seeded::RequiredBytesPerInstance(FParticleEmitterInstance* Owner)
{
	return RandomSeedInfo.GetInstancePayloadSize();
}

/**
 *	Allows the module to prep its 'per-instance' data block.
 *	
 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
 *	@param	InstData	Pointer to the data block for this module.
 */
UINT UParticleModuleColor_Seeded::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	return PrepRandomSeedInstancePayload(Owner, (FParticleRandomSeedInstancePayload*)InstData, RandomSeedInfo);
}

/** 
 *	Called when an emitter instance is looping...
 *
 *	@param	Owner	The emitter instance that owns this module
 */
void UParticleModuleColor_Seeded::EmitterLoopingNotify(FParticleEmitterInstance* Owner)
{
	if (RandomSeedInfo.bResetSeedOnEmitterLooping == TRUE)
	{
		FParticleRandomSeedInstancePayload* Payload = (FParticleRandomSeedInstancePayload*)(Owner->GetModuleInstanceData(this));
		PrepRandomSeedInstancePayload(Owner, Payload, RandomSeedInfo);
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleColorByParameter implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleColorByParameter);

void UParticleModuleColorByParameter::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;

	UParticleSystemComponent* Component = Owner->Component;

	UBOOL bFound = FALSE;
	for (INT i = 0; i < Component->InstanceParameters.Num(); i++)
	{
		FParticleSysParam Param = Component->InstanceParameters(i);
		if (Param.Name == ColorParam)
		{
			Particle.Color.R = Clamp<FLOAT>((FLOAT)Param.Color.R / 255.9f, 0.f, 1.f);
			Particle.Color.G = Clamp<FLOAT>((FLOAT)Param.Color.G / 255.9f, 0.f, 1.f);
			Particle.Color.B = Clamp<FLOAT>((FLOAT)Param.Color.B / 255.9f, 0.f, 1.f);
			Particle.Color.A = Clamp<FLOAT>((FLOAT)Param.Color.A / 255.9f, 0.f, 1.f);

			bFound	= TRUE;
			break;
		}
	}

	if (!bFound)
	{
		Particle.Color	= DefaultColor;
	}
	Particle.BaseColor	= Particle.Color;
}

void UParticleModuleColorByParameter::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	UBOOL	bFound	= FALSE;

	for (INT i = 0; i < PSysComp->InstanceParameters.Num(); i++)
	{
		FParticleSysParam* Param = &(PSysComp->InstanceParameters(i));
		
		if (Param->Name == ColorParam)
		{
			bFound	=	TRUE;
			break;
		}
	}

	if (!bFound)
	{
		INT NewParamIndex = PSysComp->InstanceParameters.AddZeroed();
		PSysComp->InstanceParameters(NewParamIndex).Name		= ColorParam;
		PSysComp->InstanceParameters(NewParamIndex).ParamType	= PSPT_Color;
		PSysComp->InstanceParameters(NewParamIndex).Actor		= NULL;
	}
}

/**
 *	Retrieve the ParticleSysParams associated with this module.
 *
 *	@param	ParticleSysParamList	The list of FParticleSysParams to add to
 */
void UParticleModuleColorByParameter::GetParticleSysParamsUtilized(TArray<FString>& ParticleSysParamList)
{
	ParticleSysParamList.AddItem(FString::Printf(TEXT("ColorByParm: %s, Default (RGBA) = %3d,%3d,%3d,%3d\n"), 
		*(ColorParam.ToString()), DefaultColor.R, DefaultColor.G, DefaultColor.B, DefaultColor.A));
}

/*-----------------------------------------------------------------------------
	UParticleModuleColorOverLife implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleColorOverLife);

void UParticleModuleColorOverLife::PostLoad()
{
	Super::PostLoad();
}

void UParticleModuleColorOverLife::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetFName() == FName(TEXT("bClampAlpha")))
		{
			UObject* OuterObj = GetOuter();
			check(OuterObj);
			UParticleLODLevel* LODLevel = Cast<UParticleLODLevel>(OuterObj);
			if (LODLevel)
			{
				// The outer is incorrect - warn the user and handle it
				warnf(NAME_Warning, TEXT("UParticleModuleColorOverLife has an incorrect outer... run FixupEmitters on package %s"),
					*(OuterObj->GetOutermost()->GetPathName()));
				OuterObj = LODLevel->GetOuter();
				UParticleEmitter* Emitter = Cast<UParticleEmitter>(OuterObj);
				check(Emitter);
				OuterObj = Emitter->GetOuter();
			}
			UParticleSystem* PartSys = CastChecked<UParticleSystem>(OuterObj);

			PartSys->UpdateColorModuleClampAlpha(this);
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleColorOverLife::AddModuleCurvesToEditor(UInterpCurveEdSetup* EdSetup)
{
#if WITH_EDITORONLY_DATA
	// Iterate over object and find any InterpCurveFloats or UDistributionFloats
	for (TFieldIterator<UStructProperty> It(GetClass()); It; ++It)
	{
		// attempt to get a distribution from a random struct property
		UObject* Distribution = FRawDistribution::TryGetDistributionObjectFromRawDistributionProperty(*It, (BYTE*)this);
		if (Distribution)
		{
			if(Distribution->IsA(UDistributionFloat::StaticClass()))
			{
				// We are assuming that this is the alpha...
				if (bClampAlpha == TRUE)
				{
					EdSetup->AddCurveToCurrentTab(Distribution, It->GetName(), ModuleEditorColor, TRUE, TRUE, TRUE, 0.0f, 1.0f);
				}
				else
				{
					EdSetup->AddCurveToCurrentTab(Distribution, It->GetName(), ModuleEditorColor, TRUE, TRUE);
				}
			}
			else
			{
				// We are assuming that this is the color...
				EdSetup->AddCurveToCurrentTab(Distribution, It->GetName(), ModuleEditorColor, TRUE, TRUE);
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UParticleModuleColorOverLife::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
	FVector ColorVec	= ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	FLOAT	fAlpha		= AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	Particle.Color.R	 = ColorVec.X;
	Particle.BaseColor.R = ColorVec.X;
	Particle.Color.G	 = ColorVec.Y;
	Particle.BaseColor.G = ColorVec.Y;
	Particle.Color.B	 = ColorVec.Z;
	Particle.BaseColor.B = ColorVec.Z;
	Particle.Color.A	 = fAlpha;
	Particle.BaseColor.A = fAlpha;
}

void UParticleModuleColorOverLife::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	if ((Owner == NULL) || (Owner->ActiveParticles <= 0) || 
		(Owner->ParticleData == NULL) || (Owner->ParticleIndices == NULL))
	{
		return;
	}
	const FRawDistribution* FastColorOverLife = ColorOverLife.GetFastRawDistribution();
	const FRawDistribution* FastAlphaOverLife = AlphaOverLife.GetFastRawDistribution();
	CONSOLE_PREFETCH(Owner->ParticleData + (Owner->ParticleIndices[0] * Owner->ParticleStride));
	CONSOLE_PREFETCH_NEXT_CACHE_LINE(Owner->ParticleData + (Owner->ParticleIndices[0] * Owner->ParticleStride));
	if( FastColorOverLife && FastAlphaOverLife )
	{
		// fast path
		BEGIN_UPDATE_LOOP;
		{
			CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
			CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
			FastColorOverLife->GetValue3None(Particle.RelativeTime, &Particle.Color.R);
			FastAlphaOverLife->GetValue1None(Particle.RelativeTime, &Particle.Color.A);
		}
		END_UPDATE_LOOP;
	}
	else
	{
		FVector ColorVec;
		FLOAT	fAlpha;
		BEGIN_UPDATE_LOOP;
		{
			ColorVec = ColorOverLife.GetValue(Particle.RelativeTime, Owner->Component);
			fAlpha = AlphaOverLife.GetValue(Particle.RelativeTime, Owner->Component);
			CONSOLE_PREFETCH(ParticleData + (ParticleIndices[i+1] * ParticleStride));
			CONSOLE_PREFETCH_NEXT_CACHE_LINE(ParticleData + (ParticleIndices[i+1] * ParticleStride));
			Particle.Color.R = ColorVec.X;
			Particle.Color.G = ColorVec.Y;
			Particle.Color.B = ColorVec.Z;
			Particle.Color.A = fAlpha;
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
void UParticleModuleColorOverLife::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	ColorOverLife.Distribution = Cast<UDistributionVectorConstantCurve>(StaticConstructObject(UDistributionVectorConstantCurve::StaticClass(), this));
	UDistributionVectorConstantCurve* ColorOverLifeDist = Cast<UDistributionVectorConstantCurve>(ColorOverLife.Distribution);
	if (ColorOverLifeDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = ColorOverLifeDist->CreateNewKey(Key * 1.0f);
			for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
			{
				if (Key == 0)
				{
					ColorOverLifeDist->SetKeyOut(SubIndex, KeyIndex, 1.0f);
				}
				else
				{
					ColorOverLifeDist->SetKeyOut(SubIndex, KeyIndex, 0.0f);
				}
			}
		}
		ColorOverLifeDist->bIsDirty = TRUE;
	}

	AlphaOverLife.Distribution = Cast<UDistributionFloatConstantCurve>(StaticConstructObject(UDistributionFloatConstantCurve::StaticClass(), this));
	UDistributionFloatConstantCurve* AlphaOverLifeDist = Cast<UDistributionFloatConstantCurve>(AlphaOverLife.Distribution);
	if (AlphaOverLifeDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = AlphaOverLifeDist->CreateNewKey(Key * 1.0f);
			if (Key == 0)
			{
				AlphaOverLifeDist->SetKeyOut(0, KeyIndex, 1.0f);
			}
			else
			{
				AlphaOverLifeDist->SetKeyOut(0, KeyIndex, 0.0f);
			}
		}
		AlphaOverLifeDist->bIsDirty = TRUE;
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleColorScaleOverLife implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleColorScaleOverLife);

void UParticleModuleColorScaleOverLife::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
	FVector ColorVec;
	FLOAT	fAlpha;

	if (bEmitterTime)
	{
		ColorVec	= ColorScaleOverLife.GetValue(Owner->EmitterTime, Owner->Component);
		fAlpha		= AlphaScaleOverLife.GetValue(Owner->EmitterTime, Owner->Component);
	}
	else
	{
		ColorVec	= ColorScaleOverLife.GetValue(Particle.RelativeTime, Owner->Component);
		fAlpha		= AlphaScaleOverLife.GetValue(Particle.RelativeTime, Owner->Component);
	}

	Particle.Color.R *= ColorVec.X;
	Particle.Color.G *= ColorVec.Y;
	Particle.Color.B *= ColorVec.Z;
	Particle.Color.A *= fAlpha;
}

void UParticleModuleColorScaleOverLife::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{ 
	const FRawDistribution* FastColorScaleOverLife = ColorScaleOverLife.GetFastRawDistribution();
	const FRawDistribution* FastAlphaScaleOverLife = AlphaScaleOverLife.GetFastRawDistribution();
	FVector ColorVec;
	FLOAT	fAlpha;
	if( FastColorScaleOverLife && FastAlphaScaleOverLife )
	{
		// fast path
		if (bEmitterTime)
		{
			BEGIN_UPDATE_LOOP;
			{
				FastColorScaleOverLife->GetValue3None(Owner->EmitterTime, &ColorVec.X);
				FastAlphaScaleOverLife->GetValue1None(Owner->EmitterTime, &fAlpha);
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
				FastColorScaleOverLife->GetValue3None(Particle.RelativeTime, &ColorVec.X);
				FastAlphaScaleOverLife->GetValue1None(Particle.RelativeTime, &fAlpha);
				Particle.Color.R *= ColorVec.X;
				Particle.Color.G *= ColorVec.Y;
				Particle.Color.B *= ColorVec.Z;
				Particle.Color.A *= fAlpha;
			}
			END_UPDATE_LOOP;
		}
	}
	else
	{
		if (bEmitterTime)
		{
			BEGIN_UPDATE_LOOP;
				ColorVec = ColorScaleOverLife.GetValue(Owner->EmitterTime, Owner->Component);
				fAlpha = AlphaScaleOverLife.GetValue(Owner->EmitterTime, Owner->Component);
				Particle.Color.R *= ColorVec.X;
				Particle.Color.G *= ColorVec.Y;
				Particle.Color.B *= ColorVec.Z;
				Particle.Color.A *= fAlpha;
			END_UPDATE_LOOP;
		}
		else
		{
			BEGIN_UPDATE_LOOP;
				ColorVec = ColorScaleOverLife.GetValue(Particle.RelativeTime, Owner->Component);
				fAlpha = AlphaScaleOverLife.GetValue(Particle.RelativeTime, Owner->Component);
				Particle.Color.R *= ColorVec.X;
				Particle.Color.G *= ColorVec.Y;
				Particle.Color.B *= ColorVec.Z;
				Particle.Color.A *= fAlpha;
			END_UPDATE_LOOP;
		}
	}
}

/**
 *	Called when the module is created, this function allows for setting values that make
 *	sense for the type of emitter they are being used in.
 *
 *	@param	Owner			The UParticleEmitter that the module is being added to.
 */
void UParticleModuleColorScaleOverLife::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	ColorScaleOverLife.Distribution = Cast<UDistributionVectorConstantCurve>(StaticConstructObject(UDistributionVectorConstantCurve::StaticClass(), this));
	UDistributionVectorConstantCurve* ColorScaleOverLifeDist = Cast<UDistributionVectorConstantCurve>(ColorScaleOverLife.Distribution);
	if (ColorScaleOverLifeDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (INT Key = 0; Key < 2; Key++)
		{
			INT	KeyIndex = ColorScaleOverLifeDist->CreateNewKey(Key * 1.0f);
			for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
			{
				ColorScaleOverLifeDist->SetKeyOut(SubIndex, KeyIndex, 1.0f);
			}
		}
		ColorScaleOverLifeDist->bIsDirty = TRUE;
	}
}

#if WITH_EDITOR
/**
 *	Get the number of custom entries this module has. Maximum of 3.
 *
 *	@return	INT		The number of custom menu entries
 */
INT UParticleModuleColorScaleOverLife::GetNumberOfCustomMenuOptions() const
{
	return 1;
}

/**
 *	Get the display name of the custom menu entry.
 *
 *	@param	InEntryIndex		The custom entry index (0-2)
 *	@param	OutDisplayString	The string to display for the menu
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleColorScaleOverLife::GetCustomMenuEntryDisplayString(INT InEntryIndex, FString& OutDisplayString) const
{
	if (InEntryIndex == 0)
	{
		OutDisplayString = LocalizeUnrealEd("Module_ColorScaleOverLife_SetupParticleParam");
		return TRUE;
	}
	return FALSE;
}

/**
 *	Perform the custom menu entry option.
 *
 *	@param	InEntryIndex		The custom entry index (0-2) to perform
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL UParticleModuleColorScaleOverLife::PerformCustomMenuEntry(INT InEntryIndex)
{
	if (GIsEditor == TRUE)
	{
		if (InEntryIndex == 0)
		{
			debugf(TEXT("Setup color scale over life for particle param!"));
			ColorScaleOverLife.Distribution = Cast<UDistributionVectorParticleParameter>(StaticConstructObject(UDistributionVectorParticleParameter::StaticClass(), this));
			UDistributionVectorParticleParameter* ColorDist = Cast<UDistributionVectorParticleParameter>(ColorScaleOverLife.Distribution);
			if (ColorDist)
			{
				ColorDist->ParameterName = FName(TEXT("InstanceColorScaleOverLife"));
				ColorDist->ParamModes[0] = DPM_Direct;
				ColorDist->ParamModes[1] = DPM_Direct;
				ColorDist->ParamModes[2] = DPM_Direct;
				ColorDist->Constant = FVector(1.0f);
				ColorDist->bIsDirty = TRUE;
			}

			AlphaScaleOverLife.Distribution = Cast<UDistributionFloatParticleParameter>(StaticConstructObject(UDistributionFloatParticleParameter::StaticClass(), this));
			UDistributionFloatParticleParameter* AlphaDist = Cast<UDistributionFloatParticleParameter>(AlphaScaleOverLife.Distribution);
			if (AlphaDist)
			{
				AlphaDist->ParameterName = FName(TEXT("InstanceAlphaScaleOverLife"));
				AlphaDist->ParamMode = DPM_Direct;
				AlphaDist->Constant = 1.0f;
				AlphaDist->bIsDirty = TRUE;
			}
		}
		return TRUE;
	}
	return FALSE;
}
#endif
