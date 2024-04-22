/*=============================================================================
	WebServer is responsible for listening to requests on the selected http
	port and will guide requests to the correct application.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class WebServer extends TcpLink config(Web);

var config string 	ServerName;
var config string 	Applications[10];
var config string 	ApplicationPaths[10];
var config bool 	bEnabled;
var config int 		ListenPort;
var config int 		MaxConnections;
var config int 		DefaultApplication;
var config int 		ExpirationSeconds;		// How long images can be cached .. default is 24 hours

var string 			ServerURL;
var WebApplication 	ApplicationObjects[10];

var int ConnectionCount;
// MC: Debug
var int ConnID;

function PostBeginPlay()
{
	local int i;
	local class<WebApplication> ApplicationClass;
	local IpAddr l;
	local string s;

	// Destroy if not a server
	if (WorldInfo.NetMode == NM_StandAlone || WorldInfo.NetMode == NM_Client)
	{
		Destroy();
		return;
	}

	if(!bEnabled)
	{
		`Log("Webserver is not enabled.  Set bEnabled to True in Advanced Options.");
		Destroy();
		return;
	}

	Super.PostBeginPlay();

	if(ServerName == "")
	{
		GetLocalIP(l);
		s = IpAddrToString(l);
		i = InStr(s, ":");
		if(i != -1)
			s = Left(s, i);
		ServerURL = "http://"$s;
	}
	else
		ServerURL = "http://"$ServerName;

	if(ListenPort != 80)
		ServerURL = ServerURL $ ":"$string(ListenPort);

	if (BindPort( ListenPort ) > 0)
	{
		if (Listen() == true)
		{
			`log("Web Server Created"@ServerURL@"Port:"@ListenPort@"MaxCon"@MaxConnections@"ExpirationSecs"@ExpirationSeconds@"Enabled"@bEnabled);
			//`log("~~~~~~~~~~~~~~~~~~~Loading Server Apps~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
			for(i=0;i<10;i++)
			{
				if(Applications[i] == "")
					break;

				ApplicationClass = class<WebApplication>(DynamicLoadObject(Applications[i], class'Class'));
				if(ApplicationClass != None)
				{
					//`log("Loading application"@Applications[i]@ApplicationClass@"Path:"@ApplicationPaths[i]);
					ApplicationObjects[i] = New(None) ApplicationClass;
					ApplicationObjects[i].WorldInfo = WorldInfo;
					ApplicationObjects[i].WebServer = Self;
					ApplicationObjects[i].Path = ApplicationPaths[i];
					ApplicationObjects[i].Init();
				}
				else
				{
					`log("Failed to load"@Applications[i]);
				}
			}
			//`log("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
			return;
		}
		else
		{
			`log("Unable to setup server for listen");
		}
	}	
	else
	{
		`log("Unable to bind webserver to a port");
	}

	//Fall through on failure, destroy ourselves
	Destroy();
}

event Destroyed()
{
	local int i;

	`log("Destroying WebServer");
	for(i = 0; i < ArrayCount(ApplicationObjects); i++)
	{
		if (ApplicationObjects[i] != None)
		{
			ApplicationObjects[i].CleanupApp();
		}
	}

	Super.Destroyed();
}

event GainedChild( Actor C )
{
	Super.GainedChild(C);
	ConnectionCount++;
	
	// if too many connections, close down listen.
	if(MaxConnections > 0 && ConnectionCount > MaxConnections && LinkState == STATE_Listening)
	{
		`Log("WebServer: Too many connections - closing down Listen.");
		Close();
	}
}

event LostChild( Actor C )
{
	Super.LostChild(C);
	ConnectionCount--;
	
	// if closed due to too many connections, start listening again.
	if(ConnectionCount <= MaxConnections && LinkState != STATE_Listening)
	{
		`Log("WebServer: Listening again - connections have been closed.");
		Listen();
	}
}

function WebApplication GetApplication(string URI, out string SubURI)
{
	local int i, l;

	SubURI = "";
	for(i=0;i<10;i++)
	{
		if(ApplicationPaths[i] != "")
		{
			l = Len(ApplicationPaths[i]);
			if(Left(URI, l) ~= ApplicationPaths[i] && (Len(URI) == l || Mid(URI, l, 1) == "/"))
			{
				SubURI = Mid(URI, l);
				`logd("Application handling request"@ApplicationObjects[i]@"Path:"@ApplicationPaths[i]);
				return ApplicationObjects[i];
			}
		}
	}

	`log("No application found to handle request"@URI);
	return None;
}

defaultproperties
{
     //Applications(0)="xWebAdmin.UTServerAdmin"
     //Applications(1)="xWebAdmin.UTImageServer"
     //ApplicationPaths(0)="/ServerAdmin"
     //ApplicationPaths(1)="/images"
     //ListenPort=80
     //MaxConnections=30
     //ExpirationSeconds=86400
     AcceptClass=Class'IpDrv.WebConnection'
}
