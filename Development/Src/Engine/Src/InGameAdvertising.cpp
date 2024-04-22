/*=============================================================================
	InGameAdvertising.cpp: Base implementation for ingame advertising management
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineGameEngineClasses.h"
#include "EnginePlatformInterfaceClasses.h"

IMPLEMENT_CLASS(UInGameAdManager);

/**
 * Sub functions for when there's no sub class implementation
 */
void UInGameAdManager::Init()
{
}
void UInGameAdManager::ShowBanner(UBOOL bShowOnBottomOfScreen)
{
}
void UInGameAdManager::HideBanner()
{
}
void UInGameAdManager::ForceCloseAd()
{
}

/**
 * Called by platform when the user clicks on the ad banner
 */
void UInGameAdManager::OnUserClickedBanner()
{
	// pause if we want to pause while ad is open
	if (bShouldPauseWhileAdOpen && GWorld->GetWorldInfo()->NetMode == NM_Standalone)
	{
		// pause the first player controller
		if (GEngine && GEngine->GamePlayers.Num() && GEngine->GamePlayers(0))
		{
			GEngine->GamePlayers(0)->Actor->ConsoleCommand(TEXT("PAUSE"));
		}
	}

	// tell script code there was a click
	FPlatformInterfaceDelegateResult Result(EC_EventParm);
	Result.bSuccessful = TRUE;
	CallDelegates(AMD_ClickedBanner, Result);
}


/**
 * Called by platform when an opened ad is closed
 */
void UInGameAdManager::OnUserClosedAd()
{
	// unpause if we want to pause while ad is open
	if (bShouldPauseWhileAdOpen && GWorld->GetWorldInfo()->NetMode == NM_Standalone)
	{
		// pause the first player controller
		if (GEngine && GEngine->GamePlayers.Num() && GEngine->GamePlayers(0))
		{
			GEngine->GamePlayers(0)->Actor->ConsoleCommand(TEXT("PAUSE"));
		}
	}

	// tell script code the user clicked an ad
	FPlatformInterfaceDelegateResult Result(EC_EventParm);
	Result.bSuccessful = TRUE;
	CallDelegates(AMD_UserClosedAd, Result);
}
