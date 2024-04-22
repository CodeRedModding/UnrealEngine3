/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __LEVEL_VIEWPORT_HOST_WINDOW_H__
#define __LEVEL_VIEWPORT_HOST_WINDOW_H__

#include "UnrealEd.h"

//-----------------------------------------------
//Actual viewport window
//-----------------------------------------------

using namespace System::Runtime::InteropServices;

ref class MLevelViewportHwndHost
: public Interop::HwndHost
{

public:

	MLevelViewportHwndHost( ELevelViewportType InViewportType, EShowFlags InShowFlags )
		: InitialViewportType( InViewportType ),
		LevelViewportClient(NULL),
		bUseJoystickCatpure(FALSE)
	{
		// level viewport = new viewport ( hwndPArent );
		LevelViewportClient = new FEditorLevelViewportClient();
		LevelViewportClient->ViewportType = InitialViewportType;
		LevelViewportClient->bSetListenerPosition = FALSE;
		LevelViewportClient->ShowFlags = InShowFlags;
		LevelViewportClient->LastShowFlags = InShowFlags;

		LevelViewportClient->SetRealtime(TRUE);

		// Assign default camera location/rotation for perspective camera
		LevelViewportClient->ViewLocation = EditorViewportDefs::DefaultPerspectiveViewLocation;
		LevelViewportClient->ViewRotation = EditorViewportDefs::DefaultPerspectiveViewRotation;
	}

	/**Accessor to the Level Viewport Client*/
	FEditorLevelViewportClient* GetLevelViewportClient(void) 
	{ 
		return LevelViewportClient;
	}

	/**Save off joystick input for viewport creation time*/
	void CaptureJoystickInput(const UBOOL bInJoystickCapture)
	{
		check(LevelViewportClient);
		bUseJoystickCatpure = bInJoystickCapture;
		if (LevelViewportClient->Viewport)
		{
			LevelViewportClient->Viewport->CaptureJoystickInput(bUseJoystickCatpure);
		}
	}

protected:

	virtual HandleRef BuildWindowCore( HandleRef hwndParent ) override
	{
		// Create viewport
		LevelViewportClient->Viewport = GEngine->Client->CreateWindowChildViewport( LevelViewportClient, (HWND)(PTRINT)hwndParent.Handle );
		//send down again now that we've made the viewport
		CaptureJoystickInput(bUseJoystickCatpure);

		return HandleRef( this, (IntPtr)(PTRINT)LevelViewportClient->Viewport->GetWindow() );
	}


	virtual void DestroyWindowCore( HandleRef hwnd ) override
	{
		GEngine->Client->CloseViewport( LevelViewportClient->Viewport );
		LevelViewportClient->Viewport = NULL;

		delete LevelViewportClient;
		LevelViewportClient = NULL;
	}


	virtual IntPtr WndProc( IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, bool% handled ) override
	{
		// ...

		return IntPtr::Zero;
	}

private:

	ELevelViewportType InitialViewportType;
	FEditorLevelViewportClient* LevelViewportClient;

	/**True if this viewport is intended for use with a joystick*/
	UBOOL bUseJoystickCatpure;
};


//-----------------------------------------------
// Viewport Panel
//-----------------------------------------------
ref class MViewportPanel : public MWPFPanel
{
public:
	MViewportPanel(String^ InXamlName, EShowFlags InShowFlags)
		: MWPFPanel(InXamlName)
	{
		InterpEd = nullptr;

		//save off region to draw the viewport to
		ViewportBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( this, "ViewportBorder" ) );

		ViewportHost = gcnew MLevelViewportHwndHost(LVT_Perspective, InShowFlags);
		//hook viewport to rect
		ViewportBorder->Child = ViewportHost;
	}

	virtual ~MViewportPanel(void)
	{
		if (ViewportHost != nullptr)
		{
			delete ViewportHost;
			ViewportHost = nullptr;
		}
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame (MWPFFrame^ InParentFrame) override
	{
		MWPFPanel::SetParentFrame(InParentFrame);
	}

	/**
	 * Connects viewport to matinee window
	 */
	void ConnectToMatineeCamera(WxInterpEd* InInterpEd, const int InCameraIndex)
	{
		check(InInterpEd);

		//save off pointer for camera reference
		InterpEd = InInterpEd;

		AActor* FollowActor = InterpEd->GetCameraActor(InCameraIndex);
		InterpEd->AddExtraViewport(ViewportHost->GetLevelViewportClient(), FollowActor);
		ViewportHost->GetLevelViewportClient()->bDrawAxes = FALSE;
		ViewportHost->GetLevelViewportClient()->bDisableInput = TRUE;

		ViewportHost->GetLevelViewportClient()->SetAllowMatineePreview(TRUE);
	}

	/**Enable viewport recording*/
	void EnableMatineeRecording (void)
	{
		check(ViewportHost);
		check(ViewportHost->GetLevelViewportClient());
		ViewportHost->CaptureJoystickInput(TRUE);
		ViewportHost->GetLevelViewportClient()->SetMatineeRecordingWindow(InterpEd);
	}

private:
	/** Internal widgets to save having to get in multiple places*/
	WxInterpEd* InterpEd;

	/**Wrapper for level viewport*/
	MLevelViewportHwndHost^ ViewportHost;

	//Rectangle to draw the viewport
	Border^ ViewportBorder;
};

#endif // #define __LEVEL_VIEWPORT_HOST_WINDOW_H__

