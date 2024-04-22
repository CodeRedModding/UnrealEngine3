/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Provides a mechanism for downloading arbitrary files from the MCP server
 */
class OnlineTitleFileDownloadBase extends MCPBase
	native
	implements(OnlineTitleFileInterface)
	dependson(OnlineSubsystem);

/** Compression types supported */
enum EMcpFileCompressionType
{
	MFCT_NONE,
	MFCT_ZLIB
};
/** Holds the data used in downloading a file along with its web request */
struct native TitleFileWeb extends OnlineSubsystem.TitleFile
{
	/** web response or string data if download succeeded */
	var string StringData;
	/** HTTP request that is in flight for the file request */
	var HttpRequestInterface HTTPRequest;
	/** The compression type of this File so the client knows how to de-compress the payload	 */
	var EMcpFileCompressionType FileCompressionType;
};

/** The list of delegates to notify when a file is read */
var private array<delegate<OnReadTitleFileComplete> > ReadTitleFileCompleteDelegates;

/** The list of delegates to notify when a file list read is done */
var array<delegate<OnRequestTitleFileListComplete> > RequestTitleFileListCompleteDelegates;

/** The base URL to used for contacting for files, such that BaseUrl?TitleID=1234&FileName=MyFile.ini is the complete URL */
var config string BaseUrl;

/** Base URL for getting list of EMS files */
var config string RequestFileListURL;
/** Base URL for downloading a single EMS file */
var config string RequestFileURL;

/** The amount of time to allow for downloading of the file */
var config float TimeOut;

/** Allows the game to route a specific file or sets of files to a specific URL. If there is no special mapping for a file, then the base URL is used */
struct native FileNameToURLMapping
{
	/** The name of the file to route to a specific URL */
	var name FileName;
	/** The URL to route the request to */
	var name UrlMapping;
};

/** The routing table to look in when trying to find special URL handlers */
var config array<FileNameToURLMapping> FilesToUrls;

/**
 * Delegate fired when a file read from the network platform's title specific storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnReadTitleFileComplete(bool bWasSuccessful,string FileName);

/**
 * Starts an asynchronous read of the specified file from the network platform's
 * title specific file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool ReadTitleFile(string FileToRead);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadTitleFileCompleteDelegate the delegate to add
 */
function AddReadTitleFileCompleteDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate)
{
	if (ReadTitleFileCompleteDelegates.Find(ReadTitleFileCompleteDelegate) == INDEX_NONE)
	{
		ReadTitleFileCompleteDelegates[ReadTitleFileCompleteDelegates.Length] = ReadTitleFileCompleteDelegate;
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param ReadTitleFileCompleteDelegate the delegate to remove
 */
function ClearReadTitleFileCompleteDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = ReadTitleFileCompleteDelegates.Find(ReadTitleFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ReadTitleFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
function bool GetTitleFileContents(string FileName,out array<byte> FileContents);

/**
 * Determines the async state of the tile file read operation
 *
 * @param FileName the name of the file to check on
 *
 * @return the async state of the file read
 */
function EOnlineEnumerationReadState GetTitleFileState(string FileName);

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
function bool ClearDownloadedFiles();

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
function bool ClearDownloadedFile(string FileName);

/**
 * Async call to request a list of files (returned as string) from EMS
 */
function RequestTitleFileList();

/**
 * Delegate fired when the request for a list of files completes
 *
 * @param bWasSuccessful whether the request completed successfully
 * @param ResultStr contains the list of files and associated meta data
 */
delegate OnRequestTitleFileListComplete(bool bWasSuccessful, string ResultStr);

/**
 * Adds the delegate to the list to be notified when the list of requested files has been received
 *
 * @param RequestTitleFileListDelegate the delegate to add
 */
function AddRequestTitleFileListCompleteDelegate(delegate<OnRequestTitleFileListComplete> RequestTitleFileListDelegate)
{
	if (RequestTitleFileListCompleteDelegates.Find(RequestTitleFileListDelegate) == INDEX_NONE)
	{
		RequestTitleFileListCompleteDelegates.AddItem(RequestTitleFileListDelegate);
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param RequestTitleFileListDelegate the delegate to remove
 */
function ClearRequestTitleFileListCompleteDelegate(delegate<OnRequestTitleFileListComplete> RequestTitleFileListDelegate)
{
	local int RemoveIndex;

	RemoveIndex = RequestTitleFileListCompleteDelegates.Find(RequestTitleFileListDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		RequestTitleFileListCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Searches the filename to URL mapping table for the specified filename
 *
 * @param FileName the file to search the table for
 *
 * @param the URL to use to request the file or BaseURL if no special mapping is present
 */
native function string GetUrlForFile(string FileName);