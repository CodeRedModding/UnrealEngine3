/**
 * Provides information about the static resources available for a particular gametype.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIGameInfoSummary extends UIResourceDataProvider
	PerObjectConfig
	Config(Game);

var	config		string	ClassName;
var	config		string	GameAcronym;
var	config		string	MapPrefix;
var	config		bool	bIsTeamGame;

/** the pathname for the OnlineGameSettings subclass associated with this gametype */
var	config		string	GameSettingsClassName;

// may want to expose other props here, like MaxPlayers, GoalScore, etc.

var	config localized	string	GameName;
var	config localized	string	Description;

var	config		bool	bIsDisabled;

DefaultProperties
{

}
