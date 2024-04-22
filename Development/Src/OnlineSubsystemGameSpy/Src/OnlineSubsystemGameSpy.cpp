/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemGameSpy.h"

#if WITH_UE3_NETWORKING && WITH_GAMESPY

/**
 * Routes GameSpy memory allocations to our allocator
 *
 * @param SizeToAllocate the size of memory to allocate
 *
 * @return pointer to the block of memory or NULL if failed to allocate
 */
void* CallbackMalloc(size_t SizeToAllocate)
{
	return GMalloc->Malloc(SizeToAllocate);
}

/**
 * Routes GameSpy memory allocations to our allocator
 *
 * @param OriginalPointer the existing block of memory
 * @param SizeToAllocate the size of memory to allocate
 *
 * @return pointer to the block of memory or NULL if failed to allocate
 */
void* CallbackRealloc(void* OriginalPointer,size_t SizeToAllocate)
{
	return GMalloc->Realloc(OriginalPointer,SizeToAllocate);
}

/**
 * Routes GameSpy memory allocations to our allocator
 *
 * @param BoundarySize the alignment wanted
 * @param SizeToAllocate the size of memory to allocate
 *
 * @return pointer to the block of memory or NULL if failed to allocate
 */
void* CallbackAlignedMalloc(size_t BoundarySize,size_t SizeToAllocate)
{
	return GMalloc->Malloc(SizeToAllocate,BoundarySize);
}

/**
 * Routes GameSpy memory allocations to our allocator
 *
 * @param PointerToFree the block of memory to free
 */
void CallbackFree(void* PointerToFree)
{
	return GMalloc->Free(PointerToFree);
}

/** Used to avoid having to involve script code with the challenge/response code */
static UOnlineSubsystemGameSpy* GOnlineSubsystemGameSpy = NULL;

/**
 * Calculates a new string based off of the challenge string
 *
 * @param Connection the connection to generate the response string for
 * @param bIsReauth whether we are processing a reauth or initial auth request
 */
void appGetOnlineChallengeResponse(UNetConnection* Connection,UBOOL bIsReauth)
{
	if (GOnlineSubsystemGameSpy)
	{
		GOnlineSubsystemGameSpy->GetChallengeResponse(Connection,bIsReauth);
	}
}

/**
 * Used to send a request to GameSpy to authenticate the user's CD Key
 *
 * @param ConnToAuth the connection to authenticate
 */
void appSubmitOnlineAuthRequest(UNetConnection* ConnToAuth)
{
	if (GOnlineSubsystemGameSpy)
	{
		GOnlineSubsystemGameSpy->SubmitAuthRequest(ConnToAuth);
	}
}

/**
 * Used to respond to reauth a request from GameSpy. Used to see if a user is still
 * using a given CD key
 *
 * @param Connection the connection to authenticate
 * @param Hint the hint that GameSpy sent us
 */
void appSubmitOnlineReauthRequest(UNetConnection* Connection,INT Hint)
{
	if (GOnlineSubsystemGameSpy)
	{
		GOnlineSubsystemGameSpy->SubmitReauthResponse(Connection,Hint);
	}
}

/** Releases the CD key for the specified client */
void appReleaseGameSpyKey(INT ResponseId)
{
#if WANTS_CD_KEY_AUTH
	gcd_disconnect_user(GOnlineSubsystemGameSpy->GameID,
		ResponseId);
#endif
}

/**
 * Casts the pointer to the GameSpy subsystem and verifies the type
 *
 * @param Pointer the chunk of memory to validate is the GameSpy interface
 */
static FORCEINLINE UOnlineSubsystemGameSpy* CastToSubsystem(void* Pointer)
{
	return CastChecked<UOnlineSubsystemGameSpy>((UObject*)Pointer);
}

/**
 * Finds the player's name based off of their net id
 *
 * @param NetId the net id we are finding the player name for
 *
 * @return the name of the player that matches the net id
 */
static FString GetPlayerName(FUniqueNetId NetId)
{
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	AGameReplicationInfo* GRI = WorldInfo->GRI;
	if (GRI != NULL)
	{
		for (INT Index = 0; Index < GRI->PRIArray.Num(); Index++)
		{
			APlayerReplicationInfo* PRI = GRI->PRIArray(Index);
			if ((QWORD&)PRI->UniqueId == (QWORD&)NetId)
			{
				return PRI->PlayerName;
			}
		}
		for (INT Index = 0; Index < GRI->InactivePRIArray.Num(); Index++)
		{
			APlayerReplicationInfo* PRI = GRI->InactivePRIArray(Index);
			if ((QWORD&)PRI->UniqueId == (QWORD&)NetId)
			{
				return PRI->PlayerName;
			}
		}
	}
	// We don't have a nick for this player
	return FString();
}

/**
 * Called when there was an error with the GP SDK
 *
 * @param Connection the GP connection
 * @param Arg the error info
 * @param UserData pointer to the subsystem
 */
static void GPErrorCallback(GPConnection * Connection, GPErrorArg * Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPErrorCallback(Arg);
}

/**
 * Called when a buddy request is received
 *
 * @param Connection the GP connection
 * @param Arg the request info
 * @param UserData pointer to the subsystem
 */
static void GPRecvBuddyRequestCallback(GPConnection * Connection, GPRecvBuddyRequestArg * Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPRecvBuddyRequestCallback(Arg);
}

/**
 * Called when a buddy's status is received
 *
 * @param Connection the GP connection
 * @param Arg the status info
 * @param UserData pointer to the subsystem
 */
static void GPRecvBuddyStatusCallback(GPConnection * Connection, GPRecvBuddyStatusArg * Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPRecvBuddyStatusCallback(Arg);
}

/**
 * Called when a buddy message is received
 *
 * @param Connection the GP connection
 * @param Arg the message info
 * @param UserData pointer to the subsystem
 */
static void GPRecvBuddyMessageCallback(GPConnection * Connection, GPRecvBuddyMessageArg * Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPRecvBuddyMessageCallback(Arg);
}

/**
 * Called when a buddy request has been authorized
 *
 * @param Connection the GP connection
 * @param Arg the auth info
 * @param UserData pointer to the subsystem
 */
static void GPRecvBuddyAuthCallback(GPConnection * Connection, GPRecvBuddyAuthArg * Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPRecvBuddyAuthCallback(Arg);
}

/**
 * Called when a buddy has revoked your friendship
 *
 * @param Connection the GP connection
 * @param Arg the revoke info
 * @param UserData pointer to the subsystem
 */
static void GPRecvBuddyRevokeCallback(GPConnection * Connection, GPRecvBuddyRevokeArg * Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPRecvBuddyRevokeCallback(Arg);
}

/**
 * Called at the end of a connection attempt
 *
 * @param Connection the GP connection
 * @param Arg the connection result
 * @param UserData pointer to the subsystem
 */
static void GPConnectCallback(GPConnection * Connection, GPConnectResponseArg* Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPConnectCallback(Arg);
}

/**
 * Called at the end of a new user attempt
 *
 * @param Connection the GP connection
 * @param Arg the new user result
 * @param UserData pointer to the subsystem
 */
static void GPNewUserCallback(GPConnection * Connection, GPNewUserResponseArg* Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPNewUserCallback(Arg);
}

/**
 * Called in response to a request for a remote profile's info
 *
 * @param Connection the GP connection
 * @param Arg the profile's info
 * @param UserData pointer to the subsystem
 */
static void GPGetInfoCallback(GPConnection * Connection, GPGetInfoResponseArg* Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPGetInfoCallback(Arg);
}

/**
 * Called in response to a request for a stats player's nick
 *
 * @param Connection the GP connection
 * @param Arg the player's info
 * @param UserData pointer to the subsystem
 */
static void GPGetStatsNickCallback(GPConnection * Connection, GPGetInfoResponseArg* Arg, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPGetStatsNickCallback(Arg);
}

/**
 * Ignored
 *
 * @param Ignored
 * @param Ignored
 * @param Ignored
 */
static void GPTransferCallback(GPConnection*,GPTransferCallbackArg*,void*)
{
}

/**
 * Called after requesting the profile from the Sake DB
 *
 * @param Sake a handle to the Sake object
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 * @param UserData pointer to the subsystem
 */
static void SakeRequestProfileCallback(SAKE Sake, SAKERequest Request, SAKERequestResult Result, SAKEGetMyRecordsInput* Input, SAKEGetMyRecordsOutput* Output, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->SakeRequestProfileCallback(Request, Result, Input, Output);
}

/**
 * Called after creating a profile record in the Sake DB
 *
 * @param Sake a handle to the Sake object
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 * @param UserData pointer to the subsystem
 */
static void SakeCreateProfileCallback(SAKE Sake, SAKERequest Request, SAKERequestResult Result, SAKECreateRecordInput* Input, SAKECreateRecordOutput* Output, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->SakeCreateProfileCallback(Request, Result, Input, Output);
}

/**
 * Called after updating the profile stored in the Sake DB
 *
 * @param Sake a handle to the Sake object
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 * @param UserData pointer to the subsystem
 */
static void SakeUpdateProfileCallback(SAKE Sake, SAKERequest Request, SAKERequestResult Result, SAKEUpdateRecordInput* Input, void* Output, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->SakeUpdateProfileCallback(Request, Result, Input);
}

/**
 * Called after searching for records in the Sake DB
 *
 * @param Sake a handle to the Sake object
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 * @param UserData pointer to the subsystem
 */
static void SakeSearchForRecordsCallback(SAKE Sake, SAKERequest Request, SAKERequestResult Result, SAKESearchForRecordsInput* Input, SAKESearchForRecordsOutput* Output, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->SakeSearchForRecordsCallback(Request, Result, Input, Output);
}

/**
 * Called after requesting a Sake table record count
 *
 * @param Sake a handle to the Sake object
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 * @param UserData pointer to the subsystem
 */
static void SakeGetRecordCountCallback(SAKE Sake, SAKERequest Request, SAKERequestResult Result, SAKEGetRecordCountInput* Input, SAKEGetRecordCountOutput* Output, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->SakeGetRecordCountCallback(Request, Result, Input, Output);
}

/**
 * Called after a login certificate request
 *
 * @param HttpResult the result of the http request
 * @param Response the response to the request
 * @param UserData pointer to the subsystem
 */
static void LoginCertificateCallback(GHTTPResult HttpResult, WSLoginResponse * Response, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->LoginCertificateCallback(HttpResult, Response);
}

/**
 * Called after a PS3 login certificate request
 *
 * @param HttpResult the result of the http request
 * @param Response the response to the request
 * @param UserData pointer to the subsystem
 */
static void PS3LoginCertificateCallback(GHTTPResult HttpResult, WSLoginPs3CertResponse* Response, void * UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->PS3LoginCertificateCallback(HttpResult, Response);
}

/**
 * Called after an attempt to create a session
 *
 * @param Interface a handle to the SC interface
 * @param HttpResult the result of the http request
 * @param Result the result of the create session attempt
 * @param UserData pointer to the subsystem
 */
static void CreateStatsSessionCallback(SCInterfacePtr Interface, GHTTPResult HttpResult, SCResult Result, void* UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->CreateStatsSessionCallback(HttpResult, Result);
}

/**
 * Called after the host sets his report intention
 *
 * @param Interface a handle to the SC interface
 * @param HttpResult the result of the http request
 * @param Result the result of the set report intention
 * @param UserData pointer to the subsystem
 */
static void HostSetStatsReportIntentionCallback(SCInterfacePtr Interface, GHTTPResult HttpResult, SCResult Result, void* UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->HostSetStatsReportIntentionCallback(HttpResult, Result);
}

/**
 * Called after the client sets his report intention
 *
 * @param Interface a handle to the SC interface
 * @param HttpResult the result of the http request
 * @param Result the result of the set report intention
 * @param UserData pointer to the subsystem
 */
static void ClientSetStatsReportIntentionCallback(SCInterfacePtr Interface, GHTTPResult HttpResult, SCResult Result, void* UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->ClientSetStatsReportIntentionCallback(HttpResult, Result);
}

/**
 * Called after the host submits a report
 *
 * @param Interface a handle to the SC interface
 * @param HttpResult the result of the http request
 * @param Result the result of the report submission
 * @param UserData pointer to the subsystem
 */
void SubmitStatsReportCallback(SCInterfacePtr Interface, GHTTPResult HttpResult, SCResult Result, void* UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->SubmitStatsReportCallback(HttpResult, Result);
}

/**
 * Called when a profile search has completed
 *
 * @param Connection the GP connection
 * @param Arg the search result
 * @param UserData pointer to the subsystem
 */
static void GPProfileSearchCallback(GPConnection* Connection, GPProfileSearchResponseArg* Arg, void* UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPProfileSearchCallback(Arg);
}

/**
 * Called when a game invite has been received
 *
 * @param Connection the GP connection
 * @param Arg the game invite information
 * @param UserData pointer to the subsystem
 */
static void GPRecvGameInviteCallback(GPConnection* Connection, GPRecvGameInviteArg* Arg, void* UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->GPRecvGameInviteCallback(Arg);
}

/**
 * Called when GameSpy has determined if a player is authorized to play
 *
 * @param GameId the game id that the auth request was for
 * @param LocalId the id the game assigned the user when requesting auth
 * @param Authenticated whether the player was authenticated or should be rejected
 * @param ErrorMsg a string describing the error if there was one
 * @param UserData the subsystem pointer used to callback on
 */
static void CDKeyAuthCallBack(INT GameId,INT LocalId,INT Authenticated,char* ErrorMsg,void* UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->CDKeyAuthCallBack(GameId,LocalId,Authenticated,ANSI_TO_TCHAR(ErrorMsg));
}

/**
 * Called when GameSpy has wants verify if a player is still playing on a host
 *
 * @param GameId the game id that the auth request was for
 * @param LocalId the id the game assigned the user when requesting auth
 * @param Hint the session id that should be passed to the reauth call
 * @param Challenge a string describing the error if there was one
 * @param UserData the subsystem pointer used to callback on
 */
static void RefreshCDKeyAuthCallBack(INT GameId,INT LocalId,INT Hint,char* Challenge,void* UserData)
{
	UOnlineSubsystemGameSpy* Subsystem = CastToSubsystem(UserData);
	Subsystem->RefreshCDKeyAuthCallBack(GameId,LocalId,Hint,ANSI_TO_TCHAR(Challenge));
}

/**
 * Calls the subsystem letting it set the name on the message
 *
 * @param Subsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FOnlineAsyncTaskGameSpyGetPlayerNick::ProcessAsyncResults(UOnlineSubsystemGameSpy* Subsystem)
{
	if (MessageSender.Len())
	{
		// Update the sender for the message specified
		FOnlineFriendMessage& Message = Subsystem->CachedFriendMessages(MessageIndex);
		Message.SendingPlayerNick = MessageSender;
		// Fire a different delegate for game invites
		if (Message.bIsGameInvite)
		{
			// Don't bother constructing the event params if there are no delegates
			if (Subsystem->ReceivedGameInviteDelegates.Num() > 0)
			{
				// Build the delegate information
				OnlineSubsystemGameSpy_eventOnReceivedGameInvite_Parms Parms(EC_EventParm);
				Parms.LocalUserNum = 0;
				Parms.InviterName = MessageSender;
				TriggerOnlineDelegates(Subsystem,Subsystem->ReceivedGameInviteDelegates,&Parms);
			}
		}
		else if (Message.bIsFriendInvite)
		{
			// Don't bother constructing the event params if there are no delegates
			if (Subsystem->FriendInviteReceivedDelegates.Num() > 0)
			{
				OnlineSubsystemGameSpy_eventOnFriendInviteReceived_Parms Parms(EC_EventParm);
				Parms.LocalUserNum = 0;
				Parms.RequestingPlayer = Message.SendingPlayerId;
				Parms.Message = Message.Message;
				Parms.RequestingNick = Message.SendingPlayerNick;
				// Fire the delegates so the UI can be shown
				TriggerOnlineDelegates(Subsystem,Subsystem->FriendInviteReceivedDelegates,&Parms);
			}
		}
		else
		{
			// Don't bother constructing the event params if there are no delegates
			if (Subsystem->FriendMessageReceivedDelegates.Num() > 0)
			{
				OnlineSubsystemGameSpy_eventOnFriendMessageReceived_Parms Parms(EC_EventParm);
				Parms.LocalUserNum = 0;
				Parms.SendingPlayer = Message.SendingPlayerId;
				Parms.Message = Message.Message;
				Parms.SendingNick = Message.SendingPlayerNick;
				// Fire the delegates so the UI can be shown
				TriggerOnlineDelegates(Subsystem,Subsystem->FriendMessageReceivedDelegates,&Parms);
			}
		}
	}
	else
	{
		// Don't know who sent it so remove
		Subsystem->CachedFriendMessages.Remove(MessageIndex);
	}
	return TRUE;
}

/**
 * Ticks the NP login task
 *
 * @param DeltaTime ignored
 */
void FOnlineAsyncTaskHandleExternalLogin::Tick(FLOAT)
{
#if PS3
	// See if the process has completed or not
	if (appPS3TickNetworkStartUtility(bWasSuccessful))
	{
		CompletionStatus = S_OK;
	}
#else
	CompletionStatus = E_FAIL;
#endif
}

/**
 * Determines which set of delegates to fire based on success or not
 *
 * @param Subsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FOnlineAsyncTaskHandleExternalLogin::ProcessAsyncResults(UOnlineSubsystemGameSpy* Subsystem)
{
#if PS3
	if (bWasSuccessful == FALSE)
	{
		// Fire off the failure delegates
		FLoginFailedParms Params(0,GP_LOGIN_CONNECTION_FAILED);
		TriggerOnlineDelegates(Subsystem,Subsystem->LoginFailedDelegates,&Params);
	}
#endif
	return TRUE;
}

/**
 * Calls the subsystem letting it do a network write if desired
 *
 * @param Subsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FOnlineAsyncTaskGameSpyWriteProfile::ProcessAsyncResults(UOnlineSubsystemGameSpy* Subsystem)
{
	Subsystem->ConditionallyWriteProfileToSake(GetCompletionCode(),
		Writer.GetFinalBuffer(),
		Writer.GetFinalBufferLength());
	return TRUE;
}

/**
 * Calls the subsystem letting it know the stats report is done
 *
 * @param Subsystem the object to make the final call on
 *
 * @return TRUE if this task should be cleaned up, FALSE to keep it
 */
UBOOL FOnlineAsyncTaskGameSpySubmitStats::ProcessAsyncResults(UOnlineSubsystemGameSpy* Subsystem)
{
	Subsystem->bIsStatsSessionOk = FALSE;
	return TRUE;
}

/**
 * Called on the specific instance to return the results of the async operation
 *
 * @param Connection the GP connection
 * @param Arg the new user result
 */
void FOnlineAsyncTaskGameSpyCreateNewUser::GPNewUserCallback(GPConnection* Connection,GPNewUserResponseArg* Arg)
{
	// If the error is bad nick, it means the account exists, but the nick doesn't
	if (((DWORD)Arg->result) == GP_NEWUSER_BAD_NICK)
	{
		debugf(NAME_DevOnline,TEXT("gpNewUser() returned GP_NEWUSER_BAD_NICK"));
		debugf(NAME_DevOnline,TEXT("Trying to register the unique nick within an existing profile instead"));
		// Connect and then register the unique nick
		GPResult Result = gpConnect(Connection,
			(const UCS2String)*UniqueNick,
			(const UCS2String)*EmailAddress,
			(const UCS2String)*Password,
			GP_NO_FIREWALL,
			GP_NON_BLOCKING,
			(GPCallback)_GPConnectCallback,
			this);
		if (Result != GP_NO_ERROR)
		{
			Arg->result = Result;
			// Forward the error through the usual path
			GOnlineSubsystemGameSpy->GPNewUserCallback(Arg);
			SetCompletionCode(E_FAIL);
		}
	}
	else
	{
		// Forward through the usual path
		GOnlineSubsystemGameSpy->GPNewUserCallback(Arg);
		// Make sure it gets deleted next tick
		SetCompletionCode(E_FAIL);
	}
}

/**
 * Called on the specific instance to return the results of user connect
 *
 * @param Connection the GP connection
 * @param Arg the new user result
 */
void FOnlineAsyncTaskGameSpyCreateNewUser::GPConnectCallback(GPConnection* Connection,GPConnectResponseArg* Arg)
{
	// If it succeeded, then register the unique nick
	if (Arg->result == GP_NO_ERROR)
	{
		// Copy this for the unique nick registration callback
		Profile = Arg->profile;
		// Attempt to bind the unique nick to this account
		GPResult Result = gpRegisterUniqueNick(Connection,
			(const UCS2String)*UniqueNick,
			(const UCS2String)*ProductKey,
			GP_NON_BLOCKING,
			(GPCallback)_GPRegisterUniqueNickCallback,
			this);
		if (Result != GP_NO_ERROR)
		{
			gpDisconnect(Connection);
			GPNewUserResponseArg ErrorArg;
			// Create a fake new user response and forward an error code
			ErrorArg.profile = Profile;
			ErrorArg.result = Result;
			// Now process as if we failed to create
			GOnlineSubsystemGameSpy->GPNewUserCallback(&ErrorArg);
			// Make sure it gets deleted next tick
			SetCompletionCode(E_FAIL);
		}
	}
	else
	{
		GPNewUserResponseArg ErrorArg;
		// Create a fake new user response and forward an error code
		ErrorArg.profile = Arg->profile;
		ErrorArg.result = (GPResult)GP_NEWUSER_BAD_NICK;
		GOnlineSubsystemGameSpy->GPNewUserCallback(&ErrorArg);
		// Make sure it gets deleted next tick
		SetCompletionCode(E_FAIL);
	}
}

/**
 * Called at the end of the unique nick registration on the specific instance housing the data
 *
 * @param Connection the GP connection
 * @param Arg the registration result
 */
void FOnlineAsyncTaskGameSpyCreateNewUser::GPRegisterUniqueNickCallback(GPConnection* Connection,GPRegisterUniqueNickResponseArg* Arg)
{
	// Disconnect because the UI is going to sign us in automatically
	gpDisconnect(Connection);
	// Send a happiness or sadness message
	if (Arg->result == GP_NO_ERROR)
	{
		debugf(NAME_DevOnline,TEXT("Registration of the unique nick within an existing profile succeeded"));
		GPNewUserResponseArg GoodCreateArg;
		// Create a fake new user response and forward an error code
		GoodCreateArg.profile = Profile;
		GoodCreateArg.result = GP_NO_ERROR;
		// Now process as if the create succeeded ok
		GOnlineSubsystemGameSpy->GPNewUserCallback(&GoodCreateArg);
		SetCompletionCode(S_OK);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Registration of the unique nick within an existing profile failed"));
		GPNewUserResponseArg ErrorArg;
		// Create a fake new user response and forward an error code
		ErrorArg.profile = Profile;
		ErrorArg.result = (GPResult)GP_NEWUSER_BAD_NICK;
		GOnlineSubsystemGameSpy->GPNewUserCallback(&ErrorArg);
		// Make sure it gets deleted next tick
		SetCompletionCode(E_FAIL);
	}
}

/**
 * Called when there was an error with the GP SDK
 *
 * @param Arg the error info
 */
void UOnlineSubsystemGameSpy::GPErrorCallback(GPErrorArg* Arg)
{
	debugf(NAME_DevOnline,
		TEXT("GPErrorCallback called with result = 0x%08X, error code = 0x%08X"),
		(DWORD)Arg->result,
		(DWORD)Arg->errorCode);
	if (Arg->result != GP_NO_ERROR)
	{
		UBOOL bIsConnError = TRUE;
		EOnlineServerConnectionStatus ConnectionStatus = OSCS_NoNetworkConnection;
		// Map the GameSpy code to ours
		switch (Arg->errorCode)
		{
			case GP_NETWORK:
				ConnectionStatus = OSCS_NoNetworkConnection;
				break;
			case GP_FORCED_DISCONNECT:
				ConnectionStatus = OSCS_DuplicateLoginDetected;
				break;
			case GP_DATABASE:
			case GP_CONNECTION_CLOSED:
				ConnectionStatus = OSCS_ServiceUnavailable;
				break;
			default:
				bIsConnError = FALSE;
				break;
		}
		if (bIsConnError)
		{
			OnlineSubsystemGameSpy_eventOnConnectionStatusChange_Parms Parms(EC_EventParm);
			Parms.ConnectionStatus = ConnectionStatus;
			TriggerOnlineDelegates(this,ConnectionStatusChangeDelegates,&Parms);
			// Disconnect the player
			gpDisconnect(&GPHandle);
			debugf(NAME_DevOnline,
				TEXT("gpDisconnect() called due to error callback with code (0x%08X)"),
				(DWORD)Arg->errorCode);
			ClearPlayerInfo();
			// Trigger the delegates so the UI can process
			OnlineSubsystemGameSpy_eventOnLoginChange_Parms Parms2(EC_EventParm);
			Parms2.LocalUserNum = 0;
			TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms2);
		}
	}
}

/**
 * Called when a buddy request is received
 *
 * @param Arg the request info
 */
void UOnlineSubsystemGameSpy::GPRecvBuddyRequestCallback(GPRecvBuddyRequestArg* Arg)
{
#if PS3
	// Hack to auto add PS3 XMB friends
	if (appStrstr((const TCHAR*)Arg->reason,TEXT("PS3 Buddy Sync")))
	{
		GPResult Result = gpAuthBuddyRequest(&GPHandle,Arg->profile);
		debugf(NAME_DevOnline,TEXT("Auto accept buddy sync: gpAuthBuddyRequest(%d) returned 0x%08X"),
			Arg->profile,(DWORD)Result);
		return;
	}
#endif
	// Add the invite to the list of cached ones for retrieval later
	INT AddIndex = CachedFriendMessages.AddZeroed();
	FOnlineFriendMessage& Invite = CachedFriendMessages(AddIndex);
	Invite.Message = (const TCHAR*)Arg->reason;
	Invite.SendingPlayerId = (DWORD)Arg->profile;
	Invite.bIsFriendInvite = TRUE;
	Invite.bWasAccepted = FALSE;
	// Create an async task to handle the callback
	FOnlineAsyncTaskGameSpyGetPlayerNick* AsyncTask = new FOnlineAsyncTaskGameSpyGetPlayerNick(AddIndex);
	GPResult Result = gpGetInfo(&GPHandle,
		Arg->profile,
		GP_CHECK_CACHE,
		GP_NON_BLOCKING,
		(GPCallback)AsyncTask->GPGetInfoCallback,
		AsyncTask);
	debugf(NAME_DevOnline,TEXT("Query for friend invite sender name returned 0x%08X"),(DWORD)Result);
	if (Result == GP_NO_ERROR)
	{
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		delete AsyncTask;
	}
}

/**
 * Called when a buddy's status is received
 *
 * @param Arg the status info
 */
void UOnlineSubsystemGameSpy::GPRecvBuddyStatusCallback(GPRecvBuddyStatusArg* Arg)
{
	// do a get info
	// we can ignore the callback, we just want to cache the info
	gpGetInfo(&GPHandle, Arg->profile, GP_CHECK_CACHE, GP_NON_BLOCKING, (GPCallback)::GPGetInfoCallback, this);
}

/**
 * Called when a buddy message is received
 *
 * @param Arg the message info
 */
void UOnlineSubsystemGameSpy::GPRecvBuddyMessageCallback(GPRecvBuddyMessageArg* Arg)
{
	// Add the message to the list of cached ones for retrieval later
	INT AddIndex = CachedFriendMessages.AddZeroed();
	FOnlineFriendMessage& Message = CachedFriendMessages(AddIndex);
	Message.Message = (const TCHAR*)Arg->message;
	Message.SendingPlayerId = (DWORD)Arg->profile;
	// Create an async task to handle the callback
	FOnlineAsyncTaskGameSpyGetPlayerNick* AsyncTask = new FOnlineAsyncTaskGameSpyGetPlayerNick(AddIndex);
	GPResult Result = gpGetInfo(&GPHandle,
		Arg->profile,
		GP_CHECK_CACHE,
		GP_NON_BLOCKING,
		(GPCallback)AsyncTask->GPGetInfoCallback,
		AsyncTask);
	debugf(NAME_DevOnline,TEXT("Query for friend message sender name returned 0x%08X"),(DWORD)Result);
	if (Result == GP_NO_ERROR)
	{
		AsyncTasks.AddItem(AsyncTask);
	}
	else
	{
		delete AsyncTask;
	}
}

/**
 * Called when a buddy request has been authorized
 *
 * @param Arg the auth info
 */
void UOnlineSubsystemGameSpy::GPRecvBuddyAuthCallback(GPRecvBuddyAuthArg* Arg)
{
}

/**
 * Called when a buddy has revoked your friendship
 *
 * @param Arg the revoke info
 */
void UOnlineSubsystemGameSpy::GPRecvBuddyRevokeCallback(GPRecvBuddyRevokeArg* Arg)
{
	debugf(NAME_DevOnline,TEXT("Friend (0x%08X) removed us, auto deleting"),
		Arg->profile);
	FUniqueNetId Id = (DWORD)Arg->profile;
	RemoveFriend(LoggedInPlayerNum,Id);
}

/**
 * Called at the end of a connection attempt
 *
 * @param Arg the connection result
 */
void UOnlineSubsystemGameSpy::GPConnectCallback(GPConnectResponseArg* Arg)
{
	bIsLoginInProcess = FALSE;
	ClearPlayerInfo();
	debugf(NAME_DevOnline,TEXT("ConnectResponseArg->result = 0x%08X"),(DWORD)Arg->result);
	if (Arg->result == GP_NO_ERROR)
	{
		// Copy over logged in information
		LoggedInPlayerName = (wchar_t*)Arg->uniquenick;
		LoggedInPlayerId = (DWORD)Arg->profile;
		LoggedInStatus = LS_LoggedIn;
		// Get the login ticket for stats
		char LoginTicket[GP_LOGIN_TICKET_LEN];
		GPResult Result = gpGetLoginTicket(&GPHandle,LoginTicket);
		debugf(NAME_DevOnline,TEXT("gpGetLoginTicket() returned 0x%08X"),(DWORD)Result);
		if (Result == GP_NO_ERROR)
		{
			// Don't expose information to people
			gpSetInfoMask(&GPHandle,GP_MASK_NONE);
			sakeSetProfile(SakeHandle,Arg->profile,LoginTicket);
			// Set the default status setting
			SetOnlineStatus(LoggedInPlayerNum,
				0,
				TArray<FLocalizedStringSetting>(),
				TArray<FSettingsProperty>());
#if PS3
			// Use the remote auth feature to complete the login
			WSLoginValue LoginResult = (WSLoginValue)wsLoginRemoteAuth(GetPartnerId(),
				GetNamespaceId(),
				FTCHARToANSI(*RemoteAuthToken),
				FTCHARToANSI(*RemoteAuthPartnerChallenge),
				::LoginCertificateCallback,
				this);
			debugf(NAME_DevOnline,TEXT("wsLoginRemoteAuth() returned 0x%08X"),(DWORD)LoginResult);
#else
			// Request a login certificate
			WSLoginValue LoginResult = (WSLoginValue)wsLoginUnique(GetPartnerId(),
				GetNamespaceId(),
				Arg->uniquenick,
				(const UCS2String)*LoggedInPlayerPassword,
				NULL,
				::LoginCertificateCallback,
				this);
			debugf(NAME_DevOnline,TEXT("wsLoginUnique() returned 0x%08X"),(DWORD)LoginResult);
#endif
			if (LoginResult != WSLogin_Success)
			{
				// handle error
				Result = (GPResult)E_FAIL;
			}
		}
#if !PS3
		bHasGameSpyAccount = TRUE;
		SaveConfig();
#endif
		// Trigger the delegates so the UI can process
		OnlineSubsystemGameSpy_eventOnLoginChange_Parms Parms2(EC_EventParm);
		Parms2.LocalUserNum = 0;
		TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms2);
	}
	else
	{
		GPErrorCode ErrorCode;
		// Get the real error code (result != error code)
		gpGetErrorCode(&GPHandle,&ErrorCode);
		FLoginFailedParms Params(LoggedInPlayerNum,ErrorCode);
		TriggerOnlineDelegates(this,LoginFailedDelegates,&Params);
	}
	// Clear the password
	LoggedInPlayerPassword.Empty();
}

/**
 * Called at the end of a new user attempt
 *
 * @param Arg the new user result
 */
void UOnlineSubsystemGameSpy::GPNewUserCallback(GPNewUserResponseArg* Arg)
{
	debugf(NAME_DevOnline,TEXT("New user create result %d"),(DWORD)Arg->result);
	// Trigger the delegates so the UI can process
	FAccountCreateResults Params((DWORD)Arg->result);
	TriggerOnlineDelegates(this,AccountCreateDelegates,&Params);
#if !PS3
	if (Arg->result == GP_NO_ERROR)
	{
		// Mark the account as created
		bHasGameSpyAccount = TRUE;
		// Save the config for this data
		SaveConfig();
	}
#endif
}

/**
 * Called in response to a request for a remote profile's info
 *
 * @param Connection the GP connection
 * @param Arg the profile's info
 */
void UOnlineSubsystemGameSpy::GPGetInfoCallback(GPGetInfoResponseArg* Arg)
{
	FAsyncTaskDelegateResults Params(S_OK);
	TriggerOnlineDelegates(this,ReadFriendsDelegates,&Params);
}

/**
 * Finish reading the stats
 */
void UOnlineSubsystemGameSpy::TryToCompleteReadOnlineStatsRequest(void)
{
	if (CurrentStatsRead != NULL)
	{
		//TODO: how do we handle errors in requesting info from the backend?
		//      we might have an error, but still need to wait for pending requests to finish
		//      we don't want to clean everything up, then have another callback attempt to access this or a separate request
		// Check if we have the record count
		if (CurrentStatsRead->TotalRowsInView == -1)
		{
			return;
		}
		for (INT Index = 0; Index < CurrentStatsRead->Rows.Num(); Index++)
		{
			if (CurrentStatsRead->Rows(Index).NickName.Len() == 0)
			{
				return;
			}
		}
		DWORD Result = S_OK;
		CurrentStatsRead = NULL;
		FAsyncTaskDelegateResults Results(Result);
		TriggerOnlineDelegates(this,ReadOnlineStatsCompleteDelegates,&Results);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Stats read object is NULL!"));
	}
}

/**
 * Called in response to a request for a stats player's nick
 *
 * @param Arg the player's info
 */
void UOnlineSubsystemGameSpy::GPGetStatsNickCallback(GPGetInfoResponseArg* Arg)
{
	if (CurrentStatsRead != NULL && Arg->result == GP_NO_ERROR)
	{
		// Find the player
		for (INT Index = 0; Index < CurrentStatsRead->Rows.Num(); Index++)
		{
			FOnlineStatsRow& Row = CurrentStatsRead->Rows(Index);
			if (Row.PlayerID.ToDWORD() == Arg->profile)
			{
				if((Arg->uniquenick != NULL) && (Arg->uniquenick[0] != '\0'))
				{
					Row.NickName = (const TCHAR*)Arg->uniquenick;
				}
				else
				{
					//TODO: this player doesn't have a uniquenick in this system, do you want to call it out somehow?
					Row.NickName = (const TCHAR*)Arg->nick;
				}
				break;
			}
		}
		// Finish up the request if it is complete
		TryToCompleteReadOnlineStatsRequest();
	}
	else
	{
		//TODO: handle error
	}
}

/**
 * Called after requesting the profile from the Sake DB
 *
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 */
void UOnlineSubsystemGameSpy::SakeRequestProfileCallback(SAKERequest Request, SAKERequestResult Result, SAKEGetMyRecordsInput* Input, SAKEGetMyRecordsOutput* Output)
{
	DWORD Return = E_FAIL;
	if (Result == SAKERequestResult_SUCCESS)
	{
		if(Output->mNumRecords <= 0)
		{
			// There's no existing record, so we need to create one
			debugf(NAME_DevOnline,
				TEXT("Profile for %s doesn't exist, using defaults"),
				*LoggedInPlayerName);
			// Only clear if there wasn't a profile loaded from disk
			if (CachedProfile->ProfileSettings.Num() == 0)
			{
				CachedProfile->eventSetToDefaults();
			}
			// Make sure the profile settings have a version number
			CachedProfile->AppendVersionToSettings();
			// Update the save count for roaming profile support
			UOnlinePlayerStorage::SetProfileSaveCount(UOnlinePlayerStorage::GetProfileSaveCount(CachedProfile->ProfileSettings, PSI_ProfileSaveCount) + 1,CachedProfile->ProfileSettings, PSI_ProfileSaveCount);
			// Used to write the profile settings into a blob
			FProfileSettingsWriter Writer(3000,TRUE);
			if (Writer.SerializeToBuffer(CachedProfile->ProfileSettings))
			{
				// Create the profile in Sake
				SakeCreateProfile(Writer.GetFinalBuffer(),Writer.GetFinalBufferLength());
				Return = ERROR_IO_PENDING;
			}
			if (Return != S_OK && Return != ERROR_IO_PENDING)
			{
				debugf(NAME_DevOnline,TEXT("Failed to create profile record for %s"),*LoggedInPlayerName);
			}
		}
		else
		{
			// Store the profile's recordid so we can update it later
			SAKEField* RecordIDField = sakeGetFieldByName("recordid", Output->mRecords[0], Input->mNumFields);
			SakeProfileRecordID = RecordIDField->mValue.mInt;
			// Handle the profile stored on the backend
			SAKEField* ProfileField = sakeGetFieldByName("profile", Output->mRecords[0], Input->mNumFields);
			FProfileSettingsReader Reader(3000,TRUE,(const BYTE*)ProfileField->mValue.mBinaryData.mValue,(DWORD)ProfileField->mValue.mBinaryData.mLength);
			TArray<FOnlineProfileSetting> ProfileSettings;
			// Serialize the profile from the buffer
			if (Reader.SerializeFromBuffer(ProfileSettings))
			{
				// Check to see which is more recent and use that set
				INT SakeSaveCount = UOnlinePlayerStorage::GetProfileSaveCount(ProfileSettings, PSI_ProfileSaveCount);
				INT LocalSaveCount = UOnlinePlayerStorage::GetProfileSaveCount(CachedProfile->ProfileSettings, PSI_ProfileSaveCount);
				// If the SAKE profile data is newer, then overwrite the local data
				if (SakeSaveCount > LocalSaveCount)
				{
					CachedProfile->ProfileSettings = ProfileSettings;
				}
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
				CachedProfile->AsyncState = OPAS_Finished;
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
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Error requesting profile from Sake for %s, using local profile"),
			*LoggedInPlayerName);
		Return = S_OK;
	}
	if (Return != ERROR_IO_PENDING)
	{
		// Remove the async state so that subsequent read/writes work
		CachedProfile->AsyncState = OPAS_Finished;
		OnlineSubsystemGameSpy_eventOnReadProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (Return == 0) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = LoggedInPlayerNum;
		// Use the common method to do the work
		TriggerOnlineDelegates(this,PerUserReadProfileSettings[LoggedInPlayerNum].Delegates,&Parms);
	}
	// We allocated the Input, so free it
	// This needs to be delayed because their code accesses it after here (though it shouldn't)
	AsyncTasks.AddItem(new FDelayedDeletionSake(Input));
}

/**
 * Called at the end of a profile search
 *
 * @param Arg the new user result
 */
void UOnlineSubsystemGameSpy::GPProfileSearchCallback(GPProfileSearchResponseArg* Arg)
{
	DWORD Result = E_FAIL;
	// If the search was successful, send them a buddy invite
	if (Arg->result == GP_NO_ERROR && Arg->numMatches > 0)
	{
		// Only send the invite if they aren't on the friend's list already
		if (gpIsBuddy(&GPHandle,Arg->matches[0].profile) == FALSE)
		{
			GPResult gpResult = gpSendBuddyRequest(&GPHandle,
				Arg->matches[0].profile,
				(const UCS2String)*CachedFriendMessage);
			debugf(NAME_DevOnline,TEXT("Buddy found sending invite..."));
			debugf(NAME_DevOnline,TEXT("gpSendBuddyRequest(,%d,) returned 0x%08X"),
				Arg->matches[0].profile,
				(DWORD)gpResult);
			Result = (DWORD)gpResult;
		}
		else
		{
			// Already on the friends list so treat it as successful
			Result = S_OK;
		}
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Couldn't find buddy to add to the buddy list (0x%08X) num matches (%d)"),
			(DWORD)Arg->result,
			Arg->numMatches);
	}
	// Fire the delegates
	FAsyncTaskDelegateResults Params(Result);
	TriggerOnlineDelegates(this,AddFriendByNameCompleteDelegates,&Params);
}

/**
 * Called when a game invite was received
 *
 * @param Arg the game invite data
 */
void UOnlineSubsystemGameSpy::GPRecvGameInviteCallback(GPRecvGameInviteArg* Arg)
{
	if (IsJoinableLocationString((const TCHAR*)Arg->location) &&
		gpIsBuddy(&GPHandle,Arg->profile))
	{
		// Add the invite to the list of cached ones for retrieval later
		INT AddIndex = CachedFriendMessages.AddZeroed();
		FOnlineFriendMessage& Invite = CachedFriendMessages(AddIndex);
		Invite.Message = GameInviteMessage;
		Invite.SendingPlayerId = (DWORD)Arg->profile;
		Invite.bIsGameInvite = TRUE;
		// Create an async task to handle the callback
		FOnlineAsyncTaskGameSpyGetPlayerNick* AsyncTask = new FOnlineAsyncTaskGameSpyGetPlayerNick(AddIndex);
		GPResult Result = gpGetInfo(&GPHandle,
			Arg->profile,
			GP_CHECK_CACHE,
			GP_NON_BLOCKING,
			(GPCallback)AsyncTask->GPGetInfoCallback,
			AsyncTask);
		debugf(NAME_DevOnline,TEXT("Query for game invite sender name returned 0x%08X"),(DWORD)Result);
		if (Result == GP_NO_ERROR)
		{
			AsyncTasks.AddItem(AsyncTask);
		}
		else
		{
			delete AsyncTask;
		}
	}
}

/**
 * Called after creating a profile record in the Sake DB
 *
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 */
void UOnlineSubsystemGameSpy::SakeCreateProfileCallback(SAKERequest Request, SAKERequestResult Result, SAKECreateRecordInput* Input, SAKECreateRecordOutput* Output)
{
	DWORD Return = E_FAIL;
	if (Result == SAKERequestResult_SUCCESS)
	{
		// Store the profile's recordid so we can update it later
		SakeProfileRecordID = Output->mRecordId;
		Return = S_OK;
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Failed to create Sake profike record for %s"),
			*LoggedInPlayerName);
		Return = E_FAIL;
	}
	// Profile is no longer being written to
	CachedProfile->AsyncState = OPAS_Finished;
	OnlineSubsystemGameSpy_eventOnReadProfileSettingsComplete_Parms Parms(EC_EventParm);
	Parms.bWasSuccessful = (Return == 0) ? FIRST_BITFIELD : 0;
	Parms.LocalUserNum = LoggedInPlayerNum;
	// Use the common method to do the work
	TriggerOnlineDelegates(this,PerUserReadProfileSettings[LoggedInPlayerNum].Delegates,&Parms);
	// We allocated the Input, so free it
	delete Input;
}

/**
 * Called after updating the profile stored in the Sake DB
 *
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 */
void UOnlineSubsystemGameSpy::SakeUpdateProfileCallback(SAKERequest Request, SAKERequestResult Result, SAKEUpdateRecordInput* Input)
{
	// We allocated the Input, so free it
	delete Input;
	// Remove the write state so that subsequent writes work
	CachedProfile->AsyncState = OPAS_Finished;
	// Pass in the data that indicates whether the call worked or not
	OnlineSubsystemGameSpy_eventOnWriteProfileSettingsComplete_Parms Parms(EC_EventParm);
	Parms.bWasSuccessful = (Result == SAKERequestResult_SUCCESS) ? FIRST_BITFIELD : 0;
	Parms.LocalUserNum = LoggedInPlayerNum;
	// Use the common method to do the work
	TriggerOnlineDelegates(this,WriteProfileSettingsDelegates,&Parms);
}

/**
 * Called after searching for records in the Sake DB
 *
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 */
void UOnlineSubsystemGameSpy::SakeSearchForRecordsCallback(SAKERequest Request, SAKERequestResult Result, SAKESearchForRecordsInput* Input, SAKESearchForRecordsOutput* Output)
{
	DWORD Return = E_FAIL;
	if (CurrentStatsRead != NULL && Result == SAKERequestResult_SUCCESS)
	{
		CurrentStatsRead->TotalRowsInView = bShouldStatsReadsRequestTotals ? -1 : Output->mNumRecords;
		// Now copy each row that was returned
		for (INT RecordIndex = 0; RecordIndex < Output->mNumRecords; RecordIndex++)
		{
			INT NewIndex = CurrentStatsRead->Rows.AddZeroed();
			FOnlineStatsRow& Row = CurrentStatsRead->Rows(NewIndex);
			// Get the record (array of fields)
			SAKEField* Record = Output->mRecords[RecordIndex];
			// Set the PlayerID
			SAKEField* OwnerIdField = sakeGetFieldByName("ownerid", Record, Input->mNumFields);
			check((OwnerIdField != NULL) && (OwnerIdField->mType == SAKEFieldType_INT));
			Row.PlayerID = (DWORD)OwnerIdField->mValue.mInt;
			// Set the Rank
			SAKEField* RowField = sakeGetFieldByName("row", Record, Input->mNumFields);
			check((RowField != NULL) && (RowField->mType == SAKEFieldType_INT));
			Row.Rank.SetData((INT)RowField->mValue.mInt);
			// Set the NickName
			SAKEField* NickField = sakeGetFieldByName("Nick", Record, Input->mNumFields);
			check((NickField != NULL) && (NickField->mType == SAKEFieldType_ASCII_STRING));
			Row.NickName = NickField->mValue.mAsciiString;
			// Setup the columns
			INT NumColumns = CurrentStatsRead->ColumnIds.Num();
			Row.Columns.AddZeroed(NumColumns);
			// And copy the fields into the columns
			for (INT FieldIndex = 0; FieldIndex < NumColumns; FieldIndex++)
			{
				SAKEField* Field = &Record[FieldIndex];
				FOnlineStatsColumn& Col = Row.Columns(FieldIndex);
				Col.ColumnNo = CurrentStatsRead->ColumnIds(FieldIndex);
				switch(Field->mType)
				{
					case SAKEFieldType_BYTE:
						Col.StatValue.SetData((INT)Field->mValue.mByte);
						break;
					case SAKEFieldType_SHORT:
						Col.StatValue.SetData((INT)Field->mValue.mShort);
						break;
					case SAKEFieldType_INT:
						Col.StatValue.SetData((INT)Field->mValue.mInt);
						break;
					case SAKEFieldType_FLOAT:
						Col.StatValue.SetData((FLOAT)Field->mValue.mFloat);
						break;
					case SAKEFieldType_ASCII_STRING:
						Col.StatValue.SetData(FString(Field->mValue.mAsciiString));
						break;
					case SAKEFieldType_UNICODE_STRING:
						Col.StatValue.SetData(FString((wchar_t*)Field->mValue.mUnicodeString));
						break;
					default:
						// Unsupported type
						Col.StatValue.SetData(TEXT("--"));
						break;
				}
			}
		}
		// We need to figure out which Players don't have stats, then add blank data and request nicks for them
		for (INT OwnerIndex = 0; OwnerIndex < Input->mNumOwnerIds; OwnerIndex++)
		{
			INT ProfileId = Input->mOwnerIds[OwnerIndex];
			UBOOL bFoundRecord = FALSE;
			for (INT RecordIndex = 0; RecordIndex < Output->mNumRecords; RecordIndex++)
			{
				SAKEField* OwnerIdField = sakeGetFieldByName("ownerid", Output->mRecords[RecordIndex], Input->mNumFields);
				if(OwnerIdField != NULL &&
					OwnerIdField->mType == SAKEFieldType_INT &&
					OwnerIdField->mValue.mInt == ProfileId)
				{
					bFoundRecord = TRUE;
					break;
				}
			}
			if (!bFoundRecord)
			{
				// Add blank data
				//TODO: can we merge some of this with the above to avoid repeating code?
				INT NewIndex = CurrentStatsRead->Rows.AddZeroed();
				FOnlineStatsRow& Row = CurrentStatsRead->Rows(NewIndex);
				Row.PlayerID = (DWORD)ProfileId;
				Row.Rank.SetData(TEXT("--"));
				// Fill the columns
				INT NumColumns = CurrentStatsRead->ColumnIds.Num();
				Row.Columns.AddZeroed(NumColumns);
				// And copy the fields into the columns
				for (INT FieldIndex = 0; FieldIndex < NumColumns; FieldIndex++)
				{
					FOnlineStatsColumn& Col = Row.Columns(FieldIndex);
					Col.ColumnNo = CurrentStatsRead->ColumnIds(FieldIndex);
					Col.StatValue.SetData(TEXT("--"));
				}
				// Request nick
				gpGetInfo(&GPHandle, ProfileId, GP_CHECK_CACHE, GP_NON_BLOCKING, (GPCallback)::GPGetStatsNickCallback, this);
			}
		}
		Return = S_OK;
	}
	else
	{
		//TODO: handle error
		debugf(NAME_DevOnline, TEXT("Error connecting to Gamespy SAKE Server :%d"), (int)Result);
	}
	if (Return == S_OK)
	{
		if(bShouldStatsReadsRequestTotals)
		{
			// Setup the Sake row count structure
			SAKEGetRecordCountInput* RecordCountInput = new SAKEGetRecordCountInput;
			appMemset(RecordCountInput, 0, sizeof(SAKEGetRecordCountInput));
			RecordCountInput->mTableId = Input->mTableId;
			RecordCountInput->mFilter = Input->mFilter;
			SAKERequest RecordCountRequest = sakeGetRecordCount(SakeHandle, RecordCountInput, (SAKERequestCallback)::SakeGetRecordCountCallback, this);
			if((RecordCountRequest == NULL) || (sakeGetStartRequestResult(SakeHandle) != SAKEStartRequestResult_SUCCESS))
			{
				//TODO: handle error
				Return = E_FAIL;
			}
			else
			{
				// Set these to NULL so that don't get freed with the rest of the input memory
				Input->mTableId = NULL;
				Input->mFilter = NULL;
			}
		}
		else
		{
			// Finish up the request if it is complete
			TryToCompleteReadOnlineStatsRequest();
		}
	}
	if (Return == E_FAIL)
	{
		CurrentStatsRead = NULL;
		FAsyncTaskDelegateResults Results(Return);
		TriggerOnlineDelegates(this,ReadOnlineStatsCompleteDelegates,&Results);
	}
	// We allocated the Input, so free it and zero out items to prevent accidental use
	delete[] Input->mTableId;
	Input->mTableId = NULL;
	for (INT Index = 0; Index < Input->mNumFields; Index++)
	{
		delete Input->mFieldNames[Index];
		Input->mFieldNames[Index] = NULL;
	}
	Input->mNumFields = 0;
	delete [] Input->mFieldNames;
	Input->mFieldNames = NULL;
	delete [] Input->mFilter;
	Input->mFilter = NULL;
	delete [] Input->mSort;
	Input->mSort = NULL;
	delete [] Input->mTargetRecordFilter;
	Input->mTargetRecordFilter = NULL;
	delete [] Input->mOwnerIds;
	Input->mOwnerIds = NULL;
	AsyncTasks.AddItem(new FDelayedDeletionSakeStatsRead(Input));
}

/**
 * Called after requesting a Sake table record count
 *
 * @param Request a handle to the request
 * @param Result the result of the request
 * @param Input the data that was sent along with the request
 * @param Output the data that was returned with the response
 */
void UOnlineSubsystemGameSpy::SakeGetRecordCountCallback(SAKERequest Request, SAKERequestResult Result, SAKEGetRecordCountInput* Input, SAKEGetRecordCountOutput* Output)
{
	if (CurrentStatsRead != NULL && Result == SAKERequestResult_SUCCESS)
	{
		CurrentStatsRead->TotalRowsInView = Output->mCount;
	}
	else
	{
		//TODO: handle error
	}
	// Finish up the request if it is complete
	TryToCompleteReadOnlineStatsRequest();
	delete[] Input->mTableId;
	Input->mTableId = NULL;
	delete[] Input->mFilter;
	Input->mFilter = NULL;
	AsyncTasks.AddItem(new FDelayedDeletionSakeStatsRecordCount(Input));
}

/**
 * Called after a login certificate request
 *
 * @param HttpResult the result of the http request
 * @param Response the response to the request
 */
void UOnlineSubsystemGameSpy::LoginCertificateCallback(GHTTPResult HttpResult, WSLoginResponse * Response)
{
	if (HttpResult == GHTTPSuccess && Response->mLoginResult == WSLogin_Success)
	{
		// Store a copy of the login cert and private data
		LoginCertificate = new GSLoginCertificate(Response->mCertificate);
		LoginPrivateData = new GSLoginPrivateData(Response->mPrivateData);
	}
	else
	{
		//TODO: handle error
		// we don't have a login certificate, so we can't report stats
	}
}

/**
 * Called after a PS3 login certificate request
 *
 * @param HttpResult the result of the http request
 * @param Response the response to the request
 */
void UOnlineSubsystemGameSpy::PS3LoginCertificateCallback(GHTTPResult HttpResult, WSLoginPs3CertResponse* Response)
{
	debugf(NAME_DevOnline,
		TEXT("PS3LoginCertificateCallback: HttpResult = 0x%08X, LoginResult = 0x%08X, LoginResponse = 0x%08X"),
		(DWORD)HttpResult,
		(DWORD)Response->mLoginResult,
		(DWORD)Response->mResponseCode);
	if (HttpResult == GHTTPSuccess && Response->mLoginResult == WSLogin_Success)
	{
		RemoteAuthToken = Response->mRemoteAuthToken;
		RemoteAuthPartnerChallenge = Response->mPartnerChallenge;
		// Connect
		GPResult Result = gpConnectPreAuthenticated(&GPHandle,
			(UCS2String)*RemoteAuthToken,
			(UCS2String)*RemoteAuthPartnerChallenge,
			GP_NO_FIREWALL,
			GP_NON_BLOCKING,
			(GPCallback)::GPConnectCallback,
			this);
		debugf(NAME_DevOnline,TEXT("gpConnectPreAuthenticated() returned 0x%08X"),(DWORD)Result);
		if (Result != GP_NO_ERROR)
		{
			ClearPlayerInfo();
		}
	}
	else
	{
#if PS3
		// If there is a NP account try again
		if (NpData->RemoteAuthTicketLength > 0)
		{
			AutoLogin();
		}
#endif
	}
}

/**
 * Called after an attempt to create a session
 *
 * @param HttpResult the result of the http request
 * @param Result the result of the create session attempt
 */
void UOnlineSubsystemGameSpy::CreateStatsSessionCallback(GHTTPResult HttpResult, SCResult Result)
{
	DWORD Return = E_FAIL;
	debugf(NAME_DevOnline,
		TEXT("CreateStatsSessionCallback: HttpResult = 0x%08X, Result = 0x%08X"),
		(DWORD)HttpResult,
		(DWORD)Result);
	if (HttpResult == GHTTPSuccess && Result == SCResult_NO_ERROR)
	{
		bIsStatsSessionOk = TRUE;
		// The host needs to set his report intention
		HostSetStatsReportIntention();
		Return = S_OK;
	}
	FAsyncTaskDelegateResults Params(Return);
	TriggerOnlineDelegates(GameInterfaceImpl,GameInterfaceImpl->StartOnlineGameCompleteDelegates,&Params);
}

/**
 * Called after the host sets his report intention
 *
 * @param HttpResult the result of the http request
 * @param Result the result of the set report intention
 */
void UOnlineSubsystemGameSpy::HostSetStatsReportIntentionCallback(GHTTPResult HttpResult, SCResult Result)
{
	debugf(NAME_DevOnline,
		TEXT("HostSetStatsReportIntentionCallback: HttpResult = 0x%08X, Result = 0x%08X"),
		(DWORD)HttpResult,
		(DWORD)Result);
	if (HttpResult == GHTTPSuccess && Result == SCResult_NO_ERROR)
	{
		// completed successfully
	}
	else
	{
		//TODO: handle error
	}
}

/**
 * Called after the client sets his report intention
 *
 * @param HttpResult the result of the http request
 * @param Result the result of the set report intention
 */
void UOnlineSubsystemGameSpy::ClientSetStatsReportIntentionCallback(GHTTPResult HttpResult, SCResult Result)
{
	DWORD Return = E_FAIL;
	debugf(NAME_DevOnline,
		TEXT("ClientSetStatsReportIntentionCallback: HttpResult = 0x%08X, Result = 0x%08X"),
		(DWORD)HttpResult,
		(DWORD)Result);
	if (HttpResult == GHTTPSuccess && Result == SCResult_NO_ERROR)
	{
		// completed successfully
		Return = S_OK;
	}
	else
	{
		//TODO: handle error
	}
	FAsyncTaskDelegateResults Params(Return);
	TriggerOnlineDelegates(this,RegisterHostStatGuidCompleteDelegates,&Params);
}

/**
 * Called after the host submits a report
 *
 * @param HttpResult the result of the http request
 * @param Result the result of the report submission
 */
void UOnlineSubsystemGameSpy::SubmitStatsReportCallback(GHTTPResult HttpResult, SCResult Result)
{
	bIsStatsSessionOk = FALSE;
	DWORD Return = E_FAIL;
	debugf(NAME_DevOnline,
		TEXT("SubmitStatsReportCallback: HttpResult = 0x%08X, Result = 0x%08X"),
		(DWORD)HttpResult,
		(DWORD)Result);
	if (HttpResult == GHTTPSuccess && Result == SCResult_NO_ERROR)
	{
		// completed successfully
		Return = S_OK;
	}
	FAsyncTaskDelegateResults Params(Return);
	TriggerOnlineDelegates(this,FlushOnlineStatsDelegates,&Params);
}

/**
 * PC specific implementation. Sets the supported interface pointers and
 * initilizes the voice engine
 *
 * @return always returns TRUE
 */
UBOOL UOnlineSubsystemGameSpy::Init(void)
{
	Super::Init();
	// Set the interface used for account creation
	eventSetAccountInterface(this);
	// Set the player interface to be the same as the object
	eventSetPlayerInterface(this);
	// Set the extended player interface to be the same as the object
	eventSetPlayerInterfaceEx(this);

	// Create the voice engine and if successful register the interface
	VoiceEngine = appCreateVoiceInterface(MaxLocalTalkers,MaxRemoteTalkers,
		bIsUsingSpeechRecognition);
	// Set the voice interface to this object
	eventSetVoiceInterface(this);

	// Construct the object that handles the game interface for us
	CachedGameInt = ConstructObject<UOnlineGameInterfaceGameSpy>(UOnlineGameInterfaceGameSpy::StaticClass(),this);
	GameInterfaceImpl = CachedGameInt;
	if (GameInterfaceImpl)
	{
		GameInterfaceImpl->OwningSubsystem = this;
		// Set the game interface to be the same as the object
		eventSetGameInterface(GameInterfaceImpl);
	}
	// Set the stats reading/writing interface
	eventSetStatsInterface(this);
	eventSetSystemInterface(this);
	// Create the voice engine and if successful register the interface
	VoiceEngine = appCreateVoiceInterface(MaxLocalTalkers,MaxRemoteTalkers,	bIsUsingSpeechRecognition);
	if (VoiceEngine != NULL)
	{
		// Set the voice interface to this object
		eventSetVoiceInterface(this);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Failed to create the voice interface"));
	}
	// Use MCP services for news or not
	if (bShouldUseMcp)
	{
		UOnlineNewsInterfaceMcp* NewsObject = ConstructObject<UOnlineNewsInterfaceMcp>(UOnlineNewsInterfaceMcp::StaticClass(),this);
		eventSetNewsInterface(NewsObject);
	}
	// Handle a missing profile data directory specification
	if (ProfileDataDirectory.Len() == 0)
	{
		ProfileDataDirectory = TEXT(".\\");
	}
	// Set the initial state so we don't trigger an event unnecessarily
	bLastHasConnection = GSocketSubsystem->HasNetworkDevice();
	// Set to the localized default
	LoggedInPlayerName = LocalProfileName;
	LoggedInPlayerNum = -1;
#if PS3
	// Initialize the PS3 NP items for single sign on support
	InitNp();
#endif
	// Finally do the GameSpy initialization
	if (InitGameSpy())
	{
		// Cache the pointer used with auth
		GOnlineSubsystemGameSpy = this;
		// Sign in locally to the default account
		SignInLocally();
	}
	return GameInterfaceImpl != NULL;
}

/** Initializes GameSpy */
UBOOL UOnlineSubsystemGameSpy::InitGameSpy(void)
{
	debugf(NAME_DevOnline,TEXT("Initializing GameSpy"));
	UBOOL bWasInited = FALSE;
	// Have GameSpy use our mallocs
	gsiMemoryCallbacksSet(CallbackMalloc,CallbackFree,CallbackRealloc,CallbackAlignedMalloc);
	// Do the availability check
	GSIACResult Result;
	GSIStartAvailableCheck((const UCS2String)appGetGameSpyGameName());
	while((Result = GSIAvailableCheckThink()) == GSIACWaiting)
	{
		msleep(5);
	}
	debugf(NAME_DevOnline,TEXT("GameSpy availability check returned 0x%08X"),(DWORD)Result);
	if (Result == GSIACAvailable)
	{
		// Initialize GP (note the parnter id is important for PS3)
		GPResult gpResult = gpInitialize(&GPHandle,
			ProductID,
			GetNamespaceId(),
			GetPartnerId());
		debugf(NAME_DevOnline,TEXT("gpInitialize() returned 0x%08X"),(DWORD)gpResult);
		if (gpResult == GP_NO_ERROR)
		{
#if PS3 && GP_NP_BUDDY_SYNC
			// GP needs the communication id to do an NP Buddy sync.
			gpSetNpCommunicationId(&GPHandle, &GPS3NetworkPlatform->GetCommunicationId());
			
			// GP needs to be aware of NP friends list changes and therefore needs a sysutil slot.
			// NOTE: for developers that need a slot, please disable any unreal engine slots that 
			// may not be necessary or we can work to provide a function to initiate syncing
			gpRegisterCellSysUtilCallbackSlot(&GPHandle, 3);
#endif
			// Setup callbacks
			gpSetCallback(&GPHandle, GP_ERROR, (GPCallback)::GPErrorCallback, this);
			gpSetCallback(&GPHandle, GP_RECV_BUDDY_REQUEST, (GPCallback)::GPRecvBuddyRequestCallback, this);
			gpSetCallback(&GPHandle, GP_RECV_BUDDY_STATUS, (GPCallback)::GPRecvBuddyStatusCallback, this);
			gpSetCallback(&GPHandle, GP_RECV_BUDDY_MESSAGE, (GPCallback)::GPRecvBuddyMessageCallback, this);
			gpSetCallback(&GPHandle, GP_RECV_BUDDY_AUTH, (GPCallback)::GPRecvBuddyAuthCallback, this);
			gpSetCallback(&GPHandle, GP_RECV_BUDDY_REVOKE, (GPCallback)::GPRecvBuddyRevokeCallback, this);
			gpSetCallback(&GPHandle, GP_RECV_GAME_INVITE, (GPCallback)::GPRecvGameInviteCallback, this);
			gpSetCallback(&GPHandle, GP_TRANSFER_CALLBACK, (GPCallback)::GPTransferCallback, this);
			// Initialize Sake
			gsCoreInitialize();
			// Set the URL to use @see the definition of SAKEI_SOAP_URL_FORMAT for more details
			snprintf(sakeiSoapUrl,SAKE_MAX_URL_LENGTH,SAKEI_SOAP_URL_FORMAT,TCHAR_TO_ANSI(appGetGameSpyGameName()));
			SAKEStartupResult sakeResult = sakeStartup(&SakeHandle);
			debugf(NAME_DevOnline,TEXT("sakeStartup() returned 0x%08X"),(DWORD)sakeResult);
			if (sakeResult == SAKEStartupResult_SUCCESS)
			{
				sakeSetGame(SakeHandle,
					(const UCS2String)appGetGameSpyGameName(),
					GameID,
					(const UCS2String)appGetGameSpySecretKey());
				// Set the URL to use @see the definition of SC_SERVICE_URL_FORMAT for more details
				snprintf(scServiceURL,SC_SERVICE_MAX_URL_LEN,SC_SERVICE_URL_FORMAT,TCHAR_TO_ANSI(appGetGameSpyGameName()));
				// Initialize stats
				SCResult scResult = scInitialize(GameID, &SCHandle);
				debugf(NAME_DevOnline,TEXT("scInitialize() returned 0x%08X"),(DWORD)scResult);
				if (scResult == SCResult_NO_ERROR)
				{
#if WANTS_CD_KEY_AUTH
					// Init CD key checking
					INT GcdInitCode = gcd_init(GameID);
					debugf(NAME_DevOnline,TEXT("gcd init returned 0x%08X"),GcdInitCode);
					if (GcdInitCode == 0)
#endif
					{
						debugf(NAME_DevOnline,TEXT("Initialization of GameSpy successful"));
						bWasInited = TRUE;
					}
				}
				else
				{
					SCHandle = NULL;
				}
			}
			else
			{
				SakeHandle = NULL;
			}
		}
		else
		{
			GPHandle = NULL;
		}
	}
	else
	{
		// Error case. Log various strings
		if (Result == GSIACUnavailable)
		{
			debugf(NAME_Error,TEXT("GameSpy services are unavailable"));
		}
		if (Result == GSIACTemporarilyUnavailable)
		{
			debugf(NAME_Error,TEXT("GameSpy services are temporarily unavailable"));
		}
	}
	if (bWasInited == FALSE)
	{
		debugf(NAME_DevOnline,TEXT("Initialization of GameSpy failed"));
	}
	return bWasInited;
}

#if PS3
/**
 * Initializes the NP libs
 */
void UOnlineSubsystemGameSpy::InitNp()
{
	check(NpData == NULL);
	// Allocate the struct that holds the NP state
	NpData = new FNpData();
	// Initialize NP
	NpData->InitNp();
}
#endif

/**
 * Handles updating of any async tasks that need to be performed
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemGameSpy::TickVoice(FLOAT DeltaTime)
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
 * Ticks the keyboard UI task if open
 */
void UOnlineSubsystemGameSpy::TickKeyboardUI(void)
{
#if PS3
	// Tick the keyboard if open and not canceled
	if (bNeedsKeyboardTicking == TRUE &&
		bWasKeyboardInputCanceled == FALSE)
	{
		UBOOL bWasCanceled = FALSE;
		if (appPS3TickVirtualKeyboard(KeyboardResultsString,bWasCanceled))
		{
			// Completed so copy results
			bNeedsKeyboardTicking = FALSE;
			bWasKeyboardInputCanceled = bWasCanceled;
		}
		// The keyboard is done being process so notify the UI
		if (bNeedsKeyboardTicking == FALSE)
		{
			// Fire off the delegates
			FAsyncTaskDelegateResults Parms(S_OK);
			// Use the common method to do the work
			TriggerOnlineDelegates(this,KeyboardInputDelegates,&Parms);
		}
	}
#endif
}

#if PS3
/**
 * Handles updating of any async tasks that need to be performed
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemGameSpy::TickNp(FLOAT DeltaTime)
{
	if (NpData)
	{
		// Tick the NP object for PS3 specific tasks
		NpData->Tick(DeltaTime);
		// Login if the player information has changed
		if (NpData->bNeedsGameSpyAuth)
		{
			AutoLogin();
			NpData->bNeedsGameSpyAuth = FALSE;
		}
		// Check for having NP shut down
		if (NpData->HasGoneOffline())
		{
			NpData->ResetOfflineEvent();
			// Force us out of GameSpy too
			gpDisconnect(&GPHandle);
			// Sign us out
			ClearPlayerInfo();
		}
	}

	// for PS3, start getting trophy info
	UBOOL bWasSuccessful = FALSE;
	ETrophyTaskComplete CompleteType = GPS3NetworkPlatform->GetTrophyTaskState(bWasSuccessful);

	if (CompleteType == TTC_Update)
	{
		OnlineSubsystemGameSpy_eventOnReadAchievementsComplete_Parms Parms(EC_EventParm);
		// PS3 can't read trophies for other titles
		Parms.TitleId = 0;
		// Use the common method to do the work
		TriggerOnlineDelegates(this,ReadAchievementsCompleteDelegates,&Parms);
	}
	else if (CompleteType == TTC_Unlock)
	{
		// Just trigger the delegate for now (achievement will be unlocked in due time)
		FAsyncTaskDelegateResults Results(bWasSuccessful);
		TriggerOnlineDelegates(this,UnlockAchievementCompleteDelegates,&Results);
	}
}
#endif

/**
 * Handles updating of any async tasks that need to be performed
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemGameSpy::Tick(FLOAT DeltaTime)
{
	// Check for there not being a logged in user, since we require at least a default user
	if (LoggedInStatus == LS_NotLoggedIn && bIsLoginInProcess == FALSE)
	{
		SignInLocally();
	}
	// Tick any async tasks that may need to notify their delegates
	TickAsyncTasks(DeltaTime);
	// Tick any tasks needed for LAN/networked game support
	TickGameInterfaceTasks(DeltaTime);
	// Let voice do any processing
	TickVoice(DeltaTime);
#if PS3
	// Tick the async keyboard if needed
	TickKeyboardUI();
	// Tick NP for sign in/out events and network loss
	TickNp(DeltaTime);
	// Tick the controllers for connection state changes
	TickControllers();
	// Check for disk ejection and shutdown any game
	extern UBOOL GNoRecovery;
	if (GNoRecovery && CachedGameInt->GameSettings)
	{
		CachedGameInt->DestroyOnlineGame(NAME_None);
		// Now shutdown the netdriver
		if (GWorld && GWorld->GetNetDriver())
		{
			UTcpNetDriver* NetDriver = Cast<UTcpNetDriver>(GWorld->GetNetDriver());
			// Close all net connections
			for (INT Index = 0; Index < NetDriver->ClientConnections.Num(); Index++)
			{
				UNetConnection* Connection = NetDriver->ClientConnections(Index);
				Connection->Close();
			}
			NetDriver->Socket->Close();
		}
	}
#endif
	TickConnectionStatusChange(DeltaTime);
	// Tick the GameSpy Core
	TickGameSpyTasks();
}

/**
 * Checks any queued async tasks for status, allows the task a change
 * to process the results, and fires off their delegates if the task is
 * complete
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineSubsystemGameSpy::TickAsyncTasks(FLOAT DeltaTime)
{
	// Check each task for completion
	for (INT Index = 0; Index < AsyncTasks.Num(); Index++)
	{
		// Allow it a time slice
		AsyncTasks(Index)->Tick(DeltaTime);
		// See if it's done
		if (AsyncTasks(Index)->HasTaskCompleted())
		{
			// Perform any task specific finalization of data before having
			// the script delegate fired off
			if (AsyncTasks(Index)->ProcessAsyncResults(this) == TRUE)
			{
				// Have the task fire off its delegates on our object
				AsyncTasks(Index)->TriggerDelegates(this);
#if !FINAL_RELEASE && !SHIPPING_PC_GAME
				AsyncTasks(Index)->LogResults();
#endif
				// Free the memory and remove it from our list
				delete AsyncTasks(Index);
				AsyncTasks.Remove(Index);
				Index--;
			}
		}
	}
}

/**
 * Allows the GameSpy code to perform their Think operations
 */
void UOnlineSubsystemGameSpy::TickGameSpyTasks(void)
{
	gsCoreThink(0);
	// Tick GP
	if(GPHandle != NULL)
	{
		gpProcess(&GPHandle);
	}
	// Tick SC
	if (SCHandle != NULL)
	{
		scThink(SCHandle);
	}
#if WANTS_CD_KEY_AUTH
	// Tick the CD key task
	gcd_think();
#endif
}

#if PS3
/** Updates the controller state and fires any delegates */
void UOnlineSubsystemGameSpy::TickControllers(void)
{
	// Check each of the controllers for state changes
	for (INT Index = 0; Index < 4; Index++)
	{
		ControllerStates[Index].bIsControllerConnected = appIsControllerPresent(Index);
		// If last frame's state doesn't match the current, fire off the delegates
		if (ControllerStates[Index].bIsControllerConnected != ControllerStates[Index].bLastIsControllerConnected)
		{
			OnlineSubsystemGameSpy_eventOnControllerChange_Parms Parms(EC_EventParm);
			Parms.ControllerId = Index;
			Parms.bIsConnected = ControllerStates[Index].bIsControllerConnected ? FIRST_BITFIELD : 0;
			TriggerOnlineDelegates(this,ControllerChangeDelegates,&Parms);
		}
		// Copy the state over for next frame's check
		ControllerStates[Index].bLastIsControllerConnected = ControllerStates[Index].bIsControllerConnected;
	}
}
#endif

/**
 * Ticks the connection checking code
 *
 * @param DeltaTime the amount of time that has elapsed since the list check
 */
void UOnlineSubsystemGameSpy::TickConnectionStatusChange(FLOAT DeltaTime)
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
			OnlineSubsystemGameSpy_eventOnLinkStatusChange_Parms Parms(EC_EventParm);
			Parms.bIsConnected = bHasConnection ? FIRST_BITFIELD : 0;
			TriggerOnlineDelegates(this,LinkStatusDelegates,&Parms);
		}
		bLastHasConnection = bHasConnection;
		ConnectionPresenceElapsedTime = 0.f;
	}
}

/** @return Builds the GameSpy location string from the game the player is connected to */
FString UOnlineSubsystemGameSpy::GetServerLocation(void) const
{
	if (CachedGameInt->GameSettings &&
		CachedGameInt->SessionInfo)
	{
		FInternetIpAddr Addr(CachedGameInt->SessionInfo->HostAddr);
		DWORD OutAddr = 0;
		// This will be zero if we are the server
		Addr.GetIp(OutAddr);
		if (OutAddr == 0)
		{
			// Get the real IP address of the machine
			GSocketSubsystem->GetLocalHostAddr(*GLog,Addr);
		}
		// Format the string using the address and our connection count
		FString Location = FString::Printf(TEXT("%s://%s/?MaxPub=%d?MaxPri=%d"),
			*LocationUrl,
			*Addr.ToString(TRUE),
			CachedGameInt->GameSettings->NumPublicConnections,
			CachedGameInt->GameSettings->NumPrivateConnections);
		// Lan matches aren't joinable, so flag them as such
		if (CachedGameInt->GameSettings->bIsLanMatch)
		{
			Location += TEXT("?bIsLanMatch");
		}
		else
		{
#if PS3 && WANTS_NAT_TRAVERSAL
			// Get the NP ID of the hosting session
			Location += appNpIdToUrl((FSessionInfoPS3*)CachedGameInt->SessionInfo);
#endif
		}
		INT IsLocked = 0;
		//hack: add passworded server info
		if (CachedGameInt->GameSettings->GetStringSettingValue(7,IsLocked))
		{
			if (IsLocked)
			{
				Location += TEXT("?bRequiresPassword");
			}
		}
		return Location;
	}
	return FString::Printf(TEXT("%s:///Standalone"),*LocationUrl);
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
UBOOL UOnlineSubsystemGameSpy::Login(BYTE LocalUserNum,const FString& LoginName,const FString& Password,UBOOL bWantsLocalOnly)
{
	UBOOL bSignedInOk = FALSE;
	if (bWantsLocalOnly == FALSE)
	{
		// Try to sign into the specified profile
		GPResult Result = gpConnectUniqueNick(&GPHandle,
			(const UCS2String)*LoginName,
			(const UCS2String)*Password,
			GP_NO_FIREWALL,
			GP_NON_BLOCKING,
			(GPCallback)::GPConnectCallback,
			this);
		debugf(NAME_DevOnline,
			TEXT("gpConnectUniqueNick() 0x%08X"),
			(DWORD)Result);
		if (Result == GP_NO_ERROR)
		{
			// Store the password temporarily during the login attempt for the login certificate request
			LoggedInPlayerPassword = Password;
			// Stash which player is the active one
			LoggedInPlayerNum = LocalUserNum;
			bIsLoginInProcess = TRUE;
			bSignedInOk = TRUE;
		}
		else
		{
			ClearPlayerInfo();
			FLoginFailedParms Params(LocalUserNum,GP_LOGIN_CONNECTION_FAILED);
			TriggerOnlineDelegates(this,LoginFailedDelegates,&Params);
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
			debugf(NAME_DevOnline,TEXT("Signing into profile %s locally"),*LoggedInPlayerName);
			// Trigger the delegates so the UI can process
			OnlineSubsystemGameSpy_eventOnLoginChange_Parms Parms2(EC_EventParm);
			Parms2.LocalUserNum = 0;
			TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms2);
		}
		else
		{
			// Restore the previous log in name
			LoggedInPlayerName = BackupLogin;
			FLoginFailedParms Params(LocalUserNum,GP_LOGIN_BAD_PROFILE);
			TriggerOnlineDelegates(this,LoginFailedDelegates,&Params);
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
UBOOL UOnlineSubsystemGameSpy::AutoLogin(void)
{
#if PS3
	if (NpData)
	{
		ClearPlayerInfo();
		// Set for viewing auth data
		const gsi_u8* Ps3Cert = (gsi_u8*)NpData->RemoteAuthTicket;
		const int Ps3CertLen = NpData->RemoteAuthTicketLength;
		// Request a login certificate
		WSLoginValue Result = (WSLoginValue)wsLoginPs3Cert(GameID,
			GetPartnerId(),
			GetNamespaceId(),
			Ps3Cert,
			Ps3CertLen,
			::PS3LoginCertificateCallback,
			this);
		debugf(NAME_DevOnline,TEXT("wsLoginPs3Cert() returned 0x%08X"),(DWORD)Result);
		if (Result == WSLogin_Success)
		{
			// Stash which player is the active one
			LoggedInPlayerNum = 0;
			bIsLoginInProcess = TRUE;
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Failed to retrieve the "),(DWORD)Result);
		}
		return Result == WSLogin_Success;
	}
	return FALSE;
#else
	UBOOL bReturn = FALSE;
	AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
	// Only dedicated servers should use auto login
	if (WorldInfo && WorldInfo->NetMode == NM_DedicatedServer)
	{
		FString GameSpyId;
		// Check to see if they specified a login
		if (Parse(appCmdLine(),TEXT("-Login="),GameSpyId))
		{
			FString GameSpyPassword;
			// Make sure there is a password too
			if (Parse(appCmdLine(),TEXT("-Password="),GameSpyPassword))
			{
				bReturn = Login(0,GameSpyId,GameSpyPassword);
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Auto sign in is for dedicated servers only"));
	}
	return bReturn;
#endif
}

/**
 * Logs the player into the default account
 */
void UOnlineSubsystemGameSpy::SignInLocally(void)
{
#if PS3
	LoggedInPlayerName = appGetUserName();
	if (LoggedInPlayerName.Len() == 0)
	{
		LoggedInPlayerName = GetClass()->GetDefaultObject<UOnlineSubsystemGameSpy>()->LocalProfileName;
	}
	debugf(NAME_DevOnline,TEXT("Signing into the local profile %s"),*LoggedInPlayerName);
	LoggedInPlayerNum = 0;
	LoggedInStatus = LS_UsingLocalProfile;
	// Trigger the delegates so the UI can process
	OnlineSubsystemGameSpy_eventOnLoginChange_Parms Parms2(EC_EventParm);
	Parms2.LocalUserNum = 0;
	TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms2);
#else
	// Don't use the auto sign in feature if they have a valid GameSpy account
	if (bHasGameSpyAccount == FALSE)
	{
		LoggedInPlayerName = GetClass()->GetDefaultObject<UOnlineSubsystemGameSpy>()->LocalProfileName;
		debugf(NAME_DevOnline,TEXT("Signing into the local profile %s"),*LoggedInPlayerName);
		LoggedInPlayerNum = 0;
		LoggedInStatus = LS_UsingLocalProfile;
		// Trigger the delegates so the UI can process
		OnlineSubsystemGameSpy_eventOnLoginChange_Parms Parms2(EC_EventParm);
		Parms2.LocalUserNum = 0;
		TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms2);
	}
#endif
}

/**
 * Signs the player out of the online service
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::Logout(BYTE LocalUserNum)
{
	DWORD Result = E_FAIL;
	// If they don't have a profile, or it's not actively doing something
	// then sign out
	if (CachedProfile == NULL ||
		(CachedProfile->AsyncState != OPAS_Read && CachedProfile->AsyncState != OPAS_Write))
	{
		debugf(NAME_DevOnline,TEXT("Logging out for %s"),*LoggedInPlayerName);
		gpDisconnect(&GPHandle);
		ClearPlayerInfo();
		Result = S_OK;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't logout when an async profile task is in progress"));
	}
	// Trigger the delegates so the UI can process
	FAsyncTaskDelegateResults Params(Result);
	TriggerOnlineDelegates(this,LogoutCompletedDelegates,&Params);
	// Only fire the login change delegate if it works
	if (Result == S_OK)
	{
		// Trigger the delegates so the UI can process
		OnlineSubsystemGameSpy_eventOnLoginChange_Parms Parms2(EC_EventParm);
		Parms2.LocalUserNum = 0;
		TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms2);
	}
	return TRUE;
}

/**
 * Fetches the login status for a given player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the enum value of their status
 */
BYTE UOnlineSubsystemGameSpy::GetLoginStatus(BYTE LocalUserNum)
{
	ELoginStatus Status = LS_NotLoggedIn;
	if (LocalUserNum == LoggedInPlayerNum && bIsLoginInProcess == FALSE)
	{
		GPEnum Connected = GP_NOT_CONNECTED;
		// Check the connected status
		if (gpIsConnected(&GPHandle,&Connected) == GP_NO_ERROR)
		{
			if (Connected == GP_CONNECTED)
			{
				LoggedInStatus = Status = LS_LoggedIn;
			}
		}
		// Handle them using a local profile
		if (LoggedInStatus == LS_UsingLocalProfile && Status == LS_NotLoggedIn)
		{
			Status = LS_UsingLocalProfile;
		}
	}
	return Status;
}

/**
 * Checks that a unique player id is part of the specified user's friends list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player being checked
 *
 * @return TRUE if a member of their friends list, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::IsFriend(BYTE LocalUserNum,FUniqueNetId PlayerID)
{
	if (LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_NotLoggedIn)
	{
		// Ask GameSpy if they are on the buddy list
		return gpIsBuddy(&GPHandle,PlayerID.ToDWORD());
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

UBOOL UOnlineSubsystemGameSpy::AreAnyFriends(BYTE LocalUserNum,TArray<FFriendsQuery>& Query)
{
	UBOOL bReturn = FALSE;
	// GameSpy doesn't have a bulk check so check one at a time
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
 * Sends off a request for the profile from Sake
 * This will result in the SakeRequestProfileCallback being called
 */
void UOnlineSubsystemGameSpy::SakeRequestProfile(void)
{
	// Check if there's an existing profile on the backend
	static char* FieldNames[] = { "recordid", "profile" };  //todo: this is a constant, where should it go?  is static ok?
	SAKEGetMyRecordsInput* Input = new SAKEGetMyRecordsInput;  //todo: this object needs to persist, can we store it as a class member?
	if(!Input)
	{
		//todo: handle error
		return;
	}
	Input->mTableId = "Profiles";  //todo: this is a constant, where should it go?
	Input->mFieldNames = FieldNames;
	Input->mNumFields = (sizeof(FieldNames) / sizeof(FieldNames[0]));
	SAKERequest Request = sakeGetMyRecords(SakeHandle, Input, (SAKERequestCallback)::SakeRequestProfileCallback, this);
	SAKEStartRequestResult Result = sakeGetStartRequestResult(SakeHandle);
	debugf(NAME_DevOnline,TEXT("sakeGetMyRecords() returned 0x%08X"),(DWORD)Result);
	if (Request == NULL || Result != SAKEStartRequestResult_SUCCESS)
	{
		//todo: handle error
	}
}

/**
 * Creates a profile in Sake
 *
 * @param Buffer a serialized version of the profile
 * @param Length the number of bytes in the buffer
 */
void UOnlineSubsystemGameSpy::SakeCreateProfile(const BYTE* Buffer, DWORD Length)
{
	// Create a profile Sake
	static SAKEField Fields[1];  //todo: this needs to persist, is static ok?
	Fields[0].mName = "profile";  //todo: this is a constant, where should it go?
	Fields[0].mType = SAKEFieldType_BINARY_DATA;
	Fields[0].mValue.mBinaryData.mValue = (gsi_u8*)Buffer;
	Fields[0].mValue.mBinaryData.mLength = (INT)Length;
	SAKECreateRecordInput* Input = new SAKECreateRecordInput;  //todo: this object needs to persist, can we store it as a class member?
	if(!Input)
	{
		//todo: handle error
		return;
	}
	Input->mTableId = "Profiles";  //todo: this is a constant, where should it go?
	Input->mFields = Fields;
	Input->mNumFields = (sizeof(Fields) / sizeof(Fields[0]));
	SAKERequest Request = sakeCreateRecord(SakeHandle, Input, (SAKERequestCallback)::SakeCreateProfileCallback, this);
	SAKEStartRequestResult Result = sakeGetStartRequestResult(SakeHandle);
	debugf(NAME_DevOnline,TEXT("sakeCreateRecord() returned 0x%08X"),(DWORD)Result);
	if (Request == NULL || Result != SAKEStartRequestResult_SUCCESS)
	{
		//todo: handle error
	}
}

/**
 * Updates a profile in Sake
 *
 * @param Buffer a serialized version of the profile
 * @param Length the number of bytes in the buffer
 */
UBOOL UOnlineSubsystemGameSpy::SakeUpdateProfile(const BYTE* Buffer, DWORD Length)
{
	UBOOL bWasSuccessful = FALSE;
	// We can't update the profile if we don't have a recordid for it
	if (SakeProfileRecordID > 0)
	{
		// Update the profile stored in Sake
		static SAKEField Fields[1];  //todo: this needs to persist, is static ok?
		Fields[0].mName = "profile";  //todo: this is a constant, where should it go?
		Fields[0].mType = SAKEFieldType_BINARY_DATA;
		Fields[0].mValue.mBinaryData.mValue = (gsi_u8*)Buffer;
		Fields[0].mValue.mBinaryData.mLength = (INT)Length;
//todo: this object needs to persist, can we store it as a class member?
		SAKEUpdateRecordInput* Input = new SAKEUpdateRecordInput;
		if (Input)
		{
//todo: this is a constant, where should it go?
			Input->mTableId = "Profiles";
			Input->mRecordId = SakeProfileRecordID;
			Input->mFields = Fields;
			Input->mNumFields = (sizeof(Fields) / sizeof(Fields[0]));
			SAKERequest Request = sakeUpdateRecord(SakeHandle, Input, (SAKERequestCallback)::SakeUpdateProfileCallback, this);
			SAKEStartRequestResult Result = sakeGetStartRequestResult(SakeHandle);
			debugf(NAME_DevOnline,TEXT("sakeUpdateRecord() returned 0x%08X"),(DWORD)Result);
			if (Request != NULL && Result == SAKEStartRequestResult_SUCCESS)
			{
				// The update is proceeding fine
				bWasSuccessful = TRUE;
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("Failed to update profile via Sake"));
				delete Input;
			}
		}
	}
	return bWasSuccessful;
}

/**
 * Generates the field name used by Sake, based on a view id and a column id
 *
 * @param StatsRead holds the definitions of the tables to read the data from
 * @param ViewId the view to read from
 * @param ColumnId the column to read.  If 0, then this is just the view's column (flag indicating if the view has been written to)
 * @return the name of the field
 */
static FString GetStatsFieldName(UOnlineStatsRead* StatsRead, INT ViewId, INT ColumnId)
{
	FString ViewName = StatsRead->ViewName;
	if(ColumnId == 0)
	{
		return ViewName;
	}
	FName ColumnName = NAME_None;
	for (INT Index = 0; Index < StatsRead->ColumnMappings.Num(); Index++)
	{
		if(StatsRead->ColumnMappings(Index).Id == ColumnId)
		{
			ColumnName = StatsRead->ColumnMappings(Index).Name;
			break;
		}
	}
	return FString::Printf(TEXT("%s_%s"), *ViewName, *ColumnName.ToString());
}

/**
 * Does the common setup for a ReadOnlineStat requests
 *
 * @param StatsRead holds the definitions of the tables to read the data from and
 *		  results are copied into the specified object
 * @param SearchInput gets allocated internally, then filled with the common request info
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::SetupReadOnlineStatsRequest(UOnlineStatsRead* StatsRead, SAKESearchForRecordsInput*& SearchInput)
{
	UBOOL Return = FALSE;
	if (CurrentStatsRead == NULL)
	{
		CurrentStatsRead = StatsRead;
		// Clear previous results
		CurrentStatsRead->Rows.Empty();
		// Setup the tableid
		FString TableIdUnicode = FString::Printf(TEXT("PlayerStats_v%d"), StatsVersion);
		char* TableId = new char[TableIdUnicode.Len() + 1];
		UCS2ToAsciiString((const UCS2String)*TableIdUnicode, TableId);
		// Create the array of field names
		TArray<FString> FieldNamesUnicode;
		for(INT Index = 0; Index < StatsRead->ColumnIds.Num(); Index++)
		{
			FString FieldName = GetStatsFieldName(StatsRead, StatsRead->ViewId, StatsRead->ColumnIds(Index));
			FieldNamesUnicode.AddItem(FieldName);
		}
		FieldNamesUnicode.AddItem(FString(TEXT("ownerid")));  // these special fields needs to be last
		FieldNamesUnicode.AddItem(FString(TEXT("row")));
		FieldNamesUnicode.AddItem(FString(TEXT("Nick")));
		INT NumFields = FieldNamesUnicode.Num();
		char** FieldNames = new char*[NumFields];
		for(INT Index = 0; Index < NumFields; Index++)
		{
			FString FieldName = FieldNamesUnicode(Index);
			FieldNames[Index] = new char[FieldName.Len() + 1];
			UCS2ToAsciiString((const UCS2String)*FieldName, FieldNames[Index]);
		}
		// Create the filter
		FString FilterFieldName = GetStatsFieldName(StatsRead, StatsRead->ViewId, 0);
		FString FilterUnicode = FString::Printf(TEXT("NUM_%s > 0"), *FilterFieldName);
		TCHAR* Filter = new TCHAR[FilterUnicode.Len() + 1];
		appStrcpy(Filter,FilterUnicode.Len(),*FilterUnicode);
		// Setup the sort
		FString SortFieldName = GetStatsFieldName(StatsRead, StatsRead->ViewId, StatsRead->SortColumnId);
		FString SortUnicode = FString::Printf(TEXT("%s desc"), *SortFieldName);
		char* Sort = new char[SortUnicode.Len() + 1];
		UCS2ToAsciiString((const UCS2String)*SortUnicode, Sort);
		// Setup the Sake search input structure
		SearchInput = new SAKESearchForRecordsInput;
		appMemset(SearchInput, 0, sizeof(SAKESearchForRecordsInput));
		SearchInput->mTableId = TableId;
		SearchInput->mFieldNames = FieldNames;
		SearchInput->mNumFields = NumFields;
		SearchInput->mFilter = (gsi_char*)Filter;
		SearchInput->mSort = Sort;
		Return = TRUE;
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't perform a stats read while one is in progress"));
	}
	return Return;
}

/**
 * Sends a ReadOnlineStats request
 *
 * @param SearchInput points to the request input
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::SendReadOnlineStatsRequest(SAKESearchForRecordsInput* SearchInput)
{
	// Submit the search request
	SAKERequest SearchRequest = sakeSearchForRecords(SakeHandle,
		SearchInput,
		(SAKERequestCallback)::SakeSearchForRecordsCallback,
		this);
	SAKEStartRequestResult Result = sakeGetStartRequestResult(SakeHandle);
	debugf(NAME_DevOnline,TEXT("sakeSearchForRecords() returned 0x%08X"),(DWORD)Result);
	return SearchRequest != NULL && Result == SAKEStartRequestResult_SUCCESS;
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
UBOOL UOnlineSubsystemGameSpy::ReadOnlineStats(const TArray<FUniqueNetId>& Players,
	UOnlineStatsRead* StatsRead)
{
	DWORD Result = E_FAIL;
	if (CurrentStatsRead == NULL)
	{
		// Validate that players were specified
		if (Players.Num() > 0)
		{
			// Setup the request
			SAKESearchForRecordsInput* SearchInput;
			if (SetupReadOnlineStatsRequest(StatsRead, SearchInput))
			{
				// Setup the ownerids
				INT NumOwnerIds = Players.Num();
				INT* OwnerIds = new INT[NumOwnerIds];
				for (INT Index = 0; Index < NumOwnerIds; Index++)
				{
					OwnerIds[Index] = Players(Index).ToDWORD();
				}
				SearchInput->mOwnerIds = OwnerIds;
				SearchInput->mNumOwnerIds = NumOwnerIds;
				SearchInput->mCacheFlag = gsi_false;
				// We should be able to drop this line after a backend update
				SearchInput->mMaxRecords = NumOwnerIds;
				// Send the request
				if (SendReadOnlineStatsRequest(SearchInput))
				{
					Result = ERROR_IO_PENDING;
				}
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't perform a stats read when one is in progress"));
	}
	if (Result != ERROR_IO_PENDING)
	{
		debugf(NAME_Error,TEXT("ReadOnlineStats() failed"));
		CurrentStatsRead = NULL;
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResults Param(Result);
		TriggerOnlineDelegates(this,ReadOnlineStatsCompleteDelegates,&Param);
	}
	return Result == ERROR_IO_PENDING;
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
UBOOL UOnlineSubsystemGameSpy::ReadOnlineStatsForFriends(BYTE LocalUserNum,
	UOnlineStatsRead* StatsRead)
{
#if PS3
	debugf(NAME_DevOnline, TEXT("Reading stats for friends is unsupported on PS3"));
	return FALSE;
#else
	DWORD Result = E_FAIL;
	if (CurrentStatsRead == NULL)
	{
		check(GPHandle);
		INT NumBuddies;
		gpGetNumBuddies(&GPHandle, &NumBuddies);
		TArray<FUniqueNetId> Players;
		Players.AddItem(LoggedInPlayerId);
		for (INT Index = 0; Index < NumBuddies; Index++)
		{
			GPResult Result;
			GPBuddyStatus BuddyStatus;
			Result = gpGetBuddyStatus(&GPHandle, Index, &BuddyStatus);
			if(Result != GP_NO_ERROR)
			{
				//TODO: handle error
				continue;
			}
			if(BuddyStatus.profile != LoggedInPlayerId.ToDWORD())
			{
				FUniqueNetId Player;
				Player = (DWORD)BuddyStatus.profile;
				Players.AddItem(Player);
			}
		}
		// Now use the common method to read the stats
		return ReadOnlineStats(Players,StatsRead);
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't perform a stats read while one is in progress"));
	}
	return FALSE;
#endif
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
UBOOL UOnlineSubsystemGameSpy::ReadOnlineStatsByRank(UOnlineStatsRead* StatsRead,
	INT StartIndex,INT NumToRead)
{
	DWORD Result = E_FAIL;
	if (CurrentStatsRead == NULL)
	{
		// Setup the request
		SAKESearchForRecordsInput* SearchInput;
		if (SetupReadOnlineStatsRequest(StatsRead, SearchInput))
		{
			// Setup the offset and max
			SearchInput->mOffset = Max(StartIndex - 1,0);
			SearchInput->mMaxRecords = NumToRead;
			SearchInput->mCacheFlag = gsi_true;
			// Send the request
			if (SendReadOnlineStatsRequest(SearchInput))
			{
				Result = ERROR_IO_PENDING;
			}
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't perform a stats read while one is in progress"));
	}
	if (Result != ERROR_IO_PENDING)
	{
		debugf(NAME_Error,TEXT("ReadOnlineStatsByRank() failed"));
		CurrentStatsRead = NULL;
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResults Param(Result);
		TriggerOnlineDelegates(this,ReadOnlineStatsCompleteDelegates,&Param);
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
UBOOL UOnlineSubsystemGameSpy::ReadOnlineStatsByRankAroundPlayer(BYTE LocalUserNum,
	UOnlineStatsRead* StatsRead,INT NumRows)
{
	DWORD Result = E_FAIL;
	if (CurrentStatsRead == NULL)
	{
		// Setup the request
		SAKESearchForRecordsInput* SearchInput;
		if (SetupReadOnlineStatsRequest(StatsRead, SearchInput))
		{
			// Setup the target filter
			FString TargetFilterUnicode = FString::Printf(TEXT("ownerid=%d"), LoggedInPlayerId.ToDWORD());
			TCHAR* TargetFilter = new TCHAR[TargetFilterUnicode.Len() + 1];
			appStrcpy(TargetFilter,TargetFilterUnicode.Len(),*TargetFilterUnicode);
			SearchInput->mTargetRecordFilter = (gsi_char*)TargetFilter;
			SearchInput->mSurroundingRecordsCount = NumRows;
			SearchInput->mCacheFlag = gsi_false;
			// Allow space for players above and below you plus yourself
			SearchInput->mMaxRecords = (NumRows * 2) + 1;
			// Send the request
			if (SendReadOnlineStatsRequest(SearchInput))
			{
				Result = ERROR_IO_PENDING;
			}
		}
	}
	else
	{
		debugf(NAME_Error,TEXT("Can't perform a stats read while one is in progress"));
	}
	if (Result != ERROR_IO_PENDING)
	{
		debugf(NAME_Error,TEXT("ReadOnlineStatsByRankAroundPlayer() failed"));
		CurrentStatsRead = NULL;
		// Just trigger the delegate as having failed
		FAsyncTaskDelegateResults Param(Result);
		TriggerOnlineDelegates(this,ReadOnlineStatsCompleteDelegates,&Param);
	}
	return Result == ERROR_IO_PENDING;
}

/**
 * Cleans up any platform specific allocated data contained in the stats data
 *
 * @param StatsRead the object to handle per platform clean up on
 */
void UOnlineSubsystemGameSpy::FreeStats(UOnlineStatsRead* StatsRead)
{
}

/**
 * Initiates a session
 * Will result in the CreateStatsSessionCallback being called
 */
void UOnlineSubsystemGameSpy::CreateStatsSession(void)
{
	// Ignore servers that have no signed in players or aren't requesting stats
	if (LoginCertificate != NULL &&
		LoginPrivateData != NULL &&
		CachedGameInt->GameWantsStats() &&
		GWorld->GetWorldInfo() &&
		GWorld->GetWorldInfo()->NetMode != NM_Client)
	{
		const INT Timeout = 0;
		SCResult Result = scCreateSession(SCHandle,
			LoginCertificate,
			LoginPrivateData,
			::CreateStatsSessionCallback,
			Timeout,
			this);
		debugf(NAME_DevOnline,TEXT("scCreateSession() returned 0x%08X"),(DWORD)Result);
		if (Result != SCResult_NO_ERROR)
		{
			DWORD Return = E_FAIL;
			FAsyncTaskDelegateResults Params(Return);
			TriggerOnlineDelegates(GameInterfaceImpl,GameInterfaceImpl->StartOnlineGameCompleteDelegates,&Params);
		}
	}
	else
	{
		FAsyncTaskDelegateResults Params(S_OK);
		TriggerOnlineDelegates(GameInterfaceImpl,GameInterfaceImpl->StartOnlineGameCompleteDelegates,&Params);
	}
}

/**
 * Called by the host after creating a stats session
 * Will result in the HostSetStatsReportIntentionCallback being called
 */
UBOOL UOnlineSubsystemGameSpy::HostSetStatsReportIntention(void)
{
	INT UserId = 0;
	gpUserIDFromProfile(&GPHandle,LoggedInPlayerId.ToDWORD(),&UserId);
	// Register the user usage analysis
	ptTrackUsage(UserId,
		ProductID,
		(const UCS2String)*FString::Printf(TEXT("%s.%d"),appGetGameSpyGameName(),GEngineVersion),
		0,
		PTFalse);
	// Ignore servers that have no signed in players
	if (SessionHasStats())
	{
		const gsi_bool Authoritative = gsi_true;
		const INT Timeout = 0;
		SCResult Result = scSetReportIntention(SCHandle,
			NULL,
			Authoritative,
			LoginCertificate,
			LoginPrivateData,
			::HostSetStatsReportIntentionCallback,
			Timeout,
			this);
		debugf(NAME_DevOnline,
			TEXT("HostSetStatsReportIntention: scSetReportIntention() returned 0x%08X"),
			(DWORD)Result);
		return Result == SCResult_NO_ERROR;
	}
	return FALSE;
}

/**
 * Called by the client after receiving the host's session id
 * Will result in the ClientSetStatsReportIntentionCallback being called
 *
 * @param SessionId the session id received from the host
 */
UBOOL UOnlineSubsystemGameSpy::ClientSetStatsReportIntention(const gsi_u8 SessionId[SC_SESSION_GUID_SIZE])
{
	INT UserId = 0;
	gpUserIDFromProfile(&GPHandle,LoggedInPlayerId.ToDWORD(),&UserId);
	// Register the user usage analysis
	ptTrackUsage(UserId,
		ProductID,
		(const UCS2String)*FString::Printf(TEXT("%s.%d"),appGetGameSpyGameName(),GEngineVersion),
		0,
		PTFalse);
	// Ignore servers that have no signed in players
	if (SessionHasStats())
	{
		scSetSessionId(SCHandle, SessionId);
		const gsi_bool Authoritative = gsi_false;
		const INT Timeout = 0;
		SCResult Result = scSetReportIntention(SCHandle,
			NULL,
			Authoritative,
			LoginCertificate,
			LoginPrivateData,
			::ClientSetStatsReportIntentionCallback,
			Timeout,
			this);
		debugf(NAME_DevOnline,TEXT("scSetReportIntention() returned 0x%08X"),(DWORD)Result);
		return Result == SCResult_NO_ERROR;
	}
	return FALSE;
}

IMPLEMENT_COMPARE_CONSTREF(FPendingPlayerStats, OnlineSubsystemGameSpy, { return (B.Score.Score - A.Score.Score); })

/**
 * Sorts players by their score and updates their "place" (1st, 2nd, etc.)
 */
void UOnlineSubsystemGameSpy::PreprocessPlayersByScore(void)
{
	// Remove any players that do not have a proper statguid
	for (INT Index = 0; Index < PendingStats.Num(); Index++)
	{
		FPendingPlayerStats& PlayerStats = PendingStats(Index);
		PlayerStats.PlayerName = GetPlayerName(PlayerStats.Player);
		// If the handshaking for this player failed or we don't know who they are,
		// don't report their stats
		if (PlayerStats.StatGuid.Len() == 0 ||
			PlayerStats.PlayerName.Len() == 0)
		{
			debugf(NAME_DevOnline,
				TEXT("Removing player %s from stats due to missing StatGuid/being unknown"),
				*PlayerStats.PlayerName);
			PendingStats.Remove(Index);
			Index--;
		}
	}
	// Sort the remaing players by score
	Sort<USE_COMPARE_CONSTREF(FPendingPlayerStats, OnlineSubsystemGameSpy)>(&PendingStats(0), PendingStats.Num());
	// Loop through and calculate the place
	// We need to do this here to deal with ties
	for (INT Index = 0; Index < PendingStats.Num(); Index++)
	{
		FPendingPlayerStats& PlayerStats = PendingStats(Index);
		INT TieCount = 1;
		for (INT SecIndex = (Index + 1); SecIndex < PendingStats.Num(); SecIndex++)
		{
			if (PendingStats(SecIndex).Score.Score == PlayerStats.Score.Score)
			{
				TieCount++;
			}
			else
			{
				break;
			}
		}
		FString Place = appItoa(Index + 1);
		if (TieCount == 1)
		{
			PlayerStats.Place = Place;
		}
		else
		{
			Place.AppendChar('t');
			Place += appItoa(TieCount);
			for (INT SecIndex = Index; SecIndex < (Index + TieCount); SecIndex++)
			{
				PendingStats(SecIndex).Place = Place;
			}
			Index += (TieCount - 1);
		}
	}
}

/**
 * Counts the number of teams that players were on
 */
INT UOnlineSubsystemGameSpy::GetTeamCount(void)
{
	INT TeamCount = 0;
	// Iterate the players finding the unique number of teams
	for (INT Index = 0; Index < PendingStats.Num(); Index++)
	{
		INT TeamID = PendingStats(Index).Score.TeamID;
		// A team id of 255 means not on a team
		if (TeamID != 255)
		{
			INT SecIndex;
			// Check if we already counted this team
			for (SecIndex = 0; SecIndex < Index; SecIndex++)
			{
				if (PendingStats(SecIndex).Score.TeamID == TeamID)
				{
					break;
				}
			}
			if (SecIndex == Index)
			{
				TeamCount++;
			}
		}
	}
	return TeamCount;
}

/**
 * Appends each stat in a players stats array to the report
 *
 * @param ReportHandle the stats report handle to write to
 * @param Stats the stats to append to the report
 */
void UOnlineSubsystemGameSpy::AppendPlayerStatsToStatsReport(SCReportPtr ReportHandle,const TArrayNoInit<FPlayerStat>& Stats)
{
	// Iterate through the stats and report them
	for (INT StatIndex = 0; StatIndex < Stats.Num(); StatIndex++)
	{
		const FPlayerStat& Stat = Stats(StatIndex);
		INT KeyId = Stat.KeyId;
		const FSettingsData& Data = Stat.Data;
		switch(Data.Type)
		{
			case SDT_Int32:
			{
				INT Value;
				Data.GetData(Value);
				scReportAddIntValue(ReportHandle, KeyId, Value);
				break;
			}
			case SDT_String:
			{
				FString Value;
				Data.GetData(Value);
				scReportAddStringValue(ReportHandle, KeyId, (const UCS2String)*Value);
				break;
			}
			case SDT_Float:
			{
				FLOAT Value;
				Data.GetData(Value);
				scReportAddFloatValue(ReportHandle, KeyId, Value);
				break;
			}
			case SDT_Int64:
			case SDT_Double:
				// Can we support these?
			default:
				// This is an unsupported type
				//TODO: do we just ignore this?
				break;
		}
	}
}

/**
 * For each player in the pending stats array, it adds them and their stats to
 * the report
 *
 * @param ReportHandle the stats report handle to write to
 * @param bNoTeams whether there are teams or not
 *
 * @return the success/error code
 */
DWORD UOnlineSubsystemGameSpy::AppendPlayersToStatsReport(SCReportPtr ReportHandle,UBOOL bNoTeams)
{
	DWORD Return = S_OK;
	scReportBeginPlayerData(ReportHandle);
	// Iterate through all the players adding them and their stats
	for (INT PlayerIndex = 0;
		PlayerIndex < PendingStats.Num() && Return == S_OK;
		PlayerIndex++)
	{
		const FPendingPlayerStats& PlayerStats = PendingStats(PlayerIndex);
		// Setup to report player data
		SCResult BeginResult = scReportBeginNewPlayer(ReportHandle);
		if (BeginResult == SCResult_NO_ERROR)
		{
			// A team id of means no teams. UE3 starts team ids at 0
			gsi_u32 TeamId = bNoTeams ? 0 : (PlayerStats.Score.TeamID + 1);
			gsi_u32 ProfileId = PlayerStats.Player.ToDWORD();
			SCGameResult GameResult = SCGameResult_NONE;
			SCResult PlayerResult = scReportSetPlayerData(ReportHandle,
				PlayerIndex,
				(const gsi_u8*)TCHAR_TO_ANSI(*PlayerStats.StatGuid),
				TeamId,
				GameResult,
				ProfileId,
				LoginCertificate,
				NULL);
			debugf(NAME_DevOnline,
				TEXT("scReportSetPlayerData(%d,,%d,%d,,) returned 0x%08X"),
				PlayerIndex,
				TeamId,
				ProfileId,
				(DWORD)PlayerResult);
			if (PlayerResult == SCResult_NO_ERROR)
			{
				// Append all of the stats data for this player
				AppendPlayerStatsToStatsReport(ReportHandle,PlayerStats.Stats);
				// Report the nick
				scReportAddStringValue(ReportHandle,NickStatsKeyId,(const UCS2String)*PlayerStats.PlayerName);
				// Report the place
				scReportAddStringValue(ReportHandle,PlaceStatsKeyId,(const UCS2String)*PlayerStats.Place);
			}
			else
			{
				Return = E_FAIL;
				debugf(NAME_DevOnline,
					TEXT("Failed to add stats for player %s"),
					*PlayerStats.PlayerName);
			}
		}
		else
		{
			Return = E_FAIL;
			debugf(NAME_DevOnline,
				TEXT("Failed to add stats for player %s"),
				*PlayerStats.PlayerName);
		}
	}
	return Return;
}

/**
 * Called when ready to submit all collected stats
 *
 * @param Players the players for which to submit stats
 * @param StatsWrites the stats to submit for the players
 * @param PlayerScores the scores for the players
 */
UBOOL UOnlineSubsystemGameSpy::CreateAndSubmitStatsReport(void)
{
	DWORD Return = S_OK;
	SCResult Result;
	if (SessionHasStats())
	{
		// Sort and filter player stats
		PreprocessPlayersByScore();
		// Get the player count remaining after filtering
		INT PlayerCount = PendingStats.Num();
		// Skip processing if there is no data to report
		if (PlayerCount > 0)
		{
			// Figure out whether this was a team game or not
			INT TeamCount = GetTeamCount();
			UBOOL bNoTeams = (TeamCount == 0 || PlayerCount == TeamCount);
			SCReportPtr ReportHandle = NULL;
			// Create the report object that we'll add data to
			Result = scCreateReport(SCHandle,
				StatsVersion,
				PlayerCount,
				bNoTeams ? 0 : TeamCount,
				&ReportHandle);
			debugf(NAME_DevOnline,TEXT("scCreateReport(,%d,%d,%d,) returned 0x%08X"),
				StatsVersion,
				PlayerCount,
				bNoTeams ? 0 : TeamCount,
				(DWORD)Result);
			if (Result == SCResult_NO_ERROR)
			{
				FOnlineAsyncTaskGameSpySubmitStats* AsyncTask = new FOnlineAsyncTaskGameSpySubmitStats(ReportHandle,&FlushOnlineStatsDelegates);
				// Fill the report
				scReportBeginGlobalData(ReportHandle);
				// Add all of the per player data to the report
				if (AppendPlayersToStatsReport(ReportHandle,bNoTeams) == S_OK)
				{
					// Team data must be present
					scReportBeginTeamData(ReportHandle);
					const gsi_bool Authoritative = gsi_true;
					const SCGameStatus GameStatus = SCGameStatus_COMPLETE;
					Result = scReportEnd(ReportHandle, Authoritative, GameStatus);
					debugf(NAME_DevOnline,TEXT("scReportEnd() returned 0x%08X"),(DWORD)Result);
					if (Result == SCResult_NO_ERROR)
					{
						// Submit the report
						const INT Timeout = 0;
						Result = scSubmitReport(SCHandle,
							ReportHandle,
							Authoritative,
							LoginCertificate,
							LoginPrivateData,
							(SCSubmitReportCallback)AsyncTask->SubmitStatsReportCallback,
							Timeout,
							AsyncTask);
						debugf(NAME_DevOnline,TEXT("scSubmitReport() returned 0x%08X"),(DWORD)Result);
						if (Result == SCResult_NO_ERROR)
						{
							AsyncTasks.AddItem(AsyncTask);
							Return = ERROR_IO_PENDING;
						}
						else
						{
							Return = E_FAIL;
							debugf(NAME_DevOnline,TEXT("Failed to submit the stats report"));
						}
					}
					else
					{
						Return = E_FAIL;
						debugf(NAME_DevOnline,TEXT("Failed to end the stats report"));
					}
				}
				else
				{
					Return = E_FAIL;
					debugf(NAME_DevOnline,TEXT("Failed to append players to the stats report"));
				}
				// If the report failed and it's valid, clean it up
				if (Return != ERROR_IO_PENDING)
				{
					// This will free the report data
					delete AsyncTask;
				}
			}
			else
			{
				Return = E_FAIL;
				debugf(NAME_DevOnline,TEXT("Failed to create the stats report"));
			}
		}
		else
		{
			Return = S_OK;
			debugf(NAME_DevOnline,TEXT("No players present to report data for"));
		}
	}
	if (Return != ERROR_IO_PENDING)
	{
		FAsyncTaskDelegateResultsNamedSession Params(FName(TEXT("Game")),Return);
		TriggerOnlineDelegates(this,FlushOnlineStatsDelegates,&Params);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Searches for a player's pending stats, returning them if they exist, or adding them if they don't
 *
 * @param Player the player to find/add stats for
 *
 * @return the existing/new stats for the player
 */
FPendingPlayerStats& UOnlineSubsystemGameSpy::FindOrAddPendingPlayerStats(const FUniqueNetId& Player)
{
	// First check if this player already has pending stats
	for(INT Index = 0; Index < PendingStats.Num(); Index++)
	{
		if (PendingStats(Index).Player.ToDWORD() == Player.ToDWORD())
		{
			return PendingStats(Index);
		}
	}
	// This player doesn't have any stats, add them to the array
	INT Index = PendingStats.AddZeroed();
	FPendingPlayerStats& PlayerStats = PendingStats(Index);
	PlayerStats.Player = Player;
	// Use the stat guid from the player struct if a client, otherwise use the host's id
	if (PlayerStats.Player.ToDWORD() == LoggedInPlayerId.ToDWORD())
	{
		PlayerStats.StatGuid = ANSI_TO_TCHAR((const gsi_u8*)scGetConnectionId(SCHandle));
	}
	return PlayerStats;
}

/**
 * Called to get the GameSpy stats key id based on a view and property
 *
 * @param ViewId the stat view
 * @param PropertyId the stat property
 *
 * @return the gamespy stat key id
 */
INT UOnlineSubsystemGameSpy::StatKeyLookup(INT ViewId, INT PropertyId)
{
	for (INT Index = 0; Index < StatsKeyMappings.Num(); Index++)
	{
		FViewPropertyToKeyId& Mapping = StatsKeyMappings(Index);
		if ((Mapping.ViewId == ViewId) && (Mapping.PropertyId == PropertyId))
		{
			return Mapping.KeyId;
		}
	}
	// This stat isn't in the mapping
	return 0;
}

void UOnlineSubsystemGameSpy::AddOrUpdatePlayerStat(TArray<FPlayerStat>& PlayerStats, INT KeyId, const FSettingsData& Data)
{
	for (INT StatIndex = 0; StatIndex < PlayerStats.Num(); StatIndex++)
	{
		if (PlayerStats(StatIndex).KeyId == KeyId)
		{
			PlayerStats(StatIndex).Data = Data;
			return;
		}
	}
	INT AddIndex = PlayerStats.AddZeroed();
	FPlayerStat& NewPlayer = PlayerStats(AddIndex);
	NewPlayer.KeyId = KeyId;
	NewPlayer.Data = Data;
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
UBOOL UOnlineSubsystemGameSpy::WriteOnlineStats(FName SessionName,FUniqueNetId Player,
	UOnlineStatsWrite* StatsWrite)
{
	// Skip processing if the server isn't logged in or if the game type is wrong
	if (SessionHasStats())
	{
		// Ignore unknown players
		if (Player.ToDWORD() != 0)
		{
			FPendingPlayerStats& PendingPlayerStats = FindOrAddPendingPlayerStats(Player);
			// Get a ref to the view ids
			const TArrayNoInit<INT>& ViewIds = StatsWrite->ViewIds;
			INT NumIndexes = ViewIds.Num();
			INT NumProperties = StatsWrite->Properties.Num();
			for (INT ViewIndex = 0; ViewIndex < NumIndexes; ViewIndex++)
			{
				INT ViewId = ViewIds(ViewIndex);
				for (INT PropertyIndex = 0; PropertyIndex < NumProperties; PropertyIndex++)
				{
					const FSettingsProperty& Property = StatsWrite->Properties(PropertyIndex);
					INT KeyId = StatKeyLookup(ViewId, Property.PropertyId);
					if (KeyId != 0)
					{
						AddOrUpdatePlayerStat(PendingPlayerStats.Stats, KeyId, Property.Data);
					}
				}
				// Add a stat for the view itself
				INT KeyId = StatKeyLookup(ViewId, 0);
				if (KeyId != 0)
				{
					FSettingsData Data(EC_EventParm);
					Data.SetData((INT)1);
					AddOrUpdatePlayerStat(PendingPlayerStats.Stats, KeyId, Data);
				}
				else
				{	
					warnf(NAME_DevOnline,
						TEXT("StatViewId %d has no GameSpy mapping for session (%s)"),
						ViewId,
						*SessionName.ToString());
				}
			}
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("WriteOnlineStats: Ignoring unknown player"));
		}
	}
	return TRUE;
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
UBOOL UOnlineSubsystemGameSpy::WriteOnlinePlayerScores(FName SessionName,INT LeaderboardId,const TArray<FOnlinePlayerScore>& PlayerScores)
{
	// Skip processing if the server isn't logged in
	if (SessionHasStats())
	{
		INT NumScores = PlayerScores.Num();
		for (INT Index = 0; Index < NumScores; Index++)
		{
			const FOnlinePlayerScore& Score = PlayerScores(Index);
			// Don't record scores for bots
			if (Score.PlayerID.ToDWORD() != 0)
			{
				FPendingPlayerStats& PendingPlayerStats = FindOrAddPendingPlayerStats(Score.PlayerID);
				PendingPlayerStats.Score = Score;
			}
		}
	}
	return TRUE;
}

/**
 * Commits any changes in the online stats cache to the permanent storage
 *
 * @param SessionName the name of the session flushing stats
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::FlushOnlineStats(FName SessionName)
{
	// Skip processing if the server isn't logged in
	if (SessionHasStats())
	{
		if (PendingStats.Num() > 0)
		{
			CreateAndSubmitStatsReport();
			PendingStats.Empty();
		}
	}
	return TRUE;
}

/**
 * Reads the host's stat guid for synching up stats. Only valid on the host.
 *
 * @return the host's stat guid
 */
FString UOnlineSubsystemGameSpy::GetHostStatGuid()
{
	if (SCHandle != NULL)
	{
		const char* SessionId = scGetSessionId(SCHandle);
		return FString(SessionId);
	}
	return FString();
}

/**
 * Registers the host's stat guid with the client for verification they are part of
 * the stat. Note this is an async task for any backend communication that needs to
 * happen before the registration is deemed complete
 *
 * @param HostStatGuid the host's stat guid
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::RegisterHostStatGuid(const FString& HostStatGuid)
{
	//TODO: do we need to call the delegate here if the call fails?
	if (SCHandle == NULL)
	{
		//TODO: handle error;
		return FALSE;
	}
	return ClientSetStatsReportIntention((const gsi_u8*)TCHAR_TO_ANSI(*HostStatGuid));
}

/**
 * Reads the client's stat guid that was generated by registering the host's guid
 * Used for synching up stats. Only valid on the client. Only callable after the
 * host registration has completed
 *
 * @return the client's stat guid
 */
FString UOnlineSubsystemGameSpy::GetClientStatGuid()
{
	if (SCHandle != NULL)
	{
		const char* ConnectionId = scGetConnectionId(SCHandle);
		return FString(ConnectionId);
	}
	return FString();
}

/**
 * Registers the client's stat guid on the host to validate that the client was in the stat.
 * Used for synching up stats. Only valid on the host.
 *
 * @param PlayerId the client's unique net id
 * @param ClientStatGuid the client's stat guid
 *
 * @return TRUE if the call is successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::RegisterStatGuid(FUniqueNetId PlayerID,const FString& ClientStatGuid)
{
	if (ClientStatGuid.Len())
	{
		FPendingPlayerStats& PendingPlayerStats = FindOrAddPendingPlayerStats(PlayerID);
		PendingPlayerStats.StatGuid = ClientStatGuid;
		return TRUE;
	}
	else
	{
		debugf(NAME_DevOnline,
			TEXT("Player (%s) is missing their StatGuid, no stats are recorded for them"),
			*GetPlayerName(PlayerID));
	}
	return FALSE;
}

/**
 * Determines whether the user's profile file exists or not
 */
UBOOL UOnlineSubsystemGameSpy::DoesProfileExist(void)
{
#if !PS3
	UBOOL bExists = FALSE;
	if (LoggedInPlayerName.Len())
	{
		// try to open the profile
		FArchive* Profile = GFileManager->CreateFileReader(*CreateProfileName(), FILEREAD_SaveGame);
		// if exists if it succeeded
		bExists = Profile != NULL;
		// toss it
		delete Profile;
	}
	// return success
	return bExists;
#else
	// Too slow to check so always expect it to be there
	return TRUE;
#endif
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
UBOOL UOnlineSubsystemGameSpy::ReadProfileSettings(BYTE LocalUserNum,
	UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = E_FAIL;
	// Only read the data for the logged in player
	if (LocalUserNum == LoggedInPlayerNum)
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
						TArray<BYTE> Buffer;
						if (appLoadFileToArray(Buffer,*CreateProfileName(),GFileManager,FILEREAD_SaveGame))
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
					// Only do GameSpy request if they are an online account
					if (LoggedInStatus > LS_UsingLocalProfile)
					{
						// Request an async read from the GameSpy server
						SakeRequestProfile();
						Return = ERROR_IO_PENDING;
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
		check(LocalUserNum < 4);
		OnlineSubsystemGameSpy_eventOnReadProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (Return == 0) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = LocalUserNum;
		// Use the common method to do the work
		TriggerOnlineDelegates(this,PerUserReadProfileSettings[LocalUserNum].Delegates,&Parms);
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
UBOOL UOnlineSubsystemGameSpy::WriteProfileSettings(BYTE LocalUserNum,
	UOnlineProfileSettings* ProfileSettings)
{
	DWORD Return = E_FAIL;
	// Only the logged in user can write their profile data
	if (LocalUserNum == LoggedInPlayerNum)
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
				FOnlineAsyncTaskGameSpyWriteProfile* AsyncTask = new FOnlineAsyncTaskGameSpyWriteProfile();
				// Write the data using an async task
				if (AsyncTask->WriteData(*CreateProfileName(),CachedProfile))
				{
					AsyncTasks.AddItem(AsyncTask);
					Return = ERROR_IO_PENDING;
				}
				else
				{
					delete AsyncTask;
					Return = E_FAIL;
				}
				if (Return != S_OK && Return != ERROR_IO_PENDING)
				{
					debugf(NAME_DevOnline,TEXT("Failed to write profile data for %s to Sake"),*LoggedInPlayerName);
					// Remove the write state so that subsequent writes work
					CachedProfile->AsyncState = OPAS_Finished;
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
		OnlineSubsystemGameSpy_eventOnWriteProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (Return == 0) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = LoggedInPlayerNum;
		TriggerOnlineDelegates(this,WriteProfileSettingsDelegates,&Parms);
	}
	return Return == S_OK || Return == ERROR_IO_PENDING;
}

/**
 * Based upon logged in status, decides if it needs to write to Sake or not.
 * Called after local writing has completed. If writing remote, it starts the
 * write. Otherwise it fires the delegates as if it is done
 *
 * @param Result the result code from the write
 * @param Profile the profile data to write
 * @param Length the number of bytes of data to write
 */
void UOnlineSubsystemGameSpy::ConditionallyWriteProfileToSake(DWORD Result,const BYTE* Profile,DWORD Length)
{
	if (Result == S_OK)
	{
		// Only do the GameSpy write if they are online
		if (LoggedInStatus > LS_UsingLocalProfile)
		{
			// We're still waiting on Sake to update
			Result = ERROR_IO_PENDING;
			// Update the profile in Sake
			if (SakeUpdateProfile(Profile,Length) == FALSE)
			{
				// Couldn't write for some reason
				Result = E_FAIL;
			}
		}
	}
	if (Result != ERROR_IO_PENDING)
	{
		// Remove the write state so that subsequent writes work
		CachedProfile->AsyncState = OPAS_Finished;
		// Send the notification of completion
		OnlineSubsystemGameSpy_eventOnWriteProfileSettingsComplete_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = (Result == 0) ? FIRST_BITFIELD : 0;
		Parms.LocalUserNum = LoggedInPlayerNum;
		TriggerOnlineDelegates(this,WriteProfileSettingsDelegates,&Parms);
	}
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
UBOOL UOnlineSubsystemGameSpy::ReadFriendsList(BYTE LocalUserNum,INT Count,INT StartingAt)
{
	DWORD Return = E_FAIL;
	if (LocalUserNum == LoggedInPlayerNum)
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
BYTE UOnlineSubsystemGameSpy::GetFriendsList(BYTE LocalUserNum,TArray<FOnlineFriend>& Friends,INT Count,INT StartingAt)
{
	// If we're not logged in return failure before checking the GPHandle.
	if (GetLoginStatus(LocalUserNum) == LS_NotLoggedIn)
	{
		debugf(NAME_DevOnline,TEXT("GetFriendsList: Not logged in."));
		return OERS_Failed;
	}

	// Empty the existing list so dupes don't happen
	Friends.Empty(Friends.Num());
	check(GPHandle);
	GPResult Result;
	INT NumBuddies;
	Result = gpGetNumBuddies(&GPHandle, &NumBuddies);
	if (Result != GP_NO_ERROR)
	{
		//todo: handle error
		return OERS_Failed;
	}
	if (Count == 0)
	{
		Count = NumBuddies;
	}
	for (INT Index = 0; Index < Count; Index++)
	{
		GPBuddyStatus BuddyStatus;
		Result = gpGetBuddyStatus(&GPHandle, StartingAt + Index, &BuddyStatus);
		if(Result != GP_NO_ERROR)
		{
			//todo: handle error
			continue;
		}
		GPGetInfoResponseArg BuddyInfo;
		Result = gpGetInfoNoWait(&GPHandle, BuddyStatus.profile, &BuddyInfo);
		if(Result != GP_NO_ERROR)
		{
			//todo: handle error
			continue;
		}
		if(BuddyInfo.uniquenick[0] == (UCS2Char)0)
		{
			// this user doesn't have a uniquenick
			// the regular nick could be used, but it not being unique could be a problem
			//TODO: should this use the regular nick?
			continue;
		}
		INT FriendIndex = Friends.AddZeroed();
		FOnlineFriend& Friend = Friends(FriendIndex);
		Friend.UniqueId = (DWORD)BuddyStatus.profile;
		Friend.NickName = (wchar_t*)BuddyInfo.uniquenick;
		Friend.PresenceInfo = (wchar_t*)BuddyStatus.locationString;
		Friend.bIsOnline = (BuddyStatus.status != GP_OFFLINE);
		Friend.bIsPlaying = (BuddyStatus.status == GP_PLAYING);
		Friend.bIsPlayingThisGame = IsJoinableLocationString((const TCHAR*)BuddyStatus.locationString);
		if (Friend.bIsPlayingThisGame)
		{
			// Build an URL so we can parse options
			FURL Url(NULL,(const TCHAR*)BuddyStatus.locationString,TRAVEL_Absolute);
			// Get our current location to prevent joining the game we are already in
			const FString CurrentLocation = CachedGameInt && CachedGameInt->SessionInfo ?
				CachedGameInt->SessionInfo->HostAddr.ToString(FALSE) :
				TEXT("");
			// Parse the host address to see if they are joinable
			if (Url.Host.Len() > 0 &&
				appStrstr((const TCHAR*)BuddyStatus.locationString,TEXT("Standalone")) == NULL &&
				appStrstr((const TCHAR*)BuddyStatus.locationString,TEXT("bIsLanMatch")) == NULL &&
				(CurrentLocation.Len() == 0 || appStrstr((const TCHAR*)BuddyStatus.locationString,*CurrentLocation) == NULL))
			{
				UBOOL bIsValid;
				FInternetIpAddr HostAddr;
				// Set the IP address listed and see if it's valid
				HostAddr.SetIp(*Url.Host,bIsValid);
				Friend.bIsJoinable = bIsValid;
			}
			Friend.bHasVoiceSupport = Url.HasOption(TEXT("bHasVoice"));
		}
	}
	return OERS_Done;
}

/**
 * Creates a network enabled account on the online service
 *
 * @param UserName the unique nickname of the account
 * @param Password the password securing the account
 * @param EmailAddress the address used to send password hints to
 * @param ProductKey the unique id for this installed product
 *
 * @return true if the account was created, false otherwise
 */
UBOOL UOnlineSubsystemGameSpy::CreateOnlineAccount(const FString& UserName,const FString& Password,const FString& EmailAddress,const FString& ProductKey)
{
	DWORD Result = E_FAIL;
	if (GPHandle)
	{
		BYTE CDKey[20];
		DWORD CDKeyLen = 19;
		appMemzero(CDKey,CDKeyLen + 1);
#if WANTS_CD_KEY_AUTH
		// Decrypt the key into the buffer
		if (DecryptProductKey(CDKey,CDKeyLen))
#endif
		{
			// Verify the strings are correct lengths
			if (UserName.Len() < GP_NICK_LEN &&
				UserName.Len() < GP_UNIQUENICK_LEN &&
				Password.Len() < GP_PASSWORD_LEN &&
				EmailAddress.Len() < GP_EMAIL_LEN)
			{
				FString LocProductKey = (ANSICHAR*)CDKey;
				// Create the async task that is going to hold the data
				FOnlineAsyncTaskGameSpyCreateNewUser* AsyncTask = new FOnlineAsyncTaskGameSpyCreateNewUser(UserName,EmailAddress,Password,LocProductKey);
				// Create account with the cd key and let the async task handle the processing
				GPResult gpResult = gpNewUser(&GPHandle,
		(const UCS2String)*UserName,
		(const UCS2String)*UserName,
		(const UCS2String)*EmailAddress,
		(const UCS2String)*Password,
					(const UCS2String)*LocProductKey,
		GP_NON_BLOCKING,
					(GPCallback)AsyncTask->_GPNewUserCallback,
					AsyncTask);
				debugf(NAME_DevOnline,TEXT("gpNewUser() returned 0x%08X"),(DWORD)gpResult);
				if (gpResult == GP_NO_ERROR)
	{
					Result = ERROR_IO_PENDING;
					// Add the async task to be ticked
					AsyncTasks.AddItem(AsyncTask);
				}
				else
		{
					Result = gpResult;
					delete AsyncTask;
				}
				appMemzero(CDKey,20);
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("CreateOnlineAccount: One or more of the strings are too long"));
			}
		}
	}
	else
	{
		Result = E_POINTER;
	}
	if (Result != ERROR_IO_PENDING)
	{
		// Trigger the delegates so the UI can process
		FAccountCreateResults Params(Result);
		TriggerOnlineDelegates(this,AccountCreateDelegates,&Params);
	}
	return Result == S_OK || Result == ERROR_IO_PENDING;
}

/**
 * Processes any talking delegates that need to be fired off
 */
void UOnlineSubsystemGameSpy::ProcessTalkingDelegates(void)
{
	// Skip all delegate handling if none are registered
	if (PlayerTalkingDelegates.Num() > 0)
	{
		// Only check players with voice
		if (CurrentLocalTalker.bHasVoice &&
			(CurrentLocalTalker.bWasTalking != CurrentLocalTalker.bIsTalking))
		{
			OnlineSubsystemGameSpy_eventOnPlayerTalkingStateChange_Parms Parms(EC_EventParm);
			// Use the cached id for this
			Parms.Player = LoggedInPlayerId;
			Parms.bIsTalking = CurrentLocalTalker.bIsTalking;
			TriggerOnlineDelegates(this,PlayerTalkingDelegates,&Parms);
			// Clear the flag so it only activates when needed
			CurrentLocalTalker.bWasTalking = CurrentLocalTalker.bIsTalking;
		}
		// Now check all remote talkers
		for (INT Index = 0; Index < RemoteTalkers.Num(); Index++)
		{
			FRemoteTalker& Talker = RemoteTalkers(Index);
			if (Talker.bWasTalking != Talker.bIsTalking)
			{
				OnlineSubsystemGameSpy_eventOnPlayerTalkingStateChange_Parms Parms(EC_EventParm);
				Parms.Player = Talker.TalkerId;
				Parms.bIsTalking = Talker.bIsTalking;
				TriggerOnlineDelegates(this,PlayerTalkingDelegates,&Parms);
				// Clear the flag so it only activates when needed
				Talker.bWasTalking = Talker.bIsTalking;
			}
		}
	}
}

/**
 * Processes any speech recognition delegates that need to be fired off
 */
void UOnlineSubsystemGameSpy::ProcessSpeechRecognitionDelegates(void)
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
UBOOL UOnlineSubsystemGameSpy::RegisterLocalTalker(BYTE LocalUserNum)
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
UBOOL UOnlineSubsystemGameSpy::UnregisterLocalTalker(BYTE LocalUserNum)
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
UBOOL UOnlineSubsystemGameSpy::RegisterRemoteTalker(FUniqueNetId UniqueId)
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
UBOOL UOnlineSubsystemGameSpy::UnregisterRemoteTalker(FUniqueNetId UniqueId)
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
 * Determines if the specified player is actively talking into the mic
 *
 * @param LocalUserNum the local player index being queried
 *
 * @return TRUE if the player is talking, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::IsLocalPlayerTalking(BYTE LocalUserNum)
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
UBOOL UOnlineSubsystemGameSpy::IsRemotePlayerTalking(FUniqueNetId UniqueId)
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
UBOOL UOnlineSubsystemGameSpy::IsHeadsetPresent(BYTE LocalUserNum)
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
UBOOL UOnlineSubsystemGameSpy::SetRemoteTalkerPriority(BYTE LocalUserNum,FUniqueNetId UniqueId,INT Priority)
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
UBOOL UOnlineSubsystemGameSpy::MuteRemoteTalker(BYTE LocalUserNum,FUniqueNetId UniqueId,UBOOL bIsSystemWide)
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
UBOOL UOnlineSubsystemGameSpy::UnmuteRemoteTalker(BYTE LocalUserNum,FUniqueNetId UniqueId,UBOOL bIsSystemWide)
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
void UOnlineSubsystemGameSpy::StartNetworkedVoice(BYTE LocalUserNum)
{
	if (LocalUserNum == LoggedInPlayerNum)
	{
		CurrentLocalTalker.bHasNetworkedVoice = TRUE;
		// Since we don't leave the capturing on all the time due to a GameSpy bug, enable it now
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
void UOnlineSubsystemGameSpy::StopNetworkedVoice(BYTE LocalUserNum)
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
UBOOL UOnlineSubsystemGameSpy::StartSpeechRecognition(BYTE LocalUserNum)
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
UBOOL UOnlineSubsystemGameSpy::StopSpeechRecognition(BYTE LocalUserNum)
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
UBOOL UOnlineSubsystemGameSpy::GetRecognitionResults(BYTE LocalUserNum,TArray<FSpeechRecognizedWord>& Words)
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
UBOOL UOnlineSubsystemGameSpy::SelectVocabulary(BYTE LocalUserNum,INT VocabularyId)
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
UBOOL UOnlineSubsystemGameSpy::SetSpeechRecognitionObject(BYTE LocalUserNum,USpeechRecognition* SpeechRecogObj)
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
void UOnlineSubsystemGameSpy::SetOnlineStatus(BYTE LocalUserNum,INT StatusId,
	const TArray<FLocalizedStringSetting>& LocalizedStringSettings,
	const TArray<FSettingsProperty>& Properties)
{
	if (LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LocalUserNum) == LS_LoggedIn)
	{
		FString Location = GetServerLocation();
		// Append their voice status
		if (VoiceEngine)
		{
			Location += TEXT("?bHasVoice");
		}
		FString Status;
		if (StatusMappings.Num())
		{
			// Find the status in the status mappings array
			for (INT StatusIndex = 0; StatusIndex < StatusMappings.Num(); StatusIndex++)
			{
				if (StatusMappings(StatusIndex).StatusId == StatusId)
				{
					Status = StatusMappings(StatusIndex).StatusString;
				}
			}
		}
		// Use the default if one wasn't found
		if (Status.Len() == 0)
		{
			Status = DefaultStatus;
		}
		// Now set the status that gp will report
		GPResult Result = gpSetStatus(&GPHandle,GP_PLAYING,(const UCS2String)*Status,(const UCS2String)*Location);
		debugf(NAME_DevOnline,TEXT("gpSetStatus(%s,%s) returned 0x%08X"),*Status,*Location,(DWORD)Result);
	}
}

/**
 * Displays the UI that shows the keyboard for inputing text
 *
 * @param LocalUserNum the controller number of the associated user
 * @param TitleText the title to display to the user
 * @param DescriptionText the text telling the user what to input
 * @param bIsPassword whether the item being entered is a password or not
 * @param bShouldValidate whether to apply the string validation API after input or not
 * @param DefaultText the default string to display
 * @param MaxResultLength the maximum length string expected to be filled in
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemGameSpy::ShowKeyboardUI(BYTE LocalUserNum,const FString& TitleText,
	const FString& DescriptionText,UBOOL bIsPassword,UBOOL bShouldValidate,
	const FString& DefaultText,INT MaxResultLength)
{
#if PS3
	UBOOL bReturn = appPS3ShowVirtualKeyboard(*DescriptionText,*DefaultText,MaxResultLength,bIsPassword);
	if (bReturn)
	{
		// Reset these for ticking to inspect
		bNeedsKeyboardTicking = TRUE;
		bWasKeyboardInputCanceled = FALSE;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Failed to show the keyboard"));
		// Trigger the delegates as failed
		FAsyncTaskDelegateResults Parms(E_FAIL);
		// Use the common method to do the work
		TriggerOnlineDelegates(this,KeyboardInputDelegates,&Parms);
	}
	return bReturn;
#else
	return FALSE;
#endif
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
UBOOL UOnlineSubsystemGameSpy::AddFriend(BYTE LocalUserNum,FUniqueNetId NewFriend,const FString& Message)
{
	if (LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_NotLoggedIn)
	{
#if PS3
		debugf(NAME_DevOnline, TEXT("Adding friends must be done using the XMB on PS3"));
		return FALSE;
#else
		// Only send the invite if they aren't on the friend's list already
		if (gpIsBuddy(&GPHandle,NewFriend.ToDWORD()) == FALSE)
		{
			GPResult Result = gpSendBuddyRequest(&GPHandle,NewFriend.ToDWORD(),(const UCS2String)*Message);
			debugf(NAME_DevOnline,TEXT("gpSendBuddyRequest(,%d,'%s') returned 0x%08X"),
				NewFriend.ToDWORD(),*Message,(DWORD)Result);
			return Result == GP_NO_ERROR;
		}
		return TRUE;
#endif
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
UBOOL UOnlineSubsystemGameSpy::AddFriendByName(BYTE LocalUserNum,const FString& FriendName,const FString& Message)
{
	if (LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_UsingLocalProfile)
	{
#if PS3 
		debugf(NAME_DevOnline, TEXT("Adding friends must be done using the XMB on PS3"));
		return FALSE;
#else
		// Store the message since the request happens async
		CachedFriendMessage = Message;
		// Kick off the search
		GPResult Result = gpProfileSearch(&GPHandle,NULL,(const UCS2String)*FriendName,NULL,NULL,NULL,0,
			GP_NON_BLOCKING,(GPCallback)::GPProfileSearchCallback,this);
		debugf(NAME_DevOnline,TEXT("gpProfileSearch('%s') returned 0x%08X"),
			*FriendName,(DWORD)Result);
		return Result == GP_NO_ERROR;
#endif
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid or not logged in player specified (%d)"),(DWORD)LocalUserNum);
	}
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
UBOOL UOnlineSubsystemGameSpy::RemoveFriend(BYTE LocalUserNum,FUniqueNetId FormerFriend)
{
	if (LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_UsingLocalProfile)
	{
#if PS3 
		debugf(NAME_DevOnline, TEXT("Removing friends must be done using the XMB on PS3"));
		return FALSE;
#else
		GPResult Result = gpDeleteBuddy(&GPHandle,FormerFriend.ToDWORD());
		debugf(NAME_DevOnline,TEXT("gpDeleteBuddy(%d) returned 0x%08X"),
			FormerFriend.ToDWORD(),(DWORD)Result);
		if (Result == GP_NO_ERROR)
		{
			Result = gpRevokeBuddyAuthorization(&GPHandle,FormerFriend.ToDWORD());
			debugf(NAME_DevOnline,TEXT("gpRevokeBuddyAuthorization(%d) returned 0x%08X"),
				FormerFriend.ToDWORD(),(DWORD)Result);
		}
		return Result == GP_NO_ERROR;
#endif
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
UBOOL UOnlineSubsystemGameSpy::AcceptFriendInvite(BYTE LocalUserNum,FUniqueNetId NewFriend)
{
	if (LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_UsingLocalProfile)
	{
		GPResult Result = gpAuthBuddyRequest(&GPHandle,NewFriend.ToDWORD());
		debugf(NAME_DevOnline,TEXT("gpAuthBuddyRequest(%d) returned 0x%08X"),
			NewFriend.ToDWORD(),(DWORD)Result);
		// If they are a recent invite, mark them as being accepted
		FOnlineFriendMessage* Invite = FindFriendInvite(NewFriend);
		if (Invite != NULL)
		{
			Invite->bWasAccepted = TRUE;
			Invite->bWasDenied = FALSE;
		}
		return Result == GP_NO_ERROR;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid or not logged in player specified (%d)"),(DWORD)LocalUserNum);
	}
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
UBOOL UOnlineSubsystemGameSpy::DenyFriendInvite(BYTE LocalUserNum,FUniqueNetId RequestingPlayer)
{
	if (LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_UsingLocalProfile)
	{
		GPResult Result = gpDenyBuddyRequest(&GPHandle,RequestingPlayer.ToDWORD());
		debugf(NAME_DevOnline,TEXT("gpDenyBuddyRequest(%d) returned 0x%08X"),
			RequestingPlayer.ToDWORD(),(DWORD)Result);
		// If the friend we just denied is on our list, remove them
		if (Result == GP_NO_ERROR && gpIsBuddy(&GPHandle,RequestingPlayer.ToDWORD()))
		{
			Result = gpDeleteBuddy(&GPHandle,RequestingPlayer.ToDWORD());
			debugf(NAME_DevOnline,TEXT("gpDeleteBuddy(,%d,) returned 0x%08X"),
				RequestingPlayer.ToDWORD(),(DWORD)Result);
		}
		// If they are a recent invite, mark them as being denied
		FOnlineFriendMessage* Invite = FindFriendInvite(RequestingPlayer);
		if (Invite != NULL)
		{
			Invite->bWasAccepted = FALSE;
			Invite->bWasDenied = TRUE;
		}
		return Result == GP_NO_ERROR;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid or not logged in player specified (%d)"),(DWORD)LocalUserNum);
	}
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
UBOOL UOnlineSubsystemGameSpy::SendMessageToFriend(BYTE LocalUserNum,FUniqueNetId Friend,const FString& Message)
{
	if (LocalUserNum == LoggedInPlayerNum && GetLoginStatus(LoggedInPlayerNum) > LS_UsingLocalProfile)
	{
		GPResult Result = gpSendBuddyMessage(&GPHandle,Friend.ToDWORD(),(const UCS2String)*Message);
		debugf(NAME_DevOnline,TEXT("gpSendBuddyMessage(%d,'%s') returned 0x%08X"),
			Friend.ToDWORD(),*Message,(DWORD)Result);
		return Result == GP_NO_ERROR;
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
 * @param Text ignored in GameSpy
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemGameSpy::SendGameInviteToFriend(BYTE LocalUserNum,FUniqueNetId Friend,const FString&)
{
	if (CachedGameInt->GameSettings != NULL &&
		CachedGameInt->GameSettings->bIsLanMatch == FALSE &&
		CachedGameInt->GameSettings->bUsesArbitration == FALSE)
	{
		// Set the server info if available
		FString Location = GetServerLocation();
		// Send them an invite to this game
		GPResult Result = gpInvitePlayer(&GPHandle,Friend.ToDWORD(),
			ProductID,
			(const UCS2String)*Location);
		debugf(NAME_DevOnline,TEXT("gpInvitePlayer(,%d,%s) returned 0x%08X"),
			ProductID,*Location,(DWORD)Result);
		return Result == GP_NO_ERROR;
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Can't invite someone to game that isn't Internet available"));
	}
	return FALSE;
}

/**
 * Sends invitations to play in the player's current session
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friends the player to send the invite to
 * @param Text ignored in GameSpy
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemGameSpy::SendGameInviteToFriends(BYTE LocalUserNum,const TArray<FUniqueNetId>& Friends,const FString& Text)
{
	// Just call the individual method for each player in the list
	for (INT Index = 0; Index < Friends.Num(); Index++)
	{
		if (SendGameInviteToFriend(LocalUserNum,Friends(Index),Text) == FALSE)
		{
			// Quit sending if it fails
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Attempts to join a friend's game session (join in progress)
 *
 * @param LocalUserNum the user that is sending the invite
 * @param Friends the player to send the invite to
 *
 * @return true if successful, false otherwise
 */
UBOOL UOnlineSubsystemGameSpy::JoinFriendGame(BYTE LocalUserNum,FUniqueNetId Friend)
{
	INT Index = -1;
	// Get the current info for the player, so we can join the right location
	GPResult Result = gpGetBuddyIndex(&GPHandle,Friend.ToDWORD(),&Index);
	if (Result == GP_NO_ERROR && Index > -1)
	{
		GPBuddyStatus BuddyStatus;
		// Read the buddy's status for following them to their current server
		Result = gpGetBuddyStatus(&GPHandle,Index,&BuddyStatus);
		if (Result == GP_NO_ERROR)
		{
			// Get our current location to prevent joining the game we are already in
			const FString CurrentLocation = CachedGameInt && CachedGameInt->SessionInfo ?
				CachedGameInt->SessionInfo->HostAddr.ToString(FALSE) :
				TEXT("");
			// Make sure this is our game, networked, not a lan match, and not a match we are in
			if (IsJoinableLocationString((const TCHAR*)BuddyStatus.locationString) &&
				appStrstr((const TCHAR*)BuddyStatus.locationString,TEXT("Standalone")) == NULL &&
				appStrstr((const TCHAR*)BuddyStatus.locationString,TEXT("bIsLanMatch")) == NULL &&
				(CurrentLocation.Len() == 0 || appStrstr((const TCHAR*)BuddyStatus.locationString,*CurrentLocation) == NULL))
			{
				// Store this for when they accept it later
				CachedGameInt->SetInviteInfo((const TCHAR*)BuddyStatus.locationString);
				// Indicate that the search for the friend game has completed
				FAsyncTaskDelegateResultsNamedSession JoinParms(FName(TEXT("Game")),S_OK);
				TriggerOnlineDelegates(this,JoinFriendGameCompleteDelegates,&JoinParms);
				// Now fire off the autoinvite delegates
				OnlineGameInterfaceImpl_eventOnGameInviteAccepted_Parms Parms(EC_EventParm);
				if (CachedGameInt->InviteGameSearch != NULL &&
					CachedGameInt->InviteGameSearch->Results.Num() > 0)
				{
					Parms.InviteResult = CachedGameInt->InviteGameSearch->Results(0);
				}
				// Use the helper method to fire the delegates
				TriggerOnlineDelegates(CachedGameInt,CachedGameInt->GameInviteAcceptedDelegates,&Parms);
				return TRUE;
			}
			else
			{
				debugf(NAME_DevOnline,TEXT("The selected friend's game is not joinable"));
				// Lie and say that it worked
				FAsyncTaskDelegateResultsNamedSession JoinParms(FName(TEXT("Game")),S_OK);
				TriggerOnlineDelegates(this,JoinFriendGameCompleteDelegates,&JoinParms);
				return TRUE;
			}
		}
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Couldn't find friend for joining"));
	}
	// Trigger them as failed
	FAsyncTaskDelegateResultsNamedSession Parms(FName(TEXT("Game")),E_FAIL);
	TriggerOnlineDelegates(this,JoinFriendGameCompleteDelegates,&Parms);
	return FALSE;
}

/**
 * Reads any data that is currently queued in the voice interface
 */
void UOnlineSubsystemGameSpy::ProcessLocalVoicePackets(void)
{
	UNetDriver* NetDriver = GWorld->GetNetDriver();
	// Skip if the netdriver isn't present, as there's no network to route data on
	if (VoiceEngine != NULL)
	{
		// Only process the voice data if either network voice is enabled or
		// if the player is trying to have their voice issue commands
		if (CurrentLocalTalker.bHasNetworkedVoice ||
			CurrentLocalTalker.bIsRecognizingSpeech)
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
#if PS3
						// If the player has muted every one skip the processing
						if (NetDriver &&
							LoggedInStatus == LS_LoggedIn &&
							CurrentLocalTalker.bHasNetworkedVoice &&
							// Don't allow voice if they are restricted
							(NpData == NULL || (NpData && NpData->CanCommunicate())))
#else
						// If there is no net connection or they aren't allowed to transmit, skip processing
						if (NetDriver &&
							LoggedInStatus == LS_LoggedIn &&
							CurrentLocalTalker.bHasNetworkedVoice)
#endif
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
void UOnlineSubsystemGameSpy::ProcessRemoteVoicePackets(void)
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
#if PS3
				// If the player has muted every one skip the processing
				if (CurrentLocalTalker.MuteType < MUTE_All &&
					// Don't allow voice if they are restricted
					(NpData == NULL || (NpData && NpData->CanCommunicate())))
#else
				// If the player has muted every one skip the processing
				if (CurrentLocalTalker.MuteType < MUTE_All)
#endif
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
					if (bIsMuted == FALSE && PlayerTalkingDelegates.Num() > 0)
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
void UOnlineSubsystemGameSpy::RegisterLocalTalkers(void)
{
	RegisterLocalTalker(LoggedInPlayerNum);
}

/** Unregisters all of the local talkers from the voice engine */
void UOnlineSubsystemGameSpy::UnregisterLocalTalkers(void)
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
BYTE UOnlineSubsystemGameSpy::CanCommunicate(BYTE LocalUserNum)
{
#if PS3
	// Any non-logged in player can chat
	if (LocalUserNum == LoggedInPlayerNum)
	{
		// If they have an NP account, check the NP permission
		if (appDoesUserHaveNpAccount())
		{
			if (NpData)
			{
				return NpData->CanCommunicate() ? FPL_Enabled : FPL_Disabled;
			}
		}
	}
#endif
	return FPL_Enabled;
}

/**
 * Determines whether the player is allowed to play matches online
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return the Privilege level that is enabled
 */
BYTE UOnlineSubsystemGameSpy::CanPlayOnline(BYTE LocalUserNum)
{
#if PS3
	// Any non-logged in player can chat
	if (LocalUserNum == LoggedInPlayerNum)
	{
		// If they have an NP account, check the NP permission
		if (appDoesUserHaveNpAccount())
		{
			if (NpData)
			{
				return NpData->CanPlayOnline() ? FPL_Enabled : FPL_Disabled;
			}
		}
		return FPL_Enabled;
	}
#endif
	return FPL_Enabled;
}

/**
 * Determines if the specified controller is connected or not
 *
 * @param ControllerId the controller to query
 *
 * @return true if connected, false otherwise
 */
UBOOL UOnlineSubsystemGameSpy::IsControllerConnected(INT ControllerId)
{
#if PS3
	return appIsControllerPresent(ControllerId);
#else
	return TRUE;
#endif
}

/**
 * Determines if the ethernet link is connected or not
 */
UBOOL UOnlineSubsystemGameSpy::HasLinkConnection(void)
{
	return GSocketSubsystem->HasNetworkDevice();
}

/**
 * Displays the UI that prompts the user for their login credentials. Each
 * platform handles the authentication of the user's data.
 *
 * @param bShowOnlineOnly whether to only display online enabled profiles or not
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
UBOOL UOnlineSubsystemGameSpy::ShowLoginUI(UBOOL bShowOnlineOnly)
{
#if PS3
	if (LoggedInStatus < LS_LoggedIn)
	{
		// Kick off the PS3 network dialog
		AsyncTasks.AddItem(new FOnlineAsyncTaskHandleExternalLogin());
	}
	else
	{
		// Trigger the delegates so the UI can process
		OnlineSubsystemGameSpy_eventOnLoginChange_Parms Parms2(EC_EventParm);
		Parms2.LocalUserNum = 0;
		TriggerOnlineDelegates(this,LoginChangeDelegates,&Parms2);
	}
	return TRUE;
#else
	return FALSE;
#endif
}

/**
 * Decrypts the product key and places it in the specified buffer
 *
 * @param Buffer the output buffer that holds the key
 * @param BufferLen the size of the buffer to fill
 *
 * @return TRUE if the decyption was successful, FALSE otherwise
 */
UBOOL UOnlineSubsystemGameSpy::DecryptProductKey(BYTE* Buffer,DWORD BufferLen)
{
#if !PS3
	if (EncryptedProductKey.Len())
	{
		BYTE EncryptedBuffer[256];
		DWORD Size = EncryptedProductKey.Len() / 3;
		// We need to convert to a buffer from the string form
		if (appStringToBlob(EncryptedProductKey,EncryptedBuffer,Size))
		{
			// Now we need to decrypt that buffer so we can submit it
			if (appDecryptBuffer(EncryptedBuffer,Size,Buffer,BufferLen))
			{
				return TRUE;
			}
		}
	}
#endif
	return FALSE;
}

/**
 * Calculates a response string based off of the server's challenge
 *
 * @param Connection the connection that we are generating the response for
 * @param bIsReauth whether we are processing a reauth or initial auth request
 */
void UOnlineSubsystemGameSpy::GetChallengeResponse(UNetConnection* Connection,UBOOL bIsReauth)
{
#if WANTS_CD_KEY_AUTH
	// Don't check auth for lan games
	if (CachedGameInt &&
		CachedGameInt->GameSettings &&
		CachedGameInt->GameSettings->bIsLanMatch == FALSE)
	{
	BYTE CDKey[20];
	DWORD CDKeyLen = 19;
	appMemzero(CDKey,CDKeyLen + 1);
	// Decrypt the key into the buffer
	if (DecryptProductKey(CDKey,CDKeyLen))
	{
		char Response[73];
		// Calculate a response
		gcd_compute_response((char*)CDKey,
			TCHAR_TO_ANSI(*Connection->Challenge),
			Response,
			bIsReauth ? CDResponseMethod_REAUTH : CDResponseMethod_NEWAUTH);
		// Copy the response to the connection for matching up later
		Connection->ClientResponse = Response;
#if _DEBUG
		debugf(NAME_DevOnline,TEXT("Response generated is %s"),*Connection->ClientResponse);
#endif
			appMemzero(CDKey,20);
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid product key has been associated with this installation"));
	}
	}
	else
	{
		// Stick something in there
		Connection->ClientResponse = TEXT("0");
	}
#else
	// Stick something in there
	Connection->ClientResponse = TEXT("0");
#endif
}

/**
 * Sends an authorization request to GameSpy's backend
 *
 * @param Connection the connection that we are authorizing
 */
void UOnlineSubsystemGameSpy::SubmitAuthRequest(UNetConnection* Connection)
{
#if WANTS_CD_KEY_AUTH
	// Don't check auth for lan games
	if (CachedGameInt &&
		CachedGameInt->GameSettings &&
		CachedGameInt->GameSettings->bIsLanMatch == FALSE)
	{
		if (Connection->Challenge.Len() > 0 && Connection->ClientResponse.Len() > 0)
		{
			INT IpAddr = Connection->GetAddrAsInt();
			// Assign the current auth id and increment for the next request
			Connection->ResponseId = NextAuthId++;
			// Ask GameSpy if the user has a valid key
			gcd_authenticate_user(GameID,
				Connection->ResponseId,
				htonl((INT)IpAddr),
				TCHAR_TO_ANSI(*Connection->Challenge),
				TCHAR_TO_ANSI(*Connection->ClientResponse),
				::CDKeyAuthCallBack,
				::RefreshCDKeyAuthCallBack,
				this);
#if _DEBUG
			// Log in debug so we can see what is going on
			debugf(NAME_DevOnline,TEXT("gcd_authenticate_user(%d,%d,%d,%s,%s,,,)"),
				GameID,
				Connection->ResponseId,
				IpAddr,
				*Connection->Challenge,
				*Connection->ClientResponse);
#endif
		}
		else
		{
			debugf(NAME_DevOnline,TEXT("Closing connection due to auth failure"));
			Connection->Close();
		}
	}
#endif
}

/**
 * Initiates an auth request for the host of the session
 */
UBOOL UOnlineSubsystemGameSpy::CheckServerProductKey(void)
{
#if WANTS_CD_KEY_AUTH
	// Always allow dedicated servers
	if (GWorld &&
		GWorld->GetWorldInfo() &&
		GWorld->GetWorldInfo()->NetMode == NM_DedicatedServer)
	{
		return TRUE;
	}
	BYTE CDKey[20];
	DWORD CDKeyLen = 19;
	appMemzero(CDKey,CDKeyLen + 1);
	// Decrypt the key into the buffer
	if (DecryptProductKey(CDKey,CDKeyLen))
	{
		// Create a server challenge for itself
		ServerChallenge = FString::Printf(TEXT("%08X"),appCycles());
		char Response[73];
		// Calculate a response
		gcd_compute_response((char*)CDKey,
			TCHAR_TO_ANSI(*ServerChallenge),
			Response,
			CDResponseMethod_NEWAUTH);
		// Copy the response for matching up later
		ServerResponse = Response;
		// Assign us one of the ids
		ServerLocalId = NextAuthId++;
		FInternetIpAddr HostAddr;
		// Get the local IP
		GSocketSubsystem->GetLocalHostAddr(*GLog,HostAddr);
		DWORD IpAddr = 0;
		HostAddr.GetIp(IpAddr);
		// Ask GameSpy if the user has a valid key
		gcd_authenticate_user(GameID,
			ServerLocalId,
			htonl((INT)IpAddr),
			TCHAR_TO_ANSI(*ServerChallenge),
			TCHAR_TO_ANSI(*ServerResponse),
			::CDKeyAuthCallBack,
			::RefreshCDKeyAuthCallBack,
			this);
#if _DEBUG
		// Log in debug so we can see what is going on
		debugf(NAME_DevOnline,TEXT("gcd_authenticate_user(%d,%d,%d,%s,%s,,,)"),
			GameID,
			ServerLocalId,
			IpAddr,
			*ServerChallenge,
			*ServerResponse);
#endif
		appMemzero(CDKey,20);
		return TRUE;
	}
	return FALSE;
#else
	return TRUE;
#endif
}

/**
 * Used to respond to reauth a request from GameSpy. Used to see if a user is still
 * using a given CD key
 *
 * @param Connection the connection to authenticate
 * @param Hint the hint that GameSpy sent us
 */
void UOnlineSubsystemGameSpy::SubmitReauthResponse(UNetConnection* Connection,INT Hint)
{
#if WANTS_CD_KEY_AUTH
	if (Connection->Challenge.Len() > 0 && Connection->ClientResponse.Len() > 0)
	{
		// Ask GameSpy if the user has a valid key
		gcd_process_reauth(GameID,
			Connection->ResponseId,
			Hint,
			TCHAR_TO_ANSI(*Connection->ClientResponse));
#if _DEBUG
		// Log in debug so we can see what is going on
		debugf(NAME_DevOnline,TEXT("gcd_process_reauth(%d,%d,%d,%s)"),
			GameID,
			Connection->ResponseId,
			Hint,
			*Connection->ClientResponse);
#endif
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Closing connection due to reauth failure"));
		Connection->Close();
	}
#endif
}

/**
 * Called when GameSpy has determined if a player is authorized to play
 *
 * @param GameId the game id that the auth request was for
 * @param LocalId the id the game assigned the user when requesting auth
 * @param Authenticated whether the player was authenticated or should be rejected
 * @param ErrorMsg a string describing the error if there was one
 */
void UOnlineSubsystemGameSpy::CDKeyAuthCallBack(INT GameId,INT LocalId,INT Authenticated,const TCHAR* ErrorMsg)
{
#if WANTS_CD_KEY_AUTH
	// We need to take action only if they failed auth
	if (Authenticated == FALSE)
	{
		// If this is the server that is missing auth, then delist and close all clients
		if (ServerLocalId == LocalId)
		{
			// Skip if we aren't playing a networked match
			if (GWorld && GWorld->GetNetDriver())
			{
				UNetDriver* NetDriver = GWorld->GetNetDriver();
				// Close all connections
				for (INT Index = 0; Index < NetDriver->ClientConnections.Num(); Index++)
				{
					UNetConnection* Connection = NetDriver->ClientConnections(Index);
					Connection->Close();
				}
			}
			// Remove the server from the GameSpy list
			CachedGameInt->DestroyOnlineGame(NAME_Game);
		}
		else
		{
			// Skip if we aren't playing a networked match
			if (GWorld && GWorld->GetNetDriver())
			{
				UNetDriver* NetDriver = GWorld->GetNetDriver();
				// Find the person's net connection and close it
				for (INT Index = 0; Index < NetDriver->ClientConnections.Num(); Index++)
				{
					UNetConnection* Connection = NetDriver->ClientConnections(Index);
					if (Connection->ResponseId == LocalId)
					{
						debugf(NAME_DevOnline,TEXT("Client failed auth, closing connection"));
						Connection->Close();
						break;
					}
				}
			}
		}
	}
#endif
}

/**
 * Called when GameSpy has wants verify if a player is still playing on a host
 *
 * @param GameId the game id that the auth request was for
 * @param LocalId the id the game assigned the user when requesting auth
 * @param Hint the session id that should be passed to the reauth call
 * @param Challenge a string describing the error if there was one
 */
void UOnlineSubsystemGameSpy::RefreshCDKeyAuthCallBack(INT GameId,INT LocalId,INT Hint,const TCHAR* Challenge)
{
#if WANTS_CD_KEY_AUTH
	// Skip if we aren't playing a networked match
	if (GWorld && GWorld->GetNetDriver())
	{
		UNetDriver* NetDriver = GWorld->GetNetDriver();
		// Find the person's net connection and send them a message to reauth
		for (INT Index = 0; Index < NetDriver->ClientConnections.Num(); Index++)
		{
			UNetConnection* Connection = NetDriver->ClientConnections(Index);
			if (Connection->ResponseId == LocalId)
			{
				APlayerController* PC = Cast<APlayerController>(Connection->Actor);
				if (PC)
				{
					// Cache the challenge string for later use
					Connection->Challenge = Challenge;
					// Use a replicated function to send to the client. The client
					// will respond via a replicated server function
					PC->eventClientConvolve(Connection->Challenge,Hint);
				}
			}
		}
	}
#endif
}

/**
 * Determines the NAT type the player is using
 */
BYTE UOnlineSubsystemGameSpy::GetNATType(void)
{
#if PS3
	if (NpData)
	{
		return NpData->GetNatType();
	}
	return NAT_Open;
#else
	return NAT_Open;
#endif
}

/**
 * Starts an asynchronous read of the specified file from the network platform's
 * title specific file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
UBOOL UOnlineSubsystemGameSpy::ReadTitleFile(const FString& FileToRead)
{
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
UBOOL UOnlineSubsystemGameSpy::GetTitleFileContents(const FString& FileName,TArray<BYTE>& FileContents)
{
	return FALSE;
}



/**
* Unlocks the specified achievement for the specified user
*
* @param LocalUserNum the controller number of the associated user
* @param AchievementId the id of the achievement to unlock
*
* @return TRUE if the call worked, FALSE otherwise
*/
UBOOL UOnlineSubsystemGameSpy::UnlockAchievement(BYTE LocalUserNum,INT AchievementId,FLOAT PercentComplete)
{
	debugf(NAME_DevOnline,TEXT("UOnlineSubsystemGameSpy::UnlockAchievement %d %d"), LocalUserNum, AchievementId);

	DWORD Return = E_FAIL;
	// Validate the user index
	if (LocalUserNum < 4)
	{
		Return = 0;//ERROR_SUCCESS;

#if PS3
		GPS3NetworkPlatform->UnlockTrophy(AchievementId);
#endif
	}
	else
	{
		debugf(NAME_DevOnline,TEXT("Invalid player index (%d) specified to UnlockAchievement()"),
			(DWORD)LocalUserNum);
	}
	return Return == 0/*ERROR_SUCCESS*/ || Return == ERROR_IO_PENDING;
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
UBOOL UOnlineSubsystemGameSpy::ReadAchievements(BYTE LocalUserNum,INT TitleId,UBOOL bShouldReadText,UBOOL bShouldReadImages)
{
	if (LocalUserNum != LoggedInPlayerNum)
	{
		return FALSE;
	}

#if PS3

	// for PS3, start getting trophy info
	GPS3NetworkPlatform->UpdateTrophyInformation(bShouldReadText, bShouldReadImages);

#else

	// for PC, just trigger that we're done, since there's nothing to do
	OnlineSubsystemGameSpy_eventOnReadAchievementsComplete_Parms Parms(EC_EventParm);
	Parms.TitleId = 0;
	// Use the common method to do the work
	TriggerOnlineDelegates(this,ReadAchievementsCompleteDelegates,&Parms);

#endif

	return TRUE;
}

/**
 * Copies the list of achievements for the specified player and title id
 *
 * @param LocalUserNum the user to read the friends list of
 * @param Achievements the out array that receives the copied data
 * @param TitleId the title id of the game that these were read for
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
BYTE UOnlineSubsystemGameSpy::GetAchievements(BYTE LocalUserNum,TArray<FAchievementDetails>& Achievements,INT TitleId)
{
	Achievements.Reset();
#if PS3
	Achievements = GPS3NetworkPlatform->GetTrophyInformation();
#endif
	return OERS_Done;
}

#endif	//#if WITH_UE3_NETWORKING
