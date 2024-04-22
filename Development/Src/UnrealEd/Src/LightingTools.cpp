/*================================================================================
	LightingTools.cpp: Lighting Tools helper
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEd.h"
#include "LightingTools.h"
#include "../../Engine/Src/DynamicLightEnvironmentComponent.h"

/**
 *	LightingTools settings
 */
/** Static: Global lighting tools adjust settings */
FLightingToolsSettings FLightingToolsSettings::LightingToolsSettings;

void FLightingToolsSettings::Init()
{
	FLightingToolsSettings& Settings = Get();
	for (INT ViewIndex = 0; ViewIndex < GEditor->ViewportClients.Num(); ViewIndex++)
	{
		if (GEditor->ViewportClients(ViewIndex)->ViewportType == LVT_Perspective)
		{
			Settings.bSavedShowSelection = (GEditor->ViewportClients(ViewIndex)->ShowFlags & SHOW_Selection) != 0;
			GEditor->ViewportClients(ViewIndex)->ShowFlags &= ~SHOW_Selection;
			break;
		}
	}
	ApplyToggle();
}

UBOOL FLightingToolsSettings::ApplyToggle()
{
	FLightingToolsSettings& Settings = Get();

	extern FLightEnvironmentDebugInfo GLightEnvironmentDebugInfo;
	GLightEnvironmentDebugInfo.bShowBounds = Settings.bShowLightingBounds;
	GLightEnvironmentDebugInfo.bShowVisibility = Settings.bShowShadowTraces;
	GLightEnvironmentDebugInfo.bShowDirectLightingOnly = Settings.bShowDirectOnly;
	GLightEnvironmentDebugInfo.bShowIndirectLightingOnly = Settings.bShowIndirectOnly;
	GLightEnvironmentDebugInfo.bShowVolumeInterpolation = Settings.bShowIndirectSamples;
	GLightEnvironmentDebugInfo.bShowDominantLightTransition = Settings.bShowAffectingDominantLights;

	{
		TComponentReattachContext<UDynamicLightEnvironmentComponent> LightEnvReattach;
	}
	GCallbackEvent->Send(CALLBACK_RedrawAllViewports);

	return TRUE;
}

void FLightingToolsSettings::Reset()
{
	extern FLightEnvironmentDebugInfo GLightEnvironmentDebugInfo;
	GLightEnvironmentDebugInfo = FLightEnvironmentDebugInfo();

	{
		TComponentReattachContext<UDynamicLightEnvironmentComponent> LightEnvReattach;
	}
	GCallbackEvent->Send(CALLBACK_RedrawAllViewports);

	FLightingToolsSettings& Settings = Get();
	if (Settings.bSavedShowSelection)
	{
		for (INT ViewIndex = 0; ViewIndex < GEditor->ViewportClients.Num(); ViewIndex++)
		{
			if (GEditor->ViewportClients(ViewIndex)->ViewportType == LVT_Perspective)
			{
				GEditor->ViewportClients(ViewIndex)->ShowFlags |= SHOW_Selection;
				break;
			}
		}
	}
}
