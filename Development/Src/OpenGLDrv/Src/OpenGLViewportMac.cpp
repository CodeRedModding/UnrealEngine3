/*=============================================================================
	OpenGLViewportMac.cpp: Mac specific OpenGL viewport RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

#if PLATFORM_MACOSX

#include "MacObjCWrapper.h"

FOpenGLViewport::FOpenGLViewport(FOpenGLDynamicRHI* InOpenGLRHI,void* InWindowHandle,UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen):
	OpenGLRHI(InOpenGLRHI),
	OpenGLView(NULL),
	FullscreenOpenGLView(NULL),
	RenderThreadGLContext(NULL),
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen),
	bIsValid(TRUE)
{
	DoubleWindow* Window = (DoubleWindow*)InWindowHandle;

	OpenGLView = MacCreateOpenGLView(this, SizeX, SizeY);
	MacAttachOpenGLViewToWindow(Window->MainWindow, OpenGLView);

	if (GMacOSXVersion < MacOSXVersion_Lion)
	{
		FullscreenOpenGLView = MacCreateFullscreenOpenGLView();
		MacAttachOpenGLViewToWindow(Window->FullscreenWindow, FullscreenOpenGLView);
	}

	if (!GUseThreadedRendering)
	{
		RenderThreadGLContext = MacGetOpenGLContextFromView(OpenGLView);
	}

	BackBuffer = CreateBackBufferSurface(InOpenGLRHI);
	OpenGLRHI->Init();

	MacShowWindow(Window->MainWindow, 1);	// we need to have it on screen to catch all events

	if (bIsFullscreen)
	{
		if (GMacOSXVersion < MacOSXVersion_Lion)
		{
			// we need to have it on screen to display all we want and cover other windows.
			// this window won't be able to become key window and receive events, so the other window will be doing that.
			MacShowWindow(Window->FullscreenWindow, 1);
		}
		else
		{
			MacLionToggleFullScreen(Window->MainWindow);
		}
	}

	MacShowCursorInViewport(OpenGLView,FALSE);
}

FOpenGLViewport::~FOpenGLViewport()
{
	if( FullscreenOpenGLView )
	{
		MacReleaseOpenGLView(FullscreenOpenGLView);
		FullscreenOpenGLView = NULL;
	}

	if( OpenGLView )
	{
		MacReleaseOpenGLView(OpenGLView);
		OpenGLView = NULL;
	}
}

void FOpenGLViewport::InitRenderThreadContext()
{
	if (RenderThreadGLContext)
	{
		DestroyRenderThreadContext();
	}
	RenderThreadGLContext = MacCreateOpenGLContext(OpenGLView, FullscreenOpenGLView, SizeX, SizeY, bIsFullscreen);
	MacMakeOpenGLContextCurrent(RenderThreadGLContext);
}

void FOpenGLViewport::DestroyRenderThreadContext()
{
	if (RenderThreadGLContext)
	{
		MacDestroyOpenGLContext(OpenGLView, RenderThreadGLContext);
		RenderThreadGLContext = NULL;
	}
}

extern void ReleaseOpenGLFramebuffersForSurface(FOpenGLDynamicRHI* Device, FSurfaceRHIParamRef SurfaceRHI);

void FOpenGLViewport::Resize(UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen)
{
	ReleaseOpenGLFramebuffersForSurface(OpenGLRHI, BackBuffer);

	BackBuffer.SafeRelease();
#if !OPENGL_USE_BLIT_FOR_BACK_BUFFER
	BackBufferTexture.SafeRelease();
#endif

	if( ( InSizeX != SizeX || InSizeY != SizeY ) && OpenGLView )
	{
		MacResizeOpenGLView(OpenGLView,InSizeX,InSizeY);
		SizeX = InSizeX;
		SizeY = InSizeY;
	}

	if( bInIsFullscreen != bIsFullscreen )
	{
		if (FullscreenOpenGLView)
		{
			MacShowWindowByView(FullscreenOpenGLView, bInIsFullscreen?1:0);
			MacUpdateGLContextByView(OpenGLView);
		}
		bIsFullscreen = bInIsFullscreen;
	}

	BackBuffer = CreateBackBufferSurface(OpenGLRHI);
}

void FOpenGLViewport::SwapBuffers()
{
	MacMakeOpenGLContextCurrent(RenderThreadGLContext);
	MacFlushGLBuffers(RenderThreadGLContext);
}

void FOpenGLDynamicRHI::GetSupportedResolution( UINT &Width, UINT &Height )
{
	const void *AllModes = MacGetDisplayModesArray();
	if (AllModes)
	{
		UINT ModesCount = MacGetDisplayModesCount(AllModes);
		UINT InitializedMode = FALSE;
		UINT BestWidth = 0;
		UINT BestHeight = 0;
		for (UINT ModeIndex = 0; ModeIndex < ModesCount; ModeIndex++)
		{
			UINT ModeWidth = MacGetDisplayModeWidth(AllModes, ModeIndex);
			UINT ModeHeight = MacGetDisplayModeHeight(AllModes, ModeIndex);
			UBOOL IsEqualOrBetterWidth = Abs((INT)ModeWidth - (INT)Width) <= Abs((INT)BestWidth - (INT)Width);
			UBOOL IsEqualOrBetterHeight = Abs((INT)ModeHeight - (INT)Height) <= Abs((INT)BestHeight - (INT)Height);
			if(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
			{
				BestWidth = ModeWidth;
				BestHeight = ModeHeight;
				InitializedMode = TRUE;
			}
		}
		MacReleaseDisplayModesArray(AllModes);
		check(InitializedMode);
		Width = BestWidth;
		Height = BestHeight;
	}
}

UBOOL FOpenGLDynamicRHI::GetAvailableResolutions(FScreenResolutionArray& Resolutions, UBOOL bIgnoreRefreshRate)
{
	const void *AllModes = MacGetDisplayModesArray();
	if (!AllModes)
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

	UINT ModeCount = MacGetDisplayModesCount(AllModes);
	for (UINT ModeIndex = 0; ModeIndex < ModeCount; ModeIndex++)
	{
		UINT ModeWidth = MacGetDisplayModeWidth(AllModes, ModeIndex);
		UINT ModeHeight = MacGetDisplayModeHeight(AllModes, ModeIndex);
		UINT ModeRefreshRate = MacGetDisplayModeRefreshRate(AllModes, ModeIndex);

		if (((INT)ModeWidth >= MinAllowableResolutionX) &&
			((INT)ModeWidth <= MaxAllowableResolutionX) &&
			((INT)ModeHeight >= MinAllowableResolutionY) &&
			((INT)ModeHeight <= MaxAllowableResolutionY)
			)
		{
			UBOOL bAddIt = TRUE;
			if (bIgnoreRefreshRate == FALSE)
			{
				if (((INT)ModeRefreshRate < MinAllowableRefreshRate) ||
					((INT)ModeRefreshRate > MaxAllowableRefreshRate)
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
					if ((CheckResolution.Width == ModeWidth) &&
						(CheckResolution.Height == ModeHeight))
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

				ScreenResolution.Width = ModeWidth;
				ScreenResolution.Height = ModeHeight;
				ScreenResolution.RefreshRate = ModeRefreshRate;
			}
		}
	}

	MacReleaseDisplayModesArray(AllModes);

	return TRUE;
}

#endif // PLATFORM_MACOSX
