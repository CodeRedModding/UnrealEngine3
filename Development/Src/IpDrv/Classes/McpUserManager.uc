/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Concrete implementation for mapping McpIds to external account ids
 */
class McpUserManager extends McpUserManagerBase;

/**
 * Holds the set of user statuses that were downloaded
 */
var array<McpUserStatus> UserStatuses;

/** The URL to use when making user registration requests to generate an id */
var config String RegisterUserMcpUrl;

/** The URL to use when making user registration requests via Facebook id/token */
var config String RegisterUserFacebookUrl;

/** The URL to use when making user query requests */
var config String QueryUserUrl;

/** The URL to use when making querying for multiple user statuses */
var config String QueryUsersUrl;

/** The URL to use when making user deletion requests */
var config String DeleteUserUrl;

/** The URL to use when making user Facebook authentication requests */
var config String FacebookAuthUrl;

/** The URL to use when making user authentication requests */
var config String McpAuthUrl;

/** Holds the state information for an outstanding user request */
struct UserRequest
{
	/** The MCP id that was returned by the backend */
	var string McpId;
	/** The request object for this request */
	var HttpRequestInterface Request;
};

/** The set of add mapping requests that are pending */
var array<HttpRequestInterface> RegisterUserRequests;

/** The set of query requests that are pending */
var array<HttpRequestInterface> QueryUsersRequests;

/** The set of delete requests that are pending */
var array<UserRequest> DeleteUserRequests;

/** The set of delete requests that are pending */
var array<HttpRequestInterface> AuthUserRequests;

/**
 * Creates a new user mapped to the UDID that is specified
 */
function RegisterUserGenerated()
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ RegisterUserMcpUrl $ GetAppAccessURL();

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("POST");
		Request.OnProcessRequestComplete = OnRegisterUserRequestComplete;
		// Store off the data for reporting later
		AddAt = RegisterUserRequests.Length;
		RegisterUserRequests.Length = AddAt + 1;
		RegisterUserRequests[AddAt] = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start RegisterUser web request for URL(" $ Url $ ")");
		}
		`Log("URL is " $ Url);
	}
}

/**
 * Maps a newly generated or existing Mcp id to the Facebook id/token requested.
 * Note: Facebook id authenticity is verified via the token
 * 
 * @param FacebookId user's FB id to generate Mcp id for
 * @param FacebookAuthToken FB auth token obtained by signing in to FB
 */
function RegisterUserFacebook(string FacebookId, string FacebookAuthToken)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ RegisterUserFacebookUrl $ GetAppAccessURL() $
			"&facebookId=" $ FacebookId $
			"&facebookToken=" $ FacebookAuthToken;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("POST");
		Request.OnProcessRequestComplete = OnRegisterUserRequestComplete;
		// Store off the data for reporting later
		AddAt = RegisterUserRequests.Length;
		RegisterUserRequests.Length = AddAt + 1;
		RegisterUserRequests[AddAt] = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start RegisterUserFacebook web request for URL(" $ Url $ ")");
		}
		`Log("URL is " $ Url);
	}
}

/**
 * Parses the user json
 * 
 * @param JsonPayload the json data to parse
 * 
 * @return the index of the user that was parsed
 */
protected function int ParseUser(String JsonPayload)
{
	local JsonObject ParsedJson;
	local int UserIndex;
	local string McpId;

	ParsedJson = class'JsonObject'.static.DecodeJson(JsonPayload);
	// If it doesn't have this field, this is bogus JSON
	if (ParsedJson.HasKey("unique_user_id"))
	{
		// Grab the McpId first, since that is our key
		McpId = ParsedJson.GetStringValue("unique_user_id");
		// See if we already have a user
		UserIndex = UserStatuses.Find('McpId', McpId);
		if (UserIndex == INDEX_NONE)
		{
			// Not stored yet, so add one
			UserIndex = UserStatuses.Length;
			UserStatuses.Length = UserIndex + 1;
			UserStatuses[UserIndex].McpId = McpId;
		}
		// These values are only there for the owner and not friends
		if (ParsedJson.HasKey("client_secret"))
		{
			UserStatuses[UserIndex].SecretKey = ParsedJson.GetStringValue("client_secret");
		}
		if (ParsedJson.HasKey("ticket"))
		{
			UserStatuses[UserIndex].Ticket = ParsedJson.GetStringValue("ticket");
		}
		if (ParsedJson.HasKey("udid"))
		{
			UserStatuses[UserIndex].UDID = ParsedJson.GetStringValue("udid");
		}
		// These are always there
		UserStatuses[UserIndex].RegisteredDate = ParsedJson.GetStringValue("registered_date");
		UserStatuses[UserIndex].LastActiveDate = ParsedJson.GetStringValue("last_active_date");
		UserStatuses[UserIndex].DaysInactive = ParsedJson.GetIntValue("days_inactive");
		UserStatuses[UserIndex].bIsBanned = ParsedJson.GetBoolValue("is_banned");
	}
	else
	{
		UserIndex = INDEX_NONE;
	}
	// Tell the caller which record was updated
	return UserIndex;
}

/**
 * Called once the request/response has completed. Used to process the register user result and notify any
 * registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnRegisterUserRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local int UserIndex;
	local string ResponseString;
	local string McpId;

	// Search for the corresponding entry in the array
	Index = RegisterUserRequests.Find(Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = 500;
		if (Response != none)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// The response string is the JSON payload for the user
		ResponseString = Response.GetContentAsString();
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == 200;
		if (bWasSuccessful)
		{
			// Parse the JSON payload into a user
			UserIndex = ParseUser(ResponseString);
			if (UserIndex == INDEX_NONE)
			{
				bWasSuccessful = false;
			}
		}
		McpId = bWasSuccessful ? UserStatuses[UserIndex].McpId : "";
		// Notify anyone waiting on this
		OnRegisterUserComplete(McpId,
			bWasSuccessful,
			ResponseString);
		`Log("Register user McpId(" $ McpId $ ") was successful " $
			bWasSuccessful $ " with ResponseCode(" $ ResponseCode $ ")");
		RegisterUserRequests.Remove(Index,1);
	}
}

/**
 * Authenticates a user is who they claim themselves to be using Facebook as the authority
 * 
 * @param FacebookId the Facebook user that is being authenticated
 * @param FacebookToken the secret that authenticates the user
 * @param UDID the device id the user is logging in from
 */
function AuthenticateUserFacebook(string FacebookId, string FacebookToken, string UDID)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ FacebookAuthUrl $ GetAppAccessURL() $
			"&facebookId=" $ FacebookId $
			"&facebookToken=" $ FacebookToken $
			"&udid=" $ UDID;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("POST");
		Request.OnProcessRequestComplete = OnAuthenticateUserRequestComplete;
		// Store off the data for reporting later
		AddAt = AuthUserRequests.Length;
		AuthUserRequests.Length = AddAt + 1;
		AuthUserRequests[AddAt] = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start AuthenticateUserFacebook web request for URL(" $ Url $ ")");
		}
	}
}

/**
 * Authenticates a user is the same as the one that was registered
 * 
 * @param McpId the user that is being authenticated
 * @param ClientSecret the secret that authenticates the user
 * @param UDID the device id the user is logging in from
 */
function AuthenticateUserMCP(string McpId, string ClientSecret, string UDID)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ McpAuthUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ McpId $
			"&clientSecret=" $ ClientSecret $
			"&udid=" $ UDID;

        `log("Started Authenticate, url: "$Url);

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("POST");
		Request.OnProcessRequestComplete = OnAuthenticateUserRequestComplete;
		// Store off the data for reporting later
		AddAt = AuthUserRequests.Length;
		AuthUserRequests.Length = AddAt + 1;
		AuthUserRequests[AddAt] = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start AuthenticateUserMCP web request for URL(" $ Url $ ")");
		}
	}
}

/**
 * Called once the request/response has completed. Used to process the returned data and notify any
 * registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnAuthenticateUserRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local int UserIndex;
	local string ResponseString;
	local string McpId;
	local string Ticket;

	// Search for the corresponding entry in the array
	Index = AuthUserRequests.Find(Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = 500;
		if (Response != none)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// The response string is the JSON payload for the user
		ResponseString = Response.GetContentAsString();
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == 200;
		if (bWasSuccessful)
		{
			// Parse the JSON payload into a user
			UserIndex = ParseUser(ResponseString);
			if (UserIndex == INDEX_NONE)
			{
				bWasSuccessful = false;
			}
		}
		// If it was successful, grab the id and ticket for notifying the caller
		if (bWasSuccessful)
		{
			McpId = UserStatuses[UserIndex].McpId;
			Ticket = UserStatuses[UserIndex].Ticket;
		}
		// Notify anyone waiting on this
		OnAuthenticateUserComplete(McpId, Ticket, bWasSuccessful, ResponseString);
		`Log("Authenticate user was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ") and Ticket ("$Ticket$")");
		AuthUserRequests.Remove(Index,1);
	}
}

/**
 * Queries the backend for the status of a users
 * 
 * @param McpId the id of the user to get the status for
 * @param bShouldUpdateLastActive if true, the act of getting the status updates the active time stamp
 */
function QueryUser(string McpId, optional bool bShouldUpdateLastActive)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none && McpId != "")
	{
		Url = GetBaseURL() $ QueryUserUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ McpId $
			"&updateLastActive=" $ bShouldUpdateLastActive;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("GET");
		Request.OnProcessRequestComplete = OnQueryUserRequestComplete;
		// Store off the data for reporting later
		AddAt = QueryUsersRequests.Length;
		QueryUsersRequests.Length = AddAt + 1;
		QueryUsersRequests[AddAt] = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start QueryUser web request for URL(" $ Url $ ")");
		}
	}
}

/**
 * Called once the request/response has completed. Used to process the returned data and notify any
 * registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnQueryUserRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local int UserIndex;
	local string ResponseString;

	// Search for the corresponding entry in the array
	Index = QueryUsersRequests.Find(Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = 500;
		if (Response != none)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// The response string is the JSON payload for the user
		ResponseString = Response.GetContentAsString();
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == 200;
		if (bWasSuccessful)
		{
			// Parse the JSON payload into a user
			UserIndex = ParseUser(ResponseString);
			if (UserIndex == INDEX_NONE)
			{
				bWasSuccessful = false;
			}
		}
		// Notify anyone waiting on this
		OnQueryUsersComplete(bWasSuccessful, ResponseString);
		`Log("Query user was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		QueryUsersRequests.Remove(Index,1);
	}
}

/**
 * Queries the backend for the status of a list of users
 * 
 * @param McpIds the set of ids to get read the status of
 */
function QueryUsers(const out array<String> McpIds)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;
	local string JsonPayload;
	local int Index;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ QueryUsersUrl $ GetAppAccessURL();

		// Make a json string from our list of ids
		JsonPayload = "[ ";
		for (Index = 0; Index < McpIds.Length; Index++)
		{
			JsonPayload $= "\"" $ McpIds[Index] $ "\"";
			// Only add the string if this isn't the last item
			if (Index + 1 < McpIds.Length)
			{
				JsonPayload $= ",";
			}
		}
		JsonPayload $= " ]";

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetContentAsString(JsonPayload);
		Request.SetVerb("POST");
		Request.SetHeader("Content-Type","multipart/form-data");
		Request.OnProcessRequestComplete = OnQueryUsersRequestComplete;
		// Store off the data for reporting later
		AddAt = QueryUsersRequests.Length;
		QueryUsersRequests.Length = AddAt + 1;
		QueryUsersRequests[AddAt] = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start QueryUsers web request for URL(" $ Url $ ")");
		}
	}
}

/**
 * Parses the json which contains an array of user data
 * 
 * @param JsonPayload the json data to parse
 * 
 * @return the index of the user that was parsed
 */
protected function ParseUsers(String JsonPayload)
{
	local JsonObject ParsedJson;
	local int JsonIndex;
	local int UserIndex;
	local string McpId;

	ParsedJson = class'JsonObject'.static.DecodeJson(JsonPayload);
	// Parse each user, adding them if needed
	for (JsonIndex = 0; JsonIndex < ParsedJson.ObjectArray.Length; JsonIndex++)
	{
		// If it doesn't have this field, this is bogus JSON
		if (ParsedJson.HasKey("unique_user_id"))
		{
			// Grab the McpId first, since that is our key
			McpId = ParsedJson.GetStringValue("unique_user_id");
			// See if we already have a user
			UserIndex = UserStatuses.Find('McpId', McpId);
			if (UserIndex == INDEX_NONE)
			{
				// Not stored yet, so add one
				UserIndex = UserStatuses.Length;
				UserStatuses.Length = UserIndex + 1;
				UserStatuses[UserIndex].McpId = McpId;
			}
			// These values are only there for the owner and not friends
			if (ParsedJson.HasKey("client_secret"))
			{
				UserStatuses[UserIndex].SecretKey = ParsedJson.GetStringValue("client_secret");
			}
			if (ParsedJson.HasKey("ticket"))
			{
				UserStatuses[UserIndex].Ticket = ParsedJson.GetStringValue("ticket");
			}
			if (ParsedJson.HasKey("udid"))
			{
				UserStatuses[UserIndex].UDID = ParsedJson.GetStringValue("udid");
			}
			// These are always there
			UserStatuses[UserIndex].RegisteredDate = ParsedJson.GetStringValue("registered_date");
			UserStatuses[UserIndex].LastActiveDate = ParsedJson.GetStringValue("last_active_date");
			UserStatuses[UserIndex].DaysInactive = ParsedJson.GetIntValue("days_inactive");
			UserStatuses[UserIndex].bIsBanned = ParsedJson.GetBoolValue("is_banned");
		}
	}
}

/**
 * Called once the request/response has completed. Used to process the returned data and notify any
 * registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnQueryUsersRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local string ResponseString;

	// Search for the corresponding entry in the array
	Index = QueryUsersRequests.Find(Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = 500;
		if (Response != none)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// The response string is the JSON payload for the user
		ResponseString = Response.GetContentAsString();
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == 200;
		if (bWasSuccessful)
		{
			// Parse the JSON payload into a user
			ParseUsers(ResponseString);
		}
		// Notify anyone waiting on this
		OnQueryUsersComplete(bWasSuccessful, ResponseString);
		`Log("Query users was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		QueryUsersRequests.Remove(Index,1);
	}
}

/**
 * Returns the set of user statuses queried so far
 * 
 * @param Users the out array that gets the copied data
 */
function GetUsers(out array<McpUserStatus> Users)
{
	Users = UserStatuses;
}

/**
 * Gets the user status entry for a single user
 *
 * @param McpId the id of the user that we want to find
 * @param User the out result to copy user data
 *
 * @return true if user was found
 */
function bool GetUser(string McpId, out McpUserStatus User)
{
	local int UserIndex;

	UserIndex = UserStatuses.Find('McpId', McpId);
	if (UserIndex != INDEX_NONE)
	{
		User = UserStatuses[UserIndex];
		return true;
	}
	return false;
}

/**
 * Deletes all data for a user
 * 
 * @param McpId the user that is being expunged from the system
 */
function DeleteUser(string McpId)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ DeleteUserUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ McpId;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("DELETE");
		Request.OnProcessRequestComplete = OnDeleteUserRequestComplete;
		// Store off the data for reporting later
		AddAt = DeleteUserRequests.Length;
		DeleteUserRequests.Length = AddAt + 1;
		DeleteUserRequests[AddAt].McpId = McpId;
		DeleteUserRequests[AddAt].Request = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start DeleteUser web request for URL(" $ Url $ ")");
		}
		`Log("URL is " $ Url);
	}
}

/**
 * Called once the delete request completes
 *
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnDeleteUserRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int UserIndex;
	local int ResponseCode;

	// Search for the corresponding entry in the array
	Index = DeleteUserRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = 500;
		if (Response != none)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == 200;
		if (bWasSuccessful)
		{
			// Delete this user from our list
			UserIndex = UserStatuses.Find('McpId', DeleteUserRequests[Index].McpId);
			if (UserIndex != INDEX_NONE)
			{
				UserStatuses.Remove(UserIndex, 1);
			}
		}
		// Notify anyone waiting on this
		OnDeleteUserComplete(bWasSuccessful,
			Response.GetContentAsString());
		`Log("Delete user for URL(" $ Request.GetURL() $ ") successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		DeleteUserRequests.Remove(Index,1);
	}
}
