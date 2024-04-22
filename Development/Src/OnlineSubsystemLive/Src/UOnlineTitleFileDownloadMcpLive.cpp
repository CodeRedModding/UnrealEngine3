/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UOnlineTitleFileDownloadMcpLive);

/**
 * Builds the URL to use when fetching the specified file
 *
 * @param FileName the file that is being requested
 *
 * @return the URL to use with all of the per platform extras
 */
FString UOnlineTitleFileDownloadMcpLive::BuildURLParameters(const FString& FileName)
{
	DWORD GameRegion = appGetGameRegion();
#if CONSOLE
	DWORD Language = XGetLanguage();
	DWORD Locale = XGetLocale();
#else
	DWORD Language = GetUserDefaultLangID();
	DWORD Locale = GetUserDefaultLCID();
#endif
	FString Url = FString::Printf(TEXT("TitleID=%d&PlatformID=%d&Filename=%s&RegionId=%d&LangId=%d&LocaleId=%d"),
		appGetTitleId(),
		(DWORD)appGetPlatformType(),
		*FileName,
		GameRegion,
		Language,
		Locale);

#if DEDICATED_SERVER
	XUID PlayerXuid = 0;
	// Try to get the xuid to append to the URL
	if (GetUserXuid(0, &PlayerXuid) == ERROR_SUCCESS)
	{
		Url += FString::Printf(TEXT("&UniqueId=%I64u"),(QWORD&)PlayerXuid);
	}
#else
	// Read the primary player's xuid if there is a player
	if (GEngine &&
		GEngine->GamePlayers.IsValidIndex(0))
	{
		ULocalPlayer* LP = GEngine->GamePlayers(0);
		if (LP)
		{
			XUID PlayerXuid = 0;
			// Try to get the xuid to append to the URL
			if (GetUserXuid(LP->ControllerId,&PlayerXuid) == ERROR_SUCCESS)
			{
				Url += FString::Printf(TEXT("&UniqueId=%I64u"),(QWORD&)PlayerXuid);
			}
		}
	}
#endif
	return Url;
}

#endif
