/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class holds the data used in reading/writing online player data.
 * The online player data is stored by an external service.
 */
class OnlinePlayerStorage extends Object
	dependson(Settings)
	native;
	
/** Enum indicating who owns a given online profile proprety */
enum EOnlineProfilePropertyOwner
{
	/** No owner assigned */
	OPPO_None,
	/** Owned by the online service */
	OPPO_OnlineService,
	/** Owned by the game in question */
	OPPO_Game
};

/**
 * Structure used to hold the information for a given profile setting
 */
struct native OnlineProfileSetting
{
	/** Which party owns the data (online service vs game) */
	var EOnlineProfilePropertyOwner Owner;
	/** The profile setting comprised of unique id and union of held types */
	var SettingsProperty ProfileSetting;

	structcpptext
	{
		/** Does nothing (no init version) */
		FOnlineProfileSetting(void)
		{
		}

		/**
		 * Zeroes members
		 */
		FOnlineProfileSetting(EEventParm) :
			Owner(0),
			ProfileSetting(EC_EventParm)
		{
		}

		/**
		 * Copy constructor. Copies the other into this object
		 *
		 * @param Other the other structure to copy
		 */
		FOnlineProfileSetting(const FOnlineProfileSetting& Other) :
			Owner(0),
			ProfileSetting(EC_EventParm)
		{
			Owner = Other.Owner;
			ProfileSetting = Other.ProfileSetting;
		}

		/**
		 * Assignment operator. Copies the other into this object
		 *
		 * @param Other the other structure to copy
		 */
		FOnlineProfileSetting& operator=(const FOnlineProfileSetting& Other)
		{	
			if (&Other != this)
			{
				Owner = Other.Owner;
				ProfileSetting = Other.ProfileSetting;
			}
			return *this;
		}
	}
};
/** Used to determine if the read online player data is the proper version or not */
var const int VersionNumber;
/** id for profile setting that contains the version # */
var const int VersionSettingsId;
/** id for profile setting that contains the save count */
var const int SaveCountSettingId;

/** Current set of player data that is either returned from a read or to be written out */
var array<OnlineProfileSetting> ProfileSettings;

/** Holds the set of mappings from native format to human readable format */
var array<SettingsPropertyPropertyMetaData> ProfileMappings;

/** Enum indicating the current async action happening on the player data */
enum EOnlinePlayerStorageAsyncState
{
	/** No read/write operation has started on this storage object */
	OPAS_NotStarted,
	/** Read operation is currently in progress */
	OPAS_Read,
	/** Write operation is currently in progress */
	OPAS_Write,
	/** Read/write operation has completed */
	OPAS_Finished
};

/** Indicates the state of the profile (whether an async action is happening and what type) */
var const EOnlinePlayerStorageAsyncState AsyncState;

/** The device id this data is stored to if applicable */
var int DeviceId;

/**
 * Searches the profile setting array for the matching string setting name and returns the id
 *
 * @param ProfileSettingName the name of the profile setting being searched for
 * @param ProfileSettingId the id of the context that matches the name
 *
 * @return true if the seting was found, false otherwise
 */
native function bool GetProfileSettingId(name ProfileSettingName,out int ProfileSettingId);

/**
 * Finds the human readable name for the profile setting
 *
 * @param ProfileSettingId the id to look up in the mappings table
 *
 * @return the name of the string setting that matches the id or NAME_None if not found
 */
native function name GetProfileSettingName(int ProfileSettingId);

/**
 * Finds the localized column header text for the profile setting
 *
 * @param ProfileSettingId the id to look up in the mappings table
 *
 * @return the string to use as the list column header for the profile setting that matches the id, or an empty string if not found.
 */
native function string GetProfileSettingColumnHeader( int ProfileSettingId );

/**
 * Finds the index of an OnlineProfileSetting struct given its settings id.
 *
 * @param	ProfileSettingId	the id of the struct to search for
 *
 * @return	the index into the ProfileSettings array for the struct with the matching id.
 */
native final function int FindProfileSettingIndex( int ProfileSettingId ) const;

/**
 * Finds the index of SettingsPropertyPropertyMetaData struct, given its settings id.
 *
 * @param	ProfileSettingId	the id of the struct to search for
 *
 * @return	the index into the ProfileMappings array for the struct with the matching id.
 */
native final function int FindProfileMappingIndex( int ProfileSettingId ) const;

/**
 * Finds the index of SettingsPropertyPropertyMetaData struct, given its settings name.
 *
 * @param	ProfileSettingId	the id of the struct to search for
 *
 * @return	the index into the ProfileMappings array for the struct with the matching name.
 */
native final function int FindProfileMappingIndexByName( name ProfileSettingName ) const;

/**
 * Finds the default index of SettingsPropertyPropertyMetaData struct, given its settings name.
 *
 * @param	ProfileSettingId	the id of the struct to search for
 *
 * @return	the index into the default ProfileMappings array for the struct with the matching name.
 */
native final static function int FindDefaultProfileMappingIndexByName( name ProfileSettingName ) const;

/**
 * Determines if the setting is id mapped or not
 *
 * @param ProfileSettingId the id to look up in the mappings table
 *
 * @return TRUE if the setting is id mapped, FALSE if it is a raw value
 */
native function bool IsProfileSettingIdMapped(int ProfileSettingId);

/**
 * Finds the human readable name for a profile setting's value. Searches the
 * profile settings mappings for the specifc profile setting and then searches
 * the set of values for the specific value index and returns that value's
 * human readable name
 *
 * @param ProfileSettingId the id to look up in the mappings table
 * @param Value the out param that gets the value copied to it
 * @param ValueMapID optional parameter that allows you to select a specific index in the ValueMappings instead
 * of automatically using the currently set index (if -1 is passed in, which is the default, it means to just
 * use the set index
 *
 * @return true if found, false otherwise
 */
native function bool GetProfileSettingValue(int ProfileSettingId,out string Value,optional int ValueMapID = -1);

/**
 * Finds the human readable name for a profile setting's value. Searches the
 * profile settings mappings for the specifc profile setting and then searches
 * the set of values for the specific value index and returns that value's
 * human readable name
 *
 * @param ProfileSettingId the id to look up in the mappings table
 *
 * @return the name of the value or NAME_None if not value mapped
 */
native function name GetProfileSettingValueName(int ProfileSettingId);

/**
 * Searches the profile settings mappings for the specifc profile setting and
 * then adds all of the possible values to the out parameter
 *
 * @param ProfileSettingId the id to look up in the mappings table
 * @param Values the out param that gets the list of values copied to it
 *
 * @return true if found and value mapped, false otherwise
 */
native function bool GetProfileSettingValues(int ProfileSettingId,out array<name> Values);

/**
 * Finds the human readable name for a profile setting's value. Searches the
 * profile settings mappings for the specifc profile setting and then searches
 * the set of values for the specific value index and returns that value's
 * human readable name
 *
 * @param ProfileSettingName the name of the profile setting to find the string value of
 * @param Value the out param that gets the value copied to it
 *
 * @return true if found, false otherwise
 */
native function bool GetProfileSettingValueByName(name ProfileSettingName,out string Value);

/**
 * Searches for the profile setting by name and sets the value index to the
 * value contained in the profile setting meta data
 *
 * @param ProfileSettingName the name of the profile setting to find
 * @param NewValue the string value to use
 *
 * @return true if the profile setting was found and the value was set, false otherwise
 */
native function bool SetProfileSettingValueByName(name ProfileSettingName,const out string NewValue);

/**
 * Searches for the profile setting by name and sets the value index to the
 * value contained in the profile setting meta data
 *
 * @param ProfileSettingName the name of the profile setting to set the string value of
 * @param NewValue the string value to use
 *
 * @return true if the profile setting was found and the value was set, false otherwise
 */
native function bool SetProfileSettingValue(int ProfileSettingId,const out string NewValue);

/**
 * Searches for the profile setting by id and gets the value index
 *
 * @param ProfileSettingId the id of the profile setting to return
 * @param ValueId the out value of the id
 * @param ListIndex the out value of the index where that value lies in the ValueMappings list
 *
 * @return true if the profile setting was found and id mapped, false otherwise
 */
native function bool GetProfileSettingValueId(int ProfileSettingId,out int ValueId,optional out int ListIndex);

/**
 * Maps a list index into a value mappings array to the actual value that should be stored in the profile
 * 
 * @param ProfileSettingId  The ID of the profile setting to use for index to value conversion
 * @param ListIndex         The index into the value options array to convert
 * @param Value             The out param that will contain the value to save in the profile
 * 
 * @return TRUE if the profile settings was found, FALSE if not 
 */
native function bool GetProfileSettingValueFromListIndex(int ProfileSettingId, int ListIndex, out int Value);

/**
 * Searches for the profile setting by id and gets the value index
 *
 * @param ProfileSettingId the id of the profile setting to return
 * @param Value the out value of the setting
 *
 * @return true if the profile setting was found and not id mapped, false otherwise
 */
native function bool GetProfileSettingValueInt(int ProfileSettingId,out int Value);

/**
 * Searches for the profile setting by id and gets the value index
 *
 * @param ProfileSettingId the id of the profile setting to return
 * @param Value the out value of the setting
 *
 * @return true if the profile setting was found and not id mapped, false otherwise
 */
native function bool GetProfileSettingValueFloat(int ProfileSettingId,out float Value);

/**
 * Searches for the profile setting by id and sets the value
 *
 * @param ProfileSettingId the id of the profile setting to return
 * @param Value the new value
 *
 * @return true if the profile setting was found and id mapped, false otherwise
 */
native function bool SetProfileSettingValueId(int ProfileSettingId,int Value);

/**
 * Searches for the profile setting by id and sets the value
 *
 * @param ProfileSettingId the id of the profile setting to return
 * @param Value the new value
 *
 * @return true if the profile setting was found and not id mapped, false otherwise
 */
native function bool SetProfileSettingValueInt(int ProfileSettingId,int Value);

/**
 * Searches for the profile setting by id and sets the value
 *
 * @param ProfileSettingId the id of the profile setting to return
 * @param Value the new value
 *
 * @return true if the profile setting was found and not id mapped, false otherwise
 */
native function bool SetProfileSettingValueFloat(int ProfileSettingId,float Value);

/**
 * Determines the mapping type for the specified property
 *
 * @param ProfileId the ID to get the mapping type for
 * @param OutType the out var the value is placed in
 *
 * @return TRUE if found, FALSE otherwise
 */
native function bool GetProfileSettingMappingType(int ProfileId,out EPropertyValueMappingType OutType);

/**
 * Get the list of Ids this profile setting maps to
 *
 * @param ProfileId the ID to get the mapping type for
 * @param Ids the list of IDs that are in this mapping
 *
 * @return TRUE if found, FALSE otherwise
 */
native static function bool GetProfileSettingMappingIds(int ProfileId,out array<int> Ids);

/**
 * Determines the min and max values of a property that is clamped to a range
 *
 * @param ProfileId the ID to get the mapping type for
 * @param OutMinValue the out var the min value is placed in
 * @param OutMaxValue the out var the max value is placed in
 * @param RangeIncrement the amount the range can be adjusted by the UI in any single update
 * @param bFormatAsInt whether the range's value should be treated as an int.
 *
 * @return TRUE if found and is a range property, FALSE otherwise
 */
native function bool GetProfileSettingRange(int ProfileId,out float OutMinValue,out float OutMaxValue,out float RangeIncrement,out byte bFormatAsInt);

/**
 * Sets the value of a ranged property, clamping to the min/max values
 *
 * @param ProfileId the ID of the property to set
 * @param NewValue the new value to apply to the
 *
 * @return TRUE if found and is a range property, FALSE otherwise
 */
native function bool SetRangedProfileSettingValue(int ProfileId,float NewValue);

/**
 * Gets the value of a ranged property
 *
 * @param ProfileId the ID to get the value of
 * @param OutValue the out var that receives the value
 *
 * @return TRUE if found and is a range property, FALSE otherwise
 */
native function bool GetRangedProfileSettingValue(int ProfileId,out float OutValue);

/**
 * Adds an id to the array, assuming that it doesn't already exist
 *
 * @param SettingId the id to add to the array
 */
native function AddSettingInt(int SettingId);

/**
 * Adds an id to the array, assuming that it doesn't already exist
 *
 * @param SettingId the id to add to the array
 */
native function AddSettingFloat(int SettingId);

cpptext
{
public:
	/**
	 * Finds the specified profile setting
	 *
	 * @param SettingId to search for
	 *
	 * @return pointer to the setting or NULL if not found
	 */
	FORCEINLINE FOnlineProfileSetting* FindSetting(INT SettingId)
	{
		for (INT ProfileIndex = 0; ProfileIndex < ProfileSettings.Num(); ++ProfileIndex)
		{
			FOnlineProfileSetting& Setting = ProfileSettings(ProfileIndex);
			if (Setting.ProfileSetting.PropertyId == SettingId)
			{
				return &Setting;
			}
		}
		return NULL;
	}

	/**
	 * Finds the specified property's meta data
	 *
	 * @param PropertyId id of the property to search the meta data for
	 *
	 * @return pointer to the property meta data or NULL if not found
	 */
	FORCEINLINE FSettingsPropertyPropertyMetaData* FindProfileSettingMetaData(INT ProfileId)
	{
		for (INT MetaDataIndex = 0; MetaDataIndex < ProfileMappings.Num(); MetaDataIndex++)
		{
			FSettingsPropertyPropertyMetaData& MetaData = ProfileMappings(MetaDataIndex);
			if (MetaData.Id == ProfileId)
			{
				return &MetaData;
			}
		}
		return NULL;
	}

	/**
	 * Searches for the profile setting by id and sets the value
	 *
	 * @param ProfileSettingId the id of the profile setting to return
	 * @param Value the new value of the setting
	 *
	 * @return true if the profile setting was found and not id mapped, false otherwise
	 */
	template<typename TYPE>
	FORCEINLINE UBOOL SetProfileSettingTypedValue(INT ProfileSettingId,TYPE Value)
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
							Setting.ProfileSetting.Data.SetData(Value);
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
	 * Searches for the profile setting by id and sets the value
	 *
	 * @param ProfileSettingId the id of the profile setting to return
	 * @param Value the new value of the setting
	 *
	 * @return true if the profile setting was found and not id mapped, false otherwise
	 */
	template<typename TYPE>
	FORCEINLINE void AddSettingTypedValue(INT ProfileSettingId)
	{
		// Don't add if it already exists
		if (FindSetting(ProfileSettingId) == NULL)
		{
			TYPE Type = 0;
			// Construct an zeroed setting of the right type
			FOnlineProfileSetting Setting(EC_EventParm);
			Setting.Owner = OPPO_Game;
			Setting.ProfileSetting.PropertyId = ProfileSettingId;
			Setting.ProfileSetting.Data.SetData(Type);
			// Now add this to the array
			ProfileSettings.AddItem(Setting);
		}
	}

	/** Finalize the clean up process */
	virtual void FinishDestroy(void);

	/**
	 * Increments the number of times this profile has been saved
	 *
	 * @param NewCount the new number to use as the save count
	 * @param ProfileSettings the array to search for the setting to update
	 */
	static void SetProfileSaveCount(INT NewCount,TArray<FOnlineProfileSetting>& ProfileSettings,INT SaveCountId);

	/**
	 * Reads the number of times this profile has been saved
	 *
	 * @param ProfileSettings the array to search for the value
	 *
	 * @return the number of times the profile data has been saved
	 */
	static INT GetProfileSaveCount(const TArray<FOnlineProfileSetting>& ProfileSettings,INT SaveCountId);
}

/**
 * Clear out the settings. Subclasses can override to set their own defaults.
 */
native event SetToDefaults();

/**
 * Adds the version number to the read data if not present
 */
native function AppendVersionToSettings();

/** Returns the version number that was found in the profile read results */
native function int GetVersionNumber();

/** Sets the version number to the class default */
native function SetDefaultVersionNumber();

defaultproperties
{
	// This must be set by subclasses
	VersionNumber=-1
	VersionSettingsId=0
	SaveCountSettingId=1
	// Set to an invalid device
	DeviceId=-1
}
