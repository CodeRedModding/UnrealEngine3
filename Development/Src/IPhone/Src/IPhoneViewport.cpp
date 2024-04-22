/*=============================================================================
	IPhoneViewport.cpp: FIPhoneViewport code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "IPhoneDrv.h"
#include "IPhoneInput.h"
#include "IPhoneObjCWrapper.h"
#include "EngineGameEngineClasses.h"

#include "EngineAIClasses.h"			// needed only for gameframeworkclasses.h
#include "EngineSequenceClasses.h"		// needed only for gameframeworkclasses.h
#include "EngineUserInterfaceClasses.h"	// needed only for gameframeworkclasses.h
#include "EnginePhysicsClasses.h"		// needed only for gameframeworkclasses.h
#include "GameFrameworkClasses.h"

// Global singleton
FIPhoneInputManager GIPhoneInputManager;

FLOAT MobileMaxSpeedModifier;

void FIPhoneViewport::StaticInit()
{

}

//
//	FIPhoneViewport::FIPhoneViewport
//
FIPhoneViewport::FIPhoneViewport(UIPhoneClient* InClient, FViewportClient* InViewportClient, UINT InSizeX, UINT InSizeY, void* InWindowHandle)
	: FViewport(InViewportClient)
	, Client(InClient)
	, bIsTiltActive(FALSE)
	, WindowHandle(InWindowHandle)
{
	// Creates the viewport window.
	Resize(InSizeX, InSizeY, TRUE);

	InitAppVersionString();
}

//
//	FIPhoneViewport::Destroy
//

void FIPhoneViewport::Destroy()
{
	ViewportClient = NULL;

	// Release the viewport RHI.
	UpdateViewportRHI(TRUE, SizeX, SizeY, TRUE);
}

//
//	FIPhoneViewport::Resize
//
void FIPhoneViewport::Resize(UINT NewSizeX, UINT NewSizeY, UBOOL NewFullscreen, INT InPosX, INT InPosY)
{
	SizeX					= NewSizeX;
	SizeY					= NewSizeY;

	// if we have a size, update the viewport RHI, otherwise destroy it
	if (NewSizeX && NewSizeY)
	{
		UpdateViewportRHI(FALSE, SizeX, SizeY, TRUE);
	}
	else
	{
		UpdateViewportRHI(TRUE, SizeX, SizeY, TRUE);
	}

	// Update any mobile input zones after a viewport resize
	extern void UpdateMobileInputZoneLayout();
	UpdateMobileInputZoneLayout();
}

FViewportFrame* FIPhoneViewport::GetViewportFrame()
{
	return this;
}

//
//	FIPhoneViewport::KeyState
//
UBOOL FIPhoneViewport::KeyState(FName Key) const
{
	// iphone won't have keys down to control the game
	return FALSE;
}

//
//	FIPhoneViewport::ProcessInput
//
void FIPhoneViewport::ProcessInput(FLOAT DeltaTime)
{
	if( !ViewportClient )
	{
		return;
	}

	// get input events
	FIPhoneInputManager::QueueType QueuedEvents;
	GIPhoneInputManager.GetAllEvents(QueuedEvents);

	for (INT QueueIndex = 0; QueueIndex < QueuedEvents.Num(); QueueIndex++)
	{
		TArray<FIPhoneTouchEvent>& Events = QueuedEvents(QueueIndex);
		// go over each one and process it
		for (INT EventIndex = 0; EventIndex < Events.Num(); EventIndex++)
		{
			// Grab the event data
			FIPhoneTouchEvent& Event = Events(EventIndex);
			FIntPoint& Loc = Event.Data;
			FVector2D TouchLocation = FVector2D(FLOAT(Loc.X),FLOAT(Loc.Y));
			ViewportClient->InputTouch(this, 0, Event.Handle, Event.Type, TouchLocation, Event.IPhoneTimestamp);
		} // end event loop
	} // end queue loop

	// Handle events that need to happen every frame

	FVector Attitude;
	FVector RotationRate;
	FVector Gravity;
	FVector Accel;

	GIPhoneInputManager.GetMovementData(Attitude, RotationRate, Gravity, Accel);

	// Fixup yaw to match directions
	Attitude.Y = -Attitude.Y;
	RotationRate.Y = -RotationRate.Y;

	// munge the vectors based on the orientation
	EUIOrientation Orientation = (EUIOrientation)IPhoneGetUIOrientation();
	UMobilePlayerInput::ModifyVectorByOrientation(Attitude, Orientation, TRUE);
	UMobilePlayerInput::ModifyVectorByOrientation(RotationRate, Orientation, TRUE);
	UMobilePlayerInput::ModifyVectorByOrientation(Gravity, Orientation, FALSE);
	UMobilePlayerInput::ModifyVectorByOrientation(Accel, Orientation, FALSE);

	ViewportClient->InputMotion(this, 0, Attitude, RotationRate, Gravity, Accel);
}


UBOOL FIPhoneViewport::IsControllerTiltActive( INT ControllerID ) const 
{
	return bIsTiltActive;
}

void FIPhoneViewport::SetControllerTiltActive( INT ControllerID, UBOOL bActive ) 
{
	bIsTiltActive = bActive;
	
	// recalibrate so that the current tilt is now 0,0,0 
	if (bIsTiltActive)
	{
		CalibrateTilt();
	}
}

void FIPhoneViewport::CalibrateTilt() 
{
	GIPhoneInputManager.CalibrateMotion();
}

/** Extracts the version string from info.plist */
void FIPhoneViewport::InitAppVersionString (void)
{
	ANSICHAR VersionStr[128];
	if (IPhoneGetBundleStringValue("EpicAppVersion", VersionStr, 128))
	{
		AppVersionString = FString(VersionStr);
	}
}


UBOOL FIPhoneViewport::IsKeyboardAvailable( INT ControllerID ) const 
{
	// iphone won't have a realtime keyboard
	return FALSE;
}

UBOOL FIPhoneViewport::IsMouseAvailable( INT ControllerID ) const 
{
	return FALSE;
}

INT FIPhoneViewport::GetMouseX()
{
	return 0;
}

INT FIPhoneViewport::GetMouseY()
{
	return 0;
}

void FIPhoneViewport::GetMousePos( FIntPoint& MousePosition )
{
}

void FIPhoneViewport::SetMouse(INT x, INT y)
{
}
