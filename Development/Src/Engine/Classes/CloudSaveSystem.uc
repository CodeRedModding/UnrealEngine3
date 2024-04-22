/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * An encapsulated system which utilizes CloudStorageBase to manage save data.
 */
class CloudSaveSystem extends Object
	native;

cpptext
{
public:
	void SerializeObject(class UObject* ObjectToSerialize, FMemoryWriter& MemoryWriter, int VersionNumber);
	UObject* DeserializeObject(class UClass* ObjectClass, FMemoryReader MemoryReader, BYTE VersionSupport, int VersionNumber);
};
//==============================================================================
const NUM_SAVE_SLOTS_KEY = "NumSaveSlots";
const DATA_STORE_ID_KEY = "DataStoreID";
const SAVE_DATA_BLOB_NAME_KEY = "DataBlobName";
const SAVE_SYSTEM_VERSION_KEY = "CloudSaveSystemVersion";
const COMMON_DATA_SAVE_SLOT_INDEX = -1;

const GET_SAVE_SLOT_ERROR =-2;
const GET_SAVE_SLOT_INVALID = -1;

enum SaveDataVersionSupport
{
	SaveDataVersionSupportLessThenEqual,
	SaveDataVersionSupportEqual,
	SaveDataVersionSupportAny,
};

//==============================================================================
var private transient CloudSaveSystemKVSInterface KeyValueStore;
var private transient CloudSaveSystemDataBlobStoreInterface DataBlobStore;
//==============================================================================
struct native GetSaveDataCallbackStruct
{
	var int SlotIndex;
	var delegate<OnGetSaveDataCallback> Callback;
};
var private  transient array<GetSaveDataCallbackStruct> OnGetSaveDataCallbacks;

struct native SetSaveDataCallbackStruct
{
	var int SlotIndex;
	var delegate<SaveSystemCallback> Callback;
};
var private transient array<SetSaveDataCallbackStruct> OnSetSaveDataCallbacks;

var private transient delegate<SaveSystemCallback> DeleteSaveDataCallback;
var private transient int ActiveSlotForDelete;

enum SaveSlotOperationEnum
{
	SSO_SET,
	SSO_GET,
	SSO_DELETE
};
struct native SaveSlotOperation
{
	var int SlotIndex;
	var SaveSlotOperationEnum SlotOperation;
};
var private transient array<SaveSlotOperation> ActiveSaveSlotOperations;

//==============================================================================
delegate OnGetSaveDataCallback(bool bWasSuccessful, int SaveSlot, out array<byte> DataBlob, string Error);
delegate SaveSystemCallback(bool bWasSuccessful, int SaveSlot, string Error);
//==============================================================================

//==============================================================================
//Utility Functions
//==============================================================================
//Returns the number of save slots.
final function bool GetNumberOfSaveSlots(out int NumSaveSlots)
{
	local PlatformInterfaceDelegateResult KVRes;
	local bool RValue;

	RValue = false;
	if (KeyValueStore != None && DataBlobStore != None)
	{
		//NumSaveSlots is slot agnostic so save to slot 0 for default name generation
		KeyValueStore.ReadKeyValue(COMMON_DATA_SAVE_SLOT_INDEX, NUM_SAVE_SLOTS_KEY, PIDT_Int, KVRes);
		if (KVRes.bSuccessful)
		{
			RValue = true;
			NumSaveSlots = KVRes.Data.IntValue;
		}
	}

	return RValue;
}

//Returns -1 on no match, slot index on match and -2 on error.
final function private int DoesSaveSlotKeyValueDataAlreadyExist(string DataStoreID, string DataBlobName)
{
	local int SaveSlotScan;
	local int NumSaveSlots;
	local string CompareDataStoreID;
	local string CompareDataBlobName;
	local int RValue;

	RValue = GET_SAVE_SLOT_INVALID;

	if (KeyValueStore != None && DataBlobStore != None && GetNumberOfSaveSlots(NumSaveSlots))
	{
		for (SaveSlotScan = 0; SaveSlotScan < NumSaveSlots; ++SaveSlotScan)
		{
			if (GetDataStoreIDAndBlobNameForSaveSlot(SaveSlotScan, CompareDataStoreID, CompareDataBlobName))
			{
				if (DataStoreID == CompareDataStoreID && DataBlobName == CompareDataBlobName)
				{
					RValue = SaveSlotScan;
					break;
				}
			}
			else
			{
				//Set error code
				RValue = GET_SAVE_SLOT_ERROR;
			}
		}

	}
	else
	{
		//Set error code
		RValue = GET_SAVE_SLOT_ERROR;
	}

	return RValue;	
}

final private function bool WriteNumSaveSlots(int NumSaveSlots)
{
	local PlatformInterfaceData KVSet;

	KVSet.IntValue = NumSaveSlots;
	KVSet.Type = PIDT_Int;
	if (KeyValueStore.WriteKeyValue(COMMON_DATA_SAVE_SLOT_INDEX, NUM_SAVE_SLOTS_KEY, KVSet))
	{
		return true;
	}

	return false;
}

final function bool IsOperationActiveForSlot(int SlotIndex)
{
	local int Index;
	Index = ActiveSaveSlotOperations.Find('SlotIndex', SlotIndex);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	else
	{
		return true;
	}
}

final function bool IsDeleteOperationActive()
{
	if (ActiveSlotForDelete == -1)
	{
		return false;
	}
	else
	{
		return true;
	}
}

final function bool AreAnySlotOperationsActive()
{
	if (ActiveSaveSlotOperations.Length > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

//==============================================================================
//Native Functions
//==============================================================================
native final function SerializeObject(Object ObjectToSerialize, out array<byte> Data, int DataVersion);
//Will return none if the version of the data isn't supported
native final function Object DeserializeObject(class ObjectClass, out array<byte> Data, SaveDataVersionSupport VersionSupport, int DataVersion);

//==============================================================================
//Init Functions
//==============================================================================

//Initializes the CloudSaveSystem into a usable state
final function Init(CloudSaveSystemKVSInterface InKeyValueStore, CloudSaveSystemDataBlobStoreInterface InDataBlobStore, int VersionNumber)
{
	local PlatformInterfaceDelegateResult SaveSystemVersionNumber;
	local PlatformInterfaceData SaveSystemVersionNumberSet;

	KeyValueStore = InKeyValueStore;
	DataBlobStore = InDataBlobStore;

	GetKeyValue(COMMON_DATA_SAVE_SLOT_INDEX, SAVE_SYSTEM_VERSION_KEY, PIDT_Int,  SaveSystemVersionNumber);
	if (!SaveSystemVersionNumber.bSuccessful || SaveSystemVersionNumber.Data.IntValue != VersionNumber)
	{
		SaveSystemVersionNumberSet.IntValue = VersionNumber;
		SaveSystemVersionNumberSet.Type = PIDT_Int;
		SetKeyValue(COMMON_DATA_SAVE_SLOT_INDEX, SAVE_SYSTEM_VERSION_KEY, SaveSystemVersionNumberSet);

		WriteNumSaveSlots(0);
	}
}

//==============================================================================
//Data Retrieval
//==============================================================================
//Starts an async request that retrieves the save data blob for the given save slot. 
//Save slot indexing is zero based.
final function GetSaveData(int SaveSlot, delegate<OnGetSaveDataCallback> OnGetSaveDataCallback)
{
	local string DataStoreID;
	local GetSaveDataCallbackStruct CallbackStruct;
	local SaveSlotOperation SlotOperation;
	local array<byte> EmptyBuffer;
	local string BlobName;
	local bool ErrorOccured;
	local string Error;

	ErrorOccured = true;

	if (KeyValueStore == None)
	{
		Error="GetSaveData::KeyValueStore instance cannot be None";
	}
	else if (DataBlobStore == None)
	{
		Error="GetSaveData::DataBlobStore instance cannot be None";
	}
	else if (OnGetSaveDataCallbacks.Find('SlotIndex', SaveSlot) != INDEX_NONE)
	{
		Error="GetSaveData::OnGetSaveDataCallback already present for save slot";
	}
	else if (ActiveSaveSlotOperations.Find('SlotIndex', SaveSlot) != INDEX_NONE)
	{
		Error="GetSaveData::Save System operation already active for save slot";
	}
	else if (IsDeleteOperationActive())
	{
		Error="GetSaveData::Delete Operation active cannot GetSaveData";
	}
	else if (!GetDataStoreIDAndBlobNameForSaveSlot(SaveSlot, DataStoreID, BlobName))
	{
		Error="GetSaveData::Failed to get store id and data blob name for save slot";
	}
	else
	{
		CallbackStruct.SlotIndex = SaveSlot;
		CallbackStruct.Callback = OnGetSaveDataCallback;
		OnGetSaveDataCallbacks.AddItem(CallbackStruct);

		SlotOperation.SlotIndex = SaveSlot;
		SlotOperation.SlotOperation = SSO_GET;
		ActiveSaveSlotOperations.AddItem(SlotOperation);

		DataBlobStore.GetDataBlob(DataStoreID, BlobName, OnGetSaveDataComplete);

		ErrorOccured = false;
	}

	
	if (ErrorOccured && OnGetSaveDataCallback != None)
	{
		OnGetSaveDataCallback(false/*bWasSuccessful*/, SaveSlot, EmptyBuffer, Error);
	}
}

final function private OnGetSaveDataComplete(bool bWasSuccessful, string StorageID, string BlobName, out array<byte> DataBlob, string Error)
{
	local int SaveSlotIndex;
	local int Index;
	local delegate<OnGetSaveDataCallback> LocalCallback;

	SaveSlotIndex = DoesSaveSlotKeyValueDataAlreadyExist(StorageID, BlobName);
	if (SaveSlotIndex >= 0)
	{

		//----------------------------------------------------------------------
		Index = ActiveSaveSlotOperations.Find('SlotIndex', SaveSlotIndex);
		if (Index != INDEX_NONE)
		{
			if(ActiveSaveSlotOperations[Index].SlotOperation != SSO_GET)
			{
				bWasSuccessful = false;
				Error = "CloudSaveSystem in corrupt stat GetSaveData request finished but active slot operation should have been"@ActiveSaveSlotOperations[Index].SlotOperation;
			}
			ActiveSaveSlotOperations.Remove(Index, 1);
		}
		else
		{
			bWasSuccessful = false;
			Error = "CloudSaveSystem in corrupt state GetData request finished but was not correctly internally tracked.";
		}

		Index = OnGetSaveDataCallbacks.Find('SlotIndex', SaveSlotIndex);
		if (Index != INDEX_NONE)
		{
			LocalCallback = OnGetSaveDataCallbacks[Index].Callback;
			LocalCallback(bWasSuccessful, SaveSlotIndex, DataBlob, "Unknown Error loading data blob from Cloud");
			OnGetSaveDataCallbacks.Remove(Index, 1);
		}
	}
	else
	{
		`warn("CloudSaveSystem in corrupt sate. save slot index does not exist for ID:"@StorageID@"BlobName:"@BlobName);
	}
}

//==============================================================================
//Data Saving
//==============================================================================
//Sets the save data blob for the given save slot. Save slot indexing is zero based.
final function SetSaveData(int SaveSlot, delegate<SaveSystemCallback> InSetSaveDataCallback, const out array<byte> SaveDataBlob)
{
	local SetSaveDataCallbackStruct CallbackStruct;
	local SaveSlotOperation SlotOperation;
	local string DataStoreID;
	local string BlobName;
	local bool ErrorOccured;
	local string Error;


	ErrorOccured = true;
	if (KeyValueStore == None)
	{
		Error="SetSaveData::KeyValueStore instance cannot be None";
	}
	else if (DataBlobStore == None)
	{
		Error="SetSaveData::DataBlobStore instance cannot be None";
	}
	else if (OnSetSaveDataCallbacks.Find('SlotIndex', SaveSlot) != INDEX_NONE)
	{
		Error="SetSaveData::OnSetSaveDataCallback already present for save slot";
	}
	else if (ActiveSaveSlotOperations.Find('SlotIndex', SaveSlot) != INDEX_NONE)
	{
		Error="SetSaveData::Save System operation already active for save slot";
	}
	else if (IsDeleteOperationActive())
	{
		Error="SetSaveData::Delete Operation active cannot SetSaveData";
	}
	else if (!GetDataStoreIDAndBlobNameForSaveSlot(SaveSlot, DataStoreID, BlobName))
	{
		Error="SetSaveData::Failed to get store id and data blob name for save slot";
	}
	else
	{	
		CallbackStruct.SlotIndex = SaveSlot;
		CallbackStruct.Callback = InSetSaveDataCallback;
		OnSetSaveDataCallbacks.AddItem(CallbackStruct);

		SlotOperation.SlotIndex = SaveSlot;
		SlotOperation.SlotOperation = SSO_SET;
		ActiveSaveSlotOperations.AddItem(SlotOperation);

		ErrorOccured = false;

		DataBlobStore.SetDataBlob(DataStoreID, BlobName, SaveDataBlob, OnSetSaveDataComplete);
	}

	if (ErrorOccured && InSetSaveDataCallback != None)
	{
		InSetSaveDataCallback(false/*bWasSuccesfful*/, SaveSlot, Error);
	}	
}

final function private OnSetSaveDataComplete(bool bWasSucessfull, string StorageID, string BlobName, string Error)
{
	local int SaveSlotIndex;
	local int Index;
	local delegate<SaveSystemCallback> LocalCallback;

	SaveSlotIndex = DoesSaveSlotKeyValueDataAlreadyExist(StorageID, BlobName);
	if (SaveSlotIndex >= 0)
	{
		//----------------------------------------------------------------------
		Index = ActiveSaveSlotOperations.Find('SlotIndex', SaveSlotIndex);
		if (Index != INDEX_NONE)
		{
			if(ActiveSaveSlotOperations[Index].SlotOperation != SSO_SET)
			{
				Error = "CloudSaveSystem in corrupt stat Set Data request finished but active slot operation should have been"@ActiveSaveSlotOperations[Index].SlotOperation;
				bWasSucessfull = false;
			}
			ActiveSaveSlotOperations.Remove(Index, 1);
		}
		else
		{
			bWasSucessfull = false;
			Error = "CloudSaveSystem in corrupt state Set Data request finished but was not correctly internally tracked.";
		}

		//----------------------------------------------------------------------
		Index = OnSetSaveDataCallbacks.Find('SlotIndex', SaveSlotIndex);
		if (Index != INDEX_NONE)
		{
			LocalCallback = OnSetSaveDataCallbacks[Index].Callback;
			LocalCallback(bWasSucessfull, SaveSlotIndex, Error);
			OnSetSaveDataCallbacks.Remove(Index, 1);
		}
	}
	else
	{
		`warn("CloudSaveSystem in corrupt sate. Save slot index does not exist for ID:"@StorageID@"BlobName:"@BlobName);
	}
}

//==============================================================================
//Data Deletion
//==============================================================================
final function bool DeleteSaveData(int SaveSlot, delegate<SaveSystemCallback> InDeleteSaveDataCallback )
{
	local string DataStoreID;
	local string BlobName;
	local SaveSlotOperation SlotOperation;
	local bool RValue;

	RValue = false;
	if (KeyValueStore != None && DataBlobStore != None 
		&& DeleteSaveDataCallback == None 
		&& !AreAnySlotOperationsActive()
		&& !IsDeleteOperationActive())
	{
		if(GetDataStoreIDAndBlobNameForSaveSlot(SaveSlot, DataStoreID, BlobName))
		{
			RValue = DataBlobStore.DeleteDataBlob(DataStoreID, BlobName,OnDeleteSaveDataComplete);
			if (RValue)
			{
				DeleteSaveDataCallback = InDeleteSaveDataCallback;
				ActiveSlotForDelete = SaveSlot;

				SlotOperation.SlotIndex = SaveSlot;
				SlotOperation.SlotOperation = SSO_DELETE;
				ActiveSaveSlotOperations.AddItem(SlotOperation);
			}
		}
	}

	return RValue;
}

final function OnDeleteSaveDataComplete(bool bWasSucessfull, string StorageID, string BlobName, string Error)
{
	local delegate<SaveSystemCallback> Callback;
	local int Scan;
	local int Index;
	local int SlotDeleted;
	local int NumSaveSlots;

	local string DataStoreID;
	local string DataBlobName;

	if (bWasSucessfull)
	{
		if(!GetNumberOfSaveSlots(NumSaveSlots))
		{
			bWasSucessfull = false;
			Error = "Could not retrieve number of save slots during slot deletion. Save system in corrupt state.";
		}
		else
		{
			for (Scan = ActiveSlotForDelete+1; Scan < NumSaveSlots && bWasSucessfull; ++Scan)
			{
				if(!GetDataStoreIDAndBlobNameForSaveSlot(Scan, DataStoreID, DataBlobName))
				{
					bWasSucessfull = false;
					Error = "Error retrieving DataStoreID and DataBlobName during slot deletion. Save system in corrupt state.";
				}
				else if (!InternalSetSaveSlotKeyValues(Scan-1, DataStoreID, DataBlobName))
				{
					bWasSucessfull = false;
					Error = "Error migrating DataStoreID and DataBlobName to new slot during slot deletion. Save system in corrupt state.";
				}
			}
			if (bWasSucessfull)
			{
				NumSaveSlots--;
				if(!WriteNumSaveSlots(NumSaveSlots))
				{
					bWasSucessfull = false;
					Error = "Error writing number of save slot to Cloud KVS";
				}
			}
		}
	}

	//--------------------------------------------------------------------------
	SlotDeleted = ActiveSlotForDelete;
	ActiveSlotForDelete = -1;

	//--------------------------------------------------------------------------
	Index = ActiveSaveSlotOperations.Find('SlotIndex', SlotDeleted);
	if (Index != INDEX_NONE)
	{
		if(ActiveSaveSlotOperations[Index].SlotOperation != SSO_DELETE)
		{
			`warn("CloudSaveSystem in corrupt stat DeleteSaveData request finished but active slot operation should have been"@ActiveSaveSlotOperations[Index].SlotOperation);
		}
		ActiveSaveSlotOperations.Remove(Index, 1);
	}
	else
	{
		`warn("CloudSaveSystem in corrupt state DeleteSaveData request finished but was not correctly internally tracked.");
	}

	//--------------------------------------------------------------------------
	if (DeleteSaveDataCallback != None)
	{
		Callback = DeleteSaveDataCallback;
		DeleteSaveDataCallback = None;
		Callback(bWasSucessfull, SlotDeleted, Error);
	}

}
//==============================================================================
//Key Value Sets
//==============================================================================
//Sets the key values for a given save slot.  If the save slot exists the data will be 
//overwritten, otherwise a new slot will be created
final function bool SetSaveSlotKeyValues(string DataStoreID, string SaveDataBlobName, out int SaveSlot)
{
	local bool RValue;
	local int NumSaveSlots;
	local bool IncSlotCount;

	RValue = false;
	IncSlotCount = false;

	if (KeyValueStore != None && DataBlobStore != None && GetNumberOfSaveSlots(NumSaveSlots))
	{
		
		SaveSlot = DoesSaveSlotKeyValueDataAlreadyExist(DataStoreID, SaveDataBlobName);
		//Error retrieving result do not change stored data
		if (SaveSlot == GET_SAVE_SLOT_ERROR)
		{
			return false;
		}
		else if (SaveSlot == GET_SAVE_SLOT_INVALID)
		{
			//No matching data found
			SaveSlot = NumSaveSlots;
			IncSlotCount = true;
		}

		if(InternalSetSaveSlotKeyValues(SaveSlot, DataStoreID, SaveDataBlobName))
		{
			if (IncSlotCount)
			{
				NumSaveSlots++;
				if (WriteNumSaveSlots(NumSaveSlots))
				{
					RValue = true;
				}
			}
			else
			{
				RValue = true;
			}
		}
	}

	return RValue;
}

final private function bool InternalSetSaveSlotKeyValues(int SaveSlot, string DataStoreID, string SaveDataBlobName)
{
	local PlatformInterfaceData KVSet;
	local bool RValue;

	RValue = false;

	KVSet.StringValue = DataStoreID;
	KVSet.Type = PIDT_String;
	if(KeyValueStore.WriteKeyValue(SaveSlot, DATA_STORE_ID_KEY, KVSet))
	{
		KVSet.StringValue = SaveDataBlobName;
		KVSet.Type = PIDT_String;
		if(KeyValueStore.WriteKeyValue(SaveSlot, SAVE_DATA_BLOB_NAME_KEY, KVSet))
		{
			RValue = true;
		}
	}

	return RValue;
}

final function bool SetKeyValue(int SaveSlot, string KeyName, const out PlatformInterfaceData Value)
{
	if (KeyValueStore == None)
	{
		return false;
	}

	return KeyValueStore.WriteKeyValue(SaveSlot, KeyName, Value);
}

//==============================================================================
//Key Value Gets
//==============================================================================
//Populates DataStoreID and DataBlobName with the data stored in the Key Value Store
final function private bool GetDataStoreIDAndBlobNameForSaveSlot(int SaveSlot, out string DataStoreID, out string DataBlobName)
{

	local PlatformInterfaceDelegateResult KVRes;
	local bool RValue;

	RValue = false;
	if (KeyValueStore != None || DataBlobStore != None)
	{
		KeyValueStore.ReadKeyValue(SaveSlot, DATA_STORE_ID_KEY, PIDT_String, KVRes);
		if (KVRes.bSuccessful)
		{
			DataStoreID = KVRes.Data.StringValue;
			KeyValueStore.ReadKeyValue(SaveSlot, SAVE_DATA_BLOB_NAME_KEY, PIDT_String, KVRes);
			if (KVRes.bSuccessful)
			{
				DataBlobName = KVRes.Data.StringValue;
				RValue = true;
			}
		}
	}

	return RValue;
}

final function bool GetKeyValue(int SaveSlot, string KeyName, EPlatformInterfaceDataType Type, out PlatformInterfaceDelegateResult Value)
{
	if (KeyValueStore == None)
	{
		return false;
	}

	return KeyValueStore.ReadKeyValue(SaveSlot, KeyName, Type, Value);
}

DefaultProperties
{
	ActiveSlotForDelete = -1
}