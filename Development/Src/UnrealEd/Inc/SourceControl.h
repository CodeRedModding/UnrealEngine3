/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __SOURCE_CONTROL_H__
#define __SOURCE_CONTROL_H__

#if _WINDOWS && ( !SHIPPING_PC_GAME || UDK ) && !UE3_LEAN_AND_MEAN
	#define HAVE_SCC 1
#else
	#define HAVE_SCC 0
#endif

#if HAVE_SCC

//include derived version
#include "SourceControlProvider.h"

//-----------------------------------------------
//Source Control Interface
//-----------------------------------------------
//Wrapper class for all source control Interfaces
class FSourceControl
{
public:

	/** Helper class to represent a revision of a file in source control */
	class FSourceControlFileRevisionInfo
	{
	public:
		/** Constructor */
		FSourceControlFileRevisionInfo();

		/** Changelist description */
		FString Description;

		/** User name of the submitter */
		FString UserName;

		/** Workspace/Clientspec of the submitter */
		FString ClientSpec;

		/** Action taken to the file this revision (branch/integrate/edit/etc.) */
		FString Action;

		/** Date of the revision, stored as number of seconds since the UNIX Epoch */
		LONG Date;

		/** Number of the revision */
		INT RevisionNumber;

		/** Changelist number of the revision */
		INT ChangelistNumber;

		/** File size of the revision (0 if the file was deleted) */
		INT FileSize;
	};

	/** Helper class to represent the history of a file in source control */
	class FSourceControlFileHistoryInfo
	{
	public:
		/** Enumeration for convenient querying of string keys for various data members (NOTE: THIS SHOULD BE KEPT UP TO DATE WITH FILE_HISTORY_KEYS) */
		enum EFileHistoryKeys
		{
			EFH_HistoryKey,
			EFH_NumRevisionsKey,
			EFH_FileNameKey,
			EFH_RevisionNumKey,
			EFH_UserNameKey,
			EFH_DateKey,
			EFH_ChangelistNumKey,
			EFH_DescriptionKey,
			EFH_FileSizeKey,
			EFH_ClientSpecKey,
			EFH_ActionKey,
			EFH_MAX
		};

		/** Constructor */
		FSourceControlFileHistoryInfo();

		/**
		 * Helper method which returns the key string associated with the provided enum value
		 *
		 * @param	KeyToGet	Enum value of the requested key string
		 *
		 * @return	Key string representing the provided enum value
		 */
		static const TCHAR* GetFileHistoryKeyString( EFileHistoryKeys KeyToGet );

		/** All revisions of this file */
		TArray<FSourceControlFileRevisionInfo> FileRevisions;

		/** Name of the file as it appears in the source control depot */
		FString FileName;

		/** String used to delimit history items compacted into a string */
		static const TCHAR* FILE_HISTORY_ITEM_DELIMITER;

	private:
		/** Keys used for lookup in a map returned by SCC commands (NOTE: THIS SHOULD BE KEPT UP TO DATE WITH EFILEHISTORYKEYS) */
		static const TCHAR* FILE_HISTORY_KEYS[EFH_MAX];
	};

	/** A thread which processes FSourceControlCommands */
	class FSourceControlThreadRunnable : public FRunnable
	{
	public:
		/** Initialization constructor. */
		FSourceControlThreadRunnable(FSourceControlCommand* InCommand):
		Command(InCommand)
		{}

		/** FRunnable interface. */
		virtual UBOOL	Init(void)	{ return TRUE; }
		virtual void	Exit(void)	{}
		virtual void	Stop(void)	{}
		virtual DWORD	Run(void)		{ IssueCommand(Command, TRUE); return 0; }

	private:
		/** Pointer to the command that needs processing. */
		FSourceControlCommand*	Command;
	};

	/** Inits the connection with the source control server */
	static void Init (void);
	/** Closes the connection with the source control server */
	static void Close (void);

	/** 
	 * Utility function to convert raw package names to absolute pathes
	 * @param InOutPackageNames - Array of package names to be converted to absolute path names 
	 */
	static void ConvertPackageNamesToSourceControlPaths(TArray<FString>& InOutPackageNames);

	/** 
	 * Check out files (Synchronous) and issues an ASYNC update state
	 * @param InEventListener - Object to receive the SourceControlCallback command
	 * @param InLocalFileNames - Array of files to check out
	 * @param bSilent - If true, will not warn about any errors 
	 */
	static UBOOL CheckOut(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames, UBOOL bSilent = GIsUCC);

	/** 
	 * Check out files (Synchronous) and issues an ASYNC update state
	 * @param InPackage - Package To CheckOut
	 * @param bSilent - If true, will not warn about any errors 
	 */
	static UBOOL CheckOut(UPackage* InPackage, UBOOL bSilent = GIsUCC);

	/** 
	 * Checks in files (Synchronous) and issues an ASYNC update state
	 * @param InEventListener - Object to receive the SourceControlCallback command
	 * @param InAddFileNames - Files that need to be added first
	 * @param InSubmitFileNames - Files that need only be submitted (not added)
	 * @param InDesc - Changelist Description
	 */
	static UBOOL CheckIn(FSourceControlEventListener* InEventListener, const TArray<FString>& InAddFileNames, const TArray<FString>& InSubmitFileNames, const FString& InDesc);

	/** 
	 * Add files (Synchronous) and issues an ASYNC update state
	 * @param InEventListener - Object to receive the SourceControlCallback command
	 * @param InLocalFileNames - Array of files to check out
	 */
	static UBOOL Add(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames);

	/** 
	 * Add the specified files to the default changelist (Synchronous) and issues an ASYNC update state
	 *
	 * @param InEventListener - Object to receive the SourceControlCallback command
	 * @param InLocalFileNames - Array of files to check out
	 */
	static UBOOL AddToDefaultChangelist(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames);

	/** 
	 * Delete files (Synchronous) and issues an ASYNC update state
	 * @param InEventListener - Object to receive the SourceControlCallback command
	 * @param InLocalFileNames - Array of files to check out
	 */
	static UBOOL Delete(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames);

	/** 
	 * Reverts files (Synchronous) and issues an ASYNC update state
	 * @param InEventListener - Object to receive the SourceControlCallback command
	 * @param InLocalFileNames - Array of files to check out
	 */
	static UBOOL Revert(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames);

	/** 
	 * Reverts files (Synchronous) files that have not changed and issues an SYNC update state
	 * @param InEventListener - Object to receive the SourceControlCallback command
	 * @param InLocalFileNames - Array of files to check out
	 */
	static UBOOL RevertUnchanged(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames);

	/**
	 * Queries for files (Synchronous) that are open by the user and have been modified from the version stored in source control.
	 * @param	InEventListener		Object to receive the SourceControlCallback command
	 * @param	InLocalFileNames	Array of files to query with, if any are specified, the query will be made against only the specified files;
	 *								If none are specified, all open files will be queried.
	 * @param	OutModifiedFiles	Array of files that are modified from the version stored in source control, if any.
	 *
	 * @return	TRUE if the command was successful, FALSE otherwise
	 */
	static UBOOL GetFilesModifiedFromServer(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames, TArray<FString>& OutModifiedFiles );
	
	/**
	 * Queries for files (Synchronous) that are open by the user and have *not* been modified from the version stored in source control.
	 * @param	InEventListener		Object to receive the SourceControlCallback command
	 * @param	InLocalFileNames	Array of files to query with, if any are specified, the query will be made against only the specified files;
	 *								If none are specified, all open files will be queried.
	 * @param	OutUnmodifiedFiles	Array of files that are unmodified from the version stored in source control, if any.
	 *
	 * @return	TRUE if the command was successful, FALSE otherwise
	 */
	static UBOOL GetFilesUnmodifiedFromServer(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames, TArray<FString>& OutUnmodifiedFiles );

	/** 
	 * Retrieve file history for the provided files (Synchronous)
	 *
	 * @param	InLocalFileNames	Array of files to retrieve history for
	 * @param	OutFileHistory		Array of file history info for the provided files
	 */
	static void GetFileHistory(const TArray<FString>& InLocalFileNames, TArray<FSourceControlFileHistoryInfo>& OutFileHistory);

	/** 
	 * Updates the status of files (Synchronous)
	 * @param InPackages - Array of packages to get the status for
	 */
	static void UpdatePackageStatus(const TArray<UPackage*>& InPackages);
	/** 
	 * Updates the status of files (Synchronous)
	 * @param InLocalFileName - File name to get the status for
	 */
	static INT ForceGetStatus(const FString& InLocalFileName);

	/** 
	 * Translates the results from the query
	 * @param InResultsMap - Results from the last perforce command
	 */
	static ESourceControlState TranslateResultsToState(const TMap<FString, FString>& InResultsMap);

	/** 
	 * Updates the status of files (ASYNC)
	 * @param InEventListener - Object to receive the SourceControlCallback command
	 * @param InLocalFileNames - Array of files to check out
	 */
	static void IssueUpdateState(FSourceControlEventListener* InEventListener, const TArray<FString>& InLocalFileNames);

	/** Quick check if source control is enabled */
	static UBOOL IsEnabled(void);

	/**
	 * Quick check if the source control server is available 
	 *
	 * @return	TRUE if the server is available, FALSE if it is not
	 */
	static UBOOL IsServerAvailable(void);

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
	static UBOOL IsPackageValidForAutoAdding(UPackage* InPackage, const FString& InFilename);

	/**
	 * Process results of commands on the main thread and send results via source control event listener
	 */
	static void Tick(void);

private:

	/** 
	 * Updates the status of files (Synchronous)
	 * @param InLocalFileName - Array of filenames to get the status for
     * @param OutFileNameToStatusMap - A map of filenames to their source control status.
	 */
	static void ForceGetStatus( const TArray<FString>& InLocalFileNames, TMap<FString,INT>& OutFileNameToStatusMap );

	/**
	 * Check if newly created files should be auto-added to source control or not
	 *
 	 * @return TRUE if newly created files should be auto-added to source control
	 */
	static UBOOL ShouldAutoAddNewFiles();

	/**
	 * Executes command but waits for it to be complete before returning
	 * @param InCommand - Command to execute
	 * @param Task - Task name
	 * @return TRUE if the command wasn't aborted
	 */
	static UBOOL ExecuteSynchronousCommand(FSourceControlCommand& InCommand, const TCHAR* Task);

	/**
	 * Issue command to thread system
	 * @param InCommand - Command to execute
	 * @param bSynchronous - Force the command to be issued on the same thread
	 */
	static void IssueCommand(FSourceControlCommand* InCommand, const UBOOL bSynchronous = FALSE);

	/**
	 * Dumps all thread output sent back to "debugf"
	 */
	static void OutputCommandMessages(FSourceControlCommand* InCommand);

	/** Derived implementation of a source control provider */
	static FSourceControlProvider StaticSourceControlProvider;

	/**Queue for commands given by the main thread*/
	static TArray <FSourceControlCommand*> CommandQueue;

	/**Unique index to use for next command*/
	static INT NextCommandID;
};

/** 
 * Helper class that ensures FSourceControl is properly initialized and shutdown by calling Init/Close in
 * its constructor/destructor respectively.
 */
class FScopedSourceControl
{
public:
	/** Constructor; Initializes FSourceControl */
	FScopedSourceControl();

	/** Destructor; Closes FSourceControl */
	~FScopedSourceControl();
};

#endif // HAVE_SCC
#endif // #define __SOURCE_CONTROL_H__

