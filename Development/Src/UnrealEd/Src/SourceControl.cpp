/*=============================================================================
	SourceControl.cpp: Interface for talking to source control clients
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "SourceControl.h"

#if HAVE_SCC

/** Derived implementation of a source control provider */
FSourceControlProvider FSourceControl::StaticSourceControlProvider;
/**Queue for commands given by the main thread*/
TArray <FSourceControlCommand*> FSourceControl::CommandQueue;
/**Unique index to use for next command*/
INT FSourceControl::NextCommandID = 0;

/** Key strings for use in looking up values in the map returned by SCC history commands (NOTE: THIS SHOULD BE KEPT UP TO DATE WITH EFILEHISTORYKEYS) */
const TCHAR* FSourceControl::FSourceControlFileHistoryInfo::FILE_HISTORY_KEYS[EFH_MAX] = 
{
	TEXT("FileHistoryInfo"),
	TEXT("NumRevisions")
	TEXT("FileName"),
	TEXT("RevisionNumbers"),
	TEXT("UserNames"),
	TEXT("Dates"),
	TEXT("ChangelistNumbers"),
	TEXT("Descriptions"),
	TEXT("FileSizes"),
	TEXT("ClientSpecs"),
	TEXT("Actions")
};

/** String used to delimit history items compacted into a string */
const TCHAR* FSourceControl::FSourceControlFileHistoryInfo::FILE_HISTORY_ITEM_DELIMITER = TEXT("|SCC|");

/** Constructor */
FSourceControl::FSourceControlFileRevisionInfo::FSourceControlFileRevisionInfo()
:	Description(""),
	UserName(""),
	ClientSpec(""),
	Date( 0 ),
	RevisionNumber( 0 ),
	ChangelistNumber( 0 ),
	FileSize( 0 )
{}

/** Constructor */
FSourceControl::FSourceControlFileHistoryInfo::FSourceControlFileHistoryInfo()
:	FileName("")
{}

/**
 * Helper method which returns the key string associated with the provided enum value
 *
 * @param	KeyToGet	Enum value of the requested key string
 *
 * @return	Key string representing the provided enum value
 */
const TCHAR* FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( EFileHistoryKeys KeyToGet )
{
	check( KeyToGet >= 0 && KeyToGet < EFH_MAX );
	return FILE_HISTORY_KEYS[KeyToGet];
}

//------------------------------------------
//Main (Static) Source Control Interface
//------------------------------------------
/** Inits internals and the connection with the source control server */
void FSourceControl::Init (void)
{
	//init source control internals, ask for login, etc (MUST be done on the main thread)
	StaticSourceControlProvider.Init();
}

/** Closes the connection with the source control server */
void FSourceControl::Close (void)
{
	StaticSourceControlProvider.Close();
}

/** 
 * Utility function to convert raw package names to absolute pathes
 * @param InOutPackageNames - Array of package names to be converted to absolute path names 
 */
void FSourceControl::ConvertPackageNamesToSourceControlPaths(TArray<FString>& InOutPackageNames)
{
	//looping backwards so potential remove has no side effects
	for (INT i = InOutPackageNames.Num()-1; i >= 0; --i)
	{
		FString AbsoluteFileName;
		GPackageFileCache->FindPackageFile(*InOutPackageNames(i), NULL, AbsoluteFileName);
		if (AbsoluteFileName.Len())
		{
			InOutPackageNames(i) = appConvertRelativePathToFull(AbsoluteFileName);;
		} 
		else
		{
			//invalid package name....remove?
			InOutPackageNames.Remove(i);
		}
	}
}


/** 
 * Check out multiple files (Synchronous)
 * @param InLocalFileNames - Array of files to check out
 */
UBOOL FSourceControl::CheckOut(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames, UBOOL bSilent )
{
	// Make sure we have the final status of the files so we do not check out anything that isnt at head revision or checked out by another.
	TMap<FString, INT> FileNameToStatusMap;
	ForceGetStatus( InLocalFileNames, FileNameToStatusMap );

	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::CHECK_OUT;

	
	FString ErrorString;
	for( TMap<FString,INT>::TConstIterator It( FileNameToStatusMap); It; ++It )
	{
		// Ensure header files from make can always be checked out.
		if( It.Value() == SCC_ReadOnly || GIsUCCMake )
		{
			Command.Params.AddItem( It.Key() );
		}
		else if( It.Value() != SCC_CheckedOut )
		{
			// Generate an error string for all files that could not be checked out.
			// In the event that the file is already checked out to the current user, do nothing.
			ErrorString += (It.Key() + TEXT("\n")); 
		}
	}

	if( Command.Params.Num() > 0 )
	{
		ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_CheckOut"));

		//update the event listener with the new states of these files
		IssueUpdateState(InEventListener, InLocalFileNames);
	}

	if( ErrorString.Len() > 0 && !bSilent )
	{
		ErrorString = FString::Printf( TEXT("The following packages could not be checked out:\n%s"), *ErrorString );
		appMsgf( AMT_OK, *ErrorString );
	}
	return Command.bCommandSuccessful;
}

/** 
 * Check out files (Synchronous) and issues an ASYNC update state
 * @param InPackage - Package To CheckOut
 * @param bSilent - If true, will not warn about any errors 
 */
UBOOL FSourceControl::CheckOut(UPackage* InPackage, UBOOL bSilent)
{
	TArray<FString> CheckOutNames;
	CheckOutNames.AddItem(InPackage->GetName());
	FSourceControl::ConvertPackageNamesToSourceControlPaths(CheckOutNames);
	return FSourceControl::CheckOut(NULL, CheckOutNames, bSilent );
}


/** 
 * Checks in files (Synchronous) and issues an ASYNC update state
 * @param InEventListener - Object to receive the SourceControlCallback command
 * @param InAddFileNames - Files that need to be added first
 * @param InSubmitFileNames - Files that need only be submitted (not added)
 */
UBOOL FSourceControl::CheckIn(FSourceControlEventListener* InEventListener, const TArray<FString>& InAddFileNames, const TArray<FString>& InSubmitFileNames, const FString& InDesc)
{
	UBOOL bSuccess = TRUE;

	//create command to send to the source control thread
	if (InAddFileNames.Num())
	{
		FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
		Command.Description = InDesc;
		Command.CommandType = SourceControl::ADD_FILE;
		Command.Params = InAddFileNames;

		//NEW FILES
		ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_Add"));

		bSuccess = bSuccess && Command.bCommandSuccessful;
	}

	//OPEN FILES
	if (InSubmitFileNames.Num())
	{
		FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
		Command.Description = InDesc;
		Command.CommandType = SourceControl::CHECK_IN;
		Command.Params = InSubmitFileNames;

		ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_CheckIn"));

		bSuccess = bSuccess && Command.bCommandSuccessful;
	}

	//update the event listener with the new states of these files
	IssueUpdateState(InEventListener, InAddFileNames);
	//update the event listener with the new states of these files
	IssueUpdateState(InEventListener, InSubmitFileNames);

	return bSuccess;
}

/** 
 * Add files (Synchronous) and issues an ASYNC update state
 * @param InEventListener - Object to receive the SourceControlCallback command
 * @param InLocalFileNames - Array of files to check out
 */
UBOOL FSourceControl::Add(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames)
{
	//SOURCE_CONTROL - NEEDED Add Dialog
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::ADD_FILE;
	Command.Params = InLocalFileNames;

	ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_Add"));

	//update the event listener with the new states of these files
	IssueUpdateState(InEventListener, InLocalFileNames);

	return Command.bCommandSuccessful;
}



/** 
 * Add the specified files to the default changelist (Synchronous) and issues an ASYNC update state
 *
 * @param InEventListener - Object to receive the SourceControlCallback command
 * @param InLocalFileNames - Array of files to check out
 */
UBOOL FSourceControl::AddToDefaultChangelist(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames)
{
	//SOURCE_CONTROL - NEEDED Add Dialog
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::ADD_TO_DEFAULT_CHANGELIST;
	Command.Params = InLocalFileNames;

	ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_Add"));

	//update the event listener with the new states of these files
	IssueUpdateState(InEventListener, InLocalFileNames);

	return Command.bCommandSuccessful;
}



/** 
 * Delete files (Synchronous) and issues an ASYNC update state
 * @param InEventListener - Object to receive the SourceControlCallback command
 * @param InLocalFileNames - Array of files to check out
 */
UBOOL FSourceControl::Delete(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames)
{
	//SOURCE_CONTROL - NEEDED Delete Dialog?
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::DELETE_FILE;
	Command.Params = InLocalFileNames;

	ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_Delete"));

	//update the event listener with the new states of these files
	IssueUpdateState(InEventListener, InLocalFileNames);

	return Command.bCommandSuccessful;
}

/** 
 * Reverts files (Synchronous) and issues an ASYNC update state
 * @param InEventListener - Object to receive the SourceControlCallback command
 * @param InLocalFileNames - Array of files to check out
 */
UBOOL FSourceControl::Revert(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames)
{
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::REVERT;
	Command.Params = InLocalFileNames;

	ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_Revert"));

	//update the event listener with the new states of these files
	IssueUpdateState(InEventListener, InLocalFileNames);

	return Command.bCommandSuccessful;
}


/** 
 * Reverts files (Synchronous) files that have not changed and issues an SYNC update state
 * @param InEventListener - Object to receive the SourceControlCallback command
 * @param InLocalFileNames - Array of files to check out
 */
UBOOL FSourceControl::RevertUnchanged(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames)
{
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::REVERT_UNCHANGED;
	Command.Params = InLocalFileNames;

	ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_RevertUnchanged"));

	FSourceControlCommand UpdateCommand(InEventListener, &StaticSourceControlProvider);
	UpdateCommand.CommandType = SourceControl::UPDATE_STATUS;
	UpdateCommand.Params = InLocalFileNames;

	ExecuteSynchronousCommand(UpdateCommand, NULL/**LocalizeUnrealEd("SourceControl_UpdateStatus")*/);

	if ( InEventListener )
	{
		UpdateCommand.EventListener->SourceControlCallback(&UpdateCommand);
	}

	return Command.bCommandSuccessful && UpdateCommand.bCommandSuccessful;
}

/**
 * Queries for files (Synchronous) that are open by the user and have been modified from the version stored in source control.
 * @param	InEventListener		Object to receive the SourceControlCallback command
 * @param	InLocalFileNames	Array of files to query with, if any are specified, the query will be made against only the specified files;
 *								If none are specified, all open files will be queried.
 * @param	OutModifiedFiles	Array of files that are modified from the version stored in source control, if any.
 *
 * @return	TRUE if the command was successful, FALSE otherwise
 */
UBOOL FSourceControl::GetFilesModifiedFromServer(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames, TArray<FString>& OutModifiedFiles )
{
	FSourceControlCommand Command( InEventListener, &StaticSourceControlProvider );
	Command.CommandType = SourceControl::GET_MODIFIED_FILES;
	Command.Params = InLocalFileNames;
	
	ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_GetModifiedFiles"));

	Command.Results.GenerateKeyArray( OutModifiedFiles );
	
	return Command.bCommandSuccessful;
}

/**
 * Queries for files (Synchronous) that are open by the user and have not been modified from the version stored in source control.
 * @param	InEventListener		Object to receive the SourceControlCallback command
 * @param	InLocalFileNames	Array of files to query with, if any are specified, the query will be made against only the specified files;
 *								If none are specified, all open files will be queried.
 * @param	OutUnmodifiedFiles	Array of files that are unmodified from the version stored in source control, if any.
 *
 * @return	TRUE if the command was successful, FALSE otherwise
 */
UBOOL FSourceControl::GetFilesUnmodifiedFromServer(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames, TArray<FString>& OutUnmodifiedFiles )
{
	FSourceControlCommand Command( InEventListener, &StaticSourceControlProvider );
	Command.CommandType = SourceControl::GET_UNMODIFIED_FILES;
	Command.Params = InLocalFileNames;

	ExecuteSynchronousCommand(Command, *LocalizeUnrealEd("SourceControl_GetUnmodifiedFiles"));

	Command.Results.GenerateKeyArray( OutUnmodifiedFiles );

	return Command.bCommandSuccessful;
}

/**
 * Helper method to retrieve a particular history string from a results map
 *
 * @param	InHistoryResults	Map of history results
 * @param	InKey				Key to look for in the map
 *
 * @return	History string retrieved from the map
 */
static FString GetHistoryString( const TMap<FString, FString>& InHistoryResults, FSourceControl::FSourceControlFileHistoryInfo::EFileHistoryKeys InKey )
{
	const FString* StringToFind = InHistoryResults.Find( FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( InKey ) );
	check( StringToFind );
	return *StringToFind;
}

/**
 * Helper method to parse the results of an SCC history command
 *
 * @param	InHistoryResults	Map of results provided by the executed SCC history command
 * @param	OutFileHistory		Array of file history items as parsed from the provided map
 */
static void ParseHistoryCommandResults( const TMap<FString, TMap<FString, FString> >& InHistoryResults, TArray<FSourceControl::FSourceControlFileHistoryInfo>& OutFileHistory )
{
	// Each key in the map is a separate history file
	const INT NumHistoryFiles = InHistoryResults.Num();
	const TCHAR* HistoryKey = FSourceControl::FSourceControlFileHistoryInfo::GetFileHistoryKeyString( FSourceControl::FSourceControlFileHistoryInfo::EFH_HistoryKey );
	for ( INT HistoryIndex = 0; HistoryIndex < NumHistoryFiles; ++HistoryIndex )
	{
		// Retrieve the current history file from the map
		const TMap<FString, FString>* CurHistoryMap = InHistoryResults.Find( FString::Printf( TEXT("%s%d"), HistoryKey, HistoryIndex ) );
		check( CurHistoryMap );

		// Create a new file history info item for this history file
		FSourceControl::FSourceControlFileHistoryInfo* CurHistory = new (OutFileHistory) FSourceControl::FSourceControlFileHistoryInfo();
		check( CurHistory );

		// Retrieve the file name represented by this item
		FString FileNameString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_FileNameKey );
		CurHistory->FileName = FileNameString;

		// Retrieve the number of revisions this item has
		FString NumRevisionsString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_NumRevisionsKey );
		const INT NumRevisions = appAtoi( *NumRevisionsString );
		
		const TCHAR* ItemDelimiter = FSourceControl::FSourceControlFileHistoryInfo::FILE_HISTORY_ITEM_DELIMITER;
		
		// Retrieve the array of revision numbers for this item
		FString RevisionNumberString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_RevisionNumKey );
		TArray<FString> RevisionNumbers;
		RevisionNumberString.ParseIntoArray( &RevisionNumbers, ItemDelimiter, TRUE );

		// Retrieve the array of user names for this item
		FString UserNameString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_UserNameKey );
		TArray<FString> UserNames;
		UserNameString.ParseIntoArray( &UserNames, ItemDelimiter, TRUE );

		// Retrieve the array of dates for this item
		FString	DateString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_DateKey );
		TArray<FString> Dates;
		DateString.ParseIntoArray( &Dates, ItemDelimiter, TRUE );

		// Retrieve the array of changelist numbers for this item
		FString ChangelistNumberString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_ChangelistNumKey );
		TArray<FString> ChangelistNumbers;
		ChangelistNumberString.ParseIntoArray( &ChangelistNumbers, ItemDelimiter, TRUE );

		// Retrieve the array of descriptions for this item
		FString DescriptionString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_DescriptionKey );
		TArray<FString> Descriptions;
		DescriptionString.ParseIntoArray( &Descriptions, ItemDelimiter, TRUE );

		// Retrieve the array of file sizes for this item
		FString FileSizeString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_FileSizeKey );
		TArray<FString> FileSizes;
		FileSizeString.ParseIntoArray( &FileSizes, ItemDelimiter, TRUE );

		// Retrieve the array of clientspecs/workspaces for this item
		FString ClientSpecString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_ClientSpecKey );
		TArray<FString> ClientSpecs;
		ClientSpecString.ParseIntoArray( &ClientSpecs, ItemDelimiter, TRUE );

		// Retrieve the array of actions for this item
		FString ActionString = GetHistoryString( *CurHistoryMap, FSourceControl::FSourceControlFileHistoryInfo::EFH_ActionKey );
		TArray<FString> Actions;
		ActionString.ParseIntoArray( &Actions, ItemDelimiter, TRUE );

		// Ensure each retrieved array has the same number of entries or an error has occurred
		check(	RevisionNumbers.Num() == UserNames.Num() &&
				RevisionNumbers.Num() == Dates.Num() &&
				RevisionNumbers.Num() == ChangelistNumbers.Num() &&
				RevisionNumbers.Num() == Descriptions.Num() &&
				RevisionNumbers.Num() == FileSizes.Num() &&
				RevisionNumbers.Num() == ClientSpecs.Num() &&
				RevisionNumbers.Num() == Actions.Num() );

		// Parse the strings of each array item into their respective data formats and store them within a new revision info object
		for ( INT RevisionIndex = 0; RevisionIndex < NumRevisions; ++RevisionIndex )
		{
			FSourceControl::FSourceControlFileRevisionInfo* CurRevision = new (CurHistory->FileRevisions) FSourceControl::FSourceControlFileRevisionInfo();
			check( CurRevision );

			CurRevision->RevisionNumber = appAtoi( *RevisionNumbers(RevisionIndex) );
			CurRevision->UserName = UserNames(RevisionIndex);
			CurRevision->Date = appAtoi64( *Dates(RevisionIndex) );
			CurRevision->ChangelistNumber = appAtoi( *ChangelistNumbers(RevisionIndex) );
			CurRevision->Description = Descriptions(RevisionIndex);
			CurRevision->FileSize = appAtoi( *FileSizes(RevisionIndex) );
			CurRevision->ClientSpec = ClientSpecs(RevisionIndex);
			CurRevision->Action = Actions(RevisionIndex);
		}
	}
}

/** 
 * Retrieve file history for the provided files (Synchronous)
 *
 * @param	InLocalFileNames	Array of files to retrieve history for
 * @param	OutFileHistory		Array of file history info for the provided files
 */
void FSourceControl::GetFileHistory(const TArray<FString>& InLocalFileNames, TArray<FSourceControlFileHistoryInfo>& OutFileHistory)
{
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::HISTORY;
	Command.Params = InLocalFileNames;

	ExecuteSynchronousCommand(Command, NULL/**LocalizeUnrealEd("SourceControl_GetFileHistory")*/);

	ParseHistoryCommandResults( Command.Results, OutFileHistory );
}

/** 
 * Updates the status of files (Synchronous)
 * @param InPackages - Array of packages to get the status for
 */
void FSourceControl::UpdatePackageStatus(const TArray<UPackage*>& InPackages)
{
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::UPDATE_STATUS;
	for (INT i = 0; i < InPackages.Num(); ++i)
	{
		if (InPackages(i))
		{
			Command.Params.AddItem(InPackages(i)->GetName());
		}
	}
	ConvertPackageNamesToSourceControlPaths(Command.Params);

	//the convert package names to source control paths can remove results
	if (Command.Params.Num() > 0)
	{
		ExecuteSynchronousCommand(Command, NULL/**LocalizeUnrealEd("SourceControl_UpdateStatus")*/);

		// See if any of the passed in packages didn't end up in the results. If so, they're
		// likely not in the depot.
		for ( TArray<UPackage*>::TConstIterator PkgIter( InPackages ); PkgIter; ++PkgIter )
		{
			const UPackage* CurPkg = *PkgIter;
			const TMap<FString, FString>* ResultsMap = Command.Results.Find( CurPkg->GetName() );
			if ( !ResultsMap )
			{
				GPackageFileCache->SetSourceControlState( *CurPkg->GetName(), SCC_NotInDepot );
			}
		}
		for(TMap<FString,TMap<FString, FString> >::TConstIterator ResultIt(Command.Results);ResultIt;++ResultIt)
		{
			// Set the updated source control state for each package that was passed in
			const ESourceControlState NewState = FSourceControl::TranslateResultsToState( ResultIt.Value() );
			GPackageFileCache->SetSourceControlState( *ResultIt.Key(), NewState );
		}
	}
}

/** 
 * Updates the status of files (Synchronous)
 * @param InLocalFileName - File name to get the status for
 */
INT FSourceControl::ForceGetStatus(const FString& InLocalFileName)
{
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::UPDATE_STATUS;
	Command.Params.AddItem(InLocalFileName);

	ConvertPackageNamesToSourceControlPaths(Command.Params);

	ESourceControlState NewState = SCC_DontCare;
	if (Command.Params.Num() > 0)
	{
		ExecuteSynchronousCommand(Command, NULL/**LocalizeUnrealEd("SourceControl_UpdateStatus")*/);

		for(TMap<FString,TMap<FString, FString> >::TConstIterator ResultIt(Command.Results);ResultIt;++ResultIt)
		{
			NewState = FSourceControl::TranslateResultsToState(ResultIt.Value());
			//just get the only result since we only asked about one file
			break;
		}
	}
	return NewState;
}

/**
 * Updates the status of files (Synchronous)
 * @param InLocalFileName - Array of filenames to get the status for
 * @param OutFileNameToStatusMap - A map of filenames to their source control status.
 */
void FSourceControl::ForceGetStatus( const TArray<FString>& InLocalFileNames, TMap<FString,INT>& OutFileNameToStatusMap )
{
	//create command to send to the source control thread
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::UPDATE_STATUS;
	Command.Params = InLocalFileNames;

	if (Command.Params.Num() > 0)
	{
		ExecuteSynchronousCommand(Command, NULL/**LocalizeUnrealEd("SourceControl_UpdateStatus")*/);

		INT FileIndex = 0;
		// Populate the status map with the newly updated status for each filename
		for(TMap<FString,TMap<FString, FString> >::TConstIterator ResultIt(Command.Results); ResultIt; ++ResultIt, ++FileIndex)
		{
			ESourceControlState NewState = FSourceControl::TranslateResultsToState(ResultIt.Value());
			// The results only contain the package name not the entire path so we need to look that up in the original filename array.
			OutFileNameToStatusMap.Set( InLocalFileNames(FileIndex), NewState );
		}
	}

}
ESourceControlState FSourceControl::TranslateResultsToState(const TMap<FString, FString>& InResultsMap)
{
	const FString* HeadRev = InResultsMap.Find(TEXT("HeadRev"));
	const FString* HaveRev = InResultsMap.Find(TEXT("HaveRev"));
	const FString* OtherOpen = InResultsMap.Find(TEXT("OtherOpen"));
	const FString* OpenType = InResultsMap.Find(TEXT("OpenType"));
	const FString* HeadAction = InResultsMap.Find(TEXT("HeadAction"));

	ESourceControlState NewState = SCC_ReadOnly;
	if (OtherOpen)
	{
		NewState = SCC_CheckedOutOther;
	}
	else if (OpenType)
	{
		NewState = SCC_CheckedOut;
	}
	else if (HeadRev && HaveRev && (*HaveRev != *HeadRev))
	{
		NewState = SCC_NotCurrent;
	}
	//file has been previously deleted, ok to add again
	else if (HeadAction && ((*HeadAction)==TEXT("delete"))) 
	{
		NewState = SCC_NotInDepot;
	}
	//case SCC_DontCare:
	//case SCC_Ignore:

	return NewState;
}



//Constant useful for debugging which elements are having problems
#define NUM_FILES_PER_UPDATE		1

/** 
 * Updates the status of files (ASYNC)
 * @param InEventListener - Object to receive the SourceControlCallback command
 * @param InLocalFileNames - Array of files to check out
 */
void FSourceControl::IssueUpdateState(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames)
{
	//if there is no call back objects, don't bother updating status
	if (!InEventListener)
	{
		return;
	}

	//create command to send to the source control thread
	if (InLocalFileNames.Num())
	{
		//INT StartIndex = 0;

		//attempt to break the output into smaller more digestable chunks
		//while(StartIndex < InLocalFileNames.Num())
		{
			//make new command
			FSourceControlCommand* Command = new FSourceControlCommand(InEventListener, &StaticSourceControlProvider);
			Command->CommandType = SourceControl::UPDATE_STATUS;

			//loop over the files in chunks of NUM_FILES_PER_UPDATE
			/*INT EndIndex = InLocalFileNames.Num();//Min(StartIndex + NUM_FILES_PER_UPDATE, InLocalFileNames.Num());
			INT ParamIndex = StartIndex;
			for (INT ParamIndex = StartIndex; ParamIndex < EndIndex; ++ ParamIndex)
			{
				Command->Params.AddItem(InLocalFileNames(ParamIndex));
			}
			StartIndex = EndIndex;*/
			Command->Params = InLocalFileNames;

			//pass that command to the thread
			IssueCommand(Command);
			CommandQueue.AddItem(Command);
		}
	}
}

/** Quick check if source control is enabled */
UBOOL FSourceControl::IsEnabled(void)
{
	return !StaticSourceControlProvider.bDisabled;
}

/**
 * Quick check if the source control server is available 
 *
 * @return	TRUE if the server is available, FALSE if it is not
 */
UBOOL FSourceControl::IsServerAvailable(void)
{
	return StaticSourceControlProvider.bServerAvailable;
}

/**
 * Checks if a provided package is valid to be auto-added to a default changelist based on several
 * specifications (and if the user has elected to enable the feature). Only brand-new packages will be
 * allowed.
 *
 * @param	InPackage	Package to consider the validity of auto-adding to a default source control changelist
 * @param	InFilename	Filename of the package after it will be saved (not necessarily the same as the filename of the first param in the event of a rename)
 *
 * @return	TRUE if the provided package is valid to be auto-added to a default source control changelist
 */
UBOOL FSourceControl::IsPackageValidForAutoAdding(UPackage* InPackage, const FString& InFilename)
{
	UBOOL bPackageIsValid = FALSE;

	// Ensure the package exists, the user is running the editor (and not a commandlet or cooking), and that source control
	// is enabled and expecting new files to be auto-added before attempting to test the validity of the package
	if ( InPackage && GIsEditor && IsEnabled() && ShouldAutoAddNewFiles() && !GIsUCC && !GIsUCCMake && !GIsCooking )
	{
		// Assume package is valid to start
		bPackageIsValid = TRUE;

		// Determine if the package has been saved before or not; if it has, it's not valid for auto-adding
		FString ExistingFilename; 
		bPackageIsValid = !GPackageFileCache->FindPackageFile( *InFilename, NULL, ExistingFilename );

		// If the package is still considered valid up to this point, ensure that it is not a script or PIE package
		// and that the editor is not auto-saving.
		if ( bPackageIsValid )
		{
			const UBOOL bIsPlayOnConsolePackage = InFilename.StartsWith( PLAYWORLD_CONSOLE_BASE_PACKAGE_PREFIX );
			const UBOOL bIsPIEOrScriptPackage = InPackage->RootPackageHasAnyFlags( PKG_ContainsScript | PKG_PlayInEditor );
			const UBOOL bIsAutosave = GUnrealEd->bIsAutoSaving;

			if ( bIsPlayOnConsolePackage || bIsPIEOrScriptPackage || bIsAutosave )
			{
				bPackageIsValid = FALSE;
			}
		}

		// If the package is sill considered valid up to this point, ensure it is not one of the shader cache packages.
		if ( bPackageIsValid )
		{
			for( INT PlatformIndex = 0; PlatformIndex < SP_NumPlatforms && bPackageIsValid; ++PlatformIndex )
			{
				EShaderPlatform CurPlatform = static_cast<EShaderPlatform>( PlatformIndex );
				bPackageIsValid = 
					InFilename != FFilename( GetLocalShaderCacheFilename( CurPlatform ) ).GetCleanFilename() && 
					InFilename != FFilename( GetReferenceShaderCacheFilename( CurPlatform ) ).GetCleanFilename();
			}
		}

		// check the list of filenames to always ignore for source control
		if (bPackageIsValid)
		{
			FConfigSection* Section = GConfig->GetSectionPrivate(TEXT("SourceControl"), FALSE, TRUE, GEditorUserSettingsIni);
			if (Section != NULL)
			{
				TArray<const FString*> IgnoredFilenames;
				Section->MultiFindPointer(FName(TEXT("IgnoredFilenames")), IgnoredFilenames);
				FString BaseFilename = FFilename(InFilename).GetBaseFilename();
				for (INT i = 0; i < IgnoredFilenames.Num() && bPackageIsValid; i++)
				{
					bPackageIsValid = (BaseFilename != *IgnoredFilenames(i));
				}
			}
		}
	}
	return bPackageIsValid;
}

/**
 * Check if newly created files should be auto-added to source control or not
 *
 * @return TRUE if newly created files should be auto-added to source control
 */
UBOOL FSourceControl::ShouldAutoAddNewFiles()
{
	return StaticSourceControlProvider.bAutoAddNewFiles;
}


/**
 * Process command messages for completion and send results via source control event listener
 */
void FSourceControl::Tick(void)
{
	for (INT i = 0; i < CommandQueue.Num(); ++i)
	{
		if (CommandQueue(i)->bExecuteProcessed)
		{
			StaticSourceControlProvider.RespondToCommandErrorType(*CommandQueue(i));
			OutputCommandMessages(CommandQueue(i));

			//non synchronous events (the only event
			check(CommandQueue(i)->EventListener);
			CommandQueue(i)->EventListener->SourceControlCallback(CommandQueue(i));
			{
				//commands that are left in the array during a tick need to be deleted
				delete CommandQueue(i);
				CommandQueue.Remove(i);
				//don't skip over the next command in the queue
				--i;
			}
			//only do one command per tick loop
			break;
		}
	}
}

/**
 * Executes command but waits for it to be complete before returning
 * @param InCommand - Command to execute
 * @param Task - Task name
 * @return TRUE if the command wasn't aborted
 */
UBOOL FSourceControl::ExecuteSynchronousCommand(FSourceControlCommand& InCommand, const TCHAR* Task)
{
	UBOOL bRet = TRUE;

	// Display the progress dialog if a string was provided
	if ( Task )
	{
		GWarn->BeginSlowTask( Task, TRUE );
	}

	// First fire off a small test command to make sure the server is still responding
	FSourceControlCommand Command(NULL, &StaticSourceControlProvider);
	Command.CommandType = SourceControl::INFO;

	// Create a thread to run this command on, but wait for it to finish
	FSourceControlThreadRunnable* ThreadRunnable = new FSourceControlThreadRunnable( &Command );
	check( ThreadRunnable );		
	FRunnableThread* Thread = GThreadFactory->CreateThread( ThreadRunnable, TEXT("SourceControlThread"), 0, 0, 0, TPri_Normal );
	check( Thread );
	warnf( NAME_SourceControl, TEXT("Created thread (ID:%d)."), Thread->GetThreadID() );

	// Wait for the above command to complete (or timeout)
	static const DOUBLE LimitTime = 10.0f;
	const DOUBLE StartTime = appSeconds();
	while ( !Command.bExecuteProcessed )
	{			
		// Has the command taken too long? (or the user canceled?)		
		if ( ( ( appSeconds() - StartTime ) > LimitTime ) || GWarn->ReceivedUserCancel() )
		{
			bRet = FALSE;
			break;
		}
		Sleep( 100 );
	}

	// Destroy the thread
	GThreadFactory->Destroy( Thread );
	delete ThreadRunnable;
	ThreadRunnable = NULL;
	Thread = NULL;

	// If the above test failed the server is unresponsive and we should skip the command
	if ( !bRet )
	{
		InCommand.Abandon();		
	}
	else
	{
		// Otherwise, attempt to perform the command synchronously
		IssueCommand( &InCommand, TRUE );
	}

	// Hide the progress dialog if a string was provided
	if ( Task )
	{
		GWarn->EndSlowTask();
	}

	// If the command failed, inform the user that they need to try again
	if ( !bRet )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("SourceControl_ServerUnresponsive") );
	}

	return bRet;
}

/**
 * Issue command to thread system
 * @param InCommand - Command to execute
 * @param bSynchronous - Force the command to be issued on the same thread
 */
void FSourceControl::IssueCommand(FSourceControlCommand* InCommand, const UBOOL bSynchronous)
{
	InCommand->CommandID = NextCommandID++;	// Do we need to give it an ID if it's not threaded?

	if ( !bSynchronous && GThreadPool != NULL )
	{
		// Queue this to our worker thread(s) for resolving
		GThreadPool->AddQueuedWork(InCommand);
	}
	else
	{
		InCommand->DoWork();

		StaticSourceControlProvider.RespondToCommandErrorType(*InCommand);

		OutputCommandMessages(InCommand);
	}
}

/**
 * Dumps all thread output sent back to "debugf"
 */
void FSourceControl::OutputCommandMessages(FSourceControlCommand* InCommand)
{
	check(InCommand);
	for (INT i = 0; i < InCommand->ErrorMessages.Num(); ++i)
	{
		debugf(NAME_SourceControl, *InCommand->ErrorMessages(i));
	}
}

/** Constructor; Initializes FSourceControl */
FScopedSourceControl::FScopedSourceControl()
{
	FSourceControl::Init();
}

/** Destructor; Closes FSourceControl */
FScopedSourceControl::~FScopedSourceControl()
{
	FSourceControl::Close();
}

#endif // HAVE_SCC
