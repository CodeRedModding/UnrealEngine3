/*=============================================================================
	WinClient.cpp: UWindowsClient code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "WinDrvPrivate.h"
#include "EngineAudioDeviceClasses.h"
#include "..\..\Launch\Resources\resource.h"
#pragma pack(push,8)
#include <OleAuto.h>
#include <WbemIdl.h>
#pragma pack(pop)

#include "WintabSupport.h"

#if WITH_WINTAB
FWinTab* GWinTab = NULL;
#endif


/**
 * Enable the new DirectInput mouse method by default. Toggle by exec "UseNewMouseInput" at run-time.
 * It merges all mouse events for a whole frame into one single event and uses the frame delta.
 * The old method was using the delta time since the mouse was last moved, which could be many many seconds.
 */
UBOOL GUseNewMouseInput = TRUE;


/**
 * Exported function used inside wxWindows to see whether the window handle is used by Unreal
 * or not.
 *
 * @param	hWnd	Window handle to check whether it is a handle used for rendering
 * @return	TRUE if we created the window, FALSE otherwise
 */
bool IsUnrealWindowHandle( HWND hWnd )
{
	bool KnownWindowHandle = false;
	if( GEngine && GEngine->Client )
	{
		UWindowsClient* Client = CastChecked<UWindowsClient>(GEngine->Client);
		for( INT ViewportIndex=0; ViewportIndex<Client->Viewports.Num(); ViewportIndex++ )
		{
			FWindowsViewport* Viewport = Client->Viewports(ViewportIndex);
			if( Viewport && Viewport->GetWindow() == hWnd )
			{
				KnownWindowHandle = true;
				break;
			}
		}
	}
	return KnownWindowHandle;
}

/** Thread procedure for the low-level keyboard hook function */
DWORD WINAPI LowLevelKeyboardThreadFunc( LPVOID unused );

/** Low-level keyboard hook function */
LRESULT CALLBACK LowLevelKeyboardProc( INT Code, WPARAM wParam, LPARAM lParam );


#if USE_NEW_MOUSECAPTURE
/**
 * Helper structure for keeping track of buffered mouse events.
 */
struct FMouseEvent
{
	/**
	 * Constructor, initializing all member variables to passed in ones.
	 *
	 * @param InTimeStamp	Time stamp of event
	 * @param InDelta		Movement delta of event
	 */
	FMouseEvent( DWORD InTimeStamp, INT InDelta )
	:	TimeStamp( InTimeStamp )
	,	Delta( InDelta )
	{}
	/** Time stamp of event in ms (arbitrarily based) */
	DWORD	TimeStamp;
	/** Movement delta of event */
	INT		Delta;	
};
#endif


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
	
	KeyMapVirtualToName.Set(VK_XBUTTON1,KEY_ThumbMouseButton);
	KeyMapVirtualToName.Set(VK_XBUTTON2,KEY_ThumbMouseButton2);

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

	// set up the Windows joystick mappings
	PS2ToXboxControllerMapping[0] = 3;		// triangle
	PS2ToXboxControllerMapping[1] = 1;		// circle
	PS2ToXboxControllerMapping[2] = 0;		// cross
	PS2ToXboxControllerMapping[3] = 2;		// square
	PS2ToXboxControllerMapping[4] = 10;		// L2
	PS2ToXboxControllerMapping[5] = 11;		// R2
	PS2ToXboxControllerMapping[6] = 4;		// L1 
	PS2ToXboxControllerMapping[7] = 5;		// R1
	PS2ToXboxControllerMapping[8] = 7;		// start
	PS2ToXboxControllerMapping[9] = 6;		// select
	PS2ToXboxControllerMapping[10] = 8;		// Left stick push
	PS2ToXboxControllerMapping[11] = 9;		// Righ stick down
	PS2ToXboxControllerMapping[12] = 12;
	PS2ToXboxControllerMapping[13] = 13;
	PS2ToXboxControllerMapping[14] = 14;
	PS2ToXboxControllerMapping[15] = 15;

	// Support X360 Controller on PC
	X360ToXboxControllerMapping[0] = 0;		// A
	X360ToXboxControllerMapping[1] = 1;		// B
	X360ToXboxControllerMapping[2] = 2;		// X
	X360ToXboxControllerMapping[3] = 3;		// Y
	X360ToXboxControllerMapping[4] = 4;		// L1
	X360ToXboxControllerMapping[5] = 5;		// R1
	X360ToXboxControllerMapping[6] = 7;		// Back 
	X360ToXboxControllerMapping[7] = 6;		// Start
	X360ToXboxControllerMapping[8] = 8;		// Left thumbstick
	X360ToXboxControllerMapping[9] = 9;		// Right thumbstick
	X360ToXboxControllerMapping[10] = 10;	// L2
	X360ToXboxControllerMapping[11] = 11;	// R2
	X360ToXboxControllerMapping[12] = 12;	// Dpad up
	X360ToXboxControllerMapping[13] = 13;	// Dpad down
	X360ToXboxControllerMapping[14] = 14;	// Dpad left
	X360ToXboxControllerMapping[15] = 15;	// Dpad right

	// Support X360 Controller on PC
	GameCasterToXboxControllerMapping[0] = 0;		// A
	GameCasterToXboxControllerMapping[1] = 1;		// B
	GameCasterToXboxControllerMapping[2] = 2;		// X
	GameCasterToXboxControllerMapping[3] = 3;		// Y
	GameCasterToXboxControllerMapping[4] = 4;		// L1
	GameCasterToXboxControllerMapping[5] = 5;		// R1
	GameCasterToXboxControllerMapping[6] = 7;		// Back 
	GameCasterToXboxControllerMapping[7] = 6;		// Start
	GameCasterToXboxControllerMapping[8] = 8;		// Left thumbstick
	GameCasterToXboxControllerMapping[9] = 9;		// Right thumbstick
	GameCasterToXboxControllerMapping[10] = 10;		// L2
	GameCasterToXboxControllerMapping[11] = 11;		// R2
	GameCasterToXboxControllerMapping[12] = 12;		// Dpad up
	GameCasterToXboxControllerMapping[13] = 13;		// Dpad down
	GameCasterToXboxControllerMapping[14] = 14;		// Dpad left
	GameCasterToXboxControllerMapping[15] = 15;		// Dpad right

	JoyNames[0] = KEY_XboxTypeS_A;
	JoyNames[1] = KEY_XboxTypeS_B;
	JoyNames[2] = KEY_XboxTypeS_X;
	JoyNames[3] = KEY_XboxTypeS_Y;
	JoyNames[4] = KEY_XboxTypeS_LeftShoulder;
	JoyNames[5] = KEY_XboxTypeS_RightShoulder;
	JoyNames[6] = KEY_XboxTypeS_Start;
	JoyNames[7] = KEY_XboxTypeS_Back;
	JoyNames[8] = KEY_XboxTypeS_LeftThumbstick;
	JoyNames[9] = KEY_XboxTypeS_RightThumbstick;
	JoyNames[10] = KEY_XboxTypeS_LeftTrigger;
	JoyNames[11] = KEY_XboxTypeS_RightTrigger;
	JoyNames[12] = KEY_XboxTypeS_DPad_Up;
	JoyNames[13] = KEY_XboxTypeS_DPad_Down;
	JoyNames[14] = KEY_XboxTypeS_DPad_Left;
	JoyNames[15] = KEY_XboxTypeS_DPad_Right;

	AudioDevice				= NULL;
	ProcessWindowsMessages	= 1;
	LastMouseXEventTime		= 0;
	LastMouseYEventTime		= 0;
	ControllerScanTime		= 0.5f;
	NextControllerToCheck	= 0;
}

//
// Static init.
//
void UWindowsClient::StaticConstructor()
{
	new(GetClass(),TEXT("AudioDeviceClass")	,RF_Public)UClassProperty(CPP_PROPERTY(AudioDeviceClass)	,TEXT("Audio")	,CPF_Config,UAudioDevice::StaticClass());
	new(GetClass(),TEXT("AllowJoystickInput"), RF_Public) UIntProperty(CPP_PROPERTY(bAllowJoystickInput), TEXT("Input"), CPF_Config);

	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UWindowsClient, Engine ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UWindowsClient, AudioDeviceClass ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UWindowsClient, AudioDevice ) );
}

//
// Initialize the platform-specific viewport manager subsystem.
// Must be called after the Unreal object manager has been initialized.
// Must be called before any viewports are created.
//
BOOL CALLBACK EnumJoystickChangesCallback( LPCDIDEVICEINSTANCE Instance, LPVOID Context );
BOOL CALLBACK EnumAxesCallback( LPCDIDEVICEOBJECTINSTANCE Instance, LPVOID Context );

/** Resource ID of icon to use for Window */
extern INT GGameIcon;

/** True if a remote desktop sesssion is currently active.  Dynamically updated. */
UBOOL UWindowsClient::bRemoteDesktopSessionActive = FALSE;

void UWindowsClient::Init( UEngine* InEngine )
{
	Engine = InEngine;

	// Register windows class.
	WindowClassName = FString(GPackage) + TEXT("Unreal") + TEXT("UWindowsClient");
	if ( !GIsGame )
	{
		WNDCLASSEXW Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize			= sizeof(Cls);
		// disable dbl-click messages in the game as the dbl-click event is sent instead of the key pressed event, which causes issues with e.g. rapid firing
		Cls.style			= GIsGame ? (CS_OWNDC) : (CS_DBLCLKS|CS_OWNDC);
		Cls.lpfnWndProc		= StaticWndProc;
		Cls.hInstance		= hInstance;
		Cls.hIcon			= LoadIcon(hInstance,MAKEINTRESOURCEW(GGameIcon));
		Cls.lpszClassName	= *WindowClassName;
		Cls.hIconSm			= LoadIcon(hInstance,MAKEINTRESOURCEW(GGameIcon));
		verify(RegisterClassExW( &Cls ));
	}

	// Initialize the audio device.
	if( GEngine->bUseSound && AudioDeviceClass )
	{
		AudioDevice = ConstructObject<UAudioDevice>( AudioDeviceClass );
		if( !AudioDevice->Init() )
		{
			AudioDevice = NULL;
		}
	}

	// remove bulk data if no sounds were initialized
	if( !GIsEditor && GIsGame && AudioDevice == NULL )
	{
		appSoundNodeRemoveBulkData();
	}

	// Save the current sticky/toggle/filter key settings so they can be restored them later
	// If there are .ini settings, use them instead of the current system settings.
	// NOTE: Whenever we exit and restore these settings gracefully, the .ini settings are removed.
	appMemzero( &StartupStickyKeys, sizeof(StartupStickyKeys) );
	StartupStickyKeys.cbSize = sizeof(StartupStickyKeys);
	appMemzero( &StartupToggleKeys, sizeof(StartupToggleKeys) );
	StartupToggleKeys.cbSize = sizeof(StartupToggleKeys);
	appMemzero( &StartupFilterKeys, sizeof(StartupFilterKeys) );
	StartupFilterKeys.cbSize = sizeof(StartupFilterKeys);
	SystemParametersInfo( SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &StartupStickyKeys, 0 );
	SystemParametersInfo( SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &StartupToggleKeys, 0 );
	SystemParametersInfo( SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &StartupFilterKeys, 0 );
	UBOOL SKHotkey		 = (StartupStickyKeys.dwFlags & SKF_HOTKEYACTIVE)  ? TRUE : FALSE;
	UBOOL TKHotkey		 = (StartupToggleKeys.dwFlags & TKF_HOTKEYACTIVE)  ? TRUE : FALSE;
	UBOOL FKHotkey		 = (StartupFilterKeys.dwFlags & FKF_HOTKEYACTIVE)  ? TRUE : FALSE;
	UBOOL SKConfirmation = (StartupStickyKeys.dwFlags & SKF_CONFIRMHOTKEY) ? TRUE : FALSE;
	UBOOL TKConfirmation = (StartupToggleKeys.dwFlags & TKF_CONFIRMHOTKEY) ? TRUE : FALSE;
	UBOOL FKConfirmation = (StartupFilterKeys.dwFlags & FKF_CONFIRMHOTKEY) ? TRUE : FALSE;
	GConfig->GetBool( TEXT("WinDrv.Accessibility"), TEXT("StickyKeysHotkey"), SKHotkey, GEngineIni );
	GConfig->GetBool( TEXT("WinDrv.Accessibility"), TEXT("ToggleKeysHotkey"), TKHotkey, GEngineIni );
	GConfig->GetBool( TEXT("WinDrv.Accessibility"), TEXT("FilterKeysHotkey"), FKHotkey, GEngineIni );
	GConfig->GetBool( TEXT("WinDrv.Accessibility"), TEXT("StickyKeysConfirmation"), SKConfirmation, GEngineIni );
	GConfig->GetBool( TEXT("WinDrv.Accessibility"), TEXT("ToggleKeysConfirmation"), TKConfirmation, GEngineIni );
	GConfig->GetBool( TEXT("WinDrv.Accessibility"), TEXT("FilterKeysConfirmation"), FKConfirmation, GEngineIni );
	StartupStickyKeys.dwFlags = SKHotkey ? (StartupStickyKeys.dwFlags | SKF_HOTKEYACTIVE) : (StartupStickyKeys.dwFlags & ~SKF_HOTKEYACTIVE);
	StartupToggleKeys.dwFlags = TKHotkey ? (StartupToggleKeys.dwFlags | TKF_HOTKEYACTIVE) : (StartupToggleKeys.dwFlags & ~TKF_HOTKEYACTIVE);
	StartupFilterKeys.dwFlags = FKHotkey ? (StartupFilterKeys.dwFlags | FKF_HOTKEYACTIVE) : (StartupFilterKeys.dwFlags & ~FKF_HOTKEYACTIVE);
	StartupStickyKeys.dwFlags = SKConfirmation ? (StartupStickyKeys.dwFlags | SKF_CONFIRMHOTKEY) : (StartupStickyKeys.dwFlags & ~SKF_CONFIRMHOTKEY);
	StartupToggleKeys.dwFlags = TKConfirmation ? (StartupToggleKeys.dwFlags | TKF_CONFIRMHOTKEY) : (StartupToggleKeys.dwFlags & ~TKF_CONFIRMHOTKEY);
	StartupFilterKeys.dwFlags = FKConfirmation ? (StartupFilterKeys.dwFlags | FKF_CONFIRMHOTKEY) : (StartupFilterKeys.dwFlags & ~FKF_CONFIRMHOTKEY);
	GConfig->SetBool( TEXT("WinDrv.Accessibility"), TEXT("StickyKeysHotkey"), SKHotkey, GEngineIni );
	GConfig->SetBool( TEXT("WinDrv.Accessibility"), TEXT("ToggleKeysHotkey"), TKHotkey, GEngineIni );
	GConfig->SetBool( TEXT("WinDrv.Accessibility"), TEXT("FilterKeysHotkey"), FKHotkey, GEngineIni );
	GConfig->SetBool( TEXT("WinDrv.Accessibility"), TEXT("StickyKeysConfirmation"), SKConfirmation, GEngineIni );
	GConfig->SetBool( TEXT("WinDrv.Accessibility"), TEXT("ToggleKeysConfirmation"), TKConfirmation, GEngineIni );
	GConfig->SetBool( TEXT("WinDrv.Accessibility"), TEXT("FilterKeysConfirmation"), FKConfirmation, GEngineIni );
	GConfig->Flush( FALSE, GEngineIni );

	// Disable accessibility shortcuts
	AllowAccessibilityShortcutKeys( FALSE );

	// Start the thread that manages the low-level keyboard hook to catch the Windows key.
	KeyboardHookThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LowLevelKeyboardThreadFunc, (LPVOID)NULL, 0, &KeyboardHookThreadId);

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

	// Pre-create 4 joysticks for XInput controllers.
	for ( INT JoystickIndex=0; JoystickIndex < 4; ++JoystickIndex )
	{
		FJoystickInfo *JoystickInfo = new(Joysticks) FJoystickInfo;
		appMemzero( JoystickInfo, sizeof(*JoystickInfo) );
		JoystickInfo->ControllerId = JoystickIndex;
		JoystickInfo->JoystickType = JOYSTICK_X360;
		JoystickInfo->bIsConnected = TRUE;
	}

	DirectInput8->EnumDevices( DI8DEVCLASS_GAMECTRL, EnumJoystickChangesCallback, NULL, DIEDFL_ATTACHEDONLY );

	// Check if we launched from inside a Remote Desktop session.
	bRemoteDesktopSessionActive = GetSystemMetrics(SM_REMOTESESSION) != 0;

#if WITH_WINTAB
	// Initialize WinTab
	UBOOL bEnableStylusPressure = FALSE;
	GConfig->GetBool( TEXT("UnrealEd.GeneralSettings"), TEXT("EnableStylusPressure"), bEnableStylusPressure, GEditorIni );
	if( GIsEditor && bEnableStylusPressure )
	{
		GWinTab = FWinTab::Init();

		if( GWinTab )
		{
			if( GWinTab->WTInfoA(0, 0, NULL) )
			{
				debugf(TEXT("WinTab initalized successfully."));
			}
		}
	}
#endif

	// Success.
	debugf( NAME_Init, TEXT("Client initialized") );
}

//
//	UWindowsClient::Serialize - Make sure objects the client reference aren't garbage collected.
//
void UWindowsClient::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Engine << AudioDeviceClass << AudioDevice;
}

//
//	UWindowsClient::Destroy - Shut down the platform-specific viewport manager subsystem.
//
void UWindowsClient::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if( GFullScreenMovie )
		{
			// force movie playback to stop
			GFullScreenMovie->GameThreadStopMovie(0,FALSE,TRUE);
		}

		// Close all viewports.
		for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
		{
			Viewports(ViewportIndex)->Destroy();
			delete Viewports(ViewportIndex);
		}
		Viewports.Empty();

		// Stop capture.
		SetCapture( NULL );
		ClipCursor( NULL );

		for(INT JoystickIndex = 0;JoystickIndex < Joysticks.Num();JoystickIndex++)
		{
			SAFE_RELEASE( Joysticks(JoystickIndex).DirectInput8Joystick );
		}
		Joysticks.Empty();

		SAFE_RELEASE( DirectInput8Mouse );
		SAFE_RELEASE( DirectInput8 );

		// Restore accessibility shortcuts and remove the saved state from the .ini
		AllowAccessibilityShortcutKeys( TRUE );
		GConfig->EmptySection( TEXT("WinDrv.Accessibility"), GEngineIni );

		// Stop the low-level keyboard hook thread.
		PostThreadMessage( KeyboardHookThreadId, WM_USER, 0, 0 );

		// Wait for thread to finish
		WaitForSingleObject( KeyboardHookThread, INFINITE );

		debugf( NAME_Exit, TEXT("Windows client shut down") );

#if WITH_WINTAB
		if( GWinTab )
		{
			delete GWinTab;
		}
#endif
	}
	Super::FinishDestroy();
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
	{
		Viewports(ViewportIndex)->ShutdownAfterError();
	}

	// Restore accessibility shortcuts
	AllowAccessibilityShortcutKeys( TRUE );
	GConfig->EmptySection( TEXT("WinDrv.Accessibility"), GEngineIni );

	Super::ShutdownAfterError();
}

//
//	UWindowsClient::Tick - Perform timer-tick processing on all visible viewports.
//
void UWindowsClient::Tick( FLOAT DeltaTime )
{
	// Process messages that have been deferred until now.
	ProcessDeferredMessages();

	// Update the viewports.
	for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
	{
		if( !Viewports(ViewportIndex)->IsPlayInEditorViewport() )
		{
			// Only tick non play in editor viewports as they will be ticked later
			Viewports(ViewportIndex)->Tick(DeltaTime);
		}
	}

	// Process input
	ProcessInput( DeltaTime );

	// Update the viewports.
	for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
	{
		// Cleanup viewports that have been destroyed.
		if(!Viewports(ViewportIndex)->GetWindow())
		{
			delete Viewports(ViewportIndex);
			Viewports.Remove(ViewportIndex--);
		}
	}
}

/**
 *	Reads input from shared resources, such as buffered D3D mouse input.
 */
void UWindowsClient::ProcessInput( FLOAT DeltaTime )
{
	SCOPE_CYCLE_COUNTER(STAT_InputTime);

#if USE_NEW_MOUSECAPTURE
	// Process mouse input.

	// Find the viewport who should receive mouse input
	FWindowsViewport* FocusViewport = NULL;
	FViewportClient* FocusViewportClient = NULL;
	for(UINT ViewportIndex = 0;ViewportIndex < (UINT)Viewports.Num();ViewportIndex++)
	{
		FWindowsViewport* Viewport = Viewports(ViewportIndex);
		FViewportClient* ViewportClient = Viewport->GetClient();
		if ( Viewport->HasFocus() )
		{
			FocusViewport = Viewport;
			FocusViewportClient = ViewportClient;
		}
		if ( Viewport->HasMouseCapture() || (ViewportClient && ViewportClient->RequiresUncapturedAxisInput()) )
		{
			FocusViewport = Viewport;
			FocusViewportClient = ViewportClient;
			break;
		}
	}

	// route all mouse messages with the proper GWorld
	GCallbackEvent->Send( CALLBACK_PreWindowsMessage, FocusViewport, 0 );
	if ( PollMouseInput() )
	{

		GInputLatencyTimer.GameThreadTick();

		if ( GUseNewMouseInput )
		{
			if ( FocusViewportClient && FocusViewportClient->WantsPollingMouseMovement() &&
				 ( FocusViewport == NULL || !FocusViewport->IsFakeMobileTouchesEnabled() ) )
			{
				INT DeltaX = 0;
				INT DeltaY = 0;
				DIDEVICEOBJECTDATA	Event;
				DWORD				Elements = 1;
				while( SUCCEEDED( DirectInput8Mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),&Event,&Elements,0) ) && (Elements > 0) )
				{
					switch( Event.dwOfs ) 
					{
						case DIMOFS_X:
							{
								DeltaX += INT(Event.dwData);
								break;
							}
						case DIMOFS_Y: 
							{
								DeltaY -= INT(Event.dwData);
								break; 
							}
						case DIMOFS_Z:
							{
								if( ((INT)Event.dwData) < 0 )
								{
									FocusViewportClient->InputKey(FocusViewport,0,KEY_MouseScrollDown,IE_Pressed);
									FocusViewportClient->InputKey(FocusViewport,0,KEY_MouseScrollDown,IE_Released);
								}
								else if( ((INT)Event.dwData) > 0)
								{
									FocusViewportClient->InputKey(FocusViewport,0,KEY_MouseScrollUp,IE_Pressed);
									FocusViewportClient->InputKey(FocusViewport,0,KEY_MouseScrollUp,IE_Released);
								}
							}
							break;
					}
					DirectInput8Mouse->Poll();
				}

				if ( DeltaX )
				{
					// When running under a Terminal Services session, the DirectInput emulation
					// returns very large numbers for the mouse delta making the editor unusable.
					// This hack scales them down to improve things somewhat.
					if( bRemoteDesktopSessionActive )
					{
						DeltaX /= 50;
					}
					FocusViewportClient->InputAxis( FocusViewport, 0, KEY_MouseX, DeltaX, DeltaTime );
				}
				if ( DeltaY )
				{
					if( bRemoteDesktopSessionActive )
					{
						DeltaY /= 50;
					}
					FocusViewportClient->InputAxis( FocusViewport, 0, KEY_MouseY, DeltaY, DeltaTime );
				}
			}
			else
			{
				FlushMouseInput();
			}
		}
		else
		{
			TArray<FMouseEvent>	MouseXEvents;
			TArray<FMouseEvent>	MouseYEvents;
			DIDEVICEOBJECTDATA	Event;
			DWORD				Elements = 1;
			while( SUCCEEDED( DirectInput8Mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),&Event,&Elements,0) ) && (Elements > 0) )
			{
				if ( FocusViewportClient )
				{
					switch( Event.dwOfs ) 
					{
						case DIMOFS_X:
							{
								MouseXEvents.AddItem( FMouseEvent( Event.dwTimeStamp, (INT)Event.dwData ) );
								break;
							}
						case DIMOFS_Y: 
							{
								MouseYEvents.AddItem( FMouseEvent( Event.dwTimeStamp, -(INT)Event.dwData ) );
								break; 
							}
						case DIMOFS_Z:
							{
								GCallbackEvent->Send( CALLBACK_PreWindowsMessage, FocusViewport, 0 );
								if( ((INT)Event.dwData) < 0 )
								{
									FocusViewportClient->InputKey(FocusViewport,0,KEY_MouseScrollDown,IE_Pressed);
									FocusViewportClient->InputKey(FocusViewport,0,KEY_MouseScrollDown,IE_Released);
								}
								else if( ((INT)Event.dwData) > 0)
								{
									FocusViewportClient->InputKey(FocusViewport,0,KEY_MouseScrollUp,IE_Pressed);
									FocusViewportClient->InputKey(FocusViewport,0,KEY_MouseScrollUp,IE_Released);
								}
								GCallbackEvent->Send( CALLBACK_PostWindowsMessage, FocusViewport, 0 );
							}
							break;
						default:
							break;
					}
				}
			}

#if MERGE_SAME_TIMESTAMPS
			// Merge duplicate entries.
			for( INT EventIndex=1; EventIndex<MouseXEvents.Num(); EventIndex++ )
			{
				FMouseEvent& PreviousMouseEvent = MouseXEvents( EventIndex - 1 );
				FMouseEvent& CurrentMouseEvent	= MouseXEvents( EventIndex );
				// Merge entries with duplicate timestamps to allow proper delta time calculation.
				if( PreviousMouseEvent.TimeStamp == CurrentMouseEvent.TimeStamp )
				{			
					PreviousMouseEvent.Delta += CurrentMouseEvent.Delta;
					MouseXEvents.Remove( EventIndex-- );
				}
			}
			for( INT EventIndex=1; EventIndex<MouseYEvents.Num(); EventIndex++ )
			{
				FMouseEvent& PreviousMouseEvent = MouseYEvents( EventIndex - 1 );
				FMouseEvent& CurrentMouseEvent	= MouseYEvents( EventIndex );
				// Merge entries with duplicate timestamps to allow proper delta time calculation.
				if( PreviousMouseEvent.TimeStamp == CurrentMouseEvent.TimeStamp )
				{			
					PreviousMouseEvent.Delta += CurrentMouseEvent.Delta;
					MouseYEvents.Remove( EventIndex-- );
				}
			}
#endif

			// Route mouse events to the correct viewport client.
			for( INT EventIndex=0; EventIndex<MouseXEvents.Num(); EventIndex++ )
			{
				FMouseEvent Event		= MouseXEvents( EventIndex );
				DWORD		DeltaTicks	= Event.TimeStamp - LastMouseXEventTime;
				FocusViewportClient->InputAxis( FocusViewport, 0, KEY_MouseX, Event.Delta, DeltaTicks / 1000.f );
				LastMouseXEventTime = Event.TimeStamp;
			}
			for( INT EventIndex=0; EventIndex<MouseYEvents.Num(); EventIndex++ )
			{
				FMouseEvent Event		= MouseYEvents( EventIndex );
				DWORD		DeltaTicks	= Event.TimeStamp - LastMouseYEventTime;
				FocusViewportClient->InputAxis( FocusViewport, 0, KEY_MouseY, Event.Delta, DeltaTicks / 1000.f );
				LastMouseYEventTime = Event.TimeStamp;
			}
		}
	}

	GCallbackEvent->Send( CALLBACK_PostWindowsMessage, FocusViewport, 0 );
#endif

	// Process viewport input.
	for(UINT ViewportIndex = 0;ViewportIndex < (UINT)Viewports.Num();ViewportIndex++)
	{
		GCallbackEvent->Send( CALLBACK_PreWindowsMessage, Viewports(ViewportIndex), 0 );
		Viewports(ViewportIndex)->ProcessInput( DeltaTime );
		GCallbackEvent->Send( CALLBACK_PostWindowsMessage, Viewports(ViewportIndex), 0 );
	}

	// Occasionally check for new XInput controllers
	ControllerScanTime -= DeltaTime;
	if ( ControllerScanTime < 0.0f )
	{
		ControllerScanTime = 0.5f;
		for ( INT ControllerIndex=0; ControllerIndex < JOYSTICK_NUM_XINPUT_CONTROLLERS; ++ControllerIndex )
		{
			FJoystickInfo &JoystickInfo = Joysticks(NextControllerToCheck);
			NextControllerToCheck = (NextControllerToCheck + 1) % JOYSTICK_NUM_XINPUT_CONTROLLERS;
			if ( JoystickInfo.bIsConnected == FALSE )
			{
				JoystickInfo.bIsConnected = TRUE;
				break;
			}
		}
	}

	// Scan for gamepad removal/insertion
	// NOTE: This is commented out because EnumDevices leaks handles like crazy. :(
	{
		// Detect unplugged gamepads.
		//INT NumJoysticks = Joysticks.Num();
		//for ( INT JoystickIndex=JOYSTICK_NUM_XINPUT_CONTROLLERS; JoystickIndex < NumJoysticks; ++JoystickIndex )
		//{
		//	FJoystickInfo &JoystickInfo = Joysticks(JoystickIndex);
		//	HRESULT DeviceStatus = DirectInput8->GetDeviceStatus( JoystickInfo.DeviceGUID );
		//	if ( JoystickInfo.DirectInput8Joystick && DeviceStatus != DI_OK )	// DI_NOTATTACHED
		//	{
		//		SAFE_RELEASE( JoystickInfo.DirectInput8Joystick );
		//		appMemzero( &JoystickInfo, sizeof(JoystickInfo) );
		//	}
		//}

		//// Check for new gamepads.
		//DirectInput8->EnumDevices( DI8DEVCLASS_GAMECTRL, EnumJoystickChangesCallback, NULL, DIEDFL_ATTACHEDONLY );
	}
}

/**
 * Poll mouse input
 */
UBOOL UWindowsClient::PollMouseInput( )
{
	
	HRESULT PollResult = DirectInput8Mouse->Poll();
	if ( FAILED(PollResult) )
	{
		// Focus issues with Viewports: Force mouse input.
		DirectInput8Mouse->Unacquire();
		DirectInput8Mouse->SetCooperativeLevel(GetForegroundWindow(),DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		DirectInput8Mouse->Acquire();
		PollResult = DirectInput8Mouse->Poll();
	}
	return SUCCEEDED( PollResult );
}

/**
 * Flush all mouse input
 */
void UWindowsClient::FlushMouseInput( )
{
	if ( PollMouseInput() )
	{
		DWORD Elements = INFINITE;
		DirectInput8Mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), NULL, &Elements, 0);
	}
}

/**	Windows-specific Exec handling */
UBOOL UWindowsClient::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if( Super::Exec(Cmd,Ar) )
	{
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("MAKESECONDSCREEN")))
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			INT SizeX = appAtoi(Cmd);
			Cmd = appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT("");
			INT SizeY = appAtoi(Cmd);

			if( SizeX && SizeY )
			{
				// Create the viewport
				GameEngine->CreateSecondaryViewport(SizeX, SizeY);
			}
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("CLOSESECONDSCREEN")))
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			GameEngine->CloseSecondaryViewports();
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PARANOIDDEVICELOSTCHECKING")))
	{
		// Whether we should be calling TestCooperativeLevel every drawcall
		extern UBOOL GParanoidDeviceLostChecking;
		GParanoidDeviceLostChecking = !GParanoidDeviceLostChecking;
		Ar.Logf( TEXT("GParanoidDeviceLostChecking=%d"), GParanoidDeviceLostChecking );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("USENEWMOUSEINPUT")))
	{
		GUseNewMouseInput = !GUseNewMouseInput;
		Ar.Logf( TEXT("GUseNewMouseInput=%d"), GUseNewMouseInput );
		return TRUE;
	}
#if !SHIPPING_PC_GAME
	// mobile emulation support
	else if (ParseCommand(&Cmd, TEXT("MOBILE")))
	{
		if (ParseCommand(&Cmd, TEXT("SaveSetting")))
		{
			FString SettingName = ParseToken(Cmd, TRUE);
			FString Value = FString(Cmd).Trim();

			// save the setting if we have proper input
			if (SettingName != TEXT("") && Value != TEXT(""))
			{
				GConfig->SetString(TEXT("MobileEmulation"), *(FString(TEXT("Setting_")) + SettingName), *Value, GGameIni);
			}
			else
			{
				debugf(TEXT("Mobile SaveSetting did not receive a setting name and value"));
			}
		}
		else if (ParseCommand(&Cmd, TEXT("LoadSetting")))
		{
			FString SettingName = ParseToken(Cmd, TRUE);
			FString Default = FString(Cmd).Trim();

			// load the setting if we got the a setting name
			if (SettingName != TEXT(""))
			{
				FString ValueString;
				// if the value was there, use it
				if (GConfig->GetString(TEXT("MobileEmulation"), *(FString(TEXT("Setting_")) + SettingName), ValueString, GGameIni))
				{
					// return the value via the Ar
					Ar.Log(*ValueString);
				}
				// otherwise return the default
				else
				{
					Ar.Log(*Default);
				}
			}
			else
			{
				debugf(TEXT("Mobile LoadSetting did not receive a setting name"));
			}
		}
		else if (GUsingES2RHI)
		{
			//debugging stat tracking.  Not thread-complient, but shouldn't harm anything
			if (ParseCommand(&Cmd, TEXT("_ResetPrimitiveTracking")))
			{
				RHIResetTrackedPrimitive();
			}
			else if (ParseCommand(&Cmd, TEXT("_IncrementTrackedPrimitive")))
			{
				INT Increment = appAtoi(Cmd);
				RHIIncrementTrackedPrimitive(Increment);
			}
		}
	}
#endif

	return FALSE;
}

/** Function to immediately stop any force feedback vibration that might be going on this frame. */
void UWindowsClient::ForceClearForceFeedback()
{
	for(UINT ViewportIndex = 0;ViewportIndex < (UINT)Viewports.Num();ViewportIndex++)
	{
		if(Viewports(ViewportIndex) && Viewports(ViewportIndex)->GetClient())
		{
			for(INT JoystickIndex = 0; JoystickIndex < UWindowsClient::Joysticks.Num(); JoystickIndex++)
			{
				UForceFeedbackManager* Manager = Viewports(ViewportIndex)->GetClient()->GetForceFeedbackManager(JoystickIndex);
				if(Manager)
				{
					Manager->ForceClearWaveformData(JoystickIndex);
				}
			}
		}
	}
}

//
//	UWindowsClient::CreateViewport
//
FViewportFrame* UWindowsClient::CreateViewportFrame(FViewportClient* ViewportClient,const TCHAR* InName,UINT SizeX,UINT SizeY,UBOOL Fullscreen)
{
	return new FWindowsViewport(this,ViewportClient,InName,SizeX,SizeY,Fullscreen,NULL);
}

//
//	UWindowsClient::CreateWindowChildViewport
//
FViewport* UWindowsClient::CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX,UINT SizeY,INT InPosX,INT InPosY)
{
	return new FWindowsViewport(this,ViewportClient,TEXT(""),SizeX,SizeY,0,(HWND)ParentWindow,InPosX,InPosY);
}

//
//	UWindowsClient::CloseViewport
//
void UWindowsClient::CloseViewport(FViewport* Viewport)
{
	// Shutdown any rumbling.
	ForceClearForceFeedback();

	FWindowsViewport* WindowsViewport = (FWindowsViewport*)Viewport;
	WindowsViewport->Destroy();
}

/**
 * Helper function to return the play world viewport (if one exists)
 * @return - NULL if there is no play world viewport, otherwise the propert viewport
 */
FViewport*  UWindowsClient::GetPlayWorldViewport (void)
{
#if _WIN64
	const FString PlatformBitsString( TEXT( "64" ) );
#else
	const FString PlatformBitsString( TEXT( "32" ) );
#endif

	const FString GameName = GConfig->GetStr(TEXT("URL"), TEXT("GameName"), GEngineIni);
	FString RHIName = ShaderPlatformToText( GRHIShaderPlatform, TRUE, TRUE );

	const FString PlayWorldViewportName = FString::Printf( LocalizeSecure( LocalizeUnrealEd( "PlayInEditor_RHI_F" ), *GameName, *PlatformBitsString, *RHIName ) );

	for (int i = 0; i < Viewports.Num(); ++i)
	{
		if (Viewports(i)->GetName() == PlayWorldViewportName)
		{
			return Viewports(i);
		}
	}
	return NULL;
}

#if WITH_EDITOR
/** Called when a play world is active to tick the play in editor viewport */
void UWindowsClient::TickPlayInEditorViewport(FLOAT DeltaSeconds)
{
	for (INT ViewportIndex = 0; ViewportIndex < Viewports.Num(); ++ViewportIndex)
	{
		if (Viewports(ViewportIndex)->IsPlayInEditorViewport() )
		{
			Viewports(ViewportIndex)->Tick(DeltaSeconds);
		}
	}
}
#endif
/**
 * Retrieves the name of the key mapped to the specified character code.
 *
 * @param	KeyCode	the key code to get the name for; should be the key's ANSI value
 */
FName UWindowsClient::GetVirtualKeyName( INT KeyCode ) const
{
	if ( KeyCode < 255 )
	{
		const FName* VirtualName = KeyMapVirtualToName.Find((BYTE)KeyCode);
		if ( VirtualName != NULL )
		{
			return *VirtualName;
		}
	}

	return NAME_None;
}

//
//	UWindowsClient::StaticWndProc
//
LRESULT APIENTRY UWindowsClient::StaticWndProc( HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam )
{
	// Prevent power management
	if (Message == WM_POWERBROADCAST)
	{
		switch( wParam )
		{
			case PBT_APMQUERYSUSPEND:
			case PBT_APMQUERYSTANDBY:
				return BROADCAST_QUERY_DENY;
		}
	}

	// Update flag on remote desktop connect / disconnect
	if( Message==WM_WTSSESSION_CHANGE )
	{
		bRemoteDesktopSessionActive = GetSystemMetrics(SM_REMOTESESSION) != 0;			
		debugf(TEXT("Remote Desktop: %d"), bRemoteDesktopSessionActive);
	}

	INT i;
	for( i=0; i<Viewports.Num(); i++ )
	{
		FWindowsViewport* Viewport = Viewports(i);
		if( Viewport && Viewport->GetWindow()==hWnd )
		{
			break;
		}
	}

	if( i==Viewports.Num() || GIsCriticalError )
	{
		return DefWindowProc( hWnd, Message, wParam, lParam );
	}
	else
	{
#if WITH_PANORAMA
		extern UBOOL appPanoramaInputHook(HWND hWnd,UINT Message,UINT wParam,LONG lParam,LONG& Return);
		LONG ReturnVal = 0;
		// G4WLive needs to intercept input when the Guide is showing. Only
		// process if Live didn't already handle it
		if (appPanoramaInputHook(hWnd,Message,wParam,lParam,ReturnVal) == FALSE)
		{
			return ReturnVal;
		}
#endif
		return Viewports(i)->ViewportWndProc( Message, wParam, lParam );
	}
}

void UWindowsClient::AllowAccessibilityShortcutKeys( UBOOL AllowKeys )
{
	if( AllowKeys )
	{
		// Restore StickyKeys/etc to original state and enable Windows key      
		SystemParametersInfo( SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &StartupStickyKeys, 0 );
		SystemParametersInfo( SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &StartupToggleKeys, 0 );
		SystemParametersInfo( SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &StartupFilterKeys, 0 );
	}
	else
	{
		// Disable StickyKeys/etc shortcuts but if the accessibility feature is on, 
		// then leave the settings alone as its probably being usefully used

		STICKYKEYS skOff = StartupStickyKeys;
		if( (skOff.dwFlags & SKF_STICKYKEYSON) == 0 )
		{
			// Disable the hotkey and the confirmation
			skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
			skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;

			SystemParametersInfo( SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0 );
		}

		TOGGLEKEYS tkOff = StartupToggleKeys;
		if( (tkOff.dwFlags & TKF_TOGGLEKEYSON) == 0 )
		{
			// Disable the hotkey and the confirmation
			tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
			tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;

			SystemParametersInfo( SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tkOff, 0 );
		}

		FILTERKEYS fkOff = StartupFilterKeys;
		if( (fkOff.dwFlags & FKF_FILTERKEYSON) == 0 )
		{
			// Disable the hotkey and the confirmation
			fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
			fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;

			SystemParametersInfo( SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fkOff, 0 );
		}
	}
}

//
//	Joystick callbacks.
//

/** This function is taken from the DirectX documentation. Complain to Microsoft... */
UBOOL IsXInputDevice( const GUID* DeviceGUID )
{
    IWbemLocator*           pIWbemLocator  = NULL;
    IEnumWbemClassObject*   pEnumDevices   = NULL;
    IWbemClassObject*       pDevices[20]   = {0};
    IWbemServices*          pIWbemServices = NULL;
    BSTR                    bstrNamespace  = NULL;
    BSTR                    bstrDeviceID   = NULL;
    BSTR                    bstrClassName  = NULL;
    DWORD                   uReturned      = 0;
    UBOOL                   bIsXinputDevice= FALSE;
    UINT                    iDevice        = 0;
    VARIANT                 var;
    HRESULT                 hr;

    // CoInit if needed
	bool bCleanupCOM = FALSE;
	if( !GIsCOMInitialized )
	{
		GIsCOMInitialized = TRUE;
		hr = CoInitialize(NULL);
		bCleanupCOM = SUCCEEDED(hr);
	}

    // Create WMI
    hr = CoCreateInstance( __uuidof(WbemLocator),
                           NULL,
                           CLSCTX_INPROC_SERVER,
                           __uuidof(IWbemLocator),
                           (LPVOID*) &pIWbemLocator);
    if( FAILED(hr) || pIWbemLocator == NULL )
        goto LCleanup;

    bstrNamespace = SysAllocString( TEXT("\\\\.\\root\\cimv2") );if( bstrNamespace == NULL ) goto LCleanup;        
    bstrClassName = SysAllocString( TEXT("Win32_PNPEntity") );   if( bstrClassName == NULL ) goto LCleanup;        
    bstrDeviceID  = SysAllocString( TEXT("DeviceID") );          if( bstrDeviceID == NULL )  goto LCleanup;        
    
    // Connect to WMI 
    hr = pIWbemLocator->ConnectServer( bstrNamespace, NULL, NULL, 0L, 
                                       0L, NULL, NULL, &pIWbemServices );
    if( FAILED(hr) || pIWbemServices == NULL )
        goto LCleanup;

    // Switch security level to IMPERSONATE. 
    CoSetProxyBlanket( pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, 
                       RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE );                    

    hr = pIWbemServices->CreateInstanceEnum( bstrClassName, 0, NULL, &pEnumDevices ); 
    if( FAILED(hr) || pEnumDevices == NULL )
        goto LCleanup;

    // Loop over all devices
    for( ;; )
    {
        // Get 20 at a time
        hr = pEnumDevices->Next( 10000, 20, pDevices, &uReturned );
        if( FAILED(hr) )
            goto LCleanup;
        if( uReturned == 0 )
            break;

        for( iDevice=0; iDevice<uReturned; iDevice++ )
        {
            // For each device, get its device ID
			check(pDevices[iDevice]);
            hr = pDevices[iDevice]->Get( bstrDeviceID, 0L, &var, NULL, NULL );
            if( SUCCEEDED( hr ) && var.vt == VT_BSTR && var.bstrVal != NULL )
            {
                // Check if the device ID contains "IG_".  If it does, then it's an XInput device
				    // This information can not be found from DirectInput 
                if( appStrstr( var.bstrVal, TEXT("IG_") ) )
                {
                    // If it does, then get the VID/PID from var.bstrVal
                    DWORD dwPid = 0, dwVid = 0;
					TCHAR* strVid = appStrstr( var.bstrVal, TEXT("VID_") );
                    if( strVid && appSSCANF( strVid, TEXT("VID_%4X"), &dwVid ) != 1 )
                        dwVid = 0;
					TCHAR* strPid = appStrstr( var.bstrVal, TEXT("PID_") );
                    if( strPid && appSSCANF( strPid, TEXT("PID_%4X"), &dwPid ) != 1 )
                        dwPid = 0;

                    // Compare the VID/PID to the DInput device
                    DWORD dwVidPid = MAKELONG( dwVid, dwPid );
                    if( dwVidPid == DeviceGUID->Data1 )
                    {
                        bIsXinputDevice = TRUE;
                        goto LCleanup;
                    }
                }
            }   
            SAFE_RELEASE( pDevices[iDevice] );
        }
    }

LCleanup:
    if(bstrNamespace)
	{
        SysFreeString(bstrNamespace);
	}
    if(bstrDeviceID)
	{
        SysFreeString(bstrDeviceID);
	}
    if(bstrClassName)
	{
        SysFreeString(bstrClassName);
	}
    for( iDevice=0; iDevice<20; iDevice++ )
	{
        SAFE_RELEASE( pDevices[iDevice] );
	}
    SAFE_RELEASE( pEnumDevices );
    SAFE_RELEASE( pIWbemLocator );
    SAFE_RELEASE( pIWbemServices );

    if( bCleanupCOM )
	{
        CoUninitialize();
		GIsCOMInitialized = FALSE;
	}

    return bIsXinputDevice;
}

/** This is a looping number between 0 and JOYSTICK_NUM_XINPUT_CONTROLLERS - 1*/
INT GXInputGUIDIndex = 0;

BOOL CALLBACK EnumJoystickChangesCallback( LPCDIDEVICEINSTANCE Instance, LPVOID Context )
{
	// Check if this device is already known to us.
	INT NumJoysticks = UWindowsClient::Joysticks.Num();
	INT ReplaceDeviceIndex = -1;
	for ( INT JoystickIndex=0; JoystickIndex < NumJoysticks; ++JoystickIndex )
	{
		FJoystickInfo& JoystickInfo = UWindowsClient::Joysticks( JoystickIndex );
		if ( Instance->guidInstance == JoystickInfo.DeviceGUID )
		{
			return DIENUM_CONTINUE;
		}

		// See if we have any devices we can replace.
		if ( ReplaceDeviceIndex < 0 && JoystickInfo.DirectInput8Joystick == NULL )
		{
			ReplaceDeviceIndex = JoystickIndex;
		}
	}

	// Don't add XInput controllers like this (they're pre-created).
	if ( IsXInputDevice(&Instance->guidProduct) )
	{
		// Use a FIFO among the XInput controllers to store this GUID.
		// It doesn't have to match the any particular controller, just so it gets caught
		// by the loop above. Calling IsXInputDevice() is really slow. :(
		UWindowsClient::Joysticks( GXInputGUIDIndex++ ).DeviceGUID = Instance->guidInstance;
		GXInputGUIDIndex = GXInputGUIDIndex % JOYSTICK_NUM_XINPUT_CONTROLLERS;
		return DIENUM_CONTINUE;
	}

	// Add a new device.
	LPDIRECTINPUTDEVICE8 DirectInput8Joystick = NULL;
	if( SUCCEEDED( UWindowsClient::DirectInput8->CreateDevice( Instance->guidInstance, &DirectInput8Joystick, NULL ) ) ) 
	{
		DIDEVCAPS JoystickCaps;
		JoystickCaps.dwSize = sizeof(JoystickCaps);

		verify( SUCCEEDED( DirectInput8Joystick->SetDataFormat( &c_dfDIJoystick2 ) ) );
		verify( SUCCEEDED( DirectInput8Joystick->GetCapabilities( &JoystickCaps ) ) );
		verify( SUCCEEDED( DirectInput8Joystick->EnumObjects( EnumAxesCallback, DirectInput8Joystick, DIDFT_AXIS ) ) );
	
		EJoystickType JoystickType = JOYSTICK_None;

		// Support X360 controller
		if( JoystickCaps.dwAxes			== 5
			&&	JoystickCaps.dwButtons	== 10
			&&	JoystickCaps.dwPOVs		== 1 )
		{
			debugf(TEXT("Found X360 controller"));
			JoystickType = JOYSTICK_X360;
		}

		// Xbox Type S controller with old drivers.
		if( JoystickCaps.dwAxes		== 4 
		&&	JoystickCaps.dwButtons	== 16 
		&&	JoystickCaps.dwPOVs		== 1 )
		{
			debugf(TEXT("Found Xbox Type S controller with old drivers"));
			JoystickType = JOYSTICK_Xbox_Type_S;
		}

		// Xbox Type S controller with new drivers.
		if( JoystickCaps.dwAxes		== 7
		&&	JoystickCaps.dwButtons	== 24 
		&&	JoystickCaps.dwPOVs		== 1
		)
		{
			debugf(TEXT("Found Xbox Type S controller with new drivers"));
			JoystickType = JOYSTICK_Xbox_Type_S;
		}

		// PS2 controller (old converter).
		if( JoystickCaps.dwAxes		== 5
		&&	JoystickCaps.dwButtons	== 16 
		&&	JoystickCaps.dwPOVs		== 1
		)
		{
			debugf(TEXT("Found PS2 controller with old converter"));
			JoystickType = JOYSTICK_PS2_Old_Converter;
		}

		// PS2 controller (new converter).
		if( JoystickCaps.dwAxes		== 4
		&&	JoystickCaps.dwButtons	== 12
		&&	JoystickCaps.dwPOVs		== 1
		)
		{
			debugf(TEXT("Found PS2 controller with new converter"));
			JoystickType = JOYSTICK_PS2_New_Converter;
		}

		//GameCaster Camera
		if( JoystickCaps.dwAxes			== 16
			&&	JoystickCaps.dwButtons	== 16
			&&	JoystickCaps.dwPOVs		== 0
			&&	JoystickCaps.dwDevType	== 66069
			)
		{
			debugf(TEXT("Found GameCaster controller"));
			JoystickType = JOYSTICK_GameCaster;
		}


		if(JoystickType != JOYSTICK_None)
		{
			// Is this replacing another (disconnected device)?
			FJoystickInfo* JoystickInfo;
			if ( ReplaceDeviceIndex >= 0 )
			{
				JoystickInfo = &UWindowsClient::Joysticks(ReplaceDeviceIndex);
				JoystickInfo->ControllerId = ReplaceDeviceIndex;
			}
			else
			{
				JoystickInfo = new(UWindowsClient::Joysticks) FJoystickInfo;
				JoystickInfo->ControllerId = UWindowsClient::Joysticks.Num() - 1;
			}
			JoystickInfo->DirectInput8Joystick = DirectInput8Joystick;
			JoystickInfo->JoystickType = JoystickType;
			JoystickInfo->DeviceGUID = Instance->guidInstance;
			JoystickInfo->bIsConnected = TRUE;

			appMemzero(JoystickInfo->JoyStates, sizeof(JoystickInfo->JoyStates));
			appMemzero(JoystickInfo->NextRepeatTime, sizeof(JoystickInfo->NextRepeatTime));
		}
		else
		{
			DirectInput8Joystick->Release();
			DirectInput8Joystick = NULL;
		}
	}

	return DIENUM_CONTINUE;
}


BOOL CALLBACK EnumAxesCallback( LPCDIDEVICEOBJECTINSTANCE Instance, LPVOID Context )
{
	LPDIRECTINPUTDEVICE8 DirectInput8Joystick = (LPDIRECTINPUTDEVICE8)Context;
	check(DirectInput8Joystick);

	DIPROPRANGE Range; 
	Range.diph.dwSize       = sizeof(DIPROPRANGE); 
	Range.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
	Range.diph.dwHow        = DIPH_BYOFFSET; 
	Range.diph.dwObj        = Instance->dwOfs; // Specify the enumerated axis

	// Ideally we'd like a range of +/- 1 though sliders can't go negative so we'll map 0..65535 to +/- 1 when polling.
	Range.lMin              = 0;
	Range.lMax              = 65535;

	// Set the range for the axis
	DirectInput8Joystick->SetProperty( DIPROP_RANGE, &Range.diph );

	return DIENUM_CONTINUE;
}


/** Handle to the low-level keyboard hook function */
HHOOK GLowLevelKeyboardHook = NULL;

/** Thread procedure for the low-level keyboard hook function */
DWORD WINAPI LowLevelKeyboardThreadFunc( LPVOID unused )
{
	// Wait for a WM_USER message (which tells us to stop the thread).
	MSG Message;
	while (GetMessage(&Message, NULL, 0, 0))
	{
		if ( Message.message == WM_USER )
		{
			// Shutdown request?
			if ( Message.wParam == 0 )
			{
				break;
			}
			// Enable keyboard hook (to disallow the WIN-key)?
			else if ( Message.wParam == 1 && GLowLevelKeyboardHook == NULL )
			{
				// Install a low-level keyboard hook to catch the Windows key.
				GLowLevelKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0 );
			}
			// Disable keyboard hook (to allow the WIN-key)?
			else if ( Message.wParam == 2 && GLowLevelKeyboardHook != NULL )
			{
				// Remove the low-level keyboard hook.
				UnhookWindowsHookEx( GLowLevelKeyboardHook );
				GLowLevelKeyboardHook = NULL;
			}
		}
	}

	if ( GLowLevelKeyboardHook )
	{
		// Remove the low-level keyboard hook.
		UnhookWindowsHookEx( GLowLevelKeyboardHook );
		GLowLevelKeyboardHook = NULL;
	}
	return 0;
}

/** Low-level keyboard hook function */
LRESULT CALLBACK LowLevelKeyboardProc( INT Code, WPARAM wParam, LPARAM lParam )
{
    if (Code < 0 || Code != HC_ACTION )  // do not process message 
	{
        return CallNextHookEx( GLowLevelKeyboardHook, Code, wParam, lParam);
	}
 
    UBOOL bEatKeystroke = FALSE;
    KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
    switch (wParam) 
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
			if ( p->vkCode == VK_LWIN || p->vkCode == VK_RWIN )
			{
				UBOOL IsFullscreenDevice = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport && GEngine->GameViewport->IsFullScreenViewport();
				bEatKeystroke = GIsGame && IsFullscreenDevice && GWindowActive;
			}
            break;
        }
    }
 
    if ( bEatKeystroke )
	{
        return 1;
	}
    else
	{
        return CallNextHookEx( GLowLevelKeyboardHook, Code, wParam, lParam );
	}
}


//
//	Static variables.
//
TArray<FWindowsViewport*>	UWindowsClient::Viewports;
LPDIRECTINPUT8				UWindowsClient::DirectInput8;
LPDIRECTINPUTDEVICE8		UWindowsClient::DirectInput8Mouse;
TArray<FJoystickInfo>		UWindowsClient::Joysticks;
HANDLE						UWindowsClient::KeyboardHookThread;
DWORD						UWindowsClient::KeyboardHookThreadId;
