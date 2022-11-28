/*=============================================================================
	UnParticleModules.cpp: Particle module implementation.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	Abstract base modules used for categorization.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UParticleModuleVelocityBase);
IMPLEMENT_CLASS(UParticleModuleAccelerationBase);
IMPLEMENT_CLASS(UParticleModuleCollisionBase);
IMPLEMENT_CLASS(UParticleModuleSizeBase);
IMPLEMENT_CLASS(UParticleModuleLocationBase);
IMPLEMENT_CLASS(UParticleModuleRotationBase);
IMPLEMENT_CLASS(UParticleModuleRotationRateBase);
IMPLEMENT_CLASS(UParticleModuleLifetimeBase);
IMPLEMENT_CLASS(UParticleModuleColorBase);
IMPLEMENT_CLASS(UParticleModuleSizeScale);
IMPLEMENT_CLASS(UParticleModuleSubUVBase);
IMPLEMENT_CLASS(UParticleModuleSubUV);
IMPLEMENT_CLASS(UParticleModuleAttractorBase);
IMPLEMENT_CLASS(UParticleModuleAttractorPoint);
IMPLEMENT_CLASS(UParticleModuleAttractorLine);

/*-----------------------------------------------------------------------------
	Helper macros.
-----------------------------------------------------------------------------*/

#define BEGIN_UPDATE_LOOP																								\
	UINT&			ActiveParticles = Owner->ActiveParticles;															\
	UINT			CurrentOffset	= Offset;																			\
	const BYTE*		ParticleData	= Owner->ParticleData;																\
	const UINT		ParticleStride	= Owner->ParticleStride;															\
	_WORD*			ParticleIndices	= Owner->ParticleIndices;															\
	for( INT i=ActiveParticles-1; i>=0; i-- )																			\
	{																													\
		const INT	CurrentIndex	= ParticleIndices[i];																\
		const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;										\
		FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
#define END_UPDATE_LOOP																									\
		CurrentOffset				= Offset;																			\
	}

#define SPAWN_INIT																										\
	const UINT		ActiveParticles	= Owner->ActiveParticles;															\
	const UINT		ParticleStride	= Owner->ParticleStride;															\
	const BYTE*		ParticleBase	= Owner->ParticleData + Owner->ParticleIndices[ActiveParticles] * ParticleStride;	\
	UINT			CurrentOffset	= Offset;																			\
	FBaseParticle&	Particle		= *((FBaseParticle*) ParticleBase);

#define PARTICLE_ELEMENT(Type,Name)																						\
	Type& ##Name = *((Type*)(ParticleBase + CurrentOffset));															\
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

static FColor ColorFromVector(const FVector& ColorVec)
{
	FColor Color;
	Color.R = appTrunc( Clamp<FLOAT>(ColorVec.X, 0.f, 255.9f) );
	Color.G = appTrunc( Clamp<FLOAT>(ColorVec.Y, 0.f, 255.9f) );
	Color.B = appTrunc( Clamp<FLOAT>(ColorVec.Z, 0.f, 255.9f) );
	Color.A = 255;
	return Color;
}


/*-----------------------------------------------------------------------------
	UParticleModule implementation.
-----------------------------------------------------------------------------*/

void UParticleModule::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
}
void UParticleModule::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
}
INT UParticleModule::RequiredBytes()
{
	return 0;
}

/** Fill an array with each Object property that fulfills the FCurveEdInterface interface. */
void UParticleModule::GetCurveObjects( TArray<UObject*>& OutCurves )
{
	for (TFieldIterator<UProperty> It(GetClass()); It; ++It)
	{
		UProperty* Prop = *It;
		UObjectProperty* ObjectProp = Cast<UObjectProperty>(Prop);
		if(ObjectProp)
		{
			if( ObjectProp->PropertyClass->IsChildOf(UDistributionFloat::StaticClass()) )
			{
				UDistributionFloat* FloatDist = *((UDistributionFloat**)(((BYTE*)this) + ObjectProp->Offset));	
				OutCurves.AddItem(FloatDist);
			}
			else if( ObjectProp->PropertyClass->IsChildOf(UDistributionVector::StaticClass()) )
			{
				UDistributionVector* VectorDist = *((UDistributionVector**)(((BYTE*)this) + ObjectProp->Offset));	
				OutCurves.AddItem(VectorDist);
			}
		}
	}
}

/** Add all curve-editable Objects within this module to the curve. */
void UParticleModule::AddModuleCurvesToEditor( UInterpCurveEdSetup* EdSetup )
{
	// Iterate over object and find any InterpCurveFloats or UDistributionFloats
	for (TFieldIterator<UProperty> It(GetClass()); It; ++It)
	{
		UProperty* Prop = *It;
		UObjectProperty* ObjectProp = Cast<UObjectProperty>(Prop);
		if(ObjectProp)
		{
			if( ObjectProp->PropertyClass->IsChildOf(UDistributionFloat::StaticClass()) )
			{
				UDistributionFloat* FloatDist = *((UDistributionFloat**)(((BYTE*)this) + ObjectProp->Offset));	
				EdSetup->AddCurveToCurrentTab( FloatDist, FString( ObjectProp->GetName() ), ModuleEditorColor, bCurvesAsColor );				
			}
			else if( ObjectProp->PropertyClass->IsChildOf(UDistributionVector::StaticClass()) )
			{
				UDistributionVector* VectorDist = *((UDistributionVector**)(((BYTE*)this) + ObjectProp->Offset));	
				EdSetup->AddCurveToCurrentTab( VectorDist, FString( ObjectProp->GetName() ), ModuleEditorColor, bCurvesAsColor );				
			}
		}
	}
}

/** Remove all curve-editable Objects within this module from the curve. */
void UParticleModule::RemoveModuleCurvesFromEditor( UInterpCurveEdSetup* EdSetup )
{
	// Iterate over object and find any InterpCurveFloats or UDistributionFloats
	for (TFieldIterator<UProperty> It(GetClass()); It; ++It)
	{
		UProperty* Prop = *It;
		UObjectProperty* ObjectProp = Cast<UObjectProperty>(Prop);
		if(ObjectProp)
		{
			if( ObjectProp->PropertyClass->IsChildOf(UDistributionFloat::StaticClass()) )
			{
				UDistributionFloat* FloatDist = *((UDistributionFloat**)(((BYTE*)this) + ObjectProp->Offset));	
				EdSetup->RemoveCurve(FloatDist);				
			}
			else if( ObjectProp->PropertyClass->IsChildOf(UDistributionVector::StaticClass()) )
			{
				UDistributionVector* VectorDist = *((UDistributionVector**)(((BYTE*)this) + ObjectProp->Offset));	
				EdSetup->RemoveCurve(VectorDist);
			}
		}
	}
}

/** Returns true if this Module has any curves that can be pushed into the curve editor. */
UBOOL UParticleModule::ModuleHasCurves()
{
	TArray<UObject*> Curves;
	GetCurveObjects(Curves);

	return (Curves.Num() > 0);
}

/** Returns whether any property of this module is displayed in the supplied CurveEd setup. */
UBOOL UParticleModule::IsDisplayedInCurveEd(  UInterpCurveEdSetup* EdSetup )
{
	TArray<UObject*> Curves;
	GetCurveObjects(Curves);

	for(INT i=0; i<Curves.Num(); i++)
	{
		if( EdSetup->ShowingCurve(Curves(i)) )
		{
			return true;
		}	
	}

	return false;
}

IMPLEMENT_CLASS(UParticleModule);

/*-----------------------------------------------------------------------------
	UParticleModuleLocation implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleLocation::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	if (Owner->Template->UseLocalSpace)
	{
		Particle.Location += StartLocation->GetValue( Owner->EmitterTime );
	}
	else
	{
		FVector StartLoc = StartLocation->GetValue(Owner->EmitterTime);
		StartLoc = Owner->Component->LocalToWorld.TransformNormal(StartLoc);
		Particle.Location += StartLoc;
	}
}

void UParticleModuleLocation::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
    // Draw the location as a wire star
//    FVector Position = StartLocation->GetValue(Owner->EmitterTime);
	FVector Position = FVector(0.0f);

    // Nothing else to do if it is constant...
    if (StartLocation->IsA(UDistributionVectorUniform::StaticClass()))
    {
        // Draw a box showing the min/max extents
        UDistributionVectorUniform* Uniform = CastChecked<UDistributionVectorUniform>(StartLocation);
		
		Position = (Uniform->GetMaxValue() + Uniform->GetMinValue()) / 2.0f;

        PRI->DrawWireBox(FBox(Uniform->GetMinValue(), Uniform->GetMaxValue()), ModuleEditorColor);
    }
    else
    if (StartLocation->IsA(UDistributionVectorConstantCurve::StaticClass()))
    {
        // Draw a box showing the min/max extents
        UDistributionVectorConstantCurve* Curve = CastChecked<UDistributionVectorConstantCurve>(StartLocation);

        //Curve->
		Position = StartLocation->GetValue(0.0f);
    }

	PRI->DrawWireStar(Position, 10.0f, ModuleEditorColor);
}

IMPLEMENT_CLASS(UParticleModuleLocation);

/*-----------------------------------------------------------------------------
	UParticleModuleMeshRotation implementation.
-----------------------------------------------------------------------------*/
void UParticleModuleMeshRotation::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
        FVector Rotation = StartRotation->GetValue(Owner->EmitterTime);
        FVector* pkVectorData = (FVector*)((BYTE*)&Particle + Owner->PayloadOffset);
        // FVector  MeshRotation
        // FVector  MeshRotationRate
        // FVector  MeshRotationRateBase
        pkVectorData[0].X += Rotation.X * (PI/180.f) * 360.0f;
        pkVectorData[0].Y += Rotation.Y * (PI/180.f) * 360.0f;
        pkVectorData[0].Z += Rotation.Z * (PI/180.f) * 360.0f;
}

IMPLEMENT_CLASS(UParticleModuleMeshRotation);

/*-----------------------------------------------------------------------------
	UParticleModuleMeshRotationRate implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleMeshRotationRate::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;
		FVector StartRate = StartRotationRate->GetValue(Owner->EmitterTime);// * ((FLOAT)PI/180.f);
        FVector* pkVectorData = (FVector*)((BYTE*)&Particle + Owner->PayloadOffset);
        // FVector  MeshRotation
        // FVector  MeshRotationRate
        // FVector  MeshRotationRateBase
        FVector StartValue;
        StartValue.X = StartRate.X * 360.0f;
        StartValue.Y = StartRate.Y * 360.0f;
        StartValue.Z = StartRate.Z * 360.0f;
        pkVectorData[2] += StartValue;
        pkVectorData[1] += StartValue;
}

IMPLEMENT_CLASS(UParticleModuleMeshRotationRate);

/*-----------------------------------------------------------------------------
	UParticleModuleRotation implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleRotation::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	Particle.Rotation += (PI/180.f) * 360.0f * StartRotation->GetValue( Owner->EmitterTime );
}
IMPLEMENT_CLASS(UParticleModuleRotation);


/*-----------------------------------------------------------------------------
	UParticleModuleRotationRate implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleRotationRate::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	FLOAT StartRotRate = (PI/180.f) * 360.0f * StartRotationRate->GetValue( Owner->EmitterTime );
	Particle.RotationRate += StartRotRate;
	Particle.BaseRotationRate += StartRotRate;
}
IMPLEMENT_CLASS(UParticleModuleRotationRate);

/*-----------------------------------------------------------------------------
	UParticleModuleRotationOverLifetime implementation.
-----------------------------------------------------------------------------*/
void UParticleModuleRotationOverLifetime::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
		FLOAT Rotation = RotationOverLife->GetValue(Particle.RelativeTime);
		// For now, we are just using the X-value
		Particle.Rotation	*= Rotation * (PI/180.f) * 360.0f;
	END_UPDATE_LOOP;
}
IMPLEMENT_CLASS(UParticleModuleRotationOverLifetime);

/*-----------------------------------------------------------------------------
	UParticleModuleSize implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleSize::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	FVector Size		 = StartSize->GetValue( Owner->EmitterTime );
	Particle.Size		+= Size;
	Particle.BaseSize	+= Size;
}
IMPLEMENT_CLASS(UParticleModuleSize);

/*-----------------------------------------------------------------------------
	UParticleModuleSizeMultiplyVelocity implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleSizeMultiplyVelocity::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	FVector SizeScale = VelocityMultiplier->GetValue( Particle.RelativeTime ) * Particle.Velocity.Size();
	if( MultiplyX )
		Particle.Size.X *= SizeScale.X;
	if( MultiplyY )
		Particle.Size.Y *= SizeScale.Y;
	if( MultiplyZ )
		Particle.Size.Z *= SizeScale.Z;
}

void UParticleModuleSizeMultiplyVelocity::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	BEGIN_UPDATE_LOOP;
		FVector SizeScale = VelocityMultiplier->GetValue( Particle.RelativeTime ) * Particle.Velocity.Size();
		if( MultiplyX )
			Particle.Size.X *= SizeScale.X;
		if( MultiplyY )
			Particle.Size.Y *= SizeScale.Y;
		if( MultiplyZ )
			Particle.Size.Z *= SizeScale.Z;
	END_UPDATE_LOOP;
}
IMPLEMENT_CLASS(UParticleModuleSizeMultiplyVelocity);

/*-----------------------------------------------------------------------------
	UParticleModuleSizeMultiplyLife implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleSizeMultiplyLife::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	FVector SizeScale = LifeMultiplier->GetValue( Particle.RelativeTime );
	if( MultiplyX )
		Particle.Size.X *= SizeScale.X;
	if( MultiplyY )
		Particle.Size.Y *= SizeScale.Y;
	if( MultiplyZ )
		Particle.Size.Z *= SizeScale.Z;
}

void UParticleModuleSizeMultiplyLife::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	BEGIN_UPDATE_LOOP;
		FVector SizeScale = LifeMultiplier->GetValue( Particle.RelativeTime );
		if( MultiplyX )
			Particle.Size.X *= SizeScale.X;
		if( MultiplyY )
			Particle.Size.Y *= SizeScale.Y;
		if( MultiplyZ )
			Particle.Size.Z *= SizeScale.Z;
	END_UPDATE_LOOP;
}
IMPLEMENT_CLASS(UParticleModuleSizeMultiplyLife);

/*-----------------------------------------------------------------------------
	UParticleModuleSizeScale implementation.
-----------------------------------------------------------------------------*/
void UParticleModuleSizeScale::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	BEGIN_UPDATE_LOOP;
		FVector ScaleFactor = SizeScale->GetValue(Particle.RelativeTime);
		Particle.Size = Particle.BaseSize * ScaleFactor;
	END_UPDATE_LOOP;
}

/*-----------------------------------------------------------------------------
	UParticleModuleSubUV implementation.
-----------------------------------------------------------------------------*/
void UParticleModuleSubUV::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	check(Owner->Template);
	if (!Owner->Template->TypeDataModule)
	{
		return;
	}
	if (!Owner->Template->TypeDataModule->IsA(UParticleModuleTypeDataSubUV::StaticClass()))
	{
		return;
	}

	FParticleSpriteSubUVEmitterInstance* Instance = (FParticleSpriteSubUVEmitterInstance*)(Owner);
	UParticleModuleTypeDataSubUV* SubUVTypeData = Instance->SubUVTypeData;

	// Grab the interpolation method...
    ESubUVInterpMethod eMethod = (ESubUVInterpMethod)(SubUVTypeData->InterpolationMethod);
	
	INT	PayloadOffset	= Owner->PayloadOffset;
	INT iTotalSubImages = SubUVTypeData->SubImages_Horizontal * SubUVTypeData->SubImages_Vertical;

	float fInterp;
	INT iImageIndex;
	INT iImageH;
	INT iImageV;
	INT iImage2H;
	INT iImage2V = 0;

	BEGIN_UPDATE_LOOP;
		if (Particle.RelativeTime > 1.0f)
		{
			continue;
		}

		if ((eMethod == PSSUVIM_Linear) || (eMethod == PSSUVIM_Linear_Blend))
		{
			fInterp = SubImageIndex->GetValue(Particle.RelativeTime);
			// Assuming a 0..<# sub images> range here...
			iImageIndex = (INT)fInterp;
			iImageIndex = Clamp(iImageIndex, 0, iTotalSubImages - 1);

			if (fInterp > (FLOAT)iImageIndex)
			{
				fInterp = fInterp - (FLOAT)iImageIndex;
			}
			else
			{
				fInterp = (FLOAT)iImageIndex - fInterp;
			}

			if (eMethod == PSSUVIM_Linear)
				fInterp = 0.0f;
		}
		else
		if ((eMethod == PSSUVIM_Random) || (eMethod == PSSUVIM_Random_Blend))
		{
			fInterp = appFrand();
			iImageIndex = (INT)(fInterp * iTotalSubImages);

			if (eMethod == PSSUVIM_Random)
				fInterp = 0.0f;
		}
		else
		{
			fInterp = 0.0f;
			iImageIndex = 0;
		}

		iImageH = iImageIndex % SubUVTypeData->SubImages_Horizontal;
		iImageV = iImageIndex / SubUVTypeData->SubImages_Horizontal;

		if (iImageH == (SubUVTypeData->SubImages_Horizontal - 1))
		{
			iImage2H = 0;
			if (iImageV == (SubUVTypeData->SubImages_Vertical - 1))
				iImage2V = 0;
			else
				iImage2V = iImageV + 1;
		}
		else
		{
			iImage2H = iImageH + 1;
			iImage2V = iImageV;
		}

		// Update the payload
		FLOAT* PayloadData = (FLOAT*)(((BYTE*)&Particle) + PayloadOffset);
		
		//     FLOAT Interp
		//     FLOAT imageH, imageV
		//     FLOAT image2H, image2V
		PayloadData[0] = fInterp;
		PayloadData[1] = (FLOAT)iImageH;
		PayloadData[2] = (FLOAT)iImageV;
		PayloadData[3] = (FLOAT)iImage2H;
		PayloadData[4] = (FLOAT)iImage2V;

	END_UPDATE_LOOP;
}

/*-----------------------------------------------------------------------------
	UParticleModuleRotationRateMultiplyLife implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleRotationRateMultiplyLife::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	FLOAT RateScale = LifeMultiplier->GetValue( Particle.RelativeTime );
	Particle.RotationRate *= RateScale;
}

void UParticleModuleRotationRateMultiplyLife::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	BEGIN_UPDATE_LOOP;
		FLOAT RateScale = LifeMultiplier->GetValue( Particle.RelativeTime );
		Particle.RotationRate *= RateScale;
	END_UPDATE_LOOP;
}
IMPLEMENT_CLASS(UParticleModuleRotationRateMultiplyLife);

/*-----------------------------------------------------------------------------
	UParticleModuleAcceleration implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleAcceleration::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	PARTICLE_ELEMENT( FVector, UsedAcceleration );
	UsedAcceleration = Acceleration->GetValue(Owner->EmitterTime);
	Particle.Velocity		+= UsedAcceleration * SpawnTime;
	Particle.BaseVelocity	+= UsedAcceleration * SpawnTime;
}

void UParticleModuleAcceleration::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	BEGIN_UPDATE_LOOP;
		PARTICLE_ELEMENT( FVector, UsedAcceleration );
		Particle.Velocity		+= UsedAcceleration * DeltaTime;
		Particle.BaseVelocity	+= UsedAcceleration * DeltaTime;
	END_UPDATE_LOOP;
}

INT UParticleModuleAcceleration::RequiredBytes()
{
	// FVector UsedAcceleration
	return sizeof(FVector);
}
IMPLEMENT_CLASS(UParticleModuleAcceleration);

/*-----------------------------------------------------------------------------
	UParticleModuleAccelerationOverLifetime implementation.
-----------------------------------------------------------------------------*/
void UParticleModuleAccelerationOverLifetime::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	BEGIN_UPDATE_LOOP;
		// Acceleration should always be in world space...
		FVector Accel = AccelOverLife->GetValue(Particle.RelativeTime);
		Particle.Velocity		+= Accel * DeltaTime;
		Particle.BaseVelocity	+= Accel * DeltaTime;
	END_UPDATE_LOOP;
}
IMPLEMENT_CLASS(UParticleModuleAccelerationOverLifetime);

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataBase implementation.
-----------------------------------------------------------------------------*/
FParticleEmitterInstance* UParticleModuleTypeDataBase::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	return 0;
}

void UParticleModuleTypeDataBase::SetToSensibleDefaults()
{
}

IMPLEMENT_CLASS(UParticleModuleTypeDataBase);

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataSubUV implementation.
-----------------------------------------------------------------------------*/
FParticleEmitterInstance* UParticleModuleTypeDataSubUV::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance* Instance = new FParticleSpriteSubUVEmitterInstance(InEmitterParent, InComponent);
	return Instance;
}

void UParticleModuleTypeDataSubUV::SetToSensibleDefaults()
{
}

void UParticleModuleTypeDataSubUV::PostEditChange(UProperty* PropertyThatChanged)
{
	UParticleSystem* PartSys = CastChecked<UParticleSystem>(GetOuter());

	if (PropertyThatChanged)
		PartSys->PostEditChange(PropertyThatChanged);
}

IMPLEMENT_CLASS(UParticleModuleTypeDataSubUV);

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataMesh implementation.
-----------------------------------------------------------------------------*/
FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
    SetToSensibleDefaults();
	FParticleEmitterInstance* Instance = new FParticleMeshEmitterInstance(InEmitterParent, InComponent);
	return Instance;
}

void UParticleModuleTypeDataMesh::SetToSensibleDefaults()
{
    if (Mesh == NULL)
        Mesh = (UStaticMesh*)UObject::StaticLoadObject(UStaticMesh::StaticClass(),NULL,TEXT("EditorMeshes.TexPropCube"),NULL,LOAD_NoFail,NULL);
}

void UParticleModuleTypeDataMesh::PostEditChange(UProperty* PropertyThatChanged)
{
	UParticleSystem* PartSys = CastChecked<UParticleSystem>(GetOuter());

	if (PropertyThatChanged->GetFName() == FName(TEXT("Mesh")))
	{
		PartSys->PostEditChange(PropertyThatChanged);
	}
}


DWORD UParticleModuleTypeDataMesh::GetLayerMask() const
{
	DWORD LayerMask = 0;
	if (Mesh)
	{
		for(INT MaterialIndex = 0;MaterialIndex < Mesh->Materials.Num();MaterialIndex++)
		{
			if (Mesh->Materials(MaterialIndex).Material)
				LayerMask |= Mesh->Materials(MaterialIndex).Material->GetLayerMask();
		}
	}

	if (LayerMask)
		return LayerMask;
	return GEngine->DefaultMaterial->GetLayerMask();
}

IMPLEMENT_CLASS(UParticleModuleTypeDataMesh);

/*-----------------------------------------------------------------------------
	UParticleModuleTypeDataTrail implementation.
-----------------------------------------------------------------------------*/
FParticleEmitterInstance* UParticleModuleTypeDataTrail::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
    SetToSensibleDefaults();
	FParticleEmitterInstance* Instance = new FParticleTrailEmitterInstance(InEmitterParent, InComponent);
	return Instance;
}

void UParticleModuleTypeDataTrail::SetToSensibleDefaults()
{
}

void UParticleModuleTypeDataTrail::PostEditChange(UProperty* PropertyThatChanged)
{
    Super::PostEditChange(PropertyThatChanged);
}

DWORD UParticleModuleTypeDataTrail::GetLayerMask() const
{
	return GEngine->DefaultMaterial->GetLayerMask();
}

IMPLEMENT_CLASS(UParticleModuleTypeDataTrail);

/*-----------------------------------------------------------------------------
	UParticleModuleVelocity implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleVelocity::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;

	FVector FromOrigin;
	FVector Vel = StartVelocity->GetValue( Owner->EmitterTime );
	if(Owner->Template->UseLocalSpace)
	{
		FromOrigin = Particle.Location.SafeNormal();
	}
	else
	{
		FromOrigin = (Particle.Location - Owner->Location).SafeNormal();
		Vel = Owner->Component->LocalToWorld.TransformNormal(Vel);
	}

	Vel += FromOrigin * StartVelocityRadial->GetValue(Owner->EmitterTime);
	Particle.Velocity		+= Vel;
	Particle.BaseVelocity	+= Vel;

}
IMPLEMENT_CLASS(UParticleModuleVelocity);

/*-----------------------------------------------------------------------------
	UParticleModuleVelocityInheritParent implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleVelocityInheritParent::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	SPAWN_INIT;

	FVector Vel = FVector(0.0f);
	if (Owner->Template->UseLocalSpace)
	{
		Vel = Owner->Component->LocalToWorld.InverseTransformNormal(Owner->Component->PartSysVelocity);
	}
	else
	{
		Vel = Owner->Component->PartSysVelocity;
	}

	Particle.Velocity		+= Vel;
	Particle.BaseVelocity	+= Vel;
}
IMPLEMENT_CLASS(UParticleModuleVelocityInheritParent);

/*-----------------------------------------------------------------------------
	UParticleModuleVelocityOverLifetime implementation.
-----------------------------------------------------------------------------*/
void UParticleModuleVelocityOverLifetime::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	BEGIN_UPDATE_LOOP;
		FVector Vel = VelOverLife->GetValue(Particle.RelativeTime);
		Particle.Velocity		*= Vel;
	END_UPDATE_LOOP;
}
IMPLEMENT_CLASS(UParticleModuleVelocityOverLifetime);

/*-----------------------------------------------------------------------------
	UParticleModuleColor implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleColor::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	FVector ColorVec = StartColor->GetValue( Owner->EmitterTime );
	Particle.Color = ColorFromVector(ColorVec);
}
IMPLEMENT_CLASS(UParticleModuleColor);

/*-----------------------------------------------------------------------------
	UParticleModuleColorByParameter implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UParticleModuleColorByParameter);

void UParticleModuleColorByParameter::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
//    FName ColorParam;
	SPAWN_INIT;

	UParticleSystemComponent* Component = Owner->Component;

	for (INT i = 0; i < Component->InstanceParameters.Num(); i++)
	{
		FParticleSysParam Param = Component->InstanceParameters(i);
		if (Param.Name == ColorParam)
		{
			// Found the param!
			Particle.Color = Param.Color;
		}
	}

//	Component->Get
//	FColor Color = Owner->
//	FVector ColorVec = StartColor->GetValue( Owner->EmitterTime );
//	Particle.Color = ColorFromVector(ColorVec);
}

/*-----------------------------------------------------------------------------
	UParticleModuleColorOverLife implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleColorOverLife::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	FVector ColorVec = ColorOverLife->GetValue( Particle.RelativeTime );
	Particle.Color = ColorFromVector(ColorVec);
}

void UParticleModuleColorOverLife::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	BEGIN_UPDATE_LOOP;
		FVector ColorVec = ColorOverLife->GetValue( Particle.RelativeTime );
		Particle.Color = ColorFromVector(ColorVec);
	END_UPDATE_LOOP;
}
IMPLEMENT_CLASS(UParticleModuleColorOverLife);

/*-----------------------------------------------------------------------------
	UParticleModuleLifetime implementation.
-----------------------------------------------------------------------------*/

void UParticleModuleLifetime::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	FLOAT MaxLifetime = Lifetime->GetValue( Owner->EmitterTime );
	if( Particle.OneOverMaxLifetime > 0.f )
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

void UParticleModuleLifetime::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	//@todo. Remove this commented out code, and in turn, the Update function 
	// of the Lifetime module?
	// Updating of relative time is a common occurance in any emitter, so that 
	// was moved up to the base emitter class. In the Tick function, when 
	// resetting the velocity and size, the following also occurs:
	//     Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
	// The killing of particles was moved to a virtual function, KillParticles, 
	// in the FParticleEmitterInstance class. This allows for custom emitters, 
	// such as the MeshEmitter, to properly 'kill' a particle - in that case, 
	// removing the mesh from the scene.
}
IMPLEMENT_CLASS(UParticleModuleLifetime);

/*-----------------------------------------------------------------------------
	UParticleModuleAttractorLine implementation.
-----------------------------------------------------------------------------*/
void UParticleModuleAttractorLine::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	//@todo. Remove this is Spawn doesn't get used!
	UParticleModule::Spawn(Owner, Offset, SpawnTime);
}

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
            FLOAT AttractorRange = Range->GetValue(fRatio);
            
            FVector LineToPoint = AdjustedLocation - ProjectedParticle;
    		FLOAT Distance = LineToPoint.Size();

            if (Distance <= AttractorRange)
            {
                // Adjust the strength based on the range ratio
                FLOAT AttractorStrength = Strength->GetValue((AttractorRange - Distance) / AttractorRange);
                FVector Direction = LineToPoint^Line;
    			// Adjust the VELOCITY of the particle based on the attractor... 
        		Particle.Velocity += Direction * AttractorStrength * DeltaTime;
            }
        }
	END_UPDATE_LOOP;
}

void UParticleModuleAttractorLine::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
    PRI->DrawLine(EndPoint0, EndPoint1, ModuleEditorColor);

    FLOAT CurrRatio = Owner->EmitterTime / Owner->Template->EmitterDuration;
    FLOAT LineRange = Range->GetValue(CurrRatio);

    // Determine the position of the range at this time.
    FVector LinePos = EndPoint0 + CurrRatio * (EndPoint1 - EndPoint0);

    // Draw a wire star at the position of the range.
	PRI->DrawWireStar(LinePos, 10.0f, ModuleEditorColor);
    // Draw bounding circle for the current range.
	// This should be oriented such that it appears correctly... ie, as 'open' to the camera as possible
	PRI->DrawCircle(LinePos, FVector(1,0,0), FVector(0,1,0), ModuleEditorColor, LineRange, 32);
}

/*-----------------------------------------------------------------------------
	UParticleModuleAttractorPoint implementation.
-----------------------------------------------------------------------------*/
void UParticleModuleAttractorPoint::Spawn(FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime)
{
	//@todo. Remove this is Spawn doesn't get used!
	UParticleModule::Spawn(Owner, Offset, SpawnTime);
}

void UParticleModuleAttractorPoint::Update(FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime)
{
	// Grab the position of the attractor in Emitter time???
	FVector AttractorPosition = Position->GetValue(Owner->EmitterTime);
    FLOAT AttractorRange = Range->GetValue(Owner->EmitterTime);

	BEGIN_UPDATE_LOOP;
		// If the particle is within range...
		FVector Dir = AttractorPosition - (Particle.Location - Owner->Location);
		FLOAT Distance = Dir.Size();
		if (Distance <= AttractorRange)
		{
			// Determine the strength
            FLOAT AttractorStrength = 0.0f;

            if (StrengthByDistance)
            {
                // on actual distance
			    AttractorStrength = Strength->GetValue((AttractorRange - Distance) / AttractorRange);
            }
            else
            {
                // on emitter time
                AttractorStrength = Strength->GetValue(Owner->EmitterTime);
            }

			// Adjust the VELOCITY of the particle based on the attractor... 
    		Particle.Velocity += Dir * AttractorStrength * DeltaTime;
		}
	END_UPDATE_LOOP;

}

void UParticleModuleAttractorPoint::Render3DPreview(FParticleEmitterInstance* Owner, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
    FVector PointPos = Position->GetValue(Owner->EmitterTime);
//    FLOAT PointStr = Strength->GetValue(Owner->EmitterTime);
    FLOAT PointRange = Range->GetValue(Owner->EmitterTime);

    // Draw a wire star at the position.
	PRI->DrawWireStar(PointPos, 10.0f, ModuleEditorColor);
    
	// Draw bounding circles for the range.
	PRI->DrawCircle(PointPos, FVector(1,0,0), FVector(0,1,0), ModuleEditorColor, PointRange, 32);
	PRI->DrawCircle(PointPos, FVector(1,0,0), FVector(0,0,1), ModuleEditorColor, PointRange, 32);
	PRI->DrawCircle(PointPos, FVector(0,1,0), FVector(0,0,1), ModuleEditorColor, PointRange, 32);

	// Draw lines showing the path of travel...
	INT	NumKeys = Position->GetNumKeys();
	INT	NumSubCurves = Position->GetNumSubCurves();

	FVector InitialPosition;
	FVector SamplePosition[2];

	for (INT i = 0; i < NumKeys; i++)
	{
		FLOAT X = Position->GetKeyOut(0, i);
		FLOAT Y = Position->GetKeyOut(1, i);
		FLOAT Z = Position->GetKeyOut(2, i);

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
		    PRI->DrawLine(SamplePosition[0], SamplePosition[1], ModuleEditorColor);
		}
	}
}

/*-----------------------------------------------------------------------------
	UParticleModuleCollision implementation.
-----------------------------------------------------------------------------*/

INT UParticleModuleCollision::RequiredBytes()
{
	// FVector	DampingFactor;
	// INT		MaxCollisions;
	return sizeof(FVector) + sizeof(INT);	
}

void UParticleModuleCollision::Spawn( FParticleEmitterInstance* Owner, INT Offset, FLOAT SpawnTime )
{
	SPAWN_INIT;
	PARTICLE_ELEMENT( FVector,	UsedDampingFactor );
	PARTICLE_ELEMENT( INT,		UsedMaxCollisions );
	UsedDampingFactor	= DampingFactor->GetValue( Owner->EmitterTime );
	UsedMaxCollisions	= MaxCollisions->GetValue( Owner->EmitterTime );
}

void UParticleModuleCollision::Update( FParticleEmitterInstance* Owner, INT Offset, FLOAT DeltaTime )
{
	AActor* Actor = Owner->Component->Owner;
	if( !Actor || Owner->Template->UseLocalSpace )
		return;

	ULevel* Level = Actor->GetLevel();
	
	BEGIN_UPDATE_LOOP;
		PARTICLE_ELEMENT( FVector,	UsedDampingFactor );
		PARTICLE_ELEMENT( INT,		UsedMaxCollisions );

		FCheckResult	Hit;
		// Location won't be calculated till after tick so we need to calculate an intermediate one here.
		FVector			Location	= Particle.Location + Particle.Velocity * DeltaTime;
		FVector			Direction	= (Location - Particle.OldLocation).SafeNormal();

		if( !Level->SingleLineCheck( Hit, Actor, Location + Direction * Particle.Size.Size(), Particle.OldLocation, TRACE_AllBlocking | TRACE_OnlyProjActor ) )
		{		
			if( UsedMaxCollisions-- > 0 )
			{
				FVector vOldVelocity = Particle.Velocity;

				// Reflect base velocity and apply damping factor.
				Particle.BaseVelocity	= Particle.BaseVelocity.MirrorByVector( Hit.Normal ) * UsedDampingFactor;

				// Reset the current velocity and manually adjust location to bounce off based on normal and time of collision.
				FVector vNewVelocity	= Direction.MirrorByVector( Hit.Normal ) * (Location - Particle.OldLocation).Size() * UsedDampingFactor;
				Particle.Velocity		= 0;
				Particle.Location	   += vNewVelocity * ( 1.f - Hit.Time );

				if (bApplyPhysics && Hit.Component)
				{
					FVector vImpulse;
					vImpulse = -(vNewVelocity - vOldVelocity) * ParticleMass->GetValue(Particle.RelativeTime);
					Hit.Component->AddImpulse(vImpulse, Hit.Location, Hit.BoneName);
				}

			}
			else
				KILL_CURRENT_PARTICLE;
		}
	END_UPDATE_LOOP;
}
IMPLEMENT_CLASS(UParticleModuleCollision);
