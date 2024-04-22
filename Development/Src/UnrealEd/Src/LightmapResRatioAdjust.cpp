/*================================================================================
	LightmapResRatioAdjust.cpp: Lightmap Resolution Ratio Adjustment helper
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEd.h"
#include "LightmapResRatioAdjust.h"
#include "EngineFluidClasses.h"
#include "LevelBrowser.h"
#include "SurfaceIterators.h"

/**
 *	LightmapResRatioAdjust settings
 */
/** Static: Global lightmap resolution ratio adjust settings */
FLightmapResRatioAdjustSettings FLightmapResRatioAdjustSettings::LightmapResRatioAdjustSettings;

UBOOL FLightmapResRatioAdjustSettings::ApplyRatioAdjustment()
{
	FLightmapResRatioAdjustSettings& Settings = Get();

	if ((Settings.bStaticMeshes == FALSE) &&
		(Settings.bBSPSurfaces == FALSE) && 
		(Settings.bTerrains == FALSE) &&
		(Settings.bFluidSurfaces == FALSE))
	{
		// No primitive type is selected...
		appMsgf(AMT_OK, *LocalizeUnrealEd("LMRatioAdjust_NoPrimitivesSelected"));
		return FALSE;
	}

	UBOOL bRebuildGeometry = FALSE;
	UBOOL bRefreshViewport = FALSE;

	// Collect the levels to process
	//@todo. This code is done in a few places for lighting related changes...
	// It should be moved to a centralized place to remove duplication.
	TArray<ULevel*> Levels;
	switch (Settings.LevelOptions)
	{
	case ELightmapResRatioAdjustLevels::Current:
		{
			check(GWorld);
			Levels.AddItem(GWorld->CurrentLevel);
		}
		break;
	case ELightmapResRatioAdjustLevels::Selected:
		{
			WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>(TEXT("LevelBrowser"));
			if (LevelBrowser != NULL)
			{
				// Assemble an ignore list from the levels that are currently selected in the level browser.
				for (WxLevelBrowser::TSelectedLevelItemIterator It(LevelBrowser->SelectedLevelItemIterator()); It; ++It)
				{
					if( It->IsLevel() )
					{
						ULevel* Level = It->GetLevel();
						if (Level)
						{
							Levels.AddUniqueItem(Level);
						}
					}
				}

				if (Levels.Num() == 0)
				{
					// Fall to the current level...
					check(GWorld);
					Levels.AddUniqueItem(GWorld->CurrentLevel);
				}
			}
		}
		break;
	case ELightmapResRatioAdjustLevels::AllLoaded:
		{
			if (GWorld != NULL)
			{
				// Add main level.
				Levels.AddUniqueItem(GWorld->PersistentLevel);

				// Add secondary levels.
				AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
				for (INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); ++LevelIndex)
				{
					ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
					if (StreamingLevel && StreamingLevel->LoadedLevel)
					{
						Levels.AddUniqueItem(StreamingLevel->LoadedLevel);
					}
				}
			}
		}
		break;
	}

	if (Levels.Num() == 0)
	{
		// No levels are selected...
		appMsgf(AMT_OK, *LocalizeUnrealEd("LMRatioAdjust_NoLevelsToProcess"));
		return FALSE;
	}

	// Find all the static meshes, terrains and fluids for potential adjustment
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	TArray<ATerrain*> Terrains;
	TArray<UFluidSurfaceComponent*> FluidComponents;

	for (TObjectIterator<UObject> ObjIt;  ObjIt; ++ObjIt)
	{
		UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(*ObjIt);
		ATerrain* Terrain = Cast<ATerrain>(*ObjIt);
		UFluidSurfaceComponent* FluidComp = Cast<UFluidSurfaceComponent>(*ObjIt);

		if (StaticMeshComp && (Settings.bStaticMeshes == TRUE))
		{
			if (Settings.bSelectedObjectsOnly == TRUE)
			{
				AActor* OwnerActor = StaticMeshComp->GetOwner();
				if (OwnerActor && OwnerActor->IsSelected() == TRUE)
				{
					StaticMeshComponents.AddUniqueItem(StaticMeshComp);
				}
			}
			else
			{
				StaticMeshComponents.AddUniqueItem(StaticMeshComp);
			}
		}
		if (Terrain && (Settings.bTerrains == TRUE))
		{
			if (Settings.bSelectedObjectsOnly == TRUE)
			{
				if (Terrain->IsSelected() == TRUE)
				{
					Terrains.AddUniqueItem(Terrain);
				}
			}
			else
			{
				Terrains.AddUniqueItem(Terrain);
			}
		}
		if (FluidComp && (Settings.bFluidSurfaces == TRUE))
		{
			if (Settings.bSelectedObjectsOnly == TRUE)
			{
				AActor* OwnerActor = FluidComp->GetOwner();
				if (OwnerActor && OwnerActor->IsSelected() == TRUE)
				{
					FluidComponents.AddUniqueItem(FluidComp);
				}
			}
			else
			{
				FluidComponents.AddUniqueItem(FluidComp);
			}
		}
	}

	ULevel* OrigCurrentLevel = GWorld->CurrentLevel;
	for (INT LevelIdx = 0; LevelIdx < Levels.Num(); LevelIdx++)
	{
		GWorld->CurrentLevel = Levels(LevelIdx);

		for (INT StaticMeshIdx = 0; StaticMeshIdx < StaticMeshComponents.Num(); StaticMeshIdx++)
		{
			UStaticMeshComponent* SMComp = StaticMeshComponents(StaticMeshIdx);
			if (SMComp && SMComp->IsIn(GWorld->CurrentLevel) == TRUE)
			{
				// Process it!
				INT CurrentResolution = SMComp->GetStaticLightMapResolution();
				UBOOL bConvertIt = TRUE;
				if (((SMComp->bOverrideLightMapRes == TRUE) && (CurrentResolution == 0)) ||
					((SMComp->bOverrideLightMapRes == FALSE) && (SMComp->StaticMesh != NULL) && (SMComp->StaticMesh->LightMapResolution == 0)))
				{
					// Don't convert vertex mapped objects!
					bConvertIt = FALSE;
				}
				else if ((Settings.Ratio >= 1.0f) && (CurrentResolution >= Settings.Max_StaticMeshes))
				{
					bConvertIt = FALSE;
				}
				else
				if ((Settings.Ratio < 1.0f) && (CurrentResolution <= Settings.Min_StaticMeshes))
				{
					bConvertIt = FALSE;
				}
				if (bConvertIt)
				{
					SMComp->Modify();
					FLOAT AdjustedResolution = (FLOAT)CurrentResolution * (Settings.Ratio);
					INT NewResolution = appTrunc(AdjustedResolution);
					NewResolution = Max(NewResolution + 3 & ~3,4);
					SMComp->SetStaticLightingMapping(TRUE, NewResolution);
					SMComp->InvalidateLightingCache();
					SMComp->BeginDeferredReattach();
					bRefreshViewport = TRUE;
				}
			}
		}
		for (INT TerrainIdx = 0; TerrainIdx < Terrains.Num(); TerrainIdx++)
		{
			ATerrain* Terrain = Terrains(TerrainIdx);
			if (Terrain && Terrain->IsIn(GWorld->CurrentLevel) == TRUE)
			{
				// Process it!
				INT CurrentResolution = Terrain->StaticLightingResolution;
				UBOOL bConvertIt = TRUE;
				if ((Settings.Ratio >= 1.0f) && (CurrentResolution >= Settings.Max_Terrains))
				{
					bConvertIt = FALSE;
				}
				else
				if ((Settings.Ratio < 1.0f) && (CurrentResolution <= Settings.Min_Terrains))
				{
					bConvertIt = FALSE;
				}
				if (bConvertIt)
				{
					Terrain->Modify();
					FLOAT AdjustedResolution = (FLOAT)CurrentResolution * (Settings.Ratio);
					INT NewResolution = appTrunc(AdjustedResolution);
 					NewResolution = Max(1, NewResolution);
					Terrain->StaticLightingResolution = NewResolution;
					Terrain->bIsOverridingLightResolution = TRUE;
					Terrain->InvalidateLightingCache();
					Terrain->ConditionalUpdateComponents(FALSE);
					bRefreshViewport = TRUE;
				}
			}
		}
		for (INT FluidIdx = 0; FluidIdx < FluidComponents.Num(); FluidIdx++)
		{
			UFluidSurfaceComponent* FluidComp = FluidComponents(FluidIdx);
			if (FluidComp && FluidComp->IsIn(GWorld->CurrentLevel) == TRUE)
			{
				// Process it!
				INT CurrentResolution = FluidComp->LightMapResolution;
				UBOOL bConvertIt = TRUE;
				if ((Settings.Ratio >= 1.0f) && (CurrentResolution >= Settings.Max_FluidSurfaces))
				{
					bConvertIt = FALSE;
				}
				else
				if ((Settings.Ratio < 1.0f) && (CurrentResolution <= Settings.Min_FluidSurfaces))
				{
					bConvertIt = FALSE;
				}
				if (bConvertIt)
				{
					FluidComp->Modify();
					FLOAT AdjustedResolution = (FLOAT)CurrentResolution * (Settings.Ratio);
					INT NewResolution = appTrunc(AdjustedResolution);
 					NewResolution = Max(1, NewResolution);
					FluidComp->LightMapResolution = NewResolution;
					FluidComp->InvalidateLightingCache();
					FluidComp->BeginDeferredReattach();
					bRefreshViewport = TRUE;
				}
			}
		}

		// Update all surfaces in this level...
		if (Settings.bBSPSurfaces == TRUE)
		{
			for (TSurfaceIterator<FCurrentLevelSurfaceLevelFilter> It; It; ++It)
			{
				UModel* Model = It.GetModel();
				UBOOL bConvertIt = TRUE;
				const INT SurfaceIndex = It.GetSurfaceIndex();
				FBspSurf& Surf = Model->Surfs(SurfaceIndex);
				if (Settings.bSelectedObjectsOnly == TRUE)
				{
					if ((Surf.PolyFlags & PF_Selected) == 0)
					{
						bConvertIt = FALSE;
					}
				}

				if (bConvertIt == TRUE)
				{
					// Process it!
					INT CurrentResolution = Surf.ShadowMapScale;
					FLOAT Scalar = 1.0f / Settings.Ratio;
					if ((Scalar < 1.0f) && (CurrentResolution <= Settings.Min_BSPSurfaces))
					{
						bConvertIt = FALSE;
					}
					else if ((Scalar >= 1.0f) && (CurrentResolution >= Settings.Max_BSPSurfaces))
					{
						bConvertIt = FALSE;
					}

					if (bConvertIt == TRUE)
					{
						Model->ModifySurf( SurfaceIndex, TRUE );
						INT NewResolution = appTrunc(Surf.ShadowMapScale * Scalar);
						NewResolution = Max(NewResolution + 3 & ~3,4);
						Surf.ShadowMapScale = (FLOAT)NewResolution;
						if (Surf.Actor != NULL)
						{
							Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly).ShadowMapScale = Surf.ShadowMapScale;
						}
						bRefreshViewport = TRUE;
						bRebuildGeometry = TRUE;
					}
				}
			}
		}
	}
	GWorld->CurrentLevel = OrigCurrentLevel;

	if (bRebuildGeometry == TRUE)
	{
		GUnrealEd->Exec( TEXT("MAP REBUILD") );
	}
	if (bRefreshViewport == TRUE)
	{
		GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
		GCallbackEvent->Send(CALLBACK_RefreshPropertyWindows);
	}

	return TRUE;
}
