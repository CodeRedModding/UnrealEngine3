/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is a simple class that allows for secure writing of output files from within Script.  The directory to which it writes
 * files is determined by the file type member variable.
*/

class FileWriter extends Info
	native;

/** Internal FArchive pointer */
var const native pointer ArchivePtr{FArchive};

/** File name, created via OpenFile() */
var const string Filename;

/** Type of file */
enum FWFileType
{
	FWFT_Log,				// Created in %GameDir%/Logs
	FWFT_Stats,				// Created in %GameDir%/Stats
	FWFT_HTML,				// Created in %GameDir%/Web/DynamicHTML
	FWFT_User,				// Created in %GameDir%/User
	FWFT_Debug,				// Created in %GameDir%/Debug
};

/** Holds the file type for this file. */
var const FWFileType FileType;

/**
 * Whether we should flush to disk every time something is written.
 * if false, only flush when the memory buffer is full or when the file is closed
 */
var bool bFlushEachWrite;

/** Whether to use async writes (if available) or not. Overrides bFlushEachWrite */
var bool bWantsAsyncWrites;

cpptext
{
	virtual void BeginDestroy();
}

/**
 * Opens the actual file using the specified name.
 *
 * @param InFilename name of file to open
 * @param InFileType the type of file being written
 * @param InExtension optional file extension to use, defaults to .txt if none is specified
 * @param bUnique whether to make unique or not
 * @param bIncludeTimeStamp whether to include timestamps or not
 */
native final function bool OpenFile(coerce string InFilename, optional FWFileType InFileType,
									optional string InExtension, optional bool bUnique, optional bool bIncludeTimeStamp);

/**
 * Closes the log file.
 */
native final function CloseFile();

/**
 * Logs the given string to the log file.
 *
 * @param	logString - string to dump
 */
native final function Logf(coerce string logString);

/**
 * Overridden to automatically close the logfile on destruction.
 */
event Destroyed()
{
	CloseFile();
}

defaultproperties
{
	bFlushEachWrite=true
	bTickIsDisabled=true
}
