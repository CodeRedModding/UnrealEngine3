/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UOnlineProfileSettings);


/**
 * Sets all of the profile settings to their default values
 */
void UOnlineProfileSettings::SetToDefaults(void)
{
	ProfileSettings.Empty();
	// Add all of the defaults for this user
	for (INT Index = 0; Index < DefaultSettings.Num(); Index++)
	{
		// Copy the default into that user's settings
		INT AddIndex = ProfileSettings.AddZeroed();
		// Deep copy the data
		ProfileSettings(AddIndex) = DefaultSettings(Index);
	}
	// Now append the version
	AppendVersionToSettings();
}

/**
 * Adds the version id to the read ids if it is not present
 */
void UOnlineProfileSettings::AppendVersionToReadIds(void)
{
	UBOOL bFound = FALSE;
	// Search the read ids for the version number
	for (INT Index = 0; Index < ProfileSettingIds.Num(); Index++)
	{
		if (ProfileSettingIds(Index) == PSI_ProfileVersionNum)
		{
			bFound = TRUE;
			break;
		}
	}
	// Add the version id if missing
	if (bFound == FALSE)
	{
		ProfileSettingIds.AddItem(PSI_ProfileVersionNum);
	}
}

/**
 * Searches for the profile setting by id and gets the default value index
 *
 * @param ProfileSettingId the id of the profile setting to return
 * @param DefaultId the out value of the default id
 * @param ListIndex the out value of the index where that value lies in the ValueMappings list
 *
 * @return true if the profile setting was found and retrieved the default id, false otherwise
 */
UBOOL UOnlineProfileSettings::GetProfileSettingDefaultId(INT ProfileSettingId, INT& DefaultId, INT& ListIndex)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the default setting that matches this id
			for (INT Index2 = 0; Index2 < DefaultSettings.Num(); Index2++)
			{
				FOnlineProfileSetting& Setting = DefaultSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is ID mapped, then read the ID
					if (MetaData.MappingType == PVMT_IdMapped)
					{
						Setting.ProfileSetting.Data.GetData(DefaultId);

						// Found the default value, now find that value in the ValueMappings list
						for (INT MapIdx = 0; MapIdx < MetaData.ValueMappings.Num(); MapIdx++)
						{
							if (MetaData.ValueMappings(MapIdx).Id == DefaultId)
							{
								ListIndex = MapIdx;
								break;
							}
						}
						return TRUE;
					}
					else
					{
						return FALSE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**
 * Searches for the profile setting by id and gets the default value int
 *
 * @param ProfileSettingId the id of the profile setting to return the default of
 * @param Value the out value of the default setting
 *
 * @return true if the profile setting was found and retrieved the default int, false otherwise
 */
UBOOL UOnlineProfileSettings::GetProfileSettingDefaultInt(INT ProfileSettingId, INT& DefaultInt)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the default setting that matches this id
			for (INT Index2 = 0; Index2 < DefaultSettings.Num(); Index2++)
			{
				FOnlineProfileSetting& Setting = DefaultSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is a raw value, then read it
					if (MetaData.MappingType == PVMT_RawValue)
					{
						Setting.ProfileSetting.Data.GetData(DefaultInt);
						return TRUE;
					}
					else
					{
						return FALSE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**
 * Searches for the profile setting by id and gets the default value float
 *
 * @param ProfileSettingId the id of the profile setting to return the default of
 * @param Value the out value of the default setting
 *
 * @return true if the profile setting was found and retrieved the default float, false otherwise
 */
UBOOL UOnlineProfileSettings::GetProfileSettingDefaultFloat(INT ProfileSettingId, FLOAT& DefaultFloat)
{
	// Search for the profile setting id in the mappings
	for (INT Index = 0; Index < ProfileMappings.Num(); Index++)
	{
		const FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(Index);
		if (MetaData.Id == ProfileSettingId)
		{
			// Find the default setting that matches this id
			for (INT Index2 = 0; Index2 < DefaultSettings.Num(); Index2++)
			{
				FOnlineProfileSetting& Setting = DefaultSettings(Index2);
				if (Setting.ProfileSetting.PropertyId == ProfileSettingId)
				{
					// If this is a raw value, then read it
					if (MetaData.MappingType == PVMT_RawValue)
					{
						Setting.ProfileSetting.Data.GetData(DefaultFloat);
						return TRUE;
					}
					else
					{
						return FALSE;
					}
				}
			}
		}
	}
	return FALSE;
}


