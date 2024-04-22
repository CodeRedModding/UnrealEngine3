/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the game type related stuff.
 */

#ifndef __GAME_TYPE_H__
#define __GAME_TYPE_H__

/**
 * The type of game that is running
 */
enum EGameType
{
	EGameType_Unknown = 0,
	EGameType_Editor,
	EGameType_Server,
	EGameType_ListenServer,
	EGameType_Client,
	EGameType_Max
};

/**
 *	Returns game type name for enum value.
 */
inline const char* ToString(EGameType type)
{
	switch (type)
	{
		case EGameType_Editor: return "Editor";
		case EGameType_Server: return "Server";
		case EGameType_ListenServer: return "Listen Server";
		case EGameType_Client: return "Client";
	}
	return "Unknown";
}

#ifndef NON_UE3_APPLICATION

#if WITH_UE3_NETWORKING

/**
 * Determines the type of game we currently are running
 */
inline EGameType appGetGameType(void)
{
	EGameType GameType = EGameType_Editor;
	if (GIsEditor == FALSE)
	{
		// Check the world's info object
		if (GWorld != NULL && GWorld->GetWorldInfo() != NULL)
		{
			// Map the netmode to our enum
			switch (GWorld->GetWorldInfo()->NetMode)
			{
				case NM_Standalone:
				case NM_Client:
				{
					GameType = EGameType_Client;
					break;
				}
				case NM_ListenServer:
				{
					GameType = EGameType_ListenServer;
					break;
				}
				case NM_DedicatedServer:
				{
					GameType = EGameType_Server;
					break;
				}
			};
		}
		else
		{
			// Can't figure it out
			GameType = EGameType_Unknown;
		}
	}
	return GameType;
}

#endif	//#if WITH_UE3_NETWORKING

#endif // UE3_APPLICATION

#endif // __GAME_TYPE_H__
