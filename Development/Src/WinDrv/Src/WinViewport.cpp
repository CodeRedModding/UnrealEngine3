/*=============================================================================
	WinViewport.cpp: FWindowsViewport code.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* Rewritten by Andrew Scheidecker and Daniel Vogel
=============================================================================*/

#include "WinDrv.h"

#define WM_MOUSEWHEEL 0x020A

//
//	FWindowsViewport::FWindowsViewport
//
FWindowsViewport::FWindowsViewport(UWindowsClient* InClient,FViewportClient* InViewportClient,const TCHAR* InName,UINT InSizeX,UINT InSizeY,UBOOL InFullscreen,HWND InParentWindow)
{
	Client					= InClient;
	ViewportClient			= InViewportClient;

	Name					= InName;
	Window					= NULL;
	ParentWindow			= InParentWindow;
	
	InitializedRenderDevice = 0;
	bCapturingMouseInput	= 0;
	bCapturingJoystickInput = 1;
	bLockingMouseToWindow	= 0;

	Minimized				= 0;
	Maximized				= 0;
	Resizing				= 0;
	
	Client->Viewports.AddItem(this);

	// Creates the viewport window.
	Resize(InSizeX,InSizeY,InFullscreen);

	// Set as active window.
	::SetActiveWindow(Window);

	// Set initial key state.
	for(UINT KeyIndex = 0;KeyIndex < 256;KeyIndex++)
	{
		FName*	KeyName = Client->KeyMapVirtualToName.Find(KeyIndex);

		if(KeyName && *KeyName != KEY_LeftMouseButton && *KeyName != KEY_RightMouseButton && *KeyName != KEY_MiddleMouseButton)
			KeyStates[KeyIndex] = ::GetKeyState(KeyIndex) & 0x8000;
	}

}

//
//	FWindowsViewport::Destroy
//

void FWindowsViewport::Destroy()
{
	ViewportClient = NULL;

	if( InitializedRenderDevice )
	{
		Client->RenderDevice->DestroyViewport(this);
		InitializedRenderDevice = 0;
	}

	CaptureMouseInput(0);

	DestroyWindow(Window);
}

//
//	FWindowsViewport::Resize
//
void FWindowsViewport::Resize(UINT NewSizeX,UINT NewSizeY,UBOOL NewFullscreen)
{
	static UBOOL ReEntrant = 0;

	if( ReEntrant )
	{
		debugf(NAME_Error,TEXT("ReEntrant == 1, FWindowsViewport::Resize"));
		appDebugBreak();
		return;
	}

	ReEntrant				= 1;

	SizeX					= NewSizeX;
	SizeY					= NewSizeY;
	Fullscreen				= NewFullscreen;

	// Figure out physical window size we must specify to get appropriate client area.

	DWORD	WindowStyle;
	INT		PhysWidth		= NewSizeX,
			PhysHeight		= NewSizeY;

	if( ParentWindow )
	{
		WindowStyle			= WS_CHILD | WS_CLIPSIBLINGS;
		Fullscreen			= 0;
	}
	else
	{
		WindowStyle	= WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

		if(!Fullscreen) // Hide the caption bar for fullscreen windows.
			WindowStyle |= WS_THICKFRAME | WS_CAPTION;

		RECT  WindowRect;
		WindowRect.left		= 100;
		WindowRect.top		= 100;
		WindowRect.right	= NewSizeX + 100;
		WindowRect.bottom	= NewSizeY + 100;

		::AdjustWindowRect(&WindowRect,WindowStyle,0);

		PhysWidth			= WindowRect.right  - WindowRect.left,
		PhysHeight			= WindowRect.bottom - WindowRect.top;
	}

	if( Window == NULL )
	{
		// Obtain width and height of primary monitor.
		INT ScreenWidth  = ::GetSystemMetrics( SM_CXSCREEN );
		INT ScreenHeight = ::GetSystemMetrics( SM_CYSCREEN );
		
		// Create the window in the upper left quadrant.
		Window = CreateWindowExX( WS_EX_APPWINDOW, *Client->WindowClassName, *Name, WindowStyle, (ScreenWidth - PhysWidth) / 6, (ScreenHeight - PhysHeight) / 6, PhysWidth, PhysHeight, ParentWindow, NULL, hInstance, this );
		verify( Window );
	}

	if(ParentWindow)
		SetWindowLongX(Window,GWL_STYLE,WS_POPUP | WS_VISIBLE); // AJS says: ??? Editor viewports don't work without this.
	else
		SetWindowLongX(Window,GWL_STYLE,WindowStyle);

	// Initialize the viewport's render device.

	if( NewSizeX && NewSizeY )
	{
		if( InitializedRenderDevice )
		{
			InitializedRenderDevice = 0;
			Client->RenderDevice->ResizeViewport(this);
			InitializedRenderDevice = 1;
		}
		else
		{
			Client->RenderDevice->CreateViewport(this);
			InitializedRenderDevice = 1;
		}
	}
	else
	{
		if( InitializedRenderDevice )
		{
			InitializedRenderDevice = 0;
			Client->RenderDevice->DestroyViewport(this);
		}
	}

	if( !ParentWindow && !NewFullscreen )
	{
		// Resize viewport window.

		RECT WindowRect;
		::GetWindowRect( Window, &WindowRect );

		RECT ScreenRect;
		ScreenRect.left		= ::GetSystemMetrics( SM_XVIRTUALSCREEN );
		ScreenRect.top		= ::GetSystemMetrics( SM_YVIRTUALSCREEN );
		ScreenRect.right	= ::GetSystemMetrics( SM_CXVIRTUALSCREEN );
		ScreenRect.bottom	= ::GetSystemMetrics( SM_CYVIRTUALSCREEN );

		if( WindowRect.left >= ScreenRect.right-4 || WindowRect.left < ScreenRect.left - 4 )
			WindowRect.left = ScreenRect.left;
		if( WindowRect.top >= ScreenRect.bottom-4 || WindowRect.top < ScreenRect.top - 4 )
			WindowRect.top = ScreenRect.top;

		::SetWindowPos( Window, HWND_TOP, WindowRect.left, WindowRect.top, PhysWidth, PhysHeight, SWP_NOSENDCHANGING | SWP_NOZORDER );
	}

	// Show the viewport.

	::ShowWindow( Window, SW_SHOW );
	::UpdateWindow( Window );

	ReEntrant = 0;
}

//
//	FWindowsViewport::ShutdownAfterError - Minimalist shutdown.
//
void FWindowsViewport::ShutdownAfterError()
{
	if(Window)
	{
		::DestroyWindow(Window);
		Window = NULL;
	}
}

//
//	FWindowsViewport::CaptureInput
//
UBOOL FWindowsViewport::CaptureMouseInput(UBOOL Capture)
{
	Capture = Capture && ::GetForegroundWindow() == Window && ::GetFocus() == Window;

	if(Capture && !bCapturingMouseInput)
	{
		// Store current mouse position and capture mouse.
		::GetCursorPos(&PreCaptureMousePos);
		::SetCapture(Window);

		// Hider mouse cursor.
		while( ::ShowCursor(FALSE)>=0 );

		// Clip mouse to window.
		LockMouseToWindow(true);

		// Clear mouse input buffer.
		if( SUCCEEDED( UWindowsClient::DirectInput8Mouse->Acquire() ) )
		{
			DIDEVICEOBJECTDATA	Event;
			DWORD				Elements = 1;
			while( SUCCEEDED( UWindowsClient::DirectInput8Mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),&Event,&Elements,0) ) && (Elements > 0) );
		}

		bCapturingMouseInput = 1;
	}
	else if(!Capture && bCapturingMouseInput)
	{
		bCapturingMouseInput = 0;

		// No longer confine mouse to window.
		LockMouseToWindow(false);

		// Show mouse cursor.
		while( ::ShowCursor(TRUE)<0 );

		// Restore old mouse position and release capture.
		::SetCursorPos(PreCaptureMousePos.x,PreCaptureMousePos.y);
		::ReleaseCapture();
	}

	return bCapturingMouseInput;
}

void FWindowsViewport::LockMouseToWindow(UBOOL bLock)
{
	if(bLock && !bLockingMouseToWindow)
	{
		// Store the old cursor clip rectangle.
		::GetClipCursor(&PreCaptureCursorClipRect);

		RECT R;
		::GetClientRect( Window, &R );
		::MapWindowPoints( Window, NULL, (POINT*)&R, 2 );
		::ClipCursor(&R);

		bLockingMouseToWindow = true;

		//debugf( TEXT("LOCK MOUSE: (%d %d) (%d %d)  SAVED: (%d %d) (%d %d)"), R.left, R.top, R.right, R.bottom, PreCaptureCursorClipRect.left, PreCaptureCursorClipRect.top, PreCaptureCursorClipRect.right, PreCaptureCursorClipRect.bottom );
	}
	else if(!bLock && bLockingMouseToWindow)
	{
		//debugf( TEXT("UNLOCK MOUSE: (%d %d) (%d %d)"), PreCaptureCursorClipRect.left, PreCaptureCursorClipRect.top, PreCaptureCursorClipRect.right, PreCaptureCursorClipRect.bottom );

		::ClipCursor(&PreCaptureCursorClipRect);

		bLockingMouseToWindow = false;
	}
}

//
//	FWindowsViewport::CaptureJoystickInput
//
UBOOL FWindowsViewport::CaptureJoystickInput(UBOOL Capture)
{
	bCapturingJoystickInput	= Capture;

	return bCapturingJoystickInput;
}

//
//	FWindowsViewport::KeyState
//
UBOOL FWindowsViewport::KeyState(FName Key)
{
	BYTE* VirtualKey = Client->KeyMapNameToVirtual.Find(Key);

	if( VirtualKey )
		return ::GetKeyState(*VirtualKey) & 0x8000;
	else
		return 0;

}

//
//	FWindowsViewport::GetMouseX
//
INT FWindowsViewport::GetMouseX()
{
	POINT P;
	::GetCursorPos( &P );
	::ScreenToClient( Window, &P );
	return P.x;
}

//
//	FWindowsViewport::GetMouseY
//
INT FWindowsViewport::GetMouseY()
{
	POINT P;
	::GetCursorPos( &P );
	::ScreenToClient( Window, &P );
	return P.y;
}

//
//	FWindowsViewport::Draw
//

void FWindowsViewport::Draw(UBOOL Synchronous)
{
	if(InitializedRenderDevice)
		Client->RenderDevice->DrawViewport(this,Synchronous);
}

//
//	FWindowsViewport::InvalidateDisplay
//

void FWindowsViewport::InvalidateDisplay()
{
	::InvalidateRect(Window,NULL,0);
}

//
//	FWindowsViewport::GetHitProxyMap
//

void FWindowsViewport::GetHitProxyMap(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<HHitProxy*>& OutMap)
{
	if(InitializedRenderDevice)
		Client->RenderDevice->GetHitProxyMap(this,MinX,MinY,MaxX,MaxY,OutMap);
	else	
	{
		check(MaxX >= MinX && MaxY >= MinY);
		OutMap.Empty((MaxX - MinX + 1) * (MaxY - MinY + 1));
		OutMap.AddZeroed((MaxX - MinX + 1) * (MaxY - MinY + 1));
	}
}

//
//	FWindowsViewport::Invalidate
//

void FWindowsViewport::Invalidate()
{
	if(InitializedRenderDevice)
		Client->RenderDevice->InvalidateHitProxies(this);
	InvalidateDisplay();
}

//
//	FWindowsViewport::ReadPixels
//

UBOOL FWindowsViewport::ReadPixels(TArray<FColor>& OutputBuffer)
{
	if(InitializedRenderDevice)
	{
		OutputBuffer.Empty(SizeX * SizeY);
		OutputBuffer.Add(SizeX * SizeY);
		Client->RenderDevice->ReadPixels(this,&OutputBuffer(0));
		return 1;
	}
	else
		return 0;
}

//
//	FWindowsViewport::SetName
//
void FWindowsViewport::SetName(const TCHAR* NewName)
{
	if( Window )
	{
		Name = NewName;
		SendMessageLX( Window, WM_SETTEXT, 0, NewName );
	}
}

//
//	FWindowsViewport::ProcessInput
//
void FWindowsViewport::ProcessInput( FLOAT DeltaTime )
{
	if(!ViewportClient)
		return;

	// Process mouse input.

	if( bCapturingMouseInput )
	{
		if( SUCCEEDED( UWindowsClient::DirectInput8Mouse->Acquire() ) )
		{
			UWindowsClient::DirectInput8Mouse->Poll();
			
			DIDEVICEOBJECTDATA	Event;
			DWORD				Elements = 1;
			while( SUCCEEDED( UWindowsClient::DirectInput8Mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),&Event,&Elements,0) ) && (Elements > 0) )
			{
				switch( Event.dwOfs ) 
				{
				case DIMOFS_X: 
					ViewportClient->InputAxis(this,KEY_MouseX,(INT)Event.dwData,DeltaTime);
					bMouseHasMoved = 1;
					break;
				case DIMOFS_Y: 
					ViewportClient->InputAxis(this,KEY_MouseY,-(INT)Event.dwData,DeltaTime);
					bMouseHasMoved = 1;
					break; 
				case DIMOFS_Z:
					if( ((INT)Event.dwData) < 0 )
					{
						ViewportClient->InputKey(this,KEY_MouseScrollDown,IE_Pressed);
						ViewportClient->InputKey(this,KEY_MouseScrollDown,IE_Released);
					}
					else if( ((INT)Event.dwData) > 0)
					{
						ViewportClient->InputKey(this,KEY_MouseScrollUp,IE_Pressed);
						ViewportClient->InputKey(this,KEY_MouseScrollUp,IE_Released);
					}
					break;
				default:
					break;
				}
			}
		}
	}

#ifdef EMULATE_CONSOLE_CONTROLLER
	if( bCapturingJoystickInput && UWindowsClient::DirectInput8Joystick && SUCCEEDED(UWindowsClient::DirectInput8Joystick->Acquire()) && SUCCEEDED(UWindowsClient::DirectInput8Joystick->Poll()) )
	{
		DIJOYSTATE2 State;
		if( SUCCEEDED( UWindowsClient::DirectInput8Joystick->GetDeviceState( sizeof(DIJOYSTATE2), &State ) ))
		{
			if( UWindowsClient::JoystickType == JOYSTICK_Xbox_Type_S )
			{
				// Buttons (treated as digital buttons).
				ViewportClient->InputKey( this, KEY_XboxTypeS_A					, State.rgbButtons[  0 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_B					, State.rgbButtons[  1 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_X					, State.rgbButtons[  2 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_Y					, State.rgbButtons[  3 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_Black				, State.rgbButtons[  4 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_White				, State.rgbButtons[  5 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_Start				, State.rgbButtons[  6 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_Back				, State.rgbButtons[  7 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_LeftThumbstick	, State.rgbButtons[  8 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_RightThumbstick	, State.rgbButtons[  9 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_LeftTrigger		, State.rgbButtons[ 10 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_RightTrigger		, State.rgbButtons[ 11 ] & 0x80 ? IE_Pressed : IE_Released );
				
				// Digital pad (treated as POV hat).
				ViewportClient->InputKey( this, KEY_XboxTypeS_DPad_Up			, State.rgdwPOV[0] == 0		? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_DPad_Down			, State.rgdwPOV[0] == 18000 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_DPad_Left			, State.rgdwPOV[0] == 27000 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_DPad_Right		, State.rgdwPOV[0] == 9000	? IE_Pressed : IE_Released );

				// Axis, convert range 0..65536 set up in EnumAxesCallback to +/- 1.
				ViewportClient->InputAxis( this, KEY_XboxTypeS_LeftX	, (State.lX  - 32768.f) / 32768.f, DeltaTime );
				ViewportClient->InputAxis( this, KEY_XboxTypeS_LeftY	, (State.lY  - 32768.f) / 32768.f, DeltaTime );
				ViewportClient->InputAxis( this, KEY_XboxTypeS_RightX	, (State.lRx - 32768.f) / 32768.f, DeltaTime );
				ViewportClient->InputAxis( this, KEY_XboxTypeS_RightY	, (State.lRy - 32768.f) / 32768.f, DeltaTime );
			}
			else if( UWindowsClient::JoystickType == JOYSTICK_PS2 )
			{
				// Buttons (treated as digital buttons).
				ViewportClient->InputKey( this, KEY_XboxTypeS_Y					, State.rgbButtons[  0 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_B					, State.rgbButtons[  1 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_A					, State.rgbButtons[  2 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_X					, State.rgbButtons[  3 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_White				, State.rgbButtons[  4 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_Black				, State.rgbButtons[  5 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_LeftTrigger		, State.rgbButtons[  6 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_RightTrigger		, State.rgbButtons[  7 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_Back				, State.rgbButtons[  8 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_Start				, State.rgbButtons[  9 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_LeftThumbstick	, State.rgbButtons[ 10 ] & 0x80 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_RightThumbstick	, State.rgbButtons[ 11 ] & 0x80 ? IE_Pressed : IE_Released );

				// Digital pad (treated as POV hat).
				ViewportClient->InputKey( this, KEY_XboxTypeS_DPad_Up			, State.rgdwPOV[0] == 0		? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_DPad_Down			, State.rgdwPOV[0] == 18000 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_DPad_Left			, State.rgdwPOV[0] == 27000 ? IE_Pressed : IE_Released );
				ViewportClient->InputKey( this, KEY_XboxTypeS_DPad_Right		, State.rgdwPOV[0] == 9000	? IE_Pressed : IE_Released );

				// Axis, convert range 0..65536 set up in EnumAxesCallback to +/- 1.
				ViewportClient->InputAxis( this, KEY_XboxTypeS_LeftX	, (State.lX  - 32768.f) / 32768.f, DeltaTime );
				ViewportClient->InputAxis( this, KEY_XboxTypeS_LeftY	, (State.lY  - 32768.f) / 32768.f, DeltaTime );
				ViewportClient->InputAxis( this, KEY_XboxTypeS_RightX	, (State.lRz - 32768.f) / 32768.f, DeltaTime );
				ViewportClient->InputAxis( this, KEY_XboxTypeS_RightY	, (State.lZ  - 32768.f) / 32768.f, DeltaTime );
			}
		}
#endif
	}

	// Process keyboard input.

	for( UINT KeyIndex = 0;KeyIndex < 256;KeyIndex++ )
	{
		FName* KeyName = Client->KeyMapVirtualToName.Find(KeyIndex);

		if(  KeyName 
		&&  *KeyName != KEY_LeftMouseButton 
		&&  *KeyName != KEY_RightMouseButton 
		&&  *KeyName != KEY_MiddleMouseButton 
		)
		{
			UBOOL NewKeyState = ::GetKeyState(KeyIndex) & 0x8000;

			if( !NewKeyState && KeyStates[KeyIndex] )
			{
				ViewportClient->InputKey(this,*KeyName,IE_Released);
				KeyStates[KeyIndex] = 0;
			}
		}
	}
}

//
//	FWindowsViewport::HandlePossibleSizeChange
//
void FWindowsViewport::HandlePossibleSizeChange()
{
	RECT	WindowClientRect;
	::GetClientRect( Window, &WindowClientRect );

	UINT	NewSizeX = WindowClientRect.right - WindowClientRect.left,
			NewSizeY = WindowClientRect.bottom - WindowClientRect.top;

	if(!Fullscreen && (NewSizeX != SizeX || NewSizeY != SizeY))
	{
		Resize( NewSizeX, NewSizeY, 0 );

		if(ViewportClient)
			ViewportClient->ReceivedFocus(this);
	}
}

//
//	FWindowsViewport::ViewportWndProc - Main viewport window function.
//
LONG FWindowsViewport::ViewportWndProc( UINT Message, WPARAM wParam, LPARAM lParam )
{
	if( !Client->ProcessWindowsMessages || Client->Viewports.FindItemIndex(this) == INDEX_NONE )
	{
		return DefWindowProcX(Window,Message,wParam,lParam);
	}

	// Message handler.
	switch(Message)
	{
		case WM_DESTROY:
			Window = NULL;
			return 0;

		case WM_CLOSE:
			if( ViewportClient )
				ViewportClient->CloseRequested(this);
			return 0;

		case WM_PAINT:
			if(ViewportClient)
				ViewportClient->RedrawRequested(this);
			break;

		case WM_MOUSEACTIVATE:
			if(LOWORD(lParam) != HTCLIENT)
			{
				// The activation was the result of a mouse click outside of the client area of the window.
				// Prevent the mouse from being captured to allow the user to drag the window around.
				PreventCapture = 1;
			}
			return MA_ACTIVATE;

		case WM_SETFOCUS:
			UWindowsClient::DirectInput8Mouse->Unacquire();
			UWindowsClient::DirectInput8Mouse->SetCooperativeLevel(Window,DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
			if( UWindowsClient::DirectInput8Joystick )
			{
				UWindowsClient::DirectInput8Joystick->Unacquire();
				UWindowsClient::DirectInput8Joystick->SetCooperativeLevel(Window,DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
			}
			if(ViewportClient && !PreventCapture)
				ViewportClient->ReceivedFocus(this);
			return 0;

		case WM_KILLFOCUS:
			if(ViewportClient)
				ViewportClient->LostFocus(this);
			return 0;

		case WM_DISPLAYCHANGE:
			if(bCapturingMouseInput)
			{
				CaptureMouseInput(0);
				CaptureMouseInput(1);
			}
			return 0;

		case WM_GETMINMAXINFO:
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 160;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 120;
			break;

		case WM_MOUSEMOVE:
			if( !bCapturingMouseInput )
			{
				INT	X = LOWORD(lParam),
					Y = HIWORD(lParam);

				if(ViewportClient)
					ViewportClient->MouseMove(this,X,Y);
				bMouseHasMoved = 1;
			}
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			{
				// Distinguish between left/right control/shift/alt.
				UBOOL ExtendedKey = (lParam & (1 << 24));
				UINT KeyCode;
				switch(wParam)
				{
				case VK_CONTROL: KeyCode = ExtendedKey ? VK_RCONTROL : VK_LCONTROL; break;
				case VK_SHIFT: KeyCode = ExtendedKey ? VK_RSHIFT : VK_LSHIFT; break;
				case VK_MENU: KeyCode = ExtendedKey ? VK_RMENU : VK_LMENU; break;
				default: KeyCode = wParam; break;
				};

				// Get key code.
				FName* Key = Client->KeyMapVirtualToName.Find(KeyCode);

				if(!Key)
					break;

				// Send key to input system.
				if( *Key==KEY_Enter && (::GetKeyState(VK_MENU)&0x8000) )
				{
					Resize(SizeX,SizeY,!Fullscreen);
					CaptureMouseInput(1);
				}
				else if( ViewportClient )
				{
					ViewportClient->InputKey(this,*Key,KeyStates[KeyCode] ? IE_Repeat : IE_Pressed);
					KeyStates[KeyCode] = 1;
				}
			}
			return 0;
	
		case WM_KEYUP:
		case WM_SYSKEYUP:
			{
				// Distinguish between left/right control/shift/alt.
				UBOOL ExtendedKey = (lParam & (1 << 24));
				UINT KeyCode;
				switch(wParam)
				{
				case VK_CONTROL: KeyCode = ExtendedKey ? VK_RCONTROL : VK_LCONTROL; break;
				case VK_SHIFT: KeyCode = ExtendedKey ? VK_RSHIFT : VK_LSHIFT; break;
				case VK_MENU: KeyCode = ExtendedKey ? VK_RMENU : VK_LMENU; break;
				default: KeyCode = wParam; break;
				};

				// Get key code.
				FName* Key = Client->KeyMapVirtualToName.Find(KeyCode);

				if( !Key )
					break;

				// Send key to input system.
				if( ViewportClient )
				{
					ViewportClient->InputKey(this,*Key,IE_Released);
					KeyStates[KeyCode] = 0;
				}
			}
			return 0;

		case WM_CHAR:
			{
				TCHAR Character = GUnicodeOS ? wParam : TCHAR(wParam);
				if(ViewportClient)
					ViewportClient->InputChar(this,Character);
			}
			return 0;

		case WM_ERASEBKGND:
			return 1;

		case WM_SETCURSOR:
			if( Fullscreen )
			{
				SetCursor( NULL );
				return 1;
			}
			else
			{
				RECT	ClientRect;
				INT		MouseX		= GetMouseX(),
						MouseY		= GetMouseY();
				::GetClientRect( Window, &ClientRect );
			
				EMouseCursor	Cursor = MC_Arrow;
				if(ViewportClient)
					Cursor = ViewportClient->GetCursor(this,MouseX,MouseY);

				LPCTSTR	CursorResource = IDC_ARROW;
				switch(Cursor)
				{
				case MC_Arrow:				CursorResource = IDC_ARROW; break;
				case MC_Cross:				CursorResource = IDC_CROSS; break;
				case MC_SizeAll:			CursorResource = IDC_SIZEALL; break;
				case MC_SizeUpRightDownLeft:CursorResource = IDC_SIZENESW; break;
				case MC_SizeUpLeftDownRight:CursorResource = IDC_SIZENWSE; break;
				case MC_SizeLeftRight:		CursorResource = IDC_SIZEWE; break;
				case MC_SizeUpDown:			CursorResource = IDC_SIZENS; break;
				};

				::SetCursor(::LoadCursor(NULL,CursorResource));
					return 1;
				}
			break;

		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
			{		
				FName Key;
				if( Message == WM_LBUTTONDBLCLK )
					Key = KEY_LeftMouseButton;
				else
				if( Message == WM_MBUTTONDBLCLK )
					Key = KEY_MiddleMouseButton;
				else 
				if( Message == WM_RBUTTONDBLCLK )
					Key = KEY_RightMouseButton;
				
				if(ViewportClient)
					ViewportClient->InputKey(this,Key,IE_DoubleClick);
			}			
			bMouseHasMoved = 0;
			return 0;
	
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			{
				FName Key;
				if( Message == WM_LBUTTONDOWN )
					Key = KEY_LeftMouseButton;
				else
				if( Message == WM_MBUTTONDOWN )
					Key = KEY_MiddleMouseButton;
				else
				if( Message == WM_RBUTTONDOWN )
					Key = KEY_RightMouseButton;
			
				if(ViewportClient)
					ViewportClient->InputKey(this,Key,IE_Pressed);
			}
			bMouseHasMoved = 0;
			return 0;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
			{
				FName Key;

				if(PreventCapture)
					PreventCapture = 0;

				if( Message == WM_LBUTTONUP )
					Key = KEY_LeftMouseButton;
				else if( Message == WM_MBUTTONUP )
					Key = KEY_MiddleMouseButton;
				else if( Message == WM_RBUTTONUP )
					Key = KEY_RightMouseButton;

				if(ViewportClient)
					ViewportClient->InputKey(this,Key,IE_Released);
			}
			return 0;

		case WM_MOUSEWHEEL:
			if(!bCapturingMouseInput)
			{
				if(ViewportClient)
				{
					SWORD zDelta = HIWORD(wParam);
					if(zDelta > 0)
					{
						ViewportClient->InputKey(this, KEY_MouseScrollUp, IE_Pressed);
						ViewportClient->InputKey(this, KEY_MouseScrollUp, IE_Released);
					}
					else if(zDelta < 0)
					{
						ViewportClient->InputKey(this, KEY_MouseScrollDown, IE_Pressed);
						ViewportClient->InputKey(this, KEY_MouseScrollDown, IE_Released);
					}
				}
			}
			break;

		case WM_ENTERSIZEMOVE:
			Resizing = 1;
			CaptureMouseInput(0);
			break;

		case WM_EXITSIZEMOVE:
			Resizing = 0;
			HandlePossibleSizeChange();
			break;

		case WM_SIZE:
			if(bCapturingMouseInput)
			{
				CaptureMouseInput(0);
				CaptureMouseInput(1);
			}
			// Show mouse cursor if we're being minimized.
			if( SIZE_MINIMIZED == wParam )
			{
				CaptureMouseInput( 0 );
				Minimized = true;
				Maximized = false;
			}
			else if( SIZE_MAXIMIZED == wParam )
			{
				Minimized = false;
				Maximized = true;
				HandlePossibleSizeChange();
			}
			else if( SIZE_RESTORED == wParam )
			{
				if( Maximized )
				{
					Maximized = false;
					HandlePossibleSizeChange();
				}
				else if( Minimized)
				{
					Minimized = false;
					HandlePossibleSizeChange();
				}
				else
				{
					// Game:
					// If we're neither maximized nor minimized, the window size 
					// is changing by the user dragging the window edges.  In this 
					// case, we don't reset the device yet -- we wait until the 
					// user stops dragging, and a WM_EXITSIZEMOVE message comes.
					if(!Resizing)
						HandlePossibleSizeChange();
				}
			}
			break;

		case WM_SYSCOMMAND:
			// Prevent moving/sizing and power loss in fullscreen mode.
			switch( wParam )
			{
				case SC_SCREENSAVE:
					return 1;

				case SC_MOVE:
				case SC_SIZE:
				case SC_MAXIMIZE:
				case SC_KEYMENU:
				case SC_MONITORPOWER:
					if( Fullscreen )
						return 1;
					break;
			}
			break;

		case WM_NCHITTEST:
			// Prevent the user from selecting the menu in fullscreen mode.
			if( Fullscreen )
				return HTCLIENT;
			break;

		case WM_POWER:
			if( PWR_SUSPENDREQUEST == wParam )
			{
				debugf( NAME_Log, TEXT("Received WM_POWER suspend") );
				return PWR_FAIL;
			}
			else
				debugf( NAME_Log, TEXT("Received WM_POWER") );
			break;

		default:
			break;
	}
	return DefWindowProcX( Window, Message, wParam, lParam );
}

