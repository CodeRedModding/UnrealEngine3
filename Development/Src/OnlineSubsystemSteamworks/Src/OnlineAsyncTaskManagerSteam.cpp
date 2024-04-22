/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


// Includes

#include "OnlineSubsystemSteamworks.h"

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

// FOnlineAsyncTaskManagerSteam implementation

/**
 * Init the online async task manager
 *
 * @return	Returns TRUE if initialization was successful, FALSE otherwise
 */
UBOOL FOnlineAsyncTaskManagerSteam::Init()
{
	UBOOL bSuccess = FOnlineAsyncTaskManager::Init();

	return bSuccess;
}

/**
 * This is called if a thread is requested to terminate early
 */
void FOnlineAsyncTaskManagerSteam::Stop()
{
	FOnlineAsyncTaskManager::Stop();
}

/**
 * Called in the context of the aggregating thread to perform any cleanup
 */
void FOnlineAsyncTaskManagerSteam::Exit()
{
	FOnlineAsyncTaskManager::Exit();
}

/**
 * Give the online service a chance to do work
 * NOTE: Call only from online thread
 */
void FOnlineAsyncTaskManagerSteam::OnlineTick()
{
	if (!bRequestingExit)
	{
		if (IsSteamClientAvailable())
		{
			SteamAPI_RunCallbacks();
		}

		if (IsSteamServerAvailable())
		{
			SteamGameServer_RunCallbacks();
		}
	}
}

#endif // WITH_UE3_NETWORKING && WITH_STEAMWORKS



