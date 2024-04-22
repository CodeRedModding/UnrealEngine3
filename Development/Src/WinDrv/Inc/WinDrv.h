/*=============================================================================
	WinDrv.h: Unreal Windows viewport and platform driver.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_WINDRV
#define _INC_WINDRV

/**
 * The joystick support is hardcoded to either handle an Xbox 1 Type S controller or
 * a PlayStation 2 Dual Shock controller plugged into a PC.
 *
 * See http://www.redcl0ud.com/xbcd.html for Windows drivers for Xbox 1 Type S.
 */

/** This is how many joystick buttons are supported. Must be less than 256 */
#define MAX_JOYSTICK_BUTTONS		16

#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

// Unreal includes.
#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"

// Windows includes.
#pragma pack (push,8)
#define DIRECTINPUT_VERSION 0x800
#include <dinput.h>
#include <xinput.h>
#pragma pack (pop)

// Forward declarations.
class FWindowsViewport;

/** This variable reflects the latest status sent to us by WM_ACTIVATEAPP */
extern UBOOL GWindowActive;

/**
 * Joystick types supported by console controller emulation.	
 */
enum EJoystickType
{
	JOYSTICK_None,
	JOYSTICK_PS2_Old_Converter,
	JOYSTICK_PS2_New_Converter,
	JOYSTICK_X360,
	JOYSTICK_Xbox_Type_S,
	/**Game Caster Virtual Camera*/
	JOYSTICK_GameCaster,
};

#define JOYSTICK_NUM_XINPUT_CONTROLLERS	4

/**
 * Information about a joystick.
 */
struct FJoystickInfo
{
	/** The DirectInput interface to this Joystick. NULL for XInput controllers or if it's unplugged. */
	LPDIRECTINPUTDEVICE8 DirectInput8Joystick;

	/** The joystick's type. */
	EJoystickType JoystickType;

	/** The joystick's controller ID. All XInput controllers are set to JOYSTICK_X360. */
	INT ControllerId;

	/** Last frame's joystick button states, so we only send events on edges */
	UBOOL JoyStates[MAX_JOYSTICK_BUTTONS];

	/** Next time a IE_Repeat event should be generated for each button */
	DOUBLE NextRepeatTime[MAX_JOYSTICK_BUTTONS];

	/** Store previous joy state. Sometimes DInput fails to query joystick, we don't want to have negative impacts such as zero acceleration */
	DIJOYSTATE2 PreviousState;

	/** Device GUID */
	GUID DeviceGUID;

	/** Whether an XInput controller is connected or not */
	UBOOL bIsConnected;
};

/** Contains the state needed to defer processing of a window message. */
struct FDeferredMessage
{
	FWindowsViewport* Viewport;
	UINT Message;
	WPARAM wParam;
	LPARAM lParam;

#if WITH_WINTAB
	UBOOL bPenActive;
	FLOAT Pressure;	
#endif

	struct
	{
		SHORT LeftControl;
		SHORT RightControl;
		SHORT LeftShift;
		SHORT RightShift;
		SHORT Menu;
	}	KeyStates;
};

//
//	UWindowsClient - Windows-specific client code.
//
class UWindowsClient : public UClient
{
	DECLARE_CLASS_INTRINSIC(UWindowsClient,UClient,CLASS_Transient|CLASS_Config,WinDrv)

	// Static variables.
	static TArray<FWindowsViewport*>	Viewports;
	static LPDIRECTINPUT8				DirectInput8;
	static LPDIRECTINPUTDEVICE8			DirectInput8Mouse;
	static TArray<FJoystickInfo>		Joysticks;
	static HANDLE						KeyboardHookThread;		// Thread that manages the low-level keyboard hook
	static DWORD						KeyboardHookThreadId;	// Thread id for the KeyboardHookThread.

	// Variables.
	UEngine*							Engine;
	FString								WindowClassName;

	TMap<BYTE,FName>					KeyMapVirtualToName;
	TMap<FName,BYTE>					KeyMapNameToVirtual;

	/** A quick lookup to map PS2 controller button indices to Xbox controller button indices */
	BYTE		PS2ToXboxControllerMapping[MAX_JOYSTICK_BUTTONS];
	/** A quick lookup to map X360 XInput controller button indices to Xbox controller button indices */
	BYTE		X360ToXboxControllerMapping[MAX_JOYSTICK_BUTTONS];
	/** A quick lookup to map GameCaster controller button indices to Xbox controller button indices */
	BYTE		GameCasterToXboxControllerMapping[MAX_JOYSTICK_BUTTONS];
	/** An array of FNames that are sent to the input system. Maps Xbox button indices to FNames */
	FName		JoyNames[MAX_JOYSTICK_BUTTONS];

	/** Last time in ms (arbitrarily based) we received an input event for mouse x axis */
	DWORD								LastMouseXEventTime;
	/** Last time in ms (arbitrarily based) we received an input event for mouse y axis */
	DWORD								LastMouseYEventTime;

	/** Whether attached viewports process windows messages or not */
	UBOOL								ProcessWindowsMessages;

	/** Time until we check for newly connected XInput Controllers again */
	FLOAT								ControllerScanTime;

	/** Next controller to check for connection (Index into Joysticks array) */
	INT									NextControllerToCheck;

	// Audio device.
	UClass*								AudioDeviceClass;
	UAudioDevice*						AudioDevice;

	// Accessibility shortcut keys
	STICKYKEYS							StartupStickyKeys;
	TOGGLEKEYS							StartupToggleKeys;
	FILTERKEYS							StartupFilterKeys;

	/** Whether to allow joystick input. */
	UBOOL								bAllowJoystickInput;

	/** Messages that have been deferred for later processing at a predetermined point. */
	TArray<FDeferredMessage>			DeferredMessages;

	/** True if a remote desktop sesssion is currently active.  Dynamically updated. */
	static UBOOL						bRemoteDesktopSessionActive;

	/**
	 * Defers a message received by a viewport until a predetermined point in the frame.
	 */
	void DeferMessage(FWindowsViewport* Viewport,UINT Message,WPARAM wParam,LPARAM lParam);

	/** Processes window messages that have been deferred. */
	void ProcessDeferredMessages();

	// Constructors.
	UWindowsClient();
	void StaticConstructor();

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
	virtual void FinishDestroy();
	virtual void ShutdownAfterError();

	// UClient interface.
	virtual void Init(UEngine* InEngine);
	virtual void Tick( FLOAT DeltaTime );
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	/**
	 * PC only, debugging only function to prevent the engine from reacting to OS messages. Used by e.g. the script
	 * debugger to avoid script being called from within message loop (input).
	 *
	 * @param InValue	If FALSE disallows OS message processing, otherwise allows OS message processing (default)
	 */
	virtual void AllowMessageProcessing( UBOOL InValue ) { ProcessWindowsMessages = InValue; }

	virtual FViewportFrame* CreateViewportFrame(FViewportClient* ViewportClient,const TCHAR* InName,UINT SizeX,UINT SizeY,UBOOL Fullscreen = 0);
	virtual FViewport* CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX=0,UINT SizeY=0,INT InPosX = -1, INT InPosY = -1);
	virtual void CloseViewport(FViewport* Viewport);

	/**
	 * Helper function to return the play world viewport (if one exists)
	 * @return - NULL if there is no play world viewport, otherwise the propert viewport
	 */
	virtual FViewport* GetPlayWorldViewport (void);

#if WITH_EDITOR
	/** Called when a play world is active to tick the play in editor viewport */
	virtual void TickPlayInEditorViewport(FLOAT DeltaSeconds);
#endif

	virtual class UAudioDevice* GetAudioDevice() { return AudioDevice; }
		
	/** Function to immediately stop any force feedback vibration that might be going on this frame. */
	virtual void ForceClearForceFeedback();

	/**
	 * Toggles accessibility shortcut keys on and off
	 * 
	 * @param State - TRUE to enable shortcuts, FALSE to disable
	 */
	void AllowAccessibilityShortcutKeys( UBOOL AllowKeys );

	/**
	 * Retrieves the name of the key mapped to the specified character code.
	 *
	 * @param	KeyCode	the key code to get the name for; should be the key's ANSI value
	 */
	virtual FName GetVirtualKeyName( INT KeyCode ) const;

	// Window message handling.
	static LRESULT APIENTRY StaticWndProc( HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam );

	/**
	 *	Reads input from shared resources, such as buffered D3D mouse input.
	 */
	void ProcessInput( FLOAT DeltaTime );

	/** Poll mouse input */
	static UBOOL PollMouseInput( );

	/** Flush all mouse input */
	static void FlushMouseInput( );
};

/**
 * A Windows implementation of FViewport and FViewportFrame.
 */
class FWindowsViewport : public FViewportFrame, public FViewport
{
public:

	enum EForceCapture		{EC_ForceCapture};

	/**
	 * Minimal initialization constructor.
	 */
	FWindowsViewport(
		UWindowsClient* InClient,
		FViewportClient* InViewportClient,
		const TCHAR* InName,
		UINT InSizeX,
		UINT InSizeY,
		UBOOL InFullscreen,
		HWND InParentWindow,
		INT InPosX = -1,
		INT InPosY = -1
		);

	/** Destructor. */
	~FWindowsViewport();

	// FViewport interface.
	virtual void*	GetWindow()					{ return Window; }
	virtual UBOOL	KeyState(FName Key) const;
	virtual UBOOL	CaptureJoystickInput(UBOOL Capture);

	// New MouseCapture/MouseLock API
	virtual UBOOL	HasMouseCapture() const		{ return bCapturingMouseInput; }
	virtual UBOOL	HasFocus() const;
	virtual void	ShowCursor( UBOOL bVisible );

	/**
	 * Returns true if the mouse cursor is currently visible
	 *
	 * @return True if the mouse cursor is currently visible, otherwise false.
	 */
	virtual UBOOL	IsCursorVisible() const;

	virtual void	CaptureMouse( UBOOL bCapture );
	virtual void	LockMouseToWindow( UBOOL bLock );
	virtual void	UpdateMouseLock( UBOOL bEnforceMouseLockRequestedFlag = FALSE );

	FString			GetName() { return Name; }
	virtual INT		GetMouseX();
	virtual INT		GetMouseY();
	virtual FLOAT	GetTabletPressure();
	virtual UBOOL	IsPenActive();
	virtual void	GetMousePos( FIntPoint& MousePosition );
	virtual void	SetMouse(INT x, INT y);

	virtual void	InvalidateDisplay();

	virtual FViewportFrame* GetViewportFrame();
	void			ProcessInput( FLOAT DeltaTime );

	// FViewportFrame interface.
	virtual FViewport* GetViewport()			{ return this; }
	virtual void	Resize(UINT NewSizeX,UINT NewSizeY,UBOOL NewFullscreen,INT InPosX = -1, INT InPosY = -1);

	// FWindowsViewport interface.
	void			Destroy();
	void			ShutdownAfterError();	
	LONG			ViewportWndProc(UINT Message,WPARAM wParam,LPARAM lParam);
	void			ProcessDeferredMessage(const FDeferredMessage& Message);
	void			Tick(FLOAT DeltaTime);

	// Helper functions.
	UBOOL			UpdateMouseCursor( UBOOL bSetCursor );

	virtual void ToggleFakeMobileTouches(UBOOL bInFakeMobileTouches)
	{
#if !FINAL_RELEASE
		bFakeMobileTouches = bInFakeMobileTouches;
		if (!bFakeMobileTouches)
		{
			bIsFakingMobileTouch = FALSE;
		}
#endif
	}
	/**
	 * Returns true if mobile input emulation is enabled for this viewport
	 *
	 * @return	True if bFakeMobileTouches is set for this viewport(
	 */
	virtual UBOOL IsFakeMobileTouchesEnabled() const
	{
#if !FINAL_RELEASE
		return bFakeMobileTouches;
#else
		return FALSE;
#endif
	}

private:

	// Viewport state.
	UWindowsClient*			Client;
	HWND					Window;
	HWND					ParentWindow;

	FString					Name;

	UBOOL					Minimized;
	UBOOL					Maximized;
	UBOOL					Resizing;			// TRUE when the user is resizing by dragging
	UBOOL					bPerformingSize;	// TRUE when the game is resizing insde Resize()

	UBOOL					KeyStates[256];

	// New MouseCapture/MouseLock API
	EMouseCursor			SystemMouseCursor;
	UBOOL					bMouseLockRequested;
	UBOOL					bCapturingMouseInput;
	UBOOL					PreventCapture;
	RECT                    MouseLockClientRect;

	// Old MouseCapture/MouseLock API (Info saved during captures and fullscreen sessions).
	POINT					PreCaptureMousePos;
	RECT					PreCaptureCursorClipRect;
	UBOOL					bCapturingJoystickInput;
	UBOOL					bLockingMouseToWindow;

	/** Last time in ms (arbitrarily based) we received an input event for mouse x axis */
	DWORD					LastMouseXEventTime;
	/** Last time in ms (arbitrarily based) we received an input event for mouse y axis */
	DWORD					LastMouseYEventTime;

	/** If TRUE, an UpdateModifierState() is enqueued. */
	UBOOL					bUpdateModifierStateEnqueued;

	/** True if ResetMouseButtons should be called after receiving focus */
	UBOOL					bShouldResetMouseButtons;

	/** Information about the state of the window before the window went fullscreen. */
	RECT					PreFullscreenWindowRect;
#if !FINAL_RELEASE
	/** If true, we want to fake a mobile touch event.  Reroute mouse input to the mobile input system */
	UBOOL					bFakeMobileTouches;

	/** If true, we are in the middle of a touch */
	UBOOL					bIsFakingMobileTouch;
#endif

#if WITH_IME
	/** Whether or not the OS supports IME */
	UBOOL					bSupportsIME;
	INT						CurrentIMESize;
#endif

#if WITH_WINTAB
	/** Wintab contet for this viewport */
	HANDLE					WintabContext;
	UBOOL					bLastPenActive;
	FLOAT					LastPressure;
	UBOOL					bCurrentPenActive;
	FLOAT					CurrentPressure;
	friend class UWindowsClient;
#endif

	void	HandlePossibleSizeChange( );
	void	UpdateRenderDevice( UINT NewSizeX, UINT NewSizeY, UBOOL bNewIsFullscreen, UBOOL bSaveResolutionSettings );
	void	UpdateWindow( UBOOL NewFullscreen, DWORD WindowStyle, INT WindowPosX, INT WindowPosY, INT WindowWidth, INT WindowHeight );
	void	UpdateModifierState( );
	void	UpdateModifierState( INT VirtKey );

	void	OnMouseButtonDoubleClick( UINT Message, WPARAM wParam );
	void	OnMouseButtonDown( UINT Message, WPARAM wParam );
	void	OnMouseButtonUp( UINT Message, WPARAM wParam );

	/**
	 * Resets mouse buttons if we were asked to by a message handler
	 */
	void ConditionallyResetMouseButtons();
};

/**
 * StoreKit subclass of generic MicroTrans API
 */
class USwrveAnalyticsWindows : public UAnalyticEventsBase
{
	DECLARE_CLASS_INTRINSIC(USwrveAnalyticsWindows, UAnalyticEventsBase, 0, WinDrv)
public:
	// UAnalyticEventsBase interface
	virtual void Init();
	virtual void BeginDestroy();
	virtual void SetUserId(const FString& NewUserId);
	virtual void StartSession();
    virtual void EndSession();
    virtual void LogStringEvent(const FString& EventName,UBOOL bTimed);
    virtual void LogStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue,UBOOL bTimed);
    virtual void LogStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray,UBOOL bTimed);
    virtual void LogErrorMessage(const FString& ErrorName,const FString& ErrorMessage);
	virtual void LogUserAttributeUpdate(const FString& AttributeName, const FString& AttributeValue);
	virtual void LogUserAttributeUpdateArray(const TArray<FEventStringParam>& ParamArray);
	virtual void LogItemPurchaseEvent(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity);
	virtual void LogCurrencyPurchaseEvent(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider);
	virtual void LogCurrencyGivenEvent(const FString& GameCurrencyType, int GameCurrencyAmount);

private:
	/** code to generate a Swrve-compatible session token. Not needed currently but left here if we need it in the future. */
	FString GetSessionToken();
	/** Sends a request to SWRVE (helper func). */
	void SendToSwrve(const FString& MethodName, const FString& OptionalParams, const FString& Payload);

	/** Sends a GET request to SWRVE (helper func). */
	void SendToSwrve(const FString& MethodName, const FString& OptionalParams)
	{
		SendToSwrve(MethodName, OptionalParams, FString());
	}
	/** Sends a GET request to SWRVE (helper func). */
	void SendToSwrve(const FString& MethodName)
	{
		SendToSwrve(MethodName, FString(), FString());
	}
	/** Helper to log any swrve event. Used by all the LogXXX functions. */
	void SwrveLogEvent(const FString& EventName, const TArray<FEventStringParam>& ParamArray);

	/** Swrve Game ID Number - Get from your account manager */
	INT GameID;
	/** Swrve Game API Key - Get from your account manager */
	FString GameAPIKey;
	/** Swrve API Server - should be http://api.swrve.com/ */
	FString APIServer;
	/** Swrve A/B test Server - should be http://abtest.swrve.com/ */
	FString ABTestServer;
	/** Time when the session began (to generate session token) */
	INT SessionStartTime;
	/** Cached session token so we don't regenerate every time. */
	FString CachedSessionToken;
};

#include "XnaForceFeedbackManager.h"

#endif //_INC_WINDRV
