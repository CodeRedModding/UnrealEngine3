// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Ionic.Zip;
using System.IO;

namespace IPAExamine
{
    /// <summary>
    /// The IPAExamine command line tool is used to quickly report on total asset size (sorted by extension) 
    /// within an IPA file for iOS devices
    /// usage: IPAExamine (path to ipa file), or /? for more options
    /// </summary>
    class Program
    {
        
        /// <summary>
        /// holds total size and all entries for a given bucket of files with common extension within the file
        /// </summary>
        class ExtensionBucket
        {
            public long TotalSize;
            public List<ZipEntry> Entries;
        }

        
        /// <summary>
        /// our global application settings class
        /// </summary>
        class Settings
        {
            public bool bSummaryOnly;
            public bool bUseSectionFilter;
            public bool bExportMeshFileList;
            public bool bCSVOutput;
            public bool bShowHelp;
            public string SectionNameFilter;
            public string IPAName;
            public string ExportPathPrefix;
        }

        /// <summary>
        /// Global application settings, modified from commandline params
        /// </summary>
        private static Settings AppSettings = new Settings();
        /// <summary>
        /// the name of the file that we write to if doing a mesh list export
        /// </summary>
        private const string MeshExportFileName = "MeshList.txt";

        /// <summary>
        /// Comparator for sorting extension buckets
        /// </summary>
        /// <param name="left"></param>
        /// <param name="right"></param>
        /// <returns></returns>
        private static int CompareExtensionBuckets (ZipEntry Left, ZipEntry Right)
        {
            return Left.CompressedSize.CompareTo(Right.CompressedSize);
        }

        /// <summary>
        /// Main!
        /// </summary>
        /// <param name="args"></param>
        static void Main(string[] Args)
        {
            if (Args.Length == 0)
            {
                ShowHelp();
                return;
            }

            //AppSettings.IPAName = @"D:\ExoGame-IPhone-Release.ipa";
            ParseArguments(Args);

            if (String.IsNullOrEmpty(AppSettings.IPAName) || AppSettings.bShowHelp)
            {
                ShowHelp();
                return;
            }

            Dictionary<string, ExtensionBucket> AllBuckets = new Dictionary<string, ExtensionBucket>();

            if (!File.Exists(AppSettings.IPAName))
            {
                WriteColorLine(ConsoleColor.Red, "File not found! Please pass the path to an existing IPA file as the first parameter");
                Console.WriteLine("");
                ShowHelp();
                return;
            }

            FillEntries(AllBuckets);
            SortBuckets(AllBuckets);
            DisplayResults(AllBuckets);
        }

        /// <summary>
        /// Open IPA file and catalogue all entries into passed in array
        /// </summary>
        /// <param name="AllBuckets">Dictionary of all extension types and their catalogued contents</param>
        private static void FillEntries(Dictionary<string, ExtensionBucket> AllBuckets)
        {
            ZipFile ZFile = new ZipFile(AppSettings.IPAName);
            foreach (ZipEntry ZEntry in ZFile.Entries)
            {
                if (!ZEntry.IsDirectory)
                {
                    string Extension = Path.GetExtension(ZEntry.FileName);
                    // see if we already have a bucket
                    if (!AllBuckets.ContainsKey(Extension))
                    {
                        ExtensionBucket NewBucket = new ExtensionBucket();
                        NewBucket.Entries = new List<ZipEntry>();
                        AllBuckets.Add(Extension, NewBucket);
                    }

                    // get the bucket
                    ExtensionBucket Bucket = AllBuckets[Extension];
                    Bucket.TotalSize += ZEntry.CompressedSize;
                    Bucket.Entries.Add(ZEntry);
                }
            }
        }

        /// <summary>
        /// Show command line help
        /// </summary>
        private static void ShowHelp()
        {
            WriteColorLine(ConsoleColor.Yellow, "IPAExamine.exe - helps inspect the contents of an .ipa file");
            WriteColorLine(ConsoleColor.White, "");
            WriteColorLine(ConsoleColor.White, "Usage:");
            WriteColorLine(ConsoleColor.White, "IPAExamine <ipafilename> [?] [SummaryOnly] [ExportMeshList] [Section=<sectionname>] [csv] [ExportPath<path>]");
            WriteColorLine(ConsoleColor.Gray, "ipafilename   : path to a valid ipa file to examine");
            WriteColorLine(ConsoleColor.Gray, "SummaryOnly   : only display summary table for section size breakdown");
            WriteColorLine(ConsoleColor.Gray, "ExportMeshList: output a MeshList.txt file with all .xxx sections listed for further analysis (no path on files)");
            WriteColorLine(ConsoleColor.Gray, "ExportPath=   : path to prefix all packages in the export list with");
            WriteColorLine(ConsoleColor.Gray, "Section=      : a file extention within the ipa that you want a singular report on");
            WriteColorLine(ConsoleColor.Gray, "csv           : display results as csv instead of tabified");
            WriteColorLine(ConsoleColor.Gray, "?             : show this help menu");
        }

        /// <summary>
        /// Parse our passed in commandline, setting global AppSettings as needed
        /// </summary>
        /// <param name="args"></param>
        private static void ParseArguments(string[] Args)
        {
            // set to defaults just in case
            AppSettings.bSummaryOnly = false;
            AppSettings.bUseSectionFilter = false;
            AppSettings.bExportMeshFileList = false;
            AppSettings.bCSVOutput = false;
            AppSettings.IPAName = Args[0];
            AppSettings.bShowHelp = false;

            // skip first entry, already used as path to filename
            for (int Index = 1; Index < Args.Length; ++Index)
            {
                string TestArg = Args[Index].ToLower();
                if (TestArg.Contains("section="))
                {
                    AppSettings.bUseSectionFilter = true;
                    AppSettings.SectionNameFilter = TestArg.Substring(TestArg.IndexOf('=') + 1);
                }

                if (TestArg.Contains("exportpath="))
                {
                    AppSettings.ExportPathPrefix = TestArg.Substring(TestArg.IndexOf('=') + 1);
                    AppSettings.ExportPathPrefix = AppSettings.ExportPathPrefix.Replace('\\', '/');
                }

                if (TestArg.Contains("summaryonly"))
                {
                    AppSettings.bSummaryOnly = true;
                }

                if (TestArg.Contains("exportmeshlist"))
                {
                    AppSettings.bExportMeshFileList = true;
                    // force section filter to xxx as well
                    AppSettings.bUseSectionFilter = true;
                    AppSettings.SectionNameFilter = "xxx";
                }

                if (TestArg.Contains("csv"))
                {
                    AppSettings.bCSVOutput = true;
                }

                if (TestArg.Contains("?") || TestArg.Contains("help"))
                {
                    AppSettings.bShowHelp = true;
                }
            }
        }

        /// <summary>
        /// Sort buckets based on total size per extension
        /// </summary>
        /// <param name="AllBuckets"></param>
        private static void SortBuckets(Dictionary<string, ExtensionBucket> AllBuckets)
        {
            foreach (KeyValuePair<string, ExtensionBucket> BucketKvp in AllBuckets)
            {
                BucketKvp.Value.Entries.Sort(CompareExtensionBuckets);
            }
        }

        /// <summary>
        /// Show results to screen based on app settings
        /// </summary>
        /// <param name="AllBuckets"></param>
        private static void DisplayResults(Dictionary<string, ExtensionBucket> AllBuckets)
        {
            long TotalSize = 0;

            // dummy open of file, put in append mode so we don't delete it
            StreamWriter OutputMeshStream = new StreamWriter(MeshExportFileName, true /* append */);

            // set up output file if exporting mesh list
            if (AppSettings.bExportMeshFileList)
            {
                OutputMeshStream.Close();
                OutputMeshStream = new StreamWriter(MeshExportFileName, false /* append */);
            }

            // per file summary
            foreach (KeyValuePair<string, ExtensionBucket> BucketKVP in AllBuckets)
            {
                TotalSize += BucketKVP.Value.TotalSize;

                if (AppSettings.bUseSectionFilter)
                {
                    
                    if (!BucketKVP.Key.Equals(AppSettings.SectionNameFilter))
                    {
                        // check for name compare without leading dot as well
                        if (BucketKVP.Key.StartsWith("."))
                        {
                            if (!BucketKVP.Key.Substring(1).Equals(AppSettings.SectionNameFilter))
                            {
                                continue;
                            }
                        }
                        else
                        {
                            continue;
                        }
                    }
                }

                // if summary only, done cataloguing this extension, don't print anything out
                if (AppSettings.bSummaryOnly)
                {
                    continue;
                }

                // don't write key name for export list
                if (!AppSettings.bExportMeshFileList)
                {
                    WriteColorLine(ConsoleColor.Yellow, BucketKVP.Key);
                }

                foreach (ZipEntry entry in BucketKVP.Value.Entries)
                {
                    if (AppSettings.bExportMeshFileList)
                    {
                        string OutFile = "";
                        if (!String.IsNullOrEmpty(AppSettings.ExportPathPrefix))
                        {
                            string EndSlash = "";
                            if (!AppSettings.ExportPathPrefix.EndsWith("/"))
                            {
                                EndSlash = "/";
                            }
                            OutFile += AppSettings.ExportPathPrefix + EndSlash;
                        }
                        OutFile += Path.GetFileName(entry.FileName);
                        OutputMeshStream.WriteLine(OutFile);
                    }
                    else 
                    {
                        string FormatString = "{0,-15:N0}\t\t{1,-35}\t\t{2,-110}";
                        if (AppSettings.bCSVOutput)
                        {
                            FormatString = "{0},{1},{2}";
                        }
                        string output = String.Format(FormatString, entry.CompressedSize, Path.GetFileName(entry.FileName), entry.FileName);
                        WriteColorLine(ConsoleColor.White, output);                    
                    }
                }
                Console.WriteLine("");
            }

            if (!AppSettings.bUseSectionFilter)
            {
                // summary table
                int CurItem = 0;
                foreach (KeyValuePair<string, ExtensionBucket> bucket_kvp in AllBuckets)
                {
                    float percent = bucket_kvp.Value.TotalSize / (float)TotalSize;
                    string FormatString = "{0,-20}{1,-20:N0}{2,10:P}";
                    if (AppSettings.bCSVOutput)
                    {
                        FormatString = "{0},{1},{2:P}";
                    }
                    string output = String.Format(FormatString, bucket_kvp.Key, bucket_kvp.Value.TotalSize, percent);
                    ConsoleColor LineColor = ConsoleColor.Gray;
                    if (CurItem % 2 == 0)
                    {
                        LineColor = ConsoleColor.Green;
                    }
                    WriteColorLine(LineColor, output);
                    CurItem++;
                }

                string finalstring = String.Format("Total IPA size: {0:N0}", TotalSize);
                Console.WriteLine("");
                WriteColorLine(ConsoleColor.White, finalstring);
            }

            if (AppSettings.bExportMeshFileList)
            {
                OutputMeshStream.Close();
            }

        }

        /// <summary>
        /// Write a line of text to the console in the given color
        /// </summary>
        /// <param name="color"></param>
        /// <param name="output"></param>
        private static void WriteColorLine(ConsoleColor InColor, string Output)
        {
            ConsoleColor OldCol = Console.ForegroundColor;
            Console.ForegroundColor = InColor;
            Console.WriteLine(Output);
            Console.ForegroundColor = OldCol;
        }
    }
}
