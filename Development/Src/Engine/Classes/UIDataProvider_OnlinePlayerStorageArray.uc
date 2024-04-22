/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This class is responsible for exposing string settings as arrays to the ui
 */
class UIDataProvider_OnlinePlayerStorageArray extends UIDataProvider
	native(inherit)
	transient;

/** Holds the storage object that will be exposed to the UI */
var OnlinePlayerStorage PlayerStorage;

/** The settings id this provider is responsible for managing */
var int PlayerStorageId;

/**
 * string to use in list column headers for this setting; assigned from the ColumnHeaderText property for the corresponding
 * property or setting from the Settings object.
 */
var	const	string	ColumnHeaderText;

/** Cached set of possible values for this array */
var array<name> Values;

cpptext
{
	/**
	 * Binds the new storage object and id to this provider.
	 *
	 * @param NewStorage the new object to bind
	 * @param NewPlayerStorageId the id of the settings array to expose
	 *
	 * @return TRUE if the call worked, FALSE otherwise
	 */
	UBOOL BindStringSetting(UOnlinePlayerStorage* NewStorage,INT NewPlayerStorageId);

	/**
	 * Binds the new storage object and id to this provider.
	 *
	 * @param NewStorage the new object to bind
	 * @param PropertyId the id of the settings array to expose
	 *
	 * @return TRUE if the call worked, FALSE otherwise
	 */
	UBOOL BindPropertySetting(UOnlinePlayerStorage* NewStorage,INT PropertyId);
}

defaultproperties
{
}
