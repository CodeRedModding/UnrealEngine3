/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "OnlineSubsystemLive.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UDownloadableContentEnumeratorLive)

/**
 * Appends the specified array to the DLCBundles array
 *
 * @param Bundles the array to append
 */
void UDownloadableContentEnumeratorLive::AppendDLC(const TArray<FOnlineContent>& Bundles)
{
	DLCBundles += Bundles;
}

#endif
