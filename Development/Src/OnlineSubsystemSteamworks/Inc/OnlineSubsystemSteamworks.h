/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef INCLUDED_ONLINESUBSYSTEMSTEAMWORKS_H
#define INCLUDED_ONLINESUBSYSTEMSTEAMWORKS_H 1


/**
 * Defines
 */

// Non-UDK developers must set this value, to advertise on Valve's master server. Get a product name from Valve to set this up
//	(UDK developers can set this through DefaultEngineUDK.ini)
#if !UDK
	#define STEAM_PRODUCT_NAME TEXT("")
#endif


// Enables/disables the debug console command catcher
#define STEAM_EXEC_DEBUG !UDK && !SHIPPING_PC_GAME && !(FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE)

// When you launch the game, you don't receive your friends current rich presence data, until the next time they update a rich presence value;
//	this works around that (until Valve can fix it) by updating a rich presence value every 10 seconds
// @todo Steam: Remove this, if the alternate fix works
#define PRESENCE_FIX 0

// Enables/disables extra debug logging for tracking auth issues
// @todo Steam: Remove at some point, when auth is free of rare issues
#define AUTH_DEBUG_LOG 0

// Whether or not to use the old or new (P2P) Steam SDK auth functions
// @todo Steam: Remove when new Steam SDK auth does not break auth (currently bugs out for listen servers)
#define WITH_STEAMWORKS_P2PAUTH 1

// In the server browser, when querying server rules, logs what query field a filter failed on
#define FILTER_FAIL_LOG 0

// In the server browser, defines the maximum value of a ping response
#ifndef MAX_QUERY_MSEC
#define MAX_QUERY_MSEC 9999
#endif

// Enable this define to highlight important Steam log messages (added using Steamdebugf, primarily for debug-only purposes)
#define STEAMLOG_HIGHLIGHT 0 && !UDK && !SHIPPING_PC_GAME && !(FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE)

// Printf formatting for 64bit integers
#undef I64_FORMAT_TAG

#if _MSC_VER
#define I64_FORMAT_TAG TEXT("%I64u")
#else
#define I64_FORMAT_TAG TEXT("%llu")
#endif

/**
 * Base includes
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS

// @todo Steam: Steam headers trigger secure-C-runtime warnings in Visual C++. Rather than mess with _CRT_SECURE_NO_WARNINGS, we'll just
//	disable the warnings locally. Remove when this is fixed in the SDK
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif

// Steamworks SDK headers
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"

// @todo Steam: See above
#ifdef _MSC_VER
#pragma warning(pop)
#endif


/**
 * Forward declarations
 */

class UOnlineSubsystemSteamworks;
class FOnlineAsyncTaskManagerSteamBase;
class FOnlineAsyncTaskSteamServerListRequest;
class FOnlineAsyncTaskSteamServerRulesRequest;
class FOnlineAsyncTaskSteamServerPingRequest;


/**
 * Globals
 */

/** Manages threaded execution of SteamAPI tasks, and passing them back to the game thread */
extern FOnlineAsyncTaskManagerSteamBase*	GSteamAsyncTaskManager;


/** Whether or not the Steam client API is initialized */
extern UBOOL					GSteamworksClientInitialized;

/** Whether or not the Steam game server API is initialized */
extern UBOOL					GSteamworksGameServerInitialized;

/** Whether or not the Steam game server API is fully logged in and connected (with a valid Steam UID etc.) */
extern UBOOL					GSteamworksGameServerConnected;


/** If the game was launched from Steam, and Steam specified a +connect address on the commandline, this stores that address */
extern FInternetIpAddr				GSteamCmdLineConnect;

/** If the we are directly connecting to a server on launch, sometimes Steam specifies a password on the commandline as well; this stores that */
extern FString					GSteamCmdLinePassword;

/** Whether or not a Steam +connect commandline was set */
extern UBOOL					GSteamCmdLineSet;


// Cached interface pointers so we don't use the getters for each API call...
extern ISteamUtils*				GSteamUtils;
extern ISteamUser*				GSteamUser;
extern ISteamFriends*				GSteamFriends;
extern ISteamRemoteStorage*			GSteamRemoteStorage;
extern ISteamUserStats*				GSteamUserStats;
extern ISteamMatchmakingServers*		GSteamMatchmakingServers;
extern ISteamGameServer*			GSteamGameServer;
extern ISteamApps*				GSteamApps;
extern ISteamGameServerStats*			GSteamGameServerStats;
extern ISteamMatchmaking*			GSteamMatchmaking;
extern ISteamNetworking*			GSteamNetworking;
extern ISteamNetworking*			GSteamGameServerNetworking;
extern ISteamUtils*				GSteamGameServerUtils;

extern uint32					GSteamAppID;


/**
 * Type declarations
 */

/**
 * Game session info for Steam
 */
struct FSessionInfoSteam : public FSessionInfo
{
	/** The UID of the game server */
	QWORD ServerUID;

	/** Whether or not the server is using steam sockets */
	UBOOL bSteamSockets;


	/** Base constructor */
	FSessionInfoSteam()
		: ServerUID(0)
		, bSteamSockets(FALSE)
	{
	}
};

/** Struct for holding a search filter value (so it can fit into a TMultiMap) */
struct FSearchFilterValue
{
	FString	Value;
	BYTE	Operator;	// EOnlineGameSearchComparisonType

	FSearchFilterValue()
		: Value(TEXT(""))
		, Operator(0)
	{
	}

	FSearchFilterValue(FString& InValue, BYTE InOperator)
		: Value(InValue)
		, Operator(InOperator)
	{
	}
};

typedef TMap<FString, FString> SteamRulesMap;
typedef TMultiMap<FString, FSearchFilterValue> FilterMap;


/**
 * UnrealScript class includes
 */

#include "OnlineSubsystemSteamworksClasses.h"


/**
 * Utility functions
 */

/**
 * Whether or not the Steam Client interfaces are available; these interfaces are only available, if the Steam Client program is running
 * NOTE: These interfaces are made unavailable, when running a dedicated server
 *
 * @return	Whether or not the Steam Client interfaces are available
 */
inline UBOOL IsSteamClientAvailable()
{
	return GSteamworksClientInitialized;
}

/**
 * Whether or not the Steam game server interfaces are available; these interfaces are always available, so long as they were initialized correctly
 * NOTE: The Steam Client does not need to be running for the game server interfaces to initialize
 * NOTE: These interfaces are made unavailable, when not running a server
 *
 * @return	Whether or not the Steam game server interfaces are available
 */
FORCEINLINE UBOOL IsSteamServerAvailable()
{
	// @todo Steam - add some logic to detect somehow we intended to be a "Steam client" but failed that part
	// yet managed to still initialize the game server aspects of Steam
	return GSteamworksGameServerInitialized;
}


/** Opportunity for Steam to manipulate cmd line arguments before the engine */
void appSteamHandleCmdLine(const TCHAR** CmdLine);

/** 
 * Check to see if we can or have enabled Steam
 * @return TRUE if Steam is currently enabled or able to be enabled, FALSE otherwise
 */
UBOOL appIsSteamEnabled();

/** 
 *  Initialize the steam hooks into the engine, possibly relaunching if required by the Steam service
 */
void appSteamInit();

/** 
 * Cleanup the Steam subsystem
 */
void appSteamShutdown();


// Special log messages for steam, which highlight the log message in non-release builds (but which do not do this in release)
VARARG_DECL(void, static void, {}, Steamdebugf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE);

FORCEINLINE UBOOL IsServer()
{
	AWorldInfo* WorldInfo = GWorld != NULL ? GWorld->GetWorldInfo() : NULL;

	if (WorldInfo != NULL)
	{
		return (WorldInfo->NetMode == NM_ListenServer || WorldInfo->NetMode == NM_DedicatedServer);
	}

	return FALSE;
}

// In most cases, this should not be used; this is only valid for cases where you may access the pending levels net driver as well
FORCEINLINE UNetDriver* GetActiveNetDriver()
{
	UNetDriver* NetDriver = GWorld != NULL ? GWorld->GetNetDriver() : NULL;

	if (NetDriver == NULL && Cast<UGameEngine>(GEngine) != NULL && ((UGameEngine*)GEngine)->GPendingLevel != NULL)
	{
		NetDriver = ((UGameEngine*)GEngine)->GPendingLevel->NetDriver;
	}

	return NetDriver;
}

/**
 * Searches the WorldInfo's PRI array for the specified player so that the
 * player's APlayerController can be found
 *
 * @param Player the player that is being searched for
 *
 * @return the controller for the player, NULL if not found
 */
inline APlayerController* FindPlayerControllerForUniqueId(FUniqueNetId Player)
{
	AWorldInfo* WorldInfo = GWorld != NULL ? GWorld->GetWorldInfo() : NULL;

	if (WorldInfo != NULL)
	{
		AGameReplicationInfo* GRI = WorldInfo->GRI;

		if (GRI != NULL)
		{
			// Find the PRI that matches the net id
			for (INT Index = 0; Index<GRI->PRIArray.Num(); Index++)
			{
				APlayerReplicationInfo* PRI = GRI->PRIArray(Index);

				// If this PRI matches, get the owning actor
				if (PRI != NULL && PRI->UniqueId == Player)
				{
					return Cast<APlayerController>(PRI->Owner);
				}
			}
		}
	}

	return NULL;
}

/**
 * Takes a Steam EResult value, and converts it into a string (with extra debug info)
 *
 * @param Result	The EResult value to convert to a string
 * @return		Returns the converted string
 */
inline FString SteamResultString(EResult Result)
{
	FString ReturnVal;

	#define SteamResultCase(Value, Desc) \
		case Value: ReturnVal = FString::Printf(TEXT("'%i' %s (%s)"), (INT)Value, TEXT(#Value), Desc); break;

	switch (Result)
	{
	SteamResultCase(k_EResultOK,						TEXT("success"));
	SteamResultCase(k_EResultFail,						TEXT("failure"));
	SteamResultCase(k_EResultNoConnection,					TEXT("no connection"));
	SteamResultCase(k_EResultInvalidPassword,				TEXT("invalid password/ticket"));
	SteamResultCase(k_EResultLoggedInElsewhere,				TEXT("same user logged in elsewhere"));
	SteamResultCase(k_EResultInvalidProtocolVer,				TEXT("incorrect protocol version"));
	SteamResultCase(k_EResultInvalidParam,					TEXT("a parameter is incorrect"));
	SteamResultCase(k_EResultFileNotFound,					TEXT("file not found"));
	SteamResultCase(k_EResultBusy,						TEXT("called method busy, no action taken"));
	SteamResultCase(k_EResultInvalidState,					TEXT("called object in invalid state"));
	SteamResultCase(k_EResultInvalidName,					TEXT("invalid name"));
	SteamResultCase(k_EResultInvalidEmail,					TEXT("invalid email"));
	SteamResultCase(k_EResultDuplicateName,					TEXT("duplicate name"));
	SteamResultCase(k_EResultAccessDenied,					TEXT("access denied"));
	SteamResultCase(k_EResultTimeout,					TEXT("operation timed out"));
	SteamResultCase(k_EResultBanned,					TEXT("VAC banned"));
	SteamResultCase(k_EResultAccountNotFound,				TEXT("account not found"));
	SteamResultCase(k_EResultInvalidSteamID,				TEXT("steamid invalid"));
	SteamResultCase(k_EResultServiceUnavailable,				TEXT("requested service currently unavailable"));
	SteamResultCase(k_EResultNotLoggedOn,					TEXT("user is not logged on"));
	SteamResultCase(k_EResultPending,					TEXT("request is pending - may be in process, or waiting on third party"));
	SteamResultCase(k_EResultEncryptionFailure,				TEXT("encryption or decryption failed"));
	SteamResultCase(k_EResultInsufficientPrivilege,				TEXT("insufficient privilege"));
	SteamResultCase(k_EResultLimitExceeded,					TEXT("limit exceeded"));
	SteamResultCase(k_EResultRevoked,					TEXT("access revoked"));
	SteamResultCase(k_EResultExpired,					TEXT("license or guest pass expired"));
	SteamResultCase(k_EResultAlreadyRedeemed,				TEXT("guest pass already redeemed"));
	SteamResultCase(k_EResultDuplicateRequest,				TEXT("duplicate request, already occurred, ignoring"));
	SteamResultCase(k_EResultAlreadyOwned,					TEXT("already owned"));
	SteamResultCase(k_EResultIPNotFound,					TEXT("IP address not found"));
	SteamResultCase(k_EResultPersistFailed,					TEXT("failed to write change to data store"));
	SteamResultCase(k_EResultLockingFailed,					TEXT("failed to acquire access lock for operation"));
	SteamResultCase(k_EResultLogonSessionReplaced,				TEXT("???"));
	SteamResultCase(k_EResultConnectFailed,					TEXT("???"));
	SteamResultCase(k_EResultHandshakeFailed,				TEXT("???"));
	SteamResultCase(k_EResultIOFailure,					TEXT("input/output failure"));
	SteamResultCase(k_EResultRemoteDisconnect,				TEXT("???"));
	SteamResultCase(k_EResultShoppingCartNotFound,				TEXT("failed to find shopping cart requested"));
	SteamResultCase(k_EResultBlocked,					TEXT("blocked"));
	SteamResultCase(k_EResultIgnored,					TEXT("ignored"));
	SteamResultCase(k_EResultNoMatch,					TEXT("nothing matching request found"));
	SteamResultCase(k_EResultAccountDisabled,				TEXT("???"));
	SteamResultCase(k_EResultServiceReadOnly,				TEXT("service not accepting content changes right now"));
	SteamResultCase(k_EResultAccountNotFeatured,				TEXT("???"));
	SteamResultCase(k_EResultAdministratorOK,				TEXT("allowed to take this action, but only because requester is admin"));
	SteamResultCase(k_EResultContentVersion,				TEXT("version mismatch in transmitted content"));
	SteamResultCase(k_EResultTryAnotherCM,					TEXT("???"));
	SteamResultCase(k_EResultPasswordRequiredToKickSession,			TEXT("???"));
	SteamResultCase(k_EResultAlreadyLoggedInElsewhere,			TEXT("already logged in elsewhere, must wait"));
	SteamResultCase(k_EResultSuspended,					TEXT("operation suspended/paused"));
	SteamResultCase(k_EResultCancelled,					TEXT("operation cancelled"));
	SteamResultCase(k_EResultDataCorruption,				TEXT("operation cancelled due to corrupt data"));
	SteamResultCase(k_EResultDiskFull,					TEXT("operation cancelled due to lack of disk space"));
	SteamResultCase(k_EResultRemoteCallFailed,				TEXT("remote call or IPC call failed"));
	SteamResultCase(k_EResultPasswordUnset,					TEXT("password not verified, as it's unset serverside"));
	SteamResultCase(k_EResultExternalAccountUnlinked,			TEXT("external account not linked to a steam account"));
	SteamResultCase(k_EResultPSNTicketInvalid,				TEXT("PSN ticket invalid"));
	SteamResultCase(k_EResultExternalAccountAlreadyLinked,			TEXT("external account linked to other account"));
	SteamResultCase(k_EResultRemoteFileConflict,				TEXT("sync cannot resume, conflict between local and remote files"));
	SteamResultCase(k_EResultIllegalPassword,				TEXT("requested password not legal"));
	SteamResultCase(k_EResultSameAsPreviousValue,				TEXT("new value same as old"));
	SteamResultCase(k_EResultAccountLogonDenied,				TEXT("account login denied due to 2nd factor auth failure"));
	SteamResultCase(k_EResultCannotUseOldPassword,				TEXT("requested password not legal"));
	SteamResultCase(k_EResultInvalidLoginAuthCode,				TEXT("account login denied, invalid auth code"));
	SteamResultCase(k_EResultAccountLogonDeniedNoMail,			TEXT("account login denied due to 2nd factor auth failure"));
	SteamResultCase(k_EResultHardwareNotCapableOfIPT,			TEXT("???"));
	SteamResultCase(k_EResultIPTInitError,					TEXT("???"));
	SteamResultCase(k_EResultParentalControlRestricted,			TEXT("operation failed due to parental controls"));
	SteamResultCase(k_EResultFacebookQueryError,				TEXT("facebook query returned error"));
	SteamResultCase(k_EResultExpiredLoginAuthCode,				TEXT("account login denied, expired auth code"));
	SteamResultCase(k_EResultIPLoginRestrictionFailed,			TEXT("???"));
	SteamResultCase(k_EResultAccountLockedDown,				TEXT("???"));
	SteamResultCase(k_EResultAccountLogonDeniedVerifiedEmailRequired,	TEXT("???"));
	SteamResultCase(k_EResultNoMatchingURL,					TEXT("no matching URL"));

	default:
		ReturnVal = FString::Printf(TEXT("Unknown result: %i (check Steam SDK)"), (INT)Result);
		break;
	}

	#undef SteamResultCase

	return ReturnVal;
}

/**
 * General OnlineSubsystemSteamworks includes
 */

#if WITH_STEAMWORKS_SOCKETS
#include "UnSocketSteamworks.h"
#include "UnNetSteamworks.h"
#endif

// Includes code which links Steam API callbacks to the Online Subsystem and it's interfaces
#include "OnlineAsyncTaskManagerSteam.h"


/**
 * OnlineLobbyInterfaceSteamworks types/defines
 */

#if STEAM_MATCHMAKING_LOBBY

// Special set of archives for safely reading/writing lobby chat messages

// NOTE: The lobby chat functions can go up to 4k, but the largest string size is about 2048
#define MAX_LOBBY_CHAT_LENGTH Min(MAX_STRING_SERIALIZE_SIZE, 3072)

// The first byte of lobby chat data designates its type
#define LOBBY_CHAT_TYPE_STRING (BYTE)0
#define LOBBY_CHAT_TYPE_BINARY (BYTE)1

class FLobbyChatReader : public FMemoryReader
{
public:
	FLobbyChatReader(TArray<BYTE>& InBytes)
		: FMemoryReader(InBytes, TRUE)
	{
		// Marks an error if the serialized string exceeds the maximum allowed size
		ArMaxSerializeSize = MAX_LOBBY_CHAT_LENGTH;
	}
};

class FLobbyChatWriter : public FMemoryWriter
{
public:
	FLobbyChatWriter(TArray<BYTE>& InBytes)
		: FMemoryWriter(InBytes, TRUE)
	{
		// Marks an error if the serialized string exceeds the maximum allowed size
		ArMaxSerializeSize = MAX_LOBBY_CHAT_LENGTH;
	}
};

#endif	// STEAM_MATCHMAKING_LOBBY


/**
 * Steam debug exec handler
 */

#if STEAM_EXEC_DEBUG

class FSteamExecCatcher : public FSelfRegisteringExec
{
private:
	UOnlineSubsystemSteamworks* OnlineSub;

public:
	FSteamExecCatcher(UOnlineSubsystemSteamworks* InOnlineSub)
		: OnlineSub(InOnlineSub)
	{
	}

	virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
};

#endif	// STEAM_EXEC_DEBUG


#endif	// WITH_UE3_NETWORKING
#endif  // !INCLUDED_ONLINESUBSYSTEMSTEAMWORKS_H

