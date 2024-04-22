/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class holds a list of players met online that the players on this PC/console
 * encountered. It does not persist the list. Both parties and individuals are tracked
 * with the individuals containing all party members. Note that it only holds the
 * unique ids of the players.
 */
class OnlineRecentPlayersList extends Object
	config(Engine);

/** The set of players that players on this PC/console have recently encountered */
var array<UniqueNetId> RecentPlayers;

/** Holds a set of players that made up a party */
struct RecentParty
{
	/** The player that was the party leader */
	var UniqueNetid PartyLeader;
	/** The list of players that comprise the party (should include the leader) */
	var array<UniqueNetId> PartyMembers;
};

/** The list of recent parties that the players on this PC/console has encountered */
var array<RecentParty> RecentParties;

/** Holds the information about the last party that a player on this pc/console was in */
var RecentParty LastParty;

/** The size of the recent player list to allow before losing the oldest entries */
var config int MaxRecentPlayers;

/** The size of the recent party list to allow before losing the oldest entries */
var config int MaxRecentParties;

/** The position in the array that new players should be added at */
var int RecentPlayersAddIndex;

/** The position in the array that new parties should be added at */
var int RecentPartiesAddIndex;

/** The set of people that the player/PC/console are currently playing with/against */
struct CurrentPlayerMet
{
	/** The team the player is on */
	var int TeamNum;
	/** The skill rating of the player */
	var int Skill;
	/** The unique net id for the player */
	var UniqueNetId NetId;
};

/** Holds the list of current players (the set of players currently in a session */
var private array<CurrentPlayerMet> CurrentPlayers;

/**
 * Adds a player to the recent players list
 *
 * @param NewPlayer the player being added
 */
function AddPlayerToRecentPlayers(UniqueNetId NewPlayer)
{
	local int FindIndex;

	// Search the list of players for this one and only add if not present
	FindIndex = RecentPlayers.Find('Uid',NewPlayer.Uid);
	if (FindIndex == INDEX_NONE)
	{
		// Wrap back to the oldest entry if we've hit our max
		if (RecentPlayersAddIndex >= MaxRecentPlayers)
		{
			RecentPlayersAddIndex = 0;
		}
		// Make sure the array has space
		if (RecentPlayersAddIndex + 1 >= RecentPlayers.Length)
		{
			RecentPlayers.Length = RecentPlayersAddIndex + 1;
		}
		RecentPlayers[RecentPlayersAddIndex] = NewPlayer;
		// Move to the next available slot. This will wrap to zero if it grows too large
		RecentPlayersAddIndex++;
	}
}

/** Clears the recent players list and resets the add index */
function ClearRecentPlayers()
{
	RecentPlayersAddIndex = 0;
	RecentPlayers.Length = 0;
}

/**
 * Adds a player to the recent players list
 *
 * @param PartyLeader the player being added
 * @param PartyMembers the members of the party
 */
function AddPartyToRecentParties(UniqueNetId PartyLeader,const out array<UniqueNetId> PartyMembers)
{
	local int FindIndex;

	// Search the list of parties for the leader and only add if not present
	FindIndex = RecentParties.Find('PartyLeader',PartyLeader);
	if (FindIndex == INDEX_NONE)
	{
		// Wrap back to the oldest entry if we've hit our max
		if (RecentPartiesAddIndex >= MaxRecentParties)
		{
			RecentPartiesAddIndex = 0;
		}
		// Make sure the array has space
		if (RecentPartiesAddIndex + 1 >= RecentParties.Length)
		{
			RecentParties.Length = RecentPartiesAddIndex + 1;
		}
		RecentParties[RecentPartiesAddIndex].PartyLeader = PartyLeader;
		RecentParties[RecentPartiesAddIndex].PartyMembers = PartyMembers;
		// Move to the next available slot. This will wrap to zero if it grows too large
		RecentPartiesAddIndex++;
	}
}

/** Clears the recent parties list and resets the add index */
function ClearRecentParties()
{
	RecentPartiesAddIndex = 0;
	RecentParties.Length = 0;
}

/**
 * Builds a single list of players from the recent parties list
 *
 * @param Players the array getting the data copied into it
 */
function GetPlayersFromRecentParties(out array<UniqueNetId> Players)
{
	local int PartyIndex;
	local int MemberIndex;
	local int AddMemberAt;

	Players.Length = 0;
	AddMemberAt = 0;
	// Look at each registered party and add them for showing
	for (PartyIndex = 0; PartyIndex < RecentParties.Length; PartyIndex++)
	{
		for (MemberIndex = 0; MemberIndex < RecentParties[PartyIndex].PartyMembers.Length; MemberIndex++)
		{
			Players.Length = AddMemberAt + 1;
			Players[AddMemberAt] = RecentParties[PartyIndex].PartyMembers[MemberIndex];
		}
	}
}

/**
 * Builds a single list of players from the current players list
 *
 * @param Players the array getting the data copied into it
 */
function GetPlayersFromCurrentPlayers(out array<UniqueNetId> Players)
{
	local int PlayerIndex;

	Players.Length = 0;
	// Look at each registered party and add them for showing
	for (PlayerIndex = 0; PlayerIndex < CurrentPlayers.Length; PlayerIndex++)
	{
		Players.AddItem(CurrentPlayers[PlayerIndex].NetId);
	}
}

/**
 * Finds the player indicated and returns their skill rating
 *
 * @param Player the player to search for
 *
 * @return the skill for the specified player
 */
function int GetSkillForCurrentPlayer(UniqueNetId Player)
{
	local int PlayerIndex;

	// Search for the specified player and return their skill
	for (PlayerIndex = 0; PlayerIndex < CurrentPlayers.Length; PlayerIndex++)
	{
		if (CurrentPlayers[PlayerIndex].NetId == Player)
		{
			return CurrentPlayers[PlayerIndex].Skill;
		}
	}
	return 0;
}

/**
 * Finds the player indicated and returns their team that was assigned
 *
 * @param Player the player to search for
 *
 * @return the team number for the specified player
 */
function int GetTeamForCurrentPlayer(UniqueNetId Player)
{
	local int PlayerIndex;

	// Search for the specified player and return their team number
	for (PlayerIndex = 0; PlayerIndex < CurrentPlayers.Length; PlayerIndex++)
	{
		if (CurrentPlayers[PlayerIndex].NetId == Player)
		{
			return CurrentPlayers[PlayerIndex].TeamNum;
		}
	}
	return 255;
}

/**
 * Adds a player to the recent players list
 *
 * @param PartyLeader the player being added
 * @param PartyMembers the members of the party
 */
function SetLastParty(UniqueNetId PartyLeader,const out array<UniqueNetId> PartyMembers)
{
	LastParty.PartyLeader = PartyLeader;
	LastParty.PartyMembers = PartyMembers;
}

/**
 * Helper function for showing the recent players list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Title the title to use for the UI
 * @param Description the text to show at the top of the UI
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowRecentPlayerList(byte LocalUserNum,string Title,string Description)
{
	local OnlineSubsystem OnlineSub;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.PlayerInterfaceEx != None)
	{
		// Use the custom UI list to display them
		return OnlineSub.PlayerInterfaceEx.ShowCustomPlayersUI(LocalUserNum,RecentPlayers,Title,Description);
	}
	return false;
}

/**
 * Builds a single player list out of the various parties encountered and shows
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Title the title to use for the UI
 * @param Description the text to show at the top of the UI
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowRecentPartiesPlayerList(byte LocalUserNum,string Title,string Description)
{
	local OnlineSubsystem OnlineSub;
	local array<UniqueNetId> Players;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.PlayerInterfaceEx != None)
	{
		GetPlayersFromRecentParties(Players);
		// Use the custom UI list to display them
		return OnlineSub.PlayerInterfaceEx.ShowCustomPlayersUI(LocalUserNum,Players,Title,Description);
	}
	return false;
}

/**
 * Shows the last party that you were in as a player list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Title the title to use for the UI
 * @param Description the text to show at the top of the UI
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowLastPartyPlayerList(byte LocalUserNum,string Title,string Description)
{
	local OnlineSubsystem OnlineSub;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.PlayerInterfaceEx != None)
	{
		// Use the custom UI list to display them
		return OnlineSub.PlayerInterfaceEx.ShowCustomPlayersUI(LocalUserNum,LastParty.PartyMembers,Title,Description);
	}
	return false;
}

/**
 * Builds a single player list out of the players in the current players list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Title the title to use for the UI
 * @param Description the text to show at the top of the UI
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowCurrentPlayersList(byte LocalUserNum,string Title,string Description)
{
	local OnlineSubsystem OnlineSub;
	local array<UniqueNetId> Players;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None &&
		OnlineSub.PlayerInterfaceEx != None)
	{
		GetPlayersFromCurrentPlayers(Players);
		// Use the custom UI list to display them
		return OnlineSub.PlayerInterfaceEx.ShowCustomPlayersUI(LocalUserNum,Players,Title,Description);
	}
	return false;
}

`if(`notdefined(FINAL_RELEASE))
/** Log list of players for debugging */
function DumpPlayersList(const out array<CurrentPlayerMet> Players)
{
	local OnlineSubsystem OnlineSub;
	local int PlayerIdx,TeamNum;
	local UniqueNetId NetId;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		for (PlayerIdx=0; PlayerIdx<Players.Length; PlayerIdx++)
		{
			NetId = Players[PlayerIdx].NetId;
			TeamNum = Players[PlayerIdx].TeamNum;
			`Log("DumpPlayersList: "
				$" TeamNum="$TeamNum
				$" PlayerIdx="$PlayerIdx
				$" UniqueId="$OnlineSub.UniqueNetIdToString(NetId)
				,,'DevOnline');
		}
	}
}
/** Log current players list for debugging */
function DumpCurrentPlayers()
{
	DumpPlayersList(CurrentPlayers);
}
`endif

/**
 * Sets the current player list to the data specified
 *
 * @param Players the list of players to copy
 */
function SetCurrentPlayersList(const array<CurrentPlayerMet> Players)
{
`if(`notdefined(FINAL_RELEASE))
	DumpPlayersList(Players);
`endif
	CurrentPlayers = Players;
}

/** @return the number of players in the current player session */
function int GetCurrentPlayersListCount()
{
	return CurrentPlayers.Length;
}