/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides the interface for user messages and the factory method
 * for creating the registered implementing object
 */
class McpMessageManager extends McpMessageBase
	native
	config(Engine)
	inherits(FTickableObject); 

`include(Engine\Classes\HttpStatusCodes.uci)

/** The URL of CreateMessage function on the server */
var config string CreateMessageUrl;

/** The URL of DeleteMessage function on the server */
var config string DeleteMessageUrl;

/** The URL of ListMessages function on the server */
var config string QueryMessagesUrl;

/** The URL of ListMessageContents function on the server */
var config string QueryMessageContentsUrl;

/** The URL of DeleteAllMessages function on the server */
var config string DeleteAllMessagesUrl;

/**
*CompressMessageRequest holds the information needed by the async compression function
*/
struct native McpCompressMessageRequest
{
	/** Uncompressed Source Buffer */
	var array<byte> SourceBuffer;
	/** Bufffer to hold the compressed data; set after compression is finished */
	var array<byte> DestBuffer;
	/** Size of the final compressed data */
	var int OutCompressedSize;
	/** Http Request */
	var HttpRequestInterface Request;
	/** The compression worker that will do the compression */
	var native pointer CompressionWorker{FAsyncTask<FCompressAsyncWorker>};
};

/**
*UncompressMessageRequest holds the information needed by async uncompression 
*/
struct native McpUncompressMessageRequest
{
	/** Id of message that the compressed payload belongs to */
	var string MessageId;
	/** Compressed Source Buffer */
	var array<byte> SourceBuffer;
	/** Bufffer to hold the uncompressed data; set after uncompression is finished */
	var array<byte> DestBuffer;
	/** Size of the final uncompressed data; passed in before compression from the header of the compressed buffer */
	var int OutUncompressedSize;
	/** The uncompression worker that will do the uncompression */
	var native pointer UncompressionWorker{FAsyncTask<FUncompressAsyncWorker>};
};

/** List of McpCompressMessageRequests to be compressed */
var native array<McpCompressMessageRequest> CompressMessageRequests;
/** List of McpUncompressMessageRequests to be compressed */
var native array<McpUncompressMessageRequest> UncompressMessageRequests;

/**
 * Called to start the compression process.
 * Adds a compression task to the compression queue
 *
 * @param MessageCompressionType 
 * @param MessageContent
 * @param Request
 */
native function bool StartAsyncCompression(EMcpMessageCompressionType MessageCompressionType, const out array<byte> MessageContent, HttpRequestInterface Request);

/**
 * Called to start the uncompression process.
 * Adds a compression task to the uncompression queue
 *
 * @param MessageId 
 * @param MessageCompressionType
 * @param MessageContent
 */
native function bool StartAsyncUncompression(string MessageId, EMcpMessageCompressionType MessageCompressionType, const out array<byte> MessageContent);

/**
 * Called once the uncompression job has finished
 * - Ensures that uncompressed message contents are cached and the query's final delegate is called
 *
 * @param bWasSuccessful passes whether or not the uncompression task completed successfully
 * @param UncompressedMessageContents 
 * @param MessageId
 */
event FinishedAsyncUncompression(bool bWasSuccessful, const out array<byte> UncompressedMessageContents, string MessageId)
{
	if(!CacheMessageContents(UncompressedMessageContents, MessageId))
	{
		`Log(`Location("Error Caching Message Contents"));
	}
	OnQueryMessageContentsComplete(MessageId, bWasSuccessful, "");
}

/**
 * Creates the URL and sends the request to create a Message.
 *  - This updates the message on the server. QueryMessages will need to be 
 *  - run again before GetMessages will reflect the this new message.
 * @param ToUniqueUserIds the ids of users to send a message to
 * @param FromUniqueUserId of the user who sent the message
 * @param FromFriendlyName The friendly name of the user sending the data
 * @param MessageType The application specific message type
 * @param PushMessage to be sent to user via push notifications
 * @param ValidUntil The date until this message is no longer valid (should be deleted)
 * @param MessageContents payload of the message
 */
function CreateMessage(
	const out array<String> ToUniqueUserIds, 
	String FromUniqueUserId, 
	String FromFriendlyName, 
	String MessageType, 
	String PushMessage, 
	String ValidUntil,
	const out array<byte> MessageContents)
{
	local string Url;
	local HttpRequestInterface CreateMessageRequest;
	local McpMessage Message;
	local string ToUniqueUserIdsStr;
	local int Idx;

	// Create HttpRequest
	CreateMessageRequest = class'HttpFactory'.static.CreateRequest();

	if(CreateMessageRequest != none)
	{
		// create comma delimited list of recipient ids
		for (Idx=0; Idx < ToUniqueUserIds.Length; Idx++)
		{
			ToUniqueUserIdsStr $= ToUniqueUserIds[Idx];
			if ((ToUniqueUserIds.Length - Idx) > 1)
			{
				 ToUniqueUserIdsStr $=  ",";
			}
		}
		// Fill url out using parameters
		Url = GetBaseURL() $ CreateMessageUrl $ GetAppAccessURL() $
			"&toUniqueUserIds=" $ ToUniqueUserIdsStr $ 
			"&fromUniqueUserId=" $ FromUniqueUserId $ 
			"&fromFriendlyName=" $ FromFriendlyName $ 
			"&messageType=" $ MessageType $ 
			"&pushMessage=" $ PushMessage $ 
			"&messageCompressionType=" $ CompressionType $ 
			// JS uses DateTo Param
			"&validUntil=" $ ValidUntil
			;

		// Build our web request with the above URL
		CreateMessageRequest.SetURL(URL);
		CreateMessageRequest.SetHeader("Content-Type","multipart/form-data");
		CreateMessageRequest.SetVerb("POST");

		CreateMessageRequest.OnProcessRequestComplete = OnCreateMessageRequestComplete;
		
		//check to see if we need compression
		if (CompressionType == MMCT_LZO || 
			CompressionType == MMCT_ZLIB)
		{
			if( !StartAsyncCompression(CompressionType, MessageContents, CreateMessageRequest))
			{
				OnCreateMessageComplete(Message, false, "Failed to Start Async Compression.");
			}
		}
		else
		{
			CreateMessageRequest.SetContent(MessageContents);
			// Call Web Request
			if (!CreateMessageRequest.ProcessRequest())
			{
				`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
			}
			`Log(`Location $ " URL is " $ Url);
		}
	}
}


/**
 * Called once the request/response has completed. 
 * Used to return any errors and notify any registered delegate
 * 
 * @param CreateMessageRequest the request object that was used
 * @param HttpResponse the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnCreateMessageRequestComplete(HttpRequestInterface CreateMessageRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	
	local int ResponseCode;
	local string Content; 
	local McpMessage CreatedMessage;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	if (HttpResponse != none && CreateMessageRequest != none)
	{
		ResponseCode = HttpResponse.GetResponseCode();

		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_CREATED;

		Content = HttpResponse.GetContentAsString();

		if ( bWasSuccessful)
		{
			`log(`Location@"Message created.");
		}
		else
		{
			`log(`Location@" CreateMessage query did not return a message.");
		}
	}
	OnCreateMessageComplete(CreatedMessage, bWasSuccessful, Content);
}

/**
 * Deletes a Message by MessageId
 * 
 * @param MessageId Id of the group
 */
function DeleteMessage(String MessageId)
{
	local string Url;
	local HttpRequestInterface  DeleteMessageRequest;

	// Delete HttpRequest
	DeleteMessageRequest = class'HttpFactory'.static.CreateRequest();

	if(DeleteMessageRequest != none)
	{
		// Fill url out using parameters
		Url = GetBaseURL() $ DeleteMessageUrl $ GetAppAccessURL() $
			"&messageId=" $ MessageId;

		// Build our web request with the above URL
		DeleteMessageRequest.SetVerb("DELETE");
		DeleteMessageRequest.SetURL(URL);
		DeleteMessageRequest.OnProcessRequestComplete = OnDeleteMessageRequestComplete;

		// call WebRequest
		if(!DeleteMessageRequest.ProcessRequest())
		{
			`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
		}
		`Log(`Location $ "URL is " $ Url);
	}	
}

/**
 * Called once the request/response has completed. 
 * Used to process the response and notify any
 * registered delegate
 * 
 * @param OriginalRequest the request object that was used
 * @param HttpResponse the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnDeleteMessageRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Content; 
	local String MessageId;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	if (HttpResponse != none)
	{
		ResponseCode = HttpResponse.GetResponseCode();
		MessageId = HttpResponse.GetURLParameter("MessageId");
		ResponseCode = HttpResponse.GetResponseCode();
		Content = HttpResponse.GetContentAsString();
	}

	bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;

	OnDeleteMessageComplete(MessageId, bWasSuccessful, Content);
}

/**
 * Queries the backend for the Messages belonging to the supplied UserId
 * 
 * @param ToUniqueUserId the id of the owner of the groups to return
 */
function QueryMessages(String ToUniqueUserId)
{
	// Cache one message result set per user
	local string Url;
	local HttpRequestInterface QueryMessagesRequest;

	// List HttpRequest
	QueryMessagesRequest = class'HttpFactory'.static.CreateRequest();
	if (QueryMessagesRequest != none)
	{
		//Create URL parameters
		Url = GetBaseURL() $ QueryMessagesUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ ToUniqueUserId;

		// Build our web request with the above URL
		QueryMessagesRequest.SetURL(URL);
		QueryMessagesRequest.SetVerb("GET");
		QueryMessagesRequest.OnProcessRequestComplete = OnQueryMessagesRequestComplete;
		`Log(`Location@"URL: " $ URL);
		// Call WebRequest
		if(!QueryMessagesRequest.ProcessRequest())
		{
			`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
		}
	}
}

/**
 * Called once the request/response has completed. Used to process the returned data and notify any
 * registered delegate
 * 
 * @param OriginalRequest the request object that was used
 * @param HttpResponse the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnQueryMessagesRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Error;
	local string JsonString;
	local JsonObject ParsedJson;
	local int JsonIndex;
	local McpMessage Message;
	local string MessageCompressionTypeString;

	// Set default response code
	ResponseCode = `HTTP_STATUS_SERVER_ERROR;

	// Both HttpResponse and OriginalRequest need to be present
	if (HttpResponse != none && OriginalRequest != none)
	{
		ResponseCode = HttpResponse.GetResponseCode();

		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
			if (bWasSuccessful)
			{
				JsonString = HttpResponse.GetContentAsString();
				if (JsonString != "")
				{
					// @todo joeg - Replace with Wes' ImportJson() once it's implemented
					// Parse the json
					ParsedJson = class'JsonObject'.static.DecodeJson(JsonString);
					// Add each mapping in the json packet if missing
					for (JsonIndex = 0; JsonIndex < ParsedJson.ObjectArray.Length; JsonIndex++)
					{
						
						Message.MessageId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("message_id");
						Message.ToUniqueUserId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("to_unique_user_id");
						Message.FromUniqueUserId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("from_unique_user_id");
						Message.FromFriendlyName = ParsedJson.ObjectArray[JsonIndex].GetStringValue("from_friendly_name");
						Message.MessageType = ParsedJson.ObjectArray[JsonIndex].GetStringValue("message_type");
						MessageCompressionTypeString = ParsedJson.ObjectArray[JsonIndex].GetStringValue("message_compression_type");
						Message.ValidUntil = ParsedJson.ObjectArray[JsonIndex].GetStringValue("valid_until");

						// Convert CompressionTypeString stored in the datastore to the enum used on the client
						switch(MessageCompressionTypeString)
						{
							case "MMCT_LZO":
								Message.MessageCompressionType = MMCT_LZO;
								break;
							case "MMCT_ZLIB":
								Message.MessageCompressionType = MMCT_ZLIB;
								break;
							default:
								Message.MessageCompressionType = MMCT_NONE;
						}

						CacheMessage(Message);
					}
				}
				else
				{
					Error = "Query did not return any content in it's response.";
					`log(`Location $ Error);
				}
			}
			else
			{
				Error = HttpResponse.GetContentAsString();
			}
	}
	OnQueryMessagesComplete(Message.ToUniqueUserId, bWasSuccessful, Error);
}

/**
 * Returns the set of messages that sent to the specified UserId
 * Called after QueryMessages. 
 * 
 * @param ToUniqueUserId the user the messages were sent to
 * @param MessageList collection of messages
 */
function GetMessageList(string ToUniqueUserId, out McpMessageList MessageList)
{
	local int MessageListIndex;

	// Check Cache Variable MessageLists
	MessageListIndex = MessageLists.Find('ToUniqueUserId', ToUniqueUserId);
	if(MessageListIndex != INDEX_NONE)
	{
		MessageList = MessageLists[MessageListIndex];
	}
	else
	{
		`Log(`Location $ " Requester Id not found or MessageLists is empty. Using ToUniqueUserId: " $ ToUniqueUserId);
	}
}

/**
 * Queries the back-end for the Messages Contents belonging to the specified group
 * 
 * @param MessageId the id of the message to return
 */
function QueryMessageContents(String MessageId)
{
	// Cache one message result set per user
	local string Url;
	local HttpRequestInterface QueryMessageContentsRequest;

	// List HttpRequest
	QueryMessageContentsRequest = class'HttpFactory'.static.CreateRequest();
	if (QueryMessageContentsRequest != none)
	{
		//Create URL parameters
		Url = GetBaseURL() $ QueryMessageContentsUrl $ GetAppAccessURL() $
			"&messageId=" $ MessageId;

		// Build our web request with the above URL
		QueryMessageContentsRequest.SetURL(URL);
		QueryMessageContentsRequest.SetVerb("GET");
		QueryMessageContentsRequest.OnProcessRequestComplete = OnQueryMessageContentsRequestComplete;
		`Log(`Location@"URL: " $ URL);
		// Call WebRequest
		if(!QueryMessageContentsRequest.ProcessRequest())
		{
			`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
		}
	}
}

/**
 * Called once the request/response has completed. Used to process the returned data and notify any
 * registered delegate
 * 
 * @param OriginalRequest the request object that was used
 * @param HttpResponse the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
private function OnQueryMessageContentsRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local array<byte> MessageContents;
	local string MessageId;
	local McpMessage Message;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;

	// Both HttpResponse and OriginalRequest need to be present
	if (HttpResponse != none && OriginalRequest != none)
	{
		MessageId = OriginalRequest.GetURLParameter("messageId");

		ResponseCode = HttpResponse.GetResponseCode();

		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful && Len(MessageId) > 0 )
		{
			HttpResponse.GetContent(MessageContents);
			`log("MessageId:" $ MessageId $" Compressed Message Contents Length:" $ MessageContents.Length);
			if (MessageContents.Length > 0)
			{
				GetMessageById(MessageId, Message);

				if( Message.MessageCompressionType == MMCT_NONE)
				{
					CacheMessageContents(MessageContents, MessageId);					
					OnQueryMessageContentsComplete(MessageId, bWasSuccessful, HttpResponse.GetContentAsString());	
				}
				else
				{
					if( !StartAsyncUncompression(MessageId, Message.MessageCompressionType, MessageContents )) 
					{
						OnQueryMessageContentsComplete(MessageId, false, "Could not Start AsyncDecompression");
					}
					// this will call OnQueryMessageContentsComplete
				}
			}
			else
			{
				OnQueryMessageContentsComplete(MessageId, false, "Query did not return any content in it's response.");
			}
		}
		else
		{
			OnQueryMessageContentsComplete(MessageId, false, HttpResponse.GetContentAsString());
		}
	}
	else
	{
		OnQueryMessageContentsComplete(MessageId, false, "There was No HttpResponse or Request");
	}
}

/**
 * Returns a copy of the Message Contents that belong to the specified MessageId
 * Called after QueryMessageContents. 
 * 
 * @param MessageId 
 * @param MessageContents 
 */
function bool GetMessageContents(String MessageId, out array<byte> MessageContents)
{
	local bool bWasSuccessful;
	local int MessageContentsIndex;

	MessageContentsIndex = MessageContentsList.Find('MessageId', MessageId);
	
	if (MessageContentsIndex != INDEX_NONE)
	{
		MessageContents = MessageContentsList[MessageContentsIndex].MessageContents;
		bWasSuccessful = true;
	}else
	{
		bWasSuccessful = false;
	}
	return bWasSuccessful;
}

/**
 *Store group in an object in memory instead of having to query the server again for it
 * 
 * @param Message to be placed in the cache
 */
function CacheMessage( McpMessage Message)
{
	local int AddAt;
	local int MessageIndex;	
	local int MessageListIndex;
	local McpMessageList UserMessageList;
	local bool bWasFound;

	//Find the user's cached message list in collection of user's message lists, MessageLists
	bWasFound = false;
	MessageListIndex = MessageLists.Find('ToUniqueUserId', Message.ToUniqueUserId);
	if(MessageListIndex != INDEX_NONE)
	{
		UserMessageList = MessageLists[MessageListIndex];

		// Search the array for any existing adding only when missing
		for (MessageIndex = 0; MessageIndex < UserMessageList.Messages.Length && !bWasFound; MessageIndex++)
		{
			bWasFound = Message.MessageId == UserMessageList.Messages[MessageIndex].MessageId;
		}
		// Add this one since it wasn't found
		if (!bWasFound)
		{
			AddAt = UserMessageList.Messages.Length;
			UserMessageList.Messages.Length = AddAt + 1;
			UserMessageList.Messages[AddAt] = Message;
			MessageLists[MessageListIndex] = UserMessageList;
		}

		`log(`Location $ " MessageId: " $ UserMessageList.Messages[AddAt].MessageId);

	}
	else
	{
		// Add User with this first returned group since it wasn't found
		AddAt = MessageLists.Length;
		MessageLists.Length = AddAt +1;
		MessageLists[AddAt].ToUniqueUserId = Message.ToUniqueUserId;
		MessageLists[AddAt].Messages[0]=Message;
	}
}

/**
 * Get Message By Id
 * 
 * @param MessageId
 * @param Message
 */
function bool GetMessageById(string MessageId, out McpMessage Message)
{
	local int MessageListsSize;
	local int MessageListsItr;
	local int MessageItr;

	MessageListsSize = MessageLists.Length;

	// Look at each MessageList (userId:array<McpMessage>) mapping 
	for(MessageListsItr = 0; MessageListsItr < MessageListsSize; MessageListsItr++)
	{
		// Look to see if the MessageID Inside each MessageList 
		for(MessageItr = 0; MessageItr < MessageLists[MessageListsItr].Messages.Length; MessageItr++)
		{
			if ( MessageLists[MessageListsItr].Messages[MessageItr].MessageId == MessageId )
			{
				// If it is in the message list set the out variable 
				Message = MessageLists[MessageListsItr].Messages[MessageItr];
				// Return here because messageIds are unique so continuing the loop is pointless
				return true;
			}
		}
	}
	return false;	
}

/**
 *Store group member in an object in memory instead of having to query the server again for it
 * 
 * @param MessageContents Message payload
 * @param MessageId to be placed in the cache
 */
function bool CacheMessageContents(const out array<byte> MessageContents, String MessageId)
{
	local int MessageContentsIndex;
    local bool bWasSuccessful;

	bWasSuccessful = false;

	// Have the variables been passed in properly
	if(MessageContents.Length > 0 && Len(MessageId) > 0)
	{
		MessageContentsIndex = MessageContentsList.Find('MessageId', MessageId);

		if (MessageContentsIndex != INDEX_NONE)
		{
			MessageContentsList[MessageContentsIndex].MessageContents = MessageContents;
			bWasSuccessful = true;
		}else
		{
			MessageContentsIndex = MessageContentsList.Length;
			MessageContentsList.Length = MessageContentsList.Length+1;
			MessageContentsList[MessageContentsIndex].MessageId = MessageId;
			MessageContentsList[MessageContentsIndex]. MessageContents = MessageContents; 

			bWasSuccessful = true;
		}
	}
	else
	{
		`Log(`Location@" Either the MessageContents or MessageId were not Specified.");
	}
	return bWasSuccessful;
}

cpptext
{
// FTickableObject interface

	/**
	 * Returns whether it is okay to tick this object. E.g. objects being loaded in the background shouldn't be ticked
	 * till they are finalized and unreachable objects cannot be ticked either.
	 *
	 * @return	TRUE if tickable, FALSE otherwise
	 */
	virtual UBOOL IsTickable() const
	{
		// We cannot tick objects that are unreachable or are in the process of being loaded in the background.
		return !HasAnyFlags( RF_Unreachable | RF_AsyncLoading );
	}

	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 *
	 * @return always TRUE as networking needs to be ticked even when paused
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	/**
	 * Needs to be overridden by child classes
	 *
	 * @param ignored
	 */
	virtual void Tick(FLOAT);
}