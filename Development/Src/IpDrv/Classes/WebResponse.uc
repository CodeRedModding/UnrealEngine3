/*=============================================================================
	WebResponse is used by WebApplication to handle most aspects of sending
	http information to the client. It serves as a bridge between WebApplication
	and WebConnection.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class WebResponse extends Object
	native
	config(Web);

/*
The correct order of sending a response is:

1. define content:
	AddHeader(...), Subst(...), ClearSubst(...), LoadParsedUHTM(...), headers, CharSet,
2. HTTPResponse(...)
	(optional, implies a 200 return when not explicitly send)
3. SendStandardHeaders(...)
	(optional, implied by SendText(...))
4. send content:
	IncludeUHTM(...), SendText(...)
*/

var array<string>				headers; // headers send before the content
var private native const Map_Mirror ReplacementMap{TMultiMap<FString, FString>};  // C++ placeholder.
var const config string 		IncludePath;
var localized string 			CharSet;
var WebConnection 				Connection;
var protected bool 				bSentText; // used to warn headers already sent
var protected bool 				bSentResponse;

cpptext
{
private:
	void SendInParts(const FString &S);
	bool IncludeTextFile(const FString &Root, const FString &Filename, bool bCache=false, FString *Result = NULL);
	bool ValidWebFile(const FString &Filename);
	FString GetIncludePath();

	//Hack to workaround script compiler code generation (mirrors the old code with fix)
    void SendBinary(INT Count,BYTE* B)
    {
        WebResponse_eventSendBinary_Parms Parms(EC_EventParm);
        Parms.Count=Count;
        appMemcpy(&Parms.B,B,sizeof(Parms.B));
        ProcessEvent(FindFunctionChecked(IPDRV_SendBinary),&Parms);
    }
};

native final function bool		FileExists(string Filename);
native final function 			Subst(string Variable, coerce string Value, optional bool bClear);
native final function 			ClearSubst();
native final function bool		IncludeUHTM(string Filename);
native final function bool		IncludeBinaryFile(string Filename);
native final function string 	LoadParsedUHTM(string Filename); // For templated web items, uses Subst too
native final function string 	GetHTTPExpiration(optional int OffsetSeconds);

native final function Dump(); // only works in dev mode

event SendText(string Text, optional bool bNoCRLF)
{
	if(!bSentText)
	{
		SendStandardHeaders();
		bSentText = True;
	}

	if(bNoCRLF)
	{
		Connection.SendText(Text);
	}
	else {
		Connection.SendText(Text$Chr(13)$Chr(10));
	}
}

event SendBinary(int Count, byte B[255])
{
	Connection.SendBinary(Count, B);
}

function bool SendCachedFile(string Filename, optional string ContentType)
{
	if(!bSentText)
	{
		SendStandardHeaders(ContentType, true);
		bSentText = True;
	}
	return IncludeUHTM(Filename);
}

function FailAuthentication(string Realm)
{
	HTTPError(401, Realm);
}

/**
 * Send the HTTP response code.
 */
function HTTPResponse(string Header)
{
	bSentResponse = True;
	HTTPHeader(Header);
}

/**
 * Will actually send a header. You should not call this method, queue the headers
 * through the AddHeader() method.
 */
function HTTPHeader(string Header)
{
	if(bSentText)
	{
		`Log("Can't send headers - already called SendText()");
	}
	else {
		if (!bSentResponse)
		{
			HTTPResponse("HTTP/1.1 200 Ok");
		}
		if (Len(header) == 0)
		{
			bSentText = true;
		}
		Connection.SendText(Header$Chr(13)$Chr(10));
	}
}

/**
 * Add/update a header to the headers list. It will be send at the first possible occasion.
 * To completely remove a given header simply give it an empty value, "X-Header:"
 * To add multiple headers with the same name (need for Set-Cookie) you'll need
 * to edit the headers array directly.
 */
function AddHeader(string header, optional bool bReplace=true)
{
	local int i, idx;
	local string part, entry;
	i = InStr(header, ":");
	if (i > -1)
	{
		part = Caps(Left(header, i+1)); // include the :
	}
	else {
		return; // not a valid header
	}
	foreach headers(entry, idx)
	{
		if (InStr(Caps(entry), part) > -1)
		{
			if (bReplace)
			{
				if (i+2 >= len(header))
				{
					headers.remove(idx, 1);
				}
				else {
					headers[idx] = header;
				}
			}
			return;
		}
	}
	if (len(header) > i+2)
	{
		// only add when it contains a value
		headers.AddItem(Header);
	}
}

/**
 * Send the stored headers.
 */
function SendHeaders()
{
	local string hdr;
	foreach headers(hdr)
	{
		HTTPHeader(hdr);
	}
}

function HTTPError(int ErrorNum, optional string Data)
{
	switch(ErrorNum)
	{
	case 400:
		HTTPResponse("HTTP/1.1 400 Bad Request");
		SendText("<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD><BODY><H1>400 Bad Request</H1>If you got this error from a standard web browser, please mail epicgames.com and submit a bug report.</BODY></HTML>");
		break;
	case 401:
		HTTPResponse("HTTP/1.1 401 Unauthorized");
		AddHeader("WWW-authenticate: basic realm=\""$Data$"\"");
		SendText("<HTML><HEAD><TITLE>401 Unauthorized</TITLE></HEAD><BODY><H1>401 Unauthorized</H1></BODY></HTML>");
		break;
	case 404:
		HTTPResponse("HTTP/1.1 404 Not Found");
		SendText("<HTML><HEAD><TITLE>404 File Not Found</TITLE></HEAD><BODY><H1>404 File Not Found</H1>The URL you requested was not found.</BODY></HTML>");
		break;
	default:
		break;
	}
}

/**
 * Send the standard response headers.
 */
function SendStandardHeaders( optional string ContentType, optional bool bCache )
{
	if(ContentType == "")
	{
		ContentType = "text/html";
	}
	if(!bSentResponse)
	{
		HTTPResponse("HTTP/1.1 200 OK");
	}
	AddHeader("Server: UnrealEngine IpDrv Web Server Build "$Connection.WorldInfo.EngineVersion, false);
	AddHeader("Content-Type: "$ContentType, false);
	if (bCache)
	{
		AddHeader("Cache-Control: max-age="$Connection.WebServer.ExpirationSeconds, false);
		// Need to compute an Expires: tag .... arrgggghhh
		AddHeader("Expires: "$GetHTTPExpiration(Connection.WebServer.ExpirationSeconds), false);
	}
	AddHeader("Connection: Close"); // always close
	SendHeaders();
	HTTPHeader("");
}

function Redirect(string URL)
{
	HTTPResponse("HTTP/1.1 302 Document Moved");
	AddHeader("Location: "$URL);
	SendText("<html><head><title>Document Moved</title></head>");
	SendText("<body><h1>Object Moved</h1>This document may be found <a HREF=\""$URL$"\">here</a>.</body></html>");
}


function bool SentText()
{
	return bSentText;
}

function bool SentResponse()
{
	return bSentResponse;
}

defaultproperties
{
     //IncludePath="/Web"
     //CharSet="iso-8859-1"
}
