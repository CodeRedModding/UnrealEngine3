/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"

#include "HTTPDownload.h"

#if WITH_UE3_NETWORKING

/**
 * Captures hardware information as a string for uploading to MCP
 *
 * @return the XML string with the hardware data
 */
FString UOnlineEventsInterfaceMcpLive::BuildHardwareXmlData(void)
{
	FString XmlPayload;
#if CONSOLE
	// Add the language/region settings
	XmlPayload = FString::Printf(TEXT("<Hardware>\r\n\t<Region Id=\"%d\" LangId=\"%d\" LocaleId=\"%d\"/>\r\n"),
		appGetGameRegion(),
		XGetLanguage(),
		XGetLocale());
	// Read the video settings so we can add those
	XVIDEO_MODE VideoMode;
	XGetVideoMode(&VideoMode);
	XmlPayload += FString::Printf(TEXT("\t<Video Width=\"%d\" Height=\"%d\" Interlaced=\"%s\" WideScreen=\"%s\" HiDef=\"%s\" RefreshRate=\"%f\" VideoStd=\"%s\"/>\r\n"),
		VideoMode.dwDisplayWidth,
		VideoMode.dwDisplayHeight,
		BoolToString(VideoMode.fIsInterlaced),
		BoolToString(VideoMode.fIsWideScreen),
		BoolToString(VideoMode.fIsHiDef),
		VideoMode.RefreshRate,
		VideoStdToString(VideoMode.VideoStandard));
	// Now figure out what storage is available
	XmlPayload += TEXT("\t<Devices>\r\n");
	XDEVICE_DATA DeviceData;
	// There is no decent way to enumerate the devices so brute force device ID checks
	for (DWORD DeviceId = 1; DeviceId < 128; DeviceId++)
	{
		// Try to read the data for this id
		if (XContentGetDeviceData(DeviceId,&DeviceData) == ERROR_SUCCESS)
		{
			// The id was valid, so dump its info
			XmlPayload += FString::Printf(TEXT("\t\t<Device ID=\"%d\" Type=\"%s\" Size=\"%I64d\"/>\r\n"),
				DeviceId,
				DeviceTypeToString(DeviceData.DeviceType),
				DeviceData.ulDeviceBytes);
		}
	}
	XmlPayload += TEXT("\t</Devices>\r\n");
	// Add network information
	XmlPayload += FString::Printf(TEXT("\t<Network NAT=\"%s\"/>\r\n"),
		NatTypeToString(XOnlineGetNatType()));
	// Close the tag
	XmlPayload += TEXT("</Hardware>\r\n");
#endif
	return XmlPayload;
}

/**
 * @return platform specific XML data
 */
FString UOnlineEventsInterfaceMcpLive::BuildPlatformXmlData(void)
{
	DWORD NatType = NAT_Unknown;
	switch (XOnlineGetNatType())
	{
		case XONLINE_NAT_OPEN:
			NatType = NAT_Open;
			break;
		case XONLINE_NAT_MODERATE:
			NatType = NAT_Moderate;
			break;
		case XONLINE_NAT_STRICT:
			NatType = NAT_Strict;
			break;
	};

#if CONSOLE
	DWORD GameRegion = appGetGameRegion();
	DWORD Language = XGetLanguage();
	DWORD Locale = XGetLocale();
#else
	DWORD GameRegion = 0;
	DWORD Language = GetUserDefaultLangID();
	DWORD Locale = GetUserDefaultLCID();
#endif

	return FString::Printf(TEXT("RegionID=\"%d\" LangID=\"%d\" LocaleID=\"%d\" NAT=\"%d\""),
		GameRegion,
		Language,
		Locale,
		NatType);
}

/**
 * Builds the URL of additional parameters used when posting playlist population data
 *
 * @param PlaylistId the playlist id being reported
 * @param NumPlayers the number of players on the host
 *
 * @return the URL to use with all of the per platform extras
 */
FString UOnlineEventsInterfaceMcpLive::BuildPlaylistPopulationURLParameters(INT PlaylistId,INT NumPlayers)
{
#if CONSOLE
	DWORD GameRegion = appGetGameRegion();
	DWORD Language = XGetLanguage();
	DWORD Locale = XGetLocale();
#else
	DWORD GameRegion = 0;
	DWORD Language = GetUserDefaultLangID();
	DWORD Locale = GetUserDefaultLCID();
#endif
	return FString::Printf(TEXT("PlaylistId=%d&NumPlayers=%d&TitleID=%d&PlatformID=%d&RegionId=%d&LangId=%d&LocaleId=%d"),
		PlaylistId,
		NumPlayers,
		appGetTitleId(),
		(DWORD)appGetPlatformType(),
		GameRegion,
		Language,
		Locale);
}

#endif
