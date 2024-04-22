/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#if WITH_UE3_NETWORKING

protected:

	/**
	 * Ticks the lan beacon for lan support
	 *
	 * @param DeltaTime the time since the last tick
	 */
	virtual void TickLanTasks(FLOAT DeltaTime);

	/**
	 * Determines if the packet header is valid or not
	 *
	 * @param Packet the packet data to check
	 * @param Length the size of the packet buffer
	 * @param ClientNonce out param that reads the client's nonce
	 *
	 * @return true if the header is valid, false otherwise
	 */
	UBOOL IsValidLanQueryPacket(const BYTE* Packet,DWORD Length,QWORD& ClientNonce);

	/**
	 * Determines if the packet header is valid or not
	 *
	 * @param Packet the packet data to check
	 * @param Length the size of the packet buffer
	 *
	 * @return true if the header is valid, false otherwise
	 */
	UBOOL IsValidLanResponsePacket(const BYTE* Packet,DWORD Length);

	/** Stops the lan beacon from accepting broadcasts */
	inline void StopLanBeacon(void)
	{
		// Don't poll anymore since we are shutting it down
		LanBeaconState = LANB_NotUsingLanBeacon;
		// Unbind the lan beacon object
		delete LanBeacon;
		LanBeacon = NULL;
	}

	/**
	 * Creates the lan beacon for queries/advertising servers
	 */
	inline DWORD StartLanBeacon(void)
	{
		DWORD Result = S_OK;
		if (LanBeacon != NULL)
		{
			StopLanBeacon();
		}
		// Bind a socket for lan beacon activity
		LanBeacon = new FLanBeacon();
		if (LanBeacon->Init(LanAnnouncePort))
		{
			// We successfully created everything so mark the socket as
			// needing polling
			LanBeaconState = LANB_Hosting;
			debugf(NAME_DevOnline,
				TEXT("Listening for lan beacon requests on %d"),
				LanAnnouncePort);
		}
		else
		{
			debugf(NAME_Error,TEXT("Failed to init to lan beacon %s"),
				GSocketSubsystem->GetSocketError());
			Result = E_FAIL;
		}
		return Result;
	}

	/**
	 * Adds the game settings data to the packet that is sent by the host
	 * in reponse to a server query
	 *
	 * @param Packet the writer object that will encode the data
	 * @param GameSettings the game settings to add to the packet
	 */
	void AppendGameSettingsToPacket(FNboSerializeToBuffer& Packet,UOnlineGameSettings* GameSettings);

	/**
	 * Reads the game settings data from the packet and applies it to the
	 * specified object
	 *
	 * @param Packet the reader object that will read the data
	 * @param GameSettings the game settings to copy the data to
	 */
	void ReadGameSettingsFromPacket(FNboSerializeFromBuffer& Packet,UOnlineGameSettings* NewServer);

	/**
	 * Builds a LAN query and broadcasts it
	 *
	 * @return an error/success code
	 */
	DWORD FindLanGames(void);

	/**
	 * Creates a new lan enabled game
	 *
	 * @param HostingPlayerNum the player hosting the game
	 *
	 * @return S_OK if it succeeded, otherwise an error code
	 */
	DWORD CreateLanGame(BYTE HostingPlayerNum);

	/**
	 * Terminates a LAN session
	 *
	 * @return an error/success code
	 */
	DWORD DestroyLanGame(void);

	/**
	 * Parses a LAN packet and handles responses/search population
	 * as needed
	 *
	 * @param PacketData the packet data to parse
	 * @param PacketLength the amount of data that was received
	 */
	void ProcessLanPacket(BYTE* PacketData,INT PacketLength);

	/**
	 * Generates a random nonce (number used once) of the desired length
	 *
	 * @param Nonce the buffer that will get the randomized data
	 * @param Length the number of bytes to generate random values for
	 */
	inline void GenerateNonce(BYTE* Nonce,DWORD Length)
	{
//@todo joeg -- switch to CryptGenRandom() if possible or something equivalent
		// Loop through generating a random value for each byte
		for (DWORD NonceIndex = 0; NonceIndex < Length; NonceIndex++)
		{
			Nonce[NonceIndex] = (BYTE)(appRand() & 255);
		}
	}

	/**
	 * Creates a new session info object that is correct for each platform
	 */
	virtual FSessionInfo* CreateSessionInfo(void)
	{
		return new FSessionInfo();
	}

	/**
	 *	Return the size of a session info struct
	 */
	virtual SIZE_T GetSessionInfoSize()
	{
		return sizeof(FSessionInfo);
	}

public:

	/**
	 * Ticks this object to update any async tasks
	 *
	 * @param DeltaTime the time since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

protected:

	/**
	 * Builds an internet query and broadcasts it
	 *
	 * @return an error/success code
	 */
	virtual DWORD FindInternetGames(void)
	{
		return (DWORD)-1;
	}

	/**
	 * Attempts to cancel an internet game search
	 *
	 * @return an error/success code
	 */
	virtual DWORD CancelFindInternetGames(void)
	{
		return (DWORD)-1;
	}

	/**
	 * Serializes the platform specific data into the provided buffer for the specified search result
	 *
	 * @param DesiredGame the game to copy the platform specific data for
	 * @param PlatformSpecificInfo the buffer to fill with the platform specific information
	 *
	 * @return true if successful serializing the data, false otherwise
	 */
	virtual DWORD ReadPlatformSpecificInternetSessionInfo(const FOnlineGameSearchResult& DesiredGame,BYTE* PlatformSpecificInfo)
	{
		return (DWORD)-1;
	}

	/**
	 * Builds a search result using the platform specific data specified
	 *
	 * @param SearchingPlayerNum the index of the player searching for a match
	 * @param SearchSettings the desired search to bind the session to
	 * @param PlatformSpecificInfo the platform specific information to convert to a server object
	 *
	 * @return an error/success code
	 */
	virtual DWORD BindPlatformSpecificSessionToInternetSearch(BYTE SearchingPlayerNum,UOnlineGameSearch* SearchSettings,BYTE* PlatformSpecificInfo)
	{
		return (DWORD)-1;
	}

	/**
	 * Creates a new internet enabled game
	 *
	 * @param HostingPlayerNum the player hosting the game
	 *
	 * @return S_OK if it succeeded, otherwise an error code
	 */
	virtual DWORD CreateInternetGame(BYTE HostingPlayerNum)
	{
		return (DWORD)-1;
	}

	/**
	 * Joins the specified internet enabled game
	 *
	 * @param PlayerNum the player joining the game
	 *
	 * @return S_OK if it succeeded, otherwise an error code
	 */
	virtual DWORD JoinInternetGame(BYTE PlayerNum)
	{
		return (DWORD)-1;
	}

	/**
	 * Starts the specified internet enabled game
	 *
	 * @return S_OK if it succeeded, otherwise an error code
	 */
	virtual DWORD StartInternetGame(void)
	{
		return (DWORD)-1;
	}

	/**
	 * Ends the specified internet enabled game
	 *
	 * @return S_OK if it succeeded, otherwise an error code
	 */
	virtual DWORD EndInternetGame(void)
	{
		return (DWORD)-1;
	}

	/**
	 * Terminates an internet session with the provider
	 *
	 * @return an error/success code
	 */
	virtual DWORD DestroyInternetGame(void)
	{
		return (DWORD)-1;
	}

	/**
	 * Updates any pending internet tasks and fires event notifications as needed
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last call
	 */
	virtual void TickInternetTasks(FLOAT DeltaTime)
	{
	}

	/** Registers all of the local talkers with the voice engine */
	virtual void RegisterLocalTalkers(void)
	{
	}

	/** Unregisters all of the local talkers from the voice engine */
	virtual void UnregisterLocalTalkers(void)
	{
	}

	/** Removes all of the remote talkers from the voice engine */
	virtual void RemoveAllRemoteTalkers(void)
	{
	}

#endif	//#if WITH_UE3_NETWORKING
