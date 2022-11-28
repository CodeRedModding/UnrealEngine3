/*=============================================================================
	D3DStats.h: Performance stats framework.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	FD3DCounter
//

struct FD3DCounter
{
	DWORD	RenderCycles,
			WaitCycles,
			Pixels;

	// Constructor.

	FD3DCounter():
		RenderCycles(0),
		WaitCycles(0),
		Pixels(0)
	{}
};

template<> inline FD3DCounter InitialStatValue<FD3DCounter>() { return FD3DCounter(); }

//
//	FD3DCycleCounter
//

struct FD3DCycleCounter: FStat<FD3DCounter>
{
	// Constructor.

	FD3DCycleCounter(FStatGroup* InGroup,const TCHAR* InLabel): FStat<FD3DCounter>(InGroup,InLabel) {}

	// FStatBase interface.

	virtual INT Render(FRenderInterface* RI,INT X,INT Y)
	{
		DOUBLE	AverageTotalCycles = 0.0;

		for(DWORD HistoryIndex = 0;HistoryIndex < STAT_HISTORY_SIZE;HistoryIndex++)
			AverageTotalCycles += (History[HistoryIndex].RenderCycles + History[HistoryIndex].WaitCycles) / DOUBLE(STAT_HISTORY_SIZE);

		RI->DrawShadowedString(
			X,
			Y,
			*FString::Printf(
				TEXT("%s: %2.1f ms (Render: %2.1f ms, Wait: %2.1f ms)"),
				Label,
				GSecondsPerCycle * 1000.0 * AverageTotalCycles,
				GSecondsPerCycle * 1000.0 * History[(HistoryHead + STAT_HISTORY_SIZE - 1) % STAT_HISTORY_SIZE].RenderCycles,
				GSecondsPerCycle * 1000.0 * History[(HistoryHead + STAT_HISTORY_SIZE - 1) % STAT_HISTORY_SIZE].WaitCycles
				),
			GEngine->SmallFont,
			FLinearColor(0,1,0)
			);
		return 12;
	}
};

//
//	FD3DCycleCounterSection
//

struct FD3DCycleCounterSection
{
	FD3DCycleCounter&	Counter;
	DWORD				StartCycles;

	// Constructor/destructor.

	FD3DCycleCounterSection(FD3DCycleCounter& InCounter):
		Counter(InCounter),
		StartCycles(appCycles())
	{}

	~FD3DCycleCounterSection();
};

#define BEGIND3DCYCLECOUNTER(x) \
	{ \
		FD3DCycleCounterSection	CycleCounter(x);

#define ENDD3DCYCLECOUNTER \
	}

//
//	FD3DStatGroup
//

struct FD3DStatGroup: FStatGroup
{
	FCycleCounter		SceneSetupTime;
	FCycleCounter		SceneRenderTime;
	FD3DCycleCounter	UnlitRenderTime;
	FD3DCycleCounter	DepthRenderTime;
	FD3DCycleCounter	EmissiveRenderTime;
	FD3DCycleCounter	ShadowRenderTime;
	FD3DCycleCounter	ShadowDepthTime;
	FD3DCycleCounter	ShadowDepthTestTime;
	FD3DCycleCounter	LightRenderTime;
	FD3DCycleCounter	PostProcessTime;
	FD3DCycleCounter	LightBufferCopyTime;
	FD3DCycleCounter	OcclusionQueryTime;

	FCycleCounter		TileRenderTime;
	FD3DCycleCounter	FillRenderTime;
	FCycleCounter		FinishRenderTime;
	FCycleCounter		WaitingTime;
	FCycleCounter		TranslucencyLayerQueryTime;

	FStatCounter		Tiles,
						TileBatches,
						MeshTriangles,
						BufferSize,
						IncompleteOcclusionQueries,
						CulledOcclusionQueries,
						DegenerateOcclusionQueries,
						UniqueOcclusionQueries,
						SceneLayers,
						ShadowFrustums,
						OpaquePrimitives,
						TranslucentPrimitives,
						BackgroundPrimitives,
						ForegroundPrimitives,
						LightPasses;

	FCycleCounter		SetUserTextureTime;
	FStatCounter		SetUserTextures;

	// Constructor.

	FD3DStatGroup():
		FStatGroup(TEXT("D3D")),
		SceneSetupTime(this,TEXT("Scene setup")),
		SceneRenderTime(this,TEXT("Scene rendering")),
		UnlitRenderTime(this,TEXT("Unlit rendering")),
		DepthRenderTime(this,TEXT("Depth rendering")),
		EmissiveRenderTime(this,TEXT("Emissive rendering")),
		ShadowRenderTime(this,TEXT("Shadow rendering")),
		ShadowDepthTime(this,TEXT("Shadow depth rendering")),
		ShadowDepthTestTime(this,TEXT("Shadow depth testing")),
		LightRenderTime(this,TEXT("Light rendering")),
		PostProcessTime(this,TEXT("Post processing")),
		LightBufferCopyTime(this,TEXT("Light buffer copying")),
		TileRenderTime(this,TEXT("Tile rendering")),
		FillRenderTime(this,TEXT("Fill rendering")),
		FinishRenderTime(this,TEXT("Finish time")),
		WaitingTime(this,TEXT("Waiting time")),
		TranslucencyLayerQueryTime(this,TEXT("Translucency layer query time")),
		OcclusionQueryTime(this,TEXT("Occlusion query time")),
		Tiles(this,TEXT("Tiles")),
		TileBatches(this,TEXT("Tile batches")),
		MeshTriangles(this,TEXT("Mesh triangles")),
		BufferSize(this,TEXT("Buffer size")),
		IncompleteOcclusionQueries(this,TEXT("Incomplete occlusion queries")),
		CulledOcclusionQueries(this,TEXT("Culled occlusion queries")),
		DegenerateOcclusionQueries(this,TEXT("Degenerate occlusion queries")),
		UniqueOcclusionQueries(this,TEXT("Unique occlusion queries")),
		SceneLayers(this,TEXT("Scene layers")),
		ShadowFrustums(this,TEXT("Shadow frustums")),
		OpaquePrimitives(this,TEXT("Opaque primitives")),
		TranslucentPrimitives(this,TEXT("Translucent primitives")),
		BackgroundPrimitives(this,TEXT("Background primitives")),
		ForegroundPrimitives(this,TEXT("Foreground primitives")),
		LightPasses(this,TEXT("Light passes")),
		SetUserTextureTime(this,TEXT("SetUserTexture time")),
		SetUserTextures(this,TEXT("SetUserTextures"))
	{}
};
