/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Concrete implementation for mapping McpIds to external account ids
 */
class McpIdMappingManager extends McpIdMappingBase;

/**
 * Holds the set of mapped accounts that we've downloaded
 */
var array<McpIdMapping> AccountMappings;

/** The URL to use when making add mapping requests */
var config String AddMappingUrl;

/** The URL to use when making query mapping requests */
var config String QueryMappingUrl;

/** Holds the state information for an outstanding add mapping request */
struct AddMappingRequest
{
	/** The McpId the add is happening to */
	var string McpId;
	/** The external account id that is being mapped to a MCP id */
	var string ExternalId;
	/** The account type that is being mapped */
	var string ExternalType;
	/** The request object for this request */
	var HttpRequestInterface Request;
};

/** The set of add mapping requests that are pending */
var array<AddMappingRequest> AddMappingRequests;

/** Holds the state information for an outstanding query mapping request */
struct QueryMappingRequest
{
	/** The account type that is being mapped */
	var string ExternalType;
	/** The request object for this request */
	var HttpRequestInterface Request;
};

/** The set of query mapping requests that are pending */
var array<QueryMappingRequest> QueryMappingRequests;

/**
 * Adds an external account mapping. Sends this request to MCP to be processed
 *
 * @param McpId the account to add the mapping to
 * @param ExternalId the external account that is being mapped to this account
 * @param ExternalType the type of account for disambiguation
 */
function AddMapping(String McpId, String ExternalId, String ExternalType)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ AddMappingUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ McpId $
			"&externalAccountId=" $ ExternalId $
			"&externalAccountType=" $ ExternalType;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("POST");
		Request.OnProcessRequestComplete = OnAddMappingRequestComplete;
		// Store off the data for reporting later
		AddAt = AddMappingRequests.Length;
		AddMappingRequests.Length = AddAt + 1;
		AddMappingRequests[AddAt].McpId = McpId;
		AddMappingRequests[AddAt].ExternalId = ExternalId;
		AddMappingRequests[AddAt].ExternalType = ExternalType;
		AddMappingRequests[AddAt].Request = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start AddMapping web request for URL(" $ Url $ ")");
		}
		`Log("URL is " $ Url);
	}
}

/**
 * Called once the request/response has completed. Used to process the add mapping result and notify any
 * registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnAddMappingRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int AddAt;
	local int ResponseCode;

	// Search for the corresponding entry in the array
	Index = AddMappingRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = 500;
		if (Response != none)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == 200;
		`Log("Account mapping McpId(" $ AddMappingRequests[Index].McpId $ "), ExternalId(" $
			AddMappingRequests[Index].ExternalId $ "), ExternalType(" $
			AddMappingRequests[Index].ExternalType $ ") was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		// Add this item to our list
		if (bWasSuccessful)
		{
			AddAt = AccountMappings.Length;
			AccountMappings.Length = AddAt + 1;
			AccountMappings[AddAt].McpId = AddMappingRequests[Index].McpId;
			AccountMappings[AddAt].ExternalId = AddMappingRequests[Index].ExternalId;
			AccountMappings[AddAt].ExternalType = AddMappingRequests[Index].ExternalType;
		}
		// Notify anyone waiting on this
		OnAddMappingComplete(AddMappingRequests[Index].McpId,
			AddMappingRequests[Index].ExternalId,
			AddMappingRequests[Index].ExternalType,
			bWasSuccessful,
			Response.GetContentAsString());
		// Done with the pending request
		AddMappingRequests.Remove(Index,1);
	}
}

/**
 * Queries the backend for the McpIds of the list of external ids of a specific type
 * 
 * @param ExternalIds the set of ids to get McpIds for
 * @param ExternalType the type of account that is being mapped to McpIds
 */
function QueryMappings(const out array<String> ExternalIds, String ExternalType)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;
	local string JsonPayload;
	local int Index;
	local bool bFirst;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ QueryMappingUrl $ GetAppAccessURL() $
			"&externalAccountType=" $ ExternalType;

		// Make a json string from our list of ids
		JsonPayload = "[ ";
		bFirst = true;
		for (Index = 0; Index < ExternalIds.Length; Index++)
		{
			if (Len(ExternalIds[Index]) > 0)
			{
				if (!bFirst)
					JsonPayload $= ",";
				bFirst = false;
				JsonPayload $= "\"" $ ExternalIds[Index] $ "\"";
			}
		}
		JsonPayload $= " ]";

		`log("QueryMappings.JsonPayload:\n" $ JsonPayload);
		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetContentAsString(JsonPayload);
		Request.SetVerb("POST");
		Request.SetHeader("Content-Type","multipart/form-data");
		Request.OnProcessRequestComplete = OnQueryMappingsRequestComplete;

		// Store off the data for reporting later
		AddAt = QueryMappingRequests.Length;
		QueryMappingRequests.Length = AddAt + 1;
		QueryMappingRequests[AddAt].ExternalType = ExternalType;
		QueryMappingRequests[AddAt].Request = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start QueryMappings web request for URL(" $ Url $ ")");
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
function OnQueryMappingsRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int AddAt;
	local int ResponseCode;
	local string JsonString;
	local JsonObject ParsedJson;
	local int JsonIndex;
	local int AccountIndex;
	local bool bWasFound;
	local string McpId;
	local string ExternalId;
	local string ExternalType;

	// Search for the corresponding entry in the array
	Index = QueryMappingRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = 500;
		if (Response != none)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == 200;
		`Log("Account mapping query for ExternalType(" $
			QueryMappingRequests[Index].ExternalType $ ") was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		// Add this item to our list
		if (bWasSuccessful)
		{
			JsonString = Response.GetContentAsString();
			if (JsonString != "")
			{
				`Log("JSON for account query = \r\n" $ JsonString);
// @todo joeg - Replace with Wes' ImportJson() once it's implemented
				// Parse the json
				ParsedJson = class'JsonObject'.static.DecodeJson(JsonString);
				// Add each mapping in the json packet if missing
				for (JsonIndex = 0; JsonIndex < ParsedJson.ObjectArray.Length; JsonIndex++)
				{
					McpId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("unique_user_id");
					ExternalId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("external_account_id");
					ExternalType = ParsedJson.ObjectArray[JsonIndex].GetStringValue("external_account_type");
					bWasFound = false;
					// Search the array for any existing adding only when missing
					for (AccountIndex = 0; AccountIndex < AccountMappings.Length && !bWasFound; AccountIndex++)
					{
						bWasFound = McpId == AccountMappings[AccountIndex].McpId &&
							ExternalId == AccountMappings[AccountIndex].ExternalId &&
							ExternalType == AccountMappings[AccountIndex].ExternalType;
					}
					// Add this one since it wasn't found
					if (!bWasFound)
					{
						AddAt = AccountMappings.Length;
						AccountMappings.Length = AddAt + 1;
						AccountMappings[AddAt].McpId = McpId;
						AccountMappings[AddAt].ExternalId = ExternalId;
						AccountMappings[AddAt].ExternalType = ExternalType;
					}
				}
			}
		}
		// Notify anyone waiting on this
		OnQueryMappingsComplete(QueryMappingRequests[Index].ExternalType,
			bWasSuccessful,
			Response.GetContentAsString());
		// Done with the pending request
		QueryMappingRequests.Remove(Index,1);
	}
}

/**
 * Returns the set of id mappings that match the requested account type
 * 
 * @param ExternalType the account type that we want the mappings for
 * @param IdMappings the out array that gets the copied data
 */
function GetIdMappings(String ExternalType, out array<McpIdMapping> IdMappings)
{
	local int Index;
	local int AddAt;

	IdMappings.Length = 0;

	// Search through for all mappings that match the desired type
	for (Index = 0; Index < AccountMappings.Length; Index++)
	{
		if (AccountMappings[Index].ExternalType == ExternalType)
		{
			AddAt = IdMappings.Length;
			IdMappings.Length = AddAt + 1;
			IdMappings[AddAt] = AccountMappings[Index];
		}
	}
}
