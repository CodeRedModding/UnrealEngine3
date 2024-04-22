/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

IMPLEMENT_CLASS(USettings);

/**
 * Copy constructor. Copies the other into this object
 *
 * @param Other the other structure to copy
 */
FSettingsData::FSettingsData(const FSettingsData& Other) :
	Type(SDT_Empty),
	Value1(0),
	Value2(0)
{
	// Use common methods for doing deep copy or just do a simple shallow copy
	if (Other.Type == SDT_String)
	{
		SetData((const TCHAR*)Other.Value2);
	}
	else if (Other.Type == SDT_Blob)
	{
		SetData((DWORD)Other.Value1,(const BYTE*)Other.Value2);
	}
	else
	{
		// Shallow copy is safe
		appMemcpy(this,&Other,sizeof(FSettingsData));
	}
}

/**
 * Assignment operator. Copies the other into this object
 *
 * @param Other the other structure to copy
 */
FSettingsData& FSettingsData::operator=(const FSettingsData& Other)
{
	if (this != &Other)
	{
		// Use common methods for doing deep copy or just do a simple shallow copy
		if (Other.Type == SDT_String)
		{
			SetData((const TCHAR*)Other.Value2);
		}
		else if (Other.Type == SDT_Blob)
		{
			SetData((DWORD)Other.Value1,(const BYTE*)Other.Value2);
		}
		else
		{
			CleanUp();
			// Shallow copy is safe
			appMemcpy(this,&Other,sizeof(FSettingsData));
		}
	}
	return *this;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FSettingsData::SetData(const TCHAR* InData)
{
	CleanUp();
	Type = SDT_String;
	if (InData != NULL)
	{	
		Value1 = appStrlen(InData);
		// Allocate a buffer for the string plus terminator
		Value2 = (INT*) new TCHAR[Value1 + 1];
		if (Value1 > 0)
		{
			// Copy the data
			appStrcpy((TCHAR*)Value2,Value1 + 1,InData);
		}
		else
		{
			((TCHAR*)Value2)[0] = TEXT('\0');
		}
	}
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FSettingsData::SetData(const FString& InData)
{
	SetData(*InData);
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FSettingsData::SetData(INT InData)
{
	CleanUp();
	Type = SDT_Int32;
	Value1 = InData;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FSettingsData::SetData(DOUBLE InData)
{
	CleanUp();
	Type = SDT_Double;
	*(DOUBLE*)&Value1 = InData;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FSettingsData::SetData(FLOAT InData)
{
	CleanUp();
	Type = SDT_Float;
	Value1 = *(INT*)&InData;
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FSettingsData::SetData(const TArray<BYTE>& InData)
{
	SetData((DWORD)InData.Num(),(const BYTE*)InData.GetData());
}

/**
 * Copies the data and sets the type
 *
 * @param Size the length of the buffer to copy
 * @param InData the new data to assign
 */
void FSettingsData::SetData(DWORD Size,const BYTE* InData)
{
	CleanUp();
	Type = SDT_Blob;
	if (Size > 0)
	{
		// Deep copy the binary data
		Value1 = (INT)Size;
		Value2 = (INT*)new BYTE[Value1];
		appMemcpy(Value2,InData,Value1);
	}
}

/**
 * Copies the data and sets the type
 *
 * @param InData the new data to assign
 */
void FSettingsData::SetData(QWORD InData)
{
	CleanUp();
	Type = SDT_Int64;
	*(QWORD*)&Value1 = InData;
}

/**
 * Copies the data into the two fields after verifying the type is DateTime
 *
 * @param InData1 the first part to assign
 * @param InData2 the second part to assign
 */
void FSettingsData::SetData(INT InData1,INT InData2)
{
	CleanUp();
	Type = SDT_DateTime;
	Value1 = InData1;
	Value2 = (INT*)(PTRINT)InData2;
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FSettingsData::GetData(FString& OutData) const
{
	if (Type == SDT_String && Value2 != NULL)
	{
		OutData = (const TCHAR*)Value2;
	}
	else
	{
		OutData = TEXT("");
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FSettingsData::GetData(INT& OutData) const
{
	if (Type == SDT_Int32)
	{
		OutData = Value1;
	}
	else
	{
		OutData = 0;
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FSettingsData::GetData(QWORD& OutData) const
{
	if (Type == SDT_Int64)
	{
		OutData = *(QWORD*)&Value1;
	}
	else
	{
		OutData = 0;
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FSettingsData::GetData(FLOAT& OutData) const
{
	if (Type == SDT_Float)
	{
		OutData = *(FLOAT*)&Value1;
	}
	else
	{
		OutData = 0.f;
	}
}

/**
 * Copies the data after verifying the type
 * NOTE: Performs a deep copy so you are repsonsible for freeing the data
 *
 * @param OutSize out value that receives the size of the copied data
 * @param OutData out value that receives the copied data
 */
void FSettingsData::GetData(DWORD& OutSize,BYTE** OutData) const
{
	if (Type == SDT_Blob)
	{
		OutSize = Value1;
		// Need to perform a deep copy
		*OutData = new BYTE[OutSize];
		appMemcpy(*OutData,Value2,Value1);
	}
	else
	{
		OutSize = 0;
		*OutData = NULL;
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FSettingsData::GetData(TArray<BYTE>& OutData) const
{
	if (Type == SDT_Blob)
	{
		// Presize the array so it only allocates what's needed
		OutData.Empty(Value1);
		OutData.Add(Value1);
		// Copy into the array
		appMemcpy(OutData.GetData(),Value2,Value1);
	}
	else
	{
		OutData.Empty();
	}
}

/**
 * Copies the data into the two fields after verifying the type is DateTime
 *
 * @param InData1 the first part to assign
 * @param InData2 the second part to assign
 */
void FSettingsData::GetData(INT& InData1,INT& InData2) const
{
	if (Type == SDT_DateTime)
	{
		InData1 = Value1;
		InData2 = (INT)(PTRINT)Value2;
	}
	else
	{
		InData1 = 0;
		InData2 = 0;
	}
}

/**
 * Copies the data after verifying the type
 *
 * @param OutData out value that receives the copied data
 */
void FSettingsData::GetData(DOUBLE& OutData) const
{
	if (Type == SDT_Double)
	{
		OutData = *(DOUBLE*)&Value1;
	}
	else
	{
		OutData = 0.0;
	}
}

/**
 * Cleans up the existing data and sets the type to SDT_Empty
 */
void FSettingsData::CleanUp(void)
{
	// Strings are copied so make sure to delete them
	if (Type == SDT_String)
	{
		delete [] (TCHAR*)Value2;
	}
	else if (Type == SDT_Blob)
	{
		delete [] (BYTE*)Value2;
	}
	Type = SDT_Empty;
	Value1 = 0;
	Value2 = NULL;
}

/**
 * Converts the data into a string representation
 */
FString FSettingsData::ToString(void) const
{
	switch (Type)
	{
		case SDT_Float:
		{
			// Convert the float to a string
			FLOAT FloatVal;
			GetData(FloatVal);
			return FString::Printf(TEXT("%f"),(DOUBLE)FloatVal);
		}
		case SDT_Int32:
		{
			// Convert the int to a string
			INT Val;
			GetData(Val);
			return FString::Printf(TEXT("%d"),Val);
		}
		case SDT_Int64:
		{
			// Convert the int to a string
			QWORD Val;
			GetData(Val);
			return FString::Printf(TEXT("%lld"),Val);
		}
		case SDT_DateTime:
		{
			INT Val1, Val2;
			GetData(Val1, Val2);
			return FString::Printf(TEXT("%08X%08X"),Val1, Val2);
		}
		case SDT_Double:
		{
			// Convert the double to a string
			DOUBLE Val;
			GetData(Val);
			return FString::Printf(TEXT("%f"),Val);
		}
		case SDT_String:
		{
			// Copy the string out
			FString StringVal;
			GetData(StringVal);
			return StringVal;
		}
		case SDT_Blob:
		{
			INT Num = (Value1);
			return FString::Printf(TEXT("%d byte blob"), Num);
		}
	}
	return TEXT("");
}

/**
 * Converts the string to the specified type of data for this setting
 *
 * @param NewValue the string value to convert
 */
UBOOL FSettingsData::FromString(const FString& NewValue)
{
	switch (Type)
	{
		case SDT_Float:
		{
			// Convert the string to a float
			FLOAT FloatVal = appAtof(*NewValue);
			SetData(FloatVal);
			return TRUE;
		}
		case SDT_Int32:
		{
			// Convert the string to a int
			INT IntVal = appAtoi(*NewValue);
			SetData(IntVal);
			return TRUE;
		}
		case SDT_Double:
		{
			// Convert the string to a double
			DOUBLE Val = appAtof(*NewValue);
			SetData(Val);
			return TRUE;
		}
		case SDT_String:
		{
			// Copy the string
			SetData(NewValue);
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Comparison of two settings data classes
 *
 * @param Other the other settings data to compare against
 *
 * @return TRUE if they are equal, FALSE otherwise
 */
UBOOL FSettingsData::operator==(const FSettingsData& Other) const
{
	if (Type == Other.Type)
	{
		if (Type == SDT_Blob)
		{
			return appMemcmp((BYTE*)Value2,(BYTE*)Other.Value2,Value1) == 0;
		}
		else if (Type == SDT_String)
		{
			return appStrcmp((const TCHAR*)Value2,(const TCHAR*)Other.Value2) == 0;
		}
		else
		{
			return Value1 == Other.Value1 && Value2 == Other.Value2;
		}
	}
	return FALSE;
}
UBOOL FSettingsData::operator!=(const FSettingsData& Other) const
{
	return !(operator==(Other));
}

/**
 * Static function for setting members of the SettingsData union
 *
 * @param Data the data structure to set the fields of
 * @param InFloat the float data to set in the union
 */
void USettings::SetSettingsDataFloat(FSettingsData& Data,FLOAT InFloat)
{
	Data.SetData(InFloat);
}

/**
 * Static function for setting members of the SettingsData union
 *
 * @param Data the data structure to set the fields of
 * @param InInt the 32 bit integer data to set in the union
 */
void USettings::SetSettingsDataInt(FSettingsData& Data,INT InInt)
{
	Data.SetData(InInt);
}

/**
 * Static function for setting members of the SettingsData union
 *
 * @param Data the data structure to set the fields of
 * @param InBlob the 8 bytes to copy into the union
 */
void USettings::SetSettingsDataBlob(FSettingsData& Data,TArray<BYTE>& InBlob)
{
	Data.SetData(InBlob);
}

/**
 * Static function for setting members of the SettingsData union
 *
 * @param Data the data structure to set the fields of
 * @param InInt1 first half of the data to set
 * @param InInt2 second half of the data to set
 */
void USettings::SetSettingsDataDateTime(FSettingsData& Data,INT InInt1,INT InInt2)
{
	Data.SetData(InInt1,InInt2);
}

/**
 * Static function for setting members of the SettingsData union
 *
 * @param Data the data structure to set the fields of
 * @param Data2Copy the SettingsData object to copy
 */
void USettings::SetSettingsData(FSettingsData& Data,FSettingsData& Data2Copy)
{
	Data = Data2Copy;
}

/**
 * Static function for setting members of the SettingsData union
 *
 * @param Data the data structure to set the fields of
 * @param Data2Copy the SettingsData object to copy
 */
void USettings::EmptySettingsData(FSettingsData& Data)
{
	Data.CleanUp();
}

/**
 * Static function for copying data out of the SettingsData union
 *
 * @param Data the data structure to copy the data from
 */
FLOAT USettings::GetSettingsDataFloat(FSettingsData& Data)
{
	FLOAT LocalFloat;
	Data.GetData(LocalFloat);
	return LocalFloat;
}

/**
 * Static function for copying data out of the SettingsData union
 *
 * @param Data the data structure to copy the data from
 */
INT USettings::GetSettingsDataInt(FSettingsData& Data)
{
	INT LocalInt;
	Data.GetData(LocalInt);
	return LocalInt;
}

/**
 * Static function for copying data out the SettingsData union
 *
 * @param Data the data structure to copy the data from
 * @param OutBlob the buffer to copy the data into
 */
void USettings::GetSettingsDataBlob(FSettingsData& Data,TArray<BYTE>& OutBlob)
{
	Data.GetData(OutBlob);
}

/**
 * Static function for getting members of the SettingsData union
 *
 * @param Data the data structure to get the fields of
 * @param InInt1 first half of the data to get
 * @param InInt2 second half of the data to get
 */
void USettings::GetSettingsDataDateTime(FSettingsData& Data,INT& OutInt1,INT& OutInt2)
{
	Data.GetData(OutInt1,OutInt2);
}

/**
 * Searches the localized string setting array for the matching id and sets the value
 *
 * @param StringSettingId the string setting to set the value for
 * @param ValueIndex the value of the string setting
 * @param bShouldAutoAdd whether to add the string setting if it is missing
 */
void USettings::SetStringSettingValue(INT StringSettingId,INT ValueIndex,UBOOL bShouldAutoAdd)
{
	UBOOL bValueChanged = FALSE, bFound = FALSE;
	// Search for the value and set it if found
	for (INT Index = 0; Index < LocalizedSettings.Num(); Index++)
	{
		if (LocalizedSettings(Index).Id == StringSettingId)
		{
			bValueChanged = LocalizedSettings(Index).ValueIndex != ValueIndex;
			LocalizedSettings(Index).ValueIndex = ValueIndex;
			bFound = TRUE;
			break;
		}
	}
	// Add the string setting if it is missing and was requested
	if (bFound == FALSE && bShouldAutoAdd == TRUE)
	{
		INT Index = LocalizedSettings.Add(1);
		LocalizedSettings(Index).Id = StringSettingId;
		LocalizedSettings(Index).ValueIndex = ValueIndex;
		bFound = bValueChanged = TRUE;
	}
}

/**
 * Searches the localized string setting array for the matching id and returns its value
 *
 * @param StringSettingId the string setting to find the value of
 * @param ValueIndex the out value that is set when found
 *
 * @return true if found, false otherwise
 */
UBOOL USettings::GetStringSettingValue(INT StringSettingId,INT& ValueIndex)
{
	// Search for the value and return it if found
	for (INT Index = 0; Index < LocalizedSettings.Num(); Index++)
	{
		if (LocalizedSettings(Index).Id == StringSettingId)
		{
			ValueIndex = LocalizedSettings(Index).ValueIndex;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Searches the localized string setting array for the matching id and sets the selected value
 * to the next (or prev) item in the list, wrapping if required
 *
 * @param StringSettingId the string setting to find the value of
 * @param Direction the direction to move in the list (+ forward, - backward)
 * @param bShouldWrap if true out of bound access wraps around, false clamps to min/max
 *
 * @return true if found, false otherwise
 */
UBOOL USettings::IncrementStringSettingValue(INT StringSettingId,INT Direction,UBOOL bShouldWrap)
{
	// Search for the value and return it if found
	for (INT Index = 0; Index < LocalizedSettings.Num(); Index++)
	{
		if (LocalizedSettings(Index).Id == StringSettingId)
		{
			// Max entries in this mappings value array
			INT Max = 0;

			// Current mapping index for the settings id
			INT CurrentMappingIndex = 0;
			
			// Current value id being held
			INT CurrentValueId = LocalizedSettings(Index).ValueIndex;

			// Current index in the settings mappings that holds the value id
			INT CurrentValueIndex = 0;

			// Search the mappings for the id
			for (INT SettingIndex = 0; SettingIndex < LocalizedSettingsMappings.Num(); SettingIndex++)
			{
				if (LocalizedSettingsMappings(SettingIndex).Id == StringSettingId)
				{
					CurrentMappingIndex = SettingIndex;
					Max = LocalizedSettingsMappings(SettingIndex).ValueMappings.Num() - 1;

					// Now search for the value index mapping
					for (INT ValueIndex = 0; ValueIndex < LocalizedSettingsMappings(Index).ValueMappings.Num(); ValueIndex++)
					{
						if (LocalizedSettingsMappings(Index).ValueMappings(ValueIndex).Id == CurrentValueId)
						{
							CurrentValueIndex = ValueIndex;
							break;
						}
					}
					break;
				}
			}

			// Determine the new value based upon direction
			INT NewValueIndex = CurrentValueIndex + Direction;
			// Handle out of bounds cases and either clamp or wrap
			if (NewValueIndex < 0 || NewValueIndex > Max)
			{
				if (bShouldWrap)
				{
					// Wrap to top if we went negative
					if (NewValueIndex < 0)
					{
						NewValueIndex = Max;
					}
					// Otherwise wrap to the bottom
					else
					{
						NewValueIndex = 0;
					}
				}
				else
				{
					Clamp(NewValueIndex,0,Max);
				}
			}
			// Finally assign the new index
			LocalizedSettings(Index).ValueIndex = LocalizedSettingsMappings(CurrentMappingIndex).ValueMappings(NewValueIndex).Id;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Searches the localized string setting array for the matching id and
 * returns the list of possible values
 *
 * @param StringSettingId the string setting to find the value of
 * @param Values the out value that is a list of value names
 *
 * @return true if found, false otherwise
 */
UBOOL USettings::GetStringSettingValueNames(INT StringSettingId,TArray<FIdToStringMapping>& Values)
{
	// Search the mappings for the id
	for (INT Index = 0; Index < LocalizedSettingsMappings.Num(); Index++)
	{
		if (LocalizedSettingsMappings(Index).Id == StringSettingId)
		{
			// Now add all of the values to the out array
			Values.Empty(LocalizedSettingsMappings(Index).ValueMappings.Num());
			Values.AddZeroed(LocalizedSettingsMappings(Index).ValueMappings.Num());
			for (INT Index2 = 0; Index2 < LocalizedSettingsMappings(Index).ValueMappings.Num(); Index2++)
			{
				Values(Index2).Id = LocalizedSettingsMappings(Index).ValueMappings(Index2).Id;
				Values(Index2).Name = LocalizedSettingsMappings(Index).ValueMappings(Index2).Name;
			}
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Searches the localized string setting array for the matching name and sets the value
 *
 * @param StringSettingName the setting name to set the value for
 * @param ValueIndex the value of the string setting
 * @param bShouldAutoAdd whether to add the string setting if it is missing
 */
void USettings::SetStringSettingValueByName(FName StringSettingName,INT ValueIndex,UBOOL bShouldAutoAdd)
{
	INT StringSettingId;
	// Find the localized string setting id matching this name
	if (GetStringSettingId(StringSettingName,StringSettingId))
	{
		// Use the common set routine
		SetStringSettingValue(StringSettingId,ValueIndex,bShouldAutoAdd);
	}
}

/**
 * Searches the localized string setting array for the matching name and returns its value
 *
 * @param StringSettingName the setting name to find the value of
 * @param ValueIndex the out value that is set when found
 *
 * @return true if found, false otherwise
 */
UBOOL USettings::GetStringSettingValueByName(FName StringSettingName,INT& ValueIndex)
{
	INT StringSettingId;
	// Find the string setting id
	if (GetStringSettingId(StringSettingName,StringSettingId))
	{
		// Now use the get by id version
		return GetStringSettingValue(StringSettingId,ValueIndex);
	}
	return FALSE;
}

/**
 * Searches the string setting array for the matching string setting name and returns the id
 *
 * @param StringSettingName the name of the string setting being searched for
 * @param StringSettingId the id of the string setting that matches the name
 *
 * @return true if the seting was found, false otherwise
 */
UBOOL USettings::GetStringSettingId(FName StringSettingName,INT& StringSettingId)
{
	// Search for the string setting name in the mappings and return its id
	for (INT Index = 0; Index < LocalizedSettingsMappings.Num(); Index++)
	{
		if (LocalizedSettingsMappings(Index).Name == StringSettingName)
		{
			StringSettingId = LocalizedSettingsMappings(Index).Id;
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Finds the human readable name for the string setting
 *
 * @param StringSettingId the id to look up in the mappings table
 *
 * @return the name of the string setting that matches the id or NAME_None if not found
 */
FName USettings::GetStringSettingName(INT StringSettingId)
{
	// Search for the string setting id in the mappings and return its name
	for (INT Index = 0; Index < LocalizedSettingsMappings.Num(); Index++)
	{
		if (LocalizedSettingsMappings(Index).Id == StringSettingId)
		{
			return LocalizedSettingsMappings(Index).Name;
		}
	}
	return NAME_None;
}

/**
 * Finds the localized column header text for the string setting
 *
 * @param StringSettingId the id to look up in the mappings table
 *
 * @return the string to use as the list column header for the string setting that matches the id, or an empty string if not found.
 */
FString USettings::GetStringSettingColumnHeader(INT StringSettingId )
{
	FString Result;
	// Search for the string setting id in the mappings and return its localized column header
	for (INT Index = 0; Index < LocalizedSettingsMappings.Num(); Index++)
	{
		if (LocalizedSettingsMappings(Index).Id == StringSettingId)
		{
			Result = LocalizedSettingsMappings(Index).ColumnHeaderText;
			break;
		}
	}
	return Result;
}

/**
 * Finds the human readable name for a string setting's value. Searches the
 * string settings mappings for the specifc string setting and then searches
 * the set of values for the specific value index and returns that value's
 * human readable name
 *
 * @param StringSettingId the id to look up in the mappings table
 * @param ValueIndex the value index to find the string value of
 *
 * @return the name of the string setting value that matches the id & index or NAME_None if not found
 */
FName USettings::GetStringSettingValueName(INT StringSettingId,INT ValueIndex)
{
	// Search for the string setting id in the mappings
	for (INT Index = 0; Index < LocalizedSettingsMappings.Num(); Index++)
	{
		if (LocalizedSettingsMappings(Index).Id == StringSettingId)
		{
			// Now search for the value index mapping
			for (INT Index2 = 0; Index2 < LocalizedSettingsMappings(Index).ValueMappings.Num(); Index2++)
			{
				if (LocalizedSettingsMappings(Index).ValueMappings(Index2).Id == ValueIndex)
				{
					return LocalizedSettingsMappings(Index).ValueMappings(Index2).Name;
				}
			}
		}
	}
	return NAME_None;
}

/**
 * Determines if the value for the specified setting is a wildcard option
 *
 * @param StringSettingId the id to check for being a wildcard
 *
 * @return true if the current value is a wildcard, false otherwise
 */
UBOOL USettings::IsWildcardStringSetting(INT StringSettingId)
{
	// Search for the string setting id in the mappings
	for (INT Index = 0; Index < LocalizedSettingsMappings.Num(); Index++)
	{
		if (LocalizedSettingsMappings(Index).Id == StringSettingId)
		{
			INT ValueIndex = -1;
			// Get the value from the current setting
			for (INT Index2 = 0; Index2 < LocalizedSettings.Num(); Index2++)
			{
				const FLocalizedStringSetting& Setting = LocalizedSettings(Index2);
				if (StringSettingId == Setting.Id)
				{
					ValueIndex = Setting.ValueIndex;
					break;
				}
			}
			// Now search for the value index mapping and return its wildcard status
			for (INT Index2 = 0; Index2 < LocalizedSettingsMappings(Index).ValueMappings.Num(); Index2++)
			{
				if (LocalizedSettingsMappings(Index).ValueMappings(Index2).Id == ValueIndex)
				{
					return LocalizedSettingsMappings(Index).ValueMappings(Index2).bIsWildcard;
				}
			}
		}
	}
	return FALSE;
}

/**
 * Finds the specified string setting and then returns the name of its current
 * value
 *
 * @param StringSettingName the name of the string setting to find the value of
 *
 * @return the name for the value or NAME_None if not found
 */
FName USettings::GetStringSettingValueNameByName(FName StringSettingName)
{
	INT StringSettingId;
	// See if this is a known string setting
	if (GetStringSettingId(StringSettingName,StringSettingId))
	{
		// Find the string setting and return its value
		for (INT Index = 0; Index < LocalizedSettings.Num(); Index++)
		{
			if (LocalizedSettings(Index).Id == StringSettingId)
			{
				// Using the string setting's current value find the name for that
				return GetStringSettingValueName(StringSettingId,LocalizedSettings(Index).ValueIndex);
			}
		}
	}
	return NAME_None;
}

/**
 * Searches for the string setting by name and sets the value index to the
 * value contained in the string setting meta data
 *
 * @param StringSettingName the name of the string setting to find
 * @param NewValue the string value to use
 *
 * @return true if the string setting was found and the value was set, false otherwise
 */
UBOOL USettings::SetStringSettingValueFromStringByName(FName StringSettingName,
	const FString& NewValue)
{
	FName ValueName(*NewValue);
	// Find the string setting
	for (INT Index = 0; Index < LocalizedSettingsMappings.Num(); Index++)
	{
		if (LocalizedSettingsMappings(Index).Name == StringSettingName)
		{
			// Now find the value by name
			for (INT Index2 = 0;
				Index2 < LocalizedSettingsMappings(Index).ValueMappings.Num();
				Index2++)
			{
				if (LocalizedSettingsMappings(Index).ValueMappings(Index2).Name == ValueName)
				{
					// Find the string setting and set its value index
					for (INT Index3 = 0; Index3 < LocalizedSettings.Num(); Index3++)
					{
						if (LocalizedSettings(Index3).Id == LocalizedSettingsMappings(Index).Id)
						{
							// Set the string setting value index and return
							const UBOOL bValueChanged = LocalizedSettings(Index3).ValueIndex != LocalizedSettingsMappings(Index).ValueMappings(Index2).Id;
							LocalizedSettings(Index3).ValueIndex = LocalizedSettingsMappings(Index).ValueMappings(Index2).Id;
							return TRUE;
						}
					}
				}
			}
		}
	}
	return FALSE;
}

/**
 * Searches the property array for the matching property and returns the id
 *
 * @param PropertyName the name of the property being searched for
 * @param PropertyId the id of the property that matches the name
 *
 * @return true if the property was found, false otherwise
 */
UBOOL USettings::GetPropertyId(FName PropertyName,INT& PropertyId)
{
	// Search for the property name in the mappings and return its id
	for (INT Index = 0; Index < PropertyMappings.Num(); Index++)
	{
		if (PropertyMappings(Index).Name == PropertyName)
		{
			PropertyId = PropertyMappings(Index).Id;
			return TRUE;
		}
	}
	return FALSE;
}
/**
 * Finds the human readable name for the property
 *
 * @param PropertyId the id to look up in the mappings table
 *
 * @return the name of the property that matches the id or NAME_None if not found
 */
FName USettings::GetPropertyName(INT PropertyId)
{
	// Search for the property id in the mappings and return its name
	for (INT Index = 0; Index < PropertyMappings.Num(); Index++)
	{
		if (PropertyMappings(Index).Id == PropertyId)
		{
			return PropertyMappings(Index).Name;
		}
	}
	return NAME_None;
}
/**
 * Finds the localized column header text for the property
 *
 * @param PropertyId the id to look up in the mappings table
 *
 * @return the string to use as the list column header for the property that matches the id, or an empty string if not found.
 */
FString USettings::GetPropertyColumnHeader(INT PropertyId)
{
	FString Result;

	// Search for the property id in the mappings and return its name
	for (INT Index = 0; Index < PropertyMappings.Num(); Index++)
	{
		if (PropertyMappings(Index).Id == PropertyId)
		{
			Result = PropertyMappings(Index).ColumnHeaderText;
			break;
		}
	}
	return Result;
}

/**
 * Finds the specified property and converts its value to a string
 *
 * @param PropertyId the id of the property to find the value of
 *
 * @return stringized version of the property's value
 */
FString USettings::GetPropertyAsString(INT PropertyId)
{
	// Find the property and return its value
	for (INT Index = 0; Index < Properties.Num(); Index++)
	{
		if (Properties(Index).PropertyId == PropertyId)
		{
			return Properties(Index).Data.ToString();
		}
	}
	return FString();
}

/**
 * Finds the specified property and converts its value to a string
 *
 * @param PropertyName the name of the property to find the value of
 *
 * @return stringized version of the property's value
 */
FString USettings::GetPropertyAsStringByName(FName PropertyName)
{
	INT PropertyId;
	// See if this is a known property
	if (GetPropertyId(PropertyName,PropertyId))
	{
		// Find the property and return its value
		for (INT Index = 0; Index < Properties.Num(); Index++)
		{
			if (Properties(Index).PropertyId == PropertyId)
			{
				return Properties(Index).Data.ToString();
			}
		}
	}
	return FString();
}

/**
 * Searches for the property by name and sets the property to the value contained
 * in the string
 *
 * @param PropertyName the name of the property to find
 * @param NewValue the string value to use
 *
 * @return true if the property was found and the value was set, false otherwise
 */
UBOOL USettings::SetPropertyFromStringByName(FName PropertyName,const FString& NewValue)
{
	INT PropertyId;
	// See if this is a known property
	if (GetPropertyId(PropertyName,PropertyId))
	{
		// Find the property and return its value
		for (INT Index = 0; Index < Properties.Num(); Index++)
		{
			if (Properties(Index).PropertyId == PropertyId)
			{
				const UBOOL bValueChanged = Properties(Index).Data.ToString() != NewValue;
				if ( Properties(Index).Data.FromString(NewValue) )
				{
					return TRUE;
				}
				break;
			}
		}
	}
	return FALSE;
}

/**
 * Sets a property of type SDT_Float to the value specified. Does nothing
 * if the property is not of the right type.
 *
 * @param PropertyId the property to change the value of
 * @param Value the new value to assign
 */
void USettings::SetFloatProperty(INT PropertyId,FLOAT Value)
{
	FSettingsProperty* Property = FindProperty(PropertyId);
	if (Property != NULL && Property->Data.Type == SDT_Float)
	{
		// Set the value
		Property->Data.SetData(Value);
	}
}

/**
 * Sets a property of type SDT_Float to the value specified. Does nothing
 * if the property is not of the right type.
 *
 * @param PropertyId the property to change the value of
 * @param Value the out value containing the property's value
 *
 * @return true if found and is the right type, false otherwise
 */
UBOOL USettings::GetFloatProperty(INT PropertyId,FLOAT& Value)
{
	UBOOL bSet = FALSE;
	// Find the specified property
	FSettingsProperty* Property = FindProperty(PropertyId);
	if (Property != NULL && Property->Data.Type == SDT_Float)
	{
		// read the value
		Property->Data.GetData(Value);
		bSet = TRUE;
	}
	return bSet;
}

/**
 * Sets a property of type SDT_Int32 to the value specified. Does nothing
 * if the property is not of the right type.
 *
 * @param PropertyId the property to change the value of
 * @param Value the new value to assign
 */
void USettings::SetIntProperty(INT PropertyId,INT Value)
{
	FSettingsProperty* Property = FindProperty(PropertyId);
	if (Property != NULL && Property->Data.Type == SDT_Int32)
	{
		// Set the value
		Property->Data.SetData(Value);
	}
}

/**
 * Reads a property of type SDT_Int32 into the value specified. Does nothing
 * if the property is not of the right type.
 *
 * @param PropertyId the property to change the value of
 * @param Value the out value containing the property's value
 *
 * @return true if found and is the right type, false otherwise
 */
UBOOL USettings::GetIntProperty(INT PropertyId,INT& Value)
{
	UBOOL bSet = FALSE;
	// Find the specified property
	FSettingsProperty* Property = FindProperty(PropertyId);
	if (Property != NULL && Property->Data.Type == SDT_Int32)
	{
		// Read the value
		Property->Data.GetData(Value);
		bSet = TRUE;
	}
	return bSet;
}

/**
 * Sets a property of type SDT_String to the value specified. Does nothing
 * if the property is not of the right type.
 *
 * @param PropertyId the property to change the value of
 * @param Value the new value to assign
 */
void USettings::SetStringProperty(INT PropertyId,const FString& Value)
{
	// Find the specified property
	FSettingsProperty* Property = FindProperty(PropertyId);
	if (Property != NULL && Property->Data.Type == SDT_String)
	{
		// Set the value
		Property->Data.SetData(Value);
	}
}

/**
 * Reads a property of type SDT_String into the value specified. Does nothing
 * if the property is not of the right type.
 *
 * @param PropertyId the property to change the value of
 * @param Value the out value containing the property's value
 *
 * @return true if found and is the right type, false otherwise
 */
UBOOL USettings::GetStringProperty(INT PropertyId,FString& Value)
{
	UBOOL bSet = FALSE;
	// Find the specified property
	FSettingsProperty* Property = FindProperty(PropertyId);
	if (Property != NULL && Property->Data.Type == SDT_String)
	{
		// Read the value
		Property->Data.GetData(Value);
		bSet = TRUE;
	}
	return bSet;
}

/**
 * Change the current value for a mapped property's using a value id.
 *
 * @param	PropertyId	the property to change the value of
 * @param	ValueId		the id for the value to set.
 *
 * @return true if the property was found and id mapped, false otherwise
 */
UBOOL USettings::SetPropertyValueId(INT PropertyId,INT ValueId)
{
	UBOOL bResult = FALSE;

	FSettingsProperty* Property = FindProperty(PropertyId);
	if ( Property != NULL && Property->Data.Type == SDT_Int32 )
	{
		FSettingsPropertyPropertyMetaData* MetaData = FindPropertyMetaData(PropertyId);
		if ( MetaData != NULL )
		{
			if ( MetaData->MappingType == PVMT_IdMapped )
			{
				for ( INT ValueIndex = 0; ValueIndex < MetaData->ValueMappings.Num(); ValueIndex++ )
				{
					FIdToStringMapping& Mapping = MetaData->ValueMappings(ValueIndex);
					if ( Mapping.Id == ValueId )
					{
						// found it!
						Property->Data.SetData(ValueId);
						bResult = TRUE;
						break;
					}
				}
			}
			else
			{
				debugfSlow( TEXT("SetPropertyValueId did not find a valid MappingType.  PropertyId: %d ValueId: %d  MappingType: %d"), PropertyId, ValueId, MetaData->MappingType );
			}
		}
		else
		{
			debugfSlow( TEXT("SetPropertyValueId did not find a property mapping with the specified PropertyId: %d"), PropertyId);
		}
	}

	return bResult;
}

/**
 * Retrieves the id for a mapped property's current value.
 *
 * @param	PropertyId	the property to change the value of
 * @param	ValueId		receives the id of the property value
 *
 * @return true if the property was found and id mapped, false otherwise
 */
UBOOL USettings::GetPropertyValueId(INT PropertyId, INT& ValueId)
{
	UBOOL bResult = FALSE;

	// need to verify that the current value is a valid mapping id
	INT PropertyValueId;
	if ( GetIntProperty(PropertyId, PropertyValueId) )
	{
		FSettingsPropertyPropertyMetaData* MetaData = FindPropertyMetaData(PropertyId);
		if ( MetaData != NULL )
		{
			if ( MetaData->MappingType == PVMT_IdMapped )
			{
				for ( INT ValueIndex = 0; ValueIndex < MetaData->ValueMappings.Num(); ValueIndex++ )
				{
					FIdToStringMapping& Mapping = MetaData->ValueMappings(ValueIndex);
					if ( Mapping.Id == PropertyValueId )
					{
						// found it!
						ValueId = PropertyValueId;
						bResult = TRUE;
						break;
					}
				}
			}
			else
			{
				debugfSlow( TEXT("GetPropertyValueId did not find a valid MappingType.  PropertyId: %d MappingType: %d"), PropertyId, MetaData->MappingType );
			}
		}
		else
		{
			debugfSlow( TEXT("GetPropertyValueId did not find a property mapping with the specified PropertyId: %d"), PropertyId);
		}
	}
	else
	{
		debugfSlow( TEXT("GetPropertyValueId did not find a property with the specified PropertyId: %d"), PropertyId);
	}

	return bResult;
}

/**
 * Determines the property type for the specified property id
 *
 * @param PropertyId the property to change the value of
 *
 * @return the type of property, or SDT_Empty if not found
 */
BYTE USettings::GetPropertyType(INT PropertyId)
{
	// Find the specified property
	FSettingsProperty* Property = FindProperty(PropertyId);
	if (Property != NULL)
	{
		return Property->Data.Type;
	}
	return SDT_Empty;
}

/**
 * Using the specified array, updates the matching settings to the new values
 * in that array. Optionally, it will add settings that aren't currently part
 * of this object.
 *
 * @param Settings the list of settings to update
 * @param bShouldAddIfMissing whether to automatically add the setting if missing
 */
void USettings::UpdateStringSettings(const TArray<FLocalizedStringSetting>& Settings,
	UBOOL bShouldAddIfMissing)
{
	// Iterate the list and update/add accordingly
	for (INT Index = 0; Index < Settings.Num(); Index++)
	{
		const FLocalizedStringSetting& SourceSetting = Settings(Index);
		// Search the array for the setting
		FLocalizedStringSetting* Setting = FindStringSetting(SourceSetting.Id);
		if (Setting != NULL)
		{
			// Update it to the new value
			*Setting = SourceSetting;
		}
		else if (bShouldAddIfMissing)
		{
			// Add and copy
			INT AddIndex = LocalizedSettings.AddZeroed();
			LocalizedSettings(AddIndex) = SourceSetting;
		}
	}
}

/**
 * Using the specified array, updates the matching properties to the new values
 * in that array. Optionally, it will add properties that aren't currently part
 * of this object.
 *
 * @param Props the list of properties to update
 * @param bShouldAddIfMissing whether to automatically add the property if missing
 */
void USettings::UpdateProperties(const TArray<FSettingsProperty>& Props,
	UBOOL bShouldAddIfMissing)
{
	// Iterate the list and update/add accordingly
	for (INT Index = 0; Index < Props.Num(); Index++)
	{
		const FSettingsProperty& SourceProp = Props(Index);
		// Search the array for the property
		FSettingsProperty* DestProp = FindProperty(SourceProp.PropertyId);
		if (DestProp != NULL)
		{
			// Update it to the new value
			*DestProp = SourceProp;
		}
		else if (bShouldAddIfMissing)
		{
			// Add and copy
			INT AddIndex = Properties.AddZeroed();
			Properties(AddIndex) = SourceProp;
		}
	}
}

/**
 * Determines if a given property is present for this object
 *
 * @param PropertyId the ID to check on
 *
 * @return TRUE if the property is part of this object, FALSE otherwise
 */
UBOOL USettings::HasProperty(INT PropertyId)
{
	return FindProperty(PropertyId) != NULL;
}

/**
 * Determines if a given localized string setting is present for this object
 *
 * @param SettingId the ID to check on
 *
 * @return TRUE if the setting is part of this object, FALSE otherwise
 */
UBOOL USettings::HasStringSetting(INT SettingId)
{
	return FindStringSetting(SettingId) != NULL;
}

/**
 * Determines the mapping type for the specified property
 *
 * @param PropertyId the ID to get the mapping type for
 * @param OutType the out var the value is placed in
 *
 * @return TRUE if found, FALSE otherwise
 */
UBOOL USettings::GetPropertyMappingType(INT PropertyId,BYTE& OutType)
{
	// Check for the property mapping
	FSettingsPropertyPropertyMetaData* MetaData = FindPropertyMetaData(PropertyId);
	if (MetaData != NULL)
	{
		OutType = MetaData->MappingType;
		return TRUE;
	}
	return FALSE;
}

/**
 * Determines the min and max values of a property that is clamped to a range
 *
 * @param PropertyId the ID to get the mapping type for
 * @param OutMinValue the out var the min value is placed in
 * @param OutMaxValue the out var the max value is placed in
 * @param RangeIncrement the amount the range can be adjusted by the UI in any single update
 * @param bOutFormatAsInt whether the range's value should be treated as an int.
 *
 * @return TRUE if found and is a range property, FALSE otherwise
 */
UBOOL USettings::GetPropertyRange(INT PropertyId,FLOAT& OutMinValue,FLOAT& OutMaxValue,FLOAT& OutRangeIncrement,BYTE& bOutFormatAsInt)
{
	UBOOL bFound = FALSE;
	// Check for the property mapping
	FSettingsPropertyPropertyMetaData* MetaData = FindPropertyMetaData(PropertyId);
	FSettingsProperty* Property = FindProperty(PropertyId);
	// And make sure it's a ranged property
	if (MetaData != NULL && MetaData->MappingType == PVMT_Ranged && Property != NULL)
	{
		OutRangeIncrement = MetaData->RangeIncrement;
		OutMinValue = MetaData->MinVal;
		OutMaxValue = MetaData->MaxVal;
		bOutFormatAsInt = Property->Data.Type == SDT_Int32;
		bFound = TRUE;
	}
	return bFound;
}

/**
 * Sets the value of a ranged property, clamping to the min/max values
 *
 * @param PropertyId the ID of the property to set
 * @param NewValue the new value to apply to the 
 *
 * @return TRUE if found and is a range property, FALSE otherwise
 */
UBOOL USettings::SetRangedPropertyValue(INT PropertyId,FLOAT NewValue)
{
	UBOOL bFound = FALSE;
	FLOAT MinValue, MaxValue, Increment;
	BYTE bFormatAsInt;
	// Read the min/max for the property id so we can clamp
	if (GetPropertyRange(PropertyId,MinValue,MaxValue,Increment,bFormatAsInt))
	{
		// Clamp within the valid range
		NewValue = Clamp<FLOAT>(NewValue,MinValue,MaxValue);
		if ( bFormatAsInt )
		{
			NewValue = appTrunc(NewValue);
		}
		// Get the property that we are going to set
		FSettingsProperty* Prop = FindProperty(PropertyId);
		check(Prop && "Missing property that has a meta data entry");
		// Now set the value
		switch (Prop->Data.Type)
		{
			case SDT_Int32:
			{
				INT Value = appTrunc(NewValue);
				Prop->Data.SetData(Value);
				bFound = TRUE;
				break;
			}
			case SDT_Float:
			{
				Prop->Data.SetData(NewValue);
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
 * @param PropertyId the ID to get the value of
 * @param OutValue the out var that receives the value
 *
 * @return TRUE if found and is a range property, FALSE otherwise
 */
UBOOL USettings::GetRangedPropertyValue(INT PropertyId,FLOAT& OutValue)
{
	// Get the property that we are going to set
	FSettingsProperty* Prop = FindProperty(PropertyId);
	if (Prop)
	{
		// Now set the value
		switch (Prop->Data.Type)
		{
			case SDT_Int32:
			{
				INT Value;
				Prop->Data.GetData(Value);
				OutValue = Value;
				break;
			}
			case SDT_Float:
			{
				Prop->Data.GetData(OutValue);
				break;
			}
			default:
			{
				Prop = NULL;
				break;
			}
		}
	}
	return Prop != NULL;
}

/**
 * Scans the properties for the ones that need to be set via QoS data
 *
 * @param QoSProps the out array holding the list of properties to advertise via QoS
 */
void USettings::GetQoSAdvertisedProperties(TArray<FSettingsProperty>& QoSProps)
{
	for (INT Index = 0; Index < Properties.Num(); Index++)
	{
		// If this is to be advertised via QoS, copy it to the out array
		if (Properties(Index).AdvertisementType == ODAT_QoS ||
			Properties(Index).AdvertisementType == ODAT_OnlineServiceAndQoS)
		{
			QoSProps.AddItem(Properties(Index));
		}
	}
}

/**
 * Scans the string settings for the ones that need to be set via QoS data
 *
 * @param QoSSettings the out array holding the list of settings to advertise via QoS
 */
void USettings::GetQoSAdvertisedStringSettings(TArray<FLocalizedStringSetting>& QoSSettings)
{
	for (INT Index = 0; Index < LocalizedSettings.Num(); Index++)
	{
		// If this is to be advertised via QoS, copy it to the out array
		if (LocalizedSettings(Index).AdvertisementType == ODAT_QoS ||
			LocalizedSettings(Index).AdvertisementType == ODAT_OnlineServiceAndQoS)
		{
			QoSSettings.AddItem(LocalizedSettings(Index));
		}
	}
}

/**
 * Appends databindings to the URL.
 *
 * @param OutURL	String to append bindings to.
 */
void USettings::AppendDataBindingsToURL(FString& URL)
{
	// Use the databinding flags to expose properties to the URL
	for (UProperty* Property = GetClass()->PropertyLink;
		Property != NULL;
		Property = Property->PropertyLinkNext)
	{
		BYTE* ValueAddress = (BYTE*)this + Property->Offset;
		// If the property is databindable and is not an object, use ExportText()
		// to append to the URL
		if ((Property->PropertyFlags & CPF_DataBinding) != 0 &&
			Cast<UObjectProperty>(Property,CLASS_IsAUObjectProperty) == NULL)
		{
			FString StringValue;
			// Use the property code to export to text
			Property->ExportTextItem(StringValue,ValueAddress,NULL,this,
				(Property->PropertyFlags & CPF_Localized) ? PPF_Localized : 0);
			UStrProperty* StrProp = Cast<UStrProperty>(Property);
			if (StrProp == NULL ||
				// Skip any strings that have spaces in them
				(StrProp != NULL && appStrrchr(*StringValue,TEXT(' ')) == NULL))
			{
				URL += FString::Printf(TEXT("?%s=%s"),
					*Property->GetName(),
					*StringValue);
			}
		}
	}
}

/**
 * Appends properties to the URL.
 *
 * @param OutURL	String to append properties to.
 */
void USettings::AppendPropertiesToURL(FString& URL)
{
	// Now add all properties the same way
	for (INT Index = 0; Index < Properties.Num(); Index++)
	{
		FName PropertyName = GetPropertyName(Properties(Index).PropertyId);
		if (PropertyName != NAME_None)
		{
			URL += FString::Printf(TEXT("?%s=%s"),
				*PropertyName.ToString(),
				*Properties(Index).Data.ToString());
		}
	}
}

/**
 * Appends contexts to the URL.
 *
 * @param OutURL	String to append contexts to.
 */
void USettings::AppendContextsToURL(FString& URL)
{
	// Iterate through localized settings and append them
	for (INT Index = 0; Index < LocalizedSettings.Num(); Index++)
	{
		FName SettingName = GetStringSettingName(LocalizedSettings(Index).Id);
		if (SettingName != NAME_None)
		{
			URL += FString::Printf(TEXT("?%s=%d"),
				*SettingName.ToString(),
				LocalizedSettings(Index).ValueIndex);
		}
	}
}

/**
 * Builds an URL out of the string settings and properties
 *
 * @param URL the string to populate
 */
void USettings::BuildURL(FString& URL)
{
	URL.Empty();
	
	AppendDataBindingsToURL(URL);
	AppendContextsToURL(URL);
	AppendPropertiesToURL(URL);
}

/**
 * Updates the game settings object from parameters passed on the URL
 *
 * @param URL the URL to parse for settings
 */
void USettings::UpdateFromURL(const FString& URL, AGameInfo* Game)
{
	FURL ParsedUrl(NULL,*URL,TRAVEL_Absolute);
	// Use the databinding flags to expose properties to the URL
	for (UProperty* Property = GetClass()->PropertyLink;
		Property != NULL;
		Property = Property->PropertyLinkNext)
	{
		// If the property is databindable and is not an object, use ImportText()
		// to set the property to the value specified by the URL
		if ((Property->PropertyFlags & CPF_DataBinding) != 0 &&
			Cast<UObjectProperty>(Property,CLASS_IsAUObjectProperty) == NULL)
		{
			const FString& PropName = Property->GetName();
			// If the URL has this property listed, read its value
			if (ParsedUrl.HasOption(*PropName))
			{
				BYTE* ValueAddress = (BYTE*)this + Property->Offset;
				// Get the option from the URL
				const TCHAR* UrlValue = ParsedUrl.GetOption(*PropName,TEXT(""));
				// Skip the = if there
				if (*UrlValue == TEXT('='))
				{
					UrlValue++;
				}
				// Use the property code to import the text into the property
				Property->ImportText(UrlValue,ValueAddress,PPF_Localized,this);
			}
		}
	}
	// Iterate through localized settings and see if the URL contains them
	for (INT Index = 0; Index < LocalizedSettings.Num(); Index++)
	{
		FName SettingName = GetStringSettingName(LocalizedSettings(Index).Id);
		// If the URL has this setting specified, set it
		if (ParsedUrl.HasOption(*SettingName.ToString()))
		{
			// Get the option from the URL
			const TCHAR* UrlValue = ParsedUrl.GetOption(*SettingName.ToString(),TEXT("0"));
			// Skip the = if there
			if (*UrlValue == TEXT('='))
			{
				UrlValue++;
			}
			LocalizedSettings(Index).ValueIndex = appAtoi(UrlValue);
		}
	}
	// Now read all of the properties from the URL the same way
	for (INT Index = 0; Index < Properties.Num(); Index++)
	{
		FName PropertyName = GetPropertyName(Properties(Index).PropertyId);
		// If the URL has this property specified, read it
		if (ParsedUrl.HasOption(*PropertyName.ToString()))
		{
			// Get the option from the URL
			const TCHAR* UrlValue = ParsedUrl.GetOption(*PropertyName.ToString(),TEXT("0"));
			// Skip the = if there
			if (*UrlValue == TEXT('='))
			{
				UrlValue++;
			}
			Properties(Index).Data.FromString(UrlValue);
		}
	}
}

/** Finalize the clean up process */
void USettings::FinishDestroy(void)
{
	Properties.Empty();
	Super::FinishDestroy();
}

