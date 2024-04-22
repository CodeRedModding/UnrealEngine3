/** 
 * Default Windows implementation for HttpRequest.
 * See HttpRequestInterface for documentation details.
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class HttpRequestWindows extends HttpRequestInterface
	native;

var private {private} const native transient pointer Request{class FHttpRequestWinInet};
var private {private} const native string RequestVerb;
var private {private} const native transient pointer RequestURL{class FURLWinInet};
var private {private} const native array<byte> Payload;

native function String GetHeader(String HeaderName);
native function array<String> GetHeaders();
native function String GetURLParameter(String ParameterName);
native function String GetContentType();
native function int GetContentLength();
native function String GetURL();
native function GetContent(out array<byte> Content);
native function String GetVerb();
native function HttpRequestInterface SetVerb(String Verb);
native function HttpRequestInterface SetURL(String URL);
native function HttpRequestInterface SetContent(const out array<byte> ContentPayload);
native function HttpRequestInterface SetContentAsString(String ContentString);
native function HttpRequestInterface SetHeader(String HeaderName, String HeaderValue);
native function bool ProcessRequest();
