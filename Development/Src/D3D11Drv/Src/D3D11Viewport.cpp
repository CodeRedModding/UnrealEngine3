/*=============================================================================
	D3D11Viewport.cpp: D3D viewport RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"

/**
 * Creates a FD3D11Surface to represent a swap chain's back buffer.
 */
static FD3D11Surface* GetSwapChainSurface(FD3D11DynamicRHI* D3DRHI,IDXGISwapChain* SwapChain)
{
	// Grab the back buffer
	TRefCountPtr<ID3D11Texture2D> BackBufferResource;
	VERIFYD3D11RESULT(SwapChain->GetBuffer(0,IID_ID3D11Texture2D,(void**)BackBufferResource.GetInitReference()));

	// create the render target view
	TRefCountPtr<ID3D11RenderTargetView> BackBufferRenderTargetView;
	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
	RTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;
	VERIFYD3D11RESULT(D3DRHI->GetDevice()->CreateRenderTargetView(BackBufferResource,&RTVDesc,BackBufferRenderTargetView.GetInitReference()));

	return new FD3D11Surface(BackBufferRenderTargetView,NULL,NULL,NULL,NULL,NULL,NULL,BackBufferResource);
}

/**
 * Sets Panorama to draw to the viewport's swap chain.
 */
static void SetPanoramaSwapChain(FD3D11DynamicRHI* D3DRHI,IDXGISwapChain* SwapChain,HWND WindowHandle)
{
	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	SwapChain->GetDesc(&SwapChainDesc);

	#if WITH_PANORAMA
		extern void appPanoramaRenderHookInit(IUnknown*,void*,HWND);
		// Allow G4WLive to allocate any resources it needs
		appPanoramaRenderHookInit(D3DRHI->GetDevice(),&SwapChainDesc,WindowHandle);
	#endif
}

FD3D11Viewport::FD3D11Viewport(FD3D11DynamicRHI* InD3DRHI,HWND InWindowHandle,UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen):
	D3DRHI(InD3DRHI),
	WindowHandle(InWindowHandle),
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen),
	bIsValid(TRUE)
{
	D3DRHI->Viewports.AddItem(this);

	// Ensure that the D3D device has been created.
	D3DRHI->InitD3DDevice();

	// Create a backbuffer/swapchain for each viewport
	TRefCountPtr<IDXGIDevice> DXGIDevice;
	VERIFYD3D11RESULT(D3DRHI->GetDevice()->QueryInterface( IID_IDXGIDevice, (void**)DXGIDevice.GetInitReference() ));

	// Restore the window.
	// If the window is minimized when the swap chain is created, it may be created as an 8x8 buffer.
	::ShowWindow(WindowHandle,SW_RESTORE);

	// Create the swapchain.
	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	appMemzero( &SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC) );
	SwapChainDesc.BufferDesc.Width = SizeX;
	SwapChainDesc.BufferDesc.Height = SizeY;
	SwapChainDesc.BufferDesc.RefreshRate.Numerator = 0;	// illamas: use 0 to avoid a potential mismatch with hw
	SwapChainDesc.BufferDesc.RefreshRate.Denominator = 0;	// illamas: ditto
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 1;
	SwapChainDesc.OutputWindow = WindowHandle;
	SwapChainDesc.Windowed = !bIsFullscreen;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	VERIFYD3D11RESULT(D3DRHI->GetFactory()->CreateSwapChain(DXGIDevice,&SwapChainDesc,SwapChain.GetInitReference()));

	// Set the DXGI message hook to not change the window behind our back.
	D3DRHI->GetFactory()->MakeWindowAssociation(WindowHandle,DXGI_MWA_NO_WINDOW_CHANGES);

	// Create a RHI surface to represent the viewport's back buffer.
	BackBuffer = GetSwapChainSurface(D3DRHI,SwapChain);

	// If this is the first viewport, set Panorama to render to it.
	if(D3DRHI->Viewports.FindItemIndex(this) == 0)
	{
		SetPanoramaSwapChain(D3DRHI,SwapChain,WindowHandle);
	}

	// Tell the window to redraw when they can.
	::PostMessage( WindowHandle, WM_PAINT, 0, 0 );
}

FD3D11Viewport::~FD3D11Viewport()
{
	// If this is the first viewport, reset Panorama's reference to it.
	if(D3DRHI->Viewports.FindItemIndex(this) == 0)
	{
		#if WITH_PANORAMA
			extern void appPanoramaHookDeviceDestroyed();
			appPanoramaHookDeviceDestroyed();
		#endif
	}

	// If the swap chain was in fullscreen mode, switch back to windowed before releasing the swap chain.
	// DXGI throws an error otherwise.
	VERIFYD3D11RESULT(SwapChain->SetFullscreenState(FALSE,NULL));

	D3DRHI->Viewports.RemoveItem(this);
}

void FD3D11Viewport::Resize(UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen)
{
	// Release our backbuffer reference, as required by DXGI before calling ResizeBuffers.
	BackBuffer.SafeRelease();

	if(SizeX != InSizeX || SizeY != InSizeY)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;

		// Resize the swap chain.
		VERIFYD3D11RESULT(SwapChain->ResizeBuffers(1,SizeX,SizeY,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	}

	if(bIsFullscreen != bInIsFullscreen)
	{
		bIsFullscreen = bInIsFullscreen;
		bIsValid = FALSE;

		// Use ConditionalResetSwapChain to call SetFullscreenState, to handle the failure case.
		// Ignore the viewport's focus state; since Resize is called as the result of a user action we assume authority without waiting for Focus.
		ConditionalResetSwapChain(TRUE);
	}

	// Create a RHI surface to represent the viewport's back buffer.
	BackBuffer = GetSwapChainSurface(D3DRHI,SwapChain);

	// If this is the first viewport, set Panorama to render to it.
	if(D3DRHI->Viewports.FindItemIndex(this) == 0)
	{
		SetPanoramaSwapChain(D3DRHI,SwapChain,WindowHandle);
	}
}

void FD3D11Viewport::ConditionalResetSwapChain(UBOOL bIgnoreFocus)
{
	if(!bIsValid)
	{
		// Check if the viewport's window is focused before resetting the swap chain's fullscreen state.
		HWND FocusWindow = ::GetFocus();
		const UBOOL bIsFocused = FocusWindow == WindowHandle;
		const UBOOL bIsIconic = ::IsIconic( WindowHandle );
		if(bIgnoreFocus || (bIsFocused && !bIsIconic) )
		{
			FlushRenderingCommands();

			HRESULT Result = SwapChain->SetFullscreenState(bIsFullscreen,NULL);
			if(SUCCEEDED(Result))
			{
				bIsValid = TRUE;
			}
			else
			{
				// Even though the docs say SetFullscreenState always returns S_OK, that doesn't always seem to be the case.
				debugf(TEXT("IDXGISwapChain::SetFullscreenState returned %08x; waiting for the next frame to try again."),Result);
			}
		}
	}
}

void FD3D11Viewport::Present(UBOOL bLockToVsync)
{
	// We can't call Present if !bIsValid, as it waits a window message to be processed, but the main thread may not be pumping the message handler.
	if(bIsValid)
	{
		// Check if the viewport's swap chain has been invalidated by DXGI.
		BOOL bSwapChainFullscreenState;
		TRefCountPtr<IDXGIOutput> SwapChainOutput;
		VERIFYD3D11RESULT(SwapChain->GetFullscreenState(&bSwapChainFullscreenState,SwapChainOutput.GetInitReference()));
		if(bSwapChainFullscreenState != bIsFullscreen)
		{
			bIsValid = FALSE;
			
			// Minimize the window.
			::ShowWindow(WindowHandle,SW_FORCEMINIMIZE);
		}
	}

	// Present the back buffer to the viewport window.
	HRESULT Result = SwapChain->Present(bLockToVsync ? 1 : 0,0);

	// Detect a lost device.
	if(Result == DXGI_ERROR_DEVICE_REMOVED || Result == DXGI_ERROR_DEVICE_RESET || Result == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
	{
		// This variable is checked periodically by the main thread.
		D3DRHI->bDeviceRemoved = TRUE;
	}
	else
	{
		VERIFYD3D11RESULT(Result);
	}
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FD3D11DynamicRHI::CreateViewport(void* WindowHandle,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen)
{
	check( IsInGameThread() );
	return new FD3D11Viewport(this,(HWND)WindowHandle,SizeX,SizeY,bIsFullscreen);
}

void FD3D11DynamicRHI::ResizeViewport(FViewportRHIParamRef ViewportRHI,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen)
{
	DYNAMIC_CAST_D3D11RESOURCE(Viewport,Viewport);

	check( IsInGameThread() );
	Viewport->Resize(SizeX,SizeY,bIsFullscreen);
}

void FD3D11DynamicRHI::Tick( FLOAT DeltaTime )
{
	check( IsInGameThread() );

	// Check to see if the device has been removed.
	if ( bDeviceRemoved )
	{
		InitD3DDevice();
	}

	// Check if any swap chains have been invalidated.
	for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
	{
		Viewports(ViewportIndex)->ConditionalResetSwapChain(FALSE);
	}
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FD3D11DynamicRHI::BeginDrawingViewport(FViewportRHIParamRef ViewportRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(Viewport,Viewport);

	SCOPE_CYCLE_COUNTER(STAT_D3D11PresentTime);

	check(!DrawingViewport);
	DrawingViewport = Viewport;

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources();

	// Set the render target and viewport.
	RHISetRenderTarget(Viewport->GetBackBuffer(), FSurfaceRHIRef());
	RHISetViewport(0,0,0.0f,Viewport->GetSizeX(),Viewport->GetSizeY(),1.0f);

	// Skip timing events when using SLI, they will not be accurate anyway
	if (GNumActiveGPUsForRendering == 1)
	{
		GPUFrameTiming.StartTiming();
	}
}

void FD3D11DynamicRHI::EndDrawingViewport(FViewportRHIParamRef ViewportRHI,UBOOL bPresent,UBOOL bLockToVsync)
{
	DYNAMIC_CAST_D3D11RESOURCE(Viewport,Viewport);

	SCOPE_CYCLE_COUNTER(STAT_D3D11PresentTime);

	// Skip timing events when using SLI, they will not be accurate anyway
	if (GNumActiveGPUsForRendering == 1)
	{
		GPUFrameTiming.EndTiming();
	}

	extern DWORD GGPUFrameTime;
	// Skip timing events when using SLI, as they will block the GPU and we want maximum throughput
	// Stat unit GPU time is not accurate anyway with SLI
	if (GPUFrameTiming.IsSupported() && GNumActiveGPUsForRendering == 1)
	{
		QWORD GPUTiming = GPUFrameTiming.GetTiming();
		QWORD GPUFreq = GPUFrameTiming.GetTimingFrequency();
		GGPUFrameTime = appTrunc( DOUBLE(GPUTiming) / DOUBLE(GPUFreq) / GSecondsPerCycle );
	}
	else
	{
		GGPUFrameTime = 0;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}

	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	check(!bTrackingEvents || CurrentEventNodeFrame);

	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			GEmitDrawEvents = bOriginalGEmitDrawEvents;

			warnf(TEXT(""));
			warnf(TEXT(""));

			CurrentEventNodeFrame->DumpEventTree();

			GProfilingGPU = FALSE;
			bLatchedGProfilingGPU = FALSE;

			if (GEngine->GameViewport)
			{
				GEngine->GameViewport->Exec(TEXT("SCREENSHOT"), *GLog);
			}
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		//@todo this really detects any hitch, even one on the game thread.
		// it would be nice to restrict the test to stalls on D3D, but for now...
		// this needs to be out here because bTrackingEvents is false during the hitch debounce
		static DOUBLE LastTime = -1.0;
		DOUBLE Now = appSeconds();
		if (bTrackingEvents)
		{
			/** How long, in seconds a frame much be to be considered a hitch **/
			static const FLOAT HitchThreshold = .1f; //100ms
			FLOAT ThisTime = Now - LastTime;
			UBOOL bHitched = (ThisTime > HitchThreshold) && LastTime > 0.0 && CurrentEventNodeFrame;
			if (bHitched)
			{
				warnf(TEXT("*******************************************************************************"));
				warnf(TEXT("********** Hitch detected on CPU, frametime = %6.1fms"),ThisTime * 1000.0f);
				warnf(TEXT("*******************************************************************************"));

				for (INT Frame = 0; Frame < GPUHitchEventNodeFrames.Num(); Frame++)
				{
					warnf(TEXT(""));
					warnf(TEXT(""));
					warnf(TEXT("********** GPU Frame: Current - %d"),GPUHitchEventNodeFrames.Num() - Frame);
					GPUHitchEventNodeFrames(Frame).DumpEventTree();
				}
				warnf(TEXT(""));
				warnf(TEXT(""));
				warnf(TEXT("********** GPU Frame: Current"));
				CurrentEventNodeFrame->DumpEventTree();

				warnf(TEXT("*******************************************************************************"));
				warnf(TEXT("********** End Hitch GPU Profile"));
				warnf(TEXT("*******************************************************************************"));
				if (GEngine->GameViewport)
				{
					GEngine->GameViewport->Exec(TEXT("SCREENSHOT"), *GLog);
				}

				GPUHitchDebounce = 5; // don't trigger this again for a while
				GPUHitchEventNodeFrames.Empty(); // clear history
			}
			else if (CurrentEventNodeFrame) // this will be null for discarded frames while recovering from a recent hitch
			{
				/** How many old frames to buffer for hitch reports **/
				static const INT HitchHistorySize = 4;

				if (GPUHitchEventNodeFrames.Num() >= HitchHistorySize)
				{
					GPUHitchEventNodeFrames.Remove(0);
				}
				GPUHitchEventNodeFrames.AddRawItem(CurrentEventNodeFrame);
				CurrentEventNodeFrame = NULL;  // prevent deletion of this below; ke kept it in the history
			}
		}
		LastTime = Now;
	}

	bTrackingEvents = FALSE;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;

	check(DrawingViewport.GetReference() == Viewport);
	DrawingViewport = NULL;

	// Clear references the device might have to resources.
	CurrentDepthSurface = NULL;
	CurrentDepthStencilTarget = NULL;
	bCurrentDSTIsReadonly = FALSE;
	CurrentRenderTargets[0] = NULL;
	for(UINT RenderTargetIndex = 1;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
	{
		CurrentRenderTargets[RenderTargetIndex] = NULL;
	}
	bCurrentRenderTargetIsMultisample = FALSE;

	for(UINT TextureIndex = 0;TextureIndex < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;TextureIndex++)
	{
		ID3D11ShaderResourceView* NullView = NULL;
		Direct3DDeviceIMContext->PSSetShaderResources(TextureIndex,1,&NullView);
		Direct3DDeviceIMContext->VSSetShaderResources(TextureIndex,1,&NullView);
		Direct3DDeviceIMContext->HSSetShaderResources(TextureIndex,1,&NullView);
		Direct3DDeviceIMContext->DSSetShaderResources(TextureIndex,1,&NullView);
	}

	ID3D11RenderTargetView* RTArray[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for(UINT RenderTargetIndex = 0;RenderTargetIndex < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++RenderTargetIndex)
	{
		RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
	}
	Direct3DDeviceIMContext->OMSetRenderTargets(
		D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
		RTArray,
		CurrentDepthStencilTarget
		);

	Direct3DDeviceIMContext->VSSetShader(NULL,NULL,0);

	for(UINT StreamIndex = 0;StreamIndex < 16;StreamIndex++)
	{
		ID3D11Buffer* NullBuffer = NULL;
		UINT Strides = 0;
		UINT Offsets = 0;
		Direct3DDeviceIMContext->IASetVertexBuffers(StreamIndex,1,&NullBuffer,&Strides,&Offsets);
	}

	Direct3DDeviceIMContext->IASetIndexBuffer(NULL,DXGI_FORMAT_R16_UINT,0);
	Direct3DDeviceIMContext->PSSetShader(NULL,NULL,0);

	#if WITH_PANORAMA
		extern void appPanoramaRenderHookRender(void);
		// Allow G4WLive to render the Live Guide as needed (or toasts)
		appPanoramaRenderHookRender();
	#endif

	if(bPresent)
	{
		Viewport->Present(bLockToVsync);
	}

	// Don't wait on the GPU when using SLI, let the driver determine how many frames behind the GPU should be allowed to get
	if (GNumActiveGPUsForRendering == 1)
	{
		// Wait for the GPU to finish rendering the previous frame before finishing this frame.
		FrameSyncEvent.WaitForCompletion();
		FrameSyncEvent.IssueEvent();

		// If the input latency timer has been triggered, block until the GPU is completely
		// finished displaying this frame and calculate the delta time.
		if ( GInputLatencyTimer.RenderThreadTrigger )
		{
			FrameSyncEvent.WaitForCompletion();
			DWORD EndTime = appCycles();
			GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
			GInputLatencyTimer.RenderThreadTrigger = FALSE;
		}
	}

	CurrentEventNode = NULL;

	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GProfilingGPU;
	bLatchedGProfilingGPUHitches = GProfilingGPUHitches;
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = FALSE; // we do NOT permit an ordinary GPU profile during hitch profiles
	}

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches))
	{
		bOriginalGEmitDrawEvents = GEmitDrawEvents;
	}

	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--; 
		}
		else
		{
			GEmitDrawEvents = TRUE;  // thwart an attempt to turn this off on the game side
			bTrackingEvents = TRUE;
			CurrentEventNodeFrame = new FD3D11EventNodeFrame(this);
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		GEmitDrawEvents = bOriginalGEmitDrawEvents;
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;
}

/**
 * Determine if currently drawing the viewport
 *
 * @return TRUE if currently within a BeginDrawingViewport/EndDrawingViewport block
 */
UBOOL FD3D11DynamicRHI::IsDrawingViewport()
{
	return DrawingViewport != NULL;
}

void FD3D11DynamicRHI::BeginScene()
{
}

void FD3D11DynamicRHI::EndScene()
{
}

FSurfaceRHIRef FD3D11DynamicRHI::GetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(Viewport,Viewport);

	return Viewport->GetBackBuffer();
}

FSurfaceRHIRef FD3D11DynamicRHI::GetViewportDepthBuffer(FViewportRHIParamRef Viewport)
{
	//@TODO:
	return NULL;
}
