/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Title file downloading implementation via web service request
 */
class OnlineTitleFileDownloadWeb extends OnlineTitleFileDownloadBase
	native;

`include(Engine\Classes\HttpStatusCodes.uci); 

/** The list of title files that have been read or are being read */
var private array<TitleFileWeb> TitleFiles;

/**
 * Uncompress the title file
 *
 * @param FileCompressionType enum that that informs how to uncompress the file
 * @param CompressedFileContents Source data to uncompress
 * @param UncompressedFileContents the out buffer to copy the data into 
 */
native function bool UncompressTitleFileContents(EMcpFileCompressionType FileCompressionType, const out array<byte> CompressedFileContents, out array<byte> UncompressedFileContents);

/**
 * Starts an asynchronous read of the specified file from the network platform's
 * title specific file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool ReadTitleFile(string FileToRead)
{
	local int FileIndex,Idx;
	local string URL;

	// check for a prior request
	FileIndex = INDEX_NONE;
	for (Idx=0; Idx < TitleFiles.Length; Idx++)
	{
		// case sensitive
		if (InStr(TitleFiles[Idx].Filename,FileToRead,true,false) != INDEX_NONE)
		{
			FileIndex = Idx;
			break;
		}
	}
	// add new entry for this file request if not found
	if (FileIndex == INDEX_NONE)
	{
		FileIndex = TitleFiles.Length;
		TitleFiles.Length = TitleFiles.Length + 1;
		TitleFiles[FileIndex].Filename = FileToRead;
		TitleFiles[FileIndex].AsyncState = OERS_NotStarted;
	}
	// file has been downloaded before successfully so already done
	if (TitleFiles[FileIndex].AsyncState == OERS_Done)
	{
		TriggerDelegates(true,FileToRead);
	}
	// file has been downloaded before but failed
	else if (TitleFiles[FileIndex].AsyncState == OERS_Failed)
	{
		TriggerDelegates(false,FileToRead);
		return false;
	}
	// download needs to start if not already in progress
	else if (TitleFiles[FileIndex].AsyncState != OERS_InProgress)
	{
		// mark the file entry as pending download
		TitleFiles[FileIndex].AsyncState = OERS_InProgress;
		// tack on the filename to the base/overridden URL
		URL = GetUrlForFile(FileToRead) $ FileToRead;
		`Log(`location @ "starting read for title file"
			@"url="$URL);
		// send off web request and register for delegate for its completion
		TitleFiles[FileIndex].HTTPRequest = class'HttpFactory'.static.CreateRequest();
		if (TitleFiles[FileIndex].HTTPRequest != None)
		{
			TitleFiles[FileIndex].HTTPRequest.OnProcessRequestComplete = OnFileDownloadComplete;
			TitleFiles[FileIndex].HTTPRequest.SetVerb("GET");
			TitleFiles[FileIndex].HTTPRequest.SetURL(URL);
			TitleFiles[FileIndex].HTTPRequest.ProcessRequest();
		}
	}
	return true;
}

/**
 * Delegate called on each completed web request
 *
 * @param	OriginalRequest - The original request object that spawned the response
 * @param	HttpResponse - The response object. Could be None if the request failed spectacularly. If the request failed to receive a complete
 *							response for some reason, this could contain a valid Response object with as much info as could be retrieved.
 *							Always use the bDidSucceed parameter to determine if the entire response was received successfully.
 * @param	bSucceeded - whether the response succeeded. If it did not, you should not trust the payload or headers. 
 *							Basically indicates a net failure occurred while receiving the response.
 */
private function OnFileDownloadComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bDidSucceed)
{
	local bool bSuccess;
	local int FileIndex,Idx;
	local string Filename;
	local array<byte> BinaryData;
	local string FileCompressionTypeString;

	if (bDidSucceed)
	{
		FileIndex = INDEX_NONE;
		// find file entry for this request based on what was passed on the URL
		for (Idx=0; Idx < TitleFiles.Length; Idx++)
		{
			if (TitleFiles[Idx].HTTPRequest == Request)
			{
				FileIndex = Idx;
				break;
			}
		}
		`Log(`location @ ""
			@"FileIndex="$FileIndex
			@"OriginalURL="$Request.GetURL()
			@"ResponseCode="$Response.GetResponseCode()
			@"ContentLength="$Response.GetContentLength());

		if (FileIndex != INDEX_NONE)
		{
			Filename = TitleFiles[FileIndex].Filename;
			// remove ref to request since it is now complete
			TitleFiles[FileIndex].HTTPRequest = None;
			TitleFiles[FileIndex].AsyncState = OERS_Failed;
				// only successful response code as we're not handling any redirects
			if (Response.GetResponseCode() == `HTTP_STATUS_OK)
				{
					bSuccess = true;
				
					// copy the payload data from the web request
				Response.GetContent(BinaryData);
				TitleFiles[FileIndex].Data = BinaryData;
				// Get the Compression Type from the Response Header
				FileCompressionTypeString = Response.GetHeader("Mcp-Content-Encoding");
				// Convert CompressionTypeString stored in the datastore to the enum used on the client
				switch(FileCompressionTypeString)
				{
					case "MFCT_ZLIB":
						TitleFiles[FileIndex].FileCompressionType = MFCT_ZLIB;
						break;
					default:
						TitleFiles[FileIndex].FileCompressionType = MFCT_NONE;
				}
					// mark as successfully done
					TitleFiles[FileIndex].AsyncState = OERS_Done;
				}
			else
			{
				// clear failed data
				TitleFiles[FileIndex].AsyncState = OERS_Failed;
				TitleFiles[FileIndex].Data.Length = 0;
			}
		}
		else
		{
			`Log(`location @ "No entry found for"
				@"FileIndex="$FileIndex);
		}
	}
	else
	{
		`Log(`location @ "web request for file download failed");
	}
	// web request is complete and delegate is triggered for success/failure 
	TriggerDelegates(bSuccess,Filename);
}

/**
 * Runs the delegates registered on this interface for each file download request
 *
 * @param bSuccess true if the request was successful
 * @param FileRead name of the file that was read
 */
private native function TriggerDelegates(bool bSuccess,string FileRead);

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
native function bool GetTitleFileContents(string FileName,out array<byte> FileContents);

/**
 * Determines the async state of the tile file read operation
 *
 * @param FileName the name of the file to check on
 *
 * @return the async state of the file read
 */
function EOnlineEnumerationReadState GetTitleFileState(string FileName)
{
	local int FileIndex;

	FileIndex = TitleFiles.Find('FileName',FileName);
	if (FileIndex != INDEX_NONE)
	{
		return TitleFiles[FileIndex].AsyncState;
	}
	return OERS_Failed;
}

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
native function bool ClearDownloadedFiles();

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
native function bool ClearDownloadedFile(string FileName);

/**
 * Async call to request a list of files (returned as string) from EMS
 */
function RequestTitleFileList()
{
	local HttpRequestInterface HTTPRequest;
	local string URL;

	HTTPRequest = class'HttpFactory'.static.CreateRequest();
	if (HTTPRequest != None)
	{
		URL = GetBaseURL() $ RequestFileListURL $ GetAppAccessURL();
		HTTPRequest.OnProcessRequestComplete = OnFileListReceived;
		HTTPRequest.SetVerb("GET");
		HTTPRequest.SetURL(URL);
		HTTPRequest.ProcessRequest();
	}
	else
	{
		`log(`location@"HTTPRequest object missing");
	}
}

/**
 * Delegate for when the EMS file list is received
 *
 * @param	OriginalRequest - The original request object that spawned the response
 * @param	HttpResponse - The response object. Could be None if the request failed spectacularly. If the request failed to receive a complete
 *							response for some reason, this could contain a valid Response object with as much info as could be retrieved.
 *							Always use the bDidSucceed parameter to determine if the entire response was received successfully.
 * @param	bSucceeded - whether the response succeeded. If it did not, you should not trust the payload or headers. 
 *							Basically indicates a net failure occurred while receiving the response.
 */
function OnFileListReceived(HttpRequestInterface Request, HttpResponseInterface Response, bool bDidSucceed)
{
	local int Index;
	local delegate<OnRequestTitleFileListComplete> RequestTitleFileListDelegate;
	local string ResponseStr;
	local bool bSuccess;

	if (bDidSucceed)
	{
		if (Response != None &&
			Response.GetResponseCode() == `HTTP_STATUS_OK)
		{
			ResponseStr = Response.GetContentAsString();
			bSuccess = true;
		}
		else
		{
			`log(`location@"Download of file list failed. Bad response."
				@"ResponseCode="$Response.GetResponseCode()
				@"URL="$Request.GetURL());
		}
	}
	else
	{
		`log(`location@"Download of file list failed.");
	}

	// Call the completion delegate for receiving the file list
	for (Index=0; Index < RequestTitleFileListCompleteDelegates.Length; Index++)
	{
		RequestTitleFileListDelegate = RequestTitleFileListCompleteDelegates[Index];
		if (RequestTitleFileListDelegate != None)
		{
			RequestTitleFileListDelegate(bSuccess,ResponseStr);
		}
	}
}

/**
 * Build the clashmob specific Url for downloading a given file
 *
 * @param FileName the file to search the table for
 *
 * @param the URL to use to request the file or BaseURL if no special mapping is present
 */
function string GetUrlForFile(string FileName)
{
	local string Url;

	Url = GetBaseURL() $ RequestFileURL $ GetAppAccessURL() $
		"&dlName=";

	return Url;
}

cpptext
{
	/**
	 * Ticks any outstanding async tasks that need processing
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Searches the list of files for the one that matches the filename
	 *
	 * @param FileName the file to search for
	 *
	 * @return the file details
	 */
	FTitleFileWeb* GetTitleFile(const FString& FileName);
}