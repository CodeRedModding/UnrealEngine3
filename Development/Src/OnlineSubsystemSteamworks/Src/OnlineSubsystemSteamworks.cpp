/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemSteamworks.h"
#include "VoiceInterfaceSteamworks.h"

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

/**
 * Globals
 */

/** Manages threaded execution of SteamAPI tasks, and passing them back to the game thread */
FOnlineAsyncTaskManagerSteamBase* GSteamAsyncTaskManager = NULL;


/** Whether or not the Steam client API is initialized */
UBOOL GSteamworksClientInitialized = FALSE;

/** Whether or not the Steam game server API is initialized */
UBOOL GSteamworksGameServerInitialized = FALSE;

/** Whether or not the Steam game server API is fully logged in and connected (with a valid Steam UID etc.) */
UBOOL GSteamworksGameServerConnected = FALSE;


/** If the game was launched from Steam, and Steam specified a +connect address on the commandline, this stores that address */
FInternetIpAddr GSteamCmdLineConnect;

/** If the we are directly connecting to a server on launch, sometimes Steam specifies a password on the commandline as well; this stores that */
FString GSteamCmdLinePassword;

/** Whether or not a Steam +connect commandline was set */
UBOOL GSteamCmdLineSet = FALSE;


// Cached Steamworks SDK interface pointers...
ISteamUtils* GSteamUtils = NULL;
ISteamUser* GSteamUser = NULL;
ISteamFriends* GSteamFriends = NULL;
ISteamRemoteStorage* GSteamRemoteStorage = NULL;
ISteamUserStats* GSteamUserStats = NULL;
ISteamMatchmakingServers* GSteamMatchmakingServers = NULL;
ISteamGameServer* GSteamGameServer = NULL;
ISteamApps* GSteamApps = NULL;
ISteamGameServerStats* GSteamGameServerStats = NULL;
ISteamMatchmaking* GSteamMatchmaking = NULL;
ISteamNetworking* GSteamNetworking = NULL;
ISteamNetworking* GSteamGameServerNetworking = NULL;
ISteamUtils* GSteamGameServerUtils = NULL;

uint32 GSteamAppID = 0;

#if STEAM_EXEC_DEBUG
FSteamExecCatcher* GSteamExecCatcher = NULL;
#endif


/**
 * Utility functions
 */

// Special log messages for steam, which highlight the log message in non-release builds (but which do not do this in release)
// NOTE: Copy-pasted from UnMisc.cpp
#define GROWABLE_LOGF(SerializeFunc) \
	INT		BufferSize	= 1024; \
	TCHAR*	Buffer		= NULL; \
	INT		Result		= -1; \
	/* allocate some stack space to use on the first pass, which matches most strings */ \
	TCHAR	StackBuffer[256]; \
	TCHAR*	AllocatedBuffer = NULL; \
\
	/* first, try using the stack buffer */ \
	Buffer = StackBuffer; \
	GET_VARARGS_RESULT( Buffer, ARRAY_COUNT(StackBuffer), ARRAY_COUNT(StackBuffer) - 1, Fmt, Fmt, Result ); \
\
	/* if that fails, then use heap allocation to make enough space */ \
	while(Result == -1) \
	{ \
		appSystemFree(AllocatedBuffer); \
		/* We need to use malloc here directly as GMalloc might not be safe. */ \
		Buffer = AllocatedBuffer = (TCHAR*) appSystemMalloc( BufferSize * sizeof(TCHAR) ); \
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result ); \
		BufferSize *= 2; \
	}; \
	Buffer[Result] = 0; \
	; \
\
	SerializeFunc; \
	appSystemFree(AllocatedBuffer);

VARARG_BODY(void, Steamdebugf, const TCHAR*, VARARG_NONE)
{
	if (!FName::SafeSuppressed(NAME_DevOnline))
	{
#if STEAMLOG_HIGHLIGHT
		SET_WARN_COLOR(COLOR_CYAN);
#endif
		GROWABLE_LOGF(GLog->Serialize(Buffer, NAME_DevOnline));

#if STEAMLOG_HIGHLIGHT
		CLEAR_WARN_COLOR();
#endif
	}
}

/** @return a UGCHandle_t from a string representation */
UBOOL StringToUGCHandle(const FString& StringHandle, UGCHandle_t& outHandle)
{
	UBOOL bResult = FALSE;

	// @todo Steam: Add and test an IsAlnum check here as well; it returns true for a-z strings
	if (!StringHandle.IsEmpty())
	{
		INT StrLen = StringHandle.Len();
		TCHAR* StrEnd = const_cast<TCHAR*>(*StringHandle + StrLen - 1);
		outHandle = appStrtoi64(*StringHandle, &StrEnd, 10);
		bResult = (outHandle != 0 && outHandle != (QWORD)-1);
	}

	return bResult;
}

/** 
 * Print Steam's view of a given file
 * @param Filename - the name of the file to get information about
 */
void PrintSteamFileState(const FString& Filename)
{
	if (Filename.Len() > 0)
	{
		INT FileSize = GSteamRemoteStorage->GetFileSize(TCHAR_TO_UTF8(*Filename));

		debugf(NAME_DevOnline, TEXT("Steam File: %s Size:%d Exists:%s Persistent:%s"),
			*Filename, FileSize, 
			GSteamRemoteStorage->FileExists(TCHAR_TO_UTF8(*Filename)) ? TEXT("Y") : TEXT("N"),
			GSteamRemoteStorage->FilePersisted(TCHAR_TO_UTF8(*Filename)) ? TEXT("Y") : TEXT("N"));
	}
}

/** 
 * Print the current state of the Steam cloud
 */
void PrintSteamCloudStorageInfo()
{
	INT TotalBytes, TotalAvailable;
	if (GSteamRemoteStorage->GetQuota(&TotalBytes, &TotalAvailable) == FALSE)
	{
		TotalBytes = TotalAvailable = 0;
	}

	debugf(NAME_DevOnline, TEXT("Steam Disk Quota: %d / %d"), TotalAvailable, TotalBytes);
	debugf(NAME_DevOnline,TEXT("Game does %shave cloud storage enabled."), GSteamRemoteStorage->IsCloudEnabledForApp() ? TEXT("") : TEXT("NOT "));
	debugf(NAME_DevOnline,TEXT("User does %shave cloud storage enabled."), GSteamRemoteStorage->IsCloudEnabledForAccount() ? TEXT("") : TEXT("NOT "));
}

/**
 * Takes an EResult struct from a Steam server disconnect, and formats it to fit the EOnlineServerConnectionStatus enum
 *
 * @param Result	The EResult to be reformatted
 * @return		The EOnlineServerConnectionStatus enum which most closely matches the EResult
 */
FORCEINLINE EOnlineServerConnectionStatus ConnectionResult(EResult Result)
{
	EOnlineServerConnectionStatus ReturnVal = OSCS_Connected;

	switch (Result)
	{
		case k_EResultAdministratorOK:
		case k_EResultOK: 
			ReturnVal = OSCS_Connected;
			
		case k_EResultNoConnection:
			ReturnVal = OSCS_NoNetworkConnection;

		case k_EResultInvalidPassword:
		case k_EResultNotLoggedOn:
		case k_EResultAccessDenied:
		case k_EResultBanned:
		case k_EResultAccountNotFound:
		case k_EResultInvalidSteamID:
		case k_EResultRevoked:
		case k_EResultExpired:
		case k_EResultAlreadyRedeemed:
		case k_EResultBlocked:
		case k_EResultIgnored:
		case k_EResultAccountDisabled:
		case k_EResultAccountNotFeatured:
		case k_EResultInsufficientPrivilege:
			ReturnVal = OSCS_InvalidUser;

		case k_EResultLogonSessionReplaced:
		case k_EResultRemoteDisconnect:
		case k_EResultLoggedInElsewhere:
			ReturnVal = OSCS_DuplicateLoginDetected;

		case k_EResultInvalidProtocolVer:
		case k_EResultContentVersion:
			ReturnVal = OSCS_UpdateRequired;

		case k_EResultBusy:
			ReturnVal = OSCS_ServersTooBusy;

		default:
			ReturnVal = OSCS_ServiceUnavailable;
	}

	return ReturnVal;
}

/** @return a string representation of a UGCHandle_t */
FORCEINLINE FString UGCHandleToString(UGCHandle_t m_handle)
{
	return FString::Printf(I64_FORMAT_TAG, m_handle);
}


/**
 * Extern functions
 */


/**
 * Steam will launch your game with "+connect xxx.xxx.xxx.xxx:yyyy" on the command line if you join a specific
 * server from the Steam client UI, outside of the game when it isn't running (when running, it'll just trigger
 * a Steam callback event to get you to connect).
 */
void appSteamHandleCmdLine(const TCHAR** CmdLine)
{
	// If Steam specified a +connect (and possibly a +password) parameter on the commandline, parse them out and store them

	// NOTE: Previously this code just removed '+connect' and let the UDK handle the specified IP as a startup URL;
	//		now the +connect IP is removed from the commandline entirely, and must be handled through the online
	//		subsystem invite code instead

	// NOTE: Log output from this function will show up in the text logfile on the hard drive, but (for whatever reason)
	//		it will >NOT< show up in the log output window

	// +connect
	TCHAR* ConnectStr = TEXT("+connect ");
	INT ConnectLen = appStrlen(ConnectStr);

	if (appStrnicmp(*CmdLine, ConnectStr, ConnectLen) == 0)
	{
		const TCHAR* OrigCmdLine = *CmdLine;

		// Cut out the +connect, and try to parse the IP
		*CmdLine += ConnectLen;

		FString IpStr;
		ParseToken(*CmdLine, IpStr, FALSE);


		INT PortDelim = IpStr.InStr(TEXT(":"));
		UBOOL bSuccess = FALSE;

		// Just an IP
		if (PortDelim == INDEX_NONE)
		{
			GSteamCmdLineConnect.SetIp(*IpStr, bSuccess);
		}
		// An IP and Port
		else
		{
			UBOOL bValidIP = FALSE;

			GSteamCmdLineConnect.SetIp(*IpStr.Left(PortDelim), bValidIP);

			if (bValidIP)
			{
				FString PortStr = IpStr.Mid(PortDelim+1);
				INT PortVal = appAtoi(*PortStr);

				// Check that the port converted properly
				if (FString::Printf(TEXT("%i"), PortVal) == PortStr)
				{
					GSteamCmdLineConnect.SetPort(PortVal);
					bSuccess = TRUE;
				}
			}
		}


		// Now skip over the IP specified for +connect
		if (bSuccess)
		{
			// Remove whitespace from start of commandline, to simplify +password parsing
			while (**CmdLine == TEXT(' '))
			{
				*CmdLine += 1;
			}

			GSteamCmdLineSet = TRUE;

			debugf(NAME_DevOnline, TEXT("Parsed Steam +connect address from commandline: %s"), *GSteamCmdLineConnect.ToString(TRUE));
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Warning! Failed to parse Steam +connect IP: %s"), *IpStr);

			// Reset the commandline to its original position
			*CmdLine = OrigCmdLine;
		}
	}


	// +password (only parse if +connect was already parsed)
	TCHAR* PasswordCmdStr = TEXT("+password ");
	INT PasswordCmdLen = appStrlen(PasswordCmdStr);

	if (GSteamCmdLineSet && appStrnicmp(*CmdLine, PasswordCmdStr, PasswordCmdLen) == 0)
	{
		const TCHAR* OrigCmdLine = *CmdLine;

		// Cut out the +password and try to parse the password
		*CmdLine += PasswordCmdLen;

		FString PasswordStr;
		ParseToken(*CmdLine, PasswordStr, FALSE);

		UBOOL bSuccess = FALSE;

		if (!PasswordStr.IsEmpty())
		{
			GSteamCmdLinePassword = PasswordStr;
			bSuccess = TRUE;
		}


		if (bSuccess)
		{
			debugf(NAME_DevOnline, TEXT("Parsed Steam +password value from commandline: %s"), *GSteamCmdLinePassword);
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Warning! Failed to parse Steam +password value: %s"), *PasswordStr);

			// Reset the commandline to its original position
			*CmdLine = OrigCmdLine;
		}
	}
}

/**
 * Whether or not the game commandline is using the specified token (the token being e.g. a commandlet name, 'editor', 'server', etc.)
 *
 * @param InToken	The token to test for
 * @return		Whether or not the commandline is using the specified token
 */
FORCEINLINE UBOOL HasCmdLineToken(const TCHAR* InToken)
{
	static FString CmdToken;

	if (CmdToken.Len() == 0)
	{
		// trim any whitespace at edges of string - this can happen if the token was quoted with leading or trailing whitespace
		// VC++ tends to do this in its "external tools" config
		const FString CmdLine = FString(appCmdLine()).Trim();
		const TCHAR* TCmdLine = *CmdLine;

		CmdToken = ParseToken(TCmdLine, FALSE);
	}

	return CmdToken == InToken;
}

/** 
 * Check to see if we can or have enabled Steam
 *
 * @return TRUE		if Steam is currently enabled or able to be enabled, FALSE otherwise
 */
UBOOL appIsSteamEnabled()
{
	if (GSteamworksClientInitialized || GSteamworksGameServerInitialized)
	{
		return TRUE;
	}


	static INT SteamEnabledCheck = -1;

	if (SteamEnabledCheck == -1)
	{
		UBOOL bEnableSteam = !ParseParam(appCmdLine(),TEXT("NOSTEAM"));
		if (bEnableSteam)
		{
			GConfig->GetBool(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("bEnableSteam"), bEnableSteam, GEngineIni);

			if (bEnableSteam)
			{
				const UBOOL bHasEditorToken = HasCmdLineToken(TEXT("EDITOR"));
				const UBOOL bHasMakeToken = HasCmdLineToken(TEXT("MAKE")) ||  HasCmdLineToken(TEXT("MAKECOMMANDLET"));
				const UBOOL bHasCookToken = HasCmdLineToken(TEXT("COOKPACKAGES"));
				bEnableSteam = !bHasEditorToken && !bHasMakeToken && !bHasCookToken && !GIsSimMobile;
			}
		}

		SteamEnabledCheck = bEnableSteam ? 1 : 0;
	}

	return SteamEnabledCheck == 1 ? TRUE : FALSE;
}

/** 
 * Initialize the steam hooks into the engine, possibly relaunching if required by the Steam service
 */
void appSteamInit()
{
#if WITH_STEAMWORKS
	if (appIsSteamEnabled())
	{
		const UBOOL bIsServer = HasCmdLineToken(TEXT("SERVER"));

		// Don't initialize the Steam Client API if we are launching as a server
		if (!bIsServer)
		{
			UBOOL bRelaunchInSteam = FALSE;
			INT RelaunchAppId = 0;

			GConfig->GetBool(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("bRelaunchInSteam"),
						bRelaunchInSteam, GEngineIni);

			GConfig->GetInt(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("RelaunchAppId"),
						RelaunchAppId, GEngineIni);

			// If the game was not launched from within Steam, but is supposed to, trigger a Steam launch and exit
			if (bRelaunchInSteam && RelaunchAppId != 0 && SteamAPI_RestartAppIfNecessary(RelaunchAppId))
			{
				debugf(TEXT("Game restarting within Steam client, exiting"));
				appRequestExit(FALSE);

				return;
			}
			// Otherwise initialize as normal
			else
			{
				// Steamworks needs to initialize as close to start as possible, so it can hook its overlay into Direct3D, etc.
				GSteamworksClientInitialized = (SteamAPI_Init() ? TRUE : FALSE);
			}

			debugf(TEXT("Steam Client API initialized %i"), GSteamworksClientInitialized);
		}
		else
		{
			debugf(TEXT("Steam Client API not initialized (not required for servers)"));
		}


		// Initialize the Steam game server interfaces (done regardless of whether or not a server will be setup)
		// NOTE: The port values specified here, are not changeable once the interface is setup
		INT ServerPort = 0;
		INT QueryPort = 0;
		UBOOL bVACEnabled = 0;
		FString GameVersion;
		DWORD LocalServerIP = 0;

		GConfig->GetInt(TEXT("URL"), TEXT("Port"), ServerPort, GEngineIni);
		GConfig->GetInt(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("QueryPort"), QueryPort, GEngineIni);
		GConfig->GetBool(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("bUseVAC"), bVACEnabled, GEngineIni);
		GConfig->GetString(TEXT("OnlineSubsystemSteamworks.OnlineSubsystemSteamworks"), TEXT("GameVersion"), GameVersion, GEngineIni);

		FString MultiHome;

		if (Parse(appCmdLine(), TEXT("MULTIHOME="), MultiHome) && !MultiHome.IsEmpty())
		{
			FInternetIpAddr HomeIP;
			UBOOL bIsValidIP = FALSE;

			HomeIP.SetIp(*MultiHome, bIsValidIP);

			if (bIsValidIP)
			{
				HomeIP.GetIp(LocalServerIP);
			}
		}


		if (QueryPort == 0)
		{
			QueryPort = 27015;
		}

		if (GameVersion.Len() == 0)
		{
			debugf(TEXT("[OnlineSubsystemSteamworks.OnlineSubsystemSteamworks].GameVersion is not set. Server advertising will fail"));
		}

		// NOTE: Default LocalServerIP of 0 causes SteamGameServer_Init to automatically use the public (external) IP
		GSteamworksGameServerInitialized = SteamGameServer_Init(LocalServerIP, ServerPort+1, ServerPort, QueryPort,
									(bVACEnabled ? eServerModeAuthenticationAndSecure : eServerModeAuthentication),
									TCHAR_TO_UTF8(*GameVersion));

		debugf(TEXT("Steam Game Server API initialized %i"), GSteamworksGameServerInitialized);

		if (GSteamworksGameServerInitialized)
		{
			GSteamGameServer = SteamGameServer();
			GSteamGameServerStats = SteamGameServerStats();
			GSteamGameServerNetworking = SteamGameServerNetworking();
			GSteamGameServerUtils = SteamGameServerUtils();

			// NOTE: It's not possible for >some< of the above globals to initialize, and others fail; it's all or none
			if (GSteamGameServer == NULL)
			{
				GSteamworksGameServerInitialized = FALSE;
			}
		}
	}
	else
	{
		debugf(TEXT("Steam Client API Disabled!"));
	}
#endif
}

/** 
 * Cleanup the Steam subsystem
 */
void appSteamShutdown()
{
	// NOTE: This intentionally does not check GSteamworksGameServerInitialized
	if (SteamGameServer() != NULL)
	{
		// Since SteamSDK 1.17, LogOff is required to stop the game server advertising after exit; ensure we don't miss this at shutdown
		//	(NOTE: the OnlineGameInterface code does not require LogOff to stop advertising though, it does this differently)
		if (GSteamGameServer != NULL && GSteamGameServer->BLoggedOn())
		{
			GSteamGameServer->LogOff();
		}

		SteamGameServer_Shutdown();
		GSteamworksGameServerConnected = FALSE;
	}

	if (GSteamworksClientInitialized)
	{
		SteamAPI_Shutdown();
		GSteamworksClientInitialized = FALSE;
	}
}

/**
 * Gets the Steam appid for this game
 *
 * @return	The appid of the game, or INDEX_NONE if it cannot be retrieved
 */
INT appGetSteamworksAppId()
{
	INT ReturnVal = INDEX_NONE;

	if (GSteamworksClientInitialized && GSteamUtils != NULL)
	{
		ReturnVal = GSteamUtils->GetAppID();
	}

	return ReturnVal;
}


/**
 * Async events/tasks
 */

/**
 * Notification event from Steam that a given user's stats/achievements data has been downloaded from the server
 */
class FOnlineAsyncEventSteamStatsReceived : public FOnlineAsyncEventSteam<UserStatsReceived_t, UOnlineSubsystemSteamworks>
{
private:
	/** User this data is for */
	QWORD	UserId;

	/** Result of the download */
	EResult	StatsReceivedResult;


	/** Hidden constructor */
	FOnlineAsyncEventSteamStatsReceived()
		: UserId(0)
		, StatsReceivedResult(k_EResultOK)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamStatsReceived(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
		, UserId(0)
		, StatsReceivedResult(k_EResultOK)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamStatsReceived()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamStatsReceived completed bWasSuccessful: %d, User: ") I64_FORMAT_TAG
					TEXT(", Result: %s"),
					((StatsReceivedResult == k_EResultOK) ? 1 : 0), UserId,
					*SteamResultString(StatsReceivedResult));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(UserStatsReceived_t* CallbackData)
	{
		UBOOL bSuccess = FALSE;
		const CGameID GameID(GSteamAppID);

		UserId = CallbackData->m_steamIDUser.ConvertToUint64();
		StatsReceivedResult = CallbackData->m_eResult;

		if (GameID.ToUint64() == CallbackData->m_nGameID)
		{
			if (StatsReceivedResult != k_EResultOK)
			{
				if (StatsReceivedResult == k_EResultFail)
				{
					debugf(NAME_DevOnline, TEXT("Failed to obtain steam user stats, user: ") I64_FORMAT_TAG
						TEXT(" has no stats entries"), UserId);
				}
				else
				{
					debugf(NAME_DevOnline, TEXT("Failed to obtained steam user stats, user '") I64_FORMAT_TAG
						TEXT("', error: %s"), UserId, *SteamResultString(CallbackData->m_eResult));
				}
			}

			bSuccess = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Obtained steam user stats, but for wrong game! Ignoring."));
		}

		return bSuccess;
	}


	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		CallbackInterface->UserStatsReceivedState = (StatsReceivedResult == k_EResultOK ? OERS_Done : OERS_Failed);

		if (StatsReceivedResult == k_EResultOK)
		{
			debugf(NAME_DevOnline, TEXT("Obtained steam user stats, user: ") I64_FORMAT_TAG, UserId);
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();

		OnlineSubsystemSteamworks_eventOnReadAchievementsComplete_Parms Parms(EC_EventParm);
		Parms.TitleId = 0;

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->AchievementReadDelegates, &Parms);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamStatsReceived);

/**
 * Notification event from Steam that the currently logged in user's stats/achievements data has been stored with the server
 */
class FOnlineAsyncEventSteamStatsStored : public FOnlineAsyncEventSteam<UserStatsStored_t, UOnlineSubsystemSteamworks>
{
private:
	/** Result of the attempted stats store */
	EResult	StatsStoredResult;


	/** Hidden constructor */
	FOnlineAsyncEventSteamStatsStored()
		: StatsStoredResult(k_EResultOK)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamStatsStored(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
		, StatsStoredResult(k_EResultOK)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamStatsStored()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamStatsStored completed bWasSuccessful: %d, Result: %s"),
					((StatsStoredResult == k_EResultOK) ? 1 : 0),
					*SteamResultString(StatsStoredResult));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(UserStatsStored_t* CallbackData)
	{
		UBOOL bSuccess = FALSE;
		const CGameID GameID(GSteamAppID);

		StatsStoredResult = CallbackData->m_eResult;

		if (GameID.ToUint64() == CallbackData->m_nGameID)
		{
			if (CallbackData->m_eResult == k_EResultOK)
			{
				debugf(NAME_DevOnline, TEXT("Stored steam user stats."));
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Failed to store steam user stats, error: %s"), *SteamResultString(StatsStoredResult));
			}

			bSuccess = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Stored steam user stats, but for wrong game! Ignoring."));
		}

		return bSuccess;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();

		if (CallbackInterface->bStoringAchievement)
		{
			FAsyncTaskDelegateResults Results(StatsStoredResult == k_EResultOK ? ERROR_SUCCESS : E_FAIL);
			TriggerOnlineDelegates(CallbackInterface, CallbackInterface->AchievementDelegates, &Results);

			CallbackInterface->bStoringAchievement = FALSE;
		}

		if (CallbackInterface->bClientStatsStorePending)
		{
			CallbackInterface->bClientStatsStorePending = FALSE;
			UBOOL bSuccess = TRUE;

			if (StatsStoredResult != k_EResultOK)
			{
				debugf(NAME_DevOnline, TEXT("Failed to store steam user stats, error: %s"), *SteamResultString(StatsStoredResult));
				bSuccess = FALSE;
			}

			FAsyncTaskDelegateResultsNamedSession Params(FName(TEXT("Game")), (bSuccess ? S_OK : E_FAIL));
			TriggerOnlineDelegates(CallbackInterface, CallbackInterface->FlushOnlineStatsDelegates, &Params);
		}
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamStatsStored);

/**
 * Notification event from Steam that the external overlay UI has been opened
 */
class FOnlineAsyncEventSteamExternalUITriggered : public FOnlineAsyncEventSteam<GameOverlayActivated_t, UOnlineSubsystemSteamworks>
{
private:
	/** Whether or not the overlay has become active */
	UBOOL bActive;


	/** Hidden constructor */
	FOnlineAsyncEventSteamExternalUITriggered()
		: bActive(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamExternalUITriggered(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
		, bActive(FALSE)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamExternalUITriggered()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamExternalUITriggered completed bActive: %i"), bActive);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(GameOverlayActivated_t* CallbackData)
	{
		bActive = CallbackData->m_bActive != 0;
		debugf(NAME_DevOnline, TEXT("Steam GameOverlayActivated: %i"), bActive);

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		// Try to pause the game, if allowed, as if we just lost the window manager's focus
		if (GEngine != NULL)
		{
			GEngine->OnLostFocusPause(bActive);
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamExternalUITriggered);

/**
 * Notification event that Steam wants to shut down
 */
class FOnlineAsyncEventSteamShutdown : public FOnlineAsyncEventSteam<SteamShutdown_t, UOnlineSubsystemSteamworks>
{
private:
	/** Hidden constructor */
	FOnlineAsyncEventSteamShutdown()
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamShutdown(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamShutdown()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamShutdown completed"));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(SteamShutdown_t* CallbackData)
	{
		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		debugf(TEXT("Steam client is shutting down; exiting, too."));

		// Steam client is shutting down, so go with it (but don't force immediate kill)
		appRequestExit(FALSE);
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamShutdown);


/**
 * Notification event from Steam when the player tries to join a game through the Steam client
 */
class FOnlineAsyncEventSteamServerChangeRequest : public FOnlineAsyncEventSteam<GameServerChangeRequested_t, UOnlineSubsystemSteamworks>
{
private:
	/** The address of the server (not always an IP, may be HTTP address needing resolve) */
	FString ServerAddress;

	/** The password for the server */
	FString ServerPassword;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServerChangeRequest()
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamServerChangeRequest(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServerChangeRequest()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServerChangeRequest completed ServerAddress: %s, ServerPassword: %s"),
					*ServerAddress, *ServerPassword);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(GameServerChangeRequested_t* CallbackData)
	{
		UBOOL bSuccess = FALSE;

		ServerAddress = UTF8_TO_TCHAR(CallbackData->m_rgchServer);
		ServerPassword = UTF8_TO_TCHAR(CallbackData->m_rgchPassword);

		if (GIsClient)
		{
			bSuccess = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("SteamServerChangeRequest: We're not a client, not changing servers! (Server: %s)"),
				*ServerAddress);
		}

		return bSuccess;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();


		INT PortDelim = ServerAddress.InStr(TEXT(":"));
		UBOOL bValidAddress = FALSE;
		FInternetIpAddr ServerIpAddr;
		INT ServerPort = 0;

		if (PortDelim != INDEX_NONE)
		{
			ServerIpAddr.SetIp(*ServerAddress.Left(PortDelim), bValidAddress);
			ServerIpAddr.SetPort(appAtoi(*ServerAddress.Mid(PortDelim+1)));
		}

		if (bValidAddress)
		{
			QWORD SteamSocketsAddr = 0;
			QWORD FriendUIDDud = 0;

			// Steam UI invites have a flaw where they only specify the IP; see if we can match the invite to a friend,
			//	if that friend is in a game, and if the friend has rich presence set containing the servers steam sockets address
			//	(if the server is using steam sockets at all)
			if (CallbackInterface->GetInviteFriend(ServerIpAddr, FriendUIDDud, SteamSocketsAddr) && SteamSocketsAddr != 0)
			{
				debugf(NAME_DevOnline, TEXT("Steam UI invite: Found steam sockets address in friend 'rich presence' info: ")
					I64_FORMAT_TAG, SteamSocketsAddr);
			}

			FString CleanAddr = FString::Printf(TEXT("%s"), *ServerIpAddr.ToString(TRUE));
			CallbackInterface->CachedGameInt->FindInviteGame(CleanAddr, SteamSocketsAddr);
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Got an invite with an invalid address: %s"), *ServerAddress);
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerChangeRequest);

/**
 * Notification event from Steam when the player tries to join a friends game, with associated rich presence info
 */
class FOnlineAsyncEventSteamRichPresenceJoinRequest : public FOnlineAsyncEventSteam<GameRichPresenceJoinRequested_t, UOnlineSubsystemSteamworks>
{
private:
	/** The UID of the friend we are joining */
	QWORD	FriendUID;

	/** The connect URL for the server the friend is on */
	FString	ConnectURL;


	/** Hidden constructor */
	FOnlineAsyncEventSteamRichPresenceJoinRequest()
		: FriendUID(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamRichPresenceJoinRequest(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
		, FriendUID(0)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamRichPresenceJoinRequest()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamRichPresenceJoinRequest completed FriendUID: ") I64_FORMAT_TAG
					TEXT(", ConnectURL: %s"), FriendUID, *ConnectURL);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(GameRichPresenceJoinRequested_t* CallbackData)
	{
		UBOOL bSuccess = FALSE;

		FriendUID = CallbackData->m_steamIDFriend.ConvertToUint64();
		ConnectURL = UTF8_TO_TCHAR(CallbackData->m_rgchConnect);

		if (GIsClient)
		{
			debugf(NAME_DevOnline, TEXT("RichPresenceJoinRequest: FriendUID: ") I64_FORMAT_TAG TEXT(", ConnectURL: %s"),
				FriendUID, *ConnectURL);

			// Only pass on to game thread if there's an actual URL to connect to
			if (!ConnectURL.IsEmpty())
			{
				bSuccess = TRUE;
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("RichPresenceJoinRequest: We're not a client, not changing servers! (ConnectURL: %s)"),
				*ConnectURL);
		}

		return bSuccess;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();


		// Trigger the travel to ConnectURL
		TCHAR ParsedURL[1024];

		if (!Parse(*ConnectURL, TEXT("SteamConnectIP="), ParsedURL, ARRAY_COUNT(ParsedURL)))
		{
			debugf(NAME_DevOnline, TEXT("RichPresenceJoinRequest: Failed to parse join URL"));
			return;
		}

		QWORD SteamSocketsAddr = 0;

#if WITH_STEAMWORKS_SOCKETS
		TCHAR ParsedUID[64];

		if (Parse(*ConnectURL, TEXT("SteamConnectUID="), ParsedUID, ARRAY_COUNT(ParsedUID)) && FString(ParsedUID).IsNumeric())
		{
			SteamSocketsAddr = appAtoi64(ParsedUID);
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("RichPresenceJoinRequest: No SteamConnectUID specified, server is not using steam sockets"));
		}
#endif

		// Take the parsed URL and feed it through IP address parsing, then convert it back to a string, to validate it
		FString ServerAddress = ParsedURL;
		FString CleanAddr;

		INT PortDelim = ServerAddress.InStr(TEXT(":"));
		UBOOL bValidAddress = FALSE;
		FInternetIpAddr ServerIpAddr;

		if (PortDelim != INDEX_NONE)
		{
			ServerIpAddr.SetIp(*ServerAddress.Left(PortDelim), bValidAddress);
			ServerIpAddr.SetPort(appAtoi(*ServerAddress.Mid(PortDelim+1)));

			if (bValidAddress)
			{
				CleanAddr = ServerIpAddr.ToString(TRUE);
			}
		}
		else
		{
			ServerIpAddr.SetIp(*ServerAddress, bValidAddress);

			if (bValidAddress)
			{
				CleanAddr = ServerIpAddr.ToString(FALSE);
			}
		}


		// Pass on to the invite code
		if (bValidAddress)
		{
			CallbackInterface->CachedGameInt->FindInviteGame(CleanAddr, SteamSocketsAddr);
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Got an invite with an invalid address: %s"), *ServerAddress);
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamRichPresenceJoinRequest);

/**
 * Notification event from Steam when the client connects to the Steam servers
 */
class FOnlineAsyncEventSteamServersConnected : public FOnlineAsyncEventSteam<SteamServersConnected_t, UOnlineSubsystemSteamworks>
{
private:

	/** Hidden constructor */
	FOnlineAsyncEventSteamServersConnected()
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamServersConnected(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServersConnected()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServersConnected completed"));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(SteamServersConnected_t* CallbackData)
	{
		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		debugf(NAME_DevOnline, TEXT("Steam Servers Connected"));

		OnlineSubsystemSteamworks_eventOnConnectionStatusChange_Parms ConnectionParms(EC_EventParm);
		ConnectionParms.ConnectionStatus = OSCS_Connected;

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->ConnectionStatusChangeDelegates, &ConnectionParms);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServersConnected);

/**
 * Notification event from Steam when the client disconnects from the Steam servers
 */
class FOnlineAsyncEventSteamServersDisconnected : public FOnlineAsyncEventSteam<SteamServersDisconnected_t, UOnlineSubsystemSteamworks>
{
private:
	/** The disconnection error type */
	EResult DisconnectError;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServersDisconnected()
		: DisconnectError(k_EResultOK)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamServersDisconnected(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
		, DisconnectError(k_EResultOK)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServersDisconnected()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServersDisconnected completed DisconnectError: %s"),
					*SteamResultString(DisconnectError));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(SteamServersDisconnected_t* CallbackData)
	{
		DisconnectError = CallbackData->m_eResult;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();


		debugf(NAME_DevOnline, TEXT("Steam Servers Disconnected: %s"), *SteamResultString(DisconnectError));

		OnlineSubsystemSteamworks_eventOnConnectionStatusChange_Parms ConnectionParms(EC_EventParm);
		ConnectionParms.ConnectionStatus = ConnectionResult(DisconnectError);

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->ConnectionStatusChangeDelegates, &ConnectionParms);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServersDisconnected);

/**
 * Notification event from Steam when receiving a large player avatar
 */
class FOnlineAsyncEventAvatarImageLoaded : public FOnlineAsyncEventSteam<AvatarImageLoaded_t, UOnlineSubsystemSteamworks>
{
private:
	/** The UID of the person this avatar is for */
	QWORD	OwnerUID;

	/** The index of the image, for use with the SteamSDK isteamutils 'GetImage*' functions */
	INT	ImageIndex;

	/** The width of the image */
	INT	ImageWidth;

	/** The height of the image */
	INT	ImageHeight;


	/** Hidden constructor */
	FOnlineAsyncEventAvatarImageLoaded()
		: OwnerUID(0)
		, ImageIndex(-1)
		, ImageWidth(-1)
		, ImageHeight(-1)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventAvatarImageLoaded(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
		, OwnerUID(0)
		, ImageIndex(-1)
		, ImageWidth(-1)
		, ImageHeight(-1)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventAvatarImageLoaded()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventAvatarImageLoaded completed OwnerUID: ") I64_FORMAT_TAG
			TEXT(", ImageIndex: %i, ImageWidth: %i, ImageHeight: %i"), OwnerUID, ImageIndex, ImageWidth, ImageHeight);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(AvatarImageLoaded_t* CallbackData)
	{
		OwnerUID = CallbackData->m_steamID.ConvertToUint64();
		ImageIndex = CallbackData->m_iImage;
		ImageWidth = CallbackData->m_iWide;
		ImageHeight = CallbackData->m_iTall;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		debugf(NAME_DevOnline, TEXT("AvatarImageLoaded: ") I64_FORMAT_TAG TEXT(", %d, %dx%d"), OwnerUID, ImageIndex, ImageWidth, ImageHeight);

		for (INT ReqIndex=0; ReqIndex<CallbackInterface->QueuedAvatarRequests.Num(); ReqIndex++)
		{
			FQueuedAvatarRequest& Request = CallbackInterface->QueuedAvatarRequests(ReqIndex);

			if (Request.PlayerNetId.Uid == OwnerUID)
			{
				UBOOL bGotIt = CallbackInterface->GetOnlineAvatar(Request.PlayerNetId, Request.Size,
										Request.ReadOnlineAvatarCompleteDelegate, FALSE);

				if (bGotIt)
				{
					CallbackInterface->QueuedAvatarRequests.Remove(ReqIndex);
				}
			}
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventAvatarImageLoaded);


/**
 * Notification event from Steam when receiving a large player avatar
 */
class FOnlineAsyncEventAntiCheatStatus : public FOnlineAsyncEventSteam<GSPolicyResponse_t, UOnlineSubsystemSteamworks>
{
private:
	/** Whether or not the server is marked as secure */
	UBOOL bSecure;


	/** Hidden constructor */
	FOnlineAsyncEventAntiCheatStatus()
		: bSecure(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventAntiCheatStatus(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
		, bSecure(FALSE)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventAntiCheatStatus()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventAntiCheatStatus completed bSecure: %i"), bSecure);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(GSPolicyResponse_t* CallbackData)
	{
		bSecure = CallbackData->m_bSecure;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		debugf(NAME_DevOnline, TEXT("AntiCheatStatus: VAC Secured: %i"), bSecure);

		if (CallbackInterface->CachedGameInt != NULL)
		{
			CallbackInterface->CachedGameInt->OnGSPolicyResponse(bSecure);
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventAntiCheatStatus);

/**
 * Notification event from Steam when the game server connects to the Steam servers
 */
class FOnlineAsyncEventSteamServersConnectedGameServer : public FOnlineAsyncEventSteam<SteamServersConnected_t, UOnlineSubsystemSteamworks>
{
private:

	/** Hidden constructor */
	FOnlineAsyncEventSteamServersConnectedGameServer()
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamServersConnectedGameServer(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServersConnectedGameServer()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServersConnectedGameServer completed"));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(SteamServersConnected_t* CallbackData)
	{
		// Set this value here for the online thread (TriggerDelegates sets it for the game thread)
		GSteamworksGameServerConnected = TRUE;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();

		// At this point, all session-dependent game server methods are now usable
		GSteamworksGameServerConnected = TRUE;

		// Refresh advertised game settings (important for steam sockets servers)
		UOnlineGameInterfaceSteamworks* CurGameInt = CallbackInterface->CachedGameInt;

		if (CurGameInt != NULL)
		{
			CurGameInt->RefreshPublishedGameSettings();

			if (GSteamGameServer != NULL && CurGameInt->SessionInfo != NULL)
			{
				// Update the session info
				UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
				UBOOL bSteamSockets = IsSteamSocketsServer();
				AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);

				// If this is a listen server, set the session info indirectly while setting the invite info
				if (WI != NULL && WI->NetMode == NM_ListenServer)
				{
					CallbackInterface->SetGameJoinInfo(GSteamGameServer->GetPublicIP(),
										CurGameInt->SessionInfo->HostAddr.GetPort(),
										SteamGameServer_GetSteamID(), bSteamSockets);
				}
				else
				{
					CurGameInt->UpdateSessionInfo(GSteamGameServer->GetPublicIP(), CurGameInt->SessionInfo->HostAddr.GetPort(),
									SteamGameServer_GetSteamID(), bSteamSockets);
				}
			}
		}

		if (GSteamGameServer != NULL)
		{
			debugf(TEXT("Steam game server UID: ") I64_FORMAT_TAG, GSteamGameServer->GetSteamID().ConvertToUint64());
		}


		debugf(NAME_DevOnline, TEXT("Steam Servers Connected"));

		OnlineSubsystemSteamworks_eventOnConnectionStatusChange_Parms ConnectionParms(EC_EventParm);
		ConnectionParms.ConnectionStatus = OSCS_Connected;

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->ConnectionStatusChangeDelegates, &ConnectionParms);


		// When GSteamGameServer is initially being setup, this callback is the first place where its auth functions become valid;
		// notify the auth interface
		if (CallbackInterface->CachedAuthInt != NULL)
		{
			CallbackInterface->CachedAuthInt->NotifyGameServerAuthReady();
		}

#if WITH_STEAMWORKS_SOCKETS
		// Notify the Steam sockets manager and Steam net driver that the server is fully logged on
		if (GSteamSocketsManager != NULL)
		{
			GSteamSocketsManager->InitGameServer();
		}

		UIpNetDriverSteamworks* SteamNetDriver = Cast<UIpNetDriverSteamworks>(GetActiveNetDriver());

		if (SteamNetDriver != NULL)
		{
			SteamNetDriver->InitGameServer();
		}
#endif
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServersConnectedGameServer);

/**
 * Notification event from Steam when the game server disconnects from the Steam servers
 */
class FOnlineAsyncEventSteamServersDisconnectedGameServer : public FOnlineAsyncEventSteam<SteamServersDisconnected_t, UOnlineSubsystemSteamworks>
{
private:
	/** The disconnection error type */
	EResult DisconnectError;


	/** Hidden constructor */
	FOnlineAsyncEventSteamServersDisconnectedGameServer()
		: DisconnectError(k_EResultOK)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem	The subsystem object this event is linked to
	 */
	FOnlineAsyncEventSteamServersDisconnectedGameServer(UOnlineSubsystemSteamworks* InSubsystem)
		: FOnlineAsyncEventSteam(InSubsystem)
		, DisconnectError(k_EResultOK)
	{
	}

	/**
	 * Base destructor
	 */
	virtual ~FOnlineAsyncEventSteamServersDisconnectedGameServer()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncEventSteamServersDisconnectedGameServer completed DisconnectError: %s"),
					*SteamResultString(DisconnectError));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	UBOOL ProcessSteamCallback(SteamServersDisconnected_t* CallbackData)
	{
		DisconnectError = CallbackData->m_eResult;

		// Set this value here for the online thread, to ensure it is in sync (Finalize sets it for the game thread)
		GSteamworksGameServerConnected = FALSE;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncEventSteam::Finalize();

		GSteamworksGameServerConnected = FALSE;
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncEventSteam::TriggerDelegates();

		// @todo Steam: Do you need to handle game server disconnect here?

		debugf(NAME_DevOnline, TEXT("Steam Servers Disconnected: %s"), *SteamResultString(DisconnectError));

		OnlineSubsystemSteamworks_eventOnConnectionStatusChange_Parms ConnectionParms(EC_EventParm);
		ConnectionParms.ConnectionStatus = ConnectionResult(DisconnectError);

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->ConnectionStatusChangeDelegates, &ConnectionParms);
	}
};

IMPLEMENT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServersDisconnectedGameServer);


/**
 * Asynchronous task for Steam, for receiving the result of a game server stats store request
 */
class FOnlineAsyncTaskSteamServerUserStatsStored : public FOnlineAsyncTaskSteamGameServer<GSStatsStored_t, UOnlineSubsystemSteamworks>
{
private:
	/** The SteamId of the player this result is for */
	QWORD		PlayerUID;

	/** Result of the attempted stats store */
	EResult		StatsStoredResult;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamServerUserStatsStored()
		: PlayerUID(0)
		, StatsStoredResult(k_EResultOK)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamServerUserStatsStored(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteamGameServer(InSubsystem, InCallbackHandle)
		, PlayerUID(0)
		, StatsStoredResult(k_EResultOK)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamServerUserStatsStored()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamServerUserStatsStored completed PlayerUID: ") I64_FORMAT_TAG
					TEXT(", StatsStoredResult: %s"), PlayerUID, *SteamResultString(StatsStoredResult));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOFailure		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(GSStatsStored_t* CallbackData, UBOOL bInIOFailure)
	{
		PlayerUID = CallbackData->m_steamIDUser.ConvertToUint64();
		StatsStoredResult = CallbackData->m_eResult;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteam::TriggerDelegates();


		// Generic failure
		if (bIOFailure || StatsStoredResult != k_EResultOK)
		{
			debugf(NAME_DevOnline, TEXT("Failed to store specific steam user stats for '") I64_FORMAT_TAG TEXT("', error: %s"),
				PlayerUID, *SteamResultString(StatsStoredResult));

			CallbackInterface->bGSStatsStoresSuccess = FALSE;
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Stored specific steam user stats for '") I64_FORMAT_TAG TEXT("'"), PlayerUID);
		}

		CallbackInterface->TotalGSStatsStoresPending--;

		if (CallbackInterface->TotalGSStatsStoresPending <= 0)
		{
			CallbackInterface->TotalGSStatsStoresPending = 0;

			FAsyncTaskDelegateResultsNamedSession Params(FName(TEXT("Game")), CallbackInterface->bGSStatsStoresSuccess ? S_OK : E_FAIL);
			TriggerOnlineDelegates(CallbackInterface, CallbackInterface->FlushOnlineStatsDelegates, &Params);
		}
	}
};

/**
 * Asynchronous task for Steam, for requesting and receiving the number of current people playing the game (both online and offline)
 */
class FOnlineAsyncTaskSteamNumberOfCurrentPlayers : public FOnlineAsyncTaskSteam<NumberOfCurrentPlayers_t, UOnlineSubsystemSteamworks>
{
private:
	/** The number of people playing (both online and offline) for the game, or -1 if retrieval was unsuccessful */
	INT NumPlayers;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamNumberOfCurrentPlayers()
		: NumPlayers(-1)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamNumberOfCurrentPlayers(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteam(InSubsystem, InCallbackHandle)
		, NumPlayers(-1)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamNumberOfCurrentPlayers()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamNumberOfCurrentPlayers completed NumPlayers: %i"), NumPlayers);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOFailure		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(NumberOfCurrentPlayers_t* CallbackData, UBOOL bInIOFailure)
	{
		if (!bInIOFailure && CallbackData->m_bSuccess && CallbackData->m_cPlayers >= 0)
		{
			bWasSuccessful = TRUE;
			NumPlayers = CallbackData->m_cPlayers;
		}
		else
		{
			bWasSuccessful = FALSE;
			NumPlayers = -1;
		}

		debugf(NAME_DevOnline, TEXT("Steam NumberOfCurrentPlayers: %i"), NumPlayers);

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteam::TriggerDelegates();

		OnlineSubsystemSteamworks_eventOnGetNumberOfCurrentPlayersComplete_Parms NumberParms(EC_EventParm);
		NumberParms.TotalPlayers = NumPlayers;
		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->GetNumberOfCurrentPlayersCompleteDelegates, &NumberParms);
	}
};

/**
 * Asynchronous task for Steam, for requesting and receiving a handle to a particular leaderboard
 */
class FOnlineAsyncTaskSteamFindLeaderboard : public FOnlineAsyncTaskSteam<LeaderboardFindResult_t, UOnlineSubsystemSteamworks>
{
private:
	/** The SteamAPI handle to the leaderboard */
	SteamLeaderboard_t	LeaderboardHandle;

	/** Whether or not the leaderboard was found */
	UBOOL			bFoundLeaderboard;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamFindLeaderboard()
		: LeaderboardHandle(NULL)
		, bFoundLeaderboard(FALSE)
	{
	}


public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamFindLeaderboard(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteam(InSubsystem, InCallbackHandle)
		, LeaderboardHandle(NULL)
		, bFoundLeaderboard(FALSE)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamFindLeaderboard()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamFindLeaderboard completed bFoundLeaderboard: %i"), bFoundLeaderboard);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOFailure		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(LeaderboardFindResult_t* CallbackData, UBOOL bInIOFailure)
	{
		UBOOL bSuccess = FALSE;

		LeaderboardHandle = CallbackData->m_hSteamLeaderboard;
		bFoundLeaderboard = CallbackData->m_bLeaderboardFound != 0;

		if (!bInIOFailure && bFoundLeaderboard && LeaderboardHandle != NULL)
		{
			bWasSuccessful = TRUE;
			bSuccess = TRUE;
		}
		else if (bInIOFailure)
		{
			debugf(TEXT("FindLeaderboard: Got IOFailure while finding leaderboard"));
		}
		else
		{
			debugf(TEXT("FindLeaderboard: Leaderboard not found"));
		}

		return bSuccess;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteam::Finalize();

		// Get the leaderboard name, for looking it up in 'LeaderboardList'
		FString LeaderboardName(UTF8_TO_TCHAR(GSteamUserStats->GetLeaderboardName(LeaderboardHandle)));

		INT ListIndex = INDEX_NONE;

		for (INT CurListIndex=0; CurListIndex<CallbackInterface->LeaderboardList.Num(); CurListIndex++)
		{
			if (CallbackInterface->LeaderboardList(CurListIndex).LeaderboardName == LeaderboardName)
			{
				// Copy over the OnlineSubsystem version of the name, to keep the case consistent in UScript
				LeaderboardName = CallbackInterface->LeaderboardList(CurListIndex).LeaderboardName;

				ListIndex = CurListIndex;
				break;
			}
		}

		// If the returned leaderboard is in 'LeaderboardList', store leaderboard info, and kickoff deferred read/write requests
		if (ListIndex != INDEX_NONE)
		{
			FLeaderboardTemplate& CurLeaderboard = CallbackInterface->LeaderboardList(ListIndex);

			CurLeaderboard.LeaderboardRef = LeaderboardHandle;


			// Store extra leaderboard info
			CurLeaderboard.LeaderboardSize = GSteamUserStats->GetLeaderboardEntryCount(LeaderboardHandle);

			// SortType
			ELeaderboardSortMethod APISortMethod = GSteamUserStats->GetLeaderboardSortMethod(LeaderboardHandle);

			if (APISortMethod == k_ELeaderboardSortMethodNone || APISortMethod == k_ELeaderboardSortMethodAscending)
			{
				CurLeaderboard.SortType = LST_Ascending;
			}
			else // if (APISortMethod == k_ELeaderboardSortMethodDescending)
			{
				CurLeaderboard.SortType = LST_Descending;
			}

			// DisplayFormat
			ELeaderboardDisplayType APIDisplayType = GSteamUserStats->GetLeaderboardDisplayType(LeaderboardHandle);

			if (APIDisplayType == k_ELeaderboardDisplayTypeNone || APIDisplayType == k_ELeaderboardDisplayTypeNumeric)
			{
				CurLeaderboard.DisplayFormat = LF_Number;
			}
			else if (APIDisplayType == k_ELeaderboardDisplayTypeTimeSeconds)
			{
				CurLeaderboard.DisplayFormat = LF_Seconds;
			}
			else // if (APIDisplayType == k_ELeaderboardDisplayTypeTimeMilliSeconds)
			{
				CurLeaderboard.DisplayFormat = LF_Milliseconds;
			}


			CurLeaderboard.bLeaderboardInitiated = TRUE;
			CurLeaderboard.bLeaderboardInitializing = FALSE;


			// Kickoff deferred read/write requests
			for (INT DefIndex=0; DefIndex<CallbackInterface->DeferredLeaderboardReads.Num(); DefIndex++)
			{
				FDeferredLeaderboardRead& CurDefRead = CallbackInterface->DeferredLeaderboardReads(DefIndex);

				if (CurDefRead.LeaderboardName == LeaderboardName)
				{
					UBOOL bSuccess = FALSE;

					// Don't specify playerlist, unless it actually has values; it overrides all other parameters for
					//	the leaderboard read
					if (CurDefRead.PlayerList.Num() > 0)
					{
						bSuccess = CallbackInterface->ReadLeaderboardEntries(LeaderboardName, 0, 0, 0, &CurDefRead.PlayerList);
					}
					else
					{
						bSuccess = CallbackInterface->ReadLeaderboardEntries(LeaderboardName, CurDefRead.RequestType,
													CurDefRead.Start, CurDefRead.End);
					}

					if (!bSuccess)
					{
						debugf(TEXT("FindLeaderboard: Deferred leaderboard read failed, leaderboard name: %s"),
								*LeaderboardName);
					}

					CallbackInterface->DeferredLeaderboardReads.Remove(DefIndex, 1);
					DefIndex--;
				}
			}

			for (INT DefIndex=0; DefIndex<CallbackInterface->DeferredLeaderboardWrites.Num(); DefIndex++)
			{
				FDeferredLeaderboardWrite& CurDefWrite = CallbackInterface->DeferredLeaderboardWrites(DefIndex);

				if (CurDefWrite.LeaderboardName == LeaderboardName)
				{
					if (!CallbackInterface->WriteLeaderboardScore(LeaderboardName, CurDefWrite.Score, CurDefWrite.LeaderboardData))
					{
						debugf(TEXT("FindLeaderboard: Deferred leaderboard write failed, leaderboard name: %s"),
								*LeaderboardName);
					}

					CallbackInterface->DeferredLeaderboardWrites.Remove(DefIndex, 1);
					DefIndex--;
				}
			}
		}
		else
		{
			FString LogMsg = FString::Printf(TEXT("%s '%s' %s (%s)"), TEXT("FindLeaderboard: Returned leaderboard name"),
								*LeaderboardName, TEXT("was not in 'LeaderboardList' array."),
								TEXT("ignore if triggered by CreateLeaderboard"));

			debugf(TEXT("%s"), *LogMsg);
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteam::TriggerDelegates();
	}
};

/**
 * Asynchronous task for Steam, for requesting and receiving entries for a particular leaderboard
 */
class FOnlineAsyncTaskSteamDownloadLeaderboardEntries : public FOnlineAsyncTaskSteam<LeaderboardScoresDownloaded_t, UOnlineSubsystemSteamworks>
{
private:
	/** The SteamAPI handle to the leaderboard */
	SteamLeaderboard_t		LeaderboardHandle;

	/** The SteamAPI handle for the downloaded leaderboard entries */
	SteamLeaderboardEntries_t	EntriesHandle;

	/** The number of entries downloaded */
	INT				NumEntries;


	/** Whether or not to skip 'TriggerDelegates' */
	UBOOL				bSkipDelegates;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamDownloadLeaderboardEntries()
		: LeaderboardHandle(NULL)
		, EntriesHandle(NULL)
		, NumEntries(0)
		, bSkipDelegates(FALSE)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamDownloadLeaderboardEntries(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteam(InSubsystem, InCallbackHandle)
		, LeaderboardHandle(NULL)
		, EntriesHandle(NULL)
		, NumEntries(0)
		, bSkipDelegates(FALSE)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamDownloadLeaderboardEntries()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamDownloadLeaderboardEntries completed NumEntries: %i"), NumEntries);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOFailure		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(LeaderboardScoresDownloaded_t* CallbackData, UBOOL bInIOFailure)
	{
		UBOOL bSuccess = FALSE;

		LeaderboardHandle = CallbackData->m_hSteamLeaderboard;
		EntriesHandle = CallbackData->m_hSteamLeaderboardEntries;
		NumEntries = CallbackData->m_cEntryCount;

		if (!bInIOFailure && LeaderboardHandle != NULL)
		{
			bSuccess = TRUE;
		}
		else
		{
			debugf(TEXT("DownloadLeaderboardEntries: IOFailure when reading leaderboard entries"));
		}

		return bSuccess;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteam::Finalize();

		// Default to false
		bWasSuccessful = FALSE;

		if (CallbackInterface->CurrentStatsRead != NULL)
		{
			UOnlineStatsRead* CurrentStatsRead = CallbackInterface->CurrentStatsRead;

			INT ListIndex = INDEX_NONE;
			FString StatsReadLeaderboard = CallbackInterface->LeaderboardNameLookup(CurrentStatsRead->ViewId);

			// Find the leaderboard in 'LeaderboardList', based on the leaderboard handle
			for (INT CurListIndex=0; CurListIndex<CallbackInterface->LeaderboardList.Num(); CurListIndex++)
			{
				if (CallbackInterface->LeaderboardList(CurListIndex).LeaderboardRef == LeaderboardHandle)
				{
					// Update the leaderboard entry count
					CallbackInterface->LeaderboardList(CurListIndex).LeaderboardSize =
										GSteamUserStats->GetLeaderboardEntryCount(LeaderboardHandle);
					ListIndex = CurListIndex;

					break;
				}
			}


			// Verify that the returned leaderboard entries are from an entry in 'LeaderboardList'
			if (ListIndex == INDEX_NONE)
			{
				TCHAR* LName = UTF8_TO_TCHAR(GSteamUserStats->GetLeaderboardName(LeaderboardHandle));
				debugf(TEXT("Got leaderboard read results for a leaderboard not in 'LeaderboardList', name: %s"), LName);
			}
			else if (StatsReadLeaderboard.IsEmpty())
			{
				debugf(TEXT("Got leaderboard results, when CurrentStatsRead is not reading a leaderboard"));
				bSkipDelegates = TRUE;
			}
			else if (StatsReadLeaderboard != CallbackInterface->LeaderboardList(ListIndex).LeaderboardName)
			{
				debugf(TEXT("Got leaderboard results which don't match CurrentStatsRead; Stats leaderboard: %s, results leaderboard: %s"),
				*StatsReadLeaderboard, *CallbackInterface->LeaderboardList(ListIndex).LeaderboardName);

				bSkipDelegates = TRUE;
			}
			// Early return
			else if (NumEntries == 0)
			{
				bWasSuccessful = TRUE;
			}
			else
			{
				TArray<FUniqueNetId> Players;
				TArray<FLeaderboardEntry> LeaderboardEntries;

				for (INT EntryIdx=0; EntryIdx<NumEntries; EntryIdx++)
				{
					LeaderboardEntry_t CurEntry;
					static int32 CurEntryDetails[64];

					appMemzero(&CurEntryDetails[0], sizeof(int32) * 64);

					if (GSteamUserStats->GetDownloadedLeaderboardEntry(EntriesHandle, EntryIdx, &CurEntry, CurEntryDetails, 64))
					{
						bWasSuccessful = TRUE;

						// Add to 'Players' list
						FUniqueNetId CurPlayer;
						CurPlayer.Uid = CurEntry.m_steamIDUser.ConvertToUint64();

						Players.AddItem(CurPlayer);


						// Add to 'LeaderboardEntries' list
						INT LeaderboardIdx = LeaderboardEntries.AddZeroed(1);

						LeaderboardEntries(LeaderboardIdx).PlayerUID = CurPlayer;
						LeaderboardEntries(LeaderboardIdx).Rank = CurEntry.m_nGlobalRank;
						LeaderboardEntries(LeaderboardIdx).Score = CurEntry.m_nScore;


						// Parse out the stats data from the leaderboard entry
						TArray<INT>& CurLeaderboardData = LeaderboardEntries(LeaderboardIdx).LeaderboardData;

						// Set the slack on the array, so it's not reallocating frequently on every add
						CurLeaderboardData.Empty(64);

						// Iterate the details list, and add the stats data
						for (INT DataIndex=0; DataIndex<CurEntry.m_cDetails; DataIndex++)
						{
							// Even entries are the ColumnId
							if ((DataIndex % 2) == 0)
							{
								// The ColumnId is stored as 'ColumnId+1', so if the current value is 0, there is no data
								if (CurEntryDetails[DataIndex] == 0)
								{
									break;
								}

								CurLeaderboardData.AddItem(CurEntryDetails[DataIndex]-1);
							}
							// Odd entries are the stat data
							else
							{
								CurLeaderboardData.AddItem(CurEntryDetails[DataIndex]);
							}
						}

						// Get rid of any unused slack
						CurLeaderboardData.Shrink();
					}
				}

				// If successful, parse the results into CurrentStatsRead
				if (bWasSuccessful)
				{
					UBOOL bHadColumnWarning = FALSE;

					for (INT EntryIdx=0; EntryIdx<LeaderboardEntries.Num(); EntryIdx++)
					{
						INT RowIndex = CurrentStatsRead->Rows.AddZeroed();
						FOnlineStatsRow& Row = CurrentStatsRead->Rows(RowIndex);

						Row.PlayerID = LeaderboardEntries(EntryIdx).PlayerUID;
						Row.Rank.SetData((INT)LeaderboardEntries(EntryIdx).Rank);
						Row.NickName = TEXT("");

						if (GSteamFriends != NULL)
						{
							Row.NickName = UTF8_TO_TCHAR(GSteamFriends->GetFriendPersonaName(CSteamID(Row.PlayerID.Uid)));
						}


						// Copy the stored stats data into CurrentStatsRead
						Row.Columns.AddZeroed(CurrentStatsRead->ColumnIds.Num());

						TArray<INT>& CurLeaderboardData = LeaderboardEntries(EntryIdx).LeaderboardData;

						for (INT DataIdx=0; DataIdx<CurLeaderboardData.Num()-1; DataIdx += 2)
						{
							INT ColumnId = CurLeaderboardData(DataIdx);
							INT StatValue = CurLeaderboardData(DataIdx+1);
							INT FieldIdx = CurrentStatsRead->ColumnIds.FindItemIndex(ColumnId);

							if (FieldIdx == INDEX_NONE)
							{
								bHadColumnWarning = TRUE;

								debugf(NAME_DevOnline,
									TEXT("ColumnId '%i' in leaderboard data, not found in CurrentStatsRead"));

								continue;
							}


							FOnlineStatsColumn& Col = Row.Columns(FieldIdx);
							Col.ColumnNo = ColumnId;
							Col.StatValue.SetData(StatValue);
						}
					}

					// If there were log messages warning about missing ColumnId's, spit out another log entry clarifying that message
					if (bHadColumnWarning)
					{
						FString LogMsg = FString::Printf(TEXT("%s %s %s"),
							TEXT("NOTE: The ColumnId warnings above are most often caused by a mismatch between the"),
							TEXT("OnlineStatsWrite used to write leaderboard data, and the OnlineStatsRead used to read"),
							TEXT("leaderboard data."));

						debugf(NAME_DevOnline, *LogMsg);

						LogMsg = FString::Printf(TEXT("%s %s"),
							TEXT("For example, if build 1 of your game writes leaderboard data and build 2 changes the"),
							TEXT("leaderboard layout, and tries to read the old build 1 data."));

						debugf(NAME_DevOnline, *LogMsg);
					}
				}
			}
		}
		else
		{
			debugf(TEXT("Got leaderboard results, when CurrentStatsRead is NULL"));
			bSkipDelegates = TRUE;
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteam::TriggerDelegates();

		if (!bSkipDelegates)
		{
			if (!bWasSuccessful)
			{
				debugf(TEXT("DownloadeLeaderboardEntries: ReadOnlineStats (leaderboard) has failed"));
			}

			CallbackInterface->CurrentStatsRead = NULL;

			FAsyncTaskDelegateResults Param(bWasSuccessful ? S_OK : E_FAIL);
			TriggerOnlineDelegates(CallbackInterface, CallbackInterface->ReadOnlineStatsCompleteDelegates, &Param);
		}
	}
};

/**
 * Asynchronous task for Steam, for uploading scores to a leaderboard
 */
class FOnlineAsyncTaskSteamUploadLeaderboardScore : public FOnlineAsyncTaskSteam<LeaderboardScoreUploaded_t, UOnlineSubsystemSteamworks>
{
private:
	/** The SteamAPI handle to the leaderboard */
	SteamLeaderboard_t		LeaderboardHandle;


	// NOTE: Not used within UE3, but can be accessed by enabling this code and the code in ProcessSteamCallback
#if 0
	/** The score that was uploaded */
	INT				UploadedScore;

	/** Whether or not the Leaderboard score changed */
	UBOOL				bScoreChanged;

	/** The new rank of the player on the leaderboard */
	INT				NewRank;

	/** The old rank of the player on the leaderboard (before the score was changed) */
	INT				OldRank;
#endif


	/** Hidden constructor */
	FOnlineAsyncTaskSteamUploadLeaderboardScore()
		: LeaderboardHandle(NULL)
		// NOTE: Not used by UE3, but can be re-enabled if access is desired
#if 0
		, UploadedScore(-1)
		, bScoreChanged(FALSE)
		, NewRank(-1)
		, OldRank(-1)
#endif
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamUploadLeaderboardScore(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteam(InSubsystem, InCallbackHandle)
		, LeaderboardHandle(NULL)
		// NOTE: Not used by UE3, but can be re-enabled if access is desired
#if 0
		, UploadedScore(-1)
		, bScoreChanged(FALSE)
		, NewRank(-1)
		, OldRank(-1)
#endif
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamUploadLeaderboardScore()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamUploadLeaderboardScore completed"));

#if 0
		return FString::Printf(
			TEXT("FOnlineAsyncTaskSteamUploadLeaderboardScore completed UploadedScore: %i, bScoreChanged: %i, NewRank: %i, OldRank: %i"),
			UploadedScore, bScoreChanged, NewRank, OldRank);
#endif
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOFailure		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(LeaderboardScoreUploaded_t* CallbackData, UBOOL bInIOFailure)
	{
		UBOOL bSuccess = FALSE;

		LeaderboardHandle = CallbackData->m_hSteamLeaderboard;

		// NOTE: Not used by UE3, but can be re-enabled if access is desired
#if 0
		UploadedScore = CallbackData->m_nScore;
		bScoreChanged = CallbackData->m_bScoreChanged != 0;
		NewRank = CallbackData->m_nGlobalRankNew;
		OldRank = CallbackData->m_nGlobalRankPrevious;
#endif

		if (!bInIOFailure && LeaderboardHandle != NULL)
		{
			bWasSuccessful = TRUE;
			bSuccess = TRUE;
		}
		else
		{
			debugf(TEXT("IOFailure when writing leaderboard score"));
		}

		return bSuccess;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteam::Finalize();

		INT ListIndex = INDEX_NONE;

		// Find the leaderboard in 'LeaderboardList', based on the leaderboard handle
		for (INT CurListIndex=0; CurListIndex<CallbackInterface->LeaderboardList.Num(); CurListIndex++)
		{
			if (CallbackInterface->LeaderboardList(CurListIndex).LeaderboardRef == LeaderboardHandle)
			{
				ListIndex = CurListIndex;
				break;
			}
		}

		// Update the leaderboard entry count
		if (ListIndex != INDEX_NONE)
		{
			debugf(NAME_DevOnline, TEXT("Successfully updated score for leaderboard '%s'"),
				*CallbackInterface->LeaderboardList(ListIndex).LeaderboardName);

			CallbackInterface->LeaderboardList(ListIndex).LeaderboardSize = GSteamUserStats->GetLeaderboardEntryCount(LeaderboardHandle);
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteam::TriggerDelegates();

		// NOTE: There is no script notification of failure to write leaderboard data
	}
};

/**
 * Asynchronous task for Steam, for requesting and receiving remote storage UGC file handles
 */
class FOnlineAsyncTaskSteamRemoteStorageFileShareDownload : public FOnlineAsyncTaskSteam<RemoteStorageDownloadUGCResult_t, UOnlineSubsystemSteamworks>
{
private:
	/** The result of the download request */
	EResult		DownloadResult;

	/** The SteamAPI handle for the downloaded file */
	UGCHandle_t	FileHandle;

	/** The appid this file was created in */
	DWORD		FileAppId;

	/** The size of the file */
	INT		FileSize;

	/** The name of the file */
	FString		FileName;

	/** The UID of the person who created the file */
	QWORD		FileOwner;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamRemoteStorageFileShareDownload()
		: DownloadResult(k_EResultOK)
		, FileHandle(NULL)
		, FileAppId(k_uAppIdInvalid)
		, FileSize(-1)
		, FileOwner(0)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamRemoteStorageFileShareDownload(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteam(InSubsystem, InCallbackHandle)
		, DownloadResult(k_EResultOK)
		, FileHandle(NULL)
		, FileAppId(k_uAppIdInvalid)
		, FileSize(-1)
		, FileOwner(0)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamRemoteStorageFileShareDownload()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(
			TEXT("FOnlineAsyncTaskSteamRemoteStorageFileShareDownload completed Result: %s, FileAppId: %d, FileSize: %i, FileOwner: ")
			I64_FORMAT_TAG /** << NOTE: part of above string, not a parameter */,
			*SteamResultString(DownloadResult), FileAppId, FileSize, FileOwner);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOFailure		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(RemoteStorageDownloadUGCResult_t* CallbackData, UBOOL bInIOFailure)
	{
		DownloadResult = CallbackData->m_eResult;
		FileHandle = CallbackData->m_hFile;
		FileAppId = CallbackData->m_nAppID;
		FileSize = CallbackData->m_nSizeInBytes;
		FileName = UTF8_TO_TCHAR(CallbackData->m_pchFileName);
		FileOwner = CallbackData->m_ulSteamIDOwner;

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteam::Finalize();

		bWasSuccessful = (!bIOFailure && DownloadResult == k_EResultOK) ? TRUE : FALSE;

		if (bWasSuccessful)
		{
			if (FileSize > 0 && FileSize <= k_unMaxCloudFileSize)
			{
				FString SharedHandle = UGCHandleToString(FileHandle);
				FTitleFile* SharedFile = CallbackInterface->GetSharedCloudFile(SharedHandle);
				if (SharedFile)
				{
					SharedFile->Data.Empty(FileSize);
					SharedFile->Data.Add(FileSize);

					// This call only works once per call to UGCDownload()
					if (GSteamRemoteStorage->UGCRead(FileHandle, SharedFile->Data.GetData(), SharedFile->Data.Num()) == FileSize)
					{
						SharedFile->AsyncState = OERS_Done;
					}
					else
					{
						SharedFile->AsyncState = OERS_Failed;
						SharedFile->Data.Empty();
						bWasSuccessful = FALSE;
					}
				}
				else
				{
					bWasSuccessful = FALSE;
				}
			}
		}
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteam::TriggerDelegates();

		OnlineSubsystemSteamworks_eventOnReadSharedFileComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = bWasSuccessful ? FIRST_BITFIELD : 0;
		Parms.SharedHandle = UGCHandleToString(FileHandle);

		debugf(NAME_DevOnline, TEXT("RemoteStorageFileShareDownload: Result: %s Handle: %s"), *SteamResultString(DownloadResult),
			*Parms.SharedHandle);

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->SharedFileReadCompleteDelegates, &Parms);
	}
};

/**
 * Asynchronous task for Steam, for sharing UGC files
 */
class FOnlineAsyncTaskSteamRemoteStorageFileShareRequest : public FOnlineAsyncTaskSteam<RemoteStorageFileShareResult_t, UOnlineSubsystemSteamworks>
{
private:
	/** The result of the file share request */
	EResult		ShareResult;

	/** The shareable handle for the file */
	UGCHandle_t	ShareHandle;


	/** The UserId which made the share request (passed in through constructor) */
	FString		UserId;

	/** The filename of the shared file */
	FString		Filename;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamRemoteStorageFileShareRequest()
		: ShareResult(k_EResultOK)
		, ShareHandle(NULL)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 * @param InUserId		The UserId which made the share request
	 * @param InFilename		The filename of the shared file
	 */
	FOnlineAsyncTaskSteamRemoteStorageFileShareRequest(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle,
								FString InUserId, FString InFilename)
		: FOnlineAsyncTaskSteam(InSubsystem, InCallbackHandle)
		, ShareResult(k_EResultOK)
		, ShareHandle(NULL)
		, UserId(InUserId)
		, Filename(InFilename)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamRemoteStorageFileShareRequest()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamRemoteStorageFileShareRequest completed ShareResult: %s, UserId: %s, Filename: %s"),
			*SteamResultString(ShareResult), *UserId, *Filename);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOFailure		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(RemoteStorageFileShareResult_t* CallbackData, UBOOL bInIOFailure)
	{
		ShareResult = CallbackData->m_eResult;
		ShareHandle = CallbackData->m_hFile;

		if (!bInIOFailure && ShareResult == k_EResultOK)
		{
			bWasSuccessful = TRUE;
		}
		else
		{
			bWasSuccessful = FALSE;
		}

		return TRUE;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteam::TriggerDelegates();

		// Trigger delegates
		OnlineSubsystemSteamworks_eventOnWriteSharedFileComplete_Parms Parms(EC_EventParm);

		Parms.bWasSuccessful = bWasSuccessful;
		Parms.UserId = UserId;
		Parms.Filename = Filename;
		Parms.SharedHandle = UGCHandleToString(ShareHandle);

		debugf(NAME_DevOnline, TEXT("RemoteStorageFileShareRequest: Result: %s File: %s Handle: %s"),
			*SteamResultString(ShareResult), *Filename, *Parms.SharedHandle);

		TriggerOnlineDelegates(CallbackInterface, CallbackInterface->SharedFileWriteCompleteDelegates, &Parms);
	}
};

// Not implemented yet
#if 0
/**
 * Asynchronous task for Steam, for requesting and receiving global stats data
 */
class FOnlineAsyncTaskSteamGlobalStatsRequest : public FOnlineAsyncTaskSteam<GlobalStatsReceived_t, UOnlineSubsystemSteamworks>
{
private:
	/** The AppId of the game global stats were returned for */
	QWORD	GameID;

	/** The result of the global stats request */
	EResult	RequestResult;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamGlobalStatsRequest()
		: GameID(0)
		, RequestResult(k_EResultOK)

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamGlobalStatsRequest(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteam(InSubsystem, InCallbackHandle)
		, GameID(0)
		, RequestResult(k_EResultOK)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamGlobalStatsRequest()
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamGlobalStatsRequest completed GameID: ") I64_FORMAT_TAG
					TEXT(", RequestResult: %s"), GameID,
			*SteamResultString(RequestResult));
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOFailure		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(GlobalStatsReceived_t* CallbackData, UBOOL bInIOFailure)
	{
		UBOOL bSuccess = FALSE;

		GameID = CallbackData->m_nGameID;
		RequestResult = CallbackData->m_eResult;

		return bSuccess;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize()
	{
		FOnlineAsyncTaskSteam::Finalize();
	}

	/**
	 * Async task is given a chance to trigger its delegates
	 */
	virtual void TriggerDelegates()
	{
		FOnlineAsyncTaskSteam::TriggerDelegates();
	}
};
#endif


/**
 * UOnlineSubsystemSteamworks implementation
 */

/**
 * Starts an async query for the total players. This is the amount of players the system thinks is playing right now, globally,
 *	not just on a specific server.
 *
 * @return	TRUE if async call started, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::GetNumberOfCurrentPlayers()
{
	if (GSteamAsyncTaskManager != NULL && GSteamUserStats != NULL)
	{
		SteamAPICall_t ApiCall = GSteamUserStats->GetNumberOfCurrentPlayers();
		GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamNumberOfCurrentPlayers(this, ApiCall));
	}

	return TRUE;
}

/**
 * Get the Clan tags for the current user.
 *
 * This functionality is currently OnlineSubsystemSteamworks specific, and the API will change to be
 *  more general if it is moved into the parent class.
 */
void UOnlineSubsystemSteamworks::GetSteamClanData(TArray<struct FSteamPlayerClanData>& Results)
{
	Results.Empty();
	if (GSteamFriends != NULL)
	{
		const int ClanCount = GSteamFriends->GetClanCount();
		for (int ClanIndex = 0; ClanIndex < ClanCount; ClanIndex++)
		{
			const INT Position = Results.Add();
			FSteamPlayerClanData &Data = Results(Position);
			const CSteamID ClanId(GSteamFriends->GetClanByIndex(ClanIndex));
			Data.ClanName = UTF8_TO_TCHAR(GSteamFriends->GetClanName(ClanId));
			Data.ClanTag = UTF8_TO_TCHAR(GSteamFriends->GetClanTag(ClanId));
		}
	}
}


/*-----------------------------------------------------------------------------
 UserCloudFileInterface implementation.

- Steam Cloud has some limitations
-- Check your per-user storage quota
-- Files are case-insensitive
-- Files are handled as complete blocks of memory, there is no seek/tell for example

- There is no file sync while the game is running; you have the latest versions on-disk when you launch
-- The external Steam client will handle uploading once you terminate (any writes during your game go to a disk cache).
-- The only exception is when calling FileShare

- Please make sure your game has Steam Cloud enabled on Valve's partner website, or all read/writes will fail!

-----------------------------------------------------------------------------*/

/** 
 * **INTERNAL**
 * Get the metadata related to a given user's file on Steam
 * This information is only available after calling EnumerateUserFiles
 *
 * @param UserId the UserId owning the file to search for
 * @param Filename the file to get metadata about
 * @return the struct with the metadata about the requested file, NULL otherwise
 *
 */
FEmsFile* UOnlineSubsystemSteamworks::GetUserCloudMetadataFile(const FString& UserId, const FString& Filename)
{
	if (Filename.Len() > 0)
	{
		// Search for the specified file
		FSteamUserCloudMetadata* UserMetadata = GetUserCloudMetadata(UserId);
		if (UserMetadata)
		{
			for (INT FileIdx = 0; FileIdx < UserMetadata->UserCloudMetadata.Num(); FileIdx++)
			{
				FEmsFile* UserFileData = &UserMetadata->UserCloudMetadata(FileIdx);
				if (UserFileData &&
					UserFileData->Filename == Filename)
				{
					return UserFileData;
				}
			}
		}
	}

	return NULL;
}

/** 
 * **INTERNAL**
 * Get the metadata related to a given user
 * This information is only available after calling EnumerateUserFiles
 *
 * @param UserId the UserId to search for
 * @return the struct with the metadata about the requested user, will always return a valid struct, creating one if necessary
 *
 */
FSteamUserCloudMetadata* UOnlineSubsystemSteamworks::GetUserCloudMetadata(const FString& UserId)
{
	for (INT UserIdx=0; UserIdx<UserCloudMetadata.Num(); UserIdx++)
	{
		FSteamUserCloudMetadata* UserMetadata = &UserCloudMetadata(UserIdx);
		if (UserMetadata->UserId == UserId)
		{
			return UserMetadata;
		}
	}

	// Always create a new one if it doesn't exist
	INT UserIdx = UserCloudMetadata.AddZeroed();
	UserCloudMetadata(UserIdx).UserId = UserId;
	return &UserCloudMetadata(UserIdx);
}

/** 
 * **INTERNAL**
 * Clear the metadata related to a given user's file on Steam
 * This information is only available after calling EnumerateUserFiles
 * It doesn't actually delete any of the actual data on disk
 *
 * @param UserId the UserId for the file to search for
 * @param Filename the file to get metadata about
 * @return the true if the delete was successful, false otherwise
 *
 */
UBOOL UOnlineSubsystemSteamworks::ClearUserCloudMetadata(const FString& UserId, const FString& Filename)
{
	if (Filename.Len() > 0)
	{
		// Search for the specified file
		FSteamUserCloudMetadata* UserMetadata = GetUserCloudMetadata(UserId);
		if (UserMetadata)
		{
			INT FoundIndex = INDEX_NONE;
			for (INT FileIdx = 0; FileIdx < UserMetadata->UserCloudMetadata.Num(); FileIdx++)
			{
				FEmsFile* UserFileData = &UserMetadata->UserCloudMetadata(FileIdx);
				if (UserFileData &&
					UserFileData->Filename == Filename)
				{
					FoundIndex = FileIdx;
					break;
				}
			}

			if (FoundIndex != INDEX_NONE)
			{
				UserMetadata->UserCloudMetadata.Remove(FoundIndex);
			}
		}
	}

	return TRUE;
}

/**
 * **INTERNAL**
 * Get physical/logical file information for a given user's cloud file
 *
 * @param UserID the UserId owning the file to search for
 * @param Filename the file to search for
 * @return the file details, NULL otherwise
 *
 */
FTitleFile* UOnlineSubsystemSteamworks::GetUserCloudDataFile(const FString& UserId, const FString& Filename)
{
	if (Filename.Len() > 0)
	{
		// Search for the specified file
		FSteamUserCloud* SteamUserCloud = GetUserCloudData(UserId);
		if (SteamUserCloud)
		{
			for (INT FileIdx = 0; FileIdx < SteamUserCloud->UserCloudFileData.Num(); FileIdx++)
			{
				FTitleFile* UserFileData = &SteamUserCloud->UserCloudFileData(FileIdx);
				if (UserFileData &&
					UserFileData->Filename == Filename)
				{
					return UserFileData;
				}
			}
		}
	}

	return NULL;
}

/** 
 * **INTERNAL**
 * Get physical/logical file information for all given user's cloud files
 * This information is only available after calling EnumerateUserFiles
 *
 * @param UserId the UserId for the file to search for
 * @return the struct with the metadata about the requested user, will always return a valid struct, creating one if necessary
 *
 */
FSteamUserCloud* UOnlineSubsystemSteamworks::GetUserCloudData(const FString& UserId)
{
	// Search for the specified file
	for (INT UserIdx = 0; UserIdx < UserCloudFiles.Num(); UserIdx++)
	{
		FSteamUserCloud* SteamUserCloud = &UserCloudFiles(UserIdx);
		if (SteamUserCloud->UserId == UserId)
		{
			return SteamUserCloud;
		}
	}

	// Always create a new one if it doesn't exist
	INT UserIdx = UserCloudFiles.AddZeroed();
	UserCloudFiles(UserIdx).UserId = UserId;
	return &UserCloudFiles(UserIdx);
}

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::GetFileContents(const FString& UserId, const FString& Filename, TArray<BYTE>& FileContents)
{
	// Search for the specified file and return the raw data
	FTitleFile* SteamCloudFile = GetUserCloudDataFile(UserId, Filename);
	if (SteamCloudFile && SteamCloudFile->AsyncState == OERS_Done && SteamCloudFile->Data.Num() > 0)
	{
		FileContents = SteamCloudFile->Data;
		return TRUE;
	}
	return FALSE;
}

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @param UserId User owning the storage
 *
 * @return true if they could be deleted, false if they could not
 */
UBOOL UOnlineSubsystemSteamworks::ClearFiles(const FString& UserId)
{
	// Search for the specified file
	FSteamUserCloud* SteamUserCloud = GetUserCloudData(UserId);
	if (SteamUserCloud)
	{
		for (INT FileIdx = 0; FileIdx < SteamUserCloud->UserCloudFileData.Num(); FileIdx++)
		{
			FTitleFile* UserFileData = &SteamUserCloud->UserCloudFileData(FileIdx);
			// If there is an async task outstanding, fail to empty
			if (UserFileData->AsyncState == OERS_InProgress)
			{
				return FALSE;
			}
		}

		// No async files being handled, so empty them all
		SteamUserCloud->UserCloudFileData.Empty();
	}

	return TRUE;
}

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
UBOOL UOnlineSubsystemSteamworks::ClearFile(const FString& UserId, const FString& Filename)
{
	if (Filename.Len() > 0)
	{
		INT FoundIndex = INDEX_NONE;

		// Search for the specified file
		FSteamUserCloud* SteamUserCloud = GetUserCloudData(UserId);
		if (SteamUserCloud)
		{
			for (INT FileIdx = 0; FileIdx < SteamUserCloud->UserCloudFileData.Num(); FileIdx++)
			{
				FTitleFile* UserFileData = &SteamUserCloud->UserCloudFileData(FileIdx);
				if (UserFileData->Filename == Filename)
				{
					// If there is an async task outstanding, fail to empty
					if (UserFileData->AsyncState == OERS_InProgress)
					{
						return FALSE;
					}
					FoundIndex = FileIdx;
					break;
				}
			}

			if (FoundIndex != INDEX_NONE)
			{
				SteamUserCloud->UserCloudFileData.Remove(FoundIndex);
			}
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Requests a list of available User files from the network store
 *
 * @param UserId User owning the storage
 */
void UOnlineSubsystemSteamworks::EnumerateUserFiles(const FString& UserId)
{
	UBOOL bSuccess = FALSE;

	if (GSteamRemoteStorage)
	{
		FUniqueNetId InUserId(EC_EventParm);
		if (StringToUniqueNetId(UserId, InUserId))
		{
			if (LoggedInPlayerId.HasValue() && LoggedInPlayerId == InUserId)
			{
				PrintSteamCloudStorageInfo();

				// Get or create the user metadata entry and empty it
				FSteamUserCloudMetadata* UserMetadata = GetUserCloudMetadata(UserId);
				UserMetadata->UserCloudMetadata.Empty();

				// Fill in the metadata entries
				const INT FileCount = (INT) GSteamRemoteStorage->GetFileCount();
				UserMetadata->UserCloudMetadata.AddZeroed(FileCount);
				for (INT FileIdx = 0; FileIdx < FileCount; FileIdx++)
				{
					int32 FileSize = 0;
					const char *Filename = GSteamRemoteStorage->GetFileNameAndSize(FileIdx, &FileSize);

					UserMetadata->UserCloudMetadata(FileIdx).FileSize = INT(FileSize);
					UserMetadata->UserCloudMetadata(FileIdx).Filename = UTF8_TO_TCHAR(Filename);
					UserMetadata->UserCloudMetadata(FileIdx).DLName = UTF8_TO_TCHAR(Filename);
					UserMetadata->UserCloudMetadata(FileIdx).Hash = FString(TEXT("0"));
				}

				bSuccess = TRUE;
			}
			else
			{
			   debugf(NAME_DevOnline,TEXT("Can only enumerate cloud files for logged in user."));
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Failed to parse UserId to valid SteamId."));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Steam remote storage API disabled."));
	}

	// Trigger delegates
	OnlineSubsystemSteamworks_eventOnEnumerateUserFilesComplete_Parms Parms(EC_EventParm);
	Parms.bWasSuccessful = bSuccess ? FIRST_BITFIELD : 0;
	Parms.UserId = UserId;
	TriggerOnlineDelegates(this,EnumerateUserFilesCompleteDelegates,&Parms);
}

/**
 * Returns the list of User files that was returned by the network store
 * 
 * @param UserId User owning the storage
 * @param UserFiles out array of file metadata
 */
void UOnlineSubsystemSteamworks::GetUserFileList(const FString& UserId, TArray<struct FEmsFile>& UserFiles)
{
	FSteamUserCloudMetadata* UserMetadata = GetUserCloudMetadata(UserId);  
	UserFiles = UserMetadata->UserCloudMetadata;
}

/**
 * Starts an asynchronous read of the specified user file from the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadUserFile(const FString& UserId, const FString& Filename)
{
	UBOOL bSuccess = FALSE;
	if (GSteamRemoteStorage && Filename.Len() > 0)
	{
		FUniqueNetId InUserId(EC_EventParm);
		if (StringToUniqueNetId(UserId, InUserId))
		{
			if (LoggedInPlayerId.HasValue() && LoggedInPlayerId == InUserId)
			{
				const INT FileSize = GSteamRemoteStorage->GetFileSize(TCHAR_TO_UTF8(*Filename));
				if (FileSize >= 0 && FileSize <= k_unMaxCloudFileSize)
				{
					// Create or get the current entry for this file
					FTitleFile* UserCloudFile = GetUserCloudDataFile(UserId, Filename);
					if (UserCloudFile == NULL)
					{
						FSteamUserCloud* SteamUserCloud = GetUserCloudData(UserId);
						if (SteamUserCloud)
						{
							INT FileIdx = SteamUserCloud->UserCloudFileData.AddZeroed();
							UserCloudFile = &SteamUserCloud->UserCloudFileData(FileIdx);
							UserCloudFile->Filename = Filename;
						}
					}

					// Allocate and read in the file
					UserCloudFile->Data.Empty(FileSize);
					UserCloudFile->Data.Add(FileSize);
					if (GSteamRemoteStorage->FileRead(TCHAR_TO_UTF8(*Filename), UserCloudFile->Data.GetData(), FileSize) == FileSize)
					{
						UserCloudFile->AsyncState = OERS_Done;
						bSuccess = TRUE;
					}
					else
					{
					   UserCloudFile->Data.Empty();
					   UserCloudFile->AsyncState = OERS_Failed;
					}
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("Requested file %s has invalid size %d."), *Filename, FileSize);
				}
			}	
			else
			{
				debugf(NAME_DevOnline,TEXT("Can only read cloud files for logged in user."));
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Failed to parse UserId to valid SteamId."));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Steam remote storage API disabled."));
	}

	// Trigger delegates
	OnlineSubsystemSteamworks_eventOnReadUserFileComplete_Parms Parms(EC_EventParm);
	Parms.bWasSuccessful = bSuccess ? FIRST_BITFIELD : 0;
	Parms.UserId = UserId;
	Parms.Filename = Filename;
	TriggerOnlineDelegates(this,ReadUserFileCompleteDelegates,&Parms);

	return bSuccess;
}

/**
 * **INTERNAL**
 * Starts an asynchronous write of the specified user file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToWrite the name of the file to write
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::WriteUserFileInternal(const FString& UserId,const FString& Filename,const TArray<BYTE>& Contents)
{
	UBOOL bSuccess = FALSE;
	if (Filename.Len() > 0 && Contents.Num() > 0)
	{
		if (GSteamRemoteStorage)
		{
			FUniqueNetId InUserId(EC_EventParm);
			if (StringToUniqueNetId(UserId, InUserId))
			{
				if (LoggedInPlayerId.HasValue() && LoggedInPlayerId == InUserId)
				{
					PrintSteamCloudStorageInfo();

					if (Contents.Num() < k_unMaxCloudFileSize)
					{
						if (GSteamRemoteStorage->FileWrite(TCHAR_TO_UTF8(*Filename), Contents.GetData(), Contents.Num()))
						{
							// Update the metadata table to reflect this write (might be new entry)
							FEmsFile* UserMetadataFile = GetUserCloudMetadataFile(UserId, Filename);
							if (UserMetadataFile == NULL)
							{
								FSteamUserCloudMetadata* UserMetadata = GetUserCloudMetadata(UserId);
								if (UserMetadata)
								{
									INT FileIdx = UserMetadata->UserCloudMetadata.AddZeroed();
									UserMetadataFile = &UserMetadata->UserCloudMetadata(FileIdx);
									UserMetadataFile->DLName = Filename;
									UserMetadataFile->Filename = Filename;
								}
							}

							UserMetadataFile->FileSize = GSteamRemoteStorage->GetFileSize(TCHAR_TO_UTF8(*Filename));
							UserMetadataFile->Hash = FString(TEXT("0"));

							// Update the file table to reflect this write
							FTitleFile* UserCloudFile = GetUserCloudDataFile(UserId, Filename);
							if (UserCloudFile == NULL)
							{
								FSteamUserCloud* SteamUserCloud = GetUserCloudData(UserId);
								if (SteamUserCloud)
								{
									INT FileIdx = SteamUserCloud->UserCloudFileData.AddZeroed();
									UserCloudFile = &SteamUserCloud->UserCloudFileData(FileIdx);
									UserCloudFile->Filename = Filename;
								}
							}

							UserCloudFile->Data = Contents;
							UserCloudFile->AsyncState = OERS_Done;
							bSuccess = TRUE;

							PrintSteamFileState(Filename);
						}
						else
						{
							debugf(NAME_DevOnline,TEXT("Failed to write file to Steam cloud \"%s\"."), *Filename);
						}
					}
					else
					{
						debugf(NAME_DevOnline,TEXT("File too large %d to write to Steam cloud."), Contents.Num());
					}
				}
				else
				{
					debugf(NAME_DevOnline,TEXT("Can only write cloud files for logged in user."));
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Failed to parse UserId to valid SteamId."));
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Steam remote storage API disabled."));
		}
	}

	return bSuccess;
}

/**
 * Starts an asynchronous write of the specified user file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToWrite the name of the file to write
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::WriteUserFile(const FString& UserId,const FString& Filename,const TArray<BYTE>& Contents)
{
	UBOOL bSuccess = WriteUserFileInternal(UserId, Filename, Contents);

	// Trigger delegates
	OnlineSubsystemSteamworks_eventOnWriteUserFileComplete_Parms Parms(EC_EventParm);
	Parms.bWasSuccessful = bSuccess ? FIRST_BITFIELD : 0;
	Parms.UserId = UserId;
	Parms.Filename = Filename;
	TriggerOnlineDelegates(this,WriteUserFileCompleteDelegates,&Parms);

	return bSuccess;
}

/**
 * Starts an asynchronous delete of the specified user file from the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToRead the name of the file to read
 * @param bShouldCloudDelete whether to delete the file from the cloud
 * @param bShouldLocallyDelete whether to delete the file locally
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::DeleteUserFile(const FString& UserId,const FString& Filename,UBOOL bShouldCloudDelete,UBOOL bShouldLocallyDelete)
{
	UBOOL bSuccess = FALSE;

	if (GSteamRemoteStorage && Filename.Len() > 0)
	{
		FUniqueNetId InUserId(EC_EventParm);
		if (StringToUniqueNetId(UserId, InUserId))
		{
			if (LoggedInPlayerId.HasValue() && LoggedInPlayerId == InUserId)
			{
				UBOOL bCloudDeleteSuccess = TRUE;
				if (bShouldCloudDelete)
				{
					// Remove the cloud flag, the file remains safely available on the local machine
					bCloudDeleteSuccess = GSteamRemoteStorage->FileForget(TCHAR_TO_UTF8(*Filename));
				}

				UBOOL bLocalDeleteSuccess = TRUE;
				if (bShouldLocallyDelete)
				{
					bLocalDeleteSuccess = FALSE;
					// Only clear the tables if we're permanently deleting the file
					// Need to make sure nothing async is happening	first (this is a formality as nothing in Steam actually is)
					if (ClearFile(UserId, Filename))
					{
						// Permanent delete
						bLocalDeleteSuccess = GSteamRemoteStorage->FileDelete(TCHAR_TO_UTF8(*Filename));
						ClearUserCloudMetadata(UserId, Filename);
					}
				}

				bSuccess = bCloudDeleteSuccess && bLocalDeleteSuccess;
			}	
			else
			{
				debugf(NAME_DevOnline,TEXT("Can only delete cloud files for logged in user."));
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Failed to parse UserId to valid SteamId."));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Steam remote storage API disabled."));
	}

	// Trigger delegates
	OnlineSubsystemSteamworks_eventOnDeleteUserFileComplete_Parms Parms(EC_EventParm);
	Parms.bWasSuccessful = bSuccess ? FIRST_BITFIELD : 0;
	Parms.UserId = UserId;
	Parms.Filename = Filename;
	TriggerOnlineDelegates(this,DeleteUserFileCompleteDelegates,&Parms);

	return bSuccess;
}


/*-----------------------------------------------------------------------------
 SharedCloudFileInterface implementation.

- Steam Cloud has some limitations
-- Check your per-user storage quota
-- Files are case-insensitive
-- Files are handled as complete blocks of memory, there is no seek/tell for example

-----------------------------------------------------------------------------*/

/** 
 * **INTERNAL**
 * Get the file entry related to a shared download
 *
 * @param SharedHandle the handle to search for
 * @return the struct with the metadata about the requested shared data, will always return a valid struct, creating one if necessary
 */
FTitleFile* UOnlineSubsystemSteamworks::GetSharedCloudFile(const FString& SharedHandle)
{
	for (INT FileIdx=0; FileIdx<SharedFileCache.Num(); FileIdx++)
	{
		FTitleFile* SharedFile = &SharedFileCache(FileIdx);
		if (SharedFile->Filename == SharedHandle)
		{
			return SharedFile;
		}
	}

	// Always create a new one if it doesn't exist
	INT FileIdx = SharedFileCache.AddZeroed();
	SharedFileCache(FileIdx).Filename = SharedHandle;
	return &SharedFileCache(FileIdx);
}

/**
 * Copies the shared data into the specified buffer for the specified file
 *
 * @param SharedHandle the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::GetSharedFileContents(const FString& SharedHandle,TArray<BYTE>& FileContents)
{
	FTitleFile* SharedFile = GetSharedCloudFile(SharedHandle);
	if (SharedFile && SharedFile->AsyncState == OERS_Done && SharedFile->Data.Num() > 0)
	{
		FileContents = SharedFile->Data;
		return TRUE;
	}
	else
	{
		FileContents.Empty();
	}

	return FALSE;
}

/**
 * Empties the set of all downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
UBOOL UOnlineSubsystemSteamworks::ClearSharedFiles()
{
	for (INT FileIdx=0; FileIdx<SharedFileCache.Num(); FileIdx++)
	{
		const FTitleFile& SharedFile = SharedFileCache(FileIdx);
		// If there is an async task outstanding, fail to empty
		if (SharedFile.AsyncState != OERS_InProgress)
		{
			return FALSE;
		}
	}

	SharedFileCache.Empty();
	return TRUE;
}

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param SharedHandle the name of the file to read
 *
 * @return true if it could be deleted, false if it could not
 */
UBOOL UOnlineSubsystemSteamworks::ClearSharedFile(const FString& SharedHandle)
{
	for (INT FileIdx=0; FileIdx<SharedFileCache.Num(); FileIdx++)
	{
		const FTitleFile& SharedFile = SharedFileCache(FileIdx);
		if (SharedFile.Filename == SharedHandle)
		{
			// If there is an async task outstanding, fail to empty
			if (SharedFile.AsyncState != OERS_InProgress)
			{
				SharedFileCache.Remove(FileIdx);
				return TRUE;
			}
			break;
		}
	}

	return FALSE;
}

/**
 * Starts an asynchronous read of the specified shared file from the network platform's file store
 *
 * @param SharedHandle the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadSharedFile(const FString& SharedHandle)
{
	UBOOL bSuccess = FALSE;
	if (GSteamRemoteStorage && SharedHandle.Len() > 0)
	{
		if (GSteamUser->BLoggedOn())
		{
			UGCHandle_t SteamHandle;
			if (StringToUGCHandle(SharedHandle, SteamHandle))
			{
				// Create the entry to hold the data
				FTitleFile* SharedFile = GetSharedCloudFile(SharedHandle);
				if (SharedFile)
				{
					SharedFile->AsyncState = OERS_InProgress;

					SteamAPICall_t ApiCall = GSteamRemoteStorage->UGCDownload(SteamHandle);
					GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamRemoteStorageFileShareDownload(this, ApiCall));

					bSuccess = TRUE;
				}
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Unable to parse Steam handle."));
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Steam user not logged in."));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Steam remote storage API disabled."));
	}

	return bSuccess;
}

/**
 * Starts an asynchronous write of the specified shared file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param Filename the name of the file to write
 * @param Contents data to write to the file
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::WriteSharedFile(const FString& UserId,const FString& Filename,const TArray<BYTE>& Contents)
{
	UBOOL bSuccess = FALSE;
	if (GSteamRemoteStorage && Filename.Len() > 0)
	{
		if (GSteamUser->BLoggedOn())
		{
			if (WriteUserFileInternal(UserId, Filename, Contents))
			{
				SteamAPICall_t ApiCall = GSteamRemoteStorage->FileShare(TCHAR_TO_UTF8(*Filename));
				GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamRemoteStorageFileShareRequest(this, ApiCall,
										UserId, Filename));


				bSuccess = TRUE;
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Steam user not logged in."));
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Steam remote storage API disabled."));
	}

	return bSuccess;
}


/**
 * Generates the field name used by Steam stats, based on a view id and a column id
 *
 * @param StatsRead holds the definitions of the tables to read the data from
 * @param ViewId the view to read from
 * @param ColumnId the column to read.  If 0, then this is just the view's column (flag indicating if the view has been written to)
 * @return the name of the field
 */
FString UOnlineSubsystemSteamworks::GetStatsFieldName(INT ViewId, INT ColumnId)
{
	return FString::Printf(TEXT("%d_%d"), ViewId, ColumnId);
}

/**
 * Cleanup stuff that happens outside of UObject's view
 */
void UOnlineSubsystemSteamworks::FinishDestroy()
{
#if STEAM_EXEC_DEBUG
	if (GSteamExecCatcher != NULL)
	{
		delete GSteamExecCatcher;
		GSteamExecCatcher = NULL;
	}
#endif

	// Let the steam sockets manager know it should destroy (it may need to wait for sockets to shutdown first though)
	if (GSteamSocketsManager != NULL)
	{
		GSteamSocketsManager->NotifyDestroy();
	}

	if (GSteamAsyncTaskManager != NULL)
	{
		GSteamAsyncTaskManager->UnregisterInterface(this);
		GSteamAsyncTaskManager->Stop();

		GSteamAsyncTaskManager = NULL;
	}


	Super::FinishDestroy();
}

/**
 * Clears the various data that is associated with a player to prevent the data being used across logins
 */
void UOnlineSubsystemSteamworks::ClearPlayerInfo()
{
	CachedProfile = NULL;
	LoggedInPlayerId = FUniqueNetId((QWORD)0);
	LoggedInStatus = LS_NotLoggedIn;
	LoggedInPlayerName.Empty();
	CachedFriendMessages.Empty();
	CachedGameInt->SetInviteInfo(NULL);
//@todo joeg -- add more as they come up
}

/**
 * Steamworks specific implementation. Sets the supported interface pointers and
 * initilizes the voice engine
 *
 * @return always returns TRUE
 */
UBOOL UOnlineSubsystemSteamworks::Init()
{
	Super::Init();

	// Set the initial state so we don't trigger an event unnecessarily
	bLastHasConnection = GSocketSubsystem->HasNetworkDevice();

	// Set to the localized default
	LoggedInPlayerName = LocalProfileName;
	LoggedInPlayerNum = -1;
	LoggedInStatus = LS_NotLoggedIn;
	LoggedInPlayerId = FUniqueNetId((QWORD)0);

	const UBOOL bIsServer = HasCmdLineToken(TEXT("SERVER")); 

	// do the Steamworks initialization
	InitSteamworks();

	// Set the interface used for account creation
	eventSetAccountInterface(this);

	// Set the player interface to be the same as the object
	eventSetPlayerInterface(this);

	// Set the extended player interface to be the same as the object
	eventSetPlayerInterfaceEx(this);

	// Construct the object that handles the game interface for us
	CachedGameInt = ConstructObject<UOnlineGameInterfaceSteamworks>(UOnlineGameInterfaceSteamworks::StaticClass(), this);

	GameInterfaceImpl = CachedGameInt;

	if (GameInterfaceImpl != NULL)
	{
		CachedGameInt->InitInterface(this);
		eventSetGameInterface(GameInterfaceImpl);
	}

	// Set the stats reading/writing interface
	eventSetStatsInterface(this);
	eventSetSystemInterface(this);

	// Set the user cloud interface to be the same as the object
	eventSetUserCloudInterface(this);

	// Set the shared cloud interface to be the same as the object
	eventSetSharedCloudInterface(this);

	// Create the voice engine and if successful register the interface
	VoiceEngine = appCreateVoiceInterface(MaxLocalTalkers, MaxRemoteTalkers, bIsUsingSpeechRecognition);

	if (VoiceEngine != NULL)
	{
		eventSetVoiceInterface(this);
	}
	else
	{
		// Don't show this warning when this is a server running without the Steam client
		if (!bIsServer || GSteamworksClientInitialized)
		{
			debugf(NAME_DevOnline, TEXT("Failed to create the voice interface"));
		}
	}

	// Use MCP services for news or not
	if (bShouldUseMcp)
	{
		UOnlineNewsInterfaceMcp* NewsObject = ConstructObject<UOnlineNewsInterfaceMcp>(UOnlineNewsInterfaceMcp::StaticClass(), this);
		eventSetNewsInterface(NewsObject);
	}

#if STEAM_MATCHMAKING_LOBBY
	// Set the lobby interface
	UOnlineLobbyInterfaceSteamworks* NewLobbyInterface =
		ConstructObject<UOnlineLobbyInterfaceSteamworks>(UOnlineLobbyInterfaceSteamworks::StaticClass(), this);

	if (NewLobbyInterface != NULL)
	{
		NewLobbyInterface->InitInterface(this);
		eventSetLobbyInterface(NewLobbyInterface);
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Failed to create the lobby interface"));
	}
#endif

	// Set the auth interface (only if steam is initialized though; UScript uses presence of auth interface to determine if auth is supported)
	if (GSteamworksClientInitialized || GSteamworksGameServerInitialized)
	{
		CachedAuthInt = ConstructObject<UOnlineAuthInterfaceSteamworks>(UOnlineAuthInterfaceSteamworks::StaticClass(), this);
		AuthInterfaceImpl = CachedAuthInt;

		if (CachedAuthInt != NULL)
		{
			CachedAuthInt->InitInterface(this);
			eventSetAuthInterface(CachedAuthInt);
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Failed to create the auth interface"));
		}
	}

	// Set the default toast location
	SetNetworkNotificationPosition(CurrentNotificationPosition);

	// Sign in locally to the default account if necessary.
	SignInLocally();

#if STEAM_EXEC_DEBUG
	GSteamExecCatcher = new FSteamExecCatcher(this);
#endif

	// If required, initialize the steam sockets manager, if it is not already initialized
	if (GSteamSocketsManager == NULL && IsSteamNetDriverEnabled())
	{
		GSteamSocketsManager = new FSteamSocketsManager();
		GSteamSocketsManager->InitializeSocketsManager();
	}


	// Kickstart rich presence; this should cause all friends presence info to be downloaded upon startup (doesn't happen otherwise)
	// @todo Steam: Determine if this works, and go back to the other PRESENCE_FIX if it doesn't
	if (GSteamFriends != NULL)
	{
		GSteamFriends->SetRichPresence(TCHAR_TO_UTF8(TEXT("Dud")), TCHAR_TO_UTF8(TEXT("0")));
	}

	return GameInterfaceImpl != NULL;
}

/**
 * Called from the engine shutdown code, to allow the online subsystem to cleanup
 */
void UOnlineSubsystemSteamworks::Exit()
{
	// Stub (required for compile)
}

extern "C" 
{ 
	static void __cdecl SteamworksWarningMessageHook(int Severity, const char* Message); 
	static void __cdecl SteamworksWarningMessageHookNoOp(int Severity, const char* Message);
}

/** 
 * Callback function into Steam error messaging system
 *
 * @param Severity - error level
 * @param Message - message from Steam
 */
static void __cdecl SteamworksWarningMessageHook(int Severity, const char* Message)
{
	const TCHAR *MessageType;

	switch (Severity)
	{
		case 0:
			MessageType = TEXT("message");
			break;

		case 1:
			MessageType = TEXT("warning");
			break;

		default:
			MessageType = TEXT("notification");
			break;  // Unknown severity; new SDK?
	}

	debugf(NAME_DevOnline, TEXT("Steamworks SDK %s: %s"), MessageType, UTF8_TO_TCHAR(Message));
}

/** 
 * Callback function into Steam error messaging system that outputs nothing
 * @param Severity - error level
 * @param Message - message from Steam
 */
static void __cdecl SteamworksWarningMessageHookNoOp(int Severity, const char *Message)
{
	// no-op.
}

FORCEINLINE const TCHAR* GetSteamUniverseName(const EUniverse SteamUniverse)
{
	switch (SteamUniverse)
	{
		case k_EUniverseInvalid: return TEXT("INVALID");
		case k_EUniversePublic: return TEXT("PUBLIC");
		case k_EUniverseBeta: return TEXT("BETA");
		case k_EUniverseInternal: return TEXT("INTERNAL");
		case k_EUniverseDev: return TEXT("DEV");
	}
	return TEXT("???");
}

/**
 * Initializes Steamworks
 *
 * @return	TRUE if successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::InitSteamworks()
{
	// We had to do the initial Steamworks init (SteamAPI_Init()) in UnMisc.cpp, near the start of the process. Check if it worked out here.

	if (GSteamworksClientInitialized)
	{
		debugf(TEXT("Initializing Steamworks"));

		#define GET_STEAMWORKS_INTERFACE(Interface) \
			if ((G##Interface = Interface()) == NULL) \
			{ \
				debugf(NAME_DevNet, TEXT("Steamworks: %s() failed!"), TEXT(#Interface)); \
				GSteamworksClientInitialized = FALSE; \
			} \

		// NOTE: If you add a new interface here, add a comment with it's full name (e.g. GSteamNetworking) above it, or code searches
		//		become a headache

		// GSteamUtils
		GET_STEAMWORKS_INTERFACE(SteamUtils);

		// GSteamUser
		GET_STEAMWORKS_INTERFACE(SteamUser);

		// GSteamFriends
		GET_STEAMWORKS_INTERFACE(SteamFriends);

		// GSteamRemoteStorage
		GET_STEAMWORKS_INTERFACE(SteamRemoteStorage);

		// GSteamUserStats
		GET_STEAMWORKS_INTERFACE(SteamUserStats);

		// GSteamMatchmakingServers
		GET_STEAMWORKS_INTERFACE(SteamMatchmakingServers);

		// GSteamApps
		GET_STEAMWORKS_INTERFACE(SteamApps);

		// GSteamNetworking
		GET_STEAMWORKS_INTERFACE(SteamNetworking);

		// GSteamMatchmaking
		GET_STEAMWORKS_INTERFACE(SteamMatchmaking);

		#undef GET_STEAMWORKS_INTERFACE
	}

	if (GSteamUtils)
	{
		GSteamAppID = GSteamUtils->GetAppID();
		AppID = GSteamAppID;

		GSteamUtils->SetWarningMessageHook(SteamworksWarningMessageHook);
	}
	else if (GSteamGameServerUtils != NULL)
	{
		GSteamAppID = GSteamGameServerUtils->GetAppID();
		GSteamGameServerUtils->SetWarningMessageHook(SteamworksWarningMessageHook);
	}

	UserStatsReceivedState = OERS_NotStarted;

	if (GSteamworksClientInitialized || GSteamworksGameServerInitialized)
	{
		// Create the online async task manager, and start up the online thread
		GSteamAsyncTaskManager = new FOnlineAsyncTaskManagerSteam();
		GThreadFactory->CreateThread(GSteamAsyncTaskManager, TEXT("SteamOnlineThread"), TRUE, TRUE);

		// Register with the async task manager
		GSteamAsyncTaskManager->RegisterInterface(this);
	}

	if (!GSteamworksClientInitialized)
	{
		if (HasCmdLineToken(TEXT("SERVER")))
		{
			debugf(TEXT("Steam Client API is unavailable (not required for servers)"));
		}
		else
		{
			debugf(TEXT("Steam Client API is unavailable"));
			debugf(NAME_DevNet, TEXT("This usually means you're not running the Steam client, your Steam client is incompatible, or you don't have a proper steam_appid.txt"));
			debugf(NAME_DevNet, TEXT("We will continue without OnlineSubsystem support."));
		}

		OnlineSubsystemSteamworks_eventOnConnectionStatusChange_Parms ConnectionParms(EC_EventParm);
		ConnectionParms.ConnectionStatus = OSCS_ServiceUnavailable;
		TriggerOnlineDelegates(this,ConnectionStatusChangeDelegates,&ConnectionParms);
		return TRUE;
	}

	// Check and display Steam remote storage quota information
	int32 SteamCloudTotal = 0;
	int32 SteamCloudAvailable = 0;
	if (!GSteamRemoteStorage || !GSteamRemoteStorage->GetQuota(&SteamCloudTotal, &SteamCloudAvailable))
	{
		SteamCloudTotal = SteamCloudAvailable = -1;
	}

#if !FORCELOWGORE
	GForceLowGore = GSteamApps->BIsLowViolence();
#endif

	debugf(NAME_DevOnline, TEXT("Steam ID: ") I64_FORMAT_TAG, GSteamUser->GetSteamID().ConvertToUint64() );
	debugf(NAME_DevOnline, TEXT("Steam universe: %s"), GetSteamUniverseName(GSteamUtils->GetConnectedUniverse()));
	debugf(NAME_DevOnline, TEXT("Steam appid: %u"), GSteamAppID);
	debugf(NAME_DevOnline, TEXT("Steam IsSubscribed: %i"), (INT)GSteamApps->BIsSubscribed());
	debugf(NAME_DevOnline, TEXT("Steam IsLowViolence: %i"), (INT)GSteamApps->BIsLowViolence());
	debugf(NAME_DevOnline, TEXT("Steam IsCybercafe: %i"), (INT)GSteamApps->BIsCybercafe());
	debugf(NAME_DevOnline, TEXT("Steam IsVACBanned: %i"), (INT)GSteamApps->BIsVACBanned());
	debugf(NAME_DevOnline, TEXT("Steam IP country: %s"), ANSI_TO_TCHAR(GSteamUtils->GetIPCountry()));
	debugf(NAME_DevOnline, TEXT("Steam official server time: %u"), (DWORD) GSteamUtils->GetServerRealTime());
	debugf(NAME_DevOnline, TEXT("Steam Cloud quota: %d / %d"), (INT) SteamCloudAvailable, (INT) SteamCloudTotal);

	if (GSteamUser->BLoggedOn())
	{
		const char *PersonaName = GSteamFriends->GetPersonaName();
		LoggedInPlayerName = FString(UTF8_TO_TCHAR(PersonaName));
		LoggedInPlayerNum = 0;
		LoggedInPlayerId.Uid = (QWORD) GSteamUser->GetSteamID().ConvertToUint64();
		LoggedInStatus = LS_LoggedIn;
		debugf(TEXT("Logged in as '%s'"), *LoggedInPlayerName);

		OnlineSubsystemSteamworks_eventOnConnectionStatusChange_Parms ConnectionParms(EC_EventParm);
		ConnectionParms.ConnectionStatus = OSCS_Connected;
		TriggerOnlineDelegates(this,ConnectionStatusChangeDelegates,&ConnectionParms);

		// We don't have a login screen, so just tell the game's delegates we logged in here.
		OnlineSubsystemSteamworks_eventOnLoginChange_Parms LoginParms(EC_EventParm);
		LoginParms.LocalUserNum = LoggedInPlayerNum;
		TriggerOnlineDelegates(this,LoginChangeDelegates,&LoginParms);
	}

	// Disable usage of relays with P2P networking
	if (GSteamNetworking != NULL)
	{
		GSteamNetworking->AllowP2PPacketRelay(FALSE);
	}

	return TRUE;
}

/**
 * Notification sent to OnlineSubsystem, that pre-travel cleanup is occuring
 *
 * @param bSessionEnded		Whether or not the game session has ended
 */
void UOnlineSubsystemSteamworks::NotifyCleanupWorld(UBOOL bSessionEnded)
{
	// Pass on notification to the VOIP subsystem, so it can cleanup audio components
	if (VoiceEngine != NULL)
	{
		FVoiceInterfaceSteamworks* VoiceEngineOSS = (FVoiceInterfaceSteamworks*)VoiceEngine;

		if (VoiceEngineOSS != NULL)
		{
			VoiceEngineOSS->NotifyCleanupWorld(bSessionEnded);
		}
	}
}


/**
 * Handles updating of any async tasks that need to be performed
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemSteamworks::TickVoice(FLOAT DeltaTime)
{
	if (VoiceEngine)
	{
		// Process VoIP data
		ProcessLocalVoicePackets();
		ProcessRemoteVoicePackets();
		// Let it do any async processing
		VoiceEngine->Tick(DeltaTime);
		// Fire off the events that script cares about
		ProcessTalkingDelegates();
		ProcessSpeechRecognitionDelegates();
	}
}


/**
 * Handles updating of any async tasks that need to be performed
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemSteamworks::Tick(FLOAT DeltaTime)
{
	// Check for there not being a logged in user, since we require at least a default user
	if (LoggedInStatus == LS_NotLoggedIn)
	{
		SignInLocally();
	}

	// Tick any tasks needed for LAN/networked game support
	TickGameInterfaceTasks(DeltaTime);

	// Let voice do any processing
	TickVoice(DeltaTime);
	TickConnectionStatusChange(DeltaTime);

	// Tick the Steamworks Core (may run callbacks!)
	if (IsSteamClientAvailable() || IsSteamServerAvailable())
	{
		TickSteamworksTasks(DeltaTime);
	}
}


/**
 * Allows the Steamworks code to perform their Think operations
 */
void UOnlineSubsystemSteamworks::TickSteamworksTasks(FLOAT DeltaTime)
{
	if (GSteamAsyncTaskManager != NULL)
	{
		GSteamAsyncTaskManager->GameTick();
	}

	if (IsSteamClientAvailable())
	{
		// Wait for GameInviteAcceptedDelegates to be set, before triggering the commandline-invite processing code
		static UBOOL bStartupInvitesProcessed = FALSE;

		if (!bStartupInvitesProcessed && CachedGameInt != NULL && CachedGameInt->GameInviteAcceptedDelegates.Num() > 0)
		{
			bStartupInvitesProcessed = TRUE;

			UBOOL bCommandlineJoin = FALSE;
			FString ServerAddr;
			QWORD ServerSteamSocketsAddr = 0;


			// Join commandline triggered by Steam UI join
			if (GSteamCmdLineSet)
			{
				bCommandlineJoin = TRUE;

				QWORD FriendUIDDud = 0;

				// Steam UI invites (which use +connect on the commandline) only specify an IP; since invites come from friends,
				//	see if there is a friend on the server, with rich presence info containing the server UID
				if (GetInviteFriend(GSteamCmdLineConnect, FriendUIDDud, ServerSteamSocketsAddr) && ServerSteamSocketsAddr != 0)
				{
					debugf(NAME_DevOnline,
						TEXT("Steam commandline join: Found server steam sockets address in friend 'rich presence' info: ")
						I64_FORMAT_TAG, ServerSteamSocketsAddr);
				}

				if (GSteamCmdLineConnect.GetPort() != 0)
				{
					ServerAddr = GSteamCmdLineConnect.ToString(TRUE);
				}
				else
				{
					ServerAddr = GSteamCmdLineConnect.ToString(FALSE);
				}

				// @todo Steam: GSteamCmdLinePassword? (don't plan on implementing this atm, just scrape it off the cmdline for now)
			}
			// Join commandline triggered by Steam presence join
			else
			{
				FString PresenceServerAddr;
				FString PresenceServerUID;

				if (GetCommandlineJoinURL(TRUE, PresenceServerAddr, PresenceServerUID) && !PresenceServerAddr.IsEmpty())
				{
					// Validate the address by putting it through IP parsing
					FInternetIpAddr ValidatedServerAddr;
					INT PortDelim = PresenceServerAddr.InStr(TEXT(":"));
					UBOOL bValidAddress = FALSE;

					if (PortDelim != INDEX_NONE)
					{
						ValidatedServerAddr.SetIp(*PresenceServerAddr.Left(PortDelim), bValidAddress);
						ValidatedServerAddr.SetPort(appAtoi(*PresenceServerAddr.Mid(PortDelim+1)));

						if (bValidAddress)
						{
							ServerAddr = ValidatedServerAddr.ToString(TRUE);
						}
					}
					else
					{
						ValidatedServerAddr.SetIp(*PresenceServerAddr, bValidAddress);

						if (bValidAddress)
						{
							ServerAddr = ValidatedServerAddr.ToString(FALSE);
						}
					}

					if (bValidAddress)
					{
						bCommandlineJoin = TRUE;

						if (!PresenceServerUID.IsEmpty())
						{
							ServerSteamSocketsAddr = appAtoi64(*PresenceServerUID);
						}
					}
				}
			}


			if (bCommandlineJoin && CachedGameInt != NULL)
			{
				// Pass on to invite code (does not skip if above check fails, since delegate could be set before this returns)
				CachedGameInt->FindInviteGame(ServerAddr, ServerSteamSocketsAddr);
			}
		}


		// See if there are any avatar request retries pending.
		for (INT Index = 0; Index < QueuedAvatarRequests.Num(); Index++)
		{
			FQueuedAvatarRequest &Request = QueuedAvatarRequests(Index);
			Request.CheckTime += (FLOAT)DeltaTime;

			if (Request.CheckTime >= 1.0f)  // make sure Steam had time to recognize this guy.
			{
				Request.CheckTime = 0.0f;  // reset timer for next try.
				Request.NumberOfAttempts++;
				const UBOOL bLastTry = (Request.NumberOfAttempts >= 60);  // 60 seconds is probably more than enough.
				const UBOOL bGotIt = GetOnlineAvatar(Request.PlayerNetId, Request.Size,
									Request.ReadOnlineAvatarCompleteDelegate, bLastTry);

				if (bGotIt || bLastTry)
				{
					QueuedAvatarRequests.Remove(Index);  // done either way
					Index--;  // so we don't skip items...
				}
			}
		}


#if PRESENCE_FIX
		static FLOAT PresenceFixCounter = 0.0f;

		// Works around an issue, where friends rich presence data doesn't update upon launching the game,
		//	until SetRichPresence is called again
		if (GSteamFriends != NULL)
		{
			PresenceFixCounter += DeltaTime;

			if (PresenceFixCounter >= 10.0f)
			{
				PresenceFixCounter = 0.0f;

				CSteamID LocalId(LoggedInPlayerId.Uid);

				if (GSteamFriends->GetFriendRichPresenceKeyCount(LocalId) > 0)
				{
					// @todo Steam: Remove this debug code
					debugf(NAME_DevOnline, TEXT("Bumping rich presence"));

					GSteamFriends->SetRichPresence(TCHAR_TO_UTF8(TEXT("Dud")), TCHAR_TO_UTF8(TEXT("0")));
				}
			}
		}
#endif

		static FLOAT CheckIPCCallsTime = 0.0f;
		CheckIPCCallsTime += DeltaTime;

		if (CheckIPCCallsTime >= 60.0f)
		{
			debugf(NAME_DevOnline, TEXT("Steam IPC calls in the last minute: %u"), (DWORD)GSteamUtils->GetIPCCallCount());
			CheckIPCCallsTime = 0.0f;
		}
	}
}

/**
 * Ticks the connection checking code
 *
 * @param DeltaTime the amount of time that has elapsed since the last check
 */
void UOnlineSubsystemSteamworks::TickConnectionStatusChange(FLOAT DeltaTime)
{
	ConnectionPresenceElapsedTime += DeltaTime;
	// See if the connection needs to be checked
	if (ConnectionPresenceElapsedTime > ConnectionPresenceTimeInterval)
	{
		// Compare the connection to the last check
		UBOOL bHasConnection = GSocketSubsystem->HasNetworkDevice();
		if (bHasConnection != bLastHasConnection)
		{
			// They differ so notify the game code
			OnlineSubsystemSteamworks_eventOnLinkStatusChange_Parms Parms(EC_EventParm);
			Parms.bIsConnected = bHasConnection ? FIRST_BITFIELD : 0;
			TriggerOnlineDelegates(this,LinkStatusDelegates,&Parms);
		}
		bLastHasConnection = bHasConnection;
		ConnectionPresenceElapsedTime = 0.f;
	}
}

/**
 * Logs the player into the online service. If this fails, it generates a
 * OnLoginFailed notification
 *
 * @param LocalUserNum the controller number of the associated user
 * @param LoginName the unique identifier for the player
 * @param Password the password for this account
 * @param bWantsLocalOnly whether the player wants to sign in locally only or not
 *
 * @return true if the async call started ok, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::Login(BYTE LocalUserNum, const FString& LoginName, const FString& Password, UBOOL bWantsLocalOnly)
{
	UBOOL bSignedInOk = FALSE;

	if (!IsSteamClientAvailable())
	{
		bWantsLocalOnly = TRUE;
	}

	if (bWantsLocalOnly == FALSE)
	{
		if (GSteamUser->BLoggedOn())
		{
			const char* PersonaName = GSteamFriends->GetPersonaName();

			LoggedInPlayerName = FString(UTF8_TO_TCHAR(PersonaName));
			LoggedInPlayerNum = 0;
			LoggedInPlayerId.Uid = (QWORD) GSteamUser->GetSteamID().ConvertToUint64();
			LoggedInStatus = LS_LoggedIn;

			debugf(NAME_DevOnline, TEXT("Logged in as '%s'"), *LoggedInPlayerName);

			// Stash which player is the active one
			LoggedInPlayerNum = LocalUserNum;
			bSignedInOk = TRUE;

			// Trigger the delegates so the UI can process
			OnlineSubsystemSteamworks_eventOnLoginChange_Parms Parms(EC_EventParm);
			Parms.LocalUserNum = 0;

			TriggerOnlineDelegates(this, LoginChangeDelegates, &Parms);
		}
		else
		{
			ClearPlayerInfo();

			OnlineSubsystemSteamworks_eventOnLoginFailed_Parms Params(EC_EventParm);
			Params.LocalUserNum = LocalUserNum;
			Params.ErrorCode = OSCS_NotConnected;

			TriggerOnlineDelegates(this, LoginFailedDelegates, &Params);
		}
	}
	else
	{
		FString BackupLogin = LoggedInPlayerName;

		// Temporarily swap the names to see if the profile exists
		LoggedInPlayerName = LoginName;

		if (DoesProfileExist())
		{
			ClearPlayerInfo();

			// Yay. Login worked
			LoggedInPlayerNum = LocalUserNum;
			LoggedInStatus = LS_UsingLocalProfile;
			LoggedInPlayerName = LoginName;
			bSignedInOk = TRUE;

			debugf(NAME_DevOnline, TEXT("Signing into profile %s locally"), *LoggedInPlayerName);

			// Trigger the delegates so the UI can process
			OnlineSubsystemSteamworks_eventOnLoginChange_Parms Parms2(EC_EventParm);
			Parms2.LocalUserNum = 0;

			TriggerOnlineDelegates(this, LoginChangeDelegates, &Parms2);
		}
		else
		{
			// Restore the previous log in name
			LoggedInPlayerName = BackupLogin;

			OnlineSubsystemSteamworks_eventOnLoginFailed_Parms Params(EC_EventParm);
			Params.LocalUserNum = LocalUserNum;
			Params.ErrorCode = OSCS_NotConnected;

			TriggerOnlineDelegates(this, LoginFailedDelegates, &Params);
		}
	}

	return bSignedInOk;
}

/**
 * Logs the player into the online service using parameters passed on the
 * command line. Expects -Login=<UserName> -Password=<password>. If either
 * are missing, the function returns false and doesn't start the login
 * process
 *
 * @return true if the async call started ok, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::AutoLogin(void)
{
	return FALSE;
}

/**
 * Logs the player into the default account
 */
void UOnlineSubsystemSteamworks::SignInLocally()
{
	if (!IsSteamClientAvailable() || (GSteamUser != NULL && !GSteamUser->BLoggedOn()))
	{
		LoggedInPlayerName = GetClass()->GetDefaultObject<UOnlineSubsystemSteamworks>()->LocalProfileName;

		debugf(NAME_DevOnline, TEXT("Signing into the local profile %s"), *LoggedInPlayerName);

		LoggedInPlayerNum = 0;
		LoggedInStatus = LS_UsingLocalProfile;

		// Trigger the delegates so the UI can process
		OnlineSubsystemSteamworks_eventOnLoginChange_Parms Parms2(EC_EventParm);
		Parms2.LocalUserNum = 0;

		TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms2);
	}
}

/**
 * Fetches the login status for a given player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the enum value of their status
 */
BYTE UOnlineSubsystemSteamworks::GetLoginStatus(BYTE LocalUserNum)
{
	if (!IsSteamClientAvailable() || LocalUserNum != LoggedInPlayerNum)
	{
		return LS_NotLoggedIn;
	}

	return ((GSteamUser != NULL && GSteamUser->BLoggedOn()) ? LS_LoggedIn : LS_NotLoggedIn);
}

/**
 * Checks that a unique player id is part of the specified user's friends list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player being checked
 *
 * @return TRUE if a member of their friends list, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::IsFriend(BYTE LocalUserNum,FUniqueNetId PlayerID)
{
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_NotLoggedIn)
	{
		// Ask Steam if they are on the buddy list
		const CSteamID SteamPlayerID((uint64) PlayerID.Uid);
		return (GSteamFriends->GetFriendRelationship(SteamPlayerID) == k_EFriendRelationshipFriend);
	}

	return FALSE;
}

/**
 * Checks that whether a group of player ids are among the specified player's
 * friends
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Query an array of players to check for being included on the friends list
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */

UBOOL UOnlineSubsystemSteamworks::AreAnyFriends(BYTE LocalUserNum,TArray<FFriendsQuery>& Query)
{
	UBOOL bReturn = FALSE;
	// Steamworks doesn't have a bulk check so check one at a time
	for (INT Index = 0; Index < Query.Num(); Index++)
	{
		FFriendsQuery& FriendQuery = Query(Index);
		if (IsFriend(LocalUserNum,FriendQuery.UniqueId))
		{
			FriendQuery.bIsFriend = TRUE;
			bReturn = TRUE;
		}
	}
	return bReturn;
}

/**
 * Reads a set of stats for the specified list of players
 *
 * @param Players the array of unique ids to read stats for
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadOnlineStats(const TArray<FUniqueNetId>& Players, UOnlineStatsRead* StatsRead)
{
	UBOOL Result = FALSE;
	UBOOL bSkipCallback = FALSE;

	if (CurrentStatsRead != NULL)
	{
		debugf(NAME_DevOnline, TEXT("Can't perform a stats read when one is in progress"));

		// Don't kill the current stats read by issuing callbacks for this
		bSkipCallback = TRUE;
	}
	else if (StatsRead == NULL)
	{
		debugf(NAME_DevOnline, TEXT("ReadOnlineStats: Input StatsRead is NULL"));
	}
	else if (Players.Num() == 0)
	{
		debugf(NAME_DevOnline, TEXT("ReadOnlineStats: No players specified"));
	}
	else if (!IsSteamClientAvailable() && !IsSteamServerAvailable())
	{
		debugf(NAME_DevOnline, TEXT("ReadOnlineStats: Steamworks not initialized"));
	}
	else
	{
		CurrentStatsRead = StatsRead;

		// Clear previous results
		CurrentStatsRead->Rows.Empty();

		FString LeaderboardName = LeaderboardNameLookup(CurrentStatsRead->ViewId);

		// Leaderboard stats read, for a list of players
		if (!LeaderboardName.IsEmpty())
		{
			if (ReadLeaderboardEntries(LeaderboardName, 0, 0, 0, &Players))
			{
				Result = TRUE;
			}
			else
			{
				debugf(TEXT("ReadOnlineStats: Call to 'ReadLaderboardEntries has failed"));
			}
		}
		else
		{
			UBOOL bGameServerStats = GameServerStatsMappings.ContainsItem(CurrentStatsRead->ViewId);

			// Setup the request
			for (INT i=0; i<Players.Num(); i++)
			{
				const CSteamID SteamPlayerId((uint64)Players(i).Uid);

				// Game server stats, use the game server stats interface
				if (bGameServerStats)
				{
					if (GSteamGameServerStats != NULL)
					{
						// Server side stats read of "server written" stats
						SteamAPICall_t ApiCall =GSteamGameServerStats->RequestUserStats(SteamPlayerId);
						GSteamAsyncTaskManager->AddToInQueue(
										new FOnlineAsyncTaskSteamServerUserStatsReceived(this, ApiCall));

						Result = TRUE;
					}
					else if (GSteamUserStats != NULL)
					{
						debugf(NAME_DevOnline, TEXT("ReadOnlineStats: Warning! Reading game server stats on client"));

						SteamAPICall_t ApiCall = GSteamUserStats->RequestUserStats(SteamPlayerId);
						GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamUserStatsReceived(this, ApiCall));


						Result = TRUE;
					}
					else
					{
						debugf(NAME_DevOnline, TEXT("Game server stats read failed; GSteamGameServerStats == NULL"));
					}
				}
				else if (GSteamUserStats != NULL)
				{
					// Client side stats read of "client written" stats
					SteamAPICall_t ApiCall = GSteamUserStats->RequestUserStats(SteamPlayerId);
					GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamUserStatsReceived(this, ApiCall));

					Result = TRUE;
				}
				else if (GSteamGameServerStats != NULL)
				{
					// Server side stats read of "client written" stats
					SteamAPICall_t ApiCall = GSteamGameServerStats->RequestUserStats(SteamPlayerId);
					GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamServerUserStatsReceived(this, ApiCall));

					Result = TRUE;
				}
				else
				{
					debugf(NAME_DevOnline, TEXT("Stats read failed; GSteamUserStats and GSteamGameServerStats are NULL"));
				}
			}

			CurrentStatsRead->TotalRowsInView = Players.Num();
		}
	}

	if (Result != TRUE && !bSkipCallback)
	{
		debugf(NAME_DevOnline, TEXT("ReadOnlineStats failed"));

		CurrentStatsRead = NULL;

		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResults Param((Result ? S_OK : E_FAIL));
		TriggerOnlineDelegates(this, ReadOnlineStatsCompleteDelegates, &Param);
	}

	return Result;
}


/**
 * Reads a player's stats and all of that player's friends stats for the
 * specified set of stat views. This allows you to easily compare a player's
 * stats to their friends.
 *
 * @param LocalUserNum the local player having their stats and friend's stats read for
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadOnlineStatsForFriends(BYTE LocalUserNum, UOnlineStatsRead* StatsRead)
{
	UBOOL bSuccess = FALSE;

	if (CurrentStatsRead != NULL)
	{
		debugf(NAME_DevOnline, TEXT("ReadOnlineStatsForFriends: Can't perform a stats read when one is in progress"));
	}
	else if (StatsRead == NULL)
	{
		debugf(NAME_DevOnline, TEXT("ReadOnlineStatsForFriends: Input StatsRead is NULL"));
	}
	else if (!IsSteamClientAvailable())
	{
		debugf(NAME_DevOnline, TEXT("ReadOnlineStatsForFriends: Steamworks not initialized"));
	}
	else
	{
		FString LeaderboardName = LeaderboardNameLookup(StatsRead->ViewId);

		// Friend leaderboard stats read
		if (!LeaderboardName.IsEmpty())
		{
			CurrentStatsRead = StatsRead;

			// Clear previous results
			CurrentStatsRead->Rows.Empty();

			if (ReadLeaderboardEntries(LeaderboardName, LRT_Friends))
			{
				bSuccess = TRUE;
			}
			else
			{
				CurrentStatsRead = NULL;
				debugf(TEXT("ReadOnlineStatsForFriends: Call to 'ReadLaderboardEntries has failed"));
			}
		}
		else
		{
			INT NumBuddies = GSteamFriends->GetFriendCount(k_EFriendFlagImmediate);
			TArray<FUniqueNetId> Players;

			Players.AddItem(LoggedInPlayerId);

			for (INT i=0; i<NumBuddies; i++)
			{
				const CSteamID SteamPlayerId(GSteamFriends->GetFriendByIndex(i, k_EFriendFlagImmediate));
				FUniqueNetId Player;

				Player.Uid = (QWORD)SteamPlayerId.ConvertToUint64();

				if (Player.Uid != LoggedInPlayerId.Uid)
				{
					Players.AddItem(Player);
				}
			}

			// Now use the common method to read the stats
			bSuccess = ReadOnlineStats(Players, StatsRead);
		}
	}

	return bSuccess;
}

/**
 * Reads stats by ranking. This grabs the rows starting at StartIndex through
 * NumToRead and places them in the StatsRead object.
 *
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param StartIndex the starting rank to begin reads at (1 for top)
 * @param NumToRead the number of rows to read (clamped at 100 underneath)
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadOnlineStatsByRank(UOnlineStatsRead* StatsRead,
	INT StartIndex,INT NumToRead)
{
	DWORD Result = E_FAIL;
	UBOOL bSkipCallback = FALSE;

	if (CurrentStatsRead == NULL)
	{
		if (StatsRead != NULL)
		{
			// Kickoff a leaderboard read request for this stats entry, and if there is no designated leaderboard, failure
			FString LeaderboardName = LeaderboardNameLookup(StatsRead->ViewId);

			if (!LeaderboardName.IsEmpty())
			{
				CurrentStatsRead = StatsRead;
				CurrentStatsRead->Rows.Empty();

				// Kickoff the read; ReadOnlineStats will be called with a list of GUIDs from OnUserDownloadedLeaderboardEntries
				if (ReadLeaderboardEntries(LeaderboardName, LRT_Global, StartIndex, StartIndex + Max(NumToRead, 0) - 1))
				{
					Result = ERROR_IO_PENDING;
				}
				else
				{
					debugf(TEXT("ReadOnlineStatsByRank: Call to ReadLeaderboardEntries has failed"));
				}
			}
			else
			{
				debugf(TEXT("ReadOnlineStatsByRank: StatsRead->ViewId does not have a LeaderboardNameMappings entry; failure"));
			}
		}
		else
		{
			debugf(TEXT("ReadOnlineStatsByRank: Input StatsRead is NULL"));
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't perform a stats read while one is in progress"));

		// Don't kill the current stats read by issuing callbacks for this
		bSkipCallback = TRUE;
	}

	if (Result != ERROR_IO_PENDING && !bSkipCallback)
	{
		debugf(NAME_Error,TEXT("ReadOnlineStatsByRank() failed"));

		CurrentStatsRead = NULL;

		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResults Param(Result);
		TriggerOnlineDelegates(this, ReadOnlineStatsCompleteDelegates, &Param);
	}

	return Result == ERROR_IO_PENDING;
}

/**
 * Reads stats by ranking centered around a player. This grabs a set of rows
 * above and below the player's current rank
 *
 * @param LocalUserNum the local player having their stats being centered upon
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param NumRows the number of rows to read above and below the player's rank
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadOnlineStatsByRankAroundPlayer(BYTE LocalUserNum,
	UOnlineStatsRead* StatsRead,INT NumRows)
{
	DWORD Result = E_FAIL;
	UBOOL bSkipCallback = FALSE;

	if (CurrentStatsRead == NULL)
	{
		if (StatsRead != NULL)
		{
			// Kickoff a leaderboard read request for this stats entry, and if there is no designated leaderboard, failure
			FString LeaderboardName = LeaderboardNameLookup(StatsRead->ViewId);

			if (!LeaderboardName.IsEmpty())
			{
				CurrentStatsRead = StatsRead;
				CurrentStatsRead->Rows.Empty();

				// Kickoff the read; ReadOnlineStats will be called with a list of GUIDs from OnUserDownloadedLeaderboardEntries
				if (ReadLeaderboardEntries(LeaderboardName, LRT_Player, -Abs(NumRows), Abs(NumRows)))
				{
					Result = ERROR_IO_PENDING;
				}
				else
				{
					debugf(TEXT("ReadOnlineStatsByRank: Call to ReadLeaderboardEntries has failed"));
				}
			}
			else
			{
				debugf(TEXT("ReadOnlineStatsByRank: StatsRead->ViewId does not have a LeaderboardNameMappings entry; failure"));
			}
		}
		else
		{
			debugf(TEXT("ReadOnlineStatsByRankAroundPlayer: Input StatsRead is NULL"));
		}
	}
	else
	{
		debugf(NAME_Error, TEXT("Can't perform a stats read while one is in progress"));

		// Don't kill the current stats read by issuing callbacks for this
		bSkipCallback = TRUE;
	}

	if (Result != ERROR_IO_PENDING && !bSkipCallback)
	{
		debugf(NAME_Error, TEXT("ReadOnlineStatsByRankAroundPlayer() failed"));

		CurrentStatsRead = NULL;

		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResults Param(Result);
		TriggerOnlineDelegates(this, ReadOnlineStatsCompleteDelegates, &Param);
	}

	return Result == ERROR_IO_PENDING;
}

/**
 * Cleans up any platform specific allocated data contained in the stats data
 *
 * @param StatsRead the object to handle per platform clean up on
 */
void UOnlineSubsystemSteamworks::FreeStats(UOnlineStatsRead* StatsRead)
{
}

/**
 * Writes out the stats contained within the stats write object to the online
 * subsystem's cache of stats data. Note the new data replaces the old. It does
 * not write the data to the permanent storage until a FlushOnlineStats() call
 * or a session ends. Stats cannot be written without a session or the write
 * request is ignored. No more than 5 stats views can be written to at a time
 * or the write request is ignored.
 *
 * @param SessionName the name of the session stats are being written for
 * @param Player the player to write stats for
 * @param StatsWrite the object containing the information to write
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::WriteOnlineStats(FName SessionName, FUniqueNetId Player, UOnlineStatsWrite* StatsWrite)
{
	UBOOL bSuccess = TRUE;

	if (!SessionHasStats())
	{
		debugf(NAME_Error, TEXT("WriteOnlineStats: SessionHasStats is FALSE"));
		bSuccess = FALSE;
	}
	else if (Player.Uid == 0)
	{
		debugf(NAME_Error, TEXT("WriteOnlineStats: Ignoring unknown player"));
		bSuccess = FALSE;
	}
	else if (!IsServer() && Player.Uid != LoggedInPlayerId.Uid)
	{
		debugf(NAME_Error, TEXT("Attempted to write stats for UID which is not the clients"));
		bSuccess = FALSE;
	}

	if (bSuccess)
	{
		AWorldInfo* WI = GWorld->GetWorldInfo();
		UBOOL bHasClientStats = WI->NetMode != NM_DedicatedServer;

		FPendingPlayerStats& PendingPlayerStats = FindOrAddPendingPlayerStats(Player);

		// Get a ref to the view ids
		const TArrayNoInit<INT>& ViewIds = StatsWrite->ViewIds;
		INT NumIndexes = ViewIds.Num();
		INT NumProperties = StatsWrite->Properties.Num();
		TArray<INT> MonitoredAchMappings;
		TArray<INT> AchViewIds;

		// Check if achievement progress needs monitoring (recalculated each time WriteOnlineStats is called, in case list changes)
		if (bHasClientStats && NumIndexes > 0)
		{
			INT LastViewId = INDEX_NONE;

			for (INT i=0; i<AchievementMappings.Num(); i++)
			{
				const FAchievementMappingInfo& Ach = AchievementMappings(i);

				if (((Ach.bAutoUnlock || Ach.ProgressCount > 0) && Ach.MaxProgress > 0) && ViewIds.ContainsItem(Ach.ViewId))
				{
					if (Ach.ViewId != LastViewId)
					{
						LastViewId = Ach.ViewId;
						AchViewIds.AddUniqueItem(LastViewId);
					}

					MonitoredAchMappings.AddItem(i);
				}
			}
		}

		for (INT ViewIndex=0; ViewIndex<NumIndexes; ViewIndex++)
		{
			INT ViewId = ViewIds(ViewIndex);

			UBOOL bMonitorAchievements = bHasClientStats && AchViewIds.ContainsItem(ViewId);

			FString LeaderboardName = LeaderboardNameLookup(ViewId);
			UBOOL bIsLeaderboardView = !LeaderboardName.IsEmpty();
			UBOOL bSetLeaderboardRank = FALSE;


			// Leaderboard entries can only be written by the client
			if (bIsLeaderboardView && !bHasClientStats)
			{
				debugf(NAME_DevOnline, TEXT("WriteOnlineStats: Can't write leaderboard stats on server, ViewId: %i"), ViewId);
				continue;
			}

			for (INT PropertyIndex=0; PropertyIndex<NumProperties; PropertyIndex++)
			{
				const FSettingsProperty& Property = StatsWrite->Properties(PropertyIndex);

				if (bIsLeaderboardView)
				{
					// Store a stats entry within leaderboard
					AddOrUpdateLeaderboardStat(LeaderboardName, Property.PropertyId, Property.Data);

					// Set leaderboard rank
					// NOTE: Rank is retrieved from the stored stat above, setting the rank here is just for Steam
					if (Property.PropertyId == StatsWrite->RatingId)
					{
						AddOrUpdateLeaderboardRank(LeaderboardName, Property.Data);
						bSetLeaderboardRank = TRUE;
					}
				}
				else
				{
					AddOrUpdatePlayerStat(PendingPlayerStats.Stats, ViewId, Property.PropertyId, Property.Data);

					// Achievement updating
					if (bHasClientStats && bMonitorAchievements)
					{
						for (INT i=0; i<MonitoredAchMappings.Num(); i++)
						{
							const FAchievementMappingInfo& Ach = AchievementMappings(MonitoredAchMappings(i));

							if (Ach.ViewId == ViewId && Ach.AchievementId == Property.PropertyId)
							{
								AddOrUpdateAchievementStat(Ach, Property.Data);

								MonitoredAchMappings.Remove(i, 1);
								break;
							}
						}
					}
				}
			}

			// If no leaderboard rank was written, no leaderboard data can be stored
			if (bIsLeaderboardView && NumProperties > 0 && !bSetLeaderboardRank)
			{
				debugf(NAME_DevOnline,
					TEXT("WriteOnlineStats: Can not write leaderboard stats without setting rank, ViewId: %i, RatingId: %i"),
					ViewId, StatsWrite->RatingId);

				ClearPendingLeaderboardData(LeaderboardName);
			}
		}
	}

	return bSuccess;
}

/**
 * Writes the specified set of scores to the skill tables
 *
 * @param SessionName the name of the session stats are being written for
 * @param LeaderboardId the leaderboard to write the score information to
 * @param PlayerScores the list of scores to write out
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::WriteOnlinePlayerScores(FName SessionName,INT LeaderboardId,const TArray<FOnlinePlayerScore>& PlayerScores)
{
	UBOOL bSuccess = FALSE;

	// Skip processing if the server isn't logged in
	if (SessionHasStats())
	{
		bSuccess = TRUE;

		INT NumScores = PlayerScores.Num();

		for (INT Index = 0; Index < NumScores; Index++)
		{
			const FOnlinePlayerScore& Score = PlayerScores(Index);
			// Don't record scores for bots
			if (Score.PlayerID.Uid != 0)
			{
				FPendingPlayerStats& PendingPlayerStats = FindOrAddPendingPlayerStats(Score.PlayerID);
				PendingPlayerStats.Score = Score;
			}
		}
	}
	else
	{
		debugf(NAME_Error, TEXT("WriteOnlinePlayerScores: SessionHasStats is FALSE"));
	}

	return bSuccess;
}

/**
 * Called when ready to submit all collected stats
 *
 * @param Players the players for which to submit stats
 * @param StatsWrites the stats to submit for the players
 * @param PlayerScores the scores for the players
 */
UBOOL UOnlineSubsystemSteamworks::CreateAndSubmitStatsReport()
{
	DWORD Return = S_OK;

	// Handle automated achievements updates (must be done before stats updates, because Steam auto-unlocks achievements from the
	//	backend, if they are linked up with backend stats; there is a bug with Steam listen hosts, where the achievement is
	//	marked as unlocked clientside when it is in fact not, due to the pending stats update)
	for (INT i=0; i<PendingAchievementProgress.Num(); i++)
	{
		const FAchievementProgressStat& CurAch = PendingAchievementProgress(i);

		if (CurAch.bUnlock)
		{
			if (UnlockAchievement(LoggedInPlayerNum, CurAch.AchievementId))
			{
				debugf(NAME_DevOnline, TEXT("Successfully triggered automatic achievement unlock for AchievementId: %i"),
					CurAch.AchievementId);
			}
			else
			{
				debugf(TEXT("Achievement unlock failed for AchievementId: %i"), CurAch.AchievementId);
			}
		}
		else if (CurAch.Progress > 0 && CurAch.Progress < CurAch.MaxProgress)
		{
			if (!DisplayAchievementProgress(CurAch.AchievementId, CurAch.Progress, CurAch.MaxProgress))
			{
				debugf(TEXT("Achievement progress display failed; AchievementId: %i, Progress: %i, MaxProgress: %i"),
						CurAch.AchievementId, CurAch.Progress, CurAch.MaxProgress);
			}
		}
		else
		{
			debugf(TEXT("Bad PendingAchievementProgress entry: AchievementId: %i, Progress: %i, MaxProgress: %i, bUnlock: %i"),
					CurAch.AchievementId, CurAch.Progress, CurAch.MaxProgress, CurAch.bUnlock);
		}
	}


	// Sort and filter player stats
	//PreprocessPlayersByScore();

	// Get the player count remaining after filtering
	const INT PlayerCount = PendingStats.Num();

	// Skip processing if there is no data to report
	if (PlayerCount > 0)
	{
		if (IsServer() && GSteamGameServerStats == NULL)
		{
			debugf(NAME_Error, TEXT("FlushOnlineStats: Warning!!! Game server stats session not setup yet"));
		}

		Return = ERROR_IO_PENDING;

		bGSStatsStoresSuccess = TRUE;
	}

	// Removed from if statement above, to reduce messy indentation
	for (INT Index=0; Index<PlayerCount; Index++)
	{
		const FPendingPlayerStats& Stats = PendingStats(Index);
		const INT StatCount = Stats.Stats.Num();
		const CSteamID SteamPlayerId((uint64)Stats.Player.Uid);
		UBOOL bPendingGSStats = FALSE;

		for (INT StatIndex=0; StatIndex<StatCount; StatIndex++)
		{
			const FPlayerStat& Stat = Stats.Stats(StatIndex);
			const FString Key(GetStatsFieldName(Stat.ViewId, Stat.ColumnId));
			UBOOL bGameServerStats = GameServerStatsMappings.ContainsItem(Stat.ViewId);
			UBOOL bOk = TRUE;

			INT Int32Value = 0;
			FLOAT FloatValue = 0.0f;

			// Temporary defines to cut down on code duplication, and to make the stat logic below easier to interpret
			#define WRITE_STEAM_CLIENT_STAT(StatKey, StatData) \
				if (StatData.Type == SDT_Int32) \
				{ \
					StatData.GetData(Int32Value); \
					bOk = GSteamUserStats->SetStat(TCHAR_TO_UTF8(*StatKey), Int32Value); \
				} \
				else if (StatData.Type == SDT_Float) \
				{ \
					StatData.GetData(FloatValue); \
					bOk = GSteamUserStats->SetStat(TCHAR_TO_UTF8(*StatKey), FloatValue); \
				} \
				else \
				{ \
					debugf(NAME_DevOnline, TEXT("Failed to write stat key %s, unsupported type: %i"), *StatKey, \
						StatData.Type); \
				}

			#define WRITE_STEAM_SERVER_STAT(StatPlayerId, StatKey, StatData) \
				if (StatData.Type == SDT_Int32) \
				{ \
					StatData.GetData(Int32Value); \
					bOk = GSteamGameServerStats->SetUserStat(StatPlayerId, TCHAR_TO_UTF8(*StatKey), Int32Value); \
				} \
				else if (StatData.Type == SDT_Float) \
				{ \
					StatData.GetData(FloatValue); \
					bOk = GSteamGameServerStats->SetUserStat(StatPlayerId, TCHAR_TO_UTF8(*StatKey), FloatValue); \
				} \
				else \
				{ \
					debugf(NAME_DevOnline, TEXT("Failed to write stat key %s, unsupported type: %i"), *StatKey, \
						StatData.Type); \
				}

			debugf(NAME_DevOnline, TEXT("Writing stats key '%s', value: %i"), *Key, Int32Value);

			if (bGameServerStats)
			{
				if (IsServer() && GSteamGameServerStats != NULL)
				{
					bPendingGSStats = TRUE;
					WRITE_STEAM_SERVER_STAT(SteamPlayerId, Key, Stat.Data);
				}
				else if (!IsServer())
				{
					debugf(NAME_DevOnline, TEXT("Attempted to write game server stats when not a server; key: %s"), *Key);
				}
				else // if (GSteamGameServerStats == NULL)
				{
					debugf(NAME_DevOnline,
						TEXT("Failed to write game server stats, GSteamGameServerStats == NULL; key: %s"), *Key);
				}
			}
			// Client stats
			else if (GSteamUserStats != NULL)
			{
				if (IsServer())
				{
					// If this is a server, can only write client stats for listen server hosts
					if (Stats.Player == LoggedInPlayerId)
					{
						WRITE_STEAM_CLIENT_STAT(Key, Stat.Data);
					}
					else
					{
						debugf(NAME_DevOnline,
							TEXT("Attempted to submit client stats for another players UID; key: %s"), *Key);
					}
				}
				else
				{
					WRITE_STEAM_CLIENT_STAT(Key, Stat.Data);
				}
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Failed to write client stats, GSteamUserStats == NULL; key: %s"), *Key);
			}

			#undef WRITE_STEAM_CLIENT_STAT
			#undef WRITE_STEAM_SERVER_STAT

			if (!bOk)
			{
				debugf(NAME_DevOnline, TEXT("Failed to store stat entry; key: %s, bGameServerStats: %i"), *Key, bGameServerStats);

				// We'll keep going though; maybe we'll get SOME of the stats through...
				bGSStatsStoresSuccess = FALSE;
			}
		}

		if (bPendingGSStats && IsServer() && GSteamGameServerStats != NULL)
		{
			debugf(NAME_DevOnline, TEXT("Storing GS stats for ") I64_FORMAT_TAG, Stats.Player.Uid);

			TotalGSStatsStoresPending++;

			SteamAPICall_t ApiCall = GSteamGameServerStats->StoreUserStats(SteamPlayerId);
			GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamServerUserStatsStored(this, ApiCall));
		}
	}

	if (PlayerCount > 0)
	{
		// If this is a client, or a listen server, store the local clients stats
		AWorldInfo* WI = (GWorld != NULL ? GWorld->GetWorldInfo() : NULL);

		if (!IsServer() || WI->NetMode == NM_ListenServer)
		{
			if (GSteamUserStats != NULL && GSteamUserStats->StoreStats())
			{
				debugf(NAME_DevOnline, TEXT("Storing stats for player (StoreStats success)"));
				bClientStatsStorePending = TRUE;
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Failed to store stats for player (StoreStats failed"));
				Return = E_FAIL;
			}
		}
	}
	else
	{
		Return = S_OK;
		debugf(NAME_DevOnline, TEXT("No players present to report data for"));
	}

	// Now store leaderboard stats
	for (INT i=0; i<PendingLeaderboardStats.Num(); i++)
	{
		if (WriteLeaderboardScore(PendingLeaderboardStats(i).LeaderboardName, PendingLeaderboardStats(i).Score,
			PendingLeaderboardStats(i).LeaderboardData))
		{
			debugf(NAME_DevOnline, TEXT("Successfully wrote to leaderboard '%s', value: %i"),
				*PendingLeaderboardStats(i).LeaderboardName, PendingLeaderboardStats(i).Score);
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Failed to write to leaderboard '%s'"), *PendingLeaderboardStats(i).LeaderboardName);
			Return = E_FAIL;
		}
	}

	if (Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResultsNamedSession Params(FName(TEXT("Game")), Return);
		TriggerOnlineDelegates(this, FlushOnlineStatsDelegates, &Params);
	}

	return (Return == S_OK || Return == ERROR_IO_PENDING);
}

/**
 * Searches for a player's pending stats, returning them if they exist, or adding them if they don't
 *
 * @param Player the player to find/add stats for
 *
 * @return the existing/new stats for the player
 */
FPendingPlayerStats& UOnlineSubsystemSteamworks::FindOrAddPendingPlayerStats(const FUniqueNetId& Player)
{
	// First check if this player already has pending stats
	for(INT Index = 0; Index < PendingStats.Num(); Index++)
	{
		if (PendingStats(Index).Player.Uid == Player.Uid)
		{
			return PendingStats(Index);
		}
	}
	// This player doesn't have any stats, add them to the array
	INT Index = PendingStats.AddZeroed();
	FPendingPlayerStats& PlayerStats = PendingStats(Index);
	PlayerStats.Player = Player;

	// Use the stat guid from the player struct if a client, otherwise use the host's id
	if (PlayerStats.Player.Uid == LoggedInPlayerId.Uid)
	{
		// @todo Steam: not used at the moment.
		//PlayerStats.StatGuid = ANSI_TO_TCHAR((const gsi_u8*)scGetConnectionId(SCHandle));
		PlayerStats.StatGuid = *FString::Printf(I64_FORMAT_TAG, LoggedInPlayerId.Uid);
	}
	return PlayerStats;
}

/**
 * Refresh data in pending stats
 *
 * @param PlayerStats	The player stats struct to be refreshed
 * @param ViewId	The target ViewId
 * @param ColumnId	The target ColumnId
 * @param Data		The new data to apply to the specific stats field
 */
void UOnlineSubsystemSteamworks::AddOrUpdatePlayerStat(TArray<FPlayerStat>& PlayerStats, INT ViewId, INT ColumnId, const FSettingsData& Data)
{
	for (INT StatIndex=0; StatIndex<PlayerStats.Num(); StatIndex++)
	{
		FPlayerStat& Stats = PlayerStats(StatIndex);

		if (Stats.ViewId == ViewId && Stats.ColumnId == ColumnId)
		{
			Stats.Data = Data;
			return;
		}
	}

	INT AddIndex = PlayerStats.AddZeroed();
	FPlayerStat& NewPlayer = PlayerStats(AddIndex);

	NewPlayer.ViewId = ViewId;
	NewPlayer.ColumnId = ColumnId;
	NewPlayer.Data = Data;
}

/**
 * Refresh pending achievement progress updates
 *
 * @param Ach	Details of the achievment that will be updated
 * @param Data	The progress data for the achievement
 */
void UOnlineSubsystemSteamworks::AddOrUpdateAchievementStat(const FAchievementMappingInfo& Ach, const FSettingsData& Data)
{
	INT CurProgress = 0;
	Data.GetData(CurProgress);

	UBOOL bUnlock = Ach.bAutoUnlock && CurProgress >= Ach.MaxProgress;

	// No monitoring needed if neither an unlock or progress toast is due
	if (!bUnlock && (Ach.ProgressCount == 0 || CurProgress == 0 || (CurProgress % Ach.ProgressCount) != 0))
	{
		return;
	}


	// Make sure the achievement hasn't already been achieved
	bool bAchieved = false;

	if (GSteamUserStats == NULL || !GSteamUserStats->GetAchievement(TCHAR_TO_ANSI(*Ach.AchievementName.ToString()), &bAchieved))
	{
		debugf(TEXT("AddOrUpdateAchievementStat: Failed to retrieve achievement status for achievement '%s'"),
			*Ach.AchievementName.ToString());

		return;
	}

	if (bAchieved)
	{
		return;
	}


	// Make sure the achievement stat value is actually changing value (otherwise you display the progress toast on every stats update)
	const CSteamID SteamPlayerId((uint64)LoggedInPlayerId.Uid);
	const FString Key(GetStatsFieldName(Ach.ViewId, Ach.AchievementId));
	INT OldProgress = 0;

	if (GSteamUserStats != NULL && GSteamUserStats->GetUserStat(SteamPlayerId, TCHAR_TO_UTF8(*Key), &OldProgress))
	{
		if (CurProgress == OldProgress)
		{
			return;
		}
	}
	else
	{
		debugf(TEXT("AddOrUpdateAchievementStat: Failed to retrieve current value for AchievementId: %i"), Ach.AchievementId);
		return;
	}


	// Store/update the progress
	for (INT i=0; i<PendingAchievementProgress.Num(); i++)
	{
		if (PendingAchievementProgress(i).AchievementId == Ach.AchievementId)
		{
			PendingAchievementProgress(i).Progress = CurProgress;
			PendingAchievementProgress(i).MaxProgress = Ach.MaxProgress;
			PendingAchievementProgress(i).bUnlock = bUnlock;

			return;
		}
	}

	INT CurIndex = PendingAchievementProgress.Add(1);

	PendingAchievementProgress(CurIndex).AchievementId = Ach.AchievementId;
	PendingAchievementProgress(CurIndex).Progress = CurProgress;
	PendingAchievementProgress(CurIndex).MaxProgress = Ach.MaxProgress;
	PendingAchievementProgress(CurIndex).bUnlock = bUnlock;
}

/**
 * Takes a stats ViewId and matches it up to a leaderboard name
 *
 * @param ViewId	The ViewId to be matched to a leaderboard
 * @return		The name of the leaderboard
 */
FString UOnlineSubsystemSteamworks::LeaderboardNameLookup(INT ViewId)
{
	for (INT i=0; i<LeaderboardNameMappings.Num(); i++)
	{
		if (LeaderboardNameMappings(i).ViewId == ViewId)
		{
			return LeaderboardNameMappings(i).LeaderboardName;
		}
	}

	return FString();
}

/**
 * Caches stats data to be stored within a leaderboard entry
 *
 * @param LeaderboardName	The name of the leaderboard stats data is to be stored in
 * @param ColumnId		The ColumnId of the stats data
 * @param Data			The raw stats data (must be int32)
 */
void UOnlineSubsystemSteamworks::AddOrUpdateLeaderboardStat(FString LeaderboardName, INT ColumnId, const FSettingsData& Data)
{
	if (Data.Type != SDT_Int32)
	{
		debugf(TEXT("AddOrUpdateLeaderboardStat: Leaderboard data must be int32"), *LeaderboardName);
		return;
	}


	INT LeaderboardIndex = INDEX_NONE;

	for (INT i=0; i<PendingLeaderboardStats.Num(); i++)
	{
		if (PendingLeaderboardStats(i).LeaderboardName == LeaderboardName)
		{
			LeaderboardIndex = i;
			break;
		}
	}

	if (LeaderboardIndex == INDEX_NONE)
	{
		LeaderboardIndex = PendingLeaderboardStats.AddZeroed(1);
		PendingLeaderboardStats(LeaderboardIndex).LeaderboardName = LeaderboardName;
	}


	// Steam can only hold 64 int values within a leaderboard entry (each stat entry from UE3 uses 2 ints)
	if (PendingLeaderboardStats(LeaderboardIndex).LeaderboardData.Num() >= 64)
	{
		debugf(NAME_DevOnline,
			TEXT("Can only store a maximum of 32 stat values with a leaderboard; dropping ColumnId '%i' from leaderboard: %s"),
			ColumnId, *LeaderboardName);

		return;
	}

	// LeaderboardData stores the ColumnId of the stat entry, and then the stat entry itself (the same order it is fed to Steam)
	// NOTE: ColumnId's are stored as 'ColumnId+1', and reverted to 'ColumnId-1' upon retrieval;
	//		the '0' value is used to indicate no stat entry
	PendingLeaderboardStats(LeaderboardIndex).LeaderboardData.AddItem(ColumnId+1);

	INT CurVal = 0;
	Data.GetData(CurVal);

	PendingLeaderboardStats(LeaderboardIndex).LeaderboardData.AddItem(CurVal);
}

/**
 * Caches the ranking of a pending leaderboard entry
 *
 * @param LeaderboardName	The name of the leaderboard pending an update
 * @param Data			The raw rank data (must be int32)
 */
void UOnlineSubsystemSteamworks::AddOrUpdateLeaderboardRank(FString LeaderboardName, const FSettingsData& Data)
{
	if (Data.Type != SDT_Int32)
	{
		debugf(TEXT("AddOrUpdateLeaderboardRank: Leaderboard data must be int32"), *LeaderboardName);
		return;
	}

	// Store/update the stats
	INT CurVal = 0;
	Data.GetData(CurVal);

	for (INT i=0; i<PendingLeaderboardStats.Num(); i++)
	{
		if (PendingLeaderboardStats(i).LeaderboardName == LeaderboardName)
		{
			PendingLeaderboardStats(i).Score = CurVal;
			return;
		}
	}

	INT CurIndex = PendingLeaderboardStats.AddZeroed(1);

	PendingLeaderboardStats(CurIndex).LeaderboardName = LeaderboardName;
	PendingLeaderboardStats(CurIndex).Score = CurVal;
}

/**
 * Wipes (or backs-out) a pending leaderboard update, before it is committed
 *
 * @param LeaderboardName	The name of the leaderboard pending an update
 */
void UOnlineSubsystemSteamworks::ClearPendingLeaderboardData(FString LeaderboardName)
{
	for (INT i=0; i<PendingLeaderboardStats.Num(); i++)
	{
		if (PendingLeaderboardStats(i).LeaderboardName == LeaderboardName)
		{
			PendingLeaderboardStats.Remove(i, 1);
			return;
		}
	}
}

/**
 * Commits any changes in the online stats cache to the permanent storage
 *
 * @param SessionName the name of the session flushing stats
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::FlushOnlineStats(FName SessionName)
{
	UBOOL bSuccess = FALSE;

	// Skip processing if the server isn't logged in
	if (SessionHasStats())
	{
		if (PendingStats.Num() > 0 || PendingLeaderboardStats.Num() > 0 || PendingAchievementProgress.Num() > 0)
		{
			bSuccess = CreateAndSubmitStatsReport();

			PendingStats.Empty();
			PendingLeaderboardStats.Empty();
			PendingAchievementProgress.Empty();
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("FlushOnlineStats: No stats to flush"));
		}
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("FlushOnlineStats: SessionHasStats returned FALSE"));
	}

	return bSuccess;
}

/** Return steamcloud filename for player profile. */
FString UOnlineSubsystemSteamworks::CreateProfileName()
{
	return TEXT("profile.bin");  // this is what the GameSpy/PS3 stuff does. Don't split it per-user, since Steam handles that.
}

/**
 * Determines whether the user's profile file exists or not
 */
UBOOL UOnlineSubsystemSteamworks::DoesProfileExist()
{
	if (IsSteamClientAvailable() && LoggedInPlayerName.Len())
	{
		// There is a proper FileExists API, but if it doesn't exist it'll return zero for the size, and we need to treat zero-len profiles as missing anyhow.
		//return GSteamRemoteStorage->FileExists(TCHAR_TO_UTF8(*CreateProfileName()));
		return GSteamRemoteStorage->GetFileSize(TCHAR_TO_UTF8(*CreateProfileName())) > 0;
	}

	return FALSE;
}

/**
 * Reads the online profile settings for a given user from disk. If the file
 * exists, an async task is used to verify the file wasn't hacked and to
 * decompress the contents into a buffer. Once the task
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param ProfileSettings the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadProfileSettings(BYTE LocalUserNum, UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = E_FAIL;

	// Only read the data for the logged in player
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		// Only read if we don't have a profile for this player
		if (CachedProfile == NULL)
		{
			if (ProfileSettings != NULL)
			{
				CachedProfile = ProfileSettings;
				CachedProfile->AsyncState = OPAS_Read;
				// Clear the previous set of results
				CachedProfile->ProfileSettings.Empty();
				// Don't try to read without being logged in
				if (LoggedInStatus > LS_NotLoggedIn)
				{
					// Don't bother reading the local file if they haven't saved it before
					if (DoesProfileExist())
					{
						const int32	Size = GSteamRemoteStorage->GetFileSize(TCHAR_TO_UTF8(*CreateProfileName()));
						TArray<BYTE> Buffer(Size);
						if (GSteamRemoteStorage->FileRead(TCHAR_TO_UTF8(*CreateProfileName()), &Buffer(0), Size) == Size)
						{
							FProfileSettingsReader Reader(3000,TRUE,Buffer.GetTypedData(),Buffer.Num());
							// Serialize the profile from that array
							if (Reader.SerializeFromBuffer(CachedProfile->ProfileSettings))
							{
								INT ReadVersion = CachedProfile->GetVersionNumber();
								// Check the version number and reset to defaults if they don't match
								if (CachedProfile->VersionNumber != ReadVersion)
								{
									debugf(NAME_DevOnline,
										TEXT("Detected profile version mismatch (%d != %d), setting to defaults"),
										CachedProfile->VersionNumber,
										ReadVersion);
									CachedProfile->eventSetToDefaults();
								}
								Return = S_OK;
							}
							else
							{
								debugf(NAME_DevOnline,
									TEXT("Profile data for %s was corrupt, using defaults"),
									*LoggedInPlayerName);
								CachedProfile->eventSetToDefaults();
								Return = S_OK;
							}
						}
						else
						{
							debugf(NAME_DevOnline, TEXT("Failed to read local profile"));
							CachedProfile->eventSetToDefaults();
							if (LoggedInStatus == LS_UsingLocalProfile)
							{
								CachedProfile->AsyncState = OPAS_Finished;
								// Immediately save to that so the profile will be there in the future
								WriteProfileSettings(LocalUserNum,ProfileSettings);
							}
							Return = S_OK;
						}
					}
					else
					{
						// First time read so use defaults
						CachedProfile->eventSetToDefaults();
						CachedProfile->AsyncState = OPAS_Finished;
						// Immediately save to that so the profile will be there in the future
						WriteProfileSettings(LocalUserNum,ProfileSettings);
						Return = S_OK;
					}
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("Player is not logged in using defaults for profile data"),
						*LoggedInPlayerName);
					CachedProfile->eventSetToDefaults();
					Return = S_OK;
				}
			}
			else
			{
				debugf(NAME_Error,TEXT("Can't specify a null profile settings object"));
			}
		}
		// Make sure the profile isn't already being read, since this is going to
		// complete immediately
		else if (CachedProfile->AsyncState != OPAS_Read)
		{
			debugf(NAME_DevOnline,TEXT("Using cached profile data instead of reading"));
			// If the specified read isn't the same as the cached object, copy the
			// data from the cache
			if (CachedProfile != ProfileSettings)
			{
				ProfileSettings->ProfileSettings = CachedProfile->ProfileSettings;
				CachedProfile = ProfileSettings;
			}
			Return = S_OK;
		}
		else
		{
			debugf(NAME_Error,TEXT("Profile read for player (%d) is already in progress"),LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Specified user is not logged in, setting to the defaults"));
		ProfileSettings->eventSetToDefaults();
		Return = S_OK;
	}
	// Trigger the delegates if there are any registered
	if (Return != ERROR_IO_PENDING)
	{
		// Mark the read as complete
		if (CachedProfile && LocalUserNum == LoggedInPlayerNum)
		{
			CachedProfile->AsyncState = OPAS_Finished;
		}

		OnlineSubsystemSteamworks_eventOnReadProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (Return == 0) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = LocalUserNum;
		// Use the common method to do the work
		TriggerOnlineDelegates(this,ProfileCache.ReadDelegates,&Parms);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Writes the online profile settings for a given user Live using an async task
 *
 * @param LocalUserNum the user that we are writing the data for
 * @param ProfileSettings the list of settings to write out
 *
 * @return true if the call succeeds, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::WriteProfileSettings(BYTE LocalUserNum, UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = E_FAIL;

	// Only the logged in user can write their profile data
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		// Don't allow a write if there is a task already in progress
		if (CachedProfile == NULL ||
			(CachedProfile->AsyncState != OPAS_Read && CachedProfile->AsyncState != OPAS_Write))
		{
			if (ProfileSettings != NULL)
			{
				// Cache to make sure GC doesn't collect this while we are waiting
				// for the task to complete
				CachedProfile = ProfileSettings;
				// Mark this as a write in progress
				CachedProfile->AsyncState = OPAS_Write;
				// Make sure the profile settings have a version number
				CachedProfile->AppendVersionToSettings();
				// Update the save count for roaming profile support
				UOnlinePlayerStorage::SetProfileSaveCount(UOnlinePlayerStorage::GetProfileSaveCount(CachedProfile->ProfileSettings, PSI_ProfileSaveCount) + 1,ProfileSettings->ProfileSettings, PSI_ProfileSaveCount);

				// Write the data to Steam Cloud. This actually just goes to a local disk cache, and the Steam client pushes it
				//  to the Cloud when the game terminates, so this is a "fast" call that doesn't need async processing.

				// Serialize them to a blob and then write to disk
				FProfileSettingsWriter Writer(3000,TRUE);
				if (Writer.SerializeToBuffer(CachedProfile->ProfileSettings))
				{
					const INT Size = Writer.GetFinalBufferLength();
					if (GSteamRemoteStorage->FileWrite(TCHAR_TO_UTF8(*CreateProfileName()), (void*)Writer.GetFinalBuffer(), Size))
					{
						debugf(NAME_DevOnline, TEXT("Wrote profile (%d bytes) to Steam Cloud."), Size);
						Return = S_OK;
					}
					else
					{
						debugf(NAME_DevOnline, TEXT("Failed to write profile (%d bytes) to Steam Cloud!"), Size);
						Return = E_FAIL;
					}
				}
			}
			else
			{
				debugf(NAME_Error,TEXT("Can't write a null profile settings object"));
			}
		}
		else
		{
			debugf(NAME_Error,
				TEXT("Can't write profile as an async profile task is already in progress for player (%d)"),
				LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Ignoring write profile request for non-logged in player"));
		Return = S_OK;
	}
	if (Return != ERROR_IO_PENDING)
	{
		if (CachedProfile)
		{
			// Remove the write state so that subsequent writes work
			CachedProfile->AsyncState = OPAS_Finished;
		}
		OnlineSubsystemSteamworks_eventOnWriteProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (Return == 0) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = LoggedInPlayerNum;
		TriggerOnlineDelegates(this,WriteProfileSettingsDelegates,&Parms);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Starts an async task that retrieves the list of friends for the player from the
 * online service. The list can be retrieved in whole or in part.
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Count the number of friends to read or zero for all
 * @param StartingAt the index of the friends list to start at (for pulling partial lists)
 *
 * @return true if the read request was issued successfully, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadFriendsList(BYTE LocalUserNum,INT Count,INT StartingAt)
{
	DWORD Return = E_FAIL;

	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		Return = S_OK;
	}
	// Always trigger the delegate immediately and again as friends are added
	FAsyncTaskDelegateResults Params(Return);
	TriggerOnlineDelegates(this,ReadFriendsDelegates,&Params);
	return Return == S_OK;
}

/**
 * Copies the list of friends for the player previously retrieved from the online
 * service. The list can be retrieved in whole or in part.
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Friends the out array that receives the copied data
 * @param Count the number of friends to read or zero for all
 * @param StartingAt the index of the friends list to start at (for pulling partial lists)
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
BYTE UOnlineSubsystemSteamworks::GetFriendsList(BYTE LocalUserNum,TArray<FOnlineFriend>& Friends,INT Count,INT StartingAt)
{
	// Empty the existing list so dupes don't happen
	Friends.Empty(Friends.Num());

	if (!IsSteamClientAvailable())
	{
		return OERS_Done;
	}

	const INT NumBuddies = GSteamFriends->GetFriendCount(k_EFriendFlagImmediate);
	if (Count == 0)
	{
		Count = NumBuddies;
	}
	for (INT Index = 0; Index < Count; Index++)
	{
		const INT SteamFriendIndex = StartingAt + Index;
		if (SteamFriendIndex >= NumBuddies)
		{
			break;
		}

		const CSteamID SteamPlayerID(GSteamFriends->GetFriendByIndex(SteamFriendIndex, k_EFriendFlagImmediate));
		const char *NickName = GSteamFriends->GetFriendPersonaName(SteamPlayerID);
		const EPersonaState PersonaState = GSteamFriends->GetFriendPersonaState(SteamPlayerID);
		FriendGameInfo_t FriendGameInfo;
		const bool bInGame = GSteamFriends->GetFriendGamePlayed(SteamPlayerID, &FriendGameInfo);

		if (NickName[0] == 0)
		{
			// this user doesn't have a uniquenick
			// the regular nick could be used, but it not being unique could be a problem
			// @todo Steam: should this use the regular nick?
			continue;
		}
		// @todo Steam: We could generate the locationString properly by pinging the server directly, but that's a mess for several reasons.
		//		It would be nice if we could publish this information per-player through SteamFriends.
		//Friend.PresenceInfo =  // @todo Steam: (wchar_t*)BuddyStatus.locationString;
		servernetadr_t Addr;
		Addr.Init(FriendGameInfo.m_unGameIP, FriendGameInfo.m_usQueryPort, FriendGameInfo.m_usGamePort);

		const INT FriendIndex = Friends.AddZeroed();
		FOnlineFriend& Friend = Friends(FriendIndex);
		Friend.UniqueId.Uid = (QWORD) SteamPlayerID.ConvertToUint64();
		Friend.NickName = UTF8_TO_TCHAR(NickName);
		Friend.PresenceInfo = ANSI_TO_TCHAR(Addr.GetConnectionAddressString());
 		Friend.bIsOnline = (PersonaState > k_EPersonaStateOffline);
		Friend.bIsPlaying = bInGame;
		Friend.bIsPlayingThisGame = (FriendGameInfo.m_gameID.AppID() == GSteamAppID);  // @todo Steam: check mod id?
		if (Friend.bIsPlayingThisGame)
		{
			// Build an URL so we can parse options
			FURL Url(NULL,*Friend.PresenceInfo,TRAVEL_Absolute);
			// Get our current location to prevent joining the game we are already in
			const FString CurrentLocation = CachedGameInt && CachedGameInt->SessionInfo ?
				CachedGameInt->SessionInfo->HostAddr.ToString(FALSE) :
				TEXT("");
			// Parse the host address to see if they are joinable
			if (Url.Host.Len() > 0 &&
				// @todo Steam
				//appStrstr((const TCHAR*)BuddyStatus.locationString,TEXT("Standalone")) == NULL &&
				//appStrstr((const TCHAR*)BuddyStatus.locationString,TEXT("bIsLanMatch")) == NULL &&
				(CurrentLocation.Len() == 0 || appStrstr(*Friend.PresenceInfo,*CurrentLocation) == NULL))
			{
				UBOOL bIsValid;
				FInternetIpAddr HostAddr;
				// Set the IP address listed and see if it's valid
				HostAddr.SetIp(*Url.Host,bIsValid);
				Friend.bIsJoinable = bIsValid;
			}
			Friend.bHasVoiceSupport = FALSE; // @todo Steam: Url.HasOption(TEXT("bHasVoice"));
		}
	}
	return OERS_Done;
}

/**
 * Processes any talking delegates that need to be fired off
 */
void UOnlineSubsystemSteamworks::ProcessTalkingDelegates(void)
{
	// Skip all delegate handling if none are registered
	if (TalkingDelegates.Num() > 0)
	{
		// Only check players with voice
		if (CurrentLocalTalker.bHasVoice &&
			(CurrentLocalTalker.bWasTalking != CurrentLocalTalker.bIsTalking))
		{
			OnlineSubsystemSteamworks_eventOnPlayerTalkingStateChange_Parms Parms(EC_EventParm);
			// Use the cached id for this
			Parms.Player = LoggedInPlayerId;
			Parms.bIsTalking = CurrentLocalTalker.bIsTalking;
			TriggerOnlineDelegates(this,TalkingDelegates,&Parms);
			// Clear the flag so it only activates when needed
			CurrentLocalTalker.bWasTalking = CurrentLocalTalker.bIsTalking;
		}
		// Now check all remote talkers
		for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
		{
			FRemoteTalker& Talker = RemoteTalkers(Index);
			if (Talker.bWasTalking != Talker.bIsTalking)
			{
				OnlineSubsystemSteamworks_eventOnPlayerTalkingStateChange_Parms Parms(EC_EventParm);
				Parms.Player = Talker.TalkerId;
				Parms.bIsTalking = Talker.bIsTalking;
				TriggerOnlineDelegates(this,TalkingDelegates,&Parms);
				// Clear the flag so it only activates when needed
				Talker.bWasTalking = Talker.bIsTalking;
			}
		}
	}
}


/**
 * Processes any speech recognition delegates that need to be fired off
 */
void UOnlineSubsystemSteamworks::ProcessSpeechRecognitionDelegates(void)
{
	// Skip all delegate handling if we aren't using speech recognition
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		if (VoiceEngine->HasRecognitionCompleted(0))
		{
			TriggerOnlineDelegates(this,SpeechRecognitionCompleteDelegates,NULL);
		}
	}
}

/**
 * Registers the user as a talker
 *
 * @param LocalUserNum the local player index that is a talker
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::RegisterLocalTalker(BYTE LocalUserNum)
{
	DWORD Return = E_FAIL;
	if (VoiceEngine != NULL)
	{
		// Register the talker locally
		Return = VoiceEngine->RegisterLocalTalker(LocalUserNum);
		debugf(NAME_DevOnline,TEXT("RegisterLocalTalker(%d) returned 0x%08X"),
			LocalUserNum,Return);
		if (Return == S_OK)
		{
			CurrentLocalTalker.bHasVoice = TRUE;
		}
	}
	else
	{
		// Not properly logged in, so skip voice for them
		CurrentLocalTalker.bHasVoice = FALSE;
	}
	return Return == S_OK;
}

/**
 * Unregisters the user as a talker
 *
 * @param LocalUserNum the local player index to be removed
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::UnregisterLocalTalker(BYTE LocalUserNum)
{
	DWORD Return = S_OK;
	// Skip the unregistration if not registered
	if (CurrentLocalTalker.bHasVoice == TRUE &&
		// Or when voice is disabled
		VoiceEngine != NULL)
	{
		Return = VoiceEngine->UnregisterLocalTalker(LocalUserNum);
		debugf(NAME_DevOnline,TEXT("UnregisterLocalTalker(%d) returned 0x%08X"),
			LocalUserNum,Return);
		CurrentLocalTalker.bHasVoice = FALSE;
	}
	return Return == S_OK;
}

/**
 * Registers a remote player as a talker
 *
 * @param UniqueId the unique id of the remote player that is a talker
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::RegisterRemoteTalker(FUniqueNetId UniqueId)
{
	DWORD Return = E_FAIL;
	if (VoiceEngine != NULL)
	{
		// See if this talker has already been registered or not
		FRemoteTalker* Talker = FindRemoteTalker(UniqueId);
		if (Talker == NULL)
		{
			// Add a new talker to our list
			INT AddIndex = RemoteTalkers.AddZeroed();
			Talker = &RemoteTalkers(AddIndex);
			// Copy the net id
			(QWORD&)Talker->TalkerId = (QWORD&)UniqueId;
			// Register the remote talker locally
			Return = VoiceEngine->RegisterRemoteTalker(UniqueId);
			debugf(NAME_DevOnline,TEXT("RegisterRemoteTalker(0x%016I64X) returned 0x%08X"),
				(QWORD&)UniqueId,Return);
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Remote talker is being re-registered"));
			Return = S_OK;
		}
		// Now start processing the remote voices
		Return = VoiceEngine->StartRemoteVoiceProcessing(UniqueId);
		debugf(NAME_DevOnline,TEXT("StartRemoteVoiceProcessing(0x%016I64X) returned 0x%08X"),
			(QWORD&)UniqueId,Return);
	}
	return Return == S_OK;
}

/**
 * Unregisters a remote player as a talker
 *
 * @param UniqueId the unique id of the remote player to be removed
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::UnregisterRemoteTalker(FUniqueNetId UniqueId)
{
	DWORD Return = E_FAIL;
	if (VoiceEngine != NULL)
	{
		// Make sure the talker is valid
		if (FindRemoteTalker(UniqueId) != NULL)
		{
			// Find them in the talkers array and remove them
			for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
			{
				const FRemoteTalker& Talker = RemoteTalkers(Index);
				// Is this the remote talker?
				if ((QWORD&)Talker.TalkerId == (QWORD&)UniqueId)
				{
					RemoteTalkers.Remove(Index);
					break;
				}
			}
			// Make sure to remove them from the mute list so that if they
			// rejoin they aren't already muted
			MuteList.RemoveItem(UniqueId);
			// Remove them from voice too
			Return = VoiceEngine->UnregisterRemoteTalker(UniqueId);
			debugf(NAME_DevOnline,TEXT("UnregisterRemoteTalker(0x%016I64X) returned 0x%08X"),
				(QWORD&)UniqueId,Return);
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Unknown remote talker specified to UnregisterRemoteTalker()"));
		}
	}
	return Return == S_OK;
}

/**
 * Finds a remote talker in the cached list
 *
 * @param UniqueId the net id of the player to search for
 *
 * @return pointer to the remote talker or NULL if not found
 */
FRemoteTalker* UOnlineSubsystemSteamworks::FindRemoteTalker(FUniqueNetId UniqueId)
{
	for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
	{
		FRemoteTalker& Talker = RemoteTalkers(Index);
		// Compare net ids to see if they match
		if (Talker.TalkerId.Uid == UniqueId.Uid)
		{
			return &RemoteTalkers(Index);
		}
	}
	return NULL;
}

/**
 * Determines if the specified player is actively talking into the mic
 *
 * @param LocalUserNum the local player index being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::IsLocalPlayerTalking(BYTE LocalUserNum)
{
	return LocalUserNum == LoggedInPlayerNum && VoiceEngine != NULL && VoiceEngine->IsLocalPlayerTalking(LocalUserNum);
}

/**
 * Determines if the specified remote player is actively talking into the mic
 * NOTE: Network latencies will make this not 100% accurate
 *
 * @param UniqueId the unique id of the remote player being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::IsRemotePlayerTalking(FUniqueNetId UniqueId)
{
	return VoiceEngine != NULL && VoiceEngine->IsRemotePlayerTalking(UniqueId);
}

/**
 * Determines if the specified player has a headset connected
 *
 * @param LocalUserNum the local player index being queried
 *
 * @return TRUE if the player has a headset plugged in, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::IsHeadsetPresent(BYTE LocalUserNum)
{
	return LocalUserNum == LoggedInPlayerNum && VoiceEngine != NULL && VoiceEngine->IsHeadsetPresent(LocalUserNum);
}

/**
 * Sets the relative priority for a remote talker. 0 is highest
 *
 * @param LocalUserNum the user that controls the relative priority
 * @param UniqueId the remote talker that is having their priority changed for
 * @param Priority the relative priority to use (0 highest, < 0 is muted)
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::SetRemoteTalkerPriority(BYTE LocalUserNum,FUniqueNetId UniqueId,INT Priority)
{
	DWORD Return = E_FAIL;
	if (VoiceEngine != NULL)
	{
		// Find the remote talker to modify
		FRemoteTalker* Talker = FindRemoteTalker(UniqueId);
		if (Talker != NULL)
		{
			Return = VoiceEngine->SetPlaybackPriority(LocalUserNum,UniqueId,Priority);
			debugf(NAME_DevOnline,TEXT("SetPlaybackPriority(%d,0x%016I64X,%d) return 0x%08X"),
				LocalUserNum,(QWORD&)UniqueId,Priority,Return);
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Unknown remote talker specified to SetRemoteTalkerPriority()"));
		}
	}
	return Return == S_OK;
}

/**
 * Mutes a remote talker for the specified local player. NOTE: This only mutes them in the
 * game unless the bIsSystemWide flag is true, which attempts to mute them globally
 *
 * @param LocalUserNum the user that is muting the remote talker
 * @param PlayerId the remote talker that is being muted
 * @param bIsSystemWide whether to try to mute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::MuteRemoteTalker(BYTE LocalUserNum,FUniqueNetId UniqueId,UBOOL bIsSystemWide)
{
	DWORD Return = E_FAIL;
	if (VoiceEngine != NULL)
	{
		// Find the specified talker
		FRemoteTalker* Talker = FindRemoteTalker(UniqueId);
		if (Talker != NULL)
		{
			// Add them to the mute list
			MuteList.AddUniqueItem(UniqueId);
			Return = S_OK;
			debugf(NAME_DevOnline,TEXT("Muted talker 0x%016I64X"),(QWORD&)UniqueId);
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Unknown remote talker specified to MuteRemoteTalker()"));
		}
	}
	return Return == S_OK;
}

/**
 * Allows a remote talker to talk to the specified local player. NOTE: This only unmutes them in the
 * game unless the bIsSystemWide flag is true, which attempts to unmute them globally
 *
 * @param LocalUserNum the user that is allowing the remote talker to talk
 * @param PlayerId the remote talker that is being restored to talking
 * @param bIsSystemWide whether to try to unmute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::UnmuteRemoteTalker(BYTE LocalUserNum,FUniqueNetId UniqueId,UBOOL bIsSystemWide)
{
	DWORD Return = E_FAIL;
	if (LocalUserNum == LoggedInPlayerNum)
	{
		if (VoiceEngine != NULL)
		{
			// Find the specified talker
			FRemoteTalker* Talker = FindRemoteTalker(UniqueId);
			if (Talker != NULL)
			{
				// Remove them from the mute list
				MuteList.RemoveItem(UniqueId);
				Return = S_OK;
				debugf(NAME_DevOnline,TEXT("Muted talker 0x%016I64X"),(QWORD&)UniqueId);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Unknown remote talker specified to UnmuteRemoteTalker()"));
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("UnmuteRemoteTalker: Invalid LocalUserNum(%d) specified"),LocalUserNum);
	}
	return Return == S_OK;
}

/**
 * Tells the voice layer that networked processing of the voice data is allowed
 * for the specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to allow network transimission for
 */
void UOnlineSubsystemSteamworks::StartNetworkedVoice(BYTE LocalUserNum)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		CurrentLocalTalker.bHasNetworkedVoice = TRUE;
		// Since we don't leave the capturing on all the time to match a GameSpy bug, enable it now
		if (VoiceEngine)
		{
			VoiceEngine->StartLocalVoiceProcessing(LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid user specified in StartNetworkedVoice(%d)"),
			(DWORD)LocalUserNum);
	}
}

/**
 * Tells the voice layer to stop processing networked voice support for the
 * specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to disallow network transimission for
 */
void UOnlineSubsystemSteamworks::StopNetworkedVoice(BYTE LocalUserNum)
{
	// Validate the range of the entry
	if (LocalUserNum == LoggedInPlayerNum)
	{
		CurrentLocalTalker.bHasNetworkedVoice = FALSE;
		// Since we don't leave the capturing on all the time due to a GameSpy bug, disable it now
		if (VoiceEngine)
		{
			VoiceEngine->StopLocalVoiceProcessing(LocalUserNum);
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid user specified in StopNetworkedVoice(%d)"),
			(DWORD)LocalUserNum);
	}
}

/**
 * Tells the voice system to start tracking voice data for speech recognition
 *
 * @param LocalUserNum the local user to recognize voice data for
 *
 * @return true upon success, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::StartSpeechRecognition(BYTE LocalUserNum)
{
	DWORD Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->StartSpeechRecognition(LocalUserNum);
		debugf(NAME_DevOnline,TEXT("StartSpeechRecognition(%d) returned 0x%08X"),
			LocalUserNum,Return);
		if (Return == S_OK)
		{
			CurrentLocalTalker.bIsRecognizingSpeech = TRUE;
		}
	}
	return Return == S_OK;
}

/**
 * Tells the voice system to stop tracking voice data for speech recognition
 *
 * @param LocalUserNum the local user to recognize voice data for
 *
 * @return true upon success, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::StopSpeechRecognition(BYTE LocalUserNum)
{
	DWORD Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->StopSpeechRecognition(LocalUserNum);
		debugf(NAME_DevOnline,TEXT("StopSpeechRecognition(%d) returned 0x%08X"),
			LocalUserNum,Return);
		CurrentLocalTalker.bIsRecognizingSpeech = FALSE;
	}
	return Return == S_OK;
}

/**
 * Gets the results of the voice recognition
 *
 * @param LocalUserNum the local user to read the results of
 * @param Words the set of words that were recognized by the voice analyzer
 *
 * @return true upon success, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::GetRecognitionResults(BYTE LocalUserNum,TArray<FSpeechRecognizedWord>& Words)
{
	DWORD Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->GetRecognitionResults(LocalUserNum,Words);
		debugf(NAME_DevOnline,TEXT("GetRecognitionResults(%d,Array) returned 0x%08X"),
			LocalUserNum,Return);
	}
	return Return == S_OK;
}

/**
 * Changes the vocabulary id that is currently being used
 *
 * @param LocalUserNum the local user that is making the change
 * @param VocabularyId the new id to use
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::SelectVocabulary(BYTE LocalUserNum,INT VocabularyId)
{
	DWORD Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->SelectVocabulary(LocalUserNum,VocabularyId);
		debugf(NAME_DevOnline,TEXT("SelectVocabulary(%d,%d) returned 0x%08X"),
			LocalUserNum,VocabularyId,Return);
	}
	return Return == S_OK;
}

/**
 * Changes the object that is in use to the one specified
 *
 * @param LocalUserNum the local user that is making the change
 * @param SpeechRecogObj the new object use
 *
 * @param true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::SetSpeechRecognitionObject(BYTE LocalUserNum,USpeechRecognition* SpeechRecogObj)
{
	DWORD Return = E_FAIL;
	if (bIsUsingSpeechRecognition && VoiceEngine != NULL)
	{
		Return = VoiceEngine->SetRecognitionObject(LocalUserNum,SpeechRecogObj);
		debugf(NAME_DevOnline,TEXT("SetRecognitionObject(%d,%s) returned 0x%08X"),
			LocalUserNum,SpeechRecogObj ? *SpeechRecogObj->GetName() : TEXT("NULL"),Return);
	}
	return Return == S_OK;
}

/**
 * Sets the online status information to use for the specified player. Used to
 * tell other players what the player is doing (playing, menus, away, etc.)
 *
 * @param LocalUserNum the controller number of the associated user
 * @param StatusId the status id to use (maps to strings where possible)
 * @param LocalizedStringSettings the list of localized string settings to set
 * @param Properties the list of properties to set
 */
void UOnlineSubsystemSteamworks::SetOnlineStatus(BYTE LocalUserNum,INT StatusId,
	const TArray<FLocalizedStringSetting>& LocalizedStringSettings,
	const TArray<FSettingsProperty>& Properties)
{
	// @todo Steam: can't explicitly set this in Steamworks (but it handles user status, location elsewhere)
	//		UPDATE: I think this changed, as of SDK v1.13; test this out
}

/**
 * Sends a friend invite to the specified player
 *
 * @param LocalUserNum the user that is sending the invite
 * @param NewFriend the player to send the friend request to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::AddFriend(BYTE LocalUserNum,FUniqueNetId NewFriend,const FString& Message)
{
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_NotLoggedIn)
	{
		// Only send the invite if they aren't on the friend's list already
		const CSteamID SteamPlayerID((uint64)NewFriend.Uid);

		if (GSteamFriends->GetFriendRelationship(SteamPlayerID) == k_EFriendRelationshipNone)
		{
			// @todo Steam: this isn't right.
			GSteamFriends->ActivateGameOverlayToUser("steamid", SteamPlayerID);
		}

		return TRUE;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid or not logged in player specified (%d)"),(DWORD)LocalUserNum);
	}

	return FALSE;
}

/**
 * Sends a friend invite to the specified player nick
 *
 * This is done in two steps:
 *		1. Search for the player by unique nick
 *		2. If found, issue the request. If not, return an error
 *
 * @param LocalUserNum the user that is sending the invite
 * @param FriendName the name of the player to send the invite to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::AddFriendByName(BYTE LocalUserNum,const FString& FriendName,const FString& Message)
{
	// @todo Steam: Can't search for arbitrary users through Steamworks (is it possible to open Steam Community search page instead?)
	return FALSE;
}

/**
 * Removes a friend from the player's friend list
 *
 * @param LocalUserNum the user that is removing the friend
 * @param FormerFriend the player to remove from the friend list
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::RemoveFriend(BYTE LocalUserNum,FUniqueNetId FormerFriend)
{
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_UsingLocalProfile)
	{
		// Only remove if they are on the friend's list already
		const CSteamID SteamPlayerID((uint64) FormerFriend.Uid);

		if (GSteamFriends->GetFriendRelationship(SteamPlayerID) == k_EFriendRelationshipFriend)
		{
			// @todo Steam: this isn't right.
			GSteamFriends->ActivateGameOverlayToUser("steamid", SteamPlayerID);
		}

		return TRUE;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid or not logged in player specified (%d)"),(DWORD)LocalUserNum);
	}

	return FALSE;
}

/**
 * Used to accept a friend invite sent to this player
 *
 * @param LocalUserNum the user the invite is for
 * @param RequestingPlayer the player the invite is from
 *
 * @param true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::AcceptFriendInvite(BYTE LocalUserNum,FUniqueNetId NewFriend)
{
	// shouldn't hit this; Steam handles all this outside of the game.
	return FALSE;
}

/**
 * Used to deny a friend request sent to this player
 *
 * @param LocalUserNum the user the invite is for
 * @param RequestingPlayer the player the invite is from
 *
 * @param true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::DenyFriendInvite(BYTE LocalUserNum,FUniqueNetId RequestingPlayer)
{
	// shouldn't hit this; Steam handles all this outside of the game.
	return FALSE;
}

/**
 * Sends a message to a friend
 *
 * @param LocalUserNum the user that is sending the message
 * @param Friend the player to send the message to
 * @param Message the message to display to the recipient
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::SendMessageToFriend(BYTE LocalUserNum,FUniqueNetId Friend,const FString& Message)
{
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_UsingLocalProfile)
	{
		// @todo Steam: this isn't right
		const CSteamID SteamPlayerID((uint64)Friend.Uid);
		GSteamFriends->ActivateGameOverlayToUser("chat", SteamPlayerID);

		return TRUE;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid or not logged in player specified (%d)"),(DWORD)LocalUserNum);
	}

	return FALSE;
}

/**
 * Sends an invitation to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friend the player to send the invite to
 * @param Text ignored in Steamworks
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::SendGameInviteToFriend(BYTE LocalUserNum, FUniqueNetId Friend, const FString& Text)
{
	TArray<FUniqueNetId> IDList;
	IDList.AddItem(Friend);

	return SendGameInviteToFriends(LocalUserNum, IDList, Text);
}

/**
 * Sends invitations to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friends the player to send the invite to
 * @param Text ignored in Steamworks
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::SendGameInviteToFriends(BYTE LocalUserNum, const TArray<FUniqueNetId>& Friends, const FString& Text)
{
	UBOOL bSuccess = FALSE;

	if (GSteamFriends == NULL)
	{
		return FALSE;
	}


	// @todo Steam: Document LocationUrl (what is it?)

	// Determine the server address and UID
	FString ServerAddr;
	FString ServerUID;
	FSessionInfoSteam* CurSessionInfo = (FSessionInfoSteam*)(CachedGameInt != NULL ? CachedGameInt->SessionInfo : NULL);

	if (CurSessionInfo != NULL)
	{
		ServerAddr = CurSessionInfo->HostAddr.ToString(TRUE);

		if (CurSessionInfo->bSteamSockets)
		{
			ServerUID = FString::Printf(I64_FORMAT_TAG, CurSessionInfo->ServerUID);
		}
	}
	else
	{
		if (IsServer())
		{
			debugf(TEXT("SendGameInviteToFriends: Need an active game session to use invites, make sure to use CreateOnlineGame"));
		}
		else
		{
			debugf(TEXT("SendGameInviteToFriends: Need an active game session to use invites, make sure to use JoinOnlineGame"));
		}
	}


	FString ServerURL;

	if (!ServerAddr.IsEmpty())
	{
		ServerURL += FString::Printf(TEXT("-SteamConnectIP=%s"), *ServerAddr);
	}

	if (!ServerUID.IsEmpty())
	{
		ServerURL += FString::Printf(TEXT(" -SteamConnectUID=%s"), *ServerUID);
	}


	if (!ServerURL.IsEmpty())
	{
		bSuccess = TRUE;

		for (INT i=0; i<Friends.Num(); i++)
		{
			CSteamID FriendId(Friends(i).Uid);

			if (GSteamFriends->InviteUserToGame(FriendId, TCHAR_TO_UTF8(*ServerURL)))
			{
				debugf(NAME_DevOnline, TEXT("Inviting friend (") I64_FORMAT_TAG TEXT(") to game at URL: %s"),
					Friends(i).Uid, *ServerURL);
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Failed to invite friend (") I64_FORMAT_TAG TEXT(")"), Friends(i).Uid);
				bSuccess = FALSE;
			}
		}
	}

	return bSuccess;
}

/**
 * Tries to match up the IP on a game invite, to a friend UID and the steam sockets address of the server (if applicable)
 * NOTE: If there are multiple friends on the server, this may not accurately retrieve the friend UID, but will accurately retrieve
 *		the servers steam sockets address
 *
 * @param ServerAddr		The server the invite is for
 * @param OutFriendUID		Outputs the UID of the friend who sent the invite (if found)
 * @param OutSteamSocketsAddr	Outputs the steam sockets address of the server we've been invited to (if set)
 * @return			Whether or not we were able to match up the invite address to a friend
 */
UBOOL UOnlineSubsystemSteamworks::GetInviteFriend(FInternetIpAddr ServerAddr, QWORD& OutFriendUID, QWORD& OutSteamSocketsAddr)
{
	UBOOL bSuccess = FALSE;

	/**
	 * A problem with invites from the Steam UI, is that they only contain the server IP address, not the steam sockets address
	 * or other presence info.
	 *
	 * Since invites can only be received from friends though, we should be able to grab that data from the friend interfaces
	 * 'rich presence' functions, so try to determine if it's a steam sockets server (and if so, it's steam address) through this.
	 *
	 * An additional problem though, is we don't know which friend sent the invite (SteamAPI callbacks don't include that info
	 * for some reason, just the server IP); we can find this out though, by iterating the friend list, and matching up the IP of
	 * the server each friend is on (if any) to the IP passed to this callback.
	 */

	if (IsSteamClientAvailable() && GSteamFriends != NULL)
	{
		INT FriendCount = GSteamFriends->GetFriendCount(k_EFriendFlagImmediate);
		INT ServerPort = ServerAddr.GetPort();

		// Wipe the port from ServerAddr, as that is used to match just the IP
		ServerAddr.SetPort(0);

		// Iterate the friend list
		for (INT FriendIdx=0; FriendIdx<FriendCount; FriendIdx++)
		{
			CSteamID CurFriend = GSteamFriends->GetFriendByIndex(FriendIdx, k_EFriendFlagImmediate);
			FriendGameInfo_t FriendGame;

			// See if this friend is currently playing a game
			if (GSteamFriends->GetFriendGamePlayed(CurFriend, &FriendGame))
			{
				FInternetIpAddr FriendServerAddr;
				FriendServerAddr.SetIp(FriendGame.m_unGameIP);

				// If the friend was playing a game, check to see if that games IP matches the invite server IP
				if (FriendServerAddr == ServerAddr && FriendGame.m_usGamePort == ServerPort)
				{
					bSuccess = TRUE;
					OutFriendUID = CurFriend.ConvertToUint64();

					break;
				}
			}
		}


		// If the correct friend was found, check for 'rich presence' info (which will contain the server IP and steam sockets address)
		if (bSuccess)
		{
			OutSteamSocketsAddr = 0;

			FString PresenceServerURL;
			FString PresenceServerUID;
			FUniqueNetId FriendUID(OutFriendUID);

			if (GetFriendJoinURL(FriendUID, PresenceServerURL, PresenceServerUID) && !PresenceServerUID.IsEmpty())
			{
				OutSteamSocketsAddr = appAtoi64(*PresenceServerUID);
			}
		}
	}

	return bSuccess;
}

/**
 * Attempts to join a friend's game session (join in progress)
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friends the player to send the invite to
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::JoinFriendGame(BYTE LocalUserNum,FUniqueNetId Friend)
{
	// Steam handles this.
	return FALSE;
}

/**
 * Reads any data that is currently queued in the voice interface
 */
void UOnlineSubsystemSteamworks::ProcessLocalVoicePackets(void)
{
	UNetDriver* NetDriver = GWorld->GetNetDriver();
	// Skip if the netdriver isn't present, as there's no network to route data on
	if (VoiceEngine != NULL)
	{
		FVoiceInterfaceSteamworks* VoiceEngineOSS = (FVoiceInterfaceSteamworks*)VoiceEngine;

		// Only process the voice data if either network voice is enabled or
		// if the player is trying to have their voice issue commands
		if (CurrentLocalTalker.bHasNetworkedVoice ||
			CurrentLocalTalker.bIsRecognizingSpeech ||
			(VoiceEngineOSS != NULL && VoiceEngineOSS->bPendingFinalCapture))
		{
			// Read the data from any local talkers
			DWORD DataReadyFlags = VoiceEngine->GetVoiceDataReadyFlags();
			// See if the logged in player has data
			if (DataReadyFlags & (1 << LoggedInPlayerNum))
			{
				// Mark the person as talking
				DWORD SpaceAvail = MAX_VOICE_DATA_SIZE - GVoiceData.LocalPackets[LoggedInPlayerNum].Length;
				// Figure out if there is space for this packet
				if (SpaceAvail > 0)
				{
					DWORD NumPacketsCopied = 0;
					// Figure out where to append the data
					BYTE* BufferStart = GVoiceData.LocalPackets[LoggedInPlayerNum].Buffer;
					BufferStart += GVoiceData.LocalPackets[LoggedInPlayerNum].Length;
					// Copy the sender info
					GVoiceData.LocalPackets[LoggedInPlayerNum].Sender = LoggedInPlayerId;
					// Process this user
					DWORD hr = VoiceEngine->ReadLocalVoiceData(LoggedInPlayerNum,
						BufferStart,
						&SpaceAvail);
					if (hr == S_OK)
					{
						// If there is no net connection or they aren't allowed to transmit, skip processing
						if (NetDriver &&
							LoggedInStatus == LS_LoggedIn &&
							CurrentLocalTalker.bHasNetworkedVoice)
						{
							// Update the length based on what it copied
							GVoiceData.LocalPackets[LoggedInPlayerNum].Length += SpaceAvail;
							if (SpaceAvail > 0)
							{
								CurrentLocalTalker.bIsTalking = TRUE;
							}
						}
						else
						{
							// Zero out the data since it isn't to be sent via the network
							GVoiceData.LocalPackets[LoggedInPlayerNum].Length = 0;
						}
					}
				}
				else
				{
					// Buffer overflow, so drop previous data
					GVoiceData.LocalPackets[LoggedInPlayerNum].Length = 0;
				}
			}
		}
	}
}

/**
 * Submits network packets to the voice interface for playback
 */
void UOnlineSubsystemSteamworks::ProcessRemoteVoicePackets(void)
{
	// Skip if we aren't networked
	if (GWorld->GetNetDriver())
	{
		// Now process all pending packets from the server
		for (INT Index = 0; Index < GVoiceData.RemotePackets.Num(); Index++)
		{
			FVoicePacket* VoicePacket = GVoiceData.RemotePackets(Index);
			if (VoicePacket != NULL)
			{
				// If the player has muted every one skip the processing
				if (CurrentLocalTalker.MuteType < MUTE_All)
				{
					UBOOL bIsMuted = FALSE;
					// Check for friends only muting
					if (CurrentLocalTalker.MuteType == MUTE_AllButFriends)
					{
						bIsMuted = IsFriend(LoggedInPlayerNum,VoicePacket->Sender) == FALSE;
					}
					// Now check the mute list
					if (bIsMuted == FALSE)
					{
						bIsMuted = MuteList.FindItemIndex(VoicePacket->Sender) != -1;
					}
					// Skip if they are muted
					if (bIsMuted == FALSE)
					{
						// Get the size since it is an in/out param
						DWORD PacketSize = VoicePacket->Length;
						// Submit this packet to the voice engine
						DWORD hr = VoiceEngine->SubmitRemoteVoiceData(VoicePacket->Sender,
							VoicePacket->Buffer,
							&PacketSize);
#if _DEBUG
						if (hr != S_OK)
						{
							debugf(NAME_DevOnline,TEXT("SubmitRemoteVoiceData() failed with 0x%08X"),hr);
						}
#endif
					}
					// Skip all delegate handling if none are registered
					if (bIsMuted == FALSE && TalkingDelegates.Num() > 0)
					{
						// Find the remote talker and mark them as talking
						for (INT Index2 = 0; Index2 < RemoteTalkers.Num(); Index2++)
						{
							FRemoteTalker& Talker = RemoteTalkers(Index2);
							// Compare the xuids
							if (Talker.TalkerId == VoicePacket->Sender)
							{
								Talker.bIsTalking = TRUE;
							}
						}
					}
				}
				VoicePacket->DecRef();
			}
		}
		// Zero the list without causing a free/realloc
		GVoiceData.RemotePackets.Reset();
	}
}

/** Registers all of the local talkers with the voice engine */
void UOnlineSubsystemSteamworks::RegisterLocalTalkers(void)
{
	RegisterLocalTalker(LoggedInPlayerNum);
}

/** Unregisters all of the local talkers from the voice engine */
void UOnlineSubsystemSteamworks::UnregisterLocalTalkers(void)
{
	UnregisterLocalTalker(LoggedInPlayerNum);
}

/**
 * Determines whether the player is allowed to use voice or text chat online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemSteamworks::CanCommunicate(BYTE LocalUserNum)
{
	return FPL_Enabled;
}

/**
 * Determines whether the player is allowed to play matches online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemSteamworks::CanPlayOnline(BYTE LocalUserNum)
{
	// GameSpy/PC always says "yes",
	// GameSpy/PS3 says "no" if you have parental controls blocking it, but never blocks for cheating.
	// Live doesn't implement it.
	return FPL_Enabled;
}

/**
 * Determines if the ethernet link is connected or not
 */
UBOOL UOnlineSubsystemSteamworks::HasLinkConnection(void)
{
	return TRUE;
}

/**
 * Determines the NAT type the player is using
 */
BYTE UOnlineSubsystemSteamworks::GetNATType(void)
{
	if (GSteamUser != NULL)
	{
		return GSteamUser->BIsBehindNAT() ? NAT_Moderate : NAT_Open;
	}
	return NAT_Unknown;
}

/**
 * Starts an asynchronous read of the specified file from the network platform's
 * title specific file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ReadTitleFile(const FString& FileToRead)
{
	debugf(NAME_DevOnline, TEXT("ReadTitleFile('%s')"), *FileToRead);
	// @todo Steam: the "title file" and Steam Cloud are different things (one is global to the game, the other is local to the user).
#if 0
	OnlineSubsystemSteamworks_eventOnReadTitleFileComplete_Parms Parms(EC_EventParm);
	Parms.bWasSuccessful = GSteamRemoteStorage->FileExists(TCHAR_TO_UTF8(*FileToRead)) ? FIRST_BITFIELD : 0;
	Parms.Filename = FileToRead;
	// Use the common method to do the work
	TriggerOnlineDelegates(this,ReadTitleFileCompleteDelegates,&Parms);
	return TRUE;   // Steam Cloud syncs before the game launches.
#endif
	return FALSE;
}

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileToRead the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
UBOOL UOnlineSubsystemSteamworks::GetTitleFileContents(const FString& FileName,TArray<BYTE>& FileContents)
{
	// @todo Steam: the "title file" and Steam Cloud are different things (one is global to the game, the other is local to the user).
#if 0
	const int32 Size = GSteamRemoteStorage->GetFileSize(TCHAR_TO_UTF8(*FileName));
	if (Size < 0)
	{
		return FALSE;
	}
	FileContents.Reserve((INT) Size);
	if (GSteamRemoteStorage->FileRead(TCHAR_TO_UTF8(*FileName), &FileContents(0), Size) != Size)
	{
		FileContents.Empty();
		return FALSE;
	}
	return TRUE;
#endif
	return FALSE;
}

/**
 * Sets a new position for the network notification icons/images
 *
 * @param NewPos the new location to use
 */
void UOnlineSubsystemSteamworks::SetNetworkNotificationPosition(BYTE NewPos)
{
	CurrentNotificationPosition = NewPos;

	if (!IsSteamClientAvailable())
	{
		return;
	}

	// Map our enum to Steamworks (NOTE: There is not 'center' in Steam)
	switch (CurrentNotificationPosition)
	{
		case NNP_TopLeft:
		{
			GSteamUtils->SetOverlayNotificationPosition(k_EPositionTopLeft);
			break;
		}
		case NNP_TopRight:
		case NNP_TopCenter:
		{
			GSteamUtils->SetOverlayNotificationPosition(k_EPositionTopRight);
			break;
		}
		case NNP_CenterLeft:
		case NNP_BottomLeft:
		{
			GSteamUtils->SetOverlayNotificationPosition(k_EPositionBottomLeft);
			break;
		}
		case NNP_CenterRight:
		case NNP_Center:
		case NNP_BottomCenter:
		case NNP_BottomRight:
		{
			GSteamUtils->SetOverlayNotificationPosition(k_EPositionBottomRight);
			break;
		}
	}
}

/**
 * Starts an async read for the achievement list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param TitleId the title id of the game the achievements are to be read for
 * @param bShouldReadText whether to fetch the text strings or not
 * @param bShouldReadImages whether to fetch the image data or not
 *
 * @return TRUE if the task starts, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ReadAchievements(BYTE LocalUserNum, INT TitleId, UBOOL bShouldReadText, UBOOL bShouldReadImages)
{
	// @todo Steam: Is TitleId for checking different games?

	// @todo Steam: Implement bShouldReadImages and bShouldReadText; the problem is, these are only relevant when calling GetAchievements

	if (!IsSteamClientAvailable() || LocalUserNum != LoggedInPlayerNum)
	{
		return FALSE;
	}

	if (UserStatsReceivedState != OERS_InProgress && UserStatsReceivedState != OERS_Done)
	{
		UserStatsReceivedState = OERS_InProgress;
		GSteamUserStats->RequestCurrentStats();

		return TRUE;
	}


	OnlineSubsystemSteamworks_eventOnReadAchievementsComplete_Parms Parms(EC_EventParm);

	Parms.TitleId = TitleId;
	TriggerOnlineDelegates(this,AchievementReadDelegates,&Parms);

	return TRUE;
}


static UTexture2D* LoadSteamImageToTexture2D(const int Icon, const TCHAR *ImageName)
{
	uint32 Width, Height;
	if (!GSteamUtils->GetImageSize(Icon, &Width, &Height))
	{
		debugf(NAME_DevOnline, TEXT("Unexpected GetImageSize failure for Steam image %d ('%s')"), (INT) Icon, ImageName);
		return NULL;
	}

	TArray<BYTE> Buffer(Width * Height * 4);
	if (!GSteamUtils->GetImageRGBA(Icon, &Buffer(0), Buffer.Num()))
	{
		debugf(NAME_DevOnline, TEXT("Unexpected GetImageRGBA failure for Steam image %d ('%s')"), (INT) Icon, ImageName);
		return NULL;
	}

	UTexture2D* NewTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), INVALID_OBJECT, FName(ImageName));
	// disable compression, tiling, sRGB, and streaming for the texture
	NewTexture->CompressionNone			= TRUE;
	NewTexture->CompressionSettings		= TC_Default;
	NewTexture->MipGenSettings			= TMGS_NoMipmaps;
	NewTexture->CompressionNoAlpha		= TRUE;
	NewTexture->DeferCompression		= FALSE;
	NewTexture->bNoTiling				= TRUE;
	NewTexture->SRGB					= FALSE;
	NewTexture->NeverStream				= TRUE;
	NewTexture->LODGroup				= TEXTUREGROUP_UI;
	NewTexture->Init(Width,Height,PF_A8R8G8B8);
	// Only the first mip level is used
	check(NewTexture->Mips.Num() > 0);
	// Mip 0 is locked for the duration of the read request
	BYTE* MipData = (BYTE*)NewTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
	if (MipData)
	{
		// Calculate the row stride for the texture
		const INT TexStride = (Height / GPixelFormats[PF_A8R8G8B8].BlockSizeY) * GPixelFormats[PF_A8R8G8B8].BlockBytes;
		appMemzero(MipData, Height * TexStride);
		const BYTE *Src = (const BYTE *) &Buffer(0);
		for (uint32 Y = 0; Y < Height; Y++)
		{
			BYTE *Dest = MipData + (Y * TexStride);
			for (uint32 X = 0; X < Width; X++)
			{
				Dest[0] = Src[2];  // rgba to bgra
				Dest[1] = Src[1];
				Dest[2] = Src[0];
				Dest[3] = Src[3];
				Src += 4;
				Dest += 4;
			}
		}
		NewTexture->Mips(0).Data.Unlock();
		NewTexture->UpdateResource();   // Update the render resource for it
	}
	else
	{
		// Couldn't lock the mip
		debugf(NAME_DevOnline, TEXT("Failed to lock texture for Steam image %d ('%s')"), (INT) Icon, ImageName);
		NewTexture = NULL;   // let the garbage collector get it.
	}

	return NewTexture;
}

/**
 * Handle actual downloading of avatars from Steam
 *
 * @param PlayerNetId				The UID of the player who's avatar you want to download
 * @param Size					The desired size of the avatar (resulting size not guaranteed to match)
 * @param ReadOnlineAvatarCompleteDelegate	The delegate which will return the result of the avatar download
 * @param bTriggerOnFailure			Whether or not the delegate should be trigger if avatar downloading fails
 * @return					TRUE if successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::GetOnlineAvatar(const struct FUniqueNetId PlayerNetId, const INT Size,
							FScriptDelegate& ReadOnlineAvatarCompleteDelegate, const UBOOL bTriggerOnFailure)
{
	if (!IsSteamClientAvailable())
	{
		return FALSE;
	}


	UTexture2D* Avatar = NULL;
	const QWORD ProfileId = PlayerNetId.Uid;
	const CSteamID SteamPlayerId((uint64)ProfileId);

	int Icon = 0;
	int FinalSize = 0;

	if (Size < 64)
	{
		Icon = GSteamFriends->GetSmallFriendAvatar(SteamPlayerId);
		FinalSize = 32;
	}
	else if (Size < 184)
	{
		Icon = GSteamFriends->GetMediumFriendAvatar(SteamPlayerId);
		FinalSize = 64;
	}
	else
	{
		Icon = GSteamFriends->GetLargeFriendAvatar(SteamPlayerId);
		FinalSize = 184;
	}


	// Data for this user (0 == not available atm, -1 == still loading, will trigger callback, but we don't care about that).
	if (Icon > 0)
	{
		Avatar = LoadSteamImageToTexture2D(Icon, *FString::Printf(TEXT("Avatar_") I64_FORMAT_TAG TEXT("_%i_"), ProfileId, FinalSize));

		if (Avatar == NULL)
		{
			debugf(NAME_DevOnline, TEXT("GetOnlineAvatar() failed"));
		}

		if (Avatar != NULL || bTriggerOnFailure)
		{
			OnlineSubsystemSteamworks_eventOnReadOnlineAvatarComplete_Parms Parms(EC_EventParm);

			Parms.PlayerNetId = PlayerNetId;
			Parms.Avatar = Avatar;

			ProcessDelegate(NAME_None, &ReadOnlineAvatarCompleteDelegate, &Parms);
		}
	}

	return Avatar != NULL;
}

/**
 * Reads an avatar images for the specified player. Results are delivered via OnReadOnlineAvatarComplete delegates.
 *
 * @param PlayerNetId the unique id to read avatar for
 * @param ReadOnlineAvatarCompleteDelegate The delegate to call with results.
 */
void UOnlineSubsystemSteamworks::ReadOnlineAvatar(const struct FUniqueNetId PlayerNetId,INT Size,FScriptDelegate ReadOnlineAvatarCompleteDelegate)
{
	debugf(NAME_DevOnline, TEXT("ReadOnlineAvatar for ") I64_FORMAT_TAG, PlayerNetId.Uid);

	// this isn't async for Steam. The Steam client either has the information or it doesn't (it caches people in your buddy list, on the same server, etc).
	if (!GetOnlineAvatar(PlayerNetId, Size, ReadOnlineAvatarCompleteDelegate, FALSE))
	{
		// no data? Wait awhile, then re-request the buddy list, in case it shows up later (server's players' list catches up, etc).
		FQueuedAvatarRequest Request(EC_EventParm);
		Request.CheckTime = 0.0f;
		Request.NumberOfAttempts = 0;
		Request.PlayerNetId = PlayerNetId;
		Request.Size = Size;
		Request.ReadOnlineAvatarCompleteDelegate = ReadOnlineAvatarCompleteDelegate;
		QueuedAvatarRequests.AddItem(Request);
	}
}

static void LoadAchievementDetails(TArray<FAchievementDetails>& Achievements, TArray<FAchievementMappingInfo>& AchievementLoadList,
					UBOOL bLoadImages=TRUE)
{
	unsigned int Index = 0;
	unsigned int ListLen = AchievementLoadList.Num();
	FString AchievementIdString;
	int Icon = 0;
	bool bAchieved;

	while (ListLen == 0 || Index < ListLen)  // If ListLen == 0, we'll break out when we run out of achievements
	{
		if (ListLen == 0)
			AchievementIdString = FString::Printf(TEXT("Achievement_%u"), Index);
		else
			AchievementIdString = AchievementLoadList(Index).AchievementName.ToString();


		FString AchievementName(UTF8_TO_TCHAR(GSteamUserStats->GetAchievementDisplayAttribute(TCHAR_TO_ANSI(*AchievementIdString), "name")));

		if (AchievementName.Len() == 0)
			break;


		if (bLoadImages)
		{
			// Disable warnings from the SDK for just this call, as we run until we fail.
			GSteamUtils->SetWarningMessageHook(SteamworksWarningMessageHookNoOp);
			Icon = GSteamUserStats->GetAchievementIcon(TCHAR_TO_ANSI(*AchievementIdString));
			GSteamUtils->SetWarningMessageHook(SteamworksWarningMessageHook);
		}


		FString AchievementDesc(UTF8_TO_TCHAR(GSteamUserStats->GetAchievementDisplayAttribute(TCHAR_TO_ANSI(*AchievementIdString), "desc")));
		FString ImageName = FString("AchievementImage_") + FString(AchievementName).Replace(TEXT(" "),TEXT("_")) + FString(TEXT("_"));

		UTexture2D* NewTexture = (Icon != 0 ? LoadSteamImageToTexture2D(Icon, *ImageName) : NULL);

		debugf(NAME_DevOnline, TEXT("Achievement #%d: '%s', '%s', Icon '%i'"), Index, *AchievementName, *AchievementDesc, Icon);

		const INT Position = Achievements.AddZeroed();
		FAchievementDetails &Achievement = Achievements(Position);
		Achievement.Id = Index;
		Achievement.AchievementName = AchievementName;
		Achievement.Description = AchievementDesc;
		Achievement.Image = NewTexture;

		if (GSteamUserStats->GetAchievement(TCHAR_TO_ANSI(*AchievementIdString), &bAchieved) && bAchieved)
		{
			Achievement.bWasAchievedOnline = TRUE;
			Achievement.bWasAchievedOffline = TRUE;
		}

		Index++;
	}
}

/**
 * Copies the list of achievements for the specified player and title id
 * NOTE: Achievement pictures are not guaranteed to be set, you will need to repeatedly call this in order to load missing pictures
 *		Check the 'Images' value for all entries in the returned achievements list, to detect missing images
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Achievements the out array that receives the copied data
 * @param TitleId the title id of the game that these were read for
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
BYTE UOnlineSubsystemSteamworks::GetAchievements(BYTE LocalUserNum, TArray<FAchievementDetails>& Achievements, INT TitleId)
{
	Achievements.Reset();

	if (!IsSteamClientAvailable() || LocalUserNum != LoggedInPlayerNum)
	{
		return OERS_NotStarted;
	}

	if (UserStatsReceivedState == OERS_Done)
	{
		LoadAchievementDetails(Achievements, AchievementMappings);
	}

	return UserStatsReceivedState;
}

/**
 * Unlocks the specified achievement for the specified user
 *
 * @param LocalUserNum the controller number of the associated user
 * @param AchievementId the id of the achievement to unlock
 *
 * @return TRUE if the call worked, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::UnlockAchievement(BYTE LocalUserNum,INT AchievementId,FLOAT PercentComplete)
{
	// Validate the user index
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		FString AchievementIdString;

		if (AchievementMappings.Num() == 0)
		{
			AchievementIdString = FString::Printf(TEXT("Achievement_%u"), AchievementId);
		}
		else
		{
			UBOOL bFound = FALSE;

			for (INT i=0; i<AchievementMappings.Num(); i++)
			{
				if (AchievementMappings(i).AchievementId == AchievementId)
				{
					AchievementIdString = AchievementMappings(i).AchievementName.ToString();

					bFound = TRUE;
					break;
				}
			}

			if (!bFound)
			{
				debugf(TEXT("UnlockAchievement: AchievementId '%i' not found in AchievementMappings list"), AchievementId);
				return FALSE;
			}
		}


		bool bAchieved = false;

		if (GSteamUserStats->GetAchievement(TCHAR_TO_ANSI(*AchievementIdString), &bAchieved) && bAchieved)
		{
			// We already have this achievement unlocked; just trigger and say we unlocked it
			FAsyncTaskDelegateResults Results(ERROR_SUCCESS);
			TriggerOnlineDelegates(this, AchievementDelegates, &Results);

			return TRUE;
		}

		if (!GSteamUserStats->SetAchievement(TCHAR_TO_ANSI(*AchievementIdString)))
		{
			debugf(NAME_DevOnline, TEXT("SetAchievement() failed!"));

			FAsyncTaskDelegateResults Results(E_FAIL);
			TriggerOnlineDelegates(this, AchievementDelegates, &Results);

			return FALSE;
		}

		bStoringAchievement = TRUE;

		if (!GSteamUserStats->StoreStats())
		{
			debugf(NAME_DevOnline, TEXT("StoreStats() failed!"));

			bStoringAchievement = FALSE;

			FAsyncTaskDelegateResults Results(E_FAIL);
			TriggerOnlineDelegates(this, AchievementDelegates, &Results);

			return FALSE;
		}
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Invalid player index (%i) specified to UnlockAchievement()"), LocalUserNum);
		return FALSE;
	}

	return TRUE;
}


/**
 * Sets up the specified leaderboard, so that read/write calls can be performed on it
 *
 * @param LeaderboardName	The name of the leaderboard to initiate
 * @return			Returns TRUE if the leaderboard is being setup, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::InitiateLeaderboard(const FString& LeaderboardName)
{
	UBOOL bResult = FALSE;
	INT ListIndex = INDEX_NONE;

	// Make sure the leaderboard isn't in 'LeaderboardList'
	for (INT i=0; i<LeaderboardList.Num(); i++)
	{
		if (LeaderboardList(i).LeaderboardName == LeaderboardName)
		{
			ListIndex = i;
			break;
		}
	}

	if (ListIndex == INDEX_NONE || (!LeaderboardList(ListIndex).bLeaderboardInitiated && !LeaderboardList(ListIndex).bLeaderboardInitializing))
	{
		if (IsSteamClientAvailable())
		{
			bResult = TRUE;

			// Add the LeaderboardList entry (if not already added)
			if (ListIndex == INDEX_NONE)
			{
				ListIndex = LeaderboardList.AddZeroed(1);
			}

			LeaderboardList(ListIndex).LeaderboardName = LeaderboardName;
			LeaderboardList(ListIndex).bLeaderboardInitializing = TRUE;

			// Kickoff the find request, and setup the returned callback
			SteamAPICall_t ApiCall = GSteamUserStats->FindLeaderboard(TCHAR_TO_UTF8(*LeaderboardName));
			GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamFindLeaderboard(this, ApiCall));
		}
	}
	else if (LeaderboardList(ListIndex).bLeaderboardInitiated)
	{
		debugf(TEXT("Leaderboard name '%s' already exists in LeaderboardList; check LeaderboardList before calling"), *LeaderboardName);
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Reads entries from the specified leaderboard
 * NOTE: If 'PlayerList' is specified, all other parameters (bar LeaderboardName) are ignored, and just those players entries are retrieved
 *
 * NOTE: The Start/End parameters determine the range of entries to return, and function differently depending upon RequestType:
 *	LRT_Global: Start/End start from the top of the leaderboard (which is 0)
 *	LRT_Player: Start/End start at the players location in the leaderboard, and can be + or - values (with End always greater than Start)
 *	LRT_Friends: Only people in the players Steam friends list are returned from the leaderboard
 *
 * @param LeaderboardName	The name of the leaderboard to access
 * @param RequestType		The type of entries to return from the leaderboard
 * @param Start				Dependant on RequestType, the start location for entries that should be returned from in the leaderboard
 * @param End				As above, but with respect to the end location, where entries should stop being returned
 * @param PlayerList		Retrieves leaderboard data for these players >ONLY<; ignores all other parameters, bar LeaderboardName
 * @return			Whether or not leaderboard entry reading failed/succeeded
 */
UBOOL UOnlineSubsystemSteamworks::ReadLeaderboardEntries(const FString& LeaderboardName, BYTE RequestType/*=LRT_Global*/, INT Start/*=0*/,
								INT End/*=0*/, const TArray<FUniqueNetId>* PlayerList/*=NULL*/)
{
	UBOOL bResult = FALSE;
	INT ListIndex = INDEX_NONE;

	// Find the leaderboard in 'LeaderboardList'
	for (INT i=0; i<LeaderboardList.Num(); i++)
	{
		if (LeaderboardList(i).LeaderboardName == LeaderboardName)
		{
			ListIndex = i;
			break;
		}
	}

	if (IsSteamClientAvailable())
	{
		// Make sure the leaderboard is initiated, and if not, defer the read request and automatically initiate it
		if (ListIndex != INDEX_NONE && LeaderboardList(ListIndex).bLeaderboardInitiated)
		{
			// If the leaderboard has a valid Steam leaderboard handle set, kick off the read request
			if (LeaderboardList(ListIndex).LeaderboardRef != NULL)
			{
				bResult = TRUE;

				// If 'PlayerList' was specified, it overrides all other parameters and only returns leaderboard results for
				//	the specified UID's
				if (PlayerList != NULL && PlayerList->Num() > 0)
				{
					const TArray<FUniqueNetId>& InPlayerList = *PlayerList;
					CSteamID* SteamIdList = new CSteamID[PlayerList->Num()];

					// Transfer the PlayerList UID's to SteamIdList
					for (INT i=0; i<InPlayerList.Num(); i++)
					{
						if (i >= 100)
						{
							debugf(NAME_DevOnline, TEXT("Can only submit a maximum of 100 UID's for leaderboard reading"));
							break;
						}

						SteamIdList[i] = CSteamID((uint64)InPlayerList(i).Uid);
					}

					// Kick off the read
					SteamAPICall_t ApiCall = GSteamUserStats->DownloadLeaderboardEntriesForUsers(
													LeaderboardList(ListIndex).LeaderboardRef,
													SteamIdList, InPlayerList.Num());

					GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamDownloadLeaderboardEntries(this, ApiCall));


					delete[] SteamIdList;
				}
				else
				{
					ELeaderboardDataRequest SubRequestType = k_ELeaderboardDataRequestGlobal;

					if (RequestType == LRT_Player)
					{
						SubRequestType = k_ELeaderboardDataRequestGlobalAroundUser;
					}
					else if (RequestType == LRT_Friends)
					{
						SubRequestType = k_ELeaderboardDataRequestFriends;
					}

					SteamAPICall_t ApiCall = GSteamUserStats->DownloadLeaderboardEntries(LeaderboardList(ListIndex).LeaderboardRef,
												SubRequestType, Start, End);

					GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamDownloadLeaderboardEntries(this, ApiCall));
				}
			}
			else
			{
				debugf(TEXT("Invalid leaderboard handle for leaderboard '%s'"), *LeaderboardList(ListIndex).LeaderboardName);
			}
		}
		else
		{
			// Initialize the leaderboard, if it hasn't been already, and store the read request for execution later
			AWorldInfo* WI = GWorld != NULL ? GWorld->GetWorldInfo() : NULL;

			INT DefIndex = INDEX_NONE;

			// Check for a matching entry first (using 'AddUniqueItem' doesn't compile for some reason)
			for (INT i=0; i<DeferredLeaderboardReads.Num(); i++)
			{
				if (DeferredLeaderboardReads(i).LeaderboardName == LeaderboardName &&
					DeferredLeaderboardReads(i).RequestType == RequestType && DeferredLeaderboardReads(i).Start == Start &&
					DeferredLeaderboardReads(i).End == End)
				{
					TArray<FUniqueNetId>& DefPlayerList = DeferredLeaderboardReads(i).PlayerList;

					// Check that the player lists match
					if (DefPlayerList.Num() == 0 && PlayerList == NULL)
					{
						DefIndex = i;
						break;
					}
					else if (PlayerList != NULL && DefPlayerList.Num() == PlayerList->Num())
					{
						const TArray<FUniqueNetId>& InPlayerList = *PlayerList;
						UBOOL bMatch = TRUE;

						for (INT j=0; j<InPlayerList.Num(); j++)
						{
							if (InPlayerList(j) != DefPlayerList(j))
							{
								bMatch = FALSE;
								break;
							}
						}

						if (bMatch)
						{
							DefIndex = i;
							break;
						}
					}
				}
			}

			// Add the entry if it's not already in the list
			if (DefIndex == INDEX_NONE)
			{
				DefIndex = DeferredLeaderboardReads.AddZeroed(1);

				DeferredLeaderboardReads(DefIndex).LeaderboardName = LeaderboardName;
				DeferredLeaderboardReads(DefIndex).RequestType = RequestType;
				DeferredLeaderboardReads(DefIndex).Start = Start;
				DeferredLeaderboardReads(DefIndex).End = End;

				if (PlayerList != NULL)
				{
					DeferredLeaderboardReads(DefIndex).PlayerList.Append(*PlayerList);
				}
			}


			// Now kickoff initialization (if not already under way)
			if (ListIndex == INDEX_NONE || !LeaderboardList(ListIndex).bLeaderboardInitiated)
			{
				bResult = InitiateLeaderboard(LeaderboardName);

				// If initialization fails, remove the deferred entry
				if (!bResult)
				{
					DeferredLeaderboardReads.Remove(DefIndex, 1);
				}
			}
			else
			{
				bResult = TRUE;
			}
		}
	}

	return bResult;
}

/**
 * Writes out the current players score, for the specified leaderboard
 *
 * @param LeaderboardName	The name of the leaderboard to write to
 * @param Score				The score value to submit to the leaderboard.
 * @param LeaderboardData	Data to be stored alongside the leaderboard
 * @return Whether or not the leaderboard score writing failed/succeeded
 */
UBOOL UOnlineSubsystemSteamworks::WriteLeaderboardScore(const FString& LeaderboardName, INT Score, const TArray<INT>& LeaderboardData)
{
	UBOOL bResult = FALSE;
	INT ListIndex = INDEX_NONE;

	if (IsSteamClientAvailable())
	{
		// Find the leaderboard in 'LeaderboardList'
		for (INT i=0; i<LeaderboardList.Num(); i++)
		{
			if (LeaderboardList(i).LeaderboardName == LeaderboardName)
			{
				ListIndex = i;
				break;
			}
		}

		// Make sure the leaderboard is initiated, and if not, defer the write request and automatically initiate it
		if (ListIndex != INDEX_NONE && LeaderboardList(ListIndex).bLeaderboardInitiated)
		{
			// If the leaderboard has a valid Steam leaderboard handle set, kick off the upload request
			if (LeaderboardList(ListIndex).LeaderboardRef != NULL)
			{
				bResult = TRUE;

				ELeaderboardUploadScoreMethod UpdateType = (LeaderboardList(ListIndex).UpdateType == LUT_KeepBest ?
										k_ELeaderboardUploadScoreMethodKeepBest :
										k_ELeaderboardUploadScoreMethodForceUpdate);

				if (LeaderboardData.Num() > 0)
				{
					// Leaderboard entry is being written with stats data; format the stats data for input
					// NOTE: Could use LeaderboardData.GetData(), or appMemcpy, but copying like this is a minor overhead
					INT ScoreDetailsCount = LeaderboardData.Num();
					int32* ScoreDetails = new int32[ScoreDetailsCount];

					for (INT i=0; i<ScoreDetailsCount; i++)
					{
						ScoreDetails[i] = LeaderboardData(i);
					}


					SteamAPICall_t ApiCall = GSteamUserStats->UploadLeaderboardScore(LeaderboardList(ListIndex).LeaderboardRef,
												UpdateType, Score, ScoreDetails, ScoreDetailsCount);

					GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamUploadLeaderboardScore(this, ApiCall));

					delete[] ScoreDetails;
				}
				else
				{
					SteamAPICall_t ApiCall = GSteamUserStats->UploadLeaderboardScore(LeaderboardList(ListIndex).LeaderboardRef,
														UpdateType, Score, NULL, 0);

					GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamUploadLeaderboardScore(this, ApiCall));
				}
			}
			else
			{
				debugf(TEXT("Invalid leaderboard handle for leaderboard '%s'"), *LeaderboardList(ListIndex).LeaderboardName);
			}
		}
		else
		{
			// Initialize the leaderboard, if it hasn't been already, and store the write request for execution later
			AWorldInfo* WI = GWorld != NULL ? GWorld->GetWorldInfo() : NULL;

			INT DefIndex = INDEX_NONE;

			// Check for a matching entry first (using 'AddUniqueItem' doesn't compile for some reason)
			for (INT i=0; i<DeferredLeaderboardWrites.Num(); i++)
			{
				if (DeferredLeaderboardWrites(i).LeaderboardName == LeaderboardName && DeferredLeaderboardWrites(i).Score == Score)
				{
					DefIndex = i;
					break;
				}
			}

			// Add the entry if it's not already in the list
			if (DefIndex == INDEX_NONE)
			{
				DefIndex = DeferredLeaderboardWrites.AddZeroed(1);

				DeferredLeaderboardWrites(DefIndex).LeaderboardName = LeaderboardName;
				DeferredLeaderboardWrites(DefIndex).Score = Score;
				DeferredLeaderboardWrites(DefIndex).LeaderboardData.Append(LeaderboardData);
			}


			// Now kickoff initialization (if not already under way)
			if (ListIndex == INDEX_NONE || !LeaderboardList(ListIndex).bLeaderboardInitiated)
			{
				bResult = InitiateLeaderboard(LeaderboardName);

				// If initialization fails, remove the deferred entry
				if (!bResult)
				{
					DeferredLeaderboardWrites.Remove(DefIndex, 1);
				}
			}
			else
			{
				bResult = TRUE;
			}
		}
	}

	return bResult;
}

/**
 * Creates the specified leaderboard on the Steamworks backend
 * NOTE: It's best to use this for game/mod development purposes only, not for release usage
 *
 * @param LeaderboardName	The name to give the leaderboard (NOTE: This will be the human-readable name displayed on the backend and stats page)
 * @param SortType		The sorting to use for the leaderboard
 * @param DisplayFormat		The way to display leaderboard data
 * @return			Returns True if the leaderboard is being created, False otherwise
 */
UBOOL UOnlineSubsystemSteamworks::CreateLeaderboard(const FString& LeaderboardName, BYTE SortType, BYTE DisplayFormat)
{
	UBOOL bResult = FALSE;
	INT Index = INDEX_NONE;

	// If the leaderboard is already in 'LeaderboardList', it already exists on the backend
	for (INT i=0; i<LeaderboardList.Num(); i++)
	{
		if (LeaderboardList(i).LeaderboardName == LeaderboardName)
		{
			Index = i;
			break;
		}
	}

	if (Index == INDEX_NONE)
	{
		if (IsSteamClientAvailable())
		{
			bResult = TRUE;

			// Kickoff the find/create request, and setup the returned callback
			ELeaderboardSortMethod SortMethod = (SortType == LST_Ascending ?
								k_ELeaderboardSortMethodAscending :
								k_ELeaderboardSortMethodDescending);

			ELeaderboardDisplayType DisplayType = k_ELeaderboardDisplayTypeNumeric;

			if (DisplayFormat == LF_Seconds)
			{
				DisplayType = k_ELeaderboardDisplayTypeTimeSeconds;
			}
			else if (DisplayFormat == LF_Milliseconds)
			{
				DisplayType = k_ELeaderboardDisplayTypeTimeMilliSeconds;
			}


			// NOTE: Leaderboard creation is asynchronous, so this function may return True, yet still fail to create the leaderboard
			SteamAPICall_t ApiCall = GSteamUserStats->FindOrCreateLeaderboard(TCHAR_TO_UTF8(*LeaderboardName), SortMethod, DisplayType);
			GSteamAsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskSteamFindLeaderboard(this, ApiCall));
		}
	}
	else
	{
		debugf(TEXT("CreateLeaderboard: Leaderboard '%s' has already been created"), *LeaderboardName);
	}

	return bResult;
}


/**
 * Displays the Steam Friends UI
 *
 * @param LocalUserNum the controller number of the user where are showing the friends for
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ShowFriendsUI(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;

	// Validate the user index passed in
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		// Show the friends UI
		GSteamFriends->ActivateGameOverlay("Friends");
		Result = ERROR_SUCCESS;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Invalid player index (%i) specified to ShowFriendsUI()"), LocalUserNum);
	}

	return Result == ERROR_SUCCESS;
}

/**
 * Displays the Steam Friends Invite Request UI
 *
 * @param LocalUserNum the controller number of the user where are showing the friends for
 * @param UniqueId the id of the player being invited (null or 0 to have UI pick)
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ShowFriendsInviteUI(BYTE LocalUserNum, FUniqueNetId UniqueId)
{
	DWORD Result = E_FAIL;

	// Validate the user index passed in
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		// Show the friends UI for the specified controller num
		const CSteamID SteamPlayerID((uint64)UniqueId.Uid);

		// @todo Steam: not exactly right
		GSteamFriends->ActivateGameOverlayToUser("steamid", SteamPlayerID);
		Result = ERROR_SUCCESS;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Invalid player index (%i) specified to ShowFriendsInviteUI()"), LocalUserNum);
	}

	return Result == ERROR_SUCCESS;
}

/**
 * Displays the UI that allows a player to give feedback on another player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param UniqueId the id of the player having feedback given for
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ShowFeedbackUI(BYTE LocalUserNum, FUniqueNetId UniqueId)
{
	DWORD Result = E_FAIL;

	// Validate the user index passed in
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		// There's a "comment" field on the profile page. I'm not sure if this qualifies as "feedback" though
		const CSteamID SteamPlayerID((uint64)UniqueId.Uid);
		GSteamFriends->ActivateGameOverlayToUser("steamid", SteamPlayerID);
		Result = ERROR_SUCCESS;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Invalid player index (%i) specified to ShowFeedbackUI()"), LocalUserNum);
	}

	return Result == ERROR_SUCCESS;
}

/**
 * Displays the gamer card UI for the specified player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param UniqueId the id of the player to show the gamer card of
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ShowGamerCardUI(BYTE LocalUserNum, FUniqueNetId UniqueId)
{
	DWORD Result = E_FAIL;

	// Validate the user index passed in
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		// No "gamer card" but we can bring up the player's profile in the overlay
		const CSteamID SteamPlayerID((uint64)UniqueId.Uid);
		GSteamFriends->ActivateGameOverlayToUser("steamid", SteamPlayerID);
		Result = ERROR_SUCCESS;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Invalid player index (%i) specified to ShowGamerCardUI()"), LocalUserNum);
	}

	return Result == ERROR_SUCCESS;
}

/**
 * Displays the messages UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ShowMessagesUI(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;

	// Validate the user index passed in
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		// Open our own profile page
		GSteamFriends->ActivateGameOverlayToUser("steamid", CSteamID(LoggedInPlayerId.Uid));
		Result = ERROR_SUCCESS;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Invalid player index (%i) specified to ShowMessagesUI()"), LocalUserNum);
	}

	return Result == ERROR_SUCCESS;
}

/**
 * Displays the achievements UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ShowAchievementsUI(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;

	// Validate the user index passed in
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		GSteamFriends->ActivateGameOverlay("Achievements");
		Result = ERROR_SUCCESS;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Invalid player index (%i) specified to ShowAchievementsUI()"), LocalUserNum);
	}

	return Result == ERROR_SUCCESS;
}
	

/**
 * Displays the players UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ShowPlayersUI(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;

	// Validate the user index passed in
	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		GSteamFriends->ActivateGameOverlay("Players");
		Result = ERROR_SUCCESS;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%i) specified to ShowPlayersUI()"), LocalUserNum);
	}

	return Result == ERROR_SUCCESS;
}

/**
 * Displays the invite ui
 *
 * @param LocalUserNum the local user sending the invite
 * @param InviteText the string to prefill the UI with
 */
UBOOL UOnlineSubsystemSteamworks::ShowInviteUI(BYTE LocalUserNum, const FString& InviteText)
{
	// @todo Steam: what to do with this?
	return FALSE;
}

/**
 * Shows a custom players UI for the specified list of players
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Players the list of players to show in the custom UI
 * @param Title the title to use for the UI
 * @param Description the text to show at the top of the UI
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemSteamworks::ShowCustomPlayersUI(BYTE LocalUserNum,const TArray<FUniqueNetId>& Players,const FString& Title,const FString& Description)
{
	// @todo Steam: nothing like this in Steam at the moment
	return FALSE;
}

/**
 * Displays the marketplace UI for content
 *
 * @param LocalUserNum the local user viewing available content
 * @param CategoryMask the bitmask to use to filter content by type
 * @param OfferId a specific offer that you want shown
 */
UBOOL UOnlineSubsystemSteamworks::ShowContentMarketplaceUI(BYTE LocalUserNum,INT CategoryMask,INT OfferId)
{
	// @todo Steam: The SDK implemented a microntransaction system; perhaps examine if it should be tied in?
	return FALSE;  // probably doesn't make sense on Steam (and Live for PC returns FALSE here, too!)
}
	
/**
 * Displays the marketplace UI for memberships
 *
 * @param LocalUserNum the local user viewing available memberships
 */
UBOOL UOnlineSubsystemSteamworks::ShowMembershipMarketplaceUI(BYTE LocalUserNum)
{
	return FALSE;  // probably doesn't make sense on Steam (and Live for PC returns FALSE here, too!)
}

/**
 * Resets the players stats (and achievements, if specified)
 *
 * @param bResetAchievements	If true, also resets player achievements
 * @return			TRUE if successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ResetStats(UBOOL bResetAchievements)
{
	UBOOL bResult = FALSE;

#if FORBID_STEAM_RESET_STATS
	debugf(TEXT("Resetting of Steam user stats is disabled"));
#else
	debugf(TEXT("Resetting Steam user stats%s"), (bResetAchievements ? TEXT(" and achievements") : TEXT("")));

	if (GSteamUserStats != NULL && GSteamUserStats->ResetAllStats(bResetAchievements == TRUE) && GSteamUserStats->StoreStats())
	{
		bResult = TRUE;
	}
#endif

	return bResult;
}


/**
 * Pops up the Steam toast dialog, notifying the player of their progress with an achievement (does not unlock achievements)
 *
 * @param AchievementId		The id of the achievment which will have its progress displayed
 * @param ProgressCount		The number of completed steps for this achievement
 * @param MaxProgress		The total number of required steps for this achievement, before it will be unlocked
 * @return			TRUE if successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::DisplayAchievementProgress(INT AchievementId, INT ProgressCount, INT MaxProgress)
{
	UBOOL bResult = FALSE;

	// @todo Steam: Try to generalize this function into base subsystem eventually

	if (IsSteamClientAvailable())
	{
		FString AchievementIdString;

		if (AchievementMappings.Num() == 0)
		{
			AchievementIdString = FString::Printf(TEXT("Achievement_%u"), AchievementId);
		}
		else
		{
			UBOOL bFound = FALSE;

			for (INT i=0; i<AchievementMappings.Num(); i++)
			{
				if (AchievementMappings(i).AchievementId == AchievementId)
				{
					AchievementIdString = AchievementMappings(i).AchievementName.ToString();

					bFound = TRUE;
					break;
				}
			}

			if (!bFound)
			{
				debugf(TEXT("DisplayAchievementProgress: AchievementId '%i' not found in AchievementMappings list"), AchievementId);
				return FALSE;
			}
		}

		if (GSteamUserStats->IndicateAchievementProgress(TCHAR_TO_ANSI(*AchievementIdString), ProgressCount, MaxProgress))
		{
			bResult = TRUE;
		}
	}

	return bResult;
}


/**
 * Converts the specified UID, into the players Steam Community name
 *
 * @param UID		The players UID
 * @return		The username of the player, as stored on the Steam backend
 */
FString UOnlineSubsystemSteamworks::UniqueNetIdToPlayerName(const FUniqueNetId& UID)
{
	FString Result = TEXT("");

	if (IsSteamClientAvailable() && GSteamFriends != NULL)
	{
		const char* PlayerName = GSteamFriends->GetFriendPersonaName(CSteamID((uint64)UID.Uid));
		Result = FString(UTF8_TO_TCHAR(PlayerName));
	}

	return Result;
}

/**
 * Shows the current (or specified) players Steam profile page, with an optional sub-URL (e.g. for displaying leaderboards)
 *
 * @param LocalUserNum		The controller number of the associated user
 * @param SubURL		An optional sub-URL within the players main profile URL
 * @param PlayerUID		If you want to show the profile of a specific player, pass their UID in here
 */
UBOOL UOnlineSubsystemSteamworks::ShowProfileUI(BYTE LocalUserNum, const FString& SubURL/*=TEXT("")*/,
							FUniqueNetId PlayerUID/*=FUniqueNetId(EC_EventParm)*/)
{
	UBOOL bResult = FALSE;

	if (IsSteamClientAvailable() && LocalUserNum == LoggedInPlayerNum)
	{
		QWORD TargetUID;

		if (PlayerUID.Uid == 0)
		{
			TargetUID = (QWORD)GSteamUser->GetSteamID().ConvertToUint64();
		}
		else
		{
			TargetUID = (QWORD)PlayerUID.Uid;
		}

		FString ProfileURL = FString::Printf(TEXT("%s/%I64d/"), TEXT("http://steamcommunity.com/profiles"), TargetUID);

		if (SubURL != TEXT(""))
		{
			// Do string validation before appending the URL
			UBOOL bInvalid = FALSE;
			TCHAR* SubChr = (TCHAR*)*SubURL;

			while (*SubChr != '\0')
			{
				if (!appIsAlnum(*SubChr) && *SubChr != '_' && *SubChr != '?' && *SubChr != '=' && *SubChr != ':' &&
					*SubChr != '\\' && *SubChr != '/')
				{
					bInvalid = TRUE;
					break;
				}

				SubChr++;
			}


			if (!bInvalid)
			{
				ProfileURL += SubURL;
			}
			else
			{
				debugf(TEXT("ShowProfileUI: Invalid characters in SubURL parameter; use a-z, A-Z, 0-9, _, :, ?, /, \\ and ="));
			}
		}

		GSteamFriends->ActivateGameOverlayToWebPage(TCHAR_TO_UTF8(*ProfileURL));

		bResult = TRUE;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Invalid player index (%d) specified to ShowProfileUI"), (DWORD)LocalUserNum);
	}

	return bResult;
}

/**
 * Internal function, for relaying VOIP AudioComponent 'Stop' events to the native code (so references are cleaned up properly)
 *
 * @param VOIPAudioComponent	The VOIP AudioComponent which has finished playing
 */
void UOnlineSubsystemSteamworks::NotifyVOIPPlaybackFinished(class UAudioComponent* VOIPAudioComponent)
{
	if (VoiceEngine != NULL)
	{
		FVoiceInterfaceSteamworks* VoiceEngineOSS = (FVoiceInterfaceSteamworks*)VoiceEngine;

		if (VoiceEngineOSS != NULL)
		{
			VoiceEngineOSS->NotifyVOIPPlaybackFinished(VOIPAudioComponent);
		}
	}
}

/**
 * Converts the specified UID, into a string representing a 64bit int
 * NOTE: Primarily for use with 'open Steam.#', when P2P sockets are enabled
 *
 * @param UID		The players UID
 * @return		The string representation of the 64bit integer, made from the UID
 */
FString UOnlineSubsystemSteamworks::UniqueNetIdToInt64(const FUniqueNetId& UID)
{
	FString Result = FString::Printf(I64_FORMAT_TAG, UID.Uid);
	return Result;
}

/**
 * Converts the specified string (representing a 64bit int), into a UID
 *
 * @param UIDString	The string representing a 64bit int
 * @param OutUID	The returned UID
 * @return		Whether or not the conversion was successful
 */
UBOOL UOnlineSubsystemSteamworks::Int64ToUniqueNetId(const FString& UIDString, FUniqueNetId& OutUID)
{
	UBOOL bResult = FALSE;

	// @todo Steam: Add and test an IsAlnum check here as well; it returns true for a-z strings
	if (!UIDString.IsEmpty())
	{
		OutUID.Uid = appAtoi64(*UIDString);
		bResult = TRUE;
	}

	return bResult;
}

/**
 * If the game was launched by a 'join friend' request in Steam, this function retrieves the server info from the commandline
 *
 * @param bMarkAsJoined	If True, future calls to this function return False (but still output the URL/UID)
 * @param ServerURL	The URL (IP:Port) of the server
 * @param ServerUID	The SteamId of the server
 * @return		Returns True if there is data available and the server needs to be joined
 */
UBOOL UOnlineSubsystemSteamworks::GetCommandlineJoinURL(UBOOL bMarkAsJoined, FString& ServerURL, FString& ServerUID)
{
	static UBOOL bJoinedCommandlineURL = FALSE;

	UBOOL bReturnVal = FALSE;

	TCHAR ParsedURL[1024];
	TCHAR ParsedUID[64];

	if (Parse(appCmdLine(), TEXT("SteamConnectIp="), ParsedURL, ARRAY_COUNT(ParsedURL)))
	{
		ServerURL = ParsedURL;

		if (Parse(appCmdLine(), TEXT("SteamConnectUID="), ParsedUID, ARRAY_COUNT(ParsedUID)))
		{
			ServerUID = ParsedUID;
		}
		else
		{
			ServerUID = TEXT("");
		}

		if (!bJoinedCommandlineURL)
		{
			bReturnVal = TRUE;
		}

		if (bMarkAsJoined)
		{
			bJoinedCommandlineURL = TRUE;
		}
	}

	return bReturnVal;
}

/**
 * Retrieves the URL/UID of the server a friend is currently in
 *
 * @param FriendUID	The UID of the friend
 * @param ServerURL	The URL (IP:Port) of the server
 * @param ServerUID	The SteamId of the server (if this is set, the server should be joined using steam sockets)
 * @return		Returns True if there is information available
 */
UBOOL UOnlineSubsystemSteamworks::GetFriendJoinURL(FUniqueNetId FriendUID, FString& ServerURL, FString& ServerUID)
{
	// @todo Steam: Implement a way of determining whether or not the server is using steam sockets (perhaps just simply >don't< return the server
	//		uid?)

	UBOOL bSuccess = FALSE;

	if (GSteamFriends != NULL)
	{
		CSteamID FriendId(FriendUID.Uid);


		// *** @todo Steam: Remove this debug code, when Valve fix the rich presence bugginess
		/*
		UBOOL bGrabDataResult = GSteamFriends->RequestUserInformation(FriendId, FALSE);

		debugf(NAME_DevOnline, TEXT("bGrabDataResult: %i"), (INT)bGrabDataResult);

		INT KeyCount = GSteamFriends->GetFriendRichPresenceKeyCount(FriendId);

		debugf(NAME_DevOnline, TEXT("KeyCount: %i"), KeyCount);

		for (INT i=0; i<KeyCount; i++)
		{
			TCHAR* CurKey = UTF8_TO_TCHAR(GSteamFriends->GetFriendRichPresenceKeyByIndex(FriendId, i));

			debugf(NAME_DevOnline, TEXT("Friend Key %i: %s"), i, CurKey);
		}
		*/
		// ***


		FString FriendInfo = UTF8_TO_TCHAR(GSteamFriends->GetFriendRichPresence(FriendId, TCHAR_TO_UTF8(TEXT("connect"))));

		if (!FriendInfo.IsEmpty())
		{
			// Parse the parameters
			TCHAR ParsedURL[1024];

			if (Parse(*FriendInfo, TEXT("SteamConnectIp="), ParsedURL, ARRAY_COUNT(ParsedURL)))
			{
				TCHAR ParsedUID[64];

				ServerURL = ParsedURL;

				if (Parse(*FriendInfo, TEXT("SteamConnectUID="), ParsedUID, ARRAY_COUNT(ParsedUID)))
				{
					ServerUID = ParsedUID;
				}
				else
				{
					ServerUID = TEXT("");
				}

				bSuccess = TRUE;
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Failed to parse friend join URL; raw data: %s"), *FriendInfo);
				bSuccess = FALSE;
			}
		}
		// *** @todo Steam: Remove this debug code (it's normal that this happens; doesn't warrant a log)
		else
		{
			debugf(NAME_DevOnline, TEXT("Player does not have 'connect' rich presence info"));
		}
		// ***
	}

	return bSuccess;
}

/**
 * Called internally by clients and listen hosts, to advertise the current server IP/UID so friends can join
 *
 * @param ServerIP	The public IP address of the server
 * @param ServerPort	The game port of the server
 * @param ServerUID	The steam address of the server
 * @param bSteamSockets	Whether or not the server uses steam sockets
 * @return		Whether or not the join info was correctly set
 */
UBOOL UOnlineSubsystemSteamworks::SetGameJoinInfo(DWORD ServerIP, INT ServerPort, QWORD ServerUID, UBOOL bSteamSockets)
{
	UBOOL bSuccess = FALSE;
	FInternetIpAddr ServerAddr;

	ServerAddr.SetIp(ServerIP);
	ServerAddr.SetPort(ServerPort);

	// NOTE: If you modify this call (put more conditions on it etc.), you break some code in the game interface;
	//		if you update this, be sure to update all code calling this function too
	if (CachedGameInt != NULL)
	{
		CachedGameInt->UpdateSessionInfo(ServerIP, ServerPort, ServerUID, bSteamSockets);
	}


	// NOTE: Don't put the server password in the join URL, as you don't want to leak server passwords
	FString JoinURL = FString::Printf(TEXT("-SteamConnectIP=%s"), *ServerAddr.ToString(TRUE));

#if WITH_STEAMWORKS_SOCKETS
	if (bSteamSockets && ServerUID != 0)
	{
		JoinURL += FString::Printf(TEXT(" -SteamConnectUID=") I64_FORMAT_TAG, ServerUID);
	}
#endif

	if (GSteamFriends != NULL && GSteamFriends->SetRichPresence(TCHAR_TO_UTF8(TEXT("connect")), TCHAR_TO_UTF8(*JoinURL)))
	{
		bSuccess = TRUE;
		debugf(NAME_DevOnline, TEXT("Successfully set the friend join URL: %s"), *JoinURL);
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Failed to set the friend join URL"));
	}

	return bSuccess;
}

/**
 * Called internally by clients and listen hosts, to clear advertising the current server info
 *
 * @return	Returns TRUE if successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemSteamworks::ClearGameJoinInfo()
{
	UBOOL bSuccess = FALSE;

	if (GSteamFriends != NULL && GSteamFriends->SetRichPresence(TCHAR_TO_UTF8(TEXT("connect")), TCHAR_TO_UTF8(TEXT(""))))
	{
		bSuccess = TRUE;
	}
	else
	{
		debugf(NAME_DevOnline, TEXT("Failed to clear the friend join URL"));
	}

	return bSuccess;
}


#if STEAM_EXEC_DEBUG

/**
 * Console command catcher for adding quick debug code
 */
UBOOL FSteamExecCatcher::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const TCHAR* Str = Cmd;
	UBOOL bSuccess = FALSE;

	static BYTE ListenAuthBlob[2048];
	static INT ListenBlobLen = 0;
	static HSteamPipe ServerPipe = NULL;


	// Testing workarounds to a bug where user rich presence only gets updated locally, not pushed to friends
	if (ParseCommand(&Str, TEXT("StoreStats")))
	{
		GSteamUserStats->StoreStats();

		return TRUE;
	}
	// Try to un-clog presence updates, when they don't get pushed to friends
	else if (ParseCommand(&Str, TEXT("BumpPresence")))
	{
		if (GSteamFriends != NULL)
		{
			static INT BumpVal = 0;
			FString BumpStr = FString::Printf(TEXT("%i"), BumpVal);

			BumpVal++;

			if (GSteamFriends->SetRichPresence(TCHAR_TO_UTF8(TEXT("blah")), TCHAR_TO_UTF8(*BumpStr)))
			{
				debugf(NAME_DevOnline, TEXT("Bumped presence"));
				bSuccess = TRUE;
			}
		}

		if (!bSuccess)
		{
			debugf(NAME_DevOnline, TEXT("Failed to bump presence"));
		}

		return TRUE;
	}
	// Debug setting of join presence info
	else if (ParseCommand(&Str, TEXT("SetJoinPresence")))
	{
		if (GSteamFriends != NULL)
		{
			FString JoinURL = TEXT("-SteamConnectIP=127.0.0.1:6666 -SteamConnectedUID=345345634734736");

			if (GSteamFriends->SetRichPresence(TCHAR_TO_UTF8(TEXT("connect")), TCHAR_TO_UTF8(*JoinURL)))
			{
				debugf(NAME_DevOnline, TEXT("Successfully set join URL"));
			}
			else
			{
				debugf(NAME_DevOnline, TEXT("Failed to set join URL"));
			}
		}

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("SetPresence")))
	{
		FString Key, Value;

		if (Parse(Str, TEXT("KEY="), Key) && Parse(Str, TEXT("VALUE="), Value))
		{
			if (GSteamFriends != NULL && GSteamFriends->SetRichPresence(TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value)))
			{
				Ar.Log(TEXT("Successfully rich presence value"));
			}
			else
			{
				Ar.Log(TEXT("Failed to set rich presence value"));
			}
		}
		else
		{
			Ar.Log(TEXT("Not enough parameters; usage: SetPresence Key=KeyName Value=Value"));
		}

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("GetPresence")))
	{
		FString UID;
		FString Key;

		if (Parse(Str, TEXT("UID="), UID) && Parse(Str, TEXT("KEY="), Key))
		{
			if (GSteamFriends != NULL)
			{
				CSteamID FriendId((uint64)appAtoi64(*UID));
				FString Value = UTF8_TO_TCHAR(GSteamFriends->GetFriendRichPresence(FriendId, TCHAR_TO_UTF8(*Key)));

				Ar.Logf(TEXT("Successfully got rich presence value: %s"), *Value);
			}
			else
			{
				Ar.Log(TEXT("Failed to get rich presence value"));
			}
		}
		else
		{
			Ar.Log(TEXT("Not enough parameters; usage: GetPresence UID=FriendUID Key=KeyName"));
		}

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("CopyUID")))
	{
		appClipboardCopy(*FString::Printf(I64_FORMAT_TAG, OnlineSub->LoggedInPlayerId.Uid));
		Ar.Log(TEXT("Copied UID to clipboard"));

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("DestroyOnlineGame")))
	{
		if (OnlineSub->GameInterfaceImpl != NULL)
		{
			debugf(NAME_DevOnline, TEXT("Destroying online game"));

			OnlineSub->GameInterfaceImpl->DestroyOnlineGame(NAME_None);
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("Failed to destroy online game; GameInterfaceImpl == NULL"));
		}

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("DumpAuthSessions")))
	{
		UOnlineAuthInterfaceSteamworks* CachedAuthInt = OnlineSub->CachedAuthInt;

		if (CachedAuthInt != NULL)
		{
			debugf(TEXT("ClientAuthSessions:"));

			for (INT i=0; i<CachedAuthInt->ClientAuthSessions.GetMaxIndex(); i++)
			{
				if (CachedAuthInt->ClientAuthSessions.IsAllocated(i))
				{
					debugf(TEXT("ClientAuthSessions(%i):"), i);

					FInternetIpAddr EndPointAddr;
					EndPointAddr.SetIp(CachedAuthInt->ClientAuthSessions(i).EndPointIP);

					debugf(TEXT("--- EndPointIP: %i (%s)"), CachedAuthInt->ClientAuthSessions(i).EndPointIP,
						*EndPointAddr.ToString(FALSE));

					debugf(TEXT("--- EndPointPort: %d"), CachedAuthInt->ClientAuthSessions(i).EndPointPort);
					debugf(TEXT("--- EndPointUID: ") I64_FORMAT_TAG, CachedAuthInt->ClientAuthSessions(i).EndPointUID.Uid);
					debugf(TEXT("--- AuthStatus: %i"), CachedAuthInt->ClientAuthSessions(i).AuthStatus);
					debugf(TEXT("--- AuthTicketUID: %d"), CachedAuthInt->ClientAuthSessions(i).AuthTicketUID);
				}
			}

			debugf(TEXT("LocalClientAuthSessions:"));

			for (INT i=0; i<CachedAuthInt->LocalClientAuthSessions.GetMaxIndex(); i++)
			{
				if (CachedAuthInt->LocalClientAuthSessions.IsAllocated(i))
				{
					debugf(TEXT("LocalClientAuthSessions(%i):"), i);

					FInternetIpAddr EndPointAddr;
					EndPointAddr.SetIp(CachedAuthInt->LocalClientAuthSessions(i).EndPointIP);

					debugf(TEXT("--- EndPointIP: %i (%s)"), CachedAuthInt->LocalClientAuthSessions(i).EndPointIP,
						*EndPointAddr.ToString(FALSE));

					debugf(TEXT("--- EndPointPort: %d"), CachedAuthInt->LocalClientAuthSessions(i).EndPointPort);
					debugf(TEXT("--- EndPointUID: ") I64_FORMAT_TAG, CachedAuthInt->LocalClientAuthSessions(i).EndPointUID.Uid);
					debugf(TEXT("--- SessionUID: %d"), CachedAuthInt->LocalClientAuthSessions(i).SessionUID);
				}
			}

			debugf(TEXT("ServerAuthSessions:"));

			for (INT i=0; i<CachedAuthInt->ServerAuthSessions.GetMaxIndex(); i++)
			{
				if (CachedAuthInt->ServerAuthSessions.IsAllocated(i))
				{
					debugf(TEXT("ServerAuthSessions(%i):"), i);

					FInternetIpAddr EndPointAddr;
					EndPointAddr.SetIp(CachedAuthInt->ServerAuthSessions(i).EndPointIP);

					debugf(TEXT("--- EndPointIP: %i (%s)"), CachedAuthInt->ServerAuthSessions(i).EndPointIP,
						*EndPointAddr.ToString(FALSE));

					debugf(TEXT("--- EndPointPort: %d"), CachedAuthInt->ServerAuthSessions(i).EndPointPort);
					debugf(TEXT("--- EndPointUID: ") I64_FORMAT_TAG, CachedAuthInt->ServerAuthSessions(i).EndPointUID.Uid);
					debugf(TEXT("--- AuthStatus: %i"), CachedAuthInt->ServerAuthSessions(i).AuthStatus);
					debugf(TEXT("--- AuthTicketUID: %d"), CachedAuthInt->ServerAuthSessions(i).AuthTicketUID);
				}
			}

			debugf(TEXT("LocalServerAuthSessions:"));

			for (INT i=0; i<CachedAuthInt->LocalServerAuthSessions.GetMaxIndex(); i++)
			{
				if (CachedAuthInt->LocalServerAuthSessions.IsAllocated(i))
				{
					debugf(TEXT("LocalServerAuthSessions(%i):"), i);

					FInternetIpAddr EndPointAddr;
					EndPointAddr.SetIp(CachedAuthInt->LocalServerAuthSessions(i).EndPointIP);

					debugf(TEXT("--- EndPointIP: %i (%s)"), CachedAuthInt->LocalServerAuthSessions(i).EndPointIP,
						*EndPointAddr.ToString(FALSE));

					debugf(TEXT("--- EndPointPort: %d"), CachedAuthInt->LocalServerAuthSessions(i).EndPointPort);
					debugf(TEXT("--- EndPointUID: ") I64_FORMAT_TAG, CachedAuthInt->LocalServerAuthSessions(i).EndPointUID.Uid);
					debugf(TEXT("--- SessionUID: %d"), CachedAuthInt->LocalServerAuthSessions(i).SessionUID);
				}
			}

			debugf(TEXT("PeerAuthSessions:"));

			for (INT i=0; i<CachedAuthInt->PeerAuthSessions.GetMaxIndex(); i++)
			{
				if (CachedAuthInt->PeerAuthSessions.IsAllocated(i))
				{
					debugf(TEXT("PeerAuthSessions(%i):"), i);

					FInternetIpAddr EndPointAddr;
					EndPointAddr.SetIp(CachedAuthInt->PeerAuthSessions(i).EndPointIP);

					debugf(TEXT("--- EndPointIP: %i (%s)"), CachedAuthInt->PeerAuthSessions(i).EndPointIP,
						*EndPointAddr.ToString(FALSE));

					debugf(TEXT("--- EndPointPort: %d"), CachedAuthInt->PeerAuthSessions(i).EndPointPort);
					debugf(TEXT("--- EndPointUID: ") I64_FORMAT_TAG, CachedAuthInt->PeerAuthSessions(i).EndPointUID.Uid);
					debugf(TEXT("--- AuthStatus: %i"), CachedAuthInt->PeerAuthSessions(i).AuthStatus);
					debugf(TEXT("--- AuthTicketUID: %d"), CachedAuthInt->PeerAuthSessions(i).AuthTicketUID);
				}
			}

			debugf(TEXT("LocalPeerAuthSessions:"));

			for (INT i=0; i<CachedAuthInt->LocalPeerAuthSessions.GetMaxIndex(); i++)
			{
				if (CachedAuthInt->LocalPeerAuthSessions.IsAllocated(i))
				{
					debugf(TEXT("LocalPeerAuthSessions(%i):"), i);

					FInternetIpAddr EndPointAddr;
					EndPointAddr.SetIp(CachedAuthInt->LocalPeerAuthSessions(i).EndPointIP);

					debugf(TEXT("--- EndPointIP: %i (%s)"), CachedAuthInt->LocalPeerAuthSessions(i).EndPointIP,
						*EndPointAddr.ToString(FALSE));


					debugf(TEXT("--- EndPointPort: %d"), CachedAuthInt->LocalPeerAuthSessions(i).EndPointPort);
					debugf(TEXT("--- EndPointUID: ") I64_FORMAT_TAG, CachedAuthInt->LocalPeerAuthSessions(i).EndPointUID.Uid);
					debugf(TEXT("--- SessionUID: %d"), CachedAuthInt->LocalPeerAuthSessions(i).SessionUID);
				}
			}
		}
		else
		{
			debugf(TEXT("CachedAuthInt == NULL"));
		}

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("DumpPendingStats")))
	{
		debugf(TEXT("PendingStats: (%i)"), OnlineSub->PendingStats.Num());

		for (INT i=0; i<OnlineSub->PendingStats.Num(); i++)
		{
			debugf(TEXT("---PendingStats(%i):"), i);

			debugf(TEXT("------ UID: ") I64_FORMAT_TAG, OnlineSub->PendingStats(i).Player.Uid);
			debugf(TEXT("------ PlayerName: %s"), *OnlineSub->PendingStats(i).PlayerName);
			debugf(TEXT("------ StatGuid: %s"), *OnlineSub->PendingStats(i).StatGuid);
			debugf(TEXT("------ Place: %s"), *OnlineSub->PendingStats(i).Place);
			debugf(TEXT("------ Score: (TeamId: %i, Score: %i)"), OnlineSub->PendingStats(i).Score.TeamID,
				OnlineSub->PendingStats(i).Score.Score);
			debugf(TEXT("------ Stats: (%i)"), OnlineSub->PendingStats(i).Stats.Num());

			for (INT j=0; j<OnlineSub->PendingStats(i).Stats.Num(); j++)
			{
				FString CurKey(OnlineSub->GetStatsFieldName(OnlineSub->PendingStats(i).Stats(j).ViewId,
									OnlineSub->PendingStats(i).Stats(j).ColumnId));

				if (OnlineSub->PendingStats(i).Stats(j).Data.Type == SDT_Int32)
				{
					INT CurValue = 0;
					OnlineSub->PendingStats(i).Stats(j).Data.GetData(CurValue);

					debugf(TEXT("--------- Stats(%i): ViewId: %i, ColumnId: %i, Key: %s, Value: %i "), j,
						OnlineSub->PendingStats(i).Stats(j).ViewId, OnlineSub->PendingStats(i).Stats(j).ColumnId,
						*CurKey, CurValue);
				}
				else if (OnlineSub->PendingStats(i).Stats(j).Data.Type == SDT_Float)
				{
					FLOAT CurValue = 0;
					OnlineSub->PendingStats(i).Stats(j).Data.GetData(CurValue);

					debugf(TEXT("--------- Stats(%i): ViewId: %i, ColumnId: %i, Key: %s, Value: %f "), j,
						OnlineSub->PendingStats(i).Stats(j).ViewId, OnlineSub->PendingStats(i).Stats(j).ColumnId,
						*CurKey, CurValue);
				}
				else
				{
					debugf(TEXT("--------- Stats(%i): ViewId: %i, ColumnId: %i, Key: %s, Value: Unsupported type '%i' "), j,
						OnlineSub->PendingStats(i).Stats(j).ViewId, OnlineSub->PendingStats(i).Stats(j).ColumnId,
						*CurKey, (INT)OnlineSub->PendingStats(i).Stats(j).Data.Type);
				}
			}
		}

		return TRUE;
	}
/*
	// Debug population of auth sessions arrays (added due to Steam being down for a time)
	else if (ParseCommand(&Str, TEXT("PopulateAuthSessions")))
	{
		UOnlineAuthInterfaceSteamworks* CachedAuthInt = OnlineSub->CachedAuthInt;

		if (CachedAuthInt != NULL)
		{
			if (CachedAuthInt->ClientAuthSessions.Num() == 0)
			{
				debugf(TEXT("Populating 'ClientAuthSessions'"));

				CachedAuthInt->ClientAuthSessions.AddZeroed(3);

				CachedAuthInt->ClientAuthSessions(1).EndPointUID.Uid = 1;
				CachedAuthInt->ClientAuthSessions(2).EndPointUID.Uid = 2;
			}
			else
			{
				debugf(TEXT("'ClientAuthSessions' already populated"));
			}

			if (CachedAuthInt->LocalClientAuthSessions.Num() == 0)
			{
				debugf(TEXT("Populating 'LocalClientAuthSessions'"));

				CachedAuthInt->LocalClientAuthSessions.AddZeroed(3);

				CachedAuthInt->LocalClientAuthSessions(1).EndPointUID.Uid = 1;
				CachedAuthInt->LocalClientAuthSessions(2).EndPointUID.Uid = 2;
			}
			else
			{
				debugf(TEXT("'LocalClientAuthSessions' already populated"));
			}

			if (CachedAuthInt->ServerAuthSessions.Num() == 0)
			{
				debugf(TEXT("Populating 'ServerAuthSessions'"));

				CachedAuthInt->ServerAuthSessions.AddZeroed(3);

				CachedAuthInt->ServerAuthSessions(1).EndPointUID.Uid = 1;
				CachedAuthInt->ServerAuthSessions(2).EndPointUID.Uid = 2;
			}
			else
			{
				debugf(TEXT("'ServerAuthSessions' already populated"));
			}

			if (CachedAuthInt->LocalServerAuthSessions.Num() == 0)
			{
				debugf(TEXT("Populating 'LocalServerAuthSessions'"));

				CachedAuthInt->LocalServerAuthSessions.AddZeroed(3);

				CachedAuthInt->LocalServerAuthSessions(1).EndPointUID.Uid = 1;
				CachedAuthInt->LocalServerAuthSessions(2).EndPointUID.Uid = 2;
			}
			else
			{
				debugf(TEXT("'LocalServerAuthSessions' already populated"));
			}
		}

		return TRUE;
	}
*/
	else if (ParseCommand(&Str, TEXT("DumpGameSettings")))
	{
		UOnlineGameSettings* CurGS = (OnlineSub->CachedGameInt != NULL ? OnlineSub->CachedGameInt->GameSettings : NULL);

		if (CurGS != NULL)
		{
			debugf(TEXT("Current GameSettings data:"));

			for (UProperty* P=CurGS->GetClass()->PropertyLink; P!=NULL; P=P->PropertyLinkNext)
			{
				if ((P->PropertyFlags & CPF_DataBinding) != 0 && Cast<UObjectProperty>(P, CLASS_IsAUObjectProperty) == NULL)
				{
					FString PropVal;
					P->ExportTextItem(PropVal, (BYTE*)CurGS + P->Offset, NULL, CurGS, P->PropertyFlags & PPF_Localized);

					debugf(TEXT("-- %s: %s"), *P->GetName(), *PropVal);
				}
			}
		}
		else
		{
			debugf(TEXT("No GameSettings set"));
		}

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("DumpSessionInfo")))
	{
		FSessionInfoSteam* CurSessionInfo =
			(FSessionInfoSteam*)(OnlineSub->CachedGameInt != NULL ? OnlineSub->CachedGameInt->SessionInfo : NULL);

		if (CurSessionInfo != NULL)
		{
			debugf(TEXT("SessionInfo: HostAddr: %s, ServerUID: ") I64_FORMAT_TAG TEXT(", bSteamSockets: %i"),
				*CurSessionInfo->HostAddr.ToString(TRUE), CurSessionInfo->ServerUID, (INT)CurSessionInfo->bSteamSockets);
		}
		else
		{
			debugf(TEXT("No SessionInfo set"));
		}

		return TRUE;
	}

	return FALSE;
}

#endif	// STEAM_EXEC_DEBUG


#endif	//#if WITH_UE3_NETWORKING

