#include "FScopedMemoryStats.h"
#include "Core.h"

#if USE_SCOPED_MEM_STATS && STATS
UBOOL FScopedMemoryStats::IsScopedMemStatsActive = FALSE;
FScopedMemoryStats::FScopedMemoryStats(FOutputDevice& outputDevice, FString const& label)
:	OutputDevice(outputDevice), 
	Label(label)
{
	if (!IsScopedMemStatsActive) 
	{
		return;
	}
	GMalloc->GetAllocationInfo( StartingAllocationStats );
	FMemoryCounter* memCounter = GStatManager.GetGroup(STATGROUP_Memory)->FirstMemoryCounter;

	StartingMemStats.Empty();
	while(memCounter)
	{
		StartingMemStats.AddItem(memCounter->Value);
		memCounter = static_cast<FMemoryCounter*>(memCounter->Next);
	}
}

FScopedMemoryStats::~FScopedMemoryStats()
{
	if (!IsScopedMemStatsActive) 
	{
		return;
	}

	OutputDevice.Logf(TEXT("====================================="));
	OutputDevice.Logf(TEXT("ScopedMemoryStats: %s"), *Label);

	GMalloc->DumpAllocationsDeltas(OutputDevice, StartingAllocationStats);

	OutputDevice.Logf(TEXT("Stats Deltas:"));
	FMemoryCounter* memCounter = GStatManager.GetGroup(STATGROUP_Memory)->FirstMemoryCounter;
	INT index = 0;
	while (memCounter)
	{
		int statDelta = memCounter->Value - StartingMemStats(index);
		if (statDelta != 0) {
			OutputDevice.Logf(TEXT("	%s:    %4.4f MB (%4.6f KB)"), memCounter->CounterName, static_cast<INT>(statDelta / 1024)/ 1024.f, static_cast<INT>(statDelta )/ 1024.f);
		}
		index++;
		memCounter = static_cast<FMemoryCounter*>(memCounter->Next);
	}

	GMalloc->DumpAllocations(OutputDevice);
	OutputDevice.Logf(TEXT("====================================="));
}
#endif