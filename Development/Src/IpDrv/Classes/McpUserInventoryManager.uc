/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the concrete implementation
 */
class McpUserInventoryManager extends McpUserInventoryBase;

`include(Engine\Classes\HttpStatusCodes.uci)

/** The URL to use when making save slot for a user */
var config string CreateSaveSlotUrl;
/** The URL to use when deleting a save slot for a user (along with all inventory items belonging to it) */
var config string DeleteSaveSlotUrl;
/** The URL to use when listing the save slots that exist for a user */
var config string ListSaveSlotUrl;
/** The URL to use when listing user's inventory items for a specific save slot */
var config string ListItemsUrl;
/** The URL to use when purchasing an item to be placed in the user's inventory */
var config string PurchaseItemUrl;
/** The URL to use when selling an item from the user's inventory */
var config string SellItemUrl;
/** The URL to use when earning an item to be placed in the user's inventory */
var config string EarnItemUrl;
/** The URL to use when consuming an item from the user's inventory */
var config string ConsumeItemUrl;
/** The URL to use when deleting an item from the user's inventory */
var config string DeleteItemUrl;
/** The URL to use when recording an IAP has occurred */
var config string IapRecordUrl;

/** Cached list of user save slots that have been retrieved from the server */
var array<McpInventorySaveSlot> SaveSlots;

/** Holds the state information for an outstanding save slot request */
struct SaveSlotRequestState
{
	/** The MCP id for the request */
	var string McpId;
	/** The save slot involved */
	var string SaveSlotId;
	/** The HTTP request object for this request */
	var HttpRequestInterface Request;
};

/** Holds the state information for an outstanding inventory item request */
struct InventoryItemRequestState extends SaveSlotRequestState
{
	/** id of the item that is being operated on. Either instance or global id */
	var string ItemId;
};

/** The set of create/delete save slot requests that are pending */
var array<SaveSlotRequestState> SaveSlotRequests;

/** The set of list save slot requests that are pending */
var array<SaveSlotRequestState> ListSaveSlotRequests;

/** The set of list inventory items requests that are pending */
var array<SaveSlotRequestState> ListItemsRequests;

/** The set of purchase item requests that are pending */
var array<InventoryItemRequestState> ItemRequests;

/**
 * Creates a new save slot for the user
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that is being create
 * @param ParentSaveSlotId [optional] if specified then all existing IAP purchases 
 *	from the parent save slot carry over to the newly created save slot
 */
function CreateSaveSlot(string McpId, string SaveSlotId, optional string ParentSaveSlotId)
{
	local string Url;
	local HttpRequestInterface Request;
	local int AddAt,ExistingIndex;

	// Check for pending request
	ExistingIndex = FindSaveSlotRequest(McpId,SaveSlotId,SaveSlotRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ CreateSaveSlotUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId;

			if (Len(ParentSaveSlotId) > 0)
			{
				Url $= "&parentSaveSlotId=" $ ParentSaveSlotId;
			}

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.OnProcessRequestComplete = OnCreateSaveSlotRequestComplete;
			// Store off the data for reporting later
			AddAt = SaveSlotRequests.Length;
			SaveSlotRequests.Length = AddAt + 1;
			SaveSlotRequests[AddAt].McpId = McpId;
			SaveSlotRequests[AddAt].SaveSlotId = SaveSlotId;
			SaveSlotRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start CreateSaveSlot web request for URL(" $ Url $ ")");
			}
			`Log("Create save slot URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending save slot request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId);
	}
}

/**
 * Called once the request/response has completed. Used to process the create save slot result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnCreateSaveSlotRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index, SaveSlotIndex;
	local int ResponseCode;
	local string ResponseString;

	// Search for the corresponding entry in the array
	Index = SaveSlotRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			// Clear out old entry if it exists
			SaveSlotIndex = FindSaveSlotIndex(
				SaveSlotRequests[Index].McpId, 
				SaveSlotRequests[Index].SaveSlotId);
			if (SaveSlotIndex != INDEX_NONE)
			{
				SaveSlots.Remove(SaveSlotIndex,1);
			}
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload of default values for this save slot
			ParseInventoryForSaveSlot(
				SaveSlotRequests[Index].McpId,
				SaveSlotRequests[Index].SaveSlotId,
				ResponseString);
		}
		// Notify anyone waiting on this
		OnCreateSaveSlotComplete(
			SaveSlotRequests[Index].McpId,
			SaveSlotRequests[Index].SaveSlotId,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Create save slot:"
			$" McpId=" $ SaveSlotRequests[Index].McpId
			$" SaveSlot=" $	SaveSlotRequests[Index].SaveSlotId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );

		SaveSlotRequests.Remove(Index,1);
	}
}

/**
 * Deletes an existing user save slot and all the inventory items that belong to it
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that is being deleted
 */
function DeleteSaveSlot(string McpId, string SaveSlotId)
{
	local string Url;
	local HttpRequestInterface Request;
	local int AddAt, ExistingIndex;

	// Check for pending request
	ExistingIndex = FindSaveSlotRequest(McpId,SaveSlotId,SaveSlotRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ DeleteSaveSlotUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId;

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("DELETE");
			Request.OnProcessRequestComplete = OnDeleteSaveSlotRequestComplete;
			// Store off the data for reporting later
			AddAt = SaveSlotRequests.Length;
			SaveSlotRequests.Length = AddAt + 1;
			SaveSlotRequests[AddAt].McpId = McpId;
			SaveSlotRequests[AddAt].SaveSlotId = SaveSlotId;
			SaveSlotRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start DeleteSaveSlot web request for URL(" $ Url $ ")");
			}
			`Log("Delete save slot URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending save slot request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId);
	}
}

/**
 * Called once the request/response has completed. Used to process the delete save slot result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnDeleteSaveSlotRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index,SaveSlotIndex;
	local int ResponseCode;

	// Search for the corresponding entry in the array
	Index = SaveSlotRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			// Clear out old entry if it exists
			SaveSlotIndex = FindSaveSlotIndex(
				SaveSlotRequests[Index].McpId, 
				SaveSlotRequests[Index].SaveSlotId);
			if (SaveSlotIndex != INDEX_NONE)
			{
				SaveSlots.Remove(SaveSlotIndex,1);
			}
		}
		// Notify anyone waiting on this
		OnDeleteSaveSlotComplete(
			SaveSlotRequests[Index].McpId,
			SaveSlotRequests[Index].SaveSlotId,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Delete save slot:"
			$" McpId=" $ SaveSlotRequests[Index].McpId
			$" SaveSlot=" $	SaveSlotRequests[Index].SaveSlotId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );
		
		SaveSlotRequests.Remove(Index,1);
	}
}

/**
 * Query for the list of save slots that exist for the user
 * 
 * @param McpId the id of the user that made the request
 */
function QuerySaveSlotList(string McpId)
{
	local string Url;
	local HttpRequestInterface Request;
	local int AddAt, ExistingIndex;

	// Check for pending request
	ExistingIndex = ListSaveSlotRequests.Find('McpId',McpId);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ ListSaveSlotUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId;

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("GET");
			Request.OnProcessRequestComplete = OnQuerySaveSlotListRequestComplete;
			// Store off the data for reporting later
			AddAt = ListSaveSlotRequests.Length;
			ListSaveSlotRequests.Length = AddAt + 1;
			ListSaveSlotRequests[AddAt].McpId = McpId;
			ListSaveSlotRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start QuerySaveSlotList web request for URL(" $ Url $ ")");
			}
			`Log("Query save slot list URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending list save slot request for"
			$" McpId="$ McpId);
	}
}

/**
 * Called once the request/response has completed. Used to process the list save slot result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnQuerySaveSlotListRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local string ResponseString;

	// Search for the corresponding entry in the array
	Index = ListSaveSlotRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload of default values for this save slot
			ParseSaveSlotList(
				ListSaveSlotRequests[Index].McpId,
				ResponseString);
		}

		// Notify anyone waiting on this
		OnQuerySaveSlotListComplete(
			ListSaveSlotRequests[Index].McpId,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("List save slots:"
			$" McpId=" $ ListSaveSlotRequests[Index].McpId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );
		
		ListSaveSlotRequests.Remove(Index,1);
	}
}

/**
 * Get the cached list of save slot ids for a user
 * 
 * @param McpId the id of the user that made the request
 *
 * @return the list of save slot ids for the user
 */
function array<string> GetSaveSlotList(string McpId)
{
	local array<string> OutSaveSlots;
	local int SaveSlotIndex;

	OutSaveSlots.Length = SaveSlots.Length;
	for (SaveSlotIndex=0; SaveSlotIndex < SaveSlots.Length; SaveSlotIndex++)
	{
		OutSaveSlots[SaveSlotIndex] = SaveSlots[SaveSlotIndex].SaveSlotId;
	}
	return OutSaveSlots;
}

/**
 * Query for the list of current inventory items that exist for the user within a save slot
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot to find items for
 */
function QueryInventoryItems(string McpId, string SaveSlotId)
{
	local string Url;
	local HttpRequestInterface Request;
	local int AddAt,ExistingIndex;

	// Check for pending request
	ExistingIndex = FindSaveSlotRequest(McpId,SaveSlotId,ListItemsRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ ListItemsUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId;

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("GET");
			Request.OnProcessRequestComplete = OnQueryInventoryItemsRequestComplete;
			// Store off the data for reporting later
			AddAt = ListItemsRequests.Length;
			ListItemsRequests.Length = AddAt + 1;
			ListItemsRequests[AddAt].McpId = McpId;
			ListItemsRequests[AddAt].SaveSlotId = SaveSlotId;
			ListItemsRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start QueryInventoryItems web request for URL(" $ Url $ ")");
			}
			`Log("Query inventory items URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending save slot request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId);
	}
}

/**
 * Called once the request/response has completed. Used to process the list inventory result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnQueryInventoryItemsRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index,SaveSlotIndex;
	local int ResponseCode;
	local string ResponseString;

	// Search for the corresponding entry in the array
	Index = ListItemsRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			// Clear out old item entries if they exists
			SaveSlotIndex = FindSaveSlotIndex(
				ListItemsRequests[Index].McpId, 
				ListItemsRequests[Index].SaveSlotId);
			if (SaveSlotIndex != INDEX_NONE)
			{
				SaveSlots[SaveSlotIndex].Items.Length = 0;
			}
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload of default values for this save slot
			ParseInventoryForSaveSlot(
				ListItemsRequests[Index].McpId,
				ListItemsRequests[Index].SaveSlotId,
				ResponseString);
		}
		// Notify anyone waiting on this
		OnQueryInventoryItemsComplete(
			ListItemsRequests[Index].McpId,
			ListItemsRequests[Index].SaveSlotId,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Query inventory items:"
			$" McpId=" $ ListItemsRequests[Index].McpId
			$" SaveSlot=" $	ListItemsRequests[Index].SaveSlotId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );

		ListItemsRequests.Remove(Index,1);
	}
}

/**
 * Access the currently cached inventory items that have been downloaded for a user's save slot
 *
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot to find items for
 * @param OutInventoryItems the list of inventory items that should be filled in
 */
function GetInventoryItems(string McpId, string SaveSlotId, out array<McpInventoryItem> OutInventoryItems)
{
	local int SaveSlotIndex;

	// Clear it out
	OutInventoryItems.Length = 0;
	// Find the save slot for the user
	SaveSlotIndex = FindSaveSlotIndex(McpId,SaveSlotId);
	if (SaveSlotIndex != INDEX_NONE)
	{
		OutInventoryItems = SaveSlots[SaveSlotIndex].Items;
	}
	else
	{
		`Log("No save slot found for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId);
	}
}

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
function bool GetInventoryItem(string McpId, string SaveSlotId, string InstanceItemId, out McpInventoryItem OutInventoryItem)
{
	local int SaveSlotIndex;
	local int ItemIndex;

	// Find the save slot for the user
	SaveSlotIndex = FindSaveSlotIndex(McpId,SaveSlotId);
	if (SaveSlotIndex != INDEX_NONE)
	{
		ItemIndex = SaveSlots[SaveSlotIndex].Items.Find('InstanceItemId', InstanceItemId);
		if (ItemIndex != INDEX_NONE)
		{
			OutInventoryItem = SaveSlots[SaveSlotIndex].Items[ItemIndex];
			return true;
		}
		else
		{
			`Log("No inventory item found for "
				$" McpId="$ McpId
				$" SaveSlotId="$ SaveSlotId
				$" InstanceItemId="$ InstanceItemId);
		}
	}
	else
	{
		`Log("No save slot found for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId);
	}
	return false;
}

/**
 * Parses the json of inventory items for a user's save slot
 * 
 * @param McpId the user that owns the data
 * @param SaveSlot the save slot where the data is stored
 * @param JsonPayload the json data to parse
 */
function array<string> ParseInventoryForSaveSlot(string McpId, string SaveSlotId, string JsonPayload)
{
	local JsonObject ParsedJson;
	local JsonObject ParsedJsonAttrs;
	local int JsonIndex;
	local int SaveSlotIndex;
	local int ItemIndex;
	local int JsonAttrsIndex;
	local string GlobalItemId, InstanceItemId;
	local array<string> UpdatedItemIds;
	local array<JsonObject> ObjectArray;

	// Find the save slot for the user
	SaveSlotIndex = FindSaveSlotIndex(McpId, SaveSlotId);
	if (SaveSlotIndex == INDEX_NONE)
	{
		// Add the save slot since it is missing
		SaveSlotIndex = SaveSlots.Length;
		SaveSlots.Length = SaveSlotIndex + 1;
		SaveSlots[SaveSlotIndex].OwningMcpId = McpId;
		SaveSlots[SaveSlotIndex].SaveSlotId = SaveSlotId;
	}

	`Log("DEBUGSZ"
		$" JsonPayload="$ JsonPayload);

	// Parse json to obj graph
	ParsedJson = class'JsonObject'.static.DecodeJson(JsonPayload);
	ObjectArray = ParsedJson.ObjectArray;
	if (ObjectArray.Length == 0)
	{
		ObjectArray.AddItem(ParsedJson);
	}
	// Add/update each inventory item to the user's save slot
	for (JsonIndex = 0; JsonIndex < ObjectArray.Length; JsonIndex++)
	{
		// Grab the item id we need to store
		InstanceItemId = ObjectArray[JsonIndex].GetStringValue("instance_item_id");
		// global template that this item instance was created from
		GlobalItemId = ObjectArray[JsonIndex].GetStringValue("global_item_id");
		if (Len(InstanceItemId) > 0)
		{
			// keep track of items that were updated/added
			UpdatedItemIds.AddItem(InstanceItemId);
			// Find the existing item with the same id
			ItemIndex = SaveSlots[SaveSlotIndex].Items.Find('InstanceItemId', InstanceItemId);
			if (ItemIndex == INDEX_NONE)
			{
				// Not stored yet, so add one
				ItemIndex = SaveSlots[SaveSlotIndex].Items.Length;
				SaveSlots[SaveSlotIndex].Items.Length = ItemIndex + 1;
				SaveSlots[SaveSlotIndex].Items[ItemIndex].GlobalItemId = GlobalItemId;
				SaveSlots[SaveSlotIndex].Items[ItemIndex].InstanceItemId = InstanceItemId;
			}
			// Store the item
			SaveSlots[SaveSlotIndex].Items[ItemIndex].Quantity = ObjectArray[JsonIndex].GetIntValue("quantity");
			SaveSlots[SaveSlotIndex].Items[ItemIndex].QuantityIAP = ObjectArray[JsonIndex].GetIntValue("iap_quantity");
			SaveSlots[SaveSlotIndex].Items[ItemIndex].Scalar = ObjectArray[JsonIndex].GetFloatValue("scalar");
			SaveSlots[SaveSlotIndex].Items[ItemIndex].LastUpdateTime = ObjectArray[JsonIndex].GetStringValue("last_update_time");
			// Store the item attributes
			SaveSlots[SaveSlotIndex].Items[ItemIndex].Attributes.Length = 0;
			ParsedJsonAttrs = ObjectArray[JsonIndex].GetObject("attributes");
			SaveSlots[SaveSlotIndex].Items[ItemIndex].Attributes.Length = ParsedJsonAttrs.ObjectArray.Length;
			for (JsonAttrsIndex = 0; JsonAttrsIndex < ParsedJsonAttrs.ObjectArray.Length; JsonAttrsIndex++)
			{
				SaveSlots[SaveSlotIndex].Items[ItemIndex].Attributes[JsonAttrsIndex].AttributeId = 
					ParsedJsonAttrs.ObjectArray[JsonAttrsIndex].GetStringValue("attribute_id");
				SaveSlots[SaveSlotIndex].Items[ItemIndex].Attributes[JsonAttrsIndex].Value = 
					ParsedJsonAttrs.ObjectArray[JsonAttrsIndex].GetIntValue("value");
			}
		}
	}
	return UpdatedItemIds;
}

/**
 * Parses the json of for a user's save slot list
 * 
 * @param McpId the user that owns the data
 * @param JsonPayload the json data to parse
 */
function ParseSaveSlotList(string McpId, string JsonPayload)
{
	local JsonObject ParsedJson;
	local int JsonIndex;
	local int SaveSlotIndex;
	local string SaveSlotId;

	// Parse json to obj graph
	ParsedJson = class'JsonObject'.static.DecodeJson(JsonPayload);
	// Add/update each save slot entry
	for (JsonIndex = 0; JsonIndex < ParsedJson.ObjectArray.Length; JsonIndex++)
	{
		SaveSlotId = ParsedJson.ObjectArray[JsonIndex].GetStringValue("save_slot_id");
		// Find existing save slot entry
		SaveSlotIndex = FindSaveSlotIndex(McpId, SaveSlotId);
		if (SaveSlotIndex == INDEX_NONE)
		{
			// Add the save slot since it is missing
			SaveSlotIndex = SaveSlots.Length;
			SaveSlots.Length = SaveSlotIndex + 1;
			SaveSlots[SaveSlotIndex].OwningMcpId = McpId;
			SaveSlots[SaveSlotIndex].SaveSlotId = SaveSlotId;
		}
	}
}

/**
 * Searches the stored save slots for the ones for this user
 * 
 * @param McpId the user being searched for
 * @param SaveSlotId the save slot being searched for
 * 
 * @return the index of the save slot if found
 */
function int FindSaveSlotIndex(string McpId, string SaveSlotId)
{
	local int SaveSlotIndex;

	// Search all of the save slots for one that matches the user and slot id
	for (SaveSlotIndex = 0; SaveSlotIndex < SaveSlots.Length; SaveSlotIndex++)
	{
		if (SaveSlots[SaveSlotIndex].OwningMcpId == McpId &&
			SaveSlots[SaveSlotIndex].SaveSlotId == SaveSlotId)
		{
			return SaveSlotIndex;
		}
	}
	return INDEX_NONE;
}

/**
 * Find the request entry in the given pending list of requests
 *
 * @param McpId the id of the user to find
 * @param SaveSlotId the save slot to be found
 * @param SaveSlotRequests list of pending requests to find entry in
 * 
 * @return index into the list if found INDEX_NONE if not
 */
function int FindSaveSlotRequest(string McpId, string SaveSlotId, const out array<SaveSlotRequestState> InSaveSlotRequests)
{
	local int Index;
	for (Index=0; Index < InSaveSlotRequests.Length; Index++)
	{
		if (InSaveSlotRequests[Index].McpId == McpId &&
			InSaveSlotRequests[Index].SaveSlotId == SaveSlotId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

/**
 * Find the request entry in the given pending list of requests
 *
 * @param McpId the id of the user to find
 * @param SaveSlotId the save slot to be found
 * @param ItemId the item id to be found
 * @param InstanceItemId the instance item id to be found
 * @param SaveSlotRequests list of pending requests to find entry in
 * 
 * @return index into the list if found INDEX_NONE if not
 */
function int FindItemRequest(string McpId, string SaveSlotId, string ItemId, const out array<InventoryItemRequestState> InItemRequests)
{
	local int Index;
	for (Index=0; Index < InItemRequests.Length; Index++)
	{
		if (InItemRequests[Index].McpId == McpId &&
			InItemRequests[Index].SaveSlotId == SaveSlotId &&
			InItemRequests[Index].ItemId == ItemId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

/**
 * Purchase item and add it to the user's inventory
 * Server determines the transaction is valid
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param GlobalItemId id for the item that is being purchased
 * @param PurchaseItemIds list of instance item ids to be used when making the purchase
 * @param Quantity number of the item to purchase
 * @param StoreVersion current known version # of backend store by client
 */
function PurchaseItem(string McpId, string SaveSlotId, string GlobalItemId, array<string> PurchaseItemIds, int Quantity, int StoreVersion, float Scalar)
{
	local string Url, paymentItemsJson;
	local HttpRequestInterface Request;
	local int AddAt,ExistingIndex,Index;

	// Check for pending request
	ExistingIndex = FindItemRequest(McpId,SaveSlotId,GlobalItemId,ItemRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ PurchaseItemUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId $
				"&globalItemId=" $ GlobalItemId $
				"&quantity=" $ Quantity $
				"&storeVersion=" $ StoreVersion $
				"&scalar=" $ Scalar;

			if (PurchaseItemIds.Length > 0)
			{
				// list of inventory items that are to be used in exchange for the purchase
				paymentItemsJson = "[ ";
				for (Index = 0; Index < PurchaseItemIds.Length; Index++)
				{
					paymentItemsJson $= "\"" $ PurchaseItemIds[Index] $ "\"";
					if (Index + 1 < PurchaseItemIds.Length)
					{
						paymentItemsJson $= ",";
					}
				}
				paymentItemsJson $= " ]";
				Url $= "&paymentItemsJson=" $ paymentItemsJson;
			}

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.OnProcessRequestComplete = OnPurchaseItemRequestComplete;
			// Store off the data for reporting later
			AddAt = ItemRequests.Length;
			ItemRequests.Length = AddAt + 1;
			ItemRequests[AddAt].McpId = McpId;
			ItemRequests[AddAt].SaveSlotId = SaveSlotId;
			ItemRequests[AddAt].ItemId = GlobalItemId;
			ItemRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start PurchaseItem web request for URL(" $ Url $ ")");
			}
			`Log("Purchase item URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending item request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId
			$" GlobalItemId="$ GlobalItemId);
	}
}

/**
 * Called once the request/response has completed. Used to process the purchase item result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnPurchaseItemRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index, SaveSlotIndex, UpdatedItemIdIndex, FoundItemIndex;
	local int ResponseCode;
	local string ResponseString;
	local array<string> UpdatedItemIds;

	// Search for the corresponding entry in the array
	Index = ItemRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload for the new item values
			UpdatedItemIds = ParseInventoryForSaveSlot(
				ItemRequests[Index].McpId,
				ItemRequests[Index].SaveSlotId,
				ResponseString);

			// delete durable items with quantity=0
			SaveSlotIndex = FindSaveSlotIndex(ItemRequests[Index].McpId,ItemRequests[Index].SaveSlotId);
			if (SaveSlotIndex != INDEX_NONE)
			{
				// Iterate over list of items that were updated by server
				for (UpdatedItemIdIndex=0; UpdatedItemIdIndex < UpdatedItemIds.Length; UpdatedItemIdIndex++)
				{
					FoundItemIndex = SaveSlots[SaveSlotIndex].Items.Find('InstanceItemId', UpdatedItemIds[UpdatedItemIdIndex]);
					if (FoundItemIndex != INDEX_NONE &&
						SaveSlots[SaveSlotIndex].Items[FoundItemIndex].Quantity == 0 &&
						SaveSlots[SaveSlotIndex].Items[FoundItemIndex].QuantityIAP == 0)
					{
						SaveSlots[SaveSlotIndex].Items.Remove(FoundItemIndex,1);
					}
				}
			}
		}
		// Notify anyone waiting on this
		OnPurchaseItemComplete(
			ItemRequests[Index].McpId,
			ItemRequests[Index].SaveSlotId,
			ItemRequests[Index].ItemId,
			UpdatedItemIds,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Purchase item:"
			$" McpId=" $ ItemRequests[Index].McpId
			$" SaveSlot=" $	ItemRequests[Index].SaveSlotId
			$" ItemId=" $ ItemRequests[Index].ItemId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );
		
		ItemRequests.Remove(Index,1);
	}
}

/**
 * Sell item and remove it from the user's inventory
 * Server determines the transaction is valid
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param InstanceItemId id for the item that is being sold
 * @param Quantity number of the item to sell
 * @param StoreVersion current known version # of backend store by client
 * @param ExpectedResultItems optional list of items/quantities that the client expects to receive from the sale
 */
function SellItem(string McpId, string SaveSlotId, string InstanceItemId, int Quantity, int StoreVersion, optional const out array<McpInventoryItemContainer> ExpectedResultItems)
{
	local string Url, expectedResultsItemsJson;
	local HttpRequestInterface Request;
	local int AddAt,ExistingIndex,Index;

	// Check for pending request
	ExistingIndex = FindItemRequest(McpId,SaveSlotId,InstanceItemId,ItemRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ SellItemUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId $
				"&instanceItemId=" $ InstanceItemId $
				"&quantity=" $ Quantity $
				"&storeVersion=" $ StoreVersion;

			if (ExpectedResultItems.Length > 0)
			{
				// optional list of items/quantities provided to the server that specify what we expect to receive from the sale
				expectedResultsItemsJson = "[ ";
				for (Index = 0; Index < ExpectedResultItems.Length; Index++)
				{
					expectedResultsItemsJson $= "{";
					expectedResultsItemsJson $= "\"global_item_id\":" $ "\"" $ ExpectedResultItems[Index].GlobalItemId $ "\",";
					expectedResultsItemsJson $= "\"quantity\":" $ ExpectedResultItems[Index].Quantity;
					expectedResultsItemsJson $= "}";
					if (Index + 1 < ExpectedResultItems.Length)
					{
						expectedResultsItemsJson $= ",";
					}
				}
				expectedResultsItemsJson $= " ]";
			}
			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.SetContentAsString(expectedResultsItemsJson);
			Request.SetHeader("Content-Type","multipart/form-data");
			Request.OnProcessRequestComplete = OnSellItemRequestComplete;
			// Store off the data for reporting later
			AddAt = ItemRequests.Length;
			ItemRequests.Length = AddAt + 1;
			ItemRequests[AddAt].McpId = McpId;
			ItemRequests[AddAt].SaveSlotId = SaveSlotId;
			ItemRequests[AddAt].ItemId = InstanceItemId;
			ItemRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start SellItem web request for URL(" $ Url $ ")");
			}
			`Log("Sell item URL is " $ Url);
			`Log("Payload: " $ expectedResultsItemsJson);
		}
	}
	else
	{
		`Log("Already have a pending item request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId
			$" InstanceItemId="$ InstanceItemId);
	}
}

/**
 * Called once the request/response has completed. Used to process the sell item result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnSellItemRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index, SaveSlotIndex, UpdatedItemIdIndex, FoundItemIndex;
	local int ResponseCode;
	local string ResponseString;
	local array<string> UpdatedItemIds;

	// Search for the corresponding entry in the array
	Index = ItemRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload for the new item values
			UpdatedItemIds = ParseInventoryForSaveSlot(
				ItemRequests[Index].McpId,
				ItemRequests[Index].SaveSlotId,
				ResponseString);

			// delete durable items with quantity=0
			SaveSlotIndex = FindSaveSlotIndex(ItemRequests[Index].McpId,ItemRequests[Index].SaveSlotId);
			if (SaveSlotIndex != INDEX_NONE)
			{
				// Iterate over list of items that were updated by server
				for (UpdatedItemIdIndex=0; UpdatedItemIdIndex < UpdatedItemIds.Length; UpdatedItemIdIndex++)
				{
					FoundItemIndex = SaveSlots[SaveSlotIndex].Items.Find('InstanceItemId', UpdatedItemIds[UpdatedItemIdIndex]);
					if (FoundItemIndex != INDEX_NONE &&
						SaveSlots[SaveSlotIndex].Items[FoundItemIndex].Quantity == 0 &&
						SaveSlots[SaveSlotIndex].Items[FoundItemIndex].QuantityIAP == 0)
					{
						SaveSlots[SaveSlotIndex].Items.Remove(FoundItemIndex,1);
					}
				}
			}
		}
		// Notify anyone waiting on this
		OnSellItemComplete(
			ItemRequests[Index].McpId,
			ItemRequests[Index].SaveSlotId,
			ItemRequests[Index].ItemId,
			UpdatedItemIds,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Sell item:"
			$" McpId=" $ ItemRequests[Index].McpId
			$" SaveSlot=" $	ItemRequests[Index].SaveSlotId
			$" ItemId=" $ ItemRequests[Index].ItemId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );
		
		ItemRequests.Remove(Index,1);
	}
}

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
function EarnItem(string McpId, string SaveSlotId, string GlobalItemId, int Quantity, int StoreVersion)
{
	local string Url;
	local HttpRequestInterface Request;
	local int AddAt,ExistingIndex;

	// Check for pending request
	ExistingIndex = FindItemRequest(McpId,SaveSlotId,GlobalItemId,ItemRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ EarnItemUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId $
				"&globalItemId=" $ GlobalItemId $
				"&quantity=" $ Quantity $
				"&storeVersion=" $ StoreVersion;

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.OnProcessRequestComplete = OnEarnItemRequestComplete;
			// Store off the data for reporting later
			AddAt = ItemRequests.Length;
			ItemRequests.Length = AddAt + 1;
			ItemRequests[AddAt].McpId = McpId;
			ItemRequests[AddAt].SaveSlotId = SaveSlotId;
			ItemRequests[AddAt].ItemId = GlobalItemId;
			ItemRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start EarnItem web request for URL(" $ Url $ ")");
			}
			`Log("Earn item URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending item request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId
			$" GlobalItemId="$ GlobalItemId);
	}
}

/**
 * Called once the request/response has completed. Used to process the earn item result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnEarnItemRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local string ResponseString;
	local array<string> UpdatedItemIds;

	// Search for the corresponding entry in the array
	Index = ItemRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload for the new item values
			UpdatedItemIds = ParseInventoryForSaveSlot(
				ItemRequests[Index].McpId,
				ItemRequests[Index].SaveSlotId,
				ResponseString);
		}
		// Notify anyone waiting on this
		OnEarnItemComplete(
			ItemRequests[Index].McpId,
			ItemRequests[Index].SaveSlotId,
			ItemRequests[Index].ItemId,
			UpdatedItemIds,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Earn item:"
			$" McpId=" $ ItemRequests[Index].McpId
			$" SaveSlot=" $	ItemRequests[Index].SaveSlotId
			$" ItemId=" $ ItemRequests[Index].ItemId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );
		
		ItemRequests.Remove(Index,1);
	}
}

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
function ConsumeItem(string McpId, string SaveSlotId, string InstanceItemId, int Quantity, int StoreVersion)
{
	local string Url;
	local HttpRequestInterface Request;
	local int AddAt,ExistingIndex;

	// Check for pending request
	ExistingIndex = FindItemRequest(McpId,SaveSlotId,InstanceItemId,ItemRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ ConsumeItemUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId $
				"&instanceItemId=" $ InstanceItemId $
				"&quantity=" $ Quantity $
				"&storeVersion=" $ StoreVersion;

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.OnProcessRequestComplete = OnConsumeItemRequestComplete;
			// Store off the data for reporting later
			AddAt = ItemRequests.Length;
			ItemRequests.Length = AddAt + 1;
			ItemRequests[AddAt].McpId = McpId;
			ItemRequests[AddAt].SaveSlotId = SaveSlotId;
			ItemRequests[AddAt].ItemId = InstanceItemId;
			ItemRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start ConsumeItem web request for URL(" $ Url $ ")");
			}
			`Log("Consume item URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending item request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId
			$" InstanceItemId="$ InstanceItemId);
	}
}

/**
 * Called once the request/response has completed. Used to process the consume item result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnConsumeItemRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local string ResponseString;
	local array<string> UpdatedItemIds;

	// Search for the corresponding entry in the array
	Index = ItemRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload for the new item values
			UpdatedItemIds = ParseInventoryForSaveSlot(
				ItemRequests[Index].McpId,
				ItemRequests[Index].SaveSlotId,
				ResponseString);
		}
		// Notify anyone waiting on this
		OnConsumeItemComplete(
			ItemRequests[Index].McpId,
			ItemRequests[Index].SaveSlotId,
			ItemRequests[Index].ItemId,
			UpdatedItemIds,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Consume item:"
			$" McpId=" $ ItemRequests[Index].McpId
			$" SaveSlot=" $	ItemRequests[Index].SaveSlotId
			$" ItemId=" $ ItemRequests[Index].ItemId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );
		
		ItemRequests.Remove(Index,1);
	}
}

/**
 * Delete item and remove it from the user's inventory
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item is in
 * @param InstanceItemId id for the item that is being deleted
 * @param StoreVersion current known version # of backend store by client
 */
function DeleteItem(string McpId, string SaveSlotId, string InstanceItemId, int StoreVersion)
{
	local string Url;
	local HttpRequestInterface Request;
	local int AddAt,ExistingIndex;

	// Check for pending request
	ExistingIndex = FindItemRequest(McpId,SaveSlotId,InstanceItemId,ItemRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ DeleteItemUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId $
				"&instanceItemId=" $ InstanceItemId $
				"&storeVersion=" $ StoreVersion;

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("DELETE");
			Request.OnProcessRequestComplete = OnDeleteItemRequestComplete;
			// Store off the data for reporting later
			AddAt = ItemRequests.Length;
			ItemRequests.Length = AddAt + 1;
			ItemRequests[AddAt].McpId = McpId;
			ItemRequests[AddAt].SaveSlotId = SaveSlotId;
			ItemRequests[AddAt].ItemId = InstanceItemId;
			ItemRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start DeleteItem web request for URL(" $ Url $ ")");
			}
			`Log("Delete item URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending item request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId
			$" InstanceItemId="$ InstanceItemId);
	}
}

/**
 * Called once the request/response has completed. Used to process the delete item result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnDeleteItemRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index,SaveSlotIndex,ItemIndex;
	local int ResponseCode;

	// Search for the corresponding entry in the array
	Index = ItemRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			// Clear out the item entry if it exists
			SaveSlotIndex = FindSaveSlotIndex(
				ItemRequests[Index].McpId, 
				ItemRequests[Index].SaveSlotId);
			if (SaveSlotIndex != INDEX_NONE)
			{
				ItemIndex = SaveSlots[SaveSlotIndex].Items.Find('InstanceItemId', ItemRequests[Index].ItemId);
				if (ItemIndex != INDEX_NONE)
				{
					SaveSlots[SaveSlotIndex].Items.Remove(ItemIndex,1);
				}
			}
		}
		// Notify anyone waiting on this
		OnDeleteItemComplete(
			ItemRequests[Index].McpId,
			ItemRequests[Index].SaveSlotId,
			ItemRequests[Index].ItemId,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Delete item:"
			$" McpId=" $ ItemRequests[Index].McpId
			$" SaveSlot=" $	ItemRequests[Index].SaveSlotId
			$" ItemId=" $ ItemRequests[Index].ItemId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );
		
		ItemRequests.Remove(Index,1);
	}
}

/**
 * Record an IAP (In App Purchase) by sending receipt to server for validation
 * Results in list of items being added to inventory if successful
 * 
 * @param McpId the id of the user that made the request
 * @param SaveSlotId the save slot that the item(s) will be placed in once validated
 * @param Receipt IAP receipt to validate the purchase on the server
 */
function RecordIap(string McpId, string SaveSlotId, string Receipt)
{
	local string Url;
	local HttpRequestInterface Request;
	local int AddAt,ExistingIndex;

	// Check for pending request
	ExistingIndex = FindSaveSlotRequest(McpId,SaveSlotId,SaveSlotRequests);
	if (ExistingIndex == INDEX_NONE)
	{
		Request = class'HttpFactory'.static.CreateRequest();
		if (Request != None)
		{
			Url = GetBaseURL() $ IapRecordUrl $ GetAppAccessURL() $
				GetUserAuthURL(McpId) $
				"&uniqueUserId=" $ McpId $
				"&saveSlotId=" $ SaveSlotId;

			// Build our web request with the above URL
			Request.SetURL(Url);
			Request.SetVerb("POST");
			Request.OnProcessRequestComplete = OnRecordIapRequestComplete;
			Request.SetContentAsString(Receipt);
			Request.SetHeader("Content-Type","multipart/form-data");

			// Store off the data for reporting later
			AddAt = SaveSlotRequests.Length;
			SaveSlotRequests.Length = AddAt + 1;
			SaveSlotRequests[AddAt].McpId = McpId;
			SaveSlotRequests[AddAt].SaveSlotId = SaveSlotId;
			SaveSlotRequests[AddAt].Request = Request;

			// Now kick off the request
			if (!Request.ProcessRequest())
			{
				`Log("Failed to start RecordIap web request for URL(" $ Url $ ")");
			}
			`Log("Iap record URL is " $ Url);
		}
	}
	else
	{
		`Log("Already have a pending record IAP request for"
			$" McpId="$ McpId
			$" SaveSlotId="$ SaveSlotId);
	}
}

/**
 * Called once the request/response has completed. Used to process the record iap result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnRecordIapRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local string ResponseString;
	local array<string> UpdatedItemIds;

	// Search for the corresponding entry in the array
	Index = SaveSlotRequests.Find('Request', Request);
	if (Index != INDEX_NONE)
	{
		ResponseCode = `HTTP_STATUS_SERVER_ERROR;
		if (Response != None)
		{
			ResponseCode = Response.GetResponseCode();
		}
		// Both of these need to be true for the request to be a success
		bWasSuccessful = bWasSuccessful && ResponseCode == `HTTP_STATUS_OK;
		if (bWasSuccessful)
		{
			ResponseString = Response.GetContentAsString();

			// Parse the JSON payload for the new item values
			UpdatedItemIds = ParseInventoryForSaveSlot(
				SaveSlotRequests[Index].McpId,
				SaveSlotRequests[Index].SaveSlotId,
				ResponseString);
			
		}
		// Notify anyone waiting on this
		OnRecordIapComplete(
			SaveSlotRequests[Index].McpId,
			SaveSlotRequests[Index].SaveSlotId,
			UpdatedItemIds,
			bWasSuccessful,
			Response.GetContentAsString());

		`Log("Iap record:"
			$" McpId=" $ SaveSlotRequests[Index].McpId
			$" SaveSlot=" $	SaveSlotRequests[Index].SaveSlotId
			$" Successful=" $ bWasSuccessful
			$" ResponseCode=" $ ResponseCode );

		SaveSlotRequests.Remove(Index,1);
	}
}
