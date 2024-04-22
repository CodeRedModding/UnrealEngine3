/*=============================================================================
	TaskBrowser.cpp: Browser window for working with a task database
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "TaskBrowser.h"
#include "TaskDatabase.h"
#include "TaskDataManager.h"



/** Implements FString sorting */
IMPLEMENT_COMPARE_CONSTREF( FString, TaskBrowser, { return appStricmp( *A, *B ); } );




/** The supported column IDs and names for the task list.  This also specifics the default order of the
    columns, although the user can rearrange the actual order to their liking. */
namespace TaskListColumn
{
	/** ID list */
	namespace IDs
	{
		enum Type
		{
			/** The task number that usually identifies this task in the actual database */
			Number = 0,

			/** Priority of the task */
			Priority,

			/** Name of the task (quick summary) */
			Name,

			/** Who the task is currently assigned to */
			AssignedTo,
			
			/** Status of this task  */
			Status,

			/** Who created the task */
			CreatedBy,

			// ...

			/** The total number of column IDs.  Must be the last entry in the enum! */
			NumColumnIDs
		};
	}


	/** Name list */
	const TCHAR* Names[ IDs::NumColumnIDs ] =
	{
		TEXT( "TaskBrowser_TaskListColumnName_Number" ),
		TEXT( "TaskBrowser_TaskListColumnName_Priority" ),
		TEXT( "TaskBrowser_TaskListColumnName_Name" ),
		TEXT( "TaskBrowser_TaskListColumnName_AssignedTo" ),
		TEXT( "TaskBrowser_TaskListColumnName_Status" ),
		TEXT( "TaskBrowser_TaskListColumnName_CreatedBy" ),
	};

}



/**
 * Handles loading and saving of Task Browser user preferences
 */
class FTaskBrowserSettings
{

public:

	/** -=-=-=-=-=-=- Settings dialog -=-=-=-=-=-=-=- */

	/** Server name */
	FString ServerName;

	/** Server port */
	INT ServerPort;

	/** Login user name */
	FString UserName;

	/** Login password */
	FString Password;

	/** Project name */
	FString ProjectName;

	/** True if we should automatically connect to the server at startup */
	UBOOL bAutoConnectAtStartup;


	
	/** -=-=-=-=-=-=- Stored interface settings -=-=-=-=-=-=-=- */

	/** Default database filter name (not localized) */
	FString DBFilterName;

	/** Display filter for 'open tasks' */
	UBOOL bFilterOnlyOpen;

	/** Display filter for 'assigned to me' */
	UBOOL bFilterAssignedToMe;

	/** Display filter for 'created by me' */
	UBOOL bFilterCreatedByMe;

	/** Display filter for 'current map' */
	UBOOL bFilterCurrentMap;

	/** The column name to sort the task list by (not localized) */
	FString TaskListSortColumn;

	/** Denotes task list ascending/descending sort order */
	UBOOL bTaskListSortAscending;
	


public:

	/** FTaskBrowserSettings constructor */
	FTaskBrowserSettings()
		: ServerName(),
		  ServerPort( 80 ),
		  UserName(),
		  Password(),
		  ProjectName(),
		  bAutoConnectAtStartup( FALSE ),
		  DBFilterName(),
		  bFilterOnlyOpen( TRUE ),
		  bFilterAssignedToMe( FALSE ),
		  bFilterCreatedByMe( FALSE ),
		  bFilterCurrentMap( FALSE ),
		  TaskListSortColumn( TaskListColumn::Names[ TaskListColumn::IDs::Priority ] ),
		  bTaskListSortAscending( TRUE )
	{
		// Load default settings for Epic task database server here
		// NOTE: Licensees, you may want to replace the following to suit your own needs
#if EPIC_INTERNAL
		GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "EpicDefaultServerName" ), ServerName, GEditorIni );
		GConfig->GetInt( TEXT( "TaskBrowser" ), TEXT( "EpicDefaultServerPort" ), ServerPort, GEditorIni );
		GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "EpicDefaultProjectName" ), ProjectName, GEditorIni );
		GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "EpicDefaultDBFilterName" ), DBFilterName, GEditorIni );
#endif
	}


	/** Loads settings from the configuration file */
	void LoadSettings()
	{
		GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "ServerName" ), ServerName, GEditorUserSettingsIni );
		
		GConfig->GetInt( TEXT( "TaskBrowser" ), TEXT( "ServerPort" ), ServerPort, GEditorUserSettingsIni );

		GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "UserName" ), UserName, GEditorUserSettingsIni );

		// Load encrypted password from disk
		FString EncryptedPasswordBlob = GConfig->GetStr( TEXT( "TaskBrowser" ), TEXT( "Password" ), GEditorUserSettingsIni );
		Password = TEXT( "" );
		const UINT MaxEncryptedPasswordSize = 2048;
		BYTE EncryptedPasswordBuffer[ MaxEncryptedPasswordSize ];
		if( appStringToBlob( EncryptedPasswordBlob, EncryptedPasswordBuffer, MaxEncryptedPasswordSize ) )
		{
			const UINT MaxDecryptedPasswordSize = 2048;
			BYTE DecryptedPasswordBuffer[ MaxDecryptedPasswordSize ];
			const UINT ExpectedEncryptedPasswordSize = EncryptedPasswordBlob.Len() / 3;
			DWORD DecryptedPasswordSize = MaxDecryptedPasswordSize;
			if( appDecryptBuffer(
					EncryptedPasswordBuffer,
					ExpectedEncryptedPasswordSize,
					DecryptedPasswordBuffer,
					DecryptedPasswordSize ) )
			{
				FString DecryptedPassword = ( const TCHAR* )DecryptedPasswordBuffer;

				// Store password
				Password = DecryptedPassword;
			}
		}

		GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "ProjectName" ), ProjectName, GEditorUserSettingsIni );

		GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "AutoConnectAtStartup" ), bAutoConnectAtStartup, GEditorUserSettingsIni );

		GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "DBFilterName" ), DBFilterName, GEditorUserSettingsIni );

		GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "FilterOnlyOpen" ), bFilterOnlyOpen, GEditorUserSettingsIni );

		GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "FilterAssignedToMe" ), bFilterAssignedToMe, GEditorUserSettingsIni );
	
		GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "FilterCreatedByMe" ), bFilterCreatedByMe, GEditorUserSettingsIni );

		GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "FilterCurrentMap" ), bFilterCurrentMap, GEditorUserSettingsIni );

		GConfig->GetString( TEXT( "TaskBrowser" ), TEXT( "TaskListSortColumn" ), TaskListSortColumn, GEditorUserSettingsIni );

		GConfig->GetBool( TEXT( "TaskBrowser" ), TEXT( "TaskListSortAscending" ), bTaskListSortAscending, GEditorUserSettingsIni );
	}


	/** Saves settings to the configuration file */
	void SaveSettings()
	{
		GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "ServerName" ), *ServerName, GEditorUserSettingsIni );
		GConfig->SetInt( TEXT( "TaskBrowser" ), TEXT( "ServerPort" ), ServerPort, GEditorUserSettingsIni );
		GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "UserName" ), *UserName, GEditorUserSettingsIni );

		UBOOL bHaveValidPassword = FALSE;
		if( Password.Len() > 0 )
		{
			// Store the user's password encrypted on disk
			const UINT MaxEncryptedPasswordSize = 2048;
			BYTE EncryptedPasswordBuffer[ MaxEncryptedPasswordSize ];
			DWORD EncryptedPasswordSize = MaxEncryptedPasswordSize;
			if( appEncryptBuffer(
					( const BYTE* )&Password[ 0 ],
					( Password.Len() + 1 ) * sizeof( TCHAR ),
					EncryptedPasswordBuffer,
					EncryptedPasswordSize ) )
			{
				FString EncryptedPasswordBlob = appBlobToString( EncryptedPasswordBuffer, EncryptedPasswordSize );
				GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "Password" ), *EncryptedPasswordBlob, GEditorUserSettingsIni );
				bHaveValidPassword = TRUE;
			}
		}

		if( !bHaveValidPassword )
		{
			// Empty password
			GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "Password" ), TEXT( "" ), GEditorUserSettingsIni );
		}

		GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "ProjectName" ), *ProjectName, GEditorUserSettingsIni );

		GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "AutoConnectAtStartup" ), bAutoConnectAtStartup, GEditorUserSettingsIni );

		GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "DBFilterName" ), *DBFilterName, GEditorUserSettingsIni );

		GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "FilterOnlyOpen" ), bFilterOnlyOpen, GEditorUserSettingsIni );

		GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "FilterAssignedToMe" ), bFilterAssignedToMe, GEditorUserSettingsIni );

		GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "FilterCreatedByMe" ), bFilterCreatedByMe, GEditorUserSettingsIni );

		GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "FilterCurrentMap" ), bFilterCurrentMap, GEditorUserSettingsIni );

		GConfig->SetString( TEXT( "TaskBrowser" ), TEXT( "TaskListSortColumn" ), *TaskListSortColumn, GEditorUserSettingsIni );

		GConfig->SetBool( TEXT( "TaskBrowser" ), TEXT( "TaskListSortAscending" ), bTaskListSortAscending, GEditorUserSettingsIni );
	}



	/**
	 * Returns true if the current connection settings are valid
	 *
	 * @return	True if we have valid settings
	 */
	UBOOL AreConnectionSettingsValid() const
	{
		if( ServerName.Len() == 0 ||
			ServerPort == 0 ||
			UserName.Len() == 0 ||
			Password.Len() == 0 ||
			ProjectName.Len() == 0 )
		{
			return FALSE;
		}

		return TRUE;
	}

};




/**
 * Dialog window for configuring server settings and other task browser preferences
 */
class WxTaskBrowserConfigDialog
	: public wxDialog
{

public:

	/** Static: Displays the dialog, waits for the user to change settings and dismiss the dialog */
	static INT ExecuteModalDialog();


public:

	/** WxTaskBrowserConfigDialog constructor */
	WxTaskBrowserConfigDialog();

	/** WxTaskBrowserConfigDialog destructor */
	virtual ~WxTaskBrowserConfigDialog();

	/** Show the modal dialog */
	virtual int ShowModal();


protected:

	// WxWidgets event table
	DECLARE_EVENT_TABLE()



protected:

};




BEGIN_EVENT_TABLE( WxTaskBrowserConfigDialog, wxDialog )
	// ...
END_EVENT_TABLE()



/** WxTaskBrowserConfigDialog constructor */
WxTaskBrowserConfigDialog::WxTaskBrowserConfigDialog()
{
	// Load the window hierarchy from the .xrc file
	verify(
		wxXmlResource::Get()->LoadDialog(
			this,										// Self
			GApp->EditorFrame,							// Parent window
			TEXT( "ID_TaskBrowserConfigDialog" ) ) );;	// Object name

	// Localize all of the static text in the loaded window
	FLocalizeWindow( this, FALSE, TRUE );

	// @todo: Consider loading/saving window position? (not size though)
}



/** WxTaskBrowserConfigDialog destructor */
WxTaskBrowserConfigDialog::~WxTaskBrowserConfigDialog()
{
}



/** Show the modal dialog */
int WxTaskBrowserConfigDialog::ShowModal()
{
	return wxDialog::ShowModal();
}



/** Static: Displays the dialog, waits for the user to change settings and dismiss the dialog */
INT WxTaskBrowserConfigDialog::ExecuteModalDialog()
{
	FTaskBrowserSettings Settings;
	Settings.LoadSettings();


	// Create the dialog window
	WxTaskBrowserConfigDialog* ConfigDialog = new WxTaskBrowserConfigDialog();


	// Grab controls
	wxTextCtrl* ServerNameControl = static_cast< wxTextCtrl* >(
		ConfigDialog->FindWindow( TEXT( "ID_TaskBrowserConfig_ServerName" ) ) );
	check( ServerNameControl != NULL );

	wxTextCtrl* ServerPortControl = static_cast< wxTextCtrl* >(
		ConfigDialog->FindWindow( TEXT( "ID_TaskBrowserConfig_ServerPort" ) ) );
	check( ServerPortControl != NULL );

	wxTextCtrl* UserNameControl = static_cast< wxTextCtrl* >(
		ConfigDialog->FindWindow( TEXT( "ID_TaskBrowserConfig_UserName" ) ) );
	check( UserNameControl != NULL );

	wxTextCtrl* PasswordControl = static_cast< wxTextCtrl* >(
		ConfigDialog->FindWindow( TEXT( "ID_TaskBrowserConfig_Password" ) ) );
	check( PasswordControl != NULL );

	wxTextCtrl* ProjectNameControl = static_cast< wxTextCtrl* >(
		ConfigDialog->FindWindow( TEXT( "ID_TaskBrowserConfig_ProjectName" ) ) );
	check( ProjectNameControl != NULL );

	wxCheckBox* AutoConnectCheckBox = static_cast< wxCheckBox* >(
		ConfigDialog->FindWindow( TEXT( "ID_TaskBrowserConfig_AutoConnectAtStartup" ) ) );
	check( AutoConnectCheckBox != NULL );


	// Fill in controls from preferences
	ServerNameControl->SetValue( *Settings.ServerName );
	ServerPortControl->SetValue( *appItoa( Settings.ServerPort ) );
	UserNameControl->SetValue( *Settings.UserName );
	PasswordControl->SetValue( *Settings.Password );
	ProjectNameControl->SetValue( *Settings.ProjectName );
	AutoConnectCheckBox->SetValue( Settings.bAutoConnectAtStartup ? true : false );


	// Show the dialog and wait for user interaction
	INT DialogResult = ConfigDialog->ShowModal();


	// Did the user click OK?
	if( DialogResult == wxID_OK )
	{
		// Store new preferences from controls
		Settings.ServerName = ServerNameControl->GetValue().c_str();
		Settings.ServerPort = appAtoi( ServerPortControl->GetValue().c_str() );
		Settings.UserName = UserNameControl->GetValue().c_str();
		Settings.Password = PasswordControl->GetValue().c_str();
		Settings.ProjectName = ProjectNameControl->GetValue().c_str();
		Settings.bAutoConnectAtStartup = AutoConnectCheckBox->GetValue();

		// Save preferences to disk
		Settings.SaveSettings();
	}

	return DialogResult;
}






/**
 * Dialog window for marking tasks as completed
 */
class WxTaskBrowserCompleteTaskDialog
	: public wxDialog
{

public:

	/**
	 * Static: Displays the dialog, waits for data, then dismisses the dialog
	 *
	 * @param	InResolutionValues	List of valid task resolution options
	 * @param	OutResolutionData	[Out] User-entered resolution data
	 *
	 * @return	True if the user wants to continue to mark the task(s) as completed
	 */
	static UBOOL ExecuteModalDialog( const TArray< FString >& InResolutionValues, FTaskResolutionData& OutResolutionData );


public:

	/** WxTaskBrowserConfigDialog constructor */
	WxTaskBrowserCompleteTaskDialog();

	/** WxTaskBrowserConfigDialog destructor */
	virtual ~WxTaskBrowserCompleteTaskDialog();

	/** Show the modal dialog */
	virtual int ShowModal();

	/** Called when the OK button is pressed to validate the contents of the window */
	virtual bool Validate();



protected:

	// WxWidgets event table
	DECLARE_EVENT_TABLE()



protected:

};




BEGIN_EVENT_TABLE( WxTaskBrowserCompleteTaskDialog, wxDialog )
	// ...
END_EVENT_TABLE()



/** WxTaskBrowserCompleteTaskDialog constructor */
WxTaskBrowserCompleteTaskDialog::WxTaskBrowserCompleteTaskDialog()
{
	// Load the window hierarchy from the .xrc file
	verify(
		wxXmlResource::Get()->LoadDialog(
			this,												// Self
			GApp->EditorFrame,									// Parent window
			TEXT( "ID_TaskBrowserCompleteTaskDialog" ) ) );;	// Object name

	// Localize all of the static text in the loaded window
	FLocalizeWindow( this, FALSE, TRUE );

	// @todo: Considering loading/saving window position? (size too if resizable)
}



/** WxTaskBrowserCompleteTaskDialog destructor */
WxTaskBrowserCompleteTaskDialog::~WxTaskBrowserCompleteTaskDialog()
{
}



/** Show the modal dialog */
int WxTaskBrowserCompleteTaskDialog::ShowModal()
{
	return wxDialog::ShowModal();
}



/** Called when the OK button is pressed to validate the contents of the window */
bool WxTaskBrowserCompleteTaskDialog::Validate()
{
	// Grab controls
	wxChoice* ResolutionTypeChoiceControl = static_cast< wxChoice* >(
		FindWindow( TEXT( "ID_TaskBrowserCompleteTask_ResolutionTypeChoice" ) ) );
	check( ResolutionTypeChoiceControl != NULL );

	wxTextCtrl* CommentsControl = static_cast< wxTextCtrl* >(
		FindWindow( TEXT( "ID_TaskBrowserCompleteTask_Comments" ) ) );
	check( CommentsControl != NULL );

	wxTextCtrl* ChangelistNumberControl = static_cast< wxTextCtrl* >(
		FindWindow( TEXT( "ID_TaskBrowserCompleteTask_ChangelistNumber" ) ) );
	check( ChangelistNumberControl != NULL );

	wxTextCtrl* HoursToCompleteControl = static_cast< wxTextCtrl* >(
		FindWindow( TEXT( "ID_TaskBrowserCompleteTask_HoursToComplete" ) ) );
	check( HoursToCompleteControl != NULL );


	// Make sure the user entered something for each
	if( FString( ResolutionTypeChoiceControl->GetStringSelection().c_str() ).Len() == 0 ||
		FString( CommentsControl->GetValue().c_str() ).Len() == 0 ||
		FString( ChangelistNumberControl->GetValue().c_str() ).Len() == 0 ||
		FString( HoursToCompleteControl->GetValue().c_str() ).Len() == 0 )
	{
		// At least one field is empty, so notify the user and veto the OK button event
		appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "TaskBrowser_Error_NeedValidDataToMarkComplete" ) ) );

		return false;
	}

	return true;
}



/**
 * Static: Displays the dialog, waits for data, then dismisses the dialog
 *
 * @param	InResolutionValues	List of valid task resolution options
 * @param	OutResolutionData	[Out] User-entered resolution data
 *
 * @return	True if the user wants to continue to mark the task(s) as completed
 */
UBOOL WxTaskBrowserCompleteTaskDialog::ExecuteModalDialog( const TArray< FString >& InResolutionValues, FTaskResolutionData& OutResolutionData )
{
	// Pop up the dialog
	WxTaskBrowserCompleteTaskDialog* CompleteTaskDialog = new WxTaskBrowserCompleteTaskDialog();


	// Grab controls
	wxChoice* ResolutionTypeChoiceControl = static_cast< wxChoice* >(
		CompleteTaskDialog->FindWindow( TEXT( "ID_TaskBrowserCompleteTask_ResolutionTypeChoice" ) ) );
	check( ResolutionTypeChoiceControl != NULL );

	wxTextCtrl* CommentsControl = static_cast< wxTextCtrl* >(
		CompleteTaskDialog->FindWindow( TEXT( "ID_TaskBrowserCompleteTask_Comments" ) ) );
	check( CommentsControl != NULL );

	wxTextCtrl* ChangelistNumberControl = static_cast< wxTextCtrl* >(
		CompleteTaskDialog->FindWindow( TEXT( "ID_TaskBrowserCompleteTask_ChangelistNumber" ) ) );
	check( ChangelistNumberControl != NULL );

	wxTextCtrl* HoursToCompleteControl = static_cast< wxTextCtrl* >(
		CompleteTaskDialog->FindWindow( TEXT( "ID_TaskBrowserCompleteTask_HoursToComplete" ) ) );
	check( HoursToCompleteControl != NULL );


	// Setup resolution type options
	{
		ResolutionTypeChoiceControl->Clear();

		// NOTE: Not localized, because they have to match the underlying task database's value strings

		for( int CurResolutionValue = 0; CurResolutionValue < InResolutionValues.Num(); ++CurResolutionValue )
		{
			ResolutionTypeChoiceControl->AppendString( *InResolutionValues( CurResolutionValue ) );
		}
	}

	// Fill in default values
	ResolutionTypeChoiceControl->SetSelection( ResolutionTypeChoiceControl->FindString( TEXT( "Code/Content Change" ) ) );


	// Show the dialog and wait for user interaction
	INT DialogResult = CompleteTaskDialog->ShowModal();


	// @todo: Should we validate everything before even allowing the user to click OK?


	// Did the user click OK?
	if( DialogResult == wxID_OK )
	{
		// Store data from controls
		OutResolutionData.ResolutionType = ResolutionTypeChoiceControl->GetStringSelection().c_str();
		OutResolutionData.Comments = CommentsControl->GetValue().c_str();
		OutResolutionData.ChangelistNumber = appAtoi( ChangelistNumberControl->GetValue().c_str() );
		OutResolutionData.HoursToComplete = ( DOUBLE )appAtof( HoursToCompleteControl->GetValue().c_str() );

		return TRUE;
	}


	return FALSE;
}





BEGIN_EVENT_TABLE( WxTaskBrowser, WxBrowser )
	EVT_SIZE( WxTaskBrowser::OnSize )
END_EVENT_TABLE()



/** WxTaskBrowser constructor */
WxTaskBrowser::WxTaskBrowser()
	: TaskDataManager( NULL ),
	  TaskBrowserPanel( NULL ),
	  SplitterWindow( NULL ),
	  TaskListControl( NULL ),
	  TaskDescriptionControl( NULL ),
	  FilterNames(),
	  TaskEntries(),
	  TaskListSortColumn( 0 ),
	  bTaskListSortAscending( TRUE )
{
}



/** WxTaskBrowser destructor */
WxTaskBrowser::~WxTaskBrowser()
{
	// Cleanup task data manager
	if( TaskDataManager != NULL )
	{
		delete TaskDataManager;
		TaskDataManager = NULL;
	}
}



/**
 * Forwards the call to our base class to create the window relationship.
 * Creates any internally used windows after that
 *
 * @param	DockID			The unique id to associate with this dockable window
 * @param	FriendlyName	The friendly name to assign to this window
 * @param	Parent			The parent of this window (should be a Notebook)
 */
void WxTaskBrowser::Create( INT DockID, const TCHAR* FriendlyName, wxWindow* Parent )
{

	// Call parent implementation
	WxBrowser::Create( DockID, FriendlyName, Parent );


	// Menu bar
	{
		MenuBar = new wxMenuBar();

		// Append the docking menu choices
		WxBrowser::AddDockingMenu( MenuBar );
	}



	// Create task data manager instance
	TaskDataManager = new FTaskDataManager( this );



	// Load the window hierarchy from the .xrc file
	TaskBrowserPanel =
		wxXmlResource::Get()->LoadPanel(
			this,								// Parent window
			TEXT( "ID_TaskBrowserPanel" ) );	// Object name
	check( TaskBrowserPanel != NULL );
	
	// Localize all of the static text in the loaded window
	FLocalizeWindow( TaskBrowserPanel, FALSE, TRUE );


	// Load preferences
	FTaskBrowserSettings Settings;
	Settings.LoadSettings();


	// Propagate server connection settings to the task data manager
	FTaskDataManagerConnectionSettings ConnectionSettings;
	{
		ConnectionSettings.ServerName = Settings.ServerName;
		ConnectionSettings.ServerPort = Settings.ServerPort;
		ConnectionSettings.UserName = Settings.UserName;
		ConnectionSettings.Password = Settings.Password;
		ConnectionSettings.ProjectName = Settings.ProjectName;
	}
	TaskDataManager->SetConnectionSettings( ConnectionSettings );



	// Fill in task list filter preferences
	{
		wxCheckBox* OnlyOpenCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterOnlyOpenCheckBox" ) ) );
		check( OnlyOpenCheckBox != NULL );
		OnlyOpenCheckBox->SetValue( Settings.bFilterOnlyOpen ? true : false );

		wxCheckBox* AssignedToMeCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterAssignedToMeCheckBox" ) ) );
		check( AssignedToMeCheckBox != NULL );
		AssignedToMeCheckBox->SetValue( Settings.bFilterAssignedToMe ? true : false );

		wxCheckBox* CreatedByMeCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterCreatedByMeCheckBox" ) ) );
		check( CreatedByMeCheckBox != NULL );
		CreatedByMeCheckBox->SetValue( Settings.bFilterCreatedByMe ? true : false );

		wxCheckBox* CurrentMapCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterCurrentMapCheckBox" ) ) );
		check( CurrentMapCheckBox != NULL );
		CurrentMapCheckBox->SetValue( Settings.bFilterCurrentMap ? true : false );
	}


	{
		SplitterWindow = static_cast< wxSplitterWindow* >(
			TaskBrowserPanel->FindWindow( TEXT( "ID_TaskBrowser_SplitterWindow" ) ) );
		check( SplitterWindow != NULL );

		// Set the minimum vertical size of the window
		SplitterWindow->SetMinimumPaneSize( 50 );

		// Set the initial sash position
		// @todo: Should the sash position be a preference that we load/save?
		SplitterWindow->SetSashPosition( 250 );
	}


	{
		TaskListControl = static_cast< wxListCtrl* >(
			TaskBrowserPanel->FindWindow( TEXT( "ID_TaskBrowser_TaskListControl" ) ) );
		check( TaskListControl != NULL );

		// Add columns
		for( INT CurColumnIndex = 0; CurColumnIndex < TaskListColumn::IDs::NumColumnIDs; ++CurColumnIndex )
		{
			const FString LocalizedColumnName = LocalizeUnrealEd( TaskListColumn::Names[ CurColumnIndex ] );
			TaskListControl->InsertColumn(
				CurColumnIndex,				// ID
				*LocalizedColumnName,		// Name
				wxLIST_FORMAT_LEFT,			// Format
				-1 );						// Width
		}
	}

	{
		TaskDescriptionControl = static_cast< wxTextCtrl* >(
			TaskBrowserPanel->FindWindow( TEXT( "ID_TaskBrowser_TaskDescriptionControl" ) ) );
		check( TaskDescriptionControl != NULL );
	}





	// Setup event handlers
	{
		#undef AddWxEventHandler
		#undef AddWxListEventHandler
		#undef AddWxTextUrlEventHandler

		#define AddWxEventHandler( id, event, func )\
		{\
		wxEvtHandler* eh = GetEventHandler();\
		eh->Connect( id, event, (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)(func) );\
		}

		#define AddWxListEventHandler( id, event, func )\
		{\
		wxEvtHandler* eh = GetEventHandler();\
		eh->Connect( id, event, (wxObjectEventFunction)(wxEventFunction)(wxListEventFunction)(func) );\
		}

		#define AddWxTextUrlEventHandler( id, event, func )\
		{\
		wxEvtHandler* eh = GetEventHandler();\
		eh->Connect( id, event, (wxObjectEventFunction)(wxEventFunction)(wxTextUrlEventFunction)(func) );\
		}

		AddWxEventHandler( XRCID( "ID_TaskBrowser_FixButton" ), wxEVT_COMMAND_BUTTON_CLICKED, &WxTaskBrowser::OnFixButtonClicked );
		AddWxEventHandler( XRCID( "ID_TaskBrowser_RefreshButton" ), wxEVT_COMMAND_BUTTON_CLICKED, &WxTaskBrowser::OnRefreshButtonClicked );
		AddWxEventHandler( XRCID( "ID_TaskBrowser_DBFilterChoiceControl" ), wxEVT_COMMAND_CHOICE_SELECTED, &WxTaskBrowser::OnDBFilterChoiceSelected );

		AddWxEventHandler( XRCID( "ID_TaskBrowser_FilterOnlyOpenCheckBox" ), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxTaskBrowser::OnDisplayFilterCheckBoxClicked );
		AddWxEventHandler( XRCID( "ID_TaskBrowser_FilterAssignedToMeCheckBox" ), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxTaskBrowser::OnDisplayFilterCheckBoxClicked );
		AddWxEventHandler( XRCID( "ID_TaskBrowser_FilterCreatedByMeCheckBox" ), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxTaskBrowser::OnDisplayFilterCheckBoxClicked );
		AddWxEventHandler( XRCID( "ID_TaskBrowser_FilterCurrentMapCheckBox" ), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxTaskBrowser::OnDisplayFilterCheckBoxClicked );

		AddWxListEventHandler( XRCID( "ID_TaskBrowser_TaskListControl" ), wxEVT_COMMAND_LIST_COL_CLICK, &WxTaskBrowser::OnTaskListColumnButtonClicked );
		AddWxListEventHandler( XRCID( "ID_TaskBrowser_TaskListControl" ), wxEVT_COMMAND_LIST_ITEM_ACTIVATED, &WxTaskBrowser::OnTaskListItemActivated );
		AddWxListEventHandler( XRCID( "ID_TaskBrowser_TaskListControl" ), wxEVT_COMMAND_LIST_ITEM_SELECTED, &WxTaskBrowser::OnTaskListItemSelected );

		AddWxTextUrlEventHandler( XRCID( "ID_TaskBrowser_TaskDescriptionControl" ), wxEVT_COMMAND_TEXT_URL, &WxTaskBrowser::OnURLLaunched )

		AddWxEventHandler( XRCID( "ID_TaskBrowser_ConnectButton" ), wxEVT_COMMAND_BUTTON_CLICKED, &WxTaskBrowser::OnConnectButtonClicked );
		AddWxEventHandler( XRCID( "ID_TaskBrowser_SettingsButton" ), wxEVT_COMMAND_BUTTON_CLICKED, &WxTaskBrowser::OnSettingsButtonClicked );
	}
	


	// We start off with an empty filter list and task entry array
	FilterNames.Empty();
	TaskEntries.Empty();



	// Set column sorting rules
	TaskListSortColumn = TaskListColumn::IDs::Priority;
	bTaskListSortAscending = Settings.bTaskListSortAscending;
	for( INT CurColumnIndex = 0; CurColumnIndex < TaskListColumn::IDs::NumColumnIDs; ++CurColumnIndex )
	{
		if( FString( TaskListColumn::Names[ CurColumnIndex ] ) == Settings.TaskListSortColumn )
		{
			TaskListSortColumn = CurColumnIndex;
			break;
		}
	}


	// Automatically connect to the server if we were asked to do that
	if( Settings.bAutoConnectAtStartup && Settings.AreConnectionSettingsValid() )
	{
		TaskDataManager->AttemptConnection();
	}
}



/** Static: Wx callback to sort task list items */
int WxTaskBrowser::TaskListItemSortCallback( UPTRINT InItem1, UPTRINT InItem2, UPTRINT InSortData )
{
	const WxTaskBrowser* TaskBrowser = reinterpret_cast< WxTaskBrowser* >( InSortData );
	check( TaskBrowser != NULL );


	const FTaskDatabaseEntry& TaskEntryA = TaskBrowser->TaskEntries( InItem1 );
	const FTaskDatabaseEntry& TaskEntryB = TaskBrowser->TaskEntries( InItem2 );

	int SortResult = 0;

	switch( TaskBrowser->TaskListSortColumn )
	{
		case TaskListColumn::IDs::Number:
			{
				if( TaskEntryA.Number != TaskEntryB.Number )
				{
					SortResult = TaskEntryA.Number > TaskEntryB.Number ? 1 : -1;
				}
			}
			break;

		case TaskListColumn::IDs::Priority:
			{
				SortResult = appStricmp( *TaskEntryA.Priority, *TaskEntryB.Priority );
			}
			break;

		case TaskListColumn::IDs::Name:
			{
				SortResult = appStricmp( *TaskEntryA.Name, *TaskEntryB.Name );
			}
			break;

		case TaskListColumn::IDs::AssignedTo:
			{
				SortResult = appStricmp( *TaskEntryA.AssignedTo, *TaskEntryB.AssignedTo );
			}
			break;

		case TaskListColumn::IDs::Status:
			{
				SortResult = appStricmp( *TaskEntryA.Status, *TaskEntryB.Status );
			}
			break;

		case TaskListColumn::IDs::CreatedBy:
			{
				SortResult = appStricmp( *TaskEntryA.CreatedBy, *TaskEntryB.CreatedBy );
			}
			break;

		default:
			// Unrecognized column type?
			check( 0 );
			break;
	}


	// If the items had the same value, then fallback to secondary sort criteria
	// @todo: Support custom secondary sorts (stack of sorted columns and ascend/descend state)
	if( SortResult == 0 )
	{
		if( TaskBrowser->TaskListSortColumn == TaskListColumn::IDs::Number )
		{
			// Secondary sort by priority
			SortResult = TaskEntryA.Number > TaskEntryB.Number ? 1 : -1;
		}
		else
		{
			// Secondary sort by name
			SortResult = appStricmp( *TaskEntryA.Name, *TaskEntryB.Name );
		}
	}

	// Reverse the sort order if we were asked to do that
	if( !TaskBrowser->bTaskListSortAscending )
	{
		SortResult = -SortResult;
	}


	return SortResult;
}




/**
 * Refresh all or part of the user interface
 *
 * @param	Options		Bitfield that describes which parts of the GUI to refresh at minimum
 */
void WxTaskBrowser::RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options )
{
	const UBOOL bShouldFreezeUI =
		( Options & ( ETaskBrowserGUIRefreshOptions::RebuildFilterList | ETaskBrowserGUIRefreshOptions::RebuildTaskList ) );
	if( bShouldFreezeUI )
	{
		// Freeze the window before we go and add dozens of items to a list, as it will reduce flicker
		BeginUpdate();
	}


	// Enable/disable window controls
	{
		SplitterWindow = static_cast< wxSplitterWindow* >(
			TaskBrowserPanel->FindWindow( TEXT( "ID_TaskBrowser_SplitterWindow" ) ) );
		check( SplitterWindow != NULL );

		// If we're not connected, then most of interface will be disabled
		if( TaskDataManager->GetConnectionStatus() != ETaskDataManagerStatus::Connected &&
			TaskDataManager->GetConnectionStatus() != ETaskDataManagerStatus::Disconnecting )
		{
			SplitterWindow->Enable( FALSE );
		}
		else
		{
 			SplitterWindow->Enable( TRUE );
		}
	}


	if( Options & ETaskBrowserGUIRefreshOptions::RebuildFilterList )
	{
		// Copy all of the filters over to our own cached array
		FilterNames = TaskDataManager->GetCachedFilterNames();


		wxChoice* DBFilterChoiceControl = static_cast< wxChoice* >( FindWindow( XRCID( "ID_TaskBrowser_DBFilterChoiceControl" ) ) );
		check( DBFilterChoiceControl != NULL );

		// Clear the contents of the list
		DBFilterChoiceControl->Clear();

		for( TArray< FString >::TConstIterator FilterIter( FilterNames ); FilterIter != NULL; ++FilterIter )
		{
			const FString& CurFilterName = *FilterIter;

			// Add this filter to our list
			DBFilterChoiceControl->AppendString( *CurFilterName );
		}



		// Do we have an active filter already?
		INT SelectedFilterIndex = INDEX_NONE;
		if( TaskDataManager->GetActiveFilterName().Len() > 0 )
		{
			verify( FilterNames.FindItem( TaskDataManager->GetActiveFilterName(), SelectedFilterIndex ) );
		}
		else
		{
			// No active filter yet, so we'll set one up now
			FTaskBrowserSettings Settings;
			Settings.LoadSettings();

			// Search for the default filter name
			if( !FilterNames.FindItem( Settings.DBFilterName, SelectedFilterIndex ) )
			{
				// Not found... hrm..
				if( FilterNames.Num() > 0 )
				{
					// Just use the first available filter
					SelectedFilterIndex = 0;
				}
			}

			if( SelectedFilterIndex != INDEX_NONE )
			{
				// Select the filter in the list (NOTE: This won't trigger a UI event)
				DBFilterChoiceControl->Select( SelectedFilterIndex );

				// Update task data manager
				TaskDataManager->ChangeActiveFilter( FilterNames( SelectedFilterIndex ) );
			}
		}
	}



	if( Options & ETaskBrowserGUIRefreshOptions::RebuildTaskList )
	{
		// Grab a list of task numbers we already had selected
		TSet< INT > SelectedTaskNumberSet;
		{
			const UBOOL bOnlyOpenTasks = FALSE;
			TArray< UINT > SelectedTaskNumbers;
			QuerySelectedTaskNumbers( SelectedTaskNumbers, bOnlyOpenTasks );

			// Add everything to a set so that we can do quick tests later on
			for( INT CurTaskIndex = 0; CurTaskIndex < SelectedTaskNumbers.Num(); ++CurTaskIndex )
			{
				SelectedTaskNumberSet.Add( SelectedTaskNumbers( CurTaskIndex ) );
			}
		}


		// Copy all of the task entries to our own cached array
		TaskEntries = TaskDataManager->GetCachedTaskArray();

		// Clear everything
		TaskListControl->DeleteAllItems();


		// Check to see which 'display filters' are enabled
		wxCheckBox* OnlyOpenCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterOnlyOpenCheckBox" ) ) );
		check( OnlyOpenCheckBox != NULL );
		const UBOOL bOnlyOpen = OnlyOpenCheckBox->IsChecked();

		wxCheckBox* AssignedToMeCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterAssignedToMeCheckBox" ) ) );
		check( AssignedToMeCheckBox != NULL );
		const UBOOL bAssignedToMe = AssignedToMeCheckBox->IsChecked();

		wxCheckBox* CreatedByMeCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterCreatedByMeCheckBox" ) ) );
		check( CreatedByMeCheckBox != NULL );
		const UBOOL bCreatedByMe = CreatedByMeCheckBox->IsChecked();

		wxCheckBox* CurrentMapCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterCurrentMapCheckBox" ) ) );
		check( CurrentMapCheckBox != NULL );
		const UBOOL bCurrentMap = CurrentMapCheckBox->IsChecked();


		// Grab the current loaded map name from the editor
		// @todo: Register for event to auto refresh task list when map loaded or new?
		FString CurrentMapName = GWorld->GetMapName();
		{
			const INT MaxPrefixChars = 2;
			const INT MaxSuffixChars = 3;

			if( CurrentMapName.Len() > MaxPrefixChars + 1 )
			{
				// Clean up any small prefixes or suffixes on the map name
				INT FirstUnderscore = CurrentMapName.InStr( TEXT( "_" ) );
				if( FirstUnderscore != INDEX_NONE )
				{
					const INT NumPrefixChars = FirstUnderscore;
					if( NumPrefixChars <= MaxPrefixChars )
					{
						// Chop it!
						CurrentMapName = CurrentMapName.Mid( FirstUnderscore + 1 );
					}
				}
			}


			// Chop off multiple small suffixes
			bool bCheckForSuffix = TRUE;
			while( bCheckForSuffix )
			{
				bCheckForSuffix = FALSE;
				if( CurrentMapName.Len() > MaxSuffixChars + 1 )
				{
					// Clean up any small prefixes or suffixes on the map name
					INT LastUnderscore = CurrentMapName.InStr( TEXT( "_" ), TRUE );
					if( LastUnderscore != INDEX_NONE )
					{
						const INT NumSuffixChars = CurrentMapName.Len() - LastUnderscore;
						if( NumSuffixChars <= MaxSuffixChars )
						{
							// Chop it!
							CurrentMapName = CurrentMapName.Left( LastUnderscore );

							// Check for another suffix!
							bCheckForSuffix = TRUE;
						}
					}
				}
			}
		}


		// Grab the current user's real name
		FString UserRealName = TaskDataManager->GetUserRealName();


		for( TArray< FTaskDatabaseEntry >::TConstIterator TaskIter( TaskEntries ); TaskIter != NULL; ++TaskIter )
		{
			const FTaskDatabaseEntry& CurTask = *TaskIter;

			if( ( !bOnlyOpen || CurTask.Status.StartsWith( TaskDataManager->GetOpenTaskStatusPrefix() ) ) &&
				( !bAssignedToMe || CurTask.AssignedTo.InStr( UserRealName, FALSE, TRUE ) != INDEX_NONE ) &&
				( !bCreatedByMe || CurTask.CreatedBy.InStr( UserRealName, FALSE, TRUE ) != INDEX_NONE ) &&
				( !bCurrentMap || CurTask.Name.InStr( CurrentMapName, FALSE, TRUE ) != INDEX_NONE ) )
			{
				// Add an empty string item
				const INT ItemIndex = TaskListControl->GetItemCount();
				TaskListControl->InsertItem( ItemIndex, TEXT( "" ) );

				// Number
				TaskListControl->SetItem( ItemIndex, TaskListColumn::IDs::Number, *appItoa( CurTask.Number ) );

				// Priority
				TaskListControl->SetItem( ItemIndex, TaskListColumn::IDs::Priority, *CurTask.Priority );

				// Name
				TaskListControl->SetItem( ItemIndex, TaskListColumn::IDs::Name, *CurTask.Name );

				// Assigned To
				TaskListControl->SetItem( ItemIndex, TaskListColumn::IDs::AssignedTo, *CurTask.AssignedTo );

				// Status
				TaskListControl->SetItem( ItemIndex, TaskListColumn::IDs::Status, *CurTask.Status );

				// Created By
				TaskListControl->SetItem( ItemIndex, TaskListColumn::IDs::CreatedBy, *CurTask.CreatedBy );


				// Store the original index of this task
				TaskListControl->SetItemData( ItemIndex, TaskIter.GetIndex() );

				
				// Try to keep the currently selected task selected
				if( SelectedTaskNumberSet.Contains( CurTask.Number ) )
				{
					TaskListControl->SetItemState( ItemIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
				}
			}
		}
	}


	if( Options & ( ETaskBrowserGUIRefreshOptions::ResetColumnSizes | ETaskBrowserGUIRefreshOptions::RebuildTaskList ) )
	{
		// Update task list column sizes
		for( INT CurColumnIndex = 0; CurColumnIndex < TaskListColumn::IDs::NumColumnIDs; ++CurColumnIndex )
		{
			INT Width = 0;
			TaskListControl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE );
			Width = Max( TaskListControl->GetColumnWidth( CurColumnIndex ), Width );
			TaskListControl->SetColumnWidth( CurColumnIndex, wxLIST_AUTOSIZE_USEHEADER );
			Width = Max( TaskListControl->GetColumnWidth( CurColumnIndex ), Width );
			TaskListControl->SetColumnWidth( CurColumnIndex, Width );
		}
	}


	if( Options & ( ETaskBrowserGUIRefreshOptions::SortTaskList | ETaskBrowserGUIRefreshOptions::RebuildTaskList ) )
	{
		// Sort the task list
		TaskListControl->SortItems( &TaskListItemSortCallback, reinterpret_cast< UPTRINT >( this ) );
	}


	if( Options & ETaskBrowserGUIRefreshOptions::UpdateTaskDescription )
	{
		FString TaskName;
		FString TaskDescription;

		const UINT TaskNumber = TaskDataManager->GetFocusedTaskNumber();

		UBOOL bHaveTask = FALSE;
		if( TaskNumber != INDEX_NONE )
		{
			// Do we have any cached results to display?
			const FTaskDatabaseEntryDetails* TaskDetails = TaskDataManager->FindCachedTaskDetails( TaskNumber );
			if( TaskDetails != NULL )
			{
				// We have the task details!
				TaskName = TaskDetails->Name;
				TaskDescription = TaskDetails->Description;

				// Append the task number to the name
				TaskName += FString::Printf( TEXT( " (# %i)" ), TaskNumber );

				bHaveTask = TRUE;
			}
		}

		if( !bHaveTask )
		{
			// OK we don't have the task description yet, but it may be on it's way!  Let's check.
			if( TaskDataManager->GetGUIStatus() == ETaskDataManagerStatus::QueryingTaskDetails )
			{
				// Try to find the name of the task in our cached task entry array
				for( INT CurTaskIndex = 0; CurTaskIndex < TaskEntries.Num(); ++CurTaskIndex )
				{
					const FTaskDatabaseEntry& CurTask = TaskEntries( CurTaskIndex );
					if( CurTask.Number == TaskDataManager->GetFocusedTaskNumber() )
					{
						// Found it!
						TaskName = CurTask.Name;

						// Append the task number to the name
						TaskName += FString::Printf( TEXT( " (# %i)" ), TaskNumber );

						break;
					}
				}
				
				// Let the user know we're downloading the task description now
				TaskDescription = LocalizeUnrealEd( TEXT( "TaskBrowser_WaitingForDescription" ) );
			}
		}

		// Update the task name text
		wxTextCtrl* TaskNameControl = static_cast< wxTextCtrl* >( FindWindow( XRCID( "ID_TaskBrowser_TaskNameText" ) ) );
		check( TaskNameControl != NULL );
		TaskNameControl->SetValue( *TaskName );

		// Update the task description
		TaskDescriptionControl->SetValue( *TaskDescription );
	}



	if( Options & ETaskBrowserGUIRefreshOptions::UpdateStatusMessage )
	{
		// Always update the server status text
		wxStaticText* StaticText = static_cast< wxStaticText* >( FindWindow( XRCID( "ID_TaskBrowser_StatusMessageText" ) ) );
		check( StaticText != NULL );

		switch( TaskDataManager->GetGUIStatus() )
		{
			case ETaskDataManagerStatus::FailedToInit:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_FailedToInit" ) ) );
				}
				break;

			case ETaskDataManagerStatus::Connecting:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_Connecting" ) ) );
				}
				break;

			case ETaskDataManagerStatus::ReadyToConnect:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_ReadyToConnect" ) ) );
				}
				break;

			case ETaskDataManagerStatus::Connected:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_Connected" ) ) );
				}
				break;

			case ETaskDataManagerStatus::ConnectionFailed:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_ConnectionFailed" ) ) );
				}
				break;

			case ETaskDataManagerStatus::Disconnecting:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_Disconnecting" ) ) );
				}
				break;

			case ETaskDataManagerStatus::QueryingFilters:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_QueryingFilters" ) ) );
				}
				break;

			case ETaskDataManagerStatus::QueryingTasks:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_QueryingTasks" ) ) );
				}
				break;

			case ETaskDataManagerStatus::QueryingTaskDetails:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_QueryingTaskDetails" ) ) );
				}
				break;

			case ETaskDataManagerStatus::MarkingTaskComplete:
				{
					StaticText->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ServerStatus_MarkingTaskComplete" ) ) );
				}
				break;
		}
	}


	// Update the 'Fix' button
	{
		wxButton* FixButton = static_cast< wxButton* >( FindWindow( XRCID( "ID_TaskBrowser_FixButton" ) ) );
		check( FixButton != NULL );

		UBOOL bEnableFixButton = FALSE;

		// Must be connected to the server
		if( TaskDatabaseSystem::IsConnected() )
		{
			// Only enable the 'Mark as Fixed' button if at least one *OPEN* task is selected
			if( TaskListControl != NULL )
			{
				const UBOOL bOnlyOpenTasks = TRUE;
				TArray< UINT > SelectedOpenTaskNumbers;
				QuerySelectedTaskNumbers( SelectedOpenTaskNumbers, bOnlyOpenTasks );
				if( SelectedOpenTaskNumbers.Num() > 0 )
				{
					bEnableFixButton = TRUE;
				}
			}
		}

		FixButton->Enable( bEnableFixButton ? true : false );
	}


	// Update the 'Connect' button
	{
		wxButton* ConnectButton = static_cast< wxButton* >( FindWindow( XRCID( "ID_TaskBrowser_ConnectButton" ) ) );
		check( ConnectButton != NULL );

		if( TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::Disconnecting ||
			TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::FailedToInit )
		{
			// Disconnecting or no database available to connect to, so grey out the button
			ConnectButton->Enable( FALSE );
			ConnectButton->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ConnectButtonLabel" ) ) );
		}
		else if( TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::Connecting )
		{
			// Already connecting, so leave the button disabled until we finish our attempt
			ConnectButton->Enable( FALSE );
			ConnectButton->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_DisconnectButtonLabel" ) ) );
		}
		else if( TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::ReadyToConnect ||
				 TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::ConnectionFailed )
		{
			// Connection failed, so allow the user to try again
			ConnectButton->Enable( TRUE );
			ConnectButton->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_ConnectButtonLabel" ) ) );
		}
		else
		{
			// We're already connected, so change the button to a 'disconnect' button
			ConnectButton->Enable( TRUE );
			ConnectButton->SetLabel( *LocalizeUnrealEd( TEXT( "TaskBrowser_DisconnectButtonLabel" ) ) );
		}
	}


	if( bShouldFreezeUI )
	{
		// Thaw the window
		EndUpdate();
	}
}



/** Tells the browser to update itself */
void WxTaskBrowser::Update()
{
	// Visual refresh.  Nothing to do here, since we rely on other events to trigger updates.
}



/** Sets the size of the list control based upon our new size */
void WxTaskBrowser::OnSize( wxSizeEvent& In )
{
	// During the creation process a sizing message can be sent so don't
	// handle it until we are initialized
	if( bAreWindowsInitialized )
	{
		// Freeze the window
		BeginUpdate();

		// Resize controls
		TaskBrowserPanel->SetSize( GetClientRect() );

		// Thaw the window
		EndUpdate();
	}
}



/** Generates a list of currently selected task numbers */
void WxTaskBrowser::QuerySelectedTaskNumbers( TArray< UINT >& OutSelectedTaskNumbers, UBOOL bOnlyOpenTasks )
{
	OutSelectedTaskNumbers.Reset();

	// Create a list of task numbers that are currently selected in the task list
	INT LastSelectedItemIndex = -1;
	for( ; ; )
	{
		LastSelectedItemIndex =
			TaskListControl->GetNextItem(
				LastSelectedItemIndex,		// Item number to start at (-1 means beginning)
				wxLIST_NEXT_ALL,			// Search rules
				wxLIST_STATE_SELECTED );	// Item type to search for

		if( LastSelectedItemIndex == -1 )
		{
			// No more entries in list
			break;
		}

		// Try to find this task entry in our cached array
		const UINT LocalTaskIndex = TaskListControl->GetItemData( LastSelectedItemIndex );
		if( TaskEntries.IsValidIndex( LocalTaskIndex ) )
		{
			const FTaskDatabaseEntry& SelectedTask = TaskEntries( LocalTaskIndex );

			// Make sure the task isn't already completed
			if( !bOnlyOpenTasks || SelectedTask.Status.StartsWith( TaskDataManager->GetOpenTaskStatusPrefix() ) )
			{
				// Add this task's number to our list of tasks
				OutSelectedTaskNumbers.AddItem( SelectedTask.Number );
			}
		}
	}
}



/** Called when the 'Mark as Fixed' button is clicked */
void WxTaskBrowser::OnFixButtonClicked( wxCommandEvent& In )
{
	const UBOOL bOnlyOpenTasks = TRUE;
	TArray< UINT > TaskNumbersToFix;
	QuerySelectedTaskNumbers( TaskNumbersToFix, bOnlyOpenTasks );
	
	if( TaskNumbersToFix.Num() > 0 )
	{
		FTaskResolutionData ResolutionData;
		if( WxTaskBrowserCompleteTaskDialog::ExecuteModalDialog( TaskDataManager->GetResolutionValues(), ResolutionData ) )
		{
			// Queue these up to be marked as fixed!
			TaskDataManager->StartMarkingTasksComplete( TaskNumbersToFix, ResolutionData );
		}
	}
}



/** Called when the refresh button is clicked */
void WxTaskBrowser::OnRefreshButtonClicked( wxCommandEvent& In )
{
	// Update everything using fresh data from the server
	TaskDataManager->ClearTaskDataAndInitiateRefresh();
}



/** Called when a new filter is selected in the choice box */
void WxTaskBrowser::OnDBFilterChoiceSelected( wxCommandEvent& In )
{
	// Change the active database filter
	wxChoice* DBFilterChoiceControl = static_cast< wxChoice* >( FindWindow( XRCID( "ID_TaskBrowser_DBFilterChoiceControl" ) ) );
	check( DBFilterChoiceControl != NULL );

	// Update the active filter!
	TaskDataManager->ChangeActiveFilter( DBFilterChoiceControl->GetStringSelection().c_str() );

	// Save the new filter name in our preferences
	{
		FTaskBrowserSettings Settings;
		Settings.LoadSettings();
		Settings.DBFilterName = DBFilterChoiceControl->GetStringSelection().c_str();
		Settings.SaveSettings();
	}
}



/** Called when a display filter check box is clicked */
void WxTaskBrowser::OnDisplayFilterCheckBoxClicked( wxCommandEvent& In )
{
	// Refresh the GUI.  It will apply the updated filters.
	RefreshGUI( ETaskBrowserGUIRefreshOptions::RebuildTaskList );

	// A filter was changed, so save updated settings to disk
	{
		FTaskBrowserSettings Settings;
		Settings.LoadSettings();

		wxCheckBox* OnlyOpenCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterOnlyOpenCheckBox" ) ) );
		check( OnlyOpenCheckBox != NULL );
		Settings.bFilterOnlyOpen = OnlyOpenCheckBox->GetValue();

		wxCheckBox* AssignedToMeCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterAssignedToMeCheckBox" ) ) );
		check( AssignedToMeCheckBox != NULL );
		Settings.bFilterAssignedToMe = AssignedToMeCheckBox->GetValue();

		wxCheckBox* CreatedByMeCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterCreatedByMeCheckBox" ) ) );
		check( CreatedByMeCheckBox != NULL );
		Settings.bFilterCreatedByMe = CreatedByMeCheckBox->GetValue();

		wxCheckBox* CurrentMapCheckBox = static_cast< wxCheckBox* >( FindWindow( XRCID( "ID_TaskBrowser_FilterCurrentMapCheckBox" ) ) );
		check( CurrentMapCheckBox != NULL );
		Settings.bFilterCurrentMap = CurrentMapCheckBox->GetValue();

		Settings.SaveSettings();
	}

}



/** Called when a column button is clicked on the task list */
void WxTaskBrowser::OnTaskListColumnButtonClicked( wxListEvent& In )
{
	const int Column = In.GetColumn();
	if( Column > -1 )
	{
		if( Column == TaskListSortColumn )
		{
			// Clicking on the same column will flip the sort order
			bTaskListSortAscending = !bTaskListSortAscending;
		}
		else
		{
			// Clicking on a new column will set that column as current and reset the sort order.
			TaskListSortColumn = In.GetColumn();
			bTaskListSortAscending = TRUE;
		}


		// Save the sorting configuration
		FTaskBrowserSettings Settings;
		Settings.LoadSettings();
		Settings.TaskListSortColumn = TaskListColumn::Names[ TaskListSortColumn ];
		Settings.bTaskListSortAscending = bTaskListSortAscending;
		Settings.SaveSettings();
	}

	// Refresh the GUI
	RefreshGUI( ETaskBrowserGUIRefreshOptions::SortTaskList );

}



/** Called when an item in the task list is selected */
void WxTaskBrowser::OnTaskListItemSelected( wxListEvent& In )
{
	// Set the focused task number
	const UINT LocalTaskIndex = TaskListControl->GetItemData( In.m_itemIndex );
	if( TaskEntries.IsValidIndex( LocalTaskIndex ) )
	{
		const FTaskDatabaseEntry& FocusedTask = TaskEntries( LocalTaskIndex );

		// Make sure the task data manager is keep track of this task
		TaskDataManager->SetFocusedTaskNumber( FocusedTask.Number );
	}
	else
	{
		// Nothing currently focused
		TaskDataManager->SetFocusedTaskNumber( INDEX_NONE );
	}


	RefreshGUI( ETaskBrowserGUIRefreshOptions::UpdateTaskDescription );
}



/** Called when an item in the task list is double-clicked */
void WxTaskBrowser::OnTaskListItemActivated( wxListEvent& In )
{
	// Set the focused task number
	const UINT LocalTaskIndex = TaskListControl->GetItemData( In.m_itemIndex );
	if( TaskEntries.IsValidIndex( LocalTaskIndex ) )
	{
		const FTaskDatabaseEntry& SelectedTask = TaskEntries( LocalTaskIndex );

		// Check to see if the bug status is "Open"
		if( SelectedTask.Status.StartsWith( TaskDataManager->GetOpenTaskStatusPrefix() ) )
		{
			// Add this task's number to our list of tasks to fix
			TArray< UINT > TaskNumbersToFix;
			TaskNumbersToFix.AddItem( SelectedTask.Number );

			FTaskResolutionData ResolutionData;
			if( WxTaskBrowserCompleteTaskDialog::ExecuteModalDialog( TaskDataManager->GetResolutionValues(), ResolutionData ) )
			{
				// Queue these up to be marked as fixed!
				TaskDataManager->StartMarkingTasksComplete( TaskNumbersToFix, ResolutionData );
			}
		}
	}
}



/** Called when the 'Connect' button is clicked */
void WxTaskBrowser::OnConnectButtonClicked( wxCommandEvent& In )
{
	if( TaskDataManager->GetConnectionStatus() != ETaskDataManagerStatus::FailedToInit )
	{
		if( TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::ReadyToConnect ||
			TaskDataManager->GetConnectionStatus() == ETaskDataManagerStatus::ConnectionFailed )
		{
			// Did the user cancel the config dialog?
			UBOOL bUserCancelled = FALSE;

			// Load server settings from disk
			FTaskBrowserSettings Settings;
			Settings.LoadSettings();

			// Make sure that everything is valid, otherwise pop up a settings dialog
			if( !Settings.AreConnectionSettingsValid() )
			{
				// Give the user a chance to fix the settings
				if( WxTaskBrowserConfigDialog::ExecuteModalDialog() == wxID_CANCEL )
				{
					// the user cancelled the dialog
					bUserCancelled = TRUE;
				}
				else
				{
					// Update the task data manager with any settings that may have changed
					Settings.LoadSettings();
					FTaskDataManagerConnectionSettings NewConnectionSettings;
					{
						NewConnectionSettings.ServerName = Settings.ServerName;
						NewConnectionSettings.ServerPort = Settings.ServerPort;
						NewConnectionSettings.UserName = Settings.UserName;
						NewConnectionSettings.Password = Settings.Password;
						NewConnectionSettings.ProjectName = Settings.ProjectName;
					}
					TaskDataManager->SetConnectionSettings( NewConnectionSettings );
				}

			}

			if( Settings.AreConnectionSettingsValid() )
			{
				// Connection failed, so allow the user to try again
				TaskDataManager->AttemptConnection();
			}
			else
			{
				// Only warn the user if the user did not press cancel
				if( !bUserCancelled )
				{
					// Warn the user that we'll need valid settings in order to connect to the server
					appMsgf( AMT_OK, *LocalizeUnrealEd( TEXT( "TaskBrowser_Error_NeedValidConnectionSettings" ) ) );
				}
			}
		}
		else
		{
			// Queue a disconnect
			TaskDataManager->AttemptDisconnection();
		}
	}
}



/** Called when the 'Settings' button is clicked */
void WxTaskBrowser::OnSettingsButtonClicked( wxCommandEvent& In )
{
	// Execute the modal dialog
	WxTaskBrowserConfigDialog::ExecuteModalDialog();

	// Update the task data manager with any settings that may have changed
	FTaskBrowserSettings Settings;
	Settings.LoadSettings();
	FTaskDataManagerConnectionSettings NewConnectionSettings;
	{
		NewConnectionSettings.ServerName = Settings.ServerName;
		NewConnectionSettings.ServerPort = Settings.ServerPort;
		NewConnectionSettings.UserName = Settings.UserName;
		NewConnectionSettings.Password = Settings.Password;
		NewConnectionSettings.ProjectName = Settings.ProjectName;
	}
	TaskDataManager->SetConnectionSettings( NewConnectionSettings );
}



/** Called when a text URL is clicked on in a text control */
void WxTaskBrowser::OnURLLaunched( wxTextUrlEvent& In )
{
	// We only care about left mouse clicks
	if( In.GetMouseEvent().ButtonDown( wxMOUSE_BTN_LEFT ) )
	{
		wxTextCtrl* TextControl = static_cast< wxTextCtrl* >(
			FindWindow( TEXT( "ID_TaskBrowser_TaskDescriptionControl" ) ) );
		check( TextControl != NULL );

		const wxURI URI( TextControl->GetRange( In.GetURLStart(), In.GetURLEnd() ) );
		if( URI.GetScheme() == wxT( "http" ) ||
			URI.GetScheme() == wxT( "https" ) ||
			URI.GetScheme() == wxT( "ftp" ) ||
			URI.GetScheme() == wxT( "mailto" ) )
		{
			// WxWidget is kind of weird/spammy with URL events, so we'll make sure not to launch a browser
			// more than once per second
			static DOUBLE s_LastURLLaunchTime = -9999.0;
			const DOUBLE CurSeconds = appSeconds();
			if( CurSeconds - s_LastURLLaunchTime >= 1.0 )
			{
				wxLaunchDefaultBrowser( URI.BuildURI() );
				s_LastURLLaunchTime = CurSeconds;
			}
 		}
	}
}



/** FTaskDataGUIInterface Callback: Called when the GUI should be refreshed */
void WxTaskBrowser::Callback_RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options )
{
	// Refresh the GUI
	RefreshGUI( Options );
}


