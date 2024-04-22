/*=============================================================================
	D3D9VertexBuffer.cpp: D3D viewport RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"
#if WITH_APEX
#include "..\Src\NvApexRender.h"
#endif

/**
 * Present sometimes returns the following error. Treating the error as a lost
 * device works around the situation.
 * https://udn.epicgames.com/lists/showpost.php?list=unprog3&id=66391
 */
#define D3DERR_UNKOWN_MAYBE_DEVICELOST MAKE_D3DHRESULT(2162)

//
// Globals.
//

FD3D9Viewport::FD3D9Viewport(FD3D9DynamicRHI* InD3DRHI,void* InWindowHandle,UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen):
	D3DRHI(InD3DRHI),
	WindowHandle(InWindowHandle),
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen)
{
	D3DRHI->Viewports.AddItem(this);
	D3DRHI->UpdateD3DDeviceFromViewports();
}

FD3D9Viewport::~FD3D9Viewport()
{
	D3DRHI->Viewports.RemoveItem(this);
	D3DRHI->UpdateD3DDeviceFromViewports();
}

void FD3D9Viewport::Resize(UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	bIsFullscreen = bInIsFullscreen;
	D3DRHI->UpdateD3DDeviceFromViewports();
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FD3D9DynamicRHI::CreateViewport(void* WindowHandle,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen)
{
	check( IsInGameThread() );
	return new FD3D9Viewport(this,WindowHandle,SizeX,SizeY,bIsFullscreen);
}

void FD3D9DynamicRHI::ResizeViewport(FViewportRHIParamRef ViewportRHI,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen)
{
	DYNAMIC_CAST_D3D9RESOURCE(Viewport,Viewport);

	check( IsInGameThread() );
	Viewport->Resize(SizeX,SizeY,bIsFullscreen);
}

void FD3D9DynamicRHI::Tick( FLOAT DeltaTime )
{
	check( IsInGameThread() );

	// Check to see if the device has been lost.
	if ( bDeviceLost )
	{
		UpdateD3DDeviceFromViewports();
	}
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FD3D9DynamicRHI::BeginDrawingViewport(FViewportRHIParamRef ViewportRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(Viewport,Viewport);

	SCOPE_CYCLE_COUNTER(STAT_D3D9PresentTime);

	check(!DrawingViewport);
	DrawingViewport = Viewport;

	// Tell D3D we're going to start rendering.
	Direct3DDevice->BeginScene();

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources();

	// Set the configured D3D render state.
	for(INT SamplerIndex = 0;SamplerIndex < 16;SamplerIndex++)
	{
		Direct3DDevice->SetSamplerState(SamplerIndex,D3DSAMP_MAXANISOTROPY,Clamp(GSystemSettings.MaxAnisotropy,1,16));
	}

	// Set the render target and viewport.
	SetRenderTarget(BackBuffer, FSurfaceRHIRef());

	GPUFrameTiming.StartTiming();
}

/** Reset all pixel shader texture references, to ensure a reference to this render target doesn't remain set. */
void FD3D9DynamicRHI::UnsetPSTextures()
{
	for(UINT TextureIndex = 0;TextureIndex < 16;TextureIndex++)
	{
		Direct3DDevice->SetTexture(TextureIndex,NULL);
	}
}

/** Reset all vertex shader texture references, to ensure a reference to this render target doesn't remain set. */
void FD3D9DynamicRHI::UnsetVSTextures()
{
	for(UINT TextureIndex = D3DVERTEXTEXTURESAMPLER0; TextureIndex <= D3DVERTEXTEXTURESAMPLER3; TextureIndex++)
	{
		Direct3DDevice->SetTexture(TextureIndex,NULL);
	}
}

void FD3D9DynamicRHI::EndDrawingViewport(FViewportRHIParamRef ViewportRHI,UBOOL bPresent,UBOOL bLockToVsync)
{
	DYNAMIC_CAST_D3D9RESOURCE(Viewport,Viewport);

	SCOPE_CYCLE_COUNTER(STAT_D3D9PresentTime);

	GPUFrameTiming.EndTiming();

	extern DWORD GGPUFrameTime;
	if ( GPUFrameTiming.IsSupported() )
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

	check(!bTrackingEvents || bLatchedGProfilingGPU);
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

	bTrackingEvents = FALSE;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;

	check(DrawingViewport.GetReference() == Viewport);
	DrawingViewport = NULL;

	// Clear references the device might have to resources.
	Direct3DDevice->SetRenderTarget(0,*BackBuffer);
	Direct3DDevice->SetDepthStencilSurface(NULL);

	UnsetPSTextures();
	UnsetVSTextures();

	Direct3DDevice->SetVertexShader(NULL);

	ResetVertexStreams();

	Direct3DDevice->SetIndices(NULL);
	Direct3DDevice->SetPixelShader(NULL);

	#if WITH_PANORAMA
		extern void appPanoramaRenderHookRender(void);
		// Allow G4WLive to render the Live Guide as needed (or toasts)
		appPanoramaRenderHookRender();
	#endif

	// Tell D3D we're done rendering.
	Direct3DDevice->EndScene();

	if(bPresent)
	{
		// Present the back buffer to the viewport window.
		HRESULT Result = S_OK;
		if(Viewport->IsFullscreen())
		{
			Result = Direct3DDevice->Present(NULL,NULL,NULL,NULL);
		}
		else
		{
			RECT DestRect;
			if(GetClientRect((HWND)Viewport->GetWindowHandle(),&DestRect))
			{		
				RECT SourceRect;
				SourceRect.left		= SourceRect.top = 0;
				SourceRect.right	= Viewport->GetSizeX();
				SourceRect.bottom	= Viewport->GetSizeY();

				// Only present to the viewport if its client area isn't zero-sized.
				if(DestRect.right > 0 && DestRect.bottom > 0)
				{
					Result = Direct3DDevice->Present(&SourceRect,NULL,(HWND)Viewport->GetWindowHandle(),NULL);
				}
			}
		}

		// Detect a lost device.
		if(Result == D3DERR_DEVICELOST || Result == E_FAIL || Result == D3DERR_UNKOWN_MAYBE_DEVICELOST)
		{
			// This variable is checked periodically by the main thread.
			bDeviceLost = TRUE;
		}
		else
		{
#if WITH_APEX
			GApexRender->SetRequireRewriteBuffers(FALSE);
#endif
			VERIFYD3D9RESULT(Result);
		}
	}

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

	CurrentEventNode = NULL;

	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GProfilingGPU;

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU)
	{
		bOriginalGEmitDrawEvents = GEmitDrawEvents;

		GEmitDrawEvents = TRUE;  // thwart an attempt to turn this off on the game side
		bTrackingEvents = TRUE;
		CurrentEventNodeFrame = new FD3D9EventNodeFrame(this);
		CurrentEventNodeFrame->StartFrame();
	}
}

/**
 * Determine if currently drawing the viewport
 *
 * @return TRUE if currently within a BeginDrawingViewport/EndDrawingViewport block
 */
UBOOL FD3D9DynamicRHI::IsDrawingViewport()
{
	return DrawingViewport != NULL;
}

void FD3D9DynamicRHI::BeginScene()
{
	// Tell D3D we're going to start rendering.
	Direct3DDevice->BeginScene();
}

void FD3D9DynamicRHI::EndScene()
{
	// Tell D3D we're done rendering.
	Direct3DDevice->EndScene();
}

FSurfaceRHIRef FD3D9DynamicRHI::GetViewportBackBuffer(FViewportRHIParamRef Viewport)
{
	return BackBuffer.GetReference();
}

FSurfaceRHIRef FD3D9DynamicRHI::GetViewportDepthBuffer(FViewportRHIParamRef Viewport)
{
	//@TODO:
	return NULL;
}
