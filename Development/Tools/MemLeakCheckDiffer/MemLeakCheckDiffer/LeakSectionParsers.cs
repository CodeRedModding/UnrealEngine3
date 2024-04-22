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
using System.IO;
using System.Diagnostics;
using System.Globalization;
using EpicCommonUtilities;

namespace MemLeakDiffer
{
    /// <summary>
    /// This represents a section in a memlk file
    /// </summary>
    class ReportSection
    {
        public string MyHeading;

        public List<string> MyLines = new List<string>();

        /// <summary>
        /// Hook point to allow sections to cook any data that needs it.
        /// This method will be called once all sections for a given file have been created.
        /// </summary>
        public virtual void Cook()
        {
        }
    }

    /// <summary>
    /// This represents a .memlk file consisting of many sections.
    /// </summary>
    class MemLeakFile : IComparable<MemLeakFile>
    {
        private string MyFilename;
        private string[] MyLines;

        public DateTime FileModifiedStamp;
        public DateTime InternalDateStamp = DateTime.MinValue;

        public List<ReportSection> Sections;

        public int ReducePoolSizeKB = 0;

        public MemLeakFile(string filename)
        {
            MyFilename = filename;
            MyLines = File.ReadAllLines(MyFilename);
            FileModifiedStamp = File.GetLastWriteTime(MyFilename);

            // Run the parsers
            try
            {
                Sections = MemLeakParser.Get().ParseFile(MyLines);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to parse '" + MyFilename + "'.  This may indicate that the memlk structure has changed slightly and the tool will need to be updated to support it.  Error: '" + ex.Message + "'.");
            }

            CleanupAfterParsing();
        }

		/// <summary>
        /// Comparison by FileCreationDate
        /// </summary>
        public int CompareTo(MemLeakFile OtherFile)
        {
            int ComparisonResult = InternalDateStamp.CompareTo(OtherFile.InternalDateStamp);
            if (ComparisonResult == 0)
            {
                // Should only occur on files with a missing internal stamp (so both have MinTime)
                return FileModifiedStamp.CompareTo(OtherFile.FileModifiedStamp);
            }
            else
            {
                return ComparisonResult;
            }
        }

        void CleanupAfterParsing()
        {
            // Factor a command line -reducepoolsize into the memory section
            LogHeaderSection HeaderSection;
            if (FindFirstSection(out HeaderSection))
            {
                // Parse the command line pool size adjustment
                string ReducedSizeString = HeaderSection.FindValueForCommandOption("ReducePoolSize", "0");
                int ReducePoolSizeMB;
                if (!int.TryParse(ReducedSizeString, out ReducePoolSizeMB))
                {
                    ReducePoolSizeMB = 0;
                }
                ReducePoolSizeKB = ReducePoolSizeMB * 1024;

                // Push that into the memory report section
                MemStatsReportSection MemStats;
                if (FindFirstSection(out MemStats))
                {
                    if (MemStats.LowestRecentTitleFreeKB != MemStatsReportSection.INVALID_SIZE)
                    {
                        MemStats.LowestRecentTitleFreeKB -= ReducePoolSizeKB;
                    }
                    if (MemStats.TitleFreeKB != MemStatsReportSection.INVALID_SIZE)
                    {
                        MemStats.TitleFreeKB -= ReducePoolSizeKB;
                    }
                }
                
                // Read a line of the form 'Log file open, 08/13/10 15:57:45' and split out the date and time
                CultureInfo EnUS = CultureInfo.CreateSpecificCulture("en-US");
                DateTime CaptureTime;
				if (DateTime.TryParseExact(HeaderSection.OpenDate + " " + HeaderSection.OpenTime, "MM/dd/yy HH:mm:ss", EnUS, DateTimeStyles.None, out CaptureTime))
				{
					InternalDateStamp = CaptureTime;
				}
				else
				{
					int ParseIdx = Filename.LastIndexOf(".memlk");
					if (ParseIdx != -1)
					{
						string TempString = Filename.Substring(0, Filename.Length - (Filename.Length - ParseIdx));

						int DashIdx = TempString.LastIndexOf("-");
						if (DashIdx != -1)
						{
							DashIdx = TempString.LastIndexOf("-", DashIdx - 1);
							if (DashIdx != -1)
							{
								TempString = TempString.Substring(DashIdx + 1, TempString.Length - DashIdx - 1);

								// Temp string should now be in the format of DD-HH.mm.ss
								string DateString = TempString.Substring(0, 2);
								string HourString = TempString.Substring(3, 2);
								string MinString = TempString.Substring(6, 2);
								string SecString = TempString.Substring(9, 2);
								string TimeString = TempString.Substring(3, TempString.Length - 4);
								TimeString = TimeString.Replace(".", ":");

								HeaderSection.OpenDate = "01/" + DateString + "/10";
								HeaderSection.OpenTime = TimeString;

								InternalDateStamp = new DateTime(2010, 1,
									System.Convert.ToInt32(DateString), 
									System.Convert.ToInt32(HourString), 
									System.Convert.ToInt32(MinString), 
									System.Convert.ToInt32(SecString));
							}
						}
					}
				}
            }
        }

        /// <summary>
        /// Finds the first section of a specific type, returning true if it was found (with the section placed in FirstSectionFound)
        /// </summary>
        public bool FindFirstSection<T>(out T FirstSectionFound) where T : ReportSection
        {
            foreach (ReportSection Section in Sections)
            {
                if (Section.GetType() == typeof(T))
                {
                    FirstSectionFound = (T)Section;
                    return true;
                }
            }

            FirstSectionFound = default(T);
            return false;
        }

        public string Filename { get { return MyFilename; } }
        public string[] Lines { get { return MyLines; } }
    }

    /// <summary>
    /// This class handles parsing a .memlk file into one or more sections.
    /// 
    /// To add support for a new section in the memlk file, add a derived class of MemLeakParser.Parser to
    /// the PrefixToParserMapping table in the constructor.
    /// </summary>
    class MemLeakParser
    {
        // List of available parsers and the prefix on a line that will trigger their use
        Dictionary<string, Parser> PrefixToParserMapping = new Dictionary<string, Parser>();

        // The parser
        private static MemLeakParser ParserSingleton = null;

        public static MemLeakParser Get()
        {
            if (ParserSingleton == null)
            {
                ParserSingleton = new MemLeakParser();
            }

            return ParserSingleton;
        }

        /// <summary>
        /// This class is the base class for a section parser.
        /// It is given the current location in a list of lines, and expected to produce a Section object.
        /// </summary>
        public abstract class Parser
        {
            public abstract ReportSection Parse(string[] Items, ref int i);
        }

        private MemLeakParser()
        {
            // Custom
            PrefixToParserMapping.Add("Obj List: ", new ObjDumpParser());

            MemStatsParser msp = new MemStatsParser();
            PrefixToParserMapping.Add("DmQueryTitleMemoryStatistics", msp);
            PrefixToParserMapping.Add("Memory Allocation Status", msp);

            //@TODO: Really triggering this section off of a "MEM DETAILED" command, but it has no unique header
            // and on platforms where GMalloc->Exec("DUMPALLOCS") does something (e.g., 360) the 'header' is different
            // and gets caught accordingly.
            MemStatsParserPS3 PS3Mem = new MemStatsParserPS3();
            PrefixToParserMapping.Add("GMainThreadMemStack", PS3Mem);

            SoundDumpParser sdp = new SoundDumpParser();
            PrefixToParserMapping.Add("Listing all sounds.", sdp);
            PrefixToParserMapping.Add(",Size Kb,NumChannels,SoundName,bAllocationInPermanentPool", sdp);

            PrefixToParserMapping.Add("Listing all sound classes.", new SoundClassesParser());

            PrefixToParserMapping.Add("Level Streaming:", new LoadedLevelsParser());

            BugItParser BugIt = new BugItParser();
            PrefixToParserMapping.Add("BugItGo", BugIt);
            PrefixToParserMapping.Add("DebugSetLocation", BugIt);

            PrefixToParserMapping.Add("Log file open", new LogFileHeaderParser());

            PrefixToParserMapping.Add("Current Texture Streaming Stats", new TextureStatsParser());

            // Not custom
            PrefixToParserMapping.Add("Memory split (in KByte)", new RawDumpParser("Global summary"));
            PrefixToParserMapping.Add("Loaded AnimSets:", new RawDumpParser("AnimSets"));
            PrefixToParserMapping.Add("Loaded Matinee AnimSets:", new RawDumpParser("Matinee AnimSets"));
            PrefixToParserMapping.Add("AnimTrees:", new RawDumpParser("AnimTrees"));
            PrefixToParserMapping.Add("Listing all GUDS.", new RawDumpParser("GUDS (Dialog)"));
            PrefixToParserMapping.Add("Listing scene render targets.", new RawDumpParser("Render Targets"));
            PrefixToParserMapping.Add("GPU resource dump:", new RawDumpParser("GPU Resources"));
            PrefixToParserMapping.Add("Total Number Of Packages Loaded:", new RawDumpParser("Loaded packages"));
            PrefixToParserMapping.Add("Listing all textures.", new RawDumpParser("Loaded Textures"));
            PrefixToParserMapping.Add("lightenv list volumes", new RawDumpParser("Light environment volumes"));
            PrefixToParserMapping.Add("lightenv list transition", new RawDumpParser("Light environment transition"));
            PrefixToParserMapping.Add("AudioComponent Dump", new RawDumpParser(""));
        }

        public List<ReportSection> ParseFile(string[] Lines)
        {
            List<ReportSection> Result = new List<ReportSection>();

            for (int i = 0; i < Lines.Length; )
            {
                string line = Lines[i].Trim();
                if (line == "")
                {
                    // Reset, time for a new thing
                    ++i;
                }
                else
                {
                    if (line.StartsWith("Log:"))
                    {
                        line = line.Substring(4).Trim();
                    }

                    // Check the available parsers
                    ReportSection Section = null;
                    foreach (string prefix in PrefixToParserMapping.Keys)
                    {
                        if (line.StartsWith(prefix))
                        {
                            Section = PrefixToParserMapping[prefix].Parse(Lines, ref i);
                            break;
                        }
                    }

                    // Use a default parser if it hasn't been handled yet
                    if (Section == null)
                    {
                        Parser p = new RawDumpParser(Lines[i]);
                        Section = p.Parse(Lines, ref i);
                    }

                    // Add the ReportSection to the list
                    Debug.Assert(Section != null);
                    Result.Add(Section);
                }
            }

            // Run over all of the sections and cook them if needed
            foreach (ReportSection section in Result)
            {
                section.Cook();
            }

            return Result;
        }
    }

    /// <summary>
    /// This class adds some helper methods to the basic parser to make creating new parsers a little easier
    /// </summary>
    abstract class SmartParser : MemLeakParser.Parser
    {
        /// <summary>
        /// Reads a line and returns true if it matches the desired line
        /// </summary>
        public static bool Ensure(string[] Items, ref int i, string DesiredLine)
        {
            string Temp;
            if (ReadLine(Items, ref i, out Temp))
            {
                return Temp.Trim() == DesiredLine;
            }
            else
            {
                return false;
            }
        }

        /// <summary>
        /// Reads the next line in the current section (returns true if successful, false if the section is finished).
        /// It also automatically strips off the Log: prefix
        /// </summary>
        /// <param name="Items"></param>
        /// <param name="i"></param>
        /// <param name="ResultingLine"></param>
        /// <returns></returns>
        public static bool ReadLine(string[] Items, ref int i, out string ResultingLine)
        {
            if (i < Items.Length)
            {
                ResultingLine  = Items[i].Trim();
                if (ResultingLine  != "")
                {
                    if (ResultingLine.StartsWith("Log:"))
                    {
                        ResultingLine = ResultingLine.Substring(4).Trim();
                    }

                    ++i;
                    return true;
                }
            }

            ResultingLine = "";
            return false;
        }

        /// <summary>
        /// Parses strings of the form "%s %i" or "%s %i foo", where %s is prefix and %i is the output value
        /// If the string doesn't match the form, value is unchanged
        /// </summary>
        public static bool ParseLineIntInMiddle(string InLine, string Prefix, ref int Value)
        {
            if (InLine.StartsWith(Prefix))
            {
                string ValueString = InLine.Substring(Prefix.Length).Trim();

                char[] Separators = { ' ' };
                string[] Sections = ValueString.Split(Separators, StringSplitOptions.RemoveEmptyEntries);

                int Temp;
                if (int.TryParse(Sections[0], out Temp))
                {
                    Value = Temp;
                    return true;
                }
            }

            return false;
        }


        /// <summary>
        /// Parses strings of the form "%s %i" or "%s %i foo", where %s is prefix and %i is the output value
        /// If the string doesn't match the form, value is unchanged
        /// </summary>
        public static bool ParseLineFloatInMiddle(string InLine, string Prefix, ref double Value)
        {
            if (InLine.StartsWith(Prefix))
            {
                string ValueString = InLine.Substring(Prefix.Length).Trim();

                char[] Separators = { ' ' };
                string[] Sections = ValueString.Split(Separators, StringSplitOptions.RemoveEmptyEntries);

                double Temp;
                if (double.TryParse(Sections[0], out Temp))
                {
                    Value = Temp;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Checks to see if InLine is of the form Key = Value.  If so, it stores the second half in Value, and returns true.
        /// </summary>
        public static bool ParseLineKeyValuePair(string InLine, string Key, ref string Value)
        {
            char[] Separators = { '=' };
            string[] Sections = InLine.Split(Separators);

            if (Sections.Length == 2)
            {
                if (Sections[0].Trim().Equals(Key, StringComparison.InvariantCultureIgnoreCase))
                {
                    Value = Sections[1].Trim();
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Checks to see if InLine is of the form Key = [float value], and if so sticks the float in Value, returning true.
        /// </summary>
        public static bool ParseLineKeyValuePair(string InLine, string Key, ref float Value)
        {
            string ValueString = null;
            if (ParseLineKeyValuePair(InLine, Key, ref ValueString))
            {
                float Temp;
                if (float.TryParse(ValueString, out Temp))
                {
                    Value = Temp;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Checks to see if InLine is of the form Key = [float value], and if so sticks the float in Value, returning true.
        /// </summary>
        public static bool ParseLineKeyValuePair(string InLine, string Key, ref int Value)
        {
            string ValueString = null;
            if (ParseLineKeyValuePair(InLine, Key, ref ValueString))
            {
                int Temp;
                if (int.TryParse(ValueString, out Temp))
                {
                    Value = Temp;
                    return true;
                }
            }

            return false;
        }
    }

    class GridRow
    {
        public string[] Items;

        public GridRow()
        {
            Items = new string[0];
        }

        public GridRow(string[] InItems)
        {
            this.Items = InItems;
        }
    }

    class GridSheet
    {
        public GridRow Headers = new GridRow();
        public List<GridRow> Rows = new List<GridRow>();
    }

    class ProcessedLine
    {
        public virtual string Name { get { return null; } }
    }

    class GridReportSection : ReportSection
    {
        public GridSheet MyItems = new GridSheet();
        public Dictionary<string, ProcessedLine> MyProcessedRows = new Dictionary<string, ProcessedLine>();

        public ProcessedLine FindByName(string name)
        {
            ProcessedLine result;
            if (!MyProcessedRows.TryGetValue(name, out result))
            {
                result = null;
            }

            return result;
        }
    }

    /// <summary>
    /// This parser is oblivious to it's contents.
    /// It just stores line after line as plain text until the end of a section is detected.
    /// </summary>
    class RawDumpParser : MemLeakParser.Parser
    {
        string MyHeading;

        public RawDumpParser(string InHeading)
        {
            MyHeading = InHeading;
        }

        public override ReportSection Parse(string[] Items, ref int i)
        {
            ReportSection Result = new ReportSection();

            Result.MyHeading = MyHeading;

            while (i < Items.Length)
            {
                string line = Items[i];
                line = line.Trim();

                if (line == "")
                {
                    break;
                }
                else
                {
                    Result.MyLines.Add(Items[i]);
                    ++i;
                }
            }

            return Result;
        }
    }

    /// <summary>
    /// Section containing a BugItGo command string
    /// </summary>
    class BugItSection : ReportSection
    {
        public string CoordinateString;
		//@todo. Need to support multiple BugItGo strings for split screen captures!
		public float[] Position;
		public int[] Rotator;

        public BugItSection(string InCoords)
        {
            CoordinateString = InCoords;

            // Expecting either:
            // Log: DebugSetLocation (X=79541.1719, Y=163594.4531, Z=33052.9883) (Pitch=-264, Yaw=-19204, Roll=0)
            // or
            // Log: BugItGo 1925.2506 -6172.9424 216.9305 -2109 15451 0
            if (CoordinateString.Contains("DebugSetLocation"))
            {
                // Convert to a bug it go
                char[] Separators = { ' ', '(', '=', ',', ')' };
                string[] Cells = CoordinateString.Split(Separators, StringSplitOptions.RemoveEmptyEntries);

                if (Cells.Length == 13)
                {
                    string X = Cells[2];
                    string Y = Cells[4];
                    string Z = Cells[6];

                    string P = Cells[8];
                    string Yaw = Cells[10];
                    string R = Cells[12];

                    CoordinateString = String.Format("BugItGo {0} {1} {2} {3} {4} {5}", X, Y, Z, P, Yaw, R);
                }
            }
        }

		/// <summary>
		/// Parse the bugit location and rotation
		/// </summary>
		public override void Cook()
		{
			// Split into cells and convert the entries...
			char[] Separators = { ' ' };
			string[] Cells = CoordinateString.Split(Separators, StringSplitOptions.RemoveEmptyEntries);
			if (Cells.Length == 7)
			{
				Position = new float[3];
				Rotator = new int[3];

				Position[0] = (float)(System.Convert.ToDouble(Cells[1]));
				Position[1] = (float)(System.Convert.ToDouble(Cells[2]));
				Position[2] = (float)(System.Convert.ToDouble(Cells[3]));
				Rotator[0] = System.Convert.ToInt32(Cells[4]);
				Rotator[1] = System.Convert.ToInt32(Cells[5]);
				Rotator[2] = System.Convert.ToInt32(Cells[6]);
			}
		}
	}

    /// <summary>
    /// Parser for the BugItGo section of a memleakcheck file
    /// </summary>
    class BugItParser : SmartParser
    {
        public override ReportSection Parse(string[] Items, ref int i)
        {
            string Coords;
            if (!ReadLine(Items, ref i, out Coords))
            {
                Coords = "";
            }
            i++;

            return new BugItSection(Coords);
        }
    }

    class LogHeaderSection : ReportSection
    {
        public string OpenDate;
        public string OpenTime;
        public List<string> CommandLineOptions = new List<string>();

        /// <summary>
        /// Returns true if the string passed in as OptionString was of the form -RequiredKey=Value, placing the read value in the out parameter Value
        /// </summary>
        protected static bool ParseCommandLineOptionAsKVP(string OptionString, string RequiredKey, out string Value)
        {
            Value = null;

            // Check for a leading -
            if (OptionString[0] != '-')
            {
                return false;
            }

            // Split "-Key=Value" into {Key, Value}
            char [] Delimiters = { '=' };
            string[] Pair = OptionString.Substring(1).Split(Delimiters);

            if (Pair.Length == 2)
            {
                if (RequiredKey.Equals(Pair[0], StringComparison.InvariantCultureIgnoreCase))
                {
                    Value = Pair[1];
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Returns true if one of the command line options was -Key
        /// </summary>
        /// <param name="Key"></param>
        /// <returns></returns>
        public bool WasBooleanCommandPresent(string Key)
        {
            if (CommandLineOptions.Contains("-" + Key.ToUpperInvariant()))
            {
                return true;
            }

            return false;
        }

        /// <summary>
        /// Returns the Value if there was -Key=Value on the commandline, and DefaultValue otherwise
        /// </summary>
        public string FindValueForCommandOption(string Key, string DefaultValue)
        {
            foreach (string OptionString in CommandLineOptions)
            {
                string Result;
                if (ParseCommandLineOptionAsKVP(OptionString, Key, out Result))
                {
                    return Result;
                }
            }

            return DefaultValue;
        }


        /// <summary>
        /// Parses a string that contains a set of space separated command line options after the string "CommandLine Options:"
        /// </summary>
        /// <param name="OptionList"></param>
        public void ParseOptionList(string OptionList)
        {
            string Junk = "CommandLine Options:";
            if (OptionList.StartsWith(Junk))
            {
                OptionList = OptionList.Substring(Junk.Length).ToUpperInvariant();

                char[] Delimiters = { ' ' };
                string[] NewOptions = OptionList.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);

                CommandLineOptions.AddRange(NewOptions);
            }
        }
    }

    /// <summary>
    /// Parser for the first section in the file, which has command line parameters, etc...
    /// </summary>
    class LogFileHeaderParser : SmartParser
    {
        public override ReportSection Parse(string[] Items, ref int i)
        {
            string DateLine;
            string CommandLine;

            LogHeaderSection Result = new LogHeaderSection();

            // Read a line of the form 'Log file open, 08/13/10 15:57:45' and split out the date and time
            ReadLine(Items, ref i, out DateLine);
            char[] Delimiters = { ' ' };
            string[] Parts = DateLine.Split(Delimiters);
			if (Parts.Length > 3)
			{
				Result.OpenDate = Parts[3];
				if (Parts.Length > 4)
				{
					Result.OpenTime = Parts[4];
				}
			}

            if (ReadLine(Items, ref i, out CommandLine))
            {
                Result.ParseOptionList(CommandLine);
            }

            return Result;
        }
    }

    public class SampleRecord
    {
        public SampleRecord(double InSample, int InPriority, EStatType OverviewType)
        {
            Sample = InSample;
            Priority = InPriority;
            OverviewStatType = OverviewType;
        }

        public double Sample;
        public int Priority;
        public EStatType OverviewStatType;
    }

    /// <summary>
    /// Represents a section that can be adequately represented as a set of key-value pairs
    /// </summary>
    class KeyValuePairSection : ReportSection
    {
        public bool bCanBeFilteredBySize = true;
        protected Dictionary<string, SampleRecord> Samples = new Dictionary<string, SampleRecord>();

        public Dictionary<string, SampleRecord>.KeyCollection GetKeys()
        {
            return Samples.Keys;
        }

        public virtual void MergePriorityLabels(Dictionary<int, string> PriorityLabels)
        {
        }

        public virtual double GetValue(string Key)
        {
            SampleRecord Result;
            if (!Samples.TryGetValue(Key, out Result))
            {
                return 0.0;
            }
            return Result.Sample;
        }

        public virtual SampleRecord GetValueAsRecord(string Key)
        {
            SampleRecord Result;
            if (!Samples.TryGetValue(Key, out Result))
            {
                return null;
            }
            return Result;
        }

        /// <summary>
        /// Adds a sample to the 'no special behavior' key-value pair list
        /// </summary>
        public void AddSample(string SampleName, int Priority, double Value, EStatType StatMode)
        {
            Samples.Add(SampleName, new SampleRecord(Value, Priority, StatMode));
        }
    }

    /// <summary>
    /// Represents a section that can be adequately represented as a set of key-value pairs, with default line processing behavior
    /// </summary>
    class KeyValuePairSectionDefaultMatching : KeyValuePairSection
    {
        public string MyMatchLine;

        public int MyBasePriority = 50;

        protected KeyValuePairSectionDefaultMatching(string MatchLine)
        {
            MyMatchLine = MatchLine;
        }

        public override void MergePriorityLabels(Dictionary<int, string> PriorityLabels)
        {
            PriorityLabels[MyBasePriority] = MyHeading;
        }

        protected virtual void ProcessLine(string InLine)
        {
        }

        public virtual void Parse(string[] Items, ref int i)
        {
            SmartParser.Ensure(Items, ref i, MyMatchLine);

            // Read the rest of the lines, ignoring most of them
            string line;
            while (SmartParser.ReadLine(Items, ref i, out line))
            {
                ProcessLine(line);
            }
        }
    }


    /// <summary>
    /// Represents a section that just has a header followed by a series of key = value pairs
    /// </summary>
    class TrueKeyValuePairSection : KeyValuePairSectionDefaultMatching
    {
        protected Dictionary<string, string> KeyToGroupNameMapping = new Dictionary<string, string>();
        protected bool bAdditiveMode = false;
        protected char[] Separators = { '=' };

        protected TrueKeyValuePairSection(string MatchLine)
            : base(MatchLine)
        {
        }

        /// <summary>
        /// Called by ProcessLine to allow adjustment of the Key or Value if parsing a section with identical 'keys' that can
        /// only be told apart from other context
        /// </summary>
        protected virtual void AllowKeyValueAdjustment(ref string Key, ref string Value)
        {
        }

        /// <summary>
        /// Adds a sample mapping, with all the attributes
        /// </summary>
        protected void AddMappedSample(string ReportName, string GroupName, EStatType StatMode)
        {
            KeyToGroupNameMapping.Add(ReportName, GroupName);
            AddSample(GroupName, MyBasePriority, 0.0, StatMode);
        }

        protected override void ProcessLine(string InLine)
        {
            string[] Sections = InLine.Split(Separators);

            if (Sections.Length == 2)
            {
                string Key = Sections[0].Trim();
                string Value = Sections[1].Trim();

                // Clean up the value, in case it has comments or units afterwards
                int ValueSpaceIndex = Value.IndexOf(' ');
                if (ValueSpaceIndex >= 0)
                {
                    Value = Value.Substring(0, ValueSpaceIndex);
                }

                AllowKeyValueAdjustment(ref Key, ref Value);

                double NewSample;
                if (KeyToGroupNameMapping.ContainsKey(Key) && double.TryParse(Value, out NewSample))
                {
                    string GroupName = KeyToGroupNameMapping[Key];

                    SampleRecord Record = Samples[GroupName];
                    Record.Sample = (bAdditiveMode ? Record.Sample : 0.0) + NewSample;
                }
            }
        }
    }
}
