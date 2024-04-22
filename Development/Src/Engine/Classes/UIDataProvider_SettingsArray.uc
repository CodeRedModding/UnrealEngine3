/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for exposing string settings as arrays to the ui
 */
class UIDataProvider_SettingsArray extends UIDataProvider
	native(inherit)
	DependsOn(Settings)
	transient;

/** Holds the settings object that will be exposed to the UI */
var Settings Settings;

/** The settings id this provider is responsible for managing */
var int SettingsId;

/** Cache for faster compares */
var name SettingsName;

/**
 * string to use in list column headers for this setting; assigned from the ColumnHeaderText property for the corresponding
 * property or setting from the Settings object.
 */
var	const	string	ColumnHeaderText;

/** Cached set of possible values for this array */
var array<Settings.IdToStringMapping> Values;

cpptext
{
	/**
	 * Binds the new settings object and id to this provider.
	 *
	 * @param NewSettings the new object to bind
	 * @param NewSettingsId the id of the settings array to expose
	 *
	 * @return TRUE if the call worked, FALSE otherwise
	 */
	UBOOL BindStringSetting(USettings* NewSettings,INT NewSettingsId);

	/**
	 * Binds the property id as an array item. Requires that the property
	 * has a mapping type of PVMT_PredefinedValues
	 *
	 * @param NewSettings the new object to bind
	 * @param PropertyId the id of the property to expose as an array
	 *
	 * @return TRUE if the call worked, FALSE otherwise
	 */
	UBOOL BindPropertySetting(USettings* NewSettings,INT PropertyId);
}

defaultproperties
{
}
