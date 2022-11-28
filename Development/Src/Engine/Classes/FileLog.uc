/**
 * Opens a file on disk for logging purposes.  Spawn one, then call
 * OpenLog() with the desired file name, output using Logf(), and then
 * finally call CloseLog(), or destroy the FileLog actor.
 */
class FileLog extends Info 
	native;

cpptext
{
	void OpenLog(FString &fileName,FString &extension);
	void Logf(FString &logString);
}

/** Internal FArchive pointer */
var const pointer LogAr;

/** Log file name, created via OpenLog() */
var const string LogFileName;

/**
 * Opens the actual file using the specified name.
 * 
 * @param	fileName - name of file to open
 * 
 * @param	extension - optional file extension to use, defaults to
 * 			.txt if none is specified
 */
native final function OpenLog(coerce string fileName, optional string extension);

/**
 * Closes the log file.
 */
native final function CloseLog();

/**
 * Logs the given string to the log file.
 * 
 * @param	logString - string to dump
 */
native final function Logf(string logString);

/**
 * Overridden to automatically close the logfile on destruction.
 */
event Destroyed()
{
	CloseLog();
}
