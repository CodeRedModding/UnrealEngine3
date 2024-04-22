/**
 * Opens a file on disk for logging purposes.  Spawn one, then call
 * OpenLog() with the desired file name, output using Logf(), and then
 * finally call CloseLog(), or destroy the FileLog actor.
 *
 * This functionality has been moved to the new FileWriter class.  Stubs
 * have been left here for compatibility
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FileLog extends FileWriter
	native;

/**
 * Opens the actual file using the specified name.
 *
 * @param	LogFilename - name of file to open
 *
 * @param	extension - optional file extension to use, defaults to
 * 			.txt if none is specified
 *
 * @param	bUnique - Makes sure the file is unique

 */

function OpenLog(coerce string LogFilename, optional string extension, optional bool bUnique)
{
	OpenFile(LogFilename, FWFT_Log, extension, bUnique);
}

/**
 * Closes the log file.
 */

function CloseLog()
{
	CloseFile();
}


