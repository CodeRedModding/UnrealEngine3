/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GenericParamListStatEntry extends Object
	native;

cpptext
{
	// don't leak the stat event if we get destroyed before disk-commit for some reason
	virtual void BeginDestroy();
};

var native transient pointer StatEvent{struct FGenericParamListEvent};
var protected transient GameplayEventsWriter Writer;

// setters for supported data types
function native AddFloat(name ParamName, float Value);
function native AddInt(name ParamName, Int Value);
function native AddVector(name ParamName, Vector Value);
function native AddString(name ParamName, coerce String Value);

// getters for supported data types
function native bool GetFloat(name ParamName, out float out_Float);
function native bool GetInt(name ParamName, out int out_int);
function native bool GetVector(name ParamName, out vector out_vector);
function native bool GetString(name ParamName, out string out_string);

// will write this event to disk
function native CommitToDisk();

