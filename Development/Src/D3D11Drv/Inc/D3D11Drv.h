/*=============================================================================
	D3D11Drv.h: Public D3D RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_D3D11DRV
#define _INC_D3D11DRV

// D3D headers.
#if _WIN64
#pragma pack(push,16)
#else
#pragma pack(push,8)
#endif
#define D3D_OVERLOADS 1
#include "D3D11.h"
// This depends on the environment variable DXSDK_DIR being setup by the DXSDK.
// All projects that use D3D must add DXSDK_DIR/Inc to their include paths
// All projects that link with D3D must add DXSDK_DIR/Lib/x(86|64) to their linker paths
#include "D3D11.h"
#include "D3DX11.h"
#undef DrawText
#pragma pack(pop)

#define NO_STEREO_D3D9 1
#include "ue3stereo.h"
#undef NO_STEREO_D3D9

#include "StaticArray.h"

/**
 * Make sure we are compiling against the DXSDK we are expecting to,
 * Which is the June 2010 DX SDK.
 */
const INT REQUIRED_D3DX11_SDK_VERSION = 43;
checkAtCompileTime(D3DX11_SDK_VERSION == REQUIRED_D3DX11_SDK_VERSION, D3DX11_SDK_VERSION_DoesNotMatchRequiredVersion);

/** This is a macro that casts a dynamically bound RHI reference to the appropriate D3D type. */
#define DYNAMIC_CAST_D3D11RESOURCE(Type,Name) \
	FD3D11##Type* Name = (FD3D11##Type*)Name##RHI;

// D3D RHI public headers.
#include "D3D11Util.h"
#include "D3D11State.h"
#include "D3D11Resources.h"
#include "D3D11RenderTarget.h"
#include "D3D11Viewport.h"
#include "D3D11ConstantBuffer.h"
#include "D3D11CachedStates.h"

/** A D3D event query resource. */
class FD3D11EventQuery : public FRenderResource
{
public:

	/** Initialization constructor. */
	FD3D11EventQuery(class FD3D11DynamicRHI* InD3DRHI):
		D3DRHI(InD3DRHI)
	{
	}

	/** Issues an event for the query to poll. */
	void IssueEvent();

	/** Waits for the event query to finish. */
	void WaitForCompletion();

	// FRenderResource interface.
	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

private:
	FD3D11DynamicRHI* D3DRHI;
	TRefCountPtr<ID3D11Query> Query;
};

class FD3D11BufferedGPUTiming : public FRenderResource
{
public:

	/**
	 * Constructor.
	 *
	 * @param InD3DRHI			RHI interface
	 * @param InBufferSize		Number of buffered measurements
	 */
	FD3D11BufferedGPUTiming(class FD3D11DynamicRHI* InD3DRHI, INT BufferSize);

	/**
	 * Start a GPU timing measurement.
	 */
	void	StartTiming();

	/**
	 * End a GPU timing measurement.
	 * The timing for this particular measurement will be resolved at a later time by the GPU.
	 */
	void	EndTiming();

	/**
	 * Retrieves the most recently resolved timing measurement.
	 * The unit is the same as for appCycles(). Returns 0 if there are no resolved measurements.
	 *
	 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	 */
	QWORD	GetTiming(UBOOL bGetCurrentResultsAndBlock = FALSE);

	/**
	 * Whether GPU timing measurements are supported by the driver.
	 *
	 * @return TRUE if GPU timing measurements are supported by the driver.
	 */
	static UBOOL IsSupported()
	{
		return GIsSupported;
	}

	/**
	 * Returns the frequency for the timing values, in number of ticks per seconds.
	 *
	 * @return Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported.
	 */
	static QWORD GetTimingFrequency()
	{
		return GTimingFrequency;
	}

	/**
	 * Initializes all D3D resources.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases all D3D resources.
	 */
	virtual void ReleaseDynamicRHI();


private:

	/**
	 * Initializes the static variables, if necessary.
	 */
	void	StaticInitialize();

	/** RHI interface */
	FD3D11DynamicRHI* D3DRHI;
	/** Number of timestamps created in 'StartTimestamps' and 'EndTimestamps'. */
	INT								BufferSize;
	/** Current timing being measured on the CPU. */
	INT								CurrentTimestamp;
	/** Number of measurements in the buffers (0 - BufferSize). */
	INT								NumIssuedTimestamps;
	/** Timestamps for all StartTimings. */
	TRefCountPtr<ID3D11Query>*	StartTimestamps;
	/** Timestamps for all EndTimings. */
	TRefCountPtr<ID3D11Query>*	EndTimestamps;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	UBOOL							bIsTiming;

	/** Whether the static variables have been initialized. */
	static UBOOL					GAreGlobalsInitialized;
	/** Whether GPU timing measurements are supported by the driver. */
	static UBOOL					GIsSupported;
	/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
	static QWORD					GTimingFrequency;
};

/** Used to track whether a period was disjoint on the GPU, which means GPU timings are invalid. */
class FD3D11DisjointTimeStampQuery : public FRenderResource
{
public:
	FD3D11DisjointTimeStampQuery(class FD3D11DynamicRHI* InD3DRHI);

	void StartTracking();
	void EndTracking();
	UBOOL WasDisjoint();

	/**
	 * Initializes all D3D resources.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases all D3D resources.
	 */
	virtual void ReleaseDynamicRHI();


private:

	TRefCountPtr<ID3D11Query> DisjointQuery;

	FD3D11DynamicRHI* D3DRHI;
};

/** Stats for a single perf event node. */
class FD3D11EventNodeStats : public FRefCountedObject
{
public:

	FD3D11EventNodeStats() :
		NumDraws(0),
		NumPrimitives(0),
		TimingResult(0),
		NumEvents(0)
	{
	}

	/** Exclusive number of draw calls rendered in this event. */
	UINT NumDraws;

	/** Exclusive number of primitives rendered in this event. */
	UINT NumPrimitives;

	/** GPU time spent inside the perf event's begin and end, in ms. */
	FLOAT TimingResult;

	/** Inclusive number of other perf events that this is the parent of. */
	UINT NumEvents;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FD3D11EventNode : public FD3D11EventNodeStats
{
public:

	FD3D11EventNode(const TCHAR* InName, FD3D11EventNode* InParent, class FD3D11DynamicRHI* InRHI) :
		FD3D11EventNodeStats(),
		Name(InName),
		Parent(InParent),
		Timing(InRHI, 1)
	{
		// Initialize Buffered timestamp queries 
		Timing.InitResource();
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	FLOAT GetTiming(); 

	~FD3D11EventNode()
	{
		Timing.ReleaseResource();
	}

	FString Name;

	FD3D11BufferedGPUTiming Timing;

	/** Pointer to parent node so we can walk up the tree on appEndDrawEvent. */
	FD3D11EventNode* Parent;

	/** Children perf event nodes. */
	TArray<TRefCountPtr<FD3D11EventNode> > Children;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FD3D11EventNodeFrame
{
public:

	FD3D11EventNodeFrame(class FD3D11DynamicRHI* InRHI) :
		RootEventTiming(InRHI, 1),
		DisjointQuery(InRHI)
	{

	  RootEventTiming.InitResource();
	  DisjointQuery.InitResource();
	}
	~FD3D11EventNodeFrame()
	{

		RootEventTiming.ReleaseResource();
		DisjointQuery.ReleaseResource();
	}

	/** Start this frame of per tracking */
	void StartFrame();

	/** End this frame of per tracking, but do not block yet */
	void EndFrame();

	/** Dumps perf event information, blocking on GPU. */
	void DumpEventTree();

	/** Root nodes of the perf event tree. */
	TArray<TRefCountPtr<FD3D11EventNode> > EventTree;

	/** Timer tracking inclusive time spent in the root nodes. */
	FD3D11BufferedGPUTiming RootEventTiming;

	/** Disjoint query tracking whether the times reported by DumpEventTree are reliable. */
	FD3D11DisjointTimeStampQuery DisjointQuery;

};

/** The interface which is implemented by the dynamically bound RHI. */
class FD3D11DynamicRHI : public FDynamicRHI
{
public:

	friend class FD3D11Viewport;

	/** Global D3D11 lock list */
	TMap<FD3D11LockedKey,FD3D11LockedData> OutstandingLocks;

	/** Initialization constructor. */
	FD3D11DynamicRHI(IDXGIFactory* InDXGIFactory,D3D_FEATURE_LEVEL InFeatureLevel);

	/** Destructor */
	~FD3D11DynamicRHI();

	/** If it hasn't been initialized yet, initializes the D3D device. */
	void InitD3DDevice();

	/**
	 * Reads a D3D query's data into the provided buffer.
	 * @param Query - The D3D query to read data from.
	 * @param Data - The buffer to read the data into.
	 * @param DataSize - The size of the buffer.
	 * @param bWait - If TRUE, it will wait for the query to finish.
	 * @return TRUE if the query finished.
	 */
	UBOOL GetQueryData(ID3D11Query* Query,void* Data,SIZE_T DataSize,UBOOL bWait);

	// The RHI methods are defined as virtual functions in URenderHardwareInterface.
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type Name ParameterTypesAndNames
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

	// Reference counting API for the different resource types.
	#define IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE(Type,ParentType) \
		virtual void AddResourceRef(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_D3D11RESOURCE(Type,Reference); \
			Reference->AddRef(); \
		} \
		virtual void RemoveResourceRef(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_D3D11RESOURCE(Type,Reference); \
			Reference->Release(); \
		} \
		virtual DWORD GetRefCount(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_D3D11RESOURCE(Type,Reference); \
			Reference->AddRef(); \
			return Reference->Release(); \
		}

	ENUM_RHI_RESOURCE_TYPES(IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE);

	#undef IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE

	// Accessors.
	ID3D11Device* GetDevice() const
	{
		return Direct3DDevice;
	}
	ID3D11DeviceContext* GetDeviceContext() const
	{
		return Direct3DDeviceIMContext;
	}
	IDXGIFactory* GetFactory() const
	{
		return DXGIFactory;
	}

	UINT GetMSAACount() {UpdateMSAASettings(); return MSAACount;}
	UINT GetMSAAQuality() {UpdateMSAASettings(); return MSAAQuality;}

	virtual void PushEvent(const TCHAR* Name);

	virtual void PopEvent();

private:

	/** Current perf event node frame. */
	FD3D11EventNodeFrame* CurrentEventNodeFrame;

	/** Current perf event node. */
	FD3D11EventNode* CurrentEventNode;

	/** Whether we are currently tracking perf events or not. */
	UBOOL bTrackingEvents;

	/** A latched version of GProfilingGPU. This is a form of pseudo-thread safety. We read the value once a frame only. */
	UBOOL bLatchedGProfilingGPU;

	/** A latched version of GProfilingGPUHitches. This is a form of pseudo-thread safety. We read the value once a frame only. */
	UBOOL bLatchedGProfilingGPUHitches;

	/** The previous latched version of GProfilingGPUHitches.*/
	UBOOL bPreviousLatchedGProfilingGPUHitches;

	/** Original state of GEmitDrawEvents before it was overridden for profiling. */
	UBOOL bOriginalGEmitDrawEvents;

	/** GPU hitch profile histories */
	TIndirectArray<FD3D11EventNodeFrame> GPUHitchEventNodeFrames;

	/** GPU hitch profile history debounce...after a hitch, we just ignore frames for a while */
	INT GPUHitchDebounce;

	/** The global D3D interface. */
	TRefCountPtr<IDXGIFactory> DXGIFactory;

	/** The global D3D device. */
	TRefCountPtr<ID3D11Device> Direct3DDevice;

	/** The global D3D device's immediate context */
	TRefCountPtr<ID3D11DeviceContext> Direct3DDeviceIMContext;

	/** A list of all viewport RHIs that have been created. */
	TArray<FD3D11Viewport*> Viewports;

	/** The viewport which is currently being drawn. */
	TRefCountPtr<FD3D11Viewport> DrawingViewport;

	/** True if the device being used has been removed. */
	UBOOL bDeviceRemoved;

	/** The width of the D3D device's back buffer. */
	UINT DeviceSizeX;

	/** The height of the D3D device's back buffer. */
	UINT DeviceSizeY;

	/** The window handle associated with the D3D device. */
	HWND DeviceWindow;

	/** The feature level of the device. */
	D3D_FEATURE_LEVEL FeatureLevel;

	/** True if the D3D device is in fullscreen mode. */
	UBOOL bIsFullscreenDevice;

	/** Active MSAA settings for surfces flagged as multi-sample */
	UINT CachedMaxMultisamples;
	UINT MSAACount;
	UINT MSAAQuality;

	/** True if the currently set render target is multisampled. */
	UBOOL bCurrentRenderTargetIsMultisample;

	/** An event used to track the GPU's progress. */
	FD3D11EventQuery FrameSyncEvent;

	/** Measure GPU time per frame. */
	FD3D11BufferedGPUTiming GPUFrameTiming;

	/** If a vertex stream with instance data is set, this tracks how many instances it contains. */
	UINT PendingNumInstances;

	// Tracks the currently set state blocks.
	D3D11_DEPTH_STENCIL_DESC CurrentDepthState;
	D3D11_DEPTH_STENCIL_DESC CurrentStencilState;
	D3D11_RASTERIZER_DESC CurrentRasterizerState;
	D3D11_BLEND_DESC CurrentBlendState;
	UINT CurrentStencilRef;
	UBOOL CurrentScissorEnable;
	FLinearColor CurrentBlendFactor;

	void *PendingDrawPrimitiveUPVertexData;
	UINT PendingNumVertices;
	UINT PendingVertexDataStride;
	void *StaticData;
	UINT StaticDataSize;

	void *PendingDrawPrimitiveUPIndexData;
	UINT PendingPrimitiveType;
	UINT PendingNumPrimitives;
	UINT PendingMinVertexIndex;
	UINT PendingIndexDataStride;

	TRefCountPtr<ID3D11RenderTargetView> CurrentRenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	TRefCountPtr<ID3D11DepthStencilView> CurrentDepthStencilTarget;
	TRefCountPtr<FD3D11Surface> CurrentDepthSurface;

	/** Tracks whether the currently set depth stencil target is read only. */
	UBOOL bCurrentDSTIsReadonly;

	TStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> CurrentColorWriteEnable;

	/** When a new shader is set, we discard all old constants set for the previous shader. */
	UBOOL bDiscardSharedConstants;

	/** Set to true when the current shading setup uses tessellation */
	UBOOL bUsingTessellation;

	// For now have 1 4mb buffer and fill it sequentially.
	enum { NumUserDataBuffers = 1 };
	enum { UserDataBufferSize = 4*1024*1024 };
	TRefCountPtr<ID3D11Buffer> DynamicVertexBufferArray[NumUserDataBuffers];
	TRefCountPtr<ID3D11Buffer> DynamicIndexBufferArray[NumUserDataBuffers];
	UINT CurrentDynamicVB;
	UINT CurrentDynamicIB;
	UINT CurrentVBOffset;
	UINT CurrentIBOffset;
    nv::stereo::UE3StereoD3D11* StereoUpdater;

	TMap<FSamplerKey,TRefCountPtr<ID3D11SamplerState> > CachedSamplers;
	TMap<FDepthStencilKey,TRefCountPtr<ID3D11DepthStencilState> > CachedDepthStencilStates;
	TMap<FRasterizerKey,TRefCountPtr<ID3D11RasterizerState> > CachedRasterizerStates;
	TMap<FBlendKey,TRefCountPtr<ID3D11BlendState> > CachedBlendStates;

	/** A list of all D3D constant buffers RHIs that have been created. */
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > VSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > HSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > DSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > PSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > GSConstantBuffers;
	TArray<TRefCountPtr<FD3D11ConstantBuffer> > CSConstantBuffers;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<1024> > BoundShaderStateHistory;

	void* CreateVertexDataBuffer(UINT Size);

	void ReleaseDynamicVBandIBBuffers();

	FD3D11Texture2D* CreateD3D11Texture2D(UINT SizeX,UINT SizeY,UINT SizeZ,UBOOL bTextureArray,UBOOL CubeTexture,BYTE Format,UINT NumMips,DWORD Flags);

	FD3D11Texture3D* CreateD3D11Texture3D(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,const BYTE* Data);

	/** Initializes the constant buffers.  Called once at RHI initialization time. */
	void InitConstantBuffers();

	void ReleaseCachedD3D11States();

	/**
	 * Returns an appropriate D3D11 buffer from an array of buffers.  It also makes sure the buffer is of the proper size.
	 * @param Count The number of objects in the buffer
	 * @param Stride The stride of each object
	 * @param Binding Which type of binding (VB or IB) this buffer will need
	 */
	ID3D11Buffer* EnsureBufferSize(UINT Count, UINT Stride, D3D11_BIND_FLAG Binding);

	/**
	 * Fills a D3D11 buffer with the input data.  Returns the D3D11 buffer with the data.
	 * @param Count The number of objects in the buffer
	 * @param Stride The stride of each object
	 * @param Data The actual buffer data
	 * @param Binding Which type of binding (VB or IB) this buffer will need
	 */
	ID3D11Buffer* FillD3D11Buffer(UINT Count, UINT Stride, const void* Data, D3D11_BIND_FLAG Binding, UINT& OutOffset);

	/**
	 * Gets
	 */
	ID3D11SamplerState* GetCachedSamplerState( FSamplerStateRHIParamRef SamplerState );

	ID3D11DepthStencilState* GetCachedDepthStencilState( const D3D11_DEPTH_STENCIL_DESC& DepthState, const D3D11_DEPTH_STENCIL_DESC& StencilState );

	ID3D11RasterizerState* GetCachedRasterizerState( const D3D11_RASTERIZER_DESC& RasterizerState, UBOOL bScissorEnabled, UBOOL bMultisampleEnable );

	ID3D11BlendState* GetCachedBlendState( const D3D11_BLEND_DESC& BlendState, const TStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>& EnabledStateValue );

	/** needs to be called before each draw call */
	void CommitNonComputeShaderConstants();

	/** needs to be called before each dispatch call */
	void CommitComputeShaderConstants();

	void UpdateMSAASettings();

	/**
	 * Cleanup the D3D device.
	 * This function must be called from the main game thread.
	 */
	void CleanupD3DDevice();

	template<typename TPixelShader>
	void ResolveSurfaceUsingShader(
		FD3D11Surface* SourceSurface,
		FD3D11Texture2D* DestTexture2D,
		ID3D11RenderTargetView* DestSurfaceRTV,
		ID3D11DepthStencilView* DestSurfaceDSV,
		const D3D11_TEXTURE2D_DESC& ResolveTargetDesc,
		const FResolveRect& SourceRect,
		const FResolveRect& DestRect,
		ID3D11DeviceContext* Direct3DDeviceContext,
		typename TPixelShader::FParameter PixelShaderParameter
		);
};

#endif
