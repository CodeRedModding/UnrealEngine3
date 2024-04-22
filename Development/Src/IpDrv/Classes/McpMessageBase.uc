/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides the interface for McpMessages and the factory method
 * for creating the registered implementing object
 */
class McpMessageBase extends McpServiceBase
	native
	abstract 
	config(Engine);

/** The class name to use in the factory method to create our instance */
var config string McpMessageManagerClassName;

/** Compression types supported */
enum EMcpMessageCompressionType
{
	MMCT_NONE,
	MMCT_LZO,
	MMCT_ZLIB
};

/** Default Compression Type */
var config EMcpMessageCompressionType CompressionType;

/**
 *  Message
 */
struct native McpMessage
{
	/**
	 * Unique id to use as a key for this message
	 */
	var String MessageId;
	/**
	 * The user id to deliver this message to
	 */
	var String ToUniqueUserId;
	/**
	 * The user id this message is from
	 */
	var String FromUniqueUserId;
	/**
	 * The friendly name of the user sending the data
	 */
	var String FromFriendlyName;
	/**
	 * The application specific message type
	 */
	var String MessageType;
	/**
	 * The date until this message is no longer valid (should be deleted)
	 */
	var String ValidUntil;
	/**
	 * The compression type of this message so the client knows how to de-compress the payload
	 */
	var EMcpMessageCompressionType MessageCompressionType;
};

/**
 * List of Messages belonging to one user 
 */
struct native McpMessageList
{
	/** User that User the messages were Sent To */
	var string ToUniqueUserId;

	/** Collection of groups that the user owns OR belongs to */
	var array<McpMessage> Messages;
};

/**
 * Message Contents
 */
struct native McpMessageContents
{
	/**
	 * Message Id
	 */
	var string MessageId;
	
	/**
 	 * Payload holding the contents of the message
     */
	var array<byte> MessageContents;
};

/** Holds the MessageContents for each user in memory */
var array<McpMessageContents> MessageContentsList;

/** Holds the members for each user in memory */
var array<McpMessageList> MessageLists;

/**
 * Create Instance of an McpMessage
 * @return the object that implements this interface or none if missing or failed to create/load
 */
static final function McpMessageBase CreateInstance()
{
	local class<McpMessageBase> McpMessageBaseClass;
	local McpMessageBase NewInstance;

	McpMessageBaseClass = class<McpMessageBase>(DynamicLoadObject(default.McpMessageManagerClassName,class'Class'));
	if (McpMessageBaseClass != None)
	{
		NewInstance = new McpMessageBaseClass;
		NewInstance.Init();
	}

	return NewInstance;
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
	const out array<byte> MessageContents);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param Message the group id of the group that was created
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnCreateMessageComplete(McpMessage Message, bool bWasSuccessful, String Error);

/**
 * Deletes a Message by MessageId
 * 
 * @param MessageId Id of the group
 */
function DeleteMessage(String MessageId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param MessageId the group id of the group that was Deleted
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnDeleteMessageComplete(String MessageId, bool bWasSuccessful, String Error);

/**
 * Queries the backend for the Messages belonging to the supplied UserId
 * 
 * @param UniqueUserId the id of the owner of the groups to return
 */
function QueryMessages(String ToUniqueUserId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param UserId the user id of the groups that were queried
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQueryMessagesComplete(string UserId, bool bWasSuccessful, String Error);

/**
 * Returns the set of messages that sent to the specified UserId
 * Called after QueryMessages. 
 * 
 * @param ToUniqueUserId the user the messages were sent to
 * @param MessageList collection of messages
 */
function GetMessageList(string ToUniqueUserId, out McpMessageList MessageList);

/**
 * Queries the back-end for the Messages Contents belonging to the specified group
 * 
 * @param MessageId the id of the owner of the groups to return
 */
function QueryMessageContents(String MessageId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param MessageId the id of the group from which the members were queried
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQueryMessageContentsComplete(String MessageId, bool bWasSuccessful, String Error);

/**
 * Returns a copy of the Message Contents that belong to the specified MessageId
 * Called after QueryMessageContents. 
 * 
 * @param MessageId 
 * @param MessageContents 
 */
function bool GetMessageContents(String MessageId, out array<byte> MessageContents);

/**
 *Store group in an object in memory instead of having to query the server again for it
 * 
 * @param Message to be placed in the cache
 */
function CacheMessage( McpMessage Message);

/**
 *Store group member in an object in memory instead of having to query the server again for it
 * 
 * @param MessageContents Message payload
 * @param MessageId to be placed in the cache
 */
function bool CacheMessageContents(const out array<byte> MessageContents, String MessageId);
