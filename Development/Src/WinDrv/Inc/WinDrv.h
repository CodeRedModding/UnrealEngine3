/*=============================================================================
	WinDrv.h: Unreal Windows viewport and platform driver.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
	* Rewritten by Andrew Scheidecker and Daniel Vogel
=============================================================================*/

#ifndef _INC_WINDRV
#define _INC_WINDRV

/**
 * The joystick support is hardcoded to either handle an Xbox 1 Type S controller or
 * a PlayStation 2 Dual Shock controller plugged into a PC.
 *
 * See http://www.redcl0ud.com/xbcd.html for Windows drivers for Xbox 1 Type S.
 */
#define EMULATE_CONSOLE_CONTROLLER

#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

// Unreal includes.
#include "Engine.h"

// Windows includes.
#pragma pack (push,8)
#define DIRECTINPUT_VERSION 0x800
#include <dinput.h>
#pragma pack (pop)

/**
 * Joystick types supported by console controller emulation.	
 */
enum EJoystickType
{
	JOYSTICK_None,
	JOYSTICK_PS2,
	JOYSTICK_Xbox_Type_S
};

// Forward declarations.
struct FWindowsViewport;

//
//	UWindowsClient - Windows-specific client code.
//
class UWindowsClient : public UClient
{
	DECLARE_CLASS(UWindowsClient,UClient,CLASS_Transient|CLASS_Config,WinDrv)

	// Static variables.
	static TArray<FWindowsViewport*>	Viewports;
	static LPDIRECTINPUT8				DirectInput8;
	static LPDIRECTINPUTDEVICE8			DirectInput8Mouse;
#ifdef EMULATE_CONSOLE_CONTROLLER
	static LPDIRECTINPUTDEVICE8			DirectInput8Joystick;
	static EJoystickType				JoystickType;
#endif

	// Variables.
	UEngine*							Engine;
	FString								WindowClassName;

	TMap<BYTE,FName>					KeyMapVirtualToName;
	TMap<FName,BYTE>					KeyMapNameToVirtual;

	/** Whether attached viewports process windows messages or not */
	UBOOL								ProcessWindowsMessages;

	// Render device.
	UClass*								RenderDeviceClass;
	URenderDevice*						RenderDevice;

	// Audio device.
	UClass*								AudioDeviceClass;
	UAudioDevice*						AudioDevice;

	// Constructors.
	UWindowsClient();
	void StaticConstructor();

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
	virtual void Destroy();
	virtual void ShutdownAfterError();

	// UClient interface.
	virtual void Init(UEngine* InEngine);
	virtual void Flush();
	virtual void Tick( FLOAT DeltaTime );
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);
	/**
	 * PC only, debugging only function to prevent the engine from reacting to OS messages. Used by e.g. the script
	 * debugger to avoid script being called from within message loop (input).
	 *
	 * @param InValue	If FALSE disallows OS message processing, otherwise allows OS message processing (default)
	 */
	virtual void AllowMessageProcessing( UBOOL InValue ) { ProcessWindowsMessages = InValue; }

	virtual FViewport* CreateViewport(FViewportClient* ViewportClient,const TCHAR* Name,UINT SizeX,UINT SizeY,UBOOL Fullscreen = 0);
	virtual FChildViewport* CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX=0,UINT SizeY=0);
	virtual void CloseViewport(FChildViewport* Viewport);

	virtual class URenderDevice* GetRenderDevice() { return RenderDevice; }
	virtual class UAudioDevice* GetAudioDevice() { return AudioDevice; }
	
	// Window message handling.
	static LONG APIENTRY StaticWndProc( HWND hWnd, UINT Message, UINT wParam, LONG lParam );
};


//
//	FWindowsViewport
//
struct FWindowsViewport: FViewport
{
	// Viewport state.
	UWindowsClient*			Client;
	HWND					Window,
							ParentWindow;
	UBOOL					InitializedRenderDevice;

	FString					Name;
	
	UINT					SizeX,
							SizeY;

	UBOOL					Fullscreen,
							Minimized,
							Maximized,
							Resizing,
							PreventCapture;

	UBOOL					KeyStates[256];
	UBOOL					bMouseHasMoved;

	// Info saved during captures and fullscreen sessions.
	POINT					PreCaptureMousePos;
	RECT					PreCaptureCursorClipRect;
	UBOOL					bCapturingMouseInput;
	UBOOL					bCapturingJoystickInput;
	UBOOL					bLockingMouseToWindow;

	enum EForceCapture		{EC_ForceCapture};

	// Constructor/destructor.
	FWindowsViewport(UWindowsClient* InClient,FViewportClient* InViewportClient,const TCHAR* InName,UINT InSizeX,UINT InSizeY,UBOOL InFullscreen,HWND InParentWindow);

	// FChildViewport interface.
	virtual void* GetWindow() { return Window; }

	virtual UBOOL CaptureMouseInput(UBOOL Capture);
	virtual void LockMouseToWindow(UBOOL bLock);
	virtual UBOOL KeyState(FName Key);

	virtual UBOOL CaptureJoystickInput(UBOOL Capture);

	virtual INT GetMouseX();
	virtual INT GetMouseY();

	virtual UBOOL MouseHasMoved() { return bMouseHasMoved; }

	virtual UINT GetSizeX() { return SizeX; }
	virtual UINT GetSizeY() { return SizeY; }

	virtual void Draw(UBOOL Synchronous);
	virtual UBOOL ReadPixels(TArray<FColor>& OutputBuffer);
	virtual void InvalidateDisplay();

	virtual void GetHitProxyMap(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<HHitProxy*>& OutMap);
	virtual void Invalidate();

	// FViewport interface.
	virtual const TCHAR* GetName() { return *Name; }
	virtual UBOOL IsFullscreen() { return Fullscreen; }
	virtual void SetName(const TCHAR* NewName);
	virtual void Resize(UINT NewSizeX,UINT NewSizeY,UBOOL NewFullscreen);
	
	// FWindowsViewport interface.
	void ShutdownAfterError();
	void ProcessInput( FLOAT DeltaTime );
	void HandlePossibleSizeChange();
	LONG ViewportWndProc(UINT Message,UINT wParam,LONG lParam);
	void Destroy();
};

#define AUTO_INITIALIZE_REGISTRANTS_WINDRV \
	UWindowsClient::StaticClass();

/*-----------------------------------------------------------------------------
	Unicode support.
-----------------------------------------------------------------------------*/

#define SetClassLongX(a,b,c)						TCHAR_CALL_OS(SetClassLongW(a,b,c),SetClassLongA(a,b,c))
#define GetClassLongX(a,b)							TCHAR_CALL_OS(GetClassLongW(a,b),GetClassLongA(a,b))
#define RemovePropX(a,b)							TCHAR_CALL_OS(RemovePropW(a,b),RemovePropA(a,TCHAR_TO_ANSI(b)))
#define DispatchMessageX(a)							TCHAR_CALL_OS(DispatchMessageW(a),DispatchMessageA(a))
#define PeekMessageX(a,b,c,d,e)						TCHAR_CALL_OS(PeekMessageW(a,b,c,d,e),PeekMessageA(a,b,c,d,e))
#define PostMessageX(a,b,c,d)						TCHAR_CALL_OS(PostMessageW(a,b,c,d),PostMessageA(a,b,c,d))
#define SendMessageX(a,b,c,d)						TCHAR_CALL_OS(SendMessageW(a,b,c,d),SendMessageA(a,b,c,d))
#define SendMessageLX(a,b,c,d)						TCHAR_CALL_OS(SendMessageW(a,b,c,(LPARAM)d),SendMessageA(a,b,c,(LPARAM)(TCHAR_TO_ANSI((TCHAR *)d))))
#define SendMessageWX(a,b,c,d)						TCHAR_CALL_OS(SendMessageW(a,b,(WPARAM)c,d),SendMessageA(a,b,(WPARAM)(TCHAR_TO_ANSI((TCHAR *)c)),d))
#define DefWindowProcX(a,b,c,d)						TCHAR_CALL_OS(DefWindowProcW(a,b,c,d),DefWindowProcA(a,b,c,d))
#define CallWindowProcX(a,b,c,d,e)					TCHAR_CALL_OS(CallWindowProcW(a,b,c,d,e),CallWindowProcA(a,b,c,d,e))
#define GetWindowLongX(a,b)							TCHAR_CALL_OS(GetWindowLongW(a,b),GetWindowLongA(a,b))
#define SetWindowLongX(a,b,c)						TCHAR_CALL_OS(SetWindowLongW(a,b,c),SetWindowLongA(a,b,c))
#define LoadMenuIdX(i,n)							TCHAR_CALL_OS(LoadMenuW(i,MAKEINTRESOURCEW(n)),LoadMenuA(i,MAKEINTRESOURCEA(n)))
#define LoadCursorIdX(i,n)							TCHAR_CALL_OS(LoadCursorW(i,MAKEINTRESOURCEW(n)),LoadCursorA(i,MAKEINTRESOURCEA(n)))
#ifndef LoadIconIdX
#define LoadIconIdX(i,n)							TCHAR_CALL_OS(LoadIconW(i,MAKEINTRESOURCEW(n)),LoadIconA(i,MAKEINTRESOURCEA(n)))
#endif
#define CreateWindowExX(a,b,c,d,e,f,g,h,i,j,k,l)	TCHAR_CALL_OS(CreateWindowEx(a,b,c,d,e,f,g,h,i,j,k,l),CreateWindowExA(a,TCHAR_TO_ANSI(b),TCHAR_TO_ANSI(c),d,e,f,g,h,i,j,k,l))

#endif //_INC_WINDRV
