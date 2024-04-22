/*=============================================================================
	SpeedTreeActorFactory.cpp: SpeedTree actor factory implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "SpeedTree.h"

IMPLEMENT_CLASS(USpeedTreeActorFactory);

AActor* USpeedTreeActorFactory::CreateActor(const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData)
{
	if( !SpeedTree )
	{
		return NULL;
	}

	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	
	ASpeedTreeActor* NewSpeedTreeActor = Cast<ASpeedTreeActor>(NewActor);
	if( NewSpeedTreeActor )
	{
		NewSpeedTreeActor->SpeedTreeComponent->SpeedTree = SpeedTree;
	}

	return NewActor;
}


UBOOL USpeedTreeActorFactory::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly ) 
{ 
	if( SpeedTree )
	{
		return TRUE;
	}
	else
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoSpeedTree");
		return FALSE;
	}
}


void USpeedTreeActorFactory::AutoFillFields(USelection* Selection)
{
	SpeedTree = Selection->GetTop<USpeedTree>();
}


FString USpeedTreeActorFactory::GetMenuName()
{
	if( SpeedTree )
	{
		return FString::Printf(TEXT("%s: %s"), *MenuName, *SpeedTree->GetPathName());
	}
	else
	{
		return MenuName;
	}
}

