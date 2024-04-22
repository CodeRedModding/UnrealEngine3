/*=============================================================================
	D3D9Drv.h: Public D3D RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_D3D9DRV
#define _INC_D3D9DRV

// D3D headers.
#if _WIN64
#pragma pack(push,16)
#else
#pragma pack(push,8)
#endif
#define D3D_OVERLOADS 1
// This depends on the environment variable DXSDK_DIR being setup by the DXSDK.
// All projects that use D3D must add DXSDK_DIR/Inc to their include paths
// All projects that link with D3D must add DXSDK_DIR/Lib/x(86|64) to their linker paths
#include <d3d9.h>
#include <d3dx9.h>
#undef DrawText
#pragma pack(pop)

#define NO_STEREO_D3D11 1
#include "ue3stereo.h"
#undef NO_STEREO_D3D11
/**
 * Make sure we are compiling against the DXSDK we are expecting to,
 * Which is the June 2010 DX SDK.
 */
const INT REQUIRED_D3DX_SDK_VERSION = 43;
checkAtCompileTime(D3DX_SDK_VERSION == REQUIRED_D3DX_SDK_VERSION, D3DX_SDK_VERSION_DoesNotMatchRequiredVersion);


/** This is a macro that casts a dynamically bound RHI reference to the appropriate D3D type. */
#define DYNAMIC_CAST_D3D9RESOURCE(Type,Name) \
	FD3D9##Type* Name = (FD3D9##Type*)Name##RHI;

// D3D RHI public headers.
#include "D3D9Util.h"
#include "D3D9State.h"
#include "D3D9Resources.h"
#include "D3D9RenderTarget.h"
#include "D3D9Viewport.h"
#include "D3D9MeshUtils.h"

/** A D3D event query resource. */
class FD3D9EventQuery : public FRenderResource
{
public:

	/** Initialization constructor. */
	FD3D9EventQuery(FD3D9DynamicRHI* InD3DRHI):
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
	FD3D9DynamicRHI* D3DRHI;
	TRefCountPtr<IDirect3DQuery9> Query;
};

/**
 * Helper class to measure GPU timings.
 * It uses a buffer of measurements to avoid unnecessary CPU/GPU synchronization.
 * When performing a measurement, it will get queued up for processing on the GPU.
 * When retrieving a measurement result, you'll get the result of the most recently processed measurement,
 * which could be from one or two frames ago.
 */
class FD3D9BufferedGPUTiming : public FRenderResource
{
public:
	/**
	 * Constructor.
	 *
	 * @param InD3DRHI			RHI interface
	 * @param InBufferSize		Number of buffered measurements
	 */
	FD3D9BufferedGPUTiming( FD3D9DynamicRHI* D3DRHI, INT BufferSize );

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
	QWORD	GetTiming();

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
	FD3D9DynamicRHI*				D3DRHI;
	/** Number of timestamps created in 'StartTimestamps' and 'EndTimestamps'. */
	INT								BufferSize;
	/** Current timing being measured on the CPU. */
	INT								CurrentTimestamp;
	/** Number of measurements in the buffers (0 - BufferSize). */
	INT								NumIssuedTimestamps;
	/** Timestamps for all StartTimings. */
	TRefCountPtr<IDirect3DQuery9>*	StartTimestamps;
	/** Timestamps for all EndTimings. */
	TRefCountPtr<IDirect3DQuery9>*	EndTimestamps;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	UBOOL							bIsTiming;

	/** Whether the static variables have been initialized. */
	static UBOOL					GAreGlobalsInitialized;
	/** Whether GPU timing measurements are supported by the driver. */
	static UBOOL					GIsSupported;
	/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
	static QWORD					GTimingFrequency;
};

/** Reuses vertex declarations that are identical. */
class FD3D9VertexDeclarationCache
{
public:

	/** Key used to map a set of vertex element definitions to an IDirect3DVertexDeclaration9 resource */
	class FKey
	{
	public:
		/** Initialization constructor. */
		FKey(const FVertexDeclarationElementList& InElements);

		/**
		* @return TRUE if the decls are the same
		* @param Other - instance to compare against
		*/
		UBOOL operator == (const FKey &Other) const;

		// Accessors.
		D3DVERTEXELEMENT9* GetVertexElements() 
		{ 
			return &VertexElements(0);
		}
		const D3DVERTEXELEMENT9* GetVertexElements() const
		{ 
			return &VertexElements(0);
		}

		/** @return hash value for this type */
		friend DWORD GetTypeHash(const FKey &Key)
		{
			return Key.Hash;
		}

	private:
		/** array of D3D vertex elements */
		TPreallocatedArray<D3DVERTEXELEMENT9,MaxVertexElementCount + 1> VertexElements;
		/** hash value based on vertex elements */
		DWORD Hash;
	};

	/** Initialization constructor. */
	FD3D9VertexDeclarationCache(FD3D9DynamicRHI* InD3DRHI):
		D3DRHI(InD3DRHI)
	{}

	/**
	 * Get a vertex declaration
	 * Tries to find the decl within the set. Creates a new one if it doesn't exist
	 * @param Declaration - key containing the vertex elements
	 * @return D3D vertex decl object
	 */
	FD3D9VertexDeclaration* GetVertexDeclaration(const FKey& Declaration);

private:

	FD3D9DynamicRHI* D3DRHI;

	/** maps key consisting of vertex elements to the D3D decl */
	TMap<FKey, TRefCountPtr<FD3D9VertexDeclaration> > VertexDeclarationMap;
};

/** Used to track whether a period was disjoint on the GPU, which means GPU timings are invalid. */
class FD3D9DisjointTimeStampQuery : public FRenderResource
{
public:
	FD3D9DisjointTimeStampQuery(class FD3D9DynamicRHI* InD3DRHI);

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

	TRefCountPtr<IDirect3DQuery9> DisjointQuery;

	FD3D9DynamicRHI* D3DRHI;
};

/** Stats for a single perf event node. */
class FD3D9EventNodeStats : public FRefCountedObject
{
public:

	FD3D9EventNodeStats() :
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
class FD3D9EventNode : public FD3D9EventNodeStats
{
public:

	FD3D9EventNode(const TCHAR* InName, FD3D9EventNode* InParent, class FD3D9DynamicRHI* InRHI) :
		FD3D9EventNodeStats(),
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

	~FD3D9EventNode()
	{
		Timing.ReleaseResource();
	}

	FString Name;

	FD3D9BufferedGPUTiming Timing;

	/** Pointer to parent node so we can walk up the tree on appEndDrawEvent. */
	FD3D9EventNode* Parent;

	/** Children perf event nodes. */
	TArray<TRefCountPtr<FD3D9EventNode> > Children;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FD3D9EventNodeFrame
{
public:

	FD3D9EventNodeFrame(class FD3D9DynamicRHI* InRHI) :
	  RootEventTiming(InRHI, 1),
		  DisjointQuery(InRHI)
	  {

		  RootEventTiming.InitResource();
		  DisjointQuery.InitResource();
	  }
	  ~FD3D9EventNodeFrame()
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
	  TArray<TRefCountPtr<FD3D9EventNode> > EventTree;

	  /** Timer tracking inclusive time spent in the root nodes. */
	  FD3D9BufferedGPUTiming RootEventTiming;

	  /** Disjoint query tracking whether the times reported by DumpEventTree are reliable. */
	  FD3D9DisjointTimeStampQuery DisjointQuery;

};

/** The interface which is implemented by the dynamically bound RHI. */
class FD3D9DynamicRHI : public FDynamicRHI
{
public:

	friend class FD3D9Viewport;

	/** Initialization constructor. */
	FD3D9DynamicRHI();

	/** Destructor. */
	~FD3D9DynamicRHI();

	/** Reinitializes the D3D device upon a viewport change. */
	void UpdateD3DDeviceFromViewports();

	/**
	 * Reads a D3D query's data into the provided buffer.
	 * @param Query - The D3D query to read data from.
	 * @param Data - The buffer to read the data into.
	 * @param DataSize - The size of the buffer.
	 * @param bWait - If TRUE, it will wait for the query to finish.
	 * @return TRUE if the query finished.
	 */
	UBOOL GetQueryData(IDirect3DQuery9* Query,void* Data,SIZE_T DataSize,UBOOL bWait);

	// The RHI methods are defined as virtual functions in URenderHardwareInterface.
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type Name ParameterTypesAndNames
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

	// Reference counting API for the different resource types.
	#define IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE(Type,ParentType) \
		virtual void AddResourceRef(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_D3D9RESOURCE(Type,Reference); \
			Reference->AddRef(); \
		} \
		virtual void RemoveResourceRef(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_D3D9RESOURCE(Type,Reference); \
			Reference->Release(); \
		} \
		virtual DWORD GetRefCount(TDynamicRHIResource<RRT_##Type>* ReferenceRHI) \
		{ \
			DYNAMIC_CAST_D3D9RESOURCE(Type,Reference); \
			Reference->AddRef(); \
			return Reference->Release(); \
		}

	ENUM_RHI_RESOURCE_TYPES(IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE);

	#undef IMPLEMENT_DYNAMICRHI_REFCOUNTING_FORTYPE

	// Accessors.
	IDirect3DDevice9* GetDevice() const
	{
		return Direct3DDevice;
	}

	virtual void PushEvent(const TCHAR* Name);

	virtual void PopEvent();

private:

	/** Current perf event node frame. */
	FD3D9EventNodeFrame* CurrentEventNodeFrame;

	/** Current perf event node. */
	FD3D9EventNode* CurrentEventNode;

	/** Whether we are currently tracking perf events or not. */
	UBOOL bTrackingEvents;

	/** A latched version of GProfilingGPU. This is a form of pseudo-thread safety. We read the value once a frame only. */
	UBOOL bLatchedGProfilingGPU;

	/** Original state of GEmitDrawEvents before it was overridden for profiling. */
	UBOOL bOriginalGEmitDrawEvents;

	/** The number of bytes in each shader register. */
	static const UINT NumBytesPerShaderRegister = sizeof(FLOAT) * 4;

	/** These are cached so that we can call Direct3D->CheckDeviceMultiSampleType at any point. */
	UINT AdapterIndex;
	D3DDEVTYPE DeviceType;

	/** The global D3D interface. */
	TRefCountPtr<IDirect3D9> Direct3D;

	/** The global D3D device. */
	TRefCountPtr<IDirect3DDevice9> Direct3DDevice;

	/** The global D3D device's back buffer. */
	TRefCountPtr<FD3D9Surface> BackBuffer;

	/** A list of all viewport RHIs that have been created. */
	TArray<FD3D9Viewport*> Viewports;

	/** The viewport which is currently being drawn. */
	TRefCountPtr<FD3D9Viewport> DrawingViewport;

	/** True if the application has lost D3D device access. */
	UBOOL bDeviceLost;

	/** The width of the D3D device's back buffer. */
	UINT DeviceSizeX;

	/** The height of the D3D device's back buffer. */
	UINT DeviceSizeY;

	/** The window handle associated with the D3D device. */
	HWND DeviceWindow;

	/** True if the D3D device is in fullscreen mode. */
	UBOOL bIsFullscreenDevice;

	/** The capabilities of the D3D device. */
	D3DCAPS9 DeviceCaps;

	/** The number of active vertex streams. */
	INT MaxActiveVertexStreamIndex;

	/** The largest viewport size the application is expecting to need for the time being, or zero for
		no preference.  This is used as a hint to the RHI to reduce redundant device resets when viewports
		are created or destroyed (typically in the editor.)  These values can change at any point, but
		they're usually configured before a viewport is created or destroyed. */
	UINT LargestExpectedViewportWidth;
	UINT LargestExpectedViewportHeight;

	/** Indicates support for Nvidia's Depth Bounds Test through a driver hack in D3D. */
	UBOOL bDepthBoundsHackSupported;

	/** An event used to track the GPU's progress. */
	FD3D9EventQuery FrameSyncEvent;

	/** Measure GPU time per frame. */
	FD3D9BufferedGPUTiming GPUFrameTiming;

	/** The vertex declarations that have been created for this device. */
	FD3D9VertexDeclarationCache VertexDeclarationCache;

	/** Information about a vertex stream that's been bound to the D3D device. */
	struct FD3D9Stream
	{
		FD3D9Stream()
		:	Stride(0)
        ,   Offset(0)
		{}
		FVertexBufferRHIParamRef VertexBuffer;
		UINT Stride;
        UINT Offset;
		UINT NumVerticesPerInstance;
	};

	enum { NumVertexStreams = 16 };

	UINT PendingNumInstances;
	UINT UpdateStreamForInstancingMask;
	FD3D9Stream PendingStreams[NumVertexStreams];

	// Information about pending BeginDraw[Indexed]PrimitiveUP calls.
	UBOOL PendingBegunDrawPrimitiveUP;
	TArray<BYTE> PendingDrawPrimitiveUPVertexData;
	UINT PendingNumVertices;
	UINT PendingVertexDataStride;
	TArray<BYTE> PendingDrawPrimitiveUPIndexData;
	UINT PendingPrimitiveType;
	UINT PendingNumPrimitives;
	UINT PendingMinVertexIndex;
	UINT PendingIndexDataStride;

    /** Updates the stereo texture with valid data */
    nv::stereo::UE3StereoD3D9* StereoUpdater;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<1024> > BoundShaderStateHistory;

	/**
	 * Cleanup the D3D device.
	 * This function must be called from the main game thread.
	 */
	void CleanupD3DDevice();
	
	/** Resets the active vertex streams. */
	void ResetVertexStreams();

	/** Reset all pixel shader texture references, to ensure a reference to this render target doesn't remain set. */
	void UnsetPSTextures();

	/** Reset all vertex shader texture references, to ensure a reference to this render target doesn't remain set. */
	void UnsetVSTextures();
};

#endif

