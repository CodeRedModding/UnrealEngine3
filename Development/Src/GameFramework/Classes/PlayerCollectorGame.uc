/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PlayerCollectorGame extends GameInfo;

/** The number of clients that are expected to join before we seamless travel */
var int NumberOfClientsToWaitFor;

/** URL of the actual game to travel to when all clients join */
var string URLToLoad;

event PlayerController Login(string Portal, string Options, const UniqueNetID UniqueID, out string ErrorMessage)
{
	local PlayerController PC;

	// perform default behavior to maket he controller
	PC = super.Login(Portal, Options, UniqueID, ErrorMessage);

	// if the PC failed to login, don't use it
	if (PC == none)
	{
		return none;
	}

	// handle the server starting up
	if (NumberOfClientsToWaitFor == 0)
	{
		// default to waiting on one client
		NumberOfClientsToWaitFor = GetIntOption(Options, "NumClients", 1); 
		URLToLoad = ParseOption(Options, "SubMap");
		URLToLoad = URLToLoad $ "?game=" $ ParseOption(Options, "SubGame");
	}
	else
	{
		NumberOfClientsToWaitFor--;
	}

	// when everyone has joined, seamless travel to the actual map
	if (NumberOfClientsToWaitFor == 0)
	{
`log("SEAMLESS TRAVELING TO " $ URLToLoad);
		WorldInfo.SeamlessTravel(URLToLoad, true, );
	}

	return PC;
}

event GetSeamlessTravelActorList(bool bToEntry, out array<Actor> ActorList)
{
	// add nothing we just want to start from scratch in this case
}