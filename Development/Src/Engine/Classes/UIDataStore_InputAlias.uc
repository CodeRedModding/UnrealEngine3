/**
 * This datastore provides aliases for input keys.  These aliases allow gameplay code to be decoupled from actual input key
 * names (which can change based on platform or language) by storing the association between a gameplay concept or event
 * (such as "Jump") with the name of the input key which should trigger that event (such as LeftMouseButton) in a way that
 * can be easily customized for different platforms and/or languages, without the need to touch gameplay code.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_InputAlias extends UIDataStore_StringBase
	native(inherit)
	config(Input);

/**
 * Stores an input key name (and optional modifier keys) and a button icon markup string.
 */
struct native UIInputKeyData
{
	/**
	 * The name of the actual input key (LeftMouseButton) and optional modifiers (Ctrl, Alt, Shift).
	 */
	var	config	RawInputKeyEventData	InputKeyData;

	/**
	 * A string containing data store markup for this key's graph from an icon font.  Can refer to the button icon
	 * directly (such as <Fonts:GamepadKeyFont>X<Fonts:/>), or cases where the button itself might be different in another
	 * language (e.g. Circle and Square are swapped on PS3 in Japan) can refer to a localized string containing the button
	 * markup (i.e. <Strings:UILocFile.ButtonIcons.CircleButton>).
	 */
	var	config	string					ButtonFontMarkupString;
};

/**
 * Defines a single input alias (i.e. Accept) along with the raw input keys for each platform which should activate that alias.
 */
struct native UIDataStoreInputAlias
{
	/**
	 * The name of the alias which will be referenced by the game (i.e. Accept, Cancel, ShiftUp, etc.).
	 */
	var	config	name			AliasName;

	/**
	 * Input keys associated with this alias, per platform.
	 */
	var	config	UIInputKeyData	PlatformInputKeys[EInputPlatformType.IPT_MAX];
};

/*
TODO
	- do we need to provide access to the owning player (to determine whether they're using a gamepad or not) or should this
		be handled elsewhere.
*/
/**
 * Defines the list of supported aliases and their associated input keys.
 */
var	protected{protected}	config	array<UIDataStoreInputAlias>		InputAliases;

/**
 * Mapping of input alias name => index into the InputAliases array for the UIDataStoreInputAlias associated with that
 * input alias.  Provides a way to quickly access the input key data for an input alias without linear searching.
 * Generated when the data store is registered.
 */
var	protected{protected}	const	transient	native	map{FName,INT}	InputAliasLookupMap;

cpptext
{
	/* === UUIDataStore_InputAlias interface === */
	/**
	 * Populates the InputAliasLookupMap based on the elements of the InputAliases array.
	 */
	void InitializeLookupMap();

	/**
	 * @return	the platform that should be used (by default) when retrieving data associated with input aliases
	 */
	EInputPlatformType GetDefaultPlatform() const;

	/* === UUIDataStore interface === */
	/**
	 * Hook for performing any initialization required for this data store.
	 *
	 * This version builds the InputAliasLookupMap based on the elements in the InputAliases array.
	 */
	virtual void InitializeDataStore();


protected:

}

/**
 * Retrieves the button icon font markup string for an input alias
 *
 * @param	DesiredAlias		the name of the alias (i.e. Accept) to get the markup string for
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the markup string for the button icon associated with the alias.
 */
native final function string GetAliasFontMarkup( name DesiredAlias, optional EInputPlatformType OverridePlatform=IPT_MAX ) const;
/**
 * Retrieves the button icon font markup string for an input alias
 *
 * @param	AliasIndex			the index [into the InputAliases array] for the alias to get the markup string for.
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the markup string for the button icon associated with the alias.
 */
native final function string GetAliasFontMarkupByIndex( int AliasIndex, optional EInputPlatformType OverridePlatform=IPT_MAX ) const;

/**
 * Retrieves the associated input key name for an input alias
 *
 * @param	AliasIndex			the index [into the InputAliases array] for the alias to get the input key for.
 * @param	OverridePlatform	specifies which platform's input key is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the name of the input key (i.e. LeftMouseButton) which triggers the alias.
 */
native final function name GetAliasInputKeyName( name DesiredAlias, optional EInputPlatformType OverridePlatform=IPT_MAX ) const;
/**
 * Retrieves the associated input key name for an input alias
 *
 * @param	AliasIndex			the index [into the InputAliases array] for the alias to get the input key for.
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the name of the input key (i.e. LeftMouseButton) which triggers the alias.
 */
native final function name GetAliasInputKeyNameByIndex( int AliasIndex, optional EInputPlatformType OverridePlatform=IPT_MAX ) const;

/**
 * Retrieves both the input key name and modifier keys for an input alias
 *
 * @param	DesiredAlias		the name of the alias (i.e. Accept) to get the input key data for
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the struct containing the input key name and modifier keys associated with the alias.
 */
native final function bool GetAliasInputKeyData( out RawInputKeyEventData out_InputKeyData, name DesiredAlias, optional EInputPlatformType OverridePlatform=IPT_MAX ) const;
/**
 * Retrieves both the input key name and modifier keys for an input alias
 *
 * @param	AliasIndex			the index [into the InputAliases array] for the alias to get the input key data for.
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the struct containing the input key name and modifier keys associated with the alias.
 */
native final function bool GetAliasInputKeyDataByIndex( out RawInputKeyEventData out_InputKeyData, int AliasIndex, optional EInputPlatformType OverridePlatform=IPT_MAX ) const;

/**
 * Finds the location [in the InputAliases array] for an input alias.
 *
 * @param	DesiredAlias	the name of the alias (i.e. Accept) to find
 *
 * @return	the index into the InputAliases array for the alias, or INDEX_NONE if it doesn't exist.
 */
native final function int FindInputAliasIndex( name DesiredAlias ) const;

/**
 * Determines whether an input alias is supported on a particular platform.
 *
 * @param	DesiredAlias		the name of the alias (i.e. Accept) to check
 * @param	DesiredPlatform		the platform to check for an input key
 *
 * @return	TRUE if the alias has a corresponding input key for the specified platform.
 */
native final function bool HasAliasMappingForPlatform( name DesiredAlias, EInputPlatformType DesiredPlatform ) const;

DefaultProperties
{
	Tag=ButtonCallouts
}
