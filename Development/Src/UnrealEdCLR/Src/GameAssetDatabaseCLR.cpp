/*================================================================================
	GameAssetDatabaseCLR.cpp: System for globally browsing and tagging game assets
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEdCLR.h"

#include "GameAssetDatabaseShared.h"
#include "ManagedCodeSupportCLR.h"

using namespace System::IO;
using namespace System::Globalization;

/** Static: Global instance of the game asset database */
FGameAssetDatabase* FGameAssetDatabase::GameAssetDatabaseSingleton = NULL;



// @todo CB: Should use SQL stored procedures for better perf/security, etc




/** Determines the working name of the current development branch */
String^ MGameAssetJournalBase::QueryBranchName()
{
	// Load the name for the current branch from the configuration file
	// @todo CB: Consider moving this setting to it's own file so branching can be disabled in P4?
	String^ BranchName =
		gcnew String( *GConfig->GetStr( TEXT( "GameAssetDatabase" ), TEXT( "BranchName" ), GEditorIni ) );
	
	if( BranchName->Length == 0 )
	{
		FString BranchNameStr;
		Parse(appCmdLine(), TEXT("GADBranchName="), BranchNameStr);

		BranchName = CLRTools::ToString( BranchNameStr );

		if( BranchName->Length == 0 )
		{
#if UDK
			// UDK users with perforce disabled need to set a branch name the first time
			FString UDKBranchName = TEXT("UDKOfflineBranch");
			BranchName = CLRTools::ToString(UDKBranchName);
			GConfig->SetString( TEXT( "GameAssetDatabase" ), TEXT( "BranchName" ), *UDKBranchName, GEditorIni );
#else
			// No branch name was configured in the editor ini file.  A globally unique branch name must be
			// set for this development branch in order for the journaling system to function correctly!
			FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString( "GameAssetDatabase_NoBranchNameSpecified" ) );
			CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::QueryBranchName: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );

			//if there is no branch specified, give it an unknown
			BranchName = UnknownBranchName;
#endif
		}
	}

	return BranchName;
}



/** Creates a text string that represents the specified journal entry */
bool MGameAssetJournalBase::CreateStringFromJournalEntry( MGameAssetJournalEntry^ JournalEntry, [Out] String^% OutString )
{
	OutString = nullptr;


	// Load the name for the current branch from the configuration file
	String^ BranchName = QueryBranchName();
	if( BranchName->Length == 0 )
	{
		// Couldn't query branch name.  Reason is stored in ErrorMessageText
		return false;
	}

	String^ GameName = CLRTools::ToString( GGameName );


	// Create a string containing all of the journal entry data we want to set for the new row
	Text::StringBuilder^ JournalEntryString = gcnew Text::StringBuilder();


	// Version number always comes first
	JournalEntryString->Append( GADDefs::JournalEntryVersionNumber.ToString() );
	JournalEntryString->Append( GADDefs::JournalEntryFieldDelimiter );

	// Development branch name
	JournalEntryString->Append( BranchName );
	JournalEntryString->Append( GADDefs::JournalEntryFieldDelimiter );

	// Game name
	JournalEntryString->Append( GameName );
	JournalEntryString->Append( GADDefs::JournalEntryFieldDelimiter );

	// Time stamp (as Tick Count in string format)
	JournalEntryString->Append( DateTime::Now.Ticks.ToString() );
	JournalEntryString->Append( GADDefs::JournalEntryFieldDelimiter );

	// User name
	JournalEntryString->Append( CLRTools::ToString( FGameAssetDatabase::GetLocalUserName() ) );
	JournalEntryString->Append( GADDefs::JournalEntryFieldDelimiter );

	// Entry type
	JournalEntryString->Append( GADDefs::JournalEntryTypeNames[ static_cast< int >( JournalEntry->Type ) ] );
	JournalEntryString->Append( GADDefs::JournalEntryFieldDelimiter );


	// Add/Remove entry types have the same format: "AssetFullName|TagName"
	if( JournalEntry->Type == EJournalEntryType::AddTag ||
		JournalEntry->Type == EJournalEntryType::RemoveTag )
	{
		// Asset name
		JournalEntryString->Append( JournalEntry->AssetFullName );
		JournalEntryString->Append( GADDefs::JournalEntryFieldDelimiter );

		// Tag (may contain spaces)
		JournalEntryString->Append( JournalEntry->Tag );
	}
	else if( JournalEntry->Type == EJournalEntryType::CreateTag ||
			 JournalEntry->Type == EJournalEntryType::DestroyTag )
	{
		// Tag name or Collection name (either shared or private)
		JournalEntryString->Append( JournalEntry->Tag );
	}
	else
	{
		// Unrecognized command
		check( 0 );
	}


	// Store the final string
	OutString = JournalEntryString->ToString();

	return true;
}

/**
 * Some branch name entries have become malformed. They are of the form UnrealEngine3-BranchSpecifier/Extra/Path/Segments
 *
 * @param InBranchName  A possibly malformed branch name
 *
 * @return A corrected branch name with path segments removed
 */
String^ ValidateBranchName( String^ InBranchName )
{
	int IndexOfSlash = InBranchName->IndexOf("/");
	if ( IndexOfSlash == 0 ) 
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format("Encountered entry with branch name {0}. I don't know how to fix it!", InBranchName) );
		return InBranchName;
	}
	else if ( IndexOfSlash > 0 )
	{
		String^ FixedBranchName = InBranchName->Substring(0, IndexOfSlash);
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format("Encountered entry with branch name {0}. Fixed it; new name is {1}", InBranchName, FixedBranchName) );
		return FixedBranchName;
	}
	else
	{
		return InBranchName;
	}
}


/** Creates a journal entry from the specified journal entry string */
bool MGameAssetJournalBase::CreateJournalEntryFromString( String^ Text, List<String^>^ AllClassNames, [Out] MGameAssetJournalEntry^% OutJournalEntry, bool bFilterBranchAndGame )
{
	OutJournalEntry = nullptr;


	// Load the name for the current branch from the configuration file
	String^ BranchName = QueryBranchName();
	if( BranchName->Length == 0 )
	{
		// Branch query failed.  Reason is stored in ErrorMessageText
		return false;
	}
	

	String^ GameName = CLRTools::ToString( GGameName );


	// Parse the text
	cli::array< TCHAR >^ TokenSeparators = { GADDefs::JournalEntryFieldDelimiter };
	cli::array< String^ >^ SplitStrings = Text->Split( TokenSeparators );


	// @todo CB: Handle malformed data here

	int NextStringIndex = 0;


	// Version number always comes first
	String^	EntryVersionNumberString = SplitStrings[ NextStringIndex++ ];
	int EntryVersionNumber = Convert::ToInt32( EntryVersionNumberString );

	// We never expect to encounter journal data from a newer version of the application!
	// If this check goes off then the build system's editor version needs to be updated!
	if( EntryVersionNumber <= GADDefs::JournalEntryVersionNumber )
	{
		// Branch name 
		String^ EntryBranchName = ValidateBranchName(SplitStrings[ NextStringIndex++ ]);
		bool bValidBranch = (EntryBranchName == BranchName) || (EntryBranchName == UnknownBranchName);

		// Game name
		String^ EntryGameName = SplitStrings[ NextStringIndex++ ];


		// Ignore branches or games that do not match our own
		// If we want to Filter Branch And Name
		if( !bFilterBranchAndGame ||
			( bValidBranch && EntryGameName == GameName ) )
		{
			MGameAssetJournalEntry^ NewJournalEntry = gcnew MGameAssetJournalEntry();
			
			NewJournalEntry->BranchName = EntryBranchName;
			NewJournalEntry->GameName = EntryGameName;

			// Time stamp
			String^ TimeStampString = SplitStrings[ NextStringIndex++ ];
			const UINT64 TimeStampTickCount = Convert::ToUInt64( TimeStampString );
			NewJournalEntry->TimeStamp = gcnew DateTime( TimeStampTickCount );

			// User name
			NewJournalEntry->UserName = SplitStrings[ NextStringIndex++ ];

			// Type name (decides how to parse the rest of the message)
			String^ EntryTypeName = SplitStrings[ NextStringIndex++ ];
			NewJournalEntry->Type =
					static_cast< EJournalEntryType >(
						cli::array< String^ >::IndexOf( GADDefs::JournalEntryTypeNames, EntryTypeName ) );

			NewJournalEntry->IsValidEntry = true;

			if ( NewJournalEntry->Type == EJournalEntryType::AddTag || 
				 NewJournalEntry->Type == EJournalEntryType::RemoveTag )
			{
				if( EntryVersionNumber >= GADDefs::JournalEntryVersionNumber_AssetFullNames )
				{
					// Asset name
					NewJournalEntry->AssetFullName = SplitStrings[ NextStringIndex++ ];
				}
				else
				{
					// Older versions stored only the object path, not the full name
					String^ AssetPath = SplitStrings[ NextStringIndex++ ];

					// Check to see if we happen to already have the asset type for this asset
					NewJournalEntry->IsValidEntry = false;
					{
						for each( String^ CurTestClassName in AllClassNames )
						{
							// Build a full name with the current test class' name and the current assets type
							String^ TestAssetFullName = CurTestClassName + " " + AssetPath;

							// Grab the system tags for this asset (if it even exists in our database!)
							List<String^>^ TestAssetTags;
							FGameAssetDatabase::Get().QueryTagsForAsset( TestAssetFullName, ETagQueryOptions::SystemTagsOnly, TestAssetTags );

							// If we have any tags then we now have a valid asset that we can map to
							if( TestAssetTags->Count > 0 )
							{
								// Great, we found a class that maps to the asset path that was missing a class name!
								// Note that this may not actually be the asset's type, in the (uncommon) case
								// where multiple assets exist with the same path but different classes.  However,
								// it's good enough for simple backwards compatibility.
								NewJournalEntry->AssetFullName = TestAssetFullName;
								NewJournalEntry->IsValidEntry = true;
								break;
							}
						}
					}

					if( NewJournalEntry->IsValidEntry )
					{
						CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
							"    Repaired asset full name in journal entry that was missing a class name: {0} -> {1}", AssetPath, NewJournalEntry->AssetFullName ) );
					}
					else
					{
						CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
							"    Found malformed or legacy journal entry (Text='{0}')", Text ) );
					}

				}

				// Tag
				NewJournalEntry->Tag = SplitStrings[ NextStringIndex++ ];
			}
			else if ( NewJournalEntry->Type == EJournalEntryType::CreateTag || 
					  NewJournalEntry->Type == EJournalEntryType::DestroyTag)
			{
				// Tag name or Collection name (either shared or private)
				NewJournalEntry->Tag = SplitStrings[ NextStringIndex++ ];
			}
			else
			{
				// Unrecognized command
				check( 0 );
			}


			OutJournalEntry = NewJournalEntry;
		}
		else
		{
			// Entry was for another game branch, or user.  Ignored!
			return false;
		}
	}
	else
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
			"    Ignored journal entry for newer version than we were built with! (OurVer={0}, Theirs={1}, Text='{2}')",
			GADDefs::JournalEntryVersionNumber, EntryVersionNumber, Text ) );
		return false;
	}

	return true;
}



/** Uploads a journal entry to the server */
bool MGameAssetJournalBase::SendJournalEntry( MGameAssetJournalEntry^ JournalEntry )
{
	List<MGameAssetJournalEntry^>^ JournalEntries = gcnew List<MGameAssetJournalEntry^>(1);
	JournalEntries->Add( JournalEntry );
	return SendJournalEntries( JournalEntries );
}



/** Constructor */
MGameAssetJournalClient::MGameAssetJournalClient()
{
}



/** Destructor */
MGameAssetJournalClient::~MGameAssetJournalClient()
{
	// Make sure we're not still connected
	if( !DisconnectFromServer() )
	{
		// Error while disconnecting during destruction.  Print a log warning.
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient: Error while disconnecting from database in destructor.  Details: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
	}
}



/** Connect to the database server */
bool MGameAssetJournalClient::ConnectToServer()
{
	// Load SQL connection info from configuration files
	String^ JournalServer =
		gcnew String( *GConfig->GetStr( TEXT( "GameAssetDatabase" ), TEXT( "JournalServer" ), GEditorIni ) );
	String^ JournalDatabase =
		gcnew String( *GConfig->GetStr( TEXT( "GameAssetDatabase" ), TEXT( "JournalDatabase" ), GEditorIni ) );

	if( JournalServer->Length == 0 || JournalDatabase->Length == 0 )
	{
		// No server configured
		FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString( "GameAssetDatabase_JournalServerNotSpecified" ) );
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::ConnectToServer: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
		return false;
	}

	// Load the connection security type from configuration files 
	UBOOL bUseIntegratedSecurity = TRUE;
		GConfig->GetBool( TEXT( "GameAssetDatabase" ), TEXT( "bUseIntegratedSecurity" ), bUseIntegratedSecurity, GEditorIni );
	String^ UserID =
		gcnew String( *GConfig->GetStr( TEXT( "GameAssetDatabase" ), TEXT( "UserId" ), GEditorIni ) );
	String^ Password =
		gcnew String( *GConfig->GetStr( TEXT( "GameAssetDatabase" ), TEXT( "Password" ), GEditorIni ) );

	if( !bUseIntegratedSecurity && ( UserID->Length == 0 || Password->Length == 0 ) )
	{
		// No login configured
		FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString( "GameAssetDatabase_JournalLoginNotSpecified" ) );
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::ConnectToServer: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
		return false;
	}

	// Try to connect to the server
	try
	{
		String^ ConnectionString = String::Format(
			"Data Source={0};" +						// Server host and name
			"Initial Catalog={1};" +					// Database name
			"Pooling=True;" +							// Pool connections?
			"Asynchronous Processing=True;" +			// Enable asynchronous operations?
			"Connection Timeout=4",						// Connection timeout,
			JournalServer,
			JournalDatabase );

		if(bUseIntegratedSecurity) // Use Windows credentials?
		{
			ConnectionString += ";Integrated Security=True";
		}
		else // Otherwise use login specified in the config
		{
			ConnectionString += String::Format(";User ID={0};Password={1}", UserID, Password );
		}

		// Setup connection string
		MySqlConnection.reset( gcnew SqlConnection( ConnectionString ) );

		// Go ahead and connect to the server		
		MySqlConnection->Open();
	}
	catch( Exception^ E )
	{
		MySqlConnection.reset();

		// @todo CB: Use thrown exception strings here instead of error strings and bools?
		FGameAssetDatabase::Get().SetErrorMessageText(
			CLRTools::LocalizeString(
				"GameAssetDatabase_ErrorConnectingToJournalServer_F",
				JournalServer,		// Server name
				JournalDatabase,	// Database name
				E->ToString() ) );	// Exception details

		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::ConnectToServer: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );

		return false;
	}



	return true;
}



/** Disconnects from the database server */
bool MGameAssetJournalClient::DisconnectFromServer()
{
	if( MySqlConnection )
	{
		try
		{
			// Close the connection
			MySqlConnection->Close();
		}
		catch( Exception^ E )
		{
			FGameAssetDatabase::Get().SetErrorMessageText(
				CLRTools::LocalizeString(
					"GameAssetDatabase_ErrorDisconnectingFromJournalServer_F",
					E->ToString() ) );	// Exception details

			CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::DisconnectFromServer: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
			return false;
		}

		// Dispose of the SQL connection object
		MySqlConnection.reset();
	}

	return true;
}

/** Loads journal entries from the server */
bool MGameAssetJournalClient::QueryJournalEntries( [Out] MGameAssetJournalEntryList^% OutJournalEntries, bool bFilterBranchAndGame )
{
	// Load the name for the current branch from the configuration file
	String^ BranchName = QueryBranchName();
	if( BranchName->Length == 0 )
	{
		// Branch query failed.  Reason is stored in ErrorMessageText
		return false;
	}


	// Connect to the server
	if( !ConnectToServer() )
	{
		// Server connect failed.  Reason is stored in ErrorMessageText
		return false;
	}


	// Query Journal Entries, if an error occurs: reason is stored in ErrorMessageText
	bool bRet = _QueryJournalEntries( OutJournalEntries, bFilterBranchAndGame );


	// We're done, so disconnect!
	if( !DisconnectFromServer() )
	{
		// Error while disconnecting.  Print a log warning.
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient: Error while disconnecting from database in QueryJournalEntries.  Details: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
	}


	return bRet;
}

bool MGameAssetJournalClient::_QueryJournalEntries( [Out] MGameAssetJournalEntryList^% OutJournalEntries, bool bFilterBranchAndGame )
{
	// Load the name for the current branch from the configuration file
	String^ BranchName = QueryBranchName();
	if( BranchName->Length == 0 )
	{
		// Branch query failed.  Reason is stored in ErrorMessageText
		return false;
	}


	OutJournalEntries = gcnew MGameAssetJournalEntryList();


	// Build a list of all classes.  We need this for backwards compatibility stuff below.
	List<String^> AllClassNames;
	for( TObjectIterator<UClass> It; It; ++It )
	{
		// @todo CB: Some of these may not actually be browsable asset types, but they shouldn't
		//   have the same path as any asset so it won't matter in practice
		AllClassNames.Add( CLRTools::ToString( It->GetName() ) );
	}

	String^ GameName = CLRTools::ToString( GGameName );

	try
	{
		String^ SQLQuery = (bFilterBranchAndGame) ?
			String::Format( "select * from dbo.Entries where (substring(Text, {0}, {2}) = '{1}') and (substring(Text, {3}, {5}) = '{4}')",
				3, BranchName, BranchName->Length,
				3 + BranchName->Length + 1, GameName, GameName->Length ) :
			"select * from dbo.Entries";

		// Setup SQL command to read entries from the database
		auto_handle< SqlCommand > MySqlCommand =
			gcnew SqlCommand(SQLQuery, MySqlConnection.get() );

		// Start reading from the SQL database
		auto_handle< SqlDataReader > MySqlReader = MySqlCommand->ExecuteReader();


		// Parse the data
		while( MySqlReader->Read() )
		{
			// First column is Database Index
			int DatabaseIndex = safe_cast< int >( MySqlReader.get()[ 0 ] );

			// Second column is Text
			String^ Text = safe_cast< String^ >( MySqlReader.get()[ 1 ] );



			MGameAssetJournalEntry^ NewJournalEntry = nullptr;
			if( CreateJournalEntryFromString( Text, %AllClassNames, NewJournalEntry, bFilterBranchAndGame ) )
			{
				NewJournalEntry->IsOfflineEntry = false;

				// Set the database index.  We'll use this both for sorting and also to uniquely identify
				// this journal entry on the SQL server (in case we need to delete it later!)
				NewJournalEntry->DatabaseIndex = DatabaseIndex;

				// Add to the list.  We'll sort them later.
				OutJournalEntries->Add( NewJournalEntry );
			}
			else
			{
				// Entry was for another game branch, or user.  Ignored!
			}
		}
	}

	catch( Exception^ E )
	{
		FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString(
				"GameAssetDatabase_ErrorExecutingSQLCommand_F",
				E->ToString() ) );	// Exception details
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::QueryJournalEntries: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
		return false;
	}

	return true;
}

/** Deletes journal entries with the specified database indices from the server */
bool MGameAssetJournalClient::DeleteJournalEntries( List< int >^ DatabaseIndices )
{
	// Check the number of indicies to delete
	if( DatabaseIndices->Count < 1 )
	{
		// No indicies to handle
		return false;
	}


	// Load the name for the current branch from the configuration file
	String^ BranchName = QueryBranchName();
	if( BranchName->Length == 0 )
	{
		// Branch query failed.  Reason is stored in ErrorMessageText
		return false;
	}


	// Connect to the server
	if( !ConnectToServer() )
	{
		// Server connect failed.  Reason is stored in ErrorMessageText
		return false;
	}


	// Delete Journal Entries, if an error occurs: reason is stored in ErrorMessageText
	bool bRet = _DeleteJournalEntries( DatabaseIndices );


	// We're done, so disconnect!
	if( !DisconnectFromServer() )
	{
		// Error while disconnecting.  Print a log warning.
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient: Error while disconnecting from database in DeleteJournalEntries.  Details: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
	}


	return bRet;
}

bool MGameAssetJournalClient::_DeleteJournalEntries( List< int >^ DatabaseIndices )
{
	// @todo CB: Should we make the database table name (dbo.Entries) data driven in .ini file?


	// We'll be gentle to the server and break up deletions into multiple commands.  This helps
	// to make sure we don't time out while talking to the database.
	const int MaxIndicesPerCommand = 1024;
	int CurIndex = 0;

	while( CurIndex < DatabaseIndices->Count )
	{
		try
		{
			// Construct a string that contains all of the database indices in the following format:
			//		"123, 124, 234, 5123, 52352"
			Text::StringBuilder DatabaseIndicesStringBuilder;
			
			int NumIndicesForCommand = 0;
			while( CurIndex < DatabaseIndices->Count && NumIndicesForCommand < MaxIndicesPerCommand )
			{
				int CurDatabaseIndex = DatabaseIndices[ CurIndex ];

				DatabaseIndicesStringBuilder.Append( CurDatabaseIndex.ToString() );

				++CurIndex;
				++NumIndicesForCommand;

				// Add a comma (only if there will be more indices)
				if( CurIndex < DatabaseIndices->Count &&
					NumIndicesForCommand < MaxIndicesPerCommand )
				{
					DatabaseIndicesStringBuilder.Append( ", " );
				}
			}

			// Setup SQL command to delete entries from the database
			auto_handle< SqlCommand > MySqlCommand =
				gcnew SqlCommand(
					String::Format( "delete from dbo.Entries where DatabaseIndex in ({0})", DatabaseIndicesStringBuilder.ToString() ),
					MySqlConnection.get() );

			// Execute the SQL query
			int NumAffectedRows = MySqlCommand->ExecuteNonQuery();

			// Make sure we successfully modified something!
			if( NumAffectedRows != NumIndicesForCommand )
			{
				FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString(
						"GameAssetDatabase_ErrorExecutingSQLCommand_F",
						TEXT( "The number of affected rows did not match what we were expecting while tring to delete rows" ) ) );	// Exception details
				CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::DeleteJournalEntries: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );

				return false;
			}
		}

		catch( Exception^ E )
		{
			FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString(
					"GameAssetDatabase_ErrorExecutingSQLCommand_F",
					E->ToString() ) );	// Exception details
			CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::DeleteJournalEntries: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
			return false;
		}
	}

	return true;
}

/**
 * Bulk uploads journal entries to the server.
 */
bool MGameAssetJournalClient::SendJournalEntries( List<MGameAssetJournalEntry^>^ JournalEntries )
{
	// Load the name for the current branch from the configuration file
	String^ BranchName = QueryBranchName();
	if( BranchName->Length == 0 )
	{
		// Couldn't query branch name.  Reason is stored in ErrorMessageText
		return false;
	}


	// Connect to the server
	if( !ConnectToServer() )
	{
		// Couldn't connect to server.  Reason is stored in ErrorMessageText
		return false;
	}


	// Send Journal Entries, if an error occurs: reason is stored in ErrorMessageText
	bool bRet = _SendJournalEntries( JournalEntries );


	// We're done, so disconnect!
	if( !DisconnectFromServer() )
	{
		// Error while disconnecting.  Print a log warning.
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient: Error while disconnecting from database in SendJournalEntry.  Details: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
	}


	return bRet;
}

bool MGameAssetJournalClient::_SendJournalEntries( List<MGameAssetJournalEntry^>^ JournalEntries )
{
	// -- The overall strategy is to make a DataTable, populate that DataTable with all the entries we want
	// to send and then use SqlBlkCopy top bulk-copy to DataTable to the Journal DB. --

	// Make a DataTable that serves as our source.
	System::Data::DataTable^ EntriesDataTable = gcnew System::Data::DataTable();

	// Add the 'ID' column
	System::Data::DataColumn^ IdColumn = gcnew System::Data::DataColumn();
	IdColumn->DataType = System::Type::GetType("System.Int32");
	IdColumn->ColumnName="DatabaseIndex";
	EntriesDataTable->Columns->Add( IdColumn );	


	// Add the 'Text' column
	System::Data::DataColumn^ TextColumn = gcnew System::Data::DataColumn();
	TextColumn->DataType = System::Type::GetType("System.String");
	TextColumn->ColumnName="Text";
	EntriesDataTable->Columns->Add( TextColumn );

	// Populate the data table with Journal entries we want to send.
	for each ( MGameAssetJournalEntry^ JournalEntry in JournalEntries )
	{
		// Convert the journal entry to string format
		String^ JournalEntryString;
		if( !CreateStringFromJournalEntry( JournalEntry, JournalEntryString ) )
		{
			// Couldn't convert to string.  Reason is stored in ErrorMessageText
			return false;
		}

		// Add the journal entry into the source DataTable
		System::Data::DataRow^ NewRow = EntriesDataTable->NewRow();

		// Add Journal entry text
		NewRow["Text"] = JournalEntryString;

		// Add identify column's value: this value will be ignored because it is the identity column, but it is needed.
		NewRow["DatabaseIndex"] = gcnew System::Int32( 0 );

		EntriesDataTable->Rows->Add( NewRow );
	}

	// If only a couple to upload, use the async method
	if ( EntriesDataTable->Rows->Count < 4 )	// 3 appears to be the maximum number a newly created asset can have by default (See SetDefaultTagsForAsset)
	{
		for each ( System::Data::DataRow^ Row in EntriesDataTable->Rows )
		{
			try
			{
				// Create the SQL command
				auto_handle< SqlCommand > MySqlCommand =
					gcnew SqlCommand(
					String::Format( "insert into dbo.Entries (Text) values ('{0}')", Row["Text"]->ToString() ),
					MySqlConnection.get() );

				// Execute the SQL query
				int NumAffectedRows = MySqlCommand->ExecuteNonQuery();

				// Make sure we successfully modified something!
				if( NumAffectedRows == 0 )
				{
					FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString(
						"GameAssetDatabase_ErrorExecutingSQLCommand_F",
						TEXT( "No rows were affected by SQL command" ) ) );	// Exception details
					CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::SendJournalEntry: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );

					return false;
				}
			}

			catch( Exception^ E )
			{
				FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString(
					"GameAssetDatabase_ErrorExecutingSQLCommand_F",
					E->ToString() ) );	// Exception details
				CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalClient::SendJournalEntry: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );

				return false;
			}
		}
	}
	else
	{
		// Make a BulkCopy so we can send many entries in one go.
		System::Data::SqlClient::SqlBulkCopy^ MyBulkCopy = gcnew System::Data::SqlClient::SqlBulkCopy( MySqlConnection.get(), SqlBulkCopyOptions::TableLock, nullptr );
		MyBulkCopy->BatchSize = EntriesDataTable->Rows->Count;
		MyBulkCopy->DestinationTableName = "dbo.Entries";
	
		try
		{
			MyBulkCopy->WriteToServer(EntriesDataTable);
		}
		catch (System::Exception^ ex)
		{
			CLRTools::LogWarningMessage( ex->ToString() );
		}
		finally
		{
			MyBulkCopy->Close();
		}
	}
	
	return true;
}



/** Constructor */
MGameAssetJournalFile::MGameAssetJournalFile()
{
}



/** Destructor */
MGameAssetJournalFile::~MGameAssetJournalFile()
{
}



/** Loads journal entries from the journal file */
bool MGameAssetJournalFile::QueryJournalEntries( [Out] MGameAssetJournalEntryList^% OutJournalEntries, bool bFilterBranchAndGame )
{
	OutJournalEntries = gcnew MGameAssetJournalEntryList();


	// Try to open the journal file
	String^ JournalFilePath = FGameAssetDatabase::MakeOfflineJournalFileName();
	auto_handle< StreamReader > JournalStreamReader;
	if ( File::Exists( JournalFilePath ) )
	{
		try
		{
			// Create the stream reader (which actually opens the file on disk.)
			JournalStreamReader.reset( gcnew StreamReader( JournalFilePath, gcnew Text::UnicodeEncoding() ) );
		}
		catch( FileNotFoundException^ )
		{
			// File didn't exist on disk.  No big deal.
			JournalStreamReader.reset();
		}
		catch( DirectoryNotFoundException^ )
		{
			// Directory didn't exist on disk.  No big deal.
			JournalStreamReader.reset();
		}
	}


	if( JournalStreamReader )
	{
		// Read file header/version info and verify everything
		int JournalFileVersion = INDEX_NONE;
		{
			// First line of the file is the file header (JOURNAL-<Ver#>)
			if( JournalStreamReader->Peek() >= 0 )
			{
				String^ FileHeaderString = JournalStreamReader->ReadLine();
				cli::array<String^>^ HeaderStrings = FileHeaderString->Split( TCHAR( '-' ), 2 );

				if( HeaderStrings->Length >= 2 )
				{
					if( HeaderStrings[ 0 ]->Equals( "JOURNAL" ) )
					{
						JournalFileVersion = Int32::Parse( HeaderStrings[ 1 ] );
					}
				}
			}
			
			if( JournalFileVersion == INDEX_NONE )
			{
				// Invalid journal file
				FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString( "GameAssetDatabase_InvalidJournalFile" ) );
				CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalFile::QueryJournalEntries: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
				return false;
			}

			
			if( JournalFileVersion > GADDefs::JournalFileVersionNumber )
			{
				// Ack!  We can't load a journal file that's newer than we're expecting
				throw gcnew SystemException( TEXT( "The journal file was created with a newer version of the application that is currently loaded." ) );
			}
		}


		// Build a list of all classes.  We need this for backwards compatibility stuff below.
		List<String^> AllClassNames;
		for( TObjectIterator<UClass> It; It; ++It )
		{
			// @todo CB: Some of these may not actually be browsable asset types, but they shouldn't
			//   have the same path as any asset so it won't matter in practice
			AllClassNames.Add( CLRTools::ToString( It->GetName() ) );
		}


		// Read the entire file
		int CurEntryIndex = 0;
		while( JournalStreamReader->Peek() >= 0 )
		{
			// Read journal entry string from the file
			String^ JournalEntryString = JournalStreamReader->ReadLine();

			// Parse the text
			MGameAssetJournalEntry^ NewJournalEntry = nullptr;
			if( CreateJournalEntryFromString( JournalEntryString, %AllClassNames, NewJournalEntry, bFilterBranchAndGame ) )
			{
				NewJournalEntry->IsOfflineEntry = true;

				// Use a current index as a "fake" database index so that sorting is consistent.  We never
				// delete journal entries when in offline mode so this is only used for sorting (when
				// time stamps are exactly equal.)
				NewJournalEntry->DatabaseIndex = CurEntryIndex;

				// Add to the list.  We'll sort them later.
				OutJournalEntries->Add( NewJournalEntry );
			}
			else
			{
				// Entry was for another game branch, or user.  Ignored!
			}

			++CurEntryIndex;
		}

	}

	return true;
}



/** Stores multiple journal entries in the journal file */
bool MGameAssetJournalFile::SendJournalEntries( List<MGameAssetJournalEntry^>^ JournalEntries )
{
	// Open the journal file.  If the journal file already exists then we'll append to that file,
	// otherwise a new journal file will be created here.  This is a unicode (UTF-16) file.
	String^ JournalFilePath = FGameAssetDatabase::MakeOfflineJournalFileName();
	auto_handle< StreamWriter > JournalStreamWriter;
	bool bIsNewJournalFile = false;
	try
	{
		// First check to see if the file exists so we can add a header if needed
		bIsNewJournalFile = !File::Exists( JournalFilePath );

		// Create the stream writer (which actually opens/creates the file on disk.)
		const bool bShouldAppendIfExists = true;
		JournalStreamWriter.reset( gcnew StreamWriter( JournalFilePath, bShouldAppendIfExists, gcnew Text::UnicodeEncoding() ) );

		// Enable this to flush text output after every single Write() call
		// JournalStreamWriter->AutoFlush = true;
	}
	catch( IOException^ )
	{
		// File isn't writable
		FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString( "GameAssetDatabase_JournalFileNotWritable" ) );
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalFile::SendJournalEntries: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
		return false;
	}
	catch( UnauthorizedAccessException^ )
	{
		// File isn't writable
		FGameAssetDatabase::Get().SetErrorMessageText( CLRTools::LocalizeString( "GameAssetDatabase_JournalFileNotWritable" ) );
		CLRTools::LogWarningMessage( String::Format( "MGameAssetJournalFile::SendJournalEntries: {0}", FGameAssetDatabase::Get().GetErrorMessageText() ) );
		return false;
	}


	// Write out a file header/version if we need to
	if( bIsNewJournalFile )
	{
		String^ HeaderString = String::Format( "JOURNAL-{0}", GADDefs::JournalFileVersionNumber );
		JournalStreamWriter->WriteLine( HeaderString );
	}


	// Populate the data table with Journal entries we want to send.
	for each ( MGameAssetJournalEntry^ JournalEntry in JournalEntries )
	{
		// Convert the journal entry to string format
		String^ JournalEntryString;
		if( CreateStringFromJournalEntry( JournalEntry, JournalEntryString ) )
		{
			// Write this entry to the file!
			JournalStreamWriter->WriteLine( JournalEntryString );
		}
		else
		{
			// Couldn't convert to string.  Reason is stored in ErrorMessageText
			return false;
		}
	}


	return true;
}



/** Deletes the journal file from disk */
bool MGameAssetJournalFile::DeleteJournalFile()
{
	// Try to open the journal file
	String^ JournalFilePath = FGameAssetDatabase::MakeOfflineJournalFileName();
	auto_handle< StreamReader > JournalStreamReader;
	try
	{
		if( File::Exists( JournalFilePath ) )
		{
			FFilename JournalFilePathAsFFilename = *CLRTools::ToFString(JournalFilePath);
			FFilename BackupJournalFilePathAsFFilename = JournalFilePathAsFFilename.GetBaseFilename(FALSE) + TEXT("_backup.") + JournalFilePathAsFFilename.GetExtension();

			//delete existing backup file
			String^ BackupJournalFilePath = CLRTools::ToString(BackupJournalFilePathAsFFilename);
			File::Delete( BackupJournalFilePath );

			//copy current journal to old journal slot
			File::Copy(JournalFilePath, BackupJournalFilePath);

			// Delete the file
			File::Delete( JournalFilePath );
		}
	}
	catch( IOException^ )
	{
		// File wasn't accessible for some reason.
		return false;
	}
	catch( UnauthorizedAccessException^ )
	{
		// File wasn't accessible for some reason.
		return false;
	}


	return true;
}



/**
 * Static: Allocates and initializes the game asset database
 *
 * @param	InConfig					Startup configuration
 * @param	OutInitErrorMessageText		On failure, a localized string containing an explanation of the problem
 */
void FGameAssetDatabase::Init( const FGameAssetDatabaseStartupConfig& InConfig, FString& OutInitErrorMessageText )
{
	OutInitErrorMessageText = TEXT( "" );


	// Database should not have already been initialized
	check( GameAssetDatabaseSingleton == NULL );

	// Allocate the database object
	GameAssetDatabaseSingleton = new FGameAssetDatabase();


	// Update the database.  If bShouldCheckpoint is set, we'll go ahead and update the checkpoint file
	// with all of the latest journal entries from the SQL server.
	bool bSuccess = Get().UpdateDatabase( InConfig );

	OutInitErrorMessageText = Get().GetErrorString();

	if( !bSuccess )
	{
		delete GameAssetDatabaseSingleton;
		GameAssetDatabaseSingleton = NULL;
	}
}



/**
 * Static: Shuts down and destroys the game asset database
 */
void FGameAssetDatabase::Destroy()
{
	if( GameAssetDatabaseSingleton != NULL )
	{
		delete GameAssetDatabaseSingleton;
		GameAssetDatabaseSingleton = NULL;
	}
}


/** Static: Checks to see if the journal file should prompt for checking for unverified assets*/
void FGameAssetDatabase::CheckJournalAlarm(void)
{
	UBOOL bInOfflineMode = GetInOfflineMode();
	if (bInOfflineMode)
	{
		UBOOL bForceJournal = TRUE;
		GConfig->GetBool(TEXT("GameAssetDatabase"), TEXT("ForceJournalUpdate"), bForceJournal, GEditorUserSettingsIni);
		if ( bForceJournal )
		{
			JournalUpdate();
			GConfig->SetBool(TEXT("GameAssetDatabase"), TEXT("ForceJournalUpdate"), FALSE, GEditorUserSettingsIni);
		}
		else
		{
			UBOOL bUseJournalAlarm = TRUE;
			GConfig->GetBool(TEXT("GameAssetDatabase"), TEXT("UseJournalUpdateAlarm"), bUseJournalAlarm, GEditorUserSettingsIni);
			if (bUseJournalAlarm)
			{
				//get snooze expire time from ini file or use now + 7 days
				DateTime JournalAlarmTime = GetJournalAlarmTime();

				//if we're past the Alarm Time
				if (DateTime::Now > JournalAlarmTime)
				{
					ShowJournalAlarmDialog();
				}
			}
		}
	}
}


/** Constructor */
FGameAssetDatabase::FGameAssetDatabase()
	: bIsJournalServerAvailable( FALSE ),
	  bInOfflineMode( FALSE )
{
#if HAVE_SCC
	// Needed to resolve GAD branch name if it doesn't exist in the INI.
	FSourceControl::Init();
#endif

	JournalClient = gcnew MGameAssetJournalClient();
	OfflineJournalFile = gcnew MGameAssetJournalFile();

	bInOfflineMode = GetInOfflineMode();

	// Initialize our asset path dictionary
	UniqueAssetPaths = gcnew RefCountedStringDictionary( StringComparer::OrdinalIgnoreCase );
}



/** Destructor */
FGameAssetDatabase::~FGameAssetDatabase()
{
}



/**
 * Returns true if the journal server is unavailable for some reason and tag writes should be disabled
 *
 * @return	True if database is in read-only mode
 */
bool FGameAssetDatabase::IsReadOnly() const
{
	return ( bIsJournalServerAvailable == FALSE && bInOfflineMode == FALSE && GIsUnitTesting == FALSE );
}



/**
 * Queries all tags
 *
 * @param	OutTags		List of all tags
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAllTags( TArray< FString >& OutTags ) const
{
	OutTags.Reset();

	for( FNameSet::TConstIterator CurTagIt( KnownTags ); CurTagIt; ++CurTagIt )
	{
		const FName& CurTag = *CurTagIt;
		OutTags.AddItem( CurTag.ToString() );
	}
}
#pragma managed



/**
 * Queries all tags
 *
 * @param	OutTags		List of all tags
 * @param	InOptions	Types of tags to query
 */
void FGameAssetDatabase::QueryAllTags( [Out] List< String^ >^% OutTags, ETagQueryOptions::Type InOptions ) const
{
	OutTags = gcnew List< String^ >();

	for( FNameSet::TConstIterator CurTagIt( KnownTags ); CurTagIt; ++CurTagIt )
	{
		String^ CurTag = CLRTools::FNameToString( *CurTagIt );
		
		bool bIncludeTagInQuery = true;

		if( InOptions != ETagQueryOptions::AllTags )
		{
			ESystemTagType TagType = GetSystemTagType( CurTag );
			bool bIsSystemTag = TagType != ESystemTagType::Invalid;

			switch( InOptions )
			{
				case ETagQueryOptions::SystemTagsOnly:
					if( !bIsSystemTag )
					{
						bIncludeTagInQuery = false;
					}
					break;

				case ETagQueryOptions::UserTagsOnly:
					if( bIsSystemTag )
					{
						bIncludeTagInQuery = false;
					}
					break;

				case ETagQueryOptions::CollectionsOnly:
					if( TagType != ESystemTagType::PrivateCollection && TagType != ESystemTagType::SharedCollection && TagType != ESystemTagType::LocalCollection )
					{
						bIncludeTagInQuery = false;
					}
					break;
			}
		}

		if( bIncludeTagInQuery )
		{
			OutTags->Add( CurTag );
		}
	}
}



/**
 * Queries all asset full names (warning: this can potentially be a LOT of data!)
 *
 * @param	OutAssetFullNames		List of all asset full names
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAllAssets( TArray< FString >& OutAssetFullNames ) const
{
	OutAssetFullNames.Reset();

	TLookupMap< FName > AssetFullNames;
	AssetToTagMap.GetKeys( AssetFullNames );	// Out

	for( TLookupMap< FName >::TConstIterator CurAssetIt( AssetFullNames ); CurAssetIt; ++CurAssetIt )
	{
		const FName& CurAsset = CurAssetIt.Key();
		OutAssetFullNames.AddItem( CurAsset.ToString() );
	}
}
#pragma managed



/**
 * Queries all asset full names (warning: this can potentially be a LOT of data!)
 *
 * @param	OutAssetFullNameFNames		List of all asset full names
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAllAssets( TLookupMap< FName >& OutAssetFullNameFNames ) const
{
	OutAssetFullNameFNames.Empty( AssetToTagMap.NumKeys() );
	AssetToTagMap.GetKeys( OutAssetFullNameFNames );	// Out
}
#pragma managed



/**
 * Queries all asset full names (warning: this can potentially be a LOT of data!)
 *
 * @param	OutAssetFullNames		List of all asset full names
 */
void FGameAssetDatabase::QueryAllAssets( [Out] List< String^ >^% OutAssetFullNames ) const
{
	OutAssetFullNames = gcnew List< String^ >();

	TLookupMap< FName > AssetFullNames;
	AssetToTagMap.GetKeys( AssetFullNames );	// Out

	for( TLookupMap< FName >::TConstIterator CurAssetIt( AssetFullNames ); CurAssetIt; ++CurAssetIt )
	{
		const FName& CurAsset = CurAssetIt.Key();
		OutAssetFullNames->Add( CLRTools::FNameToString( CurAsset ) );
	}
}



/**
 * Checks to see if the specified asset is in the database
 *
 * @param	InAssetFullName		Full name of the asset
 *
 * @return	True if the asset is known to the database
 */
UBOOL FGameAssetDatabase::IsAssetKnown( const FString& InAssetFullName ) const
{
	return AssetToTagMap.ContainsKey( FName( *InAssetFullName ) );
}



/**
* Associates a tag with a group of assets
*
* @param	InAssetFullNames	Full names of the assets to tag
* @param	InTag				Tag to assign
*
* @return	True if successful
*/
bool FGameAssetDatabase::AddTagToAssets( Generic::ICollection<String^>^ InAssetFullNames, String^ InTag )
{
	// @todo CB: Check for not-allowed characters! (|, [, ])

	// Certain system tags are never expected to be uploaded as a journal entry (they're only applied
	// through the commandlet process.)
	ESystemTagType SystemTagType = GetSystemTagType( InTag );
	if( SystemTagType != ESystemTagType::Invalid )
	{
		check( SystemTagType != ESystemTagType::Invalid );
		check( SystemTagType != ESystemTagType::Ghost );
		check( SystemTagType != ESystemTagType::Unverified );
		check( SystemTagType != ESystemTagType::DateAdded );
	}

	if ( IsReadOnly() )
	{
		SetErrorMessageText( CLRTools::LocalizeString(
				"GameAssetDatabase_CannotWriteToReadOnlyDatabase" ) );	// Exception details
		CLRTools::LogWarningMessage( String::Format( "FGameAssetDatabase::AddTagToAsset: {0}", GetErrorMessageText() ) );
	}
	else
	{

		// Build a list of Entries representing tag additions
		List<MGameAssetJournalEntry^>^ Entries = gcnew List<MGameAssetJournalEntry^>();
		for each (String^ AssetFullName in InAssetFullNames)
		{
			if ( CLRTools::IsAssetValidForTagging(AssetFullName) )
			{
				// If this is a user tag or collection tag, make sure the asset is known to the database first
				bool bOnlyKnownAssets = ( SystemTagType == ESystemTagType::Invalid || SystemTagType == ESystemTagType::PrivateCollection || SystemTagType == ESystemTagType::SharedCollection );
				if( !bOnlyKnownAssets || IsAssetKnown( CLRTools::ToFString( AssetFullName ) ) )
				{
					MGameAssetJournalEntry^ JournalEntry = MGameAssetJournalEntry::MakeAddTagToAssetEntry(AssetFullName, InTag);
					Entries->Add(JournalEntry);
				}
				else if (bOnlyKnownAssets)
				{
					// This tag should only be applied to known assets, but the current asset isn't known
					CLRTools::LogWarningMessage(String::Format(
						"Asset '{0}' is not known to the Game Asset Database and will not be tagged with '{1}'",
						AssetFullName,
						InTag));
				}
			}
		}

		if( Entries->Count > 0 )
		{
			// Send the Entries to the Journal DB. If the send succeeded ...
			if( ( bIsJournalServerAvailable && JournalClient->SendJournalEntries( Entries ) ) ||
				( bInOfflineMode && OfflineJournalFile->SendJournalEntries( Entries ) ) )
			{
				// ... update local tables.
				for each (MGameAssetJournalEntry^ CurEntry in Entries)
				{
					// Assets added by tagging through the editor are not authoritative (the asset may not even
					// be loaded so we don't know if it really exists on disk.)
					bool bIsAuthoritative = false;
					AddTagMapping( CurEntry->AssetFullName, InTag, bIsAuthoritative );
				}
			}
			else
			{
				//@todo cb [reviewed; discuss]: log warning
				return false;
			}
		}
	}


	return true;
}


/**
 * Removes a tag from an asset
 *
 * @param	InAssetFullNames	Full names of the asset from which to remove tags
 * @param	InTag				Tag to remove
 *
 * @return	True if successful
 */
bool FGameAssetDatabase::RemoveTagFromAssets( Generic::ICollection<String^>^ InAssetFullNames, String^ InTag )
{
	// Certain system tags are never expected to be uploaded as a journal entry (they're only applied
	// through the commandlet process.)
	ESystemTagType SystemTagType = GetSystemTagType( InTag );
	if( SystemTagType != ESystemTagType::Invalid )
	{
		check( SystemTagType != ESystemTagType::Invalid );
		check( SystemTagType != ESystemTagType::Ghost );
		check( SystemTagType != ESystemTagType::Unverified );
		check( SystemTagType != ESystemTagType::DateAdded );
	}

	if ( IsReadOnly() )
	{
		SetErrorMessageText( CLRTools::LocalizeString(
				"GameAssetDatabase_CannotWriteToReadOnlyDatabase" ) );	// Exception details
		CLRTools::LogWarningMessage( String::Format( "FGameAssetDatabase::AddTagToAsset: {0}", GetErrorMessageText() ) );

		return false;
	}
	else
	{

		// Build a list of Entries representing tag removals
		List<MGameAssetJournalEntry^>^ Entries = gcnew List<MGameAssetJournalEntry^>();
		for each ( String^ AssetFullName in InAssetFullNames )
		{
			if ( CLRTools::IsAssetValidForTagging(AssetFullName) )
			{
				// If this is a user tag or collection tag, make sure the asset is known to the database first
				bool bOnlyKnownAssets = ( SystemTagType == ESystemTagType::Invalid || SystemTagType == ESystemTagType::PrivateCollection || SystemTagType == ESystemTagType::SharedCollection );
				if( !bOnlyKnownAssets || IsAssetKnown( CLRTools::ToFString( AssetFullName ) ) )
				{
					MGameAssetJournalEntry^ JournalEntry = MGameAssetJournalEntry::MakeRemoveTagFromAssetEntry(AssetFullName, InTag);
					Entries->Add( JournalEntry );
				}
			}
		}

		if( Entries->Count > 0 )
		{
			// Send the removal entries to the Journal DB. If the send succeeds ...
			if( ( bIsJournalServerAvailable && JournalClient->SendJournalEntries( Entries ) ) ||
				( bInOfflineMode && OfflineJournalFile->SendJournalEntries( Entries ) ) )
			{
				// ... update local tables
				for each ( String^ AssetFullName in InAssetFullNames )
				{
					RemoveTagMapping( AssetFullName, InTag );
				}			
			}
			else
			{
				// Failed to send journal entry to server.  ErrorMessageText will contain the reason.
				return false;
			}
		}
	}


	return true;
}



/**
 * Finds the tags associated with the specified asset
 *
 * @param	InAssetFullName	Full name of the asset
 * @param	OutTags			[Out] List of tags found
 */
#pragma unmanaged
void FGameAssetDatabase::QueryTagsForAsset( const FString& InAssetFullName, TArray< FString >& OutTags ) const
{
	OutTags.Reset();

	// @todo CB: Add support for ETagQueryOptions for this path!

	TArray< FName > FoundTags;
	AssetToTagMap.FindValuesForKey( FName( *InAssetFullName ), FoundTags );
	for( INT CurTagIndex = 0; CurTagIndex < FoundTags.Num(); ++CurTagIndex )
	{
		OutTags.AddItem( FoundTags( CurTagIndex ).ToString() );
	}
}
#pragma managed




/**
 * Finds the tags associated with the specified asset
 *
 * @param	InAssetFullNameFName	Full name of the asset
 * @param	OutTags			[Out] List of tags found
 */
#pragma unmanaged
void FGameAssetDatabase::QueryTagsForAsset( const FName InAssetFullNameFName, const ETagQueryOptions::Type InOptions, TArray< FString >& OutTags ) const
{
	OutTags.Reset();

	static TArray< FName > FoundTags;
	FoundTags.Reset();
	AssetToTagMap.FindValuesForKey( InAssetFullNameFName, FoundTags );

	for( INT CurTagIndex = 0; CurTagIndex < FoundTags.Num(); ++CurTagIndex )
	{
		FString CurTagName;
		FoundTags( CurTagIndex ).ToString( CurTagName );	// Out

		bool bIncludeTagInQuery = true;

		if( InOptions != ETagQueryOptions::AllTags )
		{
			// @todo CB: Not supported with this (unmanaged) code path yet
			check( InOptions != ETagQueryOptions::CollectionsOnly );

			bool bIsSystemTag = IsSystemTag( CurTagName );
			switch( InOptions )
			{
				case ETagQueryOptions::SystemTagsOnly:
					if( !bIsSystemTag )
					{
						bIncludeTagInQuery = false;
					}
					break;

				case ETagQueryOptions::UserTagsOnly:
					if( bIsSystemTag )
					{
						bIncludeTagInQuery = false;
					}
					break;
			}
		}

		if( bIncludeTagInQuery )
		{
			OutTags.AddItem( CurTagName );
		}
	}
}
#pragma managed


/**
 * Create a new (persistent) tag.
 *
 * @param InTag   The tag to create.
 */
UBOOL FGameAssetDatabase::CreateTag( const FString& InTag )
{
	return this->CreateTag( CLRTools::ToString(InTag) );
}

/**
 * Destroy a tag. Also untags all assets tagged with this tag.
 *
 * @param InTag   The tag to destroy.
 */
UBOOL FGameAssetDatabase::DestroyTag( const FString& InTag )
{
	return this->DestroyTag( CLRTools::ToString(InTag) );
}

/**
 * Removes a tag from an asset
 *
 * @param  InAssetFullNames Full names of the asset from which to remove tags
 * @param  InTag            Tag to remove
 *
 * @return True if successful
 */
UBOOL FGameAssetDatabase::RemoveTagFromAssets( const TArray<FString>& InAssetFullNames, const FString& InTag )
{
	return this->RemoveTagFromAssets( CLRTools::ToStringArray(InAssetFullNames), CLRTools::ToString(InTag) );
}

/**
 * Associates a tag with a group of assets
 *
 * @param   InAssetFullNames    Full names of the assets to tag
 * @param   InTag               Tag to assign
 *
 * @return  True if successful
 */
UBOOL FGameAssetDatabase::AddTagToAssets( const TArray<FString>& InAssetFullNames, const FString& InTag )
{
	return this->AddTagToAssets( CLRTools::ToStringArray(InAssetFullNames), CLRTools::ToString(InTag) );
}


/**
 * Finds the tags associated with the specified asset
 *
 * @param	InAssetFullName	Full name of the asset
 * @param	InOptions		Types of tags to query
 * @param	OutTags			[Out] List of tags found
 */
void FGameAssetDatabase::QueryTagsForAsset( String^ InAssetFullName, ETagQueryOptions::Type InOptions, [Out] List< String^ >^% OutTags ) const
{
	QueryTagsForAsset( CLRTools::ToFName( InAssetFullName ), InOptions, OutTags );
}




/**
 * Finds the tags associated with the specified asset
 *
 * @param	InAssetFullNameFName	Full name of the asset
 * @param	InOptions		Types of tags to query
 * @param	OutTags			[Out] List of tags found
 */
void FGameAssetDatabase::QueryTagsForAsset( FName InAssetFullNameFName, ETagQueryOptions::Type InOptions, [Out] List< String^ >^% OutTags ) const
{
	OutTags = gcnew List< String^ >();

	TArray< FName > FoundTags;
	AssetToTagMap.FindValuesForKey( InAssetFullNameFName, FoundTags );

	for( INT CurTagIndex = 0; CurTagIndex < FoundTags.Num(); ++CurTagIndex )
	{
		String^ CurTag = CLRTools::FNameToString( FoundTags( CurTagIndex ) );
		
		bool bIncludeTagInQuery = true;

		if( InOptions != ETagQueryOptions::AllTags )
		{
			ESystemTagType TagType = GetSystemTagType( CurTag );
			bool bIsSystemTag = TagType != ESystemTagType::Invalid;

			switch( InOptions )
			{
				case ETagQueryOptions::SystemTagsOnly:
					if( !bIsSystemTag )
					{
						bIncludeTagInQuery = false;
					}
					break;

				case ETagQueryOptions::UserTagsOnly:
					if( bIsSystemTag )
					{
						bIncludeTagInQuery = false;
					}
					break;

				case ETagQueryOptions::CollectionsOnly:
					if( TagType != ESystemTagType::PrivateCollection && TagType != ESystemTagType::SharedCollection )
					{
						bIncludeTagInQuery = false;
					}
					break;
			}
		}

		if( bIncludeTagInQuery )
		{
			OutTags->Add( CurTag );
		}
	}
}



/**
 * Finds all of the assets with the specified tag
 *
 * @param	InTag				Tag
 * @param	OutAssetFullNames	[Out] List of asset full names found
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAssetsWithTag( const FString& InTag, TArray< FString >& OutAssetFullNames ) const
{
	OutAssetFullNames.Reset();

	TArray< FName > FoundAssetFullNames;
	TagToAssetMap.FindValuesForKey( FName( *InTag ), FoundAssetFullNames );
	for( INT CurAssetIndex = 0; CurAssetIndex < FoundAssetFullNames.Num(); ++CurAssetIndex )
	{
		OutAssetFullNames.AddItem( FoundAssetFullNames( CurAssetIndex ).ToString() );
	}
}
#pragma managed



/**
 * Finds all of the assets with the specified tag
 *
 * @param	InTag				Tag
 * @param	OutAssetFullNames	[Out] List of asset full names found
 */
void FGameAssetDatabase::QueryAssetsWithTag( String^ InTag, [Out] List< String^ >^% OutAssetFullNames ) const
{
	OutAssetFullNames = gcnew List< String^ >();

	TArray< FName > FoundAssetFullNames;
	TagToAssetMap.FindValuesForKey( FName( *CLRTools::ToFString( InTag ) ), FoundAssetFullNames );
	for( INT CurAssetIndex = 0; CurAssetIndex < FoundAssetFullNames.Num(); ++CurAssetIndex )
	{
		OutAssetFullNames->Add( CLRTools::ToString( FoundAssetFullNames( CurAssetIndex ).ToString() ) );
	}
}



/**
 * Finds all of the assets with all of the specified set of tags, and any of another set of tags
 *
 * @param	InAllTags			An asset must have all of these tags assigned to be returned
 * @param	InAnyTags			An asset may have any of these tags to be returned
 * @param	OutAssetFullNames	[Out] List of asset full names found
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAssetsWithTags( const TArray< FString >& InAllTags, const TArray< FString >& InAnyTags, TArray< FString >& OutAssetFullNames ) const
{
	OutAssetFullNames.Reset();

	TLookupMap< FName > AssetFullNameFNames;
	QueryAssetsWithTags( InAllTags, InAnyTags, AssetFullNameFNames );

	// Covert the TLookupMap of FNames to an array of FStrings
	const TArray< FName >& AssetFullNameFNameArray = AssetFullNameFNames.GetUniqueElements();
	OutAssetFullNames.Empty( AssetFullNameFNameArray.Num() );
	for( INT CurAssetIndex = 0; CurAssetIndex < AssetFullNameFNameArray.Num(); ++CurAssetIndex )
	{
		OutAssetFullNames.AddItem( AssetFullNameFNameArray( CurAssetIndex ).ToString() );
	}
}
#pragma managed




/**
 * Finds all of the assets with all of the specified set of tags, and any of another set of tags
 *
 * @param	InAllTags		An asset must have all of these tags assigned to be returned
 * @param	InAnyTags		An asset may have any of these tags to be returned
 * @param	OutAssetFullNameFNames	[Out] List of asset full names found
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAssetsWithTags( const TArray< FString >& InAllTags, const TArray< FString >& InAnyTags, TLookupMap< FName >& OutAssetFullNameFNames ) const
{
	OutAssetFullNameFNames.Reset();


	// If no tags were passed for the "any tags" list, then we'll default to ALL possible assets
	if( InAnyTags.Num() == 0 )
	{
		QueryAllAssets( OutAssetFullNameFNames );
	}
	else
	{
		// Case-insensitive set of asset full names
		TSet< FName > AssetFullNamesSet;

		// First gather all assets that have any tag assigned in our "any tags" list
		for( INT CurTagIndex = 0; CurTagIndex < InAnyTags.Num(); ++CurTagIndex )
		{
			TArray< FName > FoundAssetFullNames;
			TagToAssetMap.FindValuesForKey( FName( *InAnyTags( CurTagIndex ) ), FoundAssetFullNames );
			for( INT CurAssetIndex = 0; CurAssetIndex < FoundAssetFullNames.Num(); ++CurAssetIndex )
			{
				AssetFullNamesSet.Add( FoundAssetFullNames( CurAssetIndex ) );
			}
		}

		// Convert to flat list while culling out assets
		for( TSet< FName >::TConstIterator It( AssetFullNamesSet ); It; ++It ) 
		{
			const FName& CurAssetFullName = *It;

			// First make sure that the asset contains all of the required tags (the "all tags" list)
			bool bHasAllTags = true;
			for( INT CurTagIndex = 0; CurTagIndex < InAllTags.Num(); ++CurTagIndex )
			{
				const FName CurTag( *InAllTags( CurTagIndex ) );
				if( !AssetToTagMap.Contains( CurAssetFullName, CurTag ) )
				{
					bHasAllTags = false;
					break;
				}
			}

			if( bHasAllTags )
			{
				OutAssetFullNameFNames.AddItem( CurAssetFullName );
			}
		}
	}
}
#pragma managed



/**
 * Finds all of the assets with at least one tag in each of the specified tag sets
 *
 * @param	InTagSetList		List of tag sets.  Returned assets must have at least one tag in each set.
 * @param	OutAssetFullNames	[Out] List of assets found (full names)
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAssetsWithTagInAllSets( const TArray< TArray< FString >* >& InTagSetList, TArray< FString >& OutAssetFullNames ) const
{
	TLookupMap< FName > AssetFullNameFNames;
	QueryAssetsWithTagInAllSets( InTagSetList, AssetFullNameFNames );

	// Covert the TLookupMap of FNames to an array of FStrings
	const TArray< FName >& AssetFullNameFNameArray = AssetFullNameFNames.GetUniqueElements();
	OutAssetFullNames.Empty( AssetFullNameFNameArray.Num() );
	for( INT CurAssetIndex = 0; CurAssetIndex < AssetFullNameFNameArray.Num(); ++CurAssetIndex )
	{
		OutAssetFullNames.AddItem( AssetFullNameFNameArray( CurAssetIndex ).ToString() );
	}
}
#pragma managed




/**
 * Finds all of the assets with at least one tag in each of the specified tag sets
 *
 * @param	InTagSetList	List of tag sets.  Returned assets must have at least one tag in each set.
 * @param	OutAssetFNames	[Out] List of assets found (full names)
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAssetsWithTagInAllSets( const TArray< TArray< FString >* >& InTagSetList, TLookupMap< FName >& OutAssetFullNameFNames ) const
{
	OutAssetFullNameFNames.Reset();


	// If no tags were passed in, then we'll default to ALL possible assets
	if( InTagSetList.Num() == 0 )
	{
		QueryAllAssets( OutAssetFullNameFNames );
	}
	else
	{
		// Keep track of all assets that associated with ANY tag in ANY of the tag sets
		TSet< FName > AllAssetFullNames;

		// Also keep track of the assets that are associated with ANY tag in EACH tag set
		TArray< TSet< FName >* > TagSetAssetFullNames;

		for( INT CurSetIndex = 0; CurSetIndex < InTagSetList.Num(); ++CurSetIndex )
		{
			TagSetAssetFullNames.AddItem( new TSet< FName >() );

			check( InTagSetList( CurSetIndex ) != NULL );
			const TArray< FString >& CurTagSet = *InTagSetList( CurSetIndex );

			// First gather all assets that have any tag assigned in our "any tags" list
			for( INT CurTagIndex = 0; CurTagIndex < CurTagSet.Num(); ++CurTagIndex )
			{
				TArray< FName > FoundAssetFullNames;
				TagToAssetMap.FindValuesForKey( FName( *CurTagSet( CurTagIndex ) ), FoundAssetFullNames );
				for( INT CurAssetIndex = 0; CurAssetIndex < FoundAssetFullNames.Num(); ++CurAssetIndex )
				{
					const FName& CurAssetFullName = FoundAssetFullNames( CurAssetIndex );
					AllAssetFullNames.Add( CurAssetFullName );
					TagSetAssetFullNames( CurSetIndex )->Add( CurAssetFullName );
				}
			}
		}


		// Now, we'll weed out any tags that aren't associated with at least one tag in EACH set
		for( TSet< FName >::TConstIterator It( AllAssetFullNames ); It; ++It ) 
		{
			const FName& CurAssetFullName = *It;

			UBOOL bIsInAllSets = TRUE;

			// No point wasting time checking against other tag sets when there is only one set!
			if( InTagSetList.Num() > 1 )
			{
				for( INT CurSetIndex = 0; CurSetIndex < InTagSetList.Num(); ++CurSetIndex )
				{
					const TArray< FString >& CurTagSet = *InTagSetList( CurSetIndex );

					// Ignore sets with no tags in them, we won't filter based on those
					if( CurTagSet.Num() > 0 )
					{
						if( !TagSetAssetFullNames( CurSetIndex )->Contains( CurAssetFullName ) )
						{
							bIsInAllSets = FALSE;
							break;
						}
					}
				}
			}

			if( bIsInAllSets )
			{
				OutAssetFullNameFNames.AddItem( CurAssetFullName );
			}
		}


		// Clean up time!
		for( INT CurSetIndex = 0; CurSetIndex < InTagSetList.Num(); ++CurSetIndex )
		{
			delete TagSetAssetFullNames( CurSetIndex );
		}
	}
}
#pragma managed



/**
 * Finds all of the assets with all of the specified set of tags, and any of another set of tags
 *
 * @param	InAllTags			An asset must have all of these tags assigned to be returned
 * @param	InAnyTags			An asset may have any of these tags to be returned
 * @param	OutAssetFullNames	[Out] List of asset full names found
 */
void FGameAssetDatabase::QueryAssetsWithTags( Generic::ICollection< String^ >^ InAllTags, Generic::ICollection< String^ >^ InAnyTags, [Out] List< String^ >^% OutAssetFullNames ) const
{
	OutAssetFullNames = gcnew List< String^ >();

	TArray< FString > AllTags;
	CLRTools::ToFStringArray( InAllTags, AllTags );

	TArray< FString > AnyTags;
	CLRTools::ToFStringArray( InAnyTags, AnyTags );

	TArray< FString > AssetFullNames;
	QueryAssetsWithTags( AllTags, AnyTags, AssetFullNames );

	OutAssetFullNames = CLRTools::ToStringArray( AssetFullNames );
}




/**
 * Finds all of the assets with all of the specified tags
 *
 * @param	InAllTags			List of tags.  For an asset to be returned, it must be associated with all of these tags.
 * @param	OutAssetFullNames	[Out] List of asset full names found
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAssetsWithAllTags( const TArray< FString >& InAllTags, TArray< FString >& OutAssetFullNames ) const
{
	// QueryAssetsWithTags gets assts with ANY tags first and then checks the set of assets with ANY tags for ALL tags
	// so any and all must be identical.
	TArray< FString > InAnyTags = InAllTags;
	QueryAssetsWithTags( InAllTags, InAnyTags, OutAssetFullNames );
}
#pragma managed




/**
 * Finds all of the assets with all of the specified tags
 *
 * @param	InAllTags			List of tags.  For an asset to be returned, it must be associated with all of these tags.
 * @param	OutAssetFullNames	[Out] List of asset full names found
 */
void FGameAssetDatabase::QueryAssetsWithAllTags( List< String^ >^ InAllTags, [Out] List< String^ >^% OutAssetFullNames ) const
{
	List< String^ >^ InAnyTags = gcnew List< String^ >( InAllTags );

	QueryAssetsWithTags( InAllTags, InAnyTags, OutAssetFullNames );
}




/**
 * Finds all of the assets with any of the specified tags
 *
 * @param	InTags				List of tags.  For an asset to be returned, it must be associated with any of these tags.
 * @param	OutAssetFullNames	[Out] List of assets found
 */
#pragma unmanaged
void FGameAssetDatabase::QueryAssetsWithAnyTags( const TArray< FString >& InTags, TArray< FString >& OutAssetFullNames ) const
{
	TArray< FString > DummyArray;
	QueryAssetsWithTags( DummyArray, InTags, OutAssetFullNames );
}
#pragma managed




/**
 * Finds all of the assets with any of the specified tags
 *
 * @param	InTags				List of tags.  For an asset to be returned, it must be associated with any of these tags.
 * @param	OutAssetFullNames	[Out] List of assets found
 */
void FGameAssetDatabase::QueryAssetsWithAnyTags( Generic::ICollection< String^ >^ InTags, [Out] List< String^ >^% OutAssetFullNames ) const
{
	List< String^ >^ DummyArray = gcnew List< String^ >();
	QueryAssetsWithTags( DummyArray, InTags, OutAssetFullNames );
}



/**
 * Create a new (persistent) tag.
 *
 * @param InTag   The tag to create.
 */
bool FGameAssetDatabase::CreateTag( System::String^ InTag )
{
	// @todo CB: Validate collection name?

	if ( IsReadOnly() )
	{
		SetErrorMessageText( CLRTools::LocalizeString(
				"GameAssetDatabase_CannotWriteToReadOnlyDatabase" ) );	// Exception details
		CLRTools::LogWarningMessage( String::Format( "FGameAssetDatabase::CreateTag: {0}", GetErrorMessageText() ) );

		return false;
	}
	else
	{
		// Make sure the tag doesn't already exist
		if( !KnownTags.Contains( CLRTools::ToFName( InTag ) ) )
		{
			// Update the journal
			MGameAssetJournalEntry^ JournalEntry = MGameAssetJournalEntry::MakeCreateTagEntry( InTag );
			
			if( ( bIsJournalServerAvailable && JournalClient->SendJournalEntry( JournalEntry ) ) ||
				( bInOfflineMode && OfflineJournalFile->SendJournalEntry( JournalEntry ) ) )
			{
				// Update local collections
				LocalCreateTag( InTag );
			}
			else
			{
				// Failed to send journal entry to server.  ErrorMessageText will contain the reason.
				return false;
			}

		}
	}

	return true;	
}


/**
 * Destroy a tag. Also untags all assets tagged with this tag.
 *
 * @param InTag   The tag to destroy.
 */
bool FGameAssetDatabase::DestroyTag( System::String^ InTag )
{
	if ( IsReadOnly() )
	{
		SetErrorMessageText( CLRTools::LocalizeString(
				"GameAssetDatabase_CannotWriteToReadOnlyDatabase" ) );	// Exception details
		CLRTools::LogWarningMessage( String::Format( "FGameAssetDatabase::DestroyTag: {0}", GetErrorMessageText() ) );
		
		return false;
	}
	else
	{
		// Make sure the tag exists first
		if( KnownTags.Contains( CLRTools::ToFName( InTag ) ) )
		{
			// Update the journal
			MGameAssetJournalEntry^ JournalEntry = MGameAssetJournalEntry::MakeDestroyTagEntry( InTag );

			if( ( bIsJournalServerAvailable && JournalClient->SendJournalEntry( JournalEntry ) ) ||
				( bInOfflineMode && OfflineJournalFile->SendJournalEntry( JournalEntry ) ) )
			{
				// Update local collections		
				LocalDestroyTag( InTag );
			}
			else
			{
				// Failed to send journal entry to server.  ErrorMessageText will contain the reason.
				return false;
			}
		}
	}

	return true;	
}


/**
* Copies (or renames/moves) a tag
* 
* @param InCurrentTagName The tag to rename
* @param InNewTagName The new tag name
* @param bInMove True if the old tag should be destroyed after copying it
*
* @return TRUE is succeeded, FALSE if failed.
*/
bool FGameAssetDatabase::CopyTag( System::String^ InCurrentTagName, System::String^ InNewTagName, bool bInMove )
{
	// Note: This is a composite operation. Ideally we would ensure that it is atomic.
	// That is, if deleting the tag fails then we should should roll back the new tag creation.
	// This case is harmless though, as the user will just see a new tag and an old tag.
	// However, a more sinister scenario is:
	// (1)created tag (2) failed to copy (3) user saw new tag and deleted the old one.


	// Make sure the source and destination collection names aren't the same!
	if( InCurrentTagName->Equals( InNewTagName, StringComparison::OrdinalIgnoreCase ) )
	{
		throw gcnew SystemException( TEXT( "Incoming tag names must not be the same!" ) );
	}

	// Grab all of the assets with this tag
	List< String^ >^ AssetsToTag;
	QueryAssetsWithTag( InCurrentTagName, AssetsToTag );


	// Create the new collection.  Note that it's fine if the destination tag already exists.  The
	// tag creation code will check for that.
	bool bCreatedNewTag = CreateTag( InNewTagName );
	if( !bCreatedNewTag )
	{
		// Failed to create new collection, so bail
		return false;
	}

	// Any assets to copy over?
	if( AssetsToTag->Count > 0 )
	{
		// Add all of the assets assigned to the current collection to the new collection
		bool bCopiedAssets = AddTagToAssets( AssetsToTag, InNewTagName );
		if( !bCopiedAssets )
		{
			// Failed to copy assets
			return false;
		}
	}


	// Were we asked to perform a 'move'?  If so we'll destroy the old collection now.
	if( bInMove )
	{
		// Destroy the old collection.  This will implicitly remove all of the assets from the collection.
		bool bDestroyedOldTag = DestroyTag( InCurrentTagName );
		if( !bDestroyedOldTag )
		{
			// Couldn't destroy the old collection
			return false;
		}
	}


	// Done!
	return true;
}



/**
 * Create a new collection into which assets can be added.
 * The collection name must be unique.
 *
 * @param InCollectionName Name of collection to create 
 * @param InType Type of collection to create
 * 
 * @return True if collection was created successfully
 */
bool FGameAssetDatabase::CreateCollection( System::String^ InCollectionName, EGADCollection::Type InType )
{
	// @todo CB: Validate collection name?

	ESystemTagType CollectionTagType = GADCollectionTypeToSystemTag( InType );

	bool Result = false;
	if ( IsReadOnly() || CollectionTagType == ESystemTagType::LocalCollection )
	{
		// If this is a local collection type, locally create the tag and do not update the database.
		LocalCreateTag( MakeSystemTag( ESystemTagType::LocalCollection, InCollectionName ) );
		Result = true;
	}
	else
	{
		Result = CreateTag( MakeSystemTag( CollectionTagType, InCollectionName ) );
	}
	return Result;
}


/**
 * Create a new collection into which assets can be added.
 * The collection name must be unique.
 *
 * @param InCollectionName Name of collection to create 
 * @param InType Type of collection to create
 * 
 * @return True if collection was created successfully
 */
UBOOL FGameAssetDatabase::CreateCollection( const FString& InCollectionName, const EGADCollection::Type InType )
{
	return CreateCollection( CLRTools::ToString( InCollectionName ), InType );
}


/**
 * Destroy a collection.
 * 
 * @param InCollectionName The name of collection to remove
 * @param InType Type of collection to destroy
 *
 * @return True if collection was destroyed successfully.
 */
bool FGameAssetDatabase::DestroyCollection( System::String^ InCollectionName, EGADCollection::Type InType )
{
	ESystemTagType CollectionTagType = GADCollectionTypeToSystemTag( InType );

	bool Result = false;
	if ( IsReadOnly() || CollectionTagType == ESystemTagType::LocalCollection )
	{
		// If this is a local collection type, locally destroy the tag and do not update the database.
		LocalDestroyTag( MakeSystemTag( ESystemTagType::LocalCollection, InCollectionName ) );
		Result = true;
	}
	else
	{
		Result = DestroyTag( MakeSystemTag( CollectionTagType, InCollectionName ) );
	}
	return Result;
}



/**
 * Destroy a collection.
 * 
 * @param InCollectionName The name of collection to remove
 * @param InType Type of collection to destroy
 *
 * @return True if collection was destroyed successfully.
 */
UBOOL FGameAssetDatabase::DestroyCollection( const FString& InCollectionName, const EGADCollection::Type InType )
{
	return DestroyCollection( CLRTools::ToString( InCollectionName ), InType );
}



/**
 * Copies (or renames/moves) a collection
 * 
 * @param InCurrentCollectionName The collection to rename
 * @param InCurrentType The type of collection specified by InCurrentCollectionName
 * @param InNewCollectionName The new name of the collection
 * @param InNewType The type of collection specified by InNewCollectionName
 * @param bInMove True if the old collection should be destroyed after copying it
 *
 * @return TRUE is succeeded, FALSE if failed.
 */
bool FGameAssetDatabase::CopyCollection( String^ InCurrentCollectionName, EGADCollection::Type InCurrentType, String^ InNewCollectionName, EGADCollection::Type InNewType, bool bInMove )
{
	// Make sure the source and destination collection names aren't the same!
	if( InCurrentType == InNewType && InCurrentCollectionName->Equals( InNewCollectionName, StringComparison::OrdinalIgnoreCase ) )
	{
		throw gcnew SystemException( TEXT( "Incoming collection names must not be the same!" ) );
	}

	// Grab all of the assets that are in the current collection
	List< String^ >^ AssetsInCollection;
	QueryAssetsInCollection( InCurrentCollectionName, InCurrentType, AssetsInCollection );


	// Create the new collection.  Note that it's fine if the destination collection already exists.  The
	// collection creation code will check for that.
	bool bCreatedNewCollection = CreateCollection( InNewCollectionName, InNewType );
	if( !bCreatedNewCollection )
	{
		// Failed to create new collection, so bail
		return false;
	}


	// Any assets to copy over?
	if( AssetsInCollection->Count > 0 )
	{
		// Add all of the assets assigned to the current collection to the new collection
		bool bCopiedAssets = AddAssetsToCollection( InNewCollectionName, InNewType, AssetsInCollection );
		if( !bCopiedAssets )
		{
			// Failed to copy assets
			return false;
		}
	}


	// Were we asked to perform a 'move'?  If so we'll destroy the old collection now.
	if( bInMove )
	{
		// Destroy the old collection.  This will implicitly remove all of the assets from the collection.
		bool bDestroyedOldCollection = DestroyCollection( InCurrentCollectionName, InCurrentType );
		if( !bDestroyedOldCollection )
		{
			// Couldn't destroy the old collection
			return false;
		}
	}


	// Done!
	return true;
}


/** Locally create a tag */
void FGameAssetDatabase::LocalCreateTag( String^ InTag )
{
	KnownTags.Add( CLRTools::ToFName(InTag) );
}


/** Locally destroy a tag */
void FGameAssetDatabase::LocalDestroyTag( String^ InTag )
{
	List<String^>^ AssetFullNamesWithTag = gcnew List<String^>();
	this->QueryAssetsWithTag( InTag, AssetFullNamesWithTag );

	for each ( String^ CurAssetFullName in AssetFullNamesWithTag )
	{
		RemoveTagMapping( CurAssetFullName, InTag );
	}

	KnownTags.RemoveKey( CLRTools::ToFName( InTag ) );
}


/** Locally destroy an asset from the database */
void FGameAssetDatabase::LocalDestroyAsset( String^ InAssetFullName )
{
	List<String^>^ AssetTags = gcnew List<String^>();
	this->QueryTagsForAsset( InAssetFullName, ETagQueryOptions::AllTags, AssetTags );

	for each ( String^ CurTag in AssetTags )
	{
		RemoveTagMapping( InAssetFullName, CurTag );
	}
}


/**
 * Add an asset to a collection.
 *
 * @param InCollectionName  The name of collection to which to add
 * @param InType			The type of collection specified by InCollectionName
 * @param InAssetFullNames  The full names of the assets to add
 *
 * @return True if successful
 */
bool FGameAssetDatabase::AddAssetsToCollection( System::String^ InCollectionName, EGADCollection::Type InType, Generic::ICollection<System::String^>^ InAssetFullNames )
{
	ESystemTagType CollectionTagType = GADCollectionTypeToSystemTag( InType );

	bool Result = false;
	if ( IsReadOnly() || CollectionTagType == ESystemTagType::LocalCollection )
	{
		// If this is a local collection type, locally add assets to the collection and do not update the database.
		String^ CollectionTag = MakeSystemTag( ESystemTagType::LocalCollection, InCollectionName );
		for each (System::String^ AssetName in InAssetFullNames)
		{
			// Assets added by tagging through the editor are not authoritative (the asset may not even
			// be loaded so we don't know if it really exists on disk.)
			bool bIsAuthoritative = false;
			AddTagMapping( AssetName, CollectionTag, bIsAuthoritative );
			Result = true;
		}
	}
	else
	{
		String^ CollectionTag = MakeSystemTag( CollectionTagType, InCollectionName );
		Result = AddTagToAssets( InAssetFullNames, CollectionTag );
	}
	return Result;
}

/**
 * Add an asset to a collection.
 *
 * @param InCollectionName  The name of collection to which to add
 * @param InType			The type of collection specified by InCollectionName
 * @param InAssetFullNames  The full names of the assets to add
 *
 * @return True if successful
 */
UBOOL FGameAssetDatabase::AddAssetsToCollection( const FString& InCollectionName, const EGADCollection::Type InType, const TArray< FString >& InAssetFullNames )
{
	return AddAssetsToCollection( CLRTools::ToString( InCollectionName ), InType, CLRTools::ToStringArray( InAssetFullNames ) );
}


/**
 * Remove a list of assets from a collection.
 *
 * @param InCollectionName  The name of collection from which to remove
 * @param InType			The type of collection specified by InCollectionName
 * @param InAssetFullNames  The full names of the assets to remove
 *
 * @return True if successful
 */
UBOOL FGameAssetDatabase::RemoveAssetsFromCollection( const FString& InCollectionName, const EGADCollection::Type InType, const TArray< FString >& InAssetFullNames )
{
	return RemoveAssetsFromCollection( CLRTools::ToString( InCollectionName ), InType, CLRTools::ToStringArray( InAssetFullNames ));
}


/**
 * Create a new collection into which assets can be added.
 * The collection name must be unique.
 *
 * @param InCollectionName Name of collection to create 
 * @param InType Type of collection to create
 * 
 * @return True if collection was created successfully
 */
void FGameAssetDatabase::QueryAssetsInCollection( const FString& InCollectionName, const EGADCollection::Type InType, TArray< FString >& OutAssetFullNames )
{
	List<String^>^ LocalOutAssetFullNames = gcnew List<String^>();
	QueryAssetsInCollection(CLRTools::ToString(InCollectionName), InType, LocalOutAssetFullNames);
	CLRTools::ToFStringArray(LocalOutAssetFullNames, OutAssetFullNames);
}


/**													 
 * Remove an asset from a collection.
 *
 * @param InCollectionName  The name of collection from which to remove
 * @param InType			The type of collection specified by InCollectionName
 * @param InAssetFullNames  The full names of the assets to remove
 *
 * @return True if successful
 */
bool FGameAssetDatabase::RemoveAssetsFromCollection( System::String^ InCollectionName, EGADCollection::Type InType, Generic::ICollection<System::String^>^ InAssetFullNames )
{	
	ESystemTagType CollectionTagType = GADCollectionTypeToSystemTag( InType );

	bool Result = false;
	if ( IsReadOnly() || CollectionTagType == ESystemTagType::LocalCollection )
	{
		String^ CollectionTag = MakeSystemTag( ESystemTagType::LocalCollection, InCollectionName );
		for each (System::String^ AssetName in InAssetFullNames)
		{
			RemoveTagMapping( AssetName, CollectionTag );
		}
		Result = true;
	}
	else
	{
		String^ CollectionTag = MakeSystemTag( CollectionTagType, InCollectionName );
		Result = RemoveTagFromAssets( InAssetFullNames, CollectionTag );
	}
	return Result;
}

/**  
 * Find all the assets in a given a group of collections
 * 
 * @param InCollectionName  Names of collections
 * @param InType			The type of collection specified by InCollectionName
 * @param OutAssetFullNames	[Out] Set of assets in specified collection
 */
void FGameAssetDatabase::QueryAssetsInCollections( Generic::ICollection<String^>^ InCollectionNames, EGADCollection::Type InType, [Out] List<String^>^% OutAssetFullNames ) const
{
	ESystemTagType CollectionTagType = GADCollectionTypeToSystemTag( InType );

	// Turn all the collection names into collection tags
	List<String^>^ CollectionTags = gcnew List<String^>(InCollectionNames->Count);
	for each ( String^ CollectionName in InCollectionNames )
	{
		CollectionTags->Add( MakeSystemTag( CollectionTagType, CollectionName ) );
	}

	return QueryAssetsWithAnyTags( CollectionTags, OutAssetFullNames );
}

/**  
 * Find all the assets in a given collection
 * 
 * @param InCollectionName  Name of collection
 * @param InType			The type of collection specified by InCollectionName
 * @param OutAssetFullNames	[Out] Set of assets in specified collection
 */
void FGameAssetDatabase::QueryAssetsInCollection( String^ InCollectionName, EGADCollection::Type InType, [Out] List<String^>^% OutAssetFullNames ) const
{
	String^ CollectionTag = MakeSystemTag( GADCollectionTypeToSystemTag( InType ), InCollectionName );

	return QueryAssetsWithTag( CollectionTag, OutAssetFullNames );
}



/**
 * Returns an error message string that describes in detail why the previous method failed.  The text
 * will already be localized, where possible
 *
 * @return	Localized error message text
 */
const FString& FGameAssetDatabase::GetErrorString() const
{
	return ErrorMessageText;
}



/** Static: Returns true if the specified tag is a system tag */
bool FGameAssetDatabase::IsSystemTag( String^ Tag )
{
	if( Tag->Length > 0 )
	{
		if( Tag[ 0 ] == GADDefsNative::SystemTagPreDelimiter )
		{
			return true;
		}
	}

	return false;
}



/** Static: Returns true if the specified tag is a system tag */
#pragma unmanaged
bool FGameAssetDatabase::IsSystemTag( const FString& Tag )
{
	if( Tag.Len() > 0 )
	{
		if( Tag[ 0 ] == GADDefsNative::SystemTagPreDelimiter )
		{
			return true;
		}
	}

	return false;
}

#pragma managed

/** Static: Returns an object type system tag given a class name. */
FString FGameAssetDatabase::MakeObjectTypeSystemTag( const FString& ClassName )
{
	return CLRTools::ToFString(MakeSystemTag(ESystemTagType::ObjectType, CLRTools::ToString(ClassName)));
}

/** Static: Returns the system tag type for the specified tag, or Invalid if it's not a system tag */
ESystemTagType FGameAssetDatabase::GetSystemTagType( String^ Tag )
{
	if( IsSystemTag( Tag ) )
	{
		if( Tag->Length > 0 && Tag[ 0 ] == GADDefsNative::SystemTagPreDelimiter )
		{
			int CloseBracketIndex = Tag->IndexOf( GADDefsNative::SystemTagPostDelimiter );
			if( CloseBracketIndex != -1 )
			{
				String^ SystemTagTypeName = Tag->Substring( 1, CloseBracketIndex - 1 );
				int SystemTagTypeIndex = cli::array< String^ >::IndexOf( GADDefs::SystemTagTypeNames, SystemTagTypeName );
				if( SystemTagTypeIndex != -1 )
				{
					return static_cast< ESystemTagType >( SystemTagTypeIndex );
				}
			}
		}
	}

	return ESystemTagType::Invalid;
}

/** Converts a GADCollection Type to a System Tag */
ESystemTagType FGameAssetDatabase::GADCollectionTypeToSystemTag( EGADCollection::Type InType )
{
	ESystemTagType CollectionTagType = ESystemTagType::SharedCollection;
	switch( InType )
	{
	case EGADCollection::Shared:
		CollectionTagType = ESystemTagType::SharedCollection;
		break;
	case EGADCollection::Private:
		CollectionTagType = ESystemTagType::PrivateCollection;
		break;
	case EGADCollection::Local:
		CollectionTagType = ESystemTagType::LocalCollection;
		break;
	default:
		// Invalid type
		check(FALSE);
	}

	return CollectionTagType;
}

/** Static: Queries the value part of a system tag, or returns an empty string if not a system tag */
String^ FGameAssetDatabase::GetSystemTagValue( String^ Tag )
{
	if( IsSystemTag( Tag ) )
	{
		if( Tag->Length > 0 && Tag[ 0 ] == GADDefsNative::SystemTagPreDelimiter )
		{
			int CloseBracketIndex = Tag->IndexOf( GADDefsNative::SystemTagPostDelimiter );
			if( CloseBracketIndex != -1 )
			{
				String^ SystemTagValue = Tag->Substring( CloseBracketIndex + 1 );

				// If this is a private collection tag, then we'll strip the user name off of the tag
				if( IsCollectionTag( Tag, EGADCollection::Private ) )
				{
					int DelimiterIndex = SystemTagValue->IndexOf( GADDefs::PrivateCollectionUserDelimiter );
					if( DelimiterIndex != -1 )
					{
						SystemTagValue = SystemTagValue->Substring( DelimiterIndex + 1 );
					}
				}

				return SystemTagValue;
			}
		}
	}

	return String::Empty;
}


/** Static: Returns a proper tag name for the specified private collection name */
String^ FGameAssetDatabase::MakeTagNameForPrivateCollection( String^ InCollectionName )
{
	return CLRTools::ToString( GetLocalUserName() ) + GADDefs::PrivateCollectionUserDelimiter + InCollectionName;
}


/** Static: Creates a system tag name for the specified system tag type and tag value */
String^ FGameAssetDatabase::MakeSystemTag( ESystemTagType InSystemTagType, String^ TagValue )
{
	if( InSystemTagType == ESystemTagType::PrivateCollection )
	{
		TagValue = MakeTagNameForPrivateCollection( TagValue );
	}

	return gcnew String(
		GADDefsNative::SystemTagPreDelimiter +
		GADDefs::SystemTagTypeNames[ static_cast< int >( InSystemTagType ) ] +
		GADDefsNative::SystemTagPostDelimiter +
		TagValue );
}


/** Static: Returns true if the Tag is a collection system tag*/
bool FGameAssetDatabase::IsCollectionTag( String^ Tag, EGADCollection::Type InType )
{
	return IsSystemTag(Tag) && ( GetSystemTagType( Tag ) == GADCollectionTypeToSystemTag( InType ) );
}
	

/** Static: Returns true if the specified private collection tag is valid for the current user */
bool FGameAssetDatabase::IsMyPrivateCollection( String^ PrivateCollectionTag )
{
	// Input tag must be a private collection tag!
	check( IsCollectionTag( PrivateCollectionTag, EGADCollection::Private ) );

	// Find the value part of the system tag
	if( PrivateCollectionTag->Length > 0 && PrivateCollectionTag[ 0 ] == GADDefsNative::SystemTagPreDelimiter )
	{
		int CloseBracketIndex = PrivateCollectionTag->IndexOf( GADDefsNative::SystemTagPostDelimiter );
		if( CloseBracketIndex != -1 )
		{
			String^ SystemTagValue = PrivateCollectionTag->Substring( CloseBracketIndex + 1 );

			// OK, now find the 'user' part of the private collection tag
			int DelimiterIndex = SystemTagValue->IndexOf( GADDefs::PrivateCollectionUserDelimiter );
			if( DelimiterIndex != -1 )
			{
				String^ CollectionUserName = SystemTagValue->Substring( 0, DelimiterIndex );
				if( String::Equals( CollectionUserName, CLRTools::ToString( GetLocalUserName() ), StringComparison::OrdinalIgnoreCase ) )
				{
					// We have a match!
					return true;
				}
			}
		}
	}


	return false;
}


IMPLEMENT_COMPARE_CONSTREF( FString, GameAssetDatabase, { return appStricmp( *A, *B ); } );


/** Updates the game asset database from the content journal server */
bool FGameAssetDatabase::UpdateDatabase( const FGameAssetDatabaseStartupConfig& InConfig )
{
	const UBOOL bIsCommandlet = InConfig.bIsCommandlet;


	String^ CheckpointFileName = MakeCheckpointFileName();
	pin_ptr< const TCHAR > PinnedCheckpointFileName = PtrToStringChars( CheckpointFileName );


	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "Checkpoint file name: {0}", CheckpointFileName ) );


	DateTime^ CheckpointFileTimeStamp = gcnew DateTime( 0 );
	if( InConfig.bShouldLoadCheckpointFile )
	{
		// Load all concrete tags from the checkpoint file
		if( LoadCheckpointFile( CheckpointFileTimeStamp ) )	// Out
		{
			CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( 
				"Existing checkpoint file contains timestamp: {0}.",
				CheckpointFileTimeStamp->ToString() ) );

			CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( 
				"Loaded {0} assets, {1} tags/collections ({2} in use) from the checkpoint file.",
				AssetToTagMap.NumKeys(),
				KnownTags.Num(),
				TagToAssetMap.NumKeys() ) );
		}
		else
		{
			// Failed to load checkpoint file.  ErrorMessageText will contain the reason.
			if( bIsCommandlet )
			{
				return false;
			}
		}

		// Debug Tool: Print out all asset names
		if ( InConfig.bShouldDumpDatabase )
		{
			CLRTools::LogWarningMessage("======================================");
			CLRTools::LogWarningMessage(" Assets in checkpoint file");
			CLRTools::LogWarningMessage("======================================");
			DumpDatabase();
		}
	}


	TSet< FName > CurrentlyExistingAssets;
	if( bIsCommandlet && InConfig.bShouldCheckpoint )
	{
		// Generate all of the default tags (replacing existing system tags if needed)
		RebuildDefaultTags( CurrentlyExistingAssets );	// Out
	}


	// Delete entries that are more than one week old
	DateTime^ DeleteServerEntriesOlderThan = nullptr;
	if( InConfig.bShouldCheckpoint && InConfig.bAllowServerDeletesAfterCheckpoint )
	{
		DeleteServerEntriesOlderThan = DateTime::Now.AddDays( -GADDefs::DeleteJournalEntriesOlderThanDays );
	}


	// In the editor we're only interested in journal entries that were created by the user
	// running the editor right now.  This is because other users may be interacting with
	// objects and packages that don't even exist for the current user and it reduces the
	// chances of the Content Browser displaying inaccurate or confusing GAD data.
	String^ RestrictUserName = nullptr;
	if( !bIsCommandlet && InConfig.bOnlyLoadMyJournalEntries )
	{
		RestrictUserName = CLRTools::ToString( GetLocalUserName() );
	}


	// Now load all of the journal data, keeping track of the database indices that we processed
	List< int >^ JournalDatabaseIndicesToDelete = gcnew List< int >();
	DateTime^ IgnoreServerEntriesAsOldAs = CheckpointFileTimeStamp;
	if( InConfig.bShouldLoadJournalEntries )
	{
		// We'll add assets from the journal authoritatively only if we're the commandlet (checkpointing assets)
		bool bIsAuthoritative = bIsCommandlet ? true : false;

		// Load journal entries
		if( !LoadJournalData(
				RestrictUserName,
				IgnoreServerEntriesAsOldAs,
				DeleteServerEntriesOlderThan,
				bIsAuthoritative,
				JournalDatabaseIndicesToDelete ) )		// Out
		{
			// @todo CB: Consider allowing life to go on even if we can't connect (no journaling abilities, only checkpoint data)

			// Failed to load journal data.  ErrorMessageText will contain the reason.
			if( bIsCommandlet )
			{
				return false;
			}
		}
	}


	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
		"Game asset database loaded a total of {0} assets, {1} tags and {2} persistent tags.",
		AssetToTagMap.NumKeys(),
		TagToAssetMap.NumKeys(),
		KnownTags.Num() ) );


	if( bIsCommandlet )
	{

		if( InConfig.bShouldCheckpoint )
	    {
			// Remove all 'unverified' system tags.  We're checkpointing so anything that we can't verify
			// will end up tagged as a 'ghost' anyway
			{
				// NOTE: Really there should never be any Unverified tags at this point -- this is kind of a
				//		 pathological check
				List<String^> AllSystemTags;
				QueryAllTags( %AllSystemTags, ETagQueryOptions::SystemTagsOnly );
				for each( String^ CurSystemTag in AllSystemTags )
				{
					ESystemTagType CurTagType = GetSystemTagType( CurSystemTag );
					if( CurTagType == ESystemTagType::Unverified )
					{
						LocalDestroyTag( CurSystemTag );
					}
				}
			}
			CheckForGhostAssets( CurrentlyExistingAssets, InConfig );
		}

		// If we are worried about content in all branches and games, we have to load the server journal data again
		if ( ( InConfig.bShowAllOldContent || InConfig.bPurgeAllOldContent ) && !bInOfflineMode )
		{
			// get all journal entries from all branches and games, fully unfiltered
			List< int >^ AllJournalDatabaseIndicesConsideredOld = gcnew List< int >();
			
			DateTime^ PurgeServerEntriesOlderThan = DateTime::Now.AddDays( -GADDefs::PurgeJournalEntriesOlderThanDays );

			// load data - should print out to console
			if( !LoadAllServerJournalData(
					PurgeServerEntriesOlderThan,
					AllJournalDatabaseIndicesConsideredOld ) )
			{
				// Failed to load journal data.  ErrorMessageText will contain the reason.
				return false;
			}

			// if we are purging it, delete the journal indices
			if (InConfig.bPurgeAllOldContent)
			{
				if( AllJournalDatabaseIndicesConsideredOld->Count > 0 )
				{
					CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
						"Deleting {0} old journal entries from the SQL database.",
						AllJournalDatabaseIndicesConsideredOld->Count ) );

					if( bIsJournalServerAvailable && JournalClient->DeleteJournalEntries( AllJournalDatabaseIndicesConsideredOld ) )
					{
						// ...
					}
					else
					{
						// Couldn't query journal entries.  Reason is stored in ErrorMessageText
						return false;
					}
				}
			}
		}

		// Check integrity of loaded data and repair if needed
		if( InConfig.bShouldVerifyIntegrity )
		{
			VerifyIntegrityOfDatabase( InConfig.bShouldRepairIfNeeded );
		}

		// Debug Tool: Print out all asset names
		if ( InConfig.bShouldDumpDatabase )
		{
			CLRTools::LogWarningMessage("======================================");
			CLRTools::LogWarningMessage(" After Journal File Download");
			CLRTools::LogWarningMessage("======================================");
			DumpDatabase();
		}



		// If we were asked to strip any collections prior to saving the checkpoint, we'll do that now
		if( InConfig.bDeletePrivateCollections || InConfig.bDeleteNonUDKCollections )
		{
			// Grab all system tags
			Generic::List<String^>^ AllCollectionTags;
			QueryAllTags( AllCollectionTags, ETagQueryOptions::CollectionsOnly );

			for each(String^ Tag in AllCollectionTags)
			{
				if( InConfig.bDeletePrivateCollections && IsCollectionTag( Tag, EGADCollection::Private ) )
				{
					// Strip all private collections
					LocalDestroyTag( Tag );
				}
				else if( InConfig.bDeleteNonUDKCollections )
				{
					// We'll strip any collection that doesn't start with "UDK"
					String^ CollectionName = GetSystemTagValue( Tag );
					if( !CollectionName->StartsWith( "UDK" ) )
					{
						LocalDestroyTag( Tag );
					}
				}
			}
		}

		// If we were asked to strip non-UDK tags we'll do that now
		if( InConfig.bDeleteNonUDKCollections )
		{
			// Grab all user tags
			Generic::List<String^>^ AllUserTags;
			QueryAllTags( AllUserTags, ETagQueryOptions::UserTagsOnly );

			for each(String^ Tag in AllUserTags)
			{
				// We'll strip any collection that starts with the "Audit" group name (see UTagCookedReferencedAssetsCommandlet::UpdateCollections)
				if( Tag->StartsWith( "Audit." ) )
				{
					LocalDestroyTag( Tag );
				}
			}
		}

		// Save checkpoint file if we are performing the checkpoint process and not dumping debug output.
		if( InConfig.bShouldCheckpoint && !InConfig.bShouldDumpDatabase )
		{
			CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
				"Saving checkpoint file with {0} assets, {1} tags and {2} persistent tags.",
				AssetToTagMap.NumKeys(),
				TagToAssetMap.NumKeys(),
				KnownTags.Num() ) );

			// Go ahead and save the checkpoint file to disk
			if( !SaveCheckpointFile() )
			{
				// Failed to generate checkpoint file.  ErrorMessageText will contain the reason.
				return false;
			}


			if( !bInOfflineMode )
			{
				if ( InConfig.bAllowServerDeletesAfterCheckpoint )
				{
					CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
						"Journal entries older than {0} will be deleted from the server.",
						DeleteServerEntriesOlderThan->ToString() ) );


					// Now that we've saved everything, we'll delete all of the processed journal entries
					// from the server!
					if( JournalDatabaseIndicesToDelete->Count > 0 )
					{
						CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
							"Deleting {0} expired journal entries from the SQL database.",
							JournalDatabaseIndicesToDelete->Count ) );

						if( bIsJournalServerAvailable && JournalClient->DeleteJournalEntries( JournalDatabaseIndicesToDelete ) )
						{
							// ...
						}
						else
						{
							// Couldn't query journal entries.  Reason is stored in ErrorMessageText
							return false;
						}
					}
					else
					{
						CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "No expired journal entries to delete yet." ) );
					}
				}
			}
			else
			{
				//Delete the journal file now that we've performed maintenance
				CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
					"Purging local journal file (all entries were merged into checkpoint file.)" ) );
				if( OfflineJournalFile->DeleteJournalFile() )
				{
					CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
						"Journal file was deleted successfully" ) );
				}
				else
				{
					CLRTools::LogWarningMessage( String::Format(
						"GameAssetDatabase: Warning: Unable to delete the journal file from disk (possibly read-only?)" ) );
				}
			}
		}
	}

	return true;
}

/** Print the current state of the database for debug */
void FGameAssetDatabase::DumpDatabase()
{
	TLookupMap< FName > AllAssetFullNames;		
	AssetToTagMap.GetKeys( AllAssetFullNames );
	TArray< FString > SortedAssetFullNames;
	for( TLookupMap< FName >::TConstIterator It( AllAssetFullNames ); It; ++It )
	{
		SortedAssetFullNames.AddItem( It.Key().ToString() );
	}
	Sort<USE_COMPARE_CONSTREF(FString,GameAssetDatabase)>( SortedAssetFullNames.GetTypedData(), SortedAssetFullNames.Num() );
	for( INT CurNameIndex = 0; CurNameIndex < SortedAssetFullNames.Num(); ++CurNameIndex )
	{
		FString &AssetName = SortedAssetFullNames( CurNameIndex );

		System::Text::StringBuilder^ Tags = gcnew System::Text::StringBuilder();
		TSet< FName > TagsSet;
		AssetToTagMap.FindValuesForKey( FName(*AssetName), TagsSet );
		for( TSet<FName>::TConstIterator TagIt(TagsSet); TagIt; ++TagIt )
		{
			Tags->AppendFormat( "{0}; ", CLRTools::ToString( (*TagIt).ToString() ) );
		}

		CLRTools::LogWarningMessage( String::Format( "[{0}] {1} : {2}", CurNameIndex, CLRTools::ToString( AssetName ), Tags->ToString() )   );
	}

}


	
/** Sets the global error string.  Used internally only. */
void FGameAssetDatabase::SetErrorMessageText( String^ InErrorString )
{
	ErrorMessageText = CLRTools::ToFString( InErrorString );
}



/** Returns the global error message text. */
String^ FGameAssetDatabase::GetErrorMessageText() const
{
	return CLRTools::ToString( ErrorMessageText );
}



/** Static: Returns the file name of the checkpoint file we should use */
String^ FGameAssetDatabase::MakeCheckpointFileName()
{
	String^ CheckpointFileName = String::Format( "{0}Content\\GameAssetDatabase.checkpoint", gcnew String( *appGameDir() ) );
	return CheckpointFileName;
}



/** Static: Returns the file name of the offline journal file we should use */
String^ FGameAssetDatabase::MakeOfflineJournalFileName()
{
	String^ JournalFileName = String::Format( "{0}Content\\GameAssetDatabase.journal", gcnew String( *appGameDir() ) );
	return JournalFileName;
}


/** Adds an asset -> tag name mapping to our in-memory hash tables */
void FGameAssetDatabase::AddTagMapping( const FName& InAssetFullNameFName, const FName& InTagFName, bool bIsAuthoritative )
{
	String^ AssetFullName = CLRTools::ToString(InAssetFullNameFName.ToString());
	if( CLRTools::IsAssetValidForTagging( AssetFullName ) )
	{
		// If the asset/tag didn't come from a checkpoint file (e.g. it came from a SQL journal entry) then we can't
		// really trust it as the checkpoint process validates that the asset was actually checked in
		if( !bIsAuthoritative )
		{
			// If the asset didn't already exist in our map, then we'll mark it as unverified since
			// it wasn't loaded from the checkpoint file
			if( !AssetToTagMap.ContainsKey( InAssetFullNameFName ) )
			{
				// Mark the asset as unverified!
				bool bIsAuthoritativeForUnverifiedTag = true;
				AddTagMapping( InAssetFullNameFName, CLRTools::ToFName( MakeSystemTag( ESystemTagType::Unverified, "" ) ), bIsAuthoritativeForUnverifiedTag );
			}
		}

		// Add to the list of known tags
		KnownTags.Add( InTagFName );

		// Map the tag to the asset...
		TagToAssetMap.Add( InTagFName, InAssetFullNameFName );

		// ...and the asset to the tag
		bool bIsNewAsset = !AssetToTagMap.ContainsKey( InAssetFullNameFName );
		AssetToTagMap.Add( InAssetFullNameFName, InTagFName );


		// Update our set of unique asset path names
		if( bIsNewAsset )
		{
			const INT FirstSpaceIndex = AssetFullName->IndexOf( TCHAR( ' ' ) );
			check( FirstSpaceIndex != -1 );
			String^ AssetPath = AssetFullName->Substring( FirstSpaceIndex + 1 );	// Everything after the first space

			// Grab the object's name and package by splitting the full path string
			const INT LastDotIndex = AssetPath->LastIndexOf( TCHAR( '.' ) );
			check( LastDotIndex != -1 );
			String^ AssetPathOnly = AssetPath->Substring( 0, LastDotIndex );	// Everything before the last dot

			// Update the set!
			if( UniqueAssetPaths->ContainsKey( AssetPathOnly ) )
			{
				++UniqueAssetPaths.get()[ AssetPathOnly ];
			}
			else
			{
				UniqueAssetPaths->Add( AssetPathOnly, 1 );
			}
		}
	}
}


/** Adds an asset -> tag name mapping to our in-memory hash tables */
void FGameAssetDatabase::AddTagMapping( String^ InAssetFullName, String^ InTag, bool bIsAuthoritative )
{
	FName AssetFullNameFName = CLRTools::ToFName( InAssetFullName );
	FName TagFName = CLRTools::ToFName(InTag);

	AddTagMapping( AssetFullNameFName, TagFName, bIsAuthoritative );
}



/** Removes an asset -> tag name mapping from our in-memory hash tables */
void FGameAssetDatabase::RemoveTagMapping( String^ InAssetFullName, String^ InTag )
{
	FName AssetFullNameFName = CLRTools::ToFName( InAssetFullName );
	FName TagFName = CLRTools::ToFName(InTag);

	// Remove the entry from the tag -> asset map...
	TagToAssetMap.Remove( TagFName, AssetFullNameFName );

	// ...and also from the asset -> tag map
	if( AssetToTagMap.ContainsKey( AssetFullNameFName ) )
	{
		AssetToTagMap.Remove( AssetFullNameFName, TagFName );
		bool bWasLastAssetReference = !AssetToTagMap.ContainsKey( AssetFullNameFName );

		// Update our set of unique asset path names
		if( bWasLastAssetReference )
		{
			const INT FirstSpaceIndex = InAssetFullName->IndexOf( TCHAR( ' ' ) );
			check( FirstSpaceIndex != -1 );
			String^ AssetPath = InAssetFullName->Substring( FirstSpaceIndex + 1 );	// Everything after the first space

			// Grab the object's name and package by splitting the full path string
			const INT LastDotIndex = AssetPath->LastIndexOf( TCHAR( '.' ) );
			check( LastDotIndex != -1 );
			String^ AssetPathOnly = AssetPath->Substring( 0, LastDotIndex );	// Everything before the last dot

			// Update the set!
			if( UniqueAssetPaths.get()[ AssetPathOnly ] == 1 )
			{
				UniqueAssetPaths->Remove( AssetPathOnly );
			}
			else
			{
				--UniqueAssetPaths.get()[ AssetPathOnly ];
			}
		}
	}

}

/**
 * Removes all tag mappings for the specified asset.  Called when an asset is deleted.
 *
 * @param	AssetFullNameFName	the Unreal full name for the asset to remove tag mappings for.
 */
void FGameAssetDatabase::RemoveAssetTagMappings( const FName& AssetFullNameFName )
{
	TArray<FName> AssetTags;
	AssetToTagMap.FindValuesForKey(AssetFullNameFName, AssetTags);

	for ( INT TagIndex = 0; TagIndex < AssetTags.Num(); TagIndex++ )
	{
		FName Tag = AssetTags(TagIndex);
		RemoveTagMapping( CLRTools::FNameToString( AssetFullNameFName ), CLRTools::FNameToString( Tag ) );
	}
}


/** Static: Get JournalAlarmTime from ini file*/
DateTime FGameAssetDatabase::GetJournalAlarmTime (void)
{
	DateTime JournalAlarmTime;
	FString JournalAlarmTimeString;
	GConfig->GetString(TEXT("GameAssetDatabase" ), TEXT( "JournalAlarmTime" ), JournalAlarmTimeString, GEditorUserSettingsIni );
	if (JournalAlarmTimeString.Len() == 0)
	{
		//first time checking for the alarm
		INT DayCount = 7;
		//days, hours, minutes, seconds
		TimeSpan Duration (DayCount, 0, 0, 0);
		JournalAlarmTime = DateTime::Now;
		JournalAlarmTime = JournalAlarmTime.Add(Duration);
		SetJournalAlarmTime(JournalAlarmTime);
	}
	else
	{
		//convert FROM string
		JournalAlarmTime = Convert::ToDateTime(CLRTools::ToString(JournalAlarmTimeString), gcnew CultureInfo("en-US"));
	}
	return JournalAlarmTime;
}

/** Static: Sets JournalAlaramTime to ini file*/
void FGameAssetDatabase::SetJournalAlarmTime (DateTime InAlarmTime)
{
	FString JournalAlarmTimeString = CLRTools::ToFString(InAlarmTime.ToString(gcnew CultureInfo("en-US")));
	GConfig->SetString(TEXT("GameAssetDatabase" ), TEXT( "JournalAlarmTime" ), *JournalAlarmTimeString, GEditorUserSettingsIni );
}

namespace EJournalAlarmResults
{
	enum 
	{
		UpdateNow,
		SnoozeUntilNextRun,
		SnoozeOneDay,
		SnoozeOneWeek,
		//DisableAlarm,
	};
};

/** Static: Update the journal */
UBOOL FGameAssetDatabase::JournalUpdate (void)
{
	UBOOL bRet = FALSE;

	FGameAssetDatabaseStartupConfig StartupConfig;
	// Tell the GAD that we're in "commandlet mode"
	StartupConfig.bIsCommandlet = TRUE;
	StartupConfig.bShouldVerifyIntegrity = TRUE;
	StartupConfig.bShouldCheckpoint = TRUE;

	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("GameAssetDatabase_CheckPointFile_VerifyIntegrityProgressBar")), TRUE );

	// Init the game asset database and checkpoint all journal data immediately
	FString InitErrorMessageText;
	FGameAssetDatabase::Init(
		StartupConfig,
		InitErrorMessageText );	// Out

	if( InitErrorMessageText.Len() > 0 )
	{
		warnf( NAME_Error, TEXT( "Error: %s" ), *InitErrorMessageText );
	}
	else
	{
		warnf( NAME_Log, TEXT( "Game asset database maintenance completed successfully." ) );
		bRet = TRUE;
	}

	// Shutdown the game asset database
	FGameAssetDatabase::Destroy();

	GWarn->EndSlowTask();

	return bRet;
}

/** Static: Show and react to dialog when journal alarm goes off */
void FGameAssetDatabase::ShowJournalAlarmDialog (void)
{
	//display dialog to ask if they want to snooze again or run the commandlet
	WxChoiceDialog MyDialog(
		LocalizeUnrealEd( TEXT("GameAssetDatabase_CheckPointFile_UnverifiedAssetCleanseRequest")), 
		LocalizeUnrealEd( TEXT("GameAssetDatabase_CheckPointFile_UnverifiedAssetCleanseTitle")),
		WxChoiceDialogBase::Choice( EJournalAlarmResults::UpdateNow,          LocalizeUnrealEd( TEXT("GameAssetDatabase_CheckPointFile_UpdateNow") ), WxChoiceDialogBase::DCT_DefaultAffirmative ),				
		WxChoiceDialogBase::Choice( EJournalAlarmResults::SnoozeUntilNextRun, LocalizeUnrealEd( TEXT("GameAssetDatabase_CheckPointFile_SnoozeUntilNextRun") ), WxChoiceDialogBase::DCT_DefaultCancel ),
		WxChoiceDialogBase::Choice( EJournalAlarmResults::SnoozeOneDay,       LocalizeUnrealEd( TEXT("GameAssetDatabase_CheckPointFile_SnoozeOneDay") ) ),
		WxChoiceDialogBase::Choice( EJournalAlarmResults::SnoozeOneWeek,      LocalizeUnrealEd( TEXT("GameAssetDatabase_CheckPointFile_SnoozeOneWeek") ) ));
		//WxChoiceDialogBase::Choice( EJournalAlarmResults::DisableAlarm,       LocalizeUnrealEd( TEXT("GameAssetDatabase_CheckPointFile_NeverAlert") ) )
	//make sure it shows up over the splash screen
	MyDialog.SetWindowStyle( MyDialog.GetWindowStyle() | wxSTAY_ON_TOP);
	MyDialog.ShowModal();

	INT DaysToSnooze = 0;

	INT Choice = MyDialog.GetChoice().ReturnCode;
	switch (Choice)
	{
		case EJournalAlarmResults::UpdateNow:
			{
				if ( JournalUpdate() )
				{
					DaysToSnooze = 7;
				}
			}
			break;
		case EJournalAlarmResults::SnoozeUntilNextRun:
			//Do Nothing, the previous alarm time should make it go off again right away
			break;
		case EJournalAlarmResults::SnoozeOneDay:
			{
				//days, hours, minutes, seconds
				DaysToSnooze = 1;
			}
			break;
		case EJournalAlarmResults::SnoozeOneWeek:
			{
				//days, hours, minutes, seconds
				DaysToSnooze = 7;
			}
			break;
		//case EJournalAlarmResults::DisableAlarm:
		//	GConfig->SetBool(TEXT("GameAssetDatabase"), TEXT("UseJournalUpdateAlarm"), FALSE, GEditorUserSettingsIni);
		//	break;
	}

	if (DaysToSnooze > 0)
	{
		TimeSpan Duration (DaysToSnooze, 0, 0, 0);
		DateTime JournalAlarmTime = DateTime::Now;
		JournalAlarmTime = JournalAlarmTime.Add(Duration);
		SetJournalAlarmTime(JournalAlarmTime);
	}
}

/** Static: Finds if in offline mode or not*/
UBOOL FGameAssetDatabase::GetInOfflineMode (void)
{
#if UDK
	// For UDK we default to using the offline journaling system
	UBOOL bInOfflineMode = TRUE;
#else 
	// For Epic and licensees we want to encourage use of the SQL-based journal system
	UBOOL bInOfflineMode = FALSE;
#endif

	UBOOL bWantOfflineMode = FALSE;
	if( GConfig->GetBool( TEXT( "GameAssetDatabase" ), TEXT( "OfflineMode" ), bWantOfflineMode, GEditorUserSettingsIni ) )
	{
		bInOfflineMode = bWantOfflineMode;
	}

	if( ParseParam( appCmdLine(), TEXT("NODATABASE") ) )
	{
		bInOfflineMode = TRUE;
	}

	return bInOfflineMode;
}



/** Static: Queries a local user name to use for journal entries and private collections */
FString FGameAssetDatabase::GetLocalUserName()
{
	// Default to using the system user name
	FString UserName = appUserName();

	// Check to see if the user overrided their user name in the preferences.  This is useful
	// when working from a different PC (such as a home computer through VPN) which may not
	// match your work computer's login
	FString OverrideUserNameString;
	if( GConfig->GetString( TEXT( "GameAssetDatabase" ), TEXT( "OverrideUserName" ), OverrideUserNameString, GEditorUserSettingsIni ) &&
		OverrideUserNameString.Len() > 0 )
	{
		UserName = OverrideUserNameString;
	}

	return UserName;
}



/** Attempts to load the checkpoint file from disk */
bool FGameAssetDatabase::LoadCheckpointFile( [Out] DateTime^% OutCheckpointFileTimeStamp )
{
	String^ CheckpointFileName = MakeCheckpointFileName();

	try
	{
		// Open the file
		auto_handle< FileStream > File = File::OpenRead( CheckpointFileName );

		// Create the binary reader
		auto_handle< BinaryReader > Reader = gcnew BinaryReader( File.get() );


		// Load the file header
		cli::array< unsigned char >^ HeaderChars = Reader->ReadBytes( GADDefs::CheckpointFileHeaderChars->Length );
		for( int CurCharIndex = 0; CurCharIndex < GADDefs::CheckpointFileHeaderChars->Length; ++CurCharIndex )
		{
			if( HeaderChars[ CurCharIndex ] != GADDefs::CheckpointFileHeaderChars[ CurCharIndex ] )
			{
				// File header doesn't match!  Probably not a checkpoint file, or is corrupt.
				throw gcnew SystemException( TEXT( "Checkpoint file header doesn't match" ) );;
			}
		}


		// Load the checkpoint file version number
		int CheckpointFileVersion = Reader->ReadInt32();
		if( CheckpointFileVersion > GADDefs::CheckpointFileVersionNumber )
		{
			// Ack!  We can't load a checkpoint file that was created by a newer version of the editor.
			// This can happen if the build machine checks in a checkpoint file but users aren't
			// on the same version yet.
			throw gcnew SystemException( TEXT( "The checkpoint file was created with a newer version of the application that is currently loaded." ) );
		}


		// Load the time stamp for the file (as Ticks)
		OutCheckpointFileTimeStamp = gcnew DateTime( Reader->ReadUInt64() );


		// Read name ID table
		bool FoundCollisions = false;
		TMap< INT, FName > NameIDToFNameMap;
		{
			// Number of names
			int NameCount = Reader->ReadInt32();

			for( INT CurNameIndex = 0; CurNameIndex < NameCount; ++CurNameIndex )
			{
				// Name ID
				int NameID = Reader->ReadInt32();

				// Name string
				String^ Name = Reader->ReadString();

				// In a well formed checkpoint, there should be no collisions.  However, SaveCheckpoint()
				// used to have a bug which would cause fnames of the form Name_Number to collide
				// with each other.  Warn the user if any are found.
				FName* AlreadyExistingName = NameIDToFNameMap.Find(NameID);
				if (AlreadyExistingName != NULL)
				{
					FoundCollisions = true;

					CLRTools::LogWarningMessage(
						String::Format(
						    "Warning: Name collision '{0}' <-> '{1}'.",
						    Name,
							CLRTools::FNameToString(*AlreadyExistingName)));
				}

				// Add to table
				NameIDToFNameMap.Set( NameID, FName( *CLRTools::ToFString( Name ) ) );
			}
		}


		if (FoundCollisions)
		{
			CLRTools::LogWarningMessage(
				String::Format(
				"GameAssetDatabase::LoadCheckpointFile> Warning: the checkpoint file '{0}' needs to be resaved.",
				CheckpointFileName));
		}


		// Persistent collections
		if( CheckpointFileVersion >= GADDefs::CheckpointFileVersionNumber_PersistentCollections )
		{
			// Number of persistent tags
			int TagCount = Reader->ReadInt32();

			// Collections
			for( int CurTagIndex = 0; CurTagIndex < TagCount; ++CurTagIndex )
			{
				// Name ID of the collection
				int TagNameID = Reader->ReadInt32();
				FName TagName( NameIDToFNameMap.FindRef( TagNameID ) );

				// Add persistent tag to our list
				KnownTags.Add( TagName );
			}
		}



		// Assets and their associated tags/collections
		{
			// Read the number of assets
			int AssetCount = Reader->ReadInt32();

			// Read each asset
			for( int CurAssetIndex = 0; CurAssetIndex < AssetCount; ++CurAssetIndex )
			{
				// Name ID of the asset
				int AssetFullNameID = Reader->ReadInt32();

				// Asset full name
				const FName AssetFullName = NameIDToFNameMap.FindRef( AssetFullNameID );
				if( CheckpointFileVersion < GADDefs::CheckpointFileVersionNumber_AssetFullNames )
				{
					// Older versions stored only the asset path, not the full name

					// NOTE: We'll rely on a call to FixUpBrokenAssetNames() to fix these cases, after
					//       the file has been loaded.
				}

				// Number of tags
				int TagCount = Reader->ReadInt32();

				// The tags assigned to this asset
				for( int CurTagIndex = 0; CurTagIndex < TagCount; ++CurTagIndex )
				{
					int TagID = Reader->ReadInt32();
					FName Tag( NameIDToFNameMap.FindRef( TagID ) );

					// Tag data from checkpoint files are always authoritative
					bool bIsAuthoritative = true;

					// Update mappings
					AddTagMapping( AssetFullName, Tag, bIsAuthoritative );
				}
			}
		}



		// Close the file
		Reader->Close();
		File->Close();


		// If the file was an older version, then fix up asset names
		if( CheckpointFileVersion < GADDefs::CheckpointFileVersionNumber_AssetFullNames )
		{
			FixBrokenAssetNames();
		}
	}

	catch( FileNotFoundException^ )
	{
		// Couldn't open the file.  We'll handle this gracefully.
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "Warning: Checkpoint file [{0}] was not found.", CheckpointFileName ) );
	}

	catch( Exception ^E )
	{
		SetErrorMessageText( CLRTools::LocalizeString(
			"GameAssetDatabase_ErrorLoadingCheckpointFile_F",
			CheckpointFileName,	// Checkpoint file name
			E->ToString() ) );	// Exception details
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "FGameAssetDatabase::LoadCheckpointFile: {0}", GetErrorMessageText() ) );

		return false;
	}


	return true;
}



/** Attempts to save a new checkpoint file to disk */
bool FGameAssetDatabase::SaveCheckpointFile()
{
	String^ CheckpointFileName = MakeCheckpointFileName();

	try
	{
		//Check if the file already exists...
		if (File::Exists(CheckpointFileName))
		{
			FileAttributes Attr = File::GetAttributes(CheckpointFileName); 
			//we must ensure the the exisiting file will be overwritten
			if ((Attr & FileAttributes::ReadOnly) == FileAttributes::ReadOnly)
			{
				CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "Warning: Checkpoint file [{0}] is marked readonly, overwriting.", CheckpointFileName ) );
				File::SetAttributes(CheckpointFileName,(Attr & ~FileAttributes::ReadOnly));
			}
			
		}
		
		// Create the file
		auto_handle< FileStream > File = File::Create( CheckpointFileName );

		// Create the binary writer
		auto_handle< BinaryWriter > Writer = gcnew BinaryWriter( File.get() );


		// Write the file header ('G'ame 'A'sset 'D'atabase 'C'heckpoint)
		Writer->Write( GADDefs::CheckpointFileHeaderChars );

		// Write the version number
		Writer->Write( GADDefs::CheckpointFileVersionNumber );


		// Write the time stamp (as Ticks)
		UINT64 TimeStampTicks = DateTime::Now.Ticks;
		Writer->Write( TimeStampTicks );


		// Gather asset names
		TLookupMap< FName > AllAssetFullNames;
		AssetToTagMap.GetKeys( AllAssetFullNames );	// Out

		// Gather tags
		TLookupMap< FName > AllTags;
		TagToAssetMap.GetKeys( AllTags );	// Out

		
		// Build and write the name ID table
		TLookupMap< FName > AllNames;
		{
			// Add asset names
			for( TLookupMap< FName >::TConstIterator It( AllAssetFullNames ); It; ++It )
			{
				AllNames.AddItem( It.Key() );
			}
			
			// Add tags
			for( TLookupMap< FName >::TConstIterator It( AllTags ); It; ++It )
			{
				AllNames.AddItem( It.Key() );
			}

			// Add persistent tags
			for( FNameSet::TConstIterator It( KnownTags ); It; ++It )
			{
				AllNames.AddItem( *It );
			}

			// Write the number of names in the checkpoint
			Writer->Write( AllNames.Num() );

			// Write each index - name pair out
			INT NameCount = AllNames.GetUniqueElements().Num();
			for( INT index = 0; index < NameCount; ++index)
			{
				// Name index
				Writer->Write( index );

				// Name string
				const FName& CurName = AllNames.GetItem(index);
				Writer->Write( CLRTools::ToString( *CurName.ToString() ) );
			}
		}


		// Persistent tags
		{
			// Number of tags
			Writer->Write( KnownTags.Num() );

			// tags
			for( FNameSet::TConstIterator It( KnownTags ); It; ++It )
			{
				INT* pNameIndex = AllNames.Find(*It);
				check(pNameIndex != NULL);

				Writer->Write( *pNameIndex );
			}
		}


		// Assets and their associated tags
		{
			// Number of assets
			Writer->Write( AllAssetFullNames.Num() );

			// Assets
			for( TLookupMap< FName >::TConstIterator It( AllAssetFullNames ); It; ++It )
			{
				const FName& CurName = It.Key();

				TArray< FName > Tags;
				AssetToTagMap.FindValuesForKey( CurName, Tags );

				// Name ID of the asset
				INT* pNameIndex = AllNames.Find(CurName);
				check(pNameIndex != NULL);
				Writer->Write( *pNameIndex );

				// Number of tags
				Writer->Write( Tags.Num() );

				// The tags assigned to this asset
				for( INT CurTagIndex = 0; CurTagIndex < Tags.Num(); ++CurTagIndex )
				{
					// Tag ID
					const FName& CurTag = Tags( CurTagIndex );

					INT* pTagIndex = AllNames.Find(CurTag);
					check(pTagIndex != NULL);
					Writer->Write( *pTagIndex );
				}
			}
		}


		// Close the file
		Writer->Close();
		File->Close();
	}

	catch( Exception^ E )
	{
		SetErrorMessageText( CLRTools::LocalizeString(
			"GameAssetDatabase_ErrorWritingCheckpointFile_F",
			CheckpointFileName,	// Checkpoint file name
			E->ToString() ) );	// Exception details
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format("FGameAssetDatabase::SaveCheckpointFile: {0}", GetErrorMessageText() ) );

		return false;
	}

	return true;
}



/** Attempts to load journal data from disk and from the server */
bool FGameAssetDatabase::LoadJournalData( String^ RestrictUserName, DateTime^ IgnoreServerEntriesAsOldAs, DateTime^ DeleteServerEntriesOlderThan, bool bIsAuthoritative, List< int >^& OutDatabaseIndicesToDelete )
{
	OutDatabaseIndicesToDelete = gcnew List< int >();

	bIsJournalServerAvailable = FALSE;
	
	const bool bFilterBranchAndGame = true;

	
	// First try to load any entries from a journal file on disk.  We do this whether or not
	// we're in offline mode
	MGameAssetJournalEntryList^ OfflineJournalEntries = nullptr;
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, "Loading journal entries from the journal file." );


		// Query all of the records we're interested in
		if( !OfflineJournalFile->QueryJournalEntries( OfflineJournalEntries, bFilterBranchAndGame ) )
		{
			// Couldn't query journal entries.  Reason is stored in ErrorMessageText
			return false;
		}


		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
			"Loaded {0} journal entries from the journal file.",
			OfflineJournalEntries->Count ) );
	}


	// Now, unless we're in offline mode, try to connect to the SQL server and download additional
	// journal entries
	MGameAssetJournalEntryList^ ServerJournalEntries = nullptr;
	if( !bInOfflineMode )
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, "Connecting to the SQL database to download journal data. (online mode)" );


		// Query all of the records we're interested in
		if( !JournalClient->QueryJournalEntries( ServerJournalEntries, bFilterBranchAndGame ) )
		{
			// Couldn't query journal entries.  Reason is stored in ErrorMessageText
			// However, we can still log to an offline file so put us in offline mode
			bInOfflineMode = TRUE;
		}
		else
		{
			CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
				"Downloaded {0} journal entries from the SQL database.",
				ServerJournalEntries->Count ) );
		

			// We were able to connect to the server, so keep track of that so other systems can query it
			bIsJournalServerAvailable = TRUE;
		}
	}


	// If the server is available and we have offline entries, then we'll go ahead and upload
	// them to the server right now (and delete them from disk!)
	if( bIsJournalServerAvailable && OfflineJournalEntries->Count > 0 )
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
			"Uploading {0} journal entries that were stored offline to the SQL database.",
			OfflineJournalEntries->Count ) );

		// Upload the offline entries to the journal server
		if( JournalClient->SendJournalEntries( OfflineJournalEntries ) )
		{
			// Entries were uploaded successfully, so prune them from the HDD as we'll assume that
			// the user will get them from the server from now on.  We always want to upload the
			// entries ASAP so that other users will have access to those changes.

			CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
				"Purging local journal file (all entries were uploaded to SQL server.)" ) );

			if( OfflineJournalFile->DeleteJournalFile() )
			{
				CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
					"Journal file was deleted successfully" ) );
			}
			else
			{
				CLRTools::LogWarningMessage( String::Format(
					"GameAssetDatabase: Warning: Unable to delete the journal file from disk (possibly read-only?)" ) );
			}
		}
		else
		{
			// Unable to upload journal entries
			CLRTools::LogWarningMessage( String::Format(
				"GameAssetDatabase: Error: Couldn't upload offline journal entries to the server!" ) );
			return false;
		}
	}


	// Check for server-based journal entries that have expired and should be deleted.  We'll
	// delete them later on after we've actually processed them.
	if( ServerJournalEntries != nullptr )
	{
		for each( MGameAssetJournalEntry^ CurServerEntry in ServerJournalEntries )
		{
			// Check to see if we should delete this asset entry afterwards
			if( DeleteServerEntriesOlderThan != nullptr &&
				CurServerEntry->TimeStamp->Ticks < DeleteServerEntriesOlderThan->Ticks )
			{
				// Keep track of all of the journal indices that we've processed so we can delete them from
				// the server later.
				OutDatabaseIndicesToDelete->Add( CurServerEntry->DatabaseIndex );
			}
		}
	}


	// Create a final list of journal entries!
	MGameAssetJournalEntryList^ JournalEntries = gcnew MGameAssetJournalEntryList();
	{
		if( OfflineJournalEntries != nullptr )
		{
			// Append offline entries
			JournalEntries->AddRange( OfflineJournalEntries );
		}


		if( ServerJournalEntries != nullptr )
		{
			// Append server entries
			JournalEntries->AddRange( ServerJournalEntries );
		}


		// Sort all of the entries by their database timestamp index
		JournalEntries->Sort( gcnew Comparison< MGameAssetJournalEntry^ >( MGameAssetJournalEntry::JournalEntrySortDelegate ) );
	}



	int NumNewJournalEntries = 0;

	// Apply all of the journal entries to our mapping table, resolving conflicts along the way!
	for each( MGameAssetJournalEntry^ CurEntry in JournalEntries )
	{
		// NOTE: Currently conflicts are resolved by simply applying all changes in the order they occurred

		// Make sure the entry is valid (no problems loading.)
		//
		// Make sure that the entry was created by the requested user before loading it.  If we weren't
		// given a user name then allow all entries to be loaded
		//
		// For server-based entries, make sure the entry is new enough to bother loading
		if( CurEntry->IsValidEntry &&
			( RestrictUserName == nullptr || RestrictUserName == CurEntry->UserName ) &&
			( !CurEntry->IsOfflineEntry || ( CurEntry->TimeStamp->Ticks >= IgnoreServerEntriesAsOldAs->Ticks ) ) )
		{
			++NumNewJournalEntries;

			switch( CurEntry->Type )
			{
				case EJournalEntryType::AddTag:
					{
						AddTagMapping( CurEntry->AssetFullName, CurEntry->Tag, bIsAuthoritative );

						// If we're not authoritative (that is, we're downloading journal entries in
						// the editor), we'll make sure that new assets from the server are not tagged
						// as ghost, since there has clearly been new activity since the last time
						// our checkpoint file was updated.
						if( !bIsAuthoritative )
						{
							List<String^> AllAssetTags;
							QueryTagsForAsset( CurEntry->AssetFullName, ETagQueryOptions::SystemTagsOnly, %AllAssetTags );
							for each( String^ CurSystemTag in AllAssetTags )
							{
								ESystemTagType CurTagType = GetSystemTagType( CurSystemTag );
								if( CurTagType == ESystemTagType::Ghost )
								{
									RemoveTagMapping( CurEntry->AssetFullName, CurSystemTag );
								}
							}
						}
					}
					break;


				case EJournalEntryType::RemoveTag:
					{
						RemoveTagMapping( CurEntry->AssetFullName, CurEntry->Tag );
					}
					break;

				case EJournalEntryType::CreateTag:
					{
						LocalCreateTag( CurEntry->Tag );
					}
					break;

				case EJournalEntryType::DestroyTag:
					{
						LocalDestroyTag( CurEntry->Tag );
					}
					break;


				default:
					{
						// Unknown entry type.  We'll spew a warning and ignore the entry.
						CLRTools::LogWarningMessage( NAME_DevAssetDataBase, "LoadJournalData: Unrecognized Journal Entry Type (IGNORED)" );
					}
					break;
			}
		}
	}



	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
		"Merged {0} journal entries (including server entries newer than {1}.)",
		NumNewJournalEntries, IgnoreServerEntriesAsOldAs->ToString() ) );



	return true;
}

ref struct BranchAndGameCounter
{
	String^ BranchName;
	String^ GameName;
	int Counter;

	BranchAndGameCounter(String^ InBranch, String^ InGame)
	{
		BranchName = InBranch;
		GameName = InGame;
		Counter = 0;
	}
};

/** Loads all server journal data from all branches and games */
bool FGameAssetDatabase::LoadAllServerJournalData( DateTime^ DeleteServerEntriesOlderThan, List< int >^& OutDatabaseIndicesToDelete )
{
	OutDatabaseIndicesToDelete = gcnew List< int >();

	// Assuming we are not in offline mode
	// Try to connect to the SQL server and download ALL journal entries
	// even ones from different projects
	MGameAssetJournalEntryList^ ServerJournalEntries = nullptr;
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, "Connecting to the SQL database to download ALL journal data. (online mode)" );


		// Query all of the records we're interested in
		const bool bFilterBranchAndGame = false;
		if( !JournalClient->QueryJournalEntries( ServerJournalEntries, bFilterBranchAndGame ) )
		{
			// Couldn't query journal entries.  Reason is stored in ErrorMessageText
			return false;
		}


		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
			"Downloaded {0} journal entries from the SQL database.",
			ServerJournalEntries->Count ) );

		// We were able to connect to the server, so keep track of that so other systems can query it
		bIsJournalServerAvailable = TRUE;
	}

	List<BranchAndGameCounter^>^ AssetCounters = gcnew List<BranchAndGameCounter^>();

	// Check for server-based journal entries that have expired and should be deleted.  We'll
	// delete them later on after we've actually processed them.
	if( ServerJournalEntries != nullptr )
	{
		for each( MGameAssetJournalEntry^ CurServerEntry in ServerJournalEntries )
		{
			// Check to see if we should delete this asset entry afterwards
			if( DeleteServerEntriesOlderThan != nullptr &&
				CurServerEntry->TimeStamp->Ticks < DeleteServerEntriesOlderThan->Ticks )
			{
				// Count the number of tags per Branch and Game
				bool bIsUnique = true;
				for each (BranchAndGameCounter^ AssetCounter in AssetCounters)
				{
					if (String::Compare(AssetCounter->BranchName, CurServerEntry->BranchName, true) == 0 &&
						String::Compare(AssetCounter->GameName, CurServerEntry->GameName, true) == 0)
					{
						++AssetCounter->Counter;
						bIsUnique = false;
						break;
					}
				}
				if (bIsUnique)
				{
					AssetCounters->Add(gcnew BranchAndGameCounter(CurServerEntry->BranchName, CurServerEntry->GameName));
				}

				// Keep track of all of the journal indices that we've processed so we can delete them from
				// the server later.
				OutDatabaseIndicesToDelete->Add( CurServerEntry->DatabaseIndex );
			}
		}
	}

	// Print out the number of tags per Branch and Game, then print the total tags to be deleted
	int TotalTags = 0;
	for each (BranchAndGameCounter^ AssetCounter in AssetCounters)
	{
		TotalTags += AssetCounter->Counter;
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, "Branch/Game: " + AssetCounter->BranchName + "/" + AssetCounter->GameName + ", Tags: " + AssetCounter->Counter );
	}
	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, "Total Tags to Delete: " + TotalTags );

	return true;
}


/**
 * Adds or updates the default system tags for an asset in the local database
 *
 * @param	InAssetFullName			Asset full name
 * @param	InObjectTypeName		Object type name
 * @param	InOutermostPackageName	Object's outermost package
 * @param	bInIsArchetype			True if the object is an Archetype
 * @param	bSendToJournalIfNeeded	If true, default tags for new assets will be uploaded to the journal server
 */
void FGameAssetDatabase::SetDefaultTagsForAsset( String^ InAssetFullName, String^ InObjectTypeName, String^ InOutermostPackageName, bool bInIsArchetype, bool bSendToJournalIfNeeded )
{
	// First, remove any existing tags on the asset that we'll be replacing soon.  We only
	// want the asset to have one of each system tag that we're interested in.
	bool bWasAssetKnownAlready = false;
	bool bHasDateAddedTag = false;
	List< String^ >^ ExistingTags;
	QueryTagsForAsset(
		InAssetFullName,
		ETagQueryOptions::SystemTagsOnly,
		ExistingTags );
	for each( String^ CurTag in ExistingTags )
	{
		ESystemTagType TagType = GetSystemTagType( CurTag );
		if( TagType == ESystemTagType::ObjectType ||
			TagType == ESystemTagType::OutermostPackage ||
			TagType == ESystemTagType::Archetype )
		{
			// Take note that the asset already existed in our database (with type/package data), so there's no
			// need to submit that data to the journal server
			bWasAssetKnownAlready = true;

			// NOTE: Assets that were already in the originally-loaded checkpoint file will still retain tags.
			//       But, those objects will be marked as 'ghosts' by the checkpoint process so they won't be
			//		 be displayed in the editor (until the user actually checks in the asset.)
			RemoveTagMapping( InAssetFullName, CurTag );
		}

		if( TagType == ESystemTagType::DateAdded )
		{
			bHasDateAddedTag = true;
		}

		// We're creating or updating ghosts for an asset that exists in memory, so make sure to
		// remove the "ghost" tag locally if it has that
		if( TagType == ESystemTagType::Ghost )
		{
			RemoveTagMapping( InAssetFullName, CurTag );
		}
	}


	// Always update the local tables (in case we have no journal server or we're in read-only mode or something.)
	// We do this first to make sure that we create tags authoritatively when requested.

	// Create an "object type" system tag for this asset
	const bool bIsAuthoritative = true;
	String^ ObjectTypeTag = MakeSystemTag( ESystemTagType::ObjectType, InObjectTypeName );
	AddTagMapping( InAssetFullName, ObjectTypeTag, bIsAuthoritative );

	// Create an "outermost package" system tag for this asset
	// We'll store the package name as clean file name (no path or extension)
	String^ OutermostPackageTag = MakeSystemTag( ESystemTagType::OutermostPackage, InOutermostPackageName );
	AddTagMapping( InAssetFullName, OutermostPackageTag, bIsAuthoritative );

	// Archetype assets get tagged with a special system tag so we can identify them later
	String^ ArchetypeTag = MakeSystemTag( ESystemTagType::Archetype, "" );
	if( bInIsArchetype )
	{
		AddTagMapping( InAssetFullName, ArchetypeTag, bIsAuthoritative );
	}


	// Make sure that the asset has a 'date added' tag if it's missing one
	if( !bHasDateAddedTag )
	{
		// We'll only store the date (not the time), as we don't want to clutter our tag set too much
		// with every single asset always having a different DateAdded tag
		DateTime^ CurrentDate = DateTime::Today;
		String^ DateAddedString = CurrentDate->Ticks.ToString();
		String^ DateAddedTag = MakeSystemTag( ESystemTagType::DateAdded, DateAddedString );
		AddTagMapping( InAssetFullName, DateAddedTag, bIsAuthoritative );
	}



	// If this is a new asset that we haven't seen locally before, send the system tags for the
	// asset to the GAD journal server so that other users will be able to see the package/type of
	// yet-unverified assets
	if( !bWasAssetKnownAlready && bSendToJournalIfNeeded )
	{
		List< String^ > AssetsToTag;
		AssetsToTag.Add( InAssetFullName );
		AddTagToAssets( %AssetsToTag, ObjectTypeTag );
		AddTagToAssets( %AssetsToTag, OutermostPackageTag );
		if( bInIsArchetype )
		{
			AddTagToAssets( %AssetsToTag, ArchetypeTag );
		}
	}
}



/** Loads asset names from a specific package.  Returns number of assets found. */
int FGameAssetDatabase::GatherAssetsFromPackageLinker( ULinkerLoad* Linker, const TSet< FName >& InAllowedClasses, TSet< FName >& InOutCurrentlyExistingAssets )
{
	check( Linker != NULL );


	int NumAssetsFound = 0;
	for( INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
	{
		const FObjectExport& Export = Linker->ExportMap(ExportIndex);

		
		// Grab the object class
		const FName ClassFName = Linker->GetExportClassName( ExportIndex );
		const FName ClassPackageFName = Linker->GetExportClassPackage( ExportIndex );
		UClass* ObjectClass = FindObject<UClass>( ANY_PACKAGE, *ClassFName.ToString(), TRUE );
		if( ObjectClass == NULL )
		{
			ObjectClass = LoadObject<UClass>( NULL, *( ClassPackageFName.ToString() + "." + ClassFName.ToString() ), NULL, LOAD_None, NULL );
		}


		if( ObjectClass != NULL )
		{
			// @todo CB: This check isn't consistent with the check we perform for loaded assets in the browser!
			//		See use of RF_Archetype and UGenericBrowserType_Archetype::IsArchetypeSupported in GenericBrowserTypes.cpp
			const bool bIsArchetype = ( Export.ObjectFlags & RF_ArchetypeObject ) == RF_ArchetypeObject;

			// We'll use the object if it's class is in our "allowed classes" list or if it's an archetype
			bool bIsAllowedClass = 
				InAllowedClasses.Contains( ClassFName ) ||
				bIsArchetype;

			bool bIsMapPackage = ( Linker->Summary.PackageFlags & PKG_ContainsMap ) == PKG_ContainsMap;
			bool bIsAssetPackage = !bIsMapPackage;

			// Make sure this is on our list of allowed classes
			if( bIsAllowedClass &&
				( bIsAssetPackage || ( ( Export.ObjectFlags & RF_Standalone ) == RF_Standalone ) ) &&	// Ignore non-standalone objects
				( bIsAssetPackage || ( ( Export.ObjectFlags & RF_Public ) == RF_Public ) ) &&			// Ignore non-public objects
				( ( Export.ObjectFlags & RF_ClassDefaultObject ) == 0 ) && 		// Ignore class default objects
				( Export.SuperIndex == NULLSUPER_INDEX ) )						// Ignore UObjects embedded in structs
			{
				// Grab the asset name
				String^ AssetPath = CLRTools::ToString( Linker->GetExportPathName( ExportIndex ) );
				String^ AssetClassName = CLRTools::ToString( *ClassFName.ToString() );

				// Build a full name string for this asset
				String^ AssetFullName = UnrealEd::Utils::MakeFullName( AssetClassName, AssetPath );

				// Hack to ignore "Fallback" materials.  We never want to show those to the user.
				//             -> See: WxMaterialEditor::OnPropagateToFallback for more info
				// @todo CB: If we ever bother to load the asset, we could test for this properly!
				// NOTE: We use a Contains() search instead of EndsWith() to catch duplicated fallback materials
				//			e.g. "Foobar_Fallback_Dup_Dup"
				bool bIsFallbackMaterial =
					( ObjectClass->IsChildOf( UMaterial::StaticClass() ) && AssetFullName->Contains( "_Fallback" ) );

				// Ignore objects embedded inside of other UObjects (such as font textures)
				bool bIsEmbeddedObject = AssetPath->IndexOf( SUBOBJECT_DELIMITER_CHAR ) != INDEX_NONE;
				
				if( !bIsEmbeddedObject )
				{
					// OK, great, we found a valid *browsable* asset.  Add it to our set.
					FName AssetFullNameFName = CLRTools::ToFName( AssetFullName );
					InOutCurrentlyExistingAssets.Add( AssetFullNameFName );


					// Add or update the default system tags for this asset
					const bool bSendToJournalIfNeeded = false;
					SetDefaultTagsForAsset(
						AssetFullName,									// Name of asset
						AssetClassName,									// Object type name
						CLRTools::ToString( FFilename( Linker->GetArchiveName() ).GetBaseFilename() ),	// Outermost package name
						bIsArchetype,									// Is archetype?
						bSendToJournalIfNeeded );						// Update the journal server?


					++NumAssetsFound;
				}
			}
		}
	}

	return NumAssetsFound;
}


/**
 * Get a list of asset classes which are valid for displaying in the content browser.
 * The list will be generated on demand the first time this method is called.
 *
 *  @return	the list of class names which are valid asset types
 */
const TSet<FName>& FGameAssetDatabase::GetAllowedClassList( )
{
	static TSet< FName > AllowedClassNames;
	if ( AllowedClassNames.Num() == 0 )
	{
		GenerateAllowedClassList( AllowedClassNames );
	}

	return AllowedClassNames;
}

/**
 * Utility method for generating a list of asset classes which are valid for displaying in the content browser.
 *
 * @param	AllowedClassesNames		receives the list of class names which are valid asset types
 */
void FGameAssetDatabase::GenerateAllowedClassList( TSet<FName>& AllowedClassesNames )
{
	TSet< UClass* > AllowedClasses;
	for( TObjectIterator<UClass> ItC ; ItC ; ++ItC )
	{
		const UBOOL bIsAllType = (*ItC == UGenericBrowserType_All::StaticClass());
		const UBOOL bIsCustomType = (*ItC == UGenericBrowserType_Custom::StaticClass());

		if( !bIsAllType && !bIsCustomType && ItC->IsChildOf(UGenericBrowserType::StaticClass()) && !(ItC->ClassFlags&CLASS_Abstract) )
		{
			UGenericBrowserType* ResourceType = ConstructObject<UGenericBrowserType>( *ItC );
			if( ResourceType )
			{
				ResourceType->Init();

				// Each resource type may be associated with multiple classes
				for( INT CurSupportInfo = 0; CurSupportInfo < ResourceType->SupportInfo.Num(); ++CurSupportInfo )
				{
					UClass* Class = ResourceType->SupportInfo( CurSupportInfo ).Class;

					// Ignore UObjects (archetypes) since everything is a child of UObject
					// Also ignore UUISequences (see UGenericBrowserType_Sequence::IsSequenceTypeSupported)
					if( UObject::StaticClass()->GetPureName() != Class->GetPureName() )
					{
						AllowedClasses.Add( Class );
						// debugf( TEXT( "Found class: %s" ), *Class->GetName() );
					}
				}
			}
		}
	}


	// Do a second pass and make sure that we have all of the child classes accounted for, too!
	{
		TArray< UClass* > FoundChildClasses;
		for( TObjectIterator<UClass> ChildClassIt; ChildClassIt; ++ChildClassIt )
		{
			UClass* PotentialChildClass = *ChildClassIt;
			for( TSet< UClass* >::TConstIterator ParentClassIt( AllowedClasses ); ParentClassIt; ++ParentClassIt )
			{
				UClass* ParentClass = *ParentClassIt;
				if( PotentialChildClass->IsChildOf( ParentClass ) )
				{
					FoundChildClasses.AddItem( PotentialChildClass );
					// debugf( TEXT( "Added child class: %s (parent: %s)" ), *PotentialChildClass->GetName(), *ParentClass->GetName() );
				}
			}
		}

		for( INT CurChildClassIndex = 0; CurChildClassIndex < FoundChildClasses.Num(); ++CurChildClassIndex )
		{
			AllowedClasses.Add( FoundChildClasses( CurChildClassIndex ) );
		}
	}

	// Build set of class FNames
	for( TSet< UClass* >::TConstIterator ClassIt( AllowedClasses ); ClassIt; ++ClassIt )
	{
		UClass* CurClass = *ClassIt;
		AllowedClassesNames.Add( FName( *CurClass->GetName() ) );
	}
}



/** Globally renames the specified asset to the specified new name */
void FGameAssetDatabase::RenameAssetInAllTagMappings( String^ OldAssetName, String^ NewAssetName )
{
	// Make sure the source asset name exists
	const FName OldAssetFName( CLRTools::ToFName( OldAssetName ) );
	check( AssetToTagMap.ContainsKey( OldAssetFName ) );

	// Make sure the target asset name doesn't already exist anywhere
	const FName NewAssetFName( CLRTools::ToFName( NewAssetName ) );
	check( !AssetToTagMap.ContainsKey( NewAssetFName ) );


	// Create the new asset and copy all of the tags over
	TArray< FName > FoundTags;
	AssetToTagMap.FindValuesForKey( OldAssetFName, FoundTags );
	
	for( INT CurTagIndex = 0; CurTagIndex < FoundTags.Num(); ++CurTagIndex )
	{
		const bool bIsAuthoritative = true;
		AddTagMapping( NewAssetFName, FoundTags( CurTagIndex ), bIsAuthoritative );
	}

	// Remove the old asset
	LocalDestroyAsset( OldAssetName );	
}



/** Fixes any broken asset names in the database (backwards compatibility) */
void FGameAssetDatabase::FixBrokenAssetNames()
{
	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
		"Fixing up any broken asset names..." ) );

	int NumWarnings = 0;
	int NumRepairs = 0;
	int NumDeletes = 0;


	// Make sure that default tags exist for all assets
	{
		TLookupMap< FName > AssetFullNames;
		AssetToTagMap.GetKeys( AssetFullNames );	// Out
		for( TLookupMap< FName >::TConstIterator CurAssetIt( AssetFullNames ); CurAssetIt; ++CurAssetIt )
		{
			const FName& CurAssetFullNameFName = CurAssetIt.Key();
			String^ CurAssetFullName = CLRTools::ToString( CurAssetFullNameFName.ToString() );


			// Check to see if this asset is missing it's class name
			if( CurAssetFullName->IndexOf( " " ) == -1 )
			{
				++NumWarnings;

				// Uh oh, there's no class name on this asset.  Try to figure out it's object type.
				String^ CurAssetPath = CurAssetFullName;
				String^ AssetClassName = "";
				{
					List< String^ >^ ExistingTags;
					QueryTagsForAsset( CurAssetPath, ETagQueryOptions::SystemTagsOnly, ExistingTags );
					for each( String^ CurTag in ExistingTags )
					{
						// Is this a system tag?
						if( IsSystemTag( CurTag ) )
						{
							if( GetSystemTagType( CurTag ) == ESystemTagType::ObjectType )
							{
								AssetClassName = GetSystemTagValue( CurTag );
								break;
							}
						}
					}
				}

				if( AssetClassName->Length > 0 )
				{
					// Great, we found the class name!
					CurAssetFullName = AssetClassName + " " + CurAssetPath;

					// Repair the database by globally renaming this asset
					RenameAssetInAllTagMappings( CurAssetPath, CurAssetFullName );
					++NumRepairs;
				}
				else
				{
					// No asset type for this asset, so kill it!
					LocalDestroyAsset( CurAssetPath );
					++NumDeletes;
				}			
			}
		}
	}

	if( NumWarnings > 0 )
	{
		CLRTools::LogWarningMessage( String::Format(
			"Found {0} broken asset names (fixed {1}, deleted {2})",
			NumWarnings, NumRepairs, NumDeletes ) );
	}
}



/** Verifies the integrity of the asset database */
void FGameAssetDatabase::VerifyIntegrityOfDatabase( UBOOL bShouldTryToRepair )
{
	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
		"Checking integrity of game asset database..." ) );

	int NumWarnings = 0;
	int NumRepairs = 0;


	// Make sure that default tags exist for all assets
	{
		List< String^ > AssetsToRemove;

		TLookupMap< FName > AssetFullNames;
		AssetToTagMap.GetKeys( AssetFullNames );	// Out
		for( TLookupMap< FName >::TConstIterator CurAssetIt( AssetFullNames ); CurAssetIt; ++CurAssetIt )
		{
			const FName& CurAssetFullNameFName = CurAssetIt.Key();
			String^ CurAssetFullName = CLRTools::ToString( CurAssetFullNameFName.ToString() );

			// Grab all of the tags for this asset
			List< String^ >^ ExistingTags;
			QueryTagsForAsset( CurAssetFullName, ETagQueryOptions::AllTags, ExistingTags );


			bool bHasObjectTypeSystemTag = false;
			bool bHasOutermostPackageSystemTag = false;

			for each( String^ CurTag in ExistingTags )
			{
				// Make sure Known Tags contains all of the tags.  The Known Tags list also contains Collections
				// too so we'll check those as well.
				if( IsCollectionTag( CurTag, EGADCollection::Private ) ||
					IsCollectionTag( CurTag, EGADCollection::Shared ) ||
					!IsSystemTag( CurTag ) )
				{
					FName CurTagFName( CLRTools::ToFName( CurTag ) );
					if( !KnownTags.Contains( CurTagFName ) )
					{
						CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "Warning: Tag {0} on asset {1} was missing from the KnownTags set", CurTag, CurAssetFullName ) );
						++NumWarnings;

						if( bShouldTryToRepair )
						{
							// Add to the list of known tags
							KnownTags.Add( CurTagFName );
							++NumRepairs;

							CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "       -> Repaired (updated the KnownTags set)" ) );
						}
					}
				}

				// Is this a system tag?
				if( IsSystemTag( CurTag ) )
				{
					ESystemTagType TagType = GetSystemTagType( CurTag );
					switch( TagType )
					{
						case ESystemTagType::ObjectType:
							{
								bHasObjectTypeSystemTag = true;
							}
							break;

						case ESystemTagType::OutermostPackage:
							{
								bHasOutermostPackageSystemTag = true;
							}
							break;

						default:
							// Some other system tag
							break;
					}
				}
			}

			if( !bHasObjectTypeSystemTag )
			{
				CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "    Warning: Asset {0} is missing a default system tag (ObjectType)", CurAssetFullName ) );
				++NumWarnings;

				if( bShouldTryToRepair )
				{
					// Add the object to our list of assets to remove
					++NumRepairs;
					if( !AssetsToRemove.Contains( CurAssetFullName ) )
					{
						AssetsToRemove.Add( CurAssetFullName );
					}

					CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "       -> Repaired (removed the asset)" ) );
				}
			}

			if( !bHasOutermostPackageSystemTag )
			{
				CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "    Warning: Asset {0} is missing a default system tag (OutermostPackage)", CurAssetFullName ) );
				++NumWarnings;

				if( bShouldTryToRepair )
				{
					// Add the object to our list of assets to remove
					++NumRepairs;
					if( !AssetsToRemove.Contains( CurAssetFullName ) )
					{
						AssetsToRemove.Add( CurAssetFullName );
					}

					CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "       -> Repaired (removed the asset)" ) );
				}
			}
		}

		// Remove any bad assets we found
		for each( String^ CurAssetFullName in AssetsToRemove )
		{
			LocalDestroyAsset( CurAssetFullName );
		}
	}


	
	// Make sure "KnownTags" doesn't contain any system tags other than collection system tags.  It should only
	// have user tags and collection system tags in it!

	// @todo: Make sure no embedded objects are in the database (e.g. font textures)

	// @todo: Make sure no objects from map packages are in the database

	// @todo: Make sure the hash tables are in sync



	if( NumWarnings > 0 )
	{
		if( NumRepairs == NumWarnings )
		{
			CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
				"Integrity check complete.  Found and repaired all {0} problems.", NumWarnings ) );
		}
		else
		{
			CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
				"Integrity check complete.  Found {0} problems and repaired {1} of them.", NumWarnings, NumRepairs ) );
			if( !bShouldTryToRepair )
			{
				CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
					"    Use the '-Repair' option to automatically fix these problems." ) );
			}
		}
	}
	else
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
			"Integrity check completed with no warnings." ) );
	}
}



/** Rebuilds all default tags for the entire game */
void FGameAssetDatabase::RebuildDefaultTags( TSet< FName >& OutCurrentlyExistingAssets )
{
	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "Updating database from content packages..." ) );


	// Create the list of available resource types.
	const TSet< FName >& AllowedClassesNames = GetAllowedClassList();

	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "    {0} object class types will be processed.", AllowedClassesNames.Num() ) );


	// Scan packages
	TArray< FString > FilesToLoad( GPackageFileCache->GetPackageFileList() );

	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( "    Found {0} packages.", FilesToLoad.Num() ) );

	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();
	
	// Cull out any non-"INT" localized packages
	int NumLocPackages = 0;
	for( INT CurPackageIndex = 0; CurPackageIndex < FilesToLoad.Num(); ++CurPackageIndex )
	{
		const FFilename Filename( FilesToLoad( CurPackageIndex ) );
		FString BaseFilename = Filename.GetBaseFilename();

		if ( !UnrealEd::Utils::IsPackageValidForTree(CLRTools::ToString(BaseFilename)) )
		{
			FilesToLoad.Remove(CurPackageIndex--);
			continue;
		}

		for( INT CurLanguageIndex = 0; CurLanguageIndex < KnownLanguageExtensions.Num(); ++CurLanguageIndex )
		{
			// Ignore INT language packages
			if( appStricmp( TEXT( "INT" ), *KnownLanguageExtensions( CurLanguageIndex ) ) )
			{
				FString LanguageExt( TEXT( "_" ) );
				LanguageExt += KnownLanguageExtensions( CurLanguageIndex );

				if( BaseFilename.EndsWith( LanguageExt ) )
				{
					// Remove this package from our list
					FilesToLoad.RemoveSwap( CurPackageIndex );
					++NumLocPackages;
				}
			}
		}
	}


	if( NumLocPackages > 0 )
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
			"    Ignoring {0} packages with localization-related file names.",
			NumLocPackages ) );
	}


	int NumAssetsFound = 0;
	int NumPackagesWithAssets = 0;
	OutCurrentlyExistingAssets.Empty();
	for( INT FileIndex = 0; FileIndex < FilesToLoad.Num(); FileIndex++ )
	{
		const FString& Filename = FilesToLoad(FileIndex);

		GWarn->StatusUpdatef( (INT)FileIndex, (INT)FilesToLoad.Num(), 
			*FString::Printf( LocalizeSecure(LocalizeUnrealEd("GameAssetDatabase_CheckPointFile_VerifyIntegrityProgressBarDetail"), *Filename)));

		// Load up the linker for this package
		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
		UObject::EndLoad();

		
		// Now harvest the assets!  As we're gathering assets we'll also make sure that all browsable
		// assets have default system tags
		if( Linker )
		{
			int NumAssetsFoundInPackage = GatherAssetsFromPackageLinker( Linker, AllowedClassesNames, OutCurrentlyExistingAssets );
			if( NumAssetsFoundInPackage > 0 )
			{
				++NumPackagesWithAssets;

				NumAssetsFound += NumAssetsFoundInPackage;
			}
		}


		// Collect garbage every so often
		if( FileIndex % 16 == 0 )
		{
			UObject::CollectGarbage( RF_Native );
		}
	}


	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
		"    Found a total of {0} assets in {1} packages.",
		NumAssetsFound, NumPackagesWithAssets ) );

}



/** Searches for assets in the database that don't exist on disk and warns about (or purges) them */
void FGameAssetDatabase::CheckForGhostAssets( TSet< FName >& CurrentlyExistingAssets, const FGameAssetDatabaseStartupConfig &InConfig )
{
	CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
		"Checking for assets in the database that don't actually exist on disk..." ) );

	UINT NumGhostAssets = 0;

	TLookupMap< FName > AssetFullNames;
	AssetToTagMap.GetKeys( AssetFullNames );	// Out
	for( TLookupMap< FName >::TConstIterator CurAssetIt( AssetFullNames ); CurAssetIt; ++CurAssetIt )
	{
		const FName& CurAssetFullNameFName = CurAssetIt.Key();
		if( !CurrentlyExistingAssets.Contains( CurAssetFullNameFName ) )
		{
			// Asset doesn't exist on disk!
			String^ CurAssetFullName = CLRTools::ToString( CurAssetFullNameFName.ToString() );

			bool IsGhostAsset = false;
			String^ GhostAssetTimeStampString;

			// Grab the tags assigned to this asset to help us with debugging
			Text::StringBuilder AssetTagsString;
			{
				List< String^ >^ Tags;
				QueryTagsForAsset( CurAssetFullName, ETagQueryOptions::AllTags, Tags );

				for each( String^ CurTag in Tags )
				{
					// Check to see if this asset is already marked as a "ghost"
					if( IsSystemTag( CurTag ) && GetSystemTagType( CurTag ) == ESystemTagType::Ghost )
					{
						IsGhostAsset = true;
						GhostAssetTimeStampString = GetSystemTagValue( CurTag );
					}

					if( AssetTagsString.Length > 0 )
					{
						AssetTagsString.Append( ", " );
					}
					AssetTagsString.Append( CurTag );
				}
			}


			// If we were asked to purge assets that don't actually exist on disk then we'll do that now
			if( InConfig.bPurgeNonExistentAssets )
			{
				CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
					"    Purging non-existent asset: {0} (Tags: {1})",
					CurAssetFullName, AssetTagsString.ToString() ) );

				LocalDestroyAsset( CurAssetFullName );
			}
			else
			{
				DateTime^ CurrentDateTime = DateTime::Now;
				if( IsGhostAsset )
				{
					CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
						"    Warning: Non-existent 'Ghost' asset found: {0} (Tags: {1})",
						CurAssetFullName, AssetTagsString.ToString() ) );

					// Check to see if we need to expire this "ghost" asset
					const UINT64 GhostAssetTimeStampTickCount = Convert::ToUInt64( GhostAssetTimeStampString );
					DateTime^ GhostAssetTimeStamp = gcnew DateTime( GhostAssetTimeStampTickCount );

					DateTime^ AssetExpiryDate = GhostAssetTimeStamp->AddDays( GADDefs::DeleteGhostAssetsOlderThanDays );				
					if( CurrentDateTime->Ticks > AssetExpiryDate->Ticks )
					{
						// Expire this asset!  All tags/collections will be wiped.  This asset was probably deleted,
						// but might also be a new asset that was tagged but never submitted to source control.
						LocalDestroyAsset( CurAssetFullName );

						CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format( 
							"       -> Deleted this expired 'Ghost' asset" ) );
					}
					else
					{
						// Ghost asset hasn't expired yet, so we'll just print a warning about that
						int DaysUntilExpiration = TimeSpan( AssetExpiryDate->Ticks - CurrentDateTime->Ticks ).Days + 1;
						CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
							"       -> 'Ghost' asset will expire in {0} days",
							DaysUntilExpiration ) );
					}
				}
				else
				{
					// Add a "ghost" system tag to this asset so the editor will know that it doesn't
					// actually exist on disk and can hint the user.  If the asset ever shows up again
					// in a future run, we'll remove this tag from it.
					CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
						"    Warning: Non-existent asset found and tagged as 'Ghost': {0} (Tags: {1})",
						CurAssetFullName, AssetTagsString.ToString() ) );

					// Stamp the current time with the ghost tag so that we can expire assets that
					// have been MIA for a number of days (deleted, or a user tagged but never bothered to checkin.)
					bool bIsAuthoritative = true;
					GhostAssetTimeStampString = CurrentDateTime->Ticks.ToString();
					AddTagMapping( CurAssetFullName, MakeSystemTag( ESystemTagType::Ghost, GhostAssetTimeStampString ), bIsAuthoritative );

					IsGhostAsset = true;
				}
			}

			++NumGhostAssets;
		}
	}

	if( NumGhostAssets > 0 )
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
			"Warning: {0} assets found in database do not exist on disk!",
			NumGhostAssets ) );
	}
	else
	{
		CLRTools::LogWarningMessage( NAME_DevAssetDataBase, String::Format(
			"All {0} assets were found on disk.",
			AssetFullNames.Num() ) );
	}
}



