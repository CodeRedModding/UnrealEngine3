/**
 * This class is responsible for providing access to information about the achievements available to the player
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataProvider_PlayerAchievements extends UIDataProvider_OnlinePlayerDataBase
	native(inherit)
	dependson(OnlineSubsystem)
	transient;

var	transient array<AchievementDetails> Achievements;


/**
 * Returns the number of gamer points this profile has accumulated across all achievements
 *
 * @return	a number between 0 and the maximum number of gamer points allocated for each game (currently 1000), representing the total
 * gamer points earned from all achievements for this profile.
 */
native final function int GetTotalGamerScore() const;

/**
* Returns the number of gamer points that can be acquired in this game across all achievements
*
* @return	The maximum number of gamer points allocated for each game.
*/
native final function int GetMaxTotalGamerScore() const;

/**
 * Loads the achievement icons from the .ini and applies them to the list of achievements.
 */
function PopulateAchievementIcons();

/**
 * Wrapper for retrieving the path name of an achievement's icon.
 */
function string GetAchievementIconPathName( int AchievementId, optional bool bReturnLockedIcon );

/**
 * Gets achievement details based on achievement id
 *
 * @param AchievementId	EGearAchievement for which to find details
 * @param OutAchievementDetails	AchievementDetails struct to be populated
 *
 */
function GetAchievementDetails(const int AchievementId, out AchievementDetails OutAchievementDetails)
{
	local int Index;

	Index = Achievements.Find('Id', AchievementId);
	if (Index != INDEX_NONE)
	{
		OutAchievementDetails = Achievements[Index];
	}
}

/**
 * Called when the async achievements read has completed
 *
 * @param TitleId the title id that the read was for (0 means current title)
 */
function OnPlayerAchievementsChanged( int TitleId )
{
	local OnlineSubsystem OnlineSub;
	local EOnlineEnumerationReadState Result;

	if (PlayerControllerId != -1)
	{
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None && OnlineSub.PlayerInterface != None && TitleId == 0)
		{
			Result = OnlineSub.PlayerInterface.GetAchievements(PlayerControllerId, Achievements, TitleId);
			if ( Result == OERS_Done )
			{
				PopulateAchievementIcons();

			}
		}
	}
}

/**
 * Handler for online service's callback for the player unlocking an achievement.
 */
function OnPlayerAchievementUnlocked( bool bWasSuccessful )
{
	if ( bWasSuccessful )
	{
		UpdateAchievements();
	}
}

/**
 * Binds the player to this provider. Starts the async friends list gathering
 *
 * @param InPlayer the player that we are retrieving friends for
 */
event OnRegister(LocalPlayer InPlayer)
{
	local OnlineSubsystem OnlineSub;

	Super.OnRegister(InPlayer);

	// If the player is None, we are in the editor
	if (PlayerControllerId != -1)
	{
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None)
		{
			// Grab the player interface to verify the subsystem supports it
			if (OnlineSub.PlayerInterface != None)
			{
				// Register that we are interested in any sign in change for this player
				OnlineSub.PlayerInterface.AddLoginChangeDelegate(OnLoginChange);
				// Set our callback function per player
				OnlineSub.PlayerInterface.AddReadAchievementsCompleteDelegate(PlayerControllerId, OnPlayerAchievementsChanged);
				OnlineSub.PlayerInterface.AddUnlockAchievementCompleteDelegate(PlayerControllerId, OnPlayerAchievementUnlocked);
				UpdateAchievements();
			}
		}
	}
}

/**
 * Clears our delegate for getting login change notifications
 */
event OnUnregister()
{
	local OnlineSubsystem OnlineSub;

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		if (OnlineSub.PlayerInterface != None)
		{
			// Clear our callback function per player
			OnlineSub.PlayerInterface.ClearLoginChangeDelegate(OnLoginChange);
		}

		// Grab the player interface to verify the subsystem supports it
		if (OnlineSub.PlayerInterface != None)
		{
			OnlineSub.PlayerInterface.ClearUnlockAchievementCompleteDelegate(PlayerControllerId, OnPlayerAchievementUnlocked);
			OnlineSub.PlayerInterface.ClearReadAchievementsCompleteDelegate(PlayerControllerId, OnPlayerAchievementsChanged);
		}
	}

	Achievements.Length = 0;
	Super.OnUnregister();
}

/**
 * Handlers for the player's login changed delegate.  Refreshes the list of achievements.
 *
 * @param LocalUserNum the player that logged in/out
 */
function OnLoginChange(byte LocalUserNum)
{
	if (LocalUserNum == PlayerControllerId)
	{
		UpdateAchievements();
	}
}

/**
 * Queries the online service for the player's list of achievements.
 */
function UpdateAchievements()
{
	local OnlineSubsystem OnlineSub;

	if (PlayerControllerId != -1)
	{
		Achievements.Length = 0;
		// Figure out if we have an online subsystem registered
		OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
		if (OnlineSub != None &&
			OnlineSub.PlayerInterface != None &&
			OnlineSub.PlayerInterface.GetLoginStatus(PlayerControllerId) > LS_NotLoggedIn &&
			// Guests don't get achievements
			!OnlineSub.PlayerInterface.IsGuestLogin(PlayerControllerId))
		{
			// Start the async task
			OnlineSub.PlayerInterface.ReadAchievements(PlayerControllerId);
		}
	}
}
