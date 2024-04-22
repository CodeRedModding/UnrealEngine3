/** 
 * MCP specific HTTP request implementation
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class HttpRequestWindowsMcp extends HttpRequestWindows
	config(Engine);

/** The app id value to append to the URL */
var const config string AppID;

/** The app secret value to append to the URL */
var const config string AppSecret;

/** Automatically appends the specified app id and app secret values to the URL if going to MCP */
function bool ProcessRequest()
{
	local string Url;

	Url = GetURL();
	if (InStr(Url, "appspot.com") != INDEX_NONE ||
		InStr(Url, "localhost:8888") != INDEX_NONE)
	{
		// Append the app id and secret value to the URL
		if (InStr(Url, "?") != INDEX_NONE)
		{
			Url $= "&appKey=" $ AppID;
		}
		else
		{
			Url $= "?appKey=" $ AppID;
		}
		Url $= "&appSecret=" $ AppSecret;
		SetURL(Url);
	}
	return Super.ProcessRequest();
}

defaultproperties
{
}
