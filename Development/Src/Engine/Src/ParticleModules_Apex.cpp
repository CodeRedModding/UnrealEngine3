
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"
#include "ParticleEmitterInstances_Apex.h"

#if WITH_NOVODEX
	#include "UnNovodexSupport.h"
#endif		// WITH_NOVODEX

/////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS(UParticleModuleTypeDataApex);

FParticleEmitterInstance *UParticleModuleTypeDataApex::CreateInstance(UParticleEmitter *InEmitterParent, UParticleSystemComponent *InComponent)
{
#if WITH_APEX_PARTICLES
	SetToSensibleDefaults(InEmitterParent);

	if(ApexIOFX && !ApexIOFX->MApexAsset)
	{
		ApexIOFX->MarkPackageDirty();
		ApexIOFX->CreateDefaultAssetType(AAT_IOFX,NULL);
	}
	if(ApexEmitter && !ApexEmitter->MApexAsset)
	{
		ApexEmitter->MarkPackageDirty();
		ApexEmitter->CreateDefaultAssetType(AAT_APEX_EMITTER,ApexIOFX);
	}

	FParticleEmitterInstance *Instance = new FParticleApexEmitterInstance(*this);
	check(Instance);
	Instance->InitParameters(InEmitterParent, InComponent);
	
	return Instance;
#else
	return 0;
#endif
}

void UParticleModuleTypeDataApex::SetToSensibleDefaults(UParticleEmitter *Owner)
{
	Super::SetToSensibleDefaults(Owner);
	
	// TODO...
}

void UParticleModuleTypeDataApex::PreEditChange(UProperty *PropertyAboutToChange)
{
	// TODO...
	
	Super::PreEditChange(PropertyAboutToChange);
}

void UParticleModuleTypeDataApex::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// TODO...
}

void UParticleModuleTypeDataApex::FinishDestroy()
{
	Super::FinishDestroy();
}

void UParticleModuleTypeDataApex::ConditionalInitialize(void)
{
#if WITH_APEX_PARTICLES
	if(ApexIOFX && !ApexIOFX->MApexAsset)
	{
		ApexIOFX->MarkPackageDirty();
		ApexIOFX->CreateDefaultAssetType(AAT_IOFX,NULL);
	}
	if(ApexEmitter && !ApexEmitter->MApexAsset)
	{
		ApexEmitter->MarkPackageDirty();
		ApexEmitter->CreateDefaultAssetType(AAT_APEX_EMITTER,ApexIOFX);
	}
#endif
}
