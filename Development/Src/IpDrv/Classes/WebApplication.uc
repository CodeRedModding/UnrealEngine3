/*=============================================================================
// WebApplication: Parent class for Web Server applications
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
class WebApplication extends Object;

// Set by the webserver
var WorldInfo WorldInfo;
var WebServer WebServer;
var string Path;

function Init();

// This is a dummy function which should never be called
// Here for backwards compatibility
final function Cleanup();

function CleanupApp()
{
	if (WorldInfo != None)
		WorldInfo = None;

	if (WebServer != None)
		WebServer = None;
}

function bool PreQuery(WebRequest Request, WebResponse Response) { return true; }
function Query(WebRequest Request, WebResponse Response);
function PostQuery(WebRequest Request, WebResponse Response);

