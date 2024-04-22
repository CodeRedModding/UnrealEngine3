//=============================================================================
// GameCheatManager
// Object within gameplayercontroller that manages "cheat" commands
// only spawned in single player mode
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class GameCheatManager extends CheatManager within GamePlayerController
	config(Game)
	native;

/** Debug camera - used to have independent camera without stopping gameplay */
var DebugCameraController			DebugCameraControllerRef;
var class<DebugCameraController>	DebugCameraControllerClass;
var config string					DebugCameraControllerClassName;


function PatchDebugCameraController()
{
	local class<DebugCameraController> TempCameraControllerClass;

	if (DebugCameraControllerClassName != "")
	{
		TempCameraControllerClass = class<DebugCameraController>(DynamicLoadObject(DebugCameraControllerClassName, class'Class'));

		if (TempCameraControllerClass != None)
		{
			DebugCameraControllerClass = TempCameraControllerClass;
		}
	}
}

/**
 * Toggle between debug camera/player camera without locking gameplay and with locking
 * local player controller input.
 */
exec function ToggleDebugCamera(optional bool bDrawDebugText = true)
{
	local PlayerController PC;
	local DebugCameraController DCC;

	foreach WorldInfo.AllControllers(class'PlayerController', PC)
	{
		if ( PC.bIsPlayer && PC.IsLocalPlayerController() )
		{
			DCC = DebugCameraController(PC);
			if( DCC!=none && DCC.OriginalControllerRef==none )
			{
				//dcc are disabled, so we are looking for normal player controller
				continue;
			}
			break;
		}
	}

	if( DCC!=none && DCC.OriginalControllerRef!=none )
	{
		DCC.DisableDebugCamera();
		DCC.Destroy();
		DCC = None;
	}
	else if( PC!=none )
	{
		EnableDebugCamera(bDrawDebugText);
	}
}

/**
 * Teleport the player's pawn to the location of the debug camera (and, by default, toggle the debug camera off).  
 * If not in the "Debug Camera" mode, print an error message and give up.
 */
exec function TeleportPawnToCamera(optional bool bToggleDebugCameraOff = true)
{
	local PlayerController PC;
	local DebugCameraController DCC;
	local vector	ViewLocation;
	local rotator	ViewRotation;

	foreach WorldInfo.AllControllers(class'PlayerController', PC)
	{
		if ( PC.bIsPlayer && PC.IsLocalPlayerController() )
		{
			DCC = DebugCameraController(PC);
			if( DCC!=none && DCC.OriginalControllerRef==none )
			{
				//dcc are disabled, so we are looking for normal player controller
				continue;
			}
			break;
		}
	}

	if ((DCC != none) && (DCC.OriginalControllerRef!=none))
	{
		if (DCC.OriginalControllerRef.Pawn != None)
		{
			GetPlayerViewPoint(ViewLocation, ViewRotation);
			DCC.OriginalControllerRef.Pawn.SetLocation(ViewLocation);
			DCC.OriginalControllerRef.Pawn.SetRotation(ViewRotation);
		}

		if (bToggleDebugCameraOff)
			ToggleDebugCamera();
	}
	else
	{
		ClientMessage("TeleportPawnToCamera should be used in conjunction with the ToggleDebugCamera command.   Failed.");
	}
}

/**
 *  Switch controller to debug camera without locking gameplay and with locking
 *  local player controller input
 */
function EnableDebugCamera(bool bEnableDebugText)
{
	local Player P;
	local vector eyeLoc;
	local rotator eyeRot;
	local float CameraFOVAngle;

	P = Player;
	if( P!= none && Pawn != none && IsLocalPlayerController() )
	{
		PatchDebugCameraController();
		if( DebugCameraControllerRef!=None )
		{
			DebugCameraControllerRef.Destroy();
		}

		CameraFOVAngle = GetFOVAngle();

		DebugCameraControllerRef = Spawn(DebugCameraControllerClass);
		DebugCameraControllerRef.PlayerInput = none;
		DebugCameraControllerRef.OriginalPlayer = P;
		DebugCameraControllerRef.OriginalControllerRef = outer;

		GetPlayerViewPoint(eyeLoc,eyeRot);
		DebugCameraControllerRef.SetLocation(eyeLoc);
		DebugCameraControllerRef.SetRotation(eyeRot);
		DebugCameraControllerRef.bDrawDebugText=bEnableDebugText;

		P.SwitchController( DebugCameraControllerRef );
		DebugCameraControllerRef.OnActivate( outer );

		// Make sure the camera gets created and set it up.
		DebugCameraControllerRef.GetPlayerViewPoint(eyeLoc,eyeRot);
		if ( DebugCameraControllerRef.PlayerCamera != None )
		{
			DebugCameraControllerRef.PlayerCamera.SetFOV( CameraFOVAngle );
			DebugCameraControllerRef.PlayerCamera.UpdateCamera(0.0);
		}
		else
		{
			DebugCameraControllerRef.FOVAngle = CameraFOVAngle;
		}
	}
}

/**
 * Simple function to illustrate the use of the HttpRequest system.
 */
exec function TestHttp(string Verb, string Payload, string URL, optional bool bSendParallelRequest)
{
	local HttpRequestInterface R;

	// create the request instance using the factory (which handles
	// determining the proper type to create based on config).
	R = class'HttpFactory'.static.CreateRequest();
	// always set a delegate instance to handle the response.
	R.OnProcessRequestComplete = OnRequestComplete;
	`log("Created request");
	// you can make many requests from one request object.
	R.SetURL(URL);
	// Default verb is GET
	if (Len(Verb) > 0)
	{
		R.SetVerb(Verb);
	}
	else
	{
		`log("No Verb given, using the defaults.");
	}
	// Default Payload is empty
	if (Len(Payload) > 0)
	{
		R.SetContentAsString(Payload);
	}
	else
	{
		`log("No payload given.");
	}
	`log("Creating request for URL:"@URL);

	// there is currently no way to distinguish keys that are empty from keys that aren't there.
	`log("Key1 ="@R.GetURLParameter("Key1"));
	`log("Key2 ="@R.GetURLParameter("Key2"));
	`log("Key3NoValue ="@R.GetURLParameter("Key3NoValue"));
	`log("NonexistentKey ="@R.GetURLParameter("NonexistentKey"));
	// A header will not necessarily be present if you don't set one. Platform implementations
	// may add things like Content-Length when you send the request, but won't necessarily
	// be available in the Header.
	`log("NonExistentHeader ="@R.GetHeader("NonExistentHeader"));
	`log("CustomHeaderName ="@R.GetHeader("CustomHeaderName"));
	`log("ContentType ="@R.GetContentType());
	`log("ContentLength ="@R.GetContentLength());
	`log("URL ="@R.GetURL());
	`log("Verb ="@R.GetVerb());

	// multiple ProcessRequest calls can be made from the same instance if desired.
	if (!R.ProcessRequest())
	{
		`log("ProcessRequest failed. Unsuppress DevHttpRequest to see more details.");
	}
	else
	{
		`log("Request sent");
	}
	// send off a parallel request for testing.
	if (bSendParallelRequest)
	{
		if (!class'HttpFactory'.static.CreateRequest()
			.SetURL("http://www.epicgames.com")
			.SetVerb("GET")
			.SetHeader("Test", "Value")
			.SetProcessRequestCompleteDelegate(OnRequestComplete)
			.ProcessRequest())
		{
			`log("ProcessRequest for parallel request failed. Unsuppress DevHttpRequest to see more details.");
		}
		else
		{
			`log("Parallel Request sent");
		}
	}
}


/** Delegate to use for HttpResponses. */
function OnRequestComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface Response, bool bDidSucceed)
{
	local array<String> Headers;
	local String Header;
	local String Payload;
	local int PayloadIndex;

	`log("Got response!!!!!!! Succeeded="@bDidSucceed);
	`log("URL="@OriginalRequest.GetURL());
	// if we didn't succeed, we can't really trust the payload, so you should always really check this.
	if (Response != None)
	{
		`log("ResponseURL="@Response.GetURL());
		`log("Response Code="@Response.GetResponseCode());
		Headers = Response.GetHeaders();
		foreach Headers(Header)
		{
			`log("Header:"@Header);
		}
		// GetContentAsString will make a copy of the payload to add the NULL terminator,
		// then copy it again to convert it to TCHAR, so this could be fairly inefficient.
		// This call also assumes the payload is UTF8 right now, as truly determining the encoding
		// is content-type dependent.
		// You also can't trust the content-length as you don't always get one. You should instead
		// always trust the length of the content payload you receive.
		Payload = Response.GetContentAsString();
		if (Len(Payload) > 1024)
		{
			PayloadIndex = 0;
			`log("Payload:");
			while (PayloadIndex < Len(Payload))
			{
				`log("    "@Mid(Payload, PayloadIndex, 1024));
				PayloadIndex = PayloadIndex + 1024;
			}
		}
		else
		{
			`log("Payload:"@Payload);
		}
	}
}

defaultproperties
{
}
