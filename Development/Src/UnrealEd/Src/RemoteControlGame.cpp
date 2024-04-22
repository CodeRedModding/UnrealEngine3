/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlGame.h"

FRemoteControlGame::FRemoteControlGame()
{
}

FRemoteControlGame::~FRemoteControlGame()
{
}

/**
 * Toggles the named stat group.
 */
void FRemoteControlGame::ToggleStat(const TCHAR *StatGroup)
{
	const FString StrCommand( FString::Printf(TEXT("STAT %s"), StatGroup) );
	ExecConsoleCommand( *StrCommand );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Convenience SetObjectProperty wrappers.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Convenience function for setting a UBOOL property. Calls SetObjectProperty.
 */
UBOOL FRemoteControlGame::SetObjectBoolProperty(const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName, UBOOL Value)
{
	FString str;
	if(Value)
	{
		str = TEXT("True");
	}
	else
	{
		str = TEXT("False");
	}

	return SetObjectProperty(ClassName, PropertyName, ObjectName, *str);
}

/**
 * Convenience function for setting an INT property. Calls SetObjectProperty.
 */
UBOOL FRemoteControlGame::SetObjectIntProperty(const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName, INT Value)
{
	const FString StrValue( FString::Printf( TEXT("%i"), Value ) );
	return SetObjectProperty( ClassName, PropertyName, ObjectName, *StrValue );
}

/**
 * Convenience function for setting a FLOAT property. Calls SetObjectProperty.
 */
UBOOL FRemoteControlGame::SetObjectFloatProperty(const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName, FLOAT Value)
{
	const FString StrValue( FString::Printf( TEXT("%f"), Value ) );
	return SetObjectProperty( ClassName, PropertyName, ObjectName, *StrValue );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Convenience GetObjectProperty wrappers.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Convenience function for getting a UBOOL property.  Calls GetObjectProperty.
 */
UBOOL FRemoteControlGame::GetObjectBoolProperty(UBOOL &bValue, const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName)
{
	FString str;
	if( GetObjectProperty(str, ClassName, PropertyName, ObjectName) )
	{
		if( str == TEXT("True") )
		{
			bValue = TRUE;
		}
		else
		{
			bValue = FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/**
 * Convenience function for getting an INT property.  Calls GetObjectProperty.
 */
UBOOL FRemoteControlGame::GetObjectIntProperty(INT &OutValue, const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName)
{
	FString str;
	if(GetObjectProperty(str, ClassName, PropertyName, ObjectName))
	{
		const TCHAR *Buffer = *str;
		if( *Buffer=='-' || (*Buffer>='0' && *Buffer<='9') )
		{
			OutValue = appAtoi( Buffer );			
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Convenience function for getting a FLOAT property.  Calls GetObjectProperty.
 */
UBOOL FRemoteControlGame::GetObjectFloatProperty(FLOAT &OutValue, const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName)
{
	FString str;
	if(GetObjectProperty(str, ClassName, PropertyName, ObjectName))
	{
		const TCHAR *Buffer = *str;
		if ( *Buffer == '+' || *Buffer == '-' || *Buffer == '.' || (*Buffer >= '0' && *Buffer <= '9') )
		{
			// only import this value if Buffer is numeric
			OutValue = appAtof(Buffer);
			return TRUE;
		}
	}
	return FALSE;
}
