/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base interface for manipulating a user's managed values
 */
class McpManagedValueManagerBase extends McpServiceBase
	abstract
	config(Engine);

/** The class name to use in the factory method to create our instance */
var config String McpManagedValueManagerClassName;

/**
 * Name to value mapping for a value managed on the MCP server
 */
struct ManagedValue
{
	/** The ID of the value */
	var Name ValueId;
	/** The value the server from the server */
	var int Value;
};

/**
 * Holds a single user's save slot information for managed values
 */
struct ManagedValueSaveSlot
{
	/** The owner of this save slot */
	var String OwningMcpId;
	/** The save slot id */
	var String SaveSlot;
	/** The list of managed values in this save slot */
	var array<ManagedValue> Values;
};

/**
 * @return the object that implements this interface or none if missing or failed to create/load
 */
final static function McpManagedValueManagerBase CreateInstance()
{
	local class<McpManagedValueManagerBase> McpManagedValueManagerBaseClass;
	local McpManagedValueManagerBase NewInstance;

	McpManagedValueManagerBaseClass = class<McpManagedValueManagerBase>(DynamicLoadObject(default.McpManagedValueManagerClassName,class'Class'));
	// If the class was loaded successfully, create a new instance of it
	if (McpManagedValueManagerBaseClass != None)
	{
		NewInstance = new McpManagedValueManagerBaseClass;
		NewInstance.Init();
	}

	return NewInstance;
}

/**
 * Creates the user's specified save slot
 * 
 * @param McpId the id of the user that requested the create
 * @param SaveSlot the save slot that is being create
 */
function CreateSaveSlot(String McpId, String SaveSlot);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param McpId the id of the user that requested the save slot create
 * @param SaveSlot the save slot that was created
 * @param bWasSuccessful whether the mapping succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnCreateSaveSlotComplete(string McpId, string SaveSlot, bool bWasSuccessful, String Error);

/**
 * Reads all of the values in the user's specified save slot
 * 
 * @param McpId the id of the user that requested the read
 * @param SaveSlot the save slot that is being read
 */
function ReadSaveSlot(String McpId, String SaveSlot);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param McpId the id of the user that requested the read
 * @param SaveSlot the save slot that was being read
 * @param bWasSuccessful whether the mapping succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnReadSaveSlotComplete(string McpId, string SaveSlot, bool bWasSuccessful, String Error);

/**
 * @return The list of values for the requested user's specified save slot
 */
function array<ManagedValue> GetValues(String McpId, String SaveSlot);

/**
 * @return The value the server returned for the requested value id from the user's specific save slot
 */
function int GetValue(String McpId, String SaveSlot, Name ValueId);

/**
 * Updates a specific value in the user's specified save slot
 * 
 * @param McpId the id of the user that requested the update
 * @param SaveSlot the save slot that was being updated
 * @param ValueId the value that the server should update
 * @param Value the value to apply as the update (delta or absolute determined by the server)
 */
function UpdateValue(String McpId, String SaveSlot, Name ValueId, int Value);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param McpId the id of the user that requested the update
 * @param SaveSlot the save slot that was being updated
 * @param ValueId the value id that was updated
 * @param Value the value that the server returned as part of the update (in case the server overrides it)
 * @param bWasSuccessful whether the mapping succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnUpdateValueComplete(string McpId, string SaveSlot, Name ValueId, int Value, bool bWasSuccessful, String Error);

/**
 * Deletes a value from the user's specified save slot
 * 
 * @param McpId the id of the user that requested the delete
 * @param SaveSlot the save slot that is having the value deleted from
 * @param ValueId the value id for the server to delete
 */
function DeleteValue(String McpId, String SaveSlot, Name ValueId);

/**
 * Called once the results come back from the server to indicate success/failure of the operation
 *
 * @param McpId the id of the user that requested the delete
 * @param SaveSlot the save slot that was having the value deleted from
 * @param ValueId the value id that the server deleted
 * @param bWasSuccessful whether the mapping succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnDeleteValueComplete(string McpId, string SaveSlot, Name ValueId, bool bWasSuccessful, String Error);
