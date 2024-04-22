/** 
 * Base interface for HttpResponses.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class HttpResponseInterface
	extends HttpBaseInterface
	native
	abstract;

// Contains defines for all the possible status codes that can be returned.
`include(HttpStatusCodes.uci)

/**
 * Returns the response code returned by the requested server.
 * See HttpStatusCodes.uci for a complete list.
 *
 * @return	the response code.
 */
native function int GetResponseCode();

/**
 * Returns the payload as a string, assuming the payload is UTF8.
 *
 * @return	The payload as a string.
 */
native function String GetContentAsString();