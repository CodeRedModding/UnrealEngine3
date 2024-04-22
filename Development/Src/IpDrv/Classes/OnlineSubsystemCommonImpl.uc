/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Class that implements commonly needed members/features across all platforms
 */
class OnlineSubsystemCommonImpl extends OnlineSubsystem
	native
	abstract
	config(Engine);

/**
 * Holds the pointer to the platform specific FVoiceInterface implementation
 * used for voice communication
 */
var const native transient pointer VoiceEngine{class FVoiceInterface};

/** Holds the maximum number of local talkers allowed */
var config int MaxLocalTalkers;

/** Holds the maximum number of remote talkers allowed (clamped to 30 which is XHV max) */
var config int MaxRemoteTalkers;

/** Whether speech recognition is enabled */
var config bool bIsUsingSpeechRecognition;

/** The object that handles the game interface implementation across platforms */
var OnlineGameInterfaceImpl GameInterfaceImpl;

/** The object that handles the auth interface implementation across platforms */
var OnlineAuthInterfaceImpl AuthInterfaceImpl;

/**
 * Returns the name of the player for the specified index
 *
 * @param UserIndex the user to return the name of
 *
 * @return the name of the player at the specified index
 */
event string GetPlayerNicknameFromIndex(int UserIndex);

/**
 * Determine if the player is registered in the specified session
 *
 * @param PlayerId the player to check if in session or not
 * @return TRUE if the player is a registrant in the session
 */
native function bool IsPlayerInSession(name SessionName,UniqueNetId PlayerId);

/**
 * Get a list of the net ids for the players currently registered on the session
 *
 * @param SessionName name of the session to find
 * @param OutRegisteredPlayers [out] list of player net ids in the session (empty if not found)
 */
function GetRegisteredPlayers(name SessionName,out array<UniqueNetId> OutRegisteredPlayers)
{
	local int Idx,PlayerIdx;

	OutRegisteredPlayers.Length = 0;
	for (Idx=0; Idx < Sessions.Length; Idx++)
	{
		// find session by name
		if (Sessions[Idx].SessionName == SessionName)
		{
			// return list of player ids currently registered on the session
			OutRegisteredPlayers.Length = Sessions[Idx].Registrants.Length;
			for (PlayerIdx=0; PlayerIdx < Sessions[Idx].Registrants.Length; PlayerIdx++)
			{
				OutRegisteredPlayers[PlayerIdx] = Sessions[Idx].Registrants[PlayerIdx].PlayerNetId;
			}
			break;
		}
	}
}
