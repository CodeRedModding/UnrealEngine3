/*=============================================================================
	UnStats.h: Performance stats framework.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#define STAT_HISTORY_SIZE	20
#define MAX_STAT_LABEL		256

//
//	FStatGroup - A set of stat counters.
//

struct FStatGroup
{
	TCHAR				Label[MAX_STAT_LABEL];
	struct FStatBase*	FirstStat;
	FStatGroup*			NextGroup;

	/** Whether this stat group is currently enabled & rendered */
	UBOOL				Enabled;

	// Constructor/destructor.

	FStatGroup(const TCHAR* InLabel);
	~FStatGroup();

	// AdvanceFrame - Stores the current frame's stats in the history and resets the current values.

	void AdvanceFrame();

	// Render

	INT Render(FRenderInterface* RI,INT X,INT Y);
};

//
//	FStatBase
//

struct FStatBase
{
	TCHAR		Label[MAX_STAT_LABEL];
	FStatBase*	NextStat;

	// Constructor.

	FStatBase(struct FStatGroup* InGroup,const TCHAR* InLabel)
	{
		appStrncpy(Label,InLabel,MAX_STAT_LABEL);
		NextStat = InGroup->FirstStat;
		InGroup->FirstStat = this;
	}

	// AdvanceFrame - Stores the current value in the history and resets the current value.

	virtual void AdvanceFrame() = 0;

	// Render

	virtual INT Render(FRenderInterface* RI,INT X,INT Y) = 0;
};

//
//	FStat - Stores the current value and history for a stat.
//

template<class T> T InitialStatValue() { return 0; }

template<class T> struct FStat: FStatBase
{
	T		Value;
	T		History[STAT_HISTORY_SIZE];
	DWORD	HistoryHead;
	/** Whether to append value to description or append description to value */
	UBOOL	AppendValue;

	// Constructor.

	FStat(struct FStatGroup* InGroup,const TCHAR* InLabel,UBOOL InAppendValue=true):
		FStatBase(InGroup,InLabel),
		Value(InitialStatValue<T>()),
		HistoryHead(0),
		AppendValue(InAppendValue)
	{
		for(UINT HistoryIndex = 0;HistoryIndex < STAT_HISTORY_SIZE;HistoryIndex++)
			History[HistoryIndex] = InitialStatValue<T>();
	}

	// FStatBase interface.

	virtual void AdvanceFrame()
	{
		History[HistoryHead] = Value;
		HistoryHead = (HistoryHead + 1) % STAT_HISTORY_SIZE;

		Value = InitialStatValue<T>();
	}
	virtual INT Render(FRenderInterface* RI,INT X,INT Y) = 0;
};

//
//	FStatCounter
//

struct FStatCounter: FStat<DWORD>
{
	// Constructor.

	FStatCounter(struct FStatGroup* InGroup,const TCHAR* InLabel,UBOOL InAppendValue=true): FStat<DWORD>(InGroup,InLabel,InAppendValue) {}

	// FStatBase interface.

	virtual INT Render(FRenderInterface* RI,INT X,INT Y);
};



/**
 * A counter using float as intermediate storage
 */
struct FStatCounterFloat: FStat<FLOAT>
{
	/** 
	 * Constructor
	 *
	 * @param InGroup		stat group this stat belongs to
	 * @param InLable		label to be displayed when rendering 
	 * @param InAppendValue	whether to append the value to the label or the labe to the value
	 */
	FStatCounterFloat(struct FStatGroup* InGroup,const TCHAR* InLabel,UBOOL InAppendValue=true): FStat<FLOAT>(InGroup,InLabel,InAppendValue) {}

	// FStatBase interface.

	/**
	 * Displays the stat
	 *
	 * @param RI	render interface used for rendering
	 * @param X		x screen position
	 * @param Y		y screen position
	 *
	 * @return The height of the used font
	 */
	virtual INT Render(FRenderInterface* RI,INT X,INT Y);
};

//
//	FCycleCounter - A CPU cycle stat counter.
//

struct FCycleCounter: FStat<DWORD>
{
private:

	// Hide the copy constructor to prevent accidental misuse of FCycleCounter where FCycleCounterSection was intended.
	FCycleCounter(const FCycleCounter& InCopy):
	   FStat<DWORD>(InCopy)
	{}

public:

	// Constructor.

	FCycleCounter(FStatGroup* InGroup,const TCHAR* InLabel,UBOOL InAppendValue=true): FStat<DWORD>(InGroup,InLabel,InAppendValue) {}

	// FStatBase interface.

	virtual INT Render(FRenderInterface* RI,INT X,INT Y);
};

//
//	FCycleCounterSection - A utility class that adds the cycles between it's creation and destruction to a cycle counter.
//

struct FCycleCounterSection
{
	FCycleCounter&	Counter;
	DWORD			StartCycles;

	// Constructor/destructor.

	FCycleCounterSection(FCycleCounter& InCounter):
		Counter(InCounter),
		StartCycles(appCycles())
	{
	}

	~FCycleCounterSection()
	{
		Counter.Value += appCycles() - StartCycles;
	}
};

#define BEGINCYCLECOUNTER(x) \
	{ \
		FCycleCounterSection	CycleCounter(x);

#define ENDCYCLECOUNTER }

//
//	FEngineStatGroup
//

struct FEngineStatGroup: FStatGroup
{
	FCycleCounter	FrameTime,
					HudTime,
					ShadowTime,
					SkeletalAnimTime,
					SkeletalSkinningTime,
					TerrainSmoothTime,
					TerrainFoliageTime,
					InputTime,
					UnrealScriptTime;
	FStatCounter	ShadowTriangles,
					StaticMeshTriangles,
					SkeletalMeshTriangles,
					TerrainTriangles,
					TerrainFoliageInstances,
					BSPTriangles,
					SkeletalSkinVertices;

	// Constructor.

	FEngineStatGroup():
		FStatGroup(TEXT("Engine")),
		FrameTime(this,TEXT("Frame time")),
		HudTime(this,TEXT("HUD time")),
		ShadowTime(this,TEXT("Shadow time")),
		SkeletalAnimTime(this,TEXT("Skeletal anim time")),
		SkeletalSkinningTime(this,TEXT("Skeletal skinning time")),
		InputTime(this,TEXT("Input time")),
		UnrealScriptTime(this,TEXT("UnrealScript time")),
		ShadowTriangles(this,TEXT("Shadow triangles")),
		StaticMeshTriangles(this,TEXT("Static mesh triangles")),
		SkeletalMeshTriangles(this,TEXT("Skeletal mesh triangles")),
		TerrainSmoothTime(this,TEXT("Terrain smoothing time")),
		TerrainFoliageTime(this,TEXT("Terrain foliage time")),
		TerrainTriangles(this,TEXT("Terrain triangles")),
		TerrainFoliageInstances(this,TEXT("Terrain foliage instances")),
		BSPTriangles(this,TEXT("BSP triangles")),
		SkeletalSkinVertices(this,TEXT("Skeletal skin vertices"))
	{}
};

//
//	FAudioStatGroup
//

struct FAudioStatGroup : FStatGroup
{
	FCycleCounter	UpdateTime;
	
	FStatCounter	AudioComponents,
					WaveInstances;

	FAudioStatGroup()
	:	FStatGroup(TEXT("Audio")),
		UpdateTime(this,TEXT("Update time")),
		AudioComponents(this,TEXT("Sounds")),
		WaveInstances(this,TEXT("Channels"))
	{}
};

//
//	FCollisionStatGroup
//

struct FCollisionStatGroup: FStatGroup
{
	FCycleCounter	TerrainZeroExtentTime,
					TerrainExtentTime,
					TerrainPointTime,
					StaticMeshZeroExtentTime,
					StaticMeshExtentTime,
					StaticMeshPointTime,
					BSPZeroExtentTime,
					BSPExtentTime,
					BSPPointTime;

	// Constructor.

	FCollisionStatGroup():
		FStatGroup(TEXT("Collision")),
		TerrainZeroExtentTime(this,TEXT("Terrain line")),
		TerrainExtentTime(this,TEXT("Terrain swept box")),
		TerrainPointTime(this,TEXT("Terrain point")),
		StaticMeshZeroExtentTime(this,TEXT("Static mesh line")),
		StaticMeshExtentTime(this,TEXT("Static mesh swept box")),
		StaticMeshPointTime(this,TEXT("Static mesh point")),
		BSPZeroExtentTime(this,TEXT("BSP line")),
		BSPExtentTime(this,TEXT("BSP swept box")),
		BSPPointTime(this,TEXT("BSP point"))
	{}
};

//
//	FPhysicsStatGroup
//
struct FPhysicsStatGroup: FStatGroup
{
	FCycleCounter	RBCollisionTime,
					RBDynamicsTime;

	FPhysicsStatGroup():
		FStatGroup(TEXT("Physics")),
		RBCollisionTime(this,TEXT("Rigid Body Collision")),
		RBDynamicsTime(this,TEXT("Rigid Body Dynamics"))
	{}
};

//
//	FVisibilityStatGroup
//
struct FVisibilityStatGroup: FStatGroup
{
	FStatCounter	VisiblePrimitives,
					PrimitiveTests,
					VisibleOctreeNodes,
					OctreeNodeTests,
					VisiblePortals,
					VisibilityVolumes,
					LightAdds,
					LightRemoves,
					PrimitiveAdds,
					PrimitiveRemoves;
	FCycleCounter	ZoneGraphTime,
					FrustumCheckTime,
					RelevantLightTime,
					AddPrimitiveTime,
					AddLightTime,
					RemovePrimitiveTime,
					RemoveLightTime;

	FVisibilityStatGroup():
		FStatGroup(TEXT("Visibility")),
		VisiblePrimitives(this,TEXT("Visible primitives")),
		PrimitiveTests(this,TEXT("Primitive tests")),
		VisibleOctreeNodes(this,TEXT("Visible octree nodes")),
		OctreeNodeTests(this,TEXT("Octree node tests")),
		VisiblePortals(this,TEXT("Visible portals")),
		VisibilityVolumes(this,TEXT("Visibility volumes")),
		LightAdds(this,TEXT("Light adds")),
		LightRemoves(this,TEXT("Light removes")),
		PrimitiveAdds(this,TEXT("Primitive adds")),
		PrimitiveRemoves(this,TEXT("Primitive removes")),
		ZoneGraphTime(this,TEXT("Zone graph time")),
		FrustumCheckTime(this,TEXT("Frustum check time")),
		RelevantLightTime(this,TEXT("Relevant light time")),
		AddPrimitiveTime(this,TEXT("Add primitive time")),
		AddLightTime(this,TEXT("Add light time")),
		RemovePrimitiveTime(this,TEXT("Remove primitive time")),
		RemoveLightTime(this,TEXT("Remove light time"))
	{}
};

//
//	FStreamingStatGroup
//
struct FStreamingStatGroup : FStatGroup
{
	FStatCounterFloat	TextureSize;
	FStatCounterFloat	MaxTextureSize;
	FStatCounter		StreamedInTextures;
	FStatCounter		OutstandingTextureRequests;
	FStatCounter		CanceledTextureRequests;
	FCycleCounter		ProcessViewerTime;
	FCycleCounter		TickTime;
	FCycleCounter		DataShuffleTime;
	FCycleCounter		FinalizeTime;

	FStreamingStatGroup()
	:	FStatGroup(TEXT("Streaming")),
		TextureSize(this,TEXT("Total size of streamable textures in MByte")),
		MaxTextureSize(this,TEXT("Maximum size of textures in MByte")),
		StreamedInTextures(this,TEXT("Streamable textures")),
		OutstandingTextureRequests(this,TEXT("Outstanding texture requests")),
		CanceledTextureRequests(this,TEXT("Canceled texture requests")),
		ProcessViewerTime(this,TEXT("ProcessViewer time")),
		TickTime(this,TEXT("Tick time")),
		DataShuffleTime(this,TEXT("Data shuffling time")),
		FinalizeTime(this,TEXT("Finalize time"))
	{}
};

/**
 * Memory statistics.
 */
struct FMemoryStatGroup : FStatGroup
{
	/** Size of index buffers									*/
	FStatCounterFloat	IndexBufferSize;
	/** Size of vertex buffers									*/
	FStatCounterFloat	VertexBufferSize;
	/** Size of textures minus types listed explicitely below	*/
	FStatCounterFloat	TextureSize;
	/** Size of terrain shadow maps								*/
	FStatCounterFloat	TerrainShadowMapSize;
	/** Size of BSP shadow maps									*/
	FStatCounterFloat	BSPShadowMapSize;
	/** Size of static mesh shadow maps							*/
	FStatCounterFloat	StaticMeshShadowMapSize;
	/** Size of virtual allocations								*/
	FStatCounterFloat	VirtualAllocations;
	/** Size of physical allocations							*/
	FStatCounterFloat	PhysicalAllocations;

	/** Constructor, initializing variable name to caption mapping */
	FMemoryStatGroup()
	:	FStatGroup(TEXT("Memory")),
		IndexBufferSize(this,TEXT("Total size of index buffers in MByte")),
		VertexBufferSize(this,TEXT("Total size of vertex buffers in MByte")),
		TextureSize(this,TEXT("Total size of textures in MByte")),
		TerrainShadowMapSize(this,TEXT("Total size of terrain shadow maps in MByte")),
		BSPShadowMapSize(this,TEXT("Total size of BSP shadow maps in MByte")),
		StaticMeshShadowMapSize(this,TEXT("Total size of static mesh shadow maps in MByte")),
		PhysicalAllocations(this,TEXT("Total size of physical allocations in MByte")),
		VirtualAllocations(this,TEXT("Total size of virtual allocations in MByte"))
	{}
};

//
//	Stat globals.
//

extern FStatGroup*			GFirstStatGroup;
extern FEngineStatGroup		GEngineStats;
extern FCollisionStatGroup	GCollisionStats;
extern FPhysicsStatGroup	GPhysicsStats;
extern FVisibilityStatGroup	GVisibilityStats;
extern FAudioStatGroup		GAudioStats;
extern FStreamingStatGroup	GStreamingStats;
extern FMemoryStatGroup		GMemoryStats;