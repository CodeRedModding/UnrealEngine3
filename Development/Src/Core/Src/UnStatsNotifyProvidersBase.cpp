/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the base implementation of the stats notification
 * system.
 */

#include "CorePrivate.h"

#if STATS

/** Zero the static before they are constructed */
FStatNotifyProviderFactory* FStatNotifyProviderFactory::FirstFactory = NULL;

/**
 * Hidden so that only the factory or derived class can create these
 *
 * @param InProviderName the name of this provider
 * @param InProviderCmdLine	Short name for this provider that may be specified on the app's command line
 */
FStatNotifyProvider::FStatNotifyProvider( const TCHAR* InProviderName, const TCHAR* InProviderCmdLine ) :
	ProviderName(InProviderName),
	bIsEnabled(FALSE),
	CurrentFrame((DWORD)-1)
{
	if (GConfig != NULL)
	{
		// Figure out if this provider is enabled or not
		GConfig->GetBool(TEXT("StatNotifyProviders"),ProviderName,bIsEnabled,
			GEngineIni);
	}
	// Check to see if the provider was listed on the commandline
	if( ParseParam( appCmdLine(), ProviderName ) ||
		( InProviderCmdLine != NULL && ParseParam( appCmdLine(), InProviderCmdLine ) ) )
	{
		bIsEnabled = TRUE;
	}
}

/**
 * Uses the list of factory classes to create the list of providers
 * that this manager will forward calls to
 */
UBOOL FStatNotifyManager::Init(void)
{
	// For each registered factory, create the provider
	for (const FStatNotifyProviderFactory* Factory = FStatNotifyProviderFactory::FirstFactory;
		Factory != NULL; Factory = Factory->NextFactory)
	{
		// Create the instance
		FStatNotifyProvider* Provider = Factory->CreateInstance();
		// Now let it initialize itself
		if (Provider->IsEnabled())
		{
			if (Provider->Init() == TRUE)
			{
				// Force scoped cycle stats to be enabled for providers that are always enabled.
				if( Provider->IsEnabled() && Provider->IsAlwaysOn() )
				{
					GForceEnableScopedCycleStats++;
				}

				// Add this one to our list
				Providers.AddItem(Provider);
			}
			else
			{
				debugf(TEXT("Failed to initialize stat notify provider %s"),
					Provider->GetProviderName());
				delete Provider;
			}
		}
	}
	return TRUE;
}

/**
 * Works through the list of providers destroying each in turn
 */
void FStatNotifyManager::Destroy(void)
{
	// Loops through destroying each provider
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		// Destroy it and delete it
		Provider->Destroy();
		delete Provider;
	}
	Providers.Empty();
}

/**
 * Passes the current frame number on to all providers
 *
 * @param FrameNumber the new frame number being processed
 */
void FStatNotifyManager::SetFrameNumber(DWORD FrameNumber)
{
	// Loops through updating the frame number for each enabled
	// provider
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->SetFrameNumber(FrameNumber);
		}
	}
}


/**
 * Start capturing stat file
 */
void FStatNotifyManager::StartWritingStatsFile()
{
	// Give each provider a chance to flush
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if( Provider->IsEnabled() )
		{
			Provider->StartWritingStatsFile();
		}
	}
}



/**
 * Stop capturing stat file
 */
void FStatNotifyManager::StopWritingStatsFile()
{
	// Give each provider a chance to flush
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if( Provider->IsEnabled() )
		{
			Provider->StopWritingStatsFile();
		}
	}
}



/**
 * Adds a stat to the list of descriptions. Used to allow custom stats to
 * report who they are, parentage, etc. Prevents applications that consume
 * the stats data from having to change when stats information changes
 *
 * @param StatId the id of the stat
 * @param StatName the name of the stat
 * @param StatType the type of stat this is
 * @param GroupId the id of the group this stat belongs to
 */
void FStatNotifyManager::AddStatDescription(DWORD StatId,const TCHAR* StatName,
	DWORD StatType,DWORD GroupId)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->AddStatDescription(StatId,StatName,StatType,GroupId);
		}
	}
}

/**
 * Adds a group to the list of descriptions
 *
 * @param GroupId the id of the group being added
 * @param GroupName the name of the group
 */
void FStatNotifyManager::AddGroupDescription(DWORD GroupId,const TCHAR* GroupName)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->AddGroupDescription(GroupId,GroupName);
		}
	}
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param ParentId the parent id of the stat
 * @param InstanceId the instance id of the stat being written
 * @param ParentInstanceId the instance id of parent stat
 * @param ThreadId the thread this stat is for
 * @param Value the value of the stat to write out
 * @param CallsPerFrame the number of calls for this frame
 */
void FStatNotifyManager::WriteStat(DWORD StatId,DWORD GroupId,DWORD ParentId,DWORD InstanceId,
	DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,DWORD CallsPerFrame)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->WriteStat(StatId,GroupId,ParentId,InstanceId,ParentInstanceId,
				ThreadId,Value,CallsPerFrame);
		}
	}
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyManager::WriteStat(DWORD StatId,DWORD GroupId,FLOAT Value)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->WriteStat(StatId,GroupId,Value);
		}
	}
}

/**
 * Function to write the stat out to the provider's data store
 *
 * @param StatId the id of the stat that is being written out
 * @param GroupId the id of the group the stat belongs to
 * @param Value the value of the stat to write out
 */
void FStatNotifyManager::WriteStat(DWORD StatId,DWORD GroupId,DWORD Value)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->WriteStat(StatId,GroupId,Value);
		}
	}
}

/**
 * Tells the provider that we are starting to supply it with descriptions
 * for all of the stats/groups.
 */
void FStatNotifyManager::StartDescriptions(void)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->StartDescriptions();
		}
	}
}

/**
 * Tells the provider that we are finished sending descriptions for all of
 * the stats/groups.
 */
void FStatNotifyManager::EndDescriptions(void)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->EndDescriptions();
		}
	}
}

/**
 * Tells the provider that we are starting to supply it with group descriptions
 */
void FStatNotifyManager::StartGroupDescriptions(void)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->StartGroupDescriptions();
		}
	}
}

/**
 * Tells the provider that we are finished sending stat descriptions
 */
void FStatNotifyManager::EndGroupDescriptions(void)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->EndGroupDescriptions();
		}
	}
}

/**
 * Tells the provider that we are starting to supply it with stat descriptions
 */
void FStatNotifyManager::StartStatDescriptions(void)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->StartStatDescriptions();
		}
	}
}

/**
 * Tells the provider that we are finished sending group descriptions
 */
void FStatNotifyManager::EndStatDescriptions(void)
{
	// Just multicasts the calls
	for (INT Index = 0; Index < Providers.Num(); Index++)
	{
		FStatNotifyProvider* Provider = Providers(Index);
		if (Provider->IsEnabled())
		{
			Provider->EndStatDescriptions();
		}
	}
}

#endif
