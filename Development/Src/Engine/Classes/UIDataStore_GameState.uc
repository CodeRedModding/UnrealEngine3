/**
 * Tracks all data about the current game state, such as players, objectives, time remaining, current scores, etc. Game data stores
 * can be nested, in that a GameState data store can contain references to other game state data stores. This is useful for
 * isolating the weapon data store associated with a particular player, for example.
 * Game data stores are further divided into two components:
 * <p>
 * Game state data providers:	Provides state and static data about a particular instance of a data source, such as a player, weapon,
 * 								pickup, or game objective. Data providers can generally not be referenced directly by the UI. Instead,
 *								they are normally accessed through a game state data store, such as the game state data store associated
 *								with the owning player, or the current game info instance.
 * Game state data stores:		Acts as the first layer between the game and the UI. Each data store contains a collection of game state
 *								data providers, which provide the data for instances of a game object
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_GameState extends UIDataStore
	native(inherit)
	abstract;

delegate OnRefreshDataFieldValue();

/**
 * Called when the current map is being unloaded.  Cleans up any references which would prevent garbage collection.
 *
 * @return	TRUE indicates that this data store should be automatically unregistered when this game session ends.
 */
function bool NotifyGameSessionEnded()
{
	// game state data stores should always be unregistered when the match is over.
	return true;
}

DefaultProperties
{
}
