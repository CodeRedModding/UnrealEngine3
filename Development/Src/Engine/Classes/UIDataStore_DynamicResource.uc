/**
 * This data store can be used for cases where both static and dynamic information about a particular game concept must be displayed together.
 * For example, this data store could be used to display a list of levels which have been unlocked by the player.  The information about the
 * levels themselves would probably be exposed by a UIResourceDataProvider, but resource data providers cannot provide data about the player's
 * progress since they are static by nature.  The player's progress must come from something like a profile data provider that can provide
 * information about which of the levels have been unlocked.  This data store brings these two types of information together in order to
 * provide a combination of static and dynamic information about a game resource from a single location.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_DynamicResource extends UIDataStore
	native(UIPrivate)
	config(Game);

/** Provides access to the player's profile data */
var transient	UIDataProvider_OnlineProfileSettings	ProfileProvider;
var	transient	UIDataStore_GameResource				GameResourceDataStore;


struct native DynamicResourceProviderDefinition
{
	/**
	 * the tag that is used to access this provider, i.e. Players, Teams, etc.; should be the same value as the ProviderTag for the
	 * static resource this provider type is associated with.
	 */
	var	config		name									ProviderTag;

	/** the name of the class associated with this data provider */
	var	config		string									ProviderClassName;

	/** the UIDataProvider class that exposes the data for this data field tag */
	var	transient	class<UIResourceCombinationProvider>	ProviderClass;
};

/** the list of data providers supported by this data store that correspond to list element data */
var	config								array<DynamicResourceProviderDefinition>		ResourceProviderDefinitions;

/** collection of list element provider instances that are associated with each ElementProviderType */
var	const	private	native	transient	MultiMap_Mirror									ResourceProviders{TMultiMap<FName,class UUIResourceCombinationProvider*>};

/*
- init the profile provider ref
- init the game resource ds ref
- create all resource providers
- initialize all resource providers
- implement all methods, just like game resource ds
*/

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
native final function bool GetResourceProviders( name ProviderTag, out array<UIResourceCombinationProvider> out_Providers ) const;

/**
 * Re-initializes all dynamic providers.
 *
 * @param LocalUserNum the player that had a login change
 */
native final function OnLoginChange(byte LocalUserNum);

/**
 * Called when this data store is added to the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
event Registered( LocalPlayer PlayerOwner )
{
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;
	local UIDataStore_OnlinePlayerData PlayerProfileDS;

	Super.Registered(PlayerOwner);

	PlayerProfileDS = UIDataStore_OnlinePlayerData(class'UIRoot'.static.StaticResolveDataStore(class'UIDataStore_OnlinePlayerData'.default.Tag, PlayerOwner));
	if ( PlayerProfileDS != None )
	{
		ProfileProvider = PlayerProfileDS.ProfileProvider;
	}

	GameResourceDataStore = UIDataStore_GameResource(class'UIRoot'.static.StaticResolveDataStore(class'UIDataStore_GameResource'.default.Tag, PlayerOwner));

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// Grab the player interface to verify the subsystem supports it
		PlayerInterface = OnlineSub.PlayerInterface;
		if (PlayerInterface != None)
		{
			// We need to know when the player's login changes
			PlayerInterface.AddLoginChangeDelegate(OnLoginChange);
		}
	}
}

/**
 * Called when this data store is removed from the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
event Unregistered( LocalPlayer PlayerOwner )
{
	local int TypeIndex, ProviderIndex;
	local array<UIResourceCombinationProvider> ProviderInstances;
	local OnlineSubsystem OnlineSub;
	local OnlinePlayerInterface PlayerInterface;

	Super.Unregistered(PlayerOwner);

/*	if ( ProfileProvider.Player == PlayerOwner || ProfileProvider.Player == None )
	{
		ProfileProvider = None;
	}*/

	GameResourceDataStore = None;

	// now tell all our providers to clear their profile reference as well....
	for ( TypeIndex = 0; TypeIndex < ResourceProviderDefinitions.Length; TypeIndex++ )
	{
		if ( GetResourceProviders(ResourceProviderDefinitions[TypeIndex].ProviderTag, ProviderInstances) )
		{
			for ( ProviderIndex = 0; ProviderIndex < ProviderInstances.Length; ProviderIndex++ )
			{
				ProviderInstances[ProviderIndex].ClearProviderReferences();
			}
		}
	}

	// Figure out if we have an online subsystem registered
	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
	if (OnlineSub != None)
	{
		// Grab the player interface to verify the subsystem supports it
		PlayerInterface = OnlineSub.PlayerInterface;
		if (PlayerInterface != None)
		{
			// Clear our delegate
			PlayerInterface.ClearLoginChangeDelegate(OnLoginChange);
		}
	}
}

DefaultProperties
{
	Tag=DynamicGameResource
}
