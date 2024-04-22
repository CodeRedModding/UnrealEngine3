/*=============================================================================
	UnStats.cpp: Performance stats framework.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if STATS

#if USE_AG_PERFMON
	#include "MinWindows.h"
	#include "../../../External/AgPerfMon/include/AgPerfMonEventSrcAPI.h"

//	#pragma message("Linking AgPerfmon")
	#pragma comment(lib, "AgPerfMon.lib")
	
	AgPerfUtils* GAgPerfUtils;

	unsigned __int16 UnAgPmRegisterEvent(const char *name)
	{
		if(GAgPerfUtils)
		{
			return GAgPerfUtils->registerEvent(name);
		}
		else
		{
			return 0;
		}
	}
	
	AgPerfMonTimer::AgPerfMonTimer(unsigned __int16 id) : PerfEventID(id)
	{
		if(GAgPerfUtils)
		{
			GAgPerfUtils->startEvent(PerfEventID);
		}
	}

	AgPerfMonTimer::~AgPerfMonTimer(void)
	{
		if(GAgPerfUtils)
		{
			GAgPerfUtils->stopEvent(PerfEventID);
		}
	}
#endif //USE_AG_PERFMON

/** Used to prevent instances being created after the stat manager's init */
UBOOL FStatGroup::bIsPostInit = FALSE;
/** Initializes the global list of stat group factory classes */
FStatGroupFactory* FStatGroupFactory::FirstFactory = NULL;
/** Initializes the global list of stat group factory classes */
FStatFactory* FStatFactory::FirstFactory = NULL;
/** Global instance counter */
DWORD FCycleStat::LastInstanceId = 0;

/** Create and init our global stat manager */
FStatManager GStatManager;

/** Global FPS counter */
FFPSCounter GFPSCounter;

DECLARE_STATS_GROUP(TEXT("DefaultStatGroup"),STATGROUP_Default);
DECLARE_CYCLE_STAT(TEXT("Root"),STAT_Root,STATGROUP_Default);

DECLARE_STATS_GROUP(TEXT("StatSystem"),STATGROUP_StatSystem);

DECLARE_CYCLE_STAT(TEXT("PerFrameCapture"),STAT_PerFrameCapture,STATGROUP_StatSystem);
DECLARE_CYCLE_STAT(TEXT("PerFrameCapture (RT)"),STAT_PerFrameCaptureRT,STATGROUP_StatSystem);
DECLARE_CYCLE_STAT(TEXT("DrawStats"),STAT_DrawStats,STATGROUP_StatSystem);

DECLARE_DWORD_COUNTER_STAT(TEXT("Timing code calls"),STAT_TimingCodeCalls,STATGROUP_StatSystem);


/**
 * Sets the initial state for this stat
 */
FCycleStat::FCycleStat(DWORD InStatId,const TCHAR* InCounterName,
	DWORD InThreadId,FCycleStat* InParent,
	UBOOL bInAddToGroup,DWORD InGroupId) :
	FStatCommonData(InCounterName,InStatId,InGroupId),
	ThreadId(InThreadId),
	Parent(InParent),
	InstanceId(appInterlockedIncrement((INT*)&LastInstanceId)),
	LastTimeStatWasSlow(0),
	bForOfflineProfiling( FALSE )
{
	// Don't use more than 24 bits worth or the viewer will break
	checkSlow((LastInstanceId & 0xFF000000) == 0);
	RecursiveCount = NumCallsPerFrame = Cycles = 0;

	if( bInAddToGroup )
	{
		// Get the group for this stat
		FStatGroup* Group = GStatManager.GetGroup(InGroupId);
		check(Group);
		// And add us to it
		Group->AddToGroup(this);
	}
	else
	{
		// Probably a canonical stat, so don't add to the group, instead just set the group ID
		GroupId = InGroupId;
	}
}

/**
 * Calls base class and inserts into the group
 */
FMemoryCounter::FMemoryCounter(const TCHAR* InCounterName,DWORD InStatId,
	DWORD InGroupId, EMemoryCounterRegion InRegion, UBOOL bInDisplayZeroStats) :
	TAccumulator<DWORD>(InCounterName,InStatId),
	Region(InRegion),
	bDisplayZeroStats(bInDisplayZeroStats)
{
	// Get the group for this stat
	FStatGroup* Group = GStatManager.GetGroup(InGroupId);
	check(Group);
	// And add us to it
	Group->AddToGroup(this);
}

/**
 * Adds the value to the history and zeros it
 *
 * @return the inclusive time for this stat
 */
DWORD FCycleStat::AdvanceFrame(void)
{
	// Iterate through each child adding its time
	DWORD ChildTime = 0;
	for (FChildStatMapIterator It(Children); It; ++It)
	{
		// Recursively advance the frame and track the child's time
		ChildTime += It.Value()->AdvanceFrame();
	}

	if( !bForOfflineProfiling )
	{

		// Keep a running average of this stat inclusive
		History.AddToHistory(Cycles);

		// Keep a running average of this stat exclusive of its children
		ExclusiveHistory.AddToHistory(Cycles - ChildTime);

		// Keep a running average of this stat number of calls/frame
		NumCallsHistory.AddToHistory(NumCallsPerFrame);
	}

	// Store the inclusive so we can return it to our parent
	DWORD Current = Cycles;

	// Zero the cycles, calls per frame, etc.
	NumCallsPerFrame = Cycles = 0;

	return Current;
}

/**
 * Create a stat and return the common interface to it
 */
FStatCommonData* FStatFactory::CreateStat(void) const
{
	FStatCommonData* NewStat = NULL;

	// make sure memory counters use the subclass
	check(StatType != STATTYPE_MemoryCounter);

	// Create based upon the type
	switch (StatType)
	{
		case STATTYPE_AccumulatorDWORD:
		{
			NewStat = CreateDwordAccumulator();
			break;
		}
		case STATTYPE_AccumulatorFLOAT:
		{
			NewStat = CreateFloatAccumulator();
			break;
		}
		case STATTYPE_CounterDWORD:
		{
			NewStat = CreateDwordCounter();
			break;
		}
		case STATTYPE_CounterFLOAT:
		{
			NewStat = CreateFloatCounter();
			break;
		}
	}
	return NewStat;
}

/**
* Create a stat and return the common interface to it
*/
FStatCommonData* FMemoryStatFactory::CreateStat(void) const
{
	return CreateMemoryStat();
}


/**
 * Cleans up resources
 */
FStatGroup::~FStatGroup()
{
	// Delete the various stats
	DeleteStatList<FCycleStat>(FirstCycleStat);
	DeleteStatList<FStatAccumulatorFLOAT>(FirstFloatAccumulator);
	DeleteStatList<FStatCounterDWORD>(FirstDwordCounter);
	DeleteStatList<FStatCounterFLOAT>(FirstFloatCounter);
	DeleteStatList<FStatAccumulatorDWORD>(FirstDwordAccumulator);
	DeleteStatList<FMemoryCounter>(FirstMemoryCounter);
}

/**
 * Thread safe function for inserting at the head of the list
 */
void FStatGroup::AddToGroup(FCycleStat* Stat)
{
	FCycleStat* Head;
	// Update both the group pointer and the stat pointer atomically
	do
	{
		Head = FirstCycleStat;
		Stat->Next = FirstCycleStat;
	}
	while (appInterlockedCompareExchangePointer((void**)&FirstCycleStat,Stat,Head) != Head);
	Stat->GroupId = GroupId;
}

/**
 * Thread safe function for inserting at the head of the list
 */
void FStatGroup::AddToGroup(FStatCounterFLOAT* Stat)
{
	FStatCounterFLOAT* Head;
	// Update both the group pointer and the stat pointer atomically
	do
	{
		Head = FirstFloatCounter;
		Stat->Next = FirstFloatCounter;
	}
	while (appInterlockedCompareExchangePointer((void**)&FirstFloatCounter,Stat,Head) != Head);
	Stat->GroupId = GroupId;
}

/**
 * Thread safe function for inserting at the head of the list
 */
void FStatGroup::AddToGroup(FStatAccumulatorFLOAT* Stat)
{
	FStatAccumulatorFLOAT* Head;
	// Update both the group pointer and the stat pointer atomically
	do
	{
		Head = FirstFloatAccumulator;
		Stat->Next = FirstFloatAccumulator;
	}
	while (appInterlockedCompareExchangePointer((void**)&FirstFloatAccumulator,Stat,Head) != Head);
	Stat->GroupId = GroupId;
}

/**
 * Thread safe function for inserting at the head of the list
 */
void FStatGroup::AddToGroup(FStatCounterDWORD* Stat)
{
	FStatCounterDWORD* Head;
	// Update both the group pointer and the stat pointer atomically
	do
	{
		Head = FirstDwordCounter;
		Stat->Next = FirstDwordCounter;
	}
	while (appInterlockedCompareExchangePointer((void**)&FirstDwordCounter,Stat,Head) != Head);
	Stat->GroupId = GroupId;
}

/**
 * Thread safe function for inserting at the head of the list
 */
void FStatGroup::AddToGroup(FStatAccumulatorDWORD* Stat)
{
	FStatAccumulatorDWORD* Head;
	// Update both the group pointer and the stat pointer atomically
	do
	{
		Head = FirstDwordAccumulator;
		Stat->Next = FirstDwordAccumulator;
	}
	while (appInterlockedCompareExchangePointer((void**)&FirstDwordAccumulator,Stat,Head) != Head);
	Stat->GroupId = GroupId;
}

/**
 * Thread safe function for inserting at the head of the list
 */
void FStatGroup::AddToGroup(FMemoryCounter* Stat)
{
	FMemoryCounter* Head;
	// Update both the group pointer and the stat pointer atomically
	do
	{
		Head = FirstMemoryCounter;
		Stat->Next = FirstMemoryCounter;
	}
	while (appInterlockedCompareExchangePointer((void**)&FirstMemoryCounter,Stat,Head) != Head);
	Stat->GroupId = GroupId;
}

/**
 * Updates the canonical stats for group views
 */
void FStatGroup::UpdateCanonicalStats(void)
{
	const DWORD CurrentThreadId = appGetCurrentThreadId();

	// Iterate each stat and append its value to the cannonical stat
	for (FCycleStat* Stat = FirstCycleStat; Stat != NULL; Stat = Stat->Next)
	{
		if( !Stat->bForOfflineProfiling )
		{
			if(Stat->ThreadId == CurrentThreadId)
			{
				FCycleStat* Canonical = CanonicalCycleStats.FindRef(Stat->StatId);
				check(Canonical);
				// Add the instance to the canonical instance (one per stat id per group)
				appInterlockedAdd((INT*)&Canonical->Cycles,(INT)Stat->Cycles);
				appInterlockedAdd((INT*)&Canonical->NumCallsPerFrame,(INT)Stat->NumCallsPerFrame);
			}
		}
	}
}

/**
 * Initializes all of the internal state
 */
void FStatManager::Init(void)
{
#if USE_AG_PERFMON
	if(!GAgPerfUtils)
	{
		GAgPerfUtils = new AgPerfUtils();
	}
#endif

	debugfSuppressed(NAME_DevStats,TEXT("GSecondsPerCycle %e"),GSecondsPerCycle);
	check(bIsInitialized == FALSE);
	// Create a synch object
	SyncObject = GSynchronizeFactory->CreateCriticalSection();
	check(SyncObject);
	// Create the groups once
	CreateGroups();
	// Set up the factory maps
	BuildFactoryMaps();
	// Create the canonical stats for each group
	CreateCanonicalStats();
	// Create the accumulator/counter stats once, since they don't need per
	// thread construction
	CreateCountersAccumulators();
	// Init the notify system
	StatNotifyManager.Init();
	// Send all the intial info to the stats notifiers
	SendNotifiersDescriptions();
	// Declare the stat that tracks the stats system's time
	SCOPE_CYCLE_COUNTER(STAT_PerFrameCapture);
	// All done initing things
	bIsInitialized = TRUE;
}

/**
 * Adds all of the group and stats descriptions to the stat notifiers
 * NOTE: Assumes that all groups have previously been created
 */
void FStatManager::SendNotifiersDescriptions(void)
{
	// Let the notifiers know we are starting the description phase
	StatNotifyManager.StartDescriptions();
	// Add all of the groups
	StatNotifyManager.StartGroupDescriptions();
	// Loop through the groups and send their info to the notifiers
	for (const FStatGroupFactory* Factory = FStatGroupFactory::FirstFactory;
		Factory != NULL; Factory = Factory->NextFactory)
	{
		const FStatGroup* Group = GroupMap.FindRef(Factory->GroupId);
		// Don't add info for groups that aren't net enabled
		if ((Group != NULL && Group->bIsNetEnabled == TRUE) ||
			// This group must always be exported for the frame time stat
			(Group != NULL && Group->GroupId == STATGROUP_Engine))
		{
			StatNotifyManager.AddGroupDescription(Factory->GroupId,Factory->GroupDesc);
		}
	}
	StatNotifyManager.EndGroupDescriptions();
	// Now do the same for all stats
	StatNotifyManager.StartStatDescriptions();
	// Loop through the stat factories and send their info to the notifiers
	for (const FStatFactory* Factory = FStatFactory::FirstFactory;
		Factory != NULL; Factory = Factory->NextFactory)
	{
		const FStatGroup* Group = GroupMap.FindRef(Factory->GroupId);
		// Don't add stat descriptions for a group that is disabled
		if ((Group != NULL && Group->bIsNetEnabled == TRUE) ||
			// The frame time stat must always be sent
			Factory->StatId == STAT_FrameTime)
		{
			StatNotifyManager.AddStatDescription(Factory->StatId,Factory->CounterName,
				Factory->StatType,Factory->GroupId);
		}
	}
	StatNotifyManager.EndStatDescriptions();
	// Tell them we're done
	StatNotifyManager.EndDescriptions();
}

/**
 * Determines if this stat group is on the network disabled list. The network
 * exclusion list is an INI setting used to disable entire stat groups from
 * sending network information
 *
 * @param InDesc the group description to check
 *
 * @return FALSE if the group is on the exclusion list, TRUE otherwise
 */
UBOOL FStatManager::IsGroupNetEnabled(const TCHAR* InDesc)
{
	// If the array is empty, then all groups are enabled
	if (NetEnabledGroups.Num() > 0)
	{
		// Search for a matching group name
		for (INT Index = 0; Index < NetEnabledGroups.Num(); Index++)
		{
			if (appStricmp(*NetEnabledGroups(Index),InDesc) == 0)
			{
				// In the enabled list
				return TRUE;
			}
		}
		// Not in the enabled list
		return FALSE;
	}
	return TRUE;
}

/**
 * Creates the groups that are registered and places them in a hash
 */
void FStatManager::CreateGroups(void)
{
	// Read the INI setting for enable groups
	FString Enabled = GConfig->GetStr(TEXT("FStatManager"),TEXT("NetEnabledGroups"),GEngineIni);
	Enabled.ParseIntoArray(&NetEnabledGroups,TEXT(","),TRUE);
	// Create all of the group objects
	for (const FStatGroupFactory* Factory = FStatGroupFactory::FirstFactory;
		Factory != NULL; Factory = Factory->NextFactory)
	{
		// Make sure two groups aren't registered for the same id
		check(GroupMap.HasKey(Factory->GroupId) == FALSE);
		// Create the stat group
		FStatGroup* Group = Factory->CreateGroup(FirstGroup);
		// Link it in as the first
		FirstGroup = Group;
		// And add it to the map for fast look ups later
		GroupMap.Set(Factory->GroupId,Group);
		// Update whether this is enabled or not
		Group->bIsNetEnabled = IsGroupNetEnabled(Group->Desc);
	}
	// Don't let any more groups be created
	FStatGroup::bIsPostInit = TRUE;
}

/**
 * Creates the counter/accumulator stats and adds them to there groups and
 * various hashes
 */
void FStatManager::CreateCountersAccumulators(void)
{
	// Walk the factory list creating any non-cycle counter stat
	for (const FStatFactory* Factory = FStatFactory::FirstFactory;
		Factory != NULL; Factory = Factory->NextFactory)
	{
		check(Factory->StatType < STATTYPE_Error);
		// Don't create stats for cycle counters
		if (Factory->StatType > STATTYPE_CycleCounter)
		{
			// Create the stat
			FStatCommonData* NewStat = Factory->CreateStat();
			// Add to the complete list
			AllStatsMap.Add(NewStat->StatId,NewStat);
			// And add it to the correct counter/accumulator map
			switch (Factory->StatType)
			{
				case STATTYPE_AccumulatorDWORD:
				case STATTYPE_CounterDWORD:
				case STATTYPE_MemoryCounter:
				{
					DwordAccumulatorMap.Set(NewStat->StatId,
						(FStatAccumulatorDWORD*)NewStat);
					break;
				}
				case STATTYPE_AccumulatorFLOAT:
				case STATTYPE_CounterFLOAT:
				{
					FloatAccumulatorMap.Set(NewStat->StatId,
						(FStatAccumulatorFLOAT*)NewStat);
					break;
				}
			}
		}
	}
}

/**
 * Builds the mapping of statid to factories for creating it
 */
void FStatManager::BuildFactoryMaps(void)
{
	// Build the fast map of per stat factories
	for (const FStatFactory* Factory = FStatFactory::FirstFactory;
		Factory != NULL; Factory = Factory->NextFactory)
	{
		// Make sure two stats aren't registered for the same id
		check(StatFactoryMap.HasKey(Factory->StatId) == FALSE);
		// Add the stat factory to the hash
		StatFactoryMap.Set(Factory->StatId,Factory);
	}
}

/**
 * Create the canonical stats for each group
 */
void FStatManager::CreateCanonicalStats(void)
{
	// Iterate through each factory and create the canonical stat for it
	for (const FStatFactory* Factory = FStatFactory::FirstFactory;
		Factory != NULL; Factory = Factory->NextFactory)
	{
		// Only cycle counters have canonical versions
		if (Factory->StatType == STATTYPE_CycleCounter)
		{
			FStatGroup* Group = GetGroup(Factory->GroupId);
			check(Group);
			// Create the canonical stat and add it to the group
			FCycleStat* Stat = Factory->CreateStat(NULL,FALSE);
			Group->CanonicalCycleStats.Set(Stat->StatId,Stat);
		}
	}
}

/**
 * Releases any internally allocated state
 */
void FStatManager::Destroy(void)
{
#if USE_AG_PERFMON
	if(GAgPerfUtils)
	{
		delete GAgPerfUtils;
		GAgPerfUtils = NULL;
	}
#endif

	StatNotifyManager.Destroy();

	// Clean up stats objects
	for (FStatGroup* Group = FirstGroup; Group != NULL;)
	{
		FStatGroup* Delete = Group;
		Group = Group->NextGroup;
		delete Delete;
	}
}

/**
 * Returns the string name for the specified group id
 *
 * @param GroupId the id to find the name of
 *
 * @return the string name of the group
 */
const TCHAR* FStatManager::GetGroupName(DWORD GroupId)
{
	FStatGroup* Group = GetGroup(GroupId);
	// Return null if not found
	return Group ? Group->Desc : NULL;
}

/**
 * Returns the string name for the specified stat id
 *
 * @param StatId the id to find the name of
 *
 * @return the string name of the stat
 */
const TCHAR* FStatManager::GetStatName(DWORD StatId)
{
	// Use the factory to get the name
	const FStatFactory* Factory = StatFactoryMap.FindRef(StatId);
	// Return null if not found
	return Factory ? Factory->CounterName : NULL;
}

/**
 * Finds the cycle stat for the specified id for the currently running thread
 *
 * @param StatId the stat to find the cycle stat for
 * @param CurrentThreadId the thread id of the stat to match on
 */
const FCycleStat* FStatManager::GetCycleStat(DWORD StatId,DWORD CurrentThreadId)
{
	const FCycleStat* Stat = NULL;
	// Look up the factory in the hash
	const FStatFactory* Factory = StatFactoryMap.FindRef(StatId);
	check(Factory);
	// Bail if not the correct type
	if (Factory->StatType == STATTYPE_CycleCounter)
	{
		// Use that to find the correct group it's in
		FStatGroup* Group = GetGroup(Factory->GroupId);
		check(Group);
		// Now search through the group's stats to find the one for this thread
		for (FCycleStat* CurrStat = Group->FirstCycleStat;
			CurrStat != NULL && Stat == NULL;
			CurrStat = CurrStat->Next)
		{
			// If this is the right stat and the right thread
			if (CurrStat->StatId == StatId && CurrStat->ThreadId == CurrentThreadId)
			{
				Stat = CurrStat;
			}
		}
	}
	return Stat;
}

/**
 * Sends the stats data to the notifiers to send out via the network
 */
void FStatManager::AdvanceFrame(void)
{
	// This stat has to be manually tracked or it will be added without closing
	DWORD StartCycles = appCycles();

	// Keep track of calls to appSeconds & appCycles.
	static QWORD LastNumTimingCodeCalls;
	SET_DWORD_STAT(STAT_TimingCodeCalls,(DWORD)(GNumTimingCodeCalls - LastNumTimingCodeCalls));
	LastNumTimingCodeCalls = GNumTimingCodeCalls;

	// Tell it the frame we are on
	StatNotifyManager.SetFrameNumber(FrameNumber);
	if (FrameNumber > 0)
	{
		// For each group of stats, log their data
		for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
		{
			// Don't dump the default group or stats that are disabled from
			// network updating
			if (Group->GroupId != STATGROUP_Default && Group->bIsNetEnabled)
			{
				// Send each cycle stat to the providers
				for (FCycleStat* Stat = Group->FirstCycleStat; Stat != NULL; Stat = Stat->Next)
				{
					if ( Stat->NumCallsHistory.GetMostRecent() )
					{
						StatNotifyManager.WriteStat(Stat->StatId,
							Stat->GroupId,
							Stat->Parent ? Stat->Parent->StatId : STAT_Error,
							Stat->InstanceId,
							Stat->Parent ? Stat->Parent->InstanceId : 0,
							Stat->ThreadId,Stat->History.GetMostRecent(),Stat->NumCallsHistory.GetMostRecent());
					}
				}
				// Write out each float accumulator
				WriteStatList<FStatAccumulatorFLOAT>(Group->FirstFloatAccumulator);
				// Write out each dword accumulator
				WriteStatList<FStatAccumulatorDWORD>(Group->FirstDwordAccumulator);
				// Write out each float counter
				WriteStatList<FStatCounterFLOAT>(Group->FirstFloatCounter);
				// Write out each dword counter
				WriteStatList<FStatCounterDWORD>(Group->FirstDwordCounter);
				// Write out each memory counter
				WriteStatList<FMemoryCounter>(Group->FirstMemoryCounter);
			}
			// If the engine group has been disabled, still send the frame time
			else if (Group->GroupId == STATGROUP_Engine)
			{
				// Iterate looking for the frame time stat
				for (FCycleStat* Stat = Group->FirstCycleStat; Stat != NULL;
					Stat = Stat->Next)
				{
					// Only send the frame time stat
					if (Stat->StatId == STAT_FrameTime)
					{
						StatNotifyManager.WriteStat(Stat->StatId,
							Stat->GroupId,
							Stat->Parent ? Stat->Parent->StatId : STAT_Error,
							Stat->InstanceId,
							Stat->Parent ? Stat->Parent->InstanceId : 0,
							Stat->ThreadId,Stat->History.GetMostRecent(),Stat->NumCallsHistory.GetMostRecent());
					}
				}
			}
		}
	}
	// Iterate each cannonical stat advancing its frame
	for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
	{
		for (FStatGroup::FCanonicalStatIterator It(Group->CanonicalCycleStats); It; ++It)
		{
			It.Value()->AdvanceFrame();
		}
	}
	// Find the root stat in each thread and tell it to recursively update
	AdvanceFrameForThread();
	FrameNumber++;

	// Update the self-timer cycles manually
	if ( !IsInRenderingThread() )
	{
		SET_CYCLE_COUNTER( STAT_PerFrameCapture, (appCycles() - StartCycles), 1 );
	}
	// tell all pool threads to advance when they can
	GStatsFrameForPoolThreads.Increment();
}

/**
 * Advances the stats data for a given thread
 */
void FStatManager::AdvanceFrameForThread(void)
{
	// This stat has to be manually tracked or it will be added without closing
	DWORD StartCycles = appCycles();
	if (FrameNumber > 0)
	{
		// For each group of stats, advance them
		for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
		{
			if (Group->GroupId != STATGROUP_Default)
			{
				// Update the canonical stats for this group
				Group->UpdateCanonicalStats();
			}
		}
	}
	// This will get the root stat for the currently executing thread
	FCycleStat* Stat = (FCycleStat*)GetCycleStat(STAT_Root);
	if (Stat != NULL)
	{
		// Update the children recursively
		Stat->AdvanceFrame();
	}

	// Update the self-timer cycles manually
	if ( IsInRenderingThread() )
	{
		SET_CYCLE_COUNTER( STAT_PerFrameCaptureRT, (appCycles() - StartCycles), 1 );
	}
}

/**
 * Changes the currently pointed to stat to one the user chose. Used when
 * navigating hierarchical stats
 *
 * @param ViewChildStat - The child stat number to view
 */
void FStatManager::SelectChildStat(DWORD ViewChildStat)
{
	// Find the first stat, if not set
	InitCurrentRenderedStat();
	// Only search if the child was specified
	if (ViewChildStat > 0)
	{
		DWORD StatNum = 1;
		// Search through the current stat's child list for one that matches the #
		for (FCycleStat::FChildStatMapIterator It(CurrentRenderedStat->Children); It; ++It)
		{
			if (StatNum == ViewChildStat)
			{
				CurrentRenderedStat = It.Value();
				break;
			}
			StatNum++;
		}
	}
	// The parent was requested
	else
	{
		if (CurrentRenderedStat->Parent != NULL)
		{
			CurrentRenderedStat = (FCycleStat*)CurrentRenderedStat->Parent;
		}
	}
}

/**
 * Toggles the rendering state for the specified group
 *
 * @param GroupName the group to be enabled/disabled
 * @return FALSE group was not recognized
 */
UBOOL FStatManager::ToggleGroup(const TCHAR* GroupName)
{
	UBOOL bToggled = FALSE;

	// Find the matching group for this name. Search through the whole list
	// in case more than one group has the same name (possible if IDs are unique)
	for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
	{
		if (appStricmp(Group->Desc,GroupName) == 0)
		{
			NumRenderedGroups -= Group->bShowGroup;
			Group->bShowGroup ^= TRUE;
			NumRenderedGroups += Group->bShowGroup;
			bToggled = TRUE;

			// Reset individual show values to the value of the group
			// They will be different if a user group has been toggled
			for (FStatGroup::FCanonicalStatIterator CStatIt(Group->CanonicalCycleStats); CStatIt; ++CStatIt)
			{
				FStatCommonData* Stat = CStatIt.Value();
				Stat->bShowStat = Group->bShowGroup;
			}

			for (FStatGroup::FStatIterator StatIt(Group); StatIt; ++StatIt)
			{
				(*StatIt)->bShowStat = Group->bShowGroup;
			}
		}
	}

	// Only check 'user' groups (sets of stats saved by user) if this command didn't toggle normal group
	if ( (bToggled == FALSE) && (FString(GroupName) != "") )
	{
		TArray<FString> StatsArray;
		if ( GConfig->GetArray( TEXT("CustomStats"), *(FString(GroupName).ToUpper()), StatsArray, GEngineIni ) > 0 )
		{
			for (FStatGroupIterator GroupIt = GStatManager.GetGroupIterator(); GroupIt; ++GroupIt)
			{
				UBOOL bGroupIsUsed = FALSE;

				// Iterate over all stats in that group and check if any is enabled
				for (FStatGroup::FStatIterator StatIt(*GroupIt); StatIt; ++StatIt)
				{
					INT Index;
					if (StatsArray.FindItem(FString((*StatIt)->CounterName), Index))
					{
						bGroupIsUsed = TRUE;
						break;
					}
				}

				// Change stat's flag only if group is visable
				if (bGroupIsUsed)
				{
					for (FStatGroup::FCanonicalStatIterator CStatIt((*GroupIt)->CanonicalCycleStats); CStatIt; ++CStatIt)
					{
						INT Index;
						FStatCommonData *Stat = CStatIt.Value();
						if (StatsArray.FindItem(FString(Stat->CounterName), Index))
						{
							Stat->bShowStat = TRUE;
						}
						else
						{
							Stat->bShowStat = FALSE;
						}
					}

					for (FStatGroup::FStatIterator StatIt(*GroupIt); StatIt; ++StatIt)
					{
						INT Index;
						if (StatsArray.FindItem(FString((*StatIt)->CounterName), Index))
						{
							(*StatIt)->bShowStat = TRUE;
						}
						else
						{
							(*StatIt)->bShowStat = FALSE;
						}
					}
				}

				// Notify the stat manager of the change
				if ( ((*GroupIt)->bShowGroup == FALSE) && (bGroupIsUsed == TRUE) )
				{
					(*GroupIt)->bShowGroup = TRUE;
					bShowCycleCounters = FALSE;
					IncrementNumRendered();
				}
				else if ( ((*GroupIt)->bShowGroup == TRUE) && (bGroupIsUsed == TRUE) )
				{
					(*GroupIt)->bShowGroup = FALSE;
					bShowCycleCounters = TRUE;
					DecrementNumRendered();
				}
			}
		}
	}

	return bToggled;
}

/**
 * Toggles the rendering state for the specified stat
 *
 * @param StatName the stat to be enabled/disabled
 */
void FStatManager::ToggleStat(const TCHAR* StatName)
{
	DWORD UpdateStatId = STAT_Root;
	// Find the matching stat id for this name
	for (const FStatFactory* StatFactory = FStatFactory::FirstFactory;
		StatFactory != NULL;
		StatFactory = StatFactory->NextFactory)
	{
		if (appStricmp(StatFactory->CounterName,StatName) == 0)
		{
			UpdateStatId = StatFactory->StatId;
		}
	}
	// In case the user specified a bogus name
	if (UpdateStatId != STAT_Root)
	{
		FScopeLock sl(SyncObject);
		TArray<FStatCommonData*> Stats;
		// Search the hash for a corresponding set of stats
		AllStatsMap.MultiFind(UpdateStatId,Stats);
		// Iterate through toggling all of the values
		for (INT Index = 0; Index < Stats.Num(); Index++)
		{
			FStatCommonData* Stat = Stats(Index);
			Stat->bShowStat ^= TRUE;
		}
	}
}

/**
 * Dumps the name of each stats group to the specified output device
 *
 * @param Ar the device to write to
 */
void FStatManager::ListGroups(FOutputDevice& Ar)
{
	Ar.Log(TEXT("Available stat groups"));
	// Loop through the group list and print them all
	for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
	{
		Ar.Log(Group->Desc);
	}
}

/**
 * Dumps the name of each stat in the specified group to the output device
 *
 * @param Ar the device to write to
 */
void FStatManager::ListStatsForGroup(const TCHAR* Name,FOutputDevice& Ar)
{
	Ar.Logf(TEXT("Stats for group (%s)"),Name);
	// Loop through the group list and print them all
	for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
	{
		if (appStricmp(Name,Group->Desc) == 0)
		{
			// Now dump all of the counters by type
			Ar.Log(TEXT("Cycle Counters:"));
			ListStats<FCycleStat>(Group->FirstCycleStat,Ar);
			Ar.Log(TEXT("Counters:"));
			ListStats<FStatCounterDWORD>(Group->FirstDwordCounter,Ar);
			ListStats<FStatCounterFLOAT>(Group->FirstFloatCounter,Ar);
			ListStats<FStatAccumulatorDWORD>(Group->FirstDwordAccumulator,Ar);
			ListStats<FStatAccumulatorFLOAT>(Group->FirstFloatAccumulator,Ar);
			ListStats<FMemoryCounter>(Group->FirstMemoryCounter,Ar);
		}
	}
}

/** 
* Dumps name and size for all memory stats in a specified group
* 
* @param GroupID ID of group we want to dump
* @param Ar the device to write to
*/
void FStatManager::DumpMemoryStatsForGroup(DWORD GroupId,FOutputDevice& Ar)
{
	FStatGroup *Group = GetGroup(GroupId);

	if (Group)
	{
		for (FMemoryCounter* Stat = Group->FirstMemoryCounter; Stat != NULL; Stat = (FMemoryCounter*)Stat->Next)
		{
			Ar.Logf(TEXT("%s = %.2f MB"), Stat->CounterName, Stat->Value / (1024.0f * 1024.0f));
		}
	}
}

/**
 * Disables the rendering state for all groups
 */
void FStatManager::DisableAllGroups(void)
{
	// Iterate through and turn
	for (FStatGroup* Group = FirstGroup; Group != NULL; Group = Group->NextGroup)
	{
		Group->bShowGroup = FALSE;
	}
	// Make sure it works with hierarchical
	bShowHierarchical = FALSE;
	// Currently no stats are rendered.
	NumRenderedGroups = 0;
}

/**
 * Processes any stat specific exec commands
 *
 * @param Cmd the command to parse
 * @param Ar the output device to write data to
 *
 * @return TRUE if processed, FALSE otherwise
 */
UBOOL FStatManager::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if (ParseCommand(&Cmd,TEXT("INCLUSIVE")))
	{
		bShowInclusive ^= TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("EXCLUSIVE")))
	{
		bShowExclusive ^= TRUE;
		// Just to help people out
		if (bShowExclusive && StatRenderingMode == SRM_Grouped)
		{
			debugf(TEXT("Exlusive stats are only shown in hierarchical mode"));
		}
	}
	// Toggles rendering of cycle counters
	else if (ParseCommand(&Cmd,TEXT("CYCLES")))
	{
		bShowCycleCounters ^= TRUE;
	}
	// Toggles rendering of counters/accumulators
	else if (ParseCommand(&Cmd,TEXT("COUNTERS")))
	{
		bShowCounters ^= TRUE;
	}
	else if (ParseCommand(&Cmd,TEXT("GROUPED")))
	{
		StatRenderingMode = SRM_Grouped;
	}
	else if (ParseCommand(&Cmd,TEXT("SLOW")))
	{
		// Parse threshold and min duration.
		FString SlowStatThresholdString(ParseToken(Cmd, 0));
		FString MinSlowStatDurationString(ParseToken(Cmd, 0));
		SlowStatThreshold	= SlowStatThresholdString.Len() ? appAtof(*SlowStatThresholdString) : 0.01f;
		MinSlowStatDuration = MinSlowStatDurationString.Len() ? appAtof(*MinSlowStatDurationString) : 10.f;
		// Change rendering mode to only display slow cycle stats.
		StatRenderingMode = SRM_Slow;
	}
	else if ( (ParseCommand(&Cmd,TEXT("HIER"))) || (ParseCommand(&Cmd,TEXT("HIERARCHY"))) )
	{
		StatRenderingMode = SRM_Hierarchical;
		bShowHierarchical ^= TRUE;
	}
	// User is navigating the stats tree during hierarchical rendering
	else if (ParseCommand(&Cmd,TEXT("NAV")))
	{
		// Read the stat they want to look at
		FString Stat = ParseToken(Cmd,0);
		DWORD StatToMoveTo = appAtoi(*Stat);
		// Go find the specified stat
		SelectChildStat(StatToMoveTo);
	}
	// Handle changing color of items via an exec
	else if (ParseCommand(&Cmd,TEXT("COLOR")))
	{
		// @todo joeg add color change support here
	}
	// Enables/disables a given stat
	else if (ParseCommand(&Cmd,TEXT("NAME")))
	{
		FString Name = ParseToken(Cmd,0);
		ToggleStat(*Name);
	}
	// Dumps the names of all groups
	else if (ParseCommand(&Cmd,TEXT("LIST")))
	{
		// Lists the availble sets of groups
		if (ParseCommand(&Cmd,TEXT("GROUPS")))
		{
			ListGroups(Ar);
		}
		else if (ParseCommand(&Cmd,TEXT("SETS")))
		{
			// Find all unique keys in "CustomStats" section in .ini file
			FConfigSection* Section = GConfig->GetSectionPrivate( TEXT("CustomStats"), FALSE, TRUE, GEngineIni );
			if (Section)
			{
				TArray<FName> UniqueCustomStatsGroupsKeys;
				for(FConfigSectionMap::TConstIterator It(*Section);It;++It)
				{
					UniqueCustomStatsGroupsKeys.AddUniqueItem(It.Key());
				}
				if (UniqueCustomStatsGroupsKeys.Num() > 0)
				{
					Ar.Log(TEXT("Saved Sets:"));
					for (INT i = 0; i < UniqueCustomStatsGroupsKeys.Num(); ++i)
					{
						Ar.Log(*UniqueCustomStatsGroupsKeys(i).ToString());
					}
				}
			}
		}
		else if (ParseCommand(&Cmd,TEXT("GROUP")))
		{
			FString Name = ParseToken(Cmd,0);
			ListStatsForGroup(*Name,Ar);
		}
	}
	else if (ParseCommand(&Cmd,TEXT("SAVE")))
	{
		TArray<FString> EnabledStats;
		FString Name = ParseToken(Cmd,0).ToUpper();
		if (Name != "")
		{
			// Iterate over all groups and if group is visable, store (by name) all enabled stats in that group
			for (FStatGroupIterator GroupIt = GStatManager.GetGroupIterator(); GroupIt; ++GroupIt)
			{
				if ((*GroupIt)->bShowGroup)
				{
					for (FStatGroup::FStatIterator StatIt(*GroupIt); StatIt; ++StatIt)
					{
						if ((*StatIt)->bShowStat)
						{
							EnabledStats.AddItem(FString((*StatIt)->CounterName));
						}
					}
				}
			}

			// Write data even if array is empty to clear particular set in .ini file
			GConfig->SetArray(TEXT("CustomStats"), *Name, EnabledStats, GEngineIni); 
		}
	}
	// Disables all stat groups
	else if (ParseCommand(&Cmd,TEXT("NONE")))
	{
		DisableAllGroups();
		// Disable SRM_Slow!
		StatRenderingMode = SRM_Grouped;
	}
	else if( ParseCommand( &Cmd, TEXT( "STARTFILE" ) ) )
	{
		// Start capturing stat file
		bIsRecordingStats = TRUE;
		StatNotifyManager.StartWritingStatsFile();
	}
	else if( ParseCommand( &Cmd, TEXT( "STOPFILE" ) ) )
	{
		// Flush any pending file writes
		bIsRecordingStats = FALSE;
		StatNotifyManager.StopWritingStatsFile();
	}
	else if( ParseCommand( &Cmd, TEXT( "FONTSCALE" ) ) )
	{
		FString Parameter(ParseToken(Cmd, 0));

		if (Parameter.Len())
		{
			StatFontScale = Max(appAtof(*Parameter), 0.0f);
		}
		else
		{
			Ar.Logf( TEXT("Usage: STAT FONTSCALE <Scale>") );
		}	
	}
	else
	{
		// Enables/disables a given stat group
		FString Name = ParseToken(Cmd,0);
		
		if(!ToggleGroup(*Name))
		{
			return FALSE;
		}
	}

	// add successful code path fall through
	return TRUE;
}

#endif // STATS

