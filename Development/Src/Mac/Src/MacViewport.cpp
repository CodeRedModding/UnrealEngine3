/*=============================================================================
	MacViewport.cpp: FMacViewport code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "MacDrv.h"
#include "MacObjCWrapper.h"
#include "HIDControllers.h"

void* GTempViewport = 0;	// ugly hack, so Cocoa NSview can get a pointer to FMacViewport
int GToggleFullscreen = 0;
int GAppActiveChanged = 0; // 0 - not changed, 1 - became active, 2 - resigned active
bool GIsUserResizingWindow = false;

//
//	FMacViewport::FMacViewport
//
FMacViewport::FMacViewport(UMacClient* InClient,FViewportClient* InViewportClient,const TCHAR* InName,UINT InSizeX,UINT InSizeY,UBOOL InFullscreen,INT InPosX, INT InPosY)
	:	FViewport(InViewportClient)
	,	PreventCapture(FALSE)
	,	CurrentMousePosX(0)
	,	CurrentMousePosY(0)
{
	Client					= InClient;

	Name					= InName;

	// New MouseCapture/MouseLock API
	SystemMouseCursor		= MC_Arrow;
	bMouseLockRequested		= FALSE;
	bCapturingMouseInput	= FALSE;

	GTempViewport = this;

	// Creates the viewport window.
	Resize(InSizeX,InSizeY,InFullscreen,InPosX,InPosY);

#if !FINAL_RELEASE
	// Used to fake mobile input support
	bFakeMobileTouches = FALSE;

	// Turn on bFakeMobileTouches when starting a game, but never in normal editor viewports
	if( GIsGame )
	{
		GConfig->GetBool(TEXT("GameFramework.MobilePlayerInput"), TEXT("bFakeMobileTouches"), bFakeMobileTouches, GGameIni);
		if( !bFakeMobileTouches )
		{
			bFakeMobileTouches =
				ParseParam( appCmdLine(), TEXT("simmobile") ) ||
				ParseParam( appCmdLine(), TEXT("simmobileinput") );
		}
	}

	bIsFakingMobileTouch = false;
#endif
}

//
//	FMacViewport::~FMacViewport
//
FMacViewport::~FMacViewport()
{
}

//
//	FMacViewport::Destroy
//
void FMacViewport::Destroy()
{
	// Release the viewport RHI.
	UpdateViewportRHI(TRUE,0,0,FALSE);
}

//
//	FMacViewport::Resize
//
void FMacViewport::Resize(UINT NewSizeX,UINT NewSizeY,UBOOL NewFullscreen,INT InPosX, INT InPosY)
{
	if( !Window.MainWindow )
	{
		Window.MainWindow = MacCreateWindow(*GConfig->GetStr(TEXT("URL"), TEXT("GameName"), GEngineIni), FALSE);
		if (GMacOSXVersion < MacOSXVersion_Lion)
		{
			Window.FullscreenWindow = MacCreateWindow(NULL, TRUE);
		}
	}

	// Initialize the viewport's render device.
	if( NewSizeX && NewSizeY )
	{
		UpdateViewportRHI(FALSE,NewSizeX,NewSizeY,NewFullscreen);
	}
	// #19088: Based on certain startup patterns, there can be a case when all viewports are destroyed, which in turn frees up the D3D device (which results in badness).
	// There are plans to fix the initialization code, but until then hack the known case when a viewport is deleted due to being resized to zero width or height.
	// (D3D does not handle the zero width or zero height case)
	else if( NewSizeX && !NewSizeY )
	{
		NewSizeY = 1;
		UpdateViewportRHI(FALSE,NewSizeX,NewSizeY,NewFullscreen);
	}
	else if( !NewSizeX && NewSizeY )
	{
		NewSizeX = 1;
		UpdateViewportRHI(FALSE,NewSizeX,NewSizeY,NewFullscreen);
	}
	// End hack
	else
	{
		UpdateViewportRHI(TRUE,NewSizeX,NewSizeY,NewFullscreen);
	}
}

UBOOL FMacViewport::HasFocus() const
{
	return TRUE;
}

UBOOL FMacViewport::IsCursorVisible( ) const
{
	// unused in the whole engine; returning same thing as superclass
	return TRUE;
}

//
//	FMacViewport::CaptureJoystickInput
//
UBOOL FMacViewport::CaptureJoystickInput(UBOOL Capture)
{
	return Capture;
}

//
//	FMacViewport::KeyState
//
UBOOL FMacViewport::KeyState(FName Key) const
{
	FString KeyName = Key.ToString();
	return FALSE;
}

//
//	FMacViewport::GetMouseX
//
INT FMacViewport::GetMouseX()
{
	return CurrentMousePosX;
}

//
//	FMacViewport::GetMouseY
//
INT FMacViewport::GetMouseY()
{
	return CurrentMousePosY;
}

void FMacViewport::GetMousePos( FIntPoint& MousePosition )
{
	MousePosition.X = CurrentMousePosX;
	MousePosition.Y = CurrentMousePosY;
}

void FMacViewport::SetMouse(INT x, INT y)
{
	// unused in the whole engine
}

//
//	FMacViewport::InvalidateDisplay
//

void FMacViewport::InvalidateDisplay()
{
}

FViewportFrame* FMacViewport::GetViewportFrame()
{
	return this;
}

//
//	FMacViewport::ProcessInput
//
void FMacViewport::ProcessInput( FLOAT DeltaTime )
{
	if( !ViewportClient )
	{
		return;
	}

	// Process joysticks/gamepads
	{
		HIDControllerInfo* ControllerSet = HIDControllerInfo::GetControllerSet();
		ControllerSet->RefreshStates();

		for( int ControllerIndex = 0; ControllerIndex < MAX_CONTROLLERS; ++ControllerIndex )
		{
			FControllerInfo* ControllerInfo = &ControllerSet->Controllers[ControllerIndex];
			if( !ControllerInfo->HIDDevice )
			{
				continue;	// controller not connected
			}

			// Handle all valid axe values
			for( int AxisIndex = 0; AxisIndex < ControllerInfo->Axes.Num(); ++AxisIndex )
			{
				FAxisInfo& Axis = ControllerInfo->Axes( AxisIndex );
				float ValueRange = (float)( Axis.MaxValue - Axis.MinValue );
				float RangeZeroToOne = ( (float)Axis.Value - (float)Axis.MinValue ) / ValueRange;
				FLOAT Delta = Axis.MinRange + ( Axis.MaxRange - Axis.MinRange ) * RangeZeroToOne;
//				debugf( TEXT("Axis %d value (in range from 0 to 1) is %f. Delta is %f."), AxisIndex, RangeZeroToOne, Delta );
				ViewportClient->InputAxis( this, ControllerInfo->ControllerID, Axis.Name, Delta, DeltaTime, TRUE );
			}

			const DOUBLE CurrentTime = appSeconds();

			// Handle all valid button values
			for( int ButtonIndex = 0; ButtonIndex < ControllerInfo->Buttons.Num(); ++ButtonIndex )
			{
				FButtonInfo& Button = ControllerInfo->Buttons( ButtonIndex );

				if( Button.Value != Button.LastValue )
				{
					ViewportClient->InputKey( this, ControllerInfo->ControllerID, Button.Name, Button.Value ? IE_Pressed : IE_Released, 1.f, TRUE );
					if( Button.Value )
					{
						Button.NextRepeatTime = CurrentTime + Client->InitialButtonRepeatDelay;
					}
				}
				else if( Button.Value && ( Button.NextRepeatTime <= CurrentTime ) )
				{
					ViewportClient->InputKey( this, ControllerInfo->ControllerID, Button.Name, IE_Repeat, 1.f, TRUE );
					Button.NextRepeatTime = CurrentTime + Client->ButtonRepeatDelay;
				}

				Button.LastValue = Button.Value;
			}

			if( ViewportClient == NULL )	// those inputs might have caused it to close
			{
				return;
			}
		}
	}

	for( int MessageIndex = 0; MessageIndex < InputEvents.Num(); ++MessageIndex )
	{
		MacEvent Event = InputEvents(MessageIndex);
		switch( Event.EventType )
		{
		case MACEVENT_Key:
			if( ViewportClient )
			{
				bool MouseButton;
				FName* KeyName;
				switch( Event.Data.Key.KeyCode )
				{
				case MOUSEBUTTON_Left:
					KeyName = &KEY_LeftMouseButton;
					MouseButton = true;
					break;
				case MOUSEBUTTON_Right:
					KeyName = &KEY_RightMouseButton;
					MouseButton = true;
					break;
				case MOUSEBUTTON_Middle:
					KeyName = &KEY_MiddleMouseButton;
					MouseButton = true;
					break;
				default:
					KeyName = Client->KeyMapAppleCodeToName.Find( Event.Data.Key.KeyCode );
					break;
				}

				if( KeyName )
				{
					if( !Event.Data.Key.KeyPressed )
					{
						ViewportClient->InputKey( this, 0, *KeyName, IE_Released );
					}
					else
					{
						EInputEvent IE = ( Event.Data.Key.Repeat ? ( MouseButton ? IE_DoubleClick : IE_Repeat ) : IE_Pressed );
						ViewportClient->InputKey( this, 0, *KeyName, IE );
						if( Event.Data.Key.Character != 0xFFFF )
						{
							ViewportClient->InputChar( this, 0, Event.Data.Key.Character );
						}
					}
				}
			}
			break;
			
		case MACEVENT_MousePosDiff:
			if( ViewportClient )
			{
				if( ViewportClient->WantsPollingMouseMovement() )
				{
					INT DeltaX = Event.Data.MousePosDiff.X;
					if( DeltaX )
					{
						ViewportClient->InputAxis( this, 0, KEY_MouseX, DeltaX, DeltaTime );
					}

					INT DeltaY = Event.Data.MousePosDiff.Y;
					if( DeltaY )
					{
						ViewportClient->InputAxis( this, 0, KEY_MouseY, DeltaY, DeltaTime );
					}
				}
			}
			break;

		case MACEVENT_MousePos:
			if( ViewportClient )
			{
				if( bCapturingMouseInput )
				{
					ViewportClient->CapturedMouseMove( this, Event.Data.MousePos.X, Event.Data.MousePos.Y );
				}
				else
				{
					ViewportClient->MouseMove( this, Event.Data.MousePos.X, Event.Data.MousePos.Y );
				}
			}
			CurrentMousePosX = Event.Data.MousePos.X;
			CurrentMousePosY = Event.Data.MousePos.Y;
			break;

		case MACEVENT_MouseWheel:
			{
				float Move = Event.Data.MouseWheel.MoveAmount;
				if( Move > 0.f )
				{
					ViewportClient->InputKey( this, 0, KEY_MouseScrollUp, IE_Pressed );
					ViewportClient->InputKey( this, 0, KEY_MouseScrollUp, IE_Released );
				}
				else
				{
					ViewportClient->InputKey( this, 0, KEY_MouseScrollDown, IE_Pressed );
					ViewportClient->InputKey( this, 0, KEY_MouseScrollDown, IE_Released );
				}
			}
			break;

		case MACEVENT_WindowResize:
			{
				GIsUserResizingWindow = true;
				Resize( Event.Data.WindowSize.Width, Event.Data.WindowSize.Height, IsFullscreen());
				GIsUserResizingWindow = false;
			}
			break;

		default:
			break;
		}
	}
	InputEvents.Empty();

	if (GToggleFullscreen)
	{
		if (GMacOSXVersion < MacOSXVersion_Lion)
		{
			Resize(GetSizeX(), GetSizeY(), !IsFullscreen());
		}
		else
		{
			MacLionToggleFullScreen(Window.MainWindow);
		}
		GToggleFullscreen = 0;
	}

	if (GAppActiveChanged)
	{
		if (IsFullscreen() && GMacOSXVersion < MacOSXVersion_Lion)
		{
			MacShowWindow(Window.MainWindow, GAppActiveChanged == 1);
			MacShowWindow(Window.FullscreenWindow, GAppActiveChanged == 1);
		}
		GAppActiveChanged = 0;
	}
}

void AddMacEventFromNSViewToViewportQueue( void* MacViewport, MacEvent* Event )
{
	((FMacViewport*)MacViewport)->AddMacEventToQueue( Event );
}
