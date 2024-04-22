/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using EpicCommonUtilities;
using System.Globalization;
using System.IO;

namespace MemLeakDiffer
{
    /// <summary>
    /// Output structure of a single report, used to build the overall report summary
    /// </summary>
    class SeriesReportData
    {
        public string Name;

        public Dictionary<string, GroupSummary> Summaries = new Dictionary<string, GroupSummary>();

        public SeriesReportData(string InName)
        {
            Name = InName;
        }

        public void Add(GroupSummary Summary)
        {
            Summaries.Add(Summary.GroupName, Summary);
        }
    }

    class SequentialDiffReporter
    {
        /// <summary>
        /// Returns a string with "Min,Max,Avg,Median,StdDev", with Min and Max as int, Avg and StdDev as floats with only 2 decimal places
        /// </summary>
        /// <param name="value"></param>
        /// <returns></returns>
        protected static string IntegerStatToCSV(StatisticalFloat X, float SizeScaler)
        {
            return String.Format("{0:0.0},{1:0.0},{2:0.0},{3:0.0},{4:0.0}",
                X.Min * SizeScaler,
                X.Max * SizeScaler,
                X.Average * SizeScaler,
                X.Median * SizeScaler,
                X.StandardDeviation * SizeScaler);
        }

        List<MemLeakFile> Files;

        string SizeSuffix
        {
            get { return bUseMB ? "MB" : "KB"; }
        }
        
        float SizeScaler
        {
            get { return bUseMB ? 1.0f / 1024.0f : 1.0f; }
        }

        /// <summary>
        /// Controls how sizes are reported (if true, using MB, otherwise KB)
        /// </summary>
        public bool bUseMB;

        /// <summary>
        /// Contains a data series for each class group (or metadata group, etc...)
        /// </summary>
        GroupedClassInfoTracker GroupingData;

        // Streaming level info
        List<string> AllLevels = new List<string>();
        List<LoadedLevelsSection> LevelLists = new List<LoadedLevelsSection>();

        /// <summary>
        /// The priority labels will be used for the first group that's found in each priority range
        /// </summary>
        Dictionary<int, string> PriorityLabels = new Dictionary<int, string>();

        /// <summary>
        /// The coordinates for each capture
        /// </summary>
        List<string> BugItCoordinates = new List<string>();

        /// <summary>
        /// Creates a sequential diff reporter
        /// </summary>
        /// <param name="InFiles">List of files to process</param>
        public SequentialDiffReporter(List<MemLeakFile> InFiles)
        {
            Files = InFiles;

            // The priority labels will be used for the first group that's found in each priority range
            PriorityLabels.Add(-10, "GlobalStats");
            PriorityLabels.Add(0, "ImportantGroups");
            PriorityLabels.Add(10, "OtherGroups");
            PriorityLabels.Add(20, "MiscInfo");
            PriorityLabels.Add(25, "MemoryPools");
        }

        /// <summary>
        /// Generate a report as a .CSV file containing some interesting memory numbers from each file as well
        /// as first-order statistics for the entire series
        /// </summary>
        public void ProcessFiles(string GroupFilePath)
        {
            // Create the class info temporary data structure
            GroupingData = new GroupedClassInfoTracker(Files.Count);

            // Parse the group file if it exists
            if (File.Exists(GroupFilePath))
            {
                GroupingData.ParseGroupFile(GroupFilePath);
            }

            // Groups from KVP sections
            Dictionary<string, GroupInfo> KVP_Groups = new Dictionary<string, GroupInfo>();

            // Overall free memory groups
            GroupInfo ApproxFreeMemMetagroup = null;
            GroupInfo TFP_Metagroup = null;
            GroupInfo LowestTFP_Metagroup = null;
            GroupInfo LowestMemTime_Metagroup = null;

            GroupInfo CaptureTime_Metagroup = GroupingData.CreateMetaGroup("TimeElapsedSecs");
            CaptureTime_Metagroup.PriorityIndex = 20;
            CaptureTime_Metagroup.OverviewStatType = EStatType.Maximum;
            CaptureTime_Metagroup.SampleUnitType = ESampleUnit.Seconds;

            GroupInfo ReducePoolSize_Metagroup = GroupingData.CreateMetaGroup("ReducePoolSize");
            ReducePoolSize_Metagroup.PriorityIndex = 20;
            ReducePoolSize_Metagroup.OverviewStatType = EStatType.Average;

            // Sound information
            List<string> AllSounds = new List<string>();

            GroupInfo Sound_Metagroup = null;
#if REPORT_SOUND_SECTION
            Sound_Metagroup = GroupingData.CreateMetaGroup("AllSoundsCombined");
            Sound_Metagroup.PriorityIndex = 20;
#endif
            // The time of the first report
            DateTime? StartingEpoch = null;

            // Run through all of the files and collect all of the size data for every class into groupings
            for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
            {
                MemLeakFile LeakFile = Files[FileIndex];

                // Log out the reduce pool size value
                ReducePoolSize_Metagroup.SetSamplePoint(FileIndex, LeakFile.ReducePoolSizeKB);

                // Log out the capture time
                if (!StartingEpoch.HasValue)
                {
                    StartingEpoch = LeakFile.InternalDateStamp;
                }

                TimeSpan TimeSinceEpoch = LeakFile.InternalDateStamp - StartingEpoch.Value;
                double TimeSinceEpochInSeconds = TimeSinceEpoch.TotalSeconds;

                CaptureTime_Metagroup.SetSamplePoint(FileIndex, TimeSinceEpochInSeconds);
                
                // Grab all the class reports
                ObjDumpReportSection OD;
                if (LeakFile.FindFirstSection<ObjDumpReportSection>(out OD))
                {
                    foreach (ProcessedLine LineItem in OD.MyProcessedRows.Values)
                    {
                        ObjectDumpLine ClassInfo = (ObjectDumpLine)LineItem;
                        GroupingData.RegisterSize(ClassInfo.Name, FileIndex, ClassInfo.ApproxTotalSizeKB);
                    }
                }

                // Collect capture coordinates
                string CoordString = "(unknown)";

                BugItSection BugItSectionInst;
                if (LeakFile.FindFirstSection<BugItSection>(out BugItSectionInst))
                {
                    CoordString = BugItSectionInst.CoordinateString;
                }

                BugItCoordinates.Add(CoordString);

                // Collect memory statistics
                MemStatsReportSection MemStats;
                if (LeakFile.FindFirstSection<MemStatsReportSection>(out MemStats))
                {
                    if (MemStats.TitleFreeKB != 0)
                    {
                        if (TFP_Metagroup == null)
                        {
                            TFP_Metagroup = GroupingData.CreateMetaGroup("TitleFreeMemory");
                            TFP_Metagroup.PriorityIndex = -10;
                            TFP_Metagroup.OverviewStatType = EStatType.Minimum;

                            LowestTFP_Metagroup = GroupingData.CreateMetaGroup("LowestFreeMemory");
                            LowestTFP_Metagroup.PriorityIndex = -10;
                            LowestTFP_Metagroup.OverviewStatType = EStatType.Minimum;

                            LowestMemTime_Metagroup = GroupingData.CreateMetaGroup("TimeLowestFreeMemoryOccured");
                            LowestMemTime_Metagroup.PriorityIndex = 20;
                            LowestMemTime_Metagroup.bIsNotInterestingForSummary = true;
                            LowestMemTime_Metagroup.SampleUnitType = ESampleUnit.Seconds;

                            ApproxFreeMemMetagroup = GroupingData.CreateMetaGroup("ApproxFreeMemory");
                            ApproxFreeMemMetagroup.PriorityIndex = -9;
                            ApproxFreeMemMetagroup.OverviewStatType = EStatType.Minimum;
                        }

                        TFP_Metagroup.SetSamplePoint(FileIndex, MemStats.TitleFreeKB);
                        LowestTFP_Metagroup.SetSamplePoint(FileIndex, MemStats.LowestRecentTitleFreeKB);
                        LowestMemTime_Metagroup.SetSamplePoint(FileIndex, TimeSinceEpochInSeconds - MemStats.Xbox_RecentLowestAvailPagesAgo);
                        ApproxFreeMemMetagroup.SetSamplePoint(FileIndex, MemStats.AllocUnusedKB + MemStats.TitleFreeKB);
                    }

                    // Run through all the memory pools
                    foreach (MemStatsReportSection.MemPoolSet pool in MemStats.Pools.Values)
                    {                       
                        foreach (MemStatsReportSection.MemPoolLine BlockEntry in pool.Blocks)
                        {
                            int Waste;
                            int Good;
                            BlockEntry.Calculate(out Good, out Waste);

                            {
                                string Key = String.Format("Used {0} {1:000000}", pool.SetType, BlockEntry.ElementSize);

                                GroupInfo Group;
                                if (!KVP_Groups.TryGetValue(Key, out Group))
                                {
                                    Group = GroupingData.CreateMetaGroup(Key);
                                    Group.bCanBeFilteredBySize = false;
                                    Group.bIsNotInterestingForStandardReport = true;
                                    Group.PriorityIndex = 201;
                                    KVP_Groups.Add(Key, Group);
                                }

                                Group.SetSamplePoint(FileIndex, (double)Good / 1024.0);
                            }

                            {
                                string Key = String.Format("Unused {0} {1:000000}", pool.SetType, BlockEntry.ElementSize);

                                GroupInfo Group;
                                if (!KVP_Groups.TryGetValue(Key, out Group))
                                {
                                    Group = GroupingData.CreateMetaGroup(Key);
                                    Group.bCanBeFilteredBySize = false;
                                    Group.bIsNotInterestingForStandardReport = true;
                                    Group.PriorityIndex = 202;
                                    KVP_Groups.Add(Key, Group);
                                }

                                Group.SetSamplePoint(FileIndex, (double)Waste / 1024.0);
                            }

                            {
                                string Key = String.Format("Total {0} {1:000000}", pool.SetType, BlockEntry.ElementSize);

                                GroupInfo Group;
                                if (!KVP_Groups.TryGetValue(Key, out Group))
                                {
                                    Group = GroupingData.CreateMetaGroup(Key);
                                    Group.bCanBeFilteredBySize = false;
                                    Group.bIsNotInterestingForStandardReport = true;
                                    Group.PriorityIndex = 200;
                                    KVP_Groups.Add(Key, Group);
                                }

                                Group.SetSamplePoint(FileIndex, (double)(Good + Waste) / 1024.0);
                            }

                            {
                                string Key = String.Format("PctGood {0} {1:000000}", pool.SetType, BlockEntry.ElementSize);

                                GroupInfo Group;
                                if (!KVP_Groups.TryGetValue(Key, out Group))
                                {
                                    Group = GroupingData.CreateMetaGroup(Key);
                                    Group.bCanBeFilteredBySize = false;
                                    Group.bIsNotInterestingForStandardReport = true;
                                    Group.PriorityIndex = 203;
                                    KVP_Groups.Add(Key, Group);
                                }

                                double Sum = Good + Waste;

                                Group.SetSamplePoint(FileIndex, (Sum > 0) ? (100.0 * Good / Sum) : (100));
                            }
                        }                                            
                    }
                }

                // Collect loaded level information
                LoadedLevelsSection LevelSection;
                if (LeakFile.FindFirstSection<LoadedLevelsSection>(out LevelSection))
                {
                    // Add each of the levels to the all-levels list
                    foreach (ProcessedLine PL in LevelSection.MyProcessedRows.Values)
                    {
                        StreamingLevelLine Level = PL as StreamingLevelLine;

                        if (!AllLevels.Contains(Level.Name))
                        {
                            AllLevels.Add(Level.Name);
                        }
                    }

                    // And record the list for easy access later
                    LevelLists.Add(LevelSection);
                }

                // Look for the massive global list sound section
                SoundDumpReportSection SoundListSection;
                if ((Sound_Metagroup != null) && LeakFile.FindFirstSection<SoundDumpReportSection>(out SoundListSection))
                {
                    double SumSize = 0.0;
                    foreach (ProcessedLine LineItem in SoundListSection.MyProcessedRows.Values)
                    {
                        if (!AllSounds.Contains(LineItem.Name))
                        {
                            AllSounds.Add(LineItem.Name);
                        }
                        SumSize += ((SoundDumpLine)LineItem).SizeKb;
                    }

                    Sound_Metagroup.SetSamplePoint(FileIndex, SumSize);
                }

                // Look for the grouped sound class section


                // Look for KVP sections
                foreach (ReportSection Section in LeakFile.Sections)
                {
                    KeyValuePairSection KVP = Section as KeyValuePairSection;
                    if (KVP != null)
                    {
                        // Run through all the keys
                        foreach (string Key in KVP.GetKeys())
                        {
                            SampleRecord Record = KVP.GetValueAsRecord(Key);

                            GroupInfo Group;
                            if (!KVP_Groups.TryGetValue(Key, out Group))
                            {
                                Group = GroupingData.CreateMetaGroup(Key);
                                Group.bCanBeFilteredBySize = KVP.bCanBeFilteredBySize;
                                Group.PriorityIndex = Record.Priority;
                                Group.OverviewStatType = Record.OverviewStatType;
                                KVP_Groups.Add(Key, Group);
                            }

                            Group.SetSamplePoint(FileIndex, Record.Sample);
                        }
                    }
                }
            }

            // Verify that certain sections panned out as expected
            Debug.Assert((LevelLists.Count == Files.Count) || (LevelLists.Count == 0));



            // Sum of class groups
            GroupInfo SumAllMetagroup_ExceptIgnores = GroupingData.CreateMetaGroup("SumOfAllObjects_ExceptIgnores");
            SumAllMetagroup_ExceptIgnores.PriorityIndex = 5;

            GroupInfo SumAllMetagroup_Everything = GroupingData.CreateMetaGroup("SumOfAllObjects");
            SumAllMetagroup_Everything.PriorityIndex = 11;

            GroupInfo SumOfPriorityBuckets = GroupingData.CreateMetaGroup("SumOfPriorityBuckets");
            SumOfPriorityBuckets.PriorityIndex = 11;

            GroupInfo SumOfOtherBuckets_ExceptIgnores = GroupingData.CreateMetaGroup("SumOfOtherBuckets_ExceptIgnores");
            SumOfOtherBuckets_ExceptIgnores.PriorityIndex = 11;

            GroupInfo SumOfOtherBuckets = GroupingData.CreateMetaGroup("SumOfOtherBuckets");
            SumOfOtherBuckets.PriorityIndex = 11;

            for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
            {
                // Run through all groups and sum any that aren't special
                double SumSizeKB = 0;
                double SumSizeKB_ExceptIgnores = 0;
                double UnimportantSizeKB_ExceptIgnores = 0;
                double UnimportantSizeKB = 0;

                for (int GroupIndex = 0; GroupIndex < GroupingData.Groups.Count; ++GroupIndex)
                {
                    GroupInfo Group = GroupingData.Groups[GroupIndex];

                    if (Group.bIsObjListData)
                    {
                        SumSizeKB += Group.SamplePerFile[FileIndex];

                        if (!Group.bIgnoreSizes)
                        {
                            SumSizeKB_ExceptIgnores += Group.SamplePerFile[FileIndex];
                        }

                        if (Group.PriorityIndex > 0)
                        {
                            UnimportantSizeKB += Group.SamplePerFile[FileIndex];
                            if (!Group.bIgnoreSizes)
                            {
                                UnimportantSizeKB_ExceptIgnores += Group.SamplePerFile[FileIndex];
                            }
                        }
                    }
                }

                SumAllMetagroup_ExceptIgnores.SetSamplePoint(FileIndex, SumSizeKB_ExceptIgnores);
                SumAllMetagroup_Everything.SetSamplePoint(FileIndex, SumSizeKB);
                SumOfPriorityBuckets.SetSamplePoint(FileIndex, SumSizeKB_ExceptIgnores - UnimportantSizeKB_ExceptIgnores);
                SumOfOtherBuckets_ExceptIgnores.SetSamplePoint(FileIndex, UnimportantSizeKB_ExceptIgnores);
                SumOfOtherBuckets.SetSamplePoint(FileIndex, UnimportantSizeKB);
            }

            // Sort the groups
            GroupingData.Groups.Sort();
        }


        string GenerateLineFromGroup(string GroupName, string PerElementPrefix, double ScaleFactor)
        {
            // Write the header for the report
            GroupInfo Samples = GroupingData.FindGroupByName(GroupName);

            string OutputLine = GroupName + ",";
            for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
            {
                OutputLine += String.Format("{0}{1},", PerElementPrefix, Samples.SamplePerFile[FileIndex] * ScaleFactor);
            }

            return OutputLine;
        }

        /// <summary>
        /// Generates a report, filtering on a priority group
        /// </summary>
        /// <param name="OutputFilename"></param>
        /// <param name="FilterPriorityLabel"></param>
        public void GeneratePriorityGroupReport(string OutputFilename, string FilterPriorityLabel)
        {

            // Find the filter priority index
            int FilterPriorityNumber = int.MaxValue;
            foreach (KeyValuePair<int, string> KVP in PriorityLabels)
            {
                if (KVP.Value == FilterPriorityLabel)
                {
                    FilterPriorityNumber = KVP.Key;
                }
            }

            if (FilterPriorityNumber != int.MaxValue)
            {
                GeneratePriorityGroupReport(OutputFilename, FilterPriorityNumber, FilterPriorityNumber);
            }
        }

        /// <summary>
        /// Generates a report, filtering on a priority group range
        /// </summary>
        public void GeneratePriorityGroupReport(string OutputFilename, int MinPriority, int MaxPriority)
        {
            // Open the report
            Directory.CreateDirectory(Path.GetDirectoryName(OutputFilename));
            StreamWriter SW = new StreamWriter(OutputFilename);

            SW.WriteLine(GenerateLineFromGroup("TimeElapsedSecs", "T", 1.0));

            SW.WriteLine(GenerateLineFromGroup("ReducePoolSize", "", 1.0 / 1024.0));

            // Print out all the groups that pass the filter
            for (int GroupIndex = 0; GroupIndex < GroupingData.Groups.Count; ++GroupIndex)
            {
                GroupInfo Group = GroupingData.Groups[GroupIndex];

                // Print it out if it passes the filter
                if ((Group.PriorityIndex >= MinPriority) && (Group.PriorityIndex <= MaxPriority))
                {
                    // Print out the fixed portion of the report line
                    string OutLine = Group.GroupName + ",";

                    // Now print out the size per file
                    for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
                    {
                        OutLine += String.Format("{0},", Group.SampleToString(FileIndex, SizeScaler));
                    }
                    
                    SW.WriteLine(OutLine);
                }
            }

            // Print out the bugit lines
            StringBuilder BugItLine = new StringBuilder("BugItGo");
            for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
            {
                BugItLine.Append(',');
                BugItLine.Append(BugItCoordinates[FileIndex]);
            }
            SW.WriteLine(BugItLine.ToString());

            // Output loaded streaming levels
            OutputStreamingLevels(SW, "");
           

            SW.Close();
        }


        void OutputStreamingLevels(StreamWriter SW, string LinePrefix)
        {
            foreach (string LevelName in AllLevels)
            {
                string OutLine = String.Format("{0}{1},", LinePrefix, LevelName);

                foreach (LoadedLevelsSection LevelsInFrame in LevelLists)
                {
                    StreamingLevelLine Level = LevelsInFrame.FindByName(LevelName) as StreamingLevelLine;

                    string CellEntry = "";
                    if (Level != null)
                    {
                        if (Level.IsAtLeastPartiallyLoaded)
                        {
                            CellEntry = Level.LoadedStatus;
                        }
                    }
                    OutLine += CellEntry + ",";
                }

                SW.WriteLine(OutLine);
            }

        }

        /// <summary>
        /// Generates a standard report
        /// </summary>
        /// <param name="OutputFilename">The filename to write the standard report to</param>
        /// <param name="MinSignificantSizeKB">Filter size cutoff to keep the report length reasonable.  Classes with total instance sizes below this are not placed in the report.</param>
        /// <returns></returns>
        public SeriesReportData GenerateStandardReport(string OutputFilename, int MinSignificantSizeKB)
        {
            SeriesReportData Result = new SeriesReportData(Path.GetFileNameWithoutExtension(OutputFilename));

            //////////////////////
            // REPORT PART 1 - Matrix of sizes for classes, resources, free memory, etc...
            //////////////////////

            // Now dump them all to a CSV
            Directory.CreateDirectory(Path.GetDirectoryName(OutputFilename));
            StreamWriter SW = new StreamWriter(OutputFilename);

            // Write the header for the sizes portion of the report
            string HeadingLine = String.Format("Category,Min{0},Max{0},Average{0},Median{0},StdDev{0},Name,", SizeSuffix);
            for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
            {
                HeadingLine += String.Format("F{0}_{1},", FileIndex + 1, SizeSuffix);
            }
            SW.WriteLine(HeadingLine);

            // Output the group info
            int LastPriorityThreshold = int.MinValue;
            for (int GroupIndex = 0; GroupIndex < GroupingData.Groups.Count; ++GroupIndex)
            {
                GroupInfo Group = GroupingData.Groups[GroupIndex];

                // Skip if we don't care about this data for standard report
                if (Group.bIsNotInterestingForStandardReport)
                {
                    continue;
                }

                // Create a summary and add it to the result
                GroupSummary Summary = new GroupSummary(Group);
                Result.Add(Summary);

                // Print it out if it passes the filter
                if (Summary.CanPassSizeFilter(MinSignificantSizeKB, true))
                {
                    // Check to see if this is a new priority, and if that priority has a label
                    string PriorityLabel = "";
                    if (Summary.PriorityIndex != LastPriorityThreshold)
                    {
                        LastPriorityThreshold = Summary.PriorityIndex;

                        if (!PriorityLabels.TryGetValue(Summary.PriorityIndex, out PriorityLabel))
                        {
                            PriorityLabel = "";
                        }
                    }

                    // Print out the fixed portion of the report line
                    string OutLine = String.Format("{0},{1},{2},",
                        PriorityLabel,
                        IntegerStatToCSV(Summary.Distribution, (Group.SampleUnitType == ESampleUnit.Kilobytes) ? SizeScaler : 1.0f),
                        Group.GroupName);

                    // Now print out the size per file
                    for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
                    {
                        OutLine += String.Format("{0},", Group.SampleToString(FileIndex, SizeScaler));
                    }
                    SW.WriteLine(OutLine);
                }
            }

            // Output how many P-maps are loaded at each frame
            if (LevelLists.Count == Files.Count)
            {
                string OutLine = ",,,,,,Loaded P-Maps";

                for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
                {
                    LoadedLevelsSection Foo = LevelLists[FileIndex];

                    int NumPMaps = 0;
                    foreach (ProcessedLine Level in Foo.MyProcessedRows.Values)
                    {
                        if (Level.Name.EndsWith("_p", StringComparison.InvariantCultureIgnoreCase))
                        {
                            if (((StreamingLevelLine)Level).LoadedStatus != "PERMANENT")
                            {
                                NumPMaps++;
                            }
                        }
                    }

                    OutLine += String.Format(",{0}", NumPMaps);
                }

                SW.WriteLine(OutLine);
            }

            // Output loaded streaming levels
            SW.WriteLine("Levels");
            OutputStreamingLevels(SW, ",,,,,,");

            //////////////////////
            // REPORT PART 2 - FileIndex->FileName mapping
            //////////////////////

            // Output a file index -> filename mapping and bugit command for the map
            SW.WriteLine("");
            SW.WriteLine("File Mapping");
            SW.WriteLine("Index,Name,BugItGo,FullPath");
            for (int FileIndex = 0; FileIndex < Files.Count; ++FileIndex)
            {
                MemLeakFile File = Files[FileIndex];
                string CoordString = BugItCoordinates[FileIndex];
                SW.WriteLine(String.Format("{0},{1},{2},{3}", FileIndex + 1, Path.GetFileNameWithoutExtension(File.Filename), CoordString, File.Filename));
            }

            // Close the file
            SW.Close();

            return Result;
        }
    }
}