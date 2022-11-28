/*=============================================================================
	WinClient.cpp: UWindowsClient code.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* Rewritten by Andrew Scheidecker and Daniel Vogel
=============================================================================*/

#include "WinDrv.h"
#include "..\..\Launch\Resources\resource.h"

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UWindowsClient);

/*-----------------------------------------------------------------------------
	UWindowsClient implementation.
-----------------------------------------------------------------------------*/

//
// UWindowsClient constructor.
//
UWindowsClient::UWindowsClient()
{
	KeyMapVirtualToName.Set(VK_LBUTTON,KEY_LeftMouseButton);
	KeyMapVirtualToName.Set(VK_RBUTTON,KEY_RightMouseButton);
	KeyMapVirtualToName.Set(VK_MBUTTON,KEY_MiddleMouseButton);

	KeyMapVirtualToName.Set(VK_BACK,KEY_BackSpace);
	KeyMapVirtualToName.Set(VK_TAB,KEY_Tab);
	KeyMapVirtualToName.Set(VK_RETURN,KEY_Enter);
	KeyMapVirtualToName.Set(VK_PAUSE,KEY_Pause);

	KeyMapVirtualToName.Set(VK_CAPITAL,KEY_CapsLock);
	KeyMapVirtualToName.Set(VK_ESCAPE,KEY_Escape);
	KeyMapVirtualToName.Set(VK_SPACE,KEY_SpaceBar);
	KeyMapVirtualToName.Set(VK_PRIOR,KEY_PageUp);
	KeyMapVirtualToName.Set(VK_NEXT,KEY_PageDown);
	KeyMapVirtualToName.Set(VK_END,KEY_End);
	KeyMapVirtualToName.Set(VK_HOME,KEY_Home);

	KeyMapVirtualToName.Set(VK_LEFT,KEY_Left);
	KeyMapVirtualToName.Set(VK_UP,KEY_Up);
	KeyMapVirtualToName.Set(VK_RIGHT,KEY_Right);
	KeyMapVirtualToName.Set(VK_DOWN,KEY_Down);

	KeyMapVirtualToName.Set(VK_INSERT,KEY_Insert);
	KeyMapVirtualToName.Set(VK_DELETE,KEY_Delete);

	KeyMapVirtualToName.Set(0x30,KEY_Zero);
	KeyMapVirtualToName.Set(0x31,KEY_One);
	KeyMapVirtualToName.Set(0x32,KEY_Two);
	KeyMapVirtualToName.Set(0x33,KEY_Three);
	KeyMapVirtualToName.Set(0x34,KEY_Four);
	KeyMapVirtualToName.Set(0x35,KEY_Five);
	KeyMapVirtualToName.Set(0x36,KEY_Six);
	KeyMapVirtualToName.Set(0x37,KEY_Seven);
	KeyMapVirtualToName.Set(0x38,KEY_Eight);
	KeyMapVirtualToName.Set(0x39,KEY_Nine);

	KeyMapVirtualToName.Set(0x41,KEY_A);
	KeyMapVirtualToName.Set(0x42,KEY_B);
	KeyMapVirtualToName.Set(0x43,KEY_C);
	KeyMapVirtualToName.Set(0x44,KEY_D);
	KeyMapVirtualToName.Set(0x45,KEY_E);
	KeyMapVirtualToName.Set(0x46,KEY_F);
	KeyMapVirtualToName.Set(0x47,KEY_G);
	KeyMapVirtualToName.Set(0x48,KEY_H);
	KeyMapVirtualToName.Set(0x49,KEY_I);
	KeyMapVirtualToName.Set(0x4A,KEY_J);
	KeyMapVirtualToName.Set(0x4B,KEY_K);
	KeyMapVirtualToName.Set(0x4C,KEY_L);
	KeyMapVirtualToName.Set(0x4D,KEY_M);
	KeyMapVirtualToName.Set(0x4E,KEY_N);
	KeyMapVirtualToName.Set(0x4F,KEY_O);
	KeyMapVirtualToName.Set(0x50,KEY_P);
	KeyMapVirtualToName.Set(0x51,KEY_Q);
	KeyMapVirtualToName.Set(0x52,KEY_R);
	KeyMapVirtualToName.Set(0x53,KEY_S);
	KeyMapVirtualToName.Set(0x54,KEY_T);
	KeyMapVirtualToName.Set(0x55,KEY_U);
	KeyMapVirtualToName.Set(0x56,KEY_V);
	KeyMapVirtualToName.Set(0x57,KEY_W);
	KeyMapVirtualToName.Set(0x58,KEY_X);
	KeyMapVirtualToName.Set(0x59,KEY_Y);
	KeyMapVirtualToName.Set(0x5A,KEY_Z);

	KeyMapVirtualToName.Set(VK_NUMPAD0,KEY_NumPadZero);
	KeyMapVirtualToName.Set(VK_NUMPAD1,KEY_NumPadOne);
	KeyMapVirtualToName.Set(VK_NUMPAD2,KEY_NumPadTwo);
	KeyMapVirtualToName.Set(VK_NUMPAD3,KEY_NumPadThree);
	KeyMapVirtualToName.Set(VK_NUMPAD4,KEY_NumPadFour);
	KeyMapVirtualToName.Set(VK_NUMPAD5,KEY_NumPadFive);
	KeyMapVirtualToName.Set(VK_NUMPAD6,KEY_NumPadSix);
	KeyMapVirtualToName.Set(VK_NUMPAD7,KEY_NumPadSeven);
	KeyMapVirtualToName.Set(VK_NUMPAD8,KEY_NumPadEight);
	KeyMapVirtualToName.Set(VK_NUMPAD9,KEY_NumPadNine);

	KeyMapVirtualToName.Set(VK_MULTIPLY,KEY_Multiply);
	KeyMapVirtualToName.Set(VK_ADD,KEY_Add);
	KeyMapVirtualToName.Set(VK_SUBTRACT,KEY_Subtract);
	KeyMapVirtualToName.Set(VK_DECIMAL,KEY_Decimal);
	KeyMapVirtualToName.Set(VK_DIVIDE,KEY_Divide);

	KeyMapVirtualToName.Set(VK_F1,KEY_F1);
	KeyMapVirtualToName.Set(VK_F2,KEY_F2);
	KeyMapVirtualToName.Set(VK_F3,KEY_F3);
	KeyMapVirtualToName.Set(VK_F4,KEY_F4);
	KeyMapVirtualToName.Set(VK_F5,KEY_F5);
	KeyMapVirtualToName.Set(VK_F6,KEY_F6);
	KeyMapVirtualToName.Set(VK_F7,KEY_F7);
	KeyMapVirtualToName.Set(VK_F8,KEY_F8);
	KeyMapVirtualToName.Set(VK_F9,KEY_F9);
	KeyMapVirtualToName.Set(VK_F10,KEY_F10);
	KeyMapVirtualToName.Set(VK_F11,KEY_F11);
	KeyMapVirtualToName.Set(VK_F12,KEY_F12);

	KeyMapVirtualToName.Set(VK_NUMLOCK,KEY_NumLock);

	KeyMapVirtualToName.Set(VK_SCROLL,KEY_ScrollLock);

	KeyMapVirtualToName.Set(VK_LSHIFT,KEY_LeftShift);
	KeyMapVirtualToName.Set(VK_RSHIFT,KEY_RightShift);
	KeyMapVirtualToName.Set(VK_LCONTROL,KEY_LeftControl);
	KeyMapVirtualToName.Set(VK_RCONTROL,KEY_RightControl);
	KeyMapVirtualToName.Set(VK_LMENU,KEY_LeftAlt);
	KeyMapVirtualToName.Set(VK_RMENU,KEY_RightAlt);

	KeyMapVirtualToName.Set(VK_OEM_1,KEY_Semicolon);
	KeyMapVirtualToName.Set(VK_OEM_PLUS,KEY_Equals);
	KeyMapVirtualToName.Set(VK_OEM_COMMA,KEY_Comma);
	KeyMapVirtualToName.Set(VK_OEM_MINUS,KEY_Underscore);
	KeyMapVirtualToName.Set(VK_OEM_PERIOD,KEY_Period);
	KeyMapVirtualToName.Set(VK_OEM_2,KEY_Slash);
	KeyMapVirtualToName.Set(VK_OEM_3,KEY_Tilde);
	KeyMapVirtualToName.Set(VK_OEM_4,KEY_LeftBracket);
	KeyMapVirtualToName.Set(VK_OEM_5,KEY_Backslash);
	KeyMapVirtualToName.Set(VK_OEM_6,KEY_RightBracket);
	KeyMapVirtualToName.Set(VK_OEM_7,KEY_Quote);

	for(UINT KeyIndex = 0;KeyIndex < 256;KeyIndex++)
		if(KeyMapVirtualToName.Find(KeyIndex))
			KeyMapNameToVirtual.Set(KeyMapVirtualToName.FindRef(KeyIndex),KeyIndex);

	AudioDevice				= NULL;
	RenderDevice			= NULL;
	ProcessWindowsMessages	= 1;
}

//
// Static init.
//
void UWindowsClient::StaticConstructor()
{
	new(GetClass(),TEXT("RenderDeviceClass"),RF_Public)UClassProperty(CPP_PROPERTY(RenderDeviceClass)	,TEXT("Display"),CPF_Config,URenderDevice::StaticClass());
	new(GetClass(),TEXT("AudioDeviceClass")	,RF_Public)UClassProperty(CPP_PROPERTY(AudioDeviceClass)	,TEXT("Audio")	,CPF_Config,UAudioDevice::StaticClass());
}

//
// Initialize the platform-specific viewport manager subsystem.
// Must be called after the Unreal object manager has been initialized.
// Must be called before any viewports are created.
//
BOOL CALLBACK EnumJoysticksCallback( LPCDIDEVICEINSTANCE Instance, LPVOID Context );
BOOL CALLBACK EnumAxesCallback( LPCDIDEVICEOBJECTINSTANCE Instance, LPVOID Context );

/** Resource ID of icon to use for Window */
extern INT GGameIcon;

void UWindowsClient::Init( UEngine* InEngine )
{
	Engine = InEngine;

	// Register windows class.
	WindowClassName = FString(GPackage) + TEXT("Unreal") + TEXT("UWindowsClient");
#if UNICODE
	if( GUnicodeOS )
	{
		WNDCLASSEXW Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize			= sizeof(Cls);
		Cls.style			= GIsEditor ? (CS_DBLCLKS|CS_OWNDC) : (CS_OWNDC);
		Cls.lpfnWndProc		= StaticWndProc;
		Cls.hInstance		= hInstance;
		Cls.hIcon			= LoadIconIdX(hInstance,GGameIcon);
		Cls.lpszClassName	= *WindowClassName;
		Cls.hIconSm			= LoadIconIdX(hInstance,GGameIcon);
		verify(RegisterClassExW( &Cls ));
	}
	else
#endif
	{
		WNDCLASSEXA Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize			= sizeof(Cls);
		Cls.style			= GIsEditor ? (CS_DBLCLKS|CS_OWNDC): (CS_OWNDC);
		Cls.lpfnWndProc		= StaticWndProc;
		Cls.hInstance		= hInstance;
		Cls.hIcon			= LoadIconIdX(hInstance,GGameIcon);
		Cls.lpszClassName	= TCHAR_TO_ANSI(*WindowClassName);
		Cls.hIconSm			= LoadIconIdX(hInstance,GGameIcon);
		verify(RegisterClassExA( &Cls ));
	}

	// Initialize the render device.
	RenderDevice = CastChecked<URenderDevice>(StaticConstructObject(RenderDeviceClass));
	RenderDevice->Init();

	// Initialize the audio device.
	if( GEngine->UseSound )
	{
		AudioDevice = CastChecked<UAudioDevice>(StaticConstructObject(AudioDeviceClass));
		if( !AudioDevice->Init() )
		{
			delete AudioDevice;
			AudioDevice = NULL;
		}
	}

	// Initialize shared DirectInput mouse interface.
	verify( SUCCEEDED( DirectInput8Create( hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&DirectInput8, NULL ) ) );
	verify( SUCCEEDED( DirectInput8->CreateDevice( GUID_SysMouse, &DirectInput8Mouse, NULL ) ) );
	verify( SUCCEEDED( DirectInput8Mouse->SetDataFormat(&c_dfDIMouse) ) );

	DIPROPDWORD Property;
	Property.diph.dwSize		= sizeof(DIPROPDWORD);
	Property.diph.dwHeaderSize	= sizeof(DIPROPHEADER);
	Property.diph.dwObj			= 0;
	Property.diph.dwHow			= DIPH_DEVICE;
	Property.dwData				= 1023;	// buffer size
	verify( SUCCEEDED( DirectInput8Mouse->SetProperty(DIPROP_BUFFERSIZE,&Property.diph) ) );
 	Property.dwData				= DIPROPAXISMODE_REL;
	verify( SUCCEEDED( DirectInput8Mouse->SetProperty(DIPROP_AXISMODE,&Property.diph) ) );

#ifdef EMULATE_CONSOLE_CONTROLLER
	JoystickType = JOYSTICK_None;

	HRESULT hr;
	if( SUCCEEDED( hr = DirectInput8->EnumDevices( DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY )) && DirectInput8Joystick )
	{
		DIDEVCAPS JoystickCaps;
		JoystickCaps.dwSize = sizeof(JoystickCaps);

		verify( SUCCEEDED( DirectInput8Joystick->SetDataFormat( &c_dfDIJoystick2 ) ) );
		verify( SUCCEEDED( DirectInput8Joystick->GetCapabilities( &JoystickCaps ) ) );
		verify( SUCCEEDED( DirectInput8Joystick->EnumObjects( EnumAxesCallback, NULL, DIDFT_AXIS ) ) );
	
		JoystickType = JOYSTICK_None;

		// Xbox Type S controller with old drivers.
		if( JoystickCaps.dwAxes		== 4 
		&&	JoystickCaps.dwButtons	== 16 
		&&	JoystickCaps.dwPOVs		== 1 )
		{
			JoystickType = JOYSTICK_Xbox_Type_S;
		}

		// Xbox Type S controller with new drivers.
		if( JoystickCaps.dwAxes		== 7
		&&	JoystickCaps.dwButtons	== 24 
		&&	JoystickCaps.dwPOVs		== 1
		)
		{
			JoystickType = JOYSTICK_Xbox_Type_S;
		}

		// PS2 controller.
		if( JoystickCaps.dwAxes		== 5
		&&	JoystickCaps.dwButtons	== 16 
		&&	JoystickCaps.dwPOVs		== 1
		)
		{
			JoystickType = JOYSTICK_PS2;
		}

		if( JoystickType == JOYSTICK_None )
		{
			DirectInput8Joystick->Release();
			DirectInput8Joystick = NULL;
		}
	}

	if( DirectInput8Joystick == NULL )
		debugf(NAME_Warning,TEXT("Console controller emulation enabled though no compatible joystick found"));
#endif

	// Success.
	debugf( NAME_Init, TEXT("Client initialized") );
}

//
//	UWindowsClient::Flush
//
void UWindowsClient::Flush()
{
	RenderDevice->Flush();
	if( AudioDevice )
		AudioDevice->Flush();
}

//
//	UWindowsClient::Serialize - Make sure objects the client reference aren't garbage collected.
//
void UWindowsClient::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Engine << RenderDeviceClass << RenderDevice << AudioDeviceClass << AudioDevice;
}

//
//	UWindowsClient::Destroy - Shut down the platform-specific viewport manager subsystem.
//
void UWindowsClient::Destroy()
{
	// Close all viewports.

	for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
		delete Viewports(ViewportIndex);
	Viewports.Empty();

	// Stop capture.
	SetCapture( NULL );
	ClipCursor( NULL );

	// Delete the render device.
	delete RenderDevice;
	RenderDevice = NULL;

	// Delete the audio device.
	delete AudioDevice;
	AudioDevice = NULL;

	SAFE_RELEASE( DirectInput8Joystick );
	SAFE_RELEASE( DirectInput8Mouse );
	SAFE_RELEASE( DirectInput8 );

	debugf( NAME_Exit, TEXT("Windows client shut down") );
	Super::Destroy();
}

//
// Failsafe routine to shut down viewport manager subsystem
// after an error has occured. Not guarded.
//
void UWindowsClient::ShutdownAfterError()
{
	debugf( NAME_Exit, TEXT("Executing UWindowsClient::ShutdownAfterError") );

	SetCapture( NULL );
	ClipCursor( NULL );
	while( ShowCursor(TRUE)<0 );
	for(UINT ViewportIndex = 0;ViewportIndex < (UINT)Viewports.Num();ViewportIndex++)
		Viewports(ViewportIndex)->ShutdownAfterError();

	Super::ShutdownAfterError();
}

//
//	UWindowsClient::Tick - Perform timer-tick processing on all visible viewports.
//
void UWindowsClient::Tick( FLOAT DeltaTime )
{
	// Process input.
	BEGINCYCLECOUNTER(GEngineStats.InputTime);
	for(UINT ViewportIndex = 0;ViewportIndex < (UINT)Viewports.Num();ViewportIndex++)
	{
		Viewports(ViewportIndex)->ProcessInput( DeltaTime );
	}
	ENDCYCLECOUNTER;

	// Cleanup viewports that have been destroyed.

	for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
	{
		if(!Viewports(ViewportIndex)->Window)
		{
			delete Viewports(ViewportIndex);
			Viewports.Remove(ViewportIndex--);
		}
	}
}

//
//	UWindowsClient::Exec
//
UBOOL UWindowsClient::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(RenderDevice && RenderDevice->Exec(Cmd,Ar))
		return 1;
	else
		return 0;
}

//
//	UWindowsClient::CreateViewport
//
FViewport* UWindowsClient::CreateViewport(FViewportClient* ViewportClient,const TCHAR* Name,UINT SizeX,UINT SizeY,UBOOL Fullscreen)
{
	return new FWindowsViewport(this,ViewportClient,Name,SizeX,SizeY,Fullscreen,NULL);
}

//
//	UWindowsClient::CreateWindowChildViewport
//
FChildViewport* UWindowsClient::CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX,UINT SizeY)
{
	return new FWindowsViewport(this,ViewportClient,TEXT(""),SizeX,SizeY,0,(HWND)ParentWindow);
}

//
//	UWindowsClient::CloseViewport
//
void UWindowsClient::CloseViewport(FChildViewport* Viewport)
{
	FWindowsViewport* WindowsViewport = (FWindowsViewport*)Viewport;
	WindowsViewport->Destroy();
}

//
//	UWindowsClient::StaticWndProc
//
LONG APIENTRY UWindowsClient::StaticWndProc( HWND hWnd, UINT Message, UINT wParam, LONG lParam )
{
	INT i;
	for( i=0; i<Viewports.Num(); i++ )
		if( Viewports(i)->GetWindow()==hWnd )
			break;

	if( i==Viewports.Num() || GIsCriticalError )
		return DefWindowProcX( hWnd, Message, wParam, lParam );
	else
		return Viewports(i)->ViewportWndProc( Message, wParam, lParam );			
}

#ifdef EMULATE_CONSOLE_CONTROLLER
//
//	Joystick callbacks.
//

BOOL CALLBACK EnumJoysticksCallback( LPCDIDEVICEINSTANCE Instance, LPVOID Context )
{
	if( FAILED( UWindowsClient::DirectInput8->CreateDevice( Instance->guidInstance, &UWindowsClient::DirectInput8Joystick, NULL ) ) ) 
	{
		UWindowsClient::DirectInput8Joystick = NULL;
		return DIENUM_CONTINUE;
	}
	return DIENUM_STOP;
}

BOOL CALLBACK EnumAxesCallback( LPCDIDEVICEOBJECTINSTANCE Instance, LPVOID Context )
{
	if( UWindowsClient::DirectInput8Joystick )
	{
		DIPROPRANGE Range; 
		Range.diph.dwSize       = sizeof(DIPROPRANGE); 
		Range.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
		Range.diph.dwHow        = DIPH_BYOFFSET; 
		Range.diph.dwObj        = Instance->dwOfs; // Specify the enumerated axis

		// Ideally we'd like a range of +/- 1 though sliders can't go negative so we'll map 0..65535 to +/- 1 when polling.
		Range.lMin              = 0;
		Range.lMax              = 65535;

		// Set the range for the axis
		UWindowsClient::DirectInput8Joystick->SetProperty( DIPROP_RANGE, &Range.diph );

		return DIENUM_CONTINUE;
	}
	else
		return DIENUM_STOP;
}
#endif

//
//	Static variables.
//
TArray<FWindowsViewport*>	UWindowsClient::Viewports;
LPDIRECTINPUT8				UWindowsClient::DirectInput8;
LPDIRECTINPUTDEVICE8		UWindowsClient::DirectInput8Mouse;
#ifdef EMULATE_CONSOLE_CONTROLLER
LPDIRECTINPUTDEVICE8		UWindowsClient::DirectInput8Joystick;
EJoystickType				UWindowsClient::JoystickType;
#endif
