/** 
 * Factory class for creating HttpRequests and Responses.
 * Clients don't generally create responses, they are created
 * internally by the Request handling system.
 * Expected usage is to call CreateRequest() and use the
 * returned interface.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class HttpFactory extends Object
	config(Engine);

/** Configurable class to use to make requests. This is platform specific. */
var config string HttpRequestClassName;

/** Static function to create a web request. */
static function HttpRequestInterface CreateRequest()
{
	local class<Object> HttpRequestClass;
	local Object HttpReq;

	HttpRequestClass = class<Object>(DynamicLoadObject(default.HttpRequestClassName,class'Class'));

	HttpReq = new HttpRequestClass;

	return HttpRequestInterface(HttpReq);
}
