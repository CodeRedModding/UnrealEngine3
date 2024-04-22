/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for mapping properties in an OnlineGameSettings
 * object to something that the UI system can consume.
 *
 * NOTE: Each game needs to derive at least one class from this one in
 * order to expose the game's specific settings class(es)
 */
class UIDataStore_OnlineGameSettings extends UIDataStore_Settings
	native(inherit)
	abstract
	transient;

/** Holds the information used to expose 1 or more game settings providers */
struct native GameSettingsCfg
{
	/** The OnlineGameSettings derived class to create and expose */
	var class<OnlineGameSettings> GameSettingsClass;
	/** The provider that was created to process the specified game settings object */
	var UIDataProvider_Settings Provider;
	/** The object we are exposing to the UI */
	var OnlineGameSettings GameSettings;
	/** Used to set/select by name */
	var name SettingsName;
};

/** The list of settings that this data store is exposing */
var const array<GameSettingsCfg> GameSettingsCfgList;

/** the class to use for creating the data provider for each game settings object */
var	const	class<UIDataProvider_Settings>	SettingsProviderClass;

/** The index into the list that is currently being exposed */
var int SelectedIndex;

cpptext
{
private:
	/**
	 * Loads and creates an instance of the registered provider objects for each
	 * registered OnlineGameSettings class
	 */
	virtual void InitializeDataStore(void);

}

/**
 * Called to kick create an online game based upon the settings
 *
 * @param WorldInfo the world info object. useful for actor iterators
 * @param ControllerIndex the ControllerId for the player to create the game for
 *
 * @return TRUE if the game was created, FALSE otherwise
 */
event bool CreateGame( byte ControllerIndex )
{
	local OnlineSubsystem OnlineSub;
	local OnlineGameInterface GameInterface;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// Grab the game interface to verify the subsystem supports it
		GameInterface = OnlineSub.GameInterface;
		if (GameInterface != None)
		{
			// Start the async task
			return GameInterface.CreateOnlineGame(ControllerIndex,'Game',GameSettingsCfgList[SelectedIndex].GameSettings);
		}
		else
		{
			`warn("OnlineSubsystem does not support the game interface. Can't create online games");
		}
	}
	else
	{
		`warn("No OnlineSubsystem present. Can't create online games");
	}
	return false;
}

/** Returns the game settings object that is currently selected */
event OnlineGameSettings GetCurrentGameSettings()
{
	return GameSettingsCfgList[SelectedIndex].GameSettings;
}

/** Returns the provider object that is currently selected */
event UIDataProvider_Settings GetCurrentProvider()
{
	return GameSettingsCfgList[SelectedIndex].Provider;
}

/**
 * Sets the index into the list of game settings to use
 *
 * @param NewIndex the new index to use
 */
event SetCurrentByIndex(int NewIndex)
{
	// Range check to prevent accessed nones
	if (NewIndex >= 0 && NewIndex < GameSettingsCfgList.Length)
	{
		SelectedIndex = NewIndex;
		// notify any subscribers that we have new data
		RefreshSubscribers( , true, GetCurrentProvider());
	}
	else
	{
		`Log("Invalid index ("$NewIndex$") specified to SetCurrentByIndex() on "$Self);
	}
}

/**
 * Sets the index into the list of game settings to use
 *
 * @param SettingsName the name of the setting to find
 */
event SetCurrentByName(name SettingsName)
{
	local int Index;

	for (Index = 0; Index < GameSettingsCfgList.Length; Index++)
	{
		// If this is the one we want, set it and refresh
		if (GameSettingsCfgList[Index].SettingsName == SettingsName)
		{
			SetCurrentByIndex(Index);
			return;
		}
	}
	`Log("Invalid name ("$SettingsName$") specified to SetCurrentByName() on "$Self);
}

/** Moves to the next item in the list */
event MoveToNext()
{
	local int NewIndex;

	NewIndex = Min(SelectedIndex + 1, GameSettingsCfgList.Length - 1);
	if ( SelectedIndex != NewIndex )
	{
		SetCurrentByIndex(NewIndex);
	}
}

/** Moves to the previous item in the list */
event MoveToPrevious()
{
	local int NewIndex;

	NewIndex = Max(SelectedIndex - 1,0);
	if ( SelectedIndex != NewIndex )
	{
		SetCurrentByIndex(NewIndex);
	}
}

/* === UIDataStore interface === */
/**
 * Called when this data store is added to the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
event Registered( LocalPlayer PlayerOwner )
{
	Super.Registered(PlayerOwner);
}

/**
 * Called when this data store is removed from the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
event Unregistered( LocalPlayer PlayerOwner )
{

	Super.Unregistered(PlayerOwner);
}

defaultproperties
{
	// derived classes should keep the same tag if they are replacing the OnlineGameSettings data store
	Tag=OnlineGameSettings

	SettingsProviderClass=class'Engine.UIDataProvider_Settings'
}
