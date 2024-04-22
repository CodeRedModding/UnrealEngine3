/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#include "HTTPDownload.h"

IMPLEMENT_CLASS(UMCPBase);
IMPLEMENT_CLASS(UOnlineNewsInterfaceMcp);

/**
 * Ticks any outstanding news read requests
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlineNewsInterfaceMcp::Tick(FLOAT DeltaTime)
{
#if WITH_UE3_NETWORKING
	if (bNeedsTicking)
	{
		INT InProgress = 0;
		// Check each registered news item for ticking
		for (INT NewsIndex = 0; NewsIndex < NewsItems.Num(); NewsIndex++)
		{
			FNewsCacheEntry& NewsCacheEntry = NewsItems(NewsIndex);
			// Handle ticking if in progress
			if (NewsCacheEntry.ReadState == OERS_InProgress)
			{
				if (NewsCacheEntry.HttpDownloader != NULL)
				{
					InProgress++;
					// Tick the task and check for timeout
					NewsCacheEntry.HttpDownloader->Tick(DeltaTime);
					// See if we are done
					if (NewsCacheEntry.HttpDownloader->GetHttpState() == HTTP_Closed)
					{
						NewsCacheEntry.HttpDownloader->GetString(NewsCacheEntry.NewsItem);
						NewsCacheEntry.ReadState = OERS_Done;
					}
					// Or are in error
					else if (NewsCacheEntry.HttpDownloader->GetHttpState() == HTTP_Error)
					{
						// Failed zero everything
						NewsCacheEntry.ReadState = OERS_Failed;
						NewsCacheEntry.NewsItem.Empty();
					}
				}
				else
				{
					// Failed, clean up
					NewsCacheEntry.ReadState = OERS_Failed;
				}
				// Fire the delegates if complete or upon error
				if (NewsCacheEntry.ReadState != OERS_InProgress)
				{
					OnlineNewsInterfaceMcp_eventOnReadNewsCompleted_Parms Parms(EC_EventParm);
					Parms.bWasSuccessful = NewsCacheEntry.ReadState == OERS_Done ? FIRST_BITFIELD : 0;
					Parms.NewsType = NewsCacheEntry.NewsType;
					delete NewsCacheEntry.HttpDownloader;
					NewsCacheEntry.HttpDownloader = NULL;
					TriggerOnlineDelegates(this,ReadNewsDelegates,&Parms);
				}
			}
		}
		bNeedsTicking = InProgress ? TRUE : FALSE;
	}
#endif
}

/**
 * Reads the game specific news from the online subsystem
 *
 * @param LocalUserNum the local user the news is being read for
 * @param NewsType the type of news to read
 *
 * @return true if the async task was successfully started, false otherwise
 */
UBOOL UOnlineNewsInterfaceMcp::ReadNews(BYTE LocalUserNum,BYTE NewsType)
{
	DWORD Result = E_FAIL;
#if WITH_UE3_NETWORKING
	// Find the news item specified
	FNewsCacheEntry* NewsCacheEntry = FindNewsCacheEntry(NewsType);
	// Validate the entry was configured properly
	if (NewsCacheEntry &&
		NewsCacheEntry->NewsUrl.Len())
	{
		// If we haven't started downloading, start the async process for doing so
		if (NewsCacheEntry->ReadState == OERS_NotStarted ||
			NewsCacheEntry->ReadState == OERS_Failed)
		{
			// Build an url from the string
			FURL Url(NULL,*NewsCacheEntry->NewsUrl,TRAVEL_Absolute);
			FResolveInfo* ResolveInfo = NULL;
			// See if we need to resolve this string
			UBOOL bIsValidIp = FInternetIpAddr::IsValidIp(*Url.Host);
			if (bIsValidIp == FALSE)
			{
				// Allocate a platform specific resolver and pass that in
				ResolveInfo = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*Url.Host));
			}
			// Turn Unicode on for all non-English
			UBOOL bIsUnicode = NewsCacheEntry->bIsUnicode || appGetLanguageExt() != TEXT("INT");
			// Build the additional options that are to be passed to the URL
			const FString Options = FString::Printf(TEXT("TitleID=%d&Localization=%s&PlatformID=%d&bIsUnicode=%d"),
				appGetTitleId(),
				*appGetLanguageExt(),
				(DWORD)appGetPlatformType(),
				bIsUnicode);
			// Create the object that will download the data
			NewsCacheEntry->HttpDownloader = new FHttpDownloadString(bIsUnicode,
				NewsCacheEntry->TimeOut,
				Options,
				ResolveInfo);
			// Start the download task
			NewsCacheEntry->HttpDownloader->DownloadUrl(Url);
			NewsCacheEntry->ReadState = OERS_InProgress;
			bNeedsTicking = TRUE;
			Result = ERROR_IO_PENDING;
		}
		else
		{
			// Already downloaded so indicate success or failure from the previous result
			Result = NewsCacheEntry->ReadState == OERS_Done ? S_OK : E_FAIL;
		}
	}
	if (Result != ERROR_IO_PENDING)
	{
		OnlineNewsInterfaceMcp_eventOnReadNewsCompleted_Parms Parms(EC_EventParm);
		Parms.bWasSuccessful = Result == S_OK ? FIRST_BITFIELD : FALSE;
		Parms.NewsType = NewsType;
		TriggerOnlineDelegates(this,ReadNewsDelegates,&Parms);
	}
#endif
	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

#endif
