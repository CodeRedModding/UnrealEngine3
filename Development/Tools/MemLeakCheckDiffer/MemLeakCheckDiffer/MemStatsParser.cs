/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/*
Log: Memory Allocation Status
Log: Allocs       445284 Current /  8686224 Total
Log: 
Log: DmQueryTitleMemoryStatistics
Log: TotalPages              122880
Log: AvailablePages          10278
Log: StackPages              436
Log: VirtualPageTablePages   67
Log: SystemPageTablePages    0
Log: VirtualMappedPages      42217
Log: ImagePages              7344
Log: FileCachePages          256
Log: ContiguousPages         62136
Log: DebuggerPages           0
Log: 
Log: GlobalMemoryStatus
Log: dwTotalPhys             536870912
Log: dwAvailPhys             42098688
Log: dwTotalVirtual          805175296
Log: dwAvailVirtual          628518912
*/

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using EpicCommonUtilities;

namespace MemLeakDiffer
{
    class MemStatsReportSection : KeyValuePairSection
    {
        public const int INVALID_SIZE = -999999;

        public int TitleFreeKB = INVALID_SIZE;
        public int LowestRecentTitleFreeKB = INVALID_SIZE;
        public int AllocUnusedKB = 0;
        public int AllocUsedKB = 0;
        public int AllocPureOverheadKB = 0; // not being captured yet
		public double Xbox_RecentLowestAvailPagesAgo = 0.0;

        /*
		public double PhysicalFreeMemKB = INVALID_SIZE;
		public double PhysicalUsedMemKB = INVALID_SIZE;
		public double TaskResidentKB = INVALID_SIZE;
		public double TaskVirtualKB = INVALID_SIZE;
         */

		public double HighestRecentMemoryAllocatedKB = INVALID_SIZE;
		public double HighestMemoryAllocatedKB = INVALID_SIZE;
		public double iOS_TaskResidentRecentPeakAgo = 0.0;
        public double iOS_TaskVirtualRecentPeakAgo = 0.0;

        public Dictionary<string, MemPoolSet> Pools = new Dictionary<string, MemPoolSet>();

        public class MemPoolSet
        {
            /// <summary>
            /// Memory zone type (Physical 'Cached', Physical 'WriteCombined', 'Virtual')
            /// </summary>
            public string SetType;

            /// <summary>
            /// List of blocks, each providing an allocation pool for a single discrete size
            /// </summary>
            public List<MemPoolLine> Blocks = new List<MemPoolLine>();

            /// <summary>
            /// Create a clone of this pool set and return it
            /// </summary>
            public MemPoolSet Clone()
            {
                MemPoolSet Result = new MemPoolSet();

                Result.SetType = SetType;

                foreach (MemPoolLine BlockEntry in Blocks)
                {
                    Result.Blocks.Add(BlockEntry.Clone());
                }

                return Result;
            }

            /// <summary>
            /// Returns the total memory for this pool where
            ///    TotalMemUsed = TotalGood + TotalWaste;
            ///    Efficiency = TotalGood/(TotalGood + TotalWaste) * 100%
            /// </summary>
            public void Calculate(out int TotalGood, out int TotalWaste)
            {
                // Calculate waste and total size
                TotalWaste = 0;
                TotalGood = 0;

                foreach (MemPoolLine BlockEntry in Blocks)
                {
                    int Waste;
                    int Good;
                    BlockEntry.Calculate(out Good, out Waste);
                    TotalGood += Good;
                    TotalWaste += Waste;
                }
            }

            /// <summary>
            /// Adds a new pool for a given block and element size.
            /// 
            /// Multiple calls to this method should be done from smallest to largest, otherwise GetPoolForSize will be non-optimal
            /// </summary>
            /// <param name="BlockSize"></param>
            /// <param name="ElementSize"></param>
            public void AddEmptyPool(int BlockSize, int ElementSize)
            {
                MemPoolLine NewBlock = new MemPoolLine();
                NewBlock.BlockSize = BlockSize;
                NewBlock.ElementSize = ElementSize;

                // Verify that this block is being added in the correct order
                if (Blocks.Count > 0)
                {
                    Debug.Assert(Blocks[Blocks.Count - 1].ElementSize < NewBlock.ElementSize);
                }

                Blocks.Add(NewBlock);
            }

            /// <summary>
            /// Construct an empty pool set
            /// </summary>
            public MemPoolSet()
            {
            }

            /// <summary>
            /// Construct a pool set with no allocations from an array of element sizes
            /// </summary>
            /// <param name="BlockSize">Backing allocation granularity.  This is the size of each OS request when an individual element pool needs more memory.</param>
            /// <param name="PoolSizeList">List of element sizes, which should be strictly-positive and in sorted ascending order</param>
            public MemPoolSet(int BlockSize, int[] PoolSizeList)
            {
                int LastSize = 0;

                foreach (int Size in PoolSizeList)
                {
                    // Verify strictly-positive, ascending order
                    Debug.Assert(LastSize < Size);

                    LastSize = Size;

                    // Create a pool for it
                    AddEmptyPool(BlockSize, Size);
                }
            }

            /// <summary>
            /// Returns the smallest pool that can satisfy a request of Size (caring only about ElementSize, not actual free elements available ATM)
            /// </summary>
            public MemPoolLine GetPoolForSize(int Size)
            {
                // Working with the assumption that the pools are sorted by size
                foreach (MemPoolLine Block in Blocks)
                {
                    if (Block.ElementSize >= Size)
                    {
                        return Block;
                    }
                }

                return null;
            }

            /// <summary>
            /// Casts all allocations from Source into this pool.  Throws an exception if any cannot be fit
            /// </summary>
            public void Recast(MemPoolSet Source)
            {
                foreach (MemPoolLine SrcPool in Source.Blocks)
                {
                    MemPoolLine DestPool = GetPoolForSize(SrcPool.ElementSize);

                    if (DestPool == null)
                    {
                        throw new Exception("No pools were large enough for a source allocation");
                    }

                    DestPool.AddElements(SrcPool.CurAllocs);
                }
            }

            public void MakeEmpty()
            {
                foreach (MemPoolLine Block in Blocks)
                {
                    Block.MakeEmpty();
                }
            }
        }

        /// <summary>
        /// Represents a single pool for a fixed allocation size, being doled out from one of a set of pages.
        /// </summary>
        public class MemPoolLine
        {
            // There are more fields, but they're all derived values
            public int BlockSize = 65536;
            public int ElementSize;
            public int NumBlocks;
            public int CurAllocs;

            public void MakeEmpty()
            {
                NumBlocks = 0;
                CurAllocs = 0;
            }

            public MemPoolLine Clone()
            {
                MemPoolLine result = new MemPoolLine();

                result.BlockSize = BlockSize;
                result.ElementSize = ElementSize;
                result.NumBlocks = NumBlocks;
                result.CurAllocs = CurAllocs;
                
                return result;
            }

            public void AddElements(int Count)
            {
                CurAllocs += Count;
                int AllocsPerBlock = BlockSize / ElementSize;
                NumBlocks = (CurAllocs + (AllocsPerBlock - 1)) / AllocsPerBlock;
            }

            public MemPoolLine()
            {
                ElementSize = BlockSize;
                NumBlocks = 0;
                CurAllocs = 0;
            }

            public void Calculate(out int TotalBytes, out int TotalWaste)
            {
                TotalWaste = NumBlocks * BlockSize - CurAllocs * ElementSize;
                TotalBytes = CurAllocs * ElementSize;
            }

            public MemPoolLine(string SourceTextLine, int CurAllocIndex)
            {
                // Remove KB indicators
                SourceTextLine = SourceTextLine.Replace('K', ' ');

                // Split into cells
                char[] Separators = {' '};
                string[] Cells = SourceTextLine.Split(Separators, StringSplitOptions.RemoveEmptyEntries);

                // Grab the interesting cells
                ElementSize = int.Parse(Cells[0]);
                NumBlocks = int.Parse(Cells[1]);
                CurAllocs = int.Parse(Cells[CurAllocIndex]);
            }
        }
    }


    /// <summary>
    /// Parser for the mem stats report from a pooled allocator
    /// </summary>
    class MemStatsParser : SmartParser
    {
        /// <summary>
        /// Parses one sub-report for a given memory type (header + a set of lines, one for each element size)
        /// </summary>
        public MemStatsReportSection.MemPoolSet ParsePool(string StartingLine, string[] Items, ref int i)
        {
            MemStatsReportSection.MemPoolSet Pool = new MemStatsReportSection.MemPoolSet();
            Pool.SetType = StartingLine.Substring("CacheType ".Length);

            string Headers;

            ReadLine(Items, ref i, out Headers);
            Ensure(Items, ref i, "----------");

            // Determine the current allocation column (varies depending on the age of the memleakcheck file

            // Newer Rift-era: Block Size Num Pools Max Pools Cur Allocs Total Allocs Min Req Max Req Mem Used Mem Align Efficiency
            // Rift-era:   Block Size Num Pools Available Exhausted Cur Allocs Mem Used  Mem Waste Efficiency  Page Loss
            // Gears2-era: Block Size Num Pools Cur Allocs Mem Used  Mem Waste Efficiency
            int CurAllocColumn = 2;

            if (Headers.Contains("Available Exhausted"))
            {
                CurAllocColumn += 2;
            }
            else if (Headers.Contains("Cur Allocs Total Allocs"))
            {
                CurAllocColumn += 1;
            }

            bool bEndOfSection = false;

            do
            {
                string line;
                
                bEndOfSection = !ReadLine(Items, ref i, out line);

                Debug.Assert(!bEndOfSection);

                if (line.StartsWith("BlkOverall") || (line.Trim() == ""))
                {
                    bEndOfSection = true;
                }
                else
                {
                    Pool.Blocks.Add(new MemStatsReportSection.MemPoolLine(line, CurAllocColumn));
                }
            }
            while (!bEndOfSection);

            return Pool;
        }

        // Parses "[Used / MaxSize] KByte" and returns Used, MaxSize
        protected void ParseStackUsageValues(string ValueOfStackLine, out float Used, out float MaxSize)
        {
            int OpenBracket = ValueOfStackLine.IndexOf('[');
            int CloseBracket = ValueOfStackLine.IndexOf(']');
            int Divider = ValueOfStackLine.IndexOf('/');

            string UsedString = ValueOfStackLine.Substring(OpenBracket + 1, Divider - OpenBracket - 1).Trim();
            string MaxSizeString = ValueOfStackLine.Substring(Divider + 1, CloseBracket - Divider - 1).Trim();

            Used = float.Parse(UsedString);
            MaxSize = float.Parse(MaxSizeString);
        }


        public override ReportSection Parse(string[] Items, ref int i)
        {
            MemStatsReportSection Result = new MemStatsReportSection();
            Result.MyHeading = "Memory Allocation Status";
            Result.bCanBeFilteredBySize = false;

            int TitleFreePages = MemStatsReportSection.INVALID_SIZE;
            int Xbox_LowestAvailPages = MemStatsReportSection.INVALID_SIZE;
            int Xbox_RecentLowestAvailPages = MemStatsReportSection.INVALID_SIZE;
			int iOS_PhysicalFree = MemStatsReportSection.INVALID_SIZE;
			int iOS_PhysicalUsed = MemStatsReportSection.INVALID_SIZE;
			int iOS_TaskResident = MemStatsReportSection.INVALID_SIZE;
            int iOS_TaskResidentPeak = MemStatsReportSection.INVALID_SIZE;
            int iOS_TaskResidentRecentPeak = MemStatsReportSection.INVALID_SIZE;
            int iOS_TaskVirtual = MemStatsReportSection.INVALID_SIZE;
            int iOS_TaskVirtualPeak = MemStatsReportSection.INVALID_SIZE;
            int iOS_TaskVirtualRecentPeak = MemStatsReportSection.INVALID_SIZE;
            string GMainThreadMemStackString = null;
            string GRenderThreadMemStackString = null;
            int Binned_AllocatedFromOS = MemStatsReportSection.INVALID_SIZE;
            int Binned_AllocatedFromOSRecentPeak = MemStatsReportSection.INVALID_SIZE;
            int TBB_AllocatedFromOS = MemStatsReportSection.INVALID_SIZE;

            string line;

            // Read the rest of the lines, ignoring most of them
            while (ReadLine(Items, ref i, out line))
            {
                // Global
                ParseLineKeyValuePair(line, "GMainThreadMemStack allocation size [used/ unused]", ref GMainThreadMemStackString);
                ParseLineKeyValuePair(line, "GRenderingThreadMemStack allocation size [used/ unused]", ref GRenderThreadMemStackString);

                // iOS (iPhone & iPad)
                ParseLineIntInMiddle(line, "iOS_PhysicalFree", ref iOS_PhysicalFree);
                ParseLineIntInMiddle(line, "iOS_PhysicalUsed", ref iOS_PhysicalUsed);
                ParseLineIntInMiddle(line, "iOS_TaskResident", ref iOS_TaskResident);
                ParseLineIntInMiddle(line, "iOS_TaskResidentPeak", ref iOS_TaskResidentPeak);
                ParseLineIntInMiddle(line, "iOS_TaskResidentRecentPeak", ref iOS_TaskResidentRecentPeak);
                ParseLineFloatInMiddle(line, "iOS_TaskResidentRecentPeakAgo", ref Result.iOS_TaskResidentRecentPeakAgo);
                ParseLineIntInMiddle(line, "iOS_TaskVirtual", ref iOS_TaskVirtual);
                ParseLineIntInMiddle(line, "iOS_TaskVirtualPeak", ref iOS_TaskVirtualPeak);
                ParseLineIntInMiddle(line, "iOS_TaskVirtualRecentPeak", ref iOS_TaskVirtualRecentPeak);
                ParseLineFloatInMiddle(line, "iOS_TaskVirtualRecentPeakAgo", ref Result.iOS_TaskVirtualRecentPeakAgo);

                // Binned (Win32 & WiiU)
                ParseLineIntInMiddle(line, "Binned_AllocatedFromOS", ref Binned_AllocatedFromOS);
                ParseLineIntInMiddle(line, "Binned_AllocatedFromOSRecentPeak", ref Binned_AllocatedFromOSRecentPeak);

                // TBB (Win64)
                ParseLineIntInMiddle(line, "TBB_AllocatedFromOS", ref TBB_AllocatedFromOS);

                // Xbox
                ParseLineIntInMiddle(line, "AvailablePages", ref TitleFreePages);
                ParseLineIntInMiddle(line, "Xbox_LowestAvailPages", ref Xbox_LowestAvailPages);
                ParseLineIntInMiddle(line, "Xbox_RecentLowestAvailPages", ref Xbox_RecentLowestAvailPages);
                ParseLineFloatInMiddle(line, "Xbox_RecentLowestAvailPagesAgo", ref Result.Xbox_RecentLowestAvailPagesAgo);

                // Collect memory usage
                int UnusedMem = 0;

                if (ParseLineIntInMiddle(line, "TOTAL UNUSED MEMORY:", ref UnusedMem))
                {
                    Result.AllocUnusedKB += UnusedMem;
                }

                // Go into pool parsing mode if it's the start of a new pool
                if (line.StartsWith("CacheType"))
                {
                    MemStatsReportSection.MemPoolSet Pool = ParsePool(line, Items, ref i);

                    Result.Pools.Add(Pool.SetType, Pool);
                }
            }

            // Add Xbox samples if available
            Result.TitleFreeKB = TitleFreePages * 4;
            Result.LowestRecentTitleFreeKB = (Xbox_RecentLowestAvailPages != MemStatsReportSection.INVALID_SIZE) ? Xbox_RecentLowestAvailPages * 4 : Result.TitleFreeKB;

            if (Xbox_LowestAvailPages != MemStatsReportSection.INVALID_SIZE)
            {
                Result.AddSample("Xbox_LowestAvailPages", -8, (double)Xbox_LowestAvailPages / 1024, EStatType.Minimum);
            }

            if (Xbox_RecentLowestAvailPages != MemStatsReportSection.INVALID_SIZE)
            {
                Result.AddSample("Xbox_RecentLowestAvailPages", -8, (double)Xbox_RecentLowestAvailPages / 1024, EStatType.Minimum);
            }
            
            // Add iOS samples if available
            Result.HighestRecentMemoryAllocatedKB = (double)iOS_TaskResidentRecentPeak / 1024.0;
            Result.HighestMemoryAllocatedKB = (double)iOS_TaskResidentPeak / 1024.0;

            if (iOS_PhysicalFree != MemStatsReportSection.INVALID_SIZE)
            {
                Result.AddSample("iOS_PhysicalFreeMem", -8, (double)iOS_PhysicalFree / 1024.0, EStatType.Minimum);
                Result.AddSample("iOS_PhysicalUsedMem", -7, (double)iOS_PhysicalUsed / 1024.0, EStatType.Minimum);
                Result.AddSample("iOS_TaskResident", -8, (double)iOS_TaskResident / 1024.0, EStatType.Maximum);
                Result.AddSample("iOS_TaskResidentPeak", -8, (double)iOS_TaskResidentPeak / 1024.0, EStatType.Maximum);
                Result.AddSample("iOS_TaskResidentRecentPeak", -8, (double)iOS_TaskResidentRecentPeak / 1024.0, EStatType.Maximum);
                Result.AddSample("iOS_TaskVirtual", -8, (double)iOS_TaskVirtual / 1024.0, EStatType.Maximum);
                Result.AddSample("iOS_TaskVirtualPeak", -8, (double)iOS_TaskVirtualPeak / 1024.0, EStatType.Maximum);
                Result.AddSample("iOS_TaskVirtualRecentPeak", -8, (double)iOS_TaskVirtualRecentPeak / 1024.0, EStatType.Maximum);
            }

            // Add Binned samples if available
            if (Binned_AllocatedFromOS != MemStatsReportSection.INVALID_SIZE)
            {
                Result.AddSample("Binned_AllocatedFromOS", -8, (double)Binned_AllocatedFromOS / 1024, EStatType.Maximum);
            }

            if (Binned_AllocatedFromOSRecentPeak != MemStatsReportSection.INVALID_SIZE)
            {
                Result.AddSample("Binned_AllocatedFromOSRecentPeak", -7, (double)Binned_AllocatedFromOSRecentPeak / 1024, EStatType.Maximum);
            }

            // Add TBB samples if available
            if (TBB_AllocatedFromOS != MemStatsReportSection.INVALID_SIZE)
            {
                Result.AddSample("TBB_AllocatedFromOS", -8, (double)TBB_AllocatedFromOS / 1024, EStatType.Maximum);
            }

            const int MiscStatPriority = 21;

            // Parse stack usage values
            if (GMainThreadMemStackString != null)
            {
                float Used;
                float MaxSize;

                ParseStackUsageValues(GMainThreadMemStackString, out Used, out MaxSize);

                Result.AddSample("GMainThreadMemStackUsed", MiscStatPriority, Used, EStatType.Maximum);
                Result.AddSample("GMainThreadMemStackMax", MiscStatPriority, MaxSize, EStatType.Maximum);
            }

            if (GRenderThreadMemStackString != null)
            {
                float Used;
                float MaxSize;

                ParseStackUsageValues(GRenderThreadMemStackString, out Used, out MaxSize);

                Result.AddSample("GRenderingThreadMemStackUsed", MiscStatPriority, Used, EStatType.Maximum);
                Result.AddSample("GRenderingThreadMemStackMax", MiscStatPriority, MaxSize, EStatType.Maximum);
            }

            // Add samples for memory usage if available
            foreach (MemStatsReportSection.MemPoolSet pool in Result.Pools.Values)
            {
                int Good;
                int Waste;

                pool.Calculate(out Good, out Waste);

                Result.AllocUsedKB += Good / 1024;
            }

            if (Result.AllocUnusedKB != 0)
            {
                Result.AddSample("AllocatorUnused", -9, Result.AllocUnusedKB, EStatType.Maximum);
            }

            if (Result.AllocUsedKB != 0)
            {
                Result.AddSample("AllocatorUsed", -9, Result.AllocUsedKB, EStatType.Maximum);
            }
            
            return Result;
        }
    }


    class MemStatsReportSectionPS3 : TrueKeyValuePairSection
    {
        public static int PS3AdjustKB = 0;

        protected bool bSeenHostMemoryInfo = false;

        public MemStatsReportSectionPS3()
            : base("Memory Allocation Status (PS3)")
        {
            // Lines are of the form "System memory total:           210.00 MByte (220200960 Bytes)"
            Separators = new char[] { ':' };

            MyHeading = "Memory Allocation Status (PS3)";
            MyBasePriority = -8;
            bAdditiveMode = false;
            bCanBeFilteredBySize = false;

            AddMappedSample("System memory total", "SystemMemoryTotal", EStatType.Minimum);
            AddMappedSample("System memory available", "SystemMemoryAvailable", EStatType.Minimum);
            AddMappedSample("Available for malloc", "SystemMemoryAvailableForMalloc", EStatType.Minimum);
            AddMappedSample("Total expected overhead", "AllocatorTotalExpectedOverhead", EStatType.Maximum);
            AddMappedSample("AllocatedRSXHostMemory", "AllocatedRSXHostMemory", EStatType.Maximum);
            AddMappedSample("AllocatedRSXLocalMemory", "AllocatedRSXLocalMemory", EStatType.Maximum);
        }

        protected override void AllowKeyValueAdjustment(ref string Key, ref string Value)
        {
            // PS3 memory report has total allocated twice, for a field we care about!
            // Log: GPU memory info:               249.00 MByte total
            // Log:   Total allocated:             232.03 MByte (243304208 Bytes)
            // Log: 
            // Log: Host memory info:               10.00 MByte total
            // Log:   Total allocated:               6.64 MByte (6960288 Bytes)
            if (Key == "Host memory info")
            {
                bSeenHostMemoryInfo = true;
            }
            else if (Key == "Total allocated")
            {
                if (bSeenHostMemoryInfo)
                {
                    Key = "AllocatedRSXHostMemory";
                }
                else
                {
                    Key = "AllocatedRSXLocalMemory";
                }
            }
        }

        /// <summary>
        /// Cooks the data
        /// </summary>
        public override void Cook()
        {
            base.Cook();

            // All of the read values are in MByte, so we normalize to KB here
            Dictionary<string, SampleRecord> NewValues = new Dictionary<string, SampleRecord>();

            foreach (var KVP in Samples)
            {
                NewValues[KVP.Key] = new SampleRecord(KVP.Value.Sample * 1024.0f, KVP.Value.Priority, KVP.Value.OverviewStatType);
            }

            Samples = NewValues;

            // Subtract off the devkit/testkit adjustment value from the total main memory readings
            Samples.Add("PS3AdjustmentSetting", new SampleRecord(PS3AdjustKB, -8, EStatType.Maximum));
            Samples["SystemMemoryAvailable"].Sample -= PS3AdjustKB;
            Samples["SystemMemoryAvailableForMalloc"].Sample -= PS3AdjustKB;
            Samples["SystemMemoryTotal"].Sample -= PS3AdjustKB;
        }
    }

    class MemStatsParserPS3 : SmartParser
    {
        public override ReportSection Parse(string[] Items, ref int i)
        {
            MemStatsReportSectionPS3 Result = new MemStatsReportSectionPS3();

            Result.Parse(Items, ref i);

            return Result;
        }
    }
}
