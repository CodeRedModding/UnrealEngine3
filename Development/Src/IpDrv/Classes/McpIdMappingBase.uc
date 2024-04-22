/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides the interface for mapping account ids and the factory method
 * for creating the registered implementing object
 */
class McpIdMappingBase extends McpServiceBase
	abstract
	config(Engine);

/** The class name to use in the factory method to create our instance */
var config String McpIdMappingClassName;

/**
 * Maps an McpId to an external account (Facebook, G+, Twitter, etc.)
 */
struct McpIdMapping
{
	/** The McpId that owns this mapping */
	var String McpId;
	/** The external account id that is being mapped to the McpId */
	var String ExternalId;
	/** The type of account (Facebook, G+, Twitter, GameCenter, etc.) this is to avoid name collisions */
	var String ExternalType;
};

/**
 * @return the object that implements this interface or none if missing or failed to create/load
 */
final static function McpIdMappingBase CreateInstance()
{
	local class<McpIdMappingBase> McpIdMappingBaseClass;
	local McpIdMappingBase NewInstance;

	McpIdMappingBaseClass = class<McpIdMappingBase>(DynamicLoadObject(default.McpIdMappingClassName,class'Class'));
	// If the class was loaded successfully, create a new instance of it
	if (McpIdMappingBaseClass != None)
	{
		NewInstance = new McpIdMappingBaseClass;
		NewInstance.Init();
	}

	return NewInstance;
}

/**
 * Adds an external account mapping. Sends this request to MCP to be processed
 *
 * @param McpId the account to add the mapping to
 * @param ExternalId the external account that is being mapped to this account
 * @param ExternalType the type of account for disambiguation
 */
function AddMapping(String McpId, String ExternalId, String ExternalType);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param McpId the MCP account that this external account is being added to
 * @param ExternalId the account id that was being mapped
 * @param ExternalType the external account type that was being mapped
 * @param bWasSuccessful whether the mapping succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnAddMappingComplete(String McpId, String ExternalId, String ExternalType, bool bWasSuccessful, String Error);

/**
 * Queries the backend for the McpIds of the list of external ids of a specific type
 * 
 * @param ExternalIds the set of ids to get McpIds for
 * @param ExternalType the type of account that is being mapped to McpIds
 */
function QueryMappings(const out array<String> ExternalIds, String ExternalType);

/**
 * Called once the query results come back from the server to indicate success/failure of the request
 *
 * @param ExternalType the external account type that was being queried for
 * @param bWasSuccessful whether the query succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQueryMappingsComplete(String ExternalType, bool bWasSuccessful, String Error);

/**
 * Returns the set of id mappings that match the requested account type
 * 
 * @param ExternalType the account type that we want the mappings for
 * @param IdMappins the out array that gets the copied data
 */
function GetIdMappings(String ExternalType, out array<McpIdMapping> IdMappings);
