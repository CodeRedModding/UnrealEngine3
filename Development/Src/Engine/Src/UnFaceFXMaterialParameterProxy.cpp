/*=============================================================================
	UnFaceFXMaterialParameterProxy.cpp: FaceFX Face Graph node proxy to support
	animating Unreal material parameters.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if WITH_FACEFX

#include "UnFaceFXMaterialParameterProxy.h"
#include "EngineMaterialClasses.h"

//------------------------------------------------------------------------------
// FFaceFXMaterialParameterProxy.
//------------------------------------------------------------------------------

FFaceFXMaterialParameterProxy::FFaceFXMaterialParameterProxy()
	: SkeletalMeshComponent(NULL)
	, MaterialInstanceConstant(NULL)
	, MaterialSlotID(0)
	, ScalarParameterName(NAME_None)
{
}

FFaceFXMaterialParameterProxy::~FFaceFXMaterialParameterProxy()
{
}

void FFaceFXMaterialParameterProxy::Update( FLOAT Value )
{
	if( MaterialInstanceConstant )
	{
		MaterialInstanceConstant->SetScalarParameterValue(ScalarParameterName, Value);
	}
}

void FFaceFXMaterialParameterProxy::SetSkeletalMeshComponent( USkeletalMeshComponent* InSkeletalMeshComponent )
{
	SkeletalMeshComponent = InSkeletalMeshComponent;
	MaterialInstanceConstant = NULL;
	if( SkeletalMeshComponent )
	{
		UMaterialInterface* MaterialInterface = SkeletalMeshComponent->GetMaterial(MaterialSlotID);
		if( MaterialInterface && MaterialInterface->IsA(UMaterialInstanceConstant::StaticClass()) )
		{
			MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(MaterialInterface);
		}

		if( !MaterialInstanceConstant && SkeletalMeshComponent->SkeletalMesh )
		{
			if( MaterialSlotID < SkeletalMeshComponent->SkeletalMesh->Materials.Num() && 
				SkeletalMeshComponent->SkeletalMesh->Materials(MaterialSlotID) )
			{
				if( SkeletalMeshComponent->bDisableFaceFXMaterialInstanceCreation )
				{
					debugf(TEXT("FaceFX: WARNING Unable to create MaterialInstanceConstant because bDisableFaceFXMaterialInstanceCreation is true!"));
				}
				else
				{
					UMaterialInstanceConstant* NewMaterialInstanceConstant = CastChecked<UMaterialInstanceConstant>( UObject::StaticConstructObject(UMaterialInstanceConstant::StaticClass(), InSkeletalMeshComponent) );
					NewMaterialInstanceConstant->SetParent(SkeletalMeshComponent->GetMaterial(MaterialSlotID));
					SkeletalMeshComponent->SetMaterial(MaterialSlotID, NewMaterialInstanceConstant);
					MaterialInstanceConstant = NewMaterialInstanceConstant;
				}
			}
		}
	}
}

UBOOL FFaceFXMaterialParameterProxy::Link( INT InMaterialSlotID, const FName& InScalarParameterName )
{
	MaterialSlotID = InMaterialSlotID;
	ScalarParameterName = InScalarParameterName;
	return TRUE;
}

#endif // WITH_FACEFX
