/*=============================================================================
	D3D11Drv.cpp: Unreal D3D RHI library implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"

FLOAT FD3D11EventNode::GetTiming()
{
	FLOAT Result = 0;

	if (Timing.IsSupported())
	{
		// Get the timing result and block the CPU until it is ready
		const QWORD GPUTiming = Timing.GetTiming(TRUE);
		const QWORD GPUFreq = Timing.GetTimingFrequency();

		Result = DOUBLE(GPUTiming) / DOUBLE(GPUFreq);
	}

	return Result;
}

void FD3D11DynamicRHI::PushEvent(const TCHAR* Name)
{
	if (bTrackingEvents)
	{
		checkSlow(IsInRenderingThread());
		if (CurrentEventNode)
		{
			// Add to the current node's children
			CurrentEventNode->Children.AddItem(new FD3D11EventNode(Name, CurrentEventNode, this));
			CurrentEventNode = CurrentEventNode->Children.Last();
		}
		else
		{
			// Add a new root node to the tree
			CurrentEventNodeFrame->EventTree.AddItem(new FD3D11EventNode(Name, NULL, this));
			CurrentEventNode = CurrentEventNodeFrame->EventTree.Last();
		}

		check(CurrentEventNode);
		// Start timing the current node
		CurrentEventNode->Timing.StartTiming();
	}
}

void FD3D11DynamicRHI::PopEvent()
{
	if (bTrackingEvents)
	{
		checkSlow(IsInRenderingThread());
		check(CurrentEventNode);
		// Stop timing the current node and move one level up the tree
		CurrentEventNode->Timing.EndTiming();
		CurrentEventNode = CurrentEventNode->Parent;
	}
}

/** Recursively generates a histogram of nodes and stores their timing in TimingResult. */
void GatherStatsEventNode(FD3D11EventNode* Node, INT Depth, TMap<FString, FD3D11EventNodeStats>& EventHistogram)
{
	if (Node->NumDraws > 0 || Node->Children.Num() > 0)
	{
		Node->TimingResult = Node->GetTiming() * 1000.0f;

		FD3D11EventNodeStats* FoundHistogramBucket = EventHistogram.Find(Node->Name);
		if (FoundHistogramBucket)
		{
			FoundHistogramBucket->NumDraws += Node->NumDraws;
			FoundHistogramBucket->NumPrimitives += Node->NumPrimitives;
			FoundHistogramBucket->TimingResult += Node->TimingResult;
			FoundHistogramBucket->NumEvents++;
		}
		else
		{
			FD3D11EventNodeStats NewNodeStats;
			NewNodeStats.NumDraws = Node->NumDraws;
			NewNodeStats.NumPrimitives = Node->NumPrimitives;
			NewNodeStats.TimingResult = Node->TimingResult;
			NewNodeStats.NumEvents = 1;
			EventHistogram.Set(Node->Name, NewNodeStats);
		}

		for (INT ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			// Traverse children
			GatherStatsEventNode(Node->Children(ChildIndex), Depth + 1, EventHistogram);
		}
	}
}

/** Recursively dumps stats for each node with a depth first traversal. */
void DumpStatsEventNode(FD3D11EventNode* Node, FLOAT RootResult, INT Depth, INT& NumNodes, INT& NumDraws)
{
	NumNodes++;
	if (Node->NumDraws > 0 || Node->Children.Num() > 0)
	{
		NumDraws += Node->NumDraws;
		// Percent that this node was of the total frame time
		const FLOAT Percent = Node->TimingResult * 100.0f / (RootResult * 1000.0f);

		// Print information about this node, padded to its depth in the tree
		warnf(TEXT("%s%4.1f%%%5.1fms   %s %u draws %u prims"), 
			*FString(TEXT("")).LeftPad(Depth * 3), 
			Percent,
			Node->TimingResult,
			*Node->Name,
			Node->NumDraws,
			Node->NumPrimitives);

		FLOAT TotalChildTime = 0;
		UINT TotalChildDraws = 0;
		for (INT ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			FD3D11EventNode* ChildNode = Node->Children(ChildIndex);
			
			INT NumChildDraws = 0;
			// Traverse children
			DumpStatsEventNode(Node->Children(ChildIndex), RootResult, Depth + 1, NumNodes, NumChildDraws);
			NumDraws += NumChildDraws;

			TotalChildTime += ChildNode->TimingResult;
			TotalChildDraws += NumChildDraws;
		}

		const FLOAT UnaccountedTime = Max(Node->TimingResult - TotalChildTime, 0.0f);
		const FLOAT UnaccountedPercent = UnaccountedTime * 100.0f / (RootResult * 1000.0f);

		// Add an 'Unaccounted' node if necessary to show time spent in the current node that is not in any of its children
		if (Node->Children.Num() > 0 && TotalChildDraws > 0 && UnaccountedPercent > 2.0f && UnaccountedTime > .2f)
		{
			warnf(TEXT("%s%4.1f%%%5.1fms Unaccounted"), 
				*FString(TEXT("")).LeftPad((Depth + 1) * 3), 
				UnaccountedPercent,
				UnaccountedTime);
		}
	}
}

class FNodeStatsCompare
{
public:

	/** Sorts nodes by descending durations. */
	static INT Compare(const FD3D11EventNodeStats& A, const FD3D11EventNodeStats& B)
	{
		return A.TimingResult - B.TimingResult < 0 ? 1 : -1;
	}
};

/** Start this frame of per tracking */
void FD3D11EventNodeFrame::StartFrame()
{
	EventTree.Reset();
	DisjointQuery.StartTracking();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FD3D11EventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
	DisjointQuery.EndTracking();
}

/** Dumps perf event information. */
void FD3D11EventNodeFrame::DumpEventTree()
{
	if (EventTree.Num() > 0)
	{
		FLOAT RootResult = 0;

		if (RootEventTiming.IsSupported())
		{
			const QWORD GPUTiming = RootEventTiming.GetTiming(TRUE);
			const QWORD GPUFreq = RootEventTiming.GetTimingFrequency();

			RootResult = DOUBLE(GPUTiming) / DOUBLE(GPUFreq);
		}

		extern FLOAT GUnit_GPUFrameTime;
		warnf(TEXT("Perf marker hierarchy, total GPU time %.2fms (%.2fms smoothed)"), RootResult * 1000.0f, GUnit_GPUFrameTime);

		warnf(DisjointQuery.WasDisjoint() 
			? TEXT("Profiled range was disjoint!  GPU switched to doing something else while profiling.")
			: TEXT("Profiled range was continuous."));

		TMap<FString, FD3D11EventNodeStats> EventHistogram;
		for (INT BaseNodeIndex = 0; BaseNodeIndex < EventTree.Num(); BaseNodeIndex++)
		{
			GatherStatsEventNode(EventTree(BaseNodeIndex), 0, EventHistogram);
		}

		INT NumNodes = 0;
		INT NumDraws = 0;
		for (INT BaseNodeIndex = 0; BaseNodeIndex < EventTree.Num(); BaseNodeIndex++)
		{
			DumpStatsEventNode(EventTree(BaseNodeIndex), RootResult, 0, NumNodes, NumDraws);
		}

		//@todo - calculate overhead instead of hardcoding
		// This .012ms of overhead is based on what Nsight shows as the minimum draw call duration on a 580 GTX, 
		// Which is apparently how long it takes to issue two timing events.
		warnf(TEXT("Total Nodes %u Draws %u approx overhead %.2fms"), NumNodes, NumDraws, .012f * NumNodes);
		warnf(TEXT(""));
		warnf(TEXT(""));

		// Sort descending based on node duration
		EventHistogram.ValueSort<FNodeStatsCompare>();

		// Log stats about the node histogram
		warnf(TEXT("Node histogram %u buckets"), EventHistogram.Num());

		INT NumNotShown = 0;
		for (TMap<FString, FD3D11EventNodeStats>::TIterator It(EventHistogram); It; ++It)
		{
			const FD3D11EventNodeStats& NodeStats = It.Value();
			if (NodeStats.TimingResult > RootResult * 1000.0f * .005f)
			{
				warnf(TEXT("   %.2fms   %s   Events %u   Draws %u"), NodeStats.TimingResult, *It.Key(), NodeStats.NumEvents, NodeStats.NumDraws);
			}
			else
			{
				NumNotShown++;
			}
		}

		warnf(TEXT("   %u buckets not shown"), NumNotShown);
	}
}
