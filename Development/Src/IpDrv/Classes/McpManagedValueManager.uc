/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the concrete implementation
 */
class McpManagedValueManager extends McpManagedValueManagerBase;

/** The URL to use when making save slot */
var config String CreateSaveSlotUrl;

/** The URL to use when reading save slot */
var config String ReadSaveSlotUrl;

/** The URL to use when updating a value */
var config String UpdateValueUrl;

/** The URL to use when deleting a value */
var config String DeleteValueUrl;

/** The list of all save slots that are being managed */
var array<ManagedValueSaveSlot> SaveSlots;

/** Holds the state information for an outstanding save slot request */
struct SaveSlotRequestState
{
	/** The MCP id that was returned by the backend */
	var string McpId;
	/** The save slot involved */
	var string SaveSlot;
	/** The request object for this request */
	var HttpRequestInterface Request;
};

/** Holds the state information used by requests that act on a value id */
struct ValueRequestState extends SaveSlotRequestState
{
	var Name ValueId;
};

/** The set of create save slot requests that are pending */
var array<SaveSlotRequestState> CreateSaveSlotRequests;

/** The set of read save slot requests that are pending */
var array<SaveSlotRequestState> ReadSaveSlotRequests;

/** The set of update value requests that are pending */
var array<ValueRequestState> UpdateValueRequests;

/** The set of update value requests that are pending */
var array<ValueRequestState> DeleteValueRequests;

/**
 * Creates the user's specified save slot
 * 
 * @param McpId the id of the user that requested the create
 * @param SaveSlot the save slot that is being create
 */
function CreateSaveSlot(String McpId, String SaveSlot)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ CreateSaveSlotUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ McpId $
			"&saveSlotId=" $ SaveSlot;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("POST");
		Request.OnProcessRequestComplete = OnCreateSaveSlotRequestComplete;
		// Store off the data for reporting later
		AddAt = CreateSaveSlotRequests.Length;
		CreateSaveSlotRequests.Length = AddAt + 1;
		CreateSaveSlotRequests[AddAt].McpId = McpId;
		CreateSaveSlotRequests[AddAt].SaveSlot = SaveSlot;
		CreateSaveSlotRequests[AddAt].Request = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start CreateSaveSlot web request for URL(" $ Url $ ")");
		}
		`Log("Create save slot URL is " $ Url);
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
	local int Index;
	local int ResponseCode;
	local string ResponseString;

	// Search for the corresponding entry in the array
	Index = CreateSaveSlotRequests.Find('Request', Request);
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
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload of default values for this save slot
			ParseValuesForSaveSlot(CreateSaveSlotRequests[Index].McpId,
				CreateSaveSlotRequests[Index].SaveSlot,
				ResponseString);
		}
		// Notify anyone waiting on this
		OnCreateSaveSlotComplete(CreateSaveSlotRequests[Index].McpId,
			CreateSaveSlotRequests[Index].SaveSlot,
			bWasSuccessful,
			Response.GetContentAsString());
		`Log("Create save slot McpId(" $ CreateSaveSlotRequests[Index].McpId $ "), SaveSlot(" $
			CreateSaveSlotRequests[Index].SaveSlot $ ") was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		CreateSaveSlotRequests.Remove(Index,1);
	}
}

/**
 * Searches the stored save slots for the ones for this user
 * 
 * @param McpId the user being searched for
 * @param SaveSlot the save slot being searched for
 * 
 * @return the index of the save slot if found
 */
function int FindSaveSlotIndex(String McpId, String SaveSlot)
{
	local int SaveSlotIndex;

	// Search all of the save slots for one that matches the user and slot id
	for (SaveSlotIndex = 0; SaveSlotIndex < SaveSlots.Length; SaveSlotIndex++)
	{
		if (SaveSlots[SaveSlotIndex].OwningMcpId == McpId &&
			SaveSlots[SaveSlotIndex].SaveSlot == SaveSlot)
		{
			return SaveSlotIndex;
		}
	}
	return INDEX_NONE;
}

/**
 * Parses the json of managed values for a user's save slot
 * 
 * @param McpId the user that owns the data
 * @param SaveSlot the save slot where the data is stored
 * @param JsonPayload the json data to parse
 */
function ParseValuesForSaveSlot(String McpId, String SaveSlot, String JsonPayload)
{
	local JsonObject ParsedJson;
	local int JsonIndex;
	local int SaveSlotIndex;
	local int ManagedValueIndex;
	local name ValueId;
	local int Value;

	// Find the save slot for the user
	SaveSlotIndex = FindSaveSlotIndex(McpId, SaveSlot);
	if (SaveSlotIndex == INDEX_NONE)
	{
		// Add the save slot since it is missing
		SaveSlotIndex = SaveSlots.Length;
		SaveSlots.Length = SaveSlotIndex + 1;
		SaveSlots[SaveSlotIndex].OwningMcpId = McpId;
		SaveSlots[SaveSlotIndex].SaveSlot = SaveSlot;
	}
// @todo joeg - Replace with Wes' ImportJson() once it's implemented with proper variable naming support
	ParsedJson = class'JsonObject'.static.DecodeJson(JsonPayload);
	// Add/update each managed value to the user's save slot
	for (JsonIndex = 0; JsonIndex < ParsedJson.ObjectArray.Length; JsonIndex++)
	{
		// Grab the value and id we need to store
		ValueId = name(ParsedJson.ObjectArray[JsonIndex].GetStringValue("value_id"));
		Value = ParsedJson.ObjectArray[JsonIndex].GetIntValue("value");
		// Find the existing managed value
		ManagedValueIndex = SaveSlots[SaveSlotIndex].Values.Find('ValueId', ValueId);
		if (ManagedValueIndex == INDEX_NONE)
		{
			// Not stored yet, so add one
			ManagedValueIndex = SaveSlots[SaveSlotIndex].Values.Length;
			SaveSlots[SaveSlotIndex].Values.Length = ManagedValueIndex + 1;
			SaveSlots[SaveSlotIndex].Values[ManagedValueIndex].ValueId = ValueId;
		}
		// Store the updated value
		SaveSlots[SaveSlotIndex].Values[ManagedValueIndex].Value = Value;
	}
}

/**
 * Reads all of the values in the user's specified save slot
 * 
 * @param McpId the id of the user that requested the read
 * @param SaveSlot the save slot that is being read
 */
function ReadSaveSlot(String McpId, String SaveSlot)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ ReadSaveSlotUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ McpId $
			"&saveSlotId=" $ SaveSlot;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("GET");
		Request.OnProcessRequestComplete = OnReadSaveSlotRequestComplete;
		// Store off the data for reporting later
		AddAt = ReadSaveSlotRequests.Length;
		ReadSaveSlotRequests.Length = AddAt + 1;
		ReadSaveSlotRequests[AddAt].McpId = McpId;
		ReadSaveSlotRequests[AddAt].SaveSlot = SaveSlot;
		ReadSaveSlotRequests[AddAt].Request = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start ReadSaveSlot web request for URL(" $ Url $ ")");
		}
		`Log("Read save slot URL is " $ Url);
	}
}

/**
 * Called once the request/response has completed. Used to process the read save slot result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnReadSaveSlotRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local string ResponseString;

	// Search for the corresponding entry in the array
	Index = ReadSaveSlotRequests.Find('Request', Request);
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
			ResponseString = Response.GetContentAsString();
			// Parse the JSON payload of default values for this save slot
			ParseValuesForSaveSlot(ReadSaveSlotRequests[Index].McpId,
				ReadSaveSlotRequests[Index].SaveSlot,
				ResponseString);
		}
		// Notify anyone waiting on this
		OnReadSaveSlotComplete(ReadSaveSlotRequests[Index].McpId,
			ReadSaveSlotRequests[Index].SaveSlot,
			bWasSuccessful,
			Response.GetContentAsString());
		`Log("Read save slot McpId(" $ ReadSaveSlotRequests[Index].McpId $ "), SaveSlot(" $
			ReadSaveSlotRequests[Index].SaveSlot $ ") was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		ReadSaveSlotRequests.Remove(Index,1);
	}
}

/**
 * @return The list of values for the requested user's specified save slot
 */
function array<ManagedValue> GetValues(String McpId, String SaveSlot)
{
	local int SaveSlotIndex;
	local array<ManagedValue> EmptyArray;

	// Find the slot and then return the array of values stored there
	SaveSlotIndex = FindSaveSlotIndex(McpId, SaveSlot);
	if (SaveSlotIndex != INDEX_NONE)
	{
		return SaveSlots[SaveSlotIndex].Values;
	}
	// To avoid the script warning
	EmptyArray.Length = 0;
	return EmptyArray;
}

/**
 * @return The value the server returned for the requested value id from the user's specific save slot
 */
function int GetValue(String McpId, String SaveSlot, Name ValueId)
{
	local int SaveSlotIndex;
	local int ValueIndex;
	local int Value;

	// Find the slot first
	SaveSlotIndex = FindSaveSlotIndex(McpId, SaveSlot);
	if (SaveSlotIndex != INDEX_NONE)
	{
		// Find the requested value
		ValueIndex = SaveSlots[SaveSlotIndex].Values.Find('ValueId', ValueId);
		if (ValueIndex != INDEX_NONE)
		{
			Value = SaveSlots[SaveSlotIndex].Values[ValueIndex].Value;
		}
	}
	return Value;
}

/**
 * Updates a specific value in the user's specified save slot
 * 
 * @param McpId the id of the user that requested the update
 * @param SaveSlot the save slot that was being updated
 * @param ValueId the value that the server should update
 * @param Value the value to apply as the update (delta or absolute determined by the server)
 */
function UpdateValue(String McpId, String SaveSlot, Name ValueId, int Value)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ UpdateValueUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ McpId $
			"&saveSlotId=" $ SaveSlot $
			"&valueId=" $ ValueId $
			"&value=" $ Value;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("POST");
		Request.OnProcessRequestComplete = OnUpdateValueRequestComplete;
		// Store off the data for reporting later
		AddAt = UpdateValueRequests.Length;
		UpdateValueRequests.Length = AddAt + 1;
		UpdateValueRequests[AddAt].McpId = McpId;
		UpdateValueRequests[AddAt].SaveSlot = SaveSlot;
		UpdateValueRequests[AddAt].ValueId = ValueId;
		UpdateValueRequests[AddAt].Request = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start UpdateValue web request for URL(" $ Url $ ")");
		}
		`Log("Update value URL is " $ Url);
	}
}

/**
 * Called once the request/response has completed. Used to process the update value result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnUpdateValueRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local string ResponseString;
	local int UpdatedValue;

	// Search for the corresponding entry in the array
	Index = UpdateValueRequests.Find('Request', Request);
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
			ResponseString = "[" $ Response.GetContentAsString() $ "]";
			// Parse the JSON payload of default values for this save slot  
			ParseValuesForSaveSlot(UpdateValueRequests[Index].McpId,
				UpdateValueRequests[Index].SaveSlot,
				ResponseString);
		}
		UpdatedValue = GetValue(UpdateValueRequests[Index].McpId, UpdateValueRequests[Index].SaveSlot, UpdateValueRequests[Index].ValueId);
		// Notify anyone waiting on this
		OnUpdateValueComplete(UpdateValueRequests[Index].McpId,
			UpdateValueRequests[Index].SaveSlot,
			UpdateValueRequests[Index].ValueId,
			UpdatedValue,
			bWasSuccessful,
			Response.GetContentAsString());
		`Log("Update value McpId(" $ UpdateValueRequests[Index].McpId $ "), SaveSlot(" $
			UpdateValueRequests[Index].SaveSlot $ "), ValueId(" $
			UpdateValueRequests[Index].ValueId $ "), Value(" $ 
			UpdatedValue $ ") was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		UpdateValueRequests.Remove(Index,1);
	}
}

/**
 * Deletes a value from the user's specified save slot
 * 
 * @param McpId the id of the user that requested the delete
 * @param SaveSlot the save slot that is having the value deleted from
 * @param ValueId the value id for the server to delete
 */
function DeleteValue(String McpId, String SaveSlot, Name ValueId)
{
	local String Url;
	local HttpRequestInterface Request;
	local int AddAt;

	Request = class'HttpFactory'.static.CreateRequest();
	if (Request != none)
	{
		Url = GetBaseURL() $ DeleteValueUrl $ GetAppAccessURL() $
			"&uniqueUserId=" $ McpId $
			"&saveSlotId=" $ SaveSlot $
			"&valueId=" $ ValueId;

		// Build our web request with the above URL
		Request.SetURL(Url);
		Request.SetVerb("DELETE");
		Request.OnProcessRequestComplete = OnDeleteValueRequestComplete;
		// Store off the data for reporting later
		AddAt = DeleteValueRequests.Length;
		DeleteValueRequests.Length = AddAt + 1;
		DeleteValueRequests[AddAt].McpId = McpId;
		DeleteValueRequests[AddAt].SaveSlot = SaveSlot;
		DeleteValueRequests[AddAt].ValueId = ValueId;
		DeleteValueRequests[AddAt].Request = Request;

		// Now kick off the request
		if (!Request.ProcessRequest())
		{
			`Log("Failed to start DeleteValue web request for URL(" $ Url $ ")");
		}
		`Log("Delete value URL is " $ Url);
	}
}

/**
 * Called once the request/response has completed. Used to process the delete value result
 * and notify any registered delegate
 * 
 * @param Request the request object that was used
 * @param Response the response object that was generated
 * @param bWasSuccessful whether or not the request completed successfully
 */
function OnDeleteValueRequestComplete(HttpRequestInterface Request, HttpResponseInterface Response, bool bWasSuccessful)
{
	local int Index;
	local int ResponseCode;
	local int SaveSlotIndex;
	local int ValueIndex;

	// Search for the corresponding entry in the array
	Index = DeleteValueRequests.Find('Request', Request);
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
			// Find the slot first
			SaveSlotIndex = FindSaveSlotIndex(DeleteValueRequests[Index].McpId, DeleteValueRequests[Index].SaveSlot);
			if (SaveSlotIndex != INDEX_NONE)
			{
				// Find the requested value
				ValueIndex = SaveSlots[SaveSlotIndex].Values.Find('ValueId', DeleteValueRequests[Index].ValueId);
				if (ValueIndex != INDEX_NONE)
				{
					// Remove it from the array
					SaveSlots[SaveSlotIndex].Values.Remove(ValueIndex, 1);
				}
			}
		}
		// Notify anyone waiting on this
		OnDeleteValueComplete(DeleteValueRequests[Index].McpId,
			DeleteValueRequests[Index].SaveSlot,
			DeleteValueRequests[Index].ValueId,
			bWasSuccessful,
			Response.GetContentAsString());
		`Log("Delete value McpId(" $ DeleteValueRequests[Index].McpId $ "), SaveSlot(" $
			DeleteValueRequests[Index].SaveSlot $ "), ValueId(" $
			DeleteValueRequests[Index].ValueId $ ") was successful " $ bWasSuccessful $
			" with ResponseCode(" $ ResponseCode $ ")");
		DeleteValueRequests.Remove(Index,1);
	}
}
