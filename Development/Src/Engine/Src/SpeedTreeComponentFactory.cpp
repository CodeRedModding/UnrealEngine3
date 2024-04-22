/*=============================================================================
	SpeedTreeComponentFactory.cpp: SpeedTreeComponentFactory implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "SpeedTree.h"

IMPLEMENT_CLASS(USpeedTreeComponentFactory);

UPrimitiveComponent* USpeedTreeComponentFactory::CreatePrimitiveComponent( UObject* InOuter)
{
#if WITH_SPEEDTREE

	// Create a new SpeedTreeComponent using the factory's template.
	USpeedTreeComponent* NewComponent = ConstructObject<USpeedTreeComponent>(
		USpeedTreeComponent::StaticClass( ),
		InOuter,
		NAME_None,
		0,
		SpeedTreeComponent
		);

	// Reset the new SpeedTreeComponent's archetype to its class's default archetype, so it doesn't continue to reference the factory template.
	NewComponent->SetArchetype(NewComponent->GetClass()->GetDefaultObject());

	return NewComponent;
#else
	return NULL;
#endif
}

