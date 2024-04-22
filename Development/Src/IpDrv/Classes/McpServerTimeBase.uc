/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides the interface for requesting UTC time from the server
 */
class McpServerTimeBase extends McpServiceBase
	abstract
	config(Engine);

/** The class name to use in the factory method to create our instance */
var config String McpServerTimeClassName;

/**
 * @return the object that implements this interface or none if missing or failed to create/load
 */
final static function McpServerTimeBase CreateInstance()
{
	local class<McpServerTimeBase> McpServerTimeBaseClass;
	local McpServerTimeBase NewInstance;

	McpServerTimeBaseClass = class<McpServerTimeBase>(DynamicLoadObject(default.McpServerTimeClassName,class'Class'));
	// If the class was loaded successfully, create a new instance of it
	if (McpServerTimeBaseClass != None)
	{
		NewInstance = new McpServerTimeBaseClass;
		NewInstance.Init();
	}

	return NewInstance;
}

/**
 * Request current UTC time from the server
 */
function QueryServerTime();

/**
 * Called when the time request from the server is complete
 *
 * @param bWasSuccessful true if server was contacted and a valid result received
 * @param DateTimeStr string representing UTC server time (yyyy.MM.dd-HH.mm.ss)
 * @param Error string representing the error condition
 */
delegate OnQueryServerTimeComplete(bool bWasSuccessful, string DateTimeStr, string Error);

/**
 * Retrieve cached timestamp from last server time query 
 *
 * @return string representation of time (yyyy.MM.dd-HH.mm.ss)
 */
function String GetLastServerTime();