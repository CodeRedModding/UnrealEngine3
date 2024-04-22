/**
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#include "GameFramework.h"
#include "UnIpDrv.h"
#include "../../Engine/Src/ScenePrivate.h"

#if IPHONE
#include "IPhoneObjCWrapper.h"
#endif

IMPLEMENT_CLASS(UMobilePlayerInput);
IMPLEMENT_CLASS(UMobileInputZone);

IMPLEMENT_CLASS(UMobileMenuScene);
IMPLEMENT_CLASS(UMobileMenuObject);
IMPLEMENT_CLASS(UMobileMenuImage);

IMPLEMENT_CLASS(AMobileHUD);

/**
 * Requests any mobile input zones to update their layout (typically after a viewport change such as a resize)
 */
void UpdateMobileInputZoneLayout()
{
	// If there are any mobile input zones, this will alert them to resize themselves (they are not
	// sized every frame, the complex sizing logic is done once at startup, and then again when the
	// screen resizes)
	if (GEngine && GEngine->GamePlayers.Num() && GEngine->GamePlayers(0) && GEngine->GamePlayers(0)->Actor)
	{
		UMobilePlayerInput* MPI = Cast<UMobilePlayerInput>(GEngine->GamePlayers(0)->Actor->PlayerInput);
		if (MPI)
		{
			MPI->NativeInitializeInputZones(FALSE);
		}
	}
}

/**
 * @return the global scale values
 */
FVector2D UMobilePlayerInput::GetGlobalScale()
{
#if IPHONE
	static FLOAT GlobalScaleX = IPhoneGetGlobalUIScaleX();
	static FLOAT GlobalScaleY = IPhoneGetGlobalUIScaleY();
	return FVector2D(GlobalScaleX,GlobalScaleY);
#elif WITH_ES2_RHI
	return FVector2D(GSystemSettings.MobileContentScaleFactor, GSystemSettings.MobileContentScaleFactor);
#else
	return FVector2D(1.0f,1.0f);
#endif
}

void UMobilePlayerInput::NativeInitializeInputSystem()
{
#if !FINAL_RELEASE
	// Used to fake mobile input support
	UBOOL bTemp = FALSE;
	GConfig->GetBool(TEXT("GameFramework.MobilePlayerInput"), TEXT("bFakeMobileTouches"), bTemp, GGameIni);
	bFakeMobileTouches = bTemp;
	if( !bFakeMobileTouches )
	{
		bFakeMobileTouches =
			ParseParam( appCmdLine(), TEXT("simmobile") ) ||
			ParseParam( appCmdLine(), TEXT("simmobileinput") ) ||
			GEmulateMobileInput || 
			GUsingMobileRHI;
	}

	bTemp = FALSE;
	GConfig->GetBool(TEXT("GameFramework.MobilePlayerInput"), TEXT("bCollapseTouchInput"), bTemp, GGameIni);
	bCollapseTouchInput = bTemp;
#endif

	// Find all classes that inherit from UMobileInputZone (including UMobileInputZone.)
	// For each class, find all instance names of those classes.
	// Script needs this in order to load all instances.
	// To work properly any child classes of UMobileInputZone need to be loaded already.
	FMobileInputZoneClassMap ZoneMap(EC_EventParm);
	for( TObjectIterator<UClass> ChildClassIt; ChildClassIt; ++ChildClassIt )
	{
		UClass* PotentialChildClass = *ChildClassIt;
		if( PotentialChildClass->IsChildOf(UMobileInputZone::StaticClass() ) )
		{
			ZoneMap.ClassType = PotentialChildClass;

			TArray<FString> InstanceNames;
			FString ClassName = PotentialChildClass->GetName();
			// get all per object config objects with this class (if any)
			GConfig->GetPerObjectConfigSections(*PotentialChildClass->GetConfigName(), ClassName, InstanceNames);

			// ClassName will be in format of "InstanceName ClassName" - " ClassName" needs to be removed.
			// Add 1 for the space before the class name.	
			INT RemoveRight = ClassName.Len() + 1;

			// remember the class used for these instances
			for (INT InstanceNamesIdx = 0; InstanceNamesIdx < InstanceNames.Num(); InstanceNamesIdx++)
			{
				// get just the instance name (left half)
				FString InstanceName = InstanceNames(InstanceNamesIdx);
				InstanceName = InstanceName.Left(InstanceName.Len() - RemoveRight);

				ZoneMap.Name = InstanceName;
				MobileInputZoneClasses.AddItem( ZoneMap );
			}
		}
	}
}

	

/**
 * Initialize, or reinitialize a zone
 * 
 * @param Zone The Zone to initialize
 * @param ViewportSize Size of the canvas
 * @param bIsFirstInitialize TRUE if this is the first time the zone is initialized (use FALSE when canvas was resized)
 */
void UMobilePlayerInput::NativeInitializeZone(UMobileInputZone* Zone, const FVector2D& ViewportSize, UBOOL bIsFirstInitialize)
{
	FVector2D InteractiveViewportSize = ViewportSize;

	// If the viewport size is uninitialized, initialize it now
	if( InteractiveViewportSize.X == 0 || InteractiveViewportSize.Y == 0 )
	{
		GetInteractiveViewportSize(InteractiveViewportSize);
	}

	// cache the values the fist time
	if (bIsFirstInitialize)
	{
		Zone->InitialX = Zone->X;
		Zone->InitialY = Zone->Y;
		Zone->InitialSizeX = Zone->SizeX;
		Zone->InitialSizeY = Zone->SizeY;
		Zone->InitialActiveSizeX = Zone->ActiveSizeX;
		Zone->InitialActiveSizeY = Zone->ActiveSizeY;
	}
	// restore the values the other times
	else
	{
		Zone->X = Zone->InitialX;
		Zone->Y = Zone->InitialY;
		Zone->SizeX = Zone->InitialSizeX;
		Zone->SizeY = Zone->InitialSizeY;
		Zone->ActiveSizeX = Zone->InitialActiveSizeX;
		Zone->ActiveSizeY = Zone->InitialActiveSizeY;
	}


	// calculate any scale
	FVector2D GlobalScale = GetGlobalScale();
	GlobalScale.X /= Zone->AuthoredGlobalScale;
	GlobalScale.Y /= Zone->AuthoredGlobalScale;

	// If the value is relative it's a percentage so manage that first

	INT ZX = appTrunc((Zone->bRelativeX) ? (InteractiveViewportSize.X * Zone->X) : (Zone->X * GlobalScale.X));
	INT ZY = appTrunc((Zone->bRelativeY) ? (InteractiveViewportSize.Y * Zone->Y) : (Zone->Y * GlobalScale.Y));
	INT ZW = appTrunc((Zone->bRelativeSizeX) ? (InteractiveViewportSize.X * Zone->SizeX) : (Zone->SizeX * GlobalScale.X));
	INT ZH = appTrunc((Zone->bRelativeSizeY) ? (InteractiveViewportSize.Y * Zone->SizeY) : (Zone->SizeY * GlobalScale.Y));

	// We now have the zone positions converted in to actual screen positions.
	// If the zone position is negative, it's right/bottom justified so handle it and store the final values back

	Zone->X = ZX >= 0 ? ZX : ZX + InteractiveViewportSize.X;
	Zone->Y = ZY >= 0 ? ZY : ZY + InteractiveViewportSize.Y;
	Zone->SizeX = ZW >= 0 ? ZW : ZW + InteractiveViewportSize.X;

	if (Zone->bSizeYFromSizeX)
	{
		Zone->SizeY = Zone->SizeX * Zone->SizeY;
	}
	else
	{
		Zone->SizeY = ZH >= 0 ? ZH : ZH + InteractiveViewportSize.Y;
	}


	if (Zone->bCenterX)
	{
		Zone->X = Zone->X - (Zone->SizeX * 0.5f);
	}

	if (Zone->bCenterY)
	{
		Zone->Y = Zone->Y - (Zone->SizeY * 0.5);
	}

	if (Zone->ActiveSizeX == 0)
	{
		Zone->ActiveSizeX = Zone->SizeX;
	}
	else if( Zone->bRelativeSizeX )
	{
		Zone->ActiveSizeX = InteractiveViewportSize.X * Zone->ActiveSizeX;
	}
	else if( Zone->bApplyGlobalScaleToActiveSizes )
	{
		Zone->ActiveSizeX *= GlobalScale.X;
	}

	if (Zone->ActiveSizeY == 0)
	{
		Zone->ActiveSizeY = Zone->SizeY;
	}
	else if( Zone->bRelativeSizeY )
	{
		if (Zone->bActiveSizeYFromX)
		{
			Zone->ActiveSizeY = Zone->ActiveSizeX * Zone->ActiveSizeY;
		}
		else
		{
			Zone->ActiveSizeY = InteractiveViewportSize.Y * Zone->ActiveSizeY;
		}
	}
	else if( Zone->bApplyGlobalScaleToActiveSizes )
	{
		Zone->ActiveSizeY *= GlobalScale.Y;
	}

	// Setup the default Center

	if (Zone->Type == ZoneType_Slider)
	{
		Zone->CurrentCenter.X= INT( Zone->X + (Zone->SizeX * 0.5));
		Zone->CurrentCenter.Y= INT( Zone->Y + (Zone->SizeY * 0.5));
		Zone->CurrentLocation.X = Zone->X;
		Zone->CurrentLocation.Y = Zone->Y;
	}
	else
	{
		Zone->CurrentLocation.X= INT( Zone->X + (Zone->SizeX * 0.5));
		Zone->CurrentLocation.Y= INT( Zone->Y + (Zone->SizeY * 0.5));
		Zone->CurrentCenter = Zone->CurrentLocation;
	}

	// Zones always start out without any transition fades
	Zone->AnimatingFadeOpacity = 1.0f;

	// Keep track of the initial center position in case we need to reset it later
	Zone->InitialCenter = Zone->CurrentCenter;

	// load up any override textures specified by an .ini
	if (Zone->OverrideTexture1 == NULL && Zone->OverrideTexture1Name.Len() > 0)
	{
		Zone->OverrideTexture1 = LoadObject<UTexture2D>(NULL, *Zone->OverrideTexture1Name, NULL, LOAD_None, NULL);
	}
	if (Zone->OverrideTexture2 == NULL && Zone->OverrideTexture2Name.Len() > 0)
	{
		Zone->OverrideTexture2 = LoadObject<UTexture2D>(NULL, *Zone->OverrideTexture2Name, NULL, LOAD_None, NULL);
	}
}

void UMobilePlayerInput::GetInteractiveViewportSize(FVector2D& ViewportSize)
{
	// Defaults to a simple, small size
	ViewportSize.X = 100;
	ViewportSize.Y = 100;
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine)
	{
		// If there are secondary viewport clients, use the first one of those
		// viewports, otherwise, simply use the GameViewport
		checkSlow(GameEngine->SecondaryViewportClients.Num() == GameEngine->SecondaryViewportFrames.Num());
		if (GameEngine->SecondaryViewportFrames.Num() > 0)
		{
			FViewport* SecondaryViewport = GameEngine->SecondaryViewportFrames(0)->GetViewport();
			ViewportSize.X = SecondaryViewport->GetSizeX();
			ViewportSize.Y = SecondaryViewport->GetSizeY();
		}
		else
		{
			UGameViewportClient* GameViewportClient = GEngine->GameViewport;
			if (GameViewportClient != NULL)
			{
				GameViewportClient->GetViewportSize(ViewportSize);
			}
		}
	}
	else if (GEngine->GameViewport != NULL)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
}

/**
 * CalculateZones will pre-process all of the zones and calculate their bounds.
 */
void UMobilePlayerInput::NativeInitializeInputZones(UBOOL bIsFirstInitialize)
{
	// Determine the viewport size to use for layout
	FVector2D ViewportSize;
	GetInteractiveViewportSize(ViewportSize);

	// Resize each zone as needed
	for( INT ZoneIndex = 0; ZoneIndex < MobileInputZones.Num(); ZoneIndex++ )
	{
		UMobileInputZone* Zone = MobileInputZones(ZoneIndex);
		NativeInitializeZone(Zone, MobileViewportSize, bIsFirstInitialize);
	}

	// On subsequent initializations, also resize any open menus
	if( !bIsFirstInitialize )
	{
		for( INT MenuIndex = 0; MenuIndex < MobileMenuStack.Num(); MenuIndex++ )
		{
			MobileMenuStack(MenuIndex)->eventInitMenuScene(this, appTrunc(ViewportSize.X), appTrunc(ViewportSize.Y), FALSE);
		}
	}
}

/**
 * Update the mobile touch zones if the viewport aspect ratio changes
 */
void UMobilePlayerInput::ConditionalUpdateInputZones(INT NewViewportX, INT NewViewportY, INT NewViewportSizeX, INT NewViewportSizeY)
{
	if( MobileViewportOffset.X != NewViewportX
		|| MobileViewportOffset.Y != NewViewportY
		|| MobileViewportSize.X != NewViewportSizeX
		|| MobileViewportSize.Y != NewViewportSizeY )
	{
		// Undo the current zones, making them relative again
		MobileViewportSize.X = 1.f / MobileViewportSize.X;
		MobileViewportSize.Y = 1.f / MobileViewportSize.Y;
		for(INT Idx=0; Idx < MobileInputZones.Num(); Idx++)
		{
			UMobileInputZone* Zone = MobileInputZones(Idx);
			Zone->X *= MobileViewportSize.X;
			Zone->Y *= MobileViewportSize.Y;
			Zone->SizeX *= MobileViewportSize.X;
			Zone->SizeY *= MobileViewportSize.Y;
			Zone->ActiveSizeX *= MobileViewportSize.X;
			Zone->ActiveSizeY *= MobileViewportSize.Y;
		}

		// Move/Resize the input zones with the new sizes
		MobileViewportOffset.X = NewViewportX;
		MobileViewportOffset.Y = NewViewportY;
		MobileViewportSize.X = NewViewportSizeX;
		MobileViewportSize.Y = NewViewportSizeY;
		NativeInitializeInputZones(FALSE);
	}
}

UMobileInputZone* UMobilePlayerInput::HitTest(FVector2D TouchLocation, UINT TouchpadIndex)
{
	if (MobileInputGroups.Num() > 0 && CurrentMobileGroup < MobileInputGroups.Num())
	{
		// Check all zones.
		FMobileInputGroup& Group =  MobileInputGroups(CurrentMobileGroup);
		for(INT i=0;i<Group.AssociatedZones.Num();i++)
		{
			UMobileInputZone* Zone = Group.AssociatedZones(i);
			if (Zone->TouchpadIndex == TouchpadIndex)
			{
				if (Zone->Type == ZoneType_Slider)
				{
					FLOAT X1, Y1, X2, Y2;

					if (Zone->bSliderHasTrack)
					{
						// Click entire track
						X1 = Zone->X;
						Y1 = Zone->Y;
						X2 = X1 + Zone->SizeX;
						Y2 = Y1 + Zone->SizeY;
						if (Zone->SlideType == ZoneSlide_LeftRight)
						{
							X2 += Zone->ActiveSizeX;
						}
						else
						{
							Y2 += Zone->ActiveSizeY;
						}
					}
					else
					{
						// We need to figure out where the knob is.  
						X1 = Zone->SlideType == ZoneSlide_LeftRight ? Zone->CurrentLocation.X  : Zone->X;
						Y1 = Zone->SlideType == ZoneSlide_LeftRight ? Zone->Y : Zone->CurrentLocation.Y;
						X2 = X1 + Zone->ActiveSizeX;
						Y2 = Y1 + Zone->ActiveSizeY;
					}
					if (TouchLocation.X >= X1 && TouchLocation.X < X2 &&
						TouchLocation.Y >= Y1 && TouchLocation.Y < Y2)
					{
						return Zone;
					}
				}
				else
				{
					if (Zone->Border > 0)
					{
						FLOAT BorderX = Zone->Border * Zone->SizeX;
						FLOAT BorderY = Zone->Border * Zone->SizeY;
						if (TouchLocation.X >= (Zone->X - BorderX) && TouchLocation.X < Zone->X + Zone->SizeX + BorderX &&
							TouchLocation.Y >= (Zone->Y - BorderY) && TouchLocation.Y < Zone->Y + Zone->SizeY + BorderY)
						{
							return Zone;
						}
					}
					else
					{
						if (TouchLocation.X >= Zone->X && TouchLocation.X < Zone->X + Zone->SizeX &&
							TouchLocation.Y >= Zone->Y && TouchLocation.Y < Zone->Y + Zone->SizeY)
						{
							return Zone;
						}
					}
				}
			}
		}
	}

	return NULL;
}

/**
 * Process an input key event received from the viewport.
 *
 * @param	Viewport		the viewport the input event was received from
 * @param	ControllerId	gamepad/controller that generated this input event
 * @param	Key				the name of the key which an event occured for (KEY_Up, KEY_Down, etc.)
 * @param	EventType		the type of event which occured (pressed, released, etc.)
 * @param	AmountDepressed	(analog keys only) the depression percent.
 * @param	bGamepad - input came from gamepad (ie xbox controller)
 *
 * @return	TRUE to consume the key event, FALSE to pass it on.
 */
UBOOL UMobilePlayerInput::InputKey(INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed,UBOOL bGamepad)
{
	// we aren't inactive if we get a key event
	MobileInactiveTime = 0; 

	return Super::InputKey(ControllerId, Key, Event, AmountDepressed, bGamepad);
}

/**
	* Process an input axis (joystick, thumbstick, or mouse) event received from the viewport.
	*
	* @param	Viewport		the viewport the input event was received from
	* @param	ControllerId	the controller that generated this input axis event
	* @param	Key				the name of the axis that moved  (KEY_MouseX, KEY_XboxTypeS_LeftX, etc.)
	* @param	Delta			the movement delta for the axis
	* @param	DeltaTime		the time (in seconds) since the last axis update.
	*
	* @return	TRUE to consume the axis event, FALSE to pass it on.
	*/
UBOOL UMobilePlayerInput::InputAxis(INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad)
{
	// we aren't inactive if we get an axis event
	MobileInactiveTime = 0;

	return Super::InputAxis(ControllerId, Key, Delta, DeltaTime, bGamepad);
}

/**
 * Handle a touch event coming from the device. 
 *
 * NOTE: no processing of the touch happens here.  This just tracks the touch in the Touches stack.  Processing 
 * happens each tick
 *
 * @param Handle			the id of the touch
 * @param EventType			What type of event is this
 * @param TouchLocation		Where the touch occurred
 * @param DeviceTimestamp	Input event timestamp from the device
 * @param TouchpadIndex		The index of the touchpad this touch came from
 */
UBOOL UMobilePlayerInput::InputTouch(INT ControllerId, UINT Handle, ETouchType Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp, UBOOL TouchpadIndex)
{
	// we aren't inactive if we get a touch event
	MobileInactiveTime = 0;

	// let the parent class go, and if it returns TRUE, then we need to not process anything in this function
	if (Super::InputTouch(ControllerId, Handle, Type, TouchLocation, DeviceTimestamp, TouchpadIndex))
	{
		return TRUE;
	}

	// @todo: This is probably broken for scenarios where you initiate a touch on a slider bar and then drag over a touch zone!
	//        Both menu elements and touch zones should be 1st class citizens when it comes to owning a touch (should behave just like mouse capture!)
	//        Unfortunately that likely requires more work than we have time for at the moment.

	// Adjust the touch location to fit the current viewport which can change sizes (e.g. cinematic mode)
	if (!bAbsoluteTouchLocations)
	{
		TouchLocation -= MobileViewportOffset;
	}

    //debugf( TEXT( "InputTouch() - Handle:%d, Type:%d, TouchLoc:%0.2f, %0.2f, Time:%0.4f" ), Handle, Type, TouchLocation.X, TouchLocation.Y, DeviceTimestamp );

	FTouchDataEvent NewEvent;
	NewEvent.DeviceTime = DeviceTimestamp;
	NewEvent.TouchpadIndex = TouchpadIndex;
	NewEvent.Location = TouchLocation;
	NewEvent.EventType = Type;

	// @DEBUG - Remove if you wish to see all touch events
	//eventAddTouchDebug(NewEvent);

	// Only one piece of UI (e.g. a touch zone or a menu button) should get a change to handle this touch event.
	UBOOL bTouchHandled = FALSE;

	// HACK: preview touch.
	if (NewEvent.EventType == Touch_Began && DELEGATE_IS_SET(OnPreviewTouch))
	{
		bTouchHandled = delegateOnPreviewTouch(TouchLocation.X, TouchLocation.Y, TouchpadIndex);
		//debugf(TEXT("   - Delegate OnPreviewTouch returned %i"),bTouchHandled);
	}

	if (!bTouchHandled)
	{
		if (NewEvent.EventType == Touch_Began)	// It's a new touch (i.e. finger down) so we create one
		{
			// Give the menus a chance to process the event
			bTouchHandled = ProcessMenuInput(Handle, TouchpadIndex, (ETouchType)NewEvent.EventType, TouchLocation, DeviceTimestamp);
			//debugf(TEXT("  - Processing a Touch, Menus returned %i"),bTouchHandled);

			if ( !bTouchHandled )
			{
				if ( DELEGATE_IS_SET(OnTouchNotHandledInMenu) )
				{
					delegateOnTouchNotHandledInMenu();
					//debugf(TEXT("    - delegate OnTouchNotHandledInMenu returned"));
				}

				// Check to make sure we aren't already processing a touch with this supposedly-unique touch handle
				for (INT i=0;i<ARRAY_COUNT(Touches);i++)
				{
					FTouchData& Touch = Touches[i];
					if( Touch.bInUse && Touch.Handle == Handle )
					{
						// debugf(TEXT("WARNING: Found duplicate touch handle %i (array entry %i)"), Handle, i );

						// For some reason we received a touch down event with the same handle as a currently
						// active touch (a touch that has not yet received a 'touch ended/canceled' event.)  This
						// really should never happen, but just in case we want to avoid adding a new touch to
						// our list with the same handle as an existing entry, as otherwise the newest touch
						// may never get the 'touch ended/canceled' event because we'll send that to the 
						// original touch's zone!

						// NOTE: We don't bother adding this 'touched' event to the touch's event list as far as
						//		it knows it's already been touched and is attached to an active zone

						// The zone will handle this touch
						bTouchHandled = TRUE;
					}
				}

				if( !bTouchHandled )
				{
					// This is a new touch and the menus did not handle it. 
					// Find a spot in the list for it.
					for (INT i=0;i<ARRAY_COUNT(Touches);i++)
					{
						// Have we found a free spot for the touch?
						FTouchData& Touch = Touches[i];
						if( !Touch.bInUse )
						{
							//debugf(TEXT("    - Added Touch to Index %i"),i);
							appMemzero( &Touch, sizeof( FTouchData ) );
							Touch.Handle = Handle;
							Touch.TouchpadIndex = TouchpadIndex;
							Touch.bInUse = true;
							Touch.Events.AddItem( NewEvent );
						
							// Make sure we track this input event so that we
							// know the device is still active
							Touch.LastActiveTime = appSeconds();

							// The zone will handle this touch
							bTouchHandled = TRUE;

							if (DELEGATE_IS_SET(OnInputTouch))
							{
								delegateOnInputTouch(i,Type,TouchLocation,FLOAT(DeviceTimestamp), TouchpadIndex);
							}

							// Pass along the touch to any handlers that might be listening
							for (INT j=0;j<MobileRawInputSeqEventHandlers.Num();j++)
							{
								INT TouchIndex = MobileRawInputSeqEventHandlers(j)->TouchIndex;
								if (TouchIndex == -1 || TouchIndex == i)
								{
									MobileRawInputSeqEventHandlers(j)->InputTouch(Cast<APlayerController>(GetOuter()), i, TouchpadIndex, Type, TouchLocation, DeviceTimestamp);
								}
							}


							break;
						}
					}

					// If we get here, we don't have room for the touch so discard it
				}
			}
		}
		else // Handle all other events (there must be an existing touch associated with them)
		{

			//debugf(TEXT("  - Processing a not a Touch"));
			// Find this touch in the tacking array and process it
			for (INT i=0;i<ARRAY_COUNT(Touches);i++)
			{
				FTouchData& Touch = Touches[i];
				if (Touch.bInUse && Touch.Handle == Handle && Touch.TouchpadIndex == TouchpadIndex)
				{
					// This touch is owned (captured) by a zone
					bTouchHandled = TRUE;
					//debugf(TEXT("    - Found it's location %i"),i);
					Touch.Events.AddItem( NewEvent );

					// Make sure we track this input event so that we
					// know the device is still active
					Touch.LastActiveTime = appSeconds();

					if (DELEGATE_IS_SET(OnInputTouch))
					{
						delegateOnInputTouch(i,Type,TouchLocation,FLOAT(DeviceTimestamp), TouchpadIndex);
					}

					// Pass along the touch to any handlers that might be listening
					for (INT j=0;j<MobileRawInputSeqEventHandlers.Num();j++)
					{
						if (MobileRawInputSeqEventHandlers(j) != NULL)
						{
							INT TouchIndex = MobileRawInputSeqEventHandlers(j)->TouchIndex;
							if (TouchIndex == -1 || TouchIndex == i)
							{
								MobileRawInputSeqEventHandlers(j)->InputTouch(Cast<APlayerController>(GetOuter()), i, TouchpadIndex, Type, TouchLocation, DeviceTimestamp);
							}
						}
					}

					break;
				}
			}

			if ( !bTouchHandled )
			{
				// The zones are not handling this touch, so let the menus process the event
				bTouchHandled = ProcessMenuInput(Handle, TouchpadIndex, (ETouchType)NewEvent.EventType, TouchLocation, DeviceTimestamp);
				//debugf(TEXT("  - ProcessMenuInput returned %i"),bTouchHandled);
			}
		}

		// @todo: Usually this is where you would return info about this event so that it could be dealt with in case it is unhandled.
		// return bTouchHandled;
	}

	return FALSE;
}

class FCameraTiltValue
{

public:

	/** Constructor */
	FCameraTiltValue()
		: Value( 0.0f )
	{
		for( INT CurHistIndex = 0; CurHistIndex < ValuesToAverage; ++CurHistIndex )
		{
			DeltaHistory[ CurHistIndex ] = 0.0f;
		}
	}

	/** Applies a new delta to this value */
	void Update( FLOAT NewDelta )
	{
		// Update history
		{
			const INT LastFrameIndex = ValuesToAverage - 1;
			for( INT CurHistIndex = 0; CurHistIndex < LastFrameIndex; ++CurHistIndex )
			{
				DeltaHistory[ CurHistIndex ] = DeltaHistory[ CurHistIndex + 1 ];
			}
			DeltaHistory[ LastFrameIndex ] = NewDelta;
		}


		// Compute smoothed delta
		FLOAT TotalDelta = 0.0f;
		for( INT CurHistIndex = 0; CurHistIndex < ValuesToAverage; ++CurHistIndex )
		{
			TotalDelta += DeltaHistory[ CurHistIndex ];
		}
		const FLOAT SmoothDelta = TotalDelta / (FLOAT)ValuesToAverage;

		
		// Update value
		Value += SmoothDelta;


		// How much to dampen the value every time we update (1.0 = no dampen, 0.0 = max)
		const FLOAT DampenScalar = 0.95f;

		// Apply dampening
		Value *= DampenScalar;
	}


	/** Returns the current value */
	FLOAT GetValue() const
	{
		return Value;
	}


private:

	/** How many values to average (for smoothing) */
	static const INT ValuesToAverage = 5;

	/** History of the previous values */
	FLOAT DeltaHistory[ ValuesToAverage ];

	/** Current value */
	FLOAT Value;			

};


/**
 * Perform the actual processing on touches
 *
 * @param DeltaTime		Much time has elapsed since the last processing
 */
void UMobilePlayerInput::Tick(FLOAT DeltaTime)
{
	Super::Tick(DeltaTime);
}

void UMobilePlayerInput::CancelMobileInput()
{
	for (INT TouchIndex=0;TouchIndex<ARRAY_COUNT(Touches);TouchIndex++)
	{
		FTouchData& Touch = Touches[TouchIndex];
		InputTouch( 0, Touch.Handle, Touch_Cancelled, Touch.Location, Touch.MoveEventDeviceTime, Touch.TouchpadIndex );		
	}
}

/**
 * Process Mobile Input, handles mobile user input independent of game update
 */
void UMobilePlayerInput::ProcessMobileInput(float DeltaTime)
{
	// Scan though the input zones and see if we can find one that needs this touch.
	UBOOL bIsActive = false;
	for (INT TouchIndex=0;TouchIndex<ARRAY_COUNT(Touches);TouchIndex++)
	{
		FTouchData& Touch = Touches[TouchIndex];
		if (Touch.bInUse )
		{
			bIsActive = true;
			UBOOL bTouchDurationUpdated = FALSE;

			if( Touch.Events.Num() > 0 )
			{
				// Process all of the outstanding events for this touch
				for( INT CurEventIndex = 0; CurEventIndex < Touch.Events.Num(); ++CurEventIndex )
				{
					FTouchDataEvent& CurEvent = Touch.Events( CurEventIndex );

					// Test if all remaining events are the same
					if (bCollapseTouchInput)
					{
						INT TestEventIndex;
						UBOOL bCollapsed;

						do 
						{
							TestEventIndex = CurEventIndex + 1;
							bCollapsed = FALSE;

							if (TestEventIndex < Touch.Events.Num())
							{
								if (Touch.Events(TestEventIndex).EventType == CurEvent.EventType)
								{
									bCollapsed = TRUE;
									CurEventIndex++;
									CurEvent = Touch.Events(CurEventIndex);
								}

							}
						} while (bCollapsed);
					}

					// Just ignore 'stationary' events
					if( CurEvent.EventType != Touch_Stationary )
					{
						// Set the touch's state
						Touch.State = CurEvent.EventType;

						FLOAT LastTouchDuration = 0.0f;
						if( CurEvent.EventType == Touch_Began )
						{
							Touch.Location = CurEvent.Location;
							Touch.TotalMoveDistance = 0.0f;
							Touch.InitialDeviceTime = Touch.MoveEventDeviceTime = CurEvent.DeviceTime;
							Touch.MoveDeltaTime = 0.0f;
							Touch.TouchDuration = 0.0f;
							bTouchDurationUpdated = TRUE;

							UMobileInputZone* Zone = HitTest( CurEvent.Location, Touch.TouchpadIndex );

							// Look to see if that zone is already active
							if (Zone && (Zone->State == ZoneState_Inactive || Zone->State == ZoneState_Deactivating))
							{
								// Nope, assign this touch to the zone
								Touch.Zone = Zone;
							}
						}
						else	// Move or Untouch events
						{
							// Update the precise duration of the touch
							LastTouchDuration = Touch.TouchDuration;
							Touch.TouchDuration = (FLOAT)( CurEvent.DeviceTime - Touch.InitialDeviceTime );
							bTouchDurationUpdated = TRUE;

							// Keep track of time since the last movement event
							Touch.MoveDeltaTime = (FLOAT)( CurEvent.DeviceTime - Touch.MoveEventDeviceTime );

							// If this is a move event then also update location and timestamps
							const UBOOL bHasMoved = Touch.Location != CurEvent.Location;
							if( bHasMoved && CurEvent.EventType == Touch_Moved )
							{
								// Update the location
								Touch.TotalMoveDistance += (Touch.Location - CurEvent.Location).Size();
								Touch.Location = CurEvent.Location;

								Touch.MoveEventDeviceTime = CurEvent.DeviceTime;
							}
						}

						// If we have an associated touch zone, pass this along
						if (Touch.Zone != NULL)
						{
							// Only trackball zones get intra-frame movement events, all other zones get events fired
							// every single frame
							if( Touch.State != Touch_Moved || Touch.Zone->Type == ZoneType_Trackball )
							{
								const FLOAT EventDeltaTime = ( Touch.TouchDuration - LastTouchDuration );
								Touch.Zone->ProcessTouch( EventDeltaTime, Touch.Handle, (ETouchType)Touch.State, Touch.Location, Touch.TotalMoveDistance, Touch.TouchDuration, Touch.MoveDeltaTime );
							}
						}

						// Clear if we are done
						if ((Touch.State == Touch_Ended) || (Touch.State == Touch_Cancelled))
						{
							Touch.bInUse = false;
							Touch.Handle = 0;
							Touch.Zone = NULL;

							// Stop processing events
							break;
						}
					}
					else
					{
						// Still need to Update the precise duration of the touch
						Touch.TouchDuration = (FLOAT)( CurEvent.DeviceTime - Touch.InitialDeviceTime );
						bTouchDurationUpdated = TRUE;

						// Keep track of time since the last movement event
						Touch.MoveDeltaTime = (FLOAT)( CurEvent.DeviceTime - Touch.MoveEventDeviceTime );
					}
				}

				// Clear the event queue now that we've processed everything
				Touch.Events.Reset();
			}

			// If we are managing a touch event, the next event will be an update.
			if (Touch.State == Touch_Began)
			{
				Touch.State = Touch_Moved;
			}

			// Make sure that all zone types except track balls get regular updates even when no movement events
			// were received.  Note that these zones always get events every frame, but no intra-frame movement events.
			if (Touch.Zone != NULL && Touch.Zone->Type != ZoneType_Trackball)
			{
				if (!bTouchDurationUpdated)
				{
					Touch.TouchDuration += DeltaTime;
				}
				const FLOAT FrameMoveDeltaTime = 0.0f;
				Touch.Zone->ProcessTouch( DeltaTime, Touch.Handle, (ETouchType)Touch.State, Touch.Location, Touch.TotalMoveDistance, Touch.TouchDuration, FrameMoveDeltaTime );
			}
		}
	}

	// Track how active the touch system is
	MobileInactiveTime += DeltaTime;

	// For each zone, apply any remaining escape velocity
	if( MobileInputZones.Num() > 0 )
	{
		// Tick all of the zones
		for (INT ZoneIndex=0;ZoneIndex<MobileInputGroups(CurrentMobileGroup).AssociatedZones.Num();ZoneIndex++)
		{
			MobileInputGroups(CurrentMobileGroup).AssociatedZones(ZoneIndex)->TickZone(DeltaTime);
		}

		for( INT CurZoneIndex = 0; CurZoneIndex < MobileInputZones.Num(); ++CurZoneIndex )
		{
			UMobileInputZone* Zone = MobileInputZones( CurZoneIndex );

			// Only zones that currently aren't being interacted with will apply escape velocity
			if( Zone->State == ZoneState_Deactivating || Zone->State == ZoneState_Inactive )
			{
				Zone->ApplyEscapeVelocity( DeltaTime );
			}
		}
	}

	// let the older kismet nodes in on the action
	UpdateListeners();
}



/**
 * When input comes in to the player input, the first thing we need to do is process it for
 * the menus.
 *
 * @param TouchHandle       A unique id for the touch
 * @param TouchpadIndex		The index of the touchpad this touch came from
 * @param EventType         What type of event is this
 * @param TouchLocation     Where the touch occurred
 *
 * @returns true if the menu system swallowed the input
 */
UBOOL UMobilePlayerInput::ProcessMenuInput(UINT TouchHandle, UINT TouchpadIndex, ETouchType EventType, FVector2D TouchLocation, DOUBLE DeviceTimestamp)
{
	// If we are currently interacting with an object, then send any events to it
	if (this->InteractiveObject != NULL)
	{
		// Find the object that we are currently over.
		UMobileMenuObject* HitObj = this->InteractiveObject->OwnerScene->HitTest(TouchLocation.X, TouchLocation.Y);

		// Can change as user moves 
		this->InteractiveObject->bIsTouched = (HitObj == this->InteractiveObject);

		// Calculate time since last input update.
		FLOAT DeltaTime = (FLOAT)(DeviceTimestamp - this->InteractiveObjectLastTime);
		this->InteractiveObjectLastTime = DeviceTimestamp;

		// First see if component wants to handle event...
		// NOTE: HitObj might be NULL or even a different object.  This allows for dragging and dropping onto another object.
		if (!this->InteractiveObject->eventOnTouch(EventType, TouchLocation.X, TouchLocation.Y, HitObj, DeltaTime))
		{
			// Now tell scene about interactive object...
			this->InteractiveObject->OwnerScene->eventOnTouch(this->InteractiveObject, EventType, TouchLocation.X, TouchLocation.Y);
		}
		
		// We are done interactive with this object.
		if ((EventType == Touch_Ended) || (EventType == Touch_Cancelled))
		{
			this->InteractiveObject->bIsTouched = FALSE;
			this->InteractiveObject = NULL;
		}
		return true;
	}
	else
	{
		for (INT idx=0;idx<MobileMenuStack.Num();idx++)
		{
			UMobileMenuScene* MenuScene = MobileMenuStack(idx);
			if (MenuScene != NULL)	
			{
				UMobileMenuObject* HitObj = MenuScene->HitTest(TouchLocation.X, TouchLocation.Y);
				if (HitObj != NULL)
				{
					// Very rare that the first event an object gets is a release, but perhaps possible if input is moving on release.
					if (EventType == Touch_Ended)
					{
						APlayerController* Pc = Cast<APlayerController>(GetOuter());
						if (Pc!=NULL && MenuScene->UIUnTouchSound != NULL)
						{
							Pc->PlaySound(MenuScene->UIUnTouchSound);
						}
					}
					else 
					{
						if (EventType == Touch_Began)
						{
							APlayerController* Pc = Cast<APlayerController>(GetOuter());
							if (Pc!=NULL && MenuScene->UITouchSound != NULL)
							{
								Pc->PlaySound(MenuScene->UITouchSound);
							}
						}

						HitObj->bIsTouched = true;

						// When we have an UnTouch event, this interactive object needs to receive it
						this->InteractiveObject = HitObj;
						this->InteractiveObjectLastTime = DeviceTimestamp;
					}

					// Let object know that input is over it...
					// NOTE: If user starts input not on any object, then moves over an object, 
					// the object will not receive a Touch_Began.  This would allow for method of
					// selecting an object like Fruit Ninja - user is required to swipe through a button
					// instead of pressing it.
					if (!HitObj->eventOnTouch(EventType, TouchLocation.X, TouchLocation.Y, HitObj, 0.0f))
					{
						// Now tell scene about interactive object...
						HitObj->OwnerScene->eventOnTouch(HitObj, EventType, TouchLocation.X, TouchLocation.Y);
					}

					return true;
				}


				UBOOL bIsInside = false;
				if (TouchLocation.X >= MenuScene->Left && TouchLocation.X < MenuScene->Left + MenuScene->Width &&
					TouchLocation.Y >= MenuScene->Top && TouchLocation.Y < MenuScene->Top + MenuScene->Height)
				{
					bIsInside = true;
				}

				// Let OnSceneTouch decide if we eat the event or not.
				if (MenuScene->eventOnSceneTouch(EventType, TouchLocation.X,TouchLocation.Y, bIsInside))
				{
					return true;
				}
			}
		}
	}

	// We were not handled.. let the scene have a crack at it.
	return false;
}

/**
 * Allows the game to send a InputKey event though the view port.
 *
 * @param Key				The new of the key we are sending
 * @param Event				The Type of event
 * @param AmountDepressed	The strength of the event
 */

void UMobilePlayerInput::SendInputKey(FName Key,BYTE Event,FLOAT AmountDepressed)
{
	APlayerController* PC = Cast<APlayerController>(GetOuter());
	if (PC != NULL && (!PC->bCinematicMode || bAllowTouchesInCinematic) && PC->Player != NULL)
	{
		if (!bDisableTouchInput)
		{
			ULocalPlayer* LP = Cast<ULocalPlayer>(PC->Player);
			if (LP != NULL && LP->ViewportClient != NULL && LP->ViewportClient->Viewport != NULL)
			{
				//debugf(TEXT("Sending Input Key: %s %i %f"),*Key.ToString(),Event,AmountDepressed);
				LP->ViewportClient->InputKey(LP->ViewportClient->Viewport, 0, Key, EInputEvent(Event), AmountDepressed, false);
			}
		}
	}
}

/**
 * Allows the game to send an InputAxis event through the viewport                                                                     
 *
 * @param Key				the key we are sending
 * @param	Delta			the movement delta for the axis
 * @param	DeltaTime		the time (in seconds) since the last axis update.
 */
void UMobilePlayerInput::SendInputAxis(FName Key, FLOAT Delta, FLOAT DeltaTime)
{
	APlayerController* PC = Cast<APlayerController>(GetOuter());
	if (PC != NULL && (!PC->bCinematicMode || bAllowTouchesInCinematic) && PC->Player != NULL)
	{
		if (!bDisableTouchInput)
		{
			ULocalPlayer* LP = Cast<ULocalPlayer>(PC->Player);
			if (LP != NULL && LP->ViewportClient != NULL && LP->ViewportClient->Viewport != NULL)
			{
				//debugf(TEXT("Sending Input Axis: %s %f %f"),*Key.ToString(),Delta,DeltaTime);
				LP->ViewportClient->InputAxis(LP->ViewportClient->Viewport,0, Key, Delta, DeltaTime,false);
			}
		}
	}
}

/**
 * This function will iterate over the MobileSeqEventHandles array and cause them to be updated.  
 * It gets called once per frame.
 */
void UMobilePlayerInput::UpdateListeners()
{
	if (MobileSeqEventHandlers.Num() > 0)
	{
		APlayerController * PC = Cast<APlayerController>(GetOuter());
		for(INT i=0;i<MobileSeqEventHandlers.Num();i++)
		{
			if (MobileSeqEventHandlers(i) != NULL)
			{
				MobileSeqEventHandlers(i)->Update(PC,this);
			}
		}
	}
}



/**
 * Computes average location and movement time for the zone's active touch 
 *
 * @param	InTimeToAverageOver			How long a duration to average over (max)
 * @param	OutSmoothedLocation			(Out) Average location
 * @param	OutSmoothedMoveDeltaTime	(Out) Average movement delta time
 */
void UMobileInputZone::ComputeSmoothedMovement( const FLOAT InTimeToAverageOver, FVector2D& OutSmoothedLocation, FLOAT& OutSmoothedMoveDeltaTime ) const
{
	OutSmoothedLocation = CurrentLocation;
	OutSmoothedMoveDeltaTime = 0.0f;

	// Compute smoothed delta
	FVector2D TotalLocation( 0.0f, 0.0f );
	FLOAT TotalMoveDeltaTime = 0.0f;

	INT UsableHistorySamples = 0;

	for( INT CurHistIndex = 0; CurHistIndex < PreviousLocationCount; ++CurHistIndex )
	{
		// The most recent entries are stored at the end of the array
		const INT ReversedHistIndex = ( ARRAY_COUNT( PreviousLocations ) - 1 ) - CurHistIndex;

		// Maximum age of a smoothing sample that we will include in our calculation.  This is used
		// to reduce lag caused by smoothing when input deltas are spaced far apart in time.  Set this value
		// lower to reduce the amount of smoothing when time deltas are large.
		if( CurHistIndex > 0 && TotalMoveDeltaTime > InTimeToAverageOver )
		{
			break;
		}

		TotalLocation += PreviousLocations[ ReversedHistIndex ];
		TotalMoveDeltaTime += PreviousMoveDeltaTimes[ ReversedHistIndex ];
		++UsableHistorySamples;
	}

	if( UsableHistorySamples > 0 )
	{
		OutSmoothedLocation = TotalLocation / (FLOAT)UsableHistorySamples;
		OutSmoothedMoveDeltaTime = TotalMoveDeltaTime / (FLOAT)UsableHistorySamples;
	}
}



/**
 * Processes a touch event
 *
 * @param DeltaTime		Much time has elapsed since the last processing
 * @param Handle		The unique ID of this touch
 * @param EventType		The type of event that occurred
 * @param TouchLocation	Where on the device has the touch occurred.
 * @param TouchTotalMoveDistance The total distance the use has dragged this touch
 * @param TouchDuration	How long since the touch started
 * @param MoveDeltaTime	Time delta between the movement events the last time this touch moved
 */
void UMobileInputZone::ProcessTouch(FLOAT DeltaTime, UINT Handle, ETouchType EventType, FVector2D TouchLocation, FLOAT TouchTotalMoveDistance, FLOAT TouchDuration, FLOAT MoveDeltaTime)
{
	FLOAT MovePercent[2] = { 0.0f, 0.0f };

	// Let script manage it if it wants
	if (DELEGATE_IS_SET(OnProcessInputDelegate))
	{
		if ( delegateOnProcessInputDelegate(this, DeltaTime, Handle, EventType, TouchLocation))
		{
			return;	// Swallow it
		}
	}

	FLOAT RealTime = GWorld->GetRealTimeSeconds();

	UBOOL bTrackTap = DELEGATE_IS_SET(OnTapDelegate) || TapInputKey != NAME_None;
	UBOOL bTrackDoubleTap = DELEGATE_IS_SET(OnDoubleTapDelegate) || DoubleTapInputKey != NAME_None;

	if (EventType == Touch_Began)
	{

		LastWentActiveTime = GWorld->GetRealTimeSeconds();

		ActivateZone();
		UBOOL bIsDoubleTap = (RealTime - LastTouchTime) < InputOwner->MobileDoubleTapTime;	

		if (bIsDoubleTap && bTrackDoubleTap)
		{
			if (!DELEGATE_IS_SET(OnDoubleTapDelegate) || !delegateOnDoubleTapDelegate(this, EventType, TouchLocation))
			{
				InputOwner->SendInputKey(DoubleTapInputKey, IE_Pressed, 1.0f);

				if (bQuickDoubleTap)
				{
					InputOwner->SendInputKey(DoubleTapInputKey, IE_Released, 1.0f);
				}
				else
				{
					bIsDoubleTapAndHold = true;
				}
			}
		}

		LastTouchTime = RealTime;

		// Zero out history for this touch
		PreviousLocationCount = 0;
		for( INT CurHistoryIndex = 0; CurHistoryIndex < ARRAY_COUNT( PreviousLocations ); ++CurHistoryIndex )
		{
			PreviousLocations[ CurHistoryIndex ].Set( 0.0f, 0.0f );
			PreviousMoveDeltaTimes[ CurHistoryIndex ] = 0.0f;
		}

		if (Type == ZoneType_Slider)
		{
			if (bSliderHasTrack)
			{
				if (SlideType == ZoneSlide_LeftRight)
				{
					CurrentCenter.X = ActiveSizeX * 0.5f;
					CurrentLocation.X = Clamp<FLOAT>(TouchLocation.X - CurrentCenter.X, X, X+SizeX);
					CurrentLocation.Y = Y;
				}
				else
				{
					CurrentCenter.Y = ActiveSizeY * 0.5f;
					CurrentLocation.X = X;
					CurrentLocation.Y = Clamp<FLOAT>(TouchLocation.Y - CurrentCenter.Y, Y, Y+SizeY);
				}
			}
			else
			{
				CurrentCenter = TouchLocation - CurrentLocation;
			}
		}
		else
		{
			InitialLocation = TouchLocation;

			// for trackball zones we use where the initial touch is as the center, 
			// for other zones (except Slider), we use the center of the zone
			if (Type == ZoneType_Trackball || bCenterOnEvent)
			{
				CurrentCenter = TouchLocation;
			}
			else 
			{
				CurrentCenter.X = X + SizeX / 2;
				CurrentCenter.Y = Y + SizeY / 2;

				if( Type == ZoneType_Joystick )
				{
					InitialLocation = CurrentCenter;
				}
			}

			CurrentLocation = InitialLocation;

		}

		// If we are a button, then we need to send the initial touch event
		if (Type == ZoneType_Button) 
		{
			// send the initial pressed event
			InputOwner->SendInputKey(InputKey, IE_Pressed, 1.0f);
		}

		UpdateListeners();

	}
	else if ((EventType == Touch_Ended) || // Handle Untouch Events
			 (EventType == Touch_Cancelled))
	{
		DeactivateZone();

		TotalActiveTime += GWorld->GetRealTimeSeconds() - LastWentActiveTime;

		UBOOL bCancelled = (EventType == Touch_Cancelled);

		if (!bCancelled)
		{
			if (Type == ZoneType_Button)
			{
				InputOwner->SendInputKey(InputKey, IE_Released, 1.0f);							

			}
			else if (Type == ZoneType_Slider)
			{
				InitialLocation = CurrentLocation;
			}
			else
			{
				// Store off the current location
				CurrentLocation = TouchLocation;
			}

			// For untouch events, MoveDeltaTime contains the time since touch last moved (e.g. how long the user
			// has their finger in the same place before lifting it from the device.)  If the user didn't move
			// the touch very recently, then this was not a 'swipe' motion so we'll zero out the escape velocity.
			if( Type == ZoneType_Trackball )
			{
				// @todo: Make configurable?
				const FLOAT MaxEscapeVelocityTime = 0.075f;
				const FLOAT MinDistanceForEscapeVelocity = 12.0f;

				FLOAT FurthestMovedRecently = 0.0f;
				if( MoveDeltaTime < MaxEscapeVelocityTime )
				{
					FLOAT TotalMoveDeltaTime = 0.0f;
					for( INT CurHistIndex = 0; CurHistIndex < PreviousLocationCount; ++CurHistIndex )
					{
						// The most recent entries are stored at the end of the array
						const INT ReversedHistIndex = ( ARRAY_COUNT( PreviousLocations ) - 1 ) - CurHistIndex;

						TotalMoveDeltaTime += PreviousMoveDeltaTimes[ ReversedHistIndex ];
						if( TotalMoveDeltaTime > MaxEscapeVelocityTime )
						{
							break;
						}

						FLOAT Dist = ( PreviousLocations[ ReversedHistIndex ] - CurrentLocation ).Size();
						if( Dist > FurthestMovedRecently )
						{
							FurthestMovedRecently = Dist;
						}
					}
				}

				if( FurthestMovedRecently < MinDistanceForEscapeVelocity )
				{
					EscapeVelocity = FVector2D( 0.0f, 0.0f );
				}
			}

			// Send the tap if it was actually a tap 
			{
				// debugf(TEXT("Here %f %f %i"),TouchDuration,InputOwner->MobileMinHoldForTap,bTrackTap);
				if ( TouchDuration <= InputOwner->MobileMinHoldForTap && bTrackTap )
				{
					// Only consider this a tap if the cursor hasn't moved too far
					if( TapDistanceConstraint > 0 && TouchTotalMoveDistance <= TapDistanceConstraint )
					{
						if (!DELEGATE_IS_SET(OnTapDelegate) || !delegateOnTapDelegate(this, EventType, TouchLocation))
						{
							InputOwner->SendInputKey(TapInputKey, IE_Pressed, 1.0f);							
							InputOwner->SendInputKey(TapInputKey, IE_Released, 1.0f);							
						}
					}
				}
			}

			// If this was a double tap and hold, then send the end
			if (bIsDoubleTapAndHold && bTrackDoubleTap)
			{
				if (!DELEGATE_IS_SET(OnDoubleTapDelegate) || !delegateOnDoubleTapDelegate(this, EventType, TouchLocation))
				{
					InputOwner->SendInputKey(DoubleTapInputKey, IE_Released, 1.0f);							
					bIsDoubleTapAndHold = false;
				}
			}
		}

		if (Type == ZoneType_Joystick)
		{
			CurrentLocation = CurrentCenter;
		}

		UpdateListeners();

	}
	else		// Handle an Update event
	{
		// Store off the current location

		const UBOOL bHasMoved = CurrentLocation != TouchLocation;

		if (Type == ZoneType_Slider)
		{
			if (SlideType == ZoneSlide_LeftRight)
			{
				CurrentLocation.X = Clamp<FLOAT>(TouchLocation.X - CurrentCenter.X,X, X+SizeX);
				CurrentLocation.Y = Y;
			}
			else
			{
				CurrentLocation.X = X;
				CurrentLocation.Y = Clamp<FLOAT>(TouchLocation.Y - CurrentCenter.Y, Y, Y+SizeY);
			}
		}
		else
		{
			CurrentLocation = TouchLocation;
		}

		// process analog zones every frame
		if (Type == ZoneType_Joystick || Type == ZoneType_Trackball)
		{
			FVector2D SmoothedCurrentLocation = TouchLocation;
			FLOAT SmoothedMoveDeltaTime = MoveDeltaTime;


			// Trackball zones use relative movement so we support smoothing input data
			if( Type == ZoneType_Trackball )
			{
				if( bHasMoved )
				{
					// Update smoothing history
					{
						const INT LastHistIndex = ARRAY_COUNT( PreviousLocations ) - 1;
						for( INT CurHistIndex = 0; CurHistIndex < LastHistIndex; ++CurHistIndex )
						{
							PreviousLocations[ CurHistIndex ] = PreviousLocations[ CurHistIndex + 1 ];
							PreviousMoveDeltaTimes[ CurHistIndex ] = PreviousMoveDeltaTimes[ CurHistIndex + 1 ];
						}
						PreviousLocations[ LastHistIndex ] = TouchLocation;
						PreviousMoveDeltaTimes[ LastHistIndex ] = MoveDeltaTime;

						// Increment number of stored historic locations
						PreviousLocationCount = Min( PreviousLocationCount + 1, (INT)ARRAY_COUNT( PreviousLocations ) );
					}
				}


				// Apply smoothing
				if( Smoothing > KINDA_SMALL_NUMBER )
				{
					const FLOAT MaxSmoothingTime = 0.15f;
					FVector2D AverageLocation;
					FLOAT AverageMoveDeltaTime;
					ComputeSmoothedMovement( MaxSmoothingTime, AverageLocation, AverageMoveDeltaTime );

					const FLOAT ClampedSmoothingAmount = Clamp( Smoothing, 0.0f, 1.0f );
					SmoothedCurrentLocation = Lerp( CurrentLocation, AverageLocation, ClampedSmoothingAmount );
					SmoothedMoveDeltaTime = Lerp( MoveDeltaTime, AverageMoveDeltaTime, ClampedSmoothingAmount );
				}
			}


			// axis 0 is vertical, axis 1 is horizontal
			for (INT Axis = 1; Axis >= 0; Axis--)
			{
				FName InputKeyToSend = Axis ? HorizontalInputKey : InputKey;
				if (InputKeyToSend != NAME_None)
				{
					FLOAT Value = 0;

					// we want to map curLoc - initLoc into a -1.0f .. 1.0f range
					// we use the size of the zone to determine the mapping rate (how many screen pixels is -1.0f .. 1.0f)
					// so, imagine initLoc was dead center of the zone, half the zone size becomes 0..1
					// but that means you have to drag to edge of zone to get 1.0, so, let's actually half that again
					// so that you only need to drag halfway from center (again, assuming initLoc is center),
					// we divide the zone size by 4 (the 1/2 of 1/2 of the zone)
					// but maybe dragging to the edge of the zone is fine, so divide by 2...
					if (Axis)
					{
						Value = (FLOAT)(SmoothedCurrentLocation.X - InitialLocation.X);
						if ( Type == ZoneType_Joystick )
						{
							Value /= ((FLOAT)ActiveSizeX / 2.0f);
						}
						Value *= HorizMultiplier;
					}
					else
					{
						Value = (FLOAT)(SmoothedCurrentLocation.Y - InitialLocation.Y);
						if ( Type == ZoneType_Joystick )
						{
							Value /= ((FLOAT)ActiveSizeY / 2.0f);
						}
						Value *= VertMultiplier;
					}
					// Trackball zones use relative movement so we support acceleration and escape velocity
					if( Type == ZoneType_Trackball )
					{
						if( Acceleration > KINDA_SMALL_NUMBER )
						{
							// Acceleration is actually 'movement deltas per second'
							if( SmoothedMoveDeltaTime > SMALL_NUMBER )
							{
								// Limit the min and max quantum size
								const FLOAT ClampedMoveDeltaTime = Clamp( SmoothedMoveDeltaTime, 1.0f / 120.0f, 1.0f / 10.0f );	// 120Hz - 10Hz
								const FLOAT NewValue = Value * ( Acceleration / ClampedMoveDeltaTime );
								//debugf(TEXT("Handle=%d, Axis=%d, Value=%0.3f, NewValue=%0.3f, Accel=%0.3f, ClampedMoveDeltaTime=%0.3f"), Handle, Axis, Value, NewValue, Acceleration, ClampedMoveDeltaTime );
								Value = NewValue;
							}
						}

						if( bHasMoved && Type == ZoneType_Trackball )
						{
							// Store last value so we can use it for our escape velocity
							EscapeVelocity[ 1 - Axis ] = Value;
						}
					}

					if ( Type == ZoneType_Joystick )
					{
						// finally, clamp to -1.0 to 1.0 range 
						Value = Clamp<FLOAT>(Value, -1.0f, 1.0f);
						if ( InputKeyToSend.ToString() == TEXT("MOBILE_aStrafe") )
						{
							MovePercent[0] = Value;
						}
						else if ( InputKeyToSend.ToString() == TEXT("MOBILE_aForward") )
						{
							MovePercent[1] = Value;
						}
					}


					// On some devices (e.g. iOS), the hardware's built-in touch sensor has a dead zone for movement
					// that is difficult to compensate for.  We don't want input deltas to spike on the first
					// movement (as that causes the camera to jump), so we opt to SWALLOW the first movement
					// sample from the device.  We still use the sample for smoothing calculations and it
					// doesn't affect the total duration of the stroke, but by preventing the delta from
					// being sent to the input system we avoid fairly severe mouse-look artifacts.
					if( !bAllowFirstDeltaForTrackballZone && Type == ZoneType_Trackball && PreviousLocationCount == 1 )
					{
						// Swallow device's (dead-zoned) initial movement since it causes the view to pop
						// debugf(TEXT("SWALLOWING Handle=%d, Axis=%d, Value=%0.3f, MoveDeltaTime=%0.3f, PosCount=%d"), Handle, Axis, Value, MoveDeltaTime, PreviousLocationCount );
						Value = 0.0f;
					}
					else
					{
						// debugf(TEXT("SENDING Handle=%d, Axis=%d, Value=%0.3f, MoveDeltaTime=%0.3f, PosCount=%d"), Handle, Axis, Value, MoveDeltaTime, PreviousLocationCount );
					}


					// send the analog value
					InputOwner->SendInputAxis(InputKeyToSend, Value, DeltaTime);

					if (Axis) 
					{
						LastAxisValues.Y = Value;
					}
					else
					{
						LastAxisValues.X = Value;
					}
				}
			}

			if ( Type == ZoneType_Trackball )
			{
				InitialLocation = SmoothedCurrentLocation;
			}

			if (bIsDoubleTapAndHold && RealTime - LastTouchTime > InputOwner->MobileTapRepeatTime && bTrackDoubleTap)
			{

				if (!DELEGATE_IS_SET(OnDoubleTapDelegate) || !delegateOnDoubleTapDelegate(this, EventType, TouchLocation))
				{
					InputOwner->SendInputKey(DoubleTapInputKey, IE_Repeat, 1.0f);
					LastTouchTime = RealTime;
				}
			}

		}

		// only pulse button repeats every once in awhile 
		else if (Type == ZoneType_Button) 
		{
			if (TimeSinceLastTapRepeat >= InputOwner->MobileTapRepeatTime )
			{
				// repeat the button
				InputOwner->SendInputKey(InputKey, IE_Repeat, 1.0f);
				// reset the counter
				TimeSinceLastTapRepeat = 0.0f;					
			}
		}

		UpdateListeners();
	}

	// Pass along the slider delegates
	if (Type == ZoneType_Slider && DELEGATE_IS_SET(OnProcessSlide))
	{
		INT Offset;

		FVector2D ViewportSize;
		UGameViewportClient* GameViewportClient = GEngine->GameViewport;
		if (GameViewportClient != NULL)
		{
			GameViewportClient->GetViewportSize(ViewportSize);
		}

		Offset = appTrunc(SlideType == ZoneSlide_LeftRight ? CurrentLocation.X : CurrentLocation.Y);
		delegateOnProcessSlide(this, EventType, Offset, ViewportSize);
	}


	if (bScalePawnMovement)
	{
		AController* Controller = Cast<AController>(InputOwner->GetOuter());
		if (Controller && Controller->Pawn)
		{
			FLOAT MovePercentSquared = MovePercent[0] * MovePercent[0] + MovePercent[1] * MovePercent[1];
			Controller->Pawn->MovementSpeedModifier = sqrtf( MovePercentSquared > 0.0f ? MovePercentSquared : 1.0f );
		}
	}

}

/**
 * This function will iterate over the MobileSeqEventHandles array and cause them to be updated.  
 * It gets called once per frame.
 */
void UMobileInputZone::UpdateListeners()
{
	if (MobileSeqEventHandlers.Num() > 0)
	{
		APlayerController * PC = Cast<APlayerController>(InputOwner->GetOuter());
		for(INT i=0;i<MobileSeqEventHandlers.Num();i++)
		{
			if (MobileSeqEventHandlers(i) != NULL)
			{
				MobileSeqEventHandlers(i)->UpdateZone(PC,InputOwner,this);
			}
		}
	}
}


void UMobileInputZone::TickZone(FLOAT DeltaTime)
{
	TimeSinceLastTapRepeat += DeltaTime;

	// Every tick, make sure no fade is applied unless we really need it.  This variable will be
	// set below if we're in the middle of a transition
	AnimatingFadeOpacity = 1.0f;
	if (State == ZoneState_Activating || State == ZoneState_Deactivating)
	{
		if (Type == ZoneType_Slider && bCenterOnEvent && State == ZoneState_Deactivating)
		{
			if (SlideType == ZoneSlide_LeftRight)
			{
				CurrentLocation.X = FInterpEaseInOut(InitialLocation.X,X,TransitionTime / DeactivateTime, 2.0);
			}
			else
			{
				CurrentLocation.Y = FInterpEaseInOut(InitialLocation.Y,Y,TransitionTime / DeactivateTime, 2.0);
			}

			// Pass along the slider delegates
			if (DELEGATE_IS_SET(OnProcessSlide))
			{
				INT Offset;

				FVector2D ViewportSize;
				UGameViewportClient* GameViewportClient = GEngine->GameViewport;
				if (GameViewportClient != NULL)
				{
					GameViewportClient->GetViewportSize(ViewportSize);
				}

				Offset = appTrunc(SlideType == ZoneSlide_LeftRight ? CurrentLocation.X : CurrentLocation.Y);
				delegateOnProcessSlide(this, Touch_Moved, Offset, ViewportSize);
			}
		}

		// Update the Transition Time
		TransitionTime += DeltaTime;

		if (State == ZoneState_Activating && TransitionTime > ActivateTime)
		{
			State = ZoneState_Active;
			TransitionTime = 0.0f;
		}
		else if (State == ZoneState_Deactivating && TransitionTime > DeactivateTime)
		{
			State = ZoneState_Inactive;
			TransitionTime = 0.0f;
		}
	}
	else if( State == ZoneState_Inactive )
	{
		if( Type == ZoneType_Joystick && bCenterOnEvent && ResetCenterAfterInactivityTime > KINDA_SMALL_NUMBER )
		{
			// Only start the transition if the joystick is already off-center
			const FLOAT MinDistanceBeforeReset = 0.01f;
			const FLOAT DistanceFromCenter = ( CurrentCenter - InitialCenter ).Size();
			if( TransitionTime > 0.0f || DistanceFromCenter > MinDistanceBeforeReset )
			{
				// Update the Transition Time
				TransitionTime += DeltaTime;
			}

			// Check to see if enough time has passed since this joystick was last used to start
			// re-centering the graphic.
			if( TransitionTime > ResetCenterAfterInactivityTime )
			{
				FVector2D DesiredLocation = InitialCenter;

				// Are smooth transitions enabled?
				if( bUseGentleTransitions )
				{
					// Length of fade out (after we've been inactive for the configured time length)
					const FLOAT FadeOutTime = 1.0f;

					// How long to remain invisible after we've faded out, before fading back in
					const FLOAT SustainTime = 0.5f;

					// Length of fade in (after we've been invisible for SustainTime)
					const FLOAT FadeInTime = 2.0f;


					// Update fading state
					const FLOAT EffectTime = TransitionTime - ResetCenterAfterInactivityTime;
					if( EffectTime < FadeOutTime )
					{
						// Fading out at original position
						AnimatingFadeOpacity = 1.0f - EffectTime / FadeOutTime;
						DesiredLocation = CurrentCenter;
					}
					else if( EffectTime < FadeOutTime + SustainTime )
					{
						// Sustaining transparency at initial center position
						AnimatingFadeOpacity = 0.0f;
						DesiredLocation = InitialCenter;
					}
					else
					{
						// Fading back in at initial center position
						const FLOAT FadeInPos = EffectTime - ( FadeOutTime + SustainTime );
						AnimatingFadeOpacity = Min( 1.0f, FadeInPos / FadeInTime );
						DesiredLocation = InitialCenter;
					}
				}

				// Update position of joystick zone and hat location
				CurrentCenter = CurrentLocation = InitialLocation = DesiredLocation;
			}
		}
	}
}



/**
 * Applies any remaining escape velocity for this zone
 *
 * @param DeltaTime		Much time has elapsed since the last processing
 */
void UMobileInputZone::ApplyEscapeVelocity( FLOAT DeltaTime )
{
	// Were we configured to apply escape velocity after the user releases the touch?
	if( Type == ZoneType_Trackball && EscapeVelocityStrength > KINDA_SMALL_NUMBER )
	{
		if( !EscapeVelocity.IsZero() )
		{
			// debugf( TEXT( "EscapeVel=%0.3f, %0.3f" ), EscapeVelocity.X, EscapeVelocity.Y );

			// axis 0 is vertical, axis 1 is horizontal
			for (INT Axis = 1; Axis >= 0; Axis--)
			{
				FLOAT Value = EscapeVelocity[ 1 - Axis ];

				FName InputKeyToSend = Axis ? HorizontalInputKey : InputKey;
				if (InputKeyToSend != NAME_None)
				{
					FName InputKeyToSend = Axis ? HorizontalInputKey : InputKey;

					InputOwner->SendInputAxis( InputKeyToSend, Value, DeltaTime );
				}

				// Apply dampening
				{
					const FLOAT DampenStrength = 25.0f;
					const FLOAT DampenScalar = DampenStrength * Clamp( 1.0f - EscapeVelocityStrength, 0.0f, 0.999f );
					const FLOAT ClampedDeltaTime = Clamp( DeltaTime, 1.0f / 120.0f, 1.0f / 10.0f );	// 120Hz - 10Hz
					if( Value > 0.0f )
					{
						Value -= Value * DampenScalar * ClampedDeltaTime;
						if( Value < 0.01f )
						{
							Value = 0.0f;
						}
					}
					else
					{
						Value -= Value * DampenScalar * ClampedDeltaTime;
						if( Value > -0.01f )
						{
							Value = 0.0f;
						}
					}
					EscapeVelocity[ 1 - Axis ] = Value;
				}
			}
		}
	}
}


void UMobileInputZone::ActivateZone()
{
	// We only perform activating logic if we are not already active
	if (State == ZoneState_Deactivating || State == ZoneState_Inactive)
	{
		// And if we are using Gentle Transitions
		if (bUseGentleTransitions && ActivateTime > 0.0f)
		{
			if (State == ZoneState_Deactivating)
			{
				// We are already deactivating and now we are activating again.  In order
				// to avoid the pop in the fade, we need to reverse it.

				TransitionTime = ActivateTime * (1.0 - TransitionTime / DeactivateTime);
			}
			else
			{
				TransitionTime = 0;
			}

			State = ZoneState_Activating;

		}
		else
		{
			State = ZoneState_Active;
			TransitionTime = 0;
		}
	}
}

void UMobileInputZone::DeactivateZone()
{
	// We only perform activating logic if we are not already active
	if (State == ZoneState_Active || State == ZoneState_Activating)
	{
		// And if we are using Gentle Transitions
		if (bUseGentleTransitions && DeactivateTime > 0.0f)
		{
			if (State == ZoneState_Activating)
			{
				// We are already deactivating and now we are activating again.  In order
				// to avoid the pop in the fade, we need to reverse it.

				TransitionTime = DeactivateTime * (1.0 - TransitionTime / ActivateTime);
			}
			else
			{
				TransitionTime = 0;
			}

			State = ZoneState_Deactivating;

		}
		else
		{
			State = ZoneState_Inactive;
			TransitionTime = 0;
		}
	}
}


/**
 * Cleans up the mobile menu scene
 */
void UMobileMenuScene::CleanUpScene()
{
	InputOwner = NULL;
	for (INT i=0;i<MenuObjects.Num();i++)
	{
		MenuObjects(i)->InputOwner = NULL;
	}
}


/**
 * Check to see if an input event affects a menu object in this scene
 *
 * @param TouchX	The X Position of the input event
 * @param TouchY	The Y Position of the input event
 *
 * @Returns a reference to the object it touched if one is detected
 */
UMobileMenuObject* UMobileMenuScene::HitTest(FLOAT TouchX, FLOAT TouchY)
{
	for(INT idx = MenuObjects.Num() -1; idx >=0 ; idx--)
	{
		UMobileMenuObject* Obj = MenuObjects(idx);
		if ((Obj != NULL) && Obj->bIsTouched && Obj->bIsActive )
		{
			FLOAT X0, Y0, X1, Y1;
			Obj->eventGetRealPosition(X0,Y0);

			// Once a button is pressed, make the zone bigger to
			// avoid accidental roll off.
			X1 = X0 - (Obj->LeftLeeway + (Obj->Width * 0.5f));
			Y1 = Y0 - (Obj->TopLeeway + (Obj->Height * 0.5f));

			if (TouchX >= X1 && TouchY >= Y1)
			{
				FLOAT X2 = X0 + (Obj->Width * 1.5f) + Obj->RightLeeway;
				FLOAT Y2 = Y0 + (Obj->Height * 1.5f) + Obj->BottomLeeway;

				if (TouchX < X2 && TouchY < Y2)
				{
					return Obj;
				}
			}
		}
	}

	for(INT idx=MenuObjects.Num()-1; idx >= 0; idx--)
	{
		UMobileMenuObject* Obj = MenuObjects(idx);

		if ((Obj != NULL) && Obj->bIsActive)
		{
			FLOAT X0, Y0, X1, Y1;
			Obj->eventGetRealPosition(X0,Y0);
			X1 = X0 - Obj->LeftLeeway;
			Y1 = Y0 - Obj->TopLeeway;

			if (TouchX >= X1 && TouchY >= Y1)
			{
				FLOAT X2 = X0 + Obj->Width + Obj->RightLeeway;
				FLOAT Y2 = Y0 + Obj->Height + Obj->BottomLeeway;

				if (TouchX < X2 && TouchY < Y2)
				{
					return Obj;
				}
			}
		}
	}
	return NULL;
}

/**
 * @return A global X scale factor for UI elements that want a global scale applied
 */
FLOAT UMobileMenuScene::GetGlobalScaleX()
{
#if IPHONE
	static FLOAT GlobalScaleX = IPhoneGetGlobalUIScaleX();
	return GlobalScaleX;
#elif WITH_ES2_RHI
	return GSystemSettings.MobileContentScaleFactor;
#else
	return 1.0f;
#endif
}

/**
 * @return A global Y scale factor for UI elements that want a global scale applied
 */
FLOAT UMobileMenuScene::GetGlobalScaleY()
{
#if IPHONE
	static FLOAT GlobalScaleY = IPhoneGetGlobalUIScaleY();
	return GlobalScaleY;
#elif WITH_ES2_RHI
	return GSystemSettings.MobileContentScaleFactor;
#else
	return 1.0f;
#endif
}

/**
 * Handle touch events in the 3D world. This needs to be assigned to a MobileInputZone's OnTapDelegate or OnDoubleTapDelegate.
 *
 * @param Zone - The MobileInputZone that called this
 * @param EventType - The type of input event
 * @param TouchLocation - The screen location of the touch
 *
 * @return true if the world actor swallows the input
 */
UBOOL UMobilePlayerInput::ProcessWorldTouch(UMobileInputZone* Zone, BYTE EventType, FVector2D TouchLocation)
{
	// Figure out the location relative to the current viewport
	FVector2D ViewportSize;
	GEngine->GamePlayers(0)->ViewportClient->GetViewportSize(ViewportSize);
	FVector2D RelLocation;
	RelLocation.X = TouchLocation.X;
	RelLocation.Y = TouchLocation.Y;
	RelLocation += MobileViewportOffset;
	RelLocation.X /= ViewportSize.X;
	RelLocation.Y /= ViewportSize.Y;
	//debugf(TEXT("%2.2f,%2.2f - %2.2f,%2.2f"), TouchLocation.X, TouchLocation.Y, MobileViewportOffset.X, MobileViewportOffset.Y);

	// Get the location and direction of the touch in world coordinates
	FVector StartLocation, StartDirection;
	GEngine->GamePlayers(0)->DeProject(RelLocation, StartLocation, StartDirection);
	FVector EndLocation = StartLocation + StartDirection * 10000.f;

	// Start with a quick trace to find the closest actor
	FCheckResult Hit(1.f);
	GWorld->SingleLineCheck( Hit, NULL, EndLocation, StartLocation, TRACE_World|TRACE_Actors, FVector(0.f) );
	if( Hit.Actor && Hit.Actor->bEnableMobileTouch && Hit.Actor->eventOnMobileTouch(CastChecked<APlayerController>(GetOuter()), TouchLocation) )
	{
		return TRUE;
	}

	// TODO - If the quick trace doesn't work, try a more expensive algorithm to find what the player intended to touch
	// i.e. we might have to account for fat fingers :)
	// This could also be re-written to ignore blocking actors and find the first actor in the trace that has bEnableMobileTouch set to true

	return FALSE;
}



// no need to listen for tilt messages on a mobile device, and don't want it in FR/Shipping builds
#if (WITH_UE3_NETWORKING && !MOBILE && !FINAL_RELEASE && !SHIPPING_PC_GAME) || (UDK)

// disabled for now
#define  SUPPORT_IMAGE_SEND 0


/** These data types must match up on the other side! */
enum EDataType
{
	DT_TouchBegan=0,
	DT_TouchMoved=1,
	DT_TouchEnded=2,
	DT_Tilt=3,
	DT_Motion=4,
	DT_Gyro=5,
	DT_Ping=6,
};

/** Possible device orientations */
enum EDeviceOrientation
{
	DO_Unknown,
	DO_Portrait,
	DO_PortraitUpsideDown,
	DO_LandscapeLeft,
	DO_LandscapeRight,
	DO_FaceUp,
	DO_FaceDown,
};

// magic number that must match UDKRemote
#define MESSAGE_MAGIC_ID 0xAB

// versioning information for future expansion
#define CURRENT_MESSAGE_VERSION 1

/** A single event message to send over the network */
struct FMessage
{
	/** A byte that must match to what we expect */
	BYTE MagicTag;

	/** What version of message this is from UDK Remote */
	BYTE MessageVersion;

	/** Unique Id for the message, used for detecting lost/duplicate packets, etc (only duplicate currently handled) */
	WORD MessageID;

	/** What type of message is this? */
	BYTE DataType;

	/** Unique identifier for the touch/finger */
	BYTE Handle;

	/** The current orientation of the device */
	BYTE DeviceOrientation;

	/** The current orientation of the UI */
	BYTE UIOrientation;

	/** X/Y or Pitch/Yaw data or CoreMotion data */
	FLOAT Data[12];
};


class FTiltListener : public FTickableObject
{
protected:
	/** The socket to listen on, will be initialized in first tick */
	FSocket* TiltSocket;

	/** The socket to send replies on, will be initialized in first tick */
	FSocket* ReplySocket;

	/** The socket to send image data on, will be initialized in first tick */
	FSocket* ImageSocket;

	/** The address of the most recent UDKRemote to talk to us, this is who we reply to */
	FInternetIpAddr ReplyAddr;

	/** Ever increasing timestamp to send to the input system */
	DOUBLE Timestamp;

	/** Highest message ID (must handle wrapping around at 16 bits) */
	WORD HighestMessageReceived;

	FLOAT TimeSinceLastPing;
//	/** Event used to */
//	FEvent* CompressionCompleteEvent;
//	/** An event used to signal 
//	FEvent* EncoderIsReadyEvent;

public:
	FTiltListener()
		: TiltSocket(NULL)
		, Timestamp(0.0)
		, HighestMessageReceived(0xFFFF)
		, TimeSinceLastPing(200.0f)
	{
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTic.cpp after ticking all actors or from
	 * the rendering thread (depending on bIsRenderingThreadObject)
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( FLOAT DeltaTime )
	{
		// attempt to open the socket if it needs it
		if (TiltSocket == NULL)
		{
			if (GSocketSubsystem)
			{
				// create a UDP listening socket
				TiltSocket = GSocketSubsystem->CreateDGramSocket(TEXT("TiltListener"));
				
				if (TiltSocket)
				{
					// bind the socket to listen on the port
					FInternetIpAddr BindAddr;
					BindAddr.SetAnyAddress();
					INT Port;
					if (GIsEditor)
					{
						verify(GConfig->GetInt(TEXT("MobileSupport"), TEXT("UDKRemotePortPIE"), Port, GEngineIni));
					}
					else
					{
						verify(GConfig->GetInt(TEXT("MobileSupport"), TEXT("UDKRemotePort"), Port, GEngineIni));
					}

					BindAddr.SetPort(Port);

					TiltSocket->Bind(BindAddr);

					// make an async socket
					TiltSocket->SetNonBlocking(TRUE);

					// create a UDP reply socket
					ReplySocket = GSocketSubsystem->CreateDGramSocket(TEXT("TiltReply"));
					if (ReplySocket)
					{
						// make an async socket
						ReplySocket->SetNonBlocking(TRUE);
					}

					ImageSocket = GSocketSubsystem->CreateDGramSocket(TEXT("TiltImage"));
					if (ImageSocket)
					{
						// make an async socket
						ImageSocket->SetNonBlocking(TRUE);
					}
				}
			}
		}
		
		if (TiltSocket && ReplySocket)
		{
			// Get a pointer to the input system
			UGameViewportClient* ViewportClient = NULL;
			if (GWorld && GEngine && GEngine->GamePlayers.Num() && GEngine->GamePlayers(0))
			{
				ViewportClient = GEngine->GamePlayers(0)->ViewportClient;
			}

			// if there isn't one, there's no input we need to do
			if (!ViewportClient)
			{
				return;
			}

			FVector2D ViewportSize;
			ViewportClient->GetViewportSize(ViewportSize);

			// try to read some data from the 
			BYTE Msg[256];
			INT BytesRead;
			while (TiltSocket->RecvFrom(Msg, sizeof(Msg), BytesRead, ReplyAddr))
			{
				// make sure it's what we expected
				if (BytesRead != sizeof(FMessage))
				{
					debugf(TEXT("Received %d bytes, expected %d"), BytesRead, sizeof(FMessage));
					continue;
				}

				// get the message in a blob we can process
				FMessage* Message = (FMessage*)Msg;

				// make sure tag and version are okay
				UBOOL bIsValidMessageVersion = 
					Message->MagicTag == MESSAGE_MAGIC_ID && 
					// in the future, we can write code to handle older versions
					Message->MessageVersion == CURRENT_MESSAGE_VERSION;

				// make sure we got a later message than last time (handling wrapping
				// around with plenty of slop)
				UBOOL bIsValidID = 
					(Message->DataType == DT_Ping) ||
					(Message->MessageID > HighestMessageReceived) || 
					(Message->MessageID < 1000 && HighestMessageReceived > 65000);
				HighestMessageReceived = Message->MessageID;

				UBOOL bIsValidMessage = bIsValidMessageVersion && bIsValidID;
				if (!bIsValidMessage)
				{
//					debugf(TEXT("Dropped message %d, %d"), Message->MessageID, Message->DataType);
					continue;
				}
				else
				{
//					debugf(TEXT("Received message %d, %d"), Message->MessageID, Message->DataType);
				}

				// handle the type
				ETouchType Type = Touch_Stationary;
				UBOOL bIsTilt = FALSE;
				switch (Message->DataType)
				{
					case DT_TouchBegan: Type = Touch_Began; break;
					case DT_TouchMoved: Type = Touch_Moved; break;
					case DT_TouchEnded: Type = Touch_Ended; break;
					default: bIsTilt = TRUE;
				}

				// handle tilt input
				if (bIsTilt)
				{
					if (Message->DataType == DT_Tilt || Message->DataType == DT_Motion)
					{
						FVector Attitude;
						FVector RotationRate;
						FVector Gravity;
						FVector Accel;

						if (Message->DataType == DT_Tilt)
						{
							// get the raw and processed values from the other end
							FVector CurrentAccelerometer(Message->Data[0], Message->Data[1], Message->Data[2]);
							FLOAT Pitch = Message->Data[3], Roll = Message->Data[4];

							// convert it into "Motion" data
							static FLOAT LastPitch, LastRoll;

							Attitude = FVector(Pitch,0,Roll);
							RotationRate = FVector(LastPitch - Pitch,0,LastRoll - Roll);
							Gravity = FVector(0,0,0);
							Accel = CurrentAccelerometer;

							// save the previous values to delta rotation
							LastPitch = Pitch;
							LastRoll = Roll;
						}
						else if (Message->DataType == DT_Motion)
						{
							// just use the values directly from UDK Remote
							// Negate the yaw to match direction
							Attitude = FVector(Message->Data[0], -Message->Data[1], Message->Data[2]);
							RotationRate = FVector(Message->Data[3], -Message->Data[4], Message->Data[5]);
							Gravity = FVector(Message->Data[6], Message->Data[7], Message->Data[8]);
							Accel = FVector(Message->Data[9], Message->Data[10], Message->Data[11]);
						}

						// munge the vectors based on the orientation of the remote device
						EUIOrientation Orientation = (EUIOrientation)Message->UIOrientation;
						UMobilePlayerInput::ModifyVectorByOrientation(Attitude, Orientation, TRUE);
 						UMobilePlayerInput::ModifyVectorByOrientation(RotationRate, Orientation, TRUE);
 						UMobilePlayerInput::ModifyVectorByOrientation(Gravity, Orientation, FALSE);
 						UMobilePlayerInput::ModifyVectorByOrientation(Accel, Orientation, FALSE);

						// send the tilt to the input system
						ViewportClient->InputMotion(ViewportClient->Viewport, 0, Attitude, RotationRate, Gravity, Accel);
					}
					else if (Message->DataType == DT_Gyro)
					{
						// ignore for now
					}
					else if (Message->DataType == DT_Ping)
					{
						TimeSinceLastPing = 0.0f;
						// reply
						ReplyAddr.SetPort(41764);

						INT BytesSent;
						const ANSICHAR* HELO = "HELO";
						ReplySocket->SendTo((BYTE*)HELO, 5, BytesSent, ReplyAddr);
					}
				}
				// handle touches
				else
				{
					// put the location into the window coordinates of this game
					FVector2D Location(Message->Data[0] * ViewportSize.X, Message->Data[1] * ViewportSize.Y);

					// send input to handler
					ViewportClient->InputTouch(ViewportClient->Viewport, 0, Message->Handle, Type, Location, Timestamp, 0);

					// increment our local timestamps
					Timestamp += DeltaTime;
				}
			}

			TimeSinceLastPing += DeltaTime;
		}
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const
	{
		extern UBOOL appGameSupportsTiltPusher();
		return appGameSupportsTiltPusher() && GSocketSubsystem != NULL;
	}

	virtual UBOOL IsTickableWhenPaused() const
	{
		return IsTickable();
	}

	friend class FVideoCapture;
};

/** Singlton instance of the tilt listener */
FTiltListener GTiltListener;

#if SUPPORT_IMAGE_SEND

/**
 * Asynchronous vorbis decompression
 */
class FAsyncImageCompressTask : public FNonAbandonableTask
{
//	TArray<BYTE> CompressedData;

protected:
	TArray<BYTE> Bitmap;
	INT* ValueToIncrement;
	FSocket* ReplySocket;
	FInternetIpAddr ReplyAddr;
	INT SizeX, SizeY;

public:
	/**
	 * Constructor, will get the image data it needs
	 */
	FAsyncImageCompressTask(INT* InValueToIncrement, FSocket* InReplySocket, FInternetIpAddr InReplyAddr)
		: ValueToIncrement(InValueToIncrement)
		, ReplySocket(InReplySocket)
		, ReplyAddr(InReplyAddr)
		, SizeX(200)
		, SizeY(150)
	{
		// downsample the back buffer to a portion of the light attenuation buffer (it's
		// just a temp render target)
		RHIDownsampleBackBuffer(GSceneRenderTargets.GetLightAttenuationSurface(), SizeX, SizeY);

		// read that portion back to CPU memory
		RHIReadSurfaceData(
			GSceneRenderTargets.GetLightAttenuationSurface(),
			0,
			0,
			SizeX - 1,
			SizeY - 1,
			Bitmap,
			FReadSurfaceDataFlags()
			);
	}

	/**
	 * Performs the async vorbis decompression
	 */
	void DoWork()
	{
		// set to opaque
		for (INT ColorIndex = 0; ColorIndex < Bitmap.Num(); ColorIndex+=4)
		{
			// red blue swap
			Swap<BYTE>(Bitmap(ColorIndex + 0), Bitmap(ColorIndex + 2));
			// force alpha to full
			Bitmap(ColorIndex + 3) = 255;
		}

// 		jpeg_compress_struct Compressor;
// 		jpeg_create_compress(Compressor);
#if !CONSOLE
		// compress the image
		FPNGHelper PNG;
		PNG.InitRaw(Bitmap.GetData(), Bitmap.Num(), SizeX, SizeY);
		const TArray<BYTE>& PNGData = PNG.GetCompressedData();

		INT BytesSent;

		// split up the png data into chunks
		INT NumChunks = (PNGData.Num() + 32767) / 32768;
		INT Header[] = {0xFFEEDDCC, NumChunks};
		ReplySocket->SendTo((BYTE*)Header, sizeof(Header), BytesSent, ReplyAddr);
		if (BytesSent == -1)
		{
			debugf(TEXT("Socket error: %s"), GSocketSubsystem->GetSocketError());
		}

		// send each chunk
		for (INT ChunkIndex = 0; ChunkIndex < NumChunks; ChunkIndex++)
		{
			INT Attempts = 0;
			do 
			{
				ReplySocket->SendTo(PNGData.GetTypedData() + ChunkIndex * 32768, Min(32768, PNGData.Num() - ChunkIndex * 32768), BytesSent, ReplyAddr);
			} while (BytesSent == -1);// && ++Attempts < 3);

		}
//		debugf(TEXT("REPLYING TO %s [%d / %d]"), *ReplyAddr.ToString(TRUE), BytesSent, PNGData.Num());

		// tell other thread we are done
		appInterlockedIncrement(ValueToIncrement);
#endif
	}

	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncImageCompressTask");
	}
};


class FVideoCapture : public FTickableObjectRenderThread
{
	/** Thread safe flag to determine if encoding job is free */
	INT IsEncoderThreadBusy;

	INT IsEncoderThreadComplete;


public:
	FVideoCapture()
		: FTickableObjectRenderThread()
		, IsEncoderThreadBusy(0)
		, IsEncoderThreadComplete(0)
	{
	}


	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTic.cpp after ticking all actors or from
	 * the rendering thread (depending on bIsRenderingThreadObject)
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( FLOAT DeltaTime )
	{
		// start encoding a frame if we can
		if (!IsEncoderThreadBusy)
		{
			if (GTiltListener.TimeSinceLastPing < 5.0f)
			{
				// note that we are now busy
				IsEncoderThreadBusy = 1;
				IsEncoderThreadComplete = 0;
				// kick off async task
				(new FAutoDeleteAsyncTask<FAsyncImageCompressTask>(&IsEncoderThreadComplete, GTiltListener.ImageSocket, GTiltListener.ReplyAddr))->StartBackgroundTask();
			}
		}
		else
		{
			// did async task finish?
			if (IsEncoderThreadComplete)
			{
				IsEncoderThreadBusy = FALSE;
			}
		}
	}
		
	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const
	{
		return GTiltListener.IsTickable() && GSceneRenderTargets.GetLightAttenuationSurface() != NULL;
	}
};

FVideoCapture GVideoCapture;
#endif


#endif



