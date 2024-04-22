/*=============================================================================
	PerforceSourceControl.cpp: Perforce specific source control API
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"
#include "SourceControl.h"

#if HAVE_SCC && WITH_PERFORCE


#ifdef __cplusplus_cli

#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"

//Get popup controls
using namespace System::Windows;
using namespace System::Windows::Input;
using namespace System::Windows::Controls::Primitives;
//Get p4 api
using namespace P4API;

//forward declaration

ref class FPerforceNET;
//-----------------------------------------------
// Perforce Login Panel
//-----------------------------------------------
ref class MPerforceLoginPanel : public MWPFPanel
{
public:
	MPerforceLoginPanel(String^ InXamlName, String^ InServerName, String^ InUserName, String^ InClientSpecName)
		: MWPFPanel(InXamlName),
		  bReadyForUse(FALSE)
	{
		//hook up button events
		OKButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
		OKButton->Click += gcnew RoutedEventHandler( this, &MPerforceLoginPanel::OKClicked );

		Button^ CancelButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CancelButton" ) );
		CancelButton->Click += gcnew RoutedEventHandler( this, &MPerforceLoginPanel::CancelClicked );

		ReconnectButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "ReconnectButton" ) );
		ReconnectButton->Visibility = System::Windows::Visibility::Collapsed;
		ReconnectButton->Click += gcnew RoutedEventHandler( this, &MPerforceLoginPanel::ReconnectClicked );

		ClientSpecBrowseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "ClientSpecBrowseButton" ) );
		ClientSpecBrowseButton->Click += gcnew RoutedEventHandler( this, &MPerforceLoginPanel::BrowseClientSpec );

		ClientSpecListBox = safe_cast< ListBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ClientSpecListBox" ) );
		ClientSpecListBox->SelectionChanged += gcnew SelectionChangedEventHandler( this, &MPerforceLoginPanel::SelectClientSpec );

		ClientSpecPopup = safe_cast< Popup^ >( LogicalTreeHelper::FindLogicalNode( this, "ClientSpecPopup" ) );

		ServerNameTextCtrl = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ServerName" ) );
		UserNameTextCtrl = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "UserName" ) );
		ClientSpecNameTextCtrl = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ClientSpecName" ) );

		ErrorPanel = safe_cast< Panel^ >( LogicalTreeHelper::FindLogicalNode( this, "ErrorPanel" ) );
		ErrorMessageTextBlock = safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( this, "ErrorMessageTextBlock" ) );

		ServerName = InServerName;
		UserName = InUserName;
		ClientSpecName = InClientSpecName;

		// Register for property change callbacks from our properties object
		this->PropertyChanged += gcnew ComponentModel::PropertyChangedEventHandler( this, &MPerforceLoginPanel::OnLoginPropertyChanged );

		//Now that initial state has been established, setup callbacks for the 3 text boxes to try and reconnect when a value changes
		UnrealEd::Utils::CreateBinding(ServerNameTextCtrl, TextBox::TextProperty, this, "ServerName" );
		UnrealEd::Utils::CreateBinding(UserNameTextCtrl, TextBox::TextProperty, this, "UserName" );
		UnrealEd::Utils::CreateBinding(ClientSpecNameTextCtrl, TextBox::TextProperty, this, "ClientSpecName" );

		//Listen for the "enter" key to be clicked to commit the text value
		ServerNameTextCtrl->KeyUp += gcnew KeyEventHandler( this, &MPerforceLoginPanel::OnKeyUp);
		UserNameTextCtrl->KeyUp += gcnew KeyEventHandler( this, &MPerforceLoginPanel::OnKeyUp);
		ClientSpecNameTextCtrl->KeyUp += gcnew KeyEventHandler( this, &MPerforceLoginPanel::OnKeyUp);

		bReadyForUse = TRUE;

		AttemptReconnectAndRefreshClientSpecList();
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame (MWPFFrame^ InParentFrame) override
	{
		MWPFPanel::SetParentFrame(InParentFrame);
		
		//Make sure to treat the close as a cancel
		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode(InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MPerforceLoginPanel::CancelClicked );
	}

	/**
	 * Sets the clients spec (both data and UI widget)
	 */
	void SetClientSpecName (String^ InClientSpecName)
	{
		ClientSpecNameTextCtrl->Text = InClientSpecName;
	}

	/**
	 * Now that the login panel has been excepted
	 */
	void GetResult(String^% OutServerName, String^% OutUserName, String^% OutClientSpecName)
	{
		OutServerName = ServerName;
		OutUserName = UserName;
		OutClientSpecName = ClientSpecName;
	}

private:

	/**
	 * Something has changed and we want to try to re-establish a connection with perforce in order to validate our clientspec list
	 */
	void AttemptReconnectAndRefreshClientSpecList()
	{
		if (!bReadyForUse)
		{
			return;
		}

		TArray<FString> ErrorMessages;
		//attempt to ask perforce for a list of client specs that belong to this user
		FPerforceNET Connection(ServerName, UserName, nullptr, nullptr);
		List<String^>^ ClientSpecList = Connection.GetClientSpecList(UserNameTextCtrl->Text, ErrorMessages);
		if (ClientSpecList->Count > 0)
		{
			//fill the list box with valid client specs
			ClientSpecListBox->Items->Clear();
			for (int i = 0; i < ClientSpecList->Count; ++i)
			{
				ClientSpecListBox->Items->Add(ClientSpecList[i]);
			}
			//this is the only possible client spec
			if (ClientSpecList->Count == 1)
			{
				ClientSpecName = ClientSpecList[0];
				ClientSpecBrowseButton->IsEnabled = false;
				ClientSpecNameTextCtrl->IsEnabled = false;
			}
			else
			{
				ClientSpecBrowseButton->IsEnabled = true;
				ClientSpecNameTextCtrl->IsEnabled = true;
			}
			SetErrorMessage(TEXT(""));
			OKButton->IsEnabled = true;
			
			// Hide the reconnect button if the connection was successful
			ReconnectButton->Visibility = System::Windows::Visibility::Collapsed;
		} 
		else
		{
			ClientSpecName = String::Empty;
			FString FinalErrorMessage;
			if (ErrorMessages.Num() > 0)
			{
				for (int i = 0; i < ErrorMessages.Num(); ++i)
				{
					FinalErrorMessage += FString::Printf(TEXT("%s\n"), *ErrorMessages(i));
				}
			}
			else
			{
				FinalErrorMessage = *LocalizeUnrealEd("PerforceLogin_Error_UnableToConnectToServer");
			}
			SetErrorMessage(*FinalErrorMessage);
			//you can't "accept" an invalid state
			OKButton->IsEnabled = false;
			ClientSpecBrowseButton->IsEnabled = false;
			ClientSpecNameTextCtrl->IsEnabled = false;

			// Display the manual reconnect button if an error has occurred
			ReconnectButton->Visibility = System::Windows::Visibility::Visible;
		}
	}

	/** Called when a login window property is changed */
	void OnLoginPropertyChanged( Object^ Owner, ComponentModel::PropertyChangedEventArgs^ Args )
	{
		AttemptReconnectAndRefreshClientSpecList();
	}

	/**Add ability to commit text ctrls on "ENTER" */
	void OnKeyUp (Object^ Owner, KeyEventArgs^ Args)
	{
		if (Args->Key == Key::Return)
		{
			TextBox^ ChangedTextBox = safe_cast< TextBox^ >(Args->Source);
			if (ChangedTextBox != nullptr)
			{
				ChangedTextBox->GetBindingExpression(ChangedTextBox->TextProperty)->UpdateSource();
			}
		}
	}



	/** Called when the settings of the dialog are to be accepted*/
	void OKClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close(PerforceConstants::LoginAccepted);
	}

	/** Called when the settings of the dialog are to be ignored*/
	void CancelClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close(PerforceConstants::LoginCanceled);
	}

	/** Called when the user clicks on the "retry"/reconnect button */
	void ReconnectClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		AttemptReconnectAndRefreshClientSpecList();
	}

	/** Called when attempting to pick a new client spec*/
	void BrowseClientSpec( Object^ Owner, RoutedEventArgs^ Args )
	{
		if (ClientSpecListBox->Items->Count)
		{
			ClientSpecPopup->IsOpen = true;
		}
	}

	/** Called a new client spec is selected via listbox */
	void SelectClientSpec ( Object^ Owner, SelectionChangedEventArgs ^ Args )
	{
		if (Args->AddedItems->Count == 1)
		{
			ClientSpecName = (safe_cast<String^>(Args->AddedItems[0]));
			//hide popup
			ClientSpecPopup->IsOpen = false;
		}
	}

	/**Called when there is a problem logging into the perforce server*/
	void SetErrorMessage(const FString& InErrorMessage)
	{
		if (InErrorMessage.Len() > 0)
		{
			ErrorPanel->Visibility = System::Windows::Visibility::Visible;
			ErrorMessageTextBlock->Text = CLRTools::ToString(*InErrorMessage);
		}
		else
		{
			ErrorPanel->Visibility = System::Windows::Visibility::Collapsed;
		}
	}

	

private:
	/** Internal widgets to save having to get in multiple places*/
	Button^ OKButton;
	Button^ ReconnectButton;

	TextBox^ ServerNameTextCtrl;
	TextBox^ UserNameTextCtrl;
	TextBox^ ClientSpecNameTextCtrl;

	Panel^ ErrorPanel;
	TextBlock^ ErrorMessageTextBlock;
	
	Button^ ClientSpecBrowseButton;
	Popup^ ClientSpecPopup;
	ListBox^ ClientSpecListBox;

	UBOOL bReadyForUse;

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( String^, ServerName);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( String^, UserName);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( String^, ClientSpecName);
};


//-----------------------------------------------
// API Specific open of source control project
//-----------------------------------------------

//This constructor is strictly for internal questions to perforce (get client spec list, etc)
FPerforceNET::FPerforceNET(String^ InServerName, String^ InUserName, String^ InClientSpec, String^ InTicket)
{
	//copy the connection information
	SourceControlProvider = NULL;

	EstablishConnection( InServerName, InUserName, InClientSpec, InTicket );
}

FPerforceNET::FPerforceNET(const FPerforceSourceControlProvider* InSourceControlProvider)
{
	//copy the connection information
	SourceControlProvider = InSourceControlProvider;

	String^ ServerName = CLRTools::ToString(InSourceControlProvider->GetPort());
	String^ UserName = CLRTools::ToString(InSourceControlProvider->GetUser());
	String^ ClientSpecName = CLRTools::ToString(InSourceControlProvider->GetClientSpec());
	String^ Ticket = CLRTools::ToString(InSourceControlProvider->GetTicket());

	EstablishConnection(ServerName, UserName, ClientSpecName, Ticket);
}


/** API Specific close of source control project*/
FPerforceNET::~FPerforceNET ()
{
	try
	{
		p4->Disconnect();
	}
	catch( Exception^ Ex )
	{
		warnf(NAME_SourceControl, TEXT("P4ERROR: Failed to disconnect from Server %s"), *CLRTools::ToFString(Ex->Message));
	}
}

/**
 * Static function in charge of making sure the specified connection is valid or requests that data from the user via dialog
 * @param InOutPortName - Port name in the inifile.  Out value is the port name from the connection dialog
 * @param InOutUserName - User name in the inifile.  Out value is the user name from the connection dialog
 * @param InOutClientSpecName - Client Spec name in the inifile.  Out value is the client spec from the connection dialog
 * @param InTicket - the credential to use for logging in to the Perforce server
 * @return - TRUE if the connection, whether via dialog or otherwise, is valid.  False if source control should be disabled
 */
UBOOL FPerforceNET::EnsureValidConnection(FString& InOutServerName, FString& InOutUserName, FString& InOutClientSpecName, FString& InTicket)
{
	UBOOL bEstablishedConnection = FALSE;

	String^ NewServerName = CLRTools::ToString(InOutServerName);
	String^ NewUserName = CLRTools::ToString(InOutUserName);
	String^ NewClientSpecName = CLRTools::ToString(InOutClientSpecName);
	String^ NewTicket = CLRTools::ToString(InTicket);

	P4Connection^ TestP4 = gcnew P4Connection();
	TestP4->CWD = CLRTools::ToString(appRootDir());

	while (!bEstablishedConnection)
	{
		if (NewServerName->Length && NewUserName->Length && NewClientSpecName->Length)
		{
			//attempt connection with given settings
			TestP4->Port   = NewServerName;
			TestP4->User   = NewUserName;
			TestP4->Client = NewClientSpecName;

			if( !String::IsNullOrEmpty( NewTicket ) )
			{
				TestP4->Password = NewTicket;
			}

			try
			{
				//assume good connection
				bEstablishedConnection = TRUE;
				//if the connection was never properly configured.  Still needs to be called on failure to properly clean up state.
				TestP4->Connect();

				//ensure the connection is valid
				if ( !TestP4->IsValidConnection( TRUE, TRUE ) )
				{
					bEstablishedConnection = FALSE;
					warnf(NAME_SourceControl, TEXT("P4ERROR: Invalid connection to source control provider:\n   Port=%s, User=%s, ClientSpec=%s, Ticket=%s"),
						*CLRTools::ToFString(NewServerName), 
						*CLRTools::ToFString(NewUserName), 
						*CLRTools::ToFString(NewClientSpecName),
						*CLRTools::ToFString(NewTicket)
						);
				}
			}
			catch( Exception^ Ex )
			{
				//Connection FAILED
				bEstablishedConnection = FALSE;
				warnf(NAME_SourceControl, TEXT("P4ERROR: Failed to connect to source control provider %s:\n   Port=%s, User=%s, ClientSpec=%s, Ticket=%s"),
					*CLRTools::ToFString(Ex->Message),
					*CLRTools::ToFString(NewServerName), 
					*CLRTools::ToFString(NewUserName), 
					*CLRTools::ToFString(NewClientSpecName),
					*CLRTools::ToFString(NewTicket)
					);
			}
			//whether successful or not, disconnect to clean up
			try
			{
				TestP4->Disconnect();
			}
			catch( Exception^ Ex )
			{
				//Disconnect FAILED
				bEstablishedConnection = FALSE;
				warnf(NAME_SourceControl, TEXT("P4ERROR: Failed to disconnect from Server %s"), *CLRTools::ToFString(Ex->Message));
			}
		}

		//if never specified, take the default connection values
		if (NewServerName->Length == 0)
		{
			NewServerName = TestP4->Port;
		}
		if (NewUserName->Length == 0)
		{
			NewUserName = TestP4->User;
		}
		if (NewClientSpecName->Length == 0)
		{
			NewClientSpecName = TestP4->Client;
		}

		//if we haven't connected yet, let's try and get new login information
		if (!bEstablishedConnection)
		{
			//if they hit escape - BAIL OUT!
			if (GetPerforceLogin(NewServerName, NewUserName, NewClientSpecName, NewTicket) == PerforceConstants::LoginCanceled)
			{
				break;
			}
		}
	}

	if (bEstablishedConnection)
	{
		InOutServerName = CLRTools::ToFString(NewServerName);
		InOutUserName = CLRTools::ToFString(NewUserName);
		InOutClientSpecName = CLRTools::ToFString(NewClientSpecName);
	}

	return bEstablishedConnection;
}

/** 
 * Execute Command
 * @param InCommand - Command to execute
 */
UBOOL FPerforceNET::ExecuteCommand(FSourceControlCommand* InCommand)
{
	if(bEstablishedConnnection)
	{
		P4RecordSet^ Records;
		switch (InCommand->CommandType)
		{
			case SourceControl::CHECK_OUT:
				Records = RunCommand(TEXT("edit"), InCommand->Params, InCommand->ErrorMessages);
				break;
			case SourceControl::CHECK_IN:
				{
					P4PendingChangelist^ ChangeList = p4->CreatePendingChangelist(CLRTools::ToString(InCommand->Description));

					//Add changelist information to params
					InCommand->Params.InsertItem(TEXT("-c"), 0);
					InCommand->Params.InsertItem(CLRTools::ToFString(ChangeList->Number.ToString()), 1);
					Records = RunCommand(TEXT("reopen"), InCommand->Params, InCommand->ErrorMessages);

					ChangeList->Submit();
				}
				break;
			case SourceControl::ADD_FILE:
				{
					FString NewTitle = FString::Printf(TEXT("ADD: %s"), *InCommand->Description);
					P4PendingChangelist^ ChangeList = p4->CreatePendingChangelist(CLRTools::ToString(NewTitle));

					//Add changelist information to params
					InCommand->Params.InsertItem(TEXT("-c"), 0);
					InCommand->Params.InsertItem(CLRTools::ToFString(ChangeList->Number.ToString()), 1);
					Records = RunCommand(TEXT("add"), InCommand->Params, InCommand->ErrorMessages);

					ChangeList->Submit();
				}
				break;
			case SourceControl::ADD_TO_DEFAULT_CHANGELIST:
				{
					Records = RunCommand(TEXT("add"), InCommand->Params, InCommand->ErrorMessages);
				}
				break;
			case SourceControl::DELETE_FILE:
				Records = RunCommand(TEXT("delete"), InCommand->Params, InCommand->ErrorMessages);
				break;
			case SourceControl::REVERT:
				Records = RunCommand(TEXT("revert"), InCommand->Params, InCommand->ErrorMessages);
				break;
			case SourceControl::REVERT_UNCHANGED:
				InCommand->Params.InsertItem(TEXT("-a"), 0);
				Records = RunCommand(TEXT("revert"), InCommand->Params, InCommand->ErrorMessages);
				break;
			case SourceControl::GET_MODIFIED_FILES:
				// Query for open files different than the versions stored in Perforce
				InCommand->Params.InsertItem(TEXT("-sa"), 0);
				Records = RunCommand(TEXT("diff"), InCommand->Params, InCommand->ErrorMessages);
				
				// Parse the results and store them in the command
				ParseDiffResults(InCommand, Records);
				break;
			case SourceControl::GET_UNMODIFIED_FILES:
				// Query for open files completely unmodified from the versions stored in Perforce
				InCommand->Params.InsertItem(TEXT("-sr"), 0);
				Records = RunCommand(TEXT("diff"), InCommand->Params, InCommand->ErrorMessages);

				// Parse the results and store them in the command
				ParseDiffResults(InCommand, Records);
				break;
			case SourceControl::UPDATE_STATUS:
				{
					UBOOL bStandardDebugOutput = FALSE;
					UBOOL bAttemptRetry = FALSE;
					Records = RunCommand(TEXT("fstat"), InCommand->Params, InCommand->ErrorMessages, bStandardDebugOutput, bAttemptRetry);
					//DebugPrintRecordSet(Records);
					ParseUpdateStatusResults(InCommand, Records);
				}
				break;
			case SourceControl::HISTORY:
				InCommand->Params.InsertItem(TEXT("-s"), 0);
				//include branching history
				InCommand->Params.InsertItem(TEXT("-i"), 0);
				//include truncated change list descriptions
				InCommand->Params.InsertItem(TEXT("-L"), 0);
				//include time stamps
				InCommand->Params.InsertItem(TEXT("-t"), 0);
				//limit to last 100 changes
				InCommand->Params.InsertItem(TEXT("-m"), 0);
				InCommand->Params.InsertItem(TEXT("100"), 1);
				Records = RunCommand(TEXT("filelog"), InCommand->Params, InCommand->ErrorMessages);
				//DebugPrintRecordSet(Records);
				ParseHistoryResults(InCommand, Records);
				break;
			case SourceControl::INFO:
				// Query information about the current client and server.
				InCommand->Params.InsertItem(TEXT("-s"), 0);
				Records = RunCommand(TEXT("info"), InCommand->Params, InCommand->ErrorMessages);
				break;
			default:
				//Command implementation in provider required
				check(FALSE);
		}
		// If the command has any error messages associated with it, flag that a command error occurred
		if ( InCommand->ErrorMessages.Num() > 0 )
		{
			InCommand->ErrorType = FSourceControlCommand::CET_CommandError;
		}

		return ( Records != nullptr );
	}
	else
	{
		// If unable to establish connection, the command should report a connection error occurred, preventing it
		// from being executed successfully
		InCommand->ErrorType = FSourceControlCommand::CET_ConnectionError;
	}
	return FALSE;

}

/**
 * Get List of ClientSpecs
 * @param InUserName - The username who should own the client specs in the list
 * @return - List of client spec names
 */
#undef GetEnvironmentVariable
List<String^>^ FPerforceNET::GetClientSpecList (String^ InUserName, TArray<FString>& OutErrorMessages)
{
	List<String^>^ ClientSpecList = gcnew List<String^>();
	if(bEstablishedConnnection)
	{
		TArray<FString> Params;
		UBOOL bAllowWildHosts = !GIsBuildMachine;
		Params.AddItem(TEXT("-u"));
		Params.AddItem(CLRTools::ToFString(InUserName));

		P4RecordSet^ Records = RunCommand(TEXT("clients"), Params, OutErrorMessages);
		//DebugPrintRecordSet(Records);
		
		if (Records && Records->Records->Length > 0 )
		{
			String^ ApplicationPath = CLRTools::ToString(appRootDir())->ToLower();
			String^ ComputerName = Environment::GetEnvironmentVariable(TEXT("P4HOST"));
			if (ComputerName == nullptr || ComputerName == TEXT(""))
			{
				ComputerName = CLRTools::ToString(appComputerName())->ToLower();
			}
			else
			{
				ComputerName = ComputerName->ToLower();
			}

			for (INT i = 0; i < Records->Records->Length; ++i)
			{
				P4Record^ ClientRecord = Records->Records[i];
				String^ ClientName = ClientRecord->Fields["client"];
				String^ HostName = ClientRecord->Fields["Host"];
				String^ ClientRootPath = ClientRecord->Fields["Root"]->ToLower();

				//this clientspec has to be meant for this machine ( "" hostnames mean any host can use ths clientspec in p4 land)
				UBOOL bHostNameMatches = (ComputerName == HostName->ToLower());
				UBOOL bHostNameWild = (HostName==TEXT(""));

				if( bHostNameMatches || (bHostNameWild && bAllowWildHosts) )
				{
					//make sure all slashes point the same way
					ClientRootPath = ClientRootPath->Replace("/", PATH_SEPARATOR);
					ApplicationPath = ApplicationPath->Replace("/", PATH_SEPARATOR);

					if (!ClientRootPath->EndsWith(PATH_SEPARATOR))
					{
						ClientRootPath += PATH_SEPARATOR;
					}
					//Only allow paths in that ACTUALLY are legit for this application
					if (ApplicationPath->IndexOf(ClientRootPath)!=-1)
					{
						ClientSpecList->Add(ClientName);
					}
					else
					{
						warnf(NAME_SourceControl, TEXT(" %s client specs rejected due to root directory mismatch (%s)"), *CLRTools::ToFString(ClientName), *CLRTools::ToFString(ClientRootPath));
					}

					//Other useful fields: Description, Owner, Host
				}
				else
				{
					warnf(NAME_SourceControl, TEXT(" %s client specs rejected due to Host Name mismatch (%s)"), *CLRTools::ToFString(ClientName), *CLRTools::ToFString(HostName));
				}
			}
		}
	}
	return ClientSpecList;
}


/**
 * Run a simple Perforce command with retry
*/
P4RecordSet^ FPerforceNET::RunCommand(const FString& InCommand, const TArray<FString>& InParameters, TArray<FString>& OutErrorMessages, const UBOOL bStandardDebugOutput, const UBOOL bAllowRetry)
{
	P4RecordSet^ Output;
	bool bTransactionCompleted = false;
	int RetryCount = 0;

	// Work out the full description of the command
	List<String^>^ ManagedParameters = CLRTools::ToStringArray(InParameters);
	String^ Command = CLRTools::ToString(InCommand);

	// Use a StringBuilder to concatenate these strings so that we don't end up using a ton of
	// scratch memory for intermediate strings
	Text::StringBuilder^ FullCommandBuilder = gcnew Text::StringBuilder( Command );
	for (INT i = 0; i < InParameters.Num(); ++i)
	{
		FullCommandBuilder->Append( " " );
		FullCommandBuilder->Append( ManagedParameters[i] );
	}

	// Convert the StringBuilder to a regular string.  Note that this may be a very large string when
	// querying state for many packages at once
	String^ FullCommand = FullCommandBuilder->ToString();

	// Try the Perforce operation with retry if necessary
	while( !bTransactionCompleted )
	{
		try
		{
			if (bStandardDebugOutput)
			{
				OutErrorMessages.AddItem(FString::Printf(TEXT("Attempting 'p4 %s '"), *CLRTools::ToFString(FullCommand)));
				if( RetryCount > 0 )
				{
					OutErrorMessages.AddItem(FString::Printf(TEXT(" (retry #%d"), RetryCount));
				}
			}

			Output = p4->Run( Command, ManagedParameters->ToArray() );

			bTransactionCompleted = true;
		}
		catch( Exception^ Ex )
		{
			// The strings returned from Perforce might contain "%" characters in them (they act as a P4 wildcard), which will cause 
			// issues if the strings are ever later sent to debugf, etc. Replace any % found in the string with its escaped version.
			FString CommandString = CLRTools::ToFString(FullCommand);
			CommandString.ReplaceInline( TEXT("%"), TEXT("%%") );
			
			FString ErrorMessageString = CLRTools::ToFString(Ex->Message);
			ErrorMessageString.ReplaceInline( TEXT("%"), TEXT("%%") );

			OutErrorMessages.AddItem(FString::Printf(TEXT("P4ERROR: Failed Perforce command 'p4 %s'"), *CommandString));
			OutErrorMessages.AddItem(FString::Printf(TEXT("P4ERROR: Error Message - %s"), *ErrorMessageString));

			// If we're retrying, sleep for a bit and loop
			if( RetryCount < PerforceConstants::MaxRetryCount )
			{
				RetryCount++;

				// Allow the dialog to be responsive while sleeping
				int MaxCount = PerforceConstants::RetrySleepTimeInMS / 100;
				for (INT Counter = 0; Counter < MaxCount; Counter++)
				{
					//Thread.Sleep( 100 );
				}
			}
			else
			{
				// Otherwise, set the error and mark the transaction complete
				bTransactionCompleted = true;
			}
		}
		if (!bAllowRetry)
		{
			bTransactionCompleted = true;
		}
	}

	return ( Output );
}

/**
 * Make a valid connection if possible
 */
void FPerforceNET::EstablishConnection(String^ InServerName, String^ InUserName, String^ InClientSpecName, String^ InTicket)
{
	//Connection assumed successful
	bEstablishedConnnection = TRUE;

	try
	{
		p4 = gcnew P4Connection();

		//Set configuration based params
		p4->Port = InServerName;
		p4->User = InUserName;

		if (!String::IsNullOrEmpty(InClientSpecName))
		{
			p4->Client = InClientSpecName;
		}

		if (!String::IsNullOrEmpty(InTicket))
		{
			p4->Password = InTicket;
		}

		//set code params
		p4->CWD = CLRTools::ToString(appRootDir());
		p4->ExceptionLevel = P4ExceptionLevels::NoExceptionOnWarnings;
		//p4->ExceptionLevel = P4ExceptionLevels::ExceptionOnBothErrorsAndWarnings;

		//execute the connection to perforce using the above settings
		p4->Connect();

		//ensure the connection is valid
		if ( !p4->IsValidConnection( TRUE, FALSE ) )
		{
			bEstablishedConnnection = FALSE;
			warnf(NAME_SourceControl, TEXT("P4ERROR: Invalid connection to server."));
		}
	} 
	catch( Exception^ Ex )
	{
		//connection is un-usable
		bEstablishedConnnection = FALSE;
		//disconnect happens during de-construction

		warnf(NAME_SourceControl, TEXT("P4ERROR: Connecting to Server %s"), *CLRTools::ToFString(Ex->Message));
	}
}

/**
 * Function to dissect the results from an UpdateStatus Command
 */
void FPerforceNET::ParseUpdateStatusResults(FSourceControlCommand* InCommand, P4RecordSet^ Records)
{
	if (Records && Records->Records->Length > 0 )
	{
		for (INT i = 0; i < Records->Records->Length; ++i)
		{
			P4Record^ ClientRecord = Records->Records[i];
			String^ FileName = ClientRecord->Fields["clientFile"];
			String^ HeadRev  = ClientRecord->Fields["headRev"];
			String^ HaveRev  = ClientRecord->Fields["haveRev"];
			String^ OtherOpen = ClientRecord->Fields["otherOpen"];
			String^ OpenType = ClientRecord->Fields["type"];
			String^ HeadAction = ClientRecord->Fields["headAction"];
			TMap<FString, FString> RecordResultsMap;

			FFilename FullPath = CLRTools::ToFString(FileName);

			if (HeadRev != nullptr)
			{
				RecordResultsMap.Set(TEXT("HeadRev"), CLRTools::ToFString(HeadRev));
			}
			if (HaveRev != nullptr)
			{
				RecordResultsMap.Set(TEXT("HaveRev"), CLRTools::ToFString(HaveRev));
			}
			if (HeadAction != nullptr)
			{
				RecordResultsMap.Set(TEXT("HeadAction"), CLRTools::ToFString(HeadAction));
			}

			if (OtherOpen != nullptr)
			{
				RecordResultsMap.Set(TEXT("OtherOpen"), CLRTools::ToFString(OtherOpen));
			}
			if (OpenType != nullptr)
			{
				RecordResultsMap.Set(TEXT("OpenType"), CLRTools::ToFString(OpenType));
			}

			InCommand->Results.Set( FullPath.GetBaseFilename(), RecordResultsMap);
		}
	}
}

/**
 * Updates the name of the branch based on location of execution.
 */

FString FPerforceNET::UpdateBranchName( void )
{
	FString BranchName = *GConfig->GetStr( TEXT( "GameAssetDatabase" ), TEXT( "BranchName" ), GEditorIni );
	
	const UBOOL bBranchNameIsEmpty = BranchName.Len() == 0;
	const INT IndexOfSlashInBranchName = (bBranchNameIsEmpty) ? INDEX_NONE : BranchName.InStr(TEXT("/")); 
	
	if( bBranchNameIsEmpty )
	{
		// Branch name is not configured in INI file, so guess it based on P4 location.
		UBOOL bBranchNameGood = FALSE;
		TArray<FString> InParameters;
		TArray<FString> OutErrorMessages;

		// The P4 "where" command will return a location in the depot.
		// We will try to use it to guess which branch we are in.
		P4RecordSet^ Records = RunCommand(TEXT("where"), InParameters, OutErrorMessages);

		if ( Records && Records->Records->Length > 0 )
		{
			const INT TrailingTextLen = appStrlen( TEXT("/...") );
			
			// Iterate over each record, extracting the relevant information for each
			for ( INT RecordIndex = 0; RecordIndex < Records->Records->Length; ++RecordIndex )
			{
				P4Record^ ClientRecord = Records->Records[ RecordIndex ];

				// Extract the branch name;
				// They are of the form //depot/UnrealEngine3-SomeName/Possible/Path/To/Exe/...
				if( ClientRecord->Fields->ContainsKey("depotFile") )
				{
					String^ FileName = ClientRecord->Fields["depotFile"];
					FString FullPath = CLRTools::ToFString( FileName );
					// remove "//depot/" from the start of the path
					FullPath = FullPath.Right( FullPath.Len() - appStrlen( TEXT("//depot/") ) );
					INT IndexOfSlash = FullPath.InStr( TEXT("/") );
					// remove everything after the first '/'.
					if (IndexOfSlash != INDEX_NONE)
					{						
						FullPath = FullPath.Left( IndexOfSlash );
					}

					BranchName = FullPath;
					bBranchNameGood = INDEX_NONE == FullPath.InStr(TEXT("/"));
				}
			}
		}
		if( !bBranchNameGood )
		{
			debugf( TEXT( "WARNING: No branch could be determined through P4" ) );
		}
		else
		{
			GConfig->SetString( TEXT( "GameAssetDatabase" ), TEXT( "BranchName" ), *BranchName, GEditorIni );
		}
	}
	else if ( INDEX_NONE != IndexOfSlashInBranchName )
	{
		// The branch name was corrupted be a previous version of this function.
		// This resulted a path of the form UnrealEngine3-BranchSpecifier/Possible/Extra/Path/Nodes
		// The branch name we want is just UnrealEngine3-BranchSpecifier
		BranchName = BranchName.Left( IndexOfSlashInBranchName );
		GConfig->SetString( TEXT( "GameAssetDatabase" ), TEXT( "BranchName" ), *BranchName, GEditorIni );
	}
	return BranchName;
}

/**
 * Helper method to parse the results of a history query command
 *
 * @param	InCommand	History command that was executed
 * @param	Records		RecordSet that results from the provided command's execution
 */
void FPerforceNET::ParseHistoryResults(FSourceControlCommand* InCommand, P4RecordSet^ Records)
{
	if ( Records && Records->Records->Length > 0 )
	{
		const INT DepotTextLen = appStrlen( TEXT("//depot/") );
		
		// Iterate over each record, extracting the relevant information for each
		for ( INT RecordIndex = 0; RecordIndex < Records->Records->Length; ++RecordIndex )
		{
			P4Record^ ClientRecord = Records->Records[RecordIndex];
			
			// Extract the file name (and remove the "//depot/" text that precedes it)
			check( ClientRecord->Fields->ContainsKey("depotFile") );
			String^ FileName = ClientRecord->Fields["depotFile"];
			FString FullPath = CLRTools::ToFString( FileName );
			FullPath = FullPath.Right( FullPath.Len() - DepotTextLen );

			// Extract the array of revision numbers
			check( ClientRecord->ArrayFields->ContainsKey("rev") );
			array<String^>^ RevisionNumbers = ClientRecord->ArrayFields["rev"];
			check( RevisionNumbers != nullptr );

			// Extract the array of user names
			check( ClientRecord->ArrayFields->ContainsKey("user") );
			array<String^>^ UserNames = ClientRecord->ArrayFields["user"];
			check( UserNames != nullptr );

			// Extract the array of dates
			check( ClientRecord->ArrayFields->ContainsKey("time") );
			array<String^>^ Dates  = ClientRecord->ArrayFields["time"];
			check( Dates != nullptr );

			// Extract the array of changelist numbers
			check( ClientRecord->ArrayFields->ContainsKey("change") );
			array<String^>^ ChangelistNumbers = ClientRecord->ArrayFields["change"];
			check( ChangelistNumbers != nullptr );

			// Extract the array of descriptions
			check( ClientRecord->ArrayFields->ContainsKey("desc") );
			array<String^>^ Descriptions = ClientRecord->ArrayFields["desc"];
			check( Descriptions != nullptr );

			// Extract the array of file sizes
			check( ClientRecord->ArrayFields->ContainsKey("fileSize") );
			array<String^>^ FileSizes = ClientRecord->ArrayFields["fileSize"];
			check( FileSizes != nullptr );

			// Extract the array of clientspecs/workspaces
			check( ClientRecord->ArrayFields->ContainsKey("client") );
			array<String^>^ ClientSpecs = ClientRecord->ArrayFields["client"];
			check( ClientSpecs != nullptr );

			// Extract the array of actions
			check( ClientRecord->ArrayFields->ContainsKey("action") );
			array<String^>^ Actions = ClientRecord->ArrayFields["action"];
			check( Actions != nullptr );

			// Verify that each array is the same length or an error has occurred
			// NOTE: P4.NET has an unfortunate bug in which any time a file deletion has occurred,
			// the file size array is given an extra null entry in the file size array, in addition to
			// its normal, expected entry of the empty string. This makes the size of the file size array
			// unpredictable and it requires special processing.
			check(	RevisionNumbers->Length == UserNames->Length &&
					RevisionNumbers->Length == Dates->Length &&
					RevisionNumbers->Length == ChangelistNumbers->Length &&
					RevisionNumbers->Length == Descriptions->Length &&
					RevisionNumbers->Length == ClientSpecs->Length &&
					RevisionNumbers->Length == Actions->Length);

			// The source control command contains a mapping of strings, so each array can't just be simply placed within the
			// map. Instead, each array will be condensed into a delimited string to be parsed back later.
			FString RevisionNumberString;
			FString UserNamesString;
			FString DatesString;
			FString ChangelistNumbersString;
			FString DescriptionsString;
			FString FileSizesString;
			FString ClientSpecsString;
			FString ActionsString;
			const TCHAR* EntryDelimiter = FSourceControl::FSourceControlFileHistoryInfo::FILE_HISTORY_ITEM_DELIMITER;
			
			// Track the index into the file size array separately due to the P4.NET bug described above
			INT FileSizeIndex = 0;
			for ( INT RevisionIndex = 0; RevisionIndex < RevisionNumbers->Length; ++RevisionIndex )
			{
				RevisionNumberString += CLRTools::ToFString( RevisionNumbers[RevisionIndex] ) + EntryDelimiter;
				UserNamesString += CLRTools::ToFString( UserNames[RevisionIndex] ) + EntryDelimiter;
				DatesString += CLRTools::ToFString( Dates[RevisionIndex] ) + EntryDelimiter;
				ChangelistNumbersString += CLRTools::ToFString( ChangelistNumbers[RevisionIndex] ) + EntryDelimiter;
				DescriptionsString += CLRTools::ToFString( Descriptions[RevisionIndex] ) + EntryDelimiter;
				
				// Continue to increment the file size index any time a null entry in its array is detected (as it has
				// been incorrectly added as a bug in P4.NET)
				while ( FileSizes[FileSizeIndex] == nullptr && FileSizeIndex < FileSizes->Length )
				{
					++FileSizeIndex;
				}

				// If there's a valid file size provided, add it to the string; otherwise just specify the size is zero
				if ( FileSizeIndex < FileSizes->Length && !String::IsNullOrEmpty( FileSizes[FileSizeIndex] ) )
				{
					FileSizesString += CLRTools::ToFString( FileSizes[FileSizeIndex] ) + EntryDelimiter;
				}
				else
				{
					FileSizesString += FString::Printf( TEXT("0%s"), EntryDelimiter );
				}
				++FileSizeIndex;

				ClientSpecsString += CLRTools::ToFString( ClientSpecs[RevisionIndex] ) + EntryDelimiter;
				ActionsString += CLRTools::ToFString( Actions[RevisionIndex] ) + EntryDelimiter;
			}

			// Set each delimited string into the map of the SCC command so that it can be retrieved later
			const TCHAR* FileHistoryKey = FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_HistoryKey );
			TMap<FString, FString> ResultsMap;
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_FileNameKey ), FullPath );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_NumRevisionsKey ), FString::Printf( TEXT("%d"), RevisionNumbers->Length ) );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_RevisionNumKey ), RevisionNumberString );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_UserNameKey ), UserNamesString );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_DateKey ), DatesString );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_ChangelistNumKey ), ChangelistNumbersString );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_DescriptionKey ), DescriptionsString );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_FileSizeKey ), FileSizesString );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_ClientSpecKey ), ClientSpecsString );
			ResultsMap.Set( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_ActionKey ), ActionsString );
			InCommand->Results.Set( FString::Printf( TEXT("%s%d"),  FileHistoryKey, RecordIndex ), ResultsMap );
		}
	}
}

/**
 * Helper method to dissect the results from a get modified/unmodified files (aka "diff") command
 *
 * @param	InCommand	Command to set parsed results for
 * @param	Records		Results from the diff command to parse
 */
void FPerforceNET::ParseDiffResults(FSourceControlCommand* InCommand, P4RecordSet^ Records)
{
	// Iterate over each record found as a result of the command, parsing it for relevant information
	if ( Records && Records->Records->Length > 0 )
	{
		for ( INT RecordIndex = 0; RecordIndex < Records->Records->Length; ++RecordIndex )
		{
			P4Record^ ClientRecord = Records->Records[RecordIndex];
			String^ FileName = ClientRecord->Fields["clientFile"];
			String^ DepotFileName = ClientRecord->Fields["depotFile"];
			String^ Rev  = ClientRecord->Fields["rev"];
			String^ OpenType = ClientRecord->Fields["type"];
			TMap<FString, FString> RecordResultsMap;

			check( FileName );
			FFilename FullPath = CLRTools::ToFString( FileName );

			if ( DepotFileName != nullptr )
			{
				RecordResultsMap.Set( TEXT("DepotFileName"), CLRTools::ToFString( DepotFileName ) );
			}
			if ( Rev != nullptr )
			{
				RecordResultsMap.Set(TEXT("Rev"), CLRTools::ToFString( Rev ) );
			}
			if ( OpenType != nullptr )
			{
				RecordResultsMap.Set( TEXT("OpenType"), CLRTools::ToFString( OpenType ) );
			}

			// Set the results into the command's map
			InCommand->Results.Set( FullPath.GetBaseFilename(), RecordResultsMap );
		}
	}
}


/**
 * Debug helper that will print column heads and values for a record set
 */
void FPerforceNET::DebugPrintRecordSet (P4RecordSet^ Records)
{
	if (Records && Records->Records->Length > 0 )
	{
		for (INT i = 0; i < Records->Records->Length; ++i)
		{
			P4Record^ ClientRecord = Records->Records[i];
			for each (String^ Key in ClientRecord->Fields->Keys)
			//for (INT KeyIndex = 0; KeyIndex < ClientRecord->Keys; ++KeyIndex)
			{
				FString KeyName  = CLRTools::ToFString(Key);
				FString KeyValue = CLRTools::ToFString(ClientRecord->Fields[Key]);
				warnf(NAME_SourceControl, TEXT("P4Debug: Record Set %d %s %s"), i, *KeyName, *KeyValue);
			}
			for each (String^ Key in ClientRecord->ArrayFields->Keys)
			//for (INT KeyIndex = 0; KeyIndex < ClientRecord->Keys; ++KeyIndex)
			{
				FString KeyName  = CLRTools::ToFString(Key);
				INT ArrayIndex = 0;
				for each (String^ Value in ClientRecord->ArrayFields[Key])
				{
					if (Value != nullptr)
					{
						FString KeyValue = CLRTools::ToFString(Value);
						warnf(NAME_SourceControl, TEXT("P4Debug: Record Set %d, Array Index %d %s %s"), i, ArrayIndex, *KeyName, *KeyValue);
						ArrayIndex++;
					}
				}
			}
		}
	}
}
/** Displays the perforce login screen and reports back whether "OK" or "Cancel" was selected */
PerforceConstants::LoginResults FPerforceNET::GetPerforceLogin (String^% InOutServerName, String^% InOutUserName, String^% InOutClientSpecName, String^ InTicket)
{
	PerforceConstants::LoginResults Result = PerforceConstants::LoginCanceled;

	//before even trying to summon the window, try to "smart" connect with the default server/username
	TArray<FString> ErrorMessages;
	FPerforceNET Connection(InOutServerName, InOutUserName, InOutClientSpecName, InTicket);
	List<String^>^ ClientSpecList = Connection.GetClientSpecList(InOutUserName, ErrorMessages);
	//if only one client spec matched (and default connection info was correct)
	if (ClientSpecList->Count == 1)
	{
		InOutClientSpecName = ClientSpecList[0];
		return PerforceConstants::LoginAccepted;
	}
	else
	{
		warnf(NAME_SourceControl, TEXT("Source Control unable to auto-login due to ambiguous client specs"));
		warnf(NAME_SourceControl, TEXT("  The next launch of the editor will display a dialog allowing client spec selection"));
		warnf(NAME_SourceControl, TEXT("  Please ensure that in GAMEEditorUserSettings.ini [SourceControl] has Disabled=False"));
		warnf(NAME_SourceControl, TEXT("  If you are unable to run the editor, consider checking out the files by hand temporarily until the editor has a chance to store your login credentials"));

		// List out the clientspecs that were found to be ambiguous
		warnf(NAME_SourceControl, TEXT("\r\nAmbiguous clientspecs ..."));
		for each( String^ AmbiguousClientSpec in ClientSpecList )
		{
			warnf(NAME_SourceControl, TEXT(" ... %s"),*CLRTools::ToFString(AmbiguousClientSpec));
		}
	}

	//if your not on a build system or running a commandlet
	if (!GIsUnattended && !GIsUCC)
	{
		WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
		Settings->WindowTitle = CLRTools::ToString(*LocalizeUnrealEd("PerforceLoginTitle"));
		Settings->bForceToFront = TRUE;
		Settings->bUseWxDialog = FALSE;

		MWPFFrame^ LoginFrame = gcnew MWPFFrame(NULL, Settings, TEXT("PerforceLogin"));
	
		//No need to append, as the constructor does that automatically
		MPerforceLoginPanel^ LoginPanel = gcnew MPerforceLoginPanel(CLRTools::ToString(TEXT("PerforceLoginWindow.xaml")), InOutServerName, InOutUserName, InOutClientSpecName);

		//make the window modal until they either accept or cancel
		Result = (PerforceConstants::LoginResults)LoginFrame->SetContentAndShowModal(LoginPanel, PerforceConstants::LoginCanceled);
		if (Result == PerforceConstants::LoginAccepted)
		{
			LoginPanel->GetResult(InOutServerName, InOutUserName, InOutClientSpecName);
		}

		//de-allocate the frame
		delete Settings;
		delete LoginFrame;
	}
	else
	{
		warnf(NAME_SourceControl, TEXT("Source Control unable to connect during automated process or commandlet"));
	}

	return Result;
}


#endif

/** 
 * INTERFACE CLASS
 */

/** Init of connection with source control server */
void FPerforceSourceControlProvider::Init (void)
{
	LoadSettings();
	if (bDisabled)
	{
		return;
	}
#ifdef __cplusplus_cli
	FPerforceNET Connection(this);
	Connection.UpdateBranchName();
#endif // __cplusplus_cli
}

/** API Specific close the connection with source control server*/
#pragma unmanaged
void FPerforceSourceControlProvider::Close (void)
{
	bDisabled = TRUE;
	bServerAvailable = FALSE;
}
#pragma managed

/** 
 * Execute Command
 * @param InCommand - Command to execute
 */
UBOOL FPerforceSourceControlProvider::ExecuteCommand(FSourceControlCommand* InCommand) const
{
	UBOOL Result = FALSE;
#ifdef __cplusplus_cli
	if(bDisabled)
	{
		return Result;
	}

	FPerforceNET Connection(this);
	InCommand->bCommandSuccessful = Connection.ExecuteCommand(InCommand);
#endif
	return InCommand->bCommandSuccessful;
}

/**
 * Checks the provided command's error type, and responds accordingly
 *
 * @param	InCommand	Command to check the error type of
 */
void FPerforceSourceControlProvider::RespondToCommandErrorType(const FSourceControlCommand& InCommand)
{
	switch (InCommand.ErrorType)
	{
		// Respond to clean (no error) execution
	case FSourceControlCommand::CET_NoError:
		bServerAvailable = TRUE;
		break;

		// Respond to error with the command itself
	case FSourceControlCommand::CET_CommandError:
		break;

		// Respond to a connection error while attempting the command
	case FSourceControlCommand::CET_ConnectionError:
		bServerAvailable = FALSE;
		break;
	}
}

/**
 * Loads user/SCC information from the command line or INI file.
 */
void FPerforceSourceControlProvider::LoadSettings()
{
#ifdef __cplusplus_cli

	UBOOL bFoundCmdLineSettings=FALSE;
	// Look for a specified mod name and provide a default if none present.
	bFoundCmdLineSettings = Parse(appCmdLine(), TEXT("P4Port="), PortName);
	bFoundCmdLineSettings |= Parse(appCmdLine(), TEXT("P4User="), UserName);
	bFoundCmdLineSettings |= Parse(appCmdLine(), TEXT("P4Client="), ClientSpecName);
	bFoundCmdLineSettings |= Parse(appCmdLine(), TEXT("P4Passwd="), Ticket);

	// enable source control if we found any settings on the commandline
	bDisabled = !bFoundCmdLineSettings;

	if( bDisabled )
	{
		// If source control is disabled, there were no settings specified on the commandline, read them from INI settings
		
		// see if source control is even enabled
		UBOOL LocalDisabled;
		GConfig->GetBool(TEXT("SourceControl"), TEXT("Disabled"), LocalDisabled, GEditorUserSettingsIni );
		bDisabled = (LocalDisabled && !GIsUnattended);

		// Attempt to read values from the INI file, only replacing the defaults if the value exists in the INI.
		GConfig->GetString( TEXT("SourceControl"), TEXT("PortName"), PortName, GEditorUserSettingsIni );
		GConfig->GetString( TEXT("SourceControl"), TEXT("UserName"), UserName, GEditorUserSettingsIni );
		GConfig->GetString( TEXT("SourceControl"), TEXT("ClientSpecName"), ClientSpecName, GEditorUserSettingsIni );
		// Never store a ticket in the ini files
	}

	if (!bDisabled)
	{
		UBOOL LocalAutoAddNewFiles = bAutoAddNewFiles;
		GConfig->GetBool( TEXT("SourceControl"), TEXT("AutoAddNewFiles"), LocalAutoAddNewFiles, GEditorUserSettingsIni );
		bAutoAddNewFiles = LocalAutoAddNewFiles;

		UBOOL bSuccessfulInitialConnection = FPerforceNET::EnsureValidConnection(PortName, UserName, ClientSpecName, Ticket);
		if (!bSuccessfulInitialConnection)
		{
			bDisabled = TRUE;
		}
		else
		{
			bServerAvailable = TRUE;
		}
	}

#else
	bDisabled = TRUE;
#endif

	if (bDisabled)
	{
		static UBOOL bAlreadyWarned = FALSE;
		if( !bAlreadyWarned )
		{
			warnf(NAME_SourceControl, TEXT("Source Control disabled in %sEditorUserSettings.ini.  [SourceControl] has Disabled=True"), GGameName);
			bAlreadyWarned = TRUE;
		}
	}

	//Save off settings so this doesn't happen every time
	SaveSettings();
}

/**
 * Saves user/SCC information to the INI file.
 */
#pragma unmanaged
void FPerforceSourceControlProvider::SaveSettings()
{
	//if running from a commandlet, and source control was unable to connect, do not save ini file settings.  Wait until next run of the editor
	if (GIsUnattended || GIsUCC)
	{
		return;
	}

	UBOOL LocalDisabled = bDisabled;
	GConfig->SetBool(TEXT("SourceControl"), TEXT("Disabled"), LocalDisabled, GEditorUserSettingsIni );

	if (!bDisabled)
	{
		// Attempt to read values from the INI file, only replacing the defaults if the value exists in the INI.
		GConfig->SetString( TEXT("SourceControl"), TEXT("PortName"), *PortName, GEditorUserSettingsIni );
		GConfig->SetString( TEXT("SourceControl"), TEXT("UserName"), *UserName, GEditorUserSettingsIni );
		GConfig->SetString( TEXT("SourceControl"), TEXT("ClientSpecName"), *ClientSpecName, GEditorUserSettingsIni );
	}
}
#pragma managed

#endif // HAVE_SCC && WITH_PERFORCE
