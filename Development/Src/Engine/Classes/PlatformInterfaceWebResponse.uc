/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the a generic web response object that holds the entirety of the 
 * web response made from PlatformInterfaceBase subclasses
 * 
 */
 
class PlatformInterfaceWebResponse extends Object
	native(PlatformInterface)
	transient;

/** This holds the original requested URL */
var string OriginalURL;

/** Result code from the response (200=OK, 404=Not Found, etc) */
var int ResponseCode;

/** A user-specified tag specified with the request */
var int Tag;

/** Response headers and their values */
var native Map_Mirror Headers{TMap<FString, FString>};

/** For string results, this is the response */
var string StringResponse;

/** For non-string results, this is the response */
var array<byte> BinaryResponse;



/** @return the number of header/value pairs */
native function int GetNumHeaders();

/** Retrieve the header and value for the given index of header/value pair */
native function GetHeader(int HeaderIndex, out string Header, out string Value);

/** @return the value for the given header (or "" if no matching header) */
native function string GetHeaderValue(string HeaderName);

