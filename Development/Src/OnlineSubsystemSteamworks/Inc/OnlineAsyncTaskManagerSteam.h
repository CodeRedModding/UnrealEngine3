/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef INCLUDED_ONLINEASYNCTASKMANAGERSTEAM_H
#define INCLUDED_ONLINEASYNCTASKMANAGERSTEAM_H 1

#if WITH_UE3_NETWORKING && WITH_STEAMWORKS


// Includes

#include "OnlineAsyncTaskManager.h"


// Class defines


/**
 * Base class of the primary Steam task manager further below; defined here to allow tasks to access certain members,
 * and for the primary Steam task manager to implement the actual tasks (circular dependancies otherwise)
 */
class FOnlineAsyncTaskManagerSteamBase : public FOnlineAsyncTaskManager
{
protected:
	/** Cached reference to the main online subsystem */
	UOnlineSubsystemSteamworks*		SteamSubsystem;

	/** Cached reference to the online subsystem game interface */
	UOnlineGameInterfaceSteamworks*		SteamGameInterface;

	/** Cached reference to the online subsystem auth interface */
	UOnlineAuthInterfaceSteamworks*		SteamAuthInterface;

	/** Cached reference to the steam sockets manager */
	FSteamSocketsManager*			SteamSocketsManager;

#if STEAM_MATCHMAKING_LOBBY
	/** Cached reference to the online subsystem lobby interface */
	UOnlineLobbyInterfaceSteamworks*	SteamLobbyInterface;
#endif


public:
	/**
	 * Base constructor
	 */
	FOnlineAsyncTaskManagerSteamBase()
		: SteamSubsystem(NULL)
		, SteamGameInterface(NULL)
		, SteamAuthInterface(NULL)
		, SteamSocketsManager(NULL)
#if STEAM_MATCHMAKING_LOBBY
		, SteamLobbyInterface(NULL)
#endif
	{
	}

	/**
	 * Base destructor
	 */
	~FOnlineAsyncTaskManagerSteamBase()
	{
	}

	/**
	 * Returns a reference to the interface matching the class specified by the input template parameter
	 *
	 * @return	A reference to the interface specified by the input class
	 */
	template<typename Type> Type* GetCallbackInterface()
	{
	}


	/**
	 * Registers the steam subsystem interface for receiving callbacks
	 *
	 * @param InSubsystem	The online subsystem to link with the task manager
	 */
	void RegisterInterface(UOnlineSubsystemSteamworks* InSubsystem)
	{
		SteamSubsystem = InSubsystem;
	}

	/**
	 * Whether or not the steam subsystem interface is registered
	 *
	 * @param InSubsystem	This is ignored; just used to make the correct function call at compile time
	 * @return		Whether or not the interface is registered
	 */
	FORCEINLINE UBOOL IsRegisteredInterface(UOnlineSubsystemSteamworks* InSubsystem)
	{
		return SteamSubsystem != NULL;
	}

	/**
	 * Returns a reference to the interface matching the class specified by the input template parameter
	 *
	 * @return	A reference to the interface specified by the input class
	 */
	template<> FORCEINLINE UOnlineSubsystemSteamworks* GetCallbackInterface<UOnlineSubsystemSteamworks>()
	{
		return SteamSubsystem;
	}

	/**
	 * Unregisters the steam subsystem from the task manager
	 *
	 * @param InSubsystem	The online subsystem to unlink from the task manager
	 */
	void UnregisterInterface(UOnlineSubsystemSteamworks* InSubsystem)
	{
		SteamSubsystem = NULL;
	}

	/**
	 * Registers the steam game interface for receiving callbacks
	 *
	 * @param InGameInterface	The online game interface to link with the task manager
	 */
	void RegisterInterface(UOnlineGameInterfaceSteamworks* InGameInterface)
	{
		SteamGameInterface = InGameInterface;
	}

	/**
	 * Whether or not the steam game interface is registered
	 *
	 * @param InGameInterface	This is ignored; just used to make the correct function call at compile time
	 * @return			Whether or not the interface is registered
	 */
	FORCEINLINE UBOOL IsRegisteredInterface(UOnlineGameInterfaceSteamworks* InGameInterface)
	{
		return SteamGameInterface != NULL;
	}

	/**
	 * Returns a reference to the interface matching the class specified by the input template parameter
	 *
	 * @return	A reference to the interface specified by the input class
	 */
	template<> FORCEINLINE UOnlineGameInterfaceSteamworks* GetCallbackInterface<UOnlineGameInterfaceSteamworks>()
	{
		return SteamGameInterface;
	}

	/**
	 * Unregisters the steam game interface from the task manager
	 *
	 * @param InGameInterface	The online game interface to unlink from the task manager
	 */
	void UnregisterInterface(UOnlineGameInterfaceSteamworks* InGameInterface)
	{
		SteamGameInterface = NULL;
	}

	/**
	 * Registers the steam auth interface for receiving callbacks
	 *
	 * @param InAuthInterface	The online auth interface to link with the task manager
	 */
	void RegisterInterface(UOnlineAuthInterfaceSteamworks* InAuthInterface)
	{
		SteamAuthInterface = InAuthInterface;
	}

	/**
	 * Whether or not the steam auth interface is registered
	 *
	 * @param InAuthInterface	This is ignored; just used to make the correct function call at compile time
	 * @return			Whether or not the interface is registered
	 */
	FORCEINLINE UBOOL IsRegisteredInterface(UOnlineAuthInterfaceSteamworks* InAuthInterface)
	{
		return SteamAuthInterface != NULL;
	}

	/**
	 * Returns a reference to the interface matching the class specified by the input template parameter
	 *
	 * @return	A reference to the interface specified by the input class
	 */
	template<> FORCEINLINE UOnlineAuthInterfaceSteamworks* GetCallbackInterface<UOnlineAuthInterfaceSteamworks>()
	{
		return SteamAuthInterface;
	}

	/**
	 * Unregisters the steam auth interface from the task manager
	 *
	 * @param InAuthInterface	The online auth interface to unlink from the task manager
	 */
	void UnregisterInterface(UOnlineAuthInterfaceSteamworks* InAuthInterface)
	{
		SteamAuthInterface = NULL;
	}

#if STEAM_MATCHMAKING_LOBBY
	/**
	 * Registers the steam lobby interface for receiving callbacks
	 *
	 * @param InLobbyInterface	The online lobby interface to link with the task manager
	 */
	void RegisterInterface(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
	{
		SteamLobbyInterface = InLobbyInterface;
	}

	/**
	 * Whether or not the steam lobby interface is registered
	 *
	 * @param InLobbyInterface	This is ignored; just used to make the correct function call at compile time
	 * @return			Whether or not the interface is registered
	 */
	FORCEINLINE UBOOL IsRegisteredInterface(UOnlineLobbyInterfaceSteamworks* InAuthInterface)
	{
		return SteamLobbyInterface != NULL;
	}

	/**
	 * Returns a reference to the interface matching the class specified by the input template parameter
	 *
	 * @return	A reference to the interface specified by the input class
	 */
	template<> FORCEINLINE UOnlineLobbyInterfaceSteamworks* GetCallbackInterface<UOnlineLobbyInterfaceSteamworks>()
	{
		return SteamLobbyInterface;
	}

	/**
	 * Unregisters the steam lobby interface from the task manager
	 *
	 * @param InLobbyInterface	The online lobby interface to unlink from the task manager
	 */
	void UnregisterInterface(UOnlineLobbyInterfaceSteamworks* InLobbyInterface)
	{
		SteamLobbyInterface = NULL;
	}
#endif

	/**
	 * Registers the steam sockets manager interface for receiving callbacks
	 *
	 * @param InSocketsManager	The sockets manager to link with the task manager
	 */
	void RegisterInterface(FSteamSocketsManager* InSocketsManager)
	{
		SteamSocketsManager = InSocketsManager;
	}

	/**
	 * Whether or not the steam sockets manager interface is registered
	 *
	 * @param InSocketsManager	This is ignored; just used to make the correct function call at compile time
	 * @return			Whether or not the interface is registered
	 */
	FORCEINLINE UBOOL IsRegisteredInterface(FSteamSocketsManager* InSocketsManager)
	{
		return SteamSocketsManager != NULL;
	}

	/**
	 * Returns a reference to the interface matching the class specified by the input template parameter
	 *
	 * @return	A reference to the interface specified by the input class
	 */
	template<> FORCEINLINE FSteamSocketsManager* GetCallbackInterface<FSteamSocketsManager>()
	{
		return SteamSocketsManager;
	}

	/**
	 * Unregisters the steam sockets manager interface from the task manager
	 *
	 * @param InLobbyInterface	The sockets manager to unlink from the task manager
	 */
	void UnregisterInterface(FSteamSocketsManager* InSocketsManager)
	{
		SteamSocketsManager = NULL;
	}
};



/**
 * Base class that holds a delegate to fire when a given async event is triggered by the platform service
 *
 * @param InSteamDataStruct	The Steam API data struct which defines the callback data
 * @param InInterfaceClass	The class of the CallbackInterface, i.e. the UScript/native class which receives callbacks
 */
template<class InSteamDataStruct, class InInterfaceClass>
class FOnlineAsyncEventSteam : public FOnlineAsyncEvent
{
public:
	/** The Steam API data struct which defines the callback data */
	typedef InSteamDataStruct SteamDataStruct;

	/** The UScript/native class which receives callbacks */
	typedef InInterfaceClass InterfaceClass;

protected:
	/** Reference to the object which callbacks are passed to */
	InterfaceClass*	CallbackInterface;

	/** Hidden constructor */
	FOnlineAsyncEventSteam()
		: CallbackInterface(NULL)
	{
	}


public:
	/**
	 * Main constructor
	 *
	 * @param InCallbackInterface	The object this event is linked to
	 */
	FOnlineAsyncEventSteam(InterfaceClass* InCallbackInterface)
		: CallbackInterface(InCallbackInterface)
	{
	}


	/**
	 * Whether or not the task manager should execute this item when it returns to the game thread
	 * Used as a hook-in to check that the object the item is sending results to, is still valid
	 *
	 * @return	Whether or not the execute Finalize and TriggerDelegates
	 */
	virtual UBOOL CanExecute()
	{
		return GSteamAsyncTaskManager->IsRegisteredInterface(CallbackInterface);
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData	The struct containing the SteamAPI callback data
	 * @return		TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(SteamDataStruct* CallbackData)=0;
};

/**
 * Base class that holds a delegate to fire when a given async task is complete
 *
 * @param InInterfaceClass	The class of the CallbackInterface, i.e. the UScript/native class which receives callbacks
 */
template<class InInterfaceClass>
class FOnlineAsyncTaskSteamBase : public FOnlineAsyncTaskBase
{
public:
	/** The UScript/native class which receives callbacks */
	typedef InInterfaceClass InterfaceClass;

protected:

	/** Reference to the object which callbacks are passed to */
	InterfaceClass*			CallbackInterface;

	/** Has the task returned from Steam */
	UBOOL				bIsComplete;

	/** Did the task complete successfully */
	UBOOL				bWasSuccessful;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamBase()
		: CallbackInterface(NULL)
		, bIsComplete(FALSE)
		, bWasSuccessful(FALSE)
	{
	}


public:
	/**
	 * Main constructor
	 *
	 * @param InCallbackInterface	The object this event is linked to
	 */
	FOnlineAsyncTaskSteamBase(InterfaceClass* InCallbackInterface)
		: CallbackInterface(InCallbackInterface)
		, bIsComplete(FALSE)
		, bWasSuccessful(FALSE)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamBase()
	{
	}


	/**
	 * Whether or not the task manager should execute this item when it returns to the game thread
	 * Used as a hook-in to check that the object the item is sending results to, is still valid
	 *
	 * @return	Whether or not the execute Finalize and TriggerDelegates
	 */
	virtual UBOOL CanExecute()
	{
		return GSteamAsyncTaskManager->IsRegisteredInterface(CallbackInterface);
	}

	/**
	 * Check the state of the async task
	 *
	 * @return	TRUE if complete, FALSE otherwise
	 */
	virtual UBOOL IsDone()
	{
		return bIsComplete;
	}

	/**
	 * Check the success of the async task
	 *
	 * @return	TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL WasSuccessful()
	{
		return bWasSuccessful;
	}
};

/**
 * Base class that holds a delegate to fire when a given async task is complete
 *
 * @param InSteamDataStruct	The Steam API data struct which defines the callback data
 * @param InInterfaceClass	The class of the CallbackInterface, i.e. the UScript/native class which receives callbacks
 * @param bGameServerInterface	Whether or not this task is for the Steam game server interface
 */
template<class InSteamDataStruct, class InInterfaceClass, UBOOL bGameServerInterface=FALSE>
class FOnlineAsyncTaskSteam : public FOnlineAsyncTaskSteamBase<InInterfaceClass>
{
public:
	/** The Steam API data struct which defines the callback data */
	typedef InSteamDataStruct SteamDataStruct;

protected:
	/** Unique handle for the Steam async call initiated */
	SteamAPICall_t			CallbackHandle;

	/** Was there an internal SteamAPI error while processing the task? */
	UBOOL				bIOFailure;


	/** Hidden constructor */
	FOnlineAsyncTaskSteam()
		: FOnlineAsyncTaskSteamBase()
		, CallbackHandle(k_uAPICallInvalid)
		, bIOFailure(FALSE)
	{
	}


public:
	/**
	 * Main constructor
	 *
	 * @param InCallbackInterface	The object this event is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteam(InterfaceClass* InCallbackInterface, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteamBase(InCallbackInterface)
		, CallbackHandle(InCallbackHandle)
		, bIOFailure(FALSE)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteam()
	{
	}


	/**
	 * Determines if a result is pending for the specified callback, and if so, attempts to grab the result
	 * NOTE: Online thread only
	 *
	 * @param bIOFailure	Outputs whether or not there was an IO error retrieving the result
	 * @return		Whether or not the callback has completed
	 */
	FORCEINLINE UBOOL GetSteamAPIResult(SteamDataStruct& OutResult, UBOOL& OutbIOFailure)
	{
		bool bFailed = FALSE;
		UBOOL bCallComplete = bGameServerInterface ?
					(GSteamGameServerUtils != NULL && GSteamGameServerUtils->IsAPICallCompleted(CallbackHandle, &bFailed)) :
					(GSteamUtils != NULL && GSteamUtils->IsAPICallCompleted(CallbackHandle, &bFailed));

		if (bCallComplete)
		{
			UBOOL bSuccess = FALSE;

			if (bGameServerInterface)
			{
				bSuccess = GSteamGameServerUtils->GetAPICallResult(CallbackHandle, &OutResult, sizeof(SteamDataStruct),
											OutResult.k_iCallback, &bFailed);
			}
			else
			{
				bSuccess = GSteamUtils->GetAPICallResult(CallbackHandle, &OutResult, sizeof(SteamDataStruct),
										OutResult.k_iCallback, &bFailed);
			}

			OutbIOFailure = !bSuccess || bFailed;
		}

		return bCallComplete;
	}

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick()
	{
		FOnlineAsyncTaskSteamBase::Tick();

		SteamDataStruct ResultData;

		// See if the callback returned
		if (GetSteamAPIResult(ResultData, bIOFailure))
		{
			bIsComplete = TRUE;

			// NOTE: This is only the default value of bWasSuccessful; it should be refined further within ProcessSteamCallback
			bWasSuccessful = !bIOFailure;

			ProcessSteamCallback(&ResultData, bIOFailure);
		}
	}


	/**
	 * Handles events from Steam and formats it to fit the async event;
	 * if this returns false, the async event is immediately deleted and does not pass to the online subsystem
	 * NOTE: Online thread only
	 *
	 * @param CallbackData		The struct containing the SteamAPI callback data
	 * @param bInIOError		Whether or not there was a SteamAPI IOError when processing the callback
	 * @return			TRUE if the callback was processed successfully and should be passed to the online subsystem, FALSE otherwise
	 */
	virtual UBOOL ProcessSteamCallback(SteamDataStruct* CallbackData, UBOOL bInIOFailure)=0;
};

/**
 * Base class that holds a delegate to fire when a given async task is complete, for the game server interface
 *
 * @param InSteamDataStruct	The Steam API data struct which defines the callback data
 * @param InInterfaceClass	The class of the CallbackInterface, i.e. the UScript/native class which receives callbacks
 */
template<class InSteamDataStruct, class InInterfaceClass>
class FOnlineAsyncTaskSteamGameServer : public FOnlineAsyncTaskSteam<InSteamDataStruct, InInterfaceClass, TRUE>
{
protected:
	/** Hidden constructor */
	FOnlineAsyncTaskSteamGameServer()
		: FOnlineAsyncTaskSteam()
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InCallbackInterface	The object this event is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamGameServer(InterfaceClass* InCallbackInterface, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteam(InCallbackInterface, InCallbackHandle)
	{
	}
};



// Async task manager defines

/**
 * Declares a static Steam callback (specified by the input struct), and links it to an FOnlineAsyncEventSteam subclass
 * NOTE: The class used for 'AsyncEvent' doesn't need to be defined in order to use it here, only for the IMPLEMENT macro
 * NOTE: This must go within FOnlineAsyncTaskManagerSteam
 *
 * @param SteamDataStruct	The Steam SDK data struct that represents the callback and the data it returns
 * @param AsyncEvent		The FOnlineAsyncEventSteam subclass that will handle this callback
 */
#define DECLARE_STATIC_STEAM_CALLBACK(SteamDataStruct, AsyncEvent) \
	STEAM_CALLBACK(FOnlineAsyncTaskManagerSteam, AsyncEvent##Func, SteamDataStruct, AsyncEvent##Callback);

/**
 * Declares a static Steam callback (specified by the input struct) that uses the Steam gameserver interface,
 * and links it to an FOnlineAsyncEventSteam subclass
 * NOTE: The class used for 'AsyncEvent' doesn't need to be defined in order to use it here, only for the IMPLEMENT macro
 * NOTE: This must go within FOnlineAsyncTaskManagerSteam
 *
 * @param SteamDataStruct	The Steam SDK data struct that represents the callback and the data it returns
 * @param AsyncEvent		The FOnlineAsyncEventSteam subclass that will handle this callback
 */
#define DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(SteamDataStruct, AsyncEvent) \
	STEAM_GAMESERVER_CALLBACK(FOnlineAsyncTaskManagerSteam, AsyncEvent##Func, SteamDataStruct, AsyncEvent##Callback);

/**
 * Links a static Steam callback (specified by the input struct), to the specified FOnlineAsyncEventSteam subclass
 *
 * @param AsyncEvent		The FOnlineAsyncEventSteam subclass that will handle this callback
 */
#define IMPLEMENT_STATIC_STEAM_CALLBACK(AsyncEvent) \
	void FOnlineAsyncTaskManagerSteam::##AsyncEvent##Func(AsyncEvent##::SteamDataStruct* pParam) \
	{ \
		AsyncEvent##::InterfaceClass* CallbackInterface = GetCallbackInterface<AsyncEvent##::InterfaceClass>(); \
		\
		if (CallbackInterface != NULL) \
		{ \
			AsyncEvent* NewEvent = new AsyncEvent(CallbackInterface); \
			\
			if (NewEvent->ProcessSteamCallback(pParam)) \
			{ \
				GSteamAsyncTaskManager->AddToOutQueue(NewEvent); \
			} \
			else \
			{ \
				delete NewEvent; \
			} \
		} \
		else \
		{ \
			debugf(NAME_DevOnline, TEXT("FOnlineAsyncTaskManagerSteam: Got callback '%s' but CallbackInterface is NULL"), \
				TEXT(#AsyncEvent)); \
		} \
	}

/**
 * Constructs the member variables for the specified static Steam callback (one for every FOnlineAsyncEventSteam subclass)
 * NOTE: The class used for 'AsyncEvent' doesn't need to be defined in order to use it here, only for the IMPLEMENT macro
 * NOTE: This must be put in the FOnlineAsyncTaskManagerSteam instantiation list
 *
 * @param CallbackInterface	The object this event is linked to
 * @param AsyncEvent		The FOnlineAsyncEventSteam subclass that is linked to this callback
 */
#define CONSTRUCT_STATIC_STEAM_CALLBACK(AsyncEvent) \
	AsyncEvent##Callback(this, &FOnlineAsyncTaskManagerSteam::##AsyncEvent##Func)


/**
 * Asynchronous task manager for linking the SteamSDK/API to UE3, through a separate thread; handles API calls, the online thread,
 * and passing API calls to the game thread within the relevant tasks
 */
class FOnlineAsyncTaskManagerSteam : public FOnlineAsyncTaskManagerSteamBase
{
private:
	/** Delegate registered with Steam to trigger when the currently logged in user receives their stats from RequestStats() */
	DECLARE_STATIC_STEAM_CALLBACK(UserStatsReceived_t, FOnlineAsyncEventSteamStatsReceived);

	/** Delegate registered with Steam to trigger when the currently logged in user has their stats stored */
	DECLARE_STATIC_STEAM_CALLBACK(UserStatsStored_t, FOnlineAsyncEventSteamStatsStored);

	/** Delegate registered with Steam to trigger when the Steam Overlay is activated */
	DECLARE_STATIC_STEAM_CALLBACK(GameOverlayActivated_t, FOnlineAsyncEventSteamExternalUITriggered);

	/** Delegate registered with Steam to trigger when Steam wants to shutdown */
	DECLARE_STATIC_STEAM_CALLBACK(SteamShutdown_t, FOnlineAsyncEventSteamShutdown);

	/** Delegate registered with Steam to trigger when the player tries to join a game through the Steam client */
	DECLARE_STATIC_STEAM_CALLBACK(GameServerChangeRequested_t, FOnlineAsyncEventSteamServerChangeRequest);

	/** Delegate registered with Steam to trigger when the player tries to join a friends game, with associated rich presence info */
	DECLARE_STATIC_STEAM_CALLBACK(GameRichPresenceJoinRequested_t, FOnlineAsyncEventSteamRichPresenceJoinRequest);

	/** Delegate registered with Steam to trigger when the client connects to the Steam servers */
	DECLARE_STATIC_STEAM_CALLBACK(SteamServersConnected_t, FOnlineAsyncEventSteamServersConnected);

	/** Delegate registered with Steam to trigger when the client disconnects from the Steam servers */
	DECLARE_STATIC_STEAM_CALLBACK(SteamServersDisconnected_t, FOnlineAsyncEventSteamServersDisconnected);

	/** Delegate registered with Steam to trigger when receiving a large player avatar */
	DECLARE_STATIC_STEAM_CALLBACK(AvatarImageLoaded_t, FOnlineAsyncEventAvatarImageLoaded);


	/** Delegate registered with Steam to trigger when the game server receives anticheat policy status */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(GSPolicyResponse_t, FOnlineAsyncEventAntiCheatStatus);

	/** Delegate registered with Steam to trigger when the game server connects to the Steam servers */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(SteamServersConnected_t, FOnlineAsyncEventSteamServersConnectedGameServer);

	/** Delegate registered with Steam to trigger when the game server disconnects from the Steam servers */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(SteamServersDisconnected_t, FOnlineAsyncEventSteamServersDisconnectedGameServer);


	/** Delegate registered with Steam to trigger when the Steam backend requests that the client disconnect from a server */
	DECLARE_STATIC_STEAM_CALLBACK(ClientGameServerDeny_t, FOnlineAsyncEventSteamClientGameServerDeny);

	/** Delegate registered with Steam to trigger when the Steam backend returns the final result of a server or peer auth attempt */
	DECLARE_STATIC_STEAM_CALLBACK(ValidateAuthTicketResponse_t, FOnlineAsyncEventSteamClientValidateAuth);


#if WITH_STEAMWORKS_P2PAUTH
	/** Delegate registered with Steam to trigger when the game server receives an auth approval for a client */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(ValidateAuthTicketResponse_t, FOnlineAsyncEventSteamServerClientValidateAuth);
#else
	/** Delegate registered with Steam to trigger when the game server receives auth approval for a client */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(GSClientApprove_t, FOnlineAsyncEventSteamServerClientApprove);

	/** Delegate registered with Steam to trigger when the game server receives an auth denial for a client */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(GSClientDeny_t, FOnlineAsyncEventSteamServerClientDeny);

	/** Delegate registered with Steam to trigger when the game server receives a request to kick a client */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(GSClientKick_t, FOnlineAsyncEventSteamServerClientKick);
#endif


	/** Delegates registered with Steam to trigger when the Steam backend notifies the client of a Steam sockets connect failure */
	DECLARE_STATIC_STEAM_CALLBACK(P2PSessionConnectFail_t, FOnlineAsyncEventSteamSocketsConnectFail);


	/** Delegates registered with Steam to trigger when the game server receives a Steam sockets connection request */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(P2PSessionRequest_t, FOnlineAsyncEventSteamServerSocketsConnectRequest);

	/** Delegates registered with Steam to trigger when the game server receives a Steam sockets connect failure on a client connection */
	DECLARE_STATIC_STEAM_GAMESERVER_CALLBACK(P2PSessionConnectFail_t, FOnlineAsyncEventSteamServerSocketsConnectFail);


#if STEAM_MATCHMAKING_LOBBY
	/** Delegates registered with Steam to trigger when the Steam backend notifies the client of a lobby creation result */
	DECLARE_STATIC_STEAM_CALLBACK(LobbyCreated_t, FOnlineAsyncEventSteamLobbyCreated);

	/** Delegates registered with Steam to trigger when the Steam backend notifies the client of a lobby enter result */
	DECLARE_STATIC_STEAM_CALLBACK(LobbyEnter_t, FOnlineAsyncEventSteamLobbyEnterResponse);

	/** Delegates registered with Steam to trigger when the Steam backend notifies the client that data/info for a lobby has changed */
	DECLARE_STATIC_STEAM_CALLBACK(LobbyDataUpdate_t, FOnlineAsyncEventSteamLobbyDataUpdate);

	/** Delegates registered with Steam to trigger when the Steam backend notifies the client that the status of a lobby member has changed */
	DECLARE_STATIC_STEAM_CALLBACK(LobbyChatUpdate_t, FOnlineAsyncEventSteamLobbyMemberUpdate);

	/** Delegates registered with Steam to trigger when the Steam backend notifies the client that a lobby game is created and ready to join */
	DECLARE_STATIC_STEAM_CALLBACK(LobbyGameCreated_t, FOnlineAsyncEventSteamLobbyGameCreated);

	/** Delegates registered with Steam to trigger when the Steam backend notifies the client of a chat message in a lobby */
	DECLARE_STATIC_STEAM_CALLBACK(LobbyChatMsg_t, FOnlineAsyncEventSteamLobbyChatMessage);

	/** Delegates registered with Steam to trigger when the Steam backend notifies the client of lobby search results */
	DECLARE_STATIC_STEAM_CALLBACK(LobbyMatchList_t, FOnlineAsyncEventSteamLobbyMatchList);

	/** Delegates registered with Steam to trigger when the Steam backend notifies the client of a lobby invite */
	DECLARE_STATIC_STEAM_CALLBACK(LobbyInvite_t, FOnlineAsyncEventSteamLobbyInvite);

	/** Delegates registered with Steam to trigger when the Steam backend notifies the client of an accepted (through overlay) lobby invite */
	DECLARE_STATIC_STEAM_CALLBACK(GameLobbyJoinRequested_t, FOnlineAsyncEventSteamLobbyInviteAccepted);
#endif

public:
	/**
	 * Base constructor
	 */
	FOnlineAsyncTaskManagerSteam()
		: CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamStatsReceived)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamStatsStored)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamExternalUITriggered)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamShutdown)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerChangeRequest)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamRichPresenceJoinRequest)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServersConnected)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServersDisconnected)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventAvatarImageLoaded)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventAntiCheatStatus)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServersConnectedGameServer)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServersDisconnectedGameServer)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamClientGameServerDeny)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamClientValidateAuth)
#if WITH_STEAMWORKS_P2PAUTH
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerClientValidateAuth)
#else
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerClientApprove)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerClientDeny)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerClientKick)
#endif
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamSocketsConnectFail)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerSocketsConnectRequest)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamServerSocketsConnectFail)
#if STEAM_MATCHMAKING_LOBBY
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyCreated)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyEnterResponse)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyDataUpdate)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyMemberUpdate)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyGameCreated)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyChatMessage)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyMatchList)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyInvite)
		, CONSTRUCT_STATIC_STEAM_CALLBACK(FOnlineAsyncEventSteamLobbyInviteAccepted)
#endif
	{
	}

	/**
	 * Base destructor
	 */
	~FOnlineAsyncTaskManagerSteam()
	{
	}


	/**
	 * Init the online async task manager
	 *
	 * @return	Returns TRUE if initialization was successful, FALSE otherwise
	 */
	virtual UBOOL Init();

	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop();

	/**
	 * Called in the context of the aggregating thread to perform any cleanup
	 */
	virtual void Exit();

	/**
	 * Give the online service a chance to do work
	 * NOTE: Call only from online thread
	 */
	virtual void OnlineTick();
};


/**
 * Asynchronous task for Steam, for requesting and receiving a specific users stats/achievements data
 *
 * @param SteamDataStruct	The Steam API data struct which defines the callback data
 * @param bGameServerInterface	Whether or not this async task is for the Steam game server API
 */
template<class SteamDataStruct, UBOOL bGameServerInterface>
class FOnlineAsyncTaskSteamUserStatsReceivedBase : public FOnlineAsyncTaskSteam<SteamDataStruct, UOnlineSubsystemSteamworks, bGameServerInterface>
{
protected:
	/** User this data is for */
	QWORD	UserId;

	/** Result of the download */
	EResult	StatsReceivedResult;


	/** Hidden constructor */
	FOnlineAsyncTaskSteamUserStatsReceivedBase()
		: FOnlineAsyncTaskSteam()
		, UserId(0)
		, StatsReceivedResult(k_EResultOK)
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamUserStatsReceivedBase(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteam(InSubsystem, InCallbackHandle)
		, UserId(0)
		, StatsReceivedResult(k_EResultOK)
	{
	}

	/**
	 * Base virtual destructor
	 */
	virtual ~FOnlineAsyncTaskSteamUserStatsReceivedBase()
	{
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

		if (CallbackInterface->CurrentStatsRead != NULL)
		{
			UBOOL bOk = TRUE;
			FString Persona = TEXT("");
			CSteamID UserSteamId((uint64)UserId);

			if (GSteamFriends != NULL)
			{
				Persona = UTF8_TO_TCHAR(GSteamFriends->GetFriendPersonaName(UserSteamId));
			}

			if (StatsReceivedResult == k_EResultFail)
			{
				debugf(NAME_DevOnline, TEXT("No steam user stats for '") I64_FORMAT_TAG TEXT("'"), UserId);

				// Add blank data
				INT NewIndex = CallbackInterface->CurrentStatsRead->Rows.AddZeroed();
				FOnlineStatsRow& Row = CallbackInterface->CurrentStatsRead->Rows(NewIndex);

				Row.PlayerID.Uid = UserId;
				Row.Rank.SetData(TEXT("--"));

				// Fill the columns
				INT NumColumns = CallbackInterface->CurrentStatsRead->ColumnIds.Num();

				Row.Columns.AddZeroed(NumColumns);

				// And copy the fields into the columns
				for (INT FieldIndex=0; FieldIndex<NumColumns; FieldIndex++)
				{
					FOnlineStatsColumn& Col = Row.Columns(FieldIndex);

					Col.ColumnNo = CallbackInterface->CurrentStatsRead->ColumnIds(FieldIndex);
					Col.StatValue.SetData(TEXT("--"));
				}

				Row.NickName = Persona;


				bOk = FALSE;
			}
			// generic failure
			else if (bIOFailure || StatsReceivedResult != k_EResultOK)
			{
				debugf(NAME_DevOnline, TEXT("Failed to obtain specific steam user stats for '") I64_FORMAT_TAG TEXT("', error: %s"),
					UserId, *SteamResultString(StatsReceivedResult));

				CallbackInterface->CurrentStatsRead = NULL;

				FAsyncTaskDelegateResults Results(E_FAIL);
				TriggerOnlineDelegates(CallbackInterface, CallbackInterface->ReadOnlineStatsCompleteDelegates, &Results);

				return;
			}
			// got stats
			else
			{
				debugf(NAME_DevOnline, TEXT("Obtained specific steam user stats for '") I64_FORMAT_TAG TEXT("' ('%s')"),
					UserId, *Persona);

				// Add blank data
				INT NewIndex = CallbackInterface->CurrentStatsRead->Rows.AddZeroed();
				FOnlineStatsRow& Row = CallbackInterface->CurrentStatsRead->Rows(NewIndex);

				Row.PlayerID.Uid = UserId;
				Row.Rank.SetData(TEXT("--"));
	
				// Fill the columns
				INT NumColumns = CallbackInterface->CurrentStatsRead->ColumnIds.Num();

				Row.Columns.AddZeroed(NumColumns);

				// And copy the fields into the columns
				for (INT FieldIndex=0; FieldIndex<NumColumns; FieldIndex++)
				{
					FOnlineStatsColumn& Col = Row.Columns(FieldIndex);
					Col.ColumnNo = CallbackInterface->CurrentStatsRead->ColumnIds(FieldIndex);

					FString Key(CallbackInterface->GetStatsFieldName(CallbackInterface->CurrentStatsRead->ViewId, Col.ColumnNo));

					int32 IntData = 0;
					float FloatData = 0.0f;

					if (!bGameServerInterface)
					{
						if (GSteamUserStats->GetUserStat(UserSteamId, TCHAR_TO_UTF8(*Key), &IntData))
						{
							Col.StatValue.SetData((INT)IntData);
						}
						else if (GSteamUserStats->GetUserStat(UserSteamId, TCHAR_TO_UTF8(*Key), &FloatData))
						{
							Col.StatValue.SetData((FLOAT)FloatData);
						}
						else
						{
							debugf(NAME_DevOnline, TEXT("Failed to get steam user stats value for key '%s'"), *Key);
							bOk = FALSE;
						}
					}
					else
					{
						if (GSteamGameServerStats->GetUserStat(UserSteamId, TCHAR_TO_UTF8(*Key), &IntData))
						{
							Col.StatValue.SetData((INT)IntData);
						}
						else if (GSteamGameServerStats->GetUserStat(UserSteamId, TCHAR_TO_UTF8(*Key), &FloatData))
						{
							Col.StatValue.SetData((FLOAT)FloatData);
						}
						else
						{
							debugf(NAME_DevOnline, TEXT("Failed to get steam user stats value for key '%s'"), *Key);
							bOk = FALSE;
						}
					}
				}

				Row.NickName = Persona;
			}

			// see if we're done waiting on requests...
			if (CallbackInterface->CurrentStatsRead->Rows.Num() == CallbackInterface->CurrentStatsRead->TotalRowsInView)
			{
				debugf(NAME_DevOnline, TEXT("Obtained all the steam user stats we wanted."));

				CallbackInterface->CurrentStatsRead = NULL;

				FAsyncTaskDelegateResults Results(bOk ? S_OK : E_FAIL);
				TriggerOnlineDelegates(CallbackInterface, CallbackInterface->ReadOnlineStatsCompleteDelegates, &Results);
			}
		}
	}
};

/**
 * Steam Client API version of the above
 */
class FOnlineAsyncTaskSteamUserStatsReceived : public FOnlineAsyncTaskSteamUserStatsReceivedBase<UserStatsReceived_t, FALSE>
{
private:
	/** Hidden constructor */
	FOnlineAsyncTaskSteamUserStatsReceived()
		: FOnlineAsyncTaskSteamUserStatsReceivedBase()
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamUserStatsReceived(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteamUserStatsReceivedBase(InSubsystem, InCallbackHandle)
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamUserStatsReceived completed bWasSuccessful: %d, User: ") I64_FORMAT_TAG
					TEXT(", Result: %s"),
					((StatsReceivedResult == k_EResultOK) ? 1 : 0), UserId,
					*SteamResultString(StatsReceivedResult));
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
	virtual UBOOL ProcessSteamCallback(UserStatsReceived_t* CallbackData, UBOOL bInIOFailure)
	{
		UBOOL bSuccess = FALSE;

		UserId = CallbackData->m_steamIDUser.ConvertToUint64();
		StatsReceivedResult = CallbackData->m_eResult;

		if (GSteamAppID == CallbackData->m_nGameID)
		{
			debugf(NAME_DevOnline, TEXT("UserStatsReceived: bIOFailure: %i, UID: ") I64_FORMAT_TAG TEXT(", Result: %s"),
				(INT)bInIOFailure, UserId, *SteamResultString(StatsReceivedResult));

			bSuccess = TRUE;
		}

		return bSuccess;
	}
};

/**
 * Steam Game Server API version of the above
 */
class FOnlineAsyncTaskSteamServerUserStatsReceived : public FOnlineAsyncTaskSteamUserStatsReceivedBase<GSStatsReceived_t, TRUE>
{
private:
	/** Hidden constructor */
	FOnlineAsyncTaskSteamServerUserStatsReceived()
		: FOnlineAsyncTaskSteamUserStatsReceivedBase()
	{
	}

public:
	/**
	 * Main constructor
	 *
	 * @param InSubsystem		The subsystem object this task is linked to
	 * @param CallbackHandle	The SteamAPI callback handle identifying this task
	 */
	FOnlineAsyncTaskSteamServerUserStatsReceived(UOnlineSubsystemSteamworks* InSubsystem, SteamAPICall_t InCallbackHandle)
		: FOnlineAsyncTaskSteamUserStatsReceivedBase(InSubsystem, InCallbackHandle)
	{
	}

	/**
	 * Get a human readable description of task
	 *
	 * @return	The description of the task
	 */
	virtual FString ToString() const
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamServerUserStatsReceived completed bWasSuccessful: %d, User: ")
					I64_FORMAT_TAG TEXT(", Result: %s"),
					((StatsReceivedResult == k_EResultOK) ? 1 : 0), UserId,
					*SteamResultString(StatsReceivedResult));
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
	virtual UBOOL ProcessSteamCallback(GSStatsReceived_t* CallbackData, UBOOL bInIOFailure)
	{
		UserId = CallbackData->m_steamIDUser.ConvertToUint64();
		StatsReceivedResult = CallbackData->m_eResult;

		debugf(NAME_DevOnline, TEXT("ServerUserStatsReceived: bIOFailure: %i, UID: ") I64_FORMAT_TAG TEXT(", Result: %s"), (INT)bInIOFailure,
			UserId, *SteamResultString(StatsReceivedResult));

		return TRUE;
	}
};


#endif	// WITH_UE3_NETWORKING && WITH_STEAMWORKS

#endif  // !INCLUDED_ONLINEASYNCTASKMANAGERSTEAM_H


