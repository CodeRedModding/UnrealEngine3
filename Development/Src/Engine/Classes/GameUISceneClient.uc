/**
 * UISceneClient used when playing a game.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameUISceneClient extends UISceneClient
	within UIInteraction
	native(UIPrivate)
	config(UI);

/** Cached DeltaTime value from the last Tick() call */
var	const	transient							float				LatestDeltaTime;

/** The time (in seconds) that the last "key down" event was recieved from a key that can trigger double-click events */
var	const	transient							double				DoubleClickStartTime;

/**
 * The location of the mouse the last time a key press was received.  Used to determine when to simulate a double-click
 * event.
 */
var const	transient							IntPoint			DoubleClickStartPosition;

/**
 * map of controllerID to list of keys which were pressed when the UI began processing input
 * used to ignore the initial "release" key event from keys which were already pressed when the UI began processing input.
 */
var	const	transient	native					Map_Mirror			InitialPressedKeys{TMap<INT,TArray<FName> >};

/**
 * Indicates that the input processing status of the UI has potentially changed; causes UpdateInputProcessingStatus to be called
 * in the next Tick().
 */
var	const	transient							bool				bUpdateInputProcessingStatus;

/**
 * Indicates that the viewport size being used by one or more scenes is out of date; triggers a call to NotifyViewportResized during the
 * next tick.
 */
var			transient							bool				bUpdateSceneViewportSizes;

/** Controls whether debug input commands are accepted */
var			config								bool				bEnableDebugInput;
/** Controls whether debug information about the scene is rendered */
var			config								bool				bRenderDebugInfo;

/**
 * Controls whether the UI system should prevent the game from recieving input whenever it's active.  For games with
 * interactive menus that remain on-screen during gameplay, you'll want to change this value to FALSE.
 */
var	const	config								bool				bCaptureUnprocessedInput;

/** The list of navigation aliases to check input support for */
var const transient array<name> NavAliases;

/** The list of axis input keys to check input support for */
var const transient array<name> AxisInputKeys;

cpptext
{
	/* =======================================
		FExec interface
	======================================= */
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	/* =======================================
		UUISceneClient interface
	======================================= */
	/**
	 * Called when the UI controller receives a CALLBACK_ViewportResized notification.
	 *
	 * @param	SceneViewport	the viewport that was resized
	 */
	virtual void NotifyViewportResized( FViewport* SceneViewport );

	/**
	 * Process an input event which interacts with the in-game scene debugging overlays
	 *
	 * @param	Key		the key that was pressed
	 * @param	Event	the type of event received
	 *
	 * @return	TRUE if the input event was processed; FALSE otherwise.
	 */
	UBOOL DebugInputKey( FName Key, EInputEvent Event );

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
	virtual UBOOL InputKey(INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed=1.f,UBOOL bGamepad=FALSE);

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
	virtual UBOOL InputAxis(INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad=FALSE);

	/**
	 * Check a character input received by the viewport.
	 *
	 * @param	Viewport - The viewport which the axis movement is from.
	 * @param	ControllerId - The controller which the axis movement is from.
	 * @param	Character - The character.
	 *
	 * @return	True to consume the character, false to pass it on.
	 */
	virtual UBOOL InputChar(INT ControllerId,TCHAR Character);

	/* =======================================
		UGameUISceneClient interface
	======================================= */

	/**
	 * Resets the time and mouse position values used for simulating double-click events to the current value or invalid values.
	 */
	void ResetDoubleClickTracking( UBOOL bClearValues );

	/**
	 * Checks the current time and mouse position to determine whether a double-click event should be simulated.
	 */
	UBOOL ShouldSimulateDoubleClick() const;

	/**
	 * Determines whether the any active scenes process axis input.
	 *
	 * @param	bProcessAxisInput	receives the flags for whether axis input is needed for each player.
	 */
	virtual void CheckAxisInputSupport( UBOOL* bProcessAxisInput[UCONST_MAX_SUPPORTED_GAMEPADS] ) const;

	/**
	 * Called once a frame to update the UI's state.
	 *
	 * @param	DeltaTime - The time since the last frame.
	 */
	virtual void Tick(FLOAT DeltaTime);

private:

	#if WITH_GFx
	/**
	 * @return	TRUE if the scene meets the conditions defined by the bitmask specified.
	 */
	UBOOL GFxMovieMatchesFilter( DWORD FilterFlagMask, class FGFxMovie* TestMovie ) const;
	#endif //WITH_GFx
public:
	/**
	 * Returns true if there is an unhidden fullscreen UI active
	 *
	 * @param	Flags	modifies the logic which determines whether the UI is active
	 *
	 * @return TRUE if the UI is currently active
	 */
	virtual UBOOL IsUIActive( DWORD Flags=SCENEFILTER_Any ) const;

protected:

	/**
	 * Updates the value of UIInteraction.bProcessingInput to reflect whether any scenes are capable of processing input.
	 */
	void UpdateInputProcessingStatus();

	/**
	 * Clears the arrays of pressed keys for all local players in the game; used when the UI begins processing input.  Also
	 * updates the InitialPressedKeys maps for all players.
	 */
	void FlushPlayerInput();

public:
	/**
	 * Ensures that the game's paused state is appropriate considering the state of the UI.  If any scenes are active which require
	 * the game to be paused, pauses the game...otherwise, unpauses the game.
	 *
	 * @param	PlayerIndex		the index of the player that owns the scene that was just added or removed, or 0 if the scene didn't have
	 *							a player owner.
	 */
	virtual void UpdatePausedState( INT PlayerIndex );
}

/* == Delegates == */

/* == Natives == */
/**
 * @return	the current netmode, or NM_MAX if there is no valid world
 */
native static final function WorldInfo.ENetMode GetCurrentNetMode();

/**
 * Triggers a call to UpdateInputProcessingStatus on the next Tick().
 */
native final function RequestInputProcessingUpdate();

/**
 * Callback which allows the UI to prevent unpausing if scenes which require pausing are still active.
 * @see PlayerController.SetPause
 */
native final function bool CanUnpauseInternalUI();

/* == Events == */

/**
 * Wrapper for pausing the game.
 *
 * @param	bDesiredPauseState	TRUE indicates that the game should be paused.
 * @param	PlayerIndex			the index [into Engine GamePlayers array] for the player that should be used for pausing the game; can
 *								affect whether the game is actually paused or not (i.e. if the player is an admin in a multi-player match,
 *								for example).
 */
event PauseGame( bool bDesiredPauseState, optional int PlayerIndex=0 )
{
	local PlayerController PlayerOwner;

	if ( GamePlayers.Length > 0 )
	{
		PlayerIndex = Clamp(PlayerIndex, 0, GamePlayers.Length - 1);
		PlayerOwner = GamePlayers[PlayerIndex].Actor;
		if ( PlayerOwner != None )
		{
			PlayerOwner.SetPause(bDesiredPauseState, CanUnpauseInternalUI);
		}
	}
}

/**
 * Called when the local player is about to travel to a new URL.  This callback should be used to perform any preparation
 * tasks, such as updating status text and such.  All cleanup should be done from NotifyGameSessionEnded, as that function
 * will be called in some cases where NotifyClientTravel is not.
 *
 * @param	TravellingPlayer	the player that received the call to ClientTravel
 * @param	TravelURL			a string containing the mapname (or IP address) to travel to, along with option key/value pairs
 * @param	TravelType			indicates whether the player will clear previously added URL options or not.
 * @param	bIsSeamlessTravel	indicates whether seamless travelling will be used.
 */
function NotifyClientTravel( PlayerController TravellingPlayer, string TravelURL, ETravelType TravelType, bool bIsSeamlessTravel );

/**
 * Called when the current map is being unloaded.  Cleans up any references which would prevent garbage collection.
 */
function NotifyGameSessionEnded();

/**
 * Called when a new player has been added to the list of active players (i.e. split-screen join)
 *
 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was inserted
 * @param	AddedPlayer		the player that was added
 */
function NotifyPlayerAdded( int PlayerIndex, LocalPlayer AddedPlayer )
{
	if ( IsUIActive(SCENEFILTER_InputProcessorOnly) )
	{
		RequestInputProcessingUpdate();
	}
}

/**
 * Called when a player has been removed from the list of active players (i.e. split-screen players)
 *
 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was located
 * @param	RemovedPlayer	the player that was removed
 */
function NotifyPlayerRemoved( int PlayerIndex, LocalPlayer RemovedPlayer )
{
	if ( IsUIActive(SCENEFILTER_InputProcessorOnly) )
	{
		RequestInputProcessingUpdate();
	}
}

/**
 * Helper function to deduce the PlayerIndex of a Player
 * 
 * @param P - The LocalPlayer for whom you wish to deduce their PlayerIndex
 * 
 * @return Returns the index into the GamePlayers array that references this Player. If it cannot find the player, it returns 0.
 */
function int FindLocalPlayerIndex(Player P)
{
	local Engine Engine;
	local int i;

	Engine = class'Engine'.static.GetEngine();
	for (i = 0; i < Engine.GamePlayers.length; i++)
	{
		if (Engine.GamePlayers[i] == P)
		{
			return i;
		}
	}
	return 0;
}

DefaultProperties
{
	NavAliases(0)="UIKEY_NavFocusUp"
	NavAliases(1)="UIKEY_NavFocusDown"
	NavAliases(2)="UIKEY_NavFocusLeft"
	NavAliases(3)="UIKEY_NavFocusRight"

	AxisInputKeys(0)="KEY_Gamepad_LeftStick_Up"
	AxisInputKeys(1)="KEY_Gamepad_LeftStick_Down"
	AxisInputKeys(2)="KEY_Gamepad_LeftStick_Right"
	AxisInputKeys(3)="KEY_Gamepad_LeftStick_Left"
	AxisInputKeys(4)="KEY_Gamepad_RightStick_Up"
	AxisInputKeys(5)="KEY_Gamepad_RightStick_Down"
	AxisInputKeys(6)="KEY_Gamepad_RightStick_Right"
	AxisInputKeys(7)="KEY_Gamepad_RightStick_Left"
	AxisInputKeys(8)="KEY_SIXAXIS_AccelX"
	AxisInputKeys(9)="KEY_SIXAXIS_AccelY"
	AxisInputKeys(10)="KEY_SIXAXIS_AccelZ"
	AxisInputKeys(11)="KEY_SIXAXIS_Gyro"
	AxisInputKeys(12)="KEY_XboxTypeS_LeftX"
	AxisInputKeys(13)="KEY_XboxTypeS_LeftY"
	AxisInputKeys(14)="KEY_XboxTypeS_RightX"
	AxisInputKeys(15)="KEY_XboxTypeS_RightY"
}
