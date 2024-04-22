/** 
 * Default Windows implementation for HttpResponse.
 * See HttpResponseInterface for documentation details.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class HttpResponseWindows extends HttpResponseInterface
	native;

var private {private} const native transient pointer Response{class FHttpResponseWinInet};
var private {private} const native array<byte> Payload;

native function String GetHeader(String HeaderName);
native function array<String> GetHeaders();
native function String GetURLParameter(String ParameterName);
native function String GetContentType();
native function int GetContentLength();
native function String GetURL();
native function GetContent(out array<byte> Content);
native function String GetContentAsString();
native function int GetResponseCode();
