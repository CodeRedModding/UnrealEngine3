/*=============================================================================
	ParticleDataManager.cpp: Particle dynamic data manager implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

/*-----------------------------------------------------------------------------
	ParticleDataManager
-----------------------------------------------------------------------------*/
//
typedef TMap<UParticleSystemComponent*, UBOOL>::TIterator TParticleDataIterator;

#if STATS
DWORD FParticleDataManager::DynamicPSysCompCount = 0;
DWORD FParticleDataManager::DynamicPSysCompMem = 0;
DWORD FParticleDataManager::DynamicEmitterCount = 0;
DWORD FParticleDataManager::DynamicEmitterMem = 0;
DWORD FParticleDataManager::TotalGTParticleData = 0;
DWORD FParticleDataManager::TotalRTParticleData = 0;
DWORD FParticleDataManager::DynamicSpriteCount = 0;
DWORD FParticleDataManager::DynamicSubUVCount = 0;
DWORD FParticleDataManager::DynamicMeshCount = 0;
DWORD FParticleDataManager::DynamicBeamCount = 0;
DWORD FParticleDataManager::DynamicTrailCount = 0;
DWORD FParticleDataManager::DynamicRibbonCount = 0;
DWORD FParticleDataManager::DynamicAnimTrailCount = 0;
DWORD FParticleDataManager::DynamicSpriteGTMem = 0;
DWORD FParticleDataManager::DynamicSubUVGTMem = 0;
DWORD FParticleDataManager::DynamicMeshGTMem = 0;
DWORD FParticleDataManager::DynamicBeamGTMem = 0;
DWORD FParticleDataManager::DynamicTrailGTMem = 0;
DWORD FParticleDataManager::DynamicRibbonGTMem = 0;
DWORD FParticleDataManager::DynamicAnimTrailGTMem = 0;
DWORD FParticleDataManager::DynamicUntrackedGTMem = 0;
DWORD FParticleDataManager::DynamicPSysCompCount_MAX = 0;
DWORD FParticleDataManager::DynamicPSysCompMem_MAX = 0;
DWORD FParticleDataManager::DynamicEmitterCount_MAX = 0;
DWORD FParticleDataManager::DynamicEmitterMem_MAX = 0;
DWORD FParticleDataManager::DynamicEmitterGTMem_Waste_MAX = 0;
DWORD FParticleDataManager::DynamicEmitterGTMem_Largest_MAX = 0;
DWORD FParticleDataManager::TotalGTParticleData_MAX = 0;
DWORD FParticleDataManager::TotalRTParticleData_MAX = 0;
DWORD FParticleDataManager::LargestRTParticleData_MAX = 0;
DWORD FParticleDataManager::DynamicSpriteCount_MAX = 0;
DWORD FParticleDataManager::DynamicSubUVCount_MAX = 0;
DWORD FParticleDataManager::DynamicMeshCount_MAX = 0;
DWORD FParticleDataManager::DynamicBeamCount_MAX = 0;
DWORD FParticleDataManager::DynamicTrailCount_MAX = 0;
DWORD FParticleDataManager::DynamicRibbonCount_MAX = 0;
DWORD FParticleDataManager::DynamicAnimTrailCount_MAX = 0;
DWORD FParticleDataManager::DynamicSpriteGTMem_MAX = 0;
DWORD FParticleDataManager::DynamicSubUVGTMem_MAX = 0;
DWORD FParticleDataManager::DynamicMeshGTMem_MAX = 0;
DWORD FParticleDataManager::DynamicBeamGTMem_MAX = 0;
DWORD FParticleDataManager::DynamicTrailGTMem_MAX = 0;
DWORD FParticleDataManager::DynamicRibbonGTMem_MAX = 0;
DWORD FParticleDataManager::DynamicAnimTrailGTMem_MAX = 0;
DWORD FParticleDataManager::DynamicUntrackedGTMem_MAX = 0;

void FParticleDataManager::ResetParticleMemoryMaxValues()
{
	DynamicPSysCompCount_MAX = 0;
	DynamicPSysCompMem_MAX = 0;
	DynamicEmitterCount_MAX = 0;
	DynamicEmitterMem_MAX = 0;
	DynamicEmitterGTMem_Waste_MAX = 0;
	DynamicEmitterGTMem_Largest_MAX = 0;
	TotalGTParticleData_MAX = 0;
	TotalRTParticleData_MAX = 0;
	LargestRTParticleData_MAX = 0;
	DynamicSpriteCount_MAX = 0;
	DynamicSubUVCount_MAX = 0;
	DynamicMeshCount_MAX = 0;
	DynamicBeamCount_MAX = 0;
	DynamicTrailCount_MAX = 0;
	DynamicRibbonCount_MAX = 0;
	DynamicAnimTrailCount_MAX = 0;
	DynamicSpriteGTMem_MAX = 0;
	DynamicSubUVGTMem_MAX = 0;
	DynamicMeshGTMem_MAX = 0;
	DynamicBeamGTMem_MAX = 0;
	DynamicTrailGTMem_MAX = 0;
	DynamicRibbonGTMem_MAX = 0;
	DynamicAnimTrailGTMem_MAX = 0;
	DynamicUntrackedGTMem_MAX = 0;
}

void FParticleDataManager::DumpParticleMemoryStats(FOutputDevice& Ar)
{
	// 
	Ar.Logf(TEXT("Particle Dynamic Memory Stats"));

	Ar.Logf(TEXT("Type,Count,MaxCount,Mem(Bytes),MaxMem(Bytes),GTMem(Bytes),GTMemMax(Bytes)"));
	Ar.Logf(TEXT("Total PSysComponents,%d,%d,%d,%d,%d,%d"), 
		DynamicPSysCompCount, DynamicPSysCompCount_MAX, 
		DynamicPSysCompMem,DynamicPSysCompMem_MAX,
		0,0);
	Ar.Logf(TEXT("Total DynamicEmitters,%d,%d,%d,%d,%d,%d"), 
		DynamicEmitterCount, DynamicEmitterCount_MAX, 
		DynamicEmitterMem, DynamicEmitterMem_MAX,
		TotalGTParticleData, TotalGTParticleData_MAX);
	Ar.Logf(TEXT("Sprite,%d,%d,%d,%d,%d,%d"), 
		DynamicSpriteCount, DynamicSpriteCount_MAX, 
		DynamicSpriteCount * sizeof(FDynamicSpriteEmitterData), 
		DynamicSpriteCount_MAX * sizeof(FDynamicSpriteEmitterData), 
		DynamicSpriteGTMem, DynamicSpriteGTMem_MAX);
	Ar.Logf(TEXT("SubUV,%d,%d,%d,%d,%d,%d"), 
		DynamicSubUVCount, DynamicSubUVCount_MAX, 
		DynamicSubUVCount * sizeof(FDynamicSubUVEmitterData), 
		DynamicSubUVCount_MAX * sizeof(FDynamicSubUVEmitterData), 
		DynamicSubUVGTMem, DynamicSubUVGTMem_MAX);
	Ar.Logf(TEXT("Mesh,%d,%d,%d,%d,%d,%d"), 
		DynamicMeshCount, DynamicMeshCount_MAX, 
		DynamicMeshCount * sizeof(FDynamicMeshEmitterData), 
		DynamicMeshCount_MAX * sizeof(FDynamicMeshEmitterData), 
		DynamicMeshGTMem, DynamicMeshGTMem_MAX);
	Ar.Logf(TEXT("Beam,%d,%d,%d,%d,%d,%d"), 
		DynamicBeamCount, DynamicBeamCount_MAX, 
		DynamicBeamCount * sizeof(FDynamicBeam2EmitterData), 
		DynamicBeamCount_MAX * sizeof(FDynamicBeam2EmitterData), 
		DynamicBeamGTMem, DynamicBeamGTMem_MAX);
	Ar.Logf(TEXT("Trail,%d,%d,%d,%d,%d,%d"), 
		DynamicTrailCount, DynamicTrailCount_MAX, 
		DynamicTrailCount * sizeof(FDynamicTrail2EmitterData), 
		DynamicTrailCount_MAX * sizeof(FDynamicTrail2EmitterData), 
		DynamicTrailGTMem, DynamicTrailGTMem_MAX);
	Ar.Logf(TEXT("Ribbon,%d,%d,%d,%d,%d,%d"), 
		DynamicRibbonCount, DynamicRibbonCount_MAX, 
		DynamicRibbonCount * sizeof(FDynamicRibbonEmitterData), 
		DynamicRibbonCount_MAX * sizeof(FDynamicRibbonEmitterData), 
		DynamicRibbonGTMem, DynamicRibbonGTMem_MAX);
	Ar.Logf(TEXT("AnimTrail,%d,%d,%d,%d,%d,%d"), 
		DynamicAnimTrailCount, DynamicAnimTrailCount_MAX, 
		DynamicAnimTrailCount * sizeof(FDynamicAnimTrailEmitterData), 
		DynamicAnimTrailCount_MAX * sizeof(FDynamicAnimTrailEmitterData), 
		DynamicAnimTrailGTMem, DynamicAnimTrailGTMem_MAX);
	Ar.Logf(TEXT("Untracked,%d,%d,%d,%d,%d,%d"), 0, 0, 0, 0, DynamicUntrackedGTMem, DynamicUntrackedGTMem_MAX);

	Ar.Logf(TEXT("ParticleData,Total(Bytes),Max(Bytes)"));
	Ar.Logf(TEXT("GameThread,%d,%d"), TotalGTParticleData, TotalGTParticleData_MAX);
	Ar.Logf(TEXT("RenderThread,%d,%d"), TotalRTParticleData, TotalRTParticleData_MAX);

	Ar.Logf(TEXT("Max wasted GT,%d"), DynamicEmitterGTMem_Waste_MAX);
	Ar.Logf(TEXT("Largest single GT allocation,%d"), DynamicEmitterGTMem_Largest_MAX);
	Ar.Logf(TEXT("Largest single RT allocation,%d"), LargestRTParticleData_MAX);

	GParticleVertexFactoryPool.DumpInfo(Ar);
	GParticleOrderPool.DumpInfo(Ar);
}
#endif

/**
 *	Update the dynamic data for all particle system components
 */
void FParticleDataManager::UpdateDynamicData()
{
	for (TParticleDataIterator It(PSysComponents); It; ++It)
	{
		UParticleSystemComponent* PSysComp = (UParticleSystemComponent*)(It.Key());
		if (PSysComp)
		{
			FParticleSystemSceneProxy* SceneProxy = (FParticleSystemSceneProxy*)Scene_GetProxyFromInfo(PSysComp->SceneInfo);
			if (SceneProxy != NULL)
			{
				if (PSysComp->bRecacheViewRelevance == TRUE)
				{
					PSysComp->UpdateViewRelevance(SceneProxy);
				}

				// check to see if this PSC is active.  When you attach a PSC it gets added to the DataManager
				// even if it might be bIsActive = FALSE  (e.g. attach and later in the frame activate it)
				// or also for PSCs that are attached to a SkelComp which is being attached and reattached but the PSC itself is not active!
				if( PSysComp->bIsActive )
				{
					PSysComp->UpdateDynamicData(SceneProxy);
				}
				else
				{
					// so if we just were deactivated we want to update the renderer with NULL so the renderer will clear out the data there and not have outdated info which may/will cause a crash
					if ( (PSysComp->bWasDeactivated || PSysComp->bWasCompleted) && (PSysComp->SceneInfo) )
					{
						SceneProxy->UpdateData(NULL);
					}
				}
			}
		}
	}

#if STATS
	{
		// Log/setup/etc.
		DynamicPSysCompCount = GET_DWORD_STAT(STAT_DynamicPSysCompCount);
		DynamicPSysCompMem = GET_DWORD_STAT(STAT_DynamicPSysCompMem);
		DynamicEmitterCount = GET_DWORD_STAT(STAT_DynamicEmitterCount);
		DynamicEmitterMem = GET_DWORD_STAT(STAT_DynamicEmitterMem);
		TotalGTParticleData = GET_DWORD_STAT(STAT_GTParticleData);
		TotalRTParticleData = GET_DWORD_STAT(STAT_RTParticleData);
		DWORD LargestRTParticleData = GET_DWORD_STAT(STAT_RTParticleData_Largest);
		DynamicSpriteCount = GET_DWORD_STAT(STAT_DynamicSpriteCount);
		DynamicSubUVCount = GET_DWORD_STAT(STAT_DynamicSubUVCount);
		DynamicMeshCount = GET_DWORD_STAT(STAT_DynamicMeshCount);
		DynamicBeamCount = GET_DWORD_STAT(STAT_DynamicBeamCount);
		DynamicTrailCount = GET_DWORD_STAT(STAT_DynamicTrailCount);
		DynamicRibbonCount = GET_DWORD_STAT(STAT_DynamicRibbonCount);
		DynamicAnimTrailCount = GET_DWORD_STAT(STAT_DynamicAnimTrailCount);

		DynamicSpriteGTMem = GET_DWORD_STAT(STAT_DynamicSpriteGTMem);
		DynamicSubUVGTMem = GET_DWORD_STAT(STAT_DynamicSubUVGTMem);
		DynamicMeshGTMem = GET_DWORD_STAT(STAT_DynamicMeshGTMem);
		DynamicBeamGTMem = GET_DWORD_STAT(STAT_DynamicBeamGTMem);
		DynamicTrailGTMem = GET_DWORD_STAT(STAT_DynamicTrailGTMem);
		DynamicRibbonGTMem = GET_DWORD_STAT(STAT_DynamicRibbonGTMem);
		DynamicAnimTrailGTMem = GET_DWORD_STAT(STAT_DynamicAnimTrailGTMem);
		DynamicUntrackedGTMem = GET_DWORD_STAT(STAT_DynamicUntrackedGTMem);

		DWORD DynamicEmitterGTMem_Waste = GET_DWORD_STAT(STAT_DynamicEmitterGTMem_Waste);
		DWORD DynamicEmitterGTMem_Largest = GET_DWORD_STAT(STAT_DynamicEmitterGTMem_Largest);
		DynamicEmitterGTMem_Waste_MAX = Max<DWORD>(DynamicEmitterGTMem_Waste_MAX, DynamicEmitterGTMem_Waste);
		DynamicEmitterGTMem_Largest_MAX = Max<DWORD>(DynamicEmitterGTMem_Largest_MAX, DynamicEmitterGTMem_Largest);

		DynamicPSysCompCount_MAX = Max<DWORD>(DynamicPSysCompCount_MAX, DynamicPSysCompCount);
		DynamicPSysCompMem_MAX = Max<DWORD>(DynamicPSysCompMem_MAX, DynamicPSysCompMem);
		DynamicEmitterCount_MAX = Max<DWORD>(DynamicEmitterCount_MAX, DynamicEmitterCount);
		DynamicEmitterMem_MAX = Max<DWORD>(DynamicEmitterMem_MAX, DynamicEmitterMem);
		TotalGTParticleData_MAX = Max<DWORD>(TotalGTParticleData_MAX, TotalGTParticleData);
		TotalRTParticleData_MAX = Max<DWORD>(TotalRTParticleData_MAX, TotalRTParticleData);
		LargestRTParticleData_MAX = Max<DWORD>(LargestRTParticleData_MAX, LargestRTParticleData);

		DynamicSpriteCount_MAX = Max<DWORD>(DynamicSpriteCount_MAX, DynamicSpriteCount);
		DynamicSubUVCount_MAX = Max<DWORD>(DynamicSubUVCount_MAX, DynamicSubUVCount);
		DynamicMeshCount_MAX = Max<DWORD>(DynamicMeshCount_MAX, DynamicMeshCount);
		DynamicBeamCount_MAX = Max<DWORD>(DynamicBeamCount_MAX, DynamicBeamCount);
		DynamicTrailCount_MAX = Max<DWORD>(DynamicTrailCount_MAX, DynamicTrailCount);
		DynamicRibbonCount_MAX = Max<DWORD>(DynamicRibbonCount_MAX, DynamicRibbonCount);
		DynamicAnimTrailCount_MAX = Max<DWORD>(DynamicAnimTrailCount_MAX, DynamicAnimTrailCount);

		DynamicSpriteGTMem_MAX = Max<DWORD>(DynamicSpriteGTMem_MAX, DynamicSpriteGTMem);
		DynamicSubUVGTMem_MAX = Max<DWORD>(DynamicSubUVGTMem_MAX, DynamicSubUVGTMem);
		DynamicMeshGTMem_MAX = Max<DWORD>(DynamicMeshGTMem_MAX, DynamicMeshGTMem);
		DynamicBeamGTMem_MAX = Max<DWORD>(DynamicBeamGTMem_MAX, DynamicBeamGTMem);
		DynamicTrailGTMem_MAX = Max<DWORD>(DynamicTrailGTMem_MAX, DynamicTrailGTMem);
		DynamicRibbonGTMem_MAX = Max<DWORD>(DynamicRibbonGTMem_MAX, DynamicRibbonGTMem);
		DynamicAnimTrailGTMem_MAX = Max<DWORD>(DynamicAnimTrailGTMem_MAX, DynamicAnimTrailGTMem);
		DynamicUntrackedGTMem_MAX = Max<DWORD>(DynamicUntrackedGTMem_MAX, DynamicUntrackedGTMem);

		GStatManager.SetStatValue(STAT_DynamicPSysCompCount_MAX, DynamicPSysCompCount_MAX);
		GStatManager.SetStatValue(STAT_DynamicPSysCompMem_MAX, DynamicPSysCompMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicEmitterCount_MAX, DynamicEmitterCount_MAX);
		GStatManager.SetStatValue(STAT_DynamicEmitterMem_MAX, DynamicEmitterMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicEmitterGTMem_Waste_MAX, DynamicEmitterGTMem_Waste_MAX);
		GStatManager.SetStatValue(STAT_DynamicEmitterGTMem_Largest_MAX, DynamicEmitterGTMem_Largest_MAX);
		GStatManager.SetStatValue(STAT_GTParticleData_MAX, TotalGTParticleData_MAX);
		GStatManager.SetStatValue(STAT_RTParticleData_MAX, TotalRTParticleData_MAX);
		GStatManager.SetStatValue(STAT_RTParticleData_Largest_MAX, LargestRTParticleData_MAX);

		GStatManager.SetStatValue(STAT_DynamicSpriteCount_MAX, DynamicSpriteCount_MAX);
		GStatManager.SetStatValue(STAT_DynamicSubUVCount_MAX, DynamicSubUVCount_MAX);
		GStatManager.SetStatValue(STAT_DynamicMeshCount_MAX, DynamicMeshCount_MAX);
		GStatManager.SetStatValue(STAT_DynamicBeamCount_MAX, DynamicBeamCount_MAX);
		GStatManager.SetStatValue(STAT_DynamicTrailCount_MAX, DynamicTrailCount_MAX);
		GStatManager.SetStatValue(STAT_DynamicRibbonCount_MAX, DynamicRibbonCount_MAX);
		GStatManager.SetStatValue(STAT_DynamicAnimTrailCount_MAX, DynamicAnimTrailCount_MAX);

		GStatManager.SetStatValue(STAT_DynamicSpriteGTMem_MAX, DynamicSpriteGTMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicSubUVGTMem_Max, DynamicSubUVGTMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicMeshGTMem_MAX, DynamicMeshGTMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicBeamGTMem_MAX, DynamicBeamGTMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicTrailGTMem_MAX, DynamicTrailGTMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicRibbonGTMem_MAX, DynamicRibbonGTMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicAnimTrailGTMem_MAX, DynamicAnimTrailGTMem_MAX);
		GStatManager.SetStatValue(STAT_DynamicUntrackedGTMem_MAX, DynamicUntrackedGTMem_MAX);
	}
#endif
	Clear();
}
	
//
/**
 *	Add a particle system component to the list.
 *
 *	@param		InPSysComp		The particle system component to add.
 *
 */
void FParticleDataManager::AddParticleSystemComponent(UParticleSystemComponent* InPSysComp)
{
	if ((GIsUCC == FALSE) && (GIsCooking == FALSE))
	{
		if (InPSysComp)
		{
			PSysComponents.Set(InPSysComp, TRUE);
		}
	}
}

/**
 *	Remove a particle system component to the list.
 *
 *	@param		InPSysComp		The particle system component to remove.
 *
 */
void FParticleDataManager::RemoveParticleSystemComponent(UParticleSystemComponent* InPSysComp)
{
	if ((GIsUCC == FALSE) && (GIsCooking == FALSE))
	{
		PSysComponents.Remove(InPSysComp);
	}
}

/**
 *	Return TRUE if the data manager has a copy of the given particle system component.
 *
 *	@param	InPSysComp		The particle system component to look for.
 *
 *	@return	UBOOL			TRUE if the PSysComp is in the data manager, FALSE if not
 */
UBOOL FParticleDataManager::HasParticleSystemComponent(UParticleSystemComponent* InPSysComp)
{
	return (PSysComponents.Find(InPSysComp) != NULL);
}

/**
 *	Clear all pending components from the queue.
 *
 */
void FParticleDataManager::Clear()
{
	PSysComponents.Reset();
}
