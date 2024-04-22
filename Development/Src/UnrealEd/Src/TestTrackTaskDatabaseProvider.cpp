/*=============================================================================
	TestTrackTaskDatabaseProvider.cpp: Integrates TestTrack Pro task database
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "TestTrackTaskDatabaseProvider.h"


#if WITH_TESTTRACK


// TestTrack includes
#if _WIN64
	#pragma pack(push, 8)
#endif
#include "../../../External/TestTrack/Source/TestTrackSoapCGIProxy.h"
#if _WIN64
	#pragma pack(pop)
#endif

// TestTrack library
#if _WIN64
	#pragma comment( lib, "TestTrack_64.lib" )
#else
	#pragma comment( lib, "TestTrack.lib" )
#endif

/**
 * Static: Creates a new instance of a TestTrack task database object
 *
 * @return	The new database if successful, otherwise NULL
 */
FTestTrackProvider* FTestTrackProvider::CreateTestTrackProvider()
{
	FTestTrackProvider* NewDatabase = new FTestTrackProvider();
	check( NewDatabase != NULL );

	// Initialize the database
	if( !NewDatabase->Init() ) 
	{
		delete NewDatabase;
		return NULL;
	}

	return NewDatabase;
}



/**
 * FTestTrackProvider constructor
 */
FTestTrackProvider::FTestTrackProvider()
	: TestTrackSoap( NULL ),
	  LastErrorMessage( TEXT( "No error" ) ),
	  bLastErrorWasCausedByDisconnection( FALSE ),
	  ANSIServerEndpointURLString( NULL ),
	  IDCookie( 0 ),
	  UserRealName()
{
}



/**
 * FTestTrackProvider destructor
 */
FTestTrackProvider::~FTestTrackProvider()
{
	TDLOG( TEXT( "TestTrack: Destroying TestTrack provider" ) );

	// Destroy SOAP interface to TestTrack Pro
	if( TestTrackSoap != NULL )
	{
		// NOTE: The TestTrack soap destructor will call soap_destroy(), soap_end() and soap_free()
		//       will clean up allocated memory
		delete TestTrackSoap;
		TestTrackSoap = NULL;
	}

	// Clean up endpoint URL string
	if( ANSIServerEndpointURLString != NULL )
	{
		delete[] ANSIServerEndpointURLString;
		ANSIServerEndpointURLString = NULL;
	}

	TDLOG( TEXT( "TestTrack: TestTrack provider finished destroying" ) );
}



/**
 * Initializes this task database
 *
 * @return	TRUE if successful
 */
UBOOL FTestTrackProvider::Init()
{
	TDLOG( TEXT( "TestTrack: Initializing TestTrack provider" ) );


	// Allocate SOAP interface to TestTrack Pro
	TestTrackSoap = new ttsoapcgi();
	check( TestTrackSoap != NULL );

	
	// @todo: Are there any other SOAP options we want to be configuring here?


	// Disable multi-referenced strings (href="#_12").  This makes the XML structures larger but
	// TestTrack doesn't support resolving graph references.  This effectively changes the graph
	// to a tree with duplicated strings where needed.
	soap_clr_omode( TestTrackSoap->soap, SOAP_XML_GRAPH );
	soap_set_omode( TestTrackSoap->soap, SOAP_XML_TREE );


	// Set timeouts for SOAP network I/O
	TestTrackSoap->soap->recv_timeout = 10000;		/* when > 0, gives socket recv timeout in seconds, < 0 in usec */
	TestTrackSoap->soap->send_timeout = 4000;		/* when > 0, gives socket send timeout in seconds, < 0 in usec */
	TestTrackSoap->soap->connect_timeout = 4000;	/* when > 0, gives socket connect() timeout in seconds, < 0 in usec */
	TestTrackSoap->soap->accept_timeout = 4000;		/* when > 0, gives socket accept() timeout in seconds, < 0 in usec */


	TDLOG( TEXT( "TestTrack: TestTrack provider finished initializing" ) );

	return TRUE;
}



/**
 * Sets the TestTrack server endpoint address using the specified URL string
 *
 * @param	InServerURL		The server URL address
 */
void FTestTrackProvider::SetupServerEndpoint( const FString& InServerURL )
{
	TDLOG( TEXT( "TestTrack: %s" ), *FString( __FUNCTION__ ) );

	// Cleanup old endpoint string
	if( ANSIServerEndpointURLString != NULL )
	{
		delete[] ANSIServerEndpointURLString;
		ANSIServerEndpointURLString = NULL;
	}

	// Create a URL string for the server's CGI binary
	FString ServerEndPointURL = InServerURL + TEXT( "/cgi-bin/ttsoapcgi.exe" );
	ANSIServerEndpointURLString = new ANSICHAR[ ServerEndPointURL.Len() + 1 ];
	appStrcpyANSI( ANSIServerEndpointURLString, ServerEndPointURL.Len() + 1, TCHAR_TO_ANSI( *ServerEndPointURL ) );

	// Point the SOAP library toward the server endpoint
	TestTrackSoap->endpoint = ANSIServerEndpointURLString;
}



/**
 * Checks the result code and returns whether the operation succeeded.  For failure cases, keeps track
 * of the actual error message so it can be retrieved later.
 *
 * @param	ResultCode	The result code to test
 *
 * @return	True if the result code reported success
 */
UBOOL FTestTrackProvider::VerifyTTPSucceeded( const INT ResultCode )
{
	if( ResultCode != SOAP_OK )
	{
		TDLOG( TEXT( "TestTrack: Error (ResultCode: %i)" ), ResultCode );

		if( TestTrackSoap->soap->fault != NULL )
		{
			TDLOG( TEXT( "TestTrack: Error details (faultcode: '%s', faultstring: '%s', detail: '%s')" ),
				*FString( TestTrackSoap->soap->fault->faultcode ),
				*FString( TestTrackSoap->soap->fault->faultstring ),
				*FString( TestTrackSoap->soap->fault->detail->__any ) );

			// Store the error message string so it can be reported back to the user later
			LastErrorMessage = FString::Printf(
				TEXT( "%s (%s)" ),
				*FString( TestTrackSoap->soap->fault->faultstring ),
				*FString( TestTrackSoap->soap->fault->faultcode ) );
		}
		else
		{
			// Store the error message string so it can be reported back to the user later
			LastErrorMessage = FString::Printf(
				TEXT( "TestTrack SOAP API reports a result code of %i" ),
				ResultCode );
		}


		// Check to see if this was a disconnection error
		// @todo: Are there other SOAP or TestTrack error codes that indicate disconnection?  Handle them here!
		bLastErrorWasCausedByDisconnection = FALSE;
		{
			// Handle the following error:
			//        ResultCode: 2, faultcode: 'SOAP-ENV:Server', faultstring: 'Session Dropped.', detail: '17'
			if( ResultCode == 2 &&
				TestTrackSoap->soap->fault != NULL &&
				appAtoi( *FString( TestTrackSoap->soap->fault->detail->__any ) ) == 17 )
			{
				bLastErrorWasCausedByDisconnection = TRUE;
			}
		}

		return FALSE;
	}

	LastErrorMessage = TEXT( "No error" );
	bLastErrorWasCausedByDisconnection = FALSE;
	
	return TRUE;
}



/**
 * Queries the server for a list of databases that the user has access to.  This can be called before
 * the user is logged into the server
 *
 * @param	InServerURL			The server URL address
 * @param	InUserName			User name string
 * @param	InPassword			Password string
 * @param	OutDatabaseNames	List of available database names
 *
 * @return	True if successful
 */
UBOOL FTestTrackProvider::QueryAvailableDatabases( const FString& InServerURL, const FString& InUserName, const FString& InPassword, TArray< FString >& OutDatabaseNames )
{
	TDLOG( TEXT( "TestTrack: %s" ), *FString( __FUNCTION__ ) );

	OutDatabaseNames.Empty();


	// Set the server end point
	SetupServerEndpoint( InServerURL );


	// Query list of all TTP databases on the server
	TT1__getDatabaseListResponse TTPDatabaseList;
	INT ResultCode = TestTrackSoap->TT1__getDatabaseList( TTPDatabaseList );		// Out
	if( !VerifyTTPSucceeded( ResultCode ) )
	{
		// Failed to get database list
		return FALSE;
	}


	if( TTPDatabaseList.pDBList != NULL )
	{
		for( INT CurDBIndex = 0; CurDBIndex < TTPDatabaseList.pDBList->__size; ++CurDBIndex )
		{
#if LOG_TASK_DATABASE
			TDLOG( TEXT( "Found database: %s" ), *FString( TTPDatabaseList.pDBList->__ptritem[ CurDBIndex ]->name ) );
#endif

			OutDatabaseNames.AddItem( FString( TTPDatabaseList.pDBList->__ptritem[ CurDBIndex ]->name ) );
		}
	}


	return TRUE;
}



/**
 * Attempts to connect and login to the specified database
 *
 * @param	InServerURL				The server URL address
 * @param	InUserName				User name string
 * @param	InPassword				Password string
 * @param	InDatabaseName			Name of the database to connect to
 * @param	OutUserRealName			[Out] The real name of our user
 * @param	OutResolutionValues		[Out] List of valid fix resolution values
 * @param	OutOpenTaskStatusPrefix	[Out] Name of task status for 'Open' tasks
 *
 * @return	True if successful
 */
UBOOL FTestTrackProvider::ConnectToDatabase( const FString& InServerURL, const FString& InUserName, const FString& InPassword, const FString& InDatabaseName, FString& OutUserRealName, TArray< FString >& OutResolutionValues, FString& OutOpenTaskStatusPrefix )
{
	TDLOG( TEXT( "TestTrack: %s" ), *FString( __FUNCTION__ ) );

	OutUserRealName = TEXT( "" );
	OutResolutionValues.Empty();

	// TestTrack always uses "Open" for open tasks (also, "Open (Verify Failed)")
	OutOpenTaskStatusPrefix = TEXT( "Open" );

	// Set the server end point
	SetupServerEndpoint( InServerURL );


	LONG64 MyCookie = 0;

	// @todo: DatabaseLogon is the deprecated way to login to a project, but ProjectLogon doesn't seem to work
	INT ResultCode = TestTrackSoap->TT1__DatabaseLogon(
		TCHAR_TO_ANSI( *InDatabaseName ),
		TCHAR_TO_ANSI( *InUserName ),
		TCHAR_TO_ANSI( *InPassword ),
		MyCookie );		// Out


	if( !VerifyTTPSucceeded( ResultCode ) )
	{
		// Login failed
		return FALSE;
	}

	// Store the new ID cookie
	IDCookie = MyCookie;

	TDLOG( TEXT( "TestTrack: Logged in as '%s' (IDCookie: %lld)" ), *InUserName, IDCookie );




	// Check event definitions for custom dropdown field names
	TDLOG( TEXT( "TestTrack: Querying event definitions to gather custom field data" ) );
	{
		TT1__getEventDefinitionListResponse TTPEventDefs;
		ResultCode = TestTrackSoap->TT1__getEventDefinitionList(
			MyCookie,
			"Defect",	// Table name
			TTPEventDefs );		// Out

		if( TTPEventDefs.EventDefinitionList != NULL )
		{
			for( int CurEventIndex = 0; CurEventIndex < TTPEventDefs.EventDefinitionList->__size; ++CurEventIndex )
			{
				TT1__CEventDefinition* CurTTPEventDef = TTPEventDefs.EventDefinitionList->__ptritem[ CurEventIndex ];

				// We're looking for custom fields for "Fix" event types
				if( FString( CurTTPEventDef->name ) == TEXT( "Fix" ) )	// This is a standard TTP even type
				{
					// Any custom fields for this event type?
					if( CurTTPEventDef->customFields != NULL )
					{
						for( int CurCustomFieldIndex = 0; CurCustomFieldIndex < CurTTPEventDef->customFields->__size; ++CurCustomFieldIndex )
						{
							TT1__CField* CurTTPCustomField = CurTTPEventDef->customFields->__ptritem[ CurCustomFieldIndex ];

							// We're looking for dropdown values for the 'Resolution' custom event
							const FString ResolutionCustomFieldName = TEXT( "Resolution" );		// This is our custom event field
							if( CurTTPCustomField->soap_type() == SOAP_TYPE_TT1__CDropdownField &&
								FString( CurTTPCustomField->name ) == ResolutionCustomFieldName )
							{
								TDLOG( TEXT( "TestTrack: Found Resolution custom field for Fix event" ) );

								TT1__CDropdownField* TTPDropdownField =
									static_cast< TT1__CDropdownField* >( CurTTPCustomField );

								// Any dropdown values?
								if( TTPDropdownField->dropdownValues != NULL )
								{
									for( int CurDropdownValue = 0; CurDropdownValue < TTPDropdownField->dropdownValues->__size; ++CurDropdownValue )
									{
										// Found a dropdown value!
										FString ResolutionOption = TTPDropdownField->dropdownValues->__ptritem[ CurDropdownValue ]->value;

										TDLOG( TEXT( "TestTrack: Found Resolution dropdown value: %s" ), *ResolutionOption );

										OutResolutionValues.AddUniqueItem( ResolutionOption );
									}
								}
							}
						}
					}
				}
			}
		}
	}


	// Figure out what our real name is (so we can use it to filter tasks, etc.)
	TDLOG( TEXT( "TestTrack: Querying user list to find user's real name" ) );
	{
		TT1__getGlobalUserListResponse TTPUserList;
		ResultCode = TestTrackSoap->TT1__getGlobalUserList(
			IDCookie,
			TTPUserList );	// Out
			
		//
		// NOTE: If this returns the following error:
		//			"You do not have sufficient License Server security clearance to perform this function."
		//
		//   You'll need to change the permission settings in the TestTrack License Server Admin Utility
		//   such that users can have permission to View Global Users.
		//

		// @todo: It would be great if Seapine could give us the user's real name at connection time
		//        instead of having to query

		if( VerifyTTPSucceeded( ResultCode ) )
		{
			if( TTPUserList.GlobalUserList != NULL )
			{
				// Find the current user in the list
				for( INT CurUserIndex = 0; CurUserIndex < TTPUserList.GlobalUserList->__size; ++CurUserIndex )
				{
					const TT1__CGlobalUser& CurUser = *TTPUserList.GlobalUserList->__ptritem[ CurUserIndex ];

					if( CurUser.loginname != NULL && FString( CurUser.loginname ) == InUserName )
					{
						// Make sure we have a valid real name
						if( CurUser.name != NULL && FString( CurUser.name ).Len() > 0 )
						{
							// Great, we found our real name!
							OutUserRealName = CurUser.name;

							TDLOG( TEXT( "TestTrack: Found my real name: %s" ), *FString( CurUser.name ) );


#if 0
							// TestTrack has a per-user option for how the user's real name should be displayed,
							// either 'First Middle Last' or 'Last, First Middle'.  We'll try to convert it here.
							INT CommaPos = OutUserRealName.InStr( TEXT( "," ) );
							if( CommaPos != INDEX_NONE )
							{
								// OK, the user's real name was reported in the 'Last, First Middle' format.
								FString ReversedRealName = OutUserRealName;

								// Grab text after the comma ('First Middle')
								OutUserRealName = ReversedRealName.Right( ReversedRealName.Len() - ( CommaPos + 2 ) );

								// Append a space
								OutUserRealName += TEXT( " " );

								// Append text before the comma ('Last')
								OutUserRealName += ReversedRealName.Left( CommaPos );

								TDLOG( TEXT( "TestTrack: Converted real name to 'First Middle Last' format: %s" ), *OutUserRealName );
							}
#endif


							break;
						}
					}
				}
			}

			if( OutUserRealName.Len() == 0 )
			{
				// User name wasn't found in the list -- this should never happen!
				TDLOG( TEXT( "TestTrack: Couldn't find real name for user!" ) );
			}
		}

		if( OutUserRealName.Len() == 0 )
		{
			// Couldn't query user list.  This isn't a fatal error, it just means we won't have access
			// to the user's real name, which may restrict our ability to manually filter tasks.

			// Server wasn't able to retrieve the user's real name (probably due to permissions issues),
			// so we'll try to use the user's login name as the real name for filtering
			OutUserRealName = InUserName;

			// @todo: We may need to find a better way to get the user's real name in this case

			// @todo: This will never give us the correct name when the user's TTP options are set to "Last, First Middle" mode

			// Convert . characters in login name to spaces to get the real name
			OutUserRealName.ReplaceInline( TEXT( "." ), TEXT( " " ) );

			// Convert to mixed case
			for( INT CurCharIndex = 0; CurCharIndex < OutUserRealName.Len(); ++CurCharIndex )
			{
				if( CurCharIndex == 0 || OutUserRealName[ CurCharIndex - 1 ] == TCHAR( ' ' ) )
				{
					OutUserRealName[ CurCharIndex ] = appToUpper( OutUserRealName[ CurCharIndex ] );
				}
			}


			TDLOG( TEXT( "TestTrack: Tried to guess at user's real name for %s, as: %s" ), *InUserName, *OutUserRealName );
		}
	}


	
	// Store the user real name so we can use it later for marking tasks as fixed
	UserRealName = OutUserRealName;


	return TRUE;
}



/**
 * Logs the user off and disconnects from the database
 *
 * @return	True if successful
 */
UBOOL FTestTrackProvider::DisconnectFromDatabase()
{
	TDLOG( TEXT( "TestTrack: %s" ), *FString( __FUNCTION__ ) );

	// Log the user out of TTP and disconnect from the database
	INT LogoffResultCode = 0;
	INT ResultCode = TestTrackSoap->TT1__DatabaseLogoff( IDCookie, LogoffResultCode );

	if( !VerifyTTPSucceeded( ResultCode ) || !VerifyTTPSucceeded( LogoffResultCode ) )
	{
		// Failed to logoff
		return FALSE;
	}

	// Great, we logged off successfully.
	TDLOG( TEXT( "TestTrack: Logged out" ) );

	return TRUE;
}



/**
 * Retrieves a list of filters from the database
 *
 * @param	OutFilterNames	List filters names
 *
 * @return	Returns true if successful
 */
UBOOL FTestTrackProvider::QueryFilters( TArray< FString >& OutFilterNames )
{
	TDLOG( TEXT( "TestTrack: %s" ), *FString( __FUNCTION__ ) );

	OutFilterNames.Empty();


	TT1__getFilterListResponse TTPFilterList;
	INT ResultCode = TestTrackSoap->TT1__getFilterList(
		IDCookie,				// Cookie
		TTPFilterList );		// Out


	if( !VerifyTTPSucceeded( ResultCode ) )
	{
		// Couldn't get filter list!
		return FALSE;
	}


	if( TTPFilterList.pFilterList != NULL )
	{
		const INT FilterCount = TTPFilterList.pFilterList->__size;

		// Pre-size the destination array
		OutFilterNames.Empty( FilterCount );

		for( int CurFilterIndex = 0; CurFilterIndex < FilterCount; ++CurFilterIndex )
		{
			OutFilterNames.AddItem( TTPFilterList.pFilterList->__ptritem[ CurFilterIndex ]->name );
		}
	}


	return TRUE;
}



/**
 * Retrieves a list of tasks from the database that matches the specified filter name
 *
 * @param	InFilterName	Filter name to restrict the request by
 * @param	OutTaskList		List of downloaded tasks
 *
 * @return	Returns true if successful
 */
UBOOL FTestTrackProvider::QueryTasks( const FString& InFilterName, TArray< FTaskDatabaseEntry >& OutTaskList )
{
	TDLOG( TEXT( "TestTrack: %s" ), *FString( __FUNCTION__ ) );

	OutTaskList.Empty();


	// Figure out which column indices we're interested in
	UINT ColumnIndex_Number = INDEX_NONE;
	UINT ColumnIndex_Priority = INDEX_NONE;
	UINT ColumnIndex_Name = INDEX_NONE;
	UINT ColumnIndex_AssignedTo = INDEX_NONE;
	UINT ColumnIndex_Status = INDEX_NONE;
	UINT ColumnIndex_CreatedBy = INDEX_NONE;

	// Setup list of database columns we're interested in retrieving
	TT1ArrayOfCTableColumn TTPTableColumnList;
	{
		INT NextColumnIndex = 0;

		TTPTableColumnList.__size = 6;
		TTPTableColumnList.__ptritem = ( TT1__CTableColumn** )soap_malloc( TestTrackSoap->soap, TTPTableColumnList.__size * sizeof( TT1__CTableColumn* ) );

		// Number
		ColumnIndex_Number = NextColumnIndex;
		TTPTableColumnList.__ptritem[ NextColumnIndex ] = soap_new_TT1__CTableColumn( TestTrackSoap->soap, INDEX_NONE );
		TTPTableColumnList.__ptritem[ NextColumnIndex ]->name = "Number";
		++NextColumnIndex;

		// Priority
		ColumnIndex_Priority = NextColumnIndex;
		TTPTableColumnList.__ptritem[ NextColumnIndex ] = soap_new_TT1__CTableColumn( TestTrackSoap->soap, INDEX_NONE );
		TTPTableColumnList.__ptritem[ NextColumnIndex ]->name = "Priority";
		++NextColumnIndex;

		// Name
		ColumnIndex_Name = NextColumnIndex;
		TTPTableColumnList.__ptritem[ NextColumnIndex ] = soap_new_TT1__CTableColumn( TestTrackSoap->soap, INDEX_NONE );
		TTPTableColumnList.__ptritem[ NextColumnIndex ]->name = "Summary";
		++NextColumnIndex;

		// AssignedTo
		ColumnIndex_AssignedTo = NextColumnIndex;
		TTPTableColumnList.__ptritem[ NextColumnIndex ] = soap_new_TT1__CTableColumn( TestTrackSoap->soap, INDEX_NONE );
		TTPTableColumnList.__ptritem[ NextColumnIndex ]->name = "Currently Assigned To";
		++NextColumnIndex;

		// Status
		ColumnIndex_Status = NextColumnIndex;
		TTPTableColumnList.__ptritem[ NextColumnIndex ] = soap_new_TT1__CTableColumn( TestTrackSoap->soap, INDEX_NONE );
		TTPTableColumnList.__ptritem[ NextColumnIndex ]->name = "Status";
		++NextColumnIndex;

		// Created By
		ColumnIndex_CreatedBy = NextColumnIndex;
		TTPTableColumnList.__ptritem[ NextColumnIndex ] = soap_new_TT1__CTableColumn( TestTrackSoap->soap, INDEX_NONE );
		TTPTableColumnList.__ptritem[ NextColumnIndex ]->name = "Created By";	// Entered by
		++NextColumnIndex;

		check( NextColumnIndex == TTPTableColumnList.__size );
	}


	// TTP stores defects in the 'Defect' table
	// For now, we only care about the "Defect" table.  Other tables exist such as "User",
	// "Customer", "Task", "Links", "Folder", etc.
	ANSICHAR* ANSIDefectsTableName = "Defect";

	FTCHARToANSI ANSIFilterName( ( const TCHAR* )*InFilterName );

	// @todo: Sometimes this returns 'Session busy with prior request' errors
	TT1__getRecordListForTableResponse TTPRecordList;
	INT ResultCode = TestTrackSoap->TT1__getRecordListForTable(
		IDCookie,				// Cookie
		ANSIDefectsTableName,	// Table name
		ANSIFilterName,			// Filter name
		&TTPTableColumnList,	// Optional table column list
		TTPRecordList );		// Out


	if( !VerifyTTPSucceeded( ResultCode ) )
	{
		// Couldn't get record list!
		return FALSE;
	}



	if( TTPRecordList.recordlist != NULL &&
		TTPRecordList.recordlist->records != NULL )
	{
		const INT RecordCount = TTPRecordList.recordlist->records->__size;

		// Pre-size the destination array
		OutTaskList.Empty( RecordCount );

		for( int CurRecordIndex = 0; CurRecordIndex < RecordCount; ++CurRecordIndex )
		{
			FTaskDatabaseEntry NewTaskEntry;

			// Number
			if( ColumnIndex_Number != INDEX_NONE )
			{
				const TT1__CRecordData& TTPRecordDataForColumn =
					*TTPRecordList.recordlist->records->__ptritem[ CurRecordIndex ]->row->__ptritem[ ColumnIndex_Number ];
				if( TTPRecordDataForColumn.value != NULL )
				{
					NewTaskEntry.Number = appAtoi( ANSI_TO_TCHAR( TTPRecordDataForColumn.value ) );
				}
			}

			// Priority
			if( ColumnIndex_Priority != INDEX_NONE )
			{
				const TT1__CRecordData& TTPRecordDataForColumn =
					*TTPRecordList.recordlist->records->__ptritem[ CurRecordIndex ]->row->__ptritem[ ColumnIndex_Priority ];
				if( TTPRecordDataForColumn.value != NULL )
				{
					NewTaskEntry.Priority = TTPRecordDataForColumn.value;
				}
			}

			// Name
			if( ColumnIndex_Name != INDEX_NONE )
			{
				const TT1__CRecordData& TTPRecordDataForColumn =
					*TTPRecordList.recordlist->records->__ptritem[ CurRecordIndex ]->row->__ptritem[ ColumnIndex_Name ];
				if( TTPRecordDataForColumn.value != NULL )
				{
					NewTaskEntry.Name = TTPRecordDataForColumn.value;

					// @todo: For whatever reason, TestTrack seems to crop task names to 126 characters and then
					//    insert &* characters afterwards.  We'll clean up the text here.
					if( NewTaskEntry.Name.Len() > 126 )
					{
						NewTaskEntry.Name = NewTaskEntry.Name.Left( 126 );
					}
				}
			}
			
			// AssignedTo
			if( ColumnIndex_AssignedTo != INDEX_NONE )
			{
				const TT1__CRecordData& TTPRecordDataForColumn =
					*TTPRecordList.recordlist->records->__ptritem[ CurRecordIndex ]->row->__ptritem[ ColumnIndex_AssignedTo ];
				if( TTPRecordDataForColumn.value != NULL )
				{
					NewTaskEntry.AssignedTo = TTPRecordDataForColumn.value;
				}
			}

			// Status
			if( ColumnIndex_Status != INDEX_NONE )
			{
				const TT1__CRecordData& TTPRecordDataForColumn =
					*TTPRecordList.recordlist->records->__ptritem[ CurRecordIndex ]->row->__ptritem[ ColumnIndex_Status ];
				if( TTPRecordDataForColumn.value != NULL )
				{
					NewTaskEntry.Status = TTPRecordDataForColumn.value;
				}
			}

			// CreatedBy
			if( ColumnIndex_CreatedBy != INDEX_NONE )
			{
				const TT1__CRecordData& TTPRecordDataForColumn =
					*TTPRecordList.recordlist->records->__ptritem[ CurRecordIndex ]->row->__ptritem[ ColumnIndex_CreatedBy ];
				if( TTPRecordDataForColumn.value != NULL )
				{
					NewTaskEntry.CreatedBy = TTPRecordDataForColumn.value;
				}
			}

			OutTaskList.AddItem( NewTaskEntry );
		}
	}


	return TRUE;
}



 
/**
 * Retrieves details about a specific task from the database
 *
 * @param	InNumber		Task number
 * @param	OutDetails		[Out] Details for the requested task
 *
 * @return	Returns true if successful
 */
UBOOL FTestTrackProvider::QueryTaskDetails( const UINT InNumber, FTaskDatabaseEntryDetails& OutDetails )
{
	TDLOG( TEXT( "TestTrack: %s" ), *FString( __FUNCTION__ ) );

	// Reset output
	OutDetails = FTaskDatabaseEntryDetails();


	// Query defect info
	TT1__getDefectResponse TTPDefect;
	INT ResultCode = TestTrackSoap->TT1__getDefect(
		IDCookie,				// Cookie
		InNumber,				// Defect number
		NULL,					// Summary string (optional)
		false,					// Download attachments?
		TTPDefect );			// Out
	

	if( !VerifyTTPSucceeded( ResultCode ) )
	{
		// Couldn't get defect info!
		return FALSE;
	}

	
	OutDetails.Number = *TTPDefect.pDefect->defectnumber;
	OutDetails.Name = TTPDefect.pDefect->summary;
	OutDetails.Priority = TTPDefect.pDefect->priority;
	OutDetails.Status = TTPDefect.pDefect->state;
	OutDetails.CreatedBy = TTPDefect.pDefect->createdbyuser;


	// @todo: Can we use bold text here to make things look nicer?
	//         -> We're already using a rich text control, so it seems likely

	// @todo: Display date created, date modified, event time/dates, other fields, etc.

	OutDetails.Description = TEXT( "\n" );

	if( TTPDefect.pDefect->reportedbylist != NULL )
	{
		for( INT CurReportIndex = 0; CurReportIndex < TTPDefect.pDefect->reportedbylist->__size; ++CurReportIndex )
		{
			OutDetails.Description += FString( TTPDefect.pDefect->reportedbylist->__ptritem[ CurReportIndex ]->comments );
			OutDetails.Description += TEXT( "\n\n" );
		}
	}

	if( TTPDefect.pDefect->eventlist != NULL )
	{
		for( INT CurReportIndex = 0; CurReportIndex < TTPDefect.pDefect->eventlist->__size; ++CurReportIndex )
		{
			// @todo: Skip display of certain event types? (Auto assignments and such)

			OutDetails.Description += TEXT( "\n-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n" );

			OutDetails.Description +=
				FString::Printf(
					TEXT( "\n%s Event by %s (%s):\n\n" ),
					*FString( TTPDefect.pDefect->eventlist->__ptritem[ CurReportIndex ]->name ),
					*FString( TTPDefect.pDefect->eventlist->__ptritem[ CurReportIndex ]->user ),
					*FString( TTPDefect.pDefect->eventlist->__ptritem[ CurReportIndex ]->generatedeventtype ) );

			OutDetails.Description += FString( TTPDefect.pDefect->eventlist->__ptritem[ CurReportIndex ]->notes );
			OutDetails.Description += TEXT( "\n" );
		}
	}


	return TRUE;
}




/**
 * Marks the specified task as complete
 *
 * @param	InNumber			Task number
 * @param	InResolutionData	Resolution data for this task
 *
 * @return	Returns true if successful
 */
UBOOL FTestTrackProvider::MarkTaskComplete( const UINT InNumber, const FTaskResolutionData& InResolutionData )
{
	TDLOG( TEXT( "TestTrack: %s" ), *FString( __FUNCTION__ ) );


	// Open the defect for editing
	TT1__editDefectResponse TTPDefect;
	int ResultCode = TestTrackSoap->TT1__editDefect(
		IDCookie,				// Cookie
		InNumber,				// Defect number
		NULL,					// Summary string (optional)
		false,					// Download attachments?
		TTPDefect );			// Out

	if( !VerifyTTPSucceeded( ResultCode ) )
	{
		// Couldn't open defect for editing
		return FALSE;
	}



	// Grab the current date and time
	time_t CurrentDateTime;
	time( &CurrentDateTime );

	DOUBLE HoursToComplete = InResolutionData.HoursToComplete;

	TDLOG( TEXT( "TestTrack: User real name for defect accountability: '%s'" ), *UserRealName );

	FTCHARToANSI ANSIUserRealName( ( const TCHAR* )*UserRealName );


	// Change bug status to 'Fixed'
	TTPDefect.pDefect->state = "Fixed";
	TTPDefect.pDefect->datetimemodified = &CurrentDateTime;
	TTPDefect.pDefect->modifiedbyuser = ANSIUserRealName;
	TTPDefect.pDefect->actualhourstofix = &HoursToComplete;


	FTCHARToANSI ANSIComments( ( const TCHAR* )*InResolutionData.Comments );
	FTCHARToANSI ANSIResolutionType( ( const TCHAR* )*InResolutionData.ResolutionType );
	FTCHARToANSI ANSIChangelistNumber( ( const TCHAR* )*FString( appItoa( InResolutionData.ChangelistNumber ) ) );


	// Add a 'fix' event
	{
		TT1__CEvent* NewEvent = soap_new_TT1__CEvent( TestTrackSoap->soap, INDEX_NONE );
		{
			NewEvent->user = ANSIUserRealName;
 			NewEvent->notes = ANSIComments;
 			NewEvent->name = "Fix";
 			NewEvent->generatedeventtype = "User";
 			NewEvent->date = CurrentDateTime;
			NewEvent->hours = &HoursToComplete;
			NewEvent->totaltimespent = &HoursToComplete;


			// Fill in custom fields
			{
				NewEvent->fieldlist = soap_new_TT1ArrayOfCField( TestTrackSoap->soap, INDEX_NONE );

				const UINT CustomFieldCount = 4;
				NewEvent->fieldlist->__size = CustomFieldCount;
				NewEvent->fieldlist->__ptritem = ( TT1__CField** )soap_malloc( TestTrackSoap->soap, NewEvent->fieldlist->__size * sizeof( TT1__CField* ) );
				UINT NextCustomFieldIndex = 0;

				{
					TT1__CBooleanField* NewField = soap_new_TT1__CBooleanField( TestTrackSoap->soap, INDEX_NONE );
					NewField->name = "Affects Documentation";
					NewField->value = false;	// @todo: Support this?
					NewEvent->fieldlist->__ptritem[ NextCustomFieldIndex++ ] = NewField;
				}

				{
					TT1__CBooleanField* NewField = soap_new_TT1__CBooleanField( TestTrackSoap->soap, INDEX_NONE );
					NewField->name = "Affects Test Plan";
					NewField->value = false;	// @todo: Support this?
					NewEvent->fieldlist->__ptritem[ NextCustomFieldIndex++ ] = NewField;
				}

				{
					TT1__CDropdownField* NewField = soap_new_TT1__CDropdownField( TestTrackSoap->soap, INDEX_NONE );
					NewField->name = "Resolution";
					NewField->value = ANSIResolutionType;
					NewEvent->fieldlist->__ptritem[ NextCustomFieldIndex++ ] = NewField;
				}

				{
					TT1__CVersionField* NewField = soap_new_TT1__CVersionField( TestTrackSoap->soap, INDEX_NONE );
					NewField->name = "Changelist";
					NewField->value = ANSIChangelistNumber;
					NewEvent->fieldlist->__ptritem[ NextCustomFieldIndex++ ] = NewField;
				}
			}
		}


		UINT OldEventCount = 0;
		TT1__CEvent** OldEventList = NULL;
		if( TTPDefect.pDefect->eventlist != NULL )
		{
			OldEventCount = TTPDefect.pDefect->eventlist->__size;
			OldEventList = TTPDefect.pDefect->eventlist->__ptritem;
		}
		else
		{
			// Allocate event list if we need it (should never have to, really)
			TTPDefect.pDefect->eventlist = soap_new_TT1ArrayOfCEvent( TestTrackSoap->soap, INDEX_NONE );
		}

		// Allocate the new event array
		// NOTE: The old event array is still linked to SOAP and will be automatically cleaned up later
		TTPDefect.pDefect->eventlist->__size = OldEventCount + 1;
		TTPDefect.pDefect->eventlist->__ptritem =
			( TT1__CEvent** )soap_malloc( TestTrackSoap->soap, TTPDefect.pDefect->eventlist->__size * sizeof( TT1__CEvent* ) );

		// Copy old events over
		for( UINT CurEventIndex = 0; CurEventIndex < OldEventCount; ++CurEventIndex )
		{
			TTPDefect.pDefect->eventlist->__ptritem[ CurEventIndex ] = OldEventList[ CurEventIndex ];
		}
		
		// Add new event to the list
		const UINT NewEventIndex = OldEventCount;
		TTPDefect.pDefect->eventlist->__ptritem[ NewEventIndex ] = NewEvent;
	}


	// @todo: After saving the defect, why is 'AM Fix Priority' (custom field) and 'Estimated Effort'
	//		unexpectedly showing up as edited in the bug's history?

	// Save changes to this defect
	int SaveResultCode = 0;
	ResultCode = TestTrackSoap->TT1__saveDefect(
		IDCookie,				// Cookie
		TTPDefect.pDefect,		// Defect (with changes to save)
		SaveResultCode );		// Out


	if( !VerifyTTPSucceeded( ResultCode ) || !VerifyTTPSucceeded( SaveResultCode ) )
	{
		// Couldn't save defect changes
		return FALSE;
	}

	return TRUE;
}


#endif	// WITH_TESTTRACK


