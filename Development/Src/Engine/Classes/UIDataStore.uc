/**
 * Base for classes which provide data to the UI subsystem.
 * A data store is how the UI references data in the game.  Data stores allow the UI to reference game data in a safe
 * manner, since they encapsulate lifetime management.  A data store can be either persistent, in which case it is
 * attached directly to the UIInteraction object and is available to all widgets, or it can be temporary, in which case
 * it is attached to the current scene and is only accessible to the widgets contained by that scene.
 *
 * Persistent data stores might track information such as UI data for all gametypes or characters.  Temporary data
 * stores might track stuff like the name that was entered into some UI value widget.  Data stores can provide static
 * information, such as the names of all gametypes, or dynamic information, such as the name of the current gametype.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore extends UIDataProvider
	native(inherit)
	abstract;

cpptext
{
	/**
	 * Allows each data store the chance to load any dependent classes
	 */
	virtual void LoadDependentClasses(void)
	{
	}

	/**
	 * Hook for performing any initialization required for this data store
	 */
	virtual void InitializeDataStore();

	/**
	 * Called when this data store is added to the data store manager's list of active data stores.
	 *
	 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
	 *							associated with a particular player; NULL if this is a global data store.
	 */
	virtual void OnRegister( class ULocalPlayer* PlayerOwner );

	/**
	 * Called when this data store is removed from the data store manager's list of active data stores.
	 *
	 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
	 *							associated with a particular player; NULL if this is a global data store.
	 */
	virtual void OnUnregister( class ULocalPlayer* PlayerOwner );

	/**
	 * Retrieves the tag used for referencing this data store.  Normally corresponds to Tag, but may be different for some special
	 * data stores.
	 */
	virtual FName GetDataStoreID() const { return Tag; }
}

/** The name used to access this datastore */
var		name				Tag;

/** the list of delegates to call when data exposed by this data store has been updated */
var		array<delegate<OnDataStoreValueUpdated> >		RefreshSubscriberNotifies;

/**
 * This delegate is called whenever the values exposed by this data store have been updated.  Provides data stores with a way to
 * notify subscribers when they should refresh their values from this data store.
 *
 * @param	SourceDataStore		the data store that generated the refresh notification
 * @param	bValuesInvalidated	TRUE if the data values were completely invalidated; suggest a full refresh rather than an update (i.e. in lists)
 * @param	PropertyTag			the tag associated with the data field that was updated.
 * @param	SourceProvider		for data stores which contain nested providers, the provider that contains the data which changed.
 * @param	ArrayIndex			for collection fields, indicates which element was changed.  value of INDEX_NONE indicates not an array
 *								or that the entire array was updated.
 */
delegate OnDataStoreValueUpdated( UIDataStore SourceDataStore, bool bValuesInvalidated, name PropertyTag, UIDataProvider SourceProvider, int ArrayIndex );

/**
 * Called when this data store is added to the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
event Registered( LocalPlayer PlayerOwner );

/**
 * Called when this data store is removed from the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
event Unregistered( LocalPlayer PlayerOwner );

/**
 * Notification that a subscriber is using a value from this data store.  Adds the subscriber's RefreshSubscriberValue method
 * to this data store's list of refresh notifies so that the subscriber can refresh its value when the data store's value changes.
 *
 * @param	Subscriber	the subscriber that attached to the data store.
 */
event SubscriberAttached( UIDataStoreSubscriber Subscriber )
{
	local int SubscriberNotifyIndex;

	if ( Subscriber != None )
	{
		SubscriberNotifyIndex = RefreshSubscriberNotifies.Find(Subscriber.NotifyDataStoreValueUpdated);

		if ( SubscriberNotifyIndex == INDEX_NONE )
		{
			SubscriberNotifyIndex = RefreshSubscriberNotifies.Length;
			RefreshSubscriberNotifies[SubscriberNotifyIndex] = Subscriber.NotifyDataStoreValueUpdated;
		}
	}
}

/**
 * Notification that a subscriber is no longer using any values from this data store.  Removes the subscriber's RefreshSubscriberValue method
 * from this data store's list of refresh notifies so that the subscriber no longer refreshes its value when the data store's value changes.
 *
 * @param	Subscriber	the subscriber that detached from the data store.
 */
event SubscriberDetached( UIDataStoreSubscriber Subscriber )
{
	local int SubscriberNotifyIndex;

	if ( Subscriber != None )
	{
		SubscriberNotifyIndex = RefreshSubscriberNotifies.Find(Subscriber.NotifyDataStoreValueUpdated);
		if ( SubscriberNotifyIndex != INDEX_NONE )
		{
			RefreshSubscriberNotifies.Remove(SubscriberNotifyIndex,1);
		}
	}
}

/**
 * Called when the current map is being unloaded.  Cleans up any references which would prevent garbage collection.
 *
 * @return	TRUE indicates that this data store should be automatically unregistered when this game session ends.
 */
function bool NotifyGameSessionEnded();

/**
 * Loops through the subscriber notify list and calls the delegate letting the subscriber know to refresh their value.
 *
 * @param	PropertyTag			the tag associated with the data field that was updated.
 * @param	SourceProvider		for data stores which contain nested providers, the provider that contains the data which changed.
 * @param	ArrayIndex			for collection fields, indicates which element was changed.  value of INDEX_NONE indicates not an array
 *								or that the entire array was updated.
 */
final event RefreshSubscribers( optional name PropertyTag, optional bool bInvalidateValues=true, optional UIDataProvider SourceProvider, optional int ArrayIndex=INDEX_NONE )
{
	local int Idx;
	local delegate<OnDataStoreValueUpdated> Subscriber;

	//@todo: Right now we make a copy of the array because the refresh notify for datastore subscribers is unregistering the notify
	// when resolving markup.
	local array<delegate<OnDataStoreValueUpdated> > SubscriberArrayCopy;
	SubscriberArrayCopy.Length = RefreshSubscriberNotifies.Length;

	for(Idx = 0; Idx < SubscriberArrayCopy.Length; Idx++)
	{
		SubscriberArrayCopy[Idx] = RefreshSubscriberNotifies[Idx];
	}

	for (Idx = 0; Idx < SubscriberArrayCopy.Length; Idx++)
	{
		Subscriber = SubscriberArrayCopy[Idx];
		Subscriber(Self, bInvalidateValues, PropertyTag, SourceProvider, ArrayIndex);
	}
}

/**
 * Returns a reference to the global data store client, if it exists.
 *
 * @return	the global data store client for the game.
 */
final function DataStoreClient GetDataStoreClient()
{
	return class'UIInteraction'.static.GetDataStoreClient();
}

DefaultProperties
{

}
