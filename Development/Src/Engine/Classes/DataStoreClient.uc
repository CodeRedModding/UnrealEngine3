/**
 * Creates and manages all globally accessible persistent data stores.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DataStoreClient extends UIRoot
	native(inherit)
	Config(Engine);

/**
 * Represents a collection of data stores that are linked to a specific player.
 */
struct native transient PlayerDataStoreGroup
{
	/**
	 * the player that this group is associated with.
	 */
	var	const	transient	LocalPlayer			PlayerOwner;

	/**
	 * the list of data stores registered for this player.
	 */
	var	const	transient	array<UIDataStore>	DataStores;
};

/**
 * List of global data store class names to create when the data store client is created.
 */
var	config					array<string>				GlobalDataStoreClasses;

/**
 * The list of global persistent data stores.
 */
var const 					array<UIDataStore>			GlobalDataStores;

/**
 * List of data store class names that should be loaded at initialization time, but not created.  Instances of these data
 * stores will be created as they are needed (i.e. PlayerOwner, etc.)
 */
var	config					array<string>				PlayerDataStoreClassNames;

/**
 * Stores the list of dynamic player data store classes that were loaded from PlayerDataStoreClassNames.
 */
var	const private			array<class<UIDataStore> >	PlayerDataStoreClasses;

/**
 * the list of dynamic data stores that are created per-player.
 */
var	const					array<PlayerDataStoreGroup>	PlayerDataStores;

cpptext
{
	/**
	 * Loads each of the classes from the GlobalDataStoreClasses array, creates an instance of that class, and stores
	 * that instance in the GlobalDataStores array.
	 */
	virtual void InitializeDataStores();
}

/**
 * Finds the data store indicated by DataStoreTag and returns a pointer to it.
 *
 * @param	DataStoreTag	A name corresponding to the 'Tag' property of a data store
 * @param	PlayerOwner		used for resolving the correct data stores in split-screen games.
 *
 * @return	a pointer to the data store that has a Tag corresponding to DataStoreTag, or NULL if no data
 *			were found with that tag.
 */
native final function UIDataStore FindDataStore( name DataStoreTag, optional LocalPlayer PlayerOwner );

/**
 * Creates and initializes an instance of the data store class specified.
 *
 * @param	DataStoreClass	the data store class to create an instance of.  DataStoreClass should be a child class
 *							of UUIDataStore
 *
 * @return	a pointer to an instance of the data store class specified.
 */
native final function coerce UIDataStore CreateDataStore( class<UIDataStore> DataStoreClass );

/**
 * Adds a new data store to the GlobalDataStores array.
 *
 * @param	DataStore	the data store to add
 * @param	PlayerOwner	if specified, the data store will be added to the list of PlayerDataStores, rather than the list of global data stores
 *
 * @return	TRUE if the data store was successfully added, or if the data store was already in the list.
 */
native final function bool RegisterDataStore( UIDataStore DataStore, optional LocalPlayer PlayerOwner );

/**
 * Removes a data store from the GlobalDataStores array.
 *
 * @param	DataStore	the data store to remove
 *
 * @return	TRUE if the data store was successfully removed, or if the data store wasn't in the list.
 */
native final function bool UnregisterDataStore( UIDataStore DataStore );

/**
 * Finds the index into the PlayerDataStores array for the data stores associated with the specified player.
 *
 * @param	PlayerOwner		the player to search for associated data stores for.
 */
native final function int FindPlayerDataStoreIndex( LocalPlayer PlayerOwner ) const;

/* === Unrealscript === */
/**
 * Accessor for grabbing the list of player data store classes.
 */
final function GetPlayerDataStoreClasses( out array<class<UIDataStore> > out_DataStoreClasses )
{
	out_DataStoreClasses = PlayerDataStoreClasses;
}

/**
 * Searches the data store client's data store class arrays for a child of the specified meta class.
 *
 * @param	RequiredMetaClass	the data store base class to search for.
 *
 * @return	a pointer to a child class of RequiredMetaClass that was specified in the ini.
 */
final function class<UIDataStore> FindDataStoreClass( class<UIDataStore> RequiredMetaClass )
{
	local int i;
	local class<UIDataStore> Result;

	// first, search through the global data stores array
	for ( i = 0; i < GlobalDataStores.Length; i++ )
	{
		// if this global data store is an instance of the class we're searching for, stop here
		if ( GlobalDataStores[i].IsA(RequiredMetaClass.Name) )
		{
			Result = GlobalDataStores[i].Class;
			break;
		}
	}

	if ( Result == None )
	{
		// search through the player data store class arrays
		for ( i = 0; i < PlayerDataStoreClasses.Length; i++ )
		{
			if ( ClassIsChildOf(PlayerDataStoreClasses[i], RequiredMetaClass) )
			{
				Result = PlayerDataStoreClasses[i];
				break;
			}
		}
	}

	return Result;
}

/**
 * Called when the current map is being unloaded.  Cleans up any references which would prevent garbage collection.
 */
final event NotifyGameSessionEnded()
{
	local int i, DataStoreIndex;
	local array<UIDataStore> DataStoreArray;

	DataStoreArray = GlobalDataStores;

	// notify all global data stores
	for ( DataStoreIndex = 0; DataStoreIndex < DataStoreArray.Length; DataStoreIndex++ )
	{
		if ( DataStoreArray[DataStoreIndex].NotifyGameSessionEnded() )
		{
			UnregisterDataStore(DataStoreArray[DataStoreIndex]);
		}
	}

	// now notify all dynamic data stores
	for ( i = PlayerDataStores.Length - 1; i >= 0; i-- )
	{
		DataStoreArray = PlayerDataStores[i].DataStores;
		for ( DataStoreIndex = 0; DataStoreIndex < DataStoreArray.Length; DataStoreIndex++ )
		{
			DataStoreArray[DataStoreIndex].NotifyGameSessionEnded();
			UnregisterDataStore(DataStoreArray[DataStoreIndex]);
		}
	}
}


final function DebugDumpDataStoreInfo( bool bVerbose )
{
`if(`notdefined(FINAL_RELEASE))
	local int DataStoreIndex, PlayerDataStoreIndex;
	local string PlayerName;

	local LocalPlayer PlayerOwner;
	local array<UIDataStore> PlayerGroupDataStores;

	// first, search through the global data stores array
	`log("GlobalDataStores: " $ GlobalDataStores.Length,,'DevDataStore');

	for ( DataStoreIndex = 0; DataStoreIndex < GlobalDataStores.Length; DataStoreIndex++ )
	{
		//@todo ronp - expose UUIDataStore::GetDataStoreId() as a native unrealscript function, rather than using the data store's tag directly
		`log("	GlobalDataStore[" $ DataStoreIndex $ "]:" @ GlobalDataStores[DataStoreIndex].Tag @ "(" $ GlobalDataStores[DataStoreIndex] $ ")",,'DevDataStore');

		//@todo ronp - call a function on the GlobalDataStore to allow it to log more detailed information
	}

	`log("");
	`log("Player data store groups:" $ PlayerDataStores.Length,,'DevDataStore');
	for ( DataStoreIndex = 0; DataStoreIndex < PlayerDataStores.Length; DataStoreIndex++ )
	{
		PlayerOwner = PlayerDataStores[DataStoreIndex].PlayerOwner;
		PlayerGroupDataStores = PlayerDataStores[DataStoreIndex].DataStores;

		PlayerName = (PlayerOwner != None && PlayerOwner.Actor != None && PlayerOwner.Actor.PlayerReplicationInfo != None) ? PlayerOwner.Actor.PlayerReplicationInfo.PlayerName : "None";
		`log("	PlayerDataStores for player " $ DataStoreIndex $ ":" @ PlayerGroupDataStores.Length @ "(" $ Playername @ "-" @ PlayerOwner $ ")",,'DevDataStore');

		for ( PlayerDataStoreIndex = 0; PlayerDataStoreIndex < PlayerGroupDataStores.Length; PlayerDataStoreIndex++ )
		{
			//@todo ronp - expose UUIDataStore::GetDataStoreId() as a native unrealscript function, rather than using the data store's tag directly
			`log("		PlayerDataStore[" $ PlayerDataStoreIndex $ "]:" @ PlayerGroupDataStores[PlayerDataStoreIndex].Tag @ "(" $ PlayerGroupDataStores[PlayerDataStoreIndex] $ ")",,'DevDataStore');

			//@todo ronp - call a function on the GlobalDataStore to allow it to log more detailed information
		}
	}
`endif
}

DefaultProperties
{

}
