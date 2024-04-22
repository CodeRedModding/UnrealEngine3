/**
 * Provides a general purpose global storage area for game or configuration data.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_Registry extends UIDataStore
	native(inherit);

cpptext
{
	/* === UIDataStore interface === */
	/**
	 * Creates the data provider for this registry data store.
	 */
	virtual void InitializeDataStore();
}

struct native RegistryKeyValuePair
{
	var string Key;
	var string Value;
};


/**
 * The data fields which have been added to this data store.
 */
var	array<RegistryKeyValuePair> RegistryData;

/**
 * Get data from the RegistryData array
 * @param Key - The key to get data for
 * @param out_Data - the data that was recovered
 *
 * @return True if there was data to recover, false if there was no data for the specified key
 */
event bool GetData(string Key, out string out_Data)
{
	local int i;

	for (i = 0; i < RegistryData.length; i++)
	{
		if (RegistryData[i].Key == Key)
		{
			out_Data = RegistryData[i].Value;
			return true;
		}
	}
	return false;
}

/**
 * Set data to the RegistryData array
 * @param Key - The key to set data for
 * @param Value - the data that is to be stored
 */
event SetData(string Key, string Value)
{
	local int i;
	local RegistryKeyValuePair KVP;

	for (i = 0; i < RegistryData.length; i++)
	{
		if (RegistryData[i].Key == Key)
		{
			RegistryData[i].Value = Value;
			return;
		}
	}
	
	KVP.Key = Key;
	KVP.Value = Value;

	RegistryData.AddItem(KVP);
}


DefaultProperties
{
	Tag=Registry
}


