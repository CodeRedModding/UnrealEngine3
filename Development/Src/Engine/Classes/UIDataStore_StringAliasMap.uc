/**
 * This datastore allows games to map aliases to strings that may change based on the current platform or language setting.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 */
class UIDataStore_StringAliasMap extends UIDataStore_StringBase
	native(inherit)
	Config(Game);

/** Struct to store the field values and how they map to localized strings */
struct native UIMenuInputMap
{
	/** the name of the input alias; i.e. Accept, Cancel, Conditional1, etc. */
	var name FieldName;

	/**
	 * Name of the platform type this mapping is associated with.  Valid values are PC, 360, and PS3.
	 */
	var name Set;

	/**
	 * The actual markup string corresponding to this alias's letter in [usually] a button font
	 */
	var string MappedText;
};

/** Array of input string mappings for use in the front end. */
var config array<UIMenuInputMap> MenuInputMapArray;

/** collection of list element provider instances that are associated with each ElementProviderType */
var	const	private	native	transient	Map_Mirror		MenuInputSets{TMap<FName, TMap<FName, INT> >};

/** The index [into the Engine.GamePlayers array] for the player that this data store provides settings for. */
var	const transient int PlayerIndex;

cpptext
{
	/* === UIDataProvider interface === */
protected:


	/**
	* Called when this data store is added to the data store manager's list of active data stores.
	*
	* @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
	*							associated with a particular player; NULL if this is a global data store.
	*/
	virtual void OnRegister( class ULocalPlayer* PlayerOwner );
}

/**
 * Returns a reference to the ULocalPlayer that this PlayerSettingsProvdier provider settings data for
 */
native final function LocalPlayer GetPlayerOwner() const;

/**
 * Attempts to find a mapping index given a field name.
 *
 * @param FieldName		Fieldname to search for.
 *
 * @return Returns the index of the mapping in the mapping array, otherwise INDEX_NONE if the mapping wasn't found.
 */
native final function int FindMappingWithFieldName( optional String FieldName="", optional String SetName="" );

/**
 * Set MappedString to be the localized string using the FieldName as a key
 * Returns the index into the mapped string array of where it was found.
 */
native virtual function int GetStringWithFieldName( String FieldName, out String MappedString );

DefaultProperties
{
	Tag=StringAliasMap
	PlayerIndex=INDEX_NONE
}
