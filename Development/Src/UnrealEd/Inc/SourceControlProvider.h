/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __SOURCE_CONTORL_PROVIDER_H__
#define __SOURCE_CONTORL_PROVIDER_H__

#if HAVE_SCC


namespace SourceControl
{
	enum Command 
	{
		CHECK_OUT,
		CHECK_IN,
		ADD_FILE,
		ADD_TO_DEFAULT_CHANGELIST,
		DELETE_FILE,
		REVERT,
		REVERT_UNCHANGED,
		UPDATE_STATUS,
		HISTORY,
		GET_MODIFIED_FILES,
		GET_UNMODIFIED_FILES,
		INFO
	};
};

class FSourceControlEventListener;
struct FSourceControlCommand;


//-----------------------------------------------
//Source Control Event Listener
//-----------------------------------------------
class FSourceControlEventListener
{
public:
	/** Callback when a command is done executing */
	virtual void SourceControlCallback(FSourceControlCommand* InCommand) = 0;
};


//declare interface class
class ISourceControlProvider
{
protected:
	ISourceControlProvider(void)
	:	bServerAvailable( FALSE ),
		bProjectOpen( FALSE ),
		bDisabled( FALSE ),
		bAutoAddNewFiles( FALSE )
	{}

	virtual ~ISourceControlProvider(void) {};
	//PRIVATE INTERFACE FOR API SPECIFICS

	/** Interface to Init of connection with source control server */
	virtual void Init (void) = 0;
	/** Interface to Close of connection with source control server */
	virtual void Close (void) = 0;

public:
	/** 
	 * Execute Command
	 * @param InCommand - Command to execute
	 */
	virtual UBOOL ExecuteCommand(FSourceControlCommand* InCommand) const = 0;

	/**
	 * Checks the provided command's error type, and responds accordingly
	 *
	 * @param	InCommand	Command to check the error type of
	 */
	virtual void RespondToCommandErrorType(const FSourceControlCommand& InCommand) {}

protected:
	/** Indicates if source control integration is available or not. */
	BITFIELD bServerAvailable:1;

	/** Indicates that the project is currently open. */
	BITFIELD bProjectOpen:1;

	/** Flag to disable source control integration. **/
	BITFIELD bDisabled:1;

	/** Indicates if newly created files should be automatically added to source control or not */
	BITFIELD bAutoAddNewFiles:1;
};

//API Specifics
#if WITH_PERFORCE

#include "PerforceSourceControlProviderShared.h"

#else

//----------------------------------
//Stub Source Control Provider that will allow compilation
//----------------------------------
//declare interface class
class FStubSourceControlProvider : public ISourceControlProvider
{
public:
	virtual ~FStubSourceControlProvider(void) {};

	virtual void Init (void) 
	{
		warnf(NAME_SourceControl, TEXT("Stub source control provider is enabled.  Please enable a specific implementation such as WITH_PERFORCE"));
		warnf(NAME_SourceControl, TEXT("  For perforce, see SetUpSourceControlEnvironment in UE3BuildExternal.cs for requirements"));
			
		bDisabled = TRUE;
	}
	virtual void Close (void) {};

	virtual UBOOL ExecuteCommand(FSourceControlCommand* InCommand) const { return FALSE; }

	//allow the wrapper class access to this provider
	friend class FSourceControl;
};
//Make this the generic source control interface
typedef FStubSourceControlProvider FSourceControlProvider;

#endif


//------------------------------------------
//Source Control Cross Thread Command Structure
//------------------------------------------

struct FSourceControlCommand : public FQueuedWork
{
public:
	
	/**Enumeration for potential error types while executing a command*/
	enum ECommandErrorType
	{
		CET_NoError,			// No error occurred while executing the command
		CET_ConnectionError,	// A source control connection error occurred while executing the command
		CET_CommandError		// An error occurred as a result of the command itself
	};

	FSourceControlCommand(FSourceControlEventListener* InEventListener, FSourceControlProvider* InProvider)
		: CommandID(-1),
		  bExecuteProcessed(FALSE),
		  bCommandSuccessful(FALSE),
		  ErrorType(CET_NoError),
		  EventListener(InEventListener), 
		  SourceControlProvider (InProvider)
	{
	}

	/**
	 * This is where the real thread work is done. All work that is done for
	 * this queued object should be done from within the call to this function.
	 */
	virtual void DoWork(void)
	{
		check(SourceControlProvider);

		SourceControlProvider->ExecuteCommand(this);
		appInterlockedExchange((INT*)&bExecuteProcessed,TRUE);
	}

	/**
	 * Tells the queued work that it is being abandoned so that it can do
	 * per object clean up as needed. This will only be called if it is being
	 * abandoned before completion. NOTE: This requires the object to delete
	 * itself using whatever heap it was allocated in.
	 */
	virtual void Abandon(void)
	{
		appInterlockedExchange((INT*)&bExecuteProcessed,TRUE);
	}

	/**
	 * This method is also used to tell the object to cleanup but not before
	 * the object has finished it's work.
	 */ 
	virtual void DoThreadedWork(void)
	{
		DoWork();
	}

	/**Command Index that is unique to this command*/
	INT CommandID;

	/**If TRUE, this command has been processed by the source control thread*/
	UBOOL bExecuteProcessed;

	/**If TRUE, the perforce command succeeded*/
	UBOOL bCommandSuccessful;

	/**Type of error that occurred executing the command, if any*/
	ECommandErrorType ErrorType;

	/**FSourceControlEventListener that issued this command*/
	FSourceControlEventListener* EventListener;

	/**Source Control Provider that is the actual API*/
	const FSourceControlProvider* SourceControlProvider;

	/**Command String*/
	SourceControl::Command CommandType;

	/**Description - Used for Changelists*/
	FString Description;

	/**Parameters passed in*/
	TArray< FString > Params;

	/**Potential error message storage*/
	TArray< FString > ErrorMessages;

	/**Array of maps (key/value pairs) returned from perforce*/
	TMap< FString, TMap<FString, FString> > Results;
};

#endif // HAVE_SCC
#endif // #define __SOURCECONTORLINTEGRATION_H__

