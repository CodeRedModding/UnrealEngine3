/*=============================================================================
	AndroidViewport.cpp: FAndroidViewport code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Core.h"
#include "Engine.h"
#include "EngineGameEngineClasses.h"
#include "AndroidViewport.h"
#include "AndroidInput.h"

#include "EngineAIClasses.h"			// needed only for gameframeworkclasses.h
#include "EngineSequenceClasses.h"		// needed only for gameframeworkclasses.h
#include "EngineUserInterfaceClasses.h"	// needed only for gameframeworkclasses.h
#include "EnginePhysicsClasses.h"		// needed only for gameframeworkclasses.h
#include "GameFrameworkClasses.h"

// Global singleton
FAndroidInputManager GAndroidInputManager;

FLOAT MobileMaxSpeedModifier;

#define MODULE "UE3"

//#define DEBUGVIEWPORT(a) __android_log_print(ANDROID_LOG_DEBUG, MODULE, a)
#define DEBUGVIEWPORT(a)

void FKdViewport::StaticInit()
{
}

//
//	FAndroidViewport::FAndroidViewport
//
FKdViewport::FKdViewport(UKdClient* InClient, FViewportClient* InViewportClient, UINT InSizeX, UINT InSizeY)
: FViewport(InViewportClient)
, Client(InClient)
, bIsTiltActive(FALSE)
, TiltCenterPitch(0)
, TiltCenterYaw(0)
{
	// Creates the viewport window.
	Resize(InSizeX, InSizeY, TRUE);
}

//
//	FAndroidViewport::Destroy
//

void FKdViewport::Destroy()
{
	ViewportClient = NULL;

	// Release the viewport RHI.
	UpdateViewportRHI(TRUE, SizeX, SizeY, TRUE);
}

//
//	FAndroidViewport::Resize
//
void FKdViewport::Resize(UINT NewSizeX, UINT NewSizeY, UBOOL NewFullscreen, INT InPosX, INT InPosY)
{
	SizeX = NewSizeX;
	SizeY = NewSizeY;

	// if we have a size, update the viewport RHI, otherwise destroy it
	if (NewSizeX && NewSizeY)
	{
		UpdateViewportRHI(FALSE, SizeX, SizeY, TRUE);
	}
	else
	{
		UpdateViewportRHI(TRUE, SizeX, SizeY, TRUE);
	}
}

FViewportFrame* FKdViewport::GetViewportFrame()
{
	return this;
}

//
//	FAndroidViewport::KeyState
//
UBOOL FKdViewport::KeyState(FName Key) const
{
	// @todo android keyboard: we may need this for proper UI functionality (see calls to IsCtrlDown, etc)
	return FALSE;
}

//
//	FAndroidViewport::ProcessInput
//
void FKdViewport::ProcessInput(FLOAT DeltaTime)
{
	if( !ViewportClient )
	{
		return;
	}

	TArray<FAndroidTouchEvent> TouchQueuedEvents;
	GAndroidInputManager.GetAllTouchEvents(TouchQueuedEvents);

	TArray<FAndroidKeyEvent> KeyQueuedEvents;
	GAndroidInputManager.GetAllKeyEvents(KeyQueuedEvents);

	// Get a pointer to the input system
	UMobilePlayerInput* Input = NULL;
	if (GEngine && GEngine->GamePlayers.Num() && GEngine->GamePlayers(0) && GEngine->GamePlayers(0)->Actor && GEngine->GamePlayers(0)->Actor->PlayerInput)
	{
		Input = Cast<UMobilePlayerInput>(GEngine->GamePlayers(0)->Actor->PlayerInput);
	}

	// if there isn't one, there's no input we need to do
	if (!Input)
	{
		return;
	}

	for (INT QueueIndex = 0; QueueIndex < KeyQueuedEvents.Num(); QueueIndex++)
	{
		// Grab the event data
		FAndroidKeyEvent& Event = KeyQueuedEvents(QueueIndex);

		static FName StopMatinee( TEXT("MOBILE_StopMatinee") );
		static FName StartMatinee( TEXT("MOBILE_StartMatinee") );
		static bool Stop = true;
		if (Event.KeyDown)
		{
			if (Event.KeyName == KEY_Tilde) 
			{
				ViewportClient->InputKey( this, 0, Stop ? StopMatinee : StartMatinee, IE_Pressed, 1.0f, FALSE );
				ViewportClient->InputKey( this, 0, Stop ? StopMatinee : StartMatinee, IE_Released, 1.0f, FALSE );
				Stop = !Stop;						
			}
			else
			{
				ViewportClient->InputKey( this, 0, Event.KeyName, IE_Pressed, 1.0f, FALSE );
				ViewportClient->InputChar( this, 0, Event.Unichar );
			}
		}
		else
		{
			ViewportClient->InputKey( this, 0, Event.KeyName, IE_Released, 1.0f, FALSE );
		}
	}
	for (INT QueueIndex = 0; QueueIndex < TouchQueuedEvents.Num(); QueueIndex++)
	{
		// Grab the event data
		FAndroidTouchEvent& Event = TouchQueuedEvents(QueueIndex);
		FIntPoint& Loc = Event.Data;
		FVector2D TouchLocation = FVector2D(FLOAT(Loc.X),FLOAT(Loc.Y));
		ViewportClient->InputTouch(this, 0, Event.Handle, Event.Type, TouchLocation, Event.AndroidTimestamp);
		GFullScreenMovie->InputTouch(this, 0, Event.Handle, Event.Type, TouchLocation, Event.AndroidTimestamp);
	}

	// TODO re-enable when tilt input is supported on Android
// 	// Get the current tilt
//	FLOAT TiltYaw, TiltPitch;
// 	GAndroidInputManager.GetTilt(TiltYaw, TiltPitch);
// 	Input->InputTilt(TiltPitch, TiltYaw);
}



UBOOL FKdViewport::IsControllerTiltActive( INT ControllerID ) const 
{
	return bIsTiltActive;
}

void FKdViewport::SetControllerTiltActive( INT ControllerID, UBOOL bActive ) 
{
	bIsTiltActive = bActive;

	// recalibrate so that the currnt tilt is now 0,0,0 
	if (bIsTiltActive)
	{
		CalibrateTilt();
	}
}

void FKdViewport::CalibrateTilt() 
{
// 	GAndroidInputManager.GetTilt(TiltCenterYaw, TiltCenterPitch);
// 	debugf(TEXT("Centered tilt at %f, %f"), TiltCenterYaw, TiltCenterPitch);
}


void FKdViewport::SetOnlyUseControllerTiltInput( INT ControllerID, UBOOL bActive ) 
{
}


/**
* sets whether or not to use the tilt forward and back input controls
**/
void FKdViewport::SetUseTiltForwardAndBack( INT ControllerID, UBOOL bActive )
{
}	


UBOOL FKdViewport::IsKeyboardAvailable( INT ControllerID ) const 
{
	// @todo Android: how can we determine if keyboard exists?
	return FALSE;
}

UBOOL FKdViewport::IsMouseAvailable( INT ControllerID ) const 
{
	// @todo Android: how can we determine if mouse exists?
	return FALSE;
}

INT FKdViewport::GetMouseX()
{
	// @todo Android: return if mouse exists
	return 0;
}

INT FKdViewport::GetMouseY()
{
	// @todo Android: return if mouse exists
	return 0;
}

void FKdViewport::GetMousePos( FIntPoint& MousePosition )
{
	// @todo Android: return if mouse exists
}

void FKdViewport::SetMouse(INT x, INT y)
{
	// @todo Android: set if mouse exists
}

