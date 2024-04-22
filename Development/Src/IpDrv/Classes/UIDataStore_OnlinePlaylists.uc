/**
 * Data store provides access to available playlist resources
 * The data for each playlist is provided through a data provider and is specified in the .ini file for that
 * data provider class type using the PerObjectConfig paradigm.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_OnlinePlaylists extends UIDataStore
	native(inherit)
	Config(Game);

/** Constants used for accessing the data providers */
const RANKEDPROVIDERTAG		= "PlaylistsRanked";
const UNRANKEDPROVIDERTAG	= "PlaylistsUnranked";
const RECMODEPROVIDERTAG	= "PlaylistsRecMode";
const PRIVATEPROVIDERTAG    = "PlaylistsPrivate";

/** Name of provider class associated with this data store (uses them and all child classes) */
var config string ProviderClassName;

/** Class reference for the above provider class name */
var	transient	class<UIResourceDataProvider>	ProviderClass;

/** Cached array of perobjectconfig data providers for playlists determined to be "ranked" */
var const array<UIResourceDataProvider> RankedDataProviders;

/** Cached array of perobjectconfig data providers for playlists determined to be "unranked" */
var const array<UIResourceDataProvider> UnrankedDataProviders;

/** Cached array of perobjectconfig data providers for playlists determined to be "recreational mode" */
var const array<UIResourceDataProvider> RecModeDataProviders;

/** Cached array of perobjectconfig data providers for playlists determined to be "private" */
var const array<UIResourceDataProvider> PrivateDataProviders;

/** The playlist to query about match details */
var OnlinePlaylistManager PlaylistMan;

cpptext
{
	/* === UUIDataStore_GameResource interface === */
	/**
	 * Finds or creates the UIResourceDataProvider instances used by online playlists, and stores the result by ranked or unranked provider types
	 */
	virtual void InitializeListElementProviders();

	/* === UIDataStore interface === */
	/**
	 * Calls the script event so that it can access the playlist manager
	 */
	virtual void InitializeDataStore()
	{
		Super::InitializeDataStore();
		eventInit();
	}

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
 * Grabs the playlist manager
 */
event Init()
{
	local OnlineSubsystem OnlineSub;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();

	// Only download if we have a logged in player
	if (OnlineSub != None &&
		OnlineSub.Patcher != None)
	{
		PlaylistMan = OnlinePlaylistManager(OnlineSub.GetNamedInterface('PlaylistManager'));
	}
}

/**
 * Get the UIResourceDataProvider instances associated with the tag.
 *
 * @param	ProviderTag		the tag to find instances for; should match the ProviderTag value of an element in the ElementProviderTypes array.
 * @param	out_Providers	receives the list of provider instances. this array is always emptied first.
 *
 * @return	the list of UIResourceDataProvider instances registered for ProviderTag.
 */
native final function bool GetResourceProviders( name ProviderTag, out array<UIResourceDataProvider> out_Providers ) const;

/**
 * Searches for resource provider instance given its associated provider tag and an index within that subset
 *
 * @param	ProviderTag			the name of the provider type; should match the ranked or unranked provider tag
 * @param	SearchField			the index of the provider within the provider subset specified by the ProviderTag
 * @param	out_Provider		the resource provider instance found
 *
 * @return	true if the out_Provider has been found, false otherwise
 */
native final function bool GetPlaylistProvider( name ProviderTag, int ProviderIndex, out UIResourceDataProvider out_Provider);


/** Returns the OnlinePlaylistProvider with the corresponding PlaylistId */
static function OnlinePlaylistProvider GetOnlinePlaylistProvider( name ProviderTag, int PlaylistId, optional out int ProviderIndex )
{
	local UIDataStore_OnlinePlaylists PlaylistDS;
	local array<UIResourceDataProvider> Providers;
	local OnlinePlaylistProvider OPP;

	ProviderIndex = INDEX_NONE;
	PlaylistDS = UIDataStore_OnlinePlaylists(class'UIRoot'.static.StaticResolveDataStore(class'UIDataStore_OnlinePlaylists'.default.Tag));
	if ( PlaylistDS != None )
	{
		PlaylistDS.GetResourceProviders(ProviderTag, Providers);

		for (ProviderIndex = 0; ProviderIndex < Providers.length; ProviderIndex++)
		{
			OPP = OnlinePlaylistProvider(Providers[ProviderIndex]);
			if (OPP.PlaylistId == PlaylistId)
			{
				return OPP;
			}
		}
	}

	return None;
}

/**
 * Returns the match type for the specified playlist
 *
 * @param PlaylistId the playlist we are searching for
 *
 * @return the match type for the playlist
 */
event int GetMatchTypeForPlaylistId(int PlaylistId)
{
	if (PlaylistMan != None)
	{
		return PlaylistMan.GetMatchType(PlaylistId);
	}
	return -1;
}

DefaultProperties
{
	Tag=OnlinePlaylists
}
