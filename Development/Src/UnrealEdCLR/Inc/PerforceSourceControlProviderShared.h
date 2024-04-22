/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __PERFORCE_SOURCE_CONTROL_H__
#define __PERFORCE_SOURCE_CONTROL_H__

//this should only be include from SourceControl.h
#if HAVE_SCC && WITH_PERFORCE

//forward declare for 
class FPerforceSourceControlProvider;

/**
 * P4 .NET implementation
 */
#ifdef __cplusplus_cli
using namespace System;
using namespace P4API;
using namespace System::Collections::Generic;

//-------------------------------------
//p4.net API structure
//-------------------------------------
namespace PerforceConstants
{
	const INT MaxRetryCount = 5;
	const INT RetrySleepTimeInMS = 1000;

	enum LoginResults
	{
		LoginAccepted,
		LoginCanceled
	};
}

ref class FPerforceNET
{
public:
	//This constructor is strictly for internal questions to perforce (get client spec list, etc)
	FPerforceNET(String^ InServerName, String^ InUserName, String^ InClientSpec, String^ InTicket);
	/** API Specific open of source control project*/
	FPerforceNET(const FPerforceSourceControlProvider* InSourceControlProvider);
	/** API Specific close of source control project*/
	~FPerforceNET();

	/** Updates the name of the branch where we're running from */
	FString UpdateBranchName( void );

	/**
	 * Static function in charge of making sure the specified connection is valid or requests that data from the user via dialog
	 * @param InOutPortName - Port name in the inifile.  Out value is the port name from the connection dialog
	 * @param InOutUserName - User name in the inifile.  Out value is the user name from the connection dialog
	 * @param InOutClientSpecName - Client Spec name in the inifile.  Out value is the client spec from the connection dialog
	 * @param InOutTicket - The ticket to use as a password when talking to Perforce.
	 * @return - TRUE if the connection, whether via dialog or otherwise, is valid.  False if source control should be disabled
	 */
	static UBOOL EnsureValidConnection(FString& InOutServerName, FString& InOutUserName, FString& InOutClientSpecName, FString& InTicket);

	/** 
	 * Execute Command
	 * @param InCommand - Command to execute
	 */
	UBOOL ExecuteCommand(FSourceControlCommand* InCommand);

	/**
	 * Get List of ClientSpecs
	 * @param InUserName - The username who should own the client specs in the list
	 * @return - List of client spec names
	 */
	List<String^>^ GetClientSpecList (String^ InUserName, TArray<FString>& OutErrorMessage);

protected:
	/**
	 * Runs internal perforce command, catches exceptions, returns results
	 */
	P4RecordSet^ RunCommand (const FString& InCommand, const TArray<FString>& InParameters, TArray<FString>&  OutErrorMessage)
	{
		const UBOOL bStandardDebugOutput=TRUE;
		const UBOOL bAllowRetry=TRUE;
		return RunCommand(InCommand, InParameters, OutErrorMessage, bStandardDebugOutput, bAllowRetry);
	}
	/**
	 * Runs internal perforce command, catches exceptions, returns results
	 */
	P4RecordSet^ RunCommand (const FString& InCommand, const TArray<FString>& InParameters, TArray<FString>& OutErrorMessage, const UBOOL bInStandardDebugOutput, const UBOOL bInAllowRetry);

	/**
	 * Make a valid connection if possible
	 * @param InServerName - Server name for the perforce connection
	 * @param InUserName - User for the perforce connection
	 * @param InClientSpecName - Client Spec name for the perforce connection
	 * @param InTicket - The ticket to use in lieu of a login
	 */
	void EstablishConnection(String^ InServerName, String^ InUserName, String^ InClientSpecName, String^ InTicket);

	/**
	 * Function to dissect the results from an UpdateStatus Command
	 */
	void ParseUpdateStatusResults(FSourceControlCommand* InCommand, P4RecordSet^ Records);
	
	/**
	 * Helper method to parse the results of a history query command
	 *
	 * @param	InCommand	History command that was executed
	 * @param	Records		RecordSet that results from the provided command's execution
	 */
	void ParseHistoryResults(FSourceControlCommand* InCommand, P4RecordSet^ Records);

	/**
	 * Helper method to dissect the results from a get modified/unmodified files (aka "diff") command
	 *
	 * @param	InCommand	Command to set parsed results for
	 * @param	Records		Results from the diff command to parse
	 */
	void ParseDiffResults(FSourceControlCommand* InCommand, P4RecordSet^ Records);

	/**
	 * Debug helper that will print column heads and values for a record set
	 */
	void DebugPrintRecordSet (P4RecordSet^ Records);

	/** 
	 * Displays the perforce login screen and reports back whether "OK" or "Cancel" was selected 
	 */
	static PerforceConstants::LoginResults GetPerforceLogin (String^% InOutPortName, String^% InOutUserName, String^% InOutClientSpecName, String^ InTicket);


	/** Perforce .NET connection object */
	P4Connection^ p4;

	/** Copy of the calling source control provider */
	const FPerforceSourceControlProvider* SourceControlProvider;

	/** TRUE if connection was successfully established */
	UBOOL bEstablishedConnnection;
};
#endif //__cplusplus_cli


//-------------------------------------
//unmanaged perforce provider interface
//-------------------------------------

//declare interface class
class FPerforceSourceControlProvider : public ISourceControlProvider
{
protected:
	/** API Specific Init of connection with source control server */
	virtual void Init (void);
	/** API Specific Close of connection with source control server */
	virtual void Close (void);

public:
	/** 
	 * Execute Command
	 * @param InCommand - Command to execute
	 */
	virtual UBOOL ExecuteCommand(FSourceControlCommand* InCommand) const;

	/**
	 * Checks the provided command's error type, and responds accordingly
	 *
	 * @param	InCommand	Command to check the error type of
	 */
	virtual void RespondToCommandErrorType(const FSourceControlCommand& InCommand);

	const FString& GetPort(void) const { return PortName; }
	const FString& GetUser(void) const { return UserName; }
	const FString& GetClientSpec(void) const { return ClientSpecName; }
	const FString& GetTicket(void) const { return Ticket; }
private:
	/**
	 * Loads user/SCC information from the INI file.
	 */
	void LoadSettings();
	/**
	 * Saves user/SCC information from the INI file.
	 */
	void SaveSettings();

	/** The address to the perforce server. */
	FString PortName;
	/** The user name we are using for log ins. */
	FString UserName;
	/** The client spec we use for login. */
	FString ClientSpecName;
	/** The ticket we use for login. */
	FString Ticket;

	//allow the wrapper class access to this provider
	friend class FSourceControl;
};


//Make this the generic source control interface
typedef FPerforceSourceControlProvider FSourceControlProvider;



#endif // HAVE_SCC && WITH_PERFORCE
#endif // #define __PERFORCE_SOURCE_CONTROL_H__

