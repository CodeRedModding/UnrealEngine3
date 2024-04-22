/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides the interface for user groups and the factory method
 * for creating the registered implementing object
 */
class McpGroupsBase extends McpServiceBase
	abstract 
	config(Engine);

/** The class name to use in the factory method to create our instance */
var config string McpGroupsManagerClassName;

/**
 * Enum for whether or not a member has accepted a group invite
 */
enum EMcpGroupAcceptState
{
	/** Member has rejected the invite to the group, or there has been an error. */
	MGAS_Error,

	/** Member has not yet responded to the group invite.	*/
	MGAS_Pending,

	/** Member has accepted the group invite. */
	MGAS_Accepted
};

/**
 * Enum for the Access Level of the group
 */
enum EMcpGroupAccessLevel
{
	/** Only Owners have permission to make changes to this group	*/
	MGAL_Owner,

	/** Members and Owners have permission to make changes to this group */
	MGAL_Member,

	/** Everyone has permission to make changes to this group */
	MGAL_Public
};

/**
 * Group Member
 */
struct McpGroupMember
{
	/** Member Id */
	var String MemberId;

	/** Member's Accept State to the group */
	var EMcpGroupAcceptState AcceptState;
};

/**
 *  Group
 */
struct McpGroup
{
	/** The User Id of the owner of the group */
	var String OwnerId;

	/** Unique Group Id */
	var String GroupId;

	/** Human Readable Name of the group.*/
	var String GroupName;
	
	/** 
	 * Access level defining who can make modifications to the group.
	 * Note: Access is not restricted on the server, it is up to those using the API to enforce these permissions.
	 */
	var EMcpGroupAccessLevel AccessLevel;

	/** Collection of users who are members of the group */
	var array<McpGroupMember> Members;

};

/**
 * List of Groups belonging to one user 
 */
struct McpGroupList
{
	/** User that requested the list of groups */
	var string RequesterId;

	/** Collection of groups that the user owns OR belongs to */
	var array<McpGroup> Groups;
	
};

/** Holds the groups for each user in memory */
var array<McpGroupList> GroupLists;

/**
 * Create Instance of an McpGroup
 * @return the object that implements this interface or none if missing or failed to create/load
 */
static final function McpGroupsBase CreateInstance()
{
	local class<McpGroupsBase> McpGroupsManagerClass;
	local McpGroupsBase NewInstance;

	McpGroupsManagerClass = class<McpGroupsBase>(DynamicLoadObject(default.McpGroupsManagerClassName,class'Class'));
	if (McpGroupsManagerClass != None)
	{
		NewInstance = new McpGroupsManagerClass;
		NewInstance.Init();
	}

	return NewInstance;
}


/**
 * Creates the URL and sends the request to create a group.
 * 
 * @param UniqueUserId the UserId that will own the group
 * @param GroupName the name the group will be created with
 */
function CreateGroup(String OwnerId, String GroupName);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param GroupName the name of the group 
 * @param GroupId the group id of the group that was created
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnCreateGroupComplete(McpGroup Group, bool bWasSuccessful, String Error);

/**
 * Deletes a Group by GroupId
 * 
 * @param UniqueUserId UserId of the owner of the Group
 * @param GroupId Id of the group
 */
function DeleteGroup(String UniqueUserId, String GroupId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param GroupId the group id of the group that was Deleted
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnDeleteGroupComplete(String GroupId, bool bWasSuccessful, String Error);

/**
 * Queries the backend for the Groups belonging to the supplied UserId
 * 
 * @param UniqueUserId the id of the owner of the groups to return
 */
function QueryGroups(String RequesterId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param UserId the user id of the groups that were queried
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQueryGroupsComplete(string UserId, bool bWasSuccessful, String Error);

/**
 * Returns the set of groups that belonging to the specified UserId
 * Called after QueryGroups. 
 * 
 * @param UserId the request object that was used
 * @param GroupList the response object that was generated
 */
function GetGroupList(string UserId, out McpGroupList GroupList);

/**
 * Queries the back-end for the Groups Members belonging to the specified group
 * 
 * @param UniqueUserId the id of the owner of the group being queried
 * @param GroupId the id of the owner of the groups to return
 */
function QueryGroupMembers(String UniqueUserId, String GroupId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param GroupId the id of the group from which the members were queried
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQueryGroupMembersComplete(String GroupId, bool bWasSuccessful, String Error);

/**
 * Returns the set of Group Members that belong to the specified GroupId
 * Called after QueryGroupMembers. 
 * 
 * @param GroupId the request object that was used
 * @param GroupList the response object that was generated
 */
function GetGroupMembers(String GroupId, out array<McpGroupMember> GroupMembers);

/**
 * Adds an Group Members to the specified Group. Sends this request to MCP to be processed
 *
 * @param UniqueUserId the user that owns the group
 * @param GroupId the group id
 * @param MemberIds list of member ids to add to the group
 * @param bRequiresAcceptance whether or not members need to accept an invitation to a group
 */
function AddGroupMembers(String OwnerId, String GroupId, const out array<String> MemberIds, bool bRequiresAcceptance); 

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param GroupId the id of the group to which the members were added
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnAddGroupMembersComplete(String GroupId, bool bWasSuccessful, String Error);

/**
 * Remove Group Members from the specified Group. Sends this request to MCP to be processed
 *
 * @param UniqueUserId the user that owns the group
 * @param GroupId the group id
 * @param MemberIds list of member ids to add to the group
 */
function RemoveGroupMembers(String OwnerId, String GroupId, const out array<String> MemberIds);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param GroupId the id of the group from which the members were removed
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnRemoveGroupMembersComplete(String GroupId, bool bWasSuccessful, String Error);

/**
 * Deletes all Groups that belong to UniqueUserId
 * 
 * @param UniqueUserId UserId of the owner of the Group
 */
function DeleteAllGroups(String OwnerId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnDeleteAllGroupsComplete(String RequesterId, bool bWasSuccessful, String Error);

/**
 * Queries the backend for the Pending Group Invites belonging to the supplied UserId
 * 
 * @param UniqueUserId the id of the user to query invites for
 */
function QueryGroupInvites(String UniqueUserId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param bWasSuccessful whether the operation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQueryGroupInvitesComplete(bool bWasSuccessful, String Error);

/**
 * Returns the set of Pending Group Invites that belong to the specified UserId
 * Called after QueryGroupInvites. 
 * 
 * @param UserId the request object that was used
 * @param GroupList the response object that was generated
 */
function GetGroupInviteList(string UserId, out McpGroupList InviteList);

/**
 * Set's the a Member's membership status to Accept or Reject
 * based on the value of bShouldAccept
 * 
 * @param UniqueUserId User who's status is to be update
 * @param GroupId 
 * @param bShouldAccept 1 = accepted 0 = rejected
 */
function AcceptGroupInvite(String UniqueUserId, String GroupId, bool bShouldAccept);


/**
 * Called once the request/response has completed. Used to process the add mapping result and notify any
 * registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
delegate OnAcceptGroupInviteComplete(String GroupId, bool bWasSuccessful, String Error);