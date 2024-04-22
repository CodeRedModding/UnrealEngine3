/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base interface for manipulating a user's inventory
 */
class McpUserInventoryBase extends McpServiceBase
	abstract
	config(Engine);

/** The class name to use in the factory method to create our instance */
var config string McpUserInventoryClassName;

/**
 * Item property which was generated from attribute tables for the item when it was instanced
 */
struct McpInventoryItemAttribute
{
	/** The unique id for the attribute */
	var string AttributeId;
	/** The instanced value for this attribute */
	var int Value;
};

/**
 * Instanced inventory item aquired via (purchase,earn,etc)
 */
struct McpInventoryItem
{
	/** The unique id of the item instance */
	var string InstanceItemId;
	/** The id of the original template this item was instanced from */
	var string GlobalItemId;
	/** Total earned amount of this item */
	var int Quantity;
	/** Total IAP aquired amount of this item */
	var int QuantityIAP;
	/** Scalar that persists from when the item was instanced */
	var float Scalar;
	/** The last time (UTC) when this item was modified */
	var string LastUpdateTime;
	/** Attributes that were generated when instanced to define the item properties */
	var array<McpInventoryItemAttribute> Attributes;
};

/**
 * Allows for specifying list of global items and quantities
 */
struct McpInventoryItemContainer
{
	/** The id of the template item */
	var string GlobalItemId;
	/** Total amount of this item */
	var int Quantity;
};

/**
 * Holds a single user's save slot information for inventory items
 */
struct McpInventorySaveSlot
{
	/** The owner of this save slot */
	var string OwningMcpId;
	/** The save slot id */
	var string SaveSlotId;
	/** The list of inventory items in this save slot */
	var array<McpInventoryItem> Items;
};

/**
 * @return the object that implements this interface or none if missing or failed to create/load
 */
final static function McpUserInventoryBase CreateInstance()
{
	local class<McpUserInventoryBase> McpUserInventoryBaseClass;
	local McpUserInventoryBase NewInstance;

	McpUserInventoryBaseClass = class<McpUserInventoryBase>(DynamicLoadObject(default.McpUserInventoryClassName,class'Class'));
	// If the class was loaded successfully, create a new instance of it
	if (McpUserInventoryBaseClass != None)
	{
		NewInstance = new McpUserInventoryBaseClass;
		NewInstance.Init();
	}

	return NewInstance;
}

/**
 * Creates a new save slot for the user
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that is being create
 * @param ParentSaveSlotId [optional] if specified then all existing IAP purchases 
 *	from the parent save slot carry over to the newly created save slot
 */
function CreateSaveSlot(string McpId, string SaveSlotId, optional string ParentSaveSlotId);

/**
 * Called once save slot creation completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that was created
 * @param bWasSuccessful whether the creation succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnCreateSaveSlotComplete(string McpId, string SaveSlotId, bool bWasSuccessful, string Error);

/**
 * Deletes an existing user save slot and all the inventory items that belong to it
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that is being deleted
 */
function DeleteSaveSlot(string McpId, string SaveSlotId);

/**
 * Called once save slot deletion completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that was deleted
 * @param bWasSuccessful whether the deletion succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnDeleteSaveSlotComplete(string McpId, string SaveSlotId, bool bWasSuccessful, string Error);

/**
 * Query for the list of save slots that exist for the user
 * 
 * @param McpId the id of the user that made the request
 */
function QuerySaveSlotList(string McpId);

/**
 * Get the cached list of save slot ids for a user
 * 
 * @param McpId the id of the user that made the request
 *
 * @return the list of save slot ids for the user
 */
function array<string> GetSaveSlotList(string McpId);

/**
 * Called once save slot enumeration query completes
 *
 * @param McpId the id of the user that made the request
 * @param bWasSuccessful whether the deletion succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQuerySaveSlotListComplete(string McpId, bool bWasSuccessful, string Error);

/**
 * Query for the list of current inventory items that exist for the user within a save slot
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot to find items for
 */
function QueryInventoryItems(string McpId, string SaveSlotId);

/**
 * Called once inventory items enumeration query completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot to find items for
 * @param bWasSuccessful whether the deletion succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnQueryInventoryItemsComplete(string McpId, string SaveSlotId, bool bWasSuccessful, string Error);

/**
 * Access the currently cached inventory items that have been downloaded for a user's save slot
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot to find items for
 * @param OutInventoryItems the list of inventory items that should be filled in
 */
function GetInventoryItems(string McpId, string SaveSlotId, out array<McpInventoryItem> OutInventoryItems);

/**
 * Access the currently cached inventory item that have been downloaded for a user's save slot by global id
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot to find items for
 * @param InstanceItemId id for the item that is to be found
 * @param OutInventoryItem the item that should be filled in 
 *
 * @return true if found
 */
function bool GetInventoryItem(string McpId, string SaveSlotId, string InstanceItemId, out McpInventoryItem OutInventoryItem);

/**
 * Purchase item and add it to the user's inventory
 * Server determines the transaction is valid
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param GlobalItemId id for the item that is being purchased
 * @param PurchaseItemIds list of instance item ids to be used when making the purchase
 * @param StoreVersion current known version # of backend store by client
 * @param Scalar to associate with the item and pass back to client
 */
function PurchaseItem(string McpId, string SaveSlotId, string GlobalItemId, array<string> PurchaseItemIds, int Quantity, int StoreVersion, float Scalar);

/**
 * Called once item purchase completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param GlobalItemId id for the item that is being purchased
 * @param UpdatedItemIds list of global item ids for inventory items that were updated
 * @param bWasSuccessful whether the item purchase succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnPurchaseItemComplete(string McpId, string SaveSlotId, string GlobalItemId, array<string> UpdatedItemIds, bool bWasSuccessful, string Error);

/**
 * Sell item and remove it from the user's inventory
 * Server determines the transaction is valid
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param InstanceItemId id for the item that is being sold
 * @param StoreVersion current known version # of backend store by client
 * @param ExpectedResultItems optional list of items/quantities that the client expects to receive from the sale
 */
function SellItem(string McpId, string SaveSlotId, string InstanceItemId, int Quantity, int StoreVersion, optional const out array<McpInventoryItemContainer> ExpectedResultItems);

/**
 * Called once item sell completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param InstanceItemId id for the item that is being sold
 * @param UpdatedItemIds list of global item ids for inventory items that were updated
 * @param bWasSuccessful whether the item sell succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnSellItemComplete(string McpId, string SaveSlotId, string InstanceItemId, array<string> UpdatedItemIds, bool bWasSuccessful, string Error);

/**
 * Earn item and add it to the user's inventory
 * Server determines the transaction is valid
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param GlobalItemId id for the item that is being earned
 * @param Quantity number of the items that were earned
 * @param StoreVersion current known version # of backend store by client
 */
function EarnItem(string McpId, string SaveSlotId, string GlobalItemId, int Quantity, int StoreVersion);

/**
 * Called once item earn completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param GlobalItemId id for the item that is being earned
 * @param UpdatedItemIds list of global item ids for inventory items that were updated
 * @param bWasSuccessful whether the item earn succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnEarnItemComplete(string McpId, string SaveSlotId, string GlobalItemId, array<string> UpdatedItemIds, bool bWasSuccessful, string Error);

/**
 * Consume item and remove it's consumed quantity from the user's inventory
 * Server determines the transaction is valid
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param InstanceItemId id for the item that is being consumed
 * @param Quantity number of the items that were consumed
 * @param StoreVersion current known version # of backend store by client
 */
function ConsumeItem(string McpId, string SaveSlotId, string InstanceItemId, int Quantity, int StoreVersion);

/**
 * Called once item consume completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param InstanceItemId id for the item that is being consumed
 * @param UpdatedItemIds list of global item ids for inventory items that were updated
 * @param bWasSuccessful whether the item consume succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnConsumeItemComplete(string McpId, string SaveSlotId, string InstanceItemId, array<string> UpdatedItemIds, bool bWasSuccessful, string Error);

/**
 * Delete item and remove it from the user's inventory
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param InstanceItemId id for the item that is being deleted
 * @param StoreVersion current known version # of backend store by client
 */
function DeleteItem(string McpId, string SaveSlotId, string InstanceItemId, int StoreVersion);

/**
 * Called once item delete completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param InstanceItemId id for the item that is being deleted
 * @param bWasSuccessful whether the item delete succeeded or not
 * @param Error string information about the error (if an error)
 */
delegate OnDeleteItemComplete(string McpId, string SaveSlotId, string InstanceItemId, bool bWasSuccessful, string Error);

/**
 * Record an IAP (In App Purchase) by sending receipt to server for validation
 * Results in list of items being added to inventory if successful
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item(s) will be placed in once validated
 * @param Receipt IAP receipt to validate the purchase on the server
 */
function RecordIap(string McpId, string SaveSlotId, string Receipt);

/**
 * Called once record IAP operation completes
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item(s) will be placed in once validated
 * @param UpdatedItemIds list of item ids for inventory items that were updated
 * @param bWasSuccessful whether the IAP succeeded and was validated or not
 * @param Error string information about the error (if an error)
 */
delegate OnRecordIapComplete(string McpId, string SaveSlotId, array<string> UpdatedItemIds, bool bWasSuccessful, string Error);