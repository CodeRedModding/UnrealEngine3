/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"

#if WITH_UE3_NETWORKING

#if WITH_PANORAMA && !CONSOLE

#include <WinLive.h>
#include <IPTypes.h>
#include <Iphlpapi.h>

/** Global used to determine if Panoram hooks are turned on */
UBOOL GIsUsingPanorama = FALSE;
/** This will be FALSE for dedicated servers and when disabled */
UBOOL GPanoramaNeedsRendering = FALSE;
/** Holds the HWND that we render to and that Live is using as the main window */
HWND GPanoramaWindowHandle = NULL;
/** Whether the Panorama UI is rendering & intercepting input */
UBOOL GIsPanoramaUIOpen = FALSE;

/** @return a registration key for use with games for windows live */
const TCHAR* appGetPanoramaRegistrationKey()
{
	FString CmdLineKey;
	// Check the commandline for a key
	if (Parse(appCmdLine(),TEXT("LIVEREGKEY"),CmdLineKey))
	{
		static TCHAR CommandlineKey[50];
		// Copy this so it can have access to it beyond the function scope
		appStrncpy(CommandlineKey,*CmdLineKey.Right(CmdLineKey.Len() - 1),50);
		return CommandlineKey;
	}
#if !SHIPPING_PC_GAME
	#if GAMENAME == EXAMPLEGAME
		return TEXT("PPPPP-PPPPP-PPPPP-PPPPP-PPPPP");
	#elif GAMENAME == UTGAME
		return TEXT("PPPPP-PPPPP-PPPPP-PPPPP-PPPPP");
	#elif GAMENAME == GEARGAME
		return TEXT("PPPPP-PPPPP-PPPPP-PPPPP-PPPPP");
	#elif GAMENAME == EXOGAME
		return TEXT("PPPPP-PPPPP-PPPPP-PPPPP-PPPPP");
	#else
		#error Hook up your game's G4WL registration key here
	#endif
#else
	return NULL;
#endif //!SHIPPING_PC_GAME
}

/** @return TRUE if Panorama is enabled, FALSE otherwise */
UBOOL appIsPanoramaEnabled(void)
{
	FString IgnoredString;
	// Panorama is disabled in editor, if nolive was specified, or devcon was
	return !GIsEditor &&
		Parse(appCmdLine(),TEXT("NOLIVE"),IgnoredString) == FALSE &&
		Parse(appCmdLine(),TEXT("DEVCON"),IgnoredString) == FALSE;
}

/**
 * Registers a sponsor key if possible
 */
void appPanoramaRegisterSponsorKey(void)
{
	const TCHAR* RegKey = appGetPanoramaRegistrationKey();
	const DWORD TitleId = appGetTitleId();
	
	if (RegKey != NULL)
	{
		// Register the game key with Live
		HRESULT hr = XLiveSetSponsorToken(RegKey, TitleId);
		if (FAILED(hr))
		{
			debugf(NAME_Error,
				TEXT("XLiveSetSponsorToken() failed with 0x%08X"),
				hr);
		}
	}
}

/**
* Stubbed out function when not enabled
*
* @param Device the device that is being used
* @param Params the display parameters that are being used
*
* @return TRUE if it could initialize, FALSE otherwise
*/
void appPanoramaRenderHookInit(IUnknown* Device,void* PresentParams,HWND DeviceWindow)
{
	FString IgnoredString;
	// Only init Live in the game and if it wasn't suppressed
	if (appIsPanoramaEnabled())
	{
		appPanoramaRegisterSponsorKey();
		XLIVE_INITIALIZE_INFO xii;
		appMemzero(&xii,sizeof(XLIVE_INITIALIZE_INFO));
		xii.cbSize = sizeof(XLIVE_INITIALIZE_INFO);
		// These will be NULL for dedicated servers
		xii.pD3D = Device;
		xii.pD3DPP = PresentParams;
		// Used to optionally override the net adapter
		ANSICHAR OverrideAdapterName[MAX_ADAPTER_NAME_LENGTH] = "";		
		// Don't hook up the rendering if this is a dedicated server
		if (!GIsServer && !GIsUCC)
		{
			// Need to be able to show the guide
			GPanoramaNeedsRendering = TRUE;
		}
		else
		{ 
			// Use command line parameters for logging on rather than default Live account
			xii.dwFlags = XLIVE_INITFLAG_NO_AUTO_LOGON;

			UBOOL bValidAdapterMACAddress = FALSE;
			FString AdapterMACAddress;
			// Check for override of Live MAC adapter address on the command line
			if (Parse(appCmdLine(),TEXT("LiveMACAddress="),AdapterMACAddress))
			{
				DWORD OverrideAdapterNameLen = ARRAY_COUNT(OverrideAdapterName);
				bValidAdapterMACAddress = appGetAdapterName(OverrideAdapterName,OverrideAdapterNameLen,*AdapterMACAddress);
				if (!bValidAdapterMACAddress)
				{
					debugf(NAME_Warning,
						TEXT("Invalid Live adapter MAC address specified: [%s]. Using defaults"),
						*AdapterMACAddress);
				}
			}
			else 
			{
				// Check for override of Live MAC adapter address via config
				const UBOOL bHasConfig = GConfig->GetString( 
					TEXT("OnlineSubsystemLive.OnlineSubsystemLive"), 
					TEXT("LiveMACAddress"), 
					AdapterMACAddress, 
					GEngineIni 
					);
				if (bHasConfig)
				{
					DWORD OverrideAdapterNameLen = ARRAY_COUNT(OverrideAdapterName);
					bValidAdapterMACAddress = appGetAdapterName(OverrideAdapterName,OverrideAdapterNameLen,*AdapterMACAddress);
					if (!bValidAdapterMACAddress)
					{
						debugf(NAME_Warning,
							TEXT("Invalid Live adapter MAC address specified: [%s]. Using defaults"),
							*AdapterMACAddress);
					}
				}
			}
			// Allow overriding of the default net adapter used by Live for dedicated servers
			if (bValidAdapterMACAddress)
			{
				xii.dwFlags |= XLIVE_INITFLAG_USE_ADAPTER_NAME;
				xii.pszAdapterName = OverrideAdapterName;			
			}

			// Allow overriding of the port used to connect to Live for dedicated servers
			INT OverridePort = 0;
			// According to the G4WL docs the minimum port allowed 
			const INT MIN_LIVE_PORT = 5000;
			// Check for override of Live Port on the command line
			if (Parse(appCmdLine(),TEXT("LivePort="),OverridePort))
			{
				if (OverridePort >= MIN_LIVE_PORT)
				{
					xii.wLivePortOverride = htons(OverridePort);
				}
				else
				{
					debugf(NAME_Warning,
						TEXT("Invalid Live port number specified: %d (Min port # is %d). Using defaults"),
						OverridePort,
						MIN_LIVE_PORT);
				}
			}
			else
			{
				// Check for override of Live Port via config
				const UBOOL bHasConfig = GConfig->GetInt( 
					TEXT("OnlineSubsystemLive.OnlineSubsystemLive"), 
					TEXT("LivePort"), 
					OverridePort, 
					GEngineIni);
				if (bHasConfig)
				{
					if (OverridePort >= MIN_LIVE_PORT)
					{
						xii.wLivePortOverride = htons(OverridePort);
					}
					else
					{
						debugf(NAME_Warning,
							TEXT("Invalid Live port number specified: %d (Min port # is %d). Using defaults"),
							OverridePort,
							MIN_LIVE_PORT);
					}
				}
			}			
		}

		// Indicates that XLiveInput will be called to direct processing of input messages to the Guide
		xii.dwFlags |= XLIVE_INITFLAG_USE_XLIVEINPUT;
		// Let Panorama initialize and log the error code if it fails
		HRESULT hr = XLiveInitialize(&xii);
		if (SUCCEEDED(hr))
		{
			GIsUsingPanorama = TRUE;
			// Set the window handle for code that needs to pass it to Live later
			GPanoramaWindowHandle = DeviceWindow;
			// Try to init the socket layer
			appSocketInit(FALSE);
		}
		else
		{
			if (hr == E_DEBUGGER_PRESENT)
			{
				debugf(NAME_Error,
					TEXT("Shutting down because the debugger is present"));
				appMsgf(AMT_OK,
					*LocalizeError(TEXT("DebuggerPresentError"),TEXT("Engine")));
				appRequestExit(0);
			}
			else
			{
				debugf(NAME_Error,
					TEXT("XLiveInitialize() failed with 0x%08X (0x%08X)"),
					hr,
					XGetOverlappedExtendedError(NULL));
			}
		}
	}
}

/**
 * Initializes the Panorama hook without rendering support
 */
void appPanoramaHookInit(void)
{
	appPanoramaRenderHookInit(NULL,NULL,NULL);
}

/**
 * Deinitializes Panorama rendering hooks
 */
void appPanoramaHookDeviceDestroyed(void)
{
	XLiveOnDestroyDevice();
}

/**
 * Allows the Panorama hook a chance to reset any resources
 *
 * @param PresentParameters the parameters used for the display
 */
void appPanoramaRenderHookReset(void* PresentParameters)
{
	if (GIsUsingPanorama && GPanoramaNeedsRendering)
	{
		HRESULT hr = XLiveOnResetDevice(PresentParameters);
		if (FAILED(hr))
		{
			debugf(NAME_Error,
				TEXT("XLiveOnResetDevice() failed with 0x%08X (0x%08X)"),
				hr,
				XGetOverlappedExtendedError(NULL));
		}
	}
}

/**
 * Allows the Live Guide to render its UI
 */
void appPanoramaRenderHookRender(void)
{
	// This is off in the editor and UCC
	if (GIsUsingPanorama && GPanoramaNeedsRendering)
	{
		XLiveRender();
	}
}

/**
 * Tears down the Panorama hook mechanism
 */
void appPanoramaHookUninitialize(void)
{
	if (GIsUsingPanorama)
	{
		XLiveUnInitialize();
		GIsUsingPanorama = GPanoramaNeedsRendering = FALSE;
	}
}

/**
 * Allows the Live Guide to intercept windows messages and process them
 *
 * @param hWnd the window handle of the window that is to process the message
 * @param Message the message to process
 * @param wParam the word parameter to the message
 * @param lParam the long parameter to the message
 * @param Return the return value if Live handled it
 *
 * @return TRUE if the app should process the message, FALSE if the guide processed it
 */
UBOOL appPanoramaInputHook(HWND hWnd,UINT Message,UINT wParam,LONG lParam,LONG& Return)
{
	UBOOL bNeedsProcessing = TRUE;
	// This is off in the editor and UCC
	if (GIsUsingPanorama)
	{
		XLIVE_INPUT_INFO xii = {0};
	    xii.cbSize = sizeof(XLIVE_INPUT_INFO);
		// Init with the message parameters
		xii.hWnd = hWnd;
		xii.uMsg = Message;
		xii.wParam = wParam;
		xii.lParam = lParam;
		// Allow the Live guide to process the message
		HRESULT hr = XLiveInput(&xii);
		if (SUCCEEDED(hr))
		{
			// Only process if the Guide didn't
			bNeedsProcessing = !xii.fHandled;
			Return = xii.lRet;
		}
		else
		{
			debugf(NAME_Error,
				TEXT("XLiveInput() failed with 0x%08X (0x%08X)"),
				hr,
				XGetOverlappedExtendedError(NULL));
		}
	}
	return bNeedsProcessing;
}

/**
 * Returns the window handle that Live was configured to use
 *
 * @return the window handle that Live was told to use
 */
HWND appPanoramaHookGetHWND(void)
{
	return GPanoramaWindowHandle;
}

/**
 * Determines whether the Live UI is showing
 *
 * @return TRUE if the guide is open, FALSE otherwise
 */
UBOOL appIsPanoramaGuideOpen(void)
{
	return GIsPanoramaUIOpen;
}

/**
 * Allow Live to prefilter any messages for IME support
 *
 * @param Msg the windows message to filter
 *
 * @return TRUE if the app should handle it, FALSE if Live did
 */
UBOOL appPanoramaInputTranslateMessage(LPMSG Msg)
{
	return XLivePreTranslateMessage(Msg) == FALSE;
}

#endif

#endif	//#if WITH_UE3_NETWORKING
