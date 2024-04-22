/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "ScenePrivate.h"

IMPLEMENT_CLASS(ULightEnvironmentComponent);

void ULightEnvironmentComponent::SetEnabled(UBOOL bNewEnabled)
{
	if (bNewEnabled != bEnabled) 
	{
		bEnabled = bNewEnabled;

		// Reattach the primitive components using this light environment with the updated state.
		for (INT ComponentIndex = 0; ComponentIndex < AffectedComponents.Num(); ComponentIndex++)
		{
			if (AffectedComponents(ComponentIndex) && AffectedComponents(ComponentIndex)->IsAttached())
			{
				AffectedComponents(ComponentIndex)->BeginDeferredReattach();
			}
		}

		if (IsAttached())
		{
			// Reattach the light environment so it will update it's internal state.
			BeginDeferredReattach();
		}
	}
}

UBOOL ULightEnvironmentComponent::IsEnabled() const
{
	return bEnabled;
}

void ULightEnvironmentComponent::AddAffectedComponent(UPrimitiveComponent* NewComponent)
{
	AffectedComponents.AddItem(NewComponent);
}

void ULightEnvironmentComponent::RemoveAffectedComponent(UPrimitiveComponent* OldComponent)
{
	// The order of primitive components using this light environment does not matter
	AffectedComponents.RemoveItemSwap(OldComponent);
}
