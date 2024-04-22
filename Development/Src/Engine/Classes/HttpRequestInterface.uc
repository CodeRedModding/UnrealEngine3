/** 
 * Base interface for HttpRequests.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class HttpRequestInterface
	extends HttpBaseInterface
	native
	abstract;

/**
 * Gets the verb (GET, PUT, POST) used by the request.
 * 
 * @return	The verb
 */
native function String GetVerb();

/**
 * Sets the verb (GET, PUT, POST) used by the request.
 * Should be set before calling ProcessRequest.
 * Defaults to GET.
 *
 * @param	Verb - Verb to use.
 * @return this
 */
native function HttpRequestInterface SetVerb(String Verb);

/**
 * Sets the URL for the request (http://my.domain.com/something.ext?key=value&key2=value).
 * Must be set before calling ProcessRequest.
 *
 * @param	URL - URL to use
 * @return this
 */
native function HttpRequestInterface SetURL(String URL);

/**
 * Sets the content of the request (optional data).
 * Usually only set for POST requests.
 *
 * @param	ContentPayload - payload to set.
 * @return this
 */
native function HttpRequestInterface SetContent(const out array<byte> ContentPayload);

/**
 * Sets the content of the request as a string encoded as UTF8.
 *
 * @param	ContentString - payload to set.
 * @return this
 */
native function HttpRequestInterface SetContentAsString(String ContentString);

/**
 * Sets optional header info.
 * Content-Length is the only header set for you.
 * Required headers depends on the request itself.
 *
 * @param	HeaderName - Name of the header (ie, Content-Type)
 * @param	HeaderValue - Value of the header
 * @return this
 */
native function HttpRequestInterface SetHeader(String HeaderName, String HeaderValue);

/**
 * Called to begin processing the request.
 * When a response is recevied, the OnProcessRequestComplete
 * delegate is called with the response.
 * Even if the request is invalid, the OnProcessRequestComplete delegate is still called.
 * In that case, the HttpResponse parameter will be None.
 *
 * @return	if the request was successfully sent. If false, the delegate will not fire.
 */
native function bool ProcessRequest();

/**
 * Delegate called when the request is complete.
 *
 * @param	OriginalRequest - The original request object that spawned the response
 * @param	HttpResponse - The response object. Could be None if the request failed spectacularly. If the request failed to receive a complete
 *							response for some reason, this could contain a valid Response object with as much info as could be retrieved.
 *							Always use the bDidSucceed parameter to determine if the entire response was received successfully.
 * @param	bSucceeded - whether the response succeeded. If it did not, you should not trust the payload or headers. 
 *							Basically indicates a net failure occurred while receiving the response.
 */
delegate OnProcessRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface InHttpResponse, bool bDidSucceed);

/**
 * Sets the delegate as a convenience function for chaining expressions.
 *
 * @param	ProcessRequestCompleteDelegate - the delegate to set
 * @return	self
 */
function HttpRequestInterface SetProcessRequestCompleteDelegate(delegate<OnProcessRequestComplete> ProcessRequestCompleteDelegate)
{
	OnProcessRequestComplete = ProcessRequestCompleteDelegate;
	return self;
}
