/*=============================================================================
	MacClient.h: Unreal Mac platform interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MACCLIENT_H__
#define __MACCLIENT_H__

#include "MacObjCWrapper.h"	// for MacEvent

// Forward declarations.
class FMacViewport;

//
//	UMacClient - Mac-specific client code.
//

class UMacClient : public UClient
{
	DECLARE_CLASS_INTRINSIC(UMacClient, UClient, CLASS_Transient|CLASS_Config, MacDrv)

	/** Mac currently has only one viewport */
	FMacViewport*	Viewport;

	/** The command to run when the server is determined after the Bluetooth Peer picker runs */
	FString BTPickerServerCommand;

	/** The command to run when the client is determined after the Bluetooth Peer picker runs */
	FString BTPickerClientCommand;

	// Variables.
	UEngine*		Engine;

	// Audio device.
	UClass*			AudioDeviceClass;
	UAudioDevice*	AudioDevice;

	// Constructors.
	UMacClient();
	void StaticConstructor();

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
	virtual void FinishDestroy();

	// UClient interface.
	virtual void Init(UEngine* InEngine);
	virtual void Tick( FLOAT DeltaTime );
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	virtual void AllowMessageProcessing( UBOOL InValue ) { }

	virtual FViewportFrame* CreateViewportFrame(FViewportClient* ViewportClient,const TCHAR* InName,UINT SizeX,UINT SizeY,UBOOL Fullscreen = 0);
	virtual FViewport* CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX=0,UINT SizeY=0,INT InPosX = -1, INT InPosY = -1);
	virtual void CloseViewport(FViewport* Viewport);

	virtual class UAudioDevice* GetAudioDevice() { return AudioDevice; }

	virtual void ForceClearForceFeedback() {}

	/**
	 * Retrieves the name of the key mapped to the specified character code.
	 *
	 * @param	KeyCode	the key code to get the name for; should be the key's ANSI value
	 */
	virtual FName GetVirtualKeyName( INT KeyCode ) const;

	TMap<short,FName>	KeyMapAppleCodeToName;
};

/**
 * A Mac implementation of FViewport and FViewportFrame.
 */
class FMacViewport : public FViewportFrame, public FViewport
{
public:

	enum EForceCapture		{EC_ForceCapture};

	/**
	 * Minimal initialization constructor.
	 */
	FMacViewport(
		UMacClient* InClient,
		FViewportClient* InViewportClient,
		const TCHAR* InName,
		UINT InSizeX,
		UINT InSizeY,
		UBOOL InFullscreen,
		INT InPosX = -1,
		INT InPosY = -1
		);

	/** Destructor. */
	~FMacViewport();

	// FViewport interface.
	virtual void*	GetWindow()					{ return &Window; }
	virtual UBOOL	KeyState(FName Key) const;
	virtual UBOOL	CaptureJoystickInput(UBOOL Capture);

	// New MouseCapture/MouseLock API
	virtual UBOOL	HasMouseCapture() const		{ return bCapturingMouseInput; }
	virtual UBOOL	HasFocus() const;

	/**
	 * Returns true if the mouse cursor is currently visible
	 *
	 * @return True if the mouse cursor is currently visible, otherwise false.
	 */
	virtual UBOOL	IsCursorVisible() const;

	FString			GetName() { return Name; }
	virtual INT		GetMouseX();
	virtual INT		GetMouseY();
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

	void AddMacEventToQueue( MacEvent* Event )
	{
		InputEvents.Push( *Event );
	}

private:

	UMacClient*				Client;
	DoubleWindow			Window;

	FString					Name;

	// New MouseCapture/MouseLock API
	EMouseCursor			SystemMouseCursor;
	UBOOL					bMouseLockRequested;
	UBOOL					bCapturingMouseInput;
	UBOOL					PreventCapture;

	TArray<MacEvent>		InputEvents;
	INT						CurrentMousePosX;
	INT						CurrentMousePosY;

#if !FINAL_RELEASE
	/** If true, we want to fake a mobile touch event.  Reroute mouse input to the mobile input system */
	UBOOL					bFakeMobileTouches;

	/** If true, we are in the middle of a touch */
	UBOOL					bIsFakingMobileTouch;
#endif
};

#endif
