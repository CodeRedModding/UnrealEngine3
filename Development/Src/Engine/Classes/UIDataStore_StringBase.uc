/**
 * Base class for all string data stores.  String data stores provide the game with access to strings in various forms,
 * such as localized strings, input key button names, or lists of strings for use by the online subsystem.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIDataStore_StringBase extends UIDataStore
	native(inherit)
	abstract;

DefaultProperties
{
	Tag=StringBase
}
