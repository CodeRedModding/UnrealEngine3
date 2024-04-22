/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * User file downloading implementation via web service requests to MCP servers
 */
class McpUserCloudFileDownload extends McpServiceBase
	native
	config(Engine)
	implements(UserCloudFileInterface)
	dependson(OnlineSubsystem);

`include(Engine\Classes\HttpStatusCodes.uci);

/** The URL to use when requesting enumeration for list of a user's files */
var config String EnumerateCloudFilesUrl;

/** The URL to use when reading the contents of a file */
var config String ReadCloudFileUrl;

/** The URL to use when writing the contents of a file */
var config String WriteCloudFileUrl;

/** The URL to use when deleting a user cloud file */
var config String DeleteCloudFileUrl;

/** Info about a user file entry as enumerated from Mcp */
struct native McpUserCloudFileInfo extends OnlineSubsystem.EmsFile
{
	/** Date/time when file was created on server */
	var string CreationDate;
	/** Date/time when file was updated on server */
	var string LastUpdateDate;
	/** Compression type used to encode file. Ie. LZO,GZIP,etc */
	var string CompressionType;
};

/** Info for a single user's cloud files */
struct native McpUserCloudFilesEntry
{
	/** Id for user owning cloud files */
	var string UserId;
	/** list of files that have started downloads */
	var array<TitleFileWeb> DownloadedFiles;
	/** list of files available to download for this user */
	var array<McpUserCloudFileInfo> EnumeratedFiles;
	/** HTTP request that is in flight for enumerating files */
	var HttpRequestInterface HTTPRequestEnumerateFiles;
};
/** List of cloud file requests for all known users */
var	private array<McpUserCloudFilesEntry> UserCloudFileRequests;

/** The list of delegates to notify when the user file list enumeration is complete */
var private array<delegate<OnEnumerateUserFilesComplete> > EnumerateUserFilesCompleteDelegates;
/** The list of delegates to notify when a user file read is complete */
var private array<delegate<OnReadUserFileComplete> > ReadUserFileCompleteDelegates;
/** The list of delegates to notify when a user file write is complete */
var private array<delegate<OnWriteUserFileComplete> > WriteUserFileCompleteDelegates;
/** The list of delegates to notify when a user file delete is complete */
var private array<delegate<OnDeleteUserFileComplete> > DeleteUserFileCompleteDelegates;

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
function bool GetFileContents(string UserId,string FileName,out array<byte> FileContents)
{
	local bool bResult;
	local int EntryIdx,FileIdx;

	// Entry for the user
	EntryIdx = UserCloudFileRequests.Find('UserId',UserId);
	if (EntryIdx != INDEX_NONE)
	{
		// Check to see if file has been downloaded
		FileIdx = UserCloudFileRequests[EntryIdx].DownloadedFiles.Find('Filename',FileName);
		if (FileIdx != INDEX_NONE &&
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState == OERS_Done)
		{
			// Copy contents
			FileContents = UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Data;
			bResult = true;
		}
	}
	return bResult;
}

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @param UserId User owning the storage
 *
 * @return true if they could be deleted, false if they could not
 */
function bool ClearFiles(string UserId)
{
	local bool bResult;
	local int EntryIdx,FileIdx;

	// Entry for the user
	EntryIdx = UserCloudFileRequests.Find('UserId',UserId);
	if (EntryIdx != INDEX_NONE)
	{
		// Check to see if there files still pending download
		for (FileIdx=0; FileIdx < UserCloudFileRequests[EntryIdx].DownloadedFiles.Length; FileIdx++)
		{
			if (UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState == OERS_InProgress)
			{
				return false;
			}
		}
		UserCloudFileRequests[EntryIdx].DownloadedFiles.Length = 0;
		bResult = true;
	}
	return bResult;
}

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
function bool ClearFile(string UserId,string FileName)
{
	local bool bResult;
	local int EntryIdx,FileIdx;

	// Entry for the user
	EntryIdx = UserCloudFileRequests.Find('UserId',UserId);
	if (EntryIdx != INDEX_NONE)
	{
		// Check to see if file is still pending download
		FileIdx = UserCloudFileRequests[EntryIdx].DownloadedFiles.Find('Filename',FileName);
		if (FileIdx != INDEX_NONE &&
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState != OERS_InProgress)
		{
			UserCloudFileRequests[EntryIdx].DownloadedFiles.Remove(FileIdx,1);
			bResult = true;
		}
	}
	return bResult;
}

/**
 * Requests a list of available User files from the network store
 *
 * @param UserId User owning the storage
 *
 */
function EnumerateUserFiles(string UserId)
{
	local int EntryIdx;
	local string Url;
	local bool bPending;

	// Find entry for the user
	EntryIdx = UserCloudFileRequests.Find('UserId',UserId);
	// Add an entry for the user if one doesn't exist
	if (EntryIdx == INDEX_NONE)
	{
		EntryIdx = UserCloudFileRequests.Length;
		UserCloudFileRequests.Length = EntryIdx+1;
		UserCloudFileRequests[EntryIdx].UserId = UserId;
	}

	// If HTTPRequestEnumerateFiles already exists then don't need to do anything
	if (UserCloudFileRequests[EntryIdx].HTTPRequestEnumerateFiles == None)
	{
		// Create a new HTTP request
		UserCloudFileRequests[EntryIdx].HTTPRequestEnumerateFiles = class'HttpFactory'.static.CreateRequest();
		if (UserCloudFileRequests[EntryIdx].HTTPRequestEnumerateFiles != None)
		{
			// build URL for requesting list of files from MCP
			Url = GetBaseURL() $ EnumerateCloudFilesUrl $ GetAppAccessURL()
				$"&uniqueUserId=" $ UserId;
			// Configure the HTTP request and start it
			UserCloudFileRequests[EntryIdx].HTTPRequestEnumerateFiles.SetURL(Url);
			UserCloudFileRequests[EntryIdx].HTTPRequestEnumerateFiles.SetVerb("GET");
			UserCloudFileRequests[EntryIdx].HTTPRequestEnumerateFiles.OnProcessRequestComplete = OnHTTPRequestEnumerateUserFilesComplete;
			UserCloudFileRequests[EntryIdx].HTTPRequestEnumerateFiles.ProcessRequest();
			// kicked off successfully
			bPending = true;
		}
	}
	else
	{
		`log(`location@"User files already being enumerated for"
			$" UserId="$UserId);
	}
	// failed to kick off the request, always call the completion delegate
	if (!bPending)
	{
		CallEnumerateUserFileCompleteDelegates(false,UserId);
	}
}

/**
 * Called once the HTTP request/response has completed for retrieving a list of ems files for a user
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnHTTPRequestEnumerateUserFilesComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int EntryIdx,JsonIdx;
	local string JsonString,UserId;
	local JsonObject ParsedJson;
	local bool bResult;

	// Find entry for the user
	EntryIdx = UserCloudFileRequests.Find('HTTPRequestEnumerateFiles',Request);
	if (EntryIdx != INDEX_NONE)
	{
		UserId = UserCloudFileRequests[EntryIdx].UserId;
		if (bWasSuccessful &&
			Response.GetResponseCode() == `HTTP_STATUS_OK)
		{
			// Clear out any existing entries
			UserCloudFileRequests[EntryIdx].EnumeratedFiles.Length = 0;
			JsonString = Response.GetContentAsString();
			if (JsonString != "")
			{
				`log(`location@""
					$" JsonString="$JsonString);

				// Parse JSON response
				ParsedJson = class'JsonObject'.static.DecodeJson(JsonString);
				// Fill in the list of enumerated user files
				UserCloudFileRequests[EntryIdx].EnumeratedFiles.Length = ParsedJson.ObjectArray.Length;
				for (JsonIdx=0; JsonIdx < ParsedJson.ObjectArray.Length; JsonIdx++)
				{
					UserCloudFileRequests[EntryIdx].EnumeratedFiles[JsonIdx].Filename = ParsedJson.ObjectArray[JsonIdx].GetStringValue("file_name");
					UserCloudFileRequests[EntryIdx].EnumeratedFiles[JsonIdx].FileSize = int(ParsedJson.ObjectArray[JsonIdx].GetStringValue("file_size"));
					UserCloudFileRequests[EntryIdx].EnumeratedFiles[JsonIdx].DlName = ParsedJson.ObjectArray[JsonIdx].GetStringValue("file_name");
					UserCloudFileRequests[EntryIdx].EnumeratedFiles[JsonIdx].CreationDate = ParsedJson.ObjectArray[JsonIdx].GetStringValue("creation_date");
					UserCloudFileRequests[EntryIdx].EnumeratedFiles[JsonIdx].LastUpdateDate = ParsedJson.ObjectArray[JsonIdx].GetStringValue("last_update_time");
					UserCloudFileRequests[EntryIdx].EnumeratedFiles[JsonIdx].CompressionType = ParsedJson.ObjectArray[JsonIdx].GetStringValue("compression_type");
				}
			}
			bResult = true;
		}
		else
		{
			`log(`location@"Failed to enumerate files for"
				$" UserId="$UserCloudFileRequests[EntryIdx].UserId
				$" URL="$Request.GetURL());
		}
		// done with the HTTP request so clear it out
		UserCloudFileRequests[EntryIdx].HTTPRequestEnumerateFiles = None;
	}
	// call the completion delegate since the HTTP request completed
	CallEnumerateUserFileCompleteDelegates(bResult,UserId);
}

/**
 * Delegate fired when the list of files has been returned from the network store
 *
 * @param bWasSuccessful whether the file list was successful or not
 * @param UserId User owning the storage
 *
 */
delegate OnEnumerateUserFilesComplete(bool bWasSuccessful,string UserId);

/**
 * Calls delegates on file enumeration completion
 */
private function CallEnumerateUserFileCompleteDelegates(bool bWasSuccessful,string UserId)
{
	local int Index;
	local delegate<OnEnumerateUserFilesComplete> CallDelegate;

	// Call the completion delegate for receiving the file list
	for (Index=0; Index < EnumerateUserFilesCompleteDelegates.Length; Index++)
	{
		CallDelegate = EnumerateUserFilesCompleteDelegates[Index];
		if (CallDelegate != None)
		{
			CallDelegate(bWasSuccessful,UserId);
		}
	}
}

/**
 * Adds the delegate to the list to be notified when all files have been enumerated
 *
 * @param EnumerateUserFileCompleteDelegate the delegate to add
 *
 */
function AddEnumerateUserFileCompleteDelegate(delegate<OnEnumerateUserFilesComplete> EnumerateUserFileCompleteDelegate)
{
	if (EnumerateUserFilesCompleteDelegates.Find(EnumerateUserFileCompleteDelegate) == INDEX_NONE)
	{
		EnumerateUserFilesCompleteDelegates.AddItem(EnumerateUserFileCompleteDelegate);
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param EnumerateUserFileCompleteDelegate the delegate to remove
 *
 */
function ClearEnumerateUserFileCompleteDelegate(delegate<OnEnumerateUserFilesComplete> EnumerateUserFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = EnumerateUserFilesCompleteDelegates.Find(EnumerateUserFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		EnumerateUserFilesCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Returns the list of User files that was returned by the network store
 *
 * @param UserId User owning the storage
 * @param UserFiles out array of file metadata
 *
 */
function GetUserFileList(string UserId,out array<EmsFile> UserFiles)
{
	local int EntryIdx,FileIdx;

	EntryIdx = UserCloudFileRequests.Find('UserId',UserId);
	if (EntryIdx != INDEX_NONE)
	{
		UserFiles.Length = UserCloudFileRequests[EntryIdx].EnumeratedFiles.Length;
		for (FileIdx=0; FileIdx < UserCloudFileRequests[EntryIdx].EnumeratedFiles.Length; FileIdx++)
		{
			UserFiles[FileIdx].DLName = UserCloudFileRequests[EntryIdx].EnumeratedFiles[FileIdx].DLName;
			UserFiles[FileIdx].Filename = UserCloudFileRequests[EntryIdx].EnumeratedFiles[FileIdx].Filename;
			UserFiles[FileIdx].FileSize = UserCloudFileRequests[EntryIdx].EnumeratedFiles[FileIdx].FileSize;
		}
	}
	else
	{
		UserFiles.Length = 0;
	}
}

/**
 * Starts an asynchronous read of the specified user file from the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool ReadUserFile(string UserId,string FileName)
{
	local int EntryIdx,FileIdx;
	local string Url;
	local bool bPending;

	if (Len(UserId) > 0 &&
		Len(FileName) > 0)
	{
		// Find entry for the user
		EntryIdx = UserCloudFileRequests.Find('UserId',UserId);
		// Add an entry for the user if one doesn't exist
		if (EntryIdx == INDEX_NONE)
		{
			EntryIdx = UserCloudFileRequests.Length;
			UserCloudFileRequests.Length = EntryIdx+1;
			UserCloudFileRequests[EntryIdx].UserId = UserId;
		}
		// Find existing file entry
		FileIdx = UserCloudFileRequests[EntryIdx].DownloadedFiles.Find('Filename',FileName);
		// Add file entry if one doesn't exist
		if (FileIdx == INDEX_NONE)
		{
			FileIdx = UserCloudFileRequests[EntryIdx].DownloadedFiles.Length;
			UserCloudFileRequests[EntryIdx].DownloadedFiles.Length = FileIdx+1;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Filename = FileName;
		}
		// Make sure there is no operation already occurring for the file
		if (UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState != OERS_InProgress &&
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest == None)
		{
			// Update entry for the file being read
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState = OERS_InProgress;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Data.Length = 0;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest = class'HttpFactory'.static.CreateRequest();
			if (UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest != None)
			{
				// build URL for writing a single file to user's cloud storage
				Url = GetBaseURL() $ ReadCloudFileUrl $ GetAppAccessURL()
					$"&uniqueUserId=" $ UserId
					$"&fileName=" $ FileName;

				// Configure the HTTP request and start it
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.SetURL(Url);
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.SetVerb("GET");
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.OnProcessRequestComplete = OnHTTPRequestReadUserFileComplete;
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.ProcessRequest();

				// kicked off successfully
				bPending = true;
			}
		}
		else
		{
			`log(`location@"File operation already in progress"
				$" UserId="$UserId
				$" FileName="$FileName);
		}
	}
	else
	{
		`log(`location@"Invalid parameters"
			$" UserId="$UserId
			$" FileName="$FileName);
	}
	// failed to kick off the request, always call the completion delegate
	if (!bPending)
	{
		CallReadUserFileCompleteDelegates(false,UserId,FileName);
	}
	return bPending;
}

/**
 * Called once the HTTP request/response has completed for reading a user cloud file
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnHTTPRequestReadUserFileComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int EntryIdx,FileIdx;
	local string FileName,UserId;
	local bool bResult;
	local array<byte> FileContents;

	// Find entry for the user/file
	GetUserFileIndexForRequest(Request,EntryIdx,FileIdx);
	if (EntryIdx != INDEX_NONE &&
		FileIdx != INDEX_NONE)
	{
		UserId = UserCloudFileRequests[EntryIdx].UserId;
		FileName = UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Filename;
		// check for valid response for the request
		if (bWasSuccessful &&
			Response != None &&
			Response.GetResponseCode() == `HTTP_STATUS_OK)
		{
			// copy contents
			Response.GetContent(FileContents);
			// and, copy again
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Data = FileContents;
			// mark file entry as successfully completed
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState = OERS_Done;
			bResult = true;
		}
		else
		{
			// mark file entry as failed
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState = OERS_Failed;
		}
		// clear out the request as it's done
		UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest = None;
	}
	else
	{
		`log(`location@"Couldn't find entry index");
	}
	// call the completion delegate since the HTTP request completed
	CallReadUserFileCompleteDelegates(bResult,UserId,FileName);
}

/**
 * Delegate fired when a user file read from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 *
 */
delegate OnReadUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Calls delegates on file read completion
 */
private function CallReadUserFileCompleteDelegates(bool bWasSuccessful,string UserId,string FileName)
{
	local int Index;
	local delegate<OnReadUserFileComplete> CallDelegate;

	// Call the completion delegate for receiving the file list
	for (Index=0; Index < ReadUserFileCompleteDelegates.Length; Index++)
	{
		CallDelegate = ReadUserFileCompleteDelegates[Index];
		if (CallDelegate != None)
		{
			CallDelegate(bWasSuccessful,UserId,FileName);
		}
	}
}

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadUserFileCompleteDelegate the delegate to add
 */
function AddReadUserFileCompleteDelegate(delegate<OnReadUserFileComplete> ReadUserFileCompleteDelegate)
{
	if (ReadUserFileCompleteDelegates.Find(ReadUserFileCompleteDelegate) == INDEX_NONE)
	{
		ReadUserFileCompleteDelegates.AddItem(ReadUserFileCompleteDelegate);
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param ReadUserFileCompleteDelegate the delegate to remove
 */
function ClearReadUserFileCompleteDelegate(delegate<OnReadUserFileComplete> ReadUserFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = ReadUserFileCompleteDelegates.Find(ReadUserFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		ReadUserFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Starts an asynchronous write of the specified user file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToWrite the name of the file to write
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool WriteUserFile(string UserId,string FileName,const out array<byte> FileContents)
{
	local int EntryIdx,FileIdx;
	local string Url;
	local bool bPending;

	if (Len(UserId) > 0 &&
		Len(FileName) > 0 &&
		FileContents.Length > 0)
	{
		// Find entry for the user
		EntryIdx = UserCloudFileRequests.Find('UserId',UserId);
		// Add an entry for the user if one doesn't exist
		if (EntryIdx == INDEX_NONE)
		{
			EntryIdx = UserCloudFileRequests.Length;
			UserCloudFileRequests.Length = EntryIdx+1;
			UserCloudFileRequests[EntryIdx].UserId = UserId;
		}
		// Find existing file entry
		FileIdx = UserCloudFileRequests[EntryIdx].DownloadedFiles.Find('Filename',FileName);
		// Add file entry if one doesn't exist
		if (FileIdx == INDEX_NONE)
		{
			FileIdx = UserCloudFileRequests[EntryIdx].DownloadedFiles.Length;
			UserCloudFileRequests[EntryIdx].DownloadedFiles.Length = FileIdx+1;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Filename = FileName;
		}
		// Make sure there is no operation already occurring for the file
		if (UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState != OERS_InProgress &&
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest == None)
		{
			// Update entry for the file being written
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState = OERS_InProgress;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Data = FileContents;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest = class'HttpFactory'.static.CreateRequest();
			if (UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest != None)
			{
				// build URL for writing a single file to user's cloud storage
				Url = GetBaseURL() $ WriteCloudFileUrl $ GetAppAccessURL()
					$"&uniqueUserId=" $ UserId
					$"&fileName=" $ FileName;

				// Configure the HTTP request and start it
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.SetURL(Url);
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.SetVerb("POST");
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.SetHeader("Content-Type","multipart/form-data");
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.SetContent(FileContents);
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.OnProcessRequestComplete = OnHTTPRequestWriteUserFileComplete;
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.ProcessRequest();

				// kicked off successfully
				bPending = true;
			}
		}
		else
		{
			`log(`location@"File operation already in progress"
				$" UserId="$UserId
				$" FileName="$FileName);
		}
	}
	else
	{
		`log(`location@"Invalid parameters"
			$" UserId="$UserId
			$" FileName="$FileName
			$" FileContents="$FileContents.Length);
	}
	// failed to kick off the request, always call the completion delegate
	if (!bPending)
	{
		CallWriteUserFileCompleteDelegates(false,UserId,FileName);
	}
	return bPending;
}

/**
 * Helper to retrieve the user index and file index corresponding to an HTTP file download request
 *
 * @param UserIdx [out] entry for the user, -1 if not found
 * @param FileIdx [out] entry for the user file, -1 if not found
 */
private function GetUserFileIndexForRequest(HttpRequestInterface Request, out int UserIdx, out int FileIdx)
{
	for (UserIdx=0; UserIdx < UserCloudFileRequests.Length; UserIdx++)
	{
		FileIdx = UserCloudFileRequests[UserIdx].DownloadedFiles.Find('HTTPRequest',Request);
		if (FileIdx != INDEX_NONE)
		{
			break;
		}
	}
	if (FileIdx == INDEX_NONE)
	{
		UserIdx = INDEX_NONE;
	}
}

/**
 * Called once the HTTP request/response has completed for writing a user cloud file
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnHTTPRequestWriteUserFileComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int EntryIdx,FileIdx;
	local string FileName,UserId;
	local bool bResult;

	// Find entry for the user/file
	GetUserFileIndexForRequest(Request,EntryIdx,FileIdx);
	if (EntryIdx != INDEX_NONE &&
		FileIdx != INDEX_NONE)
	{
		UserId = UserCloudFileRequests[EntryIdx].UserId;
		FileName = UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Filename;
		// check for valid response for the request
		if (bWasSuccessful &&
			Response != None &&
			Response.GetResponseCode() == `HTTP_STATUS_OK)
		{
			// mark file entry as successfully completed
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState = OERS_Done;
			bResult = true;
		}
		else
		{
			// mark file entry as failed
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState = OERS_Failed;
		}
		// clear out the request as it's done
		UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest = None;
	}
	else
	{
		`log(`location@"Couldn't find entry index");
	}
	// call the completion delegate since the HTTP request completed
	CallWriteUserFileCompleteDelegates(bResult,UserId,FileName);
}

/**
 * Delegate fired when a user file write to the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file Write was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 *
 */
delegate OnWriteUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Calls delegates on file write completion
 */
private function CallWriteUserFileCompleteDelegates(bool bWasSuccessful,string UserId,string FileName)
{
	local int Index;
	local delegate<OnWriteUserFileComplete> CallDelegate;

	// Call the completion delegate for receiving the file list
	for (Index=0; Index < WriteUserFileCompleteDelegates.Length; Index++)
	{
		CallDelegate = WriteUserFileCompleteDelegates[Index];
		if (CallDelegate != None)
		{
			CallDelegate(bWasSuccessful,UserId,FileName);
		}
	}
}

/**
 * Adds the delegate to the list to be notified when a requested file has been written
 *
 * @param WriteUserFileCompleteDelegate the delegate to add
 */
function AddWriteUserFileCompleteDelegate(delegate<OnWriteUserFileComplete> WriteUserFileCompleteDelegate)
{
	if (WriteUserFileCompleteDelegates.Find(WriteUserFileCompleteDelegate) == INDEX_NONE)
	{
		WriteUserFileCompleteDelegates.AddItem(WriteUserFileCompleteDelegate);
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param WriteUserFileCompleteDelegate the delegate to remove
 */
function ClearWriteUserFileCompleteDelegate(delegate<OnWriteUserFileComplete> WriteUserFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = WriteUserFileCompleteDelegates.Find(WriteUserFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		WriteUserFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/**
 * Starts an asynchronous delete of the specified user file from the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToRead the name of the file to read
 * @param bShouldCloudDelete whether to delete the file from the cloud
 * @param bShouldLocallyDelete whether to delete the file locally
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool DeleteUserFile(string UserId,string FileName,bool bShouldCloudDelete,bool bShouldLocallyDelete)
{
	local int EntryIdx,FileIdx;
	local string Url;
	local bool bPending;

	if (Len(UserId) > 0 &&
		Len(FileName) > 0 &&
		bShouldCloudDelete)
	{
		// Find entry for the user
		EntryIdx = UserCloudFileRequests.Find('UserId',UserId);
		// Add an entry for the user if one doesn't exist
		if (EntryIdx == INDEX_NONE)
		{
			EntryIdx = UserCloudFileRequests.Length;
			UserCloudFileRequests.Length = EntryIdx+1;
			UserCloudFileRequests[EntryIdx].UserId = UserId;
		}
		// Find existing file entry
		FileIdx = UserCloudFileRequests[EntryIdx].DownloadedFiles.Find('Filename',FileName);
		// Add file entry if one doesn't exist
		if (FileIdx == INDEX_NONE)
		{
			FileIdx = UserCloudFileRequests[EntryIdx].DownloadedFiles.Length;
			UserCloudFileRequests[EntryIdx].DownloadedFiles.Length = FileIdx+1;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Filename = FileName;
		}
		// Make sure there is no operation already occurring for the file
		if (UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState != OERS_InProgress &&
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest == None)
		{
			// Update entry for the file being written
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].AsyncState = OERS_InProgress;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Data.Length = 0;
			UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest = class'HttpFactory'.static.CreateRequest();
			if (UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest != None)
			{
				// build URL for writing a single file to user's cloud storage
				Url = GetBaseURL() $ DeleteCloudFileUrl $ GetAppAccessURL()
					$"&uniqueUserId=" $ UserId
					$"&fileName=" $ FileName;

				// Configure the HTTP request and start it
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.SetURL(Url);
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.SetVerb("DELETE");
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.OnProcessRequestComplete = OnHTTPRequestDeleteUserFileComplete;
				UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].HTTPRequest.ProcessRequest();

				// kicked off successfully
				bPending = true;
			}
		}
		else
		{
			`log(`location@"File operation already in progress"
				$" UserId="$UserId
				$" FileName="$FileName);
		}
	}
	else
	{
		`log(`location@"Invalid parameters"
			$" UserId="$UserId
			$" FileName="$FileName);
	}
	// failed to kick off the request, always call the completion delegate
	if (!bPending)
	{
		if (bShouldCloudDelete)
		{
			CallDeleteUserFileCompleteDelegates(false,UserId,FileName);
		}
		else if (bShouldLocallyDelete)
		{
			//@todo - handle local cache deletion
			CallDeleteUserFileCompleteDelegates(true,UserId,FileName);
		}
	}
	return bPending;
}

/**
 * Called once the HTTP request/response has completed for deleting a user cloud file
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnHTTPRequestDeleteUserFileComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int EntryIdx,FileIdx;
	local string FileName,UserId;
	local bool bResult;

	// Find entry for the user/file
	GetUserFileIndexForRequest(Request,EntryIdx,FileIdx);
	if (EntryIdx != INDEX_NONE &&
		FileIdx != INDEX_NONE)
	{
		UserId = UserCloudFileRequests[EntryIdx].UserId;
		FileName = UserCloudFileRequests[EntryIdx].DownloadedFiles[FileIdx].Filename;
		// check for valid response for the request
		if (bWasSuccessful &&
			Response != None &&
			Response.GetResponseCode() == `HTTP_STATUS_OK)
		{
			bResult = true;
		}
		// clear out the file entry
		UserCloudFileRequests[EntryIdx].DownloadedFiles.Remove(FileIdx,1);
	}
	else
	{
		`log(`location@"Couldn't find entry index");
	}
	// call the completion delegate since the HTTP request completed
	CallDeleteUserFileCompleteDelegates(bResult,UserId,FileName);
}

/**
 * Delegate fired when a user file delete from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 */
delegate OnDeleteUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Calls delegates on file delete completion
 */
private function CallDeleteUserFileCompleteDelegates(bool bWasSuccessful,string UserId,string FileName)
{
	local int Index;
	local delegate<OnDeleteUserFileComplete> CallDelegate;

	// Call the completion delegate for receiving the file list
	for (Index=0; Index < DeleteUserFileCompleteDelegates.Length; Index++)
	{
		CallDelegate = DeleteUserFileCompleteDelegates[Index];
		if (CallDelegate != None)
		{
			CallDelegate(bWasSuccessful,UserId,FileName);
		}
	}
}

/**
 * Adds the delegate to the list to be notified when a requested file has been deleted
 *
 * @param DeleteUserFileCompleteDelegate the delegate to add
 */
function AddDeleteUserFileCompleteDelegate(delegate<OnDeleteUserFileComplete> DeleteUserFileCompleteDelegate)
{
	if (DeleteUserFileCompleteDelegates.Find(DeleteUserFileCompleteDelegate) == INDEX_NONE)
	{
		DeleteUserFileCompleteDelegates.AddItem(DeleteUserFileCompleteDelegate);
	}
}

/**
 * Removes the delegate from the notify list
 *
 * @param DeleteUserFileCompleteDelegate the delegate to remove
 */
function ClearDeleteUserFileCompleteDelegate(delegate<OnDeleteUserFileComplete> DeleteUserFileCompleteDelegate)
{
	local int RemoveIndex;

	RemoveIndex = DeleteUserFileCompleteDelegates.Find(DeleteUserFileCompleteDelegate);
	if (RemoveIndex != INDEX_NONE)
	{
		DeleteUserFileCompleteDelegates.Remove(RemoveIndex,1);
	}
}

/** clears all delegates for e.g. end of level cleanup */
function ClearAllDelegates()
{
	EnumerateUserFilesCompleteDelegates.length = 0;
	ReadUserFileCompleteDelegates.length = 0;
	WriteUserFileCompleteDelegates.length = 0;
	DeleteUserFileCompleteDelegates.length = 0;
}
