/*=============================================================================
	OpenGLViewport.cpp: Windows specific OpenGL viewport RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

#if _WINDOWS

FOpenGLViewport::FOpenGLViewport(FOpenGLDynamicRHI* InOpenGLRHI,void* InWindowHandle,UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen):
	OpenGLRHI(InOpenGLRHI),
	WindowHandle((HWND)InWindowHandle),
	GLContext(NULL),
	GLRenderContext(NULL),
	RenderThreadGLContext(NULL),
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen),
	bIsValid(TRUE)
{
	GLContext = GetDC(WindowHandle);

	PIXELFORMATDESCRIPTOR FormatDesc;
	memset(&FormatDesc, 0, sizeof(PIXELFORMATDESCRIPTOR));
	FormatDesc.nSize		= sizeof(PIXELFORMATDESCRIPTOR);
	FormatDesc.nVersion		= 1;
	FormatDesc.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	FormatDesc.iPixelType	= PFD_TYPE_RGBA;
	FormatDesc.cColorBits	= 32;
	FormatDesc.cDepthBits	= 0;
	FormatDesc.cStencilBits	= 0;
	FormatDesc.iLayerType	= PFD_MAIN_PLANE;

	INT PixelFormat = ChoosePixelFormat(GLContext, &FormatDesc);
	if (PixelFormat && SetPixelFormat(GLContext, PixelFormat, &FormatDesc))
	{
		GLRenderContext = wglCreateContext(GLContext);
		if (GLRenderContext && wglMakeCurrent(GLContext, GLRenderContext))
		{
			RenderThreadGLContext = wglCreateContext(GLContext);
			if (RenderThreadGLContext && wglShareLists(GLRenderContext, RenderThreadGLContext))
			{
				LoadWindowsOpenGL();

				BackBuffer = CreateBackBufferSurface(InOpenGLRHI);

				OpenGLRHI->Init();

				Resize(InSizeX, InSizeY, bInIsFullscreen);

				// Restore the window.
				::ShowWindow(WindowHandle,SW_RESTORE);
			}
		}
	}
}

FOpenGLViewport::~FOpenGLViewport()
{
	if (bIsFullscreen)
	{
		ChangeDisplaySettings(NULL, 0);
	}

	if (GLRenderContext)
	{
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(GLRenderContext);
	}

	if (GLContext)
	{
		ReleaseDC(WindowHandle, GLContext);
	}
}

void FOpenGLViewport::InitRenderThreadContext()
{
	if (!wglMakeCurrent(GLContext, RenderThreadGLContext))
	{
		DWORD Error = GetLastError();
		debugf(TEXT("Error activaing shared OpenGL context (0x%x)"), Error);
	}
}

void FOpenGLViewport::DestroyRenderThreadContext()
{
	wglMakeCurrent(NULL, NULL);
}

extern void ReleaseOpenGLFramebuffersForSurface(FOpenGLDynamicRHI* Device, FSurfaceRHIParamRef SurfaceRHI);

void FOpenGLViewport::Resize(UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen)
{
	// Framebuffer resources are per-context, even with wglShareLists!
	// Swap to the render thread context.
	wglMakeCurrent(GLContext, RenderThreadGLContext);

	ReleaseOpenGLFramebuffersForSurface(OpenGLRHI, BackBuffer);

	BackBuffer.SafeRelease();
#if !OPENGL_USE_BLIT_FOR_BACK_BUFFER
	BackBufferTexture.SafeRelease();
#endif

	DWORD WindowStyle = WS_CAPTION | WS_SYSMENU;
	DWORD WindowStyleEx = 0;
	HWND InsertAfter = HWND_NOTOPMOST;
	RECT WindowRect = {0, 0, InSizeX, InSizeY};

	if (bInIsFullscreen)
	{
		DEVMODE Mode;
		Mode.dmSize = sizeof(DEVMODE);
		Mode.dmBitsPerPel = 32;
		Mode.dmPelsWidth = InSizeX;
		Mode.dmPelsHeight = InSizeY;
		Mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		ChangeDisplaySettings(&Mode, CDS_FULLSCREEN);

		WindowStyle = WS_POPUP;
		WindowStyleEx = WS_EX_APPWINDOW | WS_EX_TOPMOST;
		InsertAfter = HWND_TOPMOST;
	}
	else if (bIsFullscreen)
	{
		ChangeDisplaySettings(NULL, 0);
	}

	SetWindowLong(WindowHandle, GWL_STYLE, WindowStyle);
	SetWindowLong(WindowHandle, GWL_EXSTYLE, WindowStyleEx);

	AdjustWindowRectEx(&WindowRect, WindowStyle, FALSE, WindowStyleEx);

	// Center the window
	int ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	SetWindowPos(WindowHandle, InsertAfter,
				 (ScreenWidth - InSizeX) / 2,
				 (ScreenHeight - InSizeY) / 2,
				 (WindowRect.right - WindowRect.left),
				 (WindowRect.bottom - WindowRect.top),
				 0);

	SizeX = InSizeX;
	SizeY = InSizeY;
	bIsFullscreen = bInIsFullscreen;

	// Needed after above windows calls?
	wglMakeCurrent(GLContext, RenderThreadGLContext);
	BackBuffer = CreateBackBufferSurface(OpenGLRHI);

	// Swap back to the game thread context.
	wglMakeCurrent(GLContext, GLRenderContext);
}

void FOpenGLViewport::SwapBuffers()
{
	::SwapBuffers(GLContext);
}

void FOpenGLDynamicRHI::GetSupportedResolution( UINT &Width, UINT &Height )
{
	UINT InitializedMode = FALSE;
	UINT BestWidth = 0;
	UINT BestHeight = 0;
	UINT ModeIndex = 0;
	DEVMODE DisplayMode;

	while(EnumDisplaySettings(NULL, ModeIndex++, &DisplayMode))
	{
		UBOOL IsEqualOrBetterWidth = Abs((INT)DisplayMode.dmPelsWidth - (INT)Width) <= Abs((INT)BestWidth - (INT)Width);
		UBOOL IsEqualOrBetterHeight = Abs((INT)DisplayMode.dmPelsHeight - (INT)Height) <= Abs((INT)BestHeight - (INT)Height);
		if(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
		{
			BestWidth = DisplayMode.dmPelsWidth;
			BestHeight = DisplayMode.dmPelsHeight;
			InitializedMode = TRUE;
		}
	}
	check(InitializedMode);
	Width = BestWidth;
	Height = BestHeight;
}

UBOOL FOpenGLDynamicRHI::GetAvailableResolutions(FScreenResolutionArray& Resolutions, UBOOL bIgnoreRefreshRate)
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

	UINT ModeIndex = 0;
	DEVMODE DisplayMode;

	while(EnumDisplaySettings(NULL, ModeIndex++, &DisplayMode))
	{
		if (((INT)DisplayMode.dmPelsWidth >= MinAllowableResolutionX) &&
			((INT)DisplayMode.dmPelsWidth <= MaxAllowableResolutionX) &&
			((INT)DisplayMode.dmPelsHeight >= MinAllowableResolutionY) &&
			((INT)DisplayMode.dmPelsHeight <= MaxAllowableResolutionY)
			)
		{
			UBOOL bAddIt = TRUE;
			if (bIgnoreRefreshRate == FALSE)
			{
				if (((INT)DisplayMode.dmDisplayFrequency < MinAllowableRefreshRate) ||
					((INT)DisplayMode.dmDisplayFrequency > MaxAllowableRefreshRate)
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
					if ((CheckResolution.Width == DisplayMode.dmPelsWidth) &&
						(CheckResolution.Height == DisplayMode.dmPelsHeight))
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

				ScreenResolution.Width = DisplayMode.dmPelsWidth;
				ScreenResolution.Height = DisplayMode.dmPelsHeight;
				ScreenResolution.RefreshRate = DisplayMode.dmDisplayFrequency;
			}
		}
	}

	return TRUE;
}

#endif // _WINDOWS
