/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is a generic JSON object in unrealscript
 */
class JsonObject extends Object
	native;


/// COMMENT!!


var native Map_Mirror ValueMap{TMap<FString, FString>};
var native Map_Mirror ObjectMap{TMap<FString, UJsonObject*>};

var native array<string> ValueArray;
var native array<JsonObject> ObjectArray;


/**
 * Looks up an object with the given key in the ObjectMap
 *
 * @param Key The key to search for
 *
 * @return A subobject/array inside this object
 */
native function JsonObject GetObject(const string Key);

/**
 * Looks up a value with the given key in the ObjectMap. If it was a number
 * in the Json string, this will be prepended with \# (see below helpers)
 *
 * @param Key The key to search for
 *
 * @return A string value
 */
native function string GetStringValue(const string Key);

/**
 * @param Key the key to check on this object
 * 
 * @return true if the key exists, false otherwise
 */
native function bool HasKey(const string Key);

/**
 * Helper functions to convert special strings that the decoder will make from numbers
 */
function int GetIntValue(const string Key)
{
	local string Value;
	
	// look up the key, and skip the \#
	Value = Mid(GetStringValue(Key), 2);
	return int(Value);
}

function float GetFloatValue(const string Key)
{
	local string Value;

	// look up the key, and skip the \#
	Value = Mid(GetStringValue(Key), 2);
	return float(Value);
}

function bool GetBoolValue(const string Key)
{
	local string Value;

	// look up the key, and skip the \#
	Value = Mid(GetStringValue(Key), 2);
	return bool(Value);
}


/**
 * Set an object
 */
native function SetObject(const string Key, JsonObject Object);
native function SetStringValue(const string Key, const string Value);

/**
 * Helper functions to make special strings that the encoder will turn into numbers
 */
function SetIntValue(const string Key, int Value)
{
	SetStringValue(Key, "\\#" $ Value);
}

function SetFloatValue(const string Key, float Value)
{
	SetStringValue(Key, "\\#" $ Value);
}

function SetBoolValue(const string Key, bool Value)
{
	SetStringValue(Key, "\\#" $ (Value ? "true" : "false"));
}

/**
 * Encodes an object hierarchy to a string suitable for sending over the web
 *
 * @param Root The toplevel object in the hierarchy
 *
 * @return A well-formatted Json string
 */
static native function string EncodeJson(JsonObject Root);

/**
 * Decodes a Json string into an object hierarchy (all needed objects will be created)
 *
 * @param Str A Json string (probably received from the web)
 *
 * @return The root object of the resulting hierarchy
 */
static native function JsonObject DecodeJson(const string Str);
