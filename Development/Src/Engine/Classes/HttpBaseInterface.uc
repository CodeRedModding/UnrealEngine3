/** 
 * Base interface for HttpRequests and Responses.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class HttpBaseInterface extends Object
	abstract
	native;

/** 
 * Gets the value of a header, or empty string if not found. 
 * 
 * @todo HTML supports multiple header names
 * so this function needs to support returning multiple values
 * or define how it concatenates those values.
 * 
 * @todo Also, I'd like a version that returns the value as an int
 * if possible, zero otherwise.
 * 
 * @param	HeaderName - name of the header to set.
 */
native function String GetHeader(String HeaderName);

/**
 * Return all headers in an array in "Name: Value" format.
 *
 * @return	the header array
 */
native function array<String> GetHeaders();

/** Gets a URL parameter.
  * expected format is ?Key=Value&Key=Value...
  * If that format is not used, this function will not work.
  * 
  * @param	ParameterName - the parameter to request.
  * 
  * @return	The parameter value.
  */
native function String GetURLParameter(String ParameterName);

/**
 * Shortcut to get the Content-Type header value (if available)
 *
 * @return	the content type.
 */
native function String GetContentType();

/**
 * Shortcut to get the Content-Length header value. Will not always return non-zero.
 * If you want the real length of the payload, get the payload and check it's length.
 *
 * @return	The content length (if available)
 */
native function int GetContentLength();

/**
 * Get the URL used to send the request.
 *
 * @return	The URL.
 */
native function String GetURL();

/**
 * Get the content of the request or response.
 *
 * @param	Content - array that will be filled with the content.
 */
native function GetContent(out array<byte> Content);
