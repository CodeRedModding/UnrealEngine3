/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Concrete implementation for managing groups through the groups web API
 */
class McpGroupsManager extends McpGroupsBase
	config(Engine);

`include(Engine\Classes\HttpStatusCodes.uci)

/** The URL of CreateGroup function on the server */
var config string CreateGroupUrl;

/** The URL of DeleteGroup function on the server */
var config string DeleteGroupUrl;

/** The URL of ListGroups function on the server */
var config string QueryGroupsUrl;

/** The URL of ListGroupMembers function on the server */
var config string QueryGroupMembersUrl;

/** The URL of AddGroupMembers function on the server */
var config string AddGroupMembersUrl;

/** The URL of RemoveGroupMembers function on the server */
var config string RemoveGroupMembersUrl;

/** The URL of DeleteAllGroups function on the server */
var config string DeleteAllGroupsUrl;

/** The URL of AcceptGroupInvite function on the server */
var config string AcceptGroupInviteUrl;

/** The URL of RejectGroupInvite function on the server */
var config string RejectGroupInviteUrl;

/**
 * Creates the URL and sends the request to create a group.
 *  - This updates the group on the server QueryGroups will need to be 
 *  - run again before GetGroups will reflect the this new group.
 * @param UniqueUserId the UserId that will own the group
 * @param GroupName The name the group will be created with
 */
function CreateGroup(String UniqueUserId, String GroupName)
{
	local string Url;
	local HttpRequestInterface CreateGroupRequest;
	local McpGroup FailedGroup;
	
	//Ensure that UniqueUserId and GroupName both have values
	if(Len(UniqueUserId) > 0 && Len(GroupName) > 0)
	{
		// Create HttpRequest
		CreateGroupRequest = class'HttpFactory'.static.CreateRequest();

		if(CreateGroupRequest != none)
		{
			// Fill url out using parameters
			Url = GetBaseURL() $ CreateGroupUrl $ GetAppAccessURL() $
				"&uniqueUserId=" $ UniqueUserId $ 
				"&groupName=" $ GroupName $ 
				"&accessLevel=" $ "OWNER";

			// Build our web request with the above URL
			CreateGroupRequest.SetURL(URL);
			CreateGroupRequest.SetVerb("POST");
		
			CreateGroupRequest.OnProcessRequestComplete = OnCreateGroupRequestComplete;

			// Call Web Request
			if (!CreateGroupRequest.ProcessRequest())
			{
				`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
			}
			`Log(`Location $ " URL is " $ Url);
		}
		else
		{
			OnCreateGroupComplete(FailedGroup, false, "HttpRequest was not be created");
		}
	}
	else
	{
		OnCreateGroupComplete(FailedGroup, false, "UserId or GroupName wasn't specified");
	}
}


/**
 * Called once the request/response has completed. 
 * Used to return any errors and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnCreateGroupRequestComplete(HttpRequestInterface CreateGroupRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	
	local int ResponseCode;
	local string Content; 
	local McpGroup CreatedGroup;
	local String JsonString;
	local JsonObject ParsedJson;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	if (HttpResponse != none && CreateGroupRequest != none)
	{
		ResponseCode = HttpResponse.GetResponseCode();
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_CREATED;

		Content = HttpResponse.GetContentAsString();

		//Set default parameters in case create was not successful
		CreatedGroup.OwnerId = CreateGroupRequest.GetURLParameter("uniqueUserId");
		CreatedGroup.GroupName = CreateGroupRequest.GetURLParameter("groupName");
		JsonString = HttpResponse.GetContentAsString();
		if (JsonString != "" && bWasSuccessful)
		{
			// Parse the json
			ParsedJson = class'JsonObject'.static.DecodeJson(JsonString);
			CreatedGroup.GroupId = ParsedJson.GetStringValue("group_id");
			CreatedGroup.OwnerId = ParsedJson.GetStringValue("unique_user_id");
			CreatedGroup.GroupName = ParsedJson.GetStringValue("group_name");
			CreatedGroup.AccessLevel = EMcpGroupAccessLevel(ParsedJson.GetIntValue("access_level") );
		}
		else
		{
			`log(`Location@" CreateGroup query did not return a group.");
		}
	}
	OnCreateGroupComplete(CreatedGroup, bWasSuccessful, Content);
}

/**
 * Deletes a Group by GroupId
 * 
 * @param UniqueUserId UserId of the owner of the Group
 * @param GroupId Id of the group
 */
function DeleteGroup(String UniqueUserId, String GroupId)
{
	local string Url;
	local HttpRequestInterface  DeleteGroupRequest;

		//Ensure that UniqueUserId and GroupId both have values
	if(Len(UniqueUserId) > 0 && Len(GroupId) > 0)
	{
		// Delete HttpRequest
		DeleteGroupRequest = class'HttpFactory'.static.CreateRequest();

		if(DeleteGroupRequest != none)
		{
			// Fill url out using parameters
			Url = GetBaseURL() $ DeleteGroupUrl $ GetAppAccessURL() $
				"&uniqueUserId=" $ UniqueUserId $ 
				"&groupId=" $ GroupId;

			// Build our web request with the above URL
			DeleteGroupRequest.SetVerb("DELETE");
			DeleteGroupRequest.SetURL(URL);
			DeleteGroupRequest.OnProcessRequestComplete = OnDeleteGroupRequestComplete;

			// call WebRequest
			if(!DeleteGroupRequest.ProcessRequest())
			{
				`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
			}
			`Log(`Location $ "URL is " $ Url);
		}	
		else
		{
			OnDeleteGroupComplete(GroupId, false, "HttpRequest could not be completed");
		}
	}
	else
	{
		OnDeleteGroupComplete(GroupId, false, "UniqueUserId and/or GroupId was not specified");
	}
}

/**
 * Called once the request/response has completed. 
 * Used to process the response and notify any
 * registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnDeleteGroupRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Content; 
	local String GroupId;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	if (HttpResponse != none)
	{
		ResponseCode = HttpResponse.GetResponseCode();
		GroupId = HttpResponse.GetURLParameter("GroupId");
		ResponseCode = HttpResponse.GetResponseCode();
		Content = HttpResponse.GetContentAsString();
	}

	bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;

	OnDeleteGroupComplete(GroupId, bWasSuccessful, Content);
}


/**
 * Queries the backend for the Groups belonging to the supplied UserId
 * 
 * @param UniqueUserId the id of the owner of the groups to return
 */
//Change to be not just owner but all groups userId belongs to
function QueryGroups(String RequesterId)
{
	// Cache one group result set per user, instead of only having one array<McpGroup> that gets over
	local string Url;
	local HttpRequestInterface  QueryGroupsRequest;

	//Ensure that UniqueUserId and GroupName both have values
	if(Len(RequesterId) > 0 )
	{
		// List HttpRequest
		QueryGroupsRequest = class'HttpFactory'.static.CreateRequest();
		if (QueryGroupsRequest != none)
		{
			// Fill it out using parameters
			// The server takes one of two parameters
			//  - uniqueUserId will return only groups owned by the user
			//  - memberUniqueUserId will return all groups that have the user as a member (including groups owned by that user)
			Url = GetBaseURL() $ QueryGroupsUrl $ GetAppAccessURL() $
				"&memberUniqueUserId=" $ RequesterId;
		
			// Build our web request with the above URL
			QueryGroupsRequest.SetURL(URL);
			QueryGroupsRequest.SetVerb("GET");
			QueryGroupsRequest.OnProcessRequestComplete = OnQueryGroupsRequestComplete;

			// Call Web Request
			if(!QueryGroupsRequest.ProcessRequest())
			{
				`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
			}
		}
		else
		{
			OnQueryGroupsComplete(RequesterId, false, "Http Request could not be created");
		}
	}
	else
	{
		OnQueryGroupsComplete(RequesterId, false, "RequesterId was not specified");
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
delegate OnQueryGroupsRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Error;
	local McpGroup Group;
	local string JsonString;
	local JsonObject ParsedJson;
	local int JsonIndex;
	local string RequesterId;
	
	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	// Both HttpResponse and OriginalRequest need to be present
	if (HttpResponse != none && OriginalRequest != none)
	{
		RequesterId = OriginalRequest.GetURLParameter("memberUniqueUserId");
		ResponseCode = HttpResponse.GetResponseCode();

		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			JsonString = HttpResponse.GetContentAsString();
			if (JsonString != "")
			{
				// Parse the json
				ParsedJson = class'JsonObject'.static.DecodeJson(JsonString);
				// Add each mapping in the json packet if missing
				for (JsonIndex = 0; JsonIndex < ParsedJson.ObjectArray.Length; JsonIndex++)
				{
					Group.OwnerId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("unique_user_id");
					Group.GroupId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("group_id");
					Group.GroupName = ParsedJson.ObjectArray[JsonIndex].GetStringValue("group_name");
					Group.AccessLevel = EMcpGroupAccessLevel(ParsedJson.ObjectArray[JsonIndex].GetIntValue("access_level") );

					CacheGroup(RequesterId, Group);
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
	OnQueryGroupsComplete(RequesterId, bWasSuccessful, Error);
}

/**
 * Returns the set of groups that related to the specified UserId
 * Called after QueryGroups, which fills/updates the GroupLists variable with the response from the server. 
 * To check for ownership compare the RequesterId to the OwnerId of the group.
 * 
 * @param UserId the request object that was used
 * @param GroupList the response object that was generated
 */
function GetGroupList(string UserId, out McpGroupList GroupList)
{
	local int GroupListIndex;

	// Make sure that userId is valid
	if( Len(UserId) > 0 )
	{
		// Check Cache Variable GroupLists
		GroupListIndex = GroupLists.Find('RequesterId', UserId);
		if(GroupListIndex != INDEX_NONE)
		{
			GroupList = GroupLists[GroupListIndex];
		}
		else
		{
			`Log(`Location $ " Requester Id not found or GroupLists is empty. Using UserId: " $ UserId);
		}
	}
	else
	{
		`Log(`Location $ "UserId not specified");
	}
}

/**
 * Queries the backend for the Groups Members belonging to the specified group
 *
 *	After the Query is returned from the server the data is stored in a local variable GroupLists
 *  In order to fully fill this variable for a given user you need to run QueryGroups AND QueryGroupMembers.
 *  Since there are potentially many more GroupMembers than Groups it saves bandwidth to only Query what you need
 * 
 * @param UniqueUserId the id of the owner of the group being queried
 * @param GroupId the id of the owner of the groups to return
 */
function QueryGroupMembers(String UniqueUserId, String GroupId)
{
	// Cache one group result set per user, instead of only having one array<McpGroup> that gets over
	local string Url;
	local HttpRequestInterface QueryGroupMembersRequest;


	// Make sure that UniqueUserId and GroupId have been given
	if( Len(UniqueUserId) > 0 && Len(GroupId) > 0)
	{
		// List HttpRequest
		QueryGroupMembersRequest = class'HttpFactory'.static.CreateRequest();
		if (QueryGroupMembersRequest != none)
		{
			//Create URL parameters
			Url = GetBaseURL() $ QueryGroupMembersUrl $ GetAppAccessURL() $
				"&groupId=" $ GroupId;

			// Build our web request with the above URL
			QueryGroupMembersRequest.SetURL(URL);
			QueryGroupMembersRequest.SetVerb("GET");
			QueryGroupMembersRequest.OnProcessRequestComplete = OnQueryGroupMembersRequestComplete;

			// Call WebRequest
			if(!QueryGroupMembersRequest.ProcessRequest())
			{
				`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
			}
		}
		else
		{
			OnQueryGroupMembersComplete(GroupId, false, "HttpRequest not created");
		}
	}
	else
	{
		OnQueryGroupMembersComplete(GroupId, false, "UserId and/or GroupId not specified");
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
delegate OnQueryGroupMembersRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Error;
	local string JsonString;
	local JsonObject ParsedJson;
	local int JsonIndex;
	local EMcpGroupAcceptState AcceptState;
	local string MemberId;
	local string GroupId;

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
					MemberId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("unique_user_id");
					GroupId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("group_id");
					AcceptState = EMcpGroupAcceptState(ParsedJson.ObjectArray[JsonIndex].GetIntValue("status"));
			
					CacheGroupMember(MemberId, GroupId, AcceptState);
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
	OnQueryGroupMembersComplete(GroupId, bWasSuccessful, Error);
}

/**
 * Returns the set of Group Members that belong to the specified GroupId
 * Called after QueryGroupMembers. 
 * 
 * @param GroupId the request object that was used
 * @param GroupList the response object that was generated
 */
function GetGroupMembers(String GroupId, out array<McpGroupMember> GroupMembers)
{
	local int GroupIndex;
	local McpGroupList GroupList;
	local McpGroup GroupTemp;

	foreach GroupLists(GroupList)
	{
		foreach GroupList.Groups(GroupTemp, GroupIndex)
		{
			if(GroupTemp.GroupId == GroupId)
			{
				GroupMembers = GroupTemp.Members;
			}
		}
	}
}

/**
 * Adds an Group Members to the specified Group. Sends this request to MCP to be processed
 *
 * @param UniqueUserId the user that owns the group
 * @param GroupId the group id
 * @param MemberIds list of member ids to add to the group
 * @param bRequiresAcceptance whether or not members need to accept an invitation to a group
 */
function AddGroupMembers(String UniqueUserId, String GroupId, const out array<String> MemberIds, bool bRequiresAcceptance)
{
	local string Url;
	local HttpRequestInterface AddGroupMembersRequest;
	local string JsonPayload;
	local int Index;
	

	// Make sure that UniqueUserId and GroupId have been given
	if( Len(UniqueUserId) > 0 && Len(GroupId) > 0)
	{
		// Create HttpRequest
		AddGroupMembersRequest = class'HttpFactory'.static.CreateRequest();
		AddGroupMembersRequest.OnProcessRequestComplete = OnAddGroupMembersRequestComplete;
		if(AddGroupMembersRequest != none)
		{
			Url = GetBaseURL() $ AddGroupMembersUrl $ GetAppAccessURL() $
				"&uniqueUserId=" $ UniqueUserId $ 
				"&groupId=" $ GroupId $
				"&requiresAcceptance=" $ bRequiresAcceptance ? "true" : "false";
				// If bRequiresAcceptance is set to false then the group member will be created with a status of Accepted by default
				// If it is set to true then the status will be set to Pending and the user will need to accept the invite to officially be part of the group
		
			if(MemberIds.Length > 0)
			{
				// Make a json string from our list of ids
				JsonPayload = "[ ";
				for (Index = 0; Index < MemberIds.Length; Index++)
				{
					JsonPayload $= "\"" $ MemberIds[Index] $ "\"";
					// Only add the comma to the string if this isn't the last item
					if (Index + 1 < MemberIds.Length)
					{
						JsonPayload $= ",";
					}
				}
				JsonPayload $= " ]";

				// Fill it out using parameters
				AddGroupMembersRequest.SetVerb("POST");
				AddGroupMembersRequest.SetContentAsString(JsonPayload);
				AddGroupMembersRequest.SetURL(URL);

				// Call WebRequest
				if (!AddGroupMembersRequest.ProcessRequest())
				{
					`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
				}
				`Log(`Location@"URL(" $ Url $ ")");
			}
			else
			{
				`Log(`Location@" No MemberIds given.");
			}
		}
		else
		{
			OnAddGroupMembersComplete(GroupId, false, "HttpRequest was not created");
		}
	}
	else
	{
		OnAddGroupMembersComplete(GroupId, false, "UserId and/or GroupId not specified");
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
delegate OnAddGroupMembersRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Content; 
	local String GroupId;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	if (HttpResponse != none)
	{
		GroupId = HttpResponse.GetURLParameter("GroupId");
		ResponseCode = HttpResponse.GetResponseCode();
		Content = HttpResponse.GetContentAsString();
	}
	// Both of these need to be true for the request to be a success
	bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;

	OnAddGroupMembersComplete(GroupId, bWasSuccessful, Content);
}

/**
 * Remove Group Members from the specified Group. Sends this request to MCP to be processed
 *
 * @param UniqueUserId the user that owns the group
 * @param GroupId the group id
 * @param MemberIds list of member ids to add to the group
 */
function RemoveGroupMembers(String UniqueUserId, String GroupId, const out array<String> MemberIds)
{
	local string Url;
	local HttpRequestInterface RemoveGroupMembersRequest;
	local string JsonPayload;
	local int Index;

	// Make sure that UniqueUserId and GroupId have been given
	if( Len(UniqueUserId) > 0 && Len(GroupId) > 0)
	{
		RemoveGroupMembersRequest = class'HttpFactory'.static.CreateRequest();
		if(RemoveGroupMembersRequest != none)
		{

			Url = GetBaseURL() $ RemoveGroupMembersUrl $ GetAppAccessURL() $
				"&groupId=" $ GroupId;

			if(MemberIds.Length > 0)
			{
					// Make a json string from our list of ids
					JsonPayload = "[ ";
					for (Index = 0; Index < MemberIds.Length; Index++)
					{
						JsonPayload $= "\"" $ MemberIds[Index] $ "\"";
						// Only add the comma to the string if this isn't the last item
						if (Index + 1 < MemberIds.Length)
						{
							JsonPayload $= ",";
						}
					}
					JsonPayload $= " ]";

				RemoveGroupMembersRequest.SetURL(URL);
				RemoveGroupMembersRequest.SetContentAsString(JsonPayload);
				RemoveGroupMembersRequest.SetVerb("DELETE");
				RemoveGroupMembersRequest.OnProcessRequestComplete = OnRemoveGroupMembersRequestComplete;

				// Call the request
				if (!RemoveGroupMembersRequest.ProcessRequest())
				{
					`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
				}
			}
			else
			{
				`Log(`Location@" No MemberIds given.");
			}
		}
		else
		{
			OnRemoveGroupMembersComplete(GroupId, false, "Http request was not created");
		}
	}
	else
	{
		OnRemoveGroupMembersComplete(GroupId, false, "UniqueUserId and/or GroupId was not specified");
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
function OnRemoveGroupMembersRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Content; 
	local String GroupId;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	if (HttpResponse != none)
	{
		GroupId = HttpResponse.GetURLParameter("GroupId");
		ResponseCode = HttpResponse.GetResponseCode();
		Content = HttpResponse.GetContentAsString();
	}
	// Both of these need to be true for the request to be a success
	bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;

	OnRemoveGroupMembersComplete(GroupId, bWasSuccessful, Content);
}

/**
 * Deletes all Groups that belong to UniqueUserId
 * 
 * @param UniqueUserId UserId of the owner of the Group
 */
function DeleteAllGroups(String UniqueUserId)
{
	local string Url;
	local HttpRequestInterface  DeleteGroupRequest;

	// Make sure that UniqueUserId and GroupId have been given
	if( Len(UniqueUserId) > 0 )
	{
		// Delete HttpRequest
		DeleteGroupRequest = class'HttpFactory'.static.CreateRequest();
		if(DeleteGroupRequest != none)
		{

			// Fill it out using parameters
			DeleteGroupRequest.SetVerb("DELETE");

			Url = GetBaseURL() $ DeleteAllGroupsUrl $ GetAppAccessURL() $
				"&uniqueUserId=" $ UniqueUserId;

			DeleteGroupRequest.SetURL(URL);

			`log("URL="@DeleteGroupRequest.GetURL());

			DeleteGroupRequest.OnProcessRequestComplete = OnDeleteGroupRequestComplete;

			// call WebRequest
			if(!DeleteGroupRequest.ProcessRequest())
			{
				`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
			}
		}
		else
		{
			OnDeleteAllGroupsComplete(UniqueUserId, false, "HttpRequest was not created");
		}
	}
	else
	{
		OnDeleteAllGroupsComplete(UniqueUserId, false, "UniqueUserId was not specified");
	}
}

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
function OnDeleteAllGroupsRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Content; 
	local string RequesterId;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	// Both HttpResponse and OriginalRequest need to be present
	if (HttpResponse != none && OriginalRequest != none)
	{
		RequesterId = OriginalRequest.GetURLParameter("uniqueUserId");
		ResponseCode = HttpResponse.GetResponseCode();
		Content = HttpResponse.GetContentAsString();
	}
	// Both of these need to be true for the request to be a success
	bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;

	OnDeleteAllGroupsComplete(RequesterId, bWasSuccessful, Content);
}

/**
 * Set's the a Member's membership status to Accept or Reject
 * based on the value of bShouldAccept
 * 
 * @param UniqueUserId User who's status is to be update
 * @param GroupId 
 * @param bShouldAccept 1 = accepted 0 = rejected
 */
function AcceptGroupInvite(String UniqueUserId, String GroupId, bool bShouldAccept)
{
	local string Url;
	local HttpRequestInterface AcceptGroupInviteRequest;
	
	// Make sure that UniqueUserId and GroupId have been given
	if( Len(UniqueUserId) > 0 && Len(GroupId) > 0)
	{
		// Create HttpRequest
		AcceptGroupInviteRequest = class'HttpFactory'.static.CreateRequest();
		if(AcceptGroupInviteRequest != none)
		{
			Url = GetBaseURL() $ AcceptGroupInviteUrl $ GetAppAccessURL() $
				"&uniqueUserId=" $ UniqueUserId $ 
				"&groupId=" $ GroupId $ 
				"&status=" $ bShouldAccept ? "accepted" : "rejected";

			// Fill it out using parameters
			AcceptGroupInviteRequest.SetVerb("POST");
			AcceptGroupInviteRequest.SetURL(URL);
			AcceptGroupInviteRequest.OnProcessRequestComplete = OnAcceptGroupInviteRequestComplete;
		
			// Call WebRequest
			`log("Calling Process Request");
			if(!AcceptGroupInviteRequest.ProcessRequest())
			{
				`Log(`Location@"Failed to process web request for URL(" $ Url $ ")");
			}
			
		}
		else
		{
			OnAcceptGroupInviteComplete(GroupId, false, "HttpRequest not created");
		}
	}
	else
	{
		OnAcceptGroupInviteComplete(GroupId, false, "UniqueUserId or GroupId was not specified");
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
delegate OnAcceptGroupInviteRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface HttpResponse, bool bWasSuccessful)
{
	local int ResponseCode;
	local string Content; 
	local String GroupId;

	ResponseCode = `HTTP_STATUS_SERVER_ERROR;
	if (HttpResponse != none)
	{
		GroupId = HttpResponse.GetURLParameter("GroupId");
		ResponseCode = HttpResponse.GetResponseCode();
		Content = HttpResponse.GetContentAsString();
	}
	// Both of these need to be true for the request to be a success
	bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;

	OnAcceptGroupInviteComplete(GroupId, bWasSuccessful, Content);
}


/**
 *Store group in an object in memory instead of having to query the server again for it
 * 
 * @param Group to be placed in the cache
 */
function CacheGroup(string RequesterId, McpGroup Group)
{
	local int AddAt;
	local int GroupIndex;	
	local int GroupListIndex;
	local McpGroupList UserGroupList;
	local bool bWasFound;

	//Find the user's cached group list in collection of users group lists, GroupLists
	//TODO Is there a better way to do this? Have to rely on the response not being empty, though if it is then we don't do anything so that's fine.
	// - More importantly we're querying the GroupLists for every entry returned which isn't as efficient.
	// - Could just set GroupListIndex and then check if it's been set ...
	bWasFound = false;
	GroupListIndex = GroupLists.Find('RequesterId', RequesterId);
	if(GroupListIndex != INDEX_NONE)
	{
		UserGroupList = GroupLists[GroupListIndex];

		// Search the array for any existing adding only when missing
		for (GroupIndex = 0; GroupIndex < UserGroupList.Groups.Length && !bWasFound; GroupIndex++)
		{
			bWasFound = Group.GroupId == UserGroupList.Groups[GroupIndex].GroupId;
		}
		// Add this one since it wasn't found
		if (!bWasFound)
		{
			AddAt = UserGroupList.Groups.Length;
			UserGroupList.Groups.Length = AddAt + 1;
			UserGroupList.Groups[AddAt] = Group;
			GroupLists[GroupListIndex] = UserGroupList;
		}

		`log(`Location $ " GroupName: " $ UserGroupList.Groups[AddAt].GroupName);

	}
	else
	{
		// Add User with this first returned group since it wasn't found
		AddAt = GroupLists.Length;
		GroupLists.Length = AddAt +1;
		GroupLists[AddAt].RequesterId = Group.OwnerId;
		GroupLists[AddAt].Groups[0]=Group;
	}
}

/**
 *Store group member in an object in memory instead of having to query the server again for it
 * 
 * @param MemberId to be placed in the cache
 * @param GroupId to be placed in the cache
 * @param intAcceptState whether the group member's status is accepted, pending or rejected 
 */
function CacheGroupMember(String MemberId, String GroupId, EMcpGroupAcceptState AcceptState)
{
	local int MemberIndex;
	local McpGroupList GroupList;
	local int GroupListIndex;
	local McpGroup GroupTemp;
	local int GroupIndex;
	local int AddAt;

	// Have the variables been passed in properly
	if(Len(MemberId) > 0 && Len(GroupId) > 0 && Len(AcceptState) > 0)
	{
		// Look at each GroupList (userId:<groups>) mapping to see where to add/update this member
		foreach GroupLists(GroupList, GroupListIndex)
		{
			// For each group related to the user see if it's the GroupId specified
			foreach GroupList.Groups(GroupTemp, GroupIndex)
			{
				// If the group is found Update the Member field
				// This will potential run multiple times as it will be stored under both owners and members in GroupLists
				if(GroupTemp.GroupId == GroupId)
				{
					// Locate the proper place to update or add the member
					MemberIndex = GroupTemp.Members.find('MemberId', MemberId);
					if(MemberIndex == INDEX_NONE)
					{
						// No MemberId found so add the member
						AddAt = GroupTemp.Members.Length;
						GroupTemp.Members.Length = AddAt +1;
						GroupTemp.Members[AddAt].MemberId = MemberId;
						GroupTemp.Members[AddAt].AcceptState = AcceptState;
					}
					else
					{
						// GroupId and MemberId have been confirmed so just update the accept state if it's changed
						if(GroupTemp.Members[MemberIndex].AcceptState != AcceptState)
						{
							GroupTemp.Members[MemberIndex].AcceptState = AcceptState;
						}
					}
					// Set the group at the location to the updated (or unchanged) group
					GroupList.Groups[GroupIndex] = GroupTemp;
				}
			}
			// Set the GroupList at the location to the updated (or unchanged) GroupList
			GroupLists[GroupListIndex] = GroupList;
		}
	}
	else
	{
		`Log(`Location@" Either the MemberId, GroupId, or AcceptState was not Specified.");
	}
}
