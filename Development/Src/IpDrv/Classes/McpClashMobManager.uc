/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Implementation of ClashMob Mcp services
 */
class McpClashMobManager extends McpClashMobBase;

`include(Engine\Classes\HttpStatusCodes.uci)

/** Url path for enumerating list of available challenges */
var config String ChallengeListUrl;
/** Url path for querying status of a single user wrt a challenge */
var config String ChallengeStatusUrl;
/** Url path for querying status for multiple users wrt a challenge */
var config String ChallengeMultiStatusUrl;
/** Url for having a user accept a challenge */
var config String AcceptChallengeUrl;
/** Url for updating user progress for a challenge */
var config String UpdateChallengeProgressUrl;
/** Url for updating user progress for a reward */
var config String UpdateRewardProgressUrl;

/** HTTP request object that is used to request the list of challenges. None when no request is in flight */
var HttpRequestInterface HTTPRequestChallengeList;

/** Holds HTTP request for a pending challenge query */
struct McpChallengeRequest
{
	/** Challenge that the request was initiated for */
	var string UniqueChallengeId;
	/** HTTP request object that holds the web request while it is in flight */
	var HttpRequestInterface HTTPRequest;
};
/** Holds the HTTP requests for pending user tasks */
struct McpChallengeUserRequest
{
	/** User that the request was initiated for */
	var string UniqueUserId;
	/** List of web requests that are in flight for getting challenge status for users */
	var array<McpChallengeRequest> ChallengeStatusRequests;
	/** List of web requests that are in flight for accepting challenges for users */
	var array<McpChallengeRequest> ChallengeAcceptRequests;
	/** List of web requests that are in flight for updating progress of challenges for users */
	var array<McpChallengeRequest> ChallengeUpdateProgressRequests;
	/** List of web requests that are in flight for updating rewards given to users for challenges */
	var array<McpChallengeRequest> ChallengeUpdateRewardRequests;
};
/** List of pending requests */
var array<McpChallengeUserRequest> ChallengeUserRequests;

/** List of challenges that were enumerated from the server. Filled in with QueryChallengeList */
var array<McpClashMobChallengeEvent> ChallengeEvents;
/** List of user status for challenge. Filled in with QueryChallengeStatus */
var array<McpClashMobChallengeUserStatus> ChallengeUserStatus;
/** only used for JSON import */
var McpClashMobChallengeUserStatus TempChallengeUserStatus;
/** only used for JSON import */
var array<McpClashMobChallengeUserStatus> TempChallengeUserStatusArray;

/** Caches downloaded files locally to disk and in memory */
var OnlineTitleFileCacheInterface FileCache;
/** Used to download challenge files from server */
var McpClashMobFileDownload FileDownloader;

/* Initialize always called after constructing a new MCP service subclass instance via its factory method */
function Init()
{
	Super.Init();

	// Used to cache of files to/from disk
	if (FileCache == None)
	{
		FileCache = new class'TitleFileDownloadCache';
		if (FileCache != None)
		{
			FileCache.AddLoadTitleFileCompleteDelegate(OnLoadCachedFileComplete);
		}
	}
	// Downloads the challenge files from MCP
	if (FileDownloader == None)
	{
		FileDownloader = new class'McpClashMobFileDownload';
		FileDownloader.Init();
		if (FileDownloader != None)
		{
			FileDownloader.AddReadTitleFileCompleteDelegate(OnDownloadMcpFileComplete);
		}
	}
}

/**
 * Initiates a web request to retrieve the list of available challenge events from the server.
 */
function QueryChallengeList()
{
	local string Url,ErrorStr;
	local bool bPending;

	if (HTTPRequestChallengeList == None)
	{
		HTTPRequestChallengeList = class'HttpFactory'.static.CreateRequest();
		if (HTTPRequestChallengeList != None)
		{
			Url = GetBaseURL() $ ChallengeListUrl $ GetAppAccessURL();
			HTTPRequestChallengeList.SetURL(Url);
			HTTPRequestChallengeList.SetVerb("GET");
			HTTPRequestChallengeList.OnProcessRequestComplete = OnQueryChallengeListHTTPRequestComplete;
			if (HTTPRequestChallengeList.ProcessRequest())
			{
				bPending = true;
			}
			else
			{
				ErrorStr = "failed to start request, Url="$Url;
			}
		}
	}
	else
	{
		ErrorStr = "last request is still being processed";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	if (!bPending)
	{
		OnQueryChallengeListComplete(false,ErrorStr);
	}
}

/**
 * Called once the request/response has completed for downloading the list of challenges from Mcp
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnQueryChallengeListHTTPRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local string JSONStr,ErrorStr;
	local bool bResult;

	HTTPRequestChallengeList = None;

	if (bWasSuccessful &&
		Response != None)
	{
		if (Response.GetResponseCode() == `HTTP_STATUS_OK)
		{
			JSONStr = Response.GetContentAsString();
			ImportJSON("ChallengeEvents",JSONStr);
			bResult = true;
		}
		else
		{
			ErrorStr = "invalid server response code, status="$Response.GetResponseCode();
		}
	}
	else
	{
		ErrorStr = "no response";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	OnQueryChallengeListComplete(bResult,ErrorStr);
}

/**
 * Access the currently cached challenge list that was downloaded. Use QueryChallengeList first
 *
 * @param OutChallengeEvents the list of events that should be filled in
 */
function GetChallengeList(out array<McpClashMobChallengeEvent> OutChallengeEvents)
{
	OutChallengeEvents = ChallengeEvents;
}

/**
 * Get the list of files for a given challenge
 *
 * @param UniqueChallengeId id of challenge that may have files
 * @param OutChallengeFiles list of files that should be filled in
 */
function GetChallengeFileList(string UniqueChallengeId, out array<McpClashMobChallengeFile> OutChallengeFiles)
{
	local int ChallengeEventIdx;

	OutChallengeFiles.Length = 0;

	ChallengeEventIdx = ChallengeEvents.Find('unique_challenge_id',UniqueChallengeId);
	if (ChallengeEventIdx != INDEX_NONE)
	{
		OutChallengeFiles = ChallengeEvents[ChallengeEventIdx].file_list;
	}
	else
	{
		`log(`StaticLocation@"Couldn't find event entry for"
			$" UniqueChallengeId="$UniqueChallengeId);
	}
}

/**
 * Starts the load/download of a challenge file
 *
 * @param UniqueChallengeId id of challenge that owns the file
 * @param DlName download name of the file
 */
function DownloadChallengeFile(string UniqueChallengeId, string DlName)
{
	local string ErrorStr;
	local int ChallengeIdx,FileIdx;
	local bool bPending;

	ChallengeIdx = ChallengeEvents.Find('unique_challenge_id',UniqueChallengeId);
	if (ChallengeIdx != INDEX_NONE)
	{
		FileIdx = ChallengeEvents[ChallengeIdx].file_list.Find('dl_name',DlName);
		if (FileIdx != INDEX_NONE)
		{
			// Mark as pending until load completes
			ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status = MCFS_Pending;
			// Clear out old memory copy first
			FileCache.ClearCachedFile(DlName);
			// Will always trigger the OnLoadCachedFileComplete delegate
			FileCache.LoadTitleFile(DlName);
			// pending load
			bPending = true;
		}
		else
		{
			ErrorStr = "Couldn't find file entry for"
				$" UniqueChallengeId="$UniqueChallengeId
				$" DlName="$UniqueChallengeId;
		}
	}
	else
	{
		ErrorStr = "Couldn't find event entry for"
			$" UniqueChallengeId="$UniqueChallengeId;
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	if (!bPending)
	{
		OnDownloadChallengeFileComplete(false,UniqueChallengeId,DlName,"",ErrorStr);
	}
}

/**
 * Call back when a requested file has been loaded
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
private function OnLoadCachedFileComplete(bool bWasSuccessful,string DlName)
{
	local bool bRequiresDownload;
	local string FileHashCache,FileHashChallenge;
	local string FileName;
	local int ChallengeIdx, FileIdx;
	local array<byte> FileContents;

	// Find the ChallengeIdx,FileIdx for the FileName
	for (ChallengeIdx=0; ChallengeIdx < ChallengeEvents.Length; ChallengeIdx++)
	{
		FileIdx = ChallengeEvents[ChallengeIdx].file_list.Find('dl_name',DlName);
		if (FileIdx != INDEX_NONE)
		{
			break;
		}
	}
	if (FileIdx != INDEX_NONE)
	{
		ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status = MCFS_Failed;
		FileName = ChallengeEvents[ChallengeIdx].file_list[FileIdx].file_name;
		bRequiresDownload = true;

		// Load completed successfully, but still need to verify the file hash
		if (bWasSuccessful)
		{
			// Valid hash for the file comes from the challenge request
			FileHashChallenge = ChallengeEvents[ChallengeIdx].file_list[FileIdx].hash_code;
			if	(Len(FileHashChallenge) > 0)
			{
				// Hash for the file on disk
				FileHashCache = FileCache.GetTitleFileHash(DlName);
				// Determine if hashes match
				if (FileHashCache == FileHashChallenge &&
					FileCache.GetTitleFileContents(DlName,FileContents))
				{
					`log(`StaticLocation @ "Found challenge file in cache with valid hash. DLName="$DlName @"in file cache. Not downloading.");
					// Mark as complete since file was loaded and hash matches
					ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status = MCFS_Success;
					// Call delegate to allow processing of loaded file
					OnDownloadChallengeFileComplete(true,ChallengeEvents[ChallengeIdx].unique_challenge_id,DlName,FileName,"");
					// No need to request download
					bRequiresDownload = false;
				}
			}
		}
		// Loading from cache failed, request download
		if (bRequiresDownload)
		{
			`log(`StaticLocation @ "Did not find challenge file DLName="$DlName @"in file cache with valid hash. Starting download.");

			// Start the download for the challenge file
			ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status = MCFS_Pending;
			// Local file was invalid, so delete
			FileCache.DeleteTitleFile(DlName);
			// Clear out old memory copy first
			FileDownloader.ClearDownloadedFile(DlName);
			// Start the download which will call OnDownloadMcpFileComplete
			FileDownloader.ReadTitleFile(DlName);
		}
	}
	else
	{
		`log(`StaticLocation @ "Could not find DLName="$DlName @"for challenge");
	}
}

/**
 * Call back when a requested file has been downloaded from MCP
 *
 * @param bWasSuccessful whether the file download was successful or not
 * @param FileName the name of the file this was for
 */
function OnDownloadMcpFileComplete(bool bWasSuccessful,string DlName)
{
	local array<byte> FileContents;
	local int ChallengeIdx, FileIdx;
	local string FileName;

	// Find the ChallengeIdx,FileIdx for the FileName
	for (ChallengeIdx=0; ChallengeIdx < ChallengeEvents.Length; ChallengeIdx++)
	{
		FileIdx = ChallengeEvents[ChallengeIdx].file_list.Find('dl_name',DlName);
		if (FileIdx != INDEX_NONE)
		{
			break;
		}
	}
	if	(FileIdx != INDEX_NONE)
	{
		if (bWasSuccessful &&
			FileDownloader.GetTitleFileContents(DlName,FileContents))
		{
			`log(`StaticLocation @ "Downloaded challenge file. DLName="$DlName @". Copying to file cache.");

			// Download was successful
			ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status = MCFS_Success;
			FileName = ChallengeEvents[ChallengeIdx].file_list[FileIdx].file_name;
			// Update cache and save copy on disk
			FileCache.SaveTitleFile(DlName,ChallengeEvents[ChallengeIdx].file_list[FileIdx].file_name,FileContents);
			// Clear file from download memory
			FileDownloader.ClearDownloadedFile(DlName);
			// Call delegate to allow processing of loaded file
			OnDownloadChallengeFileComplete(true,ChallengeEvents[ChallengeIdx].unique_challenge_id,DlName,FileName,"");
		}
		else
		{
			// Download failed and cache read failed
			ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status = MCFS_Failed;
			OnDownloadChallengeFileComplete(false, ChallengeEvents[ChallengeIdx].unique_challenge_id, DlName, FileName, "FileNotFound");
		}
	}
	else
	{
		`log(`StaticLocation @ "Could not find DLName="$DlName @"for challenge");
	}
}

/**
 * Access the cached copy of the file data
 *
 * @param UniqueChallengeId id of challenge that owns the file
 * @param DlName download name of the file
 * @param OutFileContents byte array filled in with the file contents
 */
function GetChallengeFileContents(string UniqueChallengeId, string DlName, out array<byte> OutFileContents)
{
	local int ChallengeIdx, FileIdx;

	OutFileContents.Length = 0;

	ChallengeIdx = ChallengeEvents.Find('unique_challenge_id',UniqueChallengeId);
	if (ChallengeIdx != INDEX_NONE)
	{
		FileIdx = ChallengeEvents[ChallengeIdx].file_list.Find('dl_name',DlName);
		if (FileIdx != INDEX_NONE)
		{
			// Copy contents if file was loaded successfuly
			if (ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status != MCFS_Success ||
				!FileCache.GetTitleFileContents(DlName,OutFileContents))
			{
				`log(`StaticLocation@"No data loaded for file entry."
					$" UniqueChallengeId="$UniqueChallengeId
					$" DLName="$DlName);
			}
		}
		else
		{
			`log(`StaticLocation@"Couldn't find file entry."
				$" UniqueChallengeId="$UniqueChallengeId
				$" DLName="$DlName);
		}
	}
	else
	{
		`log(`StaticLocation@"Couldn't find event entry."
			$" UniqueChallengeId="$UniqueChallengeId);
	}
}

/**
 * Clear the cached memory copy of a challenge file
 *
 * @param UniqueChallengeId id of challenge that owns the file
 * @param DlName download name of the file
 */
function ClearCachedChallengeFile(string UniqueChallengeId, string DlName)
{
	local int ChallengeIdx, FileIdx;

	ChallengeIdx = ChallengeEvents.Find('unique_challenge_id',UniqueChallengeId);
	if (ChallengeIdx != INDEX_NONE)
	{
		FileIdx = ChallengeEvents[ChallengeIdx].file_list.Find('dl_name',DlName);
		if (FileIdx != INDEX_NONE)
		{
			if (ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status != MCFS_Pending)
			{
				// Clear memory copy of file
				FileCache.ClearCachedFile(DlName);
			}
			else
			{
				`log(`StaticLocation@"Can't clear. File download pending."
					$" UniqueChallengeId="$UniqueChallengeId
					$" DLName="$DlName);
			}
		}
		else
		{
			`log(`StaticLocation@"Couldn't find file entry."
				$" UniqueChallengeId="$UniqueChallengeId
				$" DLName="$DlName);
		}
	}
	else
	{
		`log(`StaticLocation@"Couldn't find event entry."
			$" UniqueChallengeId="$UniqueChallengeId);
	}
}

/**
 * Clear the cached memory copy of a challenge file
 *
 * @param UniqueChallengeId id of challenge that owns the file
 * @param DlName download name of the file
 */
function DeleteCachedChallengeFile(string UniqueChallengeId, string DlName)
{
	local int ChallengeIdx, FileIdx;

	ChallengeIdx = ChallengeEvents.Find('unique_challenge_id',UniqueChallengeId);
	if (ChallengeIdx != INDEX_NONE)
	{
		FileIdx = ChallengeEvents[ChallengeIdx].file_list.Find('dl_name',DlName);
		if (FileIdx != INDEX_NONE)
		{
			if (ChallengeEvents[ChallengeIdx].file_list[FileIdx].Status != MCFS_Pending)
			{
				// Delete disk copy of file
				FileCache.DeleteTitleFile(DlName);
			}
			else
			{
				`log(`StaticLocation@"Can't delete. File download pending."
					$" UniqueChallengeId="$UniqueChallengeId
					$" DLName="$DlName);
			}
		}
		else
		{
			`log(`StaticLocation@"Couldn't find file entry."
				$" UniqueChallengeId="$UniqueChallengeId
				$" DLName="$DlName);
		}
	}
	else
	{
		`log(`StaticLocation@"Couldn't find event entry."
			$" UniqueChallengeId="$UniqueChallengeId);
	}
}

/**
 * Initiates a web request to have user accept a challenge
 *
 * @param UniqueChallengeId id of challenge to accept
 * @param UniqueUserId id of user that wants to accept challenge
 */
function AcceptChallenge(string UniqueChallengeId, string UniqueUserId)
{
	local string Url,ErrorStr;
	local int ChallengeQueryIdx,UserQueryIdx;
	local bool bPending;
	local HttpRequestInterface Request;

	// Find an existing query that is in progress by the user
	UserQueryIdx = ChallengeUserRequests.Find('UniqueUserId',UniqueUserId);
	// Allocate if not found
	if (UserQueryIdx == INDEX_NONE)
	{
		UserQueryIdx = ChallengeUserRequests.Length;
		ChallengeUserRequests.Length = ChallengeUserRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].UniqueUserId = UniqueUserId;
	}
	// Find an existing query that is in progress by the user for the given challenge
	ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests.Find('UniqueChallengeId',UniqueChallengeId);
	// Allocate if not found
	if (ChallengeQueryIdx == INDEX_NONE)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests.Length;
		ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests.Length = ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests[ChallengeQueryIdx].UniqueChallengeId = UniqueChallengeId;
	}
	// Check to see if the query is already in flight
	if (ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests[ChallengeQueryIdx].HTTPRequest == None)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			// hold ref to request
			ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests[ChallengeQueryIdx].HTTPRequest = Request;
			// build Url and start it
			Url = GetBaseURL() $ AcceptChallengeUrl $ GetAppAccessURL()
				$"&uniqueChallengeId=" $ UniqueChallengeId
				$"&uniqueUserId=" $ UniqueUserId;
			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.SetHeader("Content-Type","multipart/form-data");
			Request.OnProcessRequestComplete = OnAcceptChallengeHTTPRequestComplete;
			if (Request.ProcessRequest())
			{
				bPending = true;
			}
			else
			{
				ErrorStr = "failed to start request, Url="$Url;
			}
		}
	}
	else
	{
		ErrorStr = "last request is still being processed";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	if (!bPending)
	{
		OnAcceptChallengeComplete(false,UniqueChallengeId,UniqueUserId,ErrorStr);
	}
}

/**
 * Called once the request/response has completed for accepting a single challenge from Mcp
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnAcceptChallengeHTTPRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local string UniqueChallengeId,UniqueUserId,ErrorStr;
	local bool bResult;
	local int UserQueryIdx,ChallengeQueryIdx;

	// Find the user,challenge query entries that initiated this HTTP request
	for (UserQueryIdx=0; UserQueryIdx < ChallengeUserRequests.Length; UserQueryIdx++)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests.Find('HTTPRequest',Request);
		if (ChallengeQueryIdx != INDEX_NONE)
		{
			// found it
			break;
		}
	}
	if (UserQueryIdx != INDEX_NONE &&
		ChallengeQueryIdx != INDEX_NONE)
	{
		// Clear out the last request
		ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests[ChallengeQueryIdx].HTTPRequest = None;
		// Id the challenge/user from matched http request
		UniqueUserId = ChallengeUserRequests[UserQueryIdx].UniqueUserId;
		UniqueChallengeId = ChallengeUserRequests[UserQueryIdx].ChallengeAcceptRequests[ChallengeQueryIdx].UniqueChallengeId;
		if (bWasSuccessful &&
			Response != None)
		{
			if (Response.GetResponseCode() == `HTTP_STATUS_OK)
			{
				bResult = true;
			}
			else
			{
				ErrorStr = "invalid server response code, status="$Response.GetResponseCode();
			}
		}
		else
		{
			ErrorStr = "no response";
		}
	}
	else
	{
		ErrorStr = "couldn't find user/challenge entry for request";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	OnAcceptChallengeComplete(bResult,UniqueChallengeId,UniqueUserId,ErrorStr);
}

/**
 * Initiates a web request to retrieve the current status of a challenge for a user
 *
 * @param UniqueChallengeId id of challenge to query
 * @param UniqueUserId id of user to retrieve challenge status for
 */
function QueryChallengeUserStatus(string UniqueChallengeId, string UniqueUserId)
{
	local string Url,ErrorStr;
	local int ChallengeQueryIdx,UserQueryIdx;
	local bool bPending;
	local HttpRequestInterface Request;

	// Find an existing query that is in progress by the user
	UserQueryIdx = ChallengeUserRequests.Find('UniqueUserId',UniqueUserId);
	// Allocate if not found
	if (UserQueryIdx == INDEX_NONE)
	{
		UserQueryIdx = ChallengeUserRequests.Length;
		ChallengeUserRequests.Length = ChallengeUserRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].UniqueUserId = UniqueUserId;
	}
	// Find an existing query that is in progress by the user for the given challenge
	ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Find('UniqueChallengeId',UniqueChallengeId);
	// Allocate if not found
	if (ChallengeQueryIdx == INDEX_NONE)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Length;
		ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Length = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].UniqueChallengeId = UniqueChallengeId;
	}
	// Check to see if the query is already in flight
	if (ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].HTTPRequest == None)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			// hold ref to request
			ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].HTTPRequest = Request;
			// build Url and start it
			Url = GetBaseURL() $ ChallengeStatusUrl $ GetAppAccessURL()
				$"&uniqueChallengeId=" $ UniqueChallengeId
				$"&uniqueUserId=" $ UniqueUserId;
			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.SetHeader("Content-Type","multipart/form-data");
			Request.OnProcessRequestComplete = OnQueryChallengeStatusHTTPRequestComplete;
			if (Request.ProcessRequest())
			{
				bPending = true;
			}
			else
			{
				ErrorStr = "failed to start request, Url="$Url;
			}
		}
	}
	else
	{
		ErrorStr = "last request is still being processed";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	if (!bPending)
	{
		OnQueryChallengeUserStatusComplete(false,UniqueChallengeId,UniqueUserId,ErrorStr);
	}
}

/**
 * Called once the request/response has completed for downloading the status for a single challenge from Mcp
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnQueryChallengeStatusHTTPRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local string JSONStr,UniqueChallengeId,UniqueUserId,ErrorStr;
	local bool bResult;
	local int UserQueryIdx,ChallengeQueryIdx,UserStatusIdx;

	// Find the user,challenge query entries that initiated this HTTP request
	for (UserQueryIdx=0; UserQueryIdx < ChallengeUserRequests.Length; UserQueryIdx++)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Find('HTTPRequest',Request);
		if (ChallengeQueryIdx != INDEX_NONE)
		{
			// found it
			break;
		}
	}
	if (UserQueryIdx < ChallengeUserRequests.Length &&
		ChallengeQueryIdx != INDEX_NONE)
	{
		// Clear out the last request
		ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].HTTPRequest = None;
		// Id the challenge/user from matched http request
		UniqueUserId = ChallengeUserRequests[UserQueryIdx].UniqueUserId;
		UniqueChallengeId = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].UniqueChallengeId;
		if (bWasSuccessful &&
			Response != None)
		{
			if (Response.GetResponseCode() == `HTTP_STATUS_OK)
			{
				JSONStr = Response.GetContentAsString();
				if (Len(JSONStr) > 0)
				{
					ImportJSON("TempChallengeUserStatus",JSONStr);
					if (Len(TempChallengeUserStatus.unique_challenge_id) > 0)
					{
						// find existing user status entry
						for (UserStatusIdx=0; UserStatusIdx < ChallengeUserStatus.Length; UserStatusIdx++)
						{
							if (ChallengeUserStatus[UserStatusIdx].unique_challenge_id == UniqueChallengeId &&
								ChallengeUserStatus[UserStatusIdx].unique_user_id == UniqueUserId)
							{
								break;
							}
						}
						// add new entry if not found
						if (UserStatusIdx == ChallengeUserStatus.Length)
						{
							ChallengeUserStatus.Length = ChallengeUserStatus.Length+1;
						}
						// copy imported values
						ChallengeUserStatus[UserStatusIdx] = TempChallengeUserStatus;
					}
					bResult = true;
				}
				else
				{
					ErrorStr = "no JSON response";
				}
			}
			else
			{
				ErrorStr = "invalid server response code, status="$Response.GetResponseCode();
			}
		}
		else
		{
			ErrorStr = "no response";
		}
	}
	else
	{
		ErrorStr = "couldn't find user/challenge entry for request";

	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	OnQueryChallengeUserStatusComplete(bResult,UniqueChallengeId,UniqueUserId,ErrorStr);
}

/**
 * Initiates a web request to retrieve the current status of a challenge for a list of users user
 *
 * @param UniqueChallengeId id of challenge to query
 * @param UniqueUserId id of user that is initiating the request
 * @param UserIdsToRead list of ids to read status for
 */
function QueryChallengeMultiUserStatus(string UniqueChallengeId, string UniqueUserId, const out array<string> UserIdsToRead)
{
	local string Url,ErrorStr,JSONStr;
	local int ChallengeQueryIdx,UserQueryIdx,UserIdIdx;
	local bool bPending;
	local HttpRequestInterface Request;

	// Find an existing query that is in progress by the user
	UserQueryIdx = ChallengeUserRequests.Find('UniqueUserId',UniqueUserId);
	// Allocate if not found
	if (UserQueryIdx == INDEX_NONE)
	{
		UserQueryIdx = ChallengeUserRequests.Length;
		ChallengeUserRequests.Length = ChallengeUserRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].UniqueUserId = UniqueUserId;
	}
	// Find an existing query that is in progress by the user for the given challenge
	ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Find('UniqueChallengeId',UniqueChallengeId);
	// Allocate if not found
	if (ChallengeQueryIdx == INDEX_NONE)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Length;
		ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Length = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].UniqueChallengeId = UniqueChallengeId;
	}
	// Check to see if the query is already in flight
	if (ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].HTTPRequest == None)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			// hold ref to request
			ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].HTTPRequest = Request;

			// Make a json string from list of user ids
			JSONStr = "[ ";
			for (UserIdIdx = 0; UserIdIdx < UserIdsToRead.Length; UserIdIdx++)
			{
				JSONStr $= "\"" $ UserIdsToRead[UserIdIdx] $ "\"";
				// Only add the string if this isn't the last item
				if (UserIdIdx + 1 < UserIdsToRead.Length)
				{
					JSONStr $= ",";
				}
			}
			JSONStr $= " ]";
			Request.SetContentAsString(JSONStr);

			// build Url and start it
			Url = GetBaseURL() $ ChallengeMultiStatusUrl $ GetAppAccessURL()
				$"&uniqueChallengeId=" $ UniqueChallengeId
				$"&uniqueUserId=" $ UniqueUserId;

			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.SetHeader("Content-Type","multipart/form-data");
			Request.OnProcessRequestComplete = OnQueryChallengeMultiStatusHTTPRequestComplete;
			if (Request.ProcessRequest())
			{
				bPending = true;
			}
			else
			{
				ErrorStr = "failed to start request, Url="$Url;
			}
		}
	}
	else
	{
		ErrorStr = "last request is still being processed";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	if (!bPending)
	{
		OnQueryChallengeUserStatusComplete(false,UniqueChallengeId,UniqueUserId,ErrorStr);
	}
}

/**
 * Called once the request/response has completed for downloading the status for a single challenge from Mcp
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnQueryChallengeMultiStatusHTTPRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local string JSONStr,UniqueChallengeId,UniqueUserId,ErrorStr;
	local bool bResult;
	local int UserQueryIdx,ChallengeQueryIdx,UserStatusIdx,TempUserStatusIdx;

	// Find the user,challenge query entries that initiated this HTTP request
	for (UserQueryIdx=0; UserQueryIdx < ChallengeUserRequests.Length; UserQueryIdx++)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests.Find('HTTPRequest',Request);
		if (ChallengeQueryIdx != INDEX_NONE)
		{
			// found it
			break;
		}
	}
	if (UserQueryIdx < ChallengeUserRequests.Length &&
		ChallengeQueryIdx != INDEX_NONE)
	{
		// Clear out the last request
		ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].HTTPRequest = None;
		// Id the challenge/user from matched http request
		UniqueUserId = ChallengeUserRequests[UserQueryIdx].UniqueUserId;
		UniqueChallengeId = ChallengeUserRequests[UserQueryIdx].ChallengeStatusRequests[ChallengeQueryIdx].UniqueChallengeId;
		if (bWasSuccessful &&
			Response != None)
		{
			if (Response.GetResponseCode() == `HTTP_STATUS_OK)
			{
				JSONStr = Response.GetContentAsString();
				if (Len(JSONStr) > 0)
				{
					TempChallengeUserStatusArray.Length = 0;
					ImportJSON("TempChallengeUserStatusArray",JSONStr);
					if (TempChallengeUserStatusArray.Length > 0)
					{
						// copy all unique imported entries to global list
						for (TempUserStatusIdx=0; TempUserStatusIdx < TempChallengeUserStatusArray.Length; TempUserStatusIdx++)
						{
							// find existing user status entry
							for (UserStatusIdx=0; UserStatusIdx < ChallengeUserStatus.Length; UserStatusIdx++)
							{
								if (ChallengeUserStatus[UserStatusIdx].unique_challenge_id == TempChallengeUserStatusArray[TempUserStatusIdx].unique_challenge_id &&
									ChallengeUserStatus[UserStatusIdx].unique_user_id == TempChallengeUserStatusArray[TempUserStatusIdx].unique_user_id)
								{
									break;
								}
							}
							// add new entry if not found
							if (UserStatusIdx == ChallengeUserStatus.Length)
							{
								ChallengeUserStatus.Length = ChallengeUserStatus.Length+1;
							}
							// copy imported values
							ChallengeUserStatus[UserStatusIdx] = TempChallengeUserStatusArray[TempUserStatusIdx];
						}
					}
					bResult = true;
				}
				else
				{
					ErrorStr = "no JSON response";
				}
			}
			else
			{
				ErrorStr = "invalid server response code, status="$Response.GetResponseCode();
			}
		}
		else
		{
			ErrorStr = "no response";
		}
	}
	else
	{
		ErrorStr = "couldn't find user/challenge entry for request";

	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	OnQueryChallengeUserStatusComplete(bResult,UniqueChallengeId,UniqueUserId,ErrorStr);
}

/**
 * Get the cached status of a user for a challenge. Use QueryChallengeUserStatus first
 *
 * @param UniqueChallengeId id of challenge to retrieve
 * @param UniqueUserId id of user to retrieve challenge status for
 * @param OutChallengeUserStatus user status values to be filled in
 */
function GetChallengeUserStatus(string UniqueChallengeId, string UniqueUserId, out McpClashMobChallengeUserStatus OutChallengeUserStatus)
{
	local int UserStatusIdx;
	local McpClashMobChallengeUserStatus DefaultStatus;

	// find existing user status entry
	for (UserStatusIdx = 0; UserStatusIdx < ChallengeUserStatus.Length; UserStatusIdx++)
	{
		if (ChallengeUserStatus[UserStatusIdx].unique_challenge_id == UniqueChallengeId &&
			ChallengeUserStatus[UserStatusIdx].unique_user_id == UniqueUserId)
		{
			break;
		}
	}

	// copy result
	if (UserStatusIdx < ChallengeUserStatus.Length)
	{
		OutChallengeUserStatus = ChallengeUserStatus[UserStatusIdx];
	}
	else
	{
		// If the user has never sent UserStatus data for this challenge then
		// there is no UserStatus to retrieve. In that case we need to manually
		// create the correct McpClashMobChallengeUserStatus data.
		OutChallengeUserStatus = DefaultStatus;
		OutChallengeUserStatus.unique_challenge_id = UniqueChallengeId;
		OutChallengeUserStatus.unique_user_id = UniqueUserId;

		`log(`StaticLocation@"Couldn't find user status for"
			$" UniqueChallengeId="$UniqueChallengeId
			$" UniqueUserId="$UniqueUserId);
	}
}

/**
 * Initiates a web request to update the current progress of a challenge for a user
 *
 * @param UniqueChallengeId id of challenge to update
 * @param UniqueUserId id of user to update challenge progress for
 */
function UpdateChallengeUserProgress(string UniqueChallengeId, string UniqueUserId, bool bDidComplete, int GoalProgress)
{
	local string Url,ErrorStr;
	local int ChallengeQueryIdx,UserQueryIdx;
	local bool bPending;
	local HttpRequestInterface Request;

	// Find an existing request that is in progress by the user
	UserQueryIdx = ChallengeUserRequests.Find('UniqueUserId',UniqueUserId);
	// Allocate if not found
	if (UserQueryIdx == INDEX_NONE)
	{
		UserQueryIdx = ChallengeUserRequests.Length;
		ChallengeUserRequests.Length = ChallengeUserRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].UniqueUserId = UniqueUserId;
	}
	// Find an existing request that is in progress by the user for the given challenge
	ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests.Find('UniqueChallengeId',UniqueChallengeId);
	// Allocate if not found
	if (ChallengeQueryIdx == INDEX_NONE)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests.Length;
		ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests.Length = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests[ChallengeQueryIdx].UniqueChallengeId = UniqueChallengeId;
	}
	// Check to see if the query is already in flight
	if (ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests[ChallengeQueryIdx].HTTPRequest == None)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			// hold ref to request
			ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests[ChallengeQueryIdx].HTTPRequest = Request;
			// build Url and start it
			Url = GetBaseURL() $ UpdateChallengeProgressUrl $ GetAppAccessURL()
				$"&uniqueChallengeId=" $ UniqueChallengeId
				$"&uniqueUserId=" $ UniqueUserId
				$"&didComplete=" $ bDidComplete
				$"&goalProgress=" $ GoalProgress;

			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.SetHeader("Content-Type","multipart/form-data");
			Request.OnProcessRequestComplete = OnUpdateChallengeUserProgressHTTPRequestComplete;
			if (Request.ProcessRequest())
			{
				bPending = true;
			}
			else
			{
				ErrorStr = "failed to start request, Url="$Url;
			}
		}
	}
	else
	{
		ErrorStr = "last request is still being processed";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	if (!bPending)
	{
		OnUpdateChallengeUserProgressComplete(false,UniqueChallengeId,UniqueUserId,ErrorStr);
	}
}

/**
 * Called once the request/response has completed for updating a user's progress for a single challenge from Mcp
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnUpdateChallengeUserProgressHTTPRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local string UniqueChallengeId,UniqueUserId,ErrorStr;
	local bool bResult;
	local int UserQueryIdx,ChallengeQueryIdx;

	// Find the user,challenge query entries that initiated this HTTP request
	for (UserQueryIdx=0; UserQueryIdx < ChallengeUserRequests.Length; UserQueryIdx++)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests.Find('HTTPRequest',Request);
		if (ChallengeQueryIdx != INDEX_NONE)
		{
			// found it
			break;
		}
	}
	if (UserQueryIdx != INDEX_NONE &&
		ChallengeQueryIdx != INDEX_NONE)
	{
		// Clear out the last request
		ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests[ChallengeQueryIdx].HTTPRequest = None;
		// Id the challenge/user from matched http request
		UniqueUserId = ChallengeUserRequests[UserQueryIdx].UniqueUserId;
		UniqueChallengeId = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateProgressRequests[ChallengeQueryIdx].UniqueChallengeId;
		if (bWasSuccessful &&
			Response != None)
		{
			if (Response.GetResponseCode() == `HTTP_STATUS_OK)
			{
				bResult = true;
			}
			else
			{
				ErrorStr = "invalid server response code, status="$Response.GetResponseCode();
			}
		}
		else
		{
			ErrorStr = "no response";
		}
	}
	else
	{
		ErrorStr = "couldn't find user/challenge entry for request";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	OnUpdateChallengeUserProgressComplete(bResult,UniqueChallengeId,UniqueUserId,ErrorStr);
}

/**
 * Initiates a web request to update the current reward given to a user for a challenge
 *
 * @param UniqueChallengeId id of challenge to update
 * @param UniqueUserId id of user to update challenge progress for
 */
function UpdateChallengeUserReward(string UniqueChallengeId, string UniqueUserId, int UserReward)
{
	local string Url,ErrorStr;
	local int ChallengeQueryIdx,UserQueryIdx;
	local bool bPending;
	local HttpRequestInterface Request;

	// Find an existing request that is in progress by the user
	UserQueryIdx = ChallengeUserRequests.Find('UniqueUserId',UniqueUserId);
	// Allocate if not found
	if (UserQueryIdx == INDEX_NONE)
	{
		UserQueryIdx = ChallengeUserRequests.Length;
		ChallengeUserRequests.Length = ChallengeUserRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].UniqueUserId = UniqueUserId;
	}
	// Find an existing request that is in progress by the user for the given challenge
	ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests.Find('UniqueChallengeId',UniqueChallengeId);
	// Allocate if not found
	if (ChallengeQueryIdx == INDEX_NONE)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests.Length;
		ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests.Length = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests.Length+1;
		ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests[ChallengeQueryIdx].UniqueChallengeId = UniqueChallengeId;
	}
	// Check to see if the query is already in flight
	if (ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests[ChallengeQueryIdx].HTTPRequest == None)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			// hold ref to request
			ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests[ChallengeQueryIdx].HTTPRequest = Request;
			// build Url and start it
			Url = GetBaseURL() $ UpdateRewardProgressUrl $ GetAppAccessURL()
				$"&uniqueChallengeId=" $ UniqueChallengeId
				$"&uniqueUserId=" $ UniqueUserId
				$"&userAwardGiven=" $ UserReward;

			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.SetHeader("Content-Type","multipart/form-data");
			Request.OnProcessRequestComplete = OnUpdateChallengeUserRewardHTTPRequestComplete;
			if (Request.ProcessRequest())
			{
				bPending = true;
			}
			else
			{
				ErrorStr = "failed to start request, Url="$Url;
			}
		}
	}
	else
	{
		ErrorStr = "last request is still being processed";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	if (!bPending)
	{
		OnUpdateChallengeUserRewardComplete(false,UniqueChallengeId,UniqueUserId,ErrorStr);
	}
}

/**
 * Called once the request/response has completed for updating a user's reward for a single challenge from Mcp
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnUpdateChallengeUserRewardHTTPRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local string UniqueChallengeId,UniqueUserId,ErrorStr;
	local bool bResult;
	local int UserQueryIdx,ChallengeQueryIdx;

	// Find the user,challenge query entries that initiated this HTTP request
	for (UserQueryIdx=0; UserQueryIdx < ChallengeUserRequests.Length; UserQueryIdx++)
	{
		ChallengeQueryIdx = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests.Find('HTTPRequest',Request);
		if (ChallengeQueryIdx != INDEX_NONE)
		{
			// found it
			break;
		}
	}
	if (UserQueryIdx != INDEX_NONE &&
		ChallengeQueryIdx != INDEX_NONE)
	{
		// Clear out the last request
		ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests[ChallengeQueryIdx].HTTPRequest = None;
		// Id the challenge/user from matched http request
		UniqueUserId = ChallengeUserRequests[UserQueryIdx].UniqueUserId;
		UniqueChallengeId = ChallengeUserRequests[UserQueryIdx].ChallengeUpdateRewardRequests[ChallengeQueryIdx].UniqueChallengeId;
		if (bWasSuccessful &&
			Response != None)
		{
			if (Response.GetResponseCode() == `HTTP_STATUS_OK)
			{
				bResult = true;
			}
			else
			{
				ErrorStr = "invalid server response code, status="$Response.GetResponseCode();
			}
		}
		else
		{
			ErrorStr = "no response";
		}
	}
	else
	{
		ErrorStr = "couldn't find user/challenge entry for request";
	}
	if (Len(ErrorStr) > 0)
	{
		`log(`StaticLocation@ErrorStr);
	}
	OnUpdateChallengeUserRewardComplete(bResult,UniqueChallengeId,UniqueUserId,ErrorStr);
}
