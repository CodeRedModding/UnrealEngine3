/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides the interface for registering and querying users
 */
class McpUserManagerBase extends McpServiceBase
	abstract
	config(Engine);

/** The class name to use in the factory method to create our instance */
var config String McpUserManagerClassName;

/**
 * Holds the status information for a MCP user
 */
struct McpUserStatus
{
	/** The McpId of the user */
	var String McpId;
	/** The secret key (private field, owner only) */
	var String SecretKey;
	/** The auth ticket for this user (private field, owner only) */
	var String Ticket;
	/** The device id this user was registered on (private field, owner only) */
	var string UDID;
	/** The date the user was registered on */
	var string RegisteredDate;
	/** The last activity date for this user */
	var string LastActiveDate;
	/** The number of days inactive */
	var int DaysInactive;
	/** Whether this user has been banned from playing this game */
	var bool bIsBanned;
};

/**
 * @return the object that implements this interface or none if missing or failed to create/load
 */
final static function McpUserManagerBase CreateInstance()
{
	local class<McpUserManagerBase> McpUserManagerBaseClass;
	local McpUserManagerBase NewInstance;

	McpUserManagerBaseClass = class<McpUserManagerBase>(DynamicLoadObject(default.McpUserManagerClassName,class'Class'));
	// If the class was loaded successfully, create a new instance of it
	if (McpUserManagerBaseClass != None)
	{
		NewInstance = new McpUserManagerBaseClass;
		NewInstance.Init();
	}

	return NewInstance;
}

/**
 * Creates a new user
 */
function RegisterUserGenerated();

/**
 * Maps a newly generated or existing Mcp id to the Facebook id/token requested.
 * Note: Facebook id authenticity is verified via the token
 * 
 * @param FacebookId user's FB id to generate Mcp id for
 * @param UDID the UDID for the device
 * @param FacebookAuthToken FB auth token obtained by signing in to FB
 */
function RegisterUserFacebook(string FacebookId, string FacebookAuthToken);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param McpId the id of the user that was just created
 * @param bWasSuccessful whether the mapping succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnRegisterUserComplete(string McpId, bool bWasSuccessful, String Error);

/**
 * Authenticates a user is who they claim themselves to be using Facebook as the authority
 * 
 * @param FacebookId the Facebook user that is being authenticated
 * @param FacebookToken the secret that authenticates the user
 * @param UDID the device id the user is logging in from
 */
function AuthenticateUserFacebook(string FacebookId, string FacebookToken, string UDID);

/**
 * Authenticates a user is the same as the one that was registered
 * 
 * @param McpId the user that is being authenticated
 * @param ClientSecret the secret that authenticates the user
 * @param UDID the device id the user is logging in from
 */
function AuthenticateUserMcp(string McpId, string ClientSecret, string UDID);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param McpId the id of the user that was just authenticated
 * @param Token the security token to use for subsequent user based calls
 * @param bWasSuccessful whether the mapping succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnAuthenticateUserComplete(string McpId, string Token, bool bWasSuccessful, String Error);

/**
 * Queries the backend for the status of a users
 * 
 * @param McpId the id of the user to get the status for
 * @param bShouldUpdateLastActive if true, the act of getting the status updates the active time stamp
 */
function QueryUser(string McpId, optional bool bShouldUpdateLastActive);

/**
 * Queries the backend for the status of a list of users
 * 
 * @param McpIds the set of ids to get read the status of
 */
function QueryUsers(const out array<String> McpIds);

/**
 * Called once the query results come back from the server to indicate success/failure of the request
 *
 * @param bWasSuccessful whether the query succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQueryUsersComplete(bool bWasSuccessful, String Error);

/**
 * Returns the set of user statuses queried so far
 * 
 * @param Users the out array that gets the copied data
 */
function GetUsers(out array<McpUserStatus> Users);

/**
 * Gets the user status entry for a single user
 *
 * @param McpId the id of the user that we want to find
 * @param User the out result to copy user data
 *
 * @return true if user was found
 */
function bool GetUser(string McpId, out McpUserStatus User);

/**
 * Deletes all data for a user
 * 
 * @param McpId the user that is being expunged from the system
 */
function DeleteUser(string McpId);

/**
 * Called once the delete request completes
 *
 * @param bWasSuccessful whether the request succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnDeleteUserComplete(bool bWasSuccessful, String Error);
