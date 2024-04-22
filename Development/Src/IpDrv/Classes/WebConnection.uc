/*=============================================================================
	WebConnection is the bridge that will handle all communication between
	the web server and the client's browser.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class WebConnection extends TcpLink config(Web);

var WebServer WebServer;
var string ReceivedData;

var WebRequest Request;
var WebResponse Response;
var WebApplication Application;

var bool bDelayCleanup;

var int RawBytesExpecting;

var config int MaxValueLength;
var config int MaxLineLength;

// MC: Debug
var int ConnID;

event Accepted()
{
	WebServer = WebServer(Owner);
	SetTimer(30, False);
	ConnID = WebServer.ConnID++;
	`Logd("Connection"@ConnID@"Accepted");
}

event Closed()
{
    `Logd("Connection"@ConnID@"Closed");
	Destroy();
}

event Timer()
{
	`logd("Timer Up");
	bDelayCleanup = False;
	Cleanup();
}

event ReceivedText( string Text )
{
	local int i;
	local string S;

	`logd("ReceivedText"@Text);
	ReceivedData $= Text;
	if(RawBytesExpecting > 0)
	{
		RawBytesExpecting -= Len(Text);
		CheckRawBytes();

		return;
	}

	// remove a LF which arrived in a new packet
	// and thus didn't get cleaned up by the code below
	if(Left(ReceivedData, 1) == Chr(10))
		ReceivedData = Mid(ReceivedData, 1);
	i = InStr(ReceivedData, Chr(13));
	while(i != -1)
	{
		S = Left(ReceivedData, i);
		i++;
		// check for any LF following the CR.
		if(Mid(ReceivedData, i, 1) == Chr(10))
			i++;

		ReceivedData = Mid(ReceivedData, i);

		ReceivedLine(S);

		if(LinkState != STATE_Connected)
			return;
		if(RawBytesExpecting > 0)
		{
			CheckRawBytes();
			return;
		}

		i = InStr(ReceivedData, Chr(13));
	}
}

function ReceivedLine(string S)
{
	if (S == "")
	{
		EndOfHeaders();
	}
	else
	{
		if(Left(S, 4) ~= "GET ")
		{
			ProcessGet(S);
		}
		else if(Left(S, 5) ~= "POST ")
		{
			ProcessPost(S);
		}
		else if(Left(S, 5) ~= "HEAD ")
		{
			ProcessHead(S);
		}
		else if(Request != None)
		{
			Request.ProcessHeaderString(S);
		}
	}
}

function ProcessHead(string S)
{
	`Logd("Received HEAD:"@S);
}

function ProcessGet(string S)
{
	local int i;

	`logd("Received GET:"@S);
	if(Request == None)
		CreateResponseObject();

	Request.RequestType = Request_GET;
	S = Mid(S, 4);
	while(Left(S, 1) == " ")
		S = Mid(S, 1);

	i = InStr(S, " ");

	if(i != -1)
		S = Left(S, i);

	i = InStr(S, "?");
	if(i != -1)
	{
		Request.DecodeFormData(Mid(S, i+1));
		S = Left(S, i);
	}

	Application = WebServer.GetApplication(S, Request.URI);
	if(Application != None && Request.URI == "")
	{
		Response.Redirect(S$"/");
		Cleanup();
	}
	else if(Application == None && Webserver.DefaultApplication != -1)
	{
		Response.Redirect(Webserver.ApplicationPaths[Webserver.DefaultApplication]$"/");
		Cleanup();
	}
}

function ProcessPost(string S)
{
	local int i;

	`logd("Received POST:"@S);
	if(Request == None)
		CreateResponseObject();

	Request.RequestType = Request_POST;

	S = Mid(S, 5);
	while(Left(S, 1) == " ")
		S = Mid(S, 1);

	i = InStr(S, " ");

	if(i != -1)
		S = Left(S, i);

	i = InStr(S, "?");
	if(i != -1)
	{
		Request.DecodeFormData(Mid(S, i+1));
		S = Left(S, i);
	}

	Application = WebServer.GetApplication(S, Request.URI);
	if(Application != None && Request.URI == "")
	{
//		Response.Redirect(WebServer.ServerURL$S$"/");
		Response.Redirect(S$"/");
		Cleanup();
	}
}

function CreateResponseObject()
{
    local int i;
	Request = new(None) class'WebRequest';
	Request.RemoteAddr = IpAddrToString(RemoteAddr);
	i = InStr(Request.RemoteAddr, ":");
	if (i > -1)
	{
	   Request.RemoteAddr = Left(Request.RemoteAddr, i);
    }

	Response = new(None) class'WebResponse';
	Response.Connection = Self;
}

function EndOfHeaders()
{
	if(Response == None)
	{
		CreateResponseObject();
		Response.HTTPError(400); // Bad Request
		Cleanup();
		return;
	}

	if(Application == None)
	{
		Response.HTTPError(404); // FNF
		Cleanup();
		return;
	}

	if(Request.ContentLength != 0 && Request.RequestType == Request_POST)
	{
		RawBytesExpecting = Request.ContentLength;
		RawBytesExpecting -= Len(ReceivedData);
		CheckRawBytes();
	}
	else
	{
		if (Application.PreQuery(Request, Response))
		{
			Application.Query(Request, Response);
			Application.PostQuery(Request, Response);
		}
		Cleanup();
	}
}

function CheckRawBytes()
{
	if(RawBytesExpecting <= 0)
	{
		if(InStr(Locs(Request.ContentType), "application/x-www-form-urlencoded") != 0)
		{
			`log("WebConnection: Unknown form data content-type: "$Request.ContentType);
			Response.HTTPError(400); // Can't deal with this type of form data
		}
		else
		{
			Request.DecodeFormData(ReceivedData);
			if (Application.PreQuery(Request, Response))
			{
			  Application.Query(Request, Response);
			  Application.PostQuery(Request, Response);
			}
			ReceivedData = "";
		}
		Cleanup();
	}
}

function Cleanup()
{
	if (bDelayCleanup)
		return;

	if(Request != None)
		Request = None;

	if(Response != None)
	{
		Response.Connection = None;
		Response = None;
	}

	if (Application != None)
		Application = None;

	Close();
}

final function bool IsHanging()
{
	return bDelayCleanup;
}

defaultproperties
{
	//MaxValueLength=512	// Maximum size of a variable value
	//MaxLineLength=4096
}
