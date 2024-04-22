/**
 * Game resource data stores provide access to available game resources, such as the available gametypes, maps, or mutators
 * The data for each type of game resource is provided through a data provider and is specified in the .ini file for that
 * data provider class type using the PerObjectConfig paradigm.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_GameResource extends UIDataStore
	native(inherit)
	Config(Game);

struct native GameResourceDataProvider
{
	/** the tag that is used to access this provider, i.e. Players, Teams, etc. */
	var	config		name							ProviderTag;

	/** the name of the class associated with this data provider */
	var	config		string							ProviderClassName;

	/**
	 * indicates that each provider instance should be displayed as an element in the list of tags, rather than
	 * displaying the ProviderTag itself
	 */
	var	config		bool							bExpandProviders;

	/** the UIDataProvider class that exposes the data for this data field tag */
	var	transient	class<UIResourceDataProvider>	ProviderClass;
};

/** the list of data providers supported by this data store that correspond to list element data */
var	config								array<GameResourceDataProvider>		ElementProviderTypes;

/** collection of list element provider instances that are associated with each ElementProviderType */
var	const	private	native	transient	MultiMap_Mirror						ListElementProviders{TMultiMap<FName,class UUIResourceDataProvider*>};

cpptext
{
	/* === UUIDataStore_GameResource interface === */
	/**
	 * Finds or creates the UIResourceDataProvider instances referenced by ElementProviderTypes, and stores the result
	 * into the ListElementProvider map.
	 */
	virtual void InitializeListElementProviders();

	/* === UIDataStore interface === */
	/**
	 * Loads the classes referenced by the ElementProviderTypes array.
	 */
	virtual void LoadDependentClasses();

	/**
	 * Called when this data store is added to the data store manager's list of active data stores.
	 *
	 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
	 *							associated with a particular player; NULL if this is a global data store.
	 */
	virtual void OnRegister( ULocalPlayer* PlayerOwner );

	/* === UObject interface === */
	/** Required since maps are not yet supported by script serialization */
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );
	virtual void Serialize( FArchive& Ar );

	/**
	 * Called from ReloadConfig after the object has reloaded its configuration data.  Reinitializes the collection of list element providers.
	 */
	virtual void PostReloadConfig( UProperty* PropertyThatWasLoaded );

	/**
	 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
	 * to have natively serialized property values included in things like diffcommandlet output.
	 *
	 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
	 *								the property and the map's value should be the textual representation of the property's value.  The property value should
	 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
	 *								as the delimiter between elements, etc.)
	 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
	 *
	 * @return	return TRUE if property values were added to the map.
	 */
	virtual UBOOL GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags=0 ) const;
}

/**
 * Finds the index for the GameResourceDataProvider with a tag matching ProviderTag.
 *
 * @return	the index into the ElementProviderTypes array for the GameResourceDataProvider element that has the
 *			tag specified, or INDEX_NONE if there are no elements of the ElementProviderTypes array that have that tag.
 */
native final function int FindProviderTypeIndex( name ProviderTag ) const;

/**
 * Get the UIResourceDataProvider instances associated with the tag.
 *
 * @param	ProviderTag		the tag to find instances for; should match the ProviderTag value of an element in the ElementProviderTypes array.
 * @param	out_Providers	receives the list of provider instances. this array is always emptied first.
 *
 * @return	the list of UIResourceDataProvider instances registered for ProviderTag.
 */
native final function bool GetResourceProviders( name ProviderTag, out array<UIResourceDataProvider> out_Providers ) const;

DefaultProperties
{
	Tag=GameResources
}
