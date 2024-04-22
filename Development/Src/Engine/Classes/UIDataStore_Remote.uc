/**
 * Provides the UI with access to data received from remote machines across a network. One usage of this type of data store
 * would be for a server browser, where game and player data about internet game sessions is retrieved from the master server.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_Remote extends UIDataStore
	native(inherit)
	abstract;

DefaultProperties
{
	Tag=RemoteData
}
