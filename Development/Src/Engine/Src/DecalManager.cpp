/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineDecalClasses.h"

IMPLEMENT_CLASS(ADecalManager);

/** @return whether dynamic decals are enabled */
UBOOL ADecalManager::AreDynamicDecalsEnabled()
{
	return GSystemSettings.bAllowDynamicDecals;
}

void ADecalManager::TickSpecial(FLOAT DeltaTime)
{
	Super::TickSpecial(DeltaTime);

	for (INT i = 0; i < ActiveDecals.Num(); i++)
	{
		FActiveDecalInfo& DecalInfo = ActiveDecals(i); //@warning: will be invalidated by the various Remove() calls below
		if (DecalInfo.Decal == NULL || DecalInfo.Decal->HasAnyFlags(RF_PendingKill))
		{
			ActiveDecals.Remove(i--);
		}
		else if (DecalInfo.Decal->DecalReceivers.Num() == 0)
		{
			// not projecting on anything, so no point in keeping it around
			eventDecalFinished(DecalInfo.Decal);
			ActiveDecals.Remove(i--);
		}
		else
		{
			// update lifetime and remove if it ran out
			DecalInfo.LifetimeRemaining -= DeltaTime;
			if (DecalInfo.LifetimeRemaining <= 0.f)
			{
				eventDecalFinished(DecalInfo.Decal);
				ActiveDecals.Remove(i--);
			}
		}
	}
}
