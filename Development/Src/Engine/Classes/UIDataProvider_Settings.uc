/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for mapping properties in an Settings
 * object to something that the UI system can consume.
 */
class UIDataProvider_Settings extends UIPropertyDataProvider
	native(inherit)
	transient;

/** Holds the settings object that will be exposed to the UI */
var Settings Settings;

/** Whether this provider is a row in a list (removes array handling) */
var bool bIsAListRow;

defaultproperties
{
}
