/*=============================================================================
	D3D11Device.cpp: D3D device RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"
#include <delayimp.h>

#include "nvapi.h"

/** In bytes. 0 means unlimited. */
extern INT GTexturePoolSize;
/** Whether to read the texture pool size from engine.ini on PC. Can be turned on with -UseTexturePool on the command line. */
extern UBOOL GReadTexturePoolSizeFromIni;

/** This function is used as a SEH filter to catch only delay load exceptions. */
static UBOOL IsDelayLoadException(PEXCEPTION_POINTERS ExceptionPointers)
{
	switch(ExceptionPointers->ExceptionRecord->ExceptionCode)
	{
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
		return EXCEPTION_EXECUTE_HANDLER;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

/**
 * Since CreateDXGIFactory is a delay loaded import from the D3D11 DLL, if the user
 * doesn't have Vista/DX10, calling CreateDXGIFactory will throw an exception.
 * We use SEH to detect that case and fail gracefully.
 */
static void SafeCreateDXGIFactory(IDXGIFactory** DXGIFactory)
{
	__try
	{
		CreateDXGIFactory(__uuidof(IDXGIFactory),(void**)DXGIFactory);
	}
	__except(IsDelayLoadException(GetExceptionInformation()))
	{
	}
}

static UBOOL SafeTestD3D11CreateDevice(UINT AdapterIndex,IDXGIAdapter* Adapter,D3D_FEATURE_LEVEL& OutFeatureLevel)
{
	ID3D11Device* D3DDevice = NULL;
	ID3D11DeviceContext* D3DDeviceContext = NULL;
	UINT DeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;

	// Use a debug device if specified on the command line.
	if(ParseParam(appCmdLine(),TEXT("d3ddebug")))
	{
		DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}

	DXGI_ADAPTER_DESC Desc;
	Adapter->GetDesc(&Desc);

	D3D_FEATURE_LEVEL RequestedFeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0
	};

	__try
	{
		if(SUCCEEDED(D3D11CreateDevice(
			Adapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			NULL,
			DeviceFlags,
			RequestedFeatureLevels,
			ARRAY_COUNT(RequestedFeatureLevels),
			D3D11_SDK_VERSION,
			&D3DDevice,
			&OutFeatureLevel,
			&D3DDeviceContext
			)))
		{
			// Log some information about the available D3D11 adapters.
			DXGI_ADAPTER_DESC AdapterDesc;
			VERIFYD3D11RESULT(Adapter->GetDesc(&AdapterDesc));

			debugf(TEXT("Found D3D11 adapter %u: %s"),AdapterIndex,AdapterDesc.Description);
			debugf(
				TEXT("Adapter has %uMB of dedicated video memory, %uMB of dedicated system memory, and %uMB of shared system memory"),
				AdapterDesc.DedicatedVideoMemory / (1024*1024),
				AdapterDesc.DedicatedSystemMemory / (1024*1024),
				AdapterDesc.SharedSystemMemory / (1024*1024)
				);

			D3DDevice->Release();
			D3DDeviceContext->Release();

			return TRUE;
		}
	}
	__except(IsDelayLoadException(GetExceptionInformation()))
	{
	}

	return FALSE;
}

/** @return TRUE if Direct3D 11 is supported by the host. */
UBOOL IsDirect3D11Supported(UBOOL& OutSupportsD3D11Features)
{
	// Try to create the DXGIFactory.  This will fail if we're not running Vista.
	TRefCountPtr<IDXGIFactory> DXGIFactory;
	SafeCreateDXGIFactory(DXGIFactory.GetInitReference());
	if(!DXGIFactory)
	{
		return FALSE;
	}

	// Enumerate the DXGIFactory's adapters.
	UINT AdapterIndex = 0;
	TRefCountPtr<IDXGIAdapter> TempAdapter;
	UBOOL bHasD3D11Adapter = FALSE;
	while(DXGIFactory->EnumAdapters(AdapterIndex,TempAdapter.GetInitReference()) != DXGI_ERROR_NOT_FOUND)
	{
		// Check that if adapter supports D3D11.
		if(TempAdapter)
		{
			D3D_FEATURE_LEVEL MaxFeatureLevel = D3D_FEATURE_LEVEL_11_0;
			if(SafeTestD3D11CreateDevice(AdapterIndex,TempAdapter,MaxFeatureLevel))
			{
				OutSupportsD3D11Features = (MaxFeatureLevel == D3D_FEATURE_LEVEL_11_0);
				bHasD3D11Adapter = TRUE;
			}
		}

		AdapterIndex++;
	};

	return bHasD3D11Adapter;
}

/**
 * Called at startup to initialize the D3D11 RHI.  This assumes that the caller has already checked that IsDirect3D11Supported is TRUE.
 * @return The D3D11 RHI
 */
FDynamicRHI* D3D11CreateRHI()
{
	TRefCountPtr<IDXGIFactory> DXGIFactory;
	SafeCreateDXGIFactory(DXGIFactory.GetInitReference());
	check(DXGIFactory);
	return new FD3D11DynamicRHI(DXGIFactory,D3D_FEATURE_LEVEL_11_0);
}

FD3D11DynamicRHI::FD3D11DynamicRHI(IDXGIFactory* InDXGIFactory,D3D_FEATURE_LEVEL InFeatureLevel):
	DXGIFactory(InDXGIFactory),
	bDeviceRemoved(FALSE),
	DeviceSizeX(0),
	DeviceSizeY(0),
	DeviceWindow(NULL),
	FeatureLevel(InFeatureLevel),
	bIsFullscreenDevice(FALSE),
	CachedMaxMultisamples(0),
	bCurrentRenderTargetIsMultisample(FALSE),
	FrameSyncEvent(this),
	GPUFrameTiming(this, 4),
	PendingNumInstances(0),
	CurrentDepthState(),
	CurrentStencilState(),
	CurrentRasterizerState(),
	CurrentBlendState(),
	CurrentStencilRef(0),
	CurrentScissorEnable(FALSE),
	CurrentBlendFactor(0,0,0,0),
	PendingDrawPrimitiveUPVertexData(NULL),
	PendingNumVertices(0),
	PendingVertexDataStride(0),
	StaticData(NULL),
	StaticDataSize(0),
	PendingDrawPrimitiveUPIndexData(NULL),
	PendingPrimitiveType(0),
	PendingNumPrimitives(0),
	PendingMinVertexIndex(0),
	PendingIndexDataStride(0),
	CurrentDepthSurface(NULL),
	bCurrentDSTIsReadonly(FALSE),
	bDiscardSharedConstants(FALSE),
	bUsingTessellation(FALSE),
	CurrentDynamicVB(0),
	CurrentDynamicIB(0),
	CurrentVBOffset(0),
	CurrentIBOffset(0),
	StereoUpdater(NULL)
{
	CurrentEventNodeFrame= NULL;
	CurrentEventNode = NULL;
	bTrackingEvents = FALSE;
	bLatchedGProfilingGPU = FALSE;
	bLatchedGProfilingGPUHitches = FALSE;
	bPreviousLatchedGProfilingGPUHitches = FALSE;
	bOriginalGEmitDrawEvents = FALSE;
	GPUHitchDebounce=0;

	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );

	// @TODO verify stereo support
	StereoUpdater = new nv::stereo::UE3StereoD3D11(true);

#if UE3_LEAN_AND_MEAN
	appErrorf(TEXT("DX11 not supported in UE3_LEAN_AND_MEAN config."));
#else
	// ensure we are running against the proper D3DX runtime
	if (FAILED(D3DX11CheckVersion(D3D11_SDK_VERSION, D3DX11_SDK_VERSION)))
	{
		appErrorf(
			NAME_FriendlyError,
			TEXT("The D3DX11 runtime version does not match what the application was built with (%d). Cannot continue."),
			D3DX11_SDK_VERSION
			);
	}
#endif

	GTexturePoolSize = 0;
	if ( GReadTexturePoolSizeFromIni )
	{
		GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSize"), GTexturePoolSize, GEngineIni);
		GTexturePoolSize *= 1024*1024;
	}

	// Initialize the RHI capabilities.
	check(FeatureLevel == D3D_FEATURE_LEVEL_11_0);
	GRHIShaderPlatform = SP_PCD3D_SM5;

	GPixelCenterOffset = 0.0f;	// Note that in D3D11, there is no half-texel offset (ala DX9)	
	GSupportsVertexInstancing = TRUE;
	GSupportsDepthTextures = TRUE;
	GSupportsHardwarePCF = TRUE;
	GSupportsFetch4 = FALSE;
	GSupportsFPFiltering = TRUE;
	GSupportsVertexTextureFetch = TRUE;

	// Initialize the platform pixel format map.
	GPixelFormats[ PF_Unknown		].PlatformFormat	= DXGI_FORMAT_UNKNOWN;
	GPixelFormats[ PF_A32B32G32R32F	].PlatformFormat	= DXGI_FORMAT_R32G32B32A32_FLOAT;
	GPixelFormats[ PF_A8R8G8B8		].PlatformFormat	= DXGI_FORMAT_R8G8B8A8_UNORM;
	GPixelFormats[ PF_G8			].PlatformFormat	= DXGI_FORMAT_R8_UNORM;
	GPixelFormats[ PF_G16			].PlatformFormat	= DXGI_FORMAT_UNKNOWN;	// Not supported for rendering.
	GPixelFormats[ PF_DXT1			].PlatformFormat	= DXGI_FORMAT_BC1_UNORM;
	GPixelFormats[ PF_DXT3			].PlatformFormat	= DXGI_FORMAT_BC2_UNORM;
	GPixelFormats[ PF_DXT5			].PlatformFormat	= DXGI_FORMAT_BC3_UNORM;
	GPixelFormats[ PF_UYVY			].PlatformFormat	= DXGI_FORMAT_UNKNOWN;		// TODO: Not supported in D3D11
	// Using a typeless format for depth so that it can be bound as a depth stencil view (using DXGI_FORMAT_D24_UNORM_S8_UINT) 
	// Or as a shader resource view (using DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
	GPixelFormats[ PF_DepthStencil	].PlatformFormat	= DXGI_FORMAT_R24G8_TYPELESS;
	GPixelFormats[ PF_DepthStencil	].BlockBytes		= 4;
	GPixelFormats[ PF_ShadowDepth	].PlatformFormat	= DXGI_FORMAT_R24G8_TYPELESS;
	GPixelFormats[ PF_FilteredShadowDepth ].PlatformFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;//DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	GPixelFormats[ PF_R32F			].PlatformFormat	= DXGI_FORMAT_R32_FLOAT;
	GPixelFormats[ PF_G16R16		].PlatformFormat	= DXGI_FORMAT_R16G16_UNORM;
	GPixelFormats[ PF_G16R16F		].PlatformFormat	= DXGI_FORMAT_R16G16_FLOAT;
	GPixelFormats[ PF_G16R16F_FILTER].PlatformFormat	= DXGI_FORMAT_R16G16_FLOAT;
	GPixelFormats[ PF_G32R32F		].PlatformFormat	= DXGI_FORMAT_R32G32_FLOAT;
	GPixelFormats[ PF_A2B10G10R10   ].PlatformFormat    = DXGI_FORMAT_R10G10B10A2_UNORM;
	GPixelFormats[ PF_A16B16G16R16  ].PlatformFormat    = DXGI_FORMAT_R16G16B16A16_UNORM;
	GPixelFormats[ PF_D24 ].PlatformFormat				= DXGI_FORMAT_UNKNOWN;//(D3DFORMAT)(MAKEFOURCC('D','F','2','4'));
	GPixelFormats[ PF_R16F			].PlatformFormat	= DXGI_FORMAT_R16_FLOAT;
	GPixelFormats[ PF_R16F_FILTER	].PlatformFormat	= DXGI_FORMAT_R16_FLOAT;

	GPixelFormats[ PF_FloatRGB	].PlatformFormat		= DXGI_FORMAT_R16G16B16A16_FLOAT;
	GPixelFormats[ PF_FloatRGB	].BlockBytes			= 8;
	GPixelFormats[ PF_FloatRGBA	].PlatformFormat		= DXGI_FORMAT_R16G16B16A16_FLOAT;
	GPixelFormats[ PF_FloatRGBA	].BlockBytes			= 8;

	GPixelFormats[ PF_FloatR11G11B10].PlatformFormat	= DXGI_FORMAT_R11G11B10_FLOAT;
	GPixelFormats[ PF_FloatR11G11B10].BlockBytes		= 4;

	GPixelFormats[ PF_V8U8			].PlatformFormat	= DXGI_FORMAT_R8G8_SNORM;
	GPixelFormats[ PF_BC5			].PlatformFormat	= DXGI_FORMAT_BC5_UNORM;
	GPixelFormats[ PF_A1			].PlatformFormat	= DXGI_FORMAT_R1_UNORM; // Not supported for rendering.

	const INT MaxTextureDims = 8128;
	GMaxTextureMipCount = appCeilLogTwo( MaxTextureDims ) + 1;
	GMaxTextureMipCount = Min<INT>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
	GMaxWholeSceneDominantShadowDepthBufferSize = 4096;
	GMaxPerObjectShadowDepthBufferSizeX = 4096;
	GMaxPerObjectShadowDepthBufferSizeY = 4096;

	// Initialize the frame query event.
	FrameSyncEvent.InitResource();

	// Initialize Buffered timestamp queries 
	GPUFrameTiming.InitResource();

	// Initialize the constant buffers.
	InitConstantBuffers();
	
	// Initialize the cached state.
	CurrentColorWriteEnable = MakeUniformStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>(0);
	CurrentColorWriteEnable[0] = D3D11_COLOR_WRITE_ENABLE_ALL;
}

FD3D11DynamicRHI::~FD3D11DynamicRHI()
{
	check(IsInGameThread() && IsInRenderingThread());

	// Cleanup the D3D device.
	CleanupD3DDevice();

	// Delink the frame sync event from the resource list.
	FrameSyncEvent.ReleaseResource();

	// Release buffered timestamp queries
	GPUFrameTiming.ReleaseResource();
}

void FD3D11DynamicRHI::InitD3DDevice()
{
	check( IsInGameThread() );

	// Wait for the rendering thread to go idle.
	SCOPED_SUSPEND_RENDERING_THREAD(FSuspendRenderingThread::ST_RecreateThread);

	// If the device we were using has been removed, release it and the resources we created for it.
	if(bDeviceRemoved)
	{
		bDeviceRemoved = FALSE;

		// Cleanup the D3D device.
		CleanupD3DDevice();

		// We currently don't support removed devices because FTexture2DResource can't recreate its RHI resources from scratch.
		// We would also need to recreate the viewport swap chains from scratch.
		appErrorf(TEXT("The Direct3D 11 device that was being used has been removed.  Please restart the game."));
	}

	// If we don't have a device yet, either because this is the first viewport, or the old device was removed, create a device.
	if(!Direct3DDevice)
	{
		check(!GIsRHIInitialized);

		// Determine the adapter and device type to use.
		TRefCountPtr<IDXGIAdapter> Adapter;
		
		// In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
		//		DriverType must be D3D_DRIVER_TYPE_UNKNOWN 
		//		Software must be NULL. 
		D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;	

		UINT DeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;

		// Use a debug device if specified on the command line.
		if(ParseParam(appCmdLine(),TEXT("d3ddebug")))
		{
			DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		}

		// Allow selection of NVPerfHUD Adapter (if available)
		TRefCountPtr<IDXGIAdapter> EnumAdapter;
		UINT CurrentAdapter = 0;
		while (DXGIFactory->EnumAdapters(CurrentAdapter,EnumAdapter.GetInitReference()) != DXGI_ERROR_NOT_FOUND)
		{
			if (EnumAdapter)// && EnumAdapter->CheckInterfaceSupport(__uuidof(ID3D11Device),NULL) == S_OK)
			{
				DXGI_ADAPTER_DESC AdapterDesc;
				if (SUCCEEDED(EnumAdapter->GetDesc(&AdapterDesc)))
				{
#if UDK
					// Don't allow NvPerfHUD profiling in the UDK build
					const UBOOL bIsPerfHUD = FALSE;
#else
					const UBOOL bIsPerfHUD = !appStricmp(AdapterDesc.Description,TEXT("NVIDIA PerfHUD"));
#endif

					// Select the first adapter in normal circumstances or the PerfHUD one if it exists.
					if(CurrentAdapter == 0 || bIsPerfHUD)
					{
						Adapter = EnumAdapter;
					}
					if(bIsPerfHUD)
					{
						DriverType =  D3D_DRIVER_TYPE_REFERENCE;
					}
				}
			}
			++CurrentAdapter;
		}

		D3D_FEATURE_LEVEL MaxFeatureLevel = D3D_FEATURE_LEVEL_11_0;

		// Creating the Direct3D device.
		VERIFYD3D11RESULT(D3D11CreateDevice(
			Adapter,
			DriverType,
			NULL,
			DeviceFlags,
			&FeatureLevel,
			1,
			D3D11_SDK_VERSION,
			Direct3DDevice.GetInitReference(),
			&MaxFeatureLevel,
			Direct3DDeviceIMContext.GetInitReference()
			));

#if UDK && WIN32 && !_WIN64
		// Disable PIX for windows in the UDK build
		D3DPERF_SetOptions(1);
#endif

	// Determine PF_G8 usage as a render target
		UINT G8FormatSupport = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CheckFormatSupport(DXGI_FORMAT_R8_UNORM,&G8FormatSupport));
		GSupportsRenderTargetFormat_PF_G8 = G8FormatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET;

		// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->InitDynamicRHI();
		}
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->InitRHI();
		}

#if !SHIPPING_PC_GAME
		// Add some filter outs for known debug spew messages (that we don't care about)
		if(DeviceFlags & D3D11_CREATE_DEVICE_DEBUG)
		{
			ID3D11InfoQueue *pd3dInfoQueue = NULL;
			VERIFYD3D11RESULT(Direct3DDevice->QueryInterface( IID_ID3D11InfoQueue, (void**)&pd3dInfoQueue));
			if(pd3dInfoQueue)
			{
				D3D11_INFO_QUEUE_FILTER NewFilter;
				appMemzero(&NewFilter,sizeof(NewFilter));

				// Turn off info msgs as these get really spewy
				D3D11_MESSAGE_SEVERITY DenySeverity = D3D11_MESSAGE_SEVERITY_INFO;
				NewFilter.DenyList.NumSeverities = 1;
				NewFilter.DenyList.pSeverityList = &DenySeverity;

				D3D11_MESSAGE_ID DenyIds[]  = {
					// OMSETRENDERTARGETS_INVALIDVIEW - d3d will complain if depth and color targets don't have the exact same dimensions, but actually
					//	if the color target is smaller then things are ok.  So turn off this error.  There is a manual check in FD3D11DynamicRHI::SetRenderTarget
					//	that tests for depth smaller than color and MSAA settings to match.
					D3D11_MESSAGE_ID_OMSETRENDERTARGETS_INVALIDVIEW, 

					// QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS - The RHI exposes the interface to make and issue queries and a separate interface to use that data.
					//		Currently there is a situation where queries are issued and the results may be ignored on purpose.  Filtering out this message so it doesn't
					//		swarm the debug spew and mask other important warnings
					D3D11_MESSAGE_ID_QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS 
				
				};

				NewFilter.DenyList.NumIDs = sizeof(DenyIds)/sizeof(D3D11_MESSAGE_ID);
				NewFilter.DenyList.pIDList = (D3D11_MESSAGE_ID*)&DenyIds;

				pd3dInfoQueue->PushStorageFilter(&NewFilter);

				pd3dInfoQueue->Release();
			}
		}
#endif

		// Update SLI state
		NV_GET_CURRENT_SLI_STATE SLIState;
		SLIState.version = NV_GET_CURRENT_SLI_STATE_VER;	// need to set the version or the call will fail.
		NvAPI_Status Status = NvAPI_D3D_GetCurrentSLIState(Direct3DDevice, &SLIState);
		if (Status == NVAPI_OK)
		{
			GNumActiveGPUsForRendering = SLIState.numAFRGroups;
			warnf(TEXT("Detected %u GPUs for rendering"), GNumActiveGPUsForRendering);
		}

		UpdateMSAASettings();

		// Set the RHI initialized flag.
		GIsRHIInitialized = TRUE;
	}
}

/**
 * Returns a supported screen resolution that most closely matches the input.
 * @param Width - Input: Desired resolution width in pixels. Output: A width that the platform supports.
 * @param Height - Input: Desired resolution height in pixels. Output: A height that the platform supports.
 */
void FD3D11DynamicRHI::GetSupportedResolution( UINT &Width, UINT &Height )
{
	UINT InitializedMode = FALSE;
	DXGI_MODE_DESC BestMode;
	BestMode.Width = 0;
	BestMode.Height = 0;

	// Enumerate all DXGI adapters
	// TODO: Cap at 1 for default adapter
	for(UINT i = 0;i < 1;i++)
    {
		HRESULT hr = S_OK;
        TRefCountPtr<IDXGIAdapter> Adapter;
        hr = DXGIFactory->EnumAdapters(i,Adapter.GetInitReference());
        if( DXGI_ERROR_NOT_FOUND == hr )
        {
            hr = S_OK;
            break;
        }
        if( FAILED(hr) )
        {
            return;
        }

        // get the description of the adapter
        DXGI_ADAPTER_DESC AdapterDesc;
        VERIFYD3D11RESULT(Adapter->GetDesc(&AdapterDesc));
      
        // Enumerate outputs for this adapter
		// TODO: Cap at 1 for default output
		for(UINT o = 0;o < 1; o++)
		{
			TRefCountPtr<IDXGIOutput> Output;
			hr = Adapter->EnumOutputs(o,Output.GetInitReference());
			if(DXGI_ERROR_NOT_FOUND == hr)
				break;
			if(FAILED(hr))
				return;

			// TODO: GetDisplayModeList is a terribly SLOW call.  It can take up to a second per invocation.
			//  We might want to work around some DXGI badness here.
			DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			UINT NumModes = 0;
			hr = Output->GetDisplayModeList(Format,0,&NumModes,NULL);
			if(hr == DXGI_ERROR_NOT_FOUND)
			{
				return;
			}
			else if(hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
			{
				appErrorf(
					NAME_FriendlyError,
					TEXT("This application cannot be run over a remote desktop configuration")
					);
				return;
			}
			DXGI_MODE_DESC* ModeList = new DXGI_MODE_DESC[ NumModes ];
			VERIFYD3D11RESULT(Output->GetDisplayModeList(Format,0,&NumModes,ModeList));

			for(UINT m = 0;m < NumModes;m++)
			{
				// Search for the best mode
				UBOOL IsEqualOrBetterWidth = Abs((INT)ModeList[m].Width - (INT)Width) <= Abs((INT)BestMode.Width - (INT)Width);
				UBOOL IsEqualOrBetterHeight = Abs((INT)ModeList[m].Height - (INT)Height) <= Abs((INT)BestMode.Height - (INT)Height);
				if(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
				{
					BestMode = ModeList[m];
					InitializedMode = TRUE;
				}
			}

			delete[] ModeList;
		}
    }

	check(InitializedMode);
	Width = BestMode.Width;
	Height = BestMode.Height;
}

/**
 *	Retrieve available screen resolutions.
 *
 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
 *	@param	bIgnoreRefreshRate	If TRUE, ignore refresh rates.
 *
 *	@return	UBOOL				TRUE if successfully filled the array
 */
UBOOL FD3D11DynamicRHI::GetAvailableResolutions(FScreenResolutionArray& Resolutions, UBOOL bIgnoreRefreshRate)
{
	INT MinAllowableResolutionX = 0;
	INT MinAllowableResolutionY = 0;
	INT MaxAllowableResolutionX = 10480;
	INT MaxAllowableResolutionY = 10480;
	INT MinAllowableRefreshRate = 0;
	INT MaxAllowableRefreshRate = 10480;

	GConfig->GetInt(TEXT("WinDrv.WindowsClient"), TEXT("MinAllowableResolutionX"), MinAllowableResolutionX, GEngineIni);
	GConfig->GetInt(TEXT("WinDrv.WindowsClient"), TEXT("MinAllowableResolutionY"), MinAllowableResolutionY, GEngineIni);
	GConfig->GetInt(TEXT("WinDrv.WindowsClient"), TEXT("MaxAllowableResolutionX"), MaxAllowableResolutionX, GEngineIni);
	GConfig->GetInt(TEXT("WinDrv.WindowsClient"), TEXT("MaxAllowableResolutionY"), MaxAllowableResolutionY, GEngineIni);
	GConfig->GetInt(TEXT("WinDrv.WindowsClient"), TEXT("MinAllowableRefreshRate"), MinAllowableRefreshRate, GEngineIni);
	GConfig->GetInt(TEXT("WinDrv.WindowsClient"), TEXT("MaxAllowableRefreshRate"), MaxAllowableRefreshRate, GEngineIni);

	if (MaxAllowableResolutionX == 0)
	{
		MaxAllowableResolutionX = 10480;
	}
	if (MaxAllowableResolutionY == 0)
	{
		MaxAllowableResolutionY = 10480;
	}
	if (MaxAllowableRefreshRate == 0)
	{
		MaxAllowableRefreshRate = 10480;
	}

	// Check the default adapter only.
	INT CurrentAdapter = 0;
	HRESULT hr = S_OK;
	TRefCountPtr<IDXGIAdapter> Adapter;
	hr = DXGIFactory->EnumAdapters(CurrentAdapter,Adapter.GetInitReference());

	if( DXGI_ERROR_NOT_FOUND == hr )
		return FALSE;
	if( FAILED(hr) )
		return FALSE;

	// get the description of the adapter
	DXGI_ADAPTER_DESC AdapterDesc;
	VERIFYD3D11RESULT(Adapter->GetDesc(&AdapterDesc));

	INT CurrentOutput = 0;
	do 
	{
		TRefCountPtr<IDXGIOutput> Output;
		hr = Adapter->EnumOutputs(CurrentOutput,Output.GetInitReference());
		if(DXGI_ERROR_NOT_FOUND == hr)
			break;
		if(FAILED(hr))
			return FALSE;

		// TODO: GetDisplayModeList is a terribly SLOW call.  It can take up to a second per invocation.
		//  We might want to work around some DXGI badness here.
		DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT NumModes = 0;
		hr = Output->GetDisplayModeList(Format, 0, &NumModes, NULL);
		if(hr == DXGI_ERROR_NOT_FOUND)
		{
			continue;
		}
		else if(hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
		{
			appErrorf(
				NAME_FriendlyError,
				TEXT("This application cannot be run over a remote desktop configuration")
				);
			return FALSE;
		}

		DXGI_MODE_DESC* ModeList = new DXGI_MODE_DESC[ NumModes ];
		VERIFYD3D11RESULT(Output->GetDisplayModeList(Format, 0, &NumModes, ModeList));

		for(UINT m = 0;m < NumModes;m++)
		{
			if (((INT)ModeList[m].Width >= MinAllowableResolutionX) &&
				((INT)ModeList[m].Width <= MaxAllowableResolutionX) &&
				((INT)ModeList[m].Height >= MinAllowableResolutionY) &&
				((INT)ModeList[m].Height <= MaxAllowableResolutionY)
				)
			{
				UBOOL bAddIt = TRUE;
				if (bIgnoreRefreshRate == FALSE)
				{
					if (((INT)ModeList[m].RefreshRate.Numerator < MinAllowableRefreshRate * ModeList[m].RefreshRate.Denominator) ||
						((INT)ModeList[m].RefreshRate.Numerator > MaxAllowableRefreshRate * ModeList[m].RefreshRate.Denominator)
						)
					{
						continue;
					}
				}
				else
				{
					// See if it is in the list already
					for (INT CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
					{
						FScreenResolutionRHI& CheckResolution = Resolutions(CheckIndex);
						if ((CheckResolution.Width == ModeList[m].Width) &&
							(CheckResolution.Height == ModeList[m].Height))
						{
							// Already in the list...
							bAddIt = FALSE;
							break;
						}
					}
				}

				if (bAddIt)
				{
					// Add the mode to the list
					INT Temp2Index = Resolutions.AddZeroed();
					FScreenResolutionRHI& ScreenResolution = Resolutions(Temp2Index);

					ScreenResolution.Width = ModeList[m].Width;
					ScreenResolution.Height = ModeList[m].Height;
					ScreenResolution.RefreshRate = ModeList[m].RefreshRate.Numerator / ModeList[m].RefreshRate.Denominator;
				}
			}
		}

		delete[] ModeList;

		++CurrentOutput;

	// TODO: Cap at 1 for default output
	} while(CurrentOutput < 1);

	return TRUE;
}

void FD3D11DynamicRHI::ReleaseCachedD3D11States()
{
	CachedSamplers.Empty();
	CachedDepthStencilStates.Empty();
	CachedRasterizerStates.Empty();
	CachedBlendStates.Empty();

	// Null out the temporaries that we were using to keep track of current state
	CurrentDepthState = D3D11_DEPTH_STENCIL_DESC();
	CurrentStencilState = D3D11_DEPTH_STENCIL_DESC();
	CurrentRasterizerState = D3D11_RASTERIZER_DESC();
	CurrentBlendState = D3D11_BLEND_DESC();
	CurrentBlendFactor = FLinearColor(0,0,0,0);
	CurrentStencilRef = 0;
}

/**
 * Gets
 */
ID3D11SamplerState* FD3D11DynamicRHI::GetCachedSamplerState( FSamplerStateRHIParamRef SamplerStateRHI )
{
	DYNAMIC_CAST_D3D11RESOURCE(SamplerState,SamplerState);

	FSamplerKey SamplerKey(SamplerState);
#if !LET_D3D11_CACHE_STATE
	TRefCountPtr<ID3D11SamplerState>* FoundSampler = CachedSamplers.Find(SamplerKey);
	if(FoundSampler)
	{
		return *FoundSampler;
	}
	else
#endif
	{
		D3D11_SAMPLER_DESC Desc;
		if(SamplerState)
		{
			appMemcpy(&Desc,&SamplerState->SamplerDesc,sizeof(D3D11_SAMPLER_DESC));
		}
		else
		{
			appMemzero(&Desc,sizeof(D3D11_SAMPLER_DESC));
		}

		ID3D11SamplerState* NewState = NULL;
		VERIFYD3D11RESULT(Direct3DDevice->CreateSamplerState(&Desc,&NewState));

		// Add the key to the list so we can find it next time
		CachedSamplers.Set(SamplerKey,NewState);
		return NewState;
	}
}

ID3D11DepthStencilState* FD3D11DynamicRHI::GetCachedDepthStencilState( const D3D11_DEPTH_STENCIL_DESC& DepthState, const D3D11_DEPTH_STENCIL_DESC& StencilState )
{
	FDepthStencilKey DepthStencilKey(DepthState,StencilState);
#if !LET_D3D11_CACHE_STATE
	TRefCountPtr<ID3D11DepthStencilState>* FoundDepthStencil = CachedDepthStencilStates.Find(DepthStencilKey);
	if(FoundDepthStencil)
	{
		return *FoundDepthStencil;
	}
	else
#endif
	{
		D3D11_DEPTH_STENCIL_DESC Desc;
		appMemcpy(&Desc,&StencilState,sizeof(D3D11_DEPTH_STENCIL_DESC));
		Desc.DepthEnable = DepthState.DepthEnable;
		Desc.DepthWriteMask = DepthState.DepthWriteMask;
		Desc.DepthFunc = DepthState.DepthFunc;

		ID3D11DepthStencilState* NewState = NULL;
		VERIFYD3D11RESULT(Direct3DDevice->CreateDepthStencilState(&Desc,&NewState));

		// Add the key to the list so we can find it next time
		CachedDepthStencilStates.Set(DepthStencilKey,NewState);
		return NewState;
	}
}

ID3D11RasterizerState* FD3D11DynamicRHI::GetCachedRasterizerState( const D3D11_RASTERIZER_DESC& InRasterizerState,UBOOL bScissorEnabled, UBOOL bMultisampleEnable )
{
	D3D11_RASTERIZER_DESC RasterizerState = InRasterizerState;

	// Verify that D3D11_RASTERIZER_DESC has valid settings.  The initial zeroed values in CurrentRasterizerDesc aren't valid.
	if(RasterizerState.CullMode == 0)
	{
		RasterizerState.CullMode = D3D11_CULL_NONE;
	}
	if(RasterizerState.FillMode == 0)
	{
		RasterizerState.FillMode = D3D11_FILL_SOLID;
	}

	// Convert our platform independent depth bias into a D3D11 depth bias.
	extern FLOAT GDepthBiasOffset;
	const INT D3DDepthBias = RasterizerState.DepthBias + appFloor(GDepthBiasOffset * (FLOAT)(1 << 24));

	FRasterizerKey RasterizerKey(RasterizerState,D3DDepthBias,bScissorEnabled,bMultisampleEnable && InRasterizerState.MultisampleEnable);
#if !LET_D3D11_CACHE_STATE
	TRefCountPtr<ID3D11RasterizerState>* FoundRasterizer = CachedRasterizerStates.Find(RasterizerKey);
	if(FoundRasterizer)
	{
		return *FoundRasterizer;
	}
	else
#endif
	{
		RasterizerState.ScissorEnable = bScissorEnabled;
		RasterizerState.MultisampleEnable = bMultisampleEnable && InRasterizerState.MultisampleEnable;
		RasterizerState.DepthBias = D3DDepthBias;

		ID3D11RasterizerState* NewState = NULL;
		VERIFYD3D11RESULT(Direct3DDevice->CreateRasterizerState(&RasterizerState,&NewState));

		// Add the key to the list so we can find it next time
		CachedRasterizerStates.Set(RasterizerKey,NewState);
		return NewState;
	}
}

ID3D11BlendState* FD3D11DynamicRHI::GetCachedBlendState( const D3D11_BLEND_DESC& BlendState, const TStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>& EnabledStateValue )
{
	FBlendKey BlendKey(BlendState,EnabledStateValue);
#if !LET_D3D11_CACHE_STATE
	TRefCountPtr<ID3D11BlendState>* FoundBlend = CachedBlendStates.Find(BlendKey);
	if(FoundBlend)
	{
		return *FoundBlend;
	}
	else
#endif
	{
		D3D11_BLEND_DESC Desc;
		appMemcpy(&Desc,&BlendState,sizeof(D3D11_BLEND_DESC));

		for(UINT RenderTargetIndex = 0;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
		{
			Desc.RenderTarget[RenderTargetIndex].RenderTargetWriteMask = EnabledStateValue[RenderTargetIndex];
		}

		ID3D11BlendState* NewState = NULL;
		VERIFYD3D11RESULT(Direct3DDevice->CreateBlendState(&Desc,&NewState));

		// Add the key to the list so we can find it next time
		CachedBlendStates.Set(BlendKey,NewState);
		return NewState;
	}
}

void FD3D11DynamicRHI::UpdateMSAASettings()
{
	if(CachedMaxMultisamples != GSystemSettings.MaxMultiSamples)
	{
		CachedMaxMultisamples = GSystemSettings.MaxMultiSamples;

		if (GSystemSettings.UsesMSAA() && GSystemSettings.MaxMultiSamples != GOptimalMSAALevel)
		{
			warnf(TEXT("MSAA is enabled, but using a different sample count than the one deferred shaders were compiled for!  Rendering will be inefficient."));
		}

		// MSAA count and quality - find the best given the settings in ini
		MSAACount = CachedMaxMultisamples; // start out with the max, will reduce later on
		MSAAQuality = 0;

		if(MSAACount > 1)
		{
			// We need to be able to render to MSAA color surfaces, and resolve them to a non-MSAA texture.
			const UINT RequiredMSAASupport = D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET | D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE;

			TArray<EPixelFormat> FormatsThatRequireMSAA;
			FormatsThatRequireMSAA.AddItem(PF_A8R8G8B8); // LightAttenuation
			FormatsThatRequireMSAA.AddItem(PF_FloatRGBA); // SceneColorScratch
			FormatsThatRequireMSAA.AddItem(PF_G16R16); // MotionBlur
			FormatsThatRequireMSAA.AddItem(PF_FloatR11G11B10); // Deferred lighting diffuse G-buffer.
			FormatsThatRequireMSAA.AddItem(PF_A2B10G10R10); // Deferred lighting normal G-buffers.
			FormatsThatRequireMSAA.AddItem(PF_DepthStencil);

			for( INT FIndex=0; FIndex < FormatsThatRequireMSAA.Num(); FIndex++ )
			{
				DXGI_FORMAT PlatformFormat = (DXGI_FORMAT)GPixelFormats[FormatsThatRequireMSAA(FIndex)].PlatformFormat;

				// A format of DXGI_FORMAT_R24G8_TYPELESS will use DXGI_FORMAT_D24_UNORM_S8_UINT as the actual depth stencil view
				if (PlatformFormat == DXGI_FORMAT_R24G8_TYPELESS)
				{
					PlatformFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
				}

				UINT ColorFormatSupport = 0;
				VERIFYD3D11RESULT(Direct3DDevice->CheckFormatSupport(PlatformFormat,&ColorFormatSupport));

				// If MSAA is supported at all, find out the quality levels and set the best one that we can
				if(ColorFormatSupport&RequiredMSAASupport)
				{
					// start counting down from current setting (indicated the current "best" count) and move down looking for support
					for(UINT IndexCount = MSAACount;IndexCount > 0;IndexCount--)
					{
						UINT NumMultiSampleQualities = 0;
						if(	SUCCEEDED(Direct3DDevice->CheckMultisampleQualityLevels(PlatformFormat,IndexCount,&NumMultiSampleQualities)) &&
							NumMultiSampleQualities > 0)
						{
							MSAACount = IndexCount;
							MSAAQuality = GetMultisampleQuality(MSAACount,NumMultiSampleQualities);
							break;
						}
					}
				}
				else
				{
					appMsgf(AMT_OK,
						TEXT("DX11 CSAA/MSAA mode is not supported on this format (Fmt=%d). Disabling Multisampling."),
						(INT)PlatformFormat	);
					
#if 0
					// Make change to unset in ini file
					FSystemSettings Settings = GSystemSettings;
					Settings.MaxMultiSamples = 1;
					GSystemSettings.ApplyNewSettings(Settings, TRUE);
#else
					MSAACount = 1;
					MSAAQuality = 0;
#endif
					break;
				}
			}
		}
	}
}

void FD3D11DynamicRHI::CleanupD3DDevice()
{
	if(GIsRHIInitialized)
	{
        delete StereoUpdater;
        StereoUpdater = NULL;

		check(Direct3DDevice);
		check(Direct3DDeviceIMContext);

		// Reset the RHI initialized flag.
		GIsRHIInitialized = FALSE;

		// Ask all initialized FRenderResources to release their RHI resources.
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseRHI();
		}
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseDynamicRHI();
		}

		// release our state cache
		ReleaseCachedD3D11States();
		// release our dynamic VB and IB buffers
		ReleaseDynamicVBandIBBuffers();

		// Release the device and its IC
		Direct3DDeviceIMContext = NULL;
		Direct3DDevice = NULL;
	}
}


/**
 *	Sets the maximum viewport size the application is expecting to need for the time being, or zero for
 *	no preference.  This is used as a hint to the RHI to reduce redundant device resets when viewports
 *	are created or destroyed (typically in the editor.)
 *
 *	@param NewLargestExpectedViewportWidth Maximum width of all viewports (or zero if not known)
 *	@param NewLargestExpectedViewportHeight Maximum height of all viewports (or zero if not known)
 */
void FD3D11DynamicRHI::SetLargestExpectedViewportSize( UINT NewLargestExpectedViewportWidth,
													   UINT NewLargestExpectedViewportHeight )
{
	// @todo: Add support for this after we add robust support for editor viewports in D3D11 (similar to D3D9)
}

void FD3D11DynamicRHI::AcquireThreadOwnership()
{
	// Nothing to do
}
void FD3D11DynamicRHI::ReleaseThreadOwnership()
{
	// Nothing to do
}
