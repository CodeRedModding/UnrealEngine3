/*=============================================================================
	D3D9Device.cpp: D3D device RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"
#include "..\Src\SceneRenderTargets.h"
#if WITH_APEX
#include "..\Src\NvApexRender.h"
#endif

//
// Globals.
//

IDirect3DDevice9* GLegacyDirect3DDevice9 = NULL;

/**
 * Set if we are running the debug D3D runtime (only set in non-shipping builds)
 */
UBOOL GIsDebugD3DRuntime = FALSE;

/** In bytes. 0 means unlimited. */
extern INT GTexturePoolSize;
/** Whether to read the texture pool size from engine.ini on PC. Can be turned on with -UseTexturePool on the command line. */
extern UBOOL GReadTexturePoolSizeFromIni;

/**
 * Adapter info for vendor/device specific handling
 */
D3DADAPTER_IDENTIFIER9 GGPUAdapterID;

/**
 * Called at startup to initialize the D3D9 RHI.
 * @return The D3D9 RHI if supported, otherwise NULL.
 */
FDynamicRHI* D3D9CreateRHI()
{
	return new FD3D9DynamicRHI();
}

FD3D9DynamicRHI::FD3D9DynamicRHI():
    AdapterIndex(D3DADAPTER_DEFAULT),
    DeviceType(D3DDEVTYPE_HAL),
	bDeviceLost(FALSE),
	DeviceSizeX(0),
	DeviceSizeY(0),
	DeviceWindow(NULL),
	bIsFullscreenDevice(FALSE),
	MaxActiveVertexStreamIndex(INDEX_NONE),
	LargestExpectedViewportWidth(0),
	LargestExpectedViewportHeight(0),
	bDepthBoundsHackSupported(FALSE),
	FrameSyncEvent(this),
	GPUFrameTiming(this, 3),
	VertexDeclarationCache(this),
	PendingNumInstances(1),
	UpdateStreamForInstancingMask(0),
	PendingBegunDrawPrimitiveUP(FALSE),
	PendingNumVertices(0),
	PendingVertexDataStride(0),
	PendingPrimitiveType(0),
	PendingNumPrimitives(0),
	PendingMinVertexIndex(0),
	PendingIndexDataStride(0),
    StereoUpdater(NULL)
{
	CurrentEventNodeFrame= NULL;
	CurrentEventNode = NULL;
	bTrackingEvents = FALSE;
	bLatchedGProfilingGPU = FALSE;
	bOriginalGEmitDrawEvents = FALSE;

	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );

	StereoUpdater = new nv::stereo::UE3StereoD3D9(true);

	// Check commandline whether we want to use software rendering.
	GUseSoftwareRendering = ParseParam( appCmdLine(), TEXT("PIXO") );

	GTexturePoolSize = 0;
	if ( GReadTexturePoolSizeFromIni )
	{
		GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSize"), GTexturePoolSize, GEngineIni);
		GTexturePoolSize *= 1024*1024;
	}

	// Software rendering via Pixomatic.
	if( GUseSoftwareRendering )
	{
#ifdef _DEBUG
		void* PixoDLL = appGetDllHandle( TEXT("pixo9_debug.dll") );
#else
		void* PixoDLL = appGetDllHandle( TEXT("pixo9.dll") );
#endif
		// DLL found.
		if( PixoDLL )
		{
			typedef IDirect3D9* (WINAPI *PixoDirect3DCreate9Func)(UINT SDKVersion);
			PixoDirect3DCreate9Func PixoDirect3DCreate9 = (PixoDirect3DCreate9Func) appGetDllExport( PixoDLL, TEXT("PixoDirect3DCreate9") );
			// Creation entry point found.
			if( PixoDirect3DCreate9 )
			{
				// Create the D3D object.
				*Direct3D.GetInitReference() = PixoDirect3DCreate9(D3D_SDK_VERSION);
			}
			else
			{
				appErrorf(TEXT("Missing Pixomatic DLL entry point."));
			}
		}
		else
		{
			appErrorf(TEXT("Missing Pixomatic DLL"));
		}
	}
	// Regular D3D9.
	else
	{
		// Create the D3D object.
		*Direct3D.GetInitReference() = Direct3DCreate9(D3D_SDK_VERSION);
	}
	
	if ( !Direct3D )
	{
		appErrorf(
			NAME_FriendlyError,
			TEXT("Please install DirectX 9.0c or later (see Release Notes for instructions on how to obtain it)")
			);
	}

#if !UE3_LEAN_AND_MEAN
	// ensure we are running against the proper D3DX runtime
	if (!D3DXCheckVersion(D3D_SDK_VERSION, D3DX_SDK_VERSION))
	{
		appErrorf(
			NAME_FriendlyError,
			TEXT("The D3DX9 runtime version does not match what the application was built with (%d). Cannot continue."),
			D3DX_SDK_VERSION
			);
	}
#endif

	AdapterIndex = D3DADAPTER_DEFAULT;
	DeviceType = DEBUG_SHADERS ? D3DDEVTYPE_REF : D3DDEVTYPE_HAL;
	if(SUCCEEDED(Direct3D->GetDeviceCaps(AdapterIndex,DeviceType,&DeviceCaps)))
	{
		if ( (DeviceCaps.PixelShaderVersion & 0xff00) < 0x0300 )
		{
			appErrorf(
				NAME_FriendlyError,
				TEXT("This game requires at least Shader Model 3.0")
				);
		}

		// Flag each of the vertex element types supported based on cap bits
		GVertexElementTypeSupport.SetSupported( VET_PackedNormal, ( DeviceCaps.DeclTypes & D3DDTCAPS_UBYTE4 ) ? TRUE : FALSE );
		GVertexElementTypeSupport.SetSupported( VET_UByte4, ( DeviceCaps.DeclTypes & D3DDTCAPS_UBYTE4 ) ? TRUE : FALSE );
		GVertexElementTypeSupport.SetSupported( VET_UByte4N, ( DeviceCaps.DeclTypes & D3DDTCAPS_UBYTE4N ) ? TRUE : FALSE );
		GVertexElementTypeSupport.SetSupported( VET_Short2N, ( DeviceCaps.DeclTypes & D3DDTCAPS_SHORT2N ) ? TRUE : FALSE );
		GVertexElementTypeSupport.SetSupported( VET_Half2, ( DeviceCaps.DeclTypes & D3DDTCAPS_FLOAT16_2 ) ? TRUE : FALSE );

		GMaxWholeSceneDominantShadowDepthBufferSize = Min<INT>(Min<INT>(DeviceCaps.MaxTextureWidth, DeviceCaps.MaxTextureHeight), 4096);
	}

	// Read the current display mode.
	D3DDISPLAYMODE CurrentDisplayMode;
	VERIFYD3D9RESULT(Direct3D->GetAdapterDisplayMode(AdapterIndex,&CurrentDisplayMode));

	// Determine whether alpha blending with a floating point framebuffer is supported.
	if (!GIsBuildMachine
		&& GSystemSettings.bAllowFloatingPointRenderTargets 
		&& FAILED(Direct3D->CheckDeviceFormat(
			AdapterIndex,
			DeviceType,
			CurrentDisplayMode.Format,
			D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
			D3DRTYPE_TEXTURE,
			D3DFMT_A16B16G16R16F
		)))
	{
		appMsgf(
			AMT_OK,
			*LocalizeError(TEXT("Error_FloatingPointBlendingRequired"), TEXT("Launch"))
			);
		appRequestExit(TRUE);
	}

	if ( GSystemSettings.bAllowFloatingPointRenderTargets && FAILED(
		Direct3D->CheckDeviceFormat(
		AdapterIndex,
		DeviceType,
		CurrentDisplayMode.Format,
		D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
		D3DRTYPE_TEXTURE,
		D3DFMT_A32B32G32R32F
		)))
	{
		GPixelFormats[ PF_A32B32G32R32F	].Supported = FALSE;
	}

	if (ParseParam(appCmdLine(),TEXT("FORCESHADERMODEL3")) || ParseParam(appCmdLine(),TEXT("SM3"))
		)
	{
		GRHIShaderPlatform = SP_PCD3D_SM3;
	}

	// Initialize the platform pixel format map.
	GPixelFormats[ PF_Unknown		].PlatformFormat	= D3DFMT_UNKNOWN;
	GPixelFormats[ PF_A32B32G32R32F	].PlatformFormat	= D3DFMT_A32B32G32R32F;
	GPixelFormats[ PF_A8R8G8B8		].PlatformFormat	= D3DFMT_A8R8G8B8;
	GPixelFormats[ PF_G8			].PlatformFormat	= D3DFMT_L8;
	GPixelFormats[ PF_G16			].PlatformFormat	= D3DFMT_UNKNOWN;	// Not supported for rendering.
	GPixelFormats[ PF_DXT1			].PlatformFormat	= D3DFMT_DXT1;
	GPixelFormats[ PF_DXT3			].PlatformFormat	= D3DFMT_DXT3;
	GPixelFormats[ PF_DXT5			].PlatformFormat	= D3DFMT_DXT5;
	GPixelFormats[ PF_UYVY			].PlatformFormat	= D3DFMT_UYVY;
	GPixelFormats[ PF_DepthStencil	].PlatformFormat	= D3DFMT_D24S8;
	GPixelFormats[ PF_ShadowDepth	].PlatformFormat	= D3DFMT_D24X8;
	GPixelFormats[ PF_FilteredShadowDepth ].PlatformFormat = D3DFMT_D24X8;
	GPixelFormats[ PF_R32F			].PlatformFormat	= D3DFMT_R32F;
	GPixelFormats[ PF_G16R16		].PlatformFormat	= D3DFMT_G16R16;
	GPixelFormats[ PF_G16R16F		].PlatformFormat	= D3DFMT_G16R16F;
	GPixelFormats[ PF_G16R16F_FILTER].PlatformFormat	= D3DFMT_G16R16F;
	GPixelFormats[ PF_G32R32F		].PlatformFormat	= D3DFMT_G32R32F;
	GPixelFormats[ PF_D24			].PlatformFormat    = (D3DFORMAT)(MAKEFOURCC('D','F','2','4'));
	GPixelFormats[ PF_R16F			].PlatformFormat	= D3DFMT_R16F;
	GPixelFormats[ PF_R16F_FILTER	].PlatformFormat	= D3DFMT_R16F;
	GPixelFormats[ PF_V8U8			].PlatformFormat	= D3DFMT_V8U8;
	GPixelFormats[ PF_BC5			].PlatformFormat	= (D3DFORMAT)(MAKEFOURCC('A','T','I','2'));
	GPixelFormats[ PF_A1			].PlatformFormat	= D3DFMT_A1; // Not supported for rendering.

	FrameSyncEvent.InitResource();
	GPUFrameTiming.InitResource();
}

FD3D9DynamicRHI::~FD3D9DynamicRHI()
{
	GPUFrameTiming.ReleaseResource();
	FrameSyncEvent.ReleaseResource();
	
	// Reset the RHI initialized flag.
	GIsRHIInitialized = FALSE;
}

/**
 * Initializes the D3D device for the current viewport state.
 * This function must be called from the main game thread.
 */
void FD3D9DynamicRHI::UpdateD3DDeviceFromViewports()
{
	check( IsInGameThread() );

	// Wait for the rendering thread to go idle.
	SCOPED_SUSPEND_RENDERING_THREAD(FSuspendRenderingThread::ST_RecreateThread);

	if(Viewports.Num())
	{
		check(Direct3D);

		// Find the maximum viewport dimensions and any fullscreen viewports.
		FD3D9Viewport*	NewFullscreenViewport = NULL;
		UINT			MaxViewportSizeX = LargestExpectedViewportWidth,
						MaxViewportSizeY = LargestExpectedViewportHeight,
						MaxHitViewportSizeX = 0,
						MaxHitViewportSizeY = 0;

		// We'll check to see if we still have a viewport for the last device window handle we used.  This allows
		// us to avoid resetting the device unnecessarily in some cases.
		UBOOL bFoundLastDeviceWindowHandle = FALSE;

		for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
		{
			FD3D9Viewport* D3DViewport = Viewports(ViewportIndex);

			MaxViewportSizeX = Max(MaxViewportSizeX,D3DViewport->GetSizeX());
			MaxViewportSizeY = Max(MaxViewportSizeY,D3DViewport->GetSizeY());

			if(D3DViewport->IsFullscreen())
			{
				check(!NewFullscreenViewport);
				NewFullscreenViewport = D3DViewport;

				// Check that all fullscreen viewports use supported resolutions.
				UINT Width = D3DViewport->GetSizeX();
				UINT Height = D3DViewport->GetSizeY();
				GetSupportedResolution( Width, Height );
				check( Width == D3DViewport->GetSizeX() && Height == D3DViewport->GetSizeY() );
			}

			// Do we recognize this window handle?
			const HWND CurWindowHandle = ( HWND )D3DViewport->GetWindowHandle();
			if( CurWindowHandle != NULL && CurWindowHandle == DeviceWindow )
			{
				bFoundLastDeviceWindowHandle = TRUE;
			}
		}
		
		// Determine the adapter and device type to use.
		UINT AdapterIndex = D3DADAPTER_DEFAULT;
		D3DDEVTYPE DeviceType = DEBUG_SHADERS ? D3DDEVTYPE_REF : D3DDEVTYPE_HAL;

		// Setup the needed device parameters.
		UINT NewDeviceSizeX = MaxViewportSizeX;
		UINT NewDeviceSizeY = MaxViewportSizeY;
		UBOOL NewDeviceFullscreen = NewFullscreenViewport ? TRUE : FALSE;

		// Select device window.  In full screen mode this is used by Present as well as focus tracking.  In
		// windowed mode this isn't really used for much, but we still need to set it.
		HWND NewDeviceWindow = NULL;
		if( NewFullscreenViewport != NULL )
		{
			// Use the full screen viewport window handle
			NewDeviceWindow = (HWND)NewFullscreenViewport->GetWindowHandle();
		}
		else
		{
			// In windowed mode, try to use the previous window handle if the viewport is still around
			if( bFoundLastDeviceWindowHandle )
			{
				// Great, we can keep the window handle we used previously.  This may prevent us from
				// having to reset the device unnecessarily!
				NewDeviceWindow = DeviceWindow;
			}
			else
			{
				// Just select an arbitrary window handle from our list of viewports
				NewDeviceWindow = (HWND)Viewports(0)->GetWindowHandle();
			}
		}

		// Check to see if Direct3DDevice needs to be recreated.
		UBOOL bRecreateDevice = FALSE;
		if(!Direct3DDevice)
		{
			bRecreateDevice = TRUE;
		}
		else
		{
			if(bDeviceLost)
			{
				// Abort if none of our windows has focus
				HWND FocusWindow = ::GetFocus();
				if ( FocusWindow == NULL || ::IsIconic( FocusWindow ) )
				{
					// Abort and try again next time Present() is called again from RHIEndDrawingViewport().
					return;
				}
				FocusWindow = ::GetFocus();
				bRecreateDevice = TRUE;
			}

			// Only if we're not in the middle of shutting down
			if( !GIsRequestingExit )
			{
				if(NewDeviceFullscreen != bIsFullscreenDevice)
				{
					bRecreateDevice = TRUE;
				}

				if(NewDeviceFullscreen)
				{
					if(DeviceSizeX != NewDeviceSizeX || DeviceSizeY != NewDeviceSizeY)
					{
						bRecreateDevice = TRUE;
					}
				}
				else
				{
					#if WITH_PANORAMA
						// We have to recreate device even when new resolution is lower then the old one
						// because when using panorama we need to reset live to center Live's blade on screen.
						if(DeviceSizeX != NewDeviceSizeX || DeviceSizeY != NewDeviceSizeY)
						{
							bRecreateDevice = TRUE;
						}
					#else
						if(DeviceSizeX < NewDeviceSizeX || DeviceSizeY < NewDeviceSizeY)
						{
							bRecreateDevice = TRUE;
						}
					#endif
				}

				if(DeviceWindow != NewDeviceWindow)
				{
					bRecreateDevice	= TRUE;
				}
			}
		}

		if(bRecreateDevice)
		{
			HRESULT Result;

			// Setup the present parameters.
			D3DPRESENT_PARAMETERS PresentParameters;
			appMemzero(&PresentParameters,sizeof(PresentParameters));
			PresentParameters.BackBufferCount			= 1;
			PresentParameters.BackBufferFormat			= D3DFMT_A8R8G8B8;
			PresentParameters.BackBufferWidth			= NewDeviceSizeX;
			PresentParameters.BackBufferHeight			= NewDeviceSizeY;
			PresentParameters.SwapEffect				= NewDeviceFullscreen ? D3DSWAPEFFECT_DISCARD : D3DSWAPEFFECT_COPY;
			PresentParameters.Flags						= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
			PresentParameters.EnableAutoDepthStencil	= FALSE;
			PresentParameters.Windowed					= !NewDeviceFullscreen;
			PresentParameters.PresentationInterval		= GSystemSettings.bUseVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
			PresentParameters.hDeviceWindow				= NewDeviceWindow;

			if(Direct3DDevice)
			{
				// Release dynamic resources and render targets.
				for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
				{
					ResourceIt->ReleaseDynamicRHI();
				}

				// Let the stereo updated know the device has been lost, this avoids a potential race condition
				if (StereoUpdater) 
				{
					StereoUpdater->RequiresUpdate(true);
				}
#if WITH_APEX
				GApexRender->SetRequireRewriteBuffers(TRUE);
#endif

				// Release the back-buffer reference to allow resetting the device.
				BackBuffer = NULL;

				// Simple reset the device with the new present parameters.
				do 
				{
					#if WITH_PANORAMA
						extern void appPanoramaRenderHookReset(void*);
						// Allow Panorama to reset any resources before reseting the device
						appPanoramaRenderHookReset(&PresentParameters);
					#endif
					if( FAILED(Result=Direct3DDevice->Reset(&PresentParameters) ) )
					{
						// Sleep for a second before trying again if we couldn't reset the device as the most likely
						// cause is the device not being ready/lost which can e.g. occur if a screen saver with "lock"
						// kicks in.
						appSleep(1.0);
					}
				} 
				while( FAILED(Result) );

				// Get pointers to the device's back buffer and depth buffer.
				TRefCountPtr<IDirect3DSurface9> D3DBackBuffer;
				VERIFYD3D9RESULT(Direct3DDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,D3DBackBuffer.GetInitReference()));
				BackBuffer = new FD3D9Surface(
					(FD3D9Texture2D*)NULL,
					NULL,
					D3DBackBuffer
					);

				// Reinitialize dynamic resources and render targets.
				for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
				{
					ResourceIt->InitDynamicRHI();
				}
			}
			else
			{
				// Query max texture size supported by the device.
				GMaxTextureMipCount = appCeilLogTwo( Min<INT>( DeviceCaps.MaxTextureHeight, DeviceCaps.MaxTextureWidth ) ) + 1;
				GMaxTextureMipCount = Min<INT>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );

				// Check for ps_2_0 support.
				if((DeviceCaps.PixelShaderVersion & 0xff00) < 0x0200)
				{
					appErrorf(
						NAME_FriendlyError,
						TEXT("Video card below minimum specifications!  Device does not support pixel shaders 2.0 or greater.  PixelShaderVersion=%08x"), DeviceCaps.PixelShaderVersion
						);
				}

				// Check for hardware vertex instancing support.
				GSupportsVertexInstancing = (DeviceCaps.PixelShaderVersion & 0xff00) >= 0x0300;
				GSupportsEmulatedVertexInstancing = !GSupportsVertexInstancing && (DeviceCaps.DevCaps2 & D3DDEVCAPS2_STREAMOFFSET);

				// Check whether card supports HW TnL
				const UBOOL bSupportsHardwareTnL = ( DeviceCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) ? TRUE : FALSE;
				// Check whether card supports vertex elements sharing offsets. But check it only if there is HW TnL.
				if( bSupportsHardwareTnL )
				{
					GVertexElementsCanShareStreamOffset = (DeviceCaps.DevCaps2 & D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET) ? TRUE : FALSE; 
				}

				// Check for required format support.
				D3DDISPLAYMODE	CurrentDisplayMode;
				VERIFYD3D9RESULT(Direct3D->GetAdapterDisplayMode(AdapterIndex,&CurrentDisplayMode));

				// Determine the supported lighting buffer formats.
				if( FAILED(Direct3D->CheckDeviceFormat(
					AdapterIndex,
					DeviceType,
					CurrentDisplayMode.Format,
					D3DUSAGE_RENDERTARGET,
					D3DRTYPE_TEXTURE,
					D3DFMT_A16B16G16R16F
							)))
				{
					GSystemSettings.bAllowFloatingPointRenderTargets = FALSE;
				}

				// Setup the FP RGB pixel formats based on 16-bit FP support.
				GPixelFormats[ PF_FloatRGB	].PlatformFormat	= GSystemSettings.bAllowFloatingPointRenderTargets ? D3DFMT_A16B16G16R16F : D3DFMT_A8R8G8B8;
				GPixelFormats[ PF_FloatRGB	].BlockBytes		= GSystemSettings.bAllowFloatingPointRenderTargets ? 8 : 4;
				GPixelFormats[ PF_FloatRGBA	].PlatformFormat	= GSystemSettings.bAllowFloatingPointRenderTargets ? D3DFMT_A16B16G16R16F : D3DFMT_A8R8G8B8;
				GPixelFormats[ PF_FloatRGBA	].BlockBytes		= GSystemSettings.bAllowFloatingPointRenderTargets ? 8 : 4;

				// Determine whether hardware shadow mapping is supported.
				GSupportsHardwarePCF = SUCCEEDED(
					Direct3D->CheckDeviceFormat(
						AdapterIndex, 
						DeviceType, 
						CurrentDisplayMode.Format, 
						D3DUSAGE_DEPTHSTENCIL,
						D3DRTYPE_TEXTURE,
						D3DFMT_D24X8 
						)
					);

				// Determine whether hardware vertex texture fetch is supported (e.g. ATI/AMD X1*** series does not)
				GSupportsVertexTextureFetch = SUCCEEDED(
					Direct3D->CheckDeviceFormat(
					AdapterIndex, 
					DeviceType, 
					CurrentDisplayMode.Format, 
					D3DUSAGE_QUERY_VERTEXTEXTURE,
					D3DRTYPE_TEXTURE,
					D3DFMT_A32B32G32R32F		// only format we use currently
					)
					);

				// Check for D24 support, which indicates that ATI's Fetch4 is also supported
				GSupportsFetch4 = SUCCEEDED(
					Direct3D->CheckDeviceFormat(
						AdapterIndex, 
						DeviceType, 
						CurrentDisplayMode.Format, 
						D3DUSAGE_DEPTHSTENCIL,
						D3DRTYPE_TEXTURE,
						(D3DFORMAT)(MAKEFOURCC('D','F','2','4'))
						)
					);

				// Check for support of Nvidia's depth bounds test in D3D9
				bDepthBoundsHackSupported = SUCCEEDED(
					Direct3D->CheckDeviceFormat(
						AdapterIndex, 
						DeviceType, 
						CurrentDisplayMode.Format, 
						0,
						D3DRTYPE_SURFACE,
						(D3DFORMAT)(MAKEFOURCC('N','V','D','B'))
						)
					);

				// Determine whether filtering of floating point textures is supported.
				if (GSystemSettings.bAllowFloatingPointRenderTargets && FAILED(
					Direct3D->CheckDeviceFormat(
						AdapterIndex,
						DeviceType,
						CurrentDisplayMode.Format,
						D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_FILTER,
						D3DRTYPE_TEXTURE,
						D3DFMT_A16B16G16R16F
					)))
				{
					//only overwrite if not supported, since the previous setting may have been specified on the command line
					GSupportsFPFiltering = FALSE;
				}

				// Verify that the device supports the required single component 32-bit floating point render target format.
				if( FAILED(Direct3D->CheckDeviceFormat(
							AdapterIndex,
							DeviceType,
							CurrentDisplayMode.Format,
							D3DUSAGE_RENDERTARGET,
							D3DRTYPE_TEXTURE,
							D3DFMT_R32F
							)))
				{
					appErrorf(
						NAME_FriendlyError,
						TEXT("Video card below minimum specifications!  Device does not support 1x32 FP render target format.")
						);
				}

				// Query for YUV texture format support.

				if( SUCCEEDED(Direct3D->CheckDeviceFormat( AdapterIndex, DeviceType, CurrentDisplayMode.Format, D3DUSAGE_DYNAMIC, D3DRTYPE_TEXTURE, D3DFMT_UYVY	) ) )
				{
					// Query for SRGB read support (gamma correction on texture sampling) for YUV texture format. E.g. not supported on Radeon 9800.
					if( FAILED(Direct3D->CheckDeviceFormat( AdapterIndex, DeviceType, CurrentDisplayMode.Format, D3DUSAGE_QUERY_SRGBREAD, D3DRTYPE_TEXTURE, D3DFMT_UYVY	) ) )
					{
						GPixelFormats[PF_UYVY].Flags |= PF_REQUIRES_GAMMA_CORRECTION;
					}
					else
					{
						GPixelFormats[PF_UYVY].Flags &= ~ PF_REQUIRES_GAMMA_CORRECTION;
					}

					GPixelFormats[PF_UYVY].Supported = 1;
				}
				else
				{
					GPixelFormats[PF_UYVY].Supported = 0;
				}

				// Query for L8 render target format support
				if( SUCCEEDED(Direct3D->CheckDeviceFormat( AdapterIndex, DeviceType, CurrentDisplayMode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_L8) ) )
				{
					GSupportsRenderTargetFormat_PF_G8 = TRUE;
				}
				else
				{
					GSupportsRenderTargetFormat_PF_G8 = FALSE;
				}

				DWORD CreationFlags = 
					D3DCREATE_FPU_PRESERVE 
					// use software vertex shader if no HW T&L support
					| (bSupportsHardwareTnL ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING)
					// Games for Windows Live requires the multithreaded flag or it will BSOD
					| (WITH_PANORAMA /*|| 1*/ ? D3DCREATE_MULTITHREADED : 0)
					// Driver management of video memory currently is FAR from being optimal for streaming!
					| D3DCREATE_DISABLE_DRIVER_MANAGEMENT
					;

				// when running debug D3D runtime and not -onethread, add D3DCREATE_MULTITHREADED
#if !SHIPPING_PC_GAME
				{
					// WRH - 2007/09/18 - Note that there is no supported method of testing for the debug D3D runtime. This works on XP.
					// Also minimizes false positives. If the regkey location changes, it just won't detect the debug runtime.
					// You can also check for the presence of D3DQUERYTYPE_RESOURCEMANAGER queries, but we need to check BEFORE
					// the device is created.
					HKEY DebugRuntimeKey = NULL;
					DWORD DebugRuntimeValue = 0;
					DWORD DebugRuntimeValueKeySize = sizeof(DebugRuntimeValue);
					LONG Result = ERROR_SUCCESS;
					Result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Direct3D\\"), 0, KEY_READ, &DebugRuntimeKey);
					if (Result == ERROR_SUCCESS)
					{
						Result = RegQueryValueEx(DebugRuntimeKey, TEXT("LoadDebugRuntime"), NULL, NULL, (BYTE*)&DebugRuntimeValue, &DebugRuntimeValueKeySize);
						if (Result == ERROR_SUCCESS)
						{
							if (DebugRuntimeValue != 0)
							{
								GIsDebugD3DRuntime = TRUE;
								if (!ParseParam(appCmdLine(), TEXT("ONETHREAD")))
								{
									warnf(NAME_Init, TEXT("Using D3D Debug Runtime. Forcing D3DCREATE_MULTITHREADED to eliminate spam."));
									CreationFlags |= D3DCREATE_MULTITHREADED;
								}
							}
						}
						RegCloseKey(DebugRuntimeKey);
					}
				}
#endif

				// Try creating a new Direct3D device till it either succeeds or fails with an error code != D3DERR_DEVICELOST.
				while( 1 )
				{
					UINT AdapterNumber = D3DADAPTER_DEFAULT;

					// Don't allow NvPerfHUD profiling in the shippintg builds
#if !SHIPPING_PC_GAME
					// Automatically detect nvperfhud device
					for( UINT Adapter=0; Adapter < Direct3D->GetAdapterCount(); Adapter++ )
					{
						D3DADAPTER_IDENTIFIER9 Identifier;
						HRESULT Res = Direct3D->GetAdapterIdentifier( Adapter, 0, &Identifier );
						if( appStrstr( *FString(Identifier.Description), TEXT("PerfHUD") ) != NULL )
						{
							AdapterNumber = Adapter;
							DeviceType = D3DDEVTYPE_REF;
							break;
						}
					}
#endif

					Result = Direct3D->CreateDevice(
						AdapterNumber == INDEX_NONE ? D3DADAPTER_DEFAULT : AdapterNumber,
						DeviceType,
						NewDeviceWindow,	
						CreationFlags,
						&PresentParameters,
						Direct3DDevice.GetInitReference()
						);

					if( Result != D3DERR_DEVICELOST )
					{
						break;
					}

					appSleep( 0.5f );
				}

				if (FAILED(Result))
				{
					// This can happen if incorrect parameters have been passed to CreateDevice, or if the desktop is locked.
					// We present a user friendly error message for the case where the desktop is locked.
					appMsgf(
						AMT_OK,
						*LocalizeError(TEXT("Error_FailedToCreateD3D9Device"), TEXT("Launch"))
						);
					appRequestExit(TRUE);
				}

#if SHIPPING_PC_GAME
				// Disable PIX for windows in the shipping builds
				D3DPERF_SetOptions(1);
#endif

				#if WITH_PANORAMA
					extern void appPanoramaRenderHookInit(IUnknown*,void*,HWND);
					// Allow G4WLive to allocate any resources it needs
					appPanoramaRenderHookInit(Direct3DDevice,&PresentParameters,PresentParameters.hDeviceWindow);
				#endif

				// Fall back to D3DFMT_A8R8G8B8 if D3DFMT_G16R16 is not a supported render target format. In theory
				// we could try D3DFMT_A16B16G16R16 but cards not supporting G16R16 are usually too slow to use 'bigger' format.
				if( FAILED( Direct3D->CheckDeviceFormat(
					AdapterIndex,
					DeviceType,
					CurrentDisplayMode.Format,
					D3DUSAGE_RENDERTARGET,
					D3DRTYPE_TEXTURE,
					D3DFMT_G16R16 ) ) )
				{
					GPixelFormats[ PF_G16R16 ].PlatformFormat = D3DFMT_A8R8G8B8;
				}
				else
				{
					GPixelFormats[ PF_G16R16 ].PlatformFormat = D3DFMT_G16R16;
				}

				// Fall back to D3DFMT_A8R8G8B8 if D3DFMT_A2B10G10R10 is not a supported render target format.
				if( FAILED( Direct3D->CheckDeviceFormat(
					AdapterIndex,
					DeviceType,
					CurrentDisplayMode.Format,
					D3DUSAGE_RENDERTARGET,
					D3DRTYPE_TEXTURE,
					D3DFMT_A2B10G10R10 ) ) )
				{
					GPixelFormats[ PF_A2B10G10R10 ].PlatformFormat = D3DFMT_A8R8G8B8;
				}
				else
				{
					GPixelFormats[ PF_A2B10G10R10 ].PlatformFormat = D3DFMT_A2B10G10R10;
				}

				// Fall back to D3DFMT_A8R8G8B8 if D3DFMT_A16B16G16R16 is not a supported render target format. In theory
				// we could try D3DFMT_A32B32G32R32 but cards not supporting the 16 bit variant are usually too slow to use
				// the full 32 bit format.
				if( !GSystemSettings.bAllowFloatingPointRenderTargets || FAILED( Direct3D->CheckDeviceFormat(
					AdapterIndex,
					DeviceType,
					CurrentDisplayMode.Format,
					D3DUSAGE_RENDERTARGET,
					D3DRTYPE_TEXTURE,
					D3DFMT_A16B16G16R16 ) ) )
				{
					GPixelFormats[ PF_A16B16G16R16 ].PlatformFormat = D3DFMT_A8R8G8B8;
				}
				else
				{
					GPixelFormats[ PF_A16B16G16R16 ].PlatformFormat = D3DFMT_A16B16G16R16;
				}

				// Get pointers to the device's back buffer and depth buffer.
				TRefCountPtr<IDirect3DSurface9> D3DBackBuffer;
				VERIFYD3D9RESULT(Direct3DDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,D3DBackBuffer.GetInitReference()));
				BackBuffer = new FD3D9Surface(
					(FD3D9Texture2D*)NULL,
					NULL,
					D3DBackBuffer
					);

				// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
				check(!GIsRHIInitialized);
				GIsRHIInitialized = TRUE;
				for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
				{
					ResourceIt->InitDynamicRHI();
				}
				for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
				{
					ResourceIt->InitRHI();
				}

				// Store the device/vendor ID
				{
					D3DDEVICE_CREATION_PARAMETERS D3DCP;
					UBOOL bGotAdapaterIdentifier = FALSE;
					if (SUCCEEDED(Direct3DDevice->GetCreationParameters(&D3DCP)))
					{
						if(SUCCEEDED(Direct3D->GetAdapterIdentifier(D3DCP.AdapterOrdinal, 0, &GGPUAdapterID)))
						{
							bGotAdapaterIdentifier = TRUE;
						}
					}
					if (!bGotAdapaterIdentifier)
					{
						debugf(NAME_Init, TEXT("GetAdapterIdentifier failed. Cannot determine the GPU type"));
					}
				}

				if (GSystemSettings.UsesMSAA())
				{
					UINT MultiSampleCount = 0;
					UINT MultiSampleQuality = 0;
					GetMultisampleCountAndQuality(MultiSampleCount, MultiSampleQuality);

					TArray<EPixelFormat> FormatsThatRequireMSAA;
					FormatsThatRequireMSAA.AddItem(PF_A8R8G8B8); // LightAttenuation
					FormatsThatRequireMSAA.AddItem(PF_FloatRGBA); // SceneColorScratch
					FormatsThatRequireMSAA.AddItem(PF_G16R16); // MotionBlur
					FormatsThatRequireMSAA.AddItem(PF_DepthStencil);

					for (INT FIndex = 0; FIndex < FormatsThatRequireMSAA.Num(); FIndex++)
					{
						const D3DFORMAT PlatformFormat = (D3DFORMAT)GPixelFormats[FormatsThatRequireMSAA(FIndex)].PlatformFormat;
						DWORD QualityLevels = 0;
						const HRESULT hr = Direct3D->CheckDeviceMultiSampleType(AdapterIndex, DeviceType, PlatformFormat, !NewDeviceFullscreen, (D3DMULTISAMPLE_TYPE)MultiSampleCount, &QualityLevels);
						
						if (hr != D3D_OK || QualityLevels <= MultiSampleQuality)
						{
							warnf(TEXT("The requested DX9 MSAA mode is not supported (Fmt=%s, SampleCount=%d, Quality=%d). Disabling MSAA."),*GetD3DTextureFormatString(PlatformFormat), MultiSampleCount, MultiSampleQuality);
							FSystemSettings Settings = GSystemSettings;
							Settings.MaxMultiSamples = 1;
							GSystemSettings.ApplyNewSettings(Settings, TRUE);
							break;
						}
					}
				}

				// WRH - 2007/10/06 - disable ATI checks for texture filtering optimization if desired. 
				// Recent ATI drivers (7.8+) analyzes a texture's mips on upload to see if it is eligible
				// for filtering optimizations. When streaming, this causes hitching as the analysis takes time.
				// This disables the check so filtering optimizations take place all the time.
				if (GGPUAdapterID.VendorId == GGPUVendorATI)
				{
					UBOOL bDisableATITextureFilterOptimizationChecks = TRUE;
					GConfig->GetBool(TEXT("Engine.ISVHacks"), TEXT("DisableATITextureFilterOptimizationChecks"), bDisableATITextureFilterOptimizationChecks, GEngineIni);
					if (bDisableATITextureFilterOptimizationChecks)
					{
						Direct3DDevice->SetRenderState(D3DRS_POINTSIZE, 0x7fa02001);
					}
				}

				// Allow the user to force minimum driver shader optimizations on legacy NVIDIA hardware to reduce hitching.
				if (GGPUAdapterID.VendorId == GGPUVendorNVIDIA)
				{
					UBOOL bUseMinimalNVIDIADriverShaderOptimization = FALSE;
					GConfig->GetBool(TEXT("Engine.ISVHacks"), TEXT("UseMinimalNVIDIADriverShaderOptimization"), bUseMinimalNVIDIADriverShaderOptimization, GEngineIni);
					if(bUseMinimalNVIDIADriverShaderOptimization)
					{
						Direct3DDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y,MAKEFOURCC('C','O','P','M'));
					}
				}
			}

			// Update saved device settings.
			DeviceSizeX = NewDeviceSizeX;
			DeviceSizeY = NewDeviceSizeY;
			DeviceWindow = NewDeviceWindow;
			bIsFullscreenDevice = NewDeviceFullscreen;
			bDeviceLost = FALSE;

			// Set the global variables which are accessed by other subsystems.
			GLegacyDirect3DDevice9 = Direct3DDevice;

			// Tell the windows to redraw when they can.
			for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
			{
				FD3D9Viewport* D3DViewport = Viewports(ViewportIndex);
				::PostMessage( (HWND) D3DViewport->GetWindowHandle(), WM_PAINT, 0, 0 );
			}
		}

        // Update scene render targets with the new size
		// Note: Scene render targets must be the same size as the back buffer, 
		// Since the back buffer will be used as a color buffer with the scene depth buffer when rendering UI,
		// And D3D requires the color buffer to be smaller or equal to the depth buffer.
        GSceneRenderTargets.Allocate( MaxViewportSizeX, MaxViewportSizeY );
	}
	else
	{
		// If no viewports are open, clean up the existing device.
		CleanupD3DDevice();
		Direct3DDevice = NULL;
		BackBuffer = NULL;
	}
}

void FD3D9DynamicRHI::GetSupportedResolution(UINT& Width,UINT& Height)
{
	check(Direct3D);

	// Determine the adapter and device type to use.
	UINT AdapterIndex = D3DADAPTER_DEFAULT;
	D3DDEVTYPE DeviceType = DEBUG_SHADERS ? D3DDEVTYPE_REF : D3DDEVTYPE_HAL;

	// Find the screen size that most closely matches the desired resolution.
	D3DDISPLAYMODE BestMode = { 0, 0, 0, D3DFMT_A8R8G8B8 };
	UINT InitializedMode = FALSE;
	UINT NumAdapterModes = Direct3D->GetAdapterModeCount(AdapterIndex,D3DFMT_X8R8G8B8);
	for(UINT ModeIndex = 0;ModeIndex < NumAdapterModes;ModeIndex++)
	{
		D3DDISPLAYMODE DisplayMode;
		Direct3D->EnumAdapterModes(AdapterIndex,D3DFMT_X8R8G8B8,ModeIndex,&DisplayMode);

		UBOOL IsEqualOrBetterWidth = Abs((INT)DisplayMode.Width - (INT)Width) <= Abs((INT)BestMode.Width - (INT)Width);
		UBOOL IsEqualOrBetterHeight = Abs((INT)DisplayMode.Height - (INT)Height) <= Abs((INT)BestMode.Height - (INT)Height);
		if(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
		{
			BestMode = DisplayMode;
			InitializedMode = TRUE;
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
UBOOL FD3D9DynamicRHI::GetAvailableResolutions(FScreenResolutionArray& Resolutions, UBOOL bIgnoreRefreshRate)
{
	if (!Direct3D)
	{
		return FALSE;
	}

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

	UINT AdapterIndex = D3DADAPTER_DEFAULT;
	INT ModeCount = Direct3D->GetAdapterModeCount(AdapterIndex, D3DFMT_X8R8G8B8);
	for (INT ModeIndex = 0; ModeIndex < ModeCount; ModeIndex++)
	{
		D3DDISPLAYMODE DisplayMode;
		HRESULT d3dResult = Direct3D->EnumAdapterModes(AdapterIndex, D3DFMT_X8R8G8B8, ModeIndex, &DisplayMode);
		if (d3dResult == D3D_OK)
		{
			if (((INT)DisplayMode.Width >= MinAllowableResolutionX) &&
				((INT)DisplayMode.Width <= MaxAllowableResolutionX) &&
				((INT)DisplayMode.Height >= MinAllowableResolutionY) &&
				((INT)DisplayMode.Height <= MaxAllowableResolutionY)
				)
			{
				UBOOL bAddIt = TRUE;
				if (bIgnoreRefreshRate == FALSE)
				{
					if (((INT)DisplayMode.RefreshRate < MinAllowableRefreshRate) ||
						((INT)DisplayMode.RefreshRate > MaxAllowableRefreshRate)
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
						if ((CheckResolution.Width == DisplayMode.Width) &&
							(CheckResolution.Height == DisplayMode.Height))
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

					ScreenResolution.Width = DisplayMode.Width;
					ScreenResolution.Height = DisplayMode.Height;
					ScreenResolution.RefreshRate = DisplayMode.RefreshRate;
				}
			}
		}
	}

	return TRUE;
}



/**
 *	Sets the maximum viewport size the application is expecting to need for the time being, or zero for
 *	no preference.  This is used as a hint to the RHI to reduce redundant device resets when viewports
 *	are created or destroyed (typically in the editor.)
 *
 *	@param NewLargestExpectedViewportWidth Maximum width of all viewports (or zero if not known)
 *	@param NewLargestExpectedViewportHeight Maximum height of all viewports (or zero if not known)
 */
void FD3D9DynamicRHI::SetLargestExpectedViewportSize( UINT NewLargestExpectedViewportWidth,
													  UINT NewLargestExpectedViewportHeight )
{
	LargestExpectedViewportWidth = NewLargestExpectedViewportWidth;
	LargestExpectedViewportHeight = NewLargestExpectedViewportHeight;
}



void FD3D9DynamicRHI::CleanupD3DDevice()
{
	check( IsInGameThread() );
	if(GIsRHIInitialized)
	{
        delete StereoUpdater; 
        StereoUpdater = NULL;

		// Ask all initialized FRenderResources to release their RHI resources.
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseRHI();
		}
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseDynamicRHI();
		}
		GIsRHIInitialized = FALSE;
	}
}

void FD3D9DynamicRHI::ResetVertexStreams()
{
	for(INT StreamIndex = 0;StreamIndex <= MaxActiveVertexStreamIndex;StreamIndex++)
	{
		Direct3DDevice->SetStreamSource(StreamIndex,NULL,0,0);
		Direct3DDevice->SetStreamSourceFreq(StreamIndex,1);
	}
	MaxActiveVertexStreamIndex = INDEX_NONE;
}

void FD3D9DynamicRHI::AcquireThreadOwnership()
{
	// Nothing to do
}
void FD3D9DynamicRHI::ReleaseThreadOwnership()
{
	// Nothing to do
}
