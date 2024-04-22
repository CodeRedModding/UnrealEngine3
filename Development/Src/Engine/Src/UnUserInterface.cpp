/*=============================================================================
	UnUserInterface.cpp: UI system structs, utility, and helper class implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// engine classes
#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "ScenePrivate.h"

// widgets and supporting UI classes
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"

// UI kismet classes
#include "EngineSequenceClasses.h"

// Utility classes
#include "ScopedObjectStateChange.h"

//GFx Includes
#if WITH_GFx
#include "ScaleformEngine.h"
#endif //WITH_GFx

IMPLEMENT_CLASS(UUIRoot);
	IMPLEMENT_CLASS(UUISceneClient);
		IMPLEMENT_CLASS(UGameUISceneClient);

IMPLEMENT_CLASS(UUISoundTheme);
IMPLEMENT_CLASS(UUIManager);

#define TEMP_SPLITSCREEN_INDEX UCONST_TEMP_SPLITSCREEN_INDEX

DECLARE_STATS_GROUP(TEXT("UI"),STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("UI Kismet Time"),STAT_UIKismetTime,STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("UI Scene Render Time"),STAT_UISceneRenderTime,STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("UI Drawing Time"),STAT_UIDrawingTime,STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("UI Scene Tick Time"),STAT_UISceneTickTime,STATGROUP_UI);
DECLARE_CYCLE_STAT(TEXT("UI Tick Time"),STAT_UITickTime,STATGROUP_UI);


/**
 * Creates and initializes an instance of a UDataStoreClient.
 *
 * @param	InOuter		the object to use for Outer when creating the global data store client
 *
 * @return	a pointer to a fully initialized instance of the global data store client class.
 */
UDataStoreClient* FGlobalDataStoreClientManager::CreateGlobalDataStoreClient( UObject* InOuter ) const
{
	UDataStoreClient* Result = NULL;
	if ( GEngine->DataStoreClientClass != NULL )
	{
		Result = ConstructObject<UDataStoreClient>(GEngine->DataStoreClientClass, InOuter, NAME_None, RF_Transient);
		if ( Result != NULL )
		{
			Result->AddToRoot();
		}
	}

	return Result;
}



/* ==========================================================================================================
	UUISceneClient
========================================================================================================== */
/**
 * Handles processing console commands.
 *
 * @param	Cmd		the text that was entered into the console
 * @param	Ar		the archive to use for serializing responses
 *
 * @return	TRUE if the command specified was processed
 */
UBOOL UUISceneClient::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if ( ScriptConsoleExec(Cmd, Ar, this) )
	{
		return TRUE;
	}
	return FALSE;
}

/**
 * Performs any initialization for the UISceneClient.
 */
void UUISceneClient::InitializeClient()
{
	// use the default post process chain for UI scenes
	if( UIScenePostProcess == NULL )
	{
		UIScenePostProcess = GEngine->DefaultUIScenePostProcess;
	}

	eventInitializeSceneClient();
}

/**
 * Assigns the viewport that scenes will use for rendering.
 *
 * @param	inViewport	the viewport to use for rendering scenes
 */
void UUISceneClient::SetRenderViewport( FViewport* SceneViewport )
{
	RenderViewport = SceneViewport;
	if( SceneViewport != NULL )
	{
		if ( GCallbackEvent != NULL )
		{
			GCallbackEvent->Send(CALLBACK_ViewportResized, SceneViewport, 0);
		}
	}
}

/**
 * Returns the current local to world screen projection matrix.
 *
 * @param	Widget	if specified, the returned matrix will include the widget's tranformation matrix as well.
 *
 * @return	a matrix which can be used to project 2D pixel coordines into 3D screen coordinates.
 */
FMatrix UUISceneClient::GetCanvasToScreen() const
{
	return CanvasToScreen;
}

/**
 * Returns the inverse of the local to world screen projection matrix.
 *
 * @param	Widget	if specified, the returned matrix will include the widget's tranformation matrix as well.
 *
 * @return	a matrix which can be used to transform normalized device coordinates (i.e. origin at center of screen) into
 *			into 0,0 based pixel coordinates.
 */
FMatrix UUISceneClient::GetInverseCanvasToScreen() const
{
 	return InvCanvasToScreen;
}

/**
 * Returns true if the UI scenes should be rendered with post process
 *
 * @return TRUE if post process is enabled for any of the UI scenes
 */
UBOOL UUISceneClient::UsesPostProcess() const
{
	return( bEnablePostProcess && IsUIActive(SCENEFILTER_UsesPostProcessing) );
}

/* ==========================================================================================================
	UGameUISceneClient
========================================================================================================== */
/**
 * @return	the current netmode
 */
BYTE/*ENetMode*/ UGameUISceneClient::GetCurrentNetMode()
{
	return GWorld ? GWorld->GetNetMode() : NM_MAX;
}

/**
 * Handles processing console commands.
 *
 * @param	Cmd		the text that was entered into the console
 * @param	Ar		the archive to use for serializing responses
 *
 * @return	TRUE if the command specified was processed
 */
UBOOL UGameUISceneClient::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if ( Super::Exec(Cmd,Ar) )
	{
		return TRUE;
	}

	return FALSE;
}


/**
 * Called when the UI controller receives a CALLBACK_ViewportResized notification.
 *
 * @param	SceneViewport	the viewport that was resized
 */
void UGameUISceneClient::NotifyViewportResized( FViewport* SceneViewport )
{

}


/**
 * Resets the time and mouse position values used for simulating double-click events to the current value or invalid values.
 */
void UGameUISceneClient::ResetDoubleClickTracking( UBOOL bClearValues )
{
	if ( bClearValues )
	{
		DoubleClickStartTime = INDEX_NONE;
		DoubleClickStartPosition = FIntPoint(-1,-1);
	}
	else
	{
		DoubleClickStartTime = appSeconds();
		DoubleClickStartPosition = MousePosition;
	}
}


/**
 * Checks the current time and mouse position to determine whether a double-click event should be simulated.
 */
UBOOL UGameUISceneClient::ShouldSimulateDoubleClick() const
{
	UUIInteraction* UIController = GetOuterUUIInteraction();
	return	appSeconds() - DoubleClickStartTime < (DOUBLE)UIController->DoubleClickTriggerSeconds
		&&	Abs(MousePosition.X - DoubleClickStartPosition.X) <= UIController->DoubleClickPixelTolerance
		&&	Abs(MousePosition.Y - DoubleClickStartPosition.Y) <= UIController->DoubleClickPixelTolerance;
}


/**
 * Determines whether the any active scenes process axis input.
 *
 * @param	bProcessAxisInput	receives the flags for whether axis input is needed for each player.
 */
void UGameUISceneClient::CheckAxisInputSupport( UBOOL* bProcessAxisInput[UCONST_MAX_SUPPORTED_GAMEPADS] ) const
{
}

/**
 * Updates the value of UIInteraction.bProcessingInput to reflect whether any scenes are capable of processing input.
 */
void UGameUISceneClient::UpdateInputProcessingStatus()
{
	UBOOL bProcessAxisInput[UCONST_MAX_SUPPORTED_GAMEPADS] = { FALSE, FALSE, FALSE, FALSE };
	UBOOL* pProcessAxisInput[UCONST_MAX_SUPPORTED_GAMEPADS] = { &bProcessAxisInput[0], &bProcessAxisInput[1], &bProcessAxisInput[2], &bProcessAxisInput[3] };
	CheckAxisInputSupport(pProcessAxisInput);

	UBOOL bUIProcessesInput = FALSE;
	for ( INT Idx = 0; Idx < UCONST_MAX_SUPPORTED_GAMEPADS; Idx++ )
	{
		if ( bProcessAxisInput[Idx] )
		{
			bUIProcessesInput = TRUE;
			break;
		}
	}

	UBOOL bShouldFlushPlayerInput = FALSE;

	UUIInteraction* UIController = GetOuterUUIInteraction();

	// enable/disable the axis emulation for all players
	for ( INT PlayerIndex = 0; PlayerIndex < UCONST_MAX_SUPPORTED_GAMEPADS; PlayerIndex++ )
	{
		UIController->AxisInputEmulation[PlayerIndex].EnableAxisEmulation(bProcessAxisInput[PlayerIndex]);
	}

	const UBOOL bCurrentlyProcessingInput = UIController->bProcessInput;
	UIController->bProcessInput = bUIProcessesInput || (bEnableDebugInput && bRenderDebugInfo && IsUIActive());

	if ( bShouldFlushPlayerInput && bUIProcessesInput && !bCurrentlyProcessingInput )
	{
		FlushPlayerInput();
	}
}

/**
 * Ensures that the game's paused state is appropriate considering the state of the UI.  If any scenes are active which require
 * the game to be paused, pauses the game...otherwise, unpauses the game.
 *
 * @param	PlayerIndex		the index of the player that owns the scene that was just added or removed, or 0 if the scene didn't have
 *							a player owner.
 */
void UGameUISceneClient::UpdatePausedState( INT PlayerIndex )
{
	eventPauseGame(IsUIActive(SCENEFILTER_PausersOnly), PlayerIndex);
}

/**
 * Callback which allows the UI to prevent unpausing if scenes which require pausing are still active.
 * @see PlayerController.SetPause
 */
UBOOL UGameUISceneClient::CanUnpauseInternalUI()
{
	return !IsUIActive(SCENEFILTER_PausersOnly);
}

/**
 * Clears the arrays of pressed keys for all local players in the game; used when the UI begins processing input.  Also
 * updates the InitialPressedKeys maps for all players.
 */
void UGameUISceneClient::FlushPlayerInput()
{
	for ( INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++ )
	{
		ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
		if ( Player != NULL && Player->Actor != NULL && Player->Actor->PlayerInput != NULL )
		{
			//@todo ronp - in some cases, we only want to do this for the player that opened the scene

			// record each key that was pressed when the UI began processing input so we can ignore the released event that will be generated when that key is released
			TArray<FName>* PressedPlayerKeys = InitialPressedKeys.Find(Player->ControllerId);
			if ( PressedPlayerKeys == NULL )
			{
				PressedPlayerKeys = &InitialPressedKeys.Set(Player->ControllerId, TArray<FName>());
			}

			if ( PressedPlayerKeys != NULL )
			{
				for ( INT KeyIndex = 0; KeyIndex < Player->Actor->PlayerInput->PressedKeys.Num(); KeyIndex++ )
				{
					FName Key = Player->Actor->PlayerInput->PressedKeys(KeyIndex);
					PressedPlayerKeys->AddUniqueItem(Key);
				}
			}

			// sending the IE_Released events should have cleared the pressed keys array, but clear them manually
			Player->Actor->PlayerInput->ResetInput();
		}
	}
}


/**
 * Triggers a call to UpdateInputProcessingStatus on the next Tick().
 */
void UGameUISceneClient::RequestInputProcessingUpdate()
{
	bUpdateInputProcessingStatus = TRUE;
}


/**
 * Called once a frame to update the UI's state.
 *
 * @param	DeltaTime - The time since the last frame.
 */
void UGameUISceneClient::Tick(FLOAT DeltaTime)
{
	// Update the cached delta time
	LatestDeltaTime = DeltaTime;

	if ( bUpdateInputProcessingStatus == TRUE )
	{
		bUpdateInputProcessingStatus = FALSE;
		UpdateInputProcessingStatus();
	}

	if ( bUpdateSceneViewportSizes && RenderViewport != NULL )
	{
		bUpdateSceneViewportSizes = FALSE;

		// LayoutPlayers isn't called until viewports are redrawn, which is after Tick is called....which means that the split-screen
		// configuration might be out of date.  Force the players to update their viewport origins and sizes now!
		GetOuterUUIInteraction()->GetOuterUGameViewportClient()->eventLayoutPlayers();
		GCallbackEvent->Send(CALLBACK_ViewportResized, RenderViewport, 0);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_UIKismetTime);
	}
}

#if CONSOLE
#define SAVE_CONFIG
#else
#define SAVE_CONFIG SaveConfig()
#endif

#define LOG_ACTION_RESULT(var)	debugf(TEXT("UIDEBUG: Value of %s is now: %s"), TEXT(#var), var ? GTrue : GFalse)

/**
 * Process an input event which interacts with the in-game scene debugging overlays
 *
 * @param	Key		the key that was pressed
 * @param	Event	the type of event received
 *
 * @return	TRUE if the input event was processed; FALSE otherwise.
 */
UBOOL UGameUISceneClient::DebugInputKey( FName Key, EInputEvent Event )
{
	UBOOL bResult = FALSE;

	return bResult;
}


#if WITH_GFx
/**
 * @return	TRUE if the scene meets the conditions defined by the bitmask specified.
 */
UBOOL UGameUISceneClient::GFxMovieMatchesFilter( DWORD FilterFlagMask, FGFxMovie* TestMovie ) const
{
	checkSlow(TestMovie);

	if ( (FilterFlagMask&SCENEFILTER_Any) == SCENEFILTER_Any )
	{
		return TRUE;
	}

	if ( (FilterFlagMask&SCENEFILTER_PausersOnly) != 0 && !TestMovie->pUMovie->bPauseGameWhileActive )
	{
		return FALSE;
	}

	if ( (FilterFlagMask&SCENEFILTER_ReceivesFocus) != 0 && !TestMovie->pUMovie->bAllowFocus )
	{
		return FALSE;
	}

	if ( (FilterFlagMask&SCENEFILTER_InputProcessorOnly) != 0 && !TestMovie->pUMovie->bCaptureInput )
	{
		return FALSE;
	}

	//This is GFX, these options are not used, so of course they should return false
	if ( (FilterFlagMask&SCENEFILTER_PrimitiveUsersOnly) != 0 ||
		 (FilterFlagMask&SCENEFILTER_IncludeTransient) != 0 ||
		 (FilterFlagMask&SCENEFILTER_UsesPostProcessing) != 0)
	{
		return FALSE;
	}

	return TRUE;
}
#endif //WITH_GFx

/**
 * Returns true if there is an unhidden fullscreen UI active
 *
 * @param	Flags	modifies the logic which determines whether the UI is active
 *
 * @return TRUE if the UI is currently active
 */
UBOOL UGameUISceneClient::IsUIActive( DWORD Flags/*=SCENEFILTER_Any*/ ) const
{
	UBOOL bResult = FALSE;

#if WITH_GFx
    if (GGFxEngine)
    {
	    for (INT SceneIndex = 0; SceneIndex < GGFxEngine->OpenMovies.Num() ; SceneIndex++)
	    {
		    if ( GFxMovieMatchesFilter(Flags, GGFxEngine->OpenMovies(SceneIndex)) )
		    {
			    bResult = TRUE;
			    break;
		    }
        }
	}
#endif //WITH_GFx

	return bResult;
}

/**
 * Check a key event received by the viewport.
 *
 * @param	Viewport - The viewport which the key event is from.
 * @param	ControllerId - The controller which the key event is from.
 * @param	Key - The name of the key which an event occured for.
 * @param	Event - The type of event which occured.
 * @param	AmountDepressed - For analog keys, the depression percent.
 * @param	bGamepad - input came from gamepad (ie xbox controller)
 *
 * @return	True to consume the key event, false to pass it on.
 */
UBOOL UGameUISceneClient::InputKey(INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed/*=1.f*/,UBOOL bGamepad/*=FALSE*/)
{
	UBOOL bResult = FALSE;

	// first check to see if this is a key-release event we should ignore (see FlushPlayerInput)
	if ( InitialPressedKeys.Num() > 0 && (Event == IE_Repeat || Event == IE_Released) )
	{
		TArray<FName>* PressedPlayerKeys = InitialPressedKeys.Find(ControllerId);
		if ( PressedPlayerKeys != NULL )
		{
			INT KeyIndex = PressedPlayerKeys->FindItemIndex(Key);
			if ( KeyIndex != INDEX_NONE )
			{
				// this key was found in the InitialPressedKeys array for this player, which means that the key was already
				// pressed when the UI began processing input - ignore this key event
				if ( Event == IE_Released )
				{
					// if the player has released the key, remove the key from the list of keys to ignore
					PressedPlayerKeys->Remove(KeyIndex);
				}

				// and swallow this input event
				bResult = TRUE;
			}
		}
	}

	if ( bEnableDebugInput && !bResult && IsUIActive(SCENEFILTER_Any) )
	{
		// see if the input corresponds to a debug command
		bResult = DebugInputKey(Key, Event);
	}


	return bResult || bCaptureUnprocessedInput;
}

/**
 * Check an axis movement received by the viewport.
 *
 * @param	Viewport - The viewport which the axis movement is from.
 * @param	ControllerId - The controller which the axis movement is from.
 * @param	Key - The name of the axis which moved.
 * @param	Delta - The axis movement delta.
 * @param	DeltaTime - The time since the last axis update.
 *
 * @return	True to consume the axis movement, false to pass it on.
 */
UBOOL UGameUISceneClient::InputAxis( INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad )
{
	UBOOL bResult = FALSE;

	return bResult || bCaptureUnprocessedInput;
}

/**
 * Check a character input received by the viewport.
 *
 * @param	Viewport - The viewport which the axis movement is from.
 * @param	ControllerId - The controller which the axis movement is from.
 * @param	Character - The character.
 *
 * @return	True to consume the character, false to pass it on.
 */
UBOOL UGameUISceneClient::InputChar(INT ControllerId,TCHAR Character)
{
	UBOOL bResult = FALSE;

	return bResult || bCaptureUnprocessedInput;
}

/* ==========================================================================================================
	FUIRangeData
========================================================================================================== */
UBOOL FUIRangeData::operator==( const FUIRangeData& Other ) const
{
	UBOOL bResult = FALSE;
	if ( bIntRange )
	{
		bResult 
			=	Other.bIntRange
			&&	appRound(CurrentValue)	== appRound(Other.CurrentValue)
			&&	appRound(MinValue)		== appRound(Other.MinValue)
			&&	appRound(MaxValue)		== appRound(Other.MaxValue)
			&&	appRound(NudgeValue)	== appRound(Other.NudgeValue);
	}
	else
	{
		bResult
			=	!Other.bIntRange
			&&	ARE_FLOATS_EQUAL(CurrentValue, Other.CurrentValue)
			&&	ARE_FLOATS_EQUAL(MinValue, Other.MinValue)
			&&	ARE_FLOATS_EQUAL(MaxValue, Other.MaxValue)
			&&	ARE_FLOATS_EQUAL(NudgeValue, Other.NudgeValue);
	}
	return bResult;
}
UBOOL FUIRangeData::operator!=( const FUIRangeData& Other ) const
{
	return !(FUIRangeData::operator==(Other));
}

/**
 * Returns true if any values in this struct are non-zero.
 */
UBOOL FUIRangeData::HasValue() const
{
	return CurrentValue != 0 || MinValue != 0 || MaxValue != 0 || NudgeValue != 0 || bIntRange == TRUE;
}

/**
 * Returns the amount that this range should be incremented/decremented when nudging.
 */
FLOAT FUIRangeData::GetNudgeValue() const
{
	FLOAT Result = NudgeValue;

	// if NudgeValue is 0, nudge the value by 1% of the slider's total range
	if ( Result == 0.f )
	{
		Result = (MaxValue - MinValue) * 0.01;
	}

	return Result;
}

/**
 * Returns the current value of this UIRange.
 */
FLOAT FUIRangeData::GetCurrentValue() const
{
	return (bIntRange == TRUE) ? appRound(CurrentValue) : CurrentValue;
}

/**
 * Sets the value of this UIRange.
 *
 * @param	NewValue				the new value to assign to this UIRange.
 * @param	bClampInvalidValues		specify TRUE to automatically clamp NewValue to a valid value for this UIRange.
 *
 * @return	TRUE if the value was successfully assigned.  FALSE if NewValue was outside the valid range and
 *			bClampInvalidValues was FALSE or MinValue <= MaxValue.
 */
UBOOL FUIRangeData::SetCurrentValue( FLOAT NewValue, UBOOL bClampInvalidValues/*=TRUE*/ )
{
	UBOOL bResult = FALSE;

	if ( bClampInvalidValues == TRUE && MaxValue > MinValue )
	{
		NewValue = Clamp<FLOAT>(NewValue, MinValue, MaxValue);
	}

	if ( bIntRange == TRUE )
	{
		NewValue = appRound(NewValue);
	}

	if ( NewValue >= MinValue && NewValue <= MaxValue )
	{
		CurrentValue = NewValue;
		bResult = TRUE;
	}

	return bResult;
}

/* ==========================================================================================================
	FInputEventParameters
========================================================================================================== */
/** Default constructor */
FInputEventParameters::FInputEventParameters()
: PlayerIndex(INDEX_NONE)
, InputKeyName(NAME_None)
, EventType(IE_MAX)
, InputDelta(0.f)
, DeltaTime(0.f)
, bAltPressed(FALSE)
, bCtrlPressed(FALSE)
, bShiftPressed(FALSE)
{
}

/** Input Key Event constructor */
FInputEventParameters::FInputEventParameters( INT InPlayerIndex, INT InControllerId, FName KeyName, EInputEvent Event, UBOOL bAlt, UBOOL bCtrl, UBOOL bShift, FLOAT AmountDepressed/*=1.f*/ )
: PlayerIndex(InPlayerIndex)
, ControllerId(InControllerId)
, InputKeyName(KeyName)
, EventType(Event)
, InputDelta(AmountDepressed)
, DeltaTime(0.f)
, bAltPressed(bAlt)
, bCtrlPressed(bCtrl)
, bShiftPressed(bShift)
{
}

/** Input Axis Event constructor */
FInputEventParameters::FInputEventParameters( INT InPlayerIndex, INT InControllerId, FName KeyName, FLOAT AxisAmount, FLOAT InDeltaTime, UBOOL bAlt, UBOOL bCtrl, UBOOL bShift )
: PlayerIndex(InPlayerIndex)
, ControllerId(InControllerId)
, InputKeyName(KeyName)
, EventType(IE_Axis)
, InputDelta(AxisAmount)
, DeltaTime(InDeltaTime)
, bAltPressed(bAlt)
, bCtrlPressed(bCtrl)
, bShiftPressed(bShift)
{
}

/* ==========================================================================================================
	FSubscribedInputEventParameters
========================================================================================================== */
/** Default constructor */
FSubscribedInputEventParameters::FSubscribedInputEventParameters() : FInputEventParameters(), InputAliasName(NAME_None)
{
}

/** Input Key Event constructor */
FSubscribedInputEventParameters::FSubscribedInputEventParameters( INT InPlayerIndex, INT InControllerId, FName KeyName, EInputEvent Event, FName InInputAliasName, UBOOL bAlt, UBOOL bCtrl, UBOOL bShift, FLOAT AmountDepressed/*=1.f*/ )
: FInputEventParameters(InPlayerIndex, InControllerId, KeyName, Event, bAlt, bCtrl, bShift, AmountDepressed), InputAliasName(InInputAliasName)
{
}

/** Input Axis Event constructor */
FSubscribedInputEventParameters::FSubscribedInputEventParameters( INT InPlayerIndex, INT InControllerId, FName KeyName, FName InInputAliasName, FLOAT AxisAmount, FLOAT InDeltaTime, UBOOL bAlt, UBOOL bCtrl, UBOOL bShift )
: FInputEventParameters(InPlayerIndex, InControllerId, KeyName, AxisAmount, InDeltaTime, bAlt, bCtrl, bShift), InputAliasName(InInputAliasName)
{
}

/** Copy constructor */
FSubscribedInputEventParameters::FSubscribedInputEventParameters( const FSubscribedInputEventParameters& Other )
: FInputEventParameters((const FInputEventParameters&)Other), InputAliasName(Other.InputAliasName)
{
}
FSubscribedInputEventParameters::FSubscribedInputEventParameters( const FInputEventParameters& Other, FName InInputAliasName )
: FInputEventParameters(Other), InputAliasName(InInputAliasName)
{
}

/* ==========================================================================================================
	FInputKeyAction
========================================================================================================== */
/** Copy constructor */
FInputKeyAction::FInputKeyAction( const FInputKeyAction& Other )
: InputKeyName(Other.InputKeyName), InputKeyState(Other.InputKeyState), TriggeredOps(Other.TriggeredOps)
{
	appMemzero(&ActionsToExecute_DEPRECATED, sizeof(ActionsToExecute_DEPRECATED));
}
UBOOL FInputKeyAction::operator ==( const FInputKeyAction& Other ) const
{
	return
		Other.InputKeyName == InputKeyName &&
		Other.InputKeyState == InputKeyState;
}

/** Serialization operator */
FArchive& operator<<(FArchive& Ar,FInputKeyAction& MyInputKeyAction)
{
	Ar << MyInputKeyAction.InputKeyName << MyInputKeyAction.InputKeyState;
	if ( Ar.IsLoading() && Ar.Ver() < VER_MADE_INPUTKEYACTION_OUTPUT_LINKS )
	{
		Ar << MyInputKeyAction.ActionsToExecute_DEPRECATED;
		MyInputKeyAction.TriggeredOps.Empty(MyInputKeyAction.ActionsToExecute_DEPRECATED.Num());
		for ( INT Idx = 0; Idx < MyInputKeyAction.ActionsToExecute_DEPRECATED.Num(); Idx++ )
		{
			new(MyInputKeyAction.TriggeredOps) FSeqOpOutputInputLink(MyInputKeyAction.ActionsToExecute_DEPRECATED(Idx));
		}
	}
	else
	{
		Ar << MyInputKeyAction.TriggeredOps;
	}

	return Ar;
}

UBOOL FInputKeyAction::IsLinkedTo( const USequenceOp* CheckOp ) const
{
	UBOOL bResult = FALSE;

	if ( CheckOp != NULL )
	{
		for ( INT OpIndex = 0; OpIndex < TriggeredOps.Num(); OpIndex++ )
		{
			const FSeqOpOutputInputLink& OpLink = TriggeredOps(OpIndex);
			if ( OpLink.LinkedOp == CheckOp )
			{
				bResult = TRUE;
				break;
			}
		}
	}
	return bResult;
}

//exec wrapper
void UUIManager::execGetUIManager( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(UUIManager**)Result=GetUIManager();
}
/**
 * Returns the game's UIManager
 *
 * @return 	a pointer to the UUIManager instance currently managing the scenes for the UI System.
 */
UUIManager* UUIManager::GetUIManager()
{
	UUIInteraction* UIController = NULL;
	UUIManager* UIManager = NULL;

	if ( GEngine != NULL && GEngine->GameViewport != NULL )
	{
		UIController = GEngine->GameViewport->UIController;
	}
	if ( UIController != NULL )
	{
		UIManager = UIController->UIManager;
	}
	return UIManager;
}


/**
 * Callback which allows the UI to prevent unpausing if scenes which require pausing are still active.
 * @see PlayerController.SetPause
 */
UBOOL UUIManager::CanUnpauseInternalUI()
{
	UBOOL bResult = TRUE;
	
#if WITH_GFx
	INT SceneIndex;
	FGFxEngine* GFxEngine = FGFxEngine::GetEngine();
	for (SceneIndex = 0; SceneIndex < GFxEngine->OpenMovies.Num() ; SceneIndex++)
	{
		if ( GFxEngine->OpenMovies(SceneIndex)->pUMovie->bPauseGameWhileActive )
		{
			bResult = FALSE;
			break;
		}
	}
#endif //WITH_GFx

	return bResult;
}