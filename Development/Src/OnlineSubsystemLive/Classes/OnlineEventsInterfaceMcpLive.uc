/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Provides an in game gameplay events/stats upload mechanism via the MCP backend
 */
class OnlineEventsInterfaceMcpLive extends OnlineEventsInterfaceMcp
	native;

cpptext
{
	/**
	 * Returns a true/false string for the bool
	 *
	 * @param bBool the bool being converted
	 *
	 * @return true/false string
	 */
	FORCEINLINE const TCHAR* BoolToString(UBOOL bBool)
	{
		return bBool ? TEXT("true") : TEXT("false");
	}

	/**
	 * Returns a string for the video standard
	 *
	 * @param VideoStd the video standard to convert
	 *
	 * @return either NTSC, NTSC-J, or PAL
	 */
	FORCEINLINE const TCHAR* VideoStdToString(DWORD VideoStd)
	{
#if CONSOLE
		switch (VideoStd)
		{
			case XC_VIDEO_STANDARD_NTSC_J:
			{
				return TEXT("NTSC-J");
			}
			case XC_VIDEO_STANDARD_PAL_I:
			{
				return TEXT("PAL");
			}
		}
		return TEXT("NTSC");
#else
		return TEXT("VGA");
#endif
	}

	/**
	 * Returns a string for the device type
	 *
	 * @param DeviceType the device type to convert
	 *
	 * @return either HD, MU, or Unknown
	 */
	FORCEINLINE const TCHAR* DeviceTypeToString(DWORD DeviceType)
	{
#if CONSOLE
		switch (DeviceType)
		{
			case XCONTENTDEVICETYPE_HDD:
			{
				return TEXT("HD");
			}
			case XCONTENTDEVICETYPE_MU:
			{
				return TEXT("MU");
			}
		}
#endif
		return TEXT("UNKNOWN");
	}

	/**
	 * Returns a string for the NAT type
	 *
	 * @param NatType the NAT type to convert
	 *
	 * @return either OPEN, STRICT, or MODERATE
	 */
	FORCEINLINE const TCHAR* NatTypeToString(DWORD NatType)
	{
		switch (NatType)
		{
			case XONLINE_NAT_OPEN:
			{
				return TEXT("OPEN");
			}
			case XONLINE_NAT_MODERATE:
			{
				return TEXT("MODERATE");
			}
		}
		return TEXT("STRICT");
	}

	/**
	 * Builds the URL of additional parameters used when posting playlist population data
	 *
	 * @param PlaylistId the playlist id being reported
	 * @param NumPlayers the number of players on the host
	 *
	 * @return the URL to use with all of the per platform extras
	 */
	virtual FString BuildPlaylistPopulationURLParameters(INT PlaylistId,INT NumPlayers);

	/**
	 * Captures hardware information as a string for uploading to MCP
	 */
	virtual FString BuildHardwareXmlData(void);

	/**
	 * @return platform specific XML data
	 */
	virtual FString BuildPlatformXmlData(void);
}
