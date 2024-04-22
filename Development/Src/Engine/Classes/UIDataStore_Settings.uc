/**
 * Base class for all settings data stores.  Settings data stores provide the UI with access to the game-specific or
 * global configurable settings. A settings data store may expose such data as a gametype's configured MaxPlayers, GoalScore,
 * etc, or user-specific data such as the the user's configured button layout. Settings data stores are also responsible
 * for publishing user selections to the appropriate persistent location so that these values are used by the gameplay code when the
 * game is played
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_Settings extends UIDataStore
	native(inherit)
	config(Game)
	abstract;

DefaultProperties
{
	Tag=Settings
}
