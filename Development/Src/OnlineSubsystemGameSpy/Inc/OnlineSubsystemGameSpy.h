/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef INCLUDED_ONLINESUBSYSTEMGAMESPY_H
#define INCLUDED_ONLINESUBSYSTEMGAMESPY_H 1

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING && WITH_GAMESPY

#if EPIC_INTERNAL
	#if !PS3
		#if SHIPPING_PC_GAME && !DEMOVERSION
			#define WANTS_CD_KEY_AUTH 1
		#endif
	#else
		// Sony has said this is forbidden
		#define WANTS_CD_KEY_AUTH 0
	#endif
#endif

// GameSpy headers
#include "common/gsCommon.h"
#include "common/gsCore.h"
#include "serverbrowsing/sb_serverbrowsing.h"
#include "serverbrowsing/sb_internal.h"
#include "qr2/qr2.h"
#include "gp/gp.h"
#include "sake/sake.h"
#include "sc/sc.h"
#include "webservices/AuthService.h"
#include "gcdkey/gcdkeyc.h"
#include "gcdkey/gcdkeys.h"
#include "pt/pt.h"

#if EPIC_INTERNAL
	#define USE_PRODUCTION_PS3_SERVERS 0
	#if PS3
		// @see UnSocketPS3.cpp
		#define WANTS_NAT_TRAVERSAL 0
	#endif
#endif

#ifndef _GAMESPY_URLS_DEFINED
	#define SAKEI_SOAP_URL_FORMAT		"http://%s.sake." GSI_DOMAIN_NAME "/SakeStorageServer/StorageServer.asmx"
	#define SC_SERVICE_URL_FORMAT		"http://%s.comp.pubsvs." GSI_DOMAIN_NAME "/CompetitionService/CompetitionService.asmx"
	#define _GAMESPY_URLS_DEFINED 1
#endif

#if PS3
	#include "../../PS3/External/GameSpy/OnlineSubsystemGameSpyPS3.h"
#else
	// Stubbed out code that allows compilation without PS3 headers
	#include "../PS3_Stubs/OnlineSubsystemGameSpyPS3.h"
#endif

#ifndef E_POINTER
	#define E_POINTER 0x80004003
#endif

/**
 * Base class that holds a delegate to fire when a given async task is complete
 */
class FOnlineAsyncTaskGameSpy :
	public FOnlineAsyncTask
{
protected:
	/**
	 * Holds the state of the async task. ERROR_IO_PENDING when in progress and
	 * any other value indicates completion
	 */
	DWORD CompletionStatus;

public:
	/**
	 * Initializes members
	 *
	 * @param InScriptDelegates the delegate list to fire off when complete
	 * @param InAsyncTaskName pointer to static data identifying the type of task
	 */
	FOnlineAsyncTaskGameSpy(TArray<FScriptDelegate>* InScriptDelegates,const TCHAR* InAsyncTaskName) :
		FOnlineAsyncTask(InScriptDelegates,InAsyncTaskName),
		CompletionStatus(ERROR_IO_PENDING)
	{
	}

	/**
	 * Allows any task to be ticked if not running on another thread
	 *
	 * @param DeltaTime the amount of time since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime)
	{
		UpdateElapsedTime(DeltaTime);
	}

	/**
	 * Checks the completion status of the task
	 *
	 * @return TRUE if done, FALSE otherwise
	 */
	FORCEINLINE UBOOL HasTaskCompleted(void) const
	{
		return CompletionStatus != ERROR_IO_PENDING && !ShouldDelayCompletion();
	}

	/**
	 * Returns the status code of the completed task. Assumes the task
	 * has finished.
	 *
	 * @return ERROR_SUCCESS if ok, an HRESULT error code otherwise
	 */
	virtual DWORD GetCompletionCode(void)
	{
		return CompletionStatus;
	}

	/**
	 * Sets the completion status of the task
	 *
	 * @return ERROR_SUCCESS if ok, an HRESULT error code otherwise
	 */
	FORCEINLINE void SetCompletionCode(DWORD Status)
	{
		CompletionStatus = Status == GP_NO_ERROR ? S_OK : E_FAIL;
	}

	/**
	 * Used to route the final processing of the data to the correct subsystem
	 * function. Basically, this is a function pointer for doing final work
	 *
	 * @param Subsystem the object to make the final call on
	 *
	 * @return TRUE if this task should be cleaned up, FALSE to keep it
	 */
	virtual UBOOL ProcessAsyncResults(class UOnlineSubsystemGameSpy* Subsystem)
	{
		return TRUE;
	}
};

/** Version of the delegate parms that takes a status code from the account creation callback and exposes it */
struct FAccountCreateResults
{
	/** Whether the account create worked or not */
    EOnlineAccountCreateStatus ErrorStatus;

	/**
	 * Constructor that sets the success flag based upon the passed in results
	 *
	 * @param Result the result code to check
	 */
	inline FAccountCreateResults(DWORD Result)
	{
		switch (Result)
		{
			case GP_NO_ERROR:
			{
				ErrorStatus = OACS_CreateSuccessful;
				break;
			}
			case GP_NEWUSER_BAD_NICK:
			{
				ErrorStatus = OACS_InvalidUserName;
				break;
			}
			case GP_NEWUSER_BAD_PASSWORD:
			{
				ErrorStatus = OACS_InvalidPassword;
				break;
			}
			case GP_NEWUSER_UNIQUENICK_INVALID:
			{
				ErrorStatus = OACS_InvalidUniqueUserName;
				break;
			}
			case GP_NEWUSER_UNIQUENICK_INUSE:
			{
				ErrorStatus = OACS_UniqueUserNameInUse;
				break;
			}
			case GP_PARAMETER_ERROR:
			case GP_NETWORK_ERROR:
			case GP_SERVER_ERROR:
			case E_POINTER:
			{
				ErrorStatus = OACS_ServiceUnavailable;
				break;
			}
			default:
			{
				ErrorStatus = OACS_UnknownError;
				break;
			}
		}
	}

	/**
	 * Constructor that sets the success flag to false
	 */
	inline FAccountCreateResults(void)
	{
		ErrorStatus = OACS_UnknownError;
	}
};

/** Version of the delegate parms that takes a status code from the profile login callback and exposes it */
struct FLoginFailedParms
{
	/** The index of the player that the error is for */
	BYTE PlayerNum;
	/** Whether the login worked or not */
    BYTE ErrorStatus;

	/**
	 * Constructor that sets the success flag based upon the passed in results
	 *
	 * @param InPlayerNum the index of the player the error is for
	 * @param Result the result code to check
	 */
	inline FLoginFailedParms(BYTE InPlayerNum,DWORD Result) :
		PlayerNum(InPlayerNum)
	{
		switch (Result)
		{
			case GP_NO_ERROR:
			{
				ErrorStatus = OSCS_Connected;
				break;
			}
			case GP_SERVER_ERROR:
			case GP_LOGIN_TIMEOUT:
			case GP_LOGIN_CONNECTION_FAILED:
			case GP_LOGIN_SERVER_AUTH_FAILED:
			{
				ErrorStatus = OSCS_ServiceUnavailable;
				break;
			}
			case GP_LOGIN_BAD_NICK:
			case GP_LOGIN_BAD_EMAIL:
			case GP_LOGIN_BAD_PASSWORD:
			case GP_LOGIN_BAD_PROFILE:
			case GP_LOGIN_PROFILE_DELETED:
			case GP_LOGIN_BAD_UNIQUENICK:
			case GP_LOGIN_BAD_PREAUTH:
			{
				ErrorStatus = OSCS_InvalidUser;
				break;
			}
			default:
			{
				ErrorStatus = OSCS_NotConnected;
				break;
			}
		}
	}

	/**
	 * Constructor that sets the success flag to failed
	 */
	inline FLoginFailedParms(void) :
		PlayerNum(0)
	{
		ErrorStatus = OSCS_NotConnected;
	}
};

/**
 * Handles async look up of a player's nick name and applies the found name
 * to the message that was sent by that player
 */
class FOnlineAsyncTaskGameSpyGetPlayerNick :
	public FOnlineAsyncTaskGameSpy
{
	/** The index of the message that this query is for */
	const DWORD MessageIndex;
	/** The sender of the message (the async results) */
	FString MessageSender;

public:
	/**
	 * Initializes the index to use when applying the queries results
	 *
	 * @param InMessageIndex the index of the message to apply the name to
	 */
	FOnlineAsyncTaskGameSpyGetPlayerNick(DWORD InMessageIndex) :
		FOnlineAsyncTaskGameSpy(NULL,TEXT("gpGetInfo()")),
		MessageIndex(InMessageIndex)
	{
	}

	/**
	 * Copies the sender string that is passed in
	 *
	 * @param Sender the string to copy
	 */
	inline void SetSender(const TCHAR* Sender)
	{
		MessageSender = Sender;
	}

	/**
	 * Called in response to a request for a remote profile's info
	 *
	 * @param Connection the GP connection
	 * @param Arg the profile's info
	 * @param UserData pointer to the class holding the async data
	 */
	static void GPGetInfoCallback(GPConnection* Connection,GPGetInfoResponseArg* Arg,void* UserData)
	{
		check(UserData != NULL);
		FOnlineAsyncTaskGameSpyGetPlayerNick* AsyncTask = (FOnlineAsyncTaskGameSpyGetPlayerNick*)UserData;
		// Set the completion status and the nick if it was ok
		AsyncTask->SetCompletionCode(Arg->result);
		if (Arg->result == GP_NO_ERROR)
		{
			AsyncTask->SetSender((const TCHAR*)Arg->uniquenick);
		}
	}

	/**
	 * Calls the subsystem letting it set the name on the message
	 *
	 * @param Subsystem the object to make the final call on
	 *
	 * @return TRUE if this task should be cleaned up, FALSE to keep it
	 */
	virtual UBOOL ProcessAsyncResults(class UOnlineSubsystemGameSpy* Subsystem);
};

/**
 * Used to poll for the PS3's NP login to complete
 */
class FOnlineAsyncTaskHandleExternalLogin :
	public FOnlineAsyncTaskGameSpy
{
	/** Whether the user signed in ok or not */
	UBOOL bWasSuccessful;

public:
	/**
	 * Initializes members and starts the process
	 */
	FOnlineAsyncTaskHandleExternalLogin() :
		FOnlineAsyncTaskGameSpy(NULL,TEXT("Network start utility")),
		bWasSuccessful(FALSE)
	{
		CompletionStatus = ERROR_IO_PENDING;
#if PS3
		appPS3BeginNetworkStartUtility();
#endif
	}

	/**
	 * Ticks the NP login task
	 *
	 * @param DeltaTime the amount of time since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Calls the subsystem letting it set the name on the message
	 *
	 * @param Subsystem the object to make the final call on
	 *
	 * @return TRUE if this task should be cleaned up, FALSE to keep it
	 */
	virtual UBOOL ProcessAsyncResults(class UOnlineSubsystemGameSpy* Subsystem);
};

/**
 * Handles async look up of a player's nick name and applies the found name
 * to the message that was sent by that player
 */
class FOnlineAsyncTaskGameSpyWriteProfile :
	public FOnlineAsyncTaskGameSpy
{
	/** The file that the data is being written to */
	FArchive* FileWriter;
	/** For logging any file errors */
	FStringOutputDevice ErrorString;
	/** Holds the data serialized into a blob */
	FProfileSettingsWriter Writer;

public:
	/**
	 * Zeroes members
	 */
	FOnlineAsyncTaskGameSpyWriteProfile(void) :
		FOnlineAsyncTaskGameSpy(NULL,TEXT("'Async profile file write'")),
		FileWriter(NULL),
		Writer(3000,TRUE)
	{
	}

	/**
	 * Frees the archive
	 */
	~FOnlineAsyncTaskGameSpyWriteProfile()
	{
		delete FileWriter;
	}

	/**
	 * Creates the file writer that is being used
	 *
	 * @param FileName the file that is being written to
	 * @param Buffer the buffer that needs to be written
	 * @param Size the amount of data to write
	 */
	inline UBOOL WriteData(const TCHAR* FileName,UOnlineProfileSettings* Profile)
	{
		// Serialize them to a blob and then write to disk
		if (Writer.SerializeToBuffer(Profile->ProfileSettings))
		{
			FileWriter = GFileManager->CreateFileWriter(FileName,FILEWRITE_SaveGame | FILEWRITE_Async,&ErrorString,Writer.GetFinalBufferLength());
			if (FileWriter)
			{
				FileWriter->Serialize((void*)Writer.GetFinalBuffer(),Writer.GetFinalBufferLength());
				FileWriter->Close();
			}
		}
		return FileWriter != NULL;
	}

	/**
	 * Allows any task to be ticked if not running on another thread
	 *
	 * @param DeltaTime the amount of time since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime)
	{
		UBOOL bHasError = FALSE;
		if (FileWriter)
		{
			// Set the completion code so we don't keep ticking once this is done
			if (FileWriter->IsCloseComplete(bHasError))
			{
				CompletionStatus = S_OK;
				if (bHasError)
				{
					CompletionStatus = E_FAIL;
					debugf(NAME_DevOnline,TEXT("Writing the profile failed with '%s'"),*ErrorString);
				}
			}
		}
		else
		{
			CompletionStatus = E_FAIL;
			debugf(NAME_DevOnline,TEXT("Couldn't create file error '%s'"),*ErrorString);
		}
	}

	/**
	 * Calls the subsystem letting it do a network write if desired
	 *
	 * @param Subsystem the object to make the final call on
	 *
	 * @return TRUE if this task should be cleaned up, FALSE to keep it
	 */
	virtual UBOOL ProcessAsyncResults(class UOnlineSubsystemGameSpy* Subsystem);
};

/**
 * Handles async clean up of stats reports
 */
class FOnlineAsyncTaskGameSpySubmitStats :
	public FOnlineAsyncTaskGameSpy
{
	/** The report that needs cleaning up upon completion */
	SCReportPtr ReportHandle;

public:
	/**
	 * Initializes report handle
	 *
	 * @param InReportHandle the report object to free later
	 * @param InScriptDelegates the set of listeners to notify when complete
	 */
	FOnlineAsyncTaskGameSpySubmitStats(SCReportPtr InReportHandle,TArray<FScriptDelegate>* InScriptDelegates) :
		FOnlineAsyncTaskGameSpy(InScriptDelegates,TEXT("scSubmitReport()")),
		ReportHandle(InReportHandle)
	{
		check(ReportHandle);
	}

	/** Cleans up the report handle object */
	~FOnlineAsyncTaskGameSpySubmitStats(void)
	{
		scDestroyReport(ReportHandle);
	}

	/**
	 * Called after the host submits a report
	 *
	 * @param Interface a handle to the SC interface
	 * @param HttpResult the result of the http request
	 * @param Result the result of the report submission
	 * @param UserData pointer to the subsystem
	 */
	static void SubmitStatsReportCallback(SCInterfacePtr Interface,GHTTPResult HttpResult,SCResult Result,void* UserData)
	{
		check(UserData != NULL);
		FOnlineAsyncTaskGameSpySubmitStats* AsyncTask = (FOnlineAsyncTaskGameSpySubmitStats*)UserData;
		debugf(NAME_DevOnline,
			TEXT("SubmitStatsReportCallback: HttpResult = 0x%08X, Result = 0x%08X"),
			(DWORD)HttpResult,
			(DWORD)Result);
		// Set the completion status
		AsyncTask->SetCompletionCode(HttpResult == GHTTPSuccess && Result == SCResult_NO_ERROR ? S_OK : E_FAIL);
	}

	/**
	 * Calls the subsystem letting it know the stats report is done
	 *
	 * @param Subsystem the object to make the final call on
	 *
	 * @return TRUE if this task should be cleaned up, FALSE to keep it
	 */
	virtual UBOOL ProcessAsyncResults(class UOnlineSubsystemGameSpy* Subsystem);
};

/**
 * Template class for delayed deletion of certain GameSpy objects after use
 */
template<typename TYPE> class TOnlineAsyncTaskGameSpyDelayedDeletion :
	public FOnlineAsyncTaskGameSpy
{
	/** The object that needs deleting */
	TYPE* DataToDelete;

public:
	/**
	 * Copies the pointer that we are going delete later
	 *
	 * @param InDataToDelete the pointer to copy
	 */
	TOnlineAsyncTaskGameSpyDelayedDeletion(TYPE* InDataToDelete) :
		FOnlineAsyncTaskGameSpy(NULL,TEXT("")),
		DataToDelete(InDataToDelete)
	{
		SetCompletionCode(S_OK);
	}

	/** Deletes the held data */
	~TOnlineAsyncTaskGameSpyDelayedDeletion()
	{
		delete DataToDelete;
	}
};

typedef TOnlineAsyncTaskGameSpyDelayedDeletion<SAKEGetMyRecordsInput> FDelayedDeletionSake;
typedef TOnlineAsyncTaskGameSpyDelayedDeletion<SAKESearchForRecordsInput> FDelayedDeletionSakeStatsRead;
typedef TOnlineAsyncTaskGameSpyDelayedDeletion<SAKEGetRecordCountInput> FDelayedDeletionSakeStatsRecordCount;

/**
 * Async create of user. Create is done in multiple steps:
 *		Try to create the new account
 *		If that fails due to bad nick, connect using the details
 *			If that succeeds, register the player's unique nick
 *			Otherwise, forward failure
 *		Otherwise, forward failure
 */
class FOnlineAsyncTaskGameSpyCreateNewUser :
	public FOnlineAsyncTaskGameSpy
{
	/** The unique nick to create/register */
	FString UniqueNick;
	/** The email address associated with the account */
	FString EmailAddress;
	/** The password for the account */
	FString Password;
	/** The product key for this user */
	FString ProductKey;
	/** The profile that was returned by the connect */
	INT Profile;

public:
	/**
	 * Initializes held members
	 *
	 * @param UniqueNick The unique nick to create/register
	 * @param EmailAddress The email address associated with the account
	 * @param Passwod The password for the account
	 * @param ProductKey The product key for this user
	 */
	FOnlineAsyncTaskGameSpyCreateNewUser(const FString& InUniqueNick,const FString& InEmailAddress,const FString& InPassword,const FString& InProductKey) :
		FOnlineAsyncTaskGameSpy(NULL,TEXT("gpNewUser()")),
		UniqueNick(InUniqueNick),
		EmailAddress(InEmailAddress),
		Password(InPassword),
		ProductKey(InProductKey),
		Profile(0)
	{
	}

	/**
	 * Called to return the results of the async operation
	 *
	 * @param Connection the GP connection
	 * @param Arg the new user result
	 * @param UserData pointer to the async task
	 */
	static void _GPNewUserCallback(GPConnection* Connection,GPNewUserResponseArg* Arg,void* UserData)
	{
		check(UserData != NULL);
		FOnlineAsyncTaskGameSpyCreateNewUser* AsyncTask = (FOnlineAsyncTaskGameSpyCreateNewUser*)UserData;
		// Forward the call
		AsyncTask->GPNewUserCallback(Connection,Arg);
	}

	/**
	 * Called on the specific instance to return the results of the async operation
	 *
	 * @param Connection the GP connection
	 * @param Arg the new user result
	 */
	void GPNewUserCallback(GPConnection* Connection,GPNewUserResponseArg* Arg);

	/**
	 * Called at the end of a connection attempt
	 *
	 * @param Connection the GP connection
	 * @param Arg the connection result
	 * @param UserData pointer to the subsystem
	 */
	static void _GPConnectCallback(GPConnection* Connection,GPConnectResponseArg* Arg,void* UserData)
	{
		check(UserData != NULL);
		FOnlineAsyncTaskGameSpyCreateNewUser* AsyncTask = (FOnlineAsyncTaskGameSpyCreateNewUser*)UserData;
		// Forward the call
		AsyncTask->GPConnectCallback(Connection,Arg);
	}

	/**
	 * Called on the specific instance to return the results of user connect
	 *
	 * @param Connection the GP connection
	 * @param Arg the new user result
	 */
	void GPConnectCallback(GPConnection* Connection,GPConnectResponseArg* Arg);

	/**
	 * Called at the end of the unique nick registration
	 *
	 * @param Connection the GP connection
	 * @param Arg the registration result
	 * @param UserData pointer to the subsystem
	 */
	static void _GPRegisterUniqueNickCallback(GPConnection* Connection,GPRegisterUniqueNickResponseArg* Arg,void* UserData)
	{
		check(UserData != NULL);
		FOnlineAsyncTaskGameSpyCreateNewUser* AsyncTask = (FOnlineAsyncTaskGameSpyCreateNewUser*)UserData;
		// Forward the call
		AsyncTask->GPRegisterUniqueNickCallback(Connection,Arg);
	}

	/**
	 * Called at the end of the unique nick registration on the specific instance housing the data
	 *
	 * @param Connection the GP connection
	 * @param Arg the registration result
	 */
	void GPRegisterUniqueNickCallback(GPConnection* Connection,GPRegisterUniqueNickResponseArg* Arg);
};

/** Performs delayed deletion of the server browser instance */
class FOnlineAsyncTaskGameSpyDelayedServerBrowserFree :
	public FOnlineAsyncTaskGameSpy
{
	/** The object that needs deleting */
	struct _ServerBrowser* DataToDelete;

public:
	/**
	 * Copies the pointer that we are going delete later
	 *
	 * @param InDataToDelete the pointer to copy
	 */
	FOnlineAsyncTaskGameSpyDelayedServerBrowserFree(_ServerBrowser* InDataToDelete) :
		FOnlineAsyncTaskGameSpy(NULL,TEXT("ServerBrowserFree()")),
		DataToDelete(InDataToDelete)
	{
		SetCompletionCode(S_OK);
	}

	/** Deletes the held data */
	~FOnlineAsyncTaskGameSpyDelayedServerBrowserFree()
	{
		ServerBrowserFree(DataToDelete);
	}
};

#if PS3 && WANTS_NAT_TRAVERSAL
	#include "../../PS3/External/GameSpy/OnlineSubsystemGameSpyPS3AsyncTasks.h"
#endif

// Unreal integration script layer 
#include "OnlineSubsystemGameSpyClasses.h"

#endif	//#if WITH_UE3_NETWORKING
#endif  // !INCLUDED_ONLINESUBSYSTEMGAMESPY_H

