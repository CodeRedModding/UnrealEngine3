/**
 * Class that manages the UI for the game
 * Replaces UISceneClient
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIManager extends Object
	within UIInteraction
	native;

cpptext
{
	/**
	 * Returns the game's UI Manager.
	 *
	 * @return 	a pointer to the UUIManager instance currently managing the scenes for the UI System.
	 */
	static class UUIManager* GetUIManager();
}

/**
 * Returns the game's UI Manager.
 *
 * @return 	a pointer to the UUIManager instance currently managing the scenes for the UI System.
 */
native static final noexport function UIManager GetUIManager();

/**
 * Callback which allows the UI to prevent unpausing if scenes which require pausing are still active.
 * @see PlayerController.SetPause
 */
native final function bool CanUnpauseInternalUI();

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
	local Engine Eng;

	Eng = class'Engine'.static.GetEngine();

	if ( Eng.GamePlayers.Length > 0 )
	{
		PlayerIndex = Clamp(PlayerIndex, 0, Eng.GamePlayers.Length - 1);
		PlayerOwner = Eng.GamePlayers[PlayerIndex].Actor;
		if ( PlayerOwner != None )
		{
			PlayerOwner.SetPause(bDesiredPauseState, CanUnpauseInternalUI);
		}
	}
}

/**
 * Called when a new player has been added to the list of active players (i.e. split-screen join)
 *
 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was inserted
 * @param	AddedPlayer		the player that was added
 */
function NotifyPlayerAdded( int PlayerIndex, LocalPlayer AddedPlayer )
{
}


/**
 * Called when a player has been removed from the list of active players (i.e. split-screen players)
 *
 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was located
 * @param	RemovedPlayer	the player that was removed
 */
function NotifyPlayerRemoved( int PlayerIndex, LocalPlayer RemovedPlayer )
{
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