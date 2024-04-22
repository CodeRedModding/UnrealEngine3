/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * A Cloud Save System KVS that utilizes the CloudStorageBase implementation.
 */
 class CloudStorageBaseCloudSaveSystemKVS extends Object
	 implements(CloudSaveSystemKVSInterface)
	 dependson(CloudStorageBase);

 /*Instance of the cloud storage that this Save System KVS will utilize*/
 var transient private CloudStorageBase CloudStorage;
 
/*
*Initializes the KVS system for use.
*
*@param InCloudStorage the instance of ClousStorageBase to use for KVS.
*@param List of all keys that will be utilized with this
*/
final function Init(CloudStorageBase InCloudStorage)
{
	CloudStorage = InCloudStorage;
}

/*
*Reads a key value for the given save slot
*/
function bool ReadKeyValue(int SaveSlotIndex, string KeyName, EPlatformInterfaceDataType Type, out PlatformInterfaceDelegateResult Value)
{
	local string SaveSlotKeyName;

	if (CloudStorage == None)
	{
		return false;
	}

	SaveSlotKeyName = GenerateKeyNameForSaveSlot(SaveSlotIndex, KeyName);

	return CloudStorage.ReadKeyValue(SaveSlotKeyName, Type, Value);
}

/*
*Writes a key value for the given slot
*/
function bool WriteKeyValue(int SaveSlotIndex, string KeyName, const out PlatformInterfaceData Value)
{
	local string SaveSlotKeyName;

	if (CloudStorage == None)
	{
		return false;
	}

	SaveSlotKeyName = GenerateKeyNameForSaveSlot(SaveSlotIndex, KeyName);

	return CloudStorage.WriteKeyValue(SaveSlotKeyName, Value);
}

final function private string GenerateKeyNameForSaveSlot(int SaveSlotIndex, string KeyName)
{
	return SaveSlotIndex$KeyName;
}