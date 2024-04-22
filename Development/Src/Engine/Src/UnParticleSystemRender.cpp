/*=============================================================================
	UnParticleSystemRender.cpp: Particle system rendering functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "DiagnosticTable.h"

#include "UnParticleHelper.h"

#include "ParticleInstancedMeshVertexFactory.h"

//@todo.SAS. Remove this once the Trail packing bug is corrected.
#include "ScenePrivate.h"

#if WITH_APEX
#include "NvApexRender.h"
#include "NvApexScene.h"
#endif

/** 
 * Whether to track particle rendering stats.  
 * Enable with the TRACKPARTICLERENDERINGSTATS command. 
 */
UBOOL GTrackParticleRenderingStats = FALSE;

/** Seconds between stat captures. */
FLOAT GTimeBetweenParticleRenderStatCaptures = 5.0f;

/** Minimum render time for a single DrawDynamicElements call that should be recorded. */
FLOAT GMinParticleDrawTimeToTrack = .0001f;

/** Whether to do LOD calculation on GameThread in game */
extern UBOOL GbEnableGameThreadLODCalculation;

/** Stats gathered about each UParticleSystem that is rendered. */
struct FParticleTemplateRenderStats
{
	FLOAT MaxRenderTime;
	INT NumDraws;
	INT NumEmitters;
	INT NumComponents;
	INT NumDrawDynamicElements;

	FParticleTemplateRenderStats() :
		NumComponents(0),
		NumDrawDynamicElements(0)
	{}
};

/** Sorts FParticleTemplateRenderStats from longest MaxRenderTime to shortest. */
IMPLEMENT_COMPARE_CONSTREF(FParticleTemplateRenderStats,UnParticleSystemRender,{ return A.MaxRenderTime < B.MaxRenderTime ? 1 : -1; });

/** Stats gathered about each UParticleSystemComponent that is rendered. */
struct FParticleComponentRenderStats
{
	FLOAT MaxRenderTime;
	INT NumDraws;
};

/** Sorts FParticleComponentRenderStats from longest MaxRenderTime to shortest. */
IMPLEMENT_COMPARE_CONSTREF(FParticleComponentRenderStats,UnParticleSystemRender,{ return A.MaxRenderTime < B.MaxRenderTime ? 1 : -1; });

/** Global map from UParticleSystem path name to stats about that particle system, used when tracking particle render stats. */
TMap<FString, FParticleTemplateRenderStats> GTemplateRenderStats;

/** Global map from UParticleSystemComponent path name to stats about that component, used when tracking particle render stats. */
TMap<FString, FParticleComponentRenderStats> GComponentRenderStats;

/** 
 * Dumps particle render stats to the game's log directory, and resets the stats tracked so far. 
 * This is hooked up to the DUMPPARTICLERENDERINGSTATS console command.
 */
void DumpParticleRenderingStats(FOutputDevice& Ar)
{
#if STATS
	if (GTrackParticleRenderingStats)
	{
		{
			// Have to keep this filename short enough to be valid for Xbox 360
			const FString TemplateFileName = FString::Printf(TEXT("%sPT-%s.csv"),*appProfilingDir(),*appSystemTimeString());
			FDiagnosticTableViewer ParticleTemplateViewer(*TemplateFileName);

			// Write a row of headings for the table's columns.
			ParticleTemplateViewer.AddColumn(TEXT("MaxRenderTime ms"));
			ParticleTemplateViewer.AddColumn(TEXT("NumEmitters"));
			ParticleTemplateViewer.AddColumn(TEXT("NumDraws"));
			ParticleTemplateViewer.AddColumn(TEXT("Template Name"));
			ParticleTemplateViewer.CycleRow();

			// Sort from longest render time to shortest
			GTemplateRenderStats.ValueSort<COMPARE_CONSTREF_CLASS(FParticleTemplateRenderStats,UnParticleSystemRender)>();

			for (TMap<FString, FParticleTemplateRenderStats>::TIterator It(GTemplateRenderStats); It; ++It)
			{	
				const FParticleTemplateRenderStats& Stats = It.Value();
				ParticleTemplateViewer.AddColumn(TEXT("%.2f"), Stats.MaxRenderTime * 1000.0f);
				ParticleTemplateViewer.AddColumn(TEXT("%u"), Stats.NumEmitters);
				ParticleTemplateViewer.AddColumn(TEXT("%u"), Stats.NumDraws);
				ParticleTemplateViewer.AddColumn(*It.Key());
				ParticleTemplateViewer.CycleRow();
			}
			Ar.Logf(TEXT("Template stats saved to %s"), *TemplateFileName);
		}
		
		{
			const FString ComponentFileName = FString::Printf(TEXT("%sPC-%s.csv"),*appProfilingDir(),*appSystemTimeString());
			FDiagnosticTableViewer ParticleComponentViewer(*ComponentFileName);

			// Write a row of headings for the table's columns.
			ParticleComponentViewer.AddColumn(TEXT("MaxRenderTime ms"));
			ParticleComponentViewer.AddColumn(TEXT("NumDraws"));
			ParticleComponentViewer.AddColumn(TEXT("Actor Name"));
			ParticleComponentViewer.CycleRow();

			// Sort from longest render time to shortest
			GComponentRenderStats.ValueSort<COMPARE_CONSTREF_CLASS(FParticleComponentRenderStats,UnParticleSystemRender)>();

			for (TMap<FString, FParticleComponentRenderStats>::TIterator It(GComponentRenderStats); It; ++It)
			{	
				const FParticleComponentRenderStats& Stats = It.Value();
				ParticleComponentViewer.AddColumn(TEXT("%.2f"), Stats.MaxRenderTime * 1000.0f);
				ParticleComponentViewer.AddColumn(TEXT("%u"), Stats.NumDraws);
				ParticleComponentViewer.AddColumn(*It.Key());
				ParticleComponentViewer.CycleRow();
			}
			Ar.Logf(TEXT("Component stats saved to %s"), *ComponentFileName);
		}

		GTemplateRenderStats.Empty();
		GComponentRenderStats.Empty();
	}
	else
	{
		Ar.Logf(TEXT("Need to start tracking with TRACKPARTICLERENDERINGSTATS first."));
	}
#endif
}

UBOOL GWantsParticleStatsNextFrame = FALSE;
UBOOL GTrackParticleRenderingStatsForOneFrame = FALSE;

#if STATS

/** Kicks off a particle render stat frame capture, initiated by the DUMPPARTICLEFRAMERENDERINGSTATS console command. */
void BeginOneFrameParticleStats()
{
	if (GWantsParticleStatsNextFrame)
	{
		warnf(TEXT("BeginOneFrameParticleStats"));
		GWantsParticleStatsNextFrame = FALSE;
		// Block until the renderer processes all queued commands, because the rendering thread reads from GTrackParticleRenderingStatsForOneFrame
		FlushRenderingCommands();
		GTrackParticleRenderingStatsForOneFrame = TRUE;
	}
}

TMap<FString, FParticleTemplateRenderStats> GOneFrameTemplateRenderStats;

/** 
 * Dumps particle render stats to the game's log directory along with a screenshot. 
 * This is hooked up to the DUMPPARTICLEFRAMERENDERINGSTATS console command.
 */
void FinishOneFrameParticleStats()
{
	if (GTrackParticleRenderingStatsForOneFrame)
	{
		warnf(TEXT("FinishOneFrameParticleStats"));

		// Wait until the rendering thread finishes processing the previous frame
		FlushRenderingCommands();

		GTrackParticleRenderingStatsForOneFrame = FALSE;

		const FString PathFromScreenshots = FString(TEXT("Particle")) + appSystemTimeString();
		const FString WritePath = appScreenShotDir() + PathFromScreenshots;

		const UBOOL bDirSuccess = GFileManager->MakeDirectory(*WritePath);

		warnf(TEXT("GFileManager->MakeDirectory %s %u"), *WritePath, bDirSuccess);

		// Have to keep this filename short enough to be valid for Xbox 360
		const FString TemplateFileName = WritePath + PATH_SEPARATOR + TEXT("ParticleTemplates.csv");
		FDiagnosticTableViewer ParticleTemplateViewer(*TemplateFileName);

		warnf(TEXT("TemplateFileName %s %u"), *TemplateFileName, ParticleTemplateViewer.OutputStreamIsValid());

		// Write a row of headings for the table's columns.
		ParticleTemplateViewer.AddColumn(TEXT("RenderTime ms"));
		ParticleTemplateViewer.AddColumn(TEXT("NumComponents"));
		ParticleTemplateViewer.AddColumn(TEXT("NumPasses"));
		ParticleTemplateViewer.AddColumn(TEXT("NumEmitters"));
		ParticleTemplateViewer.AddColumn(TEXT("NumDraws"));
		ParticleTemplateViewer.AddColumn(TEXT("Template Name"));
		ParticleTemplateViewer.CycleRow();

		FLOAT TotalRenderTime = 0;
		INT TotalNumComponents = 0;
		INT TotalNumDrawDynamicElements = 0;
		INT TotalNumEmitters = 0;
		INT TotalNumDraws = 0;

		// Sort from longest render time to shortest
		GOneFrameTemplateRenderStats.ValueSort<COMPARE_CONSTREF_CLASS(FParticleTemplateRenderStats,UnParticleSystemRender)>();

		for (TMap<FString, FParticleTemplateRenderStats>::TIterator It(GOneFrameTemplateRenderStats); It; ++It)
		{	
			const FParticleTemplateRenderStats& Stats = It.Value();
			TotalRenderTime += Stats.MaxRenderTime;
			TotalNumComponents += Stats.NumComponents;
			TotalNumDrawDynamicElements += Stats.NumDrawDynamicElements;
			TotalNumEmitters += Stats.NumEmitters;
			TotalNumDraws += Stats.NumDraws;
			ParticleTemplateViewer.AddColumn(TEXT("%.2f"), Stats.MaxRenderTime * 1000.0f);
			ParticleTemplateViewer.AddColumn(TEXT("%u"), Stats.NumComponents);
			ParticleTemplateViewer.AddColumn(TEXT("%.1f"), Stats.NumDrawDynamicElements / (FLOAT)Stats.NumComponents);
			ParticleTemplateViewer.AddColumn(TEXT("%u"), Stats.NumEmitters);
			ParticleTemplateViewer.AddColumn(TEXT("%u"), Stats.NumDraws);
			ParticleTemplateViewer.AddColumn(*It.Key());
			ParticleTemplateViewer.CycleRow();
		}

		ParticleTemplateViewer.AddColumn(TEXT("%.2f"), TotalRenderTime * 1000.0f);
		ParticleTemplateViewer.AddColumn(TEXT("%u"), TotalNumComponents);
		ParticleTemplateViewer.AddColumn(TEXT("%.1f"), TotalNumDrawDynamicElements / (FLOAT)TotalNumComponents);
		ParticleTemplateViewer.AddColumn(TEXT("%u"), TotalNumEmitters);
		ParticleTemplateViewer.AddColumn(TEXT("%u"), TotalNumDraws);
		ParticleTemplateViewer.AddColumn(TEXT("Totals"));
		ParticleTemplateViewer.CycleRow();

		warnf(TEXT("One frame stats saved to %s"), *TemplateFileName);

		extern UBOOL GScreenShotRequest;
		extern FString GScreenShotName;

		// Request a screenshot to be saved next frame
		GScreenShotName = PathFromScreenshots + PATH_SEPARATOR + TEXT("Shot.bmp");
		GScreenShotRequest = TRUE;

		warnf(TEXT("GScreenShotName %s"), *GScreenShotName);

		GOneFrameTemplateRenderStats.Empty();
	}
}
#endif

#define LOG_DETAILED_PARTICLE_RENDER_STATS 0

#if LOG_DETAILED_PARTICLE_RENDER_STATS 
/** Global detailed update stats. */
static FDetailedTickStats GDetailedParticleRenderStats( 20, 10, 1, 4, TEXT("rendering") );
#define TRACK_DETAILED_PARTICLE_RENDER_STATS(Object) FScopedDetailTickStats DetailedTickStats(GDetailedParticleRenderStats,Object);
#else
#define TRACK_DETAILED_PARTICLE_RENDER_STATS(Object)
#endif

///////////////////////////////////////////////////////////////////////////////
FParticleOrderPool GParticleOrderPool;

IMPLEMENT_COMPARE_CONSTREF(FParticleOrder,UnParticleSystemRender,{ return A.Z < B.Z ? 1 : -1; });

///////////////////////////////////////////////////////////////////////////////
// Particle vertex factory pool
FParticleVertexFactoryPool GParticleVertexFactoryPool;

FParticleVertexFactory* FParticleVertexFactoryPool::GetParticleVertexFactory(EParticleVertexFactoryType InType)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePoolTime);
	check(InType < PVFT_MAX);
	FParticleVertexFactory* VertexFactory = NULL;
	if (VertexFactoriesAvailable[InType].Num() == 0)
	{
		// If there are none in the pool, create a new one, add it to the in use list and return it
		VertexFactory = CreateParticleVertexFactory(InType);
		VertexFactories.AddItem(VertexFactory);
	}
	else
	{
		// Otherwise, pull one out of the available array
		VertexFactory = VertexFactoriesAvailable[InType](VertexFactoriesAvailable[InType].Num() - 1);
		VertexFactoriesAvailable[InType].Remove(VertexFactoriesAvailable[InType].Num() - 1);
	}
	check(VertexFactory);
	// Set it to true to indicate it is in use
	VertexFactory->SetInUse(TRUE);
	return VertexFactory;
}

UBOOL FParticleVertexFactoryPool::ReturnParticleVertexFactory(FParticleVertexFactory* InVertexFactory)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePoolTime);
	// Set it to false to indicate it is not in use
	InVertexFactory->SetInUse(FALSE);
	VertexFactoriesAvailable[InVertexFactory->GetVertexFactoryType()].AddItem(InVertexFactory);
	return TRUE;
}

void FParticleVertexFactoryPool::ClearPool()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePoolTime);

	for (INT TestIndex=VertexFactories.Num()-1; TestIndex >= 0; --TestIndex)
	{
		FParticleVertexFactory* VertexFactory = VertexFactories(TestIndex);
		if (!VertexFactory->GetInUse())
		{
			VertexFactories.RemoveSwap(TestIndex);
		}
	}

	// Release all the resources...
	// We can't safely touched the 'in-use' ones... 
	for (INT PoolIdx = 0; PoolIdx < PVFT_MAX; PoolIdx++)
	{
		for (INT RemoveIdx = VertexFactoriesAvailable[PoolIdx].Num() - 1; RemoveIdx >= 0; RemoveIdx--)
		{
			FParticleVertexFactory* VertexFactory = VertexFactoriesAvailable[PoolIdx](RemoveIdx);
			VertexFactory->ReleaseResource();
			delete VertexFactory;
			VertexFactoriesAvailable[PoolIdx].Remove(RemoveIdx);
		}
	}

}

void FParticleVertexFactoryPool::FreePool()
{
	ClearPool();
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticlePoolTime);
		for (INT TestIndex=VertexFactories.Num()-1; TestIndex >= 0; --TestIndex)
		{
			FParticleVertexFactory* VertexFactory = VertexFactories(TestIndex);
			check(VertexFactory);
			if (VertexFactory->GetInUse())
			{
				// Has already been released by the device cleanup...
				delete VertexFactory;
			}
		}
		VertexFactories.Empty();
	}
}

#if STATS
INT FParticleVertexFactoryPool::GetTypeSize(EParticleVertexFactoryType InType)
{
	switch (InType)
	{
	case PVFT_Sprite:						return sizeof(FParticleVertexFactory);
	case PVFT_Sprite_DynamicParameter:		return sizeof(FParticleDynamicParameterVertexFactory);
	case PVFT_SubUV:						return sizeof(FParticleSubUVVertexFactory);
	case PVFT_SubUV_DynamicParameter:		return sizeof(FParticleSubUVDynamicParameterVertexFactory);
	case PVFT_PointSprite:					return sizeof(FParticlePointSpriteVertexFactory);
	case PVFT_BeamTrail:					return sizeof(FParticleBeamTrailVertexFactory);
	case PVFT_BeamTrail_DynamicParameter:	return sizeof(FParticleBeamTrailDynamicParameterVertexFactory);
	default:								return 0;
	}
}

void FParticleVertexFactoryPool::DumpInfo(FOutputDevice& Ar)
{
	Ar.Logf(TEXT("ParticleVertexFactoryPool State"));
	Ar.Logf(TEXT("Type,Count,Mem(Bytes)"));
	INT TotalMemory = 0;
	for (INT PoolIdx = 0; PoolIdx < PVFT_MAX; PoolIdx++)
	{
		INT LocalMemory = GetTypeSize((EParticleVertexFactoryType)PoolIdx) * VertexFactoriesAvailable[PoolIdx].Num();
		Ar.Logf(TEXT("%s,%d,%d"), 
			GetTypeString((EParticleVertexFactoryType)PoolIdx), 
			VertexFactoriesAvailable[PoolIdx].Num(),
			LocalMemory);
		TotalMemory += LocalMemory;
	}
	Ar.Logf(TEXT("TotalMemory Taken in Pool: %d"), TotalMemory);
	TotalMemory = 0;
	Ar.Logf(TEXT("ACTIVE,%d"), VertexFactories.Num());
	if (VertexFactories.Num() > 0)
	{
		INT ActiveCounts[PVFT_MAX];
		appMemzero(&ActiveCounts[0], sizeof(INT) * PVFT_MAX);
		for (INT InUseIndex = 0; InUseIndex < VertexFactories.Num(); ++InUseIndex)
		{
			FParticleVertexFactory* VertexFactory = VertexFactories(InUseIndex);
			if (VertexFactory->GetInUse())
			{
				ActiveCounts[VertexFactory->GetVertexFactoryType()]++;
			}
		}
		for (INT PoolIdx = 0; PoolIdx < PVFT_MAX; PoolIdx++)
		{
			INT LocalMemory = GetTypeSize((EParticleVertexFactoryType)PoolIdx) * ActiveCounts[PoolIdx];
			Ar.Logf(TEXT("%s,%d,%d"), 
				GetTypeString((EParticleVertexFactoryType)PoolIdx), 
				ActiveCounts[PoolIdx],
				LocalMemory);
			TotalMemory += LocalMemory;
		}
	}
	Ar.Logf(TEXT("TotalMemory Taken by Actives: %d"), TotalMemory);
}
#endif

/** 
 *	Create a vertex factory for the given type.
 *
 *	@param	InType						The type of vertex factory to create.
 *
 *	@return	FParticleVertexFactory*		The created VF; NULL if invalid InType
 */
FParticleVertexFactory* FParticleVertexFactoryPool::CreateParticleVertexFactory(EParticleVertexFactoryType InType)
{
	FParticleVertexFactory* NewVertexFactory = NULL;
	switch (InType)
	{
	case PVFT_Sprite:
		NewVertexFactory = new FParticleVertexFactory();
		break;
	case PVFT_Sprite_DynamicParameter:
		NewVertexFactory = new FParticleDynamicParameterVertexFactory();
		break;
	case PVFT_SubUV:
		NewVertexFactory = new FParticleSubUVVertexFactory();
		break;
	case PVFT_SubUV_DynamicParameter:
		NewVertexFactory = new FParticleSubUVDynamicParameterVertexFactory();
		break;
	case PVFT_PointSprite:
		NewVertexFactory = new FParticlePointSpriteVertexFactory();
		break;
	case PVFT_BeamTrail:
		NewVertexFactory = new FParticleBeamTrailVertexFactory();
		break;
	case PVFT_BeamTrail_DynamicParameter:
		NewVertexFactory = new FParticleBeamTrailDynamicParameterVertexFactory();
		break;
	default:
		break;
	}
	check(NewVertexFactory);
	NewVertexFactory->SetVertexFactoryType(InType);
	NewVertexFactory->InitResource();
	return NewVertexFactory;
}

void ParticleVertexFactoryPool_FreePool_RenderingThread()
{
	GParticleVertexFactoryPool.FreePool();
}

void ParticleVertexFactoryPool_FreePool()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		ParticleVertexFactoryFreePool,
	{
		ParticleVertexFactoryPool_FreePool_RenderingThread();
	}
	);		
}

void ParticleVertexFactoryPool_ClearPool_RenderingThread()
{
	GParticleVertexFactoryPool.ClearPool();
}

/** Globally accessible function for clearing the pool */
void ParticleVertexFactoryPool_ClearPool()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		ParticleVertexFactoryFreePool,
	{
		ParticleVertexFactoryPool_ClearPool_RenderingThread();
	}
	);		
}

///////////////////////////////////////////////////////////////////////////////
/**  
 * Simple function to pass the async buffer fill task off to the owning emitter
*/
void FAsyncParticleFill::DoWork()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleAsyncTime);
	Parent->DoBufferFill();
}


/** Pool of async particle fill tasks **/
static TArray<FAsyncTask<FAsyncParticleFill>*> AsyncTaskPool;

/** Allocate and return a new particle fill task
  * @param InParent emitter to forward the eventual async call to
*/
FAsyncTask<FAsyncParticleFill>* FAsyncParticleFill::GetAsyncTask(struct FDynamicSpriteEmitterDataBase* InParent)
{
	if (AsyncTaskPool.Num())
	{
		FAsyncTask<FAsyncParticleFill>* Ret = AsyncTaskPool.Pop();
		Ret->GetTask().Parent = InParent;
		return Ret;
	}
	return new FAsyncTask<FAsyncParticleFill>(InParent);
}
/** Return a task to the async task pool for recycling. Will call EnsureCompletion
  * @param TaskToRecycle task to recycle
*/
void FAsyncParticleFill::DisposeAsyncTask(FAsyncTask<FAsyncParticleFill>* TaskToRecycle)
{
	if (TaskToRecycle)
	{
		TaskToRecycle->EnsureCompletion();
		TaskToRecycle->GetTask().Parent = NULL;
		AsyncTaskPool.Push(TaskToRecycle);
	}
}



void FDynamicSpriteEmitterDataBase::SortSpriteParticles(INT SortMode, UBOOL bLocalSpace, 
	INT ParticleCount, const TArray<BYTE>& ParticleData, INT ParticleStride, const TArray<WORD>& ParticleIndices,
	const FSceneView* View, FMatrix& LocalToWorld, FParticleOrder* ParticleOrder)
{
	SCOPE_CYCLE_COUNTER(STAT_SortingTime);

	if (SortMode == PSORTMODE_ViewProjDepth)
	{
		for (INT ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData.GetData() + ParticleStride * ParticleIndices(ParticleIndex));
			FLOAT InZ;
			if (bLocalSpace)
			{
				InZ = View->ViewProjectionMatrix.TransformFVector(LocalToWorld.TransformFVector(Particle.Location)).Z;
			}
			else
			{
				InZ = View->ViewProjectionMatrix.TransformFVector(Particle.Location).Z;
			}
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].Z = InZ;
		}
	}
	else if (SortMode == PSORTMODE_DistanceToView)
	{
		for (INT ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData.GetData() + ParticleStride * ParticleIndices(ParticleIndex));
			FLOAT InZ;
			FVector Position;
			if (bLocalSpace)
			{
				Position = LocalToWorld.TransformFVector(Particle.Location);
			}
			else
			{
				Position = Particle.Location;
			}
			InZ = (FVector(View->ViewOrigin) - Position).SizeSquared();
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].Z = InZ;
		}
	}
	else if (SortMode == PSORTMODE_Age_OldestFirst)
	{
		for (INT ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData.GetData() + ParticleStride * ParticleIndices(ParticleIndex));
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].Z = Particle.RelativeTime;
		}
	}
	else if (SortMode == PSORTMODE_Age_NewestFirst)
	{
		for (INT ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData.GetData() + ParticleStride * ParticleIndices(ParticleIndex));
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].Z = 1.0f - Particle.RelativeTime;
		}
	}

	Sort<USE_COMPARE_CONSTREF(FParticleOrder,UnParticleSystemRender)>(ParticleOrder,ParticleCount);
}

void FDynamicSpriteEmitterDataBase::RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses)
{
	check(SceneProxy);

	const FDynamicSpriteEmitterReplayData& SpriteSource =
		static_cast< const FDynamicSpriteEmitterReplayData& >( GetSource() );

	const FMatrix& LocalToWorld = SpriteSource.bUseLocalSpace ? SceneProxy->GetLocalToWorld() : FMatrix::Identity;

	FMatrix CameraToWorld = View->ViewMatrix.Inverse();
	FVector CamX = CameraToWorld.TransformNormal(FVector(1,0,0));
	FVector CamY = CameraToWorld.TransformNormal(FVector(0,1,0));

	FLinearColor EmitterEditorColor = FLinearColor(1.0f,1.0f,0);

	for (INT i = 0; i < SpriteSource.ActiveParticleCount; i++)
	{
		DECLARE_PARTICLE(Particle, SpriteSource.ParticleData.GetData() + SpriteSource.ParticleStride * SpriteSource.ParticleIndices(i));

		FVector DrawLocation = LocalToWorld.TransformFVector(Particle.Location);
		if (bCrosses)
		{
			FVector Size = Particle.Size * SpriteSource.Scale;
			PDI->DrawLine(DrawLocation - (0.5f * Size.X * CamX), DrawLocation + (0.5f * Size.X * CamX), EmitterEditorColor, DPGIndex);
			PDI->DrawLine(DrawLocation - (0.5f * Size.Y * CamY), DrawLocation + (0.5f * Size.Y * CamY), EmitterEditorColor, DPGIndex);
		}
		else
		{
			PDI->DrawPoint(DrawLocation, EmitterEditorColor, 2, DPGIndex);
		}
	}
}

FDynamicSortableSpriteEmitterDataBase::FDynamicSortableSpriteEmitterDataBase( const UParticleModuleRequired* RequiredModule )
	: FDynamicSpriteEmitterDataBase(RequiredModule)
	, PrimitiveCount(0)
{
	if( RequiredModule )
	{
		bEnableNearParticleCulling = RequiredModule->bEnableNearParticleCulling;
		bEnableFarParticleCulling = RequiredModule->bEnableFarParticleCulling;
		NearCullDistance = RequiredModule->NearCullDistance;
		NearFadeDistance = RequiredModule->NearFadeDistance;
		FarFadeDistance = RequiredModule->FarFadeDistance;
		FarCullDistance = RequiredModule->FarCullDistance;
	}
	else
	{
		bEnableNearParticleCulling = FALSE;
		bEnableFarParticleCulling = FALSE;
		NearCullDistance = 0.0f;
		NearFadeDistance = 0.0f;
		FarFadeDistance = 0.0f;
		FarCullDistance = 0.0f;
	}
}

INT FDynamicSortableSpriteEmitterDataBase::Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_SpriteRenderingTime);

	if (bValid == FALSE)
	{
		return 0;
	}

	const FDynamicSpriteEmitterReplayDataBase* SourceData = GetSourceData();
	check(SourceData);
	FParticleVertexFactory* VertexFactory = GetVertexFactory();

	INT NumDraws = 0;
	if (SourceData->EmitterRenderMode == ERM_Normal)
	{
		// Don't render if the material will be ignored
		if (PDI->IsMaterialIgnored(MaterialResource[bSelected]) && !(View->Family->ShowFlags & SHOW_Wireframe))
		{
			return 0;
		}

		FMatrix& LocalToWorld = Proxy->GetLocalToWorld();

		VertexFactory->SetScreenAlignment(SourceData->ScreenAlignment);
		VertexFactory->SetLockAxesFlag(SourceData->LockAxisFlag);
		if (SourceData->LockAxisFlag != EPAL_NONE)
		{
			FVector Up, Right;
			Proxy->GetAxisLockValues((FDynamicSpriteEmitterDataBase*)this, SourceData->bUseLocalSpace, Up, Right);
			VertexFactory->SetLockAxes(Up, Right);
		}

		// Transform local space coordinates into worldspace
		const FVector WorldSpaceSphereCenter = LocalToWorld.TransformFVector(SourceData->NormalsSphereCenter);
		const FVector WorldSpaceCylinderDirection = LocalToWorld.TransformNormal(SourceData->NormalsCylinderDirection);
		VertexFactory->SetNormalsData(SourceData->EmitterNormalsMode, WorldSpaceSphereCenter, WorldSpaceCylinderDirection);

		INT ParticleCount = SourceData->ActiveParticleCount;

		FParticleOrder* ParticleOrder = NULL;

		//if( Source.bRequiresSorting )
		if (SourceData->SortMode != PSORTMODE_None)
		{
			// If material is using unlit translucency and the blend mode is translucent or 
			// if it is using unlit distortion then we need to sort (back to front)
			const FMaterial* Material = MaterialResource[bSelected]->GetMaterial();
			if (Material && 
				((Material->GetBlendMode() == BLEND_Translucent || Material->GetBlendMode() == BLEND_AlphaComposite || Material->IsDistorted()) ||
				((SourceData->SortMode == PSORTMODE_Age_OldestFirst) || (SourceData->SortMode == PSORTMODE_Age_NewestFirst)))
				)
			{
				ParticleOrder = GParticleOrderPool.GetParticleOrderData(ParticleCount);
				SortSpriteParticles(SourceData->SortMode, SourceData->bUseLocalSpace, SourceData->ActiveParticleCount, 
					SourceData->ParticleData, SourceData->ParticleStride, SourceData->ParticleIndices,
					View, LocalToWorld, ParticleOrder);
			}
		}

		CameraPosition = View->ViewOrigin;

		FMeshBatch Mesh;
		FMeshBatchElement& BatchElement = Mesh.Elements(0);
		Mesh.UseDynamicData = TRUE;
		BatchElement.IndexBuffer = NULL;
		Mesh.VertexFactory = GetVertexFactory();
		// if the particle rendering data is presupplied, use it directly
		if (SourceData->ParticleRenderData)
		{
			Mesh.DynamicVertexData = SourceData->ParticleRenderData;
			BatchElement.DynamicIndexData = SourceData->ParticleRenderIndices;
			Mesh.ParticleType = PET_PresuppliedMemory;
			BatchElement.DynamicIndexStride = 2;
			BatchElement.NumPrimitives = appTrunc(ParticleCount * 2);
		}
		// otherwise, set up the mesh to let render thread fill out the vertices
		else
		{
			Mesh.DynamicVertexData = this;
			BatchElement.DynamicIndexData = ParticleOrder;
			Mesh.ParticleType = GetParticleType();
			BatchElement.DynamicIndexStride = 0;
			BatchElement.NumPrimitives = ParticleCount;

			if (VertexFactory->UsePointSprites())
			{
				Mesh.ParticleType = PET_PointSprite;
				Mesh.DynamicVertexStride = sizeof(FParticlePointSpriteVertex);
			}
		}
		Mesh.DynamicVertexStride = GetDynamicVertexStride();
		Mesh.LCI = NULL;
		if (SourceData->bUseLocalSpace == TRUE)
		{
			BatchElement.LocalToWorld = LocalToWorld;
			BatchElement.WorldToLocal = Proxy->GetWorldToLocal();
		}
		else
		{
			BatchElement.LocalToWorld = FMatrix::Identity;
			BatchElement.WorldToLocal = FMatrix::Identity;
		}
		BatchElement.FirstIndex = 0;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = (ParticleCount * 4) - 1;
		Mesh.ReverseCulling = Proxy->GetLocalToWorldDeterminant() < 0.0f ? TRUE : FALSE;
		Mesh.CastShadow = Proxy->GetCastShadow();
		Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)DPGIndex;

		Mesh.MaterialRenderProxy = MaterialResource[GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? bSelected : 0];
		Mesh.Type = PT_TriangleList;
		Mesh.bUsePreVertexShaderCulling = FALSE;
		Mesh.PlatformMeshData = NULL;
		Mesh.bUseDownsampledTranslucency = (appIsNearlyZero(DownsampleThresholdScreenFraction) ? FALSE : ShouldRenderDownsampled(View, Proxy->GetBounds()));

		NumDraws += DrawRichMesh(
			PDI, 
			Mesh, 
			FLinearColor(1.0f, 0.0f, 0.0f),	//WireframeColor,
			FLinearColor(1.0f, 1.0f, 0.0f),	//LevelColor,
			FLinearColor(1.0f, 1.0f, 1.0f),	//PropertyColor,		
			Proxy->GetPrimitiveSceneInfo(),
			GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? Proxy->IsSelected() : FALSE
			);
	}
	else if (SourceData->EmitterRenderMode == ERM_Point)
	{
		RenderDebug(PDI, View, DPGIndex, FALSE);
	}
	else if (SourceData->EmitterRenderMode == ERM_Cross)
	{
		RenderDebug(PDI, View, DPGIndex, TRUE);
	}

	return NumDraws;
}

void FDynamicSortableSpriteEmitterDataBase::UpdateParticleDistanceCulling
	( 
	const FVector& ParticlePosition, 
	const FLOAT NearCullDistanceSq, 
	const FLOAT NearFadeDistanceSq, 
	const FLOAT FarCullDistanceSq, 
	const FLOAT FarFadeDistanceSq, 
	FLinearColor &OutParticleColor, 
	FVector& OutSize 
	)
{
	const FDynamicSpriteEmitterReplayDataBase* SourceData = GetSourceData();
	check(SourceData);

	if( bEnableNearParticleCulling || bEnableFarParticleCulling )
	{
		FLOAT AdjustedAlpha = 1.0f;

		const FVector Position = SourceData->bUseLocalSpace == TRUE ? SceneProxy->GetLocalToWorld().TransformFVector(ParticlePosition) : FVector4( ParticlePosition );
		const FLOAT DistanceSq = (CameraPosition - Position).SizeSquared();

		if( bEnableNearParticleCulling )
		{
			if( DistanceSq <= NearCullDistanceSq )
			{
				OutParticleColor.A = 0.0f;
				AdjustedAlpha = 0.0f;
			}
			else if( DistanceSq > NearCullDistanceSq && DistanceSq < NearFadeDistanceSq )
			{
				const FLOAT Alpha = ( DistanceSq - NearCullDistanceSq )/( NearFadeDistanceSq - NearCullDistanceSq );
				OutParticleColor.A *= Alpha;
				AdjustedAlpha = Alpha;
			}
		}

		if( bEnableFarParticleCulling )
		{
			if( DistanceSq >= FarCullDistanceSq )
			{
				OutParticleColor.A = 0.0f;
				AdjustedAlpha = 0.0f;
			}
			else if( DistanceSq > FarFadeDistanceSq && DistanceSq < FarCullDistanceSq )
			{
				const FLOAT Alpha = ( DistanceSq - FarFadeDistanceSq )/( FarCullDistanceSq - FarFadeDistanceSq );
				OutParticleColor.A *= 1.0f - Alpha;
				AdjustedAlpha = 1.0f - Alpha;
			}
		}

		const FLOAT AlphaThreshold = 5.0f / 255.0f;
		if( AdjustedAlpha < AlphaThreshold )
		{
			OutSize = FVector::ZeroVector;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ParticleMeshEmitterInstance
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//	FDynamicSpriteEmitterData
///////////////////////////////////////////////////////////////////////////////

/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicSpriteEmitterData::Init( UBOOL bInSelected )
{
	bSelected = bInSelected;

	bUsesDynamicParameter = FALSE;
	if( Source.MaterialInterface->GetMaterialResource() != NULL )
	{
		bUsesDynamicParameter = Source.MaterialInterface->GetMaterialResource()->GetUsesDynamicParameter();
	}

	MaterialResource[0] = Source.MaterialInterface->GetRenderProxy(FALSE);
	MaterialResource[1] = GIsEditor ? Source.MaterialInterface->GetRenderProxy(TRUE) : MaterialResource[0];

	// We won't need this on the render thread
	Source.MaterialInterface = NULL;
}

const FVector2D ParticleSprite_UVArray[4] = 
{
	FVector2D(0.0f, 0.0f), 
	FVector2D(0.0f, 1.0f), 
	FVector2D(1.0f, 1.0f), 
	FVector2D(1.0f, 0.0f)
};
enum EParticleSpriteUVFLip
{
	UVFLIP_None,
	UVFLIP_X,
	UVFLIP_Y,
	UVFLIP_XY,
	UVFLIP_MAX
};

const INT ParticleSprite_UVIndices[UVFLIP_MAX][4] = 
{
	{ 0, 1, 2, 3 },	// None
	{ 3, 2, 1, 0 },	// Flip X
	{ 1, 0, 3, 2 },	// Flip Y
	{ 2, 3, 0, 1 },	// Flip XY
};

FORCEINLINE INT ParticleSprite_GetUVFlipMode(const FVector& InSize, const UBOOL bSquare)
{
	UBOOL bFlipX = (InSize.X < 0.0f);
	UBOOL bFlipY = (bSquare) ? bFlipX : (InSize.Y < 0.0f);
	if (bFlipX && bFlipY)
	{
		return (INT)UVFLIP_XY;
	}
	else if (bFlipX)
	{
		return (INT)UVFLIP_X;
	}
	else if (bFlipY)
	{
		return (INT)UVFLIP_Y;
	}
	return UVFLIP_None;
}

UBOOL FDynamicSpriteEmitterData::GetVertexAndIndexData(void* VertexData, void* FillIndexData, FParticleOrder* ParticleOrder)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePackingTime);
	INT ParticleCount = Source.ActiveParticleCount;
	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
	{
		ParticleCount = Source.MaxDrawCount;
	}

	UBOOL bFlipActive = (Source.bAllowImageFlipping == TRUE);
	UBOOL bSquareAlign = (Source.ScreenAlignment == PSA_Square);
	UBOOL bSquareAlignFlip = (bSquareAlign && (Source.bSquareImageFlipping == TRUE));

	// Pack the data
	INT	ParticleIndex;
	INT	ParticlePackingIndex = 0;
	INT	IndexPackingIndex = 0;

	const FLOAT NearCullDistanceSq = NearCullDistance*NearCullDistance;
	const FLOAT NearFadeDistanceSq = NearFadeDistance*NearFadeDistance;
	const FLOAT FarFadeDistanceSq = FarFadeDistance*FarFadeDistance;
	const FLOAT FarCullDistanceSq = FarCullDistance*FarCullDistance;

	INT VertexStride = sizeof(FParticleSpriteVertex);
	if (bUsesDynamicParameter)
	{
		VertexStride = sizeof(FParticleSpriteVertexDynamicParameter);
	}
	BYTE* TempVert = (BYTE*)VertexData;
	WORD* Indices = (WORD*)FillIndexData;
	FParticleSpriteVertex* FillVertex;
	FParticleSpriteVertexDynamicParameter* DynFillVertex;

	FVector OrbitOffset(0.0f, 0.0f, 0.0f);
	FVector PrevOrbitOffset(0.0f, 0.0f, 0.0f);
	FVector CameraOffset(0.0f);
	FVector CameraPrevOffset(0.0f);
	FVector4 DynamicParameterValue(1.0f,1.0f,1.0f,1.0f);
	FVector ParticlePosition;
	FVector ParticleOldPosition;

	BYTE* ParticleData = Source.ParticleData.GetData();
	WORD* ParticleIndices = Source.ParticleIndices.GetData();
	const FParticleOrder* OrderedIndices = ParticleOrder;
	for (INT i = 0; i < ParticleCount; i++)
	{
		ParticleIndex = OrderedIndices ? OrderedIndices[i].ParticleIndex : i;
		DECLARE_PARTICLE(Particle, ParticleData + Source.ParticleStride * ParticleIndices[ParticleIndex]);
		if (i + 1 < ParticleCount)
		{
			INT NextIndex = OrderedIndices ? OrderedIndices[i+1].ParticleIndex : (i + 1);
			DECLARE_PARTICLE(NextParticle, ParticleData + Source.ParticleStride * ParticleIndices[NextIndex]);
			PREFETCH(&NextParticle);
		}

		FVector Size = Particle.Size * Source.Scale;
		INT UVIndex = 0;
		if (bFlipActive == TRUE)
		{
			UVIndex = ParticleSprite_GetUVFlipMode(Size, bSquareAlignFlip);
			Size.X = fabs(Size.X);
			Size.Y = (bSquareAlign == TRUE) ? Size.X : fabs(Size.Y);
		}
		else if (bSquareAlign == TRUE)
		{
			Size.Y = Size.X;
		}

		FOrbitChainModuleInstancePayload* LocalOrbitPayload = NULL;

		ParticlePosition = Particle.Location;
		ParticleOldPosition = Particle.OldLocation;
		if (Source.OrbitModuleOffset != 0)
		{
			INT CurrentOffset = Source.OrbitModuleOffset;
			const BYTE* ParticleBase = (const BYTE*)&Particle;
			PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
			OrbitOffset = OrbitPayload.Offset;

			if (Source.bUseLocalSpace == FALSE)
			{
				OrbitOffset = SceneProxy->GetLocalToWorld().TransformNormal(OrbitOffset);
			}
			PrevOrbitOffset = OrbitPayload.PreviousOffset;

			LocalOrbitPayload = &OrbitPayload;

			ParticlePosition += OrbitOffset;
			ParticleOldPosition += PrevOrbitOffset;
		}

		if (Source.CameraPayloadOffset != 0)
		{
			FVector SavedParticlePosition = ParticlePosition;
			GetCameraOffsetFromPayload(Source, SceneProxy->GetLocalToWorld(), Particle, SavedParticlePosition, ParticlePosition, FALSE);
			if (Source.ScreenAlignment == PSA_Velocity)
			{
				// For PSA_Velocity to work correctly, we need to shift the old location as well.
				GetCameraOffsetFromPayload(Source, SceneProxy->GetLocalToWorld(), Particle, SavedParticlePosition, ParticleOldPosition, TRUE);
			}
		}

		if (Source.DynamicParameterDataOffset > 0)
		{
			GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, Particle, DynamicParameterValue);
		}

		// Near/Far Particle Distance Culling
		UpdateParticleDistanceCulling( ParticlePosition, NearCullDistanceSq, NearFadeDistanceSq, FarCullDistanceSq, FarFadeDistanceSq, Particle.Color, Size );

		// 0
		FillVertex = (FParticleSpriteVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->OldPosition	= ParticleOldPosition;
		FillVertex->Size		= Size;
#if !PARTICLES_USE_INDEXED_SPRITES
		FillVertex->Tex_U		= ParticleSprite_UVArray[ParticleSprite_UVIndices[UVIndex][0]].X;
		FillVertex->Tex_V		= ParticleSprite_UVArray[ParticleSprite_UVIndices[UVIndex][0]].Y;
#endif	//#if !PARTICLES_USE_INDEXED_SPRITES
		FillVertex->Rotation	= Particle.Rotation;
#if !PARTICLES_USE_INDEXED_SPRITES
		FillVertex->SizerIndex	= 0;
#else
		FillVertex->SizerIndex	= UVIndex;
#endif
		FillVertex->Color		= Particle.Color;
		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleSpriteVertexDynamicParameter*)TempVert;
			DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
			DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
			DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
			DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
		}
		TempVert += VertexStride;

#if !PARTICLES_USE_INDEXED_SPRITES
		// 1
		FillVertex = (FParticleSpriteVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->OldPosition	= ParticleOldPosition;
		FillVertex->Size		= Size;
		FillVertex->Tex_U		= ParticleSprite_UVArray[ParticleSprite_UVIndices[UVIndex][1]].X;
		FillVertex->Tex_V		= ParticleSprite_UVArray[ParticleSprite_UVIndices[UVIndex][1]].Y;
		FillVertex->Rotation	= Particle.Rotation;
		FillVertex->SizerIndex	= 1;
		FillVertex->Color		= Particle.Color;
		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleSpriteVertexDynamicParameter*)TempVert;
			DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
			DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
			DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
			DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
		}
		TempVert += VertexStride;
		// 2
		FillVertex = (FParticleSpriteVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->OldPosition	= ParticleOldPosition;
		FillVertex->Size		= Size;
		FillVertex->Tex_U		= ParticleSprite_UVArray[ParticleSprite_UVIndices[UVIndex][2]].X;
		FillVertex->Tex_V		= ParticleSprite_UVArray[ParticleSprite_UVIndices[UVIndex][2]].Y;
		FillVertex->Rotation	= Particle.Rotation;
		FillVertex->SizerIndex	= 2;
		FillVertex->Color		= Particle.Color;
		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleSpriteVertexDynamicParameter*)TempVert;
			DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
			DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
			DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
			DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
		}
		TempVert += VertexStride;
		// 3
		FillVertex = (FParticleSpriteVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->OldPosition	= ParticleOldPosition;
		FillVertex->Size		= Size;
		FillVertex->Tex_U		= ParticleSprite_UVArray[ParticleSprite_UVIndices[UVIndex][3]].X;
		FillVertex->Tex_V		= ParticleSprite_UVArray[ParticleSprite_UVIndices[UVIndex][3]].Y;
		FillVertex->Rotation	= Particle.Rotation;
		FillVertex->SizerIndex	= 3;
		FillVertex->Color		= Particle.Color;
		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleSpriteVertexDynamicParameter*)TempVert;
			DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
			DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
			DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
			DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
		}
		TempVert += VertexStride;

		if (Indices)
		{
			*Indices++ = (i * 4) + 0;
			*Indices++ = (i * 4) + 2;
			*Indices++ = (i * 4) + 3;
			*Indices++ = (i * 4) + 0;
			*Indices++ = (i * 4) + 1;
			*Indices++ = (i * 4) + 2;
		}

		if (LocalOrbitPayload)
		{
			LocalOrbitPayload->PreviousOffset = OrbitOffset;
		}
#endif	//#if !PARTICLES_USE_INDEXED_SPRITES
	}

	return TRUE;
}

/**
 * Function to fill in the data when it's time to render
 */
UBOOL FDynamicSpriteEmitterData::GetPointSpriteVertexData(void* VertexData, FParticleOrder* ParticleOrder)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePackingTime);
	INT ParticleCount = Source.ActiveParticleCount;
	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
	{
		ParticleCount = Source.MaxDrawCount;
	}

	INT VertexStride = sizeof(FParticlePointSpriteVertex);

	BYTE* TempVert = (BYTE*)VertexData;
	FParticlePointSpriteVertex* FillVertex;

	FVector ParticlePosition;

	if (Source.ScreenAlignment != PSA_Square)
	{
		warnf (NAME_ParticleWarn, TEXT("Point sprite particle has non-square alignment.  Only Size.X will be used"));
	}

	BYTE* ParticleData = Source.ParticleData.GetData();
	WORD* ParticleIndices = Source.ParticleIndices.GetData();
	FParticleOrder* OrderedIndices = ParticleOrder;
	for (INT i = 0; i < ParticleCount; i++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + Source.ParticleStride * ParticleIndices[i]);
		if (i + 1 < ParticleCount)
		{
			DECLARE_PARTICLE(NextParticle, ParticleData + Source.ParticleStride * ParticleIndices[i+1]);
			PREFETCH(&NextParticle);
		}

		FVector Size = Particle.Size * Source.Scale;
		ParticlePosition = Particle.Location;

		const UBOOL bUsesRGB = FALSE;

		FillVertex = (FParticlePointSpriteVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->Size		= Size.X;
		FillVertex->Color		= Particle.Color.ToFColor(bUsesRGB).DWColor();
		TempVert += VertexStride;
	}

	return TRUE;
}


/**
 *	Create the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicSpriteEmitterData::CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	// Create the vertex factory...
	//@todo. Cache these??
	if (VertexFactory == NULL)
	{
		if (bUsesDynamicParameter == FALSE)
		{
			VertexFactory = GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_Sprite);
		}
		else
		{
			VertexFactory = GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_Sprite_DynamicParameter);
		}
		check(VertexFactory);
	}

	return (VertexFactory != NULL);
}

/**
 *	Release the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicSpriteEmitterData::ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	if (VertexFactory != NULL)
	{
		GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
		VertexFactory = NULL;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//	FDynamicSubUVEmitterData
///////////////////////////////////////////////////////////////////////////////

/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicSubUVEmitterData::Init( UBOOL bInSelected )
{
	bSelected = bInSelected;

	bUsesDynamicParameter = FALSE;
	if( Source.MaterialInterface->GetMaterialResource() != NULL )
	{
		bUsesDynamicParameter =
			Source.MaterialInterface->GetMaterialResource()->GetUsesDynamicParameter();
	}

	MaterialResource[0] = Source.MaterialInterface->GetRenderProxy(FALSE);
	MaterialResource[1] = GIsEditor ? Source.MaterialInterface->GetRenderProxy(TRUE) : MaterialResource[0];

	// We won't need this on the render thread
	Source.MaterialInterface = NULL;
}


UBOOL FDynamicSubUVEmitterData::GetVertexAndIndexData(void* VertexData, void* FillIndexData, FParticleOrder* ParticleOrder)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePackingTime);
	INT ParticleCount = Source.ActiveParticleCount;
	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
	{
		ParticleCount = Source.MaxDrawCount;
	}

	UBOOL bFlipActive = (Source.bAllowImageFlipping == TRUE);
	UBOOL bSquareAlign = (Source.ScreenAlignment == PSA_Square);

	// Pack the data
	INT	ParticleIndex;
	INT	ParticlePackingIndex = 0;
	INT	IndexPackingIndex = 0;

	const FLOAT NearCullDistanceSq = NearCullDistance*NearCullDistance;
	const FLOAT NearFadeDistanceSq = NearFadeDistance*NearFadeDistance;
	const FLOAT FarFadeDistanceSq = FarFadeDistance*FarFadeDistance;
	const FLOAT FarCullDistanceSq = FarCullDistance*FarCullDistance;

	INT			SIHorz			= Source.SubImages_Horizontal;
	INT			SIVert			= Source.SubImages_Vertical;
	INT			iTotalSubImages = SIHorz * SIVert;
	FLOAT		baseU			= (1.0f / (FLOAT)SIHorz);
	FLOAT		baseV			= (1.0f / (FLOAT)SIVert);
	FLOAT		SubU[4];
	FLOAT		SubV[4];
	FLOAT		SubU2[4];
	FLOAT		SubV2[4];

	INT VertexStride = sizeof(FParticleSpriteSubUVVertex);
	if (bUsesDynamicParameter)
	{
		VertexStride = sizeof(FParticleSpriteSubUVVertexDynamicParameter);
	}
	BYTE* TempVert = (BYTE*)VertexData;
	WORD* Indices = (WORD*)FillIndexData;
	FParticleSpriteSubUVVertex* FillVertex;
	FParticleSpriteSubUVVertexDynamicParameter* DynFillVertex;

	FVector OrbitOffset(0.0f, 0.0f, 0.0f);
	FVector PrevOrbitOffset(0.0f, 0.0f, 0.0f);
	FVector CameraOffset(0.0f);
	FVector4 DynamicParameterValue(1.0f,1.0f,1.0f,1.0f);
	FVector ParticlePosition;
	FVector ParticleOldPosition;

	const BYTE* ParticleData = Source.ParticleData.GetData();
	const WORD* ParticleIndices = Source.ParticleIndices.GetData();
	const FParticleOrder* OrderedIndices = ParticleOrder;
	for (INT i = 0; i < ParticleCount; i++)
	{
		ParticleIndex = OrderedIndices ? OrderedIndices[i].ParticleIndex : i;

		DECLARE_PARTICLE(Particle, ParticleData + Source.ParticleStride * ParticleIndices[ParticleIndex]);
		if (i + 1 < ParticleCount)
		{
			INT NextIndex = OrderedIndices ? OrderedIndices[i+1].ParticleIndex : (i + 1);
			DECLARE_PARTICLE(NextParticle, ParticleData + Source.ParticleStride * ParticleIndices[NextIndex]);
			PREFETCH(&NextParticle);
		}

		FVector Size = Particle.Size * Source.Scale;
		INT UVIndex = 0;
		if (bFlipActive == TRUE)
		{
			UVIndex = ParticleSprite_GetUVFlipMode(Size, bSquareAlign);
			Size.X = fabs(Size.X);
			Size.Y = (bSquareAlign == TRUE) ? Size.X : fabs(Size.Y);
		}
		else if (bSquareAlign == TRUE)
		{
			Size.Y = Size.X;
		}

		FFullSubUVPayload* PayloadData = (FFullSubUVPayload*)(((BYTE*)&Particle) + Source.SubUVDataOffset);

		FOrbitChainModuleInstancePayload* LocalOrbitPayload = NULL;
		ParticlePosition = Particle.Location;
		ParticleOldPosition = Particle.OldLocation;

		// Fill in the subUV data...
		for (INT SubUVIdx = 0; SubUVIdx < 4; SubUVIdx++)
		{
			FLOAT UOffset = 0.0f;
			FLOAT VOffset = 0.0f;
			if (Source.bDirectUV)
			{
				switch (SubUVIdx)
				{
				case 1:
					VOffset = PayloadData->Image2HV_UV2Offset.Y;
					break;
				case 2:
					UOffset = PayloadData->Image2HV_UV2Offset.X;
					VOffset = PayloadData->Image2HV_UV2Offset.Y;
					break;
				case 3:
					UOffset = PayloadData->Image2HV_UV2Offset.X;
					break;
				}
				SubU[SubUVIdx] = baseU * (PayloadData->ImageHVInterp_UVOffset.X + UOffset);
				SubV[SubUVIdx] = baseV * (PayloadData->ImageHVInterp_UVOffset.Y + VOffset);
				SubU2[SubUVIdx] = SubU[SubUVIdx];
				SubV2[SubUVIdx] = SubV[SubUVIdx];
			}
			else
			{
				switch (SubUVIdx)
				{
				case 1:
					VOffset = 1.0f;
					break;
				case 2:
					UOffset = 1.0f;
					VOffset = 1.0f;
					break;
				case 3:
					UOffset = 1.0f;
					break;
				}
				SubU[SubUVIdx]  = baseU * (appTruncFloat(PayloadData->ImageHVInterp_UVOffset.X) + UOffset);
				SubV[SubUVIdx]  = baseV * (appTruncFloat(PayloadData->ImageHVInterp_UVOffset.Y) + VOffset);
				SubU2[SubUVIdx] = baseU * (appTruncFloat(PayloadData->Image2HV_UV2Offset.X) + UOffset);
				SubV2[SubUVIdx] = baseV * (appTruncFloat(PayloadData->Image2HV_UV2Offset.Y) + VOffset);
			}
		}

		if (Source.OrbitModuleOffset != 0)
		{
			INT CurrentOffset = Source.OrbitModuleOffset;
			const BYTE* ParticleBase = (const BYTE*)&Particle;
			PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
			OrbitOffset = OrbitPayload.Offset;

			if (Source.bUseLocalSpace == FALSE)
			{
				OrbitOffset = SceneProxy->GetLocalToWorld().TransformNormal(OrbitOffset);
			}
			PrevOrbitOffset = OrbitPayload.PreviousOffset;

			LocalOrbitPayload = &OrbitPayload;

			ParticlePosition += OrbitOffset;
			ParticleOldPosition += PrevOrbitOffset;
		}

		if (Source.CameraPayloadOffset != 0)
		{
			FVector SavedParticlePosition = ParticlePosition;
			GetCameraOffsetFromPayload(Source, SceneProxy->GetLocalToWorld(), Particle, SavedParticlePosition, ParticlePosition, FALSE);
			if (Source.ScreenAlignment == PSA_Velocity)
			{
				// For PSA_Velocity to work correctly, we need to shift the old location as well.
				GetCameraOffsetFromPayload(Source, SceneProxy->GetLocalToWorld(), Particle, SavedParticlePosition, ParticleOldPosition, TRUE);
			}
		}

		if (Source.DynamicParameterDataOffset > 0)
		{
			GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, Particle, DynamicParameterValue);
		}

		// Near/Far Particle Distance Culling
		UpdateParticleDistanceCulling( ParticlePosition, NearCullDistanceSq, NearFadeDistanceSq, FarCullDistanceSq, FarFadeDistanceSq, Particle.Color, Size );

		// 0
		FillVertex = (FParticleSpriteSubUVVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->OldPosition	= ParticleOldPosition;
		FillVertex->Size		= Size;
		FillVertex->Rotation	= Particle.Rotation;
#if !PARTICLES_USE_INDEXED_SPRITES
		FillVertex->SizerIndex	= 0;
#else
		FillVertex->SizerIndex	= UVIndex;
#endif
		FillVertex->Color		= Particle.Color;
#if PARTICLES_USE_INDEXED_SPRITES
		FillVertex->Interp_Sizer[0] = PayloadData->ImageHVInterp_UVOffset.Z;
		FillVertex->Interp_Sizer[1] = 0.12345f;
#else	//#if PARTICLES_USE_INDEXED_SPRITES
		FillVertex->Interp		= PayloadData->ImageHVInterp_UVOffset.Z;
		FillVertex->Padding		= 0.12345f;
#endif	//#if PARTICLES_USE_INDEXED_SPRITES

#if PARTICLES_USE_INDEXED_SPRITES
// 		FillVertex->Offsets[0] = SubU[ParticleSprite_UVIndices[UVIndex][0]];
// 		FillVertex->Offsets[1] = SubV[ParticleSprite_UVIndices[UVIndex][0]];
// 		FillVertex->Offsets[2] = SubU2[ParticleSprite_UVIndices[UVIndex][0]];
// 		FillVertex->Offsets[3] = SubV2[ParticleSprite_UVIndices[UVIndex][0]];
// 		FillVertex->Offsets[0] = SubU[0];
// 		FillVertex->Offsets[1] = SubV[0];
// 		FillVertex->Offsets[2] = SubU2[0];
// 		FillVertex->Offsets[3] = SubV2[0];
		FillVertex->Offsets[0] = appTruncFloat(PayloadData->ImageHVInterp_UVOffset.X);
		FillVertex->Offsets[1] = appTruncFloat(PayloadData->ImageHVInterp_UVOffset.Y);
		FillVertex->Offsets[2] = appTruncFloat(PayloadData->Image2HV_UV2Offset.X);
		FillVertex->Offsets[3] = appTruncFloat(PayloadData->Image2HV_UV2Offset.Y);
		FillVertex->Interp_Sizer[2] = baseU;
		FillVertex->Interp_Sizer[3] = baseV;
#else	//#if PARTICLES_USE_INDEXED_SPRITES
		FillVertex->Tex_U = SubU[ParticleSprite_UVIndices[UVIndex][0]];
		FillVertex->Tex_V = SubV[ParticleSprite_UVIndices[UVIndex][0]];
		FillVertex->Tex_U2 = SubU2[ParticleSprite_UVIndices[UVIndex][0]];
		FillVertex->Tex_V2 = SubV2[ParticleSprite_UVIndices[UVIndex][0]];
		FillVertex->SizeU		= 0.f;
		FillVertex->SizeV		= 0.f;
#endif	//#if PARTICLES_USE_INDEXED_SPRITES
		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleSpriteSubUVVertexDynamicParameter*)TempVert;
			DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
			DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
			DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
			DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
		}
		TempVert += VertexStride;

#if !PARTICLES_USE_INDEXED_SPRITES
		// 1
		FillVertex = (FParticleSpriteSubUVVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->OldPosition	= ParticleOldPosition;
		FillVertex->Size		= Size;
		FillVertex->Rotation	= Particle.Rotation;
		FillVertex->SizerIndex	= 1;
		FillVertex->Color		= Particle.Color;
		FillVertex->Interp		= PayloadData->ImageHVInterp_UVOffset.Z;
		FillVertex->Tex_U = SubU[ParticleSprite_UVIndices[UVIndex][1]];
		FillVertex->Tex_V = SubV[ParticleSprite_UVIndices[UVIndex][1]];
		FillVertex->Tex_U2 = SubU2[ParticleSprite_UVIndices[UVIndex][1]];
		FillVertex->Tex_V2 = SubV2[ParticleSprite_UVIndices[UVIndex][1]];
		FillVertex->SizeU		= 0.f;
		FillVertex->SizeV		= 1.f;
		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleSpriteSubUVVertexDynamicParameter*)TempVert;
			DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
			DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
			DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
			DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
		}
		TempVert += VertexStride;
		// 2
		FillVertex = (FParticleSpriteSubUVVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->OldPosition	= ParticleOldPosition;
		FillVertex->Size		= Size;
		FillVertex->Rotation	= Particle.Rotation;
		FillVertex->SizerIndex	= 2;
		FillVertex->Color		= Particle.Color;
		FillVertex->Interp		= PayloadData->ImageHVInterp_UVOffset.Z;
		FillVertex->Tex_U = SubU[ParticleSprite_UVIndices[UVIndex][2]];
		FillVertex->Tex_V = SubV[ParticleSprite_UVIndices[UVIndex][2]];
		FillVertex->Tex_U2 = SubU2[ParticleSprite_UVIndices[UVIndex][2]];
		FillVertex->Tex_V2 = SubV2[ParticleSprite_UVIndices[UVIndex][2]];
		FillVertex->SizeU		= 1.f;
		FillVertex->SizeV		= 1.f;
		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleSpriteSubUVVertexDynamicParameter*)TempVert;
			DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
			DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
			DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
			DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
		}
		TempVert += VertexStride;
		// 3
		FillVertex = (FParticleSpriteSubUVVertex*)TempVert;
		FillVertex->Position	= ParticlePosition;
		FillVertex->OldPosition	= ParticleOldPosition;
		FillVertex->Size		= Size;
		FillVertex->Rotation	= Particle.Rotation;
		FillVertex->SizerIndex	= 3;
		FillVertex->Color		= Particle.Color;
		FillVertex->Interp		= PayloadData->ImageHVInterp_UVOffset.Z;
		FillVertex->Tex_U = SubU[ParticleSprite_UVIndices[UVIndex][3]];
		FillVertex->Tex_V = SubV[ParticleSprite_UVIndices[UVIndex][3]];
		FillVertex->Tex_U2 = SubU2[ParticleSprite_UVIndices[UVIndex][3]];
		FillVertex->Tex_V2 = SubV2[ParticleSprite_UVIndices[UVIndex][3]];
		FillVertex->SizeU		= 1.f;
		FillVertex->SizeV		= 0.f;
		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleSpriteSubUVVertexDynamicParameter*)TempVert;
			DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
			DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
			DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
			DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
		}
		TempVert += VertexStride;
#endif	//#if !PARTICLES_USE_INDEXED_SPRITES

		if (Indices)
		{
			*Indices++ = (i * 4) + 0;
			*Indices++ = (i * 4) + 2;
			*Indices++ = (i * 4) + 3;
			*Indices++ = (i * 4) + 0;
			*Indices++ = (i * 4) + 1;
			*Indices++ = (i * 4) + 2;
		}

		if (LocalOrbitPayload)
		{
			LocalOrbitPayload->PreviousOffset = OrbitOffset;
		}
	}

	return TRUE;
}

/**
 *	Create the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicSubUVEmitterData::CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	// Create the vertex factory...
	//@todo. Cache these??
	if (VertexFactory == NULL)
	{
		if (bUsesDynamicParameter == FALSE)
		{
			VertexFactory = (FParticleSubUVVertexFactory*)(GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_SubUV));
		}
		else
		{
			VertexFactory = (FParticleSubUVDynamicParameterVertexFactory*)(GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_SubUV_DynamicParameter));
		}
		check(VertexFactory);
	}

	return (VertexFactory != NULL);
}

/**
 *	Release the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicSubUVEmitterData::ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	if (VertexFactory != NULL)
	{
		GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
		VertexFactory = NULL;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
//	FDynamicMeshEmitterData
///////////////////////////////////////////////////////////////////////////////

FDynamicMeshEmitterData::FDynamicMeshEmitterData(const UParticleModuleRequired* RequiredModule)
	: FDynamicSpriteEmitterDataBase(RequiredModule)
	, LastFramePreRendered(-1)
	, StaticMesh( NULL )
	, InstancedMaterialInterface( NULL )
	, InstanceBuffer( NULL )
	, InstancedVertexFactory( NULL )
	, bUseMotionBlurData(FALSE)
	, bShouldUpdateMBTransforms(TRUE)
	, PhysXParticleBuf( NULL )
	, MEMatInstRes()
	, MeshTypeDataOffset(0xFFFFFFFF)
	, EmitterInstanceId(NULL)	
	, bApplyPreRotation(FALSE)
	, RollPitchYaw(0.0f, 0.0f, 0.0f)
	, bUseMeshLockedAxis(FALSE)
	, bUseCameraFacing(FALSE)
	, bApplyParticleRotationAsSpin(FALSE)
	, CameraFacingOption(0)
{
	// only update motion blur transforms if we are not paused
	// bPlayersOnlyPending allows us to keep the particle transforms 
	// from the last ticked frame
	AWorldInfo* WorldInfo = GEngine ? GEngine->GetCurrentWorldInfo() : NULL;
	if (WorldInfo != NULL &&	
		(WorldInfo->bPlayersOnly || WorldInfo->bPlayersOnlyPending) &&
		GSystemSettings.bAllowMotionBlurPause)
	{
		bShouldUpdateMBTransforms = FALSE;		
	}
	else
	{
		bShouldUpdateMBTransforms = TRUE;		
	}	
}

FDynamicMeshEmitterData::~FDynamicMeshEmitterData()
{
	if( PhysXParticleBuf )
	{
		delete PhysXParticleBuf;
		PhysXParticleBuf = NULL;
	}

	if(InstancedVertexFactory)
	{
		InstancedVertexFactory->ReleaseResource();
		delete InstancedVertexFactory;
	}
	if(InstanceBuffer)
	{
		InstanceBuffer->ReleaseResource();
		delete InstanceBuffer;
	}
}

/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicMeshEmitterData::Init( UBOOL bInSelected,
									const FParticleMeshEmitterInstance* InEmitterInstance,
									UStaticMesh* InStaticMesh,
									const UStaticMeshComponent* InStaticMeshComponent,
									UBOOL UseNxFluid )
{
	bSelected = bInSelected;

	// @todo: For replays, currently we're assuming the original emitter instance is bound to the same mesh as
	//        when the replay was generated (safe), and various mesh/material indices are intact.  If
	//        we ever support swapping meshes/material on the fly, we'll need cache the mesh
	//        reference and mesh component/material indices in the actual replay data.

	StaticMesh = InStaticMesh;

	check(Source.ActiveParticleCount < 16 * 1024);	// TTP #33375
	check(Source.ParticleStride < 2 * 1024);	// TTP #3375


	// Build the proxy's LOD data.
	// @todo: We only use the highest LOD for the moment
	LODInfo.Init(InStaticMeshComponent, InEmitterInstance, 0, bSelected);

	// Find the offset to the mesh type data 
	if (InEmitterInstance->MeshTypeData != NULL)
	{
		UParticleModuleTypeDataMesh* MeshTD = InEmitterInstance->MeshTypeData;
		// offset to the mesh emitter type data
		MeshTypeDataOffset = InEmitterInstance->TypeDataOffset;
		// determine if the mesh emitter instance will need a velocity pass for motion blur
		bUseMotionBlurData = MeshTD->bAllowMotionBlur;

		// Setup pre-rotation values...
		if ((MeshTD->Pitch != 0.0f) || (MeshTD->Roll != 0.0f) || (MeshTD->Yaw != 0.0f))
		{
			bApplyPreRotation = TRUE;
			RollPitchYaw = FVector(MeshTD->Roll, MeshTD->Pitch, MeshTD->Yaw);
		}
		else
		{
			bApplyPreRotation = FALSE;
		}

		// Setup the camera facing options
		if (MeshTD->bCameraFacing == TRUE)
		{
			bUseCameraFacing = TRUE;
			CameraFacingOption = MeshTD->CameraFacingOption;
			bApplyParticleRotationAsSpin = MeshTD->bApplyParticleRotationAsSpin;
		}

		// Camera facing trumps locked axis... but can still use it.
		// Setup the locked axis option
		BYTE CheckAxisLockOption = MeshTD->AxisLockOption;
		if ((CheckAxisLockOption >= EPAL_X) && (CheckAxisLockOption <= EPAL_NEGATIVE_Z))
		{
			bUseMeshLockedAxis = TRUE;
			Source.LockedAxis = FVector(
				(CheckAxisLockOption == EPAL_X) ? 1.0f : ((CheckAxisLockOption == EPAL_NEGATIVE_X) ? -1.0f :  0.0),
				(CheckAxisLockOption == EPAL_Y) ? 1.0f : ((CheckAxisLockOption == EPAL_NEGATIVE_Y) ? -1.0f :  0.0),
				(CheckAxisLockOption == EPAL_Z) ? 1.0f : ((CheckAxisLockOption == EPAL_NEGATIVE_Z) ? -1.0f :  0.0)
				);
		}
		else if ((CameraFacingOption >= LockedAxis_ZAxisFacing) && (CameraFacingOption <= LockedAxis_NegativeYAxisFacing))
		{
			// Catch the case where we NEED locked axis...
			bUseMeshLockedAxis = TRUE;
			Source.LockedAxis = FVector(1.0f, 0.0f, 0.0f);
		}
		UParticleModuleTypeDataMeshPhysX* MeshPhysXTD = Cast<UParticleModuleTypeDataMeshPhysX>(MeshTD);
		if( MeshPhysXTD != NULL )
		{
			ZOffset = MeshPhysXTD->ZOffset;
		}
	}
	// ptr to emitter instance used as a unique id for lookup
	EmitterInstanceId = InEmitterInstance;
}

FDynamicMeshEmitterData::FLODInfo::FLODInfo()
{
}

/** Information used by the proxy about a single LOD of the mesh. */
FDynamicMeshEmitterData::FLODInfo::FLODInfo(const UStaticMeshComponent* InStaticMeshComponent,
	const FParticleMeshEmitterInstance* MeshEmitInst, INT LODIndex, UBOOL bSelected)
{
	Init(InStaticMeshComponent, MeshEmitInst, LODIndex, bSelected);
}

void FDynamicMeshEmitterData::FLODInfo::Init(const UStaticMeshComponent* InStaticMeshComponent, 
	const FParticleMeshEmitterInstance* MeshEmitInst, INT LODIndex, UBOOL bSelected)
{
	check(InStaticMeshComponent);

	UMaterialInterface* RequiredMatInst = NULL;

	// Gather the materials applied to the LOD.
	const FStaticMeshRenderData& LODModel = InStaticMeshComponent->StaticMesh->LODModels(LODIndex);
	Elements.Empty(LODModel.Elements.Num());
	for (INT MaterialIndex = 0; MaterialIndex < LODModel.Elements.Num(); MaterialIndex++)
	{
		FElementInfo ElementInfo;

		ElementInfo.MaterialInterface = NULL;

		// Determine the material applied to this element of the LOD.
		UMaterialInterface* MatInst = NULL;

		// The emitter instance Materials array will be filled in with entries from the 
		// MeshMaterial module, if present.
		if (MaterialIndex < MeshEmitInst->CurrentMaterials.Num())
		{
			MatInst = MeshEmitInst->CurrentMaterials(MaterialIndex);
		}

		if (MatInst == NULL)
		{
			if (RequiredMatInst == NULL)
			{
				// Next, check the override material in the required module 
				UParticleLODLevel* CurrLOD = MeshEmitInst->SpriteTemplate->GetLODLevel(MeshEmitInst->CurrentLODLevelIndex);
				if (CurrLOD)
				{
					UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(CurrLOD->TypeDataModule);
					if (MeshTD)
					{
						if (MeshTD->bOverrideMaterial)
						{
							RequiredMatInst = CurrLOD->RequiredModule->Material;
						}
					}
				}
			}
			MatInst = RequiredMatInst;
		}

		if (MatInst == NULL)
		{
			// Next, check the static mesh component itself
			if (MaterialIndex < InStaticMeshComponent->Materials.Num())
			{
				MatInst = InStaticMeshComponent->Materials(MaterialIndex);
			}
		}

		// Safety catch - no material at this point? Use the default engine material.
		if (MatInst == NULL)
		{
			MatInst = GEngine->DefaultMaterial;
		}

		// We better have a material by now!
		check(MatInst);

		//Update Material Interface so that it is stored for the particle rendering dynamic updates in editor windows
#if WITH_EDITOR
		FMobileEmulationMaterialManager::GetManager()->UpdateMaterialInterface(MatInst, FALSE, FALSE);
#endif

		// Set up the element info entry, and add it to the component
		// tracker to prevent garbage collection.
		ElementInfo.MaterialInterface = MatInst;
		MeshEmitInst->Component->SMMaterialInterfaces.AddUniqueItem(MatInst);

		// Store the element info.
		Elements.AddItem(ElementInfo);
	}
}

/**
 *	Create the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicMeshEmitterData::CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	if (GSupportsVertexInstancing)
	{
		if (!InstancedMaterialInterface && StaticMesh)
		{
			FStaticMeshLODInfo& MeshLODInfo = StaticMesh->LODInfo(0);
			InstancedMaterialInterface = StaticMesh->LODModels(0).Elements(0).Material;
		}
	}

	if (InstancedMaterialInterface)
	{
		const FMaterialRenderProxy* MaterialResource = InstancedMaterialInterface->GetRenderProxy(FALSE);
		const FMaterial* Material = MaterialResource ? MaterialResource->GetMaterial() : 0;
		check(Material);
		if (Material && Material->IsUsedWithInstancedMeshParticles())
		{
			InstanceBuffer = new FDynamicMeshEmitterData::FParticleInstancedMeshInstanceBuffer(*this);
			InstancedVertexFactory = new FParticleInstancedMeshVertexFactory;

			InitInstancedResources(Source.ActiveParticleCount);

			return (InstanceBuffer && InstancedVertexFactory);
		}
	}

	return TRUE;
}

/**
 *	Release the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicMeshEmitterData::ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	// nothing to do here...
	return TRUE;
}

INT FDynamicMeshEmitterData::Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_MeshRenderingTime);

	if (bValid == FALSE)
	{
		return 0;
	}

	// determine if the draw interface is currently being used in the velocity rendering pass
	const UBOOL bRenderingVelocities = PDI->IsRenderingVelocities();

	INT NumDraws = 0;
	if (Source.EmitterRenderMode == ERM_Normal)
	{
		if(InstancedVertexFactory)
		{
			RenderInstanced(Proxy, PDI, View, DPGIndex);
			return 1;
		}
		
		CameraPosition = View->ViewOrigin;

		const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(0);
		TArray<INT> ValidElementIndices;

		UBOOL bNoValidElements = TRUE;
		INT MaterialSelectionIdx = (GIsGame == TRUE) ? 0 : (GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? bSelected : 0);
		for (INT LODIndex = 0; LODIndex < 1; LODIndex++)
		{
			for (INT ElementIndex = 0; ElementIndex < LODModel.Elements.Num(); ElementIndex++)
			{
				FMeshEmitterMaterialInstanceResource* MIRes = &(MEMatInstRes[MaterialSelectionIdx](ElementIndex));
				check(MIRes);

				// If the material is ignored by the PDI (or not there at all...), 
				// do not add it to the list of valid elements.
				if ((MIRes->Parent && !PDI->IsMaterialIgnored(MIRes->Parent)) || (View->Family->ShowFlags & SHOW_Wireframe))
				{
					ValidElementIndices.AddItem(ElementIndex);
					bNoValidElements = FALSE;
				}
				else
				{
					ValidElementIndices.AddItem(-1);
				}
			}
		}

		if (bNoValidElements == TRUE)
		{
			// No valid materials... quick out
			return 0;
		}

		EParticleSubUVInterpMethod eSubUVMethod = (EParticleSubUVInterpMethod)(Source.SubUVInterpMethod);

		const UBOOL bWireframe = AllowDebugViewmodes() 
			&& (View->Family->ShowFlags & SHOW_Wireframe) 
			&& !(View->Family->ShowFlags & SHOW_Materials);

		FMatrix kMat(FMatrix::Identity);
		FMatrix kMatInverse(FMatrix::Identity);
		// Reset velocity and size.

		INT ParticleCount = Source.ActiveParticleCount;
		if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
		{
			ParticleCount = Source.MaxDrawCount;
		}

		FMatrix Local2World;

		FTranslationMatrix kTransMat(FVector(0.0f));
		FScaleMatrix kScaleMat(FVector(1.0f));
		FVector Location;
		FVector ScaledSize;
		FVector	DirToCamera;
		FVector	LocalSpaceFacingAxis;
		FVector	LocalSpaceUpAxis;

		FMeshBatch Mesh;
		FMeshBatchElement& BatchElement = Mesh.Elements(0);
		Mesh.DynamicVertexData = NULL;
		Mesh.LCI = NULL;
		Mesh.UseDynamicData = FALSE;
		Mesh.ReverseCulling = Proxy->GetLocalToWorldDeterminant() < 0.0f ? TRUE : FALSE;
		Mesh.CastShadow = Proxy->GetCastShadow();
		Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)DPGIndex;
		Mesh.bUsePreVertexShaderCulling = FALSE;
		Mesh.PlatformMeshData       = NULL;
		Mesh.bUseDownsampledTranslucency = ShouldRenderDownsampled(View, Proxy->GetBounds());

		// retrieve the previous frame's motion blur info for a particle
		FParticleEmitterInstanceMotionBlurInfo* MBEmitterInstanceInfo = NULL;		
		TMap<INT, FMeshElementMotionBlurInfo> NewParticleMBInfoMap;
		if (bRenderingVelocities && bUseMotionBlurData)
		{
			// find the index of the current view in the list of views from the view family
			INT CurrentViewIdx = 0;
			for (INT ViewIdx=0; ViewIdx < View->Family->Views.Num(); ViewIdx++)
			{
				if (View == View->Family->Views(ViewIdx))
				{
					CurrentViewIdx = ViewIdx;
					break;
				}			
			}
			// the motion blur info is stored int he particle system component
			UParticleSystemComponent* ParticleSystemComponent = CastChecked<UParticleSystemComponent>(Proxy->GetPrimitiveSceneInfo()->Component);
			// find/create the motion blur info for the current view
			if (ParticleSystemComponent->ViewMBInfoArray.Num() < View->Family->Views.Num())
			{
				ParticleSystemComponent->ViewMBInfoArray.AddZeroed(View->Family->Views.Num());
			}
			FViewParticleEmitterInstanceMotionBlurInfo& MBViewInfo = ParticleSystemComponent->ViewMBInfoArray(CurrentViewIdx);
			// find/create the MB info for the emitter instance by ptr id
			MBEmitterInstanceInfo = MBViewInfo.EmitterInstanceMBInfoMap.Find(EmitterInstanceId);
			if (MBEmitterInstanceInfo == NULL)
			{
				FParticleEmitterInstanceMotionBlurInfo TempMBInfo;
				MBEmitterInstanceInfo = &MBViewInfo.EmitterInstanceMBInfoMap.Set(EmitterInstanceId,TempMBInfo);
			}
			check(MBEmitterInstanceInfo != NULL);
		}

		FQuat PointTo;
		FRotator kLockedAxisRotator = FRotator::ZeroRotator;
		if (bUseMeshLockedAxis == TRUE)
		{
			// facing axis is taken to be the local x axis.	
			PointTo = FQuatFindBetween(FVector(1,0,0), Source.LockedAxis);
		}

		FVector CameraFacingOpVector = FVector::ZeroVector;
		if (CameraFacingOption != XAxisFacing_NoUp)
		{
			switch (CameraFacingOption)
			{
			case XAxisFacing_ZUp:
				CameraFacingOpVector = FVector( 0.0f, 0.0f, 1.0f);
				break;
			case XAxisFacing_NegativeZUp:
				CameraFacingOpVector = FVector( 0.0f, 0.0f,-1.0f);
				break;
			case XAxisFacing_YUp:
				CameraFacingOpVector = FVector( 0.0f, 1.0f, 0.0f);
				break;
			case XAxisFacing_NegativeYUp:
				CameraFacingOpVector = FVector( 0.0f,-1.0f, 0.0f);
				break;
			case LockedAxis_YAxisFacing:
			case VelocityAligned_YAxisFacing:
				CameraFacingOpVector = FVector(0.0f, 1.0f, 0.0f);
				break;
			case LockedAxis_NegativeYAxisFacing:
			case VelocityAligned_NegativeYAxisFacing:
				CameraFacingOpVector = FVector(0.0f,-1.0f, 0.0f);
				break;
			case LockedAxis_ZAxisFacing:
			case VelocityAligned_ZAxisFacing:
				CameraFacingOpVector = FVector(0.0f, 0.0f, 1.0f);
				break;
			case LockedAxis_NegativeZAxisFacing:
			case VelocityAligned_NegativeZAxisFacing:
				CameraFacingOpVector = FVector(0.0f, 0.0f,-1.0f);
				break;
			}
		}

		for (INT i = ParticleCount - 1; i >= 0; i--)
		{
			const INT	CurrentIndex	= Source.ParticleIndices(i);
			const BYTE* ParticleBase	= Source.ParticleData.GetData() + CurrentIndex * Source.ParticleStride;
			FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
			// retrieve the mesh payload data that contains the unique Id for each particle
			FMeshTypeDataPayload* MeshDataPayload = NULL;
			FMeshElementMotionBlurInfo* MBParticleInfo = NULL;
			if (bRenderingVelocities &&
				bUseMotionBlurData &&
				MeshTypeDataOffset != 0xFFFFFFFF)
			{
				// offset into the particle data to get the mesh payload
				MeshDataPayload = (FMeshTypeDataPayload*)(ParticleBase + MeshTypeDataOffset);
				// unique Id for each particle used to find last frame's motion blur info for the particle
				// the particle Id is set at spawn time and persists during the lifetime of the particle
				MBParticleInfo = MBEmitterInstanceInfo->ParticleMBInfoMap.Find(MeshDataPayload->ParticleId);
			}

			if (Particle.RelativeTime < 1.0f)
			{
				{
				FVector ParticlePosition(Particle.Location);
				if (Source.CameraPayloadOffset != 0)
				{
					GetCameraOffsetFromPayload(Source, SceneProxy->GetLocalToWorld(), Particle, ParticlePosition, ParticlePosition, FALSE);
				}
				kTransMat.M[3][0] = ParticlePosition.X;
				kTransMat.M[3][1] = ParticlePosition.Y;
				kTransMat.M[3][2] = ParticlePosition.Z;
				ScaledSize = Particle.Size * Source.Scale;
				kScaleMat.M[0][0] = ScaledSize.X;
				kScaleMat.M[1][1] = ScaledSize.Y;
				kScaleMat.M[2][2] = ScaledSize.Z;

				FRotator kRotator(0,0,0);
				Local2World = Proxy->GetLocalToWorld();
				if (bUseCameraFacing == TRUE)
				{
					Location = ParticlePosition;
					FVector	VelocityDirection = Particle.Velocity;
					VelocityDirection.Normalize();
					if (Source.bUseLocalSpace)
					{
						UBOOL bClearLocal2World = FALSE;
						// Transform the location to world space
						Location = Local2World.TransformFVector(Location);
						if (CameraFacingOption <= XAxisFacing_NegativeYUp)
						{
							bClearLocal2World = TRUE;
						}
						else if (CameraFacingOption >= VelocityAligned_ZAxisFacing)
						{
							bClearLocal2World = TRUE;
							VelocityDirection = Local2World.TransformNormal(VelocityDirection);
						}

						if (bClearLocal2World)
						{
							// Set the translation matrix to the location
							kTransMat.SetOrigin(Location);
							// Set Local2World to identify to remove any rotational information
							Local2World.SetIdentity();
						}
					}
					DirToCamera	= View->ViewOrigin - Location;
					DirToCamera.Normalize();
					if (DirToCamera.SizeSquared() <	0.5f)
					{
						// Assert possible if DirToCamera is not normalized
						DirToCamera	= FVector(1,0,0);
					}

					UBOOL bFacingDirectionIsValid = TRUE;
					if (CameraFacingOption != XAxisFacing_NoUp)
					{
						FVector FacingDir;
						FVector DesiredDir;

						if ((CameraFacingOption >= VelocityAligned_ZAxisFacing) &&
							(CameraFacingOption <= VelocityAligned_NegativeYAxisFacing))
						{
							if (VelocityDirection.IsNearlyZero())
							{
								// We have to fudge it
								bFacingDirectionIsValid = FALSE;
							}
							// Velocity align the X-axis, and camera face the selected axis
							PointTo = FQuatFindBetween(FVector(1.0f, 0.0f, 0.0f), VelocityDirection);
							FacingDir = VelocityDirection;
							DesiredDir = DirToCamera;
						}
						else if (CameraFacingOption <= XAxisFacing_NegativeYUp)
						{
							// Camera face the X-axis, and point the selected axis towards the world up
							PointTo = FQuatFindBetween(FVector(1,0,0), DirToCamera);
							FacingDir = DirToCamera;
							DesiredDir = FVector(0,0,1);
						}
						else
						{
							// Align the X-axis with the selected LockAxis, and point the selected axis towards the camera
							// PointTo will contain quaternion for locked axis rotation.
							FacingDir = Source.LockedAxis;
							DesiredDir = DirToCamera;
						}

						FVector	DirToDesiredInRotationPlane = DesiredDir - ((DesiredDir | FacingDir) * FacingDir);
						DirToDesiredInRotationPlane.Normalize();
						FQuat FacingRotation = FQuatFindBetween(PointTo.RotateVector(CameraFacingOpVector), DirToDesiredInRotationPlane);
						PointTo = FacingRotation * PointTo;

						// Add in additional rotation about either the directional or camera facing axis
						if (bApplyParticleRotationAsSpin)
						{
							if (bFacingDirectionIsValid)
							{
								FQuat AddedRotation = FQuat(FacingDir, Particle.Rotation);
								kLockedAxisRotator = FRotator(AddedRotation * PointTo);
							}
						}
						else
						{
							FQuat AddedRotation = FQuat(DirToCamera, Particle.Rotation);
							kLockedAxisRotator = FRotator(AddedRotation * PointTo);
						}
					}
					else
					{
						PointTo = FQuatFindBetween(FVector(1,0,0), DirToCamera);
						// Add in additional rotation about facing axis
						FQuat AddedRotation = FQuat(DirToCamera, Particle.Rotation);
						kLockedAxisRotator = FRotator(AddedRotation * PointTo);
					}
				}
				else if (bUseMeshLockedAxis == TRUE)
				{
					// Add any 'sprite rotation' about the locked axis
					FQuat AddedRotation = FQuat(Source.LockedAxis, Particle.Rotation);
					kLockedAxisRotator = FRotator(AddedRotation * PointTo);
				}
				else if (Source.ScreenAlignment == PSA_TypeSpecific)
				{
					Location = ParticlePosition;
					if (Source.bUseLocalSpace)
					{
						// Transform the location to world space
						Location = Local2World.TransformFVector(Location);
						kTransMat.SetOrigin(Location);
						Local2World.SetIdentity();
					}
					DirToCamera	= View->ViewOrigin - Location;
					DirToCamera.Normalize();
					if (DirToCamera.SizeSquared() <	0.5f)
					{
						// Assert possible if DirToCamera is not normalized
						DirToCamera	= FVector(1,0,0);
					}

					LocalSpaceFacingAxis = FVector(1,0,0); // facing axis is taken to be the local x axis.	
					LocalSpaceUpAxis = FVector(0,0,1); // up axis is taken to be the local z axis

					if (Source.MeshAlignment == PSMA_MeshFaceCameraWithLockedAxis)
					{
						// TODO: Allow an arbitrary	vector to serve	as the locked axis

						// For the locked axis behavior, only rotate to	face the camera	about the
						// locked direction, and maintain the up vector	pointing towards the locked	direction
						// Find	the	rotation that points the localupaxis towards the targetupaxis
						FQuat PointToUp	= FQuatFindBetween(LocalSpaceUpAxis, Source.LockedAxis);

						// Add in rotation about the TargetUpAxis to point the facing vector towards the camera
						FVector	DirToCameraInRotationPlane = DirToCamera - ((DirToCamera | Source.LockedAxis)*Source.LockedAxis);
						DirToCameraInRotationPlane.Normalize();
						FQuat PointToCamera	= FQuatFindBetween(PointToUp.RotateVector(LocalSpaceFacingAxis), DirToCameraInRotationPlane);

						// Set kRotator	to the composed	rotation
						FQuat MeshRotation = PointToCamera*PointToUp;
						kRotator = FRotator(MeshRotation);
						}
						else
						if (Source.MeshAlignment == PSMA_MeshFaceCameraWithSpin)
						{
							// Implement a tangent-rotation	version	of point-to-camera.	 The facing	direction points to	the	camera,
							// with	no roll, and has addtional sprite-particle rotation	about the tangential axis
							// (c.f. the roll rotation is about	the	radial axis)

							// Find	the	rotation that points the facing	axis towards the camera
							FRotator PointToRotation = FRotator(FQuatFindBetween(LocalSpaceFacingAxis, DirToCamera));

							// When	constructing the rotation, we need to eliminate	roll around	the	dirtocamera	axis,
							// otherwise the particle appears to rotate	around the dircamera axis when it or the camera	moves
							PointToRotation.Roll = 0;

							// Add in the tangential rotation we do	want.
							FVector	vPositivePitch = FVector(0,0,1); //	this is	set	by the rotator's yaw/pitch/roll	reference frame
							FVector	vTangentAxis = vPositivePitch^DirToCamera;
							vTangentAxis.Normalize();
							if (vTangentAxis.SizeSquared() < 0.5f)
							{
								vTangentAxis = FVector(1,0,0); // assert is	possible if	FQuat axis/angle constructor is	passed zero-vector
							}

							FQuat AddedTangentialRotation =	FQuat(vTangentAxis,	Particle.Rotation);

							// Set kRotator	to the composed	rotation
							FQuat MeshRotation = AddedTangentialRotation*PointToRotation.Quaternion();
							kRotator = FRotator(MeshRotation);
						}
						else
						//if (MeshAlignment == PSMA_MeshFaceCameraWithRoll)
						{
							// Implement a roll-rotation version of	point-to-camera.  The facing direction points to the camera,
							// with	no roll, and then rotates about	the	direction_to_camera	by the spriteparticle rotation.

							// Find	the	rotation that points the facing	axis towards the camera
							FRotator PointToRotation = FRotator(FQuatFindBetween(LocalSpaceFacingAxis, DirToCamera));

							// When	constructing the rotation, we need to eliminate	roll around	the	dirtocamera	axis,
							// otherwise the particle appears to rotate	around the dircamera axis when it or the camera	moves
							PointToRotation.Roll = 0;

							// Add in the roll we do want.
							FQuat AddedRollRotation	= FQuat(DirToCamera, Particle.Rotation);

							// Set kRotator	to the composed	rotation
							FQuat MeshRotation = AddedRollRotation*PointToRotation.Quaternion();
							kRotator = FRotator(MeshRotation);
						}
					}
					else if (Source.bMeshRotationActive)
					{
						FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + Source.MeshRotationOffset);
						kRotator = FRotator::MakeFromEuler(PayloadData->Rotation);
					}
						else
					{
						FLOAT fRot = Particle.Rotation * 180.0f / PI;
						FVector kRotVec = FVector(fRot, fRot, fRot);
						kRotator = FRotator::MakeFromEuler(kRotVec);
					}

					FRotationMatrix kRotMat(kRotator);
					if (bApplyPreRotation == TRUE)
					{
						if ((bUseCameraFacing == TRUE) || (bUseMeshLockedAxis == TRUE))
						{
							kMat = FRotationMatrix(FRotator::MakeFromEuler(RollPitchYaw)) * kScaleMat * FRotationMatrix(kLockedAxisRotator) * kRotMat * kTransMat;
						}
						else
						{
							kMat = FRotationMatrix(FRotator::MakeFromEuler(RollPitchYaw)) * kScaleMat * kRotMat * kTransMat;
						}
					}
					else if ((bUseCameraFacing == TRUE) || (bUseMeshLockedAxis == TRUE))
					{
						kMat = kScaleMat * FRotationMatrix(kLockedAxisRotator) * kRotMat * kTransMat;
					}
					else
					{
						kMat = kScaleMat * kRotMat * kTransMat;
					}

					FVector OrbitOffset(0.0f, 0.0f, 0.0f);
					if (Source.OrbitModuleOffset != 0)
					{
						INT CurrentOffset = Source.OrbitModuleOffset;
						PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
						OrbitOffset = OrbitPayload.Offset;
						if (Source.bUseLocalSpace == FALSE)
						{
							OrbitOffset = SceneProxy->GetLocalToWorld().TransformNormal(OrbitOffset);
						}

						FTranslationMatrix OrbitMatrix(OrbitOffset);
						kMat *= OrbitMatrix;
					}

					if (Source.bUseLocalSpace)
					{
						kMat *= Local2World;
					}
					kMatInverse = kMat.InverseSafe();
				}
				UBOOL bBadParent = FALSE;

				//@todo. Handle LODs...
				//for (INT LODIndex = 0; LODIndex < MeshData->LODs.Num(); LODIndex++)
				for (INT LODIndex = 0; LODIndex < 1; LODIndex++)
				{
					for (INT ValidIndex = 0; ValidIndex < ValidElementIndices.Num(); ValidIndex++)
					{
						INT ElementIndex = ValidElementIndices(ValidIndex);
						if (ElementIndex == -1)
						{
							continue;
						}

						FMeshEmitterMaterialInstanceResource* MIRes = &(MEMatInstRes[MaterialSelectionIdx](ElementIndex));
						const FStaticMeshElement& Element = LODModel.Elements(ElementIndex);
						if ((Element.NumTriangles == 0) || (MIRes == NULL) || (MIRes->Parent == NULL))
						{
							//@todo. This should never happen... but it does.
							continue;
						}

						MIRes->Param_MeshEmitterVertexColor = Particle.Color;
						if (Source.SubUVInterpMethod != PSUVIM_None)
						{
							FFullSubUVPayload* SubUVPayload = (FFullSubUVPayload*)(((BYTE*)&Particle) + Source.SubUVDataOffset);

							MIRes->Param_TextureOffsetParameter = 
								FLinearColor(SubUVPayload->ImageHVInterp_UVOffset.X, SubUVPayload->ImageHVInterp_UVOffset.Y, 
									SubUVPayload->ImageHVInterp_UVOffset.Z, 0.0f);
							MIRes->Param_TextureOffset1Parameter = 
								FLinearColor(SubUVPayload->Image2HV_UV2Offset.X, SubUVPayload->Image2HV_UV2Offset.Y, 0.0f, 0.0f);

							if (Source.bScaleUV)
							{
								MIRes->Param_TextureScaleParameter = 
									FLinearColor((1.0f / (FLOAT)Source.SubImages_Horizontal),
									(1.0f / (FLOAT)Source.SubImages_Vertical),
									0.0f, 0.0f);
							}
							else
							{
								MIRes->Param_TextureScaleParameter = 
									FLinearColor(1.0f, 1.0f, 0.0f, 0.0f);
							}

							if (GUsingMobileRHI)
							{
								FVector2D SubUVScale(MIRes->Param_TextureScaleParameter.R, MIRes->Param_TextureScaleParameter.G);
								FVector2D SubUVOffset(
									MIRes->Param_TextureOffsetParameter.R,
									MIRes->Param_TextureOffsetParameter.G);

								TMatrix<3,3> OverrideTransform;
								//Rotator
								OverrideTransform.M[0][0] = SubUVScale.X;
								OverrideTransform.M[0][1] = 0.0f;
								OverrideTransform.M[1][0] = 0.0f;
								OverrideTransform.M[1][1] = SubUVScale.Y;
								//Translate
								OverrideTransform.M[2][0] = SubUVOffset.X;
								OverrideTransform.M[2][1] = SubUVOffset.Y;
								//Set the rest to identity
								OverrideTransform.M[0][2] = 0.0f;
								OverrideTransform.M[1][2] = 0.0f;
								OverrideTransform.M[2][2] = 1.0f;

								RHISetMobileTextureTransformOverride(OverrideTransform);
							}
						}

						if (Source.DynamicParameterDataOffset > 0)
						{
							FEmitterDynamicParameterPayload* DynPayload = ((FEmitterDynamicParameterPayload*)((BYTE*)(&Particle) + Source.DynamicParameterDataOffset));
							MIRes->Param_MeshEmitterDynamicParameter.R = DynPayload->DynamicParameterValue.X;
							MIRes->Param_MeshEmitterDynamicParameter.G = DynPayload->DynamicParameterValue.Y;
							MIRes->Param_MeshEmitterDynamicParameter.B = DynPayload->DynamicParameterValue.Z;
							MIRes->Param_MeshEmitterDynamicParameter.A = DynPayload->DynamicParameterValue.W;
						}
						else
						{
							MIRes->Param_MeshEmitterDynamicParameter.R = 1.0f;
							MIRes->Param_MeshEmitterDynamicParameter.G = 1.0f;
							MIRes->Param_MeshEmitterDynamicParameter.B = 1.0f;
							MIRes->Param_MeshEmitterDynamicParameter.A = 1.0f;
						}

						// Draw the static mesh elements.
						Mesh.VertexFactory = &LODModel.VertexFactory;
						BatchElement.LocalToWorld = kMat;
						BatchElement.WorldToLocal = kMatInverse;

						if (bRenderingVelocities &&
							bUseMotionBlurData &&							
							MeshDataPayload != NULL)
						{
							// use last frame's particle motion blur transforms
							// note that this will be NULL when the particle first spawns
							Mesh.MBInfo = MBParticleInfo;
							if (bShouldUpdateMBTransforms)
							{
								// store the current frame's transform for the particle
								FMeshElementMotionBlurInfo NewMBInfo;
								NewMBInfo.PreviousLocalToWorld = BatchElement.LocalToWorld;
								NewParticleMBInfoMap.Set(MeshDataPayload->ParticleId,NewMBInfo);
							}
						}

#if WITH_MOBILE_RHI || WITH_EDITOR
						//If there is dynamic Time variable use that to recalculate the mobile texture transform.
						if (Source.DynamicParameterDataOffset > 0)
						{

							INT CurrentOffset = Source.DynamicParameterDataOffset;
							PARTICLE_ELEMENT(FEmitterDynamicParameterPayload, DynamicParameterPayload);

							// TimeIndex of -1 indicates no Dynamic Parameter with name Time.
							if (DynamicParameterPayload.TimeIndex != -1)
							{
								//Get the current value for the dynamic variable time								
								FLOAT MaterialTime = DynamicParameterPayload.DynamicParameterValue[DynamicParameterPayload.TimeIndex];

#if WITH_EDITOR
								//Update the in editor material with the dynamic time if emulating mobile rendering
								if (GEmulateMobileRendering == TRUE)
								{
									FMobileEmulationMaterialManager::GetManager()->SetupMobileDynamicTimeParameter(MIRes->Parent, MaterialTime);
								}

#endif //WITH_EDITOR
#if WITH_MOBILE_RHI
								//Recaculate the mobile materials Transform using the dynamic time
								UMaterialInterface* MatIF = LODModel.Elements(ElementIndex).Material;

								TMatrix<3,3> OverrideTransform;

								GetMobileTextureTransformHelper(MatIF, MaterialTime, OverrideTransform);

								RHISetMobileTextureTransformOverride(OverrideTransform);
#endif //WITH_MOBILE_RHI
							}
						}

#endif //WITH_MOBILE_RHI || WITH_EDITOR

#if WITH_EDITOR
						// Set color for particle emulation
						if (GEmulateMobileRendering == TRUE)
						{
							FMobileEmulationMaterialManager::GetManager()->SetMeshParticleColor(MIRes->Parent, Particle.Color);
						}
#endif

						BatchElement.FirstIndex = Element.FirstIndex;
						BatchElement.MinVertexIndex = Element.MinVertexIndex;
						BatchElement.MaxVertexIndex = Element.MaxVertexIndex;
						if( bWireframe && LODModel.WireframeIndexBuffer.IsInitialized() )
						{
							Mesh.bWireframe = TRUE;
							BatchElement.IndexBuffer = &LODModel.WireframeIndexBuffer;
							Mesh.MaterialRenderProxy = Proxy->GetDeselectedWireframeMatInst();
							Mesh.Type = PT_LineList;
							BatchElement.NumPrimitives = LODModel.WireframeIndexBuffer.Indices.Num() / 2;
						}
						else
						{
							Mesh.bWireframe = FALSE;
							BatchElement.IndexBuffer = &LODModel.IndexBuffer;
							Mesh.MaterialRenderProxy = MIRes;
							Mesh.Type = PT_TriangleList;
							BatchElement.NumPrimitives = Element.NumTriangles;
						}

						NumDraws += PDI->DrawMesh(Mesh);

#if WITH_EDITOR
						// Reset in case this is shared by a sprite particle as well
						if (GEmulateMobileRendering == TRUE)
						{
							FMobileEmulationMaterialManager::GetManager()->SetMeshParticleColor(MIRes->Parent, FLinearColor::White);
						}
#endif
					}
				}
			}
			else
			{
				// Remove it from the scene???
			}
		}

		if (bRenderingVelocities && 
			bUseMotionBlurData && 
			bShouldUpdateMBTransforms)
		{
			// replace the motion blur transform data with the new transforms from the current frame
			if (NewParticleMBInfoMap.Num() > 0)
			{
				MBEmitterInstanceInfo->ParticleMBInfoMap = NewParticleMBInfoMap;
			}
			else
			{
				MBEmitterInstanceInfo->ParticleMBInfoMap.Empty();
			}			
		}
	}
	else
	if (Source.EmitterRenderMode == ERM_Point)
	{
		RenderDebug(PDI, View, DPGIndex, FALSE);
	}
	else
	if (Source.EmitterRenderMode == ERM_Cross)
	{
		RenderDebug(PDI, View, DPGIndex, TRUE);
	}

	return NumDraws;
}

/**
 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
 *  Only called for primitives that are visible and have bDynamicRelevance
 *
 *	@param	Proxy			The 'owner' particle system scene proxy
 *	@param	ViewFamily		The ViewFamily to pre-render for
 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
 *	@param	FrameNumber		The frame number of this pre-render
 */
void FDynamicMeshEmitterData::PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber)
{
	if (bValid == FALSE)
	{
		return;
	}

	// Mesh emitters don't actually care about the view...
	// They just need to setup their material instances.
	if (LastFramePreRendered != FrameNumber)
	{
		if (Source.EmitterRenderMode == ERM_Normal)
		{
			// The material setup only needs to be done once per-frame as it is view-independent.
			const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(0);
			//@todo. Handle LODs...
			for (INT ElementIndex = 0; ElementIndex < LODModel.Elements.Num(); ElementIndex++)
			{
				FMeshEmitterMaterialInstanceResource* NewMIRes[2] = {NULL,NULL};
				if (ElementIndex < MEMatInstRes[0].Num())
				{
					NewMIRes[0] = &(MEMatInstRes[0](ElementIndex));
				}
				else
				{
					NewMIRes[0] = new(MEMatInstRes[0]) FMeshEmitterMaterialInstanceResource();
				}
				if (ElementIndex < MEMatInstRes[1].Num())
				{
					NewMIRes[1] = &(MEMatInstRes[1](ElementIndex));
				}
				else
				{
					NewMIRes[1] = new(MEMatInstRes[1]) FMeshEmitterMaterialInstanceResource();
				}
				check(NewMIRes[0] && NewMIRes[1]);

				// Set the parent of our mesh material instance constant...
				NewMIRes[0]->Parent = NULL;
				NewMIRes[1]->Parent = NULL;

				// If it has been stored off when we generated the dynamic data, use that
				if (ElementIndex < LODInfo.Elements.Num())
				{
					const FDynamicMeshEmitterData::FLODInfo::FElementInfo& Info = LODInfo.Elements(ElementIndex);
					if (Info.MaterialInterface)
					{
						NewMIRes[0]->Parent = Info.MaterialInterface->GetRenderProxy(FALSE);
						if (GIsGame == FALSE)
						{
							NewMIRes[1]->Parent = Info.MaterialInterface->GetRenderProxy(TRUE);
						}
						else
						{
							NewMIRes[1]->Parent = NewMIRes[0]->Parent;
						}
					}
				}

				// Otherwise, try grabbing it from the mesh itself.
				if (NewMIRes[0]->Parent == NULL)
				{
					UMaterialInterface* MatIF = LODModel.Elements(ElementIndex).Material;
					NewMIRes[0]->Parent = MatIF ? MatIF->GetRenderProxy(0) : NULL;
					if (GIsGame == FALSE)
					{
						NewMIRes[1]->Parent = MatIF ? MatIF->GetRenderProxy(TRUE) : NULL;
					}
					else
					{
						NewMIRes[1]->Parent = NewMIRes[0]->Parent;
					}
				}
			}
		}
		LastFramePreRendered = FrameNumber;
	}
}

void FDynamicMeshEmitterData::RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses)
{
		FDynamicSpriteEmitterDataBase::RenderDebug(PDI, View, DPGIndex, bCrosses);
}

/** Render using hardware instancing. */
void FDynamicMeshEmitterData::RenderInstanced(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex)
{
	check(InstancedMaterialInterface);
	
	const FStaticMeshRenderData&       LODModel = StaticMesh->LODModels(0);

	FMeshBatch MeshElement;
	FMeshBatchElement& BatchElement = MeshElement.Elements(0);
	BatchElement.IndexBuffer   = &LODModel.IndexBuffer;
	MeshElement.VertexFactory = InstancedVertexFactory;
	
	MeshElement.MaterialRenderProxy = InstancedMaterialInterface->GetRenderProxy(FALSE);
	
	BatchElement.LocalToWorld = FMatrix::Identity;
	BatchElement.WorldToLocal = FMatrix::Identity;
	
	BatchElement.FirstIndex     = 0;
	BatchElement.NumPrimitives  = LODModel.IndexBuffer.Indices.Num() / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = LODModel.NumVertices - 1;
	
	MeshElement.Type               = PT_TriangleList;
	MeshElement.DepthPriorityGroup = (ESceneDepthPriorityGroup)DPGIndex;

	MeshElement.bUsePreVertexShaderCulling = FALSE;
	MeshElement.PlatformMeshData           = NULL;
	
	// TSC - deep down in FMeshDrawingPolicy::DrawMesh, MeshElement.LocalToWorld
	//  is ignored. so, we have to carefully change the view itself...
	const FVector PreViewTranslationBackup = View->PreViewTranslation;
	FSceneView* ModifiedView = const_cast<FSceneView*>(View);
	ModifiedView->PreViewTranslation.Z += ZOffset;
	PDI->DrawMesh(MeshElement);

	// ...and when we're done, restore it back to its *exact* prior state (-= ZOffset loses precision)
	ModifiedView->PreViewTranslation = PreViewTranslationBackup;
}

/** Initialized the vertex factory for a specific number of instances. */
void FDynamicMeshEmitterData::InitInstancedResources(UINT NumInstances)
{
	// Initialize the instance buffer.
	InstanceBuffer->InitResource();
	
	const FStaticMeshRenderData& LODModel = StaticMesh->LODModels(0);
	FParticleInstancedMeshVertexFactory::DataType VertexFactoryData;
	
	VertexFactoryData.PositionComponent = FVertexStreamComponent(
		&LODModel.PositionVertexBuffer,
		STRUCT_OFFSET(FPositionVertex,Position),
		LODModel.PositionVertexBuffer.GetStride(),
		VET_Float3
		);
	VertexFactoryData.TangentBasisComponents[0] = FVertexStreamComponent(
		&LODModel.VertexBuffer,
		STRUCT_OFFSET(FStaticMeshFullVertex,TangentX),
		LODModel.VertexBuffer.GetStride(),
		VET_PackedNormal
		);
	VertexFactoryData.TangentBasisComponents[1] = FVertexStreamComponent(
		&LODModel.VertexBuffer,
		STRUCT_OFFSET(FStaticMeshFullVertex,TangentZ),
		LODModel.VertexBuffer.GetStride(),
		VET_PackedNormal
		);
	
	if( !LODModel.VertexBuffer.GetUseFullPrecisionUVs() )
	{
		VertexFactoryData.TextureCoordinateComponent = FVertexStreamComponent(
			&LODModel.VertexBuffer,
			STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>,UVs[0]),
			LODModel.VertexBuffer.GetStride(),
			VET_Half2
			);
	}
	else
	{
		VertexFactoryData.TextureCoordinateComponent = FVertexStreamComponent(
			&LODModel.VertexBuffer,
			STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>,UVs[0]),
			LODModel.VertexBuffer.GetStride(),
			VET_Float2
			);
	}
	
	VertexFactoryData.InstanceOffsetComponent = FVertexStreamComponent(
		InstanceBuffer,
		STRUCT_OFFSET(FParticleInstancedMeshInstance,Location),
		sizeof(FParticleInstancedMeshInstance),
		VET_Float3,
		TRUE
		);
	VertexFactoryData.InstanceAxisComponents[0] = FVertexStreamComponent(
		InstanceBuffer,
		STRUCT_OFFSET(FParticleInstancedMeshInstance,XAxis),
		sizeof(FParticleInstancedMeshInstance),
		VET_Float3,
		TRUE
		);
	VertexFactoryData.InstanceAxisComponents[1] = FVertexStreamComponent(
		InstanceBuffer,
		STRUCT_OFFSET(FParticleInstancedMeshInstance,YAxis),
		sizeof(FParticleInstancedMeshInstance),
		VET_Float3,
		TRUE
		);
	VertexFactoryData.InstanceAxisComponents[2] = FVertexStreamComponent(
		InstanceBuffer,
		STRUCT_OFFSET(FParticleInstancedMeshInstance,ZAxis),
		sizeof(FParticleInstancedMeshInstance),
		VET_Float3,
		TRUE
		);
	VertexFactoryData.ColorComponent = FVertexStreamComponent(
		InstanceBuffer,
		STRUCT_OFFSET(FParticleInstancedMeshInstance,Color),
		sizeof(FParticleInstancedMeshInstance),
		VET_Float4,
		TRUE
		);
	VertexFactoryData.NumVerticesPerInstance = LODModel.NumVertices;
	VertexFactoryData.NumInstances = NumInstances;

	InstancedVertexFactory->SetData(VertexFactoryData);
	InstancedVertexFactory->InitResource();
}


// FRenderResource interface.
void FDynamicMeshEmitterData::FParticleInstancedMeshInstanceBuffer::InitDynamicRHI()
{
	const FDynamicMeshEmitterReplayData& Source = RenderResources.Source;
	INT ParticleCount = Source.ActiveParticleCount;

	FParticleInstancedMeshInstance* DestInstance =
		(FParticleInstancedMeshInstance*)CreateAndLockInstances(ParticleCount);

	for(INT i=0; i<ParticleCount; i++)
	{
		const INT	CurrentIndex  = Source.ParticleIndices(i);
		const BYTE* ParticleBase  = Source.ParticleData.GetData() + CurrentIndex * Source.ParticleStride;
		FBaseParticle& Particle   = *((FBaseParticle*) ParticleBase);

		FScaleMatrix kScaleMat(Particle.Size * Source.Scale);
		FRotator kRotator(0,0,0);

		if(Source.bMeshRotationActive)
		{
			FMeshRotationPayloadData* PayloadData = (FMeshRotationPayloadData*)((BYTE*)&Particle + Source.MeshRotationOffset);
			kRotator = FRotator::MakeFromEuler(PayloadData->Rotation);
		}

		FRotationMatrix kRotMat(kRotator);
		FMatrix kMat = kScaleMat * kRotMat;

		FParticleInstancedMeshInstance &Instance = DestInstance[i];
		Instance.Location = Particle.Location;
		kMat.GetAxes(Instance.XAxis, Instance.YAxis, Instance.ZAxis);
		Instance.Color = Particle.Color;
	}

	UnlockInstances();
}


/**
 * Allocate and lock a vertex buffer for storing instance transforms.
 */
void* FDynamicMeshEmitterData::FParticleInstancedMeshInstanceBuffer::CreateAndLockInstances(const UINT NumInstances)
{
	check(NumInstances > 0);

	const UINT BufferSize = sizeof(FParticleInstancedMeshInstance) * NumInstances;
	
	// Create the vertex buffer.
	VertexBufferRHI = RHICreateVertexBuffer(BufferSize, NULL, RUF_Dynamic|RUF_WriteOnly);
	
	// Lock the vertex buffer.
	void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, BufferSize, FALSE);

	return VertexBufferData;
}

/**
 * Unlock a vertex buffer for storing instance transforms.
 */
void FDynamicMeshEmitterData::FParticleInstancedMeshInstanceBuffer::UnlockInstances()
{
	// Unlock the vertex buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

///////////////////////////////////////////////////////////////////////////////
//	FDynamicBeam2EmitterData
///////////////////////////////////////////////////////////////////////////////


/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicBeam2EmitterData::Init( UBOOL bInSelected )
{
	bSelected = bInSelected;

	check(Source.ActiveParticleCount < (MaxBeams));	// TTP #33330 - Max of 2048 beams from a single emitter
	check(Source.ParticleStride < 
		((MaxInterpolationPoints + 2) * (sizeof(FVector) + sizeof(FLOAT))) + 
		(MaxNoiseFrequency * (sizeof(FVector) + sizeof(FVector) + sizeof(FLOAT) + sizeof(FLOAT)))
		);	// TTP #33330 - Max of 10k per beam (includes interpolation points, noise, etc.)

	MaterialResource[0] = Source.MaterialInterface->GetRenderProxy(FALSE);
	MaterialResource[1] = GIsEditor ? Source.MaterialInterface->GetRenderProxy(TRUE) : MaterialResource[0];

	bUsesDynamicParameter = FALSE;
//	if (Source.MaterialInterface->GetMaterialResource() != NULL)
//	{
//		bUsesDynamicParameter = Source.MaterialInterface->GetMaterialResource()->GetUsesDynamicParameter();
//	}

	// We won't need this on the render thread
	Source.MaterialInterface = NULL;
}

/**
 *	Create the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicBeam2EmitterData::CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	// Create the vertex factory...
	//@todo. Cache these??
	if (VertexFactory == NULL)
	{
		if (bUsesDynamicParameter == FALSE)
		{
			VertexFactory = (FParticleBeamTrailVertexFactory*)(GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_BeamTrail));
		}
		else
		{
			VertexFactory = (FParticleBeamTrailDynamicParameterVertexFactory*)(GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_BeamTrail_DynamicParameter));
		}
		check(VertexFactory);
	}

	return (VertexFactory != NULL);
}

/**
 *	Release the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicBeam2EmitterData::ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	if (VertexFactory != NULL)
	{
		GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
		VertexFactory = NULL;
	}

	return TRUE;
}

/** Perform the actual work of filling the buffer, often called from another thread 
* @param Me Fill data structure
*/
void FDynamicBeam2EmitterData::DoBufferFill(FAsyncBufferFillData& Me)
{
	FillIndexData(Me);
	if (Source.bLowFreqNoise_Enabled)
	{
		FillData_Noise(Me);
	}
	else
	{
		FillVertexData_NoNoise(Me);
	}
}


INT FDynamicBeam2EmitterData::Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_BeamRenderingTime);
	INC_DWORD_STAT(STAT_BeamParticlesRenderCalls);

	if (bValid == FALSE)
	{
		return 0;
	}

	if ((Source.VertexCount == 0) && (Source.IndexCount == 0))
	{
		return 0;
	}

	UBOOL bMaterialIgnored = PDI->IsMaterialIgnored(MaterialResource[bSelected]);
	if (bMaterialIgnored && (!(View->Family->ShowFlags & SHOW_Wireframe)))
	{
		return 0;
	}

	const FAsyncBufferFillData& Data = EnsureFillCompletion(View);

	INT NumDraws = 0;
	if (Data.OutTriangleCount > 0)
	{
		if (!bMaterialIgnored || View->Family->ShowFlags & SHOW_Wireframe)
		{
			FMeshBatch Mesh;
			FMeshBatchElement& BatchElement = Mesh.Elements(0);
			BatchElement.IndexBuffer			= NULL;
			Mesh.VertexFactory			= VertexFactory;
			Mesh.DynamicVertexData		= Data.VertexData;
			Mesh.DynamicVertexStride	= sizeof(FParticleBeamTrailVertex);
			BatchElement.DynamicIndexData		= Data.IndexData;
			BatchElement.DynamicIndexStride		= Source.IndexStride;
			Mesh.LCI					= NULL;
			if (Source.bUseLocalSpace == TRUE)
			{
				BatchElement.LocalToWorld = Proxy->GetLocalToWorld();
				BatchElement.WorldToLocal = Proxy->GetWorldToLocal();
			}
			else
			{
				BatchElement.LocalToWorld = FMatrix::Identity;
				BatchElement.WorldToLocal = FMatrix::Identity;
			}
			BatchElement.FirstIndex				= 0;
			INT TrianglesToRender = Data.OutTriangleCount;
			if ((TrianglesToRender % 2) != 0)
			{
				TrianglesToRender--;
			}
			BatchElement.NumPrimitives			= TrianglesToRender;
			BatchElement.DegenerateTriangleCount = Data.OutDegenerateTriangleCount;
			BatchElement.MinVertexIndex			= 0;
			BatchElement.MaxVertexIndex			= Source.VertexCount - 1;
			Mesh.UseDynamicData			= TRUE;
			Mesh.ReverseCulling			= Proxy->GetLocalToWorldDeterminant() < 0.0f ? TRUE : FALSE;
			Mesh.CastShadow				= Proxy->GetCastShadow();
			Mesh.DepthPriorityGroup		= (ESceneDepthPriorityGroup)DPGIndex;
			Mesh.bUsePreVertexShaderCulling = FALSE;
			Mesh.PlatformMeshData       = NULL;
			Mesh.bUseDownsampledTranslucency = ShouldRenderDownsampled(View, Proxy->GetBounds());

			if (AllowDebugViewmodes() && (View->Family->ShowFlags & SHOW_Wireframe) && !(View->Family->ShowFlags & SHOW_Materials))
			{
				Mesh.MaterialRenderProxy	= Proxy->GetDeselectedWireframeMatInst();
			}
			else
			{
				Mesh.MaterialRenderProxy	= MaterialResource[GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? bSelected : 0];
			}
			Mesh.Type = PT_TriangleStrip;
#if FLASH
			// On FLASH, we will leverage the fact these are sequential vertex TriStrips
			// and render as a generated TriList. So remove the degenerates...
			BatchElement.NumPrimitives -= Data.OutDegenerateTriangleCount;
#endif
			NumDraws += DrawRichMesh(
				PDI,
				Mesh,
				FLinearColor(1.0f, 0.0f, 0.0f),
				FLinearColor(1.0f, 1.0f, 0.0f),
				FLinearColor(1.0f, 1.0f, 1.0f),
				Proxy->GetPrimitiveSceneInfo(),
				GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? Proxy->IsSelected() : FALSE
				);

			INC_DWORD_STAT_BY(STAT_BeamParticlesTrianglesRendered, Mesh.GetNumPrimitives());
		}

		if (Source.bRenderDirectLine == TRUE)
		{
			RenderDirectLine(Proxy, PDI, View, DPGIndex);
		}

		if ((Source.bRenderLines == TRUE) ||
			(Source.bRenderTessellation == TRUE))
		{
			RenderLines(Proxy, PDI, View, DPGIndex);
		}
	}
	return NumDraws;
}

/**
 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
 *  Only called for primitives that are visible and have bDynamicRelevance
 *
 *	@param	Proxy			The 'owner' particle system scene proxy
 *	@param	ViewFamily		The ViewFamily to pre-render for
 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
 *	@param	FrameNumber		The frame number of this pre-render
 */
void FDynamicBeam2EmitterData::PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber)
{
	if (bValid == FALSE)
	{
		return;
	}

	// Only need to do this once per-view
	if (LastFramePreRendered < FrameNumber)
		{
		SceneProxy = Proxy;
			VertexFactory->SetScreenAlignment(Source.ScreenAlignment);
			// Beams/trails do not support LockAxis
			VertexFactory->SetLockAxesFlag(EPAL_NONE);

		UBOOL bOnlyOneView = !GIsEditor && ((GEngine && GEngine->GameViewport && (GEngine->GameViewport->GetCurrentSplitscreenType() == eSST_NONE)) ? TRUE : FALSE);

		BuildViewFillDataAndSubmit(ViewFamily,VisibilityMap,bOnlyOneView,Source.VertexCount,sizeof(FParticleBeamTrailVertex));

			// Set the frame tracker
			LastFramePreRendered = FrameNumber;
		}
	}

void FDynamicBeam2EmitterData::RenderDirectLine(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex)
{
	for (INT Beam = 0; Beam < Source.ActiveParticleCount; Beam++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * Beam);

		FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;

		BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
		if (BeamPayloadData->TriangleCount == 0)
		{
			continue;
		}

		DrawWireStar(PDI, BeamPayloadData->SourcePoint, 20.0f, FColor(0,255,0),DPGIndex);
		DrawWireStar(PDI, BeamPayloadData->TargetPoint, 20.0f, FColor(255,0,0),DPGIndex);
		PDI->DrawLine(BeamPayloadData->SourcePoint, BeamPayloadData->TargetPoint, FColor(255,255,0),DPGIndex);
	}
}

void FDynamicBeam2EmitterData::RenderLines(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex)
{
	if (Source.bLowFreqNoise_Enabled)
	{
		INT	TrianglesToRender = 0;

		FMatrix WorldToLocal = Proxy->GetWorldToLocal();
		FMatrix LocalToWorld = Proxy->GetLocalToWorld();
		FMatrix CameraToWorld = View->ViewMatrix.Inverse();
		FVector	ViewOrigin = CameraToWorld.GetOrigin();

		Source.Sheets = (Source.Sheets > 0) ? Source.Sheets : 1;

		// Frequency is the number of noise points to generate, evenly distributed along the line.
		Source.Frequency = (Source.Frequency > 0) ? Source.Frequency : 1;

		// NoiseTessellation is the amount of tessellation that should occur between noise points.
		INT	TessFactor	= Source.NoiseTessellation ? Source.NoiseTessellation : 1;
		FLOAT	InvTessFactor	= 1.0f / TessFactor;
		INT		i;

		// The last position processed
		FVector	LastPosition, LastDrawPosition, LastTangent;
		// The current position
		FVector	CurrPosition, CurrDrawPosition;
		// The target
		FVector	TargetPosition, TargetDrawPosition;
		// The next target
		FVector	NextTargetPosition, NextTargetDrawPosition, TargetTangent;
		// The interperted draw position
		FVector InterpDrawPos;
		FVector	InterimDrawPosition;

		FVector	Size;

		FVector Location;
		FVector EndPoint;
		FVector Offset;
		FVector LastOffset;
		FLOAT	fStrength;
		FLOAT	fTargetStrength;

		INT	 VertexCount	= 0;

		// Tessellate the beam along the noise points
		for (i = 0; i < Source.ActiveParticleCount; i++)
		{
			DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * i);

			// Retrieve the beam data from the particle.
			FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
			FLOAT*					NoiseRate			= NULL;
			FLOAT*					NoiseDelta			= NULL;
			FVector*				TargetNoisePoints	= NULL;
			FVector*				NextNoisePoints		= NULL;
			FLOAT*					TaperValues			= NULL;
			FLOAT*					NoiseDistanceScale	= NULL;

			BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
			if (BeamPayloadData->TriangleCount == 0)
			{
				continue;
			}
			if (Source.NoiseRateOffset != -1)
			{
				NoiseRate = (FLOAT*)((BYTE*)Particle + Source.NoiseRateOffset);
			}
			if (Source.NoiseDeltaTimeOffset != -1)
			{
				NoiseDelta = (FLOAT*)((BYTE*)Particle + Source.NoiseDeltaTimeOffset);
			}
			if (Source.TargetNoisePointsOffset != -1)
			{
				TargetNoisePoints = (FVector*)((BYTE*)Particle + Source.TargetNoisePointsOffset);
			}
			if (Source.NextNoisePointsOffset != -1)
			{
				NextNoisePoints = (FVector*)((BYTE*)Particle + Source.NextNoisePointsOffset);
			}
			if (Source.TaperValuesOffset != -1)
			{
				TaperValues = (FLOAT*)((BYTE*)Particle + Source.TaperValuesOffset);
			}
			if (Source.NoiseDistanceScaleOffset != -1)
			{
				NoiseDistanceScale = (FLOAT*)((BYTE*)Particle + Source.NoiseDistanceScaleOffset);
			}

			FLOAT NoiseDistScale = 1.0f;
			if (NoiseDistanceScale)
			{
				NoiseDistScale = *NoiseDistanceScale;
			}

			FVector* NoisePoints	= TargetNoisePoints;
			FVector* NextNoise		= NextNoisePoints;

			FLOAT NoiseRangeScaleFactor = Source.NoiseRangeScale;
			//@todo. How to handle no noise points?
			// If there are no noise points, why are we in here?
			if (NoisePoints == NULL)
			{
				continue;
			}

			// Pin the size to the X component
			Size = FVector(Particle->Size.X * Source.Scale.X);

			check(TessFactor > 0);

			// Setup the current position as the source point
			CurrPosition		= BeamPayloadData->SourcePoint;
			CurrDrawPosition	= CurrPosition;

			// Setup the source tangent & strength
			if (Source.bUseSource)
			{
				// The source module will have determined the proper source tangent.
				LastTangent	= BeamPayloadData->SourceTangent;
				fStrength	= BeamPayloadData->SourceStrength;
			}
			else
			{
				// We don't have a source module, so use the orientation of the emitter.
				LastTangent	= WorldToLocal.GetAxis(0);
				fStrength	= Source.NoiseTangentStrength;
			}
			LastTangent.Normalize();
			LastTangent *= fStrength;
			fTargetStrength	= Source.NoiseTangentStrength;

			// Set the last draw position to the source so we don't get 'under-hang'
			LastPosition		= CurrPosition;
			LastDrawPosition	= CurrDrawPosition;

			UBOOL	bLocked	= BEAM2_TYPEDATA_LOCKED(BeamPayloadData->Lock_Max_NumNoisePoints);

			FVector	UseNoisePoint, CheckNoisePoint;
			FVector	NoiseDir;

			// Reset the texture coordinate
			LastPosition		= BeamPayloadData->SourcePoint;
			LastDrawPosition	= LastPosition;

			// Determine the current position by stepping the direct line and offsetting with the noise point. 
			CurrPosition		= LastPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;

			if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
			{
				NoiseDir		= NextNoise[0] - NoisePoints[0];
				NoiseDir.Normalize();
				CheckNoisePoint	= NoisePoints[0] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
				if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[0].X) < Source.NoiseLockRadius) &&
					(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[0].Y) < Source.NoiseLockRadius) &&
					(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[0].Z) < Source.NoiseLockRadius))
				{
					NoisePoints[0]	= NextNoise[0];
				}
				else
				{
					NoisePoints[0]	= CheckNoisePoint;
				}
			}

			CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[0] * NoiseDistScale);

			// Determine the offset for the leading edge
			Location	= LastDrawPosition;
			EndPoint	= CurrDrawPosition;

			// 'Lead' edge
			DrawWireStar(PDI, Location, 15.0f, FColor(0,255,0), DPGIndex);

			for (INT StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
			{
				// Determine the current position by stepping the direct line and offsetting with the noise point. 
				CurrPosition		= LastPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;

				if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
				{
					NoiseDir		= NextNoise[StepIndex] - NoisePoints[StepIndex];
					NoiseDir.Normalize();
					CheckNoisePoint	= NoisePoints[StepIndex] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
					if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[StepIndex].X) < Source.NoiseLockRadius) &&
						(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[StepIndex].Y) < Source.NoiseLockRadius) &&
						(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[StepIndex].Z) < Source.NoiseLockRadius))
					{
						NoisePoints[StepIndex]	= NextNoise[StepIndex];
					}
					else
					{
						NoisePoints[StepIndex]	= CheckNoisePoint;
					}
				}

				CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[StepIndex] * NoiseDistScale);

				// Prep the next draw position to determine tangents
				UBOOL bTarget = FALSE;
				NextTargetPosition	= CurrPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;
				if (bLocked && ((StepIndex + 1) == BeamPayloadData->Steps))
				{
					// If we are locked, and the next step is the target point, set the draw position as such.
					// (ie, we are on the last noise point...)
					NextTargetDrawPosition	= BeamPayloadData->TargetPoint;
					if (Source.bTargetNoise)
					{
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
							if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
							}
							else
							{
								NoisePoints[Source.Frequency]	= CheckNoisePoint;
							}
						}

						NextTargetDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[Source.Frequency] * NoiseDistScale);
					}
					TargetTangent = BeamPayloadData->TargetTangent;
					fTargetStrength	= BeamPayloadData->TargetStrength;
				}
				else
				{
					// Just another noise point... offset the target to get the draw position.
					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[StepIndex + 1] - NoisePoints[StepIndex + 1];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[StepIndex + 1] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
						if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[StepIndex + 1].X) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[StepIndex + 1].Y) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[StepIndex + 1].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[StepIndex + 1]	= NextNoise[StepIndex + 1];
						}
						else
						{
							NoisePoints[StepIndex + 1]	= CheckNoisePoint;
						}
					}

					NextTargetDrawPosition	= NextTargetPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[StepIndex + 1] * NoiseDistScale);

					TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
				}
				TargetTangent.Normalize();
				TargetTangent *= fTargetStrength;

				InterimDrawPosition = LastDrawPosition;
				// Tessellate between the current position and the last position
				for (INT TessIndex = 0; TessIndex < TessFactor; TessIndex++)
				{
					InterpDrawPos = CubicInterp(
						LastDrawPosition, LastTangent,
						CurrDrawPosition, TargetTangent,
						InvTessFactor * (TessIndex + 1));

					Location	= InterimDrawPosition;
					EndPoint	= InterpDrawPos;

					FColor StarColor(255,0,255);
					if (TessIndex == 0)
					{
						StarColor = FColor(0,0,255);
					}
					else
					if (TessIndex == (TessFactor - 1))
					{
						StarColor = FColor(255,255,0);
					}

					// Generate the vertex
					DrawWireStar(PDI, EndPoint, 15.0f, StarColor, DPGIndex);
					PDI->DrawLine(Location, EndPoint, FLinearColor(1.0f,1.0f,0.0f), DPGIndex);
					InterimDrawPosition	= InterpDrawPos;
				}
				LastPosition		= CurrPosition;
				LastDrawPosition	= CurrDrawPosition;
				LastTangent			= TargetTangent;
			}

			if (bLocked)
			{
				// Draw the line from the last point to the target
				CurrDrawPosition	= BeamPayloadData->TargetPoint;
				if (Source.bTargetNoise)
				{
					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
						if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
						}
						else
						{
							NoisePoints[Source.Frequency]	= CheckNoisePoint;
						}
					}

					CurrDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[Source.Frequency] * NoiseDistScale);
				}

				if (Source.bUseTarget)
				{
					TargetTangent = BeamPayloadData->TargetTangent;
				}
				else
				{
					NextTargetDrawPosition	= CurrPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;
					TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * 
						(NextTargetDrawPosition - LastDrawPosition);
				}
				TargetTangent.Normalize();
				TargetTangent *= fTargetStrength;

				// Tessellate this segment
				InterimDrawPosition = LastDrawPosition;
				for (INT TessIndex = 0; TessIndex < TessFactor; TessIndex++)
				{
					InterpDrawPos = CubicInterp(
						LastDrawPosition, LastTangent,
						CurrDrawPosition, TargetTangent,
						InvTessFactor * (TessIndex + 1));

					Location	= InterimDrawPosition;
					EndPoint	= InterpDrawPos;

					FColor StarColor(255,0,255);
					if (TessIndex == 0)
					{
						StarColor = FColor(255,255,255);
					}
					else
					if (TessIndex == (TessFactor - 1))
					{
						StarColor = FColor(255,255,0);
					}

					// Generate the vertex
					DrawWireStar(PDI, EndPoint, 15.0f, StarColor, DPGIndex);
					PDI->DrawLine(Location, EndPoint, FLinearColor(1.0f,1.0f,0.0f), DPGIndex);
					VertexCount++;
					InterimDrawPosition	= InterpDrawPos;
				}
			}
		}
	}

	if (Source.InterpolationPoints > 1)
	{
		FMatrix CameraToWorld = View->ViewMatrix.Inverse();
		FVector	ViewOrigin = CameraToWorld.GetOrigin();
		INT TessFactor = Source.InterpolationPoints ? Source.InterpolationPoints : 1;

		if (TessFactor <= 1)
		{
			for (INT i = 0; i < Source.ActiveParticleCount; i++)
			{
				DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * i);
				FBeam2TypeDataPayload* BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
				if (BeamPayloadData->TriangleCount == 0)
				{
					continue;
				}

				FVector EndPoint	= Particle->Location;
				FVector Location	= BeamPayloadData->SourcePoint;

				DrawWireStar(PDI, Location, 15.0f, FColor(255,0,0), DPGIndex);
				DrawWireStar(PDI, EndPoint, 15.0f, FColor(255,0,0), DPGIndex);
				PDI->DrawLine(Location, EndPoint, FColor(255,255,0), DPGIndex);
			}
		}
		else
		{
			for (INT i = 0; i < Source.ActiveParticleCount; i++)
			{
				DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * i);

				FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
				FVector*				InterpolatedPoints	= NULL;

				BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
				if (BeamPayloadData->TriangleCount == 0)
				{
					continue;
				}
				if (Source.InterpolatedPointsOffset != -1)
				{
					InterpolatedPoints = (FVector*)((BYTE*)Particle + Source.InterpolatedPointsOffset);
				}

				FVector Location;
				FVector EndPoint;

				check(InterpolatedPoints);	// TTP #33139

				Location	= BeamPayloadData->SourcePoint;
				EndPoint	= InterpolatedPoints[0];

				DrawWireStar(PDI, Location, 15.0f, FColor(255,0,0), DPGIndex);
				for (INT StepIndex = 0; StepIndex < BeamPayloadData->InterpolationSteps; StepIndex++)
				{
					EndPoint = InterpolatedPoints[StepIndex];
					DrawWireStar(PDI, EndPoint, 15.0f, FColor(255,0,0), DPGIndex);
					PDI->DrawLine(Location, EndPoint, FColor(255,255,0), DPGIndex);
					Location = EndPoint;
				}
			}
		}
	}
}

void FDynamicBeam2EmitterData::RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses)
{
}

INT FDynamicBeam2EmitterData::FillIndexData(struct FAsyncBufferFillData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_BeamFillIndexTime);

	INT	TrianglesToRender = 0;
	INT	DegenerateTrianglesToRender = 0;

	// Beam2 polygons are packed and joined as follows:
	//
	// 1--3--5--7--9-...
	// |\ |\ |\ |\ |\...
	// | \| \| \| \| ...
	// 0--2--4--6--8-...
	//
	// (ie, the 'leading' edge of polygon (n) is the trailing edge of polygon (n+1)
	//
	// NOTE: This is primed for moving to tri-strips...
	//
	INT TessFactor	= Source.InterpolationPoints ? Source.InterpolationPoints : 1;
	if (Source.Sheets <= 0)
	{
		Source.Sheets = 1;
	}

	//	UBOOL bWireframe = ((View->Family->ShowFlags & SHOW_Wireframe) && !(View->Family->ShowFlags & SHOW_Materials));
	UBOOL bWireframe = FALSE;

	INT TempIndexCount = 0;
	for (INT ii = 0; ii < Source.TrianglesPerSheet.Num(); ii++)
	{
		INT Triangles = Source.TrianglesPerSheet(ii);
		if (bWireframe)
		{
			TempIndexCount += (8 * Triangles + 2) * Source.Sheets;
		}
		else
		{
			if (TempIndexCount == 0)
			{
				TempIndexCount = 2;
			}
			TempIndexCount += Triangles * Source.Sheets;
			TempIndexCount += 4 * (Source.Sheets - 1);	// Degenerate indices between sheets
			if ((ii + 1) < Source.TrianglesPerSheet.Num())
			{
				TempIndexCount += 4;	// Degenerate indices between beams
			}
		}
	}

	if ((Data.IndexData == NULL) || (Data.IndexCount < TempIndexCount))
	{
		check((UINT)TempIndexCount <= 65535);
		if (Data.IndexData)
		{
			appFree(Data.IndexData);
		}
		Data.IndexData = appMalloc(TempIndexCount * Source.IndexStride);
		Data.IndexCount = TempIndexCount;
	}

	if (Source.IndexStride == sizeof(WORD))
	{
		WORD*	Index				= (WORD*)Data.IndexData;
		WORD	VertexIndex			= 0;
		WORD	StartVertexIndex	= 0;

		for (INT Beam = 0; Beam < Source.ActiveParticleCount; Beam++)
		{
			DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * Beam);

			FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
			FVector*				InterpolatedPoints	= NULL;
			FLOAT*					NoiseRate			= NULL;
			FLOAT*					NoiseDelta			= NULL;
			FVector*				TargetNoisePoints	= NULL;
			FVector*				NextNoisePoints		= NULL;
			FLOAT*					TaperValues			= NULL;

			BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
			if (BeamPayloadData->TriangleCount == 0)
			{
				continue;
			}
			if ((Source.InterpolationPoints > 0) && (BeamPayloadData->Steps == 0))
			{
				continue;
			}

			if (bWireframe)
			{
				for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
				{
					VertexIndex = 0;

					// The 'starting' line
					TrianglesToRender += 1;
					*(Index++) = StartVertexIndex + 0;
					*(Index++) = StartVertexIndex + 1;

					// 4 lines per quad
					INT TriCount = Source.TrianglesPerSheet(Beam);
					INT QuadCount = TriCount / 2;
					TrianglesToRender += TriCount * 2;

					for (INT i = 0; i < QuadCount; i++)
					{
						*(Index++) = StartVertexIndex + VertexIndex + 0;
						*(Index++) = StartVertexIndex + VertexIndex + 2;
						*(Index++) = StartVertexIndex + VertexIndex + 1;
						*(Index++) = StartVertexIndex + VertexIndex + 2;
						*(Index++) = StartVertexIndex + VertexIndex + 1;
						*(Index++) = StartVertexIndex + VertexIndex + 3;
						*(Index++) = StartVertexIndex + VertexIndex + 2;
						*(Index++) = StartVertexIndex + VertexIndex + 3;

						VertexIndex += 2;
					}

					StartVertexIndex += TriCount + 2;
				}
			}
			else
			{
				// 
				if (Beam == 0)
				{
					*(Index++) = VertexIndex++;	// SheetIndex + 0
					*(Index++) = VertexIndex++;	// SheetIndex + 1
				}

				for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
				{
					// 2 triangles per tessellation factor
					TrianglesToRender += BeamPayloadData->TriangleCount;

					// Sequentially step through each triangle - 1 vertex per triangle
					for (INT i = 0; i < BeamPayloadData->TriangleCount; i++)
					{
						*(Index++) = VertexIndex++;
					}

					// Degenerate tris
					if ((SheetIndex + 1) < Source.Sheets)
					{
						*(Index++) = VertexIndex - 1;	// Last vertex of the previous sheet
						*(Index++) = VertexIndex;		// First vertex of the next sheet
						*(Index++) = VertexIndex++;		// First vertex of the next sheet
						*(Index++) = VertexIndex++;		// Second vertex of the next sheet

						TrianglesToRender += 4;
						DegenerateTrianglesToRender += 4;
					}
				}
				if ((Beam + 1) < Source.ActiveParticleCount)
				{
					*(Index++) = VertexIndex - 1;	// Last vertex of the previous sheet
					*(Index++) = VertexIndex;		// First vertex of the next sheet
					*(Index++) = VertexIndex++;		// First vertex of the next sheet
					*(Index++) = VertexIndex++;		// Second vertex of the next sheet

					TrianglesToRender += 4;
					DegenerateTrianglesToRender += 4;
				}
			}
		}
	}
	else
	{
		check(!TEXT("Rendering beam with > 5000 vertices!"));
		DWORD*	Index		= (DWORD*)Data.IndexData;
		DWORD	VertexIndex	= 0;
		for (INT Beam = 0; Beam < Source.ActiveParticleCount; Beam++)
		{
			DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * Beam);

			FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
			BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
			if (BeamPayloadData->TriangleCount == 0)
			{
				continue;
			}

			// 
			if (Beam == 0)
			{
				*(Index++) = VertexIndex++;	// SheetIndex + 0
				*(Index++) = VertexIndex++;	// SheetIndex + 1
			}

			for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				// 2 triangles per tessellation factor
				TrianglesToRender += BeamPayloadData->TriangleCount;

				// Sequentially step through each triangle - 1 vertex per triangle
				for (INT i = 0; i < BeamPayloadData->TriangleCount; i++)
				{
					*(Index++) = VertexIndex++;
				}

				// Degenerate tris
				if ((SheetIndex + 1) < Source.Sheets)
				{
					*(Index++) = VertexIndex - 1;	// Last vertex of the previous sheet
					*(Index++) = VertexIndex;		// First vertex of the next sheet
					*(Index++) = VertexIndex++;		// First vertex of the next sheet
					*(Index++) = VertexIndex++;		// Second vertex of the next sheet
					TrianglesToRender += 4;
					DegenerateTrianglesToRender += 4;
				}
			}
			if ((Beam + 1) < Source.ActiveParticleCount)
			{
				*(Index++) = VertexIndex - 1;	// Last vertex of the previous sheet
				*(Index++) = VertexIndex;		// First vertex of the next sheet
				*(Index++) = VertexIndex++;		// First vertex of the next sheet
				*(Index++) = VertexIndex++;		// Second vertex of the next sheet
				TrianglesToRender += 4;
				DegenerateTrianglesToRender += 4;
			}
		}
	}

	Data.OutTriangleCount = TrianglesToRender;
	Data.OutDegenerateTriangleCount = DegenerateTrianglesToRender;
	return TrianglesToRender;
}

INT FDynamicBeam2EmitterData::FillVertexData_NoNoise(FAsyncBufferFillData& Me)
{
	SCOPE_CYCLE_COUNTER(STAT_BeamFillVertexTime);

	INT	TrianglesToRender = 0;

	FParticleBeamTrailVertex* Vertex = (FParticleBeamTrailVertex*)Me.VertexData;
	FMatrix CameraToWorld = Me.View->ViewMatrix.InverseSafe();
	FVector	ViewOrigin = CameraToWorld.GetOrigin();
	FVector ViewDirection = Me.View->ViewMatrix.GetAxis(0);
	INT TessFactor = Source.InterpolationPoints ? Source.InterpolationPoints : 1;

	if (Source.Sheets <= 0)
	{
		Source.Sheets = 1;
	}

	FVector	Offset(0.0f), LastOffset(0.0f);
	FVector	Size;

	INT PackedCount = 0;

	if (TessFactor <= 1)
	{
		for (INT i = 0; i < Source.ActiveParticleCount; i++)
		{
			DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * i);

			FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
			FVector*				InterpolatedPoints	= NULL;
			FLOAT*					NoiseRate			= NULL;
			FLOAT*					NoiseDelta			= NULL;
			FVector*				TargetNoisePoints	= NULL;
			FVector*				NextNoisePoints		= NULL;
			FLOAT*					TaperValues			= NULL;

			BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
			if (BeamPayloadData->TriangleCount == 0)
			{
				continue;
			}
			if (Source.InterpolatedPointsOffset != -1)
			{
				InterpolatedPoints = (FVector*)((BYTE*)Particle + Source.InterpolatedPointsOffset);
			}
			if (Source.NoiseRateOffset != -1)
			{
				NoiseRate = (FLOAT*)((BYTE*)Particle + Source.NoiseRateOffset);
			}
			if (Source.NoiseDeltaTimeOffset != -1)
			{
				NoiseDelta = (FLOAT*)((BYTE*)Particle + Source.NoiseDeltaTimeOffset);
			}
			if (Source.TargetNoisePointsOffset != -1)
			{
				TargetNoisePoints = (FVector*)((BYTE*)Particle + Source.TargetNoisePointsOffset);
			}
			if (Source.NextNoisePointsOffset != -1)
			{
				NextNoisePoints = (FVector*)((BYTE*)Particle + Source.NextNoisePointsOffset);
			}
			if (Source.TaperValuesOffset != -1)
			{
				TaperValues = (FLOAT*)((BYTE*)Particle + Source.TaperValuesOffset);
			}

			// Pin the size to the X component
			Size	= FVector(Particle->Size.X * Source.Scale.X);

			FVector EndPoint	= Particle->Location;
			FVector Location	= BeamPayloadData->SourcePoint;
			FVector Right, Up;
			FVector WorkingUp;

			Right = Location - EndPoint;
			Right.Normalize();
			if (((Source.UpVectorStepSize == 1) && (i == 0)) || (Source.UpVectorStepSize == 0))
			{
				//Up = Right ^ ViewDirection;
				Up = Right ^ (Location - ViewOrigin);
				if (!Up.Normalize())
				{
					Up = CameraToWorld.GetAxis(1);
				}
			}

			FLOAT	fUEnd;
			FLOAT	Tiles		= 1.0f;
			if (Source.TextureTileDistance > KINDA_SMALL_NUMBER)
			{
				FVector	Direction	= BeamPayloadData->TargetPoint - BeamPayloadData->SourcePoint;
				FLOAT	Distance	= Direction.Size();
				Tiles				= Distance / Source.TextureTileDistance;
			}
			fUEnd		= Tiles;

			if (BeamPayloadData->TravelRatio > KINDA_SMALL_NUMBER)
			{
				fUEnd	= Tiles * BeamPayloadData->TravelRatio;
			}

			// For the direct case, this isn't a big deal, as it will not require much work per sheet.
			for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				if (SheetIndex)
				{
					FLOAT	Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
					FQuat	QuatRotator	= FQuat(Right, Angle);
					WorkingUp			= QuatRotator.RotateVector(Up);
				}
				else
				{
					WorkingUp	= Up;
				}

				FLOAT	Taper	= 1.0f;
				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[0];
				}

				Offset.X		= WorkingUp.X * Size.X * Taper;
				Offset.Y		= WorkingUp.Y * Size.Y * Taper;
				Offset.Z		= WorkingUp.Z * Size.Z * Taper;

				// 'Lead' edge
				Vertex->Position	= Location + Offset;
				Vertex->OldPosition	= Location;
				Vertex->Size		= Size;
				Vertex->Tex_U		= 0.0f;
				Vertex->Tex_V		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				Vertex->Position	= Location - Offset;
				Vertex->OldPosition	= Location;
				Vertex->Size		= Size;
				Vertex->Tex_U		= 0.0f;
				Vertex->Tex_V		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[1];
				}

				Offset.X		= WorkingUp.X * Size.X * Taper;
				Offset.Y		= WorkingUp.Y * Size.Y * Taper;
				Offset.Z		= WorkingUp.Z * Size.Z * Taper;

				//
				Vertex->Position	= EndPoint + Offset;
				Vertex->OldPosition	= Particle->OldLocation;
				Vertex->Size		= Size;
				Vertex->Tex_U		= fUEnd;
				Vertex->Tex_V		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				Vertex->Position	= EndPoint - Offset;
				Vertex->OldPosition	= Particle->OldLocation;
				Vertex->Size		= Size;
				Vertex->Tex_U		= fUEnd;
				Vertex->Tex_V		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;
			}
		}
	}
	else
	{
		FLOAT	fTextureIncrement	= 1.0f / Source.InterpolationPoints;;

		for (INT i = 0; i < Source.ActiveParticleCount; i++)
		{
			DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * i);

			FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
			FVector*				InterpolatedPoints	= NULL;
			FLOAT*					NoiseRate			= NULL;
			FLOAT*					NoiseDelta			= NULL;
			FVector*				TargetNoisePoints	= NULL;
			FVector*				NextNoisePoints		= NULL;
			FLOAT*					TaperValues			= NULL;

			BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
			if (BeamPayloadData->TriangleCount == 0)
			{
				continue;
			}
			if (Source.InterpolatedPointsOffset != -1)
			{
				InterpolatedPoints = (FVector*)((BYTE*)Particle + Source.InterpolatedPointsOffset);
			}
			if (Source.NoiseRateOffset != -1)
			{
				NoiseRate = (FLOAT*)((BYTE*)Particle + Source.NoiseRateOffset);
			}
			if (Source.NoiseDeltaTimeOffset != -1)
			{
				NoiseDelta = (FLOAT*)((BYTE*)Particle + Source.NoiseDeltaTimeOffset);
			}
			if (Source.TargetNoisePointsOffset != -1)
			{
				TargetNoisePoints = (FVector*)((BYTE*)Particle + Source.TargetNoisePointsOffset);
			}
			if (Source.NextNoisePointsOffset != -1)
			{
				NextNoisePoints = (FVector*)((BYTE*)Particle + Source.NextNoisePointsOffset);
			}
			if (Source.TaperValuesOffset != -1)
			{
				TaperValues = (FLOAT*)((BYTE*)Particle + Source.TaperValuesOffset);
			}

			if (Source.TextureTileDistance > KINDA_SMALL_NUMBER)
			{
				FVector	Direction	= BeamPayloadData->TargetPoint - BeamPayloadData->SourcePoint;
				FLOAT	Distance	= Direction.Size();
				FLOAT	Tiles		= Distance / Source.TextureTileDistance;
				fTextureIncrement	= Tiles / Source.InterpolationPoints;
			}

			// Pin the size to the X component
			Size	= FVector(Particle->Size.X * Source.Scale.X);

			FLOAT	Angle;
			FQuat	QuatRotator(0, 0, 0, 0);

			FVector Location;
			FVector EndPoint;
			FVector Right;
			FVector Up;
			FVector WorkingUp;
			FLOAT	fU;

			check(InterpolatedPoints);	// TTP #33139
			// For the direct case, this isn't a big deal, as it will not require much work per sheet.
			for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				fU			= 0.0f;
				Location	= BeamPayloadData->SourcePoint;
				EndPoint	= InterpolatedPoints[0];
				Right		= Location - EndPoint;
				Right.Normalize();
				if (Source.UpVectorStepSize == 0)
				{
					//Up = Right ^ ViewDirection;
					Up = Right ^ (Location - ViewOrigin);
					if (!Up.Normalize())
					{
						Up = CameraToWorld.GetAxis(1);
					}
				}

				if (SheetIndex)
				{
					Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
					QuatRotator	= FQuat(Right, Angle);
					WorkingUp	= QuatRotator.RotateVector(Up);
				}
				else
				{
					WorkingUp	= Up;
				}

				FLOAT	Taper	= 1.0f;

				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[0];
				}

				Offset.X	= WorkingUp.X * Size.X * Taper;
				Offset.Y	= WorkingUp.Y * Size.Y * Taper;
				Offset.Z	= WorkingUp.Z * Size.Z * Taper;

				// 'Lead' edge
				Vertex->Position	= Location + Offset;
				Vertex->OldPosition	= Location;
				Vertex->Size		= Size;
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				Vertex->Position	= Location - Offset;
				Vertex->OldPosition	= Location;
				Vertex->Size		= Size;
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				for (INT StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
				{
					EndPoint	= InterpolatedPoints[StepIndex];
					if (Source.UpVectorStepSize == 0)
					{
						//Up = Right ^ ViewDirection;
						Up = Right ^ (Location - ViewOrigin);
						if (!Up.Normalize())
						{
							Up = CameraToWorld.GetAxis(1);
						}
					}

					if (SheetIndex)
					{
						WorkingUp	= QuatRotator.RotateVector(Up);
					}
					else
					{
						WorkingUp	= Up;
					}

					if (Source.TaperMethod != PEBTM_None)
					{
						check(TaperValues);
						Taper	= TaperValues[StepIndex + 1];
					}

					Offset.X		= WorkingUp.X * Size.X * Taper;
					Offset.Y		= WorkingUp.Y * Size.Y * Taper;
					Offset.Z		= WorkingUp.Z * Size.Z * Taper;

					//
					Vertex->Position	= EndPoint + Offset;
					Vertex->OldPosition	= EndPoint;
					Vertex->Size		= Size;
					Vertex->Tex_U		= fU + fTextureIncrement;
					Vertex->Tex_V		= 0.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
				PackedCount++;

					Vertex->Position	= EndPoint - Offset;
					Vertex->OldPosition	= EndPoint;
					Vertex->Size		= Size;
					Vertex->Tex_U		= fU + fTextureIncrement;
					Vertex->Tex_V		= 1.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
				PackedCount++;

					Location			 = EndPoint;
					fU					+= fTextureIncrement;
				}

				if (BeamPayloadData->TravelRatio > KINDA_SMALL_NUMBER)
				{
					//@todo.SAS. Re-implement partial-segment beams
				}
			}
		}
	}

	check(PackedCount <= Source.VertexCount);

	return TrianglesToRender;
}

INT FDynamicBeam2EmitterData::FillData_Noise(FAsyncBufferFillData& Me)
{
	SCOPE_CYCLE_COUNTER(STAT_BeamFillVertexTime);

	INT	TrianglesToRender = 0;

	if (Source.InterpolationPoints > 0)
	{
		return FillData_InterpolatedNoise(Me);
	}

	FParticleBeamTrailVertex* Vertex = (FParticleBeamTrailVertex*)Me.VertexData;
	FMatrix CameraToWorld = Me.View->ViewMatrix.InverseSafe();
	FVector ViewDirection	= Me.View->ViewMatrix.GetAxis(0);

	if (Source.Sheets <= 0)
	{
		Source.Sheets = 1;
	}

	FVector	ViewOrigin	= CameraToWorld.GetOrigin();

	// Frequency is the number of noise points to generate, evenly distributed along the line.
	if (Source.Frequency <= 0)
	{
		Source.Frequency = 1;
	}

	// NoiseTessellation is the amount of tessellation that should occur between noise points.
	INT	TessFactor	= Source.NoiseTessellation ? Source.NoiseTessellation : 1;
	
	FLOAT	InvTessFactor	= 1.0f / TessFactor;
	INT		i;

	// The last position processed
	FVector	LastPosition, LastDrawPosition, LastTangent;
	// The current position
	FVector	CurrPosition, CurrDrawPosition;
	// The target
	FVector	TargetPosition, TargetDrawPosition;
	// The next target
	FVector	NextTargetPosition, NextTargetDrawPosition, TargetTangent;
	// The interperted draw position
	FVector InterpDrawPos;
	FVector	InterimDrawPosition;

	FVector	Size;

	FLOAT	Angle;
	FQuat	QuatRotator;

	FVector Location;
	FVector EndPoint;
	FVector Right;
	FVector Up;
	FVector WorkingUp;
	FVector LastUp;
	FVector WorkingLastUp;
	FVector Offset;
	FVector LastOffset;
	FLOAT	fStrength;
	FLOAT	fTargetStrength;

	FLOAT	fU;
	FLOAT	TextureIncrement	= 1.0f / (((Source.Frequency > 0) ? Source.Frequency : 1) * TessFactor);	// TTP #33140/33159

	INT	 CheckVertexCount	= 0;

	FVector THE_Up = FVector(0.0f);

	FMatrix WorldToLocal = SceneProxy->GetWorldToLocal();
	FMatrix LocalToWorld = SceneProxy->GetLocalToWorld();

	// Tessellate the beam along the noise points
	for (i = 0; i < Source.ActiveParticleCount; i++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * i);

		// Retrieve the beam data from the particle.
		FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;
		FLOAT*					NoiseDistanceScale	= NULL;

		BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
		if (BeamPayloadData->TriangleCount == 0)
		{
			continue;
		}
		if (Source.InterpolatedPointsOffset != -1)
		{
			InterpolatedPoints = (FVector*)((BYTE*)Particle + Source.InterpolatedPointsOffset);
		}
		if (Source.NoiseRateOffset != -1)
		{
			NoiseRate = (FLOAT*)((BYTE*)Particle + Source.NoiseRateOffset);
		}
		if (Source.NoiseDeltaTimeOffset != -1)
		{
			NoiseDelta = (FLOAT*)((BYTE*)Particle + Source.NoiseDeltaTimeOffset);
		}
		if (Source.TargetNoisePointsOffset != -1)
		{
			TargetNoisePoints = (FVector*)((BYTE*)Particle + Source.TargetNoisePointsOffset);
		}
		if (Source.NextNoisePointsOffset != -1)
		{
			NextNoisePoints = (FVector*)((BYTE*)Particle + Source.NextNoisePointsOffset);
		}
		if (Source.TaperValuesOffset != -1)
		{
			TaperValues = (FLOAT*)((BYTE*)Particle + Source.TaperValuesOffset);
		}
		if (Source.NoiseDistanceScaleOffset != -1)
		{
			NoiseDistanceScale = (FLOAT*)((BYTE*)Particle + Source.NoiseDistanceScaleOffset);
		}

		FLOAT NoiseDistScale = 1.0f;
		if (NoiseDistanceScale)
		{
			NoiseDistScale = *NoiseDistanceScale;
		}

		FVector* NoisePoints	= TargetNoisePoints;
		FVector* NextNoise		= NextNoisePoints;

		FLOAT NoiseRangeScaleFactor = Source.NoiseRangeScale;
		//@todo. How to handle no noise points?
		// If there are no noise points, why are we in here?
		if (NoisePoints == NULL)
		{
			continue;
		}

		// Pin the size to the X component
		Size	= FVector(Particle->Size.X * Source.Scale.X);

		if (TessFactor <= 1)
		{
			// Setup the current position as the source point
			CurrPosition		= BeamPayloadData->SourcePoint;
			CurrDrawPosition	= CurrPosition;

			// Setup the source tangent & strength
			if (Source.bUseSource)
			{
				// The source module will have determined the proper source tangent.
				LastTangent	= BeamPayloadData->SourceTangent;
				fStrength	= BeamPayloadData->SourceStrength;
			}
			else
			{
				// We don't have a source module, so use the orientation of the emitter.
				LastTangent	= WorldToLocal.GetAxis(0);
				fStrength	= Source.NoiseTangentStrength;
			}
			LastTangent.Normalize();
			LastTangent *= fStrength;

			fTargetStrength	= Source.NoiseTangentStrength;

			// Set the last draw position to the source so we don't get 'under-hang'
			LastPosition		= CurrPosition;
			LastDrawPosition	= CurrDrawPosition;

			UBOOL	bLocked	= BEAM2_TYPEDATA_LOCKED(BeamPayloadData->Lock_Max_NumNoisePoints);

			FVector	UseNoisePoint, CheckNoisePoint;
			FVector	NoiseDir;

			for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				// Reset the texture coordinate
				fU					= 0.0f;
				LastPosition		= BeamPayloadData->SourcePoint;
				LastDrawPosition	= LastPosition;

				// Determine the current position by stepping the direct line and offsetting with the noise point. 
				CurrPosition		= LastPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;

				if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
				{
					NoiseDir		= NextNoise[0] - NoisePoints[0];
					NoiseDir.Normalize();
					CheckNoisePoint	= NoisePoints[0] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
					if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[0].X) < Source.NoiseLockRadius) &&
						(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[0].Y) < Source.NoiseLockRadius) &&
						(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[0].Z) < Source.NoiseLockRadius))
					{
						NoisePoints[0]	= NextNoise[0];
					}
					else
					{
						NoisePoints[0]	= CheckNoisePoint;
					}
				}

				CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[0] * NoiseDistScale);

				// Determine the offset for the leading edge
				Location	= LastDrawPosition;
				EndPoint	= CurrDrawPosition;
				Right		= Location - EndPoint;
				Right.Normalize();
				if (((Source.UpVectorStepSize == 1) && (i == 0)) || (Source.UpVectorStepSize == 0))
				{
					//LastUp = Right ^ ViewDirection;
					LastUp = Right ^ (Location - ViewOrigin);
					if (!LastUp.Normalize())
					{
						LastUp = CameraToWorld.GetAxis(1);
					}
					THE_Up = LastUp;
				}
				else
				{
					LastUp = THE_Up;
				}

				if (SheetIndex)
				{
					Angle			= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
					QuatRotator		= FQuat(Right, Angle);
					WorkingLastUp	= QuatRotator.RotateVector(LastUp);
				}
				else
				{
					WorkingLastUp	= LastUp;
				}

				FLOAT	Taper	= 1.0f;

				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[0];
				}

				LastOffset.X	= WorkingLastUp.X * Size.X * Taper;
				LastOffset.Y	= WorkingLastUp.Y * Size.Y * Taper;
				LastOffset.Z	= WorkingLastUp.Z * Size.Z * Taper;

				// 'Lead' edge
				Vertex->Position	= Location + LastOffset;
				Vertex->OldPosition	= Location;
				Vertex->Size		= Size;
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				CheckVertexCount++;

				Vertex->Position	= Location - LastOffset;
				Vertex->OldPosition	= Location;
				Vertex->Size		= Size;
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				CheckVertexCount++;

				fU	+= TextureIncrement;

				for (INT StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
				{
					// Determine the current position by stepping the direct line and offsetting with the noise point. 
					CurrPosition		= LastPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;

					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[StepIndex] - NoisePoints[StepIndex];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[StepIndex] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
						if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[StepIndex].X) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[StepIndex].Y) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[StepIndex].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[StepIndex]	= NextNoise[StepIndex];
						}
						else
						{
							NoisePoints[StepIndex]	= CheckNoisePoint;
						}
					}

					CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[StepIndex] * NoiseDistScale);

					// Prep the next draw position to determine tangents
					UBOOL bTarget = FALSE;
					NextTargetPosition	= CurrPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;
					if (bLocked && ((StepIndex + 1) == BeamPayloadData->Steps))
					{
						// If we are locked, and the next step is the target point, set the draw position as such.
						// (ie, we are on the last noise point...)
						NextTargetDrawPosition	= BeamPayloadData->TargetPoint;
						if (Source.bTargetNoise)
						{
							if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
							{
								NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
								NoiseDir.Normalize();
								CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
								if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
									(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
									(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
								{
									NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
								}
								else
								{
									NoisePoints[Source.Frequency]	= CheckNoisePoint;
								}
							}

							NextTargetDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[Source.Frequency] * NoiseDistScale);
						}
						TargetTangent = BeamPayloadData->TargetTangent;
						fTargetStrength	= BeamPayloadData->TargetStrength;
					}
					else
					{
						// Just another noise point... offset the target to get the draw position.
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[StepIndex + 1] - NoisePoints[StepIndex + 1];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[StepIndex + 1] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
							if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[StepIndex + 1].X) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[StepIndex + 1].Y) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[StepIndex + 1].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[StepIndex + 1]	= NextNoise[StepIndex + 1];
							}
							else
							{
								NoisePoints[StepIndex + 1]	= CheckNoisePoint;
							}
						}

						NextTargetDrawPosition	= NextTargetPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[StepIndex + 1] * NoiseDistScale);

						TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
					}
					TargetTangent.Normalize();
					TargetTangent *= fTargetStrength;

					InterimDrawPosition = LastDrawPosition;
					// Tessellate between the current position and the last position
					for (INT TessIndex = 0; TessIndex < TessFactor; TessIndex++)
					{
						InterpDrawPos = CubicInterp(
							LastDrawPosition, LastTangent,
							CurrDrawPosition, TargetTangent,
							InvTessFactor * (TessIndex + 1));

						Location	= InterimDrawPosition;
						EndPoint	= InterpDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();
						if (Source.UpVectorStepSize == 0)
						{
							//Up = Right ^  (Location - CameraToWorld.GetOrigin());
							Up = Right ^ (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetAxis(1);
							}
						}
						else
						{
							Up = THE_Up;
						}

						if (SheetIndex)
						{
							Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
							QuatRotator	= FQuat(Right, Angle);
							WorkingUp	= QuatRotator.RotateVector(Up);
						}
						else
						{
							WorkingUp	= Up;
						}

						if (Source.TaperMethod != PEBTM_None)
						{
							check(TaperValues);
							Taper	= TaperValues[StepIndex * TessFactor + TessIndex];
						}

						Offset.X	= WorkingUp.X * Size.X * Taper;
						Offset.Y	= WorkingUp.Y * Size.Y * Taper;
						Offset.Z	= WorkingUp.Z * Size.Z * Taper;

						// Generate the vertex
						Vertex->Position	= InterpDrawPos + Offset;
						Vertex->OldPosition	= InterpDrawPos;
						Vertex->Size		= Size;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						Vertex->Position	= InterpDrawPos - Offset;
						Vertex->OldPosition	= InterpDrawPos;
						Vertex->Size		= Size;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						fU	+= TextureIncrement;
						InterimDrawPosition	= InterpDrawPos;
					}
					LastPosition		= CurrPosition;
					LastDrawPosition	= CurrDrawPosition;
					LastTangent			= TargetTangent;
				}

				if (bLocked)
				{
					// Draw the line from the last point to the target
					CurrDrawPosition	= BeamPayloadData->TargetPoint;
					if (Source.bTargetNoise)
					{
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
							if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
							}
							else
							{
								NoisePoints[Source.Frequency]	= CheckNoisePoint;
							}
						}

						CurrDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[Source.Frequency] * NoiseDistScale);
					}

					if (Source.bUseTarget)
					{
						TargetTangent = BeamPayloadData->TargetTangent;
					}
					else
					{
						NextTargetDrawPosition	= CurrPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;
						TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * 
							(NextTargetDrawPosition - LastDrawPosition);
					}
					TargetTangent.Normalize();
					TargetTangent *= fTargetStrength;

					// Tessellate this segment
					InterimDrawPosition = LastDrawPosition;
					for (INT TessIndex = 0; TessIndex < TessFactor; TessIndex++)
					{
						InterpDrawPos = CubicInterp(
							LastDrawPosition, LastTangent,
							CurrDrawPosition, TargetTangent,
							InvTessFactor * (TessIndex + 1));

						Location	= InterimDrawPosition;
						EndPoint	= InterpDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();
						if (Source.UpVectorStepSize == 0)
						{
							//Up = Right ^  (Location - CameraToWorld.GetOrigin());
							Up = Right ^ (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetAxis(1);
							}
						}
						else
						{
							Up = THE_Up;
						}

						if (SheetIndex)
						{
							Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
							QuatRotator	= FQuat(Right, Angle);
							WorkingUp	= QuatRotator.RotateVector(Up);
						}
						else
						{
							WorkingUp	= Up;
						}

						if (Source.TaperMethod != PEBTM_None)
						{
							check(TaperValues);
							Taper	= TaperValues[BeamPayloadData->Steps * TessFactor + TessIndex];
						}

						Offset.X	= WorkingUp.X * Size.X * Taper;
						Offset.Y	= WorkingUp.Y * Size.Y * Taper;
						Offset.Z	= WorkingUp.Z * Size.Z * Taper;

						// Generate the vertex
						Vertex->Position	= InterpDrawPos + Offset;
						Vertex->OldPosition	= InterpDrawPos;
						Vertex->Size		= Size;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						Vertex->Position	= InterpDrawPos - Offset;
						Vertex->OldPosition	= InterpDrawPos;
						Vertex->Size		= Size;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						fU	+= TextureIncrement;
						InterimDrawPosition	= InterpDrawPos;
					}
				}
			}
		}
		else
		{
			// Setup the current position as the source point
			CurrPosition		= BeamPayloadData->SourcePoint;
			CurrDrawPosition	= CurrPosition;

			// Setup the source tangent & strength
			if (Source.bUseSource)
			{
				// The source module will have determined the proper source tangent.
				LastTangent	= BeamPayloadData->SourceTangent;
				fStrength	= BeamPayloadData->SourceStrength;
			}
			else
			{
				// We don't have a source module, so use the orientation of the emitter.
				LastTangent	= WorldToLocal.GetAxis(0);
				fStrength	= Source.NoiseTangentStrength;
			}
			LastTangent.Normalize();
			LastTangent *= fStrength;

			// Setup the target tangent strength
			fTargetStrength	= Source.NoiseTangentStrength;

			// Set the last draw position to the source so we don't get 'under-hang'
			LastPosition		= CurrPosition;
			LastDrawPosition	= CurrDrawPosition;

			UBOOL	bLocked	= BEAM2_TYPEDATA_LOCKED(BeamPayloadData->Lock_Max_NumNoisePoints);

			FVector	UseNoisePoint, CheckNoisePoint;
			FVector	NoiseDir;

			for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				// Reset the texture coordinate
				fU					= 0.0f;
				LastPosition		= BeamPayloadData->SourcePoint;
				LastDrawPosition	= LastPosition;

				// Determine the current position by stepping the direct line and offsetting with the noise point. 
				CurrPosition		= LastPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;

				if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
				{
					NoiseDir		= NextNoise[0] - NoisePoints[0];
					NoiseDir.Normalize();
					CheckNoisePoint	= NoisePoints[0] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
					if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[0].X) < Source.NoiseLockRadius) &&
						(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[0].Y) < Source.NoiseLockRadius) &&
						(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[0].Z) < Source.NoiseLockRadius))
					{
						NoisePoints[0]	= NextNoise[0];
					}
					else
					{
						NoisePoints[0]	= CheckNoisePoint;
					}
				}

				CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[0] * NoiseDistScale);

				// Determine the offset for the leading edge
				Location	= LastDrawPosition;
				EndPoint	= CurrDrawPosition;
				Right		= Location - EndPoint;
				Right.Normalize();
				if (((Source.UpVectorStepSize == 1) && (i == 0)) || (Source.UpVectorStepSize == 0))
				{
					//LastUp = Right ^ ViewDirection;
					LastUp = Right ^ (Location - ViewOrigin);
					if (!LastUp.Normalize())
					{
						LastUp = CameraToWorld.GetAxis(1);
					}
					THE_Up = LastUp;
				}
				else
				{
					LastUp == THE_Up;
				}

				if (SheetIndex)
				{
					Angle			= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
					QuatRotator		= FQuat(Right, Angle);
					WorkingLastUp	= QuatRotator.RotateVector(LastUp);
				}
				else
				{
					WorkingLastUp	= LastUp;
				}

				FLOAT	Taper	= 1.0f;

				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[0];
				}

				LastOffset.X	= WorkingLastUp.X * Size.X * Taper;
				LastOffset.Y	= WorkingLastUp.Y * Size.Y * Taper;
				LastOffset.Z	= WorkingLastUp.Z * Size.Z * Taper;

				// 'Lead' edge
				Vertex->Position	= Location + LastOffset;
				Vertex->OldPosition	= Location;
				Vertex->Size		= Size;
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				CheckVertexCount++;

				Vertex->Position	= Location - LastOffset;
				Vertex->OldPosition	= Location;
				Vertex->Size		= Size;
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				CheckVertexCount++;

				fU	+= TextureIncrement;

				for (INT StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
				{
					// Determine the current position by stepping the direct line and offsetting with the noise point. 
					CurrPosition		= LastPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;

					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[StepIndex] - NoisePoints[StepIndex];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[StepIndex] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
						if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[StepIndex].X) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[StepIndex].Y) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[StepIndex].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[StepIndex]	= NextNoise[StepIndex];
						}
						else
						{
							NoisePoints[StepIndex]	= CheckNoisePoint;
						}
					}

					CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[StepIndex] * NoiseDistScale);

					// Prep the next draw position to determine tangents
					UBOOL bTarget = FALSE;
					NextTargetPosition	= CurrPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;
					if (bLocked && ((StepIndex + 1) == BeamPayloadData->Steps))
					{
						// If we are locked, and the next step is the target point, set the draw position as such.
						// (ie, we are on the last noise point...)
						NextTargetDrawPosition	= BeamPayloadData->TargetPoint;
						if (Source.bTargetNoise)
						{
							if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
							{
								NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
								NoiseDir.Normalize();
								CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
								if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
									(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
									(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
								{
									NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
								}
								else
								{
									NoisePoints[Source.Frequency]	= CheckNoisePoint;
								}
							}

							NextTargetDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[Source.Frequency] * NoiseDistScale);
						}
						TargetTangent = BeamPayloadData->TargetTangent;
						fTargetStrength	= BeamPayloadData->TargetStrength;
					}
					else
					{
						// Just another noise point... offset the target to get the draw position.
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[StepIndex + 1] - NoisePoints[StepIndex + 1];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[StepIndex + 1] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
							if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[StepIndex + 1].X) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[StepIndex + 1].Y) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[StepIndex + 1].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[StepIndex + 1]	= NextNoise[StepIndex + 1];
							}
							else
							{
								NoisePoints[StepIndex + 1]	= CheckNoisePoint;
							}
						}

						NextTargetDrawPosition	= NextTargetPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[StepIndex + 1] * NoiseDistScale);

						TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
					}
					TargetTangent.Normalize();
					TargetTangent *= fTargetStrength;

					InterimDrawPosition = LastDrawPosition;
					// Tessellate between the current position and the last position
					for (INT TessIndex = 0; TessIndex < TessFactor; TessIndex++)
					{
						InterpDrawPos = CubicInterp(
							LastDrawPosition, LastTangent,
							CurrDrawPosition, TargetTangent,
							InvTessFactor * (TessIndex + 1));

						CONSOLE_PREFETCH(Vertex+2);

						Location	= InterimDrawPosition;
						EndPoint	= InterpDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();
						if (Source.UpVectorStepSize == 0)
						{
							//Up = Right ^  (Location - CameraToWorld.GetOrigin());
							Up = Right ^ (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetAxis(1);
							}
						}
						else
						{
							Up = THE_Up;
						}

						if (SheetIndex)
						{
							Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
							QuatRotator	= FQuat(Right, Angle);
							WorkingUp	= QuatRotator.RotateVector(Up);
						}
						else
						{
							WorkingUp	= Up;
						}

						if (Source.TaperMethod != PEBTM_None)
						{
							check(TaperValues);
							Taper	= TaperValues[StepIndex * TessFactor + TessIndex];
						}

						Offset.X	= WorkingUp.X * Size.X * Taper;
						Offset.Y	= WorkingUp.Y * Size.Y * Taper;
						Offset.Z	= WorkingUp.Z * Size.Z * Taper;

						// Generate the vertex
						Vertex->Position	= InterpDrawPos + Offset;
						Vertex->OldPosition	= InterpDrawPos;
						Vertex->Size		= Size;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						Vertex->Position	= InterpDrawPos - Offset;
						Vertex->OldPosition	= InterpDrawPos;
						Vertex->Size		= Size;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						fU	+= TextureIncrement;
						InterimDrawPosition	= InterpDrawPos;
					}
					LastPosition		= CurrPosition;
					LastDrawPosition	= CurrDrawPosition;
					LastTangent			= TargetTangent;
				}

				if (bLocked)
				{
					// Draw the line from the last point to the target
					CurrDrawPosition	= BeamPayloadData->TargetPoint;
					if (Source.bTargetNoise)
					{
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
							if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
							}
							else
							{
								NoisePoints[Source.Frequency]	= CheckNoisePoint;
							}
						}

						CurrDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[Source.Frequency] * NoiseDistScale);
					}

					if (Source.bUseTarget)
					{
						TargetTangent = BeamPayloadData->TargetTangent;
					}
					else
					{
						NextTargetDrawPosition	= CurrPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;
						TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * 
							(NextTargetDrawPosition - LastDrawPosition);
					}
					TargetTangent.Normalize();
					TargetTangent *= fTargetStrength;

					// Tessellate this segment
					InterimDrawPosition = LastDrawPosition;
					for (INT TessIndex = 0; TessIndex < TessFactor; TessIndex++)
					{
						InterpDrawPos = CubicInterp(
							LastDrawPosition, LastTangent,
							CurrDrawPosition, TargetTangent,
							InvTessFactor * (TessIndex + 1));

						Location	= InterimDrawPosition;
						EndPoint	= InterpDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();
						if (Source.UpVectorStepSize == 0)
						{
							//Up = Right ^  (Location - CameraToWorld.GetOrigin());
							Up = Right ^ (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetAxis(1);
							}
						}
						else
						{
							Up = THE_Up;
						}

						if (SheetIndex)
						{
							Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
							QuatRotator	= FQuat(Right, Angle);
							WorkingUp	= QuatRotator.RotateVector(Up);
						}
						else
						{
							WorkingUp	= Up;
						}

						if (Source.TaperMethod != PEBTM_None)
						{
							check(TaperValues);
							Taper	= TaperValues[BeamPayloadData->Steps * TessFactor + TessIndex];
						}

						Offset.X	= WorkingUp.X * Size.X * Taper;
						Offset.Y	= WorkingUp.Y * Size.Y * Taper;
						Offset.Z	= WorkingUp.Z * Size.Z * Taper;

						// Generate the vertex
						Vertex->Position	= InterpDrawPos + Offset;
						Vertex->OldPosition	= InterpDrawPos;
						Vertex->Size		= Size;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						Vertex->Position	= InterpDrawPos - Offset;
						Vertex->OldPosition	= InterpDrawPos;
						Vertex->Size		= Size;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						fU	+= TextureIncrement;
						InterimDrawPosition	= InterpDrawPos;
					}
				}
				else
				if (BeamPayloadData->TravelRatio > KINDA_SMALL_NUMBER)
				{
					//@todo.SAS. Re-implement partial-segment beams
				}
			}
		}
	}

	check(CheckVertexCount <= Source.VertexCount);

	return TrianglesToRender;
}

INT FDynamicBeam2EmitterData::FillData_InterpolatedNoise(FAsyncBufferFillData& Me)
{
	INT	TrianglesToRender = 0;

	check(Source.InterpolationPoints > 0);
	check(Source.Frequency > 0);

	FParticleBeamTrailVertex* Vertex = (FParticleBeamTrailVertex*)Me.VertexData;
	FMatrix CameraToWorld = Me.View->ViewMatrix.Inverse();
	FVector ViewDirection	= Me.View->ViewMatrix.GetAxis(0);
	
	if (Source.Sheets <= 0)
	{
		Source.Sheets = 1;
	}

	FVector	ViewOrigin	= CameraToWorld.GetOrigin();

	// Frequency is the number of noise points to generate, evenly distributed along the line.
	if (Source.Frequency <= 0)
	{
		Source.Frequency = 1;
	}

	// NoiseTessellation is the amount of tessellation that should occur between noise points.
	INT	TessFactor	= Source.NoiseTessellation ? Source.NoiseTessellation : 1;
	
	FLOAT	InvTessFactor	= 1.0f / TessFactor;
	INT		i;

	// The last position processed
	FVector	LastPosition, LastDrawPosition, LastTangent;
	// The current position
	FVector	CurrPosition, CurrDrawPosition;
	// The target
	FVector	TargetPosition, TargetDrawPosition;
	// The next target
	FVector	NextTargetPosition, NextTargetDrawPosition, TargetTangent;
	// The interpreted draw position
	FVector InterpDrawPos;
	FVector	InterimDrawPosition;

	FVector	Size;

	FLOAT	Angle;
	FQuat	QuatRotator;

	FVector Location;
	FVector EndPoint;
	FVector Right;
	FVector Up;
	FVector WorkingUp;
	FVector LastUp;
	FVector WorkingLastUp;
	FVector Offset;
	FVector LastOffset;
	FLOAT	fStrength;
	FLOAT	fTargetStrength;

	FLOAT	fU;
	FLOAT	TextureIncrement	= 1.0f / (((Source.Frequency > 0) ? Source.Frequency : 1) * TessFactor);	// TTP #33140/33159

	FVector THE_Up = FVector(0.0f);

	INT	 CheckVertexCount	= 0;

	FMatrix WorldToLocal = SceneProxy->GetWorldToLocal();
	FMatrix LocalToWorld = SceneProxy->GetLocalToWorld();

	// Tessellate the beam along the noise points
	for (i = 0; i < Source.ActiveParticleCount; i++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * i);

		// Retrieve the beam data from the particle.
		FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
		FVector*				InterpolatedPoints	= NULL;
		FLOAT*					NoiseRate			= NULL;
		FLOAT*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		FLOAT*					TaperValues			= NULL;
		FLOAT*					NoiseDistanceScale	= NULL;

		BeamPayloadData = (FBeam2TypeDataPayload*)((BYTE*)Particle + Source.BeamDataOffset);
		if (BeamPayloadData->TriangleCount == 0)
		{
			continue;
		}
		if (BeamPayloadData->Steps == 0)
		{
			continue;
		}

		if (Source.InterpolatedPointsOffset != -1)
		{
			InterpolatedPoints = (FVector*)((BYTE*)Particle + Source.InterpolatedPointsOffset);
		}
		if (Source.NoiseRateOffset != -1)
		{
			NoiseRate = (FLOAT*)((BYTE*)Particle + Source.NoiseRateOffset);
		}
		if (Source.NoiseDeltaTimeOffset != -1)
		{
			NoiseDelta = (FLOAT*)((BYTE*)Particle + Source.NoiseDeltaTimeOffset);
		}
		if (Source.TargetNoisePointsOffset != -1)
		{
			TargetNoisePoints = (FVector*)((BYTE*)Particle + Source.TargetNoisePointsOffset);
		}
		if (Source.NextNoisePointsOffset != -1)
		{
			NextNoisePoints = (FVector*)((BYTE*)Particle + Source.NextNoisePointsOffset);
		}
		if (Source.TaperValuesOffset != -1)
		{
			TaperValues = (FLOAT*)((BYTE*)Particle + Source.TaperValuesOffset);
		}
		if (Source.NoiseDistanceScaleOffset != -1)
		{
			NoiseDistanceScale = (FLOAT*)((BYTE*)Particle + Source.NoiseDistanceScaleOffset);
		}

		FLOAT NoiseDistScale = 1.0f;
		if (NoiseDistanceScale)
		{
			NoiseDistScale = *NoiseDistanceScale;
		}

		INT Freq = BEAM2_TYPEDATA_FREQUENCY(BeamPayloadData->Lock_Max_NumNoisePoints);
		FLOAT InterpStepSize = (FLOAT)(BeamPayloadData->InterpolationSteps) / (FLOAT)(BeamPayloadData->Steps);
		FLOAT InterpFraction = appFractional(InterpStepSize);
		//UBOOL bInterpFractionIsZero = (Abs(InterpFraction) < KINDA_SMALL_NUMBER) ? TRUE : FALSE;
		UBOOL bInterpFractionIsZero = FALSE;
		INT InterpIndex = appTrunc(InterpStepSize);

		FVector* NoisePoints	= TargetNoisePoints;
		FVector* NextNoise		= NextNoisePoints;

		FLOAT NoiseRangeScaleFactor = Source.NoiseRangeScale;
		//@todo. How to handle no noise points?
		// If there are no noise points, why are we in here?
		if (NoisePoints == NULL)
		{
			continue;
		}

		// Pin the size to the X component
		Size	= FVector(Particle->Size.X * Source.Scale.X);

		// Setup the current position as the source point
		CurrPosition		= BeamPayloadData->SourcePoint;
		CurrDrawPosition	= CurrPosition;

		// Setup the source tangent & strength
		if (Source.bUseSource)
		{
			// The source module will have determined the proper source tangent.
			LastTangent	= BeamPayloadData->SourceTangent;
			fStrength	= Source.NoiseTangentStrength;
		}
		else
		{
			// We don't have a source module, so use the orientation of the emitter.
			LastTangent	= WorldToLocal.GetAxis(0);
			fStrength	= Source.NoiseTangentStrength;
		}
		LastTangent *= fStrength;

		// Setup the target tangent strength
		fTargetStrength	= Source.NoiseTangentStrength;

		// Set the last draw position to the source so we don't get 'under-hang'
		LastPosition		= CurrPosition;
		LastDrawPosition	= CurrDrawPosition;

		UBOOL	bLocked	= BEAM2_TYPEDATA_LOCKED(BeamPayloadData->Lock_Max_NumNoisePoints);

		FVector	UseNoisePoint, CheckNoisePoint;
		FVector	NoiseDir;

		for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
		{
			// Reset the texture coordinate
			fU					= 0.0f;
			LastPosition		= BeamPayloadData->SourcePoint;
			LastDrawPosition	= LastPosition;

			// Determine the current position by finding it along the interpolated path and 
			// offsetting with the noise point. 
			if (bInterpFractionIsZero)
			{
				CurrPosition = InterpolatedPoints[InterpIndex];
			}
			else
			{
				CurrPosition = 
					(InterpolatedPoints[InterpIndex + 0] * InterpFraction) + 
					(InterpolatedPoints[InterpIndex + 1] * (1.0f - InterpFraction));
			}

			if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
			{
				NoiseDir		= NextNoise[0] - NoisePoints[0];
				NoiseDir.Normalize();
				CheckNoisePoint	= NoisePoints[0] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
				if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[0].X) < Source.NoiseLockRadius) &&
					(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[0].Y) < Source.NoiseLockRadius) &&
					(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[0].Z) < Source.NoiseLockRadius))
				{
					NoisePoints[0]	= NextNoise[0];
				}
				else
				{
					NoisePoints[0]	= CheckNoisePoint;
				}
			}

			CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[0] * NoiseDistScale);

			// Determine the offset for the leading edge
			Location	= LastDrawPosition;
			EndPoint	= CurrDrawPosition;
			Right		= Location - EndPoint;
			Right.Normalize();
			if (((Source.UpVectorStepSize == 1) && (i == 0)) || (Source.UpVectorStepSize == 0))
			{
				//LastUp = Right ^ ViewDirection;
				LastUp = Right ^ (Location - ViewOrigin);
				if (!LastUp.Normalize())
				{
					LastUp = CameraToWorld.GetAxis(1);
				}
				THE_Up = LastUp;
			}
			else
			{
				LastUp = THE_Up;
			}

			if (SheetIndex)
			{
				Angle			= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
				QuatRotator		= FQuat(Right, Angle);
				WorkingLastUp	= QuatRotator.RotateVector(LastUp);
			}
			else
			{
				WorkingLastUp	= LastUp;
			}

			FLOAT	Taper	= 1.0f;

			if (Source.TaperMethod != PEBTM_None)
			{
				check(TaperValues);
				Taper	= TaperValues[0];
			}

			LastOffset.X	= WorkingLastUp.X * Size.X * Taper;
			LastOffset.Y	= WorkingLastUp.Y * Size.Y * Taper;
			LastOffset.Z	= WorkingLastUp.Z * Size.Z * Taper;

			// 'Lead' edge
			Vertex->Position	= Location + LastOffset;
			Vertex->OldPosition	= Location;
			Vertex->Size		= Size;
			Vertex->Tex_U		= fU;
			Vertex->Tex_V		= 0.0f;
			Vertex->Rotation	= Particle->Rotation;
			Vertex->Color		= Particle->Color;
			Vertex++;
			CheckVertexCount++;

			Vertex->Position	= Location - LastOffset;
			Vertex->OldPosition	= Location;
			Vertex->Size		= Size;
			Vertex->Tex_U		= fU;
			Vertex->Tex_V		= 1.0f;
			Vertex->Rotation	= Particle->Rotation;
			Vertex->Color		= Particle->Color;
			Vertex++;
			CheckVertexCount++;

			fU	+= TextureIncrement;

			check(InterpolatedPoints);
			for (INT StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
			{
				// Determine the current position by finding it along the interpolated path and 
				// offsetting with the noise point. 
				if (bInterpFractionIsZero)
				{
					CurrPosition = InterpolatedPoints[StepIndex  * InterpIndex];
				}
				else
				{
					if (StepIndex == (BeamPayloadData->Steps - 1))
					{
						CurrPosition = 
							(InterpolatedPoints[StepIndex * InterpIndex] * (1.0f - InterpFraction)) + 
							(BeamPayloadData->TargetPoint * InterpFraction);
					}
					else
					{
						CurrPosition = 
							(InterpolatedPoints[StepIndex * InterpIndex + 0] * (1.0f - InterpFraction)) + 
							(InterpolatedPoints[StepIndex * InterpIndex + 1] * InterpFraction);
					}
				}


				if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
				{
					NoiseDir		= NextNoise[StepIndex] - NoisePoints[StepIndex];
					NoiseDir.Normalize();
					CheckNoisePoint	= NoisePoints[StepIndex] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
					if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[StepIndex].X) < Source.NoiseLockRadius) &&
						(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[StepIndex].Y) < Source.NoiseLockRadius) &&
						(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[StepIndex].Z) < Source.NoiseLockRadius))
					{
						NoisePoints[StepIndex]	= NextNoise[StepIndex];
					}
					else
					{
						NoisePoints[StepIndex]	= CheckNoisePoint;
					}
				}

				CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[StepIndex] * NoiseDistScale);

				// Prep the next draw position to determine tangents
				UBOOL bTarget = FALSE;
				NextTargetPosition	= CurrPosition + BeamPayloadData->Direction * BeamPayloadData->StepSize;
				// Determine the current position by finding it along the interpolated path and 
				// offsetting with the noise point. 
				if (bInterpFractionIsZero)
				{
					if (StepIndex == (BeamPayloadData->Steps - 2))
					{
						NextTargetPosition = BeamPayloadData->TargetPoint;
					}
					else
					{
						NextTargetPosition = InterpolatedPoints[(StepIndex + 2) * InterpIndex + 0];
					}
				}
				else
				{
					if (StepIndex == (BeamPayloadData->Steps - 1))
					{
						NextTargetPosition = 
							(InterpolatedPoints[(StepIndex + 1) * InterpIndex + 0] * InterpFraction) + 
							(BeamPayloadData->TargetPoint * (1.0f - InterpFraction));
					}
					else
					{
						NextTargetPosition = 
							(InterpolatedPoints[(StepIndex + 1) * InterpIndex + 0] * InterpFraction) + 
							(InterpolatedPoints[(StepIndex + 1) * InterpIndex + 1] * (1.0f - InterpFraction));
					}
				}
				if (bLocked && ((StepIndex + 1) == BeamPayloadData->Steps))
				{
					// If we are locked, and the next step is the target point, set the draw position as such.
					// (ie, we are on the last noise point...)
					NextTargetDrawPosition	= BeamPayloadData->TargetPoint;
					if (Source.bTargetNoise)
					{
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
							if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
								(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
							}
							else
							{
								NoisePoints[Source.Frequency]	= CheckNoisePoint;
							}
						}

						NextTargetDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[Source.Frequency] * NoiseDistScale);
					}
					TargetTangent = BeamPayloadData->TargetTangent;
					fTargetStrength	= Source.NoiseTangentStrength;
				}
				else
				{
					// Just another noise point... offset the target to get the draw position.
					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[StepIndex + 1] - NoisePoints[StepIndex + 1];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[StepIndex + 1] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
						if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[StepIndex + 1].X) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[StepIndex + 1].Y) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[StepIndex + 1].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[StepIndex + 1]	= NextNoise[StepIndex + 1];
						}
						else
						{
							NoisePoints[StepIndex + 1]	= CheckNoisePoint;
						}
					}

					NextTargetDrawPosition	= NextTargetPosition + NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[StepIndex + 1] * NoiseDistScale);

					TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
				}
				TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
				TargetTangent.Normalize();
				TargetTangent *= fTargetStrength;

				InterimDrawPosition = LastDrawPosition;
				// Tessellate between the current position and the last position
				for (INT TessIndex = 0; TessIndex < TessFactor; TessIndex++)
				{
					InterpDrawPos = CubicInterp(
						LastDrawPosition, LastTangent,
						CurrDrawPosition, TargetTangent,
						InvTessFactor * (TessIndex + 1));

					Location	= InterimDrawPosition;
					EndPoint	= InterpDrawPos;
					Right		= Location - EndPoint;
					Right.Normalize();
					if (Source.UpVectorStepSize == 0)
					{
						//Up = Right ^  (Location - CameraToWorld.GetOrigin());
						Up = Right ^ (Location - ViewOrigin);
						if (!Up.Normalize())
						{
							Up = CameraToWorld.GetAxis(1);
						}
					}
					else
					{
						Up = THE_Up;
					}

					if (SheetIndex)
					{
						Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
						QuatRotator	= FQuat(Right, Angle);
						WorkingUp	= QuatRotator.RotateVector(Up);
					}
					else
					{
						WorkingUp	= Up;
					}

					if (Source.TaperMethod != PEBTM_None)
					{
						check(TaperValues);
						Taper	= TaperValues[StepIndex * TessFactor + TessIndex];
					}

					Offset.X	= WorkingUp.X * Size.X * Taper;
					Offset.Y	= WorkingUp.Y * Size.Y * Taper;
					Offset.Z	= WorkingUp.Z * Size.Z * Taper;

					// Generate the vertex
					Vertex->Position	= InterpDrawPos + Offset;
					Vertex->OldPosition	= InterpDrawPos;
					Vertex->Size		= Size;
					Vertex->Tex_U		= fU;
					Vertex->Tex_V		= 0.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
					CheckVertexCount++;

					Vertex->Position	= InterpDrawPos - Offset;
					Vertex->OldPosition	= InterpDrawPos;
					Vertex->Size		= Size;
					Vertex->Tex_U		= fU;
					Vertex->Tex_V		= 1.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
					CheckVertexCount++;

					fU	+= TextureIncrement;
					InterimDrawPosition	= InterpDrawPos;
				}
				LastPosition		= CurrPosition;
				LastDrawPosition	= CurrDrawPosition;
				LastTangent			= TargetTangent;
			}

			if (bLocked)
			{
				// Draw the line from the last point to the target
				CurrDrawPosition	= BeamPayloadData->TargetPoint;
				if (Source.bTargetNoise)
				{
					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * Source.NoiseSpeed * *NoiseRate;
						if ((Abs<FLOAT>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
							(Abs<FLOAT>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
						}
						else
						{
							NoisePoints[Source.Frequency]	= CheckNoisePoint;
						}
					}

					CurrDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformNormal(NoisePoints[Source.Frequency] * NoiseDistScale);
				}

				NextTargetDrawPosition	= BeamPayloadData->TargetPoint;
				if (Source.bUseTarget)
				{
					TargetTangent = BeamPayloadData->TargetTangent;
				}
				else
				{
					TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * 
						(NextTargetDrawPosition - LastDrawPosition);
					TargetTangent.Normalize();
				}
				TargetTangent *= fTargetStrength;

				// Tessellate this segment
				InterimDrawPosition = LastDrawPosition;
				for (INT TessIndex = 0; TessIndex < TessFactor; TessIndex++)
				{
					InterpDrawPos = CubicInterp(
						LastDrawPosition, LastTangent,
						CurrDrawPosition, TargetTangent,
						InvTessFactor * (TessIndex + 1));

					Location	= InterimDrawPosition;
					EndPoint	= InterpDrawPos;
					Right		= Location - EndPoint;
					Right.Normalize();
					if (Source.UpVectorStepSize == 0)
					{
						//Up = Right ^  (Location - CameraToWorld.GetOrigin());
						Up = Right ^ (Location - ViewOrigin);
						if (!Up.Normalize())
						{
							Up = CameraToWorld.GetAxis(1);
						}
					}
					else
					{
						Up = THE_Up;
					}

					if (SheetIndex)
					{
						Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
						QuatRotator	= FQuat(Right, Angle);
						WorkingUp	= QuatRotator.RotateVector(Up);
					}
					else
					{
						WorkingUp	= Up;
					}

					if (Source.TaperMethod != PEBTM_None)
					{
						check(TaperValues);
						Taper	= TaperValues[BeamPayloadData->Steps * TessFactor + TessIndex];
					}

					Offset.X	= WorkingUp.X * Size.X * Taper;
					Offset.Y	= WorkingUp.Y * Size.Y * Taper;
					Offset.Z	= WorkingUp.Z * Size.Z * Taper;

					// Generate the vertex
					Vertex->Position	= InterpDrawPos + Offset;
					Vertex->OldPosition	= InterpDrawPos;
					Vertex->Size		= Size;
					Vertex->Tex_U		= fU;
					Vertex->Tex_V		= 0.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
					CheckVertexCount++;

					Vertex->Position	= InterpDrawPos - Offset;
					Vertex->OldPosition	= InterpDrawPos;
					Vertex->Size		= Size;
					Vertex->Tex_U		= fU;
					Vertex->Tex_V		= 1.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
					CheckVertexCount++;

					fU	+= TextureIncrement;
					InterimDrawPosition	= InterpDrawPos;
				}
			}
			else
			if (BeamPayloadData->TravelRatio > KINDA_SMALL_NUMBER)
			{
				//@todo.SAS. Re-implement partial-segment beams
			}
		}
	}

	check(CheckVertexCount <= Source.VertexCount);

	return TrianglesToRender;
}

///////////////////////////////////////////////////////////////////////////////
//	FDynamicTrail2EmitterData
///////////////////////////////////////////////////////////////////////////////


/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicTrail2EmitterData::Init( UBOOL bInSelected )
{
	bSelected = bInSelected;

	check(Source.ActiveParticleCount < (16 * 1024));	// TTP #33330
	check(Source.ParticleStride < (2 * 1024));	// TTP #33330

	MaterialResource[0] = Source.MaterialInterface->GetRenderProxy(FALSE);
	MaterialResource[1] = GIsEditor ? Source.MaterialInterface->GetRenderProxy(TRUE) : MaterialResource[0];
	bUsesDynamicParameter = FALSE;
//	if( Source.MaterialInterface->GetMaterialResource() != NULL )
//	{
//		bUsesDynamicParameter = Source.MaterialInterface->GetMaterialResource()->GetUsesDynamicParameter();
//	}

	// We won't need this on the render thread
	Source.MaterialInterface = NULL;
}

/**
 *	Create the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicTrail2EmitterData::CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	// Create the vertex factory...
	if (VertexFactory == NULL)
	{
		VertexFactory = (FParticleBeamTrailVertexFactory*)(GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_BeamTrail));
		check(VertexFactory);
	}

	return (VertexFactory != NULL);
}

/**
 *	Release the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicTrail2EmitterData::ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	if (VertexFactory != NULL)
	{
		GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
		VertexFactory = NULL;
	}

	return TRUE;
}

/** Perform the actual work of filling the buffer, often called from another thread 
* @param Me Fill data structure
*/
void FDynamicTrail2EmitterData::DoBufferFill(FAsyncBufferFillData& Me)
{
	FillIndexData(Me);
	FillVertexData(Me);
}

INT FDynamicTrail2EmitterData::Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailRenderingTime);
	INC_DWORD_STAT(STAT_TrailParticlesRenderCalls);

	if (bValid == FALSE)
	{
		return 0;
	}

	check(PDI);
	if ((Source.VertexCount <= 0) || (Source.ActiveParticleCount <= 0) || (Source.IndexCount < 3))
	{
		return 0;
	}

	// Don't render if the material will be ignored
	if (PDI->IsMaterialIgnored(MaterialResource[bSelected]) && (!(View->Family->ShowFlags & SHOW_Wireframe)))
	{
		return 0;
	}

	const FAsyncBufferFillData& Data = EnsureFillCompletion(View);

	if (Data.OutTriangleCount == 0)
		{
		return 0;
	}

	FMeshBatch Mesh;
	FMeshBatchElement& BatchElement = Mesh.Elements(0);
	BatchElement.IndexBuffer			= NULL;
	Mesh.VertexFactory			= VertexFactory;
	Mesh.DynamicVertexData		= Data.VertexData;
	Mesh.DynamicVertexStride	= sizeof(FParticleBeamTrailVertex);
	BatchElement.DynamicIndexData		= Data.IndexData;
	BatchElement.DynamicIndexStride		= Source.IndexStride;
	Mesh.LCI					= NULL;
	if (Source.bUseLocalSpace == TRUE)
	{
		BatchElement.LocalToWorld = SceneProxy->GetLocalToWorld();
		BatchElement.WorldToLocal = SceneProxy->GetWorldToLocal();
	}
	else
	{
		BatchElement.LocalToWorld = FMatrix::Identity;
		BatchElement.WorldToLocal = FMatrix::Identity;
	}
	BatchElement.FirstIndex				= 0;
	BatchElement.NumPrimitives			= Data.OutTriangleCount;
	BatchElement.MinVertexIndex			= 0;
	BatchElement.MaxVertexIndex			= Source.VertexCount - 1;
	Mesh.UseDynamicData			= TRUE;
	Mesh.ReverseCulling			= Proxy->GetLocalToWorldDeterminant() < 0.0f ? TRUE : FALSE;
	Mesh.CastShadow				= Proxy->GetCastShadow();
	Mesh.DepthPriorityGroup		= (ESceneDepthPriorityGroup)DPGIndex;
	Mesh.bUsePreVertexShaderCulling = FALSE;
	Mesh.PlatformMeshData       = NULL;
	Mesh.bUseDownsampledTranslucency = ShouldRenderDownsampled(View, Proxy->GetBounds());

	if (AllowDebugViewmodes() && (View->Family->ShowFlags & SHOW_Wireframe) && !(View->Family->ShowFlags & SHOW_Materials))
	{
		Mesh.MaterialRenderProxy = Proxy->GetDeselectedWireframeMatInst();
	}
	else
	{
		check(Data.OutTriangleCount == Source.PrimitiveCount);
		Mesh.MaterialRenderProxy = MaterialResource[GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? bSelected : 0];
	}
	Mesh.Type = PT_TriangleStrip;

	const INT NumDraws = DrawRichMesh(
		PDI,
		Mesh,
		FLinearColor(1.0f, 0.0f, 0.0f),
		FLinearColor(1.0f, 1.0f, 0.0f),
		FLinearColor(1.0f, 1.0f, 1.0f),
		Proxy->GetPrimitiveSceneInfo(),
		GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? Proxy->IsSelected() : FALSE
		);

	INC_DWORD_STAT_BY(STAT_TrailParticlesTrianglesRendered, Mesh.GetNumPrimitives());
	return NumDraws;
}

/**
 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
 *  Only called for primitives that are visible and have bDynamicRelevance
 *
 *	@param	Proxy			The 'owner' particle system scene proxy
 *	@param	ViewFamily		The ViewFamily to pre-render for
 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
 *	@param	FrameNumber		The frame number of this pre-render
 */
void FDynamicTrail2EmitterData::PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber)
{
	if (bValid == FALSE)
	{
		return;
	}
		if (LastFramePreRendered < FrameNumber)
		{
		SceneProxy = Proxy;

			VertexFactory->SetScreenAlignment(Source.ScreenAlignment);
			VertexFactory->SetLockAxesFlag(EPAL_NONE);
		UBOOL bOnlyOneView = (GEngine && GEngine->GameViewport && (GEngine->GameViewport->GetCurrentSplitscreenType() == eSST_NONE)) ? TRUE : FALSE;

		BuildViewFillDataAndSubmit(ViewFamily,VisibilityMap,bOnlyOneView,Source.VertexCount,sizeof(FParticleBeamTrailVertex));

			// Set the frame tracker
			LastFramePreRendered = FrameNumber;
		}
	}

void FDynamicTrail2EmitterData::RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses)
{
}

INT FDynamicTrail2EmitterData::FillIndexData(struct FAsyncBufferFillData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailFillIndexTime);

	INT	TrianglesToRender = 0;

	// Trail2 polygons are packed and joined as follows:
	//
	// 1--3--5--7--9-...
	// |\ |\ |\ |\ |\...
	// | \| \| \| \| ...
	// 0--2--4--6--8-...
	//
	// (ie, the 'leading' edge of polygon (n) is the trailing edge of polygon (n+1)
	//
	// NOTE: This is primed for moving to tri-strips...
	//

	INT	Sheets = 1;
	Source.TessFactor = Max<INT>(Source.TessFactor, 1);

	FMatrix LocalToWorld = SceneProxy->GetLocalToWorld();

	UBOOL bWireframe = ((Data.View->Family->ShowFlags & SHOW_Wireframe) && !(Data.View->Family->ShowFlags & SHOW_Materials));

	if ((Data.IndexData == NULL) || (Data.IndexCount < Source.IndexCount))
	{
		if ((UINT)Source.IndexCount > 65535)
		{
			FString TemplateName = TEXT("*** UNKNOWN PSYS ***");
			UParticleSystemComponent* PSysComp = Cast<UParticleSystemComponent>(SceneProxy->GetPrimitiveSceneInfo()->Component);
			if (PSysComp)
			{
				if (PSysComp->Template)
				{
					TemplateName = PSysComp->Template->GetName();
				}
			}

			FString ErrorOut = FString::Printf(
				TEXT("*** PLEASE SUBMIT IMMEDIATELY ***%s")
				TEXT("Trail Index Error			- %s%s")
				TEXT("\tPosition				- %s%s")
				TEXT("\tPrimitiveCount			- %d%s")
				TEXT("\tVertexCount				- %d%s")
				TEXT("\tVertexData				- 0x%08x%s"),
				LINE_TERMINATOR,
				*TemplateName, LINE_TERMINATOR,
				*LocalToWorld.GetOrigin().ToString(), LINE_TERMINATOR,
				Source.PrimitiveCount, LINE_TERMINATOR,
				Source.VertexCount, LINE_TERMINATOR,
				Data.VertexData, LINE_TERMINATOR
				);
			ErrorOut += FString::Printf(
				TEXT("\tIndexCount				- %d%s")
				TEXT("\tIndexStride				- %d%s")
				TEXT("\tIndexData				- 0x%08x%s")
				TEXT("\tVertexFactory			- 0x%08x%s"),
				Source.IndexCount, LINE_TERMINATOR,
				Source.IndexStride, LINE_TERMINATOR,
				Data.IndexData, LINE_TERMINATOR,
				VertexFactory, LINE_TERMINATOR
				);
			ErrorOut += FString::Printf(
				TEXT("\tTrailDataOffset			- %d%s")
				TEXT("\tTaperValuesOffset		- %d%s")
				TEXT("\tParticleSourceOffset	- %d%s")
				TEXT("\tTrailCount				- %d%s"),
				Source.TrailDataOffset, LINE_TERMINATOR,
				Source.TaperValuesOffset, LINE_TERMINATOR,
				Source.ParticleSourceOffset, LINE_TERMINATOR,
				Source.TrailCount, LINE_TERMINATOR
				);
			ErrorOut += FString::Printf(
				TEXT("\tSheets					- %d%s")
				TEXT("\tTessFactor				- %d%s")
				TEXT("\tTessStrength			- %d%s")
				TEXT("\tTessFactorDistance		- %f%s")
				TEXT("\tActiveParticleCount		- %d%s"),
				Source.Sheets, LINE_TERMINATOR,
				Source.TessFactor, LINE_TERMINATOR,
				Source.TessStrength, LINE_TERMINATOR,
				Source.TessFactorDistance, LINE_TERMINATOR,
				Source.ActiveParticleCount, LINE_TERMINATOR
				);

			appErrorf(*ErrorOut);
		}
		if (Data.IndexData)
		{
			appFree(Data.IndexData);
		}
		Data.IndexData = appMalloc(Source.IndexCount * Source.IndexStride);
		Data.IndexCount = Source.IndexCount;

	}

	INT	CheckCount	= 0;

	WORD*	Index		= (WORD*)Data.IndexData;
	WORD	VertexIndex	= 0;

	for (INT Trail = 0; Trail < Source.ActiveParticleCount; Trail++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * Source.ParticleIndices(Trail));

		INT	CurrentOffset = Source.TrailDataOffset;

		FTrail2TypeDataPayload* TrailPayload = (FTrail2TypeDataPayload*)((BYTE*)Particle + CurrentOffset);
		CurrentOffset += sizeof(FTrail2TypeDataPayload);
		if (TRAIL_EMITTER_IS_START(TrailPayload->Flags) == FALSE)
		{
			continue;
		}

		INT LocalTrianglesToRender = TrailPayload->TriangleCount;
		if (LocalTrianglesToRender <= 0)
		{
			continue;
		}

		FLOAT* TaperValues = (FLOAT*)((BYTE*)Particle + CurrentOffset);
		CurrentOffset += sizeof(FLOAT);

		for (INT SheetIndex = 0; SheetIndex < Sheets; SheetIndex++)
		{
			// 2 triangles per tessellation factor
			if (SheetIndex == 0)
			{
				// Only need the starting two for the first sheet
				*(Index++) = VertexIndex++;	// SheetIndex + 0
				*(Index++) = VertexIndex++;	// SheetIndex + 1

				CheckCount += 2;
			}

			// Sequentially step through each triangle - 1 vertex per triangle
			for (INT i = 0; i < LocalTrianglesToRender; i++)
			{
				*(Index++) = VertexIndex++;
				CheckCount++;
				TrianglesToRender++;
			}

			// Degenerate tris
			if ((SheetIndex + 1) < Sheets)
			{
				*(Index++) = VertexIndex - 1;	// Last vertex of the previous sheet
				*(Index++) = VertexIndex;		// First vertex of the next sheet
				*(Index++) = VertexIndex++;		// First vertex of the next sheet
				*(Index++) = VertexIndex++;		// Second vertex of the next sheet
				TrianglesToRender += 4;
				CheckCount += 4;
			}
		}

		if ((Trail + 1) < Source.TrailCount)
		{
			*(Index++) = VertexIndex - 1;	// Last vertex of the previous sheet
			*(Index++) = VertexIndex;		// First vertex of the next sheet
			*(Index++) = VertexIndex++;		// First vertex of the next sheet
			*(Index++) = VertexIndex++;		// Second vertex of the next sheet
			TrianglesToRender += 4;
			CheckCount += 4;
		}
	}

	Data.OutTriangleCount = TrianglesToRender;
	return TrianglesToRender;
}

INT FDynamicTrail2EmitterData::FillVertexData(struct FAsyncBufferFillData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailFillVertexTime);
	check(SceneProxy);

	INT	TrianglesToRender = 0;
	//@todo. DYNAMIC PARAMETER SUPPORT
	FParticleBeamTrailVertex* Vertex = (FParticleBeamTrailVertex*)(Data.VertexData);
	FMatrix CameraToWorld = Data.View->ViewMatrix.Inverse();

	Source.TessFactor = Max<INT>(Source.TessFactor, 1);
	Source.Sheets = Max<INT>(Source.Sheets, 1);

	FLOAT	InvTessFactor	= 1.0f / (FLOAT)Source.TessFactor;
	FVector	InterpDrawPos;

	FVector	ViewOrigin	= CameraToWorld.GetOrigin();

	FVector	Offset, LastOffset;
	FLOAT	TextureIncrement;
	FLOAT	fU;
	FLOAT	Angle;
	FQuat	QuatRotator(0, 0, 0, 0);
	FVector	CurrPosition, CurrTangent;
	FLOAT CurrSize;
	FVector EndPoint, Location, Right;
	FVector Up, WorkingUp, NextUp, WorkingNextUp;
	FVector	NextPosition, NextTangent;
	FLOAT NextSize;
	FVector	TempDrawPos;
	FLinearColor CurrLinearColor, NextLinearColor, InterpLinearColor;

	FVector	TessDistCheck;
	INT		SegmentTessFactor;
#if defined(_TRAIL2_TESSELLATE_SCALE_BY_DISTANCE_)
	FLOAT	TessRatio;
#endif	//#if defined(_TRAIL2_TESSELLATE_SCALE_BY_DISTANCE_)

	FMatrix LocalToWorld = SceneProxy->GetLocalToWorld();

	INT		PackedVertexCount	= 0;
	for (INT i = 0; i < Source.ActiveParticleCount; i++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.ParticleData.GetData() + Source.ParticleStride * Source.ParticleIndices(i));

		INT	CurrentOffset = Source.TrailDataOffset;

		FTrail2TypeDataPayload* TrailPayload = (FTrail2TypeDataPayload*)((BYTE*)Particle + CurrentOffset);
		CurrentOffset += sizeof(FTrail2TypeDataPayload);

		if (TRAIL_EMITTER_IS_START(TrailPayload->Flags))
		{
			FLOAT* TaperValues = (FLOAT*)((BYTE*)Particle + CurrentOffset);
			CurrentOffset += sizeof(FLOAT);

			// Pin the size to the X component
			CurrSize	= Particle->Size.X * Source.Scale.X;
			CurrLinearColor	= Particle->Color;

			//@todo. This will only work for a single trail!
			TextureIncrement	= 1.0f / (Source.TessFactor * Source.ActiveParticleCount + 1);
			UBOOL	bFirstInSheet	= TRUE;
			for (INT SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				if (SheetIndex)
				{
					Angle		= ((FLOAT)PI / (FLOAT)Source.Sheets) * SheetIndex;
					QuatRotator	= FQuat(Right, Angle);
				}

				fU	= 0.0f;

				// Set the current position to the source...
				/***
				if (TrailSource)
				{
				//					TrailSource->ResolveSourcePoint(Owner, *Particle, *TrailData, CurrPosition, CurrTangent);
				}
				else
				***/
				{
					FVector	Dir = LocalToWorld.GetAxis(0);
					Dir.Normalize();
					CurrTangent	=  Dir * Source.TessStrength;
				}

				CurrPosition	= Source.SourcePosition(TrailPayload->TrailIndex);

				NextPosition	= Particle->Location;
				NextSize		= Particle->Size.X * Source.Scale.X;
				NextTangent		= TrailPayload->Tangent * Source.TessStrength;
				NextLinearColor	= Particle->Color;
				TempDrawPos		= CurrPosition;

				if (!bClipSourceSegement)
				{
					SegmentTessFactor	= Source.TessFactor;
#if defined(_TRAIL2_TESSELLATE_SCALE_BY_DISTANCE_)
					if (TrailTypeData->TessellationFactorDistance > KINDA_SMALL_NUMBER)
					{
						TessDistCheck		= (CurrPosition - NextPosition);
						TessRatio			= TessDistCheck.Size() / Source.TessFactorDistance;
						if (TessRatio <= 0.0f)
						{
							SegmentTessFactor	= 1;
						}
						else if (TessRatio < 1.0f)
						{
							SegmentTessFactor	= appTrunc((Source.TessFactor + 1) * TessRatio);
						}
					}
#endif	//#if defined(_TRAIL2_TESSELLATE_SCALE_BY_DISTANCE_)
					// Tessellate the current to next...
#if !defined(_TRAIL2_TESSELLATE_TO_SOURCE_)
					SegmentTessFactor = 1;
#endif	//#if !defined(_TRAIL2_TESSELLATE_TO_SOURCE_)
					InvTessFactor	= 1.0f / SegmentTessFactor;

					for (INT TessIndex = 0; TessIndex < SegmentTessFactor; TessIndex++)
					{
						InterpDrawPos = CubicInterp(
							CurrPosition, CurrTangent,
							NextPosition, NextTangent,
							InvTessFactor * (TessIndex + 1));
						InterpLinearColor = Lerp<FLinearColor>(
							CurrLinearColor, NextLinearColor, InvTessFactor * (TessIndex + 1));

						EndPoint	= InterpDrawPos;
						Location	= TempDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();

						if (bFirstInSheet)
						{
							Up	= Right ^  (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetAxis(1);
							}
							if (SheetIndex)
							{
								WorkingUp	= QuatRotator.RotateVector(Up);
							}
							else
							{
								WorkingUp	= Up;
							}

							if (WorkingUp.IsNearlyZero())
							{
								WorkingUp	= CameraToWorld.GetAxis(2);
								WorkingUp.Normalize();
							}

							// Setup the lead verts
							Vertex->Position	= Location + WorkingUp * CurrSize;
							Vertex->OldPosition	= Location;
							Vertex->Size.X		= CurrSize;
							Vertex->Size.Y		= CurrSize;
							Vertex->Size.Z		= CurrSize;
							Vertex->Tex_U		= fU;
							Vertex->Tex_V		= 0.0f;
							Vertex->Rotation	= Particle->Rotation;
							Vertex->Color		= CurrLinearColor;
							Vertex++;
							PackedVertexCount++;

							Vertex->Position	= Location - WorkingUp * CurrSize;
							Vertex->OldPosition	= Location;
							Vertex->Size.X		= CurrSize;
							Vertex->Size.Y		= CurrSize;
							Vertex->Size.Z		= CurrSize;
							Vertex->Tex_U		= fU;
							Vertex->Tex_V		= 1.0f;
							Vertex->Rotation	= Particle->Rotation;
							Vertex->Color		= CurrLinearColor;
							Vertex++;
							PackedVertexCount++;

							fU	+= TextureIncrement;
							bFirstInSheet	= FALSE;
						}

						// Setup the next verts
						NextUp	= Right ^  (EndPoint - ViewOrigin);
						if (!NextUp.Normalize())
						{
							NextUp = CameraToWorld.GetAxis(1);
						}
						if (SheetIndex)
						{
							WorkingNextUp	= QuatRotator.RotateVector(NextUp);
						}
						else
						{
							WorkingNextUp	= NextUp;
						}

						if (WorkingNextUp.IsNearlyZero())
						{
							WorkingNextUp	= CameraToWorld.GetAxis(2);
							WorkingNextUp.Normalize();
						}
						Vertex->Position	= EndPoint + WorkingNextUp * NextSize;
						Vertex->OldPosition	= EndPoint;
						Vertex->Size.X		= NextSize;
						Vertex->Size.Y		= NextSize;
						Vertex->Size.Z		= NextSize;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= InterpLinearColor;
						Vertex++;
						PackedVertexCount++;

						Vertex->Position	= EndPoint - WorkingNextUp * NextSize;
						Vertex->OldPosition	= EndPoint;
						Vertex->Size.X		= NextSize;
						Vertex->Size.Y		= NextSize;
						Vertex->Size.Z		= NextSize;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= InterpLinearColor;
						Vertex++;
						PackedVertexCount++;

						fU	+= TextureIncrement;

						TempDrawPos = InterpDrawPos;
					}
				}


				CurrPosition	= NextPosition;
				CurrTangent		= NextTangent;
				CurrSize		= NextSize;
				CurrLinearColor	= NextLinearColor;

				UBOOL bDone = TRAIL_EMITTER_IS_ONLY(TrailPayload->Flags);
				while (!bDone)
				{
					// Grab the next particle
					INT	NextIndex	= TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags);

					DECLARE_PARTICLE_PTR(NextParticle, Source.ParticleData.GetData() + Source.ParticleStride * NextIndex);

					CurrentOffset = Source.TrailDataOffset;

					TrailPayload = (FTrail2TypeDataPayload*)((BYTE*)NextParticle + CurrentOffset);
					CurrentOffset += sizeof(FTrail2TypeDataPayload);

					TaperValues = (FLOAT*)((BYTE*)NextParticle + CurrentOffset);
					CurrentOffset += sizeof(FLOAT);

					NextPosition	= NextParticle->Location;
					NextTangent		= TrailPayload->Tangent * Source.TessStrength;
					NextSize		= NextParticle->Size.X * Source.Scale.X;
					NextLinearColor	= NextParticle->Color;

					TempDrawPos	= CurrPosition;

					SegmentTessFactor	= Source.TessFactor;
#if defined(_TRAIL2_TESSELLATE_SCALE_BY_DISTANCE_)
					if (TrailTypeData->TessellationFactorDistance > KINDA_SMALL_NUMBER)
					{
						TessDistCheck		= (CurrPosition - NextPosition);
						TessRatio			= TessDistCheck.Size() / Source.TessFactorDistance;
						if (TessRatio <= 0.0f)
						{
							SegmentTessFactor	= 1;
						}
						else
						if (TessRatio < 1.0f)
						{
							SegmentTessFactor	= appTrunc((TessFactor + 1) * TessRatio);
						}
					}
#endif	//#if defined(_TRAIL2_TESSELLATE_SCALE_BY_DISTANCE_)
					InvTessFactor	= 1.0f / SegmentTessFactor;

					for (INT TessIndex = 0; TessIndex < SegmentTessFactor; TessIndex++)
					{
						InterpDrawPos = CubicInterp(
							CurrPosition, CurrTangent,
							NextPosition, NextTangent,
							InvTessFactor * (TessIndex + 1));
						InterpLinearColor = Lerp<FLinearColor>(
							CurrLinearColor, NextLinearColor, InvTessFactor * (TessIndex + 1));

						EndPoint	= InterpDrawPos;
						Location	= TempDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();

						if (FALSE ) // (bFirstInSheet)
						{
							Up	= Right ^  (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetAxis(1);
							}
							if (SheetIndex)
							{
								WorkingUp	= QuatRotator.RotateVector(Up);
							}
							else
							{
								WorkingUp	= Up;
							}

							if (WorkingUp.IsNearlyZero())
							{
								WorkingUp	= CameraToWorld.GetAxis(2);
								WorkingUp.Normalize();
							}

							// Setup the lead verts
							Vertex->Position	= Location + WorkingUp * CurrSize;
							Vertex->OldPosition	= Location;
							Vertex->Size.X		= CurrSize;
							Vertex->Size.Y		= CurrSize;
							Vertex->Size.Z		= CurrSize;
							Vertex->Tex_U		= fU;
							Vertex->Tex_V		= 0.0f;
							Vertex->Rotation	= Particle->Rotation;
							Vertex->Color		= CurrLinearColor;
							Vertex++;
							PackedVertexCount++;

							Vertex->Position	= Location - WorkingUp * CurrSize;
							Vertex->OldPosition	= Location;
							Vertex->Size.X		= CurrSize;
							Vertex->Size.Y		= CurrSize;
							Vertex->Size.Z		= CurrSize;
							Vertex->Tex_U		= fU;
							Vertex->Tex_V		= 1.0f;
							Vertex->Rotation	= Particle->Rotation;
							Vertex->Color		= CurrLinearColor;
							Vertex++;
							PackedVertexCount++;

							fU	+= TextureIncrement;
							bFirstInSheet	= FALSE;
						}

						// Setup the next verts
						NextUp	= Right ^  (EndPoint - ViewOrigin);
						if (!NextUp.Normalize())
						{
							NextUp = CameraToWorld.GetAxis(1);
						}
						if (SheetIndex)
						{
							WorkingNextUp	= QuatRotator.RotateVector(NextUp);
						}
						else
						{
							WorkingNextUp	= NextUp;
						}

						if (WorkingNextUp.IsNearlyZero())
						{
							WorkingNextUp	= CameraToWorld.GetAxis(2);
							WorkingNextUp.Normalize();
						}
						Vertex->Position	= EndPoint + WorkingNextUp * NextSize;
						Vertex->OldPosition	= EndPoint;
						Vertex->Size.X		= NextSize;
						Vertex->Size.Y		= NextSize;
						Vertex->Size.Z		= NextSize;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= NextParticle->Rotation;
						Vertex->Color		= InterpLinearColor;
						Vertex++;
						PackedVertexCount++;

						Vertex->Position	= EndPoint - WorkingNextUp * NextSize;
						Vertex->OldPosition	= EndPoint;
						Vertex->Size.X		= NextSize;
						Vertex->Size.Y		= NextSize;
						Vertex->Size.Z		= NextSize;
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= NextParticle->Rotation;
						Vertex->Color		= InterpLinearColor;
						Vertex++;
						PackedVertexCount++;

						fU	+= TextureIncrement;

						TempDrawPos	= InterpDrawPos;
					}

					CurrPosition	= NextPosition;
					CurrTangent		= NextTangent;
					CurrSize		= NextSize;
					CurrLinearColor	= NextLinearColor;

					if (TRAIL_EMITTER_IS_END(TrailPayload->Flags) ||
						TRAIL_EMITTER_IS_ONLY(TrailPayload->Flags))
					{
						bDone = TRUE;
					}
				}
			}
		}
	}

	return TrianglesToRender;
}

//
//	FDynamicTrailsEmitterData
//
/** Dynamic emitter data for Ribbon emitters */
/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicTrailsEmitterData::Init(UBOOL bInSelected)
{
	bSelected = bInSelected;

	check(SourcePointer->ActiveParticleCount < (16 * 1024));	// TTP #33330
	check(SourcePointer->ParticleStride < (2 * 1024));			// TTP #33330

	MaterialResource[0] = SourcePointer->MaterialInterface->GetRenderProxy(FALSE);
	MaterialResource[1] = GIsEditor ? SourcePointer->MaterialInterface->GetRenderProxy(TRUE) : MaterialResource[0];
	bUsesDynamicParameter = FALSE;
	if (SourcePointer->MaterialInterface->GetMaterialResource() != NULL)
	{
		bUsesDynamicParameter = SourcePointer->MaterialInterface->GetMaterialResource()->GetUsesDynamicParameter();
	}

	// We won't need this on the render thread
	SourcePointer->MaterialInterface = NULL;
}

/**
 *	Create the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicTrailsEmitterData::CreateRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	// Create the vertex factory...
	if (VertexFactory == NULL)
	{
		if (bUsesDynamicParameter == FALSE)
		{
			VertexFactory = (FParticleBeamTrailVertexFactory*)(GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_BeamTrail));
		}
		else
		{
			VertexFactory = (FParticleBeamTrailDynamicParameterVertexFactory*)(GParticleVertexFactoryPool.GetParticleVertexFactory(PVFT_BeamTrail_DynamicParameter));
		}
		check(VertexFactory);
	}

	return (VertexFactory != NULL);
}

/**
 *	Release the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	UBOOL			TRUE if successful, FALSE if failed
 */
UBOOL FDynamicTrailsEmitterData::ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	if (VertexFactory != NULL)
	{
		GParticleVertexFactoryPool.ReturnParticleVertexFactory(VertexFactory);
		VertexFactory = NULL;
	}

	return TRUE;
}

/** Perform the actual work of filling the buffer, often called from another thread 
* @param Me Fill data structure
*/
void FDynamicTrailsEmitterData::DoBufferFill(FAsyncBufferFillData& Me)
{
	FillIndexData(Me);
	FillVertexData(Me);
}

// Render thread only draw call
INT FDynamicTrailsEmitterData::Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailRenderingTime);
	INC_DWORD_STAT(STAT_TrailParticlesRenderCalls);

	if (bValid == FALSE)
	{
		check(!bAsyncTaskOutstanding);
		return 0;
	}

	check(PDI);

	// Don't render if the material will be ignored
	if (PDI->IsMaterialIgnored(MaterialResource[bSelected]) && (!(View->Family->ShowFlags & SHOW_Wireframe)))
	{
		return 0;
	}

	INT RenderedPrimitiveCount = 0;

	const FAsyncBufferFillData& Data = EnsureFillCompletion(View);

	if ((SourcePointer->VertexCount <= 0) || (SourcePointer->ActiveParticleCount <= 0) || (SourcePointer->IndexCount < 3))
	{
		return 0;
	}

	if (Data.OutTriangleCount == 0)
	{
		return 0;
	}

	INT NumDraws = 0;
	if (bRenderGeometry == TRUE)
	{
		FMeshBatch Mesh;
		FMeshBatchElement& BatchElement = Mesh.Elements(0);
		BatchElement.IndexBuffer			= NULL;
		Mesh.VertexFactory			= VertexFactory;
		Mesh.DynamicVertexData		= Data.VertexData;
		INT VertexStride = sizeof(FParticleBeamTrailVertex);
		if (bUsesDynamicParameter)
		{
			VertexStride = sizeof(FParticleBeamTrailVertexDynamicParameter);
		}
		Mesh.DynamicVertexStride	= VertexStride;
		BatchElement.DynamicIndexData		= Data.IndexData;
		BatchElement.DynamicIndexStride		= SourcePointer->IndexStride;
		Mesh.LCI					= NULL;
// 		if (Source.bUseLocalSpace == TRUE)
// 		{
// 			BatchElement.LocalToWorld = Proxy->GetLocalToWorld();
// 			BatchElement.WorldToLocal = Proxy->GetWorldToLocal();
// 		}
// 		else
// 		{
			BatchElement.LocalToWorld = FMatrix::Identity;
			BatchElement.WorldToLocal = FMatrix::Identity;
// 		}
		BatchElement.FirstIndex				= 0;
		BatchElement.NumPrimitives			= Data.OutTriangleCount;
		BatchElement.MinVertexIndex			= 0;
		BatchElement.MaxVertexIndex			= SourcePointer->VertexCount - 1;
		Mesh.UseDynamicData			= TRUE;
		Mesh.ReverseCulling			= Proxy->GetLocalToWorldDeterminant() < 0.0f ? TRUE : FALSE;
		Mesh.CastShadow				= Proxy->GetCastShadow();
		Mesh.DepthPriorityGroup		= (ESceneDepthPriorityGroup)DPGIndex;
		Mesh.bUsePreVertexShaderCulling = FALSE;
		Mesh.PlatformMeshData       = NULL;
		Mesh.bUseDownsampledTranslucency = ShouldRenderDownsampled(View, Proxy->GetBounds());

		if (AllowDebugViewmodes() && (View->Family->ShowFlags & SHOW_Wireframe) && !(View->Family->ShowFlags & SHOW_Materials))
		{
			Mesh.MaterialRenderProxy = Proxy->GetDeselectedWireframeMatInst();
		}
		else
		{
#if !FINAL_RELEASE
			if (Data.OutTriangleCount != SourcePointer->PrimitiveCount)
			{
				debugf(TEXT("Data.OutTriangleCount = %4d vs. SourcePrimCount = %4d"), Data.OutTriangleCount, SourcePointer->PrimitiveCount);

				INT CheckTrailCount = 0;
				INT CheckTriangleCount = 0;
				for (INT ParticleIdx = 0; ParticleIdx < SourcePointer->ActiveParticleCount; ParticleIdx++)
				{
					INT CurrentIndex = SourcePointer->ParticleIndices(ParticleIdx);
					DECLARE_PARTICLE_PTR(CheckParticle, SourcePointer->ParticleData.GetData() + SourcePointer->ParticleStride * CurrentIndex);
					FTrailsBaseTypeDataPayload* TrailPayload = (FTrailsBaseTypeDataPayload*)((BYTE*)CheckParticle + SourcePointer->TrailDataOffset);
					if (TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == FALSE)
					{
						continue;
					}

					debugf(TEXT("Trail %2d has %5d triangles"), TrailPayload->TrailIndex, TrailPayload->TriangleCount);
					CheckTriangleCount += TrailPayload->TriangleCount;
					CheckTrailCount++;
				}
				debugf(TEXT("Total 'live' trail count = %d"), CheckTrailCount);
				debugf(TEXT("\t%5d triangles total (not counting degens)"), CheckTriangleCount);
			}
#endif

			checkf(Data.OutTriangleCount <= SourcePointer->PrimitiveCount, TEXT("Data.OutTriangleCount = %4d vs. SourcePrimCount = %4d"), Data.OutTriangleCount, SourcePointer->PrimitiveCount);
			Mesh.MaterialRenderProxy = MaterialResource[GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? bSelected : 0];
		}
		Mesh.Type = PT_TriangleStrip;

		NumDraws += DrawRichMesh(
			PDI,
			Mesh,
			FLinearColor(1.0f, 0.0f, 0.0f),
			FLinearColor(1.0f, 1.0f, 0.0f),
			FLinearColor(1.0f, 1.0f, 1.0f),
			Proxy->GetPrimitiveSceneInfo(),
			GIsEditor && (View->Family->ShowFlags & SHOW_Selection) ? Proxy->IsSelected() : FALSE
			);

		RenderedPrimitiveCount = Mesh.GetNumPrimitives();
	}

	RenderDebug(PDI, View, DPGIndex, FALSE);

	INC_DWORD_STAT_BY(STAT_TrailParticlesTrianglesRendered, RenderedPrimitiveCount);
	return NumDraws;
}

/**
 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
 *  Only called for primitives that are visible and have bDynamicRelevance
 *
 *	@param	Proxy			The 'owner' particle system scene proxy
 *	@param	ViewFamily		The ViewFamily to pre-render for
 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
 *	@param	FrameNumber		The frame number of this pre-render
 */
void FDynamicTrailsEmitterData::PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber)
{
	if (bValid == FALSE)
	{
		return;
	}

		// Only need to do this once per-view
		if (LastFramePreRendered < FrameNumber)
		{
		SceneProxy = Proxy;
			VertexFactory->SetScreenAlignment(SourcePointer->ScreenAlignment);
			VertexFactory->SetLockAxesFlag(EPAL_NONE);
				INT VertexStride = sizeof(FParticleBeamTrailVertex);
				if (bUsesDynamicParameter == TRUE)
				{
					VertexStride = sizeof(FParticleBeamTrailVertexDynamicParameter);
				}

		UBOOL bOnlyOneView = ShouldUsePrerenderView() || ((GEngine && GEngine->GameViewport && (GEngine->GameViewport->GetCurrentSplitscreenType() == eSST_NONE)) ? TRUE : FALSE);

		BuildViewFillDataAndSubmit(ViewFamily,VisibilityMap,bOnlyOneView,SourcePointer->VertexCount,VertexStride);

			// Set the frame tracker
			LastFramePreRendered = FrameNumber;
		}
	}

void FDynamicTrailsEmitterData::RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses)
{
	// Can't do anything in here...
}

// Data fill functions
INT FDynamicTrailsEmitterData::FillIndexData(struct FAsyncBufferFillData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailFillIndexTime);

	INT	TrianglesToRender = 0;

	// Trails polygons are packed and joined as follows:
	//
	// 1--3--5--7--9-...
	// |\ |\ |\ |\ |\...
	// | \| \| \| \| ...
	// 0--2--4--6--8-...
	//
	// (ie, the 'leading' edge of polygon (n) is the trailing edge of polygon (n+1)
	//

	INT	Sheets = 1;
	INT	TessFactor = 1;//Max<INT>(Source.TessFactor, 1);

	UBOOL bWireframe = ((Data.View->Family->ShowFlags & SHOW_Wireframe) && !(Data.View->Family->ShowFlags & SHOW_Materials));

	if ((Data.IndexData == NULL) || (Data.IndexCount < SourcePointer->IndexCount))
	{
		check((UINT)SourcePointer->IndexCount <= 65535);
		if (Data.IndexData)
		{
			appFree(Data.IndexData);
		}
		Data.IndexData = appMalloc(SourcePointer->IndexCount * SourcePointer->IndexStride);
		Data.IndexCount = SourcePointer->IndexCount;
	}

	INT	CheckCount	= 0;

	WORD*	Index		= (WORD*)Data.IndexData;
	WORD	VertexIndex	= 0;

	INT CurrentTrail = 0;
	for (INT ParticleIdx = 0; ParticleIdx < SourcePointer->ActiveParticleCount; ParticleIdx++)
	{
		INT CurrentIndex = SourcePointer->ParticleIndices(ParticleIdx);
		DECLARE_PARTICLE_PTR(Particle, SourcePointer->ParticleData.GetData() + SourcePointer->ParticleStride * CurrentIndex);

		FTrailsBaseTypeDataPayload* TrailPayload = (FTrailsBaseTypeDataPayload*)((BYTE*)Particle + SourcePointer->TrailDataOffset);
		if (TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == FALSE)
		{
			continue;
		}

		INT LocalTrianglesToRender = TrailPayload->TriangleCount;
		if (LocalTrianglesToRender == 0)
		{
			continue;
		}

		//@todo. Support clip source segment

		// For the source particle itself
		if (CurrentTrail == 0)
		{
			*(Index++) = VertexIndex++;		// The first vertex..
			*(Index++) = VertexIndex++;		// The second index..
			CheckCount += 2;
		}
		else
		{
			// Add the verts to join this trail with the previous one
			*(Index++) = VertexIndex - 1;	// Last vertex of the previous sheet
			*(Index++) = VertexIndex;		// First vertex of the next sheet
			*(Index++) = VertexIndex++;		// First vertex of the next sheet
			*(Index++) = VertexIndex++;		// Second vertex of the next sheet
			TrianglesToRender += 4;
			CheckCount += 4;
		}

		for (INT LocalIdx = 0; LocalIdx < LocalTrianglesToRender; LocalIdx++)
		{
			*(Index++) = VertexIndex++;
			CheckCount++;
			TrianglesToRender++;
		}

		//@todo. Support sheets!

		CurrentTrail++;
	}

	Data.OutTriangleCount = TrianglesToRender;
	return TrianglesToRender;
}

INT FDynamicTrailsEmitterData::FillVertexData(struct FAsyncBufferFillData& Data)
{
	check(!TEXT("FillVertexData: Base implementation should NOT be called!"));
	return 0;
}

//
//	FDynamicRibbonEmitterData
//
/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicRibbonEmitterData::Init(UBOOL bInSelected)
{
	SourcePointer = &Source;
	FDynamicTrailsEmitterData::Init(bInSelected);
}

UBOOL FDynamicRibbonEmitterData::ShouldUsePrerenderView()
{
	return (RenderAxisOption != Trails_CameraUp);
}

void FDynamicRibbonEmitterData::RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses)
{
	if ((bRenderParticles == TRUE) || (bRenderTangents == TRUE))
	{
		// DEBUGGING
		// Draw all the points of the trail(s)
		FVector DrawPosition;
		FLOAT DrawSize;
		FColor DrawColor;
		FColor PrevDrawColor;
		FVector DrawTangentEnd;

		BYTE* Address = Source.ParticleData.GetData();
		FRibbonTypeDataPayload* StartTrailPayload;
		FRibbonTypeDataPayload* EndTrailPayload = NULL;
		FBaseParticle* DebugParticle;
		FRibbonTypeDataPayload* TrailPayload;
		FBaseParticle* PrevParticle = NULL;
		FRibbonTypeDataPayload* PrevTrailPayload;
		for (INT ParticleIdx = 0; ParticleIdx < Source.ActiveParticleCount; ParticleIdx++)
		{
			DECLARE_PARTICLE_PTR(Particle, Address + Source.ParticleStride * Source.ParticleIndices(ParticleIdx));
			StartTrailPayload = (FRibbonTypeDataPayload*)((BYTE*)Particle + Source.TrailDataOffset);
			if (TRAIL_EMITTER_IS_HEAD(StartTrailPayload->Flags) == 0)
			{
				continue;
			}

			// Pin the size to the X component
			FLOAT Increment = 1.0f / (StartTrailPayload->TriangleCount / 2);
			FLOAT ColorScale = 0.0f;

			DebugParticle = Particle;
			// Find the end particle in this chain...
			TrailPayload = StartTrailPayload;
			FBaseParticle* IteratorParticle = DebugParticle;
			while (TrailPayload)
			{
				INT	Next = TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags);
				if (Next == TRAIL_EMITTER_NULL_NEXT)
				{
					DebugParticle = IteratorParticle;
					EndTrailPayload = TrailPayload;
					TrailPayload = NULL;
				}
				else
				{
					DECLARE_PARTICLE_PTR(TempParticle, Address + Source.ParticleStride * Next);
					IteratorParticle = TempParticle;
					TrailPayload = (FRibbonTypeDataPayload*)((BYTE*)IteratorParticle + Source.TrailDataOffset);
				}
			}
			if (EndTrailPayload != StartTrailPayload)
			{
				FBaseParticle* CurrSpawnedParticle = NULL;
				FBaseParticle* NextSpawnedParticle = NULL;
				// We have more than one particle in the trail...
				TrailPayload = EndTrailPayload;

				if (TrailPayload->bInterpolatedSpawn == FALSE)
				{
					CurrSpawnedParticle = DebugParticle;
				}
				while (TrailPayload)
				{
					INT	Prev = TRAIL_EMITTER_GET_PREV(TrailPayload->Flags);
					if (Prev == TRAIL_EMITTER_NULL_PREV)
					{
						PrevParticle = NULL;
						PrevTrailPayload = NULL;
					}
					else
					{
						DECLARE_PARTICLE_PTR(TempParticle, Address + Source.ParticleStride * Prev);
						PrevParticle = TempParticle;
						PrevTrailPayload = (FRibbonTypeDataPayload*)((BYTE*)PrevParticle + Source.TrailDataOffset);
					}

					if (PrevTrailPayload && PrevTrailPayload->bInterpolatedSpawn == FALSE)
					{
						if (CurrSpawnedParticle == NULL)
						{
							CurrSpawnedParticle = PrevParticle;
						}
						else
						{
							NextSpawnedParticle = PrevParticle;
						}
					}

					DrawPosition = DebugParticle->Location;
					DrawSize = DebugParticle->Size.X * Source.Scale.X;
					INT Red   = appTrunc(255.0f * (1.0f - ColorScale));
					INT Green = appTrunc(255.0f * ColorScale);
					ColorScale += Increment;
					DrawColor = FColor(Red,Green,0);
					Red   = appTrunc(255.0f * (1.0f - ColorScale));
					Green = appTrunc(255.0f * ColorScale);
					PrevDrawColor = FColor(Red,Green,0);

					if (bRenderParticles == TRUE)
					{
						if (TrailPayload->bInterpolatedSpawn == FALSE)
						{
							DrawWireStar(PDI, DrawPosition, DrawSize, FColor(255,0,0), DPGIndex);
						}
						else
						{
							DrawWireStar(PDI, DrawPosition, DrawSize, FColor(0,255,0), DPGIndex);
						}

						//
						if (bRenderTessellation == TRUE)
						{
							if (PrevParticle != NULL)
							{
								// Draw a straight line between the particles
								// This will allow us to visualize the tessellation difference
								PDI->DrawLine(DrawPosition, PrevParticle->Location, FColor(0,0,255), DPGIndex);
								INT InterpCount = TrailPayload->RenderingInterpCount;
								// Interpolate between current and next...
								FVector LineStart = DrawPosition;
								FLOAT Diff = PrevTrailPayload->SpawnTime - TrailPayload->SpawnTime;
								FVector CurrUp = FVector(0.0f, 0.0f, 1.0f);
								FLOAT InvCount = 1.0f / InterpCount;
								FLinearColor StartColor = DrawColor;
								FLinearColor EndColor = PrevDrawColor;
								for (INT SpawnIdx = 0; SpawnIdx < InterpCount; SpawnIdx++)
								{
									FLOAT TimeStep = InvCount * SpawnIdx;
									FVector LineEnd = CubicInterp<FVector>(
										DebugParticle->Location, TrailPayload->Tangent,
										PrevParticle->Location, PrevTrailPayload->Tangent,
										TimeStep);
									FLinearColor InterpColor = Lerp<FLinearColor>(StartColor, EndColor, TimeStep);
									PDI->DrawLine(LineStart, LineEnd, InterpColor, DPGIndex);
									if (SpawnIdx > 0)
									{
										InterpColor.R = 1.0f - TimeStep;
										InterpColor.G = 1.0f - TimeStep;
										InterpColor.B = 1.0f - (1.0f - TimeStep);
									}
									DrawWireStar(PDI, LineEnd, DrawSize * 0.3f, InterpColor, DPGIndex);
									LineStart = LineEnd;
								}
								PDI->DrawLine(LineStart, PrevParticle->Location, EndColor, DPGIndex);
								//DrawWireStar(PDI, NextParticle->Location, DrawSize * 0.3f, EndColor, DPGIndex);
							}
						}
					}

					if (bRenderTangents == TRUE)
					{
						DrawTangentEnd = DrawPosition + TrailPayload->Tangent;
						if (TrailPayload == StartTrailPayload)
						{
							PDI->DrawLine(DrawPosition, DrawTangentEnd, FLinearColor(0.0f, 1.0f, 0.0f), DPGIndex);
						}
						else if (TrailPayload == EndTrailPayload)
						{
							PDI->DrawLine(DrawPosition, DrawTangentEnd, FLinearColor(1.0f, 0.0f, 0.0f), DPGIndex);
						}
						else
						{
							PDI->DrawLine(DrawPosition, DrawTangentEnd, FLinearColor(1.0f, 1.0f, 0.0f), DPGIndex);
						}
					}

					// The end will have Next set to the NULL flag...
					if (PrevParticle != NULL)
					{
						DebugParticle = PrevParticle;
						TrailPayload = PrevTrailPayload;
					}
					else
					{
						TrailPayload = NULL;
					}
				}
			}
		}
	}
}

// Data fill functions
INT FDynamicRibbonEmitterData::FillVertexData(struct FAsyncBufferFillData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailFillVertexTime);

	INT	TrianglesToRender = 0;

	BYTE* TempVertexData = (BYTE*)Data.VertexData;
	FParticleBeamTrailVertex* Vertex;
	FParticleBeamTrailVertexDynamicParameter* DynParamVertex;

	FMatrix CameraToWorld = Data.View->ViewMatrix.Inverse();
	FVector CameraUp = CameraToWorld.TransformNormal(FVector(0,0,1));
	FVector	ViewOrigin	= CameraToWorld.GetOrigin();

	INT MaxTessellationBetweenParticles = Max<INT>(Source.MaxTessellationBetweenParticles, 1);
	INT Sheets = Max<INT>(Source.Sheets, 1);
	Sheets = 1;

	// The distance tracking for tiling the 2nd UV set
	FLOAT CurrDistance = 0.0f;

	FBaseParticle* PackingParticle;
	BYTE* ParticleData = Source.ParticleData.GetData();

	// Mobile uses 1 UV, so select the tiled one
	const UBOOL bUseSingleUVTiling = GUsingMobileRHI || GEmulateMobileRendering;

	for (INT ParticleIdx = 0; ParticleIdx < Source.ActiveParticleCount; ParticleIdx++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + Source.ParticleStride * Source.ParticleIndices(ParticleIdx));
		FRibbonTypeDataPayload* TrailPayload = (FRibbonTypeDataPayload*)((BYTE*)Particle + Source.TrailDataOffset);
		if (TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == 0)
		{
			continue;
		}

		if (TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags) == TRAIL_EMITTER_NULL_NEXT)
		{
			continue;
		}

		PackingParticle = Particle;
		// Pin the size to the X component
		FLinearColor CurrLinearColor = PackingParticle->Color;
		// The increment for going [0..1] along the complete trail
		FLOAT TextureIncrement = 1.0f / (TrailPayload->TriangleCount / 2);
		FLOAT Tex_U = 0.0f;
		FVector CurrTilePosition = PackingParticle->Location;
		FVector PrevTilePosition = PackingParticle->Location;
		FVector PrevWorkingUp(0,0,1);
		INT VertexStride = sizeof(FParticleBeamTrailVertex);
		UBOOL bFillDynamic = FALSE;
		if (bUsesDynamicParameter == TRUE)
		{
			VertexStride = sizeof(FParticleBeamTrailVertexDynamicParameter);
			if (Source.DynamicParameterDataOffset > 0)
			{
				bFillDynamic = TRUE;
			}
		}
		FLOAT CurrTileU;
		FEmitterDynamicParameterPayload* CurrDynPayload = NULL;
		FEmitterDynamicParameterPayload* PrevDynPayload = NULL;
		FBaseParticle* PrevParticle = NULL;
		FRibbonTypeDataPayload* PrevTrailPayload = NULL;

		FVector WorkingUp = TrailPayload->Up;
		if (RenderAxisOption == Trails_CameraUp)
		{
			FVector DirToCamera = PackingParticle->Location - ViewOrigin;
			DirToCamera.Normalize();
			FVector NormailzedTangent = TrailPayload->Tangent;
			NormailzedTangent.Normalize();
			WorkingUp = NormailzedTangent ^ DirToCamera;
			if (WorkingUp.IsNearlyZero())
			{
				WorkingUp = CameraUp;
			}
		}

		while (TrailPayload)
		{
			FLOAT CurrSize = PackingParticle->Size.X * Source.Scale.X;

			INT InterpCount = TrailPayload->RenderingInterpCount;
			if (InterpCount > 1)
			{
				check(PrevParticle);
				check(TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == 0);

				// Interpolate between current and next...
				FVector CurrPosition = PackingParticle->Location;
				FVector CurrTangent = TrailPayload->Tangent;
				FVector CurrUp = WorkingUp;
				FLinearColor CurrColor = PackingParticle->Color;
				FLOAT CurrSize = PackingParticle->Size.X * Source.Scale.X;

				FVector PrevPosition = PrevParticle->Location;
				FVector PrevTangent = PrevTrailPayload->Tangent;
				FVector PrevUp = PrevWorkingUp;
				FLinearColor PrevColor = PrevParticle->Color;
				FLOAT PrevSize = PrevParticle->Size.X * Source.Scale.X;

				FLOAT InvCount = 1.0f / InterpCount;
				FLOAT Diff = PrevTrailPayload->SpawnTime - TrailPayload->SpawnTime;

				if (bFillDynamic == TRUE)
				{
					CurrDynPayload = ((FEmitterDynamicParameterPayload*)((BYTE*)(PackingParticle) + Source.DynamicParameterDataOffset));
					PrevDynPayload = ((FEmitterDynamicParameterPayload*)((BYTE*)(PrevParticle) + Source.DynamicParameterDataOffset));
				}

				FVector4 InterpDynamic(1.0f, 1.0f, 1.0f, 1.0f);
				for (INT SpawnIdx = InterpCount - 1; SpawnIdx >= 0; SpawnIdx--)
				{
					FLOAT TimeStep = InvCount * SpawnIdx;
					FVector InterpPos = CubicInterp<FVector>(CurrPosition, CurrTangent, PrevPosition, PrevTangent, TimeStep);
					FVector InterpUp = Lerp<FVector>(CurrUp, PrevUp, TimeStep);
					FLinearColor InterpColor = Lerp<FLinearColor>(CurrColor, PrevColor, TimeStep);
					FLOAT InterpSize = Lerp<FLOAT>(CurrSize, PrevSize, TimeStep);
					if (CurrDynPayload && PrevDynPayload)
					{
						InterpDynamic = Lerp<FVector4>(CurrDynPayload->DynamicParameterValue, PrevDynPayload->DynamicParameterValue, TimeStep);
					}

					if (bTextureTileDistance == TRUE)	
					{
						CurrTileU = Lerp<FLOAT>(TrailPayload->TiledU, PrevTrailPayload->TiledU, TimeStep);
					}
					else
					{
						CurrTileU = Tex_U;
					}

					Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
					Vertex->Position = InterpPos + InterpUp * InterpSize;
					Vertex->OldPosition = Vertex->Position;
					Vertex->Size.X = InterpSize;
					Vertex->Size.Y = InterpSize;
					Vertex->Size.Z = InterpSize;				
					Vertex->Tex_U = bUseSingleUVTiling ? CurrTileU : Tex_U;
					Vertex->Tex_V = 0.0f;
					Vertex->Tex_U2 = CurrTileU;
					Vertex->Tex_V2 = 0.0f;
					Vertex->Rotation = PackingParticle->Rotation;
					Vertex->Color = InterpColor;
					if (bUsesDynamicParameter == TRUE)
					{
						DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempVertexData);
						DynParamVertex->DynamicValue[0] = InterpDynamic.X;
						DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
						DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
						DynParamVertex->DynamicValue[3] = InterpDynamic.W;
					}
					TempVertexData += VertexStride;
					//PackedVertexCount++;

					Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
					Vertex->Position = InterpPos - InterpUp * InterpSize;
					Vertex->OldPosition = Vertex->Position;
					Vertex->Size.X = InterpSize;
					Vertex->Size.Y = InterpSize;
					Vertex->Size.Z = InterpSize;
					Vertex->Tex_U = bUseSingleUVTiling ? CurrTileU : Tex_U;
					Vertex->Tex_V = 1.0f;
					Vertex->Tex_U2 = CurrTileU;
					Vertex->Tex_V2 = 1.0f;
					Vertex->Rotation = PackingParticle->Rotation;
					Vertex->Color = InterpColor;
					if (bUsesDynamicParameter == TRUE)
					{
						DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempVertexData);
						DynParamVertex->DynamicValue[0] = InterpDynamic.X;
						DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
						DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
						DynParamVertex->DynamicValue[3] = InterpDynamic.W;
					}
					TempVertexData += VertexStride;
					//PackedVertexCount++;

					Tex_U += TextureIncrement;
				}
			}
			else
			{
				if (bFillDynamic == TRUE)
				{
					CurrDynPayload = ((FEmitterDynamicParameterPayload*)((BYTE*)(PackingParticle) + Source.DynamicParameterDataOffset));
				}

				if (bTextureTileDistance == TRUE)
				{
					CurrTileU = TrailPayload->TiledU;
				}
				else
				{
					CurrTileU = Tex_U;
				}

				Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
				Vertex->Position = PackingParticle->Location + WorkingUp * CurrSize;
				Vertex->OldPosition = PackingParticle->OldLocation;
				Vertex->Size.X = CurrSize;
				Vertex->Size.Y = CurrSize;
				Vertex->Size.Z = CurrSize;
				Vertex->Tex_U = bUseSingleUVTiling ? CurrTileU : Tex_U;
				Vertex->Tex_V = 0.0f;
				Vertex->Tex_U2 = CurrTileU;
				Vertex->Tex_V2 = 0.0f;
				Vertex->Rotation = PackingParticle->Rotation;
				Vertex->Color = PackingParticle->Color;
				if (bUsesDynamicParameter == TRUE)
				{
					DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempVertexData);
					if (CurrDynPayload != NULL)
					{
						DynParamVertex->DynamicValue[0] = CurrDynPayload->DynamicParameterValue.X;
						DynParamVertex->DynamicValue[1] = CurrDynPayload->DynamicParameterValue.Y;
						DynParamVertex->DynamicValue[2] = CurrDynPayload->DynamicParameterValue.Z;
						DynParamVertex->DynamicValue[3] = CurrDynPayload->DynamicParameterValue.W;
					}
					else
					{
						DynParamVertex->DynamicValue[0] = 1.0f;
						DynParamVertex->DynamicValue[1] = 1.0f;
						DynParamVertex->DynamicValue[2] = 1.0f;
						DynParamVertex->DynamicValue[3] = 1.0f;
					}
				}
				TempVertexData += VertexStride;
				//PackedVertexCount++;

				Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
				Vertex->Position = PackingParticle->Location - WorkingUp * CurrSize;
				Vertex->OldPosition = PackingParticle->OldLocation;
				Vertex->Size.X = CurrSize;
				Vertex->Size.Y = CurrSize;
				Vertex->Size.Z = CurrSize;
				Vertex->Tex_U = bUseSingleUVTiling ? CurrTileU : Tex_U;
				Vertex->Tex_V = 1.0f;
				Vertex->Tex_U2 = CurrTileU;
				Vertex->Tex_V2 = 1.0f;
				Vertex->Rotation = PackingParticle->Rotation;
				Vertex->Color = PackingParticle->Color;
				if (bUsesDynamicParameter == TRUE)
				{
					DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempVertexData);
					if (CurrDynPayload != NULL)
					{
						DynParamVertex->DynamicValue[0] = CurrDynPayload->DynamicParameterValue.X;
						DynParamVertex->DynamicValue[1] = CurrDynPayload->DynamicParameterValue.Y;
						DynParamVertex->DynamicValue[2] = CurrDynPayload->DynamicParameterValue.Z;
						DynParamVertex->DynamicValue[3] = CurrDynPayload->DynamicParameterValue.W;
					}
					else
					{
						DynParamVertex->DynamicValue[0] = 1.0f;
						DynParamVertex->DynamicValue[1] = 1.0f;
						DynParamVertex->DynamicValue[2] = 1.0f;
						DynParamVertex->DynamicValue[3] = 1.0f;
					}
				}
				TempVertexData += VertexStride;
				//PackedVertexCount++;

				Tex_U += TextureIncrement;
			}

			PrevParticle = PackingParticle;
			PrevTrailPayload = TrailPayload;
			PrevWorkingUp = WorkingUp;

			INT	NextIdx = TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags);
			if (NextIdx == TRAIL_EMITTER_NULL_NEXT)
			{
				TrailPayload = NULL;
				PackingParticle = NULL;
			}
			else
			{
				DECLARE_PARTICLE_PTR(TempParticle, ParticleData + Source.ParticleStride * NextIdx);
				PackingParticle = TempParticle;
				TrailPayload = (FRibbonTypeDataPayload*)((BYTE*)TempParticle + Source.TrailDataOffset);
				WorkingUp = TrailPayload->Up;
				if (RenderAxisOption == Trails_CameraUp)
				{
					FVector DirToCamera = PackingParticle->Location - ViewOrigin;
					DirToCamera.Normalize();
					FVector NormailzedTangent = TrailPayload->Tangent;
					NormailzedTangent.Normalize();
					WorkingUp = NormailzedTangent ^ DirToCamera;
					if (WorkingUp.IsNearlyZero())
					{
						WorkingUp = CameraUp;
					}
				}
			}
		}
	}

	return TrianglesToRender;
}

///////////////////////////////////////////////////////////////////////////////
/** Dynamic emitter data for AnimTrail emitters */
/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicAnimTrailEmitterData::Init(UBOOL bInSelected)
{
	SourcePointer = &Source;
	FDynamicTrailsEmitterData::Init(bInSelected);
}

void FDynamicAnimTrailEmitterData::RenderDebug(FPrimitiveDrawInterface* PDI, const FSceneView* View, UINT DPGIndex, UBOOL bCrosses)
{
	if ((bRenderParticles == TRUE) || (bRenderTangents == TRUE))
	{
		// DEBUGGING
		// Draw all the points of the trail(s)
		FVector DrawPosition;
		FVector DrawFirstEdgePosition;
		FVector DrawSecondEdgePosition;
		FLOAT DrawSize;
		FColor DrawColor;
		FColor PrevDrawColor;
		FVector DrawTangentEnd;

		BYTE* Address = Source.ParticleData.GetData();
		FAnimTrailTypeDataPayload* StartTrailPayload;
		FAnimTrailTypeDataPayload* EndTrailPayload = NULL;
		FBaseParticle* DebugParticle;
		FAnimTrailTypeDataPayload* TrailPayload;
		FBaseParticle* PrevParticle = NULL;
		FAnimTrailTypeDataPayload* PrevTrailPayload;
		for (INT ParticleIdx = 0; ParticleIdx < Source.ActiveParticleCount; ParticleIdx++)
		{
			DECLARE_PARTICLE_PTR(Particle, Address + Source.ParticleStride * Source.ParticleIndices(ParticleIdx));
			StartTrailPayload = (FAnimTrailTypeDataPayload*)((BYTE*)Particle + Source.TrailDataOffset);
			if (TRAIL_EMITTER_IS_HEAD(StartTrailPayload->Flags) == 0)
			{
				continue;
			}

			// Pin the size to the X component
			FLOAT Increment = 1.0f / (StartTrailPayload->TriangleCount / 2);
			FLOAT ColorScale = 0.0f;

			DebugParticle = Particle;
			// Find the end particle in this chain...
			TrailPayload = StartTrailPayload;
			FBaseParticle* IteratorParticle = DebugParticle;
			while (TrailPayload)
			{
				INT	Next = TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags);
				if (Next == TRAIL_EMITTER_NULL_NEXT)
				{
					DebugParticle = IteratorParticle;
					EndTrailPayload = TrailPayload;
					TrailPayload = NULL;
				}
				else
				{
					DECLARE_PARTICLE_PTR(TempParticle, Address + Source.ParticleStride * Next);
					IteratorParticle = TempParticle;
					TrailPayload = (FAnimTrailTypeDataPayload*)((BYTE*)IteratorParticle + Source.TrailDataOffset);
				}
			}
			if (EndTrailPayload != StartTrailPayload)
			{
				FBaseParticle* CurrSpawnedParticle = NULL;
				FBaseParticle* NextSpawnedParticle = NULL;
				// We have more than one particle in the trail...
				TrailPayload = EndTrailPayload;

				if (TrailPayload->bInterpolatedSpawn == FALSE)
				{
					CurrSpawnedParticle = DebugParticle;
				}
				while (TrailPayload)
				{
					INT	Prev = TRAIL_EMITTER_GET_PREV(TrailPayload->Flags);
					if (Prev == TRAIL_EMITTER_NULL_PREV)
					{
						PrevParticle = NULL;
						PrevTrailPayload = NULL;
					}
					else
					{
						DECLARE_PARTICLE_PTR(TempParticle, Address + Source.ParticleStride * Prev);
						PrevParticle = TempParticle;
						PrevTrailPayload = (FAnimTrailTypeDataPayload*)((BYTE*)PrevParticle + Source.TrailDataOffset);
					}

					if (PrevTrailPayload && PrevTrailPayload->bInterpolatedSpawn == FALSE)
					{
						if (CurrSpawnedParticle == NULL)
						{
							CurrSpawnedParticle = PrevParticle;
						}
						else
						{
							NextSpawnedParticle = PrevParticle;
						}
					}

					DrawPosition = DebugParticle->Location;
					DrawFirstEdgePosition = TrailPayload->FirstEdge;
					DrawSecondEdgePosition = TrailPayload->SecondEdge;
					DrawSize = DebugParticle->Size.X * Source.Scale.X;
					INT Red   = appTrunc(255.0f * (1.0f - ColorScale));
					INT Green = appTrunc(255.0f * ColorScale);
					ColorScale += Increment;
					DrawColor = FColor(Red,Green,0);
					Red   = appTrunc(255.0f * (1.0f - ColorScale));
					Green = appTrunc(255.0f * ColorScale);
					PrevDrawColor = FColor(Red,Green,0);

					if (bRenderParticles == TRUE)
					{
						if (TrailPayload->bInterpolatedSpawn == FALSE)
						{
							DrawWireStar(PDI, DrawPosition, DrawSize, FColor(255,0,0), DPGIndex);
							DrawWireStar(PDI, DrawFirstEdgePosition, DrawSize, FColor(255,0,0), DPGIndex);
							DrawWireStar(PDI, DrawSecondEdgePosition, DrawSize, FColor(255,0,0), DPGIndex);
						}
						else
						{
							DrawWireStar(PDI, DrawPosition, DrawSize, FColor(0,255,0), DPGIndex);
							DrawWireStar(PDI, DrawFirstEdgePosition, DrawSize, FColor(0,255,0), DPGIndex);
							DrawWireStar(PDI, DrawSecondEdgePosition, DrawSize, FColor(0,255,0), DPGIndex);
						}

						//
						if (bRenderTessellation == TRUE)
						{
							if (PrevParticle != NULL)
							{
								// Draw a straight line between the particles
								// This will allow us to visualize the tessellation difference
								PDI->DrawLine(DrawPosition, PrevParticle->Location, FColor(0,0,255), DPGIndex);
								PDI->DrawLine(DrawFirstEdgePosition, PrevTrailPayload->FirstEdge, FColor(0,0,255), DPGIndex);
								PDI->DrawLine(DrawSecondEdgePosition, PrevTrailPayload->SecondEdge, FColor(0,0,255), DPGIndex);

								INT InterpCount = TrailPayload->RenderingInterpCount;
								// Interpolate between current and next...
								FVector LineStart = DrawPosition;
								FVector FirstStart = DrawFirstEdgePosition;
								FVector SecondStart = DrawSecondEdgePosition;
								FLOAT Diff = AnimSampleTimeStep;
								FVector CurrUp = FVector(0.0f, 0.0f, 1.0f);
								FLOAT InvCount = 1.0f / InterpCount;
								FLinearColor StartColor = DrawColor;
								FLinearColor EndColor = PrevDrawColor;
								for (INT SpawnIdx = 0; SpawnIdx < InterpCount; SpawnIdx++)
								{
									FLOAT TimeStep = InvCount * SpawnIdx;
									FVector LineEnd = CubicInterp<FVector>(
										DebugParticle->Location, TrailPayload->ControlVelocity * AnimSampleTimeStep,
										PrevParticle->Location, PrevTrailPayload->ControlVelocity * AnimSampleTimeStep,
										TimeStep);
									FVector FirstEnd = CubicInterp<FVector>(
										TrailPayload->FirstEdge, TrailPayload->FirstVelocity * AnimSampleTimeStep,
										PrevTrailPayload->FirstEdge, PrevTrailPayload->FirstVelocity * AnimSampleTimeStep,
										TimeStep);
									FVector SecondEnd = CubicInterp<FVector>(
										TrailPayload->SecondEdge, TrailPayload->SecondVelocity * AnimSampleTimeStep,
										PrevTrailPayload->SecondEdge, PrevTrailPayload->SecondVelocity * AnimSampleTimeStep,
										TimeStep);
									FLinearColor InterpColor = Lerp<FLinearColor>(StartColor, EndColor, TimeStep);
									PDI->DrawLine(LineStart, LineEnd, InterpColor, DPGIndex);
									PDI->DrawLine(FirstStart, FirstEnd, InterpColor, DPGIndex);
									PDI->DrawLine(SecondStart, SecondEnd, InterpColor, DPGIndex);
									if (SpawnIdx > 0)
									{
										InterpColor.R = 1.0f - TimeStep;
										InterpColor.G = 1.0f - TimeStep;
										InterpColor.B = 1.0f - (1.0f - TimeStep);
									}
									DrawWireStar(PDI, LineEnd, DrawSize * 0.3f, InterpColor, DPGIndex);
									DrawWireStar(PDI, FirstEnd, DrawSize * 0.3f, InterpColor, DPGIndex);
									DrawWireStar(PDI, SecondEnd, DrawSize * 0.3f, InterpColor, DPGIndex);
									LineStart = LineEnd;
									FirstStart = FirstEnd;
									SecondStart = SecondEnd;
								}
								PDI->DrawLine(LineStart, PrevParticle->Location, EndColor, DPGIndex);
								PDI->DrawLine(FirstStart, PrevTrailPayload->FirstEdge, EndColor, DPGIndex);
								PDI->DrawLine(SecondStart, PrevTrailPayload->SecondEdge, EndColor, DPGIndex);
								//DrawWireStar(PDI, NextParticle->Location, DrawSize * 0.3f, EndColor, DPGIndex);
							}
						}
					}

					if (bRenderTangents == TRUE)
					{
						DrawTangentEnd = DrawPosition + TrailPayload->ControlVelocity * AnimSampleTimeStep;
						PDI->DrawLine(DrawPosition, DrawTangentEnd, FLinearColor(1.0f, 1.0f, 0.0f), DPGIndex);
						DrawTangentEnd = DrawFirstEdgePosition + TrailPayload->FirstVelocity * AnimSampleTimeStep;
						PDI->DrawLine(DrawFirstEdgePosition, DrawTangentEnd, FLinearColor(1.0f, 1.0f, 0.0f), DPGIndex);
						DrawTangentEnd = DrawSecondEdgePosition + TrailPayload->SecondVelocity * AnimSampleTimeStep;
						PDI->DrawLine(DrawSecondEdgePosition, DrawTangentEnd, FLinearColor(1.0f, 1.0f, 0.0f), DPGIndex);
					}

					// The end will have Next set to the NULL flag...
					if (PrevParticle != NULL)
					{
						DebugParticle = PrevParticle;
						TrailPayload = PrevTrailPayload;
					}
					else
					{
						TrailPayload = NULL;
					}
				}
			}
		}
	}
}

// Data fill functions
INT FDynamicAnimTrailEmitterData::FillVertexData(struct FAsyncBufferFillData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_TrailFillVertexTime);
	check(SceneProxy);

	INT	TrianglesToRender = 0;

	BYTE* TempVertexData = (BYTE*)Data.VertexData;
	FParticleBeamTrailVertex* Vertex;
	FParticleBeamTrailVertexDynamicParameter* DynParamVertex;

	INT Sheets = Max<INT>(Source.Sheets, 1);
	Sheets = 1;

	// The increment for going [0..1] along the complete trail
	FLOAT TextureIncrement = 1.0f / (Data.VertexCount / 2);
	// The distance tracking for tiling the 2nd UV set
	FLOAT CurrDistance = 0.0f;

	FBaseParticle* PackingParticle;
	BYTE* ParticleData = Source.ParticleData.GetData();
	for (INT ParticleIdx = 0; ParticleIdx < Source.ActiveParticleCount; ParticleIdx++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + Source.ParticleStride * Source.ParticleIndices(ParticleIdx));
		FAnimTrailTypeDataPayload* TrailPayload = (FAnimTrailTypeDataPayload*)((BYTE*)Particle + Source.TrailDataOffset);
		if (TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == 0)
		{
			continue;
		}

		if (TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags) == TRAIL_EMITTER_NULL_NEXT)
		{
			continue;
		}

		PackingParticle = Particle;
		// Pin the size to the X component
		FLinearColor CurrLinearColor = PackingParticle->Color;
		FLOAT Tex_U = 0.0f;
		FVector CurrTilePosition = PackingParticle->Location;
		FVector PrevTilePosition = PackingParticle->Location;
		INT VertexStride = sizeof(FParticleBeamTrailVertex);
		UBOOL bFillDynamic = FALSE;
		if (bUsesDynamicParameter == TRUE)
		{
			VertexStride = sizeof(FParticleBeamTrailVertexDynamicParameter);
			if (Source.DynamicParameterDataOffset > 0)
			{
				bFillDynamic = TRUE;
			}
		}
		FLOAT CurrTileU;
		FEmitterDynamicParameterPayload* CurrDynPayload = NULL;
		FEmitterDynamicParameterPayload* PrevDynPayload = NULL;
		FBaseParticle* PrevParticle = NULL;
		FAnimTrailTypeDataPayload* PrevTrailPayload = NULL;

		while (TrailPayload)
		{
			FLOAT CurrSize = PackingParticle->Size.X * Source.Scale.X;

			INT InterpCount = TrailPayload->RenderingInterpCount;
			if (InterpCount > 1)
			{
				check(PrevParticle);
				check(TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == 0);

				// Interpolate between current and next...
				FVector CurrPosition = PackingParticle->Location;
				FVector CurrTangent = TrailPayload->ControlVelocity;
				FVector CurrFirstEdge = TrailPayload->FirstEdge;
				FVector CurrFirstTangent = TrailPayload->FirstVelocity;
				FVector CurrSecondEdge = TrailPayload->SecondEdge;
				FVector CurrSecondTangent = TrailPayload->SecondVelocity;
				FLinearColor CurrColor = PackingParticle->Color;
				FLOAT CurrSize = PackingParticle->Size.X * Source.Scale.X;

				FVector PrevPosition = PrevParticle->Location;
				FVector PrevTangent = PrevTrailPayload->ControlVelocity;
				FVector PrevFirstEdge = PrevTrailPayload->FirstEdge;
				FVector PrevFirstTangent = PrevTrailPayload->FirstVelocity;
				FVector PrevSecondEdge = PrevTrailPayload->SecondEdge;
				FVector PrevSecondTangent = PrevTrailPayload->SecondVelocity;
				FLinearColor PrevColor = PrevParticle->Color;
				FLOAT PrevSize = PrevParticle->Size.X * Source.Scale.X;

				FLOAT InvCount = 1.0f / InterpCount;
				FLOAT Diff = PrevTrailPayload->SpawnTime - TrailPayload->SpawnTime;

				if (bFillDynamic == TRUE)
				{
					CurrDynPayload = ((FEmitterDynamicParameterPayload*)((BYTE*)(PackingParticle) + Source.DynamicParameterDataOffset));
					PrevDynPayload = ((FEmitterDynamicParameterPayload*)((BYTE*)(PrevParticle) + Source.DynamicParameterDataOffset));
				}

				FVector4 InterpDynamic(1.0f, 1.0f, 1.0f, 1.0f);
				for (INT SpawnIdx = InterpCount - 1; SpawnIdx >= 0; SpawnIdx--)
				{
					FLOAT TimeStep = InvCount * SpawnIdx;
					FVector InterpPos = CubicInterp<FVector>(
						CurrPosition, CurrTangent * AnimSampleTimeStep, 
						PrevPosition, PrevTangent * AnimSampleTimeStep, 
						TimeStep);
					FVector InterpFirst = CubicInterp<FVector>(
						CurrFirstEdge, CurrFirstTangent * AnimSampleTimeStep, 
						PrevFirstEdge, PrevFirstTangent * AnimSampleTimeStep, 
						TimeStep);
					FVector InterpSecond = CubicInterp<FVector>(
						CurrSecondEdge, CurrSecondTangent * AnimSampleTimeStep, 
						PrevSecondEdge, PrevSecondTangent * AnimSampleTimeStep, 
						TimeStep);
					FLinearColor InterpColor = Lerp<FLinearColor>(CurrColor, PrevColor, TimeStep);
					FLOAT InterpSize = Lerp<FLOAT>(CurrSize, PrevSize, TimeStep);
					if (CurrDynPayload && PrevDynPayload)
					{
						InterpDynamic = Lerp<FVector4>(CurrDynPayload->DynamicParameterValue, PrevDynPayload->DynamicParameterValue, TimeStep);
					}

					if (bTextureTileDistance == TRUE)	
					{
						CurrTileU = Lerp<FLOAT>(TrailPayload->TiledU, PrevTrailPayload->TiledU, TimeStep);
					}
					else
					{
						CurrTileU = Tex_U;
					}

					Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
					Vertex->Position = InterpFirst;//InterpPos + InterpFirst * InterpSize;
					Vertex->OldPosition = InterpFirst;
					Vertex->Size.X = InterpSize;
					Vertex->Size.Y = InterpSize;
					Vertex->Size.Z = InterpSize;
					Vertex->Tex_U = Tex_U;
					Vertex->Tex_V = 0.0f;
					Vertex->Tex_U2 = CurrTileU;
					Vertex->Tex_V2 = 0.0f;
					Vertex->Rotation = PackingParticle->Rotation;
					Vertex->Color = InterpColor;
					if (bUsesDynamicParameter == TRUE)
					{
						DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempVertexData);
						DynParamVertex->DynamicValue[0] = InterpDynamic.X;
						DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
						DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
						DynParamVertex->DynamicValue[3] = InterpDynamic.W;
					}
					TempVertexData += VertexStride;
					//PackedVertexCount++;

					Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
					Vertex->Position = InterpSecond;//InterpPos - InterpSecond * InterpSize;
					Vertex->OldPosition = InterpSecond;
					Vertex->Size.X = InterpSize;
					Vertex->Size.Y = InterpSize;
					Vertex->Size.Z = InterpSize;
					Vertex->Tex_U = Tex_U;
					Vertex->Tex_V = 1.0f;
					Vertex->Tex_U2 = CurrTileU;
					Vertex->Tex_V2 = 1.0f;
					Vertex->Rotation = PackingParticle->Rotation;
					Vertex->Color = InterpColor;
					if (bUsesDynamicParameter == TRUE)
					{
						DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempVertexData);
						DynParamVertex->DynamicValue[0] = InterpDynamic.X;
						DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
						DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
						DynParamVertex->DynamicValue[3] = InterpDynamic.W;
					}
					TempVertexData += VertexStride;
					//PackedVertexCount++;

					Tex_U += TextureIncrement;
				}
			}
			else
			{
				if (bFillDynamic == TRUE)
				{
					CurrDynPayload = ((FEmitterDynamicParameterPayload*)((BYTE*)(PackingParticle) + Source.DynamicParameterDataOffset));
				}

				if (bTextureTileDistance == TRUE)
				{
					CurrTileU = TrailPayload->TiledU;
				}
				else
				{
					CurrTileU = Tex_U;
				}

				Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
				Vertex->Position = TrailPayload->FirstEdge;//PackingParticle->Location + TrailPayload->FirstEdge * CurrSize;
				Vertex->OldPosition = PackingParticle->OldLocation;
				Vertex->Size.X = CurrSize;
				Vertex->Size.Y = CurrSize;
				Vertex->Size.Z = CurrSize;
				Vertex->Tex_U = Tex_U;
				Vertex->Tex_V = 0.0f;
				Vertex->Tex_U2 = CurrTileU;
				Vertex->Tex_V2 = 0.0f;
				Vertex->Rotation = PackingParticle->Rotation;
				Vertex->Color = PackingParticle->Color;
				if (bUsesDynamicParameter == TRUE)
				{
					DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempVertexData);
					if (CurrDynPayload != NULL)
					{
						DynParamVertex->DynamicValue[0] = CurrDynPayload->DynamicParameterValue.X;
						DynParamVertex->DynamicValue[1] = CurrDynPayload->DynamicParameterValue.Y;
						DynParamVertex->DynamicValue[2] = CurrDynPayload->DynamicParameterValue.Z;
						DynParamVertex->DynamicValue[3] = CurrDynPayload->DynamicParameterValue.W;
					}
					else
					{
						DynParamVertex->DynamicValue[0] = 1.0f;
						DynParamVertex->DynamicValue[1] = 1.0f;
						DynParamVertex->DynamicValue[2] = 1.0f;
						DynParamVertex->DynamicValue[3] = 1.0f;
					}
				}
				TempVertexData += VertexStride;
				//PackedVertexCount++;

				Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
				Vertex->Position = TrailPayload->SecondEdge;//PackingParticle->Location - TrailPayload->SecondEdge * CurrSize;
				Vertex->OldPosition = PackingParticle->OldLocation;
				Vertex->Size.X = CurrSize;
				Vertex->Size.Y = CurrSize;
				Vertex->Size.Z = CurrSize;
				Vertex->Tex_U = Tex_U;
				Vertex->Tex_V = 1.0f;
				Vertex->Tex_U2 = CurrTileU;
				Vertex->Tex_V2 = 1.0f;
				Vertex->Rotation = PackingParticle->Rotation;
				Vertex->Color = PackingParticle->Color;
				if (bUsesDynamicParameter == TRUE)
				{
					DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempVertexData);
					if (CurrDynPayload != NULL)
					{
						DynParamVertex->DynamicValue[0] = CurrDynPayload->DynamicParameterValue.X;
						DynParamVertex->DynamicValue[1] = CurrDynPayload->DynamicParameterValue.Y;
						DynParamVertex->DynamicValue[2] = CurrDynPayload->DynamicParameterValue.Z;
						DynParamVertex->DynamicValue[3] = CurrDynPayload->DynamicParameterValue.W;
					}
					else
					{
						DynParamVertex->DynamicValue[0] = 1.0f;
						DynParamVertex->DynamicValue[1] = 1.0f;
						DynParamVertex->DynamicValue[2] = 1.0f;
						DynParamVertex->DynamicValue[3] = 1.0f;
					}
				}
				TempVertexData += VertexStride;
				//PackedVertexCount++;

				Tex_U += TextureIncrement;
			}

			PrevParticle = PackingParticle;
			PrevTrailPayload = TrailPayload;

			INT	NextIdx = TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags);
			if (NextIdx == TRAIL_EMITTER_NULL_NEXT)
			{
				TrailPayload = NULL;
				PackingParticle = NULL;
			}
			else
			{
				DECLARE_PARTICLE_PTR(TempParticle, ParticleData + Source.ParticleStride * NextIdx);
				PackingParticle = TempParticle;
				TrailPayload = (FAnimTrailTypeDataPayload*)((BYTE*)TempParticle + Source.TrailDataOffset);
			}
		}
	}

	return TrianglesToRender;
}

///////////////////////////////////////////////////////////////////////////////
//	ParticleSystemSceneProxy
///////////////////////////////////////////////////////////////////////////////
/** Initialization constructor. */
FParticleSystemSceneProxy::FParticleSystemSceneProxy(const UParticleSystemComponent* Component):
FPrimitiveSceneProxy(Component, Component->Template ? Component->Template->GetFName() : NAME_None)
	, Owner(Component->GetOwner())
	, CullDistance(Component->CachedMaxDrawDistance > 0 ? Component->CachedMaxDrawDistance : WORLD_MAX)
	, bCastShadow(Component->CastShadow)
	, MaterialViewRelevance(
		((Component->GetCurrentLODIndex() >= 0) && (Component->GetCurrentLODIndex() < Component->CachedViewRelevanceFlags.Num())) ?
			Component->CachedViewRelevanceFlags(Component->GetCurrentLODIndex()) :
		((Component->GetCurrentLODIndex() == -1) && (Component->CachedViewRelevanceFlags.Num() >= 1)) ?
			Component->CachedViewRelevanceFlags(0) :
			FMaterialViewRelevance()
		)
	, DynamicData(NULL)
	, SelectedWireframeMaterialInstance(
		GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(FALSE) : NULL,
		GetSelectionColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),TRUE,FALSE)
		)
	, DeselectedWireframeMaterialInstance(
		GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(FALSE) : NULL,
		GetSelectionColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),FALSE,FALSE)
		)
	, PendingLODDistance(0.0f)
	, LODOrigin(0.0f, 0.0f, 0.0f)
	, LODHasNearClippingPlane(FALSE)
	, LastFramePreRendered(-1)
{
#if STATS
	LastStatCaptureTime = GCurrentTime;
	bCountedThisFrame = FALSE;
#endif
	LODMethod = Component->LODMethod;
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	delete DynamicData;
	DynamicData = NULL;
}

static UBOOL ShouldShowParticles(const FSceneView* View)
{
	check(View);
	check(View->Family);

	UBOOL bRet = (View->Family->ShowFlags & SHOW_Particles) != 0;

	if(!TEST_PROFILEEXSTATE(0x800, View->Family->CurrentRealTime))
	{
		bRet = FALSE;
	}

	return bRet;
}

// FPrimitiveSceneProxy interface.

/** 
* Draw the scene proxy as a dynamic element
*
* @param	PDI - draw interface to render to
* @param	View - current view
* @param	DPGIndex - current depth priority 
* @param	Flags - optional set of flags from EDrawDynamicElementFlags
*/
void FParticleSystemSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	if (ShouldShowParticles(View))
	{
#if LOG_DETAILED_PARTICLE_RENDER_STATS
		static QWORD LastFrameCounter = 0;
		if( LastFrameCounter != GFrameCounter )
		{
			GDetailedParticleRenderStats.DumpStats();
			GDetailedParticleRenderStats.Reset();
			LastFrameCounter = GFrameCounter;
		}

		UParticleSystemComponent* ParticleSystemComponent = CastChecked<UParticleSystemComponent>(PrimitiveSceneInfo->Component);
		if (ParticleSystemComponent->Template == NULL)
		{
			return;
		}
#endif
		SCOPE_CYCLE_COUNTER(STAT_ParticleRenderingTime);
		TRACK_DETAILED_PARTICLE_RENDER_STATS(ParticleSystemComponent->Template);

		const DOUBLE StartTime = GTrackParticleRenderingStats || GTrackParticleRenderingStatsForOneFrame ? appSeconds() : 0;

		INT NumDraws = 0;
		INT NumEmitters = 0;

		// Determine the DPG the primitive should be drawn in for this view.
		if (GetDepthPriorityGroup(View) == DPGIndex)
		{
			if (DynamicData != NULL)
			{
				for (INT Index = 0; Index < DynamicData->DynamicEmitterDataArray.Num(); Index++)
				{
					FDynamicEmitterDataBase* Data =	DynamicData->DynamicEmitterDataArray(Index);
					if ((Data == NULL) || (Data->bValid != TRUE))
					{
						continue;
					}
					//hold on to the emitter index in case we need to access any of its properties
					DynamicData->EmitterIndex = Index;

					Data->SceneProxy = this;
					const INT DrawCalls = Data->Render(this, PDI, View, DPGIndex);
					NumDraws += DrawCalls;
					if (DrawCalls > 0)
					{
						NumEmitters++;
					}
				}
			}
		}

		INC_DWORD_STAT_BY(STAT_ParticleDrawCalls, NumDraws);

		if (ShouldShowParticles(View))
		{
			RenderBounds(PDI, DPGIndex, View->Family->ShowFlags, PrimitiveSceneInfo->Bounds, !Owner || Owner->IsSelected());
			if (PrimitiveSceneInfo->bHasCustomOcclusionBounds == TRUE)
			{
				RenderBounds(PDI, DPGIndex, View->Family->ShowFlags, GetCustomOcclusionBounds(), !Owner || Owner->IsSelected());
			}
		}

#if STATS

		// Capture one frame rendering stats if enabled and at least one draw call was submitted
		if (GTrackParticleRenderingStatsForOneFrame
			&& NumDraws > 0)
		{
			const DOUBLE EndTime = appSeconds();
			UParticleSystemComponent* ParticleSystemComponent = CastChecked<UParticleSystemComponent>(PrimitiveSceneInfo->Component);

			const FLOAT DeltaTime = EndTime - StartTime;
			const FString TemplateName = ParticleSystemComponent->Template->GetPathName();
			FParticleTemplateRenderStats* TemplateStats = GOneFrameTemplateRenderStats.Find(TemplateName);

			if (TemplateStats)
			{
				// Update the existing record for this UParticleSystem
				TemplateStats->NumDraws += NumDraws;
				TemplateStats->MaxRenderTime += DeltaTime;
				TemplateStats->NumEmitters += NumEmitters;
				TemplateStats->NumDrawDynamicElements++;
				if (!bCountedThisFrame)
				{
					TemplateStats->NumComponents++;
				}
			}
			else
			{
				// Create a new record for this UParticleSystem
				FParticleTemplateRenderStats NewStats;
				NewStats.NumDraws = NumDraws;
				NewStats.MaxRenderTime = DeltaTime;
				NewStats.NumEmitters = NumEmitters;
				NewStats.NumComponents = 1;
				NewStats.NumDrawDynamicElements = 1;
				GOneFrameTemplateRenderStats.Set(TemplateName, NewStats);
			}
			bCountedThisFrame = TRUE;
		}

		if (!GTrackParticleRenderingStatsForOneFrame)
		{
			// Mark the proxy has not having been processed, this will be used on the next frame dump to count components rendered accurately
			bCountedThisFrame = FALSE;
		}

		// Capture render stats if enabled, and at least one draw call was submitted, and enough time has elapsed since the last capture
		// This misses spikes but allows efficient stat gathering over large periods of time
		if (GTrackParticleRenderingStats 
			// The main purpose of particle rendering stats are to optimize splitscreen, so only capture when we are in medium detail mode
			// This is needed to prevent capturing particle rendering time during cinematics where the engine switches back to high detail mode
			&& GSystemSettings.DetailMode != DM_High
			&& NumDraws > 0
			&& GCurrentTime - LastStatCaptureTime > GTimeBetweenParticleRenderStatCaptures)
		{
			LastStatCaptureTime = GCurrentTime;

			const DOUBLE EndTime = appSeconds();
			UParticleSystemComponent* ParticleSystemComponent = CastChecked<UParticleSystemComponent>(PrimitiveSceneInfo->Component);

			const FLOAT DeltaTime = EndTime - StartTime;
			// Only capture stats if the rendering time was large enough to be considered
			if (ParticleSystemComponent->Template && DeltaTime > GMinParticleDrawTimeToTrack)
			{
				const FString TemplateName = ParticleSystemComponent->Template->GetPathName();
				FParticleTemplateRenderStats* TemplateStats = GTemplateRenderStats.Find(TemplateName);
				if (TemplateStats)
				{
					// Update the existing record for this UParticleSystem if the new stat had a longer render time
					if (DeltaTime > TemplateStats->MaxRenderTime)
					{
						TemplateStats->NumDraws = NumDraws;
						TemplateStats->MaxRenderTime = DeltaTime;
						TemplateStats->NumEmitters = NumEmitters;
					}
				}
				else
				{
					// Create a new record for this UParticleSystem
					FParticleTemplateRenderStats NewStats;
					NewStats.NumDraws = NumDraws;
					NewStats.MaxRenderTime = DeltaTime;
					NewStats.NumEmitters = NumEmitters;
					GTemplateRenderStats.Set(TemplateName, NewStats);
				}

				const FString ComponentName = ParticleSystemComponent->GetPathName();
				if (ComponentName.InStr(TEXT("Emitter_")) != INDEX_NONE)
				{
					FParticleComponentRenderStats* ComponentStats = GComponentRenderStats.Find(ComponentName);
					if (ComponentStats)
					{
						// Update the existing record for this component if the new stat had a longer render time
						if (DeltaTime > ComponentStats->MaxRenderTime)
						{
							ComponentStats->NumDraws = NumDraws;
							ComponentStats->MaxRenderTime = DeltaTime;
						}
					}
					else
					{
						// Create a new record for this component
						FParticleComponentRenderStats NewStats;
						NewStats.NumDraws = NumDraws;
						NewStats.MaxRenderTime = DeltaTime;
						GComponentRenderStats.Set(ComponentName, NewStats);
					}
				}
			}
		}
#endif
	}
}

/**
 *	Called when the rendering thread adds the proxy to the scene.
 *	This function allows for generating renderer-side resources.
 */
UBOOL FParticleSystemSceneProxy::CreateRenderThreadResources()
{
	// 
	if (DynamicData == NULL)
	{
		return FALSE;
	}

	for (INT Index = 0; Index < DynamicData->DynamicEmitterDataArray.Num(); Index++)
	{
		FDynamicEmitterDataBase* Data =	DynamicData->DynamicEmitterDataArray(Index);
		if (Data != NULL)
		{
#if WITH_APEX_PARTICLES
			switch (Data->GetSource().eEmitterType)
			{
			case DET_Apex:
				{
					FDynamicApexEmitterData* SpriteData = (FDynamicApexEmitterData*)Data;
				}
				break;
			default:
				Data->CreateRenderThreadResources(this);
				break;
			}
#else
			Data->CreateRenderThreadResources(this);
#endif
		}
	}

	return TRUE;
}

/**
 *	Called when the rendering thread removes the dynamic data from the scene.
 */
UBOOL FParticleSystemSceneProxy::ReleaseRenderThreadResources()
{
	// 
	if (DynamicData == NULL)
	{
		return FALSE;
	}

	for (INT Index = 0; Index < DynamicData->DynamicEmitterDataArray.Num(); Index++)
	{
		FDynamicEmitterDataBase* Data =	DynamicData->DynamicEmitterDataArray(Index);
		if (Data != NULL)
		{
#if WITH_APEX_PARTICLES
			switch (Data->GetSource().eEmitterType)
			{
			case DET_Apex:
				{
					FDynamicApexEmitterData* SpriteData = (FDynamicApexEmitterData*)Data;
				}
				break;
			default:
				Data->ReleaseRenderThreadResources(this);
				break;
			}
#else
			Data->ReleaseRenderThreadResources(this);
#endif
		}
	}

	return TRUE;
}

void FParticleSystemSceneProxy::UpdateData(FParticleDynamicData* NewDynamicData)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		ParticleUpdateDataCommand,
		FParticleSystemSceneProxy*, Proxy, this,
		FParticleDynamicData*, NewDynamicData, NewDynamicData,
		{
			Proxy->UpdateData_RenderThread(NewDynamicData);
		}
		);
}

void FParticleSystemSceneProxy::UpdateData_RenderThread(FParticleDynamicData* NewDynamicData)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateRTTime);	
	ReleaseRenderThreadResources();
	if (DynamicData != NewDynamicData)
	{
		delete DynamicData;		
	}
	DynamicData = NewDynamicData;
	CreateRenderThreadResources();
}

void FParticleSystemSceneProxy::UpdateViewRelevance(FMaterialViewRelevance& NewViewRelevance)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		ParticleUpdateViewRelevanceCommand,
		FParticleSystemSceneProxy*, Proxy, this,
		FMaterialViewRelevance, ViewRel, NewViewRelevance,
		{
			Proxy->UpdateViewRelevance_RenderThread(ViewRel);
		}
		);		
}

void FParticleSystemSceneProxy::UpdateViewRelevance_RenderThread(FMaterialViewRelevance& NewViewRelevance)
{
	MaterialViewRelevance = NewViewRelevance;
}

void FParticleSystemSceneProxy::DetermineLODDistance(const FSceneView* View, INT FrameNumber)
{
	INT	LODIndex = -1;

	if (LODMethod == PARTICLESYSTEMLODMETHOD_Automatic)
	{
		// Default to the highest LOD level
		FVector	CameraPosition	= View->ViewOrigin;
		FVector	CompPosition	= LocalToWorld.GetOrigin();
		FVector	DistDiff		= CompPosition - CameraPosition;
		FLOAT	Distance		= DistDiff.Size() * View->LODDistanceFactor;

		if (FrameNumber != LastFramePreRendered)
		{
			// First time in the frame - then just set it...
			PendingLODDistance = Distance;
			LODOrigin = CameraPosition;
			LODHasNearClippingPlane = View->bHasNearClippingPlane;
			LODNearClippingPlane = View->NearClippingPlane;
			LastFramePreRendered = FrameNumber;
		}
		else
		if (Distance < PendingLODDistance)
		{
			// Not first time in the frame, then we compare and set if closer
			PendingLODDistance = Distance;
			LODOrigin = CameraPosition;
			LODHasNearClippingPlane = View->bHasNearClippingPlane;
			LODNearClippingPlane = View->NearClippingPlane;
		}
	}
}

/** Object position in post projection space. */
void FParticleSystemSceneProxy::GetObjectPositionAndScale(const FSceneView& View, FVector& ObjectPostProjectionPosition, FVector& ObjectNDCPosition, FVector4& ObjectMacroUVScales) const
{
	const FVector4 ObjectPostProjectionPositionWithW = View.ViewProjectionMatrix.TransformFVector(DynamicData->SystemPositionForMacroUVs);
	ObjectPostProjectionPosition = ObjectPostProjectionPositionWithW;
	ObjectNDCPosition = ObjectPostProjectionPositionWithW / Max(ObjectPostProjectionPositionWithW.W, 0.00001f);
	
	FLOAT MacroUVRadius = DynamicData->SystemRadiusForMacroUVs;
	FVector MacroUVPosition = DynamicData->SystemPositionForMacroUVs;
   
	UINT Index = DynamicData->EmitterIndex;
	const FDynamicEmitterReplayDataBase &EmitterData = DynamicData->DynamicEmitterDataArray(Index)->GetSource();
	if (EmitterData.bOverrideSystemMacroUV)
	{
		MacroUVRadius = EmitterData.MacroUVRadius;
		MacroUVPosition = LocalToWorld.TransformFVector(EmitterData.MacroUVPosition);
	}

	if (MacroUVRadius > 0.0f)
	{
		// Need to determine the scales required to transform positions into UV's for the ParticleMacroUVs material node
		// Determine screenspace extents by transforming the object position + appropriate camera vector * radius
		const FVector4 RightPostProjectionPosition = View.ViewProjectionMatrix.TransformFVector(MacroUVPosition + MacroUVRadius * View.ViewMatrix.GetColumn(0));
		const FVector4 UpPostProjectionPosition = View.ViewProjectionMatrix.TransformFVector(MacroUVPosition + MacroUVRadius * View.ViewMatrix.GetColumn(1));
		checkSlow(RightPostProjectionPosition.X - ObjectPostProjectionPositionWithW.X >= 0.0f && UpPostProjectionPosition.Y - ObjectPostProjectionPositionWithW.Y >= 0.0f);

		
		// Scales to transform the view space positions corresponding to SystemPositionForMacroUVs +- SystemRadiusForMacroUVs into [0, 1] in xy
		// Scales to transform the screen space positions corresponding to SystemPositionForMacroUVs +- SystemRadiusForMacroUVs into [0, 1] in zw
		ObjectMacroUVScales = FVector4(
			1.0f / (RightPostProjectionPosition.X - ObjectPostProjectionPositionWithW.X), 
			-1.0f / (UpPostProjectionPosition.Y - ObjectPostProjectionPositionWithW.Y),
			1.0f / (RightPostProjectionPosition.X / RightPostProjectionPosition.W - ObjectNDCPosition.X), 
			-1.0f / (UpPostProjectionPosition.Y / UpPostProjectionPosition.W - ObjectNDCPosition.Y)
			);
	}
	else
	{
		ObjectMacroUVScales = FVector4(0,0,0,0);
	}
}

UBOOL FParticleSystemSceneProxy::GetNearClippingPlane(FPlane& OutNearClippingPlane) const
{
	if(LODHasNearClippingPlane)
	{
		OutNearClippingPlane = LODNearClippingPlane;
	}
	return LODHasNearClippingPlane;
}

/**
 *	Retrieve the appropriate camera Up and Right vectors for LockAxis situations
 *
 *	@param	DynamicData		The emitter dynamic data the values are being retrieved for
 *	@param	CameraUp		OUTPUT - the resulting camera Up vector
 *	@param	CameraRight		OUTPUT - the resulting camera Right vector
 */
void FParticleSystemSceneProxy::GetAxisLockValues(FDynamicSpriteEmitterDataBase* DynamicData, UBOOL bUseLocalSpace, FVector& CameraUp, FVector& CameraRight)
{
	const FDynamicSpriteEmitterReplayData& SpriteSource =
		static_cast< const FDynamicSpriteEmitterReplayData& >( DynamicData->GetSource() );
	const FMatrix& AxisLocalToWorld = SpriteSource.bUseLocalSpace ? LocalToWorld: FMatrix::Identity;

	switch (SpriteSource.LockAxisFlag)
	{
	case EPAL_X:
		CameraUp		=  AxisLocalToWorld.GetAxis(2);
		CameraRight	=  AxisLocalToWorld.GetAxis(1);
		break;
	case EPAL_Y:
		CameraUp		=  AxisLocalToWorld.GetAxis(2);
		CameraRight	= -AxisLocalToWorld.GetAxis(0);
		break;
	case EPAL_Z:
		CameraUp		=  AxisLocalToWorld.GetAxis(0);
		CameraRight	= -AxisLocalToWorld.GetAxis(1);
		break;
	case EPAL_NEGATIVE_X:
		CameraUp		=  AxisLocalToWorld.GetAxis(2);
		CameraRight	= -AxisLocalToWorld.GetAxis(1);
		break;
	case EPAL_NEGATIVE_Y:
		CameraUp		=  AxisLocalToWorld.GetAxis(2);
		CameraRight	=  AxisLocalToWorld.GetAxis(0);
		break;
	case EPAL_NEGATIVE_Z:
		CameraUp		=  AxisLocalToWorld.GetAxis(0);
		CameraRight	=  AxisLocalToWorld.GetAxis(1);
		break;
	case EPAL_ROTATE_X:
		if (bUseLocalSpace)
		{
			CameraUp = AxisLocalToWorld.GetAxis(0);
			CameraUp = CameraUp.SafeNormal();
		}
		else
		{
			CameraUp = FVector(1.0f,0.0f,0.0f);
		}
		CameraRight = FVector(0.0f, 0.0f, 0.0f);
		break;
	case EPAL_ROTATE_Y:
		if (bUseLocalSpace)
		{
			CameraUp = AxisLocalToWorld.GetAxis(1);
			CameraUp = CameraUp.SafeNormal();
		}
		else
		{
			CameraUp = FVector(0.0f,1.0f,0.0f);
		}
		CameraRight = FVector(0.0f, 0.0f, 0.0f);
		break;
	case EPAL_ROTATE_Z:
		if (bUseLocalSpace)
		{
			CameraRight = AxisLocalToWorld.GetAxis(2);
			CameraRight = CameraRight.SafeNormal();
			CameraRight *= -1.0f;
		}
		else
		{
			CameraRight = FVector(0.0f,0.0f,-1.0f);
		}
		CameraUp = FVector(0.0f, 0.0f, 0.0f);
		break;
	}

	if ((SpriteSource.bUseLocalSpace == TRUE) &&
		(SpriteSource.LockAxisFlag >= EPAL_X) && (SpriteSource.LockAxisFlag <= EPAL_NEGATIVE_Z))
	{
		// Remove the scale factors
		CameraUp.Normalize();
		CameraRight.Normalize();
	}
}

/**
* @return Relevance for rendering the particle system primitive component in the given View
*/
FPrimitiveViewRelevance FParticleSystemSceneProxy::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;
	const EShowFlags ShowFlags = View->Family->ShowFlags;
	if (IsShown(View) && ShouldShowParticles(View))
	{
		Result.bDynamicRelevance = TRUE;
		Result.bNeedsPreRenderView = TRUE;
		Result.SetDPG(GetDepthPriorityGroup(View),TRUE);
		if (!(View->Family->ShowFlags & SHOW_Wireframe) && (View->Family->ShowFlags & SHOW_Materials))
		{
			MaterialViewRelevance.SetPrimitiveViewRelevance(Result);
		}
		SetRelevanceForShowBounds(View->Family->ShowFlags, Result);
		if (View->Family->ShowFlags & SHOW_Bounds)
		{
			Result.bOpaqueRelevance = TRUE;
		}
		// see if any of the emitters use dynamic vertex data
		if (DynamicData != NULL)
		{
			for (INT Index = 0; Index < DynamicData->DynamicEmitterDataArray.Num(); Index++)
			{
				FDynamicEmitterDataBase* Data =	DynamicData->DynamicEmitterDataArray(Index);
				if (Data == NULL)
				{
					continue;
				}
			}
		}
		else
		{
			// In order to get the LOD distances to update,
			// we need to force a call to DrawDynamicElements...
			Result.bOpaqueRelevance = TRUE;
		}
	}

	if (IsShadowCast(View))
	{
		Result.bShadowRelevance = TRUE;
	}

	return Result;
}

/**
 *	Helper function for calculating the tessellation for a given view.
 *
 *	@param	View		The view of interest.
 *	@param	FrameNumber		The frame number being rendered.
 */
void FParticleSystemSceneProxy::ProcessPreRenderView(const FSceneView* View, INT FrameNumber)
{
	const FSceneView* LocalView = View;
	if (View->ParentViewFamily)
	{
		if ((View->ParentViewIndex != -1) && (View->ParentViewIndex <= View->ParentViewFamily->Views.Num()))
		{
			// If the ParentViewIndex is set to a valid index, use that View
			LocalView = View->ParentViewFamily->Views(View->ParentViewIndex);
		}
		else
		if (View->ParentViewIndex == -1)
		{
			// Iterate over all the Views in the ParentViewFamily
			FSceneView TempView(
				View->Family,
				View->State,
				-1,
				View->ParentViewFamily,
				View->ActorVisibilityHistory,
				View->ViewActor,
				View->PostProcessChain,
				View->PostProcessSettings,
				View->Drawer,
				View->X,
				View->Y,
				View->ClipX,
				View->ClipY,
				View->SizeX,
				View->SizeY,
				View->ViewMatrix,
				View->ProjectionMatrix,
				View->BackgroundColor,
				View->OverlayColor,
				View->ColorScale,
				View->HiddenPrimitives,
				FRenderingPerformanceOverrides(E_ForceInit),
				View->LODDistanceFactor
				);
			for (INT ViewIdx = 0; ViewIdx < View->ParentViewFamily->Views.Num(); ViewIdx++)
			{
				TempView.ParentViewIndex = ViewIdx;
				ProcessPreRenderView(&TempView, FrameNumber);
			}
			return;
		}
	}

	if ((GIsEditor == TRUE) || (GbEnableGameThreadLODCalculation == FALSE))
	{
	if (DynamicData && DynamicData->bNeedsLODDistanceUpdate)
	{
		DetermineLODDistance(LocalView, FrameNumber);
	}
}
}

/**
 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
 *  Only called for primitives that are visible and have bDynamicRelevance
 *
 *	@param	ViewFamily		The ViewFamily to pre-render for
 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
 *	@param	FrameNumber		The frame number of this pre-render
 */
void FParticleSystemSceneProxy::PreRenderView(const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber)
{
	for (INT ViewIndex = 0; ViewIndex < ViewFamily->Views.Num(); ViewIndex++)
	{
		ProcessPreRenderView(ViewFamily->Views(ViewIndex), FrameNumber);
	}

	if (DynamicData != NULL)
	{
		for (INT EmitterIndex = 0; EmitterIndex < DynamicData->DynamicEmitterDataArray.Num(); EmitterIndex++)
		{
			FDynamicEmitterDataBase* DynamicEmitterData = DynamicData->DynamicEmitterDataArray(EmitterIndex);
			if (DynamicEmitterData)
			{
				DynamicEmitterData->PreRenderView(this, ViewFamily, VisibilityMap, FrameNumber);
			}
		}
	}
}

/**
 *	Occluding particle system scene proxy...
 */
/** Initialization constructor. */
FParticleSystemOcclusionSceneProxy::FParticleSystemOcclusionSceneProxy(const UParticleSystemComponent* Component) :
	  FParticleSystemSceneProxy(Component)
	, FPrimitiveSceneProxyOcclusionTracker(Component)
	, bHasCustomOcclusionBounds(FALSE)
{
	if (Component->Template && (Component->Template->OcclusionBoundsMethod == EPSOBM_CustomBounds))
	{
		OcclusionBounds = FBoxSphereBounds(Component->Template->CustomOcclusionBounds);
		bHasCustomOcclusionBounds = TRUE;
	}
}

FParticleSystemOcclusionSceneProxy::~FParticleSystemOcclusionSceneProxy()
{
}

/** 
 * Draw the scene proxy as a dynamic element
 *
 * @param	PDI - draw interface to render to
 * @param	View - current view
 * @param	DPGIndex - current depth priority 
 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
 */
void FParticleSystemOcclusionSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	if (ShouldShowParticles(View))
	{
		if (DynamicData != NULL)
		{
			FBoxSphereBounds TempOcclusionBounds = OcclusionBounds;
			if (bHasCustomOcclusionBounds == TRUE)
			{
				OcclusionBounds = OcclusionBounds.TransformBy(LocalToWorld);
			}

			UBOOL bDrawElements = TRUE;
			// Update the occlusion data every frame
			if (UpdateAndRenderOcclusionData(PrimitiveSceneInfo->Component, PDI, View, DPGIndex, Flags) == FALSE)
			{
				bDrawElements = FALSE;
			}

			// Update the occlusion bounds for next frame...
			if (bHasCustomOcclusionBounds == FALSE)
			{
				OcclusionBounds = GetBounds();
			}
			else
			{
				OcclusionBounds = TempOcclusionBounds;
			}

			if (bDrawElements == TRUE)
			{
				FParticleSystemSceneProxy::DrawDynamicElements(PDI, View, DPGIndex, Flags);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//	ParticleSystemComponent
///////////////////////////////////////////////////////////////////////////////

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	FParticleSystemSceneProxy* NewProxy = NULL;

	//@fixme EmitterInstances.Num() check should be here to avoid proxies for dead emitters but there are some edge cases where it happens for emitters that have just activated...
	if ((bIsActive == TRUE)/** && (EmitterInstances.Num() > 0)*/ && Template)
	{
		if (EmitterInstances.Num() > 0)
		{
			CacheViewRelevanceFlags(NULL);
		}

		if (Template->OcclusionBoundsMethod == EPSOBM_None)
		{
			NewProxy = ::new FParticleSystemSceneProxy(this);
		}
		else
		{
			Template->CustomOcclusionBounds.IsValid = TRUE;
			NewProxy = ::new FParticleSystemOcclusionSceneProxy(this);
		}
		check (NewProxy);
	}
	
	// 
	return NewProxy;
}

void UParticleSystemComponent::SetLightEnvironment(ULightEnvironmentComponent* NewLightEnvironment)
{
	// Verify that any light environments set on this component derive from UParticleLightEnvironmentComponent to avoid leaking DLE's
	UParticleLightEnvironmentComponent* ParticleDLE = Cast<UParticleLightEnvironmentComponent>(NewLightEnvironment);
	check(!NewLightEnvironment || ParticleDLE);
	Super::SetLightEnvironment(NewLightEnvironment);
}

#if WITH_EDITOR
void DrawParticleSystemHelpers(UParticleSystemComponent* InPSysComp, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	if (InPSysComp != NULL)
	{
		for (INT EmitterIdx = 0; EmitterIdx < InPSysComp->EmitterInstances.Num(); EmitterIdx++)
		{
			FParticleEmitterInstance* EmitterInst = InPSysComp->EmitterInstances(EmitterIdx);
			if (EmitterInst && EmitterInst->SpriteTemplate)
			{
				UParticleLODLevel* LODLevel = EmitterInst->SpriteTemplate->GetCurrentLODLevel(EmitterInst);
				for (INT ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
				{
					UParticleModule* Module = LODLevel->Modules(ModuleIdx);
					if (Module && Module->bSupported3DDrawMode && Module->b3DDrawMode)
					{
						Module->Render3DPreview(EmitterInst, View, PDI);
					}
				}
			}
		}
	}
}

void DrawParticleSystemHelpers(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	for (TObjectIterator<AEmitter> It; It; ++It)
	{
		AEmitter* EmitterActor = *It;
		if (EmitterActor->ParticleSystemComponent != NULL)
		{
			DrawParticleSystemHelpers(EmitterActor->ParticleSystemComponent, View, PDI);
		}
	}
}
#endif	//#if WITH_EDITOR
