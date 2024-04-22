/*================================================================================
	SplashScreen.cpp: Splash screen for game/editor startup
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "EnginePrivate.h"

#include "SplashScreen.h"


#if !CONSOLE && !PLATFORM_MACOSX

// From LaunchEngineLoop.cpp
extern INT GGameIcon;
extern INT GEditorIcon;


/**
 * Splash screen functions and static globals
 */

static HANDLE GSplashScreenThread = NULL;
static HBITMAP GSplashScreenBitmap = NULL;
static HWND GSplashScreenWnd = NULL; 
static TCHAR* GSplashScreenFileName;
static FString GSplashScreenText[ SplashTextType::NumTextTypes ];
static RECT GSplashScreenTextRects[ SplashTextType::NumTextTypes ];
static HFONT GSplashScreenSmallTextFontHandle = NULL;
static HFONT GSplashScreenNormalTextFontHandle = NULL;
static FCriticalSection GSplashScreenSynchronizationObject;


/**
 * Window's proc for splash screen
 */
LRESULT CALLBACK SplashScreenWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
    PAINTSTRUCT ps;

	switch( message )
	{
		case WM_PAINT:
			{
				hdc = BeginPaint(hWnd, &ps);

				// Draw splash bitmap
				DrawState(hdc, DSS_NORMAL, NULL, (LPARAM)GSplashScreenBitmap, 0, 0, 0, 0, 0, DST_BITMAP);

				{
					// Take a critical section since another thread may be trying to set the splash text
					FScopeLock ScopeLock( &GSplashScreenSynchronizationObject );

					// Draw splash text
					for( INT CurTypeIndex = 0; CurTypeIndex < SplashTextType::NumTextTypes; ++CurTypeIndex )
					{
						const FString& SplashText = GSplashScreenText[ CurTypeIndex ];
						const RECT& TextRect = GSplashScreenTextRects[ CurTypeIndex ];

						if( SplashText.Len() > 0 )
						{
							if( CurTypeIndex == SplashTextType::StartupProgress ||
								CurTypeIndex == SplashTextType::VersionInfo1 )
							{
								SelectObject( hdc, GSplashScreenNormalTextFontHandle );
							}
							else
							{
								SelectObject( hdc, GSplashScreenSmallTextFontHandle );
							}
							SetBkColor( hdc, 0x00000000 );
							SetBkMode( hdc, TRANSPARENT );

							RECT ClientRect;
							GetClientRect( hWnd, &ClientRect );

							// Draw background text passes
							const INT NumBGPasses = 8;
							for( INT CurBGPass = 0; CurBGPass < NumBGPasses; ++CurBGPass )
							{
								INT BGXOffset, BGYOffset;
								switch( CurBGPass )
								{
									default:
									case 0:	BGXOffset = -1; BGYOffset =  0; break;
									case 2:	BGXOffset = -1; BGYOffset = -1; break;
									case 3:	BGXOffset =  0; BGYOffset = -1; break;
									case 4:	BGXOffset =  1; BGYOffset = -1; break;
									case 5:	BGXOffset =  1; BGYOffset =  0; break;
									case 6:	BGXOffset =  1; BGYOffset =  1; break;
									case 7:	BGXOffset =  0; BGYOffset =  1; break;
									case 8:	BGXOffset = -1; BGYOffset =  1; break;
								}

								SetTextColor( hdc, 0x00000000 );
								TextOut(
									hdc,
									TextRect.left + BGXOffset,
									TextRect.top + BGYOffset,
									*SplashText,
									SplashText.Len() );
							}

							// Draw foreground text pass
							if( CurTypeIndex == SplashTextType::StartupProgress )
							{
								SetTextColor( hdc, RGB( 180, 180, 180 ) );
							}
							else if( CurTypeIndex == SplashTextType::VersionInfo1 )
							{
								SetTextColor( hdc, RGB( 240, 240, 240 ) );
							}
							else
							{
								SetTextColor( hdc, RGB( 160, 160, 160 ) );
							}
							TextOut(
								hdc,
								TextRect.left,
								TextRect.top,
								*SplashText,
								SplashText.Len() );
						}
					}
				}

				EndPaint(hWnd, &ps);
			}
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

/**
 * Splash screen thread entry function
 */
DWORD WINAPI StartSplashScreenThread( LPVOID unused )
{
    WNDCLASS wc;
	wc.style       = CS_HREDRAW | CS_VREDRAW; 
	wc.lpfnWndProc = (WNDPROC) SplashScreenWindowProc; 
	wc.cbClsExtra  = 0; 
	wc.cbWndExtra  = 0; 
	wc.hInstance   = hInstance; 

	wc.hIcon       = LoadIcon(hInstance, MAKEINTRESOURCE(GIsEditor ? GEditorIcon : GGameIcon));
	if(wc.hIcon == NULL)
	{
		wc.hIcon   = LoadIcon((HINSTANCE) NULL, IDI_APPLICATION); 
	}

	wc.hCursor     = LoadCursor((HINSTANCE) NULL, IDC_ARROW); 
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = TEXT("SplashScreenClass"); 
 
	if(!RegisterClass(&wc)) 
	{
		return 0; 
    } 

	// Load splash screen image, display it and handle all window's messages
	GSplashScreenBitmap = (HBITMAP) LoadImage(hInstance, (LPCTSTR)GSplashScreenFileName, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	if(GSplashScreenBitmap)
	{
		BITMAP bm;
		GetObject(GSplashScreenBitmap, sizeof(bm), &bm);
		INT ScreenPosX = (GetSystemMetrics(SM_CXSCREEN) - bm.bmWidth) / 2;
		INT ScreenPosY = (GetSystemMetrics(SM_CYSCREEN) - bm.bmHeight) / 2;

		// Force the editor splash screen to show up in the taskbar and alt-tab lists
		DWORD dwWindowStyle = (GIsEditor ? WS_EX_APPWINDOW : 0) | WS_EX_TOOLWINDOW;

		GSplashScreenWnd = CreateWindowEx(dwWindowStyle, wc.lpszClassName, TEXT("SplashScreen"), WS_BORDER|WS_POPUP,
			ScreenPosX, ScreenPosY, bm.bmWidth, bm.bmHeight, (HWND) NULL, (HMENU) NULL, hInstance, (LPVOID) NULL); 

		// Setup font
		{
			HFONT SystemFontHandle = ( HFONT )GetStockObject( DEFAULT_GUI_FONT );

			// Create small font
			{
				LOGFONT MyFont;
				appMemzero( &MyFont, sizeof( MyFont ) );
				GetObject( SystemFontHandle, sizeof( MyFont ), &MyFont );
				MyFont.lfHeight = 10;
				// MyFont.lfQuality = ANTIALIASED_QUALITY;
				GSplashScreenSmallTextFontHandle = CreateFontIndirect( &MyFont );
				if( GSplashScreenSmallTextFontHandle == NULL )
				{
					// Couldn't create font, so just use a system font
					GSplashScreenSmallTextFontHandle = SystemFontHandle;
				}
			}

			// Create normal font
			{
				LOGFONT MyFont;
				appMemzero( &MyFont, sizeof( MyFont ) );
				GetObject( SystemFontHandle, sizeof( MyFont ), &MyFont );
				MyFont.lfHeight = 12;
				// MyFont.lfQuality = ANTIALIASED_QUALITY;
				GSplashScreenNormalTextFontHandle = CreateFontIndirect( &MyFont );
				if( GSplashScreenNormalTextFontHandle == NULL )
				{
					// Couldn't create font, so just use a system font
					GSplashScreenNormalTextFontHandle = SystemFontHandle;
				}
			}
		}
		

		UBOOL bShowThirdPartyCopyrightInfo = FALSE;
#if UDK
		bShowThirdPartyCopyrightInfo = TRUE;
#endif
		// Setup bounds for version info text 1
		GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].top = bm.bmHeight - 20;
		GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].bottom = bm.bmHeight;
		GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].left = 10;
		GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].right = bm.bmWidth - 20;

		// Setup bounds for copyright info text
		if( GIsEditor )
		{
			GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].top = bm.bmHeight - 44;
			GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].bottom = bm.bmHeight - 34;
			GSplashScreenTextRects[ SplashTextType::ThirdPartyCopyrightInfo ].top = bm.bmHeight - 34;
			GSplashScreenTextRects[ SplashTextType::ThirdPartyCopyrightInfo ].bottom = bm.bmHeight - 24;
		}
		else
		{
			if( bShowThirdPartyCopyrightInfo )
			{
				GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].top = bm.bmHeight - 26;
				GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].bottom = bm.bmHeight - 16;
				GSplashScreenTextRects[ SplashTextType::ThirdPartyCopyrightInfo ].top = bm.bmHeight - 16;
				GSplashScreenTextRects[ SplashTextType::ThirdPartyCopyrightInfo ].bottom = bm.bmHeight - 6;
			}
			else
			{
				GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].top = bm.bmHeight - 16;
				GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].bottom = bm.bmHeight - 6;
			}
		}
		GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].left = 10;
		GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].right = bm.bmWidth - 20;
		GSplashScreenTextRects[ SplashTextType::ThirdPartyCopyrightInfo ].left = 10;
		GSplashScreenTextRects[ SplashTextType::ThirdPartyCopyrightInfo ].right = bm.bmWidth - 20;

		// Setup bounds for startup progress text
		GSplashScreenTextRects[ SplashTextType::StartupProgress ].top = bm.bmHeight - 20;
		GSplashScreenTextRects[ SplashTextType::StartupProgress ].bottom = bm.bmHeight;
		GSplashScreenTextRects[ SplashTextType::StartupProgress ].left = 10;
		GSplashScreenTextRects[ SplashTextType::StartupProgress ].right = bm.bmWidth - 20;


		// In the editor, we'll display loading info
		if( GIsEditor )
		{
			// Set initial startup progress info
			{
				appSetSplashText( SplashTextType::StartupProgress,
								  *LocalizeUnrealEd( TEXT( "SplashScreen_InitialStartupProgress" ) ) );
			}

			// Set version info
			{
#if _WIN64
				const FString PlatformBitsString( TEXT( "64" ) );
#else
				const FString PlatformBitsString( TEXT( "32" ) );
#endif

				FString RHIName = ShaderPlatformToText( GRHIShaderPlatform, TRUE, TRUE );

#if UDK
				const FString AppName = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UDKTitle_RHI_F" ), *PlatformBitsString, *RHIName ) );
#else
				FString GameName = GConfig->GetStr(TEXT("URL"), TEXT("GameName"), GEngineIni);

				const FString AppName = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UnrealEdTitle_RHI_F" ), *GameName, *PlatformBitsString, *RHIName ) );
#endif
				const FString VersionInfo1 = TEXT("Duke's Enormous Tool 2004 (Loading Please Wait...)"); //FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "SplashScreen_VersionInfo1_F" ) ), *AppName, GEngineVersion, GBuiltFromChangeList ) );
				appSetSplashText( SplashTextType::VersionInfo1, *VersionInfo1 );

				// Change the window text (which will be displayed in the taskbar)
				SetWindowText(GSplashScreenWnd, *AppName);
			}
		}
		

		// Only show copyright info on game splash
		if( !GIsEditor )
		{
			const FString CopyrightInfo = LocalizeGeneral( TEXT( "SplashScreen_CopyrightInfo" ), TEXT("Engine") );
			appSetSplashText( SplashTextType::CopyrightInfo, *CopyrightInfo );

			if( bShowThirdPartyCopyrightInfo )
			{
				// In UDK builds, we must also display third-party copyright info
				const FString ThirdPartyCopyrightInfo = LocalizeGeneral( TEXT( "SplashScreen_ThirdPartyCopyrightInfo" ), TEXT("Engine") );
				appSetSplashText( SplashTextType::ThirdPartyCopyrightInfo, *ThirdPartyCopyrightInfo );
			}
		}


		if (GSplashScreenWnd)
		{
			ShowWindow(GSplashScreenWnd, SW_SHOW); 
			UpdateWindow(GSplashScreenWnd); 
		 
			MSG message;
			while (GetMessage(&message, NULL, 0, 0))
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}

		DeleteObject(GSplashScreenBitmap);
		GSplashScreenBitmap = NULL;
	}

	UnregisterClass(wc.lpszClassName, hInstance);
    return 0; 
}

/**
 * Displays a splash window with the specified image.  The function does not use wxWidgets.  The splash
 * screen variables are stored in static global variables (SplashScreen*).  If the image file doesn't exist,
 * the function does nothing.
 *
 * @param	InSplashName	Name of splash screen file (and relative path if needed)
 */
void appShowSplash( const TCHAR* InSplashName )
{
	if( ParseParam(appCmdLine(),TEXT("NOSPLASH")) != TRUE )
	{
		// make sure a splash was found
		FString SplashPath;
		if (appGetSplashPath(InSplashName, SplashPath) == TRUE)
		{
			const FString Filepath = FString( appBaseDir() ) * SplashPath;
			GSplashScreenFileName = (TCHAR*) appMalloc(MAX_FILEPATH_LENGTH*sizeof(TCHAR));
			appStrncpy(GSplashScreenFileName, *Filepath, MAX_FILEPATH_LENGTH);
			GSplashScreenThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)StartSplashScreenThread, (LPVOID)NULL, 0, NULL);
		}
	}
}

/**
 * Destroys the splash window that was previously shown by appShowSplash(). If the splash screen isn't active,
 * it will do nothing.
 */
void appHideSplash()
{
	if(GSplashScreenThread)
	{
		if(GSplashScreenWnd)
		{
			// Send message to splash screen window to destroy itself
			PostMessage(GSplashScreenWnd, WM_DESTROY, 0, 0);
		}

		// Wait for splash screen thread to finish
		WaitForSingleObject(GSplashScreenThread, INFINITE);

		// Clean up
		CloseHandle(GSplashScreenThread);
		appFree(GSplashScreenFileName);
		GSplashScreenFileName = NULL; 
		GSplashScreenThread = NULL;
		GSplashScreenWnd = NULL;
	}
}



/**
 * Sets the text displayed on the splash screen (for startup/loading progress)
 *
 * @param	InType		Type of text to change
 * @param	InText		Text to display
 */
void appSetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
{
// jmarshall - this was too noisy
	if(InType == SplashTextType::StartupProgress)
		return;
// jmarshall end

	// We only want to bother drawing startup progress in the editor, since this information is
	// not interesting to an end-user (also, it's not usually localized properly.)
	if( GSplashScreenThread )
	{
		// Only allow copyright text displayed while loading the game.  Editor displays all.
		if( InType == SplashTextType::CopyrightInfo || InType == SplashTextType::ThirdPartyCopyrightInfo || GIsEditor )
		{
			{
				// Take a critical section since the splash thread may already be repainting using this text
				FScopeLock ScopeLock( &GSplashScreenSynchronizationObject );

				// Update splash text
				GSplashScreenText[ InType ] = InText;
			}

			// Repaint the window
			InvalidateRect( GSplashScreenWnd, &GSplashScreenTextRects[ InType ], FALSE );
		}
	}
}


#endif