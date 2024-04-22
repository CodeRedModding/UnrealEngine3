/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UOnlinePlayerStorage);

/**
* Searches the profile setting array for the matching string setting name and returns the id
*
* @param ProfileSettingName the name of the profile setting being searched for
* @param ProfileSettingId the id of the context that matches the name
*
* @return true if the seting was found, false otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingId(FName ProfileSettingName,
												  INT& ProfileSettingId)
{
	// Search for the profile setting name in the mappings and return its id
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Name == ProfileSettingName)
		{
			ProfileSettingId = MetaData.Id;
			return TRUE;
		}
	}
	return FALSE;
}

/**
* Finds the human readable name for the profile setting
*
* @param ProfileSettingId the id to look up in the mappings table
*
* @return the name of the string setting that matches the id or NAME_None if not found
*/
FName UOnlinePlayerStorage::GetProfileSettingName(INT ProfileSettingId)
{
	// Search for the string setting id in the mappings and return its name
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		// If this is the ID we are looking for
		if (MetaData.Id == ProfileSettingId)
		{
			return MetaData.Name;
		}
	}
	return NAME_None;
}

/**
* Finds the localized column header text for the profile setting
*
* @param ProfileSettingId the id to look up in the mappings table
*
* @return the string to use as the list column header for the profile setting that matches the id, or an empty string if not found.
*/
FString UOnlinePlayerStorage::GetProfileSettingColumnHeader( INT ProfileSettingId )
{
	FString Result;
	// Search for the string setting id in the mappings and return its localized column header
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		// If this is the ID we are looking for
		if (MetaData.Id == ProfileSettingId)
		{
			Result = MetaData.ColumnHeaderText;
			break;
		}
	}
	return Result;
}

/**
* Finds the index of an OnlineProfileSetting struct given its settings id.
*
* @param	ProfileSettingId	the id of the struct to search for
*
* @return	the index into the ProfileSettings array for the struct with the matching id.
*/
INT UOnlinePlayerStorage::FindProfileSettingIndex( INT ProfileSettingId ) const
{
	INT Result = INDEX_NONE;

	for (INT SettingsIndex = 0; SettingsIndex < ProfileSettings.Num(); SettingsIndex++)
	{
		const FOnlineProfileSetting& Setting = ProfileSettings(SettingsIndex);
		if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
		{
			Result = SettingsIndex;
			break;
		}
	}

	return Result;
}

/**
* Finds the index of SettingsPropertyPropertyMetaData struct, given its settings id. 
*
* @param	ProfileSettingId	the id of the struct to search for
*
* @return	the index into the ProfileMappings array for the struct with the matching id.
*/
INT UOnlinePlayerStorage::FindProfileMappingIndex( INT ProfileSettingId ) const
{
	INT Result = INDEX_NONE;

	// Search for the string setting id in the mappings
	for (INT MappingIndex = 0; MappingIndex < ProfileMappings.Num(); MappingIndex++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(MappingIndex);
		if (MetaData.Id == ProfileSettingId)
		{
			Result = MappingIndex;
			break;
		}
	}

	return Result;
}

/**
* Finds the index of SettingsPropertyPropertyMetaData struct, given its settings name. 
*
* @param	ProfileSettingId	the id of the struct to search for
*
* @return	the index into the ProfileMappings array for the struct with the matching name.
*/
INT UOnlinePlayerStorage::FindProfileMappingIndexByName( FName ProfileSettingName ) const
{
	INT Result = INDEX_NONE;

	// Search for the string setting id in the mappings
	for (INT MappingIndex = 0; MappingIndex < ProfileMappings.Num(); MappingIndex++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(MappingIndex);
		if (MetaData.Name == ProfileSettingName)
		{
			Result = MappingIndex;
			break;
		}
	}

	return Result;
}

/**
* Finds the default index of SettingsPropertyPropertyMetaData struct, given its settings name. 
*
* @param	ProfileSettingId	the id of the struct to search for
*
* @return	the default index into the ProfileMappings array for the struct with the matching name.
*/
INT UOnlinePlayerStorage::FindDefaultProfileMappingIndexByName( FName ProfileSettingName ) const
{
	INT Result = INDEX_NONE;

	// Search for the string setting id in the mappings
	for (INT MappingIndex = 0; MappingIndex < ProfileMappings.Num(); MappingIndex++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData =  ProfileMappings(MappingIndex);
		if (MetaData.Name == ProfileSettingName)
		{
			Result = MappingIndex;
			break;
		}
	}

	return Result;
}

/**
* Determines if the setting is id mapped or not
*
* @param ProfileSettingId the id to look up in the mappings table
*
* @return TRUE if the setting is id mapped, FALSE if it is a raw value
*/
UBOOL UOnlinePlayerStorage::IsProfileSettingIdMapped(INT ProfileSettingId)
{
	// Search for the string setting id in the mappings and return its name
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		// If this is the ID we are looking for
		if (MetaData.Id == ProfileSettingId)
		{
			return MetaData.MappingType == PVMT_IdMapped;
		}
	}
	return FALSE;
}

/**
* Finds the human readable name for a profile setting's value. Searches the
* profile settings mappings for the specifc profile setting and then searches
* the set of values for the specific value index and returns that value's
* human readable name
*
* @param Value the out param that gets the value copied to it
* @param ValueMapID optional parameter that allows you to select a specific index in the ValueMappings instead
* of automatically using the currently set index (if -1 is passed in, which is the default, it means to just
* use the set index
*
* @return true if found, false otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingValue(INT ProfileSettingId,FString& Value,INT ValueMapID)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the profile setting that matches this id
			for (INT Index2 = 0; Index2 < ProfileSettings.Num(); Index2++)
			{
				const FOnlineProfileSetting& Setting = ProfileSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is ID mapped, then find the ID
					if (MetaData.MappingType == PVMT_IdMapped)
					{
						INT ValueIndex;
						// Use ValueMapID if one was passed in, else use the current value in the setting
						if ( ValueMapID >= 0 )
						{
							ValueIndex = ValueMapID;
						}
						else
						{
							// Read the index so we can find its name
							Setting.ProfileSetting.Data.GetData(ValueIndex);
						}
						// Now search for the value index mapping
						for (INT Index3 = 0; Index3 < MetaData.ValueMappings.Num(); Index3++)
						{
							const FIdToStringMapping& Mapping = MetaData.ValueMappings(Index3);
							if (Mapping.Id == ValueIndex)
							{
								Value = Mapping.Name.ToString();
								return TRUE;
							}
						}
					}
					else
					{
						// This item should be treated as a string
						Value = Setting.ProfileSetting.Data.ToString();
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**
* Finds the human readable name for a profile setting's value. Searches the
* profile settings mappings for the specifc profile setting and then searches
* the set of values for the specific value index and returns that value's
* human readable name
*
* @param ProfileSettingId the id to find the value's name
*
* @return the name of the value or NAME_None if not value mapped
*/
FName UOnlinePlayerStorage::GetProfileSettingValueName(INT ProfileSettingId)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the profile setting that matches this id
			for (INT Index2 = 0; Index2 < ProfileSettings.Num(); Index2++)
			{
				const FOnlineProfileSetting& Setting = ProfileSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is ID mapped, then find the ID
					if (MetaData.MappingType == PVMT_IdMapped)
					{
						INT ValueIndex;
						// Read the index so we can find its name
						Setting.ProfileSetting.Data.GetData(ValueIndex);
						// Now search for the value index mapping
						for (INT Index3 = 0; Index3 < MetaData.ValueMappings.Num(); Index3++)
						{
							const FIdToStringMapping& Mapping = MetaData.ValueMappings(Index3);
							if (Mapping.Id == ValueIndex)
							{
								return Mapping.Name;
							}
						}
					}
					else
					{
						return NAME_None;
					}
				}
			}
		}
	}
	return NAME_None;
}

/**
* Searches the profile settings mappings for the specifc profile setting and
* then adds all of the possible values to the out parameter
*
* @param ProfileSettingId the id to look up in the mappings table
* @param Values the out param that gets the list of values copied to it
*
* @return true if found and value mapped, false otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingValues(INT ProfileSettingId,TArray<FName>& Values)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// If this is ID mapped, then find the ID
			if (MetaData.MappingType == PVMT_IdMapped)
			{
				// Add each name mapped value to the array
				for (INT Index2 = 0; Index2 < MetaData.ValueMappings.Num(); Index2++)
				{
					Values.AddItem(MetaData.ValueMappings(Index2).Name);
				}
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
	}
	return FALSE;
}

/**
* Finds the human readable value for a profile setting. Searches the
* profile settings mappings for the specific profile setting and then searches
* the set of values for the specific value index and returns that value's
* human readable name
*
* @param ProfileSettingName the name of the profile setting to find the string value of
* @param Value the out param that gets the value copied to it
*
* @return true if found, false otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingValueByName(FName ProfileSettingName,
														   FString& Value)
{
	INT ProfileSettingId;
	// See if this is a known string setting
	if (GetProfileSettingId(ProfileSettingName,ProfileSettingId))
	{
		return GetProfileSettingValue(ProfileSettingId,Value);
	}
	return FALSE;
}

/**
* Searches for the profile setting by name and sets the value index to the
* value contained in the profile setting meta data
*
* @param ProfileSettingName the name of the profile setting to find
* @param NewValue the string value to use
*
* @return true if the profile setting was found and the value was set, false otherwise
*/
UBOOL UOnlinePlayerStorage::SetProfileSettingValueByName(FName ProfileSettingName,
														   const FString& NewValue)
{
	INT ProfileSettingId;
	// See if this is a known string setting
	if (GetProfileSettingId(ProfileSettingName,ProfileSettingId))
	{
		return SetProfileSettingValue(ProfileSettingId,NewValue);
	}
	return FALSE;
}

/**
* Searches for the profile setting by name and sets the value index to the
* value contained in the profile setting meta data
*
* @param ProfileSettingName the name of the profile setting to set the string value of
* @param NewValue the string value to use
*
* @return true if the profile setting was found and the value was set, false otherwise
*/
UBOOL UOnlinePlayerStorage::SetProfileSettingValue(INT ProfileSettingId,
													 const FString& NewValue)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the profile setting that matches this id
			for (INT Index2 = 0; Index2 < ProfileSettings.Num(); Index2++)
			{
				FOnlineProfileSetting& Setting = ProfileSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is ID mapped, then find the ID
					if (MetaData.MappingType == PVMT_IdMapped)
					{
						FName ValueName(*NewValue);
						// Now search for the value index mapping
						for (INT Index3 = 0; Index3 < MetaData.ValueMappings.Num(); Index3++)
						{
							const FIdToStringMapping& Mapping = MetaData.ValueMappings(Index3);
							if (Mapping.Name == ValueName)
							{
								// Set the value based off the ID for this mapping
								Setting.ProfileSetting.Data.SetData(Mapping.Id);
								return TRUE;
							}
						}
					}
					else
					{
						// This item should be treated as a string
						Setting.ProfileSetting.Data.FromString(NewValue);
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**
* Searches for the profile setting by id and gets the value index
*
* @param ProfileSettingId the id of the profile setting to return
* @param ValueId the out value of the id
* @param ListIndex the out value of the index where that value lies in the ValueMappings list
*
* @return true if the profile setting was found and id mapped, false otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingValueId(INT ProfileSettingId,INT& ValueId,INT* ListIndex)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the profile setting that matches this id
			for (INT Index2 = 0; Index2 < ProfileSettings.Num(); Index2++)
			{
				FOnlineProfileSetting& Setting = ProfileSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is ID mapped, then read the ID
					if (MetaData.MappingType == PVMT_IdMapped)
					{
						Setting.ProfileSetting.Data.GetData(ValueId);

						// If a ListIndex is requested, look for it in the ValueMappings array
						if ( ListIndex )
						{
							// Find that value in the ValueMappings list
							for (INT MapIdx = 0; MapIdx < MetaData.ValueMappings.Num(); MapIdx++)
							{
								if (MetaData.ValueMappings(MapIdx).Id == ValueId)
								{
									*ListIndex = MapIdx;
									break;
								}
							}
						}
						return TRUE;
					}
					else
					{
						warnf( TEXT( "GetProfileSettingValueId did not find a valid MappingType.  ProfileSettingId: %d ValueId: %d" ), ProfileSettingId, ValueId );
						return FALSE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**
* Maps a list index into a value mappings array to the actual value that should be stored in the profile
* 
* @param ProfileSettingId  The ID of the profile setting to use for index to value conversion
* @param ListIndex         The index into the value options array to convert
* @param Value             The out param that will contain the value to save in the profile
* 
* @return TRUE if the profile settings was found, FALSE if not 
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingValueFromListIndex(INT ProfileSettingId, INT ListIndex, INT& Value)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			Value = MetaData.ValueMappings(ListIndex).Id;
			return TRUE;
		}
	}

	return FALSE;
}

/**
* Searches for the profile setting by id and gets the value index
*
* @param ProfileSettingId the id of the profile setting to return
* @param Value the out value of the setting
*
* @return true if the profile setting was found and not id mapped, false otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingValueInt(INT ProfileSettingId,INT& Value)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the profile setting that matches this id
			for (INT Index2 = 0; Index2 < ProfileSettings.Num(); Index2++)
			{
				FOnlineProfileSetting& Setting = ProfileSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is a raw value, then read it
					if (MetaData.MappingType == PVMT_RawValue)
					{
						Setting.ProfileSetting.Data.GetData(Value);
						return TRUE;
					}
					else
					{
						warnf( TEXT( "GetProfileSettingValueInt did not find a valid MappingType.  ProfileSettingId: %d Value: %d" ), ProfileSettingId, Value );
						return FALSE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**
* Searches for the profile setting by id and gets the value index
*
* @param ProfileSettingId the id of the profile setting to return
* @param Value the out value of the setting
*
* @return true if the profile setting was found and not id mapped, false otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingValueFloat(INT ProfileSettingId,FLOAT& Value)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the profile setting that matches this id
			for (INT Index2 = 0; Index2 < ProfileSettings.Num(); Index2++)
			{
				FOnlineProfileSetting& Setting = ProfileSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is a raw value, then read it
					if (MetaData.MappingType == PVMT_RawValue)
					{
						Setting.ProfileSetting.Data.GetData(Value);
						return TRUE;
					}
					else
					{
						warnf( TEXT( "GetProfileSettingValueFloat did not find a valid MappingType.  ProfileSettingId: %d ValueId: %d" ), ProfileSettingId, Value );
						return FALSE;
					}
				}
			}
		}
	}
	return FALSE;
}


/**
* Searches for the profile setting by id and sets the value
*
* @param ProfileSettingId the id of the profile setting to return
* @param Value the new value of the setting
*
* @return true if the profile setting was found and not id mapped, false otherwise
*/
UBOOL UOnlinePlayerStorage::SetProfileSettingValueId(INT ProfileSettingId,INT Value)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the profile setting that matches this id
			for (INT Index2 = 0; Index2 < ProfileSettings.Num(); Index2++)
			{
				FOnlineProfileSetting& Setting = ProfileSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is a raw value, then read it
					if (MetaData.MappingType == PVMT_IdMapped)
					{
						Setting.ProfileSetting.Data.SetData(Value);
						return TRUE;
					}
					else
					{
						warnf( TEXT( "SetProfileSettingValueId did not find a valid MappingType.  ProfileSettingId: %d Value: %d" ), ProfileSettingId, Value );
						return FALSE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**
* Searches for the profile setting by id and sets the value
*
* @param ProfileSettingId the id of the profile setting to return
* @param Value the new value of the setting
*
* @return true if the profile setting was found and not id mapped, false otherwise
*/
UBOOL UOnlinePlayerStorage::SetProfileSettingValueInt(INT ProfileSettingId,INT Value)
{
	return SetProfileSettingTypedValue<INT>(ProfileSettingId,Value);
}

/**
* Searches for the profile setting by id and sets the value
*
* @param ProfileSettingId the id of the profile setting to return
* @param Value the new value of the setting
*
* @return true if the profile setting was found and not id mapped, false otherwise
*/
UBOOL UOnlinePlayerStorage::SetProfileSettingValueFloat(INT ProfileSettingId,FLOAT Value)
{
	return SetProfileSettingTypedValue<FLOAT>(ProfileSettingId,Value);
}

/**
* Determines the mapping type for the specified property
*
* @param ProfileId the ID to get the mapping type for
* @param OutType the out var the value is placed in
*
* @return TRUE if found, FALSE otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingMappingType(INT ProfileId,BYTE& OutType)
{
	// Check for the property mapping
	FSettingsPropertyPropertyMetaData* MetaData = FindProfileSettingMetaData(ProfileId);
	if (MetaData != NULL)
	{
		OutType = MetaData->MappingType;
		return TRUE;
	}
	return FALSE;
}

/**
 * Get the list of Ids this profile setting maps to
 *
 * @param ProfileId the ID to get the mapping type for
 * @param Ids the list of IDs that are in this mapping
 *
 * @return TRUE if found, FALSE otherwise
 */
UBOOL UOnlinePlayerStorage::GetProfileSettingMappingIds(INT ProfileId,TArray<INT>& IDs)
{
	// Check for the property mapping
	FSettingsPropertyPropertyMetaData* MetaData = FindProfileSettingMetaData(ProfileId);
	if (MetaData != NULL && MetaData->MappingType == PVMT_IdMapped)
	{
		IDs.Empty(MetaData->ValueMappings.Num());
		for (INT MappingIdx=0; MappingIdx<MetaData->ValueMappings.Num(); MappingIdx++)
		{
			IDs.AddItem(MetaData->ValueMappings(MappingIdx).Id);
		}
		return TRUE;
	}
	return FALSE;
}

/**
* Determines the min and max values of a property that is clamped to a range
*
* @param ProfileId the ID to get the mapping type for
* @param OutMinValue the out var the min value is placed in
* @param OutMaxValue the out var the max value is placed in
* @param OutRangeIncrement the amount the range can be adjusted by the UI in any single update
* @param bFormatAsInt whether the range's value should be treated as an int.
*
* @return TRUE if found and is a range property, FALSE otherwise
*/
UBOOL UOnlinePlayerStorage::GetProfileSettingRange(INT ProfileId,FLOAT& OutMinValue,FLOAT& OutMaxValue,FLOAT& OutRangeIncrement,BYTE& bOutFormatAsInt)
{
	UBOOL bFound = FALSE;
	// Check for the property mapping
	FSettingsPropertyPropertyMetaData* MetaData = FindProfileSettingMetaData(ProfileId);
	FOnlineProfileSetting* Setting = FindSetting(ProfileId);
	// And make sure it's a ranged property
	if (MetaData != NULL && MetaData->MappingType == PVMT_Ranged && Setting != NULL)
	{
		OutRangeIncrement = MetaData->RangeIncrement;
		OutMinValue = MetaData->MinVal;
		OutMaxValue = MetaData->MaxVal;
		bOutFormatAsInt = Setting->ProfileSetting.Data.Type == SDT_Int32;
		bFound = TRUE;
	}
	return bFound;
}

/**
* Sets the value of a ranged property, clamping to the min/max values
*
* @param ProfileId the ID of the property to set
* @param NewValue the new value to apply to the 
*
* @return TRUE if found and is a range property, FALSE otherwise
*/
UBOOL UOnlinePlayerStorage::SetRangedProfileSettingValue(INT ProfileId,FLOAT NewValue)
{
	UBOOL bFound = FALSE;
	FLOAT MinValue, MaxValue, Increment;
	BYTE bIntRange;
	// Read the min/max for the property id so we can clamp
	if (GetProfileSettingRange(ProfileId,MinValue,MaxValue,Increment,bIntRange))
	{
		// Clamp within the valid range
		NewValue = Clamp<FLOAT>(NewValue,MinValue,MaxValue);
		if ( bIntRange )
		{
			NewValue = appTrunc(NewValue);
		}
		// Get the profile setting that we are going to set
		FOnlineProfileSetting* ProfileSetting = FindSetting(ProfileId);
		check(ProfileSetting && "Missing profile setting that has a meta data entry");
		// Now set the value
		switch (ProfileSetting->ProfileSetting.Data.Type)
		{
		case SDT_Int32:
			{
				INT Value = appTrunc(NewValue);

				ProfileSetting->ProfileSetting.Data.SetData(Value);
				bFound = TRUE;
				break;
			}
		case SDT_Float:
			{
				ProfileSetting->ProfileSetting.Data.SetData(NewValue);
				bFound = TRUE;
				break;
			}
		}
	}
	return bFound;
}

/**
* Gets the value of a ranged property
*
* @param ProfileId the ID to get the value of
* @param OutValue the out var that receives the value
*
* @return TRUE if found and is a range property, FALSE otherwise
*/
UBOOL UOnlinePlayerStorage::GetRangedProfileSettingValue(INT ProfileId,FLOAT& OutValue)
{
	// Get the profile setting that we are going to read
	FOnlineProfileSetting* ProfileSetting = FindSetting(ProfileId);
	if (ProfileSetting)
	{
		// Now set the value
		switch (ProfileSetting->ProfileSetting.Data.Type)
		{
		case SDT_Int32:
			{
				INT Value;
				ProfileSetting->ProfileSetting.Data.GetData(Value);
				OutValue = Value;
				break;
			}
		case SDT_Float:
			{
				ProfileSetting->ProfileSetting.Data.GetData(OutValue);
				break;
			}
		default:
			{
				ProfileSetting = NULL;
				break;
			}
		}
	}
	return ProfileSetting != NULL;
}

/** Finalize the clean up process */
void UOnlinePlayerStorage::FinishDestroy(void)
{
	ProfileSettings.Empty();
	Super::FinishDestroy();
}

/**
 * Adds an id to the array, assuming that it doesn't already exist
 *
 * @param SettingId the id to add to the array
 */
void UOnlinePlayerStorage::AddSettingInt(INT SettingId)
{
	AddSettingTypedValue<INT>(SettingId);
}

/**
 * Adds an id to the array, assuming that it doesn't already exist
 *
 * @param SettingId the id to add to the array
 */
void UOnlinePlayerStorage::AddSettingFloat(INT SettingId)
{
	AddSettingTypedValue<FLOAT>(SettingId);
}

/**
 * Sets all of the profile settings to their default values
 */
void UOnlinePlayerStorage::SetToDefaults(void)
{
	ProfileSettings.Empty();
	// Now append the version
	AppendVersionToSettings();
}

/**
 * Adds the version number to the read data if not present
 */
void UOnlinePlayerStorage::AppendVersionToSettings(void)
{
	UBOOL bFound = FALSE;
	// Search the read ids for the version number
	for (INT Index = 0; Index < ProfileSettings.Num(); Index++)
	{
		FOnlineProfileSetting& Setting = ProfileSettings(Index);
		if (Setting.ProfileSetting.PropertyId == VersionSettingsId)
		{
			Setting.ProfileSetting.Data.SetData(VersionNumber);
			bFound = TRUE;
			break;
		}
	}
	// Add the version number if missing
	if (bFound == FALSE)
	{
		INT AddIndex = ProfileSettings.AddZeroed();
		FOnlineProfileSetting& Setting = ProfileSettings(AddIndex);
		// Copy over the data
		Setting.Owner = OPPO_Game;
		Setting.ProfileSetting.PropertyId = VersionSettingsId;
		Setting.ProfileSetting.Data.SetData(VersionNumber);
	}
}

/** Returns the version number that was found in the profile read results */
INT UOnlinePlayerStorage::GetVersionNumber(void)
{
	// Set to an invalid number
	INT VerNum = -1;
	// Search the read ids for the version number
	for (INT Index = 0; Index < ProfileSettings.Num(); Index++)
	{
		const FOnlineProfileSetting& Setting = ProfileSettings(Index);
		if (Setting.ProfileSetting.PropertyId == VersionSettingsId)
		{
			// Read whatever came back from the provider
			Setting.ProfileSetting.Data.GetData(VerNum);
			break;
		}
	}
	return VerNum;
}

/** Sets the version number to the class default */
void UOnlinePlayerStorage::SetDefaultVersionNumber(void)
{
	// Search the read ids for the version number
	for (INT Index = 0; Index < ProfileSettings.Num(); Index++)
	{
		FOnlineProfileSetting& Setting = ProfileSettings(Index);
		if (Setting.ProfileSetting.PropertyId == VersionSettingsId)
		{
			// Set it with the default
			Setting.ProfileSetting.Data.SetData(VersionNumber);
			break;
		}
	}
}

/**
 * Increments the number of times this profile has been saved
 *
 * @param NewCount the new number to use as the save count
 * @param ProfileSettings the array to search for the setting to update
 */
void UOnlinePlayerStorage::SetProfileSaveCount(INT NewCount,TArray<FOnlineProfileSetting>& ProfileSettings,INT SaveCountId)
{
	INT SaveCountIndex = -1;
	// Search the read ids for the save count
	for (INT Index = 0; Index < ProfileSettings.Num(); Index++)
	{
		const FOnlineProfileSetting& Setting = ProfileSettings(Index);
		if (Setting.ProfileSetting.PropertyId == SaveCountId)
		{
			SaveCountIndex = Index;
			break;
		}
	}
	// Add the save countif missing
	if (SaveCountIndex == -1)
	{
		SaveCountIndex = ProfileSettings.AddZeroed();
	}
	check(SaveCountIndex >= 0 && SaveCountIndex < ProfileSettings.Num());
	FOnlineProfileSetting& Setting = ProfileSettings(SaveCountIndex);
	// Set the data for the save count
	Setting.Owner = OPPO_Game;
	Setting.ProfileSetting.PropertyId = SaveCountId;
	Setting.ProfileSetting.Data.SetData(NewCount);
}

/**
 * Reads the number of times this profile has been saved
 *
 * @param ProfileSettings the array to search for the value
 *
 * @return the number of times the profile data has been saved
 */
INT UOnlinePlayerStorage::GetProfileSaveCount(const TArray<FOnlineProfileSetting>& ProfileSettings,INT SaveCountId)
{
	// Set to an invalid number
	INT SaveCount = -1;
	// Search the read ids for the save count
	for (INT Index = 0; Index < ProfileSettings.Num(); Index++)
	{
		const FOnlineProfileSetting& Setting = ProfileSettings(Index);
		// Is this the property holding the save count?
		if (Setting.ProfileSetting.PropertyId == SaveCountId)
		{
			Setting.ProfileSetting.Data.GetData(SaveCount);
			break;
		}
	}
	return SaveCount;
}
