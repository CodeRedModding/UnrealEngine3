/**
 * Controls the UI system.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIInteraction extends Interaction
	within GameViewportClient
	native(UserInterface)
	config(UI)
	transient
	inherits(FExec,FGlobalDataStoreClientManager,FCallbackEventDevice);

/** The UI Manager - Acts as the interface between the UIInteraction and the active scenes */
var UIManager UIManager;

/** The class of UIManager to instantiate */
var class<UIManager> UIManagerClass;

/** the class to use for the scene client */
var											class<GameUISceneClient>				SceneClientClass;

/**
 * Acts as the interface between the UIInteraction and the active scenes.
 */
var const transient							GameUISceneClient						SceneClient;

/** list of keys that can trigger double-click events */
var	transient								array<name>								SupportedDoubleClickKeys;

/**
 * Manages all persistent global data stores.  Created when UIInteraction is initialized using the value of
 * GEngine.DataStoreClientClass.
 */
var	const transient private{private}		DataStoreClient							DataStoreManager;

/**
 * Indicates whether there are any active scenes capable of processing input.  Set in UpdateInputProcessingStatus, based
 * on whether there are any active scenes which are capable of processing input.
 */
var	const	transient						bool									bProcessInput;

/**
 * The amount of movement required before the UI will process a joystick's axis input.
 */
var	const	config							float									UIJoystickDeadZone;

/**
 * Mouse & joystick axis input will be multiplied by this amount in the UI system.  Higher values make the cursor move faster.
 */
var	const	config							float									UIAxisMultiplier;

/**
 * The amount of time (in seconds) to wait between generating simulated button presses from axis input.
 */
var	const	config							float									AxisRepeatDelay;

/**
 * The amount of time (in seconds) to wait between generating repeat events for mouse buttons (which are not handled by windows).
 */
var	const	config							float									MouseButtonRepeatDelay;

/**
 * The maximum amount of time (in seconds) that can pass between a key press and key release in order to trigger a double-click event
 */
var	const	config							float									DoubleClickTriggerSeconds;

/**
 * The maximum number of pixels to allow between the current mouse position and the last click's mouse position for a double-click
 * event to be triggered
 */
var	const	config							int										DoubleClickPixelTolerance;

/**
 * Tracks information relevant to simulating IE_Repeat input events.
 */
struct native transient UIKeyRepeatData
{
	/**
	 * The name of the axis input key that is currently being held.  Used to determine which type of input event
	 * to simulate (i.e. IE_Pressed, IE_Released, IE_Repeat)
	 */
	var	name	CurrentRepeatKey;

	/**
	 * The time (in seconds since the process started) when the next simulated input event will be generated.
	 */
	var	double	NextRepeatTime;

structcpptext
{
    /** Constructors */
	FUIKeyRepeatData()
	: CurrentRepeatKey(NAME_None)
	, NextRepeatTime(0.f)
	{}
}
};

/**
 * Contains parameters for emulating button presses using axis input.
 */
struct native transient UIAxisEmulationData extends UIKeyRepeatData
{
	/**
	 * Determines whether to emulate button presses.
	 */
	var	bool	bEnabled;

structcpptext
{
    /** Constructors */
	FUIAxisEmulationData()
	: FUIKeyRepeatData(), bEnabled(TRUE)
	{}

	/**
	 * Toggles whether this axis emulation is enabled.
	 */
	void EnableAxisEmulation( UBOOL bShouldEnable )
	{
		if ( bEnabled != bShouldEnable )
		{
			bEnabled = bShouldEnable;
			CurrentRepeatKey = NAME_None;
			NextRepeatTime = 0.f;
		}
	}
}
};

/**
 * Tracks the mouse button that is currently being held down for simulating repeat input events.
 */
var	const			transient		UIKeyRepeatData									MouseButtonRepeatInfo;


/**
 * Default button press emulation definitions for gamepad and joystick axis input keys.
 */
var	const	config							array<UIAxisEmulationDefinition>		ConfiguredAxisEmulationDefinitions;

/**
 * Runtime mapping of the axis button-press emulation configurations.  Built in UIInteraction::InitializeAxisInputEmulations() based
 * on the values retrieved from ConfiguredAxisEmulationDefinitions.
 */
var	const	native	transient		Map{FName,struct FUIAxisEmulationDefinition}	AxisEmulationDefinitions;

/**
 * Tracks the axis key-press emulation data for all players in the game.
 */
var					transient		UIAxisEmulationData								AxisInputEmulation[MAX_SUPPORTED_GAMEPADS];

cpptext
{
	/* =======================================
		UObject interface
	======================================= */
	/**
	* Called to finish destroying the object.
	*/
	virtual void FinishDestroy();

	/* =======================================
		FExec interface
	======================================= */
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	/* === FCallbackEventDevice interface === */
	/**
	 * Called for notifications that require no additional information.
	 */
	virtual void Send( ECallbackEventType InType );

	/**
	 * Called when the viewport has been resized.
	 */
	virtual void Send( ECallbackEventType InType, FViewport* InViewport, UINT InMessage);

	/* ==============================================
		FGlobalDataStoreClientManager interface
	============================================== */
	/**
	 * Initializes the singleton data store client that will manage the global data stores.
	 */
	virtual void InitializeGlobalDataStore();

	/* =======================================
		UInteraction interface
	======================================= */
	/**
	 * Called when UIInteraction is added to the GameViewportClient's Interactions array
	 */
	virtual void Init();

	/**
	 * Called once a frame to update the interaction's state.
	 *
	 * @param	DeltaTime - The time since the last frame.
	 */
	virtual void Tick(FLOAT DeltaTime);

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
		UUIInteraction interface
	======================================= */
	/**
	 * Constructor
	 */
	UUIInteraction();

	/**
	 * Cleans up all objects created by this UIInteraction, including unrooting objects and unreferencing any other objects.
	 * Called when the UI system is being closed down (such as when exiting PIE).
	 */
	virtual void TearDownUI();

	/**
	 * Initializes the axis button-press/release emulation map.
	 */
	void InitializeAxisInputEmulations();

	/**
	 * Initializes all of the UI input alias names.
	 */
	void InitializeUIInputAliasNames();

	/**
	 * Returns the CDO for the configured scene client class.
	 */
	class UGameUISceneClient* GetDefaultSceneClient() const;

	/**
	 * Returns the number of players currently active.
	 */
	static INT GetPlayerCount();

	/**
	 * Retrieves the index (into the Engine.GamePlayers array) for the player which has the ControllerId specified
	 *
	 * @param	ControllerId	the gamepad index of the player to search for
	 *
	 * @return	the index [into the Engine.GamePlayers array] for the player that has the ControllerId specified, or INDEX_NONE
	 *			if no players have that ControllerId
	 */
	static INT GetPlayerIndex( INT ControllerId );

	/**
	 * Returns the index [into the Engine.GamePlayers array] for the player specified.
	 *
	 * @param	Player	the player to search for
	 *
	 * @return	the index of the player specified, or INDEX_NONE if the player is not in the game's list of active players.
	 */
	static INT GetPlayerIndex( class ULocalPlayer* Player );

	/**
	 * Retrieves the ControllerId for the player specified.
	 *
	 * @param	PlayerIndex		the index [into the Engine.GamePlayers array] for the player to retrieve the ControllerId for
	 *
	 * @return	the ControllerId for the player at the specified index in the GamePlayers array, or INDEX_NONE if the index is invalid
	 */
	static INT GetPlayerControllerId( INT PlayerIndex );

	/**
	 * Returns TRUE if button press/release events should be emulated for the specified axis input.
	 *
	 * @param	AxisKeyName		the name of the axis key that
	 */
	static UBOOL ShouldEmulateKeyPressForAxis( const FName& AxisKeyName );

	/**
	 * Returns a reference to the global data store client, if it exists.
	 *
	 * @return	the global data store client for the game.
	 */
	static class UDataStoreClient* GetDataStoreClient();
}

/**
 * Returns the number of players currently active.
 */
static native noexportheader final function int GetPlayerCount() const;

/**
 * Retrieves the index (into the Engine.GamePlayers array) for the player which has the ControllerId specified
 *
 * @param	ControllerId	the gamepad index of the player to search for
 *
 * @return	the index [into the Engine.GamePlayers array] for the player that has the ControllerId specified, or INDEX_NONE
 *			if no players have that ControllerId
 */
static native noexportheader final function int GetPlayerIndex( int ControllerId );

/**
 * Retrieves the ControllerId for the player specified.
 *
 * @param	PlayerIndex		the index [into the Engine.GamePlayers array] for the player to retrieve the ControllerId for
 *
 * @return	the ControllerId for the player at the specified index in the GamePlayers array, or INDEX_NONE if the index is invalid
 */
static native noexportheader final function int GetPlayerControllerId( int PlayerIndex );

/**
 * Returns a reference to the global data store client, if it exists.
 *
 * @return	the global data store client for the game.
 */
static native noexportheader final function DataStoreClient GetDataStoreClient();

/**
 * Wrapper for retrieving a LocalPlayer reference for one of the players in the GamePlayers array.
 *
 * @param	PlayerIndex		the index of the player reference to retrieve.
 *
 * @return	a reference to the LocalPlayer object at the specified index in the Engine's GamePlayers array, or None if the index isn't valid.
 */
static final function LocalPlayer GetLocalPlayer( int PlayerIndex )
{
	local UIInteraction CurrentUIController;
	local LocalPlayer Result;

	CurrentUIController = class'UIRoot'.static.GetCurrentUIController();
	if ( CurrentUIController != None && PlayerIndex >= 0 && PlayerIndex < CurrentUIController.Outer.Outer.GamePlayers.Length )
	{
		Result = CurrentUIController.Outer.Outer.GamePlayers[PlayerIndex];
	}

	return Result;
}

/**
 * Called when a new player has been added to the list of active players (i.e. split-screen join)
 *
 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was inserted
 * @param	AddedPlayer		the player that was added
 */
function NotifyPlayerAdded( int PlayerIndex, LocalPlayer AddedPlayer )
{
	local UIAxisEmulationData Empty;

	// make sure the axis emulation data for this player has been reset
	if ( PlayerIndex >=0 && PlayerIndex < MAX_SUPPORTED_GAMEPADS )
	{
		Empty.CurrentRepeatKey = 'None';
		AxisInputEmulation[PlayerIndex] = Empty;
	}

	if ( SceneClient != None )
	{
		SceneClient.NotifyPlayerAdded(PlayerIndex, AddedPlayer);
	}

	if (UIManager != none)
	{
		UIManager.NotifyPlayerAdded(PlayerIndex, AddedPlayer);
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
	local int PlayerCount, NextPlayerIndex, i;
	local UIAxisEmulationData Empty;


	// clear the axis emulation data for this player
	if ( PlayerIndex >=0 && PlayerIndex < MAX_SUPPORTED_GAMEPADS )
	{
		// if we removed a player from the middle of the list, we need to migrate all of the axis emulation data from
		// that player's previous slot into the new slot
		PlayerCount = GetPlayerCount();

		// PlayerCount has to be less that MAX_SUPPORTED_GAMEPADS if we just removed a player; if it does not, it means
		// that someone changed the order in which NotifyPlayerRemoved is called so that the player is actually removed from
		// the array after calling NotifyPlayerRemoved.  If that happens, this assertion is here to ensure that this code is
		// updated as well.

		// we removed a player that was in a middle slot - migrate the data for all subsequence players into the correct position
		for ( i = PlayerIndex; i < PlayerCount; i++ )
		{
			NextPlayerIndex = i + 1;
			AxisInputEmulation[i].NextRepeatTime = AxisInputEmulation[NextPlayerIndex].NextRepeatTime;
			AxisInputEmulation[i].CurrentRepeatKey = AxisInputEmulation[NextPlayerIndex].CurrentRepeatKey;
			AxisInputEmulation[i].bEnabled = AxisInputEmulation[NextPlayerIndex].bEnabled;
		}

		Empty.CurrentRepeatKey = 'None';
		AxisInputEmulation[PlayerCount] = Empty;
	}

	if ( SceneClient != None )
	{
		SceneClient.NotifyPlayerRemoved(PlayerIndex, RemovedPlayer);
	}

	if (UIManager != none)
	{
		UIManager.NotifyPlayerRemoved(PlayerIndex, RemovedPlayer);
	}
}

/** @return Returns the current login status for the specified controller id. */
static final event ELoginStatus GetLoginStatus( int ControllerId )
{
	local ELoginStatus Result;
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	Result = LS_NotLoggedIn;

	if ( ControllerId != INDEX_NONE )
	{
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			// Grab the player interface to verify the subsystem supports it
			PlayerInterface = OnlineSub.PlayerInterface;
			if (PlayerInterface != None)
			{
				// Get status
				Result = PlayerInterface.GetLoginStatus(ControllerId);
			}
		}
	}

	return Result;
}

/** @return	the lowest common denominator for the login status of all local players */
final function ELoginStatus GetLowestLoginStatusOfControllers()
{
	local ELoginStatus Result, LoginStatus;
	local int PlayerIndex;

	Result = LS_LoggedIn;

	for( PlayerIndex = 0; PlayerIndex < GamePlayers.Length; PlayerIndex++ )
	{
		LoginStatus = GetLoginStatus( GamePlayers[PlayerIndex].ControllerId );
		if ( LoginStatus < Result )
		{
			Result = LoginStatus;
		}
	}

	return Result;
}

/** @return Returns the current status of the platform's network connection. */
static final event bool HasLinkConnection()
{
	local bool bResult;
	local OnlineSubsystem OnlineSub;
	local OnlineSystemInterface SystemInterface;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		SystemInterface = OnlineSub.SystemInterface;
		if (SystemInterface != None)
		{
			bResult = SystemInterface.HasLinkConnection();
		}
	}

	return bResult;
}

/** @return Returns whether or not the specified player is logged in at all. */
static final event bool IsLoggedIn( int ControllerId, optional bool bRequireOnlineLogin )
{
	local bool bResult;
	local ELoginStatus LoginStatus;

	LoginStatus = GetLoginStatus(ControllerId);

	bResult = (LoginStatus == LS_LoggedIn) || (LoginStatus == LS_UsingLocalProfile && !bRequireOnlineLogin);
	return bResult;
}

/** @return	the number of players signed into the online service */
static final function int GetLoggedInPlayerCount( optional bool bRequireOnlineLogin )
{
	local int ControllerId, Result;

	for ( ControllerId = 0; ControllerId < MAX_SUPPORTED_GAMEPADS; ControllerId++ )
	{
		if ( IsLoggedIn(ControllerId, bRequireOnlineLogin) )
		{
			Result++;
		}
	}

	return Result;
}

/** Returns the number of guests logged in */
static final function int GetNumGuestsLoggedIn()
{
	local OnlineSubsystem OnlineSub;
	local int ControllerId;
	local int GuestCount;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None && OnlineSub.PlayerInterface != none)
	{
		for (ControllerId = 0; ControllerId < MAX_SUPPORTED_GAMEPADS; ControllerId++)
		{
			if (OnlineSub.PlayerInterface.IsGuestLogin(ControllerId))
			{
				GuestCount++;
			}
		}
	}
	return GuestCount;
}

/**
 * Check whether a gamepad is connected and turned on.
 *
 * @param	ControllerId	the id of the gamepad to check
 *
 * @return	TRUE if the gamepad with the specified id is connected.
 */
static final function bool IsGamepadConnected( int ControllerId )
{
	local bool bResult;
	local OnlineSubsystem OnlineSub;
	local OnlineSystemInterface SystemInterface;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		SystemInterface = OnlineSub.SystemInterface;
		if (SystemInterface != None)
		{
			bResult = SystemInterface.IsControllerConnected(ControllerId);
		}
	}

	return bResult;
}

/**
 * @param	ControllerConnectionStatusOverrides		array indicating the connection status of each gamepad; should always contain
 *													MAX_SUPPORTED_GAMEPADS elements; useful when executing code as a result of a controller
 *													insertion/removal notification, as IsControllerConnected isn't reliable in that case.
 *
 * @return	the number of gamepads which are currently connected and turned on.
 */
static final function int GetConnectedGamepadCount( optional array<bool> ControllerConnectionStatusOverrides )
{
	local int i, Result;

	for ( i = 0; i < MAX_SUPPORTED_GAMEPADS; i++ )
	{
		if ( i < ControllerConnectionStatusOverrides.Length )
		{
			if ( ControllerConnectionStatusOverrides[i] )
			{
				Result++;
			}
		}
		else if ( IsGamepadConnected(i) )
		{
			Result++;
		}
	}

	return Result;
}

/**
 * Wrapper for getting the NAT type
 */
static final event ENATType GetNATType()
{
	local OnlineSubsystem OnlineSub;
	local OnlineSystemInterface SystemInterface;
	local ENATType Result;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		SystemInterface = OnlineSub.SystemInterface;
		if (SystemInterface != None)
		{
			Result = SystemInterface.GetNATType();
		}
	}

	return Result;
}

/* === Interaction interface === */

/**
 * Called when the current map is being unloaded.  Cleans up any references which would prevent garbage collection.
 */
function NotifyGameSessionEnded()
{
	// notify the UI first so that all player data stores are still around for their subscribers to publish to.
	if ( SceneClient != None )
	{
		SceneClient.NotifyGameSessionEnded();
	}

	if ( DataStoreManager != None )
	{
		DataStoreManager.NotifyGameSessionEnded();
	}
}

DefaultProperties
{
	SceneClientClass=class'GameUISceneClient'
	UIManagerClass=class'UIManager'
}
