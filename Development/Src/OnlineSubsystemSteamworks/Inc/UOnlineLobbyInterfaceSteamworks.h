/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS && STEAM_MATCHMAKING_LOBBY
	/**
	 * Interface initialization
	 *
	 * @param InSubsystem	Reference to the initializing subsystem
	 */
	void InitInterface(UOnlineSubsystemSteamworks* InSubsystem);

	/**
	 * Cleanup
	 */
	virtual void FinishDestroy();

	// General functions

	/** Attempts to parse the specified lobbies settings into the target array, returning FALSE if the data needs to be requested first */
	UBOOL FillLobbySettings(TArray<FLobbyMetaData>& TargetArray, FUniqueNetId LobbyId);

	/** Attempts to parse the settings of a member in the specified lobby, into the target array */
	UBOOL FillMemberSettings(TArray<FLobbyMetaData>& TargetArray, FUniqueNetId LobbyId, FUniqueNetId MemberId);

#endif	// WITH_UE3_NETWORKING && WITH_STEAMWORKS && STEAM_MATCHMAKING_LOBBY

